#!/usr/bin/env python3
"""Regenerate and validate HOUSE-01048's project-authored dining suite."""

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
import scale_check  # noqa: E402
sys.path.insert(0, str(REPO / "tools" / "world"))
import layout_io  # noqa: E402

SCRIPT = REPO / "tools" / "blender" / "dining_suite.py"
COLLISION = REPO / "tools" / "blender" / "collision_proxy.py"
TARGET = REPO / "assets-src" / "Models" / "Furniture" / "Dining"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSETS = {
    "table": ("MODEL_FURNITURE_DINING_TABLE", "dining_table.glb", "table-dining", True),
    "chair": ("MODEL_FURNITURE_DINING_CHAIR", "dining_chair.glb", "chair", True),
    "chandelier": ("MODEL_FIXTURE_DINING_CHANDELIER", "dining_chandelier.glb", "fixture", False),
}
EXPECTED_MATERIALS = {
    "table": {"DINING_WOOD", "DINING_BRASS"},
    "chair": {"DINING_WOOD", "DINING_FABRIC"},
    "chandelier": {"CHANDELIER_METAL", "ChandelierShade"},
}
REQUIRED_COMPONENTS = {
    "table": ("top_panel", "breadboard_-1", "long_apron_1", "brass_foot"),
    "chair": ("seat_cushion", "front_leg", "back_pad", "back_crest"),
    "chandelier": ("ceiling_canopy", "drop_stem", "long_frame", "shade_"),
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


def generate(folder: Path, name: str, collidable: bool) -> Path:
    raw = folder / f"{name}_raw.glb"
    final = folder / ASSETS[name][1]
    command = blender_env.build_command(SCRIPT, ["--asset", name, "--out", str(raw)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot regenerate the authored dining suite")
    checked_run(command, sentinel="dining_suite: EXIT 0")
    if collidable:
        checked_run([sys.executable, str(COLLISION), str(raw), str(final), "--mode", "box"])
    else:
        final.write_bytes(raw.read_bytes())
    if not final.is_file():
        raise RuntimeError(f"generation produced no {final.name}")
    return final


def visible(document: dict) -> tuple[list[dict], set[str]]:
    nodes = [node for node in document["nodes"] if "mesh" in node and
             not node.get("name", "").endswith("_COL")]
    primitives = [primitive for node in nodes
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    return primitives, {node.get("name", "") for node in nodes}


def validate(path: Path, name: str, row: dict) -> tuple[list[float], int, str | None]:
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
    collision = row["geometry"].get("collision")
    all_names = {node.get("name", "") for node in document["nodes"]}
    if ASSETS[name][3] and collision not in all_names:
        raise RuntimeError(f"{name}: declared collision proxy is missing")
    if not ASSETS[name][3] and any(node.endswith("_COL") for node in all_names):
        raise RuntimeError(f"{name}: non-collidable chandelier contains a collision proxy")
    return measured, triangles, collision


def validate_world() -> None:
    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props", "lights"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    table = props["PROP_L0_DINING_TABLE"]
    chairs = [props[f"PROP_L0_DINING_CHAIR_{index}"] for index in range(1, 9)]
    chandelier = props["PROP_L0_DINING_CHANDELIER"]
    if table["cell"] != "L0_DINING" or table["position"] != [-5.2, 0.6, -21.6] or \
            table["collision"] != "proxy":
        raise RuntimeError("canonical dining table placement changed")
    if len(chairs) != 8 or any(chair["cell"] != "L0_DINING" or
                               chair["collision"] != "proxy" for chair in chairs):
        raise RuntimeError("the canonical table no longer has eight collidable dining chairs")
    if chandelier["position"] != [-5.2, 2.46, -21.6] or chandelier["collision"] != "none":
        raise RuntimeError("canonical chandelier no longer meets the ceiling/light point")
    lights = layout_io.by_id(layout_io.rows(layout, "lights"), "light")
    light = lights["LIGHT_L0_DINING_CHANDELIER_1"]
    if light["fixtureProp"] != chandelier["id"] or \
            light["emissiveMaterialSlot"] != "ChandelierShade" or \
            light["position"] != [-5.2, 2.55, -21.6]:
        raise RuntimeError("dining chandelier is not linked to its exact shade/optical point")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    options = parser.parse_args()
    try:
        rows = ({row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
                if options.check else {})
        with tempfile.TemporaryDirectory(prefix="house01048-dining-", dir="/tmp") as scratch:
            for name, (asset_id, filename, _category, collidable) in ASSETS.items():
                generated = generate(Path(scratch), name, collidable)
                if options.write:
                    target = TARGET / filename
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(generated.read_bytes())
                    document, _ = gltf_io.read_model(target)
                    bounds = scale_check.accessor_bounds(document)
                    primitives, _names = visible(document)
                    triangles = sum(document["accessors"][p["indices"]]["count"] // 3
                                    for p in primitives)
                    collision = next((node.get("name") for node in document["nodes"]
                                      if node.get("name", "").endswith("_COL")), None)
                    measured = [round(high - low, 6) for low, high in zip(*bounds)]
                    print(f"dining_suite_prepare: {name} bounds={measured} triangles={triangles} "
                          f"collision={collision} sha256={sha256(target)}")
                    continue
                target = TARGET / filename
                if not target.is_file():
                    raise RuntimeError(f"committed dining model is missing: {target}")
                actual = sha256(generated)
                committed = sha256(target)
                row = rows[asset_id]
                if actual != committed or actual != row["sourceSha256"]:
                    raise RuntimeError(
                        f"{name}: regenerated={actual}, committed={committed}, "
                        f"manifest={row['sourceSha256']}")
                measured, triangles, _collision = validate(target, name, row)
                print(f"dining_suite_prepare: {name} {triangles} triangles {measured} m "
                      f"{actual[:16]} deterministic")
            if options.check:
                validate_world()
    except (KeyError, RuntimeError, TypeError, ValueError) as error:
        print(f"dining_suite_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
