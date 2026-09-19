#!/usr/bin/env python3
"""Regenerate and validate the measured eight-place dining table setting."""

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

SCRIPT = REPO / "tools" / "blender" / "dining_table_setting.py"
MODEL = REPO / "assets-src" / "Models" / "Furniture" / "Dining" / "table_setting.glb"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSET = "MODEL_DINING_TABLE_SETTING"
PROP = "PROP_L0_DINING_TABLE_SETTING"


def hash_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check() -> None:
    assets = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
    row = assets[ASSET]
    with tempfile.TemporaryDirectory(prefix="house01073-dining-", dir="/tmp") as scratch:
        regenerated = Path(scratch) / "table_setting.glb"
        command = blender_env.build_command(SCRIPT, ["--out", str(regenerated)])
        if command is None:
            raise RuntimeError("Blender unavailable; cannot validate the dining setting")
        result = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                                capture_output=True, text=True, check=False)
        if result.returncode != 0 or "dining_table_setting: EXIT 0" not in result.stdout.splitlines():
            tail = "\n".join((result.stdout + "\n" + result.stderr).splitlines()[-20:])
            raise RuntimeError(f"Blender regeneration failed:\n{tail}")
        if hash_file(regenerated) != hash_file(MODEL) or hash_file(MODEL) != row["sourceSha256"]:
            raise RuntimeError("regenerated, committed and manifest GLB hashes disagree")

    document, _ = gltf_io.read_model(MODEL)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError("setting has no authoritative POSITION bounds")
    measured = [high - low for low, high in zip(*bounds)]
    if any(abs(actual - declared) > 0.005 for actual, declared in
           zip(measured, row["geometry"]["boundsMetres"])):
        raise RuntimeError(f"manifest bounds disagree with {measured}")
    problems = scale_check.check(MODEL, "table-setting", row["geometry"])
    if problems:
        raise RuntimeError("; ".join(problems))
    if abs(bounds[0][1]) > 0.001:
        raise RuntimeError("setting's support plane must be local Y=0")
    nodes = [node for node in document["nodes"] if "mesh" in node]
    names = [node.get("name", "") for node in nodes]
    if any(name.endswith("_COL") for name in names):
        raise RuntimeError("non-collidable table dressing contains a collision proxy")
    for fragment, expected in (("plate_rim", 8), ("plate_well", 8),
                               ("napkin_", 6), ("linen_runner", 1),
                               ("centre_bowl_body", 1)):
        if sum(fragment in name for name in names) != expected:
            raise RuntimeError(f"expected {expected} component(s) named {fragment}")
    primitives = [primitive for node in nodes
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    if triangles != row["geometry"]["triangles"]["LOD0"] or triangles > 5000:
        raise RuntimeError(f"unexpected visible triangle count {triangles}")
    if any("TEXCOORD_0" not in primitive["attributes"] for primitive in primitives):
        raise RuntimeError("a close-range component lacks UV0")
    source_roles = {document["materials"][primitive["material"]]["name"]
                    for primitive in primitives}
    if source_roles != set(row["materialMap"]):
        raise RuntimeError(f"source/material-map roles disagree: {sorted(source_roles)}")

    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    prop = props[PROP]
    table = props["PROP_L0_DINING_TABLE"]
    if (prop["asset"] != ASSET or prop["cell"] != "L0_DINING" or
            prop["position"] != [-5.2, 1.36, -21.6] or prop["yawDeg"] != 0 or
            prop["scale"] != 1 or prop["collision"] != "none"):
        raise RuntimeError("canonical table-setting placement or collision changed")
    if (table["position"] != [-5.2, 0.6, -21.6] or
            abs(prop["position"][1] - table["position"][1] -
                assets[table["asset"]]["geometry"]["boundsMetres"][1]) > 0.001):
        raise RuntimeError("setting does not rest on the physical dining tabletop")
    table_bounds = assets[table["asset"]]["geometry"]["boundsMetres"]
    if measured[0] >= table_bounds[0] or measured[2] >= table_bounds[2]:
        raise RuntimeError("setting extends beyond the dining tabletop")
    print(f"dining_table_setting_prepare: {triangles} triangles, {measured} m, "
          f"eight measured places, sha256 {row['sourceSha256'][:16]} deterministic")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", required=True)
    parser.parse_args()
    try:
        check()
    except (RuntimeError, KeyError, ValueError, StopIteration) as error:
        print(f"dining_table_setting_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
