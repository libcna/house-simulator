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

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "assets"))
import gltf_io  # noqa: E402
from layout_io import LayoutError  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "assets-src" / "world"
#: Where `--tiles` writes. A build product like the shell, and committed for the same reason: none.
TILES = REPO / "build" / "terrain"

#: §11.5: a 1.0 m grid over §10.3's playable area, X -40..+40 and Z -52..+12.
WIDTH, HEIGHT = 81, 65
ORIGIN_X, ORIGIN_Z = -40.0, -52.0
STEP = 1.0

#: The encoding: sample 0 is `originY`, 65535 is `originY + yScale`. A 6 m range over a
#: lot that moves 0.5 m is deliberate headroom -- a bank or a swale added later re-encodes nothing.
ORIGIN_Y, Y_SCALE = -3.0, 6.0

#: §11.5's eight classes, in the order the index uses. The index is the position in this list.
MATERIALS = ["grass", "lawn_worn", "soil", "gravel", "concrete", "asphalt", "bluestone", "mulch"]

#: §11.5's tile: *"one static chunk per 16 x 16 m tile ... each with its own `BoundingBox`, so
#: distance culling works"* (`HOUSE-00762`).
#:
#: **Twenty tiles, not §11.5's twenty-five.** 81 x 65 samples on a 1 m grid is 80 x 64 m, which is
#: 5 x 4 tiles of 16 m. Twenty-five would need a 5 x 5 field; §10.3's playable area is 80 x 64 and
#: the height field is sized from it. The parenthesis in §11.5 is arithmetic that does not follow
#: from the extents beside it, and is corrected there rather than worked around here.
TILE_METRES = 16.0

#: How far a tile's edge hangs down, in metres.
#:
#: A skirt is there for the seam a SIMPLIFIED tile leaves: at LOD0 two tiles share their edge
#: samples exactly and cannot crack, but §26's decimation moves an edge vertex by up to the
#: height difference between the samples it dropped. Measured over this lot's own field, the
#: largest step between two adjacent samples is 0.45 m (the terrace's own edge), so 0.50 m of
#: skirt covers the worst simplification the ground can suffer and is still invisible from
#: standing height.
SKIRT_METRES = 0.50

#: §18.3's lightmap density for a room, in texels a metre, and the gutter between islands. One
#: atlas holds the whole ground: a 16 m tile is 64 texels, five across and four deep with a
#: 4-texel gutter round each is 360 x 288, which fits `shell_unwrap`'s 512 limit with room to
#: spare -- so the terrain costs ONE atlas rather than one per tile.
LIGHTMAP_DENSITY = 4.0
LIGHTMAP_GUTTER = 4
LIGHTMAP_ATLAS = 512

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
    # A structure's pad is flat under it AT ITS OWN FLOOR, which is what a pad is (`HOUSE-00768`).
    #
    # §10.2 names it -- *"expressed as a coarse height field so drainage, the terrace step and the
    # shed pad read correctly"* -- and until this the pad changed the MATERIAL and not the level:
    # the shed's cell declares its floor at 0.00 and the lawn under it falls to -0.288, so the shed
    # stood a foot in the air with daylight under the door. The terrace escaped because its floor
    # is +0.45 and the exterior-cell rule below only fires on a NON-ZERO override; a floor of
    # exactly zero is still a floor, and this is the rule that says so.
    #
    # A structure with no CELL is not a building: §11.1's raised beds, the compost bin and the
    # trellis stand on the garden, and gravelling and levelling the soil under a vegetable bed
    # would be the generator inventing a yard nobody asked for. Only something with a floor gets
    # a pad, and having a floor is what `yOverride` on its cell says.
    cells_by_id = {row["id"]: row for row in layout_io.rows(layout, "cells")}
    for row in exterior.get("structures", []):
        override = (cells_by_id.get(row.get("cell")) or {}).get("yOverride")
        if not override:
            continue
        out.append(([row["footprint"]], float(override[0]), "gravel"))
    # An outdoor cell with a floor of its own is a deck or a terrace, and the ground under it is
    # flat at that height. `EXT_TERRACE` at +0.45 is the case §11.6 names.
    #
    # **Only on the ground storey.** An exterior cell on L1 or L2 is a BALCONY: it is attached to
    # the house, the ground under it is the lawn, and writing its floor into the height field
    # raises the lawn to first-floor level. The ground storey is found rather than named -- the
    # lowest level whose `ffl` is at or above §10.2's grade -- so a level inserted below L0 does
    # not silently move which one counts as the ground.
    levels = {row["id"]: float(row.get("ffl", 0.0)) for row in layout_io.rows(layout, "levels")}
    above_grade = [ffl for ffl in levels.values() if ffl >= 0.0]
    ground_ffl = min(above_grade) if above_grade else 0.0
    for cell in layout_io.rows(layout, "cells"):
        if cell.get("kind") != "exterior" or cell["id"] == "EXT_WORLD":
            continue
        if levels.get(cell.get("level"), 0.0) != ground_ffl:
            continue          # a balcony, not a pad; the ground under it is whatever the lot is
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
                # A pad's LEVEL is rounded OUTWARD, by half a sample, and its material is not.
                #
                # The two are different questions. What a footstep sounds like at a point is a
                # point sample, and half a metre of flagstone noise on the lawn would be inventing
                # a terrace nobody laid. How high the ground is under a slab is not: §11.6's
                # terrace is x -6.7…6.7 and this grid is 1 m, so a point test puts the plateau at
                # -6…6 and leaves the terrace's own floor slab **overhanging its ground by
                # 0.70 m**. A body that walked up the slope under that overhang stood on the
                # terrain 0.19 m inside the slab, which `HOUSE-00618`'s bot found the moment
                # `HOUSE-00774` let it walk round the terrace at all. Growing the pad instead
                # leaves the ground half a sample WIDER than the deck it carries, which is a
                # plinth and not a hole.
                if level is not None and any(
                        box["x"][0] - STEP / 2.0 <= x <= box["x"][1] + STEP / 2.0
                        and box["z"][0] - STEP / 2.0 <= z <= box["z"][1] + STEP / 2.0
                        for box in boxes):
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


def read_png(blob: bytes) -> tuple[int, int, int, list[int]]:
    """`(width, height, depth, samples)` from a greyscale PNG this module wrote.

    The decoder lives beside the encoder on purpose: they are one decision about how the ground is
    stored, and a second file that knew how to read these images would be a second place for that
    decision to drift. It is deliberately narrow -- greyscale, non-interlaced, filter 0 on every
    row, which is exactly what `png()` above emits -- and says so rather than half-supporting a
    PNG it was never given.
    """
    if blob[:8] != b"\x89PNG\r\n\x1a\n":
        raise LayoutError("not a PNG")
    at = 8
    header = None
    data = bytearray()
    while at + 8 <= len(blob):
        (length,) = struct.unpack(">I", blob[at:at + 4])
        tag = blob[at + 4:at + 8]
        payload = blob[at + 8:at + 8 + length]
        at += 12 + length
        if tag == b"IHDR":
            header = struct.unpack(">IIBBBBB", payload)
        elif tag == b"IDAT":
            data += payload
        elif tag == b"IEND":
            break
    if header is None:
        raise LayoutError("PNG has no IHDR")
    width, height, depth, colour, compression, filtering, interlace = header
    if colour != 0 or compression != 0 or filtering != 0 or interlace != 0:
        raise LayoutError(f"PNG is not the greyscale non-interlaced kind this reads "
                          f"(colour {colour}, interlace {interlace})")
    if depth not in (8, 16):
        raise LayoutError(f"PNG bit depth {depth} is neither 8 nor 16")

    raw = zlib.decompress(bytes(data))
    stride = width * (depth // 8)
    samples: list[int] = []
    for row in range(height):
        start = row * (stride + 1)
        if raw[start] != 0:
            raise LayoutError(f"PNG row {row} uses filter {raw[start]}; this reads filter 0 only")
        line = raw[start + 1:start + 1 + stride]
        if depth == 8:
            samples.extend(line)
        else:
            samples.extend(value for (value,) in struct.iter_unpack(">H", line))
    if len(samples) != width * height:
        raise LayoutError(f"PNG holds {len(samples)} samples, not {width * height}")
    return width, height, depth, samples


def decode(directory: Path) -> tuple[int, int, list[float], list[int]]:
    """`(width, height, heights_in_metres, material_indices)` read back from the PNGs on disk.

    This is what a build stage that needs the ground reads -- `build_collision.py` puts it in
    `collision.bin` so the runtime never opens a PNG -- and it goes through the images rather than
    calling `fields()` again so that what the game collides with is what the committed ground
    actually says, quantisation included.
    """
    width, height, depth, samples = read_png((directory / "terrain.png").read_bytes())
    if (width, height, depth) != (WIDTH, HEIGHT, 16):
        raise LayoutError(f"terrain.png is {width}x{height}@{depth}, not {WIDTH}x{HEIGHT}@16")
    heights = [ORIGIN_Y + (value / 65535.0) * Y_SCALE for value in samples]

    mw, mh, mdepth, material = read_png((directory / "terrain_materials.png").read_bytes())
    if (mw, mh, mdepth) != (WIDTH, HEIGHT, 8):
        raise LayoutError(f"terrain_materials.png is {mw}x{mh}@{mdepth}, not {WIDTH}x{HEIGHT}@8")
    for value in material:
        if value >= len(MATERIALS):
            raise LayoutError(f"terrain_materials.png holds index {value}; there are "
                              f"{len(MATERIALS)} materials")
    return WIDTH, HEIGHT, heights, material


def _encode(heights: list[float]) -> list[int]:
    """§11.5's 16-bit quantisation, REFUSING anything the range cannot hold.

    Clamping is the obvious thing to write and it writes a plateau where the ground was meant to
    be: three balconies spent a release as a 3.00 m mesa in the back garden, and every check
    passed, because the check compared the file against an encoder that clamped the same way
    (`HOUSE-00553` found it).
    """
    span = max(1e-9, Y_SCALE)
    for index, value in enumerate(heights):
        if value < ORIGIN_Y or value > ORIGIN_Y + Y_SCALE:
            x = ORIGIN_X + (index % WIDTH) * STEP
            z = ORIGIN_Z + (index // WIDTH) * STEP
            raise LayoutError(
                f"the ground at x={x:.1f} z={z:.1f} is {value:.2f} m, outside the height field's "
                f"{ORIGIN_Y:.1f} to {ORIGIN_Y + Y_SCALE:.1f}; widen `yScale`, or find out what "
                f"put a first-floor surface into the terrain")
    return [round((value - ORIGIN_Y) / span * 65535.0) for value in heights]


def tile_grid() -> tuple[int, int]:
    """How many tiles across (X) and deep (Z). Five by four over §10.3's 80 x 64 m."""
    return (round((WIDTH - 1) * STEP / TILE_METRES), round((HEIGHT - 1) * STEP / TILE_METRES))


def _vertex(ix: int, iz: int, heights: list[float], normals: list[tuple[float, float, float]],
            tile: tuple[int, int]) -> tuple:
    """One ground vertex: position, normal, world-metre UV0 and the atlas UV1 for its tile."""
    x = ORIGIN_X + ix * STEP
    z = ORIGIN_Z + iz * STEP
    per = int(TILE_METRES / STEP)
    # One atlas for the whole ground: each tile's island is 64 texels square with a 4-texel gutter
    # round it, so the islands step by 72 and the five-by-four grid comes to 360 x 288 texels.
    pitch = TILE_METRES * LIGHTMAP_DENSITY + 2 * LIGHTMAP_GUTTER
    u = tile[0] * pitch + LIGHTMAP_GUTTER + (ix - tile[0] * per) * LIGHTMAP_DENSITY
    v = tile[1] * pitch + LIGHTMAP_GUTTER + (iz - tile[1] * per) * LIGHTMAP_DENSITY
    return ((x, heights[iz * WIDTH + ix], z),
            normals[iz * WIDTH + ix],
            # UV0 is WORLD METRES, so the material decides its own repeat and two tiles cannot
            # disagree about where a texture starts. A tile-local 0..1 would tile the lawn five
            # times across the lot and put a seam down every tile edge.
            (x, z),
            (u / LIGHTMAP_ATLAS, v / LIGHTMAP_ATLAS))


def _sample_normals(heights: list[float]) -> list[tuple[float, float, float]]:
    """A normal per sample, area-weighted over the triangles that meet there.

    **The LIGHTING normal, and deliberately not the collider's.** `physics::TerrainAt` answers with
    the FACE normal of the triangle a point lands in, because a body has to be told about the
    surface it is standing on; a lawn shaded per face is a lawn made of visible facets. The
    surface is the same surface -- the same samples, the same diagonal -- and only the normal
    differs, which is the ordinary split between what is drawn and what is collided with.
    """
    accumulated = [[0.0, 0.0, 0.0] for _ in range(len(heights))]
    for iz in range(HEIGHT - 1):
        for ix in range(WIDTH - 1):
            corners = {}
            for du, dv in ((0, 0), (1, 0), (0, 1), (1, 1)):
                index = (iz + dv) * WIDTH + (ix + du)
                corners[(du, dv)] = (ORIGIN_X + (ix + du) * STEP, heights[index],
                                     ORIGIN_Z + (iz + dv) * STEP)
            # §11.5's own split, the one `physics::Terrain` collides with: the (0,0)-(1,1)
            # diagonal. A renderer that split the other way would draw a surface a body does not
            # stand on -- by a quarter of the square's twist, which `HOUSE-00553` measured at 74 mm.
            for triangle in (((0, 0), (1, 1), (1, 0)), ((0, 0), (0, 1), (1, 1))):
                a, b, c = (corners[key] for key in triangle)
                u = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
                v = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
                # Unnormalised, so the sum is area-weighted: a big triangle should count for more.
                normal = (u[1] * v[2] - u[2] * v[1],
                          u[2] * v[0] - u[0] * v[2],
                          u[0] * v[1] - u[1] * v[0])
                for key in triangle:
                    index = (iz + key[1]) * WIDTH + (ix + key[0])
                    for axis in range(3):
                        accumulated[index][axis] += normal[axis]
    out = []
    for total in accumulated:
        length = (total[0] ** 2 + total[1] ** 2 + total[2] ** 2) ** 0.5
        out.append((0.0, 1.0, 0.0) if length < 1e-12
                   else (total[0] / length, total[1] / length, total[2] / length))
    return out


def tiles(directory: Path) -> list[dict]:
    """§11.5's ground as tiles, ready to write: one primitive per material present in each.

    **A primitive per material and not per tile**, because §17.4 batches by material and a tile
    that is half lawn and half asphalt is two draws whatever this does. The tile is still the unit
    with a `BoundingBox`, which is what §11.5 asks distance culling to have.
    """
    _width, _height, heights, materials = decode(directory)
    normals = _sample_normals(heights)
    columns, rows = tile_grid()
    per = int(TILE_METRES / STEP)

    out = []
    for row in range(rows):
        for column in range(columns):
            primitives: dict[str, dict] = {}

            def add(name: str, corners: list[tuple]) -> None:
                entry = primitives.setdefault(name, {"material": f"TERRAIN_{name}", "ground": name,
                                                     "vertices": [], "index": {}, "indices": []})
                for corner in corners:
                    # Welded on the WHOLE vertex and not on the position: the skirt's top row
                    # stands exactly on the ground's edge and points sideways, so welding on
                    # position alone would hand the skirt the ground's normal and shade a
                    # vertical apron as if it were lawn.
                    position = entry["index"].get(corner)
                    if position is None:
                        position = len(entry["vertices"])
                        entry["index"][corner] = position
                        entry["vertices"].append(corner)
                    entry["indices"].append(position)

            for dz in range(per):
                for dx in range(per):
                    ix, iz = column * per + dx, row * per + dz
                    name = MATERIALS[materials[iz * WIDTH + ix]]
                    corner = {(du, dv): _vertex(ix + du, iz + dv, heights, normals, (column, row))
                              for du in (0, 1) for dv in (0, 1)}
                    # Wound to face UP: the collider's two triangles are `(00,10,11)` and
                    # `(00,11,01)`, whose cross products point down, and §14's front face is
                    # counter-clockwise. Same split, same surface, opposite winding.
                    for triangle in (((0, 0), (1, 1), (1, 0)), ((0, 0), (0, 1), (1, 1))):
                        add(name, [corner[key] for key in triangle])

            # The skirt, round the tile's own edge. Its vertices carry the edge's UVs and a normal
            # that points OUT of the tile, so a skirt lit as ground would not glow at grazing sun.
            for dz in range(per):
                for dx in range(per):
                    ix, iz = column * per + dx, row * per + dz
                    if not (dx == 0 or dz == 0 or dx == per - 1 or dz == per - 1):
                        continue
                    name = MATERIALS[materials[iz * WIDTH + ix]]
                    borders = []
                    if dx == 0:
                        borders.append((((0, 0), (0, 1)), (-1.0, 0.0, 0.0)))
                    if dx == per - 1:
                        borders.append((((1, 1), (1, 0)), (1.0, 0.0, 0.0)))
                    if dz == 0:
                        borders.append((((1, 0), (0, 0)), (0.0, 0.0, -1.0)))
                    if dz == per - 1:
                        borders.append((((0, 1), (1, 1)), (0.0, 0.0, 1.0)))
                    for (first, second), outward in borders:
                        top = [_vertex(ix + first[0], iz + first[1], heights, normals, (column, row)),
                               _vertex(ix + second[0], iz + second[1], heights, normals, (column, row))]
                        skirt = []
                        for point in top:
                            skirt.append(((point[0][0], point[0][1] - SKIRT_METRES, point[0][2]),
                                          outward, point[2], point[3]))
                        top = [(point[0], outward, point[2], point[3]) for point in top]
                        add(name, [top[0], skirt[0], top[1]])
                        add(name, [top[1], skirt[0], skirt[1]])

            ordered = [primitives[name] for name in MATERIALS if name in primitives]
            points = [vertex[0] for primitive in ordered for vertex in primitive["vertices"]]
            out.append({
                "id": f"TERRAIN_R{row}C{column}",
                "column": column,
                "row": row,
                "primitives": [{"material": primitive["material"],
                                "ground": primitive["ground"],
                                "vertices": primitive["vertices"],
                                "indices": primitive["indices"]} for primitive in ordered],
                "bounds": (min(p[0] for p in points), min(p[1] for p in points),
                           min(p[2] for p in points), max(p[0] for p in points),
                           max(p[1] for p in points), max(p[2] for p in points)),
            })
    return out


#: §11.4's street, cut into segments so distance culling has something to cull (`HOUSE-00763`).
#:
#: Twenty metres, which is 22 segments over the 440 m the `road` row's centreline declares -- and
#: chosen so that §10.3's playable edge at x = ±40 falls ON a segment boundary rather than inside
#: one. The carriageway is drawn only outside the height field, and a rule about whole segments is
#: easier to hold than one about parts of them.
ROAD_SEGMENT_METRES = 20.0

#: §11.4: *"drain grates at x = ±14"*, in the near gutter, 0.6 m by 0.4 m.
GRATE_X = (-14.0, 14.0)
GRATE_SIZE = (0.6, 0.4)

#: §11.4's *"faded centre line"*: 0.10 m wide, and 3 mm over the asphalt so it cannot z-fight with
#: the surface it is painted on.
CENTRE_LINE_WIDTH = 0.10
PAINT_LIFT = 0.003
#: A grate is metal in the road surface, lifted by the same hair for the same reason.
GRATE_LIFT = 0.005


def _quad(material: str, corners: list[tuple[float, float, float]],
          normal: tuple[float, float, float], uv_scale: float = 1.0) -> dict:
    """One flat rectangle as a primitive-ready dict. @p corners run round it."""
    vertices = [(corner, normal, (corner[0] * uv_scale, corner[2] * uv_scale), (0.0, 0.0))
                for corner in corners]
    return {"material": material, "ground": material.split("_", 1)[1],
            "vertices": vertices, "indices": [0, 1, 2, 0, 2, 3]}


def _strip(material: str, x0: float, x1: float, z0: float, z1: float, y: float) -> dict:
    """A horizontal rectangle facing up, wound counter-clockwise from above (§14's front face)."""
    return _quad(material,
                 [(x0, y, z0), (x0, y, z1), (x1, y, z1), (x1, y, z0)],
                 (0.0, 1.0, 0.0))


def road_pieces(directory: Path) -> list[dict]:
    """§11.4's road, kerbs, sidewalks, drain grates and centre line, in 40 m segments.

    **The carriageway is drawn only where the height field is not.** Inside §10.3's playable area
    the ground IS the road: `fields()` flattens the corridor to the `paths` rows' own heights and
    paints it asphalt and concrete, so `HOUSE-00762`'s tiles already draw it. Drawing it again
    would be two surfaces in one plane, which is exactly what `HOUSE-00485` spent a day removing.
    Beyond the field there is no ground at all, and this is where the street comes from.
    """
    layout = layout_io.load_layout(directory)
    exterior = layout.get("exterior") or {}
    road = exterior.get("road") or {}
    if not road.get("centreline") or not road.get("width"):
        return []
    centre_z = float(road["centreline"][0][2])
    half = float(road["width"]) / 2.0
    start = min(point[0] for point in road["centreline"])
    end = max(point[0] for point in road["centreline"])
    field_x0, field_x1 = ORIGIN_X, ORIGIN_X + (WIDTH - 1) * STEP

    # The corridor as the LAYOUT declares it, outermost first, so a strip that is not authored is
    # not invented: §11.4 calls the far side "mirrored" and the layout gives it a sidewalk and no
    # verge, so neither does this.
    strips = [(float(row["boxes"][0]["z"][0]), float(row["boxes"][0]["z"][1]),
               float(row.get("y") or 0.0), BY_MATERIAL.get(row.get("material"), "grass"))
              for row in exterior.get("paths", [])
              if row.get("kind") in ("verge", "sidewalk")
              and float(row["boxes"][0]["x"][0]) <= start + 1e-6]
    strips.append((centre_z - half, centre_z + half, 0.0,
                   BY_MATERIAL.get(road.get("material"), "asphalt")))
    strips.sort()

    out = []
    segments = int(round((end - start) / ROAD_SEGMENT_METRES))
    for index in range(segments):
        x0 = start + index * ROAD_SEGMENT_METRES
        x1 = x0 + ROAD_SEGMENT_METRES
        pieces: list[dict] = []
        # Outside the height field only, and CLIPPED rather than skipped: the boundary is a
        # segment edge at this length, and a clip keeps that a property of the arithmetic rather
        # than of the constant.
        for span in ((x0, min(x1, field_x0)), (max(x0, field_x1), x1)):
            if span[1] - span[0] <= 1e-6:
                continue
            for z0, z1, y, material in strips:
                pieces.append(_strip(f"TERRAIN_{material}", span[0], span[1], z0, z1, y))

        # The kerbs run the WHOLE length: inside the field the ground is flat asphalt and concrete
        # with no upstand at all, which is what a kerb is.
        for kerb in exterior.get("kerbs", []):
            line = float(kerb["path"][0][2])
            height = float(kerb.get("height") or 0.15)
            inward = 1.0 if line < centre_z else -1.0
            back = line - inward * height
            # The road-facing face, and the top. The other two are buried in the sidewalk.
            face = [(x0, 0.0, line), (x0, height, line), (x1, height, line), (x1, 0.0, line)]
            if inward > 0.0:
                # Wound so the face looks INTO the road, which for the near kerb is +Z and for the
                # far one -Z. The claim below caught this the wrong way round on both of them.
                face = list(reversed(face))
            pieces.append(_quad("TERRAIN_concrete", face, (0.0, 0.0, inward)))
            top = [(x0, height, min(line, back)), (x0, height, max(line, back)),
                   (x1, height, max(line, back)), (x1, height, min(line, back))]
            pieces.append(_quad("TERRAIN_concrete", top, (0.0, 1.0, 0.0)))

        # §11.4's faded centre line, and its drain grates.
        pieces.append(_strip("ROAD_paint", x0, x1,
                             centre_z - CENTRE_LINE_WIDTH / 2.0, centre_z + CENTRE_LINE_WIDTH / 2.0,
                             PAINT_LIFT))
        for grate in GRATE_X:
            if not (x0 - 1e-6 <= grate < x1 - 1e-6):
                continue
            gutter = min(float(kerb["path"][0][2]) for kerb in exterior.get("kerbs", [])
                         if float(kerb["path"][0][2]) < centre_z) if exterior.get("kerbs") \
                else centre_z - half
            pieces.append(_strip("ROAD_grate",
                                 grate - GRATE_SIZE[0] / 2.0, grate + GRATE_SIZE[0] / 2.0,
                                 gutter, gutter + GRATE_SIZE[1], GRATE_LIFT))

        merged: dict[str, dict] = {}
        for piece in pieces:
            entry = merged.setdefault(piece["material"],
                                      {"material": piece["material"], "ground": piece["ground"],
                                       "vertices": [], "indices": []})
            base = len(entry["vertices"])
            entry["vertices"].extend(piece["vertices"])
            entry["indices"].extend(base + i for i in piece["indices"])
        ordered = [merged[name] for name in sorted(merged)]
        points = [vertex[0] for primitive in ordered for vertex in primitive["vertices"]]
        out.append({
            "id": f"ROAD_S{index:02d}",
            "primitives": ordered,
            "bounds": (min(p[0] for p in points), min(p[1] for p in points),
                       min(p[2] for p in points), max(p[0] for p in points),
                       max(p[1] for p in points), max(p[2] for p in points)),
        })
    return out


def emit_road(directory: Path, output: Path) -> dict:
    """Writes one `.glb` per road segment into @p output. Returns a report."""
    output.mkdir(parents=True, exist_ok=True)
    written = []
    triangles = 0
    for piece in road_pieces(directory):
        document, blob = _tile_document(piece, receiver=False)
        gltf_io.write_glb(output / f"{piece['id']}.glb", document, blob)
        written.append(piece["id"])
        triangles += sum(len(primitive["indices"]) // 3 for primitive in piece["primitives"])
    return {"written": written, "triangles": triangles}


def _tile_document(tile: dict, receiver: bool = True) -> tuple[dict, bytes]:
    """One tile as a glTF document and its buffer, in `read_shell_geometry`'s own shape.

    One primitive per material, each with a NAMED material whose `extras` carry the surface class
    and the lightmap decision -- which is how `build_chunks.py` reads the shell, and reading the
    ground the same way is what keeps one chunker rather than two.
    """
    blob = bytearray()
    accessors: list[dict] = []
    views: list[dict] = []
    primitives: list[dict] = []
    materials: list[dict] = []

    def store(values: list[tuple], kind: str) -> int:
        nonlocal blob
        count = {"VEC3": 3, "VEC2": 2}[kind]
        offset = len(blob)
        for value in values:
            blob += struct.pack(f"<{count}f", *value[:count])
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(blob) - offset})
        accessors.append({"bufferView": len(views) - 1, "componentType": 5126,
                          "count": len(values), "type": kind,
                          "min": [min(v[i] for v in values) for i in range(count)],
                          "max": [max(v[i] for v in values) for i in range(count)]})
        return len(accessors) - 1

    for primitive in tile["primitives"]:
        vertices = primitive["vertices"]
        position = store([v[0] for v in vertices], "VEC3")
        normal = store([v[1] for v in vertices], "VEC3")
        uv0 = store([v[2] for v in vertices], "VEC2")
        uv1 = store([v[3] for v in vertices], "VEC2")
        offset = len(blob)
        for index in primitive["indices"]:
            blob += struct.pack("<H", index)
        while len(blob) % 4:
            blob += b"\0"
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(primitive["indices"]) * 2})
        accessors.append({"bufferView": len(views) - 1, "componentType": 5123,
                          "count": len(primitive["indices"]), "type": "SCALAR"})
        materials.append({
            "name": primitive["material"],
            "extras": {
                "surfaceClass": "terrain" if receiver else "road",
                # §11.5's ground is lit by §22's baked sun shading over the same second UV
                # channel a room's lightmap uses, which is why the tiles carry one at all. The
                # STREET beyond the lot is not a receiver: it would need an atlas of its own for
                # 440 m of asphalt, and the seam between the two falls on the property line where
                # §11.2's fence stands (`HOUSE-00763`).
                "lightmapReceiver": receiver,
                "groundMaterial": primitive.get("ground", primitive["material"]),
            },
        })
        primitives.append({"attributes": {"POSITION": position, "NORMAL": normal,
                                          "TEXCOORD_0": uv0, "TEXCOORD_1": uv1},
                           "indices": len(accessors) - 1, "material": len(materials) - 1,
                           "mode": 4})

    document = {
        "asset": {"version": "2.0", "generator": "cna-house terrain_gen.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": tile["id"], "mesh": 0}],
        "meshes": [{"name": tile["id"], "primitives": primitives}],
        "materials": materials,
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
    }
    return document, bytes(blob)


def emit_tiles(directory: Path, output: Path) -> dict:
    """Writes one `.glb` per tile into @p output. Returns a report."""
    output.mkdir(parents=True, exist_ok=True)
    written = []
    triangles = 0
    for tile in tiles(directory):
        document, blob = _tile_document(tile)
        gltf_io.write_glb(output / f"{tile['id']}.glb", document, blob)
        written.append(tile["id"])
        triangles += sum(len(primitive["indices"]) // 3 for primitive in tile["primitives"])
    return {"written": written, "triangles": triangles}


def rendered(directory: Path) -> dict[str, bytes | str]:
    heights, materials = fields(directory)
    samples = _encode(heights)
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
        # ...and a BALCONY is not a pad. `L1_BALCONY_REAR` is an exterior cell with a floor, at
        # +3.65, and it is attached to the house rather than standing on the lot -- so the ground
        # under it is the lawn on §10.2's slope. Writing its floor into the height field raised
        # 66 samples of the back garden to first-floor level, where the encoder then CLAMPED them
        # to +3.00 and said nothing (`HOUSE-00553`).
        require(at(-2.0, -30.0)[0] < 1.0,
                f"a first-floor balcony is not the ground under it ({at(-2.0, -30.0)})")
        require(abs(at(-2.0, -30.0)[0] - ground_at(-2.0, -30.0)) < 1e-9,
                f"...it is the lot's own slope there ({at(-2.0, -30.0)[0]:.3f} vs "
                f"{ground_at(-2.0, -30.0):.3f})")
        # The porch IS a pad: it is on the ground storey, and this is what says the rule above cut
        # balconies rather than everything with a floor.
        require(at(0.0, -13.0) == (0.57, "bluestone"),
                f"the porch at +0.57 is still a pad, paved by the walk over it "
                f"({at(0.0, -13.0)})")
        # And a height the encoding cannot hold is REFUSED rather than flattened.
        over = [10.0] + [0.0] * (WIDTH * HEIGHT - 1)
        caught = False
        try:
            _encode(over)
        except LayoutError as error:
            caught = "10.00" in str(error)
        require(caught, "a height outside the 6 m range is refused, not clamped to the top of it")
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

        # 7. `HOUSE-00762`'s tiles, over the house's own ground.
        rows = tiles(SOURCE)
        columns, deep = tile_grid()
        require((columns, deep) == (5, 4) and len(rows) == 20,
                f"§11.5's ground is {columns} x {deep} = {len(rows)} tiles of {TILE_METRES:.0f} m "
                f"over §10.3's 80 x 64 m -- not the 25 §11.5's parenthesis says, which would need "
                f"a 5 x 5 field")

        covered = {(tile["column"], tile["row"]) for tile in rows}
        require(len(covered) == len(rows), "every tile has its own square of the grid")
        edges = []
        for tile in rows:
            x0 = ORIGIN_X + tile["column"] * TILE_METRES
            z0 = ORIGIN_Z + tile["row"] * TILE_METRES
            # A skirt vertex carries a horizontal normal and a ground vertex does not, which is
            # what tells the two apart without carrying a flag through the mesh.
            ground = [v for primitive in tile["primitives"] for v in primitive["vertices"]
                      if abs(v[1][1]) > 1e-6]
            inside = all(x0 - 1e-6 <= v[0][0] <= x0 + TILE_METRES + 1e-6
                         and z0 - 1e-6 <= v[0][2] <= z0 + TILE_METRES + 1e-6
                         for primitive in tile["primitives"] for v in primitive["vertices"])
            edges.append(inside and bool(ground))
        require(all(edges), "and no tile has a vertex outside its own 16 m square")

        # The seam: two tiles that touch share the samples along their edge EXACTLY. At LOD0 that
        # is what makes the ground one surface rather than twenty; the skirt is for §26's
        # decimation, which is allowed to move an edge and is why the skirt exists at all.
        by_key = {(tile["column"], tile["row"]): tile for tile in rows}
        seams = 0
        cracks = []
        for (column, row), tile in sorted(by_key.items()):
            right = by_key.get((column + 1, row))
            if right is None:
                continue
            seams += 1
            plane = ORIGIN_X + (column + 1) * TILE_METRES
            mine = {v[0] for primitive in tile["primitives"] for v in primitive["vertices"]
                    if abs(v[0][0] - plane) < 1e-9 and v[1][0] >= 0.0}
            theirs = {v[0] for primitive in right["primitives"] for v in primitive["vertices"]
                      if abs(v[0][0] - plane) < 1e-9 and v[1][0] <= 0.0}
            heights_mine = {(round(p[2], 6), round(p[1], 6)) for p in mine}
            heights_theirs = {(round(p[2], 6), round(p[1], 6)) for p in theirs}
            if not heights_theirs <= heights_mine and not heights_mine <= heights_theirs:
                cracks.append((column, row))
        require(seams > 0 and not cracks,
                f"and two tiles that touch agree about every height along the seam ({seams} seam(s) "
                f"checked, {cracks[:3] if cracks else 'no crack'})")

        # The split and the winding: §11.5's ground is drawn as the two triangles it is COLLIDED
        # with, and `physics::Terrain` splits along (0,0)-(1,1). A renderer that split the other
        # way would draw a surface a body does not stand on, by up to the 74 mm `HOUSE-00553`
        # measured on this lot.
        upward = 0
        downward = []
        outward = 0
        inward = []
        for tile in rows:
            for primitive in tile["primitives"]:
                for i in range(0, len(primitive["indices"]), 3):
                    corners = [primitive["vertices"][primitive["indices"][i + k]] for k in range(3)]
                    a, b, c = (corner[0] for corner in corners)
                    u = tuple(b[k] - a[k] for k in range(3))
                    v = tuple(c[k] - a[k] for k in range(3))
                    normal = (u[1] * v[2] - u[2] * v[1],
                              u[2] * v[0] - u[0] * v[2],
                              u[0] * v[1] - u[1] * v[0])
                    if all(abs(corner[1][1]) < 1e-6 for corner in corners):
                        # A skirt face: it must point the way its vertices say it does.
                        wanted = corners[0][1]
                        dot = sum(normal[k] * wanted[k] for k in range(3))
                        if dot > 1e-9:
                            outward += 1
                        else:
                            inward.append(tile["id"])
                    elif normal[1] > 1e-9:
                        upward += 1
                    else:
                        downward.append(tile["id"])
        require(upward == 20 * 512 and not downward,
                f"every one of the {upward} ground triangles is wound to face UP "
                f"({downward[:3] if downward else 'none downward'})")
        require(outward == 20 * 128 and not inward,
                f"and every one of the {outward} skirt faces points out of its tile "
                f"({inward[:3] if inward else 'none inward'})")

        # The skirt, which is the other half of `HOUSE-00762`'s ask.
        skirted = []
        for tile in rows:
            lowest = min(v[0][1] for primitive in tile["primitives"] for v in primitive["vertices"])
            floor = min(v[0][1] for primitive in tile["primitives"] for v in primitive["vertices"]
                        if abs(v[1][1]) > 1e-6)
            skirted.append(abs((floor - lowest) - SKIRT_METRES) < 1e-6)
        require(all(skirted),
                f"and every tile hangs a {SKIRT_METRES:.2f} m skirt below its lowest ground vertex")

        # The bounds §11.5 asks distance culling to have.
        wrong = [tile["id"] for tile in rows
                 if any(not (tile["bounds"][0] - 1e-6 <= v[0][0] <= tile["bounds"][3] + 1e-6
                             and tile["bounds"][1] - 1e-6 <= v[0][1] <= tile["bounds"][4] + 1e-6
                             and tile["bounds"][2] - 1e-6 <= v[0][2] <= tile["bounds"][5] + 1e-6)
                         for primitive in tile["primitives"] for v in primitive["vertices"])]
        require(not wrong, f"every tile's own BoundingBox holds every vertex it has ({wrong[:3]})")

        # One atlas for the whole ground, and the islands do not touch.
        pitch = TILE_METRES * LIGHTMAP_DENSITY + 2 * LIGHTMAP_GUTTER
        boxes = []
        for tile in rows:
            us = [v[3][0] * LIGHTMAP_ATLAS for primitive in tile["primitives"]
                  for v in primitive["vertices"]]
            vs = [v[3][1] * LIGHTMAP_ATLAS for primitive in tile["primitives"]
                  for v in primitive["vertices"]]
            boxes.append((min(us), min(vs), max(us), max(vs)))
        inside = all(0.0 <= box[0] and 0.0 <= box[1] and box[2] <= LIGHTMAP_ATLAS
                     and box[3] <= LIGHTMAP_ATLAS for box in boxes)
        gutters = []
        for i, a in enumerate(boxes):
            for b in boxes[i + 1:]:
                overlap_u = min(a[2], b[2]) - max(a[0], b[0])
                overlap_v = min(a[3], b[3]) - max(a[1], b[1])
                gutters.append(overlap_u < -2 * LIGHTMAP_GUTTER + 1e-6
                               or overlap_v < -2 * LIGHTMAP_GUTTER + 1e-6
                               or min(overlap_u, overlap_v) <= 0.0)
        require(inside and all(gutters),
                f"the whole ground is ONE {LIGHTMAP_ATLAS}-texel atlas at "
                f"{LIGHTMAP_DENSITY:.0f} texels/m, with {LIGHTMAP_GUTTER} texels between islands "
                f"(pitch {pitch:.0f})")

        # Every square's material comes from the index image, so the ground and the footsteps
        # cannot disagree about what a person is standing on.
        _w, _h, _heights, index = decode(SOURCE)
        present = {primitive["ground"] for tile in rows for primitive in tile["primitives"]}
        require(present == {MATERIALS[value] for value in set(index)},
                f"and the materials the tiles carry are exactly the ones the index image uses "
                f"({sorted(present)})")

        first = _tile_document(rows[0])
        again = _tile_document(tiles(SOURCE)[0])
        require(first == again, "a tile renders the same bytes twice")

        # 8. `HOUSE-00763`'s street.
        street = road_pieces(SOURCE)
        exterior = layout_io.load_layout(SOURCE)["exterior"]
        centre_z = float(exterior["road"]["centreline"][0][2])
        half = float(exterior["road"]["width"]) / 2.0
        length = (max(p[0] for p in exterior["road"]["centreline"])
                  - min(p[0] for p in exterior["road"]["centreline"]))
        require(len(street) == round(length / ROAD_SEGMENT_METRES) == 22,
                f"§11.4's street is {len(street)} segments of {ROAD_SEGMENT_METRES:.0f} m over its "
                f"own {length:.0f} m centreline")
        spans = sorted((piece["bounds"][0], piece["bounds"][3]) for piece in street)
        require(all(abs(spans[i][1] - spans[i + 1][0]) < 1e-6 for i in range(len(spans) - 1))
                and abs(spans[0][0] + 220.0) < 1e-6 and abs(spans[-1][1] - 220.0) < 1e-6,
                "and they meet end to end from one end of it to the other, with no gap and no "
                "overlap")

        # The carriageway is the height field's inside §10.3's playable area and the street's
        # outside it, and never both -- which is what stops `HOUSE-00485`'s two-surfaces-one-plane
        # from coming back on the road.
        field_x0, field_x1 = ORIGIN_X, ORIGIN_X + (WIDTH - 1) * STEP
        asphalt = [(vertex[0][0], piece["id"]) for piece in street
                   for primitive in piece["primitives"] if primitive["material"] == "TERRAIN_asphalt"
                   for vertex in primitive["vertices"]]
        require(asphalt and all(x <= field_x0 + 1e-6 or x >= field_x1 - 1e-6 for x, _ in asphalt),
                f"the street draws carriageway only OUTSIDE the height field ({len(asphalt)} "
                f"vertices, none between {field_x0:.0f} and {field_x1:.0f})")
        _w, _h, _heights, index = decode(SOURCE)
        require(MATERIALS.index("asphalt") in set(index),
                "and the field paints the rest of it, which is why the street must not")

        # Both kerbs, over the whole length, at the height the layout gives them.
        for kerb in exterior["kerbs"]:
            line = float(kerb["path"][0][2])
            height = float(kerb["height"])
            tops = [vertex[0] for piece in street for primitive in piece["primitives"]
                    for vertex in primitive["vertices"]
                    if abs(vertex[0][1] - height) < 1e-6
                    and min(abs(vertex[0][2] - line), abs(vertex[0][2] - (line + height)),
                            abs(vertex[0][2] - (line - height))) < 1e-6]
            covered = sorted({round(point[0], 3) for point in tops})
            require(len(covered) >= 2 and abs(covered[0] + 220.0) < 1e-6
                    and abs(covered[-1] - 220.0) < 1e-6,
                    f"{kerb['id']} stands {height:.2f} m over the road along the whole street "
                    f"({len(tops)} vertices from x={covered[0] if covered else 0:.0f} to "
                    f"{covered[-1] if covered else 0:.0f})")

        grates = [primitive for piece in street for primitive in piece["primitives"]
                  if primitive["material"] == "ROAD_grate"]
        centres = sorted(round(sum(v[0][0] for v in primitive["vertices"]) / len(primitive["vertices"]), 3)
                         for primitive in grates)
        require(centres == sorted(GRATE_X),
                f"§11.4's two drain grates are at x = {centres}, in the near gutter")
        require(all(v[0][1] > 0.0 for primitive in grates for v in primitive["vertices"]),
                "and they sit ON the road surface rather than in it")

        paint = [primitive for piece in street for primitive in piece["primitives"]
                 if primitive["material"] == "ROAD_paint"]
        widths = {round(max(v[0][2] for v in primitive["vertices"])
                        - min(v[0][2] for v in primitive["vertices"]), 4) for primitive in paint}
        require(len(paint) == len(street) and widths == {CENTRE_LINE_WIDTH}
                and all(abs((max(v[0][2] for v in primitive["vertices"])
                             + min(v[0][2] for v in primitive["vertices"])) / 2.0 - centre_z) < 1e-6
                        for primitive in paint),
                f"and the faded centre line runs the whole street, {CENTRE_LINE_WIDTH * 100:.0f} cm "
                f"wide on the centreline at z = {centre_z:.1f}")
        require(all(v[0][1] >= PAINT_LIFT - 1e-9 for primitive in paint for v in primitive["vertices"]),
                f"lifted {PAINT_LIFT * 1000:.0f} mm over the asphalt, so the paint and the road it "
                f"is painted on cannot z-fight")

        wrong = []
        for piece in street:
            for primitive in piece["primitives"]:
                for i in range(0, len(primitive["indices"]), 3):
                    corners = [primitive["vertices"][primitive["indices"][i + k]] for k in range(3)]
                    a, b, c = (corner[0] for corner in corners)
                    u = tuple(b[k] - a[k] for k in range(3))
                    v = tuple(c[k] - a[k] for k in range(3))
                    normal = (u[1] * v[2] - u[2] * v[1],
                              u[2] * v[0] - u[0] * v[2],
                              u[0] * v[1] - u[1] * v[0])
                    if sum(normal[k] * corners[0][1][k] for k in range(3)) <= 1e-9:
                        wrong.append((piece["id"], primitive["material"]))
        require(not wrong,
                f"every face of the street is wound the way its own normal says ({wrong[:3]})")
        require(all(all(piece["bounds"][k] - 1e-6 <= v[0][k] <= piece["bounds"][k + 3] + 1e-6
                        for k in range(3))
                    for piece in street for primitive in piece["primitives"]
                    for v in primitive["vertices"]),
                "and every segment's BoundingBox holds every vertex it has")
        require(_tile_document(street[0], receiver=False)
                == _tile_document(road_pieces(SOURCE)[0], receiver=False),
                "a road segment renders the same bytes twice")
        # 9. `HOUSE-00765`: every paved surface §11 declares is IN the field, at its own height
        #    and its own material. The front walk, the terrace paving and the garden paths are
        #    therefore drawn by `HOUSE-00762`'s tiles, and nothing has to draw them again -- which
        #    is the same rule that keeps the carriageway off the street's own segments.
        _w2, _h2, field_heights, field_index = decode(SOURCE)

        def _sample(x: float, z: float) -> tuple[float, str]:
            ix = int(round((x - ORIGIN_X) / STEP))
            iz = int(round((z - ORIGIN_Z) / STEP))
            at = iz * WIDTH + ix
            return field_heights[at], MATERIALS[field_index[at]]

        wrong_surface = []
        for row in exterior.get("paths", []):
            box = row["boxes"][0]
            if box["x"][0] < ORIGIN_X or box["x"][1] > ORIGIN_X + (WIDTH - 1) * STEP:
                continue          # the street, which reaches 220 m past the field
            height, material = _sample((box["x"][0] + box["x"][1]) / 2.0,
                                       (box["z"][0] + box["z"][1]) / 2.0)
            wanted = BY_MATERIAL.get(row.get("material"), "grass")
            if abs(height - float(row.get("y") or 0.0)) > 1e-3 or material != wanted:
                wrong_surface.append((row["id"], round(height, 3), material, wanted))
        require(not wrong_surface,
                f"every paved surface §11 declares is in the height field at its own height and "
                f"material -- the front walk, the driveway, the apron, the connector and the "
                f"garden path ({wrong_surface[:3]})")

        pads = []
        whole = layout_io.load_layout(SOURCE)
        storeys = {row["id"]: float(row.get("ffl", 0.0)) for row in layout_io.rows(whole, "levels")}
        grade = min([ffl for ffl in storeys.values() if ffl >= 0.0] or [0.0])
        for cell in layout_io.rows(whole, "cells"):
            override = cell.get("yOverride")
            if cell.get("kind") != "exterior" or not override or float(override[0]) <= 0.0:
                continue
            if storeys.get(cell.get("level"), 0.0) != grade:
                continue          # a BALCONY: the ground under it is the lawn, not its own floor
            box = cell["boxes"][0]
            height, material = _sample((float(box["x"][0]) + float(box["x"][1])) / 2.0,
                                       (float(box["z"][0]) + float(box["z"][1])) / 2.0)
            if abs(height - float(override[0])) > 1e-3:
                pads.append((cell["id"], round(height, 3), float(override[0])))
        require(not pads,
                f"and every outdoor cell with a floor of its own stands on a flat pad at that "
                f"height -- §11.1's terrace at +0.45 is the case it is for ({pads[:3]})")

        # And a STRUCTURE's pad, which is the same rule for the thing standing on it rather than
        # for the cell inside it (`HOUSE-00768`). A floor of exactly zero is still a floor: the
        # shed's is 0.00 and the lawn under it falls to -0.29, so without this it stood a foot in
        # the air with daylight under its door.
        standing = []
        for row in (whole.get("exterior") or {}).get("structures", []):
            cell = next((one for one in layout_io.rows(whole, "cells")
                         if one["id"] == row.get("cell")), None)
            override = (cell or {}).get("yOverride")
            if not override:
                continue
            box = row["footprint"]
            height, material = _sample((float(box["x"][0]) + float(box["x"][1])) / 2.0,
                                       (float(box["z"][0]) + float(box["z"][1])) / 2.0)
            beside, _outside = _sample(float(box["x"][1]) + 2.0,
                                       (float(box["z"][0]) + float(box["z"][1])) / 2.0)
            standing.append((row["id"], round(height, 3), float(override[0]), round(beside, 3),
                             material))
        wrong_pad = [row for row in standing if abs(row[1] - row[2]) > 1e-3]
        described = ", ".join(f"{row[0]} at {row[1]:+.2f} on {row[4]} with the ground at "
                              f"{row[3]:+.2f} two metres away" for row in standing)
        require(standing and not wrong_pad,
                f"and every structure stands on a pad at ITS OWN floor: {described} "
                f"({wrong_pad[:2]})")

        require(all(primitive["material"] != "TERRAIN_grass"
                    or any(abs(v[0][2] - 13.4) < 1e-6 for v in primitive["vertices"]) is False
                    for piece in street for primitive in piece["primitives"]),
                "and the far verge §11.4 calls 'mirrored' is NOT invented: the layout gives that "
                "side a sidewalk and no verge row, so neither does this")

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
    parser.add_argument("--tiles", action="store_true",
                        help="write §11.5's terrain tiles as one .glb each (HOUSE-00762)")
    parser.add_argument("--road", action="store_true",
                        help="write §11.4's road, kerbs, sidewalks, grates and markings "
                             "(HOUSE-00763)")
    parser.add_argument("--output", type=Path, default=TILES,
                        help="where --tiles writes; a build product, never committed")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if args.tiles or args.road:
        if not (args.directory / "terrain.png").is_file():
            print(f"terrain_gen: no height field in {args.directory}; run this without --tiles "
                  f"first.", file=sys.stderr)
            return 1
        where = args.output.relative_to(REPO) if args.output.is_relative_to(REPO) else args.output
        if args.tiles:
            report = emit_tiles(args.directory, args.output)
            columns, rows = tile_grid()
            print(f"terrain_gen: {len(report['written'])} tile(s) ({columns} x {rows} of "
                  f"{TILE_METRES:.0f} m), {report['triangles']} triangles -> {where}")
        if args.road:
            report = emit_road(args.directory, args.output)
            print(f"terrain_gen: {len(report['written'])} road segment(s) of "
                  f"{ROAD_SEGMENT_METRES:.0f} m, {report['triangles']} triangles -> {where}")
        return 0
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
