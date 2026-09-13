#!/usr/bin/env python3
"""Render and verify compact review sheets for HOUSE-00297's vegetation set.

The committed artefacts are four JPEG contact sheets.  Individual lossless renders stay under
``build/p2-vegetation-review`` so review does not add dozens of redundant images to the repository.
The fourth sheet puts every mature tree's LOD0/1/2 beside each other; this is the alpha-aware visual
check that ``lod_gen.py``'s geometry-only silhouette rasteriser cannot perform for leaf cards.

    tools/blender/vegetation_preview.py --render
    python3 tools/blender/vegetation_preview.py --compose
    python3 tools/blender/vegetation_preview.py --check
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "assets"))
import polyhaven_vegetation  # noqa: E402

try:
    import bpy  # type: ignore  # noqa: F401

    INSIDE_BLENDER = True
except ImportError:
    INSIDE_BLENDER = False

if not INSIDE_BLENDER and "--render" in sys.argv:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import blender_env

    sys.exit(blender_env.relaunch(Path(__file__).resolve(), sys.argv[1:], tool="vegetation_preview"))

MODELS = REPO / "assets-src" / "Models" / "Vegetation"
OUTPUT = REPO / "docs" / "asset-review" / "vegetation"
DEFAULT_RENDER_DIR = REPO / "build" / "p2-vegetation-review"
WIDTH = 240
HEIGHT = 270

SHEETS = {
    "trees.jpg": (6, 3),
    "shrubs.jpg": (3, 3),
    "flowers-and-grass.jpg": (4, 2),
    "tree-lods.jpg": (6, 3),
}


def render_name(item, level: str = "LOD0") -> str:
    return f"{item.slug}__{level}.png"


def render_specs() -> list[tuple[object, str]]:
    specs = [(item, "LOD0") for item in polyhaven_vegetation.OUTPUTS]
    specs += [
        (item, level)
        for item in polyhaven_vegetation.OUTPUTS
        if item.role == "tree" and item.age == "mature"
        for level in ("LOD1", "LOD2")
    ]
    return specs


def _look_at(obj, target, Vector) -> None:
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def _bounds(obj, Vector):
    points = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    low = Vector(tuple(min(point[axis] for point in points) for axis in range(3)))
    high = Vector(tuple(max(point[axis] for point in points) for axis in range(3)))
    return low, high


def render(render_dir: Path, *, force: bool, only: str | None = None) -> int:
    import bpy
    from mathutils import Vector

    render_dir.mkdir(parents=True, exist_ok=True)
    for item, level in render_specs():
        if only is not None and item.slug != only:
            continue
        target = render_dir / render_name(item, level)
        if target.is_file() and not force:
            print(f"vegetation_preview: keep {target.name}", flush=True)
            continue

        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=str(MODELS / f"{item.slug}.glb"))
        meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
        suffix = "" if level == "LOD0" else f"_{level}"
        candidates = [obj for obj in meshes if obj.data.name.endswith(suffix)] if suffix else [
            obj for obj in meshes if not obj.data.name.endswith(("_LOD1", "_LOD2"))
        ]
        if len(candidates) != 1:
            raise RuntimeError(f"{item.slug}: expected one {level} mesh, found {len(candidates)}")
        subject = candidates[0]
        for obj in meshes:
            if obj != subject:
                bpy.data.objects.remove(obj, do_unlink=True)

        low, high = _bounds(subject, Vector)
        centre = (low + high) * 0.5
        size = high - low
        span = max(size.x, size.y, size.z, 0.01)

        scene = bpy.context.scene
        scene.render.engine = "BLENDER_EEVEE_NEXT"
        scene.render.resolution_x = WIDTH
        scene.render.resolution_y = HEIGHT
        scene.render.resolution_percentage = 100
        scene.render.image_settings.file_format = "PNG"
        scene.render.image_settings.color_mode = "RGB"
        scene.render.image_settings.color_depth = "8"
        scene.render.image_settings.compression = 90
        scene.render.film_transparent = False
        scene.render.threads_mode = "FIXED"
        scene.render.threads = 4
        if hasattr(scene, "eevee"):
            scene.eevee.taa_render_samples = 4
        scene.view_settings.look = "AgX - Medium High Contrast"
        scene.world = bpy.data.worlds.new("vegetation-review-world")
        scene.world.use_nodes = True
        background = scene.world.node_tree.nodes["Background"]
        background.inputs[0].default_value = (0.055, 0.065, 0.075, 1.0)
        background.inputs[1].default_value = 0.7

        bpy.ops.mesh.primitive_plane_add(size=span * 8.0, location=(centre.x, centre.y, low.z - 0.003))
        floor = bpy.context.object
        floor_material = bpy.data.materials.new("neutral-ground")
        floor_material.diffuse_color = (0.12, 0.13, 0.11, 1.0)
        floor.data.materials.append(floor_material)

        camera_data = bpy.data.cameras.new("camera")
        camera_data.type = "ORTHO"
        # Portrait frame: reserve a little ground below and canopy air above.
        plan_diameter = math.hypot(size.x, size.y)
        frame = max(size.z * 1.22, plan_diameter * HEIGHT / WIDTH * 1.13)
        # The comparison sheet judges reduced meshes near their actual transition sizes, not as
        # 300-pixel close-ups: LOD1 hands off at 90 px and LOD2 at 30 px (§26.1).
        if level == "LOD1":
            frame *= 2.5
        elif level == "LOD2":
            frame *= 7.5
        camera_data.ortho_scale = frame
        camera = bpy.data.objects.new("camera", camera_data)
        scene.collection.objects.link(camera)
        camera.location = centre + Vector((1.35, -2.4, 0.70)).normalized() * span * 4.0
        _look_at(camera, (centre.x, centre.y, low.z + size.z * 0.46), Vector)
        scene.camera = camera

        # Thin double-sided leaf cards need light from both hemispheres. One directional source
        # made a valid tree facing away from it read as an empty black silhouette in the review.
        for name, energy, rotation in (
            ("key-sun", 2.8, (0.55, -0.65, -0.35)),
            ("fill-sun", 1.8, (-0.65, 0.45, 2.6)),
        ):
            sun_data = bpy.data.lights.new(name, "SUN")
            sun_data.energy = energy
            sun_data.angle = 0.35
            sun = bpy.data.objects.new(name, sun_data)
            sun.rotation_euler = rotation
            scene.collection.objects.link(sun)

        scene.render.filepath = str(target)
        bpy.ops.render.render(write_still=True)
        print(f"vegetation_preview: rendered {target.name}", flush=True)
    return 0


def _sheet_rows():
    trees = [item for item in polyhaven_vegetation.OUTPUTS if item.role == "tree"]
    shrubs = [item for item in polyhaven_vegetation.OUTPUTS if item.role == "shrub"]
    ground = [item for item in polyhaven_vegetation.OUTPUTS if item.role in ("flower", "grass-card")]
    mature = [item for item in trees if item.age == "mature"]
    return {
        "trees.jpg": [(item, "LOD0") for item in trees],
        "shrubs.jpg": [(item, "LOD0") for item in shrubs],
        "flowers-and-grass.jpg": [(item, "LOD0") for item in ground],
        "tree-lods.jpg": [(item, level) for level in ("LOD0", "LOD1", "LOD2") for item in mature],
    }


def compose(render_dir: Path) -> int:
    from PIL import Image, ImageDraw

    OUTPUT.mkdir(parents=True, exist_ok=True)
    label_height = 34
    for filename, cells in _sheet_rows().items():
        columns, rows = SHEETS[filename]
        sheet = Image.new("RGB", (columns * WIDTH, rows * (HEIGHT + label_height)), "#20252a")
        draw = ImageDraw.Draw(sheet)
        for index, (item, level) in enumerate(cells):
            source = render_dir / render_name(item, level)
            if not source.is_file():
                raise FileNotFoundError(f"missing render {source}; run --render first")
            x = (index % columns) * WIDTH
            y = (index // columns) * (HEIGHT + label_height)
            with Image.open(source) as image:
                sheet.paste(image.convert("RGB"), (x, y))
            label = item.slug.removeprefix("tree_").replace("_", " ")
            if filename == "tree-lods.jpg":
                label = f"{label.removesuffix(' mature')}  {level}"
            draw.text((x + 8, y + HEIGHT + 9), label, fill="#f0f2f3")
        sheet.save(OUTPUT / filename, format="JPEG", quality=88, optimize=True)
        print(f"vegetation_preview: composed {filename}")
    return check()


def check() -> int:
    from PIL import Image

    expected = set(SHEETS)
    actual = {path.name for path in OUTPUT.glob("*.jpg")} if OUTPUT.is_dir() else set()
    problems = [f"missing {name}" for name in sorted(expected - actual)]
    problems += [f"unexpected {name}" for name in sorted(actual - expected)]
    for name in sorted(expected & actual):
        columns, rows = SHEETS[name]
        with Image.open(OUTPUT / name) as image:
            wanted = (columns * WIDTH, rows * (HEIGHT + 34))
            if image.size != wanted:
                problems.append(f"{name}: {image.size}, expected {wanted}")
    if problems:
        for problem in problems:
            print(f"vegetation_preview: {problem}", file=sys.stderr)
        return 1
    print("vegetation_preview: clean -- 4 contact sheets including mature-tree LOD comparison")
    return 0


def main() -> int:
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--render", action="store_true")
    group.add_argument("--compose", action="store_true")
    group.add_argument("--check", action="store_true")
    parser.add_argument("--render-dir", type=Path, default=DEFAULT_RENDER_DIR)
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--item", choices=[item.slug for item in polyhaven_vegetation.OUTPUTS],
                        help="render only one output (valid only with --render)")
    args = parser.parse_args(arguments)
    if args.force and not args.render:
        parser.error("--force requires --render")
    if args.item and not args.render:
        parser.error("--item requires --render")
    if args.render:
        return render(args.render_dir, force=args.force, only=args.item)
    if args.compose:
        return compose(args.render_dir)
    return check()


if __name__ == "__main__":
    try:
        status = main()
    except BaseException:
        import traceback

        traceback.print_exc()
        status = 1
    if INSIDE_BLENDER:
        print(f"vegetation_preview: EXIT {status}")
    sys.exit(status)
