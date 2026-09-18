#!/usr/bin/env python3
"""Regenerate and validate HOUSE-01063's formal-living surface dressing."""

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
import gltf_io  # noqa: E402
import origin_check  # noqa: E402
import scale_check  # noqa: E402
sys.path.insert(0, str(REPO / "tools" / "world"))
import layout_io  # noqa: E402

SCRIPT = REPO / "tools" / "blender" / "living_surface_dressing.py"
TARGET = REPO / "assets-src" / "Models" / "Furniture" / "LivingRoom"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSETS = {
    "art": ("MODEL_LIVING_WALL_ART_PAIR", "living_wall_art_pair.glb", "picture", None),
    "table": ("MODEL_LIVING_TABLE_DRESSING", "living_table_dressing.glb",
              "tabletop-decor", None),
}
EXPECTED_MATERIALS = {
    "art": {"DRESSING_WOOD", "DRESSING_BRASS", "DRESSING_CANVAS", "DRESSING_CREAM"},
    "table": {"DRESSING_BRASS", "DRESSING_CREAM", "DRESSING_BOOK", "DRESSING_STONE"},
}
EXPECTED_MAPS = {
    "art": {
        "DRESSING_WOOD": "MAT_FURNITURE_PIANO_WOOD",
        "DRESSING_BRASS": "MAT_FURNITURE_PIANO_BRASS",
        "DRESSING_CANVAS": "MAT_FAMILY_ART_CANVAS",
        "DRESSING_CREAM": "MAT_LIVING_ART_PAPER",
    },
    "table": {
        "DRESSING_BRASS": "MAT_FURNITURE_PIANO_BRASS",
        "DRESSING_CREAM": "MAT_LIVING_ART_PAPER",
        "DRESSING_BOOK": "MAT_FAMILY_BOOK_SPINES",
        "DRESSING_STONE": "MAT_KITCHEN_COUNTER_STONE",
    },
}
REQUIRED_COMPONENTS = {
    "art": ("frame_vertical", "canvas", "botanical_stem", "botanical_leaf"),
    "table": ("book_lower_pages", "book_upper_cover", "tray_rim", "sculptural_bowl"),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(folder: Path, name: str) -> Path:
    output = folder / ASSETS[name][1]
    command = blender_env.build_command(SCRIPT, ["--asset", name, "--out", str(output)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot regenerate living surface dressing")
    result = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                            capture_output=True, text=True, check=False)
    if result.returncode != 0 or \
            "living_surface_dressing: EXIT 0" not in result.stdout.splitlines():
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


def inspect(path: Path) -> tuple[list[float], int, set[str], set[str]]:
    document, _ = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError(f"{path.name}: no authoritative POSITION bounds")
    measured = [round(high - low, 6) for low, high in zip(*bounds)]
    primitives, names = visible(document)
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives if "material" in primitive}
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise RuntimeError(f"{path.name}: a close-range component lacks metre-scaled UV0")
    return measured, triangles, materials, names


def validate(path: Path, name: str, row: dict) -> tuple[list[float], int]:
    measured, triangles, materials, names = inspect(path)
    if measured != row["geometry"]["boundsMetres"]:
        raise RuntimeError(f"{name}: manifest bounds differ from measured {measured}")
    if triangles != row["geometry"]["triangles"]["LOD0"]:
        raise RuntimeError(f"{name}: manifest triangle count differs from measured {triangles}")
    if materials != EXPECTED_MATERIALS[name] or row["materialMap"] != EXPECTED_MAPS[name]:
        raise RuntimeError(f"{name}: canonical finishes changed")
    for fragment in REQUIRED_COMPONENTS[name]:
        if not any(fragment in node for node in names):
            raise RuntimeError(f"{name}: missing authored component {fragment}")
    if any(node.endswith("_COL") for node in names) or "collision" in row["geometry"]:
        raise RuntimeError(f"{name}: static surface dressing must not add collision")
    problems = scale_check.check(path, ASSETS[name][2], row["geometry"])
    if problems:
        raise RuntimeError(f"{name}: {'; '.join(problems)}")
    problems = origin_check.check(path, ASSETS[name][2])
    if problems:
        raise RuntimeError(f"{name}: {'; '.join(problems)}")
    return measured, triangles


def validate_world(rows: dict[str, dict]) -> None:
    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["cells", "props"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    art = props["PROP_LIVING_WALL_ART_PAIR"]
    if art["asset"] != ASSETS["art"][0] or art["cell"] != "L0_LIVING" or \
            art["position"] != [-5.2, 2.17, -20.18] or art["yawDeg"] != 180 or \
            art["scale"] != 1 or not art["static"] or art["collision"] != "none":
        raise RuntimeError("paired wall art no longer owns its measured south-wall placement")

    table = props["PROP_LIVING_TABLE_DRESSING"]
    if table["asset"] != ASSETS["table"][0] or table["cell"] != "L0_LIVING" or \
            table["position"] != [-6.3, 1.019303, -17.55] or table["yawDeg"] != 0 or \
            table["scale"] != 1 or not table["static"] or table["collision"] != "none":
        raise RuntimeError("table vignette no longer owns its measured coffee-table placement")

    coffee = props["PROP_LIVING_COFFEE"]
    coffee_row = rows["MODEL_FURNITURE_COFFEE_TABLE"]
    top = coffee["position"][1] + coffee_row["geometry"]["boundsMetres"][1] * coffee["scale"]
    if abs(top - table["position"][1]) > 0.000001:
        raise RuntimeError(f"table vignette floats or clips: table top {top:.6f}")
    cells = layout_io.by_id(layout_io.rows(layout, "cells"), "cell")
    south_wall = cells["L0_LIVING"]["boxes"][0]["z"][0]
    if abs(art["position"][2] - south_wall) > 0.020001:
        raise RuntimeError("paired wall art is no longer within 20 mm of the real south wall")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    options = parser.parse_args()
    try:
        rows = ({row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
                if options.check else {})
        with tempfile.TemporaryDirectory(prefix="house01063-living-dressing-", dir="/tmp") as scratch:
            for name, (asset_id, filename, _category, _collision) in ASSETS.items():
                generated = generate(Path(scratch), name)
                if options.write:
                    target = TARGET / filename
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(generated.read_bytes())
                    measured, triangles, _materials, _names = inspect(target)
                    print(f"living_surface_dressing_prepare: {name} bounds={measured} "
                          f"triangles={triangles} sha256={sha256(target)}")
                    continue
                target = TARGET / filename
                if not target.is_file():
                    raise RuntimeError(f"committed dressing model is missing: {target}")
                actual = sha256(generated)
                committed = sha256(target)
                row = rows[asset_id]
                if actual != committed or actual != row["sourceSha256"]:
                    raise RuntimeError(
                        f"{name}: regenerated={actual}, committed={committed}, "
                        f"manifest={row['sourceSha256']}")
                measured, triangles = validate(target, name, row)
                print(f"living_surface_dressing_prepare: {name} {triangles} triangles "
                      f"{measured} m {actual[:16]} deterministic")
            if options.check:
                validate_world(rows)
    except (KeyError, OSError, RuntimeError, TypeError, ValueError) as error:
        print(f"living_surface_dressing_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
