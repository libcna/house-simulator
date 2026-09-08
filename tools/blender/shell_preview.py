#!/usr/bin/env python3
"""shell_preview.py -- a picture of the blockout, and of what is lightmapped in it.

`HOUSE-00471`'s visual validation. UV statistics say the receivers were unwrapped and the detail
was not; they do not say the result looks right. This renders the generated shell twice from the
same camera:

* **`--mode blockout`** — the placeholder materials of `HOUSE-00470`, so a floor, a ceiling, a wall,
  the trim and the glass are told apart at a glance;
* **`--mode receivers`** — every lightmap receiver in one colour and every dynamically lit detail
  in another, so the decision `HOUSE-00471` implements is a thing you can look at.

    tools/blender/shell_preview.py --cell L0_HALL
    tools/blender/shell_preview.py --cell L0_HALL --mode receivers
    tools/blender/shell_preview.py --selftest

Renders headlessly with EEVEE and **no display**: `blender_env.py` removes `DISPLAY` from the
child's environment, so nothing appears on anybody's screen.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import os
import sys

try:
    import bpy  # type: ignore

    INSIDE_BLENDER = True
except ImportError:
    INSIDE_BLENDER = False

if not INSIDE_BLENDER:
    from pathlib import Path

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import blender_env  # noqa: E402

    sys.exit(blender_env.relaunch(Path(__file__).resolve(), sys.argv[1:], tool="shell_preview"))

import argparse  # noqa: E402
import math  # noqa: E402
from pathlib import Path  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import house_shell_gen  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SHELL = REPO / "build" / "shell"
OUTPUT = REPO / "docs" / "blockout"

def png_colours(path: Path) -> list[tuple[bytes, float]]:
    """The most common colours in a PNG, as `(rgb, share)`, sampled on a coarse grid.

    A picture nobody looks at is not validation, and a picture that is uniformly black looks
    exactly like a picture of an unlit room. This is what turns "a file was written" into "the
    render shows something": it decodes the PNG -- `zlib` plus the five filters, no dependency --
    and reports what is actually in it.
    """
    import zlib  # noqa: PLC0415

    data = path.read_bytes()
    index, idat, width, height, colour = 8, b"", 0, 0, 6
    while index < len(data):
        length = int.from_bytes(data[index:index + 4], "big")
        tag = data[index + 4:index + 8]
        if tag == b"IHDR":
            width = int.from_bytes(data[index + 8:index + 12], "big")
            height = int.from_bytes(data[index + 12:index + 16], "big")
            colour = data[index + 17]
        elif tag == b"IDAT":
            idat += data[index + 8:index + 8 + length]
        index += 12 + length
    channels = {0: 1, 2: 3, 4: 2, 6: 4}[colour]
    raw = zlib.decompress(idat)
    stride = width * channels
    counts: dict[bytes, int] = {}
    previous = bytearray(stride)
    at = 0
    for _row in range(height):
        filtered = raw[at]
        at += 1
        line = bytearray(raw[at:at + stride])
        at += stride
        for position in range(stride):
            left = line[position - channels] if position >= channels else 0
            up = previous[position]
            corner = previous[position - channels] if position >= channels else 0
            if filtered == 1:
                line[position] = (line[position] + left) & 0xFF
            elif filtered == 2:
                line[position] = (line[position] + up) & 0xFF
            elif filtered == 3:
                line[position] = (line[position] + (left + up) // 2) & 0xFF
            elif filtered == 4:
                estimate = left + up - corner
                deltas = (abs(estimate - left), abs(estimate - up), abs(estimate - corner))
                nearest = left if deltas[0] <= deltas[1] and deltas[0] <= deltas[2] else (
                    up if deltas[1] <= deltas[2] else corner)
                line[position] = (line[position] + nearest) & 0xFF
        for column in range(0, width, 4):
            key = bytes(line[column * channels:column * channels + 3])
            counts[key] = counts.get(key, 0) + 1
        previous = line
    total = sum(counts.values()) or 1
    return sorted(((key, value / total) for key, value in counts.items()),
                  key=lambda pair: -pair[1])


#: The two colours the receiver view uses. Deliberately unlike each other and unlike the blockout's
#: palette, because this image is read for one thing only: which surfaces take a bake.
RECEIVER_COLOUR = (0.20, 0.55, 0.95, 1.0)
DETAIL_COLOUR = (0.95, 0.45, 0.15, 1.0)

RESOLUTION = (960, 540)


def load(cells: list[str]) -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for name in cells:
        path = SHELL / f"{name}.glb"
        if not path.is_file():
            raise RuntimeError(f"{path} is not there; run tools/blender/house_shell_gen.py first")
        bpy.ops.import_scene.gltf(filepath=str(path))


def shade(material, colour) -> None:
    """Give @p material a Principled BSDF of @p colour, so EEVEE renders it.

    `diffuse_color` alone is the **viewport** colour: the generator sets it because that is what
    the glTF exporter writes as `baseColorFactor`, and a render of materials with `use_nodes` off
    comes out black. The first version of this tool produced two identical black images and the
    claim that the render is not uniform is what said so.
    """
    material.use_nodes = True
    material.diffuse_color = colour
    tree = material.node_tree
    principled = next((node for node in tree.nodes if node.type == "BSDF_PRINCIPLED"), None)
    if principled is None:
        principled = tree.nodes.new("ShaderNodeBsdfPrincipled")
        output = next((node for node in tree.nodes if node.type == "OUTPUT_MATERIAL"), None)
        if output is None:
            output = tree.nodes.new("ShaderNodeOutputMaterial")
        tree.links.new(principled.outputs["BSDF"], output.inputs["Surface"])
    principled.inputs["Base Color"].default_value = colour
    if "Roughness" in principled.inputs:
        principled.inputs["Roughness"].default_value = 0.85
    material.blend_method = "BLEND" if colour[3] < 1.0 else "OPAQUE"


def paint_blockout() -> int:
    """Give every material its §11-class placeholder colour as a shader. Returns the count."""
    painted = 0
    for material in bpy.data.materials:
        klass = str(material.get("surfaceClass") or material.name).replace("BLOCKOUT_", "")
        shade(material, house_shell_gen.SURFACE_COLOURS.get(klass, (0.8, 0.8, 0.8, 1.0)))
        painted += 1
    return painted


def paint_by_receiver() -> tuple[int, int]:
    """Recolour every material by whether it is a lightmap receiver. Returns `(receiver, detail)`
    material counts."""
    receiver = detail = 0
    for material in bpy.data.materials:
        klass = str(material.get("surfaceClass") or material.name).replace("BLOCKOUT_", "")
        is_receiver = klass in house_shell_gen.LIGHTMAP_RECEIVERS
        shade(material, RECEIVER_COLOUR if is_receiver else DETAIL_COLOUR)
        receiver += 1 if is_receiver else 0
        detail += 0 if is_receiver else 1
    return receiver, detail


def frame_everything(elevation: float = 0.45, azimuth: float = 0.9) -> None:
    """A camera that sees all of the loaded geometry, and a sun to light it.

    Framed from the geometry's own bounds rather than from a remembered position, so the same
    script frames one cell or the whole house without a second set of numbers.
    """
    lows = [1e9, 1e9, 1e9]
    highs = [-1e9, -1e9, -1e9]
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        for corner in obj.bound_box:
            point = obj.matrix_world @ __import__("mathutils").Vector(corner)
            for axis in range(3):
                lows[axis] = min(lows[axis], point[axis])
                highs[axis] = max(highs[axis], point[axis])
    centre = [(lows[axis] + highs[axis]) / 2.0 for axis in range(3)]
    span = max(highs[axis] - lows[axis] for axis in range(3)) or 1.0
    distance = span * 1.9

    camera_data = bpy.data.cameras.new("preview")
    camera = bpy.data.objects.new("preview", camera_data)
    bpy.context.scene.collection.objects.link(camera)
    import mathutils  # noqa: PLC0415

    camera.location = (centre[0] + distance * math.cos(azimuth) * math.cos(elevation),
                       centre[1] + distance * math.sin(azimuth) * math.cos(elevation),
                       centre[2] + distance * math.sin(elevation))
    # `to_track_quat` rather than three hand-built Euler terms: a camera looks down its local −Z
    # with +Y up, and the hand-built version pointed at the sky -- three renders of 42 % background
    # and no room in them, which is what the "not mostly background" claim now refuses.
    towards = mathutils.Vector(centre) - mathutils.Vector(camera.location)
    camera.rotation_euler = towards.to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = camera

    sun_data = bpy.data.lights.new("sun", type="SUN")
    sun_data.energy = 3.0
    sun = bpy.data.objects.new("sun", sun_data)
    sun.rotation_euler = (math.radians(50.0), 0.0, math.radians(35.0))
    bpy.context.scene.collection.objects.link(sun)

    # A grey sky, so a surface facing away from the sun is dark rather than black. Without it the
    # inside of a room reads as a silhouette and the picture says nothing about the walls.
    world = bpy.data.worlds.new("preview")
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    if background is not None:
        background.inputs[0].default_value = (0.35, 0.38, 0.42, 1.0)
        background.inputs[1].default_value = 1.0
    bpy.context.scene.world = world


def render(destination: Path) -> None:
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x, scene.render.resolution_y = RESOLUTION
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(destination)
    scene.display_settings.display_device = "sRGB"
    destination.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.render.render(write_still=True)


# ============================================================================== selftest =========


def selftest() -> int:
    failures = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("shell_preview: selftest")
    if not (SHELL / "L0_HALL.glb").is_file():
        require(False, "the shell is generated; run tools/blender/house_shell_gen.py first")
    else:
        load(["L0_HALL"])
        receiver, detail = paint_by_receiver()
        require(receiver > 0 and detail > 0,
                f"the hall has both receiver and detail materials to tell apart "
                f"({receiver}, {detail})")
        require(RECEIVER_COLOUR != DETAIL_COLOUR,
                "and the two colours are different, which is the entire content of the image")
        frame_everything()
        require(bpy.context.scene.camera is not None, "a camera is framed from the geometry")
        target = OUTPUT / "selftest.png"
        render(target)
        written = target.stat().st_size if target.is_file() else 0
        require(written > 4096, f"and EEVEE writes a picture headlessly ({written} bytes)")

        # The claim that matters: the picture has a picture in it. The first version of this tool
        # wrote two 250 KB files that were 100 % black, because `diffuse_color` is the viewport's
        # colour and EEVEE renders the node tree.
        common = png_colours(target)
        require(common[0][1] < 0.90,
                f"and the render is not one flat colour ({common[0][0].hex()} at "
                f"{common[0][1] * 100:.0f} %)")
        # The background is one flat colour, so anything close to it is sky rather than house. A
        # frame that is mostly background is a camera pointed at nothing, which is exactly what the
        # first version of `frame_everything` produced.
        sky = (89, 97, 104)
        background = sum(share for colour, share in common
                         if max(abs(colour[index] - sky[index]) for index in range(3)) < 12)
        require(background < 0.60,
                f"...and it is a picture of the house rather than of the sky "
                f"({background * 100:.0f} % background)")

        # And it shows the distinction it exists to show: blue receivers and orange detail, both
        # of them, in one frame of one room. An image with only one of the two would be a picture
        # of a decision nobody made.
        blue = sum(share for colour, share in common if colour[2] > colour[0] + 20)
        orange = sum(share for colour, share in common if colour[0] > colour[2] + 25)
        require(blue > 0.30 and orange > 0.02,
                f"and both classes are in the frame: {blue * 100:.0f} % receiver, "
                f"{orange * 100:.0f} % detail")
        target.unlink(missing_ok=True)

    if failures:
        print(f"shell_preview: {len(failures)} claim(s) FAILED")
        return 1
    print("shell_preview: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--cell", default="", help="comma-separated cell ids; default is all")
    parser.add_argument("--mode", choices=("blockout", "receivers"), default="blockout")
    parser.add_argument("--out", type=Path, default=None)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])

    if args.selftest:
        return selftest()

    names = [name.strip() for name in args.cell.split(",") if name.strip()]
    if not names:
        names = sorted(path.stem for path in SHELL.glob("*.glb"))
    load(names)
    if args.mode == "receivers":
        receiver, detail = paint_by_receiver()
        print(f"shell_preview: {receiver} receiver material(s), {detail} detail")
    else:
        print(f"shell_preview: {paint_blockout()} placeholder material(s)")
    frame_everything()
    leaf = f"{args.mode}-{names[0] if len(names) == 1 else 'house'}.png"
    # `--out` names a file, or a DIRECTORY to put the usual name in. Passing the directory is the
    # obvious thing to try and used to render into a path that was already one, silently.
    destination = OUTPUT / leaf if args.out is None else (
        args.out / leaf if args.out.is_dir() else args.out)
    render(destination)
    try:
        shown = destination.resolve().relative_to(REPO)
    except ValueError:
        shown = destination
    print(f"shell_preview: {shown}")
    return 0


if __name__ == "__main__":
    try:
        _status = main()
    except SystemExit as _exit:
        _status = 1 if isinstance(_exit.code, str) else int(_exit.code or 0)
    except BaseException:  # noqa: BLE001
        import traceback

        traceback.print_exc()
        print("shell_preview: EXIT 1")
        raise
    print(f"shell_preview: EXIT {_status}")
    sys.exit(_status)
