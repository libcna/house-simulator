#!/usr/bin/env python3
"""build_skyexposure.py -- how much sky each room can hear.

`HOUSE-00213`. `cna-house.md` §64.6: each cell has a precomputed **sky exposure** -- "the solid
angle of open sky reachable from the cell's centre through its windows and doors, computed offline
by ray casting" -- and a **facade exposure** per orientation. The open-air rain and wind layers are
gained by `skyExposure(cell) + Σ aperture-weighted window contributions`, so this file is the part
that does not change when a window opens, and the runtime adds the part that does.

    tools/world/build_skyexposure.py --world assets-src/world --out content/world/skyexposure.bin
    tools/world/build_skyexposure.py --world assets-src/world --report
    tools/world/build_skyexposure.py --selftest

§64.6's own worked examples are the acceptance test: `L3_STORE_W` under the roof, `B1_CINEMA`
getting "essentially nothing", and the sunroom with its slider open getting "almost the outdoor
level". A basement with no opening must come out at zero, a room open to the sky at one, and a room
with one doorway somewhere strictly between.

## The openings are already holes

`HOUSE-00210` punched every portal out of its wall, so a ray leaving through a doorway meets no
geometry there and escapes on its own. Nothing here needs to know what a portal is. That is also
the right semantics for §64.6: the baked figure is the **geometric** opening, aperture-independent,
because the runtime multiplies in the aperture live as windows open.

## Why the ray count is what it is, and why that is a measurement

A Monte Carlo fraction over `n` uniform directions has a standard error of `sqrt(p(1-p)/n)`. A
single interior door subtends something like 2 % of the hemisphere, so at 512 rays the error on it
is around 0.6 % absolute -- comfortably under the 1 % an 8-bit audio gain can even represent, which
is the only consumer. `--report` prints the *measured* worst-case error rather than this argument,
and the selftest checks the estimator against three geometries whose exposure is known exactly.

Directions are a **Fibonacci hemisphere**: deterministic, uniform in solid angle (which is what
§64.6's "solid angle" asks for, and what a random or a latitude/longitude grid would not give --
the latter clusters wildly at the zenith), and the same every run.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import build_collision as bc  # noqa: E402
import layout_io  # noqa: E402
from layout_io import LayoutError  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

MAGIC = b"CSKY"
VERSION = 2

#: Rays per cell over the upper hemisphere. See the module docstring for why this number.
DEFAULT_RAYS = 512

#: The listener's ears above the cell floor. §64.6 samples "the cell's centre"; the centre of a
#: room in plan, at the height of the thing doing the listening.
EAR_HEIGHT = 1.60

#: How far apart the listening points are, in metres, and how many of them a cell may have.
#:
#: §64.6 says "the cell's centre" and `HOUSE-00213` took it literally. `HOUSE-00779` measured the
#: house with it and found the sentence does not survive a room that is not convex: `L3_ROOM` is a
#: T -- a body 7 m deep with two dormer bays reaching the front wall -- and NO ray from its
#: centroid reaches any of its three dormer windows, because the straight line to each of them
#: leaves the room through `L3_STORE_S` on the way. A room §13.6 calls "lit by three dormers"
#: measured 0.000, and one point in a room can only ever be one point in a room.
#:
#: So the figure is the MEAN over the cell's floor, and the centre is the first sample rather than
#: the only one. The cap is what makes that affordable: `EXT_WORLD` is 200 m across and a metre
#: grid over it is 150 000 points, so the step grows with the cell until the count fits.
SAMPLE_STEP = 1.0
MAX_SAMPLES = 16

#: The compass sectors a facade exposure is reported for. §14: −Z is north, +Z is the road.
#: `layout.cells.json`'s `daylight.orientation` uses the four cardinals; the four diagonals are
#: reported too because they cost nothing -- the rays are already cast -- and a corner room's
#: exposure is genuinely diagonal.
ORIENTATIONS = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]

#: The broad-phase grid. A ray marched through 2 m cells touches a few dozen of them and tests the
#: handful of shapes in each, instead of every shape in the house. Without it this tool is
#: 78 cells x 512 rays x ~4 300 shapes of pure Python and does not finish.
BROADPHASE_CELL = 2.0

EPS = 1e-9


# ==================================================================================== ray casting


def fibonacci_hemisphere(count: int):
    """`count` directions, uniform in solid angle over the upper hemisphere, deterministic.

    A latitude/longitude grid is the obvious alternative and is wrong for this: it puts as many
    samples in the last degree below the zenith as in the first degree above the horizon, so a
    skylight would be weighted like a wall of glass. The golden-angle spiral is uniform by
    construction and needs no random number generator to be reproducible.
    """
    golden = math.pi * (3.0 - math.sqrt(5.0))
    out = []
    for i in range(count):
        # y uniform in (0, 1] gives uniform solid angle over the hemisphere.
        y = (i + 0.5) / count
        radius = math.sqrt(max(0.0, 1.0 - y * y))
        theta = golden * i
        out.append((math.cos(theta) * radius, y, math.sin(theta) * radius))
    return out


def ray_obb(origin, direction, record) -> float | None:
    """Slab test against a yaw-only OBB. Returns the entry distance, or `None`.

    The yaw is undone on both the origin and the direction, which leaves an axis-aligned slab test
    in the box's own frame -- exact, and cheaper than transforming the box's eight corners.
    """
    (cx, cy, cz), (hx, hy, hz), yaw, _surface, _kind = record
    ox, oy, oz = origin[0] - cx, origin[1] - cy, origin[2] - cz
    dx, dy, dz = direction
    if abs(yaw) > 1e-9:
        c, s = math.cos(yaw), math.sin(yaw)
        ox, oz = ox * c - oz * s, ox * s + oz * c
        dx, dz = dx * c - dz * s, dx * s + dz * c
    near, far = -math.inf, math.inf
    for o, d, h in ((ox, dx, hx), (oy, dy, hy), (oz, dz, hz)):
        if abs(d) < EPS:
            if abs(o) > h:
                return None
            continue
        t0, t1 = (-h - o) / d, (h - o) / d
        if t0 > t1:
            t0, t1 = t1, t0
        near, far = max(near, t0), min(far, t1)
        if near > far:
            return None
    return near if far >= 0.0 else None


def ray_triangle(origin, direction, a, b, c) -> float | None:
    """Moller-Trumbore, two-sided: a collision mesh's winding is not a promise anything keeps."""
    e1 = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    e2 = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
    p = (direction[1] * e2[2] - direction[2] * e2[1],
         direction[2] * e2[0] - direction[0] * e2[2],
         direction[0] * e2[1] - direction[1] * e2[0])
    det = sum(e1[i] * p[i] for i in range(3))
    if abs(det) < EPS:
        return None
    inv = 1.0 / det
    t = (origin[0] - a[0], origin[1] - a[1], origin[2] - a[2])
    u = sum(t[i] * p[i] for i in range(3)) * inv
    if u < 0.0 or u > 1.0:
        return None
    q = (t[1] * e1[2] - t[2] * e1[1], t[2] * e1[0] - t[0] * e1[2], t[0] * e1[1] - t[1] * e1[0])
    v = sum(direction[i] * q[i] for i in range(3)) * inv
    if v < 0.0 or u + v > 1.0:
        return None
    distance = sum(e2[i] * q[i] for i in range(3)) * inv
    return distance if distance > EPS else None


class Caster:
    """Every shape in the world, in a uniform grid, answering "does this ray escape?"."""

    def __init__(self, built: dict) -> None:
        shapes: bc.Shapes = built["shapes"]
        self.obbs = list(shapes.obbs)
        self.meshes = list(shapes.meshes)
        entries = [(bc.obb_aabb(o), ("obb", i)) for i, o in enumerate(self.obbs)]
        entries += [(bc.mesh_aabb(m), ("mesh", i)) for i, m in enumerate(self.meshes)]
        if not entries:
            self.origin = (0.0, 0.0, 0.0)
            self.dims = (1, 1, 1)
            self.buckets = {}
            return
        self.origin = tuple(min(e[0][k] for e in entries) for k in range(3))
        high = tuple(max(e[0][k + 3] for e in entries) for k in range(3))
        self.dims = tuple(
            max(1, int(math.ceil((high[k] - self.origin[k]) / BROADPHASE_CELL)) + 1)
            for k in range(3))
        self.buckets: dict[tuple[int, int, int], list] = {}
        for aabb, ref in entries:
            lo = [self._index(aabb[k], k) for k in range(3)]
            hi = [self._index(aabb[k + 3], k) for k in range(3)]
            for i in range(lo[0], hi[0] + 1):
                for j in range(lo[1], hi[1] + 1):
                    for k in range(lo[2], hi[2] + 1):
                        self.buckets.setdefault((i, j, k), []).append(ref)

    def _index(self, value: float, axis: int) -> int:
        return max(0, min(self.dims[axis] - 1,
                          int((value - self.origin[axis]) / BROADPHASE_CELL)))

    def escapes(self, origin, direction) -> bool:
        """True if the ray leaves the world without meeting anything.

        The grid is marched cell by cell (a 3-D DDA) and the first cell that yields a hit ends the
        walk. Testing the whole bucket set at once would be simpler and would also throw away the
        early exit that makes the broad phase worth having.
        """
        if not self.buckets:
            return True
        current = [self._index(origin[k], k) for k in range(3)]
        step = [1 if direction[k] > 0 else -1 for k in range(3)]
        next_boundary = []
        delta = []
        for k in range(3):
            if abs(direction[k]) < EPS:
                next_boundary.append(math.inf)
                delta.append(math.inf)
                continue
            edge = self.origin[k] + (current[k] + (1 if direction[k] > 0 else 0)) * BROADPHASE_CELL
            next_boundary.append((edge - origin[k]) / direction[k])
            delta.append(BROADPHASE_CELL / abs(direction[k]))

        while True:
            for kind, index in self.buckets.get(tuple(current), ()):
                if kind == "obb":
                    if ray_obb(origin, direction, self.obbs[index]) is not None:
                        return False
                else:
                    mesh = self.meshes[index]
                    vertices = mesh["vertices"]
                    for a, b, c in mesh["triangles"]:
                        if ray_triangle(origin, direction, vertices[a], vertices[b],
                                        vertices[c]) is not None:
                            return False
            axis = min(range(3), key=lambda k: next_boundary[k])
            if next_boundary[axis] == math.inf:
                return True
            current[axis] += step[axis]
            if not (0 <= current[axis] < self.dims[axis]):
                return True
            next_boundary[axis] += delta[axis]

    def inside(self, point) -> bool:
        """Is this point inside any shape? A listener cannot stand inside the sofa."""
        bucket = tuple(self._index(point[k], k) for k in range(3))
        for kind, index in self.buckets.get(bucket, ()):
            if kind != "obb":
                continue
            (cx, cy, cz), (hx, hy, hz), yaw, _s, _k = self.obbs[index]
            dx, dy, dz = point[0] - cx, point[1] - cy, point[2] - cz
            if abs(yaw) > 1e-9:
                c, s = math.cos(yaw), math.sin(yaw)
                dx, dz = dx * c - dz * s, dx * s + dz * c
            if abs(dx) <= hx and abs(dy) <= hy and abs(dz) <= hz:
                return True
        return False


# ========================================================================================== build


def sector_of(direction) -> str:
    """Which compass sector a direction points into. §14: −Z is north, +Z is the road."""
    azimuth = math.degrees(math.atan2(direction[0], -direction[2])) % 360.0
    return ORIENTATIONS[int((azimuth + 22.5) % 360.0 // 45.0)]


def listener_points(cell, level, caster: Caster, stats):
    """Every point the cell is listened from: the centre first, then the rest of its floor.

    `listener_point` below is still the centre and still the point written into the file; this is
    the set the figure is averaged over (`HOUSE-00779`). The grid is laid over each box at
    `SAMPLE_STEP`, coarsened until the whole cell fits in `MAX_SAMPLES`, and a point inside
    geometry is dropped rather than measured -- a listener inside the sofa hears nothing, and one
    sample that says zero would pull the room's mean down by its share of it.
    """
    boxes = layout_io.cell_boxes(cell)
    floor = layout_io.cell_extent(cell, level)[0]
    ceiling = layout_io.cell_extent(cell, level)[1]
    height = min(EAR_HEIGHT, max(0.1, (ceiling - floor) * 0.5))
    area = sum((x1 - x0) * (z1 - z0) for x0, x1, z0, z1 in boxes)
    step = max(SAMPLE_STEP, math.sqrt(area / MAX_SAMPLES) if area > 0 else SAMPLE_STEP)

    points = [listener_point(cell, level, caster, stats)]
    for x0, x1, z0, z1 in boxes:
        steps_x = max(1, int((x1 - x0) / step))
        steps_z = max(1, int((z1 - z0) / step))
        for i in range(steps_x):
            for j in range(steps_z):
                point = (x0 + (i + 0.5) * (x1 - x0) / steps_x, floor + height,
                         z0 + (j + 0.5) * (z1 - z0) / steps_z)
                if caster.inside(point):
                    stats["blockedSamples"] += 1
                    continue
                if all(math.dist(point, other) > 1e-6 for other in points):
                    points.append(point)
    return points


def listener_point(cell, level, caster: Caster, stats):
    """Where §64.6's "the cell's centre" actually is, and a fallback when a prop is standing there.

    The area-weighted centroid of the cell's boxes, at ear height. An L-shaped room's centroid can
    fall outside the room and a sofa can be sitting on it, and a listener inside a solid box hears
    nothing at all -- so the centroid is tested, and when it fails the cell's boxes are scanned on a
    coarse grid for the nearest free point. The fallback is counted and reported, never silent.
    """
    boxes = layout_io.cell_boxes(cell)
    floor = layout_io.cell_extent(cell, level)[0]
    area = sum((x1 - x0) * (z1 - z0) for x0, x1, z0, z1 in boxes)
    cx = sum((x0 + x1) / 2 * (x1 - x0) * (z1 - z0) for x0, x1, z0, z1 in boxes) / area
    cz = sum((z0 + z1) / 2 * (x1 - x0) * (z1 - z0) for x0, x1, z0, z1 in boxes) / area
    ceiling = layout_io.cell_extent(cell, level)[1]
    height = min(EAR_HEIGHT, max(0.1, (ceiling - floor) * 0.5))
    candidate = (cx, floor + height, cz)
    if not caster.inside(candidate):
        return candidate

    best = None
    for x0, x1, z0, z1 in boxes:
        steps_x = max(1, int((x1 - x0) / 0.5))
        steps_z = max(1, int((z1 - z0) / 0.5))
        for i in range(steps_x):
            for j in range(steps_z):
                point = (x0 + (i + 0.5) * (x1 - x0) / steps_x, floor + height,
                         z0 + (j + 0.5) * (z1 - z0) / steps_z)
                if caster.inside(point):
                    continue
                distance = math.dist(point, candidate)
                if best is None or distance < best[0]:
                    best = (distance, point)
    stats["centroidBlocked"].append(cell["id"])
    if best is None:
        stats["noListenerPoint"].append(cell["id"])
        return candidate
    return best[1]


def build(world_dir: Path, manifest_path: Path | None = None, rays: int = DEFAULT_RAYS) -> dict:
    built = bc.build(world_dir, manifest_path)
    layout = layout_io.load_layout(world_dir, ["levels", "cells", "portals"])
    for optional in ("props", "stairs", "materials"):
        name, _ = layout_io.FILES[optional]
        if (world_dir / name).is_file():
            layout[optional] = layout_io.load_file(world_dir / name, optional)

    levels = layout_io.by_id(layout_io.rows(layout, "levels"), "level")
    caster = Caster(built)
    directions = fibonacci_hemisphere(rays)
    sectors = [sector_of(d) for d in directions]
    per_sector_total = {name: sectors.count(name) for name in ORIENTATIONS}

    stats = {"rays": rays, "centroidBlocked": [], "noListenerPoint": [], "casts": 0,
             "blockedSamples": 0, "samples": 0}
    results = []
    for cell in sorted(layout_io.rows(layout, "cells"), key=lambda c: c["id"]):
        level = levels.get(cell["level"])
        if level is None:
            raise LayoutError(f"cell {cell['id']!r} names level {cell['level']!r}, "
                              f"which does not exist")
        points = listener_points(cell, level, caster, stats)
        stats["samples"] += len(points)
        escaped = 0
        by_sector = {name: 0 for name in ORIENTATIONS}
        for origin in points:
            for direction, sector in zip(directions, sectors):
                stats["casts"] += 1
                if caster.escapes(origin, direction):
                    escaped += 1
                    by_sector[sector] += 1
        casts = rays * len(points)
        results.append({
            "id": cell["id"],
            # The CENTRE, which is where §64.6 says the figure is measured and is still the first
            # sample; the figure itself is the mean over `samples` of them (`HOUSE-00779`).
            "origin": points[0],
            "samples": len(points),
            "sky": escaped / casts,
            "facade": {name: (by_sector[name] / (per_sector_total[name] * len(points))
                              if per_sector_total[name] else 0.0)
                       for name in ORIENTATIONS},
        })

    # The Monte Carlo error actually incurred, rather than the argument for it in the docstring.
    # Averaging over a cell's samples is more casts, so the error falls with the square root of
    # all of them and not of the ray count alone.
    worst = 0.0
    for entry in results:
        p = entry["sky"]
        worst = max(worst, math.sqrt(p * (1 - p) / (rays * entry["samples"])))
    stats["worstStandardError"] = worst
    return {"cells": results, "stats": stats, "worldHash": built["worldHash"],
            "collision": built, "caster": caster}


# ========================================================================================= writer


def serialise(exposure: dict) -> bytes:
    out = bytearray()
    out += MAGIC
    out += struct.pack("<II", VERSION, 0)
    out += bc._string(exposure["worldHash"])
    out += struct.pack("<I", exposure["stats"]["rays"])
    out += struct.pack("<I", len(ORIENTATIONS))
    for name in ORIENTATIONS:
        out += bc._string(name)
    out += struct.pack("<I", len(exposure["cells"]))
    for entry in exposure["cells"]:
        out += bc._string(entry["id"])
        out += struct.pack("<3f", *entry["origin"])
        out += struct.pack("<I", entry["samples"])
        out += struct.pack("<f", entry["sky"])
        for name in ORIENTATIONS:
            out += struct.pack("<f", entry["facade"][name])
    return bytes(out)


def read_back(data: bytes) -> dict:
    view = memoryview(data)
    at = 0

    def take(n):
        nonlocal at
        chunk = view[at:at + n]
        if len(chunk) != n:
            raise LayoutError(f"skyexposure.bin truncated at byte {at}, wanted {n} more")
        at += n
        return chunk

    def unpack(fmt):
        return struct.unpack(fmt, take(struct.calcsize(fmt)))

    def text():
        (length,) = unpack("<H")
        return bytes(take(length)).decode("utf-8")

    if bytes(take(4)) != MAGIC:
        raise LayoutError("not a skyexposure.bin: bad magic")
    version, flags = unpack("<II")
    if version != VERSION:
        raise LayoutError(f"skyexposure.bin is version {version}; this reader knows {VERSION}")
    if flags:
        raise LayoutError(f"skyexposure.bin sets unknown flag bits {flags:#x}")
    world_hash = text()
    (rays,) = unpack("<I")
    (orientation_count,) = unpack("<I")
    orientations = [text() for _ in range(orientation_count)]
    (cell_count,) = unpack("<I")
    cells = []
    for _ in range(cell_count):
        identifier = text()
        origin = unpack("<3f")
        (samples,) = unpack("<I")
        (sky,) = unpack("<f")
        facade = {name: unpack("<f")[0] for name in orientations}
        cells.append({"id": identifier, "origin": origin, "samples": samples, "sky": sky,
                      "facade": facade})
    if at != len(data):
        raise LayoutError(f"{len(data) - at} bytes left over after the last cell")
    return {"worldHash": world_hash, "rays": rays, "orientations": orientations, "cells": cells}


def report(exposure: dict) -> str:
    stats = exposure["stats"]
    lines = [
        f"{len(exposure['cells'])} cells, {stats['samples']} listening points, "
        f"{stats['rays']} rays each ({stats['casts']} casts); worst standard error "
        f"{stats['worstStandardError']:.4f}; {stats['blockedSamples']} sample point(s) dropped "
        f"for standing inside something",
    ]
    for entry in sorted(exposure["cells"], key=lambda e: -e["sky"]):
        top = max(entry["facade"], key=lambda k: entry["facade"][k])
        lines.append(f"  {entry['id']:<22} sky {entry['sky']:.3f}   "
                     f"strongest facade {top} {entry['facade'][top]:.3f}")
    for cell_id in stats["centroidBlocked"]:
        lines.append(f"  note: {cell_id}'s centroid is inside geometry; a free point was used")
    for cell_id in stats["noListenerPoint"]:
        lines.append(f"  WARNING: {cell_id} has no free point at ear height at all")
    return "\n".join(lines)


# ======================================================================================= selftest


def selftest() -> int:
    import json
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("build_skyexposure: selftest")
    workspace = Path(tempfile.mkdtemp(prefix="build_skyexposure_selftest_"))
    try:
        # 1. The direction set. Uniform in SOLID ANGLE, which a latitude/longitude grid is not.
        directions = fibonacci_hemisphere(4096)
        require(all(abs(math.dist(d, (0, 0, 0)) - 1.0) < 1e-9 for d in directions),
                "every direction is a unit vector")
        require(all(d[1] > 0 for d in directions), "...and all point into the upper hemisphere")
        upper = sum(1 for d in directions if d[1] > math.sqrt(0.5))
        require(abs(upper / len(directions) - 0.293) < 0.01,
                f"the fraction above 45 degrees elevation is 1-cos(45)=0.293, as uniform solid "
                f"angle requires (got {upper / len(directions):.3f}) -- a lat/long grid gives "
                f"about 0.5 here and would weight a skylight like a wall of glass")

        # 2. Ray-OBB, against answers arithmetic gives.
        box = ((0.0, 0.0, 0.0), (1.0, 1.0, 1.0), 0.0, 0, bc.KIND_WALL)
        require(abs(ray_obb((-5.0, 0.0, 0.0), (1.0, 0.0, 0.0), box) - 4.0) < 1e-9,
                "a ray 5 m away hits the box's face at 4 m")
        require(ray_obb((-5.0, 0.0, 0.0), (-1.0, 0.0, 0.0), box) is None,
                "a ray pointing away misses")
        require(ray_obb((-5.0, 5.0, 0.0), (1.0, 0.0, 0.0), box) is None,
                "a ray passing above misses")
        turned = ((0.0, 0.0, 0.0), (3.0, 1.0, 0.2), math.pi / 2, 0, bc.KIND_WALL)
        require(ray_obb((0.0, 0.0, -5.0), (0.0, 0.0, 1.0), turned) is not None,
                "a yawed box is hit along the axis its rotation put it on")
        require(ray_obb((2.0, 0.0, -5.0), (0.0, 0.0, 1.0), turned) is None,
                "...and missed 2 m to the side, where the unrotated box would have been hit")

        # 3. Ray-triangle.
        tri = ((-1.0, 0.0, -1.0), (1.0, 0.0, -1.0), (0.0, 0.0, 1.0))
        require(abs(ray_triangle((0.0, -3.0, 0.0), (0.0, 1.0, 0.0), *tri) - 3.0) < 1e-9,
                "a ray hits a triangle from below at the right distance -- two-sided, because a "
                "collision mesh's winding is not a promise anything keeps")
        require(ray_triangle((5.0, -3.0, 0.0), (0.0, 1.0, 0.0), *tri) is None,
                "a ray outside the triangle misses")
        require(abs(ray_triangle((0.0, 3.0, 0.0), (0.0, -1.0, 0.0), *tri) - 3.0) < 1e-9,
                "...and from ABOVE too: one-sided would cull whichever face the winding happens "
                "to present, and a collision mesh's winding is not a promise anything keeps")

        # 3b. A triangle mesh is an obstacle. Nothing generated so far is one that blocks a
        #     listener, so it is built by hand -- otherwise the whole mesh branch of the caster
        #     is dead code that no claim reaches.
        mesh_only = bc.Shapes()
        mesh_only.mesh([(-50.0, 6.0, -50.0), (50.0, 6.0, -50.0), (50.0, 6.0, 50.0),
                        (-50.0, 6.0, 50.0)],
                       [(0, 1, 2), (0, 2, 3)], None, bc.KIND_CEILING)
        canopy = Caster({"shapes": mesh_only, "cells": [], "worldHash": ""})
        require(not canopy.escapes((0.0, 1.6, 0.0), (0.0, 1.0, 0.0)),
                "a ray straight up is stopped by a triangle-mesh canopy")
        blocked_fraction = sum(1 for d in fibonacci_hemisphere(DEFAULT_RAYS)
                               if not canopy.escapes((0.0, 1.6, 0.0), d)) / DEFAULT_RAYS
        require(blocked_fraction > 0.5,
                f"...and it blocks most of the hemisphere, not one ray "
                f"({blocked_fraction:.2f})")

        # 4. The estimator against geometry whose answer is known exactly. This is what makes the
        #    Monte Carlo trustworthy: open sky is 1, a sealed box is 0, and an infinite half-plane
        #    overhead is 0.5.
        def caster_of(*records):
            shapes = bc.Shapes()
            for centre, half, kind in records:
                shapes.obb(centre, half, 0.0, None, kind)
            return Caster({"shapes": shapes, "cells": [], "worldHash": ""})

        origin = (0.0, 1.6, 0.0)
        open_sky = Caster({"shapes": bc.Shapes(), "cells": [], "worldHash": ""})
        fraction = sum(1 for d in fibonacci_hemisphere(DEFAULT_RAYS)
                       if open_sky.escapes(origin, d)) / DEFAULT_RAYS
        require(fraction == 1.0, f"with nothing in the world, exposure is exactly 1 ({fraction})")

        # The lid has to be effectively unbounded, not merely large: a ray leaving the origin at
        # 5 degrees of elevation travels 50 m horizontally before it rises 4.4 m, so under a 50 m
        # lid it escapes past the edge. That is geometrically correct and it is not what this
        # claim is about, and the first version of the fixture measured 0.070 because of it.
        lid = caster_of(((0.0, 6.0, 0.0), (5000.0, 0.5, 5000.0), bc.KIND_CEILING))
        fraction = sum(1 for d in fibonacci_hemisphere(DEFAULT_RAYS)
                       if lid.escapes(origin, d)) / DEFAULT_RAYS
        require(fraction == 0.0, f"under an unbounded lid, exposure is exactly 0 ({fraction})")
        small = caster_of(((0.0, 6.0, 0.0), (50.0, 0.5, 50.0), bc.KIND_CEILING))
        leak = sum(1 for d in fibonacci_hemisphere(DEFAULT_RAYS)
                   if small.escapes(origin, d)) / DEFAULT_RAYS
        require(abs(leak - math.sin(math.atan2(4.4, 50.0))) < 0.02,
                f"under a 50 m lid the near-horizon rays escape past its edge, and the fraction "
                f"that do is sin(atan(4.4/50)) = {math.sin(math.atan2(4.4, 50.0)):.3f} "
                f"(measured {leak:.3f}) -- the estimator is measuring geometry, not leaking")

        # A half-plane covering everything with x > 0 blocks exactly half the hemisphere.
        half = caster_of(((2500.0, 6.0, 0.0), (2500.0, 0.5, 5000.0), bc.KIND_CEILING))
        fraction = sum(1 for d in fibonacci_hemisphere(DEFAULT_RAYS)
                       if half.escapes(origin, d)) / DEFAULT_RAYS
        error = abs(fraction - 0.5)
        require(error < 3 * math.sqrt(0.25 / DEFAULT_RAYS),
                f"half the sky covered measures {fraction:.4f}, within three standard errors of "
                f"0.5 ({error:.4f} against {3 * math.sqrt(0.25 / DEFAULT_RAYS):.4f})")

        # 5. The sector split. A half-plane over x > 0 removes the E facade and leaves W intact.
        counts = {name: [0, 0] for name in ORIENTATIONS}
        for d in fibonacci_hemisphere(DEFAULT_RAYS):
            sector = sector_of(d)
            counts[sector][1] += 1
            if half.escapes(origin, d):
                counts[sector][0] += 1
        require(counts["E"][0] == 0,
                f"nothing escapes east, where the lid is ({counts['E'][0]} did)")
        require(counts["W"][0] == counts["W"][1],
                f"everything escapes west, where it is not "
                f"({counts['W'][0]} of {counts['W'][1]})")
        require(sector_of((0.0, 0.1, -1.0)) == "N" and sector_of((1.0, 0.1, 0.0)) == "E"
                and sector_of((0.0, 0.1, 1.0)) == "S" and sector_of((-1.0, 0.1, 0.0)) == "W",
                "§14's axes: -Z is north, +Z is the road (south), +X is east")

        # 6. §64.6's own worked examples, on a fixture built to be them.
        world_dir = workspace / "world"
        world_dir.mkdir()
        bc._fixture_world(world_dir)
        cells = json.loads((world_dir / "layout.cells.json").read_text())
        cells["cells"].append({
            "id": "L0_TERRACE", "level": "L0", "kind": "exterior",
            "boxes": [{"x": [-6.0, -3.0], "z": [0.0, 3.0]}],
            "footstepSurface": "concrete", "wallMaterial": "MAT_PAINT",
            "ceilingMaterial": "MAT_CEIL", "yOverride": [0.0, 2.5]})
        (world_dir / "layout.cells.json").write_text(
            json.dumps(cells, indent=2) + "\n", encoding="utf-8")

        exposure = build(world_dir, rays=DEFAULT_RAYS)
        by_id = {e["id"]: e for e in exposure["cells"]}

        require(by_id["L0_GARAGE"]["sky"] == 0.0,
                f"the garage has no opening at all, so it is §64.6's `B1_CINEMA`: exactly zero "
                f"({by_id['L0_GARAGE']['sky']})")
        require(by_id["L0_LOUNGE"]["sky"] == 0.0,
                "a sealed room is zero however many walls it has")
        require(0.0 < by_id["L0_TERRACE"]["sky"] <= 1.0,
                f"the terrace is open to the sky above and gets a real figure "
                f"({by_id['L0_TERRACE']['sky']:.3f})")
        require(by_id["L0_TERRACE"]["sky"] > by_id["L0_LOUNGE"]["sky"],
                "...strictly more than the room next to it, which is the ordering §64.6's rain "
                "routing depends on")

        # 6b. A facade exposure is a fraction of ITS OWN sector, not of the whole hemisphere.
        #     Divided by the total ray count instead, every figure would be eight times too small
        #     and the terrace's rain would come in at an eighth of its level -- quiet enough to
        #     sound like a mix decision rather than a bug. The identity that catches it: the mean
        #     of the eight sectors is the sky exposure, because the sectors partition the rays.
        terrace_entry = by_id["L0_TERRACE"]
        mean_facade = sum(terrace_entry["facade"].values()) / len(ORIENTATIONS)
        require(abs(mean_facade - terrace_entry["sky"]) < 0.05,
                f"the eight facades average to the sky exposure "
                f"({mean_facade:.3f} against {terrace_entry['sky']:.3f}), because they partition "
                f"the same rays -- an eighth of it would mean each was divided by the wrong total")
        require(max(terrace_entry["facade"].values()) > 0.5,
                f"...and the open terrace's best facade is over half "
                f"({max(terrace_entry['facade'].values()):.3f}), not the 0.125 ceiling a "
                f"divide-by-total would impose")

        # 7. The listener point. A cell whose centroid is inside a prop must not report silence.
        #     Driven directly, because no fixture cell happens to have a sofa on its centroid and
        #     an untaken branch proves nothing.
        solid_centre = bc.Shapes()
        solid_centre.obb((2.0, 1.25, 1.5), (1.0, 1.25, 1.0), 0.0, None, bc.KIND_PROP)
        blocked_caster = Caster({"shapes": solid_centre, "cells": [], "worldHash": ""})
        blocked_stats = {"centroidBlocked": [], "noListenerPoint": []}
        point = listener_point(
            {"id": "L0_TEST", "level": "L0", "boxes": [{"x": [0.0, 4.0], "z": [0.0, 3.0]}]},
            {"id": "L0", "ffl": 0.0, "ceiling": 2.5}, blocked_caster, blocked_stats)
        require(blocked_stats["centroidBlocked"] == ["L0_TEST"],
                f"a centroid inside a prop is detected and named "
                f"({blocked_stats['centroidBlocked']})")
        require(not blocked_caster.inside(point),
                f"...and the listener is moved to a free point "
                f"({tuple(round(c, 2) for c in point)})")
        require(blocked_stats["noListenerPoint"] == [],
                "...without claiming the cell has nowhere to stand, which it has")
        require(exposure["stats"]["noListenerPoint"] == [],
                f"every cell found a free point at ear height "
                f"({exposure['stats']['noListenerPoint']})")
        blocked = Caster({"shapes": bc.Shapes(), "cells": [], "worldHash": ""})
        require(not blocked.inside((0.0, 1.6, 0.0)), "an empty world contains no point")
        solid = caster_of(((0.0, 1.0, 0.0), (2.0, 2.0, 2.0), bc.KIND_PROP))
        require(solid.inside((0.0, 1.6, 0.0)) and not solid.inside((9.0, 1.6, 0.0)),
                "a point inside a prop is inside; one outside it is not")

        # 8. The broad phase must not change any answer -- it exists for speed, and a broad phase
        #    that also changes results is not an optimisation. Checked against brute force.
        brute_caster = Caster(exposure["collision"])
        shapes: bc.Shapes = exposure["collision"]["shapes"]

        def brute(origin_point, direction):
            for record in shapes.obbs:
                if ray_obb(origin_point, direction, record) is not None:
                    return False
            for mesh in shapes.meshes:
                vertices = mesh["vertices"]
                for a, b, c in mesh["triangles"]:
                    if ray_triangle(origin_point, direction, vertices[a], vertices[b],
                                    vertices[c]) is not None:
                        return False
            return True

        disagreements = 0
        checked = 0
        for entry in exposure["cells"]:
            for direction in fibonacci_hemisphere(64):
                checked += 1
                if brute_caster.escapes(entry["origin"], direction) != brute(entry["origin"],
                                                                            direction):
                    disagreements += 1
        require(disagreements == 0,
                f"the grid broad phase agrees with brute force on all {checked} rays "
                f"({disagreements} disagreed)")

        # 9. Determinism and the round trip.
        data = serialise(exposure)
        back = read_back(data)
        require(len(back["cells"]) == len(exposure["cells"]),
                "the file reads back with the same cell count")
        require(all(abs(a["sky"] - b["sky"]) < 1e-6
                    for a, b in zip(back["cells"], exposure["cells"])),
                "every cell's sky exposure round trips")
        require(all(abs(a["facade"][name] - b["facade"][name]) < 1e-6
                    for a, b in zip(back["cells"], exposure["cells"])
                    for name in ORIENTATIONS),
                "every one of the eight facade exposures round trips")
        require(back["orientations"] == ORIENTATIONS and back["rays"] == DEFAULT_RAYS,
                "the orientation names and the ray count travel with the file, so a consumer "
                "knows what the numbers were measured with")
        require(serialise(build(world_dir, rays=DEFAULT_RAYS)) == data,
                "two builds of one layout produce byte-identical output -- the direction set is "
                "deterministic, with no random number generator anywhere")

        # 10. Truncation, version, flags and trailing bytes.
        for cut in (3, 12, len(data) - 1):
            try:
                read_back(data[:cut])
                caught = False
            except (LayoutError, struct.error):
                caught = True
            require(caught, f"a file truncated to {cut} bytes is refused")
        try:
            read_back(data + b"\0\0")
            caught = False
        except LayoutError as exc:
            caught = "left over" in str(exc)
        require(caught, "trailing bytes are refused")
        for offset, value, what in ((4, 5, "version"), (8, 0x2, "flag bit")):
            broken = bytearray(data)
            broken[offset:offset + 4] = struct.pack("<I", value)
            try:
                read_back(bytes(broken))
                caught = False
            except LayoutError:
                caught = True
            require(caught, f"an unknown {what} is refused rather than ignored")

        # 11. `HOUSE-00779`: the listening POINTS, which are the cell's floor and not just its
        #     centre. Driven on the fixture, where the answers can be counted.
        terrace_cell = next(row for row in layout_io.rows(
            layout_io.load_layout(world_dir, ["levels", "cells"]), "cells")
            if row["id"] == "L0_TERRACE")
        terrace_level = {"id": "L0", "ffl": 0.0, "ceiling": 2.5}
        sample_stats = {"centroidBlocked": [], "noListenerPoint": [], "blockedSamples": 0}
        spread = listener_points(terrace_cell, terrace_level, Caster(exposure["collision"]),
                                 sample_stats)
        require(len(spread) > 1,
                f"a cell is listened to from its whole floor, not from one point ({len(spread)})")
        require(spread[0] == listener_point(terrace_cell, terrace_level,
                                            Caster(exposure["collision"]),
                                            {"centroidBlocked": [], "noListenerPoint": []}),
                "and the CENTRE is the first of them, which is the point the file carries")
        wide = listener_points({"id": "L0_WIDE", "level": "L0",
                                "boxes": [{"x": [-100.0, 100.0], "z": [-100.0, 100.0]}]},
                               terrace_level, Caster(exposure["collision"]), sample_stats)
        require(len(wide) <= MAX_SAMPLES + 1,
                f"the step grows with the cell so a 200 m one is not 40 000 points "
                f"({len(wide)} for 40 000 m²)")
        sofa = bc.Shapes()
        sofa.obb((2.0, 1.25, 1.5), (1.0, 1.25, 1.0), 0.0, None, bc.KIND_PROP)
        sofa_caster = Caster({"shapes": sofa, "cells": [], "worldHash": ""})
        sofa_stats = {"centroidBlocked": [], "noListenerPoint": [], "blockedSamples": 0}
        around = listener_points({"id": "L0_SOFA", "level": "L0",
                                  "boxes": [{"x": [0.0, 4.0], "z": [0.0, 3.0]}]},
                                 terrace_level, sofa_caster, sofa_stats)
        require(sofa_stats["blockedSamples"] > 0 and all(not sofa_caster.inside(point)
                                                         for point in around),
                f"a sample standing inside the sofa is dropped rather than measured as silence "
                f"({sofa_stats['blockedSamples']} dropped of {len(around)} kept)")

        # 12. `HOUSE-00779`: the AUTHORED house and §64.6's own worked examples. The fixtures above
        #     prove the estimator; this proves the property, and every case is NAMED, because "8 %
        #     of the house hears the sky" is a number that stays true while a room loses its
        #     windows.
        authored = REPO / "assets-src" / "world"
        if (authored / "layout.cells.json").is_file():
            house = build(authored, REPO / "assets-src" / "assets.manifest.json")
            sky = {entry["id"]: entry["sky"] for entry in house["cells"]}
            rooms = layout_io.load_layout(authored, ["levels", "cells", "portals", "openings"])
            cells_by_id = layout_io.by_id(layout_io.rows(rooms, "cells"), "cell")

            # §64.6: "`B1_CINEMA` gets essentially nothing".
            require(sky["B1_CINEMA"] == 0.0,
                    f"§64.6's own example: `B1_CINEMA` gets nothing at all, exactly 0 "
                    f"({sky['B1_CINEMA']})")
            sealed = sorted(name for name, value in sky.items()
                            if value == 0.0 and cells_by_id[name]["level"] == "B1")
            require(len(sealed) >= 8,
                    f"and so does every other basement room with no opening to the outside "
                    f"({len(sealed)}: {sealed})")

            # The outdoors is the outdoors: every open exterior cell sees most of the sky.
            # ...with ONE exception, and it is the one §37.2 already names: the front porch is
            # outdoors and roofed, by the balcony over it rather than by anything of its own.
            shut_in = {name: round(sky[name], 3) for name, row in cells_by_id.items()
                       if row.get("kind") == "exterior" and row.get("visibilityHint") == "open"
                       and sky[name] < 0.5}
            require(set(shut_in) == {"L0_PORCH"},
                    f"every open exterior cell hears more than half the sky except the porch, "
                    f"which the front balcony roofs over ({shut_in})")
            require(sky["L0_PORCH"] > 0.2,
                    f"...and the porch still hears a third of it, because it is open on three "
                    f"sides ({sky['L0_PORCH']:.3f})")
            require(sky["EXT_ROAD"] > sky["L0_PORCH"] > sky["L0_LIVING"] > sky["B1_CINEMA"],
                    f"and the house falls away from it in order: road {sky['EXT_ROAD']:.3f} > "
                    f"porch {sky['L0_PORCH']:.3f} > living {sky['L0_LIVING']:.3f} > cinema "
                    f"{sky['B1_CINEMA']:.3f}")

            # A room with a window hears the sky through it. This is the claim `HOUSE-00784` and
            # `HOUSE-00490` were both found by: an invisible wall standing on the sunroom's roof
            # and a roof plane drawn across every dormer window each turned a whole room silent.
            by_portal = {row["id"]: row for row in layout_io.rows(rooms, "portals")}
            windowed = {}
            for opening in layout_io.rows(rooms, "openings"):
                if opening.get("kind") != "window":
                    continue
                portal = by_portal.get(opening.get("portal")) or {}
                for side in ("cellA", "cellB"):
                    room = portal.get(side)
                    if room in cells_by_id and cells_by_id[room].get("kind") != "exterior":
                        windowed.setdefault(room, []).append(str(opening.get("type")))
            # `HOUSE-00491`: `W_GABLE` is a louvre in a gable end, and this roof is a hip -- both
            # of them are 0.73 m inside solid roof and neither can see anything. Named here, with
            # the task that owns the decision, so the claim below stays a real claim.
            gabled = {name for name, types in windowed.items() if set(types) == {"W_GABLE"}}
            require(gabled == {"L3_STORE_W", "L3_STORE_E"},
                    f"the only rooms whose every window is a gable louvre are the two "
                    f"`HOUSE-00491` is about ({sorted(gabled)})")
            silent = sorted(name for name in windowed if name not in gabled and sky[name] <= 0.0)
            require(not silent,
                    f"every room with a window in a WALL hears the sky through it ({silent})")
            require(sky["L3_ROOM"] > 0.0 and sky["L3_STORE_N"] > 0.0,
                    f"...including the three rooms lit by dormers, which measured 0.000 until "
                    f"`HOUSE-00490` cut the roof out of the way of their windows "
                    f"({sky['L3_ROOM']:.3f}, {sky['L3_STORE_N']:.3f})")
            require(all(sky[name] == 0.0 for name in gabled),
                    f"and the two gable louvres still hear nothing, which is what "
                    f"`HOUSE-00491` records "
                    f"({[round(sky[name], 3) for name in sorted(gabled)]})")

            # §64.6: "the sunroom with its slider open gets almost the outdoor level". The slider
            # is not open in this file and cannot be: the baked figure is the GEOMETRIC opening
            # and the runtime multiplies the aperture in as a window moves. So the check is that
            # the sunroom is the loudest room in the house and still well under the terrace it
            # opens onto -- the gap between them is exactly what the runtime term has to close.
            indoor = {name: value for name, value in sky.items()
                      if cells_by_id[name].get("kind") not in ("exterior",)}
            loudest = max(indoor, key=lambda name: indoor[name])
            require(loudest == "L0_SUNROOM",
                    f"the sunroom is the loudest room in the house ({loudest} is)")
            require(sky["L0_SUNROOM"] < sky["EXT_TERRACE"] / 2.0,
                    f"...and still well under the terrace it opens onto "
                    f"({sky['L0_SUNROOM']:.3f} against {sky['EXT_TERRACE']:.3f}) -- §64.6's "
                    f"'almost the outdoor level' is the aperture term the runtime adds when the "
                    f"slider opens, not a number this file can bake")

            # The freezer looks like a bug and is not: a chest freezer's lid IS its portal, so its
            # interior has a hole in the top of it and hears what the pantry hears through the
            # window. The fridge's door is in a wall plane facing the kitchen and hears nothing.
            require(sky["CELL_FREEZER_INTERIOR"] > 0.0 and sky["CELL_FRIDGE_INTERIOR"] == 0.0,
                    f"the chest freezer's lid is a `y` portal and its interior is not sealed "
                    f"({sky['CELL_FREEZER_INTERIOR']:.3f}); the fridge's door faces the kitchen "
                    f"and its interior is ({sky['CELL_FRIDGE_INTERIOR']:.3f})")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("build_skyexposure: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--world", type=Path, default=REPO / "assets-src" / "world")
    parser.add_argument("--manifest", type=Path,
                        default=REPO / "assets-src" / "assets.manifest.json")
    parser.add_argument("--out", type=Path,
                        default=REPO / "content" / "world" / "skyexposure.bin")
    parser.add_argument("--rays", type=int, default=DEFAULT_RAYS)
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    try:
        exposure = build(args.world, args.manifest, rays=args.rays)
    except LayoutError as exc:
        print(f"build_skyexposure: {exc}", file=sys.stderr)
        return 1

    print(report(exposure))
    if args.report:
        return 0

    data = serialise(exposure)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(data)
    print(f"build_skyexposure: wrote {args.out} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
