#!/usr/bin/env python3
"""shell_unwrap.py -- the shell's second UV channel, at §18.3's densities.

`HOUSE-00471`. `lightmap_unwrap.py` (`HOUSE-00205`) packs one object's faces into a
density-uniform atlas and checks the six things that ruin a bake. This drives it over the whole
generated shell and picks the density §18.3 asks for, per cell:

> a second UV channel packed per cell into a texel-density-uniform atlas -- **4 texels/metre for
> rooms, 8 for small rooms, 2 for the attic and basement**

    tools/blender/shell_unwrap.py                     # build/shell -> build/shell-lm
    tools/blender/shell_unwrap.py --cells L0_KITCHEN
    tools/blender/shell_unwrap.py --selftest

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
from pathlib import Path  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
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


def unwrap_one(source: Path, destination: Path, density: float) -> dict:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(source))
    result = lightmap_unwrap.unwrap(density, lightmap_unwrap.DEFAULT_GUTTER,
                                    lightmap_unwrap.DEFAULT_MAX_ATLAS)
    destination.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(destination), export_format="GLB",
                              export_apply=False, export_yup=True)
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

    if not (SHELL / "L0_WC1.glb").is_file():
        require(False, "the shell is generated; run tools/blender/house_shell_gen.py first")
    else:
        result = unwrap_one(SHELL / "L0_WC1.glb", OUTPUT / "selftest" / "L0_WC1.glb",
                            DENSITY_SMALL)
        require(not result["problems"],
                f"the smallest room unwraps clean ({result['problems'][:2]})")
        require(result["minDensity"] >= DENSITY_SMALL - 0.01,
                f"at no less than the density it was asked for "
                f"({result['minDensity']:.2f} of {DENSITY_SMALL})")
        require(len(bpy.data.objects[0].data.uv_layers) == 2,
                f"and the result has TWO channels, the albedo one and the lightmap one "
                f"({len(bpy.data.objects[0].data.uv_layers)})")

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
    done = 0
    skipped = 0
    worst = 1e9
    for path in sorted(args.shell.glob("*.glb")):
        if wanted is not None and path.stem not in wanted:
            continue
        cell = cells.get(path.stem)
        if not wants_lightmap(cell):
            skipped += 1
            continue
        try:
            result = unwrap_one(path, args.output / path.name, density_for(cell))
        except RuntimeError as error:
            # Reported and carried on, not raised: one cell that will not pack must not hide the
            # state of the other ninety-five.
            problems.append(f"{path.stem}: {error}")
            continue
        done += 1
        worst = min(worst, result["minDensity"])
        for problem in result["problems"]:
            problems.append(f"{path.stem}: {problem}")

    for problem in problems:
        print(f"shell_unwrap: {problem}", file=sys.stderr)
    print(f"shell_unwrap: {done} object(s) unwrapped, {skipped} skipped as outdoors, "
          f"worst density {worst:.2f} texels/m")
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
