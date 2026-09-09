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


class House:
    """One house's five numbers, and the palette it is painted with.

    Everything else -- where the windows go, how the roof meets the walls, how deep a reveal is --
    is the grammar's, so a new house on this street is five numbers and a palette index.
    """

    def __init__(self, name: str, width: float, depth: float, storeys: int, roof: str,
                 garage: str, porch: str, palette: int) -> None:
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
        self.palette = PALETTES[palette % len(PALETTES)]

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
TYPES = {
    "MODEL_NB_HOUSE_A": House("A", 11.0, 8.5, 2, "hip", "attached", "stoop", 0),
    "MODEL_NB_HOUSE_B": House("B", 9.5, 9.0, 2, "gable", "none", "full", 1),
    "MODEL_NB_HOUSE_C": House("C", 12.5, 7.0, 1, "hip", "detached", "stoop", 2),
}


def house_of(asset: str) -> tuple[House, bool]:
    """`(house, detailed)` for an asset id: `MODEL_NB_HOUSE_B_LOW` is B without its reveals."""
    base = asset[:-4] if asset.endswith("_LOW") else asset
    if base not in TYPES:
        raise layout_io.LayoutError(f"{asset!r} is not one of {sorted(TYPES)}")
    return TYPES[base], not asset.endswith("_LOW")


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


def house_faces(house: House, detailed: bool) -> list:
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

    if house.porch != "none":
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

    # 2. The three types the layout names differ in SHAPE and not only in paint: a street of one
    #    silhouette in three colours is a housing estate.
    require(sorted(TYPES) == ["MODEL_NB_HOUSE_A", "MODEL_NB_HOUSE_B", "MODEL_NB_HOUSE_C"],
            f"three types, which is what `HOUSE-00391` placed ({sorted(TYPES)})")
    shapes = {(h.width, h.depth, h.storeys, h.roof, h.garage, h.porch) for h in TYPES.values()}
    require(len(shapes) == 3, f"and no two of them are the same house ({len(shapes)} shapes)")
    require(len({h.storeys for h in TYPES.values()}) > 1
            and len({h.roof for h in TYPES.values()}) > 1,
            "-- they differ in storeys AND in roof, which is what reads at 90 m")

    # 3. Geometry: a house stands on the ground, its roof starts where its walls stop, and its
    #    ridge is over its own footprint.
    for asset, house in sorted(TYPES.items()):
        faces = house_faces(house, True)
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
        full = sum(1 if len(c) == 3 else 2 for c, _n, _m in house_faces(house, True))
        low = sum(1 if len(c) == 3 else 2 for c, _n, _m in house_faces(house, False))
        require(low < full,
                f"{house.name}_LOW is smaller than {house.name} ({low} against {full} triangles)")
        require(0.30 <= low / full <= 0.85,
                f"...and it is the massing rather than a decimation: {low / full:.2f} of it")

    # 5. The openings are on the STREET elevation and inside the wall they are cut into -- a
    #    window that overhangs its own wall is a window in the air.
    for asset, house in sorted(TYPES.items()):
        for x0, x1, y0, y1 in openings_on(house):
            require(-house.width / 2.0 <= x0 and x1 <= house.width / 2.0,
                    f"{house.name}: an opening at x {x0:.2f}..{x1:.2f} is inside its "
                    f"{house.width:.1f} m front")
            require(y1 <= house.eaves,
                    f"{house.name}: ...and under its eaves ({y1:.2f} of {house.eaves:.2f})")
    doors = [row for row in openings_on(TYPES["MODEL_NB_HOUSE_A"]) if row[2] <= 1e-9]
    require(len(doors) == 1, f"and exactly one of them reaches the ground: the door ({len(doors)})")

    # 6. The roof comes from `roof_geometry` and not from a second derivation.
    house = TYPES["MODEL_NB_HOUSE_A"]
    planes = roof_geometry.roof_planes(house.outer, house.eaves, HIP_PITCH)
    require(len(planes) == 4, f"a hip roof is `roof_geometry`'s four planes ({len(planes)})")
    roof_faces = [f for f in house_faces(house, True) if f[2] == house.palette.roof]
    require(len(roof_faces) >= 4,
            f"and the house draws them ({len(roof_faces)} roof faces, garage included)")
    gable = TYPES["MODEL_NB_HOUSE_B"]
    gable_faces = _roof_faces(gable, gable.outer, gable.eaves, "gable")
    require(sum(1 for corners, _n, _m in gable_faces if len(corners) == 3) == 2,
            "a GABLE is two slopes and two triangular ends, and the ends are wall")
    require(sum(1 for _c, _n, material in gable_faces if material == gable.palette.wall) == 2,
            "-- the ends are the WALL's material, because a gable end is a wall")

    # 7. The impostor is §26.1's two triangles and nothing else.
    card = impostor_faces()
    require(sum(1 if len(c) == 3 else 2 for c, _n, _m in card) == 3,
            f"an impostor is a quad and a gable -- three triangles, which is what a house 200 m "
            f"away is ({sum(1 if len(c) == 3 else 2 for c, _n, _m in card)})")
    require(len({material for _c, _n, material in card}) == 1,
            "and one material, so it is one draw")

    # 8. Against the layout the street was authored from.
    if (SOURCE / "layout.exterior.json").is_file():
        assets = wanted_assets(SOURCE)
        require(len(assets) == 7,
                f"the layout names seven neighbourhood assets -- three types, three `_LOW` and "
                f"the impostor card ({assets})")
        built = build(SOURCE)
        require(set(built) == set(assets),
                f"and every one of them is built ({sorted(set(assets) - set(built))} missing)")
        require(all(faces for faces in built.values()),
                "with geometry in each")
        document, blob = _document("MODEL_NB_HOUSE_A", built["MODEL_NB_HOUSE_A"])
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
    print(report(built) if args.report else
          f"neighbourhood_gen: {len(built)} asset(s) -> "
          f"{args.output.relative_to(REPO) if args.output.is_relative_to(REPO) else args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
