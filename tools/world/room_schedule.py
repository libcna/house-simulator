#!/usr/bin/env python3
"""room_schedule.py -- §13's room schedule, measured from the layout.

`HOUSE-00400`. §13.2-§13.7 tabulate every cell in the house with its extent, floor area, light
groups, windows and doors. They were written before `assets-src/world/` existed and have been
maintained by hand ever since, which is the arrangement §12.6's window table was in when
`HOUSE-00376` found it fifteen windows out.

    tools/world/room_schedule.py --check
    tools/world/room_schedule.py --emit      # rewrite the numeric columns from the layout
    tools/world/room_schedule.py --selftest

## What is measured, and what is left alone

The four numeric columns -- `Area`, `Lights`, `Win`, `Doors` -- and the per-section row count.
The `X`/`Z` extents are compared but never rewritten: an extent that disagrees is a question about
which is right, and this tool cannot answer it. `Name` and `Notes` are prose and are not touched.

## The definitions, which §13.1 now states because this tool has to pick one

* **Area** is the sum of the cell's boxes, not the area of their bounding rectangle. An L-shaped
  cell is the L, not the rectangle around it -- `B1_UNDERSTAIR` is 4.68 m² inside a 7.56 m² AABB,
  and the table said 7.6 for a year after a note in the same section had already worked out 4.68.
* **Lights** is the number of distinct switch groups among the cell's fixtures. §13.1 already said
  "groups, not bulbs" and the layout agreed everywhere, which is why this column needed no
  corrections.
* **Win** and **Doors** count the openings on the cell's **boundary** -- every sash and every leaf
  you can see from inside the room. A door between the hall and the WC therefore counts for both,
  which is the only definition a reader can check by standing in the room. The columns used to
  hold a mixture of counts, attributions and prose ("2 sidelights + transom", "3 (sectional, side,
  house)", and a central hall with three doorways written as `0`), and no single rule reproduced
  them: counting by the leaf's `swing` cell matched 45 rows and counting the boundary matched 29.
  The descriptions moved to `Notes`, where prose belongs.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import collections
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_io  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "assets-src" / "world"
DOCUMENT = REPO / "cna-house.md"

SECTION = re.compile(r"^### (13\.[2-7]) ")
HEADING = re.compile(r"^\| ID \|")
ROW = re.compile(r"^\| `([A-Z][A-Z0-9_]*)`\s*∪?\s*\|")

#: The columns this tool owns. Everything else in a row is prose and is copied through.
MEASURED = ("Area", "Lights", "Win", "Doors")


def measure(directory: Path) -> dict[str, dict[str, float]]:
    """Every cell's `Area`, `Lights`, `Win` and `Doors`, from the layout alone."""
    layout = layout_io.load_layout(directory)
    cells = {row["id"]: row for row in layout_io.rows(layout, "cells")}
    portals = {row["id"]: row for row in layout_io.rows(layout, "portals")}

    windows: collections.Counter = collections.Counter()
    doors: collections.Counter = collections.Counter()
    for opening in layout_io.rows(layout, "openings"):
        portal = portals.get(opening.get("portal"))
        if portal is None:
            continue
        counter = {"window": windows, "door": doors}.get(opening.get("kind"))
        if counter is None:
            continue
        # BOTH sides. A leaf is on the boundary of two cells and is visible from each of them.
        for side in ("cellA", "cellB"):
            if portal.get(side) in cells:
                counter[portal[side]] += 1

    groups: dict[str, set] = collections.defaultdict(set)
    for light in layout_io.rows(layout, "lights"):
        if light.get("cell") in cells:
            groups[light["cell"]].add(light.get("group"))

    out = {}
    for identifier, cell in cells.items():
        boxes = cell.get("boxes") or []
        area = sum((box["x"][1] - box["x"][0]) * (box["z"][1] - box["z"][0]) for box in boxes)
        out[identifier] = {"Area": round(area, 1),
                           "Lights": len(groups[identifier]),
                           "Win": windows[identifier],
                           "Doors": doors[identifier],
                           "boxes": len(boxes)}
    return out


def render(column: str, value: float) -> str:
    """How a measured value is written in §13: one decimal for an area, an em dash for no count."""
    if column == "Area":
        return f"{value:.1f}"
    return "—" if value == 0 else f"{value:g}"


def parse_range(text: str) -> tuple[float, float] | None:
    cleaned = text.replace("−", "-").replace("–", "-").replace("…", "...").strip()
    match = re.match(r"^([+-]?\d+(?:\.\d+)?)\s*\.\.\.\s*([+-]?\d+(?:\.\d+)?)$", cleaned)
    return (float(match.group(1)), float(match.group(2))) if match else None


def stated(cell: str) -> float | None:
    """The leading number of a table cell, or None when it states none."""
    match = re.match(r"^\s*([+-]?\d+(?:\.\d+)?)", cell.replace("−", "-"))
    return float(match.group(1)) if match else None


def sections(lines: list[str]):
    """`(section, header, index, columns)` for every tabulated cell row in §13.2-§13.7."""
    section = None
    header: list[str] | None = None
    for index, line in enumerate(lines):
        found = SECTION.match(line)
        if found:
            section, header = found.group(1), None
            continue
        if line.startswith("### 13.8"):
            section = None
        if section and HEADING.match(line):
            header = [name.strip() for name in line.strip("|").split("|")]
            continue
        if section and header and ROW.match(line):
            columns = [value.strip() for value in line.strip("|").split("|")]
            if len(columns) == len(header):
                yield section, header, index, columns


def problems(directory: Path, lines: list[str]) -> tuple[list[str], dict[int, str]]:
    """`(problems, rewritten_lines_by_index)`."""
    measured = measure(directory)
    found: list[str] = []
    rewrites: dict[int, str] = {}
    tabulated: set[str] = set()

    for section, header, index, columns in sections(lines):
        row = dict(zip(header, columns))
        identifier = row["ID"].strip().strip("∪").strip().strip("`")
        tabulated.add(identifier)
        if identifier not in measured:
            found.append(f"§{section}: {identifier} is tabulated and is not in the layout")
            continue
        truth = measured[identifier]

        changed = dict(row)
        # §13.1: "a cell may be a union of boxes (marked ∪)". The convention was stated and never
        # applied, and `B1_UNDERSTAIR` -- the one row where the union and its bounding rectangle
        # differ by 2.9 m² -- is exactly the row that carried the bounding rectangle's area for a
        # year. The marking is the reader's warning that the extent is an envelope.
        union = truth["boxes"] > 1
        wanted_id = f"`{identifier}` ∪" if union else f"`{identifier}`"
        if row["ID"].strip() != wanted_id:
            found.append(f"§{section}: {identifier} is "
                         + ("a union of boxes and is not marked ∪"
                            if union else "one box and is marked ∪"))
            changed["ID"] = wanted_id

        for column in MEASURED:
            if column not in header:
                continue
            was = stated(row[column])
            now = truth[column]
            canonical = render(column, now)
            if row[column].strip() == canonical:
                continue
            if was is None or abs(was - now) >= (0.05 if column == "Area" else 0.5):
                found.append(f"§{section}: {identifier} {column} says "
                             f"{row[column] or '—'!r} and the layout measures {canonical}")
            else:
                # The number is right and the cell is not just the number. These columns are
                # counts (§13.1); a description belongs in `Notes`, where it cannot be mistaken
                # for arithmetic.
                found.append(f"§{section}: {identifier} {column} is {row[column]!r}, which is "
                             f"the right count wrapped in prose; move the prose to Notes")
            changed[column] = canonical
        if changed != row:
            rewrites[index] = "| " + " | ".join(changed[name] for name in header) + " |"

    # A cell the schedule does not list. Nested sub-cells are exempt: §13 is a schedule of ROOMS,
    # and the interior of a refrigerator is not one. §13.7 counts them separately and says so.
    nested = {row["id"] for row in layout_io.rows(layout_io.load_layout(directory), "cells")
              if row.get("parent")}
    for identifier in sorted(set(measured) - tabulated - nested):
        found.append(f"{identifier} is in the layout, is not nested, and is in no §13 table")
    return found, rewrites


STACK_ROW = re.compile(r"^\| \*\*STACK-([A-Z])\*\*")


def plumbing(directory: Path, lines: list[str]) -> list[str]:
    """§12.5's stack table against `layout.levels.json`.

    The other half of `HOUSE-00400`: the task is to walk the plan against the room schedule **and**
    the plumbing diagram. Rule 9 already proves every fixture sits over a declared chase; nothing
    proved that the table in §12.5 still names the same stacks the data does, and `STACK-G` was
    added to both by hand on the day `HOUSE-00413` discovered it.
    """
    layout = layout_io.load_layout(directory)
    stacks = {row.get("id"): row
              for row in ((layout.get("levels") or {}).get("plumbing") or {}).get("stacks") or []}
    out = []
    seen = set()
    for line in lines:
        found = STACK_ROW.match(line)
        if not found:
            continue
        identifier = f"STACK_{found.group(1)}"
        seen.add(identifier)
        stack = stacks.get(identifier)
        if stack is None:
            out.append(f"§12.5 names {identifier} and layout.levels.json does not declare it")
            continue
        columns = [value.strip() for value in line.strip("|").split("|")]
        said = [name for name in re.findall(r"`([A-Z][A-Z0-9_]*)`", columns[1])]
        if said != list(stack.get("cells") or []):
            out.append(f"§12.5: {identifier} serves {said} and the layout says "
                       f"{list(stack.get('cells') or [])}")
        drop = re.findall(r"`([A-Z][A-Z0-9_]*)`", columns[-1])
        if (drop[0] if drop else None) != stack.get("dropTo"):
            out.append(f"§12.5: {identifier} drops to {drop[0] if drop else None} and the layout "
                       f"says {stack.get('dropTo')}")
    for identifier in sorted(set(stacks) - seen):
        out.append(f"{identifier} is in layout.levels.json and §12.5's table does not list it")
    return out


STAIR_ROW = re.compile(r"^\| `(STAIR_[A-Z0-9_]+|STEPS_[A-Z0-9_]+)` \|")


def stairs(directory: Path, lines: list[str]) -> list[str]:
    """§12.4's stair table against `layout.stairs.json`.

    Six hand-written numbers a flight -- total rise, risers, rise, going, width and footprint --
    over data that `HOUSE-00379` has already found eight ways wrong once. The footprint column is
    prose for three of the eight rows ("same footprint", "inside the garage at the house wall"),
    so it is compared where it states coordinates and left alone where it does not.
    """
    layout = layout_io.load_layout(directory, kinds=["stairs"])
    flights = {row.get("id"): row for row in layout_io.rows(layout, "stairs")}
    out = []
    seen = set()
    for line in lines:
        found = STAIR_ROW.match(line)
        if not found:
            continue
        identifier = found.group(1)
        flight = flights.get(identifier)
        if flight is None:
            out.append(f"§12.4 names {identifier} and layout.stairs.json does not declare it")
            continue
        columns = [value.strip() for value in line.strip("|").split("|")]
        if len(columns) < 8:
            # §12.4's row has eight columns. A flight id also appears in §16.2's vertical
            # adjacency table, and that is not this table.
            continue
        seen.add(identifier)
        risers = re.match(r"^(\d+) ×\s*([\d.]+) mm", columns[3].replace("\u00a0", " "))
        if risers:
            if int(risers.group(1)) != int(flight["risers"]):
                out.append(f"§12.4: {identifier} has {risers.group(1)} risers and the layout has "
                           f"{flight['risers']}")
            # §12.4 states the rise to a tenth of a millimetre and says in as many words that
            # its tenths are the exact quotient rounded, so half a tenth is the tolerance.
            if abs(float(risers.group(2)) - float(flight["rise"]) * 1000.0) > 0.0500001:
                out.append(f"§12.4: {identifier}'s rise is {risers.group(2)} mm and the layout's "
                           f"is {float(flight['rise']) * 1000.0:.1f} mm")
        going = re.search(r"([\d.]+) mm", columns[4])
        if going and abs(float(going.group(1)) - float(flight["going"]) * 1000.0) > 0.05:
            out.append(f"§12.4: {identifier}'s going is {going.group(1)} mm and the layout's is "
                       f"{float(flight['going']) * 1000.0:.1f} mm")
        width = re.search(r"([\d.]+) m", columns[5])
        if width and abs(float(width.group(1)) - float(flight["width"])) > 0.005:
            out.append(f"§12.4: {identifier} is {width.group(1)} m wide and the layout says "
                       f"{flight['width']}")
        footprint = flight.get("footprint") or {}
        for axis in ("X", "Z"):
            said = re.search(axis + r"\s*([−+\-\d.]+)\s*…\s*([−+\-\d.]+)", columns[7])
            if not said or axis.lower() not in footprint:
                continue
            want = [float(value.replace("−", "-").replace("+", "")) for value in said.groups()]
            got = [float(value) for value in footprint[axis.lower()]]
            if any(abs(a - b) > 0.005 for a, b in zip(want, got)):
                out.append(f"§12.4: {identifier}'s {axis} footprint is {want} and the layout's is "
                           f"{got}")
    for identifier in sorted(set(flights) - seen):
        out.append(f"{identifier} is in layout.stairs.json and §12.4's table does not list it")
    return out


def extents(directory: Path, lines: list[str]) -> list[str]:
    """Rows whose `X`/`Z` disagree with the cell's bounding box. Reported, never rewritten."""
    layout = layout_io.load_layout(directory)
    cells = {row["id"]: row for row in layout_io.rows(layout, "cells")}
    out = []
    for section, header, _index, columns in sections(lines):
        row = dict(zip(header, columns))
        identifier = row["ID"].strip().strip("∪").strip().strip("`")
        cell = cells.get(identifier)
        if cell is None or "X" not in header:
            continue
        boxes = cell.get("boxes") or []
        if not boxes:
            continue
        bounds = {"X": (min(b["x"][0] for b in boxes), max(b["x"][1] for b in boxes)),
                  "Z": (min(b["z"][0] for b in boxes), max(b["z"][1] for b in boxes))}
        for axis in ("X", "Z"):
            said = parse_range(row[axis])
            if said is None:
                continue  # prose, e.g. L3_ROOM's "plus two dormer bays"
            if abs(said[0] - bounds[axis][0]) > 0.005 or abs(said[1] - bounds[axis][1]) > 0.005:
                out.append(f"§{section}: {identifier} {axis} says {said[0]:g}…{said[1]:g} and the "
                           f"layout's boxes span {bounds[axis][0]:g}…{bounds[axis][1]:g}")
    return out


# ======================================================================================= selftest


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("room_schedule: selftest")

    require(stated("3 (sectional, side, house)") == 3.0,
            "the leading number is read out of a descriptive cell")
    require(stated("—") is None and stated("") is None,
            "and an em dash states no number rather than zero")
    require(stated("−12.70 … −8.20") == -12.70,
            "a minus sign in this document is U+2212, and it is a minus sign")
    require(parse_range("−12.70 … −8.20") == (-12.70, -8.20),
            "an extent parses with the document's dashes and ellipsis")
    require(parse_range("−24.00 … −17.00, plus two dormer bays to −14.30") is None,
            "and prose in an extent cell parses as prose, not as a silent zero")

    if not (SOURCE / "layout.cells.json").is_file() or not DOCUMENT.is_file():
        require(False, "the authored world and cna-house.md are both here")
    else:
        lines = DOCUMENT.read_text(encoding="utf-8").splitlines()
        rows = list(sections(lines))
        require(len(rows) >= 90, f"§13.2-§13.7 parse as {len(rows)} tabulated rows")
        require(len({columns[0].strip(" `∪") for _s, _h, _i, columns in rows}) == len(rows),
                "each row is a distinct cell")

        truth = measure(SOURCE)
        # An L-shaped cell is the L. This is the case the table got wrong, so it is the case the
        # measurement has to be right about.
        require(truth["B1_UNDERSTAIR"]["boxes"] > 1
                and abs(truth["B1_UNDERSTAIR"]["Area"] - 4.7) < 0.05,
                f"a union of boxes measures the union, not its bounding rectangle "
                f"({truth['B1_UNDERSTAIR']['Area']})")
        require(truth["L0_HALL"]["Doors"] == 3,
                f"a door counts for BOTH cells it is on the boundary of, so a hall with three "
                f"doorways has three ({truth['L0_HALL']['Doors']})")
        require(truth["L0_FOYER"]["Win"] == 3,
                f"and the foyer's two sidelights and transom are three windows "
                f"({truth['L0_FOYER']['Win']})")
        require(truth["L0_KITCHEN"]["Lights"] == 4,
                f"lights are switch groups ({truth['L0_KITCHEN']['Lights']})")

        found, _ = problems(SOURCE, lines)
        require(not found, f"and §13 states every one of them ({found[:3]})")
        drift = extents(SOURCE, lines)
        require(not drift, f"...as well as every extent ({drift[:3]})")

        steps = stairs(SOURCE, lines)
        require(not steps, f"§12.4's eight flights match the layout ({steps[:3]})")
        wrong = [line.replace("17 × 179.4 mm", "18 × 179.4 mm") for line in lines]
        require(any("risers" in problem for problem in stairs(SOURCE, wrong)),
                "and a riser added to the table and not to the data is caught")
        moved = [line.replace("X +2.20…+4.90, Z −20.20…−14.30", "X +2.20…+4.90, Z −21.20…−14.30")
                 for line in lines]
        require(any("footprint" in problem for problem in stairs(SOURCE, moved)),
                "as is a footprint that has drifted a metre")
        shallow = [line.replace("| 280 mm |", "| 300 mm |") for line in lines]
        require(any("going" in problem for problem in stairs(SOURCE, shallow)),
                "as is a going")
        narrow = [line.replace("| 1.10 m |", "| 1.20 m |") for line in lines]
        require(any("wide" in problem for problem in stairs(SOURCE, narrow)),
                "as is a width")
        dropped = [line for line in lines if "`STAIR_ATTIC_L2_L3` |" not in line]
        require(any("does not list it" in problem for problem in stairs(SOURCE, dropped)),
                "and a flight deleted from the table is reported, which a check that only "
                "compared the rows it found could not see")

        pipes = plumbing(SOURCE, lines)
        require(not pipes, f"§12.5's seven stacks match the layout ({pipes[:3]})")
        moved = [line.replace("`B1_UTILITY` |", "`B1_CINEMA` |") for line in lines]
        require(any("drops to" in problem for problem in plumbing(SOURCE, moved)),
                "and a stack draining into the wrong room is caught")
        renamed = [line.replace("`L1_WC4`", "`L1_WC3`") for line in lines]
        require(any("serves" in problem for problem in plumbing(SOURCE, renamed)),
                "as is one whose fixture list has drifted")

    if failures:
        print(f"\nroom_schedule: {len(failures)} claim(s) FAILED")
        return 1
    print("room_schedule: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", nargs="?", type=Path, default=SOURCE)
    parser.add_argument("--document", type=Path, default=DOCUMENT)
    parser.add_argument("--emit", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if not (args.directory / "layout.cells.json").is_file():
        print(f"room_schedule: no world in {args.directory} yet -- nothing to measure.")
        return 0

    lines = args.document.read_text(encoding="utf-8").splitlines()
    found, rewrites = problems(args.directory, lines)
    drift = (extents(args.directory, lines) + plumbing(args.directory, lines)
             + stairs(args.directory, lines))

    if args.emit and rewrites:
        for index, line in rewrites.items():
            lines[index] = line
        args.document.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"room_schedule: rewrote {len(rewrites)} row(s) of §13 from the layout")
        found = [problem for problem in found if " says " not in problem]

    for problem in found + drift:
        print(f"room_schedule: {problem}", file=sys.stderr)
    if found or drift:
        if drift:
            print("room_schedule: an extent is never rewritten -- decide which is right",
                  file=sys.stderr)
        return 1
    print(f"room_schedule: §13 matches {args.directory}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
