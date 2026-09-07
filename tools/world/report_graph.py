#!/usr/bin/env python3
"""report_graph.py -- §16's room/portal graph, measured from the data rather than written down.

`HOUSE-00359`. `cna-house.md` §16.1-§16.3 states what the house's graph is. Those numbers were
designed before the layout existed -- 186 portals, a mean interior degree of 3.9, a diameter of 11
hops, 34 components once every door is shut -- and `HOUSE-00374` replaced five of them with what
the data actually says, including a "largest component" this tool had been reporting wrongly.
This computes them from
`assets-src/world/` so that the design and the data can be compared, and so that a portal quietly
dropped during authoring shows up as a number that moved rather than as a room nobody can enter.

    tools/world/report_graph.py assets-src/world              # the §16 tables, as markdown
    tools/world/report_graph.py assets-src/world --json       # the same numbers, machine-readable
    tools/world/report_graph.py assets-src/world -o docs/world-graph.md
    tools/world/report_graph.py --selftest

## Two graphs, and the difference between them is the point

**Through open doors** every portal but a window is an edge: this is the graph the player walks,
and its diameter is how far apart two places in the house are.

**With every door closed** only the always-open portals are edges -- a cased opening and a stair
well. §16.3 predicts the house falls into 34 pieces, and calls that "the whole point": the attic
door and the basement door really do cut those levels off, and the visibility system really does
see three floors at once through the stair well. Both facts fall out of the data.

A window is never an edge in either. It is a hole for light and for sound, not for a person, and
`validate_world.py` rule 5 makes the same distinction for the same reason.

## Why this is a report and not a gate

The numbers move for good reasons -- a task adds a room, a door becomes an opening -- so a gate
over them would fail on every legitimate change and be disabled within a week. What is worth
gating is *connectivity*, and `validate_world.py` rule 5 already does that. This measures.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import collections
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_io  # noqa: E402
import validate_world  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

#: §16.1's legend, so the generated table reads like the one in the architecture document.
GLYPH = {
    "cased_opening": "○", "door": "▣", "double_door": "◫", "slider": "▤",
    "garage_door": "▦", "window": "▥", "stair_well": "↕", "exterior_door": "▣",
    "hatch": "⌸",
}

#: Open whether or not anybody has touched anything. Everything else in the vocabulary has a leaf
#: that can be shut, which is what the second graph is about.
ALWAYS_OPEN = {"cased_opening", "stair_well"}

#: Never a way through for a person, in either graph.
NEVER_PASSABLE = {"window"}


def edges(portals: list[dict], kinds: set[str] | None = None,
          ) -> dict[str, set[str]]:
    """The undirected adjacency of the cells, over the portals whose kind is in `kinds`."""
    adjacency: dict[str, set[str]] = {}
    for portal in portals:
        kind = portal.get("kind")
        if kind in NEVER_PASSABLE:
            continue
        if kinds is not None and kind not in kinds:
            continue
        a, b = portal.get("cellA"), portal.get("cellB")
        if not isinstance(a, str) or not isinstance(b, str):
            continue
        adjacency.setdefault(a, set()).add(b)
        adjacency.setdefault(b, set()).add(a)
    return adjacency


def components(nodes: list[str], adjacency: dict[str, set[str]]) -> list[list[str]]:
    """The connected components, each sorted, the list ordered by each group's smallest member.

    The walk starts from `sorted(nodes)`, so a group is opened by the smallest cell it contains and
    the list comes out ordered without a second sort. That matters because this is written into a
    document that gets diffed: a component list that reshuffles when a cell is renamed hides the
    one component that actually changed.
    """
    universe = set(nodes)
    seen: set[str] = set()
    out = []
    for node in sorted(universe):
        if node in seen:
            continue
        group = []
        frontier = [node]
        seen.add(node)
        while frontier:
            current = frontier.pop()
            group.append(current)
            for neighbour in adjacency.get(current, ()):
                if neighbour not in seen and neighbour in universe:
                    seen.add(neighbour)
                    frontier.append(neighbour)
        out.append(sorted(group))
    return out


def diameter(nodes: list[str], adjacency: dict[str, set[str]],
             ) -> tuple[int | None, tuple[str, str] | None]:
    """The longest shortest path, and the pair that realises it.

    `None` when the graph is disconnected: the diameter of a disconnected graph is infinite, and
    reporting the largest *finite* distance instead would be a number that looks fine and means
    something else. §16.3 writes it as ∞ for exactly this reason.
    """
    universe = set(nodes)
    if len(components(nodes, adjacency)) > 1:
        return None, None
    best = -1
    pair = None
    for source in nodes:
        distance = {source: 0}
        queue = collections.deque([source])
        while queue:
            current = queue.popleft()
            for neighbour in adjacency.get(current, ()):
                if neighbour in universe and neighbour not in distance:
                    distance[neighbour] = distance[current] + 1
                    queue.append(neighbour)
        for target, hops in distance.items():
            if hops > best or (hops == best and pair is not None
                               and (source, target) < pair):
                best, pair = hops, (source, target)
    return (best if best >= 0 else None), pair


def measure(directory: Path) -> dict:
    """Every number §16.3 states, computed from the layout."""
    layout = layout_io.load_layout(directory, ["cells", "portals", "levels"])
    cells = layout_io.rows(layout, "cells")
    portals = layout_io.rows(layout, "portals")
    levels = [row.get("id") for row in layout_io.rows(layout, "levels")]

    ids = [str(row.get("id")) for row in cells]
    interior = [str(row.get("id")) for row in cells
                if row.get("kind") not in ("exterior", "void")]
    exterior = [str(row.get("id")) for row in cells if row.get("kind") == "exterior"]

    by_kind = collections.Counter(str(row.get("kind")) for row in portals)
    open_graph = edges(portals)
    shut_graph = edges(portals, ALWAYS_OPEN)

    degree = {cell: len(open_graph.get(cell, ())) for cell in interior}
    hops, pair = diameter(ids, open_graph)
    shut_components = components(ids, shut_graph)

    return {
        "levels": levels,
        "cells": {"total": len(ids), "interior": len(interior), "exterior": len(exterior)},
        "portals": {"total": len(portals), "byKind": dict(sorted(by_kind.items()))},
        "degree": {
            "mean": round(sum(degree.values()) / len(degree), 2) if degree else 0.0,
            "max": max(degree.values()) if degree else 0,
            "maxCell": max(sorted(degree), key=lambda cell: degree[cell]) if degree else None,
            "isolated": sorted(cell for cell, count in degree.items() if count == 0),
        },
        "openGraph": {"diameter": hops, "diameterBetween": list(pair) if pair else None,
                      "components": len(components(ids, open_graph))},
        # `max`, not `[0]`. `components` orders its groups by their smallest member so that the
        # list diffs cleanly, so `[0]` is the group holding the alphabetically first cell -- which
        # on the authored house is `B1_CELLAR`, alone behind its door. It reported "largest
        # component: 1 cells" beside an exterior ring of 18 cased openings (`HOUSE-00374`).
        "shutGraph": {"components": len(shut_components),
                      "largestComponent": max((len(g) for g in shut_components), default=0)},
        "rows": adjacency_rows(cells, portals),
    }


def adjacency_rows(cells: list[dict], portals: list[dict]) -> list[dict]:
    """§16.1's table, one row per portal, in a stable order."""
    level_of = {str(row.get("id")): str(row.get("level")) for row in cells}
    out = []
    for portal in portals:
        a, b = str(portal.get("cellA")), str(portal.get("cellB"))
        kind = str(portal.get("kind"))
        rect = portal.get("rect") or {}
        try:
            width = float(rect["u"][1]) - float(rect["u"][0])
        except (KeyError, IndexError, TypeError, ValueError):
            width = 0.0
        out.append({
            "from": a, "to": b, "portal": str(portal.get("id")), "kind": kind,
            "glyph": GLYPH.get(kind, "?"),
            "levelA": level_of.get(a, "?"), "levelB": level_of.get(b, "?"),
            "vertical": level_of.get(a) != level_of.get(b),
            "width": round(width, 2),
            "opacity": portal.get("opacity") or "open",
        })
    return sorted(out, key=lambda row: (row["levelA"], row["from"], row["portal"]))


# ====================================================================================== markdown ==


def markdown(report: dict) -> str:
    lines = [
        "# The room / portal graph, as built",
        "",
        "Generated by `tools/world/report_graph.py` (`HOUSE-00359`) from `assets-src/world/`.",
        "`cna-house.md` §16 is the design; this is the measurement. Do not edit.",
        "",
        "## Adjacency",
        "",
        "`○` cased opening · `▣` door · `◫` double door · `▤` slider · `▦` garage door ·",
        "`▥` window · `↕` stair well · `⌸` hatch.",
        "",
    ]

    horizontal = [row for row in report["rows"] if not row["vertical"]]
    for level in report["levels"]:
        rows = [row for row in horizontal if row["levelA"] == level]
        if not rows:
            continue
        lines += [f"### {level}", "",
                  "| From → To | Portal | Kind | Width | Opacity |",
                  "|---|---|---|---|---|"]
        for row in rows:
            lines.append(f"| `{row['from']}` → `{row['to']}` | `{row['portal']}` | "
                         f"{row['glyph']} | {row['width']:.2f} m | {row['opacity']} |")
        lines.append("")

    vertical = [row for row in report["rows"] if row["vertical"]]
    if vertical:
        lines += ["### Vertical", "",
                  "| Portal | From | To | Kind |", "|---|---|---|---|"]
        for row in vertical:
            lines.append(f"| `{row['portal']}` | `{row['from']}` ({row['levelA']}) | "
                         f"`{row['to']}` ({row['levelB']}) | {row['glyph']} {row['kind']} |")
        lines.append("")

    cells, portals, degree = report["cells"], report["portals"], report["degree"]
    open_graph, shut_graph = report["openGraph"], report["shutGraph"]
    hops = open_graph["diameter"]
    between = open_graph["diameterBetween"]
    lines += ["## Graph shape", "", "| Metric | Value |", "|---|---|",
              f"| Cells | {cells['total']} "
              f"({cells['interior']} interior + {cells['exterior']} exterior) |",
              f"| Portals total | {portals['total']} |"]
    for kind, count in portals["byKind"].items():
        lines.append(f"| — {kind} | {count} |")
    lines += [
        f"| Mean interior cell degree | {degree['mean']} |",
        f"| Max interior cell degree | "
        f"`{degree['maxCell']}` = {degree['max']} |",
        f"| Graph diameter (through open doors) | "
        + (f"{hops} hops (`{between[0]}` → `{between[1]}`)" if hops is not None
           else f"∞ — {open_graph['components']} components")
        + " |",
        f"| Diameter with all doors closed | ∞ — the graph fragments into "
        f"{shut_graph['components']} components, which is the whole point |",
        f"| Largest component with all doors closed | "
        f"{shut_graph['largestComponent']} cells |",
        "",
    ]
    if degree["isolated"]:
        lines += ["Interior cells with no passable portal at all — "
                  "`validate_world.py` rule 5 fails on every one of these:", ""]
        lines += [f"* `{cell}`" for cell in degree["isolated"]]
        lines.append("")
    return "\n".join(lines)


# ====================================================================================== selftest ==


def selftest() -> int:
    import copy
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("report_graph: selftest")
    workspace = Path(tempfile.mkdtemp(prefix="report_graph_selftest_"))
    try:
        # The same house `validate_world.py` validates, deliberately: two tools that disagree
        # about what a house looks like will disagree about it silently, and the day one of them
        # is wrong the other is the only thing that can say so.
        base = validate_world.fixture()
        world = workspace / "world"
        validate_world.write_fixture(world, base)
        report = measure(world)

        require(report["cells"] == {"total": 9, "interior": 8, "exterior": 1},
                f"9 cells, 8 interior and 1 exterior terrace ({report['cells']})")
        require(report["portals"]["total"] == 8,
                f"8 portals ({report['portals']['total']})")

        # 1. The two graphs differ, and they differ by the doors. This is the claim §16.3 makes
        #    about the real house and the only one worth proving on a fixture.
        open_graph = edges(base["portals"]["portals"])
        shut_graph = edges(base["portals"]["portals"], ALWAYS_OPEN)
        require(sum(len(v) for v in open_graph.values()) == 16,
                f"through open doors there are 8 undirected edges "
                f"({sum(len(v) for v in open_graph.values()) // 2})")
        require(sum(len(v) for v in shut_graph.values()) == 10,
                f"with every door shut there are 5 -- the two WC doors and the terrace door go "
                f"({sum(len(v) for v in shut_graph.values()) // 2})")
        require(report["shutGraph"]["components"] == 4,
                f"and the house falls into 4 pieces: the two WCs and the terrace each become "
                f"their own ({report['shutGraph']['components']})")
        # The LARGEST piece, not the first one listed. `components` orders its groups by their
        # smallest member so the list diffs cleanly, so the two answers coincide only while the
        # alphabetically first cell happens to be in the biggest group. Renaming one WC so that it
        # sorts first separates them: the authored house does this by itself, where `B1_CELLAR` is
        # alone behind its door and the report said "largest component: 1 cells" next to an
        # exterior ring of eighteen (`HOUSE-00374`).
        renamed = copy.deepcopy(base)
        for cell in renamed["cells"]["cells"]:
            if cell["id"] == "L0_WC1":
                cell["id"] = "B1_WC1"
        for portal in renamed["portals"]["portals"]:
            for end in ("cellA", "cellB"):
                if portal[end] == "L0_WC1":
                    portal[end] = "B1_WC1"
        first_last = workspace / "first-last"
        validate_world.write_fixture(first_last, renamed)
        renamed_report = measure(first_last)
        groups = components([c["id"] for c in renamed["cells"]["cells"]],
                            edges(renamed["portals"]["portals"], ALWAYS_OPEN))
        require(len(groups[0]) == 1 and renamed_report["shutGraph"]["largestComponent"] == 6,
                f"the largest piece is the 6-cell one even when the FIRST piece listed is a "
                f"1-cell WC ({len(groups[0])} vs "
                f"{renamed_report['shutGraph']['largestComponent']})")

        # 2. A window is not a way through, in either graph. `validate_world.py` rule 5 makes the
        #    same distinction; if these two ever disagree, one of them is letting a player walk
        #    through glass.
        glazed = copy.deepcopy(base)
        for portal in glazed["portals"]["portals"]:
            if portal["id"] == "P_HALL__WC1":
                portal["kind"] = "window"
        walled = workspace / "glazed"
        validate_world.write_fixture(walled, glazed)
        after = measure(walled)
        require(after["openGraph"]["diameter"] is None,
                "turning the WC's door into a window disconnects the house, so its diameter is "
                "∞ rather than a finite number that looks fine")
        require(after["degree"]["isolated"] == ["L0_WC1"],
                f"and the WC is reported as isolated by name ({after['degree']['isolated']})")
        _, rule5 = validate_world.validate(walled, wanted=[5])
        require(len(rule5) == 1 and "L0_WC1" in rule5[0].message,
                f"which is exactly what validate_world.py rule 5 says about the same file "
                f"({[str(p) for p in rule5]})")

        # 3. The diameter is the longest SHORTEST path, and it is measured, not assumed.
        #    L0_TERRACE to L1_WC4: terrace -> foyer -> hall -> stair -> landing -> L1 hall -> WC4.
        hops, pair = report["openGraph"]["diameter"], report["openGraph"]["diameterBetween"]
        require(hops == 6,
                f"the fixture's diameter is 6 hops ({hops}) -- terrace to the upstairs WC, "
                f"through the foyer, the hall, the stair, the landing and the upstairs hall")
        require(set(pair) == {"L0_TERRACE", "L1_WC4"},
                f"between the terrace and the upstairs WC ({pair})")

        # 4. Degree counts passable portals, and the busiest cell is named.
        require(report["degree"]["maxCell"] == "L0_HALL" and report["degree"]["max"] == 4,
                f"the hall is the busiest cell with 4 ways out "
                f"({report['degree']['maxCell']} = {report['degree']['max']})")
        require(abs(report["degree"]["mean"] - 1.88) < 1e-9,
                f"and the mean interior degree is 1.88 -- of the 16 edge ends, one belongs to "
                f"the exterior terrace, so 15 fall on the 8 interior cells "
                f"({report['degree']['mean']})")

        # 5. Disconnected means ∞, not "the largest finite distance I found".
        island = copy.deepcopy(base)
        island["cells"]["cells"].append(
            {"id": "L0_ISLAND", "level": "L0", "kind": "room",
             "boxes": [{"x": [20.0, 24.0], "z": [20.0, 24.0]}]})
        marooned = workspace / "island"
        validate_world.write_fixture(marooned, island)
        after = measure(marooned)
        require(after["openGraph"]["diameter"] is None
                and after["openGraph"]["components"] == 2,
                f"a room with no portal makes the diameter ∞ and the component count 2 "
                f"({after['openGraph']})")

        # 6. The markdown is deterministic and mentions every portal, or it is a table that
        #    silently lost a row. Deterministic against a *reordered* file, not merely against a
        #    second run of the same one: within one process Python's own ordering is stable, so
        #    running twice proves nothing about a table emitted in whatever order it was read.
        shuffled = copy.deepcopy(base)
        shuffled["portals"]["portals"].reverse()
        shuffled["cells"]["cells"].reverse()
        reordered = workspace / "reordered"
        validate_world.write_fixture(reordered, shuffled)
        require(markdown(measure(reordered)) == markdown(report),
                "the same house with its rows written in the opposite order produces the same "
                "document, byte for byte")
        require(components(["C", "A", "B"], {}) == [["A"], ["B"], ["C"]],
                f"and components() comes out ordered by each group's smallest member "
                f"({components(['C', 'A', 'B'], {})})")
        first, second = markdown(report), markdown(measure(world))
        require(first == second, "two runs produce byte-identical markdown")
        require(all(row["portal"] in first for row in report["rows"]),
                "and every portal appears in it")
        require("↕" in first and "### Vertical" in first,
                "the stair well is in the vertical table, where §16.2 puts it")

        # 7. The report is JSON-serialisable, because the numbers are what another tool wants.
        require(json.loads(json.dumps(report)) == report,
                "the report round-trips through JSON unchanged")
    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        print(f"\nreport_graph: {len(failures)} claim(s) FAILED")
        return 1
    print("report_graph: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", nargs="?", type=Path,
                        default=REPO / "assets-src" / "world")
    parser.add_argument("--json", action="store_true", help="emit the numbers, not the tables")
    parser.add_argument("-o", "--output", type=Path, help="write the markdown to a file")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if not args.directory.is_dir():
        print(f"report_graph: {args.directory}: no such directory", file=sys.stderr)
        return 2

    report = measure(args.directory)
    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
        return 0
    text = markdown(report)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
        print(f"report_graph: wrote {args.output.relative_to(REPO)}")
        return 0
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
