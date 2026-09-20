#!/usr/bin/env python3
"""Author two measured, stocked open shelves for the L0 butler-pantry side walls.

The project-authored cabinets are shallow enough to preserve the through-room aisle.
Their support-relative Y-up GLBs use the existing stock-XNA material bridge.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kitchen_builtins as kit  # noqa: E402


def cylinder(label: str, x: float, y: float, z: float, radius: float,
             height: float, finish: str, vertices: int = 16) -> None:
    kit.COUNTERS[kit.CURRENT] += 1
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=height,
                                        location=kit.blender_xyz((x, y, z)))
    obj = bpy.context.object
    obj.name = f"{kit.CURRENT}_{kit.COUNTERS[kit.CURRENT]:03d}_{label}"
    kit.bake_world_vertices(obj)
    obj.data.materials.append(kit.material(finish))
    kit.metre_uv(obj, 0.30)


def cabinet() -> None:
    # Finished 1.02 × 2.23 × 0.30 m carcase; front is local -Z. The broad,
    # light-painted back makes the useful storage read against the dark passage.
    kit.box("back", (0, 1.12, 0.145), (1.02, 2.20, 0.018), "CAB_PAINT")
    for side in (-1, 1):
        kit.box(f"side_{side}", (side * 0.50, 1.12, 0),
                (0.034, 2.22, 0.30), "CAB_PAINT", bevel=0.003)
    for index, height in enumerate((0.10, 0.51, 0.92, 1.33, 1.74, 2.16)):
        kit.box(f"oak_shelf_{index}", (0, height, -0.012),
                (1.02, 0.032, 0.30), "CAB_OAK", bevel=0.003)
    kit.box("base_plinth", (0, 0.043, -0.055),
            (1.02, 0.086, 0.205), "CAB_PAINT", bevel=0.003)
    kit.box("top_cornice", (0, 2.225, 0),
            (1.055, 0.03, 0.32), "CAB_OAK", bevel=0.003)


def dry_goods() -> None:
    # Practical stock at varied heights; no random scatter or duplicated arrangement.
    for index, x in enumerate((-0.32, -0.08, 0.18, 0.37)):
        kit.box(f"lower_basket_{index}", (x, 0.285, -0.015),
                (0.18 if index < 3 else 0.14, 0.25, 0.20), "CAB_OAK", bevel=0.009)
    for index, x in enumerate((-0.30, 0.0, 0.29)):
        cylinder(f"flour_tin_{index}", x, 0.70, 0.005, 0.095, 0.31,
                 "PANTRY_CERAMIC")
        cylinder(f"flour_lid_{index}", x, 0.865, 0.005, 0.101, 0.02,
                 "CAB_OAK")
    for index, x in enumerate((-0.34, -0.13, 0.12, 0.34)):
        cylinder(f"spice_jar_{index}", x, 1.105, 0.005, 0.064, 0.22,
                 "PANTRY_CERAMIC", vertices=12)
        cylinder(f"spice_lid_{index}", x, 1.225, 0.005, 0.067, 0.018,
                 "CAB_STEEL", vertices=12)
    for index, x in enumerate((-0.29, 0.10, 0.36)):
        kit.box(f"paper_packet_{index}", (x, 1.54, 0.015),
                (0.22 if index == 0 else 0.16, 0.30, 0.13),
                "PANTRY_PAPER", bevel=0.003)
    kit.box("folded_linen", (-0.18, 1.95, 0.015),
            (0.44, 0.095, 0.18), "PANTRY_LINEN", bevel=0.008)
    cylinder("upper_canister", 0.27, 1.94, 0.008, 0.11, 0.32,
             "PANTRY_CERAMIC")


def crockery() -> None:
    for index, x in enumerate((-0.26, 0.21)):
        kit.box(f"lower_crate_{index}", (x, 0.29, -0.016),
                (0.38, 0.27, 0.22), "CAB_OAK", bevel=0.010)
    for stack, x in enumerate((-0.30, 0.08, 0.34)):
        for level in range(3 + (stack % 2)):
            cylinder(f"plate_{stack}_{level}", x, 0.555 + level * 0.028,
                     0.0, 0.13 if stack == 0 else 0.10, 0.018,
                     "PANTRY_CERAMIC", vertices=20)
    for index, x in enumerate((-0.31, -0.10, 0.13, 0.34)):
        cylinder(f"mug_{index}", x, 1.09, 0.0, 0.066, 0.22,
                 "PANTRY_CERAMIC", vertices=12)
    for index, x in enumerate((-0.32, 0.0, 0.31)):
        cylinder(f"preserve_jar_{index}", x, 1.54, 0.0, 0.084,
                 0.27, "PANTRY_CERAMIC")
        cylinder(f"preserve_lid_{index}", x, 1.687, 0.0, 0.09,
                 0.02, "CAB_STEEL")
    kit.box("serving_tray", (-0.23, 1.91, 0.03),
            (0.46, 0.045, 0.22), "CAB_OAK", bevel=0.008)
    for index, x in enumerate((0.13, 0.32)):
        cylinder(f"upper_pitcher_{index}", x, 1.95, 0.0, 0.085,
                 0.30, "PANTRY_CERAMIC")


def main() -> None:
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--variant", choices=("dry", "crockery"), required=True)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(args)
    kit.CURRENT = f"butlers_{options.variant}_shelf"
    kit.COUNTERS[kit.CURRENT] = 0
    kit.MATERIAL_COLOURS.update({
        "PANTRY_CERAMIC": (0.92, 0.88, 0.78, 1.0),
        "PANTRY_PAPER": (0.78, 0.69, 0.52, 1.0),
        "PANTRY_LINEN": (0.75, 0.72, 0.62, 1.0),
    })

    def author() -> None:
        cabinet()
        (dry_goods if options.variant == "dry" else crockery)()

    kit.export(options.out, author)
    print("butlers_pantry_shelf: EXIT 0")


if __name__ == "__main__":
    main()
