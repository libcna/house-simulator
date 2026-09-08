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
#: The four sides, wound so their normals point OUT of the room, in world −Z, +X, +Z, −X order.
#: `to_blender` negates z, which reverses a plan winding -- `HOUSE-00451` wound all four the
#: obvious way and every one of them faced inward, invisible from the room it bounds, until
#: `HOUSE-00452` measured the polygon normals instead of reasoning about the vertex order.
SIDES = ((4, 5, 1, 0),   # world −Z
         (5, 6, 2, 1),   # world +X
         (6, 7, 3, 2),   # world +Z
         (7, 4, 0, 3))   # world −X


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


def wall_across(side: str, box: tuple, cell: dict, neighbours: list[tuple[dict, tuple]]) -> str:
    """Which wall bounds @p box on @p side: `wallExterior`, `wallPartition` or `wallGarage`.

    A side is a partition when an **interior** cell's box abuts it on the same plane and overlaps
    it. An exterior cell counts as nothing: the back wall of the family room faces `EXT_BACKYARD`,
    which is a cell, and it is still an exterior wall.
    """
    x0, x1, y0, y1, z0, z1 = box
    axis, value = {"-X": (0, x0), "+X": (0, x1), "-Z": (2, z0), "+Z": (2, z1)}[side]
    for other, obox in neighbours:
        if other["id"] == cell["id"]:
            continue
        ox0, ox1, oy0, oy1, oz0, oz1 = obox
        if oy1 <= y0 + 1e-6 or oy0 >= y1 - 1e-6:
            continue                      # a different storey; not across this wall
        if axis == 0:
            if abs((ox1 if side == "-X" else ox0) - value) > 1e-6:
                continue
            if min(z1, oz1) - max(z0, oz0) <= 1e-6:
                continue
        else:
            if abs((oz1 if side == "-Z" else oz0) - value) > 1e-6:
                continue
            if min(x1, ox1) - max(x0, ox0) <= 1e-6:
                continue
        if "garage" in (cell.get("kind"), other.get("kind")):
            return "wallGarage"
        return "wallPartition"
    return "wallGarage" if cell.get("kind") == "garage" else "wallExterior"


def inset_box(box: tuple, cell: dict, neighbours: list, construction: dict) -> tuple:
    """@p box's floor rectangle: each side moved in by half the wall that bounds it."""
    x0, x1, y0, y1, z0, z1 = box
    half = {side: float(construction.get(wall, 0.0)) / 2.0
            for side, wall in ((side, wall_across(side, box, cell, neighbours))
                               for side in ("-X", "+X", "-Z", "+Z"))}
    return (x0 + half["-X"], x1 - half["+X"], y0, y1, z0 + half["-Z"], z1 - half["+Z"])


def build_cell(cell: dict, extent: tuple[float, float], *, neighbours=(), construction=None):
    """One mesh object named for the cell: inset floors and ceilings, plus the massing sides."""
    construction = construction or {}
    vertices = []
    faces = []
    for box in cell_boxes(cell, extent):
        x0, x1, y0, y1, z0, z1 = box
        base = len(vertices)
        for cx, cy, cz in CORNERS:
            vertices.append(to_blender(x1 if cx else x0, y1 if cy else y0, z1 if cz else z0))
        faces.extend([tuple(base + index for index in face) for face in SIDES])

        # `HOUSE-00452`: the floor and the ceiling, inset, replacing the massing's flat top and
        # bottom. The sides stay on the centre line until `HOUSE-00453`/`HOUSE-00454` move them.
        ix0, ix1, _, _, iz0, iz1 = inset_box(box, cell, list(neighbours), construction)
        slab = len(vertices)
        for cy in (0, 1):
            for cx, cz in ((0, 0), (1, 0), (1, 1), (0, 1)):
                vertices.append(to_blender(ix1 if cx else ix0, y1 if cy else y0,
                                           iz1 if cz else iz0))
        # Wound the other way round than they read: the corners walk (x, z) anticlockwise, and
        # `to_blender` negates z, so the plan's anticlockwise is Blender's clockwise. The claim
        # that caught this measures the polygon normal rather than reasoning about the order.
        faces.append((slab + 3, slab + 2, slab + 1, slab + 0))          # floor, normal up
        faces.append((slab + 4, slab + 5, slab + 6, slab + 7))          # ceiling, normal down

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
            want_min = [box[0], box[2], box[4]]
            want_max = [box[1], box[3], box[5]]
            close = all(abs(a - b) < 1e-4 for a, b in zip(bounds[0], want_min)) and \
                all(abs(a - b) < 1e-4 for a, b in zip(bounds[1], want_max))
            require(close,
                    f"and the exported box is the layout's box, in world coordinates: "
                    f"{[round(v, 2) for v in bounds[0]]}..{[round(v, 2) for v in bounds[1]]} "
                    f"against {want_min}..{want_max}")
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
    walls = {side: wall_across(side, kitchen_box, subject, neighbours)
             for side in ("-X", "+X", "-Z", "+Z")}
    require(set(walls.values()) == {"wallPartition"},
            f"the kitchen is surrounded by rooms, so all four of its walls are partitions "
            f"({walls})")
    family = cells["L0_FAMILY"]
    family_box = list(cell_boxes(family, extent_of(family, levels[family["level"]])[0]))[0]
    require(wall_across("+X", family_box, family, neighbours) == "wallExterior",
            "the family room's east side has nothing across it, so it is an exterior wall")
    require(wall_across("-Z", family_box, family, neighbours) == "wallPartition",
            "and its north side has the sunroom, so it is a partition")
    # The case the exterior rule exists for: `L0_PORCH` is a CELL, it abuts the foyer's front
    # face, and the wall between them is the front of the house.
    foyer = cells["L0_FOYER"]
    foyer_box = list(cell_boxes(foyer, extent_of(foyer, levels[foyer["level"]])[0]))[0]
    require(wall_across("+Z", foyer_box, foyer, neighbours) == "wallExterior",
            "the porch abuts the foyer's front and is still outside, so their shared wall is "
            "exterior")
    lawn = wall_across("+Z", list(cell_boxes(cells["B1_CELLAR"],
                                             extent_of(cells["B1_CELLAR"],
                                                       levels["B1"])[0]))[0],
                       cells["B1_CELLAR"], neighbours)
    require(lawn == "wallPartition", f"a basement cell against another is a partition ({lawn})")
    garage = cells["L0_GARAGE"]
    garage_box = list(cell_boxes(garage, extent_of(garage, levels[garage["level"]])[0]))[0]
    require({wall_across(side, garage_box, garage, neighbours)
             for side in ("-X", "+X", "-Z", "+Z")} == {"wallGarage"},
            "and every wall of the garage is a garage wall, from either side")

    inset = inset_box(kitchen_box, subject, neighbours, construction)
    half = float(construction["wallPartition"]) / 2.0
    require(abs(inset[0] - (kitchen_box[0] + half)) < 1e-9
            and abs(inset[1] - (kitchen_box[1] - half)) < 1e-9
            and abs(inset[4] - (kitchen_box[4] + half)) < 1e-9
            and abs(inset[5] - (kitchen_box[5] - half)) < 1e-9,
            f"so its floor stops {half:.3f} m short on every side "
            f"({inset[0]:.3f}…{inset[1]:.3f} by {inset[4]:.3f}…{inset[5]:.3f})")
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
    polygons = obj.data.polygons
    require(len(polygons) == 6, f"six faces to a box ({len(polygons)})")
    require(polygons[4].normal.z > 0.99, f"the floor faces up ({polygons[4].normal.z:.2f})")
    require(polygons[5].normal.z < -0.99, f"the ceiling faces down ({polygons[5].normal.z:.2f})")
    # ...and so do the four sides, outward. `to_blender` negates z, so a world −Z face points to
    # Blender +Y: the mapping is worth asserting once rather than reasoning about at each face.
    sides = {"world −Z": polygons[0].normal.y > 0.99,
             "world +X": polygons[1].normal.x > 0.99,
             "world +Z": polygons[2].normal.y < -0.99,
             "world −X": polygons[3].normal.x < -0.99}
    require(all(sides.values()),
            f"and every side faces out of the room, not into it "
            f"({[name for name, ok in sides.items() if not ok]})")

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
