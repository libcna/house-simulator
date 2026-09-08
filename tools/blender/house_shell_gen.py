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

This is the **massing**: one closed box per footprint box, from the cell's floor to its ceiling,
one `.glb` per cell. It is real geometry -- you can open the house in Blender or the game and see
the rooms in their right places at their right heights -- and it is what the next four tasks carve
into a building: `HOUSE-00452` replaces the top and bottom faces with floor and ceiling slabs inset
to the partition centre lines, `HOUSE-00453` the shared side faces with deduplicated partitions,
`HOUSE-00454` the outer ones with real exterior walls, and `HOUSE-00455` cuts the openings. Each of
those *supersedes* part of this mesh rather than adding beside it; when the last one lands, no
massing face is left.

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
FACES = ((0, 3, 2, 1),   # floor, normal down
         (4, 5, 6, 7),   # ceiling, normal up
         (0, 1, 5, 4),   # -Z
         (1, 2, 6, 5),   # +X
         (2, 3, 7, 6),   # +Z
         (3, 0, 4, 7))   # -X


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


def build_cell(cell: dict, extent: tuple[float, float]):
    """One mesh object named for the cell, holding a closed box per footprint box."""
    vertices = []
    faces = []
    for x0, x1, y0, y1, z0, z1 in cell_boxes(cell, extent):
        base = len(vertices)
        for cx, cy, cz in CORNERS:
            vertices.append(to_blender(x1 if cx else x0, y1 if cy else y0, z1 if cz else z0))
        faces.extend([tuple(base + index for index in face) for face in FACES])

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
    report = {"written": [], "skipped": [], "problems": []}

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
        obj = build_cell(cell, extent)
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
