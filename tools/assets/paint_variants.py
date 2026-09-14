#!/usr/bin/env python3
"""Author and verify HOUSE-00901's 12 wall and 6 ceiling paint variants.

The eventual cell-to-palette assignment belongs to HOUSE-00908. This tool owns only the fixed
material library and its tint review. Every row is derived from HOUSE-00900's measured base row,
so colour selection cannot accidentally invent a different roughness/specular mapping for the
same prepared texture.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ambientcg_materials  # noqa: E402
import pbr_to_stock  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
MATERIALS_FILE = REPO / "assets-src" / "world" / "layout.materials.json"
PREVIEW = REPO / "docs" / "asset-review" / "materials" / "interior-paints" / "contact-sheet.png"
BEGIN = "    // BEGIN GENERATED INTERIOR PAINT VARIANTS (paint_variants.py)"
END = "    // END GENERATED INTERIOR PAINT VARIANTS"


@dataclass(frozen=True)
class Variant:
    material_id: str
    source: str
    tint: tuple[float, float, float]
    role: str


# Multipliers stay at or below one because XNA's DiffuseColor can darken/tint a sampled albedo but
# cannot restore clipped detail by amplifying it.  Visual review rejected broad use of five
# distressed paint sources: a coherent occupied house uses the clean fine plaster for its regular
# palette and reserves chipped maps for four explicitly aged/service finishes.
VARIANTS = (
    Variant("MAT_PAINT_WARM_WHITE", "paint_white_fine", (1.00, 0.96, 0.90), "wall"),
    Variant("MAT_PAINT_SOFT_WHITE", "paint_white_fine", (0.96, 0.98, 1.00), "wall"),
    Variant("MAT_PAINT_FLAT_WHITE", "paint_white_fine", (1.00, 1.00, 1.00), "ceiling"),
    Variant("MAT_PAINT_IVORY", "paint_white_fine", (1.00, 0.91, 0.74), "wall"),
    Variant("MAT_PAINT_LINEN", "paint_white_fine", (0.88, 0.80, 0.69), "wall"),
    Variant("MAT_PAINT_CEILING_WARM", "paint_white_fine", (1.00, 0.98, 0.93), "ceiling"),
    Variant("MAT_PAINT_SOFT_GREY", "paint_white_fine", (0.78, 0.82, 0.85), "wall"),
    Variant("MAT_PAINT_SAGE", "paint_white_fine", (0.68, 0.79, 0.64), "wall"),
    Variant("MAT_PAINT_CEILING_COOL", "paint_white_fine", (0.94, 0.98, 1.00), "ceiling"),
    Variant("MAT_PAINT_PALE_SAND", "paint_white_fine", (0.94, 0.80, 0.60), "wall"),
    Variant("MAT_PAINT_PALE_ROSE", "paint_white_fine", (1.00, 0.75, 0.78), "wall"),
    Variant("MAT_PAINT_CEILING_CREAM", "paint_white_fine", (1.00, 0.95, 0.86), "ceiling"),
    Variant("MAT_PAINT_DUSTY_BLUE", "paint_cool_rough", (0.78, 0.86, 1.00), "wall"),
    Variant("MAT_PAINT_MUTED_TEAL", "paint_white_fine", (0.56, 0.80, 0.75), "wall"),
    Variant("MAT_PAINT_CEILING_MOISTURE", "paint_white_fine", (0.91, 0.98, 1.00), "ceiling"),
    Variant("MAT_PAINT_AGED_PLASTER", "paint_grey_fine", (0.90, 0.90, 0.86), "wall"),
    Variant("MAT_PAINT_SMOKY_OCHRE", "paint_aged", (0.72, 0.55, 0.40), "wall"),
    Variant("MAT_PAINT_CEILING_ATTIC", "paint_grey_fine", (1.00, 0.96, 0.87), "ceiling"),
)


def source_material(slug: str):
    return next(material for material in ambientcg_materials.MATERIALS if material.slug == slug)


def material_row(variant: Variant) -> dict:
    row = pbr_to_stock.base_material_row(source_material(variant.source))
    row["id"] = variant.material_id
    row["tint"] = list(variant.tint)
    row["snowResponse"] = {"coverable": False, "slopeLimitDeg": 0.0}
    return row


def validate_definitions() -> list[str]:
    problems: list[str] = []
    ids = [variant.material_id for variant in VARIANTS]
    sources = {material.slug for material in ambientcg_materials.MATERIALS
               if material.category == "paint"}
    if len(VARIANTS) != 18:
        problems.append(f"defined {len(VARIANTS)} variants, expected 18")
    if len(set(ids)) != len(ids):
        problems.append("variant ids are not unique")
    if sum(variant.role == "wall" for variant in VARIANTS) != 12:
        problems.append("expected exactly 12 wall variants")
    if sum(variant.role == "ceiling" for variant in VARIANTS) != 6:
        problems.append("expected exactly 6 ceiling variants")
    for variant in VARIANTS:
        if variant.source not in sources:
            problems.append(f"{variant.material_id} uses non-paint source {variant.source}")
        if any(channel < 0.0 or channel > 1.0 for channel in variant.tint):
            problems.append(f"{variant.material_id} tint is outside 0..1")
    for required in ("MAT_PAINT_WARM_WHITE", "MAT_PAINT_FLAT_WHITE"):
        if required not in ids:
            problems.append(f"architecture example {required} is missing")
    return problems


def load_rows() -> dict[str, dict]:
    sys.path.insert(0, str(REPO / "tools" / "world"))
    import layout_io

    document = json.loads(layout_io.strip_jsonc(MATERIALS_FILE.read_text(encoding="utf-8")))
    return {row["id"]: row for row in document["materials"]}


def check() -> int:
    from PIL import Image, ImageChops

    problems = validate_definitions()
    rows = load_rows()
    for variant in VARIANTS:
        actual = rows.get(variant.material_id)
        expected = material_row(variant)
        if actual is None:
            problems.append(f"missing {variant.material_id} in {MATERIALS_FILE}")
            continue
        for field in sorted(set(actual) | set(expected)):
            if actual.get(field) != expected.get(field):
                problems.append(
                    f"{variant.material_id}/{field}: {actual.get(field)!r}, "
                    f"expected {expected.get(field)!r}"
                )
    try:
        with Image.open(PREVIEW) as source:
            actual_preview = source.convert("RGB")
            actual_preview.load()
        expected_preview = preview_image()
        if actual_preview.size != expected_preview.size:
            problems.append(
                f"{PREVIEW}: size {actual_preview.size}, expected {expected_preview.size}"
            )
        elif ImageChops.difference(actual_preview, expected_preview).getbbox() is not None:
            problems.append(f"{PREVIEW}: pixels are stale; regenerate with --preview")
    except OSError as error:
        problems.append(f"{PREVIEW}: {error}")
    if problems:
        for problem in problems:
            print(f"paint_variants: {problem}", file=sys.stderr)
        return 1
    source_count = len({variant.source for variant in VARIANTS})
    print(f"paint_variants: 18 derived variants clean -- 12 wall, 6 ceiling, "
          f"{source_count} selected source paints")
    return 0


def render_row(row: dict) -> str:
    def value(field: str) -> str:
        return json.dumps(row[field])

    return "\n".join((
        "    {",
        f'      "id": {value("id")}, "class": {value("class")},',
        f'      "albedo": {value("albedo")},',
        f'      "normal": {value("normal")}, "lightmapChannel": {value("lightmapChannel")},',
        f'      "tint": {value("tint")}, "specularColor": {value("specularColor")},',
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
            print(f"paint_variants: {problem}", file=sys.stderr)
        return 1
    text = MATERIALS_FILE.read_text(encoding="utf-8")
    if text.count(BEGIN) != 1 or text.count(END) != 1:
        print("paint_variants: generated markers are missing or duplicated", file=sys.stderr)
        return 1
    rendered = "\n".join(render_row(material_row(variant))
                           for variant in sorted(VARIANTS, key=lambda item: item.material_id))
    before, remainder = text.split(BEGIN, 1)
    _, after = remainder.split(END, 1)
    MATERIALS_FILE.write_text(before + BEGIN + "\n" + rendered + "\n" + END + after,
                              encoding="utf-8")
    print(f"paint_variants: wrote {len(VARIANTS)} variants to {MATERIALS_FILE}")
    return 0


def preview_image():
    """Return a six-column tint sheet using the runtime multiply operation."""
    from PIL import Image, ImageChops, ImageDraw

    tile_width = 300
    image_height = 150
    label_height = 30
    columns = 6
    rows = 3
    sheet = Image.new("RGB", (columns * tile_width, rows * (image_height + label_height)),
                      (31, 34, 40))
    draw = ImageDraw.Draw(sheet)
    for index, variant in enumerate(VARIANTS):
        with Image.open(pbr_to_stock.TEXTURES / f"{variant.source}_albedo.png") as source:
            albedo = source.convert("RGB").resize(
                (tile_width, image_height), Image.Resampling.LANCZOS
            )
        tint = tuple(round(channel * 255.0) for channel in variant.tint)
        tint_image = Image.new("RGB", albedo.size, tint)
        pixels = ImageChops.multiply(albedo, tint_image)
        x = (index % columns) * tile_width
        y = (index // columns) * (image_height + label_height)
        sheet.paste(pixels, (x, y))
        label = variant.material_id.removeprefix("MAT_PAINT_").lower()
        draw.rectangle((x, y + image_height, x + tile_width, y + image_height + label_height),
                       fill=(31, 34, 40))
        draw.text((x + 7, y + image_height + 7), label, fill=(235, 235, 235))
    return sheet


def write_preview() -> int:
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    preview_image().save(PREVIEW, optimize=True)
    print(f"paint_variants: wrote 18-variant review to {PREVIEW}")
    return 0


def selftest() -> int:
    problems = validate_definitions()
    if problems:
        for problem in problems:
            print(f"paint_variants: {problem}", file=sys.stderr)
        return 1
    first = material_row(VARIANTS[0])
    base = pbr_to_stock.base_material_row(source_material(VARIANTS[0].source))
    if first["specularPower"] != base["specularPower"] or first["albedo"] != base["albedo"]:
        print("paint_variants: derived visual mapping differs from its base", file=sys.stderr)
        return 1
    if first["snowResponse"]["coverable"]:
        print("paint_variants: an interior paint is snow-coverable", file=sys.stderr)
        return 1
    print("paint_variants: selftest passed")
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
