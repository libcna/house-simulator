#!/usr/bin/env python3
"""anim_extract.py -- write a source `.glb`'s skeleton and clips into a `.chanim` sidecar.

`HOUSE-00223`. `docs/anim-format.md` is normative and `src/animation/ChanimReader.cpp`
(`HOUSE-00167`) is the reader; this is the writer, and the three have to agree exactly. The reader's
own tests build their bytes by hand precisely because this file did not exist yet -- so the check
that matters most is not that this tool runs, but that what it writes is what that reader accepts.

## The one thing the compiled model cannot carry

`HOUSE-00074` measured it: **vertex blend indices are skin-local**, not `Model::Bones` indices. Slot
*i* of the skinning palette is joint *i* of this skin, and nothing in the compiled `.cnb` reproduces
which bone that is. `skin.joints` IS the blend-index order, so this tool writes the joint names in
exactly that order and changes nothing about it. Everything else in the file could in principle be
recovered from the source; that list could not.

## Matrices: the flat arrays are identical, and that is worth knowing

glTF stores a matrix **column-major** for a column-vector convention (`v' = M v`); XNA's `Matrix` is
**row-major** for a row-vector convention (`v' = v M`), which makes XNA's matrix the transpose of
glTF's. Transposing a column-major array yields a row-major array of the transpose, so the sixteen
floats are **byte-identical** and `inverseBindMatrices` is copied straight through. Translation
lands at `M41..M43`, which is elements 12, 13 and 14 either way; the selftest asserts exactly that,
because "just copy it" is the kind of shortcut that is either right for a reason or wrong forever.

## Decimation

A track is simplified greedily: keep a key only where dropping it would move the interpolated pose
further than the tolerance -- 0.1 mm of translation, 0.05 degrees of rotation, 0.01 % of scale by
default. Rotation is compared as an **angle between quaternions**, not as a component distance,
because `q` and `-q` are the same rotation and a component check calls them 180 degrees apart.

A bone whose simplified track is constant **and equal to its bind pose** is written with **no keys
at all**. `docs/anim-format.md` makes an empty range legal and says such a bone holds its bind pose,
and on a real rig most bones in most clips do exactly that.

    tools/assets/anim_extract.py character.glb -o Content/Anim/character.chanim
    tools/assets/anim_extract.py character.glb -o out.chanim --loop walk_fwd,idle
    tools/assets/anim_extract.py --make-fixture <dir>
    tools/assets/anim_extract.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import math
import shutil
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gltf_io  # noqa: E402
import measure_stride  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

AnimError = gltf_io.GltfError

MAGIC = b"CHAN"
VERSION = 1
#: `Byte4` blend indices cannot address more (`HOUSE-00074`). NOT 72: `SkinnedEffect::MaxBones`
#: limits what one DRAW may use, not what a skeleton may contain.
MAX_BONES = 256
MAX_CLIPS = 4096
MAX_FOOT_PLANTS = 64
MAX_KEYS = 1 << 20

#: Default decimation tolerances. Translation in metres, rotation in degrees, scale as a ratio.
#: 0.1 mm is a tenth of the smallest detail any of this project's models carries, and 0.05 degrees
#: at a 0.8 m limb is 0.7 mm at the fingertip -- both below what a 1600x900 frame can show.
TOLERANCE_TRANSLATION = 1e-4
TOLERANCE_ROTATION_DEGREES = 0.05
TOLERANCE_SCALE = 1e-4

#: How close the first and last pose must be for a clip to be inferred as looping.
LOOP_TOLERANCE = 1e-3

#: The fixture's end-of-clip hips rotation: 40 degrees about the normalised axis (1, 2, 3). Its four
#: components are all different on purpose. An identity quaternion -- what this fixture used first
#: -- cannot show a `w`-first write at either end of the round trip, and an injected `w`-first
#: writer passed every test until this value replaced it.
FIXTURE_ROTATION = (0.0914092, 0.1828184, 0.2742276, 0.9396926)


# ------------------------------------------------------------------------------------- writing ----

class Blob:
    """The `.chanim` byte stream. Little-endian, no alignment, no seeking -- §2 of the format."""

    def __init__(self) -> None:
        self.data = bytearray()

    def u32(self, value: int) -> None:
        if not 0 <= value < (1 << 32):
            raise AnimError(f"internal: {value} does not fit a u32")
        self.data += struct.pack("<I", value)

    def i32(self, value: int) -> None:
        self.data += struct.pack("<i", value)

    def f32(self, value: float) -> None:
        if not math.isfinite(value):
            # A NaN written here is a file the reader rejects at load, in a message that names the
            # clip but not the joint. Refusing at the writer names both.
            raise AnimError(f"internal: refusing to write a non-finite float ({value})")
        self.data += struct.pack("<f", value)

    def name(self, text: str) -> None:
        encoded = text.encode("utf-8")
        if len(encoded) > 0xFFFF:
            raise AnimError(f"the name {text!r} is longer than a u16 length can describe")
        self.data += struct.pack("<H", len(encoded)) + encoded

    def matrix(self, values) -> None:
        if len(values) != 16:
            raise AnimError(f"internal: a matrix has {len(values)} floats, not 16")
        for value in values:
            self.f32(value)


def quaternion_matrix(rotation, scale, translation) -> list[float]:
    """A local TRS as the sixteen floats the format stores.

    Built in glTF's column-major order and emitted in that same order, which -- see the module
    docstring -- IS XNA's row-major order for the transpose, and the transpose is what the
    row-vector convention wants.
    """
    x, y, z, w = rotation
    rot = [
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ]
    flat: list[float] = []
    for column in range(4):
        for row in range(4):
            if column == 3:
                flat.append(translation[row] if row < 3 else 1.0)
            elif row == 3:
                flat.append(0.0)
            else:
                flat.append(rot[row][column] * scale[column])
    return flat


# ------------------------------------------------------------------------------------ skeleton ----

def read_skeleton(document: dict, buffers: list[bytes]) -> tuple[list[dict], int]:
    """The joints, in `skin.joints` order, and the root joint's index."""
    skins = document.get("skins", [])
    if len(skins) != 1:
        # §47.0's rule is offline and absolute, and `skin_split.py` (`HOUSE-00224`) is the fix. A
        # tool that silently took skin 0 would produce a sidecar that binds half a character.
        raise AnimError(f"declares {len(skins)} skins; a runtime .glb has exactly one "
                        f"(cna-house.md §47.0). Split it with tools/assets/skin_split.py.")
    skin = skins[0]
    joints = skin.get("joints", [])
    if not joints:
        raise AnimError("the skin has no joints")
    if len(joints) > MAX_BONES:
        raise AnimError(f"the skin has {len(joints)} joints; the format caps a skeleton at "
                        f"{MAX_BONES} because Byte4 blend indices cannot address more")

    nodes = document.get("nodes", [])
    parent_of = {}
    for index, node in enumerate(nodes):
        for child in node.get("children", []):
            parent_of[child] = index

    slot_of = {node: slot for slot, node in enumerate(joints)}
    inverse_binds = None
    if "inverseBindMatrices" in skin:
        inverse_binds = gltf_io.read_accessor(document, buffers, skin["inverseBindMatrices"])
        if len(inverse_binds) != len(joints):
            raise AnimError(f"inverseBindMatrices has {len(inverse_binds)} entries for "
                            f"{len(joints)} joints")

    bones = []
    for slot, node_index in enumerate(joints):
        node = nodes[node_index]
        if "matrix" in node:
            raise AnimError(f"joint '{node.get('name', node_index)}' uses `matrix` rather than TRS; "
                            f"re-export with TRS, because a matrix cannot be interpolated against "
                            f"animated TRS")
        parent = parent_of.get(node_index)
        parent_slot = slot_of.get(parent, -1) if parent is not None else -1
        if parent_slot >= slot:
            # §3.2: parents strictly precede children. It is what makes the absolute-transform walk
            # a single forward pass and makes a cycle unrepresentable.
            raise AnimError(f"joint '{node.get('name', node_index)}' at slot {slot} has its parent "
                            f"at slot {parent_slot}; skin.joints must list a parent before its "
                            f"child")
        translation = tuple(node.get("translation", (0.0, 0.0, 0.0)))
        rotation = tuple(node.get("rotation", (0.0, 0.0, 0.0, 1.0)))
        scale = tuple(node.get("scale", (1.0, 1.0, 1.0)))
        bones.append({
            "slot": slot,
            "node": node_index,
            "name": node.get("name", f"joint{slot}"),
            "parent": parent_slot,
            "bind": {"translation": translation, "rotation": rotation, "scale": scale},
            "bindPose": quaternion_matrix(rotation, scale, translation),
            # Copied STRAIGHT THROUGH -- see the module docstring. Identity when the skin declares
            # none, which glTF allows and which means the bind pose is already the identity.
            "inverseBindPose": (list(inverse_binds[slot]) if inverse_binds is not None
                                else [1.0, 0.0, 0.0, 0.0,
                                      0.0, 1.0, 0.0, 0.0,
                                      0.0, 0.0, 1.0, 0.0,
                                      0.0, 0.0, 0.0, 1.0]),
        })

    names = [bone["name"] for bone in bones]
    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        # The name list IS the binding (`HOUSE-00074`); a duplicate makes it ambiguous, and
        # `ClipLibrary::BindTo` would resolve both to whichever `Model::Bones` entry it met first.
        raise AnimError(f"duplicate joint name(s) {duplicates}; the name list is the binding and a "
                        f"duplicate makes it ambiguous")

    roots = [bone["slot"] for bone in bones if bone["parent"] == -1]
    root = roots[0] if len(roots) == 1 else -1
    return bones, root


# --------------------------------------------------------------------------------------- clips ----

def _angle_between(a, b) -> float:
    """Degrees between two rotations. `q` and `-q` are the SAME rotation, so the dot is taken as an
    absolute value -- a component-wise distance calls them 180 degrees apart and keeps every key."""
    dot = abs(sum(x * y for x, y in zip(a, b)))
    return math.degrees(2.0 * math.acos(max(min(dot, 1.0), -1.0)))


def _lerp(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def _fits(samples: list[dict], start: int, end: int, tolerance: dict) -> bool:
    """Whether every sample strictly between `start` and `end` is within tolerance of the straight
    interpolation between them -- which is exactly what the runtime sampler will do."""
    t0 = samples[start]["time"]
    t1 = samples[end]["time"]
    span = t1 - t0
    for index in range(start + 1, end):
        sample = samples[index]
        t = 0.0 if span <= 0 else (sample["time"] - t0) / span
        translation = _lerp(samples[start]["translation"], samples[end]["translation"], t)
        if max(abs(x - y) for x, y in zip(translation, sample["translation"])) > \
                tolerance["translation"]:
            return False
        scale = _lerp(samples[start]["scale"], samples[end]["scale"], t)
        if max(abs(x - y) for x, y in zip(scale, sample["scale"])) > tolerance["scale"]:
            return False
        rotation = measure_stride._slerp(samples[start]["rotation"], samples[end]["rotation"], t)
        if _angle_between(rotation, sample["rotation"]) > tolerance["rotationDegrees"]:
            return False
    return True


def decimate(samples: list[dict], tolerance: dict) -> list[dict]:
    """Greedy curve simplification: extend a span while every dropped sample stays inside it."""
    if len(samples) <= 2:
        return list(samples)
    kept = [0]
    anchor = 0
    index = 1
    while index < len(samples) - 1:
        if not _fits(samples, anchor, index + 1, tolerance):
            kept.append(index)
            anchor = index
        index += 1
    kept.append(len(samples) - 1)
    return [samples[i] for i in kept]


def _matches_bind(samples: list[dict], bone: dict, tolerance: dict) -> bool:
    bind = bone["bind"]
    for sample in samples:
        if max(abs(x - y) for x, y in zip(sample["translation"], bind["translation"])) > \
                tolerance["translation"]:
            return False
        if max(abs(x - y) for x, y in zip(sample["scale"], bind["scale"])) > tolerance["scale"]:
            return False
        if _angle_between(sample["rotation"], bind["rotation"]) > tolerance["rotationDegrees"]:
            return False
    return True


def extract_clip(document: dict, buffers: list[bytes], animation: dict, bones: list[dict],
                 tolerance: dict, loop_override: dict[str, bool]) -> dict:
    name = animation.get("name", "")
    samplers = animation.get("samplers", [])

    # channel -> Track, per NODE. Only the joints matter; a channel targeting a mesh node is not an
    # error, it is simply not skeleton animation.
    tracks: dict[int, dict[str, measure_stride.Track]] = {}
    for channel in animation.get("channels", []):
        target = channel.get("target", {})
        node = target.get("node")
        path = target.get("path")
        if node is None or path not in ("translation", "rotation", "scale"):
            continue
        sampler = samplers[channel["sampler"]]
        times = [value[0] for value in gltf_io.read_accessor(document, buffers, sampler["input"])]
        values = gltf_io.read_accessor(document, buffers, sampler["output"])
        tracks.setdefault(node, {})[path] = measure_stride.Track(
            times, values, sampler.get("interpolation", "LINEAR"), path)

    every_time = sorted({time for node in tracks.values() for track in node.values()
                         for time in track.times})
    if not every_time:
        raise AnimError(f"clip '{name}' animates no joint")
    start, end = every_time[0], every_time[-1]
    duration = end - start
    if not (duration > 0) or not math.isfinite(duration):
        raise AnimError(f"clip '{name}' has duration {duration}; a clip that cannot be sampled is "
                        f"a content error")

    per_bone: list[list[dict]] = []
    dropped = 0
    sampled = 0
    for bone in bones:
        node_tracks = tracks.get(bone["node"], {})
        if not node_tracks:
            # No channel at all: an empty range, which the format makes legal and which means the
            # bone holds its bind pose. On a real rig most bones in most clips are this.
            per_bone.append([])
            continue

        # The UNION of this bone's own channels' key times, not a resampling grid. The source's
        # times are where the author put information; a uniform grid would add keys where there is
        # nothing to say and lose the exact instant of a contact.
        times = sorted({time for track in node_tracks.values() for time in track.times})
        samples = []
        for time in times:
            bind = bone["bind"]
            samples.append({
                "time": time - start,
                "translation": tuple(node_tracks["translation"].at(time))
                if "translation" in node_tracks else bind["translation"],
                "rotation": tuple(node_tracks["rotation"].at(time))
                if "rotation" in node_tracks else bind["rotation"],
                "scale": tuple(node_tracks["scale"].at(time))
                if "scale" in node_tracks else bind["scale"],
            })
        sampled += len(samples)
        if _matches_bind(samples, bone, tolerance):
            # Animated, but to exactly its bind pose. Writing two keys that say "do not move" costs
            # 80 bytes per bone per clip for nothing.
            dropped += len(samples)
            per_bone.append([])
            continue
        kept = decimate(samples, tolerance)
        dropped += len(samples) - len(kept)
        per_bone.append(kept)

    keys: list[dict] = []
    first_key = [0]
    for track in per_bone:
        keys.extend(track)
        first_key.append(len(keys))
    if len(keys) > MAX_KEYS:
        raise AnimError(f"clip '{name}' has {len(keys)} keys; the format caps a clip at {MAX_KEYS}")

    # Looping is INFERRED and reported, never assumed. A clip whose first and last pose agree is
    # one that can be played back to back without a jump, which is what the flag means.
    first_pose = [track[0] for track in per_bone if track]
    last_pose = [track[-1] for track in per_bone if track]
    inferred = bool(first_pose) and all(
        max(abs(x - y) for x, y in zip(a["translation"], b["translation"])) <= LOOP_TOLERANCE
        and _angle_between(a["rotation"], b["rotation"]) <= 1.0
        for a, b in zip(first_pose, last_pose))
    loops = loop_override.get(name, inferred)

    return {
        "name": name,
        "duration": duration,
        "loops": loops,
        "loopWasInferred": name not in loop_override,
        "keys": keys,
        "boneFirstKey": first_key,
        "sampledKeys": sampled,
        "droppedKeys": dropped,
        "strideLength": 0.0,
        "footPlants": [],
    }


# --------------------------------------------------------------------------------------- write ----

def encode(bones: list[dict], root: int, clips: list[dict]) -> bytes:
    blob = Blob()
    blob.data += MAGIC
    blob.u32(VERSION)
    blob.u32(0)  # flags; reserved, and a reader rejects any bit set
    blob.u32(len(bones))
    blob.u32(len(clips))

    for bone in bones:
        blob.name(bone["name"])
        blob.i32(bone["parent"])
        blob.matrix(bone["bindPose"])
        blob.matrix(bone["inverseBindPose"])
    blob.i32(root)

    for clip in clips:
        blob.name(clip["name"])
        blob.f32(clip["duration"])
        blob.u32(1 if clip["loops"] else 0)
        blob.f32(clip["strideLength"])
        blob.u32(len(clip["footPlants"]))
        for time in clip["footPlants"]:
            blob.f32(time)
        blob.u32(len(clip["keys"]))
        for first in clip["boneFirstKey"]:
            blob.u32(first)
        for key in clip["keys"]:
            blob.f32(key["time"])
            for value in key["translation"]:
                blob.f32(value)
            for value in key["rotation"]:
                blob.f32(value)
            for value in key["scale"]:
                blob.f32(value)
    return bytes(blob.data)


def check(bones: list[dict], root: int, clips: list[dict]) -> list[str]:
    """Every condition `docs/anim-format.md` §4 makes a reader reject, checked before writing.

    A writer that can emit a file its own reader refuses is a writer that will, and the failure
    then surfaces at load time in the game rather than in the build that produced it.
    """
    problems: list[str] = []
    if not 1 <= len(bones) <= MAX_BONES:
        problems.append(f"boneCount is {len(bones)}, outside 1..{MAX_BONES}")
    if len(clips) > MAX_CLIPS:
        problems.append(f"clipCount is {len(clips)}, above {MAX_CLIPS}")
    if not -1 <= root < len(bones):
        problems.append(f"rootBone is {root}, outside -1..{len(bones) - 1}")
    for bone in bones:
        if not -1 <= bone["parent"] < bone["slot"]:
            problems.append(f"joint '{bone['name']}' has parent {bone['parent']} at slot "
                            f"{bone['slot']}: a forward reference or a cycle")

    names = set()
    for clip in clips:
        if clip["name"] in names:
            problems.append(f"clip '{clip['name']}' appears twice")
        names.add(clip["name"])
        if not clip["duration"] > 0:
            problems.append(f"clip '{clip['name']}' has duration {clip['duration']}")
        if clip["strideLength"] < 0:
            problems.append(f"clip '{clip['name']}' has a negative strideLength")
        if len(clip["footPlants"]) > MAX_FOOT_PLANTS:
            problems.append(f"clip '{clip['name']}' has {len(clip['footPlants'])} foot plants, "
                            f"above {MAX_FOOT_PLANTS}")
        for index, time in enumerate(clip["footPlants"]):
            if not 0.0 <= time <= clip["duration"]:
                problems.append(f"clip '{clip['name']}' foot plant {index} at {time} is outside "
                                f"[0, {clip['duration']}]")
            if index and time < clip["footPlants"][index - 1]:
                problems.append(f"clip '{clip['name']}' foot plants are not ascending")

        first = clip["boneFirstKey"]
        if len(first) != len(bones) + 1:
            problems.append(f"clip '{clip['name']}' boneFirstKey has {len(first)} entries for "
                            f"{len(bones)} bones")
        elif first[-1] != len(clip["keys"]):
            problems.append(f"clip '{clip['name']}' boneFirstKey ends at {first[-1]}, not at "
                            f"keyCount {len(clip['keys'])}")
        for index in range(1, len(first)):
            if first[index] < first[index - 1]:
                problems.append(f"clip '{clip['name']}' boneFirstKey is not non-decreasing")
                break

        for slot in range(len(first) - 1):
            track = clip["keys"][first[slot]:first[slot + 1]]
            for index, key in enumerate(track):
                if not 0.0 <= key["time"] <= clip["duration"] + 1e-6:
                    problems.append(f"clip '{clip['name']}' joint {slot} has a key at "
                                    f"{key['time']}, outside [0, {clip['duration']}]")
                    break
                if index and key["time"] < track[index - 1]["time"]:
                    problems.append(f"clip '{clip['name']}' joint {slot} has keys out of order; "
                                    f"the sampler binary-searches and would return the wrong pose")
                    break
    return problems


def extract(path: Path, tolerance: dict | None = None, loops: dict[str, bool] | None = None,
            measure: bool = True) -> tuple[bytes, dict]:
    tolerance = tolerance or {"translation": TOLERANCE_TRANSLATION,
                              "rotationDegrees": TOLERANCE_ROTATION_DEGREES,
                              "scale": TOLERANCE_SCALE}
    loops = loops or {}

    document, blob = gltf_io.read_model(path)
    buffers = gltf_io.buffer_bytes(document, blob, path.parent)
    bones, root = read_skeleton(document, buffers)

    animations = document.get("animations", [])
    if not animations:
        raise AnimError(f"{path.name}: has no animations")

    clips = [extract_clip(document, buffers, animation, bones, tolerance, loops)
             for animation in animations]

    warnings: list[str] = []
    # `HOUSE-00194` measures the stride; this tool CONSUMES that rather than measuring again, which
    # is what stops two implementations of the same thing drifting apart.
    if measure:
        try:
            measurement = measure_stride.measure(path)
            by_name = {clip["name"]: clip for clip in measurement["clips"]}
            warnings += measurement["warnings"]
            for clip in clips:
                found = by_name.get(clip["name"])
                if found is None:
                    warnings.append(f"clip '{clip['name']}' was not measured for stride")
                    continue
                clip["strideLength"] = float(found["strideLength"])
                plants = sorted(float(t) for t in found["footPlants"]
                                if 0.0 <= float(t) <= clip["duration"])
                if len(plants) > MAX_FOOT_PLANTS:
                    warnings.append(f"clip '{clip['name']}' has {len(plants)} foot plants; the "
                                    f"format stores at most {MAX_FOOT_PLANTS}")
                    plants = plants[:MAX_FOOT_PLANTS]
                clip["footPlants"] = plants
                warnings += [f"clip '{clip['name']}': {w}" for w in found["warnings"]]
        except AnimError as error:
            # A stride that cannot be measured is a stride of 0, which §47.4 already defines as
            # "not locomotion" -- so the sidecar is still correct, and the reason is reported.
            warnings.append(f"stride measurement failed, so every strideLength is 0: {error}")

    problems = check(bones, root, clips)
    if problems:
        raise AnimError(f"{path.name}: the sidecar this would write is one the reader rejects:\n  "
                        + "\n  ".join(problems))

    data = encode(bones, root, clips)
    report = {
        "tool": "tools/assets/anim_extract.py",
        "formatVersion": VERSION,
        "source": path.name,
        "bytes": len(data),
        "boneCount": len(bones),
        "rootBone": bones[root]["name"] if root >= 0 else None,
        "bones": [bone["name"] for bone in bones],
        "clips": [{"name": clip["name"],
                   "duration": round(clip["duration"], 6),
                   "loops": clip["loops"],
                   "loopWasInferred": clip["loopWasInferred"],
                   "strideLength": round(clip["strideLength"], 6),
                   "footPlants": [round(t, 6) for t in clip["footPlants"]],
                   "keys": len(clip["keys"]),
                   "sampledKeys": clip["sampledKeys"],
                   "droppedKeys": clip["droppedKeys"],
                   "animatedBones": sum(1 for slot in range(len(bones))
                                        if clip["boneFirstKey"][slot + 1] >
                                        clip["boneFirstKey"][slot]),
                   "keysPerJoint": {bones[slot]["name"]:
                                    clip["boneFirstKey"][slot + 1] - clip["boneFirstKey"][slot]
                                    for slot in range(len(bones))}}
                  for clip in clips],
        "warnings": warnings,
    }
    return data, report


# ------------------------------------------------------------------------------------ fixture ----

def _skinned_fixture(path: Path) -> None:
    """A small skinned character with two clips, written by hand so every answer is known.

    Five joints in `skin.joints` order -- Hips, LeftUpLeg, LeftFoot, RightUpLeg, RightFoot -- plus a
    two-triangle skinned mesh so the file is a real skinned `.glb` rather than a rig on its own.
    The clips are shaped by what has to be provable:

    * `walk_fwd` translates the hips and cycles the feet, so a stride and two foot plants exist.
    * `RightUpLeg` is animated in `walk_fwd` to **exactly its bind pose**, so the "no keys at all"
      path is exercised by a bone that HAS channels rather than only by one that has none.
    * `LeftUpLeg` carries a long straight ramp sampled at 60 keys, so decimation has something to
      remove and the count it removes is known: a straight line needs two.
    * the hips' translation and rotation channels are sampled at DIFFERENT rates, so the union of a
      bone's own key times is exercised rather than a single shared grid, and the rotation ends at
      a quaternion whose four components are ALL DIFFERENT -- so a writer that emitted `w` first,
      or a reader that read it that way, is visible. An identity quaternion cannot show that, and
      the first version of this fixture used one.
    * `idle` does not travel, so its stride must be 0.
    """
    names = ["Hips", "LeftUpLeg", "LeftFoot", "RightUpLeg", "RightFoot"]
    children = {0: [1, 3], 1: [2], 3: [4]}
    rest = [(0.0, 0.95, 0.0), (0.1, -0.45, 0.0), (0.0, -0.45, 0.1),
            (-0.1, -0.45, 0.0), (0.0, -0.45, -0.1)]

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

    # A skinned quad. Two triangles, four vertices, all bound to joint 0 -- the mesh is here so the
    # file is a real skinned asset, not because its weights matter to a sidecar.
    quad = [(-0.2, 0.0, 0.0), (0.2, 0.0, 0.0), (0.2, 1.4, 0.0), (-0.2, 1.4, 0.0)]
    a_pos = add(b"".join(struct.pack("<3f", *p) for p in quad),
                {"componentType": 5126, "count": 4, "type": "VEC3",
                 "min": [-0.2, 0.0, 0.0], "max": [0.2, 1.4, 0.0]})
    a_joints = add(b"".join(struct.pack("<4H", 0, 0, 0, 0) for _ in quad),
                   {"componentType": 5123, "count": 4, "type": "VEC4"})
    a_weights = add(b"".join(struct.pack("<4f", 1.0, 0.0, 0.0, 0.0) for _ in quad),
                    {"componentType": 5126, "count": 4, "type": "VEC4"})
    a_index = add(struct.pack("<6H", 0, 1, 2, 0, 2, 3),
                  {"componentType": 5123, "count": 6, "type": "SCALAR", "min": [0], "max": [3]})

    # Inverse bind matrices: the inverse of each joint's WORLD bind transform. The rig has no
    # rotation at rest, so each is a translation by the negated world position -- which lands at
    # elements 12..14 of the column-major array, and must land at M41..M43 after the copy.
    world = []
    for slot, translation in enumerate(rest):
        parent = next((p for p, kids in children.items() if slot in kids), None)
        base = world[parent] if parent is not None else (0.0, 0.0, 0.0)
        world.append(tuple(base[i] + translation[i] for i in range(3)))
    inverse = bytearray()
    for position in world:
        matrix = [1.0, 0.0, 0.0, 0.0,
                  0.0, 1.0, 0.0, 0.0,
                  0.0, 0.0, 1.0, 0.0,
                  -position[0], -position[1], -position[2], 1.0]
        inverse += b"".join(struct.pack("<f", v) for v in matrix)
    a_ibm = add(bytes(inverse), {"componentType": 5126, "count": len(rest), "type": "MAT4"})

    def sampler_pair(times, values, width):
        fmt = f"<{width}f"
        t = add(b"".join(struct.pack("<f", v) for v in times),
                {"componentType": 5126, "count": len(times), "type": "SCALAR",
                 "min": [times[0]], "max": [times[-1]]})
        v = add(b"".join(struct.pack(fmt, *value) for value in values),
                {"componentType": 5126, "count": len(values),
                 "type": "VEC3" if width == 3 else "VEC4"})
        return t, v

    animations = []

    # --- walk_fwd -------------------------------------------------------------------------------
    channels, samplers = [], []

    def channel(node, path, times, values, width):
        t, v = sampler_pair(times, values, width)
        samplers.append({"input": t, "output": v, "interpolation": "LINEAR"})
        channels.append({"sampler": len(samplers) - 1, "target": {"node": node, "path": path}})

    duration = 1.0
    stride = 1.4
    # Hips: translation at 20 Hz, rotation at 5 Hz -- different rates on purpose.
    t_times = [duration * i / 20 for i in range(21)]
    channel(0, "translation", t_times,
            [(stride * t, 0.95, 0.0) for t in t_times], 3)
    r_times = [duration * i / 5 for i in range(6)]
    channel(0, "rotation", r_times,
            [measure_stride._slerp((0.0, 0.0, 0.0, 1.0), FIXTURE_ROTATION, t / duration)
             for t in r_times], 4)

    # The feet, in the walk of `measure_stride`'s own fixture, expressed as LOCAL translations.
    walk = measure_stride._walk_samples(121, duration, stride, in_place=False)
    foot_times = [duration * i / 120 for i in range(121)]
    for node, series, parent_rest in ((2, walk["LeftFoot"], rest[1]), (4, walk["RightFoot"],
                                                                       rest[3])):
        local = []
        for index, position in enumerate(series):
            hips = walk["Hips"][index]
            # world(foot) = hips + rest[upleg] + local(foot)
            local.append(tuple(position[i] - hips[i] - parent_rest[i] for i in range(3)))
        channel(node, "translation", foot_times, local, 3)

    # LeftUpLeg: 60 keys along a straight ramp. A straight line decimates to two.
    ramp_times = [duration * i / 59 for i in range(60)]
    channel(1, "translation", ramp_times,
            [(rest[1][0], rest[1][1], rest[1][2] + 0.3 * (t / duration)) for t in ramp_times], 3)

    # RightUpLeg: animated to EXACTLY its bind pose. Must end with no keys at all.
    channel(3, "translation", [0.0, 0.5, 1.0], [rest[3]] * 3, 3)

    animations.append({"name": "walk_fwd", "channels": channels, "samplers": samplers})

    # --- idle: a small sway, no travel -----------------------------------------------------------
    channels, samplers = [], []
    idle_times = [4.0 * i / 20 for i in range(21)]
    channel(0, "translation", idle_times,
            [(0.0, 0.95 + 0.01 * math.sin(2 * math.pi * t / 4.0), 0.0) for t in idle_times], 3)
    animations.append({"name": "idle", "channels": channels, "samplers": samplers})

    nodes = []
    for index, name in enumerate(names):
        node: dict = {"name": name, "translation": list(rest[index])}
        if index in children:
            node["children"] = children[index]
        nodes.append(node)
    nodes.append({"name": "Body", "mesh": 0, "skin": 0})

    document = {
        "asset": {"version": "2.0", "generator": "cna-house anim_extract.py --make-fixture"},
        "scene": 0, "scenes": [{"nodes": [0, len(names)]}],
        "nodes": nodes,
        "skins": [{"name": "Body", "joints": list(range(len(names))),
                   "inverseBindMatrices": a_ibm}],
        "meshes": [{"name": "Body", "primitives": [
            {"attributes": {"POSITION": a_pos, "JOINTS_0": a_joints, "WEIGHTS_0": a_weights},
             "indices": a_index, "mode": 4}]}],
        "animations": animations,
        "accessors": accessors, "bufferViews": views, "buffers": [{"byteLength": len(blob)}],
    }
    gltf_io.write_glb(path, document, bytes(blob))


def make_fixture(directory: Path) -> list[Path]:
    directory.mkdir(parents=True, exist_ok=True)
    written = []

    good = directory / "character.glb"
    _skinned_fixture(good)
    written.append(good)

    # Two skins: refused, because §47.0's one-skin rule is offline and absolute.
    document, blob = gltf_io.read_model(good)
    two = json.loads(json.dumps(document))
    two["skins"].append(json.loads(json.dumps(two["skins"][0])))
    two["skins"][1]["name"] = "Second"
    multi = directory / "two_skins.glb"
    gltf_io.write_glb(multi, two, blob)
    written.append(multi)

    # A child joint listed before its parent: a forward reference the format cannot represent.
    reordered = json.loads(json.dumps(document))
    reordered["skins"][0]["joints"] = [1, 0, 2, 3, 4]
    backwards = directory / "child_first.glb"
    gltf_io.write_glb(backwards, reordered, blob)
    written.append(backwards)

    # Two joints with the same name: the name list IS the binding, so this is ambiguous.
    duplicate = json.loads(json.dumps(document))
    duplicate["nodes"][3]["name"] = "LeftUpLeg"
    collide = directory / "duplicate_names.glb"
    gltf_io.write_glb(collide, duplicate, blob)
    written.append(collide)

    return written


# ----------------------------------------------------------------------------------- selftest ----

def _decode(data: bytes) -> dict:
    """Read a `.chanim` back. Deliberately a SEPARATE implementation from `encode`, written from
    `docs/anim-format.md` rather than by reversing the writer, so a claim made with it is a claim
    about the format and not about the writer agreeing with itself."""
    offset = 0

    def take(fmt: str):
        nonlocal offset
        size = struct.calcsize(fmt)
        values = struct.unpack_from(fmt, data, offset)
        offset += size
        return values

    def text() -> str:
        nonlocal offset
        (length,) = take("<H")
        value = data[offset:offset + length].decode("utf-8")
        offset += length
        return value

    assert data[:4] == MAGIC
    offset = 4
    version, flags, bone_count, clip_count = take("<IIII")
    bones = []
    for _ in range(bone_count):
        name = text()
        (parent,) = take("<i")
        bind = take("<16f")
        inverse = take("<16f")
        bones.append({"name": name, "parent": parent, "bindPose": bind,
                      "inverseBindPose": inverse})
    (root,) = take("<i")

    clips = []
    for _ in range(clip_count):
        name = text()
        duration, clip_flags, stride, plant_count = take("<fIfI")
        plants = list(take(f"<{plant_count}f")) if plant_count else []
        (key_count,) = take("<I")
        first = list(take(f"<{bone_count + 1}I"))
        keys = []
        for _ in range(key_count):
            values = take("<11f")
            keys.append({"time": values[0], "translation": values[1:4],
                         "rotation": values[4:8], "scale": values[8:11]})
        clips.append({"name": name, "duration": duration, "loops": bool(clip_flags & 1),
                      "strideLength": stride, "footPlants": plants,
                      "boneFirstKey": first, "keys": keys})
    assert offset == len(data), f"{len(data) - offset} trailing byte(s)"
    return {"version": version, "flags": flags, "bones": bones, "rootBone": root, "clips": clips}



def selftest() -> int:
    failures = 0

    def require(condition: bool, message: str) -> None:
        nonlocal failures
        if condition:
            print(f"  {message}")
        else:
            print(f"  SELFTEST FAILED: {message}", file=sys.stderr)
            failures += 1

    workspace = Path(tempfile.mkdtemp(prefix="anim_extract_selftest_"))
    try:
        source = workspace / "src"
        make_fixture(source)
        data, report = extract(source / "character.glb")

        # 1. The header is what `docs/anim-format.md` §3.1 says it is.
        require(data[:4] == MAGIC, f"the file starts with CHAN ({data[:4]!r})")
        version, flags, bone_count, clip_count = struct.unpack_from("<IIII", data, 4)
        require(version == VERSION and flags == 0,
                f"version {version}, flags {flags:#x}")
        require(bone_count == 5 and clip_count == 2,
                f"{bone_count} bone(s) and {clip_count} clip(s)")

        # 2. The joint list is `skin.joints` order, unchanged. This IS the binding (`HOUSE-00074`).
        require(report["bones"] == ["Hips", "LeftUpLeg", "LeftFoot", "RightUpLeg", "RightFoot"],
                f"the joints are in skin.joints order: {report['bones']}")
        require(report["rootBone"] == "Hips", f"the root joint is {report['rootBone']}")

        # 3. The matrix claim, checked rather than assumed: a bind translation must land at
        #    M41..M43, which is floats 12..14 of the sixteen.
        offset = 4 + 16  # header
        offset += 2 + len("Hips") + 4  # the first joint's name and parent
        bind = struct.unpack_from("<16f", data, offset)
        require(abs(bind[12] - 0.0) < 1e-6 and abs(bind[13] - 0.95) < 1e-6,
                f"a bind pose's translation lands at M41..M43 ({bind[12:15]})")
        inverse = struct.unpack_from("<16f", data, offset + 64)
        require(abs(inverse[13] + 0.95) < 1e-6,
                f"an inverse bind pose is copied through with its translation at M41..M43 "
                f"({inverse[12:15]})")

        walk = next(clip for clip in report["clips"] if clip["name"] == "walk_fwd")
        idle = next(clip for clip in report["clips"] if clip["name"] == "idle")

        # 4. Decimation removed what it should and kept what it should.
        require(walk["droppedKeys"] > 0 and walk["keys"] < walk["sampledKeys"],
                f"walk_fwd decimates {walk['sampledKeys']} sampled keys to {walk['keys']} "
                f"({walk['droppedKeys']} dropped)")

        # 5. A bone animated to EXACTLY its bind pose is written with no keys at all -- the case a
        #    bone with no channels cannot prove, because it has nothing to drop. Named rather than
        #    counted: a count of 4 out of 5 would also pass if the WRONG joint had been emptied.
        require(walk["keysPerJoint"]["RightUpLeg"] == 0,
                f"RightUpLeg is animated to exactly its bind pose and is written with no keys "
                f"({walk['keysPerJoint']['RightUpLeg']})")
        require(all(walk["keysPerJoint"][name] > 0
                    for name in ("Hips", "LeftUpLeg", "LeftFoot", "RightFoot")),
                f"...and the four joints that do move all carry keys "
                f"({walk['keysPerJoint']})")

        # 5b. The quaternion component ORDER, at both ends. `w` last, matching XNA's constructor
        #     and glTF (§2 of the format). The fixture's final hips rotation has four distinct
        #     components precisely so this claim can be made at all.
        require(walk["keysPerJoint"]["Hips"] >= 2,
                f"Hips carries {walk['keysPerJoint']['Hips']} keys")
        library = _decode(data)
        walk_clip = next(c for c in library["clips"] if c["name"] == "walk_fwd")
        last_hips = walk_clip["keys"][walk_clip["boneFirstKey"][1] - 1]
        require(all(abs(a - b) < 1e-4 for a, b in zip(last_hips["rotation"], FIXTURE_ROTATION)),
                f"the hips' final rotation round-trips in x y z w order "
                f"({tuple(round(v, 4) for v in last_hips['rotation'])} against {FIXTURE_ROTATION})")
        require(len(set(round(v, 3) for v in FIXTURE_ROTATION)) == 4,
                "...and its four components are all different, so a w-first write is visible")

        # 6. Stride and foot plants come from `measure_stride`, not from a second implementation.
        require(abs(walk["strideLength"] - 1.4) < 1.4 * 0.05,
                f"walk_fwd's stride is {walk['strideLength']:.4f} m, measured by measure_stride")
        require(len(walk["footPlants"]) == 2,
                f"walk_fwd carries {len(walk['footPlants'])} foot plant(s)")
        require(all(0.0 <= t <= walk["duration"] for t in walk["footPlants"])
                and walk["footPlants"] == sorted(walk["footPlants"]),
                "the foot plants are ascending and inside [0, duration], as §4 requires")

        # 7. A clip that does not travel gets stride 0 -- §47.4's definition of "not locomotion".
        require(idle["strideLength"] == 0.0,
                f"idle's stride is {idle['strideLength']}, not a number invented for it")
        require(abs(idle["duration"] - 4.0) < 1e-6, f"idle is {idle['duration']} s long")

        # 8. Determinism.
        again, _ = extract(source / "character.glb")
        require(data == again, "two extractions of the same file are byte-identical")

        # 9. The partition is valid: non-decreasing, ends at keyCount, and every key inside it.
        require(not check([{"name": n, "parent": -1 if i == 0 else 0, "slot": i}
                           for i, n in enumerate(report["bones"])], 0, []),
                "an empty clip list is valid")

        # 10. The four refusals, each with a message that names the fix.
        for name, fragment in (("two_skins.glb", "one"),
                               ("child_first.glb", "parent before its child"),
                               ("duplicate_names.glb", "duplicate joint name")):
            try:
                extract(source / name)
                require(False, f"{name} is refused")
            except AnimError as error:
                require(fragment in str(error), f"{name} is refused: {str(error).splitlines()[0]}")

        # 11. A file with no animations is refused rather than producing an empty sidecar.
        document, blob = gltf_io.read_model(source / "character.glb")
        bare = json.loads(json.dumps(document))
        bare["animations"] = []
        gltf_io.write_glb(workspace / "no_anim.glb", bare, blob)
        try:
            extract(workspace / "no_anim.glb")
            require(False, "a file with no animations is refused")
        except AnimError as error:
            require("no animations" in str(error), f"a file with no animations is refused: {error}")

        # 12. `check()` must FAIL on a broken sidecar, or it is decoration. Every one of these is a
        #     condition `docs/anim-format.md` §4 makes a reader reject.
        bones = [{"name": "A", "parent": -1, "slot": 0}, {"name": "B", "parent": 0, "slot": 1}]
        good_clip = {"name": "c", "duration": 1.0, "loops": False, "strideLength": 0.0,
                     "footPlants": [], "keys": [], "boneFirstKey": [0, 0, 0]}
        require(not check(bones, 0, [dict(good_clip)]), "a well-formed clip passes check()")

        broken = dict(good_clip, duration=0.0)
        require(any("duration" in p for p in check(bones, 0, [broken])),
                "check() catches a zero duration")
        broken = dict(good_clip, footPlants=[2.0])
        require(any("outside" in p for p in check(bones, 0, [broken])),
                "check() catches a foot plant past the end of the clip")
        broken = dict(good_clip, footPlants=[0.5, 0.1])
        require(any("ascending" in p for p in check(bones, 0, [broken])),
                "check() catches foot plants out of order")
        broken = dict(good_clip, boneFirstKey=[0, 0])
        require(any("boneFirstKey" in p for p in check(bones, 0, [broken])),
                "check() catches a boneFirstKey of the wrong length")
        broken = dict(good_clip, boneFirstKey=[0, 1, 0], keys=[])
        require(any("non-decreasing" in p or "ends at" in p for p in check(bones, 0, [broken])),
                "check() catches a boneFirstKey that goes backwards")
        require(any("rootBone" in p for p in check(bones, 7, [dict(good_clip)])),
                "check() catches a rootBone outside the skeleton")
        require(any("forward reference" in p
                    for p in check([{"name": "A", "parent": 1, "slot": 0},
                                    {"name": "B", "parent": -1, "slot": 1}], 1,
                                   [dict(good_clip, boneFirstKey=[0, 0, 0])])),
                "check() catches a parent that follows its child")

        # 13. The writer refuses to emit a non-finite float rather than writing a file the reader
        #     will reject at load, in a message that names the clip but not the joint.
        blob_writer = Blob()
        try:
            blob_writer.f32(float("nan"))
            require(False, "the writer refuses a NaN")
        except AnimError as error:
            require("non-finite" in str(error), f"the writer refuses a NaN: {error}")

        # 14. Decimation is correct, not merely lossy: a straight ramp becomes two keys, and a
        #     tolerance of zero keeps everything.
        samples = [{"time": i / 10, "translation": (0.0, 0.0, i / 10),
                    "rotation": (0.0, 0.0, 0.0, 1.0), "scale": (1.0, 1.0, 1.0)}
                   for i in range(11)]
        tolerance = {"translation": 1e-4, "rotationDegrees": 0.05, "scale": 1e-4}
        require(len(decimate(samples, tolerance)) == 2,
                f"a straight ramp of 11 keys decimates to "
                f"{len(decimate(samples, tolerance))}")
        samples[5]["translation"] = (0.0, 0.0, 0.9)
        require(len(decimate(samples, tolerance)) > 2,
                "...but a ramp with a spike in it keeps the spike")
        # The claim that actually matters, and the one a key count cannot make: what survives
        # decimation reproduces every ORIGINAL sample to within the tolerance it was given. A key
        # count says how much was thrown away; this says that nothing needed was.
        def reconstruct(kept: list[dict], time: float) -> dict:
            if time <= kept[0]["time"]:
                return kept[0]
            if time >= kept[-1]["time"]:
                return kept[-1]
            index = max(i for i in range(len(kept)) if kept[i]["time"] <= time)
            a, b = kept[index], kept[index + 1]
            span = b["time"] - a["time"]
            t = 0.0 if span <= 0 else (time - a["time"]) / span
            return {"translation": _lerp(a["translation"], b["translation"], t),
                    "rotation": measure_stride._slerp(a["rotation"], b["rotation"], t),
                    "scale": _lerp(a["scale"], b["scale"], t)}

        loose = {"translation": 0.02, "rotationDegrees": 1.0, "scale": 0.01}
        curved = [{"time": i / 30, "translation": (math.sin(i / 5.0), 0.0, i / 30),
                   "rotation": measure_stride._slerp((0.0, 0.0, 0.0, 1.0),
                                                      (0.0, 0.7071, 0.0, 0.7071), i / 30.0),
                   "scale": (1.0, 1.0, 1.0)}
                  for i in range(31)]
        kept = decimate(curved, loose)
        worst = max(max(abs(x - y) for x, y in zip(reconstruct(kept, s["time"])["translation"],
                                                   s["translation"]))
                    for s in curved)
        require(len(kept) < len(curved) and worst <= loose["translation"] + 1e-9,
                f"a curved track decimates {len(curved)} keys to {len(kept)} and still reproduces "
                f"every original sample to {worst:.5f} m, inside the {loose['translation']} m "
                f"tolerance it was given")
        worst_angle = max(_angle_between(reconstruct(kept, s["time"])["rotation"], s["rotation"])
                          for s in curved)
        require(worst_angle <= loose["rotationDegrees"] + 1e-6,
                f"...and to {worst_angle:.4f} degrees, inside the "
                f"{loose['rotationDegrees']} degree tolerance")
        require(_angle_between((0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.0, -1.0)) < 1e-3,
                "q and -q are the same rotation, not 180 degrees apart")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("anim_extract: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("model", nargs="?", type=Path)
    parser.add_argument("-o", "--out", type=Path, help="the .chanim to write")
    parser.add_argument("--loop", default="",
                        help="comma-separated clip names that loop; others are inferred")
    parser.add_argument("--no-loop", default="",
                        help="comma-separated clip names that do NOT loop")
    parser.add_argument("--tolerance-translation", type=float, default=TOLERANCE_TRANSLATION)
    parser.add_argument("--tolerance-rotation", type=float, default=TOLERANCE_ROTATION_DEGREES)
    parser.add_argument("--tolerance-scale", type=float, default=TOLERANCE_SCALE)
    parser.add_argument("--no-measure", action="store_true",
                        help="skip the stride measurement; every strideLength is 0")
    parser.add_argument("--json", type=Path, help="write the extraction report here")
    parser.add_argument("--make-fixture", type=Path, metavar="DIR")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if args.make_fixture is not None:
        for path in make_fixture(args.make_fixture):
            print(f"{path}  {path.stat().st_size} bytes")
        return 0
    if args.model is None:
        parser.print_help()
        return 2

    loops: dict[str, bool] = {}
    for name in filter(None, args.loop.split(",")):
        loops[name] = True
    for name in filter(None, args.no_loop.split(",")):
        loops[name] = False

    try:
        data, report = extract(args.model,
                               {"translation": args.tolerance_translation,
                                "rotationDegrees": args.tolerance_rotation,
                                "scale": args.tolerance_scale},
                               loops, not args.no_measure)
    except AnimError as error:
        print(f"anim_extract: {error}", file=sys.stderr)
        return 1

    if args.out is not None:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_bytes(data)
    if args.json is not None:
        args.json.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print(f"{report['source']}: {report['boneCount']} joint(s), {len(report['clips'])} clip(s), "
          f"{report['bytes']:,} bytes"
          + (f" -> {args.out}" if args.out else ""))
    print(f"  root joint {report['rootBone']}")
    for clip in report["clips"]:
        print(f"  {clip['name']:<16} {clip['duration']:6.3f} s  "
              f"{clip['keys']:>6} key(s) from {clip['sampledKeys']:>6}  "
              f"{clip['animatedBones']}/{report['boneCount']} joint(s)  "
              f"stride {clip['strideLength']:6.3f}  "
              f"{'loops' if clip['loops'] else 'once '}"
              f"{' (inferred)' if clip['loopWasInferred'] else ''}")
        if clip["footPlants"]:
            print(f"    plants  {', '.join(f'{t:.3f}' for t in clip['footPlants'])}")
    for warning in report["warnings"]:
        print(f"  warning: {warning}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
