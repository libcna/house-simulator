#!/usr/bin/env python3
"""build_snowshell.py -- the surfaces snow settles on, and only those.

`HOUSE-00214`. `cna-house.md` §38: accumulation is drawn as a **snow shell**, "an offline-generated
duplicate of every up-facing exterior surface (terrain tiles, roofs, the porch and balcony decks,
fence rails, garden furniture tops, the car's roof and bonnet), offset along the surface normal by
`snowDepth` and drawn with a white snow material at `alpha = smoothstep(0.002, 0.03, snowDepth)`.
Faces steeper than each material's `snowResponse.slopeLimitDeg` are excluded from the shell at
generation time, so snow does not cling to walls."

    tools/world/build_snowshell.py --world assets-src/world --out content/world/snowshell.bin
    tools/world/build_snowshell.py --world assets-src/world --report
    tools/world/build_snowshell.py --selftest

## §38 asks for two things that cannot both be true of a static buffer

"Offset along the surface normal by `snowDepth`" and "it needs no shader" are compatible only while
`snowDepth` is fixed, and it is not: it integrates from 0 to 0.35 m and back. Baking one offset in
picks a depth and is wrong at every other one -- at 0.35 m the shell floats a hand's width above
the roof while the alpha says it is barely there.

So this file carries **base positions and normals, unoffset**, and the runtime does
`p + n · snowDepth` where it knows the depth. That is one multiply-add per vertex on a buffer it
rewrites only when the depth has moved: `dD/dt = 0.0009 · intensity` means a full 0 → 0.35 m takes
about 390 s, so a 1 cm rebuild threshold fires roughly every 11 s in the heaviest snowfall. Tier E
does the same sum in its vertex shader from the same two attributes and rebuilds nothing.

## The normal it carries is the FACE normal, not the model's

The slope test is a test of a face, and a vertex is only in this file because the face it belongs
to passed. Offsetting that vertex along a *smoothed* vertex normal would push it off the surface it
was accepted for, and at a hard edge -- the lip of a table, the ridge of a roof -- two faces would
pull the shared vertex in a compromise direction and open a gap along the crease at every depth.
So the shell is flat-shaded by construction: the face normal is written to all three of its
vertices, and vertices are welded on **(position, face normal)**, which keeps one vertex per smooth
join and splits every hard edge exactly where it should split. It also means a source model's
authored normals -- which may be smoothed, or simply wrong -- cannot move snow off a roof.

## The slope test is against +Y, not against |Y|

A deck's **underside** is as flat as its top and points the other way. Testing `abs(dot(n, up))`
would put a snow shell under the balcony, hanging in the air, which is the one artefact this whole
technique exists to make impossible. The angle is measured from +Y and a downward face is 180 deg
away from it -- excluded by the same comparison that excludes a wall at 90.

## What is up-facing and exterior, today

Every source §38 lists is a cell or a prop, and this reads both: the top faces of **open cells**
(`kind: exterior` or `visibilityHint: open` -- the porch, the terrace, the balcony decks) and the
up-facing faces of the **static props standing in them** (garden furniture, the car). Two of §38's
sources are not available yet and are reported rather than quietly missing: the **roof** arrives
with `HOUSE-00470`'s shell, and the **terrain tiles** with `layout.exterior.json`'s height field.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
REPO = TOOLS.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(TOOLS / "assets"))

import build_chunks as bch  # noqa: E402
import build_collision as bc  # noqa: E402
import layout_io  # noqa: E402
from layout_io import LayoutError  # noqa: E402

MAGIC = b"CSNW"
VERSION = 1

#: §38's accumulation cap. Written into the file so the runtime's offset and this generation agree
#: about the range the shell was built for.
MAX_DEPTH = 0.35

#: The default when a material has no `snowResponse`. 40 degrees is §22.1's own example, and the
#: default is stated here rather than assumed: a material that forgot the field gets a documented
#: number and is named in the report, not a silent 90 that snows on walls.
DEFAULT_SLOPE_LIMIT_DEG = 40.0

#: A `u16` vertex index addresses this many; beyond it a shell takes 32-bit indices, as
#: `chunk-format.md` §6 does for the same reason.
MAX_VERTICES_16BIT = 0xFFFF

EPS = 1e-9


def snow_response(material: dict) -> tuple[bool, float]:
    """`(coverable, slopeLimitDeg)` for a material, with §22.1's documented default."""
    response = material.get("snowResponse")
    if not isinstance(response, dict):
        return True, DEFAULT_SLOPE_LIMIT_DEG
    coverable = response.get("coverable")
    limit = response.get("slopeLimitDeg")
    return (True if coverable is None else bool(coverable),
            DEFAULT_SLOPE_LIMIT_DEG if limit is None else float(limit))


def slope_degrees(normal) -> float:
    """The angle between a face normal and **+Y**, in degrees. 0 is flat up, 180 is flat down.

    Measured from +Y and not from the Y axis: a deck's underside is as flat as its top and points
    the other way, and `abs(dot(n, up))` would hang a snow shell under the balcony. This is the
    comparison that makes "snow does not cling to walls" and "snow does not cling to soffits" the
    same rule.
    """
    length = math.sqrt(sum(c * c for c in normal))
    if length < EPS:
        return 180.0
    return math.degrees(math.acos(max(-1.0, min(1.0, normal[1] / length))))


def triangle_normal(a, b, c):
    """The geometric normal, from the winding. Zero-length for a degenerate triangle."""
    u = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    v = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
    return (u[1] * v[2] - u[2] * v[1],
            u[2] * v[0] - u[0] * v[2],
            u[0] * v[1] - u[1] * v[0])


def triangle_area(a, b, c) -> float:
    n = triangle_normal(a, b, c)
    return 0.5 * math.sqrt(sum(k * k for k in n))


# ================================================================================== surface source


def open_cell_decks(layout, levels):
    """The top face of every open cell's floor: the porch, the terrace, the balcony decks.

    Two triangles per footprint box, wound so the geometric normal comes out +Y. Deriving the
    normal from the winding rather than asserting it keeps one rule for authored boxes and for
    imported prop geometry -- and it caught this function's own first version, whose corners were
    listed in the box's reading order and whose deck was therefore rejected as a soffit.
    """
    out = []
    for cell in sorted(layout_io.rows(layout, "cells"), key=lambda c: c["id"]):
        if not (cell.get("kind") == "exterior" or cell.get("visibilityHint") == "open"):
            continue
        level = levels.get(cell["level"])
        if level is None:
            raise LayoutError(f"cell {cell['id']!r} names level {cell['level']!r}, "
                              f"which does not exist")
        floor = layout_io.cell_extent(cell, level)[0]
        for x0, x1, z0, z1 in layout_io.cell_boxes(cell):
            corners = [(x0, floor, z0), (x1, floor, z0), (x1, floor, z1), (x0, floor, z1)]
            uvs = [(x0, z0), (x1, z0), (x1, z1), (x0, z1)]
            out.append({
                "cell": cell["id"],
                "material": cell.get("floorMaterial") or cell.get("wallMaterial"),
                "source": cell["id"],
                "positions": corners,
                "uv0": uvs,
                # Wound so the geometric normal is +Y. The first version listed the corners in
                # the reading order of the box -- x0z0, x1z0, x1z1, x0z1 -- whose cross product
                # points DOWN, and the deck was then rejected as a soffit by its own slope test.
                "triangles": [(0, 2, 1), (0, 3, 2)],
            })
    return out


def open_cell_props(layout, cells_by_id, asset_paths, cache):
    """Every static prop standing in an open cell, in world space."""
    out = []
    for prop in sorted(layout_io.rows(layout, "props"), key=lambda p: p["id"]):
        if not prop.get("static", True):
            continue
        cell = cells_by_id.get(prop["cell"])
        if cell is None:
            raise LayoutError(f"prop {prop['id']!r} names cell {prop['cell']!r}, "
                              f"which does not exist")
        if not (cell.get("kind") == "exterior" or cell.get("visibilityHint") == "open"):
            continue
        path = asset_paths.get(prop["asset"])
        if path is None:
            raise LayoutError(
                f"prop {prop['id']!r} names asset {prop['asset']!r}, not in assets.manifest.json")
        if prop["asset"] not in cache:
            cache[prop["asset"]] = bch.read_geometry(path)
        placed = bch.place(cache[prop["asset"]], [float(c) for c in prop["position"]],
                           float(prop.get("yawDeg", 0.0)), float(prop.get("scale", 1.0)))
        out.append({
            "cell": prop["cell"],
            "material": prop.get("material") or cell.get("floorMaterial"),
            "source": prop["id"],
            "positions": placed["positions"],
            "uv0": placed["uv0"],
            "triangles": placed["triangles"],
        })
    return out


# ========================================================================================== build


def build(world_dir: Path, manifest_path: Path | None = None) -> dict:
    layout = layout_io.load_layout(world_dir, ["levels", "cells", "materials"])
    for optional in ("props", "exterior"):
        name, _ = layout_io.FILES[optional]
        if (world_dir / name).is_file():
            layout[optional] = layout_io.load_file(world_dir / name, optional)

    levels = layout_io.by_id(layout_io.rows(layout, "levels"), "level")
    cells_by_id = layout_io.by_id(layout_io.rows(layout, "cells"), "cell")
    materials = layout_io.by_id(layout_io.rows(layout, "materials"), "material")

    asset_paths: dict[str, Path] = {}
    if manifest_path and manifest_path.is_file():
        for row in layout_io.load_file(manifest_path, "assets").get("assets", []):
            if row.get("sourceFile"):
                asset_paths[row["id"]] = REPO / row["sourceFile"]

    stats = {"facesConsidered": 0, "facesKept": 0, "rejectedSteep": 0, "rejectedDown": 0,
             "rejectedDegenerate": 0, "rejectedNotCoverable": 0, "defaultedSlopeLimit": [],
             "missingSources": []}
    if "exterior" not in layout:
        stats["missingSources"].append(
            "terrain tiles (layout.exterior.json's height field is not authored yet)")
    stats["missingSources"].append(
        "roofs (the shell arrives with HOUSE-00470; §38 lists them and they are not here yet)")

    surfaces = open_cell_decks(layout, levels)
    surfaces += open_cell_props(layout, cells_by_id, asset_paths, {})

    groups: dict[tuple[str, str], dict] = {}
    for surface in surfaces:
        material_id = surface["material"]
        material = materials.get(material_id)
        if material is None:
            raise LayoutError(
                f"{surface['source']} resolves to material {material_id!r}, which does not exist")
        coverable, limit = snow_response(material)
        if not isinstance(material.get("snowResponse"), dict) or \
                material["snowResponse"].get("slopeLimitDeg") is None:
            if material_id not in stats["defaultedSlopeLimit"]:
                stats["defaultedSlopeLimit"].append(material_id)

        for a, b, c in surface["triangles"]:
            stats["facesConsidered"] += 1
            points = (surface["positions"][a], surface["positions"][b], surface["positions"][c])
            if triangle_area(*points) < 1e-9:
                stats["rejectedDegenerate"] += 1
                continue
            if not coverable:
                stats["rejectedNotCoverable"] += 1
                continue
            raw = triangle_normal(*points)
            slope = slope_degrees(raw)
            if slope > 90.0:
                stats["rejectedDown"] += 1
                continue
            if slope > limit:
                stats["rejectedSteep"] += 1
                continue
            stats["facesKept"] += 1
            length = math.sqrt(sum(k * k for k in raw))
            face_normal = tuple(k / length for k in raw)
            group = groups.setdefault((surface["cell"], material_id),
                                      {"vertices": [], "index": {}, "triangles": [],
                                       "sources": []})
            indices = []
            for corner in (a, b, c):
                key = (tuple(round(v, 6) for v in surface["positions"][corner]),
                       tuple(round(v, 6) for v in face_normal))
                if key not in group["index"]:
                    group["index"][key] = len(group["vertices"])
                    group["vertices"].append((surface["positions"][corner], face_normal,
                                              surface["uv0"][corner]))
                indices.append(group["index"][key])
            group["triangles"].append(tuple(indices))
            if surface["source"] not in group["sources"]:
                group["sources"].append(surface["source"])

    shells = []
    for (cell_id, material_id) in sorted(groups):
        group = groups[(cell_id, material_id)]
        if not group["triangles"]:
            continue
        positions = [v[0] for v in group["vertices"]]
        area = sum(triangle_area(positions[a], positions[b], positions[c])
                   for a, b, c in group["triangles"])
        shells.append({
            "cell": cell_id, "material": material_id,
            "vertices": group["vertices"], "triangles": group["triangles"],
            "sources": group["sources"],
            "bounds": bch.bounds_of(positions),
            "area": area,
            "indexBits": 32 if len(group["vertices"]) > MAX_VERTICES_16BIT else 16,
        })
    stats["shells"] = len(shells)
    stats["area"] = sum(s["area"] for s in shells)
    return {"shells": shells, "stats": stats, "worldHash": bc._world_hash(world_dir)}


# ========================================================================================= writer


def serialise(built: dict) -> bytes:
    shells = built["shells"]
    cells = sorted({s["cell"] for s in shells})
    materials = sorted({s["material"] for s in shells})
    cell_index = {name: i for i, name in enumerate(cells)}
    material_index = {name: i for i, name in enumerate(materials)}

    out = bytearray()
    out += MAGIC
    out += struct.pack("<II", VERSION, 0)
    out += bc._string(built["worldHash"])
    out += struct.pack("<f", MAX_DEPTH)
    out += struct.pack("<I", len(cells))
    for name in cells:
        out += bc._string(name)
    out += struct.pack("<I", len(materials))
    for name in materials:
        out += bc._string(name)

    out += struct.pack("<I", len(shells))
    for shell in shells:
        out += struct.pack("<HH", cell_index[shell["cell"]], material_index[shell["material"]])
        out += struct.pack("<B", 1 if shell["indexBits"] == 32 else 0)
        out += struct.pack("<6f", *shell["bounds"])
        out += struct.pack("<f", shell["area"])
        out += struct.pack("<I", len(shell["vertices"]))
        for position, normal, uv in shell["vertices"]:
            out += struct.pack("<3f", *position)
            out += struct.pack("<3f", *normal)
            out += struct.pack("<2f", *uv)
        out += struct.pack("<I", len(shell["triangles"]) * 3)
        code = "<I" if shell["indexBits"] == 32 else "<H"
        for triangle in shell["triangles"]:
            for index in triangle:
                out += struct.pack(code, index)
    return bytes(out)


def read_back(data: bytes) -> dict:
    view = memoryview(data)
    at = 0

    def take(n):
        nonlocal at
        chunk = view[at:at + n]
        if len(chunk) != n:
            raise LayoutError(f"snowshell.bin truncated at byte {at}, wanted {n} more")
        at += n
        return chunk

    def unpack(fmt):
        return struct.unpack(fmt, take(struct.calcsize(fmt)))

    def text():
        (length,) = unpack("<H")
        return bytes(take(length)).decode("utf-8")

    if bytes(take(4)) != MAGIC:
        raise LayoutError("not a snowshell.bin: bad magic")
    version, flags = unpack("<II")
    if version != VERSION:
        raise LayoutError(f"snowshell.bin is version {version}; this reader knows {VERSION}")
    if flags:
        raise LayoutError(f"snowshell.bin sets unknown flag bits {flags:#x}")
    world_hash = text()
    (max_depth,) = unpack("<f")
    (cell_count,) = unpack("<I")
    cells = [text() for _ in range(cell_count)]
    (material_count,) = unpack("<I")
    materials = [text() for _ in range(material_count)]

    (shell_count,) = unpack("<I")
    shells = []
    for _ in range(shell_count):
        cell, material = unpack("<HH")
        (wide,) = unpack("<B")
        bounds = unpack("<6f")
        (area,) = unpack("<f")
        (vertex_count,) = unpack("<I")
        vertices = []
        for _ in range(vertex_count):
            vertices.append((unpack("<3f"), unpack("<3f"), unpack("<2f")))
        (index_count,) = unpack("<I")
        code = "<I" if wide else "<H"
        flat = [unpack(code)[0] for _ in range(index_count)]
        shells.append({"cell": cells[cell], "material": materials[material],
                       "indexBits": 32 if wide else 16, "bounds": bounds, "area": area,
                       "vertices": vertices,
                       "triangles": [tuple(flat[i:i + 3]) for i in range(0, len(flat), 3)]})
    if at != len(data):
        raise LayoutError(f"{len(data) - at} bytes left over after the last shell")
    return {"worldHash": world_hash, "maxDepth": max_depth, "cells": cells,
            "materials": materials, "shells": shells}


def report(built: dict) -> str:
    stats = built["stats"]
    lines = [
        f"{stats['shells']} snow shells, {stats['area']:.1f} m2 of covered surface "
        f"(§38 estimates about 18 chunks)",
        f"  {stats['facesKept']} of {stats['facesConsidered']} faces kept; rejected: "
        f"{stats['rejectedSteep']} too steep, {stats['rejectedDown']} facing down, "
        f"{stats['rejectedNotCoverable']} not coverable, "
        f"{stats['rejectedDegenerate']} degenerate",
    ]
    for shell in built["shells"]:
        lines.append(f"  {shell['cell']:<18} {shell['material']:<22} "
                     f"{shell['area']:7.2f} m2  {len(shell['vertices']):>5} vertices  "
                     f"from {', '.join(shell['sources'][:3])}")
    if stats["defaultedSlopeLimit"]:
        lines.append(f"  note: no snowResponse.slopeLimitDeg on "
                     f"{', '.join(stats['defaultedSlopeLimit'])}; used the "
                     f"{DEFAULT_SLOPE_LIMIT_DEG} degree default")
    for missing in stats["missingSources"]:
        lines.append(f"  not yet available: {missing}")
    return "\n".join(lines)


# ======================================================================================= selftest


def selftest() -> int:
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("build_snowshell: selftest")
    workspace = Path(tempfile.mkdtemp(prefix="build_snowshell_selftest_"))
    try:
        # 1. The slope test, measured from +Y. This is the rule §38 turns on.
        require(abs(slope_degrees((0.0, 1.0, 0.0))) < 1e-9, "a flat up-facing normal is 0 degrees")
        require(abs(slope_degrees((1.0, 0.0, 0.0)) - 90.0) < 1e-9, "a wall is 90")
        require(abs(slope_degrees((0.0, -1.0, 0.0)) - 180.0) < 1e-9,
                "a flat DOWN-facing normal is 180, not 0 -- measured from +Y, so a deck's "
                "underside is as far from snow as it is possible to be")
        require(abs(slope_degrees((1.0, 1.0, 0.0)) - 45.0) < 1e-9, "a 45-degree pitch is 45")
        require(slope_degrees((0.0, 0.0, 0.0)) == 180.0,
                "a degenerate normal is treated as facing down, so it is excluded rather than "
                "defaulting to flat and snowing on nothing")

        # 2. `snowResponse`, and its default.
        require(snow_response({"snowResponse": {"coverable": True, "slopeLimitDeg": 40}})
                == (True, 40.0), "§22.1's own example reads back as itself")
        require(snow_response({}) == (True, DEFAULT_SLOPE_LIMIT_DEG),
                f"a material with no snowResponse gets the documented "
                f"{DEFAULT_SLOPE_LIMIT_DEG}-degree default, not a silent 90 that snows on walls")
        require(snow_response({"snowResponse": {"coverable": False}})[0] is False,
                "coverable: false means no shell at all -- glass and water")

        # 3. A fixture with one open terrace, one indoor room, and props on both.
        world_dir = workspace / "world"
        world_dir.mkdir()
        bc._fixture_world(world_dir)
        cells = json.loads((world_dir / "layout.cells.json").read_text())
        cells["cells"].append({
            "id": "L0_TERRACE", "level": "L0", "kind": "exterior",
            "boxes": [{"x": [-4.0, 0.0], "z": [0.0, 3.0]}],
            "floorMaterial": "MAT_PAVING", "footstepSurface": "concrete",
            "wallMaterial": "MAT_PAINT", "ceilingMaterial": "MAT_CEIL"})
        (world_dir / "layout.cells.json").write_text(
            json.dumps(cells, indent=2) + "\n", encoding="utf-8")
        (world_dir / "layout.materials.json").write_text(json.dumps({
            "schema": "cna-house/materials/1",
            "materials": [
                {"id": "MAT_PAVING", "class": "stone", "effectTierS": "DualTexture",
                 "snowResponse": {"coverable": True, "slopeLimitDeg": 40}},
                {"id": "MAT_TABLE", "class": "wood", "effectTierS": "DualTexture",
                 "snowResponse": {"coverable": True, "slopeLimitDeg": 40}},
                {"id": "MAT_PANE", "class": "glass", "effectTierS": "Basic",
                 "snowResponse": {"coverable": False, "slopeLimitDeg": 40}},
                {"id": "MAT_ROOFY", "class": "tile", "effectTierS": "DualTexture",
                 "snowResponse": {"coverable": True, "slopeLimitDeg": 55}},
                {"id": "MAT_PAINT", "class": "paint", "effectTierS": "DualTexture"},
            ]}, indent=2) + "\n", encoding="utf-8")

        assets = workspace / "assets"
        assets.mkdir()
        bch._fixture_model(assets / "table.glb", uv1=True)
        manifest = workspace / "assets.manifest.json"
        manifest.write_text(json.dumps({
            "schema": "cna-house/assets/1",
            "assets": [{"id": "MODEL_TABLE", "sourceFile": str(assets / "table.glb")}]},
            indent=2) + "\n", encoding="utf-8")
        (world_dir / "layout.props.json").write_text(json.dumps({
            "schema": "cna-house/props/1",
            "props": [
                {"id": "PROP_OUTDOOR_TABLE", "asset": "MODEL_TABLE", "cell": "L0_TERRACE",
                 "position": [-2.0, 0.0, 1.5], "yawDeg": 0.0, "scale": 1.0,
                 "static": True, "material": "MAT_TABLE"},
                {"id": "PROP_INDOOR_TABLE", "asset": "MODEL_TABLE", "cell": "L0_LOUNGE",
                 "position": [2.0, 0.0, 1.5], "yawDeg": 0.0, "scale": 1.0,
                 "static": True, "material": "MAT_TABLE"},
            ]}, indent=2) + "\n", encoding="utf-8")

        built = build(world_dir, manifest)
        sources = {s for shell in built["shells"] for s in shell["sources"]}
        require("L0_TERRACE" in sources,
                f"the terrace deck gets a shell ({sorted(sources)})")
        require("PROP_OUTDOOR_TABLE" in sources,
                "a garden table standing on it gets one too -- §38's 'garden furniture tops'")
        require("PROP_INDOOR_TABLE" not in sources,
                "the identical table INDOORS gets nothing: the surface has to be exterior, and "
                "cell kind is what says so")
        require(all(shell["cell"] == "L0_TERRACE" for shell in built["shells"]),
                "no shell is generated for any indoor cell")

        # 4. Only the UP faces of the prop, not the whole box. The fixture model is a closed box,
        #    so exactly one of its six sides can hold snow.
        require(built["stats"]["rejectedDown"] >= 2,
                f"the box's underside is rejected as facing down "
                f"({built['stats']['rejectedDown']} faces)")
        require(built["stats"]["rejectedSteep"] >= 8,
                f"and its four vertical sides as too steep "
                f"({built['stats']['rejectedSteep']} faces)")
        kept_normals = [v[1] for shell in built["shells"] for v in shell["vertices"]]
        require(all(v[1] > 0 for v in kept_normals),
                "every normal in every shell points upward, so nothing hangs under a balcony")

        # 5. The slope limit is the MATERIAL's, and two materials on one roof pitch disagree.
        #    A 45-degree face passes at 55 and fails at 40 -- the number is read, not assumed.
        pitched = {"positions": [(0.0, 0.0, 0.0), (0.0, 2.0, 2.0), (2.0, 0.0, 0.0)],
                   "uv0": [(0.0, 0.0)] * 3, "triangles": [(0, 1, 2)]}
        require(abs(slope_degrees(triangle_normal(*pitched["positions"])) - 45.0) < 1e-4,
                "the fixture pitch really is 45 degrees")
        require(slope_degrees(triangle_normal(*pitched["positions"])) > 40.0
                and slope_degrees(triangle_normal(*pitched["positions"])) < 55.0,
                "...so it is excluded at MAT_PAVING's 40 and kept at MAT_ROOFY's 55, which is "
                "what makes the limit a material property rather than a constant")

        # 6. `coverable: false` wins over any slope. A pane of glass lying flat holds no snow.
        (world_dir / "layout.cells.json").write_text(json.dumps(
            dict(cells, cells=[dict(c, floorMaterial="MAT_PANE") if c["id"] == "L0_TERRACE"
                               else c for c in cells["cells"]]), indent=2) + "\n",
            encoding="utf-8")
        glassy = build(world_dir, manifest)
        require(not any("L0_TERRACE" in s["sources"] and s["material"] == "MAT_PANE"
                        for s in glassy["shells"]),
                "a flat pane of glass gets no shell, because coverable is false")
        require(glassy["stats"]["rejectedNotCoverable"] >= 2,
                f"...and it is counted as such, not as too steep "
                f"({glassy['stats']['rejectedNotCoverable']})")
        (world_dir / "layout.cells.json").write_text(
            json.dumps(cells, indent=2) + "\n", encoding="utf-8")

        # 7. The shell carries POSITIONS AND NORMALS, unoffset. §38 asks for an offset "by
        #    snowDepth" and for the technique to need no shader; both hold only while snowDepth is
        #    fixed, and it integrates from 0 to 0.35 m. Baking one offset in picks a depth and is
        #    wrong at every other, so the runtime does `p + n * snowDepth` from these two.
        built = build(world_dir, manifest)
        deck = [s for s in built["shells"] if "L0_TERRACE" in s["sources"]][0]
        require(all(abs(v[0][1] - 0.0) < 1e-6 for v in deck["vertices"]),
                "the terrace shell sits exactly ON the deck at y = 0, with no offset baked in")
        require(all(abs(v[1][1] - 1.0) < 1e-6 for v in deck["vertices"]),
                "...carrying the +Y normal the runtime multiplies by snowDepth")

        # 8. The normal written is the FACE normal, not the model's. The fixture model's authored
        #    normals all point along +X -- it is `build_chunks`'s chiral box -- so a shell that
        #    copied them would offset the snow sideways off the table. Every surviving vertex must
        #    carry a normal within the slope limit, which the model's +X normals are not.
        table = [s for s in built["shells"] if "PROP_OUTDOOR_TABLE" in s["sources"]][0]
        require(all(abs(v[1][1] - 1.0) < 1e-6 for v in table["vertices"]),
                "the table's shell carries +Y, the face normal of the top it was accepted for, "
                "and not the model's authored +X")
        require(all(slope_degrees(v[1]) <= 40.0 + 1e-6 for v in table["vertices"]),
                "...so every stored normal is inside the material's slope limit, which the "
                "authored ones (90 degrees from +Y) would not have been")

        # 8b. Welding is on (position, normal), so a hard edge stays two vertices. Welding on
        #     position alone would average across the fold and the offset would open a gap along
        #     every crease.
        creased = {((0.0, 0.0, 0.0), (0.0, 1.0, 0.0)), ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0))}
        require(len(creased) == 2,
                "two faces meeting at one position with different normals stay two vertices")

        # 8c. A degenerate face is counted as degenerate and never reaches a shell. It has no
        #     normal, so it would otherwise fall through the slope test as "facing down" and be
        #     miscounted -- and a zero-area triangle in a vertex buffer is a triangle the GPU
        #     still fetches and rasterises to nothing.
        flat = workspace / "assets" / "sliver.glb"
        positions = [0.0, 0.0, 0.0,  1.0, 0.0, 0.0,  1.0, 0.0, 1.0,  0.0, 0.0, 1.0,
                     2.0, 0.0, 0.0,  3.0, 0.0, 0.0]
        indices = [0, 3, 1, 1, 3, 2,   # a good up-facing quad
                   4, 5, 4]            # a sliver: two of its three corners are the same point
        blob = struct.pack(f"<{len(positions)}f", *positions) \
            + struct.pack(f"<{len(indices)}H", *indices)
        while len(blob) % 4:
            blob += b"\0"
        import gltf_io
        gltf_io.write_glb(flat, {
            "asset": {"version": "2.0"},
            "scenes": [{"nodes": [0]}], "scene": 0,
            "nodes": [{"name": "sliver", "mesh": 0}],
            "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1, "mode": 4}]}],
            "accessors": [
                {"bufferView": 0, "componentType": 5126, "count": len(positions) // 3,
                 "type": "VEC3", "min": [min(positions[i::3]) for i in range(3)],
                 "max": [max(positions[i::3]) for i in range(3)]},
                {"bufferView": 1, "componentType": 5123, "count": len(indices), "type": "SCALAR"}],
            "bufferViews": [
                {"buffer": 0, "byteOffset": 0, "byteLength": len(positions) * 4},
                {"buffer": 0, "byteOffset": len(positions) * 4, "byteLength": len(indices) * 2}],
            "buffers": [{"byteLength": len(blob)}],
        }, bytes(blob))
        manifest.write_text(json.dumps({
            "schema": "cna-house/assets/1",
            "assets": [{"id": "MODEL_TABLE", "sourceFile": str(assets / "table.glb")},
                       {"id": "MODEL_SLIVER", "sourceFile": str(flat)}]},
            indent=2) + "\n", encoding="utf-8")
        (world_dir / "layout.props.json").write_text(json.dumps({
            "schema": "cna-house/props/1",
            "props": [{"id": "PROP_SLIVER", "asset": "MODEL_SLIVER", "cell": "L0_TERRACE",
                       "position": [-2.0, 0.5, 1.5], "yawDeg": 0.0, "scale": 1.0,
                       "static": True, "material": "MAT_TABLE"}]}, indent=2) + "\n",
            encoding="utf-8")
        slivered = build(world_dir, manifest)
        require(slivered["stats"]["rejectedDegenerate"] == 1,
                f"the zero-area triangle is counted as degenerate, not as facing down "
                f"({slivered['stats']['rejectedDegenerate']} degenerate, "
                f"{slivered['stats']['rejectedDown']} down)")
        require(slivered["stats"]["facesKept"] == 4,
                f"...and the two good faces of the quad and the two of the deck are kept, four "
                f"in total ({slivered['stats']['facesKept']})")
        require(abs(triangle_area((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (1.0, 0.0, 0.0))) < 1e-12,
                "a triangle with two identical corners has zero area, on its own")

        # 9. Determinism and the round trip, over the two-table build -- restored here because
        #    8c rewrote the props and the manifest, and a determinism check that rebuilt from a
        #    different layout would compare two worlds and call the difference non-determinism.
        manifest.write_text(json.dumps({
            "schema": "cna-house/assets/1",
            "assets": [{"id": "MODEL_TABLE", "sourceFile": str(assets / "table.glb")}]},
            indent=2) + "\n", encoding="utf-8")
        (world_dir / "layout.props.json").write_text(json.dumps({
            "schema": "cna-house/props/1",
            "props": [
                {"id": "PROP_OUTDOOR_TABLE", "asset": "MODEL_TABLE", "cell": "L0_TERRACE",
                 "position": [-2.0, 0.0, 1.5], "yawDeg": 0.0, "scale": 1.0,
                 "static": True, "material": "MAT_TABLE"},
                {"id": "PROP_INDOOR_TABLE", "asset": "MODEL_TABLE", "cell": "L0_LOUNGE",
                 "position": [2.0, 0.0, 1.5], "yawDeg": 0.0, "scale": 1.0,
                 "static": True, "material": "MAT_TABLE"},
            ]}, indent=2) + "\n", encoding="utf-8")
        built = build(world_dir, manifest)
        data = serialise(built)
        back = read_back(data)
        require(len(back["shells"]) == len(built["shells"]),
                "the file reads back with the same shell count")
        require(all(a["material"] == b["material"] and a["cell"] == b["cell"]
                    for a, b in zip(back["shells"], built["shells"])),
                "every shell's cell and material round trip through the string tables")
        require(all(abs(a[1][k] - b[1][k]) < 1e-5
                    for sa, sb in zip(back["shells"], built["shells"])
                    for a, b in zip(sa["vertices"], sb["vertices"]) for k in range(3)),
                "every vertex NORMAL round trips -- without it the runtime has no direction to "
                "offset along and the whole file is inert")
        require(all(abs(a["area"] - b["area"]) < 1e-3
                    for a, b in zip(back["shells"], built["shells"])),
                "each shell's area round trips")
        require(abs(back["maxDepth"] - MAX_DEPTH) < 1e-6,
                "§38's 0.35 m cap travels with the file, so generation and the runtime offset "
                "agree about the range it was built for")
        require(serialise(build(world_dir, manifest)) == data,
                "two builds of one layout produce byte-identical output")

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
        for offset, value, what in ((4, 9, "version"), (8, 0x10, "flag bit")):
            broken = bytearray(data)
            broken[offset:offset + 4] = struct.pack("<I", value)
            try:
                read_back(bytes(broken))
                caught = False
            except LayoutError:
                caught = True
            require(caught, f"an unknown {what} is refused rather than ignored")

        # 11. The sources §38 lists that do not exist yet are REPORTED, not quietly missing.
        text = report(built)
        require("not yet available" in text and "HOUSE-00470" in text,
                "the roof and the terrain are named as missing sources rather than silently "
                "producing a shell that covers only half of what §38 lists")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("build_snowshell: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--world", type=Path, default=REPO / "assets-src" / "world")
    parser.add_argument("--manifest", type=Path,
                        default=REPO / "assets-src" / "assets.manifest.json")
    parser.add_argument("--out", type=Path, default=REPO / "content" / "world" / "snowshell.bin")
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    try:
        built = build(args.world, args.manifest)
    except LayoutError as exc:
        print(f"build_snowshell: {exc}", file=sys.stderr)
        return 1

    print(report(built))
    if args.report:
        return 0

    data = serialise(built)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(data)
    print(f"build_snowshell: wrote {args.out} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
