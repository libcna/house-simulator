#!/usr/bin/env python3
"""Author the measured closed exterior of the canonical L0 kitchen refrigerator.

The local front is -Z, the base is Y=0, and the support-centre origin is preserved. The
closed frontage is intentionally separate mesh pieces from the carcass, so a later animated
appliance-door task can replace just these pieces without changing the canonical portal ID.
The existing CELL_FRIDGE_INTERIOR, FRIDGE_L0_KITCHEN and shell collision remain authoritative.

Run: blender --background --python tools/blender/kitchen_refrigerator.py -- --out PATH
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kitchen_builtins as joinery


def author() -> None:
    joinery.CURRENT = "refrigerator"
    joinery.COUNTERS[joinery.CURRENT] = 0
    # 1.80 × 1.95 × 0.70 m, matching layout.cells.json's American-fridge envelope.
    # The 120 mm end returns and 160 mm top/bottom bands conceal the shell leaf and reveal.
    joinery.box("carcass_back", (0, 0.975, 0.326), (1.76, 1.90, 0.028),
                "CAB_PAINT", bevel=0.004)
    joinery.box("carcass_top", (0, 1.926, 0), (1.80, 0.048, 0.70),
                "CAB_STEEL", bevel=0.004)
    joinery.box("carcass_bottom", (0, 0.027, 0), (1.80, 0.054, 0.70),
                "CAB_STEEL", bevel=0.003)
    for side in (-1, 1):
        joinery.box(f"carcass_side_{side}",
                    (side * 0.879, 0.975, 0), (0.042, 1.85, 0.70),
                    "CAB_PAINT", bevel=0.004)
        joinery.box(f"front_side_return_{side}",
                    (side * 0.836, 0.975, -0.349), (0.090, 1.81, 0.022),
                    "CAB_STEEL", bevel=0.003)
        joinery.box(f"toe_end_{side}",
                    (side * 0.79, 0.071, -0.346), (0.140, 0.142, 0.025),
                    "CAB_OAK", bevel=0.002)

    joinery.box("top_facade", (0, 1.866, -0.351), (1.58, 0.103, 0.024),
                "CAB_STEEL", bevel=0.003)
    joinery.box("bottom_vent_frame", (0, 0.126, -0.356), (1.58, 0.135, 0.024),
                "CAB_STEEL", bevel=0.003)
    for index in range(11):
        joinery.box(f"bottom_vent_louvre_{index:02d}",
                    (0, 0.074 + index * 0.009, -0.375),
                    (1.48, 0.0035, 0.008), "CAB_OAK", bevel=0.0008)

    # Two deliberately inset, independent door meshes with a physical 12 mm centre reveal.
    # The old one-leaf portal still represents the whole closed aperture; no dynamic swing is
    # claimed by this static furnishing checkpoint.
    for side in (-1, 1):
        centre_x = side * 0.397
        joinery.box(f"door_{side}_gasket_recess", (centre_x, 1.02, -0.349),
                    (0.778, 1.612, 0.014), "CAB_OAK", bevel=0.002)
        # A pale enamel face reads as a domestic appliance even under the kitchen's muted
        # indirect light; small steel rails/handles supply the metal accents.
        joinery.box(f"door_{side}_enamel_face", (centre_x, 1.02, -0.385),
                    (0.758, 1.592, 0.058), "CAB_PAINT", bevel=0.010)
        joinery.box(f"door_{side}_lower_rail", (centre_x, 0.272, -0.419),
                    (0.720, 0.026, 0.012), "CAB_STEEL", bevel=0.002)
        joinery.box(f"door_{side}_upper_rail", (centre_x, 1.769, -0.419),
                    (0.720, 0.026, 0.012), "CAB_STEEL", bevel=0.002)
        # A human-scale 970 mm vertical pull on each inward edge, with a 42 mm stand-off.
        handle_x = -0.043 if side < 0 else 0.043
        for y in (0.603, 1.551):
            joinery.rod(f"door_{side}_pull_post_{y:.3f}",
                        (handle_x, y, -0.425), (handle_x, y, -0.469),
                        0.009, vertices=12)
        joinery.rod(f"door_{side}_pull_grip", (handle_x, 0.588, -0.469),
                    (handle_x, 1.566, -0.469), 0.014, vertices=16)

    # A subtle integrated side water/dispenser bezel belongs on one door, not on both.
    joinery.box("dispenser_bezel", (-0.397, 1.20, -0.419),
                (0.246, 0.303, 0.008), "CAB_OAK", bevel=0.004)
    joinery.box("dispenser_inset", (-0.397, 1.20, -0.425),
                (0.219, 0.265, 0.006), "CAB_STEEL", bevel=0.003)
    joinery.box("dispenser_tray", (-0.397, 1.080, -0.437),
                (0.190, 0.018, 0.025), "CAB_STEEL", bevel=0.002)
    joinery.rod("dispenser_spout", (-0.397, 1.290, -0.428),
                (-0.397, 1.257, -0.448), 0.005, vertices=12)

    # The L0 ceiling is 2.70 m above the finished floor. A measured overhead bridge
    # cabinet hides the unfinished dark wall band above the 1.95 m appliance without
    # pretending that the refrigerator itself reaches the ceiling.
    joinery.box("bridge_carcass", (0, 2.313, 0.008),
                (1.80, 0.726, 0.684), "CAB_PAINT", bevel=0.003)
    joinery.box("bridge_lower_rail", (0, 1.972, -0.354),
                (1.80, 0.038, 0.020), "CAB_OAK", bevel=0.002)
    joinery.box("bridge_crown", (0, 2.672, -0.354),
                (1.80, 0.055, 0.030), "CAB_PAINT", bevel=0.004)
    for side in (-1, 1):
        joinery.shaker(f"bridge_{side}", side * 0.398, -0.366,
                       0.765, 2.067, 2.617, 2.345)


def export(path: Path) -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    author()
    # Include the projecting vertical pulls in the asset's geometric centre, just as the
    # first kitchen built-ins do. A support-centred but 67 mm off-axis glTF would fail
    # origin_check and rotate about the wrong point. The canonical prop translation is
    # adjusted by the same amount, leaving every finished world vertex unchanged.
    depths = [-vertex.co.y for obj in bpy.context.scene.objects if obj.type == "MESH"
              for vertex in obj.data.vertices]
    centre_z = (min(depths) + max(depths)) * 0.5
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH":
            for vertex in obj.data.vertices:
                vertex.co.y += centre_z
            obj.data.update()
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(path), export_format="GLB",
                              export_yup=True, export_apply=True,
                              export_materials="EXPORT", export_lights=False,
                              export_cameras=False)
    print(f"kitchen_refrigerator: {joinery.COUNTERS[joinery.CURRENT]} pieces")


def main() -> None:
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(args)
    export(options.out)
    print("kitchen_refrigerator: EXIT 0")


if __name__ == "__main__":
    main()
