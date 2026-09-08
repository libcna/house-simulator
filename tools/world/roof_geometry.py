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


def roof_planes(box: tuple, eaves_y: float, pitch: float):
    """A hip roof over a rectangle: `(corners, outward)` per plane, in world coordinates.

    Two trapezoids along the long sides and two triangles at the ends -- or four triangles when the
    rectangle is square, which the garage's 8.4 x 8.4 wing is, and a pyramid is what a hip roof
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
        plane([(x1, eaves_y, z0), (x0, eaves_y, z0), (xm, top, ridge0[1])],
              (0.0, pitch, -1.0)),
        plane([(x0, eaves_y, z1), (x1, eaves_y, z1), (xm, top, ridge1[1])],
              (0.0, pitch, 1.0)),
    ]


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

    if failures:
        print(f"\nroof_geometry: {len(failures)} claim(s) FAILED")
        return 1
    print("roof_geometry: selftest passed.")
    return 0


if __name__ == "__main__":
    sys.exit(selftest())
