#!/usr/bin/env python3
"""Regenerate and validate HOUSE-01067's family-room window treatment."""

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

SCRIPT = REPO / "tools" / "blender" / "family_window_treatment.py"
TARGET = REPO / "assets-src" / "Models" / "Furniture" / "Family" / "window_treatment.glb"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSET_ID = "MODEL_FAMILY_WINDOW_TREATMENT"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(folder: Path) -> Path:
    output = folder / TARGET.name
    command = blender_env.build_command(SCRIPT, ["--out", str(output)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot regenerate the family curtains")
    completed = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                               capture_output=True, text=True, check=False)
    if completed.returncode != 0 or \
            "family_window_treatment: EXIT 0" not in completed.stdout.splitlines():
        tail = "\n".join((completed.stdout + "\n" + completed.stderr).splitlines()[-40:])
        raise RuntimeError(f"Blender generation failed (exit {completed.returncode}):\n{tail}")
    return output


def inspect(path: Path) -> tuple[list[float], int, set[str], set[str]]:
    document, _ = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError("window treatment has no authoritative POSITION bounds")
    measured = [round(high - low, 6) for low, high in zip(*bounds)]
    nodes = [node for node in document["nodes"] if "mesh" in node]
    primitives = [primitive for node in nodes
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise RuntimeError("a close-range curtain component lacks UV0")
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives}
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    names = {node.get("name", "") for node in nodes}
    return measured, triangles, materials, names


def validate(path: Path, row: dict) -> tuple[list[float], int]:
    measured, triangles, materials, names = inspect(path)
    if measured != row["geometry"]["boundsMetres"]:
        raise RuntimeError(f"manifest bounds differ from measured {measured}")
    if triangles != row["geometry"]["triangles"]["LOD0"]:
        raise RuntimeError(f"manifest triangle count differs from measured {triangles}")
    if materials != {"CURTAIN_FABRIC", "CURTAIN_HARDWARE"} or row["materialMap"] != {
            "CURTAIN_FABRIC": "MAT_FAMILY_CURTAIN_WOOL",
            "CURTAIN_HARDWARE": "MAT_KITCHEN_HARDWARE_STEEL"}:
        raise RuntimeError("window treatment lost its two approved canonical material roles")
    required = {"family_curtain_left_panel", "family_curtain_right_panel",
                "family_curtain_rod", "family_curtain_bracket_-1",
                "family_curtain_bracket_1"}
    if not required.issubset(names) or \
            len([name for name in names if "_tab_" in name]) != 10 or \
            len([name for name in names if name.endswith("_tieback")]) != 2:
        raise RuntimeError("window treatment lost a panel, support, tab or physical tieback")
    if any(name.endswith("_COL") for name in names) or "collision" in row["geometry"]:
        raise RuntimeError("soft window dressing must not add collision")
    problems = scale_check.check(path, "window-treatment", row["geometry"])
    if problems:
        raise RuntimeError("; ".join(problems))
    problems = origin_check.check(path, "window-treatment")
    if problems:
        raise RuntimeError("; ".join(problems))

    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    expected = {
        "PROP_FAMILY_CURTAIN_NORTH": ([5.70, 0.6, -27.10], 180),
        "PROP_FAMILY_CURTAIN_EAST": ([8.70, 0.6, -24.55], 90),
    }
    for identifier, (position, yaw) in expected.items():
        prop = props[identifier]
        if prop["asset"] != ASSET_ID or prop["cell"] != "L0_FAMILY" or \
                prop["position"] != position or prop["yawDeg"] != yaw or \
                prop["scale"] != 1 or not prop["static"] or \
                prop["collision"] != "none" or prop["material"] is not None:
            raise RuntimeError(f"{identifier}: canonical window alignment changed")
    return measured, triangles


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    options = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix="house01067-family-curtains-", dir="/tmp") as scratch:
            generated = generate(Path(scratch))
            if options.write:
                TARGET.parent.mkdir(parents=True, exist_ok=True)
                TARGET.write_bytes(generated.read_bytes())
                measured, triangles, _materials, _names = inspect(TARGET)
                print(f"family_window_treatment_prepare: bounds={measured} triangles={triangles} "
                      f"sha256={sha256(TARGET)}")
                return 0

            row = next(row for row in json.loads(MANIFEST.read_text())["assets"]
                       if row["id"] == ASSET_ID)
            regenerated = sha256(generated)
            committed = sha256(TARGET)
            if regenerated != committed or regenerated != row["sourceSha256"]:
                raise RuntimeError(
                    f"regenerated={regenerated}, committed={committed}, "
                    f"manifest={row['sourceSha256']}")
            measured, triangles = validate(TARGET, row)
            print(f"family_window_treatment_prepare: {triangles} triangles {measured} m "
                  f"{committed[:16]} deterministic")
    except (KeyError, OSError, RuntimeError, StopIteration, TypeError, ValueError) as error:
        print(f"family_window_treatment_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
