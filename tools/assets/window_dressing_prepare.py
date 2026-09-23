#!/usr/bin/env python3
"""Regenerate and validate HOUSE-02681's bounded curtain-and-blind family."""

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

SCRIPT = REPO / "tools" / "blender" / "static_blind.py"
TARGET = REPO / "assets-src" / "Models" / "Furniture" / "Generated" / "static_blind.glb"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSET_ID = "MODEL_WINDOW_BLIND"
CURTAIN_ID = "MODEL_FAMILY_WINDOW_TREATMENT"
PROP_ID = "PROP_L1_BED2_BLIND_EAST"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(folder: Path) -> Path:
    output = folder / TARGET.name
    command = blender_env.build_command(SCRIPT, ["--out", str(output)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot regenerate the static blind")
    completed = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                               capture_output=True, text=True, check=False)
    if completed.returncode != 0 or "static_blind: EXIT 0" not in completed.stdout.splitlines():
        tail = "\n".join((completed.stdout + "\n" + completed.stderr).splitlines()[-40:])
        raise RuntimeError(f"Blender generation failed (exit {completed.returncode}):\n{tail}")
    return output


def inspect(path: Path) -> tuple[list[float], int, set[str], set[str]]:
    document, _ = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError("blind has no authoritative POSITION bounds")
    measured = [round(high - low, 6) for low, high in zip(*bounds)]
    nodes = [node for node in document["nodes"] if "mesh" in node]
    primitives = [primitive for node in nodes
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise RuntimeError("a blind component lacks UV0")
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives}
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    names = {node.get("name", "") for node in nodes}
    return measured, triangles, materials, names


def validate(path: Path, rows: dict[str, dict]) -> tuple[list[float], int]:
    row = rows[ASSET_ID]
    measured, triangles, materials, names = inspect(path)
    if measured != row["geometry"]["boundsMetres"] or \
            triangles != row["geometry"]["triangles"]["LOD0"]:
        raise RuntimeError(f"blind manifest geometry differs from measured {measured}/{triangles}")
    if materials != {"BLIND_SLAT"} or row["materialMap"] != {
            "BLIND_SLAT": "MAT_WINDOW_BLIND_BLUE"}:
        raise RuntimeError("blind lost its approved tintable canonical material role")
    if len([name for name in names if name.startswith("blind_slat_")]) != 20 or not {
            "blind_head_rail", "blind_bottom_rail", "blind_ladder_left",
            "blind_ladder_right"}.issubset(names):
        raise RuntimeError("blind lost a slat, rail or ladder cord")
    if any(name.endswith("_COL") for name in names) or "collision" in row["geometry"]:
        raise RuntimeError("soft window dressing must not add collision")
    for checker in (scale_check.check(path, "window-blind", row["geometry"]),
                    origin_check.check(path, "window-blind")):
        if checker:
            raise RuntimeError("; ".join(checker))

    curtain = rows[CURTAIN_ID]
    if curtain["category"] != "window-treatment" or curtain["materialMap"] != {
            "CURTAIN_FABRIC": "MAT_FAMILY_CURTAIN_WOOL",
            "CURTAIN_HARDWARE": "MAT_KITCHEN_HARDWARE_STEEL"}:
        raise RuntimeError("the retained curtain family is no longer static canonical dressing")

    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props", "portals"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    prop = props[PROP_ID]
    if prop["asset"] != ASSET_ID or prop["cell"] != "L1_BED2" or \
            prop["position"] != [8.7, 5.3, -25.0] or prop["yawDeg"] != 90 or \
            prop["scale"] != [0.92, 1.0, 1.0] or prop["tint"] != [0.64, 0.74, 0.9] or \
            not prop["static"] or prop["collision"] != "none" or \
            prop["material"] is not None or prop["interactable"] is not None:
        raise RuntimeError(f"{PROP_ID}: canonical recipe placement or variation changed")
    curtain_props = [entry for entry in props.values()
                     if entry["asset"] == CURTAIN_ID and entry["collision"] == "none"
                     and entry["interactable"] is None]
    if len(curtain_props) < 2:
        raise RuntimeError("the curtain family no longer has its two static recipe placements")

    portals = layout_io.by_id(layout_io.rows(layout, "portals"), "portal")
    for identifier in ("P_L0_FAMILY__W1", "P_L1_BED2__W1"):
        portal = portals[identifier]
        if portal.get("opacity") != "glass":
            raise RuntimeError(f"{identifier}: window portal no longer retains glass opacity")
        forbidden = set(portal) & {"blindFraction", "interaction", "translucent"}
        if forbidden:
            raise RuntimeError(f"{identifier}: forbidden interactive blind state {sorted(forbidden)}")
    return measured, triangles


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    options = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix="house02681-window-dressing-", dir="/tmp") as scratch:
            generated = generate(Path(scratch))
            if options.write:
                TARGET.parent.mkdir(parents=True, exist_ok=True)
                TARGET.write_bytes(generated.read_bytes())
                measured, triangles, _materials, _names = inspect(TARGET)
                print(f"window_dressing_prepare: bounds={measured} triangles={triangles} "
                      f"sha256={sha256(TARGET)}")
                return 0

            rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
            regenerated = sha256(generated)
            committed = sha256(TARGET)
            if regenerated != committed or regenerated != rows[ASSET_ID]["sourceSha256"]:
                raise RuntimeError(
                    f"regenerated={regenerated}, committed={committed}, "
                    f"manifest={rows[ASSET_ID]['sourceSha256']}")
            measured, triangles = validate(TARGET, rows)
            print(f"window_dressing_prepare: curtain+blind static family; {triangles} triangles "
                  f"{measured} m {committed[:16]} deterministic")
    except (KeyError, OSError, RuntimeError, StopIteration, TypeError, ValueError) as error:
        print(f"window_dressing_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
