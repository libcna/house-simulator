#!/usr/bin/env python3
"""Deterministically author the formal living room's fireplace composition.

The existing shell chimney is deliberately only the structural brick stack.  This model supplies
the missing room-side architecture: a stone hearth and surround, walnut mantel, recessed firebox,
iron grate, logs and one original framed over-mantel relief. Local -Z faces the room, Y=0 is the
finished floor and local Z=0 is the brick face, so the canonical prop can be measured directly
from the chimney rather than hidden behind an asset transform.

Run with: blender --background --python tools/blender/living_fireplace.py -- --out PATH
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


COLOURS = {
    "FIREPLACE_STONE": (0.72, 0.62, 0.49, 1.0),
    "FIREPLACE_IRON": (0.035, 0.040, 0.045, 1.0),
    "FIREPLACE_WOOD": (0.20, 0.075, 0.028, 1.0),
    "FIREPLACE_CANVAS": (0.16, 0.31, 0.37, 1.0),
    "FIREPLACE_BRASS": (0.58, 0.39, 0.14, 1.0),
}
MATERIALS = {}
COUNTER = 0


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
    return f"living_fireplace_{COUNTER:03d}_{label}"


def box(label, location, size, finish="FIREPLACE_STONE", bevel=0.008,
        bevel_segments=2, rotate_z_deg=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.dimensions = (size[0], size[2], size[1])
    obj.rotation_euler.z = math.radians(-rotate_z_deg)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.24 if finish in {"FIREPLACE_CANVAS", "FIREPLACE_WOOD"} else 0.32)
    if bevel:
        modifier = obj.modifiers.new("crafted_edge", "BEVEL")
        modifier.width = min(bevel, min(size) * 0.22)
        modifier.segments = bevel_segments
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    for polygon in obj.data.polygons:
        polygon.use_smooth = False
    return obj


def rod(label, start, end, radius, finish="FIREPLACE_IRON", vertices=16):
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
    metre_uv(obj, 0.20)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def wall_disc(label, location, radius, depth, finish="FIREPLACE_BRASS", vertices=32):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth,
                                        location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    # Primitive depth begins on Blender Z (glTF Y); turn it onto the wall's glTF Z axis.
    obj.rotation_euler.x = math.radians(90.0)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.24)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def collision_box(name: str, location, size):
    """One authored low proxy: the projecting hearth, not a wall-height trap."""
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = (size[0], size[2], size[1])
    bake_world_vertices(obj)
    return obj


def author():
    """A 1.78 m-wide traditional surround with a 0.88 m usable opening."""
    # Projecting hearth slabs establish a credible fire-safe floor edge and hide the raw stack's
    # square termination. Two heights give the edge a real shadow rather than a painted line.
    box("hearth_base", (0.0, 0.050, -0.285), (1.78, 0.10, 0.57), bevel=0.024,
        bevel_segments=4)
    box("hearth_inset", (0.0, 0.115, -0.245), (1.56, 0.055, 0.47), bevel=0.014,
        bevel_segments=3)

    # The firebox is recessed nearly to the brick plane. It is an actual dark volume bounded by
    # jambs and a lintel, not a black rectangle pasted across the chimney.
    box("firebox_back", (0.0, 0.555, -0.035), (0.88, 0.78, 0.040),
        finish="FIREPLACE_IRON", bevel=0.004)
    box("firebox_floor", (0.0, 0.165, -0.175), (0.88, 0.050, 0.30),
        finish="FIREPLACE_IRON", bevel=0.006)
    for side in (-1, 1):
        box(f"stone_jamb_{side}", (side * 0.575, 0.590, -0.155),
            (0.265, 0.90, 0.30), bevel=0.018, bevel_segments=3)
        box(f"jamb_plinth_{side}", (side * 0.575, 0.185, -0.175),
            (0.335, 0.20, 0.34), bevel=0.014, bevel_segments=3)
        box(f"jamb_cap_{side}", (side * 0.575, 1.010, -0.175),
            (0.335, 0.14, 0.34), bevel=0.014, bevel_segments=3)
        box(f"walnut_corbel_{side}", (side * 0.620, 1.205, -0.215),
            (0.185, 0.20, 0.30), finish="FIREPLACE_WOOD", bevel=0.026,
            bevel_segments=4)

    box("stone_lintel", (0.0, 1.075, -0.155), (1.38, 0.25, 0.31),
        bevel=0.018, bevel_segments=3)
    box("stone_keystone", (0.0, 1.085, -0.325), (0.19, 0.29, 0.055),
        bevel=0.010, bevel_segments=3)
    box("walnut_frieze", (0.0, 1.235, -0.205), (1.55, 0.11, 0.34),
        finish="FIREPLACE_WOOD", bevel=0.012, bevel_segments=3)
    box("walnut_mantel", (0.0, 1.340, -0.245), (1.78, 0.10, 0.49),
        finish="FIREPLACE_WOOD", bevel=0.020, bevel_segments=4)

    # A low iron grate and three deliberately non-parallel logs make the opening read as usable
    # even while the still-unimplemented fire state is off.
    rod("grate_front", (-0.37, 0.285, -0.360), (0.37, 0.285, -0.360), 0.018)
    rod("grate_back", (-0.34, 0.245, -0.145), (0.34, 0.245, -0.145), 0.014)
    for side in (-1, 1):
        rod(f"andiron_post_{side}", (side * 0.32, 0.175, -0.345),
            (side * 0.32, 0.455, -0.345), 0.018)
        rod(f"grate_side_{side}", (side * 0.32, 0.220, -0.365),
            (side * 0.32, 0.220, -0.130), 0.014)
    rod("log_low", (-0.34, 0.255, -0.285), (0.34, 0.300, -0.235), 0.060,
        finish="FIREPLACE_WOOD", vertices=18)
    rod("log_left", (-0.34, 0.335, -0.235), (0.16, 0.410, -0.315), 0.055,
        finish="FIREPLACE_WOOD", vertices=18)
    rod("log_right", (-0.12, 0.400, -0.315), (0.34, 0.335, -0.215), 0.052,
        finish="FIREPLACE_WOOD", vertices=18)

    # The structural brick continues upward, so an original layered relief provides a domestic
    # focal point without claiming reflective glass that the strict Tier-S renderer cannot show.
    box("overmantel_canvas", (0.0, 1.985, -0.030), (0.92, 0.56, 0.035),
        finish="FIREPLACE_CANVAS", bevel=0.004)
    for side in (-1, 1):
        box(f"art_frame_vertical_{side}", (side * 0.490, 1.985, -0.045),
            (0.055, 0.68, 0.075), finish="FIREPLACE_WOOD", bevel=0.009,
            bevel_segments=3)
        box(f"art_frame_horizontal_{side}", (0.0, 1.985 + side * 0.312, -0.045),
            (0.925, 0.055, 0.075), finish="FIREPLACE_WOOD", bevel=0.009,
            bevel_segments=3)
    box("art_horizon", (-0.07, 1.940, -0.058), (0.67, 0.042, 0.025),
        finish="FIREPLACE_BRASS", bevel=0.008, bevel_segments=3, rotate_z_deg=-3.0)
    wall_disc("art_sun", (-0.245, 2.085, -0.063), 0.075, 0.022,
              finish="FIREPLACE_BRASS", vertices=28)
    box("art_foreground", (0.18, 1.820, -0.060), (0.45, 0.095, 0.025),
        finish="FIREPLACE_WOOD", bevel=0.012, bevel_segments=3,
        rotate_z_deg=4.0)

    # Collision follows only the low projecting stone. A full-height box against the west wall
    # would form an artificial pinch volume with that wall; the tall surround needs no second
    # barrier because its back is already closed by the canonical structural chimney.
    collision_box("living_fireplace_COL", (0.0, 0.085, -0.285), (1.78, 0.17, 0.57))


def main():
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(arguments)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    author()
    options.out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(options.out), export_format="GLB",
                              export_yup=True, export_apply=True,
                              export_materials="EXPORT", export_lights=False,
                              export_cameras=False)
    print(f"living_fireplace: {COUNTER} visible pieces")
    print("living_fireplace: EXIT 0")


if __name__ == "__main__":
    main()
