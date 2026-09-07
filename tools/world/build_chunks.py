#!/usr/bin/env python3
"""build_chunks.py -- 140 props in a kitchen become 5 draw calls.

`HOUSE-00215`. `cna-house.md` §17.4: every cell's static props are grouped by
`(effectClass, material, lightGroupSet, alphaMode)` and merged into one vertex/index buffer pair
per group, with a bounding box per group and one per **sub-range** so a big group can still be
partially culled. Target: ≤ 6 chunks per cell, ≤ 65 535 vertices per chunk.

    tools/world/build_chunks.py --world assets-src/world --out content/world/chunks.bin
    tools/world/build_chunks.py --world assets-src/world --report
    tools/world/build_chunks.py --selftest

## The four-part key has one part

§17.4's key reads as four independent axes, and measuring it against the data model is the first
thing this tool did. `alphaMode` is a **field of the material** (`layout.materials.json`), and so is
`class`, which is what decides the effect. A prop has no light groups of its own -- `lightGroups`
lives on the *cell*, and a chunk never spans two cells -- so within a cell that term is constant.

So `(effectClass, material, lightGroupSet, alphaMode)` collapses, exactly and without loss, to
**the material id**, and "≤ 6 chunks per cell" means "≤ 6 distinct materials among a cell's static
props". That is a far more useful sentence to give an author than the four-part one, because it is
the thing they control. The key is still built from all four parts, so that a later schema which
gives a prop its own light groups keeps working; the report says how many chunks the extra three
parts actually separated, which today is none.

## The vertex layout is the effect's, not the model's

`cna-house.md` §349 measured CNA's `ModelProcessor` vertex at 48 bytes -- `Position@0`,
`Normal@12`, `Tangent@24`, `TexCoord0@40`. A chunk cannot use it, for two reasons that point in
opposite directions and meet in the middle:

* It has **no second UV**, and §17.4's chunks are drawn with `DualTextureEffect`, whose whole point
  is albedo × lightmap on two channels. The lightmap UV comes from `HOUSE-00205`.
* It has a **normal and a tangent that `DualTextureEffect` never reads**. That effect is unlit --
  the lightmap *is* the lighting -- so 28 of its 48 bytes would be uploaded, stored in VRAM and
  never sampled.

A chunk is built by us and uploaded into our own `VertexBuffer`, so it declares the layout its own
effect reads and nothing more. Three layouts, one per effect class, and since chunks are grouped by
effect class anyway a chunk always has exactly one. Measured on the selftest fixture: 28 bytes per
vertex against 48, a 42 % saving on the geometry that dominates §22's budget.

## Where the two acceptance criteria pull against each other

A group over 65 535 vertices must be split to keep 16-bit indices, and splitting makes more chunks
-- which is the other criterion. The rule here is: **split on prop boundaries** to stay 16-bit,
because a sub-range is a prop already; and if one prop alone exceeds the cap, that chunk goes to
32-bit indices instead, since it cannot be split at all. Both outcomes are reported rather than
being resolved silently, because a cell that needs eight chunks is an authoring problem and the
tool's job is to say so.

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

import build_collision as bc  # noqa: E402
import gltf_io  # noqa: E402
import layout_io  # noqa: E402
from layout_io import LayoutError  # noqa: E402

MAGIC = b"CCHK"
VERSION = 1

#: §17.4's two limits.
MAX_CHUNKS_PER_CELL = 6
MAX_VERTICES_16BIT = 0xFFFF

#: The vertex layouts, one per stock effect, with the attributes that effect actually reads.
#: `MaterialBinder`'s `MaterialKind` is the same closed list of four; `Skinned` never appears here
#: because a skinned prop is an animated one and animated props are not batched (§17.4).
LAYOUT_BASIC, LAYOUT_DUAL, LAYOUT_ALPHATEST = 0, 1, 2
LAYOUTS = {
    LAYOUT_BASIC: ("basic", ("position", "normal", "uv0"), 32),
    LAYOUT_DUAL: ("dual", ("position", "uv0", "uv1"), 28),
    LAYOUT_ALPHATEST: ("alphatest", ("position", "uv0"), 20),
}

#: `layout.materials.json`'s `class` -> the effect its geometry is drawn with. Unknown classes are
#: an error naming the class, never a silent fallback to `Basic`: a material quietly drawn with the
#: wrong effect is a rendering bug that looks like an art bug.
CLASS_TO_LAYOUT = {
    "lightmapped_opaque": LAYOUT_DUAL,
    "lightmapped_mask": LAYOUT_ALPHATEST,
    "lit_opaque": LAYOUT_BASIC,
    "lit_mask": LAYOUT_ALPHATEST,
    "unlit_opaque": LAYOUT_DUAL,
}

EPS = 1e-6


# ================================================================================ reading geometry


def read_geometry(path: Path) -> dict:
    """Every non-`_COL` primitive's positions, normals and both UV sets, welded into one mesh.

    `_COL` meshes are skipped here for the same reason `HOUSE-00210` collects them: §18 says the
    build "extracts them into the collision file and strips them from the runtime model". A proxy
    left in a chunk is invisible geometry the GPU still transforms.
    """
    document, blob = gltf_io.read_model(path)
    buffers = gltf_io.buffer_bytes(document, blob, path.parent)
    transforms = bc.node_world_transforms(document)

    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    uv0: list[tuple[float, float]] = []
    uv1: list[tuple[float, float]] = []
    triangles: list[tuple[int, int, int]] = []
    has_uv1 = True

    for index, node in enumerate(document.get("nodes", [])):
        if "mesh" not in node or node.get("name", "").endswith("_COL"):
            continue
        matrix = transforms[index]
        for primitive in document["meshes"][node["mesh"]].get("primitives", []):
            if primitive.get("mode", 4) != 4:
                continue
            attributes = primitive["attributes"]
            if "POSITION" not in attributes:
                raise LayoutError(f"{path.name}: a primitive has no POSITION")
            raw = gltf_io.read_accessor(document, buffers, attributes["POSITION"])
            base = len(positions)
            for px, py, pz in raw:
                positions.append(tuple(
                    matrix[r][0] * px + matrix[r][1] * py + matrix[r][2] * pz + matrix[r][3]
                    for r in range(3)))
            if "NORMAL" in attributes:
                for nx, ny, nz in gltf_io.read_accessor(document, buffers, attributes["NORMAL"]):
                    normals.append(tuple(
                        matrix[r][0] * nx + matrix[r][1] * ny + matrix[r][2] * nz
                        for r in range(3)))
            else:
                normals.extend([(0.0, 1.0, 0.0)] * len(raw))
            if "TEXCOORD_0" in attributes:
                uv0.extend(tuple(v[:2]) for v in gltf_io.read_accessor(
                    document, buffers, attributes["TEXCOORD_0"]))
            else:
                uv0.extend([(0.0, 0.0)] * len(raw))
            if "TEXCOORD_1" in attributes:
                uv1.extend(tuple(v[:2]) for v in gltf_io.read_accessor(
                    document, buffers, attributes["TEXCOORD_1"]))
            else:
                has_uv1 = False
                uv1.extend([(0.0, 0.0)] * len(raw))
            if "indices" in primitive:
                flat = [int(v[0]) for v in gltf_io.read_accessor(
                    document, buffers, primitive["indices"])]
            else:
                flat = list(range(len(raw)))
            for i in range(0, len(flat) - 2, 3):
                triangles.append((base + flat[i], base + flat[i + 1], base + flat[i + 2]))

    if not triangles:
        raise LayoutError(f"{path.name}: no renderable geometry (only `_COL` proxies?)")
    return {"positions": positions, "normals": normals, "uv0": uv0, "uv1": uv1,
            "triangles": triangles, "hasUv1": has_uv1}


def place(mesh: dict, position, yaw_deg: float, scale: float) -> dict:
    """Bake the prop's placement into the geometry -- §17.4's chunks draw with `Matrix::Identity`.

    Normals are rotated but **not scaled**, and with a uniform scale that is the whole story. A
    non-uniform scale would need the inverse transpose, so it is refused rather than silently
    producing normals that are wrong by a factor nobody can see in a wireframe.
    """
    yaw = math.radians(yaw_deg)
    c, s = math.cos(yaw), math.sin(yaw)
    px, py, pz = position

    def rotate(x, y, z):
        return (x * c + z * s, y, -x * s + z * c)

    out_positions = []
    for x, y, z in mesh["positions"]:
        rx, ry, rz = rotate(x * scale, y * scale, z * scale)
        out_positions.append((px + rx, py + ry, pz + rz))
    out_normals = []
    for x, y, z in mesh["normals"]:
        nx, ny, nz = rotate(x, y, z)
        length = math.sqrt(nx * nx + ny * ny + nz * nz)
        out_normals.append((nx / length, ny / length, nz / length) if length > EPS
                           else (0.0, 1.0, 0.0))
    return dict(mesh, positions=out_positions, normals=out_normals)


def bounds_of(positions) -> tuple:
    xs = [p[0] for p in positions]
    ys = [p[1] for p in positions]
    zs = [p[2] for p in positions]
    return (min(xs), min(ys), min(zs), max(xs), max(ys), max(zs))


# ======================================================================================== grouping


def effect_layout(material: dict) -> int:
    """The vertex layout a material's class implies, or an error naming the class.

    Never a silent fallback to `Basic`: a material quietly drawn with the wrong effect is a
    rendering bug that presents as an art bug, and is looked for in the wrong place for a day.
    """
    effect_class = material.get("class")
    if effect_class not in CLASS_TO_LAYOUT:
        raise LayoutError(
            f"material {material['id']!r} has class {effect_class!r}, which no stock effect "
            f"covers; the known classes are {', '.join(sorted(CLASS_TO_LAYOUT))}")
    return CLASS_TO_LAYOUT[effect_class]


def group_key(prop: dict, cell: dict, material: dict) -> tuple:
    """§17.4's key, built from all four parts even though three of them are not free.

    Kept faithful rather than reduced, so that a schema which later gives a prop its own light
    groups needs no change here. `--report` says how many chunks the three non-material parts
    actually separated, which today is none -- and saying so is better than a comment claiming it.
    """
    effect_layout(material)
    light_groups = tuple(sorted(cell.get("lightGroups") or ()))
    return (material["class"], material["id"], light_groups,
            material.get("alphaMode", "opaque"))


def build(world_dir: Path, manifest_path: Path | None = None) -> dict:
    layout = layout_io.load_layout(world_dir, ["levels", "cells", "materials", "props"])
    cells = layout_io.by_id(layout_io.rows(layout, "cells"), "cell")
    materials = layout_io.by_id(layout_io.rows(layout, "materials"), "material")

    asset_paths: dict[str, Path] = {}
    if manifest_path and manifest_path.is_file():
        for row in layout_io.load_file(manifest_path, "assets").get("assets", []):
            if row.get("sourceFile"):
                asset_paths[row["id"]] = REPO / row["sourceFile"]

    geometry_cache: dict[str, dict] = {}
    groups: dict[tuple[str, tuple], list[dict]] = {}
    stats = {"props": 0, "dynamic": 0, "split": 0, "wide": 0,
             "cellsOverChunkLimit": [], "materialsPerCell": {}}

    for prop in sorted(layout_io.rows(layout, "props"), key=lambda p: p["id"]):
        stats["props"] += 1
        if not prop.get("static", True):
            stats["dynamic"] += 1
            continue
        cell = cells.get(prop["cell"])
        if cell is None:
            raise LayoutError(f"prop {prop['id']!r} names cell {prop['cell']!r}, which does not exist")
        material_id = prop.get("material") or cell.get("wallMaterial")
        material = materials.get(material_id)
        if material is None:
            raise LayoutError(
                f"prop {prop['id']!r} resolves to material {material_id!r}, which does not exist")
        path = asset_paths.get(prop["asset"])
        if path is None:
            raise LayoutError(
                f"prop {prop['id']!r} names asset {prop['asset']!r}, not in assets.manifest.json")
        if prop["asset"] not in geometry_cache:
            geometry_cache[prop["asset"]] = read_geometry(path)
        mesh = geometry_cache[prop["asset"]]

        layout_id = effect_layout(material)
        if layout_id == LAYOUT_DUAL and not mesh["hasUv1"]:
            raise LayoutError(
                f"prop {prop['id']!r} is drawn with DualTextureEffect (material "
                f"{material_id!r}, class {material['class']!r}) but {path.name} has no "
                f"TEXCOORD_1; run tools/blender/lightmap_unwrap.py over it (HOUSE-00205)")

        placed = place(mesh, [float(c) for c in prop["position"]],
                       float(prop.get("yawDeg", 0.0)), float(prop.get("scale", 1.0)))
        key = (prop["cell"], group_key(prop, cell, material))
        groups.setdefault(key, []).append({
            "prop": prop["id"], "mesh": placed, "layout": layout_id, "material": material_id})

    chunks = []
    for (cell_id, key) in sorted(groups, key=lambda k: (k[0], k[1])):
        members = groups[(cell_id, key)]
        for chunk in _split(members, stats):
            chunk.update({"cell": cell_id, "material": key[1], "key": key})
            chunks.append(chunk)

    per_cell: dict[str, int] = {}
    for chunk in chunks:
        per_cell[chunk["cell"]] = per_cell.get(chunk["cell"], 0) + 1
    stats["cellsOverChunkLimit"] = sorted(
        c for c, n in per_cell.items() if n > MAX_CHUNKS_PER_CELL)
    stats["materialsPerCell"] = {
        cell_id: len({k[1][1] for k in groups if k[0] == cell_id})
        for cell_id in sorted({k[0] for k in groups})}
    # How many chunks the three non-material parts of §17.4's key actually separated.
    stats["keysBeyondMaterial"] = len({k for k in groups}) - len(
        {(k[0], k[1][1]) for k in groups})
    stats["chunksPerCell"] = per_cell

    return {"chunks": chunks, "stats": stats, "worldHash": bc._world_hash(world_dir)}


def _split(members: list[dict], stats: dict) -> list[dict]:
    """One group becomes one chunk, or several if it would exceed the 16-bit vertex cap.

    Splitting is on **prop boundaries**, because a sub-range is a prop already and a sub-range that
    straddled two buffers could not have one bounding box. A single prop over the cap cannot be
    split at all, so that chunk takes 32-bit indices -- §17.4's "32-bit is available on EasyGL if
    needed" -- and is reported.
    """
    out: list[dict] = []
    current: list[dict] = []
    current_vertices = 0

    def flush(batch, wide: bool):
        if not batch:
            return
        vertices = []
        indices = []
        sub_ranges = []
        for member in batch:
            mesh = member["mesh"]
            base = len(vertices)
            start = len(indices)
            for i in range(len(mesh["positions"])):
                vertices.append((mesh["positions"][i], mesh["normals"][i],
                                 mesh["uv0"][i], mesh["uv1"][i]))
            for a, b, c in mesh["triangles"]:
                indices.extend((base + a, base + b, base + c))
            sub_ranges.append({"prop": member["prop"], "indexStart": start,
                               "indexCount": len(indices) - start,
                               "bounds": bounds_of(mesh["positions"])})
        out.append({"layout": batch[0]["layout"], "vertices": vertices, "indices": indices,
                    "subRanges": sub_ranges, "bounds": bounds_of([v[0] for v in vertices]),
                    "indexBits": 32 if wide else 16})

    for member in members:
        count = len(member["mesh"]["positions"])
        if count > MAX_VERTICES_16BIT:
            flush(current, False)
            current, current_vertices = [], 0
            flush([member], True)
            stats["wide"] += 1
            continue
        if current_vertices + count > MAX_VERTICES_16BIT:
            flush(current, False)
            stats["split"] += 1
            current, current_vertices = [], 0
        current.append(member)
        current_vertices += count
    flush(current, False)
    return out


# ========================================================================================= writer


def _pack_vertex(layout_id: int, vertex) -> bytes:
    position, normal, uv0, uv1 = vertex
    fields = LAYOUTS[layout_id][1]
    out = struct.pack("<3f", *position)
    if "normal" in fields:
        out += struct.pack("<3f", *normal)
    if "uv0" in fields:
        out += struct.pack("<2f", *uv0)
    if "uv1" in fields:
        out += struct.pack("<2f", *uv1)
    return out


def serialise(built: dict) -> bytes:
    chunks = built["chunks"]
    cells = sorted({c["cell"] for c in chunks})
    materials = sorted({c["material"] for c in chunks})
    cell_index = {name: i for i, name in enumerate(cells)}
    material_index = {name: i for i, name in enumerate(materials)}

    out = bytearray()
    out += MAGIC
    out += struct.pack("<II", VERSION, 0)
    out += bc._string(built["worldHash"])
    out += struct.pack("<I", len(cells))
    for name in cells:
        out += bc._string(name)
    out += struct.pack("<I", len(materials))
    for name in materials:
        out += bc._string(name)

    out += struct.pack("<I", len(chunks))
    for chunk in chunks:
        out += struct.pack("<HH", cell_index[chunk["cell"]], material_index[chunk["material"]])
        out += struct.pack("<BB", chunk["layout"], 1 if chunk["indexBits"] == 32 else 0)
        out += struct.pack("<6f", *chunk["bounds"])
        out += struct.pack("<I", len(chunk["vertices"]))
        for vertex in chunk["vertices"]:
            out += _pack_vertex(chunk["layout"], vertex)
        out += struct.pack("<I", len(chunk["indices"]))
        code = "<I" if chunk["indexBits"] == 32 else "<H"
        for index in chunk["indices"]:
            out += struct.pack(code, index)
        out += struct.pack("<I", len(chunk["subRanges"]))
        for sub in chunk["subRanges"]:
            out += bc._string(sub["prop"])
            out += struct.pack("<II", sub["indexStart"], sub["indexCount"])
            out += struct.pack("<6f", *sub["bounds"])
    return bytes(out)


def read_back(data: bytes) -> dict:
    view = memoryview(data)
    at = 0

    def take(n):
        nonlocal at
        chunk = view[at:at + n]
        if len(chunk) != n:
            raise LayoutError(f"chunks.bin truncated at byte {at}, wanted {n} more")
        at += n
        return chunk

    def unpack(fmt):
        return struct.unpack(fmt, take(struct.calcsize(fmt)))

    def text():
        (length,) = unpack("<H")
        return bytes(take(length)).decode("utf-8")

    if bytes(take(4)) != MAGIC:
        raise LayoutError("not a chunks.bin: bad magic")
    version, flags = unpack("<II")
    if version != VERSION:
        raise LayoutError(f"chunks.bin is version {version}; this reader knows {VERSION}")
    if flags:
        raise LayoutError(f"chunks.bin sets unknown flag bits {flags:#x}")
    world_hash = text()
    (cell_count,) = unpack("<I")
    cells = [text() for _ in range(cell_count)]
    (material_count,) = unpack("<I")
    materials = [text() for _ in range(material_count)]

    (chunk_count,) = unpack("<I")
    chunks = []
    for _ in range(chunk_count):
        cell, material = unpack("<HH")
        layout_id, wide = unpack("<BB")
        bounds = unpack("<6f")
        (vertex_count,) = unpack("<I")
        fields = LAYOUTS[layout_id][1]
        vertices = []
        for _ in range(vertex_count):
            position = unpack("<3f")
            normal = unpack("<3f") if "normal" in fields else None
            texture0 = unpack("<2f") if "uv0" in fields else None
            texture1 = unpack("<2f") if "uv1" in fields else None
            vertices.append((position, normal, texture0, texture1))
        (index_count,) = unpack("<I")
        code = "<I" if wide else "<H"
        indices = [unpack(code)[0] for _ in range(index_count)]
        (sub_count,) = unpack("<I")
        sub_ranges = []
        for _ in range(sub_count):
            name = text()
            start, count = unpack("<II")
            sub_ranges.append({"prop": name, "indexStart": start, "indexCount": count,
                               "bounds": unpack("<6f")})
        chunks.append({"cell": cells[cell], "material": materials[material],
                       "layout": layout_id, "indexBits": 32 if wide else 16,
                       "bounds": bounds, "vertices": vertices, "indices": indices,
                       "subRanges": sub_ranges})
    if at != len(data):
        raise LayoutError(f"{len(data) - at} bytes left over after the last chunk")
    return {"worldHash": world_hash, "cells": cells, "materials": materials, "chunks": chunks}


def report(built: dict) -> str:
    stats = built["stats"]
    chunks = built["chunks"]
    vertices = sum(len(c["vertices"]) for c in chunks)
    packed = sum(len(c["vertices"]) * LAYOUTS[c["layout"]][2] for c in chunks)
    lines = [
        f"{len(chunks)} chunks over {len(stats['chunksPerCell'])} cells, "
        f"{stats['props']} props ({stats['dynamic']} dynamic, excluded)",
        f"  {vertices} vertices, {packed} bytes packed against "
        f"{vertices * 48} in CNA's 48-byte model vertex "
        f"({100 * (1 - packed / (vertices * 48)):.0f} % saved)" if vertices else "  no geometry",
        f"  §17.4's key separated {stats['keysBeyondMaterial']} chunk(s) beyond what the "
        f"material id alone would have",
        f"  {stats['split']} group(s) split for the 16-bit cap, {stats['wide']} chunk(s) on "
        f"32-bit indices",
    ]
    for cell_id, count in sorted(stats["chunksPerCell"].items()):
        flag = "  <-- over the limit" if count > MAX_CHUNKS_PER_CELL else ""
        lines.append(f"  {cell_id:<24} {count} chunk(s), "
                     f"{stats['materialsPerCell'].get(cell_id, 0)} material(s){flag}")
    return "\n".join(lines)


# ======================================================================================= selftest


def _fixture_model(path: Path, *, uv1: bool, boxes=1, col_proxy=False) -> None:
    """A `.glb` with POSITION, NORMAL, TEXCOORD_0 and optionally TEXCOORD_1.

    `bc._fixture_proxy` writes positions only, which is right for a collision proxy and useless
    here: the whole question this tool answers is which attributes reach the buffer.

    The box is deliberately **chiral** -- 0.5 m across x and 2.0 m along z, with every normal
    pointing along +x. A cube with up-facing normals is invariant under the yaw this tool has to
    apply, so a fixture built from one cannot tell a working rotation from a missing one. The first
    version of this fixture was a cube, and two injected bugs walked straight through it.
    """
    positions: list[float] = []
    normals: list[float] = []
    uvs0: list[float] = []
    uvs1: list[float] = []
    indices: list[int] = []
    for b in range(boxes):
        cx = b * 2.0
        base = len(positions) // 3
        corners = [(-0.25, 0.0, -1.0), (0.25, 0.0, -1.0), (0.25, 1.0, -1.0), (-0.25, 1.0, -1.0),
                   (-0.25, 0.0, 1.0), (0.25, 0.0, 1.0), (0.25, 1.0, 1.0), (-0.25, 1.0, 1.0)]
        for i, (x, y, z) in enumerate(corners):
            positions += [cx + x, y, z]
            normals += [1.0, 0.0, 0.0]
            uvs0 += [(i % 2), (i // 2) / 4.0]
            uvs1 += [(i % 4) / 4.0, (i // 4)]
        for a, bb, c in [(0, 1, 2), (0, 2, 3), (4, 6, 5), (4, 7, 6),
                         (0, 4, 5), (0, 5, 1), (3, 2, 6), (3, 6, 7),
                         (0, 3, 7), (0, 7, 4), (1, 5, 6), (1, 6, 2)]:
            indices += [base + a, base + bb, base + c]

    blob = bytearray()
    offsets = {}
    for name, values, code in (("pos", positions, "f"), ("nrm", normals, "f"),
                               ("uv0", uvs0, "f"), ("uv1", uvs1, "f")):
        offsets[name] = len(blob)
        blob += struct.pack(f"<{len(values)}{code}", *values)
    offsets["idx"] = len(blob)
    # The FIXTURE's own indices need 32 bits once it has more than 65 535 vertices -- which is the
    # whole point of the wide fixture, and a reminder that the same limit binds the source asset.
    wide_source = len(positions) // 3 > 0xFFFF
    blob += struct.pack(f"<{len(indices)}{'I' if wide_source else 'H'}", *indices)
    while len(blob) % 4:
        blob += b"\0"

    count = len(positions) // 3
    attributes = {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}
    accessors = [
        {"bufferView": 0, "componentType": 5126, "count": count, "type": "VEC3",
         "min": [min(positions[i::3]) for i in range(3)],
         "max": [max(positions[i::3]) for i in range(3)]},
        {"bufferView": 1, "componentType": 5126, "count": count, "type": "VEC3"},
        {"bufferView": 2, "componentType": 5126, "count": count, "type": "VEC2"},
    ]
    views = [
        {"buffer": 0, "byteOffset": offsets["pos"], "byteLength": len(positions) * 4},
        {"buffer": 0, "byteOffset": offsets["nrm"], "byteLength": len(normals) * 4},
        {"buffer": 0, "byteOffset": offsets["uv0"], "byteLength": len(uvs0) * 4},
    ]
    if uv1:
        attributes["TEXCOORD_1"] = len(accessors)
        accessors.append({"bufferView": len(views), "componentType": 5126, "count": count,
                          "type": "VEC2"})
        views.append({"buffer": 0, "byteOffset": offsets["uv1"], "byteLength": len(uvs1) * 4})
    index_accessor = len(accessors)
    accessors.append({"bufferView": len(views),
                      "componentType": 5125 if wide_source else 5123,
                      "count": len(indices), "type": "SCALAR"})
    views.append({"buffer": 0, "byteOffset": offsets["idx"],
                  "byteLength": len(indices) * (4 if wide_source else 2)})

    nodes = [{"name": path.stem, "mesh": 0}]
    meshes = [{"primitives": [{"attributes": attributes, "indices": index_accessor, "mode": 4}]}]
    scene_nodes = [0]
    if col_proxy:
        # The same geometry again, named `<stem>_COL`. §18 says the build strips proxies from the
        # runtime model; a proxy left in a chunk is invisible geometry the GPU still transforms.
        nodes.append({"name": f"{path.stem}_COL", "mesh": 0})
        scene_nodes.append(1)
    gltf_io.write_glb(path, {
        "asset": {"version": "2.0"},
        "scenes": [{"nodes": scene_nodes}], "scene": 0,
        "nodes": nodes,
        "meshes": meshes,
        "accessors": accessors, "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
    }, bytes(blob))


def selftest() -> int:
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("build_chunks: selftest")
    workspace = Path(tempfile.mkdtemp(prefix="build_chunks_selftest_"))
    try:
        world_dir = workspace / "world"
        world_dir.mkdir()
        bc._fixture_world(world_dir)
        assets = workspace / "assets"
        assets.mkdir()
        _fixture_model(assets / "lit.glb", uv1=True)
        _fixture_model(assets / "nouv1.glb", uv1=False)
        _fixture_model(assets / "wide.glb", uv1=True, boxes=8300)    # 66 400 vertices, over the cap
        _fixture_model(assets / "medium.glb", uv1=True, boxes=3800)  # 30 400, under it alone
        _fixture_model(assets / "withcol.glb", uv1=True, col_proxy=True)

        (world_dir / "layout.materials.json").write_text(json.dumps({
            "schema": "cna-house/materials/1",
            "materials": [
                {"id": "MAT_SHELL", "class": "lightmapped_opaque", "alphaMode": "opaque"},
                {"id": "MAT_WOOD", "class": "lightmapped_opaque", "alphaMode": "opaque"},
                {"id": "MAT_LEAF", "class": "lit_mask", "alphaMode": "mask"},
                {"id": "MAT_LAMP", "class": "lit_opaque", "alphaMode": "opaque"},
            ]}, indent=2) + "\n", encoding="utf-8")

        def write_props(rows):
            (world_dir / "layout.props.json").write_text(json.dumps({
                "schema": "cna-house/props/1", "props": rows}, indent=2) + "\n",
                encoding="utf-8")

        def write_manifest(rows):
            path = workspace / "assets.manifest.json"
            path.write_text(json.dumps({"schema": "cna-house/assets/1", "assets": rows},
                                       indent=2) + "\n", encoding="utf-8")
            return path

        manifest = write_manifest([
            {"id": "MODEL_LIT", "sourceFile": str(assets / "lit.glb")},
            {"id": "MODEL_NOUV1", "sourceFile": str(assets / "nouv1.glb")},
            {"id": "MODEL_WIDE", "sourceFile": str(assets / "wide.glb")},
            {"id": "MODEL_MEDIUM", "sourceFile": str(assets / "medium.glb")},
            {"id": "MODEL_WITHCOL", "sourceFile": str(assets / "withcol.glb")},
        ])

        def prop(identifier, material, **kw):
            row = {"id": identifier, "asset": "MODEL_LIT", "cell": "L0_LOUNGE",
                   "position": [1.0, 0.0, 1.0], "yawDeg": 0.0, "scale": 1.0,
                   "static": True, "material": material}
            row.update(kw)
            return row

        # 1. Four props on two materials become two chunks, not four draw calls.
        write_props([prop("PROP_A", "MAT_SHELL"), prop("PROP_B", "MAT_SHELL"),
                     prop("PROP_C", "MAT_WOOD"), prop("PROP_D", "MAT_WOOD")])
        built = build(world_dir, manifest)
        require(len(built["chunks"]) == 2,
                f"four props on two materials merge into two chunks (got "
                f"{len(built['chunks'])})")
        require(all(len(c["subRanges"]) == 2 for c in built["chunks"]),
                "...each carrying two sub-ranges, one per source prop, so a big group can still "
                "be partially culled (§17.4)")

        # 2. §17.4's four-part key has exactly one free part, and the tool says so rather than a
        #    comment claiming it. Two materials of the SAME class, alphaMode and cell still make
        #    two chunks; nothing else in the key ever separates one.
        require(built["stats"]["keysBeyondMaterial"] == 0,
                f"the effectClass, lightGroupSet and alphaMode terms separate no chunk the "
                f"material id would not have ({built['stats']['keysBeyondMaterial']}), because "
                f"all three are properties of the material or of the cell")

        # 3. The vertex layout is the effect's. `DualTextureEffect` is unlit, so a normal in its
        #    buffer is 12 bytes uploaded and never sampled.
        dual = [c for c in built["chunks"] if c["layout"] == LAYOUT_DUAL]
        require(len(dual) == 2, "a lightmapped_opaque material is drawn with DualTextureEffect")
        require(LAYOUTS[LAYOUT_DUAL][1] == ("position", "uv0", "uv1"),
                "...whose vertex carries position and two UV sets, and NO normal")
        require(LAYOUTS[LAYOUT_DUAL][2] == 28 and LAYOUTS[LAYOUT_BASIC][2] == 32,
                f"28 bytes against BasicEffect's 32 and CNA's 48-byte model vertex "
                f"({LAYOUTS[LAYOUT_DUAL][2]}, {LAYOUTS[LAYOUT_BASIC][2]})")

        # 4. The placement is BAKED IN -- §17.4 draws chunks with Matrix::Identity, so a prop's
        #    yaw and position have to be in the vertices or they are nowhere.
        write_props([prop("PROP_A", "MAT_SHELL", position=[5.0, 0.0, 2.0], yawDeg=0.0)])
        straight = build(world_dir, manifest)
        box = straight["chunks"][0]["bounds"]
        require(abs((box[0] + box[3]) / 2 - 5.0) < 1e-5
                and abs((box[2] + box[5]) / 2 - 2.0) < 1e-5,
                f"the chunk's bounds are centred on the prop's world position "
                f"({tuple(round(v, 3) for v in box)})")
        require(abs((box[3] - box[0]) - 0.5) < 1e-5 and abs((box[5] - box[2]) - 2.0) < 1e-5,
                f"unturned, the box is 0.5 m across x and 2.0 m along z "
                f"({box[3] - box[0]:.3f} by {box[5] - box[2]:.3f})")
        write_props([prop("PROP_A", "MAT_SHELL", position=[5.0, 0.0, 2.0], yawDeg=90.0)])
        turned = build(world_dir, manifest)
        box = turned["chunks"][0]["bounds"]
        require(abs((box[3] - box[0]) - 2.0) < 1e-5 and abs((box[5] - box[2]) - 0.5) < 1e-5,
                f"turned 90 degrees the extents SWAP -- 2.0 across x, 0.5 along z "
                f"({box[3] - box[0]:.3f} by {box[5] - box[2]:.3f}) -- which a fixture built from "
                f"a cube could not have shown")
        require(abs((box[0] + box[3]) / 2 - 5.0) < 1e-5
                and abs((box[2] + box[5]) / 2 - 2.0) < 1e-5,
                "...and it is still centred where the author put it")
        write_props([prop("PROP_A", "MAT_LAMP", position=[0.0, 0.0, 0.0], yawDeg=0.0)])
        require(all(abs(n[1][0] - 1.0) < 1e-5
                    for n in build(world_dir, manifest)["chunks"][0]["vertices"]),
                "unturned, the fixture's +x normal is +x")
        write_props([prop("PROP_A", "MAT_LAMP", position=[0.0, 0.0, 0.0], yawDeg=90.0)])
        rotated = build(world_dir, manifest)["chunks"][0]["vertices"]
        require(all(abs(n[1][0]) < 1e-5 and abs(n[1][2] + 1.0) < 1e-5 for n in rotated),
                f"turned 90 degrees it is -z: normals are rotated with the prop, not left in "
                f"asset space where every lit surface would face the wrong way "
                f"({tuple(round(c, 3) for c in rotated[0][1])})")

        # 5. A DualTexture prop with no second UV set is refused, naming the tool that makes one.
        write_props([prop("PROP_A", "MAT_SHELL", asset="MODEL_NOUV1")])
        try:
            build(world_dir, manifest)
            raised = ""
        except LayoutError as exc:
            raised = str(exc)
        require("TEXCOORD_1" in raised and "lightmap_unwrap" in raised,
                "a lightmapped prop with no TEXCOORD_1 is refused, naming HOUSE-00205's tool")

        # 6. An unknown material class is an error, never a silent fall back to BasicEffect: a
        #    material quietly drawn with the wrong effect is a rendering bug that looks like art.
        (world_dir / "layout.materials.json").write_text(json.dumps({
            "schema": "cna-house/materials/1",
            "materials": [{"id": "MAT_ODD", "class": "shiny", "alphaMode": "opaque"}]},
            indent=2) + "\n", encoding="utf-8")
        write_props([prop("PROP_A", "MAT_ODD")])
        try:
            build(world_dir, manifest)
            raised = ""
        except LayoutError as exc:
            raised = str(exc)
        require("shiny" in raised and "stock effect" in raised,
                "an unknown material class is refused, naming the class")

        (world_dir / "layout.materials.json").write_text(json.dumps({
            "schema": "cna-house/materials/1",
            "materials": [
                {"id": "MAT_SHELL", "class": "lightmapped_opaque", "alphaMode": "opaque"},
                {"id": "MAT_WOOD", "class": "lightmapped_opaque", "alphaMode": "opaque"},
                {"id": "MAT_LEAF", "class": "lit_mask", "alphaMode": "mask"},
                {"id": "MAT_LAMP", "class": "lit_opaque", "alphaMode": "opaque"},
            ]}, indent=2) + "\n", encoding="utf-8")

        # 7. Dynamic props are excluded -- §17.4: "props that must move are excluded from batching
        #    and become dynamic instances".
        write_props([prop("PROP_A", "MAT_SHELL"),
                     prop("PROP_B", "MAT_SHELL", static=False)])
        built = build(world_dir, manifest)
        require(built["stats"]["dynamic"] == 1
                and sum(len(c["subRanges"]) for c in built["chunks"]) == 1,
                f"a non-static prop is excluded from every chunk "
                f"({built['stats']['dynamic']} dynamic, "
                f"{sum(len(c['subRanges']) for c in built['chunks'])} sub-ranges)")

        # 8. The 16-bit cap. Eight props of 8 300 boxes each is 66 400 vertices per prop, over the
        #    cap on its own, so it cannot be split and takes 32-bit indices.
        write_props([prop("PROP_BIG", "MAT_SHELL", asset="MODEL_WIDE")])
        big = build(world_dir, manifest)
        require(len(big["chunks"][0]["vertices"]) > MAX_VERTICES_16BIT,
                f"the fixture really does exceed the cap "
                f"({len(big['chunks'][0]['vertices'])} vertices)")
        require(big["chunks"][0]["indexBits"] == 32 and big["stats"]["wide"] == 1,
                "a single prop over 65 535 vertices cannot be split, so its chunk takes 32-bit "
                "indices and is reported")

        # 9. A group of several props that each FIT but together do not is split, on prop
        #    boundaries. Three 30 400-vertex props is 91 200, over the cap; each alone is under it,
        #    so this exercises the split rather than the oversized-prop path, which is what the
        #    first version of this claim accidentally tested instead.
        write_props([prop(f"PROP_{i}", "MAT_SHELL", asset="MODEL_MEDIUM") for i in range(3)])
        split = build(world_dir, manifest)
        require(len(split["chunks"]) == 2 and split["stats"]["split"] == 1,
                f"three props of 30 400 vertices become 2 chunks, split once "
                f"({len(split['chunks'])} chunks, {split['stats']['split']} split)")
        require(all(len(c["vertices"]) <= MAX_VERTICES_16BIT and c["indexBits"] == 16
                    for c in split["chunks"]),
                "...both under the cap and still on 16-bit indices, which is the point of "
                "splitting at all")
        require(sorted(len(c["subRanges"]) for c in split["chunks"]) == [1, 2],
                f"...and the split fell on a prop boundary: 2 props then 1, never half of one "
                f"({sorted(len(c['subRanges']) for c in split['chunks'])})")
        require(all(split["stats"]["wide"] == 0 for _ in (0,)),
                "no chunk needed 32-bit indices, because splitting removed the need")

        # 9b. A `_COL` proxy inside the model is stripped, not batched (§18). The fixture's proxy
        #     is a second node over the same mesh, so a tool that batched it would double every
        #     vertex count -- invisible geometry the GPU still transforms.
        write_props([prop("PROP_A", "MAT_SHELL", asset="MODEL_WITHCOL")])
        with_col = build(world_dir, manifest)
        write_props([prop("PROP_A", "MAT_SHELL", asset="MODEL_LIT")])
        without = build(world_dir, manifest)
        require(len(with_col["chunks"][0]["vertices"])
                == len(without["chunks"][0]["vertices"]),
                f"a model carrying a `_COL` proxy batches the same vertex count as one without "
                f"({len(with_col['chunks'][0]['vertices'])} vs "
                f"{len(without['chunks'][0]['vertices'])})")

        # 10. §17.4's chunk limit, and the honest report when the two limits disagree.
        write_props([prop("PROP_A", "MAT_SHELL"), prop("PROP_B", "MAT_WOOD"),
                     prop("PROP_C", "MAT_LEAF"), prop("PROP_D", "MAT_LAMP")])
        built = build(world_dir, manifest)
        require(built["stats"]["cellsOverChunkLimit"] == [],
                "four materials in one cell is four chunks, inside §17.4's six")
        require(built["stats"]["materialsPerCell"]["L0_LOUNGE"] == 4,
                "...and the report says which cells have how many materials, since that is the "
                "number an author actually controls")

        # 11. Sub-range index ranges partition the chunk's index buffer exactly. A sub-range that
        #     overlapped or left a gap would cull the wrong triangles.
        for chunk in built["chunks"]:
            covered = []
            for sub in chunk["subRanges"]:
                covered.append((sub["indexStart"], sub["indexStart"] + sub["indexCount"]))
            covered.sort()
            joined = covered[0][0] == 0 and covered[-1][1] == len(chunk["indices"])
            for a, b in zip(covered, covered[1:]):
                joined = joined and a[1] == b[0]
            require(joined,
                    f"the sub-ranges of a {chunk['material']} chunk tile its index buffer exactly "
                    f"({covered} over {len(chunk['indices'])} indices)")

        # 11b. A sub-range's bounding box is its OWN prop's, not the chunk's. That is the whole
        #      reason it exists (§17.4: "so a big group can still be partially culled"); a
        #      sub-range carrying the group's box culls nothing and costs 24 bytes to do it.
        write_props([prop("PROP_NEAR", "MAT_SHELL", position=[0.0, 0.0, 0.0]),
                     prop("PROP_FAR", "MAT_SHELL", position=[3.0, 0.0, 0.0])])
        spread = build(world_dir, manifest)
        chunk = spread["chunks"][0]
        near = next(r for r in chunk["subRanges"] if r["prop"] == "PROP_NEAR")
        far = next(r for r in chunk["subRanges"] if r["prop"] == "PROP_FAR")
        require(abs(near["bounds"][0] - -0.25) < 1e-5 and abs(far["bounds"][0] - 2.75) < 1e-5,
                f"each sub-range's box is its own prop's, three metres apart "
                f"({near['bounds'][0]:.2f} and {far['bounds'][0]:.2f})")
        require(near["bounds"] != far["bounds"],
                "...so two props in one chunk do not share a bounding box")
        for sub in chunk["subRanges"]:
            inside = all(sub["bounds"][k] >= chunk["bounds"][k] - 1e-5 for k in range(3)) and \
                     all(sub["bounds"][k] <= chunk["bounds"][k] + 1e-5 for k in range(3, 6))
            require(inside,
                    f"{sub['prop']}'s box is inside the chunk's, as a partial cull requires")

        # 12. Round trip, over claim 10's four-material build -- restored here because 11b
        #     rewrote the props file, and a determinism check that rebuilt from a different
        #     layout would compare two different worlds and call the difference non-determinism.
        write_props([prop("PROP_A", "MAT_SHELL"), prop("PROP_B", "MAT_WOOD"),
                     prop("PROP_C", "MAT_LEAF"), prop("PROP_D", "MAT_LAMP")])
        built = build(world_dir, manifest)
        data = serialise(built)
        back = read_back(data)
        require(len(back["chunks"]) == len(built["chunks"]),
                "the file reads back with the same chunk count")
        require([c["material"] for c in back["chunks"]]
                == [c["material"] for c in built["chunks"]],
                "every chunk's material round trips through the string table")
        require(all(c["layout"] == o["layout"] and c["indexBits"] == o["indexBits"]
                    for c, o in zip(back["chunks"], built["chunks"])),
                "the vertex layout and index width round trip -- a reader that guessed either "
                "would decode the buffer as noise")
        require(all(v[1] is None for v in back["chunks"][0]["vertices"])
                if back["chunks"][0]["layout"] == LAYOUT_DUAL else True,
                "a DualTexture chunk really carries no normal in the file")
        require(all(abs(a - b) < 1e-4
                    for c, o in zip(back["chunks"], built["chunks"])
                    for a, b in zip(c["bounds"], o["bounds"])),
                "every chunk's bounding box round trips")
        require(all(sa["prop"] == so["prop"] and sa["indexStart"] == so["indexStart"]
                    for c, o in zip(back["chunks"], built["chunks"])
                    for sa, so in zip(c["subRanges"], o["subRanges"])),
                "every sub-range's prop and index range round trip")
        require(serialise(build(world_dir, manifest)) == data,
                "two builds of one layout produce byte-identical output")

        # 12b. The 32-bit path round trips too. The build above has no wide chunk, so a writer
        #      that emitted a constant "16-bit" would have satisfied every claim so far and then
        #      handed the runtime a 32-bit index buffer to decode as 16-bit noise.
        write_props([prop("PROP_BIG", "MAT_SHELL", asset="MODEL_WIDE")])
        wide_built = build(world_dir, manifest)
        wide_back = read_back(serialise(wide_built))
        require(wide_back["chunks"][0]["indexBits"] == 32,
                "a 32-bit chunk reads back as 32-bit")
        require(wide_back["chunks"][0]["indices"] == wide_built["chunks"][0]["indices"],
                f"...and all {len(wide_built['chunks'][0]['indices'])} of its indices survive, "
                f"including those above 65 535")
        require(max(wide_built["chunks"][0]["indices"]) > MAX_VERTICES_16BIT,
                "...which really are above 65 535, so the claim above has something to prove")

        # 13. Truncation, version, flags and trailing bytes.
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
        for offset, value, what in ((4, 42, "version"), (8, 0x4, "flag bit")):
            broken = bytearray(data)
            broken[offset:offset + 4] = struct.pack("<I", value)
            try:
                read_back(bytes(broken))
                caught = False
            except LayoutError:
                caught = True
            require(caught, f"an unknown {what} is refused rather than ignored")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("build_chunks: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--world", type=Path, default=REPO / "assets-src" / "world")
    parser.add_argument("--manifest", type=Path,
                        default=REPO / "assets-src" / "assets.manifest.json")
    parser.add_argument("--out", type=Path, default=REPO / "content" / "world" / "chunks.bin")
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    try:
        built = build(args.world, args.manifest)
    except LayoutError as exc:
        print(f"build_chunks: {exc}", file=sys.stderr)
        return 1

    print(report(built))
    over = built["stats"]["cellsOverChunkLimit"]
    if over:
        print(f"build_chunks: {len(over)} cell(s) exceed §17.4's {MAX_CHUNKS_PER_CELL}-chunk "
              f"target: {', '.join(over)}", file=sys.stderr)
    if args.report:
        return 1 if over else 0

    data = serialise(built)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(data)
    print(f"build_chunks: wrote {args.out} ({len(data)} bytes)")
    return 1 if over else 0


if __name__ == "__main__":
    sys.exit(main())
