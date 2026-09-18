#!/usr/bin/env python3
"""Regenerate and validate HOUSE-01062's formal-sofa draped textile accent."""

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

SCRIPT = REPO / "tools" / "blender" / "living_sofa_throw.py"
TARGET = REPO / "assets-src" / "Models" / "Furniture" / "LivingRoom" / "living_sofa_throw.glb"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSET_ID = "MODEL_LIVING_SOFA_THROW"
PROP_ID = "PROP_LIVING_SOFA_THROW"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(folder: Path) -> Path:
    output = folder / TARGET.name
    command = blender_env.build_command(SCRIPT, ["--out", str(output)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot regenerate the authored sofa throw")
    completed = subprocess.run(command, cwd=REPO, env=blender_env.environment(), capture_output=True,
                               text=True, check=False)
    if completed.returncode != 0 or "living_sofa_throw: EXIT 0" not in completed.stdout.splitlines():
        tail = "\n".join((completed.stdout + "\n" + completed.stderr).splitlines()[-40:])
        raise RuntimeError(f"Blender generation failed (exit {completed.returncode}):\n{tail}")
    return output


def inspect(path: Path) -> tuple[list[float], int, set[str], set[str]]:
    document, _ = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError("sofa throw has no authoritative POSITION bounds")
    measured = [round(high - low, 6) for low, high in zip(*bounds)]
    nodes = [node for node in document["nodes"] if "mesh" in node]
    primitives = [primitive for node in nodes
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise RuntimeError("a close-range textile component lacks UV0")
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
    if materials != {"THROW_WOOL"} or row["materialMap"] != {
            "THROW_WOOL": "MAT_LIVING_THROW_WOOL"}:
        raise RuntimeError("sofa throw no longer uses the one approved woven textile role")
    if "living_sofa_throw_body" not in names or \
            len([name for name in names if name.startswith("living_sofa_throw_fringe_")]) != 9:
        raise RuntimeError("sofa throw lost its curved body or nine physical fringe cords")
    if any(name.endswith("_COL") for name in names) or "collision" in row["geometry"]:
        raise RuntimeError("the sofa-mounted textile must not add a collision volume")
    problems = scale_check.check(path, "throw-blanket", row["geometry"])
    if problems:
        raise RuntimeError("; ".join(problems))
    problems = origin_check.check(path, "throw-blanket")
    if problems:
        raise RuntimeError("; ".join(problems))

    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props"])
    prop = layout_io.by_id(layout_io.rows(layout, "props"), "prop")[PROP_ID]
    if prop["asset"] != ASSET_ID or prop["cell"] != "L0_LIVING" or \
            prop["position"] != [-4.12, 0.92, -19.11] or prop["yawDeg"] != 0 or \
            prop["scale"] != 0.75 or not prop["static"] or prop["collision"] != "none":
        raise RuntimeError("canonical sofa-throw placement changed")
    return measured, triangles


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    options = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix="house01062-sofa-throw-", dir="/tmp") as scratch:
            generated = generate(Path(scratch))
            if options.write:
                TARGET.parent.mkdir(parents=True, exist_ok=True)
                TARGET.write_bytes(generated.read_bytes())
                measured, triangles, _materials, _names = inspect(TARGET)
                print(f"living_sofa_throw_prepare: bounds={measured} triangles={triangles} "
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
            print(f"living_sofa_throw_prepare: {triangles} triangles {measured} m "
                  f"{committed[:16]} deterministic")
    except (KeyError, OSError, RuntimeError, StopIteration, TypeError, ValueError) as error:
        print(f"living_sofa_throw_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
