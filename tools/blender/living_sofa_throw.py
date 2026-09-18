#!/usr/bin/env python3
"""Deterministically author HOUSE-01062's formal-sofa draped wool throw.

The blanket is tailored to the camera-near arm of the approved formal sofa. Local Y=0 is the
lowest fringe point; X and Z are centred for transparent placement. A curved cross-section,
low-amplitude longitudinal folds, real solidified thickness and individual fringe cords keep it
from reading as a box or a painted card.

Run with: blender --background --python tools/blender/living_sofa_throw.py -- --out PATH
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


FINISH = "THROW_WOOL"


def material():
    entry = bpy.data.materials.new(FINISH)
    entry.diffuse_color = (0.42, 0.56, 0.80, 1.0)
    entry.use_nodes = True
    entry.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = entry.diffuse_color
    return entry


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


def blanket(finish):
    # Inner seat edge -> rounded arm crest -> outside hanging face. The 55 mm Y offset leaves
    # room for the fringe below while keeping the exported support plane exactly at Y=0.
    section = (
        (-0.225, 0.525),
        (-0.175, 0.590),
        (-0.105, 0.625),
        (-0.020, 0.635),
        (0.065, 0.610),
        (0.135, 0.520),
        (0.185, 0.350),
        (0.210, 0.165),
        (0.215, 0.055),
    )
    across = 17
    vertices = []
    uvs = []
    faces = []
    for row, (base_x, base_y) in enumerate(section):
        hang = row / (len(section) - 1)
        for column in range(across):
            along = column / (across - 1)
            z = -0.31 + 0.62 * along
            fold = math.sin(along * math.tau * 3.0 + hang * 0.8)
            x = base_x + fold * (0.004 + 0.013 * hang)
            y = base_y + math.cos(along * math.tau * 2.0 + hang) * (0.004 + 0.006 * hang)
            vertices.append(blender_xyz((x, y, z)))
            uvs.append((row / (len(section) - 1), along))
    for row in range(len(section) - 1):
        for column in range(across - 1):
            a = row * across + column
            b = a + 1
            c = (row + 1) * across + column + 1
            d = c - 1
            faces.append((a, b, c, d))

    mesh = bpy.data.meshes.new("living_sofa_throw_body_mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(finish)
    uv = mesh.uv_layers.new(name="UV0")
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            uv.data[loop_index].uv = uvs[mesh.loops[loop_index].vertex_index]
        polygon.use_smooth = True
    obj = bpy.data.objects.new("living_sofa_throw_body", mesh)
    bpy.context.collection.objects.link(obj)

    solidify = obj.modifiers.new("woven_thickness", "SOLIDIFY")
    solidify.thickness = 0.012
    solidify.offset = 0.0
    solidify.use_rim = True
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=solidify.name)
    bevel = obj.modifiers.new("soft_edge", "BEVEL")
    bevel.width = 0.003
    bevel.segments = 2
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    return obj


def fringe(index, z, finish):
    start = Vector(blender_xyz((0.214, 0.062, z)))
    sway = 0.008 * math.sin(index * 1.7)
    end = Vector(blender_xyz((0.216 + sway, 0.0, z + 0.006 * math.cos(index))))
    direction = end - start
    bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.0045, depth=direction.length,
                                        location=(start + end) * 0.5)
    obj = bpy.context.object
    obj.name = f"living_sofa_throw_fringe_{index:02d}"
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    bake_world_vertices(obj)
    obj.data.materials.append(finish)
    uv = obj.data.uv_layers.active or obj.data.uv_layers.new(name="UV0")
    for polygon in obj.data.polygons:
        for loop_index in polygon.loop_indices:
            point = obj.data.vertices[obj.data.loops[loop_index].vertex_index].co
            uv.data[loop_index].uv = (point.x * 8.0, point.y * 8.0)
        polygon.use_smooth = True


def author():
    finish = material()
    blanket(finish)
    for index in range(9):
        fringe(index, -0.27 + index * 0.0675, finish)


def main():
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(arguments)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    author()
    options.out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(options.out), export_format="GLB", export_yup=True,
                              export_apply=True, export_materials="EXPORT", export_lights=False,
                              export_cameras=False)
    print("living_sofa_throw: curved body + 9 fringe cords")
    print("living_sofa_throw: EXIT 0")


if __name__ == "__main__":
    main()
