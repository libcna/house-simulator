#!/usr/bin/env python3
"""Author a measured linen-and-ceramic eight-place formal dining setting.

The existing table is 2.35 x 0.98 m with its top at local Y=0.76. This
separate non-collidable model rests on that top; it can be replaced without
changing the table's physical collision or the eight chairs' circulation.

Run with: blender --background --python tools/blender/dining_table_setting.py -- --out FILE
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dining_suite as kit  # noqa: E402


def setting() -> None:
    kit.PREFIX = "dining_setting"
    kit.COUNTER = 0
    kit.COLOURS.update({
        "DINING_LINEN": (0.85, 0.78, 0.65, 1.0),
        "DINING_PORCELAIN": (0.94, 0.90, 0.81, 1.0),
    })
    # A pale 1.66 m runner leaves the 75 mm breadboard ends visible; fine
    # hems, a short fold and metre-scaled fabric UV keep it from being a flat
    # monochrome rectangle seen down the room's long axis.
    kit.box("linen_runner", (0.0, 0.005, 0.0), (1.66, 0.010, 0.28),
            finish="DINING_LINEN", bevel=0.004)
    for side in (-1, 1):
        kit.box(f"linen_hem_{side}", (side * 0.808, 0.010, 0.0),
                (0.018, 0.006, 0.28), finish="DINING_LINEN", bevel=0.002)
    kit.box("linen_centre_fold", (0.0, 0.011, 0.0), (1.52, 0.005, 0.018),
            finish="DINING_LINEN", bevel=0.002)

    # Three places on each long side and one at each end match the eight
    # physical chairs. Every plate stays at least 65 mm from the table edge.
    places = [(x, z) for z in (-0.305, 0.305) for x in (-0.68, 0.0, 0.68)]
    places.extend(((-0.99, 0.0), (0.99, 0.0)))
    for index, (x, z) in enumerate(places):
        kit.vertical_cylinder(f"plate_rim_{index}", (x, 0.014, z),
                              0.010, 0.112, finish="DINING_PORCELAIN", vertices=32)
        kit.vertical_cylinder(f"plate_well_{index}", (x, 0.021, z),
                              0.004, 0.088, finish="DINING_PORCELAIN", vertices=32)
        if abs(z) > 0.1:
            # A folded sage napkin beside each side setting is purposeful,
            # not an extra stack of identical ceramic on the working lane.
            kit.box(f"napkin_{index}", (x + 0.155, 0.015, z),
                    (0.055, 0.012, 0.105), finish="DINING_FABRIC", bevel=0.003)

    # A single low serving vessel is the visual centre but does not hide
    # the chandelier or turn the table into a clutter shelf.
    kit.vertical_cylinder("centre_bowl_foot", (0.0, 0.021, 0.0),
                          0.021, 0.063, finish="DINING_PORCELAIN", vertices=32)
    kit.vertical_taper("centre_bowl_body", (0.0, 0.059, 0.0),
                       0.055, 0.081, 0.125,
                       finish="DINING_PORCELAIN", vertices=32)
    kit.vertical_cylinder("centre_bowl_inner", (0.0, 0.087, 0.0),
                          0.003, 0.104, finish="DINING_PORCELAIN", vertices=32)


def main() -> None:
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(arguments)
    kit.bpy.ops.object.select_all(action="SELECT")
    kit.bpy.ops.object.delete(use_global=False)
    setting()
    options.out.parent.mkdir(parents=True, exist_ok=True)
    kit.bpy.ops.export_scene.gltf(filepath=str(options.out), export_format="GLB",
                                  export_yup=True, export_apply=True,
                                  export_materials="EXPORT", export_lights=False,
                                  export_cameras=False)
    print("dining_table_setting: EXIT 0")


if __name__ == "__main__":
    main()
