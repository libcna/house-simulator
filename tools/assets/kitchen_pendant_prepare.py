#!/usr/bin/env python3
"""Regenerate and validate HOUSE-01283's project-authored kitchen pendant."""

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

SCRIPT = REPO / "tools" / "blender" / "kitchen_pendant.py"
TARGET = REPO / "assets-src" / "Models" / "Fixtures" / "kitchen_pendant.glb"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSET_ID = "MODEL_FIXTURE_KITCHEN_PENDANT"
EXPECTED_MATERIALS = {"PENDANT_METAL", "PendantShade"}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(folder: Path) -> Path:
    result = folder / TARGET.name
    command = blender_env.build_command(SCRIPT, ["--out", str(result)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot regenerate the kitchen pendant")
    completed = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                               capture_output=True, text=True, check=False)
    if completed.returncode != 0 or "kitchen_pendant: EXIT 0" not in completed.stdout.splitlines():
        tail = "\n".join((completed.stdout + "\n" + completed.stderr).splitlines()[-30:])
        raise RuntimeError(f"Blender failed (exit {completed.returncode}):\n{tail}")
    return result


def inspect(path: Path) -> tuple[list[float], int, set[str], set[str]]:
    document, _ = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError("pendant has no authoritative POSITION bounds")
    measured = [round(high - low, 6) for low, high in zip(*bounds)]
    nodes = [node for node in document["nodes"] if "mesh" in node]
    names = {node.get("name", "") for node in nodes}
    primitives = [primitive for node in nodes
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives if "material" in primitive}
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise RuntimeError("a close-range pendant component lacks metre-scaled UV0")
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    return measured, triangles, materials, names


def validate_asset(path: Path, row: dict) -> tuple[list[float], int]:
    measured, triangles, materials, names = inspect(path)
    if any(abs(actual - declared) > 0.005 for actual, declared in
           zip(measured, row["geometry"]["boundsMetres"])):
        raise RuntimeError(f"manifest bounds differ from measured {measured}")
    if triangles != row["geometry"]["triangles"]["LOD0"]:
        raise RuntimeError(f"manifest triangle count differs from measured {triangles}")
    if materials != EXPECTED_MATERIALS:
        raise RuntimeError(f"unexpected source finishes {sorted(materials)}")
    for component in ("bell_shade", "frosted_diffuser", "suspension_cord", "ceiling_canopy"):
        if not any(component in name for name in names):
            raise RuntimeError(f"missing authored component {component}")
    problems = scale_check.check(path, "fixture", row["geometry"])
    if problems:
        raise RuntimeError("; ".join(problems))
    return measured, triangles


def validate_world() -> None:
    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props", "lights"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    lights = layout_io.by_id(layout_io.rows(layout, "lights"), "light")
    expected_x = (-4.05, -3.30, -2.55)
    for index, x in enumerate(expected_x, 1):
        prop_id = f"PROP_L0_KITCHEN_PENDANT_{index}"
        light_id = f"LIGHT_L0_KITCHEN_ISLAND_{index}"
        prop = props[prop_id]
        light = lights[light_id]
        expected_prop = [x, 2.43, -24.62]
        expected_light = [x, 2.45, -24.62]
        if prop["asset"] != ASSET_ID or prop["cell"] != "L0_KITCHEN" or \
                prop["position"] != expected_prop or prop["yawDeg"] != 0 or \
                prop["scale"] != 1 or not prop["static"] or prop["collision"] != "none":
            raise RuntimeError(f"canonical pendant placement changed: {prop_id}")
        if light["fixtureProp"] != prop_id or light["position"] != expected_light or \
                light["type"] != "spot" or light["direction"] != [0.0, -1.0, 0.0] or \
                light["coneInnerDeg"] != 48.0 or light["coneOuterDeg"] != 92.0 or \
                light["colorK"] != 2850 or light["bulbClass"] != "led" or \
                light["intensityLm"] != 700.0 or light["range"] != 5.20 or \
                light["emissiveMaterialSlot"] != "PendantShade" or \
                not light["bakedIntoLightmap"] or not light["defaultOn"]:
            raise RuntimeError(f"pendant optical linkage changed: {light_id}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    options = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix="house01283-pendant-", dir="/tmp") as scratch:
            generated = generate(Path(scratch))
            measured, triangles, _materials, _names = inspect(generated)
            if options.write:
                TARGET.parent.mkdir(parents=True, exist_ok=True)
                TARGET.write_bytes(generated.read_bytes())
                print(f"kitchen_pendant_prepare: bounds={measured} triangles={triangles} "
                      f"sha256={sha256(TARGET)}")
                return 0

            if not TARGET.is_file():
                raise RuntimeError(f"committed model is missing: {TARGET}")
            rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
            row = rows[ASSET_ID]
            regenerated = sha256(generated)
            committed = sha256(TARGET)
            if regenerated != committed or regenerated != row["sourceSha256"]:
                raise RuntimeError(
                    f"regenerated={regenerated}, committed={committed}, "
                    f"manifest={row['sourceSha256']}")
            measured, triangles = validate_asset(TARGET, row)
            validate_world()
            print(f"kitchen_pendant_prepare: {triangles} triangles {measured} m "
                  f"{committed[:16]} deterministic")
    except (KeyError, RuntimeError, TypeError, ValueError) as error:
        print(f"kitchen_pendant_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
