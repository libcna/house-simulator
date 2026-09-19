#!/usr/bin/env python3
"""Deterministically author the formal dining suite and its sideboard lamps.

The five models form a coherent early-twentieth-century walnut set rather than blockout boxes:
the table has breadboard ends, moulded aprons and tapered brass-footed legs; the chair has a real
wood frame with separate upholstered seat/back; the six-shade chandelier has a ceiling canopy,
stem, frame and exact emissive shade slot. Coordinates follow the repository's Y-up, -Z-forward
glTF convention and every floor-standing asset is support-grounded.

Run with: blender --background --python tools/blender/dining_suite.py --
          --asset table|chair|chandelier|sideboard|side_lamp --out PATH
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


COLOURS = {
    "DINING_WOOD": (0.18, 0.075, 0.030, 1.0),
    "DINING_FABRIC": (0.30, 0.40, 0.24, 1.0),
    "DINING_BRASS": (0.52, 0.32, 0.10, 1.0),
    "CHANDELIER_METAL": (0.11, 0.065, 0.035, 1.0),
    "ChandelierShade": (1.0, 0.80, 0.52, 1.0),
    "SIDEBOARD_CERAMIC": (0.73, 0.69, 0.60, 1.0),
    "DiningSideShade": (0.98, 0.82, 0.60, 1.0),
}
MATERIALS = {}
COUNTER = 0
PREFIX = "dining"


def material(name: str):
    if name not in MATERIALS:
        entry = bpy.data.materials.new(name)
        entry.diffuse_color = COLOURS[name]
        entry.use_nodes = True
        entry.node_tree.nodes.get("Principled BSDF").inputs[
            "Base Color"].default_value = COLOURS[name]
        MATERIALS[name] = entry
    return MATERIALS[name]


def blender_xyz(point):
    """glTF X/Y/Z to Blender X/Y/Z (Blender is Z-up)."""
    return (point[0], -point[2], point[1])


def bake_world_vertices(obj):
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    for vertex in obj.data.vertices:
        vertex.co = transform @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()


def metre_uv(obj, tile_metres: float = 0.32):
    mesh = obj.data
    uv = mesh.uv_layers.active or mesh.uv_layers.new(name="UV0")
    for face in mesh.polygons:
        axis = max(range(3), key=lambda index: abs(face.normal[index]))
        for loop_index in face.loop_indices:
            point = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            if axis == 0:
                pair = (point.y, point.z)
            elif axis == 1:
                pair = (point.x, point.z)
            else:
                pair = (point.x, point.y)
            uv.data[loop_index].uv = (pair[0] / tile_metres, pair[1] / tile_metres)


def name_for(label: str) -> str:
    global COUNTER
    COUNTER += 1
    return f"{PREFIX}_{COUNTER:03d}_{label}"


def box(label, location, size, finish="DINING_WOOD", bevel=0.006,
        rotate_x_deg=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.dimensions = (size[0], size[2], size[1])
    obj.rotation_euler.x = math.radians(-rotate_x_deg)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.22 if finish == "DINING_FABRIC" else 0.34)
    if bevel:
        modifier = obj.modifiers.new("crafted_edge", "BEVEL")
        modifier.width = min(bevel, min(size) * 0.22)
        modifier.segments = 2
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    for polygon in obj.data.polygons:
        polygon.use_smooth = False
    return obj


def vertical_taper(label, location, height, radius_bottom, radius_top,
                   finish="DINING_WOOD", vertices=12):
    bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=radius_bottom,
                                    radius2=radius_top, depth=height,
                                    location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj)
    for polygon in obj.data.polygons:
        polygon.use_smooth = polygon.normal.z == 0.0
    return obj


def vertical_cylinder(label, location, height, radius, finish="DINING_BRASS",
                      vertices=16):
    return vertical_taper(label, location, height, radius, radius, finish, vertices)


def rod(label, start, end, radius=0.012, finish="CHANDELIER_METAL", vertices=12):
    a, b = Vector(blender_xyz(start)), Vector(blender_xyz(end))
    direction = b - a
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                        depth=direction.length,
                                        location=(a + b) * 0.5)
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.18)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def author_table():
    """A 2.35 x 0.98 m eight-place walnut table with a 0.76 m top."""
    # A broad centre panel and breadboard ends make the top read as joinery from the low review
    # camera. Thin edge rails add a shadow line without stacking a fake second tabletop.
    box("top_panel", (0.0, 0.7325, 0.0), (2.15, 0.055, 0.88), bevel=0.012)
    for side in (-1, 1):
        box(f"breadboard_{side}", (side * 1.125, 0.7325, 0.0),
            (0.10, 0.055, 0.98), bevel=0.012)
        box(f"end_apron_{side}", (side * 1.045, 0.635, 0.0),
            (0.060, 0.145, 0.76), bevel=0.008)
    for side in (-1, 1):
        box(f"long_apron_{side}", (0.0, 0.635, side * 0.410),
            (2.06, 0.145, 0.060), bevel=0.008)
        box(f"edge_moulding_{side}", (0.0, 0.700, side * 0.468),
            (2.16, 0.035, 0.035), bevel=0.006)

    for x in (-1.005, 1.005):
        for z in (-0.365, 0.365):
            vertical_taper(f"leg_{x:+.0f}_{z:+.0f}", (x, 0.330, z),
                           0.660, 0.045, 0.068)
            vertical_cylinder(f"brass_foot_{x:+.0f}_{z:+.0f}",
                              (x, 0.0275, z), 0.055, 0.050,
                              finish="DINING_BRASS", vertices=16)
    # A restrained low stretcher keeps the silhouette crafted while leaving knee space intact.
    box("low_stretcher", (0.0, 0.205, 0.0), (1.92, 0.075, 0.090), bevel=0.015)


def author_chair():
    """A 0.54 x 0.56 x 0.99 m upholstered dining chair; local -Z is the front."""
    box("seat_frame", (0.0, 0.395, -0.005), (0.515, 0.095, 0.515), bevel=0.012)
    box("seat_cushion", (0.0, 0.445, -0.030), (0.485, 0.065, 0.465),
        finish="DINING_FABRIC", bevel=0.025)

    # Front legs stop at the seat. Rear posts continue into the back and lean very slightly rearward.
    for side in (-1, 1):
        vertical_taper(f"front_leg_{side}", (side * 0.205, 0.195, -0.205),
                       0.390, 0.030, 0.044)
        box(f"rear_post_{side}", (side * 0.205, 0.485, 0.210),
            (0.070, 0.970, 0.070), bevel=0.012, rotate_x_deg=-1.5)
        box(f"side_rail_{side}", (side * 0.220, 0.330, -0.010),
            (0.055, 0.075, 0.390), bevel=0.008)

    box("front_rail", (0.0, 0.325, -0.220), (0.395, 0.080, 0.055), bevel=0.008)
    box("rear_rail", (0.0, 0.325, 0.220), (0.395, 0.080, 0.055), bevel=0.008)
    box("back_lower_rail", (0.0, 0.545, 0.205), (0.405, 0.065, 0.060), bevel=0.010)
    box("back_crest", (0.0, 0.940, 0.205), (0.535, 0.090, 0.080), bevel=0.018)
    box("back_pad", (0.0, 0.745, 0.185), (0.370, 0.315, 0.065),
        finish="DINING_FABRIC", bevel=0.030, rotate_x_deg=-2.0)
    # A narrow walnut reveal frames the upholstered panel and avoids a featureless slab back.
    for side in (-1, 1):
        box(f"back_reveal_{side}", (side * 0.205, 0.745, 0.205),
            (0.045, 0.310, 0.060), bevel=0.009, rotate_x_deg=-2.0)


def author_chandelier():
    """A six-shade rectangular chandelier, 1.25 x 0.52 x 1.19 m overall."""
    # Y=1.19 is the ceiling mounting plane once the canonical prop is placed at 2.46 m.
    vertical_cylinder("ceiling_canopy", (0.0, 1.155, 0.0), 0.070, 0.115,
                      finish="CHANDELIER_METAL", vertices=24)
    rod("drop_stem", (0.0, 0.455, 0.0), (0.0, 1.125, 0.0), radius=0.018)
    rod("long_frame", (-0.625, 0.455, 0.0), (0.625, 0.455, 0.0), radius=0.022)
    for x in (-0.50, 0.0, 0.50):
        rod(f"crossbar_{x:+.1f}", (x, 0.455, -0.260), (x, 0.455, 0.260), radius=0.016)
        for z in (-0.205, 0.205):
            rod(f"shade_drop_{x:+.1f}_{z:+.1f}", (x, 0.205, z),
                (x, 0.455, z), radius=0.012)
            vertical_cylinder(f"shade_collar_{x:+.1f}_{z:+.1f}",
                              (x, 0.195, z), 0.070, 0.050,
                              finish="CHANDELIER_METAL", vertices=16)
            # The lowest shade surface is Y=0. It is the exact switched emissive slot, with a
            # frosted bell volume rather than a floating billboard.
            bpy.ops.mesh.primitive_cone_add(vertices=24, radius1=0.088, radius2=0.055,
                                            depth=0.160,
                                            location=blender_xyz((x, 0.080, z)))
            shade = bpy.context.object
            shade.name = name_for(f"shade_{x:+.1f}_{z:+.1f}")
            bake_world_vertices(shade)
            shade.data.materials.append(material("ChandelierShade"))
            metre_uv(shade, 0.16)
            for polygon in shade.data.polygons:
                polygon.use_smooth = True


def author_sideboard():
    """A shallow 1.62 m walnut cabinet for the short solid north-wall bay.

    The enclosed carcass, framed door fronts and small drawers read as storage rather than a
    repeated foyer console. Its 0.48 m depth preserves the living, kitchen and storeroom routes.
    """
    box("carcass", (0.0, 0.485, 0.0), (1.55, 0.620, 0.445), bevel=0.007)
    box("top_cap", (0.0, 0.825, 0.0), (1.62, 0.060, 0.480), bevel=0.013)
    box("bottom_shadow_rail", (0.0, 0.190, -0.205), (1.48, 0.040, 0.035), bevel=0.005)
    for side in (-1, 1):
        box(f"carcase_edge_{side}", (side * 0.758, 0.490, -0.224),
            (0.040, 0.610, 0.035), bevel=0.005)
        for z in (-0.170, 0.170):
            vertical_taper(f"tapered_foot_{side}_{z:+.2f}",
                           (side * 0.695, 0.090, z), 0.180, 0.040, 0.054)
            vertical_cylinder(f"brass_sabot_{side}_{z:+.2f}",
                              (side * 0.695, 0.018, z), 0.036, 0.043,
                              finish="DINING_BRASS")
    for x in (-0.50, 0.0, 0.50):
        box(f"raised_door_{x:+.1f}", (x, 0.405, -0.231),
            (0.455, 0.395, 0.026), bevel=0.012)
        box(f"door_inset_{x:+.1f}", (x, 0.405, -0.249),
            (0.340, 0.275, 0.014), bevel=0.008)
        box(f"top_drawer_{x:+.1f}", (x, 0.690, -0.234),
            (0.455, 0.115, 0.025), bevel=0.006)
        box(f"drawer_pull_{x:+.1f}", (x, 0.690, -0.258),
            (0.100, 0.012, 0.025), finish="DINING_BRASS", bevel=0.004)
        box(f"door_pull_{x:+.1f}", (x + 0.155, 0.495, -0.266),
            (0.012, 0.090, 0.024), finish="DINING_BRASS", bevel=0.003)
    # One low ceramic bowl and a short serving-book stack keep the centre intentional without
    # obstructing the two separately switched, physically placed table lamps.
    vertical_taper("ceramic_serving_bowl", (0.0, 0.895, 0.015),
                   0.080, 0.125, 0.185, finish="SIDEBOARD_CERAMIC", vertices=32)
    for index, x in enumerate((-0.16, 0.17)):
        box(f"serving_book_{index}", (x, 0.875 + index * 0.022, 0.095),
            (0.20, 0.020, 0.14), finish="SIDEBOARD_CERAMIC", bevel=0.003)


def author_side_lamp():
    """A support-grounded 0.616 m ceramic/brass lamp with one exact switched shade slot."""
    vertical_cylinder("brass_foot", (0.0, 0.018, 0.0), 0.036, 0.092,
                      finish="DINING_BRASS", vertices=24)
    vertical_taper("ceramic_urn", (0.0, 0.165, 0.0), 0.265, 0.105, 0.080,
                   finish="SIDEBOARD_CERAMIC", vertices=32)
    vertical_cylinder("brass_neck", (0.0, 0.320, 0.0), 0.050, 0.038,
                      finish="DINING_BRASS", vertices=20)
    vertical_taper("side_shade", (0.0, 0.465, 0.0), 0.280, 0.190, 0.125,
                   finish="DiningSideShade", vertices=32)
    vertical_cylinder("shade_top_ring", (0.0, 0.610, 0.0), 0.012, 0.125,
                      finish="DINING_BRASS", vertices=32)


def main():
    global PREFIX
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset", choices=("table", "chair", "chandelier",
                                             "sideboard", "side_lamp"), required=True)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(arguments)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    PREFIX = f"dining_{options.asset}"
    {"table": author_table, "chair": author_chair,
     "chandelier": author_chandelier, "sideboard": author_sideboard,
     "side_lamp": author_side_lamp}[options.asset]()
    options.out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(options.out), export_format="GLB",
                              export_yup=True, export_apply=True,
                              export_materials="EXPORT", export_lights=False,
                              export_cameras=False)
    print(f"dining_suite: {options.asset} {COUNTER} pieces")
    print("dining_suite: EXIT 0")


if __name__ == "__main__":
    main()
