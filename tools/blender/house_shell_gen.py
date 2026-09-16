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
import json  # noqa: E402
import math  # noqa: E402
from pathlib import Path  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "world"))
sys.path.insert(0, str(REPO / "tools" / "assets"))
import gltf_validate  # noqa: E402
import layout_io  # noqa: E402
import roof_geometry  # noqa: E402
import stair_geometry  # noqa: E402
import terrain_gen  # noqa: E402

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
                   ) -> list[tuple[float, float, str, bool]]:
    """@p side split into `(lo, hi, wall, bounded)` runs: which wall bounds each stretch of it,
    and whether there is another CELL across that stretch.

    A side is rarely all one thing. `L0_KITCHEN`'s north side is 8.9 m of partition against the
    sunroom and 1.5 m of exterior wall beside it; seven of the house's 324 sides are mixed like
    that. A generator that took the first neighbour it found and applied that wall to the whole
    side would put 1.5 m of the kitchen floor inside the outside wall.

    An **interior** cell across the side makes it a partition; an exterior cell counts as nothing,
    because the back wall of the family room faces `EXT_BACKYARD`, which is a cell, and is still
    an exterior wall.

    **`covers` is not `wall == "wallPartition"`, and the difference is the garage**
    (`HOUSE-00485`). A side between the garage and a room is `wallGarage` from either side, so a
    caller asking "is there a room across this?" by looking at the wall's NAME is told no -- and
    then draws an outer skin into a room that is drawing its own inner face on the same plane.
    278.97 m² of this house was drawn twice that way, all of it on the garage's two long walls.

    **And the answer is a Y RANGE, not a yes.** The garage is one storey and the house beside it is
    three: over the garage's own 0.15--4.30 the room across the wall is the garage, and above its
    roof the same wall faces the weather. A run that answered only *bounded* would take the outer
    skin off the whole storey and leave a hole above the garage roof -- which is what the first
    attempt at `HOUSE-00485` did, and what `ext-east` caught. So each run carries the extents of
    the cells across it, and the caller subtracts them from the span it was going to draw.
    """
    plane, lo, hi = side_span(side, box)
    _x0, _x1, y0, y1, _z0, _z1 = box
    covered: list[tuple[float, float, str, float, float]] = []
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
        covered.append((start, end, wall, oy0, oy1))

    # A SWEEP over every boundary, rather than the first-covered-wins pass this replaced: two cells
    # can cover the same stretch of one side at different heights -- the garage over the mudroom
    # and the basement plant room under it -- and a run has to carry both of their extents or the
    # caller cannot subtract what it cannot see.
    outside = "wallGarage" if cell.get("kind") == "garage" else "wallExterior"
    edges = sorted({lo, hi} | {edge for start, end, *_ in covered for edge in (start, end)
                               if lo - 1e-9 <= edge <= hi + 1e-9})
    out: list[tuple[float, float, str, tuple]] = []
    for a, b in zip(edges, edges[1:]):
        if b - a <= 1e-6:
            continue
        here = [row for row in covered if row[0] <= a + 1e-6 and row[1] >= b - 1e-6]
        if not here:
            out.append((a, b, outside, ()))
            continue
        wall = "wallGarage" if any(row[2] == "wallGarage" for row in here) else "wallPartition"
        out.append((a, b, wall, tuple((row[3], row[4]) for row in here)))
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
    walls = {wall for _lo, _hi, wall, _covers in side_intervals(side, box, cell, neighbours)}
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


#: A gap between two cells stacked on the far side of a wall that is FLOOR STRUCTURE rather than
#: weather, in metres (`HOUSE-00485`). §13's storeys are 2.90-3.05 apart and the deepest floor band
#: in this house is 0.45; a gap wider than this is a room's worth of air, and the wall beside it --
#: the storey of house that stands over the garage's roof -- needs its outer skin.
INTERIOR_GAP = 0.75


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


def enclosed_by_parent(cell: dict, box: tuple, cells_by_id: dict, levels: dict) -> bool:
    """A child wholly inside its parent has no weather-facing outer wall.

    Side adjacency alone cannot find a parent whose larger footprint surrounds a fridge,
    freezer or mezzanine cell. In that case the normal exterior-skin extension to the next
    storey's floor would draw a wall *inside* the parent, above the child's own ceiling.
    Check the actual authored extents rather than treating every `parent` link as enclosure.
    """
    parent = cells_by_id.get(cell.get("parent"))
    parent_level = levels.get(parent.get("level")) if parent else None
    if parent_level is None:
        return False
    parent_extent, _ = extent_of(parent, parent_level)
    if parent_extent is None:
        return False
    x0, x1, y0, y1, z0, z1 = box
    return any(px0 <= x0 + 1e-6 and px1 >= x1 - 1e-6 and
               py0 <= y0 + 1e-6 and py1 >= y1 - 1e-6 and
               pz0 <= z0 + 1e-6 and pz1 >= z1 - 1e-6
               for px0, px1, py0, py1, pz0, pz1 in cell_boxes(parent, parent_extent))


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


def under_roof(u0: float, u1: float, v0: float, v1: float, cuts):
    """One wall panel as `(us, vs)`, clipped under the roof -- or nothing, if it is all over it.

    Returns the rectangle unchanged when there is no roof to be under, so a level bounded by a
    ceiling plane pays nothing for this (`HOUSE-00496`).
    """
    rectangle = [(u0, v0), (u0, v1), (u1, v1), (u1, v0)]
    if not cuts:
        return [tuple(zip(*rectangle))]
    clipped = clip_half_planes(rectangle, cuts)
    if len(clipped) < 3:
        return []
    return [tuple(zip(*clipped))]


def clip_half_planes(polygon, cuts):
    """@p polygon in 2-D, clipped to `nu*u + nv*v <= k` for every `(nu, nv, k)` in @p cuts.

    `HOUSE-00496`. The roof surface is the lower envelope of its planes
    (`roof_geometry.roof_height`), so "under the roof" -- and, in plan, "where the roof is over
    this height" -- are both intersections of half-planes, and a convex polygon clipped by one
    stays convex and stays one polygon. Sutherland-Hodgman, one half-plane at a time; an empty
    list means nothing of the panel is on the keeping side.
    """
    out = list(polygon)
    for nu, nv, k in cuts:
        if not out:
            return []
        clipped = []
        for index, (u, v) in enumerate(out):
            pu, pv = out[index - 1]
            here = nu * u + nv * v - k
            there = nu * pu + nv * pv - k
            inside = here <= 1e-9
            was = there <= 1e-9
            if inside != was and abs(here - there) > 1e-12:
                t = there / (there - here)
                clipped.append((pu + t * (u - pu), pv + t * (v - pv)))
            if inside:
                clipped.append((u, v))
        out = clipped
    return [point for index, point in enumerate(out)
            if index == 0 or max(abs(a - b) for a, b in zip(point, out[index - 1])) > 1e-9]


def roof_lines(equations, side: str, plane: float):
    """Each roof plane as a `(nu, nv, k)` half-plane in a WALL's own `(u, v)` coordinates.

    A wall on a `+/-X` side has `u = z` and `v = y`, so `y <= a*x + b*z + c` becomes
    `-b*u + v <= a*plane + c`; on a `+/-Z` side `u = x` and the roles swap.
    """
    cuts = []
    for a, b, c in equations or ():
        if side in ("-X", "+X"):
            cuts.append((-b, 1.0, a * plane + c))
        else:
            cuts.append((-a, 1.0, b * plane + c))
    return cuts


def roof_over(equations, height: float):
    """Each roof plane as a `(nu, nv, k)` half-plane in PLAN, keeping where the roof is above
    @p height: `a*x + b*z + c >= height`, which is `-a*x - b*z <= c - height`.

    A ceiling slab at §13.6's maximum head-room is only a lid where the roof is over it, and over
    the rest of the attic the rafters are (`HOUSE-00496`). Left whole, `L3_STORE_W`'s +13.90 slab
    stands through a roof that is +10.57 at its own wall.
    """
    return [(-a, -b, c - height) for a, b, c in equations or ()]


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

#: `HOUSE-00933`: the front elevation's authored `six_over_six` grille and louvered shutter
#: sections. Opening rows decide WHERE they exist; these are joinery dimensions, kept beside the
#: frame/sash dimensions for the same reason. A 25 mm muntin is visibly lighter than the 42 mm
#: sash. Each decorative 420 mm shutter has 55 mm stiles, 70 mm rails and recessed 22 mm slats.
MUNTIN_SECTION = 0.025
SHUTTER_WIDTH = 0.420
SHUTTER_GAP = 0.065
SHUTTER_DEPTH = 0.040
SHUTTER_STILE = 0.055
SHUTTER_RAIL = 0.070
SHUTTER_SLAT = 0.022
SHUTTER_SLAT_PITCH = 0.070


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


def flight_steps(flight: dict, bottom: float, portals=()):
    """Every tread of a flight, from `stair_geometry` -- the module the COLLISION is built from.

    `HOUSE-00472`. This used to work the placement out here, and got a `u` wrong: it laid the
    second run beyond the landing and climbed back towards it, which puts the flight's TOP tread
    against the half-landing. You would have stepped off `STAIR_MAIN_L0_L1`'s half-landing at
    +2.2147 and arrived on L1 at +3.65 in one stride. §12 calls the flight `U, half-landing at
    riser 9`: you turn through 180 degrees and climb back beside the way you came, which is why
    its footprint is 2.70 m across for a flight 1.10 m wide.

    Each tread is a dict with a world `box`, the `y1` you stand on, its `lane`, its `run`, the
    `axis` it travels along and the `up` it rises towards. Returns None for a flight that authors
    no `footprint`/`run`, which is a flight this generator cannot draw.
    """
    return stair_geometry.flight_steps(flight, bottom, portals)


#: §12's trim, in metres. `casing` and `skirting` are `layout.levels.json`'s; the two below are
#: not in the data because §12 never gives them a number, and a board that stands proud of the
#: wall by nothing z-fights with it.
ARCHITRAVE_PROUD = 0.018
#: A skirting and a cornice stand as proud of the wall as an architrave does, and mitre against
#: each other at a corner by exactly that much.
TRIM_PROUD = 0.018
THRESHOLD_THICK = 0.015

#: `D_ENTRY` is the one close-range exterior leaf in the canonical route. Its source row gives
#: the overall leaf but §19.4 deliberately leaves generated doors' joinery to this tool. These
#: are measured millwork/hardware sections, not fractions disguised as magic geometry: a 32 mm
#: raised moulding standing 16 mm proud, a 52 x 270 mm escutcheon, a 153 mm lever and a separate
#: 65 mm deadbolt. The panel bounds themselves scale with the leaf so the two authored entry
#: leaves share one grammar even if their schedule changes later.
ENTRY_PANEL_MOULDING = 0.032
ENTRY_PANEL_RELIEF = 0.016
ENTRY_BACKPLATE_WIDTH = 0.052
ENTRY_BACKPLATE_HEIGHT = 0.270
ENTRY_LEVER_LENGTH = 0.153
ENTRY_LEVER_HEIGHT = 0.022
ENTRY_HARDWARE_PROJECTION = 0.055
ENTRY_DEADBOLT_SIZE = 0.065
ENTRY_THRESHOLD_CAP = 0.009


def entry_door_detail_boxes(lu0: float, lu1: float, lv0: float, lv1: float,
                            depth_lo: float, depth_hi: float, hinge: str):
    """Raised four-panel millwork and hardware boxes in the leaf's local wall axes.

    Each tuple is ``(u0, u1, v0, v1, d0, d1, surface_class, part)``. Both faces are complete:
    the entry can be inspected from the porch or foyer without a two-sided material exception.
    The lock stile mirrors with the authored hinge rather than assuming the canonical left hinge.
    """
    width = lu1 - lu0
    height = lv1 - lv0
    if width <= 0.0 or height <= 0.0 or depth_hi <= depth_lo:
        return []

    # Four restrained raised-panel outlines. The wider lock stile is on the side opposite the
    # hinge; mirroring the whole layout keeps the two panel columns optically balanced with it.
    columns = [(0.10, 0.40), (0.48, 0.77)]
    if str(hinge or "left").lower() == "right":
        columns = sorted((1.0 - hi, 1.0 - lo) for lo, hi in columns)
    rows = [(0.067, 0.343), (0.400, 0.924)]
    moulding = min(ENTRY_PANEL_MOULDING, width * 0.04, height * 0.02)
    boxes = []

    def both_faces(u0, u1, v0, v1, klass, part, projection):
        boxes.append((u0, u1, v0, v1, depth_lo - projection, depth_lo, klass, part))
        boxes.append((u0, u1, v0, v1, depth_hi, depth_hi + projection, klass, part))

    for column_lo, column_hi in columns:
        for row_lo, row_hi in rows:
            panel_u0, panel_u1 = lu0 + column_lo * width, lu0 + column_hi * width
            panel_v0, panel_v1 = lv0 + row_lo * height, lv0 + row_hi * height
            both_faces(panel_u0, panel_u1, panel_v0, panel_v0 + moulding,
                       "exterior_door_panel", "panel_moulding", ENTRY_PANEL_RELIEF)
            both_faces(panel_u0, panel_u1, panel_v1 - moulding, panel_v1,
                       "exterior_door_panel", "panel_moulding", ENTRY_PANEL_RELIEF)
            both_faces(panel_u0, panel_u0 + moulding, panel_v0 + moulding,
                       panel_v1 - moulding, "exterior_door_panel", "panel_moulding",
                       ENTRY_PANEL_RELIEF)
            both_faces(panel_u1 - moulding, panel_u1, panel_v0 + moulding,
                       panel_v1 - moulding, "exterior_door_panel", "panel_moulding",
                       ENTRY_PANEL_RELIEF)

    lock_right = str(hinge or "left").lower() != "right"
    handle_u = lu0 + (0.865 if lock_right else 0.135) * width
    handle_v = lv0 + min(0.98, height * 0.47)
    plate_u0, plate_u1 = (handle_u - ENTRY_BACKPLATE_WIDTH / 2.0,
                           handle_u + ENTRY_BACKPLATE_WIDTH / 2.0)
    plate_v0, plate_v1 = (handle_v - ENTRY_BACKPLATE_HEIGHT / 2.0,
                           handle_v + ENTRY_BACKPLATE_HEIGHT / 2.0)
    both_faces(plate_u0, plate_u1, plate_v0, plate_v1,
               "exterior_door_hardware", "backplate", ENTRY_PANEL_RELIEF + 0.008)

    if lock_right:
        lever_u0, lever_u1 = handle_u - ENTRY_LEVER_LENGTH + 0.018, handle_u + 0.018
    else:
        lever_u0, lever_u1 = handle_u - 0.018, handle_u + ENTRY_LEVER_LENGTH - 0.018
    both_faces(lever_u0, lever_u1, handle_v - ENTRY_LEVER_HEIGHT / 2.0,
               handle_v + ENTRY_LEVER_HEIGHT / 2.0, "exterior_door_hardware", "lever",
               ENTRY_HARDWARE_PROJECTION)

    deadbolt_v = lv0 + min(1.35, height * 0.64)
    both_faces(handle_u - ENTRY_DEADBOLT_SIZE / 2.0,
               handle_u + ENTRY_DEADBOLT_SIZE / 2.0,
               deadbolt_v - ENTRY_DEADBOLT_SIZE / 2.0,
               deadbolt_v + ENTRY_DEADBOLT_SIZE / 2.0,
               "exterior_door_hardware", "deadbolt", ENTRY_PANEL_RELIEF + 0.008)
    return boxes


def build_flight(flight: dict, solid, bottom: float, *, add=None, construction=None,
                 inner=None, portals=()) -> None:
    """A flight's steps, nosings, landing, handrails and newels, as boxes, through @p solid.

    Solid steps rather than treads on a carriage: a blockout wants the shape you walk on and the
    volume you cannot walk through, and a closed string is both. The nosing is a separate board
    because it overhangs, which is the one part of a step's profile you see from below.

    Where every one of those boxes goes comes from `stair_geometry` (`HOUSE-00472`), so the shell
    and `build_collision.py` cannot disagree about it again.
    """
    placed = stair_geometry.flight_runs(flight, bottom, portals)
    treads = stair_geometry.flight_steps(flight, bottom, portals)
    if placed is None or treads is None:
        return
    axis = placed[0]["axis"]
    along_axis_x = axis == "x"
    rise = float(flight["rise"])

    def emit(box, low, high):
        solid(box[0], box[1], low, high, box[2], box[3])

    def along_of(box):
        """`(low, high)` on the axis the flight travels, whichever that is."""
        return (box[0], box[1]) if along_axis_x else (box[2], box[3])

    def with_along(box, low, high):
        return (low, high, box[2], box[3]) if along_axis_x else (box[0], box[1], low, high)

    for tread in treads:
        emit(tread["box"], bottom, tread["y1"])
        # The nosing overhangs the riser below it: it projects from the tread's FRONT edge, which
        # is the end nearer the foot of the run, and a `u`'s second run faces the other way.
        low, high = along_of(tread["box"])
        front = low if tread["up"] > 0 else high
        edge = sorted((front - tread["up"] * NOSING_PROJECT, front))
        emit(with_along(tread["box"], edge[0], edge[1]),
             tread["y1"] - NOSING_THICK, tread["y1"])

    # `HOUSE-00460`: a handrail up every side of a run that is not against a wall, and a newel at
    # each end of it. "Against a wall" is the run's across edge lying on the CELL's own boundary --
    # both are centre-line numbers, which is why they can be compared at all. A `u`'s two inner
    # edges face the well and never do, so a U-stair gets one rail up each run and the outer sides
    # get none.
    if add is not None and construction:
        height = float(construction.get("balustrade", 0.0))
        # "Against a wall" is the run's across edge lying on whatever BOUNDS the flight. Since
        # `HOUSE-00480` that is the stairwell the flight comes up -- `frame`'s cross range -- and
        # not the cell's box, whose edges are wall centre lines and which the flight no longer
        # touches. Comparing against the box gave the main stair a handrail on both sides of both
        # runs the moment it moved 0.20 m off it, which is a rail up the wall.
        wall_edges = set()
        fields = stair_geometry.frame(flight, portals)
        if fields is not None:
            wall_edges.add(round(fields[3], 4))
            wall_edges.add(round(fields[4], 4))
        if inner is not None:
            for value in ((inner[4], inner[5]) if along_axis_x else (inner[0], inner[1])):
                wall_edges.add(round(value, 4))
        for number in sorted({tread["run"] for tread in treads}):
            run_treads = [tread for tread in treads if tread["run"] == number]
            first, last = run_treads[0], run_treads[-1]
            up = first["up"]
            a_start = along_of(first["box"])[0 if up > 0 else 1]
            a_end = along_of(last["box"])[1 if up > 0 else 0]
            lane_lo, lane_hi = ((first["box"][2], first["box"][3]) if along_axis_x
                                else (first["box"][0], first["box"][1]))
            for edge in (lane_lo, lane_hi):
                if round(edge, 4) in wall_edges:
                    continue
                rail_along(add, along_axis_x, a_start, a_end,
                           first["y1"] + height, last["y1"] + height, edge, RAIL_SECTION)
                for a_at, y_at in ((a_start, first["y1"]), (a_end, last["y1"])):
                    post_lo, post_hi = sorted((a_at, a_at + up * NEWEL_SECTION))
                    if along_axis_x:
                        solid(post_lo, post_hi, y_at - rise, y_at + height,
                              edge - NEWEL_SECTION / 2.0, edge + NEWEL_SECTION / 2.0)
                    else:
                        solid(edge - NEWEL_SECTION / 2.0, edge + NEWEL_SECTION / 2.0,
                              y_at - rise, y_at + height, post_lo, post_hi)

    for entry in placed:
        if entry["kind"] == "landing":
            emit(entry["box"], bottom, entry["y0"])


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


def top_tread_box(flight: dict, bottom: float, portals=()):
    """The world box of a flight's top step, or None. Where you step off it onto the floor above."""
    treads = stair_geometry.flight_steps(flight, bottom, portals)
    if not treads:
        return None
    box, top = treads[-1]["box"], treads[-1]["y1"]
    return (box[0], box[1], box[2], box[3], top)


#: The roof's overhang, its eaves height and its four planes are `tools/world/roof_geometry.py`'s,
#: so that `build_collision.py` can build the attic's rafter envelope from the numbers this
#: generator draws it with (`HOUSE-00472`). §12 over-determines the roof and `HOUSE-00461` settled
#: it once; nothing here may settle it a second time. The fascia's own section is this generator's,
#: because §12 gives the board no dimension at all.
EAVES_OVERHANG = roof_geometry.EAVES_OVERHANG
eaves_height = roof_geometry.eaves_height
roof_planes = roof_geometry.roof_planes
#: ...and so is the dormer, since `HOUSE-00490`: it comes THROUGH the roof, so the shape and the
#: hole it leaves have to be one answer, and `build_collision.py` needs both.
DORMER_CHEEK = roof_geometry.DORMER_CHEEK
DORMER_HEAD = roof_geometry.DORMER_HEAD
dormer_shell = roof_geometry.dormer_shell
dormers_on = roof_geometry.dormers_on
FASCIA_DEPTH = 0.20
FASCIA_THICK = 0.035


#: `HOUSE-00470`'s diagnostic colours remain useful when a material definition cannot be supplied
#: by a small unit fixture. Production generation (`HOUSE-00907`) replaces every `BLOCKOUT_*`
#: name with an authored material id and uses that row's tint and alpha.
SURFACE_COLOURS = {
    "floor":     (0.62, 0.51, 0.38, 1.0),
    "ceiling":   (0.92, 0.92, 0.90, 1.0),
    "wall":      (0.80, 0.78, 0.74, 1.0),
    "exterior":  (0.72, 0.70, 0.64, 1.0),
    "exterior_door": (0.55, 0.42, 0.30, 1.0),
    "exterior_door_panel": (0.44, 0.28, 0.17, 1.0),
    "exterior_door_hardware": (0.55, 0.34, 0.14, 1.0),
    "trim":      (0.96, 0.96, 0.94, 1.0),
    "glass":     (0.55, 0.72, 0.80, 0.35),
    "window_frame": (0.96, 0.94, 0.90, 1.0),
    "window_shutter": (0.11, 0.12, 0.14, 1.0),
    "window_glass": (0.55, 0.72, 0.80, 0.35),
    "stair":     (0.55, 0.42, 0.30, 1.0),
    "roof":      (0.32, 0.30, 0.30, 1.0),
    "structure": (0.68, 0.58, 0.44, 1.0),
    "metal":     (0.45, 0.46, 0.48, 1.0),
}
SURFACE_ORDER = list(SURFACE_COLOURS)

#: Real finishes for shell classes which are not one of a cell palette's four fields. The
#: assignments are semantic defaults; stairs and glazing override them from their authored rows.
SHELL_MATERIALS = {
    "floor": "MAT_CONCRETE_BROOM",
    "ceiling": "MAT_SOFFIT_WHITE",
    "wall": "MAT_SIDING_WARM_WHITE",
    "exterior": "MAT_SIDING_WARM_WHITE",
    "exterior_door": "MAT_EXTERIOR_DOOR_HARDWOOD",
    "exterior_door_panel": "MAT_EXTERIOR_DOOR_PANEL_HARDWOOD",
    "exterior_door_hardware": "MAT_EXTERIOR_DOOR_HARDWARE_BRONZE",
    "trim": "MAT_DOOR_PAINTED",
    "glass": "MAT_GLASS_CLEAR",
    "window_frame": "MAT_WINDOW_FRAME_WHITE",
    "window_shutter": "MAT_WINDOW_SHUTTER_BLACK",
    "window_glass": "MAT_WINDOW_GLASS_CLEAR",
    "stair": "MAT_DOOR_HARDWOOD",
    "roof": "MAT_ROOF_SHINGLE",
    "structure": "MAT_HATCH_PLY",
    "metal": "MAT_METAL_BALCONY",
}

STAIR_MATERIALS = {
    "stair_wood": "MAT_DOOR_HARDWOOD",
    "stair_wood_open": "MAT_HATCH_PLY",
    "concrete": "MAT_CONCRETE_BROOM",
    "bluestone": "MAT_BLUESTONE_PAVER",
}

ROOF_MATERIALS = dict(SHELL_MATERIALS,
                      trim="MAT_SOFFIT_WHITE", metal="MAT_METAL_GUTTER")

#: The surface classes that receive a baked lightmap (`HOUSE-00471`, `cna-house.md` §18.3).
#:
#: **Semantic, not geometric.** A wall is a lightmap receiver because it is a wall -- a large,
#: static architectural surface whose illumination is low-frequency and worth baking -- and not
#: because any particular triangle of it happens to be bigger than a texel. Triangulating a wall
#: differently must not change whether it is lit by a bake, which is why this is a set of classes
#: and not a size threshold.
#:
#: Everything else is architectural DETAIL -- skirtings, cornices, architraves, thresholds, window
#: frames and sashes, glass, stair nosings, handrails, balusters, rafters, gutters, downspouts --
#: and is lit by the room's dynamic term instead. §18.3 records the measurement that settled it:
#: 26 704 of the shell's 43 528 faces were under one texel at 4 texels/metre when it was taken
#: (`HOUSE-00475` removed the yard walls; the shell is 33 486 triangles now), and a 55 mm
#: handrail
#: face is a fifth of a texel across, so no atlas anyone can budget would light them from a bake.
LIGHTMAP_RECEIVERS = ("floor", "ceiling", "wall", "exterior")


def is_exterior_door_material(material_id) -> bool:
    """Whether an authored leaf material is the isolated weather-facing door role."""
    return isinstance(material_id, str) and material_id.startswith("MAT_EXTERIOR_DOOR_")


def planar_uvs(mesh) -> None:
    """A world-space planar UV0, one unit per metre, projected on each face's dominant axis.

    The shell had **no UV layer at all** until `HOUSE-00471` went to add the second one and found
    there was no first: §21.3's `DualTextureEffect` samples the albedo channel first, and a shell
    with no UV0 cannot take a material at all, placeholder or real.

    Planar and world-space rather than unwrapped, because that is what an architectural surface
    wants: a 1 m tile is 1 m everywhere, so the boards on a floor are the same size in the kitchen
    as in the attic, and no seam moves when a room is resized. The second channel -- the packed,
    density-uniform one the lightmaps bake into -- is `lightmap_unwrap.py`'s and is a different
    thing for a different reason.
    """
    layer = mesh.uv_layers.new(name="UVMap")
    for polygon in mesh.polygons:
        normal = polygon.normal
        axis = max(range(3), key=lambda index: abs(normal[index]))
        for loop_index in polygon.loop_indices:
            point = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            if axis == 0:
                layer.data[loop_index].uv = (point.y, point.z)
            elif axis == 1:
                layer.data[loop_index].uv = (point.x, point.z)
            else:
                layer.data[loop_index].uv = (point.x, point.y)


def cell_surface_materials(cell: dict, openings=(), portals=(), flights=(),
                           cells_by_id=None) -> dict[str, str]:
    """Resolve the semantic shell classes to authored material ids for one cell.

    Floor, wall, ceiling and trim are the `HOUSE-00908` room palette. A window already names its
    clear or obscured glass in `layout.openings.json`, and a stair already names its construction
    surface in `layout.stairs.json`; using either from a room-name heuristic would make those data
    ornamental. Exterior cells have only a floor palette because they have no generated walls.
    """
    result = dict(SHELL_MATERIALS)
    for klass, field in (("floor", "floorMaterial"), ("wall", "wallMaterial"),
                         ("ceiling", "ceilingMaterial"), ("trim", "trimMaterial")):
        if cell.get(field):
            result[klass] = cell[field]

    # The basement shell is the brick water table under the painted siding. Nested appliance
    # cells are inside the house, so their outward faces retain their own wall finish rather than
    # pretending a refrigerator is clad in siding.
    if cell.get("parent") and cell.get("wallMaterial"):
        result["exterior"] = cell["wallMaterial"]
    elif cell.get("level") == "B1":
        result["exterior"] = "MAT_BRICK_WATER_TABLE"

    portal_cells = {row["id"]: (row.get("cellA"), row.get("cellB")) for row in portals}
    glass = {
        row.get("material")
        for row in openings
        if row.get("kind") == "window" and cell.get("id") in portal_cells.get(row.get("portal"), ())
        and window_owner(portal_cells.get(row.get("portal")), cells_by_id or {}, cell.get("id"))
        == cell.get("id")
        and row.get("material")
    }
    if len(glass) > 1:
        raise ValueError(
            f"{cell.get('id')}: generated glass is one surface class but its windows name "
            f"multiple materials: {', '.join(sorted(glass))}")
    if glass:
        source_glass = next(iter(glass))
        result["glass"] = source_glass
        result["window_glass"] = {
            "MAT_GLASS_CLEAR": "MAT_WINDOW_GLASS_CLEAR",
            "MAT_GLASS_OBSCURED": "MAT_WINDOW_GLASS_OBSCURED",
        }.get(source_glass, source_glass)

    shutters = {
        row.get("shutterMaterial")
        for row in openings
        if row.get("kind") == "window" and cell.get("id") in portal_cells.get(row.get("portal"), ())
        and window_owner(portal_cells.get(row.get("portal")), cells_by_id or {}, cell.get("id"))
        == cell.get("id")
        and row.get("shutterMaterial")
    }
    if len(shutters) > 1:
        raise ValueError(
            f"{cell.get('id')}: generated shutters are one surface class but name "
            f"multiple materials: {', '.join(sorted(shutters))}")
    if shutters:
        result["window_shutter"] = next(iter(shutters))

    exterior_doors = {
        row.get("material")
        for row in openings
        if row.get("kind") == "door"
        and cell.get("id") in portal_cells.get(row.get("portal"), ())
        and is_exterior_door_material(row.get("material"))
    }
    if len(exterior_doors) > 1:
        raise ValueError(
            f"{cell.get('id')}: generated exterior doors are one surface class but name "
            f"multiple materials: {', '.join(sorted(exterior_doors))}")
    if exterior_doors:
        result["exterior_door"] = next(iter(exterior_doors))

    stair_surfaces = {
        row.get("surface") for row in flights if row.get("fromCell") == cell.get("id")
    }
    unknown = sorted(surface for surface in stair_surfaces if surface not in STAIR_MATERIALS)
    if unknown:
        raise ValueError(f"{cell.get('id')}: no shell material for stair surface {unknown}")
    stair_materials = {STAIR_MATERIALS[surface] for surface in stair_surfaces}
    if len(stair_materials) > 1:
        raise ValueError(
            f"{cell.get('id')}: generated stairs are one surface class but resolve to multiple "
            f"materials: {', '.join(sorted(stair_materials))}")
    if stair_materials:
        result["stair"] = next(iter(stair_materials))
    return result


def material_slots(mesh, assignments: dict[str, str], definitions: dict[str, dict] | None = None) -> None:
    """Give @p mesh one authored material per surface class, in `SURFACE_ORDER`.

    A slot name is diagnostic (`<material-id>__shell_<surface-class>`); `materialId` is the
    machine-readable identity. Keeping both fields matters when two classes use the same finish:
    the terrace's bluestone floor receives a lightmap while its bluestone step remains detail, so
    merging both into one Blender material would erase the receiver decision before export.
    """
    definitions = definitions or {}
    for name in SURFACE_ORDER:
        material_id = assignments.get(name)
        if not material_id:
            raise ValueError(f"shell surface {name!r} resolves to no authored material")
        definition = definitions.get(material_id)
        if definitions and definition is None:
            raise ValueError(f"shell surface {name!r} names unknown material {material_id!r}")
        material = bpy.data.materials.new(f"{material_id}__shell_{name}")
        material.use_nodes = False
        tint = tuple(float(value) for value in (definition or {}).get("tint", SURFACE_COLOURS[name][:3]))
        alpha = float((definition or {}).get("alpha", SURFACE_COLOURS[name][3]))
        material.diffuse_color = (*tint, alpha)
        material["materialId"] = material_id
        material["surfaceClass"] = name
        material["lightmapReceiver"] = name in LIGHTMAP_RECEIVERS
        mesh.materials.append(material)


def weld(mesh) -> int:
    """Merge coincident vertices **of the lightmap receivers**. Returns how many were removed.

    Every face is built with its own eight corners, so before this a wall split into three strips
    round a doorway is three islands that share no vertex -- and `smart_project` would give each
    strip an island of its own, which is how a receiver surface ends up with sub-texel islands for
    reasons that have nothing to do with the wall. Welding makes the logical surface **connected**,
    so the unwrapper sees one wall and packs one island for it (`HOUSE-00471`).

    Only the receivers, because only they are unwrapped: a skirting board is not lightmapped and
    welding it would change nothing but the face count. The threshold is 10 µm -- coincident means
    coincident -- so two boards 18 mm apart do not merge and nor do the two faces of a 6 mm pane.
    """
    import bmesh  # noqa: PLC0415  (only available inside Blender)

    receivers = {SURFACE_ORDER.index(name) for name in LIGHTMAP_RECEIVERS}
    working = bmesh.new()
    working.from_mesh(mesh)
    working.faces.ensure_lookup_table()
    working.verts.ensure_lookup_table()
    # By INDEX, not by a set of vertex objects: `remove_doubles` keeps the first vertex of each
    # cluster, so the order it is handed decides which one survives and therefore the file's bytes
    # -- and a `set` of BMVerts iterates by address, which is a different answer every run. §18.4's
    # byte-identical claim caught it.
    chosen = sorted({vertex.index for face in working.faces
                     if face.material_index in receivers for vertex in face.verts})
    wanted = [working.verts[index] for index in chosen]
    before = len(working.verts)
    if wanted:
        bmesh.ops.remove_doubles(working, verts=wanted, dist=1e-5)
    removed = before - len(working.verts)
    working.to_mesh(mesh)
    working.free()
    mesh.update()
    return removed


#: A basement window well (`HOUSE-00469`). §12.6 says `W_BASEMENT` is a hopper "in 0.9 m window
#: wells" and gives its sill as −0.45 absolute, so the WIDTH of the well is §12.6's; how far it
#: stands out from the wall, how thick its retaining wall is and how far its floor sits below the
#: sill are this generator's.
WELL_WIDTH = 0.90
WELL_PROJECT = 0.60
WELL_WALL = 0.10
WELL_BELOW_SILL = 0.15
GRADE_Y = 0.0

#: The rainwater goods and the chimney (`HOUSE-00468`). §12 names none of them, so every section
#: here is this generator's; what is NOT invented is where the chimney stands, which is the
#: fireplace's own position, and how high it goes, which is §12's ridge plus the 0.60 m a stack
#: has to clear it by.
GUTTER_SECTION = 0.12
DOWNSPOUT_SECTION = 0.10
#: The ridge vent is `roof_geometry.ridge_vent`'s since `HOUSE-00491` -- one definition, and the
#: one the ventilation decision is written against. `HOUSE-00468` drew a flat lid floating above
#: the ridge from constants of its own; that was two answers to the same question and the flat one
#: could not straddle a slot it was supposed to cap.
CHIMNEY_ALONG = 1.10
CHIMNEY_ACROSS = 0.60
CHIMNEY_OVER_RIDGE = 0.60

#: A balcony's edge (`HOUSE-00465`, corrected by `HOUSE-00931`). §12.2 says the rear-extension
#: roof is "open" and used as the master balcony, while §12.3 supplies only the 1.10 m guard
#: height. These are ordinary painted-timber balustrade proportions: a low rail, 45 mm square
#: balusters with no clear opening over 95 mm, and 120 mm newels. The whole assembly is centred
#: 100 mm inside the deck edge, matching the conservative 200 mm collision band.
BALCONY_GUARD_INSET = 0.10
BALCONY_BOTTOM_RAIL_DEPTH = 0.10
BALCONY_BOTTOM_RAIL_HEIGHT = 0.08
BALCONY_BOTTOM_RAIL_CENTRE = 0.15
BALCONY_BALUSTER_SECTION = 0.045
BALCONY_BALUSTER_MAX_CLEAR = 0.095
BALCONY_NEWEL_SECTION = 0.12

#: The porch (`HOUSE-00464`, finished by `HOUSE-00929`). §12.1 says "a full-width front porch on
#: four square columns" and gives no section. The base/shaft/capital proportions below are in the
#: ordinary 250--400 mm range for a two-storey Colonial Revival entrance; the layered edge closes
#: the authored 300 mm floor-structure zone between the porch head and the balcony floor.
PORCH_COLUMNS = 4
COLUMN_SHAFT_SECTION = 0.26
COLUMN_BASE_SECTION = 0.40
COLUMN_BASE_HEIGHT = 0.14
COLUMN_BASE_CAP_SECTION = 0.34
COLUMN_BASE_CAP_HEIGHT = 0.08
COLUMN_NECK_SECTION = 0.32
COLUMN_NECK_HEIGHT = 0.08
COLUMN_CAPITAL_SECTION = 0.40
COLUMN_CAPITAL_HEIGHT = 0.12
PORCH_BEAM = 0.22
PORCH_FASCIA_DEPTH = 0.10
PORCH_CORNICE_HEIGHT = 0.08
PORCH_CORNICE_PROJECTION = 0.07

#: The attic's structure (`HOUSE-00463`). §12 gives the pitch, the ridge and the collar tie and
#: says nothing about members, so the spacing and the sections are this generator's: rafters at
#: 400 mm centres, 50 × 200, a purlin under each slope at mid-span, and a 600 mm walkway board.
RAFTER_SPACING = 0.40
RAFTER_WIDTH = 0.05
RAFTER_DEPTH = 0.20
PURLIN_SECTION = 0.15
WALKWAY_WIDTH = 0.60
WALKWAY_THICK = 0.030

def covering_floor(cell: dict, extent: tuple[float, float], cells_by_id: dict) -> float | None:
    """Return the floor height of a cell stacked over the whole footprint, or ``None``.

    That is what makes the porch a porch: `L1_BALCONY_FRONT` has the same box and its floor is
    0.30 m over the porch's head, so the porch has a roof and the rear balcony does not. Reading
    it this way means no cell has to be named here. `HOUSE-00929` needs the height as well as the
    yes/no answer so the finish closes the real authored floor-structure zone rather than a second
    guessed thickness.
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
            return float(above[0])
    return None


def covered_by(cell: dict, extent: tuple[float, float], cells_by_id: dict) -> bool:
    """Is another cell stacked over the whole of @p cell's footprint, just above it?"""
    return covering_floor(cell, extent, cells_by_id) is not None


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
            if not any(covers
                       for _lo, _hi, _wall, covers in side_intervals(side, box, cell, neighbours))]


def balcony_baluster_centres(lo: float, hi: float) -> list[float]:
    """Evenly spaced baluster centres between two end newels.

    The count is solved from the clear-opening limit rather than from an attractive-looking pitch:
    ``available = balusters + gaps`` and there is one more gap than baluster. This keeps the two
    end gaps under the same 95 mm bound as every internal one, including a short Juliet return.
    """
    available = hi - lo - 2.0 * BALCONY_NEWEL_SECTION
    if available <= 0.0:
        return []
    count = max(1, math.ceil((available - BALCONY_BALUSTER_MAX_CLEAR) /
                             (BALCONY_BALUSTER_SECTION + BALCONY_BALUSTER_MAX_CLEAR)))
    gap = (available - count * BALCONY_BALUSTER_SECTION) / (count + 1)
    first = lo + BALCONY_NEWEL_SECTION + gap + BALCONY_BALUSTER_SECTION / 2.0
    pitch = BALCONY_BALUSTER_SECTION + gap
    return [first + index * pitch for index in range(count)]


def balcony_clear_gaps(lo: float, hi: float) -> list[float]:
    """Clear gaps across the open guard span, including those beside its end newels."""
    centres = [lo + BALCONY_NEWEL_SECTION / 2.0]
    sections = [BALCONY_NEWEL_SECTION]
    for centre in balcony_baluster_centres(lo, hi):
        centres.append(centre)
        sections.append(BALCONY_BALUSTER_SECTION)
    centres.append(hi - BALCONY_NEWEL_SECTION / 2.0)
    sections.append(BALCONY_NEWEL_SECTION)
    return [centres[index + 1] - centres[index] -
            (sections[index] + sections[index + 1]) / 2.0
            for index in range(len(centres) - 1)]


def build_balcony_edge(cell: dict, extent: tuple, neighbours: list, construction, solid, add
                       ) -> int:
    """An open painted balustrade with a metal top rail round an elevated deck.

    §70.5's threshold is a drop over a metre, and the same number decides the rail's height, so it
    is asked once. A ground-level deck -- the porch at +0.57, the terrace at +0.45 -- gets nothing
    here; the porch's own balustrade is `HOUSE-00464`'s and is a different thing. Returns guarded
    side count, not primitive count: changing baluster spacing must not change what a caller thinks
    is an open edge.
    """
    if cell.get("kind") != "exterior" or not construction or extent[0] <= 1.0:
        return 0
    built = 0
    deck = extent[0]
    rail_height = float(construction.get("railing", 0.0))
    def emit_along(side, plane, a0, a1, y0, y1, depth, surface):
        inward = 1.0 if side in ("-X", "-Z") else -1.0
        across = plane + BALCONY_GUARD_INSET * inward
        if side in ("-X", "+X"):
            solid(across - depth / 2.0, across + depth / 2.0,
                  y0, y1, a0, a1, surface)
        else:
            solid(a0, a1, y0, y1,
                  across - depth / 2.0, across + depth / 2.0, surface)
        return across

    for box in cell_boxes(cell, extent):
        for side in open_sides_of(cell, box, neighbours):
            plane, lo, hi = side_span(side, box)
            bottom_low = deck + BALCONY_BOTTOM_RAIL_CENTRE - BALCONY_BOTTOM_RAIL_HEIGHT / 2.0
            bottom_high = bottom_low + BALCONY_BOTTOM_RAIL_HEIGHT
            guard_centre = emit_along(side, plane, lo, hi, bottom_low, bottom_high,
                                      BALCONY_BOTTOM_RAIL_DEPTH, "trim")
            post_top = deck + rail_height + RAIL_SECTION / 2.0
            for centre in (lo + BALCONY_NEWEL_SECTION / 2.0,
                           hi - BALCONY_NEWEL_SECTION / 2.0):
                emit_along(side, plane,
                           centre - BALCONY_NEWEL_SECTION / 2.0,
                           centre + BALCONY_NEWEL_SECTION / 2.0,
                           deck, post_top, BALCONY_NEWEL_SECTION, "trim")
            for centre in balcony_baluster_centres(lo, hi):
                emit_along(side, plane,
                           centre - BALCONY_BALUSTER_SECTION / 2.0,
                           centre + BALCONY_BALUSTER_SECTION / 2.0,
                           bottom_high, deck + rail_height - RAIL_SECTION / 2.0,
                           BALCONY_BALUSTER_SECTION, "trim")
            rail_along(add, side in ("-X", "+X"), lo, hi,
                       deck + rail_height, deck + rail_height,
                       guard_centre, RAIL_SECTION)
            built += 1
    return built


def window_well(side: str, outer_plane: float, u0: float, u1: float, sill: float) -> list:
    """The four boxes of a basement window well: two sides, an end and a floor.

    Outside the wall's OUTER face, which is the only place a well can be -- a well inside the wall
    is a hole in the basement. §12.6 puts the hopper's sill at −0.45 absolute and the well holds
    the earth back from it up to grade.
    """
    well_lo, well_hi = u0 - WELL_WALL, u1 + WELL_WALL
    floor_y = sill - WELL_BELOW_SILL
    out_dir = -1.0 if side in ("-X", "-Z") else 1.0
    far_out = outer_plane + WELL_PROJECT * out_dir
    lo_face, hi_face = min(outer_plane, far_out), max(outer_plane, far_out)
    end = far_out - WELL_WALL / 2.0 * out_dir
    end_lo, end_hi = min(end, far_out), max(end, far_out)
    boxes = []
    for edge in (well_lo, well_hi):
        if side in ("-X", "+X"):
            boxes.append((lo_face, hi_face, floor_y, GRADE_Y,
                          edge - WELL_WALL / 2.0, edge + WELL_WALL / 2.0))
        else:
            boxes.append((edge - WELL_WALL / 2.0, edge + WELL_WALL / 2.0,
                          floor_y, GRADE_Y, lo_face, hi_face))
    if side in ("-X", "+X"):
        boxes.append((end_lo, end_hi, floor_y, GRADE_Y, well_lo, well_hi))
        boxes.append((lo_face, hi_face, floor_y - WELL_WALL, floor_y, well_lo, well_hi))
    else:
        boxes.append((well_lo, well_hi, floor_y, GRADE_Y, end_lo, end_hi))
        boxes.append((well_lo, well_hi, floor_y - WELL_WALL, floor_y, lo_face, hi_face))
    return boxes


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


def roof_faces_for(level, layout, construction):
    """The `(corners, outward)` faces of the roof this level is bounded by, or `()`.

    `roof_planes_for` returns the same roof as plane EQUATIONS, which is what clipping a wall
    under it needs; this is the geometry itself, which is what drawing its underside needs.
    """
    if not level or level.get("ceiling") is not None or not level.get("roof"):
        return ()
    if not construction or not construction.get("roofPitch"):
        return ()
    box = roof_geometry.roof_boxes(layout).get(level["roof"])
    if box is None:
        return ()
    outer = roof_geometry.outer_box(box, construction)
    eaves = roof_geometry.roof_eaves(layout, level["roof"], box, construction)
    return roof_geometry.roof_planes(outer, eaves, float(construction["roofPitch"]))


def rafters_for(level, layout, construction):
    """`rafter_faces` for the roof this LEVEL is bounded by, or `()`.

    The same three-line derivation `roof_faces_for` makes, for the same reason: a level with a
    `roof` and a `null` ceiling looks up at that roof's structure (`HOUSE-00488`).
    """
    if not level or level.get("ceiling") is not None or not level.get("roof"):
        return ()
    if not construction or not construction.get("roofPitch"):
        return ()
    box = roof_geometry.roof_boxes(layout).get(level["roof"])
    if box is None:
        return ()
    outer = roof_geometry.outer_box(box, construction)
    eaves = roof_geometry.roof_eaves(layout, level["roof"], box, construction)
    return rafter_faces(outer, eaves, float(construction["roofPitch"]))


def roof_planes_for(level, layout, construction):
    """`roof_geometry.plane_equations` for the roof this LEVEL is bounded by, or `()`.

    The same derivation `build_collision.py`'s rafters use, and for the same reason: a level that
    declares a `roof` and a `null` ceiling is a level whose upper bound IS the roof
    (`HOUSE-00472`). A level with a ceiling plane has one and is left alone.
    """
    if not level or level.get("ceiling") is not None or not level.get("roof"):
        return ()
    if not construction or not construction.get("roofPitch"):
        return ()
    boxes = roof_geometry.roof_boxes(layout)
    box = boxes.get(level["roof"])
    if box is None:
        return ()
    outer = roof_geometry.outer_box(box, construction)
    eaves = roof_geometry.roof_eaves(layout, level["roof"], box, construction)
    return roof_geometry.plane_equations(
        roof_geometry.roof_planes(outer, eaves, float(construction["roofPitch"])))


def build_cell(cell: dict, extent: tuple[float, float], *, neighbours=(), construction=None,
               level=None, levels=None, portals=(), openings=None, cells_by_id=None,
               flights=(), roof=(), roof_planes_here=(), rafters_here=(),
               material_definitions=None):
    """One mesh object named for the cell: its floor, its ceiling and its walls' inner faces.

    @p roof is `roof_geometry.plane_equations` for the roof this cell's level is bounded by, or
    empty. It is what stops a rafter-bounded cell being drawn as a BOX (`HOUSE-00496`): §13.6's
    `yOverride` on an attic cell is its MAXIMUM head-room, so `L3_STORE_W` declares +13.90 and a
    skin built to that height stands outside a roof whose eaves are +10.57 at that wall. The
    house was a flat-topped box from the road, with §12.1's roof drawn inside it: dropping every
    roof chunk moved a full front elevation by 170 pixels. `HOUSE-00472` fixed the same misreading
    for collision and left the drawn shell alone.
    """
    construction = construction or {}
    neighbours = list(neighbours)
    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[int, ...]] = []

    openings = openings or {}
    cells_by_id = cells_by_id or {}
    portal_cells = {row["id"]: (row.get("cellA"), row.get("cellB")) for row in portals}

    classes: list[str] = []
    surface = {"class": "wall"}   # what the current pass is building; see `SURFACE_COLOURS`

    def add(points, outward, klass=None) -> None:
        base = len(vertices)
        vertices.extend(to_blender(*point) for point in facing(points, outward))
        faces.append(tuple(range(base, base + len(points))))
        classes.append(klass or surface["class"])

    def solid(bx0, bx1, by0, by1, bz0, bz1, klass=None) -> None:
        """A closed box, every face wound outward. Trim is looked at from every side."""
        if bx1 - bx0 <= 1e-9 or by1 - by0 <= 1e-9 or bz1 - bz0 <= 1e-9:
            return
        for value, outward in ((bx0, (-1.0, 0.0, 0.0)), (bx1, (1.0, 0.0, 0.0))):
            add([(value, by0, bz0), (value, by1, bz0), (value, by1, bz1), (value, by0, bz1)],
                outward, klass)
        for value, outward in ((by0, (0.0, -1.0, 0.0)), (by1, (0.0, 1.0, 0.0))):
            add([(bx0, value, bz0), (bx1, value, bz0), (bx1, value, bz1), (bx0, value, bz1)],
                outward, klass)
        for value, outward in ((bz0, (0.0, 0.0, -1.0)), (bz1, (0.0, 0.0, 1.0))):
            add([(bx0, by0, value), (bx1, by0, value), (bx1, by1, value), (bx0, by1, value)],
                outward, klass)

    for box in cell_boxes(cell, extent):
        x0, x1, y0, y1, z0, z1 = box
        ix0, ix1, _, _, iz0, iz1 = inset_box(box, cell, neighbours, construction)
        nested = enclosed_by_parent(cell, box, cells_by_id, levels or {})

        # `HOUSE-00453`: one face per run of the side, at the inner face of the wall that bounds
        # THAT run. A shared wall is described once -- here, from the pair of cells that share the
        # plane -- and each cell carries the face that looks at it, so a cell's chunk is complete
        # on its own (§27's residency) and no two chunks hold the same surface twice.
        #
        # A run is CLAMPED to the room's inset extent on the perpendicular axis: at a corner the
        # two inner faces stop at each other, and a face that ran on to the centre line would
        # continue 75 mm into the wall it meets.
        # `HOUSE-00475`: an EXTERIOR cell has no walls. A yard is ground and sky, and the wall it
        # abuts belongs to the house on the other side of it, which draws its own outer face
        # (`exterior`, below). Building them here gave every exterior cell a full set of faces at
        # its own `yOverride` height -- 20 m for the yards, 65 m for `EXT_WORLD` -- so the first
        # frame the blockout ever drew was the inside of a 400 m box with the house somewhere in
        # it. `build_collision.py` has always known this (`is_open and neighbour is None`); the
        # shell did not, and nothing had drawn the shell.
        #
        # `HOUSE-00488`: and ONLY an exterior cell. §16's `visibilityHint: "open"` says a cell is
        # open to its neighbours for VISIBILITY -- a landing and the stair well it hangs over are
        # one space to look through -- and reading it here as "has no walls" left the five open
        # interior cells drawing a floor and a ceiling and nothing else. The wall a body on
        # `L1_LANDING` faces then belonged to the room behind it, §25 correctly left that room out,
        # and the wall went with it: 5 267 pixels of three frames became the clear colour. A
        # landing has walls; what makes it open is the cased openings in them, and those are
        # portals, which `holes_in` has always cut.
        open_cell = cell.get("kind") == "exterior"
        for side, inward in INWARD.items():
            side_holes = holes_in(side, box, cell, list(portals))
            for lo, hi, wall, covers in ([] if open_cell
                                         else side_intervals(side, box, cell, neighbours)):
                half = float(construction.get(wall, 0.0)) / 2.0
                plane = {"-X": x0 + half, "+X": x1 - half,
                         "-Z": z0 + half, "+Z": z1 - half}[side]
                clamp = (iz0, iz1) if side in ("-X", "+X") else (ix0, ix1)
                lo, hi = max(lo, clamp[0]), min(hi, clamp[1])
                if hi - lo <= 1e-6:
                    continue
                holes = [hole for hole in side_holes
                         if min(hole[1], hi) - max(hole[0], lo) > 1e-6]
                inner_lines = roof_lines(roof, side, plane)
                for pu0, pu1, pv0, pv1 in panel(lo, hi, y0, y1, holes):
                    for pu, pv in under_roof(pu0, pu1, pv0, pv1, inner_lines):
                        if side in ("-X", "+X"):
                            corners = [(plane, v, u) for u, v in zip(pu, pv)]
                        else:
                            corners = [(u, v, plane) for u, v in zip(pu, pv)]
                        add(corners, inward)

                # `HOUSE-00454`: the OUTER face of the same wall, over the heights where there
                # IS an outside. A partition has two rooms and each draws its inner face; an
                # exterior wall has one room and the weather, and the weather's side is here.
                #
                # `HOUSE-00485`: "is there an outside" is a question about a Y RANGE and not about
                # the wall's name. Reading the name put an `exterior` face into the room across a
                # garage wall -- coplanar with and facing the same way as that room's own inner
                # face, 278.97 m² of it, and which one a pixel showed decided by nothing but
                # submission order. Answering it with a plain "is anything across this run" then
                # took the skin off the whole storey and left a hole above the garage roof, where
                # the same wall really does face the weather. So the span this would have drawn has
                # the cells across the run subtracted from it, and what is left is drawn.
                outside = not covers and level is not None and not nested
                outer_plane = plane
                if level is not None and not nested:
                    outer_name = outer_wall_name(level, wall)
                    outer_half = float(construction.get(outer_name, 0.0)) / 2.0
                    outer_plane = {"-X": x0 - outer_half, "+X": x1 + outer_half,
                                   "-Z": z0 - outer_half, "+Z": z1 + outer_half}[side]
                    oy0, oy1 = outer_span(cell, (y0, y1), level, levels or {})
                    # Two cells stacked on the far side of this wall leave a gap between them
                    # -- the floor band of the upper one -- and that gap is floor structure, not
                    # weather. Bridging it keeps the skin out of the sandwich between two rooms;
                    # anything wider than `INTERIOR_GAP` is a room's worth of air and is left
                    # alone, which is what the space above the garage's roof is.
                    cuts: list[tuple[float, float]] = []
                    for c0, c1 in sorted(covers):
                        if cuts and c0 - cuts[-1][1] <= INTERIOR_GAP:
                            cuts[-1] = (cuts[-1][0], max(cuts[-1][1], c1))
                        else:
                            cuts.append((c0, c1))
                    # The floor band above this cell -- `outer_span`'s 0.35 m of joists, which the
                    # LOWER cell carries -- is interior wherever a cell across this run spans the
                    # whole of this one: the room next door has a floor band of its own there. It
                    # is NOT interior where the cell across is shorter than this one, which is the
                    # garage: above its roof the band is as much weather as the wall under it.
                    if any(c0 <= y0 + 1e-6 and c1 >= y1 - 1e-6 for c0, c1 in covers):
                        cuts.append((y1, oy1))
                    outer_lines = roof_lines(roof, side, outer_plane)
                    for vy0, vy1 in minus(oy0, oy1, cuts):
                        for pu0, pu1, pv0, pv1 in panel(lo, hi, vy0, vy1, holes):
                            for pu, pv in under_roof(pu0, pu1, pv0, pv1, outer_lines):
                                if side in ("-X", "+X"):
                                    outer = [(outer_plane, v, u) for u, v in zip(pu, pv)]
                                else:
                                    outer = [(u, v, outer_plane) for u, v in zip(pu, pv)]
                                add(outer, tuple(-value for value in inward), "exterior")

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
                            solid(lo_face, hi_face, bv0, bv1, bu0, bu1, "trim")
                        else:
                            solid(bu0, bu1, bv0, bv1, lo_face, hi_face, "trim")
                    # The threshold: a board across the opening, this room's half of the wall.
                    sill_lo, sill_hi = min(plane, far), max(plane, far)
                    if side in ("-X", "+X"):
                        solid(sill_lo, sill_hi, hv0, hv0 + THRESHOLD_THICK, hu0, hu1, "trim")
                    else:
                        solid(hu0, hu1, hv0, hv0 + THRESHOLD_THICK, sill_lo, sill_hi, "trim")

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
                                  band_lo, band_hi, bu0, bu1, "trim")
                        else:
                            solid(bu0, bu1, band_lo, band_hi,
                                  min(plane, proud_at), max(plane, proud_at), "trim")

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

                    def band(u0, u1, v0, v1, d0=lo_face, d1=hi_face, at=side, klass="trim"):
                        if at in ("-X", "+X"):
                            solid(d0, d1, v0, v1, u0, u1, klass)
                        else:
                            solid(u0, u1, v0, v1, d0, d1, klass)

                    owner = window_owner(portal_cells.get(hole[4]), cells_by_id, cell["id"])
                    if owner == cell["id"]:
                        frame_class = "window_frame" if outside else "trim"
                        glass_class = "window_glass" if outside else "glass"
                        # The frame: a ring round the hole, filling the reveal's depth.
                        f = FRAME_SECTION
                        band(hu0, hu1, hv0, hv0 + f, klass=frame_class)
                        band(hu0, hu1, hv1 - f, hv1, klass=frame_class)
                        band(hu0, hu0 + f, hv0 + f, hv1 - f, klass=frame_class)
                        band(hu1 - f, hu1, hv0 + f, hv1 - f, klass=frame_class)
                        # The sash, inside the frame, and the glass inside the sash.
                        su0, su1, sv0, sv1 = hu0 + f, hu1 - f, hv0 + f, hv1 - f
                        g = SASH_SECTION
                        depth = (lo_face + hi_face) / 2.0
                        sash_lo, sash_hi = depth - g / 2.0, depth + g / 2.0
                        band(su0, su1, sv0, sv0 + g, sash_lo, sash_hi, klass=frame_class)
                        band(su0, su1, sv1 - g, sv1, sash_lo, sash_hi, klass=frame_class)
                        band(su0, su0 + g, sv0 + g, sv1 - g, sash_lo, sash_hi, klass=frame_class)
                        band(su1 - g, su1, sv0 + g, sv1 - g, sash_lo, sash_hi, klass=frame_class)
                        if has_meeting_rail(opening.get("type")):
                            middle = (sv0 + sv1) / 2.0
                            band(su0, su1, middle - MEETING_RAIL / 2.0,
                                 middle + MEETING_RAIL / 2.0, sash_lo, sash_hi,
                                 klass=frame_class)
                        band(su0 + g, su1 - g, sv0 + g, sv1 - g,
                             depth - GLASS_THICK / 2.0, depth + GLASS_THICK / 2.0,
                             klass=glass_class)

                        # `HOUSE-00933`: §12.1's front windows are 6-over-6, not one broad pane
                        # above another. The opening row opts in; each sash receives one vertical
                        # and two horizontal bars, hence exactly two columns by three rows. These
                        # stay in the existing weather-facing frame role and therefore cannot pull
                        # a room's skirting into the exterior scene.
                        if opening.get("muntinPattern") == "six_over_six":
                            middle = (sv0 + sv1) / 2.0
                            pane_lo = sv0 + g
                            pane_hi = sv1 - g
                            sash_panes = ((pane_lo, middle - MEETING_RAIL / 2.0),
                                          (middle + MEETING_RAIL / 2.0, pane_hi))
                            muntin_u = (su0 + su1) / 2.0
                            for pane_v0, pane_v1 in sash_panes:
                                band(muntin_u - MUNTIN_SECTION / 2.0,
                                     muntin_u + MUNTIN_SECTION / 2.0,
                                     pane_v0, pane_v1, sash_lo, sash_hi,
                                     klass=frame_class)
                                for division in (1.0 / 3.0, 2.0 / 3.0):
                                    muntin_v = pane_v0 + (pane_v1 - pane_v0) * division
                                    band(su0 + g, su1 - g,
                                         muntin_v - MUNTIN_SECTION / 2.0,
                                         muntin_v + MUNTIN_SECTION / 2.0,
                                         sash_lo, sash_hi, klass=frame_class)

                        # The same authored treatment supplies paired decorative shutters. They
                        # are joinery, not two black slabs: full-depth stiles/rails surround two
                        # banks of recessed horizontal louvers. Their 420 mm width fits the close
                        # pairs on this elevation without overlapping, while remaining a credible
                        # proportion for the 1.20 m openings.
                        if outside and opening.get("shutterMaterial"):
                            outward = -1.0 if side in ("-X", "-Z") else 1.0
                            frame_d0, frame_d1 = sorted(
                                (far_side, far_side + outward * SHUTTER_DEPTH))
                            slat_d0, slat_d1 = sorted(
                                (far_side, far_side + outward * SHUTTER_DEPTH * 0.62))

                            def shutter_leaf(shutter_u0, shutter_u1):
                                band(shutter_u0, shutter_u0 + SHUTTER_STILE, hv0, hv1,
                                     frame_d0, frame_d1, klass="window_shutter")
                                band(shutter_u1 - SHUTTER_STILE, shutter_u1, hv0, hv1,
                                     frame_d0, frame_d1, klass="window_shutter")
                                shutter_middle = (hv0 + hv1) / 2.0
                                for rail_v0, rail_v1 in (
                                        (hv0, hv0 + SHUTTER_RAIL),
                                        (shutter_middle - SHUTTER_RAIL / 2.0,
                                         shutter_middle + SHUTTER_RAIL / 2.0),
                                        (hv1 - SHUTTER_RAIL, hv1)):
                                    band(shutter_u0 + SHUTTER_STILE,
                                         shutter_u1 - SHUTTER_STILE, rail_v0, rail_v1,
                                         frame_d0, frame_d1, klass="window_shutter")
                                louver_u0 = shutter_u0 + SHUTTER_STILE
                                louver_u1 = shutter_u1 - SHUTTER_STILE
                                for panel_v0, panel_v1 in (
                                        (hv0 + SHUTTER_RAIL,
                                         shutter_middle - SHUTTER_RAIL / 2.0),
                                        (shutter_middle + SHUTTER_RAIL / 2.0,
                                         hv1 - SHUTTER_RAIL)):
                                    span = panel_v1 - panel_v0
                                    count = max(1, int(span / SHUTTER_SLAT_PITCH))
                                    spacing = span / (count + 1)
                                    for slat_index in range(1, count + 1):
                                        slat_v = panel_v0 + spacing * slat_index
                                        band(louver_u0, louver_u1,
                                             slat_v - SHUTTER_SLAT / 2.0,
                                             slat_v + SHUTTER_SLAT / 2.0,
                                             slat_d0, slat_d1, klass="window_shutter")

                            shutter_leaf(hu0 - SHUTTER_GAP - SHUTTER_WIDTH,
                                         hu0 - SHUTTER_GAP)
                            shutter_leaf(hu1 + SHUTTER_GAP,
                                         hu1 + SHUTTER_GAP + SHUTTER_WIDTH)

                    # `HOUSE-00469`: a basement hopper sits in a well, outside the wall, open to
                    # the sky. §12.6 says so and gives the sill at −0.45 absolute; the well holds
                    # the earth back from it.
                    if str(opening.get("type")) == "W_BASEMENT" and outside:
                        for well_box in window_well(side, outer_plane, hu0, hu1, hv0):
                            solid(*well_box, "exterior")

                    # The sill board, projecting into THIS room under the opening.
                    casing = float((opening.get("frame") or {}).get("casing") or 0.0)
                    proud = plane + SILL_PROJECT * (1.0 if side in ("-X", "-Z") else -1.0)
                    band(hu0 - casing, hu1 + casing, hv0 - SILL_PROJECT, hv0,
                         min(plane, proud), max(plane, proud))

                # `HOUSE-00486`: the door LEAF, and the lining that closes the gap round it.
                #
                # The shell filled a window with glass and a doorway with nothing until §25's
                # culling was turned on (`HOUSE-00684`) and every shut door became a hole to the
                # clear colour. §65.6 starts them all shut, so a shut leaf is what the house looks
                # like; §15's animated door replaces this in phase 15 and this class disappears
                # with it.
                #
                # Built by BOTH rooms, in each one's own half of the reveal -- like the
                # architrave and the threshold above, and unlike a window's frame. A window is one
                # object and its owner builds it; a leaf built once is a leaf that belongs to ONE
                # cell's chunk, and §25 culls the room behind a shut door -- so the room in front
                # of it would be left looking at the hole again, which is the whole bug this task
                # is fixing. Two half-wall-deep slabs never meet, so there is nothing to z-fight.
                for hole in holes:
                    opening = openings.get(hole[4])
                    if opening is None or opening.get("kind") != "door":
                        continue
                    hu0, hu1, hv0, hv1 = hole[0], hole[1], hole[2], hole[3]
                    leaf = opening.get("leaf") or {}
                    thickness = float(leaf.get("thickness") or 0.04)
                    width = min(float(leaf.get("width") or (hu1 - hu0)), hu1 - hu0)
                    height = min(float(leaf.get("height") or (hv1 - hv0)), hv1 - hv0)
                    # Centred across the opening and sitting on the threshold, which is where a
                    # closed door is. The leftover -- 20 mm each side and 50 mm at the head on
                    # §12's doors -- is the lining, so the hole is closed and the gap a real door
                    # has is still the gap it has.
                    lu0 = (hu0 + hu1) / 2.0 - width / 2.0
                    lu1 = lu0 + width
                    lv0, lv1 = hv0, hv0 + height
                    reveal_lo, reveal_hi = min(plane, far), max(plane, far)
                    middle = (reveal_lo + reveal_hi) / 2.0

                    def leaf_box(u0, u1, v0, v1, d0, d1, klass):
                        if side in ("-X", "+X"):
                            solid(d0, d1, v0, v1, u0, u1, klass)
                        else:
                            solid(u0, u1, v0, v1, d0, d1, klass)

                    # `trim`, and NOT a class of its own. A leaf in its own colour would be
                    # easier to pick out, and it would be an eleventh blockout material -- which
                    # pushes `L0_GARAGE` and the three attic stores past §17.4's six chunks a cell,
                    # measured. A door is joinery: it belongs in the class that already holds the
                    # architrave round it, the sash beside it and the skirting under it, and §18.3
                    # keeps all four out of the bake for the same reason. §11's material table
                    # separates them when it arrives.
                    portal_pair = portal_cells.get(hole[4], ())
                    weather_facing = any(
                        (cells_by_id.get(adjacent) or {}).get("kind") == "exterior"
                        for adjacent in portal_pair)
                    leaf_class = ("exterior_door"
                                  if weather_facing
                                  and is_exterior_door_material(opening.get("material"))
                                  else "trim")
                    leaf_box(lu0, lu1, lv0, lv1,
                             middle - thickness / 2.0, middle + thickness / 2.0, leaf_class)
                    if leaf_class == "exterior_door" and opening.get("type") == "D_ENTRY":
                        for detail in entry_door_detail_boxes(
                                lu0, lu1, lv0, lv1, middle - thickness / 2.0,
                                middle + thickness / 2.0, opening.get("hinge")):
                            leaf_box(*detail[:6], detail[6])
                        # A thin bronze cap finishes the existing full-depth timber threshold.
                        # It begins exactly at that board's top, so it neither changes the clear
                        # opening nor introduces coincident faces.
                        leaf_box(hu0 + 0.010, hu1 - 0.010, hv0 + THRESHOLD_THICK,
                                 hv0 + THRESHOLD_THICK + ENTRY_THRESHOLD_CAP,
                                 reveal_lo, reveal_hi, "exterior_door_hardware")
                    # The lining: the reveal's full depth, filling what the leaf does not.
                    leaf_box(hu0, lu0, hv0, hv1, reveal_lo, reveal_hi, "trim")
                    leaf_box(lu1, hu1, hv0, hv1, reveal_lo, reveal_hi, "trim")
                    leaf_box(lu0, lu1, lv1, hv1, reveal_lo, reveal_hi, "trim")

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
                        add(quad, look, "trim")

        # `HOUSE-00452`: the floor and the ceiling, inset to the same inner faces.
        for level_y, look in ((y0, (0.0, 1.0, 0.0)), (y1, (0.0, -1.0, 0.0))):
            if not slab_here(cell, extent, level_y == y0):
                continue
            wells = slab_holes(list(portals), cell, level_y, box)
            # A CEILING under a roof is only a lid where the roof is over it (`HOUSE-00496`):
            # §13.6's `yOverride` on an attic cell is its maximum head-room, so `L3_STORE_W`'s
            # +13.90 slab stands out through a roof that is +10.57 at its own wall. Over the rest
            # of the attic the rafters are the lid, and they are already there.
            over = roof_over(roof, level_y) if (roof and level_y != y0) else []
            for px0, px1, pz0, pz1 in panel(ix0, ix1, iz0, iz1, wells):
                for corners in ([[(px0, pz0), (px1, pz0), (px1, pz1), (px0, pz1)]] if not over
                                else [clip_half_planes([(px0, pz0), (px1, pz0),
                                                        (px1, pz1), (px0, pz1)], over)]):
                    if len(corners) < 3:
                        continue
                    add([(x, level_y, z) for x, z in corners],
                        look, "floor" if level_y == y0 else "ceiling")

            # `HOUSE-00460`: a railing round the hole in the FLOOR -- §70.5 asks for 1.05 m at a
            # drop over a metre and §12 declares 1.10 -- with a gap where the stair arrives. A
            # railing across the top of the flight would be a railing you have to climb.
            if level_y != y0 or not construction:
                continue
            arrivals = [top_tread_box(row, float(row.get("fromY") or 0.0), list(portals))
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

    # `HOUSE-00496`: and where the roof is the lid, the roof is what this cell draws overhead.
    # Clipping the walls to the slope without this leaves the attic open to the sky from inside --
    # `l3-store-w`'s pose went from a room to 68 % of one -- because §12.1's roof is drawn in
    # `ROOF_MAIN`, which belongs to the outdoors and is not visible from in here.
    # `build_collision.py` gives each attic cell its own rafter pieces for the same reason and
    # from the same planes (`HOUSE-00472`); this is that decision, drawn.
    if roof_planes_here:
        for plane_corners, _outward in roof_planes_here:
            for bx0, bx1, bz0, bz1 in layout_io.cell_boxes(cell):
                piece = roof_geometry.clip_to_rect(plane_corners, (bx0, bx1, bz0, bz1))
                if roof_geometry.plan_area(piece) < 0.01:
                    continue
                # Class `roof` and not `ceiling`: a cell's ceiling is ONE horizontal plane and
                # `verify_shell` reads clear heights off it, while this is a slope. §13.6 measures
                # an attic in a range for the same reason -- "1.2 -> 4.6 m, so most of it is
                # crouch-only".
                add([tuple(point) for point in piece], (0.0, -1.0, 0.0), "roof")

        # `HOUSE-00488`: and the RAFTERS under that slope, which is what a rafter-bounded attic
        # actually looks up at. `HOUSE-00496` gave the cell the roof surface and left the rafters
        # in `ROOF_MAIN`, so `l3-room` lost 4 456 pixels of them the moment §25 culled the cell
        # that file is drawn with. `build_collision.py` has given each attic cell its own rafter
        # pieces since `HOUSE-00472`; this is the same decision, drawn.
        for rafter_corners, rafter_outward in rafters_here or ():
            for bx0, bx1, bz0, bz1 in layout_io.cell_boxes(cell):
                # `clip_face` and not `clip_to_rect`: a PURLIN is vertical, its plan projection is
                # a line, and the plane-equation clip correctly refuses it. Losing it cost 1 722
                # pixels of `l3-room` and was caught by the reference comparison.
                piece = roof_geometry.clip_face(rafter_corners, (bx0, bx1, bz0, bz1))
                if len(piece) < 3:
                    continue
                add([tuple(point) for point in piece], rafter_outward, "structure")

    surface["class"] = "metal"
    build_balcony_edge(cell, extent, list(neighbours), construction, solid, add)
    build_mezzanine_guard(cell, extent, cells_by_id, levels or {}, construction, add)
    surface["class"] = "wall"

    # `HOUSE-00464`: a covered deck stands on columns and has a balustrade round its open sides.
    # The porch is the one cell in this house that is covered -- `L1_BALCONY_FRONT` sits on it --
    # and the open sides are the ones with no interior cell across them, which is the same test
    # the walls use.
    cover_y = covering_floor(cell, extent, cells_by_id)
    if cell.get("kind") == "exterior" and construction and cover_y is not None:
        for box in cell_boxes(cell, extent):
            bx0, bx1, by0, by1, bz0, bz1 = box
            open_sides = [side for side in ("-X", "+X", "-Z", "+Z")
                          if not any(covers
                                     for _lo, _hi, _wall, covers in side_intervals(
                                         side, box, cell, list(neighbours)))]
            beam_lo = by1 - PORCH_BEAM
            shaft_half = COLUMN_SHAFT_SECTION / 2.0
            base_half = COLUMN_BASE_SECTION / 2.0

            # A covered exterior cell is not an indoor room and therefore receives no ordinary
            # ceiling slab. It still needs the painted underside of the construction above it.
            # A single downward-facing finish plane preserves the shell's floor/ceiling winding
            # contract while the fascia closes the exact gap to the covering cell's floor.
            add([(bx0, by1, bz0), (bx1, by1, bz0),
                 (bx1, by1, bz1), (bx0, by1, bz1)],
                (0.0, -1.0, 0.0), "ceiling")

            # The columns go along the longest open side, evenly spread including its two ends.
            longest = max(open_sides, key=lambda side: side_span(side, box)[2]
                          - side_span(side, box)[1], default=None)
            if longest is not None:
                plane, lo, hi = side_span(longest, box)
                for index in range(PORCH_COLUMNS):
                    at = (lo + base_half
                          + (hi - lo - COLUMN_BASE_SECTION) * index / (PORCH_COLUMNS - 1))
                    centre_x, centre_z = ((plane, at) if longest in ("-X", "+X")
                                          else (at, plane))

                    def column_box(section: float, bottom: float, top: float) -> None:
                        half = section / 2.0
                        solid(centre_x - half, centre_x + half, bottom, top,
                              centre_z - half, centre_z + half, "trim")

                    base_top = by0 + COLUMN_BASE_HEIGHT
                    base_cap_top = base_top + COLUMN_BASE_CAP_HEIGHT
                    capital_bottom = beam_lo - COLUMN_CAPITAL_HEIGHT
                    neck_bottom = capital_bottom - COLUMN_NECK_HEIGHT
                    column_box(COLUMN_BASE_SECTION, by0, base_top)
                    column_box(COLUMN_BASE_CAP_SECTION, base_top, base_cap_top)
                    column_box(COLUMN_SHAFT_SECTION, base_cap_top, neck_bottom)
                    column_box(COLUMN_NECK_SECTION, neck_bottom, capital_bottom)
                    column_box(COLUMN_CAPITAL_SECTION, capital_bottom, beam_lo)
            for side in open_sides:
                plane, lo, hi = side_span(side, box)
                inward = 1.0 if side in ("-X", "-Z") else -1.0
                if side in ("-X", "+X"):
                    solid(plane - shaft_half, plane + shaft_half,
                          beam_lo, by1, lo, hi, "trim")
                else:
                    solid(lo, hi, beam_lo, by1,
                          plane - shaft_half, plane + shaft_half, "trim")

                # The beam is structural; fascia and a slightly projecting cornice make the
                # 300 mm balcony-floor zone read as a supported roof edge instead of a floating
                # slab. Both follow every open side and retain the existing clean painted trim.
                fascia_outer = plane
                fascia_inner = plane + PORCH_FASCIA_DEPTH * inward
                cornice_outer = plane - PORCH_CORNICE_PROJECTION * inward
                if side in ("-X", "+X"):
                    solid(min(fascia_outer, fascia_inner), max(fascia_outer, fascia_inner),
                          by1, cover_y, lo, hi, "trim")
                    solid(min(cornice_outer, fascia_inner), max(cornice_outer, fascia_inner),
                          cover_y - PORCH_CORNICE_HEIGHT, cover_y, lo, hi, "trim")
                else:
                    solid(lo, hi, by1, cover_y,
                          min(fascia_outer, fascia_inner), max(fascia_outer, fascia_inner), "trim")
                    solid(lo, hi, cover_y - PORCH_CORNICE_HEIGHT, cover_y,
                          min(cornice_outer, fascia_inner), max(cornice_outer, fascia_inner),
                          "trim")
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
                    rail_along(lambda points, out: add(points, out, "metal"),
                               side in ("-X", "+X"), rail_lo, rail_hi,
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
                      middle - WALKWAY_WIDTH / 2.0, middle + WALKWAY_WIDTH / 2.0, "structure")
            else:
                middle = (bx0 + bx1) / 2.0
                solid(middle - WALKWAY_WIDTH / 2.0, middle + WALKWAY_WIDTH / 2.0,
                      by0, by0 + WALKWAY_THICK, bz0, bz1, "structure")

    # `HOUSE-00459`: the flights that stand in this cell. A flight is carried by its `fromCell`,
    # the one it starts in, so it is built once and it is in the chunk of the room you are
    # standing in when you begin to climb.
    for flight in flights or ():
        if flight.get("fromCell") != cell["id"]:
            continue
        # The foot of the flight: what it declares if it declares one -- the porch, terrace and
        # garage steps join two cells on ONE level and are the only things that know what they
        # climb -- and otherwise the floor of the cell it stands in.
        surface["class"] = "stair"
        foot = flight.get("fromY")
        build_flight(flight, solid, float(foot) if foot is not None else extent[0],
                     add=add, construction=construction,
                     inner=list(cell_boxes(cell, extent))[0], portals=list(portals))
        surface["class"] = "wall"

    mesh = bpy.data.meshes.new(f"{cell['id']}_mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.validate()
    mesh.update()
    material_slots(mesh, cell_surface_materials(cell, (openings or {}).values(), portals, flights,
                                                cells_by_id),
                   material_definitions)
    for polygon, klass in zip(mesh.polygons, classes):
        polygon.material_index = SURFACE_ORDER.index(klass)
    weld(mesh)          # before the UVs: welding moves loops, and a face keeps its material
    planar_uvs(mesh)
    obj = bpy.data.objects.new(cell["id"], mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def export(obj, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    # `export_extras` carries each material's `surfaceClass` and `lightmapReceiver` into the
    # `.glb`, so the classification the generator made is in the file rather than in a convention.
    bpy.ops.export_scene.gltf(filepath=str(path), export_format="GLB", use_selection=True,
                              export_yup=True, export_extras=True)


def generate(directory: Path, output: Path, wanted: set[str] | None = None) -> dict:
    """Every cell the layout declares, as one `.glb` each. Returns a report."""
    layout = layout_io.load_layout(
        directory,
        kinds=["levels", "cells", "portals", "openings", "stairs", "interactables", "materials"])
    levels = {row["id"]: row for row in layout_io.rows(layout, "levels")}
    portals = layout_io.rows(layout, "portals")
    openings = {row["portal"]: row for row in layout_io.rows(layout, "openings")
                if row.get("portal")}
    stair_rows = layout_io.rows(layout, "stairs")
    material_definitions = {
        row["id"]: row for row in layout_io.rows(layout, "materials")
    }
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
                         flights=stair_rows, roof=roof_planes_for(level, layout, construction),
                         roof_planes_here=roof_faces_for(level, layout, construction),
                         rafters_here=rafters_for(level, layout, construction),
                         material_definitions=material_definitions)
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
        material_slots(mesh, dict(SHELL_MATERIALS, exterior="MAT_BRICK_WATER_TABLE"),
                       material_definitions)
        for polygon in mesh.polygons:
            polygon.material_index = SURFACE_ORDER.index("exterior")
        weld(mesh)
        planar_uvs(mesh)
        stack = bpy.data.objects.new("CHIMNEY", mesh)
        bpy.context.scene.collection.objects.link(stack)
        export(stack, output / "CHIMNEY.glb")
        report["written"].append("CHIMNEY")

    # `HOUSE-00776`: where the water comes off this roof, and the ground it lands on.
    all_spouts = roof_geometry.house_downspouts(layout)
    try:
        _w, _h, terrain_heights, _materials = terrain_gen.decode(directory)
        for row in all_spouts:
            row["groundY"] = terrain_gen.height_at(terrain_heights, row["x"], row["z"])
    except (FileNotFoundError, OSError):
        pass                                    # a world with no height field: the pipes reach +0
    for name, box in sorted(roof_boxes(layout, levels).items()):
        if wanted is not None and name not in wanted:
            continue
        reset_scene()
        # `HOUSE-00488`: the rafters move to the cells under a roof that BOUNDS a level -- a
        # level with a `roof` and a `null` ceiling. A roof over a level that has a ceiling
        # (`ROOF_GARAGE` over `L0`) keeps its own, because nothing below it will draw them.
        bounded = any(row.get("roof") == name and row.get("ceiling") is None
                      for row in layout_io.rows(layout, "levels"))
        obj = build_roof(name, box, construction,
                         dormers=dormers_on(box, portals, openings.values()),
                         eaves=roof_geometry.roof_eaves(layout, name, box, construction),
                         spouts=[row for row in all_spouts if row["roof"] == name],
                         structure_by_cells=bounded,
                         material_definitions=material_definitions)
        export(obj, output / f"{name}.glb")
        report["written"].append(name)
    return report


def rafter_faces(outer: tuple, eaves_y: float, pitch: float):
    """`HOUSE-00463`'s rafters and purlins under the two long planes, as `(corners, outward)`.

    Factored out of `build_roof` by `HOUSE-00488` so that a rafter-bounded CELL can draw the ones
    that stand over it. The hip ends carry jack rafters in a real roof and none here: they are a
    different length each, and the attic's unfinished stores -- the only place you see structure --
    are under the long slopes.
    """
    x0, x1, z0, z1 = outer
    dx, dz = x1 - x0, z1 - z0
    if min(dx, dz) <= 0.0:
        return []
    faces = []
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
            faces.append((rafter, (0.0, -1.0, 0.0)))
    # A purlin under each slope, halfway up it.
    for side in (-1.0, 1.0):
        near = (z0 if side < 0 else z1) if dx >= dz else (x0 if side < 0 else x1)
        mid = ((z0 + z1) / 2.0) if dx >= dz else ((x0 + x1) / 2.0)
        at = (near + mid) / 2.0
        level = (eaves_y + top) / 2.0 - RAFTER_DEPTH
        if dx >= dz:
            faces.append(([(along0, level - PURLIN_SECTION, at - PURLIN_SECTION / 2.0),
                           (along1, level - PURLIN_SECTION, at - PURLIN_SECTION / 2.0),
                           (along1, level, at - PURLIN_SECTION / 2.0),
                           (along0, level, at - PURLIN_SECTION / 2.0)], (0.0, 0.0, -1.0)))
        else:
            faces.append(([(at - PURLIN_SECTION / 2.0, level - PURLIN_SECTION, along0),
                           (at - PURLIN_SECTION / 2.0, level - PURLIN_SECTION, along1),
                           (at - PURLIN_SECTION / 2.0, level, along1),
                           (at - PURLIN_SECTION / 2.0, level, along0)], (-1.0, 0.0, 0.0)))
    return faces


def build_roof(name: str, box: tuple, construction: dict, dormers=(), eaves=None,
               spouts=(), structure_by_cells: bool = False, material_definitions=None):
    """One roof object over @p box, with its fascia. @p box is the WALL CENTRE-LINE rectangle.

    @p eaves is `roof_geometry.roof_eaves`'s answer, which is §12's ridge for the roof a level
    declares and the head of the covered cells for one it does not (`HOUSE-00484`).
    """
    half_wall = float(construction.get("wallExterior", 0.0)) / 2.0
    reach = half_wall + EAVES_OVERHANG
    outer = (box[0] - reach, box[1] + reach, box[2] - reach, box[3] + reach)
    eaves_y = eaves_height(construction, outer) if eaves is None else float(eaves)
    pitch = float(construction["roofPitch"])

    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[int, ...]] = []

    classes: list[str] = []

    def add(points, outward, klass="roof") -> None:
        base = len(vertices)
        vertices.extend(to_blender(*point) for point in facing(points, outward))
        faces.append(tuple(range(base, base + len(points))))
        classes.append(klass)

    # The planes, with a hole in them where each dormer comes through (`HOUSE-00490`): leaving
    # them whole put a slope across the inside of every dormer window.
    for corners, outward in roof_planes(outer, eaves_y, pitch, dormers or ()):
        add(corners, outward, "roof")

    # `HOUSE-00462`: the dormers, which belong to the roof they come through.
    for rect_u, rect_v, plane_z in dormers or ():
        for corners, face_outward in dormer_shell(rect_u, rect_v, plane_z, outer, eaves_y, pitch):
            add(corners, face_outward, "roof")


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
        # Fascia and soffit are one painted eaves finish; shingles stop at the roof plane.
        add(corners, outward, "trim")
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
        add(corners, (0.0, -1.0, 0.0), "trim")

    # `HOUSE-00463`: the rafters and the purlins, under the two long planes -- unless the cells
    # under this roof draw them themselves (`HOUSE-00488`). They are only ever seen from INSIDE
    # the attic, and this file belongs to the outdoors: from `L3_ROOM` the cell it is filed in is
    # not visible and 4 456 pixels of rafter went with it. Moved rather than copied, so nothing is
    # drawn twice.
    if not structure_by_cells:
        for corners, outward in rafter_faces(outer, eaves_y, pitch):
            add(corners, outward, "structure")

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
                     (value, box_of[3], box_of[5]), (value, box_of[2], box_of[5])], outward,
                    "metal")
            for value, outward in ((box_of[2], (0.0, -1.0, 0.0)), (box_of[3], (0.0, 1.0, 0.0))):
                add([(box_of[0], value, box_of[4]), (box_of[1], value, box_of[4]),
                     (box_of[1], value, box_of[5]), (box_of[0], value, box_of[5])], outward,
                    "metal")
    # `HOUSE-00776`: the pipes, from `roof_geometry.house_downspouts` rather than from a loop over
    # this roof's four corners. Two of the eight corners are UNDER the other roof -- the garage
    # wing projects from the house's east wall -- so a pipe at each of them is a pipe indoors, and
    # the water it carries lands in the dining room. Each one also runs to the GROUND under it
    # rather than to +0.00, which is 0.13 m short at the north corners where the lot has fallen.
    half_spout = DOWNSPOUT_SECTION / 2.0
    for spout in spouts or ():
        corner_x, corner_z, foot = spout["x"], spout["z"], spout.get("groundY", 0.0)
        for value, outward in ((corner_x - half_spout, (-1.0, 0.0, 0.0)),
                               (corner_x + half_spout, (1.0, 0.0, 0.0))):
            add([(value, foot, corner_z - half_spout), (value, gutter_y, corner_z - half_spout),
                 (value, gutter_y, corner_z + half_spout), (value, foot, corner_z + half_spout)],
                outward, "metal")
    # §12.1's ridge vent (`HOUSE-00468`, rebuilt by `HOUSE-00491`): the exhaust half of the attic
    # ventilation that replaced the two impossible gable louvres. `metal`, like the gutters and
    # downspouts it shares a class with, and a BOX straddling the ridge rather than the flat lid
    # this drew before -- a cap over a cut slot is what a shingle-over ridge vent is, and a lid
    # floating 0.08 m above the apex was a lid floating above the apex.
    for corners, outward in roof_geometry.ridge_vent(outer, eaves_y, pitch):
        add(corners, outward, "metal")

    mesh = bpy.data.meshes.new(f"{name}_mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.validate()
    mesh.update()
    material_slots(mesh, ROOF_MATERIALS, material_definitions)
    for polygon, klass in zip(mesh.polygons, classes):
        polygon.material_index = SURFACE_ORDER.index(klass)
    weld(mesh)          # before the UVs: welding moves loops, and a face keeps its material
    planar_uvs(mesh)
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


def roof_boxes(layout: dict, levels: dict) -> dict:
    """`{name: centre-line rectangle}` for every roof, from `roof_geometry` (`HOUSE-00472`)."""
    del levels
    return roof_geometry.roof_boxes(layout)


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
    all_portals = list(portal_rows.values())
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
        # The UNION of every position accessor. Since `HOUSE-00470` the exporter splits a cell
        # into one primitive per material, so the first accessor is the first surface class and
        # not the cell -- a claim that read it alone would be a claim about the floor.
        bounds = None
        for accessor in document.get("accessors", []):
            if accessor.get("type") != "VEC3" or "min" not in accessor:
                continue
            if bounds is None:
                bounds = ([float(v) for v in accessor["min"]],
                          [float(v) for v in accessor["max"]])
                continue
            bounds = ([min(a, float(b)) for a, b in zip(bounds[0], accessor["min"])],
                      [max(a, float(b)) for a, b in zip(bounds[1], accessor["max"])])
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
    fridge = cells["CELL_FRIDGE_INTERIOR"]
    fridge_extent = layout_io.cell_extent(fridge, levels[fridge["level"]])
    fridge_box = list(cell_boxes(fridge, fridge_extent))[0]
    require(enclosed_by_parent(fridge, fridge_box, cells, levels),
            "the canonical refrigerator is fully enclosed by its parent kitchen")
    require(not enclosed_by_parent(subject, kitchen_box, cells, levels),
            "a normal kitchen cell must retain its genuine outside wall")
    generated_fridge = generate(SOURCE, output, wanted={"CELL_FRIDGE_INTERIOR"})
    require(generated_fridge["written"] == ["CELL_FRIDGE_INTERIOR"] and
            not generated_fridge["problems"],
            f"the nested refrigerator shell exports ({generated_fridge})")
    fridge_document, fridge_error = gltf_validate.read_gltf_json(
        output / "CELL_FRIDGE_INTERIOR.glb")
    require(fridge_document is not None,
            f"the nested refrigerator export is valid glTF ({fridge_error})")
    if fridge_document is not None:
        fridge_positions = [accessor for accessor in fridge_document.get("accessors", [])
                            if accessor.get("type") == "VEC3" and "max" in accessor]
        require(bool(fridge_positions) and
                max(float(accessor["max"][1]) for accessor in fridge_positions) <=
                fridge_extent[1] + 0.03,
                "the nested cell has no exterior skin extended above its own 2.45 m ceiling "
                "(allowing its 20 mm door-head trim)")
    for nested_name in ("CELL_FREEZER_INTERIOR", "CELL_FRIDGE_INTERIOR", "L0_GARAGE_LOFT"):
        child = cells[nested_name]
        child_extent = layout_io.cell_extent(child, levels[child["level"]])
        require(all(enclosed_by_parent(child, box, cells, levels)
                    for box in cell_boxes(child, child_extent)),
                f"{nested_name} is wholly enclosed by its authored parent")
        reset_scene()
        child_mesh = build_cell(child, child_extent, neighbours=neighbours,
                                construction=construction, level=levels[child["level"]],
                                levels=levels, portals=all_portals,
                                openings=openings_by_portal, cells_by_id=cells)
        outside_faces = sum(polygon.material_index == SURFACE_ORDER.index("exterior")
                            for polygon in child_mesh.data.polygons)
        require(outside_faces == 0,
                f"{nested_name} emits no weather-facing outer-skin polygon ({outside_faces})")
    # ...and that fourth side is MIXED, which is the case a single wall per side gets wrong.
    north = side_intervals("-Z", kitchen_box, subject, neighbours)
    require(len(north) == 2 and {wall for _lo, _hi, wall, _covers in north}
            == {"wallPartition", "wallExterior"},
            f"its north side is 8.9 m of partition against the sunroom and 1.5 m of exterior "
            f"wall beside it ({[(round(a, 2), round(b, 2), w) for a, b, w, _c in north]})")
    require(abs(sum(hi - lo for lo, hi, _w, _c in north)
                - (kitchen_box[1] - kitchen_box[0])) < 1e-6,
            "and the runs cover the side exactly once, with no gap and no overlap")
    family = cells["L0_FAMILY"]
    family_box = list(cell_boxes(family, extent_of(family, levels[family["level"]])[0]))[0]
    require(wall_across("+X", family_box, family, neighbours, construction) == "wallExterior",
            "the family room's east side has nothing across it, so it is an exterior wall")
    family_north = side_intervals("-Z", family_box, family, neighbours)
    require(any(wall == "wallPartition" for _lo, _hi, wall, _c in family_north)
            and any(wall == "wallExterior" for _lo, _hi, wall, _c in family_north),
            f"and its north side clips the corner of the sunroom for half a metre and is the "
            f"outside wall for the other six "
            f"({[(round(a, 2), round(b, 2), w) for a, b, w, _c in family_north]})")
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
    # `HOUSE-00485`: a second face where the WEATHER is on the other side, which is not the same
    # question as "the wall is not a partition" -- a garage boundary is neither.
    outside_runs = [run for _side, run in all_runs if not run[3]]
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
        for lo, hi, _wall, _covers in side_intervals(side, kitchen_box, subject, neighbours):
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
    # `HOUSE-00486`: and EVERY door adds its leaf plus the lining that closes the gap round it --
    # one box per side that has a gap, so a leaf exactly as wide as its hole adds only the leaf.
    # Both rooms build one, in their own half of the reveal, because a leaf built once belongs to
    # one cell's chunk and §25 culls the room behind a shut door. Counted from the leaf's own
    # dimensions rather than assumed to be four, because §12's doors are 0.86 x 2.05 in a
    # 0.90 x 2.10 hole and a later one might not be.
    for hole in kitchen_holes:
        row = openings_by_portal.get(hole[4]) or {}
        if row.get("kind") != "door":
            continue
        leaf = row.get("leaf") or {}
        width = min(float(leaf.get("width") or (hole[1] - hole[0])), hole[1] - hole[0])
        height = min(float(leaf.get("height") or (hole[3] - hole[2])), hole[3] - hole[2])
        boxes_expected += 1
        boxes_expected += 2 if (hole[1] - hole[0]) - width > 2e-9 else 0
        boxes_expected += 1 if (hole[3] - hole[2]) - height > 1e-9 else 0
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
    styled = [row for row in openings_by_portal.values()
              if row.get("muntinPattern") == "six_over_six"]
    require(len(styled) == 17 and all(row.get("shutterMaterial")
                                      == "MAT_WINDOW_SHUTTER_BLACK" for row in styled),
            f"§12.1's seventeen front double-hungs author one complete grille/shutter treatment "
            f"({len(styled)})")
    require(all(portal_rows[row["portal"]]["plane"]["axis"] == "z"
                and abs(float(portal_rows[row["portal"]]["plane"]["value"]) + 14.30) < 1e-9
                and str(row.get("type")).startswith("W_DH_") for row in styled),
            "and only front-elevation W_DH_* rows select it; side/rear/special windows stay plain")
    stair_cell = cells["L0_STAIR_MAIN"]
    stair_extent = extent_of(stair_cell, levels[stair_cell["level"]])[0]
    reset_scene()
    styled_window = build_cell(stair_cell, stair_extent, neighbours=neighbours,
                               construction=construction, level=levels[stair_cell["level"]],
                               levels=levels, portals=list(portal_rows.values()),
                               openings=openings_by_portal, cells_by_id=cells)
    styled_classes = [SURFACE_ORDER[polygon.material_index]
                      for polygon in styled_window.data.polygons]
    require(styled_classes.count("window_frame") == 90,
            f"one standard 6-over-6 has the original nine frame/sash boxes plus six muntin "
            f"boxes ({styled_classes.count('window_frame')} faces)")
    require(styled_classes.count("window_shutter") == 276,
            f"and its paired louvered shutters have two stiles, three rails and eighteen "
            f"recessed slats per leaf ({styled_classes.count('window_shutter')} faces)")
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
    steps = flight_steps(main, 0.60)
    require(len(steps) == int(main["risers"]),
            f"a flight has one step per riser ({len(steps)} of {main['risers']})")
    require(abs(steps[-1]["y1"] - float(levels["L1"]["ffl"])) < 1e-6,
            f"and its last tread IS the floor above, {steps[-1]['y1']:.4f} against "
            f"{levels['L1']['ffl']}")
    require(abs(steps[0]["y1"] - (0.60 + float(main["rise"]))) < 1e-9,
            "and its first is one rise off the floor below")
    lanes = {tread["lane"] for tread in steps}
    require(lanes == {0, 1},
            f"and a U-stair has two runs, one each side of its own landing ({sorted(lanes)})")
    require({tread["lane"] for tread in flight_steps(
        flight_rows["STAIR_ATTIC_L2_L3"], 6.55)} == {0},
        "while a straight flight has one")

    # `HOUSE-00472`: the two runs of a `u` climb TOWARDS each other. This generator used to lay the
    # second one beyond the landing and climb back to it, so the top tread finished against the
    # half-landing and you reached L1 in one stride from +2.2147.
    first_run = [tread for tread in steps if tread["run"] == 0]
    second_run = [tread for tread in steps if tread["run"] == 1]
    require(first_run[0]["up"] == -second_run[0]["up"],
            f"the two runs of a `u` climb in opposite directions "
            f"({first_run[0]['up']}, {second_run[0]['up']})")
    landing_y = 0.60 + 9 * float(main["rise"])
    foot = second_run[0]["box"][2 if second_run[0]["up"] > 0 else 3]
    require(abs(second_run[0]["y1"] - landing_y - float(main["rise"])) < 1e-9,
            f"the second run's FIRST tread is one riser above the half-landing at "
            f"+{landing_y:.4f} ({second_run[0]['y1']:.4f})")
    require(abs(foot - -17.92) < 1e-6,
            f"and it starts at the landing's far edge, z = -17.92 ({foot:.3f})")

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
    # A nosing overhangs the tread BELOW it, so it must sit outside its own tread on the side the
    # run descends. Winding this the wrong way round for the second run of a `u` puts the nosing
    # inside the step, where it is invisible and does nothing.
    treads_only = {(round(t["box"][0], 6), round(t["box"][1], 6),
                    round(t["box"][2], 6), round(t["box"][3], 6)) for t in steps}
    nosings = [box for box in boxes if abs((box[3] - box[2]) - NOSING_THICK) < 1e-9]
    require(len(nosings) == int(main["risers"]),
            f"there is a nosing over every riser ({len(nosings)})")
    overhangs = sum(1 for bx0, bx1, _by0, _by1, bz0, bz1 in nosings
                    if (round(bx0, 6), round(bx1, 6), round(bz0, 6), round(bz1, 6))
                    not in treads_only)
    require(overhangs == int(main["risers"]),
            f"and every one of them stands proud of its own tread rather than inside it "
            f"({overhangs})")

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

    # ---- `HOUSE-00907`: authored shell materials ------------------------------------------------
    reset_scene()
    painted = build_cell(subject, extent, neighbours=neighbours, construction=construction,
                         level=levels[subject["level"]], levels=levels, portals=all_portals,
                         openings=openings_by_portal, cells_by_id=cells)
    used = {SURFACE_ORDER[polygon.material_index] for polygon in painted.data.polygons}
    require(len(painted.data.materials) == len(SURFACE_ORDER),
            f"a cell carries one material per surface class ({len(painted.data.materials)})")
    require({"floor", "ceiling", "wall", "exterior", "trim", "glass"} <= used,
            f"and the kitchen uses the six classes a room has ({sorted(used)})")
    require({"window_frame", "window_glass"} <= used,
            f"weather-facing frames and panes split from borrowed indoor detail ({sorted(used)})")
    assigned = cell_surface_materials(subject, openings_by_portal.values(), all_portals,
                                      cells_by_id=cells)
    require(assigned["floor"] == subject["floorMaterial"]
            and assigned["wall"] == subject["wallMaterial"]
            and assigned["ceiling"] == subject["ceilingMaterial"]
            and assigned["trim"] == subject["trimMaterial"],
            f"its four palette fields are the four generated finishes ({assigned})")
    require(assigned["glass"] == "MAT_GLASS_CLEAR"
            and assigned["window_glass"] == "MAT_WINDOW_GLASS_CLEAR"
            and assigned["exterior"] == "MAT_SIDING_WARM_WHITE",
            "and the opening schedule supplies both indoor and weather-facing glass")

    foyer = cells["L0_FOYER"]
    foyer_extent = extent_of(foyer, levels[foyer["level"]])[0]
    entry = build_cell(foyer, foyer_extent, neighbours=neighbours,
                       construction=construction, level=levels[foyer["level"]], levels=levels,
                       portals=all_portals, openings=openings_by_portal, cells_by_id=cells)
    entry_classes = [SURFACE_ORDER[polygon.material_index] for polygon in entry.data.polygons]
    require(entry_classes.count("exterior_door") == 6,
            "the weather-facing D_ENTRY leaf retains one closed six-face body")
    require(entry_classes.count("exterior_door_panel") == 192,
            f"the weather-facing D_ENTRY leaf has 32 two-faced panel moulding boxes "
            f"({entry_classes.count('exterior_door_panel')} faces)")
    require(entry_classes.count("exterior_door_hardware") == 42,
            f"its two handle sets, two deadbolts and one threshold cap are seven closed metal "
            f"boxes ({entry_classes.count('exterior_door_hardware')} faces)")
    entry_materials = cell_surface_materials(foyer, openings_by_portal.values(), all_portals,
                                             cells_by_id=cells)
    require(entry_materials["exterior_door"] == "MAT_EXTERIOR_DOOR_HARDWOOD",
            f"and the opening row supplies that role's material ({entry_materials['exterior_door']})")
    require(entry_materials["exterior_door_panel"]
            == "MAT_EXTERIOR_DOOR_PANEL_HARDWOOD",
            "and raised panels have a stable exterior-visible hardwood role")
    require(entry_materials["exterior_door_hardware"]
            == "MAT_EXTERIOR_DOOR_HARDWARE_BRONZE",
            "and entry hardware has a stable exterior-visible metal role")
    document, _error = gltf_validate.read_gltf_json(output / "L0_KITCHEN.glb")
    require(len(document.get("materials", [])) >= 6,
            f"the exported file carries them ({len(document.get('materials', []))})")
    exported_ids = {row.get("extras", {}).get("materialId")
                    for row in document.get("materials", [])}
    require(None not in exported_ids and not any(str(value).startswith("BLOCKOUT_")
                                                  for value in exported_ids),
            f"every exported primitive names a real material id ({sorted(exported_ids)})")

    # ---- `HOUSE-00471`: the albedo UVs ----------------------------------------------------------
    uv_layers = list(painted.data.uv_layers)
    require(len(uv_layers) == 1 and uv_layers[0].name == "UVMap",
            f"a cell has exactly one UV channel, and the second is the lightmap unwrapper's "
            f"({[layer.name for layer in uv_layers]})")
    # The BIGGEST up-facing face, which is the floor slab: a skirting board's top also looks up.
    floor_face = max((face for face in painted.data.polygons if face.normal.z > 0.99),
                     key=lambda face: face.area)
    corners = [tuple(round(value, 4) for value in
                     painted.data.uv_layers[0].data[loop].uv)
               for loop in floor_face.loop_indices]
    world = [tuple(round(value, 4) for value in
                   painted.data.vertices[painted.data.loops[loop].vertex_index].co[:2])
             for loop in floor_face.loop_indices]
    require(corners == world,
            f"and a floor's UVs ARE its world x and y, so a 1 m tile is 1 m everywhere "
            f"({corners[:2]} against {world[:2]})")
    spans = [max(value[axis] for value in corners) - min(value[axis] for value in corners)
             for axis in (0, 1)]
    require(min(spans) > 3.0,
            f"the kitchen's floor spans metres of UV, not a normalised 0..1 ({spans})")

    # ---- `HOUSE-00471`: the receivers are welded, so a wall is one surface ----------------------
    require(set(LIGHTMAP_RECEIVERS) == {"floor", "ceiling", "wall", "exterior"},
            f"the receivers are the room-scale classes, named once ({LIGHTMAP_RECEIVERS})")
    require(not (set(LIGHTMAP_RECEIVERS) & {"trim", "glass", "metal", "stair", "structure",
                                            "roof", "window_frame", "window_shutter", "window_glass",
                                            "exterior_door", "exterior_door_panel",
                                            "exterior_door_hardware"}),
            "and no detail class is one of them")
    receiver_indices = {SURFACE_ORDER.index(name) for name in LIGHTMAP_RECEIVERS}
    loose = 0
    for edge in painted.data.edges:
        users = [face for face in painted.data.polygons
                 if edge.key[0] in face.vertices and edge.key[1] in face.vertices
                 and face.material_index in receiver_indices]
        if len(users) > 1:
            loose += 1
    require(loose > 0,
            f"receiver faces SHARE edges after the weld, so a wall broken into strips round a "
            f"doorway is one connected surface and not three islands ({loose} shared edges)")
    detail_verts = {index for face in painted.data.polygons
                    if face.material_index not in receiver_indices
                    for index in face.vertices}
    require(detail_verts, "and the trim is still there, unwelded, with its own vertices")

    kitchen_material = painted.data.materials[SURFACE_ORDER.index("wall")]
    require(kitchen_material.get("lightmapReceiver") is True
            and kitchen_material.get("surfaceClass") == "wall",
            "a wall's material says so itself, so the unwrap reads the generator's decision "
            "rather than re-deriving it")
    require(painted.data.materials[SURFACE_ORDER.index("trim")].get("lightmapReceiver") is False,
            "and the trim's says it is not a receiver")
    exported, _err = gltf_validate.read_gltf_json(output / "L0_KITCHEN.glb")
    tagged = [material for material in exported.get("materials", [])
              if isinstance(material.get("extras"), dict)
              and "lightmapReceiver" in material["extras"]
              and "materialId" in material["extras"]]
    require(len(tagged) == len(exported.get("materials", [])),
            f"and receiver semantics plus the real id survive in `.glb` extras, on every material "
            f"({len(tagged)} of {len(exported.get('materials', []))})")

    # ---- `HOUSE-00469`: the basement window wells -----------------------------------------------
    wells_wanted = [row for row in openings_by_portal.values()
                    if str(row.get("type")) == "W_BASEMENT"]
    require(len(wells_wanted) == 8,
            f"§12.6's `W_BASEMENT` hoppers are the windows that need wells ({len(wells_wanted)})")
    well_portal = portal_rows[wells_wanted[0]["portal"]]
    well_cell = cells[well_portal["cellA"] if cells.get(well_portal["cellA"], {}).get("kind")
                      != "exterior" else well_portal["cellB"]]
    well_extent = extent_of(well_cell, levels[well_cell["level"]])[0]
    reset_scene()
    with_well = len(build_cell(well_cell, well_extent, neighbours=neighbours,
                               construction=construction, level=levels[well_cell["level"]],
                               levels=levels, portals=all_portals, openings=openings_by_portal,
                               cells_by_id=cells).data.polygons)
    # `W_PICTURE` rather than `W_DH_STD`: a double-hung would also gain a meeting rail, and the
    # difference has to be the well and nothing else.
    hoppers = {row["id"]: dict(row, type="W_PICTURE") for row in openings_by_portal.values()}
    reset_scene()
    without_well = len(build_cell(well_cell, well_extent, neighbours=neighbours,
                                  construction=construction, level=levels[well_cell["level"]],
                                  levels=levels, portals=all_portals,
                                  openings={key: hoppers[row["id"]] for key, row
                                            in openings_by_portal.items()},
                                  cells_by_id=cells).data.polygons)
    here = [row for row in wells_wanted
            if well_cell["id"] in (portal_rows[row["portal"]]["cellA"],
                                   portal_rows[row["portal"]]["cellB"])]
    require(with_well == without_well + 6 * 4 * len(here),
            f"{well_cell['id']}'s {len(here)} hopper(s) each stand in a well of four boxes — two "
            f"sides, an end and a floor ({without_well} -> {with_well})")
    # All four orientations, because a well on the north wall and one on the east wall go through
    # different halves of the same function -- and the first version of this claim only ever
    # exercised one of them, so an injected bug in the other went unnoticed.
    for well_side in ("-X", "+X", "-Z", "+Z"):
        shape = window_well(well_side, 10.0, 0.0, 0.9, -0.45)
        require(len(shape) == 4, f"a {well_side} well is four boxes ({len(shape)})")
        axis = 0 if well_side in ("-X", "+X") else 4
        outward = -1.0 if well_side in ("-X", "-Z") else 1.0
        beyond = (all(box[axis + 1] <= 10.0 + 1e-9 for box in shape) if outward < 0
                  else all(box[axis] >= 10.0 - 1e-9 for box in shape))
        require(beyond,
                f"and every one of them is OUTSIDE the {well_side} wall's outer face — a well "
                f"inside the wall is a hole in the basement "
                f"({[round(box[axis], 3) for box in shape]})")
        require(min(box[2] for box in shape) < -0.45 - WELL_BELOW_SILL + 1e-9,
                f"its floor is under the sill, so water has somewhere to go "
                f"({min(box[2] for box in shape)})")
        floor_top = -0.45 - WELL_BELOW_SILL
        upright = [box for box in shape if box[3] > floor_top + 1e-9]
        require(len(upright) == 3 and all(box[3] == GRADE_Y for box in upright),
                f"and all three of its walls come up to grade, not just one of them "
                f"({[round(box[3], 3) for box in upright]})")
    require(float(well_portal["rect"]["v"][0]) < GRADE_Y,
            f"a basement hopper's sill is below grade, which is why it needs one "
            f"({well_portal['rect']['v'][0]})")

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
    dormer_list = dormers_on(main_box, all_portals, openings_by_portal.values())
    require(len(dormer_list) == 5,
            f"§12.1's five dormers are the five `W_DORMER` windows ({len(dormer_list)})")
    subject_dormer = dormer_list[0]
    dormer_out = 1.0 if abs(subject_dormer[2] - main_box[3]) < abs(subject_dormer[2] - main_box[2]) \
        else -1.0
    faces = dormer_shell(subject_dormer[0], subject_dormer[1], subject_dormer[2],
                         outer, eaves_y, float(construction["roofPitch"]))
    require(len(faces) == 8,
            f"a dormer is a gable, two jambs and a header round its window, two cheeks and two "
            f"roof planes ({len(faces)})")
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
    # `HOUSE-00490`: the planes are CUT where each dormer comes through, so the count is the cut
    # planes plus the dormers' own faces, not the whole planes plus them. A roof left whole under
    # a dormer is a roof across the inside of its window.
    pitch_here = float(construction["roofPitch"])
    expected = (plain_roof_faces
                - len(roof_planes(outer, eaves_y, pitch_here))
                + len(roof_planes(outer, eaves_y, pitch_here, dormer_list))
                + sum(len(dormer_shell(one[0], one[1], one[2], outer, eaves_y, pitch_here))
                      for one in dormer_list))
    require(len(dormered.data.polygons) == expected,
            f"and the roof is its cut planes plus each dormer's own faces ({plain_roof_faces} -> "
            f"{len(dormered.data.polygons)}, expected {expected})")

    # ---- `HOUSE-00463`: the attic structure -----------------------------------------------------
    ridge_run = (outer[1] - outer[0]) - (outer[3] - outer[2])
    expected_rafters = 2 * (int(ridge_run / RAFTER_SPACING) + 1)
    require(len(dormered.data.polygons) == expected,
            "the roof's face count is the cut planes, the dormers, the eaves boards and the "
            "structure")
    # planes, fascia, soffit, four gutters of four faces, one closed six-face ridge-vent box --
    # and NO downspouts,
    # since `HOUSE-00776`: they are `roof_geometry.house_downspouts`'s to place, because two of the
    # eight corners are under the other roof, and a roof built without that list has none.
    bare = plain_roof_faces - (4 + 4 + 4 + 4 * 4 + 6)
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
    # `HOUSE-00496`: the same cell WITH the roof it is bounded by, which is how `generate` builds
    # it. §13.6's `yOverride` is maximum head-room, and drawn as a box this store stands 3.33 m
    # over a hip whose eaves are +10.57 at its own west wall -- the house was a flat-topped box
    # from the road with §12.1's roof inside it.
    reset_scene()
    attic_roof = roof_planes_for(levels["L3"], layout, construction)
    require(len(attic_roof) == 4,
            f"`L3` is bounded by a roof and its four planes are read from `roof_geometry` "
            f"({len(attic_roof)})")
    clipped = build_cell(store, extent_of(store, levels["L3"])[0], neighbours=neighbours,
                         construction=construction, level=levels["L3"], levels=levels,
                         cells_by_id=cells, roof=attic_roof)
    over = 0.0
    for polygon in clipped.data.polygons:
        if SURFACE_ORDER[polygon.material_index] == "trim":
            continue
        for index in polygon.vertices:
            point = clipped.data.vertices[index].co
            # Blender axes: `to_blender` negates z and swaps it with y.
            height = roof_geometry.roof_height(attic_roof, point.x, -point.y)
            if height is not None:
                over = max(over, point.z - height)
    require(over <= 0.001,
            f"and nothing it draws stands over that roof: worst {over:.3f} m")
    boxed = 0.0
    reset_scene()
    unclipped = build_cell(store, extent_of(store, levels["L3"])[0], neighbours=neighbours,
                           construction=construction, level=levels["L3"], levels=levels,
                           cells_by_id=cells)
    for polygon in unclipped.data.polygons:
        if SURFACE_ORDER[polygon.material_index] == "trim":
            continue
        for index in polygon.vertices:
            point = unclipped.data.vertices[index].co
            height = roof_geometry.roof_height(attic_roof, point.x, -point.y)
            if height is not None:
                boxed = max(boxed, point.z - height)
    require(boxed > 3.0,
            f"-- and the same cell built without it stands {boxed:.2f} m over, which is the "
            f"defect and not a tolerance")

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
    porch_cover_y = covering_floor(porch, porch_extent, cells)
    require(porch_cover_y is not None and abs(porch_cover_y - 3.65) < 1e-9
            and abs(porch_cover_y - porch_extent[1] - 0.30) < 1e-9,
            f"and the finish closes the authored 300 mm structure zone rather than guessing one "
            f"({porch_extent[1]} -> {porch_cover_y})")
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
    balcony_box = list(cell_boxes(balcony, balcony_extent))[0]
    open_sides = [side for side in ("-X", "+X", "-Z", "+Z")
                  if not any(covers for _lo, _hi, _wall, covers in side_intervals(
                      side, balcony_box, balcony, neighbours))]
    painted, rail_faces = [], []
    built = build_balcony_edge(balcony, balcony_extent, neighbours, construction,
                               lambda *args: painted.append(args),
                               lambda *args: rail_faces.append(args))
    expected_verticals = sum(2 + len(balcony_baluster_centres(
        *side_span(side, balcony_box)[1:])) for side in open_sides)
    require(built == len(open_sides) and len(painted) == len(open_sides) + expected_verticals
            and len(rail_faces) == 6 * len(open_sides),
            f"it gains a low rail, measured balusters, two newels and a top rail on each of its "
            f"{len(open_sides)} open sides ({len(painted)} painted boxes, "
            f"{len(rail_faces)} top-rail faces)")
    require(all(box[6] == "trim" for box in painted),
            "balcony lower rails, balusters and newels use painted trim while their upper rails "
            "retain metal")
    bottom_rails = [box for box in painted
                    if abs((box[3] - box[2]) - BALCONY_BOTTOM_RAIL_HEIGHT) < 1e-9]
    require(len(bottom_rails) == len(open_sides),
            f"one low rail anchors every open side instead of a solid half-height parapet "
            f"({len(bottom_rails)})")
    verticals = [box for box in painted if box not in bottom_rails]
    tops_of = {round(box[3], 4) for box in verticals}
    require(tops_of == {round(balcony_extent[0] + float(construction["railing"]) -
                              RAIL_SECTION / 2.0, 4),
                        round(balcony_extent[0] + float(construction["railing"]) +
                              RAIL_SECTION / 2.0, 4)},
            f"balusters meet the rail underside and newels support its full depth "
            f"({sorted(tops_of)})")
    clear_gaps = []
    for side in open_sides:
        _plane, lo, hi = side_span(side, balcony_box)
        clear_gaps.extend(balcony_clear_gaps(lo, hi))
    require(clear_gaps and max(clear_gaps) <= BALCONY_BALUSTER_MAX_CLEAR + 1e-9,
            f"no opening in the balustrade exceeds {BALCONY_BALUSTER_MAX_CLEAR * 1000:.0f} mm "
            f"(worst {max(clear_gaps) * 1000:.1f} mm)")
    rail_tops = {round(point[1], 4) for corners in rail_faces for point in corners[0]}
    require(max(rail_tops) > balcony_extent[0] + float(construction["railing"]),
            f"and the rail's top is over §12's authored railing centre height "
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

    # `HOUSE-00475`: and it gets no WALLS either. Every exterior cell was building a full set of
    # faces at its own `yOverride` height -- 20 m round each yard, 65 m round `EXT_WORLD`, which
    # is 400 m square -- so the first frame the blockout drew was the inside of that box. The wall
    # a yard abuts belongs to the house, which draws its own outer face.
    for identifier in ("EXT_FRONTYARD_W", "EXT_BACKYARD", "EXT_WORLD", "EXT_ROAD"):
        yard = cells[identifier]
        yard_extent = extent_of(yard, levels[yard["level"]])[0]
        reset_scene()
        built = build_cell(yard, yard_extent, neighbours=neighbours, construction=construction,
                           level=levels[yard["level"]], levels=levels,
                           portals=list(portal_rows.values()), openings=openings_by_portal,
                           cells_by_id=cells)
        used = {built.data.materials[polygon.material_index].get("surfaceClass")
                for polygon in built.data.polygons} if built.data.polygons else set()
        require("wall" not in used,
                f"{identifier} is open to the sky and has no walls ({sorted(used)})")
        if identifier == "EXT_WORLD":
            require(not used,
                    f"and the world cell is nothing at all -- no ground, no lid, no sides "
                    f"({sorted(used)})")

    # The house on the other side of that boundary still has its outer skin, or removing the
    # yard's walls would have removed the wall you can see from the yard.
    outer_cell = cells["L0_LIVING"]
    outer_extent = extent_of(outer_cell, levels[outer_cell["level"]])[0]
    reset_scene()
    outer_built = build_cell(outer_cell, outer_extent, neighbours=neighbours,
                             construction=construction, level=levels[outer_cell["level"]],
                             levels=levels, portals=list(portal_rows.values()),
                             openings=openings_by_portal, cells_by_id=cells)
    outer_used = {outer_built.data.materials[polygon.material_index].get("surfaceClass")
                  for polygon in outer_built.data.polygons}
    require("exterior" in outer_used and "wall" in outer_used,
            f"while the living room keeps both its inner wall and its outer skin ({sorted(outer_used)})")
    require(slab_here(porch, porch_extent, True) and not slab_here(porch, porch_extent, False),
            "the porch is a deck, so it has a floor and no ordinary exterior ceiling slab; "
            "its derived covered-deck soffit is separate")
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
    require(juliet_edges == 3,
            f"the juliet balcony gets its open balustrade from the same rule as the rear one "
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
    require(0.24 <= COLUMN_SHAFT_SECTION <= 0.30
            and COLUMN_BASE_SECTION > COLUMN_BASE_CAP_SECTION > COLUMN_SHAFT_SECTION
            and COLUMN_CAPITAL_SECTION > COLUMN_NECK_SECTION > COLUMN_SHAFT_SECTION,
            f"and each one has a grounded base, a 260 mm shaft, neck and capital "
            f"({COLUMN_BASE_SECTION}, {COLUMN_BASE_CAP_SECTION}, {COLUMN_SHAFT_SECTION}, "
            f"{COLUMN_NECK_SECTION}, {COLUMN_CAPITAL_SECTION})")

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
    porch_box = list(cell_boxes(porch, porch_extent))[0]
    porch_open_sides = open_sides_of(porch, porch_box, neighbours)
    # Five boxes dress each column; each open side has a beam, fascia and cornice; the continuous
    # soffit is one downward-facing quad. The four remaining six-face pieces are the rails either
    # side of the centred stair opening and along the two returns.
    finish_boxes = PORCH_COLUMNS * 5 + len(porch_open_sides) * 3
    require(porch_faces == bare_porch + 6 * finish_boxes + 1 + 6 * 4,
            f"and it gains {PORCH_COLUMNS} five-part columns, a continuous soffit, a beam/fascia/"
            f"cornice on each of its {len(porch_open_sides)} open sides and a balustrade broken "
            f"by the steps ({bare_porch} -> {porch_faces})")
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


#: The provenance record this generator writes beside its output (`HOUSE-00482`).
SHELL_MANIFEST = REPO / "docs" / "shell-manifest.json"
#: What the generator's own version IS: the bytes of every module that decides the geometry. Not a
#: number somebody has to remember to raise -- a number that cannot be wrong.
GENERATOR_SOURCES = ("tools/blender/house_shell_gen.py",
                     "tools/world/stair_geometry.py",
                     "tools/world/roof_geometry.py",
                     "tools/world/layout_io.py")


def generator_version() -> str:
    """A SHA-256 over the sources that decide the shell's geometry, in a fixed order."""
    accumulator = hashlib.sha256()
    for name in GENERATOR_SOURCES:
        accumulator.update(name.encode("utf-8"))
        accumulator.update(b"\0")
        accumulator.update((REPO / name).read_bytes())
    return f"sha256:{accumulator.hexdigest()}"


def world_hash(source: Path) -> str:
    """`world.manifest.json`'s hash of the layout this shell was generated from, or empty."""
    for path in (source / "world.manifest.json", REPO / "content" / "world" / "world.manifest.json"):
        if path.is_file():
            return str(layout_io.load_file(path, "manifest").get("worldHash", ""))
    return ""


def shell_manifest(source: Path, output: Path) -> dict:
    """The provenance of every file in @p output: what made it, from what, and what came out.

    §20.1's rule is "every file under `assets-src/` has a manifest row", and the shell is not under
    `assets-src/`: it is a build product, regenerated from the layout by a tool in this repository.
    What a row would have bought it -- provenance and a licence -- it gets here instead, in the
    form the question actually takes for a generated asset: WHICH generator, at WHICH version, over
    WHICH layout, producing WHICH bytes. `origin.kind` is `generated` for all of it and the licence
    is the project's, which is why neither is repeated 99 times.
    """
    return {
        "schema": "cna-house/shell-manifest/1",
        "generator": GENERATOR_SOURCES[0],
        "generatorVersion": generator_version(),
        "origin": {"kind": "generated", "licence": "MIT", "attribution": ""},
        "worldHash": world_hash(source),
        # The FULL digest, not `digest()`'s 64-bit prefix: that one exists to compare two runs of
        # this tool in the same minute, and this one is a provenance record that has to be worth
        # trusting later.
        "files": {path.name: "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in sorted(output.glob("*.glb"))},
    }


def write_shell_manifest(source: Path, output: Path, destination: Path, check: bool) -> int:
    """Writes @p destination, or compares it and reports. Returns a process exit status."""
    manifest = shell_manifest(source, output)
    if not manifest["files"]:
        print(f"house_shell_gen: no shell in {output} -- NOTHING WAS RECORDED.")
        return 0
    text = json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    if not check:
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(text, encoding="utf-8")
        print(f"house_shell_gen: {len(manifest['files'])} file(s) recorded in "
              f"{destination.relative_to(REPO) if destination.is_relative_to(REPO) else destination}")
        return 0
    if not destination.is_file():
        print(f"house_shell_gen: {destination} does not exist; run --manifest", file=sys.stderr)
        return 1
    if destination.read_text(encoding="utf-8") == text:
        print(f"house_shell_gen: {destination.name} matches the shell in {output}")
        return 0
    previous = json.loads(destination.read_text(encoding="utf-8"))
    if previous.get("generatorVersion") != manifest["generatorVersion"]:
        print("house_shell_gen: the generator has changed since the manifest was written",
              file=sys.stderr)
    if previous.get("worldHash") != manifest["worldHash"]:
        print("house_shell_gen: the layout has changed since the manifest was written",
              file=sys.stderr)
    for name in sorted(set(previous.get("files") or {}) | set(manifest["files"])):
        before = (previous.get("files") or {}).get(name)
        after = manifest["files"].get(name)
        if before != after:
            print(f"house_shell_gen: {name} {'is new' if before is None else ('has gone' if after is None else 'changed')}",
                  file=sys.stderr)
    return 1


def check_material_assignments(source: Path, output: Path) -> int:
    """Validate HOUSE-00907's data-driven mapping and, when present, the generated GLBs."""
    layout = layout_io.load_layout(
        source, kinds=["cells", "portals", "openings", "stairs", "materials"])
    cells = {row["id"]: row for row in layout_io.rows(layout, "cells")}
    portals = layout_io.rows(layout, "portals")
    openings = layout_io.rows(layout, "openings")
    flights = layout_io.rows(layout, "stairs")
    definitions = {row["id"]: row for row in layout_io.rows(layout, "materials")}
    expected = {
        cell_id: cell_surface_materials(cell, openings, portals, flights, cells)
        for cell_id, cell in cells.items()
    }
    problems: list[str] = []
    for cell_id, assignments in expected.items():
        for klass, material_id in assignments.items():
            if material_id not in definitions:
                problems.append(f"{cell_id}/{klass}: unknown material {material_id}")
            if material_id.startswith("BLOCKOUT_"):
                problems.append(f"{cell_id}/{klass}: placeholder {material_id} remains")

    checked_files = 0
    if output.is_dir():
        for path in sorted(output.glob("*.glb")):
            document, error = gltf_validate.read_gltf_json(path)
            if document is None:
                problems.append(f"{path.name}: {error}")
                continue
            checked_files += 1
            if path.stem == "CHIMNEY":
                wanted = dict(SHELL_MATERIALS, exterior="MAT_BRICK_WATER_TABLE")
            elif path.stem.startswith("ROOF_"):
                wanted = ROOF_MATERIALS
            else:
                wanted = expected.get(path.stem)
            if wanted is None:
                problems.append(f"{path.name}: no cell or fixed shell role owns this file")
                continue
            for material in document.get("materials", []):
                extras = material.get("extras") or {}
                klass = extras.get("surfaceClass")
                material_id = extras.get("materialId")
                if "BLOCKOUT_" in str(material.get("name")):
                    problems.append(f"{path.name}: placeholder slot {material.get('name')}")
                if klass not in SURFACE_ORDER:
                    problems.append(f"{path.name}: unknown surface class {klass!r}")
                elif material_id != wanted[klass]:
                    problems.append(
                        f"{path.name}/{klass}: generated {material_id!r}, expected {wanted[klass]!r}")

    for problem in problems:
        print(f"house_shell_gen: {problem}", file=sys.stderr)
    print(f"house_shell_gen: {len(cells)} cell material maps and {checked_files} generated "
          f"file(s) checked; {len(definitions)} authored materials")
    return 1 if problems else 0


def determinism(source: Path, reference: Path, scratch: Path) -> int:
    """`HOUSE-00481`: generate again and compare every byte with the tree at @p reference.

    Not a hash of a hash: every `.glb` is compared on its own, and the report names the files that
    differ rather than saying "the tree changed". A generator that is non-deterministic is usually
    non-deterministic in ONE place -- a set iterated by address, a dict ordered by insertion, a
    float summed in a different order -- and knowing which cell moved is most of finding it.

    §18.4 wants byte-identical output because the content build is incremental and the asset
    manifest hashes what it produced: a generator that writes different bytes for the same input
    invalidates every downstream step on every run, and no cache can help it.
    """
    if not reference.is_dir():
        print(f"house_shell_gen: nothing to compare with at {reference}", file=sys.stderr)
        return 1
    report = generate(source, scratch, None)
    if report["problems"]:
        for problem in report["problems"]:
            print(f"house_shell_gen: {problem}", file=sys.stderr)
        return 1

    before = {path.name: digest(path) for path in sorted(reference.glob("*.glb"))}
    after = {path.name: digest(path) for path in sorted(scratch.glob("*.glb"))}
    missing = sorted(set(before) - set(after))
    added = sorted(set(after) - set(before))
    differing = sorted(name for name in set(before) & set(after)
                       if before[name] != after[name])
    print(f"house_shell_gen: {len(after)} file(s) regenerated, {len(differing)} differ, "
          f"{len(missing)} missing, {len(added)} new")
    for name in missing + added + differing:
        print(f"house_shell_gen: {name} is not byte-identical", file=sys.stderr)
    return 1 if (missing or added or differing) else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--source", type=Path, default=SOURCE)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--cells", default="", help="comma-separated cell ids; default is all")
    parser.add_argument("--check", type=Path, default=None,
                        help="regenerate and compare every file with the tree at this path "
                             "(HOUSE-00481); writes nothing to it")
    parser.add_argument("--manifest", action="store_true",
                        help="write docs/shell-manifest.json from the shell in --output")
    parser.add_argument("--check-manifest", action="store_true",
                        help="compare docs/shell-manifest.json with the shell in --output")
    parser.add_argument("--check-materials", action="store_true",
                        help="validate real shell assignments against world material data")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(blender_env_argv())

    if args.manifest or args.check_manifest:
        return write_shell_manifest(args.source, args.output, SHELL_MANIFEST,
                                    args.check_manifest)
    if args.check_materials:
        return check_material_assignments(args.source, args.output)

    if args.selftest:
        return selftest(args.output / "selftest")
    if args.check is not None:
        return determinism(args.source, args.check, args.output)
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
