#!/usr/bin/env python3
"""Prepare and verify HOUSE-01055's pinned Wayfair SheenChair source.

The Khronos source includes two material variants, seven embedded preview/runtime maps, a small
manufacturer label and PBR-only sheen metadata.  The house static-prop path rebuilds geometry into
canonical material chunks, so this tool keeps the three visible furniture roles, removes the
label and unsupported metadata, recentres/grounds the geometry and appends a deterministic
twelve-triangle collision box.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import struct
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "assets"))
import gltf_io  # noqa: E402
import gltf_validate  # noqa: E402
import origin_check  # noqa: E402
import scale_check  # noqa: E402
sys.path.insert(0, str(REPO / "tools" / "world"))
import layout_io  # noqa: E402

SOURCE_URL = (
    "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/"
    "SheenChair/glTF-Binary/SheenChair.glb"
)
SOURCE_SHA256 = "f0af2a2b102d28d540236306ae19f8fb36842df76bd38cf76f063f9bd2853399"
SOURCE_BYTES = 4_125_648
TARGET = (REPO / "assets-src" / "Models" / "Furniture" / "LivingRoom" /
          "sheen_chair.glb")
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSET_ID = "MODEL_FURNITURE_SHEEN_CHAIR"
COLLISION_NAME = "SheenChair_COL"
VISIBLE_NODES = {"SheenChair_fabric", "SheenChair_wood", "SheenChair_metal"}
VISIBLE_MATERIALS = {"fabric Mystere Mango Velvet", "wood Brown", "metal"}
EXPECTED_TRIANGLES = 39_808
EXPECTED_BOUNDS = [0.826558, 0.686247, 0.570265]
EXPECTED_MATERIAL_MAP = {
    "fabric Mystere Mango Velvet": "MAT_FURNITURE_SHEEN_CHAIR_VELVET",
    "wood Brown": "MAT_FURNITURE_PIANO_WOOD",
    "metal": "MAT_KITCHEN_HARDWARE_STEEL",
}
EXPECTED_PROPS = {
    "PROP_LIVING_CHAIR_GREEN": ("L0_LIVING", [-7.92, 0.6, -17.65], 90, 0.98),
    "PROP_LIVING_CHAIR_PURPLE": ("L0_LIVING", [-5.1, 0.6, -15.65], 180, 1),
    "PROP_FAMILY_CHAIR": ("L0_FAMILY", [4.15, 0.6, -26.15], 0, 1),
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def position_accessors(document: dict) -> list[int]:
    return sorted({
        primitive["attributes"]["POSITION"]
        for mesh in document["meshes"][:3]
        for primitive in mesh["primitives"]
    })


def source_bounds(document: dict) -> tuple[list[float], list[float]]:
    accessors = [document["accessors"][index] for index in position_accessors(document)]
    if any("min" not in accessor or "max" not in accessor for accessor in accessors):
        raise ValueError("source POSITION accessors do not carry authoritative bounds")
    return ([min(accessor["min"][axis] for accessor in accessors) for axis in range(3)],
            [max(accessor["max"][axis] for accessor in accessors) for axis in range(3)])


def translate_positions(document: dict, blob: bytearray,
                        offset: tuple[float, float, float]) -> None:
    for accessor_index in position_accessors(document):
        accessor = document["accessors"][accessor_index]
        if (accessor.get("componentType") != 5126 or accessor.get("type") != "VEC3" or
                "sparse" in accessor):
            raise ValueError(f"POSITION accessor {accessor_index} is not a writable FLOAT/VEC3")
        view = document["bufferViews"][accessor["bufferView"]]
        stride = view.get("byteStride", 12)
        if view.get("buffer", 0) != 0 or stride < 12:
            raise ValueError(f"POSITION accessor {accessor_index} has an unsupported layout")
        start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        for vertex in range(accessor["count"]):
            address = start + vertex * stride
            value = struct.unpack_from("<3f", blob, address)
            struct.pack_into("<3f", blob, address,
                             value[0] + offset[0], value[1] + offset[1],
                             value[2] + offset[2])
        accessor["min"] = [accessor["min"][axis] + offset[axis] for axis in range(3)]
        accessor["max"] = [accessor["max"][axis] + offset[axis] for axis in range(3)]


def append_collision(document: dict, blob: bytearray,
                     minimum: list[float], maximum: list[float]) -> None:
    vertices = [
        (minimum[0], minimum[1], minimum[2]),
        (maximum[0], minimum[1], minimum[2]),
        (maximum[0], maximum[1], minimum[2]),
        (minimum[0], maximum[1], minimum[2]),
        (minimum[0], minimum[1], maximum[2]),
        (maximum[0], minimum[1], maximum[2]),
        (maximum[0], maximum[1], maximum[2]),
        (minimum[0], maximum[1], maximum[2]),
    ]
    indices = (
        0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
        0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2,
        0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5,
    )
    blob.extend(b"\0" * ((-len(blob)) % 4))
    position_offset = len(blob)
    blob.extend(struct.pack("<24f", *(component for vertex in vertices for component in vertex)))
    position_view = len(document["bufferViews"])
    document["bufferViews"].append({"buffer": 0, "byteOffset": position_offset,
                                    "byteLength": 8 * 12, "target": 34962})
    position_accessor = len(document["accessors"])
    document["accessors"].append({
        "bufferView": position_view, "componentType": 5126, "count": 8, "type": "VEC3",
        "min": minimum, "max": maximum,
    })

    index_offset = len(blob)
    blob.extend(struct.pack("<36H", *indices))
    index_view = len(document["bufferViews"])
    document["bufferViews"].append({"buffer": 0, "byteOffset": index_offset,
                                    "byteLength": 36 * 2, "target": 34963})
    index_accessor = len(document["accessors"])
    document["accessors"].append({
        "bufferView": index_view, "componentType": 5123, "count": 36, "type": "SCALAR",
        "min": [0], "max": [7],
    })
    document["meshes"].append({
        "name": COLLISION_NAME,
        "primitives": [{"attributes": {"POSITION": position_accessor},
                        "indices": index_accessor}],
    })
    document["nodes"].append({"name": COLLISION_NAME, "mesh": len(document["meshes"]) - 1})
    document["scenes"][document.get("scene", 0)]["nodes"].append(len(document["nodes"]) - 1)


def prepared_bytes(source: Path) -> bytes:
    raw = source.read_bytes()
    if len(raw) != SOURCE_BYTES or digest(raw) != SOURCE_SHA256:
        raise ValueError(
            f"{source}: not pinned source {SOURCE_SHA256} ({SOURCE_BYTES} bytes); URL {SOURCE_URL}"
        )
    source_document, source_blob = gltf_io.read_model(source)
    document = copy.deepcopy(source_document)
    if [node.get("name") for node in document.get("nodes", [])] != [
            "SheenChair_fabric", "SheenChair_wood", "SheenChair_metal", "SheenChair_label"]:
        raise ValueError("pinned source node roles changed")
    if len(document.get("meshes", [])) != 4 or len(document.get("materials", [])) != 6:
        raise ValueError("pinned source mesh/material structure changed")
    if position_accessors(document) != [1, 6, 11]:
        raise ValueError("pinned source POSITION streams changed")
    if [document["meshes"][index]["primitives"][0].get("material") for index in range(3)] != [0, 1, 2]:
        raise ValueError("pinned source default mango/wood/metal roles changed")
    geometry_views = {document["accessors"][index]["bufferView"] for index in range(15)}
    if geometry_views != {0, 1, 2}:
        raise ValueError("pinned source furniture geometry buffer layout changed")

    geometry_end = max(document["bufferViews"][index].get("byteOffset", 0) +
                       document["bufferViews"][index]["byteLength"] for index in geometry_views)
    blob = bytearray(source_blob[:geometry_end])
    old_minimum, old_maximum = source_bounds(document)
    centre_x = (old_minimum[0] + old_maximum[0]) * 0.5
    centre_z = (old_minimum[2] + old_maximum[2]) * 0.5
    translate_positions(document, blob, (-centre_x, -old_minimum[1], -centre_z))
    minimum, maximum = source_bounds(document)

    document["meshes"] = copy.deepcopy(document["meshes"][:3])
    for mesh in document["meshes"]:
        for primitive in mesh["primitives"]:
            primitive.pop("extensions", None)
    document["materials"] = copy.deepcopy(document["materials"][:3])
    for material in document["materials"]:
        pbr = material.get("pbrMetallicRoughness", {})
        material.pop("extensions", None)
        material.pop("normalTexture", None)
        material.pop("occlusionTexture", None)
        material["pbrMetallicRoughness"] = {
            "baseColorFactor": pbr.get("baseColorFactor", [1, 1, 1, 1]),
            "metallicFactor": pbr.get("metallicFactor", 0),
            "roughnessFactor": pbr.get("roughnessFactor", 1),
        }
    document["nodes"] = copy.deepcopy(document["nodes"][:3])
    document["scenes"] = [{"nodes": [0, 1, 2]}]
    document["scene"] = 0
    document["accessors"] = copy.deepcopy(document["accessors"][:15])
    document["bufferViews"] = copy.deepcopy(document["bufferViews"][:3])
    for key in ("extensions", "extensionsUsed", "extensionsRequired", "images", "textures",
                "samplers"):
        document.pop(key, None)
    document["asset"]["generator"] = "cna-house visual_slice_chair_prepare.py"
    append_collision(document, blob, minimum, maximum)
    document["buffers"] = [{"byteLength": len(blob)}]

    with tempfile.TemporaryDirectory(prefix="house01055-chair-", dir="/tmp") as scratch:
        output = Path(scratch) / "sheen_chair.glb"
        return gltf_io.write_glb(output, document, bytes(blob))


def visible(document: dict) -> tuple[list[dict], set[str]]:
    nodes = [node for node in document["nodes"] if "mesh" in node and
             not node.get("name", "").endswith("_COL")]
    primitives = [primitive for node in nodes
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    return primitives, {node.get("name", "") for node in nodes}


def validate_committed(path: Path, row: dict) -> None:
    data = path.read_bytes()
    if digest(data) != row["sourceSha256"]:
        raise ValueError("committed chair hash differs from assets.manifest.json")
    if row.get("materialMap") != EXPECTED_MATERIAL_MAP:
        raise ValueError("manifest does not preserve the three verified chair material roles")
    origin = row.get("origin", {})
    if (origin.get("author") != "Wayfair, LLC" or origin.get("licence") != "CC0-1.0" or
            origin.get("licenceFile") != "licenses/cc0-1.0/LICENCE.txt" or
            SOURCE_SHA256 not in origin.get("note", "")):
        raise ValueError("manifest does not preserve pinned Wayfair/CC0 provenance")
    document, _blob = gltf_io.read_model(path)
    if any(key in document for key in ("extensions", "extensionsUsed", "extensionsRequired",
                                       "images", "textures", "samplers")):
        raise ValueError("prepared chair retains unsupported or unused runtime metadata")
    primitives, nodes = visible(document)
    if nodes != VISIBLE_NODES:
        raise ValueError(f"unexpected visible chair nodes {sorted(nodes)}")
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives}
    if materials != VISIBLE_MATERIALS:
        raise ValueError(f"unexpected visible chair materials {sorted(materials)}")
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise ValueError("a close-range chair component lacks UV0")
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    if triangles != EXPECTED_TRIANGLES or row["geometry"]["triangles"]["LOD0"] != triangles:
        raise ValueError(f"visible triangle count is {triangles}, expected {EXPECTED_TRIANGLES}")
    all_nodes = {node.get("name", ""): node for node in document["nodes"]}
    if row["geometry"].get("collision") != COLLISION_NAME or COLLISION_NAME not in all_nodes:
        raise ValueError("declared deterministic collision proxy is missing")
    collision = all_nodes[COLLISION_NAME]
    collision_triangles = sum(
        document["accessors"][primitive["indices"]]["count"] // 3
        for primitive in document["meshes"][collision["mesh"]]["primitives"])
    if collision_triangles != 12:
        raise ValueError(f"collision proxy has {collision_triangles} triangles, expected 12")

    bounds = scale_check.accessor_bounds(document)
    measured = [round(high - low, 6) for low, high in zip(*bounds)]
    if measured != EXPECTED_BOUNDS or row["geometry"]["boundsMetres"] != measured:
        raise ValueError(f"chair bounds {measured} differ from expected {EXPECTED_BOUNDS}")
    problems = scale_check.check(path, row["category"], row["geometry"])
    if problems:
        raise ValueError("; ".join(problems))
    problems = origin_check.check(path, row["category"])
    if problems:
        raise ValueError("; ".join(problems))
    problems = gltf_validate.validate(path, gltf_validate.find_cna_content())
    if problems:
        raise ValueError("; ".join(problems))


def validate_world() -> None:
    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    for prop_id, (cell, position, yaw, scale) in EXPECTED_PROPS.items():
        prop = props[prop_id]
        if (prop["asset"] != ASSET_ID or prop["cell"] != cell or
                prop["position"] != position or prop["yawDeg"] != yaw or
                prop["scale"] != scale or prop["collision"] != "proxy"):
            raise ValueError(f"{prop_id} placement, scale or collision contract changed")
    if props["PROP_LIVING_SOFA"]["asset"] != "MODEL_FURNITURE_FORMAL_SOFA":
        raise ValueError("chair replacement changed the formal-living sofa")
    if props["PROP_FAMILY_SOFA"]["asset"] != "MODEL_FAMILY_GLAM_VELVET_SOFA":
        raise ValueError("chair replacement changed the family sofa")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    parser.add_argument("--source", type=Path,
                        help="pinned upstream SheenChair.glb; required for --write")
    options = parser.parse_args()
    try:
        generated = prepared_bytes(options.source) if options.source is not None else None
        if options.write:
            if generated is None:
                parser.error("--write requires --source")
            TARGET.parent.mkdir(parents=True, exist_ok=True)
            TARGET.write_bytes(generated)
            print(f"visual_slice_chair_prepare: wrote {TARGET.relative_to(REPO)} "
                  f"{len(generated)} bytes sha256:{digest(generated)}")
            return 0
        if not TARGET.is_file():
            raise ValueError(f"committed chair is missing: {TARGET}")
        committed = TARGET.read_bytes()
        if generated is not None and generated != committed:
            raise ValueError("committed chair differs from deterministic pinned-source preparation")
        rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
        validate_committed(TARGET, rows[ASSET_ID])
        validate_world()
        print(f"visual_slice_chair_prepare: {len(committed)} bytes {digest(committed)[:16]} "
              f"deterministic, {EXPECTED_TRIANGLES} visible triangles, 12 proxy triangles")
    except (KeyError, OSError, ValueError, gltf_io.GltfError) as error:
        print(f"visual_slice_chair_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
