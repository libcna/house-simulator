#!/usr/bin/env python3
"""Regenerate and validate HOUSE-01065's two complementary hall-gallery clusters."""

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

SCRIPT = REPO / "tools" / "blender" / "hall_gallery.py"
TARGET = REPO / "assets-src" / "Models" / "Furniture" / "Hall"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSETS = {
    "west": ("MODEL_HALL_GALLERY_WEST", TARGET / "gallery_west.glb"),
    "east": ("MODEL_HALL_GALLERY_EAST", TARGET / "gallery_east.glb"),
}
EXPECTED_MATERIALS = {"GALLERY_FRAME", "GALLERY_MAT", "GALLERY_IMAGE", "GALLERY_RELIEF"}
EXPECTED_MAP = {
    "GALLERY_FRAME": "MAT_FURNITURE_PIANO_WOOD",
    "GALLERY_MAT": "MAT_LIVING_ART_PAPER",
    "GALLERY_IMAGE": "MAT_FAMILY_ART_CANVAS",
    "GALLERY_RELIEF": "MAT_FURNITURE_PIANO_BRASS",
}
PLACEMENTS = {
    "west": ("PROP_HALL_GALLERY_WEST", [-2.18, 1.95, -19.47], 270),
    "east": ("PROP_HALL_GALLERY_EAST", [2.18, 1.95, -19.47], 90),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(folder: Path, name: str) -> Path:
    output = folder / ASSETS[name][1].name
    command = blender_env.build_command(SCRIPT, ["--asset", name, "--out", str(output)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot regenerate the hall gallery")
    result = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                            capture_output=True, text=True, check=False)
    if result.returncode != 0 or "hall_gallery: EXIT 0" not in result.stdout.splitlines():
        tail = "\n".join((result.stdout + "\n" + result.stderr).splitlines()[-40:])
        raise RuntimeError(f"Blender failed for {name} (exit {result.returncode}):\n{tail}")
    if not output.is_file():
        raise RuntimeError(f"generation produced no {output.name}")
    return output


def inspect(path: Path) -> tuple[list[float], int, set[str], set[str]]:
    document, _ = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError(f"{path.name}: no authoritative POSITION bounds")
    measured = [round(high - low, 6) for low, high in zip(*bounds)]
    nodes = [node for node in document["nodes"] if "mesh" in node]
    primitives = [primitive for node in nodes
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives if "material" in primitive}
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise RuntimeError(f"{path.name}: a close-range component lacks metre-scaled UV0")
    return measured, triangles, materials, {node.get("name", "") for node in nodes}


def validate_asset(path: Path, name: str, row: dict) -> tuple[list[float], int]:
    measured, triangles, materials, names = inspect(path)
    if measured != row["geometry"]["boundsMetres"]:
        raise RuntimeError(f"{name}: manifest bounds differ from measured {measured}")
    if triangles != row["geometry"]["triangles"]["LOD0"]:
        raise RuntimeError(f"{name}: manifest triangle count differs from measured {triangles}")
    if materials != EXPECTED_MATERIALS or row["materialMap"] != EXPECTED_MAP:
        raise RuntimeError(f"{name}: canonical gallery finishes changed")
    frame_count = sum("_back" in node for node in names)
    expected_count = 5 if name == "west" else 4
    if frame_count != expected_count or not any("figure_head" in node for node in names):
        raise RuntimeError(f"{name}: expected {expected_count} complete abstract portraits")
    if "collision" in row["geometry"]:
        raise RuntimeError(f"{name}: wall dressing must not add collision")
    problems = scale_check.check(path, "picture", row["geometry"])
    if problems:
        raise RuntimeError(f"{name}: {'; '.join(problems)}")
    problems = origin_check.check(path, "picture")
    if problems:
        raise RuntimeError(f"{name}: {'; '.join(problems)}")
    return measured, triangles


def validate_world(rows: dict[str, dict]) -> None:
    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["cells", "props", "portals"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    hall = layout_io.by_id(layout_io.rows(layout, "cells"), "cell")["L0_HALL"]["boxes"][0]
    for name, (prop_id, position, yaw) in PLACEMENTS.items():
        asset_id = ASSETS[name][0]
        prop = props[prop_id]
        if prop["asset"] != asset_id or prop["cell"] != "L0_HALL" or \
                prop["position"] != position or prop["yawDeg"] != yaw or \
                prop["scale"] != 1 or not prop["static"] or prop["collision"] != "none":
            raise RuntimeError(f"canonical gallery placement changed: {prop_id}")
        wall_x = hall["x"][0] if name == "west" else hall["x"][1]
        if abs(abs(position[0] - wall_x) - 0.02) > 0.000001:
            raise RuntimeError(f"{prop_id}: gallery back is no longer 20 mm from its wall")
        width, height, _depth = rows[asset_id]["geometry"]["boundsMetres"]
        if position[2] - width * 0.5 < -20.65 or position[2] + width * 0.5 > -18.30:
            raise RuntimeError(f"{prop_id}: gallery overlaps a side-wall doorway")
        if position[1] - height * 0.5 < 1.10 or position[1] + height * 0.5 > 2.80:
            raise RuntimeError(f"{prop_id}: gallery is outside a plausible eye-level hanging band")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    options = parser.parse_args()
    try:
        rows = ({row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
                if options.check else {})
        with tempfile.TemporaryDirectory(prefix="house01065-hall-gallery-", dir="/tmp") as scratch:
            for name, (asset_id, target) in ASSETS.items():
                generated = generate(Path(scratch), name)
                if options.write:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(generated.read_bytes())
                    measured, triangles, _materials, _names = inspect(target)
                    print(f"hall_gallery_prepare: {name} {triangles} triangles {measured} m "
                          f"{sha256(target)}")
                    continue
                if not target.is_file():
                    raise RuntimeError(f"committed gallery model is missing: {target}")
                actual = sha256(generated)
                committed = sha256(target)
                row = rows[asset_id]
                if actual != committed or actual != row["sourceSha256"]:
                    raise RuntimeError(f"{name}: regenerated={actual}, committed={committed}, "
                                       f"manifest={row['sourceSha256']}")
                measured, triangles = validate_asset(target, name, row)
                print(f"hall_gallery_prepare: {name} {triangles} triangles {measured} m "
                      f"{actual[:16]} deterministic")
            if options.check:
                validate_world(rows)
    except (KeyError, OSError, RuntimeError, TypeError, ValueError) as error:
        print(f"hall_gallery_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
