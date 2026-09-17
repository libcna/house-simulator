#!/usr/bin/env python3
"""Deterministically author the kitchen island's close-range pendant fixture.

The fixture is a restrained dark-bronze bell with a separately switched frosted diffuser.  Its
local origin is the diffuser's lower face; canonical placement therefore makes the authored light
point and visible emitter agree without a hidden transform offset.

Run with: blender --background --python tools/blender/kitchen_pendant.py -- --out PATH
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


COLOURS = {
    "PENDANT_METAL": (0.075, 0.045, 0.025, 1.0),
    "PendantShade": (1.0, 0.78, 0.48, 1.0),
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
    """Convert the repository's Y-up coordinates to Blender's Z-up coordinates."""
    return (point[0], -point[2], point[1])


def name_for(label: str) -> str:
    global COUNTER
    COUNTER += 1
    return f"kitchen_pendant_{COUNTER:03d}_{label}"


def bake_world_vertices(obj):
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    for vertex in obj.data.vertices:
        vertex.co = transform @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()


def metre_uv(obj, tile_metres: float = 0.18):
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


def vertical_cylinder(label: str, location, height: float, radius: float, finish: str,
                      vertices: int = 24):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=height,
                                        location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj)
    for polygon in obj.data.polygons:
        polygon.use_smooth = polygon.normal.z == 0.0
    return obj


def rod(label: str, start, end, radius: float = 0.008):
    a, b = Vector(blender_xyz(start)), Vector(blender_xyz(end))
    direction = b - a
    bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=radius, depth=direction.length,
                                        location=(a + b) * 0.5)
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    bake_world_vertices(obj)
    obj.data.materials.append(material("PENDANT_METAL"))
    metre_uv(obj)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def author():
    # A 300 mm metal bell stops direct upward light. The real switched emitter is the shallow
    # frosted diffuser closing its lower face, not the whole outer shell.
    bpy.ops.mesh.primitive_cone_add(vertices=32, radius1=0.165, radius2=0.067, depth=0.245,
                                    location=blender_xyz((0.0, 0.1475, 0.0)))
    bell = bpy.context.object
    bell.name = name_for("bell_shade")
    bake_world_vertices(bell)
    bell.data.materials.append(material("PENDANT_METAL"))
    metre_uv(bell)
    for polygon in bell.data.polygons:
        polygon.use_smooth = True

    vertical_cylinder("frosted_diffuser", (0.0, 0.020, 0.0), 0.040, 0.150,
                      "PendantShade", vertices=32)
    vertical_cylinder("shade_collar", (0.0, 0.2925, 0.0), 0.045, 0.058,
                      "PENDANT_METAL", vertices=24)
    rod("suspension_cord", (0.0, 0.315, 0.0), (0.0, 0.815, 0.0), radius=0.007)
    vertical_cylinder("ceiling_canopy", (0.0, 0.835, 0.0), 0.080, 0.095,
                      "PENDANT_METAL", vertices=24)


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
    print(f"kitchen_pendant: {COUNTER} pieces")
    print("kitchen_pendant: EXIT 0")


if __name__ == "__main__":
    main()
