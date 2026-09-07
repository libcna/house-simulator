#!/usr/bin/env python3
"""measure_stride.py -- measure a locomotion clip's stride length, duration and foot plants.

`HOUSE-00194`. `cna-house.md` §47.4 removes foot sliding with one number per clip:

    rate = horizontalSpeed / (clipStrideLength / clipDuration)

`clipStrideLength` is metres of ground travel per gait cycle, measured offline and stored in the
`.chanim` sidecar's `Clip::strideLength` (§47.0, `docs/anim-format.md` §3.3). It costs nothing at
runtime and it is the whole of the correction, so it has to be right. This tool measures it, and
`anim_extract.py` (`HOUSE-00223`) consumes this tool's JSON rather than measuring again.

## The two kinds of source, and the one formula that covers both

A clip either **translates its root** (raw mocap: the hips travel across the capture volume) or is
**in place** (retargeted for a game: the root stays at the origin and the feet cycle beneath it).
`cna-house` will hold both -- CMU trials arrive as the first and leave Blender as the second -- and
they cannot be measured the same way by eye.

They can be measured the same way by machine, because of one fact: **while a foot is on the ground
it does not move**, so the body's true velocity over the ground is the negative of that foot's
velocity *relative to the root*:

    v_ground(t) = -d/dt (footWorld(t) - rootWorld(t))     while that foot is planted

For a root-motion clip this reproduces the root's own velocity, which gives a free cross-check --
and the residual between the two IS the foot sliding, reported as `footSlideRms`. For an in-place
clip it is the only measurement available, and it is exact. Summing `|v_ground| dt` over the clip
gives the ground travel; dividing by the number of gait cycles gives the stride.

Simply summing each foot's stance displacement is *wrong* and tempting: with double support both
feet are planted at once and the overlap is counted twice. Averaging the planted feet's velocities
per sample, then integrating, counts it once.

## Detecting a foot plant without assuming the answer

Plant detection is circular if done naively -- "the foot is planted when it is not moving over the
ground" needs the ground motion the plant is meant to measure. The way out is that during stance
every planted foot moves backwards *relative to the root* at the same speed, so:

1. take every sample where a foot is below `heightFraction` of its own vertical range;
2. take the **median** horizontal relative velocity over that whole set -- stance dominates it, so
   the median is the gait speed, and no plant has been assumed to find it;
3. keep the samples whose velocity is within `--speed-tolerance` of that median.

Step 3 is what removes the tails: a foot is low for a few frames after lift-off and before
touch-down, and in those frames it is moving *forwards*, fast. Height alone would fold them into
the stance and shorten the measured travel.

## What it does not assume

* **Not the up axis.** `--up` (default `y`, glTF's convention) picks it; the other two axes are the
  horizontal plane and the travel direction is measured, not assumed to be -Z.
* **Not the units.** `unitsHint` compares the skeleton's own height against a human being and says
  `metres`, `centimetres` or `inches`. CMU's raw skeletons are in inches, and a stride of 56 is a
  units bug that would otherwise reach `Clip::strideLength` and make every character sprint.
* **Not that the clip is locomotion at all.** A turn-in-place or an idle measures ~0 travel and is
  reported `isLocomotion: false` with `strideLength: 0`, which is exactly what §47.4 wants for a
  non-locomotion clip.
* **Not that the feet are findable.** If the skeleton has no recognisable feet the tool falls back
  to root motion and says so in `method`; if there is no root motion either it reports 0 and a
  warning rather than a number nobody can trace.

    tools/assets/measure_stride.py character.glb
    tools/assets/measure_stride.py --clip walk_fwd --json out.json character.glb
    tools/assets/measure_stride.py --make-fixture <dir>
    tools/assets/measure_stride.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import shutil
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gltf_io  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

StrideError = gltf_io.GltfError

#: Samples per second the clip is resampled to before anything is measured. A source's own key rate
#: is whatever its exporter chose -- 24, 30, 120, or non-uniform -- and measuring on the source keys
#: would make the answer depend on it. 120 is high enough that a 1 m/s gait moves 8 mm per sample.
DEFAULT_RATE = 120

#: A foot counts as low when it is within this fraction of its own vertical range of its own
#: minimum. A fraction rather than an absolute height because a retarget can leave the floor at any
#: Y, and the foot's own range is the only scale the clip carries.
HEIGHT_FRACTION = 0.25

#: ...and it counts as PLANTED when its relative horizontal speed is also within this fraction of
#: the median stance speed. This is what trims the low-but-moving frames either side of stance.
SPEED_TOLERANCE = 0.4

#: Below this, in metres per second, a clip is not locomotion: an idle sways, a turn-in-place
#: pivots, and neither should be given a stride length. 0.05 m/s is 5 cm/s -- a tenth of the
#: slowest walk in the clip set (§47.1) and well above retarget noise.
LOCOMOTION_SPEED_FLOOR = 0.05

#: Height of a standing human, used only to guess the units of a skeleton. The band is wide because
#: it separates metres from centimetres from inches, not tall people from short ones.
HUMAN_HEIGHT_M = 1.7

FOOT_TOKENS = (("ankle", 0), ("foot", 1), ("toe", 2))


# ---------------------------------------------------------------------------- node evaluation ----

def _quat_to_matrix(q: tuple[float, float, float, float]) -> list[list[float]]:
    x, y, z, w = q
    return [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]


def _compose(translation, rotation, scale) -> list[list[float]]:
    """A 3x4 affine transform as rows of [m00 m01 m02 tx]."""
    r = _quat_to_matrix(rotation)
    return [[r[row][0] * scale[0], r[row][1] * scale[1], r[row][2] * scale[2], translation[row]]
            for row in range(3)]


def _multiply(a: list[list[float]], b: list[list[float]]) -> list[list[float]]:
    out = []
    for row in range(3):
        out.append([sum(a[row][k] * b[k][column] for k in range(3))
                    + (a[row][3] if column == 3 else 0.0)
                    for column in range(3)]
                   + [sum(a[row][k] * b[k][3] for k in range(3)) + a[row][3]])
    return [row[:3] + [row[3]] for row in out]


def _translation_of(matrix: list[list[float]]) -> tuple[float, float, float]:
    return (matrix[0][3], matrix[1][3], matrix[2][3])


def _slerp(a, b, t):
    dot = sum(x * y for x, y in zip(a, b))
    if dot < 0.0:
        b = tuple(-v for v in b)
        dot = -dot
    if dot > 0.9995:
        out = tuple(x + (y - x) * t for x, y in zip(a, b))
    else:
        theta = math.acos(max(min(dot, 1.0), -1.0))
        sin_theta = math.sin(theta)
        wa = math.sin((1 - t) * theta) / sin_theta
        wb = math.sin(t * theta) / sin_theta
        out = tuple(x * wa + y * wb for x, y in zip(a, b))
    length = math.sqrt(sum(v * v for v in out)) or 1.0
    return tuple(v / length for v in out)


class Track:
    """One animation channel, resampled on demand. Owns its interpolation, including CUBICSPLINE."""

    def __init__(self, times: list[float], values: list[tuple], interpolation: str, path: str):
        self.times = times
        self.values = values
        self.interpolation = interpolation
        self.path = path
        if interpolation == "CUBICSPLINE":
            if len(values) != 3 * len(times):
                raise StrideError("a CUBICSPLINE sampler has the wrong number of output elements")
        elif len(values) != len(times):
            raise StrideError(f"a {interpolation} sampler has {len(values)} outputs for "
                              f"{len(times)} inputs")
        for earlier, later in zip(times, times[1:]):
            if later < earlier:
                raise StrideError("an animation sampler's input times are not ascending")

    def at(self, time: float):
        times = self.times
        if not times:
            raise StrideError("an animation sampler has no keys")
        if time <= times[0]:
            return self._value(0)
        if time >= times[-1]:
            return self._value(len(times) - 1)
        low, high = 0, len(times) - 1
        while high - low > 1:
            middle = (low + high) // 2
            if times[middle] <= time:
                low = middle
            else:
                high = middle
        span = times[high] - times[low]
        t = 0.0 if span <= 0 else (time - times[low]) / span
        if self.interpolation == "STEP":
            return self._value(low)
        if self.interpolation == "CUBICSPLINE":
            p0, m0 = self.values[3 * low + 1], self.values[3 * low + 2]
            m1, p1 = self.values[3 * high], self.values[3 * high + 1]
            t2, t3 = t * t, t * t * t
            h00 = 2 * t3 - 3 * t2 + 1
            h10 = t3 - 2 * t2 + t
            h01 = -2 * t3 + 3 * t2
            h11 = t3 - t2
            out = tuple(h00 * p0[i] + h10 * span * m0[i] + h01 * p1[i] + h11 * span * m1[i]
                        for i in range(len(p0)))
            return self._normalise(out)
        a, b = self.values[low], self.values[high]
        if self.path == "rotation":
            return _slerp(a, b, t)
        return tuple(x + (y - x) * t for x, y in zip(a, b))

    def _value(self, index: int):
        if self.interpolation == "CUBICSPLINE":
            return self._normalise(self.values[3 * index + 1])
        return self.values[index]

    def _normalise(self, value):
        if self.path != "rotation":
            return value
        length = math.sqrt(sum(v * v for v in value)) or 1.0
        return tuple(v / length for v in value)


class Rig:
    """The node hierarchy of a glTF, evaluated at arbitrary times."""

    def __init__(self, document: dict, buffers: list[bytes]):
        self.document = document
        self.nodes = document.get("nodes", [])
        if not self.nodes:
            raise StrideError("the file has no nodes")
        self.parent = [-1] * len(self.nodes)
        for index, node in enumerate(self.nodes):
            for child in node.get("children", []):
                if self.parent[child] != -1:
                    raise StrideError(f"node {child} has two parents; the hierarchy is not a tree")
                self.parent[child] = index
        # Parents before children, so evaluation is one forward pass -- the same property
        # `docs/anim-format.md` §3.2 requires of a `.chanim` skeleton, for the same reason.
        self.order = self._topological()
        self.rest = []
        for node in self.nodes:
            if "matrix" in node:
                raise StrideError("a node uses `matrix` rather than TRS; re-export with TRS, "
                                  "because a matrix cannot be interpolated against animated TRS")
            self.rest.append((tuple(node.get("translation", (0.0, 0.0, 0.0))),
                              tuple(node.get("rotation", (0.0, 0.0, 0.0, 1.0))),
                              tuple(node.get("scale", (1.0, 1.0, 1.0)))))
        self.buffers = buffers

    def _topological(self) -> list[int]:
        order, pending = [], [i for i, p in enumerate(self.parent) if p == -1]
        pending.sort()
        while pending:
            index = pending.pop(0)
            order.append(index)
            pending = sorted(self.nodes[index].get("children", [])) + pending
        if len(order) != len(self.nodes):
            raise StrideError("the node hierarchy contains a cycle or an unreachable node")
        return order

    def tracks(self, animation: dict) -> dict[int, dict[str, Track]]:
        samplers = animation.get("samplers", [])
        result: dict[int, dict[str, Track]] = {}
        for channel in animation.get("channels", []):
            target = channel.get("target", {})
            node = target.get("node")
            path = target.get("path")
            if node is None or path not in ("translation", "rotation", "scale"):
                continue  # `weights` targets morph targets, which carry no locomotion.
            sampler = samplers[channel["sampler"]]
            times = [v[0] for v in gltf_io.read_accessor(self.document, self.buffers,
                                                         sampler["input"])]
            values = gltf_io.read_accessor(self.document, self.buffers, sampler["output"])
            result.setdefault(node, {})[path] = Track(
                times, values, sampler.get("interpolation", "LINEAR"), path)
        return result

    def world_positions(self, tracks: dict[int, dict[str, Track]],
                        time: float) -> list[tuple[float, float, float]]:
        local: list[list[list[float]]] = [None] * len(self.nodes)  # type: ignore[list-item]
        for index in range(len(self.nodes)):
            translation, rotation, scale = self.rest[index]
            node_tracks = tracks.get(index, {})
            if "translation" in node_tracks:
                translation = node_tracks["translation"].at(time)
            if "rotation" in node_tracks:
                rotation = node_tracks["rotation"].at(time)
            if "scale" in node_tracks:
                scale = node_tracks["scale"].at(time)
            local[index] = _compose(translation, rotation, scale)

        world: list[list[list[float]]] = [None] * len(self.nodes)  # type: ignore[list-item]
        for index in self.order:
            parent = self.parent[index]
            world[index] = local[index] if parent == -1 else _multiply(world[parent], local[index])
        return [_translation_of(matrix) for matrix in world]


# ------------------------------------------------------------------------------ bone matching ----

def _side_of(name: str) -> str | None:
    """Which side a bone name refers to, across the naming conventions this project will meet.

    CMU/BVH `LeftFoot`, Mixamo `mixamorig:LeftToeBase`, Rigify `foot.L`, and the `L_`/`_l` variants
    every other exporter produces. A single-letter `l`/`r` is only accepted when it is delimited,
    so `Roll` and `Ball` are not read as sides.
    """
    lowered = name.lower()
    if "left" in lowered:
        return "left"
    if "right" in lowered:
        return "right"
    if re.search(r"(^|[_.\-: ])l([_.\-: 0-9]|$)", lowered):
        return "left"
    if re.search(r"(^|[_.\-: ])r([_.\-: 0-9]|$)", lowered):
        return "right"
    if re.match(r"^l(?=[a-z]*(foot|ankle|toe))", lowered):
        return "left"
    if re.match(r"^r(?=[a-z]*(foot|ankle|toe))", lowered):
        return "right"
    return None


def find_feet(names: list[str]) -> tuple[dict[str, int], list[str]]:
    """(side -> node index, warnings). Ankles are preferred to toes: the ankle is what plants."""
    warnings: list[str] = []
    best: dict[str, tuple[int, int, str]] = {}
    for index, name in enumerate(names):
        lowered = name.lower()
        rank = None
        for token, token_rank in FOOT_TOKENS:
            if token in lowered:
                rank = token_rank if rank is None else min(rank, token_rank)
        if rank is None:
            continue
        side = _side_of(name)
        if side is None:
            continue
        current = best.get(side)
        if current is None or rank < current[1]:
            best[side] = (index, rank, name)
        elif rank == current[1] and name != current[2]:
            warnings.append(f"more than one {side} foot bone at the same priority "
                            f"('{current[2]}' and '{name}'); using '{current[2]}'")
    feet = {side: entry[0] for side, entry in best.items()}
    if len(feet) != 2:
        warnings.append(f"could not identify a left and a right foot bone "
                        f"(found {sorted(feet)}); stride will be measured from root motion only")
    return feet, warnings


def find_root(document: dict, names: list[str], parents: list[int]) -> int | None:
    """The joint locomotion is measured against: the skin's root joint, else a hips-like name."""
    for skin in document.get("skins", []):
        joints = skin.get("joints", [])
        if not joints:
            continue
        joint_set = set(joints)
        roots = [j for j in joints if parents[j] not in joint_set]
        if len(roots) == 1:
            return roots[0]
    for pattern in ("hips", "pelvis", "root", "hip"):
        for index, name in enumerate(names):
            if pattern in name.lower():
                return index
    return None


# ---------------------------------------------------------------------------------- measuring ----

def _median(values: list[float]) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    middle = len(ordered) // 2
    if len(ordered) % 2:
        return ordered[middle]
    return 0.5 * (ordered[middle - 1] + ordered[middle])


def _runs(flags: list[bool]) -> list[tuple[int, int]]:
    """Maximal runs of True as half-open [start, end) index pairs."""
    runs, start = [], None
    for index, flag in enumerate(flags):
        if flag and start is None:
            start = index
        elif not flag and start is not None:
            runs.append((start, index))
            start = None
    if start is not None:
        runs.append((start, len(flags)))
    return runs


def measure_clip(rig: Rig, animation: dict, feet: dict[str, int], root: int | None,
                 up: int, rate: int, height_fraction: float,
                 speed_tolerance: float) -> dict:
    name = animation.get("name", "")
    tracks = rig.tracks(animation)
    if not tracks:
        raise StrideError(f"clip '{name}' animates no node translation, rotation or scale")

    end = max(track.times[-1] for node in tracks.values() for track in node.values() if track.times)
    start = min(track.times[0] for node in tracks.values() for track in node.values()
                if track.times)
    duration = end - start
    if not (duration > 0) or not math.isfinite(duration):
        raise StrideError(f"clip '{name}' has duration {duration}; a clip that cannot be sampled "
                          f"is a content error, not a zero-length clip")

    horizontal = [axis for axis in (0, 1, 2) if axis != up]
    count = max(int(round(duration * rate)) + 1, 3)
    step = duration / (count - 1)
    samples = [rig.world_positions(tracks, start + i * step) for i in range(count)]
    for frame in samples:
        for position in frame:
            if not all(math.isfinite(v) for v in position):
                raise StrideError(f"clip '{name}' evaluates to a non-finite position; the source "
                                  f"animation data is malformed")

    warnings: list[str] = []

    # Root motion, if any: the straight-line and the integrated path are both reported, because a
    # curved walk has two different honest answers and rate matching wants the integrated one.
    root_net = root_path = 0.0
    root_delta = [0.0, 0.0]
    if root is not None:
        first, last = samples[0][root], samples[-1][root]
        root_delta = [last[axis] - first[axis] for axis in horizontal]
        root_net = math.hypot(*root_delta)
        for a, b in zip(samples, samples[1:]):
            root_path += math.hypot(*[b[root][axis] - a[root][axis] for axis in horizontal])
    has_root_motion = root_net > 0.01 or root_path > 0.02

    # Foot contact. Relative to the root, so a root-motion clip and an in-place clip present the
    # planted foot the same way -- moving backwards at the gait speed.
    relative: dict[str, list[tuple[float, float]]] = {}
    heights: dict[str, list[float]] = {}
    for side, node in sorted(feet.items()):
        relative[side] = [(frame[node][horizontal[0]] - (frame[root][horizontal[0]] if root
                                                         is not None else 0.0),
                           frame[node][horizontal[1]] - (frame[root][horizontal[1]] if root
                                                         is not None else 0.0))
                          for frame in samples]
        heights[side] = [frame[node][up] for frame in samples]

    low: dict[str, list[bool]] = {}
    for side, series in heights.items():
        lowest, highest = min(series), max(series)
        threshold = lowest + height_fraction * (highest - lowest)
        low[side] = [value <= threshold for value in series]

    # The median relative velocity over every LOW sample of every foot. Stance dominates the set,
    # so this is the gait velocity -- found without having first decided which samples are stance,
    # which is the circularity that makes naive plant detection fail.
    #
    # It is taken as a VECTOR, per component, and that is the whole point. A swinging foot moves at
    # very nearly the same SPEED as a planted one -- in an ideal in-place cycle at exactly the same
    # speed -- so a magnitude test cannot tell stance from swing at all. What separates them is
    # direction: stance goes backwards under the root, swing goes forwards. Comparing magnitudes
    # was the first implementation here and it folded every swing frame into the stance, halving
    # the measured travel and doubling the cycle count.
    velocities: dict[str, list[tuple[float, float]]] = {}
    low_velocities: list[tuple[float, float]] = []
    for side, series in relative.items():
        velocity = []
        for index in range(len(series)):
            a = series[max(index - 1, 0)]
            b = series[min(index + 1, len(series) - 1)]
            span = step * (min(index + 1, len(series) - 1) - max(index - 1, 0))
            velocity.append(((b[0] - a[0]) / span, (b[1] - a[1]) / span) if span > 0 else (0.0, 0.0))
        velocities[side] = velocity
        low_velocities.extend(velocity[i] for i in range(len(series)) if low[side][i])

    median_velocity = (_median([v[0] for v in low_velocities]),
                       _median([v[1] for v in low_velocities]))
    stance_speed = math.hypot(*median_velocity)
    direction = ((median_velocity[0] / stance_speed, median_velocity[1] / stance_speed)
                 if stance_speed > 1e-9 else (0.0, 0.0))

    planted: dict[str, list[bool]] = {}
    for side in relative:
        marks = []
        for index in range(len(relative[side])):
            velocity = velocities[side][index]
            # Forward hemisphere against the median direction, rather than equality with the median
            # VECTOR, so a walk that curves is not rejected the moment its heading rotates.
            along = velocity[0] * direction[0] + velocity[1] * direction[1]
            marks.append(low[side][index] and along > 0.0
                         and abs(math.hypot(*velocity) - stance_speed)
                         <= speed_tolerance * max(stance_speed, 1e-9))
        planted[side] = marks

    # Ground travel: per sample, the mean of the planted feet's backwards relative velocity. The
    # MEAN over simultaneously planted feet, not the sum over stance phases, is what stops double
    # support being counted twice.
    travel = 0.0
    airborne = 0
    slide_residuals: list[float] = []
    last_velocity = (0.0, 0.0)
    for index in range(len(samples)):
        contributing = [velocities[side][index] for side in sorted(planted) if planted[side][index]]
        if contributing:
            ground = (-sum(v[0] for v in contributing) / len(contributing),
                      -sum(v[1] for v in contributing) / len(contributing))
            last_velocity = ground
            if len(contributing) == 2:
                slide_residuals.append(math.hypot(contributing[0][0] - contributing[1][0],
                                                  contributing[0][1] - contributing[1][1]))
        else:
            airborne += 1
            ground = last_velocity  # A flight phase: hold, rather than pretend the body stopped.
        travel += math.hypot(*ground) * step
    foot_travel = travel if feet else 0.0

    # Plant intervals, and the contact times `.chanim` stores. A run that touches both ends of a
    # looping clip is one plant crossing the loop point, not two.
    intervals = []
    for side in sorted(planted):
        runs = _runs(planted[side])
        if len(runs) > 1 and runs[0][0] == 0 and runs[-1][1] == len(planted[side]):
            runs = [(runs[-1][0] - len(planted[side]), runs[0][1])] + runs[1:-1]
        for begin, finish in runs:
            if (finish - begin) * step < 0.02:
                continue  # Two frames of contact is a detection artefact, not a foot plant.
            wraps = begin < 0
            # A wrapped plant keeps its REAL start and its real length, so `end` may run past the
            # clip's duration. Clamping it to [0, duration] instead -- the first version here --
            # threw away most of the contact and made the stance phases look like they never
            # overlapped, which is the one thing this interval list exists to show.
            first = (begin + len(planted[side])) * step if wraps else begin * step
            intervals.append({"foot": side,
                              "start": round(first, 6),
                              "end": round(first + (finish - begin) * step, 6),
                              "wrapsLoopPoint": wraps})
    intervals.sort(key=lambda entry: (entry["start"], entry["foot"]))

    per_foot = [sum(1 for entry in intervals if entry["foot"] == side) for side in sorted(planted)]
    cycles = max(round(sum(per_foot) / len(per_foot)), 1) if per_foot else 1

    if has_root_motion:
        method = "rootMotion"
        clip_travel = root_path
    elif feet and foot_travel > 0:
        method = "footContact"
        clip_travel = foot_travel
    else:
        method = "none"
        clip_travel = 0.0
        if not feet:
            warnings.append("no foot bones and no root motion: this clip carries nothing to "
                            "measure a stride from")

    speed = clip_travel / duration
    is_locomotion = speed >= LOCOMOTION_SPEED_FLOOR
    stride = clip_travel / cycles if is_locomotion else 0.0

    # The cross-check that only exists when both measurements do. Their disagreement is foot
    # sliding -- the thing §47.4 exists to remove -- so it is reported rather than averaged away.
    cross_check = None
    if has_root_motion and feet and foot_travel > 0:
        cross_check = {"rootPath": round(root_path, 6), "footContactPath": round(foot_travel, 6),
                       "relativeDifference": round(abs(root_path - foot_travel)
                                                   / max(root_path, 1e-9), 6)}
        if cross_check["relativeDifference"] > 0.1:
            warnings.append(f"root motion and foot contact disagree by "
                            f"{cross_check['relativeDifference'] * 100:.1f} % "
                            f"({root_path:.3f} m vs {foot_travel:.3f} m): the feet slide")

    if is_locomotion and not intervals:
        warnings.append("the clip travels but no foot plant was detected; footPlants is empty and "
                        "the stride is the whole clip's travel")

    forward = [0.0, 0.0, 0.0]
    if root_net > 1e-6:
        for slot, axis in enumerate(horizontal):
            forward[axis] = round(root_delta[slot] / root_net, 6)

    return {
        "name": name,
        "duration": round(duration, 6),
        "sampleRate": rate,
        "sampleCount": count,
        "isLocomotion": is_locomotion,
        "method": method,
        "strideLength": round(stride, 6),
        "cycles": cycles,
        "clipTravel": round(clip_travel, 6),
        "speed": round(speed, 6),
        "forward": forward,
        "footPlants": sorted(round(max(entry["start"], 0.0), 6) for entry in intervals),
        "plantIntervals": intervals,
        "rootMotion": {"present": has_root_motion, "netDisplacement": round(root_net, 6),
                       "pathLength": round(root_path, 6)},
        "footContactTravel": round(foot_travel, 6),
        "crossCheck": cross_check,
        "footSlideRms": round(math.sqrt(sum(v * v for v in slide_residuals)
                                        / len(slide_residuals)), 6) if slide_residuals else None,
        "airborneFraction": round(airborne / count, 6),
        "stanceSpeed": round(stance_speed, 6),
        "warnings": warnings,
    }


def units_hint(rig: Rig, feet: dict[str, int], root: int | None,
               tracks: dict[int, dict[str, Track]] | None = None) -> tuple[str, float]:
    """Guess the source's units from the skeleton's own size, and say what it measured.

    Measured on the FIRST ANIMATED FRAME, not the rest pose: an exporter is free to leave the rest
    pose at the identity and carry the whole skeleton in the tracks, and a units check that read
    the rest pose would then measure nothing and quietly answer "metres".
    """
    if root is None or not feet:
        return "unknown", 0.0
    positions = rig.world_positions(tracks or {}, 0.0)
    heights = [abs(positions[root][1] - positions[node][1]) for node in feet.values()]
    hip_height = max(heights) if heights else 0.0
    if hip_height <= 0:
        return "unknown", 0.0
    # A hip is roughly half a person. Compare against that, and pick the unit whose ratio is
    # nearest 1 -- the three candidates are a factor of 2.54 and 100 apart, so the bands do not
    # touch and no plausible human lands between them.
    expected = HUMAN_HEIGHT_M * 0.53
    candidates = {"metres": 1.0, "inches": 0.0254, "centimetres": 0.01}
    best = min(candidates, key=lambda unit: abs(math.log(hip_height * candidates[unit] / expected)))
    return best, round(hip_height, 6)


def measure(path: Path, up_axis: str = "y", rate: int = DEFAULT_RATE,
            clip_filter: str | None = None, foot_left: str | None = None,
            foot_right: str | None = None, root_name: str | None = None,
            height_fraction: float = HEIGHT_FRACTION,
            speed_tolerance: float = SPEED_TOLERANCE) -> dict:
    up = {"x": 0, "y": 1, "z": 2}[up_axis]
    document, blob = gltf_io.read_model(path)
    buffers = gltf_io.buffer_bytes(document, blob, path.parent)
    rig = Rig(document, buffers)
    names = [node.get("name", f"node{i}") for i, node in enumerate(rig.nodes)]

    warnings: list[str] = []
    if foot_left or foot_right:
        feet = {}
        for side, wanted in (("left", foot_left), ("right", foot_right)):
            if wanted is None:
                continue
            if wanted not in names:
                raise StrideError(f"{path.name}: no node named '{wanted}'")
            feet[side] = names.index(wanted)
    else:
        feet, foot_warnings = find_feet(names)
        warnings.extend(foot_warnings)

    if root_name is not None:
        if root_name not in names:
            raise StrideError(f"{path.name}: no node named '{root_name}'")
        root = names.index(root_name)
    else:
        root = find_root(document, names, rig.parent)
        if root is None:
            warnings.append("no root joint identified; root motion cannot be measured")

    first_tracks = rig.tracks(animations[0]) if (animations := document.get("animations", [])) \
        else {}
    unit, hip_height = units_hint(rig, feet, root, first_tracks)
    if unit != "metres" and unit != "unknown":
        warnings.append(f"the skeleton is {hip_height:.1f} units from hip to foot, which reads as "
                        f"{unit}, not metres. Every distance below is in the SOURCE's units; scale "
                        f"the source before this measurement reaches Clip::strideLength.")

    if not animations:
        raise StrideError(f"{path.name}: has no animations")

    clips = []
    for animation in animations:
        if clip_filter is not None and animation.get("name", "") != clip_filter:
            continue
        clips.append(measure_clip(rig, animation, feet, root, up, rate, height_fraction,
                                  speed_tolerance))
    if not clips:
        raise StrideError(f"{path.name}: no clip named '{clip_filter}'")

    return {
        "tool": "tools/assets/measure_stride.py",
        "formatVersion": 1,
        "source": {"file": path.name,
                   "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                   "upAxis": up_axis,
                   "unitsHint": unit,
                   "hipHeight": hip_height},
        "skeleton": {"nodeCount": len(names),
                     "rootBone": names[root] if root is not None else None,
                     "footLeft": names[feet["left"]] if "left" in feet else None,
                     "footRight": names[feet["right"]] if "right" in feet else None},
        "settings": {"sampleRate": rate, "heightFraction": height_fraction,
                     "speedTolerance": speed_tolerance,
                     "locomotionSpeedFloor": LOCOMOTION_SPEED_FLOOR},
        "warnings": warnings,
        "clips": clips,
    }


# ------------------------------------------------------------------------------------ fixture ----

def _walk_samples(count: int, duration: float, stride: float, in_place: bool, cycles: int = 1,
                  stance: float = 0.5, scuff: float = 0.0, swing_height: float = 0.12,
                  scale: float = 1.0,
                  offset: tuple[float, float, float] = (0.0, 0.0, 0.0)) -> dict:
    """An analytically exact walk, so the fixture's right answer is known rather than eyeballed.

    Each foot stands for the first half of its own cycle and swings for the second, advancing
    exactly `stride` per cycle; the right foot's cycle is offset by half. Ground travel per cycle is
    therefore exactly `stride`, the plants fall on exactly the half-cycle boundaries, and both the
    root-motion and the foot-contact measurement must return the same number.

    `cycles` is the number of gait cycles the clip contains, which is NOT the same as its duration
    -- the whole point of `strideLength` being per cycle rather than per clip.

    `stance` is the fraction of each cycle a foot spends on the ground. At 0.5 the feet exchange
    instantaneously and only one is ever down. Above 0.5 there is **double support**: a window
    where both are planted, which is what a real walk does and what separates a correct
    implementation from one that sums each foot's stance displacement and counts the overlap twice.
    A real walk is around 0.6.

    `scuff` reproduces a retarget artefact rather than a gait: for the first `scuff` of its swing
    the foot stays on the floor and slides BACKWARDS at four times the gait rate. It is low and it
    is moving the right way, so height and direction both accept it; only the speed band rejects
    it. Left in, it inflates the measured travel by about a fifth.
    """
    speed = stride * cycles / duration
    hips, left, right = [], [], []
    for index in range(count):
        t = duration * index / (count - 1)
        phase = cycles * t / duration

        def foot(shift: float) -> tuple[float, float]:
            """A foot whose stance begins at phase `shift` of every cycle."""
            local = phase - shift
            which = math.floor(local)
            within = local - which
            base = (which + shift) * stride     # the plant it is standing on, or just left
            if within < stance:
                return base, 0.0
            swing = (within - stance) / (1.0 - stance)
            if scuff > 0.0:
                if swing < scuff:
                    return base - stride * 2.0 * swing, 0.002
                lifted = base - stride * 2.0 * scuff
                swing = (swing - scuff) / (1.0 - scuff)
                return (lifted + (base + stride - lifted) * swing,
                        swing_height * math.sin(math.pi * swing))
            return base + stride * swing, swing_height * math.sin(math.pi * swing)

        left_x, left_y = foot(0.0)
        right_x, right_y = foot(0.5)
        hip_x = speed * t
        if in_place:
            left_x -= hip_x
            right_x -= hip_x
            hip_x = 0.0
        hips.append(((hip_x + offset[0]) * scale, (0.95 + offset[1]) * scale, offset[2] * scale))
        left.append(((left_x + offset[0]) * scale, (left_y + offset[1]) * scale,
                     (0.1 + offset[2]) * scale))
        right.append(((right_x + offset[0]) * scale, (right_y + offset[1]) * scale,
                      (-0.1 + offset[2]) * scale))
    return {"Hips": hips, "LeftFoot": left, "RightFoot": right}


def _turn_samples(count: int, duration: float) -> dict:
    """A turn in place: the hips rotate, the feet pivot, and nothing travels."""
    hips, left, right = [], [], []
    for index in range(count):
        angle = math.pi / 2 * index / (count - 1)
        hips.append((0.0, 0.95, 0.0))
        for side, series in ((1.0, left), (-1.0, right)):
            radius = 0.1 * side
            series.append((-radius * math.sin(angle), 0.0, radius * math.cos(angle)))
    return {"Hips": hips, "LeftFoot": left, "RightFoot": right}


def _fixture_glb(path: Path, clips: dict[str, tuple[float, dict]],
                 foot_names: tuple[str, str] = ("LeftFoot", "RightFoot")) -> None:
    """Write a small rigged `.glb` whose joints carry the given per-clip world position tracks.

    The hierarchy is Hips -> {LeftUpLeg -> LeftFoot, RightUpLeg -> RightFoot}: three levels deep on
    purpose, so the tool's parent composition is exercised rather than a flat list of nodes. The
    legs carry no animation, so a foot's world position is its own translation plus its parents' --
    which is exactly the composition being tested.
    """
    names = ["Hips", "LeftUpLeg", foot_names[0], "RightUpLeg", foot_names[1]]
    parents = {1: 0, 2: 1, 3: 0, 4: 3}
    rest_translation = [(0.0, 0.0, 0.0), (0.1, -0.45, 0.0), (0.0, 0.0, 0.0),
                        (-0.1, -0.45, 0.0), (0.0, 0.0, 0.0)]
    blob = bytearray()
    views: list[dict] = []
    accessors: list[dict] = []

    def add(payload: bytes, accessor: dict) -> int:
        while len(blob) % 4:
            blob.append(0)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(payload)})
        blob.extend(payload)
        accessor["bufferView"] = len(views) - 1
        accessors.append(accessor)
        return len(accessors) - 1

    animations = []
    for clip_name, (duration, tracks) in sorted(clips.items()):
        count = len(next(iter(tracks.values())))
        times = [duration * i / (count - 1) for i in range(count)]
        time_accessor = add(b"".join(struct.pack("<f", t) for t in times),
                            {"componentType": 5126, "count": count, "type": "SCALAR",
                             "min": [times[0]], "max": [times[-1]]})
        # Every node's WORLD track, animated or not, computed parent-first. A node with no track
        # still moves, because its parent does -- subtracting only the animated parents would add
        # the rest pose of the leg twice and put the feet somewhere nobody authored.
        world_track: dict[int, list[tuple[float, float, float]]] = {}
        for node in range(len(names)):
            given = tracks.get(names[node])
            parent = parents.get(node)
            base = world_track[parent] if parent is not None else [(0.0, 0.0, 0.0)] * count
            if given is not None:
                world_track[node] = list(given)
            else:
                rest = rest_translation[node]
                world_track[node] = [tuple(base[i][k] + rest[k] for k in range(3))
                                     for i in range(count)]

        channels, samplers = [], []
        for node_name, series in sorted(tracks.items()):
            node = names.index(node_name)
            parent = parents.get(node)
            local = []
            for index, position in enumerate(series):
                if parent is None:
                    local.append(position)
                else:
                    local.append(tuple(position[i] - world_track[parent][index][i]
                                       for i in range(3)))
            values = add(b"".join(struct.pack("<3f", *v) for v in local),
                         {"componentType": 5126, "count": count, "type": "VEC3",
                          "min": [min(v[i] for v in local) for i in range(3)],
                          "max": [max(v[i] for v in local) for i in range(3)]})
            samplers.append({"input": time_accessor, "output": values, "interpolation": "LINEAR"})
            channels.append({"sampler": len(samplers) - 1,
                             "target": {"node": node, "path": "translation"}})
        animations.append({"name": clip_name, "channels": channels, "samplers": samplers})

    nodes = []
    for index, name in enumerate(names):
        node: dict = {"name": name}
        children = [child for child, parent in parents.items() if parent == index]
        if children:
            node["children"] = sorted(children)
        nodes.append(node)
    # A rest pose so the legs are not all at the origin; the animation overrides Hips and the feet,
    # and the un-animated legs in between are exactly what makes the composition worth testing.
    for index, rest in enumerate(rest_translation):
        if any(rest):
            nodes[index]["translation"] = list(rest)

    document = {
        "asset": {"version": "2.0", "generator": "cna-house measure_stride.py --make-fixture"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": nodes,
        "skins": [{"joints": list(range(len(names))), "name": "Body"}],
        "animations": animations,
        "accessors": accessors, "bufferViews": views, "buffers": [{"byteLength": len(blob)}],
    }
    gltf_io.write_glb(path, document, bytes(blob))


#: The fixture's exact answers, asserted by the selftest and printed by `--make-fixture`.
FIXTURE_STRIDE = 1.4
FIXTURE_DURATION = 1.0


def make_fixture(directory: Path) -> list[Path]:
    directory.mkdir(parents=True, exist_ok=True)
    written = []
    stride, duration = FIXTURE_STRIDE, FIXTURE_DURATION

    def write(name: str, clips: dict, feet=("LeftFoot", "RightFoot")) -> Path:
        path = directory / name
        _fixture_glb(path, clips, feet)
        written.append(path)
        return path

    # The same walk in both conventions. These must measure the same stride.
    write("walk_rootmotion.glb",
          {"walk_fwd": (duration, _walk_samples(121, duration, stride, in_place=False))})
    write("walk_inplace.glb",
          {"walk_fwd": (duration, _walk_samples(121, duration, stride, in_place=True))})

    # Sampling rate: the same motion at 24 and at 240 source keys.
    write("walk_inplace_24.glb",
          {"walk_fwd": (duration, _walk_samples(25, duration, stride, in_place=True))})
    write("walk_inplace_240.glb",
          {"walk_fwd": (duration, _walk_samples(241, duration, stride, in_place=True))})

    # Translated 10 m away from the origin: nothing about a stride depends on where it happened.
    write("walk_translated.glb",
          {"walk_fwd": (duration, _walk_samples(121, duration, stride, in_place=True,
                                                offset=(10.0, 0.0, -4.0)))})

    # The same skeleton in inches, which is how CMU's raw data arrives.
    write("walk_inches.glb",
          {"walk_fwd": (duration, _walk_samples(121, duration, stride, in_place=True,
                                                scale=1 / 0.0254))})

    # Two cycles in one clip: the stride is per cycle, not per clip.
    two = _walk_samples(241, 2 * duration, stride, in_place=True, cycles=2)
    write("walk_two_cycles.glb", {"walk_fwd": (2 * duration, two)})

    # A real walk's 60 % stance, so 20 % of the cycle has BOTH feet planted. The ground travel is
    # still exactly one stride; an implementation that sums stance displacements reports 1.2x it.
    write("walk_double_support.glb",
          {"walk_fwd": (duration, _walk_samples(121, duration, stride, in_place=True,
                                                stance=0.6))})

    # A foot that drags backwards on the floor for the first 12 % of its swing -- low, moving the
    # right way, four times too fast. Height and direction both accept those frames; only the speed
    # band throws them out, and without it the measured stride is ~18 % long.
    write("walk_scuff.glb",
          {"walk_fwd": (duration, _walk_samples(121, duration, stride, in_place=True,
                                                scuff=0.12))})

    # Not locomotion: a turn in place. Must report strideLength 0.
    write("turn_in_place.glb", {"turn_left_90": (0.7, _turn_samples(85, 0.7))})

    # Feet the matcher cannot name, so the in-place clip has nothing to measure from.
    write("walk_unnamed_feet.glb",
          {"walk_fwd": (duration, {"Hips": _walk_samples(121, duration, stride, True)["Hips"],
                                   "Paddle01": _walk_samples(121, duration, stride,
                                                             True)["LeftFoot"],
                                   "Paddle02": _walk_samples(121, duration, stride,
                                                             True)["RightFoot"]})},
          feet=("Paddle01", "Paddle02"))

    # A deliberately sliding walk. Sliding IS the foot moving under the root by less than the root
    # travels, so it is built that way: keep the root motion, shrink each foot's motion RELATIVE to
    # the root to 70 %. The root then says 1.4 m and the feet say 0.98 m, and the tool must report
    # the disagreement rather than average the two.
    clean = _walk_samples(121, duration, stride, in_place=False)
    slid = {"Hips": clean["Hips"]}
    for side in ("LeftFoot", "RightFoot"):
        slid[side] = [tuple(hip[k] + 0.7 * (foot[k] - hip[k]) if k != 1 else foot[k]
                            for k in range(3))
                      for hip, foot in zip(clean["Hips"], clean[side])]
    write("walk_sliding.glb", {"walk_fwd": (duration, slid)})

    return written


# ----------------------------------------------------------------------------------- selftest ----

def selftest() -> int:
    failures = 0

    def require(condition: bool, message: str) -> None:
        nonlocal failures
        if condition:
            print(f"  {message}")
        else:
            print(f"  SELFTEST FAILED: {message}", file=sys.stderr)
            failures += 1

    workspace = Path(tempfile.mkdtemp(prefix="measure_stride_selftest_"))
    try:
        source = workspace / "src"
        make_fixture(source)
        tolerance = 0.03  # 3 %, which at 120 Hz is a couple of samples of stance either side.

        root_result = measure(source / "walk_rootmotion.glb")
        root_clip = root_result["clips"][0]
        place_clip = measure(source / "walk_inplace.glb")["clips"][0]

        # 1. Duration is read from the sampler times, not guessed.
        require(abs(root_clip["duration"] - FIXTURE_DURATION) < 1e-6,
                f"duration is {root_clip['duration']} s")

        # 2/3. Both conventions measure the SAME stride, and it is the authored one.
        require(abs(root_clip["strideLength"] - FIXTURE_STRIDE) < FIXTURE_STRIDE * tolerance,
                f"root-motion stride is {root_clip['strideLength']:.4f} m "
                f"(authored {FIXTURE_STRIDE})")
        require(abs(place_clip["strideLength"] - FIXTURE_STRIDE) < FIXTURE_STRIDE * tolerance,
                f"in-place stride is {place_clip['strideLength']:.4f} m "
                f"(authored {FIXTURE_STRIDE})")
        require(abs(root_clip["strideLength"] - place_clip["strideLength"])
                < FIXTURE_STRIDE * tolerance,
                "root-motion and in-place measurements of the same walk agree")

        # 4. ...by DIFFERENT routes. If both said `rootMotion` the agreement would be trivial.
        require(root_clip["method"] == "rootMotion" and place_clip["method"] == "footContact",
                f"the two clips are measured by different methods "
                f"({root_clip['method']} and {place_clip['method']})")

        # 5. Two foot plants per cycle, at the authored times.
        require(len(place_clip["footPlants"]) == 2,
                f"two foot plants per cycle: {place_clip['footPlants']}")
        require(all(abs(a - b) < 0.03 for a, b in zip(place_clip["footPlants"], [0.0, 0.5])),
                f"the plants are at 0.00 s and 0.50 s: {place_clip['footPlants']}")
        require(place_clip["footPlants"] == sorted(place_clip["footPlants"])
                and all(0.0 <= t <= place_clip["duration"] for t in place_clip["footPlants"]),
                "footPlants is ascending and inside [0, duration], as `.chanim` requires")
        require({entry["foot"] for entry in place_clip["plantIntervals"]} == {"left", "right"},
                "one plant from each foot, not two from one")

        # 6. Sample-rate independence, across a 10x range of SOURCE key rates.
        strides = {}
        for name in ("walk_inplace_24.glb", "walk_inplace.glb", "walk_inplace_240.glb"):
            strides[name] = measure(source / name)["clips"][0]["strideLength"]
        spread = max(strides.values()) - min(strides.values())
        require(spread < FIXTURE_STRIDE * 0.05,
                f"24, 120 and 240 source keys give the same stride within 5 % "
                f"({', '.join(f'{v:.3f}' for v in strides.values())})")

        # 7. Translation invariance: a stride is a distance, not a place.
        translated = measure(source / "walk_translated.glb")["clips"][0]
        require(abs(translated["strideLength"] - place_clip["strideLength"]) < 1e-4,
                f"moving the whole clip 10 m away changes nothing "
                f"({translated['strideLength']:.4f} m)")

        # 8. Scale proportionality AND the units warning -- a stride of 55 must not pass silently.
        inches = measure(source / "walk_inches.glb")
        inch_clip = inches["clips"][0]
        require(abs(inch_clip["strideLength"] - place_clip["strideLength"] / 0.0254)
                < place_clip["strideLength"] / 0.0254 * tolerance,
                f"an inch-scaled source measures {inch_clip['strideLength']:.1f} units, "
                f"proportionally")
        require(inches["source"]["unitsHint"] == "inches"
                and any("inches" in w for w in inches["warnings"]),
                f"...and it is REPORTED as inches, not passed on as metres "
                f"({inches['source']['unitsHint']})")
        require(measure(source / "walk_inplace.glb")["source"]["unitsHint"] == "metres",
                "a metric source is reported as metres")

        # 9. Stride is per CYCLE, not per clip.
        two = measure(source / "walk_two_cycles.glb")["clips"][0]
        require(two["cycles"] == 2
                and abs(two["strideLength"] - FIXTURE_STRIDE) < FIXTURE_STRIDE * tolerance,
                f"a two-cycle clip reports {two['cycles']} cycles and a per-cycle stride of "
                f"{two['strideLength']:.4f} m")
        require(abs(two["clipTravel"] - 2 * FIXTURE_STRIDE) < 2 * FIXTURE_STRIDE * tolerance,
                f"...and its whole-clip travel is {two['clipTravel']:.4f} m")

        # 9b. Double support: 20 % of the cycle has both feet down, and it must be counted ONCE.
        #     Summing the two feet's stance displacements -- the obvious implementation -- reports
        #     1.2x the stride here, which is why the fixture has this case at all.
        double = measure(source / "walk_double_support.glb")["clips"][0]
        require(abs(double["strideLength"] - FIXTURE_STRIDE) < FIXTURE_STRIDE * tolerance,
                f"a walk with 20 % double support still measures "
                f"{double['strideLength']:.4f} m, not 1.2x it")
        require(len(double["plantIntervals"]) == 2 and double["cycles"] == 1,
                f"...from {len(double['plantIntervals'])} plants over "
                f"{double['cycles']} cycle(s)")
        require(sum(entry["end"] - entry["start"] for entry in double["plantIntervals"])
                > double["duration"] * 1.1,
                f"...and the stance phases really do overlap "
                f"({sum(e['end'] - e['start'] for e in double['plantIntervals']):.3f} s of stance "
                f"in a {double['duration']:.2f} s clip, so both feet are down at once)")

        # 9c. A dragging foot is rejected by the SPEED band alone. It is low and it moves
        #     backwards, so neither the height test nor the direction test can see it.
        scuffed = measure(source / "walk_scuff.glb")["clips"][0]
        require(abs(scuffed["strideLength"] - FIXTURE_STRIDE) < FIXTURE_STRIDE * tolerance,
                f"a walk whose feet drag for 12 % of the swing still measures "
                f"{scuffed['strideLength']:.4f} m")
        loose = measure(source / "walk_scuff.glb", speed_tolerance=100.0)["clips"][0]
        require(loose["strideLength"] > FIXTURE_STRIDE * 1.08,
                f"...and with the speed band opened up it does not "
                f"({loose['strideLength']:.4f} m), so the band is load-bearing")

        # 10. A turn in place is NOT locomotion and gets stride 0, as §47.4 requires.
        turn = measure(source / "turn_in_place.glb")["clips"][0]
        require(not turn["isLocomotion"] and turn["strideLength"] == 0.0,
                f"a turn in place: isLocomotion={turn['isLocomotion']}, "
                f"strideLength={turn['strideLength']}")

        # 11. Unfindable feet on an in-place clip: 0 and a REASON, never a plausible-looking guess.
        unnamed = measure(source / "walk_unnamed_feet.glb")
        unnamed_clip = unnamed["clips"][0]
        require(unnamed["skeleton"]["footLeft"] is None
                and any("could not identify" in w for w in unnamed["warnings"]),
                "unrecognisable foot bones are reported, not silently ignored")
        require(unnamed_clip["method"] == "none" and unnamed_clip["strideLength"] == 0.0,
                f"...and the clip measures 0 with method '{unnamed_clip['method']}' rather than "
                f"inventing a stride")
        # ...but naming them explicitly recovers the full measurement.
        named = measure(source / "walk_unnamed_feet.glb", foot_left="Paddle01",
                        foot_right="Paddle02")["clips"][0]
        require(abs(named["strideLength"] - FIXTURE_STRIDE) < FIXTURE_STRIDE * tolerance,
                f"--foot-left/--foot-right recovers it: {named['strideLength']:.4f} m")

        # 12. Foot sliding is MEASURED, not averaged away.
        sliding = measure(source / "walk_sliding.glb")["clips"][0]
        require(sliding["crossCheck"] is not None
                and sliding["crossCheck"]["relativeDifference"] > 0.1
                and any("slide" in w for w in sliding["warnings"]),
                f"a walk whose feet slide 30 % is reported as sliding "
                f"({sliding['crossCheck']['relativeDifference'] * 100:.0f} % disagreement)")
        require(root_clip["crossCheck"] is not None
                and root_clip["crossCheck"]["relativeDifference"] < 0.05
                and not any("slide" in w for w in root_clip["warnings"]),
                f"...and the clean walk is not "
                f"({root_clip['crossCheck']['relativeDifference'] * 100:.1f} %)")

        # 13. Bone-name matching across the conventions this project will actually meet.
        cases = {"LeftFoot": "left", "RightFoot": "right", "mixamorig:LeftToeBase": "left",
                 "foot.L": "left", "foot.R": "right", "L_Ankle": "left", "ankle_r": "right",
                 "lFoot": "left", "RightUpLeg": "right"}
        wrong = {name: _side_of(name) for name, want in cases.items() if _side_of(name) != want}
        require(not wrong, f"bone-name sides across CMU, Mixamo, Rigify and the underscore "
                           f"conventions ({wrong or 'all correct'})")
        # A single letter must be delimited, or `Ball` and `Roll` become sides.
        require(_side_of("Ball") is None and _side_of("Roll") is None
                and _side_of("Collar") is None,
                "an undelimited l or r inside a word is not read as a side")
        feet, _ = find_feet(["Hips", "LeftUpLeg", "LeftFoot", "LeftToeBase",
                             "RightUpLeg", "RightFoot", "RightToeBase"])
        require(feet == {"left": 2, "right": 5},
                "the ankle is preferred to the toe when a skeleton has both")

        # 14. Determinism, including across a repeated process-free run.
        first = json.dumps(measure(source / "walk_inplace.glb"), sort_keys=True)
        second = json.dumps(measure(source / "walk_inplace.glb"), sort_keys=True)
        require(first == second, "two measurements of the same file are identical")

        # 15. Malformed sources are refused with a reason, not measured into nonsense.
        bad = workspace / "bad"
        bad.mkdir()
        document, blob = gltf_io.read_model(source / "walk_inplace.glb")

        broken = json.loads(json.dumps(document))
        for accessor in broken["accessors"]:
            if accessor["type"] == "SCALAR":
                accessor["max"] = accessor["min"] = [0.0]
        times = broken["animations"][0]["samplers"][0]["input"]
        view = broken["bufferViews"][broken["accessors"][times]["bufferView"]]
        mutable = bytearray(blob)
        for i in range(broken["accessors"][times]["count"]):
            struct.pack_into("<f", mutable, view["byteOffset"] + 4 * i, 0.0)
        gltf_io.write_glb(bad / "zero_duration.glb", broken, bytes(mutable))
        try:
            measure(bad / "zero_duration.glb")
            require(False, "a zero-duration clip is refused")
        except StrideError as error:
            require("duration" in str(error), f"a zero-duration clip is refused: {error}")

        reversed_doc = json.loads(json.dumps(document))
        mutable = bytearray(blob)
        view = reversed_doc["bufferViews"][reversed_doc["accessors"][times]["bufferView"]]
        count = reversed_doc["accessors"][times]["count"]
        for i in range(count):
            struct.pack_into("<f", mutable, view["byteOffset"] + 4 * i, (count - 1 - i) / 120.0)
        gltf_io.write_glb(bad / "unsorted.glb", reversed_doc, bytes(mutable))
        try:
            measure(bad / "unsorted.glb")
            require(False, "non-ascending sampler times are refused")
        except StrideError as error:
            require("ascending" in str(error),
                    f"non-ascending sampler times are refused: {error}")

        nan_doc = json.loads(json.dumps(document))
        mutable = bytearray(blob)
        output = nan_doc["animations"][0]["samplers"][1]["output"]
        view = nan_doc["bufferViews"][nan_doc["accessors"][output]["bufferView"]]
        struct.pack_into("<f", mutable, view["byteOffset"], float("nan"))
        gltf_io.write_glb(bad / "nan.glb", nan_doc, bytes(mutable))
        try:
            measure(bad / "nan.glb")
            require(False, "a non-finite position is refused")
        except StrideError as error:
            require("non-finite" in str(error), f"a non-finite position is refused: {error}")

        no_anim = json.loads(json.dumps(document))
        no_anim["animations"] = []
        gltf_io.write_glb(bad / "no_animation.glb", no_anim, blob)
        try:
            measure(bad / "no_animation.glb")
            require(False, "a file with no animation is refused")
        except StrideError as error:
            require("no animations" in str(error), f"a file with no animation is refused: {error}")

        # 16. Everything `.chanim` §3.3 needs for a Clip is present and in range.
        for key in ("duration", "strideLength", "footPlants"):
            require(key in place_clip, f"the output carries `.chanim`'s {key}")
        require(len(place_clip["footPlants"]) <= 64,
                "footPlantCount stays inside `.chanim`'s 0..64")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("measure_stride: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("models", nargs="*", type=Path)
    parser.add_argument("--clip", help="measure only the clip with this name")
    parser.add_argument("--up", choices=("x", "y", "z"), default="y",
                        help="which axis is up (default y, glTF's convention)")
    parser.add_argument("--rate", type=int, default=DEFAULT_RATE,
                        help=f"resampling rate in Hz (default {DEFAULT_RATE})")
    parser.add_argument("--foot-left", help="name the left foot bone instead of matching it")
    parser.add_argument("--foot-right", help="name the right foot bone instead of matching it")
    parser.add_argument("--root", help="name the root bone instead of matching it")
    parser.add_argument("--height-fraction", type=float, default=HEIGHT_FRACTION)
    parser.add_argument("--speed-tolerance", type=float, default=SPEED_TOLERANCE)
    parser.add_argument("--json", type=Path, help="write the measurement to this file")
    parser.add_argument("--make-fixture", type=Path, metavar="DIR")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    if args.make_fixture is not None:
        for path in make_fixture(args.make_fixture):
            print(f"{path}  {path.stat().st_size} bytes")
        print(f"authored stride {FIXTURE_STRIDE} m over {FIXTURE_DURATION} s")
        return 0

    if not args.models:
        parser.print_help()
        return 2

    status = 0
    for path in args.models:
        try:
            result = measure(path, args.up, args.rate, args.clip, args.foot_left, args.foot_right,
                             args.root, args.height_fraction, args.speed_tolerance)
        except StrideError as error:
            print(f"measure_stride: {error}", file=sys.stderr)
            status = 1
            continue

        if args.json is not None:
            args.json.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                                 encoding="utf-8")
        print(f"# {path.name}  units={result['source']['unitsHint']}  "
              f"root={result['skeleton']['rootBone']}  "
              f"feet={result['skeleton']['footLeft']}/{result['skeleton']['footRight']}")
        for warning in result["warnings"]:
            print(f"  warning: {warning}")
        for clip in result["clips"]:
            print(f"  {clip['name']:<16} {clip['duration']:6.3f} s  "
                  f"stride {clip['strideLength']:7.4f}  "
                  f"cycles {clip['cycles']}  speed {clip['speed']:6.3f}  "
                  f"[{clip['method']}]"
                  f"{'' if clip['isLocomotion'] else '  not locomotion'}")
            if clip["footPlants"]:
                print(f"    plants  {', '.join(f'{t:.3f}' for t in clip['footPlants'])}")
            for warning in clip["warnings"]:
                print(f"    warning: {warning}")
    return status


if __name__ == "__main__":
    sys.exit(main())
