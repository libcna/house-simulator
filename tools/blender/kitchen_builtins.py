#!/usr/bin/env python3
"""Deterministically author the first measured L0 kitchen joinery GLBs in Blender.

These are finished static built-ins, not interactive doors or the eventual 62-container kitchen.
The front is local -Z, support is Y=0, and UV0 repeats by metres rather than one atlas tile per
oversized countertop. Runtime source-material ids are mapped in assets.manifest.json.

Run with: blender --background --python tools/blender/kitchen_builtins.py -- --out DIR
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


MATERIAL_COLOURS = {
    "CAB_PAINT": (0.82, 0.78, 0.70, 1.0),
    "CAB_OAK": (0.55, 0.32, 0.17, 1.0),
    "CAB_STONE": (0.83, 0.82, 0.78, 1.0),
    "CAB_STEEL": (0.44, 0.47, 0.50, 1.0),
}
MATERIALS = {}
COUNTERS = {"north_run": 0, "island": 0}
CURRENT = ""


def material(name: str):
    if name not in MATERIALS:
        entry = bpy.data.materials.new(name)
        entry.diffuse_color = MATERIAL_COLOURS[name]
        entry.use_nodes = True
        entry.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = MATERIAL_COLOURS[name]
        MATERIALS[name] = entry
    return MATERIALS[name]


def metre_uv(obj, tile_metres: float):
    """Assign a physical repeat to all six cube faces, including long and horizontal ones."""
    mesh = obj.data
    uv = mesh.uv_layers.active or mesh.uv_layers.new(name="UV0")
    for face in mesh.polygons:
        axis = max(range(3), key=lambda i: abs(face.normal[i]))
        for loop_index in face.loop_indices:
            p = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            if axis == 0:
                pair = (p.y, p.z)
            elif axis == 1:
                pair = (p.x, p.z)
            else:
                pair = (p.x, p.y)
            uv.data[loop_index].uv = (pair[0] / tile_metres, pair[1] / tile_metres)


def bake_world_vertices(obj):
    """Put every POSITION at its actual support-relative metre coordinate, without nodes."""
    bpy.context.view_layer.update()
    transform = obj.matrix_world.copy()
    for vertex in obj.data.vertices:
        vertex.co = transform @ vertex.co
    obj.matrix_world = Matrix.Identity(4)
    obj.data.update()


def blender_xyz(gltf_xyz):
    """Blender is Z-up; glTF's approved convention is Y-up and -Z-forward."""
    return (gltf_xyz[0], -gltf_xyz[2], gltf_xyz[1])


def box(label, location, size, finish="CAB_PAINT", bevel=0.002):
    COUNTERS[CURRENT] += 1
    bpy.ops.mesh.primitive_cube_add(size=1, location=blender_xyz(location))
    obj = bpy.context.object
    obj.name = f"{CURRENT}_{COUNTERS[CURRENT]:03d}_{label}"
    obj.dimensions = (size[0], size[2], size[1])
    # Blender's transform_apply(location=True) recentres each mesh and does not make a reliable
    # whole-asset accessor AABB; write the actual world coordinates into vertex streams instead.
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.58 if finish == "CAB_STONE" else 0.45)
    if bevel:
        modifier = obj.modifiers.new("joinery_edge", "BEVEL")
        modifier.width = min(bevel, min(size) * 0.2)
        modifier.segments = 2
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    for polygon in obj.data.polygons:
        polygon.use_smooth = False
    return obj


def rod(label, a, b, radius=0.006, finish="CAB_STEEL", vertices=10):
    start, end = Vector(blender_xyz(a)), Vector(blender_xyz(b))
    centre = (start + end) * 0.5
    direction = end - start
    COUNTERS[CURRENT] += 1
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                        depth=direction.length, location=centre)
    obj = bpy.context.object
    obj.name = f"{CURRENT}_{COUNTERS[CURRENT]:03d}_{label}"
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    bake_world_vertices(obj)
    obj.data.materials.append(material(finish))
    metre_uv(obj, 0.45)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def pull(label, x, y, front_z, length=0.13):
    projection = 0.035
    rod(f"{label}_post_L", (x - length * 0.36, y, front_z),
        (x - length * 0.36, y, front_z - projection), 0.004)
    rod(f"{label}_post_R", (x + length * 0.36, y, front_z),
        (x + length * 0.36, y, front_z - projection), 0.004)
    rod(f"{label}_grip", (x - length * 0.5, y, front_z - projection),
        (x + length * 0.5, y, front_z - projection), 0.0065)


def shaker(label, x, z, width, bottom, top, pull_y):
    """Real recessed 55 mm five-piece front with a 12 mm shadow reveal, not a flat cuboid."""
    height = top - bottom
    mid = (bottom + top) * 0.5
    box(f"{label}_inset", (x, mid, z + 0.009), (width - 0.02, height - 0.02, 0.012),
        bevel=0.001)
    rail = 0.052
    box(f"{label}_top_rail", (x, top - rail * 0.5, z - 0.003),
        (width, rail, 0.019), bevel=0.0025)
    box(f"{label}_lower_rail", (x, bottom + rail * 0.5, z - 0.003),
        (width, rail, 0.019), bevel=0.0025)
    for side in (-1, 1):
        box(f"{label}_{'L' if side < 0 else 'R'}_stile",
            (x + side * (width - rail) * 0.5, mid, z - 0.003),
            (rail, height - 2 * rail, 0.019), bevel=0.0025)
    pull(label, x, pull_y, z - 0.014)


def toe_and_carcass(label, width, depth):
    box(f"{label}_toe", (0, 0.061, 0.045), (width - 0.07, 0.122, depth - 0.09),
        finish="CAB_OAK")
    box(f"{label}_carcass", (0, 0.50, 0), (width, 0.756, depth), bevel=0.003)
    box(f"{label}_top_support", (0, 0.873, 0), (width, 0.018, depth), bevel=0.001)
    # Both side panels have a separate small bevel; the oak recessed toe remains visible.
    # An earlier 0.54 m end return projected past the front and looked like a loose floor plank.
    for side in (-1, 1):
        box(f"{label}_end_{side}",
            (side * (width * 0.5 - 0.010), 0.5, 0),
            (0.021, 0.766, depth), bevel=0.002)


def north_run():
    global CURRENT
    CURRENT = "north_run"
    # 2.80 m long × 0.64 m deep, four 700 mm modules; top is 0.940 m above floor.
    width, depth = 2.8, 0.64
    toe_and_carcass("run", width, depth)
    front = -depth * 0.5 - 0.013
    for index in range(4):
        centre = -1.05 + index * 0.70
        box(f"run_mullion_{index}", (centre - 0.345, 0.51, front + 0.012),
            (0.008, 0.748, 0.016), bevel=0.001)
        if index in (2, 3):
            for drawer in range(3):
                bottom = 0.134 + drawer * 0.246
                shaker(f"run_{index}_drawer_{drawer}", centre, front,
                       0.665, bottom, bottom + 0.236, bottom + 0.18)
        else:
            shaker(f"run_{index}_door", centre, front, 0.665, 0.134, 0.864, 0.74)
    # Four actual countertop slabs around a 560 × 440 mm basin opening. The stone cutout is
    # not a painted rectangle sitting above an uninterrupted surface.
    # Sink is on the opening-side of the north wall, near STACK-E's x≈-4 service route after
    # the 180° placement turn, with two cabinet doors (not a drawer bank) beneath its basin.
    opening = (-1.03, -0.47, -0.22, 0.22)
    slabs = [
        (-1.43, opening[0], -0.35, 0.35, "left"),
        (opening[1], 1.43, -0.35, 0.35, "right"),
        (opening[0], opening[1], -0.35, opening[2], "front"),
        (opening[0], opening[1], opening[3], 0.35, "back"),
    ]
    for x0, x1, z0, z1, part in slabs:
        box(f"run_stone_{part}", ((x0 + x1) * 0.5, 0.918, (z0 + z1) * 0.5),
            (x1 - x0, 0.044, z1 - z0), finish="CAB_STONE", bevel=0.003)
    sink_x = (opening[0] + opening[1]) * 0.5
    box("sink_bottom", (sink_x, 0.761, 0), (0.505, 0.016, 0.375),
        finish="CAB_STEEL", bevel=0.012)
    for sign in (-1, 1):
        box(f"sink_side_{sign}", (sink_x + sign * 0.257, 0.844, 0),
            (0.018, 0.164, 0.39), finish="CAB_STEEL", bevel=0.004)
        box(f"sink_end_{sign}", (sink_x, 0.844, sign * 0.197),
            (0.528, 0.164, 0.016), finish="CAB_STEEL", bevel=0.004)
        box(f"sink_lip_long_{sign}", (sink_x, 0.937, sign * 0.224),
            (0.584, 0.008, 0.018), finish="CAB_STEEL", bevel=0.003)
        box(f"sink_lip_short_{sign}", (sink_x + sign * 0.287, 0.937, 0),
            (0.018, 0.008, 0.438), finish="CAB_STEEL", bevel=0.003)
    # A squared gooseneck and spout; the wide basin and task fixture are recognizable at range.
    faucet_x = sink_x
    rod("tap_stem", (faucet_x, 0.94, 0.28), (faucet_x, 1.16, 0.28), 0.012, vertices=16)
    rod("tap_elbow", (faucet_x, 1.16, 0.28), (faucet_x, 1.24, 0.19), 0.012, vertices=16)
    rod("tap_spout", (faucet_x, 1.24, 0.19), (faucet_x, 1.24, -0.06), 0.012,
        vertices=16)
    rod("tap_nozzle", (faucet_x, 1.24, -0.06), (faucet_x, 1.18, -0.06), 0.013,
        vertices=16)
    rod("tap_lever", (faucet_x + 0.036, 0.97, 0.26),
        (faucet_x + 0.11, 1.01, 0.26), 0.007)


def island():
    global CURRENT
    CURRENT = "island"
    # 2.2 × 0.96 m island, 1.06 m from the north run and at least 1.2 m from hall access.
    width, depth = 2.2, 0.96
    toe_and_carcass("island", width, depth)
    for front_side in (-1, 1):
        face_z = front_side * (depth * 0.5 + 0.013)
        # Five-piece panel/drawer fronts on both use sides, not a seamless monolith.
        for index in range(3):
            centre = -0.73 + index * 0.73
            if index == 1:
                for drawer in range(3):
                    bottom = 0.134 + drawer * 0.246
                    if front_side < 0:
                        shaker(f"island_{front_side}_{index}_drawer_{drawer}", centre,
                               face_z, 0.685, bottom, bottom + 0.236, bottom + 0.18)
                    else:
                        # Rear-side reveals and oak shelves remain visible from the sunroom;
                        # handles are on the perimeter but do not protrude through the island.
                        box(f"island_rear_{index}_{drawer}",
                            (centre, bottom + 0.118, face_z),
                            (0.685, 0.224, 0.014), finish="CAB_OAK", bevel=0.003)
            elif front_side < 0:
                shaker(f"island_{front_side}_{index}_door", centre, face_z, 0.685,
                       0.134, 0.864, 0.74)
            else:
                box(f"island_rear_{index}_panel", (centre, 0.5, face_z),
                    (0.685, 0.73, 0.014), finish="CAB_OAK", bevel=0.003)
    box("island_stone_top", (0, 0.918, 0),
        (2.32, 0.044, 1.08), finish="CAB_STONE", bevel=0.005)
    # The exposed ends have oak pilaster returns so the mass reads as crafted joinery.
    for sign in (-1, 1):
        for z in (-0.39, 0.39):
            box(f"island_oak_end_{sign}_{z}",
                (sign * 1.076, 0.51, z), (0.018, 0.754, 0.042),
                finish="CAB_OAK", bevel=0.002)


def export(path: Path, author):
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    author()
    depths = [-vertex.co.y for obj in bpy.context.scene.objects if obj.type == "MESH"
              for vertex in obj.data.vertices]
    centre_z = (min(depths) + max(depths)) * 0.5
    # Source origin is the support-centre of the *whole* joinery, including front handles and
    # tap. This avoids a correct-looking position with a 10 cm off-centre collision rotation.
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH":
            for vertex in obj.data.vertices:
                vertex.co.y += centre_z
            obj.data.update()
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(path), export_format="GLB",
                              export_yup=True, export_apply=True,
                              export_materials="EXPORT", export_lights=False,
                              export_cameras=False)
    print(f"KITCHEN_BUILTIN {path.name} {COUNTERS[CURRENT]} objects")


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(args)
    export(options.out / "north_run_raw.glb", north_run)
    export(options.out / "island_raw.glb", island)
    # Debian Blender exits 0 after a Python exception; callers require this terminal marker.
    print("kitchen_builtins: EXIT 0")


if __name__ == "__main__":
    main()
