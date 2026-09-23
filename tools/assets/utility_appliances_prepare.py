#!/usr/bin/env python3
"""Prepare and validate HOUSE-00975's capped three-piece utility appliance set."""

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
OUTPUT = REPO / "assets-src" / "Models" / "Furniture" / "Appliances"


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
    used_in: tuple[str, ...]


SOURCES = (
    Source("MODEL_APPLIANCE_LAUNDRY_BODY", "house-00975-washer.glb", "laundry_body.glb",
           "https://cdn.3dassets.dev/assets/14050/v1/model.glb",
           "937e52a6c4991697209e1a860034ba535098986847ea3f2b8e27bada62189f02",
           "LaundryBody_COL", "appliance-laundry", (0.634919, 0.8512, 0.722977), 2654,
           ("L0_LAUNDRY", "B1_LAUNDRY2")),
    Source("MODEL_APPLIANCE_CHEST_FREEZER", "house-00975-freezer.glb", "chest_freezer.glb",
           "https://cdn.3dassets.dev/assets/14043/v1/model.glb",
           "f2f885ab83db79982362acdf4a3a95f7f8f0a2cd37d48d2e66cd8678f9dac2f4",
           "ChestFreezer_COL", "appliance-chest-freezer", (1.12, 0.882051, 0.831407), 1656,
           ("B1_UTILITY",)),
    Source("MODEL_APPLIANCE_BOILER", "house-00975-boiler.glb", "boiler.glb",
           "https://cdn.3dassets.dev/assets/14137/v1/model.glb",
           "cd1a6132cb47560b2b4e92083b6707c33440d44ecd43cc57ad8397876820efc6",
           "Boiler_COL", "appliance-boiler", (0.445984, 1.1173, 0.373427), 1944,
           ("B1_UTILITY",)),
)

MATERIAL_MAP = {
    "accent": "MAT_FURNITURE_PIANO_BRASS",
    "carcass": "MAT_WINDOW_FRAME_WHITE",
    "dark": "MAT_KITCHEN_OVEN_GLASS",
    "glass": "MAT_GLASS_CABINET",
    "metal": "MAT_KITCHEN_HARDWARE_STEEL",
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def obtain(source: Source, cache: Path, download: bool) -> Path:
    path = cache / source.cache_name
    if not path.is_file() and download:
        request = urllib.request.Request(source.url, headers={"User-Agent": "cna-house/HOUSE-00975"})
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
        raise RuntimeError("Blender unavailable; cannot prepare utility appliances")
    completed = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                               capture_output=True, text=True, check=False)
    if completed.returncode != 0 or \
            "utility_appliance_prepare: EXIT 0" not in completed.stdout.splitlines():
        tail = "\n".join((completed.stdout + "\n" + completed.stderr).splitlines()[-40:])
        raise RuntimeError(f"Blender preparation failed (exit {completed.returncode}):\n{tail}")


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
    visible_triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                            for primitive in visible_primitives)
    collision_triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                              for primitive in collision_primitives)
    if visible_triangles != source.visible_triangles or collision_triangles != 12:
        raise RuntimeError(
            f"{source.identifier}: triangles visible/collision={visible_triangles}/{collision_triangles}")
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in visible_primitives}
    if materials != set(MATERIAL_MAP) or row.get("materialMap") != MATERIAL_MAP:
        raise RuntimeError(f"{source.identifier}: canonical five-role material map changed")
    if row.get("geometry", {}).get("boundsMetres") != list(source.bounds) or \
            row["geometry"].get("triangles", {}).get("LOD0") != source.visible_triangles or \
            row["geometry"].get("collision") != source.collision_name:
        raise RuntimeError(f"{source.identifier}: manifest geometry differs from the pinned model")
    for problem in (scale_check.check(path, source.category, row["geometry"]),
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
                print(f"utility_appliances_prepare: cached {source.identifier} {digest(path)}")
            return 0

        if options.write:
            for source in SOURCES:
                source_path = obtain(source, options.cache, False)
                with tempfile.TemporaryDirectory(prefix="house00975-appliance-", dir="/tmp") as scratch:
                    generated = Path(scratch) / source.output_name
                    generate(source, source_path, generated)
                    target = OUTPUT / source.output_name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(generated.read_bytes())
                    print(f"utility_appliances_prepare: wrote {target.relative_to(REPO)} "
                          f"{digest(target)}")
            return 0

        rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
        if len(SOURCES) != 3:
            raise RuntimeError("HOUSE-00975 acquisition cap is exactly three appliance models")
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
        print("utility_appliances_prepare: three closed static appliances, 6254 visible and "
              "36 collision triangles pass")
    except (KeyError, OSError, RuntimeError, subprocess.SubprocessError,
            urllib.error.URLError, ValueError) as error:
        print(f"utility_appliances_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
