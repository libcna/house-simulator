#!/usr/bin/env python3
"""Author and verify HOUSE-00905's fourteen fully-wet Tier-S material variants."""

from __future__ import annotations

import argparse
import copy
import json
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

REPO = Path(__file__).resolve().parents[2]
MATERIALS_FILE = REPO / "assets-src" / "world" / "layout.materials.json"
TEXTURES = REPO / "assets-src" / "Textures" / "Materials"
PREVIEW = REPO / "docs" / "asset-review" / "materials" / "wet" / "contact-sheet.png"
BEGIN = "    // BEGIN GENERATED WET MATERIALS (wet_materials.py)"
END = "    // END GENERATED WET MATERIALS"

# All exposed non-metal outdoor finishes.  The downward-facing soffit is sheltered; balcony and
# gutter metal already use BasicEffect's metal specular response and have no albedo tint to darken.
# Those three exclusions turn the complete seventeen-finish exterior inventory into this exact 14.
BASE_IDS = (
    "MAT_ASPHALT_01",
    "MAT_BLUESTONE_PAVER",
    "MAT_BRICK_WATER_TABLE",
    "MAT_CONCRETE_BROOM",
    "MAT_CONCRETE_KERB",
    "MAT_CONCRETE_SLAB",
    "MAT_DECK_WOOD",
    "MAT_GRAVEL_PATH",
    "MAT_GROUND_LAWN",
    "MAT_ROOF_SHINGLE",
    "MAT_SIDING_DUSTY_BLUE",
    "MAT_SIDING_SAGE",
    "MAT_SIDING_WARM_WHITE",
    "MAT_SOIL_GARDEN",
)


def load_rows() -> list[dict]:
    sys.path.insert(0, str(REPO / "tools" / "world"))
    import layout_io

    document = json.loads(layout_io.strip_jsonc(MATERIALS_FILE.read_text(encoding="utf-8")))
    return document["materials"]


def rounded(value: float) -> float:
    return round(value, 6)


def wet_id(base_id: str) -> str:
    return f"{base_id}_WET"


def wet_row(base: dict) -> dict:
    """Bake one dry row's response into a full-wet stock-XNA material."""
    response = base["wetResponse"]
    result = copy.deepcopy(base)
    result["id"] = wet_id(base["id"])
    result["class"] = f"wet_{base['class']}"
    result["tint"] = [rounded(value * (1.0 - response["albedoDarken"]))
                      for value in base["tint"]]
    result["specularColor"] = [rounded(min(1.0, value * response["specularBoost"]))
                               for value in base["specularColor"]]
    result["specularPower"] = rounded(base["specularPower"] * response["powerBoost"])
    # The response above is already baked into this Tier-S endpoint.  Identity prevents a future
    # generic response path from applying the darkening twice to a row whose class says `wet_`.
    result["wetResponse"] = {
        "albedoDarken": 0.0,
        "specularBoost": 1.0,
        "powerBoost": 1.0,
    }
    result["effectTierE"] = "SurfaceBlend/Wet"
    if base["id"] == "MAT_SOIL_GARDEN":
        result["footstepSurface"] = "mud"
    return result


def base_rows(rows: list[dict]) -> list[dict]:
    by_id = {row["id"]: row for row in rows}
    return [by_id[material_id] for material_id in BASE_IDS if material_id in by_id]


def validate_definitions(rows: list[dict]) -> list[str]:
    problems: list[str] = []
    bases = base_rows(rows)
    missing = sorted(set(BASE_IDS) - {row["id"] for row in bases})
    if missing:
        problems.append(f"missing dry bases: {', '.join(missing)}")
        return problems
    if len(BASE_IDS) != 14 or len(set(BASE_IDS)) != 14:
        problems.append("expected exactly 14 unique dry base ids")
    classes = [row["class"] for row in bases]
    expected_classes = {
        "asphalt": 2,
        "concrete": 3,
        "grass": 1,
        "gravel": 1,
        "soil": 1,
        "stone": 2,
        "wood": 4,
    }
    actual_classes = {name: classes.count(name) for name in set(classes)}
    if actual_classes != expected_classes:
        problems.append(f"base classes {actual_classes}, expected {expected_classes}")
    if any(row["class"].startswith(("wet_", "snow_")) for row in bases):
        problems.append("a wet variant cannot derive from another surface-state variant")
    if "MAT_SOFFIT_WHITE" in BASE_IDS or any(row["class"] == "metal" for row in bases):
        problems.append("sheltered soffit and already-specular metal are not wet-albedo variants")

    for base in bases:
        response = base.get("wetResponse", {})
        if not 0.0 < response.get("albedoDarken", 0.0) < 1.0:
            problems.append(f"{base['id']} has no usable albedo darkening")
            continue
        if response.get("specularBoost", 0.0) <= 1.0 or response.get("powerBoost", 0.0) <= 1.0:
            problems.append(f"{base['id']} does not increase both specular terms")
            continue
        wet = wet_row(base)
        if not any(after < before for before, after in zip(base["tint"], wet["tint"])):
            problems.append(f"{wet['id']} is not visibly darker than its dry base")
        if any(after < before for before, after in
               zip(base["specularColor"], wet["specularColor"])):
            problems.append(f"{wet['id']} reduced a specular channel")
        if wet["specularPower"] <= base["specularPower"]:
            problems.append(f"{wet['id']} did not tighten its highlight")
    return problems


def preview_image(rows: list[dict]) -> Image.Image:
    bases = base_rows(rows)
    tile_width = 320
    image_height = 170
    label_height = 30
    columns = 4
    sheet_rows = 4
    sheet = Image.new("RGB", (columns * tile_width, sheet_rows * (image_height + label_height)),
                      (31, 34, 40))
    draw = ImageDraw.Draw(sheet)
    for index, base in enumerate(bases):
        wet = wet_row(base)
        source_path = TEXTURES / f"{Path(base['albedo']).name}.png"
        with Image.open(source_path) as source:
            albedo = source.convert("RGB").resize(
                (tile_width // 2, image_height), Image.Resampling.LANCZOS
            )
        dry_tint = tuple(round(value * 255.0) for value in base["tint"])
        wet_tint = tuple(round(value * 255.0) for value in wet["tint"])
        dry_pixels = ImageChops.multiply(albedo, Image.new("RGB", albedo.size, dry_tint))
        wet_pixels = ImageChops.multiply(albedo, Image.new("RGB", albedo.size, wet_tint))
        x = (index % columns) * tile_width
        y = (index // columns) * (image_height + label_height)
        sheet.paste(dry_pixels, (x, y))
        sheet.paste(wet_pixels, (x + tile_width // 2, y))
        draw.line((x + tile_width // 2, y, x + tile_width // 2, y + image_height),
                  fill=(235, 235, 235), width=2)
        draw.rectangle((x, y + image_height, x + tile_width, y + image_height + label_height),
                       fill=(31, 34, 40))
        label = base["id"].removeprefix("MAT_").lower()
        draw.text((x + 7, y + image_height + 7), f"{label}: dry | wet", fill=(235, 235, 235))
    return sheet


def compare_preview(rows: list[dict], problems: list[str]) -> None:
    try:
        with Image.open(PREVIEW) as source:
            actual = source.convert("RGB")
            actual.load()
        expected = preview_image(rows)
        if actual.size != expected.size:
            problems.append(f"{PREVIEW}: size {actual.size}, expected {expected.size}")
        elif ImageChops.difference(actual, expected).getbbox() is not None:
            problems.append(f"{PREVIEW}: pixels are stale; regenerate with --preview")
    except OSError as error:
        problems.append(f"{PREVIEW}: {error}")


def check() -> int:
    rows = load_rows()
    problems = validate_definitions(rows)
    by_id = {row["id"]: row for row in rows}
    if len(by_id) != len(rows):
        problems.append("material table contains duplicate ids")
    for base in base_rows(rows):
        expected = wet_row(base)
        actual = by_id.get(expected["id"])
        if actual is None:
            problems.append(f"missing {expected['id']} in {MATERIALS_FILE}")
            continue
        for field in sorted(set(actual) | set(expected)):
            if actual.get(field) != expected.get(field):
                problems.append(
                    f"{expected['id']}/{field}: {actual.get(field)!r}, "
                    f"expected {expected.get(field)!r}"
                )
    wet_rows = [row for row in rows if row["class"].startswith("wet_")]
    if len(wet_rows) != 14 or {row["id"] for row in wet_rows} != {
            wet_id(material_id) for material_id in BASE_IDS}:
        problems.append("the authored wet set is not exactly the fourteen owned variants")
    compare_preview(rows, problems)
    if problems:
        for problem in problems:
            print(f"wet_materials: {problem}", file=sys.stderr)
        return 1
    print("wet_materials: 14 dry/wet endpoint pairs clean")
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
    rows = load_rows()
    problems = validate_definitions(rows)
    if problems:
        for problem in problems:
            print(f"wet_materials: {problem}", file=sys.stderr)
        return 1
    text = MATERIALS_FILE.read_text(encoding="utf-8")
    if text.count(BEGIN) != 1 or text.count(END) != 1:
        print("wet_materials: generated markers are missing or duplicated", file=sys.stderr)
        return 1
    rendered = "\n".join(render_row(wet_row(base)) for base in base_rows(rows))
    before, remainder = text.split(BEGIN, 1)
    _, after = remainder.split(END, 1)
    MATERIALS_FILE.write_text(before + BEGIN + "\n" + rendered + "\n" + END + after,
                              encoding="utf-8")
    print(f"wet_materials: wrote {len(BASE_IDS)} materials to {MATERIALS_FILE}")
    return 0


def write_preview() -> int:
    rows = load_rows()
    problems = validate_definitions(rows)
    if problems:
        for problem in problems:
            print(f"wet_materials: {problem}", file=sys.stderr)
        return 1
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    preview_image(rows).save(PREVIEW, optimize=True)
    print(f"wet_materials: wrote wet review to {PREVIEW}")
    return 0


def selftest() -> int:
    rows = load_rows()
    problems = validate_definitions(rows)
    variants = [wet_row(base) for base in base_rows(rows)]
    if any(row["effectTierS"] != "DualTexture" or row["lightmapChannel"] != 1
           for row in variants):
        problems.append("every wet endpoint must remain a static DualTexture lightmap receiver")
    if any(row["effectTierE"] != "SurfaceBlend/Wet" for row in variants):
        problems.append("every wet endpoint must name the continuous Tier-E technique")
    soil = next((row for row in variants if row["id"] == "MAT_SOIL_GARDEN_WET"), None)
    if soil is None or soil["footstepSurface"] != "mud":
        problems.append("saturated garden soil must use the mud footstep surface")
    if problems:
        for problem in problems:
            print(f"wet_materials: {problem}", file=sys.stderr)
        return 1
    print("wet_materials: selftest passed")
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
