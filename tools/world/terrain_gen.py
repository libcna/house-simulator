#!/usr/bin/env python3
"""terrain_gen.py -- §11.5's height field and its material index, generated from the layout.

`HOUSE-00761`. The ground the house stands on: 81 x 65 samples on a 1.0 m grid over §10.3's
playable area, as a 16-bit greyscale PNG, plus an 8-bit material index over the same grid.
How to read them is `layout.exterior.json`'s `terrain` block, and `--check` proves the two agree.

    tools/world/terrain_gen.py --emit
    tools/world/terrain_gen.py --check
    tools/world/terrain_gen.py --selftest

## Why it is generated and not painted

§10.2 gives the slope in one sentence -- "+0.15 at the front property line falling to -0.35 at the
rear fence" -- and §11.6 gives the terrace, the shed pad and the driveway as flat things at stated
heights. Painting that by hand would be re-deriving numbers the layout already holds, and the first
time a terrace moved the ground under it would not. So the surfaces come from
`layout.exterior.json`'s `paths` and `structures` and from the cells themselves, and the slope is
the one line of arithmetic §10.2 states.

The **material index** is the same argument: §11.5 wants grass, worn lawn, soil, gravel, concrete,
asphalt, bluestone and mulch, and every one of those is already a `paths` row's material or a
cell's `footstepSurface`. Reading them is how the ground and the footsteps agree without anybody
maintaining two lists.

## The encoding

A 16-bit greyscale PNG because §11.5 says so: it is lossless, every tool opens it, and it diffs as
a binary blob that is regenerated rather than edited. Sample 0 is `terrain.origin`'s Y and 65535 is
that plus `terrain.yScale`.

There is deliberately **no sidecar**. A `terrain.json` beside the images would be a second place
that says how big the grid is, it is not a world file so `deploy_world.py` would not copy it and
`world.manifest.json` would not hash it, and the runtime would be reading its geometry from a file
nobody checks. The block the game already loads is the one that says how to read the images, and
`contract()` refuses to let it drift from the generator.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_io  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "assets-src" / "world"

#: §11.5: a 1.0 m grid over §10.3's playable area, X -40..+40 and Z -52..+12.
WIDTH, HEIGHT = 81, 65
ORIGIN_X, ORIGIN_Z = -40.0, -52.0
STEP = 1.0

#: The encoding: sample 0 is `originY`, 65535 is `originY + yScale`. A 6 m range over a
#: lot that moves 0.5 m is deliberate headroom -- a bank or a swale added later re-encodes nothing.
ORIGIN_Y, Y_SCALE = -3.0, 6.0

#: §11.5's eight classes, in the order the index uses. The index is the position in this list.
MATERIALS = ["grass", "lawn_worn", "soil", "gravel", "concrete", "asphalt", "bluestone", "mulch"]

#: A `paths` row's material maps to one of the eight. Anything unlisted stays grass, which is what
#: the lot is.
BY_MATERIAL = {
    "MAT_BLUESTONE_PAVER": "bluestone",
    "MAT_CONCRETE_BROOM": "concrete",
    "MAT_CONCRETE_SLAB": "concrete",
    "MAT_CONCRETE_KERB": "concrete",
    "MAT_ASPHALT_01": "asphalt",
    "MAT_GRAVEL_PATH": "gravel",
    "MAT_GROUND_LAWN": "grass",
}


def ground_at(x: float, z: float) -> float:
    """§10.2's slope: +0.15 at the front property line (Z 0) falling to -0.35 at the rear fence.

    Linear between them, flat beyond the property in both directions, because §10.4's terrain
    "rises gently and is planted" outside the fence and nothing there is walked on closely enough
    for a curve to be worth the arithmetic.
    """
    front, rear = 0.0, -48.0
    if z >= front:
        return 0.15
    if z <= rear:
        return -0.35
    t = (front - z) / (front - rear)
    return 0.15 + (-0.35 - 0.15) * t


def surfaces(directory: Path):
    """`(boxes, height, material)` for every flat thing the layout declares, outermost first."""
    layout = layout_io.load_layout(directory)
    exterior = layout.get("exterior") or {}
    out = []
    for row in exterior.get("paths", []):
        name = BY_MATERIAL.get(row.get("material"), "grass")
        out.append((row["boxes"], row.get("y"), name))
    # The carriageway is asphalt between the kerbs, and the road row gives a centreline and a
    # width rather than a box.
    road = exterior.get("road") or {}
    if road.get("centreline") and road.get("width"):
        half = float(road["width"]) / 2.0
        z = float(road["centreline"][0][2])
        out.append(([{"x": [-40.0, 40.0], "z": [z - half, z + half]}], 0.0,
                    BY_MATERIAL.get(road.get("material"), "asphalt")))
    # A structure's pad is flat under it: §11.6 puts the shed on one.
    for row in exterior.get("structures", []):
        out.append(([row["footprint"]], None, "gravel"))
    # An outdoor cell with a floor of its own is a deck or a terrace, and the ground under it is
    # flat at that height. `EXT_TERRACE` at +0.45 is the case §11.6 names.
    levels = {row["id"]: row for row in layout_io.rows(layout, "levels")}
    for cell in layout_io.rows(layout, "cells"):
        if cell.get("kind") != "exterior" or cell["id"] == "EXT_WORLD":
            continue
        override = cell.get("yOverride")
        if not override or abs(float(override[0])) < 1e-6:
            continue
        floor = float(override[0])
        if floor < 0.0:
            continue          # a window well is a hole, not a pad
        surface = cell.get("footstepSurface") or "grass"
        name = surface if surface in MATERIALS else "bluestone"
        out.append((cell["boxes"], floor, name))
    return out


def fields(directory: Path):
    """`(heights, materials)` as row-major lists of length WIDTH*HEIGHT."""
    flat = surfaces(directory)
    heights = []
    materials = []
    for row in range(HEIGHT):
        z = ORIGIN_Z + row * STEP
        for column in range(WIDTH):
            x = ORIGIN_X + column * STEP
            height = ground_at(x, z)
            material = "grass"
            for boxes, level, name in flat:
                if any(box["x"][0] <= x <= box["x"][1] and box["z"][0] <= z <= box["z"][1]
                       for box in boxes):
                    material = name
                    if level is not None:
                        height = float(level)
            heights.append(height)
            materials.append(MATERIALS.index(material))
    return heights, materials


def png(width: int, height: int, depth: int, rows: list[bytes]) -> bytes:
    """A greyscale PNG. Written here because the alternative is a dependency for 5 265 samples."""
    def chunk(tag: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + tag + payload
                + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    header = struct.pack(">IIBBBBB", width, height, depth, 0, 0, 0, 0)
    raw = b"".join(b"\x00" + row for row in rows)     # filter 0: none, on every row
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def rendered(directory: Path) -> dict[str, bytes | str]:
    heights, materials = fields(directory)
    span = max(1e-9, Y_SCALE)
    samples = [min(65535, max(0, round((value - ORIGIN_Y) / span * 65535.0)))
               for value in heights]
    height_rows = [b"".join(struct.pack(">H", samples[row * WIDTH + column])
                            for column in range(WIDTH)) for row in range(HEIGHT)]
    material_rows = [bytes(materials[row * WIDTH:(row + 1) * WIDTH]) for row in range(HEIGHT)]

    return {
        "terrain.png": png(WIDTH, HEIGHT, 16, height_rows),
        "terrain_materials.png": png(WIDTH, HEIGHT, 8, material_rows),
    }


def contract(directory: Path) -> list[str]:
    """Every way `layout.exterior.json`'s terrain block disagrees with what this generates.

    There is no sidecar. The block that the game reads is the one that says how to read the
    images, so the two cannot be in two files -- and a generator whose grid and a layout's grid
    differ silently is a terrain that samples the wrong metre.
    """
    layout = layout_io.load_layout(directory)
    return _contract_problems(((layout.get("exterior") or {}).get("terrain")) or {})


def _contract_problems(terrain: dict) -> list[str]:
    """`contract`, against a terrain block already in hand -- so the selftest can perturb one."""
    if not terrain:
        return ["layout.exterior.json declares no terrain block"]
    problems = []
    for field, want in (("samples", [WIDTH, HEIGHT]), ("step", STEP),
                        ("origin", [ORIGIN_X, ORIGIN_Y, ORIGIN_Z]), ("yScale", Y_SCALE),
                        ("materials", MATERIALS),
                        ("heightfield", "world/terrain.png"),
                        ("materialIndex", "world/terrain_materials.png")):
        got = terrain.get(field)
        if got != want:
            problems.append(f"layout.exterior.json terrain/{field} is {got!r}; the generator "
                            f"uses {want!r}")
    size = terrain.get("size")
    if size != [(WIDTH - 1) * STEP, (HEIGHT - 1) * STEP]:
        problems.append(f"layout.exterior.json terrain/size is {size!r}; "
                        f"{WIDTH} x {HEIGHT} samples a metre apart span "
                        f"{[(WIDTH - 1) * STEP, (HEIGHT - 1) * STEP]}")
    return problems


# ======================================================================================= selftest


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("terrain_gen: selftest")

    # 1. §10.2's slope, at the two points it names and one between.
    require(abs(ground_at(0.0, 0.0) - 0.15) < 1e-6,
            f"the front property line is +0.15 ({ground_at(0.0, 0.0):.3f})")
    require(abs(ground_at(0.0, -48.0) + 0.35) < 1e-6,
            f"the rear fence is -0.35 ({ground_at(0.0, -48.0):.3f})")
    require(abs(ground_at(0.0, -24.0) - (-0.10)) < 1e-6,
            f"and halfway is halfway ({ground_at(0.0, -24.0):.3f})")
    # ...and a QUARTER of the way, because halfway is the one point on the slope where running it
    # backwards gives the same answer, and a claim that cannot tell uphill from downhill is not a
    # claim about a slope.
    require(abs(ground_at(0.0, -12.0) - 0.025) < 1e-6,
            f"a quarter of the way down is +0.025, which says which way it falls "
            f"({ground_at(0.0, -12.0):.3f})")
    require(ground_at(0.0, 6.0) == ground_at(0.0, 0.0),
            "beyond the front line it is flat, not extrapolated uphill for ever")

    # 2. The grid is §11.5's, and it covers §10.3's playable area exactly.
    require((WIDTH, HEIGHT) == (81, 65), f"81 x 65 samples ({WIDTH} x {HEIGHT})")
    require(abs(ORIGIN_X + (WIDTH - 1) * STEP - 40.0) < 1e-9
            and abs(ORIGIN_Z + (HEIGHT - 1) * STEP - 12.0) < 1e-9,
            "the last sample lands on §10.3's far corner, not past it")

    if not (SOURCE / "layout.exterior.json").is_file():
        require(False, "layout.exterior.json exists to read the surfaces from")
    else:
        heights, materials = fields(SOURCE)
        require(len(heights) == WIDTH * HEIGHT == len(materials),
                f"one height and one material per sample ({len(heights)})")

        def at(x, z):
            column = round((x - ORIGIN_X) / STEP)
            row = round((z - ORIGIN_Z) / STEP)
            index = row * WIDTH + column
            return heights[index], MATERIALS[materials[index]]

        # 3. The flat things are flat, at the heights the layout gives them -- which is the whole
        #    reason this is generated rather than painted.
        require(at(0.0, -34.0) == (0.45, "bluestone"),
                f"the terrace is flat at +0.45 and paved ({at(0.0, -34.0)})")
        require(at(13.0, -6.0)[1] == "concrete",
                f"the driveway is concrete ({at(13.0, -6.0)})")
        require(at(0.0, 7.0)[1] == "asphalt", f"the carriageway is asphalt ({at(0.0, 7.0)})")
        require(at(0.0, -5.0)[1] == "bluestone",
                f"the front walk is bluestone ({at(0.0, -5.0)})")
        require(at(-18.0, -42.0)[1] == "gravel",
                f"the shed stands on a gravel pad ({at(-18.0, -42.0)})")
        require(at(-20.0, -20.0) == (ground_at(-20.0, -20.0), "grass"),
                f"and the side yard is grass on the slope ({at(-20.0, -20.0)})")

        # 4. The PNGs are PNGs, of the right shape and depth. A 16-bit height field written as
        #    8-bit would quantise the lot's 0.5 m of fall to two steps and nobody would see it in
        #    a diff.
        out = rendered(SOURCE)
        head = out["terrain.png"]
        require(head[:8] == b"\x89PNG\r\n\x1a\n", "the height field is a PNG")
        width, height_px, depth, colour = struct.unpack(">IIBB", head[16:26])
        require((width, height_px, depth, colour) == (WIDTH, HEIGHT, 16, 0),
                f"16-bit greyscale at 81 x 65 ({width}, {height_px}, {depth}, {colour})")
        index_head = out["terrain_materials.png"]
        require(struct.unpack(">IIBB", index_head[16:26]) == (WIDTH, HEIGHT, 8, 0),
                "and the material index is 8-bit greyscale at the same size")

        # 5. Deterministic, or `--check` is a gate that fails at random.
        require(rendered(SOURCE) == out, "the same layout renders the same bytes")

        # 6. The grid the generator uses and the grid the runtime is told to use are one grid.
        problems = contract(SOURCE)
        require(not problems, "layout.exterior.json describes this grid, not another"
                              + (f" ({problems[0]})" if problems else ""))
        stated = dict(layout_io.load_layout(SOURCE)["exterior"]["terrain"])
        stated["step"] = 0.5
        require(bool([p for p in _contract_problems(stated) if "step" in p]),
                "and a layout that halved the step would be caught")

    if failures:
        print(f"\nterrain_gen: {len(failures)} claim(s) FAILED")
        return 1
    print("terrain_gen: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", nargs="?", type=Path, default=SOURCE)
    parser.add_argument("--emit", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if not (args.directory / "layout.exterior.json").is_file():
        print(f"terrain_gen: no exterior in {args.directory} yet -- nothing to generate.")
        return 0

    problems = contract(args.directory)
    for problem in problems:
        print(f"terrain_gen: {problem}", file=sys.stderr)

    out = rendered(args.directory)
    stale = []
    for name, payload in out.items():
        path = args.directory / name
        data = payload.encode("utf-8") if isinstance(payload, str) else payload
        if not path.is_file() or path.read_bytes() != data:
            stale.append(name)
            if not args.check:
                path.write_bytes(data)

    if args.check:
        if problems:
            return 1
        if stale:
            print(f"terrain_gen: stale -- {', '.join(stale)}. Run tools/world/terrain_gen.py",
                  file=sys.stderr)
            return 1
        print(f"terrain_gen: the height field matches {args.directory}.")
        return 0
    print(f"terrain_gen: {len(out)} file(s)"
          + (f" -- {len(stale)} written" if stale else " (already current)"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
