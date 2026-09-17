#!/usr/bin/env python3
"""Deterministically author the family room's secondary furniture and lived-in detail.

The suite deliberately fills authored roles already named by the canonical room and navigation
data: a real dog bed, a bookcase with books, a compact sofa-side table and one framed artwork.
It uses four shared physical finish slots, measured metre geometry and explicit low-cost collision
where a standing case/table should stop the player. Coordinates are glTF Y-up, -Z-forward.

Run with: blender --background --python tools/blender/family_secondary_suite.py --
          --asset dog_bed|bookcase|side_table|wall_art --out PATH
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix


COLOURS = {
    "FAMILY_WOOD": (0.25, 0.105, 0.045, 1.0),
    "FAMILY_TEXTILE": (0.32, 0.45, 0.25, 1.0),
    "FAMILY_BOOK": (0.50, 0.16, 0.085, 1.0),
    "FAMILY_ART": (0.18, 0.32, 0.39, 1.0),
}
MATERIALS = {}
COUNTER = 0
PREFIX = "family"


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


def metre_uv(obj, tile_metres: float = 0.28):
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


def box(label, location, size, finish="FAMILY_WOOD", bevel=0.006,
        rotate_y_deg=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.dimensions = (size[0], size[2], size[1])
    obj.rotation_euler.z = math.radians(-rotate_y_deg)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.20 if finish in {"FAMILY_TEXTILE", "FAMILY_ART"} else 0.30)
    if bevel:
        modifier = obj.modifiers.new("softened_edge", "BEVEL")
        modifier.width = min(bevel, min(size) * 0.24)
        modifier.segments = 2
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    for polygon in obj.data.polygons:
        polygon.use_smooth = False
    return obj


def vertical_cylinder(label, location, height, radius, finish="FAMILY_WOOD", vertices=24):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=height,
                                        location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def collision_box(label, location, size):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = label
    obj.dimensions = (size[0], size[2], size[1])
    bake_world_vertices(obj)
    return obj


def join_collision(parts, name: str):
    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    parts[0].name = name


def author_dog_bed():
    """A 0.90 x 0.66 m washable bolster bed, 0.26 m high."""
    box("support_pad", (0.0, 0.060, 0.0), (0.90, 0.12, 0.66),
        finish="FAMILY_TEXTILE", bevel=0.050)
    box("inner_cushion", (0.0, 0.135, -0.025), (0.62, 0.11, 0.39),
        finish="FAMILY_ART", bevel=0.040)
    box("back_bolster", (0.0, 0.180, 0.255), (0.74, 0.16, 0.15),
        finish="FAMILY_TEXTILE", bevel=0.070)
    for side in (-1, 1):
        box(f"side_bolster_{side}", (side * 0.375, 0.180, 0.0),
            (0.15, 0.16, 0.54), finish="FAMILY_TEXTILE", bevel=0.070)


def add_book(row: int, index: int, x: float, shelf_y: float, width: float,
             height: float, lean: float = 0.0):
    # Muted terracotta, blue cloth and sage cloth bindings produce a composed shelf without
    # introducing one material/draw boundary per volume.
    finish = ("FAMILY_BOOK", "FAMILY_ART", "FAMILY_TEXTILE")[(row + index) % 3]
    box(f"book_{row}_{index}", (x, shelf_y + height * 0.5, -0.075),
        (width, height, 0.16), finish=finish, bevel=0.004,
        rotate_y_deg=lean)


def author_bookcase():
    """A 1.15 x 1.55 x 0.32 m walnut bookcase with composed, non-uniform shelves."""
    box("back", (0.0, 0.775, 0.142), (1.15, 1.55, 0.036), bevel=0.003)
    for side in (-1, 1):
        box(f"side_{side}", (side * 0.545, 0.775, 0.0), (0.060, 1.55, 0.32),
            bevel=0.010)
    shelf_levels = (0.035, 0.515, 0.995, 1.515)
    for index, y in enumerate(shelf_levels):
        box(f"shelf_{index}", (0.0, y, 0.0), (1.09, 0.070, 0.32), bevel=0.008)
    # Deliberately varied widths/heights and one leaning volume avoid the repeated-copy look.
    rows = (
        ((-0.43, 0.10, 0.31, 0.0), (-0.31, 0.08, 0.27, 0.0),
         (-0.20, 0.11, 0.35, -5.0), (0.17, 0.12, 0.28, 0.0),
         (0.32, 0.09, 0.34, 0.0), (0.43, 0.10, 0.25, 0.0)),
        ((-0.42, 0.09, 0.32, 0.0), (-0.30, 0.10, 0.38, 0.0),
         (-0.17, 0.08, 0.29, 4.0), (0.12, 0.13, 0.34, 0.0),
         (0.28, 0.11, 0.27, 0.0), (0.42, 0.10, 0.36, 0.0)),
        ((-0.41, 0.11, 0.30, 0.0), (-0.27, 0.08, 0.37, 0.0),
         (-0.16, 0.10, 0.32, 0.0), (0.14, 0.10, 0.26, -6.0),
         (0.28, 0.09, 0.35, 0.0), (0.41, 0.11, 0.31, 0.0)),
    )
    for row, volumes in enumerate(rows):
        support = shelf_levels[row] + 0.035
        for index, (x, width, height, lean) in enumerate(volumes):
            add_book(row, index, x, support, width, height, lean)
    collision_box(f"{PREFIX}_COL", (0.0, 0.775, 0.0),
                  (1.15, 1.55, 0.32))


def author_side_table():
    """A compact 0.52 m round walnut lamp/sofa table with an open base."""
    vertical_cylinder("top", (0.0, 0.535, 0.0), 0.050, 0.260, vertices=32)
    vertical_cylinder("lower_shelf", (0.0, 0.175, 0.0), 0.035, 0.215, vertices=32)
    legs = []
    for x in (-0.185, 0.185):
        for z in (-0.185, 0.185):
            vertical_cylinder(f"leg_{x:+.1f}_{z:+.1f}", (x, 0.255, z),
                              0.510, 0.025, vertices=12)
            legs.append(collision_box("leg_collision", (x, 0.255, z),
                                      (0.055, 0.510, 0.055)))
    box("reading_book", (0.045, 0.582, -0.015), (0.24, 0.025, 0.17),
        finish="FAMILY_BOOK", bevel=0.004, rotate_y_deg=-8.0)
    top_collision = collision_box("top_collision", (0.0, 0.535, 0.0),
                                  (0.52, 0.050, 0.52))
    join_collision([top_collision, *legs], f"{PREFIX}_COL")


def author_wall_art():
    """A 0.92 x 0.64 m framed abstract print, with its back on local +Z = 0."""
    box("canvas", (0.0, 0.0, -0.025), (0.82, 0.54, 0.030),
        finish="FAMILY_ART", bevel=0.002)
    for side in (-1, 1):
        box(f"frame_vertical_{side}", (side * 0.435, 0.0, -0.025),
            (0.050, 0.64, 0.050), bevel=0.006)
        box(f"frame_horizontal_{side}", (0.0, side * 0.295, -0.025),
            (0.82, 0.050, 0.050), bevel=0.006)
    # Three shallow shapes make this a composed image rather than a single coloured rectangle.
    box("art_field_warm", (-0.20, 0.06, -0.043), (0.25, 0.33, 0.012),
        finish="FAMILY_BOOK", bevel=0.004, rotate_y_deg=7.0)
    box("art_field_sage", (0.15, -0.08, -0.043), (0.34, 0.22, 0.012),
        finish="FAMILY_TEXTILE", bevel=0.004, rotate_y_deg=-5.0)
    box("art_horizon", (0.03, 0.17, -0.044), (0.43, 0.055, 0.014),
        finish="FAMILY_BOOK", bevel=0.003)


def main():
    global PREFIX
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset", choices=("dog_bed", "bookcase", "side_table", "wall_art"),
                        required=True)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(arguments)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    PREFIX = f"family_{options.asset}"
    {
        "dog_bed": author_dog_bed,
        "bookcase": author_bookcase,
        "side_table": author_side_table,
        "wall_art": author_wall_art,
    }[options.asset]()
    options.out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(options.out), export_format="GLB",
                              export_yup=True, export_apply=True,
                              export_materials="EXPORT", export_lights=False,
                              export_cameras=False)
    print(f"family_secondary_suite: {options.asset} {COUNTER} visible pieces")
    print("family_secondary_suite: EXIT 0")


if __name__ == "__main__":
    main()
