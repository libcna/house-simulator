#!/usr/bin/env python3
"""fence_gen.py -- §11.2's fences and gates and §11.1's shed, from the exterior layout.

`HOUSE-00766`, `HOUSE-00767`, `HOUSE-00768`. The boundary a lot has and the things standing on it:
the 1.85 m board fence on the west, north and east boundaries and the 1.35 m ornamental fence along
the road frontage, as posts with caps, rails and the surface between them; the three gates cut into
them, each with the ironmongery its kind needs; and the garden shed, which is an enterable cell.

    tools/world/fence_gen.py                       # -> build/fence/<ID>.glb
    tools/world/fence_gen.py --selftest

§17's generator table gives this tool "fences, gates, trellis, the shed", and they are one tool
because they are one question: what the exterior layout says stands on the boundary.

## Why it is generated and not modelled

`layout.exterior.json` already says where every run starts and ends, how tall it is, and where the
gates cut it: `EXT_FENCE_FRONT_W` stops at x = -0.6 and `EXT_FENCE_FRONT_C` starts at +0.6 because
the pedestrian gate is between them. A modelled fence would be a second statement of the same
numbers, and the first time a boundary moved the fence would not.

## The ground it stands on

A fence follows the LOT, and §10.2's lot slopes: +0.15 at the front property line falling to -0.35
at the rear. Each post is set on the height field at its own position and each bay's rails and
boarding rake between its two posts, which is how a real board fence is built on a slope -- the
alternative, stepping each bay level, needs a plinth under the low end and §11.2 does not have one.

The height field is read through `terrain_gen.decode`, so the fence stands on the ground
`build_collision.py` collides with rather than on a second idea of where the ground is.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "assets"))
import gltf_io  # noqa: E402
import layout_io  # noqa: E402
import terrain_gen  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "assets-src" / "world"
#: A build product like the shell and the ground, and committed for the same reason: none.
OUTPUT = REPO / "build" / "fence"

#: §11.2 gives heights and positions and no carpentry, so the sections below are ordinary fence
#: carpentry in metres, stated here rather than spread through the code.
POST_SPACING = 2.4
POST_SECTION = 0.10
POST_CAP = (0.14, 0.04)
RAIL_SECTION = (0.09, 0.04)
#: A board fence's boarding, and the gap under it that keeps the timber out of the wet.
BOARD_THICKNESS = 0.02
GROUND_GAP = 0.05
#: An ornamental fence is pickets: §11.2 calls it "ornamental, painted white", and pickets with
#: daylight between them are what makes it that rather than a low board fence.
PICKET_WIDTH = 0.07
PICKET_PITCH = 0.14
PICKET_THICKNESS = 0.02

#: The two styles the layout's `asset` names, and what each is made of.
BOARD_ASSET = "MODEL_FENCE_BOARD_01"
ORNAMENTAL_ASSET = "MODEL_FENCE_ORNAMENTAL_01"


def _box(low: tuple[float, float, float], high: tuple[float, float, float]) -> list[dict]:
    """An axis-aligned box as six outward-facing quads, each `(corners, normal)`."""
    x0, y0, z0 = low
    x1, y1, z1 = high
    return [
        ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0.0, 0.0, 1.0)),
        ([(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], (0.0, 0.0, -1.0)),
        ([(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], (1.0, 0.0, 0.0)),
        ([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], (-1.0, 0.0, 0.0)),
        ([(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)], (0.0, 1.0, 0.0)),
        ([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0.0, -1.0, 0.0)),
    ]


def _raked(low: tuple[float, float, float], high: tuple[float, float, float],
           rise: tuple[float, float], along_x: bool) -> list[dict]:
    """A box whose top and bottom RAKE: @p rise is the y offset at each end along the run.

    A rail on a slope is not a horizontal box with a step at each post; it follows the ground, and
    so does the boarding over it. Everything else about the shape is a box.
    """
    faces = []
    for corners, normal in _box(low, high):
        moved = []
        for x, y, z in corners:
            span = (high[0] - low[0]) if along_x else (high[2] - low[2])
            unit = 0.0 if abs(span) < 1e-9 else (((x - low[0]) if along_x else (z - low[2])) / span)
            moved.append((x, y + rise[0] + (rise[1] - rise[0]) * unit, z))
        faces.append((moved, normal))
    return faces


def fences(directory: Path) -> list[dict]:
    """§11.2's runs as `{id, style, faces, bounds}`, ready to write."""
    layout = layout_io.load_layout(directory)
    exterior = layout.get("exterior") or {}
    _w, _h, heights, _materials = terrain_gen.decode(directory)

    def ground(x: float, z: float) -> float:
        ix = min(max(int(round((x - terrain_gen.ORIGIN_X) / terrain_gen.STEP)), 0),
                 terrain_gen.WIDTH - 1)
        iz = min(max(int(round((z - terrain_gen.ORIGIN_Z) / terrain_gen.STEP)), 0),
                 terrain_gen.HEIGHT - 1)
        return heights[iz * terrain_gen.WIDTH + ix]

    out = []
    for row in exterior.get("fences", []):
        start, end = row["path"][0], row["path"][-1]
        along_x = abs(end[0] - start[0]) >= abs(end[2] - start[2])
        length = abs(end[0] - start[0]) if along_x else abs(end[2] - start[2])
        if length <= 1e-6:
            continue
        low_x, high_x = min(start[0], end[0]), max(start[0], end[0])
        low_z, high_z = min(start[2], end[2]), max(start[2], end[2])
        height = float(row.get("height") or 1.85)
        board = row.get("asset") == BOARD_ASSET
        style = "board" if board else "ornamental"
        faces: list[dict] = []

        def at(distance: float) -> tuple[float, float]:
            """`(x, z)` @p distance along the run from its low end."""
            return ((low_x + distance, low_z) if along_x else (low_x, low_z + distance))

        # Posts, evenly spread and always one at each end: a run that ended between posts would
        # leave the last bay's boarding hanging on nothing.
        # CEIL, not round: `POST_SPACING` is the most a fence may span between posts, and rounding
        # a 3.05 m run to one bay puts them 3.05 m apart. The bays are then even and never wider
        # than the spacing, which is how a fence is actually set out.
        bays = max(1, math.ceil(length / POST_SPACING - 1e-9))
        spacing = length / bays
        half = POST_SECTION / 2.0
        posts = []
        pickets: list[tuple[float, float, float]] = []
        rakes: list[tuple[float, float]] = []
        rails: list[tuple[int, float]] = []
        for index in range(bays + 1):
            x, z = at(index * spacing)
            base = ground(x, z)
            faces += _box((x - half, base - 0.30, z - half), (x + half, base + height, z + half))
            cap = POST_CAP[0] / 2.0
            faces += _box((x - cap, base + height, z - cap),
                          (x + cap, base + height + POST_CAP[1], z + cap))
            posts.append((x, z, base, base + height + POST_CAP[1]))

        # The bays between them: two rails, and either boarding or pickets.
        thickness = BOARD_THICKNESS if board else PICKET_THICKNESS
        for bay in range(bays):
            x0, z0 = at(bay * spacing + half)
            x1, z1 = at((bay + 1) * spacing - half)
            rise = (ground(*at(bay * spacing)), ground(*at((bay + 1) * spacing)))
            rakes.append(rise)
            for level in (0.35, height - 0.25):
                rails.append((bay, level))
                lo = (min(x0, x1), level - RAIL_SECTION[1] / 2.0, min(z0, z1) - RAIL_SECTION[0] / 2.0) \
                    if along_x else \
                    (min(x0, x1) - RAIL_SECTION[0] / 2.0, level - RAIL_SECTION[1] / 2.0, min(z0, z1))
                hi = (max(x0, x1), level + RAIL_SECTION[1] / 2.0, max(z0, z1) + RAIL_SECTION[0] / 2.0) \
                    if along_x else \
                    (max(x0, x1) + RAIL_SECTION[0] / 2.0, level + RAIL_SECTION[1] / 2.0, max(z0, z1))
                faces += _raked(lo, hi, rise, along_x)
            if board:
                lo = (min(x0, x1), GROUND_GAP, min(z0, z1) - thickness / 2.0) if along_x else \
                    (min(x0, x1) - thickness / 2.0, GROUND_GAP, min(z0, z1))
                hi = (max(x0, x1), height - 0.05, max(z0, z1) + thickness / 2.0) if along_x else \
                    (max(x0, x1) + thickness / 2.0, height - 0.05, max(z0, z1))
                faces += _raked(lo, hi, rise, along_x)
            else:
                span = (max(x0, x1) - min(x0, x1)) if along_x else (max(z0, z1) - min(z0, z1))
                count = max(1, int(span / PICKET_PITCH))
                pitch = span / count
                for picket in range(count):
                    offset = picket * pitch + (pitch - PICKET_WIDTH) / 2.0
                    px0 = min(x0, x1) + (offset if along_x else 0.0)
                    pz0 = min(z0, z1) + (0.0 if along_x else offset)
                    px1 = px0 + (PICKET_WIDTH if along_x else thickness)
                    pz1 = pz0 + (thickness if along_x else PICKET_WIDTH)
                    if along_x:
                        pz0 -= thickness / 2.0
                        pz1 -= thickness / 2.0
                    else:
                        px0 -= thickness / 2.0
                        px1 -= thickness / 2.0
                    unit = (offset + PICKET_WIDTH / 2.0) / max(span, 1e-9)
                    lift = rise[0] + (rise[1] - rise[0]) * unit
                    faces += _box((px0, lift + GROUND_GAP, pz0), (px1, lift + height - 0.05, pz1))
                    pickets.append(((px0 + px1) / 2.0, (pz0 + pz1) / 2.0, lift))

        points = [point for corners, _normal in faces for point in corners]
        out.append({
            "id": row["id"],
            "style": style,
            "faces": faces,
            "bays": bays,
            "posts": bays + 1,
            "postTops": posts,
            "pickets": pickets,
            "rails": rails,
            "rakes": rakes,
            "spacing": spacing,
            "bounds": (min(p[0] for p in points), min(p[1] for p in points),
                       min(p[2] for p in points), max(p[0] for p in points),
                       max(p[1] for p in points), max(p[2] for p in points)),
        })
    return out


#: A gate's own carpentry, in metres (`HOUSE-00767`). §11.2 gives the leaf widths and the heights
#: and no ironmongery, so the sections are stated here rather than spread through the code.
STILE_SECTION = (0.07, 0.045)
GATE_GAP = 0.04
HINGE = (0.22, 0.05, 0.012)
LATCH = (0.14, 0.05, 0.012)
BOLT = (0.30, 0.04, 0.012)
#: The sliding gate runs on a track at the ground and two rollers under its leaf.
TRACK_SECTION = (0.06, 0.04)
ROLLER = (0.09, 0.09)


def gates(directory: Path) -> list[dict]:
    """§11.2's three gates as `{id, kind, leaf, fixed, pivot, travel, bounds}`.

    **The leaf and the fixed ironmongery are separate**, because one of them moves. §65 makes all
    three interactable -- the pedestrian gate swings, the vehicle gate slides six metres, the rear
    one is bolted -- and a leaf welded to its own hinge straps is a leaf that cannot open. The
    pivot and the travel travel WITH the geometry, in the node's `extras`, so the behaviour task
    reads them off the asset rather than re-deriving them from the layout.
    """
    layout = layout_io.load_layout(directory)
    exterior = layout.get("exterior") or {}
    _w, _h, heights, _materials = terrain_gen.decode(directory)

    def ground(x: float, z: float) -> float:
        ix = min(max(int(round((x - terrain_gen.ORIGIN_X) / terrain_gen.STEP)), 0),
                 terrain_gen.WIDTH - 1)
        iz = min(max(int(round((z - terrain_gen.ORIGIN_Z) / terrain_gen.STEP)), 0),
                 terrain_gen.HEIGHT - 1)
        return heights[iz * terrain_gen.WIDTH + ix]

    styles = {row["id"]: row.get("asset") for row in exterior.get("fences", [])}
    out = []
    for row in exterior.get("gates", []):
        opening = row["opening"]
        x0, x1 = float(opening["x"][0]), float(opening["x"][1])
        z0, z1 = float(opening["z"][0]), float(opening["z"][1])
        width = x1 - x0
        height = float(row.get("height") or 1.35)
        board = styles.get(row.get("fence")) == BOARD_ASSET
        style = "board" if board else "ornamental"
        base = ground((x0 + x1) / 2.0, (z0 + z1) / 2.0)
        plane = (z0 + z1) / 2.0
        thickness = STILE_SECTION[1]
        leaf_x0, leaf_x1 = x0 + GATE_GAP, x1 - GATE_GAP
        low = base + GROUND_GAP
        high = base + height

        leaf: list[dict] = []
        # Two stiles and two rails: a gate is a frame first, whatever is nailed to it.
        for stile in (leaf_x0, leaf_x1 - STILE_SECTION[0]):
            leaf += _box((stile, low, plane - thickness / 2.0),
                         (stile + STILE_SECTION[0], high, plane + thickness / 2.0))
        for level in (low, high - STILE_SECTION[0]):
            leaf += _box((leaf_x0, level, plane - thickness / 2.0),
                         (leaf_x1, level + STILE_SECTION[0], plane + thickness / 2.0))
        # And then either boarding or pickets, the same as the fence it hangs in.
        inner0, inner1 = leaf_x0 + STILE_SECTION[0], leaf_x1 - STILE_SECTION[0]
        infill = 0
        if board:
            leaf += _box((inner0, low, plane - BOARD_THICKNESS / 2.0),
                         (inner1, high, plane + BOARD_THICKNESS / 2.0))
            infill = 1
        else:
            span = inner1 - inner0
            count = max(1, int(span / PICKET_PITCH))
            pitch = span / count
            for picket in range(count):
                start = inner0 + picket * pitch + (pitch - PICKET_WIDTH) / 2.0
                leaf += _box((start, low, plane - PICKET_THICKNESS / 2.0),
                             (start + PICKET_WIDTH, high, plane + PICKET_THICKNESS / 2.0))
                infill += 1

        fixed: list[dict] = []
        hardware: list[tuple[str, str]] = []
        hinged = row.get("kind") in ("hinged", "bolted")
        # §11.2 says the pedestrian gate is "single hinge east", and a hinge side cannot be derived
        # from anything else in the file, so the layout carries it (`HOUSE-00767`). A gate that
        # does not say is hung at the LOW end of its own opening, which is a default and is
        # recorded as one.
        far_side = row.get("hinge") in ("east", "north")
        if hinged:
            # The hinge stile and the one the catch is on, which is the whole of what the side
            # decides: everything else about the leaf is symmetric.
            hinge_x, catch_x = (leaf_x1, leaf_x0) if far_side else (leaf_x0, leaf_x1)
            post_x = x1 if far_side else x0
            reach = -HINGE[0] if far_side else HINGE[0]
            for level in (low + 0.20, high - 0.30):
                leaf += _box((min(hinge_x, hinge_x + reach), level,
                              plane - HINGE[2] / 2.0 - thickness / 2.0),
                             (max(hinge_x, hinge_x + reach), level + HINGE[1],
                              plane - thickness / 2.0))
                fixed += _box((min(hinge_x, post_x + (HINGE[1] if far_side else -HINGE[1])), level,
                               plane - HINGE[2] / 2.0 - thickness / 2.0),
                              (max(hinge_x, post_x + (HINGE[1] if far_side else -HINGE[1])),
                               level + HINGE[1], plane - thickness / 2.0))
                hardware += [("hinge", "leaf"), ("hinge", "fixed")]
            catch = BOLT if row.get("kind") == "bolted" else LATCH
            span = catch[0] if far_side else -catch[0]
            leaf += _box((min(catch_x, catch_x + span), base + 0.95, plane + thickness / 2.0),
                         (max(catch_x, catch_x + span), base + 0.95 + catch[1],
                          plane + thickness / 2.0 + catch[2]))
            keeper = x0 if far_side else x1
            fixed += _box((min(catch_x, keeper), base + 0.95, plane + thickness / 2.0),
                          (max(catch_x, keeper), base + 0.95 + catch[1],
                           plane + thickness / 2.0 + catch[2]))
            name = "bolt" if row.get("kind") == "bolted" else "latch"
            hardware += [(name, "leaf"), (name, "fixed")]
            pivot = (hinge_x, base, plane)
            travel = None
        else:
            # A sliding gate: a track on the ground, two rollers under the leaf, and six metres of
            # travel along the fence line.
            fixed += _box((x0 - width, base, plane - TRACK_SECTION[0] / 2.0),
                          (x1, base + TRACK_SECTION[1], plane + TRACK_SECTION[0] / 2.0))
            hardware.append(("track", "fixed"))
            for at in (leaf_x0 + 0.4, leaf_x1 - 0.4):
                leaf += _box((at - ROLLER[0] / 2.0, base + TRACK_SECTION[1],
                              plane - ROLLER[1] / 2.0),
                             (at + ROLLER[0] / 2.0, base + TRACK_SECTION[1] + ROLLER[1],
                              plane + ROLLER[1] / 2.0))
                hardware.append(("roller", "leaf"))
            pivot = ((leaf_x0 + leaf_x1) / 2.0, base, plane)
            travel = (-width, 0.0, 0.0)

        points = [point for corners, _n in leaf + fixed for point in corners]
        only_leaf = [point for corners, _n in leaf for point in corners]
        out.append({
            "id": row["id"],
            "kind": row.get("kind"),
            "leafBounds": (min(p[0] for p in only_leaf), min(p[1] for p in only_leaf),
                           min(p[2] for p in only_leaf), max(p[0] for p in only_leaf),
                           max(p[1] for p in only_leaf), max(p[2] for p in only_leaf)),
            "style": style,
            "leaf": leaf,
            "fixed": fixed,
            "infill": infill,
            "hardware": hardware,
            "width": width,
            "pivot": pivot,
            "travel": travel,
            "interactable": row.get("interactable"),
            "hinge": row.get("hinge"),
            "bounds": (min(p[0] for p in points), min(p[1] for p in points),
                       min(p[2] for p in points), max(p[0] for p in points),
                       max(p[1] for p in points), max(p[2] for p in points)),
        })
    return out


def _panel(u0: float, u1: float, v0: float, v1: float,
           holes: list[tuple[float, float, float, float]]) -> list[tuple[float, float, float, float]]:
    """@p (u0, u1) x (v0, v1) with @p holes taken out of it, as the rectangles that are left.

    Bands first, then runs: split at every hole's u boundary, and inside each band take the holes
    that span it out of the height. The same shape of answer `house_shell_gen.panel` gives, done
    again here because that one lives inside Blender and this tool does not.
    """
    edges = sorted({u0, u1} | {edge for hole in holes for edge in (hole[0], hole[1])
                               if u0 - 1e-9 < edge < u1 + 1e-9})
    out = []
    for left, right in zip(edges, edges[1:]):
        if right - left <= 1e-6:
            continue
        cuts = [(hole[2], hole[3]) for hole in holes
                if hole[0] <= left + 1e-6 and hole[1] >= right - 1e-6]
        cursor = v0
        for cut_low, cut_high in sorted(cuts):
            if cut_low > cursor + 1e-6:
                out.append((left, right, cursor, min(cut_low, v1)))
            cursor = max(cursor, cut_high)
        if v1 > cursor + 1e-6:
            out.append((left, right, cursor, v1))
    return out


def shed(directory: Path) -> dict | None:
    """§11.1's garden shed: floor, four walls with their openings cut, and a gable roof.

    **An enterable cell, so it is built from the CELL and not from the footprint.** `EXT_SHED` is
    the volume a body stands in (3.2 m square, head at 2.35) and `STRUCT_SHED`'s footprint is that
    plus the wall thickness, which is where the walls go. Building it the other way round would put
    the walls inside the room and the door frame in the wrong plane from §16's portal.
    """
    layout = layout_io.load_layout(directory)
    exterior = layout.get("exterior") or {}
    structures = exterior.get("structures") or []
    if not structures:
        return None
    row = structures[0]
    cell = next((one for one in layout_io.rows(layout, "cells")
                 if one["id"] == row.get("cell")), None)
    if cell is None:
        return None

    box = cell["boxes"][0]
    ix0, ix1 = float(box["x"][0]), float(box["x"][1])
    iz0, iz1 = float(box["z"][0]), float(box["z"][1])
    override = cell.get("yOverride") or [0.0, 2.35]
    floor, head = float(override[0]), float(override[1])
    ox0, ox1 = float(row["footprint"]["x"][0]), float(row["footprint"]["x"][1])
    oz0, oz1 = float(row["footprint"]["z"][0]), float(row["footprint"]["z"][1])
    eaves = float(row.get("eavesY") or head)
    ridge = float(row.get("ridgeY") or eaves + 0.5)

    # §16's own openings, in the wall each one is in. A shed with a door drawn where the portal is
    # not is a shed you cannot walk into.
    holes: dict[tuple[str, float], list[tuple[float, float, float, float]]] = {}
    for portal in layout_io.rows(layout, "portals"):
        if cell["id"] not in (portal.get("cellA"), portal.get("cellB")):
            continue
        plane, rect = portal.get("plane") or {}, portal.get("rect") or {}
        if not rect.get("u") or not isinstance(plane.get("value"), (int, float)):
            continue
        key = (plane["axis"], round(float(plane["value"]), 4))
        holes.setdefault(key, []).append((float(rect["u"][0]), float(rect["u"][1]),
                                          floor + float(rect["v"][0]), floor + float(rect["v"][1])))

    parts: list[tuple[str, list]] = []
    walls: list[dict] = []
    # The four walls, each from the room's own face out to the footprint.
    for axis, plane, outer, other in (("x", ix0, ox0, (iz0, iz1)), ("x", ix1, ox1, (iz0, iz1)),
                                      ("z", iz0, oz0, (ix0, ix1)), ("z", iz1, oz1, (ix0, ix1))):
        cut = holes.get((axis, round(plane, 4)), [])
        for u0, u1, v0, v1 in _panel(other[0], other[1], floor, head, cut):
            low = (min(plane, outer), v0, u0) if axis == "x" else (u0, v0, min(plane, outer))
            high = (max(plane, outer), v1, u1) if axis == "x" else (u1, v1, max(plane, outer))
            walls += _box(low, high)
    parts.append(("STRUCT_SHED_WALLS", walls))

    slab = _box((ox0, floor - 0.10, oz0), (ox1, floor, oz1))
    parts.append(("STRUCT_SHED_FLOOR", slab))

    # A gable roof with its ridge along X, so the door is in a gable end -- which is what a shed
    # this shape is: 3.6 m square, eaves at 2.35 and the ridge half a metre over them.
    middle = (oz0 + oz1) / 2.0
    rise, run = ridge - eaves, abs(middle - oz0)
    length = (rise ** 2 + run ** 2) ** 0.5
    roof: list[dict] = []
    for near in (oz0, oz1):
        # The pitch's own normal, from the rise and the run, rather than a pair of numbers that
        # look about right: §11.1 gives eaves 2.35 and ridge 2.85 over 1.8 m, which is not 45°.
        outward = (0.0, run / length, (-rise / length) if near < middle else (rise / length))
        corners = [(ox0, eaves, near), (ox1, eaves, near), (ox1, ridge, middle), (ox0, ridge, middle)]
        if near < middle:
            corners = list(reversed(corners))
        roof.append((corners, outward))
    for gable in (ox0, ox1):
        corners = [(gable, eaves, oz0), (gable, eaves, oz1), (gable, ridge, middle)]
        if gable == ox1:
            corners = list(reversed(corners))
        roof.append((corners, (-1.0 if gable == ox0 else 1.0, 0.0, 0.0)))
    parts.append(("STRUCT_SHED_ROOF", roof))

    points = [point for _name, faces in parts for corners, _n in faces for point in corners]
    return {
        "id": row["id"],
        "cell": cell["id"],
        "parts": parts,
        "openings": sum(len(value) for value in holes.values()),
        "interior": (ix1 - ix0) * (iz1 - iz0),
        "eaves": eaves,
        "ridge": ridge,
        "bounds": (min(p[0] for p in points), min(p[1] for p in points), min(p[2] for p in points),
                   max(p[0] for p in points), max(p[1] for p in points), max(p[2] for p in points)),
    }


def _document(parts: list[tuple[str, list, dict]], material: str,
              style: str) -> tuple[dict, bytes]:
    """@p parts as one glTF document -- a node each, one shared material.

    `read_shell_geometry`'s shape, so `build_chunks.py` reads a fence the way it reads the shell.
    Several NODES rather than one, because a gate's leaf moves and its hinge straps do not: the
    part that swings is its own node with its pivot in `extras`, which is what §65's behaviour
    reads off the asset instead of re-deriving it from the layout.
    """
    blob = bytearray()
    accessors: list[dict] = []
    views: list[dict] = []
    meshes: list[dict] = []
    nodes: list[dict] = []

    def store(values: list[tuple], kind: str) -> int:
        count = {"VEC3": 3, "VEC2": 2}[kind]
        offset = len(blob)
        for value in values:
            blob.extend(struct.pack(f"<{count}f", *value[:count]))
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(blob) - offset})
        accessors.append({"bufferView": len(views) - 1, "componentType": 5126,
                          "count": len(values), "type": kind,
                          "min": [min(v[i] for v in values) for i in range(count)],
                          "max": [max(v[i] for v in values) for i in range(count)]})
        return len(accessors) - 1

    for part, faces, extras in parts:
        if not faces:
            continue
        positions: list[tuple[float, float, float]] = []
        normals: list[tuple[float, float, float]] = []
        uvs: list[tuple[float, float]] = []
        indices: list[int] = []
        for corners, normal in faces:
            base = len(positions)
            for point in corners:
                positions.append(point)
                normals.append(normal)
                # World metres, like the ground's: a fence's boards are the same size everywhere.
                uvs.append((point[0] + point[2], point[1]))
            # A quad is two triangles and a gable end is one: a face with three corners is a
            # triangle, not a quad with two of its corners in the same place.
            indices += ([base, base + 1, base + 2] if len(corners) == 3
                        else [base, base + 1, base + 2, base, base + 2, base + 3])
        position = store(positions, "VEC3")
        normal_at = store(normals, "VEC3")
        uv0 = store(uvs, "VEC2")
        offset = len(blob)
        for index in indices:
            blob.extend(struct.pack("<I", index))
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(indices) * 4})
        accessors.append({"bufferView": len(views) - 1, "componentType": 5125,
                          "count": len(indices), "type": "SCALAR"})
        meshes.append({"name": part, "primitives": [
            {"attributes": {"POSITION": position, "NORMAL": normal_at, "TEXCOORD_0": uv0},
             "indices": len(accessors) - 1, "material": 0, "mode": 4}]})
        node = {"name": part, "mesh": len(meshes) - 1}
        if extras:
            node["extras"] = extras
        nodes.append(node)

    document = {
        "asset": {"version": "2.0", "generator": "cna-house fence_gen.py"},
        "scene": 0,
        "scenes": [{"nodes": list(range(len(nodes)))}],
        "nodes": nodes,
        "meshes": meshes,
        "materials": [{"name": material,
                       "extras": {"surfaceClass": "fence",
                                  # Lit by the sun term like the rest of the boundary: a fence is
                                  # thin, and §18.3 keeps the lightmap for surfaces that carry
                                  # low-frequency light rather than for every board.
                                  "lightmapReceiver": False,
                                  "groundMaterial": style}}],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
    }
    return document, bytes(blob)


def _fence_document(fence: dict) -> tuple[dict, bytes]:
    return _document([(fence["id"], fence["faces"], {})],
                     f"FENCE_{fence['style']}", fence["style"])


def _gate_document(gate: dict) -> tuple[dict, bytes]:
    """A gate: the LEAF, with its pivot and travel, and the ironmongery that stays put."""
    moving = {"pivot": [round(value, 4) for value in gate["pivot"]],
              "gateKind": gate["kind"],
              "interactable": gate["interactable"],
              "width": round(gate["width"], 4)}
    if gate["travel"] is not None:
        moving["travel"] = [round(value, 4) for value in gate["travel"]]
    else:
        # A hinged leaf swings about +Y through its own pivot; the layout says which way the
        # pedestrian gate is hung and §11.2's other two are the same arrangement.
        moving["axis"] = [0.0, 1.0, 0.0]
    return _document([(f"{gate['id']}_LEAF", gate["leaf"], moving),
                      (f"{gate['id']}_FIXED", gate["fixed"], {"fixed": True})],
                     f"GATE_{gate['style']}", gate["style"])


def emit(directory: Path, output: Path) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    written, triangles = [], 0
    for fence in fences(directory):
        document, blob = _fence_document(fence)
        gltf_io.write_glb(output / f"{fence['id']}.glb", document, blob)
        written.append(fence["id"])
        triangles += len(fence["faces"]) * 2
    built = shed(directory)
    if built is not None:
        document, blob = _document([(name, faces, {}) for name, faces in built["parts"]],
                                   "SHED_timber", "shed")
        gltf_io.write_glb(output / f"{built['id']}.glb", document, blob)
        written.append(built["id"])
        triangles += sum(len(faces) * 2 for _name, faces in built["parts"])
    for gate in gates(directory):
        document, blob = _gate_document(gate)
        gltf_io.write_glb(output / f"{gate['id']}.glb", document, blob)
        written.append(gate["id"])
        triangles += (len(gate["leaf"]) + len(gate["fixed"])) * 2
    return {"written": written, "triangles": triangles}


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("fence_gen: selftest")
    if not (SOURCE / "layout.exterior.json").is_file():
        print("fence_gen: no exterior layout; nothing to check.")
        return 0

    runs = fences(SOURCE)
    layout = layout_io.load_layout(SOURCE)
    rows = (layout.get("exterior") or {}).get("fences", [])
    require(len(runs) == len(rows) == 7,
            f"§11.2's boundary is {len(runs)} runs -- three ornamental across the front and four "
            f"of board fence round the other three sides")

    # 1. Every run stands where the layout says, and no further.
    stray = []
    for run, row in zip(runs, rows):
        start, end = row["path"][0], row["path"][-1]
        lo = (min(start[0], end[0]), min(start[2], end[2]))
        hi = (max(start[0], end[0]), max(start[2], end[2]))
        # A post is a section wide, so the run may overhang its own end by half of one.
        margin = POST_SECTION / 2.0 + POST_CAP[0] / 2.0
        if (run["bounds"][0] < lo[0] - margin - 1e-6 or run["bounds"][3] > hi[0] + margin + 1e-6
                or run["bounds"][2] < lo[1] - margin - 1e-6
                or run["bounds"][5] > hi[1] + margin + 1e-6):
            stray.append(run["id"])
    require(not stray, f"and every one of them stands between its own two endpoints ({stray})")

    # 2. The gate openings are in the DATA and this is what proves the runs respect them.
    openings = (layout.get("exterior") or {}).get("gates", [])
    inside = []
    for gate in openings:
        opening = gate["opening"]
        for run in runs:
            if run["bounds"][0] > opening["x"][1] - 1e-6 or run["bounds"][3] < opening["x"][0] + 1e-6:
                continue
            if run["bounds"][2] > opening["z"][1] - 1e-6 or run["bounds"][5] < opening["z"][0] + 1e-6:
                continue
            overlap_x = min(run["bounds"][3], opening["x"][1]) - max(run["bounds"][0], opening["x"][0])
            if overlap_x > POST_SECTION + 1e-6:
                inside.append((gate["id"], run["id"], round(overlap_x, 3)))
    require(not inside,
            f"no fence stands in a gate opening -- the pedestrian gate, the drive and the rear "
            f"service gate ({inside})")

    # 3. Height: a post reaches its run's own height over the ground it stands on, and the cap
    #    sits on top of that.
    _w, _h, heights, _m = terrain_gen.decode(SOURCE)

    def ground(x: float, z: float) -> float:
        ix = min(max(int(round((x - terrain_gen.ORIGIN_X) / terrain_gen.STEP)), 0),
                 terrain_gen.WIDTH - 1)
        iz = min(max(int(round((z - terrain_gen.ORIGIN_Z) / terrain_gen.STEP)), 0),
                 terrain_gen.HEIGHT - 1)
        return heights[iz * terrain_gen.WIDTH + ix]

    short = []
    for run, row in zip(runs, rows):
        wanted = float(row["height"]) + POST_CAP[1]
        for x, z, base, top in run["postTops"]:
            if abs((top - base) - wanted) > 1e-6 or abs(base - ground(x, z)) > 1e-6:
                short.append((run["id"], round(x, 1), round(z, 1), round(top - base, 3),
                              round(base - ground(x, z), 3)))
    capless = [run["id"] for run in runs
               if abs(run["bounds"][4] - max(top for _x, _z, _base, top in run["postTops"])) > 1e-6]
    require(not short and not capless,
            f"every POST stands ON the height field and reaches its run's own height plus its cap -- "
            f"measured at each of the {sum(run['posts'] for run in runs)} of them, because a run "
            f"whose top was measured against one end's ground would read half a metre tall on a "
            f"lot that slopes. The cap is the highest thing in the run, which is what says it is "
            f"there at all ({short[:3]}{capless[:3]})")

    # 4. The rake: the lot slopes half a metre from the road to the rear fence, so a side fence's
    #    two ends cannot be at the same height. A fence that ignored the ground would be level and
    #    buried at one end.
    side = next(run for run in runs if run["id"] == "EXT_FENCE_W")
    ends = (ground(-22.5, 0.0), ground(-22.5, -48.0))
    require(abs(ends[0] - ends[1]) > 0.2,
            f"§10.2's lot really does slope under the west fence ({ends[0]:+.2f} to {ends[1]:+.2f})")
    flat = []
    for run in runs:
        start = run["postTops"][0]
        along_x = abs(run["postTops"][-1][0] - start[0]) >= abs(run["postTops"][-1][1] - start[1])
        for bay, rise in enumerate(run["rakes"]):
            here = ground(*(run["postTops"][bay][0], run["postTops"][bay][1]))
            there = ground(*(run["postTops"][bay + 1][0], run["postTops"][bay + 1][1]))
            if abs(rise[0] - here) > 1e-6 or abs(rise[1] - there) > 1e-6:
                flat.append((run["id"], bay, round(rise[0] - here, 3), round(rise[1] - there, 3)))
        assert along_x or True
    falls = [abs(rise[1] - rise[0]) for run in runs for rise in run["rakes"]]
    require(not flat and max(falls) > 0.01,
            f"so every bay's rails and boarding rake between the ground at ITS OWN two posts "
            f"(steepest bay {max(falls) * 1000:.0f} mm over {runs[0]['spacing']:.2f} m; {flat[:3]})")

    # 5. Posts, caps and rails -- the three things the task names -- are all there, in the numbers
    #    the spacing implies.
    counted = sum(run["posts"] for run in runs)
    require(all(run["posts"] == run["bays"] + 1 for run in runs) and counted > 80,
            f"{counted} posts over {sum(run['bays'] for run in runs)} bays, one at each end of "
            f"every run")
    railless = [(run["id"], len(run["rails"]), run["bays"]) for run in runs
                if len(run["rails"]) != 2 * run["bays"]
                or {round(level, 3) for _bay, level in run["rails"]}
                != {0.35, round(float(dict(zip([r["id"] for r in rows], rows))[run["id"]]["height"])
                                - 0.25, 3)}]
    require(not railless,
            f"and every bay carries TWO rails, at 0.35 m and a quarter-metre under its own top "
            f"({railless[:3]})")

    widest = 0.0
    for run in runs:
        tops = run["postTops"]
        for i in range(len(tops) - 1):
            widest = max(widest, ((tops[i + 1][0] - tops[i][0]) ** 2
                                  + (tops[i + 1][1] - tops[i][1]) ** 2) ** 0.5)
    require(widest <= POST_SPACING + 1e-6,
            f"and no bay is wider than the {POST_SPACING:.1f} m a fence may span between posts "
            f"(widest {widest:.3f} m)")
    ornamental = [run for run in runs if run["style"] == "ornamental"]
    board = [run for run in runs if run["style"] == "board"]
    require(len(ornamental) == 3 and len(board) == 4,
            "the front is ornamental and the boundary is board fence, which is what the layout's "
            "two asset names mean")
    pickets = sum(len(run["pickets"]) for run in ornamental)
    bays = sum(run["bays"] for run in ornamental)
    per_bay = [len(run["pickets"]) / max(run["bays"], 1) for run in ornamental]
    # The CLEAR span between two post faces, which is what the pickets fill -- not the centres.
    wanted = [(run["spacing"] - POST_SECTION) / PICKET_PITCH for run in ornamental]
    require(pickets > 200 and all(abs(a - b) <= 1.0 for a, b in zip(per_bay, wanted))
            and not any(run["pickets"] for run in board),
            f"and the ornamental runs are PICKETS with daylight between them, not a low board "
            f"fence -- {pickets} of them over {bays} bays, "
            f"{'/'.join(f'{value:.1f}' for value in per_bay)} a bay against the "
            f"{'/'.join(f'{value:.1f}' for value in wanted)} the {PICKET_PITCH:.2f} m pitch asks "
            f"for, and none at all on the board runs")
    gaps = []
    for run in ornamental:
        along_x = abs(run["bounds"][3] - run["bounds"][0]) >= abs(run["bounds"][5] - run["bounds"][2])
        line = sorted(picket[0] if along_x else picket[1] for picket in run["pickets"])
        gaps += [round(line[i + 1] - line[i], 3) for i in range(len(line) - 1)]
    require(gaps and min(gaps) > PICKET_WIDTH + 1e-6,
            f"with real daylight between them: the closest two are {min(gaps):.3f} m apart and a "
            f"picket is {PICKET_WIDTH:.2f} m wide")

    # 6. Wound outwards, or the fence is inside out from the street.
    wrong = []
    for run in runs:
        for corners, normal in run["faces"]:
            a, b, c = corners[0], corners[1], corners[2]
            u = tuple(b[k] - a[k] for k in range(3))
            v = tuple(c[k] - a[k] for k in range(3))
            cross = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
            if sum(cross[k] * normal[k] for k in range(3)) <= 1e-9:
                wrong.append(run["id"])
    require(not wrong, f"every face is wound the way its own normal says ({sorted(set(wrong))[:3]})")

    require(_fence_document(runs[0]) == _fence_document(fences(SOURCE)[0]),
            "a fence renders the same bytes twice")

    # 7. `HOUSE-00767`'s gates.
    hung = gates(SOURCE)
    rows = openings
    listed = ", ".join(f"{gate['id']} ({gate['kind']})" for gate in hung)
    require(len(hung) == len(rows) == 3 and {gate["kind"] for gate in hung}
            == {"hinged", "sliding", "bolted"},
            f"§11.2's three gates: {listed}")
    fits = []
    for gate, row in zip(hung, rows):
        opening = row["opening"]
        if (gate["leafBounds"][0] < float(opening["x"][0]) - 1e-6
                or gate["leafBounds"][3] > float(opening["x"][1]) + 1e-6):
            fits.append((gate["id"], round(gate["leafBounds"][0], 3),
                         round(gate["leafBounds"][3], 3)))
    require(not fits,
            f"and each LEAF fills its own opening and no more, so it cannot foul the fence it "
            f"hangs in. The bounds of the whole gate are wider on purpose: a hinge strap is "
            f"screwed to the post outside the opening, and the sliding gate's track has to reach "
            f"as far as the leaf travels ({fits})")
    gaps = [round(float(row["opening"]["x"][1]) - float(row["opening"]["x"][0])
                  - (gate["leafBounds"][3] - gate["leafBounds"][0]), 4)
            for gate, row in zip(hung, rows)]
    require(all(gap >= 2 * GATE_GAP - 1e-6 for gap in gaps),
            f"with a {GATE_GAP * 1000:.0f} mm gap at each side, which is what lets it swing "
            f"({gaps})")

    swinging = [gate for gate in hung if gate["travel"] is None]
    sliding = [gate for gate in hung if gate["travel"] is not None]
    require(len(swinging) == 2 and len(sliding) == 1,
            "two swing and one slides, which is §11.2's own arrangement")
    hinged_at = [(gate["id"], min(abs(gate["pivot"][0] - gate["leafBounds"][0]),
                                  abs(gate["pivot"][0] - gate["leafBounds"][3])),
                  gate["hinge"]) for gate in swinging]
    require(all(offset < 0.05 for _id, offset, _side in hinged_at),
            f"a swinging leaf's pivot is at ONE of its two stiles and not at its middle "
            f"({[(one[0], round(one[1], 3)) for one in hinged_at]})")
    authored = {gate["id"]: gate for gate in swinging if gate["hinge"]}
    wrong_side = [(gate_id, gate["hinge"], round(gate["pivot"][0], 2))
                  for gate_id, gate in authored.items()
                  if (gate["hinge"] in ("east", "north"))
                  != (abs(gate["pivot"][0] - gate["leafBounds"][3]) < 1e-6)]
    described = ", ".join(f"{gate_id} on its {gate['hinge']} stile"
                          for gate_id, gate in authored.items())
    require(authored and not wrong_side,
            f"and the one §11.2 says is hung EAST is hung east: {described} ({wrong_side})")
    require(all(abs(abs(gate["travel"][0]) - gate["width"]) < 1e-6 for gate in sliding),
            f"and the sliding gate travels its own width -- {sliding[0]['width']:.1f} m of "
            f"opening, {abs(sliding[0]['travel'][0]):.1f} m of travel")

    document, _blob = _gate_document(hung[0])
    names = [node["name"] for node in document["nodes"]]
    moving = next(node for node in document["nodes"] if node["name"].endswith("_LEAF"))
    require(names == [f"{hung[0]['id']}_LEAF", f"{hung[0]['id']}_FIXED"]
            and moving["extras"]["interactable"] == rows[0]["interactable"],
            f"the leaf and the ironmongery are SEPARATE nodes, and the leaf carries its pivot, its "
            f"axis and §65's interactable id ({names})")
    ironmongery = {gate["id"]: sorted(gate["hardware"]) for gate in hung}
    wanted = {
        "hinged": [("hinge", "fixed"), ("hinge", "fixed"), ("hinge", "leaf"), ("hinge", "leaf"),
                   ("latch", "fixed"), ("latch", "leaf")],
        "bolted": [("bolt", "fixed"), ("bolt", "leaf"), ("hinge", "fixed"), ("hinge", "fixed"),
                   ("hinge", "leaf"), ("hinge", "leaf")],
        "sliding": [("roller", "leaf"), ("roller", "leaf"), ("track", "fixed")],
    }
    missing = [(gate["id"], ironmongery[gate["id"]]) for gate in hung
               if ironmongery[gate["id"]] != wanted[gate["kind"]]]
    require(not missing,
            f"and every gate carries the ironmongery its kind needs, half on the leaf and half on "
            f"the post: two hinges and a latch, two hinges and a bolt, or a track and two rollers "
            f"({missing[:2]})")
    require(_gate_document(hung[0]) == _gate_document(gates(SOURCE)[0]),
            "a gate renders the same bytes twice")

    # 8. `HOUSE-00768`'s shed.
    built = shed(SOURCE)
    require(built is not None, "§11.1's garden shed is built from the layout's own structure row")
    if built is not None:
        structure = ((layout.get("exterior") or {}).get("structures") or [])[0]
        cell = next(one for one in layout_io.rows(layout, "cells") if one["id"] == built["cell"])
        box = cell["boxes"][0]
        interior = (float(box["x"][1]) - float(box["x"][0])) * (float(box["z"][1]) - float(box["z"][0]))
        require(abs(built["interior"] - interior) < 1e-6 and built["cell"] == structure["cell"],
                f"and it is built from the CELL a body stands in ({interior:.2f} m² of floor) "
                f"inside the footprint the structure declares, which is where the walls go")
        require(abs(built["bounds"][0] - float(structure["footprint"]["x"][0])) < 1e-6
                and abs(built["bounds"][3] - float(structure["footprint"]["x"][1])) < 1e-6
                and abs(built["bounds"][2] - float(structure["footprint"]["z"][0])) < 1e-6
                and abs(built["bounds"][5] - float(structure["footprint"]["z"][1])) < 1e-6,
                "so its walls stand exactly on that footprint, no wider and no narrower")
        require(abs(built["bounds"][4] - float(structure["ridgeY"])) < 1e-6
                and built["ridge"] > built["eaves"],
                f"its ridge is the layout's own {structure['ridgeY']} m and its roof really does "
                f"pitch from {built['eaves']:.2f} m of eaves")
        require(built["openings"] == 2,
                f"§11.1's one door and one window are cut in it, because §16's portals are where "
                f"the holes are ({built['openings']})")

        # A shed you can walk into: the door's own hole is empty all the way to the floor.
        door = next(portal for portal in layout_io.rows(layout, "portals")
                    if portal.get("id") == "P_EXT_GARDEN__EXT_SHED")
        plane = float(door["plane"]["value"])
        blocked = []
        for name, faces in built["parts"]:
            if name != "STRUCT_SHED_WALLS":
                continue
            for corners, _normal in faces:
                xs = [point[0] for point in corners]
                if not (min(xs) <= plane + 1e-6 and max(xs) >= plane - 1e-6):
                    continue
                zs = [point[2] for point in corners]
                ys = [point[1] for point in corners]
                du = min(max(zs), float(door["rect"]["u"][1])) - max(min(zs), float(door["rect"]["u"][0]))
                dv = min(max(ys), float(door["rect"]["v"][1])) - max(min(ys), float(door["rect"]["v"][0]))
                if du > 1e-6 and dv > 1e-6:
                    blocked.append((round(du, 3), round(dv, 3)))
        require(not blocked,
                f"and nothing is drawn across the doorway, which is what makes the cell enterable "
                f"({blocked[:3]})")

        wrong_shed = []
        for name, faces in built["parts"]:
            for corners, normal in faces:
                a, b, c = corners[0], corners[1], corners[2]
                u = tuple(b[k] - a[k] for k in range(3))
                v = tuple(c[k] - a[k] for k in range(3))
                cross = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                         u[0] * v[1] - u[1] * v[0])
                if sum(cross[k] * normal[k] for k in range(3)) <= 1e-9:
                    wrong_shed.append(name)
        require(not wrong_shed,
                f"every face of it is wound the way its own normal says, gable ends included "
                f"({sorted(set(wrong_shed))})")

    if failures:
        print(f"\nfence_gen: {len(failures)} claim(s) FAILED")
        return 1
    print("fence_gen: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", nargs="?", type=Path, default=SOURCE)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if not (args.directory / "layout.exterior.json").is_file():
        print(f"fence_gen: no exterior in {args.directory} yet -- nothing to generate.")
        return 0
    report = emit(args.directory, args.output)
    where = args.output.relative_to(REPO) if args.output.is_relative_to(REPO) else args.output
    print(f"fence_gen: {len(report['written'])} run(s) and gate(s), {report['triangles']} "
          f"triangles -> {where}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
