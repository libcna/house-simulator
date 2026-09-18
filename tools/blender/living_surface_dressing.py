#!/usr/bin/env python3
"""Deterministically author HOUSE-01063's formal-living surface dressing.

The set is intentionally small: an original paired botanical relief for the blank south-wall bay
and one measured books/tray/bowl vignette for the existing coffee table.  Wall-art local +Z is the
wall plane and -Z faces the viewer; tabletop Y=0 is the support plane.  Every visible primitive has
metre-scaled UV0 and uses a canonical material slot rather than an embedded texture.

Run with: blender --background --python tools/blender/living_surface_dressing.py --
          --asset art|table --out PATH
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


COLOURS = {
    "DRESSING_WOOD": (0.24, 0.10, 0.045, 1.0),
    "DRESSING_BRASS": (0.62, 0.42, 0.16, 1.0),
    "DRESSING_CANVAS": (0.24, 0.42, 0.50, 1.0),
    "DRESSING_CREAM": (0.84, 0.74, 0.57, 1.0),
    "DRESSING_BOOK": (0.54, 0.22, 0.11, 1.0),
    "DRESSING_STONE": (0.62, 0.60, 0.55, 1.0),
}
MATERIALS = {}
COUNTER = 0
PREFIX = "living_dressing"


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
    """Convert repository glTF Y-up coordinates to Blender Z-up coordinates."""
    return (point[0], -point[2], point[1])


def bake_world_vertices(obj):
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    for vertex in obj.data.vertices:
        vertex.co = transform @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()


def metre_uv(obj, tile_metres: float = 0.20):
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


def box(label, location, size, finish, bevel=0.004, rotate_y_deg=0.0,
        rotate_z_deg=0.0, bevel_segments=2):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.dimensions = (size[0], size[2], size[1])
    obj.rotation_euler.y = math.radians(-rotate_y_deg)
    obj.rotation_euler.z = math.radians(-rotate_z_deg)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj)
    if bevel:
        modifier = obj.modifiers.new("crafted_edge", "BEVEL")
        modifier.width = min(bevel, min(size) * 0.24)
        modifier.segments = bevel_segments
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    for polygon in obj.data.polygons:
        polygon.use_smooth = False
    return obj


def cylinder_between(label, start, end, radius, finish="DRESSING_BRASS", vertices=12):
    a, b = Vector(blender_xyz(start)), Vector(blender_xyz(end))
    direction = b - a
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                        depth=direction.length, location=(a + b) * 0.5)
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def vertical_cylinder(label, location, radius, height, finish, vertices=24,
                      scale_x=1.0, scale_z=1.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=height,
                                        location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.scale.x = scale_x
    obj.scale.y = scale_z
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def author_art():
    """Two 0.64 x 0.78 m walnut-framed botanical reliefs with their backs at +Z=0."""
    for side in (-1, 1):
        centre = side * 0.39
        # Cream mat and inset blue field retain visible layered depth instead of a screenshot card.
        box(f"mat_{side}", (centre, 0.0, -0.034), (0.54, 0.66, 0.022),
            "DRESSING_CREAM", bevel=0.002)
        box(f"canvas_{side}", (centre, 0.0, -0.049), (0.44, 0.56, 0.012),
            "DRESSING_CANVAS", bevel=0.003)
        for edge in (-1, 1):
            box(f"frame_vertical_{side}_{edge}",
                (centre + edge * 0.295, 0.0, -0.030), (0.050, 0.78, 0.060),
                "DRESSING_WOOD", bevel=0.007)
            box(f"frame_horizontal_{side}_{edge}",
                (centre, edge * 0.365, -0.030), (0.54, 0.050, 0.060),
                "DRESSING_WOOD", bevel=0.007)

        # The paired stems mirror the overall rhythm but have deliberately different branches.
        lean = 0.035 * side
        cylinder_between(f"botanical_stem_{side}",
                         (centre - lean, -0.22, -0.066),
                         (centre + lean, 0.22, -0.066), 0.008)
        branches = ((-0.10, -0.04, -21.0), (0.02, 0.08, 18.0),
                    (0.12, 0.17, -16.0))
        for index, (y, x_offset, angle) in enumerate(branches):
            direction = side if index != 1 else -side
            x = centre + x_offset * direction
            cylinder_between(f"botanical_branch_{side}_{index}",
                             (centre + lean * (y / 0.22), y, -0.066),
                             (x, y + 0.085, -0.066), 0.006)
            box(f"botanical_leaf_{side}_{index}",
                (x + 0.028 * direction, y + 0.105, -0.070),
                (0.10, 0.045, 0.012), "DRESSING_BRASS", bevel=0.015,
                rotate_z_deg=angle * direction, bevel_segments=4)


def book(label, centre, size, angle):
    """One closed hardback with visible cream page block and finite covers."""
    x, y, z = centre
    box(f"{label}_pages", (x, y, z), (size[0] - 0.014, size[1] - 0.012,
                                      size[2] - 0.012), "DRESSING_CREAM",
        bevel=0.003, rotate_y_deg=angle)
    for sign in (-1, 1):
        box(f"{label}_cover_{sign}",
            (x, y + sign * (size[1] * 0.5 - 0.003), z),
            (size[0], 0.006, size[2]), "DRESSING_BOOK", bevel=0.003,
            rotate_y_deg=angle)


def author_table():
    """A restrained 0.72 m coffee-table vignette: two books, brass tray and stone bowl."""
    book("book_lower", (-0.18, 0.026, 0.0), (0.36, 0.052, 0.25), -6.0)
    book("book_upper", (-0.17, 0.075, 0.0), (0.30, 0.046, 0.21), 5.0)

    # The shallow elliptical tray has a separate raised rim so it reads at the review distance.
    vertical_cylinder("tray_base", (0.18, 0.010, 0.0), 0.18, 0.020,
                      "DRESSING_BRASS", vertices=32, scale_z=0.72)
    bpy.ops.mesh.primitive_torus_add(major_segments=32, minor_segments=8,
                                     location=blender_xyz((0.18, 0.026, 0.0)),
                                     major_radius=0.157, minor_radius=0.010)
    rim = bpy.context.object
    rim.name = name_for("tray_rim")
    rim.scale.y = 0.72
    bake_world_vertices(rim)
    rim.data.materials.append(material("DRESSING_BRASS"))
    metre_uv(rim)
    for polygon in rim.data.polygons:
        polygon.use_smooth = True

    # A low sculptural stone bowl, not loose procedural clutter or simulated food.
    vertical_cylinder("bowl_foot", (0.18, 0.040, 0.0), 0.055, 0.025,
                      "DRESSING_STONE", vertices=24)
    bpy.ops.mesh.primitive_torus_add(major_segments=32, minor_segments=10,
                                     location=blender_xyz((0.18, 0.087, 0.0)),
                                     major_radius=0.092, minor_radius=0.020)
    bowl = bpy.context.object
    bowl.name = name_for("sculptural_bowl")
    bowl.scale.z = 1.35
    bake_world_vertices(bowl)
    bowl.data.materials.append(material("DRESSING_STONE"))
    metre_uv(bowl)
    for polygon in bowl.data.polygons:
        polygon.use_smooth = True


def main():
    global PREFIX
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset", choices=("art", "table"), required=True)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(arguments)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    PREFIX = f"living_dressing_{options.asset}"
    {"art": author_art, "table": author_table}[options.asset]()
    options.out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(options.out), export_format="GLB",
                              export_yup=True, export_apply=True,
                              export_materials="EXPORT", export_lights=False,
                              export_cameras=False)
    print(f"living_surface_dressing: {options.asset} {COUNTER} visible pieces")
    print("living_surface_dressing: EXIT 0")


if __name__ == "__main__":
    main()
