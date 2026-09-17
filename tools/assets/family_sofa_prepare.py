#!/usr/bin/env python3
"""Prepare and verify HOUSE-01052's pinned Wayfair GlamVelvetSofa source.

The Khronos source contains five material variants, PBR-only material extensions, an embedded
directional light and two embedded maps.  Static house props are rebuilt into canonical material
chunks, so keeping that unsupported runtime metadata would add 3 MB without affecting the shipped
XNA draw.  This tool selects the authored navy role, recentres/grounds the source POSITION data,
removes only unused material data and appends a deterministic twelve-triangle collision box.
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
    "GlamVelvetSofa/glTF-Binary/GlamVelvetSofa.glb"
)
SOURCE_SHA256 = "67202c74a1a33377771f162dc7fad612a6c9bd51ee15124c488e9851d9ac5266"
SOURCE_BYTES = 3_149_844
TARGET = (REPO / "assets-src" / "Models" / "Furniture" / "Family" /
          "glam_velvet_sofa.glb")
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSET_ID = "MODEL_FAMILY_GLAM_VELVET_SOFA"
COLLISION_NAME = "GlamVelvetSofa_COL"
VISIBLE_NODES = {
    "GlamVelvetSofa_legs",
    "GlamVelvetSofa_fabric",
    "GlamVelvetSofa_feet",
}
VISIBLE_MATERIALS = {
    "GlamVelvetSofa_legs",
    "GlamVelvetSofa_fabric_navy",
    "GlamVelvetSofa_feet",
}
EXPECTED_TRIANGLES = 4_196
EXPECTED_MATERIAL_MAP = {
    "GlamVelvetSofa_legs": "MAT_FAMILY_SOFA_FRAME",
    "GlamVelvetSofa_fabric_navy": "MAT_FAMILY_SOFA_VELVET",
    "GlamVelvetSofa_feet": "MAT_KITCHEN_HARDWARE_STEEL",
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def position_accessors(document: dict) -> list[int]:
    return sorted({primitive["attributes"]["POSITION"]
                   for mesh in document["meshes"][:3]
                   for primitive in mesh["primitives"]})


def source_bounds(document: dict) -> tuple[list[float], list[float]]:
    accessors = [document["accessors"][index] for index in position_accessors(document)]
    if any("min" not in accessor or "max" not in accessor for accessor in accessors):
        raise ValueError("source POSITION accessors do not carry authoritative bounds")
    return ([min(accessor["min"][axis] for accessor in accessors) for axis in range(3)],
            [max(accessor["max"][axis] for accessor in accessors) for axis in range(3)])


def translate_positions(document: dict, blob: bytearray, offset: tuple[float, float, float]) -> None:
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
    if [node.get("name") for node in document.get("nodes", [])[:3]] != [
            "GlamVelvetSofa_legs", "GlamVelvetSofa_fabric", "GlamVelvetSofa_feet"]:
        raise ValueError("pinned source node roles changed")
    if len(document.get("meshes", [])) != 3 or len(document.get("materials", [])) != 7:
        raise ValueError("pinned source mesh/material structure changed")
    if document["meshes"][1]["primitives"][0].get("material") != 3:
        raise ValueError("pinned source no longer selects the navy fabric variant")
    if position_accessors(document) != [1, 5, 9]:
        raise ValueError("pinned source POSITION streams changed")
    geometry_views = {document["accessors"][index]["bufferView"]
                      for index in range(len(document["accessors"]))}
    if geometry_views != {0, 1, 2}:
        raise ValueError("pinned source geometry buffer layout changed")

    geometry_end = max(document["bufferViews"][index].get("byteOffset", 0) +
                       document["bufferViews"][index]["byteLength"] for index in geometry_views)
    blob = bytearray(source_blob[:geometry_end])
    old_minimum, old_maximum = source_bounds(document)
    centre_x = (old_minimum[0] + old_maximum[0]) * 0.5
    centre_z = (old_minimum[2] + old_maximum[2]) * 0.5
    translate_positions(document, blob, (-centre_x, -old_minimum[1], -centre_z))
    minimum, maximum = source_bounds(document)

    for mesh in document["meshes"]:
        for primitive in mesh["primitives"]:
            primitive.pop("extensions", None)
    document["meshes"][1]["primitives"][0]["material"] = 2
    document["materials"] = [copy.deepcopy(document["materials"][index])
                             for index in (0, 1, 3)]
    for material in document["materials"]:
        material.pop("extensions", None)
        material.pop("normalTexture", None)
        material.pop("occlusionTexture", None)
        material["pbrMetallicRoughness"] = {
            "baseColorFactor": material.get("pbrMetallicRoughness", {}).get(
                "baseColorFactor", [1, 1, 1, 1]),
            "metallicFactor": material.get("pbrMetallicRoughness", {}).get("metallicFactor", 0),
            "roughnessFactor": material.get("pbrMetallicRoughness", {}).get("roughnessFactor", 1),
        }
    document["nodes"] = copy.deepcopy(document["nodes"][:3])
    document["scenes"] = [{"nodes": [0, 1, 2]}]
    document["scene"] = 0
    document["bufferViews"] = copy.deepcopy(document["bufferViews"][:3])
    document.pop("extensions", None)
    document.pop("extensionsUsed", None)
    document.pop("extensionsRequired", None)
    document.pop("images", None)
    document.pop("textures", None)
    document.pop("samplers", None)
    document["asset"]["generator"] = "cna-house family_sofa_prepare.py"
    append_collision(document, blob, minimum, maximum)
    document["buffers"] = [{"byteLength": len(blob)}]

    with tempfile.TemporaryDirectory(prefix="house01052-sofa-", dir="/tmp") as scratch:
        output = Path(scratch) / "glam_velvet_sofa.glb"
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
        raise ValueError("committed sofa hash differs from assets.manifest.json")
    if row.get("materialMap") != EXPECTED_MATERIAL_MAP:
        raise ValueError("manifest does not preserve the three verified sofa material roles")
    origin = row.get("origin", {})
    if (origin.get("author") != "Wayfair, LLC" or origin.get("licence") != "CC-BY-4.0" or
            origin.get("licenceFile") != "licenses/cc-by-4.0/LICENCE.txt" or
            SOURCE_SHA256 not in origin.get("note", "")):
        raise ValueError("manifest does not preserve pinned Wayfair/CC-BY-4.0 provenance")
    document, _blob = gltf_io.read_model(path)
    if any(key in document for key in ("extensions", "extensionsUsed", "extensionsRequired",
                                       "images", "textures", "samplers")):
        raise ValueError("prepared sofa retains unsupported or unused runtime metadata")
    primitives, nodes = visible(document)
    if nodes != VISIBLE_NODES:
        raise ValueError(f"unexpected visible sofa nodes {sorted(nodes)}")
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives}
    if materials != VISIBLE_MATERIALS:
        raise ValueError(f"unexpected visible sofa materials {sorted(materials)}")
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise ValueError("a close-range sofa component lacks UV0")
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
    declared = row["geometry"]["boundsMetres"]
    if any(abs(actual - stored) > 0.00001 for actual, stored in zip(measured, declared)):
        raise ValueError(f"manifest bounds {declared} differ from {measured}")
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
    family = props["PROP_FAMILY_SOFA"]
    if (family["asset"] != ASSET_ID or family["cell"] != "L0_FAMILY" or
            family["position"] != [6.65, 0.6, -24.75] or family["yawDeg"] != 270 or
            family["scale"] != 1 or family["collision"] != "proxy"):
        raise ValueError("family sofa canonical placement or collision changed")
    living = props["PROP_LIVING_SOFA"]
    if living["asset"] != "MODEL_FURNITURE_LEATHER_SOFA":
        raise ValueError("family-only replacement changed the formal-living sofa")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    parser.add_argument("--source", type=Path,
                        help="pinned upstream GlamVelvetSofa.glb; required for --write")
    options = parser.parse_args()
    try:
        generated = prepared_bytes(options.source) if options.source is not None else None
        if options.write:
            if generated is None:
                parser.error("--write requires --source")
            TARGET.parent.mkdir(parents=True, exist_ok=True)
            TARGET.write_bytes(generated)
            print(f"family_sofa_prepare: wrote {TARGET.relative_to(REPO)} "
                  f"{len(generated)} bytes sha256:{digest(generated)}")
            return 0
        if not TARGET.is_file():
            raise ValueError(f"committed sofa is missing: {TARGET}")
        committed = TARGET.read_bytes()
        if generated is not None and generated != committed:
            raise ValueError("committed sofa differs from deterministic pinned-source preparation")
        rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
        validate_committed(TARGET, rows[ASSET_ID])
        validate_world()
        print(f"family_sofa_prepare: {len(committed)} bytes {digest(committed)[:16]} "
              f"deterministic, {EXPECTED_TRIANGLES} visible triangles, 12 proxy triangles")
    except (KeyError, OSError, ValueError, gltf_io.GltfError) as error:
        print(f"family_sofa_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
