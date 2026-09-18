#!/usr/bin/env python3
"""Deterministically author HOUSE-01065's two complementary hall-gallery clusters.

The nine frames use cna-house.md §20.4 route 2: deliberately abstracted family imagery made from
original low relief, with no identifiable person and no AI-generated likeness. Local +Z is the
wall plane and -Z faces the viewer. Every visible primitive carries metre-scaled UV0.

Run with: blender --background --python tools/blender/hall_gallery.py --
          --asset west|east --out PATH
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix


COLOURS = {
    "GALLERY_FRAME": (0.25, 0.11, 0.05, 1.0),
    "GALLERY_MAT": (0.84, 0.76, 0.62, 1.0),
    "GALLERY_IMAGE": (0.22, 0.40, 0.47, 1.0),
    "GALLERY_RELIEF": (0.62, 0.42, 0.16, 1.0),
}
MATERIALS = {}
COUNTER = 0
PREFIX = "hall_gallery"


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
    """Convert repository glTF Y-up coordinates to Blender Z-up coordinates."""
    return (point[0], -point[2], point[1])


def bake_world_vertices(obj):
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    for vertex in obj.data.vertices:
        vertex.co = transform @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()


def metre_uv(obj, tile_metres: float = 0.20):
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


def box(label, location, size, finish, bevel=0.004, rotate_deg=0.0,
        bevel_segments=2):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.dimensions = (size[0], size[2], size[1])
    obj.rotation_euler.z = math.radians(-rotate_deg)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj)
    if bevel:
        modifier = obj.modifiers.new("crafted_edge", "BEVEL")
        modifier.width = min(bevel, min(size) * 0.24)
        modifier.segments = bevel_segments
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    for polygon in obj.data.polygons:
        polygon.use_smooth = False
    return obj


def portrait(index: int, centre, size, figures: int, lean: float = 0.0):
    """One physically layered frame with anonymous, painterly human suggestions."""
    x, y = centre
    width, height = size
    box(f"portrait_{index}_back", (x, y, -0.020), (width, height, 0.040),
        "GALLERY_FRAME", bevel=0.010)
    box(f"portrait_{index}_mat", (x, y, -0.047),
        (width - 0.085, height - 0.085, 0.018), "GALLERY_MAT", bevel=0.003)
    box(f"portrait_{index}_image", (x, y, -0.061),
        (width - 0.145, height - 0.145, 0.012), "GALLERY_IMAGE", bevel=0.003)

    # Heads and shoulders are intentionally geometric and distant: they imply the residents
    # without creating or borrowing a person's likeness. Vary every frame's grouping.
    image_width = width - 0.145
    spacing = min(0.16, image_width / (figures + 0.5))
    start = x - spacing * (figures - 1) * 0.5
    base_y = y - height * 0.13
    for figure in range(figures):
        offset = (figure - (figures - 1) * 0.5)
        figure_x = start + figure * spacing + lean * offset
        figure_y = base_y + (0.025 if (index + figure) % 2 else -0.015)
        head = min(0.058, width * 0.085)
        box(f"portrait_{index}_figure_head_{figure}",
            (figure_x, figure_y + height * 0.17, -0.073),
            (head, head, 0.010), "GALLERY_RELIEF", bevel=head * 0.46,
            rotate_deg=(figure - 1) * 4.0, bevel_segments=4)
        box(f"portrait_{index}_figure_shoulder_{figure}",
            (figure_x, figure_y + height * 0.075, -0.073),
            (head * 1.65, height * 0.13, 0.010), "GALLERY_RELIEF",
            bevel=head * 0.35, rotate_deg=offset * 3.0, bevel_segments=4)

    # A short horizon stroke gives each abstract photograph a room/landscape context.
    horizon_width = image_width * (0.46 + 0.04 * (index % 3))
    box(f"portrait_{index}_horizon", (x + lean * 0.03, y - height * 0.26, -0.073),
        (horizon_width, 0.018, 0.010), "GALLERY_RELIEF", bevel=0.008,
        rotate_deg=lean * 8.0, bevel_segments=3)


def centre_visible_geometry_on_x():
    points = [vertex.co.x for obj in bpy.context.scene.objects if obj.type == "MESH"
              for vertex in obj.data.vertices]
    centre = (min(points) + max(points)) * 0.5
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH":
            for vertex in obj.data.vertices:
                vertex.co.x -= centre
            obj.data.update()


def author_west():
    """Five-frame salon cluster, deliberately irregular but visually balanced."""
    entries = (
        ((-0.70, 0.34), (0.50, 0.68), 2, -0.4),
        ((-0.05, 0.39), (0.60, 0.82), 3, 0.3),
        ((0.68, 0.30), (0.50, 0.64), 1, -0.2),
        ((-0.43, -0.42), (0.66, 0.54), 2, 0.2),
        ((0.43, -0.43), (0.72, 0.56), 3, -0.3),
    )
    for index, (centre, size, figures, lean) in enumerate(entries, 1):
        portrait(index, centre, size, figures, lean)


def author_east():
    """Four different frames answering the opposite wall without mirrored repetition."""
    entries = (
        ((-0.52, 0.33), (0.70, 0.68), 3, 0.2),
        ((0.42, 0.39), (0.56, 0.80), 2, -0.3),
        ((-0.48, -0.41), (0.54, 0.56), 1, 0.4),
        ((0.40, -0.42), (0.72, 0.58), 2, -0.2),
    )
    for index, (centre, size, figures, lean) in enumerate(entries, 6):
        portrait(index, centre, size, figures, lean)


def main():
    global PREFIX
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset", choices=("west", "east"), required=True)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(arguments)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    PREFIX = f"hall_gallery_{options.asset}"
    {"west": author_west, "east": author_east}[options.asset]()
    centre_visible_geometry_on_x()
    options.out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(options.out), export_format="GLB",
                              export_yup=True, export_apply=True,
                              export_materials="EXPORT", export_lights=False,
                              export_cameras=False)
    print(f"hall_gallery: {options.asset} {COUNTER} visible pieces")
    print("hall_gallery: EXIT 0")


if __name__ == "__main__":
    main()
