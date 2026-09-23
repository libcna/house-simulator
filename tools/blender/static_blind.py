#!/usr/bin/env python3
"""Deterministically author HOUSE-02681's reusable static slatted blind.

The blind is deliberately only a mesh and two material roles.  Local Z=0 is the wall plane,
the visible slats project toward local -Z, and local Y=0 is the centre of the window opening.
Room recipes provide size and tint through the existing baked placement variation path.

Run with: blender --background --python tools/blender/static_blind.py -- --out PATH
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


SLAT = "BLIND_SLAT"


def material(name: str, colour: tuple[float, float, float, float]):
    entry = bpy.data.materials.new(name)
    entry.diffuse_color = colour
    entry.use_nodes = True
    entry.node_tree.nodes.get("Principled BSDF").inputs[
        "Base Color"].default_value = colour
    return entry


def blender_xyz(point: tuple[float, float, float]):
    """Convert repository glTF Y-up coordinates to Blender Z-up coordinates."""
    return (point[0], -point[2], point[1])


def bake_world_vertices(obj) -> None:
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    for vertex in obj.data.vertices:
        vertex.co = transform @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()


def metre_uv(obj, tile_metres: float = 0.15) -> None:
    uv = obj.data.uv_layers.active or obj.data.uv_layers.new(name="UV0")
    for polygon in obj.data.polygons:
        axis = max(range(3), key=lambda index: abs(polygon.normal[index]))
        for loop_index in polygon.loop_indices:
            point = obj.data.vertices[obj.data.loops[loop_index].vertex_index].co
            pair = ((point.y, point.z), (point.x, point.z), (point.x, point.y))[axis]
            uv.data[loop_index].uv = (pair[0] / tile_metres, pair[1] / tile_metres)


def box(name: str, centre: tuple[float, float, float], size: tuple[float, float, float],
        finish, bevel: float = 0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(centre))
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = (size[0], size[2], size[1])
    bake_world_vertices(obj)
    obj.data.materials.append(finish)
    metre_uv(obj)
    if bevel:
        modifier = obj.modifiers.new("soft_edge", "BEVEL")
        modifier.width = bevel
        modifier.segments = 2
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    return obj


def cord(name: str, x: float, finish) -> None:
    start = Vector(blender_xyz((x, -0.70, -0.105)))
    end = Vector(blender_xyz((x, 0.68, -0.105)))
    direction = end - start
    bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.006, depth=direction.length,
                                        location=(start + end) * 0.5)
    obj = bpy.context.object
    obj.name = name
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    bake_world_vertices(obj)
    obj.data.materials.append(finish)
    metre_uv(obj, 0.05)


def author(output: Path) -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)

    slat = material(SLAT, (0.72, 0.82, 0.91, 1.0))

    # A compact domestic window family: one head rail, 20 fixed slats, two ladder cords and a
    # weighted bottom rail.  Slats intentionally remain fixed; interaction was removed from scope.
    box("blind_head_rail", (0.0, 0.765, -0.060), (1.32, 0.08, 0.12), slat, 0.01)
    for index in range(20):
        y = 0.675 - index * 0.071
        box(f"blind_slat_{index:02d}", (0.0, y, -0.085), (1.26, 0.034, 0.11),
            slat, 0.006)
    box("blind_bottom_rail", (0.0, -0.765, -0.080), (1.30, 0.075, 0.11),
        slat, 0.008)
    cord("blind_ladder_left", -0.41, slat)
    cord("blind_ladder_right", 0.41, slat)

    output.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(output), export_format="GLB", export_yup=True,
                              export_apply=True, export_materials="EXPORT",
                              export_texcoords=True, export_normals=True,
                              export_cameras=False, export_lights=False)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    author(options.out.resolve())
    print("static_blind: EXIT 0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
