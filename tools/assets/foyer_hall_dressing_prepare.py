#!/usr/bin/env python3
"""Regenerate and validate HOUSE-01064/01071's foyer/hall arrival dressing."""

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

SCRIPT = REPO / "tools" / "blender" / "foyer_hall_dressing.py"
FOYER_TARGET = REPO / "assets-src" / "Models" / "Furniture" / "Foyer"
HALL_TARGET = REPO / "assets-src" / "Models" / "Furniture" / "Hall"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSETS = {
    "console": ("MODEL_FOYER_CONSOLE_DRESSING", FOYER_TARGET / "console_dressing.glb",
                "console-decor"),
    "runner": ("MODEL_HALL_RUNNER", HALL_TARGET / "hall_runner.glb", "rug"),
    "entry": ("MODEL_FOYER_ENTRY_RUG", FOYER_TARGET / "entry_rug.glb", "rug"),
    "art": ("MODEL_HALL_PORTAL_ART_PAIR", HALL_TARGET / "portal_art_pair.glb", "picture"),
}
EXPECTED_MATERIALS = {
    "console": {"ARRIVAL_WOOD", "ARRIVAL_BRASS", "ARRIVAL_CANVAS", "ARRIVAL_CREAM",
                "ARRIVAL_CERAMIC"},
    "runner": {"ARRIVAL_RUNNER_BASE", "ARRIVAL_RUNNER_BORDER"},
    "entry": {"ARRIVAL_RUNNER_BASE", "ARRIVAL_RUNNER_BORDER", "ARRIVAL_ENTRY_LIGHT"},
    "art": {"ARRIVAL_WOOD", "ARRIVAL_BRASS", "ARRIVAL_CANVAS", "ARRIVAL_CREAM"},
}
EXPECTED_MAPS = {
    "console": {
        "ARRIVAL_WOOD": "MAT_FURNITURE_PIANO_WOOD",
        "ARRIVAL_BRASS": "MAT_FURNITURE_PIANO_BRASS",
        "ARRIVAL_CANVAS": "MAT_FAMILY_ART_CANVAS",
        "ARRIVAL_CREAM": "MAT_LIVING_ART_PAPER",
        "ARRIVAL_CERAMIC": "MAT_FOYER_CERAMIC",
    },
    "runner": {
        "ARRIVAL_RUNNER_BASE": "MAT_HALL_RUNNER_WOOL",
        "ARRIVAL_RUNNER_BORDER": "MAT_HALL_RUNNER_BORDER_WOOL",
    },
    "entry": {
        "ARRIVAL_RUNNER_BASE": "MAT_HALL_RUNNER_WOOL",
        "ARRIVAL_RUNNER_BORDER": "MAT_HALL_RUNNER_BORDER_WOOL",
        "ARRIVAL_ENTRY_LIGHT": "MAT_LIVING_RUG_WOOL",
    },
    "art": {
        "ARRIVAL_WOOD": "MAT_FURNITURE_PIANO_WOOD",
        "ARRIVAL_BRASS": "MAT_FURNITURE_PIANO_BRASS",
        "ARRIVAL_CANVAS": "MAT_FAMILY_ART_CANVAS",
        "ARRIVAL_CREAM": "MAT_LIVING_ART_PAPER",
    },
}
REQUIRED_COMPONENTS = {
    "console": ("vase_body", "branch_leaf", "photo_field", "key_tray", "keys"),
    "runner": ("runner_body", "runner_long_border", "runner_diamond", "runner_fringe"),
    "entry": ("entry_rug_body", "entry_rug_field", "entry_centre_medallion",
              "entry_centre_inlay", "entry_corner_mark"),
    "art": ("relief_back", "relief_canvas", "relief_horizon", "relief_mark"),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(folder: Path, name: str) -> Path:
    output = folder / ASSETS[name][1].name
    command = blender_env.build_command(SCRIPT, ["--asset", name, "--out", str(output)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot regenerate foyer/hall dressing")
    result = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                            capture_output=True, text=True, check=False)
    if result.returncode != 0 or \
            "foyer_hall_dressing: EXIT 0" not in result.stdout.splitlines():
        tail = "\n".join((result.stdout + "\n" + result.stderr).splitlines()[-40:])
        raise RuntimeError(f"Blender failed for {name} (exit {result.returncode}):\n{tail}")
    if not output.is_file():
        raise RuntimeError(f"generation produced no {output.name}")
    return output


def inspect(path: Path) -> tuple[list[float], int, set[str], set[str]]:
    document, _ = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError(f"{path.name}: no authoritative POSITION bounds")
    measured = [round(high - low, 6) for low, high in zip(*bounds)]
    nodes = [node for node in document["nodes"] if "mesh" in node and
             not node.get("name", "").endswith("_COL")]
    primitives = [primitive for node in nodes
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives if "material" in primitive}
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise RuntimeError(f"{path.name}: a close-range component lacks metre-scaled UV0")
    return measured, triangles, materials, {node.get("name", "") for node in nodes}


def validate(path: Path, name: str, row: dict) -> tuple[list[float], int]:
    measured, triangles, materials, names = inspect(path)
    if measured != row["geometry"]["boundsMetres"]:
        raise RuntimeError(f"{name}: manifest bounds differ from measured {measured}")
    if triangles != row["geometry"]["triangles"]["LOD0"]:
        raise RuntimeError(f"{name}: manifest triangle count differs from measured {triangles}")
    if materials != EXPECTED_MATERIALS[name] or row["materialMap"] != EXPECTED_MAPS[name]:
        raise RuntimeError(f"{name}: canonical finishes changed")
    for fragment in REQUIRED_COMPONENTS[name]:
        if not any(fragment in node for node in names):
            raise RuntimeError(f"{name}: missing authored component {fragment}")
    if any(node.endswith("_COL") for node in names) or "collision" in row["geometry"]:
        raise RuntimeError(f"{name}: arrival dressing must not add collision")
    problems = scale_check.check(path, ASSETS[name][2], row["geometry"])
    if problems:
        raise RuntimeError(f"{name}: {'; '.join(problems)}")
    problems = origin_check.check(path, ASSETS[name][2])
    if problems:
        raise RuntimeError(f"{name}: {'; '.join(problems)}")
    return measured, triangles


def validate_world(rows: dict[str, dict]) -> None:
    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["cells", "props"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    expected = {
        "PROP_FOYER_CONSOLE_DRESSING": (ASSETS["console"][0], "L0_FOYER",
                                          [-1.39, 1.207883, -14.52], 0),
        "PROP_HALL_RUNNER": (ASSETS["runner"][0], "L0_HALL", [0.0, 0.602, -20.65], 0),
        "PROP_FOYER_ENTRY_RUG": (ASSETS["entry"][0], "L0_FOYER",
                                   [0.0, 0.602, -16.55], 0),
        "PROP_HALL_PORTAL_ART_PAIR": (ASSETS["art"][0], "L0_HALL",
                                       [0.0, 1.90, -22.98], 180),
    }
    for prop_id, (asset_id, cell, position, yaw) in expected.items():
        prop = props[prop_id]
        if prop["asset"] != asset_id or prop["cell"] != cell or \
                prop["position"] != position or prop["yawDeg"] != yaw or \
                prop["scale"] != 1 or not prop["static"] or prop["collision"] != "none":
            raise RuntimeError(f"canonical arrival placement changed: {prop_id}")

    console = props["PROP_FOYER_CONSOLE"]
    console_top = (console["position"][1] +
                   rows["MODEL_FURNITURE_FOYER_CONSOLE"]["geometry"]["boundsMetres"][1] *
                   console["scale"])
    if abs(console_top - props["PROP_FOYER_CONSOLE_DRESSING"]["position"][1]) > 0.000001:
        raise RuntimeError(f"console dressing floats or clips: console top {console_top:.6f}")

    cells = layout_io.by_id(layout_io.rows(layout, "cells"), "cell")
    hall_box = cells["L0_HALL"]["boxes"][0]
    art = props["PROP_HALL_PORTAL_ART_PAIR"]
    if abs(art["position"][2] - hall_box["z"][0]) > 0.020001:
        raise RuntimeError("portal relief is no longer within 20 mm of the real kitchen-end wall")
    runner = props["PROP_HALL_RUNNER"]
    width = rows[ASSETS["runner"][0]]["geometry"]["boundsMetres"][0]
    side_clearance = ((hall_box["x"][1] - hall_box["x"][0]) - width) * 0.5
    if abs(runner["position"][1] - 0.602) > 0.000001 or side_clearance < 1.20:
        raise RuntimeError("hall runner no longer retains the measured circulation clearance")
    entry = props["PROP_FOYER_ENTRY_RUG"]
    foyer_box = cells["L0_FOYER"]["boxes"][0]
    entry_width, _, entry_length = rows[ASSETS["entry"][0]]["geometry"]["boundsMetres"]
    front_clearance = foyer_box["z"][1] - (entry["position"][2] + entry_length * 0.5)
    hall_clearance = (entry["position"][2] - entry_length * 0.5) - foyer_box["z"][0]
    side_clearance = ((foyer_box["x"][1] - foyer_box["x"][0]) - entry_width) * 0.5
    if (entry_width < 2.0 or entry_length < 2.4 or front_clearance < 0.96 or
            hall_clearance < 0.45 or side_clearance < 1.10):
        raise RuntimeError("foyer entry rug blocks door swing, hall or side circulation")
    plant = props["PROP_FOYER_ARRIVAL_PLANT"]
    if (plant["asset"] != "MODEL_FURNITURE_POTTED_PLANT_A" or
            plant["cell"] != "L0_FOYER" or plant["position"] != [-1.67, 0.6, -17.76] or
            plant["yawDeg"] != 35 or plant["scale"] != 1.2 or
            plant["collision"] != "none"):
        raise RuntimeError("foyer arrival plant placement changed")
    plant_width = rows["MODEL_FURNITURE_POTTED_PLANT_A"]["geometry"]["boundsMetres"][0]
    plant_depth = rows["MODEL_FURNITURE_POTTED_PLANT_A"]["geometry"]["boundsMetres"][2]
    if (plant["position"][0] - plant_width * plant["scale"] * 0.5 < -2.20 or
            plant["position"][2] - plant_depth * plant["scale"] * 0.5 < -18.30 or
            plant["position"][2] + plant_depth * plant["scale"] * 0.5 > -17.30):
        raise RuntimeError("foyer arrival plant intersects the living or hall opening")
    art_width = rows[ASSETS["art"][0]]["geometry"]["boundsMetres"][0]
    if art_width < 3.50 or art_width > 4.10:
        raise RuntimeError("portal relief no longer spans both flanks as one composed asset")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    options = parser.parse_args()
    try:
        rows = ({row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
                if options.check else {})
        with tempfile.TemporaryDirectory(prefix="house01064-arrival-dressing-", dir="/tmp") as scratch:
            for name, (asset_id, target, _category) in ASSETS.items():
                generated = generate(Path(scratch), name)
                if options.write:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(generated.read_bytes())
                    measured, triangles, _materials, _names = inspect(target)
                    print(f"foyer_hall_dressing_prepare: {name} bounds={measured} "
                          f"triangles={triangles} sha256={sha256(target)}")
                    continue
                if not target.is_file():
                    raise RuntimeError(f"committed dressing model is missing: {target}")
                actual = sha256(generated)
                committed = sha256(target)
                row = rows[asset_id]
                if actual != committed or actual != row["sourceSha256"]:
                    raise RuntimeError(
                        f"{name}: regenerated={actual}, committed={committed}, "
                        f"manifest={row['sourceSha256']}")
                measured, triangles = validate(target, name, row)
                print(f"foyer_hall_dressing_prepare: {name} {triangles} triangles "
                      f"{measured} m {actual[:16]} deterministic")
            if options.check:
                validate_world(rows)
    except (KeyError, OSError, RuntimeError, TypeError, ValueError) as error:
        print(f"foyer_hall_dressing_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
