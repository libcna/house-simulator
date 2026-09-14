#!/usr/bin/env python3
"""Author and verify HOUSE-00902's twelve interior floor materials."""

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
PREVIEW = REPO / "docs" / "asset-review" / "materials" / "interior-floors" / "contact-sheet.png"
BEGIN = "    // BEGIN GENERATED FLOOR MATERIALS (floor_materials.py)"
END = "    // END GENERATED FLOOR MATERIALS"


@dataclass(frozen=True)
class Floor:
    material_id: str
    source: str
    tint: tuple[float, float, float]
    role: str
    uv_scale: tuple[float, float] = (1.0, 1.0)
    class_override: str | None = None
    footstep_override: str | None = None
    absorption_override: float | None = None


FLOORS = (
    Floor("MAT_WOOD_OAK_FLOOR", "wood_oak_floor", (1.00, 0.97, 0.93), "oak"),
    Floor("MAT_WOOD_WALNUT_FLOOR", "wood_dark_floor", (0.78, 0.60, 0.48), "walnut"),
    Floor("MAT_CARPET_BEIGE", "carpet_beige", (1.00, 0.96, 0.88), "carpet"),
    Floor("MAT_CARPET_GREY", "carpet_grey", (0.82, 0.86, 0.90), "carpet"),
    Floor("MAT_CARPET_BROWN", "carpet_brown", (0.88, 0.78, 0.66), "carpet"),
    # The acquired slugs record their source-selection roles, but the retained review shows their
    # actual values in the opposite order: tile_light is charcoal and tile_dark is pale. Variant
    # names follow what is visible, while the immutable source slugs remain honest provenance.
    Floor("MAT_TILE_PORCELAIN_GREY", "tile_warm_square", (0.92, 0.96, 1.00), "tile"),
    Floor("MAT_TILE_CERAMIC_WARM", "tile_grey_square", (1.00, 0.94, 0.86), "tile"),
    Floor("MAT_TILE_CERAMIC_LIGHT", "tile_dark_square", (1.00, 1.00, 1.00), "tile"),
    Floor("MAT_TILE_CERAMIC_DARK", "tile_light_square", (0.86, 0.88, 0.92), "tile"),
    Floor("MAT_FLOOR_CONCRETE", "concrete_smooth", (0.88, 0.86, 0.82), "concrete",
          (0.75, 0.75)),
    Floor("MAT_FLOOR_STONE", "stone_marble", (1.00, 0.98, 0.95), "stone"),
    # §22.2 has no vinyl class. Tile is the nearest static hard-floor path and, unlike Plastic,
    # retains DualTextureEffect's second UV/lightmap slot. Explicit footstep/audio data preserves
    # vinyl's gameplay semantics rather than pretending that it sounds like ceramic.
    Floor("MAT_FLOOR_VINYL", "wood_light_floor", (0.90, 0.88, 0.82), "vinyl",
          class_override="tile", footstep_override="vinyl", absorption_override=0.12),
)


def source_material(slug: str):
    return next(material for material in ambientcg_materials.MATERIALS if material.slug == slug)


def material_row(floor: Floor) -> dict:
    row = pbr_to_stock.base_material_row(source_material(floor.source))
    row["id"] = floor.material_id
    row["tint"] = list(floor.tint)
    row["uvScale"] = list(floor.uv_scale)
    row["snowResponse"] = {"coverable": False, "slopeLimitDeg": 0.0}
    if floor.class_override is not None:
        row["class"] = floor.class_override
    if floor.footstep_override is not None:
        row["footstepSurface"] = floor.footstep_override
    if floor.absorption_override is not None:
        row["audioAbsorption"] = floor.absorption_override
    return row


def validate_definitions() -> list[str]:
    problems: list[str] = []
    ids = [floor.material_id for floor in FLOORS]
    roles = [floor.role for floor in FLOORS]
    source_slugs = {material.slug for material in ambientcg_materials.MATERIALS}
    if len(FLOORS) != 12 or len(set(ids)) != 12:
        problems.append("expected exactly 12 unique floor ids")
    expected_roles = {
        "oak": 1, "walnut": 1, "carpet": 3, "tile": 4,
        "concrete": 1, "stone": 1, "vinyl": 1,
    }
    actual_roles = {role: roles.count(role) for role in set(roles)}
    if actual_roles != expected_roles:
        problems.append(f"roles {actual_roles}, expected {expected_roles}")
    for floor in FLOORS:
        if floor.source not in source_slugs:
            problems.append(f"{floor.material_id} uses unknown source {floor.source}")
        if any(channel < 0.0 or channel > 1.0 for channel in floor.tint):
            problems.append(f"{floor.material_id} tint is outside 0..1")
        if any(scale <= 0.0 for scale in floor.uv_scale):
            problems.append(f"{floor.material_id} UV scale is not positive")
    for required in ("MAT_WOOD_OAK_FLOOR", "MAT_TILE_PORCELAIN_GREY"):
        if required not in ids:
            problems.append(f"architecture example {required} is missing")
    vinyl = next(floor for floor in FLOORS if floor.role == "vinyl")
    if vinyl.class_override != "tile" or vinyl.footstep_override != "vinyl":
        problems.append("vinyl must use the lightmapped tile path and explicit vinyl footsteps")
    return problems


def load_rows() -> dict[str, dict]:
    sys.path.insert(0, str(REPO / "tools" / "world"))
    import layout_io

    document = json.loads(layout_io.strip_jsonc(MATERIALS_FILE.read_text(encoding="utf-8")))
    return {row["id"]: row for row in document["materials"]}


def preview_image():
    from PIL import Image, ImageChops, ImageDraw

    tile_width = 320
    image_height = 190
    label_height = 30
    columns = 4
    rows = 3
    sheet = Image.new("RGB", (columns * tile_width, rows * (image_height + label_height)),
                      (31, 34, 40))
    draw = ImageDraw.Draw(sheet)
    for index, floor in enumerate(FLOORS):
        with Image.open(pbr_to_stock.TEXTURES / f"{floor.source}_albedo.png") as source:
            albedo = source.convert("RGB").resize(
                (tile_width, image_height), Image.Resampling.LANCZOS
            )
        tint = tuple(round(channel * 255.0) for channel in floor.tint)
        pixels = ImageChops.multiply(albedo, Image.new("RGB", albedo.size, tint))
        x = (index % columns) * tile_width
        y = (index // columns) * (image_height + label_height)
        sheet.paste(pixels, (x, y))
        label = floor.material_id.removeprefix("MAT_").lower()
        draw.rectangle((x, y + image_height, x + tile_width, y + image_height + label_height),
                       fill=(31, 34, 40))
        draw.text((x + 7, y + image_height + 7), label, fill=(235, 235, 235))
    return sheet


def check() -> int:
    from PIL import Image, ImageChops

    problems = validate_definitions()
    rows = load_rows()
    for floor in FLOORS:
        actual = rows.get(floor.material_id)
        expected = material_row(floor)
        if actual is None:
            problems.append(f"missing {floor.material_id} in {MATERIALS_FILE}")
            continue
        for field in sorted(set(actual) | set(expected)):
            if actual.get(field) != expected.get(field):
                problems.append(
                    f"{floor.material_id}/{field}: {actual.get(field)!r}, "
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
            print(f"floor_materials: {problem}", file=sys.stderr)
        return 1
    print("floor_materials: 12 derived interior floors clean")
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
            print(f"floor_materials: {problem}", file=sys.stderr)
        return 1
    text = MATERIALS_FILE.read_text(encoding="utf-8")
    if text.count(BEGIN) != 1 or text.count(END) != 1:
        print("floor_materials: generated markers are missing or duplicated", file=sys.stderr)
        return 1
    rendered = "\n".join(render_row(material_row(floor))
                           for floor in sorted(FLOORS, key=lambda item: item.material_id))
    before, remainder = text.split(BEGIN, 1)
    _, after = remainder.split(END, 1)
    MATERIALS_FILE.write_text(before + BEGIN + "\n" + rendered + "\n" + END + after,
                              encoding="utf-8")
    print(f"floor_materials: wrote {len(FLOORS)} floors to {MATERIALS_FILE}")
    return 0


def write_preview() -> int:
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    preview_image().save(PREVIEW, optimize=True)
    print(f"floor_materials: wrote floor review to {PREVIEW}")
    return 0


def selftest() -> int:
    problems = validate_definitions()
    if problems:
        for problem in problems:
            print(f"floor_materials: {problem}", file=sys.stderr)
        return 1
    for floor in FLOORS:
        row = material_row(floor)
        if row["lightmapChannel"] != 1 or row["effectTierS"] != "DualTexture":
            print(f"floor_materials: {floor.material_id} is not a lightmap receiver", file=sys.stderr)
            return 1
        if row["snowResponse"]["coverable"]:
            print(f"floor_materials: {floor.material_id} is snow-coverable", file=sys.stderr)
            return 1
    print("floor_materials: selftest passed")
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
