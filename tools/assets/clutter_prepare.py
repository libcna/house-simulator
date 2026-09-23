#!/usr/bin/env python3
"""Prepare and validate HOUSE-00984's capped reuse-first clutter group."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
import urllib.error
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
KIT = REPO / "docs" / "furnishing-kit.md"
OUTPUT = REPO / "assets-src" / "Models" / "Furniture" / "Clutter"

REUSED = {
    "MODEL_PROP_KIT_STORAGE_BOX",
    "MODEL_FILL_BOOKS",
    "MODEL_FILL_CROCKERY",
    "MODEL_FILL_TEXTILES",
    "MODEL_FILL_TOOLS",
}


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


SOURCES = (
    Source(
        "MODEL_CLUTTER_BIN", "house-00984-bin.glb", "bin.glb",
        "https://cdn.3dassets.dev/assets/26268/v1/model.glb",
        "c9d03d627722f8fa9207868582060188d90e01ec2734c32d27ba23e5d56a2cdb",
        "Bin_COL", "waste-bin", (0.32, 0.439993, 0.32), 556,
        {"dark": "MAT_FURNITURE_PIANO_EBONITE", "metal": "MAT_APPLIANCE_STEEL",
         "accent": "MAT_PAINT_MUTED_TEAL"},
        ("B1_CINEMA", "B1_WORKSHOP", "L0_LAUNDRY", "L0_MUDROOM", "L0_GARAGE",
         "EXT_SHED"),
    ),
    Source(
        "MODEL_CLUTTER_SUITCASE", "house-00984-suitcase.glb", "suitcase.glb",
        "https://cdn.3dassets.dev/assets/30311/v1/model.glb",
        "0a6cf35a040fc9b8a3ba842920a455e822de2c18124d15932bb25f52c17c7f28",
        "Suitcase_COL", "suitcase", (0.579014, 0.85, 0.307957), 1308,
        {"fabric": "MAT_FAMILY_CURTAIN_WOOL", "steel": "MAT_KITCHEN_HARDWARE_STEEL",
         "dark": "MAT_FURNITURE_PIANO_EBONITE"},
        ("L0_GARAGE_LOFT", "L1_MASTER_CLOSET", "L1_CLOSET_2", "L1_CLOSET_3",
         "L2_CLOSET_4", "L3_STORE_E", "L3_STORE_W"),
    ),
    Source(
        "MODEL_CLUTTER_TOOLBOX", "house-00984-toolbox.glb", "toolbox.glb",
        "https://cdn.3dassets.dev/assets/23944/v1/model.glb",
        "0af6f33a5508f40757ff6092467d2ec889c57710cc5a28137a320e3b794f0cba",
        "Toolbox_COL", "toolbox", (0.555, 0.481332, 0.276839), 944,
        {"dark": "MAT_FURNITURE_PIANO_EBONITE", "accent": "MAT_PAINT_DUSTY_BLUE",
         "metal": "MAT_KITCHEN_HARDWARE_STEEL"},
        ("B1_ELECTRICAL", "B1_WORKSHOP", "L0_GARAGE", "EXT_SHED"),
    ),
    Source(
        "MODEL_CLUTTER_GARDEN_TOOLS", "house-00984-garden-tools.glb",
        "garden_tools.glb", "https://cdn.3dassets.dev/assets/17752/v1/model.glb",
        "e1b3888a6ba75492647ea8b6e2fa2ef4bf1d72fc35e9f39d1739683f9f1bb229",
        "GardenTools_COL", "garden-tool-set", (0.88588, 1.580602, 0.669707), 2804,
        {"dark": "MAT_FURNITURE_PIANO_EBONITE", "metal": "MAT_OUTDOOR_FENCE_METAL",
         "surface": "MAT_OUTDOOR_GARDEN_WOOD", "accent": "MAT_PAINT_MUTED_TEAL"},
        ("L0_GARAGE", "EXT_GARDEN", "EXT_SHED"),
    ),
    Source(
        "MODEL_CLUTTER_PAINT_TIN", "house-00984-paint-tin.glb", "paint_tin.glb",
        "https://cdn.3dassets.dev/assets/23929/v1/model.glb",
        "c7269c2acd263fc4078aa5b3c98cb3728472ef12471396ff5f9546d3b84d342c",
        "PaintTin_COL", "paint-tin", (0.236896, 0.343995, 0.233293), 598,
        {"metal": "MAT_APPLIANCE_STEEL", "accent": "MAT_PAINT_SAGE"},
        ("B1_WORKSHOP", "L0_GARAGE", "EXT_SHED"),
    ),
)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def obtain(source: Source, cache: Path, download: bool) -> Path:
    path = cache / source.cache_name
    if not path.is_file() and download:
        request = urllib.request.Request(source.url, headers={"User-Agent": "cna-house/HOUSE-00984"})
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
    command = blender_env.build_command(
        SCRIPT,
        ["--source", str(source_path), "--out", str(output),
         "--collision-name", source.collision_name],
    )
    if command is None:
        raise RuntimeError("Blender unavailable; cannot prepare clutter")
    completed = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                               capture_output=True, text=True, check=False)
    if completed.returncode != 0 or \
            "utility_appliance_prepare: EXIT 0" not in completed.stdout.splitlines():
        tail = "\n".join((completed.stdout + "\n" + completed.stderr).splitlines()[-40:])
        raise RuntimeError(f"Blender preparation failed (exit {completed.returncode}):\n{tail}")


def triangle_count(document: dict, primitives: list[dict]) -> int:
    return sum(document["accessors"][primitive.get(
        "indices", primitive["attributes"]["POSITION"]
    )]["count"] // 3 for primitive in primitives)


def inspect(source: Source, path: Path, row: dict) -> None:
    document, _ = gltf_io.read_model(path)
    if document.get("animations") or document.get("skins"):
        raise RuntimeError(f"{source.identifier}: static clutter contains animation or skinning")
    nodes = [node for node in document.get("nodes", []) if "mesh" in node]
    collision = [node for node in nodes if node.get("name", "").endswith("_COL")]
    visible = [node for node in nodes if not node.get("name", "").endswith("_COL")]
    if [node.get("name") for node in collision] != [source.collision_name]:
        raise RuntimeError(f"{source.identifier}: expected only {source.collision_name}")
    visible_primitives = [primitive for node in visible
                          for primitive in document["meshes"][node["mesh"]]["primitives"]]
    collision_primitives = [primitive for node in collision
                            for primitive in document["meshes"][node["mesh"]]["primitives"]]
    if not visible_primitives or not all("TEXCOORD_0" in primitive["attributes"]
                                         for primitive in visible_primitives):
        raise RuntimeError(f"{source.identifier}: visible geometry lacks UV0")
    if triangle_count(document, visible_primitives) != source.visible_triangles or \
            triangle_count(document, collision_primitives) != 12:
        raise RuntimeError(f"{source.identifier}: triangle counts changed")
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in visible_primitives}
    if materials != set(source.material_map) or row.get("materialMap") != source.material_map:
        raise RuntimeError(f"{source.identifier}: canonical material map changed")
    geometry = row.get("geometry", {})
    if geometry.get("boundsMetres") != list(source.bounds) or \
            geometry.get("triangles", {}).get("LOD0") != source.visible_triangles or \
            geometry.get("collision") != source.collision_name:
        raise RuntimeError(f"{source.identifier}: manifest geometry differs from pinned model")
    for problems in (scale_check.check(path, source.category, geometry),
                     origin_check.check(path, source.category)):
        if problems:
            raise RuntimeError("; ".join(problems))


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
                print(f"clutter_prepare: cached {source.identifier} {digest(path)}")
            return 0
        if options.write:
            for source in SOURCES:
                source_path = obtain(source, options.cache, False)
                with tempfile.TemporaryDirectory(prefix="house00984-clutter-", dir="/tmp") as scratch:
                    generated = Path(scratch) / source.output_name
                    generate(source, source_path, generated)
                    target = OUTPUT / source.output_name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(generated.read_bytes())
                    print(f"clutter_prepare: wrote {target.relative_to(REPO)} {digest(target)}")
            return 0
        rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
        if len(SOURCES) != 5 or len(SOURCES) > 6:
            raise RuntimeError("HOUSE-00984 requires exactly five acquisitions within cap six")
        if REUSED - rows.keys():
            raise RuntimeError(f"required generated/fill reuse lost {sorted(REUSED - rows.keys())}")
        kit = KIT.read_text()
        if "| Clutter | **5 / 6**" not in kit or \
                "generated boxes replace a separate crate model" not in kit:
            raise RuntimeError("the authoritative kit no longer requests the five-item clutter set")
        expected_files = {source.output_name for source in SOURCES} | {"SOURCE.md"}
        actual_files = {path.name for path in OUTPUT.iterdir() if path.is_file()}
        if actual_files != expected_files:
            raise RuntimeError(f"clutter output set changed: expected {sorted(expected_files)}, "
                               f"found {sorted(actual_files)}")
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
        print(f"clutter_prepare: five acquired shapes within cap six; generated boxes and fill "
              f"families reused; {total} visible and 60 collision triangles pass")
    except (KeyError, OSError, RuntimeError, subprocess.SubprocessError,
            urllib.error.URLError, ValueError) as error:
        print(f"clutter_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
