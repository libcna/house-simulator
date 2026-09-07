#!/usr/bin/env python3
"""validate_world.py -- the eleven rules of `cna-house.md` §15.7, over a whole world directory.

`HOUSE-00358`. `world_schema.py` (`HOUSE-00341`) checks that each of the sixteen files has the
right *shape*. This checks that the sixteen agree with each other and with the house: that a
portal's rectangle really lies in the wall it claims, that every room can be walked to from the
foyer, that a stair's risers add up to the floor above, that an interactable can actually be
reached by someone standing in the room.

    tools/world/validate_world.py assets-src/world
    tools/world/validate_world.py assets-src/world --rules 4,5      # just these
    tools/world/validate_world.py --selftest

Exit status is 0 when every rule passes and 1 otherwise, so it is a gate (`HOUSE-00361`) and a
pre-build step. `cna-house.md` §15.7: a failure fails the build.

## Every failure, not the first

Each rule collects **all** its failures and the run reports all eleven rules' worth, because
fixing forty authoring mistakes one build at a time is intolerable (`conventions.md` §5.1). Each
message names the file, the JSON path and what was expected against what was found -- a message
that says "portal misaligned" and stops has told the author to go and search.

The one place the run does stop early is a **shape** failure. If a file does not match its schema
the semantic rules cannot read it without inventing what a missing field meant, and every rule
would then report a cascade of consequences of one typo. Shape problems are reported in full and
the semantic rules are skipped, and the output says so.

## What this tool does NOT decide, and why

Two parts of rule 10 are not here and are not missing:

* **counter, table, seat, sill and switch heights** (§70.5's long table) need the *asset's*
  geometry, not the layout's -- a prop row is a position and an asset id, and the height that
  matters is inside the `.glb`. `scale_check.py` measures assets; `HOUSE-00360` joins the two.
* **human, pet and car scale** are likewise a property of the asset, and §70.5 assigns them to
  `scale_check.py` in as many words.

What *is* here is everything the layout alone determines: door leaf sizes, room clear heights,
stair `2R + G`, and player capsule clearance through every portal. §70.5's "rise consistency within
a flight" is not among them and cannot be: `layout.stairs.json` carries **one** `rise` per flight,
so every riser in a flight is equal by construction and the check would assert a tautology.

## Rule 11 is a proof, and it is a proof about the room, not about the props

§15.7 asks for a **reachability proof**: a 2.5 m ray from a standing eye position must reach the
interactable's `focus.point`. This searches the cell's floor for an eye position that has one, and
requires the whole segment to stay inside the cell's own footprint. That last condition is what
makes it a proof rather than a distance check: walls exist only on cell boundaries, so a segment
that never leaves the cell cannot have crossed one. Props are not obstacles at this level -- a
sofa in front of a socket is a dressing question, and putting it in this rule would make the
validator depend on furniture that has not been placed yet.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_io  # noqa: E402
import world_schema  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

# --------------------------------------------------------------------------------- tolerances ---
# Every one of these is §15.7's or §70.5's number, named here so a message can quote it.

PLANE_TOLERANCE = 0.01          # §15.7 rule 4: "within 1 cm"
OVERLAP_AREA_TOLERANCE = 1e-4   # §15.7 rule 3: "more than 1 cm²"
RISE_TOLERANCE = 0.001          # §15.7 rule 8: "within 1 mm"
DOOR_HEIGHT = (1.98, 2.10)      # §70.5, interior door leaf
DOOR_WIDTH = (0.76, 0.95)
CEILING_CLEAR = (2.35, 3.10)    # §70.5, habitable rooms
STAIR_2R_G = (0.600, 0.650)     # §70.5, 2·rise + going
CAPSULE_WIDTH = 0.62            # §70.5, player capsule through a portal
CAPSULE_HEIGHT = 1.95
REACH_RANGE = 2.50              # §15.7 rule 11
EYE_HEIGHT = 1.60               # a standing eye above the floor
CAPSULE_RADIUS = CAPSULE_WIDTH / 2.0   # somewhere a player can actually stand

#: §15.7 rule 5 walks "always-open or door" portals. A window is not a way through, whatever its
#: opacity; everything else is, including a hatch, which is a way through for someone willing to
#: crouch.
IMPASSABLE_PORTAL_KINDS = {"window"}

#: Where the walk starts. §15.7 names it.
ROOT_CELL = "L0_FOYER"

RULE_TITLES = {
    1: "ids are unique and well formed",
    2: "cell boxes are non-degenerate and inside their level",
    3: "no two cells on a level overlap",
    4: "portal rectangles lie in both cells' planes",
    5: "the portal graph is connected",
    6: "every reference resolves",
    7: "every door and window has exactly one portal",
    8: "stair flights reach the floor they claim",
    9: "every plumbing fixture is on a declared stack",
    10: "the layout is dimensionally plausible",
    11: "every interactable is reachable from the floor",
}


class Problem:
    """One failure, addressed the way an author needs it: file, path, expected, found."""

    def __init__(self, rule: int, file: str, path: str, message: str) -> None:
        self.rule = rule
        self.file = file
        self.path = path
        self.message = message

    def __str__(self) -> str:
        return f"{self.file}:{self.path}: {self.message}"

    def __repr__(self) -> str:  # pragma: no cover -- selftest diagnostics
        return f"<rule {self.rule} {self}>"


FILE_OF = {kind: name for kind, (name, _) in layout_io.FILES.items()}


# ==================================================================================== geometry ===


def overlap(a: tuple[float, float], b: tuple[float, float]) -> float:
    """Length of the intersection of two closed intervals; 0 when they do not meet."""
    return max(0.0, min(a[1], b[1]) - max(a[0], b[0]))


def boxes_of(cell: dict) -> list[tuple[float, float, float, float]]:
    """The cell's footprint as `(x0, x1, z0, z1)`, tolerantly.

    `layout_io.cell_boxes` raises on an inverted box, which is right for a tool that must not build
    nonsense from one. A validator must not: an inverted box is rule 2's finding, and if reading it
    threw, rules 3, 4, 9 and 11 would all die on one typo instead of reporting their own.
    """
    out = []
    for box in cell.get("boxes", []) or []:
        try:
            x0, x1 = float(box["x"][0]), float(box["x"][1])
            z0, z1 = float(box["z"][0]), float(box["z"][1])
        except (KeyError, IndexError, TypeError, ValueError):
            continue
        out.append((min(x0, x1), max(x0, x1), min(z0, z1), max(z0, z1)))
    return out


def point_in_boxes(x: float, z: float,
                   boxes: list[tuple[float, float, float, float]],
                   margin: float = 0.0) -> bool:
    return any(x0 - margin <= x <= x1 + margin and z0 - margin <= z <= z1 + margin
               for x0, x1, z0, z1 in boxes)


def standable(x: float, z: float,
              boxes: list[tuple[float, float, float, float]],
              radius: float = CAPSULE_RADIUS) -> bool:
    """Can the player capsule stand at `(x, z)`?

    The point and its four cardinal offsets by the capsule radius must all be inside the union.
    Testing the offsets rather than shrinking each box is what makes a cell authored as two
    abutting rectangles behave like the one room it is -- shrinking would carve a phantom wall out
    of the seam. It is an approximation of a disc, and a generous one at the corners, which is the
    right way round: rule 11 must not reject a house that can be walked through.
    """
    return all(point_in_boxes(x + dx, z + dz, boxes)
               for dx, dz in ((0.0, 0.0), (radius, 0.0), (-radius, 0.0),
                              (0.0, radius), (0.0, -radius)))


def segment_inside(x0: float, z0: float, x1: float, z1: float,
                   boxes: list[tuple[float, float, float, float]], steps: int = 48) -> bool:
    """Does the whole segment stay inside the union of `boxes`?

    Sampled, not analytic. A union of axis-aligned rectangles is not convex -- an L-shaped room is
    the reason cells carry a *list* of boxes -- so the endpoints being inside proves nothing about
    the middle. 48 samples over at most 2.5 m is a step of 5 cm, well under the thinnest thing a
    cell boundary can be (`wallPartition`, 15 cm).
    """
    for index in range(steps + 1):
        t = index / steps
        if not point_in_boxes(x0 + (x1 - x0) * t, z0 + (z1 - z0) * t, boxes, margin=1e-9):
            return False
    return True


# ======================================================================================= loader ==


class World:
    """The layout, indexed the way the rules need it. Built once, read by all eleven."""

    def __init__(self, layout: dict[str, dict]) -> None:
        self.layout = layout
        self.levels = layout_io.rows(layout, "levels") if "levels" in layout else []
        self.cells = layout_io.rows(layout, "cells") if "cells" in layout else []
        self.portals = layout_io.rows(layout, "portals") if "portals" in layout else []
        self.openings = layout_io.rows(layout, "openings") if "openings" in layout else []
        self.flights = layout_io.rows(layout, "stairs") if "stairs" in layout else []
        self.lights = layout_io.rows(layout, "lights") if "lights" in layout else []
        self.props = layout_io.rows(layout, "props") if "props" in layout else []
        self.materials = layout_io.rows(layout, "materials") if "materials" in layout else []
        self.interactables = (layout_io.rows(layout, "interactables")
                              if "interactables" in layout else [])
        self.nav_nodes = layout_io.rows(layout, "nav") if "nav" in layout else []
        self.audio_zones = layout_io.rows(layout, "audio") if "audio" in layout else []
        self.assets = layout_io.rows(layout, "assets") if "assets" in layout else []

        self.level_by_id = {row.get("id"): row for row in self.levels}
        self.cell_by_id = {row.get("id"): row for row in self.cells}
        self.portal_by_id = {row.get("id"): row for row in self.portals}

    def extent(self, cell: dict) -> tuple[float, float] | None:
        """The cell's floor and ceiling Y, or None when the layout cannot say."""
        level = self.level_by_id.get(cell.get("level"))
        if level is None:
            return None
        try:
            return layout_io.cell_extent(cell, level)
        except layout_io.LayoutError:
            return None

    def next_level_above(self, level_id: str) -> dict | None:
        """The level immediately above @p level_id by `ffl`, or None at the top."""
        ordered = sorted((row for row in self.levels if isinstance(row.get("ffl"), (int, float))),
                         key=lambda row: float(row["ffl"]))
        for index, row in enumerate(ordered):
            if row.get("id") == level_id and index + 1 < len(ordered):
                return ordered[index + 1]
        return None

    def next_level_below(self, level_id: str) -> dict | None:
        """The level immediately below @p level_id by `ffl`, or None at the bottom."""
        ordered = sorted((row for row in self.levels if isinstance(row.get("ffl"), (int, float))),
                         key=lambda row: float(row["ffl"]))
        for index, row in enumerate(ordered):
            if row.get("id") == level_id and index > 0:
                return ordered[index - 1]
        return None

    def _overlaps_level(self, cell: dict, level: dict | None) -> bool:
        if level is None:
            return False
        boxes = boxes_of(cell)
        for other in self.cells:
            if other.get("level") != level.get("id"):
                continue
            for ax0, ax1, az0, az1 in boxes_of(other):
                for bx0, bx1, bz0, bz1 in boxes:
                    if overlap((ax0, ax1), (bx0, bx1)) * overlap((az0, az1), (bz0, bz1)) > 0.0:
                        return True
        return False

    def covered_from_below(self, cell: dict, level_id: str) -> bool:
        """Does any cell on the level below overlap @p cell's footprint?"""
        return self._overlaps_level(cell, self.next_level_below(level_id))

    def covered_from_above(self, cell: dict, level_id: str) -> bool:
        """Does any cell on the level above overlap @p cell's footprint?

        The question rule 2 actually wants to ask. "Is this cell's ceiling below the next storey's
        slab" only means something where that storey exists over it; a projecting single-storey
        wing has nothing above it and is entitled to its own roof height.
        """
        return self._overlaps_level(cell, self.next_level_above(level_id))

    def level_ceiling_limit(self, level_id: str) -> float | None:
        """The Y above which a cell on this level would be poking into the floor above.

        The next level up's `ffl` minus its `structureDepth` -- the underside of its slab. The
        topmost level has nothing above it, so it has no limit here; `ridgeY` bounds it and the
        roof, not this rule.
        """
        ordered = sorted((row for row in self.levels if isinstance(row.get("ffl"), (int, float))),
                         key=lambda row: float(row["ffl"]))
        for index, row in enumerate(ordered):
            if row.get("id") != level_id:
                continue
            if index + 1 >= len(ordered):
                return None
            above = ordered[index + 1]
            return float(above["ffl"]) - float(above.get("structureDepth", 0.0))
        return None


# ======================================================================================== rules ==


def rule_1_ids(world: World) -> list[Problem]:
    """Every id is unique **across every kind** and matches the pattern.

    Across every kind, not within one: `world-format.md` says "globally unique across every kind",
    and the reason is that references do not carry the kind they point at -- a prop's `material`
    and a cell's `floorMaterial` are both bare ids, and a light group that shares a name with a
    material makes rule 6 answer a question nobody asked.
    """
    import re

    pattern = re.compile(world_schema.ID_PATTERN)
    seen: dict[str, str] = {}
    problems = []
    for kind in ("levels", "cells", "portals", "openings", "stairs", "lights", "props",
                 "materials", "interactables"):
        if kind not in world.layout:
            continue
        _, key = layout_io.FILES[kind]
        for index, row in enumerate(layout_io.rows(world.layout, kind)):
            identifier = row.get("id")
            where = f"{key}/{index}/id"
            if not isinstance(identifier, str) or not pattern.match(identifier):
                problems.append(Problem(1, FILE_OF[kind], where,
                                        f"id {identifier!r} does not match "
                                        f"{world_schema.ID_PATTERN}"))
                continue
            if identifier in seen:
                problems.append(Problem(1, FILE_OF[kind], where,
                                        f"id {identifier!r} is already used in {seen[identifier]}; "
                                        f"ids are unique across every kind, not within one"))
                continue
            seen[identifier] = FILE_OF[kind]
    return problems


def rule_2_boxes(world: World) -> list[Problem]:
    """Boxes are non-degenerate, and a cell does not poke through the floor above.

    `exterior` cells are exempt from the upper bound: a terrace's ceiling is the sky, and a level
    above it in the schedule does not roof it over. This is the same distinction that
    `build_collision.py` had to learn the hard way -- it gave every cell a ceiling slab and roofed
    the terrace, and the defect only surfaced when `build_skyexposure.py` cast a ray from a patio.
    """
    problems = []
    for index, cell in enumerate(world.cells):
        where = f"cells/{index}"
        cell_id = cell.get("id")
        level = world.level_by_id.get(cell.get("level"))
        if level is None:
            continue  # rule 6 reports the dangling level reference; do not report it twice
        for box_index, box in enumerate(cell.get("boxes", [])):
            try:
                x0, x1 = float(box["x"][0]), float(box["x"][1])
                z0, z1 = float(box["z"][0]), float(box["z"][1])
            except (KeyError, IndexError, TypeError, ValueError):
                continue  # a shape problem, and the shape gate already said so
            if not x0 < x1 or not z0 < z1:
                problems.append(Problem(2, FILE_OF["cells"], f"{where}/boxes/{box_index}",
                                        f"cell {cell_id}: box x {[x0, x1]} z {[z0, z1]} is "
                                        f"degenerate or inverted; ranges are [min, max] "
                                        f"with min < max"))
        extent = world.extent(cell)
        if extent is None:
            problems.append(Problem(2, FILE_OF["cells"], f"{where}/yOverride",
                                    f"cell {cell_id} is on level {cell.get('level')}, whose "
                                    f"ceiling is null (rafter-bounded), so it must declare its "
                                    f"own yOverride"))
            continue
        low, high = extent
        ffl = float(level.get("ffl", 0.0))
        floor_underside = ffl - float(level.get("structureDepth", 0.0))
        # The same question as the ceiling bound, the other way up: a cell may sit below its
        # level's floor structure where there is no storey underneath to sink into. The garage is
        # a slab-on-grade wing at +0.15 with nothing but ground beneath it, and a bound taken from
        # a basement that stops short of it is a bound about a different building.
        if (low < floor_underside - 1e-9
                and world.covered_from_below(cell, str(level.get("id")))):
            problems.append(Problem(2, FILE_OF["cells"], f"{where}/yOverride",
                                    f"cell {cell_id} starts at y {low:.3f}, below level "
                                    f"{level.get('id')}'s slab underside {floor_underside:.3f}, "
                                    f"and there is a storey under it"))
        if cell.get("kind") in ("exterior", "stair", "void"):
            # A terrace's ceiling is the sky; a stair cell pierces the slab it climbs through, and
            # a void is the hole it climbs through. All three legitimately reach past the level
            # above's slab underside, and forbidding it would leave no way to author a stair.
            continue
        limit = world.level_ceiling_limit(str(level.get("id")))
        # ...and neither is there anything to poke into where the storey above does not reach. The
        # garage is a single-storey wing with a 4.15 m bay under a roof of its own, and a bound
        # taken from a level that stops short of it is a bound about a different building.
        if limit is not None and not world.covered_from_above(cell, str(level.get("id"))):
            limit = None
        if limit is not None and high > limit + 1e-9:
            problems.append(Problem(2, FILE_OF["cells"], f"{where}/yOverride",
                                    f"cell {cell_id} reaches y {high:.3f}, through the slab "
                                    f"underside {limit:.3f} of the level above"))
    return problems


def rule_3_overlap(world: World) -> list[Problem]:
    """No two cells on a level overlap by more than 1 cm² -- unless one is nested in the other.

    Two guards, and neither is a weakening of §15.7.

    **Vertical.** Two cells on the same level legitimately share a footprint when one carries a
    `yOverride`: a stair void open to the floor below sits over the room it looks into, and both
    are cells on the level named in their rows. Without the guard the rule would forbid the one
    arrangement the format has a field for.

    **Nesting.** §54: "a container is a tiny sub-cell with its own portal, so this falls out of the
    visibility system rather than being a special case", and §56.1 names two of them --
    `CELL_FRIDGE_INTERIOR` and `CELL_FREEZER_INTERIOR`. A sub-cell is inside its parent by
    construction. It has to **declare** the parent, because a room accidentally drawn inside
    another looks identical from here; and declaring it is not a way of switching the rule off,
    because a declared sub-cell is then checked to be inside its parent in all three axes, and to
    be only one level deep.
    """
    problems = []
    entries = []
    for index, cell in enumerate(world.cells):
        extent = world.extent(cell)
        if extent is None:
            continue
        entries.append((index, cell, boxes_of(cell), extent))

    # A sub-cell IS inside its parent, and says so. §54: "a container is a tiny sub-cell with its
    # own portal, so this falls out of the visibility system rather than being a special case."
    # The nesting is declared rather than inferred, because a room accidentally drawn inside
    # another looks exactly the same from here -- and it is checked below, so declaring it is not
    # a way of switching the rule off.
    for index, cell in enumerate(world.cells):
        parent_id = cell.get("parent")
        if parent_id is None:
            continue
        parent = world.cell_by_id.get(parent_id)
        if parent is None:
            continue  # rule 6
        if parent.get("parent") is not None:
            problems.append(Problem(
                3, FILE_OF["cells"], f"cells/{index}/parent",
                f"cell {cell.get('id')} nests in {parent_id}, which is itself nested; a container "
                f"inside a container is a depth the visibility solver does not walk"))
        inside = all(
            any(px0 - 1e-6 <= x0 and x1 <= px1 + 1e-6 and pz0 - 1e-6 <= z0 and z1 <= pz1 + 1e-6
                for px0, px1, pz0, pz1 in boxes_of(parent))
            for x0, x1, z0, z1 in boxes_of(cell))
        if not inside:
            problems.append(Problem(
                3, FILE_OF["cells"], f"cells/{index}/boxes",
                f"cell {cell.get('id')} says it nests in {parent_id} and is not inside it; a "
                f"sub-cell that pokes out of its parent is two rooms overlapping by another name"))
        child_extent = world.extent(cell)
        parent_extent = world.extent(parent)
        if (child_extent is not None and parent_extent is not None
                and (child_extent[0] < parent_extent[0] - 1e-6
                     or child_extent[1] > parent_extent[1] + 1e-6)):
            problems.append(Problem(
                3, FILE_OF["cells"], f"cells/{index}/yOverride",
                f"cell {cell.get('id')} nests in {parent_id} and reaches outside its vertical "
                f"extent {parent_extent[0]:.2f}..{parent_extent[1]:.2f}"))

    def nested(a: dict, b: dict) -> bool:
        return a.get("parent") == b.get("id") or b.get("parent") == a.get("id")

    for i in range(len(entries)):
        index_a, cell_a, boxes_a, (low_a, high_a) = entries[i]
        for j in range(i + 1, len(entries)):
            index_b, cell_b, boxes_b, (low_b, high_b) = entries[j]
            if cell_a.get("level") != cell_b.get("level"):
                continue
            if nested(cell_a, cell_b):
                continue
            if overlap((low_a, high_a), (low_b, high_b)) <= 0.0:
                continue
            area = 0.0
            for ax0, ax1, az0, az1 in boxes_a:
                for bx0, bx1, bz0, bz1 in boxes_b:
                    area += overlap((ax0, ax1), (bx0, bx1)) * overlap((az0, az1), (bz0, bz1))
            if area > OVERLAP_AREA_TOLERANCE:
                problems.append(Problem(
                    3, FILE_OF["cells"], f"cells/{index_a}",
                    f"cell {cell_a.get('id')} overlaps {cell_b.get('id')} (cells/{index_b}) on "
                    f"level {cell_a.get('level')} by {area * 1e4:.1f} cm², over the 1 cm² "
                    f"tolerance, and they share vertical space "
                    f"y {max(low_a, low_b):.2f}..{min(high_a, high_b):.2f}"))
    return problems


def _boundary_span(cell: dict, axis: str, value: float,
                   ) -> list[tuple[float, float]]:
    """The spans of the *other* horizontal axis where `cell` has a face on the plane `axis=value`.

    A cell is a union of boxes, so a wall on one plane can be several disjoint runs. Returning them
    all is what lets rule 4 accept a portal that lies in one run of an L-shaped room and reject one
    that lies in the gap between two runs.
    """
    spans = []
    for x0, x1, z0, z1 in boxes_of(cell):
        if axis == "x":
            if abs(x0 - value) <= PLANE_TOLERANCE or abs(x1 - value) <= PLANE_TOLERANCE:
                spans.append((z0, z1))
        else:
            if abs(z0 - value) <= PLANE_TOLERANCE or abs(z1 - value) <= PLANE_TOLERANCE:
                spans.append((x0, x1))
    return spans


def _horizontal_problems(world: World, portal: dict, index: int,
                         value: float, rect: tuple[float, float, float, float],
                         ) -> list[Problem]:
    """Rule 4 for a portal in a HORIZONTAL plane -- a stair well or a hatch.

    `stair_well` and `hatch` are in the portal vocabulary and neither is a hole in a wall, so
    `plane.axis` accepts `y` as well as `x` and `z`. On a horizontal plane `u` is world X and `v`
    is world Z (both horizontal), the rectangle must lie inside **both** cells' footprints, and the
    plane must be the boundary the two cells actually share: one's ceiling is the other's floor.
    """
    where = f"portals/{index}"
    portal_id = portal.get("id")
    u0, u1, v0, v1 = rect
    problems = []
    for side in ("cellA", "cellB"):
        cell = world.cell_by_id.get(portal.get(side))
        if cell is None:
            continue
        boxes = boxes_of(cell)
        corners = [(u0, v0), (u0, v1), (u1, v0), (u1, v1)]
        if not all(point_in_boxes(x, z, boxes, margin=PLANE_TOLERANCE) for x, z in corners):
            problems.append(Problem(
                4, FILE_OF["portals"], f"{where}/rect",
                f"portal {portal_id}: the opening x {[u0, u1]} z {[v0, v1]} is not inside cell "
                f"{cell.get('id')} ({side})'s footprint"))
        extent = world.extent(cell)
        if extent is None:
            continue
        if min(abs(extent[0] - value), abs(extent[1] - value)) > PLANE_TOLERANCE:
            problems.append(Problem(
                4, FILE_OF["portals"], f"{where}/plane",
                f"portal {portal_id}: y = {value:.3f} is neither the floor nor the ceiling of "
                f"cell {cell.get('id')} ({side}), whose extent is "
                f"{extent[0]:.3f}..{extent[1]:.3f}"))
    return problems


def rule_4_portal_planes(world: World) -> list[Problem]:
    """A portal's rectangle lies in both cells' boundary planes, and inside both extents."""
    problems = []
    for index, portal in enumerate(world.portals):
        where = f"portals/{index}"
        portal_id = portal.get("id")
        plane = portal.get("plane") or {}
        axis, value = plane.get("axis"), plane.get("value")
        rect = portal.get("rect") or {}
        if axis not in ("x", "y", "z") or not isinstance(value, (int, float)):
            continue  # shape
        try:
            u0, u1 = float(rect["u"][0]), float(rect["u"][1])
            v0, v1 = float(rect["v"][0]), float(rect["v"][1])
        except (KeyError, IndexError, TypeError, ValueError):
            continue
        if not u0 < u1 or not v0 < v1:
            problems.append(Problem(4, FILE_OF["portals"], f"{where}/rect",
                                    f"portal {portal_id}: rect u {[u0, u1]} v {[v0, v1]} is "
                                    f"degenerate or inverted"))
            continue
        # A portal into a container sub-cell is not in a shared WALL: the sub-cell is inside its
        # parent, so the opening lies in the sub-cell's own face -- the fridge door, the chest lid
        # -- and the parent has no face there at all. Checked against the child, plus that the
        # opening is inside the parent's volume, which is the same statement one nesting deeper.
        cell_a = world.cell_by_id.get(portal.get("cellA"))
        cell_b = world.cell_by_id.get(portal.get("cellB"))
        if cell_a is not None and cell_b is not None:
            child = None
            if cell_a.get("parent") == cell_b.get("id"):
                child, parent = cell_a, cell_b
            elif cell_b.get("parent") == cell_a.get("id"):
                child, parent = cell_b, cell_a
            if child is not None:
                problems.extend(_nested_problems(world, portal, index, child, parent,
                                                 str(axis), float(value), (u0, u1, v0, v1)))
                continue

        if axis == "y":
            problems.extend(_horizontal_problems(world, portal, index, float(value),
                                                 (u0, u1, v0, v1)))
            continue
        for side in ("cellA", "cellB"):
            cell = world.cell_by_id.get(portal.get(side))
            if cell is None:
                continue  # rule 6
            spans = _boundary_span(cell, str(axis), float(value))
            if not spans:
                problems.append(Problem(
                    4, FILE_OF["portals"], f"{where}/plane",
                    f"portal {portal_id}: cell {cell.get('id')} ({side}) has no face on "
                    f"{axis} = {float(value):.3f} within {PLANE_TOLERANCE * 100:.0f} cm"))
                continue
            if not any(span[0] - PLANE_TOLERANCE <= u0 and u1 <= span[1] + PLANE_TOLERANCE
                       for span in spans):
                problems.append(Problem(
                    4, FILE_OF["portals"], f"{where}/rect/u",
                    f"portal {portal_id}: u {[u0, u1]} is not inside any run of cell "
                    f"{cell.get('id')}'s face on {axis} = {float(value):.3f} "
                    f"(runs {[[round(a, 3), round(b, 3)] for a, b in spans]})"))
            extent = world.extent(cell)
            if extent is None:
                continue
            low, high = extent
            if v0 < low - PLANE_TOLERANCE or v1 > high + PLANE_TOLERANCE:
                problems.append(Problem(
                    4, FILE_OF["portals"], f"{where}/rect/v",
                    f"portal {portal_id}: v {[v0, v1]} is outside cell {cell.get('id')}'s "
                    f"vertical extent {low:.3f}..{high:.3f}"))
    return problems


def _nested_problems(world: World, portal: dict, index: int, child: dict, parent: dict,
                     axis: str, value: float, rect: tuple[float, float, float, float],
                     ) -> list[Problem]:
    """Rule 4 for a portal between a container sub-cell and the cell it nests in."""
    where = f"portals/{index}"
    portal_id = portal.get("id")
    u0, u1, v0, v1 = rect
    problems = []

    if axis == "y":
        extent = world.extent(child)
        if extent is not None and min(abs(extent[0] - value), abs(extent[1] - value)) > PLANE_TOLERANCE:
            problems.append(Problem(
                4, FILE_OF["portals"], f"{where}/plane",
                f"portal {portal_id}: y = {value:.3f} is neither the floor nor the lid of "
                f"{child.get('id')}, whose extent is {extent[0]:.3f}..{extent[1]:.3f}"))
        if not all(point_in_boxes(x, z, boxes_of(child), margin=PLANE_TOLERANCE)
                   for x, z in ((u0, v0), (u0, v1), (u1, v0), (u1, v1))):
            problems.append(Problem(
                4, FILE_OF["portals"], f"{where}/rect",
                f"portal {portal_id}: the opening is not inside {child.get('id')}'s footprint"))
        return problems

    runs = _boundary_span(child, axis, value)
    if not runs:
        problems.append(Problem(
            4, FILE_OF["portals"], f"{where}/plane",
            f"portal {portal_id}: {child.get('id')} nests in {parent.get('id')}, so the opening "
            f"is in ITS face -- and it has none on {axis} = {value:.3f} within "
            f"{PLANE_TOLERANCE * 100:.0f} cm"))
    elif not any(run[0] - PLANE_TOLERANCE <= u0 and u1 <= run[1] + PLANE_TOLERANCE
                 for run in runs):
        problems.append(Problem(
            4, FILE_OF["portals"], f"{where}/rect/u",
            f"portal {portal_id}: the opening is not inside {child.get('id')}'s face on that "
            f"plane"))

    extent = world.extent(child)
    if extent is not None and (v0 < extent[0] - PLANE_TOLERANCE or v1 > extent[1] + PLANE_TOLERANCE):
        problems.append(Problem(
            4, FILE_OF["portals"], f"{where}/rect/v",
            f"portal {portal_id}: the opening is taller than {child.get('id')}"))
    return problems


def rule_5_connected(world: World) -> list[Problem]:
    """Every cell is reachable from `L0_FOYER` through always-open or door portals.

    §15.7 rule 5 and `docs/world-format.md` both say **interior** cell, and `HOUSE-00374` found
    what that misses: `EXT_SHED` is an `exterior` cell that is indoors -- roofed, `yOverride`
    [0.0, 2.35], `visibilityHint: opaque` -- and it had no portal at all. A building you cannot
    enter is the same defect as a room you cannot enter, and the word "interior" was the only
    reason the rule could not see it. So the walk now covers every cell that is not `void`.

    Nothing is exempted, `EXT_WORLD` included. It is §16.4 step 4's fallback and it would have
    been defensible to exclude it, but it does not need excluding: it is where the road runs off
    the map, so it has a portal for the same reason every other exterior cell does, and a rule
    with no exceptions is one fewer place for the next unreachable cell to hide.

    The rule needs a graph to walk, and a layout under construction does not have one yet: the
    cells are authored a level at a time (`HOUSE-00367`…`HOUSE-00372`) and the portals come after
    them (`HOUSE-00374`…). Until `layout.portals.json` exists the data has made no connectivity
    claim, so there is nothing here to be right or wrong about and the rule stands down. That is
    the file's presence, not its contents: an **empty** portals file is a claim, and a wrong one.

    It does **not** stand down once portals exist. A portals file with no `L0_FOYER` is somebody
    who authored a graph and no front door, and that is exactly the mistake this rule is for.
    """
    problems = []
    walkable = {cell.get("id") for cell in world.cells if cell.get("kind") != "void"}
    if not walkable:
        return problems
    if "portals" not in world.layout:
        return problems
    if ROOT_CELL not in world.cell_by_id:
        return [Problem(5, FILE_OF["cells"], "cells",
                        f"there is no {ROOT_CELL} and {FILE_OF['portals']} exists; §15.7 rule 5 "
                        f"walks the graph from it, so a portal graph without it is a house with "
                        f"no front door")]

    adjacency: dict[str, set[str]] = {}
    for portal in world.portals:
        if portal.get("kind") in IMPASSABLE_PORTAL_KINDS:
            continue
        a, b = portal.get("cellA"), portal.get("cellB")
        if a is None or b is None:
            continue
        adjacency.setdefault(a, set()).add(b)
        adjacency.setdefault(b, set()).add(a)

    reached = {ROOT_CELL}
    frontier = [ROOT_CELL]
    while frontier:
        current = frontier.pop()
        for neighbour in adjacency.get(current, ()):  # noqa: B007
            if neighbour not in reached:
                reached.add(neighbour)
                frontier.append(neighbour)

    for index, cell in enumerate(world.cells):
        cell_id = cell.get("id")
        if cell_id in walkable and cell_id not in reached:
            problems.append(Problem(
                5, FILE_OF["cells"], f"cells/{index}",
                f"cell {cell_id} is not reachable from {ROOT_CELL} through open or door "
                f"portals; it has {len(adjacency.get(cell_id, ()))} passable portal(s)"))
    return problems


def _asset_ids(world: World) -> set[str]:
    out = set()
    for row in world.assets:
        for key in ("id", "assetId"):
            value = row.get(key)
            if isinstance(value, str):
                out.add(value)
    return out


def rule_6_references(world: World) -> list[Problem]:
    """Every referenced material, asset, light group, sound, nav region and cell exists.

    A reference to a kind whose file is not loaded is **not** reported. A tool run over three of
    the sixteen files would otherwise report every asset id in the layout as missing, which is
    noise that trains an author to ignore the rule.
    """
    problems = []
    cells = set(world.cell_by_id)
    materials = {row.get("id") for row in world.materials}
    portals = set(world.portal_by_id)
    assets = _asset_ids(world)
    light_groups = {row.get("group") for row in world.lights}
    interactables = {row.get("id") for row in world.interactables}
    nav_regions = {cell.get("navMeshRegion") for cell in world.cells
                   if cell.get("navMeshRegion")}

    def check(kind: str, index: int, field: str, value, universe: set,
              universe_name: str, loaded: bool) -> None:
        if value is None or not loaded:
            return
        if value not in universe:
            _, key = layout_io.FILES[kind]
            problems.append(Problem(
                6, FILE_OF[kind], f"{key}/{index}/{field}",
                f"{field} {value!r} is not a known {universe_name}"))

    have_materials = "materials" in world.layout
    have_assets = "assets" in world.layout
    have_cells = "cells" in world.layout
    have_portals = "portals" in world.layout
    have_interactables = "interactables" in world.layout

    for index, cell in enumerate(world.cells):
        for field in ("floorMaterial", "wallMaterial", "ceilingMaterial"):
            check("cells", index, field, cell.get(field), materials, "material", have_materials)
        for group_index, group in enumerate(cell.get("lightGroups", []) or []):
            check("cells", index, f"lightGroups/{group_index}", group, light_groups,
                  "light group (no light declares it)", "lights" in world.layout)

    for index, portal in enumerate(world.portals):
        for field in ("cellA", "cellB"):
            check("portals", index, field, portal.get(field), cells, "cell", have_cells)

    for index, opening in enumerate(world.openings):
        check("openings", index, "portal", opening.get("portal"), portals, "portal", have_portals)
        check("openings", index, "material", opening.get("material"), materials, "material",
              have_materials)
        check("openings", index, "asset", opening.get("asset"), assets, "asset", have_assets)

    for index, flight in enumerate(world.flights):
        for field in ("fromCell", "toCell"):
            check("stairs", index, field, flight.get(field), cells, "cell", have_cells)

    for index, light in enumerate(world.lights):
        check("lights", index, "cell", light.get("cell"), cells, "cell", have_cells)

    for index, prop in enumerate(world.props):
        check("props", index, "cell", prop.get("cell"), cells, "cell", have_cells)
        check("props", index, "asset", prop.get("asset"), assets, "asset", have_assets)
        check("props", index, "material", prop.get("material"), materials, "material",
              have_materials)
        check("props", index, "interactable", prop.get("interactable"), interactables,
              "interactable", have_interactables)

    for index, node in enumerate(world.nav_nodes):
        check("nav", index, "cell", node.get("cell"), cells, "cell", have_cells)

    for index, zone in enumerate(world.audio_zones):
        check("audio", index, "cell", zone.get("cell"), cells, "cell", have_cells)

    for index, item in enumerate(world.interactables):
        check("interactables", index, "cell", item.get("cell"), cells, "cell", have_cells)
        check("interactables", index, "portal", item.get("portal"), portals, "portal",
              have_portals)

    if nav_regions and "nav" in world.layout:
        declared = {node.get("kind") for node in world.nav_nodes}
        for index, cell in enumerate(world.cells):
            region = cell.get("navMeshRegion")
            if region and region not in declared and region not in cells:
                problems.append(Problem(
                    6, FILE_OF["cells"], f"cells/{index}/navMeshRegion",
                    f"navMeshRegion {region!r} names neither a cell nor a nav node kind"))
    return problems


def rule_7_openings(world: World) -> list[Problem]:
    """Every door has exactly one portal and every window has exactly one portal.

    Both directions. A door with no portal is a leaf that swings in a solid wall; a portal claimed
    by two doors is two leaves in one hole, and neither of those can be seen by looking at one row.

    The rule needs both files. Portals are authored before leaves (`HOUSE-00375` then
    `HOUSE-00378`), so it stands down until `layout.openings.json` exists -- the same arrangement
    rule 5 makes for the portals file, and for the same reason: reporting every door in the house
    against a file nobody has written yet is a gate somebody turns off.
    """
    problems = []
    # The bijection is a statement about two files, and a layout under construction has only one:
    # `HOUSE-00375` authors the door portals and `HOUSE-00378` the leaves. Until
    # `layout.openings.json` exists the data has claimed nothing about leaves, and reporting every
    # door in the house for a file nobody has written yet is a gate somebody turns off.
    if "openings" not in world.layout:
        return problems

    claimed: dict[str, list[str]] = {}
    for index, opening in enumerate(world.openings):
        portal_id = opening.get("portal")
        if not isinstance(portal_id, str):
            continue
        claimed.setdefault(portal_id, []).append(str(opening.get("id")))
        if len(claimed[portal_id]) > 1:
            problems.append(Problem(
                7, FILE_OF["openings"], f"openings/{index}/portal",
                f"portal {portal_id} is already claimed by opening "
                f"{claimed[portal_id][0]}; a portal carries exactly one leaf"))

    for index, portal in enumerate(world.portals):
        kind = portal.get("kind")
        if kind not in ("door", "double_door", "window", "exterior_door", "slider"):
            continue
        portal_id = portal.get("id")
        if portal_id not in claimed:
            problems.append(Problem(
                7, FILE_OF["portals"], f"portals/{index}",
                f"portal {portal_id} is a {kind} and no row in "
                f"{FILE_OF['openings']} declares its leaf"))
    return problems


def rule_8_stairs(world: World) -> list[Problem]:
    """A flight's risers add up to the difference between the floors it connects, to within 1 mm."""
    problems = []
    for index, flight in enumerate(world.flights):
        where = f"flights/{index}"
        flight_id = flight.get("id")
        source = world.cell_by_id.get(flight.get("fromCell"))
        target = world.cell_by_id.get(flight.get("toCell"))
        if source is None or target is None:
            continue  # rule 6
        source_level = world.level_by_id.get(source.get("level"))
        target_level = world.level_by_id.get(target.get("level"))
        if source_level is None or target_level is None:
            continue
        try:
            risers = int(flight["risers"])
            rise = float(flight["rise"])
        except (KeyError, TypeError, ValueError):
            continue
        climbed = risers * rise
        wanted = float(target_level.get("ffl", 0.0)) - float(source_level.get("ffl", 0.0))
        if abs(climbed - wanted) > RISE_TOLERANCE:
            problems.append(Problem(
                8, FILE_OF["stairs"], where,
                f"flight {flight_id}: {risers} risers x {rise:.4f} m = {climbed:.4f} m, but "
                f"{source_level.get('id')} to {target_level.get('id')} is {wanted:.4f} m "
                f"(out by {abs(climbed - wanted) * 1000:.1f} mm, tolerance "
                f"{RISE_TOLERANCE * 1000:.0f} mm)"))
    return problems


def _stacks(world: World) -> dict[str, dict]:
    plumbing = (world.layout.get("levels") or {}).get("plumbing") or {}
    return {row.get("id"): row for row in plumbing.get("stacks", [])}


def rule_9_plumbing(world: World) -> list[Problem]:
    """Every plumbing fixture's cell appears in a declared stack, and every stack is plausible.

    §12.5's stacks are a first-class reason several rooms are where they are, so the rule checks
    the stack too, not only the fixture: its cells must exist, be on distinct levels, and each
    must overlap the chase the stack declares. A stack whose three WCs are not actually above one
    another is a drawing, not a drain.
    """
    problems = []
    if "levels" not in world.layout:
        return problems
    stacks = _stacks(world)

    plumbing = (world.layout.get("levels") or {}).get("plumbing") or {}
    for index, stack in enumerate(plumbing.get("stacks", [])):
        where = f"plumbing/stacks/{index}"
        stack_id = stack.get("id")
        chase = stack.get("chase") or {}
        try:
            cx = (float(chase["x"][0]), float(chase["x"][1]))
            cz = (float(chase["z"][0]), float(chase["z"][1]))
        except (KeyError, IndexError, TypeError, ValueError):
            continue
        drop_to = stack.get("dropTo")
        if drop_to is not None and "cells" in world.layout \
                and drop_to not in world.cell_by_id:
            problems.append(Problem(
                9, FILE_OF["levels"], f"{where}/dropTo",
                f"stack {stack_id} drops to {drop_to!r}, which is not a cell; §12.5 requires "
                f"every stack to land on a basement drain run"))
        levels_used: dict[str, str] = {}
        for cell_index, cell_id in enumerate(stack.get("cells", [])):
            cell = world.cell_by_id.get(cell_id)
            if cell is None:
                if "cells" in world.layout:
                    problems.append(Problem(
                        9, FILE_OF["levels"], f"{where}/cells/{cell_index}",
                        f"stack {stack_id} names cell {cell_id!r}, which does not exist"))
                continue
            level_id = str(cell.get("level"))
            if level_id in levels_used:
                problems.append(Problem(
                    9, FILE_OF["levels"], f"{where}/cells/{cell_index}",
                    f"stack {stack_id} has both {levels_used[level_id]} and {cell_id} on level "
                    f"{level_id}; a stack rises through the levels, one cell each"))
            levels_used[level_id] = str(cell_id)
            area = sum(overlap((x0, x1), cx) * overlap((z0, z1), cz)
                       for x0, x1, z0, z1 in boxes_of(cell))
            if area <= OVERLAP_AREA_TOLERANCE:
                problems.append(Problem(
                    9, FILE_OF["levels"], f"{where}/cells/{cell_index}",
                    f"stack {stack_id}: cell {cell_id} does not overlap the chase "
                    f"x {[round(v, 2) for v in cx]} z {[round(v, 2) for v in cz]}, so it is not "
                    f"above the drop"))

    for index, prop in enumerate(world.props):
        stack_id = prop.get("plumbing")
        if stack_id is None:
            continue
        if stack_id not in stacks:
            problems.append(Problem(
                9, FILE_OF["props"], f"props/{index}/plumbing",
                f"prop {prop.get('id')} drains to {stack_id!r}, which is not a declared stack"))
            continue
        members = set(stacks[stack_id].get("cells", []))
        if prop.get("cell") not in members:
            problems.append(Problem(
                9, FILE_OF["props"], f"props/{index}/plumbing",
                f"prop {prop.get('id')} is in cell {prop.get('cell')}, which is not a member of "
                f"stack {stack_id} ({', '.join(sorted(members)) or 'no cells'})"))
    return problems


def rule_10_realism(world: World) -> list[Problem]:
    """The dimensional checks of §70.5 that the layout alone decides.

    Deliberately not here: counter, table, seat, sill, switch and socket heights, and human, pet
    and car scale. Every one of those is a property of an *asset*, measured by `scale_check.py`
    over the `.glb` and joined to the layout by `HOUSE-00360`. A prop row is a position and an
    asset id; it does not know how tall the thing is.
    """
    problems = []

    for index, opening in enumerate(world.openings):
        if opening.get("kind") != "door":
            continue
        portal = world.portal_by_id.get(opening.get("portal"))
        if portal is not None and portal.get("kind") in ("exterior_door", "garage_door", "hatch"):
            continue  # §70.5's leaf range is the interior door's
        leaf = opening.get("leaf") or {}
        for field, (low, high) in (("height", DOOR_HEIGHT), ("width", DOOR_WIDTH)):
            value = leaf.get(field)
            if not isinstance(value, (int, float)):
                continue
            if not low - 1e-9 <= float(value) <= high + 1e-9:
                problems.append(Problem(
                    10, FILE_OF["openings"], f"openings/{index}/leaf/{field}",
                    f"door {opening.get('id')}: leaf {field} {float(value):.3f} m is outside "
                    f"§70.5's {low:.2f}–{high:.2f} m for an interior door"))

    for index, cell in enumerate(world.cells):
        if cell.get("kind") not in ("room", "corridor"):
            continue
        # §70.5's 2.35–3.10 m is a range for a room with a **flat** ceiling. A rafter-bounded
        # level has none: §13.6's attic room runs 2.4 m at the knee wall to 5.0 m at the ridge, and
        # a cell there is a bounding volume rather than a height. Checking it would report every
        # attic room in every house ever built.
        level = world.level_by_id.get(cell.get("level"))
        if level is not None and level.get("ceiling") is None:
            continue
        extent = world.extent(cell)
        if extent is None:
            continue
        clear = extent[1] - extent[0]
        low, high = CEILING_CLEAR
        if not low - 1e-9 <= clear <= high + 1e-9:
            problems.append(Problem(
                10, FILE_OF["cells"], f"cells/{index}",
                f"cell {cell.get('id')}: clear height {clear:.3f} m is outside §70.5's "
                f"{low:.2f}–{high:.2f} m for a habitable room"))

    for index, flight in enumerate(world.flights):
        try:
            rise = float(flight["rise"])
            going = float(flight["going"])
        except (KeyError, TypeError, ValueError):
            continue
        blondel = 2.0 * rise + going
        low, high = STAIR_2R_G
        if not low - 1e-9 <= blondel <= high + 1e-9:
            problems.append(Problem(
                10, FILE_OF["stairs"], f"flights/{index}",
                f"flight {flight.get('id')}: 2·{rise:.4f} + {going:.4f} = {blondel * 1000:.1f} mm, "
                f"outside §70.5's {low * 1000:.0f}–{high * 1000:.0f} mm"))

    for index, portal in enumerate(world.portals):
        if portal.get("kind") in IMPASSABLE_PORTAL_KINDS or portal.get("crouch"):
            continue
        if portal.get("kind") == "hatch":
            continue  # a hatch is crouched through by definition
        rect = portal.get("rect") or {}
        try:
            width = float(rect["u"][1]) - float(rect["u"][0])
            height = float(rect["v"][1]) - float(rect["v"][0])
        except (KeyError, IndexError, TypeError, ValueError):
            continue
        if (portal.get("plane") or {}).get("axis") == "y":
            # A horizontal portal is climbed through, not walked through: both of its dimensions
            # are horizontal, so the capsule's WIDTH is the constraint twice over and its height
            # is not a constraint at all.
            narrow = min(width, height)
            if narrow < CAPSULE_WIDTH - 1e-9:
                problems.append(Problem(
                    10, FILE_OF["portals"], f"portals/{index}/rect",
                    f"portal {portal.get('id')}: {narrow:.3f} m across at its narrowest, under "
                    f"the player capsule's {CAPSULE_WIDTH:.2f} m"))
            continue
        if width < CAPSULE_WIDTH - 1e-9:
            problems.append(Problem(
                10, FILE_OF["portals"], f"portals/{index}/rect/u",
                f"portal {portal.get('id')}: {width:.3f} m wide, under the player capsule's "
                f"{CAPSULE_WIDTH:.2f} m, and it is not marked crouch"))
        if height < CAPSULE_HEIGHT - 1e-9:
            problems.append(Problem(
                10, FILE_OF["portals"], f"portals/{index}/rect/v",
                f"portal {portal.get('id')}: {height:.3f} m high, under the player capsule's "
                f"{CAPSULE_HEIGHT:.2f} m, and it is not marked crouch"))
    return problems


def _passable_neighbours(world: World, cell_id: str) -> list[tuple[str, dict]]:
    """`(neighbour cell id, portal)` for every portal a person can pass through."""
    out = []
    for portal in world.portals:
        if portal.get("kind") in IMPASSABLE_PORTAL_KINDS:
            continue
        a, b = portal.get("cellA"), portal.get("cellB")
        if a == cell_id and isinstance(b, str):
            out.append((b, portal))
        elif b == cell_id and isinstance(a, str):
            out.append((a, portal))
    return out


def _crosses_portal(eye: tuple[float, float, float], focus: tuple[float, float, float],
                    portal: dict) -> bool:
    """Does the segment leave one cell for the other THROUGH the hole, rather than the wall?

    A horizontal portal is accepted without this test: a stair well is not something an arm
    reaches through, so an interactable that needs one is a mistake rule 11 would report for the
    wrong reason.
    """
    plane = portal.get("plane") or {}
    axis, value = plane.get("axis"), plane.get("value")
    rect = portal.get("rect") or {}
    if axis == "y":
        return False
    try:
        value = float(value)
        u0, u1 = float(rect["u"][0]), float(rect["u"][1])
        v0, v1 = float(rect["v"][0]), float(rect["v"][1])
    except (KeyError, IndexError, TypeError, ValueError):
        return False
    index = 0 if axis == "x" else 2
    other = 2 if axis == "x" else 0
    a, b = eye[index], focus[index]
    if abs(b - a) < 1e-12:
        return False
    t = (value - a) / (b - a)
    if not 0.0 <= t <= 1.0:
        return False
    u = eye[other] + (focus[other] - eye[other]) * t
    v = eye[1] + (focus[1] - eye[1]) * t
    return (u0 - PLANE_TOLERANCE <= u <= u1 + PLANE_TOLERANCE
            and v0 - PLANE_TOLERANCE <= v <= v1 + PLANE_TOLERANCE)


def rule_11_reachable(world: World) -> list[Problem]:
    """Every interactable's `focus.point` is in its cell and reachable from a standing eye.

    Three conditions, and each one exists because dropping it lets something absurd through:

    * the focus is inside its cell's footprint and vertical extent -- otherwise the row is simply
      pointing at the wrong room;
    * the eye position is somewhere the player capsule can **stand**, which is what makes this a
      proof rather than a distance measurement: a switch in a 0.4 m slot has floor beneath it and
      nobody who can reach it;
    * the segment from eye to focus stays inside the footprint, and when the eye is in a
      *neighbouring* cell it crosses the shared plane inside the portal rectangle -- through the
      hole, not through the wall.

    The neighbour search is not a generosity, it is the common case. §15.7 says "the room's floor",
    and a shallow closet, a cabinet, a meter cupboard and a serving hatch are all reached from the
    room next door; a rule that searched only the interactable's own cell would reject every one
    of them.

    Props are not obstacles here. A sofa in front of a socket is a dressing question, and putting
    it in this rule would make the validator depend on furniture that has not been placed yet.
    """
    problems = []
    step = 0.25
    for index, item in enumerate(world.interactables):
        where = f"interactables/{index}"
        cell = world.cell_by_id.get(item.get("cell"))
        if cell is None:
            continue  # rule 6
        focus = (item.get("focus") or {}).get("point")
        if not (isinstance(focus, list) and len(focus) == 3):
            continue  # shape
        fx, fy, fz = (float(v) for v in focus)
        home = boxes_of(cell)
        extent = world.extent(cell)
        if extent is None:
            continue
        low, high = extent
        if not point_in_boxes(fx, fz, home):
            problems.append(Problem(
                11, FILE_OF["interactables"], f"{where}/focus/point",
                f"interactable {item.get('id')}: focus ({fx:.2f}, {fz:.2f}) is outside cell "
                f"{cell.get('id')}'s footprint"))
            continue
        if not low - 1e-9 <= fy <= high + 1e-9:
            problems.append(Problem(
                11, FILE_OF["interactables"], f"{where}/focus/point",
                f"interactable {item.get('id')}: focus y {fy:.2f} is outside cell "
                f"{cell.get('id')}'s extent {low:.2f}..{high:.2f}"))
            continue

        candidates: list[tuple[dict, dict | None]] = [(cell, None)]
        for neighbour_id, portal in _passable_neighbours(world, str(cell.get("id"))):
            neighbour = world.cell_by_id.get(neighbour_id)
            if neighbour is not None:
                candidates.append((neighbour, portal))

        best = math.inf
        found = False
        for room, portal in candidates:
            if found:
                break
            room_boxes = boxes_of(room)
            room_extent = world.extent(room)
            if room_extent is None:
                continue
            eye_y = room_extent[0] + EYE_HEIGHT
            union = room_boxes if portal is None else room_boxes + home
            for x0, x1, z0, z1 in room_boxes:
                x = x0 + step / 2
                while x < x1 and not found:
                    z = z0 + step / 2
                    while z < z1:
                        distance = math.dist((x, eye_y, z), (fx, fy, fz))
                        if distance < best:
                            best = distance
                        if (distance <= REACH_RANGE
                                and standable(x, z, room_boxes)
                                and segment_inside(x, z, fx, fz, union)
                                and (portal is None
                                     or _crosses_portal((x, eye_y, z), (fx, fy, fz), portal))):
                            found = True
                            break
                        z += step
                    x += step
        if not found:
            problems.append(Problem(
                11, FILE_OF["interactables"], f"{where}/focus/point",
                f"interactable {item.get('id')}: no position the player can stand in, in cell "
                f"{cell.get('id')} or through a portal from it, has an unobstructed "
                f"{REACH_RANGE:.2f} m reach to its focus; the closest eye position of any kind is "
                f"{best:.2f} m away"))
    return problems


RULES = {
    1: rule_1_ids, 2: rule_2_boxes, 3: rule_3_overlap, 4: rule_4_portal_planes,
    5: rule_5_connected, 6: rule_6_references, 7: rule_7_openings, 8: rule_8_stairs,
    9: rule_9_plumbing, 10: rule_10_realism, 11: rule_11_reachable,
}


# ========================================================================================== run ==


def validate(directory: Path, wanted: list[int] | None = None,
             ) -> tuple[list[str], list[Problem]]:
    """`(shape problems, rule problems)`. A non-empty first list means the second did not run."""
    shape = []
    for name, problems in sorted(world_schema.validate_directory(directory).items()):
        shape.extend(f"{name}:{problem}" for problem in problems)
    if shape:
        return shape, []

    layout = layout_io.load_layout(directory)
    world = World(layout)
    out: list[Problem] = []
    for number in sorted(RULES):
        if wanted is not None and number not in wanted:
            continue
        out.extend(RULES[number](world))
    return [], out


def report(directory: Path, wanted: list[int] | None = None, stream=sys.stdout) -> int:
    shape, problems = validate(directory, wanted)
    if shape:
        print(f"validate_world: {len(shape)} shape problem(s); the eleven rules did not run, "
              f"because a rule cannot read a field that is not the type it says it is.",
              file=stream)
        for line in shape:
            print(f"  {line}", file=stream)
        return 1

    by_rule: dict[int, list[Problem]] = {}
    for problem in problems:
        by_rule.setdefault(problem.rule, []).append(problem)

    for number in sorted(RULES):
        if wanted is not None and number not in wanted:
            continue
        failures = by_rule.get(number, [])
        mark = "ok  " if not failures else "FAIL"
        print(f"  {mark}  rule {number:>2}: {RULE_TITLES[number]}"
              f"{'' if not failures else f'  ({len(failures)})'}", file=stream)
        for problem in failures:
            print(f"          {problem}", file=stream)

    if problems:
        print(f"\nvalidate_world: {len(problems)} problem(s) in {len(by_rule)} rule(s).",
              file=stream)
        return 1
    print(f"\nvalidate_world: {directory} passes all "
          f"{len(RULES) if wanted is None else len(wanted)} rule(s).", file=stream)
    return 0


# ======================================================================================= fixture ==


def fixture() -> dict[str, dict]:
    """A small house that satisfies all eleven rules, and exercises each of them at least once.

    Small enough to hold in the head and real enough to be worth passing: two storeys, a foyer that
    everything is reachable from, a WC stacked over a WC on the drain the plumbing rule wants, a
    stair that climbs exactly one storey through a horizontal `stair_well` portal, an exterior door
    onto a terrace, and a light switch a person standing in the hall can reach.

    The numbers are chosen so that nothing passes by luck: the L0 ceiling is exactly the L1 slab
    underside, so rule 2's bound is met with no slack; 17 risers at 3.05/17 m is exactly the storey
    height, so rule 8 has no rounding to hide in; and `2R + G` comes to 638.8 mm, inside §70.5's
    600-650 but not in the middle of it.
    """
    storey = 3.05
    risers = 17
    rise = storey / risers

    levels = {
        "schema": "cna-house/levels/1", "units": "metres", "north": "-Z",
        "levels": [
            {"id": "L0", "name": "Main Floor", "ffl": 0.60, "ceiling": 3.30,
             "structureDepth": 0.35},
            {"id": "L1", "name": "Upper Floor", "ffl": 3.65, "ceiling": 6.20,
             "structureDepth": 0.35},
        ],
        "construction": {"wallExterior": 0.30, "wallPartition": 0.15},
        "plumbing": {"stacks": [
            {"id": "STACK_A", "cells": ["L0_WC1", "L1_WC4"],
             "chase": {"x": [2.0, 4.0], "z": [4.0, 6.0]}, "dropTo": None},
        ]},
    }

    def cell(identifier, level, kind, boxes, **extra):
        row = {"id": identifier, "level": level, "kind": kind,
               "boxes": [{"x": list(x), "z": list(z)} for x, z in boxes]}
        row.update(extra)
        return row

    cells = {"schema": "cna-house/cells/1", "cells": [
        cell("L0_FOYER", "L0", "room", [((-2.0, 2.0), (0.0, 4.0))],
             floorMaterial="MAT_TILE"),
        cell("L0_HALL", "L0", "corridor", [((-2.0, 2.0), (4.0, 10.0))],
             floorMaterial="MAT_TILE", lightGroups=["LG_HALL"]),
        cell("L0_WC1", "L0", "room", [((2.0, 4.0), (4.0, 6.0))], floorMaterial="MAT_TILE"),
        cell("L0_STAIR", "L0", "stair", [((-6.0, -2.0), (4.0, 8.0))],
             yOverride=[0.60, 3.65]),
        cell("L0_TERRACE", "L0", "exterior", [((-2.0, 2.0), (-4.0, 0.0))]),
        # 0.5 m deep: there is floor in it and nobody can stand on that floor. It is here because
        # a validator that searched only an interactable's own cell would reject every closet,
        # cabinet, meter cupboard and serving hatch in the house.
        cell("L0_CLOSET", "L0", "closet", [((2.0, 2.5), (6.0, 8.0))]),
        cell("L1_HALL", "L1", "corridor", [((-2.0, 2.0), (4.0, 10.0))]),
        cell("L1_WC4", "L1", "room", [((2.0, 4.0), (4.0, 6.0))]),
        cell("L1_LANDING", "L1", "room", [((-6.0, -2.0), (4.0, 8.0))]),
    ]}

    def portal(identifier, a, b, axis, value, u, v, kind, **extra):
        row = {"id": identifier, "cellA": a, "cellB": b,
               "plane": {"axis": axis, "value": value},
               "rect": {"u": list(u), "v": list(v)}, "kind": kind}
        row.update(extra)
        return row

    portals = {"schema": "cna-house/portals/1", "portals": [
        portal("P_FOYER__HALL", "L0_FOYER", "L0_HALL", "z", 4.0,
               (-0.5, 0.5), (0.60, 2.65), "cased_opening"),
        portal("P_HALL__WC1", "L0_HALL", "L0_WC1", "x", 2.0,
               (4.6, 5.5), (0.60, 2.62), "door"),
        portal("P_HALL__STAIR", "L0_HALL", "L0_STAIR", "x", -2.0,
               (5.0, 6.0), (0.60, 2.65), "cased_opening"),
        portal("P_FOYER__TERRACE", "L0_FOYER", "L0_TERRACE", "z", 0.0,
               (-0.5, 0.5), (0.60, 2.65), "exterior_door"),
        # The horizontal one. L0_STAIR's yOverride reaches 3.65, which is L1_LANDING's floor.
        portal("P_STAIR__LANDING", "L0_STAIR", "L1_LANDING", "y", 3.65,
               (-5.5, -2.5), (4.5, 7.5), "stair_well"),
        portal("P_HALL__CLOSET", "L0_HALL", "L0_CLOSET", "x", 2.0,
               (6.4, 7.4), (0.60, 2.65), "cased_opening"),
        portal("P_L1HALL__LANDING", "L1_HALL", "L1_LANDING", "x", -2.0,
               (5.0, 6.0), (3.65, 5.70), "cased_opening"),
        portal("P_L1HALL__WC4", "L1_HALL", "L1_WC4", "x", 2.0,
               (4.6, 5.5), (3.65, 5.67), "door"),
    ]}

    openings = {"schema": "cna-house/openings/1", "openings": [
        {"id": "DOOR_WC1", "kind": "door", "portal": "P_HALL__WC1",
         "leaf": {"width": 0.86, "height": 2.02, "thickness": 0.040},
         "asset": "MODEL_DOOR_LEAF", "material": "MAT_PAINT"},
        {"id": "DOOR_WC4", "kind": "door", "portal": "P_L1HALL__WC4",
         "leaf": {"width": 0.86, "height": 2.02, "thickness": 0.040},
         "asset": "MODEL_DOOR_LEAF", "material": "MAT_PAINT"},
        # An exterior door: §70.5's interior leaf range does not apply, and this row proves the
        # exemption is real by being 2.15 m tall.
        {"id": "DOOR_TERRACE", "kind": "door", "portal": "P_FOYER__TERRACE",
         "leaf": {"width": 0.92, "height": 2.15, "thickness": 0.055},
         "asset": "MODEL_DOOR_LEAF", "material": "MAT_PAINT"},
    ]}

    stairs = {"schema": "cna-house/stairs/1", "flights": [
        {"id": "STAIR_L0_L1", "fromCell": "L0_STAIR", "toCell": "L1_LANDING",
         "risers": risers, "rise": rise, "going": 0.280, "width": 1.20,
         "collisionRamp": True, "surface": "wood"},
    ]}

    lights = {"schema": "cna-house/lights/1", "lights": [
        {"id": "LIGHT_HALL", "cell": "L0_HALL", "group": "LG_HALL", "type": "point",
         "position": [0.0, 3.10, 7.0], "colorK": 2700, "intensityLm": 800.0},
    ]}

    materials = {"schema": "cna-house/materials/1", "materials": [
        {"id": "MAT_TILE", "class": "tile", "effectTierS": "Basic"},
        {"id": "MAT_PAINT", "class": "paint", "effectTierS": "Basic"},
    ]}

    props = {"schema": "cna-house/props/1", "props": [
        {"id": "PROP_WC1_PAN", "asset": "MODEL_WC", "cell": "L0_WC1",
         "position": [3.0, 0.60, 5.0], "plumbing": "STACK_A"},
        {"id": "PROP_WC4_PAN", "asset": "MODEL_WC", "cell": "L1_WC4",
         "position": [3.0, 3.65, 5.0], "plumbing": "STACK_A"},
    ]}

    interactables = {"schema": "cna-house/interactables/1", "interactables": [
        {"id": "SWITCH_HALL", "kind": "light_switch", "cell": "L0_HALL",
         "focus": {"point": [1.90, 1.80, 5.00], "normal": [-1.0, 0.0, 0.0], "radius": 0.05},
         "actions": [{"verb": "Toggle", "do": "toggle(LG_HALL)"}]},
        {"id": "SHELF_CLOSET", "kind": "container", "cell": "L0_CLOSET",
         "focus": {"point": [2.45, 1.20, 7.00], "normal": [-1.0, 0.0, 0.0], "radius": 0.05},
         "actions": [{"verb": "Open", "do": "open(SHELF_CLOSET)"}]},
    ]}

    assets = {"schema": "cna-house/assets/1", "assets": [
        {"id": "MODEL_WC", "file": "Models/Fixtures/wc.glb"},
        {"id": "MODEL_DOOR_LEAF", "file": "Models/Doors/leaf.glb"},
    ]}

    return {"levels": levels, "cells": cells, "portals": portals, "openings": openings,
            "stairs": stairs, "lights": lights, "materials": materials, "props": props,
            "interactables": interactables, "assets": assets}


def write_fixture(directory: Path, documents: dict[str, dict]) -> None:
    import json

    directory.mkdir(parents=True, exist_ok=True)
    for kind, document in documents.items():
        name, _ = layout_io.FILES[kind]
        (directory / name).write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


# ====================================================================================== selftest ==


def selftest() -> int:
    import copy
    import io
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("validate_world: selftest")
    workspace = Path(tempfile.mkdtemp(prefix="validate_world_selftest_"))
    try:
        base = fixture()

        def row(docs: dict, kind: str, identifier: str) -> dict:
            """The row with this id. By id, never by index -- adding a room to the fixture must
            not silently repoint a mutation at a different room."""
            _, key = layout_io.FILES[kind]
            for candidate in docs[kind][key]:
                if candidate.get("id") == identifier:
                    return candidate
            raise AssertionError(f"the fixture has no {kind} {identifier!r}")
        world_dir = workspace / "world"
        write_fixture(world_dir, base)

        # 1. The fixture is a house, and it passes. A validator whose own example fails every run
        #    teaches its author to read past the output.
        shape, problems = validate(world_dir)
        require(not shape, f"the fixture matches every schema ({shape[:2]})")
        require(not problems,
                f"and passes all eleven rules ({[str(p) for p in problems[:3]]})")

        # 2. Every rule is actually exercised by the fixture -- a rule with nothing to look at
        #    passes for the wrong reason. Counted as: the rule reads at least one row.
        world = World(layout_io.load_layout(world_dir))
        require(len(world.cells) == 9 and len(world.portals) == 8 and len(world.openings) == 3,
                f"the fixture has 9 cells, 8 portals, 3 openings "
                f"({len(world.cells)}, {len(world.portals)}, {len(world.openings)})")
        require(any(p["plane"]["axis"] == "y" for p in world.portals),
                "including a HORIZONTAL portal -- a stair_well is in the vocabulary and is not a "
                "hole in a wall")

        # 3. The numbers that are supposed to be exact are exact, so nothing below passes by luck.
        flight = world.flights[0]
        climbed = flight["risers"] * flight["rise"]
        require(abs(climbed - 3.05) < 1e-12,
                f"17 risers climb exactly one storey ({climbed!r} m vs 3.05)")
        blondel = 2 * flight["rise"] + flight["going"]
        require(0.600 <= blondel <= 0.650,
                f"and 2R + G = {blondel * 1000:.1f} mm is inside §70.5's 600-650")
        l0 = world.level_by_id["L0"]
        require(abs(l0["ceiling"] - world.level_ceiling_limit("L0")) < 1e-12,
                "L0's ceiling IS L1's slab underside, so rule 2's bound is met with no slack")

        # 4. One mutation per rule, each caught by ITS OWN rule and by no other. That last half is
        #    the point: a rule that reports its neighbour's problem looks like a working rule right
        #    up to the day the neighbour is switched off.
        mutations = []

        def mutation(rule: int, description: str):
            def register(function):
                mutations.append((rule, description, function))
                return function
            return register

        @mutation(1, "a material id repeated")
        def _(docs):
            docs["materials"]["materials"].append(
                {"id": "MAT_TILE", "class": "stone", "effectTierS": "Basic"})

        @mutation(2, "a hall that reaches through the slab above")
        def _(docs):
            # The hall HAS a room above it -- `L1_HALL` shares its footprint -- which is what makes
            # this a violation and the garage below not one.
            row(docs, "cells", "L0_HALL")["yOverride"] = [0.60, 3.50]

        @mutation(3, "a second cell over the hall's footprint")
        def _(docs):
            docs["cells"]["cells"].append(
                {"id": "L0_GHOST", "level": "L0", "kind": "void",
                 "boxes": [{"x": [-2.0, 2.0], "z": [4.0, 10.0]}]})

        @mutation(4, "a door moved 0.5 m off the wall it is in")
        def _(docs):
            row(docs, "portals", "P_HALL__WC1")["plane"]["value"] = 2.5

        @mutation(5, "the WC's door turned into a window")
        def _(docs):
            row(docs, "portals", "P_HALL__WC1")["kind"] = "window"

        @mutation(6, "a prop pointing at an asset that does not exist")
        def _(docs):
            row(docs, "props", "PROP_WC1_PAN")["asset"] = "MODEL_MISSING"

        @mutation(7, "a door leaf deleted, leaving the portal unclaimed")
        def _(docs):
            docs["openings"]["openings"].remove(row(docs, "openings", "DOOR_WC1"))

        @mutation(8, "one riser fewer than the storey needs")
        def _(docs):
            row(docs, "stairs", "STAIR_L0_L1")["risers"] = 16

        @mutation(9, "a stack dropping to a cell that does not exist")
        def _(docs):
            docs["levels"]["plumbing"]["stacks"][0]["dropTo"] = "B1_NOPE"

        @mutation(10, "a 2.40 m interior door")
        def _(docs):
            row(docs, "openings", "DOOR_WC1")["leaf"]["height"] = 2.40

        @mutation(11, "a light switch outside the room it is in")
        def _(docs):
            row(docs, "interactables", "SWITCH_HALL")["focus"]["point"] = [5.0, 1.80, 5.0]

        require(sorted(rule for rule, _, _ in mutations) == list(range(1, 12)),
                "there is a mutation for each of the eleven rules")

        for rule, description, mutate in mutations:
            docs = copy.deepcopy(base)
            mutate(docs)
            broken = workspace / f"broken-{rule}"
            write_fixture(broken, docs)
            shape, problems = validate(broken)
            hit = sorted({problem.rule for problem in problems})
            require(not shape, f"rule {rule}: {description} is a SEMANTIC error, not a shape one")
            require(hit == [rule],
                    f"rule {rule} catches {description}, and only rule {rule} does "
                    f"(rules that fired: {hit or 'none'})")
            named = [p for p in problems if p.rule == rule]
            require(all(p.file and p.path and len(p.message) > 20 for p in named),
                    f"rule {rule}'s message names a file, a JSON path and what was expected "
                    f"({str(named[0]) if named else 'nothing reported'})")
            shutil.rmtree(broken, ignore_errors=True)

        # 5. Every failure, not the first. conventions.md §5.1.
        docs = copy.deepcopy(base)
        row(docs, "stairs", "STAIR_L0_L1")["risers"] = 16
        row(docs, "openings", "DOOR_WC1")["leaf"]["height"] = 2.40
        row(docs, "props", "PROP_WC1_PAN")["asset"] = "MODEL_MISSING"
        many = workspace / "broken-many"
        write_fixture(many, docs)
        _, problems = validate(many)
        require(sorted({p.rule for p in problems}) == [6, 8, 10],
                f"three unrelated mistakes are all reported in one run "
                f"({sorted({p.rule for p in problems})})")

        # 6. A shape error stops the semantic rules, and says so rather than pretending.
        docs = copy.deepcopy(base)
        row(docs, "cells", "L0_FOYER")["kind"] = "conservatory"
        broken = workspace / "broken-shape"
        write_fixture(broken, docs)
        shape, problems = validate(broken)
        require(shape and not problems,
                f"an unknown cell kind is a shape failure and the rules do not run "
                f"({len(shape)} shape, {len(problems)} rule)")
        stream = io.StringIO()
        code = report(broken, stream=stream)
        require(code == 1 and "did not run" in stream.getvalue(),
                "and the report says the rules did not run, instead of printing eleven oks")

        # 7. --rules runs what it is asked for and nothing else.
        docs = copy.deepcopy(base)
        row(docs, "stairs", "STAIR_L0_L1")["risers"] = 16
        subset = workspace / "broken-subset"
        write_fixture(subset, docs)
        _, only_ten = validate(subset, wanted=[10])
        _, only_eight = validate(subset, wanted=[8])
        require(not only_ten and len(only_eight) == 1,
                f"--rules 10 passes and --rules 8 fails on the same stair "
                f"({len(only_ten)}, {len(only_eight)})")

        # 8. The reachability proof is a proof, not a distance check. A focus point 1.5 m away
        #    through the wall of the NEXT room must fail even though 1.5 m < 2.50 m.
        docs = copy.deepcopy(base)
        row(docs, "interactables", "SWITCH_HALL")["cell"] = "L0_WC1"
        row(docs, "interactables", "SWITCH_HALL")["focus"]["point"] = [2.10, 1.80, 5.00]
        near = workspace / "reach-near"
        write_fixture(near, docs)
        _, problems = validate(near, wanted=[11])
        require(not problems,
                f"a switch 0.1 m inside the WC is reachable from inside the WC ({problems})")
        row(docs, "interactables", "SWITCH_HALL")["cell"] = "L0_HALL"
        write_fixture(near, docs)
        _, problems = validate(near, wanted=[11])
        require(len(problems) == 1 and "footprint" in problems[0].message,
                f"and the SAME point fails when the switch claims to be in the hall next door, "
                f"0.1 m away through the wall ({[str(p) for p in problems]})")

        # 9. Rule 5 walks doors, not windows -- and the fixture's terrace proves an exterior cell
        #    is not required to be reachable while an interior one is.
        docs = copy.deepcopy(base)
        row(docs, "cells", "L0_TERRACE")["kind"] = "room"
        row(docs, "portals", "P_FOYER__TERRACE")["kind"] = "window"
        outside = workspace / "broken-outside"
        write_fixture(outside, docs)
        _, problems = validate(outside, wanted=[5])
        require(len(problems) == 1 and "L0_TERRACE" in problems[0].message,
                f"a terrace reclassified as a room, reached only through a window, is "
                f"unreachable ({[str(p) for p in problems]})")

        # 10. Rule 9 checks the stack, not only the fixture on it: §12.5's alignment is the
        #     reason those rooms are where they are.
        docs = copy.deepcopy(base)
        row(docs, "cells", "L1_WC4")["boxes"] = [{"x": [8.0, 10.0], "z": [4.0, 6.0]}]
        moved = workspace / "broken-stack"
        write_fixture(moved, docs)
        _, problems = validate(moved, wanted=[9])
        require(any("chase" in p.message for p in problems),
                f"a WC moved off the chase breaks the stack above it "
                f"({[str(p) for p in problems]})")

        # 11. The horizontal portal is checked, not skipped.
        docs = copy.deepcopy(base)
        row(docs, "portals", "P_STAIR__LANDING")["plane"]["value"] = 4.20
        wrong = workspace / "broken-y"
        write_fixture(wrong, docs)
        _, problems = validate(wrong, wanted=[4])
        require(len(problems) == 2 and all("y = 4.200" in p.message for p in problems),
                f"a stair well at the wrong height is caught for BOTH cells "
                f"({[str(p) for p in problems]})")
        docs = copy.deepcopy(base)
        row(docs, "portals", "P_STAIR__LANDING")["rect"]["u"] = [-5.5, -5.2]
        narrow = workspace / "broken-narrow"
        write_fixture(narrow, docs)
        _, problems = validate(narrow, wanted=[10])
        require(len(problems) == 1 and "narrowest" in problems[0].message,
                f"and a 0.30 m stair well is too narrow for the player capsule "
                f"({[str(p) for p in problems]})")

        # 12. The tool is deterministic: the same directory twice gives the same problems in the
        #     same order, or a diff of two CI runs is unreadable.
        docs = copy.deepcopy(base)
        row(docs, "stairs", "STAIR_L0_L1")["risers"] = 16
        row(docs, "openings", "DOOR_WC1")["leaf"]["height"] = 2.40
        twice = workspace / "broken-twice"
        write_fixture(twice, docs)
        first = [str(p) for p in validate(twice)[1]]
        second = [str(p) for p in validate(twice)[1]]
        require(first == second and len(first) >= 2,
                f"two runs over one directory report the same {len(first)} problem(s) in the "
                f"same order")

        # A portal into a container sub-cell is not in a shared WALL: the sub-cell is inside its
        # parent, so the opening is in the sub-cell's own face -- the fridge door, the chest lid --
        # and the parent has no face there at all.
        def with_container(portal_patch=None, y=False):
            docs = copy.deepcopy(base)
            docs["cells"]["cells"].append(
                {"id": "CELL_FRIDGE_INTERIOR", "level": "L0", "kind": "closet",
                 "parent": "L0_HALL",
                 "boxes": [{"x": [0.0, 1.0], "z": [5.0, 6.0]}],
                 "yOverride": [0.70, 2.45]})
            door = {"id": "P_FRIDGE_INTERIOR", "cellA": "L0_HALL",
                    "cellB": "CELL_FRIDGE_INTERIOR",
                    "plane": {"axis": "z", "value": 6.0},
                    "rect": {"u": [0.0, 1.0], "v": [0.70, 2.45]},
                    "kind": "door", "opacity": "opaque_when_closed", "crouch": True}
            if y:
                door["plane"] = {"axis": "y", "value": 2.45}
                door["rect"] = {"u": [0.0, 1.0], "v": [5.0, 6.0]}
                door["kind"] = "hatch"
            if portal_patch:
                door.update(portal_patch)
            docs["portals"]["portals"].append(door)
            return docs

        fridge = workspace / "fridge"
        write_fixture(fridge, with_container())
        _, problems = validate(fridge, wanted=[4])
        require(not problems,
                f"a portal in a sub-cell's OWN face is accepted, though its parent has no face "
                f"there ({[str(p) for p in problems]})")

        chest = workspace / "chest"
        write_fixture(chest, with_container(y=True))
        _, problems = validate(chest, wanted=[4])
        require(not problems,
                f"and so is a chest lid, which is horizontal ({[str(p) for p in problems]})")

        askew = workspace / "askew"
        write_fixture(askew, with_container({"plane": {"axis": "z", "value": 5.5}}))
        _, problems = validate(askew, wanted=[4])
        require(any("has none on" in p.message for p in problems),
                f"a portal in NEITHER cell's face is still refused "
                f"({[str(p) for p in problems]})")

        tall_door = workspace / "tall-door"
        write_fixture(tall_door, with_container({"rect": {"u": [0.0, 1.0], "v": [0.70, 3.20]}}))
        _, problems = validate(tall_door, wanted=[4])
        require(any("taller than" in p.message for p in problems),
                f"and so is one taller than the container it opens into "
                f"({[str(p) for p in problems]})")

        # ...and the same question the other way up. The stair cell's `yOverride` starts at 0.60,
        # which is L0's floor; a wing whose slab is BELOW its level's floor structure is only wrong
        # where there is a storey underneath to sink into.
        docs = copy.deepcopy(base)
        row(docs, "cells", "L0_CLOSET")["yOverride"] = [0.15, 3.30]
        slab = workspace / "slab"
        write_fixture(slab, docs)
        _, problems = validate(slab, wanted=[2])
        require(not problems,
                f"a cell with no storey below it may sit on its own slab "
                f"({[str(p) for p in problems]})")

        docs = copy.deepcopy(base)
        # Put a basement under it. `L1` is the only other level in the fixture, so borrow it: the
        # rule reads levels by `ffl` order, and a level below L0 is a level below L0.
        docs["levels"]["levels"].append(
            {"id": "B1", "name": "Basement", "ffl": -2.30, "ceiling": 0.25,
             "structureDepth": 0.35})
        docs["cells"]["cells"].append(
            {"id": "B1_UNDER", "level": "B1", "kind": "room",
             "boxes": [{"x": [2.0, 2.5], "z": [6.0, 8.0]}]})
        row(docs, "cells", "L0_CLOSET")["yOverride"] = [0.15, 3.30]
        dug = workspace / "dug"
        write_fixture(dug, docs)
        _, problems = validate(dug, wanted=[2])
        require(len(problems) == 1 and "L0_CLOSET" in problems[0].message,
                f"and the same cell is refused once a basement is put under it "
                f"({[str(p) for p in problems]})")

        # A single-storey wing with nothing above it may have a roof of its own. The fixture's
        # closet is on L0 and no L1 cell overlaps it, so it can reach past L1's slab underside;
        # the hall, which L1_HALL sits directly on top of, cannot. Rule 2's bound is about what is
        # actually above a cell, not about which level it is on.
        docs = copy.deepcopy(base)
        row(docs, "cells", "L0_CLOSET")["yOverride"] = [0.60, 4.30]
        wing = workspace / "wing"
        write_fixture(wing, docs)
        _, problems = validate(wing, wanted=[2])
        require(not problems,
                f"a cell with no storey above it may have its own roof height "
                f"({[str(p) for p in problems]})")

        docs = copy.deepcopy(base)
        row(docs, "cells", "L1_HALL")["boxes"] = [{"x": [2.0, 2.5], "z": [6.0, 8.0]}]
        row(docs, "cells", "L0_CLOSET")["yOverride"] = [0.60, 4.30]
        roofed = workspace / "roofed"
        write_fixture(roofed, docs)
        _, problems = validate(roofed, wanted=[2])
        require(len(problems) == 1 and "L0_CLOSET" in problems[0].message,
                f"and the same cell is refused once a room is put over it "
                f"({[str(p) for p in problems]})")

        # §70.5's clear-height range is a range for a FLAT ceiling. The attic has none -- §13.6's
        # room runs 2.4 m at the knee wall to 5.0 m at the ridge -- so a cell on a rafter-bounded
        # level is a bounding volume rather than a height, and checking it would report every
        # attic room in every house ever built.
        docs = copy.deepcopy(base)
        docs["levels"]["levels"].append(
            {"id": "L3", "name": "Attic", "ffl": 9.30, "ceiling": None, "structureDepth": 0.30})
        docs["cells"]["cells"].append(
            {"id": "L3_ROOM", "level": "L3", "kind": "room",
             "boxes": [{"x": [-2.0, 2.0], "z": [4.0, 10.0]}],
             "yOverride": [9.30, 12.60]})
        attic = workspace / "attic"
        write_fixture(attic, docs)
        _, problems = validate(attic, wanted=[10])
        require(not problems,
                f"a 3.30 m room under a collar tie is not a §70.5 violation, because §70.5's "
                f"range is about a flat ceiling ({[str(p) for p in problems]})")

        docs["levels"]["levels"][-1]["ceiling"] = 12.60
        flat = workspace / "flat"
        write_fixture(flat, docs)
        _, problems = validate(flat, wanted=[10])
        require(len(problems) == 1 and "L3_ROOM" in problems[0].message,
                f"and the same 3.30 m room IS a violation once its level declares a ceiling plane "
                f"({[str(p) for p in problems]})")

        # Nesting: a container sub-cell is inside its parent and says so, and saying so is not a
        # way of switching rule 3 off.
        def with_fridge(parent="L0_KITCHEN", boxes=None, override=None):
            docs = copy.deepcopy(base)
            docs["cells"]["cells"].append(
                {"id": "CELL_FRIDGE_INTERIOR", "level": "L0", "kind": "closet",
                 "boxes": boxes or [{"x": [0.0, 1.0], "z": [5.0, 6.0]}],
                 "yOverride": override or [0.70, 2.45], "parent": parent})
            return docs

        # The kitchen stand-in is the hall: the fridge sits inside it.
        nested = workspace / "nested"
        write_fixture(nested, with_fridge(parent="L0_HALL"))
        _, problems = validate(nested, wanted=[3])
        require(not problems,
                f"a sub-cell inside the cell it declares as its parent is not an overlap "
                f"({[str(p) for p in problems]})")

        undeclared = workspace / "undeclared"
        docs = with_fridge(parent=None)
        docs["cells"]["cells"][-1].pop("parent")
        write_fixture(undeclared, docs)
        _, problems = validate(undeclared, wanted=[3])
        require(len(problems) == 1 and "overlaps" in problems[0].message,
                f"...and the SAME cell without the declaration is an overlap, so nesting is "
                f"authored rather than inferred ({[str(p) for p in problems]})")

        poking = workspace / "poking"
        write_fixture(poking, with_fridge(parent="L0_HALL",
                                          boxes=[{"x": [0.0, 9.0], "z": [5.0, 6.0]}]))
        _, problems = validate(poking, wanted=[3])
        require(any("not inside it" in p.message for p in problems),
                f"a sub-cell that pokes out of its parent is refused "
                f"({[str(p) for p in problems]})")

        tall = workspace / "tall"
        write_fixture(tall, with_fridge(parent="L0_HALL", override=[0.70, 9.00]))
        _, problems = validate(tall, wanted=[3])
        require(any("vertical extent" in p.message for p in problems),
                f"and so is one that reaches out of it in Y ({[str(p) for p in problems]})")

        deep = workspace / "deep"
        docs = with_fridge(parent="L0_HALL")
        docs["cells"]["cells"].append(
            {"id": "CELL_FRIDGE_SHELF", "level": "L0", "kind": "closet",
             "boxes": [{"x": [0.1, 0.9], "z": [5.1, 5.9]}], "yOverride": [0.80, 1.00],
             "parent": "CELL_FRIDGE_INTERIOR"})
        write_fixture(deep, docs)
        _, problems = validate(deep, wanted=[3])
        require(any("itself nested" in p.message for p in problems),
                f"a container inside a container is a depth the visibility solver does not walk "
                f"({[str(p) for p in problems]})")

        # 13. The tolerances and the guards, each pinned by a case that turns on it alone. Every
        #     one of these was added because a mutation of the rule it belongs to survived the
        #     claims above: a rule that fires on a 24 m² overlap says nothing about 1 cm².
        docs = copy.deepcopy(base)
        docs["cells"]["cells"].append(
            {"id": "L0_VOID", "level": "L0", "kind": "void",
             "boxes": [{"x": [-2.0, 2.0], "z": [4.0, 10.0]}], "yOverride": [3.30, 3.64]})
        stacked = workspace / "stacked"
        write_fixture(stacked, docs)
        _, problems = validate(stacked, wanted=[3])
        require(not problems,
                f"a void directly over the hall, in the slab it does not reach into, is NOT an "
                f"overlap -- that is what yOverride is for ({[str(p) for p in problems]})")

        for area_cm2, wanted_report in ((2.0, True), (0.5, False)):
            docs = copy.deepcopy(base)
            width = area_cm2 * 1e-4 / 0.10          # a 10 cm strip of the required area
            docs["cells"]["cells"].append(
                {"id": "L0_SLIVER", "level": "L0", "kind": "void",
                 "boxes": [{"x": [2.0 - width, 2.0], "z": [4.0, 4.10]}]})
            sliver = workspace / "sliver"
            write_fixture(sliver, docs)
            _, problems = validate(sliver, wanted=[3])
            verdict = "reported" if wanted_report else "inside rule 3's 1 cm² tolerance"
            require(bool(problems) == wanted_report,
                    f"an overlap of {area_cm2} cm² is {verdict} "
                    f"({[str(p) for p in problems]})")

        docs = copy.deepcopy(base)
        row(docs, "cells", "L0_WC1")["boxes"] = [{"x": [2.5, 4.0], "z": [4.0, 6.0]}]
        one_side = workspace / "one-side"
        write_fixture(one_side, docs)
        _, problems = validate(one_side, wanted=[4])
        require(len(problems) == 1 and "cellB" in problems[0].message,
                f"a portal still in cellA's wall but no longer in cellB's is caught -- both "
                f"sides are checked, not the first ({[str(p) for p in problems]})")

        docs = copy.deepcopy(base)
        docs["cells"]["cells"] = [c for c in docs["cells"]["cells"] if c["id"] != "L0_FOYER"]
        docs["portals"]["portals"] = [p_ for p_ in docs["portals"]["portals"]
                                      if "FOYER" not in p_["id"]]
        rootless = workspace / "rootless"
        write_fixture(rootless, docs)
        _, problems = validate(rootless, wanted=[5])
        require(len(problems) == 1 and ROOT_CELL in problems[0].message,
                f"a portal graph with no {ROOT_CELL} is a house with no front door, and rule 5 "
                f"says so rather than walking from whichever cell happens to be first "
                f"({[str(p) for p in problems]})")

        # Rule 5 needs a graph to walk, and a layout under construction has not authored one yet:
        # `HOUSE-00367`..`HOUSE-00372` write the cells a level at a time and the portals come
        # after them. Until `layout.portals.json` exists the data has made no connectivity claim.
        partial = workspace / "partial"
        cells_only = {"cells": copy.deepcopy(base["cells"]), "levels": copy.deepcopy(base["levels"])}
        cells_only["cells"]["cells"] = [c for c in cells_only["cells"]["cells"]
                                        if c["id"] != "L0_FOYER"]
        write_fixture(partial, cells_only)
        _, problems = validate(partial, wanted=[5])
        require(not problems,
                f"a layout with cells and no portals file has claimed no connectivity, so rule 5 "
                f"stands down rather than failing every commit of a house being authored "
                f"({[str(p) for p in problems]})")

        # The file's PRESENCE, not its contents. An empty portals file is a claim, and a wrong
        # one: it says nothing in this house connects to anything.
        cells_only["portals"] = {"schema": "cna-house/portals/1", "portals": []}
        cells_only["cells"]["cells"] = copy.deepcopy(base["cells"]["cells"])
        empty = workspace / "empty-portals"
        write_fixture(empty, cells_only)
        _, problems = validate(empty, wanted=[5])
        require(len(problems) >= 7,
                f"an EMPTY portals file is a claim that nothing connects, and rule 5 says so "
                f"({len(problems)})")

        docs = copy.deepcopy(base)
        cells = docs["cells"]["cells"]
        cells.append(cells.pop(next(i for i, c in enumerate(cells) if c["id"] == "L0_FOYER")))
        docs["portals"]["portals"] = [p_ for p_ in docs["portals"]["portals"]
                                      if "FOYER" not in p_["id"]]
        cut_off = workspace / "cut-off"
        write_fixture(cut_off, docs)
        _, problems = validate(cut_off, wanted=[5])
        # Eight, not seven: the fixture's exterior cell reaches the house through the foyer's
        # front door like everything else, and rule 5 stopped saying "interior" on `HOUSE-00374`.
        require(len(problems) == 8,
                f"when the foyer is walled off, the EIGHT cells cut off from it are the failures "
                f"-- the walk starts at {ROOT_CELL}, not at whichever cell is written first, and "
                f"the two answers differ by seven ({len(problems)})")

        # `HOUSE-00374`: the rule used to say "interior cell", and `EXT_SHED` -- an `exterior`
        # cell that is indoors, roofed and `visibilityHint: opaque` -- had no portal at all. A
        # building you cannot enter is the same defect as a room you cannot enter.
        docs = copy.deepcopy(base)
        docs["cells"]["cells"].append(
            {"id": "EXT_SHED", "level": "L0", "kind": "exterior",
             "boxes": [{"x": [30.0, 33.0], "z": [30.0, 33.0]}],
             "yOverride": [0.0, 2.35]})
        shed = workspace / "shed"
        write_fixture(shed, docs)
        _, problems = validate(shed, wanted=[5])
        require(len(problems) == 1 and "EXT_SHED" in problems[0].message,
                f"an EXTERIOR cell with no portal is unreachable too, and rule 5 no longer stops "
                f"at the word \"interior\" ({[str(x) for x in problems]})")

        # Rule 7 needs both files, and a layout under construction has only one: portals are
        # authored before leaves. It stands down until `layout.openings.json` exists, and bites as
        # hard as before once it does -- the two cases either side of this are that.
        docs = copy.deepcopy(base)
        del docs["openings"]
        leafless = workspace / "leafless"
        write_fixture(leafless, docs)
        _, problems = validate(leafless, wanted=[7])
        require(not problems,
                f"with no openings file the data has claimed nothing about leaves, and rule 7 "
                f"says nothing rather than reporting every door in the house "
                f"({[str(p) for p in problems]})")

        docs = copy.deepcopy(base)
        second = copy.deepcopy(row(docs, "openings", "DOOR_WC1"))
        second["id"] = "DOOR_WC1_AGAIN"
        docs["openings"]["openings"].append(second)
        twinned = workspace / "twinned"
        write_fixture(twinned, docs)
        _, problems = validate(twinned, wanted=[7])
        require(len(problems) == 1 and "already claimed" in problems[0].message,
                f"two leaves in one hole is caught as well as none "
                f"({[str(p) for p in problems]})")

        # 14. Rule 11 is a proof about STANDING, and about the hole rather than the wall.
        require(standable(0.0, 7.0, [(-2.0, 2.0, 4.0, 10.0)]),
                "standable() accepts the middle of a 4 m room")
        require(not standable(2.25, 7.0, [(2.0, 2.5, 6.0, 8.0)]),
                "and refuses a 0.5 m closet: there is floor in it and nobody can stand on it")
        require(not standable(1.9, 7.0, [(-2.0, 2.0, 4.0, 10.0)]),
                "or a spot 0.1 m from a wall, where the capsule would be in the wall")
        ell = [(-2.0, -1.0, 4.0, 10.0), (-2.0, 2.0, 9.0, 10.0)]
        require(segment_inside(-1.5, 5.0, -1.5, 9.5, ell),
                "segment_inside() follows an L-shaped room down its own arm")
        require(not segment_inside(-1.5, 5.0, 1.5, 9.5, ell),
                "and refuses the diagonal that cuts the corner out through the wall")

        # 15. The closet: reached from the corridor, and NOT reached when the doorway is gone.
        #     Without the neighbour search the first fails; without the portal test the second
        #     passes. Both halves are needed and each one proves the other is doing work.
        _, problems = validate(world_dir, wanted=[11])
        require(not problems,
                f"the 0.5 m closet's shelf is reachable -- from the hall, through the doorway "
                f"({[str(p) for p in problems]})")
        docs = copy.deepcopy(base)
        docs["portals"]["portals"] = [row for row in docs["portals"]["portals"]
                                      if row["id"] != "P_HALL__CLOSET"]
        walled = workspace / "broken-closet"
        write_fixture(walled, docs)
        _, problems = validate(walled, wanted=[11])
        require(len(problems) == 1 and "SHELF_CLOSET" in problems[0].message,
                f"and unreachable once the doorway is bricked up, even though the hall floor is "
                f"1.3 m away through the wall ({[str(p) for p in problems]})")

        # 16. And it is JSONC-tolerant, because the authored files are.
        commented = workspace / "commented"
        write_fixture(commented, base)
        cells_path = commented / layout_io.FILES["cells"][0]
        cells_path.write_text("// the cells of a small house\n"
                              + cells_path.read_text(encoding="utf-8"), encoding="utf-8")
        require(validate(commented) == ([], []),
                "a comment at the top of a world file changes nothing")
    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        print(f"\nvalidate_world: {len(failures)} claim(s) FAILED")
        return 1
    print("validate_world: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", nargs="?", type=Path,
                        default=REPO / "assets-src" / "world")
    parser.add_argument("--rules", help="comma-separated rule numbers to run (default: all)")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    wanted = None
    if args.rules:
        wanted = [int(part) for part in args.rules.split(",") if part.strip()]
        unknown = [n for n in wanted if n not in RULES]
        if unknown:
            print(f"validate_world: no such rule(s): {unknown}", file=sys.stderr)
            return 2
    if not args.directory.is_dir():
        print(f"validate_world: {args.directory}: no such directory", file=sys.stderr)
        return 2
    return report(args.directory, wanted)


if __name__ == "__main__":
    sys.exit(main())
