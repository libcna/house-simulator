#!/usr/bin/env python3
"""Regenerate and validate the shared physical family/porch ceiling fixture."""

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
import scale_check  # noqa: E402
sys.path.insert(0, str(REPO / "tools" / "world"))
import layout_io  # noqa: E402

SCRIPT = REPO / "tools" / "blender" / "family_ceiling_light.py"
TARGET = REPO / "assets-src" / "Models" / "Fixtures" / "family_ceiling_light.glb"
SOURCE_LAMP = REPO / "assets-src" / "Models" / "Furniture" / "LivingRoom" / "floor_lamp.glb"
TARGET_LAMP = REPO / "assets-src" / "Models" / "Furniture" / "Family" / "floor_lamp_lit.glb"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSET_ID = "MODEL_FIXTURE_FAMILY_CEILING"
LAMP_ASSET_ID = "MODEL_FAMILY_FLOOR_LAMP_LIT"
EXPECTED_MATERIALS = {
    "FAMILY_CEILING_METAL",
    "FamilyCeilingGlass",
    "FamilyCeilingDiffuser",
}
MAIN_POSITIONS = (
    [4.37, 3.12, -25.40],
    [6.53, 3.12, -25.40],
    [4.37, 3.12, -23.70],
    [6.53, 3.12, -23.70],
)
PORCH_POSITIONS = (
    [-2.45, 3.17, -12.95],
    [2.45, 3.17, -12.95],
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(folder: Path) -> Path:
    result = folder / TARGET.name
    command = blender_env.build_command(SCRIPT, ["--out", str(result)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot regenerate the family ceiling light")
    completed = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                               capture_output=True, text=True, check=False)
    if completed.returncode != 0 or \
            "family_ceiling_light: EXIT 0" not in completed.stdout.splitlines():
        tail = "\n".join((completed.stdout + "\n" + completed.stderr).splitlines()[-30:])
        raise RuntimeError(f"Blender failed (exit {completed.returncode}):\n{tail}")
    return result


def inspect(path: Path) -> tuple[list[float], int, set[str], set[str]]:
    document, _ = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError("family ceiling light has no authoritative POSITION bounds")
    measured = [round(high - low, 6) for low, high in zip(*bounds)]
    nodes = [node for node in document["nodes"] if "mesh" in node]
    names = {node.get("name", "") for node in nodes}
    primitives = [primitive for node in nodes
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives if "material" in primitive}
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise RuntimeError("a close-range fixture component lacks metre-scaled UV0")
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
    for component in (
            "opal_diffuser", "opal_emitter", "bronze_trim", "ceiling_canopy", "lower_finial"):
        if not any(component in name for name in names):
            raise RuntimeError(f"missing authored component {component}")
    problems = scale_check.check(path, "fixture", row["geometry"])
    if problems:
        raise RuntimeError("; ".join(problems))
    return measured, triangles


def validate_lamp(row: dict) -> None:
    if sha256(SOURCE_LAMP) != sha256(TARGET_LAMP) or sha256(TARGET_LAMP) != row["sourceSha256"]:
        raise RuntimeError("family floor lamp is no longer an exact deterministic source derivation")
    document, _ = gltf_io.read_model(TARGET_LAMP)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError("family floor lamp has no authoritative POSITION bounds")
    measured = [round(high - low, 6) for low, high in zip(*bounds)]
    if any(abs(actual - declared) > 0.005 for actual, declared in
           zip(measured, row["geometry"]["boundsMetres"])):
        raise RuntimeError(f"family floor-lamp manifest bounds differ from {measured}")
    materials = {material.get("name", "") for material in document.get("materials", [])}
    if materials != {"PaletteMaterial001", "LampshaderOuterBSDF"}:
        raise RuntimeError(f"family floor lamp source slots changed: {sorted(materials)}")


def validate_world() -> None:
    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props", "lights"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    lights = layout_io.by_id(layout_io.rows(layout, "lights"), "light")
    for index, position in enumerate(MAIN_POSITIONS, 1):
        prop_id = f"PROP_L0_FAMILY_CEILING_{index}"
        light_id = f"LIGHT_L0_FAMILY_MAIN_{index}"
        prop = props[prop_id]
        light = lights[light_id]
        if prop["asset"] != ASSET_ID or prop["cell"] != "L0_FAMILY" or \
                prop["position"] != position or prop["yawDeg"] != 0 or \
                prop["scale"] != 1 or not prop["static"] or prop["collision"] != "none":
            raise RuntimeError(f"canonical family ceiling placement changed: {prop_id}")
        if light["fixtureProp"] != prop_id or light["position"] != position or \
                light["type"] != "spot" or light["direction"] != [0.0, -1.0, 0.0] or \
                light["coneInnerDeg"] != 72.0 or light["coneOuterDeg"] != 140.0 or \
                light["colorK"] != 3000 or light["bulbClass"] != "led" or \
                light["intensityLm"] != 1200.0 or light["range"] != 6.62 or \
                light["emissiveMaterialSlot"] != "FamilyCeilingDiffuser" or \
                not light["bakedIntoLightmap"] or not light["defaultOn"]:
            raise RuntimeError(f"family ceiling optical linkage changed: {light_id}")

    for index, position in enumerate(PORCH_POSITIONS, 1):
        prop_id = f"PROP_L0_PORCH_CEILING_{index}"
        light_id = f"LIGHT_L0_PORCH_CEILING_{index}"
        prop = props[prop_id]
        light = lights[light_id]
        expected_receiver = "L0_LIVING" if index == 1 else "L0_STAIR_MAIN"
        if prop["asset"] != ASSET_ID or prop["cell"] != "L0_PORCH" or \
                prop["position"] != position or prop["yawDeg"] != 0 or \
                prop["scale"] != 1 or not prop["static"] or prop["collision"] != "none":
            raise RuntimeError(f"canonical porch ceiling placement changed: {prop_id}")
        if light["fixtureProp"] != prop_id or light["position"] != position or \
                light["group"] != "LG_L0_PORCH_LANTERN" or light["type"] != "point" or \
                light["direction"] != [0.0, -1.0, 0.0] or \
                light["colorK"] != 2700 or "bulbClass" in light or \
                light["intensityLm"] != 1000.0 or light["range"] != 7.28 or \
                light["bakeLumensPerRadiantWatt"] != 100.0 or \
                light["bakeCells"] != ["L0_FOYER", expected_receiver] or \
                light["spillCells"] != ["EXT_WALK"] or \
                light["emissiveMaterialSlot"] != "FamilyCeilingDiffuser" or \
                not light["bakedIntoLightmap"] or light["defaultOn"] or \
                not light["duskSensor"]:
            raise RuntimeError(f"porch ceiling optical linkage changed: {light_id}")

    lamp = props["PROP_FAMILY_LAMP"]
    reading = lights["LIGHT_L0_FAMILY_READING_1"]
    if lamp["asset"] != LAMP_ASSET_ID or lamp["position"] != [8.05, 0.6, -26.45] or \
            reading["position"] != [8.05, 1.997, -26.45] or \
            reading["fixtureProp"] != "PROP_FAMILY_LAMP" or \
            reading["emissiveMaterialSlot"] != "LampshaderOuterBSDF" or \
            reading["type"] != "point" or reading["intensityLm"] != 450.0 or \
            reading["defaultOn"]:
        raise RuntimeError("family reading light no longer matches its physical floor lamp")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    options = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix="house01285-family-light-", dir="/tmp") as scratch:
            generated = generate(Path(scratch))
            measured, triangles, _materials, _names = inspect(generated)
            if options.write:
                TARGET.parent.mkdir(parents=True, exist_ok=True)
                TARGET.write_bytes(generated.read_bytes())
                TARGET_LAMP.parent.mkdir(parents=True, exist_ok=True)
                TARGET_LAMP.write_bytes(SOURCE_LAMP.read_bytes())
                print(f"family_ceiling_light_prepare: bounds={measured} triangles={triangles} "
                      f"sha256={sha256(TARGET)} lamp={sha256(TARGET_LAMP)}")
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
            validate_lamp(rows[LAMP_ASSET_ID])
            validate_world()
            print(f"family_ceiling_light_prepare: {triangles} triangles {measured} m "
                  f"{committed[:16]} deterministic")
    except (KeyError, OSError, RuntimeError, TypeError, ValueError) as error:
        print(f"family_ceiling_light_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
