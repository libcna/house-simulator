#!/usr/bin/env python3
"""Regenerate and validate the two authored L0 butler-pantry storage shelves."""

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

SCRIPT = REPO / "tools" / "blender" / "butlers_pantry_shelf.py"
COLLISION = REPO / "tools" / "blender" / "collision_proxy.py"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
MODELS = REPO / "assets-src" / "Models" / "Furniture" / "Kitchen"
VARIANTS = {
    "dry": ("MODEL_BUTLERS_DRY_SHELF", "PROP_L0_BUTLERS_DRY_SHELF",
            [-11.55, 0.60, -22.78], 0),
    "crockery": ("MODEL_BUTLERS_CROCKERY_SHELF", "PROP_L0_BUTLERS_CROCKERY_SHELF",
                  [-11.55, 0.60, -24.82], 180),
}
MATERIALS = {
    "CAB_OAK": "MAT_DOOR_HARDWOOD",
    "CAB_PAINT": "MAT_DOOR_PAINTED",
    "CAB_STEEL": "MAT_KITCHEN_HARDWARE_STEEL",
    "PANTRY_CERAMIC": "MAT_FOYER_CERAMIC",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def checked(command: list[str], *, environment: dict | None = None,
            sentinel: str | None = None) -> None:
    result = subprocess.run(command, cwd=REPO, env=environment,
                            capture_output=True, text=True, check=False)
    if result.returncode != 0 or (sentinel and sentinel not in result.stdout.splitlines()):
        tail = "\n".join((result.stdout + "\n" + result.stderr).splitlines()[-22:])
        raise RuntimeError(f"{command[0]} failed (exit {result.returncode}):\n{tail}")


def check_variant(variant: str, manifest: dict, props: dict,
                  folder: Path) -> None:
    asset_id, prop_id, position, yaw = VARIANTS[variant]
    filename = f"butlers_{variant}_shelf.glb"
    row = manifest[asset_id]
    model = MODELS / filename
    raw = folder / f"butlers_{variant}_shelf_raw.glb"
    generated = folder / filename
    command = blender_env.build_command(SCRIPT, ["--variant", variant,
                                                  "--out", str(raw)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot check authored pantry shelving")
    checked(command, environment=blender_env.environment(),
            sentinel="butlers_pantry_shelf: EXIT 0")
    checked([sys.executable, str(COLLISION), str(raw), str(generated),
             "--mode", "box"])
    digest = sha256(generated)
    if digest != sha256(model) or digest != row["sourceSha256"]:
        raise RuntimeError(f"{variant}: regenerated, committed and manifest hashes disagree")

    document, _blob = gltf_io.read_model(model)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError(f"{variant}: no authoritative POSITION bounds")
    measured = [high - low for low, high in zip(*bounds)]
    if any(abs(actual - declared) > 0.005 for actual, declared in
           zip(measured, row["geometry"]["boundsMetres"])):
        raise RuntimeError(f"{variant}: manifest differs from measured {measured}")
    if scale_check.check(model, "pantry-shelf", row["geometry"]):
        raise RuntimeError(f"{variant}: invalid scale")
    if origin_check.check(model, "pantry-shelf"):
        raise RuntimeError(f"{variant}: support origin is not centred and grounded")
    visible = [node for node in document["nodes"] if "mesh" in node and
               not node.get("name", "").endswith("_COL")]
    primitives = [primitive for node in visible
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    if triangles != row["geometry"]["triangles"]["LOD0"] or triangles > 3500:
        raise RuntimeError(f"{variant}: unexpected {triangles} visible triangles")
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise RuntimeError(f"{variant}: close-range component lacks UV0")
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives}
    expected = dict(MATERIALS)
    if variant == "dry":
        expected.update(PANTRY_LINEN="MAT_FURNITURE_DINING_LINEN",
                        PANTRY_PAPER="MAT_LIVING_ART_PAPER")
    if materials != set(expected) or row["materialMap"] != expected:
        raise RuntimeError(f"{variant}: source/canonical material bridge changed")
    names = {node.get("name", "") for node in visible}
    for fragment in (("flour_tin", "spice_jar", "paper_packet") if variant == "dry"
                     else ("plate_", "mug_", "preserve_jar")):
        if not any(fragment in name for name in names):
            raise RuntimeError(f"{variant}: missing visible {fragment}")
    proxy = next((node for node in document["nodes"]
                  if node.get("name") == row["geometry"]["collision"]), None)
    if proxy is None or sum(document["accessors"][primitive["indices"]]["count"] // 3
                            for primitive in document["meshes"][proxy["mesh"]]["primitives"]) != 12:
        raise RuntimeError(f"{variant}: the enclosing box proxy is missing")

    prop = props[prop_id]
    if (prop["asset"] != asset_id or prop["cell"] != "L0_BUTLERS" or
            prop["position"] != position or prop["yawDeg"] != yaw or
            prop["collision"] != "proxy" or not prop["static"]):
        raise RuntimeError(f"{variant}: canonical placement or collision changed")
    west, east = position[0] - measured[0] / 2, position[0] + measured[0] / 2
    if not (-12.70 < west and east < -11.00):
        raise RuntimeError(f"{variant}: shelf enters the west wall or side-door casing")
    inward = position[2] - measured[2] / 2 if variant == "dry" else position[2] + measured[2] / 2
    outer = position[2] + measured[2] / 2 if variant == "dry" else position[2] - measured[2] / 2
    if not ((outer < -22.60 and inward > -23.00) if variant == "dry"
            else (outer > -25.00 and inward < -24.60)):
        raise RuntimeError(f"{variant}: shelf clips a wall, service run or aisle")
    print(f"butlers_pantry_shelf_prepare: {variant} {triangles} triangles, "
          f"{[round(value, 3) for value in measured]} m, sha256 {digest[:16]}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", required=True)
    parser.parse_args()
    try:
        manifest = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
        layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props", "portals"])
        props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
        with tempfile.TemporaryDirectory(prefix="house01075-butlers-", dir="/tmp") as scratch:
            for variant in VARIANTS:
                check_variant(variant, manifest, props, Path(scratch))
        if (-22.78 - 0.322 / 2) - (-24.82 + 0.322 / 2) < 1.60:
            raise RuntimeError("opposing cabinets leave less than 1.60 m aisle")
    except (RuntimeError, KeyError, ValueError, StopIteration) as error:
        print(f"butlers_pantry_shelf_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
