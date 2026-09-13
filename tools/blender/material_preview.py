#!/usr/bin/env python3
"""Render and verify HOUSE-00296's sphere-and-floor material previews.

Run the render through Blender and the cheap check through the normal Python interpreter:

    blender --background --python tools/blender/material_preview.py -- --render
    python3 tools/blender/material_preview.py --check

The preview is deliberately a material review, not an albedo contact sheet: the sphere and tiled
floor both use base colour, OpenGL tangent normal, AO, roughness and metalness from the committed
maps.  Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "assets"))
import ambientcg_materials  # noqa: E402

TEXTURES = REPO / "assets-src" / "Textures" / "Materials"
OUTPUT = REPO / "docs" / "asset-review" / "materials"
WIDTH = 640
HEIGHT = 400


def look_at(obj, target, Vector) -> None:
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def make_material(bpy, material) -> object:
    node_tree = bpy.data.materials.new(material.slug)
    node_tree.use_nodes = True
    nodes = node_tree.node_tree.nodes
    links = node_tree.node_tree.links
    nodes.clear()

    output = nodes.new("ShaderNodeOutputMaterial")
    shader = nodes.new("ShaderNodeBsdfPrincipled")
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

    orm = nodes.new("ShaderNodeTexImage")
    orm.image = bpy.data.images.load(str(TEXTURES / f"{material.slug}_orm.png"))
    orm.image.colorspace_settings.name = "Non-Color"
    orm.extension = "REPEAT"
    split = nodes.new("ShaderNodeSeparateColor")
    multiply = nodes.new("ShaderNodeMixRGB")
    multiply.blend_type = "MULTIPLY"
    multiply.inputs[0].default_value = 1.0

    links.new(texcoord.outputs["UV"], mapping.inputs["Vector"])
    for texture in (albedo, normal_texture, orm):
        links.new(mapping.outputs["Vector"], texture.inputs["Vector"])
    links.new(albedo.outputs["Color"], multiply.inputs[1])
    links.new(orm.outputs["Color"], split.inputs["Color"])
    links.new(split.outputs["Red"], multiply.inputs[2])
    links.new(multiply.outputs["Color"], shader.inputs["Base Color"])
    links.new(split.outputs["Green"], shader.inputs["Roughness"])
    links.new(split.outputs["Blue"], shader.inputs["Metallic"])
    links.new(normal_texture.outputs["Color"], normal.inputs["Color"])
    links.new(normal.outputs["Normal"], shader.inputs["Normal"])
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])
    return node_tree


def render(*, force: bool = False) -> int:
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

    bpy.ops.mesh.primitive_plane_add(size=5.5, location=(0.0, 0.35, 0.0))
    floor = bpy.context.object
    floor.name = "test_floor"
    bpy.ops.mesh.primitive_uv_sphere_add(segments=64, ring_count=32,
                                         location=(-0.75, 0.0, 0.78), radius=0.78)
    sphere = bpy.context.object
    sphere.name = "test_sphere"
    bpy.ops.object.shade_smooth()

    camera_data = bpy.data.cameras.new("camera")
    camera = bpy.data.objects.new("camera", camera_data)
    scene.collection.objects.link(camera)
    camera.location = (3.4, -5.2, 3.15)
    camera_data.lens = 54
    look_at(camera, (0.0, 0.4, 0.55), Vector)
    scene.camera = camera

    for name, location, energy, size, colour in (
        ("key", (-3.0, -3.0, 5.0), 850.0, 3.0, (1.0, 0.90, 0.80)),
        ("fill", (4.0, -1.0, 2.8), 600.0, 3.0, (0.72, 0.84, 1.0)),
        ("rim", (0.0, 4.0, 4.5), 900.0, 2.0, (1.0, 1.0, 1.0)),
    ):
        light_data = bpy.data.lights.new(name, "AREA")
        light_data.energy = energy
        light_data.shape = "DISK"
        light_data.size = size
        light_data.color = colour
        light = bpy.data.objects.new(name, light_data)
        light.location = location
        look_at(light, (0.0, 0.4, 0.35), Vector)
        scene.collection.objects.link(light)

    OUTPUT.mkdir(parents=True, exist_ok=True)
    for material in ambientcg_materials.MATERIALS:
        target = OUTPUT / f"{material.slug}.png"
        if target.is_file() and not force:
            print(f"material_preview: keep existing {material.slug}", flush=True)
            continue
        preview_material = make_material(bpy, material)
        floor.data.materials.clear()
        sphere.data.materials.clear()
        floor.data.materials.append(preview_material)
        sphere.data.materials.append(preview_material)
        scene.render.filepath = str(target)
        bpy.ops.render.render(write_still=True)
        bpy.data.materials.remove(preview_material, do_unlink=True)
        print(f"material_preview: rendered {material.slug}", flush=True)
    return check()


def check() -> int:
    from PIL import Image

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
        except OSError as error:
            problems.append(f"{path}: {error}")
    if problems:
        for problem in problems:
            print(f"material_preview: {problem}", file=sys.stderr)
        return 1
    print(f"material_preview: clean -- {len(expected)} sphere-and-floor renders at "
          f"{WIDTH}x{HEIGHT}")
    return 0


def main() -> int:
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--render", action="store_true")
    group.add_argument("--check", action="store_true")
    parser.add_argument("--force", action="store_true",
                        help="rerender existing previews (valid only with --render)")
    args = parser.parse_args(arguments)
    if args.force and not args.render:
        parser.error("--force requires --render")
    return render(force=args.force) if args.render else check()


if __name__ == "__main__":
    sys.exit(main())
