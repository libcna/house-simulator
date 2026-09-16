#!/usr/bin/env python3
"""Regenerate and validate the project-authored formal-living upright piano."""

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

SCRIPT = REPO / "tools" / "blender" / "living_piano.py"
COLLISION = REPO / "tools" / "blender" / "collision_proxy.py"
TARGET = REPO / "assets-src" / "Models" / "Furniture" / "LivingRoom" / "upright_piano.glb"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSET_ID = "MODEL_FURNITURE_UPRIGHT_PIANO"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str], *, sentinel: str | None = None) -> str:
    result = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                            capture_output=True, text=True, check=False)
    if result.returncode != 0 or (sentinel and sentinel not in result.stdout.splitlines()):
        tail = "\n".join((result.stdout + "\n" + result.stderr).splitlines()[-30:])
        raise RuntimeError(f"{command[0]} failed (exit {result.returncode}):\n{tail}")
    return result.stdout


def generate(folder: Path) -> Path:
    raw = folder / "upright_piano_raw.glb"
    final = folder / "upright_piano.glb"
    command = blender_env.build_command(SCRIPT, ["--out", str(raw)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot regenerate the authored piano")
    run(command, sentinel="living_piano: EXIT 0")
    run([sys.executable, str(COLLISION), str(raw), str(final), "--mode", "box"])
    if not final.is_file():
        raise RuntimeError("collision exporter produced no upright_piano.glb")
    return final


def validate(path: Path, row: dict) -> None:
    document, _blob = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError("piano has no authoritative POSITION bounds")
    measured = [high - low for low, high in zip(*bounds)]
    declared = row["geometry"]["boundsMetres"]
    if any(abs(actual - stored) > 0.005 for actual, stored in zip(measured, declared)):
        raise RuntimeError(f"manifest bounds {declared} differ from {measured}")
    problems = scale_check.check(path, row["category"], row["geometry"])
    if problems:
        raise RuntimeError("; ".join(problems))

    visible_nodes = [node for node in document["nodes"] if "mesh" in node and
                     not node.get("name", "").endswith("_COL")]
    names = {node.get("name", "") for node in visible_nodes}
    for fragment in ("upper_inset", "key_shelf", "lower_inset", "music_desk",
                     "pedal_stem_0", "pedal_pad_2"):
        if not any(fragment in name for name in names):
            raise RuntimeError(f"piano is missing authored component {fragment}")
    if sum("white_key_" in name for name in names) != 52:
        raise RuntimeError("piano must have 52 independently modelled white keys")
    if sum("black_key_" in name for name in names) != 36:
        raise RuntimeError("piano must have 36 independently modelled black keys")

    materials = {document["materials"][primitive["material"]]["name"]
                 for node in visible_nodes
                 for primitive in document["meshes"][node["mesh"]]["primitives"]
                 if "material" in primitive}
    expected_materials = {"PIANO_WOOD", "PIANO_IVORY", "PIANO_EBONITE", "PIANO_BRASS"}
    if materials != expected_materials:
        raise RuntimeError(f"unexpected piano finishes {sorted(materials)}")
    visible = [primitive for node in visible_nodes
               for primitive in document["meshes"][node["mesh"]]["primitives"]]
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in visible)
    if triangles != row["geometry"]["triangles"]["LOD0"]:
        raise RuntimeError(f"manifest LOD0 triangle count differs from {triangles}")
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in visible):
        raise RuntimeError("a close-range piano component lacks physical UV0")
    if row["geometry"]["collision"] not in names | {
            node.get("name", "") for node in document["nodes"]}:
        raise RuntimeError("declared piano collision proxy is missing")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    options = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix="house01045-piano-", dir="/tmp") as scratch:
            generated = generate(Path(scratch))
            if options.write:
                TARGET.parent.mkdir(parents=True, exist_ok=True)
                TARGET.write_bytes(generated.read_bytes())
                print(f"living_piano_prepare: wrote {TARGET.relative_to(REPO)} {sha256(TARGET)}")
                return 0
            if not TARGET.is_file():
                raise RuntimeError(f"committed piano is missing: {TARGET}")
            rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
            row = rows[ASSET_ID]
            actual = sha256(generated)
            committed = sha256(TARGET)
            if actual != committed or actual != row["sourceSha256"]:
                raise RuntimeError(
                    f"regenerated={actual}, committed={committed}, manifest={row['sourceSha256']}")
            validate(TARGET, row)
            print(f"living_piano_prepare: {actual[:16]} deterministic")
    except (KeyError, RuntimeError, ValueError) as error:
        print(f"living_piano_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
