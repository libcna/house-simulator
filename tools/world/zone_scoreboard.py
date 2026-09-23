#!/usr/bin/env python3
"""Measure breadth-first completion inputs for every CNA House zone.

``docs/zones.json`` is the single assignment of authored cells to the eleven planning zones.  This
tool joins that manifest to the world data and reports the cheap, objective inputs used beside the
human-reviewed C3/C4/C5 levels in ``plan.md``: accessible cells, props, empty cells, lighting,
review poses, and the main/hero target sets.

    tools/world/zone_scoreboard.py
    tools/world/zone_scoreboard.py --check
    tools/world/zone_scoreboard.py --zones path/to/zones.json --check

``--check`` is deliberately a static CI gate.  It rejects a source cell assigned to zero or two
places, unknown cells, malformed tiers, and inconsistent hero metadata without changing files.
Offline tooling is not runtime code and is not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import collections
import json
import math
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_io  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "assets-src" / "world"
ZONES = REPO / "docs" / "zones.json"
TIERS = {"H", "M", "S", "U"}
# HOUSE-03206 retired H4 (master bedroom) and H7 (attic room); hero ids are permanent, never reused.
HERO_AREAS = {"H1", "H2", "H3", "H5", "H6"}
HERO_CELLS = 11
MAIN_CELLS = 12
ZONE_ID = re.compile(r"^Z-[A-Z0-9]+$")
EXPECTED_EXCLUSIONS = {
    "EXT_WORLD", "EXT_NORTHSTRIP", "L2_BALCONY_JULIET",
    "CELL_FRIDGE_INTERIOR", "CELL_FREEZER_INTERIOR",
}


class ZoneError(Exception):
    """The zone manifest is malformed or disagrees with the authored world."""


def _accessibility_problems(row: dict, where: str) -> list[str]:
    """Validate the manifest half of HOUSE-03225; collision is validate_world rule 15."""
    accessible = row.get("accessible")
    if not isinstance(accessible, bool):
        return [f"{where}: accessible must be true or false"]
    point = row.get("standingPoint")
    posture = row.get("standingPosture", "standing")
    if accessible:
        if (not isinstance(point, list) or len(point) != 3
                or any(isinstance(value, bool) or not isinstance(value, (int, float))
                       or not math.isfinite(value) for value in point)):
            return [f"{where}: an accessible cell needs a finite [x, y, z] standingPoint"]
        if posture not in ("standing", "crouched"):
            return [f"{where}: standingPosture must be standing or crouched"]
        return []
    found = []
    if not isinstance(row.get("reason"), str) or not row["reason"].strip():
        found.append(f"{where}: a non-accessible cell needs a non-empty reason")
    if "standingPoint" in row or "standingPosture" in row:
        found.append(f"{where}: a non-accessible cell must not have a standing point")
    return found


def load_zones(path: Path) -> dict:
    """Read the strict-JSON planning manifest with actionable parse errors."""
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise ZoneError(f"{path}: {exc.strerror or exc}") from exc
    except json.JSONDecodeError as exc:
        raise ZoneError(f"{path}:{exc.lineno}:{exc.colno}: {exc.msg}") from exc
    if not isinstance(document, dict):
        raise ZoneError(f"{path}: top level is not an object")
    if document.get("schema") != "cna-house/zones/1":
        raise ZoneError(f"{path}: schema must be 'cna-house/zones/1'")
    return document


def _rows(document: dict) -> tuple[list[dict], list[dict]]:
    zones = document.get("zones")
    none = document.get("none")
    if not isinstance(zones, list) or not isinstance(none, list):
        raise ZoneError("zones and none must both be arrays")
    return zones, none


def problems(document: dict, source_cells: set[str]) -> list[str]:
    """Return every manifest error; do not stop after the first missing assignment."""
    try:
        zones, none = _rows(document)
    except ZoneError as exc:
        return [str(exc)]

    found: list[str] = []
    assignments: dict[str, list[str]] = collections.defaultdict(list)
    poses: dict[str, list[str]] = collections.defaultdict(list)
    zone_ids: set[str] = set()
    hero_cells = 0
    main_cells = 0
    excluded: set[str] = set()

    if len(zones) != 11:
        found.append(f"zones has {len(zones)} entries; the plan defines 11")

    for zone_index, zone in enumerate(zones):
        where = f"zones[{zone_index}]"
        if not isinstance(zone, dict):
            found.append(f"{where} is not an object")
            continue
        zone_id = zone.get("id")
        if not isinstance(zone_id, str) or not ZONE_ID.fullmatch(zone_id):
            found.append(f"{where}.id must match Z-[A-Z0-9]+")
            zone_id = where
        elif zone_id in zone_ids:
            found.append(f"zone id {zone_id} appears twice")
        zone_ids.add(zone_id)
        if not isinstance(zone.get("name"), str) or not zone["name"].strip():
            found.append(f"{zone_id}: name must be a non-empty string")

        cells = zone.get("cells")
        if not isinstance(cells, list) or not cells:
            found.append(f"{zone_id}: cells must be a non-empty array")
            cells = []
        for row_index, row in enumerate(cells):
            cell_where = f"{zone_id}.cells[{row_index}]"
            if not isinstance(row, dict):
                found.append(f"{cell_where} is not an object")
                continue
            cell_id = row.get("id")
            if not isinstance(cell_id, str):
                found.append(f"{cell_where}.id must be a string")
                continue
            assignments[cell_id].append(zone_id)
            accessible = row.get("accessible")
            tier = row.get("tier")
            found.extend(_accessibility_problems(row, f"{zone_id}/{cell_id}"))
            if accessible is False:
                excluded.add(cell_id)
            if tier not in TIERS and tier is not None:
                found.append(f"{zone_id}/{cell_id}: tier must be H, M, S, U, or null")
            if accessible and tier not in TIERS:
                found.append(f"{zone_id}/{cell_id}: an accessible cell needs a quality tier")
            hero = row.get("heroArea")
            if tier == "H":
                hero_cells += 1
                if hero not in HERO_AREAS:
                    found.append(f"{zone_id}/{cell_id}: tier H needs a heroArea in "
                                 f"{', '.join(sorted(HERO_AREAS))}")
            elif hero is not None:
                found.append(f"{zone_id}/{cell_id}: only tier H may name a heroArea")
            if tier == "M":
                main_cells += 1

        review_poses = zone.get("reviewPoses")
        if not isinstance(review_poses, list):
            found.append(f"{zone_id}: reviewPoses must be an array")
            review_poses = []
        for pose in review_poses:
            if not isinstance(pose, str) or not pose:
                found.append(f"{zone_id}: every review pose must be a non-empty string")
            else:
                poses[pose].append(zone_id)

    for row_index, row in enumerate(none):
        where = f"none[{row_index}]"
        if not isinstance(row, dict):
            found.append(f"{where} is not an object")
            continue
        cell_id = row.get("id")
        if not isinstance(cell_id, str):
            found.append(f"{where}.id must be a string")
            continue
        assignments[cell_id].append("none")
        excluded.add(cell_id)
        found.extend(_accessibility_problems(row, f"none/{cell_id}"))
        if row.get("accessible") is not False:
            found.append(f"none/{cell_id}: accessible must be false")
        if row.get("tier") is not None:
            found.append(f"none/{cell_id}: tier must be null")

    for cell_id in sorted(source_cells | set(assignments)):
        places = assignments.get(cell_id, [])
        if cell_id not in source_cells:
            found.append(f"{cell_id} is assigned to {', '.join(places)} but is not a source cell")
        elif not places:
            found.append(f"{cell_id} is a source cell assigned to zero zones")
        elif len(places) > 1:
            found.append(f"{cell_id} is assigned more than once: {', '.join(places)}")

    for pose, owners in sorted(poses.items()):
        if len(owners) > 1:
            found.append(f"review pose {pose!r} is assigned more than once: {', '.join(owners)}")
    if excluded != EXPECTED_EXCLUSIONS:
        found.append("accessibility exclusions must be exactly "
                     f"{', '.join(sorted(EXPECTED_EXCLUSIONS))}; got "
                     f"{', '.join(sorted(excluded))}")
    if hero_cells != HERO_CELLS:
        found.append(f"manifest has {hero_cells} tier-H cells; the approved plan defines {HERO_CELLS}")
    if main_cells != MAIN_CELLS:
        found.append(f"manifest has {main_cells} tier-M cells; the approved plan defines {MAIN_CELLS}")
    used_heroes = {
        row.get("heroArea")
        for zone in zones if isinstance(zone, dict)
        for row in zone.get("cells", []) if isinstance(row, dict) and row.get("tier") == "H"
    }
    if used_heroes != HERO_AREAS:
        found.append(f"heroArea coverage must be exactly {', '.join(sorted(HERO_AREAS))}")
    return found


def measure(document: dict, world: Path) -> list[dict]:
    """Join zone metadata to authored props and light groups."""
    layout = layout_io.load_layout(world, ["cells", "props", "lights"])
    props = collections.Counter(row.get("cell") for row in layout_io.rows(layout, "props"))
    light_groups: dict[str, set[str]] = collections.defaultdict(set)
    for light in layout_io.rows(layout, "lights"):
        if isinstance(light.get("cell"), str) and isinstance(light.get("group"), str):
            light_groups[light["cell"]].add(light["group"])

    zones, _ = _rows(document)
    out = []
    for zone in zones:
        members = zone["cells"]
        accessible = [row for row in members if row.get("accessible") is True]
        accessible_ids = [row["id"] for row in accessible]
        prop_count = sum(props[cell_id] for cell_id in accessible_ids)
        zero_props = [cell_id for cell_id in accessible_ids if props[cell_id] == 0]
        lit = [cell_id for cell_id in accessible_ids if light_groups[cell_id]]
        main = [row["id"] for row in members if row.get("tier") == "M"]
        hero = [row["id"] for row in members if row.get("tier") == "H"]
        out.append({
            "id": zone["id"],
            "cells": len(members),
            "accessible": len(accessible),
            "props": prop_count,
            "propsPerAccessible": prop_count / len(accessible) if accessible else None,
            "zeroProps": zero_props,
            "lit": lit,
            "reviewPoses": list(zone["reviewPoses"]),
            "main": main,
            "hero": hero,
        })
    return out


def markdown(rows: list[dict]) -> str:
    """Render a stable table suitable for pasting beside the plan scoreboard."""
    lines = [
        "| Zone | Cells | Accessible | Props | Props/access. | Zero-prop accessible | "
        "Lit accessible | Review poses | C4 main targets | C5 hero targets |",
        "|---|---:|---:|---:|---:|---|---|---:|---|---|",
    ]
    for row in rows:
        ratio = "—" if row["propsPerAccessible"] is None else f"{row['propsPerAccessible']:.2f}"
        lines.append(
            f"| `{row['id']}` | {row['cells']} | {row['accessible']} | {row['props']} | {ratio} | "
            f"{len(row['zeroProps'])} | {len(row['lit'])} / {row['accessible']} | "
            f"{len(row['reviewPoses'])} | {len(row['main'])} | "
            f"{len(row['hero'])} |"
        )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("world", nargs="?", type=Path, default=SOURCE,
                        help="world source directory (default: assets-src/world)")
    parser.add_argument("--zones", type=Path, default=ZONES,
                        help="zone manifest (default: docs/zones.json)")
    parser.add_argument("--check", action="store_true",
                        help="validate membership and metadata; write nothing")
    args = parser.parse_args()
    try:
        document = load_zones(args.zones)
        layout = layout_io.load_layout(args.world, ["cells"])
        source_cells = {row["id"] for row in layout_io.rows(layout, "cells")}
        found = problems(document, source_cells)
        if found:
            for problem in found:
                print(f"zone_scoreboard: {problem}", file=sys.stderr)
            return 1
        if args.check:
            zones, none = _rows(document)
            print(f"zone_scoreboard: {len(source_cells)} cells assigned exactly once "
                  f"across {len(zones)} zones and {len(none)} explicit none entries")
            return 0
        print(markdown(measure(document, args.world)))
        return 0
    except (ZoneError, layout_io.LayoutError) as exc:
        print(f"zone_scoreboard: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
