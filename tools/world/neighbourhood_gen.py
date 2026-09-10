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
import math
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


#: §11.4's horizon ring: "distant roof/gable silhouettes and tree lines". Four cards, because a
#: ring of thirty-six identical ones is the copy-pasted estate again at 200 m, and because the
#: only thing that reads at that range is the shape of a top edge (`HOUSE-00845`).
IMPOSTOR_KINDS = ("GABLE", "HIP", "TERRACE", "TREELINE")

#: What an impostor id looks like: `MODEL_NB_IMPOSTOR_<KIND>`.
IMPOSTOR_PREFIX = "MODEL_NB_IMPOSTOR_"


def impostor_faces(kind: str = "GABLE") -> list:
    """One horizon card: a quad standing on the ground with its own roofline cut into the top.

    §26.1 calls an impostor two triangles; a silhouette with a roof on it is three or four, and
    that is the whole content of the thing -- there is no other detail at 120-260 m, which is where
    `HOUSE-00391` put this ring. Everything is one material and one primitive, so a card is one
    draw.
    """
    if kind not in IMPOSTOR_KINDS:
        raise layout_io.LayoutError(f"impostor kind {kind!r} is not one of {list(IMPOSTOR_KINDS)}")
    front = (0.0, 0.0, 1.0)
    material = "NB_IMPOSTOR"

    def wall(half: float, top: float):
        return ([(-half, 0.0, 0.0), (half, 0.0, 0.0), (half, top, 0.0), (-half, top, 0.0)],
                front, material)

    if kind == "GABLE":
        half, eaves, ridge = 5.5, 5.1, 8.5
        return [wall(half, eaves),
                ([(-half, eaves, 0.0), (half, eaves, 0.0), (0.0, ridge, 0.0)], front, material)]
    if kind == "HIP":
        # A hip reads as a TRAPEZOID against the sky where a gable reads as a triangle, and that
        # difference is the whole reason this kind exists.
        half, eaves, ridge = 6.0, 5.4, 8.0
        return [wall(half, eaves),
                ([(-half, eaves, 0.0), (half, eaves, 0.0), (half * 0.35, ridge, 0.0),
                  (-half * 0.35, ridge, 0.0)], front, material)]
    if kind == "TERRACE":
        # Two roofs over one long wall: a row of houses, which is what most of a suburb's horizon
        # is. Wider and lower than a detached silhouette.
        half, eaves, ridge = 9.0, 4.8, 7.0
        return [wall(half, eaves),
                ([(-half, eaves, 0.0), (0.0, eaves, 0.0), (-half / 2.0, ridge, 0.0)], front,
                 material),
                ([(0.0, eaves, 0.0), (half, eaves, 0.0), (half / 2.0, ridge, 0.0)], front,
                 material)]
    # TREELINE: §11.4 asks for one, and a tree line is a wide low mass with a bumpy top rather
    # than a roof -- three crowns over a trunk band.
    half, band, crown = 15.0, 3.0, 11.5
    faces = [wall(half, band)]
    for index in range(3):
        centre = -half + half * (index + 0.5) * 2.0 / 3.0
        faces.append(([(centre - half / 3.2, band, 0.0), (centre + half / 3.2, band, 0.0),
                       (centre, crown - index % 2 * 2.0, 0.0)], front, material))
    return faces


# ================================================================================ the horizon

#: §11.4's far background -- "a low tree-line ridge and a distant water tower on the horizon ring"
#: -- which is a ring of its own, OUTSIDE `HOUSE-00391`'s 114-266 m impostors and inside §10.3's
#: 420 m far plane. 360 m is far enough that nothing on it has parallax over the 70 m the player
#: can walk, and near enough that §33's fog still has something to fade (`HOUSE-00848`).
HORIZON_RADIUS = 360.0
#: Twenty-four cards round the circle, 15 degrees each. The chord of 15 degrees at 360 m is
#: 93.96 m and a card is `HORIZON_WIDTH` wide, so neighbours OVERLAP rather than leaving a gap:
#: a horizon with 24 slots of sky in it is not a horizon.
HORIZON_SEGMENTS = 24
HORIZON_WIDTH = 96.0
#: Every kind starts and ends at this height, so one card meets the next without a step. The
#: profile between the ends is what makes them different landscapes.
HORIZON_EDGE = 6.0

HORIZON_PREFIX = "MODEL_HORIZON_"
#: What the ring is made of. Three, because two kinds alternating IS a repeat, just a slower one.
HORIZON_KINDS = ("RIDGE", "SPUR", "WOODS")
#: The profile of each kind, sampled evenly across the card. §11.4 asks for a LOW ridge: 22 m at
#: 360 m is 3.5 degrees, which is a hill on the skyline rather than a mountain behind the house.
HORIZON_PROFILES = {
    "RIDGE": (6.0, 12.0, 18.5, 22.0, 20.5, 17.0, 13.0, 9.0, 6.0),
    "SPUR": (6.0, 7.5, 9.5, 11.5, 13.5, 14.5, 12.5, 9.0, 6.0),
    "WOODS": (6.0, 13.5, 9.0, 15.0, 10.5, 14.0, 9.5, 12.5, 6.0),
}
#: How tall the trees on the ridge are, and how many. §11.4's phrase is "tree-line ridge", so the
#: RIDGE carries them; the SPUR is bare pasture and the WOODS are trees on flat ground, which is
#: the difference between the three things you see on a horizon.
HORIZON_CROWN = 3.2
HORIZON_CROWNS = 5

WATER_TOWER_ASSET = "MODEL_WATER_TOWER"
#: §11.4's water tower, as a silhouette: the tank sits on `TOWER_LEGS` metres of leg and the whole
#: thing is `TOWER_TOP` tall. At 360 m that is 5.4 degrees -- a landmark you can point at, which is
#: what a water tower on a horizon is for.
TOWER_LEGS = 22.0
TOWER_TANK = 31.0
TOWER_TOP = 34.5
TOWER_HALF = 3.6


def horizon_of(asset: str) -> str:
    """The kind in `MODEL_HORIZON_<KIND>`, refused if it is not one of `HORIZON_KINDS`."""
    if not asset.startswith(HORIZON_PREFIX):
        raise layout_io.LayoutError(f"{asset!r} is not a horizon card")
    kind = asset[len(HORIZON_PREFIX):]
    if kind not in HORIZON_KINDS:
        raise layout_io.LayoutError(
            f"horizon kind {kind!r} is not one of {list(HORIZON_KINDS)}")
    return kind


def horizon_faces(kind: str) -> list:
    """One segment of §11.4's horizon: a strip of landscape standing on the ring.

    A vertical strip rather than a quad with a roof cut into it, because a ridge is a CURVE and the
    thing that reads at 360 m is its line against the sky. Eight quads carry that; the impostor
    cards nearer in (`HOUSE-00845`) carry three to five triangles because a roof is straight.
    """
    if kind not in HORIZON_KINDS:
        raise layout_io.LayoutError(
            f"horizon kind {kind!r} is not one of {list(HORIZON_KINDS)}")
    profile = HORIZON_PROFILES[kind]
    front = (0.0, 0.0, 1.0)
    half = HORIZON_WIDTH / 2.0
    steps = len(profile) - 1
    # A wood is trees and a ridge is ground: the material says which, so the blockout reads as two
    # kinds of landscape rather than one shape drawn twice.
    ground = "NB_HORIZON_TREES" if kind == "WOODS" else "NB_HORIZON"
    faces = []
    for index in range(steps):
        u0 = -half + HORIZON_WIDTH * index / steps
        u1 = -half + HORIZON_WIDTH * (index + 1) / steps
        faces.append(([(u0, 0.0, 0.0), (u1, 0.0, 0.0), (u1, profile[index + 1], 0.0),
                       (u0, profile[index], 0.0)], front, ground))
    if kind == "RIDGE":
        # §11.4's "tree-line ridge": the trees are ON it, standing along the skyline rather than
        # beside it. Crowns only -- at 360 m a trunk is a tenth of a pixel.
        for index in range(HORIZON_CROWNS):
            at = -half + HORIZON_WIDTH * (index + 1.0) / (HORIZON_CROWNS + 1.0)
            base = _profile_at(profile, at, half)
            width = HORIZON_WIDTH / (HORIZON_CROWNS * 3.0)
            faces.append(([(at - width, base - 0.5, 0.0), (at + width, base - 0.5, 0.0),
                           (at, base + HORIZON_CROWN, 0.0)], front, "NB_HORIZON_TREES"))
    return faces


def _profile_at(profile, at: float, half: float) -> float:
    """The profile's height @p at metres from the card's centre, linearly between samples."""
    steps = len(profile) - 1
    unit = (at + half) / (2.0 * half) * steps
    low = max(0, min(steps - 1, int(unit)))
    return profile[low] + (profile[low + 1] - profile[low]) * (unit - low)


def water_tower_faces() -> list:
    """§11.4's water tower, as a card: legs, bracing, tank and a conical top.

    A card and not a solid, for the same reason the ring is cards: at 360 m a leg is 0.08 degrees
    wide and the whole of the thing that reaches the player is its outline against the sky.
    """
    front = (0.0, 0.0, 1.0)
    material = "NB_HORIZON_TOWER"
    faces = []
    for side in (-1.0, 1.0):
        # The legs splay, which is what stops a water tower reading as a chimney.
        foot, top = side * (TOWER_HALF + 1.5), side * (TOWER_HALF - 0.6)
        for offset in (0.0, side * 1.9):
            faces.append(([(foot + offset, 0.0, 0.0), (foot + offset + side * 0.45, 0.0, 0.0),
                           (top + offset + side * 0.45, TOWER_LEGS, 0.0),
                           (top + offset, TOWER_LEGS, 0.0)], front, material))
    for height in (TOWER_LEGS * 0.38, TOWER_LEGS * 0.72):
        span = TOWER_HALF + 1.5 - (1.5 + 0.6) * height / TOWER_LEGS
        faces.append(([(-span, height, 0.0), (span, height, 0.0),
                       (span, height + 0.35, 0.0), (-span, height + 0.35, 0.0)], front, material))
    faces.append(([(-TOWER_HALF, TOWER_LEGS, 0.0), (TOWER_HALF, TOWER_LEGS, 0.0),
                   (TOWER_HALF, TOWER_TANK, 0.0), (-TOWER_HALF, TOWER_TANK, 0.0)], front,
                  material))
    faces.append(([(-TOWER_HALF, TOWER_TANK, 0.0), (TOWER_HALF, TOWER_TANK, 0.0),
                   (0.0, TOWER_TOP, 0.0)], front, material))
    return faces


# ============================================================================= street furniture

#: Where §11.4's nine street lanterns HANG: `layout.lights.json`'s own `LIGHT_EXT_STREET_1`…`_9`,
#: at `[x, 8.00, 1.50]` for x = -100…100 every 25 m. The geometry below is built to put a lantern
#: at exactly that point, because a light with nothing holding it up is a light shining out of the
#: air (`HOUSE-00846`).
LAMP_HEIGHT = 8.0
#: The posts stand in one line along the verge at Z +0.80 -- the line §11.4's utility poles were
#: already on -- so the lantern reaches the light's Z +1.50 on a 0.70 m outreach arm.
POST_LINE_Z = 0.80
LAMP_REACH = 1.50 - POST_LINE_Z
#: How far the outreach arm rises above the lantern, and so how tall a lamp column is.
LAMP_TOP = 0.18

#: A wooden utility pole, tall enough that the wires it carries clear a lamp column standing
#: mid-span: the wires leave the crossarm at `WIRE_ATTACH` and sag `SPAN_SAG` in the middle, which
#: has to stay above the 8.00 m lantern with room to see daylight between them.
POLE_HEIGHT = 10.5
CROSSARM_HEIGHT = 9.6
WIRE_ATTACH = 9.9
#: §11.4 puts the poles 50 m apart. A span with no sag is a wire under a tension no pole survives.
SPAN_LENGTH = 50.0
SPAN_SAG = 1.2
#: Three wires, at the three insulators: one over the pole and one at each end of the crossarm.
WIRE_OFFSETS = (-0.95, 0.0, 0.95)
#: A catenary drawn as straight segments, which is what a wire is at 50 m when it is made of boxes.
#: Eight, measured: the worst a chord strays from the curve is 18.8 mm at eight, 33.3 mm at six and
#: 75.0 mm at four. The wire is 40 mm thick, so at eight the error is inside the wire.
SPAN_SEGMENTS = 8

STREET_LIGHT_ASSET = "MODEL_STREET_LIGHT"
UTILITY_POLE_ASSET = "MODEL_UTILITY_POLE"
#: The pole that also carries a lantern. All five of §11.4's poles stand at an X where §11.4 also
#: wants a street light, and two posts 0.70 m apart read as one doubled pole from the garden -- so
#: the lantern is mounted on the pole, the way a suburban street actually does it. The plain pole
#: stays in the grammar for a position that has no lamp; `build` only draws what the layout names.
UTILITY_POLE_LAMP_ASSET = "MODEL_UTILITY_POLE_LAMP"
UTILITY_SPAN_ASSET = "MODEL_UTILITY_SPAN"

FURNITURE_KINDS = (STREET_LIGHT_ASSET, UTILITY_POLE_ASSET, UTILITY_POLE_LAMP_ASSET,
                   UTILITY_SPAN_ASSET, "MODEL_STREET_SIGN", "MODEL_MAILBOX", "MODEL_BIN_CLUSTER",
                   "MODEL_BASKETBALL_HOOP")


def _lantern_faces() -> list:
    """The outreach arm and the lantern, from a post whose base is the origin.

    Shared by the standalone column and the lamp-carrying pole, so there is ONE answer to where a
    street lantern is and both assets put it in the same place.
    """
    faces = box((-0.05, LAMP_HEIGHT + 0.06, 0.0), (0.05, LAMP_HEIGHT + LAMP_TOP, LAMP_REACH),
                "NB_METAL_GREY")
    # Centred on `LAMP_HEIGHT`, because that is where the LIGHT is: a lantern whose glass is
    # 60 mm below its own light source is the same mistake as one 1.6 m away from it, smaller.
    faces += box((-0.17, LAMP_HEIGHT - 0.09, LAMP_REACH - 0.26),
                 (0.17, LAMP_HEIGHT + 0.09, LAMP_REACH + 0.26), "NB_LAMP_GLASS")
    return faces


def _pole_faces() -> list:
    """The pole and its crossarm, without the lantern."""
    faces = box((-0.13, 0.0, -0.13), (0.13, POLE_HEIGHT, 0.13), "NB_TIMBER_POLE")
    faces += box((-0.05, CROSSARM_HEIGHT, WIRE_OFFSETS[0] - 0.10),
                 (0.05, CROSSARM_HEIGHT + 0.12, WIRE_OFFSETS[-1] + 0.10), "NB_TIMBER_POLE")
    for at in WIRE_OFFSETS:
        faces += box((-0.05, CROSSARM_HEIGHT + 0.12, at - 0.05),
                     (0.05, WIRE_ATTACH, at + 0.05), "NB_METAL_GREY")
    return faces


def _wire_faces() -> list:
    """One 50 m span of §11.4's catenary, from the pole at the origin running +X to the next."""
    faces: list = []
    for offset in WIRE_OFFSETS:
        for index in range(SPAN_SEGMENTS):
            u0 = SPAN_LENGTH * index / SPAN_SEGMENTS
            u1 = SPAN_LENGTH * (index + 1) / SPAN_SEGMENTS
            faces += _wire_segment(u0, wire_height(u0), u1, wire_height(u1), offset)
    return faces


def wire_height(at: float) -> float:
    """How high the wire is @p at metres along a span -- `WIRE_ATTACH` at each end, sagging between.

    A parabola rather than a true `cosh`: over 50 m with 1.2 m of sag the two curves differ by
    **0.23 mm** at worst (the catenary parameter is 260.6 m), which is a fortieth of the wire's own
    thickness -- and the parabola is the one that is exactly `WIRE_ATTACH` at both poles.
    """
    unit = 2.0 * at / SPAN_LENGTH - 1.0
    return WIRE_ATTACH - SPAN_SAG * (1.0 - unit * unit)


def _wire_segment(u0: float, y0: float, u1: float, y1: float, offset: float,
                  thickness: float = 0.04) -> list:
    """One straight length of wire from `(u0, y0)` to `(u1, y1)`, at Z @p offset."""
    half = thickness / 2.0
    z0, z1 = offset - half, offset + half
    return [
        ([(u0, y0 - half, z1), (u1, y1 - half, z1), (u1, y1 + half, z1), (u0, y0 + half, z1)],
         (0.0, 0.0, 1.0), "NB_WIRE"),
        ([(u1, y1 - half, z0), (u0, y0 - half, z0), (u0, y0 + half, z0), (u1, y1 + half, z0)],
         (0.0, 0.0, -1.0), "NB_WIRE"),
        ([(u0, y0 + half, z1), (u1, y1 + half, z1), (u1, y1 + half, z0), (u0, y0 + half, z0)],
         (0.0, 1.0, 0.0), "NB_WIRE"),
        ([(u0, y0 - half, z0), (u1, y1 - half, z0), (u1, y1 - half, z1), (u0, y0 - half, z1)],
         (0.0, -1.0, 0.0), "NB_WIRE"),
    ]


def _rotate(x: float, z: float, yaw_deg: float) -> tuple[float, float]:
    """`build_chunks.place`'s own yaw, so a point computed here is where the chunk builder puts it."""
    yaw = math.radians(yaw_deg)
    cos, sin = math.cos(yaw), math.sin(yaw)
    return (x * cos + z * sin, -x * sin + z * cos)


def carries_lantern(asset: str) -> bool:
    """Whether @p asset draws one of §11.4's nine street lanterns."""
    return asset in (STREET_LIGHT_ASSET, UTILITY_POLE_LAMP_ASSET)


def lantern_world(position, yaw_deg: float = 0.0) -> tuple[float, float, float]:
    """Where the lantern of a lamp-carrying asset placed at @p position ends up, in world metres.

    The point `layout.lights.json` has to agree with: a street light is a light AND a lamp, and
    the two being 1.6 m apart is the sort of thing only a screenshot ever finds.
    """
    x, z = _rotate(0.0, LAMP_REACH, yaw_deg)
    return (position[0] + x, position[1] + LAMP_HEIGHT, position[2] + z)


def span_ends(position, yaw_deg: float = 0.0) -> tuple[tuple[float, float], tuple[float, float]]:
    """The `(x, z)` of the two poles a span placed at @p position hangs between."""
    near = _rotate(0.0, 0.0, yaw_deg)
    far = _rotate(SPAN_LENGTH, 0.0, yaw_deg)
    return ((position[0] + near[0], position[2] + near[1]),
            (position[0] + far[0], position[2] + far[1]))


def furniture_faces(asset: str) -> list:
    """One piece of §11.4's street furniture, in world metres, its foot at the origin.

    Boxes, like the fences and the garden structures: at 20-60 m these are silhouettes with a
    scale, and the SCALE is the part that has to be right -- a mailbox the size of a wheelie bin
    reads as a bin, whatever it is textured with later.
    """
    if asset == STREET_LIGHT_ASSET:
        return (box((-0.09, 0.0, -0.09), (0.09, LAMP_HEIGHT + LAMP_TOP, 0.09), "NB_METAL_GREY")
                + _lantern_faces())
    if asset == UTILITY_POLE_ASSET:
        return _pole_faces()
    if asset == UTILITY_POLE_LAMP_ASSET:
        return _pole_faces() + _lantern_faces()
    if asset == UTILITY_SPAN_ASSET:
        return _wire_faces()
    if asset == "MODEL_STREET_SIGN":
        faces = box((-0.04, 0.0, -0.04), (0.04, 2.30, 0.04), "NB_METAL_GREY")
        faces += box((-0.02, 1.85, -0.35), (0.02, 2.25, 0.35), "NB_SIGN_PLATE")
        return faces
    if asset == "MODEL_MAILBOX":
        faces = box((-0.05, 0.0, -0.05), (0.05, 1.05, 0.05), "NB_TIMBER_POLE")
        faces += box((-0.10, 1.05, -0.24), (0.10, 1.30, 0.24), "NB_METAL_GREY")
        return faces
    if asset == "MODEL_BIN_CLUSTER":
        # §11.4's "rubbish-bin clusters (only on the in-fiction collection day)": three wheelie
        # bins side by side, which is what a kerb looks like on a Thursday.
        faces: list = []
        for index in range(3):
            centre = (index - 1) * 0.70
            faces += box((centre - 0.29, 0.0, -0.32), (centre + 0.29, 1.05, 0.32), "NB_BIN")
            faces += box((centre - 0.31, 1.05, -0.34), (centre + 0.31, 1.11, 0.30), "NB_BIN_LID")
        return faces
    if asset == "MODEL_BASKETBALL_HOOP":
        # The ring is at 3.05 m because that is what a basketball ring is, everywhere.
        faces = box((-0.08, 0.0, -0.08), (0.08, 3.40, 0.08), "NB_METAL_GREY")
        faces += box((-0.90, 2.95, 0.08), (0.90, 4.05, 0.16), "NB_BACKBOARD")
        faces += box((-0.23, 3.02, 0.16), (0.23, 3.08, 0.61), "NB_METAL_RING")
        return faces
    raise layout_io.LayoutError(
        f"{asset!r} is not one of §11.4's street furniture: {list(FURNITURE_KINDS)}")


def impostor_of(asset: str) -> str:
    """The KIND an impostor asset id names, or an error saying which kinds there are."""
    if not asset.startswith(IMPOSTOR_PREFIX):
        raise layout_io.LayoutError(f"{asset!r} is not a neighbourhood impostor id")
    kind = asset[len(IMPOSTOR_PREFIX):]
    if kind not in IMPOSTOR_KINDS:
        raise layout_io.LayoutError(
            f"{asset!r} names impostor kind {kind!r}, which is not one of {list(IMPOSTOR_KINDS)}")
    return kind


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


def is_ours(asset: str) -> bool:
    """Whether @p asset is one THIS tool draws.

    The neighbourhood also names §11.4's vehicles (`HOUSE-00847`), which are not this grammar's;
    the test is used both to pick what to build and to decide what a stale file in the output
    directory is, so a file another tool wrote is never swept away by this one.
    """
    return (asset.startswith((HOUSE_PREFIX, IMPOSTOR_PREFIX, HORIZON_PREFIX))
            or asset in FURNITURE_KINDS or asset == WATER_TOWER_ASSET)


def wanted_assets(directory: Path) -> list[str]:
    """Every neighbourhood asset the layout asks for, in id order."""
    layout = layout_io.load_layout(directory, kinds=["exterior"])
    exterior = layout.get("exterior") or {}
    return sorted({row["asset"] for row in exterior.get("neighbourhood", [])
                   if is_ours(str(row.get("asset", "")))})


def build(directory: Path) -> dict:
    """`{asset: faces}` for every neighbourhood asset the layout names."""
    out: dict[str, list] = {}
    for asset in wanted_assets(directory):
        if asset.startswith(IMPOSTOR_PREFIX):
            out[asset] = impostor_faces(impostor_of(asset))
            continue
        if asset in FURNITURE_KINDS:
            out[asset] = furniture_faces(asset)
            continue
        if asset.startswith(HORIZON_PREFIX):
            out[asset] = horizon_faces(horizon_of(asset))
            continue
        if asset == WATER_TOWER_ASSET:
            out[asset] = water_tower_faces()
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
        require(not low, f"{house.name}_LOW has no drive: §26.2's LOD2 is the massing alone")

    # 7. `HOUSE-00845`: the horizon ring's four cards. §26.1 calls an impostor two triangles; a
    #    silhouette with a roofline is three to six, and that is the whole content of one.
    for kind in IMPOSTOR_KINDS:
        card = impostor_faces(kind)
        count = sum(1 if len(c) == 3 else 2 for c, _n, _m in card)
        require(2 <= count <= 8,
                f"the {kind} card is {count} triangles -- a silhouette and its roofline, which is "
                f"all that reads at 120-260 m")
        require(len({material for _c, _n, material in card}) == 1,
                f"...and one material, so the {kind} card is one draw")
        require(all(abs(point[2]) < 1e-9 for corners, _n, _m in card for point in corners),
                f"...and it is a CARD: every corner of {kind} is in one plane")
        ys = [point[1] for corners, _n, _m in card for point in corners]
        require(abs(min(ys)) < 1e-9 and 6.0 <= max(ys) <= 14.0,
                f"...standing on the ground and {max(ys):.1f} m tall, which is a house or a tree "
                f"and not a fence or a tower")
    tops = {kind: round(max(point[1] for corners, _n, _m in impostor_faces(kind)
                            for point in corners), 2) for kind in IMPOSTOR_KINDS}
    widths = {kind: round(max(point[0] for corners, _n, _m in impostor_faces(kind)
                              for point in corners) * 2.0, 1) for kind in IMPOSTOR_KINDS}
    require(len(set(tops.values())) == len(IMPOSTOR_KINDS)
            and len(set(widths.values())) == len(IMPOSTOR_KINDS),
            f"and no two kinds are the same silhouette: {tops} over {widths}")
    for bad in ("MODEL_NB_IMPOSTOR_CARD", "MODEL_NB_IMPOSTOR_", "MODEL_NB_HOUSE_A_CREAM"):
        try:
            impostor_of(bad)
            caught = False
        except layout_io.LayoutError:
            caught = True
        require(caught, f"{bad} is refused as an impostor id, not built as some default card")

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

        # `HOUSE-00845`: the ring. Thirty-six cards, four kinds, and no two NEIGHBOURS on the ring
        # alike -- the same rule as the street's palettes, applied round a circle, where "adjacent"
        # is the next card by bearing and the last one wraps to the first.
        ring = sorted(((row["id"], impostor_of(row["asset"]),
                        math.degrees(math.atan2(row["position"][0], row["position"][2])) % 360.0)
                       for row in rows
                       if str(row.get("asset", "")).startswith(IMPOSTOR_PREFIX)),
                      key=lambda entry: entry[2])
        require(len(ring) == 36, f"§11.4's thirty-six distant silhouettes ({len(ring)})")
        repeats = [(ring[index][0], ring[(index + 1) % len(ring)][0], ring[index][1])
                   for index in range(len(ring))
                   if ring[index][1] == ring[(index + 1) % len(ring)][1]]
        require(not repeats,
                f"and no two next to each other on the ring are the same card, the wrap from the "
                f"last to the first included ({repeats})")
        spread = {kind: sum(1 for entry in ring if entry[1] == kind) for kind in IMPOSTOR_KINDS}
        require(min(spread.values()) >= 6,
                f"and every kind is on the horizon, not one of them once: {spread}")

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

        # 9. `HOUSE-00846`'s street furniture. The claim that matters is the one nobody could
        #    have made before this task existed: every one of §11.4's nine street lights now has
        #    a lamp under it, at the point the LIGHT is at and not near it.
        furniture = [row for row in rows if is_ours(str(row.get("asset", "")))
                     and str(row["asset"]) in FURNITURE_KINDS]
        # §11.4's vehicles share the `neighbourhood` array and are `HOUSE-00847`'s, not this
        # grammar's. `is_ours` is what keeps this tool's stale sweep from deleting them.
        theirs = sorted({str(row["asset"]) for row in rows if not is_ours(str(row["asset"]))})
        require(theirs == ["MODEL_DELIVERY_VAN", "MODEL_PARKED_CAR"],
                f"the vehicles in the array are somebody else's to draw ({theirs})")
        lanterns = sorted(lantern_world(row["position"], float(row.get("yawDeg", 0.0)))
                          for row in furniture if carries_lantern(str(row["asset"])))
        street_lights = sorted(tuple(float(c) for c in light["position"])
                               for light in layout_io.rows(
                                   layout_io.load_layout(SOURCE, kinds=["lights"]), "lights")
                               if str(light["id"]).startswith("LIGHT_EXT_STREET_"))
        require(len(street_lights) == 9, f"§11.4's nine street lights ({len(street_lights)})")
        require(len(lanterns) == len(street_lights),
                f"and nine lanterns to hang them in ({len(lanterns)})")
        worst = max((max(abs(a - b) for a, b in zip(lantern, light))
                     for lantern, light in zip(lanterns, street_lights)), default=None)
        require(worst is not None and worst < 1e-6,
                f"every lantern is AT its light, not near it (worst axis {worst} m)")

        # ...and `lantern_world` is telling the truth about the GEOMETRY, not just about its own
        # constants: the glass a lamp-carrying asset draws is where that function says it is.
        def glass_centre(asset: str):
            points = [point for corners, _n, material in built[asset]
                      if material == "NB_LAMP_GLASS" for point in corners]
            return (tuple(sum(point[axis] for point in points) / len(points) for axis in range(3))
                    if points else None)

        drawn = {asset: glass_centre(asset) for asset in built if asset in FURNITURE_KINDS}
        missing = sorted(asset for asset, centre in drawn.items()
                         if carries_lantern(asset) and centre is None)
        require(not missing, f"a lamp-carrying asset draws a lantern ({missing})")
        stray = sorted(asset for asset, centre in drawn.items()
                       if not carries_lantern(asset) and centre is not None)
        require(not stray, f"and one that is not lamp-carrying draws none ({stray})")
        off = {asset: centre for asset, centre in drawn.items() if centre is not None
               and max(abs(a - b) for a, b in zip(centre, lantern_world((0.0, 0.0, 0.0)))) > 1e-6}
        require(not off,
                f"and the glass really is at {lantern_world((0.0, 0.0, 0.0))}, which is what "
                f"`lantern_world` promised the light ({off})")

        # ...and the wires reach from pole to pole. A span that ends in mid-air is a wire hanging
        # off the end of the street.
        poles = sorted((round(row["position"][0], 3), round(row["position"][2], 3))
                       for row in furniture
                       if str(row["asset"]) in (UTILITY_POLE_ASSET, UTILITY_POLE_LAMP_ASSET))
        require(len(poles) == 5, f"§11.4's five utility poles ({len(poles)})")
        spans = [span_ends(row["position"], float(row.get("yawDeg", 0.0)))
                 for row in furniture if str(row["asset"]) == UTILITY_SPAN_ASSET]
        require(len(spans) == len(poles) - 1,
                f"and one span between each neighbouring pair of them ({len(spans)})")
        dangling = [end for span in spans for end in span
                    if (round(end[0], 3), round(end[1], 3)) not in poles]
        require(not dangling, f"with both ends of every wire ON a pole ({dangling})")

        # Nothing standing on the verge grows into a wire. The lamp columns are the tall ones and
        # they stand at the MIDDLE of a span, which is exactly where a catenary is lowest.
        heights = {asset: max(point[1] for corners, _n, _m in faces for point in corners)
                   for asset, faces in built.items() if asset in FURNITURE_KINDS}
        under_wire = []
        for row in furniture:
            asset = str(row["asset"])
            if asset in (UTILITY_SPAN_ASSET, UTILITY_POLE_ASSET, UTILITY_POLE_LAMP_ASSET):
                continue
            for (x0, z0), (x1, _z1) in spans:
                if min(x0, x1) <= row["position"][0] <= max(x0, x1) and abs(row["position"][2] - z0) < 2.0:
                    clear = wire_height(abs(row["position"][0] - x0)) - heights[asset]
                    if clear < 0.30:
                        under_wire.append((row["id"], round(clear, 3)))
        require(not under_wire,
                f"nothing on the verge reaches into the catenary above it ({under_wire})")
        require(abs(wire_height(0.0) - WIRE_ATTACH) < 1e-9
                and abs(wire_height(SPAN_LENGTH) - WIRE_ATTACH) < 1e-9
                and abs(wire_height(SPAN_LENGTH / 2.0) - (WIRE_ATTACH - SPAN_SAG)) < 1e-9,
                f"and the wire really does sag: {WIRE_ATTACH:.2f} m at each pole, "
                f"{wire_height(SPAN_LENGTH / 2.0):.2f} m in the middle")

        # §11.4's carriageway is 7.0 m between kerbs at Z +3.2 and +10.2. A bin in it is a bin a
        # car drives through; the vehicles that ARE parked there are `HOUSE-00847`'s and are not
        # this grammar's.
        in_road = [row["id"] for row in furniture if 3.2 < row["position"][2] < 10.2]
        require(not in_road, f"no street furniture stands in the carriageway ({in_road})")

        # Scale, which is the whole job at 20-60 m: these are silhouettes, and a mailbox as tall
        # as a lamp post is a lamp post.
        expected = {STREET_LIGHT_ASSET: (8.1, 8.2), UTILITY_POLE_LAMP_ASSET: (10.5, 10.6),
                    UTILITY_POLE_ASSET: (10.5, 10.6), "MODEL_STREET_SIGN": (2.3, 2.4),
                    "MODEL_MAILBOX": (1.2, 1.4), "MODEL_BIN_CLUSTER": (1.0, 1.2),
                    "MODEL_BASKETBALL_HOOP": (4.0, 4.1)}
        wrong = {asset: round(heights[asset], 3) for asset, (low, high) in expected.items()
                 if asset in heights and not low <= heights[asset] <= high}
        require(not wrong, f"every piece of furniture is the height it is in life ({wrong})")
        # The plain pole is the one exception: every pole on this street carries a lantern, so
        # `build` never draws it, which is ADR-0013's "only the combinations the layout names".
        unnamed = sorted(set(expected) - set(heights) - {UTILITY_POLE_ASSET})
        require(not unnamed, f"and the street names all of them but the plain pole ({unnamed})")
        require(UTILITY_POLE_ASSET not in built,
                "-- which is not built, because nothing asks for one")

        try:
            furniture_faces("MODEL_TARDIS")
            caught = False
        except layout_io.LayoutError:
            caught = True
        require(caught, "an asset this grammar does not know is refused, not built as a box")

        # 10. `HOUSE-00848`'s far background: §11.4's "low tree-line ridge and a distant water
        #     tower on the horizon ring", which is a ring of its own beyond the impostors.
        for bad in ("MODEL_HORIZON_ALPS", "MODEL_HORIZON_", "MODEL_NB_IMPOSTOR_GABLE"):
            try:
                horizon_of(bad)
                caught = False
            except layout_io.LayoutError:
                caught = True
            require(caught, f"{bad} is refused as a horizon id, not drawn as some default land")
        edges = {kind: (HORIZON_PROFILES[kind][0], HORIZON_PROFILES[kind][-1])
                 for kind in HORIZON_KINDS}
        require(all(low == HORIZON_EDGE and high == HORIZON_EDGE
                    for low, high in edges.values()),
                f"every kind starts and ends at {HORIZON_EDGE:.2f} m, so one card meets the next "
                f"without a step in the skyline ({edges})")
        chord = 2.0 * HORIZON_RADIUS * math.sin(math.pi / HORIZON_SEGMENTS)
        require(HORIZON_WIDTH > chord,
                f"a card ({HORIZON_WIDTH:.2f} m) is wider than the chord it spans ({chord:.2f} m), "
                f"so neighbours overlap rather than leaving sky between them")
        crests = {kind: max(HORIZON_PROFILES[kind]) for kind in HORIZON_KINDS}
        require(len(set(crests.values())) == len(HORIZON_KINDS),
                f"no two kinds reach the same height ({crests})")

        def humps(profile) -> int:
            return sum(1 for index in range(1, len(profile) - 1)
                       if profile[index] > profile[index - 1] and profile[index] > profile[index + 1])

        shape = {kind: humps(HORIZON_PROFILES[kind]) for kind in HORIZON_KINDS}
        require(shape["WOODS"] > shape["RIDGE"] and shape["WOODS"] > shape["SPUR"],
                f"a wood is a bumpy line and a hill is one crest, which is what makes them two "
                f"kinds and not one drawn twice ({shape})")
        materials = {kind: sorted({material for _c, _n, material in horizon_faces(kind)})
                     for kind in HORIZON_KINDS}
        require(materials["RIDGE"] == ["NB_HORIZON", "NB_HORIZON_TREES"],
                f"§11.4's ridge carries its TREE LINE ({materials['RIDGE']})")
        require(materials["SPUR"] == ["NB_HORIZON"] and materials["WOODS"] == ["NB_HORIZON_TREES"],
                f"the spur is bare and the wood is all trees ({materials})")

        # ...and the ring the layout puts them on.
        ring = [(row["id"], row["position"][0], row["position"][2], str(row["asset"]),
                 float(row.get("yawDeg", 0.0)))
                for row in rows if str(row.get("asset", "")).startswith(HORIZON_PREFIX)]
        require(len(ring) == HORIZON_SEGMENTS,
                f"§11.4's horizon is {HORIZON_SEGMENTS} cards ({len(ring)})")
        radii = [math.hypot(x, z) for _id, x, z, _a, _y in ring]
        require(all(abs(radius - HORIZON_RADIUS) < 0.02 for radius in radii),
                f"all of them on one ring at {HORIZON_RADIUS:.0f} m "
                f"({min(radii):.2f}-{max(radii):.2f})")
        impostor_radius = max(math.hypot(row["position"][0], row["position"][2]) for row in rows
                              if str(row.get("asset", "")).startswith(IMPOSTOR_PREFIX))
        require(min(radii) > impostor_radius,
                f"beyond `HOUSE-00391`'s impostors, which reach {impostor_radius:.0f} m")
        require(max(radii) < 420.0,
                f"and inside §10.3's 420 m far plane ({max(radii):.0f} m)")
        facing = [(name, round((math.degrees(math.atan2(x, z)) + 180.0) % 360.0 - yaw % 360.0, 3))
                  for name, x, z, _a, yaw in ring]
        require(all(abs(error) < 0.05 for _n, error in facing),
                f"every card faces the origin ({[f for f in facing if abs(f[1]) >= 0.05]})")
        order = [asset for _id, _x, _z, asset, _y in
                 sorted(ring, key=lambda r: math.degrees(math.atan2(r[1], r[2])) % 360.0)]
        adjacent = [(a, b) for a, b in zip(order, order[1:] + order[:1]) if a == b]
        require(not adjacent,
                f"no two next to each other on the ring are the same land, the wrap included "
                f"({adjacent})")
        counts = {kind: order.count(HORIZON_PREFIX + kind) for kind in HORIZON_KINDS}
        require(set(counts.values()) == {HORIZON_SEGMENTS // len(HORIZON_KINDS)},
                f"and each kind carries an equal share of the horizon ({counts})")

        towers = [row for row in rows if str(row.get("asset", "")) == WATER_TOWER_ASSET]
        require(len(towers) == 1, f"§11.4's ONE water tower ({len(towers)})")
        if towers:
            tower_radius = math.hypot(towers[0]["position"][0], towers[0]["position"][2])
            require(tower_radius < min(radii),
                    f"standing inside the skyline at {tower_radius:.0f} m, so its legs read "
                    f"against the sky and not against a hillside")
            top = max(point[1] for corners, _n, _m in water_tower_faces() for point in corners)
            require(top > max(crests.values()),
                    f"and over the top of it: {top:.1f} m against the tallest land at "
                    f"{max(crests.values()):.1f} m -- a landmark you can point at")
            bearing = math.degrees(math.atan2(towers[0]["position"][0],
                                              towers[0]["position"][2])) % 360.0
            gap = min(abs(((bearing - math.degrees(math.atan2(x, z))) + 180.0) % 360.0 - 180.0)
                      for _id, x, z, _a, _y in ring)
            require(gap > 360.0 / HORIZON_SEGMENTS / 4.0,
                    f"and between two cards rather than in front of one ({gap:.1f} degrees off "
                    f"the nearest card's centre)")

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
    for path in sorted(args.output.glob("MODEL_*.glb")):
        if is_ours(path.stem) and path.stem not in built:
            path.unlink()
            removed += 1
    print(report(built) if args.report else
          f"neighbourhood_gen: {len(built)} asset(s) -> "
          f"{args.output.relative_to(REPO) if args.output.is_relative_to(REPO) else args.output}"
          + (f", {removed} stale variant(s) removed" if removed else ""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
