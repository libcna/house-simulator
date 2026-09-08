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
import layout_io  # noqa: E402
import roof_geometry  # noqa: E402
import stair_geometry  # noqa: E402
from layout_io import LayoutError  # noqa: E402

MAGIC = b"CCOL"
VERSION = 1

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
    portals_by_plane: dict[tuple[str, float], list[dict]] = {}
    for portal in portals:
        plane = portal.get("plane") or {}
        key = (plane.get("axis"), snap(float(plane.get("value", 0.0))))
        portals_by_plane.setdefault(key, []).append(portal)

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
        for box in boxes_by_cell[cell["id"]]:
            x0, x1, z0, z1 = box
            indices.append(shapes.obb(
                ((x0 + x1) / 2, y0 - depth / 2, (z0 + z1) / 2),
                ((x1 - x0) / 2, depth / 2, (z1 - z0) / 2),
                0.0, floor_surface, KIND_FLOOR))
            if not is_open:
                indices.append(shapes.obb(
                    ((x0 + x1) / 2, y1 + depth / 2, (z0 + z1) / 2),
                    ((x1 - x0) / 2, depth / 2, (z1 - z0) / 2),
                    0.0, cell.get("ceilingMaterial"), KIND_CEILING))

            for side in _side_planes(box):
                axis, value, su0, su1, outward = side
                openings = _same_cell_openings(cell["id"], box, side, boxes_by_cell, (y0, y1))
                on_plane = portals_by_plane.get((axis, snap(value)), [])
                for u0, u1, neighbour in _neighbour_segments(
                        cell, box, side, cells_by_level.get(cell["level"], []), boxes_by_cell):
                    if is_open and neighbour is None:
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
                        indices.append(shapes.obb(
                            centre, half, 0.0, cell.get("wallMaterial"), KIND_WALL))
                        stats["wallPieces"] += 1
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


def _guard_obb(shapes: Shapes, axis: str, value: float, u0: float, u1: float, outward: int,
               floor: float, height: float, surface) -> int:
    """One guard box along an open edge, standing INSIDE it the way the drawn parapet does."""
    inward = -outward * GUARD_THICK / 2.0
    centre_u, half_u = (u0 + u1) / 2, (u1 - u0) / 2
    if axis == "x":
        centre = (value + inward, floor + height / 2, centre_u)
        half = (GUARD_THICK / 2, height / 2, half_u)
    else:
        centre = (centre_u, floor + height / 2, value + inward)
        half = (half_u, height / 2, GUARD_THICK / 2)
    return shapes.obb(centre, half, 0.0, surface, KIND_WALL)


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
    for optional in ("stairs", "props", "materials"):
        name, _ = layout_io.FILES[optional]
        if (world_dir / name).is_file():
            layout[optional] = layout_io.load_file(world_dir / name, optional)

    asset_paths: dict[str, Path] = {}
    if manifest_path and manifest_path.is_file():
        for row in layout_io.load_file(manifest_path, "assets").get("assets", []):
            source = row.get("sourceFile")
            if source:
                asset_paths[row["id"]] = (REPO / source)

    stats = {"wallPieces": 0, "stairMeshes": 0, "stairSteps": 0, "stairLandings": 0,
             "stairsGuessed": 0, "rafterMeshes": 0, "rafterArea": 0.0, "rafterAboveCeiling": 0, "guards": 0,
             "propObbs": 0, "propMeshes": 0, "propsSkipped": 0}
    shapes = Shapes()
    per_cell = build_shell(layout, shapes, stats)
    build_stairs(layout, shapes, per_cell, stats)
    build_rafters(layout, shapes, per_cell, stats)
    build_mezzanine_guards(layout, shapes, per_cell, stats)
    build_props(layout, shapes, per_cell, asset_paths, stats)

    obb_count = len(shapes.obbs)
    _resolve_meshes(per_cell, obb_count)
    aabbs = [obb_aabb(o) for o in shapes.obbs] + [mesh_aabb(m) for m in shapes.meshes]

    cells = []
    for cell_id in sorted(per_cell):
        indices = per_cell[cell_id]
        if len(indices) > MAX_PER_CELL:
            raise LayoutError(
                f"cell {cell_id!r} has {len(indices)} collision shapes; the format's u16 bucket "
                f"indices address {MAX_PER_CELL}")
        local = [aabbs[i] for i in indices]
        bounds = (min(a[0] for a in local), min(a[1] for a in local), min(a[2] for a in local),
                  max(a[3] for a in local), max(a[4] for a in local), max(a[5] for a in local))
        nx, nz, origin, buckets = build_grid(bounds, local)
        cells.append({"id": cell_id, "shapes": indices, "bounds": bounds,
                      "nx": nx, "nz": nz, "origin": origin, "buckets": buckets})

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
    return {"shapes": shapes, "cells": cells, "stats": stats,
            "worldHash": _world_hash(world_dir)}


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
        bounds = unpack("<6f")
        (shape_count,) = unpack("<I")
        indices = [unpack("<I")[0] for _ in range(shape_count)]
        nx, nz = unpack("<II")
        origin = unpack("<2f")
        buckets = []
        for _ in range(nx * nz):
            (count,) = unpack("<H")
            buckets.append([unpack("<H")[0] for _ in range(count)])
        cells.append({"id": cell_id, "bounds": bounds, "shapes": indices,
                      "nx": nx, "nz": nz, "origin": origin, "buckets": buckets})

    if at != len(data):
        raise LayoutError(f"{len(data) - at} bytes left over after the last cell")
    return {"worldHash": world_hash, "gridCell": grid_cell, "surfaces": surfaces,
            "obbs": obbs, "meshes": meshes, "cells": cells}


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
        f"  rafters: {stats['rafterMeshes']} clipped roof planes over "
        f"{stats['rafterArea']:.1f} m² of plan, {stats['rafterAboveCeiling']} dropped as "
        f"unreachable above a flat ceiling",
        f"  stairs: {stats['stairMeshes']} ramp wedges, {stats['stairLandings']} landings, "
        f"{stats['stairSteps']} stepped OBBs; {stats['stairsGuessed']} flight(s) placed by guess "
        f"for want of an authored footprint",
        f"  props: {stats['propObbs']} OBBs, {stats['propMeshes']} meshes, "
        f"{stats['propsSkipped']} without collision",
    ]
    return "\n".join(lines)


# ======================================================================================= selftest


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
        divider = [
            i for i in lounge["shapes"]
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
                    for i in lounge["shapes"]),
                "...but the part of that same edge facing outdoors IS a wall")

        # 3. The wall carrying a door and a window is punched by both.
        shared = [i for i in lounge["shapes"]
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
                          if index >= offset and house_cells[identifier].get("level") != "L3"}
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
                        ax0, _, az0, ax1, _, az1 = all_aabbs[index]
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
    parser.add_argument("--report", action="store_true", help="print the shape census and stop")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

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
