#!/usr/bin/env python3
"""impostor_render.py -- 8-yaw impostor atlases, rendered with EEVEE and packed.

`HOUSE-00204`. `cna-house.md` §26.3: each impostor subject is rendered from 8 yaw angles at a fixed
elevation into a 2048² RGBA atlas, the alpha carrying the silhouette and the colour **pre-lit for
an overcast sky**; at runtime it is one camera-facing quad choosing the nearest yaw slice, tinted by
the current sky colour so it does not look pasted on at sunset. §26.1 reaches it at 8 px of screen
height, which is why two triangles are enough.

    blender --background --factory-startup --python impostor_render.py -- IN.glb OUT_DIR
    tools/blender/impostor_render.py IN.glb OUT_DIR --name=oak_mature
    tools/blender/impostor_render.py --selftest

**Run it either way.** Invoked as a plain script it finds `blender` and re-runs itself inside it.

## The lighting is deliberately baked, and deliberately flat

There is no sun. The world is a uniform white dome, so nothing in the atlas carries a direction —
which is the whole point: the runtime tints the quad by the sky colour, and a baked sun would fight
that tint and light the tree from the wrong side for twenty of every twenty-four hours. "Pre-lit
for an overcast sky" means exactly a uniform dome, and the selftest asserts it by rendering the
same surface from opposite yaws and requiring the same value. What the atlas therefore carries is
albedo times ambient occlusion, which tints correctly and reads as a distant tree on any day.

`view_transform` is forced to **Standard**. Blender's default (AgX, or Filmic before it) is a film
response curve for photographic look development, and it would bake a tone curve into an image the
runtime multiplies by a sky colour — a white leaf would arrive at about 0.8 and every tint would
compound the curve a second time.

## The atlas

Eight slices of 512 × 1024 fill a 2048² atlas exactly, in a 4 × 2 grid, yaw 0 at the top left and
increasing left to right then downward. 512 × 1024 rather than 724² because the subjects are trees
and gable ends: taller than they are wide, and a square cell would spend half its texels on empty
sky.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import json
import math
import os
import sys
from array import array

# --------------------------------------------------------------------------------------------
# Re-exec under Blender when run as a plain script.
# --------------------------------------------------------------------------------------------
try:
    import bpy  # type: ignore

    INSIDE_BLENDER = True
except ImportError:  # pragma: no cover - the relauncher path
    INSIDE_BLENDER = False

if not INSIDE_BLENDER:
    from pathlib import Path

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import blender_env  # noqa: E402

    sys.exit(blender_env.relaunch(Path(__file__).resolve(), sys.argv[1:], tool="impostor_render"))


# --------------------------------------------------------------------------------------------
# Everything below runs inside Blender.
# --------------------------------------------------------------------------------------------
from mathutils import Vector  # type: ignore  # noqa: E402

#: §26.3's eight yaws.
YAWS = 8
ATLAS = 2048
COLUMNS = 4
ROWS = 2
SLICE_WIDTH = ATLAS // COLUMNS      # 512
SLICE_HEIGHT = ATLAS // ROWS        # 1024

#: The camera's elevation above horizontal. A tree at §26.1's 8 px of screen height is 400 m away
#: and a metre and a half of eye height is 0.2 degrees of depression -- so the honest elevation for
#: an impostor seen from the ground is a few degrees, not the 30 an authoring tool defaults to. Ten
#: keeps a little of the canopy's top visible where the ground rises toward the neighbourhood.
ELEVATION_DEGREES = 10.0

#: A margin around the subject's bounds, as a fraction. Not zero: an orthographic frame that
#: touches the silhouette clips the antialiased edge, and a clipped edge on a billboard is a hard
#: line against the sky.
FRAME_MARGIN = 0.04

#: EEVEE samples. 32 is well past the point where a flat-lit subject stops changing, and the
#: selftest checks that two renders are byte-identical rather than trusting that.
SAMPLES = 32

#: The uniform world. White at strength 1 so the atlas carries albedo x ambient occlusion, in
#: 0..1, ready to be multiplied by a sky colour at runtime.
WORLD_STRENGTH = 1.0


def report(message: str) -> None:
    print(f"  {message}")


def _scene_bounds(objects) -> tuple[Vector, Vector]:
    low = Vector((float("inf"),) * 3)
    high = Vector((float("-inf"),) * 3)
    for obj in objects:
        for corner in obj.bound_box:
            point = obj.matrix_world @ Vector(corner)
            for axis in range(3):
                low[axis] = min(low[axis], point[axis])
                high[axis] = max(high[axis], point[axis])
    return low, high


def _mesh_objects():
    return [o for o in bpy.data.objects if o.type == "MESH" and len(o.data.polygons) > 0]


def setup_world() -> None:
    """A uniform white dome and NO sun. See the module docstring: a baked direction would fight
    the runtime's sky tint and light the subject from the wrong side most of the day."""
    world = bpy.data.worlds.new("Overcast")
    bpy.context.scene.world = world
    world.use_nodes = True
    background = world.node_tree.nodes["Background"]
    background.inputs[0].default_value = (1.0, 1.0, 1.0, 1.0)
    background.inputs[1].default_value = WORLD_STRENGTH
    for obj in list(bpy.data.objects):
        if obj.type == "LIGHT":
            bpy.data.objects.remove(obj, do_unlink=True)


def setup_render() -> None:
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = SLICE_WIDTH
    scene.render.resolution_y = SLICE_HEIGHT
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.render.image_settings.compression = 100
    # STANDARD, not AgX. A film response curve baked into an image the runtime multiplies by a sky
    # colour would compound the curve twice and land a white leaf at about 0.8.
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0
    if hasattr(scene, "eevee"):
        scene.eevee.taa_render_samples = SAMPLES
        if hasattr(scene.eevee, "use_motion_blur"):
            scene.eevee.use_motion_blur = False


def make_camera(low: Vector, high: Vector) -> tuple[object, dict]:
    """One orthographic camera, framing the subject's bounds identically at every yaw.

    ORTHOGRAPHIC, and that is not a style choice: a perspective camera at a fixed distance gives a
    different projected size for a near branch than for a far one, so the eight slices would
    disagree about how wide the subject is and the quad would appear to breathe as the camera
    circles it. The runtime quad is a rectangle of a known world size, which is exactly what an
    orthographic render produces.
    """
    size = high - low
    centre = (high + low) * 0.5
    # The frame must hold the subject at EVERY yaw, so its half-width is the bounding CIRCLE in
    # plan, not the box's own half-width -- otherwise the corners clip at 45 degrees.
    #
    # EVERYTHING HERE IS IN BLENDER'S Z-UP, and that is not a detail. The glTF importer converts
    # a Y-up source to Z-up on the way in, and `Vector.to_track_quat` keeps an object upright
    # against world +Z. Working in the source's Y-up instead -- which the first version of this
    # tool did, including in its own fixture -- rolls the camera 90 degrees at every yaw that is
    # not on an axis: yaws 0 and 180 framed correctly and the other six came out lying on their
    # side, stretched across the full width of the cell and squashed to half its height. The
    # silhouette AREA barely changed, so a coverage check saw nothing.
    half_plan = math.hypot(size.x, size.y) * 0.5
    elevation = math.radians(ELEVATION_DEGREES)
    # Looking down by `elevation` tilts depth into height, so the projected half-height is the
    # subject's own plus the plan radius leaning into it. Omitting that term clips the top of a
    # deep subject and nothing else, which reads as a badly modelled tree.
    half_height = (size.z * 0.5 * math.cos(elevation) + half_plan * math.sin(elevation))

    # MEASURED, and the bug it caused was invisible: Blender's `ortho_scale` is the view size along
    # the LARGER image dimension, which for a 512x1024 slice is the HEIGHT. Setting it to the
    # required width made the frame 4.37 m TALL for a 5.40 m subject -- so the tree was cropped at
    # the top, every slice still carried a plausible silhouette, and only "is anything touching the
    # cell border" caught it.
    frame_height = max(half_height * 2.0,
                       half_plan * 2.0 * SLICE_HEIGHT / SLICE_WIDTH) * (1.0 + FRAME_MARGIN)
    ortho_scale = frame_height

    data = bpy.data.cameras.new("impostor")
    data.type = "ORTHO"
    data.ortho_scale = ortho_scale
    data.clip_start = 0.01
    data.clip_end = max(size.length * 8.0, 100.0)
    camera = bpy.data.objects.new("impostor", data)
    bpy.context.collection.objects.link(camera)
    bpy.context.scene.camera = camera

    return camera, {
        "centre": [round(v, 6) for v in centre],
        "boundsMin": [round(v, 6) for v in low],
        "boundsMax": [round(v, 6) for v in high],
        "worldWidth": round(frame_height * SLICE_WIDTH / SLICE_HEIGHT, 6),
        "worldHeight": round(frame_height, 6),
        "planRadius": round(half_plan, 6),
    }


def place_camera(camera, centre: Vector, distance: float, yaw_degrees: float) -> None:
    yaw = math.radians(yaw_degrees)
    elevation = math.radians(ELEVATION_DEGREES)
    # Blender's Z-up: the yaw circles in XY and the elevation lifts in +Z.
    offset = Vector((math.sin(yaw) * math.cos(elevation),
                     -math.cos(yaw) * math.cos(elevation),
                     math.sin(elevation))) * distance
    camera.location = centre + offset
    # The camera looks down its own -Z with its local +Y kept toward world +Z, which is what
    # `to_track_quat` means by "up" -- and world +Z is up only because everything here is in
    # Blender's convention. See `make_camera`.
    direction = -offset.normalized()
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def render_slices(directory: str, name: str) -> tuple[list[str], dict]:
    objects = _mesh_objects()
    if not objects:
        raise RuntimeError("the file has no mesh with faces")
    setup_world()
    setup_render()
    low, high = _scene_bounds(objects)
    camera, frame = make_camera(low, high)
    centre = Vector(frame["centre"])
    distance = max((high - low).length * 2.0, 10.0)

    paths = []
    for index in range(YAWS):
        place_camera(camera, centre, distance, index * (360.0 / YAWS))
        path = os.path.join(directory, f"{name}_yaw{index}.png")
        bpy.context.scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        paths.append(path)
    return paths, frame


def compose(paths: list[str], destination: str) -> dict:
    """The eight slices into one 2048² atlas, 4 x 2, yaw 0 at the top left.

    Composed through Blender's own image buffers with `foreach_get`/`foreach_set`, which is C-speed
    and needs no numpy -- Blender's bundled interpreter here has none, and a 2048² RGBA image is
    16.7 million floats, which a Python list cannot hold cheaply.
    """
    atlas = bpy.data.images.new(os.path.basename(destination), ATLAS, ATLAS, alpha=True)
    buffer = array("f", bytes(ATLAS * ATLAS * 4 * 4))

    for index, path in enumerate(paths):
        image = bpy.data.images.load(path)
        if tuple(image.size) != (SLICE_WIDTH, SLICE_HEIGHT):
            raise RuntimeError(f"{path} is {image.size[0]}x{image.size[1]}, not "
                               f"{SLICE_WIDTH}x{SLICE_HEIGHT}")
        slice_pixels = array("f", bytes(SLICE_WIDTH * SLICE_HEIGHT * 4 * 4))
        image.pixels.foreach_get(slice_pixels)

        column = index % COLUMNS
        row = index // COLUMNS
        # Blender's image origin is BOTTOM-left, so "yaw 0 at the top left" is the LAST row of
        # cells in buffer order. Getting this wrong produces an atlas that looks correct and whose
        # slice indices are upside down, which no pixel check would notice.
        x0 = column * SLICE_WIDTH
        y0 = (ROWS - 1 - row) * SLICE_HEIGHT
        for y in range(SLICE_HEIGHT):
            source = y * SLICE_WIDTH * 4
            target = ((y0 + y) * ATLAS + x0) * 4
            buffer[target:target + SLICE_WIDTH * 4] = \
                slice_pixels[source:source + SLICE_WIDTH * 4]
        bpy.data.images.remove(image)

    atlas.pixels.foreach_set(buffer)
    atlas.filepath_raw = destination
    atlas.file_format = "PNG"
    atlas.save()
    return {"file": os.path.basename(destination), "size": ATLAS,
            "columns": COLUMNS, "rows": ROWS,
            "sliceWidth": SLICE_WIDTH, "sliceHeight": SLICE_HEIGHT}


def build(source: str | None, out: str, name: str) -> dict:
    if source is not None:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=source)
    os.makedirs(out, exist_ok=True)

    slices_dir = os.path.join(out, "_slices")
    os.makedirs(slices_dir, exist_ok=True)
    paths, frame = render_slices(slices_dir, name)
    atlas = compose(paths, os.path.join(out, f"{name}_impostor.png"))

    result = {
        "tool": "tools/blender/impostor_render.py",
        "formatVersion": 1,
        "name": name,
        "yaws": YAWS,
        "yawDegrees": [index * (360.0 / YAWS) for index in range(YAWS)],
        "elevationDegrees": ELEVATION_DEGREES,
        "atlas": atlas,
        "frame": frame,
        # THE statement §26.3 requires be explicit: the atlas is baked-lit, flat, and the runtime
        # tints it. Written into the metadata rather than only into a docstring, so a later reader
        # of the asset finds it without reading the tool.
        "lighting": {
            "model": "uniform white dome, no directional light",
            "worldStrength": WORLD_STRENGTH,
            "viewTransform": "Standard",
            "bakedLit": True,
            "note": "Deliberately baked-lit and deliberately DIRECTIONLESS. cna-house.md §26.3 "
                    "tints the impostor quad by the sky colour at runtime; a baked sun would "
                    "fight that tint and light the subject from the wrong side for most of the "
                    "day. What the atlas carries is albedo x ambient occlusion.",
        },
        "samples": SAMPLES,
        "engine": bpy.context.scene.render.engine,
    }
    with open(os.path.join(out, f"{name}_impostor.json"), "w", encoding="utf-8") as handle:
        json.dump(result, handle, indent=2, sort_keys=True)
        handle.write("\n")
    return result


# ------------------------------------------------------------------------------------- fixture ----

def build_tree_fixture() -> None:
    """An asymmetric subject, so a yaw that did not happen is visible.

    A trunk with a canopy pushed to one side and a single low branch on the other. Symmetry is the
    enemy of this fixture: a cone on a cylinder renders identically from all eight yaws, and every
    check below would pass on a tool that rendered the same view eight times.

    White, so the lighting check has something to measure: under a uniform dome every face of a
    white subject must come back at the same value, and under a sun they would not.
    """
    bpy.ops.wm.read_factory_settings(use_empty=True)
    material = bpy.data.materials.new("white")
    material.use_nodes = True
    bsdf = material.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (1.0, 1.0, 1.0, 1.0)
    bsdf.inputs["Roughness"].default_value = 1.0
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.0

    def box(name: str, centre, size) -> None:
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=centre)
        obj = bpy.context.active_object
        obj.name = name
        obj.scale = size
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        obj.data.materials.append(material)

    # Z-UP, which is Blender's convention and what the glTF importer converts a source into.
    box("trunk", (0.0, 0.0, 2.0), (0.35, 0.35, 4.0))
    # The canopy is OFF-CENTRE in +X and the branch reaches out in -Y, so no two of the eight yaws
    # see the same silhouette -- and, more to the point, no two see a MIRRORED one either.
    box("canopy", (0.9, 0.0, 4.4), (2.6, 1.8, 2.0))
    box("branch", (0.35, -1.3, 2.6), (0.25, 2.2, 0.25))


# ------------------------------------------------------------------------------------ selftest ----

def _load(path: str) -> tuple[int, int, array]:
    image = bpy.data.images.load(path)
    width, height = image.size
    pixels = array("f", bytes(width * height * 4 * 4))
    image.pixels.foreach_get(pixels)
    bpy.data.images.remove(image)
    return width, height, pixels


def _cell(pixels: array, index: int) -> tuple[float, float, int, bool]:
    """(alpha coverage, mean opaque luminance, silhouette height in rows, touches the border)."""
    column = index % COLUMNS
    row = index // COLUMNS
    x0 = column * SLICE_WIDTH
    y0 = (ROWS - 1 - row) * SLICE_HEIGHT
    covered = 0
    luminance = 0.0
    lowest = SLICE_HEIGHT
    highest = -1
    border = False
    for y in range(SLICE_HEIGHT):
        base = ((y0 + y) * ATLAS + x0) * 4
        for x in range(SLICE_WIDTH):
            alpha = pixels[base + x * 4 + 3]
            if alpha > 0.05:
                if y == 0 or y == SLICE_HEIGHT - 1 or x == 0 or x == SLICE_WIDTH - 1:
                    border = True
            if alpha > 0.9:
                covered += 1
                luminance += (pixels[base + x * 4] + pixels[base + x * 4 + 1]
                              + pixels[base + x * 4 + 2]) / 3.0
                lowest = min(lowest, y)
                highest = max(highest, y)
    fraction = covered / (SLICE_WIDTH * SLICE_HEIGHT)
    return (fraction, luminance / covered if covered else 0.0,
            (highest - lowest + 1) if highest >= 0 else 0, border)


def selftest() -> int:
    failures = 0

    def require(condition: bool, message: str) -> None:
        nonlocal failures
        if condition:
            print(f"  PASS  {message}")
        else:
            print(f"  FAIL  {message}")
            failures += 1

    import tempfile

    workspace = tempfile.mkdtemp(prefix="impostor_selftest_")
    build_tree_fixture()
    result = build(None, workspace, "tree")

    atlas_path = os.path.join(workspace, "tree_impostor.png")
    width, height, pixels = _load(atlas_path)
    require((width, height) == (ATLAS, ATLAS),
            f"the atlas is {width}x{height}")
    require(result["engine"] == "BLENDER_EEVEE_NEXT",
            f"rendered with {result['engine']}, which is §26.3's EEVEE")

    cells = [_cell(pixels, index) for index in range(YAWS)]

    # 1. Eight slices, every one of them carrying a subject.
    require(all(c[0] > 0.02 for c in cells),
            f"all {YAWS} slices carry a silhouette "
            f"({', '.join(f'{c[0] * 100:.1f}%' for c in cells)})")
    require(all(c[0] < 0.6 for c in cells),
            "...and none of them fills its cell, so the frame is not clipping the subject")

    # 2. The yaws are DIFFERENT. A tool that rendered the same view eight times passes every other
    #    check here, which is why the fixture is asymmetric.
    coverage = [round(c[0], 4) for c in cells]

    def differing(a: int, b: int) -> int:
        """Texels where two slices' alpha disagrees. AREA cannot distinguish a view from its
        MIRROR -- yaw 90 and yaw 270 of any subject have the same silhouette area and different
        silhouettes -- so the comparison is pixel by pixel."""
        count = 0
        ax, ay = (a % COLUMNS) * SLICE_WIDTH, (ROWS - 1 - a // COLUMNS) * SLICE_HEIGHT
        bx, by = (b % COLUMNS) * SLICE_WIDTH, (ROWS - 1 - b // COLUMNS) * SLICE_HEIGHT
        for y in range(0, SLICE_HEIGHT, 3):
            abase = ((ay + y) * ATLAS + ax) * 4
            bbase = ((by + y) * ATLAS + bx) * 4
            for x in range(0, SLICE_WIDTH, 3):
                if (pixels[abase + x * 4 + 3] > 0.5) != (pixels[bbase + x * 4 + 3] > 0.5):
                    count += 1
        return count

    pairs = [(i, j) for i in range(YAWS) for j in range(i + 1, YAWS)]
    same = [(i, j) for i, j in pairs if differing(i, j) < 200]
    require(not same,
            f"no two of the eight slices show the same silhouette (identical pairs: {same}); "
            f"coverage alone would not see it, because a view and its mirror have equal area "
            f"({coverage})")

    # 3. NOTHING IS CLIPPED. This is the check that earned its place: `ortho_scale` is the view
    #    size along the LARGER image dimension, so setting it to the required WIDTH made a
    #    512x1024 frame 4.37 m tall for a 5.40 m subject. Every slice still carried a plausible
    #    silhouette, the coverage still differed between yaws, the lighting was still flat -- and
    #    the tree was decapitated. Only "does anything touch the cell border" saw it.
    touching = [index for index, c in enumerate(cells) if c[3]]
    require(not touching,
            f"no slice's silhouette reaches its cell border, so nothing is cropped "
            f"(touching: {touching})")

    # 3b. The camera is one camera: the same orthographic scale for every yaw, so the quad does not
    #     appear to breathe as the camera circles it. Asserted from the CAMERA rather than from
    #     pixels, because an asymmetric subject legitimately projects to different pixel heights
    #     from different sides and a pixel test would be measuring the fixture's shape.
    camera = bpy.context.scene.camera
    require(camera.data.type == "ORTHO",
            f"the camera is orthographic ({camera.data.type}), so a near branch and a far one "
            f"project at the same scale")
    require(abs(camera.data.ortho_scale - result["frame"]["worldHeight"]) < 1e-4,
            f"the camera's ortho_scale {camera.data.ortho_scale:.4f} is the frame height the "
            f"metadata reports ({result['frame']['worldHeight']:.4f}) -- they are the same number "
            f"or the runtime quad is the wrong size")
    heights = [c[2] for c in cells]
    require(max(heights) < SLICE_HEIGHT * 0.95,
            f"the tallest silhouette is {max(heights)} of {SLICE_HEIGHT} rows, leaving margin")

    # 4. THE lighting claim, and the reason it is checked rather than asserted: under a uniform
    #    dome a white subject must come back at the same value from opposite yaws. Under a sun --
    #    the default an authoring tool gives you -- yaw 0 and yaw 180 would differ by a lot.
    front = cells[0][1]
    back = cells[YAWS // 2][1]
    require(abs(front - back) < 0.05,
            f"yaw 0 and yaw 180 have the same mean luminance ({front:.3f} vs {back:.3f}), so "
            f"nothing directional is baked in")
    spread = max(c[1] for c in cells) - min(c[1] for c in cells)
    require(spread < 0.08,
            f"...and all eight agree to {spread:.3f}, so the atlas can be tinted at runtime")

    # 5. `view_transform` is Standard, so a white surface arrives near 1.0. Under AgX it would land
    #    near 0.8 and every runtime tint would compound the curve a second time.
    require(front > 0.85,
            f"a white subject renders at {front:.3f}, not the ~0.8 a film response curve gives")
    require(result["lighting"]["viewTransform"] == "Standard"
            and result["lighting"]["bakedLit"] is True,
            "the metadata says plainly that the atlas is baked-lit and how")

    # 6. The alpha is a SILHOUETTE: opaque inside, transparent outside, and hardly anything in
    #    between beyond the antialiased edge.
    partial = 0
    opaque = 0
    for index in range(0, ATLAS * ATLAS, 37):     # a stride, so this is a sample and not an hour
        alpha = pixels[index * 4 + 3]
        if alpha > 0.9:
            opaque += 1
        elif alpha > 0.1:
            partial += 1
    require(opaque > 0 and partial < opaque * 0.2,
            f"the alpha is a silhouette rather than a soft mask ({opaque} opaque, {partial} "
            f"partial in the sample)")

    # 7. The frame metadata describes a quad the runtime can build: the world size the atlas covers.
    frame = result["frame"]
    require(abs(frame["worldHeight"] - frame["worldWidth"] * 2.0) < 1e-3,
            f"the frame is 1:2, matching the 512x1024 slice "
            f"({frame['worldWidth']:.2f} x {frame['worldHeight']:.2f} m)")
    height_m = frame["boundsMax"][2] - frame["boundsMin"][2]
    require(frame["worldHeight"] >= height_m,
            f"the frame is tall enough for the {height_m:.2f} m subject "
            f"({frame['worldHeight']:.2f} m)")

    # 8. Repeatability, to the byte.
    second = tempfile.mkdtemp(prefix="impostor_selftest2_")
    build_tree_fixture()
    build(None, second, "tree")
    with open(atlas_path, "rb") as a, open(os.path.join(second, "tree_impostor.png"), "rb") as b:
        require(a.read() == b.read(), "two runs produce a byte-identical atlas")

    # 9. Slice 0 really is at the top left. Blender's image origin is bottom-left, so this is the
    #    off-by-one that produces an atlas which looks right and whose indices are upside down.
    top_left = _cell(pixels, 0)
    bottom_left = _cell(pixels, COLUMNS)
    require(top_left[0] != bottom_left[0],
            f"the top-left cell and the cell below it hold different yaws "
            f"({top_left[0]:.4f} vs {bottom_left[0]:.4f})")

    print(f"impostor_render: {'FAILED' if failures else 'selftest passed'}.")
    return 1 if failures else 0


def main() -> int:
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if "--selftest" in arguments:
        return selftest()

    positional = [a for a in arguments if not a.startswith("--")]
    options = {a.split("=", 1)[0]: a.split("=", 1)[1] for a in arguments
               if a.startswith("--") and "=" in a}
    if len(positional) != 2:
        print(__doc__.splitlines()[0], file=sys.stderr)
        print("usage: impostor_render.py IN.glb OUT_DIR [--name=<stem>]", file=sys.stderr)
        return 2

    source, out = positional
    name = options.get("--name", os.path.splitext(os.path.basename(source))[0])
    result = build(source, out, name)
    report(f"{name}: {result['yaws']} yaws at {result['elevationDegrees']:.0f} degrees elevation "
           f"into {result['atlas']['size']}² "
           f"({result['frame']['worldWidth']:.2f} x {result['frame']['worldHeight']:.2f} m per "
           f"slice, {result['engine']}, {result['samples']} samples)")
    report(f"lighting: {result['lighting']['model']}, view transform "
           f"{result['lighting']['viewTransform']} -- baked-lit and tinted at runtime (§26.3)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
