#!/usr/bin/env python3
"""Regenerate and validate HOUSE-01060's formal-living fireplace composition."""

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

SCRIPT = REPO / "tools" / "blender" / "living_fireplace.py"
TARGET = REPO / "assets-src" / "Models" / "Furniture" / "LivingRoom" / "living_fireplace.glb"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSET_ID = "MODEL_LIVING_FIREPLACE"
PROP_ID = "PROP_LIVING_FIREPLACE"
EXPECTED_MATERIALS = {
    "FIREPLACE_STONE",
    "FIREPLACE_IRON",
    "FIREPLACE_WOOD",
    "FIREPLACE_CANVAS",
    "FIREPLACE_BRASS",
}
REQUIRED_COMPONENTS = (
    "hearth_base",
    "stone_jamb_-1",
    "stone_lintel",
    "walnut_mantel",
    "firebox_back",
    "grate_front",
    "log_low",
    "overmantel_canvas",
    "art_sun",
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def checked_run(command: list[str], *, sentinel: str | None = None) -> str:
    result = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                            capture_output=True, text=True, check=False)
    if result.returncode != 0 or (sentinel and sentinel not in result.stdout.splitlines()):
        tail = "\n".join((result.stdout + "\n" + result.stderr).splitlines()[-40:])
        raise RuntimeError(f"{command[0]} failed (exit {result.returncode}):\n{tail}")
    return result.stdout


def generate(folder: Path) -> Path:
    final = folder / TARGET.name
    command = blender_env.build_command(SCRIPT, ["--out", str(final)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot regenerate the authored fireplace")
    checked_run(command, sentinel="living_fireplace: EXIT 0")
    if not final.is_file():
        raise RuntimeError("fireplace generation produced no GLB")
    return final


def visible(document: dict) -> tuple[list[dict], set[str]]:
    nodes = [node for node in document["nodes"] if "mesh" in node and
             not node.get("name", "").endswith("_COL")]
    primitives = [primitive for node in nodes
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    return primitives, {node.get("name", "") for node in nodes}


def inspect(path: Path) -> tuple[list[float], int, set[str], set[str], str, int]:
    document, _ = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError("fireplace has no authoritative POSITION bounds")
    measured = [round(high - low, 6) for low, high in zip(*bounds)]
    primitives, names = visible(document)
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives if "material" in primitive}
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise RuntimeError("a close-range fireplace component lacks metre-scaled UV0")
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    collision_nodes = [node for node in document["nodes"]
                       if node.get("name", "").endswith("_COL")]
    if len(collision_nodes) != 1:
        raise RuntimeError(f"expected one fireplace collision mesh, got {len(collision_nodes)}")
    collision = collision_nodes[0]
    collision_triangles = sum(
        document["accessors"][primitive["indices"]]["count"] // 3
        for primitive in document["meshes"][collision["mesh"]]["primitives"])
    return measured, triangles, materials, names, collision["name"], collision_triangles


def validate_asset(path: Path, row: dict) -> tuple[list[float], int, str, int]:
    measured, triangles, materials, names, collision, collision_triangles = inspect(path)
    if any(abs(actual - declared) > 0.005 for actual, declared in
           zip(measured, row["geometry"]["boundsMetres"])):
        raise RuntimeError(f"manifest bounds differ from measured {measured}")
    if triangles != row["geometry"]["triangles"]["LOD0"]:
        raise RuntimeError(f"manifest triangle count differs from measured {triangles}")
    if materials != EXPECTED_MATERIALS:
        raise RuntimeError(f"unexpected source finishes {sorted(materials)}")
    for fragment in REQUIRED_COMPONENTS:
        if not any(fragment in name for name in names):
            raise RuntimeError(f"missing authored component {fragment}")
    if row["geometry"].get("collision") != collision or collision_triangles != 12:
        raise RuntimeError(
            f"fireplace collision differs: {collision}, {collision_triangles} triangles")
    problems = scale_check.check(path, "fireplace-surround", row["geometry"])
    if problems:
        raise RuntimeError("; ".join(problems))
    problems = origin_check.check(path, "fireplace-surround")
    if problems:
        raise RuntimeError("; ".join(problems))
    return measured, triangles, collision, collision_triangles


def validate_world() -> None:
    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props", "interactables"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    prop = props[PROP_ID]
    if prop["asset"] != ASSET_ID or prop["cell"] != "L0_LIVING" or \
            prop["position"] != [-7.65, 0.60, -17.55] or prop["yawDeg"] != 270 or \
            prop["scale"] != 1 or not prop["static"] or prop["collision"] != "proxy":
        raise RuntimeError("canonical fireplace placement changed")
    interactables = layout_io.by_id(layout_io.rows(layout, "interactables"), "interactable")
    fireplace = interactables["APPL_L0_LIVING_FIREPLACE"]
    if fireplace["cell"] != "L0_LIVING" or \
            fireplace["focus"]["point"] != [-7.96, 1.15, -17.55]:
        raise RuntimeError("fireplace suite no longer aligns with the canonical appliance/chimney")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    options = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix="house01060-fireplace-", dir="/tmp") as scratch:
            generated = generate(Path(scratch))
            measured, triangles, _materials, _names, collision, collision_triangles = \
                inspect(generated)
            if options.write:
                TARGET.parent.mkdir(parents=True, exist_ok=True)
                TARGET.write_bytes(generated.read_bytes())
                print(f"living_fireplace_prepare: bounds={measured} triangles={triangles} "
                      f"collision={collision}/{collision_triangles} "
                      f"sha256={sha256(TARGET)}")
                return 0

            if not TARGET.is_file():
                raise RuntimeError(f"committed fireplace model is missing: {TARGET}")
            row = next(row for row in json.loads(MANIFEST.read_text())["assets"]
                       if row["id"] == ASSET_ID)
            regenerated = sha256(generated)
            committed = sha256(TARGET)
            if regenerated != committed or regenerated != row["sourceSha256"]:
                raise RuntimeError(
                    f"regenerated={regenerated}, committed={committed}, "
                    f"manifest={row['sourceSha256']}")
            measured, triangles, collision, collision_triangles = validate_asset(TARGET, row)
            validate_world()
            print(f"living_fireplace_prepare: {triangles} triangles {measured} m "
                  f"{collision}/{collision_triangles} {committed[:16]} deterministic")
    except (KeyError, OSError, RuntimeError, StopIteration, TypeError, ValueError) as error:
        print(f"living_fireplace_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
