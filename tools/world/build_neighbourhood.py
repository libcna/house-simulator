#!/usr/bin/env python3
"""build_neighbourhood.py -- the neighbourhood's meshes, as one file the runtime can open.

`HOUSE-00856`. `neighbourhood_gen.py` has written `build/neighbourhood/<ASSET>.glb` since
`HOUSE-00841` and **nothing read that directory**: `grep` found the path twice, both times inside
the generator that writes it. Every other generated exterior tree has a consumer -- `build/terrain`
and `build/fence` are `build_chunks.py`'s exterior dirs, `build/shell` is its shell dirs -- and the
neighbourhood cannot take that route, for the reason this file exists.

## Why the neighbourhood is not chunked

`docs/chunk-format.md` §1: *"a chunk has no transform, so a prop's position and yaw are in the
geometry or they are nowhere"*. That is right for a prop, which stands in one place for ever, and
wrong for a neighbour:

* §11.4 places **122 instances of 34 assets**. Baking the placement writes the same house out once
  per instance -- twenty-four houses where there are nineteen meshes, thirty-six impostor cards
  where there are four.
* §26.1 selects an instance's LOD **by its projected height**, and §26.2's impostor takes over at
  `impostorFrom`. A baked instance cannot change what it draws, because what it draws is the only
  copy of those vertices in the file.
* §25.4 asks the culler a **per-category instance list**, not a per-cell chunk list. The whole
  neighbourhood is one category and every one of its members is at a different distance.

So this file holds **meshes in their own space**, keyed by asset id, and nothing else. Where each
one stands is already in `layout.exterior.json`'s `neighbourhood` rows, which `WorldLoader` reads
into `ExteriorContents::neighbourhood`; writing the positions here too would be a second answer to
where a house is, and two answers disagree eventually.

    tools/world/build_neighbourhood.py                 # -> content/world/neighbourhood.bin
    tools/world/build_neighbourhood.py --report
    tools/world/build_neighbourhood.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import build_chunks as chunks  # noqa: E402
import build_collision as bc  # noqa: E402
import layout_io  # noqa: E402
import neighbourhood_gen as neighbourhood  # noqa: E402
from layout_io import LayoutError  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "assets-src" / "world"
MESHES = REPO / "build" / "neighbourhood"
OUTPUT = REPO / "content" / "world" / "neighbourhood.bin"

MAGIC = b"CNBH"
VERSION = 1

#: `docs/chunk-format.md` §3's layout 0: `Position` `Normal` `TexCoord0`, 32 bytes, `BasicEffect`.
#: The neighbourhood is always this one and the writer PROVES it rather than assuming it -- §18.3
#: bakes rooms, and a neighbour has no inside to bake, so `neighbourhood_gen.py` writes
#: `lightmapReceiver: false` on every material and a `true` here is a contradiction worth failing
#: on. Sharing the id with the chunk format is deliberate: the runtime already has a vertex
#: declaration for it.
LAYOUT_BASIC = 0
VERTEX_STRIDE = 32


def wanted(directory: Path) -> list[str]:
    """Every neighbourhood asset the layout names, in id order -- the generator's own answer."""
    return neighbourhood.wanted_assets(directory)


def read_asset(path: Path) -> list[dict]:
    """One asset's `.glb` as primitives: `[{material, positions, normals, uv0, triangles}]`.

    `build_chunks.read_shell_geometry` splits by MATERIAL, which is what a primitive is here, and
    applies the node transform -- identity in these files, so the vertices stay in the asset's own
    space, which is the whole point of this format.
    """
    out = []
    for material, mesh in sorted(chunks.read_shell_geometry(path).items()):
        check_not_a_receiver(path.name, material, mesh.get("extras") or {})
        if not mesh["triangles"]:
            continue
        out.append({"material": material, "positions": mesh["positions"],
                    "normals": mesh["normals"], "uv0": mesh["uv0"],
                    "triangles": mesh["triangles"]})
    if not out:
        raise LayoutError(f"{path.name}: no geometry, so nothing would be drawn for it")
    return out


def check_not_a_receiver(where: str, material: str, extras: dict) -> None:
    """Refuse a material that claims §18.3's lightmap, which decides the vertex layout.

    A separate function because it is the one rule in this file that no real input exercises: every
    material `neighbourhood_gen.py` emits says `lightmapReceiver: false`, so a guard written inline
    would be a guard nothing ever ran. The selftest calls this directly.
    """
    if extras.get("lightmapReceiver"):
        raise LayoutError(
            f"{where}: material {material!r} says it is a lightmap receiver, and §18.3 bakes "
            f"ROOMS -- a neighbour has no inside to bake, so there is no second UV for "
            f"`DualTextureEffect` to read")


def _bounds(positions) -> tuple:
    return (min(p[0] for p in positions), min(p[1] for p in positions),
            min(p[2] for p in positions), max(p[0] for p in positions),
            max(p[1] for p in positions), max(p[2] for p in positions))


def _union(boxes) -> tuple:
    return (min(b[0] for b in boxes), min(b[1] for b in boxes), min(b[2] for b in boxes),
            max(b[3] for b in boxes), max(b[4] for b in boxes), max(b[5] for b in boxes))


def build(directory: Path, meshes: Path) -> dict:
    """`{worldHash, assets: [...]}` -- every asset the layout names, read from @p meshes."""
    assets = []
    for name in wanted(directory):
        path = meshes / f"{name}.glb"
        if not path.is_file():
            raise LayoutError(
                f"{path} is missing: the layout names {name!r} and nothing has drawn it. Run "
                f"tools/world/neighbourhood_gen.py")
        primitives = []
        for primitive in read_asset(path):
            primitives.append({
                "material": primitive["material"],
                "layout": LAYOUT_BASIC,
                "bounds": _bounds(primitive["positions"]),
                "vertices": list(zip(primitive["positions"], primitive["normals"],
                                     primitive["uv0"])),
                "indices": [index for triangle in primitive["triangles"] for index in triangle],
            })
        assets.append({"asset": name, "bounds": _union([p["bounds"] for p in primitives]),
                       "primitives": primitives})
    # `build_collision`'s own answer, not a second one: an empty hash is legal and means the
    # runtime's staleness check cannot run, which is different from the file being wrong.
    return {"worldHash": bc._world_hash(directory), "assets": assets}


def serialise(built: dict) -> bytes:
    """The bytes of `neighbourhood.bin`, exactly as `docs/neighbourhood-format.md` describes."""
    materials = sorted({primitive["material"] for asset in built["assets"]
                        for primitive in asset["primitives"]})
    material_index = {name: index for index, name in enumerate(materials)}

    out = bytearray()
    out += MAGIC
    out += struct.pack("<II", VERSION, 0)
    out += bc._string(built["worldHash"])
    out += struct.pack("<I", len(materials))
    for name in materials:
        out += bc._string(name)

    out += struct.pack("<I", len(built["assets"]))
    for asset in built["assets"]:
        out += bc._string(asset["asset"])
        out += struct.pack("<6f", *asset["bounds"])
        out += struct.pack("<I", len(asset["primitives"]))
        for primitive in asset["primitives"]:
            wide = len(primitive["vertices"]) > 0xFFFF
            out += struct.pack("<H", material_index[primitive["material"]])
            out += struct.pack("<BB", primitive["layout"], 1 if wide else 0)
            out += struct.pack("<6f", *primitive["bounds"])
            out += struct.pack("<I", len(primitive["vertices"]))
            for position, normal, uv0 in primitive["vertices"]:
                out += struct.pack("<3f", *position)
                out += struct.pack("<3f", *normal)
                out += struct.pack("<2f", *uv0[:2])
            out += struct.pack("<I", len(primitive["indices"]))
            code = "<I" if wide else "<H"
            for index in primitive["indices"]:
                out += struct.pack(code, index)
    return bytes(out)


def read_back(data: bytes) -> dict:
    """Parse the bytes again, so the writer is checked by something that is not the writer."""
    view = memoryview(data)
    at = 0

    def take(size: int) -> memoryview:
        nonlocal at
        if at + size > len(view):
            raise LayoutError(f"neighbourhood.bin: truncated at byte {at}")
        piece = view[at:at + size]
        at += size
        return piece

    def u32() -> int:
        return struct.unpack("<I", take(4))[0]

    def text() -> str:
        length = struct.unpack("<H", take(2))[0]
        return bytes(take(length)).decode("utf-8")

    if bytes(take(4)) != MAGIC:
        raise LayoutError("neighbourhood.bin: wrong magic")
    version, flags = struct.unpack("<II", take(8))
    if version != VERSION:
        raise LayoutError(f"neighbourhood.bin: version {version}, expected {VERSION}")
    if flags:
        raise LayoutError(f"neighbourhood.bin: unknown flags {flags:#x}")
    world_hash = text()
    materials = [text() for _ in range(u32())]
    assets = []
    for _ in range(u32()):
        name = text()
        bounds = struct.unpack("<6f", take(24))
        primitives = []
        for _ in range(u32()):
            material = struct.unpack("<H", take(2))[0]
            layout, wide = struct.unpack("<BB", take(2))
            primitive_bounds = struct.unpack("<6f", take(24))
            count = u32()
            vertices = []
            for _ in range(count):
                raw = struct.unpack("<8f", take(VERTEX_STRIDE))
                vertices.append((raw[0:3], raw[3:6], raw[6:8]))
            index_count = u32()
            code = "I" if wide else "H"
            size = 4 if wide else 2
            indices = list(struct.unpack(f"<{index_count}{code}", take(index_count * size)))
            primitives.append({"material": materials[material], "layout": layout,
                               "bounds": primitive_bounds, "vertices": vertices,
                               "indices": indices})
        assets.append({"asset": name, "bounds": bounds, "primitives": primitives})
    if at != len(view):
        raise LayoutError(f"neighbourhood.bin: {len(view) - at} trailing byte(s)")
    return {"worldHash": world_hash, "materials": materials, "assets": assets}


def report(built: dict) -> str:
    lines = [f"{len(built['assets'])} asset(s)"]
    for asset in built["assets"]:
        vertices = sum(len(p["vertices"]) for p in asset["primitives"])
        triangles = sum(len(p["indices"]) // 3 for p in asset["primitives"])
        lines.append(f"  {asset['asset']:30s} {len(asset['primitives'])} primitive(s), "
                     f"{vertices:5d} vertex(es), {triangles:5d} triangle(s)")
    return "\n".join(lines)


def fixture_library() -> dict:
    """A tiny library whose every number is stated here and asserted in C++.

    `HOUSE-00856`, and the same idiom as `build_chunks.fixture_library` for the same reason:
    `NeighbourhoodReaderTests` builds its bytes by hand, which makes it an excellent test of the
    reader and no test at all of the reader and the WRITER agreeing. A writer that emitted the box
    max before its min, or the index count before the vertices, would pass every test in this
    repository and fail in the game.

    Everything here is deliberately asymmetric: two assets so the ascending order can be seen, two
    primitives in one of them so the material index matters, distinct values in every float, and a
    normal that is not +Y so a reader that skipped it would be caught.
    """
    return {
        "worldHash": "sha256:" + "5c" * 32,
        "assets": [
            {
                "asset": "MODEL_NB_HOUSE_A_CREAM",
                "bounds": (-5.5, 0.0, -4.25, 5.5, 8.125, 4.25),
                "primitives": [
                    {
                        "material": "NB_ROOF_GREY", "layout": LAYOUT_BASIC,
                        "bounds": (-5.5, 6.0, -4.25, 5.5, 8.125, 4.25),
                        "vertices": [((-5.5, 6.0, -4.25), (0.0, 0.5, -0.5), (0.125, 0.25)),
                                     ((5.5, 6.0, -4.25), (0.0, 0.5, -0.5), (0.375, 0.5)),
                                     ((0.0, 8.125, 4.25), (0.0, 0.5, 0.5), (0.625, 0.75))],
                        "indices": [0, 1, 2],
                    },
                    {
                        "material": "NB_WALL_CREAM", "layout": LAYOUT_BASIC,
                        "bounds": (-5.5, 0.0, -4.25, 5.5, 6.0, -4.25),
                        "vertices": [((-5.5, 0.0, -4.25), (0.0, 0.0, -1.0), (0.0, 0.0)),
                                     ((5.5, 0.0, -4.25), (0.0, 0.0, -1.0), (1.0, 0.0)),
                                     ((5.5, 6.0, -4.25), (0.0, 0.0, -1.0), (1.0, 0.875)),
                                     ((-5.5, 6.0, -4.25), (0.0, 0.0, -1.0), (0.0, 0.875))],
                        "indices": [0, 1, 2, 0, 2, 3],
                    },
                ],
            },
            {
                "asset": "MODEL_NB_IMPOSTOR_GABLE",
                "bounds": (-5.5, 0.0, 0.0, 5.5, 8.5, 0.0),
                "primitives": [{
                    "material": "NB_IMPOSTOR", "layout": LAYOUT_BASIC,
                    "bounds": (-5.5, 0.0, 0.0, 5.5, 8.5, 0.0),
                    "vertices": [((-5.5, 5.1, 0.0), (0.0, 0.0, 1.0), (0.0, 0.6)),
                                 ((5.5, 5.1, 0.0), (0.0, 0.0, 1.0), (1.0, 0.6)),
                                 ((0.0, 8.5, 0.0), (0.0, 0.0, 1.0), (0.5, 1.0))],
                    "indices": [0, 1, 2],
                }],
            },
        ],
    }


# ======================================================================================= selftest


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("build_neighbourhood: selftest")

    # 1. A hand-built library round-trips, field for field. This is the claim that can see the
    #    writer and the reader disagreeing about ORDER, which no single-sided test can.
    fixture = {
        "worldHash": "sha256:" + "ab" * 32,
        "assets": [{
            "asset": "MODEL_NB_HOUSE_A_CREAM",
            "bounds": (-1.0, 0.0, -2.0, 3.0, 4.0, 5.0),
            "primitives": [{
                "material": "NB_WALL_CREAM", "layout": LAYOUT_BASIC,
                "bounds": (-1.0, 0.0, -2.0, 3.0, 4.0, 5.0),
                "vertices": [((-1.0, 0.0, -2.0), (0.0, 1.0, 0.0), (0.25, 0.5)),
                             ((3.0, 0.0, -2.0), (0.0, 1.0, 0.0), (0.75, 0.5)),
                             ((3.0, 4.0, 5.0), (0.0, 1.0, 0.0), (0.75, 1.0))],
                "indices": [0, 1, 2],
            }],
        }],
    }
    data = serialise(fixture)
    back = read_back(data)
    require(back["worldHash"] == fixture["worldHash"], "the world hash survives the round trip")
    require([a["asset"] for a in back["assets"]] == ["MODEL_NB_HOUSE_A_CREAM"],
            "and the asset's id, which is what a row resolves through")
    require(back["materials"] == ["NB_WALL_CREAM"],
            f"and the material table ({back['materials']})")
    same = back["assets"][0]["primitives"][0]
    require(same["indices"] == [0, 1, 2], f"and the indices ({same['indices']})")
    require(all(abs(a - b) < 1e-6 for pair in zip(same["vertices"], fixture["assets"][0]
                                                  ["primitives"][0]["vertices"])
                for va, vb in zip(*pair) for a, b in zip(va, vb)),
            "and every vertex, position, normal and UV in that order")
    expected = (4 + 8                                        # magic, version, flags
                + (2 + 71) + 4 + (2 + 13)                    # world hash, material table
                + 4 + (2 + 22) + 24 + 4                      # asset table, name, bounds, count
                + (2 + 2 + 24 + 4 + 3 * VERTEX_STRIDE + 4 + 3 * 2))
    require(len(data) == expected,
            f"and the file is exactly the size the format says ({len(data)} of {expected} bytes)")
    require(back["assets"][0]["bounds"] == fixture["assets"][0]["bounds"],
            f"and the box, min xyz then max xyz in that order ({back['assets'][0]['bounds']})")
    require(back["assets"][0]["primitives"][0]["bounds"]
            == fixture["assets"][0]["primitives"][0]["bounds"],
            "and a primitive's own box, which is not the asset's")

    # ...and the fixture the C++ round trip reads, whose whole job is to be asymmetric.
    library = fixture_library()
    order = [asset["asset"] for asset in library["assets"]]
    require(order == sorted(order),
            f"the fixture's assets are in ascending id order, which `Find` binary-searches ({order})")
    require(len({p["material"] for a in library["assets"] for p in a["primitives"]}) == 3,
            "and it holds three materials, so a u16 index that was ignored would show")
    fixture_back = read_back(serialise(library))
    require([a["asset"] for a in fixture_back["assets"]] == order,
            "and it round-trips through this tool before C++ ever opens it")

    # A neighbour is never a lightmap receiver: §18.3 bakes ROOMS, and the layout this format
    # writes has no second UV channel for `DualTextureEffect` to read.
    try:
        check_not_a_receiver("MODEL_NB_HOUSE_A_CREAM.glb", "NB_WALL_CREAM",
                             {"lightmapReceiver": True})
        caught = False
    except LayoutError:
        caught = True
    require(caught, "a material claiming §18.3's lightmap is refused, not written as layout 0")
    check_not_a_receiver("x.glb", "NB_WALL_CREAM", {"lightmapReceiver": False})
    require(True, "...and one that does not claim it is accepted")

    # 2. Every way the bytes can be wrong is refused, naming what was wrong with them.
    for name, broken in (("wrong magic", b"XXXX" + data[4:]),
                         ("a version this reader does not know",
                          data[:4] + struct.pack("<I", 99) + data[8:]),
                         ("an unknown flag bit",
                          data[:8] + struct.pack("<I", 1) + data[12:]),
                         ("a truncated file", data[:-4]),
                         ("trailing bytes", data + b"\0\0\0\0")):
        try:
            read_back(broken)
            caught = False
        except LayoutError:
            caught = True
        require(caught, f"{name} is refused, not read as geometry")

    # 3. The vertex is the chunk format's, so the runtime declares it once.
    require(VERTEX_STRIDE == 32 and LAYOUT_BASIC == 0,
            "the vertex is `docs/chunk-format.md` §3's layout 0, so the runtime's existing "
            "declaration reads it")

    # 4. Against the real tree, when there is one.
    if (SOURCE / "layout.exterior.json").is_file() and MESHES.is_dir():
        names = wanted(SOURCE)
        require(bool(names), f"the layout names {len(names)} neighbourhood asset(s)")
        missing = [name for name in names if not (MESHES / f"{name}.glb").is_file()]
        require(not missing,
                f"and `neighbourhood_gen.py` has drawn every one of them ({missing})")
        if not missing:
            built = build(SOURCE, MESHES)
            data = serialise(built)
            back = read_back(data)
            require([a["asset"] for a in back["assets"]] == names,
                    "the file holds exactly what the layout asks for, in id order")
            require(all(p["layout"] == LAYOUT_BASIC for a in back["assets"]
                        for p in a["primitives"]),
                    "and all of it is one vertex layout, so it is one vertex declaration")
            # The bounds are the asset's OWN, in its own space, which is what makes an instance
            # possible: a house at the origin and the same house 90 m away are one mesh.
            local = [a for a in back["assets"]
                     if not (-60.0 < a["bounds"][0] and a["bounds"][3] < 60.0)]
            require(not local,
                    f"every mesh is in its own space and not the world's -- nothing reaches past "
                    f"60 m from its own origin ({[a['asset'] for a in local]})")
            grounded = [a["asset"] for a in back["assets"] if a["bounds"][1] < -0.51]
            require(not grounded,
                    f"and stands on its own ground plane rather than under it ({grounded})")
            rows = (layout_io.load_layout(SOURCE, kinds=["exterior"]).get("exterior")
                    or {}).get("neighbourhood", [])
            held = {a["asset"] for a in back["assets"]}
            unresolved = sorted({str(row["asset"]) for row in rows
                                 if neighbourhood.is_ours(str(row["asset"]))} - held)
            require(not unresolved,
                    f"and every row this grammar owns resolves to a mesh in it ({unresolved})")
            # ...and the ones it does NOT own are somebody else's to deliver, which is a
            # different statement from "the file is complete".
            theirs = sorted({str(row["asset"]) for row in rows
                             if not neighbourhood.is_ours(str(row["asset"]))})
            require(theirs == ["MODEL_DELIVERY_VAN", "MODEL_PARKED_CAR"],
                    f"§11.4's vehicles are still `HOUSE-00847`'s and are not in here ({theirs})")

    if failures:
        print(f"\nbuild_neighbourhood: {len(failures)} claim(s) FAILED")
        return 1
    print("build_neighbourhood: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--directory", type=Path, default=SOURCE)
    parser.add_argument("--meshes", type=Path, default=MESHES)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--fixture", type=Path, default=None,
                        help="write the C++ round-trip fixture (HOUSE-00856) and exit")
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    if args.fixture is not None:
        data = serialise(fixture_library())
        read_back(data)
        args.fixture.parent.mkdir(parents=True, exist_ok=True)
        args.fixture.write_bytes(data)
        print(f"build_neighbourhood: wrote the round-trip fixture to {args.fixture} "
              f"({len(data)} bytes)")
        return 0

    if not (args.directory / "layout.exterior.json").is_file():
        print(f"build_neighbourhood: no exterior layout in {args.directory}", file=sys.stderr)
        return 1
    if not args.meshes.is_dir():
        print(f"build_neighbourhood: {args.meshes} does not exist; run "
              f"tools/world/neighbourhood_gen.py", file=sys.stderr)
        return 1

    built = build(args.directory, args.meshes)
    data = serialise(built)
    read_back(data)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    if args.report:
        print(report(built))
    print(f"build_neighbourhood: wrote {args.output} ({len(data)} bytes, "
          f"{len(built['assets'])} asset(s))")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
