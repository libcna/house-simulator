#!/usr/bin/env python3
"""Generate/check HOUSE-00923's BasicEffect variants from approved canonical source rows.

Unbaked outdoors uses source albedos and normals already proven in HOUSE-00902/00903. The
variants differ only in honest use (fence/marking tint) and the XNA Basic vertex/effect profile;
neither a new download nor a runtime material-name guess is involved.
"""

from __future__ import annotations

import argparse
import copy
import json
import sys
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
MATERIALS_FILE = REPO / "assets-src/world/layout.materials.json"
BEGIN = "    // BEGIN GENERATED OUTDOOR STATIC MATERIALS (outdoor_static_materials.py)"
END = "    // END GENERATED OUTDOOR STATIC MATERIALS"
sys.path.insert(0, str(REPO / "tools/world"))
import layout_io  # noqa: E402
import outdoor_materials  # noqa: E402


@dataclass(frozen=True)
class Variant:
    material_id: str
    base_id: str
    tint: tuple[float, float, float] | None = None
    footstep: str | None = None


VARIANTS = (
    Variant("MAT_OUTDOOR_ASPHALT", "MAT_ASPHALT_01"),
    Variant("MAT_OUTDOOR_BLUESTONE", "MAT_BLUESTONE_PAVER"),
    Variant("MAT_OUTDOOR_BRICK", "MAT_BRICK_WATER_TABLE"),
    Variant("MAT_OUTDOOR_CONCRETE", "MAT_CONCRETE_BROOM"),
    Variant("MAT_OUTDOOR_DECK", "MAT_DECK_WOOD"),
    Variant("MAT_OUTDOOR_FENCE_BOARD", "MAT_SIDING_WARM_WHITE", (0.82, 0.77, 0.68)),
    Variant("MAT_OUTDOOR_FENCE_METAL", "MAT_METAL_GUTTER", (0.84, 0.85, 0.81)),
    Variant("MAT_OUTDOOR_GARDEN_WOOD", "MAT_DECK_WOOD", (0.68, 0.60, 0.47)),
    Variant("MAT_OUTDOOR_GRASS", "MAT_GROUND_LAWN"),
    Variant("MAT_OUTDOOR_GRAVEL", "MAT_GRAVEL_PATH"),
    Variant("MAT_OUTDOOR_LAWN_WORN", "MAT_GROUND_LAWN", (0.69, 0.68, 0.48)),
    Variant("MAT_OUTDOOR_MULCH", "MAT_SOIL_GARDEN", (0.54, 0.45, 0.35)),
    Variant("MAT_OUTDOOR_ROAD_GRATE", "MAT_METAL_GUTTER", (0.48, 0.49, 0.50)),
    Variant("MAT_OUTDOOR_ROAD_MARKING", "MAT_SOFFIT_WHITE", (0.94, 0.93, 0.84), "asphalt"),
    Variant("MAT_OUTDOOR_ROOF", "MAT_ROOF_SHINGLE"),
    Variant("MAT_OUTDOOR_SIDING", "MAT_SIDING_WARM_WHITE"),
    Variant("MAT_OUTDOOR_SOFFIT", "MAT_SOFFIT_WHITE"),
    Variant("MAT_OUTDOOR_SOIL", "MAT_SOIL_GARDEN"),
)


def load_rows() -> dict[str, dict]:
    document = layout_io.load_file(MATERIALS_FILE, "materials")
    return {row["id"]: row for row in document["materials"]}


def variant_row(variant: Variant, rows: dict[str, dict]) -> dict:
    row = copy.deepcopy(rows[variant.base_id])
    row["id"] = variant.material_id
    row["lightmapChannel"] = 0
    row["effectTierS"] = "Basic"
    if variant.tint is not None:
        row["tint"] = list(variant.tint)
    if variant.footstep is not None:
        row["footstepSurface"] = variant.footstep
    if variant.material_id == "MAT_OUTDOOR_ROAD_MARKING":
        row["snowResponse"] = {"coverable": False, "slopeLimitDeg": 0.0}
    return row


def expected_rows(rows: dict[str, dict]) -> dict[str, dict]:
    return {variant.material_id: variant_row(variant, rows) for variant in VARIANTS}


def problems(rows: dict[str, dict]) -> list[str]:
    issues = []
    ids = [variant.material_id for variant in VARIANTS]
    if len(ids) != len(set(ids)):
        issues.append("duplicate outdoor variant id")
    required = set(outdoor_materials.ROLE_IDS.values()) | set(outdoor_materials.UNBAKED_VARIANTS.values())
    if set(ids) != required:
        issues.append(f"variant ids {sorted(ids)} do not match source-role and unbaked-shell maps")
    for variant in VARIANTS:
        if variant.base_id not in rows:
            issues.append(f"{variant.material_id} has no canonical source row {variant.base_id}")
            continue
        if variant.tint is not None and any(channel < 0 or channel > 1 for channel in variant.tint):
            issues.append(f"{variant.material_id} has tint outside 0..1")
        if not rows[variant.base_id].get("albedo"):
            issues.append(f"{variant.material_id} source has no albedo")
    return issues


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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--check", action="store_true")
    group.add_argument("--write", action="store_true")
    args = parser.parse_args()
    rows = load_rows()
    issues = problems(rows)
    if issues:
        for issue in issues:
            print(f"outdoor_static_materials: {issue}", file=sys.stderr)
        return 1
    generated = expected_rows(rows)
    if args.check:
        for material_id, expected in generated.items():
            if rows.get(material_id) != expected:
                issues.append(f"{material_id} is missing or stale against its approved source")
        if issues:
            for issue in issues:
                print(f"outdoor_static_materials: {issue}", file=sys.stderr)
            return 1
        print(f"outdoor_static_materials: {len(generated)} stock-XNA outdoor variants clean")
        return 0

    source = MATERIALS_FILE.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        print("outdoor_static_materials: generated markers are missing or duplicated", file=sys.stderr)
        return 1
    before, remainder = source.split(BEGIN, 1)
    _, after = remainder.split(END, 1)
    rendered = "\n".join(render_row(generated[material_id]) for material_id in sorted(generated))
    MATERIALS_FILE.write_text(before + BEGIN + "\n" + rendered + "\n" + END + after,
                              encoding="utf-8")
    print(f"outdoor_static_materials: wrote {len(generated)} derived rows")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
