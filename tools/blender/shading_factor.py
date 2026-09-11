#!/usr/bin/env python3
"""shading_factor.py -- how much of each window the sun actually reaches, from every direction.

`HOUSE-00207`. `cna-house.md` §22: "`shadingFactor` is precomputed offline per window per (sun
altitude, azimuth) on a 12 x 24 grid by ray-casting against the house and neighbour geometry in
Blender -- so the porch roof genuinely keeps the sun out of the foyer in the afternoon, and the
west neighbour's gable shades the study at sunset. 81 windows x 288 samples x 1 byte = 23 KB.
Cheap, and it is the single detail that makes interior daylight feel real."

    tools/blender/shading_factor.py SHELL --world assets-src/world --out DIR
    tools/blender/shading_factor.py SHELL --world assets-src/world --out DIR --samples 6
    tools/blender/shading_factor.py --selftest

`SHELL` is a `.glb` file **or a directory of them**, because the house shell is a directory of
them: `build_shell.py` writes one `.glb` per cell and there has never been a single file to hand
this. `--neighbourhood DIR` additionally places `layout.exterior.json`'s neighbour rows from a
model library, which is the other half of what §22 says to cast against -- *"the house and
neighbour geometry"* -- and without it the west neighbour's gable cannot shade anything.

## Why the window is SAMPLED and not probed once

A single ray from the window's centre answers a yes/no question, and the answer this feeds is not
yes/no: §22 multiplies `shadingFactor` into a daylight sum, so a window half-covered by an eave
must read about a half. One ray reads 1 until the shadow crosses the centre and then 0 -- the
foyer's daylight would step rather than slide as the sun moved, once per window per afternoon.
An `n x n` grid over the window's own rectangle costs `n²` rays and gives the fraction directly.

## The grid is NODES, not cells

12 altitudes and 24 azimuths, sampled **at** 0, 90/11, ... 90 degrees and **at** 0, 15, ... 345 --
the boundaries of the domain rather than the middles of its cells. The runtime interpolates
bilinearly between four samples, and node sampling makes that exact at the ends: altitude 0 and
altitude 90 are measured rather than extrapolated, and the azimuth axis wraps from 345 back to 0
with no seam. Cell-centre sampling would leave the horizon and the zenith to be guessed from the
nearest interior sample, and the horizon is where a low sun and a long shadow make the difference
most visible.

## One byte per sample, and what that costs

§22 budgets `1 byte`, so a factor is stored as `round(fraction * 255)`. The quantum is 0.4 %, well
under what an 8-bit daylight response can show, and the selftest measures the round-trip error
rather than asserting it.

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
    from mathutils import Matrix, Vector  # type: ignore
    from mathutils.bvhtree import BVHTree  # type: ignore

    INSIDE_BLENDER = True

    #: Blender's glTF import maps `(x, y, z)` to `(x, -z, y)`. This is the inverse, and every
    #: import in this tool is composed with it -- see `import_gltf`.
    GLTF_TO_SECTION14 = Matrix.Rotation(math.radians(-90.0), 4, "X")
except ImportError:
    INSIDE_BLENDER = False

if not INSIDE_BLENDER:
    from pathlib import Path

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import blender_env  # noqa: E402

    sys.exit(blender_env.relaunch(Path(__file__).resolve(), sys.argv[1:], tool="shading_factor"))

MAGIC = b"CSHF"
VERSION = 1

#: §22's grid. Nodes, not cell centres -- see the module docstring.
ALTITUDE_STEPS = 12
AZIMUTH_STEPS = 24

#: Samples across the window rectangle, per axis. 4 x 4 = 16 rays per grid node; §22's 81 windows
#: over 288 nodes is then 373 000 rays, which a BVH answers in seconds.
DEFAULT_SAMPLES = 4

#: A ray starts this far off the glass along the window's outward normal. Starting exactly on the
#: surface lets a ray hit the very face it left, and the window would shade itself completely.
#:
#: **This is the clearance past the SASH, not past the wall**, and the difference was found by
#: `HOUSE-01279` running this tool against the real house for the first time. The shell draws the
#: window leaf in the opening -- `layout.openings.json` gives it a `thickness`, 0.03 m for the
#: standard double-hung -- and the leaf is centred on the portal plane, so 0.002 m from that plane
#: is 0.013 m INSIDE the glass. A ray cast from inside a solid hits it immediately whichever way it
#: points, and every window in the house reported 0 of 288 nodes lit. The offset is now
#: `leaf/2 + RAY_EPSILON`; this constant is what is left when a window has no leaf at all.
#:
#: It stays far below the 0.15 m thinnest wall the layout can build, so it still cannot push a
#: sample through the geometry it is measuring: 0.017 m for the standard sash.
RAY_EPSILON = 0.002

#: Far enough to leave the property and the neighbours behind.
RAY_DISTANCE = 200.0

#: How far inside the portal rectangle the GLASS starts, in metres.
#:
#: `house_shell_gen` draws a frame ring `FRAME_SECTION` deep round the hole and a sash ring
#: `SASH_SECTION` inside that, both solid, and glazes what is left. Imported from that module
#: rather than restated, because two numbers that must agree and live in two files eventually do
#: not: a frame section changed there and not here would bias every small window's daylight and
#: nothing would report it.
try:
    from house_shell_gen import FRAME_SECTION as _FRAME, SASH_SECTION as _SASH

    GLAZING_INSET = _FRAME + _SASH
except ImportError:  # pragma: no cover -- only when run outside the tools directory
    GLAZING_INSET = 0.055 + 0.042


def report(message: str) -> None:
    print(f"  {message}")


# ============================================================================== the grid and angles


def altitude_of(index: int) -> float:
    """Degrees. Node `0` is the horizon and node `ALTITUDE_STEPS - 1` is the zenith."""
    return 90.0 * index / (ALTITUDE_STEPS - 1)


def azimuth_of(index: int) -> float:
    """Degrees clockwise from north. Node `0` is north; the axis wraps, so 360 is not sampled."""
    return 360.0 * index / AZIMUTH_STEPS


def sun_direction(altitude_deg: float, azimuth_deg: float):
    """A unit vector pointing FROM the surface TOWARDS the sun, in §14's axes.

    §14: right-handed, Y up, **-Z is north**, +Z is the road. So azimuth 0 (north) is -Z, and
    azimuth 90 (east) is +X. Getting this wrong does not produce an error, it produces a house
    whose windows are shaded at the wrong time of day -- and it looks plausible all afternoon.
    """
    altitude = math.radians(altitude_deg)
    azimuth = math.radians(azimuth_deg)
    horizontal = math.cos(altitude)
    return Vector((horizontal * math.sin(azimuth), math.sin(altitude),
                   -horizontal * math.cos(azimuth)))


# ================================================================================== the ray tests


def import_gltf(path: str) -> list:
    """Imports one `.glb` and puts it back into §14's axes. Returns the objects it added.

    **Blender's glTF importer rotates the scene and this is not optional.** glTF is Y-up and
    Blender is Z-up, so `import_scene.gltf` maps `(x, y, z)` to `(x, -z, y)` on the way in. Every
    other coordinate in this tool -- `sun_direction`, the window rectangles out of
    `layout.portals.json`, the outward normals -- is in §14's frame, which is glTF's: Y up, -Z
    north. Leaving the import converted puts the rays and the geometry in DIFFERENT FRAMES, and
    the failure is silent: rays that should go up go north, so a ground-floor window is reported
    as shaded by the basement and the file looks entirely plausible.

    Measured 2026-09-11 on `build/shell/ROOF_MAIN.glb`: imported unrotated, the roof's 0..14.3 m
    of height sits on Blender's Z and its Y holds 13.9..27.5, which is the house's northing.
    `R(-90 deg, X)` maps `(x, y, z)` to `(x, z, -y)` and is exactly the inverse.
    """
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    fresh = [o for o in bpy.context.scene.objects if o not in before]
    for obj in fresh:
        # Only the roots: a child's world matrix follows its parent's.
        if obj.parent is None or obj.parent not in fresh:
            obj.matrix_world = GLTF_TO_SECTION14 @ obj.matrix_world
    return fresh


def import_shell(path: str) -> int:
    """Imports @p path, which may be one `.glb` or a DIRECTORY of them.

    The house shell is a directory: `build_shell.py` writes one `.glb` per cell -- 99 of them,
    including the roofs and the chimney, which are exactly the geometry §22 means by "the porch
    roof genuinely keeps the sun out of the foyer". Sorted, so two runs over the same directory
    import in the same order and produce the same bytes.
    """
    if os.path.isdir(path):
        files = sorted(f for f in os.listdir(path) if f.endswith(".glb"))
        if not files:
            raise SystemExit(f"shading_factor: {path} holds no .glb")
        for name in files:
            import_gltf(os.path.join(path, name))
        return len(files)
    import_gltf(path)
    return 1


def import_neighbourhood(world_dir: str, library_dir: str) -> int:
    """Places `layout.exterior.json`'s neighbour rows from @p library_dir.

    §22 casts against *"the house and neighbour geometry"*, and the neighbourhood is not geometry
    on disk -- it is 122 placements of a dozen models. Each referenced model is imported ONCE and
    linked into the scene at each placement, so the BVH sees 122 buildings for the cost of loading
    twelve. A row whose model is missing from the library is reported and skipped rather than
    failing the bake: an absent neighbour shades nothing, which is a smaller error than no file.
    """
    path = os.path.join(world_dir, "layout.exterior.json")
    if not os.path.isfile(path):
        raise SystemExit(f"shading_factor: {path} is required by --neighbourhood and not present")
    with open(path, encoding="utf-8") as handle:
        rows = json.loads(_strip(handle.read())).get("neighbourhood", [])
    loaded = {}
    placed = 0
    missing = set()
    for row in rows:
        asset = row.get("asset")
        if asset is None:
            continue
        if asset not in loaded:
            path = os.path.join(library_dir, f"{asset}.glb")
            if not os.path.exists(path):
                missing.add(asset)
                loaded[asset] = None
                continue
            fresh = [o for o in import_gltf(path) if o.type == "MESH"]
            for obj in fresh:
                bpy.context.scene.collection.objects.unlink(obj)
            loaded[asset] = fresh
        if not loaded[asset]:
            continue
        position = row.get("position", [0.0, 0.0, 0.0])
        yaw = math.radians(float(row.get("yawDeg", 0.0)))
        # `position` and `yawDeg` are §14's, and `import_gltf` has already put the model there,
        # so the placement is a plain yaw about §14's up (+Y) and a translation. Composed onto the
        # model's own matrix rather than assigned, because the import left one there.
        placement = (Matrix.Translation((float(position[0]), float(position[1]), float(position[2])))
                     @ Matrix.Rotation(yaw, 4, "Y"))
        for source in loaded[asset]:
            copy = source.copy()
            copy.data = source.data
            copy.matrix_world = placement @ source.matrix_world
            bpy.context.scene.collection.objects.link(copy)
        placed += 1
    for asset in sorted(missing):
        report(f"neighbourhood: no model for {asset}; that neighbour shades nothing")
    return placed


def build_bvh():
    """One BVH over every mesh in the scene, in world space -- the house and its neighbours."""
    vertices = []
    polygons = []
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        matrix = obj.matrix_world
        base = len(vertices)
        vertices.extend([matrix @ vertex.co for vertex in mesh.vertices])
        for polygon in mesh.polygons:
            indices = [base + i for i in polygon.vertices]
            for k in range(1, len(indices) - 1):
                polygons.append((indices[0], indices[k], indices[k + 1]))
        evaluated.to_mesh_clear()
    if not polygons:
        raise SystemExit("shading_factor: the scene has no geometry to cast against")
    return BVHTree.FromPolygons(vertices, polygons, all_triangles=True)


def window_samples(window: dict, samples: int):
    """`samples x samples` points spread over the window's GLAZED aperture, pushed off the glass.

    Spread over the opening, not clustered at its centre: an eave shades the top of a window
    before the bottom, and a reveal shades one jamb before the other, and both of those are
    fractions this has to be able to report.

    **Over the GLASS, not over the portal rectangle**, and the difference is not cosmetic.
    `house_shell_gen` fills the portal rectangle with a frame ring `FRAME_SECTION` deep and a sash
    ring `SASH_SECTION` inside that, both solid; only what is left is glazed. A sample that lands
    on the frame is inside geometry and its ray is blocked whichever way it points, so that sample
    contributes a permanent zero to every one of the window's 288 nodes.

    Found by `HOUSE-01279`. The bias is invisible on a big window and total on a small one, which
    is why it looked like a data defect in particular rooms: with a 4x4 grid and the shell's
    0.055 + 0.042 m inset, all 16 samples of a 1.18 x 1.48 m `W_DH_STD` land on glass, exactly 8
    of a 0.68 x 0.88 m `W_BATH` do, and exactly 8 of a 0.28 m wide `W_SIDELIGHT` do -- which is
    what was making the foyer read a fifth of the study's daylight.
    """
    origin = Vector(window["origin"])
    across = Vector(window["across"])
    up = Vector(window["up"])
    normal = Vector(window["normal"]).normalized()
    # Clear of the sash the shell draws in the opening, not merely clear of the portal plane.
    offset = float(window.get("leafThickness", 0.0)) / 2.0 + RAY_EPSILON

    # The glazed aperture: the portal rectangle less the frame and sash rings, as fractions of each
    # edge. A window too small to have any glass left would divide by nothing; it falls back to the
    # whole rectangle and says so, because a zero-area window is a data defect and not this tool's
    # to silently paper over.
    width = across.length
    height = up.length
    inset = GLAZING_INSET
    if width <= 2.0 * inset or height <= 2.0 * inset:
        report(f"{window['id']}: {width:.2f} x {height:.2f} m leaves no glass inside a "
               f"{inset:.3f} m frame and sash; sampling the whole opening")
        u_lo, u_hi, v_lo, v_hi = 0.0, 1.0, 0.0, 1.0
    else:
        u_lo, u_hi = inset / width, 1.0 - inset / width
        v_lo, v_hi = inset / height, 1.0 - inset / height

    points = []
    for i in range(samples):
        for j in range(samples):
            u = u_lo + (u_hi - u_lo) * (i + 0.5) / samples
            v = v_lo + (v_hi - v_lo) * (j + 0.5) / samples
            points.append(origin + across * u + up * v + normal * offset)
    return points


def shading_factor(bvh, points, direction) -> float:
    """The fraction of the window's samples with a clear line to the sun."""
    clear = 0
    for point in points:
        hit = bvh.ray_cast(point, direction, RAY_DISTANCE)
        if hit[0] is None:
            clear += 1
    return clear / len(points)


def bake_window(bvh, window: dict, samples: int) -> list[int]:
    """One window's whole 12 x 24 grid, as bytes in altitude-major order."""
    points = window_samples(window, samples)
    out = []
    for altitude_index in range(ALTITUDE_STEPS):
        for azimuth_index in range(AZIMUTH_STEPS):
            direction = sun_direction(altitude_of(altitude_index), azimuth_of(azimuth_index))
            # A sun behind the window's own wall cannot light it however clear the sky is. The
            # geometry would report the same thing -- the wall is in the way -- but only after
            # sixteen rays, and this is the majority of the grid.
            if direction.dot(Vector(window["normal"])) <= 0.0:
                out.append(0)
                continue
            out.append(round(shading_factor(bvh, points, direction) * 255))
    return out


# =========================================================================== windows from the layout


def windows_from_layout(world_dir: str) -> list[dict]:
    """Every `kind: window` opening, with the rectangle and outward normal its portal implies."""
    def load(name):
        path = os.path.join(world_dir, name)
        if not os.path.isfile(path):
            raise SystemExit(f"shading_factor: {path} is required and not present")
        with open(path, encoding="utf-8") as handle:
            return json.loads(_strip(handle.read()))

    portals = {row["id"]: row for row in load("layout.portals.json")["portals"]}
    cells = {row["id"]: row for row in load("layout.cells.json")["cells"]}
    out = []
    for opening in load("layout.openings.json")["openings"]:
        if opening.get("kind") != "window":
            continue
        portal = portals.get(opening.get("portal"))
        if portal is None:
            raise SystemExit(
                f"shading_factor: window {opening['id']!r} names portal "
                f"{opening.get('portal')!r}, which does not exist (validator rule 7)")
        out.append(window_frame(opening["id"], portal, cells,
                                float((opening.get("leaf") or {}).get("thickness", 0.0))))
    return out


def _strip(text: str) -> str:
    """The world files are JSONC. `tools/world/layout_io.py` owns the real stripper; this is the
    same two rules, kept local because a Blender tool cannot import from `tools/world` without
    putting a second directory on Blender's path for two functions."""
    out = []
    i, n, in_string = 0, len(text), False
    while i < n:
        ch = text[i]
        if in_string:
            out.append(ch)
            if ch == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if ch == '"':
                in_string = False
            i += 1
            continue
        if ch == '"':
            in_string = True
            out.append(ch)
            i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            out.append("  ")
            i += 2
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def window_frame(identifier: str, portal: dict, cells: dict, leafThickness: float = 0.0) -> dict:
    """A portal rectangle as an origin and two edge vectors, with the outward normal.

    Outward is **away from the interior cell**, decided from the cells' own boxes rather than from
    the order `cellA`/`cellB` happen to appear in: a window authored the other way round would
    otherwise cast every ray into the room it is trying to light.
    """
    plane, rect = portal["plane"], portal["rect"]
    axis, value = plane["axis"], float(plane["value"])
    u0, u1 = float(rect["u"][0]), float(rect["u"][1])
    v0, v1 = float(rect["v"][0]), float(rect["v"][1])

    interior = cells.get(portal["cellA"])
    if interior is None or interior.get("kind") == "exterior":
        interior = cells.get(portal["cellB"])
    if interior is None:
        raise SystemExit(
            f"shading_factor: neither cell of portal {portal['id']!r} exists, so there is no "
            f"inside to face away from")
    boxes = interior.get("boxes") or []
    if not boxes:
        raise SystemExit(f"shading_factor: cell {interior['id']!r} has no boxes")
    centre_axis = sum((float(b[axis][0]) + float(b[axis][1])) / 2 for b in boxes) / len(boxes)
    outward = 1.0 if value > centre_axis else -1.0

    if axis == "x":
        origin = (value, v0, u0)
        across = (0.0, 0.0, u1 - u0)
        normal = (outward, 0.0, 0.0)
    else:
        origin = (u0, v0, value)
        across = (u1 - u0, 0.0, 0.0)
        normal = (0.0, 0.0, outward)
    return {"id": identifier, "portal": portal["id"], "cell": interior["id"],
            "origin": origin, "across": across, "up": (0.0, v1 - v0, 0.0), "normal": normal,
            "leafThickness": float(leafThickness)}


# ========================================================================================= writer


def serialise(windows: list[dict], grids: dict[str, list[int]], samples: int) -> bytes:
    out = bytearray()
    out += MAGIC
    out += struct.pack("<II", VERSION, 0)
    out += struct.pack("<III", ALTITUDE_STEPS, AZIMUTH_STEPS, samples)
    out += struct.pack("<I", len(windows))
    for window in windows:
        raw = window["id"].encode("utf-8")
        out += struct.pack("<H", len(raw)) + raw
        raw = window["cell"].encode("utf-8")
        out += struct.pack("<H", len(raw)) + raw
        out += struct.pack("<3f", *window["normal"])
        out += bytes(grids[window["id"]])
    return bytes(out)


def read_back(data: bytes) -> dict:
    at = 0

    def take(n):
        nonlocal at
        chunk = data[at:at + n]
        if len(chunk) != n:
            raise SystemExit(f"shading.bin truncated at byte {at}, wanted {n} more")
        at += n
        return chunk

    def unpack(fmt):
        return struct.unpack(fmt, take(struct.calcsize(fmt)))

    def text():
        (length,) = unpack("<H")
        return bytes(take(length)).decode("utf-8")

    if bytes(take(4)) != MAGIC:
        raise SystemExit("not a shading.bin: bad magic")
    version, flags = unpack("<II")
    if version != VERSION:
        raise SystemExit(f"shading.bin is version {version}; this reader knows {VERSION}")
    if flags:
        raise SystemExit(f"shading.bin sets unknown flag bits {flags:#x}")
    altitudes, azimuths, samples = unpack("<III")
    (count,) = unpack("<I")
    windows = []
    for _ in range(count):
        identifier = text()
        cell = text()
        normal = unpack("<3f")
        grid = list(take(altitudes * azimuths))
        windows.append({"id": identifier, "cell": cell, "normal": normal, "grid": grid})
    if at != len(data):
        raise SystemExit(f"{len(data) - at} bytes left over after the last window")
    return {"altitudes": altitudes, "azimuths": azimuths, "samples": samples,
            "windows": windows}


def at(grid: list[int], altitude_index: int, azimuth_index: int) -> float:
    """One node of a grid, as a fraction."""
    return grid[altitude_index * AZIMUTH_STEPS + azimuth_index] / 255.0


# ======================================================================================= fixtures


def build_eave_fixture(with_neighbour: bool = True):
    """One south-facing window, an eave over it, and a neighbour's gable to the west.

    §22 names both effects by name -- "the porch roof genuinely keeps the sun out of the foyer in
    the afternoon, and the west neighbour's gable shades the study at sunset" -- so the fixture is
    those two, and the claims are that each shows up in the grid where it should and nowhere else.
    """
    bpy.ops.wm.read_factory_settings(use_empty=True)
    verts, faces = [], []

    def quad(a, b, c, d):
        base = len(verts)
        verts.extend([a, b, c, d])
        faces.append([base, base + 1, base + 2, base + 3])

    # A south wall at z = +2 with a 2 m x 1.2 m window from y 0.9 to 2.1, x -1 to 1. §14 puts the
    # road at +Z, so "south" is +Z and a south window faces azimuth 180.
    z = 2.0
    quad((-6.0, 0.0, z), (-1.0, 0.0, z), (-1.0, 3.0, z), (-6.0, 3.0, z))     # left of the window
    quad((1.0, 0.0, z), (6.0, 0.0, z), (6.0, 3.0, z), (1.0, 3.0, z))         # right
    quad((-1.0, 0.0, z), (1.0, 0.0, z), (1.0, 0.9, z), (-1.0, 0.9, z))       # under the sill
    quad((-1.0, 2.1, z), (1.0, 2.1, z), (1.0, 3.0, z), (-1.0, 3.0, z))       # over the head
    # THE EAVE: a horizontal slab 0.6 m above the window head, projecting 1.2 m out.
    quad((-3.0, 2.7, z), (3.0, 2.7, z), (3.0, 2.7, z + 1.2), (-3.0, 2.7, z + 1.2))
    # THE NEIGHBOUR'S GABLE, to the south-WEST. Placing it due west is the mistake this fixture
    # was built with first: a south-facing window's rays only ever travel towards +Z, so nothing
    # due west of it -- at the same Z -- can ever be hit, and the claim measured the eave instead.
    # A ray leaving the window towards azimuth 225 reaches x = -6 at about z = 8, so that is where
    # the gable is.
    if with_neighbour:
        quad((-6.0, 0.0, z + 2.0), (-6.0, 0.0, z + 8.0),
             (-6.0, 8.0, z + 8.0), (-6.0, 8.0, z + 2.0))

    mesh = bpy.data.meshes.new("shell")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    obj = bpy.data.objects.new("shell", mesh)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj

    window = {"id": "W_STUDY_S1", "portal": "P_STUDY__EXT", "cell": "L0_STUDY",
              "origin": (-1.0, 0.9, z), "across": (2.0, 0.0, 0.0), "up": (0.0, 1.2, 0.0),
              "normal": (0.0, 0.0, 1.0)}
    return [window]


# ======================================================================================= selftest


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("shading_factor: selftest")

    # 1. The angles, against §14's axes. Getting these wrong shades the house at the wrong time of
    #    day and looks plausible all afternoon.
    north = sun_direction(0.0, 0.0)
    east = sun_direction(0.0, 90.0)
    south = sun_direction(0.0, 180.0)
    zenith = sun_direction(90.0, 0.0)
    require(abs(north.z + 1.0) < 1e-9 and abs(north.x) < 1e-9,
            f"azimuth 0 is north, and §14's north is -Z ({tuple(round(v, 4) for v in north)})")
    require(abs(east.x - 1.0) < 1e-9, "azimuth 90 is east, +X")
    require(abs(south.z - 1.0) < 1e-9,
            f"azimuth 180 is south, +Z -- the road side ({tuple(round(v, 4) for v in south)})")
    require(abs(zenith.y - 1.0) < 1e-9, "altitude 90 is straight up")
    require(all(abs(sun_direction(a, z).length - 1.0) < 1e-6
                for a in (0, 30, 90) for z in (0, 137, 300)),
            "every direction is a unit vector (to single precision -- mathutils is float32, and "
            "a 1e-9 tolerance here is below its noise floor rather than a real check)")

    # 1b. The IMPORT lands in the same axes those directions are in.
    #
    #     Found 2026-09-11 by `HOUSE-01279`, the first run of this tool against the real house:
    #     glTF is Y-up, Blender is Z-up, and `import_scene.gltf` converts on the way in. Every
    #     other coordinate here is §14's, so an unconverted import puts the rays and the geometry
    #     in different frames -- rays that should go UP go NORTH, and a ground-floor window is
    #     reported as shaded by the BASEMENT. The file looks entirely plausible.
    #
    #     Every claim above this one builds its geometry directly in §14's axes and so could never
    #     see it. This one exports a box whose dimensions name their own axis and imports it back
    #     through `import_gltf`, which is the only way to catch it.
    import tempfile

    with tempfile.TemporaryDirectory() as temporary:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        marker = bpy.data.meshes.new("marker")
        # A tall thin slab, 1 m x 2 m in plan and 8 m tall, standing on the ground. Every extent is
        # different, so the box says which axis it landed on and no symmetry can hide a swap.
        #
        # **Built in BLENDER's axes, because that is what the exporter reads.** `export_scene.gltf`
        # converts Z-up to Y-up on the way out, so "8 m tall" here is 8 m along Blender's Z; what
        # comes back through `import_shell` is then in §14's, where tall is Y. The two conversions
        # are what this claim is about and stating them the wrong way round is how it was first
        # written -- it failed, and the failure was the test's rather than the tool's.
        marker.from_pydata([(x, y, z) for x in (-0.5, 0.5) for y in (-1.0, 1.0) for z in (0.0, 8.0)],
                           [], [(0, 1, 3, 2), (4, 5, 7, 6), (0, 1, 5, 4),
                                (2, 3, 7, 6), (0, 2, 6, 4), (1, 3, 7, 5)])
        marker.update()
        bpy.context.scene.collection.objects.link(bpy.data.objects.new("marker", marker))
        path = os.path.join(temporary, "marker.glb")
        bpy.ops.export_scene.gltf(filepath=path, export_format="GLB")

        bpy.ops.wm.read_factory_settings(use_empty=True)
        import_shell(path)
        low = [1e9, 1e9, 1e9]
        high = [-1e9, -1e9, -1e9]
        for obj in bpy.context.scene.objects:
            if obj.type != "MESH":
                continue
            for vertex in obj.data.vertices:
                world = obj.matrix_world @ vertex.co
                for axis in range(3):
                    low[axis] = min(low[axis], world[axis])
                    high[axis] = max(high[axis], world[axis])
        extents = [high[axis] - low[axis] for axis in range(3)]
        require(abs(extents[1] - 8.0) < 1e-3,
                f"an imported .glb keeps §14's UP on Y ({tuple(round(e, 3) for e in extents)}) -- "
                f"Blender's importer converts Y-up to Z-up and every ray in this tool is §14's")
        require(abs(extents[0] - 1.0) < 1e-3 and abs(extents[2] - 2.0) < 1e-3,
                f"...and X and Z with it, so nothing was merely rotated into place by luck "
                f"({tuple(round(e, 3) for e in extents)})")
        require(abs(low[1]) < 1e-3,
                f"the slab still stands ON the ground rather than under it (y from {low[1]:.3f})")

    # 2. The grid is NODES: the horizon and the zenith are measured, and the azimuth wraps.
    require(altitude_of(0) == 0.0 and altitude_of(ALTITUDE_STEPS - 1) == 90.0,
            f"altitude spans 0 to 90 inclusive over {ALTITUDE_STEPS} nodes, so the horizon and "
            f"the zenith are sampled rather than extrapolated")
    require(azimuth_of(0) == 0.0 and azimuth_of(AZIMUTH_STEPS) == 360.0,
            "azimuth spans a full turn, and 360 is not stored because it is 0")
    require(ALTITUDE_STEPS * AZIMUTH_STEPS == 288,
            f"288 samples per window, which is §22's figure "
            f"({ALTITUDE_STEPS} x {AZIMUTH_STEPS})")

    windows = build_eave_fixture()
    bvh = build_bvh()
    window = windows[0]
    grid = bake_window(bvh, window, DEFAULT_SAMPLES)
    require(len(grid) == 288, f"the grid is 288 bytes ({len(grid)})")
    require(all(0 <= v <= 255 for v in grid), "every sample is one byte")

    # 3. A sun behind the wall never reaches the window, whatever the geometry does.
    behind = [at(grid, a, z) for a in range(ALTITUDE_STEPS)
              for z in range(AZIMUTH_STEPS)
              if sun_direction(altitude_of(a), azimuth_of(z)).z <= 0]
    require(behind and max(behind) == 0.0,
            f"every sample with the sun behind the wall reads 0 ({len(behind)} of them)")

    # 4. THE EAVE. §22's "the porch roof keeps the sun out in the afternoon": due south, a high sun
    #    is cut off by the eave and a low one gets under it. That ordering is the whole feature.
    south_index = AZIMUTH_STEPS // 2
    require(abs(azimuth_of(south_index) - 180.0) < 1e-9, "azimuth node 12 is due south")
    low = at(grid, 1, south_index)
    high = at(grid, ALTITUDE_STEPS - 2, south_index)
    require(low > 0.5,
            f"a LOW southern sun reaches most of the window ({low:.3f}) -- it passes under the "
            f"eave")
    require(high < 0.05,
            f"a HIGH southern sun reaches almost none of it ({high:.3f}) -- the eave is in the "
            f"way, which is exactly the porch-roof effect §22 asks for")
    require(low > high * 5,
            f"...and the ordering is the point: {low:.3f} low against {high:.3f} high")

    # 5. PARTIAL shading, which a single centre ray cannot express. Somewhere between the two the
    #    eave covers part of the window and the factor must be strictly between 0 and 1.
    column = [at(grid, a, south_index) for a in range(ALTITUDE_STEPS)]
    partial = [v for v in column if 0.02 < v < 0.98]
    require(partial,
            f"at least one altitude reads a fraction rather than 0 or 1 ({[round(v, 2) for v in column]}) "
            f"-- a single centre ray would step from 1 to 0 and the foyer's daylight would jump")
    require(column == sorted(column, reverse=True),
            f"...and the column falls monotonically as the sun rises, because an eave that shades "
            f"at one altitude shades at every higher one ({[round(v, 2) for v in column]})")

    # 6. THE NEIGHBOUR'S GABLE. §22's "the west neighbour's gable shades the study at sunset".
    #    Compared south-WEST against south-EAST, not west against east: due west is exactly
    #    tangent to a south-facing window, so both sides of that comparison are about the wall
    #    rather than about the neighbour, and the first version of this claim measured the eave.
    southwest = AZIMUTH_STEPS * 5 // 8
    southeast = AZIMUTH_STEPS * 3 // 8
    require(abs(azimuth_of(southwest) - 225.0) < 1e-9
            and abs(azimuth_of(southeast) - 135.0) < 1e-9,
            "azimuth nodes 15 and 9 are south-west and south-east")
    low_altitude = 2
    blocked = at(grid, low_altitude, southwest)
    open_side = at(grid, low_altitude, southeast)
    require(open_side > 0.9,
            f"the south-EAST is open at this altitude ({open_side:.3f}), so the comparison below "
            f"is about the neighbour and not about the eave or the wall")
    require(blocked < 0.1,
            f"...and the south-WEST is shut by the neighbour's gable ({blocked:.3f}) -- §22's "
            f"study, shaded at sunset")
    # And it IS the neighbour: the same scene without it opens the same node. Proved by removing
    # the gable rather than by comparing altitudes, because the eave shades the south-west too and
    # an altitude comparison cannot say which of the two did it.
    build_eave_fixture(with_neighbour=False)
    without = bake_window(build_bvh(), window, DEFAULT_SAMPLES)
    require(at(without, low_altitude, southwest) > 0.9,
            f"delete the gable and the same node opens ({at(without, low_altitude, southwest):.3f} "
            f"against {blocked:.3f}) -- so it is the neighbour shading the study, not the eave")
    require(at(without, low_altitude, southeast) == open_side,
            "...and the south-east is untouched by its removal, as a neighbour to the west "
            "should be")
    build_eave_fixture()
    bvh = build_bvh()

    # 7. Sampling the rectangle, not the centre. One sample per axis is the centre ray, and it
    #    must lose the partial values the 4x4 grid finds.
    coarse = bake_window(bvh, window, 1)
    coarse_column = [coarse[a * AZIMUTH_STEPS + south_index] / 255.0
                     for a in range(ALTITUDE_STEPS)]
    require(all(v in (0.0, 1.0) for v in coarse_column),
            f"a single centre ray gives only 0 or 1 ({[round(v, 2) for v in coarse_column]})")
    require(any(0.02 < v < 0.98 for v in column),
            "...where the 4x4 grid reports the fraction, which is what §22's multiply needs")

    # 8. The ray epsilon. Starting exactly on the glass lets a ray hit the wall it left.
    points = window_samples(window, 2)
    require(all(abs(p.z - 2.0 - RAY_EPSILON) < 1e-5 for p in points),
            f"every sample sits {RAY_EPSILON} m off the glass along the outward normal, so no "
            f"ray can hit the face it started from")
    require(all(-1.0 <= p.x <= 1.0 and 0.9 <= p.y <= 2.1 for p in points),
            "and inside the window's own rectangle, not beyond its jambs")
    leafed = dict(window)
    leafed["leafThickness"] = 0.03
    leafed_points = window_samples(leafed, 2)
    require(all(abs(p.z - 2.0 - 0.015 - RAY_EPSILON) < 1e-5 for p in leafed_points),
            f"a window with a 0.03 m leaf starts its rays {0.015 + RAY_EPSILON} m out, CLEAR of "
            f"the sash -- the shell draws the leaf in the opening, and a sample inside it makes "
            f"the window shade itself completely (`HOUSE-01279`)")
    # The glazed aperture. `HOUSE-01279`: samples used to cover the whole portal rectangle, which
    # the shell fills with a solid frame and sash, so on a small window half of them sat inside
    # geometry and contributed a permanent zero to all 288 nodes.
    narrow = dict(window)
    narrow["across"] = (0.28, 0.0, 0.0)
    narrow["up"] = (0.0, 2.03, 0.0)
    narrow_points = window_samples(narrow, 4)
    narrow_u = [p.x - window["origin"][0] for p in narrow_points]
    require(all(GLAZING_INSET < u < 0.28 - GLAZING_INSET for u in narrow_u),
            f"every sample of a 0.28 m `W_SIDELIGHT` lands on GLASS, inside the "
            f"{GLAZING_INSET:.3f} m frame and sash the shell draws ({min(narrow_u):.3f}..."
            f"{max(narrow_u):.3f} m across a 0.28 m opening)")
    wide_points = window_samples(window, 4)
    wide_u = sorted(p.x - window["origin"][0] for p in wide_points)
    require(wide_u[0] > GLAZING_INSET and wide_u[-1] < window["across"][0] - GLAZING_INSET,
            "...and so does every sample of a wide one, which is why this bias was invisible on "
            "the 36 double-hungs and total on the sidelights")
    require(GLAZING_INSET > 0.0,
            f"the inset is {GLAZING_INSET:.3f} m and comes from `house_shell_gen`'s own "
            f"FRAME_SECTION + SASH_SECTION rather than a second copy of the numbers")
    require(0.015 + RAY_EPSILON < 0.15 / 2,
            "...and that is still inside the thinnest wall's half-thickness, so it cannot push a "
            "sample out through the far face of the wall it is measuring")
    require(RAY_EPSILON < 0.15 / 10,
            f"the offset is {RAY_EPSILON} m, an order below the thinnest wall the layout can "
            f"build (0.15 m), so it cannot push a sample through the geometry it is measuring")

    # 9. Outward is decided by the geometry, not by cellA/cellB order.
    cells = {"L0_STUDY": {"id": "L0_STUDY", "kind": "room",
                          "boxes": [{"x": [-6.0, 6.0], "z": [-2.0, 2.0]}]},
             "EXT": {"id": "EXT", "kind": "exterior",
                     "boxes": [{"x": [-20.0, 20.0], "z": [2.0, 20.0]}]}}
    portal = {"id": "P", "cellA": "L0_STUDY", "cellB": "EXT",
              "plane": {"axis": "z", "value": 2.0},
              "rect": {"u": [-1.0, 1.0], "v": [0.9, 2.1]}}
    forward = window_frame("W", portal, cells)
    reversed_portal = dict(portal, cellA="EXT", cellB="L0_STUDY")
    backward = window_frame("W", reversed_portal, cells)
    require(forward["normal"] == (0.0, 0.0, 1.0),
            f"the outward normal points away from the room ({forward['normal']})")
    require(backward["normal"] == forward["normal"],
            f"...and is the same when the portal names its cells the other way round "
            f"({backward['normal']}) -- a window authored in reverse would otherwise cast every "
            f"ray into the room it is trying to light")
    require(forward["cell"] == "L0_STUDY" and backward["cell"] == "L0_STUDY",
            "and the interior cell is identified as the one that is not exterior, either way")

    # 10. One byte per sample. The stored byte is checked against the fraction measured here,
    #     not against a fraction recomputed the same way the tool computes it -- with 16 samples
    #     the fractions are sixteenths, and 3/16 rounds to 48 but truncates to 47.
    points = window_samples(window, DEFAULT_SAMPLES)
    mismatches = []
    exacts = []
    for altitude_index in range(ALTITUDE_STEPS):
        for azimuth_index in (south_index, southeast, southwest):
            direction = sun_direction(altitude_of(altitude_index), azimuth_of(azimuth_index))
            if direction.dot(Vector(window["normal"])) <= 0.0:
                continue
            exact = shading_factor(bvh, points, direction)
            exacts.append(exact)
            stored = grid[altitude_index * AZIMUTH_STEPS + azimuth_index]
            if stored != round(exact * 255):
                mismatches.append((altitude_index, azimuth_index, exact, stored))
    require(not mismatches,
            f"every stored byte is the measured fraction ROUNDED to 255ths, not truncated "
            f"({mismatches[:3]})")
    # The EXACT fractions, not the stored bytes -- a stored byte times 255 is an integer by
    # construction and would report that rounding never matters.
    ambiguous = [v for v in exacts if abs(v * 255 - round(v * 255)) > 0.2]
    require(ambiguous,
            f"...and at least one measured fraction lands where rounding and truncation disagree "
            f"({[round(v, 4) for v in ambiguous[:3]]} of {len(exacts)} nodes), so the claim above "
            f"can tell the two apart")
    require(max(abs(at(grid, a, z) - round(at(grid, a, z) * 255) / 255.0)
                for a in range(ALTITUDE_STEPS) for z in range(AZIMUTH_STEPS))
            <= 0.5 / 255.0 + 1e-6,
            f"and the quantum costs at most {0.5 / 255:.5f}, well under what an 8-bit daylight "
            f"response can show")

    # 11. Round trip and determinism.
    grids = {window["id"]: grid}
    data = serialise(windows, grids, DEFAULT_SAMPLES)
    require(len(data) == 4 + 8 + 12 + 4 + (2 + len("W_STUDY_S1")) + (2 + len("L0_STUDY"))
            + 12 + 288,
            f"the file is header + one window + 288 bytes ({len(data)} bytes); §22's 81 windows "
            f"would be about {81 * 288 / 1024:.0f} KB of grid, against its 23 KB estimate")
    back = read_back(data)
    require(back["altitudes"] == ALTITUDE_STEPS and back["azimuths"] == AZIMUTH_STEPS
            and back["samples"] == DEFAULT_SAMPLES,
            "the grid shape and the sample count travel with the file")
    require(back["windows"][0]["grid"] == grid,
            "every one of the 288 bytes round trips")
    require(back["windows"][0]["id"] == "W_STUDY_S1"
            and back["windows"][0]["cell"] == "L0_STUDY",
            "the window and its cell round trip")
    require(tuple(round(v, 6) for v in back["windows"][0]["normal"]) == (0.0, 0.0, 1.0),
            "so does the outward normal, which the runtime needs for §22's +-75 degree test")
    require(bake_window(bvh, window, DEFAULT_SAMPLES) == grid,
            "two bakes of one scene give the same grid -- ray casting, no sampling noise")

    # 12. Truncation, version, flags and trailing bytes.
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
    for offset, value, what in ((4, 3, "version"), (8, 0x8, "flag bit")):
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
    print("shading_factor: selftest passed.")
    return 0


def main() -> int:
    argv = blender_argv()
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
        print("shading_factor: usage: SHELL --world DIR --out DIR "
              "[--neighbourhood DIR] [--samples N]",
              file=sys.stderr)
        return 2

    bpy.ops.wm.read_factory_settings(use_empty=True)
    imported = import_shell(positional[0])
    report(f"shell: {imported} file(s)")
    if "neighbourhood" in options:
        placed = import_neighbourhood(options["world"], options["neighbourhood"])
        report(f"neighbourhood: {placed} placement(s)")
    windows = windows_from_layout(options["world"])
    if not windows:
        print("shading_factor: layout.openings.json declares no window", file=sys.stderr)
        return 1
    samples = int(options.get("samples", DEFAULT_SAMPLES))
    bvh = build_bvh()
    grids = {}
    for window in windows:
        grids[window["id"]] = bake_window(bvh, window, samples)
        lit = sum(1 for v in grids[window["id"]] if v > 0)
        report(f"{window['id']}: {lit} of 288 nodes see the sun")
    os.makedirs(options["out"], exist_ok=True)
    path = os.path.join(options["out"], "shading.bin")
    with open(path, "wb") as handle:
        handle.write(serialise(windows, grids, samples))
    report(f"wrote {path} ({len(windows)} window(s), {os.path.getsize(path)} bytes)")
    return 0


def blender_argv() -> list[str]:
    return sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


if __name__ == "__main__":
    # Blender does not propagate a script's exit status; see tools/blender/blender_env.py.
    try:
        _status = main()
    except SystemExit as _exit:
        _status = int(_exit.code or 0) if not isinstance(_exit.code, str) else 1
        if isinstance(_exit.code, str):
            print(_exit.code, file=sys.stderr)
    except BaseException:  # noqa: BLE001
        import traceback

        traceback.print_exc()
        print("shading_factor: EXIT 1")
        raise
    print(f"shading_factor: EXIT {_status}")
    sys.exit(_status)
