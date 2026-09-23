#!/usr/bin/env python3
"""Generate one compact, seeded shelf/surface fill kit for HOUSE-00973.

The four kinds are intentionally broad: books, folded textiles, crockery/jars and tools/paint
tins.  They are support-relative Y-up GLBs, suitable for repeated static placement on open
shelves, worktops and storage furniture.  A seed changes count, spacing, lean and palette choices;
the same seed is reproducible.  Containers and their interiors are deliberately not modelled.
"""

from __future__ import annotations

import argparse
import math
import random
import sys
from pathlib import Path

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kitchen_builtins as kit  # noqa: E402


KINDS = ("books", "textiles", "crockery", "tools")
COLOURS = {
    "FILL_CREAM": (0.79, 0.73, 0.62, 1.0),
    "FILL_BLUE": (0.20, 0.34, 0.48, 1.0),
    "FILL_GREEN": (0.27, 0.43, 0.32, 1.0),
    "FILL_OCHRE": (0.62, 0.40, 0.16, 1.0),
    "FILL_RED": (0.52, 0.18, 0.15, 1.0),
    "FILL_STEEL": (0.42, 0.45, 0.47, 1.0),
    "FILL_WOOD": (0.38, 0.21, 0.11, 1.0),
    "FILL_GLASS": (0.57, 0.69, 0.68, 1.0),
}
SOFT = ("FILL_CREAM", "FILL_BLUE", "FILL_GREEN", "FILL_OCHRE", "FILL_RED")


def box(label: str, location, size, finish: str, *, lean_deg: float = 0.0,
        yaw_deg: float = 0.0, bevel: float = 0.002) -> None:
    """A metre-scale cuboid whose requested glTF-Y base is preserved through its lean."""
    kit.COUNTERS[kit.CURRENT] += 1
    bpy.ops.mesh.primitive_cube_add(size=1, location=kit.blender_xyz(location))
    obj = bpy.context.object
    obj.name = f"{kit.CURRENT}_{kit.COUNTERS[kit.CURRENT]:03d}_{label}"
    obj.dimensions = (size[0], size[2], size[1])
    # glTF +Y is Blender +Z.  A glTF-Z book lean is therefore a Blender-Y rotation, while a
    # support-plane yaw is Blender Z.  The signs merely choose a convention; variation is bounded.
    obj.rotation_euler[1] = math.radians(-lean_deg)
    obj.rotation_euler[2] = math.radians(yaw_deg)
    bpy.context.view_layer.update()
    wanted_base = float(location[1]) - size[1] * 0.5
    actual_base = min((obj.matrix_world @ Vector(corner)).z for corner in obj.bound_box)
    obj.location.z += wanted_base - actual_base
    kit.bake_world_vertices(obj)
    obj.data.materials.append(kit.material(finish))
    kit.metre_uv(obj, 0.22)
    if bevel:
        modifier = obj.modifiers.new("soft_edge", "BEVEL")
        modifier.width = min(bevel, min(size) * 0.18)
        modifier.segments = 1
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    for polygon in obj.data.polygons:
        polygon.use_smooth = False


def cylinder(label: str, location, radius: float, height: float, finish: str,
             *, vertices: int = 16) -> None:
    kit.COUNTERS[kit.CURRENT] += 1
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=height,
                                        location=kit.blender_xyz(location))
    obj = bpy.context.object
    obj.name = f"{kit.CURRENT}_{kit.COUNTERS[kit.CURRENT]:03d}_{label}"
    kit.bake_world_vertices(obj)
    obj.data.materials.append(kit.material(finish))
    kit.metre_uv(obj, 0.20)


def books(rng: random.Random) -> None:
    count = rng.randint(6, 11)
    widths = [rng.uniform(0.026, 0.054) for _ in range(count)]
    gaps = [rng.uniform(0.004, 0.014) for _ in range(count - 1)]
    x = -(sum(widths) + sum(gaps)) * 0.5
    for index, width in enumerate(widths):
        height = rng.uniform(0.18, 0.31)
        depth = rng.uniform(0.14, 0.21)
        lean = rng.uniform(-9.0, 9.0) if index not in (0, count - 1) else 0.0
        box(f"book_{index}", (x + width * 0.5, height * 0.5, rng.uniform(-0.012, 0.012)),
            (width, height, depth), rng.choice(SOFT), lean_deg=lean, bevel=0.0015)
        x += width + (gaps[index] if index < len(gaps) else 0.0)
    # A small horizontal pair prevents every seed reading as one picket fence.
    if rng.random() < 0.75:
        width = rng.uniform(0.18, 0.27)
        stack_height = 0.0
        for layer in range(rng.randint(1, 2)):
            height = rng.uniform(0.025, 0.04)
            box(f"book_flat_{layer}", (0.0, stack_height + height * 0.5,
                -0.02 - rng.uniform(0.08, 0.13)), (width, height, rng.uniform(0.13, 0.18)),
                rng.choice(SOFT), yaw_deg=rng.uniform(-4.0, 4.0), bevel=0.0015)
            stack_height += height + 0.003


def textiles(rng: random.Random) -> None:
    height = 0.0
    for index in range(rng.randint(3, 6)):
        layer = rng.uniform(0.038, 0.064)
        width = rng.uniform(0.32, 0.49)
        depth = rng.uniform(0.24, 0.35)
        box(f"fold_{index}", (rng.uniform(-0.025, 0.025), height + layer * 0.5,
            rng.uniform(-0.018, 0.018)), (width, layer, depth), rng.choice(SOFT),
            yaw_deg=rng.uniform(-5.0, 5.0), bevel=min(0.012, layer * 0.22))
        # A visible air line between folds avoids a single coloured block.
        height += layer + rng.uniform(0.004, 0.010)


def crockery(rng: random.Random) -> None:
    stacks = rng.randint(2, 4)
    centres = [(-0.22 + 0.44 * index / max(1, stacks - 1)) for index in range(stacks)]
    for stack, x in enumerate(centres):
        radius = rng.uniform(0.075, 0.115)
        levels = rng.randint(2, 5)
        for level in range(levels):
            thickness = rng.uniform(0.010, 0.016)
            cylinder(f"plate_{stack}_{level}", (x, thickness * 0.5 + level * 0.018,
                     rng.uniform(-0.025, 0.015)), radius, thickness,
                     rng.choice(("FILL_CREAM", "FILL_BLUE", "FILL_GREEN")), vertices=18)
    for index in range(rng.randint(1, 3)):
        radius = rng.uniform(0.045, 0.071)
        height = rng.uniform(0.13, 0.24)
        x = rng.uniform(-0.23, 0.23)
        z = rng.uniform(-0.13, -0.08)
        cylinder(f"jar_{index}", (x, height * 0.5, z), radius, height, "FILL_GLASS")
        cylinder(f"jar_lid_{index}", (x, height + 0.008, z), radius * 1.04, 0.016,
                 rng.choice(("FILL_RED", "FILL_OCHRE", "FILL_STEEL")), vertices=14)


def tools(rng: random.Random) -> None:
    tins = rng.randint(2, 4)
    for index in range(tins):
        radius = rng.uniform(0.055, 0.082)
        height = rng.uniform(0.12, 0.22)
        x = -0.24 + index * (0.48 / max(1, tins - 1)) + rng.uniform(-0.018, 0.018)
        z = rng.uniform(0.025, 0.08)
        cylinder(f"paint_tin_{index}", (x, height * 0.5, z),
                 radius, height, rng.choice(SOFT), vertices=18)
        cylinder(f"paint_lid_{index}", (x, height + 0.006, z),
                 radius * 1.03, 0.012, "FILL_STEEL", vertices=18)
    for index in range(rng.randint(2, 5)):
        length = rng.uniform(0.18, 0.34)
        yaw = rng.uniform(-38.0, 38.0)
        x = rng.uniform(-0.16, 0.16)
        z = rng.uniform(-0.15, -0.09)
        box(f"tool_handle_{index}", (x, 0.022, z), (length, 0.044, 0.032), "FILL_WOOD",
            yaw_deg=yaw, bevel=0.003)
        angle = math.radians(yaw)
        head_x = x + math.cos(angle) * length * 0.43
        head_z = z + math.sin(angle) * length * 0.43
        box(f"tool_head_{index}", (head_x, 0.035, head_z),
            (rng.uniform(0.075, 0.12), 0.07, 0.055), "FILL_STEEL",
            yaw_deg=yaw + 90.0, bevel=0.003)


AUTHORS = {"books": books, "textiles": textiles, "crockery": crockery, "tools": tools}


def main() -> None:
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--kind", choices=KINDS, required=True)
    parser.add_argument("--seed", type=int, required=True)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(arguments)
    if options.seed < 0:
        parser.error("--seed must be non-negative")

    kit.CURRENT = f"fill_{options.kind}"
    kit.COUNTERS[kit.CURRENT] = 0
    kit.MATERIAL_COLOURS.update(COLOURS)
    rng = random.Random(options.seed)
    kit.export(options.out, lambda: AUTHORS[options.kind](rng))
    print(f"fill_kit_gen: {options.kind} seed {options.seed} EXIT 0")


if __name__ == "__main__":
    main()
