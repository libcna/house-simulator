#!/usr/bin/env python3
"""Author one low-cost wall-art frame for HOUSE-00982's bounded CC0 image set."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy
from mathutils import Matrix


COLOURS = {
    "ART_FRAME_DARK": (0.08, 0.06, 0.05, 1.0),
    "ART_FRAME_BRASS": (0.42, 0.25, 0.07, 1.0),
    "ART_MAT": (0.82, 0.77, 0.67, 1.0),
    "ART_IMAGE": (0.72, 0.48, 0.38, 1.0),
}
MATERIALS = {}


def material(name: str):
    if name not in MATERIALS:
        entry = bpy.data.materials.new(name)
        colour = COLOURS["ART_IMAGE"] if name.startswith("ART_IMAGE_") else COLOURS[name]
        entry.diffuse_color = colour
        entry.use_nodes = True
        entry.node_tree.nodes.get("Principled BSDF").inputs[
            "Base Color"].default_value = colour
        MATERIALS[name] = entry
    return MATERIALS[name]


def blender_xyz(point):
    return (point[0], -point[2], point[1])


def bake(obj) -> None:
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    for vertex in obj.data.vertices:
        vertex.co = transform @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()


def box(name: str, centre, size, finish: str, bevel: float = 0.002):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(centre))
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = (size[0], size[2], size[1])
    bake(obj)
    obj.data.materials.append(material(finish))
    uv = obj.data.uv_layers.new(name="UV0")
    for polygon in obj.data.polygons:
        for loop_index in polygon.loop_indices:
            point = obj.data.vertices[obj.data.loops[loop_index].vertex_index].co
            uv.data[loop_index].uv = (point.x / 0.25, point.z / 0.25)
    if bevel:
        modifier = obj.modifiers.new("frame_edge", "BEVEL")
        modifier.width = bevel
        modifier.segments = 2
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    return obj


def image_plane(width: float, height: float, bottom: float, z: float, image_path: Path,
                image_material: str):
    vertices = [
        blender_xyz((-width * 0.5, bottom, z)),
        blender_xyz((width * 0.5, bottom, z)),
        blender_xyz((width * 0.5, bottom + height, z)),
        blender_xyz((-width * 0.5, bottom + height, z)),
    ]
    mesh = bpy.data.meshes.new("art_image_mesh")
    mesh.from_pydata(vertices, [], [(0, 1, 2, 3)])
    mesh.update()
    obj = bpy.data.objects.new("art_image", mesh)
    bpy.context.collection.objects.link(obj)
    finish = material(image_material)
    finish.use_nodes = True
    texture = finish.node_tree.nodes.new("ShaderNodeTexImage")
    texture.image = bpy.data.images.load(str(image_path))
    finish.node_tree.links.new(texture.outputs["Color"],
                               finish.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
    obj.data.materials.append(finish)
    uv = obj.data.uv_layers.new(name="UV0")
    # Blender's glTF exporter preserves this winding as a front-facing -Z picture surface.
    for loop_index, coordinate in enumerate(((0.0, 0.0), (1.0, 0.0),
                                              (1.0, 1.0), (0.0, 1.0))):
        uv.data[loop_index].uv = coordinate


def author(width: float, height: float, style: str, image_path: Path, image_material: str,
           output: Path) -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    frame = "ART_FRAME_BRASS" if style == "brass" else "ART_FRAME_DARK"
    mat = 0.055 if style == "matted" else 0.025
    rail = 0.042 if style == "brass" else 0.052
    outer_width = width + 2.0 * (mat + rail)
    outer_height = height + 2.0 * (mat + rail)

    # The wall is at glTF +Z=0; all visible depth projects toward the viewer at -Z.
    if mat:
        box("art_mat", (0.0, outer_height * 0.5, -0.018),
            (width + 2.0 * mat, height + 2.0 * mat, 0.036), "ART_MAT", 0.0)
    half_w = outer_width * 0.5
    half_h = outer_height * 0.5
    box("frame_left", (-half_w + rail * 0.5, half_h, -0.025),
        (rail, outer_height, 0.05), frame)
    box("frame_right", (half_w - rail * 0.5, half_h, -0.025),
        (rail, outer_height, 0.05), frame)
    box("frame_bottom", (0.0, rail * 0.5, -0.025),
        (outer_width - 2.0 * rail, rail, 0.05), frame)
    box("frame_top", (0.0, outer_height - rail * 0.5, -0.025),
        (outer_width - 2.0 * rail, rail, 0.05), frame)
    image_plane(width, height, rail + mat, -0.039, image_path, image_material)

    box("WallArt_COL", (0.0, outer_height * 0.5, -0.025),
        (outer_width, outer_height, 0.05), "ART_FRAME_DARK", 0.0)
    bpy.context.object.name = "WallArt_COL"

    output.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(output), export_format="GLB", export_yup=True,
                              export_apply=True, export_materials="EXPORT",
                              export_texcoords=True, export_normals=True,
                              export_animations=False, export_cameras=False,
                              export_lights=False)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=float, required=True)
    parser.add_argument("--height", type=float, required=True)
    parser.add_argument("--style", choices=("dark", "brass", "matted"), required=True)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--image-material", required=True)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    if options.width <= 0.0 or options.height <= 0.0:
        parser.error("frame dimensions must be positive")
    author(options.width, options.height, options.style, options.image.resolve(),
           options.image_material,
           options.out.resolve())
    print("wall_art_set: EXIT 0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
