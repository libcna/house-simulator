#!/usr/bin/env python3
"""Author and verify HOUSE-00904's eight glass and water materials."""

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
TEXTURES = REPO / "assets-src" / "Textures" / "Materials"
ALBEDO = TEXTURES / "water_flow_albedo.png"
NORMAL = TEXTURES / "water_flow_normal.png"
PREVIEW = REPO / "docs" / "asset-review" / "materials" / "glass-water" / "contact-sheet.png"
BEGIN = "    // BEGIN GENERATED GLASS/WATER MATERIALS (glass_water_materials.py)"
END = "    // END GENERATED GLASS/WATER MATERIALS"
SIZE = 256


@dataclass(frozen=True)
class TransparentMaterial:
    material_id: str
    material_class: str
    role: str
    tint: tuple[float, float, float]
    specular: tuple[float, float, float]
    power: float
    alpha: float
    absorption: float
    effect_tier_e: str
    uv_scale: tuple[float, float] = (1.0, 1.0)


MATERIALS = (
    TransparentMaterial("MAT_GLASS_CLEAR", "glass", "clear glass", (0.78, 0.90, 1.00),
                        (0.90, 0.95, 1.00), 128.0, 0.12, 0.03, "RoomLit/Glass"),
    TransparentMaterial("MAT_GLASS_OBSCURED", "glass", "obscured glass", (0.82, 0.90, 0.92),
                        (0.78, 0.84, 0.86), 64.0, 0.32, 0.05, "RoomLit/Glass"),
    TransparentMaterial("MAT_GLASS_CABINET", "glass", "cabinet glass", (0.82, 0.90, 1.00),
                        (0.85, 0.90, 1.00), 96.0, 0.18, 0.03, "RoomLit/Glass"),
    TransparentMaterial("MAT_GLASS_SHOWER", "glass", "shower screen", (0.75, 0.92, 1.00),
                        (0.90, 0.96, 1.00), 112.0, 0.16, 0.04, "RoomLit/Glass"),
    TransparentMaterial("MAT_WATER_FLOW", "water", "tap and shower flow", (0.58, 0.82, 1.00),
                        (1.00, 1.00, 1.00), 128.0, 0.52, 0.02, "WaterFlow", (0.5, 3.0)),
    TransparentMaterial("MAT_WATER_BASIN", "water", "basin and bath", (0.42, 0.70, 0.88),
                        (0.92, 0.98, 1.00), 112.0, 0.38, 0.02, "WaterFlow"),
    TransparentMaterial("MAT_WATER_TOILET", "water", "toilet bowl", (0.50, 0.78, 0.92),
                        (0.92, 0.98, 1.00), 96.0, 0.46, 0.02, "WaterFlow", (1.5, 1.5)),
    TransparentMaterial("MAT_WATER_PUDDLE", "water", "rain puddle", (0.32, 0.55, 0.68),
                        (0.85, 0.94, 1.00), 144.0, 0.28, 0.01, "WaterFlow", (0.75, 0.75)),
)


def material_row(material: TransparentMaterial) -> dict:
    water = material.material_class == "water"
    return {
        "id": material.material_id,
        "class": material.material_class,
        "albedo": "Textures/Materials/water_flow_albedo" if water else None,
        "normal": "Textures/Materials/water_flow_normal" if water else None,
        "lightmapChannel": 0,
        "tint": list(material.tint),
        "specularColor": list(material.specular),
        "specularPower": material.power,
        "alphaMode": "blend",
        "alpha": material.alpha,
        "alphaCutoff": None,
        "twoSided": True,
        "uvScale": list(material.uv_scale),
        "wetResponse": {"albedoDarken": 0.0, "specularBoost": 1.0, "powerBoost": 1.0},
        "snowResponse": {"coverable": False, "slopeLimitDeg": 0.0},
        "footstepSurface": material.material_class,
        "audioAbsorption": material.absorption,
        "effectTierS": "Basic",
        "effectTierE": material.effect_tier_e,
    }


def height_at(x: int, y: int) -> float:
    """Periodic directional ripples: stretch V at draw time and scroll it for flowing water."""
    u = x / SIZE
    v = y / SIZE
    return (0.55 * math.sin(math.tau * (4.0 * u + v)) +
            0.30 * math.sin(math.tau * (9.0 * u - 3.0 * v)) +
            0.15 * math.cos(math.tau * (2.0 * u + 5.0 * v)))


def texture_images() -> tuple[Image.Image, Image.Image]:
    albedo = Image.new("RGBA", (SIZE, SIZE))
    normal = Image.new("RGB", (SIZE, SIZE))
    albedo_pixels = albedo.load()
    normal_pixels = normal.load()
    heights = [[height_at(x, y) for x in range(SIZE)] for y in range(SIZE)]
    for y in range(SIZE):
        for x in range(SIZE):
            height = heights[y][x]
            highlight = max(0.0, height)
            alpha = round(112.0 + 54.0 * (height + 1.0) / 2.0 + 42.0 * highlight)
            albedo_pixels[x, y] = (
                round(184.0 + 36.0 * highlight),
                round(220.0 + 25.0 * highlight),
                round(239.0 + 16.0 * highlight),
                min(230, alpha),
            )
            dx = heights[y][(x + 1) % SIZE] - heights[y][(x - 1) % SIZE]
            dy = heights[(y + 1) % SIZE][x] - heights[(y - 1) % SIZE][x]
            nx = -3.5 * dx
            ny = -3.5 * dy
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
    water_albedo, _ = texture_images()
    tile_width = 320
    image_height = 190
    label_height = 30
    sheet = Image.new("RGB", (4 * tile_width, 2 * (image_height + label_height)), (31, 34, 40))
    draw = ImageDraw.Draw(sheet)
    for index, material in enumerate(MATERIALS):
        backdrop = Image.new("RGBA", (tile_width, image_height), (57, 60, 64, 255))
        backdrop_draw = ImageDraw.Draw(backdrop)
        backdrop_draw.rectangle((0, 0, tile_width // 2, image_height), fill=(194, 164, 124, 255))
        backdrop_draw.rectangle((tile_width // 2, 0, tile_width, image_height),
                                fill=(47, 71, 91, 255))
        for line in range(15, tile_width, 32):
            backdrop_draw.line((line, 0, line, image_height), fill=(238, 231, 211, 255), width=2)
        for line in range(15, image_height, 32):
            backdrop_draw.line((0, line, tile_width, line), fill=(35, 38, 42, 255), width=2)

        tint = tuple(round(channel * 255.0) for channel in material.tint)
        if material.material_class == "water":
            overlay = water_albedo.resize((tile_width, image_height), Image.Resampling.BILINEAR)
            coloured = ImageChops.multiply(overlay.convert("RGB"), Image.new("RGB", overlay.size, tint))
            alpha = overlay.getchannel("A").point(
                lambda value: round(value * material.alpha)
            )
            coloured.putalpha(alpha)
        else:
            alpha = round(material.alpha * 255.0)
            # Obscured glass additionally veils fine detail, while the source-less clear variants
            # remain honest uniform BasicEffect tints.
            if material.role == "obscured glass":
                alpha = round(0.42 * 255.0)
            coloured = Image.new("RGBA", backdrop.size, tint + (alpha,))
        composed = Image.alpha_composite(backdrop, coloured).convert("RGB")
        x = (index % 4) * tile_width
        y = (index // 4) * (image_height + label_height)
        sheet.paste(composed, (x, y))
        draw.rectangle((x, y + image_height, x + tile_width, y + image_height + label_height),
                       fill=(31, 34, 40))
        draw.text((x + 7, y + image_height + 7),
                  material.material_id.removeprefix("MAT_").lower(), fill=(235, 235, 235))
    return sheet


def validate_definitions() -> list[str]:
    problems: list[str] = []
    ids = [material.material_id for material in MATERIALS]
    classes = [material.material_class for material in MATERIALS]
    if len(MATERIALS) != 8 or len(set(ids)) != 8:
        problems.append("expected exactly 8 unique material ids")
    if classes.count("glass") != 4 or classes.count("water") != 4:
        problems.append("expected exactly 4 glass and 4 water materials")
    required_roles = {
        "clear glass", "obscured glass", "cabinet glass", "shower screen",
        "tap and shower flow", "basin and bath", "toilet bowl", "rain puddle",
    }
    if {material.role for material in MATERIALS} != required_roles:
        problems.append("glass/water role inventory is incomplete")
    for material in MATERIALS:
        if not 0.0 < material.alpha < 1.0:
            problems.append(f"{material.material_id} alpha must be translucent")
        if any(channel < 0.0 or channel > 1.0 for channel in material.tint + material.specular):
            problems.append(f"{material.material_id} colour is outside 0..1")
        if material.power <= 0.0 or any(scale <= 0.0 for scale in material.uv_scale):
            problems.append(f"{material.material_id} power/UV scale must be positive")
    return problems


def load_rows() -> list[dict]:
    sys.path.insert(0, str(REPO / "tools" / "world"))
    import layout_io

    document = json.loads(layout_io.strip_jsonc(MATERIALS_FILE.read_text(encoding="utf-8")))
    return document["materials"]


def compare_image(path: Path, expected: Image.Image, problems: list[str]) -> None:
    try:
        with Image.open(path) as source:
            actual = source.convert(expected.mode)
            actual.load()
        if actual.size != expected.size:
            problems.append(f"{path}: size {actual.size}, expected {expected.size}")
        elif ImageChops.difference(actual, expected).getbbox() is not None:
            problems.append(f"{path}: pixels are stale; regenerate with --write/--preview")
    except OSError as error:
        problems.append(f"{path}: {error}")


def check() -> int:
    problems = validate_definitions()
    rows = load_rows()
    by_id = {row["id"]: row for row in rows}
    if len(by_id) != len(rows):
        problems.append("material table contains duplicate ids")
    for material in MATERIALS:
        actual = by_id.get(material.material_id)
        expected = material_row(material)
        if actual is None:
            problems.append(f"missing {material.material_id} in {MATERIALS_FILE}")
            continue
        for field in sorted(set(actual) | set(expected)):
            if actual.get(field) != expected.get(field):
                problems.append(
                    f"{material.material_id}/{field}: {actual.get(field)!r}, "
                    f"expected {expected.get(field)!r}"
                )
    expected_albedo, expected_normal = texture_images()
    compare_image(ALBEDO, expected_albedo, problems)
    compare_image(NORMAL, expected_normal, problems)
    compare_image(PREVIEW, preview_image(), problems)
    if problems:
        for problem in problems:
            print(f"glass_water_materials: {problem}", file=sys.stderr)
        return 1
    hashes = (hashlib.sha256(ALBEDO.read_bytes()).hexdigest()[:12],
              hashlib.sha256(NORMAL.read_bytes()).hexdigest()[:12])
    print(f"glass_water_materials: 8 materials and water maps clean ({hashes[0]}, {hashes[1]})")
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
    problems = validate_definitions()
    if problems:
        for problem in problems:
            print(f"glass_water_materials: {problem}", file=sys.stderr)
        return 1
    text = MATERIALS_FILE.read_text(encoding="utf-8")
    if text.count(BEGIN) != 1 or text.count(END) != 1:
        print("glass_water_materials: generated markers are missing or duplicated", file=sys.stderr)
        return 1
    rendered = "\n".join(render_row(material_row(material))
                           for material in sorted(MATERIALS, key=lambda item: item.material_id))
    before, remainder = text.split(BEGIN, 1)
    _, after = remainder.split(END, 1)
    MATERIALS_FILE.write_text(before + BEGIN + "\n" + rendered + "\n" + END + after,
                              encoding="utf-8")
    albedo, normal = texture_images()
    ALBEDO.write_bytes(png_bytes(albedo))
    NORMAL.write_bytes(png_bytes(normal))
    print(f"glass_water_materials: wrote 8 rows and two {SIZE}x{SIZE} water maps")
    return 0


def write_preview() -> int:
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    PREVIEW.write_bytes(png_bytes(preview_image()))
    print(f"glass_water_materials: wrote transparent review to {PREVIEW}")
    return 0


def selftest() -> int:
    problems = validate_definitions()
    albedo, normal = texture_images()
    if png_bytes(albedo) != png_bytes(texture_images()[0]):
        problems.append("water albedo generation is not byte-deterministic")
    if png_bytes(normal) != png_bytes(texture_images()[1]):
        problems.append("water normal generation is not byte-deterministic")
    for material in MATERIALS:
        row = material_row(material)
        if (row["effectTierS"] != "Basic" or row["alphaMode"] != "blend" or
                not row["twoSided"] or row["snowResponse"]["coverable"]):
            problems.append(f"{material.material_id} violates the transparent stock path")
    if problems:
        for problem in problems:
            print(f"glass_water_materials: {problem}", file=sys.stderr)
        return 1
    print("glass_water_materials: selftest passed")
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
