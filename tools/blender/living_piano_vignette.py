#!/usr/bin/env python3
"""Deterministically author the formal living room's piano vignette.

The suite adds the missing human-scale pieces around the existing upright: a measured upholstered
bench, one original framed relief print and a wall-mounted picture light.  Local -Z faces the
viewer, Y=0 is the support plane for the bench, and all geometry uses metre-scaled UV0.

Run with: blender --background --python tools/blender/living_piano_vignette.py --
          --asset bench|art|picture_light --out PATH
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


COLOURS = {
    "PIANO_WOOD": (0.24, 0.10, 0.045, 1.0),
    "PIANO_LEATHER": (0.22, 0.09, 0.045, 1.0),
    "PIANO_BRASS": (0.62, 0.42, 0.16, 1.0),
    "LIVING_ART_CANVAS": (0.22, 0.39, 0.48, 1.0),
    "PictureLightDiffuser": (1.0, 0.80, 0.55, 1.0),
}
MATERIALS = {}
COUNTER = 0
PREFIX = "living_piano"


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
    """Convert the repository's glTF Y-up coordinates to Blender Z-up coordinates."""
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


def box(label, location, size, finish="PIANO_WOOD", bevel=0.006,
        rotate_z_deg=0.0, bevel_segments=2):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.dimensions = (size[0], size[2], size[1])
    obj.rotation_euler.z = math.radians(-rotate_z_deg)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.20 if finish in {"PIANO_LEATHER", "LIVING_ART_CANVAS"} else 0.30)
    if bevel:
        modifier = obj.modifiers.new("crafted_edge", "BEVEL")
        modifier.width = min(bevel, min(size) * 0.24)
        modifier.segments = bevel_segments
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    for polygon in obj.data.polygons:
        polygon.use_smooth = False
    return obj


def cylinder_between(label, start, end, radius, finish="PIANO_BRASS", vertices=16):
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
    metre_uv(obj)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def collision_box(name: str, location, size):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = (size[0], size[2], size[1])
    bake_world_vertices(obj)


def author_bench():
    """A 1.00 m duet bench with a 0.49 m seat and open stretcher base."""
    box("tufted_cushion", (0.0, 0.425, 0.0), (1.00, 0.105, 0.40),
        finish="PIANO_LEATHER", bevel=0.042, bevel_segments=6)
    box("seat_rail_front", (0.0, 0.335, -0.155), (0.88, 0.080, 0.055), bevel=0.010)
    box("seat_rail_back", (0.0, 0.335, 0.155), (0.88, 0.080, 0.055), bevel=0.010)
    for side in (-1, 1):
        x = side * 0.425
        box(f"leg_front_{side}", (x, 0.170, -0.145), (0.075, 0.34, 0.075),
            bevel=0.010)
        box(f"leg_back_{side}", (x, 0.170, 0.145), (0.075, 0.34, 0.075),
            bevel=0.010)
        box(f"side_stretcher_{side}", (x, 0.155, 0.0), (0.060, 0.060, 0.29),
            bevel=0.008)
    box("long_stretcher", (0.0, 0.145, 0.0), (0.82, 0.065, 0.060), bevel=0.009)
    # Six restrained covered buttons add upholstery depth without loose geometry.
    for row, z in enumerate((-0.095, 0.095)):
        for column, x in enumerate((-0.30, 0.0, 0.30)):
            box(f"tuft_button_{row}_{column}", (x, 0.481, z),
                (0.040, 0.010, 0.040), finish="PIANO_BRASS", bevel=0.010,
                bevel_segments=4)
    collision_box("living_piano_bench_COL", (0.0, 0.240, 0.0), (1.00, 0.48, 0.40))


def author_art():
    """A 1.15 x 0.72 m framed, project-authored geometric landscape relief."""
    box("canvas", (0.0, 0.0, -0.025), (1.03, 0.60, 0.030),
        finish="LIVING_ART_CANVAS", bevel=0.003)
    for side in (-1, 1):
        box(f"frame_vertical_{side}", (side * 0.55, 0.0, -0.015),
            (0.050, 0.72, 0.060), bevel=0.007)
        box(f"frame_horizontal_{side}", (0.0, side * 0.335, -0.015),
            (1.05, 0.050, 0.060), bevel=0.007)
    # A shallow asymmetric horizon, moon and foreground make this visibly distinct from the
    # family-room print while using only materials already present in the formal room.
    box("relief_horizon", (-0.08, -0.03, -0.049), (0.79, 0.055, 0.020),
        finish="PIANO_BRASS", bevel=0.008, rotate_z_deg=-4.0)
    box("relief_foreground", (0.19, -0.18, -0.050), (0.47, 0.10, 0.022),
        finish="PIANO_WOOD", bevel=0.010, rotate_z_deg=5.0)
    bpy.ops.mesh.primitive_cylinder_add(vertices=24, radius=0.085, depth=0.018,
                                        location=blender_xyz((-0.28, 0.15, -0.055)))
    moon = bpy.context.object
    moon.name = name_for("relief_moon")
    # Blender's primitive axis is Z (glTF Y); rotate it onto the wall plane so its shallow axis
    # becomes glTF Z. A cylinder is deterministic here, unlike Blender's UV-sphere pole ordering.
    moon.rotation_euler.x = math.radians(90.0)
    bake_world_vertices(moon)
    moon.data.materials.append(material("PIANO_BRASS"))
    metre_uv(moon)
    for polygon in moon.data.polygons:
        polygon.use_smooth = True


def author_picture_light():
    """A compact brass picture light whose diffuser names the linked runtime emission slot."""
    box("wall_backplate", (0.0, 0.0, -0.018), (0.20, 0.18, 0.036),
        finish="PIANO_BRASS", bevel=0.020, bevel_segments=4)
    for x in (-0.24, 0.24):
        cylinder_between(f"support_arm_{x:+.2f}", (x, -0.015, -0.025),
                         (x, -0.055, -0.165), 0.018)
    cylinder_between("shade_bar", (-0.34, -0.055, -0.175),
                     (0.34, -0.055, -0.175), 0.045, vertices=24)
    box("diffuser", (0.0, -0.097, -0.175), (0.62, 0.025, 0.070),
        finish="PictureLightDiffuser", bevel=0.010, bevel_segments=3)


def main():
    global PREFIX
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset", choices=("bench", "art", "picture_light"), required=True)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(arguments)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    PREFIX = f"living_piano_{options.asset}"
    {
        "bench": author_bench,
        "art": author_art,
        "picture_light": author_picture_light,
    }[options.asset]()
    options.out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(options.out), export_format="GLB",
                              export_yup=True, export_apply=True,
                              export_materials="EXPORT", export_lights=False,
                              export_cameras=False)
    print(f"living_piano_vignette: {options.asset} {COUNTER} visible pieces")
    print("living_piano_vignette: EXIT 0")


if __name__ == "__main__":
    main()
