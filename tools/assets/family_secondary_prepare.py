#!/usr/bin/env python3
"""Regenerate and validate HOUSE-01051's family-room secondary suite."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))
import blender_env  # noqa: E402
sys.path.insert(0, str(REPO / "tools" / "assets"))
import gltf_io  # noqa: E402
import origin_check  # noqa: E402
import scale_check  # noqa: E402
sys.path.insert(0, str(REPO / "tools" / "world"))
import layout_io  # noqa: E402

SCRIPT = REPO / "tools" / "blender" / "family_secondary_suite.py"
TARGET = REPO / "assets-src" / "Models" / "Furniture" / "Family"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSETS = {
    "dog_bed": ("MODEL_FAMILY_DOG_BED", "dog_bed.glb", "pet-bed", None),
    "bookcase": ("MODEL_FAMILY_BOOKCASE", "bookcase.glb", "bookcase",
                 "family_bookcase_COL"),
    "side_table": ("MODEL_FAMILY_SIDE_TABLE", "side_table.glb", "occasional-table",
                    "family_side_table_COL"),
    "wall_art": ("MODEL_FAMILY_WALL_ART", "wall_art.glb", "picture", None),
}
EXPECTED_MATERIALS = {
    "dog_bed": {"FAMILY_TEXTILE", "FAMILY_ART", "FAMILY_PIPING"},
    "bookcase": {"FAMILY_WOOD", "FAMILY_TEXTILE", "FAMILY_BOOK", "FAMILY_ART"},
    "side_table": {"FAMILY_WOOD", "FAMILY_BOOK"},
    "wall_art": {"FAMILY_WOOD", "FAMILY_TEXTILE", "FAMILY_BOOK", "FAMILY_ART"},
}
REQUIRED_COMPONENTS = {
    "dog_bed": ("support_pad", "inner_cushion", "back_bolster", "side_bolster",
                "base_piping", "cushion_piping", "cushion_button"),
    "bookcase": ("back", "side_", "shelf_", "book_"),
    "side_table": ("top", "lower_shelf", "leg_", "reading_book"),
    "wall_art": ("canvas", "frame_vertical", "frame_horizontal", "art_field"),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(folder: Path, name: str) -> Path:
    output = folder / ASSETS[name][1]
    command = blender_env.build_command(SCRIPT, ["--asset", name, "--out", str(output)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot regenerate the family secondary suite")
    result = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                            capture_output=True, text=True, check=False)
    if result.returncode != 0 or "family_secondary_suite: EXIT 0" not in result.stdout.splitlines():
        tail = "\n".join((result.stdout + "\n" + result.stderr).splitlines()[-40:])
        raise RuntimeError(f"Blender failed for {name} (exit {result.returncode}):\n{tail}")
    if not output.is_file():
        raise RuntimeError(f"generation produced no {output.name}")
    return output


def visible(document: dict) -> tuple[list[dict], set[str]]:
    nodes = [node for node in document["nodes"] if "mesh" in node and
             not node.get("name", "").endswith("_COL")]
    primitives = [primitive for node in nodes
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    return primitives, {node.get("name", "") for node in nodes}


def validate(path: Path, name: str, row: dict) -> tuple[list[float], int]:
    document, _ = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError(f"{name}: no authoritative POSITION bounds")
    measured = [round(high - low, 6) for low, high in zip(*bounds)]
    declared = row["geometry"]["boundsMetres"]
    if any(abs(actual - stored) > 0.005 for actual, stored in zip(measured, declared)):
        raise RuntimeError(f"{name}: manifest bounds {declared} differ from {measured}")
    problems = scale_check.check(path, ASSETS[name][2], row["geometry"])
    if problems:
        raise RuntimeError(f"{name}: {'; '.join(problems)}")
    problems = origin_check.check(path, ASSETS[name][2])
    if problems:
        raise RuntimeError(f"{name}: {'; '.join(problems)}")

    primitives, names = visible(document)
    for fragment in REQUIRED_COMPONENTS[name]:
        if not any(fragment in node for node in names):
            raise RuntimeError(f"{name}: missing authored component {fragment}")
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives if "material" in primitive}
    if materials != EXPECTED_MATERIALS[name]:
        raise RuntimeError(f"{name}: unexpected finishes {sorted(materials)}")
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise RuntimeError(f"{name}: a close-range component lacks metre-scaled UV0")
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    if triangles != row["geometry"]["triangles"]["LOD0"]:
        raise RuntimeError(f"{name}: manifest LOD0 triangle count differs from {triangles}")

    collision = ASSETS[name][3]
    all_nodes = {node.get("name", ""): node for node in document["nodes"]}
    if collision is None:
        if any(node.endswith("_COL") for node in all_nodes):
            raise RuntimeError(f"{name}: non-collidable asset contains a collision proxy")
    else:
        if row["geometry"].get("collision") != collision or collision not in all_nodes:
            raise RuntimeError(f"{name}: declared collision proxy is missing")
        node = all_nodes[collision]
        collision_triangles = sum(
            document["accessors"][primitive["indices"]]["count"] // 3
            for primitive in document["meshes"][node["mesh"]]["primitives"])
        if collision_triangles > 64:
            raise RuntimeError(f"{name}: collision proxy has {collision_triangles} triangles")
    return measured, triangles


def validate_world() -> None:
    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props", "nav"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    expected = {
        "PROP_FAMILY_DOG_BED": ("MODEL_FAMILY_DOG_BED", [3.20, 0.6, -24.00], 0, "none"),
        "PROP_FAMILY_BOOKCASE": ("MODEL_FAMILY_BOOKCASE", [7.95, 0.6, -22.20], 0, "none"),
        "PROP_FAMILY_SIDE_TABLE": ("MODEL_FAMILY_SIDE_TABLE", [7.72, 0.6, -26.00], 0,
                                   "proxy"),
        "PROP_FAMILY_WALL_ART": ("MODEL_FAMILY_WALL_ART", [5.45, 2.62, -22.22], 0, "none"),
    }
    for prop_id, (asset, position, yaw, collision) in expected.items():
        prop = props[prop_id]
        if prop["asset"] != asset or prop["cell"] != "L0_FAMILY" or \
                prop["position"] != position or prop["yawDeg"] != yaw or \
                prop["collision"] != collision:
            raise RuntimeError(f"{prop_id}: canonical family placement changed")
    plant = props["PROP_FAMILY_PLANT"]
    if plant["position"] != [7.95, 2.15, -22.2]:
        raise RuntimeError("PROP_FAMILY_PLANT is no longer dressed on the bookcase")
    beds = layout_io.by_id(layout["nav"].get("beds", []), "bed")
    dog_bed = beds["BED_DOG_FAMILY"]
    if dog_bed["prop"] != "PROP_FAMILY_DOG_BED" or dog_bed["position"] != [3.20, 0.66, -24.00]:
        raise RuntimeError("BED_DOG_FAMILY is no longer linked to its physical prop")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    options = parser.parse_args()
    try:
        rows = ({row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
                if options.check else {})
        with tempfile.TemporaryDirectory(prefix="house01051-family-", dir="/tmp") as scratch:
            for name, (asset_id, filename, _category, collision) in ASSETS.items():
                generated = generate(Path(scratch), name)
                if options.write:
                    target = TARGET / filename
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(generated.read_bytes())
                    document, _ = gltf_io.read_model(target)
                    bounds = scale_check.accessor_bounds(document)
                    primitives, _names = visible(document)
                    triangles = sum(document["accessors"][p["indices"]]["count"] // 3
                                    for p in primitives)
                    measured = [round(high - low, 6) for low, high in zip(*bounds)]
                    print(f"family_secondary_prepare: {name} bounds={measured} "
                          f"triangles={triangles} collision={collision} sha256={sha256(target)}")
                    continue
                target = TARGET / filename
                if not target.is_file():
                    raise RuntimeError(f"committed family model is missing: {target}")
                actual = sha256(generated)
                committed = sha256(target)
                row = rows[asset_id]
                if actual != committed or actual != row["sourceSha256"]:
                    raise RuntimeError(
                        f"{name}: regenerated={actual}, committed={committed}, "
                        f"manifest={row['sourceSha256']}")
                measured, triangles = validate(target, name, row)
                print(f"family_secondary_prepare: {name} {triangles} triangles {measured} m "
                      f"{actual[:16]} deterministic")
            if options.check:
                validate_world()
    except (KeyError, OSError, RuntimeError, TypeError, ValueError) as error:
        print(f"family_secondary_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
