#!/usr/bin/env python3
"""Generate HOUSE-00985's bounded static storage, fixture and service-run kit from JSON data."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kitchen_builtins as kit  # noqa: E402


DATA = Path(__file__).with_name("prop_kit_data.json")
COLOURS = {
    "KIT_PAINT": (0.69, 0.72, 0.68, 1.0),
    "KIT_WOOD": (0.40, 0.23, 0.12, 1.0),
    "KIT_METAL": (0.42, 0.45, 0.47, 1.0),
    "KIT_GLASS": (0.62, 0.72, 0.74, 1.0),
    "KIT_SCREEN": (0.035, 0.045, 0.055, 1.0),
    "KIT_CARDBOARD": (0.55, 0.39, 0.22, 1.0),
    "KIT_WHITE": (0.88, 0.87, 0.82, 1.0),
}


def box(label: str, location, size, finish="KIT_PAINT", bevel=0.003):
    return kit.box(label, location, size, finish=finish, bevel=bevel)


def collision(size, mount: str) -> None:
    x, y, z = size
    centre_y = 0.0 if mount == "wall" else y * 0.5
    centre_z = -z * 0.5 if mount == "wall" else 0.0
    obj = box("proxy", (0.0, centre_y, centre_z), size, "KIT_METAL", bevel=0.0)
    obj.name = f"{kit.CURRENT}_COL"


def carcass(spec: dict) -> None:
    width, height, depth = spec["size"]
    panel = 0.025
    front = -depth * 0.5 - 0.006
    box("back", (0, height * 0.5, depth * 0.5 - panel * 0.5),
        (width, height, panel), "KIT_WOOD")
    for side in (-1, 1):
        box(f"side_{side}", (side * (width - panel) * 0.5, height * 0.5, 0),
            (panel, height, depth), "KIT_WOOD")
    box("base", (0, panel * 0.5, 0), (width, panel, depth), "KIT_WOOD")
    box("top", (0, height - panel * 0.5, 0), (width, panel, depth), "KIT_WOOD")
    variant = spec["variant"]
    if variant == "open":
        for index in range(1, 5):
            y = panel + index * (height - 2 * panel) / 5
            box(f"shelf_{index}", (0, y, 0), (width - 2 * panel, panel, depth - panel),
                "KIT_WOOD")
    elif variant == "drawers":
        count = 4
        for index in range(count):
            h = (height - 0.08) / count
            y = 0.04 + (index + 0.5) * h
            box(f"drawer_{index}", (0, y, front), (width - 0.06, h - 0.015, 0.025))
            box(f"pull_{index}", (0, y, front - 0.026), (width * 0.32, 0.018, 0.025),
                "KIT_METAL", 0.002)
    elif variant == "chest":
        box("front", (0, height * 0.48, front), (width - 0.05, height - 0.10, 0.025))
        box("lid", (0, height + 0.008, 0), (width + 0.025, 0.032, depth + 0.025),
            "KIT_WOOD")
    elif variant == "door-drawer":
        box("door", (0, height * 0.38, front), (width - 0.05, height * 0.55, 0.025))
        box("drawer", (0, height * 0.80, front), (width - 0.05, height * 0.22, 0.025))
        box("pull", (0, height * 0.80, front - 0.026), (width * 0.34, 0.018, 0.025),
            "KIT_METAL", 0.002)
    else:
        for side in (-1, 1):
            box(f"door_{side}", (side * width * 0.245, height * 0.5, front),
                (width * 0.47, height - 0.07, 0.025))
            box(f"pull_{side}", (side * width * 0.055, height * 0.53, front - 0.025),
                (0.018, min(0.22, height * 0.24), 0.025), "KIT_METAL", 0.002)
    collision(spec["size"], "floor")


def shelving(spec: dict) -> None:
    width, height, depth = spec["size"]
    post = 0.045
    for x in (-width * 0.5 + post * 0.5, width * 0.5 - post * 0.5):
        for z in (-depth * 0.5 + post * 0.5, depth * 0.5 - post * 0.5):
            box("post", (x, height * 0.5, z), (post, height, post), "KIT_METAL")
    for index in range(5):
        y = 0.08 + index * (height - 0.16) / 4
        box(f"shelf_{index}", (0, y, 0), (width, 0.035, depth), "KIT_WOOD")
    collision(spec["size"], "floor")


def worktop(spec: dict) -> None:
    width, height, depth = spec["size"]
    box("top", (0, height - 0.035, 0), (width, 0.07, depth), "KIT_WOOD", 0.006)
    for x in (-width * 0.43, width * 0.43):
        for z in (-depth * 0.37, depth * 0.37):
            box("leg", (x, (height - 0.07) * 0.5, z),
                (0.075, height - 0.07, 0.075), "KIT_METAL")
    box("lower_shelf", (0, 0.20, 0), (width * 0.88, 0.035, depth * 0.72), "KIT_WOOD")
    collision(spec["size"], "floor")


def storage_box(spec: dict) -> None:
    width, height, depth = spec["size"]
    box("body", (0, (height - 0.05) * 0.5, 0),
        (width, height - 0.05, depth), "KIT_CARDBOARD", 0.006)
    box("lid", (0, height - 0.025, 0), (width + 0.035, 0.05, depth + 0.035),
        "KIT_CARDBOARD", 0.004)
    box("label", (0, height * 0.56, -depth * 0.5 - 0.004),
        (width * 0.32, height * 0.18, 0.008), "KIT_WHITE", 0.001)
    collision(spec["size"], "floor")


def fixture(spec: dict) -> None:
    width, height, depth = spec["size"]
    variant = spec["variant"]
    if variant == "mirror":
        box("frame", (0, 0, -depth * 0.5), (width, height, depth), "KIT_METAL")
        box("glass", (0, 0, -depth - 0.003), (width - 0.055, height - 0.055, 0.006),
            "KIT_GLASS", 0.001)
    elif variant == "towel-rail":
        for x in (-width * 0.45, width * 0.45):
            box("bracket", (x, 0, -depth * 0.42), (0.045, height, depth * 0.84),
                "KIT_METAL")
        box("rail", (0, 0, -depth), (width, 0.035, 0.035), "KIT_METAL", 0.006)
    elif variant == "screen":
        box("border", (0, 0, -depth * 0.5), (width, height, depth), "KIT_SCREEN", 0.004)
        box("surface", (0, 0, -depth - 0.003), (width - 0.08, height - 0.08, 0.006),
            "KIT_WHITE", 0.001)
    else:
        box("body", (0, 0, -depth * 0.5), (width, height, depth), "KIT_WHITE", 0.018)
        box("lens", (0, 0, -depth - 0.018), (width * 0.24, height * 0.48, 0.036),
            "KIT_GLASS", 0.006)
    collision(spec["size"], "wall")


def service_run(spec: dict) -> None:
    length, height, depth = spec["size"]
    variant = spec["variant"]
    if variant == "duct":
        box("duct", (0, 0, -depth * 0.5), (length, height, depth), "KIT_METAL", 0.008)
        for x in (-length * 0.34, 0, length * 0.34):
            box("seam", (x, 0, -depth - 0.006), (0.018, height + 0.025, 0.012),
                "KIT_METAL", 0.001)
    else:
        radius = height * 0.5
        kit.rod(variant, (-length * 0.5, 0, -radius), (length * 0.5, 0, -radius),
                radius=radius, finish="KIT_METAL", vertices=12)
        for x in (-length * 0.38, 0, length * 0.38):
            box("clip", (x, 0, -depth * 0.36), (0.035, height * 1.30, depth * 0.72),
                "KIT_METAL", 0.002)
    collision(spec["size"], "wall")


AUTHORS = {"carcass": carcass, "shelving": shelving, "worktop": worktop,
           "box": storage_box, "fixture": fixture, "service-run": service_run}


def export(name: str, spec: dict, target: Path) -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    kit.CURRENT = f"propkit_{name}"
    kit.COUNTERS[kit.CURRENT] = 0
    AUTHORS[spec["family"]](spec)
    if spec.get("mount", "floor") == "floor":
        depths = [-vertex.co.y for obj in bpy.context.scene.objects if obj.type == "MESH"
                  for vertex in obj.data.vertices]
        centre_z = (min(depths) + max(depths)) * 0.5
        for obj in bpy.context.scene.objects:
            if obj.type == "MESH":
                for vertex in obj.data.vertices:
                    vertex.co.y += centre_z
                obj.data.update()
    target.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(target), export_format="GLB", export_yup=True,
                              export_apply=True, export_materials="EXPORT",
                              export_lights=False, export_cameras=False)


def main() -> None:
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--name", required=True)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(arguments)
    rows = json.loads(DATA.read_text())
    if options.name not in rows:
        parser.error(f"unknown kit piece {options.name!r}")
    kit.MATERIAL_COLOURS.update(COLOURS)
    export(options.name, rows[options.name], options.out)
    print(f"prop_kit_gen: {options.name} EXIT 0")


if __name__ == "__main__":
    main()
