#!/usr/bin/env python3
"""Deterministically author HOUSE-00771's terrace and rear-lawn furniture.

The six assets form one restrained painted-metal/wood garden suite: a four-place round dining
group, a slatted lounger reused twice, an A-frame swing bench, an unlit fire pit, a stone birdbath
and a planted square pot reused around the terrace.  Coordinates use the repository's Y-up,
-Z-forward glTF convention and every model is support-grounded at Y=0.

Run with: blender --background --python tools/blender/garden_furniture_suite.py --
          --asset dining|lounger|swing|firepit|birdbath|planter --out PATH
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


COLOURS = {
    "GARDEN_WOOD": (0.34, 0.17, 0.075, 1.0),
    "GARDEN_METAL": (0.095, 0.11, 0.10, 1.0),
    "GARDEN_TEXTILE": (0.76, 0.69, 0.56, 1.0),
    "GARDEN_STONE": (0.25, 0.29, 0.31, 1.0),
    "GARDEN_SOIL": (0.12, 0.065, 0.035, 1.0),
    "GARDEN_FOLIAGE": (0.17, 0.38, 0.14, 1.0),
}
MATERIALS = {}
COUNTER = 0
PREFIX = "garden"


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


def metre_uv(obj, tile_metres: float = 0.30):
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


def box(label, location, size, finish="GARDEN_WOOD", bevel=0.008,
        yaw_deg=0.0, rotate_x_deg=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.dimensions = (size[0], size[2], size[1])
    obj.rotation_euler.x = math.radians(-rotate_x_deg)
    obj.rotation_euler.z = math.radians(-yaw_deg)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.18 if finish in {"GARDEN_TEXTILE", "GARDEN_FOLIAGE"} else 0.30)
    if bevel:
        modifier = obj.modifiers.new("crafted_edge", "BEVEL")
        modifier.width = min(bevel, min(size) * 0.20)
        modifier.segments = 2
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    return obj


def tapered_cylinder(label, location, height, radius_bottom, radius_top,
                     finish="GARDEN_METAL", vertices=16):
    bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=radius_bottom,
                                    radius2=radius_top, depth=height,
                                    location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def cylinder(label, location, height, radius, finish="GARDEN_METAL", vertices=16):
    return tapered_cylinder(label, location, height, radius, radius, finish, vertices)


def rod(label, start, end, radius=0.015, finish="GARDEN_METAL", vertices=12):
    a, b = Vector(blender_xyz(start)), Vector(blender_xyz(end))
    direction = b - a
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=direction.length,
                                        location=(a + b) * 0.5)
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.22)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def torus(label, location, major_radius, minor_radius, finish="GARDEN_STONE",
          major_segments=24, minor_segments=8):
    bpy.ops.mesh.primitive_torus_add(major_segments=major_segments, minor_segments=minor_segments,
                                    location=blender_xyz(location), major_radius=major_radius,
                                    minor_radius=minor_radius)
    obj = bpy.context.object
    obj.name = name_for(label)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.25)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def transformed_box(label, centre, local_location, size, yaw_deg, **kwargs):
    dx, dz = rotate_xz(local_location[0], local_location[2], yaw_deg)
    return box(label, (centre[0] + dx, local_location[1], centre[1] + dz), size,
               yaw_deg=yaw_deg, **kwargs)


def dining_chair(index: int, centre: tuple[float, float], yaw_deg: float):
    transformed_box(f"chair_{index}_seat", centre, (0.0, 0.455, 0.0),
                    (0.54, 0.07, 0.50), yaw_deg, finish="GARDEN_TEXTILE", bevel=0.025)
    transformed_box(f"chair_{index}_seat_frame", centre, (0.0, 0.405, 0.0),
                    (0.58, 0.075, 0.54), yaw_deg, bevel=0.012)
    for side in (-1, 1):
        for front in (-1, 1):
            transformed_box(f"chair_{index}_leg_{side}_{front}", centre,
                            (side * 0.225, 0.205, front * 0.205),
                            (0.045, 0.41, 0.045), yaw_deg, finish="GARDEN_METAL",
                            bevel=0.006)
        transformed_box(f"chair_{index}_back_post_{side}", centre,
                        (side * 0.235, 0.675, 0.225), (0.045, 0.53, 0.045), yaw_deg,
                        finish="GARDEN_METAL", bevel=0.006, rotate_x_deg=-4.0)
    for rung in range(3):
        transformed_box(f"chair_{index}_back_slat_{rung}", centre,
                        (0.0, 0.58 + rung * 0.115, 0.235), (0.43, 0.055, 0.035), yaw_deg,
                        bevel=0.008, rotate_x_deg=-4.0)


def author_dining():
    """A compact four-place 1.30 m round table group with 0.75 m tabletop."""
    cylinder("table_top", (0.0, 0.72, 0.0), 0.06, 0.65, "GARDEN_WOOD", 32)
    cylinder("table_pedestal", (0.0, 0.38, 0.0), 0.68, 0.065, "GARDEN_METAL", 16)
    for yaw in (0.0, 90.0, 180.0, 270.0):
        angle = math.radians(yaw)
        rod(f"table_foot_{int(yaw)}", (0.0, 0.055, 0.0),
            (math.sin(angle) * 0.48, 0.055, -math.cos(angle) * 0.48),
            0.035, "GARDEN_METAL", 12)
    dining_chair(1, (0.0, -1.02), 180.0)
    dining_chair(2, (1.02, 0.0), 270.0)
    dining_chair(3, (0.0, 1.02), 0.0)
    dining_chair(4, (-1.02, 0.0), 90.0)


def author_lounger():
    """A 0.72 x 1.92 m adjustable timber lounger with a breathable canvas pad."""
    for side in (-1, 1):
        box(f"side_rail_{side}", (side * 0.315, 0.30, -0.08),
            (0.055, 0.12, 1.78), bevel=0.010)
        box(f"front_leg_{side}", (side * 0.315, 0.15, -0.78),
            (0.065, 0.30, 0.065), bevel=0.008)
        box(f"rear_leg_{side}", (side * 0.315, 0.20, 0.67),
            (0.065, 0.40, 0.065), bevel=0.008, rotate_x_deg=-8.0)
    for index in range(9):
        z = -0.76 + index * 0.155
        box(f"seat_slat_{index}", (0.0, 0.36, z), (0.62, 0.035, 0.105), bevel=0.008)
    box("seat_pad", (0.0, 0.405, -0.12), (0.60, 0.055, 1.20),
        finish="GARDEN_TEXTILE", bevel=0.025)
    # The raised head section leans 34 degrees above the seat while keeping the overall support
    # point at Y=0 and the local -Z foot end visually obvious for placement.
    box("back_frame", (0.0, 0.62, 0.60), (0.64, 0.070, 0.82),
        bevel=0.012, rotate_x_deg=-34.0)
    box("back_pad", (0.0, 0.665, 0.58), (0.58, 0.060, 0.72),
        finish="GARDEN_TEXTILE", bevel=0.025, rotate_x_deg=-34.0)
    rod("back_stay_left", (-0.26, 0.32, 0.48), (-0.26, 0.72, 0.77),
        0.016, "GARDEN_METAL")
    rod("back_stay_right", (0.26, 0.32, 0.48), (0.26, 0.72, 0.77),
        0.016, "GARDEN_METAL")


def author_swing():
    """A 2.45 m timber A-frame with a suspended two-person bench and shallow canvas canopy."""
    for side in (-1, 1):
        rod(f"frame_{side}_front", (side * 1.02, 0.0, -0.52),
            (side * 0.82, 2.02, 0.0), 0.055, "GARDEN_WOOD", 16)
        rod(f"frame_{side}_rear", (side * 1.02, 0.0, 0.52),
            (side * 0.82, 2.02, 0.0), 0.055, "GARDEN_WOOD", 16)
        box(f"foot_{side}_front", (side * 1.02, 0.035, -0.52),
            (0.22, 0.07, 0.16), bevel=0.014)
        box(f"foot_{side}_rear", (side * 1.02, 0.035, 0.52),
            (0.22, 0.07, 0.16), bevel=0.014)
    rod("top_beam", (-1.18, 2.02, 0.0), (1.18, 2.02, 0.0), 0.065,
        "GARDEN_WOOD", 18)
    # Real suspension rods terminate at the seat side rails; the bench is static until the later
    # ambient-motion task, but it is visibly hung rather than balanced in the frame.
    for side in (-1, 1):
        rod(f"chain_{side}_front", (side * 0.68, 1.98, -0.08),
            (side * 0.68, 0.54, -0.34), 0.010, "GARDEN_METAL", 10)
        rod(f"chain_{side}_rear", (side * 0.68, 1.98, 0.08),
            (side * 0.68, 0.82, 0.30), 0.010, "GARDEN_METAL", 10)
    box("bench_seat_frame", (0.0, 0.50, -0.05), (1.55, 0.10, 0.64), bevel=0.014)
    box("bench_seat_pad", (0.0, 0.575, -0.08), (1.43, 0.075, 0.52),
        finish="GARDEN_TEXTILE", bevel=0.030)
    box("bench_back_frame", (0.0, 0.90, 0.25), (1.55, 0.62, 0.075),
        bevel=0.014, rotate_x_deg=-7.0)
    for index in range(5):
        box(f"bench_back_slat_{index}", (-0.56 + index * 0.28, 0.91, 0.205),
            (0.085, 0.48, 0.035), bevel=0.010, rotate_x_deg=-7.0)
    box("canopy", (0.0, 2.06, -0.02), (2.42, 0.055, 1.12),
        finish="GARDEN_TEXTILE", bevel=0.025, rotate_x_deg=3.0)


def author_firepit():
    """An intentionally unlit 1.20 m stone-ring fire pit; later work owns flames and audio."""
    cylinder("stone_plinth", (0.0, 0.09, 0.0), 0.18, 0.60, "GARDEN_STONE", 28)
    torus("stone_ring", (0.0, 0.26, 0.0), 0.43, 0.15, "GARDEN_STONE", 28, 8)
    cylinder("steel_bowl", (0.0, 0.285, 0.0), 0.10, 0.40, "GARDEN_METAL", 28)
    cylinder("cold_ash", (0.0, 0.345, 0.0), 0.025, 0.31, "GARDEN_SOIL", 24)
    for yaw in (30.0, 150.0):
        dx, dz = rotate_xz(0.0, 0.0, yaw)
        box(f"split_log_{int(yaw)}", (dx, 0.405, dz), (0.62, 0.09, 0.09),
            yaw_deg=yaw, bevel=0.025)


def author_birdbath():
    """A 0.88 m turned-stone birdbath with a broad shallow basin."""
    tapered_cylinder("foot", (0.0, 0.07, 0.0), 0.14, 0.27, 0.22,
                     "GARDEN_STONE", 24)
    tapered_cylinder("pedestal", (0.0, 0.43, 0.0), 0.62, 0.12, 0.075,
                     "GARDEN_STONE", 20)
    tapered_cylinder("capital", (0.0, 0.73, 0.0), 0.12, 0.16, 0.23,
                     "GARDEN_STONE", 24)
    torus("basin_rim", (0.0, 0.83, 0.0), 0.30, 0.075, "GARDEN_STONE", 28, 8)
    cylinder("basin_floor", (0.0, 0.79, 0.0), 0.055, 0.31, "GARDEN_STONE", 28)


def author_planter():
    """A 0.58 m square tapered pot with deliberate evergreen planting."""
    bpy.ops.mesh.primitive_cone_add(vertices=4, radius1=0.245, radius2=0.30, depth=0.44,
                                    rotation=(0.0, 0.0, math.radians(45.0)),
                                    location=blender_xyz((0.0, 0.22, 0.0)))
    pot = bpy.context.object
    pot.name = name_for("tapered_pot")
    bake_world_vertices(pot)
    pot.data.materials.append(material("GARDEN_STONE"))
    metre_uv(pot, 0.25)
    box("pot_rim", (0.0, 0.45, 0.0), (0.58, 0.09, 0.58),
        finish="GARDEN_STONE", bevel=0.018)
    cylinder("soil", (0.0, 0.492, 0.0), 0.025, 0.245, "GARDEN_SOIL", 20)
    for index, (x, y, z, radius) in enumerate((
            (0.0, 0.69, 0.0, 0.24), (-0.17, 0.66, 0.03, 0.17),
            (0.17, 0.67, 0.02, 0.18), (-0.05, 0.80, -0.10, 0.18),
            (0.08, 0.82, 0.12, 0.16), (-0.19, 0.76, -0.13, 0.14),
            (0.20, 0.74, -0.11, 0.15))):
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=radius,
                                             location=blender_xyz((x, y, z)))
        leaf = bpy.context.object
        leaf.name = name_for(f"foliage_{index}")
        leaf.scale = (1.0, 0.80, 1.30)
        bake_world_vertices(leaf)
        leaf.data.materials.append(material("GARDEN_FOLIAGE"))
        metre_uv(leaf, 0.18)
        for polygon in leaf.data.polygons:
            polygon.use_smooth = True


def main():
    global PREFIX
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset",
                        choices=("dining", "lounger", "swing", "firepit", "birdbath", "planter"),
                        required=True)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(arguments)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    PREFIX = f"garden_{options.asset}"
    {
        "dining": author_dining,
        "lounger": author_lounger,
        "swing": author_swing,
        "firepit": author_firepit,
        "birdbath": author_birdbath,
        "planter": author_planter,
    }[options.asset]()
    options.out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(options.out), export_format="GLB", export_yup=True,
                              export_apply=True, export_materials="EXPORT", export_lights=False,
                              export_cameras=False)
    print(f"garden_furniture_suite: {options.asset} {COUNTER} pieces")
    print("garden_furniture_suite: EXIT 0")


if __name__ == "__main__":
    main()
