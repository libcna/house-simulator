#!/usr/bin/env python3
"""Author the measured under-window butler-pantry service run.

The 1.56 m cabinet fits below the west window's world-Y 1.50 m sill and stays well west of
both side-wall doors. It shares the finished kitchen's shaker joinery and approved
stone/steel/tile material roles, but its sink, wine cooler and drawers form a distinct
small-room composition. Front is local -Z, support is Y=0.

Run with: blender --background --python tools/blender/butlers_service_run.py -- --out FILE
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kitchen_builtins as kit  # noqa: E402


def service_run() -> None:
    kit.CURRENT = "butlers_run"
    kit.COUNTERS[kit.CURRENT] = 0
    width, depth = 1.56, 0.56
    front = -depth * 0.5 - 0.013
    kit.toe_and_carcass("service", width, depth)

    # Three 520 mm working bays: a closed glass-fronted wine cooler, sink base and
    # three real recessed drawer faces. Hardware projects only 35 mm into the room.
    cooler_x = -0.52
    kit.box("wine_cooler_body", (cooler_x, 0.50, front + 0.012),
            (0.49, 0.736, 0.036), finish="CAB_STEEL", bevel=0.004)
    kit.box("wine_cooler_glass", (cooler_x, 0.50, front - 0.014),
            (0.430, 0.645, 0.016), finish="CAB_WINE_GLASS", bevel=0.006)
    for height in (0.21, 0.50, 0.79):
        kit.box(f"wine_shelf_rail_{height}", (cooler_x, height, front - 0.026),
                (0.428, 0.008, 0.014), finish="CAB_STEEL", bevel=0.002)
    kit.pull("wine_cooler", cooler_x + 0.155, 0.61, front - 0.032, length=0.16)

    kit.shaker("sink_base", 0.0, front, 0.495, 0.134, 0.864, 0.74)
    for index in range(3):
        bottom = 0.134 + index * 0.246
        kit.shaker(f"service_drawer_{index}", 0.52, front, 0.495,
                   bottom, bottom + 0.236, bottom + 0.18)

    # Four contiguous slabs create an actual 430 × 330 mm basin cutout rather
    # than placing an opaque painted rectangle on top of a solid work surface.
    opening = (-0.215, 0.215, -0.165, 0.165)
    stone_edges = (
        (-0.81, opening[0], -0.31, 0.31, "left"),
        (opening[1], 0.81, -0.31, 0.31, "right"),
        (opening[0], opening[1], -0.31, opening[2], "front"),
        (opening[0], opening[1], opening[3], 0.31, "back"),
    )
    for x0, x1, z0, z1, part in stone_edges:
        kit.box(f"counter_stone_{part}", ((x0 + x1) * 0.5, 0.918, (z0 + z1) * 0.5),
                (x1 - x0, 0.044, z1 - z0), finish="CAB_STONE", bevel=0.003)
    kit.box("sink_bottom", (0, 0.765, 0), (0.396, 0.016, 0.292),
            finish="CAB_STEEL", bevel=0.009)
    for side in (-1, 1):
        kit.box(f"sink_side_{side}", (side * 0.205, 0.843, 0),
                (0.016, 0.159, 0.302), finish="CAB_STEEL", bevel=0.003)
        kit.box(f"sink_end_{side}", (0, 0.843, side * 0.155),
                (0.422, 0.159, 0.016), finish="CAB_STEEL", bevel=0.003)
        kit.box(f"sink_lip_long_{side}", (0, 0.938, side * 0.174),
                (0.45, 0.007, 0.018), finish="CAB_STEEL", bevel=0.002)
        kit.box(f"sink_lip_short_{side}", (side * 0.225, 0.938, 0),
                (0.018, 0.007, 0.35), finish="CAB_STEEL", bevel=0.002)

    # Compact low-profile tap. Its short arch rises only 0.13 m into the lower
    # sash after installation, leaving the window as the room's visual anchor.
    kit.rod("tap_stem", (0, 0.94, 0.20), (0, 1.02, 0.20), 0.010)
    kit.rod("tap_elbow", (0, 1.02, 0.20), (0, 1.08, 0.13), 0.010)
    kit.rod("tap_spout", (0, 1.08, 0.13), (0, 1.08, -0.045), 0.010)
    kit.rod("tap_nozzle", (0, 1.08, -0.045), (0, 1.03, -0.045), 0.011)
    kit.rod("tap_lever", (0.04, 0.96, 0.19), (0.09, 1.00, 0.19), 0.006)

    # The L0 floor is world Y=0.60 and the window starts at Y=1.50, leaving
    # only 0.90 m below the sash. A measured 95% vertical profile puts the
    # counter at world Y=1.493; a standard 0.94 m kitchen run would block it.
    for obj in kit.bpy.context.scene.objects:
        if obj.type == "MESH":
            for vertex in obj.data.vertices:
                vertex.co.z *= 0.95
            obj.data.update()


def main() -> None:
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(args)
    kit.export(options.out, service_run)
    print("butlers_service_run: EXIT 0")


if __name__ == "__main__":
    main()
