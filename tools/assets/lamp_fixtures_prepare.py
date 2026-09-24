#!/usr/bin/env python3
"""Prepare and validate HOUSE-00981's reuse-first interior lamp group."""

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
OUTPUT = REPO / "assets-src" / "Models" / "Furniture" / "Lamps"

REUSED = {
    "MODEL_FURNITURE_FLOOR_LAMP",
    "MODEL_FAMILY_FLOOR_LAMP_LIT",
    "MODEL_FIXTURE_DINING_SIDE_LAMP",
    "MODEL_FIXTURE_FAMILY_CEILING",
    "MODEL_FIXTURE_KITCHEN_PENDANT",
    "MODEL_FIXTURE_KITCHEN_TASK_PUCK",
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
        "MODEL_FIXTURE_DESK_LAMP", "house-00981-desk-lamp.glb", "desk_lamp.glb",
        "https://cdn.3dassets.dev/assets/38798/v1/model.glb",
        "cb3870cb3d46718a31c267a14eb8f670caaa346c457572aa982342dbd9889c21",
        "DeskLamp_COL", "desk-lamp", (0.200026, 0.598535, 0.346816), 736,
        {"black": "MAT_FURNITURE_PIANO_EBONITE",
         "metal": "MAT_KITCHEN_HARDWARE_STEEL",
         "sage": "MAT_DOOR_PAINTED",
         "linen": "MAT_FIXTURE_EMISSIVE_WARM"},
        ("B1_HOBBY", "L0_OFFICE", "L1_BED2", "L1_BED3", "L1_BED4", "L1_BED5",
         "L1_LANDING", "L1_MASTER_BED", "L2_BED6", "L2_BED7", "L2_LIBRARY",
         "L3_ROOM"),
    ),
    Source(
        "MODEL_FIXTURE_UTILITY_CEILING", "house-00981-utility-light.glb",
        "utility_ceiling_light.glb",
        "https://cdn.3dassets.dev/assets/34238/v1/model.glb",
        "602452de98c0ae8087936b47549d271fc8b446da029bd01bf34261f6bf46fa63",
        "UtilityLight_COL", "ceiling-light", (2.4, 0.341994, 0.259993), 208,
        {"steel": "MAT_APPLIANCE_STEEL",
         "paint": "MAT_DOOR_PAINTED",
         "charcoal": "MAT_FURNITURE_PIANO_EBONITE",
         "cream": "MAT_FIXTURE_EMISSIVE_NEUTRAL"},
        ("B1_CELLAR", "B1_ELECTRICAL", "B1_GYM", "B1_HALL", "B1_HOBBY",
         "B1_LAUNDRY2", "B1_MECHANICAL", "B1_STAIR", "B1_STOR1", "B1_STOR2",
         "B1_UNDERSTAIR", "B1_UTILITY", "B1_WORKSHOP", "EXT_SHED", "L0_CLOSET_W",
         "L0_GARAGE", "L0_GARAGE_LOFT", "L0_LAUNDRY", "L0_MUDROOM", "L0_PANTRY",
         "L0_STOR",
         "L1_CLOSET_2", "L1_CLOSET_3", "L1_LINEN", "L1_MASTER_CLOSET", "L1_STOR",
         "L2_CLOSET_4", "L2_LINEN2", "L2_STAIR_ATTIC", "L2_STOR2", "L3_STAIR_HEAD",
         "L3_STORE_E", "L3_STORE_N", "L3_STORE_S", "L3_STORE_W"),
    ),
)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def obtain(source: Source, cache: Path, download: bool) -> Path:
    path = cache / source.cache_name
    if not path.is_file() and download:
        request = urllib.request.Request(source.url, headers={"User-Agent": "cna-house/HOUSE-00981"})
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
        raise RuntimeError("Blender unavailable; cannot prepare lamps")
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
        raise RuntimeError(f"{source.identifier}: static fixture contains animation or skinning")
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
                print(f"lamp_fixtures_prepare: cached {source.identifier} {digest(path)}")
            return 0
        if options.write:
            for source in SOURCES:
                source_path = obtain(source, options.cache, False)
                with tempfile.TemporaryDirectory(prefix="house00981-lamps-", dir="/tmp") as scratch:
                    generated = Path(scratch) / source.output_name
                    generate(source, source_path, generated)
                    target = OUTPUT / source.output_name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(generated.read_bytes())
                    print(f"lamp_fixtures_prepare: wrote {target.relative_to(REPO)} {digest(target)}")
            return 0
        rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
        if len(SOURCES) > 4:
            raise RuntimeError("HOUSE-00981's acquired lamp cap is four models")
        missing_reuse = REUSED - rows.keys()
        if missing_reuse:
            raise RuntimeError(f"reuse-first catalogue lost {sorted(missing_reuse)}")
        kit = KIT.read_text()
        if "| Lamps | **2 / 4**" not in kit or \
                "desk lamp, bare utility fitting" not in kit:
            raise RuntimeError("the authoritative kit no longer requests only two lamp acquisitions")
        expected_files = {source.output_name for source in SOURCES} | {"SOURCE.md"}
        actual_files = {path.name for path in OUTPUT.iterdir() if path.is_file()}
        if actual_files != expected_files:
            raise RuntimeError(
                f"lamp output set changed: expected {sorted(expected_files)}, "
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
        print(f"lamp_fixtures_prepare: six existing fixtures reused before two acquisitions "
              f"within cap four; {total} visible and 24 collision triangles pass")
    except (KeyError, OSError, RuntimeError, subprocess.SubprocessError,
            urllib.error.URLError, ValueError) as error:
        print(f"lamp_fixtures_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
