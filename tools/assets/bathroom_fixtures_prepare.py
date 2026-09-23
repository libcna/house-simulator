#!/usr/bin/env python3
"""Prepare and validate HOUSE-00976's capped four-piece bathroom fixture set."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
import urllib.request
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))
import blender_env  # noqa: E402
sys.path.insert(0, str(REPO / "tools" / "assets"))
import gltf_io  # noqa: E402
import origin_check  # noqa: E402
import scale_check  # noqa: E402

SCRIPT = REPO / "tools" / "blender" / "utility_appliance_prepare.py"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
OUTPUT = REPO / "assets-src" / "Models" / "Furniture" / "Bathroom"
BATHROOMS = ("L1_MASTER_BATH", "L1_BATH2", "L1_BATH3", "L2_BATH4", "L2_BATH5")
WCS = ("B1_WC7", "L0_WC1", "L0_WC2", "L1_WC3", "L1_WC4", "L2_WC5", "L2_WC6")


@dataclass(frozen=True)
class Source:
    identifier: str
    cache_name: str
    output_name: str
    url: str
    source_sha256: str
    collision_name: str
    category: str
    bounds: tuple[float, float, float]
    visible_triangles: int
    material_map: dict[str, str]
    used_in: tuple[str, ...]
    seat_height: float | None = None


SOURCES = (
    Source("MODEL_BATHROOM_TOILET", "house-00976-toilet.glb", "toilet.glb",
           "https://cdn.3dassets.dev/assets/17870/v1/model.glb",
           "175f19f79e3f9f97041c571cd1adf462fa8f43f45bea4cbcc4f043db11422c89",
           "Toilet_COL", "wc", (0.43, 0.824012, 0.61499), 1920,
           {"ceramic": "MAT_FURNITURE_DINING_PORCELAIN",
            "metal": "MAT_KITCHEN_HARDWARE_STEEL"}, BATHROOMS + WCS, 0.4),
    Source("MODEL_BATHROOM_VANITY", "house-00976-vanity.glb", "vanity_basin.glb",
           "https://cdn.3dassets.dev/assets/17883/v1/model.glb",
           "b3c7e9ce121ff86b52e6d27389a085607c9cc646b6eaf76b09d833e4bef7b263",
           "VanityBasin_COL", "basin", (0.62, 0.827966, 0.530489), 1516,
           {"carcass": "MAT_DOOR_PAINTED", "ceramic": "MAT_FURNITURE_DINING_PORCELAIN",
            "dark": "MAT_KITCHEN_OVEN_GLASS", "front": "MAT_WINDOW_BLIND_BLUE_MUTED",
            "metal": "MAT_KITCHEN_HARDWARE_STEEL"}, BATHROOMS + WCS),
    Source("MODEL_BATHROOM_BATH", "house-00976-bath.glb", "bath.glb",
           "https://cdn.3dassets.dev/assets/17865/v1/model.glb",
           "a484dfdeb90903177f2d93d5c8c8aa4b901d7e42a7fad60f874d36b83e566a6d",
           "Bath_COL", "bath", (1.7, 0.549995, 0.700037), 680,
           {"ceramic": "MAT_FURNITURE_DINING_PORCELAIN",
            "metal": "MAT_KITCHEN_HARDWARE_STEEL"}, BATHROOMS),
    Source("MODEL_BATHROOM_SHOWER", "house-00976-shower.glb", "shower_enclosure.glb",
           "https://cdn.3dassets.dev/assets/17894/v1/model.glb",
           "87042eed1c27b5a5ef5bb848052e9b225502b237e88c73acbe6827a1af72db1e",
           "ShowerEnclosure_COL", "shower-enclosure", (0.9, 1.961407, 0.9), 940,
           {"ceramic": "MAT_FURNITURE_DINING_PORCELAIN", "glass": "MAT_GLASS_CLEAR",
            "metal": "MAT_KITCHEN_HARDWARE_STEEL"}, BATHROOMS),
)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def obtain(source: Source, cache: Path, download: bool) -> Path:
    path = cache / source.cache_name
    if not path.is_file() and download:
        request = urllib.request.Request(source.url, headers={"User-Agent": "cna-house/HOUSE-00976"})
        with urllib.request.urlopen(request, timeout=30) as response:
            data = response.read()
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    if not path.is_file():
        raise RuntimeError(f"{path}: missing pinned source; use --download")
    actual = digest(path)
    if actual != source.source_sha256:
        raise RuntimeError(f"{path}: source SHA-256 {actual} differs from pin")
    return path


def generate(source: Source, source_path: Path, output: Path) -> None:
    command = blender_env.build_command(SCRIPT, ["--source", str(source_path), "--out",
                                                 str(output), "--collision-name",
                                                 source.collision_name])
    if command is None:
        raise RuntimeError("Blender unavailable; cannot prepare bathroom fixtures")
    completed = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                               capture_output=True, text=True, check=False)
    if completed.returncode != 0 or \
            "utility_appliance_prepare: EXIT 0" not in completed.stdout.splitlines():
        tail = "\n".join((completed.stdout + "\n" + completed.stderr).splitlines()[-40:])
        raise RuntimeError(f"Blender preparation failed (exit {completed.returncode}):\n{tail}")


def triangle_count(document: dict, primitives: list[dict]) -> int:
    total = 0
    for primitive in primitives:
        accessor = primitive.get("indices", primitive["attributes"]["POSITION"])
        total += document["accessors"][accessor]["count"] // 3
    return total


def inspect(source: Source, path: Path, row: dict) -> None:
    document, _ = gltf_io.read_model(path)
    if document.get("animations") or document.get("skins"):
        raise RuntimeError(f"{source.identifier}: static dressing contains animation or skinning")
    mesh_nodes = [node for node in document.get("nodes", []) if "mesh" in node]
    collision = [node for node in mesh_nodes if node.get("name", "").endswith("_COL")]
    visible = [node for node in mesh_nodes if not node.get("name", "").endswith("_COL")]
    if [node.get("name") for node in collision] != [source.collision_name]:
        raise RuntimeError(f"{source.identifier}: expected only {source.collision_name}")
    visible_primitives = [primitive for node in visible
                          for primitive in document["meshes"][node["mesh"]]["primitives"]]
    collision_primitives = [primitive for node in collision
                            for primitive in document["meshes"][node["mesh"]]["primitives"]]
    if not visible_primitives or not all("TEXCOORD_0" in primitive["attributes"]
                                         for primitive in visible_primitives):
        raise RuntimeError(f"{source.identifier}: visible geometry lacks UV0")
    visible_triangles = triangle_count(document, visible_primitives)
    collision_triangles = triangle_count(document, collision_primitives)
    if visible_triangles != source.visible_triangles or collision_triangles != 12:
        raise RuntimeError(
            f"{source.identifier}: triangles visible/collision={visible_triangles}/{collision_triangles}")
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in visible_primitives}
    if materials != set(source.material_map) or row.get("materialMap") != source.material_map:
        raise RuntimeError(f"{source.identifier}: canonical material map changed")
    geometry = row.get("geometry", {})
    if geometry.get("boundsMetres") != list(source.bounds) or \
            geometry.get("triangles", {}).get("LOD0") != source.visible_triangles or \
            geometry.get("collision") != source.collision_name or \
            geometry.get("seatHeightMetres") != source.seat_height:
        raise RuntimeError(f"{source.identifier}: manifest geometry differs from the pinned model")
    for problem in (scale_check.check(path, source.category, geometry),
                    origin_check.check(path, source.category)):
        if problem:
            raise RuntimeError("; ".join(problem))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--download", action="store_true")
    mode.add_argument("--check", action="store_true")
    parser.add_argument("--cache", type=Path)
    options = parser.parse_args()
    if (options.write or options.download) and options.cache is None:
        parser.error("--write and --download require --cache")
    try:
        if options.download:
            for source in SOURCES:
                path = obtain(source, options.cache, True)
                print(f"bathroom_fixtures_prepare: cached {source.identifier} {digest(path)}")
            return 0
        if options.write:
            for source in SOURCES:
                source_path = obtain(source, options.cache, False)
                with tempfile.TemporaryDirectory(prefix="house00976-bathroom-", dir="/tmp") as scratch:
                    generated = Path(scratch) / source.output_name
                    generate(source, source_path, generated)
                    target = OUTPUT / source.output_name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(generated.read_bytes())
                    print(f"bathroom_fixtures_prepare: wrote {target.relative_to(REPO)} "
                          f"{digest(target)}")
            return 0
        rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
        if len(SOURCES) != 4:
            raise RuntimeError("HOUSE-00976 acquisition cap is exactly four bathroom models")
        for source in SOURCES:
            row = rows[source.identifier]
            path = OUTPUT / source.output_name
            if digest(path) != row.get("sourceSha256"):
                raise RuntimeError(f"{source.identifier}: committed SHA-256 differs from manifest")
            origin = row.get("origin", {})
            if origin.get("url") != source.url or source.source_sha256 not in origin.get("note", ""):
                raise RuntimeError(f"{source.identifier}: pinned upstream provenance changed")
            if row.get("usedIn") != list(source.used_in) or row.get("category") != source.category:
                raise RuntimeError(f"{source.identifier}: recipe use or category changed")
            inspect(source, path, row)
        total = sum(source.visible_triangles for source in SOURCES)
        print(f"bathroom_fixtures_prepare: four closed static fixtures, {total} visible and "
              "48 collision triangles pass")
    except (KeyError, OSError, RuntimeError, subprocess.SubprocessError,
            urllib.error.URLError, ValueError) as error:
        print(f"bathroom_fixtures_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
