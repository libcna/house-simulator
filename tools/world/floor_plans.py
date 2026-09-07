#!/usr/bin/env python3
"""floor_plans.py -- a printable plan of each level, drawn from the layout.

`HOUSE-00398`. Six SVGs in `docs/floor-plans/`, one per level plus the site. They are for review:
the house is 96 cells and 179 portals of JSON, and the fastest way to see that a room is the wrong
shape or that a door opens into a wall is to look at it.

    tools/world/floor_plans.py --emit
    tools/world/floor_plans.py --check
    tools/world/floor_plans.py --selftest

## Drawn from the data, never beside it

Every line comes from `assets-src/world/`: cell boxes, portal rectangles, stair flights and the
plumbing chases. Nothing is positioned by hand, so a plan cannot drift from the house -- and the
`floor-plans` gate fails when the committed SVGs no longer match, which is the only way a generated
document stays true.

## Why SVG and not a raster

It prints, it diffs as text, and a reviewer can open it in a browser with no tooling. A PNG would
be a binary blob in the repository that nobody can review in a pull request.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import html
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_io  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "assets-src" / "world"
OUT_DIR = REPO / "docs" / "floor-plans"

SCALE = 26.0        # pixels per metre: a 45 m plot lands on a page at about 1 200 px
MARGIN = 46.0

#: Fill per cell kind, so a plan reads at a glance: circulation pale, service grey, outside green.
FILL = {
    "room": "#ffffff",
    "corridor": "#f4f1e8",
    "stair": "#e8eef6",
    "closet": "#efeceb",
    "garage": "#eceff1",
    "exterior": "#eef5e8",
    "void": "#f7f7f7",
}
PORTAL_COLOUR = {
    "door": "#b03a2e", "double_door": "#b03a2e", "exterior_door": "#7d2018",
    "slider": "#1f6f8b", "window": "#2e86c1", "cased_opening": "#7f8c8d",
    "stair_well": "#8e44ad", "hatch": "#8e44ad", "garage_door": "#7d2018",
}


def levels_of(layout):
    return list(layout_io.rows(layout, "levels")) if "levels" in layout else []


def bounds(cells):
    """The drawing extent, in metres, over every box of every cell."""
    xs = [value for cell in cells for box in cell["boxes"] for value in box["x"]]
    zs = [value for cell in cells for box in cell["boxes"] for value in box["z"]]
    return min(xs), max(xs), min(zs), max(zs)


def project(x, z, box, height):
    """World (x, z) to SVG (x, y). -Z is north (§14), so north is UP: y grows with z."""
    minX, _, minZ, _ = box
    return (MARGIN + (x - minX) * SCALE, MARGIN + (z - minZ) * SCALE)


def plan(level_id, cells, portals, flights, layout) -> str:
    """One level, as an SVG document."""
    box = bounds(cells)
    minX, maxX, minZ, maxZ = box
    width = (maxX - minX) * SCALE + 2 * MARGIN
    height = (maxZ - minZ) * SCALE + 2 * MARGIN + 34.0

    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width:.0f}" height="{height:.0f}" '
           f'viewBox="0 0 {width:.0f} {height:.0f}" font-family="DejaVu Sans, sans-serif">',
           '  <style>.id{font-size:9px;fill:#333}.name{font-size:8px;fill:#777}'
           '.key{font-size:10px;fill:#333}</style>',
           f'  <rect width="{width:.0f}" height="{height:.0f}" fill="#fbfbfa"/>']

    for cell in sorted(cells, key=lambda row: row["id"]):
        fill = FILL.get(cell.get("kind", "room"), "#ffffff")
        for index, cell_box in enumerate(cell["boxes"]):
            x0, y0 = project(cell_box["x"][0], cell_box["z"][0], box, height)
            x1, y1 = project(cell_box["x"][1], cell_box["z"][1], box, height)
            out.append(f'  <rect x="{min(x0, x1):.1f}" y="{min(y0, y1):.1f}" '
                       f'width="{abs(x1 - x0):.1f}" height="{abs(y1 - y0):.1f}" '
                       f'fill="{fill}" stroke="#2c3e50" stroke-width="1.4"/>')
            if index == 0:
                cx, cy = (min(x0, x1) + abs(x1 - x0) / 2, min(y0, y1) + abs(y1 - y0) / 2)
                out.append(f'  <text class="id" x="{cx:.1f}" y="{cy:.1f}" '
                           f'text-anchor="middle">{html.escape(cell["id"])}</text>')
                if cell.get("name"):
                    out.append(f'  <text class="name" x="{cx:.1f}" y="{cy + 10:.1f}" '
                               f'text-anchor="middle">{html.escape(cell["name"])}</text>')

    for portal in portals:
        colour = PORTAL_COLOUR.get(portal.get("kind"), "#000000")
        axis = portal["plane"]["axis"]
        value = float(portal["plane"]["value"])
        u0, u1 = portal["rect"]["u"]
        if axis == "x":
            a = project(value, u0, box, height)
            b = project(value, u1, box, height)
        elif axis == "z":
            a = project(u0, value, box, height)
            b = project(u1, value, box, height)
        else:
            # A horizontal portal is a hole in a floor: `u` is world X and `v` is world Z, so it
            # is drawn as the rectangle it is rather than as a line.
            v0, v1 = portal["rect"]["v"]
            x0, y0 = project(u0, v0, box, height)
            x1, y1 = project(u1, v1, box, height)
            out.append(f'  <rect x="{min(x0, x1):.1f}" y="{min(y0, y1):.1f}" '
                       f'width="{abs(x1 - x0):.1f}" height="{abs(y1 - y0):.1f}" fill="none" '
                       f'stroke="{colour}" stroke-width="1.6" stroke-dasharray="5 3"/>')
            continue
        out.append(f'  <line x1="{a[0]:.1f}" y1="{a[1]:.1f}" x2="{b[0]:.1f}" y2="{b[1]:.1f}" '
                   f'stroke="{colour}" stroke-width="3.2" stroke-linecap="round"/>')

    for flight in flights:
        out.append(f'  <!-- flight {flight["id"]}: {flight["risers"]} risers -->')

    # North arrow: §14 puts north at -Z, and -Z is up in this projection.
    out.append(f'  <g stroke="#2c3e50" stroke-width="1.4" fill="none">'
               f'<line x1="{width - 30:.0f}" y1="34" x2="{width - 30:.0f}" y2="8"/>'
               f'<path d="M{width - 34:.0f} 14 L{width - 30:.0f} 8 L{width - 26:.0f} 14"/></g>')
    out.append(f'  <text class="key" x="{width - 30:.0f}" y="46" text-anchor="middle">N</text>')

    # A scale bar: five metres, so a printed plan can be measured.
    bar = 5.0 * SCALE
    y = height - 18
    out.append(f'  <line x1="{MARGIN}" y1="{y}" x2="{MARGIN + bar:.1f}" y2="{y}" '
               f'stroke="#2c3e50" stroke-width="2"/>')
    out.append(f'  <text class="key" x="{MARGIN}" y="{y - 6}">5 m</text>')
    out.append(f'  <text class="key" x="{MARGIN + bar + 14:.1f}" y="{y + 4}">'
               f'{html.escape(level_id)} &#183; {len(cells)} cells &#183; {len(portals)} portals'
               f'</text>')

    out.append('</svg>')
    return "\n".join(out) + "\n"


def plans(directory: Path) -> dict[str, str]:
    layout = layout_io.load_layout(directory)
    if "cells" not in layout:
        return {}
    cells = list(layout_io.rows(layout, "cells"))
    portals = list(layout_io.rows(layout, "portals")) if "portals" in layout else []
    flights = list(layout_io.rows(layout, "stairs")) if "stairs" in layout else []
    by_cell = {cell["id"]: cell for cell in cells}

    out = {}
    for level in levels_of(layout):
        level_id = level["id"]
        members = [cell for cell in cells
                   if cell.get("level") == level_id and cell.get("kind") != "exterior"]
        if not members:
            continue
        ids = {cell["id"] for cell in members}
        drawn = [portal for portal in portals
                 if portal.get("cellA") in ids or portal.get("cellB") in ids]
        out[level_id] = plan(level_id, members, drawn,
                             [f for f in flights
                              if by_cell.get(f.get("fromCell"), {}).get("level") == level_id],
                             layout)

    # The site plan: every exterior cell, which is where the plot, the yards and the shed are.
    outside = [cell for cell in cells if cell.get("kind") == "exterior" and cell["id"] != "EXT_WORLD"]
    if outside:
        ids = {cell["id"] for cell in outside}
        out["SITE"] = plan("SITE", outside,
                           [p for p in portals
                            if p.get("cellA") in ids and p.get("cellB") in ids], [], layout)
    return out


# ======================================================================================= selftest


def selftest() -> int:
    import json
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("floor_plans: selftest")
    workspace = Path(tempfile.mkdtemp(prefix="floor_plans_selftest_"))
    try:
        world = workspace / "world"
        world.mkdir()
        (world / "layout.levels.json").write_text(json.dumps({
            "schema": "cna-house/levels/1",
            "levels": [{"id": "L0", "ffl": 0.60, "ceiling": 3.30}],
            "construction": {}}), encoding="utf-8")
        (world / "layout.cells.json").write_text(json.dumps({
            "schema": "cna-house/cells/1",
            "cells": [{"id": "L0_HALL", "level": "L0", "name": "Hall", "kind": "corridor",
                       "boxes": [{"x": [-2.0, 2.0], "z": [-10.0, 0.0]}]},
                      {"id": "L0_WC1", "level": "L0", "kind": "room",
                       "boxes": [{"x": [2.0, 4.0], "z": [-6.0, -4.0]}]},
                      {"id": "EXT_LAWN", "level": "L0", "kind": "exterior",
                       "boxes": [{"x": [-6.0, 6.0], "z": [0.0, 8.0]}],
                       "yOverride": [0.0, 20.0]}]}), encoding="utf-8")
        (world / "layout.portals.json").write_text(json.dumps({
            "schema": "cna-house/portals/1",
            "portals": [{"id": "P_HALL__WC1", "cellA": "L0_HALL", "cellB": "L0_WC1",
                         "plane": {"axis": "x", "value": 2.0},
                         "rect": {"u": [-5.5, -4.6], "v": [0.60, 2.65]}, "kind": "door"}]},
        ), encoding="utf-8")

        drawn = plans(world)
        require(set(drawn) == {"L0", "SITE"},
                f"a level with rooms and a site plan for the outside ({sorted(drawn)})")
        require("L0_HALL" in drawn["L0"] and "L0_WC1" in drawn["L0"],
                "every room on the level is labelled")
        require("EXT_LAWN" not in drawn["L0"] and "EXT_LAWN" in drawn["SITE"],
                "the outside is on the site plan and not on the floor plan")
        require(drawn["L0"].count("<rect") >= 3,
                f"a rect per box plus the page ({drawn['L0'].count('<rect')})")
        require('stroke="#b03a2e"' in drawn["L0"], "and the door is drawn, in the door colour")

        # 1. Deterministic: two runs give the same bytes, or `--check` is a gate that fails at
        #    random and somebody turns it off.
        require(plans(world) == drawn, "the same layout draws the same bytes")

        # 2. North is UP. §14 puts north at -Z, so a cell further north must have a SMALLER y.
        #    Getting this backwards is the one error in a plan that nobody notices and everybody
        #    acts on.
        north = json.loads((world / "layout.cells.json").read_text(encoding="utf-8"))
        north["cells"][1]["boxes"] = [{"x": [2.0, 4.0], "z": [-9.5, -8.5]}]
        (world / "layout.cells.json").write_text(json.dumps(north), encoding="utf-8")
        moved = plans(world)
        def wc_y(text):
            row = next(line for line in text.splitlines() if "L0_WC1" in line and "<text" in line)
            return float(row.split('y="')[1].split('"')[0])
        require(wc_y(moved["L0"]) < wc_y(drawn["L0"]),
                f"a room further north is drawn higher up ({wc_y(moved['L0']):.1f} vs "
                f"{wc_y(drawn['L0']):.1f})")
    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        print(f"\nfloor_plans: {len(failures)} claim(s) FAILED")
        return 1
    print("floor_plans: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", nargs="?", type=Path, default=SOURCE)
    parser.add_argument("--emit", action="store_true", help="write docs/floor-plans/*.svg")
    parser.add_argument("--check", action="store_true", help="fail if they are stale")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if not (args.directory / "layout.cells.json").is_file():
        print(f"floor_plans: no cells in {args.directory} yet -- nothing to draw.")
        return 0

    drawn = plans(args.directory)
    stale = []
    for level, text in sorted(drawn.items()):
        path = OUT_DIR / f"{level.lower()}.svg"
        if not path.is_file() or path.read_text(encoding="utf-8") != text:
            stale.append(path.name)
            if not args.check:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8")
    expected = {f"{level.lower()}.svg" for level in drawn}
    for orphan in sorted(OUT_DIR.glob("*.svg")) if OUT_DIR.is_dir() else []:
        if orphan.name not in expected:
            stale.append(f"{orphan.name} (no level draws it)")

    if args.check:
        if stale:
            print(f"floor_plans: stale -- {', '.join(stale)}. Run tools/world/floor_plans.py",
                  file=sys.stderr)
            return 1
        print(f"floor_plans: {len(drawn)} plan(s) match {args.directory}.")
        return 0

    print(f"floor_plans: {len(drawn)} plan(s) in {OUT_DIR.relative_to(REPO)}"
          + (f" -- {len(stale)} rewritten" if stale else " (already current)"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
