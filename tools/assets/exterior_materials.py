#!/usr/bin/env python3
"""Author and verify HOUSE-00903's nine exterior materials."""

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
PREVIEW = REPO / "docs" / "asset-review" / "materials" / "exterior" / "contact-sheet.png"
BEGIN = "    // BEGIN GENERATED EXTERIOR MATERIALS (exterior_materials.py)"
END = "    // END GENERATED EXTERIOR MATERIALS"


@dataclass(frozen=True)
class Exterior:
    material_id: str
    source: str
    tint: tuple[float, float, float]
    role: str
    uv_scale: tuple[float, float] = (1.0, 1.0)
    snow: tuple[bool, float] = (True, 40.0)
    class_override: str | None = None
    footstep_override: str | None = None
    absorption_override: float | None = None
    wet_override: tuple[float, float, float] | None = None


EXTERIORS = (
    Exterior("MAT_SIDING_WARM_WHITE", "wood_board", (1.00, 0.96, 0.88), "siding"),
    Exterior("MAT_SIDING_SAGE", "wood_board", (0.68, 0.78, 0.62), "siding"),
    Exterior("MAT_SIDING_DUSTY_BLUE", "wood_board", (0.64, 0.74, 0.90), "siding"),
    Exterior("MAT_BRICK_WATER_TABLE", "brick_red", (1.00, 0.92, 0.86), "brick",
             (0.75, 0.75)),
    # The acquired set has no roofing map. The reviewed charcoal square-tile source is the only
    # retained pattern with discrete weather-shedding units, so it supplies the visible shingle
    # courses while explicit overrides give the roof asphalt's wet/audio semantics.
    Exterior("MAT_ROOF_SHINGLE", "tile_light_square", (0.66, 0.69, 0.74), "roof shingle",
             (1.5, 1.5), (True, 55.0), "asphalt", "asphalt", 0.12,
             (0.35, 2.4, 2.6)),
    Exterior("MAT_SOFFIT_WHITE", "paint_white_fine", (1.00, 0.98, 0.93), "soffit",
             snow=(False, 0.0), class_override="paint",
             footstep_override="painted_wood", absorption_override=0.12),
    Exterior("MAT_CONCRETE_BROOM", "concrete_smooth", (0.88, 0.86, 0.82), "concrete",
             (0.75, 0.75), (True, 15.0)),
    Exterior("MAT_ASPHALT_01", "asphalt_road", (1.00, 1.00, 1.00), "asphalt",
             (0.5, 0.5), (True, 12.0)),
    Exterior("MAT_GRAVEL_PATH", "gravel_mixed", (0.92, 0.90, 0.86), "gravel",
             snow=(True, 30.0)),
)


def source_material(slug: str):
    return next(material for material in ambientcg_materials.MATERIALS if material.slug == slug)


def material_row(exterior: Exterior) -> dict:
    row = pbr_to_stock.base_material_row(source_material(exterior.source))
    row["id"] = exterior.material_id
    row["tint"] = list(exterior.tint)
    row["uvScale"] = list(exterior.uv_scale)
    row["snowResponse"] = {
        "coverable": exterior.snow[0],
        "slopeLimitDeg": exterior.snow[1],
    }
    if exterior.class_override is not None:
        row["class"] = exterior.class_override
    if exterior.footstep_override is not None:
        row["footstepSurface"] = exterior.footstep_override
    if exterior.absorption_override is not None:
        row["audioAbsorption"] = exterior.absorption_override
    if exterior.wet_override is not None:
        row["wetResponse"] = {
            "albedoDarken": exterior.wet_override[0],
            "specularBoost": exterior.wet_override[1],
            "powerBoost": exterior.wet_override[2],
        }
    return row


def validate_definitions() -> list[str]:
    problems: list[str] = []
    ids = [exterior.material_id for exterior in EXTERIORS]
    roles = [exterior.role for exterior in EXTERIORS]
    source_slugs = {material.slug for material in ambientcg_materials.MATERIALS}
    if len(EXTERIORS) != 9 or len(set(ids)) != 9:
        problems.append("expected exactly 9 unique exterior ids")
    expected_roles = {
        "siding": 3, "brick": 1, "roof shingle": 1, "soffit": 1,
        "concrete": 1, "asphalt": 1, "gravel": 1,
    }
    actual_roles = {role: roles.count(role) for role in set(roles)}
    if actual_roles != expected_roles:
        problems.append(f"roles {actual_roles}, expected {expected_roles}")
    for exterior in EXTERIORS:
        if exterior.source not in source_slugs:
            problems.append(f"{exterior.material_id} uses unknown source {exterior.source}")
        if any(channel < 0.0 or channel > 1.0 for channel in exterior.tint):
            problems.append(f"{exterior.material_id} tint is outside 0..1")
        if any(scale <= 0.0 for scale in exterior.uv_scale):
            problems.append(f"{exterior.material_id} UV scale is not positive")
        if exterior.snow[0] and not 0.0 < exterior.snow[1] <= 90.0:
            problems.append(f"{exterior.material_id} has an invalid snow slope")
    roof = next(exterior for exterior in EXTERIORS if exterior.role == "roof shingle")
    if roof.class_override != "asphalt" or roof.snow[1] < 40.0 or roof.wet_override is None:
        problems.append("roof must retain asphalt response and accept snow on the house pitch")
    soffit = next(exterior for exterior in EXTERIORS if exterior.role == "soffit")
    if soffit.snow[0]:
        problems.append("downward-facing soffit must not be snow-coverable")
    return problems


def load_rows() -> list[dict]:
    sys.path.insert(0, str(REPO / "tools" / "world"))
    import layout_io

    document = json.loads(layout_io.strip_jsonc(MATERIALS_FILE.read_text(encoding="utf-8")))
    return document["materials"]


def preview_image():
    from PIL import Image, ImageChops, ImageDraw

    tile_width = 320
    image_height = 190
    label_height = 30
    columns = 3
    rows = 3
    sheet = Image.new("RGB", (columns * tile_width, rows * (image_height + label_height)),
                      (31, 34, 40))
    draw = ImageDraw.Draw(sheet)
    for index, exterior in enumerate(EXTERIORS):
        with Image.open(pbr_to_stock.TEXTURES / f"{exterior.source}_albedo.png") as source:
            albedo = source.convert("RGB").resize(
                (tile_width, image_height), Image.Resampling.LANCZOS
            )
        tint = tuple(round(channel * 255.0) for channel in exterior.tint)
        pixels = ImageChops.multiply(albedo, Image.new("RGB", albedo.size, tint))
        x = (index % columns) * tile_width
        y = (index // columns) * (image_height + label_height)
        sheet.paste(pixels, (x, y))
        label = exterior.material_id.removeprefix("MAT_").lower()
        draw.rectangle((x, y + image_height, x + tile_width, y + image_height + label_height),
                       fill=(31, 34, 40))
        draw.text((x + 7, y + image_height + 7), label, fill=(235, 235, 235))
    return sheet


def check() -> int:
    from PIL import Image, ImageChops

    problems = validate_definitions()
    rows = load_rows()
    by_id = {row["id"]: row for row in rows}
    if len(by_id) != len(rows):
        problems.append("material table contains duplicate ids")
    for exterior in EXTERIORS:
        actual = by_id.get(exterior.material_id)
        expected = material_row(exterior)
        if actual is None:
            problems.append(f"missing {exterior.material_id} in {MATERIALS_FILE}")
            continue
        for field in sorted(set(actual) | set(expected)):
            if actual.get(field) != expected.get(field):
                problems.append(
                    f"{exterior.material_id}/{field}: {actual.get(field)!r}, "
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
            print(f"exterior_materials: {problem}", file=sys.stderr)
        return 1
    print("exterior_materials: 9 derived exterior materials clean")
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
            print(f"exterior_materials: {problem}", file=sys.stderr)
        return 1
    text = MATERIALS_FILE.read_text(encoding="utf-8")
    if text.count(BEGIN) != 1 or text.count(END) != 1:
        print("exterior_materials: generated markers are missing or duplicated", file=sys.stderr)
        return 1
    rendered = "\n".join(render_row(material_row(exterior))
                           for exterior in sorted(EXTERIORS, key=lambda item: item.material_id))
    before, remainder = text.split(BEGIN, 1)
    _, after = remainder.split(END, 1)
    MATERIALS_FILE.write_text(before + BEGIN + "\n" + rendered + "\n" + END + after,
                              encoding="utf-8")
    print(f"exterior_materials: wrote {len(EXTERIORS)} materials to {MATERIALS_FILE}")
    return 0


def write_preview() -> int:
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    preview_image().save(PREVIEW, optimize=True)
    print(f"exterior_materials: wrote exterior review to {PREVIEW}")
    return 0


def selftest() -> int:
    problems = validate_definitions()
    if problems:
        for problem in problems:
            print(f"exterior_materials: {problem}", file=sys.stderr)
        return 1
    rows = [material_row(exterior) for exterior in EXTERIORS]
    if any(row["lightmapChannel"] != 1 or row["effectTierS"] != "DualTexture"
           for row in rows):
        print("exterior_materials: every exterior must remain a static lightmap receiver",
              file=sys.stderr)
        return 1
    if sum(row["snowResponse"]["coverable"] for row in rows) != 8:
        print("exterior_materials: exactly the downward-facing soffit must reject snow",
              file=sys.stderr)
        return 1
    print("exterior_materials: selftest passed")
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
