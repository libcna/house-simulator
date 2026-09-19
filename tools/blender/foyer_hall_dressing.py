#!/usr/bin/env python3
"""Deterministically author HOUSE-01064/01071's foyer/hall arrival dressing.

The assets are intentionally bounded: a supported console vignette, a long hall runner, a
distinct broad foyer entry rug, and paired relief on the kitchen-portal wall flanks.
Wall-art local +Z is the wall plane and -Z faces the viewer; floor/table assets use Y=0 as their
support plane. Every visible primitive has metre-scaled UV0 and a canonical material slot.

Run with: blender --background --python tools/blender/foyer_hall_dressing.py --
          --asset console|runner|entry|art --out PATH
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


COLOURS = {
    "ARRIVAL_WOOD": (0.25, 0.11, 0.05, 1.0),
    "ARRIVAL_BRASS": (0.62, 0.42, 0.16, 1.0),
    "ARRIVAL_CANVAS": (0.22, 0.40, 0.47, 1.0),
    "ARRIVAL_CREAM": (0.84, 0.76, 0.62, 1.0),
    "ARRIVAL_CERAMIC": (0.44, 0.52, 0.48, 1.0),
    "ARRIVAL_RUNNER_BASE": (0.34, 0.22, 0.18, 1.0),
    "ARRIVAL_RUNNER_BORDER": (0.68, 0.50, 0.31, 1.0),
    "ARRIVAL_ENTRY_LIGHT": (0.78, 0.68, 0.52, 1.0),
}
MATERIALS = {}
COUNTER = 0
PREFIX = "arrival_dressing"


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


def box(label, location, size, finish, bevel=0.004, yaw_deg=0.0,
        bevel_segments=2):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.dimensions = (size[0], size[2], size[1])
    obj.rotation_euler.z = math.radians(-yaw_deg)
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


def vertical_cylinder(label, location, radius, height, finish, vertices=24,
                      scale_x=1.0, scale_z=1.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=height,
                                        location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.scale.x = scale_x
    obj.scale.y = scale_z
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def ellipsoid(label, location, radii, finish, yaw_deg=0.0, segments=24, rings=12):
    """Build a smooth ellipsoid without Blender operator-generated mesh metadata.

    Blender's UV-sphere operator emits byte-varying custom-normal data in otherwise identical
    GLBs on this supported toolchain. Explicit vertices and faces keep the authored source exactly
    reproducible while retaining a normal smooth-shaded silhouette.
    """
    yaw = math.radians(yaw_deg)
    sine_yaw, cosine_yaw = math.sin(yaw), math.cos(yaw)

    def point(x, y, z):
        rotated_x = x * cosine_yaw + z * sine_yaw
        rotated_z = -x * sine_yaw + z * cosine_yaw
        return blender_xyz((location[0] + rotated_x, location[1] + y,
                            location[2] + rotated_z))

    vertices = [point(0.0, radii[1], 0.0)]
    for ring in range(1, rings):
        latitude = math.pi * ring / rings
        for segment in range(segments):
            longitude = 2.0 * math.pi * segment / segments
            vertices.append(point(radii[0] * math.sin(latitude) * math.cos(longitude),
                                  radii[1] * math.cos(latitude),
                                  radii[2] * math.sin(latitude) * math.sin(longitude)))
    bottom = len(vertices)
    vertices.append(point(0.0, -radii[1], 0.0))

    faces = []
    for segment in range(segments):
        following = (segment + 1) % segments
        faces.append((0, 1 + segment, 1 + following))
    for ring in range(rings - 2):
        current = 1 + ring * segments
        following_ring = current + segments
        for segment in range(segments):
            following = (segment + 1) % segments
            faces.append((current + segment, following_ring + segment,
                          following_ring + following, current + following))
    last_ring = 1 + (rings - 2) * segments
    for segment in range(segments):
        following = (segment + 1) % segments
        faces.append((last_ring + following, last_ring + segment, bottom))

    object_name = name_for(label)
    mesh = bpy.data.meshes.new(f"{object_name}_mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.validate(verbose=False)
    mesh.update()
    obj = bpy.data.objects.new(object_name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def cylinder_between(label, start, end, radius, finish="ARRIVAL_BRASS", vertices=12):
    a, b = Vector(blender_xyz(start)), Vector(blender_xyz(end))
    direction = b - a
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                        depth=direction.length, location=(a + b) * 0.5)
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def centre_visible_geometry_on_x():
    points = [vertex.co.x for obj in bpy.context.scene.objects if obj.type == "MESH"
              for vertex in obj.data.vertices]
    centre = (min(points) + max(points)) * 0.5
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH":
            for vertex in obj.data.vertices:
                vertex.co.x -= centre
            obj.data.update()


def author_console():
    """A vase/branch, standing photograph and shallow key tray for the existing console."""
    # Ceramic vase: a foot, tapered body suggestion and narrow neck, all genuinely supported.
    vertical_cylinder("vase_foot", (0.27, 0.018, 0.0), 0.105, 0.036,
                      "ARRIVAL_CERAMIC", vertices=32)
    ellipsoid("vase_body", (0.27, 0.205, 0.0), (0.13, 0.185, 0.11),
              "ARRIVAL_CERAMIC", segments=32, rings=16)
    vertical_cylinder("vase_neck", (0.27, 0.395, 0.0), 0.055, 0.15,
                      "ARRIVAL_CERAMIC", vertices=24)
    for index, end in enumerate(((0.10, 0.78, 0.01), (0.30, 0.82, -0.02),
                                 (0.44, 0.73, 0.03))):
        cylinder_between(f"branch_{index}", (0.27, 0.46, 0.0), end, 0.006,
                         "ARRIVAL_BRASS")
        ellipsoid(f"branch_leaf_{index}", end, (0.055, 0.020, 0.012),
                  "ARRIVAL_BRASS", yaw_deg=(-18.0, 8.0, 22.0)[index],
                  segments=16, rings=8)

    # A small freestanding photograph: layered frame/mat/canvas, not an image card.
    box("photo_back", (-0.30, 0.18, 0.0), (0.30, 0.36, 0.035),
        "ARRIVAL_WOOD", bevel=0.012)
    box("photo_mat", (-0.30, 0.19, -0.022), (0.245, 0.295, 0.014),
        "ARRIVAL_CREAM", bevel=0.004)
    box("photo_field", (-0.30, 0.19, -0.032), (0.18, 0.225, 0.010),
        "ARRIVAL_CANVAS", bevel=0.003)
    box("photo_stand", (-0.30, 0.035, 0.075), (0.16, 0.07, 0.16),
        "ARRIVAL_BRASS", bevel=0.008)

    # A useful landing-place for keys, with a separate inset so it reads as a tray.
    vertical_cylinder("key_tray", (-0.02, 0.018, -0.09), 0.11, 0.036,
                      "ARRIVAL_BRASS", vertices=32, scale_x=1.45, scale_z=0.72)
    box("keys", (-0.02, 0.045, -0.09), (0.13, 0.018, 0.035),
        "ARRIVAL_WOOD", bevel=0.008, yaw_deg=12.0)
    # Keep the whole asymmetric vignette centred over the existing console's authored origin.
    centre_visible_geometry_on_x()


def author_runner():
    """A 3.8 m wool runner with physical border, motif and end fringe."""
    box("runner_body", (0.0, 0.007, 0.0), (1.14, 0.014, 3.68),
        "ARRIVAL_RUNNER_BASE", bevel=0.006)
    for x in (-0.505, 0.505):
        box(f"runner_long_border_{x}", (x, 0.015, 0.0), (0.075, 0.008, 3.55),
            "ARRIVAL_RUNNER_BORDER", bevel=0.003)
    for z in (-1.76, 1.76):
        box(f"runner_end_border_{z}", (0.0, 0.015, z), (1.00, 0.008, 0.08),
            "ARRIVAL_RUNNER_BORDER", bevel=0.003)
    for z in (-1.18, -0.59, 0.0, 0.59, 1.18):
        box(f"runner_diamond_{z}", (0.0, 0.016, z), (0.30, 0.009, 0.30),
            "ARRIVAL_RUNNER_BORDER", bevel=0.018, yaw_deg=45.0, bevel_segments=3)
    for end in (-1, 1):
        for index in range(11):
            x = -0.50 + index * 0.10
            box(f"runner_fringe_{end}_{index}",
                (x, 0.006, end * 1.88), (0.018, 0.012, 0.20),
                "ARRIVAL_RUNNER_BORDER", bevel=0.005)


def author_entry():
    """A broad 2.12 × 2.50 m flatwoven arrival rug, not a repeated hall runner."""
    box("entry_rug_body", (0.0, 0.006, 0.0), (2.12, 0.012, 2.50),
        "ARRIVAL_RUNNER_BASE", bevel=0.005)
    box("entry_rug_field", (0.0, 0.013, 0.0), (1.88, 0.005, 2.26),
        "ARRIVAL_ENTRY_LIGHT", bevel=0.004)
    for side in (-1, 1):
        box(f"entry_long_border_{side}", (side * 0.87, 0.018, 0.0),
            (0.055, 0.007, 2.10), "ARRIVAL_RUNNER_BORDER", bevel=0.003)
        box(f"entry_end_border_{side}", (0.0, 0.018, side * 1.055),
            (1.74, 0.007, 0.055), "ARRIVAL_RUNNER_BORDER", bevel=0.003)
    # A centred medallion and four corner marks distinguish the foyer's broader
    # domestic composition from the hall's repeated five-diamond cadence.
    box("entry_centre_medallion", (0.0, 0.020, 0.0), (0.62, 0.009, 0.62),
        "ARRIVAL_RUNNER_BASE", bevel=0.023, yaw_deg=45, bevel_segments=3)
    box("entry_centre_inlay", (0.0, 0.025, 0.0), (0.27, 0.005, 0.27),
        "ARRIVAL_RUNNER_BORDER", bevel=0.012, yaw_deg=45, bevel_segments=3)
    for x in (-0.61, 0.61):
        for z in (-0.80, 0.80):
            box(f"entry_corner_mark_{x}_{z}", (x, 0.019, z),
                (0.18, 0.007, 0.18), "ARRIVAL_RUNNER_BASE",
                bevel=0.009, yaw_deg=45, bevel_segments=3)


def one_relief(side: int):
    centre = side * 1.58
    box(f"relief_back_{side}", (centre, 0.0, -0.025), (0.76, 1.05, 0.050),
        "ARRIVAL_WOOD", bevel=0.010)
    box(f"relief_mat_{side}", (centre, 0.0, -0.058), (0.65, 0.94, 0.018),
        "ARRIVAL_CREAM", bevel=0.004)
    box(f"relief_canvas_{side}", (centre, 0.0, -0.071), (0.54, 0.82, 0.012),
        "ARRIVAL_CANVAS", bevel=0.004)
    # Original raised landscape lines give each flank asymmetry while retaining a matched pair.
    horizon = -0.08 if side < 0 else 0.04
    cylinder_between(f"relief_horizon_{side}",
                     (centre - 0.21, horizon, -0.086),
                     (centre + 0.21, horizon + 0.03 * side, -0.086),
                     0.009, "ARRIVAL_BRASS")
    cylinder_between(f"relief_slope_{side}",
                     (centre - 0.20 * side, -0.26, -0.086),
                     (centre + 0.18 * side, 0.24, -0.086),
                     0.009, "ARRIVAL_BRASS")
    for index, y in enumerate((-0.20, 0.08, 0.27)):
        x = centre + side * (0.14 - index * 0.07)
        box(f"relief_mark_{side}_{index}", (x, y, -0.091),
            (0.12 + 0.02 * index, 0.045, 0.012), "ARRIVAL_BRASS",
            bevel=0.012, yaw_deg=side * (12.0 - index * 7.0), bevel_segments=4)


def author_art():
    for side in (-1, 1):
        one_relief(side)


def main():
    global PREFIX
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset", choices=("console", "runner", "entry", "art"), required=True)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(arguments)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    PREFIX = f"arrival_dressing_{options.asset}"
    {"console": author_console, "runner": author_runner,
     "entry": author_entry, "art": author_art}[options.asset]()
    options.out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(options.out), export_format="GLB",
                              export_yup=True, export_apply=True,
                              export_materials="EXPORT", export_lights=False,
                              export_cameras=False)
    print(f"foyer_hall_dressing: {options.asset} {COUNTER} visible pieces")
    print("foyer_hall_dressing: EXIT 0")


if __name__ == "__main__":
    main()
