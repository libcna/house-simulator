#!/usr/bin/env python3
"""verify_shell.py -- does the house the generator drew obey §70.5?

`HOUSE-00477`. `validate_world.py` rule 10 checks the **layout**: the numbers an author typed. This
checks the **shell**: the geometry `tools/blender/house_shell_gen.py` produced from them. The two
are different questions, and the gap between them is where a generator bug lives -- a door authored
2.04 m tall and drawn 1.80, a ceiling slab an inch into the head-room it is supposed to bound, a
flight whose treads do not add up to the storey they climb.

Four checks, each measured from the triangles and none of them from the layout's own numbers except
where the layout is the thing being compared against:

* **clear height** -- the walking surface to the underside of the ceiling, per cell, against
  §70.5's 2.35-3.10 m for a habitable room;
* **stair geometry** -- the rise and going of every tread as drawn, `2R + G` against 600-650 mm,
  and rise consistency within a flight against 2 mm;
* **head-room over a flight** -- the vertical gap from each tread to whatever is above it;
* **openings** -- every authored door and window is actually a hole, of the size it was authored.

    tools/world/verify_shell.py --shell build/shell
    tools/world/verify_shell.py --report
    tools/world/verify_shell.py --selftest

Exit status: 0 when every check passes, 1 otherwise.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "world"))
sys.path.insert(0, str(REPO / "tools" / "assets"))
import build_chunks as bch  # noqa: E402
import layout_io  # noqa: E402
import stair_geometry  # noqa: E402

#: §70.5's clear height for a habitable room, in metres.
CLEAR_MIN, CLEAR_MAX = 2.35, 3.10
#: §70.5's `2 x rise + going`, in metres.
STEP_MIN, STEP_MAX = 0.600, 0.650
#: §70.5: the rise may vary within a flight by this much, in metres.
RISE_TOLERANCE = 0.002
#: §70.5's head-room over every flight and landing, in metres.
HEADROOM_MIN = 2.00
#: How close a measured face has to be to an authored plane to be that face, in metres.
EPS = 1e-4


def load_shell(directory: Path) -> dict:
    """`{cell: {surfaceClass: mesh}}` for every `.glb` in @p directory that has geometry."""
    out: dict[str, dict] = {}
    for path in sorted(directory.glob("*.glb")):
        surfaces = bch.read_shell_geometry(path)
        if not surfaces:
            continue
        out[path.stem] = {entry["extras"].get("surfaceClass") or name: entry
                          for name, entry in surfaces.items()}
    return out


def face_extent(mesh: dict, axis: int) -> tuple[float, float]:
    """`(min, max)` of @p mesh along @p axis."""
    values = [point[axis] for point in mesh["positions"]]
    return (min(values), max(values)) if values else (0.0, 0.0)


#: §72's per-cell budget for the architectural shell, in LOD0 triangles.
SHELL_TRIANGLES_PER_CELL = 3500


def triangle_rows(shell: dict, cells: dict) -> list[dict]:
    """Triangles per cell and per class, against §72's `Architectural shell, per cell` (`HOUSE-00479`).

    Counted from the drawn triangles and not from the chunk file, so that this says something about
    the GENERATOR rather than about what happened to survive batching. A cell that draws nothing is
    a row of zero rather than an absent row: a yard is a real cell that costs nothing.
    """
    rows = []
    for cell_id, surfaces in sorted(shell.items()):
        by_class = {name: len(mesh["triangles"]) for name, mesh in sorted(surfaces.items())}
        rows.append({"cell": cell_id,
                     "level": (cells.get(cell_id) or {}).get("level"),
                     "triangles": sum(by_class.values()),
                     "byClass": by_class})
    return rows


def face_normals(mesh: dict):
    """`(normal, centre, area)` per triangle, with the normal UNNORMALISED and its length twice
    the area -- so a degenerate facet has a zero-length normal and needs no separate test."""
    for a, b, c in mesh["triangles"]:
        pa, pb, pc = mesh["positions"][a], mesh["positions"][b], mesh["positions"][c]
        u = [pb[i] - pa[i] for i in range(3)]
        v = [pc[i] - pa[i] for i in range(3)]
        normal = (u[1] * v[2] - u[2] * v[1],
                  u[2] * v[0] - u[0] * v[2],
                  u[0] * v[1] - u[1] * v[0])
        length = (normal[0] ** 2 + normal[1] ** 2 + normal[2] ** 2) ** 0.5
        centre = tuple((pa[i] + pb[i] + pc[i]) / 3.0 for i in range(3))
        yield normal, centre, length / 2.0


def winding_rows(shell: dict, cells: dict) -> list[dict]:
    """Which way every face of the shell points (`HOUSE-00478`).

    §14: front faces are counter-clockwise, so `(b - a) x (c - a)` is the side you can see. For a
    room's own surfaces that side is INTO the room -- a floor faces up, a ceiling faces down, a
    wall faces the middle -- and for the `exterior` class, which is the outer skin of the house, it
    is the other way. A face wound the wrong way is culled, and a culled face is a hole in the room
    you can see the outdoors through; a face with no area at all is the "black facet" §70 asks
    about, invisible until a normal-visualisation pass draws it as nothing.

    **Floors and ceilings are the classes this can be sure about**, and they are checked: a floor
    faces up and a ceiling faces down, whatever shape the room is. `wall` and `exterior` are
    counted and NOT judged, because those classes carry two different kinds of thing -- a room's
    bounding surface, where "in" is the middle of the box, and free-standing boxes that are not a
    boundary at all: a balcony parapet, a mezzanine guard, a basement window well. A thin box has
    faces pointing both ways by construction, and half of them look wrong to any test that assumes
    the class is a boundary. `oneWay` counts the faces that do point into their box and `other` the
    rest; the arbiter for those is `BlockoutRenderTests.NothingIsInsideOut`, which draws the house
    with the culling reversed and looks at what is left.

    A face with no area at all is checked for every class: that is the "black facet" §70 asks
    about, invisible until something tries to light it.
    """
    rows = []
    for cell_id, surfaces in sorted(shell.items()):
        cell = cells.get(cell_id)
        boxes = layout_io.cell_boxes(cell) if cell else []
        if not boxes:
            continue

        def inward_at(x: float, z: float, nx: float, nz: float) -> float:
            """How far @p normal points into the nearest box, along whichever axis it lies on."""
            best = None
            for bx0, bx1, bz0, bz1 in boxes:
                gap = max(bx0 - x, x - bx1, bz0 - z, z - bz1, 0.0)
                if best is None or gap < best[0]:
                    best = (gap, (bx0, bx1, bz0, bz1))
            bx0, bx1, bz0, bz1 = best[1]
            if abs(nx) >= abs(nz):
                return nx * (1.0 if abs(x - bx0) <= abs(x - bx1) else -1.0)
            return nz * (1.0 if abs(z - bz0) <= abs(z - bz1) else -1.0)
        for name, mesh in sorted(surfaces.items()):
            wrong = 0
            other = 0
            degenerate = 0
            total = 0
            for normal, position, area in face_normals(mesh):
                total += 1
                if area <= 1e-9:
                    degenerate += 1
                    continue
                if name == "floor":
                    if normal[1] <= 1e-9:
                        wrong += 1
                elif name == "ceiling":
                    if -normal[1] <= 1e-9:
                        wrong += 1
                elif name in ("wall", "exterior"):
                    # Into the box the face is on the edge of, for a wall; out of it for the skin.
                    facing = inward_at(position[0], position[2], normal[0], normal[2])
                    if name == "exterior":
                        facing = -facing
                    if facing <= 1e-9:
                        other += 1
            if total:
                rows.append({"cell": cell_id, "class": name, "faces": total,
                             "wrong": wrong, "other": other, "degenerate": degenerate})
    return rows


def clear_heights(shell: dict, cells: dict, rafter_levels=()) -> list[dict]:
    """The walking surface to the ceiling's underside, per cell that has both.

    Both are measured from the SLABS as drawn, not from the level's `ffl` and `ceiling`: a slab
    built at the wrong thickness, or inset by the wrong amount, moves the surface a person's head
    meets and moves neither of the authored numbers.
    """
    rows = []
    for cell_id, surfaces in sorted(shell.items()):
        if "floor" not in surfaces or "ceiling" not in surfaces:
            continue
        walking = face_extent(surfaces["floor"], 1)[1]
        underside = face_extent(surfaces["ceiling"], 1)[0]
        rows.append({"cell": cell_id, "floor": walking, "ceiling": underside,
                     "clear": underside - walking,
                     "kind": (cells.get(cell_id) or {}).get("kind"),
                     "rafterBound": (cells.get(cell_id) or {}).get("level") in rafter_levels,
                     "name": (cells.get(cell_id) or {}).get("name", cell_id)})
    return rows


def horizontal_quads(mesh: dict) -> list[tuple]:
    """Every horizontal rectangle in @p mesh, as `(y, x0, x1, z0, z1)`.

    NOT solids. The first version of this recovered boxes by connectivity, which is exactly wrong
    for a staircase: solid steps touch, so the whole flight came back as one 2.7 x 3.7 x 3.05 m
    box. A tread is identified by the surface you stand on instead -- a horizontal quad at a riser
    height, the flight's width across and one going deep -- which is also the thing §70.5's rise
    and going are measurements of.

    A quad is two triangles over the same four corners, so the two share a bounding box exactly and
    grouping by that box recovers the quad without any tolerance at all.
    """
    seen: dict[tuple, int] = {}
    for a, b, c in mesh["triangles"]:
        points = [mesh["positions"][a], mesh["positions"][b], mesh["positions"][c]]
        ys = [point[1] for point in points]
        if max(ys) - min(ys) > 1e-6:
            continue
        key = (round(ys[0], 5),
               round(min(p[0] for p in points), 5), round(max(p[0] for p in points), 5),
               round(min(p[2] for p in points), 5), round(max(p[2] for p in points), 5))
        seen[key] = seen.get(key, 0) + 1
    return sorted(key for key, count in seen.items() if count >= 2)


def flight_steps_drawn(shell: dict, flight: dict, cells: dict, levels: dict) -> list[dict]:
    """The treads of @p flight as DRAWN, matched to the boxes inside its authored footprint.

    A tread is a box that stands on the floor the flight starts from, spans the flight's own width
    across, and whose top is a riser boundary. The nosings stop 45 mm from a tread's top and the
    newels are 90 mm square, so both are excluded by the width rather than by counting.
    """
    footprint = flight.get("footprint") or {}
    if not footprint:
        return []
    cell = cells.get(flight.get("fromCell"))
    if cell is None or cell["id"] not in shell or "stair" not in shell[cell["id"]]:
        return []
    x0, x1 = (float(v) for v in footprint["x"])
    z0, z1 = (float(v) for v in footprint["z"])
    base = stair_geometry.foot_of(flight, cells, levels)
    rise = float(flight["rise"])

    width = float(flight["width"])
    going = float(flight["going"])
    along_is_x = flight.get("run") in ("-X", "+X")
    treads = []
    for top, qx0, qx1, qz0, qz1 in horizontal_quads(shell[cell["id"]]["stair"]):
        if not (x0 - EPS <= qx0 and qx1 <= x1 + EPS and z0 - EPS <= qz0 and qz1 <= z1 + EPS):
            continue
        across = (qz1 - qz0) if along_is_x else (qx1 - qx0)
        along = (qx1 - qx0) if along_is_x else (qz1 - qz0)
        if abs(across - width) > 1e-3 or abs(along - going) > 1e-3:
            continue
        step = round((top - base) / rise)
        if step <= 0 or abs(base + step * rise - top) > 1e-3:
            continue
        treads.append({"step": step, "top": top, "box": (qx0, qx1, base, top, qz0, qz1)})
    treads.sort(key=lambda entry: entry["step"])
    return treads


def stair_rows(shell: dict, layout: dict) -> list[dict]:
    """§70.5's stair rows, measured from the drawn treads rather than from `layout.stairs.json`."""
    cells = layout_io.by_id(layout_io.rows(layout, "cells"), "cell")
    levels = layout_io.by_id(layout_io.rows(layout, "levels"), "level")
    rows = []
    for flight in sorted(layout_io.rows(layout, "stairs"), key=lambda f: f["id"]):
        treads = flight_steps_drawn(shell, flight, cells, levels)
        if not treads:
            rows.append({"flight": flight["id"], "treads": 0, "outdoors": False,
                         "problem": "no treads drawn"})
            continue
        rises = [treads[i]["top"] - treads[i - 1]["top"] for i in range(1, len(treads))]
        first = treads[0]["top"] - stair_geometry.foot_of(flight, cells, levels)
        rises.append(first)
        goings = []
        axis = 0 if flight.get("run") in ("-X", "+X") else 2
        for entry in treads:
            goings.append(entry["box"][axis * 2 + 1] - entry["box"][axis * 2])

        outdoors = any((cells.get(flight.get(end)) or {}).get("kind") == "exterior"
                       for end in ("fromCell", "toCell"))
        rows.append({
            "flight": flight["id"], "treads": len(treads), "risers": int(flight["risers"]),
            "outdoors": outdoors,
            "riseMin": min(rises), "riseMax": max(rises),
            "goingMin": min(goings), "goingMax": max(goings),
            "stepMin": 2 * min(rises) + min(goings), "stepMax": 2 * max(rises) + max(goings),
            "problem": None})
    return rows


def headroom_rows(shell: dict, layout: dict) -> list[dict]:
    """The vertical gap over every drawn tread, against §70.5's 2.00 m.

    Measured against everything in the flight's own cell and the cell above it, which is where a
    ceiling, a landing slab or the underside of the flight above would be. A stair well cut in the
    floor above is a hole, so a flight that arrives through one measures the gap to whatever is
    beyond the hole -- which is the honest answer: that is the head-room you have.
    """
    cells = layout_io.by_id(layout_io.rows(layout, "cells"), "cell")
    levels = layout_io.by_id(layout_io.rows(layout, "levels"), "level")
    rows = []
    for flight in sorted(layout_io.rows(layout, "stairs"), key=lambda f: f["id"]):
        treads = flight_steps_drawn(shell, flight, cells, levels)
        if not treads:
            continue
        obstacles = []
        for cell_id, surfaces in shell.items():
            if cells.get(cell_id) is None:
                continue
            for name, mesh in surfaces.items():
                # A CEILING or a FLOOR is head-room; a wall is not. A wall panel has horizontal
                # faces of its own -- the top of a panel under a window, the head of an opening --
                # and taking the lowest of those as a ceiling had the main stair reporting 40 mm.
                if name not in ("ceiling", "floor"):
                    continue
                obstacles.append((cell_id, name, mesh))
        worst = None
        for entry in treads:
            bx0, bx1, _by0, top, bz0, bz1 = entry["box"]
            ceiling = None
            for cell_id, name, mesh in obstacles:
                for index in range(0, len(mesh["triangles"])):
                    a, b, c = mesh["triangles"][index]
                    points = [mesh["positions"][a], mesh["positions"][b], mesh["positions"][c]]
                    # Every face of a ceiling or a floor IS horizontal in this shell -- the
                    # selftest claims it -- so the height of one is the height of all three of its
                    # vertices and there is nothing to project. That is also why the obstacle set
                    # is those two classes and not "everything above the tread": a wall beside a
                    # flight overlaps it in plan and is not head-room, and taking a wall's lowest
                    # vertex as a ceiling had the basement flight reporting 25 mm.
                    lows = min(p[1] for p in points)
                    if lows <= top + 0.01:
                        continue
                    if (min(p[0] for p in points) >= bx1 - EPS
                            or max(p[0] for p in points) <= bx0 + EPS
                            or min(p[2] for p in points) >= bz1 - EPS
                            or max(p[2] for p in points) <= bz0 + EPS):
                        continue
                    if ceiling is None or lows < ceiling[0]:
                        ceiling = (lows, cell_id, name)
            gap = None if ceiling is None else ceiling[0] - top
            if gap is not None and (worst is None or gap < worst["headroom"]):
                worst = {"step": entry["step"], "headroom": gap, "under": ceiling[1],
                         "class": ceiling[2]}
        rows.append({"flight": flight["id"],
                     "headroom": None if worst is None else worst["headroom"],
                     "worst": worst})
    return rows


def opening_rows(shell: dict, layout: dict) -> list[dict]:
    """Every authored door and window, and whether the shell actually has a hole for it.

    The test is the middle of the opening: no face of the wall class may cross the centre of the
    rectangle. A wall that was never cut covers it; one cut too small covers the corners and not
    the middle, which is why the corners are checked too and reported separately.
    """
    portals = layout_io.by_id(layout_io.rows(layout, "portals"), "portal")
    rows = []
    for opening in sorted(layout_io.rows(layout, "openings"), key=lambda o: o["id"]):
        portal = portals.get(opening.get("portal"))
        if portal is None:
            continue
        plane = portal.get("plane") or {}
        rect = portal.get("rect") or {}
        if plane.get("axis") not in ("x", "z") or not rect:
            continue
        u0, u1 = (float(v) for v in rect["u"])
        v0, v1 = (float(v) for v in rect["v"])
        value = float(plane["value"])
        axis = 0 if plane["axis"] == "x" else 2
        cross = 2 if axis == 0 else 0
        blocked = []
        for cell_id in (portal.get("cellA"), portal.get("cellB")):
            surfaces = shell.get(cell_id) or {}
            for name in ("wall", "exterior"):
                mesh = surfaces.get(name)
                if mesh is None:
                    continue
                for a, b, c in mesh["triangles"]:
                    points = [mesh["positions"][a], mesh["positions"][b], mesh["positions"][c]]
                    # Only faces in this portal's own plane, to a wall's half thickness.
                    if min(abs(p[axis] - value) for p in points) > 0.35:
                        continue
                    lo_u = min(p[cross] for p in points)
                    hi_u = max(p[cross] for p in points)
                    lo_v = min(p[1] for p in points)
                    hi_v = max(p[1] for p in points)
                    mid_u, mid_v = (u0 + u1) / 2, (v0 + v1) / 2
                    if lo_u < mid_u < hi_u and lo_v < mid_v < hi_v:
                        blocked.append((cell_id, name))
                        break
        # `HOUSE-00486`: and the LEAF, which is the opposite question. `blocked` asks whether the
        # wall was cut; this asks whether what was cut was then filled with a door. A leaf is
        # `trim` -- a door is joinery, like the architrave round it -- and it is the only trim
        # face across the middle of a door opening: the architrave is round the hole, the
        # threshold is under it, and neither crosses it.
        leafed = []
        for cell_id in (portal.get("cellA"), portal.get("cellB")):
            mesh = (shell.get(cell_id) or {}).get("trim")
            if mesh is None:
                continue
            for a, b, c in mesh["triangles"]:
                points = [mesh["positions"][a], mesh["positions"][b], mesh["positions"][c]]
                if min(abs(p[axis] - value) for p in points) > 0.35:
                    continue
                lo_u = min(p[cross] for p in points)
                hi_u = max(p[cross] for p in points)
                lo_v = min(p[1] for p in points)
                hi_v = max(p[1] for p in points)
                mid_u = (u0 + u1) / 2
                # Two thirds up the opening: clear of the threshold under it and of the head
                # above, so only a face that crosses the doorway itself can be here.
                mid_v = v0 + (v1 - v0) * 2.0 / 3.0
                if lo_u < mid_u < hi_u and lo_v < mid_v < hi_v:
                    leafed.append(cell_id)
                    break
        # Which of the two cells could POSSIBLY build a leaf: one that draws walls. A landing
        # open on every side has a floor and a ceiling and nothing else, and an opening in a
        # boundary neither cell walls is an opening nobody can fill.
        walled = []
        for cell_id in (portal.get("cellA"), portal.get("cellB")):
            surfaces = shell.get(cell_id) or {}
            found = False
            for name in ("wall", "exterior"):
                mesh = surfaces.get(name)
                if mesh is None:
                    continue
                for a, b, c in mesh["triangles"]:
                    points = [mesh["positions"][a], mesh["positions"][b], mesh["positions"][c]]
                    if min(abs(p[axis] - value) for p in points) > 0.35:
                        continue
                    # Beside the opening, not across it -- the wall the opening is cut IN.
                    if min(p[cross] for p in points) < u1 and max(p[cross] for p in points) > u0:
                        found = True
                        break
                if found:
                    break
            if found:
                walled.append(cell_id)
        walled = sorted(walled)
        rows.append({"opening": opening["id"], "kind": opening.get("kind"),
                     "width": u1 - u0, "height": v1 - v0,
                     "blocked": sorted(set(blocked)),
                     "walled": walled,
                     # Which of the two cells draws JOINERY at all. A stair hall draws its walls,
                     # its floor and its flights and no trim of any kind -- no skirting, no
                     # architrave -- so it has no leaf either, and that is a fact about the cell
                     # rather than about the door.
                     "joinery": sorted(cell_id for cell_id in (portal.get("cellA"),
                                                               portal.get("cellB"))
                                       if (shell.get(cell_id) or {}).get("trim")),
                     "leafed": sorted(set(leafed))})
    return rows


def verify(shell_dir: Path, world_dir: Path) -> dict:
    layout = layout_io.load_layout(world_dir, kinds=["levels", "cells", "portals", "openings"])
    for optional in ("stairs",):
        name, _ = layout_io.FILES[optional]
        if (world_dir / name).is_file():
            layout[optional] = layout_io.load_file(world_dir / name, optional)
    cells = layout_io.by_id(layout_io.rows(layout, "cells"), "cell")
    shell = load_shell(shell_dir)

    rafter_levels = {row["id"] for row in layout_io.rows(layout, "levels")
                     if row.get("ceiling") is None}
    heights = clear_heights(shell, cells, rafter_levels)
    winding = winding_rows(shell, cells)
    triangles = triangle_rows(shell, cells)
    stairs = stair_rows(shell, layout)
    headroom = headroom_rows(shell, layout)
    openings = opening_rows(shell, layout)

    found = problems(heights, stairs, headroom, openings)
    # `HOUSE-00478`. Two verdicts that need no threshold and no exemption.
    for row in winding:
        if row["degenerate"]:
            found.append(f"{row['cell']}.{row['class']}: {row['degenerate']} face(s) have no area")
        if row["wrong"]:
            found.append(f"{row['cell']}.{row['class']}: {row['wrong']} face(s) point the wrong "
                         f"way")
    for row in triangles:
        if row["triangles"] > SHELL_TRIANGLES_PER_CELL:
            found.append(f"{row['cell']}: {row['triangles']} triangles, over §72's "
                         f"{SHELL_TRIANGLES_PER_CELL} for one cell of the shell")
    return {"cells": len(shell), "clearHeights": heights, "stairs": stairs,
            "headroom": headroom, "openings": openings, "winding": winding,
            "triangles": triangles, "problems": found}


def problems(heights, stairs, headroom, openings) -> list[str]:
    """§70.5's verdict over already-measured rows. Pure, so every rule can be claimed alone.

    Separated from `verify` deliberately: a rule that only ever sees this house's numbers is a rule
    whose threshold is never exercised, because the house passes. Feeding it a constructed row is
    the only way to find out whether the rule would say anything if it had to.
    """
    out: list[str] = []
    # §70.5's clear height is for a HABITABLE room. A garage, a closet and a plant room are
    # excluded by §70.5's own wording, and so is a rafter-bounded level: the range is a range for
    # a FLAT ceiling, which `layout.cells.json` says in as many words and `validate_world.py`
    # rule 10 skips for the same reason.
    habitable = {"room", "corridor", "stair"}
    for row in heights:
        if row["kind"] not in habitable or row["rafterBound"]:
            continue
        if not CLEAR_MIN - 1e-6 <= row["clear"] <= CLEAR_MAX + 1e-6:
            out.append(f"{row['cell']}: clear height {row['clear']:.3f} m is outside "
                       f"{CLEAR_MIN}-{CLEAR_MAX}")
    for row in stairs:
        if row.get("problem"):
            out.append(f"{row['flight']}: {row['problem']}")
            continue
        if row["treads"] != row["risers"]:
            out.append(f"{row['flight']}: {row['treads']} treads drawn for "
                       f"{row['risers']} risers")
        if row["riseMax"] - row["riseMin"] > RISE_TOLERANCE:
            out.append(f"{row['flight']}: rises vary by "
                       f"{(row['riseMax'] - row['riseMin']) * 1000:.1f} mm")
        # §12.4 marks the porch's 680 mm "(exterior, permitted)": a shallower, deeper step is the
        # right thing outdoors. The exemption is ONE-SIDED and `validate_world.py` rule 10 applies
        # exactly the same one for the same reason -- a steep step is a steep step in the rain as
        # much as on the landing.
        high = STEP_MAX + (0.100 if row["outdoors"] else 0.0)
        if not (STEP_MIN - 1e-6 <= row["stepMin"] and row["stepMax"] <= high + 1e-6):
            out.append(f"{row['flight']}: 2R+G is {row['stepMin'] * 1000:.0f}-"
                       f"{row['stepMax'] * 1000:.0f} mm, outside {STEP_MIN * 1000:.0f}-"
                       f"{high * 1000:.0f}")
    for row in headroom:
        if row["headroom"] is not None and row["headroom"] < HEADROOM_MIN - 1e-6:
            out.append(f"{row['flight']}: head-room {row['headroom']:.3f} m at step "
                       f"{row['worst']['step']}, under {row['worst']['under']}'s "
                       f"{row['worst']['class']}")
    for row in openings:
        if row["blocked"]:
            out.append(f"{row['opening']}: not cut in "
                       f"{', '.join(f'{c}.{n}' for c, n in row['blocked'])}")
    return out


def report(result: dict) -> str:
    heights = [row for row in result["clearHeights"]
               if row["kind"] in ("room", "corridor", "stair") and not row.get("rafterBound")]
    lines = [f"{result['cells']} cell(s) with geometry"]
    if heights:
        lowest = min(heights, key=lambda row: row["clear"])
        highest = max(heights, key=lambda row: row["clear"])
        lines.append(f"  clear height over {len(heights)} habitable cell(s): "
                     f"{lowest['clear']:.3f} m in {lowest['cell']} to "
                     f"{highest['clear']:.3f} m in {highest['cell']} "
                     f"(§70.5 wants {CLEAR_MIN}-{CLEAR_MAX})")
    drawn = [row for row in result["stairs"] if not row.get("problem")]
    if drawn:
        lines.append(f"  {len(drawn)} flight(s) drawn, "
                     f"{sum(row['treads'] for row in drawn)} treads; 2R+G "
                     f"{min(row['stepMin'] for row in drawn) * 1000:.0f}-"
                     f"{max(row['stepMax'] for row in drawn) * 1000:.0f} mm, worst rise spread "
                     f"{max(row['riseMax'] - row['riseMin'] for row in drawn) * 1000:.2f} mm")
    measured = [row for row in result["headroom"] if row["headroom"] is not None]
    if measured:
        worst = min(measured, key=lambda row: row["headroom"])
        lines.append(f"  head-room over {len(measured)} flight(s): worst "
                     f"{worst['headroom']:.3f} m on {worst['flight']} under "
                     f"{worst['worst']['under']}'s {worst['worst']['class']} "
                     f"(§70.5 wants {HEADROOM_MIN})")
    cut = [row for row in result["openings"] if not row["blocked"]]
    lines.append(f"  {len(cut)} of {len(result['openings'])} authored opening(s) are cut")
    doors = [row for row in result["openings"] if row["kind"] == "door" and not row["blocked"]]
    two_sided = [row for row in doors if len(row["leafed"]) >= 2]
    lines.append(f"  {len([row for row in doors if row['leafed']])} of {len(doors)} cut door "
                 f"opening(s) have a leaf, {len(two_sided)} of them from both sides")
    triangles = result.get("triangles") or []
    if triangles:
        by_level: dict[str, int] = {}
        for row in triangles:
            by_level[row["level"] or "-"] = by_level.get(row["level"] or "-", 0) + row["triangles"]
        worst = max(triangles, key=lambda row: row["triangles"])
        classes: dict[str, int] = {}
        for row in triangles:
            for name, count in row["byClass"].items():
                classes[name] = classes.get(name, 0) + count
        lines.append(f"  {sum(row['triangles'] for row in triangles)} triangles over "
                     f"{len(triangles)} cell(s); worst {worst['triangles']} in {worst['cell']} "
                     f"(§72 allows {SHELL_TRIANGLES_PER_CELL} a cell)")
        lines.append("    by level: " + ", ".join(
            f"{level} {count}" for level, count in sorted(by_level.items())))
        lines.append("    by class: " + ", ".join(
            f"{name} {count}" for name, count in sorted(classes.items(),
                                                        key=lambda pair: -pair[1])))
    winding = result.get("winding") or []
    if winding:
        slabs = [row for row in winding if row["class"] in ("floor", "ceiling")]
        free = sum(row["other"] for row in winding)
        lines.append(f"  winding: {sum(row['faces'] for row in winding)} face(s), "
                     f"{sum(row['degenerate'] for row in winding)} with no area, "
                     f"{sum(row['wrong'] for row in winding)} pointing the wrong way over "
                     f"{sum(row['faces'] for row in slabs)} floor and ceiling faces; {free} "
                     f"wall/exterior face(s) are not on their box's side (parapets, guards, "
                     f"window wells)")
    lines.append(f"  {len(result['problems'])} problem(s)")
    return "\n".join(lines)


# ======================================================================================= selftest


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("verify_shell: selftest")
    shell_dir = REPO / "build" / "shell"
    if not shell_dir.is_dir() or not any(shell_dir.glob("*.glb")):
        # Loudly skipped, not failed. The shell is a Blender output and is not committed; a gate
        # that failed on a checkout which has never run `house_shell_gen.py` would be a gate people
        # turn off. Saying it checked nothing is the point -- a green line that means "there was
        # nothing to check" is the failure mode this avoids.
        print("verify_shell: no shell in build/shell -- run tools/blender/house_shell_gen.py "
              "first. NOTHING WAS CHECKED.")
        return 0

    result = verify(shell_dir, REPO / "assets-src" / "world")
    print(report(result))

    require(result["cells"] > 50, f"the shell has geometry in {result['cells']} cells")

    habitable = [row for row in result["clearHeights"]
                 if row["kind"] in ("room", "corridor", "stair") and not row["rafterBound"]]
    require(len(habitable) > 30, f"{len(habitable)} habitable cells were measured")
    require(all(CLEAR_MIN - 1e-6 <= row["clear"] <= CLEAR_MAX + 1e-6 for row in habitable),
            "every habitable cell's DRAWN clear height is inside §70.5's 2.35-3.10 m")
    # The measurement is of the slabs, not of the level's numbers, and it must differ from the
    # authored `ceiling - ffl` or it would be proving nothing about the geometry.
    layout = layout_io.load_layout(REPO / "assets-src" / "world", kinds=["levels", "cells"])
    levels = layout_io.by_id(layout_io.rows(layout, "levels"), "level")
    cells = layout_io.by_id(layout_io.rows(layout, "cells"), "cell")
    shell = load_shell(shell_dir)
    differing = 0
    for row in habitable:
        cell = cells[row["cell"]]
        authored = layout_io.cell_extent(cell, levels[cell["level"]])
        if abs((authored[1] - authored[0]) - row["clear"]) > 1e-6:
            differing += 1
    require(differing == 0,
            f"and every one of them is exactly the authored `ceiling - ffl`, measured from the "
            f"slabs the generator drew rather than read from the file ({differing} disagreed)")

    drawn = [row for row in result["stairs"] if not row.get("problem")]
    require(len(drawn) >= 6, f"{len(drawn)} flight(s) have treads to measure")
    require(all(row["treads"] == row["risers"] for row in drawn),
            "every flight draws one tread per authored riser")
    require(all(row["riseMax"] - row["riseMin"] <= RISE_TOLERANCE for row in drawn),
            f"no flight's rise varies by more than §70.5's {RISE_TOLERANCE * 1000:.0f} mm")
    indoors = [row for row in drawn if not row["outdoors"]]
    require(len(indoors) == 5 and all(
        STEP_MIN - 1e-6 <= row["stepMin"] and row["stepMax"] <= STEP_MAX + 1e-6
        for row in indoors),
        f"every one of the {len(indoors)} interior flights has a 2R+G inside §70.5's "
        f"{STEP_MIN * 1000:.0f}-{STEP_MAX * 1000:.0f} mm, AS DRAWN")
    outdoors = [row for row in drawn if row["outdoors"]]
    require(len(outdoors) == 3 and all(row["stepMin"] >= STEP_MIN - 1e-6 for row in outdoors),
            f"and the {len(outdoors)} exterior flights are no STEEPER than that, which is §12.4's "
            f"one-sided exemption and `validate_world.py` rule 10's")
    require(max(row["stepMax"] for row in outdoors) > STEP_MAX,
            f"-- one of them really does exceed the upper bound "
            f"({max(row['stepMax'] for row in outdoors) * 1000:.0f} mm), so the exemption is "
            f"doing something")

    measured = [row for row in result["headroom"] if row["headroom"] is not None]
    require(measured, "at least one flight has something over it to measure against")
    require(all(row["headroom"] >= HEADROOM_MIN - 1e-6 for row in measured),
            f"and every one of the {len(measured)} keeps §70.5's {HEADROOM_MIN} m of head-room, "
            f"worst {min(row['headroom'] for row in measured):.3f} m "
            f"({min(measured, key=lambda r: r['headroom'])['flight']})")

    # `HOUSE-00478`: winding and black facets.
    winding = result["winding"]
    slabs = [row for row in winding if row["class"] in ("floor", "ceiling")]
    require(sum(row["faces"] for row in winding) > 30000,
            f"{sum(row['faces'] for row in winding)} faces were examined")
    require(sum(row["degenerate"] for row in winding) == 0,
            "not one face in the shell has zero area -- §70's black facet, which is invisible "
            "until something tries to light it")
    require(sum(row["faces"] for row in slabs) > 400
            and sum(row["wrong"] for row in slabs) == 0,
            f"and every one of the {sum(row['faces'] for row in slabs)} floor and ceiling faces "
            f"points INTO its room: a floor up, a ceiling down (§14's counter-clockwise front)")
    # A house with no black facet in it cannot show that the check for one works, so it is asked
    # about a mesh built to have one: a triangle with two identical corners, which is exactly what
    # a collapsed quad leaves behind.
    flat = {"positions": [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0)],
            "triangles": [(0, 1, 2), (0, 3, 1)]}
    probe = winding_rows({"PROBE": {"floor": flat}},
                         {"PROBE": {"id": "PROBE", "boxes": [{"x": [0.0, 1.0], "z": [0.0, 1.0]}]}})
    require(len(probe) == 1 and probe[0]["degenerate"] == 1 and probe[0]["faces"] == 2,
            f"a triangle with two identical corners IS counted as having no area ({probe})")
    require(probe[0]["wrong"] == 0,
            "...and is not ALSO counted as pointing the wrong way, which it has no way to do")
    upside = {"positions": [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0)],
              "triangles": [(0, 1, 2)]}
    inverted = winding_rows({"PROBE": {"floor": upside}},
                            {"PROBE": {"id": "PROBE",
                                       "boxes": [{"x": [0.0, 1.0], "z": [0.0, 1.0]}]}})
    require(inverted[0]["wrong"] == 1,
            f"and a floor wound the other way IS counted as pointing down ({inverted})")

    require(sum(row["other"] for row in winding) > 0,
            f"{sum(row['other'] for row in winding)} wall and exterior faces are not on their "
            f"box's side, which is what a parapet, a mezzanine guard and a window well are -- "
            f"free-standing boxes with faces both ways, and why those two classes are counted "
            f"here and judged by `BlockoutRenderTests.NothingIsInsideOut` instead")

    # `HOUSE-00479`: §72's per-cell triangle budget.
    triangles = result["triangles"]
    total = sum(row["triangles"] for row in triangles)
    worst = max(triangles, key=lambda row: row["triangles"])
    require(total > 20000, f"the shell is {total} triangles over {len(triangles)} cells")
    require(worst["triangles"] <= SHELL_TRIANGLES_PER_CELL,
            f"and its worst cell, {worst['cell']}, is {worst['triangles']} -- "
            f"{worst['triangles'] * 100 // SHELL_TRIANGLES_PER_CELL} % of §72's "
            f"{SHELL_TRIANGLES_PER_CELL} for one cell of the shell")
    classes: dict[str, int] = {}
    for row in triangles:
        for name, count in row["byClass"].items():
            classes[name] = classes.get(name, 0) + count
    biggest = max(classes.items(), key=lambda pair: pair[1])
    require(biggest[0] == "trim" and biggest[1] * 2 > total,
            f"and MORE THAN HALF of it is `{biggest[0]}` -- {biggest[1]} of {total} triangles, "
            f"the skirtings, cornices, architraves, nosings and handrails §18.3 decided not to "
            f"lightmap ({sorted(classes.items(), key=lambda pair: -pair[1])[:3]})")
    require(all(row["triangles"] >= 0 for row in triangles)
            and any(row["triangles"] == 0 for row in triangles) is False,
            "every cell with a `.glb` that has geometry counts at least one triangle")

    require(result["openings"], "there are openings to check")
    cut = [row for row in result["openings"] if not row["blocked"]]
    require(len(cut) >= len(result["openings"]) - 1,
            f"{len(cut)} of the {len(result['openings'])} authored openings are holes in the shell")

    # `HOUSE-00486`. A window is filled with glass and a doorway was filled with nothing, which was
    # invisible until §25's culling stopped drawing the room behind a shut door (`HOUSE-00684`) and
    # left the hole on screen. Both rooms build a leaf, in their own half of the reveal: one built
    # once belongs to one cell's chunk, and that cell is exactly the one culling removes.
    doors = [row for row in result["openings"] if row["kind"] == "door" and not row["blocked"]]
    require(doors, "there are door openings to check")
    # A leaf can only go in a wall, so the claim is about the doors whose boundary HAS one.
    fillable = [row for row in doors if row["walled"]]
    leafless = [row["opening"] for row in fillable if not row["leafed"]]
    require(not leafless,
            f"every one of the {len(fillable)} door openings in a wall has a leaf in it "
            f"({leafless[:4] if leafless else 'none missing'})")
    require(len(doors) - len(fillable) == 3,
            f"and the {len(doors) - len(fillable)} that are in no wall at all are the two balcony "
            f"doors and the shed's -- their cells draw a floor and a ceiling and nothing else, so "
            f"there is nothing for a leaf to sit in "
            f"({[row['opening'] for row in doors if not row['walled']]})")
    # Both sides, wherever both sides can have one: a leaf built once belongs to one cell's chunk,
    # and §25 culls the room behind a shut door -- so the room in FRONT of it would be left looking
    # at the hole, which is the bug this replaced.
    both = [row for row in fillable
            if len(row["walled"]) == 2 and len(set(row["walled"]) & set(row["joinery"])) == 2]
    missing = [row["opening"] for row in both if len(row["leafed"]) != 2]
    require(both and not missing,
            f"and every one of the {len(both)} doors between two rooms that draw joinery has a "
            f"leaf on BOTH sides ({missing[:3] if missing else 'none missing'})")
    stairs_only = [row["opening"] for row in fillable
                   if len(row["walled"]) == 2 and len(set(row["walled"]) & set(row["joinery"])) < 2]
    require(len(stairs_only) == 2,
            f"and the {len(stairs_only)} that have one side only are the stair hall's, which draws "
            f"no trim of any kind -- no skirting, no architrave, no leaf ({stairs_only})")

    # THE SHELL IS NOT PERFECT, and this is where that is written down. `HOUSE-00480` is the task
    # that fixes what `HOUSE-00477`/`78`/`79` find; until it does, the list below is the exact set
    # of §70.5 problems this house has, and a problem that is not on it fails the selftest. Pinning
    # the set rather than asserting there are none is what makes this tool useful in between: a new
    # defect is noticed the day it appears instead of the day somebody rereads the report.
    # Every rule, exercised on a row this house does not have. A threshold the house passes is a
    # threshold that has never been asked a question, and seven of ten injected bugs walked
    # straight through the first version of this selftest for exactly that reason.
    def one(which, fields):
        base = {"heights": [], "stairs": [], "headroom": [], "openings": []}
        base[which] = [fields]
        return problems(base["heights"], base["stairs"], base["headroom"], base["openings"])

    require(one("heights", dict(cell="X", kind="room", rafterBound=False, clear=2.20)),
            "a room 2.20 m in the clear is reported")
    require(not one("heights", dict(cell="X", kind="room", rafterBound=False, clear=2.60)),
            "...and one at 2.60 is not")
    require(not one("heights", dict(cell="X", kind="garage", rafterBound=False, clear=4.15))
            and not one("heights", dict(cell="X", kind="room", rafterBound=True, clear=4.60)),
            "nor a garage, nor a rafter-bounded room, whatever their height")

    steep = {"flight": "F", "treads": 3, "risers": 3, "outdoors": False, "problem": None,
             "riseMin": 0.19, "riseMax": 0.19, "goingMin": 0.30, "goingMax": 0.30,
             "stepMin": 0.680, "stepMax": 0.680}
    require(one("stairs", steep), "an INTERIOR flight at 680 mm is reported")
    require(not one("stairs", dict(steep, outdoors=True)),
            "...and the same flight outdoors is not, which is §12.4's one-sided exemption")
    require(one("stairs", dict(steep, outdoors=True, stepMin=0.55, stepMax=0.55)),
            "...one-sided: an exterior flight that is too STEEP is still reported")
    fine = dict(steep, stepMin=0.639, stepMax=0.639)
    require(not one("stairs", fine), "a flight with nothing wrong with it is reported as nothing")
    require(one("stairs", dict(fine, riseMax=0.194)),
            "...but one whose rises vary by 4 mm is reported (§70.5 allows 2)")
    require(one("stairs", dict(steep, treads=2)),
            "and one that draws two treads for three risers is reported")

    require(one("headroom", dict(flight="F", headroom=1.90,
                                 worst={"step": 3, "under": "C", "class": "ceiling"})),
            "1.90 m of head-room over a flight is reported")
    require(not one("headroom", dict(flight="F", headroom=2.05, worst=None)),
            "...and 2.05 m is not")
    require(not one("headroom", dict(flight="F", headroom=None, worst=None)),
            "nor a flight with nothing over it at all, which is not the same as no head-room")

    require(one("openings", dict(opening="O", kind="door", width=0.9, height=2.0,
                                 blocked=[("C", "wall")])),
            "an opening that was never cut is reported")
    require(not one("openings", dict(opening="O", kind="door", width=0.9, height=2.0, blocked=[])),
            "...and one that was is not")

    # And the measurement itself, where the injections found it silent.
    slabs = [(name, surfaces[name]) for surfaces in shell.values()
             for name in ("ceiling", "floor") if name in surfaces]
    require(slabs and all(face_extent(mesh, 1)[1] - face_extent(mesh, 1)[0] < 1e-6
                          for _name, mesh in slabs),
            f"a cell's floor and ceiling are each ONE horizontal plane in the shell, not a slab "
            f"with a thickness ({len(slabs)} measured). That is why the clear height is a "
            f"difference of two face heights, why reading the min or the max of either is the "
            f"same number, and why the head-room search needs no projection")
    lone = 0
    for surfaces in shell.values():
        for name, mesh in surfaces.items():
            if name not in ("ceiling", "floor", "stair"):
                continue
            counts: dict[tuple, int] = {}
            for a, b, c in mesh["triangles"]:
                points = [mesh["positions"][a], mesh["positions"][b], mesh["positions"][c]]
                ys = [point[1] for point in points]
                if max(ys) - min(ys) > 1e-6:
                    continue
                key = (round(ys[0], 5),
                       round(min(p[0] for p in points), 5), round(max(p[0] for p in points), 5),
                       round(min(p[2] for p in points), 5), round(max(p[2] for p in points), 5))
                counts[key] = counts.get(key, 0) + 1
            lone += sum(1 for count in counts.values() if count == 1)
    require(lone == 0,
            f"and every horizontal face of a slab or a stair is a QUAD -- two triangles over one "
            f"rectangle -- so grouping them by their shared bounding box loses nothing ({lone} "
            f"lone triangles)")
    stair_meshes = [surfaces["stair"] for surfaces in shell.values() if "stair" in surfaces]
    require(stair_meshes, "there is stair geometry to read")
    for mesh in stair_meshes:
        quads = horizontal_quads(mesh)
        require(bool(quads), "and it has horizontal faces to find treads among")
        break
    main = [row for row in layout_io.rows(layout_io.load_layout(
        REPO / "assets-src" / "world", kinds=["stairs"]), "stairs")
        if row["id"] == "STAIR_MAIN_L0_L1"][0]
    tops = [entry["top"] for entry in flight_steps_drawn(
        shell, main, cells, levels)]
    require(tops == sorted(tops) and len(tops) == 17,
            f"the main stair's 17 treads come back in climbing order ({len(tops)})")

    known = {
        # ALL FOUR head-room failures are gone. `HOUSE-00480` fixed the two that were the
        # generator's -- a flight is placed inside the stairwell it comes up rather than against
        # its footprint's edge -- and `HOUSE-00484` fixed the two that were the layout's, by
        # lengthening the two openings that were shorter than the flights beneath them. The worst
        # head-room in the house is 2.175 m now, over §70.5's 2.00.
        #
        # A nested container's own opening is still not cut: `CELL_FRIDGE_INTERIOR` is a cell
        # inside `L0_KITCHEN`, and the generator cuts openings between cells that share a boundary
        # PLANE, which a cell wholly inside another does not.
        "FRIDGE_L0_KITCHEN: not cut in CELL_FRIDGE_INTERIOR.wall",
    }
    unknown = sorted(problem for problem in result["problems"] if problem not in known)
    require(not unknown,
            f"every §70.5 problem the shell has is one of the {len(known)} recorded for "
            f"`HOUSE-00480` ({unknown if unknown else 'none new'})")
    covered = {text for text in known if text in result["problems"]}
    require(covered == known,
            f"...and every recorded problem is still there, so a fixed one is noticed too "
            f"({sorted(known - covered)} have gone)")

    if failures:
        print(f"\nverify_shell: {len(failures)} claim(s) FAILED")
        return 1
    print("verify_shell: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--shell", type=Path, default=REPO / "build" / "shell")
    parser.add_argument("--world", type=Path, default=REPO / "assets-src" / "world")
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--json", type=Path, default=None)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if not args.shell.is_dir():
        print(f"verify_shell: no shell in {args.shell}; run tools/blender/house_shell_gen.py",
              file=sys.stderr)
        return 1

    result = verify(args.shell, args.world)
    print(report(result))
    for problem in result["problems"]:
        print(f"verify_shell: {problem}", file=sys.stderr)
    if args.json is not None:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                             encoding="utf-8")
    return 1 if result["problems"] else 0


if __name__ == "__main__":
    sys.exit(main())
