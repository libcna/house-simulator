#!/usr/bin/env python3
"""neighbourhood_gen.py -- §11.4's sixty buildings, from a grammar rather than from a modeller.

`HOUSE-00841`. §11.4 puts two houses next door, six across the street, sixteen further along and
thirty-six silhouettes on the horizon ring, and none of them can be entered. `HOUSE-00391` already
placed all sixty: `layout.exterior.json`'s `neighbourhood` rows say WHERE each stands, which way it
faces, which LOD band it is in and which asset it draws. This is the asset.

    tools/world/neighbourhood_gen.py                  # -> build/neighbourhood/<ASSET>.glb
    tools/world/neighbourhood_gen.py --selftest

## Why a grammar

Sixty modelled houses is sixty files nobody will ever look at closely, and §3's whole position is
that this project builds systems rather than hand-made content. A grammar is five numbers per
house -- footprint, storeys, roof, garage, porch -- and the rest is arithmetic. It also makes the
street's variety cheap and honest: the three types the layout names differ in those five numbers
and in their palette, not in how much work went into them.

## What the roof is

`roof_geometry.roof_planes`, the same function `house_shell_gen.py` draws OUR roof with and
`build_collision.py` builds its rafters from (`HOUSE-00472`). A neighbour's hip roof is the same
geometry at a different size, and a second implementation of "where is a roof" is exactly the kind
of thing that drifts. A gable roof is the same four planes with the two ends removed and the
trapezoids extended -- which is what a gable IS -- rather than a separate builder.

## The LOD bands, and what "detail" means here

§26.1 selects on projected height and §26.2's ratios are 1.00 / 0.35 / 0.12. A neighbour at LOD0 is
90 m away at the nearest, so "full detail" means the massing, the roof, a garage, a porch, and
window and door RECESSES -- a reveal deep enough to read as an opening in a silhouette. LOD1 drops
the recesses and keeps the massing; that is the 0.35. `HOUSE-00842`…`HOUSE-00845` are the three
bands and the impostors; this file is the grammar all four draw from.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "assets"))

import gltf_io  # noqa: E402
import layout_io  # noqa: E402
import roof_geometry  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "assets-src" / "world"
OUTPUT = REPO / "build" / "neighbourhood"

#: §12.2's storey height, which a neighbour has no reason to differ on: these houses are the same
#: era and the same street as ours, and a 2.4 m storey next door would read as a doll's house.
STOREY = 3.05
#: The eaves oversail the wall by the same 0.15 m `roof_geometry` gives ours (§12.1, by
#: subtraction), so a neighbour's roof line reads as the same kind of building.
OVERHANG = roof_geometry.EAVES_OVERHANG
#: §12.1's 7:12 for a hip; a gable is steeper because that is what a gable is for.
HIP_PITCH = 7.0 / 12.0
GABLE_PITCH = 9.0 / 12.0
#: How deep a window or door recess is at LOD0. Deep enough to read at 90 m against the sky, which
#: is the nearest a neighbour ever gets (§11.4's `impostorFrom` is 90 for N1 and N2).
REVEAL = 0.12


class Palette:
    """§11.4's variety, as the eight material sets the grammar paints a house with.

    A palette is material IDS and not colours: the blockout draws a surface by its material name
    (`HOUSE-00473`), and §22's real materials arrive later against the same names. Eight because
    twenty-four houses share three shapes, and three shapes in one colour is a housing estate
    rather than a street.
    """

    def __init__(self, name: str, wall: str, trim: str, roof: str, door: str) -> None:
        self.name = name
        self.wall = wall
        self.trim = trim
        self.roof = roof
        self.door = door


PALETTES = [
    Palette("cream", "NB_WALL_CREAM", "NB_TRIM_WHITE", "NB_ROOF_GREY", "NB_DOOR_OAK"),
    Palette("clapboard", "NB_WALL_CLAPBOARD", "NB_TRIM_WHITE", "NB_ROOF_SLATE", "NB_DOOR_GREEN"),
    Palette("brick", "NB_WALL_BRICK", "NB_TRIM_STONE", "NB_ROOF_TILE", "NB_DOOR_BLACK"),
    Palette("sage", "NB_WALL_SAGE", "NB_TRIM_WHITE", "NB_ROOF_GREY", "NB_DOOR_RED"),
    Palette("ochre", "NB_WALL_OCHRE", "NB_TRIM_CREAM", "NB_ROOF_TILE", "NB_DOOR_OAK"),
    Palette("stone", "NB_WALL_STONE", "NB_TRIM_STONE", "NB_ROOF_SLATE", "NB_DOOR_BLUE"),
    Palette("slate-blue", "NB_WALL_SLATEBLUE", "NB_TRIM_WHITE", "NB_ROOF_GREY", "NB_DOOR_WHITE"),
    Palette("render", "NB_WALL_RENDER", "NB_TRIM_CREAM", "NB_ROOF_SLATE", "NB_DOOR_GREEN"),
]


#: How near two houses have to be for the street to notice they match, in metres across and deep.
#: A plot is 18-28 m wide on this street and the rows are 4-9 m apart in depth, so this reaches a
#: house's immediate neighbours and the ones behind them, and no further (ADR-0013).
NEIGHBOUR_X = 30.0
NEIGHBOUR_Z = 10.0

#: A palette by its §11.4 name, and by the part of an asset id that names it.
BY_NAME = {palette.name: palette for palette in PALETTES}
BY_SUFFIX = {palette.name.upper().replace("-", ""): palette for palette in PALETTES}


class House:
    """One house's five numbers, and the palette it is painted with.

    Everything else -- where the windows go, how the roof meets the walls, how deep a reveal is --
    is the grammar's, so a new house on this street is five numbers and a palette.
    """

    def __init__(self, name: str, width: float, depth: float, storeys: int, roof: str,
                 garage: str, porch: str, palette) -> None:
        if roof not in ("hip", "gable"):
            raise ValueError(f"{name}: roof {roof!r} is neither hip nor gable")
        if garage not in ("none", "attached", "detached"):
            raise ValueError(f"{name}: garage {garage!r} is not one of none/attached/detached")
        if porch not in ("none", "stoop", "full"):
            raise ValueError(f"{name}: porch {porch!r} is not one of none/stoop/full")
        self.name = name
        self.width = width
        self.depth = depth
        self.storeys = storeys
        self.roof = roof
        self.garage = garage
        self.porch = porch
        # A palette by NAME, never by index (ADR-0013): the asset id says `..._CREAM`, and an
        # index would be a second way to say the same thing that could disagree with it.
        if isinstance(palette, Palette):
            self.palette = palette
        elif palette in BY_NAME:
            self.palette = BY_NAME[palette]
        else:
            raise ValueError(f"{name}: palette {palette!r} is not one of {sorted(BY_NAME)}")

    @property
    def eaves(self) -> float:
        """Where the wall stops and the roof starts."""
        return self.storeys * STOREY

    @property
    def outer(self) -> tuple[float, float, float, float]:
        """The roof's rectangle: the walls plus the overhang, centred on the origin."""
        return (-self.width / 2.0 - OVERHANG, self.width / 2.0 + OVERHANG,
                -self.depth / 2.0 - OVERHANG, self.depth / 2.0 + OVERHANG)


#: The three types §11.4's list names, and their `_LOW` counterparts. `HOUSE-00391` authored the
#: asset ids; this is what each of them IS. A is the plain two-storey hip with an attached garage,
#: B has a gable and a full porch, C is the small single-storey with a detached garage -- three
#: silhouettes, which is what makes a row of them read as a street.
#: The three SHAPES §11.4's list names, as `(width, depth, storeys, roof, garage, porch)`. The
#: palette is not here: it comes from the asset id, and a shape painted eight ways is eight
#: variants of one grammar rather than eight houses (ADR-0013).
SHAPES = {
    "A": (11.0, 8.5, 2, "hip", "attached", "stoop"),
    "B": (9.5, 9.0, 2, "gable", "none", "full"),
    "C": (12.5, 7.0, 1, "hip", "detached", "stoop"),
}

#: What an asset id looks like: `MODEL_NB_HOUSE_<SHAPE>_<PALETTE>` and an optional `_LOW`.
HOUSE_PREFIX = "MODEL_NB_HOUSE_"


def house_of(asset: str) -> tuple[House, bool]:
    """`(house, detailed)` for an asset id.

    `MODEL_NB_HOUSE_B_SAGE_LOW` is shape B in §11.4's sage palette without its reveals or its
    plot. The id is the only place either is said (ADR-0013), and an id naming a shape or a
    palette this tool does not know is an error rather than a house painted some default.
    """
    if not asset.startswith(HOUSE_PREFIX):
        raise layout_io.LayoutError(f"{asset!r} is not a neighbourhood house id")
    body = asset[len(HOUSE_PREFIX):]
    detailed = not body.endswith("_LOW")
    if not detailed:
        body = body[:-len("_LOW")]
    shape, _, suffix = body.partition("_")
    if shape not in SHAPES:
        raise layout_io.LayoutError(
            f"{asset!r} names shape {shape!r}, which is not one of {sorted(SHAPES)}")
    if suffix not in BY_SUFFIX:
        raise layout_io.LayoutError(
            f"{asset!r} names palette {suffix!r}, which is not one of {sorted(BY_SUFFIX)}")
    width, depth, storeys, roof, garage, porch = SHAPES[shape]
    return (House(shape, width, depth, storeys, roof, garage, porch, BY_SUFFIX[suffix]),
            detailed)


def box(low, high, material: str):
    """An axis-aligned box as six outward quads, each `(corners, normal, material)`."""
    x0, y0, z0 = low
    x1, y1, z1 = high
    if x1 - x0 <= 1e-9 or y1 - y0 <= 1e-9 or z1 - z0 <= 1e-9:
        return []
    return [
        ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0.0, 0.0, 1.0), material),
        ([(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], (0.0, 0.0, -1.0), material),
        ([(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], (1.0, 0.0, 0.0), material),
        ([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], (-1.0, 0.0, 0.0), material),
        ([(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)], (0.0, 1.0, 0.0), material),
        ([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0.0, -1.0, 0.0), material),
    ]


def openings_on(house: House) -> list[tuple[float, float, float, float]]:
    """`(x0, x1, y0, y1)` for every window and the front door on the STREET elevation.

    Regular, because a builder's street is: one door and a window each side of it on the ground
    floor, and one window over each of those upstairs. §11.4 asks for "real windows with
    interior-glow cards at night" on N1 and N2, and a glow card needs somewhere to be.
    """
    out = []
    door_width, door_height = 0.95, 2.05
    window_width, window_height = 1.20, 1.40
    sill = 0.95
    out.append((-door_width / 2.0, door_width / 2.0, 0.0, door_height))
    for side in (-1.0, 1.0):
        centre = side * (house.width / 4.0 + 0.2)
        for storey in range(house.storeys):
            base = storey * STOREY + sill
            out.append((centre - window_width / 2.0, centre + window_width / 2.0,
                        base, base + window_height))
    return out


def house_faces(house: House, detailed: bool, plot: bool = True) -> list:
    """Every face of one house, in world metres, with the origin at the middle of its footprint.

    Faces are `(corners, normal, material)`, which is what `_document` writes and what
    `build_chunks.py` reads: one primitive per material, so a roof and a wall are two chunks and
    not one (§17.4).
    """
    faces: list = []
    half_w, half_d = house.width / 2.0, house.depth / 2.0
    faces += box((-half_w, 0.0, -half_d), (half_w, house.eaves, half_d), house.palette.wall)

    # §26.2's LOD1 is the massing without the detail, so the reveals are what LOD0 has and LOD1
    # does not -- 0.35 of the triangles, from one decision rather than from a decimation pass.
    if detailed:
        for x0, x1, y0, y1 in openings_on(house):
            material = house.palette.door if y0 <= 1e-9 else house.palette.trim
            faces += box((x0, y0, half_d - REVEAL), (x1, y1, half_d), material)

    if house.garage != "none":
        width, depth, height = 5.6, 6.0, 2.7
        if house.garage == "attached":
            low = (half_w, 0.0, -depth / 2.0)
            high = (half_w + width, height, depth / 2.0)
        else:
            low = (half_w + 2.5, 0.0, half_d - depth)
            high = (half_w + 2.5 + width, height, half_d)
        faces += box(low, high, house.palette.wall)
        # The door is the whole point of a garage in a silhouette.
        if detailed:
            faces += box((low[0] + 0.4, 0.0, high[2] - REVEAL),
                         (high[0] - 0.4, height - 0.4, high[2]), house.palette.trim)
        faces += _roof_faces(house, (low[0] - OVERHANG, high[0] + OVERHANG,
                                     low[2] - OVERHANG, high[2] + OVERHANG), height, "hip")

    # The porch is LOD0's, like the reveals and the plot (`HOUSE-00844`). §26.2 puts LOD2 at 0.12
    # of the triangles and this house's `_LOW` variant sat at 0.24-0.32 with a porch on it: four
    # boxes of trim -- a slab, two posts and a beam -- that at 30-90 px on screen is a smudge under
    # the eaves. Without it the three shapes come to 0.13, 0.07 and 0.14, which is the band.
    if house.porch != "none" and detailed:
        depth = 2.4 if house.porch == "full" else 1.2
        width = house.width if house.porch == "full" else 2.6
        height = 2.6
        faces += box((-width / 2.0, 0.0, half_d), (width / 2.0, 0.15, half_d + depth),
                     house.palette.trim)
        for side in (-1.0, 1.0):
            post = side * (width / 2.0 - 0.15)
            faces += box((post - 0.08, 0.15, half_d + depth - 0.25),
                         (post + 0.08, height, half_d + depth - 0.09), house.palette.trim)
        faces += box((-width / 2.0, height, half_d), (width / 2.0, height + 0.18,
                                                      half_d + depth), house.palette.trim)

    faces += _roof_faces(house, house.outer, house.eaves, house.roof)
    if detailed and plot:
        faces += plot_faces(house)
    return faces


#: A neighbour's drive: §11.4 gives ours 7.8 m of width and theirs is a single car's.
DRIVE_WIDTH = 3.2
DRIVE_REACH = 9.0
DRIVE_THICK = 0.12
#: The boundary between two plots: a low rail fence, which is what a suburban side boundary is.
BOUNDARY_HEIGHT = 1.05
BOUNDARY_POST = 0.09
BOUNDARY_RAIL = 0.06
BOUNDARY_BAY = 2.4


def plot_faces(house: House) -> list:
    """§11.4's "driveways, cars, mailboxes, lawns, fences" for one house, as far as geometry goes.

    `HOUSE-00842`. A house at LOD0 is 90 m away at the nearest and still reads as a house on a
    PLOT rather than as a model on grass: a drive running out to the street from wherever the cars
    are kept, and a rail fence down one boundary. The cars and the mailboxes are
    `layout.exterior.json`'s own rows (`NB_CAR_01`…`NB_CAR_03`, twelve `MODEL_MAILBOX`), placed by
    `HOUSE-00391` and drawn by their own assets; what the house owes its plot is the ground and
    the boundary.

    Only at LOD0: §26.2's LOD1 is the massing, and a drive 200 m away is a grey line.
    """
    faces: list = []
    half_w, half_d = house.width / 2.0, house.depth / 2.0

    # The drive starts where the cars are: at the garage if there is one, and beside the front
    # door if there is not.
    if house.garage == "attached":
        centre = half_w + 2.8
    elif house.garage == "detached":
        centre = half_w + 2.5 + 2.8
    else:
        centre = -half_w + DRIVE_WIDTH / 2.0 + 0.6
    faces += box((centre - DRIVE_WIDTH / 2.0, -DRIVE_THICK, half_d),
                 (centre + DRIVE_WIDTH / 2.0, 0.0, half_d + DRIVE_REACH), "NB_DRIVE")

    # The boundary, down the side away from the drive so the two do not fight over the same metre.
    edge = -half_w - 1.6 if centre > 0.0 else half_w + 1.6
    bays = max(1, int(round((house.depth + DRIVE_REACH) / BOUNDARY_BAY)))
    start, end = -half_d, half_d + DRIVE_REACH
    for index in range(bays + 1):
        at = start + (end - start) * index / bays
        faces += box((edge - BOUNDARY_POST / 2.0, 0.0, at - BOUNDARY_POST / 2.0),
                     (edge + BOUNDARY_POST / 2.0, BOUNDARY_HEIGHT, at + BOUNDARY_POST / 2.0),
                     house.palette.trim)
    for level in (0.45, BOUNDARY_HEIGHT - BOUNDARY_RAIL - 0.05):
        faces += box((edge - BOUNDARY_RAIL / 2.0, level, start),
                     (edge + BOUNDARY_RAIL / 2.0, level + BOUNDARY_RAIL, end),
                     house.palette.trim)
    return faces


def _roof_faces(house: House, outer: tuple, eaves: float, kind: str) -> list:
    """The roof over @p outer, as `roof_geometry`'s planes -- hipped, or gabled at the ends.

    A GABLE is the hip with its two end triangles removed and the two long planes carried out to
    the ends, plus a wall triangle closing each end. Built from the same planes rather than from a
    second derivation, so a neighbour's ridge is where `roof_geometry` says a ridge is.
    """
    pitch = HIP_PITCH if kind == "hip" else GABLE_PITCH
    planes = roof_geometry.roof_planes(outer, eaves, pitch)
    faces = []
    if kind == "hip":
        for corners, outward in planes:
            faces.append((list(corners), outward, house.palette.roof))
        return faces

    x0, x1, z0, z1 = outer
    ridge = eaves + min(x1 - x0, z1 - z0) / 2.0 * pitch
    mid_z = (z0 + z1) / 2.0
    faces.append(([(x0, eaves, z0), (x1, eaves, z0), (x1, ridge, mid_z), (x0, ridge, mid_z)],
                  (0.0, pitch, -1.0), house.palette.roof))
    faces.append(([(x1, eaves, z1), (x0, eaves, z1), (x0, ridge, mid_z), (x1, ridge, mid_z)],
                  (0.0, pitch, 1.0), house.palette.roof))
    for x, outward in ((x0, (-1.0, 0.0, 0.0)), (x1, (1.0, 0.0, 0.0))):
        faces.append(([(x, eaves, z0), (x, eaves, z1), (x, ridge, mid_z)], outward,
                      house.palette.wall))
    return faces


def impostor_faces(width: float = 11.0, height: float = 8.5) -> list:
    """§26.1's impostor: two triangles, and §11.4's "distant roof/gable silhouettes".

    One quad standing on the ground, with the gable cut into its top: a card is what a house 200 m
    away is, and the shape of its top edge is the only thing that reads at that range.
    """
    half = width / 2.0
    eaves = height * 0.6
    return [
        ([(-half, 0.0, 0.0), (half, 0.0, 0.0), (half, eaves, 0.0), (-half, eaves, 0.0)],
         (0.0, 0.0, 1.0), "NB_IMPOSTOR"),
        ([(-half, eaves, 0.0), (half, eaves, 0.0), (0.0, height, 0.0)],
         (0.0, 0.0, 1.0), "NB_IMPOSTOR"),
    ]


def _document(name: str, faces: list) -> tuple[dict, bytes]:
    """@p faces as one glTF document, one primitive per material.

    `read_shell_geometry`'s shape (`HOUSE-00473`), so `build_chunks.py` reads a neighbour the way
    it reads the shell and the fences: grouped by material, with the surface class in `extras`.
    """
    blob = bytearray()
    accessors: list[dict] = []
    views: list[dict] = []
    primitives: list[dict] = []
    materials: list[dict] = []

    def store(values: list[tuple], kind: str) -> int:
        count = {"VEC3": 3, "VEC2": 2}[kind]
        offset = len(blob)
        for value in values:
            blob.extend(struct.pack(f"<{count}f", *value[:count]))
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(blob) - offset})
        accessors.append({"bufferView": len(views) - 1, "componentType": 5126,
                          "count": len(values), "type": kind,
                          "min": [min(v[i] for v in values) for i in range(count)],
                          "max": [max(v[i] for v in values) for i in range(count)]})
        return len(accessors) - 1

    by_material: dict[str, list] = {}
    for corners, normal, material in faces:
        by_material.setdefault(material, []).append((corners, normal))

    for material, group in sorted(by_material.items()):
        positions: list[tuple[float, float, float]] = []
        normals: list[tuple[float, float, float]] = []
        uvs: list[tuple[float, float]] = []
        indices: list[int] = []
        for corners, normal in group:
            base = len(positions)
            for point in corners:
                positions.append(point)
                normals.append(normal)
                # World metres, like the fence's and the ground's: cladding is the same size on
                # every house, which is what makes a row of them a street.
                uvs.append((point[0] + point[2], point[1]))
            indices += ([base, base + 1, base + 2] if len(corners) == 3
                        else [base, base + 1, base + 2, base, base + 2, base + 3])
        position = store(positions, "VEC3")
        normal_at = store(normals, "VEC3")
        uv0 = store(uvs, "VEC2")
        offset = len(blob)
        for index in indices:
            blob.extend(struct.pack("<I", index))
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(indices) * 4})
        accessors.append({"bufferView": len(views) - 1, "componentType": 5125,
                          "count": len(indices), "type": "SCALAR"})
        primitives.append({"attributes": {"POSITION": position, "NORMAL": normal_at,
                                          "TEXCOORD_0": uv0},
                           "indices": len(accessors) - 1, "material": len(materials), "mode": 4})
        materials.append({"name": material,
                          "extras": {"surfaceClass": "exterior",
                                     # §18.3 bakes the ROOMS; a neighbour is lit by the sun and
                                     # the sky, and there is no inside of it to bake.
                                     "lightmapReceiver": False}})

    document = {
        "asset": {"version": "2.0", "generator": "cna-house neighbourhood_gen.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": name, "mesh": 0}],
        "meshes": [{"name": name, "primitives": primitives}],
        "materials": materials,
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
    }
    return document, bytes(blob)


def wanted_assets(directory: Path) -> list[str]:
    """Every neighbourhood asset the layout asks for, in id order."""
    layout = layout_io.load_layout(directory, kinds=["exterior"])
    exterior = layout.get("exterior") or {}
    return sorted({row["asset"] for row in exterior.get("neighbourhood", [])
                   if str(row.get("asset", "")).startswith(("MODEL_NB_HOUSE", "MODEL_NB_IMPOSTOR"))})


def build(directory: Path) -> dict:
    """`{asset: faces}` for every neighbourhood asset the layout names."""
    out: dict[str, list] = {}
    for asset in wanted_assets(directory):
        if asset.startswith("MODEL_NB_IMPOSTOR"):
            out[asset] = impostor_faces()
            continue
        house, detailed = house_of(asset)
        out[asset] = house_faces(house, detailed)
    return out


def report(built: dict) -> str:
    lines = [f"{len(built)} neighbourhood asset(s)"]
    for asset, faces in sorted(built.items()):
        triangles = sum(1 if len(corners) == 3 else 2 for corners, _n, _m in faces)
        materials = sorted({material for _c, _n, material in faces})
        lines.append(f"  {asset:26s} {len(faces):3d} face(s), {triangles:3d} triangle(s), "
                     f"{len(materials)} material(s)")
    return "\n".join(lines)


# ======================================================================================= selftest


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("neighbourhood_gen: selftest")

    # 1. The grammar's own vocabulary. A house is five numbers and a palette, and a value outside
    #    the vocabulary is an error rather than a silently different house.
    for bad in ({"roof": "flat"}, {"garage": "carport"}, {"porch": "veranda"}):
        try:
            House("BAD", 10.0, 8.0, 2, bad.get("roof", "hip"), bad.get("garage", "none"),
                  bad.get("porch", "none"), 0)
            caught = False
        except ValueError:
            caught = True
        require(caught, f"a house with {list(bad.values())[0]!r} is refused, not built")
    require(len(PALETTES) == 8,
            f"§11.4's variety is eight palettes ({len(PALETTES)})")
    require(len({palette.name for palette in PALETTES}) == 8,
            "and every one of them is a different name")
    require(len({(p.wall, p.trim, p.roof, p.door) for p in PALETTES}) == 8,
            "...and a different set of materials, so two houses in a row do not match")

    # 2. The three SHAPES the layout names differ as shapes and not only in paint: a street of one
    #    silhouette in three colours is a housing estate.
    require(sorted(SHAPES) == ["A", "B", "C"],
            f"three shapes, which is what `HOUSE-00391` placed ({sorted(SHAPES)})")
    TYPES = {f"MODEL_NB_HOUSE_{shape}_CREAM": house_of(f"MODEL_NB_HOUSE_{shape}_CREAM")[0]
             for shape in SHAPES}
    shapes = {(h.width, h.depth, h.storeys, h.roof, h.garage, h.porch) for h in TYPES.values()}
    require(len(shapes) == 3, f"and no two of them are the same house ({len(shapes)} shapes)")
    require(len({h.storeys for h in TYPES.values()}) > 1
            and len({h.roof for h in TYPES.values()}) > 1,
            "-- they differ in storeys AND in roof, which is what reads at 90 m")

    # 2b. `HOUSE-00843`, ADR-0013: the palette is in the ID, and an id is refused rather than
    #     painted with a default.
    require(house_of("MODEL_NB_HOUSE_B_SAGE")[0].palette.name == "sage",
            "an asset id names its palette, and that is where the palette comes from")
    require(house_of("MODEL_NB_HOUSE_B_SAGE_LOW")[0].palette.name == "sage"
            and not house_of("MODEL_NB_HOUSE_B_SAGE_LOW")[1],
            "...and `_LOW` is the LOD band, not part of the palette's name")
    require(house_of("MODEL_NB_HOUSE_C_SLATEBLUE")[0].palette.name == "slate-blue",
            "a palette whose §11.4 name has a hyphen is `SLATEBLUE` in an id and `slate-blue` "
            "everywhere else, and the two are one table")
    for bad in ("MODEL_NB_HOUSE_D_CREAM", "MODEL_NB_HOUSE_A_TEAL", "MODEL_SOMETHING_ELSE"):
        try:
            house_of(bad)
            caught = False
        except layout_io.LayoutError:
            caught = True
        require(caught, f"{bad} is refused, not built as something else")

    # 3. Geometry: a house stands on the ground, its roof starts where its walls stop, and its
    #    ridge is over its own footprint.
    for asset, house in sorted(TYPES.items()):
        # The BUILDING and not its plot: a drive is a slab buried in the ground like every other
        # slab in this world, and a boundary fence runs out past the house's own corner. Both are
        # `plot_faces`'s and both are claimed below.
        faces = house_faces(house, True, plot=False)
        ys = [point[1] for corners, _n, _m in faces for point in corners]
        # Of the WALL and not of the house: a porch slab lies on the ground too, and a house
        # lifted off it with its porch left behind would pass a claim about the lowest point of
        # anything.
        wall_ys = [point[1] for corners, _n, material in faces
                   if material == house.palette.wall for point in corners]
        require(wall_ys and abs(min(wall_ys)) < 1e-9,
                f"{house.name}'s WALL stands on the ground: its lowest point is "
                f"{min(wall_ys) if wall_ys else None}")
        require(abs(min(ys)) < 1e-9,
                f"...and nothing of it is under the ground either ({min(ys):.3f})")
        ridge = house.eaves + min(house.width + 2 * OVERHANG,
                                  house.depth + 2 * OVERHANG) / 2.0 * (
            HIP_PITCH if house.roof == "hip" else GABLE_PITCH)
        require(abs(max(ys) - ridge) < 0.25,
                f"...and its highest is its own ridge, {max(ys):.2f} against {ridge:.2f}")
        xs = [point[0] for corners, _n, _m in faces for point in corners]
        reach = house.width / 2.0 + OVERHANG + (8.1 if house.garage != "none" else 0.0)
        require(max(xs) <= reach + 1e-6,
                f"...and nothing of it reaches past its own garage ({max(xs):.2f} of {reach:.2f})")

    # 4. §26.2's LOD1 is the massing without the detail. Measured as a RATIO, because that is what
    #    §26.1's table states, and asserted per type rather than in the aggregate.
    for asset, house in sorted(TYPES.items()):
        full = sum(1 if len(c) == 3 else 2 for c, _n, _m in house_faces(house, True, plot=False))
        low = sum(1 if len(c) == 3 else 2 for c, _n, _m in house_faces(house, False))
        require(low < full,
                f"{house.name}_LOW is smaller than {house.name} ({low} against {full} triangles)")
        require(0.10 <= low / full <= 0.60,
                f"...and it is the massing rather than a decimation: {low / full:.2f} of the "
                f"BUILDING")
        # `HOUSE-00844`: and of the whole ASSET, which is where §26.2's ratios live. This street
        # has TWO variants for three bands -- the layout gives `LODG_NB_HOUSE_MED` the full asset
        # and `LODG_NB_HOUSE_LOW` the `_LOW` one -- so `_LOW` is LOD2 and its target is 0.12, not
        # LOD1's 0.35. Measured: 0.12, 0.07 and 0.14 for the three shapes.
        whole = sum(1 if len(c) == 3 else 2 for c, _n, _m in house_faces(house, True))
        require(0.05 <= low / whole <= 0.20,
                f"...and {low / whole:.2f} of the whole asset, which is §26.2's LOD2 band")

    # 5. The openings are on the STREET elevation and inside the wall they are cut into -- a
    #    window that overhangs its own wall is a window in the air.
    for asset, house in sorted(TYPES.items()):
        for x0, x1, y0, y1 in openings_on(house):
            require(-house.width / 2.0 <= x0 and x1 <= house.width / 2.0,
                    f"{house.name}: an opening at x {x0:.2f}..{x1:.2f} is inside its "
                    f"{house.width:.1f} m front")
            require(y1 <= house.eaves,
                    f"{house.name}: ...and under its eaves ({y1:.2f} of {house.eaves:.2f})")
    doors = [row for row in openings_on(TYPES["MODEL_NB_HOUSE_A_CREAM"]) if row[2] <= 1e-9]
    require(len(doors) == 1, f"and exactly one of them reaches the ground: the door ({len(doors)})")

    # 6. The roof comes from `roof_geometry` and not from a second derivation.
    house = TYPES["MODEL_NB_HOUSE_A_CREAM"]
    planes = roof_geometry.roof_planes(house.outer, house.eaves, HIP_PITCH)
    require(len(planes) == 4, f"a hip roof is `roof_geometry`'s four planes ({len(planes)})")
    roof_faces = [f for f in house_faces(house, True) if f[2] == house.palette.roof]
    require(len(roof_faces) >= 4,
            f"and the house draws them ({len(roof_faces)} roof faces, garage included)")
    gable = TYPES["MODEL_NB_HOUSE_B_CREAM"]
    gable_faces = _roof_faces(gable, gable.outer, gable.eaves, "gable")
    require(sum(1 for corners, _n, _m in gable_faces if len(corners) == 3) == 2,
            "a GABLE is two slopes and two triangular ends, and the ends are wall")
    require(sum(1 for _c, _n, material in gable_faces if material == gable.palette.wall) == 2,
            "-- the ends are the WALL's material, because a gable end is a wall")

    # 6b. `HOUSE-00842`: the plot. A house at LOD0 stands on a drive and a boundary, and one at
    #     LOD1 stands on neither.
    for asset, house in sorted(TYPES.items()):
        plot = plot_faces(house)
        require(plot, f"{house.name} has a plot: a drive and a boundary fence")
        drive = [f for f in plot if f[2] == "NB_DRIVE"]
        require(drive, f"{house.name}'s drive is its own material, so it is its own chunk")
        zs = [point[2] for corners, _n, _m in drive for point in corners]
        require(min(zs) >= house.depth / 2.0 - 1e-6,
                f"{house.name}'s drive starts at the front of the house and runs OUT, rather than "
                f"under it ({min(zs):.2f} against {house.depth / 2.0:.2f})")
        require(max(zs) - min(zs) >= 8.0,
                f"...and reaches the street: {max(zs) - min(zs):.1f} m of it")
        ys = [point[1] for corners, _n, _m in drive for point in corners]
        require(max(ys) <= 1e-6,
                f"...and lies IN the ground rather than on it, like every other slab in this "
                f"world ({max(ys):.3f})")
        if house.garage != "none":
            xs = [point[0] for corners, _n, _m in drive for point in corners]
            require(min(xs) > house.width / 2.0,
                    f"{house.name} has a garage, so its drive is on the garage's side "
                    f"({min(xs):.2f} against {house.width / 2.0:.2f})")
        posts = [f for f in plot if f[2] == house.palette.trim]
        require(len(posts) >= 6 * 4,
                f"{house.name}'s boundary is posts and rails, not one long box ({len(posts)} faces)")
        low = [f for f in house_faces(house, False) if f[2] == "NB_DRIVE"]
        require(not low, f"{house.name}_LOW has no drive: §26.2's LOD1 is the massing")

    # 7. The impostor is §26.1's two triangles and nothing else.
    card = impostor_faces()
    require(sum(1 if len(c) == 3 else 2 for c, _n, _m in card) == 3,
            f"an impostor is a quad and a gable -- three triangles, which is what a house 200 m "
            f"away is ({sum(1 if len(c) == 3 else 2 for c, _n, _m in card)})")
    require(len({material for _c, _n, material in card}) == 1,
            "and one material, so it is one draw")

    # 8. Against the layout the street was authored from -- `HOUSE-00843`'s street, with a palette
    #    in every id.
    if (SOURCE / "layout.exterior.json").is_file():
        assets = wanted_assets(SOURCE)
        built = build(SOURCE)
        require(set(built) == set(assets),
                f"every asset the layout names is built ({sorted(set(assets) - set(built))} "
                f"missing)")
        require(all(faces for faces in built.values()), "with geometry in each")

        # ADR-0013's first rule: only the combinations the street actually names. Three shapes,
        # eight palettes and two LOD bands is forty-eight; a Cartesian product is what this
        # decision exists to avoid.
        houses = [asset for asset in assets if asset.startswith(HOUSE_PREFIX)]
        require(len(houses) < len(SHAPES) * len(PALETTES) * 2 // 2,
                f"the street names {len(houses)} house variants, well under the "
                f"{len(SHAPES) * len(PALETTES) * 2} a Cartesian product would be")
        require(all(house_of(asset) for asset in houses),
                "and every one of them parses as a shape this grammar knows in a palette §11.4 "
                "names")
        used = {house_of(asset)[0].palette.name for asset in houses}
        require(used == {palette.name for palette in PALETTES},
                f"all eight palettes are on the street, which is what §11.4 asks for "
                f"({sorted({p.name for p in PALETTES} - used)} unused)")

        # ...and its second: no two houses you can see together are painted alike.
        rows = (layout_io.load_layout(SOURCE, kinds=["exterior"]).get("exterior")
                or {}).get("neighbourhood", [])
        placed = [(row["id"], row["position"][0], row["position"][2],
                   house_of(row["asset"])[0].palette.name)
                  for row in rows if str(row.get("asset", "")).startswith(HOUSE_PREFIX)]
        clashes = [(a[0], b[0], a[3]) for index, a in enumerate(placed) for b in placed[index + 1:]
                   if abs(a[1] - b[1]) <= NEIGHBOUR_X and abs(a[2] - b[2]) <= NEIGHBOUR_Z
                   and a[3] == b[3]]
        require(not clashes,
                f"no two houses within {NEIGHBOUR_X:.0f} m across and {NEIGHBOUR_Z:.0f} m deep of "
                f"each other share a palette, so the street is not a copy-pasted estate "
                f"({clashes})")
        require(len(placed) == 24, f"§11.4's twenty-four placed houses ({len(placed)})")

        # `HOUSE-00844`: which asset each LOD band names. Two variants over three bands, and the
        # layout is what decides which band gets which -- LOD0 and LOD1 share the full asset
        # because a house 90 m away and one 160 m away differ in what the CULLER does with them,
        # not in what the file contains.
        bands = {}
        for row in rows:
            asset = str(row.get("asset", ""))
            if asset.startswith(HOUSE_PREFIX):
                bands.setdefault(row.get("lodGroup"), set()).add(asset.endswith("_LOW"))
        require(bands.get("LODG_NB_HOUSE_FULL") == {False}
                and bands.get("LODG_NB_HOUSE_MED") == {False},
                f"LOD0 and LOD1 name the full variant ({bands})")
        require(bands.get("LODG_NB_HOUSE_LOW") == {True},
                f"and LOD2 names `_LOW`, all sixteen of them ({bands})")
        require(sum(1 for row in rows if str(row.get("asset", "")).endswith("_LOW")) == 16,
                "which is §11.4's N9-N24")
        # ...and the radius that "together" means is the street's own geometry, not a number that
        # can be shrunk until nothing clashes: a plot on this street is 18-28 m wide and the rows
        # are 4-9 m apart in depth.
        require(abs(NEIGHBOUR_X - 30.0) < 1e-9 and abs(NEIGHBOUR_Z - 10.0) < 1e-9,
                f"the neighbourly radius is a plot's width across and a row's depth back "
                f"({NEIGHBOUR_X} x {NEIGHBOUR_Z})")
        spans = sorted(abs(a[1] - b[1]) for index, a in enumerate(placed) for b in placed[index + 1:]
                       if abs(a[2] - b[2]) <= NEIGHBOUR_Z and abs(a[1] - b[1]) <= NEIGHBOUR_X)
        require(spans and max(spans) <= NEIGHBOUR_X,
                f"-- and it really does reach neighbours: {len(spans)} pair(s) are inside it, the "
                f"widest {max(spans) if spans else 0:.0f} m apart")

        document, blob = _document(houses[0], built[houses[0]])
        require(len(document["meshes"][0]["primitives"]) == len(document["materials"]),
                "a house is one primitive per material, which is what §17.4 chunks by")
        require(all(material["extras"].get("surfaceClass") == "exterior"
                    for material in document["materials"]),
                "and every material says it is `exterior`, so the chunk builder reads it")
        require(len(blob) > 0 and document["buffers"][0]["byteLength"] == len(blob),
                "the buffer's declared length is the buffer")

    if failures:
        print(f"\nneighbourhood_gen: {len(failures)} claim(s) FAILED")
        return 1
    print("neighbourhood_gen: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--directory", type=Path, default=SOURCE)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    if not (args.directory / "layout.exterior.json").is_file():
        print(f"neighbourhood_gen: no exterior layout in {args.directory}", file=sys.stderr)
        return 1

    built = build(args.directory)
    args.output.mkdir(parents=True, exist_ok=True)
    for asset, faces in sorted(built.items()):
        document, blob = _document(asset, faces)
        gltf_io.write_glb(args.output / f"{asset}.glb", document, blob)
    # A variant the layout no longer names is a file nothing draws, and a stale tree is how
    # `HOUSE-00785` had every fence in the world one task out of date. Only this tool's own
    # output is removed, and only from its own directory.
    removed = 0
    for path in sorted(args.output.glob("MODEL_NB_*.glb")):
        if path.stem not in built:
            path.unlink()
            removed += 1
    print(report(built) if args.report else
          f"neighbourhood_gen: {len(built)} asset(s) -> "
          f"{args.output.relative_to(REPO) if args.output.is_relative_to(REPO) else args.output}"
          + (f", {removed} stale variant(s) removed" if removed else ""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
