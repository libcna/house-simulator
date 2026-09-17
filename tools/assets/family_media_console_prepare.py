#!/usr/bin/env python3
"""Prepare and verify HOUSE-01053's pinned CC0 Bedroom low cabinet.

The upstream scene is Draco-compressed.  Preparation therefore starts from the exact deterministic
Draco-decoded, node-extracted intermediate produced by living-room-simulator's glTF-Transform
recipe, while separately verifying the exact upstream Bedroom GLB.  It keeps both visible meshes
and UV0, strips source textures/extensions, adapts the chest's proportions into a low console,
recentres/grounds it and appends a two-box, twenty-four-triangle collision proxy. Runtime finishes remain
canonical cna-house materials.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
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

UPSTREAM_URL = (
    "https://raw.githubusercontent.com/gkjohnson/3d-demo-data/main/models/"
    "bitterli-rendering-resources/bedroom.glb"
)
UPSTREAM_SHA256 = "30e26c0fe67da6b73612284598952011f7dc7f7319d5d9a802112147226ad2ba"
UPSTREAM_BYTES = 4_892_652
DECODED_SHA256 = "51bb4602e858dc8155ed6dca1ddab80ff459d5ff710a41e4c1a15340782054cc"
DECODED_BYTES = 3_252_936
TARGET = (REPO / "assets-src" / "Models" / "Furniture" / "Family" /
          "media_console.glb")
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSET_ID = "MODEL_FAMILY_MEDIA_CONSOLE"
COLLISION_NAME = "FamilyMediaConsole_COL"
VISIBLE_NODES = {"WoodFurniture_0004", "StainlessSmooth_0003"}
VISIBLE_MATERIALS = {"MEDIA_CONSOLE_WOOD", "MEDIA_CONSOLE_HARDWARE"}
EXPECTED_MATERIAL_MAP = {
    "MEDIA_CONSOLE_WOOD": "MAT_FURNITURE_PIANO_WOOD",
    "MEDIA_CONSOLE_HARDWARE": "MAT_KITCHEN_HARDWARE_STEEL",
}
EXPECTED_TRIANGLES = 780
MODEL_SCALE = (1.55, 1.10, 1.55)
# Keep the central solid case physical while leaving the 0.22 m end overhangs out of the player
# proxy.  This matches the prior wall unit's measured 1.097432 m circulation footprint and avoids
# making the family-to-hall route narrower merely because the replacement has a wider top/case.
COLLISION_HALF_WIDTH = 0.5487159490585327
COLLISION_PLINTH_HEIGHT = 0.20
COLLISION_COMPONENT_GAP = 0.001


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def position_accessors(document: dict) -> list[int]:
    return sorted({primitive["attributes"]["POSITION"]
                   for mesh in document["meshes"]
                   for primitive in mesh["primitives"]})


def normal_accessors(document: dict) -> list[int]:
    return sorted({primitive["attributes"]["NORMAL"]
                   for mesh in document["meshes"]
                   for primitive in mesh["primitives"]})


def source_bounds(document: dict) -> tuple[list[float], list[float]]:
    accessors = [document["accessors"][index] for index in position_accessors(document)]
    if any("min" not in accessor or "max" not in accessor for accessor in accessors):
        raise ValueError("source POSITION accessors do not carry authoritative bounds")
    return ([min(accessor["min"][axis] for accessor in accessors) for axis in range(3)],
            [max(accessor["max"][axis] for accessor in accessors) for axis in range(3)])


def transform_positions(document: dict, blob: bytearray,
                        centre_x: float, floor_y: float, centre_z: float) -> None:
    offsets = (-centre_x, -floor_y, -centre_z)
    for accessor_index in position_accessors(document):
        accessor = document["accessors"][accessor_index]
        if (accessor.get("componentType") != 5126 or accessor.get("type") != "VEC3" or
                "sparse" in accessor):
            raise ValueError(f"POSITION accessor {accessor_index} is not writable FLOAT/VEC3")
        view = document["bufferViews"][accessor["bufferView"]]
        stride = view.get("byteStride", 12)
        if view.get("buffer", 0) != 0 or stride < 12:
            raise ValueError(f"POSITION accessor {accessor_index} has unsupported layout")
        start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        for vertex in range(accessor["count"]):
            address = start + vertex * stride
            value = struct.unpack_from("<3f", blob, address)
            transformed = tuple((value[axis] + offsets[axis]) * MODEL_SCALE[axis]
                                for axis in range(3))
            struct.pack_into("<3f", blob, address, *transformed)
        accessor["min"] = [(accessor["min"][axis] + offsets[axis]) * MODEL_SCALE[axis]
                           for axis in range(3)]
        accessor["max"] = [(accessor["max"][axis] + offsets[axis]) * MODEL_SCALE[axis]
                           for axis in range(3)]


def transform_normals(document: dict, blob: bytearray) -> None:
    """Apply the inverse-transpose of MODEL_SCALE and keep normals unit length."""
    for accessor_index in normal_accessors(document):
        accessor = document["accessors"][accessor_index]
        if (accessor.get("componentType") != 5126 or accessor.get("type") != "VEC3" or
                "sparse" in accessor):
            raise ValueError(f"NORMAL accessor {accessor_index} is not writable FLOAT/VEC3")
        view = document["bufferViews"][accessor["bufferView"]]
        stride = view.get("byteStride", 12)
        if view.get("buffer", 0) != 0 or stride < 12:
            raise ValueError(f"NORMAL accessor {accessor_index} has unsupported layout")
        start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        transformed_values = []
        for vertex in range(accessor["count"]):
            address = start + vertex * stride
            value = struct.unpack_from("<3f", blob, address)
            transformed = tuple(value[axis] / MODEL_SCALE[axis] for axis in range(3))
            length = math.sqrt(sum(component * component for component in transformed))
            if length <= 1.0e-12:
                raise ValueError(f"NORMAL accessor {accessor_index} contains a zero normal")
            normal = tuple(component / length for component in transformed)
            struct.pack_into("<3f", blob, address, *normal)
            transformed_values.append(normal)
        if "min" in accessor:
            accessor["min"] = [min(value[axis] for value in transformed_values)
                               for axis in range(3)]
        if "max" in accessor:
            accessor["max"] = [max(value[axis] for value in transformed_values)
                               for axis in range(3)]


def append_collision(document: dict, blob: bytearray,
                     minimum: list[float], maximum: list[float]) -> None:
    split_y = minimum[1] + COLLISION_PLINTH_HEIGHT
    if not minimum[1] < split_y < maximum[1]:
        raise ValueError("media-console collision plinth does not fit inside visible bounds")
    half_gap = COLLISION_COMPONENT_GAP * 0.5
    boxes = ((minimum, [maximum[0], split_y - half_gap, maximum[2]]),
             ([minimum[0], split_y + half_gap, minimum[2]], maximum))
    vertices = []
    indices = []
    box_indices = (
        0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
        0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2,
        0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5,
    )
    for low, high in boxes:
        base = len(vertices)
        vertices.extend([
            (low[0], low[1], low[2]), (high[0], low[1], low[2]),
            (high[0], high[1], low[2]), (low[0], high[1], low[2]),
            (low[0], low[1], high[2]), (high[0], low[1], high[2]),
            (high[0], high[1], high[2]), (low[0], high[1], high[2]),
        ])
        indices.extend(base + index for index in box_indices)
    blob.extend(b"\0" * ((-len(blob)) % 4))
    position_offset = len(blob)
    blob.extend(struct.pack("<48f", *(component for vertex in vertices for component in vertex)))
    position_view = len(document["bufferViews"])
    document["bufferViews"].append({"buffer": 0, "byteOffset": position_offset,
                                    "byteLength": 16 * 12, "target": 34962})
    position_accessor = len(document["accessors"])
    document["accessors"].append({
        "bufferView": position_view, "componentType": 5126, "count": 16, "type": "VEC3",
        "min": minimum, "max": maximum,
    })
    index_offset = len(blob)
    blob.extend(struct.pack("<72H", *indices))
    index_view = len(document["bufferViews"])
    document["bufferViews"].append({"buffer": 0, "byteOffset": index_offset,
                                    "byteLength": 72 * 2, "target": 34963})
    index_accessor = len(document["accessors"])
    document["accessors"].append({
        "bufferView": index_view, "componentType": 5123, "count": 72, "type": "SCALAR",
        "min": [0], "max": [15],
    })
    document["meshes"].append({
        "name": COLLISION_NAME,
        "primitives": [{"attributes": {"POSITION": position_accessor},
                        "indices": index_accessor}],
    })
    document["nodes"].append({"name": COLLISION_NAME, "mesh": len(document["meshes"]) - 1})
    document["scenes"][document.get("scene", 0)]["nodes"].append(len(document["nodes"]) - 1)


def prepared_bytes(decoded_source: Path, upstream: Path | None) -> bytes:
    decoded = decoded_source.read_bytes()
    if len(decoded) != DECODED_BYTES or digest(decoded) != DECODED_SHA256:
        raise ValueError(f"{decoded_source}: not the pinned decoded low-cabinet intermediate")
    if upstream is not None:
        raw = upstream.read_bytes()
        if len(raw) != UPSTREAM_BYTES or digest(raw) != UPSTREAM_SHA256:
            raise ValueError(f"{upstream}: not pinned upstream {UPSTREAM_URL}")

    source_document, source_blob = gltf_io.read_model(decoded_source)
    document = copy.deepcopy(source_document)
    if [node.get("name") for node in document.get("nodes", [])] != [
            "WoodFurniture_0004", "StainlessSmooth_0003"]:
        raise ValueError("decoded source node selection changed")
    if (len(document.get("meshes", [])) != 2 or len(document.get("materials", [])) != 2 or
            position_accessors(document) != [3, 6]):
        raise ValueError("decoded source mesh/material/accessor structure changed")
    if len(document.get("bufferViews", [])) != 6:
        raise ValueError("decoded source buffer layout changed")
    geometry_views = document["bufferViews"][3:]
    geometry_end = max(view.get("byteOffset", 0) + view["byteLength"]
                       for view in geometry_views)
    if geometry_end != 56_392:
        raise ValueError(f"decoded geometry ends at {geometry_end}, expected 56392")
    blob = bytearray(source_blob[:geometry_end])
    document["bufferViews"] = copy.deepcopy(geometry_views)
    for accessor in document["accessors"]:
        accessor["bufferView"] -= 3

    minimum, maximum = source_bounds(document)
    transform_positions(document, blob, (minimum[0] + maximum[0]) * 0.5, minimum[1],
                        (minimum[2] + maximum[2]) * 0.5)
    transform_normals(document, blob)
    minimum, maximum = source_bounds(document)
    document["materials"] = [
        {"name": "MEDIA_CONSOLE_WOOD", "doubleSided": False,
         "pbrMetallicRoughness": {"baseColorFactor": [0.13, 0.055, 0.025, 1.0],
                                  "metallicFactor": 0.0, "roughnessFactor": 0.72}},
        {"name": "MEDIA_CONSOLE_HARDWARE", "doubleSided": False,
         "pbrMetallicRoughness": {"baseColorFactor": [0.12, 0.13, 0.14, 1.0],
                                  "metallicFactor": 0.72, "roughnessFactor": 0.34}},
    ]
    for key in ("extensions", "extensionsUsed", "extensionsRequired", "images", "textures",
                "samplers"):
        document.pop(key, None)
    document["asset"]["generator"] = "cna-house family_media_console_prepare.py"
    collision_minimum = list(minimum)
    collision_maximum = list(maximum)
    collision_minimum[2] = max(collision_minimum[2], -COLLISION_HALF_WIDTH)
    collision_maximum[2] = min(collision_maximum[2], COLLISION_HALF_WIDTH)
    append_collision(document, blob, collision_minimum, collision_maximum)
    document["buffers"] = [{"byteLength": len(blob)}]
    with tempfile.TemporaryDirectory(prefix="house01053-media-", dir="/tmp") as scratch:
        output = Path(scratch) / "media_console.glb"
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
        raise ValueError("committed media-console hash differs from assets.manifest.json")
    if row.get("materialMap") != EXPECTED_MATERIAL_MAP:
        raise ValueError("manifest does not preserve the two verified console material roles")
    origin = row.get("origin", {})
    if (origin.get("author") != "SlykDrako" or origin.get("licence") != "CC0-1.0" or
            origin.get("licenceFile") != "licenses/cc0-1.0/LICENCE.txt" or
            UPSTREAM_SHA256 not in origin.get("note", "") or
            DECODED_SHA256 not in origin.get("note", "")):
        raise ValueError("manifest does not preserve pinned SlykDrako/CC0 provenance")
    document, _blob = gltf_io.read_model(path)
    if any(key in document for key in ("extensions", "extensionsUsed", "extensionsRequired",
                                       "images", "textures", "samplers")):
        raise ValueError("prepared console retains unsupported or unused runtime metadata")
    primitives, nodes = visible(document)
    if nodes != VISIBLE_NODES:
        raise ValueError(f"unexpected visible console nodes {sorted(nodes)}")
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives}
    if materials != VISIBLE_MATERIALS:
        raise ValueError(f"unexpected visible console materials {sorted(materials)}")
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise ValueError("a close-range console component lacks UV0")
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    if triangles != EXPECTED_TRIANGLES or row["geometry"]["triangles"]["LOD0"] != triangles:
        raise ValueError(f"visible triangle count is {triangles}, expected {EXPECTED_TRIANGLES}")
    collision_nodes = [node for node in document["nodes"]
                       if node.get("name") == COLLISION_NAME and "mesh" in node]
    if row["geometry"].get("collision") != COLLISION_NAME or len(collision_nodes) != 1:
        raise ValueError("declared deterministic collision proxy is missing")
    collision_triangles = sum(
        document["accessors"][primitive["indices"]]["count"] // 3
        for primitive in document["meshes"][collision_nodes[0]["mesh"]]["primitives"])
    if collision_triangles != 24:
        raise ValueError(f"collision proxy has {collision_triangles} triangles, expected 24")
    collision_primitives = document["meshes"][collision_nodes[0]["mesh"]]["primitives"]
    collision_positions = document["accessors"][
        collision_primitives[0]["attributes"]["POSITION"]]
    collision_size = [round(high - low, 6) for low, high in
                      zip(collision_positions["min"], collision_positions["max"])]
    if collision_size != [0.465029, 0.65386, 1.097432]:
        raise ValueError(f"collision proxy dimensions changed: {collision_size}")
    bounds = scale_check.accessor_bounds(document)
    measured = [round(high - low, 6) for low, high in zip(*bounds)]
    if any(abs(actual - stored) > 0.00001
           for actual, stored in zip(measured, row["geometry"]["boundsMetres"])):
        raise ValueError(f"manifest bounds {row['geometry']['boundsMetres']} differ from {measured}")
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
    console = props["PROP_FAMILY_TV_UNIT"]
    if (console["asset"] != ASSET_ID or console["cell"] != "L0_FAMILY" or
            console["position"] != [4.1, 0.6, -22.55] or console["yawDeg"] != 90 or
            console["scale"] != 1 or console["collision"] != "proxy"):
        raise ValueError("family media-console canonical placement or collision changed")
    television = props["PROP_FAMILY_TV"]
    if (television["asset"] != "MODEL_FURNITURE_TV" or
            television["position"] != [4.1, 1.315, -22.26] or
            television["yawDeg"] != 90 or television["scale"] != 1 or
            television["collision"] != "none"):
        raise ValueError("approved television or measured console gap changed")
    uses = [prop["id"] for prop in props.values() if prop["asset"] == ASSET_ID]
    if uses != ["PROP_FAMILY_TV_UNIT"]:
        raise ValueError(f"media console must remain family-only, got {uses}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    parser.add_argument("--decoded-source", type=Path,
                        help="pinned decoded low-cabinet.glb intermediate")
    parser.add_argument("--upstream", type=Path, help="pinned upstream Bedroom GLB")
    options = parser.parse_args()
    try:
        if options.write and (options.decoded_source is None or options.upstream is None):
            parser.error("--write requires --decoded-source and --upstream")
        generated = (prepared_bytes(options.decoded_source, options.upstream)
                     if options.decoded_source is not None else None)
        if options.write:
            TARGET.parent.mkdir(parents=True, exist_ok=True)
            TARGET.write_bytes(generated)
            print(f"family_media_console_prepare: wrote {TARGET.relative_to(REPO)} "
                  f"{len(generated)} bytes sha256:{digest(generated)}")
            return 0
        if not TARGET.is_file():
            raise ValueError(f"committed media console is missing: {TARGET}")
        committed = TARGET.read_bytes()
        if generated is not None and generated != committed:
            raise ValueError("committed console differs from deterministic pinned-source preparation")
        rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
        validate_committed(TARGET, rows[ASSET_ID])
        validate_world()
        print(f"family_media_console_prepare: {len(committed)} bytes "
              f"{digest(committed)[:16]} deterministic, {EXPECTED_TRIANGLES} visible triangles, "
              "24 proxy triangles")
    except (KeyError, OSError, ValueError, gltf_io.GltfError) as error:
        print(f"family_media_console_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
