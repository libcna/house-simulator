#!/usr/bin/env python3
"""Render and verify HOUSE-00900's stock-mapped base-material previews.

Run the render through Blender and the cheap check through the normal Python interpreter:

    blender --background --python tools/blender/material_preview.py -- --render
    python3 tools/blender/material_preview.py --check

Each image shows the material on a sphere, tiled floor and wall in three repeated bays.  The bays
are lit low, medium and high from left to right.  The shader is the authored stock mapping rather
than a PBR preview: textured diffuse plus the constant specular colour and Blinn-Phong-derived
roughness recorded by `pbr_to_stock.py`.  Offline tooling: not runtime code, not subject to the
XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "assets"))
import ambientcg_materials  # noqa: E402
import pbr_to_stock  # noqa: E402
sys.path.insert(0, str(REPO / "tools" / "world"))
import layout_io  # noqa: E402

TEXTURES = REPO / "assets-src" / "Textures" / "Materials"
OUTPUT = REPO / "docs" / "asset-review" / "materials"
CONTACT_SHEET = OUTPUT / "contact-sheet.jpg"
WIDTH = 960
HEIGHT = 480
CONTACT_COLUMNS = 4
CONTACT_THUMB = (320, 160)


def look_at(obj, target, Vector) -> None:
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def base_rows() -> dict[str, dict]:
    document = json.loads(
        layout_io.strip_jsonc(pbr_to_stock.MATERIALS_FILE.read_text(encoding="utf-8"))
    )
    return {
        row["id"]: row
        for row in document["materials"]
        if row["id"].startswith("MAT_BASE_")
    }


def make_material(bpy, material, row: dict) -> object:
    node_tree = bpy.data.materials.new(material.slug)
    node_tree.use_nodes = True
    nodes = node_tree.node_tree.nodes
    links = node_tree.node_tree.links
    nodes.clear()

    output = nodes.new("ShaderNodeOutputMaterial")
    diffuse = nodes.new("ShaderNodeBsdfDiffuse")
    glossy = nodes.new("ShaderNodeBsdfGlossy")
    add = nodes.new("ShaderNodeAddShader")
    texcoord = nodes.new("ShaderNodeTexCoord")
    mapping = nodes.new("ShaderNodeMapping")
    mapping.inputs["Scale"].default_value = (2.2, 2.2, 2.2)

    albedo = nodes.new("ShaderNodeTexImage")
    albedo.image = bpy.data.images.load(str(TEXTURES / f"{material.slug}_albedo.png"))
    albedo.image.colorspace_settings.name = "sRGB"
    albedo.extension = "REPEAT"

    normal_texture = nodes.new("ShaderNodeTexImage")
    normal_texture.image = bpy.data.images.load(str(TEXTURES / f"{material.slug}_normal.png"))
    normal_texture.image.colorspace_settings.name = "Non-Color"
    normal_texture.extension = "REPEAT"
    normal = nodes.new("ShaderNodeNormalMap")
    normal.inputs["Strength"].default_value = 0.75

    tint_multiply = nodes.new("ShaderNodeMixRGB")
    tint_multiply.blend_type = "MULTIPLY"
    tint_multiply.inputs[0].default_value = 1.0
    tint_multiply.inputs[2].default_value = (*row["tint"], 1.0)
    glossy.inputs["Color"].default_value = (*row["specularColor"], 1.0)
    # Inverse of pbr_to_stock.py's `power = 2 / alpha^2 - 2`, with glTF's
    # `alpha = roughness^2`: this lets Blender show the width of the authored stock lobe.
    glossy.inputs["Roughness"].default_value = math.pow(
        2.0 / (float(row["specularPower"]) + 2.0), 0.25
    )

    links.new(texcoord.outputs["UV"], mapping.inputs["Vector"])
    for texture in (albedo, normal_texture):
        links.new(mapping.outputs["Vector"], texture.inputs["Vector"])
    links.new(albedo.outputs["Color"], tint_multiply.inputs[1])
    links.new(tint_multiply.outputs["Color"], diffuse.inputs["Color"])
    links.new(normal_texture.outputs["Color"], normal.inputs["Color"])
    links.new(normal.outputs["Normal"], diffuse.inputs["Normal"])
    links.new(normal.outputs["Normal"], glossy.inputs["Normal"])
    links.new(diffuse.outputs["BSDF"], add.inputs[0])
    links.new(glossy.outputs["BSDF"], add.inputs[1])
    links.new(add.outputs["Shader"], output.inputs["Surface"])
    return node_tree


def render(*, force: bool = False, only: str | None = None) -> int:
    import bpy
    from mathutils import Vector

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = WIDTH
    scene.render.resolution_y = HEIGHT
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.image_settings.compression = 90
    scene.render.film_transparent = False
    scene.eevee.taa_render_samples = 16
    scene.render.threads_mode = "FIXED"
    scene.render.threads = 4
    scene.render.image_settings.color_depth = "8"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.world = bpy.data.worlds.new("preview_world")
    scene.world.color = (0.025, 0.025, 0.025)

    bpy.ops.mesh.primitive_plane_add(size=9.0, location=(0.0, 0.5, 0.0))
    floor = bpy.context.object
    floor.name = "test_floor"
    bpy.ops.mesh.primitive_plane_add(
        size=2.0,
        location=(0.0, 2.3, 1.8),
        rotation=(math.pi / 2.0, 0.0, 0.0),
    )
    wall = bpy.context.object
    wall.name = "test_wall"
    wall.scale = (4.5, 1.8, 1.0)
    spheres = []
    for index, x in enumerate((-2.7, 0.0, 2.7)):
        bpy.ops.mesh.primitive_uv_sphere_add(
            segments=48, ring_count=24, location=(x, 0.0, 0.72), radius=0.72
        )
        sphere = bpy.context.object
        sphere.name = f"test_sphere_{index}"
        bpy.ops.object.shade_smooth()
        spheres.append(sphere)

    camera_data = bpy.data.cameras.new("camera")
    camera = bpy.data.objects.new("camera", camera_data)
    scene.collection.objects.link(camera)
    camera.location = (0.0, -8.2, 3.25)
    camera_data.lens = 52
    look_at(camera, (0.0, 0.65, 0.8), Vector)
    scene.camera = camera

    for name, x, energy in (
        ("low", -2.7, 180.0),
        ("medium", 0.0, 650.0),
        ("high", 2.7, 1450.0),
    ):
        light_data = bpy.data.lights.new(name, "SPOT")
        light_data.energy = energy
        light_data.color = (1.0, 0.94, 0.86)
        light_data.spot_size = math.radians(47.0)
        light_data.spot_blend = 0.28
        light = bpy.data.objects.new(name, light_data)
        light.location = (x, -2.2, 4.0)
        look_at(light, (x, 0.55, 0.7), Vector)
        scene.collection.objects.link(light)

    OUTPUT.mkdir(parents=True, exist_ok=True)
    rows = base_rows()
    selected = [material for material in ambientcg_materials.MATERIALS
                if only is None or material.slug == only]
    if not selected:
        print(f"material_preview: unknown material {only}", file=sys.stderr)
        return 2
    for material in selected:
        target = OUTPUT / f"{material.slug}.png"
        if target.is_file() and not force:
            print(f"material_preview: keep existing {material.slug}", flush=True)
            continue
        row = rows[pbr_to_stock.base_material_id(material.slug)]
        preview_material = make_material(bpy, material, row)
        floor.data.materials.clear()
        wall.data.materials.clear()
        floor.data.materials.append(preview_material)
        wall.data.materials.append(preview_material)
        for sphere in spheres:
            sphere.data.materials.clear()
            sphere.data.materials.append(preview_material)
        scene.render.filepath = str(target)
        bpy.ops.render.render(write_still=True)
        bpy.data.materials.remove(preview_material, do_unlink=True)
        print(f"material_preview: rendered {material.slug}", flush=True)
    if only is not None:
        return 0
    make_contact_sheet()
    return check()


def make_contact_sheet() -> None:
    from PIL import Image, ImageDraw

    rows = math.ceil(len(ambientcg_materials.MATERIALS) / CONTACT_COLUMNS)
    label_height = 24
    sheet = Image.new(
        "RGB",
        (CONTACT_COLUMNS * CONTACT_THUMB[0], rows * (CONTACT_THUMB[1] + label_height)),
        (31, 34, 40),
    )
    draw = ImageDraw.Draw(sheet)
    for index, material in enumerate(ambientcg_materials.MATERIALS):
        with Image.open(OUTPUT / f"{material.slug}.png") as source:
            thumb = source.convert("RGB").resize(CONTACT_THUMB, Image.Resampling.LANCZOS)
        x = (index % CONTACT_COLUMNS) * CONTACT_THUMB[0]
        y = (index // CONTACT_COLUMNS) * (CONTACT_THUMB[1] + label_height)
        sheet.paste(thumb, (x, y))
        draw.text((x + 6, y + CONTACT_THUMB[1] + 4), material.slug, fill=(235, 235, 235))
    sheet.save(CONTACT_SHEET, quality=90, optimize=True)


def check() -> int:
    from PIL import Image, ImageStat

    expected = {f"{material.slug}.png" for material in ambientcg_materials.MATERIALS}
    actual = {path.name for path in OUTPUT.glob("*.png")} if OUTPUT.is_dir() else set()
    problems = [f"missing {OUTPUT / name}" for name in sorted(expected - actual)]
    problems += [f"unexpected {OUTPUT / name}" for name in sorted(actual - expected)]
    for name in sorted(expected & actual):
        path = OUTPUT / name
        try:
            with Image.open(path) as image:
                image.load()
                if image.size != (WIDTH, HEIGHT):
                    problems.append(f"{path}: {image.size}, expected {(WIDTH, HEIGHT)}")
                if image.mode not in ("RGB", "RGBA"):
                    problems.append(f"{path}: mode {image.mode}, expected RGB/RGBA")
                if image.size == (WIDTH, HEIGHT):
                    thirds = [image.crop((i * WIDTH // 3, 0, (i + 1) * WIDTH // 3, HEIGHT))
                              for i in range(3)]
                    means = [
                        sum(ImageStat.Stat(third.convert("RGB")).mean) / 3.0
                        for third in thirds
                    ]
                    if not means[0] < means[1] < means[2]:
                        problems.append(
                            f"{path}: low/medium/high bands are not increasing: {means}"
                        )
        except OSError as error:
            problems.append(f"{path}: {error}")
    expected_contact_size = (
        CONTACT_COLUMNS * CONTACT_THUMB[0],
        math.ceil(len(expected) / CONTACT_COLUMNS) * (CONTACT_THUMB[1] + 24),
    )
    try:
        with Image.open(CONTACT_SHEET) as image:
            image.load()
            if image.size != expected_contact_size:
                problems.append(
                    f"{CONTACT_SHEET}: {image.size}, expected {expected_contact_size}"
                )
    except OSError as error:
        problems.append(f"{CONTACT_SHEET}: {error}")
    if problems:
        for problem in problems:
            print(f"material_preview: {problem}", file=sys.stderr)
        return 1
    print(f"material_preview: clean -- {len(expected)} stock-mapped sphere/floor/wall renders "
          f"at three light levels, {WIDTH}x{HEIGHT}")
    return 0


def main() -> int:
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--render", action="store_true")
    group.add_argument("--check", action="store_true")
    parser.add_argument("--force", action="store_true",
                        help="rerender existing previews (valid only with --render)")
    parser.add_argument("--only", choices=[material.slug for material in ambientcg_materials.MATERIALS],
                        help="render one material while iterating (valid only with --render)")
    args = parser.parse_args(arguments)
    if args.force and not args.render:
        parser.error("--force requires --render")
    if args.only and not args.render:
        parser.error("--only requires --render")
    return render(force=args.force, only=args.only) if args.render else check()


if __name__ == "__main__":
    sys.exit(main())
