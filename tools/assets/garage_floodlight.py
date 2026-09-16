#!/usr/bin/env python3
"""Author the measured LED wall pack above the canonical garage door.

The fixture is project-authored and deterministic: a dark-bronze cast housing, shallow visor,
cooling ribs and one neutral frosted lens. Its asset origin is the lowest support point with X/Z
centred. ``FloodLens`` is the exact emissive slot linked from ``layout.lights.json``; keeping it
separate from ``FloodMetal`` lets the existing stock-XNA fixture path switch only the diffuser.

No network input or third-party model is involved.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import tempfile
from pathlib import Path

import gltf_io

REPO = Path(__file__).resolve().parents[2]
TARGET = REPO / "assets-src" / "Models" / "Fixtures" / "garage_floodlight.glb"
MATERIALS = ("FloodMetal", "FloodLens")
DEPTH_CENTRE_Z = 0.0975


class Mesh:
    def __init__(self) -> None:
        self.positions: list[list[tuple[float, float, float]]] = [[], []]
        self.normals: list[list[tuple[float, float, float]]] = [[], []]
        self.uvs: list[list[tuple[float, float]]] = [[], []]
        self.indices: list[list[int]] = [[], []]

    def quad(self, material: int, corners: tuple[tuple[float, float, float], ...],
             normal: tuple[float, float, float]) -> None:
        base = len(self.positions[material])
        self.positions[material].extend(corners)
        self.normals[material].extend([normal] * 4)
        self.uvs[material].extend(((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)))
        self.indices[material].extend((base, base + 1, base + 2, base, base + 2, base + 3))

    def box(self, material: int, centre: tuple[float, float, float],
            size: tuple[float, float, float]) -> None:
        cx, cy, cz = centre
        hx, hy, hz = (value * 0.5 for value in size)
        lo = (cx - hx, cy - hy, cz - hz)
        hi = (cx + hx, cy + hy, cz + hz)
        faces = (
            ((0.0, 0.0, 1.0), ((lo[0], lo[1], hi[2]), (hi[0], lo[1], hi[2]),
                                (hi[0], hi[1], hi[2]), (lo[0], hi[1], hi[2]))),
            ((0.0, 0.0, -1.0), ((hi[0], lo[1], lo[2]), (lo[0], lo[1], lo[2]),
                                 (lo[0], hi[1], lo[2]), (hi[0], hi[1], lo[2]))),
            ((1.0, 0.0, 0.0), ((hi[0], lo[1], hi[2]), (hi[0], lo[1], lo[2]),
                                (hi[0], hi[1], lo[2]), (hi[0], hi[1], hi[2]))),
            ((-1.0, 0.0, 0.0), ((lo[0], lo[1], lo[2]), (lo[0], lo[1], hi[2]),
                                 (lo[0], hi[1], hi[2]), (lo[0], hi[1], lo[2]))),
            ((0.0, 1.0, 0.0), ((lo[0], hi[1], hi[2]), (hi[0], hi[1], hi[2]),
                                (hi[0], hi[1], lo[2]), (lo[0], hi[1], lo[2]))),
            ((0.0, -1.0, 0.0), ((lo[0], lo[1], lo[2]), (hi[0], lo[1], lo[2]),
                                 (hi[0], lo[1], hi[2]), (lo[0], lo[1], hi[2]))),
        )
        for normal, corners in faces:
            self.quad(material, corners, normal)


def author(path: Path) -> tuple[int, list[float], str]:
    mesh = Mesh()

    # A wall pack rather than a floating luminous rectangle: backplate, cast body, visor, side
    # cheeks, lower drip lip and five shallow heat-sink ribs remain legible in the driveway view.
    mesh.box(0, (0.0, 0.105, 0.018), (0.420, 0.180, 0.036))
    mesh.box(0, (0.0, 0.105, 0.090), (0.340, 0.140, 0.130))
    mesh.box(0, (0.0, 0.190, 0.100), (0.380, 0.025, 0.190))
    mesh.box(0, (-0.177, 0.105, 0.105), (0.026, 0.145, 0.160))
    mesh.box(0, (0.177, 0.105, 0.105), (0.026, 0.145, 0.160))
    mesh.box(0, (0.0, 0.023, 0.105), (0.360, 0.026, 0.165))
    for x in (-0.120, -0.060, 0.0, 0.060, 0.120):
        mesh.box(0, (x, 0.142, 0.045), (0.022, 0.072, 0.050))
    # A small drain boss establishes the repository's bottom-centred support origin at Y=0.
    mesh.box(0, (0.0, 0.008, 0.020), (0.030, 0.016, 0.030))

    # Frosted LED lens. It is a shallow physical insert rather than a single camera-facing quad,
    # so the off-state still has a credible edge and the material works from oblique approaches.
    mesh.box(1, (0.0, 0.100, 0.159), (0.292, 0.082, 0.008))

    # Authoring coordinates above are measured outward from the mounting plane. Asset convention
    # still requires a depth-centred pivot; the canonical prop compensates this translation so the
    # plate, lens and authored optical point retain exactly the same world positions.
    mesh.positions = [[(x, y, z - DEPTH_CENTRE_Z) for x, y, z in group]
                      for group in mesh.positions]

    blob = bytearray()
    views: list[dict] = []
    accessors: list[dict] = []

    def add(values: list, fmt: str, component: int, kind: str, target: int) -> int:
        offset = len(blob)
        raw = b"".join(struct.pack("<" + fmt, *value) if isinstance(value, tuple)
                       else struct.pack("<" + fmt, value) for value in values)
        blob.extend(raw)
        blob.extend(b"\0" * ((4 - len(raw) % 4) % 4))
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(raw), "target": target})
        accessor = {"bufferView": len(views) - 1, "componentType": component,
                    "count": len(values), "type": kind}
        width = len(values[0]) if isinstance(values[0], tuple) else 1
        accessor["min"] = ([min(value[index] for value in values) for index in range(width)]
                           if width > 1 else [min(values)])
        accessor["max"] = ([max(value[index] for value in values) for index in range(width)]
                           if width > 1 else [max(values)])
        accessors.append(accessor)
        return len(accessors) - 1

    primitives = []
    triangle_count = 0
    for material in range(len(MATERIALS)):
        position = add(mesh.positions[material], "3f", 5126, "VEC3", 34962)
        normal = add(mesh.normals[material], "3f", 5126, "VEC3", 34962)
        uv = add(mesh.uvs[material], "2f", 5126, "VEC2", 34962)
        indices = add(mesh.indices[material], "H", 5123, "SCALAR", 34963)
        triangle_count += len(mesh.indices[material]) // 3
        primitives.append({"attributes": {"POSITION": position, "NORMAL": normal,
                                           "TEXCOORD_0": uv},
                           "indices": indices, "mode": 4, "material": material})

    all_positions = [position for group in mesh.positions for position in group]
    lows = [min(value[index] for value in all_positions) for index in range(3)]
    highs = [max(value[index] for value in all_positions) for index in range(3)]
    bounds = [round(highs[index] - lows[index], 6) for index in range(3)]
    document = {
        "asset": {"version": "2.0", "generator": "cna-house tools/assets/garage_floodlight.py"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "GarageFloodlight", "mesh": 0}],
        "meshes": [{"name": "GarageFloodlight", "primitives": primitives}],
        "materials": [
            {"name": "FloodMetal", "doubleSided": False,
             "pbrMetallicRoughness": {"baseColorFactor": [0.12, 0.09, 0.065, 1.0],
                                      "metallicFactor": 0.72, "roughnessFactor": 0.36}},
            {"name": "FloodLens", "doubleSided": False,
             "pbrMetallicRoughness": {"baseColorFactor": [0.92, 0.94, 0.92, 1.0],
                                      "metallicFactor": 0.0, "roughnessFactor": 0.82}},
        ],
        "accessors": accessors, "bufferViews": views, "buffers": [{"byteLength": len(blob)}],
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    data = gltf_io.write_glb(path, document, bytes(blob))
    return triangle_count, bounds, hashlib.sha256(data).hexdigest()


def validate(path: Path) -> tuple[int, list[float], str]:
    document, _ = gltf_io.read_model(path)
    names = tuple(material["name"] for material in document["materials"])
    if names != MATERIALS:
        raise RuntimeError(f"garage floodlight material contract changed: {names}")
    primitives = document["meshes"][0]["primitives"]
    if len(primitives) != 2 or any(set(primitive["attributes"]) !=
                                   {"POSITION", "NORMAL", "TEXCOORD_0"}
                                   for primitive in primitives):
        raise RuntimeError("garage floodlight must retain two complete visible material slots")
    positions = [document["accessors"][primitive["attributes"]["POSITION"]]
                 for primitive in primitives]
    lows = [min(accessor["min"][axis] for accessor in positions) for axis in range(3)]
    highs = [max(accessor["max"][axis] for accessor in positions) for axis in range(3)]
    bounds = [round(highs[axis] - lows[axis], 6) for axis in range(3)]
    if not (0.40 <= bounds[0] <= 0.44 and 0.20 <= bounds[1] <= 0.22 and
            0.19 <= bounds[2] <= 0.21):
        raise RuntimeError(f"garage floodlight is no longer a plausible wall pack: {bounds}")
    if abs(lows[0] + highs[0]) > 1.0e-6 or abs(lows[2] + highs[2]) > 1.0e-6 or abs(lows[1]) > 1.0e-6:
        raise RuntimeError(f"garage floodlight origin is not bottom-centred: {lows}..{highs}")
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    if triangles < 140 or triangles > 500:
        raise RuntimeError(f"garage floodlight detail budget changed unexpectedly: {triangles}")
    return triangles, bounds, hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.check:
        if not TARGET.is_file():
            raise RuntimeError(f"missing committed floodlight: {TARGET}")
        with tempfile.TemporaryDirectory(prefix="garage-floodlight-") as scratch:
            candidate = Path(scratch) / TARGET.name
            author(candidate)
            if candidate.read_bytes() != TARGET.read_bytes():
                raise RuntimeError("committed garage floodlight is not byte-identical to its generator")
        triangles, bounds, digest = validate(TARGET)
    elif args.selftest:
        with tempfile.TemporaryDirectory(prefix="garage-floodlight-selftest-") as scratch:
            candidate = Path(scratch) / TARGET.name
            triangles, bounds, digest = author(candidate)
            validate(candidate)
    else:
        triangles, bounds, digest = author(TARGET)
        validate(TARGET)
    print(f"garage_floodlight: {triangles} triangles, {bounds} m, sha256 {digest[:16]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
