#!/usr/bin/env python3
"""build_collision.py -- the layout and the `_COL` proxies become `content/world/collision.bin`.

`HOUSE-00210`. `cna-house.md` §49.2 specifies what the runtime reads: per cell a list of OBBs and a
short list of triangle meshes, indexed by a 1 m loose grid so a capsule sweep tests about six
shapes rather than nine hundred. `docs/collision-format.md` is the normative description of the
bytes; this is the only thing that writes them.

    tools/world/build_collision.py --world assets-src/world --out content/world/collision.bin
    tools/world/build_collision.py --world assets-src/world --report
    tools/world/build_collision.py --selftest

## The house is authored as rooms, and collision needs walls

The layout has no walls in it. `layout.cells.json` gives each room's *interior* boxes and
`layout.levels.json` gives the construction thicknesses; the wall between two rooms is implied by
the two rooms abutting, and this tool is where the implication is made concrete. Four rules do it,
and each of the four is a decision rather than an obvious step:

1. **A wall is centred on the boundary plane, not butted against it.** `world-format.md`'s
   validator rule 4 requires a portal rectangle to lie in *both* cells' boundary planes, so two
   abutting cells share one plane exactly, with no gap authored between them for a wall to sit in.
   A wall of thickness *t* on that plane therefore takes *t*/2 from each room. The alternative --
   growing the wall outward from one room -- would put it inside the neighbour, and which room got
   the intrusion would depend on the order the cells happen to appear in the file.

2. **A side is segmented by who is on the other side of it**, then given that segment's thickness.
   A living-room wall that is partition for four metres and exterior for two is two OBBs of
   different thickness, not one averaged one. `wallGarage` wins over `wallPartition` when either
   cell is a garage, and `foundationWall` replaces `wallExterior` below grade.

3. **A wall shared by two boxes of the SAME cell does not exist.** An L-shaped room is authored as
   several boxes (`world-format.md`), and the boundary between them is the middle of the room. This
   is the one that fails silently and badly: the room looks right, the lightmap bakes, and the
   player walks into an invisible wall across the lounge. The same subtraction that punches a
   doorway punches this, so there is one mechanism and not two.

4. **Portals are holes, and a hole in a rectangle is up to four rectangles.** A doorway that
   reaches the floor leaves a jamb each side and a header above -- three pieces. A window leaves
   four, because it also has a sill below. `subtract_rects` decomposes by coordinate compression
   and then merges greedily, so the count is the small one and not one rectangle per grid cell.

## Shapes are shared, not copied per cell

The naive file gives every cell its own shapes, and then a partition wall is written twice --
once for the room on each side -- because both rooms must collide with it. Storing a global pool
and letting cells reference it by index costs a `u32` per reference and saves an entire 31-byte
OBB on every shared surface. Measured on the selftest fixture the pool is 31 shapes against 38
references, so 7 of 38 copies never exist; §49.2's own estimate of ~4 300 OBBs is a count of
*shapes*, and only the pooled file means what that number says. `--report` prints the saving for a
real layout, where the shared surfaces are a far larger fraction than in a three-room fixture.

## Most proxies are boxes, and a box should not become a triangle mesh

`collision_proxy.py` writes three shapes: `box` (12 triangles), `hull` (a 14-DOP) and `boxes` (up
to five boxes joined into one mesh). Two of the three are boxes as far as collision is concerned,
and a swept capsule against an OBB is a handful of operations against a mesh's per-triangle loop.
So a `_COL` mesh is split into connected components and each component that *is* an axis-aligned
box in the asset's own space -- 8 distinct vertices, 12 triangles, spanning its own bounds -- is
emitted as an OBB carrying the prop's yaw. Only what is left becomes a triangle mesh. §49.2 says
"mostly OBBs, with triangle meshes kept for the curved minority" and this is what makes that true.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
REPO = TOOLS.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(TOOLS / "assets"))

import gltf_io  # noqa: E402
import terrain_gen  # noqa: E402
import layout_io  # noqa: E402
import roof_geometry  # noqa: E402
import stair_geometry  # noqa: E402
from layout_io import LayoutError  # noqa: E402

MAGIC = b"CCOL"
VERSION = 3

#: §49.2's loose grid is 1 m. Written into the file so the reader does not carry a second copy of
#: the constant that could disagree with this one.
GRID_CELL = 1.0

#: Shape kinds. The runtime uses them to answer "is this the ground?" without a surface lookup,
#: and the report uses them to say where the OBBs went.
KIND_FLOOR, KIND_CEILING, KIND_WALL, KIND_STAIR, KIND_PROP, KIND_EXTERIOR = range(6)
KIND_NAMES = ["floor", "ceiling", "wall", "stair", "prop", "exterior"]

#: A `u16` index into a cell's own shape list addresses this many shapes, and a `u16` vertex index
#: addresses this many vertices in one mesh. Both are asserted rather than assumed.
MAX_PER_CELL = 0xFFFF
MAX_MESH_VERTS = 0xFFFF

#: Coordinates are snapped to this before they are compared or hashed. Two walls that should be the
#: same wall are authored from two rooms and arrive as 3.6500000000000004 and 3.65; without a snap
#: the pool never dedupes them and the saving above is zero. 1e-6 m is a thousandth of a millimetre,
#: far below the 1 cm the validator itself works to.
EPS = 1e-6


def snap(value: float) -> float:
    """Round to `EPS` and normalise `-0.0`, so equal geometry hashes equally."""
    return round(value / EPS) * EPS + 0.0


# ============================================================================== rectangle algebra


def subtract_rects(
    rect: tuple[float, float, float, float],
    holes: list[tuple[float, float, float, float]],
) -> list[tuple[float, float, float, float]]:
    """`rect` minus `holes`, as a small set of disjoint rectangles covering exactly the remainder.

    Rectangles are `(u0, v0, u1, v1)` with u0 < u1 and v0 < v1. The decomposition is by coordinate
    compression -- every hole edge becomes a grid line, each grid box is kept or dropped by testing
    its centre -- followed by a greedy merge along u and then along v.

    The merge is what keeps the count honest. A doorway in a wall compresses to a 3x2 grid of which
    four boxes survive; unmerged that is four OBBs, merged it is the three a builder would name:
    the two jambs and the header. On a wall with several openings the difference compounds.
    """
    u0, v0, u1, v1 = rect
    if not (u0 < u1 and v0 < v1):
        return []
    clipped = []
    for hu0, hv0, hu1, hv1 in holes:
        cu0, cu1 = max(u0, hu0), min(u1, hu1)
        cv0, cv1 = max(v0, hv0), min(v1, hv1)
        if cu0 < cu1 - EPS and cv0 < cv1 - EPS:
            clipped.append((cu0, cv0, cu1, cv1))
    if not clipped:
        return [rect]

    us = sorted({u0, u1} | {c for h in clipped for c in (h[0], h[2])})
    vs = sorted({v0, v1} | {c for h in clipped for c in (h[1], h[3])})
    us = [u for u in us if u0 - EPS <= u <= u1 + EPS]
    vs = [v for v in vs if v0 - EPS <= v <= v1 + EPS]

    # Rows of maximal solid runs, one row per v band.
    rows: list[list[tuple[int, int]]] = []
    for j in range(len(vs) - 1):
        vc = 0.5 * (vs[j] + vs[j + 1])
        runs: list[tuple[int, int]] = []
        for i in range(len(us) - 1):
            uc = 0.5 * (us[i] + us[i + 1])
            solid = not any(h[0] < uc < h[2] and h[1] < vc < h[3] for h in clipped)
            if not solid:
                continue
            if runs and runs[-1][1] == i:
                runs[-1] = (runs[-1][0], i + 1)
            else:
                runs.append((i, i + 1))
        rows.append(runs)

    # Merge identical run sets across adjacent v bands, then emit.
    out: list[tuple[float, float, float, float]] = []
    j = 0
    while j < len(rows):
        k = j + 1
        while k < len(rows) and rows[k] == rows[j]:
            k += 1
        for a, b in rows[j]:
            out.append((us[a], vs[j], us[b], vs[k]))
        j = k
    return out


def overlap_1d(a0: float, a1: float, b0: float, b1: float) -> tuple[float, float] | None:
    """The shared interval of two 1-D ranges, or `None` when they only touch or miss."""
    lo, hi = max(a0, b0), min(a1, b1)
    return (lo, hi) if lo < hi - EPS else None


# ================================================================================== shape records


class Shapes:
    """The global pool. Identical geometry submitted twice is stored once and returns one index."""

    def __init__(self) -> None:
        self.obbs: list[tuple] = []
        self.meshes: list[dict] = []
        self.surfaces: list[str] = []
        self._obb_index: dict[tuple, int] = {}
        self._surface_index: dict[str, int] = {}
        self.duplicate_obbs = 0

    def surface(self, name: str | None) -> int:
        name = name or "default"
        if name not in self._surface_index:
            self._surface_index[name] = len(self.surfaces)
            self.surfaces.append(name)
        return self._surface_index[name]

    def obb(self, centre, half, yaw: float, surface: str | None, kind: int) -> int:
        key = (
            tuple(snap(c) for c in centre),
            tuple(snap(h) for h in half),
            snap(yaw),
            self.surface(surface),
            kind,
        )
        if key in self._obb_index:
            self.duplicate_obbs += 1
            return self._obb_index[key]
        self._obb_index[key] = len(self.obbs)
        self.obbs.append(key)
        return len(self.obbs) - 1

    def mesh(self, vertices, triangles, surface: str | None, kind: int) -> int:
        if len(vertices) > MAX_MESH_VERTS:
            raise LayoutError(
                f"a collision mesh has {len(vertices)} vertices; the format's u16 indices "
                f"address {MAX_MESH_VERTS}. Split the proxy."
            )
        self.meshes.append({
            "vertices": [tuple(snap(c) for c in v) for v in vertices],
            "triangles": [tuple(t) for t in triangles],
            "surface": self.surface(surface),
            "kind": kind,
        })
        return len(self.meshes) - 1


def obb_aabb(record: tuple) -> tuple[float, float, float, float, float, float]:
    """The world AABB of an OBB, honouring its yaw. Used by the grid and by the cell bounds."""
    (cx, cy, cz), (hx, hy, hz), yaw, _surface, _kind = record
    if abs(yaw) < EPS:
        ex, ez = hx, hz
    else:
        c, s = abs(math.cos(yaw)), abs(math.sin(yaw))
        ex, ez = hx * c + hz * s, hx * s + hz * c
    return (cx - ex, cy - hy, cz - ez, cx + ex, cy + hy, cz + ez)


def mesh_aabb(record: dict) -> tuple[float, float, float, float, float, float]:
    xs = [v[0] for v in record["vertices"]]
    ys = [v[1] for v in record["vertices"]]
    zs = [v[2] for v in record["vertices"]]
    return (min(xs), min(ys), min(zs), max(xs), max(ys), max(zs))


# =================================================================================== shell walls


def _side_planes(box: tuple[float, float, float, float]):
    """The four sides of a footprint box as `(axis, value, u0, u1, outward)`.

    `axis` is the axis the wall plane is perpendicular to, `u` runs along the wall in the other
    horizontal axis, and `outward` is +1 when the outside of the cell is at increasing `axis`.
    """
    x0, x1, z0, z1 = box
    return [
        ("x", x0, z0, z1, -1),
        ("x", x1, z0, z1, +1),
        ("z", z0, x0, x1, -1),
        ("z", z1, x0, x1, +1),
    ]


def _wall_thickness(construction: dict, cell: dict, neighbour: dict | None, level: dict) -> float:
    """Which of §12.2's five thicknesses this segment gets, and why."""
    if neighbour is None:
        below_grade = float(level.get("ffl", 0.0)) < 0.0
        key = "foundationWall" if below_grade else "wallExterior"
    elif "garage" in (cell.get("kind"), neighbour.get("kind")):
        key = "wallGarage"
    else:
        key = "wallPartition"
    value = construction.get(key)
    if value is None:
        raise LayoutError(f"layout.levels.json construction has no {key!r}")
    return float(value)


def _neighbour_segments(cell, box, side, cells_on_level, boxes_by_cell):
    """Split one side into `(u0, u1, neighbour_cell_or_None)` runs.

    A neighbour is a *different* cell whose box touches this plane from the other side. Boxes of
    the same cell are handled by the caller as openings, not as neighbours: a shared edge inside
    one room is not a wall at all, and calling it a partition would build one.
    """
    axis, value, u0, u1, outward = side
    cuts: list[tuple[float, float, dict]] = []
    for other in cells_on_level:
        if other["id"] == cell["id"]:
            continue
        for ox0, ox1, oz0, oz1 in boxes_by_cell[other["id"]]:
            # The neighbour is on the far side of the plane, so the face of ITS box that touches
            # this plane is the one nearest us: its low face when we look outward along +axis.
            if axis == "x":
                near = ox0 if outward > 0 else ox1
                span = overlap_1d(u0, u1, oz0, oz1)
            else:
                near = oz0 if outward > 0 else oz1
                span = overlap_1d(u0, u1, ox0, ox1)
            if abs(near - value) > 0.01:
                continue
            if span:
                cuts.append((span[0], span[1], other))

    edges = sorted({u0, u1} | {c for a, b, _ in cuts for c in (a, b)})
    out = []
    for i in range(len(edges) - 1):
        a, b = edges[i], edges[i + 1]
        if b - a <= EPS:
            continue
        mid = 0.5 * (a + b)
        owner = next((o for ca, cb, o in cuts if ca < mid < cb), None)
        if out and out[-1][2] is owner:
            out[-1] = (out[-1][0], b, owner)
        else:
            out.append((a, b, owner))
    return out


def _same_cell_openings(cell_id, box, side, boxes_by_cell, extent):
    """Where another box of the SAME cell abuts this side, there is no wall -- rule 3."""
    axis, value, u0, u1, outward = side
    y0, y1 = extent
    holes = []
    for ox0, ox1, oz0, oz1 in boxes_by_cell[cell_id]:
        if (ox0, ox1, oz0, oz1) == box:
            continue
        if axis == "x":
            near = ox0 if outward > 0 else ox1
            span = overlap_1d(u0, u1, oz0, oz1)
        else:
            near = oz0 if outward > 0 else oz1
            span = overlap_1d(u0, u1, ox0, ox1)
        if abs(near - value) > 0.01 or not span:
            continue
        holes.append((span[0], y0, span[1], y1))
    return holes


def _portal_holes(portals_on_plane, u0, u1, y0, y1):
    holes = []
    for portal in portals_on_plane:
        rect = portal["rect"]
        pu0, pu1 = float(rect["u"][0]), float(rect["u"][1])
        pv0, pv1 = float(rect["v"][0]), float(rect["v"][1])
        if overlap_1d(u0, u1, pu0, pu1) and overlap_1d(y0, y1, pv0, pv1):
            holes.append((pu0, pv0, pu1, pv1))
    return holes


def box_rect(box) -> tuple[float, float, float, float]:
    """A cell box `(x0, x1, z0, z1)` as `subtract_rects`' `(u0, v0, u1, v1)` with u = x, v = z.

    The same convention §15.4 gives a `y` portal -- *"on `Y`, `u` is world X and `v` is world Z"* --
    so a hole and the slab it is cut from are in one coordinate system and neither needs swapping.
    """
    x0, x1, z0, z1 = box
    return (x0, z0, x1, z1)


def _step_off_rects(layout, cells, levels, portals, cell_id: str, plane_y: float):
    """Where a flight arriving at @p plane_y lets you step OFF it. Floor, not well.

    A stair well is authored as a `y` portal, and the rect is the VISIBILITY opening: generous, and
    on the main stair 0.88 m longer than the flight. Cut whole out of the floor it leaves a gap
    between the last tread and the floor -- the player climbs seventeen steps and finds nothing to
    stand on (`HOUSE-00620` measured it: run 2 tops out at z -15.68 and the floor resumed at
    -14.80). What a builder does is run the trimmer to the last tread, so the strip beyond it, in
    the direction of travel and across the run's own width, is floor.
    """
    out = []
    for flight in layout_io.rows(layout, "stairs"):
        if flight.get("toCell") != cell_id:
            continue
        base = stair_geometry.foot_of(flight, cells, levels)
        top = base + int(flight["risers"]) * float(flight["rise"])
        if abs(top - plane_y) > 0.05:
            continue
        treads = stair_geometry.flight_steps(flight, base, portals)
        if not treads:
            continue
        last = treads[-1]
        x0, x1, z0, z1 = last["box"]
        # Forward is the way the flight rises, along its own axis.
        reach = 3.0  # further than any well in this house is long; the hole clips it
        if last["axis"] == "z":
            out.append((x0, z1, x1, z1 + reach) if last["up"] > 0 else (x0, z0 - reach, x1, z0))
        else:
            out.append((x1, z0, x1 + reach, z1) if last["up"] > 0 else (x0 - reach, z0, x0, z1))
    return out


def _slab_holes(portals_by_plane, cell_id: str, plane_y: float, step_off=()):
    """The `y` portals of @p cell_id on the horizontal plane @p plane_y, as hole rects.

    Minus @p step_off: the floor at the head of each arriving flight, which is the difference
    between a stair well and a hole a player falls into.
    """
    holes = []
    for portal in portals_by_plane.get(("y", snap(plane_y)), []):
        if cell_id not in (portal.get("cellA"), portal.get("cellB")):
            continue
        rect = portal.get("rect") or {}
        u, v = rect.get("u"), rect.get("v")
        if not u or not v:
            continue
        whole = (float(u[0]), float(v[0]), float(u[1]), float(v[1]))
        holes.extend(subtract_rects(whole, list(step_off)) if step_off else [whole])
    return holes


def cells_on_level_outside(cell, cells_on_level, boxes_by_cell, axis, value, outward, span):
    """Exterior cells whose box faces @p value from the far side, within a wall's thickness.

    A house's outer wall separates a room from the garden, and §15 leaves the wall's own footprint
    between their boxes -- so `_neighbour_segments` sees no neighbour and the wall goes into the
    room's list alone. This is the other half of it: whichever yard, terrace or lawn is on the
    outside gets the same shape, so a body walking there meets the house.
    """
    out = []
    for other in cells_on_level:
        if other["id"] == cell["id"] or other.get("kind") != "exterior":
            continue
        for ox0, ox1, oz0, oz1 in boxes_by_cell[other["id"]]:
            if axis == "x":
                near = ox0 if outward > 0 else ox1
                overlap = overlap_1d(span[0], span[1], oz0, oz1)
            else:
                near = oz0 if outward > 0 else oz1
                overlap = overlap_1d(span[0], span[1], ox0, ox1)
            # Within half a metre: the widest wall in §12.3 is 0.30, and a yard authored a
            # hand's breadth further out is still the yard on the other side of this wall.
            if abs(near - value) <= 0.5 and overlap:
                out.append(other["id"])
                break
    return out


def build_shell(layout, shapes: Shapes, stats: dict) -> dict[str, list[int]]:
    """Floors, ceilings and walls for every cell. Returns cell id -> shape indices."""
    levels = layout_io.by_id(layout_io.rows(layout, "levels"), "level")
    construction = layout["levels"].get("construction", {})
    cells = layout_io.rows(layout, "cells")
    cells_by_level: dict[str, list[dict]] = {}
    boxes_by_cell: dict[str, list] = {}
    for cell in cells:
        boxes_by_cell[cell["id"]] = layout_io.cell_boxes(cell)
        cells_by_level.setdefault(cell["level"], []).append(cell)

    portals = layout_io.rows(layout, "portals")
    cells_by_id = layout_io.by_id(cells, "cell")
    portals_by_plane: dict[tuple[str, float], list[dict]] = {}
    for portal in portals:
        plane = portal.get("plane") or {}
        key = (plane.get("axis"), snap(float(plane.get("value", 0.0))))
        portals_by_plane.setdefault(key, []).append(portal)

    grade = ground_storey(layout)
    level_ffl = {row["id"]: float(row.get("ffl", 0.0)) for row in layout_io.rows(layout, "levels")}

    def on_the_ground(row: dict) -> bool:
        """An open exterior cell standing on §11.5's ground rather than on a storey of the house."""
        return open_air(row) and abs(level_ffl.get(row.get("level"), 0.0) - grade) < 1e-6

    out: dict[str, list[int]] = {}
    for cell in sorted(cells, key=lambda c: c["id"]):
        level = levels.get(cell["level"])
        if level is None:
            raise LayoutError(f"cell {cell['id']!r} names level {cell['level']!r}, which does not exist")
        y0, y1 = layout_io.cell_extent(cell, level)
        depth = float(level.get("structureDepth", construction.get("wallPartition", 0.15)))
        floor_surface = cell.get("footstepSurface")
        indices: list[int] = []

        # An OPEN cell -- a terrace, a porch, a garden region -- has a floor and no lid, and no
        # wall on the sides that face outdoors. `world-format.md` says so in as many words
        # (`visibilityHint: open` is "no walls, e.g. exterior"), and building one anyway roofs the
        # terrace over: the rain stops above it (`HOUSE-00212`) and no ray from it reaches the sky
        # (`HOUSE-00213`). It still gets the wall it shares with the house, because that wall is
        # real and the room on the other side needs it too.
        is_open = cell.get("kind") == "exterior" or cell.get("visibilityHint") == "open"
        # ...but only an EXTERIOR cell loses its walls. `visibilityHint: open` on an interior cell
        # means open to the STAIRWELL -- §16's three stair cells and two landings carry it -- and
        # treating that as "no walls" left the house with a 1.30 m hole in its front elevation at
        # every storey of the main stair, where the only thing on the other side is the garden.
        # `HOUSE-00618`'s bot found it: at 16.7 minutes it walked in off the front lawn, under the
        # ground floor, and spent the rest of the run wedged against the slab's edge.
        outdoors = cell.get("kind") == "exterior"
        # A HOLE in a slab is a `y` portal on that slab's plane: §16.2's four stair wells and two
        # hatches, and nothing else. Until `HOUSE-00615` walked the flights this was not cut, so
        # `L0_STAIR_MAIN`'s floor lay across the top of the basement flight and `L1_STAIR_MAIN`'s
        # across the top of the main one -- every interior flight in the house arrived at a
        # ceiling, and the basement was unreachable on foot. The world already says where the
        # openings are; the collision simply was not reading it.
        floor_holes = _slab_holes(portals_by_plane, cell["id"], y0,
                                  _step_off_rects(layout, cells_by_id, levels, portals, cell["id"], y0))
        ceiling_holes = _slab_holes(portals_by_plane, cell["id"], y1,
                                    _step_off_rects(layout, cells_by_id, levels, portals, cell["id"], y1))
        # An open exterior cell ON THE GROUND STOREY has no floor of its own: §11.5's height
        # field is the ground it stands on, and §49.2 says exterior collision is *"the height
        # field plus OBBs"* (`HOUSE-00782`). A BALCONY is an exterior cell too and its floor is
        # 3.65 m over the lawn, which no height field carries -- `terrain_gen` draws the same line
        # for its pads, and taking a balcony's slab away drops a body through it into the garden. A slab as well as the field is two answers to "how high is the ground
        # here", and where they differed the slab won: `EXT_ORCHARD` declares `yOverride` 0.00 and
        # the lawn under it falls to -0.30, so the orchard stood on a 0.30 m plinth with a step
        # round it that §43.1's 0.22 m step-up could not climb. The orchard was unreachable, and
        # so was half the east side yard. A deck -- the terrace at +0.45, the porch at +0.57 -- is
        # in the height field too, as `terrain_gen`'s pad.
        for box in boxes_by_cell[cell["id"]]:
            x0, x1, z0, z1 = box
            for rx0, rz0, rx1, rz1 in ([] if on_the_ground(cell)
                                       else subtract_rects(box_rect(box), floor_holes)):
                indices.append(shapes.obb(
                    ((rx0 + rx1) / 2, y0 - depth / 2, (rz0 + rz1) / 2),
                    ((rx1 - rx0) / 2, depth / 2, (rz1 - rz0) / 2),
                    0.0, floor_surface, KIND_FLOOR))
                stats["floorPieces"] += 1
            if not is_open:
                for rx0, rz0, rx1, rz1 in subtract_rects(box_rect(box), ceiling_holes):
                    indices.append(shapes.obb(
                        ((rx0 + rx1) / 2, y1 + depth / 2, (rz0 + rz1) / 2),
                        ((rx1 - rx0) / 2, depth / 2, (rz1 - rz0) / 2),
                        0.0, cell.get("ceilingMaterial"), KIND_CEILING))
                    stats["ceilingPieces"] += 1

            for side in _side_planes(box):
                axis, value, su0, su1, outward = side
                openings = _same_cell_openings(cell["id"], box, side, boxes_by_cell, (y0, y1))
                on_plane = portals_by_plane.get((axis, snap(value)), [])
                for u0, u1, neighbour in _neighbour_segments(
                        cell, box, side, cells_by_level.get(cell["level"], []), boxes_by_cell):
                    if outdoors and neighbour is None:
                        # An open side a storey up is a drop, and §70.5 asks for a guard at more
                        # than a metre of it. The shell DRAWS one -- a 0.20 m parapet with a rail
                        # on top (`HOUSE-00465`) -- and nothing stopped you walking through it:
                        # this cell has no wall here by construction, so until `HOUSE-00472` you
                        # could step off the rear balcony at +3.65 and off the juliet at +6.55.
                        # The porch at +0.57 and the terrace at +0.45 get nothing, which is the
                        # same metre deciding it. `kind == "exterior"` because that is
                        # `build_balcony_edge`'s own first line: an INTERIOR cell marked
                        # `visibilityHint: open` -- a landing open to the stairwell -- has a drop
                        # too, and its guard is the rail round the well `HOUSE-00460` draws from
                        # the floor's hole rather than round the cell's boundary. That one is not
                        # built here and is recorded as a gap against Phase 7.
                        if cell.get("kind") == "exterior" and y0 > GUARD_DROP:
                            indices.append(_guard_obb(
                                shapes, axis, value, u0, u1, outward, y0,
                                float(construction.get("railing", 0.0)),
                                cell.get("wallMaterial")))
                            stats["guards"] += 1
                        continue
                    # Two open yards abut on grass, and until `HOUSE-00774` this built a WALL
                    # between them: 86 pieces, 5 219 m² of it, 29 over five metres tall, none of
                    # it drawn by anything. §49.2's exterior collision is the height field plus
                    # what stands on it, and `build_exterior` is what puts the fences there.
                    if open_air(cell) and neighbour is not None and open_air(neighbour):
                        stats["openBoundaries"] += 1
                        continue
                    thickness = _wall_thickness(construction, cell, neighbour, level)
                    holes = openings + _portal_holes(on_plane, u0, u1, y0, y1)
                    for ru0, rv0, ru1, rv1 in subtract_rects((u0, y0, u1, y1), holes):
                        centre_u, half_u = (ru0 + ru1) / 2, (ru1 - ru0) / 2
                        centre_v, half_v = (rv0 + rv1) / 2, (rv1 - rv0) / 2
                        if axis == "x":
                            centre = (value, centre_v, centre_u)
                            half = (thickness / 2, half_v, half_u)
                        else:
                            centre = (centre_u, centre_v, value)
                            half = (half_u, half_v, thickness / 2)
                        piece = shapes.obb(centre, half, 0.0, cell.get("wallMaterial"), KIND_WALL)
                        indices.append(piece)
                        stats["wallPieces"] += 1
                        if neighbour is None:
                            # The house's OUTER face, and the garden on the other side of it needs
                            # it too. §15's exterior cells stop 0.30 m short of the house -- the
                            # wall's own footprint is the gap -- so neither cell calls the other a
                            # neighbour and the wall lands in the house's list alone. A body
                            # walking the yard is swept against the YARD's shapes, so nothing
                            # stopped it until the cell tracker changed its mind, by which time it
                            # was inside the wall. `HOUSE-00618`'s bot walked in off the front lawn
                            # that way. A wall belongs to both sides of itself, and one of them is
                            # the outdoors.
                            for other in cells_on_level_outside(
                                    cell, cells_by_level.get(cell["level"], []), boxes_by_cell,
                                    axis, value, outward, (ru0, ru1)):
                                _add(out, other, piece)
                                stats["outerShared"] += 1
        out[cell["id"]] = indices
    return out


#: §70.5's own metre: a drop of more than this needs a guard, and the same number decides whether
#: the front porch (+0.57) and the terrace (+0.45) are decks or drops. `house_shell_gen.py` asks it
#: of the same two places for the geometry it draws.
GUARD_DROP = 1.0
#: A parapet's thickness, `house_shell_gen.py`'s `PARAPET_THICK`. The drawn guard is a parapet with
#: a rail above it; what stops you is one solid box from the deck to the rail's height, because a
#: capsule does not fit between a 0.55 m parapet and a 1.10 m rail.
GUARD_THICK = 0.20

#: A stair balustrade is a RAIL and not a parapet: 60 mm, against the balcony guard's masonry.
#: §12.3 gives the height (0.95 m) and not the thickness, and the difference matters only in that a
#: fat one leaves a slot behind it that nothing can stand in.
BALUSTRADE_THICK = 0.06


def _guard_obb(shapes: Shapes, axis: str, value: float, u0: float, u1: float, outward: int,
               floor: float, height: float, surface, thickness: float = None) -> int:
    """One guard box along an open edge, standing INSIDE it the way the drawn parapet does."""
    thick = GUARD_THICK if thickness is None else thickness
    inward = -outward * thick / 2.0
    centre_u, half_u = (u0 + u1) / 2, (u1 - u0) / 2
    if axis == "x":
        centre = (value + inward, floor + height / 2, centre_u)
        half = (thick / 2, height / 2, half_u)
    else:
        centre = (centre_u, floor + height / 2, value + inward)
        half = (half_u, height / 2, thick / 2)
    return shapes.obb(centre, half, 0.0, surface, KIND_WALL)


def build_stairwell_guards(layout, shapes: Shapes, per_cell: dict[str, list[int]],
                           stats: dict) -> None:
    """A rail round the hole in a floor, with a gap where the flight comes up (`HOUSE-00567`).

    §49.2 recorded this as the one drop in the house with nothing at its edge: *"The landings open
    to the stairwell are a drop too and are not guarded yet -- their rail is drawn round the hole
    in the floor rather than round the cell, and Phase 7 owns it."* `HOUSE-00618`'s twenty-minute
    bot then walked off one at 15.5 minutes, fell to the basement and spent the rest of the run
    wedged under the floor, which is what an unguarded well does to whoever finds it.

    §12.3 gives a stair balustrade 0.95 m, and the gap is the step-off strip: the way ON to the
    flight has to stay open or the rail is a wall round a staircase.
    """
    levels = layout_io.by_id(layout_io.rows(layout, "levels"), "level")
    cells = layout_io.rows(layout, "cells")
    cells_by_id = layout_io.by_id(cells, "cell")
    portals = layout_io.rows(layout, "portals")
    construction = layout["levels"].get("construction") or {}
    height = float(construction.get("balustrade", 0.0))
    if height <= 0.0:
        return

    portals_by_plane: dict[tuple[str, float], list[dict]] = {}
    for portal in portals:
        plane = portal.get("plane") or {}
        portals_by_plane.setdefault((plane.get("axis"), snap(float(plane.get("value", 0.0)))), []).append(
            portal)

    for cell in sorted(cells, key=lambda row: row["id"]):
        level = levels.get(cell["level"])
        if level is None:
            continue
        floor = layout_io.cell_extent(cell, level)[0]
        step_off = _step_off_rects(layout, cells_by_id, levels, portals, cell["id"], floor)
        holes = _slab_holes(portals_by_plane, cell["id"], floor, step_off)
        # A rail goes BESIDE a flight, never across one. The main stair leaves the L0 floor at
        # z -14.30 and crosses the basement well's north edge 1.24 m up, so a rail drawn round
        # that well without this opening is a fence across the bottom of the staircase.
        through = list(step_off)
        for flight in layout_io.rows(layout, "stairs"):
            if cell["id"] not in (flight.get("fromCell"), flight.get("toCell")):
                continue
            placed = stair_geometry.flight_runs(
                flight, stair_geometry.foot_of(flight, cells_by_id, levels), portals)
            for entry in placed or ():
                if entry["y1"] <= floor + 0.05:
                    # A flight that tops out AT this floor or below it passes UNDER the rail, and
                    # a rail with a gap over it is a gap you fall through. The way off such a
                    # flight is its step-off strip, which is already in the list. Only a flight
                    # that climbs ABOVE this floor is one you walk onto here.
                    continue
                x0, x1, z0, z1 = entry["box"]
                through.append((x0, z0, x1, z1))
        for hx0, hz0, hx1, hz1 in holes:
            # The four edges of the well, each as (axis, plane value, span, which way the FLOOR is).
            edges = (
                ("x", hx0, (hz0, hz1), -1),
                ("x", hx1, (hz0, hz1), +1),
                ("z", hz0, (hx0, hx1), -1),
                ("z", hz1, (hx0, hx1), +1),
            )
            for axis, value, (u0, u1), outward in edges:
                # Where a flight steps off across this edge there is no rail, or the rail is a wall
                # round a staircase. `_step_off_rects` is the same strip the floor was given back.
                openings = []
                for sx0, sz0, sx1, sz1 in through:
                    if axis == "x":
                        touches = sx0 - 1e-6 <= value <= sx1 + 1e-6
                        span = (max(sz0, u0), min(sz1, u1))
                    else:
                        touches = sz0 - 1e-6 <= value <= sz1 + 1e-6
                        span = (max(sx0, u0), min(sx1, u1))
                    if touches and span[1] - span[0] > 1e-6:
                        openings.append((span[0], 0.0, span[1], 1.0))
                for a0, _v0, a1, _v1 in subtract_rects((u0, 0.0, u1, 1.0), openings):
                    # Is the other side of this edge FLOOR, or more well? Taking the step-off strip
                    # out of the portal rect leaves the well as two rectangles, and the seam
                    # between them is not an edge of anything: a rail there is a rail across the
                    # middle of the hole. Asked of a point a centimetre outside the edge.
                    probe_u = 0.5 * (a0 + a1)
                    probe_v = value + outward * 0.01
                    probe = (probe_v, probe_u) if axis == "x" else (probe_u, probe_v)
                    if any(hx0 - 1e-9 <= probe[0] <= hx1 + 1e-9 and hz0 - 1e-9 <= probe[1] <= hz1 + 1e-9
                           for hx0, hz0, hx1, hz1 in holes):
                        continue
                    _add(per_cell, cell["id"],
                         _guard_obb(shapes, axis, value, a0, a1, -outward, floor, height,
                                    cell.get("wallMaterial"), BALUSTRADE_THICK))
                    stats["stairGuards"] += 1


def build_mezzanine_guards(layout, shapes: Shapes, per_cell: dict[str, list[int]],
                           stats: dict) -> None:
    """A guard round a platform nested in another cell, a storey above ITS floor.

    The garage's storage loft is the case (`HOUSE-00467`): 27 m² at +2.90 over a slab at +0.15,
    reached by a ladder, with nothing at its edge. §70.5 asks for a guard at a drop over a metre
    and does not say the drop has to be outdoors. A container's interior is nested too and is
    0.10 m over its room's floor, so the same metre leaves it alone.

    Its four sides all face into the parent cell, so `build_shell` gives it no walls: `is_open` is
    false for it, but `_neighbour_segments` finds the parent across every side and a cell inside
    another cell shares no boundary plane with it. Nothing else in this file would ever put a
    shape here.
    """
    levels = layout_io.by_id(layout_io.rows(layout, "levels"), "level")
    construction = layout["levels"].get("construction") or {}
    cells = layout_io.by_id(layout_io.rows(layout, "cells"), "cell")
    height = float(construction.get("railing", 0.0))
    if height <= 0.0:
        return
    for identifier, cell in sorted(cells.items()):
        parent = cells.get(cell.get("parent"))
        level, parent_level = levels.get(cell.get("level")), None
        if parent is None or level is None:
            continue
        parent_level = levels.get(parent.get("level"))
        if parent_level is None:
            continue
        floor = layout_io.cell_extent(cell, level)[0]
        if floor - layout_io.cell_extent(parent, parent_level)[0] <= GUARD_DROP:
            continue
        for box in layout_io.cell_boxes(cell):
            for axis, value, u0, u1, outward in _side_planes(box):
                _add(per_cell, identifier,
                     _guard_obb(shapes, axis, value, u0, u1, outward, floor, height,
                                cell.get("wallMaterial")))
                stats["guards"] += 1


# ======================================================================================== rafters


def build_rafters(layout, shapes: Shapes, per_cell: dict[str, list[int]], stats: dict) -> None:
    """The attic's roof slope, as one clipped triangle mesh per cell per plane (`HOUSE-00472`).

    §49.2 builds shell collision from the layout, and the layout cannot say this. A cell is an
    axis-aligned bounding volume, so `L3_STORE_W` declares `yOverride: [9.30, 13.90]` -- §13.6's
    MAXIMUM head-room, "1.2 -> 4.6 m, so most of it is crouch-only". Read as a box that is a flat
    lid at +13.90 over the whole west store, and you may stand upright anywhere in a room whose
    roof is 1.20 m tall at the knee wall. The rafter slope inside the volume is geometry, which
    `layout.cells.json` says in as many words, and geometry is this file's job.

    Each plane is clipped in plan to each cell's own box, so a cell carries the piece of roof over
    itself and not the whole 22 x 13.4 m envelope.

    WHICH cells get rafters is derived, not named: a level that declares a `roof` and a null
    `ceiling` is a level whose upper bound IS the roof, which is the whole meaning of that null
    (`layout.levels.json`: *"the attic's `ceiling` is null because it is bounded by rafters, not by
    a plane"*). `L3` is the only such level. The garage has a flat ceiling at +4.30 and therefore a
    lid already, so its roof -- whose height is wrong, `HOUSE-00480` -- contributes nothing here,
    and a level given rafters later needs no change to this function.
    """
    construction = layout["levels"].get("construction") or {}
    if not construction.get("ridgeY") or not construction.get("roofPitch"):
        return
    levels = layout_io.by_id(layout_io.rows(layout, "levels"), "level")
    pitch = float(construction["roofPitch"])
    roofs = roof_geometry.roof_boxes(layout)
    for level_id, level in sorted(levels.items()):
        name = level.get("roof")
        if not name or level.get("ceiling") is not None or name not in roofs:
            continue
        outer = roof_geometry.outer_box(roofs[name], construction)
        eaves = roof_geometry.eaves_height(construction, outer)
        planes = roof_geometry.roof_planes(outer, eaves, pitch)
        for cell in sorted((row for row in layout_io.rows(layout, "cells")
                            if row.get("level") == level_id), key=lambda c: c["id"]):
            if cell.get("kind") == "exterior":
                continue
            head = layout_io.cell_extent(cell, level)[1]
            for index, (corners, _outward) in enumerate(planes):
                for box in layout_io.cell_boxes(cell):
                    piece = roof_geometry.clip_to_rect(corners, box)
                    if roof_geometry.plan_area(piece) < MIN_RAFTER_AREA:
                        continue
                    # `L3_ROOM` has a plastered collar ceiling at +12.60 and its own flat slab
                    # stops you there; the roof over it carries on to the ridge at +14.30 and is
                    # on the far side of that ceiling. A shape you cannot reach is a shape the
                    # broad phase pays for and nothing ever hits.
                    if min(point[1] for point in piece) >= head - 1e-6:
                        stats["rafterAboveCeiling"] += 1
                        continue
                    vertices, triangles = _fan(roof_geometry.face_up(piece))
                    shape = shapes.mesh(vertices, triangles, cell.get("footstepSurface"),
                                        KIND_CEILING)
                    _add_mesh(per_cell, cell["id"], shape)
                    stats["rafterMeshes"] += 1
                    stats["rafterArea"] += roof_geometry.plan_area(piece)
                    stats.setdefault("rafterRoofs", set()).add(f"{name}.{index}")


#: The smallest piece of roof worth a collision shape, in plan m². A cell that clips a plane at a
#: corner produces a sliver a millimetre across, and a sliver is a shape the broad phase pays for
#: and the narrow phase can never usefully hit.
MIN_RAFTER_AREA = 0.01


def _fan(polygon):
    """@p polygon as a triangle fan from its first vertex: `(vertices, triangles)`.

    A clipped convex face, so a fan is a valid triangulation of it. The winding is the polygon's
    own, so the caller hands it a polygon already faced the right way -- `roof_geometry.face_up`
    for a roof, whose outside is the sky.
    """
    vertices = [tuple(point) for point in polygon]
    return vertices, [(0, index, index + 1) for index in range(1, len(polygon) - 1)]


# ========================================================================================= stairs


def build_stairs(layout, shapes: Shapes, per_cell: dict[str, list[int]], stats: dict) -> None:
    """A flight becomes a closed wedge per run plus a box per landing, or per-step OBBs.

    `collisionRamp` is authored per flight and both branches are real. The ramp is a **closed**
    prism -- top, underside, two sides and two ends -- not just the walking surface: a sweep that
    only ever meets the top face passes straight through the flight from underneath, which is
    exactly what the stairwell below is.

    WHERE the flight is comes from `stair_geometry`, the module `house_shell_gen.py` draws the
    visible steps from, so the thing you collide with is the thing you can see (`HOUSE-00472`).
    This function used to guess: it ran every flight along +Z from the low cell's box edge, in one
    lane, because it was written three phases before `HOUSE-00459` authored a `footprint`, a `run`
    and a `shape`. A flight that still has no placement falls back to that guess and is counted in
    `stats["stairsGuessed"]`, so `--report` says so out loud rather than quietly inventing a
    staircase.
    """
    levels = layout_io.by_id(layout_io.rows(layout, "levels"), "level")
    cells = layout_io.by_id(layout_io.rows(layout, "cells"), "cell")
    # `HOUSE-00480`: a flight is placed inside the stairwell it comes up, not against its own
    # footprint's edge, which is a wall centre line.
    portals = list(layout_io.rows(layout, "portals"))
    for flight in sorted(layout_io.rows(layout, "stairs"), key=lambda f: f["id"]):
        from_cell = cells.get(flight["fromCell"])
        to_cell = cells.get(flight["toCell"])
        if from_cell is None or to_cell is None:
            raise LayoutError(
                f"stair {flight['id']!r} connects {flight['fromCell']!r} -> {flight['toCell']!r}; "
                f"one of those cells does not exist")
        width = float(flight["width"])
        base = stair_geometry.foot_of(flight, cells, levels)
        surface = flight.get("surface")
        walk = stair_geometry.flight_runs(flight, base, portals)
        if walk is None:
            stats["stairsGuessed"] += 1
            walk = _guessed_walk(flight, from_cell, base, width)

        if flight.get("collisionRamp", True):
            for entry in walk:
                if entry["kind"] == "run":
                    vertices, triangles = _wedge(entry["box"], entry["y0"],
                                                 entry["y1"] - entry["y0"],
                                                 entry["axis"], entry["up"])
                    index = shapes.mesh(vertices, triangles, surface, KIND_STAIR)
                    _add_mesh(per_cell, from_cell["id"], index)
                    _add_mesh(per_cell, to_cell["id"], index)
                    stats["stairMeshes"] += 1
                else:
                    x0, x1, z0, z1 = entry["box"]
                    thickness = float(flight["rise"])
                    _add(per_cell, from_cell["id"], shapes.obb(
                        ((x0 + x1) / 2, entry["y0"] - thickness / 2, (z0 + z1) / 2),
                        ((x1 - x0) / 2, thickness / 2, (z1 - z0) / 2), 0.0, surface, KIND_STAIR))
                    stats["stairLandings"] += 1
        else:
            treads = stair_geometry.flight_steps(flight, base, portals)
            if treads is None:
                treads = _guessed_treads(flight, walk, base)
            rise = float(flight["rise"])
            for tread in treads:
                x0, x1, z0, z1 = tread["box"]
                _add(per_cell, from_cell["id"], shapes.obb(
                    ((x0 + x1) / 2, tread["y1"] - rise / 2, (z0 + z1) / 2),
                    ((x1 - x0) / 2, rise / 2, (z1 - z0) / 2), 0.0, surface, KIND_STAIR))
                stats["stairSteps"] += 1


def _guessed_walk(flight: dict, from_cell: dict, base: float, width: float):
    """The pre-`HOUSE-00472` placement, for a flight that authors no footprint.

    It runs along +Z from the low cell's box, centred across. Nothing in this house needs it --
    every flight has an authored footprint -- and it exists so that a layout mid-edit still builds
    instead of throwing, which is what the fixture world exercises.
    """
    x0, x1, _z0, z1 = layout_io.cell_boxes(from_cell)[0]
    origin_x = (x0 + x1) / 2 - width / 2
    walk = stair_geometry.segments(flight, base)
    for entry in walk:
        low, high = sorted((entry["along0"], entry["along1"]))
        entry["box"] = (origin_x, origin_x + width, z1 + low, z1 + high)
        entry["axis"], entry["up"] = "z", 1
    return walk


def _guessed_treads(flight: dict, walk, base: float):
    """One box per riser over `_guessed_walk`'s runs, for the same unauthored case."""
    going, rise = float(flight["going"]), float(flight["rise"])
    treads = []
    for entry in walk:
        if entry["kind"] != "run":
            continue
        x0, x1, z0, _z1 = entry["box"]
        for index in range(entry["risers"]):
            treads.append({"box": (x0, x1, z0 + index * going, z0 + (index + 1) * going),
                           "y1": entry["y0"] + (index + 1) * rise})
    return treads


#: How a run's local frame -- `lx` across, `lz` up the slope -- maps onto the world, per travel
#: axis and rise direction: `(along edge of the box, its sign, cross edge, its sign)`, indices into
#: `(x0, x1, z0, z1)`. Every one has a POSITIVE determinant, and that is the reason the cross axis
#: flips too: mirror a prism and its faces turn inside out, and a collision mesh whose normals
#: point into itself is a solid you fall through.
_FRAMES = {
    ("z", 1): (2, +1.0, 0, +1.0),
    ("z", -1): (3, -1.0, 1, -1.0),
    ("x", 1): (0, +1.0, 3, -1.0),
    ("x", -1): (1, -1.0, 2, +1.0),
}


def _wedge(box, y, height, axis, up):
    """A closed right-triangular prism over @p box, rising towards @p up along @p axis.

    Six vertices -- the bottom rectangle plus the two raised corners at the high end -- and eight
    triangles. `selftest` counts the edges rather than trusting this comment: in a closed surface
    every edge belongs to exactly two triangles, and the first version of this function had the
    left face twice and the walking surface not at all while still looking like a staircase.
    """
    x0, x1, z0, z1 = box
    length = (x1 - x0) if axis == "x" else (z1 - z0)
    width = (z1 - z0) if axis == "x" else (x1 - x0)
    along_edge, along_sign, cross_edge, cross_sign = _FRAMES[(axis, int(up))]

    def world(lx, lz):
        along = box[along_edge] + along_sign * lz
        cross = box[cross_edge] + cross_sign * lx
        return (along, cross) if axis == "x" else (cross, along)

    corners = [world(0.0, 0.0), world(width, 0.0), world(0.0, length), world(width, length)]
    vertices = [
        (corners[0][0], y, corners[0][1]), (corners[1][0], y, corners[1][1]),
        (corners[2][0], y, corners[2][1]), (corners[3][0], y, corners[3][1]),
        (corners[2][0], y + height, corners[2][1]), (corners[3][0], y + height, corners[3][1]),
    ]
    # Counter-clockwise seen from OUTSIDE, which is §14's convention for everything this
    # repository generates. `HOUSE-00210` wound this prism the other way and nothing noticed,
    # because a collision mesh is never drawn -- but the sweep that `HOUSE-00473` onwards will
    # write reads a face normal to decide which side of a surface a body is on, and every one of
    # these eight pointed into the solid.
    triangles = [
        (0, 1, 3), (0, 3, 2),      # underside
        (0, 5, 1), (0, 4, 5),      # the sloped walking surface
        (2, 3, 5), (2, 5, 4),      # the vertical face at the top of the run
        (0, 2, 4),                 # left
        (1, 5, 3),                 # right
    ]
    return vertices, triangles


def _add(per_cell: dict[str, list[int]], cell_id: str, index: int) -> None:
    per_cell.setdefault(cell_id, [])
    if index not in per_cell[cell_id]:
        per_cell[cell_id].append(index)


def _add_mesh(per_cell: dict[str, list[int]], cell_id: str, mesh: int) -> None:
    """Reference a triangle MESH, which is numbered after every OBB -- once they all exist.

    A mesh's shape index is `len(obbs) + mesh`, and callers used to compute that on the spot. It
    is only right if no OBB is ever made afterwards, and `HOUSE-00472` made two builders that do:
    every stair wedge and every rafter in the house pointed four shapes wrong the moment the
    garage loft got a guard. Props would have done the same to the stairs since `HOUSE-00210`, and
    this house has no props with proxies yet, which is the only reason nothing had noticed.

    So a mesh is referenced as `-(mesh + 1)` and `_resolve_meshes` turns it into a shape index when
    the shape list is complete. A negative index is not a number anything can use by accident.
    """
    _add(per_cell, cell_id, -(mesh + 1))


def _resolve_meshes(per_cell: dict[str, list[int]], obb_count: int) -> None:
    """Turn every `-(mesh + 1)` reference into its final shape index, in place."""
    for cell_id, indices in per_cell.items():
        per_cell[cell_id] = [obb_count + (-index - 1) if index < 0 else index
                             for index in indices]


# ============================================================================ the property outdoors

#: What a body meets at §11.2's fences: the BOARDS, 0.10 m of them, which is the post section the
#: drawn fence is set out on. A fence is not a wall and does not get a wall's thickness.
FENCE_THICKNESS = 0.10
#: How long one piece of fence is. Not a bay -- `fence_gen` owns where the posts are -- but how
#: often the collision has to follow §11.5's ground, which falls 0.9 m from the road to the rear
#: fence. One box per run would be a fence hanging in the air at one end and buried at the other.
FENCE_PIECE = 2.0
#: §11.4's kerb: 0.15 m high (the layout says so) and this wide. A body walks OVER one, because
#: §43.1's step-up is 0.22 m -- which is what a kerb is for.
KERB_WIDTH = 0.16
#: §49.2 collides with **tree trunks**, not canopies: `(width, height)` in metres, per species.
#: A canopy you cannot walk under is a tree that has swallowed the garden it stands in.
TRUNKS = {
    "MODEL_TREE_MAPLE_MATURE": (0.45, 3.0),
    "MODEL_TREE_MAPLE_STREET": (0.35, 3.0),
    "MODEL_TREE_BIRCH": (0.30, 3.0),
    "MODEL_TREE_FRUIT": (0.25, 2.2),
}
#: §10.4's hedges, which bound the road corridor: `(depth, height)`. 2.1 m is §10.4's own figure
#: and 0.5 m is what an extruded 1 m strip is thick. A hedge is not a tree -- it is a barrier, and
#: `HOUSE-00775` is where the road's ends became ones.
HEDGES = {"MODEL_HEDGE_PRIVET_1M": (0.5, 2.1)}
#: §70.5's car, which §49.2 lists: 4.4 m long, 1.80 m across and 1.50 m tall, and written the way
#: the MODEL is -- `(across, tall, along)`. §14 puts a model's forward at -Z and yaws it about +Y,
#: so a car's length runs along its own Z and the two cars parked at this east-west kerb are
#: authored at 90 and 270 degrees. Writing the length along X instead parked them ACROSS the road,
#: and `HOUSE-00775`'s walk to the end of it stopped 24 m early against a car's flank.
CAR_SIZE = (1.8, 1.5, 4.4)
#: The assets that are ones. §10.4's delivery van at the east end of the road is the second.
CAR_ASSETS = ("MODEL_PARKED_CAR", "MODEL_DELIVERY_VAN")
#: §10.4's van, which is bigger than a car: 5.4 m long, 2.10 m across and 2.40 m tall.
VAN_SIZE = (2.1, 2.4, 5.4)


def open_air(cell: dict) -> bool:
    """Is this cell the open outdoors -- a yard, a deck, the road -- rather than a building?

    §15.7 rule 5 already draws this line: *"an `exterior` cell that is roofed and
    `visibilityHint: opaque` is a building"*. `EXT_SHED` is the one, and it keeps its walls; the
    seventeen open ones are ground, and the boundary between two of them is grass.
    """
    return cell.get("kind") == "exterior" and cell.get("visibilityHint") == "open"


def ground_storey(layout) -> float:
    """The `ffl` of the lowest level at or above §10.2's grade: the storey the lot is at.

    Found rather than named, so a level inserted below `L0` does not silently move which one
    counts as the ground -- `terrain_gen.surfaces` finds it the same way, for the same reason.
    """
    above = [float(row.get("ffl", 0.0)) for row in layout_io.rows(layout, "levels")
             if float(row.get("ffl", 0.0)) >= 0.0]
    return min(above) if above else 0.0


def _exterior_cells(layout) -> list[tuple[str, list]]:
    """Every open exterior cell but `EXT_WORLD`, with its footprint boxes.

    `EXT_WORLD` is excluded because it is the backdrop: it reaches 200 m in each direction and
    §10.3's playable volume is 40, so a kerb piece 180 m down the road is scenery that no body can
    ever touch. What the property's own cells cover is what a body can walk into.
    """
    out = []
    for cell in layout_io.rows(layout, "cells"):
        if cell["id"] == "EXT_WORLD" or not open_air(cell):
            continue
        out.append((cell["id"], layout_io.cell_boxes(cell)))
    return out


def _outdoor_owners(aabb, cells: list[tuple[str, list]], on_the_ground: bool = False) -> list[str]:
    """Which open cells a shape belongs to: every one a body standing in it could reach it from.

    The same `OPENING_REACH` the holes use, and for the same reason. Two yards abut with no wall
    and no portal between them, so §16.4 hands the body from one to the other in the middle of an
    open lawn; a fence post on the boundary has to be in both lists or the body meets it in one
    yard and walks through it in the other.

    A shape no named cell reaches belongs to `EXT_WORLD` when it stands on the GROUND
    (`HOUSE-00775`). §10.4's road termination is at x = ±35 and the property's cells stop at
    ±22.5, so the barriers that end the road are outside every one of them -- and §16.4 answers
    `EXT_WORLD` for a body standing there, which is the cell whose list they have to be in. The
    ground is the limit because §11.5's height field IS §10.3's playable volume: 80 x 64 m, x
    ±40 by z -52…+12. A kerb 180 m down the road is on no ground and behind the boundary that
    stops the player at 40, so it is scenery and not collision.
    """
    out = []
    for cell_id, boxes in cells:
        for x0, x1, z0, z1 in boxes:
            if (aabb[0] < x1 + OPENING_REACH and aabb[3] > x0 - OPENING_REACH
                    and aabb[2] < z1 + OPENING_REACH and aabb[5] > z0 - OPENING_REACH):
                out.append(cell_id)
                break
    if not out and on_the_ground:
        out.append("EXT_WORLD")
    return out


def build_exterior(layout, shapes: Shapes, per_cell: dict[str, list[int]], stats: dict,
                   world_dir: Path) -> None:
    """§49.2's exterior collision: OBBs for what stands on §11.5's ground (`HOUSE-00774`).

    *"Exterior collision uses the terrain height field plus OBBs for fences, walls, kerbs, the
    shed, vehicles and tree trunks."* The height field is `HOUSE-00553`'s and is read straight into
    the file; the shed is a cell and gets its walls from `build_shell` like any room. This is
    everything else, and until it existed the property was divided by the CELL boundaries instead:
    86 wall pieces between one open yard and another, 5 219 m² of invisible wall, 29 of them over
    5 m tall. A body could not walk from the front lawn to the side yard.

    Each piece is put in every open cell within reach of it, because two yards abut with no wall
    and no portal, so nothing else would share it.
    """
    cells = _exterior_cells(layout)
    if not cells:
        return
    exterior = layout.get("exterior") or {}
    heights = None
    if (world_dir / "terrain.png").is_file():
        _w, _h, samples, _materials = terrain_gen.decode(world_dir)
        heights = samples

    def ground(x: float, z: float) -> float:
        """§11.5's height field at a point, or zero for a world that has none (the fixture)."""
        if heights is None:
            return 0.0
        ix = min(max(int(round((x - terrain_gen.ORIGIN_X) / terrain_gen.STEP)), 0),
                 terrain_gen.WIDTH - 1)
        iz = min(max(int(round((z - terrain_gen.ORIGIN_Z) / terrain_gen.STEP)), 0),
                 terrain_gen.HEIGHT - 1)
        return heights[iz * terrain_gen.WIDTH + ix]

    def on_the_lot(aabb) -> bool:
        """Is any of this shape over §11.5's height field, which is §10.3's playable volume?"""
        return (aabb[0] < terrain_gen.ORIGIN_X + (terrain_gen.WIDTH - 1) * terrain_gen.STEP
                and aabb[3] > terrain_gen.ORIGIN_X
                and aabb[2] < terrain_gen.ORIGIN_Z + (terrain_gen.HEIGHT - 1) * terrain_gen.STEP
                and aabb[5] > terrain_gen.ORIGIN_Z)

    def place(centre, half, yaw: float, surface: str, kind: int, counter: str) -> None:
        """One OBB, in every open cell that can reach it -- and NOT in the pool if none can.

        The kerbs run 440 m down the road and 34 street trees stand along it; the property's own
        cells cover 45 m of that. A shape nothing references is a shape in the file for no reason,
        so the reach test comes first and the pool is only asked for what survives it.
        """
        aabb = obb_aabb((centre, half, yaw, 0, kind))
        owners = _outdoor_owners(aabb, cells, on_the_ground=on_the_lot(aabb))
        if not owners:
            return
        index = shapes.obb(centre, half, yaw, surface, kind)
        for cell_id in owners:
            _add(per_cell, cell_id, index)
        stats[counter] += 1

    # §11.2's fences, in pieces short enough to follow the ground, with the gate openings left out
    # of them: a gate is a leaf that opens (§65), so the hole is what the static file carries and
    # `DynamicObstacles` carries the leaf.
    openings = [(float(gate["opening"]["x"][0]), float(gate["opening"]["x"][1]),
                 float(gate["opening"]["z"][0]), float(gate["opening"]["z"][1]))
                for gate in exterior.get("gates", []) if gate.get("opening")]
    for row in exterior.get("fences", []):
        start, end = row["path"][0], row["path"][-1]
        along_x = abs(end[0] - start[0]) >= abs(end[2] - start[2])
        low = min(start[0], end[0]) if along_x else min(start[2], end[2])
        high = max(start[0], end[0]) if along_x else max(start[2], end[2])
        across = (start[2] + end[2]) / 2.0 if along_x else (start[0] + end[0]) / 2.0
        height = float(row.get("height") or 1.85)
        if high - low <= EPS:
            continue
        # A gate is a HOLE in the fence, and the hole is cut before the run is cut into pieces.
        # Dropping whichever PIECE happened to be centred in it would leave §11.2's 1.2 m
        # pedestrian gate covered by the two pieces either side of it -- the opening is narrower
        # than a piece -- and would board up a gate that a piece boundary happened to straddle.
        # §11.2 authors its three runs to stop either side of each gate, so nothing in this house
        # exercises it; a run drawn THROUGH a gate is what it is here for.
        spans = [(low, high)]
        for x0, x1, z0, z1 in openings:
            if along_x:
                if not (z0 - FENCE_THICKNESS <= across <= z1 + FENCE_THICKNESS):
                    continue
                cut_low, cut_high = x0, x1
            else:
                if not (x0 - FENCE_THICKNESS <= across <= x1 + FENCE_THICKNESS):
                    continue
                cut_low, cut_high = z0, z1
            spans = [piece for span in spans
                     for piece in ((span[0], min(span[1], cut_low)), (max(span[0], cut_high), span[1]))
                     if piece[1] - piece[0] > EPS]
        for span_low, span_high in spans:
            length = span_high - span_low
            pieces = max(1, int(math.ceil(length / FENCE_PIECE - 1e-9)))
            step = length / pieces
            for index in range(pieces):
                a, b = span_low + index * step, span_low + (index + 1) * step
                middle = (a + b) / 2.0
                x, z = (middle, across) if along_x else (across, middle)
                base = ground(x, z)
                half = ((b - a) / 2.0, height / 2.0, FENCE_THICKNESS / 2.0) if along_x else \
                       (FENCE_THICKNESS / 2.0, height / 2.0, (b - a) / 2.0)
                place((x, base + height / 2.0, z), half, 0.0, "fence", KIND_EXTERIOR, "fencePieces")

    # §11.4's kerbs, which a body steps over rather than into.
    for row in exterior.get("kerbs", []):
        start, end = row["path"][0], row["path"][-1]
        along_x = abs(end[0] - start[0]) >= abs(end[2] - start[2])
        low = min(start[0], end[0]) if along_x else min(start[2], end[2])
        high = max(start[0], end[0]) if along_x else max(start[2], end[2])
        across = (start[2] + end[2]) / 2.0 if along_x else (start[0] + end[0]) / 2.0
        height = float(row.get("height") or 0.15)
        pieces = max(1, int(math.ceil((high - low) / FENCE_PIECE - 1e-9)))
        step = (high - low) / pieces
        for index in range(pieces):
            a, b = low + index * step, low + (index + 1) * step
            middle = (a + b) / 2.0
            x, z = (middle, across) if along_x else (across, middle)
            base = ground(x, z)
            half = ((b - a) / 2.0, height / 2.0, KERB_WIDTH / 2.0) if along_x else \
                   (KERB_WIDTH / 2.0, height / 2.0, (b - a) / 2.0)
            place((x, base + height / 2.0, z), half, 0.0, "kerb", KIND_EXTERIOR, "kerbPieces")

    # §11.1's garden furniture: a raised bed is a solid box 0.45 m high, which is twice §43.1's
    # step-up, so you walk round it. The ones with a CELL are buildings and have walls already.
    for row in exterior.get("structures", []):
        if row.get("cell"):
            continue
        x0, x1 = float(row["footprint"]["x"][0]), float(row["footprint"]["x"][1])
        z0, z1 = float(row["footprint"]["z"][0]), float(row["footprint"]["z"][1])
        height = float(row.get("height") or 0.45)
        base = min(ground(x, z) for x in (x0, x1, (x0 + x1) / 2.0)
                   for z in (z0, z1, (z0 + z1) / 2.0))
        place(((x0 + x1) / 2.0, base + height / 2.0, (z0 + z1) / 2.0),
              ((x1 - x0) / 2.0, height / 2.0, (z1 - z0) / 2.0), 0.0,
              "structure", KIND_EXTERIOR, "structureObbs")

    # §49.2's tree TRUNKS. A canopy is not collision: a body walks under a maple.
    for group in exterior.get("vegetation", []):
        size = TRUNKS.get(group.get("asset"))
        if size is None:
            continue
        width, height = size
        for instance in group.get("instances", []):
            position = instance.get("position") or [0.0, 0.0, 0.0]
            scale = float(instance.get("scale") or 1.0)
            x, z = float(position[0]), float(position[2])
            base = ground(x, z)
            place((x, base + height * scale / 2.0, z),
                  (width * scale / 2.0, height * scale / 2.0, width * scale / 2.0),
                  0.0, "bark", KIND_EXTERIOR, "trunks")

    # §10.4's hedges: the second of its five containment layers, and the one that ends the road.
    # A hedge is authored as an extruded strip -- one 1 m section per instance -- so the sections
    # meet and the barrier is continuous, which is what makes it a barrier and not a row of bushes.
    for group in exterior.get("vegetation", []):
        size = HEDGES.get(group.get("asset"))
        if size is None:
            continue
        depth, height = size
        for instance in group.get("instances", []):
            position = instance.get("position") or [0.0, 0.0, 0.0]
            x, z = float(position[0]), float(position[2])
            yaw = math.radians(float(instance.get("yawDeg") or 0.0))
            base = ground(x, z)
            # The strip runs along its own local X, so a section turned 90 degrees runs along Z.
            place((x, base + height / 2.0, z), (0.5, height / 2.0, depth / 2.0), yaw,
                  "hedge", KIND_EXTERIOR, "hedges")

    # §11.4's parked cars, which are §49.2's "vehicles". They carry a yaw, and an OBB is the one
    # shape in this file that can.
    for row in exterior.get("neighbourhood", []):
        if row.get("asset") not in CAR_ASSETS:
            continue
        size = VAN_SIZE if row.get("asset") == "MODEL_DELIVERY_VAN" else CAR_SIZE
        position = row.get("position") or [0.0, 0.0, 0.0]
        x, z = float(position[0]), float(position[2])
        yaw = math.radians(float(row.get("yawDeg") or 0.0))
        base = ground(x, z)
        place((x, base + size[1] / 2.0, z),
              (size[0] / 2.0, size[1] / 2.0, size[2] / 2.0),
              yaw, "vehicle", KIND_EXTERIOR, "vehicles")


# ================================================================== what reaches through a hole

#: §43.1's capsule radius. Collision is built for the body that walks it, so the body's own size
#: is a number this file needs.
BODY_RADIUS = 0.30
#: §16.4's hysteresis, `SpatialIndex::kHysteresis`: how far past its own boundary the cell lookup
#: still answers with the cell the body was already in. A body that far past the plane is still
#: being swept against the cell it came from, so the cell it came from has to know what is there.
CELL_HYSTERESIS = 0.05
#: How far past a hole in its boundary a body simulated in a cell can touch something: the two
#: above, plus 50 mm so the rule does not sit exactly on the number it is derived from. 0.40 m.
OPENING_REACH = BODY_RADIUS + CELL_HYSTERESIS + 0.05
#: §43.1's standing body, floor to crown. What a body in a doorway can reach ABOVE the floor it
#: stands on, and so how high up the other side of a hole is worth carrying.
BODY_HEIGHT = 1.80
#: §43.1's step-up, `physics::kStepUpHeight`. A hole whose sill is no higher than this above the
#: floor is one a body walks THROUGH; anything higher is a window, and what a body can put through
#: a window is its shoulder rather than itself.
STEP_UP = 0.22


def _intersect_aabb(a, b):
    """The overlap of two AABBs, or `None` when they do not touch."""
    low = tuple(max(a[i], b[i]) for i in range(3))
    high = tuple(min(a[i + 3], b[i + 3]) for i in range(3))
    if any(high[i] <= low[i] for i in range(3)):
        return None
    return low + high


def union_aabb(boxes):
    """The AABB of a list of AABBs. A shape borrowed through two holes is indexed by both."""
    return (tuple(min(box[i] for box in boxes) for i in range(3))
            + tuple(max(box[i + 3] for box in boxes) for i in range(3)))


def share_through_openings(layout, per_cell: dict[str, list[int]], aabbs, stats: dict) -> dict:
    """A body standing in a doorway is in BOTH rooms, so both rooms' lists carry what it can touch.

    §49.2 partitions collision per cell and the sweep is given ONE cell -- whichever §16.4's lookup
    answers with. That is right for everything a wall separates, because a wall belongs to the
    lists on both sides of it and nothing behind one can be reached. It is wrong at a **hole**: the
    main stair's flight starts 0.20 m east of `L0_FOYER`'s cased opening, in `L0_STAIR_MAIN`'s list
    alone, and a body walking east through that opening met nothing until the cell tracker changed
    its mind -- by which time it was 0.16 m inside the flight and being shoved back out. That is
    `HOUSE-00618`'s bot walking into a staircase, and it is the same defect `HOUSE-00567` fixed for
    the outer walls the yards could not see, one hole further in.

    So: through every hole in a boundary, each side gains the other side's shapes within
    `OPENING_REACH` of the plane, over the hole's own width and the body's own height. `y` portals
    are deliberately not holes for this purpose -- a hole in a slab is a way DOWN, and the flights
    that reach through one are already in the lists of both cells they connect (`build_stairs`).

    A shared shape is INDEXED by the parts of it that are within reach (the returned clips, one per
    hole it came through), not by all of it: a neighbour's floor slab spans the neighbour's whole
    room, and letting that size this cell's grid would grow `L0_HALL`'s from 5 x 5 buckets to
    17 x 13 and make the cell's bounds a statement about a room the body cannot be in. The shape
    itself is whole -- the narrow phase gets the real geometry -- the clip only decides which
    buckets have to find it.
    """
    levels = layout_io.by_id(layout_io.rows(layout, "levels"), "level")
    cells = layout_io.by_id(layout_io.rows(layout, "cells"), "cell")
    listed = {cell_id: set(indices) for cell_id, indices in per_cell.items()}
    clipped: dict[tuple[str, int], tuple] = {}
    for portal in sorted(layout_io.rows(layout, "portals"), key=lambda row: row["id"]):
        plane = portal.get("plane") or {}
        axis = plane.get("axis")
        if axis not in ("x", "z"):
            continue
        value = float(plane.get("value", 0.0))
        rect = portal.get("rect") or {}
        u0, u1 = (float(v) for v in rect["u"])
        v0, v1 = (float(v) for v in rect["v"])
        for me, other in ((portal["cellA"], portal["cellB"]), (portal["cellB"], portal["cellA"])):
            if (me not in per_cell or other not in per_cell
                    or me not in cells or other not in cells):
                continue
            here, _lid = layout_io.cell_extent(cells[me], levels[cells[me]["level"]])
            over, _other_lid = layout_io.cell_extent(cells[other], levels[cells[other]["level"]])
            low_floor, high_floor = min(here, over), max(here, over)
            if v0 <= high_floor + STEP_UP:
                # A way THROUGH: the body stands in the hole, on the higher of the two floors, and
                # what it can touch is its own height above that -- which is not the hole's height.
                # The fridge sub-cell is where the difference showed: its ceiling slab starts
                # exactly at the top of its own opening, and a body standing on its 0.70 m floor
                # has 50 mm of head inside that slab while the kitchen, reading the HOLE, did not
                # carry it.
                low_y, high_y = low_floor, high_floor + BODY_HEIGHT
            else:
                # A window. A body cannot be in it -- the wall under it is what it stands against
                # -- so only what is level with the hole itself is within reach.
                low_y, high_y = v0, v1
            if high_y <= low_y:
                continue
            if axis == "x":
                reach = (value - OPENING_REACH, low_y, u0 - OPENING_REACH,
                         value + OPENING_REACH, high_y, u1 + OPENING_REACH)
            else:
                reach = (u0 - OPENING_REACH, low_y, value - OPENING_REACH,
                         u1 + OPENING_REACH, high_y, value + OPENING_REACH)
            for index in per_cell[other]:
                overlap = _intersect_aabb(aabbs[index], reach)
                if overlap is None:
                    continue
                if index in listed[me]:
                    # Already this cell's own shape, whole. Only what is borrowed is clipped, and
                    # a shape borrowed through a second hole is indexed by both parts.
                    if (me, index) in clipped:
                        clipped[(me, index)].append(overlap)
                    continue
                per_cell[me].append(index)
                listed[me].add(index)
                clipped[(me, index)] = [overlap]
                stats["openingShared"] += 1
    return clipped


# ========================================================================================== props


def node_world_transforms(document: dict) -> dict[int, tuple]:
    """Every node's world transform as a 4x4 row-tuple, walking each scene from its roots."""
    nodes = document.get("nodes", [])
    out: dict[int, tuple] = {}

    def local(node):
        if "matrix" in node:
            m = node["matrix"]  # glTF matrices are column-major
            return tuple(tuple(m[c * 4 + r] for c in range(4)) for r in range(4))
        t = node.get("translation", [0.0, 0.0, 0.0])
        r = node.get("rotation", [0.0, 0.0, 0.0, 1.0])
        s = node.get("scale", [1.0, 1.0, 1.0])
        x, y, z, w = r
        rot = (
            (1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)),
            (2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)),
            (2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)),
        )
        return tuple(
            tuple(rot[i][j] * s[j] for j in range(3)) + (t[i],) for i in range(3)
        ) + ((0.0, 0.0, 0.0, 1.0),)

    def multiply(a, b):
        return tuple(
            tuple(sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)) for i in range(4)
        )

    def walk(index, parent):
        world = multiply(parent, local(nodes[index]))
        out[index] = world
        for child in nodes[index].get("children", []):
            walk(child, world)

    identity = tuple(tuple(1.0 if i == j else 0.0 for j in range(4)) for i in range(4))
    seen_roots = {c for node in nodes for c in node.get("children", [])}
    for index in range(len(nodes)):
        if index not in seen_roots:
            walk(index, identity)
    return out


def read_col_meshes(path: Path) -> list[tuple[list, list]]:
    """Every `*_COL` node's triangles, in the asset's own space, as `(vertices, triangles)`."""
    document, blob = gltf_io.read_model(path)
    buffers = gltf_io.buffer_bytes(document, blob, path.parent)
    transforms = node_world_transforms(document)
    out = []
    for index, node in enumerate(document.get("nodes", [])):
        name = node.get("name", "")
        if not name.endswith("_COL") or "mesh" not in node:
            continue
        matrix = transforms[index]
        vertices: list[tuple[float, float, float]] = []
        triangles: list[tuple[int, int, int]] = []
        for primitive in document["meshes"][node["mesh"]].get("primitives", []):
            if primitive.get("mode", 4) != 4:
                continue
            positions = gltf_io.read_accessor(document, buffers, primitive["attributes"]["POSITION"])
            base = len(vertices)
            for px, py, pz in positions:
                vertices.append(tuple(
                    matrix[r][0] * px + matrix[r][1] * py + matrix[r][2] * pz + matrix[r][3]
                    for r in range(3)))
            if "indices" in primitive:
                flat = [int(v[0]) for v in gltf_io.read_accessor(
                    document, buffers, primitive["indices"])]
            else:
                flat = list(range(len(positions)))
            for i in range(0, len(flat) - 2, 3):
                triangles.append((base + flat[i], base + flat[i + 1], base + flat[i + 2]))
        if triangles:
            out.append((vertices, triangles))
    return out


def split_components(vertices, triangles):
    """Connected components by shared *position*, not by vertex index.

    `collision_proxy.py`'s `boxes` mode joins five separate boxes into one mesh, and the exporter
    splits vertices per face, so index adjacency says nothing. Welding by snapped position is what
    actually finds the five boxes.
    """
    weld: dict[tuple, int] = {}
    label: list[int] = []
    for v in vertices:
        key = tuple(snap(c) for c in v)
        label.append(weld.setdefault(key, len(weld)))

    parent = list(range(len(weld)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for a, b, c in triangles:
        for x, y in ((a, b), (b, c)):
            ra, rb = find(label[x]), find(label[y])
            if ra != rb:
                parent[ra] = rb

    groups: dict[int, list[tuple[int, int, int]]] = {}
    for triangle in triangles:
        groups.setdefault(find(label[triangle[0]]), []).append(triangle)

    out = []
    for root in sorted(groups, key=lambda r: min(min(t) for t in groups[r])):
        tris = groups[root]
        used = sorted({i for t in tris for i in t})
        remap = {old: new for new, old in enumerate(used)}
        out.append(([vertices[i] for i in used],
                    [(remap[a], remap[b], remap[c]) for a, b, c in tris]))
    return out


def as_box(vertices, triangles):
    """`(centre, half)` if this component is an axis-aligned box, else `None`.

    Eight distinct positions, twelve triangles, and every distinct position a corner of the
    component's own bounds. The last clause is the one that matters: twelve triangles over eight
    corners is also a shape with a dent in it, and a dent the player can walk into is not a box.
    """
    if len(triangles) != 12:
        return None
    distinct = sorted({tuple(snap(c) for c in v) for v in vertices})
    if len(distinct) != 8:
        return None
    lo = [min(p[i] for p in distinct) for i in range(3)]
    hi = [max(p[i] for p in distinct) for i in range(3)]
    for i in range(3):
        if hi[i] - lo[i] <= EPS:
            return None
    corners = {(lo[0] if a else hi[0], lo[1] if b else hi[1], lo[2] if c else hi[2])
               for a in (0, 1) for b in (0, 1) for c in (0, 1)}
    if {tuple(snap(c) for c in p) for p in distinct} != {tuple(snap(c) for c in p) for p in corners}:
        return None
    return (tuple((lo[i] + hi[i]) / 2 for i in range(3)),
            tuple((hi[i] - lo[i]) / 2 for i in range(3)))


def build_props(layout, shapes: Shapes, per_cell, asset_paths, stats) -> None:
    """Static props with collision become OBBs where they can and triangle meshes where they must."""
    materials = layout_io.by_id(layout_io.rows(layout, "materials"), "material")
    for prop in sorted(layout_io.rows(layout, "props"), key=lambda p: p["id"]):
        mode = prop.get("collision", "proxy")
        if mode == "none" or not prop.get("static", True):
            stats["propsSkipped"] += 1
            continue
        cell_id = prop["cell"]
        yaw = math.radians(float(prop.get("yawDeg", 0.0)))
        scale = float(prop.get("scale", 1.0))
        px, py, pz = (float(c) for c in prop["position"])
        surface = None
        material = prop.get("material")
        if material and material in materials:
            surface = materials[material].get("footstepSurface")

        path = asset_paths.get(prop["asset"])
        if path is None:
            raise LayoutError(
                f"prop {prop['id']!r} names asset {prop['asset']!r}, which is not in "
                f"assets.manifest.json")
        components: list[tuple[list, list]] = []
        for vertices, triangles in read_col_meshes(path):
            components.extend(split_components(vertices, triangles))
        if not components:
            raise LayoutError(
                f"prop {prop['id']!r} asks for collision {mode!r} but {path.name} has no "
                f"`<name>_COL` mesh (§18)")

        if mode == "box":
            everything = [v for component in components for v in component[0]]
            lo = [min(v[i] for v in everything) for i in range(3)]
            hi = [max(v[i] for v in everything) for i in range(3)]
            centre = tuple((lo[i] + hi[i]) / 2 for i in range(3))
            half = tuple((hi[i] - lo[i]) / 2 for i in range(3))
            _add(per_cell, cell_id, shapes.obb(
                _place(centre, px, py, pz, yaw, scale),
                tuple(h * scale for h in half), yaw, surface, KIND_PROP))
            stats["propObbs"] += 1
            continue

        for vertices, triangles in components:
            box = as_box(vertices, triangles)
            if box is not None:
                _add(per_cell, cell_id, shapes.obb(
                    _place(box[0], px, py, pz, yaw, scale),
                    tuple(h * scale for h in box[1]), yaw, surface, KIND_PROP))
                stats["propObbs"] += 1
            else:
                placed = [_place(v, px, py, pz, yaw, scale) for v in vertices]
                index = shapes.mesh(placed, triangles, surface, KIND_PROP)
                _add_mesh(per_cell, cell_id, index)
                stats["propMeshes"] += 1


def _place(point, px, py, pz, yaw, scale):
    """Local point -> world: scale, then yaw about Y, then translate."""
    x, y, z = (c * scale for c in point)
    c, s = math.cos(yaw), math.sin(yaw)
    return (px + x * c + z * s, py + y, pz - x * s + z * c)


# ============================================================================== the loose grid


#: The most 1 m buckets one cell's grid may have. `EXT_WORLD` is 400 x 400 = 160 000 of
#: them, so this is about six times the biggest thing this house contains.
MAX_GRID_BUCKETS = 1_000_000


def build_grid(bounds, shape_aabbs):
    """A 1 m grid in x/z over the cell, listing every shape whose AABB overlaps each bucket.

    x/z and not x/y/z: a cell is one storey tall, so a third axis would add buckets without
    dividing the shapes -- the floor slab alone spans every bucket of every vertical layer. A
    capsule sweep is mostly horizontal and the vertical reject is one comparison against the
    shape's own AABB.
    """
    x0, _y0, z0, x1, _y1, z1 = bounds
    nx = max(1, math.ceil((x1 - x0 - EPS) / GRID_CELL))
    nz = max(1, math.ceil((z1 - z0 - EPS) / GRID_CELL))
    # A cell's bounds are the union of its own shapes' AABBs, so one shape that escapes its cell
    # sizes the grid. `HOUSE-00472` found this the way you would expect: a clipping bug put a
    # rafter a long way from the house and this function sat allocating buckets until the process
    # was killed. The largest legitimate grid in this house is `EXT_WORLD`'s 400 x 400.
    if nx * nz > MAX_GRID_BUCKETS:
        raise LayoutError(
            f"a cell's collision grid would be {nx} x {nz} = {nx * nz} buckets over "
            f"{x1 - x0:.1f} x {z1 - z0:.1f} m; a shape has escaped its cell")
    buckets: list[list[int]] = [[] for _ in range(nx * nz)]
    for index, (ax0, _ay0, az0, ax1, _ay1, az1) in enumerate(shape_aabbs):
        # The EPS matches the interval-overlap test the selftest checks this against: a shape
        # whose AABB stops exactly ON a bucket's edge does not overlap that bucket, and it must
        # not land there through a floating-point crumb either. `(5.85 - 3.85) / 1.0` is
        # 1.9999999999999996 in binary, which put a rafter in the bucket next door.
        i0 = max(0, min(nx - 1, int(math.floor((ax0 - x0) / GRID_CELL + EPS))))
        i1 = max(0, min(nx - 1, int(math.ceil((ax1 - x0) / GRID_CELL - EPS)) - 1))
        j0 = max(0, min(nz - 1, int(math.floor((az0 - z0) / GRID_CELL + EPS))))
        j1 = max(0, min(nz - 1, int(math.ceil((az1 - z0) / GRID_CELL - EPS)) - 1))
        for j in range(j0, max(j0, j1) + 1):
            for i in range(i0, max(i0, i1) + 1):
                buckets[j * nx + i].append(index)
    return nx, nz, (x0, z0), buckets


# ========================================================================================== build


def build(world_dir: Path, manifest_path: Path | None = None) -> dict:
    """Read the layout, generate every shape, and return the world ready to be written."""
    layout = layout_io.load_layout(world_dir, ["levels", "cells", "portals"])
    # `exterior` joins them for `HOUSE-00774`: §11.2's fences, §11.4's kerbs and cars, §11.1's
    # garden structures and §49.2's tree trunks are all in it, and until this was read the only
    # thing outdoors that stopped a body was a cell boundary.
    for optional in ("stairs", "props", "materials", "exterior"):
        name, _ = layout_io.FILES[optional]
        if (world_dir / name).is_file():
            layout[optional] = layout_io.load_file(world_dir / name, optional)

    asset_paths: dict[str, Path] = {}
    if manifest_path and manifest_path.is_file():
        for row in layout_io.load_file(manifest_path, "assets").get("assets", []):
            source = row.get("sourceFile")
            if source:
                asset_paths[row["id"]] = (REPO / source)

    stats = {"wallPieces": 0, "floorPieces": 0, "ceilingPieces": 0, "stairMeshes": 0, "stairSteps": 0, "stairLandings": 0,
             "stairsGuessed": 0, "rafterMeshes": 0, "rafterArea": 0.0, "rafterAboveCeiling": 0, "guards": 0, "stairGuards": 0, "outerShared": 0,
             "openingShared": 0, "openBoundaries": 0, "fencePieces": 0, "kerbPieces": 0,
             "structureObbs": 0, "trunks": 0, "vehicles": 0, "hedges": 0,
             "propObbs": 0, "propMeshes": 0, "propsSkipped": 0}
    shapes = Shapes()
    per_cell = build_shell(layout, shapes, stats)
    build_stairs(layout, shapes, per_cell, stats)
    build_stairwell_guards(layout, shapes, per_cell, stats)
    build_rafters(layout, shapes, per_cell, stats)
    build_mezzanine_guards(layout, shapes, per_cell, stats)
    build_props(layout, shapes, per_cell, asset_paths, stats)
    build_exterior(layout, shapes, per_cell, stats, world_dir)

    obb_count = len(shapes.obbs)
    _resolve_meshes(per_cell, obb_count)
    aabbs = [obb_aabb(o) for o in shapes.obbs] + [mesh_aabb(m) for m in shapes.meshes]
    # Last, because it needs every shape's final index and every cell's finished list: what a body
    # can reach through a doorway includes the props and the stair wedges the builders above put
    # on the other side of it.
    reachable = share_through_openings(layout, per_cell, aabbs, stats)

    # Which cells §11.5's ground belongs to (`HOUSE-00774`). §49.2 says exterior collision is
    # *"the terrain height field plus OBBs"*, and the height field is one surface over the whole
    # lot -- including the ground the house's basement is under. A body on the basement stair is
    # 0.1 m from that surface and must not be pushed by it, so the file says which cells are the
    # open outdoors and the runtime asks the ground only there.
    outdoor_ids = {cell_id for cell_id, _boxes in _exterior_cells(layout)}
    outdoor_ids.add("EXT_WORLD")

    cells = []
    for cell_id in sorted(per_cell):
        indices = per_cell[cell_id]
        if len(indices) > MAX_PER_CELL:
            raise LayoutError(
                f"cell {cell_id!r} has {len(indices)} collision shapes; the format's u16 bucket "
                f"indices address {MAX_PER_CELL}")
        local = [union_aabb(reachable[(cell_id, i)]) if (cell_id, i) in reachable else aabbs[i]
                 for i in indices]
        bounds = (min(a[0] for a in local), min(a[1] for a in local), min(a[2] for a in local),
                  max(a[3] for a in local), max(a[4] for a in local), max(a[5] for a in local))
        nx, nz, origin, buckets = build_grid(bounds, local)
        cells.append({"id": cell_id, "shapes": indices, "bounds": bounds, "nx": nx, "nz": nz,
                      "origin": origin, "buckets": buckets,
                      "outdoors": cell_id in outdoor_ids})

    references = sum(len(c["shapes"]) for c in cells)
    occupied = [len(b) for c in cells for b in c["buckets"] if b]
    stats.update({
        "obbs": obb_count,
        "meshes": len(shapes.meshes),
        "shapes": obb_count + len(shapes.meshes),
        "references": references,
        "pooledAway": references - (obb_count + len(shapes.meshes)),
        "duplicateObbs": shapes.duplicate_obbs,
        "meanBucketOccupancy": round(sum(occupied) / len(occupied), 2) if occupied else 0.0,
        "maxBucketOccupancy": max(occupied) if occupied else 0,
    })
    terrain = _terrain(world_dir, shapes, stats)
    return {"shapes": shapes, "cells": cells, "stats": stats, "terrain": terrain,
            "borrowed": reachable, "worldHash": _world_hash(world_dir)}


def _terrain(world_dir: Path, shapes: Shapes, stats: dict) -> dict | None:
    """§11.5's height field, read back from the committed PNGs, or `None` when there is none.

    **The ground goes in this file rather than being decoded at runtime.** `terrain.png` is a
    16-bit PNG, which the XNA-only runtime has no way to read: `Texture2D::FromStream` needs a
    `GraphicsDevice` (so it could not run in a headless physics test) and would hand back 8-bit
    colour anyway. Putting the samples here costs 26 KB in a 700 KB file and means the collider
    reads what everything else reads.

    They are read from the PNG rather than recomputed with `terrain_gen.fields()`, so that what the
    game collides with is the ground that is COMMITTED, 16-bit quantisation and all.
    """
    if not (world_dir / "terrain.png").is_file():
        return None
    width, height, heights, materials = terrain_gen.decode(world_dir)
    table = [shapes.surface(name) for name in terrain_gen.MATERIALS]
    stats["terrainSamples"] = width * height
    stats["terrainMin"] = round(min(heights), 3)
    stats["terrainMax"] = round(max(heights), 3)
    return {
        "samplesX": width,
        "samplesZ": height,
        "originX": terrain_gen.ORIGIN_X,
        "originZ": terrain_gen.ORIGIN_Z,
        "step": terrain_gen.STEP,
        "heights": heights,
        "materials": table,
        "materialIndex": materials,
    }


def _world_hash(world_dir: Path) -> str:
    """`world.manifest.json`'s `worldHash`, so the runtime can reject a stale binary.

    The manifest is written by `deploy_world.py` into `content/world/` (`HOUSE-00364`), NOT beside
    the authored source, so looking only in @p world_dir found nothing and stamped every file
    built from `assets-src/world` with an empty hash. Nothing read the field until `HOUSE-00474`
    put a C++ reader behind it, which is exactly how a write-only field goes wrong.

    An empty string is still a legal answer -- a fixture world has no manifest and does not need
    one -- and it means the staleness check cannot run, which the report says out loud.
    """
    for path in (world_dir / "world.manifest.json", REPO / "content" / "world" / "world.manifest.json"):
        if path.is_file():
            return str(layout_io.load_file(path, "manifest").get("worldHash", ""))
    return ""


# ========================================================================================= writer


def _string(text: str) -> bytes:
    raw = text.encode("utf-8")
    if len(raw) > 0xFFFF:
        raise LayoutError(f"string of {len(raw)} bytes exceeds the format's u16 length")
    return struct.pack("<H", len(raw)) + raw


def serialise(world: dict) -> bytes:
    """The bytes of `collision.bin`, exactly as `docs/collision-format.md` describes them."""
    shapes: Shapes = world["shapes"]
    out = bytearray()
    out += MAGIC
    out += struct.pack("<II", VERSION, 0)
    out += _string(world["worldHash"])
    out += struct.pack("<f", GRID_CELL)

    out += struct.pack("<I", len(shapes.surfaces))
    for name in shapes.surfaces:
        out += _string(name)

    out += struct.pack("<I", len(shapes.obbs))
    for (cx, cy, cz), (hx, hy, hz), yaw, surface, kind in shapes.obbs:
        out += struct.pack("<7fHB", cx, cy, cz, hx, hy, hz, yaw, surface, kind)

    out += struct.pack("<I", len(shapes.meshes))
    for mesh in shapes.meshes:
        aabb = mesh_aabb(mesh)
        out += struct.pack("<HB", mesh["surface"], mesh["kind"])
        out += struct.pack("<6f", *aabb)
        out += struct.pack("<I", len(mesh["vertices"]))
        for vertex in mesh["vertices"]:
            out += struct.pack("<3f", *vertex)
        out += struct.pack("<I", len(mesh["triangles"]))
        for triangle in mesh["triangles"]:
            out += struct.pack("<3H", *triangle)

    out += struct.pack("<I", len(world["cells"]))
    for cell in world["cells"]:
        out += _string(cell["id"])
        out += struct.pack("<B", 1 if cell.get("outdoors") else 0)
        out += struct.pack("<6f", *cell["bounds"])
        out += struct.pack("<I", len(cell["shapes"]))
        for index in cell["shapes"]:
            out += struct.pack("<I", index)
        out += struct.pack("<II", cell["nx"], cell["nz"])
        out += struct.pack("<2f", *cell["origin"])
        local = {global_index: i for i, global_index in enumerate(cell["shapes"])}
        for bucket in cell["buckets"]:
            out += struct.pack("<H", len(bucket))
            for entry in bucket:
                out += struct.pack("<H", local[cell["shapes"][entry]])

    terrain = world.get("terrain")
    out += struct.pack("<B", 1 if terrain else 0)
    if terrain:
        out += struct.pack("<II", terrain["samplesX"], terrain["samplesZ"])
        out += struct.pack("<3f", terrain["originX"], terrain["originZ"], terrain["step"])
        for value in terrain["heights"]:
            out += struct.pack("<f", value)
        out += struct.pack("<I", len(terrain["materials"]))
        for index in terrain["materials"]:
            out += struct.pack("<H", index)
        out += bytes(terrain["materialIndex"])
    return bytes(out)


def read_back(data: bytes) -> dict:
    """Parse the format again, forward, once -- the round-trip the selftest checks against."""
    view = memoryview(data)
    at = 0

    def take(n):
        nonlocal at
        chunk = view[at:at + n]
        if len(chunk) != n:
            raise LayoutError(f"collision.bin truncated at byte {at}, wanted {n} more")
        at += n
        return chunk

    def unpack(fmt):
        return struct.unpack(fmt, take(struct.calcsize(fmt)))

    def text():
        (length,) = unpack("<H")
        return bytes(take(length)).decode("utf-8")

    if bytes(take(4)) != MAGIC:
        raise LayoutError("not a collision.bin: bad magic")
    version, flags = unpack("<II")
    if version != VERSION:
        raise LayoutError(f"collision.bin is version {version}; this reader knows {VERSION}")
    if flags:
        raise LayoutError(f"collision.bin sets unknown flag bits {flags:#x}")
    world_hash = text()
    (grid_cell,) = unpack("<f")

    (surface_count,) = unpack("<I")
    surfaces = [text() for _ in range(surface_count)]

    (obb_count,) = unpack("<I")
    obbs = []
    for _ in range(obb_count):
        cx, cy, cz, hx, hy, hz, yaw, surface, kind = unpack("<7fHB")
        obbs.append({"centre": (cx, cy, cz), "half": (hx, hy, hz), "yaw": yaw,
                     "surface": surfaces[surface], "kind": KIND_NAMES[kind]})

    (mesh_count,) = unpack("<I")
    meshes = []
    for _ in range(mesh_count):
        surface, kind = unpack("<HB")
        bounds = unpack("<6f")
        (vertex_count,) = unpack("<I")
        vertices = [unpack("<3f") for _ in range(vertex_count)]
        (triangle_count,) = unpack("<I")
        triangles = [unpack("<3H") for _ in range(triangle_count)]
        meshes.append({"surface": surfaces[surface], "kind": KIND_NAMES[kind], "bounds": bounds,
                       "vertices": vertices, "triangles": triangles})

    (cell_count,) = unpack("<I")
    cells = []
    for _ in range(cell_count):
        cell_id = text()
        (outdoors,) = unpack("<B")
        bounds = unpack("<6f")
        (shape_count,) = unpack("<I")
        indices = [unpack("<I")[0] for _ in range(shape_count)]
        nx, nz = unpack("<II")
        origin = unpack("<2f")
        buckets = []
        for _ in range(nx * nz):
            (count,) = unpack("<H")
            buckets.append([unpack("<H")[0] for _ in range(count)])
        cells.append({"id": cell_id, "outdoors": bool(outdoors), "bounds": bounds,
                      "shapes": indices, "nx": nx, "nz": nz, "origin": origin,
                      "buckets": buckets})

    (has_terrain,) = unpack("<B")
    terrain = None
    if has_terrain:
        samples_x, samples_z = unpack("<II")
        origin_x, origin_z, step = unpack("<3f")
        count = samples_x * samples_z
        heights = list(struct.unpack(f"<{count}f", take(4 * count)))
        (material_count,) = unpack("<I")
        materials = [surfaces[unpack("<H")[0]] for _ in range(material_count)]
        index = list(bytes(take(count)))
        terrain = {"samplesX": samples_x, "samplesZ": samples_z, "originX": origin_x,
                   "originZ": origin_z, "step": step, "heights": heights,
                   "materials": materials, "materialIndex": index}

    if at != len(data):
        raise LayoutError(f"{len(data) - at} bytes left over after the terrain")
    return {"worldHash": world_hash, "gridCell": grid_cell, "surfaces": surfaces,
            "obbs": obbs, "meshes": meshes, "cells": cells, "terrain": terrain}


# ========================================================================================= report


def report(world: dict) -> str:
    stats = world["stats"]
    shapes: Shapes = world["shapes"]
    kinds = {name: 0 for name in KIND_NAMES}
    for record in shapes.obbs:
        kinds[KIND_NAMES[record[4]]] += 1
    for mesh in shapes.meshes:
        kinds[KIND_NAMES[mesh["kind"]]] += 1
    lines = [
        f"{stats['shapes']} shapes: {stats['obbs']} OBBs, {stats['meshes']} triangle "
        f"mesh{'' if stats['meshes'] == 1 else 'es'}",
        f"  by kind: " + ", ".join(f"{k} {v}" for k, v in kinds.items() if v),
        f"  {stats['references']} cell references over {stats['shapes']} shapes "
        f"({stats['pooledAway']} copies avoided by pooling)",
        f"  {stats['duplicateObbs']} identical OBBs collapsed while building",
        f"  {len(world['cells'])} cells, mean bucket occupancy "
        f"{stats['meanBucketOccupancy']} shapes, worst {stats['maxBucketOccupancy']}",
        f"  guards: {stats['guards']} at drops over {GUARD_DROP:.1f} m, which §70.5 asks for "
        f"and the layout has no way to state",
        f"  stair rails: {stats['stairGuards']} round the wells, §12.3's 0.95 m balustrade, open "
        f"where a flight climbs from that floor ({stats['outerShared']} outer wall pieces shared "
        f"with the yard on the other side of them)",
        f"  rafters: {stats['rafterMeshes']} clipped roof planes over "
        f"{stats['rafterArea']:.1f} m² of plan, {stats['rafterAboveCeiling']} dropped as "
        f"unreachable above a flat ceiling",
        f"  stairs: {stats['stairMeshes']} ramp wedges, {stats['stairLandings']} landings, "
        f"{stats['stairSteps']} stepped OBBs; {stats['stairsGuessed']} flight(s) placed by guess "
        f"for want of an authored footprint",
        f"  props: {stats['propObbs']} OBBs, {stats['propMeshes']} meshes, "
        f"{stats['propsSkipped']} without collision",
        f"  openings: {stats['openingShared']} shape(s) shared across a hole, each carried "
        f"{OPENING_REACH:.2f} m past the plane -- a body in a doorway is in both rooms",
        f"  outdoors: {stats['fencePieces']} fence piece(s), {stats['kerbPieces']} kerb, "
        f"{stats['structureObbs']} garden structure(s), {stats['trunks']} tree trunk(s), "
        f"{stats['hedges']} hedge section(s) and {stats['vehicles']} vehicle(s); "
        f"{stats['openBoundaries']} boundary between two open yards left as grass",
    ]
    terrain = world.get("terrain")
    if terrain:
        lines.append(
            f"  terrain: {terrain['samplesX']} x {terrain['samplesZ']} samples "
            f"{terrain['step']:.1f} m apart, {stats['terrainMin']:.2f} to "
            f"{stats['terrainMax']:.2f} m, {len(terrain['materials'])} materials")
    else:
        lines.append("  terrain: none in this layout")
    return "\n".join(lines)


# ======================================================================================= selftest


def fixture_world() -> dict:
    """A tiny collision world whose every number is stated here and asserted in C++.

    `HOUSE-00541`. `CollisionLoaderTests` builds its bytes by hand, which makes it an excellent
    test of the reader and no test at all of the reader and the WRITER agreeing: a writer that
    emitted `halfExtents` before `centre`, or a bucket's entries as GLOBAL indices, would pass
    every test in this repository and stop the player in the wrong place. `HOUSE-00225` is the
    standing lesson -- a reader and a writer each tested against their own hand-written fixtures,
    both passing, producing and expecting different bytes.

    Deliberately asymmetric: a yaw that is not zero, half-extents that differ on all three axes,
    two surfaces, an OBB and a mesh with DIFFERENT kinds, a 2 x 3 grid so `nx` and `nz` cannot be
    swapped unnoticed, and buckets holding two shapes, one shape and none.
    """
    shapes = Shapes()
    tile = shapes.surface("tile")
    wood = shapes.surface("wood")
    shapes.obbs.append(((1.0, 0.5, 2.0), (0.25, 0.5, 1.5), 0.7853982, tile, KIND_FLOOR))
    shapes.obbs.append(((-1.0, 1.25, 0.5), (0.1, 1.25, 2.0), 0.0, wood, KIND_WALL))
    shapes.mesh([(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (1.0, 0.75, 0.0), (0.0, 0.75, 2.5)],
                [(0, 1, 2), (0, 2, 3)], "wood", KIND_STAIR)

    cell = {
        "id": "L0_FIXTURE",
        "bounds": (-1.1, 0.0, -1.5, 1.25, 2.5, 2.5),
        "shapes": [0, 1, 2],
        "nx": 2, "nz": 3,
        "origin": (-1.1, -1.5),
        "buckets": [[0, 1], [], [], [2], [], []],
    }
    # A 3 x 2 height field, deliberately asymmetric so `samplesX` and `samplesZ` cannot be
    # swapped unnoticed, with a step that is not the world grid's 1.0 and heights that are not a
    # plane. Two materials, the second of which is NOT the first surface in the table, so a reader
    # that ignored the material table would produce `tile` for the lot.
    grass = shapes.surface("grass")
    gravel = shapes.surface("gravel")
    terrain = {
        "samplesX": 3, "samplesZ": 2,
        "originX": -1.0, "originZ": -2.0, "step": 2.0,
        "heights": [0.0, 0.25, 0.5, 1.0, 1.25, 2.0],
        "materials": [grass, gravel],
        "materialIndex": [0, 0, 1, 1, 0, 1],
    }
    return {"shapes": shapes, "cells": [cell], "terrain": terrain,
            "worldHash": "0123456789abcdef0123456789abcdef",
            "stats": {"obbs": len(shapes.obbs), "meshes": len(shapes.meshes),
                      "shapes": len(shapes.obbs) + len(shapes.meshes)}}


def _fixture_world(directory: Path) -> None:
    """A layout small enough to reason about and large enough to exercise every rule.

    `L0_LOUNGE` is L-shaped -- two boxes -- so rule 3 has something to prove. `L0_HALL` abuts it
    across a plane carrying a door portal and a window portal, so a wall is punched twice in two
    different ways. `L0_GARAGE` abuts the hall so the garage thickness is chosen. The east side of
    the lounge faces nothing, so it is exterior; part of the hall's east side faces the garage and
    part faces outdoors, so that side must segment.
    """
    write = lambda name, doc: (directory / name).write_text(  # noqa: E731
        json.dumps(doc, indent=2) + "\n", encoding="utf-8")

    write("world.manifest.json", {
        "schema": "cna-house/manifest/1", "worldHash": "sha256:fixture", "members": []})
    write("layout.levels.json", {
        "schema": "cna-house/levels/1",
        "levels": [{"id": "L0", "name": "Main", "ffl": 0.0, "ceiling": 2.5,
                    "structureDepth": 0.30}],
        "construction": {"wallExterior": 0.30, "wallPartition": 0.15, "wallPlumbing": 0.20,
                         "wallGarage": 0.25, "foundationWall": 0.30},
    })
    write("layout.cells.json", {
        "schema": "cna-house/cells/1",
        "cells": [
            {"id": "L0_LOUNGE", "level": "L0", "kind": "room",
             "boxes": [{"x": [0.0, 4.0], "z": [0.0, 3.0]},
                       {"x": [0.0, 2.0], "z": [3.0, 5.0]}],
             "footstepSurface": "wood", "wallMaterial": "MAT_PAINT",
             "ceilingMaterial": "MAT_CEIL"},
            {"id": "L0_HALL", "level": "L0", "kind": "corridor",
             "boxes": [{"x": [4.0, 6.0], "z": [0.0, 5.0]}],
             "footstepSurface": "tile", "wallMaterial": "MAT_PAINT",
             "ceilingMaterial": "MAT_CEIL"},
            {"id": "L0_GARAGE", "level": "L0", "kind": "garage",
             "boxes": [{"x": [6.0, 9.0], "z": [0.0, 3.0]}],
             "footstepSurface": "concrete", "wallMaterial": "MAT_PAINT",
             "ceilingMaterial": "MAT_CEIL"},
        ],
    })
    write("layout.portals.json", {
        "schema": "cna-house/portals/1",
        "portals": [
            {"id": "P_L0_LOUNGE__L0_HALL", "cellA": "L0_LOUNGE", "cellB": "L0_HALL",
             "plane": {"axis": "x", "value": 4.0},
             "rect": {"u": [1.0, 1.9], "v": [0.0, 2.04]},
             "kind": "door", "opacity": "opaque_when_closed"},
            {"id": "P_L0_HALL__WINDOW", "cellA": "L0_HALL", "cellB": "L0_LOUNGE",
             "plane": {"axis": "x", "value": 4.0},
             "rect": {"u": [2.4, 3.4], "v": [0.9, 2.1]},
             "kind": "window", "opacity": "glass"},
        ],
    })
    write("layout.stairs.json", {
        "schema": "cna-house/stairs/1",
        "flights": [{"id": "STAIR_L0_UP", "fromCell": "L0_HALL", "toCell": "L0_HALL",
                     "risers": 8, "rise": 0.18, "going": 0.28, "width": 1.0,
                     "landings": [], "collisionRamp": True, "surface": "wood"}],
    })


def _fixture_proxy(path: Path, boxes) -> None:
    """A `.glb` holding one `PROP_COL` node whose mesh is `boxes` axis-aligned boxes."""
    positions: list[float] = []
    indices: list[int] = []
    for (cx, cy, cz), (hx, hy, hz) in boxes:
        base = len(positions) // 3
        for sx in (-1, 1):
            for sy in (-1, 1):
                for sz in (-1, 1):
                    positions += [cx + sx * hx, cy + sy * hy, cz + sz * hz]
        # 0=---,1=--+,2=-+-,3=-++,4=+--,5=+-+,6=++-,7=+++
        faces = [(0, 1, 3), (0, 3, 2), (4, 6, 7), (4, 7, 5), (0, 4, 5), (0, 5, 1),
                 (2, 3, 7), (2, 7, 6), (0, 2, 6), (0, 6, 4), (1, 5, 7), (1, 7, 3)]
        for a, b, c in faces:
            indices += [base + a, base + b, base + c]

    blob = bytearray()
    position_offset = len(blob)
    blob += struct.pack(f"<{len(positions)}f", *positions)
    index_offset = len(blob)
    blob += struct.pack(f"<{len(indices)}H", *indices)
    while len(blob) % 4:
        blob += b"\0"
    lo = [min(positions[i::3]) for i in range(3)]
    hi = [max(positions[i::3]) for i in range(3)]
    document = {
        "asset": {"version": "2.0"},
        "scenes": [{"nodes": [0]}], "scene": 0,
        "nodes": [{"name": "PROP_COL", "mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1, "mode": 4}]}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions) // 3,
             "type": "VEC3", "min": lo, "max": hi},
            {"bufferView": 1, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": position_offset, "byteLength": len(positions) * 4},
            {"buffer": 0, "byteOffset": index_offset, "byteLength": len(indices) * 2},
        ],
        "buffers": [{"byteLength": len(blob)}],
    }
    gltf_io.write_glb(path, document, bytes(blob))


def selftest() -> int:
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("build_collision: selftest")
    workspace = Path(tempfile.mkdtemp(prefix="build_collision_selftest_"))
    try:
        world_dir = workspace / "world"
        world_dir.mkdir()
        _fixture_world(world_dir)

        # 1. Rectangle subtraction, on its own, against answers that can be counted by hand.
        whole = (0.0, 0.0, 3.0, 2.5)
        door = subtract_rects(whole, [(1.0, 0.0, 1.9, 2.04)])
        require(len(door) == 3,
                f"a doorway reaching the floor leaves 3 pieces -- two jambs and a header "
                f"(got {len(door)})")
        window = subtract_rects(whole, [(1.0, 0.9, 1.9, 2.1)])
        require(len(window) == 4,
                f"a window leaves 4 -- two jambs, a sill and a header (got {len(window)})")
        require(abs(sum((r[2] - r[0]) * (r[3] - r[1]) for r in window)
                    - (3.0 * 2.5 - 0.9 * 1.2)) < 1e-9,
                "...and the pieces' area is the wall's area minus the opening's, exactly")
        require(subtract_rects(whole, [(-1.0, -1.0, 4.0, 4.0)]) == [],
                "a hole that covers the wall leaves nothing")
        require(subtract_rects(whole, [(5.0, 0.0, 6.0, 1.0)]) == [whole],
                "a hole that misses the wall leaves it whole and unsplit")

        # 1b. The vertical merge, which nothing above reaches: two openings stacked in the same
        #     u range -- a door with a transom over it -- put a grid line across the wall that
        #     changes nothing. Unmerged that is four pieces where three are correct, and the
        #     spurious horizontal join is a seam a lightmap will find.
        stacked = subtract_rects((0.0, 0.0, 3.0, 3.0),
                                 [(1.0, 0.0, 2.0, 1.0), (1.0, 1.0, 2.0, 2.0)])
        require(len(stacked) == 3,
                f"two openings stacked with no gap merge into 3 pieces, not 4 (got "
                f"{len(stacked)})")
        require(sorted(round(r[3] - r[1], 6) for r in stacked) == [1.0, 2.0, 2.0],
                f"...and the two jambs are one 2 m piece each, not two 1 m pieces "
                f"({sorted(round(r[3] - r[1], 6) for r in stacked)})")

        # 2. The L-shaped room has no wall across the middle of itself. This is the failure that
        #    looks correct in every render and stops the player in the middle of the lounge.
        world = build(world_dir)
        lounge = next(c for c in world["cells"] if c["id"] == "L0_LOUNGE")
        shapes: Shapes = world["shapes"]
        # What the cell OWNS: a shape borrowed through one of its own holes (`HOUSE-00568`) is
        # the room next door's, and these claims are about what this room generates.
        owned = [i for i in lounge["shapes"] if ("L0_LOUNGE", i) not in world["borrowed"]]
        divider = [
            i for i in owned
            if i < len(shapes.obbs) and shapes.obbs[i][4] == KIND_WALL
            and abs(shapes.obbs[i][0][2] - 3.0) < 0.01
            and shapes.obbs[i][0][0] < 2.0
        ]
        require(divider == [],
                f"no wall is generated across the L-shaped lounge's own internal edge "
                f"(found {len(divider)})")
        require(any(i < len(shapes.obbs) and shapes.obbs[i][4] == KIND_WALL
                    and abs(shapes.obbs[i][0][2] - 3.0) < 0.01
                    and shapes.obbs[i][0][0] > 2.0
                    for i in owned),
                "...but the part of that same edge facing outdoors IS a wall")

        # 3. The wall carrying a door and a window is punched by both.
        shared = [i for i in owned
                  if i < len(shapes.obbs) and shapes.obbs[i][4] == KIND_WALL
                  and abs(shapes.obbs[i][0][0] - 4.0) < 0.01]
        require(len(shared) == 6,
                f"the lounge/hall wall becomes 6 pieces around a door and a window "
                f"(got {len(shared)})")
        heights = [shapes.obbs[i][0][1] for i in shared]
        require(any(h > 2.2 for h in heights),
                "...including a header above the door")

        # 4. The thickness of a segment is chosen by what is on the other side of it.
        def thickness_at(cell_id, axis_index, value, u_index, u_value):
            cell = next(c for c in world["cells"] if c["id"] == cell_id)
            for i in cell["shapes"]:
                if i >= len(shapes.obbs) or shapes.obbs[i][4] != KIND_WALL:
                    continue
                centre, half, _, _, _ = shapes.obbs[i]
                if abs(centre[axis_index] - value) < 0.01 and \
                        abs(centre[u_index] - u_value) < 0.6:
                    return half[axis_index] * 2
            return None

        require(abs((thickness_at("L0_HALL", 0, 6.0, 2, 1.5) or 0) - 0.25) < 1e-6,
                f"the hall/garage wall is the garage thickness, 0.25 "
                f"(got {thickness_at('L0_HALL', 0, 6.0, 2, 1.5)})")
        require(abs((thickness_at("L0_HALL", 0, 6.0, 2, 4.0) or 0) - 0.30) < 1e-6,
                f"the same side, where it faces outdoors instead, is exterior, 0.30 "
                f"(got {thickness_at('L0_HALL', 0, 6.0, 2, 4.0)}) -- one side, two thicknesses")

        # 5. A wall is centred on the plane, because the two cells share it exactly.
        wall = shapes.obbs[shared[0]]
        require(abs(wall[0][0] - 4.0) < 1e-6,
                "a shared wall's centre is ON the boundary plane, not offset into one room")

        # 6. Pooling. The lounge/hall wall pieces are referenced by both cells and stored once.
        hall = next(c for c in world["cells"] if c["id"] == "L0_HALL")
        both = set(lounge["shapes"]) & set(hall["shapes"])
        require(len(both) >= 6,
                f"the lounge and the hall reference the same shapes for their shared wall "
                f"({len(both)} shared)")
        require(world["stats"]["pooledAway"] > 0,
                f"pooling avoids {world['stats']['pooledAway']} copies "
                f"({world['stats']['references']} references over "
                f"{world['stats']['shapes']} shapes)")

        # 6b. An OPEN cell has a floor, no lid, and no wall facing outdoors. Building one anyway
        #     roofs the terrace over, and `HOUSE-00212` and `HOUSE-00213` both then report a
        #     sheltered, sky-less patio -- which is how this rule was found, three tools later.
        terrace = json.loads((world_dir / "layout.cells.json").read_text())
        terrace["cells"].append({
            "id": "L0_TERRACE", "level": "L0", "kind": "exterior",
            "boxes": [{"x": [-3.0, 0.0], "z": [0.0, 3.0]}],
            "footstepSurface": "concrete", "wallMaterial": "MAT_PAINT",
            "ceilingMaterial": "MAT_CEIL"})
        (world_dir / "layout.cells.json").write_text(
            json.dumps(terrace, indent=2) + "\n", encoding="utf-8")
        outdoor = build(world_dir)
        outdoor_shapes: Shapes = outdoor["shapes"]
        patio = next(c for c in outdoor["cells"] if c["id"] == "L0_TERRACE")
        kinds = [outdoor_shapes.obbs[i][4] for i in patio["shapes"]
                 if i < len(outdoor_shapes.obbs)]
        require(KIND_CEILING not in kinds,
                f"an exterior cell gets no ceiling slab -- it is open to the sky "
                f"({[KIND_NAMES[k] for k in kinds]})")
        require(KIND_FLOOR in kinds, "...but it does get a floor; a terrace is a real surface")
        walls = [outdoor_shapes.obbs[i] for i in patio["shapes"]
                 if i < len(outdoor_shapes.obbs) and outdoor_shapes.obbs[i][4] == KIND_WALL]
        require(len(walls) == 1 and abs(walls[0][0][0] - 0.0) < 1e-6,
                f"...and exactly one wall, the one it shares with the lounge at x = 0, not four "
                f"({len(walls)} walls at "
                f"{[round(w[0][0], 2) for w in walls]})")

        # 7. The stair is a CLOSED prism, not a walking surface. An open surface is a floor the
        #    player falls through from below.
        stair_meshes = [m for m in shapes.meshes if m["kind"] == KIND_STAIR]
        require(len(stair_meshes) == 1, f"the flight makes one wedge (got {len(stair_meshes)})")
        edges: dict[tuple[int, int], int] = {}
        for a, b, c in stair_meshes[0]["triangles"]:
            for x, y in ((a, b), (b, c), (c, a)):
                edges[(min(x, y), max(x, y))] = edges.get((min(x, y), max(x, y)), 0) + 1
        require(all(count == 2 for count in edges.values()),
                "...and every edge is shared by exactly two triangles, so the wedge is closed")

        # 7b. A flight with a half-landing is TWO wedges and one landing box, not three wedges.
        #     `HOUSE-00347` found the third: the riser a run starts on is the riser the landing
        #     below it ended on, so without the `run == 0` guard every run after a landing was
        #     exactly one riser long. The geometry was still continuous, which is why nothing here
        #     caught it -- so the claim is about the COUNT and about the risers adding up.
        source = layout_io.load_layout(world_dir, ["levels", "cells", "stairs"])
        flight = dict(source["stairs"]["flights"][0], id="STAIR_LANDED", risers=10,
                      landings=[{"at": 5, "depth": 1.0}])
        with_landing = dict(source)
        with_landing["stairs"] = {"schema": "cna-house/stairs/1", "flights": [flight]}
        shapes_landed, per_cell_landed = Shapes(), {}
        stats_landed = {"stairMeshes": 0, "stairSteps": 0, "stairLandings": 0,
                        "stairsGuessed": 0}
        build_stairs(with_landing, shapes_landed, per_cell_landed, stats_landed)
        require(stats_landed["stairMeshes"] == 2,
                f"a flight with one half-landing makes TWO wedges "
                f"({stats_landed['stairMeshes']})")
        rise = float(flight["rise"])
        heights = sorted(round(max(v[1] for v in m["vertices"])
                               - min(v[1] for v in m["vertices"]), 6)
                         for m in shapes_landed.meshes if m["kind"] == KIND_STAIR)
        require(heights == [round(5 * rise, 6), round(5 * rise, 6)],
                f"...five risers each, so the ten add up ({heights} vs {round(5 * rise, 6)})")
        require(stats_landed["stairsGuessed"] == 1,
                "and the fixture flight authors no footprint, so it is COUNTED as guessed rather "
                "than silently invented")

        # 7c. `HOUSE-00472`: the authored house. Where a flight is comes from `stair_geometry`, so
        #     the thing you collide with is the thing `house_shell_gen.py` draws. The fixture
        #     cannot show this -- it has no footprint -- so this is claimed against the real
        #     `layout.stairs.json`, which is data this repository owns.
        authored = Path(__file__).resolve().parents[2] / "assets-src" / "world"
        if (authored / "layout.stairs.json").is_file():
            real = layout_io.load_layout(authored, ["levels", "cells", "stairs"])
            main = [f for f in layout_io.rows(real, "stairs") if f["id"] == "STAIR_MAIN_L0_L1"][0]
            real["stairs"] = {"schema": "cna-house/stairs/1", "flights": [main]}
            shapes_u, per_cell_u = Shapes(), {}
            stats_u = {"stairMeshes": 0, "stairSteps": 0, "stairLandings": 0, "stairsGuessed": 0}
            build_stairs(real, shapes_u, per_cell_u, stats_u)
            require(stats_u["stairsGuessed"] == 0,
                    "the main stair is placed from its authored footprint, not guessed")
            require(stats_u["stairMeshes"] == 2 and stats_u["stairLandings"] == 1,
                    f"a `u` is two wedges and one landing box "
                    f"({stats_u['stairMeshes']}, {stats_u['stairLandings']})")
            wedges = [m for m in shapes_u.meshes if m["kind"] == KIND_STAIR]
            spans = sorted((min(v[0] for v in m["vertices"]), max(v[0] for v in m["vertices"]))
                           for m in wedges)
            require(spans[0][1] <= spans[1][0] + 1e-6,
                    f"...whose two runs are side by side across the well, not through each other "
                    f"({spans})")
            # The defect this task fixed: the second run laid beyond the landing and climbing back
            # towards it put the flight's TOP tread against the half-landing, so you arrived a
            # whole storey early. The two runs must rise towards each other, not the same way.
            def rises_towards(mesh):
                low = min(v[1] for v in mesh["vertices"])
                at_low = [v[2] for v in mesh["vertices"] if abs(v[1] - low) < 1e-6]
                at_high = [v[2] for v in mesh["vertices"] if abs(v[1] - low) > 1e-6]
                return 1 if sum(at_high) / len(at_high) > sum(at_low) / len(at_low) else -1
            require(rises_towards(wedges[0]) == -rises_towards(wedges[1]),
                    "...and they climb in OPPOSITE directions, because a `u` turns through 180 "
                    "degrees on its landing")
            landing = [o for o in shapes_u.obbs if o[4] == KIND_STAIR][0]
            half = float(main["rise"]) * 9 + 0.60
            require(abs(landing[0][1] + float(main["rise"]) / 2 - half) < 1e-6,
                    f"the landing's walking surface is the top of the ninth riser, +{half:.4f} m "
                    f"({landing[0][1] + float(main['rise']) / 2:.4f})")
            treads = stair_geometry.flight_steps(main, 0.60)
            top = treads[-1]["box"]
            reached = [m for m in wedges
                       if min(v[0] for v in m["vertices"]) <= (top[0] + top[1]) / 2
                       <= max(v[0] for v in m["vertices"])]
            require(len(reached) == 1
                    and abs(max(v[1] for v in reached[0]["vertices"]) - treads[-1]["y1"]) < 1e-6,
                    "the wedge under the shell's top tread ends at exactly that tread's height, "
                    "so what you see and what you stand on are the same staircase")

        # 7d. `_wedge` must not mirror. All four travel directions exist in this house -- the
        #     garage steps run along X -- and a mirrored prism has its faces inside out, which is a
        #     solid the sweep reports as empty space.
        for axis, up in (("z", 1), ("z", -1), ("x", 1), ("x", -1)):
            vertices, triangles = _wedge((1.0, 2.0, 3.0, 4.5), 0.0, 0.5, axis, up)
            normals = []
            for a, b, c in triangles:
                pa, pb, pc = vertices[a], vertices[b], vertices[c]
                u = [pb[i] - pa[i] for i in range(3)]
                v = [pc[i] - pa[i] for i in range(3)]
                normals.append((u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                                u[0] * v[1] - u[1] * v[0]))
            centre = [sum(v[i] for v in vertices) / len(vertices) for i in range(3)]
            outward = 0
            for (a, _b, _c), normal in zip(triangles, normals):
                away = [vertices[a][i] - centre[i] for i in range(3)]
                outward += 1 if sum(normal[i] * away[i] for i in range(3)) > 0 else 0
            require(outward == len(triangles),
                    f"a wedge running {axis}{'+' if up > 0 else '-'} has all {len(triangles)} "
                    f"faces pointing OUT of itself ({outward})")
            require(abs(max(v[1] for v in vertices) - 0.5) < 1e-9
                    and len({(round(v[0], 6), round(v[1], 6), round(v[2], 6))
                             for v in vertices}) == 6,
                    f"...and is 0.5 m tall over six distinct corners ({axis}{up})")
        high = {}
        for axis, up in (("z", 1), ("z", -1), ("x", 1), ("x", -1)):
            vertices, _t = _wedge((1.0, 2.0, 3.0, 4.5), 0.0, 0.5, axis, up)
            index = 2 if axis == "z" else 0
            raised = [v[index] for v in vertices if v[1] > 0.25]
            high[(axis, up)] = round(sum(raised) / len(raised), 6)
        require(high[("z", 1)] > high[("z", -1)] and high[("x", 1)] > high[("x", -1)],
                f"and it rises the way it is told to: +1 puts the high end at the larger "
                f"coordinate ({high})")

        # 7e. `HOUSE-00472`: the rafter envelope and the guards, both of which exist because the
        #     layout CANNOT state them. Claimed against the authored house -- the fixture has no
        #     attic and no balcony, and inventing one here would be claiming about the fixture.
        authored = Path(__file__).resolve().parents[2] / "assets-src" / "world"
        if (authored / "layout.cells.json").is_file():
            house = build(authored)
            house_shapes: Shapes = house["shapes"]
            offset = len(house_shapes.obbs)
            references = {row["id"]: row["shapes"] for row in house["cells"]}
            rows = layout_io.load_layout(authored, ["levels", "cells"])
            house_levels = {row["id"]: row for row in layout_io.rows(rows, "levels")}
            house_cells = {row["id"]: row for row in layout_io.rows(rows, "cells")}
            build_rules = rows["levels"].get("construction") or {}

            require(house["stats"]["rafterMeshes"] > 0,
                    f"the attic gets a rafter envelope ({house['stats']['rafterMeshes']} pieces)")
            store = house_cells["L3_STORE_W"]
            head = layout_io.cell_extent(store, house_levels["L3"])[1]
            over_store = [house_shapes.meshes[index - offset]
                          for index in references["L3_STORE_W"] if index >= offset]
            lowest = min(vertex[1] for mesh in over_store for vertex in mesh["vertices"])
            require(over_store and lowest < head - 3.0,
                    f"and it bites: the lowest rafter over the west store is +{lowest:.2f}, "
                    f"{head - lowest:.2f} m below the flat +{head:.2f} lid its box alone would "
                    f"have given it")
            # Every square metre of the attic has roof over it. Anything less is a hole you walk
            # up through; anything more is a plane clipped to the wrong box.
            outer = roof_geometry.outer_box(roof_geometry.roof_boxes(rows)["ROOF_MAIN"],
                                            build_rules)
            eaves = roof_geometry.eaves_height(build_rules, outer)
            planes = roof_geometry.roof_planes(outer, eaves, float(build_rules["roofPitch"]))
            attic_area = covered = 0.0
            for cell in (row for row in house_cells.values() if row.get("level") == "L3"):
                for box in layout_io.cell_boxes(cell):
                    attic_area += (box[1] - box[0]) * (box[3] - box[2])
                    for corners, _out in planes:
                        covered += roof_geometry.plan_area(
                            roof_geometry.clip_to_rect(corners, box))
            require(abs(covered - attic_area) < 0.01 and attic_area > 250.0,
                    f"the four planes cover the attic's whole {attic_area:.1f} m² exactly once "
                    f"({covered:.1f} m²)")
            everywhere = {KIND_NAMES[house_shapes.meshes[index - offset]["kind"]]
                          for identifier, indices in references.items()
                          for index in indices
                          if index >= offset and house_cells[identifier].get("level") != "L3"
                          and (identifier, index) not in house["borrowed"]}
            require("ceiling" not in everywhere,
                    f"and no cell outside the attic gets one -- the garage has a flat ceiling at "
                    f"+4.30 and therefore a lid already ({sorted(everywhere)})")
            equations = [roof_geometry.plane_equation(corners) for corners, _out in planes]
            on_plane = all(any(abs(a * v[0] + b * v[2] + c - v[1]) < 1e-6
                               for a, b, c in equations)
                           for mesh in over_store for v in mesh["vertices"])
            require(on_plane,
                    "and every rafter vertex lies ON one of the roof's own planes, so the pieces "
                    "are the roof rather than something shaped like it")

            # A roof plane faces the sky. Winding a clipped piece backwards makes a rafter you
            # fall through from above and stand on from below, and nothing else here would say so.
            skyward = 0
            for mesh in over_store:
                for a, b, c in mesh["triangles"]:
                    pa, pb, pc = mesh["vertices"][a], mesh["vertices"][b], mesh["vertices"][c]
                    u = [pb[i] - pa[i] for i in range(3)]
                    v = [pc[i] - pa[i] for i in range(3)]
                    skyward += 1 if u[2] * v[0] - u[0] * v[2] > 0 else -1
            require(skyward == sum(len(mesh["triangles"]) for mesh in over_store),
                    f"every rafter triangle faces the sky, which is the way a roof points "
                    f"({skyward} of {sum(len(m['triangles']) for m in over_store)})")

            # A level bounded by a CEILING has its lid already; only a level whose ceiling is null
            # is bounded by rafters. Nothing in this house is the first case, so it is constructed.
            lidded = dict(rows)
            lidded["levels"] = dict(rows["levels"], levels=[
                dict(row, ceiling=12.0) if row["id"] == "L3" else row
                for row in layout_io.rows(rows, "levels")])
            probe_shapes, probe_cells = Shapes(), {}
            probe_stats = {"rafterMeshes": 0, "rafterArea": 0.0, "rafterAboveCeiling": 0}
            build_rafters(lidded, probe_shapes, probe_cells, probe_stats)
            require(probe_stats["rafterMeshes"] == 0,
                    f"a level that declares a roof AND a ceiling plane gets no rafters: the slab "
                    f"is its lid ({probe_stats['rafterMeshes']})")

            # The guards. The shell DRAWS a parapet and a rail here; nothing stopped you.
            railing = float(build_rules["railing"])
            guarded = {}
            for identifier, indices in references.items():
                floor = layout_io.cell_extent(house_cells[identifier],
                                              house_levels[house_cells[identifier]["level"]])[0]
                found = [house_shapes.obbs[index] for index in indices if index < offset
                         and (identifier, index) not in house["borrowed"]
                         and house_shapes.obbs[index][4] == KIND_WALL
                         and abs(house_shapes.obbs[index][0][1] - (floor + railing / 2)) < 1e-6
                         and min(house_shapes.obbs[index][1][0],
                                 house_shapes.obbs[index][1][2]) * 2 <= GUARD_THICK + 1e-9]
                if found:
                    guarded[identifier] = found
            require(sorted(guarded) == ["L0_GARAGE_LOFT", "L1_BALCONY_FRONT", "L1_BALCONY_REAR",
                                        "L2_BALCONY_JULIET"],
                    f"exactly the four places you could fall more than a metre off are guarded "
                    f"({sorted(guarded)})")
            require(len(guarded["L1_BALCONY_REAR"]) == 3
                    and len(guarded["L0_GARAGE_LOFT"]) == 4,
                    f"three open sides on the rear balcony -- the fourth is the house -- and four "
                    f"round the loft, which is nested in the garage "
                    f"({len(guarded['L1_BALCONY_REAR'])}, {len(guarded['L0_GARAGE_LOFT'])})")
            require(all(identifier not in guarded
                        for identifier in ("EXT_PORCH", "EXT_TERRACE")),
                    "and neither the porch at +0.57 nor the terrace at +0.45 is, which is §70.5's "
                    "own metre deciding it")
            balcony = house_cells["L1_BALCONY_REAR"]
            deck = layout_io.cell_extent(balcony, house_levels[balcony["level"]])[0]
            tops = {round(record[0][1] + record[1][1], 4) for record in guarded["L1_BALCONY_REAR"]}
            require(tops == {round(deck + railing, 4)},
                    f"a guard's top is §12's railing height over the deck, +{deck + railing:.2f} "
                    f"({sorted(tops)})")
            box = layout_io.cell_boxes(balcony)[0]
            require(all(box[0] - 1e-6 <= record[0][0] <= box[1] + 1e-6
                        and box[2] - 1e-6 <= record[0][2] <= box[3] + 1e-6
                        for record in guarded["L1_BALCONY_REAR"]),
                    "and it stands INSIDE the deck's edge, like the parapet the shell draws, "
                    "rather than hanging in the air outside it")

            # 7f. `HOUSE-00568`: a body standing in a hole is in BOTH rooms, so what is within
            #     reach through the hole is in both lists. Claimed on the authored house, because
            #     the fixture has one room and no doorway with anything behind it.
            house_aabbs = ([obb_aabb(o) for o in house_shapes.obbs]
                           + [mesh_aabb(m) for m in house_shapes.meshes])
            house_portals = layout_io.load_layout(authored, ["portals"])
            planes = {}
            for portal in layout_io.rows(house_portals, "portals"):
                plane = portal.get("plane") or {}
                if plane.get("axis") in ("x", "z"):
                    for side in (portal["cellA"], portal["cellB"]):
                        planes.setdefault(side, []).append(
                            (plane["axis"], float(plane["value"])))

            require(house["stats"]["openingShared"] > 0,
                    f"the house has holes with something behind them "
                    f"({house['stats']['openingShared']} shape(s) shared)")

            # The regression itself, named. `STAIR_MAIN_L0_L1`'s first run is a wedge whose west
            # face is at x = +2.40, and `P_L0_FOYER__L0_STAIR` is the cased opening at x = +2.20:
            # 0.20 m, which is less than a 0.30 m body's radius. `HOUSE-00618`'s bot walked east
            # out of the foyer and was 0.151 m inside the staircase before anything stopped it.
            first_run = [index for index in range(len(house_shapes.meshes))
                         if abs(mesh_aabb(house_shapes.meshes[index])[0] - 2.40) < 1e-6
                         and abs(mesh_aabb(house_shapes.meshes[index])[1] - 0.60) < 1e-6
                         and house_shapes.meshes[index]["kind"] == KIND_STAIR]
            require(len(first_run) == 1,
                    f"the main stair's first run is one wedge starting at x +2.40, y +0.60 "
                    f"({len(first_run)})")
            if first_run:
                run_index = offset + first_run[0]
                require(run_index in references["L0_STAIR_MAIN"] and run_index in references["L0_FOYER"],
                        "and it is in L0_FOYER's list as well as L0_STAIR_MAIN's: the opening is "
                        "0.20 m from it and a body in the opening is standing in the staircase")

            # The fridge, which is where the vertical band had to be the BODY's and not the hole's:
            # `CELL_FRIDGE_INTERIOR`'s ceiling slab starts exactly at the top of its own opening,
            # and a body standing on its +0.70 floor has 50 mm of head inside that slab.
            fridge_lid = [index for index in references["CELL_FRIDGE_INTERIOR"]
                          if index < offset and house_shapes.obbs[index][4] == KIND_CEILING]
            require(len(fridge_lid) == 1 and fridge_lid[0] in references["L0_KITCHEN"],
                    f"the kitchen carries the fridge's own ceiling slab, which is above the top "
                    f"of the fridge's opening and still inside a body standing in it "
                    f"({len(fridge_lid)})")

            # Nothing is carried further than a body can reach, and the borrowed part is what
            # sizes the grid: `L0_FOYER` borrows a wedge that runs to x +3.50 and its own bounds
            # stop at +2.60, which is the opening's plane plus `OPENING_REACH`.
            foyer = [row for row in house["cells"] if row["id"] == "L0_FOYER"][0]
            require(abs(foyer["bounds"][3] - (2.20 + OPENING_REACH)) < 1e-6,
                    f"L0_FOYER's bounds stop {OPENING_REACH:.2f} m past its own opening, not at "
                    f"the far end of the staircase it borrowed ({foyer['bounds'][3]:.3f})")
            require(foyer["nx"] * foyer["nz"] <= 30,
                    f"...so its grid is still the foyer's ({foyer['nx']} x {foyer['nz']} buckets)")

            # And every borrowed shape is INDEXED by the part of it within reach of a hole in
            # the cell that borrowed it -- not by all of it. A neighbour's floor slab spans the
            # neighbour's whole room, and a cell that indexed one would have a grid over a room
            # its body can never be in.
            far = []
            for (cell_id, index), clips in house["borrowed"].items():
                for clip in clips:
                    if not any((clip[0] >= value - OPENING_REACH - 1e-6
                                and clip[3] <= value + OPENING_REACH + 1e-6) if axis == "x" else
                               (clip[2] >= value - OPENING_REACH - 1e-6
                                and clip[5] <= value + OPENING_REACH + 1e-6)
                               for axis, value in planes.get(cell_id, [])):
                        far.append((cell_id, index, tuple(round(v, 3) for v in clip)))
            require(not far,
                    f"every borrowed shape is indexed within {OPENING_REACH:.2f} m of the hole it "
                    f"came through ({len(far)} not: {far[:2]})")
            require(all(min(clip[3] - clip[0], clip[5] - clip[2]) <= 2 * OPENING_REACH + 1e-6
                        for clips in house["borrowed"].values() for clip in clips),
                    "...and no borrowed box is wider than the reach in BOTH horizontal axes, "
                    "which is what a clip that never fired would look like")

            # 7g. `HOUSE-00774`: §49.2's exterior collision -- the height field plus OBBs for
            #     what stands on it -- and the cell boundaries that stopped being walls.
            grade_of_house = ground_storey(rows)
            rows_exterior = layout_io.load_layout(authored, ["cells"])
            rows_exterior["exterior"] = layout_io.load_file(
                authored / layout_io.FILES["exterior"][0], "exterior")
            outdoor = {row["id"] for row in house["cells"] if row["outdoors"]}
            open_cells = {cell_id for cell_id, _boxes in _exterior_cells(rows_exterior)}
            require(outdoor == open_cells | {"EXT_WORLD"},
                    f"exactly the open exterior cells carry §11.5's ground, and every room is "
                    f"without it ({sorted(outdoor.symmetric_difference(open_cells | {'EXT_WORLD'}))[:3]})")
            require("EXT_SHED" not in outdoor and "L0_HALL" not in outdoor,
                    "the shed is a BUILDING and the hall is a room: neither stands on the lawn")

            require(house["stats"]["openBoundaries"] > 50,
                    f"the boundaries between one open yard and another are grass, not wall "
                    f"({house['stats']['openBoundaries']} of them)")
            walled = []
            for row in house["cells"]:
                if row["id"] not in open_cells:
                    continue
                for index in row["shapes"]:
                    if index >= offset or house_shapes.obbs[index][4] != KIND_WALL:
                        continue
                    # Borrowed shapes are somebody else's (`HOUSE-00568`): the shed's own wall
                    # reaches into the garden and the side yard through their openings, and that is
                    # a building's wall being shared, not a boundary being walled.
                    owners = [other["id"] for other in house["cells"]
                              if index in other["shapes"]
                              and (other["id"], index) not in house["borrowed"]]
                    if len(owners) > 1 and all(one in open_cells for one in owners):
                        walled.append((row["id"], index))
            require(not walled,
                    f"and not one wall piece is shared by two of them ({walled[:3]})")

            fences = [index for index in range(len(house_shapes.obbs))
                      if house_shapes.obbs[index][4] == KIND_EXTERIOR
                      and house_shapes.surfaces[house_shapes.obbs[index][3]] == "fence"]
            require(house["stats"]["fencePieces"] == len(fences) and len(fences) > 60,
                    f"§11.2's seven runs are collision now ({len(fences)} pieces)")
            # The property is ENCLOSED: every metre of every boundary either has a fence piece on
            # it or is one of §11.2's three gate openings.
            gates = [(float(gate["opening"]["x"][0]), float(gate["opening"]["x"][1]),
                      float(gate["opening"]["z"][0]), float(gate["opening"]["z"][1]))
                     for gate in (rows_exterior.get("exterior") or {}).get("gates", [])]
            missing = []
            for row in (rows_exterior.get("exterior") or {}).get("fences", []):
                start, end = row["path"][0], row["path"][-1]
                along_x = abs(end[0] - start[0]) >= abs(end[2] - start[2])
                low = min(start[0], end[0]) if along_x else min(start[2], end[2])
                high = max(start[0], end[0]) if along_x else max(start[2], end[2])
                across = (start[2] + end[2]) / 2.0 if along_x else (start[0] + end[0]) / 2.0
                probe = low + 0.25
                while probe < high - 0.25:
                    x, z = (probe, across) if along_x else (across, probe)
                    if any(x0 - 0.3 <= x <= x1 + 0.3 and z0 - 0.3 <= z <= z1 + 0.3
                           for x0, x1, z0, z1 in gates):
                        probe += 0.5
                        continue
                    if not any(abs(house_shapes.obbs[index][0][0] - x) <=
                               house_shapes.obbs[index][1][0] + 1e-6
                               and abs(house_shapes.obbs[index][0][2] - z) <=
                               house_shapes.obbs[index][1][2] + 1e-6
                               for index in fences):
                        missing.append((row["id"], round(x, 2), round(z, 2)))
                    probe += 0.5
            require(not missing,
                    f"and every metre of every run is covered except at the three gates "
                    f"({len(missing)} gaps: {missing[:3]})")
            in_a_gate = [index for index in fences
                         if any(x0 + 1e-6 < house_shapes.obbs[index][0][0] < x1 - 1e-6
                                and z0 - 0.2 < house_shapes.obbs[index][0][2] < z1 + 0.2
                                for x0, x1, z0, z1 in gates)]
            require(not in_a_gate,
                    f"and NOTHING static stands in a gate's opening: a gate is a leaf that opens, "
                    f"so the hole is what the file carries ({in_a_gate[:3]})")

            # A gate is a HOLE in the fence, and the house cannot show it: §11.2 authors three
            # runs that stop either side of each gate, so the rule that leaves an opening out has
            # nothing to do there. Asked of a synthetic run that spans one instead -- which is the
            # layout a fence with a gate in the middle of it would be authored as.
            spanning = {
                "levels": {"levels": [{"id": "L0", "ffl": 0.0, "ceiling": 3.0}]},
                "cells": {"cells": [{"id": "EXT_YARD", "level": "L0", "kind": "exterior",
                                     "visibilityHint": "open", "yOverride": [0.0, 20.0],
                                     "boxes": [{"x": [-10.0, 10.0], "z": [-10.0, 10.0]}]}]},
                "exterior": {
                    "fences": [{"id": "FENCE_ONE", "asset": "MODEL_FENCE_BOARD_01", "height": 1.85,
                                "path": [[-8.0, 0.0, 0.0], [8.0, 0.0, 0.0]]}],
                    "gates": [{"id": "GATE_ONE", "fence": "FENCE_ONE", "kind": "hinged",
                               "opening": {"x": [-0.6, 0.6], "z": [-0.05, 0.05]}, "height": 1.85}],
                },
            }
            probe_shapes, probe_cells = Shapes(), {}
            probe_stats = {name: 0 for name in ("fencePieces", "kerbPieces", "structureObbs",
                                                "trunks", "vehicles")}
            build_exterior(spanning, probe_shapes, probe_cells, probe_stats, workspace / "nowhere")
            require(probe_stats["fencePieces"] >= 8,
                    f"a 16 m run with a gate in it becomes at least eight pieces "
                    f"({probe_stats['fencePieces']})")
            west = {round(record[0][0] + record[1][0], 4) for record in probe_shapes.obbs}
            east = {round(record[0][0] - record[1][0], 4) for record in probe_shapes.obbs}
            require(-0.6 in west and 0.6 in east,
                    f"and the run stops ON each of the gate's jambs rather than near them "
                    f"({sorted(w for w in west if w < 1)[-2:]}, {sorted(e for e in east if e > -1)[:2]})")
            require(not [record for record in probe_shapes.obbs
                         if abs(record[0][0]) < 0.6 - 1e-6 and abs(record[0][2]) < 0.05],
                    f"and none of them stands in the gate's own opening "
                    f"({[record[0] for record in probe_shapes.obbs if abs(record[0][0]) < 0.6][:2]})")
            require(any(record[0][0] < -0.6 for record in probe_shapes.obbs)
                    and any(record[0][0] > 0.6 for record in probe_shapes.obbs),
                    "...while the run either side of it is there")

            trunks = [house_shapes.obbs[index] for index in range(len(house_shapes.obbs))
                      if house_shapes.obbs[index][4] == KIND_EXTERIOR
                      and house_shapes.surfaces[house_shapes.obbs[index][3]] == "bark"]
            require(len(trunks) == house["stats"]["trunks"] and trunks,
                    f"§49.2 collides with tree TRUNKS ({len(trunks)} of them)")
            require(all(max(record[1][0], record[1][2]) * 2 <= 0.7 for record in trunks),
                    f"...trunks and not canopies: the widest is "
                    f"{max(max(record[1][0], record[1][2]) * 2 for record in trunks):.2f} m across")
            require(all(record[1][1] > 2.0 * max(record[1][0], record[1][2]) for record in trunks),
                    f"...and each is a COLUMN, taller than it is wide by more than three times: "
                    f"the stubbiest is {min(record[1][1] * 2 / max(record[1][0], record[1][2]) / 2 for record in trunks):.1f} "
                    f"times its own width, which is a trunk and not a canopy dropped on the lawn")

            cars = [house_shapes.obbs[index] for index in range(len(house_shapes.obbs))
                    if house_shapes.obbs[index][4] == KIND_EXTERIOR
                    and house_shapes.surfaces[house_shapes.obbs[index][3]] == "vehicle"]
            require(len(cars) == house["stats"]["vehicles"] and cars
                    and any(abs(record[2]) > 1e-6 for record in cars),
                    f"§11.4's parked cars are OBBs with the yaw the layout gives them "
                    f"({len(cars)} of them, yaws "
                    f"{sorted(round(math.degrees(record[2])) for record in cars)})")

            # §11.5's ground is the only floor the outdoors has (`HOUSE-00782`). A slab as well
            # would be a second answer to "how high is the ground here", and the two disagree
            # wherever the lot has dropped away from a cell's declared floor: `EXT_ORCHARD` says
            # 0.00 and the lawn under it is 0.30 m lower, which is a plinth with a step round it
            # that §43.1's 0.22 m step-up cannot climb.
            slabbed = []
            for row in house["cells"]:
                cell_row = house_cells.get(row["id"])
                if cell_row is None or not open_air(cell_row):
                    continue
                if abs(float(house_levels[cell_row["level"]].get("ffl", 0.0)) - grade_of_house) > 1e-6:
                    continue          # a balcony: its floor is a storey up and no field carries it
                for index in row["shapes"]:
                    if (index < offset and house_shapes.obbs[index][4] == KIND_FLOOR
                            and (row["id"], index) not in house["borrowed"]):
                        slabbed.append((row["id"], index))
            require(not slabbed,
                    f"no open exterior cell on the ground storey has a floor slab: the height "
                    f"field is what it stands on ({slabbed[:3]})")
            require(any(house_shapes.obbs[index][4] == KIND_FLOOR
                        for index in references["L1_BALCONY_REAR"] if index < offset),
                    "...and a BALCONY still has one, because its floor is 3.65 m over the lawn "
                    "and no height field carries that")

            # §10.4's road termination: the accessible corridor's three open sides -- x = ±35 and
            # the far side -- each have a continuous barrier across them, with no gap a 0.62 m
            # body could walk through. The near side is the property's own fence, above.
            solid = [house_shapes.obbs[index] for index in range(len(house_shapes.obbs))
                     if house_shapes.obbs[index][4] == KIND_EXTERIOR
                     and house_shapes.surfaces[house_shapes.obbs[index][3]] in
                     ("hedge", "vehicle", "structure", "bark")]

            def covered(x: float, z: float) -> bool:
                """Is there something solid within a body's radius of this point?"""
                for record in solid:
                    (cx, cy, cz), (hx, hy, hz), yaw, _surface, _kind = record
                    if cy + hy < 0.5:
                        continue          # a kerb or a bed: a body walks over it
                    cos, sin = abs(math.cos(yaw)), abs(math.sin(yaw))
                    ex, ez = hx * cos + hz * sin, hx * sin + hz * cos
                    if (abs(cx - x) <= ex + BODY_RADIUS and abs(cz - z) <= ez + BODY_RADIUS):
                        return True
                return False

            gaps = []
            for side, fixed in (("x", -35.0), ("x", 35.0), ("z", 11.75)):
                span = (0.5, 11.5) if side == "x" else (-34.5, 34.5)
                probe = span[0]
                while probe <= span[1]:
                    x, z = (fixed, probe) if side == "x" else (probe, fixed)
                    if not covered(x, z):
                        gaps.append((side, round(fixed, 2), round(probe, 2)))
                    probe += 0.5
            require(not gaps,
                    f"§10.4's road termination is continuous on all three open sides of the "
                    f"corridor ({len(gaps)} gap(s): {gaps[:3]})")
            require(house["stats"]["hedges"] > 90,
                    f"and most of it is §10.4's hedge ({house['stats']['hedges']} sections)")

            kerbs = [house_shapes.obbs[index] for index in range(len(house_shapes.obbs))
                     if house_shapes.obbs[index][4] == KIND_EXTERIOR
                     and house_shapes.surfaces[house_shapes.obbs[index][3]] == "kerb"]
            require(kerbs and all(record[1][1] * 2 <= 0.22 + 1e-6 for record in kerbs),
                    f"and §11.4's kerb is a kerb: {max(record[1][1] * 2 for record in kerbs):.2f} m "
                    f"high, which §43.1's 0.22 m step-up walks over rather than round")

            # A window is not a way through, and what is behind one is not carried: the sunroom's
            # floor slab reaches to `P_L0_KITCHEN__W2`'s plane, and the kitchen does not have it.
            sunroom_floor = [index for index in references["L0_SUNROOM"]
                             if index < offset and house_shapes.obbs[index][4] == KIND_FLOOR]
            require(sunroom_floor
                    and all(index not in references["L0_KITCHEN"] for index in sunroom_floor),
                    "the kitchen does not carry the sunroom's floor: the only hole between them "
                    "is a window with a 0.95 m sill, and a body cannot stand in one")

        # 8. Proxies: a box becomes an OBB, five boxes become five OBBs, and only what is not a
        #    box becomes a mesh.
        assets_dir = workspace / "assets"
        assets_dir.mkdir()
        _fixture_proxy(assets_dir / "one_box.glb", [((0, 0.5, 0), (0.3, 0.5, 0.2))])
        _fixture_proxy(assets_dir / "five_boxes.glb",
                       [((x, 0.5, 0), (0.2, 0.5, 0.2)) for x in (-2, -1, 0, 1, 2)])
        # Deliberately OFF the asset origin in x, so that a yaw or a scale that is dropped moves
        # the box somewhere the assertions below can see. A proxy centred on its own origin is
        # invariant under both and would prove nothing.
        _fixture_proxy(assets_dir / "offset_box.glb", [((1.0, 0.5, 0.0), (0.1, 0.5, 0.1))])
        components = split_components(*read_col_meshes(assets_dir / "five_boxes.glb")[0])
        require(len(components) == 5,
                f"five disjoint boxes joined into one mesh are found as five components "
                f"(got {len(components)})")
        require(all(as_box(v, t) is not None for v, t in components),
                "...and each one is recognised as a box")

        vertices, triangles = read_col_meshes(assets_dir / "one_box.glb")[0]
        moved = list(vertices)
        moved[0] = (moved[0][0], moved[0][1], moved[0][2] + 0.15)
        require(as_box(moved, triangles) is None,
                "a shape with one corner pushed in is NOT a box, even with 8 vertices and 12 "
                "triangles -- the dent is where the player would walk in")

        # 9. Props end up in the file, yawed, with the box path taken.
        (world_dir / "layout.props.json").write_text(json.dumps({
            "schema": "cna-house/props/1",
            "props": [{"id": "PROP_A", "asset": "MODEL_A", "cell": "L0_LOUNGE",
                       "position": [1.0, 0.0, 1.0], "yawDeg": 0.0, "scale": 1.0,
                       "static": True, "collision": "proxy"},
                      {"id": "PROP_B", "asset": "MODEL_B", "cell": "L0_LOUNGE",
                       "position": [3.0, 0.0, 1.0], "yawDeg": 0.0, "scale": 2.0,
                       "static": True, "collision": "proxy"},
                      {"id": "PROP_C", "asset": "MODEL_A", "cell": "L0_LOUNGE",
                       "position": [3.0, 0.0, 2.0], "yawDeg": 0.0,
                       "static": False, "collision": "proxy"},
                      {"id": "PROP_D", "asset": "MODEL_D", "cell": "L0_LOUNGE",
                       "position": [1.0, 0.0, 4.0], "yawDeg": 90.0, "scale": 1.0,
                       "static": True, "collision": "proxy"},
                      {"id": "PROP_E", "asset": "MODEL_D", "cell": "L0_LOUNGE",
                       "position": [1.5, 0.0, 4.5], "yawDeg": 0.0, "scale": 3.0,
                       "static": True, "collision": "proxy"}]},
            indent=2) + "\n", encoding="utf-8")
        manifest = workspace / "assets.manifest.json"
        manifest.write_text(json.dumps({
            "schema": "cna-house/assets/1",
            "assets": [
                {"id": "MODEL_A", "sourceFile": str(assets_dir / "one_box.glb")},
                {"id": "MODEL_B", "sourceFile": str(assets_dir / "five_boxes.glb")},
                {"id": "MODEL_D", "sourceFile": str(assets_dir / "offset_box.glb")},
            ]}, indent=2) + "\n", encoding="utf-8")
        world = build(world_dir, manifest)
        require(world["stats"]["propObbs"] == 8 and world["stats"]["propMeshes"] == 0,
                f"eight prop OBBs (one box + five boxes + two offset), no triangle meshes "
                f"(got {world['stats']['propObbs']} and {world['stats']['propMeshes']})")
        require(world["stats"]["propsSkipped"] == 1,
                "a non-static prop is excluded -- it becomes a DynamicInstance, not static "
                "collision")
        yawed = [o for o in world["shapes"].obbs
                 if o[4] == KIND_PROP and abs(o[2] - math.pi / 2) < 1e-6]
        require(len(yawed) == 1, "the 90-degree prop carries its yaw as radians in the record")
        # PROP_D sits at (1, 0, 4) and its proxy is 1 m along the asset's +x. Turned 90 degrees
        # about Y that 1 m must come out along -z, at (1, 0.5, 3). A dropped yaw leaves it at
        # (2, 0.5, 4) -- still a plausible-looking box, one metre from where the author put it.
        require(all(abs(a - b) < 1e-5 for a, b in zip(yawed[0][0], (1.0, 0.5, 3.0))),
                f"...and the yaw MOVES it: the proxy's 1 m offset comes out along -z at "
                f"(1, 0.5, 3), not (2, 0.5, 4) (got {tuple(round(c, 4) for c in yawed[0][0])})")
        scaled = [o for o in world["shapes"].obbs
                  if o[4] == KIND_PROP and abs(o[1][1] - 1.5) < 1e-6]
        require(len(scaled) == 1 and abs(scaled[0][1][0] - 0.3) < 1e-6,
                "a scale of 3.0 triples the proxy's half-extents")
        require(scaled and all(abs(a - b) < 1e-5 for a, b in zip(scaled[0][0], (4.5, 1.5, 4.5))),
                f"...and scales the offset too: 1 m out becomes 3 m out, at (4.5, 1.5, 4.5) "
                f"(got {tuple(round(c, 4) for c in scaled[0][0]) if scaled else None})")

        # 10. The grid. §49.2's claim is "about six shapes, not nine hundred".
        require(world["stats"]["maxBucketOccupancy"] < 20,
                f"the worst 1 m bucket holds {world['stats']['maxBucketOccupancy']} shapes, "
                f"not the whole cell")
        cell = next(c for c in world["cells"] if c["id"] == "L0_LOUNGE")
        covered = {i for bucket in cell["buckets"] for i in bucket}
        require(len(covered) == len(cell["shapes"]),
                f"every one of the cell's {len(cell['shapes'])} shapes appears in at least one "
                f"bucket ({len(covered)} did) -- a shape the grid forgot is a shape the sweep "
                f"never tests")
        # A cell's bounds are the union of its own shapes, so one escaped shape sizes the grid.
        # `HOUSE-00472` watched this allocate until the process was killed.
        try:
            build_grid((0.0, 0.0, 0.0, 4_000.0, 3.0, 4_000.0), [])
            refused = ""
        except LayoutError as error:
            refused = str(error)
        require("escaped its cell" in refused,
                f"a grid of 16 million buckets is refused rather than allocated ({refused[:60]})")
        require(build_grid((0.0, 0.0, 0.0, 400.0, 3.0, 400.0), [])[0] == 400,
                "while `EXT_WORLD`'s own 400 x 400 is built without complaint")

        # `HOUSE-00472`: a mesh's shape index is `len(obbs) + mesh`, and it is only right once no
        # more OBBs will be made. The fixture builds prop OBBs AFTER the stair wedge, so a cell
        # that referenced its stair by the count at the time points at a prop instead. Nothing
        # said so: the reference was in range, the round trip agreed with itself, and the grid
        # was consistently wrong.
        obb_total = len(world["shapes"].obbs)
        stair_refs = {}
        for check in world["cells"]:
            for index in check["shapes"]:
                if index >= obb_total:
                    kind = KIND_NAMES[world["shapes"].meshes[index - obb_total]["kind"]]
                    stair_refs.setdefault(kind, set()).add(check["id"])
        require(world["stats"]["propObbs"] > 0 and sorted(stair_refs) == ["stair"],
                f"the only mesh any cell references is its stair, even though "
                f"{world['stats']['propObbs']} prop OBBs were numbered after it "
                f"({sorted(stair_refs)})")
        require(all(index < obb_total + len(world["shapes"].meshes)
                    for check in world["cells"] for index in check["shapes"]),
                "and no cell references a shape that does not exist")

        # A shape must be in every bucket it OVERLAPS, not just the one its minimum corner falls
        # in: a floor slab indexed by its corner leaves the player standing on nothing everywhere
        # but one square metre. Checked against an independent formulation -- interval overlap per
        # bucket rather than index arithmetic over the AABB -- so the two cannot be wrong together.
        all_aabbs = ([obb_aabb(o) for o in world["shapes"].obbs]
                     + [mesh_aabb(m) for m in world["shapes"].meshes])
        mismatches = 0
        entries = 0
        for check in world["cells"]:
            ox, oz = check["origin"]
            for j in range(check["nz"]):
                for i in range(check["nx"]):
                    bx0, bx1 = ox + i * GRID_CELL, ox + (i + 1) * GRID_CELL
                    bz0, bz1 = oz + j * GRID_CELL, oz + (j + 1) * GRID_CELL
                    expected = set()
                    for n, index in enumerate(check["shapes"]):
                        borrowed_clips = world["borrowed"].get((check["id"], index))
                        ax0, _, az0, ax1, _, az1 = (union_aabb(borrowed_clips) if borrowed_clips
                                                    else all_aabbs[index])
                        if (min(ax1, bx1) - max(ax0, bx0) > EPS
                                and min(az1, bz1) - max(az0, bz0) > EPS):
                            expected.add(n)
                    actual = set(check["buckets"][j * check["nx"] + i])
                    entries += len(actual)
                    if actual != expected:
                        mismatches += 1
        require(mismatches == 0,
                f"every bucket holds exactly the shapes whose AABB overlaps it "
                f"({mismatches} buckets disagreed over {entries} entries)")
        require(entries > 3 * sum(len(c["shapes"]) for c in world["cells"]),
                f"...and shapes really do span several buckets each ({entries} entries for "
                f"{sum(len(c['shapes']) for c in world['cells'])} references)")

        # 11. Round trip. The reader is in this file so that the writer cannot drift from a
        #     reader that only exists in C++ and only runs nightly.
        data = serialise(world)
        back = read_back(data)
        require(len(back["obbs"]) == len(world["shapes"].obbs)
                and len(back["meshes"]) == len(world["shapes"].meshes)
                and len(back["cells"]) == len(world["cells"]),
                "the file reads back with the same shape and cell counts")
        require(back["gridCell"] == GRID_CELL and back["worldHash"] == "sha256:fixture",
                "the grid size and the world hash survive the round trip")
        first = world["shapes"].obbs[0]
        require(all(abs(a - b) < 1e-5 for a, b in zip(back["obbs"][0]["centre"], first[0])),
                "an OBB's centre survives as float32")
        # Every OBB's surface, not just the first one's -- the first is index 0, and a writer that
        # emitted a constant 0 would satisfy a check that only ever looked at it.
        require(all(record["surface"] == world["shapes"].surfaces[built[3]]
                    for record, built in zip(back["obbs"], world["shapes"].obbs)),
                "every OBB's surface round trips through the string table")
        require(len({record["surface"] for record in back["obbs"]}) >= 3,
                f"...and the fixture exercises more than one entry of that table "
                f"({len({r['surface'] for r in back['obbs']})} distinct surfaces read back)")

        # 11b. §11.5's ground (`HOUSE-00553`). It is 26 KB of the file and the only part of it a
        #      reader could plausibly skip, so the fixture carries a 3 x 2 field whose every
        #      number is stated in `fixture_world` and asserted in C++ as well as here.
        fixture = fixture_world()
        pair = read_back(serialise(fixture))["terrain"]
        want = fixture["terrain"]
        require(pair is not None and (pair["samplesX"], pair["samplesZ"]) == (3, 2),
                f"the height field round trips 3 x 2 and not 2 x 3 "
                f"({pair and (pair['samplesX'], pair['samplesZ'])})")
        require(pair["heights"] == want["heights"],
                f"every sample survives as float32 ({pair['heights']})")
        require((pair["originX"], pair["originZ"], pair["step"]) == (-1.0, -2.0, 2.0),
                f"so do the origin and the step, which is NOT the world grid's 1.0 "
                f"({pair['originX']}, {pair['originZ']}, {pair['step']})")
        require(pair["materials"] == ["grass", "gravel"] and pair["materialIndex"] == want["materialIndex"],
                f"and the per-sample material resolves through the shared surface table "
                f"({pair['materials']}, {pair['materialIndex']})")
        require(len(set(pair["materialIndex"])) > 1,
                "...over more than one material, so a reader that returned a constant fails")
        world_no_terrain = dict(fixture)
        world_no_terrain["terrain"] = None
        require(read_back(serialise(world_no_terrain))["terrain"] is None,
                "a world with no ground writes the flag and stops, and reads back as none")

        # 12. Truncation and a wrong version are refused, not misread.
        for cut in (3, 12, len(data) - 1):
            try:
                read_back(data[:cut])
                caught = False
            except LayoutError:
                caught = True
            require(caught, f"a file truncated to {cut} bytes is refused")
        try:
            read_back(data + b"\0\0\0\0")
            caught = False
        except LayoutError as exc:
            caught = "left over" in str(exc)
        require(caught,
                "trailing bytes after the last cell are refused -- a file the reader only half "
                "consumed is a writer and a reader that disagree, and it must not pass silently")
        bumped = bytearray(data)
        bumped[4:8] = struct.pack("<I", 99)
        try:
            read_back(bytes(bumped))
            caught = False
        except LayoutError as exc:
            caught = "99" in str(exc)
        require(caught, "a version this reader does not know is refused, naming the number found")
        flagged = bytearray(data)
        flagged[8:12] = struct.pack("<I", 1)
        try:
            read_back(bytes(flagged))
            caught = False
        except LayoutError:
            caught = True
        require(caught, "an unknown flag bit is refused rather than ignored")

        # 13. Determinism -- the same layout twice is the same bytes.
        require(serialise(build(world_dir, manifest)) == data,
                "two builds of one layout produce byte-identical output")

        # 14. A prop whose asset has no `_COL` mesh is an error, not a silently uncollidable prop.
        _fixture_proxy(assets_dir / "bare.glb", [((0, 0, 0), (1, 1, 1))])
        document, blob = gltf_io.read_model(assets_dir / "bare.glb")
        document["nodes"][0]["name"] = "bare"
        gltf_io.write_glb(assets_dir / "bare.glb", document, blob)
        manifest.write_text(json.dumps({
            "schema": "cna-house/assets/1",
            "assets": [{"id": "MODEL_A", "sourceFile": str(assets_dir / "bare.glb")},
                       {"id": "MODEL_B", "sourceFile": str(assets_dir / "five_boxes.glb")}]},
            indent=2) + "\n", encoding="utf-8")
        try:
            build(world_dir, manifest)
            raised = ""
        except LayoutError as exc:
            raised = str(exc)
        require("_COL" in raised and "PROP_A" in raised,
                "a prop asking for proxy collision whose asset has no _COL mesh is refused, "
                "naming the prop")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("build_collision: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--world", type=Path, default=REPO / "assets-src" / "world")
    parser.add_argument("--manifest", type=Path,
                        default=REPO / "assets-src" / "assets.manifest.json")
    parser.add_argument("--out", type=Path, default=REPO / "content" / "world" / "collision.bin")
    parser.add_argument("--fixture", type=Path, default=None,
                        help="write the C++ round-trip fixture (HOUSE-00541) and exit")
    parser.add_argument("--report", action="store_true", help="print the shape census and stop")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    if args.fixture is not None:
        args.fixture.parent.mkdir(parents=True, exist_ok=True)
        data = serialise(fixture_world())
        args.fixture.write_bytes(data)
        print(f"build_collision: wrote the round-trip fixture {args.fixture} ({len(data)} bytes)")
        return 0

    try:
        world = build(args.world, args.manifest)
    except LayoutError as exc:
        print(f"build_collision: {exc}", file=sys.stderr)
        return 1

    print(report(world))
    if args.report:
        return 0

    data = serialise(world)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(data)
    print(f"build_collision: wrote {args.out} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
