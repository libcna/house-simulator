#!/usr/bin/env python3
"""Deterministically author the family room's low-profile ceiling fixture.

The fixture is a restrained opal-glass semi-flush dome with dark-bronze trim.  Its local origin
is the diffuser's lower optical face, so the canonical prop and light position are the same point;
the 180 mm body then meets L0's 3.30 m ceiling without a hidden transform offset.

Run with: blender --background --python tools/blender/family_ceiling_light.py -- --out PATH
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy
from mathutils import Matrix


COLOURS = {
    "FAMILY_CEILING_METAL": (0.11, 0.065, 0.035, 1.0),
    "FamilyCeilingDiffuser": (1.0, 0.84, 0.62, 1.0),
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
    """Convert repository Y-up coordinates to Blender Z-up coordinates."""
    return (point[0], -point[2], point[1])


def name_for(label: str) -> str:
    global COUNTER
    COUNTER += 1
    return f"family_ceiling_{COUNTER:03d}_{label}"


def bake_world_vertices(obj):
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    for vertex in obj.data.vertices:
        vertex.co = transform @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()


def metre_uv(obj, tile_metres: float = 0.16):
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


def cylinder(label: str, location, height: float, radius: float, finish: str,
             vertices: int = 32):
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


def author():
    # A shallow tapered opal diffuser reads as a domestic semi-flush dome while keeping the
    # source's lower optical face exactly at local Y=0. The overlapping ring hides its seam.
    bpy.ops.mesh.primitive_cone_add(vertices=40, radius1=0.185, radius2=0.150, depth=0.105,
                                    location=blender_xyz((0.0, 0.0525, 0.0)))
    diffuser = bpy.context.object
    diffuser.name = name_for("opal_diffuser")
    bake_world_vertices(diffuser)
    diffuser.data.materials.append(material("FamilyCeilingDiffuser"))
    metre_uv(diffuser)
    for polygon in diffuser.data.polygons:
        polygon.use_smooth = True

    cylinder("bronze_trim", (0.0, 0.105, 0.0), 0.026, 0.210,
             "FAMILY_CEILING_METAL", vertices=40)
    cylinder("ceiling_canopy", (0.0, 0.145, 0.0), 0.070, 0.135,
             "FAMILY_CEILING_METAL", vertices=32)
    cylinder("lower_finial", (0.0, 0.010, 0.0), 0.020, 0.018,
             "FAMILY_CEILING_METAL", vertices=20)


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
    print(f"family_ceiling_light: {COUNTER} pieces")
    print("family_ceiling_light: EXIT 0")


if __name__ == "__main__":
    main()
