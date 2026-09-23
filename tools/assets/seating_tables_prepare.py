#!/usr/bin/env python3
"""Prepare and validate HOUSE-00977's capped seating-and-tables acquisition set."""

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
OUTPUT = REPO / "assets-src" / "Models" / "Furniture" / "SeatingTables"
FURNISHING_KIT = REPO / "docs" / "furnishing-kit.md"
REUSED_SEATING = (
    "MODEL_FURNITURE_FOYER_ARMCHAIR",
    "MODEL_FURNITURE_SHEEN_CHAIR",
    "MODEL_FURNITURE_FORMAL_SOFA",
    "MODEL_FAMILY_GLAM_VELVET_SOFA",
    "MODEL_FURNITURE_DINING_CHAIR",
    "MODEL_FURNITURE_KITCHEN_COUNTER_STOOL",
    "MODEL_FURNITURE_PIANO_BENCH",
)
REUSED_TABLES = (
    "MODEL_FURNITURE_COFFEE_TABLE",
    "MODEL_FAMILY_SIDE_TABLE",
    "MODEL_FURNITURE_DINING_TABLE",
)


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
        "MODEL_FURNITURE_CINEMA_SEAT_ROW", "house-00977-cinema.glb", "cinema_seat_row.glb",
        "https://cdn.3dassets.dev/assets/34650/v1/model.glb",
        "4057aec4f6f1a13982d1f031057dd44c712ff7dda05354750b6d70a29c6b59f4",
        "CinemaSeatRow_COL", "cinema-seat-row", (3.08, 1.077667, 1.099859), 6320,
        {"accent": "MAT_FAMILY_SOFA_VELVET", "amber": "MAT_FURNITURE_PIANO_BRASS",
         "dark": "MAT_FURNITURE_PIANO_EBONITE", "frame": "MAT_FAMILY_SOFA_FRAME",
         "metal": "MAT_KITCHEN_HARDWARE_STEEL"},
        ("B1_CINEMA",),
    ),
    Source(
        "MODEL_FURNITURE_DESK", "house-00977-desk.glb", "writing_desk.glb",
        "https://cdn.3dassets.dev/assets/38795/v1/model.glb",
        "29678a25099beed66d4b225b439e0c5fe05ecced4362791d130a15eeb7386146",
        "Desk_COL", "desk", (1.4, 0.745054, 0.700107), 1168,
        {"black": "MAT_FURNITURE_PIANO_EBONITE", "carcass": "MAT_DOOR_PAINTED",
         "oak": "MAT_FURNITURE_DINING_WALNUT"},
        ("L0_OFFICE", "B1_HOBBY", "L2_LIBRARY", "L3_ROOM"),
    ),
    Source(
        "MODEL_FURNITURE_POOL_TABLE", "house-00977-pool.glb", "pool_table.glb",
        "https://cdn.3dassets.dev/assets/33854/v1/model.glb",
        "9d781d16d488d5865a5a6fb65ef970e5d9e1dfae73415b297af6c5f8f6bac4fd",
        "PoolTable_COL", "table-pool", (2.83, 0.801383, 1.559968), 3984,
        {"cloth": "MAT_FURNITURE_DINING_UPHOLSTERY",
         "cream": "MAT_FURNITURE_PIANO_IVORY",
         "dark": "MAT_FURNITURE_PIANO_EBONITE", "metal": "MAT_KITCHEN_HARDWARE_STEEL",
         "timber": "MAT_FURNITURE_DINING_WALNUT"},
        ("L2_GAMES",),
    ),
)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def obtain(source: Source, cache: Path, download: bool) -> Path:
    path = cache / source.cache_name
    if not path.is_file() and download:
        request = urllib.request.Request(source.url, headers={"User-Agent": "cna-house/HOUSE-00977"})
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
        raise RuntimeError("Blender unavailable; cannot prepare seating and tables")
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
            f"{source.identifier}: triangles visible/collision="
            f"{visible_triangles}/{collision_triangles}"
        )
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in visible_primitives}
    if materials != set(source.material_map) or row.get("materialMap") != source.material_map:
        raise RuntimeError(f"{source.identifier}: canonical material map changed")
    geometry = row.get("geometry", {})
    if geometry.get("boundsMetres") != list(source.bounds) or \
            geometry.get("triangles", {}).get("LOD0") != source.visible_triangles or \
            geometry.get("collision") != source.collision_name:
        raise RuntimeError(f"{source.identifier}: manifest geometry differs from the pinned model")
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
                print(f"seating_tables_prepare: cached {source.identifier} {digest(path)}")
            return 0
        if options.write:
            for source in SOURCES:
                source_path = obtain(source, options.cache, False)
                with tempfile.TemporaryDirectory(prefix="house00977-seating-", dir="/tmp") as scratch:
                    generated = Path(scratch) / source.output_name
                    generate(source, source_path, generated)
                    target = OUTPUT / source.output_name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(generated.read_bytes())
                    print(f"seating_tables_prepare: wrote {target.relative_to(REPO)} "
                          f"{digest(target)}")
            return 0
        rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
        if len(SOURCES) != 3:
            raise RuntimeError("HOUSE-00977 uses exactly three of the seven-model cap")
        kit = FURNISHING_KIT.read_text()
        for identifier in REUSED_SEATING + REUSED_TABLES:
            if identifier not in rows or f"`{identifier}`" not in kit:
                raise RuntimeError(f"{identifier}: required reuse-first kit entry is missing")
        expected_files = {source.output_name for source in SOURCES} | {"SOURCE.md"}
        actual_files = {path.name for path in OUTPUT.iterdir() if path.is_file()}
        if actual_files != expected_files:
            raise RuntimeError(
                f"seating/table output set changed: expected {sorted(expected_files)}, "
                f"found {sorted(actual_files)}"
            )
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
        print(f"seating_tables_prepare: ten existing models reused before three static "
              f"acquisitions within cap seven; {total} visible and 36 collision triangles pass")
    except (KeyError, OSError, RuntimeError, subprocess.SubprocessError,
            urllib.error.URLError, ValueError) as error:
        print(f"seating_tables_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
