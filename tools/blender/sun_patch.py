#!/usr/bin/env python3
"""sun_patch.py -- the shape the sun makes on the floor, for every hour of every season at once.

`HOUSE-00208`. `cna-house.md` §29.1: blob shadows "cannot cast a chair's shape onto a wall, or the
window frame's shape onto the floor -- and the second of those is the one that matters most in a
house. So Tier S adds one more thing: **baked window 'sun patch' decals**. For each window,
offline, we precompute the polygon the sun casts through it onto the room's floor and walls, for
the same 12 x 24 (altitude, azimuth) grid used by `shadingFactor`. At runtime the two nearest grid
entries are interpolated and the patch is drawn as an additive, softly-edged quad-strip tinted by
the sun colour and scaled by `(1 - cloudCover)³`."

    tools/blender/sun_patch.py SHELL.glb --world assets-src/world --out DIR
    tools/blender/sun_patch.py SHELL.glb --world assets-src/world --out DIR --subdivisions 4
    tools/blender/sun_patch.py --selftest

## "The two nearest grid entries are interpolated" decides the whole format

A patch is a polygon, and two polygons of different shapes cannot be blended vertex by vertex
unless they have the same vertices in the same order. So the patch is **not** the outline of
wherever the light happens to land: it is a fixed `(n+1) x (n+1)` lattice of points, one per corner
of an `n x n` subdivision of the window, in the same order at every one of the 288 nodes. Node 5
and node 6 then interpolate by a lerp per point, with nothing to match up and no vertex appearing
or disappearing as the sun moves.

That also settles what a ray that hits nothing does: it still produces a point, clamped inside the
room, because dropping it would change the vertex count and break the node it is being interpolated
with.

## Subdividing is what lets the patch bend

§29.1 asks for a **quad-strip**, not a quad, and the reason shows up the moment the sun is low: the
patch runs across the floor and part-way up the far wall. Projecting only the window's four corners
gives a flat quad that cuts through the wall; projecting an `n x n` lattice lets each little quad
land on whatever surface its own corners hit, so the patch folds along the floor/wall junction by
construction rather than by a special case.

## What it costs, measured

`(n+1)² * 3` coordinates per node, quantised to `u16` against the cell's own bounds -- 0.15 mm over
a 10 m room, far finer than a soft-edged decal can show. At the default `n = 3` that is 96 bytes a
node. Half the grid is empty because the sun is behind the window's wall, and an empty node costs
one byte, so a window is about 14 KB and §22's 81 windows about 1.1 MB. `--report` prints the real
figure for a real layout; §72 has no line for this yet and that number is what it needs.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import json
import math
import os
import struct
import sys

try:
    import bpy  # type: ignore
    from mathutils import Vector  # type: ignore

    INSIDE_BLENDER = True
except ImportError:
    INSIDE_BLENDER = False

if not INSIDE_BLENDER:
    from pathlib import Path

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import blender_env  # noqa: E402

    sys.exit(blender_env.relaunch(Path(__file__).resolve(), sys.argv[1:], tool="sun_patch"))

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import shading_factor as shf  # noqa: E402

MAGIC = b"CSUN"
VERSION = 1

#: The same grid as `HOUSE-00207`, because §29.1 says so and because a patch and its shading factor
#: are read together.
ALTITUDE_STEPS = shf.ALTITUDE_STEPS
AZIMUTH_STEPS = shf.AZIMUTH_STEPS

#: `n` in the `n x n` subdivision of the window. 3 gives a 4 x 4 lattice of 16 points.
DEFAULT_SUBDIVISIONS = 3

#: How far a projected ray travels before it is treated as having escaped the room.
MAX_THROW = 60.0

#: A ray starts this far inside the opening, so it cannot hit the reveal it is passing through.
RAY_EPSILON = 0.002


def report(message: str) -> None:
    print(f"  {message}")


# ================================================================================== the projection


def lattice(window: dict, subdivisions: int):
    """The `(n+1) x (n+1)` points of the window's own opening, in a fixed order.

    Fixed order is the whole point: §29.1 interpolates two grid nodes, and a lerp per point only
    means anything if point `k` is the same corner of the same little quad at both of them.
    """
    origin = Vector(window["origin"])
    across = Vector(window["across"])
    up = Vector(window["up"])
    points = []
    for j in range(subdivisions + 1):
        for i in range(subdivisions + 1):
            points.append(origin + across * (i / subdivisions) + up * (j / subdivisions))
    return points


def project_point(bvh, start, direction, bounds):
    """Where a ray from the window lands, or a point clamped inside the room if it lands nowhere.

    A miss still produces a point. Dropping it would change the node's vertex count, and §29.1's
    interpolation between two nodes cannot survive one of them having fewer points than the other.
    """
    hit = bvh.ray_cast(start + direction * RAY_EPSILON, direction, MAX_THROW)
    if hit[0] is not None:
        return hit[0]
    far = start + direction * MAX_THROW
    return Vector([min(max(far[k], bounds[k]), bounds[k + 3]) for k in range(3)])


def patch_for(bvh, window: dict, points, altitude_deg: float, azimuth_deg: float, bounds):
    """One node: where the sun through this window lands, or `None` when it cannot shine in.

    The light travels **opposite** to the direction of the sun: `sun_direction` points from the
    surface towards the sun, so inside the room the ray runs along its negation. Getting that
    backwards throws every patch out through the window onto the garden, where nothing sees it.
    """
    to_sun = shf.sun_direction(altitude_deg, azimuth_deg)
    if to_sun.dot(Vector(window["normal"])) <= 0.0:
        return None
    inward = -to_sun
    return [project_point(bvh, point, inward, bounds) for point in points]


def cell_bounds(bvh_vertices) -> tuple:
    lo = [min(v[k] for v in bvh_vertices) for k in range(3)]
    hi = [max(v[k] for v in bvh_vertices) for k in range(3)]
    # A degenerate axis would make the u16 quantisation divide by zero, and a room is never
    # actually flat -- but a fixture can be, so the span is floored rather than assumed.
    return tuple(lo) + tuple(max(hi[k], lo[k] + 1e-3) for k in range(3))


def scene_vertices():
    out = []
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        out.extend([obj.matrix_world @ vertex.co for vertex in mesh.vertices])
        evaluated.to_mesh_clear()
    return out


def bake_window(bvh, window: dict, bounds, subdivisions: int):
    """Every node of one window's grid: a list of points, or `None` where the sun cannot reach."""
    points = lattice(window, subdivisions)
    grid = []
    for altitude_index in range(ALTITUDE_STEPS):
        for azimuth_index in range(AZIMUTH_STEPS):
            grid.append(patch_for(bvh, window, points,
                                  shf.altitude_of(altitude_index),
                                  shf.azimuth_of(azimuth_index), bounds))
    return grid


# ========================================================================================= writer


def quantise(value: float, low: float, high: float) -> int:
    return max(0, min(65535, round((value - low) / (high - low) * 65535)))


def dequantise(code: int, low: float, high: float) -> float:
    return low + code / 65535 * (high - low)


def serialise(windows: list[dict], grids: dict, bounds: dict, subdivisions: int) -> bytes:
    out = bytearray()
    out += MAGIC
    out += struct.pack("<II", VERSION, 0)
    out += struct.pack("<III", ALTITUDE_STEPS, AZIMUTH_STEPS, subdivisions)
    out += struct.pack("<I", len(windows))
    for window in windows:
        raw = window["id"].encode("utf-8")
        out += struct.pack("<H", len(raw)) + raw
        raw = window["cell"].encode("utf-8")
        out += struct.pack("<H", len(raw)) + raw
        box = bounds[window["id"]]
        out += struct.pack("<6f", *box)
        for node in grids[window["id"]]:
            if node is None:
                out += struct.pack("<B", 0)
                continue
            out += struct.pack("<B", 1)
            for point in node:
                for k in range(3):
                    out += struct.pack("<H", quantise(point[k], box[k], box[k + 3]))
    return bytes(out)


def read_back(data: bytes) -> dict:
    at = 0

    def take(n):
        nonlocal at
        chunk = data[at:at + n]
        if len(chunk) != n:
            raise SystemExit(f"sunpatch.bin truncated at byte {at}, wanted {n} more")
        at += n
        return chunk

    def unpack(fmt):
        return struct.unpack(fmt, take(struct.calcsize(fmt)))

    def text():
        (length,) = unpack("<H")
        return bytes(take(length)).decode("utf-8")

    if bytes(take(4)) != MAGIC:
        raise SystemExit("not a sunpatch.bin: bad magic")
    version, flags = unpack("<II")
    if version != VERSION:
        raise SystemExit(f"sunpatch.bin is version {version}; this reader knows {VERSION}")
    if flags:
        raise SystemExit(f"sunpatch.bin sets unknown flag bits {flags:#x}")
    altitudes, azimuths, subdivisions = unpack("<III")
    count_points = (subdivisions + 1) ** 2
    (window_count,) = unpack("<I")
    windows = []
    for _ in range(window_count):
        identifier = text()
        cell = text()
        box = unpack("<6f")
        grid = []
        for _ in range(altitudes * azimuths):
            (present,) = unpack("<B")
            if not present:
                grid.append(None)
                continue
            node = []
            for _ in range(count_points):
                codes = unpack("<3H")
                node.append(tuple(dequantise(codes[k], box[k], box[k + 3]) for k in range(3)))
            grid.append(node)
        windows.append({"id": identifier, "cell": cell, "bounds": box, "grid": grid})
    if at != len(data):
        raise SystemExit(f"{len(data) - at} bytes left over after the last window")
    return {"altitudes": altitudes, "azimuths": azimuths, "subdivisions": subdivisions,
            "windows": windows}


def node_index(altitude_index: int, azimuth_index: int) -> int:
    return altitude_index * AZIMUTH_STEPS + azimuth_index


# ======================================================================================= selftest


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("sun_patch: selftest")

    # A room 6 x 3 x 4 with one south window, and NO eave -- this tool is about where the light
    # lands, and `HOUSE-00207` already owns what stops it arriving.
    bpy.ops.wm.read_factory_settings(use_empty=True)
    verts, faces = [], []

    def quad(a, b, c, d):
        base = len(verts)
        verts.extend([a, b, c, d])
        faces.append([base, base + 1, base + 2, base + 3])

    height, z_front, z_back = 3.0, 2.0, -2.0
    quad((-3.0, 0.0, z_back), (3.0, 0.0, z_back), (3.0, 0.0, z_front), (-3.0, 0.0, z_front))
    quad((-3.0, height, z_back), (3.0, height, z_back),
         (3.0, height, z_front), (-3.0, height, z_front))
    quad((-3.0, 0.0, z_back), (3.0, 0.0, z_back), (3.0, height, z_back), (-3.0, height, z_back))
    quad((-3.0, 0.0, z_back), (-3.0, 0.0, z_front), (-3.0, height, z_front),
         (-3.0, height, z_back))
    quad((3.0, 0.0, z_back), (3.0, 0.0, z_front), (3.0, height, z_front), (3.0, height, z_back))
    # The south wall, with a 2.0 x 1.2 m opening from y 0.9 to 2.1.
    quad((-3.0, 0.0, z_front), (-1.0, 0.0, z_front), (-1.0, height, z_front),
         (-3.0, height, z_front))
    quad((1.0, 0.0, z_front), (3.0, 0.0, z_front), (3.0, height, z_front), (1.0, height, z_front))
    quad((-1.0, 0.0, z_front), (1.0, 0.0, z_front), (1.0, 0.9, z_front), (-1.0, 0.9, z_front))
    quad((-1.0, 2.1, z_front), (1.0, 2.1, z_front), (1.0, height, z_front),
         (-1.0, height, z_front))

    mesh = bpy.data.meshes.new("shell")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new("shell", mesh)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj

    window = {"id": "W_BED_S1", "portal": "P", "cell": "L1_BED",
              "origin": (-1.0, 0.9, z_front), "across": (2.0, 0.0, 0.0), "up": (0.0, 1.2, 0.0),
              "normal": (0.0, 0.0, 1.0)}
    bvh = shf.build_bvh()
    bounds = cell_bounds(scene_vertices())
    subdivisions = DEFAULT_SUBDIVISIONS
    points = lattice(window, subdivisions)

    # 1. The lattice: fixed size, fixed order, and covering the opening.
    require(len(points) == (subdivisions + 1) ** 2,
            f"the lattice is (n+1)^2 = {(subdivisions + 1) ** 2} points ({len(points)})")
    require(all(-1.0 - 1e-6 <= p.x <= 1.0 + 1e-6 and 0.9 - 1e-6 <= p.y <= 2.1 + 1e-6
                for p in points),
            "every lattice point is on the window's own opening")
    require(abs(points[0].x + 1.0) < 1e-5 and abs(points[0].y - 0.9) < 1e-5,
            "point 0 is the same corner every time -- the order is what §29.1's interpolation "
            "between two nodes is a lerp over")
    # ROW-MAJOR along `across`, and the claim has to say so: checking point 0 alone cannot tell a
    # row-major lattice from a transposed one, since both start at the same corner. §29.1 draws a
    # quad-STRIP, and a strip's indices assume consecutive points run along a row.
    step_along = points[1] - points[0]
    step_up = points[subdivisions + 1] - points[0]
    require(abs(step_along.x - 2.0 / subdivisions) < 1e-5 and abs(step_along.y) < 1e-5,
            f"consecutive points step ALONG the window "
            f"({tuple(round(v, 3) for v in step_along)}), not up it")
    require(abs(step_up.y - 1.2 / subdivisions) < 1e-5 and abs(step_up.x) < 1e-5,
            f"and point k+(n+1) steps UP it ({tuple(round(v, 3) for v in step_up)}) -- row-major, "
            f"which is what a quad-strip's indices assume")

    # 2. The light travels OPPOSITE the sun. Backwards, every patch lands in the garden.
    south = AZIMUTH_STEPS // 2
    node = patch_for(bvh, window, points, shf.altitude_of(4), shf.azimuth_of(south), bounds)
    require(node is not None, "a southern sun does shine through a south-facing window")
    require(all(p.z < z_front - 1e-6 for p in node),
            f"every landing point is INSIDE the room, behind the window plane "
            f"(z < {z_front}); the sun's own direction points the other way and would put the "
            f"patch on the lawn")
    require(all(bounds[k] - 1e-4 <= p[k] <= bounds[k + 3] + 1e-4 for p in node for k in range(3)),
            "and inside the room's bounds on every axis")

    # 3. A low sun throws a long patch and a high sun a short one. This IS the feature: §29.1's
    #    "moving sun patches crossing a bedroom floor through the afternoon".
    low = patch_for(bvh, window, points, shf.altitude_of(1), shf.azimuth_of(south), bounds)
    high = patch_for(bvh, window, points, shf.altitude_of(8), shf.azimuth_of(south), bounds)
    low_z = sum(p.z for p in low) / len(low)
    high_z = sum(p.z for p in high) / len(high)
    require(low_z < high_z,
            f"a low sun reaches further into the room than a high one "
            f"(mean z {low_z:.2f} against {high_z:.2f})")
    # The claim this replaced was "a low sun throws a LONGER patch", and it is false in a room
    # this size: the low patch runs out of floor and is truncated by the back wall four metres
    # away, so its extent (2.33 m) is no larger than the high one's (2.07 m). What is true in any
    # room, and is what §29.1 actually asks to see, is that the patch MOVES -- monotonically, so
    # it sweeps rather than jumps.
    depths = []
    for altitude_index in range(ALTITUDE_STEPS):
        node = patch_for(bvh, window, points, shf.altitude_of(altitude_index),
                         shf.azimuth_of(south), bounds)
        if node is not None:
            depths.append(sum(p.z for p in node) / len(node))
    require(depths == sorted(depths),
            f"the patch retreats towards the window monotonically as the sun rises "
            f"({[round(v, 2) for v in depths]}) -- §29.1's 'moving sun patches crossing a bedroom "
            f"floor through the afternoon', with no reversal for the eye to catch")
    require(max(depths) - min(depths) > 2.0,
            f"...and it crosses {max(depths) - min(depths):.2f} m of the room while doing it, so "
            f"there is a sweep to see")

    # 4. A high sun puts the patch on the FLOOR; a very low one puts it on the back wall. The
    #    subdivision is what lets one patch be on both at once.
    require(max(p.y for p in high) < 0.5,
            f"a high sun lands on the floor ({max(p.y for p in high):.2f} m up)")
    grazing = patch_for(bvh, window, points, shf.altitude_of(0), shf.azimuth_of(south), bounds)
    require(max(p.y for p in grazing) > 0.5,
            f"a sun on the horizon lands up the back wall "
            f"({max(p.y for p in grazing):.2f} m up)")
    folded = [n for n in (patch_for(bvh, window, points, shf.altitude_of(a),
                                    shf.azimuth_of(south), bounds)
                          for a in range(ALTITUDE_STEPS))
              if n is not None and min(p.y for p in n) < 0.05 and max(p.y for p in n) > 0.3]
    require(folded,
            "at least one altitude puts part of the patch on the floor and part up the wall -- "
            "which is why §29.1 asks for a quad-STRIP, and what projecting only the window's four "
            "corners could not represent")

    # 5. The sun behind the wall has no patch at all, and that is one byte rather than sixteen
    #    points of nothing.
    grid = bake_window(bvh, window, bounds, subdivisions)
    empty = sum(1 for n in grid if n is None)
    require(empty >= AZIMUTH_STEPS * ALTITUDE_STEPS // 2 - ALTITUDE_STEPS,
            f"about half the grid is empty because the sun is behind the wall ({empty} of "
            f"{len(grid)})")
    require(all(len(n) == len(points) for n in grid if n is not None),
            "every non-empty node has exactly the same number of points, which is what makes "
            "§29.1's interpolation a lerp and not a polygon-matching problem")

    # 6. Adjacent nodes are close together, so interpolating between them is meaningful. A patch
    #    that jumped across the room between two neighbouring azimuths would tear on screen.
    jumps = []
    for altitude_index in range(ALTITUDE_STEPS):
        for azimuth_index in range(AZIMUTH_STEPS):
            here = grid[node_index(altitude_index, azimuth_index)]
            there = grid[node_index(altitude_index, (azimuth_index + 1) % AZIMUTH_STEPS)]
            if here is None or there is None:
                continue
            jumps.append(max((a - b).length for a, b in zip(here, there)))
    require(jumps, "there are adjacent node pairs that both have a patch")
    require(max(jumps) < 8.0,
            f"no two adjacent nodes move a point more than the room is wide "
            f"({max(jumps):.2f} m) -- interpolating between them is a lerp over a short distance, "
            f"not a jump across the floor")

    # 7. Quantisation. u16 against the room's own bounds.
    box = bounds
    worst = 0.0
    for node_points in grid:
        if node_points is None:
            continue
        for point in node_points:
            for k in range(3):
                code = quantise(point[k], box[k], box[k + 3])
                worst = max(worst, abs(dequantise(code, box[k], box[k + 3]) - point[k]))
    require(worst < 0.001,
            f"u16 against the room's bounds costs at most {worst * 1000:.3f} mm, far finer than a "
            f"soft-edged decal can show")

    # 8. Round trip and determinism.
    windows = [window]
    grids = {window["id"]: grid}
    boxes = {window["id"]: bounds}
    data = serialise(windows, grids, boxes, subdivisions)
    back = read_back(data)
    require(back["subdivisions"] == subdivisions and back["altitudes"] == ALTITUDE_STEPS
            and back["azimuths"] == AZIMUTH_STEPS,
            "the grid shape and the subdivision travel with the file, so a reader knows how many "
            "points to expect before it reads them")
    read_grid = back["windows"][0]["grid"]
    require([n is None for n in read_grid] == [n is None for n in grid],
            "every empty node round trips as empty")
    require(all(abs(a[k] - b[k]) < 0.001
                for x, y in zip(read_grid, grid) if x is not None
                for a, b in zip(x, y) for k in range(3)),
            "and every point of every patch survives to within the quantum")
    require(back["windows"][0]["id"] == "W_BED_S1" and back["windows"][0]["cell"] == "L1_BED",
            "the window and its cell round trip")
    require(serialise(windows, {window["id"]: bake_window(bvh, window, bounds, subdivisions)},
                      boxes, subdivisions) == data,
            "two bakes of one scene produce byte-identical output")

    # 9. What it costs, measured rather than asserted.
    per_window = len(data) - 4 - 8 - 12 - 4
    require(per_window < 20 * 1024,
            f"one window is {per_window / 1024:.1f} KB at n={subdivisions}, so §22's 81 windows "
            f"are about {per_window * 81 / (1024 * 1024):.2f} MB -- a number §72 does not have a "
            f"line for yet")

    # 10. Truncation, version, flags and trailing bytes.
    for cut in (3, 12, len(data) - 1):
        try:
            read_back(data[:cut])
            caught = False
        except SystemExit:
            caught = True
        require(caught, f"a file truncated to {cut} bytes is refused")
    try:
        read_back(data + b"\0")
        caught = False
    except SystemExit as exc:
        caught = "left over" in str(exc)
    require(caught, "trailing bytes are refused")
    for offset, value, what in ((4, 6, "version"), (8, 0x20, "flag bit")):
        broken = bytearray(data)
        broken[offset:offset + 4] = struct.pack("<I", value)
        try:
            read_back(bytes(broken))
            caught = False
        except SystemExit:
            caught = True
        require(caught, f"an unknown {what} is refused rather than ignored")

    if failures:
        return 1
    print("sun_patch: selftest passed.")
    return 0


def main() -> int:
    argv = shf.blender_argv()
    if "--selftest" in argv:
        return selftest()

    options = {}
    index = 0
    while index < len(argv):
        if argv[index].startswith("--") and index + 1 < len(argv) \
                and not argv[index + 1].startswith("--"):
            options[argv[index][2:]] = argv[index + 1]
            index += 2
        else:
            index += 1
    positional = [a for a in argv if not a.startswith("--") and a not in options.values()]

    if len(positional) != 1 or "world" not in options or "out" not in options:
        print("sun_patch: usage: SHELL.glb --world DIR --out DIR [--subdivisions N]",
              file=sys.stderr)
        return 2

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=positional[0])
    windows = shf.windows_from_layout(options["world"])
    if not windows:
        print("sun_patch: layout.openings.json declares no window", file=sys.stderr)
        return 1
    subdivisions = int(options.get("subdivisions", DEFAULT_SUBDIVISIONS))
    bvh = shf.build_bvh()
    bounds = cell_bounds(scene_vertices())
    grids, boxes = {}, {}
    for window in windows:
        grids[window["id"]] = bake_window(bvh, window, bounds, subdivisions)
        boxes[window["id"]] = bounds
        lit = sum(1 for n in grids[window["id"]] if n is not None)
        report(f"{window['id']}: {lit} of {ALTITUDE_STEPS * AZIMUTH_STEPS} nodes cast a patch")
    os.makedirs(options["out"], exist_ok=True)
    path = os.path.join(options["out"], "sunpatch.bin")
    data = serialise(windows, grids, boxes, subdivisions)
    with open(path, "wb") as handle:
        handle.write(data)
    report(f"wrote {path} ({len(windows)} window(s), {len(data) / 1024:.1f} KB, "
           f"n={subdivisions})")
    return 0


if __name__ == "__main__":
    # Blender does not propagate a script's exit status; see tools/blender/blender_env.py.
    try:
        _status = main()
    except SystemExit as _exit:
        if isinstance(_exit.code, str):
            print(_exit.code, file=sys.stderr)
            _status = 1
        else:
            _status = int(_exit.code or 0)
    except BaseException:  # noqa: BLE001
        import traceback

        traceback.print_exc()
        print("sun_patch: EXIT 1")
        raise
    print(f"sun_patch: EXIT {_status}")
    sys.exit(_status)
