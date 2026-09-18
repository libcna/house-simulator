#!/usr/bin/env python3
"""Regenerate and validate HOUSE-00771's project-authored garden furniture suite."""

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

SCRIPT = REPO / "tools" / "blender" / "garden_furniture_suite.py"
COLLISION = REPO / "tools" / "blender" / "collision_proxy.py"
TARGET = REPO / "assets-src" / "Models" / "Furniture" / "Garden"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSETS = {
    "dining": ("MODEL_GARDEN_DINING_SET", "dining_set.glb", "garden-dining-set"),
    "lounger": ("MODEL_GARDEN_LOUNGER", "lounger.glb", "garden-lounger"),
    "swing": ("MODEL_GARDEN_SWING_BENCH", "swing_bench.glb", "garden-swing"),
    "firepit": ("MODEL_GARDEN_FIRE_PIT", "fire_pit.glb", "garden-firepit"),
    "birdbath": ("MODEL_GARDEN_BIRDBATH", "birdbath.glb", "birdbath"),
    "planter": ("MODEL_GARDEN_PLANTER", "planter.glb", "planter"),
}
EXPECTED_MATERIALS = {
    "dining": {"GARDEN_WOOD", "GARDEN_METAL", "GARDEN_TEXTILE"},
    "lounger": {"GARDEN_WOOD", "GARDEN_METAL", "GARDEN_TEXTILE"},
    "swing": {"GARDEN_WOOD", "GARDEN_METAL", "GARDEN_TEXTILE"},
    "firepit": {"GARDEN_WOOD", "GARDEN_METAL", "GARDEN_STONE", "GARDEN_SOIL"},
    "birdbath": {"GARDEN_STONE"},
    "planter": {"GARDEN_STONE", "GARDEN_SOIL", "GARDEN_FOLIAGE"},
}
REQUIRED_COMPONENTS = {
    "dining": ("table_top", "table_pedestal", "chair_1_seat", "chair_4_back_slat"),
    "lounger": ("side_rail", "seat_pad", "back_frame", "back_stay"),
    "swing": ("top_beam", "chain_", "bench_seat_pad", "canopy"),
    "firepit": ("stone_ring", "steel_bowl", "cold_ash", "split_log"),
    "birdbath": ("pedestal", "basin_rim", "basin_floor"),
    "planter": ("tapered_pot", "pot_rim", "soil", "foliage_6"),
}
MATERIAL_MAP = {
    "GARDEN_WOOD": "MAT_OUTDOOR_GARDEN_WOOD",
    "GARDEN_METAL": "MAT_METAL_BALCONY",
    "GARDEN_TEXTILE": "MAT_FAMILY_CURTAIN_WOOL",
    "GARDEN_STONE": "MAT_OUTDOOR_BLUESTONE",
    "GARDEN_SOIL": "MAT_OUTDOOR_SOIL",
    "GARDEN_FOLIAGE": "MAT_FURNITURE_PLANT_LEAF",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def checked_run(command: list[str], *, sentinel: str | None = None) -> str:
    result = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                            capture_output=True, text=True, check=False)
    if result.returncode != 0 or (sentinel and sentinel not in result.stdout.splitlines()):
        tail = "\n".join((result.stdout + "\n" + result.stderr).splitlines()[-30:])
        raise RuntimeError(f"{command[0]} failed (exit {result.returncode}):\n{tail}")
    return result.stdout


def generate(folder: Path, name: str) -> Path:
    raw = folder / f"{name}_raw.glb"
    final = folder / ASSETS[name][1]
    command = blender_env.build_command(SCRIPT, ["--asset", name, "--out", str(raw)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot regenerate the authored garden suite")
    checked_run(command, sentinel="garden_furniture_suite: EXIT 0")
    checked_run([sys.executable, str(COLLISION), str(raw), str(final), "--mode", "box"])
    if not final.is_file():
        raise RuntimeError(f"generation produced no {final.name}")
    return final


def visible(document: dict) -> tuple[list[dict], set[str]]:
    nodes = [node for node in document["nodes"] if "mesh" in node and
             not node.get("name", "").endswith("_COL")]
    primitives = [primitive for node in nodes
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    return primitives, {node.get("name", "") for node in nodes}


def inspect(path: Path) -> tuple[list[float], int, str, set[str], set[str]]:
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
    collision = next((node.get("name") for node in document["nodes"]
                      if node.get("name", "").endswith("_COL")), None)
    if collision is None:
        raise RuntimeError(f"{path.name}: collision proxy is missing")
    return measured, triangles, collision, materials, names


def validate(path: Path, name: str, row: dict) -> tuple[list[float], int, str]:
    measured, triangles, collision, materials, names = inspect(path)
    if any(abs(actual - stored) > 0.005
           for actual, stored in zip(measured, row["geometry"]["boundsMetres"])):
        raise RuntimeError(f"{name}: manifest bounds differ from measured {measured}")
    if triangles != row["geometry"]["triangles"]["LOD0"]:
        raise RuntimeError(f"{name}: manifest triangle count differs from {triangles}")
    if collision != row["geometry"].get("collision"):
        raise RuntimeError(f"{name}: manifest collision proxy differs from {collision}")
    if materials != EXPECTED_MATERIALS[name]:
        raise RuntimeError(f"{name}: unexpected finishes {sorted(materials)}")
    expected_map = {slot: MATERIAL_MAP[slot] for slot in EXPECTED_MATERIALS[name]}
    if row["materialMap"] != expected_map:
        raise RuntimeError(f"{name}: canonical material map changed")
    for fragment in REQUIRED_COMPONENTS[name]:
        if not any(fragment in node for node in names):
            raise RuntimeError(f"{name}: missing authored component {fragment}")
    document, _ = gltf_io.read_model(path)
    primitives, _names = visible(document)
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise RuntimeError(f"{name}: a close-range component lacks metre-scaled UV0")
    problems = scale_check.check(path, ASSETS[name][2], row["geometry"])
    if problems:
        raise RuntimeError(f"{name}: {'; '.join(problems)}")
    problems = origin_check.check(path, ASSETS[name][2])
    if problems:
        raise RuntimeError(f"{name}: {'; '.join(problems)}")
    return measured, triangles, collision


def validate_world() -> None:
    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    expected = {
        "PROP_GARDEN_DINING_SET": ("MODEL_GARDEN_DINING_SET", "EXT_TERRACE",
                                    [3.80, 0.45, -34.10], 15),
        "PROP_GARDEN_LOUNGER_1": ("MODEL_GARDEN_LOUNGER", "EXT_TERRACE",
                                  [-3.90, 0.45, -33.50], 350),
        "PROP_GARDEN_LOUNGER_2": ("MODEL_GARDEN_LOUNGER", "EXT_TERRACE",
                                  [-2.65, 0.45, -33.75], 8),
        "PROP_GARDEN_SWING_BENCH": ("MODEL_GARDEN_SWING_BENCH", "EXT_BACKYARD",
                                    [5.20, -0.266651, -40.30], 180),
        "PROP_GARDEN_FIRE_PIT": ("MODEL_GARDEN_FIRE_PIT", "EXT_BACKYARD",
                                 [0.40, -0.266651, -40.60], 0),
        "PROP_GARDEN_BIRDBATH": ("MODEL_GARDEN_BIRDBATH", "EXT_BACKYARD",
                                 [9.10, -0.277089, -41.10], 0),
        "PROP_GARDEN_PLANTER_1": ("MODEL_GARDEN_PLANTER", "EXT_TERRACE",
                                  [-5.95, 0.45, -32.85], 12),
        "PROP_GARDEN_PLANTER_2": ("MODEL_GARDEN_PLANTER", "EXT_TERRACE",
                                  [5.95, 0.45, -32.85], 348),
        "PROP_GARDEN_PLANTER_3": ("MODEL_GARDEN_PLANTER", "EXT_TERRACE",
                                  [-5.95, 0.45, -35.55], 168),
        "PROP_GARDEN_PLANTER_4": ("MODEL_GARDEN_PLANTER", "EXT_TERRACE",
                                  [5.95, 0.45, -35.55], 192),
    }
    for identifier, (asset, cell, position, yaw) in expected.items():
        prop = props[identifier]
        if prop["asset"] != asset or prop["cell"] != cell or \
                prop["position"] != position or prop["yawDeg"] != yaw or \
                prop["scale"] != 1 or not prop["static"] or \
                prop["collision"] != "proxy" or prop["material"] is not None or \
                prop["interactable"] is not None:
            raise RuntimeError(f"{identifier}: canonical garden placement changed")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    options = parser.parse_args()
    try:
        rows = ({row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
                if options.check else {})
        with tempfile.TemporaryDirectory(prefix="house00771-garden-furniture-", dir="/tmp") as work:
            for name, (asset_id, filename, _category) in ASSETS.items():
                generated = generate(Path(work), name)
                if options.write:
                    target = TARGET / filename
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(generated.read_bytes())
                    measured, triangles, collision, _materials, _names = inspect(target)
                    print(f"garden_furniture_prepare: {name} bounds={measured} "
                          f"triangles={triangles} collision={collision} sha256={sha256(target)}")
                    continue
                target = TARGET / filename
                if not target.is_file():
                    raise RuntimeError(f"committed garden model is missing: {target}")
                actual = sha256(generated)
                committed = sha256(target)
                row = rows[asset_id]
                if actual != committed or actual != row["sourceSha256"]:
                    raise RuntimeError(
                        f"{name}: regenerated={actual}, committed={committed}, "
                        f"manifest={row['sourceSha256']}")
                measured, triangles, _collision = validate(target, name, row)
                print(f"garden_furniture_prepare: {name} {triangles} triangles {measured} m "
                      f"{actual[:16]} deterministic")
            if options.check:
                validate_world()
    except (KeyError, OSError, RuntimeError, StopIteration, TypeError, ValueError) as error:
        print(f"garden_furniture_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
