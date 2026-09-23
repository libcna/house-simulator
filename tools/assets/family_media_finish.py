#!/usr/bin/env python3
"""Protect the family room's finished television and media-surround material roles.

The selected CC-BY television remains in use and the former media unit remains a stable approved
asset after HOUSE-01053 replaces that one instance. Their material names are coarse: the
44-triangle television has one palette slot, while the legacy unit's `BlackMarble` node uses its
second slot. HOUSE-01050 maps those exact roles to dark physical finishes; HOUSE-01053's dedicated
checker owns the replacement console and measured TV/console alignment.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "assets"))
import gltf_io  # noqa: E402
sys.path.insert(0, str(REPO / "tools" / "world"))
import layout_io  # noqa: E402


def manifest_rows() -> dict[str, dict]:
    document = json.loads((REPO / "assets-src/assets.manifest.json").read_text(encoding="utf-8"))
    return {row["id"]: row for row in document["assets"]}


def material_rows() -> dict[str, dict]:
    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["materials"])
    return layout_io.by_id(layout_io.rows(layout, "materials"), "material")


def mesh_materials(path: Path) -> dict[str, str]:
    document, _ = gltf_io.read_model(path)
    result: dict[str, str] = {}
    for node in document.get("nodes", []):
        if "mesh" not in node:
            continue
        mesh = document["meshes"][node["mesh"]]
        names = {
            document["materials"][primitive["material"]]["name"]
            for primitive in mesh.get("primitives", [])
            if "material" in primitive
        }
        if len(names) == 1:
            result[node.get("name", "")] = next(iter(names))
    return result


def check() -> None:
    assets = manifest_rows()
    television = assets["MODEL_FURNITURE_TV"]
    media = assets["MODEL_FURNITURE_TV_UNIT"]
    if television["materialMap"] != {"PaletteMaterial002": "MAT_FAMILY_TV_SCREEN"}:
        raise RuntimeError("television: its sole source slot is no longer the dark screen role")
    if media["materialMap"] != {
        "PaletteMaterial001": "MAT_FURNITURE_WHITE_ROOM_PALETTE",
        "PaletteMaterial002": "MAT_FAMILY_MEDIA_STONE",
    }:
        raise RuntimeError("media unit: painted carcass and BlackMarble roles are no longer split")

    tv_nodes = mesh_materials(REPO / television["sourceFile"])
    if tv_nodes.get("TvScreen") != "PaletteMaterial002" or \
            tv_nodes.get("TvBevel") != "PaletteMaterial002":
        raise RuntimeError("television: expected TvScreen and TvBevel on PaletteMaterial002")
    media_nodes = mesh_materials(REPO / media["sourceFile"])
    if media_nodes.get("BlackMarble") != "PaletteMaterial002":
        raise RuntimeError("media unit: BlackMarble no longer uses the mapped second source slot")

    materials = material_rows()
    screen = materials["MAT_FAMILY_TV_SCREEN"]
    stone = materials["MAT_FAMILY_MEDIA_STONE"]
    if screen["effectTierS"] != "Basic" or max(screen["tint"]) > 0.18 or \
            screen["specularPower"] < 80.0:
        raise RuntimeError("television: inactive screen is no longer dark, glossy stock Basic")
    if stone["effectTierS"] != "Basic" or max(stone["tint"]) > 0.28 or \
            stone["class"] != "stone":
        raise RuntimeError("media unit: hearth finish is no longer a bounded dark stone")

    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props"])
    props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
    tv_prop = props["PROP_FAMILY_TV"]
    unit_prop = props["PROP_FAMILY_TV_UNIT"]
    if tv_prop["position"] != [4.1, 1.254, -22.33] or tv_prop["yawDeg"] != 90:
        raise RuntimeError("television: canonical wall alignment changed")
    if unit_prop["position"] != [4.1, 0.6, -22.55] or unit_prop["yawDeg"] != 90:
        raise RuntimeError("media unit: canonical wall alignment changed")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="validate the canonical finish")
    parser.parse_args()
    try:
        check()
        print("family_media_finish: television screen and BlackMarble media role are physical")
        return 0
    except (KeyError, OSError, RuntimeError, ValueError) as error:
        print(f"family_media_finish: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
