#!/usr/bin/env python3
"""Regenerate and validate HOUSE-00985's data-driven static prop kit as one group."""

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

SCRIPT = REPO / "tools" / "blender" / "prop_kit_gen.py"
DATA = SCRIPT.with_name("prop_kit_data.json")
MODELS = REPO / "assets-src" / "Models" / "Furniture" / "Generated"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
PROPS = REPO / "assets-src" / "world" / "layout.props.json"
MATERIAL_MAP = {
    # Furniture GLBs carry UV0 only, so their surface roles must use the stock Basic path. Shell
    # paint rows require UV1 and would make an otherwise valid reusable prop fail chunk building.
    "KIT_PAINT": "MAT_DOOR_PAINTED",
    "KIT_WOOD": "MAT_DOOR_HARDWOOD",
    "KIT_METAL": "MAT_KITCHEN_HARDWARE_STEEL",
    "KIT_GLASS": "MAT_GLASS_CABINET",
    "KIT_SCREEN": "MAT_KITCHEN_OVEN_GLASS",
    "KIT_CARDBOARD": "MAT_LIVING_ART_PAPER",
    "KIT_WHITE": "MAT_WINDOW_FRAME_WHITE",
}
PLACED_FAMILIES = {
    "carcass": "PROP_KIT_WARDROBE_EXAMPLE",
    "shelving": "PROP_KIT_SHELF_EXAMPLE",
    "worktop": "PROP_KIT_WORKBENCH_EXAMPLE",
    "box": "PROP_KIT_BOX_EXAMPLE",
    "fixture": "PROP_KIT_MIRROR_EXAMPLE",
    "service-run": "PROP_KIT_DUCT_EXAMPLE",
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(name: str, target: Path) -> None:
    command = blender_env.build_command(SCRIPT, ["--name", name, "--out", str(target)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot check generated prop kit")
    result = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                            capture_output=True, text=True, check=False)
    sentinel = f"prop_kit_gen: {name} EXIT 0"
    if result.returncode != 0 or sentinel not in result.stdout.splitlines():
        tail = "\n".join((result.stdout + "\n" + result.stderr).splitlines()[-24:])
        raise RuntimeError(f"{name}: generator failed ({result.returncode}):\n{tail}")


def measure(path: Path) -> tuple[list[float], int, int, set[str]]:
    document, _blob = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError(f"{path.name}: no POSITION bounds")
    low, high = bounds
    size = [high[index] - low[index] for index in range(3)]
    visible_triangles = 0
    collision_triangles = 0
    materials = set()
    collision_nodes = 0
    for node in document.get("nodes", []):
        if "mesh" not in node:
            continue
        primitives = document["meshes"][node["mesh"]]["primitives"]
        triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                        for primitive in primitives)
        if node.get("name", "").endswith("_COL"):
            collision_nodes += 1
            collision_triangles += triangles
        else:
            visible_triangles += triangles
            for primitive in primitives:
                if "TEXCOORD_0" not in primitive["attributes"]:
                    raise RuntimeError(f"{path.name}: visible primitive lacks UV0")
                materials.add(document["materials"][primitive["material"]]["name"])
    if collision_nodes != 1 or collision_triangles != 12:
        raise RuntimeError(f"{path.name}: expected one 12-triangle _COL box, got "
                           f"{collision_nodes} nodes/{collision_triangles} triangles")
    return size, visible_triangles, collision_triangles, materials


def check_piece(name: str, spec: dict, row: dict, folder: Path) -> None:
    first = folder / f"{name}.glb"
    repeated = folder / f"repeat_{name}.glb"
    generate(name, first)
    generate(name, repeated)
    committed = MODELS / f"{name}.glb"
    if digest(first) != digest(repeated):
        raise RuntimeError(f"{name}: generator is not byte-reproducible")
    if digest(first) != digest(committed) or digest(first) != row["sourceSha256"]:
        raise RuntimeError(f"{name}: regenerated, committed and manifest hashes disagree")
    size, triangles, _collision_triangles, materials = measure(first)
    expected = row["geometry"]["boundsMetres"]
    if any(abs(actual - declared) > 0.002 for actual, declared in zip(size, expected)):
        raise RuntimeError(f"{name}: measured bounds {size} differ from manifest {expected}")
    if triangles != row["geometry"]["triangles"]["LOD0"] or triangles > 1800:
        raise RuntimeError(f"{name}: unexpected visible triangle count {triangles}")
    if not materials or not materials <= set(MATERIAL_MAP) or row["materialMap"] != MATERIAL_MAP:
        raise RuntimeError(f"{name}: source/canonical material bridge changed: {materials}")
    mount = spec.get("mount", "floor")
    document, _blob = gltf_io.read_model(first)
    low, high = scale_check.accessor_bounds(document)
    if mount == "wall" and abs(high[2]) > 0.002:
        raise RuntimeError(f"{name}: wall plane is z={high[2]:.4f}, expected zero")
    if mount == "floor" and abs(low[1]) > 0.002:
        raise RuntimeError(f"{name}: support plane is y={low[1]:.4f}, expected zero")
    print(f"prop_kit_prepare: {name} {triangles} triangles, "
          f"{[round(value, 3) for value in size]} m, {digest(first)[:16]} deterministic")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", required=True)
    parser.parse_args()
    try:
        specs = json.loads(DATA.read_text())
        rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
        props = {row["id"]: row for row in json.loads(PROPS.read_text())["props"]}
        families = {spec["family"] for spec in specs.values()}
        if families != set(PLACED_FAMILIES):
            raise RuntimeError(f"data families {sorted(families)} differ from placement contract")
        for family, prop_id in PLACED_FAMILIES.items():
            row = props.get(prop_id)
            if row is None or not row.get("static") or row.get("collision") != "proxy":
                raise RuntimeError(f"{family}: canonical example {prop_id} is not a static proxy prop")
        # Acceptance explicitly excludes invented radiators: none exists in the authored house.
        if any(spec.get("variant") == "radiator" for spec in specs.values()):
            raise RuntimeError("radiator generated although the house data does not contain one")
        with tempfile.TemporaryDirectory(prefix="house00985-props-", dir="/tmp") as scratch:
            for name, spec in specs.items():
                asset_id = f"MODEL_PROP_KIT_{name.upper()}"
                row = rows[asset_id]
                expected_source = f"assets-src/Models/Furniture/Generated/{name}.glb"
                if row["sourceFile"] != expected_source or row["category"] != spec["category"]:
                    raise RuntimeError(f"{name}: manifest identity/category changed")
                check_piece(name, spec, row, Path(scratch))
        print(f"prop_kit_prepare: {len(specs)} pieces across {len(families)} families pass")
    except (FileNotFoundError, KeyError, RuntimeError, ValueError) as error:
        print(f"prop_kit_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
