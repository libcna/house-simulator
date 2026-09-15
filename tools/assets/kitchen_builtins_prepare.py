#!/usr/bin/env python3
"""Check the two committed project-authored kitchen built-ins against a fresh Blender export.

No network or third-party model cache is involved. The temporary Blender products are generated
under /tmp; no compiler or CMake build is run there. `--check` compares final collidable GLBs and
their manifest SHA-256 identities, making the source authoring script durable rather than a one-off
asset dump.
"""

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

NAMES = ("north_run", "island")
SCRIPT = REPO / "tools" / "blender" / "kitchen_builtins.py"
COLLISION = REPO / "tools" / "blender" / "collision_proxy.py"
MODELS = REPO / "assets-src" / "Models" / "Furniture" / "Kitchen"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSET_IDS = {"north_run": "MODEL_KITCHEN_NORTH_BASE_RUN",
             "island": "MODEL_KITCHEN_ISLAND"}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def checked_run(command: list[str], *, environment=None, sentinel: str | None = None) -> None:
    result = subprocess.run(command, cwd=REPO, env=environment,
                            capture_output=True, text=True, check=False)
    if result.returncode != 0 or (sentinel and sentinel not in result.stdout.splitlines()):
        tail = "\n".join((result.stdout + "\n" + result.stderr).splitlines()[-28:])
        raise RuntimeError(f"{command[0]} failed (exit {result.returncode}):\n{tail}")


def check_manifest_geometry(path: Path, row: dict) -> None:
    document, _ = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError(f"{path.name}: no authoritative POSITION bounds")
    measured = [high - low for low, high in zip(*bounds)]
    declared = row["geometry"]["boundsMetres"]
    if any(abs(actual - stored) > 0.005 for actual, stored in zip(measured, declared)):
        raise RuntimeError(f"{path.name}: geometry.boundsMetres {declared} != {measured}")
    visible = sum(document["accessors"][document["meshes"][node["mesh"]]
                  ["primitives"][0]["indices"]]["count"] // 3
                  for node in document["nodes"] if "mesh" in node and
                  not node.get("name", "").endswith("_COL"))
    if visible != row["geometry"]["triangles"]["LOD0"]:
        raise RuntimeError(f"{path.name}: declared LOD0 triangles do not equal {visible}")
    if row["geometry"]["collision"] not in {node.get("name") for node in document["nodes"]}:
        raise RuntimeError(f"{path.name}: declared collision proxy node is missing")
    stone_tops = [document["accessors"][primitive["attributes"]["POSITION"]]["max"][1]
                  for mesh in document["meshes"] for primitive in mesh["primitives"]
                  if "material" in primitive and
                  document["materials"][primitive["material"]]["name"] == "CAB_STONE"]
    if not stone_tops:
        raise RuntimeError(f"{path.name}: working-surface stone is missing")
    actual_top = max(stone_tops)
    expected_top = row["geometry"].get("counterHeightMetres", 0.94)
    if abs(actual_top - expected_top) > 0.005:
        raise RuntimeError(
            f"{path.name}: stone top is {actual_top:.3f} m, manifest says "
            f"{expected_top:.3f} m")


def check() -> None:
    rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
    with tempfile.TemporaryDirectory(prefix="house01040-builtins-", dir="/tmp") as scratch:
        folder = Path(scratch)
        command = blender_env.build_command(SCRIPT, ["--out", str(folder)])
        if command is None:
            raise RuntimeError("Blender unavailable; cannot validate authored built-ins")
        checked_run(command, environment=blender_env.environment(),
                    sentinel="kitchen_builtins: EXIT 0")
        for name in NAMES:
            source = folder / f"{name}_raw.glb"
            generated = folder / f"{name}.glb"
            if not source.is_file():
                raise RuntimeError(f"{source}: Blender did not export this model")
            checked_run([sys.executable, str(COLLISION), str(source), str(generated),
                         "--mode", "box"])
            if not generated.is_file():
                raise RuntimeError(f"{generated}: collision exporter produced no GLB")
            expected = MODELS / generated.name
            if not expected.is_file():
                raise RuntimeError(f"{expected}: committed source model is missing")
            actual_hash = sha256(generated.read_bytes())
            committed_hash = sha256(expected.read_bytes())
            manifest_hash = rows[ASSET_IDS[name]]["sourceSha256"]
            if actual_hash != committed_hash or actual_hash != manifest_hash:
                raise RuntimeError(
                    f"{name}: regenerated={actual_hash}, committed={committed_hash}, "
                    f"manifest={manifest_hash}")
            check_manifest_geometry(expected, rows[ASSET_IDS[name]])
            print(f"kitchen_builtins_prepare: {name} {actual_hash[:16]} deterministic")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", required=True)
    parser.parse_args()
    try:
        check()
    except (RuntimeError, KeyError, ValueError) as error:
        print(f"kitchen_builtins_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
