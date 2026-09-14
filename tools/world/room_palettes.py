#!/usr/bin/env python3
"""Author and verify HOUSE-00908's palette for all 78 interior cells.

The assignments are offline authoring data.  ``--write`` places their four material ids directly
in ``layout.cells.json``; runtime code therefore reads the house rather than recreating these
choices.  The compact role names here make the family relationships reviewable while the JSON
remains the authoritative deployed result.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_io  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
CELLS_FILE = REPO / "assets-src" / "world" / "layout.cells.json"
MATERIALS_FILE = REPO / "assets-src" / "world" / "layout.materials.json"
REVIEW_FILE = REPO / "docs" / "asset-selection" / "room-palettes.md"

FLOORS = {
    "oak": "MAT_WOOD_OAK_FLOOR",
    "walnut": "MAT_WOOD_WALNUT_FLOOR",
    "carpet_beige": "MAT_CARPET_BEIGE",
    "carpet_grey": "MAT_CARPET_GREY",
    "carpet_brown": "MAT_CARPET_BROWN",
    "tile_grey": "MAT_TILE_PORCELAIN_GREY",
    "tile_warm": "MAT_TILE_CERAMIC_WARM",
    "tile_light": "MAT_TILE_CERAMIC_LIGHT",
    "tile_dark": "MAT_TILE_CERAMIC_DARK",
    "concrete": "MAT_FLOOR_CONCRETE",
    "stone": "MAT_FLOOR_STONE",
    "vinyl": "MAT_FLOOR_VINYL",
}
WALLS = {
    "warm_white": "MAT_PAINT_WARM_WHITE",
    "soft_white": "MAT_PAINT_SOFT_WHITE",
    "ivory": "MAT_PAINT_IVORY",
    "linen": "MAT_PAINT_LINEN",
    "soft_grey": "MAT_PAINT_SOFT_GREY",
    "sage": "MAT_PAINT_SAGE",
    "pale_sand": "MAT_PAINT_PALE_SAND",
    "pale_rose": "MAT_PAINT_PALE_ROSE",
    "dusty_blue": "MAT_PAINT_DUSTY_BLUE",
    "muted_teal": "MAT_PAINT_MUTED_TEAL",
    "aged": "MAT_PAINT_AGED_PLASTER",
    "ochre": "MAT_PAINT_SMOKY_OCHRE",
}
CEILINGS = {
    "flat": "MAT_PAINT_FLAT_WHITE",
    "warm": "MAT_PAINT_CEILING_WARM",
    "cool": "MAT_PAINT_CEILING_COOL",
    "cream": "MAT_PAINT_CEILING_CREAM",
    "moisture": "MAT_PAINT_CEILING_MOISTURE",
    "attic": "MAT_PAINT_CEILING_ATTIC",
}
TRIMS = {
    "painted": "MAT_DOOR_PAINTED",
    "hardwood": "MAT_DOOR_HARDWOOD",
    "ply": "MAT_HATCH_PLY",
}


@dataclass(frozen=True)
class Palette:
    floor: str
    wall: str
    ceiling: str
    trim: str = "painted"

    def ids(self) -> tuple[str, str, str, str]:
        return (FLOORS[self.floor], WALLS[self.wall], CEILINGS[self.ceiling], TRIMS[self.trim])


def p(floor: str, wall: str, ceiling: str, trim: str = "painted") -> Palette:
    return Palette(floor, wall, ceiling, trim)


# A restrained house-wide progression: warm public ground floor, quiet personalised bedrooms,
# slightly cooler upper floor, and deliberately worn service/attic spaces.  Repeated combinations
# across adjacent circulation and storage cells are intentional continuity, not missing choices.
PALETTES = {
    # B1 -- hard-wearing service shell with three deliberately finished destinations.
    "B1_STAIR": p("oak", "aged", "attic"),
    "B1_HALL": p("concrete", "aged", "attic"),
    "B1_MECHANICAL": p("concrete", "aged", "attic", "ply"),
    "B1_ELECTRICAL": p("concrete", "aged", "attic", "ply"),
    "B1_UTILITY": p("concrete", "aged", "attic", "ply"),
    "B1_CINEMA": p("carpet_brown", "ochre", "attic", "hardwood"),
    "B1_WC7": p("tile_light", "soft_grey", "moisture"),
    "B1_GYM": p("concrete", "soft_grey", "cool"),
    "B1_WORKSHOP": p("concrete", "aged", "attic", "ply"),
    "B1_STOR1": p("concrete", "aged", "attic", "ply"),
    "B1_STOR2": p("concrete", "aged", "attic", "ply"),
    "B1_HOBBY": p("carpet_beige", "dusty_blue", "warm"),
    "B1_CELLAR": p("stone", "ochre", "attic", "hardwood"),
    "B1_LAUNDRY2": p("tile_grey", "soft_grey", "moisture"),
    "B1_UNDERSTAIR": p("concrete", "aged", "attic", "ply"),

    # L0 -- the warm family/public palette; walnut and hardwood mark the formal west rooms.
    "L0_FOYER": p("stone", "ivory", "warm", "hardwood"),
    "L0_HALL": p("oak", "warm_white", "warm"),
    "L0_STAIR_MAIN": p("oak", "warm_white", "warm"),
    "L0_MUDROOM": p("tile_warm", "sage", "moisture"),
    "L0_LAUNDRY": p("tile_grey", "soft_grey", "moisture"),
    "L0_WC1": p("tile_light", "soft_grey", "moisture"),
    "L0_FAMILY": p("oak", "warm_white", "warm"),
    "L0_LIVING": p("walnut", "ivory", "cream", "hardwood"),
    "L0_OFFICE": p("carpet_brown", "dusty_blue", "warm", "hardwood"),
    "L0_WC2": p("tile_warm", "sage", "moisture"),
    "L0_CLOSET_W": p("carpet_brown", "dusty_blue", "warm", "hardwood"),
    "L0_DINING": p("walnut", "linen", "cream", "hardwood"),
    "L0_STOR": p("oak", "warm_white", "warm"),
    "L0_BUTLERS": p("tile_warm", "linen", "warm", "hardwood"),
    "L0_PANTRY": p("vinyl", "warm_white", "warm"),
    "L0_KITCHEN": p("tile_grey", "warm_white", "warm"),
    "L0_SUNROOM": p("stone", "sage", "warm"),
    "L0_GARAGE": p("concrete", "aged", "attic", "ply"),
    "L0_GARAGE_LOFT": p("oak", "aged", "attic", "ply"),
    "CELL_FRIDGE_INTERIOR": p("tile_light", "soft_white", "moisture"),
    "CELL_FREEZER_INTERIOR": p("tile_light", "soft_white", "moisture"),

    # L1 -- warm circulation with a small, related set of bedroom identities.
    "L1_LANDING": p("oak", "warm_white", "warm"),
    "L1_STAIR_MAIN": p("oak", "warm_white", "warm"),
    "L1_HALL": p("carpet_beige", "warm_white", "warm"),
    "L1_HALL_W": p("carpet_beige", "warm_white", "warm"),
    "L1_MASTER_BED": p("carpet_beige", "linen", "cream", "hardwood"),
    "L1_MASTER_BATH": p("stone", "soft_grey", "moisture"),
    "L1_MASTER_CLOSET": p("carpet_beige", "linen", "cream", "hardwood"),
    "L1_BED2": p("carpet_grey", "sage", "warm"),
    "L1_LINEN": p("oak", "warm_white", "warm"),
    "L1_STOR": p("oak", "warm_white", "warm"),
    "L1_BED3": p("carpet_beige", "pale_rose", "warm"),
    "L1_BED4": p("carpet_grey", "muted_teal", "cool"),
    "L1_BATH2": p("tile_warm", "soft_grey", "moisture"),
    "L1_WC3": p("tile_light", "soft_grey", "moisture"),
    "L1_CLOSET_2": p("carpet_beige", "warm_white", "warm"),
    "L1_CLOSET_3": p("carpet_beige", "warm_white", "warm"),
    "L1_BED5": p("carpet_beige", "pale_sand", "cream"),
    "L1_BATH3": p("tile_grey", "soft_grey", "moisture"),
    "L1_WC4": p("tile_light", "sage", "moisture"),

    # L2 -- the same family shifted cooler; library joinery echoes the formal rooms below.
    "L2_LANDING": p("oak", "soft_white", "cool"),
    "L2_STAIR_MAIN": p("oak", "soft_white", "cool"),
    "L2_STAIR_ATTIC": p("oak", "aged", "attic", "ply"),
    "L2_HALL": p("carpet_grey", "soft_white", "cool"),
    "L2_HALL_W": p("carpet_grey", "soft_white", "cool"),
    "L2_LIBRARY": p("walnut", "dusty_blue", "cool", "hardwood"),
    "L2_BED6": p("carpet_beige", "sage", "cool"),
    "L2_BATH4": p("tile_grey", "soft_grey", "moisture"),
    "L2_CLOSET_4": p("carpet_grey", "soft_white", "cool"),
    "L2_WC5": p("tile_light", "soft_grey", "moisture"),
    "L2_LINEN2": p("oak", "soft_white", "cool"),
    "L2_BED7": p("carpet_brown", "pale_sand", "cream"),
    "L2_GAMES": p("oak", "muted_teal", "cool"),
    "L2_SITTING": p("carpet_grey", "linen", "cool"),
    "L2_STOR2": p("oak", "soft_white", "cool"),
    "L2_BATH5": p("tile_dark", "soft_grey", "moisture"),
    "L2_WC6": p("tile_light", "sage", "moisture"),

    # L3 -- one maintained room surrounded by visibly older, utilitarian roof-space.
    "L3_STAIR_HEAD": p("oak", "aged", "attic", "ply"),
    "L3_ROOM": p("oak", "warm_white", "attic", "hardwood"),
    "L3_STORE_W": p("oak", "aged", "attic", "ply"),
    "L3_STORE_E": p("oak", "aged", "attic", "ply"),
    "L3_STORE_N": p("oak", "aged", "attic", "ply"),
    "L3_STORE_S": p("oak", "aged", "attic", "ply"),
}

ID_LINE = re.compile(r'^\s*"id":\s*"([A-Z0-9_]+)"')
PALETTE_LINE = re.compile(
    r'^\s*"(?:floorMaterial|wallMaterial|ceilingMaterial|trimMaterial)"\s*:')


def rewritten(text: str) -> str:
    """Replace only the four generated palette lines while retaining every authored comment."""
    output: list[str] = []
    current: str | None = None
    inserted: set[str] = set()
    for line in text.splitlines():
        match = ID_LINE.match(line)
        if match:
            current = match.group(1)
        if current in PALETTES and PALETTE_LINE.match(line):
            continue
        if current in PALETTES and '"footstepSurface"' in line:
            floor, wall, ceiling, trim = PALETTES[current].ids()
            output.append(f'      "floorMaterial": "{floor}", "wallMaterial": "{wall}",')
            output.append(f'      "ceilingMaterial": "{ceiling}", "trimMaterial": "{trim}",')
            inserted.add(current)
        output.append(line)
    missing = set(PALETTES) - inserted
    if missing:
        raise RuntimeError(f"no footstepSurface insertion point for {sorted(missing)}")
    return "\n".join(output) + "\n"


def review(cells: dict[str, dict]) -> str:
    lines = [
        "# Room palettes",
        "",
        "`HOUSE-00908` assigns the four architectural finishes of every non-exterior cell. The",
        "repeated warm ground-floor, bedroom and cool upper-floor families are deliberate; service",
        "and roof-space wear is confined to the rooms whose schedule calls for it.",
        "",
        "| Cell | Floor | Wall | Ceiling | Trim |",
        "|---|---|---|---|---|",
    ]
    for identifier, palette in PALETTES.items():
        floor, wall, ceiling, trim = palette.ids()
        name = cells[identifier].get("name", identifier)
        lines.append(
            f"| `{identifier}` — {name} | `{floor}` | `{wall}` | `{ceiling}` | `{trim}` |")
    lines.extend([
        "",
        "All wall and ceiling choices use the lightmapped `DualTexture` path. Trim uses only",
        "non-lightmapped `Basic` joinery finishes, so `HOUSE-00907` can replace the generated",
        "shell slots without changing the declared vertex layout. The two appliance sub-cells are",
        "included in the 78 and deliberately share one clean, moisture-resistant interior palette.",
        "",
    ])
    return "\n".join(lines)


def problems() -> list[str]:
    errors: list[str] = []
    cells_doc = layout_io.load_file(CELLS_FILE, "cells")
    materials_doc = layout_io.load_file(MATERIALS_FILE, "materials")
    cells = {row["id"]: row for row in cells_doc["cells"]}
    materials = {row["id"]: row for row in materials_doc["materials"]}
    interiors = {identifier for identifier, row in cells.items() if row.get("kind") != "exterior"}
    if len(interiors) != 78:
        errors.append(f"world has {len(interiors)} non-exterior cells, expected 78")
    if set(PALETTES) != interiors:
        errors.append(
            f"assignment set differs: missing {sorted(interiors - set(PALETTES))}, "
            f"extra {sorted(set(PALETTES) - interiors)}")

    for identifier, palette in PALETTES.items():
        row = cells.get(identifier, {})
        actual = tuple(row.get(field) for field in
                       ("floorMaterial", "wallMaterial", "ceilingMaterial", "trimMaterial"))
        expected = palette.ids()
        if actual != expected:
            errors.append(f"{identifier} palette {actual}, expected {expected}")
            continue
        for material_id in actual:
            if material_id not in materials:
                errors.append(f"{identifier} names missing material {material_id}")
        floor, wall, ceiling, trim = (materials[value] for value in actual)
        if floor["id"] not in set(FLOORS.values()) or floor.get("effectTierS") != "DualTexture":
            errors.append(f"{identifier} floor is not a HOUSE-00902 lightmapped floor")
        for role, material in (("wall", wall), ("ceiling", ceiling)):
            if material.get("class") != "paint" or material.get("effectTierS") != "DualTexture":
                errors.append(f"{identifier} {role} is not lightmapped paint")
        if trim["id"] not in set(TRIMS.values()) or trim.get("effectTierS") != "Basic":
            errors.append(f"{identifier} trim is not a non-lightmapped joinery finish")

    # Measurable cohesion invariants around the subjective acceptance sentence.
    for identifiers in (
        ("L0_HALL", "L0_STAIR_MAIN", "L0_FAMILY", "L0_STOR"),
        ("L1_LANDING", "L1_STAIR_MAIN"),
        ("L1_HALL", "L1_HALL_W", "L1_CLOSET_2", "L1_CLOSET_3"),
        ("L2_LANDING", "L2_STAIR_MAIN"),
        ("L2_HALL", "L2_HALL_W", "L2_CLOSET_4"),
        ("CELL_FRIDGE_INTERIOR", "CELL_FREEZER_INTERIOR"),
        ("L3_STORE_W", "L3_STORE_E", "L3_STORE_N", "L3_STORE_S"),
    ):
        values = {PALETTES[identifier] for identifier in identifiers}
        if len(values) != 1:
            errors.append(f"connected family {identifiers} does not share one palette")

    expected_review = review(cells)
    if not REVIEW_FILE.is_file() or REVIEW_FILE.read_text(encoding="utf-8") != expected_review:
        errors.append(f"{REVIEW_FILE.relative_to(REPO)} is stale; run --write")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        assert len(PALETTES) == 78
        original = CELLS_FILE.read_text(encoding="utf-8")
        assert rewritten(rewritten(original)) == rewritten(original)
        print("room_palettes: selftest passed")
    if args.write:
        CELLS_FILE.write_text(rewritten(CELLS_FILE.read_text(encoding="utf-8")), encoding="utf-8")
        cells = {row["id"]: row for row in layout_io.load_file(CELLS_FILE, "cells")["cells"]}
        REVIEW_FILE.parent.mkdir(parents=True, exist_ok=True)
        REVIEW_FILE.write_text(review(cells), encoding="utf-8")
    if args.check:
        errors = problems()
        if errors:
            for error in errors:
                print(f"room_palettes: {error}", file=sys.stderr)
            return 1
        print("room_palettes: 78 complete cell palettes form seven coherent room families")
    if not (args.write or args.check or args.selftest):
        parser.error("choose --write, --check or --selftest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
