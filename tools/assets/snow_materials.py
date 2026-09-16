#!/usr/bin/env python3
"""Author and verify HOUSE-00906's six snow-shell material-class endpoints."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

REPO = Path(__file__).resolve().parents[2]
MATERIALS_FILE = REPO / "assets-src" / "world" / "layout.materials.json"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
TEXTURES = REPO / "assets-src" / "Textures" / "Materials"
ALBEDO = TEXTURES / "snow_shell_albedo.png"
NORMAL = TEXTURES / "snow_shell_normal.png"
PREVIEW = REPO / "docs" / "asset-review" / "materials" / "snow" / "contact-sheet.png"
BEGIN = "    // BEGIN GENERATED SNOW MATERIALS (snow_materials.py)"
END = "    // END GENERATED SNOW MATERIALS"
SIZE = 256


@dataclass(frozen=True)
class SnowMaterial:
    material_id: str
    base_class: str
    tint: tuple[float, float, float]
    specular: tuple[float, float, float]
    power: float


# These are exactly the classes in build_snowshell.py's current and planned sources: terrain,
# roofs, open-cell decks, fence/furniture tops and the car.  The checker derives the live set from
# snowshell.bin's source graph so a seventh class can never arrive as invisible white geometry.
MATERIALS = (
    SnowMaterial("MAT_SNOW_ASPHALT", "asphalt", (0.88, 0.91, 0.94), (0.10, 0.11, 0.12), 8.0),
    SnowMaterial("MAT_SNOW_GRASS", "grass", (0.92, 0.95, 0.96), (0.08, 0.09, 0.10), 6.0),
    SnowMaterial("MAT_SNOW_METAL", "metal", (0.94, 0.96, 1.00), (0.14, 0.15, 0.16), 12.0),
    SnowMaterial("MAT_SNOW_SOIL", "soil", (0.89, 0.91, 0.92), (0.08, 0.09, 0.09), 6.0),
    SnowMaterial("MAT_SNOW_STONE", "stone", (0.91, 0.93, 0.95), (0.11, 0.12, 0.13), 10.0),
    SnowMaterial("MAT_SNOW_WOOD", "wood", (0.90, 0.93, 0.94), (0.10, 0.11, 0.12), 8.0),
)


def material_row(material: SnowMaterial) -> dict:
    return {
        "id": material.material_id,
        "class": f"snow_{material.base_class}",
        "albedo": "Textures/Materials/snow_shell_albedo",
        "normal": "Textures/Materials/snow_shell_normal",
        # The Tier-S shell is displaced and drawn as one dynamic overlay by HOUSE-01793/01794.
        # It has only TEXCOORD_0 and therefore deliberately uses BasicEffect, not a lightmap slot.
        "lightmapChannel": 0,
        "tint": list(material.tint),
        "specularColor": list(material.specular),
        "specularPower": material.power,
        "alphaMode": "blend",
        "alpha": 1.0,
        "alphaCutoff": None,
        "twoSided": False,
        "uvScale": [1.0, 1.0],
        "wetResponse": {"albedoDarken": 0.0, "specularBoost": 1.0, "powerBoost": 1.0},
        "snowResponse": {"coverable": False, "slopeLimitDeg": 0.0},
        "footstepSurface": "snow",
        "audioAbsorption": 0.85,
        "effectTierS": "Basic",
        "effectTierE": "SurfaceBlend/Snowy",
    }


def lattice(ix: int, iy: int, period: int, seed: int) -> float:
    """A stable -1..1 value at one point of a wrapping integer lattice."""
    value = ((ix % period) * 374761393 + (iy % period) * 668265263 + seed * 2246822519)
    value = (value ^ (value >> 13)) * 1274126177
    value ^= value >> 16
    return ((value & 0xffff) / 32767.5) - 1.0


def value_noise(x: int, y: int, period: int, seed: int) -> float:
    """Cosine-smoothed periodic value noise; the integer period makes every edge wrap."""
    u = x * period / SIZE
    v = y * period / SIZE
    x0 = math.floor(u)
    y0 = math.floor(v)
    tx = (1.0 - math.cos((u - x0) * math.pi)) * 0.5
    ty = (1.0 - math.cos((v - y0) * math.pi)) * 0.5
    top = lattice(x0, y0, period, seed) * (1.0 - tx) + \
        lattice(x0 + 1, y0, period, seed) * tx
    bottom = lattice(x0, y0 + 1, period, seed) * (1.0 - tx) + \
        lattice(x0 + 1, y0 + 1, period, seed) * tx
    return top * (1.0 - ty) + bottom * ty


def height_at(x: int, y: int) -> float:
    """Low-relief periodic powder from four deterministic, non-directional noise octaves."""
    return (
        0.52 * value_noise(x, y, 4, 11)
        + 0.27 * value_noise(x, y, 8, 23)
        + 0.14 * value_noise(x, y, 16, 47)
        + 0.07 * value_noise(x, y, 32, 89)
    )


def texture_images() -> tuple[Image.Image, Image.Image]:
    albedo = Image.new("RGBA", (SIZE, SIZE))
    normal = Image.new("RGB", (SIZE, SIZE))
    albedo_pixels = albedo.load()
    normal_pixels = normal.load()
    heights = [[height_at(x, y) for x in range(SIZE)] for y in range(SIZE)]
    for y in range(SIZE):
        for x in range(SIZE):
            height = heights[y][x]
            # Fine powder stays high-value but not flat white; the cooler blue channel preserves
            # readable relief under the stock effect without baking a directional light into it.
            value = max(0, min(255, round(226.0 + 18.0 * height)))
            albedo_pixels[x, y] = (
                max(0, value - 7),
                max(0, value - 3),
                min(255, value + 3),
                255,
            )
            dx = heights[y][(x + 1) % SIZE] - heights[y][(x - 1) % SIZE]
            dy = heights[(y + 1) % SIZE][x] - heights[(y - 1) % SIZE][x]
            nx = -1.8 * dx
            ny = -1.8 * dy
            nz = 1.0
            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            normal_pixels[x, y] = (
                round((nx / length * 0.5 + 0.5) * 255.0),
                round((ny / length * 0.5 + 0.5) * 255.0),
                round((nz / length * 0.5 + 0.5) * 255.0),
            )
    return albedo, normal


def png_bytes(image: Image.Image) -> bytes:
    output = io.BytesIO()
    image.save(output, format="PNG", optimize=True)
    return output.getvalue()


def preview_image() -> Image.Image:
    albedo, _ = texture_images()
    tile_width = 400
    image_height = 250
    label_height = 36
    sheet = Image.new("RGB", (3 * tile_width, 2 * (image_height + label_height)), (31, 34, 40))
    draw = ImageDraw.Draw(sheet)
    substrate_colours = {
        "asphalt": (52, 56, 61),
        "grass": (54, 93, 45),
        "metal": (91, 99, 110),
        "soil": (89, 65, 43),
        "stone": (100, 112, 122),
        "wood": (111, 79, 50),
    }
    for index, material in enumerate(MATERIALS):
        texture = albedo.convert("RGB").resize((tile_width, image_height), Image.Resampling.BILINEAR)
        tint = tuple(round(channel * 255.0) for channel in material.tint)
        snow = ImageChops.multiply(texture, Image.new("RGB", texture.size, tint)).convert("RGBA")
        shallow = snow.copy()
        shallow.putalpha(round(0.38 * 255.0))
        deep = snow.copy()
        deep.putalpha(round(0.96 * 255.0))
        base = Image.new("RGBA", (tile_width, image_height), substrate_colours[material.base_class] + (255,))
        left = Image.alpha_composite(base.crop((0, 0, tile_width // 2, image_height)),
                                     shallow.crop((0, 0, tile_width // 2, image_height)))
        right = Image.alpha_composite(base.crop((tile_width // 2, 0, tile_width, image_height)),
                                      deep.crop((tile_width // 2, 0, tile_width, image_height)))
        composed = Image.new("RGB", (tile_width, image_height))
        composed.paste(left.convert("RGB"), (0, 0))
        composed.paste(right.convert("RGB"), (tile_width // 2, 0))
        x = (index % 3) * tile_width
        y = (index // 3) * (image_height + label_height)
        sheet.paste(composed, (x, y))
        draw.line((x + tile_width // 2, y, x + tile_width // 2, y + image_height),
                  fill=(235, 235, 235), width=2)
        draw.rectangle((x, y + image_height, x + tile_width, y + image_height + label_height),
                       fill=(31, 34, 40))
        draw.text((x + 9, y + image_height + 10),
                  f"{material.base_class}: shallow | deep", fill=(235, 235, 235))
    return sheet


def load_rows() -> list[dict]:
    sys.path.insert(0, str(REPO / "tools" / "world"))
    import layout_io

    document = json.loads(layout_io.strip_jsonc(MATERIALS_FILE.read_text(encoding="utf-8")))
    return document["materials"]


def shell_classes(rows: list[dict]) -> set[str]:
    sys.path.insert(0, str(REPO / "tools" / "world"))
    import build_snowshell

    by_id = {row["id"]: row for row in rows}
    built = build_snowshell.build(REPO / "assets-src" / "world", MANIFEST)
    return {by_id[shell["material"]]["class"] for shell in built["shells"]}


def validate_definitions(rows: list[dict]) -> list[str]:
    problems: list[str] = []
    ids = [material.material_id for material in MATERIALS]
    classes = [material.base_class for material in MATERIALS]
    if len(MATERIALS) != 6 or len(set(ids)) != 6 or len(set(classes)) != 6:
        problems.append("expected exactly six unique ids and base classes")
    expected_classes = {"asphalt", "grass", "metal", "soil", "stone", "wood"}
    if set(classes) != expected_classes:
        problems.append(f"snow classes are {sorted(classes)}, expected {sorted(expected_classes)}")
    try:
        actual_shell_classes = shell_classes(rows)
        if actual_shell_classes != expected_classes:
            problems.append(
                f"snow-shell source classes are {sorted(actual_shell_classes)}, "
                f"but the authored endpoints are {sorted(expected_classes)}"
            )
    except (KeyError, OSError, ValueError) as error:
        problems.append(f"could not derive snow-shell material classes: {error}")
    for material in MATERIALS:
        row = material_row(material)
        if (row["effectTierS"] != "Basic" or row["lightmapChannel"] != 0 or
                row["alphaMode"] != "blend" or row["alpha"] != 1.0):
            problems.append(f"{material.material_id} is not a full-alpha Basic shell endpoint")
        if row["snowResponse"]["coverable"] or row["footstepSurface"] != "snow":
            problems.append(f"{material.material_id} can recursively collect snow or lacks snow audio")
        if row["effectTierE"] != "SurfaceBlend/Snowy" or row["audioAbsorption"] < 0.8:
            problems.append(f"{material.material_id} does not carry the Tier-E/quiet-snow semantics")
    return problems


def compare_image(path: Path, expected: Image.Image, problems: list[str]) -> None:
    try:
        with Image.open(path) as source:
            actual = source.convert(expected.mode)
            actual.load()
        if actual.size != expected.size:
            problems.append(f"{path}: size {actual.size}, expected {expected.size}")
        elif ImageChops.difference(actual, expected).getbbox() is not None:
            problems.append(f"{path}: pixels are stale; regenerate them")
    except OSError as error:
        problems.append(f"{path}: {error}")


def check() -> int:
    rows = load_rows()
    problems = validate_definitions(rows)
    by_id = {row["id"]: row for row in rows}
    if len(by_id) != len(rows):
        problems.append("material table contains duplicate ids")
    for material in MATERIALS:
        expected = material_row(material)
        actual = by_id.get(material.material_id)
        if actual is None:
            problems.append(f"missing {material.material_id} in {MATERIALS_FILE}")
            continue
        for field in sorted(set(actual) | set(expected)):
            if actual.get(field) != expected.get(field):
                problems.append(
                    f"{material.material_id}/{field}: {actual.get(field)!r}, "
                    f"expected {expected.get(field)!r}"
                )
    snow_rows = [row for row in rows if row["class"].startswith("snow_")]
    if len(snow_rows) != 6 or {row["id"] for row in snow_rows} != {
            material.material_id for material in MATERIALS}:
        problems.append("the authored snowy-state set is not exactly the six owned variants")
    expected_albedo, expected_normal = texture_images()
    compare_image(ALBEDO, expected_albedo, problems)
    compare_image(NORMAL, expected_normal, problems)
    compare_image(PREVIEW, preview_image(), problems)
    if problems:
        for problem in problems:
            print(f"snow_materials: {problem}", file=sys.stderr)
        return 1
    hashes = (hashlib.sha256(ALBEDO.read_bytes()).hexdigest()[:12],
              hashlib.sha256(NORMAL.read_bytes()).hexdigest()[:12])
    print(f"snow_materials: 6 class endpoints and shared snow-shell maps clean "
          f"({hashes[0]}, {hashes[1]})")
    return 0


def render_row(row: dict) -> str:
    def value(field: str) -> str:
        return json.dumps(row[field])

    return "\n".join((
        "    {",
        f'      "id": {value("id")}, "class": {value("class")},',
        f'      "albedo": {value("albedo")}, "normal": {value("normal")},',
        f'      "lightmapChannel": {value("lightmapChannel")}, "tint": {value("tint")},',
        f'      "specularColor": {value("specularColor")},',
        f'      "specularPower": {value("specularPower")}, "alphaMode": {value("alphaMode")},',
        f'      "alpha": {value("alpha")}, "alphaCutoff": {value("alphaCutoff")},',
        f'      "twoSided": {value("twoSided")}, "uvScale": {value("uvScale")},',
        f'      "wetResponse": {value("wetResponse")},',
        f'      "snowResponse": {value("snowResponse")},',
        f'      "footstepSurface": {value("footstepSurface")},',
        f'      "audioAbsorption": {value("audioAbsorption")},',
        f'      "effectTierS": {value("effectTierS")}, "effectTierE": {value("effectTierE")}',
        "    },",
    ))


def write() -> int:
    rows = load_rows()
    problems = validate_definitions(rows)
    if problems:
        for problem in problems:
            print(f"snow_materials: {problem}", file=sys.stderr)
        return 1
    text = MATERIALS_FILE.read_text(encoding="utf-8")
    if text.count(BEGIN) != 1 or text.count(END) != 1:
        print("snow_materials: generated markers are missing or duplicated", file=sys.stderr)
        return 1
    rendered = "\n".join(render_row(material_row(material)) for material in MATERIALS)
    before, remainder = text.split(BEGIN, 1)
    _, after = remainder.split(END, 1)
    MATERIALS_FILE.write_text(before + BEGIN + "\n" + rendered + "\n" + END + after,
                              encoding="utf-8")
    albedo, normal = texture_images()
    ALBEDO.write_bytes(png_bytes(albedo))
    NORMAL.write_bytes(png_bytes(normal))
    print(f"snow_materials: wrote 6 rows and two {SIZE}x{SIZE} shared shell maps")
    return 0


def write_preview() -> int:
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    PREVIEW.write_bytes(png_bytes(preview_image()))
    print(f"snow_materials: wrote snow-shell review to {PREVIEW}")
    return 0


def selftest() -> int:
    rows = load_rows()
    problems = validate_definitions(rows)
    albedo, normal = texture_images()
    if png_bytes(albedo) != png_bytes(texture_images()[0]):
        problems.append("snow albedo generation is not byte-deterministic")
    if png_bytes(normal) != png_bytes(texture_images()[1]):
        problems.append("snow normal generation is not byte-deterministic")
    if hasattr(albedo, "get_flattened_data"):
        values = list(albedo.get_flattened_data())
    else:
        values = list(albedo.getdata())
    if albedo.mode != "RGBA" or normal.mode != "RGB" or albedo.size != (SIZE, SIZE) or \
            normal.size != (SIZE, SIZE):
        problems.append("snow maps must be 256-square RGBA albedo and RGB normal")
    if any(pixel[3] != 255 for pixel in values):
        problems.append("the shell albedo must stay opaque; snowDepth owns runtime alpha")
    if min(pixel[0] for pixel in values) < 190 or max(pixel[2] for pixel in values) < 235:
        problems.append("snow albedo is not a restrained high-value powder surface")
    normal_extrema = normal.getextrema()
    if normal_extrema[0][0] >= normal_extrema[0][1] or \
            normal_extrema[1][0] >= normal_extrema[1][1] or normal_extrema[2][0] < 245:
        problems.append("the snow normal lacks subtle +Z powder relief")
    seam_jumps = [
        max(abs(a - b) for a, b in zip(albedo.getpixel((0, y)),
                                      albedo.getpixel((SIZE - 1, y))))
        for y in range(SIZE)
    ] + [
        max(abs(a - b) for a, b in zip(albedo.getpixel((x, 0)),
                                      albedo.getpixel((x, SIZE - 1))))
        for x in range(SIZE)
    ]
    if max(seam_jumps) > 2:
        problems.append(f"the periodic snow albedo has an edge jump of {max(seam_jumps)}")
    if problems:
        for problem in problems:
            print(f"snow_materials: {problem}", file=sys.stderr)
        return 1
    print("snow_materials: selftest passed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--check", action="store_true")
    group.add_argument("--write", action="store_true")
    group.add_argument("--preview", action="store_true")
    group.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.write:
        return write()
    if args.preview:
        return write_preview()
    if args.selftest:
        return selftest()
    return check()


if __name__ == "__main__":
    sys.exit(main())
