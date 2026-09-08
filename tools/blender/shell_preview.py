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


def paint_by_receiver() -> tuple[int, int]:
    """Recolour every material by whether it is a lightmap receiver. Returns `(receiver, detail)`
    material counts."""
    receiver = detail = 0
    for material in bpy.data.materials:
        klass = str(material.get("surfaceClass") or material.name).replace("BLOCKOUT_", "")
        is_receiver = klass in house_shell_gen.LIGHTMAP_RECEIVERS
        material.use_nodes = False
        material.diffuse_color = RECEIVER_COLOUR if is_receiver else DETAIL_COLOUR
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
    camera.location = (centre[0] + distance * math.cos(azimuth) * math.cos(elevation),
                       centre[1] + distance * math.sin(azimuth) * math.cos(elevation),
                       centre[2] + distance * math.sin(elevation))
    direction = [centre[axis] - camera.location[axis] for axis in range(3)]
    horizontal = math.sqrt(direction[0] ** 2 + direction[1] ** 2)
    camera.rotation_euler = (math.atan2(horizontal, direction[2]), 0.0,
                             math.atan2(direction[1], direction[0]) + math.pi / 2.0)
    bpy.context.scene.camera = camera

    sun_data = bpy.data.lights.new("sun", type="SUN")
    sun_data.energy = 3.0
    sun = bpy.data.objects.new("sun", sun_data)
    sun.rotation_euler = (math.radians(50.0), 0.0, math.radians(35.0))
    bpy.context.scene.collection.objects.link(sun)


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
    frame_everything()
    destination = args.out or (OUTPUT / f"{args.mode}-{names[0] if len(names) == 1 else 'house'}.png")
    render(destination)
    print(f"shell_preview: {destination.relative_to(REPO)}")
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
