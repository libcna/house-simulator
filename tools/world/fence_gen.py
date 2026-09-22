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
import outdoor_materials  # noqa: E402

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


def _transform_faces(faces: list, point_transform, normal_transform) -> list:
    """Apply one rigid static-pose transform to face points and normals."""
    return [([point_transform(*point) for point in corners], normal_transform(*normal))
            for corners, normal in faces]


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
    # Pick the hinged direction that opens toward the house/property rather than the public side
    # of its fence. This is derived from the interior architecture, not from a gate id.
    interiors = [box for cell in layout_io.rows(layout, "cells")
                 if cell.get("kind") != "exterior" for box in layout_io.cell_boxes(cell)]
    property_centre = (
        sum((box[0] + box[1]) / 2.0 for box in interiors) / len(interiors),
        sum((box[2] + box[3]) / 2.0 for box in interiors) / len(interiors),
    )
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

        closed_leaf = leaf
        closed_points = [point for corners, _n in closed_leaf for point in corners]
        closed_bounds = (min(p[0] for p in closed_points), min(p[1] for p in closed_points),
                         min(p[2] for p in closed_points), max(p[0] for p in closed_points),
                         max(p[1] for p in closed_points), max(p[2] for p in closed_points))
        fraction = max(0.0, min(1.0, float(row.get("openFraction") or 0.0)))
        if travel is not None:
            offset = tuple(component * fraction for component in travel)
            leaf = _transform_faces(
                leaf,
                lambda x, y, z: (x + offset[0], y + offset[1], z + offset[2]),
                lambda x, y, z: (x, y, z))
        elif fraction > 0.0:
            magnitude = math.radians(90.0 * fraction)
            leaf_centre = ((closed_bounds[0] + closed_bounds[3]) / 2.0,
                           (closed_bounds[2] + closed_bounds[5]) / 2.0)

            def rotated_centre(angle):
                dx, dz = leaf_centre[0] - pivot[0], leaf_centre[1] - pivot[2]
                return (pivot[0] + dx * math.cos(angle) - dz * math.sin(angle),
                        pivot[2] + dx * math.sin(angle) + dz * math.cos(angle))

            candidates = (magnitude, -magnitude)
            angle = min(candidates, key=lambda candidate: math.dist(
                rotated_centre(candidate), property_centre))
            cosine, sine = math.cos(angle), math.sin(angle)

            def rotate_point(x, y, z):
                dx, dz = x - pivot[0], z - pivot[2]
                return (pivot[0] + dx * cosine - dz * sine, y,
                        pivot[2] + dx * sine + dz * cosine)

            def rotate_normal(x, y, z):
                return (x * cosine - z * sine, y, x * sine + z * cosine)

            leaf = _transform_faces(leaf, rotate_point, rotate_normal)

        points = [point for corners, _n in leaf + fixed for point in corners]
        only_leaf = [point for corners, _n in leaf for point in corners]
        out.append({
            "id": row["id"],
            "kind": row.get("kind"),
            "leafBounds": (min(p[0] for p in only_leaf), min(p[1] for p in only_leaf),
                           min(p[2] for p in only_leaf), max(p[0] for p in only_leaf),
                           max(p[1] for p in only_leaf), max(p[2] for p in only_leaf)),
            "closedLeafBounds": closed_bounds,
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
            "openFraction": fraction,
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
    # The structure with a CELL, not the first row: §11.1's raised beds, trellis and compost bin
    # are structures too, and `garden_structures` draws those. A building is the one you can stand
    # inside, which is what having a cell means.
    row = next((one for one in exterior.get("structures") or [] if one.get("cell")), None)
    if row is None:
        return None
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
    openings_by_id = layout_io.by_id(layout_io.rows(layout, "openings"), "opening")
    shed_door = None
    for portal in layout_io.rows(layout, "portals"):
        if cell["id"] not in (portal.get("cellA"), portal.get("cellB")):
            continue
        plane, rect = portal.get("plane") or {}, portal.get("rect") or {}
        if not rect.get("u") or not isinstance(plane.get("value"), (int, float)):
            continue
        key = (plane["axis"], round(float(plane["value"]), 4))
        holes.setdefault(key, []).append((float(rect["u"][0]), float(rect["u"][1]),
                                          floor + float(rect["v"][0]), floor + float(rect["v"][1])))
        aperture = openings_by_id.get(portal.get("aperture"))
        if aperture and aperture.get("kind") == "door":
            shed_door = (portal, aperture)

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

    door_pose = None
    if shed_door is not None:
        portal, opening = shed_door
        plane_data, rect = portal["plane"], portal["rect"]
        if plane_data["axis"] != "x":
            raise layout_io.LayoutError("shed door must be in one of the two gable-end X walls")
        plane = float(plane_data["value"])
        outer = ox0 if abs(plane - ix0) < abs(plane - ix1) else ox1
        middle = (plane + outer) / 2.0
        hu0, hu1 = (float(value) for value in rect["u"])
        hv0, hv1 = (floor + float(value) for value in rect["v"])
        leaf_data = opening.get("leaf") or {}
        width = min(float(leaf_data.get("width") or hu1 - hu0), hu1 - hu0)
        height = min(float(leaf_data.get("height") or hv1 - hv0), hv1 - hv0)
        thickness = float(leaf_data.get("thickness") or 0.05)
        lu0 = (hu0 + hu1 - width) / 2.0
        lu1 = lu0 + width
        # Looking into EXT_SHED through its east wall, authored left is the high-Z jamb. The
        # positive local rotation carries the free edge west, into the shed.
        owner_on_low_x = abs(plane - ix0) < abs(plane - ix1)
        left = lu0 if owner_on_low_x else lu1
        hinge = left if opening.get("hinge") == "left" else (lu1 if left == lu0 else lu0)
        angle = math.radians(float(opening.get("maxAngleDeg") or 0.0)) \
            * max(0.0, min(1.0, float(opening.get("openFraction") or 0.0)))
        normal_sign = 1.0 if owner_on_low_x else -1.0
        delta_sign = 1.0 if hinge == lu0 else -1.0
        signed_angle = normal_sign * delta_sign * angle
        cosine, sine = math.cos(signed_angle), math.sin(signed_angle)

        def pose_point(x, y, z):
            across, normal = z - hinge, x - middle
            return (middle + across * sine + normal * cosine, y,
                    hinge + across * cosine - normal * sine)

        def pose_normal(x, y, z):
            return (z * sine + x * cosine, y, z * cosine - x * sine)

        closed = _box((middle - thickness / 2.0, hv0, lu0),
                      (middle + thickness / 2.0, hv0 + height, lu1))
        posed = _transform_faces(closed, pose_point, pose_normal)
        parts.append(("STRUCT_SHED_DOOR", posed))
        end = pose_point(middle, hv0, lu0 if hinge == lu1 else lu1)
        door_pose = {"id": opening["id"], "hinge": (middle, hv0, hinge),
                     "freeEdge": end, "openFraction": float(opening["openFraction"])}

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
        "doorPose": door_pose,
        "bounds": (min(p[0] for p in points), min(p[1] for p in points), min(p[2] for p in points),
                   max(p[0] for p in points), max(p[1] for p in points), max(p[2] for p in points)),
    }


# ============================================================================ garden structures

#: §11.1's raised beds, in the timber they are actually built from. A board 0.04 m thick and a
#: 0.07 m corner post is a cedar bed anyone has built; the bed's SIZE is authored, because how many
#: fit and where the paths between them run is the garden's business and not this file's.
BED_BOARD = 0.04
BED_POST = 0.07
#: Soil sits a hand's breadth below the rim, the way a bed that has been watered and topped up
#: does. A bed filled flush to the top is a bed that washes onto the path.
BED_FREEBOARD = 0.06

#: The trellis: two posts, a rail top and bottom, and a lattice between them. The pitch is what
#: makes it a trellis rather than a fence -- 0.25 m is a gap a runner bean can cross.
TRELLIS_POST = 0.07
TRELLIS_RAIL = (0.06, 0.035)
TRELLIS_SLAT = (0.03, 0.015)
TRELLIS_PITCH = 0.25

#: The compost bin: four posts and slatted sides, because compost has to breathe. The gap is what
#: makes it a bin and not a box.
COMPOST_POST = 0.07
COMPOST_BOARD = 0.15
COMPOST_GAP = 0.03
#: Its front is two boards high and the other three sides are full height: that is how you get a
#: barrow-load in and a fork-load out.
COMPOST_FRONT_BOARDS = 2

BED_ASSET = "MODEL_RAISED_BED_01"
TRELLIS_ASSET = "MODEL_TRELLIS_01"
COMPOST_ASSET = "MODEL_COMPOST_BIN_01"


def _bed(box: tuple[float, float, float, float], base: float, height: float) -> list[dict]:
    """One raised bed: four boards, four corner posts and the soil inside them."""
    x0, x1, z0, z1 = box
    faces: list[dict] = []
    top = base + height
    # The boards, mitred the easy way: the two along X run the full length and the two along Z fit
    # between them, so no two boards occupy the same volume.
    faces += _box((x0, base, z0), (x1, top, z0 + BED_BOARD))
    faces += _box((x0, base, z1 - BED_BOARD), (x1, top, z1))
    faces += _box((x0, base, z0 + BED_BOARD), (x0 + BED_BOARD, top, z1 - BED_BOARD))
    faces += _box((x1 - BED_BOARD, base, z0 + BED_BOARD), (x1, top, z1 - BED_BOARD))
    # Corner posts INSIDE the boards, which is what the boards are screwed to. They stop at the
    # rim rather than standing proud of it: a post that pokes up is a barked shin.
    for corner_x in (x0 + BED_BOARD, x1 - BED_BOARD - BED_POST):
        for corner_z in (z0 + BED_BOARD, z1 - BED_BOARD - BED_POST):
            faces += _box((corner_x, base - 0.05, corner_z),
                          (corner_x + BED_POST, top, corner_z + BED_POST))
    # The soil, one box, its top `BED_FREEBOARD` under the rim.
    faces += _box((x0 + BED_BOARD, base, z0 + BED_BOARD),
                  (x1 - BED_BOARD, top - BED_FREEBOARD, z1 - BED_BOARD))
    return faces


def _trellis(box: tuple[float, float, float, float], base: float, height: float) -> list[dict]:
    """§11.1's trellis: a lattice panel between two posts, along whichever axis it is longer in."""
    x0, x1, z0, z1 = box
    along_x = (x1 - x0) >= (z1 - z0)
    lo, hi = (x0, x1) if along_x else (z0, z1)
    cross_lo, cross_hi = (z0, z1) if along_x else (x0, x1)
    faces: list[dict] = []

    def slab(a0: float, a1: float, y0: float, y1: float, b0: float, b1: float) -> list[dict]:
        """A box given along-axis, height and cross-axis ranges, whichever way round they are."""
        return (_box((a0, y0, b0), (a1, y1, b1)) if along_x
                else _box((b0, y0, a0), (b1, y1, a1)))

    top = base + height
    # The posts are set 0.4 m into the ground -- a trellis catches wind like a sail.
    for post in (lo, hi - TRELLIS_POST):
        faces += slab(post, post + TRELLIS_POST, base - 0.40, top, cross_lo, cross_hi)
    for level in (base + 0.10, top - TRELLIS_RAIL[0]):
        faces += slab(lo + TRELLIS_POST, hi - TRELLIS_POST, level, level + TRELLIS_RAIL[0],
                      cross_lo, cross_lo + TRELLIS_RAIL[1])
    # The lattice: verticals at `TRELLIS_PITCH` and horizontals at the same, both inside the frame,
    # both on the same face of it. A count rather than a spacing, so the last gap is not a sliver.
    inner_lo, inner_hi = lo + TRELLIS_POST, hi - TRELLIS_POST
    inner_low, inner_high = base + 0.10 + TRELLIS_RAIL[0], top - TRELLIS_RAIL[0]
    verticals = max(1, int(round((inner_hi - inner_lo) / TRELLIS_PITCH)) - 1)
    horizontals = max(1, int(round((inner_high - inner_low) / TRELLIS_PITCH)) - 1)
    for index in range(verticals):
        at = inner_lo + (inner_hi - inner_lo) * (index + 1) / (verticals + 1)
        faces += slab(at - TRELLIS_SLAT[0] / 2.0, at + TRELLIS_SLAT[0] / 2.0, inner_low, inner_high,
                      cross_lo + TRELLIS_RAIL[1], cross_lo + TRELLIS_RAIL[1] + TRELLIS_SLAT[1])
    for index in range(horizontals):
        at = inner_low + (inner_high - inner_low) * (index + 1) / (horizontals + 1)
        faces += slab(inner_lo, inner_hi, at - TRELLIS_SLAT[0] / 2.0, at + TRELLIS_SLAT[0] / 2.0,
                      cross_lo + TRELLIS_RAIL[1] + TRELLIS_SLAT[1],
                      cross_lo + TRELLIS_RAIL[1] + 2 * TRELLIS_SLAT[1])
    return faces


def _compost(box: tuple[float, float, float, float], base: float, height: float) -> list[dict]:
    """The compost bin: four posts, three slatted sides and a low front you tip a barrow over.

    The front is the +X side, which is the one facing the garden it serves.
    """
    x0, x1, z0, z1 = box
    faces: list[dict] = []
    top = base + height
    for corner_x in (x0, x1 - COMPOST_POST):
        for corner_z in (z0, z1 - COMPOST_POST):
            faces += _box((corner_x, base - 0.20, corner_z),
                          (corner_x + COMPOST_POST, top, corner_z + COMPOST_POST))
    courses = max(1, int((height + COMPOST_GAP) // (COMPOST_BOARD + COMPOST_GAP)))
    for course in range(courses):
        low = base + course * (COMPOST_BOARD + COMPOST_GAP)
        high = low + COMPOST_BOARD
        # The two sides and the back, full height; the front only as high as `COMPOST_FRONT_BOARDS`.
        faces += _box((x0 + COMPOST_POST, low, z0), (x1 - COMPOST_POST, high, z0 + BOARD_THICKNESS))
        faces += _box((x0 + COMPOST_POST, low, z1 - BOARD_THICKNESS), (x1 - COMPOST_POST, high, z1))
        faces += _box((x0, low, z0 + COMPOST_POST), (x0 + BOARD_THICKNESS, high, z1 - COMPOST_POST))
        if course < COMPOST_FRONT_BOARDS:
            faces += _box((x1 - BOARD_THICKNESS, low, z0 + COMPOST_POST),
                          (x1, high, z1 - COMPOST_POST))
    return faces


#: §10.4's low stone wall at the west road end: a coped dry-stone wall, `HOUSE-00775`'s barrier.
STONE_WALL_ASSET = "MODEL_STONE_WALL_LOW"
#: The coping oversails the wall by this much on each face, and is this deep. A coping course is
#: what makes a stone wall read as one rather than as a low grey box.
COPING_OVERSAIL = 0.06
COPING_DEPTH = 0.12
#: How tall a course of stone is. Courses are drawn because the wall is 0.90 m tall and 3.4 m long
#: at the end of a road a player walks up to: a single box at that size reads as a kerb on end.
COURSE = 0.22


def _stone_wall(box: tuple[float, float, float, float], base: float, height: float) -> list[dict]:
    """§10.4's low stone wall: courses of stone with a coping over them.

    `HOUSE-00775` put it across the road end as a barrier, and nothing drew it: the wall is a
    `structures` row like the raised beds, and this tool refuses an asset it has no builder for --
    which is exactly what it did, loudly, for six weeks' worth of nobody running it (`HOUSE-00785`).
    """
    x0, x1, z0, z1 = box
    faces: list[dict] = []
    coping_low = base + max(0.0, height - COPING_DEPTH)
    courses = max(1, int(round(max(0.0, coping_low - base) / COURSE)))
    for course in range(courses):
        low = base + (coping_low - base) * course / courses
        high = base + (coping_low - base) * (course + 1) / courses
        # Alternate courses are set back a little, which is what gives a dry-stone wall its face.
        inset = 0.0 if course % 2 == 0 else 0.015
        faces += _box((x0 + inset, low, z0 + inset), (x1 - inset, high, z1 - inset))
    faces += _box((x0 - COPING_OVERSAIL, coping_low, z0 - COPING_OVERSAIL),
                  (x1 + COPING_OVERSAIL, base + height, z1 + COPING_OVERSAIL))
    return faces


#: Which builder draws which §11.1 structure. A structure whose asset is not here is a structure
#: nothing would draw, and `garden_structures` refuses rather than writing an empty file.
_GARDEN_BUILDERS = {BED_ASSET: _bed, TRELLIS_ASSET: _trellis, COMPOST_ASSET: _compost,
                    STONE_WALL_ASSET: _stone_wall}


def garden_structures(directory: Path) -> list[dict]:
    """§11.1's raised beds, trellis and compost bin: every structure that is not a building.

    **A structure with a `cell` is a building and `shed` draws it**, from the cell, because a body
    stands inside it. These have no cell and no inside: what they need from the world is the GROUND
    under them, which is §11.5's height field and not a level's floor, so each one sits on the
    lowest ground its own footprint covers and is buried 50 mm into it. A bed whose boards stop at
    the mean has daylight under its low corner, and this garden falls 0.29 m across itself.
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

    out = []
    for row in exterior.get("structures") or []:
        if row.get("cell"):
            continue
        builder = _GARDEN_BUILDERS.get(row.get("asset"))
        if builder is None:
            raise layout_io.LayoutError(
                f"structure {row['id']!r} has asset {row.get('asset')!r}, which nothing draws; "
                f"the ones this tool knows are {sorted(_GARDEN_BUILDERS)}")
        x0, x1 = float(row["footprint"]["x"][0]), float(row["footprint"]["x"][1])
        z0, z1 = float(row["footprint"]["z"][0]), float(row["footprint"]["z"][1])
        height = float(row.get("height") or 0.45)
        base = min(ground(x, z) for x in (x0, x1, (x0 + x1) / 2.0)
                   for z in (z0, z1, (z0 + z1) / 2.0)) - 0.05
        faces = builder((x0, x1, z0, z1), base, height)
        points = [point for corners, _n in faces for point in corners]
        out.append({
            "id": row["id"],
            "asset": row["asset"],
            "faces": faces,
            "base": base,
            "height": height,
            "footprint": (x0, x1, z0, z1),
            "bounds": (min(p[0] for p in points), min(p[1] for p in points), min(p[2] for p in points),
                       max(p[0] for p in points), max(p[1] for p in points), max(p[2] for p in points)),
        })
    return out


#: The style each garden structure is drawn in, for the material its faces carry.
_GARDEN_STYLE = {BED_ASSET: "bed", TRELLIS_ASSET: "trellis", COMPOST_ASSET: "compost",
                 STONE_WALL_ASSET: "stone"}


def _triangles(faces: list) -> int:
    """How many TRIANGLES a face list is: a quad is two and a gable end is one."""
    return sum(1 if len(corners) == 3 else 2 for corners, _normal in faces)


def _document(parts: list[tuple[str, list, dict]], material: str,
              style: str) -> tuple[dict, bytes]:
    """@p parts as one glTF document -- a node each, one shared material.

    `read_shell_geometry`'s shape, so `build_chunks.py` reads a fence the way it reads the shell.
    Several NODES rather than one, because a gate's leaf moves and its hinge straps do not: the
    part that swings is its own node with its pivot in `extras`, which is what §65's behaviour
    reads off the asset instead of re-deriving it from the layout.
    """
    material_id = outdoor_materials.ROLE_IDS.get(material)
    if material_id is None:
        raise layout_io.LayoutError(f"outdoor source role {material!r} has no canonical materialId")
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
                       "extras": {"materialId": material_id,
                                  "surfaceClass": "fence",
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
              "width": round(gate["width"], 4),
              "openFraction": gate["openFraction"],
              "staticPose": True}
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
        triangles += _triangles(fence["faces"])
    built = shed(directory)
    if built is not None:
        document, blob = _document([(name, faces, {}) for name, faces in built["parts"]],
                                   "SHED_timber", "shed")
        gltf_io.write_glb(output / f"{built['id']}.glb", document, blob)
        written.append(built["id"])
        triangles += sum(_triangles(faces) for _name, faces in built["parts"])
    for structure in garden_structures(directory):
        style = _GARDEN_STYLE[structure["asset"]]  # `_GARDEN_BUILDERS`'s keys, checked below
        document, blob = _document([(structure["id"], structure["faces"], {})],
                                   f"GARDEN_{style}", style)
        gltf_io.write_glb(output / f"{structure['id']}.glb", document, blob)
        written.append(structure["id"])
        triangles += _triangles(structure["faces"])
    for gate in gates(directory):
        document, blob = _gate_document(gate)
        gltf_io.write_glb(output / f"{gate['id']}.glb", document, blob)
        written.append(gate["id"])
        triangles += _triangles(gate["leaf"]) + _triangles(gate["fixed"])
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
    fence_slot = _fence_document(runs[0])[0]["materials"][0]
    require(fence_slot["extras"]["materialId"] ==
            outdoor_materials.ROLE_IDS[fence_slot["name"]],
            "and the fence slot names its canonical unbaked stock-XNA material")

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
        if (gate["closedLeafBounds"][0] < float(opening["x"][0]) - 1e-6
                or gate["closedLeafBounds"][3] > float(opening["x"][1]) + 1e-6):
            fits.append((gate["id"], round(gate["closedLeafBounds"][0], 3),
                         round(gate["closedLeafBounds"][3], 3)))
    require(not fits,
            f"and each LEAF fills its own opening and no more, so it cannot foul the fence it "
            f"hangs in. The bounds of the whole gate are wider on purpose: a hinge strap is "
            f"screwed to the post outside the opening, and the sliding gate's track has to reach "
            f"as far as the leaf travels ({fits})")
    gaps = [round(float(row["opening"]["x"][1]) - float(row["opening"]["x"][0])
                  - (gate["closedLeafBounds"][3] - gate["closedLeafBounds"][0]), 4)
            for gate, row in zip(hung, rows)]
    require(all(gap >= 2 * GATE_GAP - 1e-6 for gap in gaps),
            f"with a {GATE_GAP * 1000:.0f} mm gap at each side, which is what lets it swing "
            f"({gaps})")

    swinging = [gate for gate in hung if gate["travel"] is None]
    sliding = [gate for gate in hung if gate["travel"] is not None]
    require(len(swinging) == 2 and len(sliding) == 1,
            "two swing and one slides, which is §11.2's own arrangement")
    hinged_at = [(gate["id"], min(abs(gate["pivot"][0] - gate["closedLeafBounds"][0]),
                                  abs(gate["pivot"][0] - gate["closedLeafBounds"][3])),
                  gate["hinge"]) for gate in swinging]
    require(all(offset < 0.05 for _id, offset, _side in hinged_at),
            f"a swinging leaf's pivot is at ONE of its two stiles and not at its middle "
            f"({[(one[0], round(one[1], 3)) for one in hinged_at]})")
    authored = {gate["id"]: gate for gate in swinging if gate["hinge"]}
    wrong_side = [(gate_id, gate["hinge"], round(gate["pivot"][0], 2))
                  for gate_id, gate in authored.items()
                  if (gate["hinge"] in ("east", "north"))
                  != (abs(gate["pivot"][0] - gate["closedLeafBounds"][3]) < 1e-6)]
    described = ", ".join(f"{gate_id} on its {gate['hinge']} stile"
                          for gate_id, gate in authored.items())
    require(authored and not wrong_side,
            f"and the one §11.2 says is hung EAST is hung east: {described} ({wrong_side})")
    require(all(abs(abs(gate["travel"][0]) - gate["width"]) < 1e-6 for gate in sliding),
            f"and the sliding gate travels its own width -- {sliding[0]['width']:.1f} m of "
            f"opening, {abs(sliding[0]['travel'][0]):.1f} m of travel")
    by_kind = {gate["kind"]: gate for gate in hung}
    require(by_kind["hinged"]["leafBounds"][2] < -0.70
            and by_kind["hinged"]["leafBounds"][3]
            - by_kind["hinged"]["leafBounds"][0] < 0.35,
            "the pedestrian leaf is rotated about its east hinge and rests open into the property")
    drive_opening = next(row["opening"] for row in rows if row["kind"] == "sliding")
    require(by_kind["sliding"]["leafBounds"][3] <= float(drive_opening["x"][0]) + 1e-6,
            "and the driveway leaf is translated fully clear of its opening")
    require(by_kind["bolted"]["leafBounds"] == by_kind["bolted"]["closedLeafBounds"],
            "while the non-traversal rear gate remains at its authored closed pose")

    document, _blob = _gate_document(hung[0])
    gate_slot = document["materials"][0]
    require(gate_slot["extras"]["materialId"] ==
            outdoor_materials.ROLE_IDS[gate_slot["name"]],
            "the moving gate and fixed ironmongery share an authored stock-XNA material")
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
        pose = built["doorPose"]
        require(pose is not None and pose["openFraction"] >= 0.85
                and pose["freeEdge"][0] < pose["hinge"][0] - 0.70,
                "and its door rotates about the authored hinge into the shed, leaving the route "
                "visibly open")
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

        # ------------------------------------------------------------ §11.1's garden structures
        garden = garden_structures(SOURCE)
        by_asset: dict[str, list[dict]] = {}
        for one in garden:
            by_asset.setdefault(one["asset"], []).append(one)
        require(len(by_asset.get(BED_ASSET, [])) == 6
                and len(by_asset.get(TRELLIS_ASSET, [])) == 1
                and len(by_asset.get(COMPOST_ASSET, [])) == 1,
                f"§11.1's six raised beds, one trellis and one compost bin are drawn "
                f"({[(name.split('_', 1)[1], len(rows)) for name, rows in sorted(by_asset.items())]})")
        require(all(one["id"] != built["id"] for one in garden),
                "and the SHED is not among them: a structure with a cell is a building, and "
                "`shed` draws that one from the cell a body stands in")

        # They stand on §11.5's GROUND, which is not a floor and is not flat: the garden falls
        # 0.29 m across itself, so a structure drawn at y = 0 has daylight under one end of it.
        _w, _h, terrain_heights, _materials = terrain_gen.decode(SOURCE)

        def ground_at(x: float, z: float) -> float:
            ix = min(max(int(round((x - terrain_gen.ORIGIN_X) / terrain_gen.STEP)), 0),
                     terrain_gen.WIDTH - 1)
            iz = min(max(int(round((z - terrain_gen.ORIGIN_Z) / terrain_gen.STEP)), 0),
                     terrain_gen.HEIGHT - 1)
            return terrain_heights[iz * terrain_gen.WIDTH + ix]

        floating = []
        for one in garden:
            x0, x1, z0, z1 = one["footprint"]
            lowest = min(ground_at(x, z) for x in (x0, x1, (x0 + x1) / 2.0)
                         for z in (z0, z1, (z0 + z1) / 2.0))
            if one["bounds"][1] > lowest - 1e-6 or one["base"] > lowest - 0.049:
                floating.append((one["id"], round(one["base"], 3), round(lowest, 3)))
        require(not floating,
                f"every one of them is buried in the ground under its own footprint rather than "
                f"standing at zero ({floating[:3]})")
        require(all(abs((one["bounds"][4] - one["base"]) - one["height"]) < 1e-6
                    for one in garden if one["asset"] != TRELLIS_ASSET),
                f"and each is exactly the height the layout gives it, measured from that base "
                f"({[(one['id'], round(one['bounds'][4] - one['base'], 3)) for one in garden][:3]})")

        # Nothing stands in the shed or in a path, which is the defect that put this task here:
        # the six beds were authored as vegetation INSTANCES -- points -- and two of them were
        # inside the shed's walls, where a point is happy and a 2.2 x 1.1 m box is not.
        shed_box = (float(structure["footprint"]["x"][0]), float(structure["footprint"]["x"][1]),
                    float(structure["footprint"]["z"][0]), float(structure["footprint"]["z"][1]))
        paths = [(float(box["x"][0]), float(box["x"][1]), float(box["z"][0]), float(box["z"][1]))
                 for row in (layout.get("exterior") or {}).get("paths", []) for box in row["boxes"]]

        def overlap(a, b) -> bool:
            return (min(a[1], b[1]) - max(a[0], b[0]) > 1e-6
                    and min(a[3], b[3]) - max(a[2], b[2]) > 1e-6)

        # §10.4's barrier is the exemption `validate_world.py` rule 12 already makes and for the
        # same reason: the low stone wall at the road end stands ON the verge and ACROSS the
        # sidewalk, which is what a barrier does. Everything else in the garden that crosses a
        # path is a bed in a path (`HOUSE-00769` found three).
        clashes = [one["id"] for one in garden
                   if one.get("asset") != STONE_WALL_ASSET
                   and (overlap(one["footprint"], shed_box)
                        or any(overlap(one["footprint"], path) for path in paths))]
        require(not clashes,
                f"and none of them stands in the shed or in a path, the road-end wall aside "
                f"({clashes})")
        barriers = [one["id"] for one in garden if one.get("asset") == STONE_WALL_ASSET]
        # Every builder must have a ground style, and the pair is what `emit` indexes: a builder
        # with no style is a `KeyError` at write time, which is how this task's own fix failed the
        # first time it ran (`HOUSE-00785`).
        require(sorted(_GARDEN_BUILDERS) == sorted(_GARDEN_STYLE),
                f"every garden builder has a ground style and every style has a builder "
                f"({sorted(set(_GARDEN_BUILDERS) ^ set(_GARDEN_STYLE))})")

        require(barriers and any(overlap(one["footprint"], path)
                                 for one in garden if one["id"] in barriers for path in paths),
                f"-- and the road-end wall really does cross one, so the exemption is doing "
                f"something ({barriers})")
        pairs = [(a["id"], b["id"]) for index, a in enumerate(garden) for b in garden[index + 1:]
                 if overlap(a["footprint"], b["footprint"])]
        require(not pairs, f"nor in each other ({pairs})")

        # A bed is boards, posts and soil, and the soil is BELOW the rim: a bed filled flush is a
        # bed that washes onto the path the first time it rains.
        bed = by_asset[BED_ASSET][0]
        tops = sorted({round(point[1], 4) for corners, _n in bed["faces"] for point in corners})
        require(tops[-1] == round(bed["base"] + bed["height"], 4)
                and round(bed["base"] + bed["height"] - BED_FREEBOARD, 4) in tops,
                f"a bed's soil is {BED_FREEBOARD:.2f} m under its rim, and its rim is the top of "
                f"its boards ({tops[-3:]})")
        sides = {round(min(point[axis] for point in corners), 3)
                 for corners, _n in bed["faces"] for axis in (0, 2)}
        require({round(bed['footprint'][0], 3), round(bed['footprint'][2], 3)} <= sides,
                "and its boards run all the way round it, starting at its own footprint")

        # Every garden structure is BOXES -- `_box` and nothing else -- so its faces come in sixes
        # and each six is one timber. Chunking them is what lets the claims below count boards and
        # slats rather than faces, and the chunking is itself the first claim: six faces that are
        # not a box would make every number after it meaningless.
        def timbers(one: dict) -> list[tuple]:
            out = []
            faces = one["faces"]
            for start in range(0, len(faces), 6):
                corners = [point for corners, _n in faces[start:start + 6] for point in corners]
                out.append((min(p[0] for p in corners), min(p[1] for p in corners),
                            min(p[2] for p in corners), max(p[0] for p in corners),
                            max(p[1] for p in corners), max(p[2] for p in corners),
                            len({tuple(round(v, 5) for v in point) for point in corners}))
                           )
            return out

        require(all(len(one["faces"]) % 6 == 0 and all(box[6] == 8 for box in timbers(one))
                    for one in garden),
                "every garden structure is boxes: six faces each, eight corners each")

        # The trellis is a LATTICE, which is what makes it a trellis and not a fence panel.
        trellis = by_asset[TRELLIS_ASSET][0]
        pieces = timbers(trellis)
        uprights = sorted(box[0] for box in pieces
                          if box[3] - box[0] <= TRELLIS_SLAT[0] + 1e-6 and box[4] - box[1] > 0.5)
        crosspieces = [box for box in pieces
                       if box[4] - box[1] <= TRELLIS_SLAT[0] + 1e-6 and box[3] - box[0] > 0.5]
        require(len(uprights) >= 4 and len(crosspieces) >= 3,
                f"the trellis is a lattice: {len(uprights)} upright(s) and {len(crosspieces)} "
                f"horizontal(s) between its posts and rails")
        gaps = sorted(b - a for a, b in zip(uprights, uprights[1:]))
        require(gaps and gaps[-1] - gaps[0] < 1e-9
                and 0.15 <= gaps[0] <= TRELLIS_PITCH + 0.10,
                f"...evenly spaced, at a pitch a bean can cross "
                f"({[round(gap, 4) for gap in gaps[:3]]})")
        tx0, tx1, tz0, tz1 = trellis["footprint"]
        require(all(box[0] >= tx0 - 1e-6 and box[3] <= tx1 + 1e-6
                    and box[2] >= tz0 - 1e-6 and box[5] <= tz1 + 1e-6 for box in pieces),
                f"and every timber of it is inside its own {tz1 - tz0:.2f} m footprint -- a panel, "
                f"not a fence with a lean on it")

        # The compost bin is slatted and open-fronted: it has to breathe, and you have to get a
        # fork into it.
        compost = by_asset[COMPOST_ASSET][0]
        cx0, cx1, cz0, cz1 = compost["footprint"]
        pieces = timbers(compost)
        boards = [box for box in pieces if min(box[3] - box[0], box[5] - box[2]) <= BOARD_THICKNESS + 1e-6]
        front = [box for box in boards if box[0] >= cx1 - BOARD_THICKNESS - 1e-6]
        back = [box for box in boards if box[3] <= cx0 + BOARD_THICKNESS + 1e-6]
        require(front and back and max(box[4] for box in front) < max(box[4] for box in back) - 0.2,
                f"its front is lower than its back, which is how a barrow-load gets in "
                f"({len(front)} board(s) to {len(back)})")
        boarded = sum(box[4] - box[1] for box in back)
        courses = sorted((box[1], box[4]) for box in back)
        air = [high[0] - low[1] for low, high in zip(courses, courses[1:])]
        # 20 mm rather than `COMPOST_GAP`: a claim that reads the constant it is checking passes
        # whatever that constant becomes, including zero. This is the number a gap has to beat to
        # be one you can see daylight through.
        require(boarded < compost["height"] * 0.95 and air and min(air) >= 0.02,
                f"and its sides are SLATTED -- {boarded:.2f} m of board in {len(back)} courses "
                f"over a {compost['height']:.2f} m side, the tightest gap {min(air, default=0.0):.3f} m "
                f"-- because compost that cannot breathe is a bin of silage")
        require(len([box for box in pieces if box[3] - box[0] <= COMPOST_POST + 1e-6
                     and box[5] - box[2] <= COMPOST_POST + 1e-6 and box[4] - box[1] > 0.5]) == 4,
                "and it stands on four posts, one at each corner")

        wrong_garden = []
        for one in garden:
            for corners, normal in one["faces"]:
                a, b, c = corners[0], corners[1], corners[2]
                u = tuple(b[k] - a[k] for k in range(3))
                v = tuple(c[k] - a[k] for k in range(3))
                cross = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                         u[0] * v[1] - u[1] * v[0])
                if sum(cross[k] * normal[k] for k in range(3)) <= 1e-9:
                    wrong_garden.append(one["id"])
        require(not wrong_garden,
                f"and every face of all {len(garden)} of them is wound the way its own normal says "
                f"({sorted(set(wrong_garden))})")

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
