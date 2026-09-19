#!/usr/bin/env python3
"""Verify the project-authored butler-pantry service run and its measured placement."""

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

SCRIPT = REPO / "tools" / "blender" / "butlers_service_run.py"
COLLISION = REPO / "tools" / "blender" / "collision_proxy.py"
MODEL = REPO / "assets-src" / "Models" / "Furniture" / "Kitchen" / "butlers_run.glb"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSET = "MODEL_BUTLERS_SERVICE_RUN"
PROP = "PROP_L0_BUTLERS_SERVICE_RUN"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str], *, environment: dict | None = None,
        sentinel: str | None = None) -> None:
    result = subprocess.run(command, cwd=REPO, env=environment,
                            capture_output=True, text=True, check=False)
    if result.returncode != 0 or (sentinel and sentinel not in result.stdout.splitlines()):
        tail = "\n".join((result.stdout + "\n" + result.stderr).splitlines()[-20:])
        raise RuntimeError(f"{command[0]} failed (exit {result.returncode}):\n{tail}")


def check() -> None:
    rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
    row = rows[ASSET]
    with tempfile.TemporaryDirectory(prefix="house01072-butlers-", dir="/tmp") as scratch:
        folder = Path(scratch)
        raw = folder / "butlers_run_raw.glb"
        generated = folder / "butlers_run.glb"
        command = blender_env.build_command(SCRIPT, ["--out", str(raw)])
        if command is None:
            raise RuntimeError("Blender unavailable; cannot validate the authored service run")
        run(command, environment=blender_env.environment(),
            sentinel="butlers_service_run: EXIT 0")
        run([sys.executable, str(COLLISION), str(raw), str(generated), "--mode", "box"])
        actual = sha256(generated)
        if actual != sha256(MODEL) or actual != row["sourceSha256"]:
            raise RuntimeError("regenerated, committed and manifest GLB hashes disagree")

    document, _ = gltf_io.read_model(MODEL)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError("service run has no authoritative POSITION bounds")
    measured = [high - low for low, high in zip(*bounds)]
    if any(abs(actual - declared) > 0.005 for actual, declared in
           zip(measured, row["geometry"]["boundsMetres"])):
        raise RuntimeError(f"manifest bounds differ from {measured}")
    visible = [node for node in document["nodes"] if "mesh" in node and
               not node.get("name", "").endswith("_COL")]
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for node in visible
                    for primitive in document["meshes"][node["mesh"]]["primitives"])
    if triangles != row["geometry"]["triangles"]["LOD0"] or triangles > 8000:
        raise RuntimeError(f"unexpected visible triangle count {triangles}")
    if row["geometry"]["collision"] not in {node.get("name") for node in document["nodes"]}:
        raise RuntimeError("declared collision proxy is missing")
    proxy = next(node for node in document["nodes"]
                 if node.get("name") == row["geometry"]["collision"])
    proxy_triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                          for primitive in document["meshes"][proxy["mesh"]]["primitives"])
    if proxy_triangles != 12:
        raise RuntimeError(f"box proxy has {proxy_triangles} triangles")
    names = {node.get("name", "") for node in visible}
    for fragment in ("wine_cooler_glass", "sink_bottom", "tap_spout",
                     "service_drawer_0_inset", "counter_stone_front"):
        if not any(fragment in name for name in names):
            raise RuntimeError(f"missing visible {fragment} component")
    primitives = [primitive for node in visible
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    if any("TEXCOORD_0" not in primitive["attributes"] for primitive in primitives):
        raise RuntimeError("a close-range joinery component lacks UV0")
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives}
    if materials != set(row["materialMap"]):
        raise RuntimeError(f"source/material map roles disagree: {sorted(materials)}")
    stone_top = max(document["accessors"][primitive["attributes"]["POSITION"]]["max"][1]
                    for primitive in primitives if
                    document["materials"][primitive["material"]]["name"] == "CAB_STONE")
    if abs(stone_top - row["geometry"]["counterHeightMetres"]) > 0.005:
        raise RuntimeError(f"stone counter top is {stone_top:.3f} m")

    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props", "portals"])
    prop = layout_io.by_id(layout_io.rows(layout, "props"), "prop")[PROP]
    if (prop["asset"] != ASSET or prop["cell"] != "L0_BUTLERS" or
            prop["position"] != [-12.33, 0.6, -23.8] or prop["yawDeg"] != 270 or
            prop["scale"] != 1 or prop["collision"] != "proxy"):
        raise RuntimeError("canonical service-run placement or collision changed")
    westmost = prop["position"][0] - bounds[1][2]
    eastmost = prop["position"][0] - bounds[0][2]
    northmost = prop["position"][2] + bounds[0][0]
    southmost = prop["position"][2] + bounds[1][0]
    if not (-12.70 < westmost < -12.64 and eastmost < -11.95 and
            -25.0 < northmost and southmost < -22.6):
        raise RuntimeError("service run intersects the west wall or side doors")
    sill = 1.50
    counter_world = prop["position"][1] + stone_top
    faucet_world = prop["position"][1] + bounds[1][1]
    if not (0.0 < sill - counter_world < 0.025 and faucet_world - sill < 0.15):
        raise RuntimeError("counter/faucet no longer clear the low window sill")
    print(f"butlers_service_prepare: {triangles} triangles, {measured} m, "
          f"counter {counter_world:.3f} m, faucet {faucet_world:.3f} m, "
          f"sha256 {actual[:16]} deterministic")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", required=True)
    parser.parse_args()
    try:
        check()
    except (RuntimeError, KeyError, ValueError, StopIteration) as error:
        print(f"butlers_service_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
