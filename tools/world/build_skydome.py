#!/usr/bin/env python3
"""build_skydome.py -- generate §31.1's 32 x 18 sky hemisphere and horizon skirt.

`HOUSE-01642`.  The file contains positions and indices only.  `SkySystem` creates
`VertexPositionColor` vertices from them because §31.2 changes colour with the weather and sun;
putting a baked colour here would be a second, immediately stale sky model.

    tools/world/build_skydome.py --out content/world/sky_dome.bin
    tools/world/build_skydome.py --check content/world/sky_dome.bin
    tools/world/build_skydome.py --report
    tools/world/build_skydome.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_OUT = REPO / "content" / "world" / "sky_dome.bin"

MAGIC = b"CSKY"
VERSION = 1
FLAGS = 0
LONGITUDE_SEGMENTS = 32
LATITUDE_SEGMENTS = 18
RADIUS = 900.0
SKIRT_DEPTH = 90.0
EPSILON = 1e-6


def build() -> dict:
    """Return the indexed mesh, without seam duplicates or a duplicated pole."""
    vertices: list[tuple[float, float, float]] = [(0.0, RADIUS, 0.0)]

    # Eighteen rings follow the pole: 5, 10, ... 90 degrees down from it.  The last is the
    # horizon.  Each ring wraps by index, so no vertex at 360 duplicates the one at 0.
    for latitude_index in range(1, LATITUDE_SEGMENTS + 1):
        down = latitude_index * (math.pi * 0.5 / LATITUDE_SEGMENTS)
        horizontal = RADIUS * math.sin(down)
        y = RADIUS * math.cos(down)
        for longitude_index in range(LONGITUDE_SEGMENTS):
            azimuth = longitude_index * (2.0 * math.pi / LONGITUDE_SEGMENTS)
            vertices.append((horizontal * math.sin(azimuth), y,
                             -horizontal * math.cos(azimuth)))

    indices: list[int] = []
    first_ring = 1
    for longitude_index in range(LONGITUDE_SEGMENTS):
        following = (longitude_index + 1) % LONGITUDE_SEGMENTS
        indices.extend((0, first_ring + following, first_ring + longitude_index))

    for ring_index in range(LATITUDE_SEGMENTS - 1):
        upper = first_ring + ring_index * LONGITUDE_SEGMENTS
        lower = upper + LONGITUDE_SEGMENTS
        for longitude_index in range(LONGITUDE_SEGMENTS):
            following = (longitude_index + 1) % LONGITUDE_SEGMENTS
            indices.extend((upper + longitude_index, upper + following,
                            lower + longitude_index))
            indices.extend((upper + following, lower + following,
                            lower + longitude_index))

    dome_vertex_count = len(vertices)
    horizon = first_ring + (LATITUDE_SEGMENTS - 1) * LONGITUDE_SEGMENTS
    skirt = len(vertices)
    for longitude_index in range(LONGITUDE_SEGMENTS):
        azimuth = longitude_index * (2.0 * math.pi / LONGITUDE_SEGMENTS)
        vertices.append((RADIUS * math.sin(azimuth), -SKIRT_DEPTH,
                         -RADIUS * math.cos(azimuth)))

    # The sloping wall hides a horizon crack.  The disc behind it fills everything below that
    # horizon; later terrain and house draws overwrite it because the sky is drawn first.
    for longitude_index in range(LONGITUDE_SEGMENTS):
        following = (longitude_index + 1) % LONGITUDE_SEGMENTS
        indices.extend((horizon + longitude_index, horizon + following,
                        skirt + longitude_index))
        indices.extend((horizon + following, skirt + following,
                        skirt + longitude_index))

    centre = len(vertices)
    vertices.append((0.0, -SKIRT_DEPTH, 0.0))
    for longitude_index in range(LONGITUDE_SEGMENTS):
        following = (longitude_index + 1) % LONGITUDE_SEGMENTS
        indices.extend((centre, skirt + longitude_index, skirt + following))

    return {
        "longitudeSegments": LONGITUDE_SEGMENTS,
        "latitudeSegments": LATITUDE_SEGMENTS,
        "radius": RADIUS,
        "skirtDepth": SKIRT_DEPTH,
        "domeVertexCount": dome_vertex_count,
        "vertices": vertices,
        "indices": indices,
    }


def encode(mesh: dict) -> bytes:
    vertices = mesh["vertices"]
    indices = mesh["indices"]
    if len(vertices) > 0xFFFF:
        raise ValueError("sky dome no longer fits u16 indices")
    out = bytearray()
    out += struct.pack(
        "<4sIIIffIII",
        MAGIC,
        VERSION,
        FLAGS,
        mesh["longitudeSegments"],
        mesh["radius"],
        mesh["skirtDepth"],
        mesh["latitudeSegments"],
        mesh["domeVertexCount"],
        len(vertices),
    )
    out += struct.pack("<I", len(indices))
    for vertex in vertices:
        out += struct.pack("<3f", *vertex)
    for index in indices:
        out += struct.pack("<H", index)
    return bytes(out)


def decode(data: bytes) -> dict:
    header = struct.Struct("<4sIIIffIIII")
    if len(data) < header.size:
        raise ValueError("sky_dome.bin is truncated before its header")
    (magic, version, flags, longitude_segments, radius, skirt_depth, latitude_segments,
     dome_vertex_count, vertex_count, index_count) = header.unpack_from(data)
    if magic != MAGIC:
        raise ValueError("not a sky_dome.bin: bad magic")
    if version != VERSION:
        raise ValueError(f"sky_dome.bin is version {version}; this reader knows {VERSION}")
    if flags != FLAGS:
        raise ValueError(f"sky_dome.bin sets unknown flag bits {flags:#x}")
    wanted = header.size + vertex_count * 12 + index_count * 2
    if len(data) != wanted:
        detail = "truncated" if len(data) < wanted else "has trailing bytes"
        raise ValueError(f"sky_dome.bin {detail}: {len(data)} bytes, expected {wanted}")
    offset = header.size
    vertices = []
    for _ in range(vertex_count):
        vertices.append(struct.unpack_from("<3f", data, offset))
        offset += 12
    indices = []
    for _ in range(index_count):
        indices.append(struct.unpack_from("<H", data, offset)[0])
        offset += 2
    return {
        "longitudeSegments": longitude_segments,
        "latitudeSegments": latitude_segments,
        "radius": radius,
        "skirtDepth": skirt_depth,
        "domeVertexCount": dome_vertex_count,
        "vertices": vertices,
        "indices": indices,
    }


def triangle_area(a, b, c) -> float:
    ab = tuple(b[i] - a[i] for i in range(3))
    ac = tuple(c[i] - a[i] for i in range(3))
    cross = (ab[1] * ac[2] - ab[2] * ac[1],
             ab[2] * ac[0] - ab[0] * ac[2],
             ab[0] * ac[1] - ab[1] * ac[0])
    return 0.5 * math.sqrt(sum(value * value for value in cross))


def claims(mesh: dict) -> list[tuple[bool, str]]:
    vertices = mesh["vertices"]
    indices = mesh["indices"]
    dome_count = mesh["domeVertexCount"]
    triangles = [indices[at:at + 3] for at in range(0, len(indices), 3)]
    dome_vertices = vertices[:dome_count]
    horizon = [vertex for vertex in dome_vertices if abs(vertex[1]) < EPSILON]
    skirt_bottom = [vertex for vertex in vertices[dome_count:] if abs(vertex[1] + SKIRT_DEPTH) < EPSILON]
    areas = [triangle_area(*(vertices[index] for index in triangle)) for triangle in triangles]
    radial_errors = [abs(math.sqrt(sum(value * value for value in vertex)) - RADIUS)
                     for vertex in dome_vertices]
    return [
        (mesh["longitudeSegments"] == 32 and mesh["latitudeSegments"] == 18,
         "the hemisphere is exactly 32 x 18 segments"),
        (dome_count == 577 and len(vertices) == 610,
         f"one pole plus 18 rings and the skirt use 577 + 33 = {len(vertices)} vertices"),
        (len(indices) == 3648 and len(triangles) == 1216,
         f"the mesh is {len(triangles)} triangles / {len(indices)} indices"),
        (len(horizon) == 32 and len(skirt_bottom) == 33,
         f"the horizon has {len(horizon)} vertices and its closed skirt/disc has 32 + 1"),
        (max(radial_errors) < 1e-3,
         f"every dome vertex is on the {RADIUS:.0f} m sphere (worst error "
         f"{max(radial_errors):.6f} m)"),
        (min(vertex[1] for vertex in vertices) == -SKIRT_DEPTH
         and max(vertex[1] for vertex in vertices) == RADIUS,
         f"the bounds include the pole and the {-SKIRT_DEPTH:.0f} m ground skirt"),
        (all(0 <= index < len(vertices) for index in indices),
         "every u16 index addresses a vertex"),
        (min(areas) > EPSILON,
         f"there are no degenerate triangles (smallest area {min(areas):.3f} m^2)"),
        (len(set(vertices)) == len(vertices),
         "there are no duplicated pole or seam vertices"),
        (encode(decode(encode(mesh))) == encode(mesh),
         "the binary round-trips byte-for-byte through binary32 values"),
        (encode(build()) == encode(build()),
         "generation is byte-deterministic"),
    ]


def report(mesh: dict) -> None:
    print(f"sky_dome: {len(mesh['vertices'])} vertices, {len(mesh['indices']) // 3} triangles, "
          f"{len(encode(mesh))} bytes; {mesh['longitudeSegments']} x "
          f"{mesh['latitudeSegments']}, radius {mesh['radius']:.0f} m, "
          f"skirt {mesh['skirtDepth']:.0f} m")


def selftest() -> int:
    print("build_skydome: selftest")
    failures = 0
    for condition, message in claims(build()):
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        failures += 0 if condition else 1

    good = encode(build())
    corruptions = [
        (b"NOPE" + good[4:], "bad magic"),
        (good[:4] + struct.pack("<I", VERSION + 1) + good[8:], "unknown version"),
        (good[:-1], "truncation"),
        (good + b"x", "trailing bytes"),
    ]
    rejected = 0
    for data, _ in corruptions:
        try:
            decode(data)
        except ValueError:
            rejected += 1
    condition = rejected == len(corruptions)
    print(f"  {'ok  ' if condition else 'FAIL'}  malformed binaries are rejected "
          f"({rejected}/{len(corruptions)})")
    failures += 0 if condition else 1
    print("build_skydome: selftest passed." if not failures
          else f"build_skydome: {failures} claim(s) FAILED")
    return 1 if failures else 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", type=Path)
    parser.add_argument("--check", type=Path)
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)
    selected = sum((args.out is not None, args.check is not None, args.report, args.selftest))
    if selected != 1:
        parser.error("choose exactly one of --out, --check, --report or --selftest")

    if args.selftest:
        return selftest()
    mesh = build()
    if args.report:
        report(mesh)
        return 0
    wanted = encode(mesh)
    if args.check is not None:
        if not args.check.is_file():
            print(f"build_skydome: {args.check} is missing")
            return 1
        if args.check.read_bytes() != wanted:
            print(f"build_skydome: {args.check} is stale; regenerate it")
            return 1
        report(mesh)
        return 0
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(wanted)
    report(mesh)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
