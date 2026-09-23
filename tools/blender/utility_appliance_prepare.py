#!/usr/bin/env python3
"""Prepare one closed, static utility appliance and add a box collision proxy.

The pinned upstream models contain optional rigid open/close clips. House Simulator uses them only
as static dressing, so this tool bakes their closed rest pose, removes the animation hierarchy,
adds UV0 for the project's canonical material path and appends one enclosing ``_COL`` box.

Run with: blender --background --python tools/blender/utility_appliance_prepare.py -- \
    --source PATH --out PATH --collision-name NAME
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


def bake_world_vertices(obj) -> None:
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    obj.parent = None
    for vertex in obj.data.vertices:
        vertex.co = transform @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()


def metre_uv(obj, tile_metres: float = 0.25) -> None:
    """Add deterministic box-projected UV0 without changing the source geometry."""
    uv = obj.data.uv_layers.active or obj.data.uv_layers.new(name="UV0")
    for polygon in obj.data.polygons:
        axis = max(range(3), key=lambda index: abs(polygon.normal[index]))
        for loop_index in polygon.loop_indices:
            point = obj.data.vertices[obj.data.loops[loop_index].vertex_index].co
            pair = ((point.y, point.z), (point.x, point.z), (point.x, point.y))[axis]
            uv.data[loop_index].uv = (pair[0] / tile_metres, pair[1] / tile_metres)


def prepare(source: Path, output: Path, collision_name: str) -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for action in tuple(bpy.data.actions):
        bpy.data.actions.remove(action)

    bpy.ops.import_scene.gltf(filepath=str(source))
    visible = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if not visible:
        raise RuntimeError("source contains no mesh objects")

    for obj in bpy.context.scene.objects:
        obj.animation_data_clear()
    for obj in visible:
        bake_world_vertices(obj)
        metre_uv(obj)

    # Once the world transforms have been baked into mesh data, the imported empties are dead
    # hierarchy rather than useful content. Keeping only mesh nodes also prevents a future exporter
    # from accidentally reviving the upstream rigid animation clips.
    for obj in tuple(bpy.context.scene.objects):
        if obj.type != "MESH":
            bpy.data.objects.remove(obj, do_unlink=True)

    def visible_bounds() -> tuple[Vector, Vector]:
        low = Vector((float("inf"), float("inf"), float("inf")))
        high = Vector((float("-inf"), float("-inf"), float("-inf")))
        for entry in visible:
            for vertex in entry.data.vertices:
                for axis in range(3):
                    low[axis] = min(low[axis], vertex.co[axis])
                    high[axis] = max(high[axis], vertex.co[axis])
        return low, high

    low, high = visible_bounds()
    # Blender is Z-up here. Preserve the floor support at Z=0 while moving the horizontal X/Y
    # footprint centre to the origin, so later recipe rotation does not orbit the appliance.
    horizontal_offset = Vector(((low.x + high.x) * 0.5, (low.y + high.y) * 0.5, 0.0))
    for obj in visible:
        for vertex in obj.data.vertices:
            vertex.co -= horizontal_offset
        obj.data.update()
    low, high = visible_bounds()

    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(low + high) * 0.5)
    proxy = bpy.context.object
    proxy.name = collision_name
    proxy.dimensions = high - low
    bake_world_vertices(proxy)

    output.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(output), export_format="GLB", export_yup=True,
                              export_apply=True, export_materials="EXPORT",
                              export_texcoords=True, export_normals=True,
                              export_animations=False, export_cameras=False,
                              export_lights=False)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--collision-name", required=True)
    options = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    prepare(options.source.resolve(), options.out.resolve(), options.collision_name)
    print("utility_appliance_prepare: EXIT 0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
