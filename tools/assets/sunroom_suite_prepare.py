#!/usr/bin/env python3
"""Regenerate and validate the project-authored sunroom suite."""

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

SCRIPT = REPO / "tools" / "blender" / "sunroom_suite.py"
COLLISION = REPO / "tools" / "blender" / "collision_proxy.py"
TARGET = REPO / "assets-src" / "Models" / "Furniture" / "Sunroom"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSETS = {
    "breakfast": ("MODEL_SUNROOM_BREAKFAST_GROUP", "breakfast_group.glb",
                  "breakfast-dining-group"),
    "lounge": ("MODEL_SUNROOM_LOUNGE_GROUP", "lounge_group.glb",
               "sunroom-lounge-group"),
    "bar": ("MODEL_SUNROOM_WET_BAR", "wet_bar.glb", "wet-bar"),
}
EXPECTED_MATERIALS = {
    "breakfast": {"SUNROOM_OAK", "SUNROOM_RATTAN", "SUNROOM_CUSHION",
                  "SUNROOM_CERAMIC", "SUNROOM_FRUIT"},
    "lounge": {"SUNROOM_OAK", "SUNROOM_RATTAN", "SUNROOM_CUSHION",
               "SUNROOM_CERAMIC"},
    "bar": {"SUNROOM_CABINET", "SUNROOM_OAK", "SUNROOM_STONE",
            "SUNROOM_METAL", "SUNROOM_CERAMIC"},
}
MATERIAL_MAP = {
    "SUNROOM_OAK": "MAT_FURNITURE_PIANO_WOOD",
    "SUNROOM_RATTAN": "MAT_SUNROOM_RATTAN",
    "SUNROOM_CUSHION": "MAT_SUNROOM_CUSHION",
    "SUNROOM_CERAMIC": "MAT_FOYER_CERAMIC",
    "SUNROOM_FRUIT": "MAT_KITCHEN_PRODUCE_LEMON",
    "SUNROOM_CABINET": "MAT_DOOR_PAINTED",
    "SUNROOM_STONE": "MAT_KITCHEN_COUNTER_STONE",
    "SUNROOM_METAL": "MAT_KITCHEN_HARDWARE_STEEL",
}
REQUIRED_COMPONENTS = {
    "breakfast": ("table_top", "table_pedestal", "chair_1_cushion", "chair_4_crest",
                  "chair_2_cane_v_", "fruit_bowl", "mug_handle", "plate_"),
    "lounge": ("lounge_1_seat_cushion", "lounge_2_crest", "lounge_1_cane_v_",
               "lounge_2_lumbar", "tea_table_top", "tea_table_leg_", "tea_cup_handle"),
    "bar": ("base_carcass", "left_door_panel", "stone_counter", "sink_basin",
            "faucet_spout", "open_shelf_2", "tumbler_", "bottle_", "bowl_"),
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
        raise RuntimeError("Blender unavailable; cannot regenerate the authored sunroom suite")
    checked_run(command, sentinel="sunroom_suite: EXIT 0")
    collision_mode = "boxes" if name == "lounge" else "box"
    checked_run([sys.executable, str(COLLISION), str(raw), str(final),
                 "--mode", collision_mode])
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
    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props", "nav"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    expected = {
        "PROP_L0_SUNROOM_BREAKFAST":
            ("MODEL_SUNROOM_BREAKFAST_GROUP", [-5.00, 0.60, -29.10], 0, "proxy"),
        "PROP_L0_SUNROOM_WET_BAR":
            ("MODEL_SUNROOM_WET_BAR", [1.15, 0.60, -27.44], 0, "proxy"),
        "PROP_L0_SUNROOM_LOUNGE":
            ("MODEL_SUNROOM_LOUNGE_GROUP", [1.40, 0.60, -29.80], 0, "proxy"),
    }
    for identifier, (asset, position, yaw, collision) in expected.items():
        prop = props[identifier]
        if prop["asset"] != asset or prop["cell"] != "L0_SUNROOM" or \
                prop["position"] != position or prop["yawDeg"] != yaw or \
                prop["scale"] != 1 or not prop["static"] or \
                prop["collision"] != collision or prop["material"] is not None or \
                prop["interactable"] is not None:
            raise RuntimeError(f"{identifier}: canonical sunroom placement changed")
    rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
    lounge = props["PROP_L0_SUNROOM_LOUNGE"]
    lounge_width = rows[lounge["asset"]]["geometry"]["boundsMetres"][0]
    lounge_left = lounge["position"][0] - lounge_width * 0.5
    if lounge_left - (-0.80) < 1.0:
        raise RuntimeError("sunroom lounge intrudes into the terrace slider's east-side lane")
    nav_nodes = layout_io.by_id(layout["nav"].get("nodes", []), "node")
    for identifier, z in (("NAV_L0_SUNROOM_04", -30.43),
                          ("NAV_L0_SUNROOM_08", -28.77)):
        position = nav_nodes[identifier]["position"]
        if position != [0.0, 0.60, z] or lounge_left - position[0] < 0.60:
            raise RuntimeError(f"{identifier}: east aisle node collides with reading group")
    perch = layout_io.by_id(layout["nav"].get("perches", []), "perch")[
        "PERCH_L0_SUNROOM_WICKER"]
    if perch["position"] != [-3.88, 1.05, -29.10]:
        raise RuntimeError("sunroom wicker perch no longer meets the east chair's cushion")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    options = parser.parse_args()
    try:
        rows = ({row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
                if options.check else {})
        with tempfile.TemporaryDirectory(prefix="house01068-sunroom-", dir="/tmp") as work:
            for name, (asset_id, filename, _category) in ASSETS.items():
                generated = generate(Path(work), name)
                if options.write:
                    target = TARGET / filename
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(generated.read_bytes())
                    measured, triangles, collision, _materials, _names = inspect(target)
                    print(f"sunroom_suite_prepare: {name} bounds={measured} "
                          f"triangles={triangles} collision={collision} sha256={sha256(target)}")
                    continue
                target = TARGET / filename
                if not target.is_file():
                    raise RuntimeError(f"committed sunroom model is missing: {target}")
                actual = sha256(generated)
                committed = sha256(target)
                row = rows[asset_id]
                if actual != committed or actual != row["sourceSha256"]:
                    raise RuntimeError(
                        f"{name}: regenerated={actual}, committed={committed}, "
                        f"manifest={row['sourceSha256']}")
                measured, triangles, _collision = validate(target, name, row)
                print(f"sunroom_suite_prepare: {name} {triangles} triangles {measured} m "
                      f"{actual[:16]} deterministic")
            if options.check:
                validate_world()
    except (KeyError, OSError, RuntimeError, StopIteration, TypeError, ValueError) as error:
        print(f"sunroom_suite_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
