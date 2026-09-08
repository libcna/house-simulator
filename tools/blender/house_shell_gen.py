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


def build_cell(cell: dict, extent: tuple[float, float], *, neighbours=(), construction=None):
    """One mesh object named for the cell: its floor, its ceiling and its walls' inner faces."""
    construction = construction or {}
    neighbours = list(neighbours)
    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[int, ...]] = []

    def add(points, outward) -> None:
        base = len(vertices)
        vertices.extend(to_blender(*point) for point in facing(points, outward))
        faces.append(tuple(range(base, base + len(points))))

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
            for lo, hi, wall in side_intervals(side, box, cell, neighbours):
                half = float(construction.get(wall, 0.0)) / 2.0
                plane = {"-X": x0 + half, "+X": x1 - half,
                         "-Z": z0 + half, "+Z": z1 - half}[side]
                clamp = (iz0, iz1) if side in ("-X", "+X") else (ix0, ix1)
                lo, hi = max(lo, clamp[0]), min(hi, clamp[1])
                if hi - lo <= 1e-6:
                    continue
                if side in ("-X", "+X"):
                    corners = [(plane, y0, lo), (plane, y1, lo), (plane, y1, hi), (plane, y0, hi)]
                else:
                    corners = [(lo, y0, plane), (lo, y1, plane), (hi, y1, plane), (hi, y0, plane)]
                add(corners, inward)

        # `HOUSE-00452`: the floor and the ceiling, inset to the same inner faces.
        floor = [(ix0, y0, iz0), (ix1, y0, iz0), (ix1, y0, iz1), (ix0, y0, iz1)]
        add(floor, (0.0, 1.0, 0.0))
        add([(x, y1, z) for x, _y, z in floor], (0.0, -1.0, 0.0))

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
    layout = layout_io.load_layout(directory, kinds=["levels", "cells"])
    levels = {row["id"]: row for row in layout_io.rows(layout, "levels")}
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
        obj = build_cell(cell, extent, neighbours=neighbours, construction=construction)
        destination = output / f"{cell['id']}.glb"
        export(obj, destination)
        report["written"].append(cell["id"])
    return report


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
            want_min = [inner[0], inner[2], inner[4]]
            want_max = [inner[1], inner[3], inner[5]]
            close = all(abs(a - b) < 1e-4 for a, b in zip(bounds[0], want_min)) and \
                all(abs(a - b) < 1e-4 for a, b in zip(bounds[1], want_max))
            require(close,
                    f"and the exported room is the layout's box with its walls taken off, in world "
                    f"coordinates: {[round(v, 3) for v in bounds[0]]}.."
                    f"{[round(v, 3) for v in bounds[1]]} against "
                    f"{[round(v, 3) for v in want_min]}..{[round(v, 3) for v in want_max]}")
            require(bounds[0][0] > box[0] and bounds[1][0] < box[1]
                    and bounds[0][2] > box[4] and bounds[1][2] < box[5],
                    "which is strictly inside the centre-line box on all four sides")
            require(abs(bounds[1][1] - bounds[0][1] - (extent[1] - extent[0])) < 1e-4,
                    f"and it stands from the floor to the ceiling, "
                    f"{extent[1] - extent[0]:.2f} m of it")

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
    reset_scene()
    obj = build_cell(subject, extent, neighbours=neighbours, construction=construction)
    polygons = list(obj.data.polygons)
    runs = sum(len(side_intervals(side, kitchen_box, subject, neighbours))
               for side in ("-X", "+X", "-Z", "+Z"))
    require(len(polygons) == runs + 2,
            f"one face per run of each side, plus a floor and a ceiling ({len(polygons)} for "
            f"{runs} runs)")
    require(sum(1 for face in polygons if face.normal.z > 0.99) == 1
            and sum(1 for face in polygons if face.normal.z < -0.99) == 1,
            "exactly one face looks up and one looks down: the floor and the ceiling")

    # Every face looks INTO the room. These are the inner faces of the walls, seen from the only
    # place anyone stands; the outside of the house is `HOUSE-00454`'s. A face that points the
    # other way is invisible from the room it bounds, and no claim about coordinates can see it.
    centre = ((kitchen_box[0] + kitchen_box[1]) / 2.0,
              (kitchen_box[2] + kitchen_box[3]) / 2.0,
              (kitchen_box[4] + kitchen_box[5]) / 2.0)
    blender_centre = to_blender(*centre)
    inward = [face for face in polygons
              if sum(n * (c - p) for n, c, p in zip(face.normal, blender_centre, face.center)) > 0]
    require(len(inward) == len(polygons),
            f"every one of the {len(polygons)} faces looks into the room "
            f"({len(polygons) - len(inward)} do not)")

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
    require(len(everything["written"]) == len(cells) and not everything["problems"]
            and not everything["skipped"],
            f"every one of the {len(cells)} cells generates "
            f"({len(everything['written'])} written, {len(everything['skipped'])} skipped, "
            f"{everything['problems'][:1]})")
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
