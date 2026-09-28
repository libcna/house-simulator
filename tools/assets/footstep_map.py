#!/usr/bin/env python3
"""Validate and document M8's six broad footstep categories.

The final scope reduction replaced the historical twenty bespoke surface sets with six reusable
NOX banks. The authored aliases live on those banks in ``layout.audio.json``; this gate proves
that every non-null cell, material and stair surface maps exactly once and that every selected
sample still comes directly from the intended retained NOX walk pack.

    tools/assets/footstep_map.py            # refresh the report
    tools/assets/footstep_map.py --check    # validate data and fail if the report is stale
    tools/assets/footstep_map.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import manifest as manifest_tool  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "world"))
import layout_io  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
WORLD = REPO / "assets-src" / "world"
REPORT = REPO / "docs" / "asset-selection" / "footstep-surfaces.md"

# Bank id -> (human category, direct NOX source category, exact authored aliases). Exactness is
# intentional: a new material spelling must make the gate fail until somebody makes the audible
# choice, rather than silently falling into a default.
CATEGORIES = {
    "BANK_FOOTSTEP_WOOD": (
        "wood", "footstep/wood/walk",
        {"hardwood", "painted_wood", "stair_wood", "stair_wood_open", "wood"}),
    "BANK_FOOTSTEP_CARPET": (
        "carpet", "footstep/sand/walk", {"carpet", "snow", "stair_carpet"}),
    "BANK_FOOTSTEP_TILE": (
        "tile", "footstep/tile/walk", {"ceramic", "glass", "metal", "tile", "vinyl", "water"}),
    "BANK_FOOTSTEP_STONE": (
        "stone/concrete", "footstep/rock/walk", {"asphalt", "bluestone", "concrete", "rock"}),
    "BANK_FOOTSTEP_GRAVEL": (
        "gravel", "footstep/gravel/walk", {"gravel", "mud", "soil"}),
    "BANK_FOOTSTEP_GRASS": (
        "grass", "footstep/grass/walk", {"grass"}),
}


def world_surfaces(layout: dict[str, dict]) -> dict[str, set[str]]:
    """Return every non-null surface spelling, grouped by the world table that authored it."""
    return {
        "cells": {row["footstepSurface"] for row in layout_io.rows(layout, "cells")
                  if row.get("footstepSurface")},
        "materials": {row["footstepSurface"] for row in layout_io.rows(layout, "materials")
                      if row.get("footstepSurface")},
        "stairs": {row["surface"] for row in layout_io.rows(layout, "stairs")
                   if row.get("surface")},
    }


def bank_rows(layout: dict[str, dict]) -> dict[str, dict]:
    rows = layout.get("audio", {}).get("banks", [])
    return layout_io.by_id(rows, "audio bank")


def validate(layout: dict[str, dict], manifest_rows: list[dict]) -> list[str]:
    errors: list[str] = []
    banks = bank_rows(layout)
    assets = {row.get("id"): row for row in manifest_rows}
    surfaces = world_surfaces(layout)
    required = set().union(*surfaces.values())
    mapped: dict[str, str] = {}

    for bank_id, (category, source, expected_aliases) in CATEGORIES.items():
        bank = banks.get(bank_id)
        if bank is None:
            errors.append(f"missing retained {category} bank {bank_id}")
            continue
        aliases = bank.get("surfaces")
        if not isinstance(aliases, list) or not aliases:
            errors.append(f"{bank_id} has no non-empty surfaces array")
            aliases = []
        alias_set = set(aliases)
        if len(alias_set) != len(aliases):
            errors.append(f"{bank_id} repeats a surface alias")
        if alias_set != expected_aliases:
            errors.append(
                f"{bank_id} aliases are {sorted(alias_set)}, expected {sorted(expected_aliases)}")
        for alias in aliases:
            previous = mapped.setdefault(alias, bank_id)
            if previous != bank_id:
                errors.append(f"surface {alias!r} maps to both {previous} and {bank_id}")

        samples = bank.get("samples")
        if not isinstance(samples, list) or len(samples) < 6:
            count = len(samples) if isinstance(samples, list) else 0
            errors.append(f"{bank_id} has {count} samples; need 6")
            continue
        for sample_id in samples:
            asset = assets.get(sample_id)
            if asset is None:
                errors.append(f"{bank_id} names missing manifest asset {sample_id}")
                continue
            actual = (asset.get("audio") or {}).get("noxCategory")
            if actual != source:
                errors.append(f"{bank_id} sample {sample_id} is {actual!r}, expected {source!r}")
            origin = asset.get("origin") or {}
            if origin.get("name") != "NOX Sound — Essentials Series SFX":
                errors.append(f"{bank_id} sample {sample_id} is not from the retained NOX collection")

    missing = required - set(mapped)
    extra = set(mapped) - required
    if missing:
        errors.append(f"world surfaces without a broad category: {sorted(missing)}")
    if extra:
        errors.append(f"mapped aliases not present in current world data: {sorted(extra)}")
    return errors


def document(layout: dict[str, dict]) -> str:
    banks = bank_rows(layout)
    grouped = world_surfaces(layout)
    all_surfaces = set().union(*grouped.values())
    lines = [
        "# Footstep surfaces — six retained categories",
        "",
        "*Generated by `tools/assets/footstep_map.py` (`HOUSE-01920`). Regenerate rather than edit.*",
        "",
        f"All **{len(all_surfaces)}** non-null surface spellings in the authored cells, materials and "
        "stairs map exactly once to the six active M8 banks. The samples are existing direct NOX "
        "selections; this mapping adds no sourced or offline-derived audio.",
        "",
        "| Category | Bank | World surface spellings | NOX walk pack | Samples | Gain |",
        "|---|---|---|---|---:|---:|",
    ]
    for bank_id, (category, source, _aliases) in CATEGORIES.items():
        bank = banks[bank_id]
        aliases = ", ".join(f"`{one}`" for one in sorted(bank["surfaces"]))
        lines.append(
            f"| {category} | `{bank_id}` | {aliases} | `{source}` | "
            f"{len(bank['samples'])} | {float(bank.get('gain', 1.0)):.2f} |")

    lines += [
        "",
        "The carpet row deliberately uses the softest retained NOX pack (`sand`) at the lowest "
        "bank gain. Material-only legacy spellings such as glass, water, wet soil and snow use the "
        "nearest broad retained category; they do not create a seventh set. Runtime collision "
        "surface names and authored stair spellings use the same table.",
        "",
        "## Coverage by world table",
        "",
        "| Table | Distinct spellings | Values |",
        "|---|---:|---|",
    ]
    for table in ("cells", "materials", "stairs"):
        values = ", ".join(f"`{one}`" for one in sorted(grouped[table]))
        lines.append(f"| {table} | {len(grouped[table])} | {values} |")
    lines.append("")
    return "\n".join(lines)


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("footstep_map: selftest")
    layout = layout_io.load_layout(WORLD, ["cells", "materials", "stairs", "audio"])
    manifest_rows = manifest_tool.load()["assets"]
    errors = validate(layout, manifest_rows)
    surfaces = world_surfaces(layout)
    require(len(CATEGORIES) == 6, "the active map has exactly six broad categories")
    require(len(set().union(*surfaces.values())) == 22,
            "the real authored world currently has 22 distinct non-null spellings")
    require(not errors, "the real world is covered exactly once by direct retained NOX banks")

    broken = {kind: dict(document) for kind, document in layout.items()}
    broken["audio"] = dict(layout["audio"])
    broken["audio"]["banks"] = [dict(row) for row in layout["audio"]["banks"]]
    wood = next(row for row in broken["audio"]["banks"] if row["id"] == "BANK_FOOTSTEP_WOOD")
    wood["surfaces"] = list(wood["surfaces"][:-1])
    broken_errors = validate(broken, manifest_rows)
    require(any("aliases are" in error or "without a broad category" in error
                for error in broken_errors),
            "removing one authored alias is rejected")

    if failures:
        return 1
    print("footstep_map: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail if data or report is stale")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    try:
        layout = layout_io.load_layout(WORLD, ["cells", "materials", "stairs", "audio"])
        manifest_rows = manifest_tool.load()["assets"]
    except (layout_io.LayoutError, OSError, ValueError) as error:
        print(f"footstep_map: {error}", file=sys.stderr)
        return 1

    errors = validate(layout, manifest_rows)
    if errors:
        for error in errors:
            print(f"footstep_map: {error}", file=sys.stderr)
        return 1
    text = document(layout)

    if args.check:
        if not REPORT.is_file() or REPORT.read_text(encoding="utf-8") != text:
            print(f"footstep_map: {REPORT.relative_to(REPO)} is stale; run "
                  "tools/assets/footstep_map.py", file=sys.stderr)
            return 1
        print("footstep_map: 22 world spellings map once to six direct NOX banks; report is current.")
        return 0

    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(text, encoding="utf-8")
    print(f"footstep_map: 22 world spellings map once to six direct NOX banks; wrote "
          f"{REPORT.relative_to(REPO)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
