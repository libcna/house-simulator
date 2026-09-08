#!/usr/bin/env python3
"""house_shell_gen.py -- the architectural shell, generated from the layout.

`HOUSE-00451`, the head of phase 6. `cna-house.md` §18.3: *"reads `layout.cells.json` and generates
the architectural shell -- floors, ceilings, walls, openings, stairs, roof -- as a Blender scene,
deterministically"*, and §17.2 makes it the single biggest generator in the project, ~180 000
triangles.

    tools/blender/house_shell_gen.py                 # every cell -> build/shell/<CELL>.glb
    tools/blender/house_shell_gen.py --cells L0_HALL,L0_KITCHEN
    tools/blender/house_shell_gen.py --selftest

## What this pass emits, and what replaces it

One `.glb` per cell, holding:

* the **floor and ceiling** of every footprint box, inset to the inner faces of the walls that
  bound them (`HOUSE-00452`);
* the box's four **sides**, still at the cell's centre-line boundary. That is the massing
  `HOUSE-00451` started with, and it is what `HOUSE-00453` replaces with deduplicated partitions
  and `HOUSE-00454` with real exterior walls, after which `HOUSE-00455` cuts the openings. Each of
  those *supersedes* faces rather than adding beside them; when the last lands, no massing face
  is left.

## The inset, which is why the data stores centre lines

§13.1: *"the data file stores the centre-line box; the geometry builder applies the insets"*. A
cell's box runs to the middle of the wall it shares with the next room, because that is the one
description both rooms agree on. The floor you can stand on stops at the wall's inner face, half a
wall thickness short -- and which half depends on which wall, so the inset is computed **per side**
from `layout.levels.json`'s own `construction` block rather than from §13.1's transcribed 0.075 and
0.15:

| The space across that side | Wall | Inset |
|---|---|---|
| another interior cell | `wallPartition` 0.15 | 0.075 |
| an exterior cell, or nothing at all | `wallExterior` 0.30 | 0.15 |
| the garage, from either side | `wallGarage` 0.25 | 0.125 |

`wallPlumbing` (0.20) is **not** applied: §12.5 gives each stack a chase as a volume in a room and
never says which of that room's four walls is the thick one, so a geometry builder that guessed
would be inventing the house. It is a wall two centimetres thicker than a partition; when the
chases carry a face, this table gains a row.

Everything else here is the pipeline those tasks need and will not have to reinvent: reading the
layout, driving Blender headlessly, the axis convention, the naming, the per-cell export, and a
determinism claim.

## The axis convention, which is the one thing here that is easy to get silently wrong

The world is **Y-up, −Z north** (§9.1) and glTF is Y-up too, so the exported file must carry world
coordinates unchanged. Blender is **Z-up**, and `export_yup=True` -- the exporter's default and the
setting every other tool in this directory uses -- writes `gltf(x, y, z) = blender(X, Z, −Y)`.

So a world point becomes a Blender point as `(x, −z, y)`, and comes back out of the exporter as
itself. That is `to_blender` below, and the selftest asserts it the only way that means anything:
it exports a cell whose world box is known, reads the `POSITION` accessor's `min`/`max` out of the
`.glb` with the project's own glTF reader, and compares them with the layout. A sign convention
that is merely reasoned about is a sign convention that is wrong half the time.

## Determinism

§18.4: every generated file records its inputs, and the same input produces the same output. Cells
are visited in sorted id order, boxes in their authored order, and each box's eight vertices in a
fixed order, so two runs over an unchanged layout produce byte-identical files -- claimed, not
assumed. `build/shell/` is generated and gitignored; nothing here is committed.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import os
import sys

try:
    import bpy  # type: ignore

    INSIDE_BLENDER = True
except ImportError:
    INSIDE_BLENDER = False

if not INSIDE_BLENDER:
    from pathlib import Path

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import blender_env  # noqa: E402

    sys.exit(blender_env.relaunch(Path(__file__).resolve(), sys.argv[1:], tool="house_shell_gen"))

import argparse  # noqa: E402
import hashlib  # noqa: E402
from pathlib import Path  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "world"))
sys.path.insert(0, str(REPO / "tools" / "assets"))
import gltf_validate  # noqa: E402
import layout_io  # noqa: E402

SOURCE = REPO / "assets-src" / "world"
#: Generated, and `content/` is gitignored entirely (§18.4). `build/` is one of the six directory
#: names `AGENTS.md` allows; the shell lives in a subdirectory of it and is never committed.
OUTPUT = REPO / "build" / "shell"

#: The eight corners of a box, in a fixed order, and the six quads over them wound outward.
#: Written out rather than generated so that the winding is reviewable: a face wound the wrong way
#: is invisible from the side you are standing on, which is the side you notice it from.
CORNERS = ((0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1),
           (0, 1, 0), (1, 1, 0), (1, 1, 1), (0, 1, 1))
#: Which way each side's INNER face looks, in world axes: into the room.
#:
#: The faces this generator emits are the inner faces of the walls, and they are seen from inside
#: the room, like the floor from above and the ceiling from below. `HOUSE-00451`'s massing was a
#: solid block seen from outside and its normals pointed the other way; the outer skin of the house
#: is `HOUSE-00454`'s, and it faces out.
INWARD = {"-X": (1.0, 0.0, 0.0), "+X": (-1.0, 0.0, 0.0),
          "-Z": (0.0, 0.0, 1.0), "+Z": (0.0, 0.0, -1.0)}


def to_blender(x: float, y: float, z: float) -> tuple[float, float, float]:
    """A world point (Y-up, −Z north) as a Blender point (Z-up), so that the export is identity.

    `export_yup=True` writes `gltf(x, y, z) = blender(X, Z, −Y)`; inverting that gives this.
    """
    return (x, -z, y)


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)


def extent_of(cell: dict, level: dict) -> tuple[tuple[float, float] | None, str]:
    """`(extent, "")`, or `(None, reason)` when the layout does not give the cell a ceiling.

    A level with a `null` ceiling is bounded by rafters (§13.6's attic), and a cell there has a
    height only if it declares a `yOverride`. Every attic cell in this house does; the branch is
    here because the format allows one not to, and inventing a flat ceiling for it would put a
    slab through the roof. Returned rather than raised so one such cell does not end the run.
    """
    try:
        return layout_io.cell_extent(cell, level), ""
    except layout_io.LayoutError as error:
        return None, str(error)


def cell_boxes(cell: dict, extent: tuple[float, float]):
    """`(x0, x1, y0, y1, z0, z1)` per footprint box, in the cell's authored order."""
    for box in cell.get("boxes") or []:
        yield (float(box["x"][0]), float(box["x"][1]),
               extent[0], extent[1],
               float(box["z"][0]), float(box["z"][1]))


def neighbour_boxes(layout: dict, levels: dict) -> list:
    """Every INTERIOR cell's boxes, once, so a side can ask what is across it.

    Exterior cells are left out on purpose: the back wall of the family room faces `EXT_BACKYARD`,
    which is a cell, and it is still an exterior wall. One function rather than two loops, because
    the selftest has to ask the same question the generator asks -- the first version had a copy
    in each and an injected bug that deleted the exterior rule went unnoticed.
    """
    out = []
    for cell in layout_io.rows(layout, "cells"):
        level = levels.get(cell.get("level"))
        if level is None or cell.get("kind") == "exterior":
            continue
        extent, _ = extent_of(cell, level)
        if extent is not None:
            out.extend((cell, box) for box in cell_boxes(cell, extent))
    return out


def side_span(side: str, box: tuple) -> tuple[float, float, float]:
    """`(plane, lo, hi)` for one side: where it is, and the range it runs over in plan."""
    x0, x1, _y0, _y1, z0, z1 = box
    if side in ("-X", "+X"):
        return (x0 if side == "-X" else x1, z0, z1)
    return (z0 if side == "-Z" else z1, x0, x1)


def side_intervals(side: str, box: tuple, cell: dict, neighbours: list,
                   ) -> list[tuple[float, float, str]]:
    """@p side split into `(lo, hi, wall)` runs: which wall bounds each stretch of it.

    A side is rarely all one thing. `L0_KITCHEN`'s north side is 8.9 m of partition against the
    sunroom and 1.5 m of exterior wall beside it; seven of the house's 324 sides are mixed like
    that. A generator that took the first neighbour it found and applied that wall to the whole
    side would put 1.5 m of the kitchen floor inside the outside wall.

    An **interior** cell across the side makes it a partition; an exterior cell counts as nothing,
    because the back wall of the family room faces `EXT_BACKYARD`, which is a cell, and is still
    an exterior wall.
    """
    plane, lo, hi = side_span(side, box)
    _x0, _x1, y0, y1, _z0, _z1 = box
    covered: list[tuple[float, float, str]] = []
    for other, obox in neighbours:
        if other["id"] == cell["id"]:
            continue
        ox0, ox1, oy0, oy1, oz0, oz1 = obox
        if oy1 <= y0 + 1e-6 or oy0 >= y1 - 1e-6:
            continue                      # a different storey; not across this wall
        oplane, olo, ohi = side_span({"-X": "+X", "+X": "-X",
                                      "-Z": "+Z", "+Z": "-Z"}[side], obox)
        if abs(oplane - plane) > 1e-6:
            continue
        start, end = max(lo, olo), min(hi, ohi)
        if end - start <= 1e-6:
            continue
        wall = "wallGarage" if "garage" in (cell.get("kind"), other.get("kind")) \
            else "wallPartition"
        covered.append((start, end, wall))

    outside = "wallGarage" if cell.get("kind") == "garage" else "wallExterior"
    out: list[tuple[float, float, str]] = []
    cursor = lo
    for start, end, wall in sorted(covered):
        if start > cursor + 1e-6:
            out.append((cursor, start, outside))
        if end > cursor:
            out.append((max(cursor, start), end, wall))
            cursor = end
    if hi > cursor + 1e-6:
        out.append((cursor, hi, outside))
    return out


def wall_across(side: str, box: tuple, cell: dict, neighbours: list, construction=None) -> str:
    """The **thinnest** wall on @p side.

    The floor is a rectangle, so its edge cannot follow a side that is partition for part of its
    length and exterior for the rest -- seven of the house's 324 sides are. It runs to the
    thinnest, which puts a few centimetres of floor **under** the thicker wall over that stretch.

    That is the right way round to be wrong. Floor inside a wall is never seen; floor that stops
    short of one leaves a 75 mm slot between the floor and the skirting, running the length of the
    room, that you can see the void through. The first version took the thickest and left exactly
    that gap along 8.9 m of the kitchen.
    """
    construction = construction or {}
    walls = {wall for _lo, _hi, wall in side_intervals(side, box, cell, neighbours)}
    return min(walls, key=lambda name: float(construction.get(name, 0.0)), default="wallExterior")


def inset_box(box: tuple, cell: dict, neighbours: list, construction: dict) -> tuple:
    """@p box's floor rectangle: each side moved in by half the thinnest wall that bounds it."""
    x0, x1, y0, y1, z0, z1 = box
    half = {side: float(construction.get(
        wall_across(side, box, cell, neighbours, construction), 0.0)) / 2.0
        for side in ("-X", "+X", "-Z", "+Z")}
    return (x0 + half["-X"], x1 - half["+X"], y0, y1, z0 + half["-Z"], z1 - half["+Z"])


def facing(points: list[tuple[float, float, float]], wanted: tuple[float, float, float]):
    """@p points, reversed if their winding does not face @p wanted. Both in WORLD axes.

    Wound rather than remembered. `to_blender` negates z, so a plan winding reverses on the way
    into Blender, and every face in the first version of this file pointed into the room it bounds
    -- invisible from the only side anyone looks at it from, and not caught by any claim about
    coordinates, because a mesh turned inside out has exactly the right bounds.
    """
    (ax, ay, az), (bx, by, bz), (cx, cy, cz) = points[0], points[1], points[2]
    ux, uy, uz = bx - ax, by - ay, bz - az
    vx, vy, vz = cx - bx, cy - by, cz - bz
    normal = (uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx)
    return points if sum(n * o for n, o in zip(normal, wanted)) > 0 else list(reversed(points))


def outer_wall_name(level: dict, wall: str) -> str:
    """`foundationWall` for an exterior run below grade, otherwise @p wall.

    §10.2 puts grade at about 0, and only `B1` sits under it. Read from the level's `ffl` rather
    than from its id, so a house with a second basement gets the same answer.
    """
    if wall == "wallExterior" and float(level.get("ffl", 0.0)) < 0.0:
        return "foundationWall"
    return wall


def outer_span(cell: dict, extent: tuple[float, float], level: dict, levels: dict):
    """The Y range an exterior run's OUTER face covers: this cell's floor to the next level's.

    A cell's own extent stops at its ceiling -- 3.30 on `L0` -- and the next storey's floor is at
    3.65. Between them is 0.35 m of floor structure, and an outer skin built from cell extents
    alone leaves a slot round the whole house at every storey, level with the joists. The lower
    cell carries that band, so every band is carried once.
    """
    above = [float(row["ffl"]) for row in levels.values()
             if isinstance(row.get("ffl"), (int, float)) and float(row["ffl"]) > extent[1] - 1e-6]
    return (extent[0], min(above) if above else extent[1])


def holes_in(side: str, box: tuple, cell: dict, portals: list) -> list:
    """The portal rectangles that pierce @p side, as `(u0, u1, v0, v1, portal_id)`.

    A portal **is** the hole. Every one of them, not only the ones with a leaf: a cased opening has
    no door and is still a doorway you walk through, and §13.1's "cased openings are not doors" is
    about counting doors, not about whether there is a hole. Horizontal portals -- the stairwells --
    pierce floors rather than walls and are `HOUSE-00460`'s.
    """
    plane, lo, hi = side_span(side, box)
    axis = "x" if side in ("-X", "+X") else "z"
    out = []
    for portal in portals:
        if (portal.get("plane") or {}).get("axis") != axis:
            continue
        if abs(float(portal["plane"]["value"]) - plane) > 1e-6:
            continue
        if cell["id"] not in (portal.get("cellA"), portal.get("cellB")):
            continue
        rect = portal.get("rect") or {}
        u0, u1 = (float(value) for value in rect["u"])
        v0, v1 = (float(value) for value in rect["v"])
        if min(u1, hi) - max(u0, lo) <= 1e-6:
            continue
        out.append((max(u0, lo), min(u1, hi), v0, v1, portal["id"]))
    return sorted(out)


def slab_holes(portals: list, cell: dict, at_y: float, box: tuple) -> list:
    """The horizontal portals that pierce a slab of @p cell at @p at_y, as `(x0, x1, z0, z1, id)`.

    A stairwell is a portal with a `y` plane: the hole in the floor you walk round. Cutting it is
    `HOUSE-00460`'s, and until it was cut the landing above every flight had a floor across it and
    the stair arrived in a ceiling.
    """
    x0, x1, _y0, _y1, z0, z1 = box
    out = []
    for portal in portals:
        plane = portal.get("plane") or {}
        if plane.get("axis") != "y" or abs(float(plane["value"]) - at_y) > 1e-6:
            continue
        if cell["id"] not in (portal.get("cellA"), portal.get("cellB")):
            continue
        rect = portal.get("rect") or {}
        hx0, hx1 = (float(value) for value in rect["u"])
        hz0, hz1 = (float(value) for value in rect["v"])
        if min(hx1, x1) - max(hx0, x0) <= 1e-6 or min(hz1, z1) - max(hz0, z0) <= 1e-6:
            continue
        out.append((max(hx0, x0), min(hx1, x1), max(hz0, z0), min(hz1, z1), portal["id"]))
    return sorted(out)


def panel(lo: float, hi: float, v0: float, v1: float,
          holes: list[tuple[float, float, float, float]]):
    """`(lo, hi, v0, v1)` rectangles covering the panel except where a hole is.

    A grid rather than a boolean: the cuts are the holes' own edges, and a cell of the grid is
    emitted unless its centre is inside a hole. With at most a handful of openings in a wall this
    is exact, deterministic and needs no library -- and a rectangle it emits is always a rectangle,
    which the lightmap unwrap downstream would rather have than a triangulated polygon with a slot
    in it.
    """
    if not holes:
        return [(lo, hi, v0, v1)]
    us = sorted({lo, hi} | {value for hole in holes for value in hole[:2]
                            if lo - 1e-9 < value < hi + 1e-9})
    vs = sorted({v0, v1} | {value for hole in holes for value in hole[2:4]
                            if v0 - 1e-9 < value < v1 + 1e-9})
    out = []
    for ulo, uhi in zip(us, us[1:]):
        for vlo, vhi in zip(vs, vs[1:]):
            if uhi - ulo <= 1e-9 or vhi - vlo <= 1e-9:
                continue
            umid, vmid = (ulo + uhi) / 2.0, (vlo + vhi) / 2.0
            if any(hole[0] - 1e-9 <= umid <= hole[1] + 1e-9
                   and hole[2] - 1e-9 <= vmid <= hole[3] + 1e-9 for hole in holes):
                continue
            out.append((ulo, uhi, vlo, vhi))
    return out


def architrave_boards(hu0: float, hu1: float, hv0: float, hv1: float, casing: float):
    """The two jambs and the head of an architrave round an opening, in the wall's own axes.

    A function rather than three lines inside the builder, so the width can be claimed: every door
    in this house declares the same 0.06 m casing, and a builder that hard-coded 0.06 would be
    indistinguishable from one that read the opening -- until the day a door had a wider casing.
    """
    if casing <= 0.0:
        return []
    return [(hu0 - casing, hu0, hv0, hv1 + casing),
            (hu1, hu1 + casing, hv0, hv1 + casing),
            (hu0 - casing, hu1 + casing, hv1, hv1 + casing)]


#: A window's section, in metres. §12.6 gives every type a leaf size and a sill height and no
#: section at all, so these four are this generator's and are named here rather than buried:
#: a frame 55 mm wide, a sash 42 mm inside it, 6 mm of glass, and a sill board projecting 30 mm
#: into the room. They are the sizes a joiner would use; nothing in the layout contradicts them,
#: and when §12 gains a section this is the one place that changes.
FRAME_SECTION = 0.055
SASH_SECTION = 0.042
GLASS_THICK = 0.006
SILL_PROJECT = 0.030

#: §12.6's type table names the double-hung windows `W_DH_*`, and a double-hung window has a
#: meeting rail across the middle where the two sashes pass. Every other type in that table --
#: picture, panel, slider, hopper, louvre, bay, dormer -- is a single light and has no bar. Read
#: from the type prefix, which `docs/conventions.md` makes an id and §12.6 makes a schedule entry.
MEETING_RAIL = 0.050


def has_meeting_rail(opening_type: str) -> bool:
    return str(opening_type or "").startswith("W_DH_")


def window_owner(sides, cells_by_id: dict, fallback: str) -> str:
    """Which cell builds a window: the first of its INTERIOR cells in id order.

    A window is one object. A borrowed-light window between the kitchen and the sunroom must not
    be built by both of them, and a window onto the back lawn must not be built by the lawn, which
    has no walls. Falls back to @p fallback when the cell table is not to hand, so a caller that
    forgets it gets a window built once by the cell asking rather than a window built by whichever
    id happens to sort first -- which is how `EXT_BACKYARD` came to own the kitchen's window and
    the kitchen came to have a hole with a sill and no glass in it.
    """
    interior = sorted(identifier for identifier in sides or ()
                      if identifier and (cells_by_id.get(identifier) or {}).get("kind")
                      not in (None, "exterior"))
    return interior[0] if interior else fallback


def minus(lo: float, hi: float, cuts) -> list[tuple[float, float]]:
    """`[lo, hi]` with @p cuts taken out of it, as the runs that are left."""
    out = []
    cursor = lo
    for cut_lo, cut_hi in sorted(cuts):
        if cut_hi <= cursor + 1e-9 or cut_lo >= hi - 1e-9:
            continue
        if cut_lo > cursor + 1e-9:
            out.append((cursor, min(cut_lo, hi)))
        cursor = max(cursor, cut_hi)
    if hi > cursor + 1e-9:
        out.append((cursor, hi))
    return out


def mitred(lo: float, hi: float, corner_lo: float, corner_hi: float, proud: float):
    """@p lo…@p hi shortened by @p proud at whichever end is a corner of the room.

    Two boards that both ran to the corner would overlap there in a square of their own projection,
    which is what a mitre is for. An end that is not a corner -- where one run of wall meets the
    next along the same side -- is left alone, or the boards would part company in the middle of
    a wall.
    """
    return (lo + (proud if abs(lo - corner_lo) < 1e-9 else 0.0),
            hi - (proud if abs(hi - corner_hi) < 1e-9 else 0.0))


#: A tread's nosing: how far it overhangs the riser below it, and how thick the board is. §12.4
#: gives every flight a rise, a going, a width and a footprint and says nothing about the section,
#: so these two are this generator's, like the window's.
NOSING_PROJECT = 0.025
NOSING_THICK = 0.045

#: A handrail's section and a newel's, in metres. §12's `balustrade` and `railing` give the two
#: HEIGHTS -- 0.95 up a flight, 1.10 at a drop -- and no section, so these are this generator's.
RAIL_SECTION = 0.055
NEWEL_SECTION = 0.090


def flight_steps(flight: dict, bottom: float):
    """Every step of a flight as `(along_lo, along_hi, top_y, across_index)`, in run order.

    `along` runs in the direction of travel from 0, so the caller maps it onto the footprint and
    the sign of `run`; `across_index` is 0 for the first run of a `u` and 1 for the one that
    doubles back. The landing is not a step and is placed by the caller: it is where a riser ends
    (§12.4), which is why `HOUSE-00379` had to move both of this house's landings onto a riser
    boundary.
    """
    risers = int(flight["risers"])
    rise = float(flight["rise"])
    going = float(flight["going"])
    turn = int((flight.get("landings") or [{}])[0].get("at") or 0) if flight.get("shape") == "u" \
        else 0
    depth = float((flight.get("landings") or [{}])[0].get("depth") or 0.0) if turn else 0.0
    steps = []
    for index in range(1, risers + 1):
        if turn and index <= turn:
            along = (index - 1) * going
            steps.append((along, along + going, bottom + index * rise, 0))
        elif turn:
            # Back down the other side, measured from the landing's far edge.
            back = (risers - index) * going
            steps.append((back, back + going, bottom + index * rise, 1))
        else:
            along = (index - 1) * going
            steps.append((along, along + going, bottom + index * rise, 0))
    return steps, turn, depth


#: §12's trim, in metres. `casing` and `skirting` are `layout.levels.json`'s; the two below are
#: not in the data because §12 never gives them a number, and a board that stands proud of the
#: wall by nothing z-fights with it.
ARCHITRAVE_PROUD = 0.018
#: A skirting and a cornice stand as proud of the wall as an architrave does, and mitre against
#: each other at a corner by exactly that much.
TRIM_PROUD = 0.018
THRESHOLD_THICK = 0.015


def build_flight(flight: dict, solid, bottom: float, *, add=None, construction=None,
                 inner=None) -> None:
    """A flight's steps, nosings and landing, as boxes, through @p solid.

    Solid steps rather than treads on a carriage: a blockout wants the shape you walk on and the
    volume you cannot walk through, and a closed string is both. The nosing is a separate board
    because it overhangs, which is the one part of a step's profile you see from below.
    """
    footprint = flight.get("footprint") or {}
    if not footprint or flight.get("run") not in ("-X", "+X", "-Z", "+Z"):
        return
    x0, x1 = (float(value) for value in footprint["x"])
    z0, z1 = (float(value) for value in footprint["z"])
    run = flight["run"]
    width = float(flight["width"])

    along_axis_x = run in ("-X", "+X")
    start = (x1 if run == "-X" else x0) if along_axis_x else (z1 if run == "-Z" else z0)
    sign = -1.0 if run in ("-X", "-Z") else 1.0
    across_lo, across_hi = (z0, z1) if along_axis_x else (x0, x1)

    steps, turn, landing_depth = flight_steps(flight, bottom)

    def place(a0, a1, low, high, lane):
        """One box, from run coordinates to world ones."""
        if lane == 0:
            c0, c1 = across_lo, across_lo + width
        else:
            c0, c1 = across_hi - width, across_hi
        p0, p1 = sorted((start + sign * a0, start + sign * a1))
        if along_axis_x:
            solid(p0, p1, low, high, c0, c1)
        else:
            solid(c0, c1, low, high, p0, p1)

    for a0, a1, top, lane in steps:
        offset = (turn * float(flight["going"]) + landing_depth) if lane else 0.0
        place(a0 + offset, a1 + offset, bottom, top, lane)
        # The nosing overhangs the riser below it by its own projection.
        place(a0 + offset - NOSING_PROJECT, a0 + offset, top - NOSING_THICK, top, lane)

    # `HOUSE-00460`: a handrail up every side of a run that is not against a wall, and a newel at
    # each end of it. "Against a wall" is the run's across edge lying on the CELL's own boundary --
    # both are centre-line numbers, which is why they can be compared at all. A `u`'s two inner
    # edges face the well and never do, so a U-stair gets one rail up each run and the outer sides
    # get none.
    if add is not None and construction:
        height = float(construction.get("balustrade", 0.0))
        wall_edges = set()
        if inner is not None:
            for value in ((inner[4], inner[5]) if along_axis_x else (inner[0], inner[1])):
                wall_edges.add(round(value, 4))
        lanes = sorted({lane for _a0, _a1, _top, lane in steps})
        for lane in lanes:
            run_steps = [step for step in steps if step[3] == lane]
            offset = (turn * float(flight["going"]) + landing_depth) if lane else 0.0
            lane_lo = across_lo if lane == 0 else across_hi - width
            lane_hi = lane_lo + width
            first, last = run_steps[0], run_steps[-1]
            a_start = start + sign * (first[0] + offset)
            a_end = start + sign * (last[1] + offset)
            for edge in (lane_lo, lane_hi):
                if round(edge, 4) in wall_edges:
                    continue
                rail_along(add, along_axis_x, a_start, a_end,
                           first[2] + height, last[2] + height, edge, RAIL_SECTION)
                for a_at, y_at in ((a_start, first[2]), (a_end, last[2])):
                    post_lo, post_hi = sorted((a_at, a_at + sign * NEWEL_SECTION))
                    if along_axis_x:
                        solid(post_lo, post_hi, y_at - float(flight["rise"]), y_at + height,
                              edge - NEWEL_SECTION / 2.0, edge + NEWEL_SECTION / 2.0)
                    else:
                        solid(edge - NEWEL_SECTION / 2.0, edge + NEWEL_SECTION / 2.0,
                              y_at - float(flight["rise"]), y_at + height, post_lo, post_hi)

    if turn and landing_depth > 0.0:
        low = bottom + turn * float(flight["rise"])
        a0 = turn * float(flight["going"])
        p0, p1 = sorted((start + sign * a0, start + sign * (a0 + landing_depth)))
        if along_axis_x:
            solid(p0, p1, bottom, low, across_lo, across_hi)
        else:
            solid(across_lo, across_hi, bottom, low, p0, p1)


def rail_along(add, axis_x: bool, a0: float, a1: float, y0: float, y1: float,
               across: float, section: float) -> None:
    """A rail from `(a0, y0)` to `(a1, y1)` along one axis, centred on @p across.

    Six quads rather than an axis-aligned box, because a handrail up a flight is **raked**: its two
    ends are a storey apart in height. A stepped rail -- a level box over each tread -- is the easy
    way to avoid sloping quads and is not a handrail; you can see the difference from the hall.
    """
    half = section / 2.0
    corners = []
    for a, y in ((a0, y0), (a1, y1)):
        for dy in (-half, half):
            for dc in (-half, half):
                corners.append((a, y + dy, across + dc))
    def point(index):
        a, y, c = corners[index]
        return (a, y, c) if axis_x else (c, y, a)
    for face, outward in (((0, 1, 3, 2), (-1.0, 0.0, 0.0)), ((4, 5, 7, 6), (1.0, 0.0, 0.0)),
                          ((0, 2, 6, 4), (0.0, -1.0, 0.0)), ((1, 3, 7, 5), (0.0, 1.0, 0.0)),
                          ((0, 1, 5, 4), (0.0, 0.0, -1.0)), ((2, 3, 7, 6), (0.0, 0.0, 1.0))):
        world_outward = outward if axis_x else (outward[2], outward[1], outward[0])
        add([point(index) for index in face], world_outward)


def flight_going(flights, cell) -> float:
    """The deepest going among the flights arriving in @p cell, as the tolerance for "at the edge".

    A top tread sits one going short of the floor's edge or level with it depending on how the
    flight was authored; anything within a tread's depth of the hole's edge is the way in.
    """
    return max([float(row.get("going") or 0.0) for row in flights or ()
                if row.get("toCell") == cell["id"]] or [0.0])


def top_tread_box(flight: dict, bottom: float):
    """The world box of a flight's top step, or None. Where you step off it onto the floor above."""
    footprint = flight.get("footprint") or {}
    if not footprint or flight.get("run") not in ("-X", "+X", "-Z", "+Z"):
        return None
    x0, x1 = (float(value) for value in footprint["x"])
    z0, z1 = (float(value) for value in footprint["z"])
    run = flight["run"]
    width = float(flight["width"])
    along_axis_x = run in ("-X", "+X")
    start = (x1 if run == "-X" else x0) if along_axis_x else (z1 if run == "-Z" else z0)
    sign = -1.0 if run in ("-X", "-Z") else 1.0
    across_lo, across_hi = (z0, z1) if along_axis_x else (x0, x1)
    steps, turn, landing_depth = flight_steps(flight, bottom)
    a0, a1, top, lane = steps[-1]
    offset = (turn * float(flight["going"]) + landing_depth) if lane else 0.0
    c0, c1 = (across_lo, across_lo + width) if lane == 0 else (across_hi - width, across_hi)
    p0, p1 = sorted((start + sign * (a0 + offset), start + sign * (a1 + offset)))
    return (p0, p1, c0, c1, top) if along_axis_x else (c0, c1, p0, p1, top)


#: How far the roof oversails the outer face of the wall, in metres. §12.1 says the attic is "under
#: a 7:12 roof over a 13.4 m span", and the main block is 12.80 m between wall centre lines, 13.10
#: between their outer faces: the missing 0.30 is 0.15 of overhang on each side. So the number is
#: §12.1's, arrived at by subtraction, and it is the one place it appears.
EAVES_OVERHANG = 0.15
FASCIA_DEPTH = 0.20
FASCIA_THICK = 0.035


def roof_planes(box: tuple, eaves_y: float, pitch: float):
    """A hip roof over a rectangle: `(corners, outward)` per plane, in world coordinates.

    Two trapezoids along the long sides and two triangles at the ends -- or four triangles when the
    rectangle is square, which the garage's 8.4 × 8.4 wing is, and a pyramid is what a hip roof
    over a square is.

    §12.1's roof is "hipped-and-gabled". The hips are here; the gables are the five dormers
    (`HOUSE-00462`) and the projecting garage wing, which is what makes the phrase true without
    this generator having to invent a gablet §12 never describes.
    """
    x0, x1, z0, z1 = box
    dx, dz = x1 - x0, z1 - z0
    half = min(dx, dz) / 2.0
    top = eaves_y + half * pitch

    def plane(points, outward):
        """One face, with a degenerate ridge collapsed: over a square the two trapezoids meet at
        a point, which makes them triangles, which is what a pyramid is."""
        kept = [point for index, point in enumerate(points)
                if index == 0 or max(abs(a - b) for a, b in zip(point, points[index - 1])) > 1e-9]
        return (kept, outward)

    if dx >= dz:
        zm = (z0 + z1) / 2.0
        ridge0, ridge1 = (x0 + half, zm), (x1 - half, zm)
        return [
            plane([(x0, eaves_y, z0), (x1, eaves_y, z0), (ridge1[0], top, zm),
                   (ridge0[0], top, zm)], (0.0, pitch, -1.0)),
            plane([(x1, eaves_y, z1), (x0, eaves_y, z1), (ridge0[0], top, zm),
                   (ridge1[0], top, zm)], (0.0, pitch, 1.0)),
            plane([(x0, eaves_y, z1), (x0, eaves_y, z0), (ridge0[0], top, zm)],
                  (-1.0, pitch, 0.0)),
            plane([(x1, eaves_y, z0), (x1, eaves_y, z1), (ridge1[0], top, zm)],
                  (1.0, pitch, 0.0)),
        ]
    xm = (x0 + x1) / 2.0
    ridge0, ridge1 = (xm, z0 + half), (xm, z1 - half)
    return [
        plane([(x0, eaves_y, z1), (x0, eaves_y, z0), (xm, top, ridge0[1]),
               (xm, top, ridge1[1])], (-1.0, pitch, 0.0)),
        plane([(x1, eaves_y, z0), (x1, eaves_y, z1), (xm, top, ridge1[1]),
               (xm, top, ridge0[1])], (1.0, pitch, 0.0)),
        plane([(x0, eaves_y, z0), (x1, eaves_y, z0), (xm, top, ridge0[1])], (0.0, pitch, -1.0)),
        plane([(x1, eaves_y, z1), (x0, eaves_y, z1), (xm, top, ridge1[1])], (0.0, pitch, 1.0)),
    ]


#: The rainwater goods and the chimney (`HOUSE-00468`). §12 names none of them, so every section
#: here is this generator's; what is NOT invented is where the chimney stands, which is the
#: fireplace's own position, and how high it goes, which is §12's ridge plus the 0.60 m a stack
#: has to clear it by.
GUTTER_SECTION = 0.12
DOWNSPOUT_SECTION = 0.10
RIDGE_VENT_WIDTH = 0.30
RIDGE_VENT_HEIGHT = 0.08
CHIMNEY_ALONG = 1.10
CHIMNEY_ACROSS = 0.60
CHIMNEY_OVER_RIDGE = 0.60

#: A balcony's edge (`HOUSE-00465`): a solid parapet with a railing capping it to §12's `railing`
#: height. §12.2 says the rear-extension roof is "open" and used as the master balcony and gives no
#: section, so the parapet's thickness and height are this generator's.
PARAPET_THICK = 0.20
PARAPET_HEIGHT = 0.55

#: The porch (`HOUSE-00464`). §12.1 says "a full-width front porch on four square columns" and
#: gives no section, so the column is 0.20 square and the beam over it 0.25 deep, and the four are
#: spread evenly along the open edge.
PORCH_COLUMNS = 4
COLUMN_SECTION = 0.20
PORCH_BEAM = 0.25

#: The attic's structure (`HOUSE-00463`). §12 gives the pitch, the ridge and the collar tie and
#: says nothing about members, so the spacing and the sections are this generator's: rafters at
#: 400 mm centres, 50 × 200, a purlin under each slope at mid-span, and a 600 mm walkway board.
RAFTER_SPACING = 0.40
RAFTER_WIDTH = 0.05
RAFTER_DEPTH = 0.20
PURLIN_SECTION = 0.15
WALKWAY_WIDTH = 0.60
WALKWAY_THICK = 0.030

#: A dormer's cheek thickness, and how far its front wall rises above the window head.
DORMER_CHEEK = 0.10
DORMER_HEAD = 0.15


def dormer_shell(rect_u: tuple, rect_v: tuple, plane_z: float, outward: float,
                 outer: tuple, eaves_y: float, pitch: float):
    """One dormer over a window, as `(corners, outward)` faces: front, two cheeks, two roof planes.

    A **wall** dormer: this house's five sit in the front and rear walls (`W_DORMER`'s portals are
    on the wall's own plane) and rise through the roof, rather than standing back on the slope. Its
    own roof is a little gable at the main roof's pitch, and it runs back until its ridge meets the
    main plane -- which is where a dormer roof dies into a roof.
    """
    u0, u1 = rect_u[0] - DORMER_CHEEK, rect_u[1] + DORMER_CHEEK
    head = rect_v[1] + DORMER_HEAD
    ridge_y = head + ((u1 - u0) / 2.0) * pitch
    # The main roof's surface at a given z on this side, and where the dormer's ridge meets it.
    eaves_z = outer[3] if outward > 0 else outer[2]
    def roof_at(z):
        return eaves_y + abs(eaves_z - z) * pitch
    back = eaves_z - outward * ((ridge_y - eaves_y) / pitch)
    mid = (u0 + u1) / 2.0
    faces = [
        # The front gable, from the roof surface at the wall up to the dormer's own ridge.
        ([(u0, roof_at(plane_z), plane_z), (u1, roof_at(plane_z), plane_z),
          (u1, head, plane_z), (mid, ridge_y, plane_z), (u0, head, plane_z)],
         (0.0, 0.0, outward)),
    ]
    for edge, side in ((u0, -1.0), (u1, 1.0)):
        faces.append(([(edge, roof_at(plane_z), plane_z), (edge, head, plane_z),
                       (edge, roof_at(back), back)], (side, 0.0, 0.0)))
    for edge, side in ((u0, -1.0), (u1, 1.0)):
        faces.append(([(edge, head, plane_z), (mid, ridge_y, plane_z),
                       (mid, ridge_y, back), (edge, roof_at(back), back)],
                      (side, pitch, 0.0)))
    return faces


def eaves_height(construction: dict, box: tuple) -> float:
    """Where the roof's eaves EDGE is, derived from the ridge and the pitch (`HOUSE-00461`).

    §12 over-determines the roof: it states a ridge at +14.30, a 7:12 pitch, a 1.20 m knee wall and
    a 13.4 m span, and the four do not quite agree. The ridge and the pitch win -- 14.30 is the
    house's height above grade and 7:12 is what you see -- and the knee wall comes out at 1.179 m
    against §12's 1.20, a 21 mm difference that is §12 rounding rather than a disagreement about
    the house.
    """
    x0, x1, z0, z1 = box
    return float(construction["ridgeY"]) - (min(x1 - x0, z1 - z0) / 2.0) * \
        float(construction["roofPitch"])


def covered_by(cell: dict, extent: tuple[float, float], cells_by_id: dict) -> bool:
    """Is another cell stacked over the whole of @p cell's footprint, just above it?

    That is what makes the porch a porch: `L1_BALCONY_FRONT` has the same box and its floor is
    0.30 m over the porch's head, so the porch has a roof and the rear balcony does not. Reading
    it this way means no cell has to be named here.
    """
    boxes = cell.get("boxes") or []
    if not boxes:
        return False
    for other in cells_by_id.values():
        if other.get("id") == cell.get("id"):
            continue
        above = other.get("yOverride")
        if not above or not (extent[1] - 0.6 <= float(above[0]) <= extent[1] + 0.6):
            continue
        for box in boxes:
            covered = any(float(other_box["x"][0]) <= float(box["x"][0]) + 1e-6
                          and float(other_box["x"][1]) >= float(box["x"][1]) - 1e-6
                          and float(other_box["z"][0]) <= float(box["z"][0]) + 1e-6
                          and float(other_box["z"][1]) >= float(box["z"][1]) - 1e-6
                          for other_box in other.get("boxes") or [])
            if not covered:
                break
        else:
            return True
    return False


#: A deck is an exterior cell raised this far over grade. Below it, an exterior cell IS the ground
#: and its surface is the terrain (`HOUSE-00761`), not a slab.
DECK_OVER_GRADE = 0.30


def slab_here(cell: dict, extent: tuple, is_floor: bool) -> bool:
    """Does @p cell get this slab? (`HOUSE-00466`.)

    **Outside has no ceiling**, and until this was asked every exterior cell had one: a lid over
    the front lawn at +20 m, one over each balcony at +9, and one over `EXT_WORLD` at +60 -- a
    single polygon 160 000 m² across, roofing the world.

    Outside has no floor either, unless it is a **deck**. The yards' ground is the terrain; the
    porch, the terrace and the three balconies are platforms above it, and their floor is a real
    slab. The test is the cell's own floor height, because that is the difference.
    """
    if cell.get("kind") != "exterior":
        return True
    return is_floor and extent[0] > DECK_OVER_GRADE


def open_sides_of(cell: dict, box: tuple, neighbours: list) -> list:
    """The sides of @p box with no interior cell across them -- the sides you can fall off."""
    return [side for side in ("-X", "+X", "-Z", "+Z")
            if not any(wall == "wallPartition"
                       for _lo, _hi, wall in side_intervals(side, box, cell, neighbours))]


def build_balcony_edge(cell: dict, extent: tuple, neighbours: list, construction, solid, add
                       ) -> int:
    """A parapet with a railing on it round a deck you could fall a storey off. Returns the count.

    §70.5's threshold is a drop over a metre, and the same number decides the rail's height, so it
    is asked once. A ground-level deck -- the porch at +0.57, the terrace at +0.45 -- gets nothing
    here; the porch's own balustrade is `HOUSE-00464`'s and is a different thing.
    """
    if cell.get("kind") != "exterior" or not construction or extent[0] <= 1.0:
        return 0
    built = 0
    for box in cell_boxes(cell, extent):
        for side in open_sides_of(cell, box, neighbours):
            plane, lo, hi = side_span(side, box)
            inward = 1.0 if side in ("-X", "-Z") else -1.0
            near, far_edge = plane, plane + PARAPET_THICK * inward
            if side in ("-X", "+X"):
                solid(min(near, far_edge), max(near, far_edge),
                      extent[0], extent[0] + PARAPET_HEIGHT, lo, hi)
            else:
                solid(lo, hi, extent[0], extent[0] + PARAPET_HEIGHT,
                      min(near, far_edge), max(near, far_edge))
            rail_along(add, side in ("-X", "+X"), lo, hi,
                       extent[0] + float(construction.get("railing", 0.0)),
                       extent[0] + float(construction.get("railing", 0.0)),
                       plane + (PARAPET_THICK / 2.0) * inward, RAIL_SECTION)
            built += 2
    return built


def build_mezzanine_guard(cell: dict, extent: tuple, cells_by_id: dict, levels: dict,
                          construction, add) -> int:
    """A railing round a platform nested inside another cell, a storey above its floor.

    The garage's storage loft is the case (`HOUSE-00467`): a 27 m² platform at +2.90 over a slab at
    +0.15, with nothing at its edge. §70.5 asks for a guard at a drop over a metre, and it does not
    say the drop has to be outdoors. A container's interior is nested too and is 0.10 m over its
    room's floor, so the same test leaves it alone.
    """
    parent = cells_by_id.get(cell.get("parent"))
    if parent is None or not construction:
        return 0
    parent_level = levels.get(parent.get("level")) if levels else None
    parent_extent, _ = extent_of(parent, parent_level) if parent_level else (None, "")
    if parent_extent is None or extent[0] - parent_extent[0] <= 1.0:
        return 0
    height = float(construction.get("railing", 0.0))
    built = 0
    for box in cell_boxes(cell, extent):
        for side in ("-X", "+X", "-Z", "+Z"):
            plane, lo, hi = side_span(side, box)
            rail_along(add, side in ("-X", "+X"), lo, hi,
                       extent[0] + height, extent[0] + height, plane, RAIL_SECTION)
            built += 1
    return built


def build_cell(cell: dict, extent: tuple[float, float], *, neighbours=(), construction=None,
               level=None, levels=None, portals=(), openings=None, cells_by_id=None,
               flights=()):
    """One mesh object named for the cell: its floor, its ceiling and its walls' inner faces."""
    construction = construction or {}
    neighbours = list(neighbours)
    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[int, ...]] = []

    openings = openings or {}
    cells_by_id = cells_by_id or {}
    portal_cells = {row["id"]: (row.get("cellA"), row.get("cellB")) for row in portals}

    def add(points, outward) -> None:
        base = len(vertices)
        vertices.extend(to_blender(*point) for point in facing(points, outward))
        faces.append(tuple(range(base, base + len(points))))

    def solid(bx0, bx1, by0, by1, bz0, bz1) -> None:
        """A closed box, every face wound outward. Trim is looked at from every side."""
        if bx1 - bx0 <= 1e-9 or by1 - by0 <= 1e-9 or bz1 - bz0 <= 1e-9:
            return
        for value, outward in ((bx0, (-1.0, 0.0, 0.0)), (bx1, (1.0, 0.0, 0.0))):
            add([(value, by0, bz0), (value, by1, bz0), (value, by1, bz1), (value, by0, bz1)],
                outward)
        for value, outward in ((by0, (0.0, -1.0, 0.0)), (by1, (0.0, 1.0, 0.0))):
            add([(bx0, value, bz0), (bx1, value, bz0), (bx1, value, bz1), (bx0, value, bz1)],
                outward)
        for value, outward in ((bz0, (0.0, 0.0, -1.0)), (bz1, (0.0, 0.0, 1.0))):
            add([(bx0, by0, value), (bx1, by0, value), (bx1, by1, value), (bx0, by1, value)],
                outward)

    for box in cell_boxes(cell, extent):
        x0, x1, y0, y1, z0, z1 = box
        ix0, ix1, _, _, iz0, iz1 = inset_box(box, cell, neighbours, construction)

        # `HOUSE-00453`: one face per run of the side, at the inner face of the wall that bounds
        # THAT run. A shared wall is described once -- here, from the pair of cells that share the
        # plane -- and each cell carries the face that looks at it, so a cell's chunk is complete
        # on its own (§27's residency) and no two chunks hold the same surface twice.
        #
        # A run is CLAMPED to the room's inset extent on the perpendicular axis: at a corner the
        # two inner faces stop at each other, and a face that ran on to the centre line would
        # continue 75 mm into the wall it meets.
        for side, inward in INWARD.items():
            side_holes = holes_in(side, box, cell, list(portals))
            for lo, hi, wall in side_intervals(side, box, cell, neighbours):
                half = float(construction.get(wall, 0.0)) / 2.0
                plane = {"-X": x0 + half, "+X": x1 - half,
                         "-Z": z0 + half, "+Z": z1 - half}[side]
                clamp = (iz0, iz1) if side in ("-X", "+X") else (ix0, ix1)
                lo, hi = max(lo, clamp[0]), min(hi, clamp[1])
                if hi - lo <= 1e-6:
                    continue
                holes = [hole for hole in side_holes
                         if min(hole[1], hi) - max(hole[0], lo) > 1e-6]
                for pu0, pu1, pv0, pv1 in panel(lo, hi, y0, y1, holes):
                    if side in ("-X", "+X"):
                        corners = [(plane, pv0, pu0), (plane, pv1, pu0),
                                   (plane, pv1, pu1), (plane, pv0, pu1)]
                    else:
                        corners = [(pu0, pv0, plane), (pu0, pv1, plane),
                                   (pu1, pv1, plane), (pu1, pv0, plane)]
                    add(corners, inward)

                # `HOUSE-00454`: the OUTER face of the same wall, where there is an outside. A
                # partition has two rooms and each has its inner face; an exterior wall has one
                # room and the weather, and the weather's side is here.
                outside = wall != "wallPartition" and level is not None
                outer_plane = plane
                if outside:
                    outer_name = outer_wall_name(level, wall)
                    outer_half = float(construction.get(outer_name, 0.0)) / 2.0
                    outer_plane = {"-X": x0 - outer_half, "+X": x1 + outer_half,
                                   "-Z": z0 - outer_half, "+Z": z1 + outer_half}[side]
                    oy0, oy1 = outer_span(cell, (y0, y1), level, levels or {})
                    for pu0, pu1, pv0, pv1 in panel(lo, hi, oy0, oy1, holes):
                        if side in ("-X", "+X"):
                            outer = [(outer_plane, pv0, pu0), (outer_plane, pv1, pu0),
                                     (outer_plane, pv1, pu1), (outer_plane, pv0, pu1)]
                        else:
                            outer = [(pu0, pv0, outer_plane), (pu0, pv1, outer_plane),
                                     (pu1, pv1, outer_plane), (pu1, pv0, outer_plane)]
                        add(outer, tuple(-value for value in inward))

                # The reveal runs from this room's inner face to the outer face of an exterior
                # wall, or to the CENTRE LINE of a partition -- the room on the other side carries
                # its own half, for the same reason it carries its own inner face.
                far = outer_plane if outside else {"-X": x0, "+X": x1, "-Z": z0, "+Z": z1}[side]

                # `HOUSE-00456`: the door trim on THIS room's face -- two jambs, a head and a
                # threshold. A window's frame is `HOUSE-00457`'s; the two are different objects
                # with different sections, and lumping them together would give every window an
                # architrave and a doorstep.
                for hole in holes:
                    opening = openings.get(hole[4])
                    if opening is None or opening.get("kind") != "door":
                        continue
                    casing = float((opening.get("frame") or {}).get("casing") or 0.0)
                    hu0, hu1, hv0, hv1 = hole[0], hole[1], hole[2], hole[3]
                    near, deep = plane, plane + ARCHITRAVE_PROUD * (
                        1.0 if side in ("-X", "-Z") else -1.0)
                    lo_face, hi_face = min(near, deep), max(near, deep)
                    for bu0, bu1, bv0, bv1 in architrave_boards(hu0, hu1, hv0, hv1, casing):
                        if side in ("-X", "+X"):
                            solid(lo_face, hi_face, bv0, bv1, bu0, bu1)
                        else:
                            solid(bu0, bu1, bv0, bv1, lo_face, hi_face)
                    # The threshold: a board across the opening, this room's half of the wall.
                    sill_lo, sill_hi = min(plane, far), max(plane, far)
                    if side in ("-X", "+X"):
                        solid(sill_lo, sill_hi, hv0, hv0 + THRESHOLD_THICK, hu0, hu1)
                    else:
                        solid(hu0, hu1, hv0, hv0 + THRESHOLD_THICK, sill_lo, sill_hi)

                # `HOUSE-00458`: the skirting and the cornice, along the foot and the head of
                # this run of wall. Interrupted wherever an opening crosses the band -- a doorway
                # has no skirting across it, and neither does a sidelight whose 0.10 m sill is
                # below the board's 0.14 m top. Mitred at the room's corners: each board is
                # shortened by its own projection at an end that IS a corner, so the two boards
                # meet in the corner instead of overlapping in it.
                for height, base, name in ((float(construction.get("skirting", 0.0)), y0,
                                            "skirting"),
                                           (float(construction.get("cornice", 0.0)), None,
                                            "cornice")):
                    if height <= 0.0:
                        continue
                    band_lo = base if base is not None else y1 - height
                    band_hi = band_lo + height
                    crossing = [(hole[0], hole[1]) for hole in holes
                                if hole[3] > band_lo + 1e-9 and hole[2] < band_hi - 1e-9]
                    corner_lo, corner_hi = (iz0, iz1) if side in ("-X", "+X") else (ix0, ix1)
                    start, end = mitred(lo, hi, corner_lo, corner_hi, TRIM_PROUD)
                    proud_at = plane + TRIM_PROUD * (1.0 if side in ("-X", "-Z") else -1.0)
                    for bu0, bu1 in minus(start, end, crossing):
                        if side in ("-X", "+X"):
                            solid(min(plane, proud_at), max(plane, proud_at),
                                  band_lo, band_hi, bu0, bu1)
                        else:
                            solid(bu0, bu1, band_lo, band_hi,
                                  min(plane, proud_at), max(plane, proud_at))

                # `HOUSE-00457`: the window. Frame, sash, glass and -- for a double-hung -- the
                # meeting rail are generated ONCE, by the first of the portal's interior cells in
                # id order, because a window is one object and two rooms must not each build it.
                # The sill board is per-room, like the architrave: both rooms have one.
                for hole in holes:
                    opening = openings.get(hole[4])
                    if opening is None or opening.get("kind") != "window":
                        continue
                    hu0, hu1, hv0, hv1 = hole[0], hole[1], hole[2], hole[3]
                    near, far_side = plane, far
                    lo_face, hi_face = min(near, far_side), max(near, far_side)

                    def band(u0, u1, v0, v1, d0=lo_face, d1=hi_face, at=side):
                        if at in ("-X", "+X"):
                            solid(d0, d1, v0, v1, u0, u1)
                        else:
                            solid(u0, u1, v0, v1, d0, d1)

                    owner = window_owner(portal_cells.get(hole[4]), cells_by_id, cell["id"])
                    if owner == cell["id"]:
                        # The frame: a ring round the hole, filling the reveal's depth.
                        f = FRAME_SECTION
                        band(hu0, hu1, hv0, hv0 + f)
                        band(hu0, hu1, hv1 - f, hv1)
                        band(hu0, hu0 + f, hv0 + f, hv1 - f)
                        band(hu1 - f, hu1, hv0 + f, hv1 - f)
                        # The sash, inside the frame, and the glass inside the sash.
                        su0, su1, sv0, sv1 = hu0 + f, hu1 - f, hv0 + f, hv1 - f
                        g = SASH_SECTION
                        depth = (lo_face + hi_face) / 2.0
                        sash_lo, sash_hi = depth - g / 2.0, depth + g / 2.0
                        band(su0, su1, sv0, sv0 + g, sash_lo, sash_hi)
                        band(su0, su1, sv1 - g, sv1, sash_lo, sash_hi)
                        band(su0, su0 + g, sv0 + g, sv1 - g, sash_lo, sash_hi)
                        band(su1 - g, su1, sv0 + g, sv1 - g, sash_lo, sash_hi)
                        if has_meeting_rail(opening.get("type")):
                            middle = (sv0 + sv1) / 2.0
                            band(su0, su1, middle - MEETING_RAIL / 2.0,
                                 middle + MEETING_RAIL / 2.0, sash_lo, sash_hi)
                        band(su0 + g, su1 - g, sv0 + g, sv1 - g,
                             depth - GLASS_THICK / 2.0, depth + GLASS_THICK / 2.0)

                    # The sill board, projecting into THIS room under the opening.
                    casing = float((opening.get("frame") or {}).get("casing") or 0.0)
                    proud = plane + SILL_PROJECT * (1.0 if side in ("-X", "-Z") else -1.0)
                    band(hu0 - casing, hu1 + casing, hv0 - SILL_PROJECT, hv0,
                         min(plane, proud), max(plane, proud))

                for hu0, hu1, hv0, hv1, _portal_id in holes:
                    for corner_lo, corner_hi, along, look in (
                            (hv0, hv0, "v", (0.0, 1.0, 0.0)),      # the sill, looking up
                            (hv1, hv1, "v", (0.0, -1.0, 0.0)),     # the head, looking down
                            (hu0, hu0, "u", None), (hu1, hu1, "u", None)):
                        if along == "v":
                            edges = [(hu0, corner_lo), (hu1, corner_lo)]
                        else:
                            edges = [(corner_lo, hv0), (corner_lo, hv1)]
                        (au, av), (bu, bv) = edges
                        if side in ("-X", "+X"):
                            quad = [(plane, av, au), (plane, bv, bu), (far, bv, bu), (far, av, au)]
                        else:
                            quad = [(au, av, plane), (bu, bv, plane), (bu, bv, far), (au, av, far)]
                        if look is None:
                            # A jamb looks across the opening, towards the other jamb.
                            towards = (hu0 + hu1) / 2.0 - corner_lo
                            look = ((towards, 0.0, 0.0) if side in ("-Z", "+Z")
                                    else (0.0, 0.0, towards))
                        add(quad, look)

        # `HOUSE-00452`: the floor and the ceiling, inset to the same inner faces.
        for level_y, look in ((y0, (0.0, 1.0, 0.0)), (y1, (0.0, -1.0, 0.0))):
            if not slab_here(cell, extent, level_y == y0):
                continue
            wells = slab_holes(list(portals), cell, level_y, box)
            for px0, px1, pz0, pz1 in panel(ix0, ix1, iz0, iz1, wells):
                add([(px0, level_y, pz0), (px1, level_y, pz0),
                     (px1, level_y, pz1), (px0, level_y, pz1)], look)
            # `HOUSE-00460`: a railing round the hole in the FLOOR -- §70.5 asks for 1.05 m at a
            # drop over a metre and §12 declares 1.10 -- with a gap where the stair arrives. A
            # railing across the top of the flight would be a railing you have to climb.
            if level_y != y0 or not construction:
                continue
            arrivals = [top_tread_box(row, float(row.get("fromY") or 0.0))
                        for row in flights or () if row.get("toCell") == cell["id"]]
            for hx0, hx1, hz0, hz1, _identifier in wells:
                for axis_x, fixed, span in ((True, hz0, (hx0, hx1)), (True, hz1, (hx0, hx1)),
                                            (False, hx0, (hz0, hz1)), (False, hx1, (hz0, hz1))):
                    cuts = []
                    for tread in arrivals:
                        if tread is None:
                            continue
                        tx0, tx1, tz0, tz1, _top = tread
                        near = (min(abs(tz0 - fixed), abs(tz1 - fixed)) if axis_x
                                else min(abs(tx0 - fixed), abs(tx1 - fixed)))
                        if near > float(flight_going(flights, cell)) + 1e-6:
                            continue
                        cuts.append((tx0, tx1) if axis_x else (tz0, tz1))
                    for lo_at, hi_at in minus(span[0], span[1], cuts):
                        rail_along(add, axis_x, lo_at, hi_at,
                                   level_y + float(construction.get("railing", 0.0)),
                                   level_y + float(construction.get("railing", 0.0)),
                                   fixed, RAIL_SECTION)

    build_balcony_edge(cell, extent, list(neighbours), construction, solid, add)
    build_mezzanine_guard(cell, extent, cells_by_id, levels or {}, construction, add)

    # `HOUSE-00464`: a covered deck stands on columns and has a balustrade round its open sides.
    # The porch is the one cell in this house that is covered -- `L1_BALCONY_FRONT` sits on it --
    # and the open sides are the ones with no interior cell across them, which is the same test
    # the walls use.
    if (cell.get("kind") == "exterior" and construction
            and covered_by(cell, extent, cells_by_id)):
        for box in cell_boxes(cell, extent):
            bx0, bx1, by0, by1, bz0, bz1 = box
            open_sides = [side for side in ("-X", "+X", "-Z", "+Z")
                          if all(wall != "wallPartition"
                                 for _lo, _hi, wall in side_intervals(side, box, cell,
                                                                      list(neighbours)))]
            beam_lo = by1 - PORCH_BEAM
            half = COLUMN_SECTION / 2.0
            # The columns go along the longest open side, evenly spread including its two ends.
            longest = max(open_sides, key=lambda side: side_span(side, box)[2]
                          - side_span(side, box)[1], default=None)
            if longest is not None:
                plane, lo, hi = side_span(longest, box)
                for index in range(PORCH_COLUMNS):
                    at = lo + half + (hi - lo - COLUMN_SECTION) * index / (PORCH_COLUMNS - 1)
                    if longest in ("-X", "+X"):
                        solid(plane - half, plane + half, by0, beam_lo, at - half, at + half)
                    else:
                        solid(at - half, at + half, by0, beam_lo, plane - half, plane + half)
            for side in open_sides:
                plane, lo, hi = side_span(side, box)
                if side in ("-X", "+X"):
                    solid(plane - half, plane + half, beam_lo, by1, lo, hi)
                else:
                    solid(lo, hi, beam_lo, by1, plane - half, plane + half)
                # The balustrade between the columns, broken where the steps come up.
                drop = by0 - 0.0
                height = float(construction.get("railing" if drop > 1.0 else "balustrade", 0.0))
                cuts = []
                for row in flights or ():
                    step_print = row.get("footprint") or {}
                    if not step_print or row.get("toCell") != cell["id"]:
                        continue
                    cuts.append((float(step_print["x"][0]), float(step_print["x"][1]))
                                if side in ("-Z", "+Z")
                                else (float(step_print["z"][0]), float(step_print["z"][1])))
                for rail_lo, rail_hi in minus(lo, hi, cuts):
                    rail_along(add, side in ("-X", "+X"), rail_lo, rail_hi,
                               by0 + height, by0 + height, plane, RAIL_SECTION)

    # `HOUSE-00463`: the walkway boards in an unfinished attic store. A rafter-bounded level's
    # `closet` cells are the stores -- §13.6's "unfinished: rafters, insulation, walkway boards" --
    # and the finished attic room is `kind: room`, which is the only thing in the data that tells
    # them apart. The finished room's collar-tie ceiling is not built here: it is its own cell's
    # ceiling at §12.2's +12.60, which `HOUSE-00452` already laid.
    if (level or {}).get("ceiling") is None and cell.get("kind") == "closet":
        for box in cell_boxes(cell, extent):
            bx0, bx1, by0, _by1, bz0, bz1 = box
            if (bx1 - bx0) >= (bz1 - bz0):
                middle = (bz0 + bz1) / 2.0
                solid(bx0, bx1, by0, by0 + WALKWAY_THICK,
                      middle - WALKWAY_WIDTH / 2.0, middle + WALKWAY_WIDTH / 2.0)
            else:
                middle = (bx0 + bx1) / 2.0
                solid(middle - WALKWAY_WIDTH / 2.0, middle + WALKWAY_WIDTH / 2.0,
                      by0, by0 + WALKWAY_THICK, bz0, bz1)

    # `HOUSE-00459`: the flights that stand in this cell. A flight is carried by its `fromCell`,
    # the one it starts in, so it is built once and it is in the chunk of the room you are
    # standing in when you begin to climb.
    for flight in flights or ():
        if flight.get("fromCell") != cell["id"]:
            continue
        # The foot of the flight: what it declares if it declares one -- the porch, terrace and
        # garage steps join two cells on ONE level and are the only things that know what they
        # climb -- and otherwise the floor of the cell it stands in.
        foot = flight.get("fromY")
        build_flight(flight, solid, float(foot) if foot is not None else extent[0],
                     add=add, construction=construction,
                     inner=list(cell_boxes(cell, extent))[0])

    mesh = bpy.data.meshes.new(f"{cell['id']}_mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.validate()
    mesh.update()
    obj = bpy.data.objects.new(cell["id"], mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def export(obj, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.gltf(filepath=str(path), export_format="GLB", use_selection=True,
                              export_yup=True)


def generate(directory: Path, output: Path, wanted: set[str] | None = None) -> dict:
    """Every cell the layout declares, as one `.glb` each. Returns a report."""
    layout = layout_io.load_layout(directory, kinds=["levels", "cells", "portals", "openings", "stairs", "interactables"])
    levels = {row["id"]: row for row in layout_io.rows(layout, "levels")}
    portals = layout_io.rows(layout, "portals")
    openings = {row["portal"]: row for row in layout_io.rows(layout, "openings")
                if row.get("portal")}
    stair_rows = layout_io.rows(layout, "stairs")
    construction = (layout.get("levels") or {}).get("construction") or {}
    report = {"written": [], "skipped": [], "problems": []}

    neighbours = neighbour_boxes(layout, levels)

    for cell in sorted(layout_io.rows(layout, "cells"), key=lambda row: row["id"]):
        if wanted is not None and cell["id"] not in wanted:
            continue
        level = levels.get(cell.get("level"))
        if level is None:
            report["problems"].append(f"{cell['id']}: level {cell.get('level')!r} is not declared")
            continue
        extent, reason = extent_of(cell, level)
        if extent is None:
            report["skipped"].append(f"{cell['id']}: {reason}")
            continue
        reset_scene()
        obj = build_cell(cell, extent, neighbours=neighbours, construction=construction,
                         level=level, levels=levels, portals=portals, openings=openings,
                         cells_by_id={row["id"]: row for row in layout_io.rows(layout, "cells")},
                         flights=stair_rows)
        destination = output / f"{cell['id']}.glb"
        export(obj, destination)
        report["written"].append(cell["id"])

    fireplace = next((row for row in layout_io.rows(layout, "interactables")
                      if "FIREPLACE" in str(row.get("id"))), None)
    chimney = chimney_at(fireplace, construction,
                         float(levels[fireplace["cell"][:2] if False else "L0"]["ffl"])
                         if fireplace else 0.0)
    if chimney is not None and (wanted is None or "CHIMNEY" in wanted):
        reset_scene()
        reset_vertices: list = []
        reset_faces: list = []

        def chimney_add(points, outward) -> None:
            base = len(reset_vertices)
            reset_vertices.extend(to_blender(*point) for point in facing(points, outward))
            reset_faces.append(tuple(range(base, base + len(points))))

        cx0, cx1, cy0, cy1, cz0, cz1 = chimney
        for value, outward in ((cx0, (-1.0, 0.0, 0.0)), (cx1, (1.0, 0.0, 0.0))):
            chimney_add([(value, cy0, cz0), (value, cy1, cz0), (value, cy1, cz1),
                         (value, cy0, cz1)], outward)
        for value, outward in ((cz0, (0.0, 0.0, -1.0)), (cz1, (0.0, 0.0, 1.0))):
            chimney_add([(cx0, cy0, value), (cx1, cy0, value), (cx1, cy1, value),
                         (cx0, cy1, value)], outward)
        chimney_add([(cx0, cy1, cz0), (cx1, cy1, cz0), (cx1, cy1, cz1), (cx0, cy1, cz1)],
                    (0.0, 1.0, 0.0))
        mesh = bpy.data.meshes.new("CHIMNEY_mesh")
        mesh.from_pydata(reset_vertices, [], reset_faces)
        mesh.validate()
        mesh.update()
        stack = bpy.data.objects.new("CHIMNEY", mesh)
        bpy.context.scene.collection.objects.link(stack)
        export(stack, output / "CHIMNEY.glb")
        report["written"].append("CHIMNEY")

    for name, box in sorted(roof_boxes(layout, levels).items()):
        if wanted is not None and name not in wanted:
            continue
        reset_scene()
        obj = build_roof(name, box, construction,
                         dormers=dormers_on(box, portals, openings.values()))
        export(obj, output / f"{name}.glb")
        report["written"].append(name)
    return report


def build_roof(name: str, box: tuple, construction: dict, dormers=()):
    """One roof object over @p box, with its fascia. @p box is the WALL CENTRE-LINE rectangle."""
    half_wall = float(construction.get("wallExterior", 0.0)) / 2.0
    reach = half_wall + EAVES_OVERHANG
    outer = (box[0] - reach, box[1] + reach, box[2] - reach, box[3] + reach)
    eaves_y = eaves_height(construction, outer)
    pitch = float(construction["roofPitch"])

    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[int, ...]] = []

    def add(points, outward) -> None:
        base = len(vertices)
        vertices.extend(to_blender(*point) for point in facing(points, outward))
        faces.append(tuple(range(base, base + len(points))))

    for corners, outward in roof_planes(outer, eaves_y, pitch):
        add(corners, outward)

    # `HOUSE-00462`: the dormers, which belong to the roof they come through.
    for rect_u, rect_v, plane_z in dormers or ():
        outward = 1.0 if abs(plane_z - box[3]) < abs(plane_z - box[2]) else -1.0
        for corners, face_outward in dormer_shell(rect_u, rect_v, plane_z, outward, outer,
                                                  eaves_y, pitch):
            add(corners, face_outward)

    # The fascia: a board round the eaves edge, hanging below it, and the soffit closing the
    # underside back to the wall. Without them you see the roof planes end in mid-air.
    x0, x1, z0, z1 = outer
    for corners, outward in (
            ([(x0, eaves_y - FASCIA_DEPTH, z0), (x1, eaves_y - FASCIA_DEPTH, z0),
              (x1, eaves_y, z0), (x0, eaves_y, z0)], (0.0, 0.0, -1.0)),
            ([(x1, eaves_y - FASCIA_DEPTH, z1), (x0, eaves_y - FASCIA_DEPTH, z1),
              (x0, eaves_y, z1), (x1, eaves_y, z1)], (0.0, 0.0, 1.0)),
            ([(x0, eaves_y - FASCIA_DEPTH, z1), (x0, eaves_y - FASCIA_DEPTH, z0),
              (x0, eaves_y, z0), (x0, eaves_y, z1)], (-1.0, 0.0, 0.0)),
            ([(x1, eaves_y - FASCIA_DEPTH, z0), (x1, eaves_y - FASCIA_DEPTH, z1),
              (x1, eaves_y, z1), (x1, eaves_y, z0)], (1.0, 0.0, 0.0))):
        add(corners, outward)
    soffit_y = eaves_y - FASCIA_DEPTH + FASCIA_THICK
    for corners in (
            [(x0, soffit_y, z0), (x1, soffit_y, z0),
             (x1, soffit_y, z0 + reach), (x0, soffit_y, z0 + reach)],
            [(x0, soffit_y, z1 - reach), (x1, soffit_y, z1 - reach),
             (x1, soffit_y, z1), (x0, soffit_y, z1)],
            [(x0, soffit_y, z0 + reach), (x0 + reach, soffit_y, z0 + reach),
             (x0 + reach, soffit_y, z1 - reach), (x0, soffit_y, z1 - reach)],
            [(x1 - reach, soffit_y, z0 + reach), (x1, soffit_y, z0 + reach),
             (x1, soffit_y, z1 - reach), (x1 - reach, soffit_y, z1 - reach)]):
        add(corners, (0.0, -1.0, 0.0))

    # `HOUSE-00463`: the rafters and the purlins, under the two long planes. The hip ends carry
    # jack rafters in a real roof and none here: they are a different length each and the attic's
    # unfinished stores -- the only place you see structure -- are under the long slopes.
    dx, dz = x1 - x0, z1 - z0
    if min(dx, dz) > 0.0:
        half = min(dx, dz) / 2.0
        top = eaves_y + half * pitch
        along0, along1 = (x0 + half, x1 - half) if dx >= dz else (z0 + half, z1 - half)
        count = int((along1 - along0) / RAFTER_SPACING)
        for index in range(count + 1):
            at = along0 + index * RAFTER_SPACING
            if at > along1 + 1e-9:
                break
            for side in (-1.0, 1.0):
                near = (z0 if side < 0 else z1) if dx >= dz else (x0 if side < 0 else x1)
                mid = ((z0 + z1) / 2.0) if dx >= dz else ((x0 + x1) / 2.0)
                if dx >= dz:
                    rafter = [(at - RAFTER_WIDTH / 2.0, eaves_y - RAFTER_DEPTH, near),
                              (at + RAFTER_WIDTH / 2.0, eaves_y - RAFTER_DEPTH, near),
                              (at + RAFTER_WIDTH / 2.0, top - RAFTER_DEPTH, mid),
                              (at - RAFTER_WIDTH / 2.0, top - RAFTER_DEPTH, mid)]
                else:
                    rafter = [(near, eaves_y - RAFTER_DEPTH, at - RAFTER_WIDTH / 2.0),
                              (near, eaves_y - RAFTER_DEPTH, at + RAFTER_WIDTH / 2.0),
                              (mid, top - RAFTER_DEPTH, at + RAFTER_WIDTH / 2.0),
                              (mid, top - RAFTER_DEPTH, at - RAFTER_WIDTH / 2.0)]
                add(rafter, (0.0, -1.0, 0.0))
        # A purlin under each slope, halfway up it.
        for side in (-1.0, 1.0):
            near = (z0 if side < 0 else z1) if dx >= dz else (x0 if side < 0 else x1)
            mid = ((z0 + z1) / 2.0) if dx >= dz else ((x0 + x1) / 2.0)
            at = (near + mid) / 2.0
            level = (eaves_y + top) / 2.0 - RAFTER_DEPTH
            if dx >= dz:
                add([(along0, level - PURLIN_SECTION, at - PURLIN_SECTION / 2.0),
                     (along1, level - PURLIN_SECTION, at - PURLIN_SECTION / 2.0),
                     (along1, level, at - PURLIN_SECTION / 2.0),
                     (along0, level, at - PURLIN_SECTION / 2.0)], (0.0, 0.0, -1.0))
            else:
                add([(at - PURLIN_SECTION / 2.0, level - PURLIN_SECTION, along0),
                     (at - PURLIN_SECTION / 2.0, level - PURLIN_SECTION, along1),
                     (at - PURLIN_SECTION / 2.0, level, along1),
                     (at - PURLIN_SECTION / 2.0, level, along0)], (-1.0, 0.0, 0.0))

    # `HOUSE-00468`: a gutter along each eaves edge, a downspout at each corner, and a vent along
    # the ridge. The gutter hangs on the fascia, so its height comes from the fascia's.
    gutter_y = eaves_y - FASCIA_DEPTH
    for side_x in (False, True):
        for at in ((x0, x1) if side_x else (z0, z1)):
            if side_x:
                box_of = (at - GUTTER_SECTION / 2.0, at + GUTTER_SECTION / 2.0,
                          gutter_y, gutter_y + GUTTER_SECTION, z0, z1)
            else:
                box_of = (x0, x1, gutter_y, gutter_y + GUTTER_SECTION,
                          at - GUTTER_SECTION / 2.0, at + GUTTER_SECTION / 2.0)
            for value, outward in ((box_of[0], (-1.0, 0.0, 0.0)), (box_of[1], (1.0, 0.0, 0.0))):
                add([(value, box_of[2], box_of[4]), (value, box_of[3], box_of[4]),
                     (value, box_of[3], box_of[5]), (value, box_of[2], box_of[5])], outward)
            for value, outward in ((box_of[2], (0.0, -1.0, 0.0)), (box_of[3], (0.0, 1.0, 0.0))):
                add([(box_of[0], value, box_of[4]), (box_of[1], value, box_of[4]),
                     (box_of[1], value, box_of[5]), (box_of[0], value, box_of[5])], outward)
    half_spout = DOWNSPOUT_SECTION / 2.0
    for corner_x in (x0, x1):
        for corner_z in (z0, z1):
            for value, outward in ((corner_x - half_spout, (-1.0, 0.0, 0.0)),
                                   (corner_x + half_spout, (1.0, 0.0, 0.0))):
                add([(value, 0.0, corner_z - half_spout), (value, gutter_y, corner_z - half_spout),
                     (value, gutter_y, corner_z + half_spout), (value, 0.0, corner_z + half_spout)],
                    outward)
    ridge_top = eaves_y + (min(x1 - x0, z1 - z0) / 2.0) * pitch
    if dx >= dz:
        vent = (x0 + (z1 - z0) / 2.0, x1 - (z1 - z0) / 2.0)
        add([(vent[0], ridge_top + RIDGE_VENT_HEIGHT, (z0 + z1) / 2.0 - RIDGE_VENT_WIDTH / 2.0),
             (vent[1], ridge_top + RIDGE_VENT_HEIGHT, (z0 + z1) / 2.0 - RIDGE_VENT_WIDTH / 2.0),
             (vent[1], ridge_top + RIDGE_VENT_HEIGHT, (z0 + z1) / 2.0 + RIDGE_VENT_WIDTH / 2.0),
             (vent[0], ridge_top + RIDGE_VENT_HEIGHT, (z0 + z1) / 2.0 + RIDGE_VENT_WIDTH / 2.0)],
            (0.0, 1.0, 0.0))

    mesh = bpy.data.meshes.new(f"{name}_mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.validate()
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def chimney_at(fireplace, construction: dict, floor: float):
    """The chimney's box, from the hearth to `CHIMNEY_OVER_RIDGE` above §12's ridge.

    Where it stands is the fireplace's own position -- `APPL_L0_LIVING_FIREPLACE`, on
    `L0_LIVING`'s west wall -- so moving the fireplace moves the chimney. How high it goes is §12's
    `ridgeY` plus the height a stack has to clear a ridge by. Only its section is invented.
    """
    if fireplace is None:
        return None
    point = (fireplace.get("focus") or {}).get("point")
    if not isinstance(point, list) or len(point) != 3:
        return None
    x, _y, z = (float(value) for value in point)
    return (x - CHIMNEY_ACROSS / 2.0, x + CHIMNEY_ACROSS / 2.0,
            floor, float(construction["ridgeY"]) + CHIMNEY_OVER_RIDGE,
            z - CHIMNEY_ALONG / 2.0, z + CHIMNEY_ALONG / 2.0)


def dormers_on(box: tuple, portals, openings) -> list:
    """`(u range, v range, plane)` for every `W_DORMER` window in the walls under @p box."""
    by_id = {row["id"]: row for row in portals}
    out = []
    for opening in openings:
        if str(opening.get("type") or "") != "W_DORMER":
            continue
        portal = by_id.get(opening.get("portal"))
        plane = (portal or {}).get("plane") or {}
        if plane.get("axis") != "z":
            continue
        value = float(plane["value"])
        if not (box[2] - 1e-6 <= value <= box[3] + 1e-6):
            continue
        rect = portal["rect"]
        out.append(((float(rect["u"][0]), float(rect["u"][1])),
                    (float(rect["v"][0]), float(rect["v"][1])), value))
    return sorted(out)


def roof_boxes(layout: dict, levels: dict) -> dict:
    """`{name: centre-line rectangle}` for every roof this generator builds.

    `ROOF_MAIN` covers the attic, which is what `layout.levels.json` says: `L3` declares
    `"roof": "ROOF_MAIN"` and its cells ARE the main block. `ROOF_GARAGE` covers the garage, the
    projecting wing §12.1 describes; the sunroom's roof is flat and is the rear balcony's floor,
    so it is a cell's ceiling and not a roof.
    """
    out = {}
    attic = [row for row in layout_io.rows(layout, "cells") if row.get("level") == "L3"]
    if attic:
        boxes = [box for row in attic for box in (row.get("boxes") or [])]
        out["ROOF_MAIN"] = (min(float(b["x"][0]) for b in boxes),
                            max(float(b["x"][1]) for b in boxes),
                            min(float(b["z"][0]) for b in boxes),
                            max(float(b["z"][1]) for b in boxes))
    garage = next((row for row in layout_io.rows(layout, "cells")
                   if row.get("kind") == "garage"), None)
    if garage:
        boxes = garage.get("boxes") or []
        out["ROOF_GARAGE"] = (min(float(b["x"][0]) for b in boxes),
                              max(float(b["x"][1]) for b in boxes),
                              min(float(b["z"][0]) for b in boxes),
                              max(float(b["z"][1]) for b in boxes))
    return out


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()[:16]


# ============================================================================== selftest =========


def selftest(output: Path) -> int:
    failures = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("house_shell_gen: selftest")

    require(to_blender(1.0, 2.0, 3.0) == (1.0, -3.0, 2.0),
            f"a world point becomes a Blender point ({to_blender(1.0, 2.0, 3.0)})")

    if not (SOURCE / "layout.cells.json").is_file():
        require(False, "the authored world is here to generate from")
        print(f"house_shell_gen: {len(failures)} claim(s) FAILED")
        return 1

    # One cell with a known box, exported and then READ BACK. The claim that matters is not that
    # the code computed a coordinate but that the coordinate arrived in the file.
    layout = layout_io.load_layout(SOURCE, kinds=["levels", "cells"])
    portal_rows = {row["id"]: row for row in layout_io.rows(
        layout_io.load_layout(SOURCE, kinds=["portals"]), "portals")}
    openings_by_portal = {row["portal"]: row for row in layout_io.rows(
        layout_io.load_layout(SOURCE, kinds=["openings"]), "openings") if row.get("portal")}
    levels = {row["id"]: row for row in layout_io.rows(layout, "levels")}
    cells = {row["id"]: row for row in layout_io.rows(layout, "cells")}
    subject = cells["L0_KITCHEN"]
    extent = layout_io.cell_extent(subject, levels[subject["level"]])
    report = generate(SOURCE, output, wanted={"L0_KITCHEN"})
    require(report["written"] == ["L0_KITCHEN"] and not report["problems"],
            f"the kitchen exports ({report})")

    path = output / "L0_KITCHEN.glb"
    document, error = gltf_validate.read_gltf_json(path)
    require(document is not None, f"and the file is a glTF the project's own reader accepts "
                                  f"({error})")
    if document is not None:
        bounds = None
        for accessor in document.get("accessors", []):
            if accessor.get("type") == "VEC3" and "min" in accessor and "max" in accessor:
                bounds = (accessor["min"], accessor["max"])
                break
        require(bounds is not None, "with a POSITION accessor carrying its bounds")
        if bounds is not None:
            box = list(cell_boxes(subject, extent))[0]
            walls = neighbour_boxes(layout, levels)
            construction_now = (layout.get("levels") or {}).get("construction") or {}
            inner = inset_box(box, subject, walls, construction_now)
            # Three sides are partitions, so the mesh stops at their inner faces; the north side
            # has an exterior run, so it reaches the OUTER face of a 0.30 m wall, half a thickness
            # the other side of the centre line. And it rises past this storey's 3.30 ceiling to
            # `L1`'s 3.65 floor, because the outer skin carries the band at the joists.
            outer_z = box[4] - float(construction_now["wallExterior"]) / 2.0
            # On the three partitioned sides the mesh reaches the CENTRE LINE, because a
            # partition's reveal runs to it -- that is what "each room carries its half of the
            # opening" means, and the room on the other side carries the other half.
            want_min = [box[0], inner[2], outer_z]
            want_max = [box[1], float(levels["L1"]["ffl"]), box[5]]
            close = all(abs(a - b) < 1e-4 for a, b in zip(bounds[0], want_min)) and \
                all(abs(a - b) < 1e-4 for a, b in zip(bounds[1], want_max))
            require(close,
                    f"and the exported cell spans its inner faces and its outer skin: "
                    f"{[round(v, 3) for v in bounds[0]]}..{[round(v, 3) for v in bounds[1]]} "
                    f"against {[round(v, 3) for v in want_min]}.."
                    f"{[round(v, 3) for v in want_max]}")
            exterior_inner = box[4] + float(construction_now["wallExterior"]) / 2.0
            require(abs((exterior_inner - outer_z) - float(construction_now["wallExterior"]))
                    < 1e-6,
                    f"so the north wall's two faces are {construction_now['wallExterior']} m "
                    f"apart, straddling the centre line the layout stores — which is what §12's "
                    f"construction block says it is")
            require(bounds[1][1] > box[3] + 1e-6,
                    f"and the skin passes this storey's ceiling ({box[3]}) to reach the next "
                    f"storey's floor ({bounds[1][1]}), so there is no slot round the house at the "
                    f"joists")
            require(abs(bounds[0][1] - extent[0]) < 1e-4,
                    f"and it stands on the floor the layout gives it ({extent[0]})")

    require(len(document.get("meshes", [])) == 1,
            f"one mesh per cell ({len(document.get('meshes', []))})")

    # Determinism (§18.4). Byte-identical, not "the same shape".
    before = digest(path)
    generate(SOURCE, output, wanted={"L0_KITCHEN"})
    require(digest(path) == before, "a second run over an unchanged layout writes the same bytes")

    # ---- `HOUSE-00452`: the inset, per side --------------------------------------------------
    construction = (layout.get("levels") or {}).get("construction") or {}
    neighbours = neighbour_boxes(layout, levels)

    kitchen_box = list(cell_boxes(subject, extent))[0]
    walls = {side: wall_across(side, kitchen_box, subject, neighbours, construction)
             for side in ("-X", "+X", "-Z", "+Z")}
    require(set(walls.values()) == {"wallPartition"},
            f"the kitchen's thinnest wall on every side is a partition -- three of them entirely, "
            f"and the fourth for most of its length ({walls})")
    # ...and that fourth side is MIXED, which is the case a single wall per side gets wrong.
    north = side_intervals("-Z", kitchen_box, subject, neighbours)
    require(len(north) == 2 and {wall for _lo, _hi, wall in north}
            == {"wallPartition", "wallExterior"},
            f"its north side is 8.9 m of partition against the sunroom and 1.5 m of exterior "
            f"wall beside it ({[(round(a, 2), round(b, 2), w) for a, b, w in north]})")
    require(abs(sum(hi - lo for lo, hi, _w in north)
                - (kitchen_box[1] - kitchen_box[0])) < 1e-6,
            "and the runs cover the side exactly once, with no gap and no overlap")
    family = cells["L0_FAMILY"]
    family_box = list(cell_boxes(family, extent_of(family, levels[family["level"]])[0]))[0]
    require(wall_across("+X", family_box, family, neighbours, construction) == "wallExterior",
            "the family room's east side has nothing across it, so it is an exterior wall")
    family_north = side_intervals("-Z", family_box, family, neighbours)
    require(any(wall == "wallPartition" for _lo, _hi, wall in family_north)
            and any(wall == "wallExterior" for _lo, _hi, wall in family_north),
            f"and its north side clips the corner of the sunroom for half a metre and is the "
            f"outside wall for the other six "
            f"({[(round(a, 2), round(b, 2), w) for a, b, w in family_north]})")
    # The case the exterior rule exists for: `L0_PORCH` is a CELL, it abuts the foyer's front
    # face, and the wall between them is the front of the house.
    foyer = cells["L0_FOYER"]
    foyer_box = list(cell_boxes(foyer, extent_of(foyer, levels[foyer["level"]])[0]))[0]
    require(wall_across("+Z", foyer_box, foyer, neighbours, construction) == "wallExterior",
            "the porch abuts the foyer's front and is still outside, so their shared wall is "
            "exterior")
    lawn = wall_across("+Z", list(cell_boxes(cells["B1_CELLAR"],
                                             extent_of(cells["B1_CELLAR"],
                                                       levels["B1"])[0]))[0],
                       cells["B1_CELLAR"], neighbours, construction)
    require(lawn == "wallPartition", f"a basement cell against another is a partition ({lawn})")
    garage = cells["L0_GARAGE"]
    garage_box = list(cell_boxes(garage, extent_of(garage, levels[garage["level"]])[0]))[0]
    require({wall_across(side, garage_box, garage, neighbours, construction)
             for side in ("-X", "+X", "-Z", "+Z")} == {"wallGarage"},
            "and every wall of the garage is a garage wall, from either side")

    inset = inset_box(kitchen_box, subject, neighbours, construction)
    half = float(construction["wallPartition"]) / 2.0
    require(all(abs(a - b) < 1e-9 for a, b in
                ((inset[0], kitchen_box[0] + half), (inset[1], kitchen_box[1] - half),
                 (inset[4], kitchen_box[4] + half), (inset[5], kitchen_box[5] - half))),
            f"so its floor stops {half:.3f} m short on every side -- the THINNEST wall on each, "
            f"which runs the floor under the thicker one rather than leaving a slot short of it "
            f"({inset[0]:.3f}…{inset[1]:.3f} by {inset[4]:.3f}…{inset[5]:.3f})")
    require(north[0][2] == "wallExterior" and north[1][2] == "wallPartition"
            and abs(inset[4] - (kitchen_box[4] + float(construction["wallExterior"]) / 2.0)) > 1e-6,
            "and the mixed north side runs exterior FIRST and partition second, so taking the "
            "thinnest is not the same answer as taking the first one found")
    require(abs(inset[2] - kitchen_box[2]) < 1e-9 and abs(inset[3] - kitchen_box[3]) < 1e-9,
            "and the inset moves the floor in, never up or down")
    walkable = (inset[1] - inset[0]) * (inset[5] - inset[4])
    centre = (kitchen_box[1] - kitchen_box[0]) * (kitchen_box[5] - kitchen_box[4])
    require(walkable < centre,
            f"the walkable floor is smaller than §13's centre-line area: {walkable:.2f} m² of "
            f"{centre:.2f} m²")

    # Six faces a box: four sides on the centre line and two inset slabs. And they face the right
    # way -- a floor whose normal points down is invisible from the room and lit from underneath.
    # The walls alone, with the trim switched off the way the DATA switches it off: a construction
    # block that declares no skirting and no cornice gets none. No test-only knob.
    plain = dict(construction, skirting=0.0, cornice=0.0)
    reset_scene()
    obj = build_cell(subject, extent, neighbours=neighbours, construction=plain,
                     level=levels[subject["level"]], levels=levels, cells_by_id=cells)
    polygons = list(obj.data.polygons)
    all_runs = [(side, run) for side in ("-X", "+X", "-Z", "+Z")
                for run in side_intervals(side, kitchen_box, subject, neighbours)]
    outside_runs = [run for _side, run in all_runs if run[2] != "wallPartition"]
    require(len(polygons) == len(all_runs) + len(outside_runs) + 2,
            f"one face per run, a second for each run that has weather on the other side, plus a "
            f"floor and a ceiling ({len(polygons)} for {len(all_runs)} runs of which "
            f"{len(outside_runs)} are outside walls)")
    require(sum(1 for face in polygons if face.normal.z > 0.99) == 1
            and sum(1 for face in polygons if face.normal.z < -0.99) == 1,
            "with the trim off, exactly one face looks up and one looks down: the floor and the "
            "ceiling")

    # Every face looks INTO the room. These are the inner faces of the walls, seen from the only
    # place anyone stands; the outside of the house is `HOUSE-00454`'s. A face that points the
    # other way is invisible from the room it bounds, and no claim about coordinates can see it.
    centre = ((kitchen_box[0] + kitchen_box[1]) / 2.0,
              (kitchen_box[2] + kitchen_box[3]) / 2.0,
              (kitchen_box[4] + kitchen_box[5]) / 2.0)
    blender_centre = to_blender(*centre)
    inward = [face for face in polygons
              if sum(n * (c - p) for n, c, p in zip(face.normal, blender_centre, face.center)) > 0]
    require(len(inward) == len(all_runs) + 2,
            f"every inner face and both slabs look into the room ({len(inward)} of "
            f"{len(all_runs) + 2})")
    require(len(polygons) - len(inward) == len(outside_runs),
            f"and every outer face looks away from it, at the weather "
            f"({len(polygons) - len(inward)} of {len(outside_runs)})")

    # Below grade the same run is a foundation wall. Read from the level's `ffl`, so a house with
    # a second basement gets the same answer without this tool learning its name.
    require(outer_wall_name(levels["B1"], "wallExterior") == "foundationWall"
            and outer_wall_name(levels["L0"], "wallExterior") == "wallExterior",
            "an exterior run below grade is a foundation wall and one above it is not")
    require(outer_wall_name(levels["B1"], "wallGarage") == "wallGarage",
            "...and a garage wall stays a garage wall wherever it is")

    # The band at the joists, from the level's own numbers rather than from a constant.
    span = outer_span(subject, extent, levels["L0"], levels)
    require(span == (extent[0], float(levels["L1"]["ffl"])),
            f"an outer run runs from its own floor to the NEXT storey's, {span}")
    top = cells["L3_STORE_W"]
    top_extent = extent_of(top, levels["L3"])[0]
    require(outer_span(top, top_extent, levels["L3"], levels)[1] == top_extent[1],
            "and the topmost storey's stops at its own ceiling, because there is no next one")

    # ---- `HOUSE-00458`: the skirting and the cornice ------------------------------------------
    reset_scene()
    trimmed = build_cell(subject, extent, neighbours=neighbours, construction=construction,
                         level=levels[subject["level"]], levels=levels, cells_by_id=cells)
    require(len(trimmed.data.polygons) == len(polygons) + 12 * len(all_runs),
            f"every run of wall gains a skirting and a cornice, six faces each "
            f"({len(polygons)} -> {len(trimmed.data.polygons)} over {len(all_runs)} runs)")
    require(abs(float(construction["skirting"]) - 0.14) < 1e-9
            and abs(float(construction["cornice"]) - 0.11) < 1e-9,
            f"and their heights are §12's, from the construction block "
            f"({construction['skirting']}, {construction['cornice']})")
    require(minus(0.0, 10.0, [(2.0, 3.0), (6.0, 7.0)]) == [(0.0, 2.0), (3.0, 6.0), (7.0, 10.0)],
            "a board is interrupted by what crosses it and continues after it")
    require(minus(0.0, 10.0, [(-1.0, 11.0)]) == [],
            "and a doorway the width of the wall leaves no board at all")
    require(minus(0.0, 10.0, [(2.0, 5.0), (3.0, 4.0)]) == [(0.0, 2.0), (5.0, 10.0)],
            "an interruption inside another one does not reopen the board between them")
    require(mitred(0.0, 4.0, 0.0, 4.0, 0.018) == (0.018, 3.982),
            "a board that runs corner to corner is shortened at both ends, so the two boards "
            "meeting there mitre instead of overlapping")
    require(mitred(1.0, 4.0, 0.0, 4.0, 0.018) == (1.0, 3.982),
            "...and an end that is not a corner is left alone, or the boards would part company "
            "in the middle of a wall")


    # The mesh follows `minus`: with the doorways cut, the boards are the pieces it returns.
    reset_scene()
    pierced_plain = build_cell(subject, extent, neighbours=neighbours, construction=plain,
                               level=levels[subject["level"]], levels=levels,
                               portals=list(portal_rows.values()), cells_by_id=cells)
    plain_faces = len(pierced_plain.data.polygons)
    reset_scene()
    pierced = build_cell(subject, extent, neighbours=neighbours, construction=construction,
                         level=levels[subject["level"]], levels=levels,
                         portals=list(portal_rows.values()), cells_by_id=cells)
    boards = 0
    for side in ("-X", "+X", "-Z", "+Z"):
        side_holes_all = holes_in(side, kitchen_box, subject, list(portal_rows.values()))
        inner = inset_box(kitchen_box, subject, neighbours, construction)
        corner = (inner[4], inner[5]) if side in ("-X", "+X") else (inner[0], inner[1])
        for lo, hi, _wall in side_intervals(side, kitchen_box, subject, neighbours):
            here = [hole for hole in side_holes_all if min(hole[1], hi) - max(hole[0], lo) > 1e-6]
            for height, base in ((float(construction["skirting"]), kitchen_box[2]),
                                 (float(construction["cornice"]),
                                  kitchen_box[3] - float(construction["cornice"]))):
                crossed = [(hole[0], hole[1]) for hole in here
                           if hole[3] > base + 1e-9 and hole[2] < base + height - 1e-9]
                start, end = mitred(lo, hi, corner[0], corner[1], TRIM_PROUD)
                boards += len(minus(start, end, crossed))
    require(len(pierced.data.polygons) == plain_faces + 6 * boards,
            f"the kitchen's skirting and cornice come to {boards} boards once the doorways have "
            f"broken them ({plain_faces} -> {len(pierced.data.polygons)})")

    tops = {round(vertex.co.z, 6) for vertex in pierced.data.vertices}
    require(round(kitchen_box[3] - float(construction["cornice"]), 6) in tops,
            f"and the cornice hangs under the ceiling, not down at the floor "
            f"({kitchen_box[3] - float(construction['cornice']):.3f} m)")

    # ---- `HOUSE-00455`: the openings ----------------------------------------------------------
    #
    # `panel` is a rectangle with rectangular bites out of it. The two things that can go wrong are
    # that it covers a hole and that it loses area somewhere else, so both are measured rather than
    # one of them assumed.
    pieces = panel(0.0, 10.0, 0.0, 3.0, [(2.0, 3.0, 0.0, 2.1), (6.0, 7.2, 0.9, 2.4)])
    covered = sum((u1 - u0) * (v1 - v0) for u0, u1, v0, v1 in pieces)
    bites = 1.0 * 2.1 + 1.2 * 1.5
    require(abs(covered + bites - 30.0) < 1e-9,
            f"a panel plus its holes is the whole wall: {covered:.3f} + {bites:.3f} of 30")
    overlap = any(min(a[1], b[1]) - max(a[0], b[0]) > 1e-9 and min(a[3], b[3]) - max(a[2], b[2])
                  > 1e-9 for index, a in enumerate(pieces) for b in pieces[index + 1:])
    require(not overlap, "and no two of its pieces overlap, so nothing is drawn twice")
    require(not panel(0.0, 2.0, 0.0, 2.0, [(0.0, 2.0, 0.0, 2.0)]),
            "a hole the size of the wall leaves no wall")

    # The acceptance criterion, over the whole house: every opening is a hole in the right wall.
    missing = []
    for opening in layout_io.rows(layout_io.load_layout(SOURCE, kinds=["openings"]), "openings"):
        portal = portal_rows.get(opening.get("portal"))
        if portal is None or (portal.get("plane") or {}).get("axis") == "y":
            continue
        for side_of, across in ((portal["cellA"], portal["cellB"]),
                                (portal["cellB"], portal["cellA"])):
            cell = cells.get(side_of)
            if cell is None or cell.get("kind") == "exterior":
                continue  # a yard has no wall to cut; the room on the other side has it
            if (cells.get(across) or {}).get("parent") == side_of:
                continue  # a container's door is a hole in the CONTAINER, not in the room's wall
            cut = False
            for box in cell_boxes(cell, extent_of(cell, levels[cell["level"]])[0]):
                for side in ("-X", "+X", "-Z", "+Z"):
                    for hole in holes_in(side, box, cell, [portal]):
                        want = (float(portal["rect"]["v"][0]), float(portal["rect"]["v"][1]))
                        if abs(hole[2] - want[0]) < 1e-6 and abs(hole[3] - want[1]) < 1e-6:
                            cut = True
            if not cut:
                missing.append(f"{opening['id']} in {side_of}")
    require(not missing,
            f"every opening in layout.openings.json is a hole of its own height in the wall of "
            f"both cells it joins ({len(missing)} are not: {missing[:3]})")

    # A cased opening has no leaf and is still a doorway. Counting doors is §13.1's business;
    # a hole in a wall is this tool's, and a generator that cut only apertured portals would wall
    # up every archway in the house.
    cased = [row for row in portal_rows.values()
             if not row.get("aperture") and (row.get("plane") or {}).get("axis") != "y"]
    require(cased, "the house has cased openings to check")
    uncut = []
    for row in cased:
        for side_of in (row["cellA"], row["cellB"]):
            cell = cells.get(side_of)
            if cell is None or cell.get("kind") == "exterior":
                continue
            if not any(holes_in(side, box, cell, [row])
                       for box in cell_boxes(cell, extent_of(cell, levels[cell["level"]])[0])
                       for side in ("-X", "+X", "-Z", "+Z")):
                uncut.append(f"{row['id']} in {side_of}")
    require(not uncut,
            f"and all {len(cased)} of them are cut too, in both rooms ({uncut[:3]})")

    # ...and a portal in one wall is not a hole in another. The plane's VALUE has to match, not
    # only its axis, or every room with a door would have the same door in its opposite wall.
    on_plus_z = [row for row in portal_rows.values()
                 if subject["id"] in (row.get("cellA"), row.get("cellB"))
                 and (row.get("plane") or {}).get("axis") == "z"
                 and abs(float(row["plane"]["value"]) - kitchen_box[5]) < 1e-6]
    require(on_plus_z, "the kitchen has a portal in its south wall")
    require(not holes_in("-Z", kitchen_box, subject, on_plus_z),
            "which is not a hole in its north wall")

    # And the mesh really gains the bites: the kitchen's walls are cut by its portals.
    reset_scene()
    with_holes = build_cell(subject, extent, neighbours=neighbours, construction=construction,
                            level=levels[subject["level"]], levels=levels,
                            portals=list(portal_rows.values()), cells_by_id=cells)
    holes_faces = len(with_holes.data.polygons)   # read now: `reset_scene` invalidates the object
    require(holes_faces > len(polygons),
            f"cutting the kitchen's openings adds faces to it, {len(polygons)} -> {holes_faces}")

    # ---- `HOUSE-00456`: the door trim ---------------------------------------------------------
    portal_cell_sides = {row["id"]: (row.get("cellA"), row.get("cellB"))
                         for row in portal_rows.values()}
    # ...and over the real house: the kitchen's doorway to the pantry breaks the skirting on the
    # wall it is in, and the window above it does not.
    door_side = next(side for side in ("-X", "+X", "-Z", "+Z")
                     for hole in holes_in(side, kitchen_box, subject, list(portal_rows.values()))
                     if (openings_by_portal.get(hole[4]) or {}).get("kind") == "door")
    side_holes_here = holes_in(door_side, kitchen_box, subject, list(portal_rows.values()))
    skirting_top = kitchen_box[2] + float(construction["skirting"])
    crossing = [(hole[0], hole[1]) for hole in side_holes_here
                if hole[3] > kitchen_box[2] + 1e-9 and hole[2] < skirting_top - 1e-9]
    require(crossing, f"something crosses the skirting on the kitchen's {door_side} wall")
    span = side_span(door_side, kitchen_box)
    require(len(minus(span[1], span[2], crossing)) == len(crossing) + 1,
            f"so the board runs up to each doorway and starts again after it "
            f"({len(minus(span[1], span[2], crossing))} pieces for {len(crossing)} openings)")
    above = [hole for hole in side_holes_here
             if (openings_by_portal.get(hole[4]) or {}).get("kind") == "window"
             and hole[2] >= skirting_top]
    require(all((hole[0], hole[1]) not in crossing for hole in above),
            "and a window whose sill is above the board does not interrupt it")

    kitchen_holes = [hole for side in ("-X", "+X", "-Z", "+Z")
                     for hole in holes_in(side, kitchen_box, subject,
                                          list(portal_rows.values()))]
    kinds = [(openings_by_portal.get(hole[4]) or {}).get("kind") for hole in kitchen_holes]
    doors_here = kinds.count("door")
    require(doors_here and kinds.count("window"),
            f"the kitchen has doors AND windows in its walls to tell apart ({kinds})")

    reset_scene()
    with_trim = build_cell(subject, extent, neighbours=neighbours, construction=construction,
                           level=levels[subject["level"]], levels=levels,
                           portals=list(portal_rows.values()), openings=openings_by_portal,
                           cells_by_id=cells)
    # Three boards and a threshold, six faces each, per door -- and nothing for a window, whose
    # frame is `HOUSE-00457`'s and has a different section.
    # A door gains four boxes -- two jambs, a head, a threshold. A window gains a four-board
    # frame, a four-board sash, a pane of glass and a sill board, plus a meeting rail if it is
    # double-hung. Six faces to a box. Counted as parts rather than as a magic number, so an
    # injected bug that drops one part is a claim about the part that went missing.
    window_holes = [hole for hole in kitchen_holes
                    if (openings_by_portal.get(hole[4]) or {}).get("kind") == "window"]
    boxes_expected = 4 * doors_here
    for hole in window_holes:
        row = openings_by_portal[hole[4]]
        boxes_expected += 1                                   # the sill board, in every room
        if window_owner(portal_cell_sides.get(hole[4]), cells, subject["id"]) == subject["id"]:
            boxes_expected += 4 + 4 + 1 + (1 if has_meeting_rail(row.get("type")) else 0)
    require(len(with_trim.data.polygons) == holes_faces + 6 * boxes_expected,
            f"the {doors_here} doorway(s) and {len(window_holes)} window(s) add "
            f"{boxes_expected} boxes between them ({holes_faces} -> "
            f"{len(with_trim.data.polygons)}, expected {holes_faces + 6 * boxes_expected})")

    rails = {row["type"] for row in openings_by_portal.values()
             if row.get("kind") == "window" and has_meeting_rail(row.get("type"))}
    plain = {row["type"] for row in openings_by_portal.values()
             if row.get("kind") == "window" and not has_meeting_rail(row.get("type"))}
    require(rails and plain and all(name.startswith("W_DH_") for name in rails),
            f"only §12.6's double-hung types get a meeting rail ({sorted(rails)}), and the "
            f"single lights do not ({sorted(plain)})")
    require(window_owner(("L0_KITCHEN", "EXT_BACKYARD"), cells, "EXT_BACKYARD") == "L0_KITCHEN",
            "a window onto the back lawn is built by the room, not by the lawn")
    require(window_owner(("L0_SUNROOM", "L0_KITCHEN"), cells, "L0_SUNROOM") == "L0_KITCHEN",
            "and a borrowed-light window between two rooms is built once, by the first of them")

    casings = {float((row.get("frame") or {}).get("casing") or 0.0)
               for row in openings_by_portal.values() if row.get("kind") == "door"}
    require(casings and 0.0 not in casings,
            f"every door declares a `frame.casing` ({sorted(casings)})")
    narrow = architrave_boards(0.0, 0.9, 0.0, 2.02, 0.06)
    wide = architrave_boards(0.0, 0.9, 0.0, 2.02, 0.12)
    require(abs((narrow[0][1] - narrow[0][0]) - 0.06) < 1e-9
            and abs((wide[0][1] - wide[0][0]) - 0.12) < 1e-9,
            f"and the architrave is as wide as the casing the opening declares, not as wide as "
            f"the one this house happens to use everywhere "
            f"({narrow[0][1] - narrow[0][0]:.3f}, {wide[0][1] - wide[0][0]:.3f})")
    require(not architrave_boards(0.0, 0.9, 0.0, 2.02, 0.0),
            "and an opening with no casing gets no architrave rather than three boards of nothing")

    # ...and the builder READS it. The claim above is about the boards; this one is about the path
    # from the opening row to them, which a hard-coded 0.06 satisfies just as well until a door
    # has a different casing. Doubling one and seeing the mesh move is the difference.
    import copy as _copy
    doubled = {portal_id: _copy.deepcopy(row) for portal_id, row in openings_by_portal.items()}
    for row in doubled.values():
        if row.get("kind") == "door" and (row.get("frame") or {}).get("casing"):
            row["frame"]["casing"] = float(row["frame"]["casing"]) * 2.0
    reset_scene()
    widened = build_cell(subject, extent, neighbours=neighbours, construction=construction,
                         level=levels[subject["level"]], levels=levels,
                         portals=list(portal_rows.values()), openings=doubled,
                         cells_by_id=cells)
    widened_points = sorted(tuple(round(value, 6) for value in vertex.co)
                            for vertex in widened.data.vertices)
    reset_scene()
    again = build_cell(subject, extent, neighbours=neighbours, construction=construction,
                       level=levels[subject["level"]], levels=levels,
                       portals=list(portal_rows.values()), openings=openings_by_portal,
                       cells_by_id=cells)
    same_points = sorted(tuple(round(value, 6) for value in vertex.co)
                         for vertex in again.data.vertices)
    require(widened_points != same_points,
            "and doubling a door's declared casing moves the mesh, so the builder is reading the "
            "opening rather than a constant that happens to match every door in this house")

    # ---- `HOUSE-00459`: the stairs ------------------------------------------------------------
    flight_rows = {row["id"]: row for row in layout_io.rows(
        layout_io.load_layout(SOURCE, kinds=["stairs"]), "stairs")}
    main = flight_rows["STAIR_MAIN_L0_L1"]
    steps, turn, landing_depth = flight_steps(main, 0.60)
    require(len(steps) == int(main["risers"]),
            f"a flight has one step per riser ({len(steps)} of {main['risers']})")
    require(abs(steps[-1][2] - float(levels["L1"]["ffl"])) < 1e-6,
            f"and its last tread IS the floor above, {steps[-1][2]:.4f} against "
            f"{levels['L1']['ffl']}")
    require(abs(steps[0][2] - (0.60 + float(main["rise"]))) < 1e-9,
            "and its first is one rise off the floor below")
    require(turn == 9 and abs(landing_depth - 1.1) < 1e-9,
            f"the main stair turns at riser 9 on a 1.10 m landing ({turn}, {landing_depth})")
    lanes = {lane for _a0, _a1, _top, lane in steps}
    require(lanes == {0, 1},
            f"and a U-stair has two runs, one each side of its own landing ({sorted(lanes)})")
    require({lane for _a0, _a1, _top, lane in flight_steps(
        flight_rows["STAIR_ATTIC_L2_L3"], 6.55)[0]} == {0},
        "while a straight flight has one")

    # The steps are inside the footprint the flight declares, which rule 10 checks the SIZE of and
    # nothing checked the placement of until here.
    boxes = []
    build_flight(main, lambda *args: boxes.append(args), 0.60)
    footprint = main["footprint"]
    # The bottom step's nosing overhangs the foot of the flight, into the room, by its own
    # projection -- which is what a nosing is. Everything else is inside the declared footprint.
    slack = NOSING_PROJECT + 1e-6
    inside = all(float(footprint["x"][0]) - 1e-6 <= bx0 and bx1 <= float(footprint["x"][1]) + 1e-6
                 and float(footprint["z"][0]) - slack <= bz0
                 and bz1 <= float(footprint["z"][1]) + slack
                 for bx0, bx1, _by0, _by1, bz0, bz1 in boxes)
    require(boxes and inside,
            f"every one of the {len(boxes)} boxes of the main stair is inside its footprint, but "
            f"for the bottom nosing's {NOSING_PROJECT * 1000:.0f} mm overhang")
    require(len(boxes) == 2 * int(main["risers"]) + 1,
            f"a step, a nosing over each, and the landing ({len(boxes)})")
    require(min(by0 for _a, _b, by0, _c, _d, _e in boxes) >= 0.60 - 1e-6
            and abs(max(by1 for _a, _b, _c, by1, _d, _e in boxes)
                    - float(levels["L1"]["ffl"])) < 1e-6,
            "and none of it is below the floor it starts on or above the floor it reaches")

    # ---- `HOUSE-00460`: the stairwell openings ------------------------------------------------
    #
    # A stairwell is a portal with a `y` plane. Until it was cut, the landing above every flight
    # had a floor across it and the stair arrived in a ceiling.
    upper = cells["L1_STAIR_MAIN"]
    upper_extent = extent_of(upper, levels[upper["level"]])[0]
    upper_box = list(cell_boxes(upper, upper_extent))[0]
    wells = slab_holes(list(portal_rows.values()), upper, upper_extent[0], upper_box)
    require(len(wells) == 1 and wells[0][4] == "P_STAIR_L0_L1",
            f"the cell over the main stair has one hole in its floor ({wells})")
    inner_upper = inset_box(upper_box, upper, neighbours, construction)
    pieces = panel(inner_upper[0], inner_upper[1], inner_upper[4], inner_upper[5], wells)
    covered = sum((a1 - a0) * (b1 - b0) for a0, a1, b0, b1 in pieces)
    hole = (min(wells[0][1], inner_upper[1]) - max(wells[0][0], inner_upper[0])) * \
           (min(wells[0][3], inner_upper[5]) - max(wells[0][2], inner_upper[4]))
    whole = (inner_upper[1] - inner_upper[0]) * (inner_upper[5] - inner_upper[4])
    require(abs(covered + hole - whole) < 1e-6 and hole > 1.0,
            f"and its floor is that room's floor less the {hole:.2f} m² you would fall through "
            f"({covered:.2f} + {hole:.2f} of {whole:.2f})")
    lower = cells["L0_STAIR_MAIN"]
    lower_extent = extent_of(lower, levels[lower["level"]])[0]
    lower_box = list(cell_boxes(lower, lower_extent))[0]
    require(len(slab_holes(list(portal_rows.values()), lower, lower_extent[1], lower_box)) == 1,
            "and the same hole is missing from the ceiling below it, which is the same hole")
    require(not slab_holes(list(portal_rows.values()), subject, extent[0],
                           list(cell_boxes(subject, extent))[0]),
            "while a room with no stair under it keeps its whole floor")

    # ...and the cell that carries the flight actually gets it. The claims above call
    # `build_flight` directly, which a builder that never called it would satisfy perfectly.
    stair_cell = cells[main["fromCell"]]
    stair_extent = extent_of(stair_cell, levels[stair_cell["level"]])[0]
    reset_scene()
    without = build_cell(stair_cell, stair_extent, neighbours=neighbours,
                         construction=construction, level=levels[stair_cell["level"]],
                         levels=levels, portals=list(portal_rows.values()),
                         openings=openings_by_portal, cells_by_id=cells)
    without_faces = len(without.data.polygons)
    reset_scene()
    with_stair = build_cell(stair_cell, stair_extent, neighbours=neighbours,
                            construction=construction, level=levels[stair_cell["level"]],
                            levels=levels, portals=list(portal_rows.values()),
                            openings=openings_by_portal, cells_by_id=cells,
                            flights=list(flight_rows.values()))
    # 35 boxes of stair, a handrail up the open side of each of the U's two runs with a newel at
    # each end (2 + 4), and one more railing piece round the hole in this cell's OWN floor: the
    # basement stair arrives through it, and telling the builder about the flights turns the one
    # rail along that edge into two with a gap between them. A railing across the top of a flight
    # is a railing you have to climb.
    require(len(with_stair.data.polygons) == without_faces + 6 * (len(boxes) + 2 + 4 + 1),
            f"{main['fromCell']} gains the flight's {len(boxes)} boxes, two handrails, four "
            f"newels, and a gap in the railing where the basement stair comes up "
            f"({without_faces} -> {len(with_stair.data.polygons)})")
    top = max(vertex.co.z for vertex in with_stair.data.vertices)
    handrail = float(levels["L1"]["ffl"]) + float(construction["balustrade"]) \
        + RAIL_SECTION / 2.0
    require(abs(top - handrail) < 1e-4,
            f"and the highest thing in it is the handrail over the top tread, {handrail:.3f} m -- "
            f"§12's balustrade height above the floor it arrives at ({top:.3f})")
    # The FOOT of the flight, which the cell's own walls do not give away: the stair well already
    # reaches `L1`'s floor whether or not a stair is in it, so the claim above cannot tell a
    # flight that starts on this floor from one that starts at the world origin.
    heights = {round(vertex.co.z, 6) for vertex in with_stair.data.vertices}
    require(round(stair_extent[0] + float(main["rise"]), 6) in heights,
            f"and its first tread is one rise above THIS floor, at "
            f"{stair_extent[0] + float(main['rise']):.4f} m")

    # A cell with no ceiling to be had. §13.6's attic level declares `ceiling: null` and every
    # attic cell overrides it, so this branch is unreachable from the authored house -- which is
    # exactly why it is claimed against a made-up cell instead of hoping one turns up.
    attic = levels["L3"]
    require(attic.get("ceiling") is None,
            "§13.6's attic level really does declare no ceiling")
    got, reason = extent_of({"id": "L3_MADE_UP", "level": "L3", "boxes": []}, attic)
    require(got is None and reason,
            f"a rafter-bounded cell with no yOverride is refused with a reason, not given a flat "
            f"ceiling ({got}, {reason!r})")
    require(extent_of(cells["L3_ROOM"], attic)[0] == (9.3, 12.6),
            f"...and one that overrides it gets its own height "
            f"({extent_of(cells['L3_ROOM'], attic)[0]})")

    # The whole house, which is the deliverable and not a sample.
    everything = generate(SOURCE, output)
    roofs = sorted(roof_boxes(layout_io.load_layout(SOURCE, kinds=["levels", "cells"]), levels))
    require(len(everything["written"]) == len(cells) + len(roofs) + 1
            and not everything["problems"] and not everything["skipped"],
            f"every one of the {len(cells)} cells generates, plus {len(roofs)} roof(s) and the "
            f"chimney ({len(everything['written'])} written, {len(everything['skipped'])} "
            f"skipped, {everything['problems'][:1]})")

    # ---- `HOUSE-00468`: the chimney ------------------------------------------------------------
    hearth = next(row for row in layout_io.rows(
        layout_io.load_layout(SOURCE, kinds=["interactables"]), "interactables")
        if "FIREPLACE" in str(row.get("id")))
    stack_box = chimney_at(hearth, construction, float(levels["L0"]["ffl"]))
    point = hearth["focus"]["point"]
    require(abs((stack_box[0] + stack_box[1]) / 2.0 - float(point[0])) < 1e-9
            and abs((stack_box[4] + stack_box[5]) / 2.0 - float(point[2])) < 1e-9,
            "the chimney stands over the fireplace, wherever the fireplace is — its plan position "
            "is `APPL_L0_LIVING_FIREPLACE`'s own and not a number written here")
    require(stack_box[3] > float(construction["ridgeY"]) + 0.5,
            f"and it clears §12's ridge by more than half a metre, which is what a stack has to "
            f"do ({stack_box[3]} against {construction['ridgeY']})")
    require(abs(stack_box[2] - float(levels["L0"]["ffl"])) < 1e-9,
            "and it starts at the hearth's own floor")
    require(chimney_at(None, construction, 0.6) is None
            and chimney_at({"focus": {}}, construction, 0.6) is None,
            "a house with no fireplace gets no chimney rather than one at the origin")

    # ---- `HOUSE-00461`: the roof ---------------------------------------------------------------
    main_box = roof_boxes(layout_io.load_layout(SOURCE, kinds=["levels", "cells"]),
                          levels)["ROOF_MAIN"]
    reach = float(construction["wallExterior"]) / 2.0 + EAVES_OVERHANG
    outer = (main_box[0] - reach, main_box[1] + reach, main_box[2] - reach, main_box[3] + reach)
    require(abs((outer[3] - outer[2]) - 13.4) < 1e-9,
            f"§12.1's roof is 13.4 m over the span, and the main block plus its overhang is "
            f"{outer[3] - outer[2]:.2f} m — the 0.30 m §12.1 does not account for is the eaves")
    eaves_y = eaves_height(construction, outer)
    ridge = eaves_y + ((outer[3] - outer[2]) / 2.0) * float(construction["roofPitch"])
    require(abs(ridge - float(construction["ridgeY"])) < 1e-9,
            f"the ridge comes out at §12's +{construction['ridgeY']} ({ridge:.4f})")
    knee = (eaves_y + reach * float(construction["roofPitch"])) - float(levels["L3"]["ffl"])
    require(abs(knee - 1.267) < 0.001 and knee > float(construction["kneeWallHeight"]),
            f"and the rafter line over the wall centre is {knee:.3f} m above the attic floor "
            f"against §12's kneeWallHeight {construction['kneeWallHeight']} — 67 mm the generous "
            f"way, so nothing is short of headroom, and the ridge and the pitch are what give way "
            f"if anything ever has to")

    planes = roof_planes(outer, eaves_y, float(construction["roofPitch"]))
    require(len(planes) == 4 and sum(1 for corners, _ in planes if len(corners) == 3) == 2,
            f"a hip roof over a rectangle is two trapezoids and two triangles "
            f"({[len(corners) for corners, _ in planes]})")
    square = roof_planes((0.0, 8.4, 0.0, 8.4), 0.0, 0.5)
    require(all(len(corners) == 3 for corners, _ in square),
            "and over a square it is four triangles, which is what a pyramid is")
    # ---- `HOUSE-00462`: the dormers -----------------------------------------------------------
    all_portals = list(portal_rows.values())
    dormer_list = dormers_on(main_box, all_portals, openings_by_portal.values())
    require(len(dormer_list) == 5,
            f"§12.1's five dormers are the five `W_DORMER` windows ({len(dormer_list)})")
    subject_dormer = dormer_list[0]
    dormer_out = 1.0 if abs(subject_dormer[2] - main_box[3]) < abs(subject_dormer[2] - main_box[2]) \
        else -1.0
    faces = dormer_shell(subject_dormer[0], subject_dormer[1], subject_dormer[2], dormer_out,
                         outer, eaves_y, float(construction["roofPitch"]))
    require(len(faces) == 5,
            f"a dormer is a gable face, two cheeks and two roof planes ({len(faces)})")
    heads = {round(point[1], 4) for corners, _ in faces for point in corners}
    window_head = subject_dormer[1][1]
    front_top = window_head + DORMER_HEAD
    require(max(heads) > front_top + 1e-6,
            f"its ridge is over the top of its own front wall, not level with it: {max(heads)} "
            f"against {front_top} — a dormer whose ridge is its head has no roof")

    # A gable louvre is not a dormer. It sits in a gable end rather than coming through the roof,
    # and this house's two are in `x` planes; the type test is what would keep a `z`-plane one out.
    synthetic_portal = {"id": "P_FAKE", "plane": {"axis": "z", "value": main_box[3]},
                        "rect": {"u": [0.0, 0.8], "v": [11.3, 12.1]}}
    require(not dormers_on(main_box, [synthetic_portal],
                           [{"type": "W_GABLE", "portal": "P_FAKE"}]),
            "a gable louvre in the same wall is not a dormer")
    require(len(dormers_on(main_box, [synthetic_portal],
                           [{"type": "W_DORMER", "portal": "P_FAKE"}])) == 1,
            "...and the same opening as a dormer is")
    roof_here = eaves_y + abs((outer[3] if dormer_out > 0 else outer[2]) - subject_dormer[2]) \
        * float(construction["roofPitch"])
    require(abs(min(heads) - roof_here) < 1e-3,
            f"and its foot is on the roof it comes through, at {roof_here:.3f} m")
    widths = {round(point[0], 4) for corners, _ in faces for point in corners}
    require(min(widths) < subject_dormer[0][0] and max(widths) > subject_dormer[0][1],
            "and its cheeks stand outside the window, not through it")

    reset_scene()
    plain_roof = build_roof("ROOF_PLAIN", main_box, construction)
    plain_roof_faces = len(plain_roof.data.polygons)
    reset_scene()
    dormered = build_roof("ROOF_MAIN", main_box, construction, dormers=dormer_list)
    require(len(dormered.data.polygons) == plain_roof_faces + 5 * len(dormer_list),
            f"and the roof gains five faces for each of the five ({plain_roof_faces} -> "
            f"{len(dormered.data.polygons)})")

    # ---- `HOUSE-00463`: the attic structure -----------------------------------------------------
    ridge_run = (outer[1] - outer[0]) - (outer[3] - outer[2])
    expected_rafters = 2 * (int(ridge_run / RAFTER_SPACING) + 1)
    require(len(dormered.data.polygons) == plain_roof_faces + 5 * len(dormer_list),
            "the roof's face count is the planes, the eaves boards and the structure")
    # planes, fascia, soffit, four gutters of four faces, four downspouts of two, one ridge vent
    bare = plain_roof_faces - (4 + 4 + 4 + 4 * 4 + 4 * 2 + 1)
    require(bare == expected_rafters + 2,
            f"a rafter every {RAFTER_SPACING * 1000:.0f} mm over the ridge's {ridge_run:.2f} m, "
            f"both slopes, and a purlin under each ({bare} against {expected_rafters + 2})")

    store = cells["L3_STORE_W"]
    require(store.get("kind") == "closet" and levels["L3"].get("ceiling") is None,
            "an attic store is a `closet` on a rafter-bounded level, which is the only thing in "
            "the data that tells it from the finished room")
    require(cells["L3_ROOM"].get("kind") == "room"
            and abs(float(cells["L3_ROOM"]["yOverride"][1]) - 12.60) < 1e-9,
            "and the finished room's ceiling IS §12.2's collar tie at +12.60, already laid by "
            "`HOUSE-00452` rather than built again here")
    reset_scene()
    boarded = build_cell(store, extent_of(store, levels["L3"])[0], neighbours=neighbours,
                         construction=construction, level=levels["L3"], levels=levels,
                         cells_by_id=cells)
    board_height = float(store["yOverride"][0]) + WALKWAY_THICK / 2.0
    boards = len([face for face in boarded.data.polygons
                  if abs(face.center.z - board_height) < 0.05])
    boarded_faces = len(boarded.data.polygons)
    reset_scene()
    plain_store_faces = len(build_cell(store, extent_of(store, levels["L3"])[0],
                                       neighbours=neighbours, construction=construction,
                                       level=dict(levels["L3"], ceiling=13.9), levels=levels,
                                       cells_by_id=cells).data.polygons)
    finished = cells["L3_ROOM"]
    reset_scene()
    room_rafters = build_cell(finished, extent_of(finished, levels["L3"])[0],
                              neighbours=neighbours, construction=construction,
                              level=levels["L3"], levels=levels, cells_by_id=cells)
    room_faces = len(room_rafters.data.polygons)
    reset_scene()
    room_flat_faces = len(build_cell(finished, extent_of(finished, levels["L3"])[0],
                                     neighbours=neighbours, construction=construction,
                                     level=dict(levels["L3"], ceiling=12.6), levels=levels,
                                     cells_by_id=cells).data.polygons)
    require(room_faces == room_flat_faces,
            f"the FINISHED attic room gets no walkway, being a room and not a store "
            f"({room_faces} against {room_flat_faces})")
    require(boards >= 4 and boarded_faces == plain_store_faces + 6,
            f"the store gets a walkway board along it, six faces of it ({boards} faces near the "
            f"floor, {plain_store_faces} -> {boarded_faces})")

    # ---- `HOUSE-00464`: the porch ---------------------------------------------------------------
    porch = cells["L0_PORCH"]
    porch_extent = extent_of(porch, levels[porch["level"]])[0]
    require(covered_by(porch, porch_extent, cells),
            "the porch is covered -- `L1_BALCONY_FRONT` sits on it -- which is what makes it a "
            "porch and not a terrace")
    require(not covered_by(cells["L1_BALCONY_REAR"],
                           extent_of(cells["L1_BALCONY_REAR"], levels["L1"])[0], cells),
            "and the rear balcony is not, so it gets no columns")
    require(not covered_by(cells["EXT_TERRACE"],
                           extent_of(cells["EXT_TERRACE"], levels["L0"])[0], cells),
            "nor is the terrace")

    # ---- `HOUSE-00465`: the balcony edges -------------------------------------------------------
    balcony = cells["L1_BALCONY_REAR"]
    balcony_extent = extent_of(balcony, levels["L1"])[0]
    require(balcony_extent[0] > 1.0 and porch_extent[0] < 1.0,
            f"the rear balcony is a storey up ({balcony_extent[0]}) and the porch deck is not "
            f"({porch_extent[0]}), which is §70.5's own threshold for a drop")
    open_sides = [side for side in ("-X", "+X", "-Z", "+Z")
                  if not any(wall == "wallPartition" for _lo, _hi, wall in side_intervals(
                      side, list(cell_boxes(balcony, balcony_extent))[0], balcony,
                      neighbours))]
    parapets, rail_faces = [], []
    built = build_balcony_edge(balcony, balcony_extent, neighbours, construction,
                               lambda *args: parapets.append(args),
                               lambda *args: rail_faces.append(args))
    require(built == 2 * len(open_sides) and len(parapets) == len(open_sides)
            and len(rail_faces) == 6 * len(open_sides),
            f"it gains a parapet and a railing on each of its {len(open_sides)} open sides "
            f"({len(parapets)} parapets, {len(rail_faces)} rail faces)")
    tops_of = {round(box[3], 4) for box in parapets}
    require(tops_of == {round(balcony_extent[0] + PARAPET_HEIGHT, 4)},
            f"the parapet stands {PARAPET_HEIGHT} m off the deck ({sorted(tops_of)})")
    rail_tops = {round(point[1], 4) for corners in rail_faces for point in corners[0]}
    require(max(tops_of) < balcony_extent[0] + float(construction["railing"]) - RAIL_SECTION,
            f"which is below the rail, so the rail sits ON the parapet rather than inside it "
            f"({max(tops_of)} against "
            f"{balcony_extent[0] + float(construction['railing']) - RAIL_SECTION:.3f})")
    require(max(rail_tops) > balcony_extent[0] + float(construction["railing"]),
            f"and the rail's top is over §12's railing height above it "
            f"({max(rail_tops)} against {balcony_extent[0] + float(construction['railing'])})")
    require(len(open_sides) == 3,
            f"three of them: the fourth is the house ({open_sides})")
    require(not build_balcony_edge(porch, porch_extent, neighbours, construction,
                                   lambda *args: None, lambda *args: None),
            "and the porch, 0.57 m up, gets none of it -- its balustrade is a different thing")

    # ---- `HOUSE-00466`: the balconies, and what outside does not have ---------------------------
    require(not slab_here(cells["EXT_WORLD"], (-5.0, 60.0), False)
            and not slab_here(cells["EXT_WORLD"], (-5.0, 60.0), True),
            "`EXT_WORLD` gets neither slab: it is the world, and it had a lid at +60 m over "
            "160 000 m² of it until this was asked")
    require(not slab_here(cells["EXT_FRONTYARD_W"], (-0.9, 20.0), True),
            "a lawn's ground is the terrain, not a slab")
    require(slab_here(porch, porch_extent, True) and not slab_here(porch, porch_extent, False),
            "the porch is a deck, so it has a floor and still no ceiling")
    for deck in ("L1_BALCONY_FRONT", "L2_BALCONY_JULIET", "EXT_TERRACE"):
        row = cells[deck]
        deck_extent = extent_of(row, levels[row["level"]])[0]
        require(slab_here(row, deck_extent, True) and not slab_here(row, deck_extent, False),
                f"and so is {deck}, at +{deck_extent[0]}")
    require(slab_here(subject, extent, False),
            "while a room keeps its ceiling, which is the thing you are standing under")

    juliet = cells["L2_BALCONY_JULIET"]
    juliet_extent = extent_of(juliet, levels["L2"])[0]
    juliet_edges = build_balcony_edge(juliet, juliet_extent, neighbours, construction,
                                      lambda *args: None, lambda *args: None)
    require(juliet_edges == 2 * 3,
            f"the juliet balcony gets its parapet and rail from the same rule as the rear one "
            f"({juliet_edges})")

    # ---- `HOUSE-00467`: the garage wing ---------------------------------------------------------
    loft = cells["L0_GARAGE_LOFT"]
    loft_extent = extent_of(loft, levels[loft["level"]])[0]
    guards = build_mezzanine_guard(loft, loft_extent, cells, levels, construction,
                                   lambda *args: None)
    require(guards == 4,
            f"the garage loft is a platform 2.75 m over the slab, so it gets a guard on all four "
            f"sides ({guards})")
    fridge = cells["CELL_FRIDGE_INTERIOR"]
    require(not build_mezzanine_guard(fridge, extent_of(fridge, levels["L0"])[0], cells, levels,
                                      construction, lambda *args: None),
            "and a refrigerator's interior, nested and 0.10 m up, does not — the same test, and "
            "§70.5's own metre is what separates them")
    require(not build_mezzanine_guard(subject, extent, cells, levels, construction,
                                      lambda *args: None),
            "nor does a room that is nested in nothing")

    garage = cells["L0_GARAGE"]
    garage_extent = extent_of(garage, levels[garage["level"]])[0]
    require(abs(garage_extent[0] - 0.15) < 1e-9 and abs(garage_extent[1] - 4.30) < 1e-9,
            f"the garage's slab is §12.2's +0.15 and its head +4.30, which are its own cell's "
            f"({garage_extent})")
    sectional = next(row for row in openings_by_portal.values()
                     if row["id"] == "DOOR_GARAGE_SECTIONAL")
    require(sectional.get("kind") == "door",
            "the sectional door is an `opening` of kind `door`, so `HOUSE-00455` cut it and "
            "`HOUSE-00456` lined it like any other")

    require(PORCH_COLUMNS == 4,
            f"§12.1 says the porch stands on FOUR square columns ({PORCH_COLUMNS})")

    flight_list = list(flight_rows.values())
    reset_scene()
    porch_faces = len(build_cell(porch, porch_extent, neighbours=neighbours,
                                 construction=construction, level=levels[porch["level"]],
                                 levels=levels, portals=all_portals,
                                 openings=openings_by_portal, cells_by_id=cells,
                                 flights=flight_list).data.polygons)
    # The same porch with the thing that covers it taken away -- and nothing else changed, so the
    # difference is the columns, the beams and the balustrade and not a side effect.
    uncovered = {key: value for key, value in cells.items() if key != "L1_BALCONY_FRONT"}
    reset_scene()
    bare_porch = len(build_cell(porch, porch_extent, neighbours=neighbours,
                                construction=construction, level=levels[porch["level"]],
                                levels=levels, portals=all_portals,
                                openings=openings_by_portal, cells_by_id=uncovered,
                                flights=flight_list).data.polygons)
    require(porch_faces == bare_porch + 6 * (PORCH_COLUMNS + 3) + 6 * 4,
            f"and it gains {PORCH_COLUMNS} columns, a beam over each of its three open sides and "
            f"a balustrade broken by the steps ({bare_porch} -> {porch_faces})")
    steps_here = [row for row in flight_list if row.get("toCell") == "L0_PORCH"]
    require(steps_here, "the porch steps arrive in it, so its balustrade has a gap for them")

    tops = {round(point[1], 6) for corners, _ in planes for point in corners}
    require(tops == {round(eaves_y, 6), round(float(construction["ridgeY"]), 6)},
            f"every corner of it is either at the eaves or at the ridge ({sorted(tops)})")
    sizes = sorted((output / f"{name}.glb").stat().st_size for name in everything["written"])
    require(all(size > 0 for size in sizes),
            f"and every file has bytes in it (smallest {sizes[0]})")

    if failures:
        print(f"house_shell_gen: {len(failures)} claim(s) FAILED")
        return 1
    print("house_shell_gen: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--source", type=Path, default=SOURCE)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--cells", default="", help="comma-separated cell ids; default is all")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(blender_env_argv())

    if args.selftest:
        return selftest(args.output / "selftest")
    if not (args.source / "layout.cells.json").is_file():
        print(f"house_shell_gen: no world in {args.source} yet -- nothing to generate.")
        return 0

    wanted = {name.strip() for name in args.cells.split(",") if name.strip()} or None
    report = generate(args.source, args.output, wanted)
    for problem in report["problems"]:
        print(f"house_shell_gen: {problem}", file=sys.stderr)
    for skipped in report["skipped"]:
        print(f"house_shell_gen: skipped {skipped}")
    print(f"house_shell_gen: {len(report['written'])} cell(s) -> "
          f"{args.output.relative_to(REPO) if args.output.is_relative_to(REPO) else args.output}")
    return 1 if report["problems"] else 0


def blender_env_argv() -> list[str]:
    return sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


if __name__ == "__main__":
    # Blender does not propagate a script's exit status; see tools/blender/blender_env.py.
    try:
        _status = main()
    except SystemExit as _exit:
        _status = 1 if isinstance(_exit.code, str) else int(_exit.code or 0)
    except BaseException:  # noqa: BLE001
        import traceback

        traceback.print_exc()
        print("house_shell_gen: EXIT 1")
        raise
    print(f"house_shell_gen: EXIT {_status}")
    sys.exit(_status)
