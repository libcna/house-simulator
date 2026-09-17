#!/usr/bin/env python3
"""Deterministically author the range hood's close-range task-light puck.

The project-authored fixture is a shallow brushed-steel bezel with a separate frosted diffuser.
Its local origin is the diffuser's lower face, so the canonical point can sit 10 mm below the
visible lens without inventing a hidden transform.

Run with: blender --background --python tools/blender/kitchen_task_puck.py -- --out PATH
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy
from mathutils import Matrix


COLOURS = {
    "PUCK_METAL": (0.42, 0.44, 0.45, 1.0),
    "PuckDiffuser": (1.0, 0.84, 0.62, 1.0),
}
MATERIALS = {}


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
    """Convert the repository's Y-up coordinates to Blender's Z-up coordinates."""
    return (point[0], -point[2], point[1])


def bake_world_vertices(obj):
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    for vertex in obj.data.vertices:
        vertex.co = transform @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()


def metre_uv(obj, tile_metres: float = 0.08):
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


def cylinder(name: str, radius: float, height: float, centre_y: float,
             finish: str, vertices: int):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=height,
                                        location=blender_xyz((0.0, centre_y, 0.0)))
    obj = bpy.context.object
    obj.name = name
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj)
    for polygon in obj.data.polygons:
        polygon.use_smooth = polygon.normal.z == 0.0


def author():
    # The 82 mm steel body is flush to the hood filter. The 66 mm lens sits on the lower face,
    # leaving an 8 mm readable bezel at arm's length without a fake glowing outer shell.
    cylinder("kitchen_task_puck_steel_bezel", 0.041, 0.018, 0.009,
             "PUCK_METAL", 24)
    cylinder("kitchen_task_puck_frosted_diffuser", 0.033, 0.006, 0.003,
             "PuckDiffuser", 32)


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
    print("kitchen_task_puck: 2 pieces")
    print("kitchen_task_puck: EXIT 0")


if __name__ == "__main__":
    main()
