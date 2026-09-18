#!/usr/bin/env python3
"""Deterministically author HOUSE-01067's family-room picture-window treatment.

The model is a pair of open, full-length gathered curtains on one measured rod.  Local Z=0 is
the wall plane; all visible geometry projects toward local -Z, and local Y=0 is finished-floor
height.  The central 2.06 m stays clear, so this is a static soft furnishing rather than a fake
closed blind or a substitute for phase 45's later ``blindFraction`` behaviour.

Run with: blender --background --python tools/blender/family_window_treatment.py -- --out PATH
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


FABRIC = "CURTAIN_FABRIC"
HARDWARE = "CURTAIN_HARDWARE"


def material(name: str, colour: tuple[float, float, float, float]):
    entry = bpy.data.materials.new(name)
    entry.diffuse_color = colour
    entry.use_nodes = True
    entry.node_tree.nodes.get("Principled BSDF").inputs[
        "Base Color"].default_value = colour
    return entry


def blender_xyz(point: tuple[float, float, float]):
    """Repository glTF Y-up coordinates as Blender Z-up coordinates."""
    return (point[0], -point[2], point[1])


def bake_world_vertices(obj) -> None:
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    for vertex in obj.data.vertices:
        vertex.co = transform @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()


def metre_uv(obj, tile_metres: float) -> None:
    mesh = obj.data
    uv = mesh.uv_layers.active or mesh.uv_layers.new(name="UV0")
    for polygon in mesh.polygons:
        axis = max(range(3), key=lambda index: abs(polygon.normal[index]))
        for loop_index in polygon.loop_indices:
            point = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            if axis == 0:
                pair = (point.y, point.z)
            elif axis == 1:
                pair = (point.x, point.z)
            else:
                pair = (point.x, point.y)
            uv.data[loop_index].uv = (pair[0] / tile_metres, pair[1] / tile_metres)


def box(name: str, centre: tuple[float, float, float], size: tuple[float, float, float],
        finish, bevel: float = 0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=blender_xyz(centre))
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = (size[0], size[2], size[1])
    bake_world_vertices(obj)
    obj.data.materials.append(finish)
    metre_uv(obj, 0.18 if finish.name == FABRIC else 0.12)
    if bevel > 0.0:
        modifier = obj.modifiers.new("soft_edge", "BEVEL")
        modifier.width = bevel
        modifier.segments = 3
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    return obj


def cylinder_between(name: str, start: tuple[float, float, float],
                     end: tuple[float, float, float], radius: float, finish,
                     vertices: int = 16):
    a = Vector(blender_xyz(start))
    b = Vector(blender_xyz(end))
    direction = b - a
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=direction.length,
                                        location=(a + b) * 0.5)
    obj = bpy.context.object
    obj.name = name
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    bake_world_vertices(obj)
    obj.data.materials.append(finish)
    metre_uv(obj, 0.12)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def holdback_button(name: str, centre: tuple[float, float, float], finish) -> None:
    """Create a deterministic flattened fabric rosette without Blender operator ordering."""
    segments = 12
    rings = 6
    radius_x = 0.014
    radius_y = 0.014
    radius_z = 0.006
    vertices = [blender_xyz((centre[0], centre[1] - radius_y, centre[2]))]
    uvs = [(0.5, 0.0)]
    for ring in range(1, rings):
        phi = -math.pi * 0.5 + math.pi * ring / rings
        for segment in range(segments):
            theta = math.tau * segment / segments
            vertices.append(blender_xyz((
                centre[0] + radius_x * math.cos(phi) * math.cos(theta),
                centre[1] + radius_y * math.sin(phi),
                centre[2] + radius_z * math.cos(phi) * math.sin(theta),
            )))
            uvs.append((segment / segments, ring / rings))
    top = len(vertices)
    vertices.append(blender_xyz((centre[0], centre[1] + radius_y, centre[2])))
    uvs.append((0.5, 1.0))

    faces = []
    first_ring = 1
    for segment in range(segments):
        next_segment = (segment + 1) % segments
        faces.append((0, first_ring + segment, first_ring + next_segment))
    for ring in range(rings - 2):
        lower = first_ring + ring * segments
        upper = lower + segments
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            faces.append((lower + segment, upper + segment,
                          upper + next_segment, lower + next_segment))
    last_ring = first_ring + (rings - 2) * segments
    for segment in range(segments):
        next_segment = (segment + 1) % segments
        faces.append((last_ring + segment, top, last_ring + next_segment))

    mesh = bpy.data.meshes.new(f"{name}_mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(finish)
    uv = mesh.uv_layers.new(name="UV0")
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            uv.data[loop_index].uv = uvs[mesh.loops[loop_index].vertex_index]
        polygon.use_smooth = True
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)


def curtain_panel(side: int, finish) -> None:
    """One tailored panel, wide at header/hem and gathered into its real tieback."""
    columns = 17
    rows = 25
    centre_x = side * 1.18
    vertices = []
    uvs = []
    faces = []
    for row in range(rows):
        v = row / (rows - 1)
        y = 0.10 + v * 2.35
        # A human-height tieback gathers the fabric without turning the lower drape into a cone.
        gather = math.exp(-((y - 1.17) / 0.38) ** 2)
        half_width = 0.310 - gather * 0.140
        for column in range(columns):
            u = column / (columns - 1)
            across = (u - 0.5) * 2.0
            x = centre_x + across * half_width
            # Seven soft folds, deepest at the free header and hem and restrained at the tieback.
            fold = math.sin(u * math.tau * 7.0 + v * 0.48)
            z = -0.095 - fold * (0.055 - gather * 0.024)
            # A small relaxed hem wave avoids the CAD-straight bottom edge.
            if row == 0:
                y += 0.012 * (0.5 + 0.5 * math.cos(u * math.tau * 2.0))
            vertices.append(blender_xyz((x, y, z)))
            uvs.append((u * 2.35, v * 11.75))
    for row in range(rows - 1):
        for column in range(columns - 1):
            a = row * columns + column
            b = a + 1
            c = (row + 1) * columns + column + 1
            d = c - 1
            # Seen from the room, local -Z: wind the primary surface toward the viewer. The
            # opposite order remains visible with a two-sided runtime material but carries an
            # outward normal, leaving a pale back-lit curtain almost black in BasicEffect.
            faces.append((a, d, c, b))

    label = "left" if side < 0 else "right"
    mesh = bpy.data.meshes.new(f"family_curtain_{label}_mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(finish)
    uv = mesh.uv_layers.new(name="UV0")
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            uv.data[loop_index].uv = uvs[mesh.loops[loop_index].vertex_index]
        polygon.use_smooth = True
    obj = bpy.data.objects.new(f"family_curtain_{label}_panel", mesh)
    bpy.context.collection.objects.link(obj)
    solidify = obj.modifiers.new("woven_thickness", "SOLIDIFY")
    solidify.thickness = 0.008
    solidify.offset = 0.0
    solidify.use_rim = True
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=solidify.name)

    # A small fabric-covered holdback button marks the gathered waist.  Its cord is concealed
    # behind the panel: a straight visible strap projected as a metre-long stick when the north
    # treatment was viewed nearly edge-on from the fixed family-room camera.
    holdback_button(f"family_curtain_{label}_tieback", (centre_x, 1.17, -0.145), finish)
    # Five short fabric tabs visibly carry each gathered header from the rod.
    for index in range(5):
        x = centre_x - 0.18 + index * 0.09
        box(f"family_curtain_{label}_tab_{index:02d}", (x, 2.49, -0.078),
            (0.035, 0.10, 0.025), finish, bevel=0.006)


def author() -> None:
    fabric = material(FABRIC, (0.90, 0.82, 0.70, 1.0))
    hardware = material(HARDWARE, (0.16, 0.14, 0.12, 1.0))
    curtain_panel(-1, fabric)
    curtain_panel(1, fabric)

    # A 3.04 m rod gives each 2.40 m opening 320 mm of stack-back at both jambs.
    cylinder_between("family_curtain_rod", (-1.46, 2.53, -0.07),
                     (1.46, 2.53, -0.07), 0.018, hardware, vertices=20)
    for side in (-1, 1):
        cylinder_between(f"family_curtain_finial_{side}",
                         (side * 1.46, 2.53, -0.07),
                         (side * 1.52, 2.53, -0.07), 0.034, hardware, vertices=16)
        # The bracket back reaches Z=0 exactly: canonical prop positions are the real wall planes.
        box(f"family_curtain_bracket_{side}", (side * 1.30, 2.53, -0.035),
            (0.055, 0.09, 0.07), hardware, bevel=0.008)


def main() -> None:
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
    print("family_window_treatment: two gathered panels, rod, brackets, tabs and tiebacks")
    print("family_window_treatment: EXIT 0")


if __name__ == "__main__":
    main()
