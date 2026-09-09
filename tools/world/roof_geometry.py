#!/usr/bin/env python3
"""roof_geometry.py -- where the roof planes are, in world coordinates.

`HOUSE-00472`. §12.1's roof is stated four ways that do not quite agree -- a ridge at +14.30, a
7:12 pitch, a 1.20 m knee wall and a 13.4 m span -- and `HOUSE-00461` settled that the ridge and
the pitch win. That resolution used to live inside `tools/blender/house_shell_gen.py`, which is a
Blender script and cannot be imported anywhere else, so `build_collision.py` had no way to know
where the attic's rafters are. It gave the attic a flat lid at the top of each cell's `yOverride`
box instead, which is §13.6's MAXIMUM head-room: you could stand at +13.90 anywhere in the west
store, three metres outside the roof.

So the resolution lives here, both tools import it, and the rafter envelope you collide with is
the roof you can see.

    tools/world/roof_geometry.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_io  # noqa: E402

#: How far the roof oversails the outer face of the wall, in metres. §12.1 says the attic is "under
#: a 7:12 roof over a 13.4 m span", and the main block is 12.80 m between wall centre lines, 13.10
#: between their outer faces: the missing 0.30 is 0.15 of overhang on each side. So the number is
#: §12.1's, arrived at by subtraction, and it is the one place it appears.
EAVES_OVERHANG = 0.15


def roof_boxes(layout: dict) -> dict:
    """`{name: centre-line rectangle}` for every roof this house has.

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


def outer_box(box: tuple, construction: dict) -> tuple:
    """@p box grown from wall centre lines to the eaves edge: half a wall plus the overhang."""
    reach = float(construction.get("wallExterior", 0.0)) / 2.0 + EAVES_OVERHANG
    return (box[0] - reach, box[1] + reach, box[2] - reach, box[3] + reach)


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


def roof_eaves(layout: dict, name: str, box: tuple, construction: dict) -> float:
    """Where a named roof's eaves are (`HOUSE-00484`).

    Two cases, and the difference is whether the layout says so:

    * a roof a LEVEL declares (`L3` declares `ROOF_MAIN`) is bounded by §12's own `ridgeY`, and
      `eaves_height` derives the eaves from it and the span. That is `HOUSE-00461`'s resolution and
      it stands;
    * a roof no level declares -- the garage wing -- has no authored ridge, and using the house's
      put `ROOF_GARAGE`'s eaves at **+11.675 over a garage whose head is +4.30**: a roof floating
      seven metres above the building it covers, which is what the blockout drew until this. Its
      eaves are the head of the cells it covers, which is where a wall stops and a roof starts.

    Derived rather than authored, because §12 gives the wing no ridge and no eaves and a number
    invented in `layout.levels.json` would be a number nobody could check.
    """
    outer = outer_box(box, construction)
    for level in layout_io.rows(layout, "levels"):
        if level.get("roof") == name:
            return eaves_height(construction, outer)

    levels = {row["id"]: row for row in layout_io.rows(layout, "levels")}
    heads = []
    for cell in layout_io.rows(layout, "cells"):
        level = levels.get(cell.get("level"))
        if level is None:
            continue
        for cx0, cx1, cz0, cz1 in layout_io.cell_boxes(cell):
            if (min(cx1, box[1]) - max(cx0, box[0]) > 1e-6
                    and min(cz1, box[3]) - max(cz0, box[2]) > 1e-6):
                heads.append(layout_io.cell_extent(cell, level)[1])
                break
    if not heads:
        return eaves_height(construction, outer)
    return max(heads)


def roof_planes(box: tuple, eaves_y: float, pitch: float, dormers=()):
    """A hip roof over a rectangle: `(corners, outward)` per plane, in world coordinates.

    Two trapezoids along the long sides and two triangles at the ends -- or four triangles when the
    rectangle is square, which the garage's 8.4 x 8.4 wing is, and a pyramid is what a hip roof
    over a square is.

    §12.1's roof is "hipped-and-gabled". The hips are here; the gables are the five dormers
    (`HOUSE-00462`) and the projecting garage wing, which is what makes the phrase true without
    this generator having to invent a gablet §12 never describes.

    @p dormers is `dormers_on`'s answer, and **the roof is not there where one comes through it**
    (`HOUSE-00490`). A wall dormer's own roof covers exactly its own footprint, so leaving the main
    plane whole underneath it puts a roof across the inside of every dormer window: from the attic
    you look at a slope where the window is, and §64.6 measured `L3_ROOM` -- a room §13.6 says is
    "lit by three dormers" -- at a sky exposure of 0.000. Each footprint is subtracted in plan, so
    a face becomes up to four, and the pieces are still convex.
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
        faces = [
            plane([(x0, eaves_y, z0), (x1, eaves_y, z0), (ridge1[0], top, zm),
                   (ridge0[0], top, zm)], (0.0, pitch, -1.0)),
            plane([(x1, eaves_y, z1), (x0, eaves_y, z1), (ridge0[0], top, zm),
                   (ridge1[0], top, zm)], (0.0, pitch, 1.0)),
            plane([(x0, eaves_y, z1), (x0, eaves_y, z0), (ridge0[0], top, zm)],
                  (-1.0, pitch, 0.0)),
            plane([(x1, eaves_y, z0), (x1, eaves_y, z1), (ridge1[0], top, zm)],
                  (1.0, pitch, 0.0)),
        ]
    else:
        xm = (x0 + x1) / 2.0
        ridge0, ridge1 = (xm, z0 + half), (xm, z1 - half)
        faces = [
            plane([(x0, eaves_y, z1), (x0, eaves_y, z0), (xm, top, ridge0[1]),
                   (xm, top, ridge1[1])], (-1.0, pitch, 0.0)),
            plane([(x1, eaves_y, z0), (x1, eaves_y, z1), (xm, top, ridge1[1]),
                   (xm, top, ridge0[1])], (1.0, pitch, 0.0)),
            plane([(x1, eaves_y, z0), (x0, eaves_y, z0), (xm, top, ridge0[1])],
                  (0.0, pitch, -1.0)),
            plane([(x0, eaves_y, z1), (x1, eaves_y, z1), (xm, top, ridge1[1])],
                  (0.0, pitch, 1.0)),
        ]
    if not dormers:
        return faces
    holes = [dormer_footprint(rect_u, rect_v, plane_z, box, eaves_y, pitch)
             for rect_u, rect_v, plane_z in dormers]
    out = []
    for corners, outward in faces:
        pieces = [corners]
        for hole in holes:
            pieces = [piece for whole in pieces for piece in subtract_rect(whole, hole)]
        out.extend((piece, outward) for piece in pieces)
    return out


#: A dormer's cheek thickness, and how far its front wall rises above the window head. Both were
#: `house_shell_gen.py`'s until `HOUSE-00490` needed the dormer in the collision as well as in the
#: picture: a shape that is drawn in one tool and collided in another has to be ONE shape.
DORMER_CHEEK = 0.10
DORMER_HEAD = 0.15


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
        # ...and ACROSS it as well. Filtering on the dormer's plane alone gave `ROOF_GARAGE` every
        # dormer of the main block, whose walls run through the same z range 15 m to the west
        # (`HOUSE-00484`): the garage's `BLOCKOUT_roof` spanned X -6.00 to 17.40 for a wing 8.4 m
        # wide.
        if float(rect["u"][0]) < box[0] - 1e-6 or float(rect["u"][1]) > box[1] + 1e-6:
            continue
        out.append(((float(rect["u"][0]), float(rect["u"][1])),
                    (float(rect["v"][0]), float(rect["v"][1])), value))
    return sorted(out)


def _dormer_metrics(rect_u: tuple, rect_v: tuple, plane_z: float, outer: tuple, eaves_y: float,
                    pitch: float):
    """The numbers `dormer_shell` and `dormer_footprint` must agree on, worked out once.

    They are the same dormer seen two ways -- the faces that are drawn and collided, and the
    rectangle of main roof they replace -- and two derivations of `back` would be two dormers.
    """
    u0, u1 = rect_u[0] - DORMER_CHEEK, rect_u[1] + DORMER_CHEEK
    head = rect_v[1] + DORMER_HEAD
    ridge_y = head + ((u1 - u0) / 2.0) * pitch
    # Which side of the roof this dormer is on: the eaves edge it faces is the near one.
    outward = 1.0 if abs(plane_z - outer[3]) < abs(plane_z - outer[2]) else -1.0
    eaves_z = outer[3] if outward > 0 else outer[2]
    # Where the dormer's ridge meets the main plane, which is where a dormer roof dies into a roof.
    back = eaves_z - outward * ((ridge_y - eaves_y) / pitch)
    return u0, u1, head, ridge_y, outward, eaves_z, back


def dormer_footprint(rect_u: tuple, rect_v: tuple, plane_z: float, outer: tuple, eaves_y: float,
                     pitch: float) -> tuple:
    """The plan rectangle of main roof one dormer replaces: `(x0, x1, z0, z1)`.

    From its front wall back to where its ridge dies into the slope, and no further: the eaves
    OVERSAIL the wall by `EAVES_OVERHANG`, and that strip of roof is in front of the dormer rather
    than under it. Cutting it too would leave a slot in the roof over the overhang.
    """
    u0, u1, _head, _ridge, _outward, _eaves_z, back = _dormer_metrics(
        rect_u, rect_v, plane_z, outer, eaves_y, pitch)
    return (u0, u1, min(plane_z, back), max(plane_z, back))


def dormer_shell(rect_u: tuple, rect_v: tuple, plane_z: float, outer: tuple, eaves_y: float,
                 pitch: float):
    """One dormer over a window, as `(corners, outward)` faces: front, two cheeks, two roof planes.

    A **wall** dormer: this house's five sit in the front and rear walls (`W_DORMER`'s portals are
    on the wall's own plane) and rise through the roof, rather than standing back on the slope. Its
    own roof is a little gable at the main roof's pitch, and it runs back until its ridge meets the
    main plane -- which is where a dormer roof dies into a roof.
    """
    u0, u1, head, ridge_y, outward, eaves_z, back = _dormer_metrics(
        rect_u, rect_v, plane_z, outer, eaves_y, pitch)

    def roof_at(z):
        """The main roof's surface at a given z on this side."""
        return eaves_y + abs(eaves_z - z) * pitch

    def facing(points, wanted):
        """@p points, reversed if their winding does not face @p wanted.

        `house_shell_gen.py` does this to every face it draws; the dormer does it HERE because
        `build_collision.py` collides these faces and has no `facing` of its own -- and a cheek
        has no "up" for `face_up` to use, which is how the roof planes are oriented.
        """
        (ax, ay, az), (bx, by, bz), (cx, cy, cz) = points[0], points[1], points[2]
        ux, uy, uz = bx - ax, by - ay, bz - az
        vx, vy, vz = cx - bx, cy - by, cz - bz
        normal = (uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx)
        return points if sum(n * o for n, o in zip(normal, wanted)) > 0 else list(reversed(points))

    mid = (u0 + u1) / 2.0
    front = (0.0, 0.0, outward)
    foot = roof_at(plane_z)
    # The front, from the roof surface at the wall up to the dormer's own ridge -- **with the
    # window left open** (`HOUSE-00490`). It was one pentagon until then, and a pentagon across
    # the whole width covers the window from the roof line (+10.57) to the head (+11.15): the
    # wall below has the opening punched out of it and the dormer put a board back over the top
    # two thirds of it. §13.6 calls `L3_ROOM` "lit by three dormers" and §64.6 measured its sky
    # exposure at 0.000.
    faces = [([(u0, head, plane_z), (u1, head, plane_z), (mid, ridge_y, plane_z)], front)]
    window_low, window_high = max(rect_v[0], foot), max(rect_v[1], foot)
    for left, right in ((u0, rect_u[0]), (rect_u[1], u1)):
        if right - left > 1e-9:
            faces.append(([(left, foot, plane_z), (right, foot, plane_z),
                           (right, head, plane_z), (left, head, plane_z)], front))
    for low, high in ((window_high, head), (foot, window_low)):
        if high - low > 1e-9:
            faces.append(([(rect_u[0], low, plane_z), (rect_u[1], low, plane_z),
                           (rect_u[1], high, plane_z), (rect_u[0], high, plane_z)], front))
    for edge, side in ((u0, -1.0), (u1, 1.0)):
        faces.append(([(edge, roof_at(plane_z), plane_z), (edge, head, plane_z),
                       (edge, roof_at(back), back)], (side, 0.0, 0.0)))
    for edge, side in ((u0, -1.0), (u1, 1.0)):
        faces.append(([(edge, head, plane_z), (mid, ridge_y, plane_z),
                       (mid, ridge_y, back), (edge, roof_at(back), back)],
                      (side, pitch, 0.0)))
    return [(facing(corners, outward_vector), outward_vector) for corners, outward_vector in faces]


def subtract_rect(corners, rect):
    """@p corners in plan, minus the axis-aligned @p rect: up to four convex pieces.

    Four `clip_to_rect` calls against the bands left round the hole rather than a general polygon
    difference, because the input is convex and the hole is a rectangle: the bands are convex, the
    pieces are `clip_to_rect`'s own answers, and the height of every vertex is still evaluated from
    the face's own plane rather than interpolated.
    """
    if len(corners) < 3:
        return []
    xs = [point[0] for point in corners]
    zs = [point[2] for point in corners]
    lo_x, hi_x, lo_z, hi_z = min(xs), max(xs), min(zs), max(zs)
    rx0, rx1, rz0, rz1 = rect
    if rx1 <= lo_x or rx0 >= hi_x or rz1 <= lo_z or rz0 >= hi_z:
        return [list(corners)]
    mid_x0, mid_x1 = max(rx0, lo_x), min(rx1, hi_x)
    out = []
    for band in ((lo_x, mid_x0, lo_z, hi_z),
                 (mid_x1, hi_x, lo_z, hi_z),
                 (mid_x0, mid_x1, lo_z, min(rz0, hi_z)),
                 (mid_x0, mid_x1, max(rz1, lo_z), hi_z)):
        if band[1] - band[0] <= 1e-9 or band[3] - band[2] <= 1e-9:
            continue
        piece = clip_to_rect(corners, band)
        if plan_area(piece) > 1e-9:
            out.append(piece)
    return out


#: The gutter's section and the fascia's depth, both `house_shell_gen.py`'s until `HOUSE-00776`
#: needed to know where the water comes out. The gutter hangs on the fascia, so the head of a
#: downspout is the eaves less the fascia's depth, and §37.3's splash lands under it.
FASCIA_DEPTH = 0.20
GUTTER_SECTION = 0.12
DOWNSPOUT_SECTION = 0.10

#: §14: -Z is north and +Z is the road, so a corner's name is the sign of its z first.
_CORNERS = (("NW", 0, 2), ("NE", 1, 2), ("SW", 0, 3), ("SE", 1, 3))


def downspouts(name: str, box: tuple, construction: dict, eaves_y: float) -> list[dict]:
    """One downspout at each corner of @p name's roof: `{id, roof, x, z, headY}`.

    `HOUSE-00468` has drawn these pipes since the shell had a roof, and nothing else knew they
    existed -- §37.3's splash particles and §37.4's trickle emitter both need to know where the
    water lands, and neither can read a Blender mesh. The head is the gutter's own height, which
    is the fascia's, which is why those two numbers moved here with it.

    @p box is the roof's centre-line rectangle; the pipe stands at the corner of the OUTER one,
    because that is where the gutter it drains is.
    """
    outer = outer_box(box, construction)
    return [{"id": f"DS_{name.removeprefix('ROOF_')}_{corner}",
             "roof": name,
             "x": outer[ix],
             "z": outer[iz],
             "headY": eaves_y - FASCIA_DEPTH}
            for corner, ix, iz in _CORNERS]


def house_downspouts(layout: dict) -> list[dict]:
    """Every downspout on the property, in id order.

    Four at each roof MINUS the corners that stand under another roof, which is not a detail: the
    garage wing projects east from the house's east wall, so `ROOF_GARAGE`'s outer north-west
    corner (+8.40, -22.00) is 0.60 m inside `ROOF_MAIN`'s footprint. A pipe there is a pipe in the
    dining room, and the water it carries has nowhere to land. A lower roof that dies into a wall
    drains the other way along its gutter, which is what a real one does.
    """
    construction = layout["levels"].get("construction") or {}
    boxes = roof_boxes(layout)
    outers = {name: outer_box(box, construction) for name, box in boxes.items()}
    out = []
    for name, box in sorted(boxes.items()):
        for row in downspouts(name, box, construction,
                              roof_eaves(layout, name, box, construction)):
            under = [other for other, rect in outers.items()
                     if other != name
                     and rect[0] < row["x"] < rect[1] and rect[2] < row["z"] < rect[3]]
            if under:
                row["under"] = sorted(under)[0]
                continue
            out.append(row)
    return sorted(out, key=lambda row: row["id"])


def plane_equations(planes):
    """`(a, b, c)` per face of `roof_planes`, dropping any that is not a plane."""
    out = []
    for corners, _outward in planes:
        equation = plane_equation(corners)
        if equation is not None:
            out.append(equation)
    return out


def roof_height(equations, x: float, z: float) -> float | None:
    """The roof SURFACE at (@p x, @p z): the lower envelope of @p equations.

    `HOUSE-00496`. A hip roof's four planes all rise inward, so the one whose plan polygon
    contains a point is the one that is LOWEST there -- checked over 400 random points against the
    polygon test, with no disagreement, and it is why anything under the roof can be expressed as
    the intersection of four half-spaces rather than as a point-in-polygon search. Being a minimum
    of linear functions it is concave, so the region under it is convex and a rectangle clipped
    against all four planes stays one polygon.
    """
    if not equations:
        return None
    return min(a * x + b * z + c for a, b, c in equations)


def plane_equation(corners):
    """`(a, b, c)` with `y = a*x + b*z + c` over @p corners, or None if they are not a plane.

    A roof face is planar by construction, so the height of a clipped piece of one is not
    interpolated: it is evaluated. Three corners that happen to be collinear in plan would leave
    the system singular, and a roof plane whose plan projection is a line has no area to clip
    against anything.
    """
    (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = corners[0], corners[1], corners[2]
    det = (x1 - x0) * (z2 - z0) - (x2 - x0) * (z1 - z0)
    if abs(det) < 1e-12:
        return None
    a = ((y1 - y0) * (z2 - z0) - (y2 - y0) * (z1 - z0)) / det
    b = ((x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)) / det
    return (a, b, y0 - a * x0 - b * z0)


def clip_to_rect(corners, rect):
    """@p corners clipped in plan to the axis-aligned @p rect `(x0, x1, z0, z1)`.

    Sutherland-Hodgman against four half-planes, with the height re-evaluated from the face's own
    plane rather than interpolated -- the two agree for a convex face and the second says why.
    Returns `[]` when nothing of the face is inside, which is the common answer: most cells lie
    under one plane of a hip roof, not four.
    """
    equation = plane_equation(corners)
    if equation is None:
        return []
    a, b, c = equation
    x0, x1, z0, z1 = rect
    polygon = [(point[0], point[2]) for point in corners]
    for inside, intersect in (
            (lambda p: p[0] >= x0, lambda p, q: _cut(p, q, 0, x0)),
            (lambda p: p[0] <= x1, lambda p, q: _cut(p, q, 0, x1)),
            (lambda p: p[1] >= z0, lambda p, q: _cut(p, q, 1, z0)),
            (lambda p: p[1] <= z1, lambda p, q: _cut(p, q, 1, z1))):
        if not polygon:
            return []
        clipped = []
        for index, current in enumerate(polygon):
            previous = polygon[index - 1]
            if inside(current):
                if not inside(previous):
                    clipped.append(intersect(previous, current))
                clipped.append(current)
            elif inside(previous):
                clipped.append(intersect(previous, current))
        polygon = clipped
    return _dedupe([(x, a * x + b * z + c, z) for x, z in polygon])


def _dedupe(polygon, tolerance: float = 1e-9):
    """@p polygon with repeated neighbours dropped, the wrap-around included.

    Sutherland-Hodgman emits a corner twice whenever a cut lands exactly on one the polygon
    already had, which over an axis-aligned house is most of them. The repeat is harmless to the
    area and not harmless to a triangle fan: the two triangles either side of it are degenerate,
    have no normal, and are shapes a sweep can never hit.
    """
    kept = []
    for point in polygon:
        if kept and max(abs(a - b) for a, b in zip(point, kept[-1])) <= tolerance:
            continue
        kept.append(point)
    while len(kept) > 1 and max(abs(a - b) for a, b in zip(kept[0], kept[-1])) <= tolerance:
        kept.pop()
    return kept


def _cut(p, q, axis: int, value: float):
    """Where the segment `p`-`q` crosses `axis == value`."""
    span = q[axis] - p[axis]
    t = 0.0 if abs(span) < 1e-15 else (value - p[axis]) / span
    return (p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t)


def face_up(polygon):
    """@p polygon reordered so that a triangle fan from its first vertex faces the SKY.

    `roof_planes` gives each face's corners in the order that reads naturally on the page and
    hands the caller an `outward` hint to orient them by; a caller that triangulates the corners
    as they come gets whichever way round they happened to be. For a roof there is nothing to
    decide -- every plane of it faces up -- so this settles it by measurement. §14's convention is
    counter-clockwise seen from outside, and outside a roof is above it.
    """
    if len(polygon) < 3:
        return list(polygon)
    # Summed over the WHOLE fan, not read off the first three corners: a clipped polygon often has
    # three collinear ones where a cut landed on an edge it already had, and their cross product is
    # zero. This total is twice the face's signed area in plan, which is zero only if it has none.
    total = 0.0
    ax, _ay, az = polygon[0]
    for index in range(1, len(polygon) - 1):
        bx, _by, bz = polygon[index]
        cx, _cy, cz = polygon[index + 1]
        total += (bz - az) * (cx - ax) - (bx - ax) * (cz - az)
    return list(polygon) if total > 0 else list(reversed(polygon))


def plan_area(polygon) -> float:
    """The shoelace area of @p polygon projected into plan, always positive."""
    if len(polygon) < 3:
        return 0.0
    total = 0.0
    for index, (x, _y, z) in enumerate(polygon):
        nx, _ny, nz = polygon[(index + 1) % len(polygon)]
        total += x * nz - nx * z
    return abs(total) / 2.0


# ======================================================================================= selftest


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("roof_geometry: selftest")
    source = Path(__file__).resolve().parents[2] / "assets-src" / "world"
    layout = layout_io.load_layout(source, kinds=["levels", "cells"])
    construction = layout["levels"].get("construction") or {}

    boxes = roof_boxes(layout)
    require(sorted(boxes) == ["ROOF_GARAGE", "ROOF_MAIN"],
            f"this house has two roofs ({sorted(boxes)})")

    outer = outer_box(boxes["ROOF_MAIN"], construction)
    require(abs((outer[1] - outer[0]) - 22.0) < 1e-9 and abs((outer[3] - outer[2]) - 13.4) < 1e-9,
            f"the main roof spans §12.1's own 13.4 m over the eaves "
            f"({outer[3] - outer[2]:.2f} m)")
    eaves = eaves_height(construction, outer)
    require(abs(eaves - 10.391669) < 1e-6,
            f"whose eaves are at +{eaves:.6f}, the ridge less half the span at 7:12")
    require(abs((float(construction["ridgeY"]) - eaves)
                - (13.4 / 2) * float(construction["roofPitch"])) < 1e-9,
            "-- which is what a pitch IS, so the two are not independently authored numbers")

    planes = roof_planes(outer, eaves, float(construction["roofPitch"]))
    require(len(planes) == 4, f"a hip roof is four planes ({len(planes)})")
    require(sorted(len(corners) for corners, _out in planes) == [3, 3, 4, 4],
            "two trapezoids along the long sides and a triangle at each end")
    tops = {round(max(point[1] for point in corners), 6) for corners, _out in planes}
    bottoms = {round(min(point[1] for point in corners), 6) for corners, _out in planes}
    require(tops == {round(float(construction["ridgeY"]), 6)} and bottoms == {round(eaves, 6)},
            f"every plane runs from the eaves to the ridge ({sorted(bottoms)} -> {sorted(tops)})")

    # `HOUSE-00484`: where each roof's eaves are, and why the two answers differ.
    garage_box = boxes["ROOF_GARAGE"]
    main_eaves = roof_eaves(layout, "ROOF_MAIN", boxes["ROOF_MAIN"], construction)
    garage_eaves = roof_eaves(layout, "ROOF_GARAGE", garage_box, construction)
    require(abs(main_eaves - eaves) < 1e-9,
            f"a roof a LEVEL declares keeps §12's ridge: `L3` declares `ROOF_MAIN` and its eaves "
            f"are still +{main_eaves:.4f}")
    require(abs(garage_eaves - 4.30) < 1e-9,
            f"a roof no level declares springs from the head of the cells it covers: the garage's "
            f"is +4.30 and so are its eaves ({garage_eaves})")
    garage_outer = outer_box(garage_box, construction)
    garage_ridge = garage_eaves + (min(garage_outer[1] - garage_outer[0],
                                       garage_outer[3] - garage_outer[2]) / 2.0) \
        * float(construction["roofPitch"])
    require(garage_ridge < float(construction["ridgeY"]),
            f"so the wing's ridge is +{garage_ridge:.3f}, BELOW the house's +"
            f"{float(construction['ridgeY']):.2f} -- it was +14.30 with its eaves at +11.675, "
            f"seven metres over the garage it covers")

    square = roof_planes((0.0, 8.4, 0.0, 8.4), 0.0, 0.5)
    require(all(len(corners) == 3 for corners, _out in square),
            "and over a square rectangle all four are triangles, which is a pyramid")

    # Clipping. A cell sees the piece of the roof over ITS OWN plan box and no more.
    corners, _outward = planes[0]
    whole = plan_area(corners)
    halves = [plan_area(clip_to_rect(corners, (outer[0], (outer[0] + outer[1]) / 2,
                                               outer[2], outer[3]))),
              plan_area(clip_to_rect(corners, ((outer[0] + outer[1]) / 2, outer[1],
                                               outer[2], outer[3])))]
    require(abs(sum(halves) - whole) < 1e-6 and min(halves) > 0.1,
            f"a plane cut in two loses nothing: {halves[0]:.3f} + {halves[1]:.3f} = "
            f"{whole:.3f} m²")
    require(not clip_to_rect(corners, (100.0, 200.0, 100.0, 200.0)),
            "a box nowhere near the roof gets no rafters at all")
    inside = clip_to_rect(corners, (outer[0] - 5, outer[1] + 5, outer[2] - 5, outer[3] + 5))
    require(abs(plan_area(inside) - whole) < 1e-9,
            "and a box that contains the whole plane gets the whole plane back")

    for face, _outward in planes:
        up = face_up(face)
        fan = [(0, index, index + 1) for index in range(1, len(up) - 1)]
        ys = []
        for a, b, c in fan:
            u = [up[b][i] - up[a][i] for i in range(3)]
            v = [up[c][i] - up[a][i] for i in range(3)]
            ys.append(u[2] * v[0] - u[0] * v[2])
        require(ys and all(value > 0 for value in ys),
                f"a fan over an up-faced plane points at the sky, every triangle of it "
                f"({len(ys)} triangles)")
    require(sorted(face_up(planes[0][0])) == sorted(planes[0][0]),
            "and facing one up only reorders its corners -- it is the same face")
    leading = clip_to_rect(planes[0][0], (outer[0], outer[0] + 3.0, outer[2], outer[2] + 3.0))
    require(len(leading) >= 3 and plan_area(face_up(leading)) > 0.1,
            f"a clipped corner of a roof plane still has area to face ({plan_area(leading):.2f} "
            f"m²)")

    # A cut that lands on a corner the face already had. Every roof plane over this house's
    # axis-aligned cells does it, and a repeated corner is two degenerate triangles in the fan.
    corner_cut = clip_to_rect(planes[2][0], (outer[0], outer[0] + 4.0, outer[2], outer[3]))
    require(corner_cut and all(
        max(abs(a - b) for a, b in zip(corner_cut[index], corner_cut[index - 1])) > 1e-9
        for index in range(len(corner_cut))),
        f"no clipped face repeats a corner, so no triangle of its fan is degenerate "
        f"({len(corner_cut)} corners)")
    degenerate = 0
    for face, _out in planes:
        for rect in ((outer[0], outer[0] + 4.0, outer[2], outer[3]),
                     (outer[0], outer[1], outer[2], outer[2] + 4.0),
                     (-6.0, 4.9, -24.0, -17.0)):
            up = face_up(clip_to_rect(face, rect))
            for index in range(1, len(up) - 1):
                u = [up[index][k] - up[0][k] for k in range(3)]
                v = [up[index + 1][k] - up[0][k] for k in range(3)]
                if abs(u[2] * v[0] - u[0] * v[2]) < 1e-12:
                    degenerate += 1
    require(degenerate == 0,
            f"and no fan over any of the four planes clipped to any of three boxes has one "
            f"({degenerate})")

    equation = plane_equation(corners)
    require(equation is not None
            and all(abs(equation[0] * x + equation[1] * z + equation[2] - y) < 1e-9
                    for x, y, z in corners),
            "the plane equation reproduces every corner of the face it came from")
    clipped = clip_to_rect(corners, (0.0, 4.0, outer[2], outer[2] + 3.0))
    require(clipped and all(eaves - 1e-9 <= point[1] <= float(construction["ridgeY"]) + 1e-9
                            for point in clipped),
            "and a clipped piece stays between the eaves and the ridge, so its height was "
            "evaluated rather than guessed")
    require(clipped and max(point[1] for point in clipped) > eaves + 1.0,
            f"...and it really does slope: {max(p[1] for p in clipped):.3f} against the eaves' "
            f"{eaves:.3f}")

    # ---- `HOUSE-00490`: the dormers, and the hole each one leaves in the plane it comes through.
    with_openings = layout_io.load_layout(source, kinds=["levels", "cells", "portals", "openings"])
    dormers = dormers_on(boxes["ROOF_MAIN"], layout_io.rows(with_openings, "portals"),
                         layout_io.rows(with_openings, "openings"))
    require(len(dormers) == 5,
            f"§12.1's five dormers are the five `W_DORMER` windows ({len(dormers)})")
    louvre = {"id": "P_FAKE", "plane": {"axis": "z", "value": boxes["ROOF_MAIN"][3]},
              "rect": {"u": [0.0, 0.8], "v": [11.3, 12.1]}}
    require(not dormers_on(boxes["ROOF_MAIN"], [louvre], [{"type": "W_GABLE", "portal": "P_FAKE"}]),
            "a gable louvre in the same wall is not a dormer -- it sits in a gable end rather "
            "than coming through the roof")
    require(len(dormers_on(boxes["ROOF_MAIN"], [louvre],
                           [{"type": "W_DORMER", "portal": "P_FAKE"}])) == 1,
            "...and the same opening as a dormer is")

    pitch = float(construction["roofPitch"])
    one = dormers[0]
    hole = dormer_footprint(one[0], one[1], one[2], outer, eaves, pitch)
    cut = roof_planes(outer, eaves, pitch, [one])
    plain_area = sum(plan_area(face) for face, _out in planes)
    cut_area = sum(plan_area(face) for face, _out in cut)
    hole_area = (hole[1] - hole[0]) * (hole[3] - hole[2])
    require(abs((plain_area - cut_area) - hole_area) < 1e-9,
            f"the roof is not there where a dormer comes through it: {plain_area:.4f} - "
            f"{cut_area:.4f} = {plain_area - cut_area:.4f} m² of plan, the dormer's own footprint "
            f"({hole_area:.4f})")
    shell = dormer_shell(one[0], one[1], one[2], outer, eaves, pitch)
    require(abs(sum(plan_area(face) for face, _out in shell) - hole_area) < 1e-9,
            f"...and the dormer's own roof covers exactly that hole, so the roof still has the "
            f"plan area it had ({sum(plan_area(f) for f, _o in shell):.4f} against "
            f"{hole_area:.4f})")
    require(max(hole[2], hole[3], key=lambda v: abs(v - one[2])) != one[2]
            and min(abs(hole[2] - one[2]), abs(hole[3] - one[2])) < 1e-9,
            f"the cut stops AT the dormer's front wall ({one[2]}) rather than at the eaves: the "
            f"roof oversails the wall by {EAVES_OVERHANG} m and that strip is in front of the "
            f"dormer, not under it ({hole[2]}..{hole[3]})")

    # The window is a HOLE in the dormer's front, which is what the front is for. It was one
    # pentagon across the whole width until this task, and the wall below has the opening cut out
    # of it: the dormer put a board back over the top two thirds of every dormer window in the
    # house, and §64.6 measured `L3_ROOM` -- "lit by three dormers" -- at 0.000.
    window_mid_y = (max(one[1][0], eaves + abs(outer[3] - one[2]) * pitch) + one[1][1]) / 2.0
    window_mid_x = (one[0][0] + one[0][1]) / 2.0
    covering = [face for face, _out in shell
                if all(abs(point[2] - one[2]) < 1e-9 for point in face)
                and min(p[0] for p in face) - 1e-9 <= window_mid_x <= max(p[0] for p in face) + 1e-9
                and min(p[1] for p in face) - 1e-9 <= window_mid_y <= max(p[1] for p in face) + 1e-9]
    require(not covering,
            f"the window is a hole in the dormer's front: nothing of the front stands at "
            f"({window_mid_x:.2f}, {window_mid_y:.2f}) ({len(covering)} face(s) do)")
    jambs = [face for face, _out in shell if all(abs(point[2] - one[2]) < 1e-9 for point in face)]
    require(len(jambs) == 4,
            f"...and the front is four pieces round it -- two jambs, a header and the gable above "
            f"({len(jambs)})")
    require(all(abs(plan_area(face)) < 1e-12 for face, _out in shell
                if all(abs(point[2] - one[2]) < 1e-9 for point in face)),
            "the front stands in one plane, so it has no plan area and takes none of the roof's")

    # Subtraction on its own, against answers that can be counted by hand.
    square_face = [(0.0, 0.0, 0.0), (4.0, 0.0, 0.0), (4.0, 0.0, 4.0), (0.0, 0.0, 4.0)]
    require(len(subtract_rect(square_face, (1.0, 2.0, 1.0, 2.0))) == 4,
            "a hole in the middle of a face leaves four pieces round it")
    require(abs(sum(plan_area(piece) for piece in subtract_rect(square_face, (1.0, 2.0, 1.0, 2.0)))
                - (16.0 - 1.0)) < 1e-9,
            "...whose area is the face's less the hole's, exactly")
    require(len(subtract_rect(square_face, (0.0, 2.0, 0.0, 2.0))) == 2,
            "a hole in a corner leaves two")
    require(subtract_rect(square_face, (9.0, 10.0, 9.0, 10.0)) == [square_face],
            "and a hole nowhere near the face leaves the face alone, uncopied and unclipped")
    require(subtract_rect(square_face, (-1.0, 5.0, -1.0, 5.0)) == [],
            "a hole that swallows the face leaves nothing")

    # ---- `HOUSE-00776`: the downspouts, and the two corners that do not get one.
    spouts = house_downspouts(layout)
    require(len(spouts) == 6,
            f"this house has six downspouts and not eight: four corners on each roof, less the "
            f"two where the roofs meet ({len(spouts)})")
    require([row["id"] for row in spouts] == sorted(row["id"] for row in spouts),
            "and they come back in id order, so a file written from them is stable")
    garage_outer = outer_box(boxes["ROOF_GARAGE"], construction)
    main_outer = outer_box(boxes["ROOF_MAIN"], construction)
    absent = {"DS_GARAGE_NW", "DS_MAIN_SE"} - {row["id"] for row in spouts}
    require(absent == {"DS_GARAGE_NW", "DS_MAIN_SE"},
            f"the two missing are the ones INSIDE the other roof: the garage wing projects from "
            f"the house's east wall, so a pipe at either would stand indoors "
            f"({sorted({'DS_GARAGE_NW', 'DS_MAIN_SE'} & {row['id'] for row in spouts})} are there)")
    require(main_outer[0] < 8.4 < main_outer[1] and main_outer[2] < -22.0 < main_outer[3],
            f"-- and that is a measurement, not an opinion: the garage's north-west corner "
            f"(8.40, -22.00) is inside `ROOF_MAIN`'s {[round(v, 2) for v in main_outer]}")
    require(garage_outer[0] < 9.0 < garage_outer[1] and garage_outer[2] < -14.0 < garage_outer[3],
            f"...and the main roof's south-east corner (9.00, -14.00) is inside `ROOF_GARAGE`'s "
            f"{[round(v, 2) for v in garage_outer]}")
    for row in spouts:
        rect = main_outer if row["roof"] == "ROOF_MAIN" else garage_outer
        require(row["x"] in (rect[0], rect[1]) and row["z"] in (rect[2], rect[3]),
                f"{row['id']} stands at a corner of {row['roof']}'s outer rectangle, which is "
                f"where the gutter it drains ends ({row['x']:.2f}, {row['z']:.2f})")
    main_eaves_y = roof_eaves(layout, "ROOF_MAIN", boxes["ROOF_MAIN"], construction)
    heads = {round(row["headY"], 6) for row in spouts if row["roof"] == "ROOF_MAIN"}
    require(heads == {round(main_eaves_y - FASCIA_DEPTH, 6)},
            f"and its head is the GUTTER's height -- the eaves less the fascia the gutter hangs "
            f"on -- rather than the eaves ({sorted(heads)} against {main_eaves_y:.3f})")

    if failures:
        print(f"\nroof_geometry: {len(failures)} claim(s) FAILED")
        return 1
    print("roof_geometry: selftest passed.")
    return 0


if __name__ == "__main__":
    sys.exit(selftest())
