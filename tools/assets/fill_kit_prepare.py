#!/usr/bin/env python3
"""Regenerate and validate HOUSE-00973's four reusable seeded fill kits."""

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
import scale_check  # noqa: E402

SCRIPT = REPO / "tools" / "blender" / "fill_kit_gen.py"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
MODELS = REPO / "assets-src" / "Models" / "Furniture" / "Fill"
VARIANTS = {
    "books": ("MODEL_FILL_BOOKS", "fill_books.glb", 97301, (0.20, 0.75, 0.12, 0.40)),
    "textiles": ("MODEL_FILL_TEXTILES", "fill_textiles.glb", 97302, (0.28, 0.58, 0.14, 0.42)),
    "crockery": ("MODEL_FILL_CROCKERY", "fill_crockery.glb", 97303, (0.30, 0.70, 0.12, 0.38)),
    "tools": ("MODEL_FILL_TOOLS", "fill_tools.glb", 97304, (0.35, 0.85, 0.10, 0.42)),
}
MATERIAL_MAP = {
    "FILL_CREAM": "MAT_FURNITURE_DINING_LINEN",
    "FILL_BLUE": "MAT_PAINT_DUSTY_BLUE",
    "FILL_GREEN": "MAT_PAINT_SAGE",
    "FILL_OCHRE": "MAT_PAINT_SMOKY_OCHRE",
    "FILL_RED": "MAT_PAINT_PALE_ROSE",
    "FILL_STEEL": "MAT_KITCHEN_HARDWARE_STEEL",
    "FILL_WOOD": "MAT_DOOR_HARDWOOD",
    "FILL_GLASS": "MAT_FOYER_CERAMIC",
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(kind: str, seed: int, target: Path) -> None:
    command = blender_env.build_command(
        SCRIPT, ["--kind", kind, "--seed", str(seed), "--out", str(target)])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot check generated fill kits")
    result = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                            capture_output=True, text=True, check=False)
    sentinel = f"fill_kit_gen: {kind} seed {seed} EXIT 0"
    if result.returncode != 0 or sentinel not in result.stdout.splitlines():
        tail = "\n".join((result.stdout + "\n" + result.stderr).splitlines()[-24:])
        raise RuntimeError(f"{kind} seed {seed}: generator failed ({result.returncode}):\n{tail}")


def measure(path: Path) -> tuple[list[float], int, set[str], list[str]]:
    document, _blob = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError(f"{path.name}: no POSITION bounds")
    low, high = bounds
    size = [high[index] - low[index] for index in range(3)]
    visible = [node for node in document["nodes"] if "mesh" in node]
    primitives = [primitive for node in visible
                  for primitive in document["meshes"][node["mesh"]]["primitives"]]
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in primitives}
    names = [node.get("name", "") for node in visible]
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in primitives):
        raise RuntimeError(f"{path.name}: a visible primitive has no UV0")
    if abs(low[1]) > 0.002:
        raise RuntimeError(f"{path.name}: support origin is {low[1]:.4f} m, expected y=0")
    return size, triangles, materials, names


def check_kind(kind: str, row: dict, folder: Path) -> None:
    asset_id, filename, seed, (min_x, max_x, min_y, max_y) = VARIANTS[kind]
    if row["id"] != asset_id or row["sourceFile"] != f"assets-src/Models/Furniture/Fill/{filename}":
        raise RuntimeError(f"{kind}: manifest identity changed")
    first = folder / filename
    repeated = folder / f"repeat_{filename}"
    alternate = folder / f"alternate_{filename}"
    generate(kind, seed, first)
    generate(kind, seed, repeated)
    generate(kind, seed + 1000, alternate)
    committed = MODELS / filename
    if digest(first) != digest(repeated):
        raise RuntimeError(f"{kind}: the same seed is not byte-reproducible")
    if digest(first) == digest(alternate):
        raise RuntimeError(f"{kind}: two different seeds produced the same asset")
    if digest(first) != digest(committed) or digest(first) != row["sourceSha256"]:
        raise RuntimeError(f"{kind}: regenerated, committed and manifest hashes disagree")

    size, triangles, materials, names = measure(first)
    alt_size, alt_triangles, alt_materials, alt_names = measure(alternate)
    if not (min_x <= size[0] <= max_x and min_y <= size[1] <= max_y and
            0.10 <= size[2] <= 0.50):
        raise RuntimeError(f"{kind}: implausible shelf/surface envelope {size}")
    declared = row["geometry"]["boundsMetres"]
    if any(abs(actual - expected) > 0.005 for actual, expected in zip(size, declared)):
        raise RuntimeError(f"{kind}: measured bounds {size} differ from manifest {declared}")
    if triangles != row["geometry"]["triangles"]["LOD0"] or triangles > 1800:
        raise RuntimeError(f"{kind}: unexpected {triangles} visible triangles")
    if not materials <= set(MATERIAL_MAP) or row["materialMap"] != MATERIAL_MAP:
        raise RuntimeError(f"{kind}: source/canonical material bridge changed")
    # Geometry and colour both vary.  The alternate may coincidentally retain the same total
    # triangle count, so node names/count and used material set form the remaining visible
    # signature rather than relying on bytes alone.
    if (len(names), triangles, materials, [round(v, 4) for v in size]) == \
            (len(alt_names), alt_triangles, alt_materials, [round(v, 4) for v in alt_size]):
        raise RuntimeError(f"{kind}: alternate seed has no visible structural/palette variation")
    required = {"books": "book_", "textiles": "fold_", "crockery": "plate_",
                "tools": "paint_tin_"}[kind]
    if not any(required in name for name in names):
        raise RuntimeError(f"{kind}: missing its defining visible component {required}")
    print(f"fill_kit_prepare: {kind} seed {seed}/{seed + 1000}, "
          f"{len(names)}/{len(alt_names)} pieces, {triangles}/{alt_triangles} triangles, "
          f"{[round(v, 3) for v in size]} m, {digest(first)[:16]} deterministic")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", required=True)
    parser.parse_args()
    try:
        rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
        with tempfile.TemporaryDirectory(prefix="house00973-fill-", dir="/tmp") as scratch:
            for kind, (asset_id, _filename, _seed, _bounds) in VARIANTS.items():
                check_kind(kind, rows[asset_id], Path(scratch))
    except (FileNotFoundError, KeyError, RuntimeError, ValueError) as error:
        print(f"fill_kit_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
