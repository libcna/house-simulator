#!/usr/bin/env python3
"""Deterministically author the sunroom breakfast, lounge and fitted wet bar.

The breakfast asset is a measured four-place oak/rattan composition with real chair frames,
woven backs, cushions and restrained table dressing.  The bar is fitted joinery rather than a
blockout box: shaker fronts, stone worktop, sink/faucet, tiled upstand and dressed open shelves.
Coordinates use the repository's Y-up, -Z-forward glTF convention and both assets are grounded.

Run with: blender --background --python tools/blender/sunroom_suite.py --
          --asset breakfast|lounge|bar --out PATH
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


COLOURS = {
    "SUNROOM_OAK": (0.48, 0.25, 0.095, 1.0),
    "SUNROOM_RATTAN": (0.62, 0.42, 0.20, 1.0),
    "SUNROOM_CUSHION": (0.78, 0.73, 0.60, 1.0),
    "SUNROOM_CERAMIC": (0.78, 0.83, 0.75, 1.0),
    "SUNROOM_FRUIT": (0.92, 0.68, 0.10, 1.0),
    "SUNROOM_CABINET": (0.72, 0.76, 0.66, 1.0),
    "SUNROOM_STONE": (0.82, 0.80, 0.73, 1.0),
    "SUNROOM_METAL": (0.46, 0.48, 0.48, 1.0),
}
MATERIALS = {}
COUNTER = 0
PREFIX = "sunroom"


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
    return (point[0], -point[2], point[1])


def bake_world_vertices(obj):
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    for vertex in obj.data.vertices:
        vertex.co = transform @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()


def metre_uv(obj, tile_metres: float = 0.24):
    mesh = obj.data
    uv = mesh.uv_layers.active or mesh.uv_layers.new(name="UV0")
    for polygon in mesh.polygons:
        axis = max(range(3), key=lambda index: abs(polygon.normal[index]))
        for loop_index in polygon.loop_indices:
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


def rotate_xz(x: float, z: float, yaw_deg: float) -> tuple[float, float]:
    angle = math.radians(yaw_deg)
    return (math.cos(angle) * x - math.sin(angle) * z,
            math.sin(angle) * x + math.cos(angle) * z)


def box(label, location, size, finish="SUNROOM_OAK", bevel=0.006,
        yaw_deg=0.0, rotate_x_deg=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.dimensions = (size[0], size[2], size[1])
    obj.rotation_euler.x = math.radians(-rotate_x_deg)
    obj.rotation_euler.z = math.radians(-yaw_deg)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.16 if finish == "SUNROOM_RATTAN" else 0.26)
    if bevel:
        modifier = obj.modifiers.new("crafted_edge", "BEVEL")
        modifier.width = min(bevel, min(size) * 0.20)
        modifier.segments = 2
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    return obj


def tapered_cylinder(label, location, height, radius_bottom, radius_top,
                     finish="SUNROOM_OAK", vertices=20):
    bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=radius_bottom,
                                    radius2=radius_top, depth=height,
                                    location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.20)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def cylinder(label, location, height, radius, finish="SUNROOM_OAK", vertices=20):
    return tapered_cylinder(label, location, height, radius, radius, finish, vertices)


def sphere(label, location, radius, finish, subdivisions=2, scale=(1.0, 1.0, 1.0)):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdivisions, radius=radius,
                                         location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.scale = (scale[0], scale[2], scale[1])
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.14)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def rod(label, start, end, radius=0.012, finish="SUNROOM_RATTAN", vertices=10):
    a, b = Vector(blender_xyz(start)), Vector(blender_xyz(end))
    direction = b - a
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                        depth=direction.length, location=(a + b) * 0.5)
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.14)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def transformed_box(label, centre, local, size, yaw_deg, **kwargs):
    dx, dz = rotate_xz(local[0], local[2], yaw_deg)
    return box(label, (centre[0] + dx, local[1], centre[1] + dz), size,
               yaw_deg=yaw_deg, **kwargs)


def transformed_rod(label, centre, start, end, yaw_deg, **kwargs):
    ax, az = rotate_xz(start[0], start[2], yaw_deg)
    bx, bz = rotate_xz(end[0], end[2], yaw_deg)
    return rod(label, (centre[0] + ax, start[1], centre[1] + az),
               (centre[0] + bx, end[1], centre[1] + bz), **kwargs)


def breakfast_chair(index: int, centre: tuple[float, float], yaw_deg: float):
    """A 0.56 m woven barrel chair; local -Z is the seated person's front."""
    transformed_box(f"chair_{index}_seat_frame", centre, (0.0, 0.405, 0.0),
                    (0.54, 0.075, 0.50), yaw_deg, finish="SUNROOM_RATTAN", bevel=0.018)
    transformed_box(f"chair_{index}_cushion", centre, (0.0, 0.465, -0.015),
                    (0.49, 0.075, 0.44), yaw_deg, finish="SUNROOM_CUSHION", bevel=0.030)
    for side in (-1, 1):
        transformed_rod(f"chair_{index}_front_leg_{side}", centre,
                        (side * 0.215, 0.02, -0.195), (side * 0.215, 0.405, -0.195),
                        yaw_deg, radius=0.027)
        transformed_rod(f"chair_{index}_rear_post_{side}", centre,
                        (side * 0.225, 0.02, 0.205), (side * 0.255, 0.915, 0.235),
                        yaw_deg, radius=0.027)
        transformed_rod(f"chair_{index}_arm_{side}", centre,
                        (side * 0.245, 0.665, -0.18), (side * 0.255, 0.770, 0.225),
                        yaw_deg, radius=0.023)
    # A framed cane panel with a visible physical weave, rather than a solid back slab.
    transformed_rod(f"chair_{index}_crest", centre, (-0.255, 0.915, 0.235),
                    (0.255, 0.915, 0.235), yaw_deg, radius=0.030)
    transformed_rod(f"chair_{index}_back_rail", centre, (-0.230, 0.555, 0.215),
                    (0.230, 0.555, 0.215), yaw_deg, radius=0.021)
    for strand in range(5):
        x = -0.18 + strand * 0.09
        transformed_rod(f"chair_{index}_cane_v_{strand}", centre,
                        (x, 0.57, 0.220), (x, 0.89, 0.232), yaw_deg, radius=0.007,
                        vertices=8)
    for strand in range(4):
        y = 0.62 + strand * 0.075
        transformed_rod(f"chair_{index}_cane_h_{strand}", centre,
                        (-0.205, y, 0.225), (0.205, y, 0.225), yaw_deg, radius=0.006,
                        vertices=8)


def author_breakfast():
    """A compact 2.41 m square four-place breakfast composition."""
    cylinder("table_top", (0.0, 0.725, 0.0), 0.055, 0.555, "SUNROOM_OAK", 40)
    cylinder("table_edge", (0.0, 0.690, 0.0), 0.055, 0.575, "SUNROOM_RATTAN", 40)
    tapered_cylinder("table_pedestal", (0.0, 0.380, 0.0), 0.620, 0.085, 0.125,
                     "SUNROOM_OAK", 24)
    cylinder("table_foot", (0.0, 0.055, 0.0), 0.11, 0.36, "SUNROOM_OAK", 28)
    for index, (x, z, yaw) in enumerate(((0.0, -0.93, 180.0), (0.93, 0.0, 270.0),
                                         (0.0, 0.93, 0.0), (-0.93, 0.0, 90.0)), 1):
        breakfast_chair(index, (x, z), yaw)

    # Deliberate, sparse table dressing: two place settings, two mugs and a central fruit bowl.
    cylinder("fruit_bowl", (0.0, 0.790, 0.0), 0.055, 0.19, "SUNROOM_CERAMIC", 28)
    for index, (x, z) in enumerate(((-0.07, -0.02), (0.05, -0.06), (0.04, 0.06))):
        sphere(f"fruit_{index}", (x, 0.855, z), 0.060, "SUNROOM_FRUIT", 2,
               (1.0, 0.92, 1.0))
    for index, (x, z) in enumerate(((-0.29, -0.20), (0.27, 0.23))):
        cylinder(f"mug_{index}", (x, 0.825, z), 0.13, 0.055, "SUNROOM_CERAMIC", 20)
        # A physical handle silhouette is more legible than a texture at table distance.
        rod(f"mug_handle_{index}", (x + 0.050, 0.795, z),
            (x + 0.085, 0.850, z), 0.012, "SUNROOM_CERAMIC", 10)
    for index, (x, z) in enumerate(((-0.29, 0.18), (0.28, -0.18))):
        cylinder(f"plate_{index}", (x, 0.765, z), 0.018, 0.145, "SUNROOM_CERAMIC", 28)


def lounge_chair(index: int, centre: tuple[float, float]):
    """Broader cane reading chair, facing west toward the terrace route."""
    yaw = 270.0
    transformed_box(f"lounge_{index}_seat_frame", centre, (0.0, 0.405, 0.0),
                    (0.72, 0.085, 0.65), yaw, finish="SUNROOM_RATTAN", bevel=0.026)
    transformed_box(f"lounge_{index}_seat_cushion", centre, (0.0, 0.485, -0.025),
                    (0.65, 0.105, 0.58), yaw, finish="SUNROOM_CUSHION", bevel=0.046)
    for side in (-1, 1):
        transformed_rod(f"lounge_{index}_front_leg_{side}", centre,
                        (side * 0.295, 0.025, -0.25), (side * 0.295, 0.41, -0.25),
                        yaw, radius=0.033)
        transformed_rod(f"lounge_{index}_rear_post_{side}", centre,
                        (side * 0.305, 0.025, 0.27), (side * 0.32, 0.97, 0.37),
                        yaw, radius=0.034)
        transformed_rod(f"lounge_{index}_arm_{side}", centre,
                        (side * 0.32, 0.675, -0.255), (side * 0.32, 0.79, 0.35),
                        yaw, radius=0.028)
    transformed_rod(f"lounge_{index}_crest", centre, (-0.32, 0.97, 0.37),
                    (0.32, 0.97, 0.37), yaw, radius=0.036)
    transformed_rod(f"lounge_{index}_back_rail", centre, (-0.30, 0.55, 0.315),
                    (0.30, 0.55, 0.315), yaw, radius=0.024)
    for strand in range(7):
        x = -0.24 + strand * 0.08
        transformed_rod(f"lounge_{index}_cane_v_{strand}", centre,
                        (x, 0.565, 0.32), (x, 0.945, 0.365), yaw,
                        radius=0.007, vertices=8)
    for strand in range(5):
        y = 0.60 + strand * 0.075
        transformed_rod(f"lounge_{index}_cane_h_{strand}", centre,
                        (-0.275, y, 0.32), (0.275, y, 0.36), yaw,
                        radius=0.006, vertices=8)
    transformed_box(f"lounge_{index}_lumbar", centre, (0.0, 0.625, 0.22),
                    (0.48, 0.17, 0.09), yaw, finish="SUNROOM_CUSHION", bevel=0.035)
    for side in (-1, 1):
        for end in (-1, 1):
            transformed_box(f"lounge_{index}_foot_{side}_{end}", centre,
                            (side * 0.295, 0.015, end * 0.25),
                            (0.072, 0.030, 0.072), yaw,
                            finish="SUNROOM_OAK", bevel=0.004)


def author_lounge():
    """Two cane reading chairs and one shared tea table on the slider's east side."""
    lounge_chair(1, (0.0, -0.72))
    lounge_chair(2, (0.0, 0.72))
    cylinder("tea_table_top", (-0.77, 0.535, 0.0), 0.055, 0.29,
             "SUNROOM_OAK", 28)
    cylinder("tea_table_lip", (-0.77, 0.505, 0.0), 0.038, 0.30,
             "SUNROOM_RATTAN", 28)
    for index, (x, z) in enumerate(((-0.96, -0.16), (-0.58, -0.16),
                                    (-0.77, 0.22)), 1):
        rod(f"tea_table_leg_{index}", (x, 0.035, z), (x, 0.51, z),
            0.022, "SUNROOM_OAK", 12)
    cylinder("tea_cup", (-0.76, 0.63, -0.06), 0.13, 0.055,
             "SUNROOM_CERAMIC", 20)
    rod("tea_cup_handle", (-0.71, 0.61, -0.06), (-0.67, 0.66, -0.06),
        0.011, "SUNROOM_CERAMIC", 10)


def shaker_front(label: str, x: float, y: float, width: float, height: float):
    box(f"{label}_panel", (x, y, -0.307), (width - 0.065, height - 0.065, 0.025),
        finish="SUNROOM_CABINET", bevel=0.008)
    for side in (-1, 1):
        box(f"{label}_stile_{side}", (x + side * (width * 0.5 - 0.027), y, -0.327),
            (0.054, height, 0.035), finish="SUNROOM_OAK", bevel=0.006)
    for side in (-1, 1):
        box(f"{label}_rail_{side}", (x, y + side * (height * 0.5 - 0.027), -0.327),
            (width - 0.060, 0.054, 0.035), finish="SUNROOM_OAK", bevel=0.006)


def author_bar():
    """A 2.35 m fitted wet bar with a 0.94 m counter and dressed open shelving."""
    box("base_carcass", (0.0, 0.455, 0.0), (2.35, 0.83, 0.55),
        finish="SUNROOM_CABINET", bevel=0.012)
    box("recessed_plinth", (0.0, 0.055, -0.04), (2.20, 0.11, 0.42),
        finish="SUNROOM_OAK", bevel=0.006)
    shaker_front("left_door", -0.78, 0.46, 0.71, 0.69)
    shaker_front("sink_door", 0.0, 0.46, 0.71, 0.69)
    shaker_front("right_door", 0.78, 0.46, 0.71, 0.69)
    for x in (-0.78, 0.0, 0.78):
        cylinder(f"door_knob_{x:+.1f}", (x + 0.24, 0.50, -0.362), 0.026, 0.018,
                 "SUNROOM_METAL", 16)

    box("stone_counter", (0.0, 0.905, -0.015), (2.43, 0.075, 0.62),
        finish="SUNROOM_STONE", bevel=0.018)
    box("tiled_upstand", (0.0, 1.15, 0.270), (2.35, 0.42, 0.035),
        finish="SUNROOM_CERAMIC", bevel=0.004)
    # A dark stainless basin inset into the stone plus a real arched faucet.
    box("sink_basin", (0.0, 0.930, -0.06), (0.56, 0.035, 0.36),
        finish="SUNROOM_METAL", bevel=0.035)
    rod("faucet_riser", (0.0, 0.945, 0.13), (0.0, 1.26, 0.13),
        0.018, "SUNROOM_METAL", 14)
    rod("faucet_spout", (0.0, 1.26, 0.13), (0.0, 1.26, -0.08),
        0.018, "SUNROOM_METAL", 14)
    rod("faucet_drop", (0.0, 1.26, -0.08), (0.0, 1.17, -0.08),
        0.016, "SUNROOM_METAL", 14)

    for index, y in enumerate((1.58, 2.02), 1):
        box(f"open_shelf_{index}", (0.0, y, 0.12), (2.25, 0.055, 0.30),
            finish="SUNROOM_OAK", bevel=0.010)
        for x in (-0.98, 0.98):
            box(f"shelf_{index}_bracket_{x:+.1f}", (x, y - 0.13, 0.22),
                (0.035, 0.24, 0.035), finish="SUNROOM_METAL", bevel=0.004)

    # Controlled bar dressing: carafe, four tumblers, three anonymous bottles and two bowls.
    cylinder("carafe_body", (-0.65, 1.715, 0.09), 0.22, 0.075,
             "SUNROOM_CERAMIC", 24)
    tapered_cylinder("carafe_neck", (-0.65, 1.87, 0.09), 0.09, 0.035, 0.050,
                     "SUNROOM_CERAMIC", 20)
    for index, x in enumerate((-0.32, -0.12, 0.12, 0.32)):
        tapered_cylinder(f"tumbler_{index}", (x, 1.68, 0.05), 0.145, 0.045, 0.055,
                         "SUNROOM_CERAMIC", 20)
    for index, (x, height) in enumerate(((-0.64, 0.30), (0.0, 0.35), (0.62, 0.28))):
        cylinder(f"bottle_{index}_body", (x, 2.20 + height * 0.5, 0.08), height, 0.055,
                 "SUNROOM_CERAMIC", 20)
        cylinder(f"bottle_{index}_neck", (x, 2.20 + height + 0.045, 0.08), 0.09, 0.025,
                 "SUNROOM_METAL", 16)
    for index, x in enumerate((0.70, 0.91)):
        cylinder(f"bowl_{index}", (x, 1.66, 0.04), 0.10, 0.11,
                 "SUNROOM_CERAMIC", 24)


def centre_depth():
    """Put the fitted asset's rotation origin at its exact depth centre."""
    vertices = [vertex for obj in bpy.context.scene.objects if obj.type == "MESH"
                for vertex in obj.data.vertices]
    centre = (min(vertex.co.y for vertex in vertices) +
              max(vertex.co.y for vertex in vertices)) * 0.5
    for vertex in vertices:
        vertex.co.y -= centre


def centre_width():
    """Give the asymmetrical chair-and-table group a true placement centre."""
    vertices = [vertex for obj in bpy.context.scene.objects if obj.type == "MESH"
                for vertex in obj.data.vertices]
    centre = (min(vertex.co.x for vertex in vertices) +
              max(vertex.co.x for vertex in vertices)) * 0.5
    for vertex in vertices:
        vertex.co.x -= centre


def main():
    global PREFIX
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset", choices=("breakfast", "lounge", "bar"), required=True)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(arguments)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    PREFIX = f"sunroom_{options.asset}"
    {"breakfast": author_breakfast, "lounge": author_lounge,
     "bar": author_bar}[options.asset]()
    if options.asset == "lounge":
        centre_width()
    if options.asset == "bar":
        centre_depth()
    options.out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(options.out), export_format="GLB", export_yup=True,
                              export_apply=True, export_materials="EXPORT", export_lights=False,
                              export_cameras=False)
    print(f"sunroom_suite: {options.asset} {COUNTER} pieces")
    print("sunroom_suite: EXIT 0")


if __name__ == "__main__":
    main()
