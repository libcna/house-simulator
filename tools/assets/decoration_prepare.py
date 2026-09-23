#!/usr/bin/env python3
"""Prepare and validate HOUSE-00982's reuse-first decoration and wall-art set."""

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

from PIL import Image, ImageOps

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))
import blender_env  # noqa: E402
sys.path.insert(0, str(REPO / "tools" / "assets"))
import gltf_io  # noqa: E402
import origin_check  # noqa: E402
import scale_check  # noqa: E402
sys.path.insert(0, str(REPO / "tools" / "world"))
import layout_io  # noqa: E402

STATIC_SCRIPT = REPO / "tools" / "blender" / "utility_appliance_prepare.py"
ART_SCRIPT = REPO / "tools" / "blender" / "wall_art_set.py"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
KIT = REPO / "docs" / "furnishing-kit.md"
MODEL_OUTPUT = REPO / "assets-src" / "Models" / "Furniture" / "Decoration"
TEXTURE_OUTPUT = REPO / "assets-src" / "Textures" / "Furniture" / "Decoration"
RUNTIME_TEXTURE_PIXELS = (512, 512)


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
    origin_mode: str = "support"
    strip_textures: bool = False


@dataclass(frozen=True)
class Art:
    number: str
    source_sha256: str
    crop: str
    pixels: tuple[int, int]
    frame_metres: tuple[float, float]
    style: str
    bounds: tuple[float, float, float]
    model_sha256: str
    texture_sha256: str
    used_in: tuple[str, ...]

    @property
    def url(self) -> str:
        return f"https://opengameart.org/sites/default/files/abstract_{self.number}.jpg"

    @property
    def cache_name(self) -> str:
        return f"house-00982-art-{self.number}.jpg"

    @property
    def texture_name(self) -> str:
        return f"abstract_{self.number}.png"

    @property
    def model_name(self) -> str:
        return f"wall_art_{self.number}.glb"

    @property
    def model_id(self) -> str:
        return f"MODEL_DECOR_WALL_ART_{self.number}"

    @property
    def texture_id(self) -> str:
        return f"TEXTURE_DECOR_WALL_ART_{self.number}"

    @property
    def material_id(self) -> str:
        return f"MAT_DECOR_WALL_ART_{self.number}"


SOURCES = (
    Source(
        "MODEL_DECOR_FLOOR_MIRROR", "house-00982-mirror.glb", "floor_mirror.glb",
        "https://cdn.3dassets.dev/assets/38808/v1/model.glb",
        "3a8d064c228cde84ff16e17c86bdd7352a1d596fc7ad658ea979017b649ce2a9",
        "Mirror_COL", "floor-mirror", (0.82, 1.792, 0.43992), 1104,
        {"walnut": "MAT_FURNITURE_PIANO_WOOD", "metal": "MAT_KITCHEN_HARDWARE_STEEL",
         "glass": "MAT_GLASS_CLEAR"},
        ("L1_MASTER_CLOSET", "L1_BED2", "L2_BED5", "L2_BED6"),
    ),
    Source(
        "MODEL_DECOR_WALL_CLOCK", "house-00982-clock.glb", "wall_clock.glb",
        "https://cdn.3dassets.dev/assets/25552/v1/model.glb",
        "2ba3406ef891bf9ea941e52f4c9dd2295301c0ea167ef4012de43f191a9549da",
        "WallClock_COL", "wall-clock", (0.34, 0.34, 0.122999), 988,
        {"dark": "MAT_FIXTURE_DARK_BRONZE", "carcass": "MAT_DOOR_PAINTED",
         "accent": "MAT_PAINT_PALE_ROSE", "metal": "MAT_KITCHEN_HARDWARE_STEEL",
         "glass": "MAT_GLASS_CLEAR"},
        ("B1_WORKSHOP", "L0_LAUNDRY", "L1_HALL", "L2_HALL"), "wall",
    ),
    Source(
        "MODEL_DECOR_PLANT_GROUP", "house-00982-plant.glb", "plant_group.glb",
        "https://cdn.3dassets.dev/assets/36579/v1/model.glb",
        "2336e6733dc6d3c825da78a04b4bd6122e823e66075158af4c65a6b1448a58fa",
        "PlantGroup_COL", "houseplant", (1.0782, 0.821747, 0.974054), 1326,
        {"terracotta": "MAT_FOYER_CERAMIC", "timber": "MAT_FURNITURE_PIANO_WOOD",
         "leaf": "MAT_FURNITURE_PLANT_LEAF", "teal": "MAT_PAINT_MUTED_TEAL"},
        ("B1_GYM", "L1_SITTING", "L2_LIBRARY", "L3_ROOM"), "support", True,
    ),
    Source(
        "MODEL_DECOR_NARROW_VASE", "house-00982-vase.glb", "narrow_vase.glb",
        "https://cdn.3dassets.dev/assets/29517/v1/model.glb",
        "99d4497e6995aa92c5293c7d437a12c190edf0b7489c965d8df9aae86e653239",
        "Vase_COL", "vase", (0.29998, 0.48, 0.29998), 1792,
        {"sage": "MAT_FOYER_CERAMIC"},
        ("B1_WINE", "L1_SITTING", "L2_LIBRARY", "L3_ROOM"),
    ),
)

# Output pins are filled from the deterministic preparation path. Source pins above are the bytes
# retrieved from the publishers, so upstream changes cannot silently alter committed content.
ART = (
    Art("13", "3019297b5ea067c205f4879111ad1c2b929bbafe26aa86067e81ddc1b803c7f2",
        "landscape", (512, 341), (0.84, 0.56), "dark", (0.994, 0.714, 0.05),
        "b432f197d4e1951392c30541dbdcaafa569b89f0c5d215b2ab9b2e53a14a7148",
        "b47d5679f0759b02a24cf8c492d41744c64077a77c493e7ecc555e1c01f5893c",
        ("L1_SITTING", "L2_LIBRARY")),
    Art("14", "2c985e7a036016c5774a2568f57093afe7a36c3947a7e605f134de182633c59c",
        "landscape", (512, 340), (0.95, 0.63), "brass", (1.084, 0.764, 0.05),
        "2b28cfbbfcd615d5845a51093003c0dcf02d3b9c57bc5af476c751600f612623",
        "62787bb46c4845dc5e8cba353d99148c76042c29483bbb43ab08bc24cb523b48",
        ("L1_MASTER_BED", "L2_BED5")),
    Art("15", "130869082f07dd2a4b5dc1e147ce92293c31f231c8ab111fe751caee74a64ce1",
        "portrait", (360, 512), (0.48, 0.68), "matted", (0.694, 0.894, 0.05),
        "71f71f8f38a417b4c9af595f19f6f5d2265b707b826060c11e0d9661b7fecbe7",
        "05f04ee31942ae18359e7229759280d127cc32296e2836c7d32f8f5a925db929",
        ("B1_CINEMA", "L1_HALL", "L1_HALL_W", "L2_HALL", "L2_HALL_W")),
    Art("16", "b28967e69522ad9daf6c95c4e9c40000a83eda32fa9fd4a58d8657e4cf82c002",
        "square", (512, 512), (0.62, 0.62), "dark", (0.774, 0.774, 0.05),
        "f811da0b993c67242155da7c9d277253a7a9610fe8ab14e8ce4053c6c8b16d96",
        "2831bff24b2292861d4933d9fcb59e3c71350bf2f8e4e01f5d47fbdc22fb4889",
        ("B1_HOBBY", "L3_ROOM")),
)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def obtain(url: str, expected: str, path: Path, download: bool) -> Path:
    if not path.is_file() and download:
        request = urllib.request.Request(url, headers={"User-Agent": "cna-house/HOUSE-00982"})
        with urllib.request.urlopen(request, timeout=30) as response:
            data = response.read()
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    if not path.is_file():
        raise RuntimeError(f"{path}: missing pinned source; use --download")
    actual = digest(path)
    if actual != expected:
        raise RuntimeError(f"{path}: source SHA-256 {actual} differs from pin")
    return path


def run_blender(script: Path, arguments: list[str], sentinel: str) -> None:
    command = blender_env.build_command(script, arguments)
    if command is None:
        raise RuntimeError("Blender unavailable; cannot prepare decorations")
    completed = subprocess.run(command, cwd=REPO, env=blender_env.environment(),
                               capture_output=True, text=True, check=False)
    if completed.returncode != 0 or sentinel not in completed.stdout.splitlines():
        tail = "\n".join((completed.stdout + "\n" + completed.stderr).splitlines()[-40:])
        raise RuntimeError(f"Blender preparation failed (exit {completed.returncode}):\n{tail}")


def prepare_source(source: Source, source_path: Path, output: Path) -> None:
    arguments = ["--source", str(source_path), "--out", str(output),
                 "--collision-name", source.collision_name, "--origin-mode", source.origin_mode]
    if source.strip_textures:
        arguments.append("--strip-textures")
    run_blender(STATIC_SCRIPT, arguments,
                "utility_appliance_prepare: EXIT 0")


def prepare_texture(art: Art, source: Path, output: Path) -> None:
    with Image.open(source) as opened:
        image = opened.convert("RGB")
        if art.crop == "portrait":
            image = ImageOps.fit(image, art.pixels, method=Image.Resampling.LANCZOS,
                                 centering=(0.5, 0.5))
        elif art.crop == "square":
            image = ImageOps.fit(image, art.pixels, method=Image.Resampling.LANCZOS,
                                 centering=(0.5, 0.5))
        else:
            image.thumbnail(art.pixels, Image.Resampling.LANCZOS)
            if image.size != art.pixels:
                image = image.resize(art.pixels, Image.Resampling.LANCZOS)
        # The shared stock-XNA material path intentionally uses the same wrapping sampler as the
        # house's tiled materials. Reach therefore requires both texture axes to be power-of-two.
        # Encode the already-cropped aspect into a square texture; the matching physical picture
        # plane restores that aspect when it maps the full 0..1 UV range.
        if image.size != RUNTIME_TEXTURE_PIXELS:
            image = image.resize(RUNTIME_TEXTURE_PIXELS, Image.Resampling.LANCZOS)
        output.parent.mkdir(parents=True, exist_ok=True)
        image.save(output, format="PNG", compress_level=9)


def prepare_art_model(art: Art, image: Path, output: Path) -> None:
    run_blender(ART_SCRIPT,
                ["--width", str(art.frame_metres[0]), "--height", str(art.frame_metres[1]),
                 "--style", art.style, "--image", str(image),
                 "--image-material", f"ART_IMAGE_{art.number}", "--out", str(output)],
                "wall_art_set: EXIT 0")
    # Blender declares trilinear minification even though this bounded source GLB embeds only one
    # image level. CNA correctly warns about that mismatch. The canonical runtime texture is built
    # separately, so keep the self-contained review GLB honest and request ordinary linear sampling.
    document, blob = gltf_io.read_model(output)
    for sampler in document.get("samplers", []):
        sampler["magFilter"] = 9729
        sampler["minFilter"] = 9729
    gltf_io.write_glb(output, document, blob)


def triangle_count(document: dict, primitives: list[dict]) -> int:
    return sum(document["accessors"][primitive.get(
        "indices", primitive["attributes"]["POSITION"]
    )]["count"] // 3 for primitive in primitives)


def inspect_model(path: Path, identifier: str, category: str, bounds: tuple[float, float, float],
                  triangles: int, collision_name: str, material_map: dict[str, str], row: dict) -> None:
    document, _ = gltf_io.read_model(path)
    if document.get("animations") or document.get("skins"):
        raise RuntimeError(f"{identifier}: static decoration contains animation or skinning")
    nodes = [node for node in document.get("nodes", []) if "mesh" in node]
    collision = [node for node in nodes if node.get("name", "").endswith("_COL")]
    visible = [node for node in nodes if not node.get("name", "").endswith("_COL")]
    if [node.get("name") for node in collision] != [collision_name]:
        raise RuntimeError(f"{identifier}: expected only {collision_name}")
    visible_primitives = [primitive for node in visible
                          for primitive in document["meshes"][node["mesh"]]["primitives"]]
    collision_primitives = [primitive for node in collision
                            for primitive in document["meshes"][node["mesh"]]["primitives"]]
    if not visible_primitives or not all("TEXCOORD_0" in primitive["attributes"]
                                         for primitive in visible_primitives):
        raise RuntimeError(f"{identifier}: visible geometry lacks UV0")
    actual_triangles = triangle_count(document, visible_primitives)
    if actual_triangles != triangles or triangle_count(document, collision_primitives) != 12:
        raise RuntimeError(f"{identifier}: triangle counts changed")
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in visible_primitives}
    if materials != set(material_map) or row.get("materialMap") != material_map:
        raise RuntimeError(f"{identifier}: canonical material map changed: {sorted(materials)}")
    geometry = row.get("geometry", {})
    if geometry.get("boundsMetres") != list(bounds) or \
            geometry.get("triangles", {}).get("LOD0") != triangles or \
            geometry.get("collision") != collision_name:
        raise RuntimeError(f"{identifier}: manifest geometry differs from the pinned model")
    for problems in (scale_check.check(path, category, geometry), origin_check.check(path, category)):
        if problems:
            raise RuntimeError("; ".join(problems))


def validate_rug_reuse(rows: dict[str, dict]) -> None:
    required = {"MODEL_FOYER_ENTRY_RUG", "MODEL_HALL_RUNNER"}
    if not required <= rows.keys():
        raise RuntimeError("generated rug family is missing")
    materials = layout_io.load_file(
        REPO / "assets-src" / "world" / "layout.materials.json", "materials")["materials"]
    by_id = layout_io.by_id(materials, "material")
    for material_id in ("MAT_LIVING_RUG_WOOL", "MAT_HALL_RUNNER_WOOL",
                        "MAT_HALL_RUNNER_BORDER_WOOL"):
        if by_id[material_id].get("albedo") != "Textures/Materials/fabric_weave_albedo":
            raise RuntimeError(f"{material_id}: generated rug lost the CC0 Fabric061 weave")
    texture = rows["TEXTURE_MATERIAL_FABRIC_WEAVE_ALBEDO"]
    if texture["origin"].get("licence") != "CC0-1.0":
        raise RuntimeError("generated rug weave is no longer recorded as CC0")


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
                path = obtain(source.url, source.source_sha256,
                              options.cache / source.cache_name, True)
                print(f"decoration_prepare: cached {source.identifier} {digest(path)}")
            for art in ART:
                path = obtain(art.url, art.source_sha256,
                              options.cache / art.cache_name, True)
                print(f"decoration_prepare: cached art {art.number} {digest(path)}")
            return 0
        if options.write:
            MODEL_OUTPUT.mkdir(parents=True, exist_ok=True)
            TEXTURE_OUTPUT.mkdir(parents=True, exist_ok=True)
            for source in SOURCES:
                source_path = obtain(source.url, source.source_sha256,
                                     options.cache / source.cache_name, False)
                with tempfile.TemporaryDirectory(prefix="house00982-decoration-", dir="/tmp") as tmp:
                    generated = Path(tmp) / source.output_name
                    prepare_source(source, source_path, generated)
                    target = MODEL_OUTPUT / source.output_name
                    target.write_bytes(generated.read_bytes())
                    print(f"decoration_prepare: wrote {target.relative_to(REPO)} {digest(target)}")
            for art in ART:
                source_path = obtain(art.url, art.source_sha256,
                                     options.cache / art.cache_name, False)
                prepare_texture(art, source_path, TEXTURE_OUTPUT / art.texture_name)
                prepare_art_model(art, TEXTURE_OUTPUT / art.texture_name,
                                  MODEL_OUTPUT / art.model_name)
                print(f"decoration_prepare: wrote art {art.number} model="
                      f"{digest(MODEL_OUTPUT / art.model_name)} texture="
                      f"{digest(TEXTURE_OUTPUT / art.texture_name)}")
            return 0

        rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
        if len(SOURCES) > 6 or len(ART) > 8:
            raise RuntimeError("HOUSE-00982 acquisition or art-set cap exceeded")
        if "| Decoration | **4 / 6 + art**" not in KIT.read_text():
            raise RuntimeError("authoritative kit no longer requests four decoration acquisitions")
        expected_files = {source.output_name for source in SOURCES} | \
            {art.model_name for art in ART} | {"SOURCE.md"}
        actual_files = {path.name for path in MODEL_OUTPUT.iterdir() if path.is_file()}
        if actual_files != expected_files:
            raise RuntimeError(f"decoration model set changed: {sorted(actual_files)}")
        expected_textures = {art.texture_name for art in ART} | {"SOURCE.md"}
        actual_textures = {path.name for path in TEXTURE_OUTPUT.iterdir() if path.is_file()}
        if actual_textures != expected_textures:
            raise RuntimeError(f"wall-art texture set changed: {sorted(actual_textures)}")

        for source in SOURCES:
            row = rows[source.identifier]
            path = MODEL_OUTPUT / source.output_name
            if digest(path) != row.get("sourceSha256"):
                raise RuntimeError(f"{source.identifier}: committed hash differs from manifest")
            origin = row.get("origin", {})
            if origin.get("url") != source.url or source.source_sha256 not in origin.get("note", ""):
                raise RuntimeError(f"{source.identifier}: pinned upstream provenance changed")
            if row.get("usedIn") != list(source.used_in) or row.get("category") != source.category:
                raise RuntimeError(f"{source.identifier}: recipe use or category changed")
            inspect_model(path, source.identifier, source.category, source.bounds,
                          source.visible_triangles, source.collision_name,
                          source.material_map, row)

        with tempfile.TemporaryDirectory(prefix="house00982-art-gate-", dir="/tmp") as tmp:
            for art in ART:
                model_row = rows[art.model_id]
                texture_row = rows[art.texture_id]
                model = MODEL_OUTPUT / art.model_name
                texture = TEXTURE_OUTPUT / art.texture_name
                if digest(model) != art.model_sha256 or digest(texture) != art.texture_sha256:
                    raise RuntimeError(f"art {art.number}: committed output differs from group pin")
                if model_row.get("sourceSha256") != art.model_sha256 or \
                        texture_row.get("sourceSha256") != art.texture_sha256:
                    raise RuntimeError(f"art {art.number}: manifest output hash changed")
                if texture_row.get("texture", {}).get("width") != RUNTIME_TEXTURE_PIXELS[0] or \
                        texture_row.get("texture", {}).get("height") != RUNTIME_TEXTURE_PIXELS[1]:
                    raise RuntimeError(f"art {art.number}: texture dimensions changed")
                if texture_row.get("origin", {}).get("licence") != "CC0-1.0" or \
                        art.source_sha256 not in texture_row.get("origin", {}).get("note", ""):
                    raise RuntimeError(f"art {art.number}: CC0 provenance changed")
                material_map = {
                    "ART_FRAME_DARK": "MAT_FURNITURE_PIANO_WOOD",
                    "ART_MAT": "MAT_LIVING_ART_PAPER",
                    f"ART_IMAGE_{art.number}": art.material_id,
                }
                if art.style == "brass":
                    material_map.pop("ART_FRAME_DARK")
                    material_map["ART_FRAME_BRASS"] = "MAT_FURNITURE_PIANO_BRASS"
                inspect_model(model, art.model_id, "picture", art.bounds, 446,
                              "WallArt_COL", material_map, model_row)
                generated = Path(tmp) / art.model_name
                prepare_art_model(art, texture, generated)
                if digest(generated) != art.model_sha256:
                    raise RuntimeError(f"art {art.number}: deterministic frame regeneration changed")
        validate_rug_reuse(rows)
        total = sum(source.visible_triangles for source in SOURCES) + 446 * len(ART)
        print(f"decoration_prepare: four acquired decorations within cap six, four-image CC0 "
              f"art set within cap eight and reused CC0-weave rug planes pass; {total} visible "
              f"and 96 collision triangles")
    except (KeyError, OSError, RuntimeError, subprocess.SubprocessError,
            urllib.error.URLError, ValueError) as error:
        print(f"decoration_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
