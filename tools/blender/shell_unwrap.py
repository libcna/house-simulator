#!/usr/bin/env python3
"""shell_unwrap.py -- the lightmap UVs for the shell's RECEIVER surfaces, at §18.3's densities.

`HOUSE-00471`. `lightmap_unwrap.py` (`HOUSE-00205`) packs one object's faces into a
density-uniform atlas and checks the six things that ruin a bake. This drives it over the
generated shell, picks the density §18.3 asks for per cell, and -- the part that took a decision --
gives it only the surfaces that are **lightmap receivers**.

    tools/blender/shell_unwrap.py                     # build/shell -> build/shell-lm
    tools/blender/shell_unwrap.py --cells L0_KITCHEN
    tools/blender/shell_unwrap.py --selftest

## Which surfaces, and why it is not a size test

§18.3 used to say "every shell face" gets a second UV channel. It cannot: 26 704 of the shell's
43 528 faces are under one texel at 4 texels/metre, and a 55 mm handrail face is a fifth of a texel
across. The decision (2026-09-09) is **selective semantic receivers**: floors, ceilings, walls and
the outer skin are lightmapped; skirtings, cornices, architraves, thresholds, frames, sashes,
glass, nosings, handrails, balusters, rafters, gutters and downspouts are lit by the room's
dynamic term.

**Semantic, not geometric.** The rule is the surface's CLASS, which `house_shell_gen.py` writes
into each material as `lightmapReceiver`, and never the size of an individual triangle: a wall must
not stop receiving baked light because the generator split it differently. The same generator welds
its receiver faces, so a wall broken into strips round a doorway is one connected surface and the
unwrapper packs one island for it rather than three.

## What "small room" means, since §18.3 does not say

A room under **6 m²** of floor: the WCs, the closets and the linen cupboards. They are the rooms
where a lightmap at 4 texels/m has a dozen texels across the whole wall, and they are cheap to
double because they are small -- 8 texels/m over 4 m² is the same atlas area as 4 over 16.

The attic and the basement take 2 because §18.3 says so and because both are lit by one bulb.
A roof and a chimney are not cells at all; they take the exterior density, 2, for the same reason:
the sky lights them evenly and there is nothing on them to read.

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

    sys.exit(blender_env.relaunch(Path(__file__).resolve(), sys.argv[1:], tool="shell_unwrap"))

import argparse  # noqa: E402
import json  # noqa: E402
import time  # noqa: E402
from pathlib import Path  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import house_shell_gen  # noqa: E402
import lightmap_unwrap  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "world"))
import layout_io  # noqa: E402

SOURCE = REPO / "assets-src" / "world"
SHELL = REPO / "build" / "shell"
OUTPUT = REPO / "build" / "shell-lm"

#: §18.3's three densities, in texels per metre.
DENSITY_ROOM = 4.0
DENSITY_SMALL = 8.0
DENSITY_DIM = 2.0

#: What counts as a small room, in square metres. §18.3 asks for the density and not the threshold.
SMALL_ROOM_AREA = 6.0

#: The levels §18.3 calls "the attic and the basement".
DIM_LEVELS = ("B1", "L3")

#: The largest atlas one cell may use, in texels a side.
#:
#: §72 budgets **21 art atlases and 21 daylight atlases of 2048²** for the whole house — about a
#: fifth of a 2048² atlas per cell. 512² is a sixteenth of one, so 96 cells come to roughly six
#: 2048² atlases' worth of texels and sit inside that budget with room for the daylight set.
#:
#: It is also what makes a whole-shell run finish: `lightmap_unwrap` verifies its work by
#: rasterising the atlas and measuring the gutter between islands, which is `size²` work, and
#: 2048² is sixteen times 512². A cell that cannot reach §18.3's density inside 512² is reported
#: with its shortfall rather than given a bigger atlas quietly.
MAX_ATLAS = 512


def wants_lightmap(cell: dict | None) -> bool:
    """Is this object lightmapped at all?

    §18.3 bakes "per cell, one lightmap per light group plus one daylight lightmap lit only by a
    uniform sky dome through that cell's window openings" -- which is a description of an INTERIOR.
    Outside there is no cell to bake for: the sun and the sky light it directly every frame (§22),
    and `EXT_WORLD` is 160 000 m², which at any useful density is an atlas nobody can allocate.
    So the yards, the decks, the roofs and the chimney are skipped, and the reason is recorded
    rather than discovered again the next time the batch dies.
    """
    return cell is not None and cell.get("kind") != "exterior"


def density_for(cell: dict | None) -> float:
    """The texel density §18.3 gives this cell, or the exterior's when it is not a cell at all."""
    if cell is None:
        return DENSITY_DIM
    if cell.get("level") in DIM_LEVELS:
        return DENSITY_DIM
    area = sum((float(box["x"][1]) - float(box["x"][0]))
               * (float(box["z"][1]) - float(box["z"][0]))
               for box in cell.get("boxes") or [])
    return DENSITY_SMALL if 0.0 < area < SMALL_ROOM_AREA else DENSITY_ROOM


def receiver_material(material) -> bool:
    """Does @p material say it is a lightmap receiver?

    The generator's own decision, read back: `house_shell_gen.material_slots` sets
    `lightmapReceiver` on every material it makes and the exporter writes it into the `.glb`'s
    material `extras`. Falling back to the class NAME rather than to a guess keeps a hand-made
    fixture working; falling back to "yes" would silently lightmap the trim again.
    """
    if material is None:
        return False
    flag = material.get("lightmapReceiver")
    if flag is not None:
        return bool(flag)
    name = str(material.get("surfaceClass") or material.name).replace("BLOCKOUT_", "")
    return name in house_shell_gen.LIGHTMAP_RECEIVERS


def split_receivers(obj):
    """Split @p obj into `(receiver, detail)` objects by material. Either may be None.

    Two meshes rather than a face selection, because `lightmap_unwrap.unwrap` works on every mesh
    in the scene and must not see the detail at all: one sub-texel island is enough to make the
    pack fail, and the failure would be about a handrail rather than about the wall it is on.
    """
    mesh = obj.data
    receivers = {index for index, material in enumerate(mesh.materials)
                 if receiver_material(material)}
    groups = {"receiver": [], "detail": []}
    for polygon in mesh.polygons:
        key = "receiver" if polygon.material_index in receivers else "detail"
        groups[key].append(polygon.index)

    import bmesh  # noqa: PLC0415

    def carved(keep: list[int], suffix: str):
        """A copy of the mesh with only @p keep's faces, as a DATABLOCK, not an object."""
        copy = mesh.copy()
        copy.name = f"{mesh.name}{suffix}"
        working = bmesh.new()
        working.from_mesh(copy)
        working.faces.ensure_lookup_table()
        wanted = set(keep)
        drop = [face for face in working.faces if face.index not in wanted]
        bmesh.ops.delete(working, geom=drop, context="FACES")
        working.to_mesh(copy)
        working.free()
        copy.update()
        return copy

    receiver_mesh = carved(groups["receiver"], "") if groups["receiver"] else None
    detail_mesh = carved(groups["detail"], "_DETAIL") if groups["detail"] else None
    name = obj.name
    bpy.data.objects.remove(obj, do_unlink=True)

    receiver = None
    if receiver_mesh is not None:
        receiver = bpy.data.objects.new(name, receiver_mesh)
        bpy.context.scene.collection.objects.link(receiver)
    # The detail stays a datablock until the unwrap is over: `lightmap_unwrap.unwrap` works on
    # every mesh in the scene, and one sub-texel handrail island is enough to fail the pack --
    # with a message about the handrail rather than about the wall it is on.
    return receiver, detail_mesh


def summarise(rows: list[dict], skipped: list[str], problems: list[str]) -> dict:
    """The report `HOUSE-00471` asks for: what was lightmapped, what was not, and how well."""
    if not rows:
        return {"cells": 0, "receiverFaces": 0, "receiverArea": 0.0, "detailFaces": 0,
                "detailArea": 0.0, "atlases": 0, "worstOccupancy": 0.0,
                "worstOccupancyCell": "", "worstDensity": 0.0, "smallestIslandGutter": None,
                "cellsWithRoomToSpare": [], "islands": 0, "texels": 0,
                "skipped": skipped, "problems": problems, "perCell": []}
    worst_row = min(rows, key=lambda row: row["utilisation"])
    return {
        "cells": len(rows),
        "receiverFaces": sum(row["faces"] for row in rows),
        "receiverArea": round(sum(row["worldArea"] for row in rows), 1),
        "detailFaces": sum(row["detailFaces"] for row in rows),
        "detailArea": round(sum(row["detailArea"] for row in rows), 1),
        "atlases": len(rows),
        "atlasSizes": sorted({row["atlasSize"] for row in rows}),
        "worstOccupancy": round(worst_row["utilisation"], 4),
        "worstOccupancyCell": worst_row["cell"],
        "worstDensity": round(min(row["minDensity"] for row in rows), 3),
        # The tightest gap between two islands anybody has to bake across, in empty texels. `None`
        # when no two islands in any cell came within the measured reach of each other at all --
        # which is the good answer, and reporting it as "0 texels" would read as a gutter violation
        # that is not there.
        "smallestIslandGutter": (min(gaps) if (gaps := [row["minIslandGutter"] for row in rows
                                                       if row["minIslandGutter"] is not None])
                                 else None),
        "cellsWithRoomToSpare": sorted(row["cell"] for row in rows
                                       if row["minIslandGutter"] is None),
        "islands": sum(row["islands"] for row in rows),
        "texels": sum(row["atlasSize"] ** 2 for row in rows),
        # A cell worth a second look: almost nothing packed into its atlas, or a density short of
        # §18.3's target by more than the unwrapper's own tolerance.
        "needsAttention": sorted(
            row["cell"] for row in rows
            if row["utilisation"] < 0.02
            or row["minDensity"] < row["density"] * (1.0 - lightmap_unwrap.DENSITY_TOLERANCE)),
        "skipped": skipped,
        "problems": problems,
        "perCell": [{"cell": row["cell"], "density": row["density"],
                     "atlas": row["atlasSize"], "faces": row["faces"],
                     "islands": row["islands"], "detailFaces": row["detailFaces"],
                     "occupancy": row["utilisation"], "minDensity": row["minDensity"],
                     "area": row["worldArea"]} for row in rows],
    }


def unwrap_one(source: Path, destination: Path, density: float,
               max_atlas: int = MAX_ATLAS) -> dict:
    """Unwrap one cell's receivers and write both halves back out.

    `CNAHOUSE_UNWRAP_TIMING=1` prints where the seconds went. A whole-shell run is minutes a cell
    and the phases are not equally to blame; guessing which one is how an afternoon goes.
    """
    timing = bool(os.environ.get("CNAHOUSE_UNWRAP_TIMING"))
    marks: list[tuple[str, float]] = []

    def mark(name: str) -> None:
        if timing:
            marks.append((name, time.perf_counter()))

    mark("start")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(source))
    mark("import")
    originals = [obj for obj in list(bpy.context.scene.objects) if obj.type == "MESH"]
    receivers, details = [], []
    for obj in originals:
        receiver, detail_mesh = split_receivers(obj)
        if receiver is not None:
            receivers.append(receiver)
        if detail_mesh is not None:
            details.append(detail_mesh)

    mark("split")
    detail_faces = sum(len(mesh.polygons) for mesh in details)
    detail_area = sum(sum(face.area for face in mesh.polygons) for mesh in details)
    if not receivers:
        return {"skipped": "no receiver surface", "detailFaces": detail_faces,
                "detailArea": round(detail_area, 3), "faces": 0, "problems": []}

    result = lightmap_unwrap.unwrap(density, lightmap_unwrap.DEFAULT_GUTTER, max_atlas)
    mark("unwrap")
    for index, mesh in enumerate(details):
        obj = bpy.data.objects.new(mesh.name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        del index

    result["detailFaces"] = detail_faces
    result["detailArea"] = round(detail_area, 3)
    destination.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.gltf(filepath=str(destination), export_format="GLB",
                              export_apply=False, export_yup=True, export_extras=True)
    mark("export")
    if timing and len(marks) > 1:
        print("shell_unwrap: " + source.stem + " " + ", ".join(
            f"{name} {marks[index][1] - marks[index - 1][1]:.1f}s"
            for index, (name, _at) in enumerate(marks) if index > 0))
    return result


# ============================================================================== selftest =========


def selftest() -> int:
    failures = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("shell_unwrap: selftest")

    layout = layout_io.load_layout(SOURCE, kinds=["cells"])
    cells = {row["id"]: row for row in layout_io.rows(layout, "cells")}
    require(density_for(cells["L0_KITCHEN"]) == DENSITY_ROOM,
            f"a room takes §18.3's 4 texels/m ({density_for(cells['L0_KITCHEN'])})")
    require(density_for(cells["L0_WC1"]) == DENSITY_SMALL,
            f"a 4.9 m² powder room is a small room and takes 8 "
            f"({density_for(cells['L0_WC1'])})")
    require(density_for(cells["B1_CINEMA"]) == DENSITY_DIM
            and density_for(cells["L3_ROOM"]) == DENSITY_DIM,
            "the basement and the attic take 2, whatever size they are")
    require(density_for(cells["B1_WC7"]) == DENSITY_DIM,
            f"and a small room in the basement takes the BASEMENT's 2: §18.3 names the levels "
            f"first ({density_for(cells['B1_WC7'])})")
    require(density_for(None) == DENSITY_DIM,
            "a roof is not a cell and takes the exterior's 2")
    require(wants_lightmap(cells["L0_KITCHEN"]) and not wants_lightmap(cells["EXT_WORLD"])
            and not wants_lightmap(cells["L0_PORCH"]) and not wants_lightmap(None),
            "a room is lightmapped; the world, a deck and a roof are not — outside is lit by the "
            "sky every frame and `EXT_WORLD` is 160 000 m² of it")

    require(set(house_shell_gen.LIGHTMAP_RECEIVERS)
            == {"floor", "ceiling", "wall", "exterior"},
            f"the receiver classes are the generator's, not a second list here "
            f"({house_shell_gen.LIGHTMAP_RECEIVERS})")

    if not (SHELL / "L0_HALL.glb").is_file():
        require(False, "the shell is generated; run tools/blender/house_shell_gen.py first")
    else:
        # `L0_HALL` is the cell that would not pack at all before this decision: 224 of its faces
        # collapsed to no UV area at 2048², and the same 224 collapsed at a gutter of 1.
        out = OUTPUT / "selftest" / "L0_HALL.glb"
        result = unwrap_one(SHELL / "L0_HALL.glb", out, DENSITY_ROOM)

        # (6) the pack succeeds, and (5)(7)(8) are `lightmap_unwrap`'s own checks: no island with
        # zero area, no UV outside [0, 1], the gutter measured between islands.
        require(not result["problems"],
                f"the hall packs, with no zero-area island, nothing outside the atlas and the "
                f"gutter kept ({result['problems'][:2]})")
        require(result["minDensity"] >= DENSITY_ROOM - 0.01,
                f"at no less than the density §18.3 asks for "
                f"({result['minDensity']:.2f} of {DENSITY_ROOM})")

        # (1) and (2): the receivers were unwrapped and the detail was not there to be.
        receivers = [obj for obj in bpy.context.scene.objects
                     if obj.type == "MESH" and not obj.name.endswith("_DETAIL")]
        details = [obj for obj in bpy.context.scene.objects if obj.name.endswith("_DETAIL")]
        require(receivers and details,
                f"the cell comes out in two halves, receiver and detail "
                f"({len(receivers)}, {len(details)})")
        # The materials the polygons USE, not the slots the mesh carries: copying a mesh copies
        # its whole material list, so the receiver half still has a slot for the trim it no
        # longer has a face of.
        receiver_classes = set()
        for obj in receivers:
            for index in {polygon.material_index for polygon in obj.data.polygons}:
                material = obj.data.materials[index]
                if material is not None:
                    receiver_classes.add(str(material.get("surfaceClass")))
        require(receiver_classes and receiver_classes
                <= set(house_shell_gen.LIGHTMAP_RECEIVERS),
                f"every class actually used on the receiver half is a receiver class "
                f"({sorted(receiver_classes)})")
        detail_classes = set()
        for obj in details:
            used = {polygon.material_index for polygon in obj.data.polygons}
            for index in used:
                material = obj.data.materials[index]
                if material is not None:
                    detail_classes.add(str(material.get("surfaceClass")))
        require(detail_classes and not (detail_classes
                                        & set(house_shell_gen.LIGHTMAP_RECEIVERS)),
                f"and every class actually used on the detail half is not "
                f"({sorted(detail_classes)})")
        require(all(lightmap_unwrap.LIGHTMAP_UV in obj.data.uv_layers for obj in receivers),
                "the receivers have the lightmap channel")
        require(not any(lightmap_unwrap.LIGHTMAP_UV in obj.data.uv_layers for obj in details),
                "and the detail does not: it is lit by the room's dynamic term (§22.2)")

        # (4): a wall broken into strips round a doorway is ONE island, not one per strip. The
        # hall has three doorways; if triangulation decided coverage, its islands would outnumber
        # its faces' logical surfaces several times over.
        require(result["islands"] < result["faces"] / 2,
                f"the hall's {result['faces']} receiver faces pack into {result['islands']} "
                f"islands, so the welded strips are carried by their wall")

        # (10) and (3): the statistics exist, and the exclusions are counted rather than dropped.
        require(result["detailFaces"] > 0 and result["detailArea"] > 0.0,
                f"the detail is reported, not silently discarded "
                f"({result['detailFaces']} faces, {result['detailArea']} m²)")
        summary = summarise([{"cell": "L0_HALL", "density": DENSITY_ROOM, **result}], [], [])
        for key in ("receiverFaces", "receiverArea", "detailFaces", "detailArea", "atlases",
                    "atlasSizes", "islands", "texels", "worstOccupancy", "worstOccupancyCell",
                    "worstDensity", "smallestIslandGutter", "needsAttention", "skipped",
                    "perCell"):
            require(key in summary, f"the report carries `{key}`")

        # (9): a rerun writes the same bytes.
        first = out.read_bytes()
        unwrap_one(SHELL / "L0_HALL.glb", out, DENSITY_ROOM)
        require(out.read_bytes() == first,
                "and a second run over an unchanged shell writes the same bytes")

    if failures:
        print(f"shell_unwrap: {len(failures)} claim(s) FAILED")
        return 1
    print("shell_unwrap: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--shell", type=Path, default=SHELL)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--cells", default="")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])

    if args.selftest:
        return selftest()
    if not args.shell.is_dir():
        print(f"shell_unwrap: no shell in {args.shell}; run house_shell_gen.py first.")
        return 0

    layout = layout_io.load_layout(SOURCE, kinds=["cells"])
    cells = {row["id"]: row for row in layout_io.rows(layout, "cells")}
    wanted = {name.strip() for name in args.cells.split(",") if name.strip()} or None

    problems: list[str] = []
    rows: list[dict] = []
    skipped: list[str] = []
    for path in sorted(args.shell.glob("*.glb")):
        if wanted is not None and path.stem not in wanted:
            continue
        cell = cells.get(path.stem)
        if not wants_lightmap(cell):
            skipped.append(path.stem)
            continue
        try:
            result = unwrap_one(path, args.output / path.name, density_for(cell))
        except RuntimeError as error:
            # Reported and carried on, not raised: one cell that will not pack must not hide the
            # state of the other ninety-five.
            problems.append(f"{path.stem}: {error}")
            continue
        if result.get("skipped"):
            skipped.append(f"{path.stem} ({result['skipped']})")
            continue
        rows.append({"cell": path.stem, "density": density_for(cell), **result})
        for problem in result["problems"]:
            problems.append(f"{path.stem}: {problem}")

    report = summarise(rows, skipped, problems)
    (args.output).mkdir(parents=True, exist_ok=True)
    (args.output / "report.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                                             encoding="utf-8")
    for problem in problems:
        print(f"shell_unwrap: {problem}", file=sys.stderr)
    print(f"shell_unwrap: {report['cells']} cell(s) unwrapped, {len(skipped)} not receivers; "
          f"{report['receiverFaces']} receiver face(s) over {report['receiverArea']:.0f} m², "
          f"{report['detailFaces']} detail face(s) over {report['detailArea']:.0f} m² left to the "
          f"dynamic term")
    gutter = report["smallestIslandGutter"]
    print(f"shell_unwrap: {report['atlases']} atlas(es), worst occupancy "
          f"{report['worstOccupancy'] * 100:.1f} % in {report['worstOccupancyCell']}, "
          f"worst density {report['worstDensity']:.2f} texels/m over {report['islands']} "
          f"island(s) and {report['texels'] / 1e6:.2f} M texel(s); tightest island gutter "
          f"{gutter if gutter is not None else 'none within reach'}")
    if report["needsAttention"]:
        print(f"shell_unwrap: worth a look: {', '.join(report['needsAttention'])}")
    return 1 if problems else 0


if __name__ == "__main__":
    try:
        _status = main()
    except SystemExit as _exit:
        _status = 1 if isinstance(_exit.code, str) else int(_exit.code or 0)
    except BaseException:  # noqa: BLE001
        import traceback

        traceback.print_exc()
        print("shell_unwrap: EXIT 1")
        raise
    print(f"shell_unwrap: EXIT {_status}")
    sys.exit(_status)
