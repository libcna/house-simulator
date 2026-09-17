#!/usr/bin/env python3
"""Deterministically author HOUSE-01049's kitchen seating and worktop dressing.

The four reusable assets give the fitted kitchen human scale without procedural litter: one
upholstered counter stool, a composed cutting-board/fruit-bowl focal object, three ceramic
canisters and one recognisable stove-top kettle. Coordinates follow the repository's Y-up,
-Z-forward glTF convention and every asset is support-grounded for canonical prop placement.

Run with: blender --background --python tools/blender/kitchen_dressing.py --
          --asset stool|island|canisters|kettle --out PATH
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


COLOURS = {
    "KITCHEN_WOOD": (0.18, 0.075, 0.030, 1.0),
    "KITCHEN_FABRIC": (0.30, 0.40, 0.24, 1.0),
    "KITCHEN_PRODUCE": (0.92, 0.68, 0.08, 1.0),
    "CAB_PAINT": (0.84, 0.80, 0.70, 1.0),
    "CAB_STEEL": (0.32, 0.35, 0.37, 1.0),
    "KITCHEN_BLACK": (0.025, 0.022, 0.020, 1.0),
}
MATERIALS = {}
COUNTER = 0
PREFIX = "kitchen_dressing"


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
    """glTF X/Y/Z to Blender X/Y/Z (Blender is Z-up)."""
    return (point[0], -point[2], point[1])


def bake_world_vertices(obj):
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    for vertex in obj.data.vertices:
        vertex.co = transform @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()


def metre_uv(obj, tile_metres: float = 0.22):
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


def box(label, location, size, finish="KITCHEN_WOOD", bevel=0.006,
        rotate_z_deg=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.dimensions = (size[0], size[2], size[1])
    obj.rotation_euler.z = math.radians(rotate_z_deg)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.18 if finish == "KITCHEN_FABRIC" else 0.28)
    if bevel:
        modifier = obj.modifiers.new("crafted_edge", "BEVEL")
        modifier.width = min(bevel, min(size) * 0.22)
        modifier.segments = 2
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    for polygon in obj.data.polygons:
        polygon.use_smooth = False
    return obj


def vertical_taper(label, location, height, radius_bottom, radius_top,
                   finish="KITCHEN_WOOD", vertices=12):
    bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=radius_bottom,
                                    radius2=radius_top, depth=height,
                                    location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = name_for(label)
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def vertical_cylinder(label, location, height, radius, finish="CAB_STEEL",
                      vertices=20):
    return vertical_taper(label, location, height, radius, radius, finish, vertices)


def ellipsoid(label, location, radii, finish, rotate_y_deg=0.0):
    # Build explicit triangles instead of asking Blender to triangulate UV-sphere pole n-gons.
    # Blender 4.3 legitimately chose different equal diagonals between clean processes, changing
    # the GLB index bytes and defeating source-hash reproducibility.
    segments, rings = 20, 12
    rotation = math.radians(rotate_y_deg)
    points = [blender_xyz((location[0], location[1] + radii[1], location[2]))]
    for ring in range(1, rings):
        theta = math.pi * ring / rings
        for segment in range(segments):
            phi = 2.0 * math.pi * segment / segments + rotation
            points.append(blender_xyz((
                location[0] + radii[0] * math.sin(theta) * math.cos(phi),
                location[1] + radii[1] * math.cos(theta),
                location[2] + radii[2] * math.sin(theta) * math.sin(phi),
            )))
    bottom = len(points)
    points.append(blender_xyz((location[0], location[1] - radii[1], location[2])))

    faces = []
    for segment in range(segments):
        current = 1 + segment
        following = 1 + (segment + 1) % segments
        faces.append((0, following, current))
    for ring in range(rings - 2):
        upper = 1 + ring * segments
        lower = upper + segments
        for segment in range(segments):
            following = (segment + 1) % segments
            faces.append((upper + segment, upper + following, lower + following))
            faces.append((upper + segment, lower + following, lower + segment))
    last = 1 + (rings - 2) * segments
    for segment in range(segments):
        faces.append((bottom, last + segment, last + (segment + 1) % segments))

    object_name = name_for(label)
    mesh = bpy.data.meshes.new(f"{object_name}_mesh")
    mesh.from_pydata(points, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(object_name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.16)
    for polygon in obj.data.polygons:
        polygon.use_smooth = False
    return obj


def rod(label, start, end, radius=0.012, finish="CAB_STEEL", vertices=12):
    a, b = Vector(blender_xyz(start)), Vector(blender_xyz(end))
    direction = b - a
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                        depth=direction.length,
                                        location=(a + b) * 0.5)
    obj = bpy.context.object
    obj.name = name_for(label)
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.16)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def author_stool():
    """A 650 mm seat-height counter stool with a low upholstered back."""
    box("seat_frame", (0.0, 0.585, -0.01), (0.46, 0.075, 0.46), bevel=0.012)
    box("seat_cushion", (0.0, 0.635, -0.025), (0.43, 0.065, 0.42),
        finish="KITCHEN_FABRIC", bevel=0.025)
    for x in (-0.175, 0.175):
        for z in (-0.17, 0.17):
            vertical_taper(f"leg_{x:+.2f}_{z:+.2f}", (x, 0.285, z),
                           0.57, 0.025, 0.040)
    for side in (-1, 1):
        box(f"side_stretcher_{side}", (side * 0.18, 0.225, 0.0),
            (0.035, 0.045, 0.34), bevel=0.006)
        box(f"back_post_{side}", (side * 0.185, 0.745, 0.185),
            (0.055, 0.36, 0.055), bevel=0.008)
    box("front_stretcher", (0.0, 0.225, -0.18), (0.34, 0.045, 0.035), bevel=0.006)
    box("back_stretcher", (0.0, 0.225, 0.18), (0.34, 0.045, 0.035), bevel=0.006)
    box("back_pad", (0.0, 0.80, 0.18), (0.36, 0.22, 0.060),
        finish="KITCHEN_FABRIC", bevel=0.026)
    box("back_crest", (0.0, 0.925, 0.18), (0.46, 0.060, 0.070), bevel=0.014)


def author_island():
    """A composed board, walnut fruit bowl and seven lemons for the island."""
    box("cutting_board", (-0.08, 0.020, 0.0), (0.56, 0.040, 0.36), bevel=0.018,
        rotate_z_deg=-7.0)
    vertical_cylinder("bowl_foot", (0.13, 0.045, 0.0), 0.050, 0.070,
                      finish="KITCHEN_WOOD", vertices=24)
    ellipsoid("bowl_body", (0.13, 0.105, 0.0), (0.205, 0.075, 0.195),
              "KITCHEN_WOOD")
    vertical_cylinder("bowl_rim", (0.13, 0.165, 0.0), 0.035, 0.215,
                      finish="KITCHEN_WOOD", vertices=32)
    lemon_positions = (
        (0.05, 0.215, -0.045, -18.0), (0.14, 0.225, -0.060, 22.0),
        (0.21, 0.214, -0.010, -8.0), (0.07, 0.220, 0.055, 28.0),
        (0.17, 0.228, 0.060, -25.0), (0.12, 0.274, 0.000, 10.0),
        (0.25, 0.245, 0.045, 35.0),
    )
    for index, (x, y, z, rotation) in enumerate(lemon_positions, start=1):
        ellipsoid(f"lemon_{index}", (x, y, z), (0.052, 0.042, 0.068),
                  "KITCHEN_PRODUCE", rotate_y_deg=rotation)


def author_canisters():
    """Three graduated ceramic pantry canisters with steel lid pulls."""
    specs = ((-0.19, 0.245, 0.080), (0.0, 0.205, 0.072), (0.17, 0.165, 0.064))
    for index, (x, height, radius) in enumerate(specs, start=1):
        vertical_cylinder(f"canister_{index}", (x, height * 0.5, 0.0), height,
                          radius, finish="CAB_PAINT", vertices=24)
        vertical_cylinder(f"lid_{index}", (x, height + 0.012, 0.0), 0.024,
                          radius * 1.04, finish="CAB_STEEL", vertices=24)
        vertical_cylinder(f"pull_{index}", (x, height + 0.038, 0.0), 0.030,
                          0.018, finish="CAB_STEEL", vertices=16)


def author_kettle():
    """A compact brushed-steel kettle with a black arch handle and lid grip."""
    vertical_cylinder("base", (0.0, 0.015, 0.0), 0.030, 0.115,
                      finish="KITCHEN_BLACK", vertices=28)
    ellipsoid("body", (0.0, 0.145, 0.0), (0.145, 0.135, 0.125), "CAB_STEEL")
    vertical_cylinder("lid", (0.0, 0.275, 0.0), 0.025, 0.075,
                      finish="CAB_STEEL", vertices=24)
    vertical_cylinder("lid_grip", (0.0, 0.310, 0.0), 0.045, 0.025,
                      finish="KITCHEN_BLACK", vertices=16)
    rod("spout_lower", (-0.105, 0.145, -0.005), (-0.205, 0.205, -0.005),
        radius=0.035, finish="CAB_STEEL", vertices=16)
    rod("spout_tip", (-0.205, 0.205, -0.005), (-0.285, 0.255, -0.005),
        radius=0.025, finish="CAB_STEEL", vertices=16)
    handle_points = ((0.105, 0.17, 0.0), (0.145, 0.30, 0.0),
                     (0.075, 0.405, 0.0), (-0.075, 0.405, 0.0),
                     (-0.105, 0.30, 0.0))
    for index, (start, end) in enumerate(zip(handle_points, handle_points[1:]), start=1):
        rod(f"handle_{index}", start, end, radius=0.018,
            finish="KITCHEN_BLACK", vertices=14)

    # The spout makes the assembled bounds asymmetric. Placement coordinates are pivots, not
    # approximate centres: recenter the completed visible assembly so a later yaw does not orbit
    # the kettle around a point beside it. The canonical prop position compensates this source
    # translation, preserving the reviewed world-space silhouette.
    pieces = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    low_x = min(vertex.co.x for obj in pieces for vertex in obj.data.vertices)
    high_x = max(vertex.co.x for obj in pieces for vertex in obj.data.vertices)
    centre_x = (low_x + high_x) * 0.5
    for obj in pieces:
        for vertex in obj.data.vertices:
            vertex.co.x -= centre_x
        obj.data.update()


def main():
    global PREFIX
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset", choices=("stool", "island", "canisters", "kettle"),
                        required=True)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(arguments)

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    PREFIX = f"kitchen_{options.asset}"
    {"stool": author_stool, "island": author_island,
     "canisters": author_canisters, "kettle": author_kettle}[options.asset]()
    options.out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(options.out), export_format="GLB",
                              export_yup=True, export_apply=True,
                              export_materials="EXPORT", export_lights=False,
                              export_cameras=False)
    print(f"kitchen_dressing: {options.asset} {COUNTER} pieces")
    print("kitchen_dressing: EXIT 0")


if __name__ == "__main__":
    main()
