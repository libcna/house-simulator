#!/usr/bin/env python3
"""Deterministically author the formal living room's measured upright piano.

The instrument is project-authored rather than a blockout: its case has inset panels and moulded
edges, all 88 keys are separate at the correct 52/36 split, and the keyboard, music desk, legs,
feet and three pedals have physical depth.  Local -Z faces the player, Y=0 is the support plane,
and UV0 repeats in metres.  Runtime source finishes are mapped in ``assets.manifest.json``.

Run with: blender --background --python tools/blender/living_piano.py -- --out PATH
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


MATERIAL_COLOURS = {
    "PIANO_WOOD": (0.12, 0.055, 0.028, 1.0),
    "PIANO_IVORY": (0.92, 0.89, 0.79, 1.0),
    "PIANO_EBONITE": (0.018, 0.020, 0.024, 1.0),
    "PIANO_BRASS": (0.55, 0.35, 0.10, 1.0),
}
MATERIALS = {}
COUNTER = 0
# The authored front projects farther than the closed back. Re-centre that asymmetric 0.7325 m
# envelope on the placement origin so yaw rotates around the case instead of a point behind it.
DEPTH_RECENTRE = 0.08625


def material(name: str):
    if name not in MATERIALS:
        entry = bpy.data.materials.new(name)
        entry.diffuse_color = MATERIAL_COLOURS[name]
        entry.use_nodes = True
        entry.node_tree.nodes.get("Principled BSDF").inputs[
            "Base Color"].default_value = MATERIAL_COLOURS[name]
        MATERIALS[name] = entry
    return MATERIALS[name]


def blender_xyz(gltf_xyz):
    """Blender is Z-up; the repository's glTF convention is Y-up and -Z-forward."""
    return (gltf_xyz[0], -(gltf_xyz[2] + DEPTH_RECENTRE), gltf_xyz[1])


def bake_world_vertices(obj):
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    for vertex in obj.data.vertices:
        vertex.co = transform @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()


def metre_uv(obj, tile_metres: float = 0.35):
    mesh = obj.data
    uv = mesh.uv_layers.active or mesh.uv_layers.new(name="UV0")
    for face in mesh.polygons:
        axis = max(range(3), key=lambda i: abs(face.normal[i]))
        for loop_index in face.loop_indices:
            point = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            if axis == 0:
                pair = (point.y, point.z)
            elif axis == 1:
                pair = (point.x, point.z)
            else:
                pair = (point.x, point.y)
            uv.data[loop_index].uv = (pair[0] / tile_metres, pair[1] / tile_metres)


def box(label, location, size, finish="PIANO_WOOD", bevel=0.004,
        rotate_x_deg=0.0):
    global COUNTER
    COUNTER += 1
    bpy.ops.mesh.primitive_cube_add(size=1, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = f"upright_piano_{COUNTER:03d}_{label}"
    obj.dimensions = (size[0], size[2], size[1])
    # glTF X maps directly to Blender X; the sign gives a backward-leaning music desk.
    obj.rotation_euler.x = math.radians(-rotate_x_deg)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.22 if finish in {"PIANO_IVORY", "PIANO_EBONITE"} else 0.35)
    if bevel:
        modifier = obj.modifiers.new("crafted_edge", "BEVEL")
        modifier.width = min(bevel, min(size) * 0.2)
        modifier.segments = 2
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    for polygon in obj.data.polygons:
        polygon.use_smooth = False
    return obj


def rod(label, start, end, radius=0.010, finish="PIANO_BRASS", vertices=12):
    global COUNTER
    a, b = Vector(blender_xyz(start)), Vector(blender_xyz(end))
    direction = b - a
    COUNTER += 1
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                        depth=direction.length,
                                        location=(a + b) * 0.5)
    obj = bpy.context.object
    obj.name = f"upright_piano_{COUNTER:03d}_{label}"
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def author():
    """A 1.48 x 1.24 x 0.62 m domestic upright, front toward local -Z."""
    # Structural back/case.  The shallow inset front and separate frame prevent the silhouette
    # from reading as one dark cuboid at the fixed room camera.
    box("case_back", (0.0, 0.68, 0.145), (1.42, 1.08, 0.27), bevel=0.010)
    box("top_lid", (0.0, 1.205, 0.075), (1.48, 0.070, 0.39), bevel=0.012)
    box("upper_inset", (0.0, 1.005, -0.012), (1.20, 0.285, 0.035), bevel=0.006)
    box("upper_rail_top", (0.0, 1.165, -0.040), (1.31, 0.060, 0.060), bevel=0.006)
    box("upper_rail_bottom", (0.0, 0.835, -0.040), (1.31, 0.060, 0.060), bevel=0.006)
    for side in (-1, 1):
        box(f"upper_stile_{side}", (side * 0.625, 1.000, -0.040),
            (0.060, 0.290, 0.060), bevel=0.006)

    # Keyboard shelf, fallboard and cheek blocks establish the characteristic stepped profile.
    box("key_shelf", (0.0, 0.738, -0.155), (1.46, 0.075, 0.55), bevel=0.008)
    box("fallboard", (0.0, 0.838, -0.055), (1.285, 0.145, 0.105), bevel=0.010,
        rotate_x_deg=5.0)
    for side in (-1, 1):
        box(f"key_cheek_{side}", (side * 0.682, 0.790, -0.245),
            (0.075, 0.160, 0.335), bevel=0.008)

    # A full 88-key keyboard.  MIDI pitches 21..108 produce exactly 52 white and 36 black keys.
    pitches = list(range(21, 109))
    white_pitches = [pitch for pitch in pitches if pitch % 12 not in {1, 3, 6, 8, 10}]
    white_width = 1.255 / len(white_pitches)
    white_x = {}
    for index, pitch in enumerate(white_pitches):
        x = -1.255 * 0.5 + (index + 0.5) * white_width
        white_x[pitch] = x
        box(f"white_key_{pitch}", (x, 0.797, -0.300),
            (white_width - 0.0012, 0.034, 0.305), finish="PIANO_IVORY", bevel=0.0006)
    black_index = 0
    for pitch in pitches:
        if pitch % 12 not in {1, 3, 6, 8, 10}:
            continue
        lower = max(value for value in white_pitches if value < pitch)
        upper = min(value for value in white_pitches if value > pitch)
        x = (white_x[lower] + white_x[upper]) * 0.5
        box(f"black_key_{pitch}", (x, 0.831, -0.238),
            (white_width * 0.58, 0.052, 0.185), finish="PIANO_EBONITE", bevel=0.0012)
        black_index += 1
    assert len(white_pitches) == 52 and black_index == 36

    # Framed lower panel, legs and feet retain air under the keyboard instead of filling the
    # entire instrument with a collision-looking slab.
    box("lower_inset", (0.0, 0.435, 0.010), (0.98, 0.410, 0.045), bevel=0.006)
    box("lower_rail_top", (0.0, 0.665, -0.010), (1.10, 0.070, 0.070), bevel=0.006)
    box("lower_rail_bottom", (0.0, 0.195, -0.005), (1.10, 0.075, 0.070), bevel=0.006)
    for side in (-1, 1):
        box(f"lower_stile_{side}", (side * 0.515, 0.430, -0.010),
            (0.070, 0.410, 0.070), bevel=0.006)
        box(f"leg_{side}", (side * 0.625, 0.360, -0.135),
            (0.095, 0.610, 0.105), bevel=0.010)
        box(f"foot_{side}", (side * 0.625, 0.050, -0.165),
            (0.235, 0.100, 0.395), bevel=0.016)
    box("base_plinth", (0.0, 0.060, 0.085), (1.34, 0.120, 0.300), bevel=0.012)

    # The tilted music desk and its ledge add a recognizable focal plane above the keys.
    box("music_desk", (0.0, 0.995, -0.145), (0.66, 0.245, 0.025), bevel=0.010,
        rotate_x_deg=10.0)
    box("music_ledge", (0.0, 0.866, -0.190), (0.72, 0.045, 0.095), bevel=0.006,
        rotate_x_deg=5.0)

    # Three separate brass pedals on real stems, rather than a gold decal on the lower panel.
    for index, x in enumerate((-0.075, 0.0, 0.075)):
        rod(f"pedal_stem_{index}", (x, 0.185, -0.015),
            (x * 1.25, 0.095, -0.265), radius=0.009)
        box(f"pedal_pad_{index}", (x * 1.25, 0.090, -0.310),
            (0.055, 0.025, 0.125), finish="PIANO_BRASS", bevel=0.010,
            rotate_x_deg=-6.0)


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(args)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    author()
    options.out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(options.out), export_format="GLB",
                              export_yup=True, export_apply=True,
                              export_materials="EXPORT", export_lights=False,
                              export_cameras=False)
    print(f"living_piano: {COUNTER} pieces")
    print("living_piano: EXIT 0")


if __name__ == "__main__":
    main()
