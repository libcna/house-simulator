#!/usr/bin/env python3
"""Author and validate the four ground-mounted front-facade uplights.

The project-authored deterministic fixture is a compact dark-bronze landscape spotlight with a
round anchored foot, a real yoke and an upward-tilted cylindrical head. ``UplightLens`` remains a
separate material slot so the existing stock-XNA fixture path can switch only the optical face.
No network input, third-party mesh or embedded texture is involved.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import struct
import sys
import tempfile
from pathlib import Path

import gltf_io

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "world"))
import layout_io  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
TARGET = REPO / "assets-src" / "Models" / "Fixtures" / "facade_uplight.glb"
PROPS = REPO / "assets-src" / "world" / "layout.props.json"
LIGHTS = REPO / "assets-src" / "world" / "layout.lights.json"
CELLS = REPO / "assets-src" / "world" / "layout.cells.json"
MATERIALS = ("UplightMetal", "UplightLens")
LIGHT_DIRECTION = (0.0, 0.9734, -0.2290)
LIGHT_OFFSET = (0.0, 0.243, -0.022)
GROUP = "LG_EXT_FACADE_UPLIGHT"
PLACEMENTS = (
    ("PROP_EXT_FACADE_UPLIGHT_W1", "LIGHT_EXT_FACADE_UPLIGHT_W1", "EXT_FRONTYARD_W",
     [-11.35, 0.0, -12.65], ["L1_BED3", "L2_BED6"]),
    ("PROP_EXT_FACADE_UPLIGHT_W2", "LIGHT_EXT_FACADE_UPLIGHT_W2", "EXT_FRONTYARD_W",
     [-6.15, 0.0, -12.66], ["L1_BED4", "L2_LIBRARY"]),
    ("PROP_EXT_FACADE_UPLIGHT_E1", "LIGHT_EXT_FACADE_UPLIGHT_E1", "EXT_FRONTYARD_E",
     [4.65, 0.0, -12.66], ["L1_STAIR_MAIN", "L2_STAIR_MAIN"]),
    ("PROP_EXT_FACADE_UPLIGHT_E2", "LIGHT_EXT_FACADE_UPLIGHT_E2", "EXT_FRONTYARD_E",
     [7.95, 0.0, -12.65], ["L1_BED5", "L2_STAIR_ATTIC"]),
)


class Mesh:
    def __init__(self) -> None:
        self.positions: list[list[tuple[float, float, float]]] = [[], []]
        self.normals: list[list[tuple[float, float, float]]] = [[], []]
        self.uvs: list[list[tuple[float, float]]] = [[], []]
        self.indices: list[list[int]] = [[], []]

    def triangle(self, material: int, corners: tuple[tuple[float, float, float], ...],
                 normal: tuple[float, float, float]) -> None:
        base = len(self.positions[material])
        self.positions[material].extend(corners)
        self.normals[material].extend((normal,) * 3)
        self.uvs[material].extend(((0.0, 0.0), (1.0, 0.0), (0.5, 1.0)))
        self.indices[material].extend((base, base + 1, base + 2))

    def quad(self, material: int, corners: tuple[tuple[float, float, float], ...],
             normal: tuple[float, float, float]) -> None:
        base = len(self.positions[material])
        self.positions[material].extend(corners)
        self.normals[material].extend((normal,) * 4)
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

    def cylinder(self, material: int, centre: tuple[float, float, float],
                 axis: tuple[float, float, float], length: float, radius: float,
                 segments: int, front_cap: bool = True, back_cap: bool = True) -> None:
        magnitude = math.sqrt(sum(value * value for value in axis))
        direction = tuple(value / magnitude for value in axis)
        if abs(direction[1]) > 0.95:
            tangent = (1.0, 0.0, 0.0)
        else:
            tangent_magnitude = math.sqrt(direction[1] * direction[1] +
                                          direction[2] * direction[2])
            tangent = (0.0, direction[2] / tangent_magnitude,
                       -direction[1] / tangent_magnitude)
        bitangent = (
            direction[1] * tangent[2] - direction[2] * tangent[1],
            direction[2] * tangent[0] - direction[0] * tangent[2],
            direction[0] * tangent[1] - direction[1] * tangent[0],
        )
        back = tuple(centre[index] - direction[index] * length * 0.5 for index in range(3))
        front = tuple(centre[index] + direction[index] * length * 0.5 for index in range(3))

        def ring_point(origin: tuple[float, float, float], angle: float):
            return tuple(origin[index] + radius *
                         (tangent[index] * math.cos(angle) +
                          bitangent[index] * math.sin(angle)) for index in range(3))

        for segment in range(segments):
            a0 = 2.0 * math.pi * segment / segments
            a1 = 2.0 * math.pi * (segment + 1) / segments
            normal = tuple(tangent[index] * math.cos((a0 + a1) * 0.5) +
                           bitangent[index] * math.sin((a0 + a1) * 0.5)
                           for index in range(3))
            self.quad(material, (ring_point(back, a0), ring_point(front, a0),
                                 ring_point(front, a1), ring_point(back, a1)), normal)
            if back_cap:
                self.triangle(material, (back, ring_point(back, a1), ring_point(back, a0)),
                              tuple(-value for value in direction))
            if front_cap:
                self.triangle(material, (front, ring_point(front, a0), ring_point(front, a1)),
                              direction)


def author(path: Path) -> tuple[int, list[float], str]:
    mesh = Mesh()

    # An anchored round foot and two-piece yoke make the source read as a real adjustable garden
    # luminaire even in the close front-bed camera. The optical head is pitched 77 degrees above
    # horizontal and is authored facing the facade, so instances require no hidden rotation.
    mesh.cylinder(0, (0.0, 0.015, 0.0), (0.0, 1.0, 0.0), 0.030, 0.100, 16)
    mesh.cylinder(0, (0.0, 0.050, 0.0), (0.0, 1.0, 0.0), 0.040, 0.060, 12)
    mesh.box(0, (-0.067, 0.105, 0.0), (0.018, 0.105, 0.045))
    mesh.box(0, (0.067, 0.105, 0.0), (0.018, 0.105, 0.045))
    mesh.cylinder(0, (0.0, 0.151, 0.0), LIGHT_DIRECTION, 0.188, 0.072, 16,
                  front_cap=False)
    # The warm face sits just proud of the bronze barrel rather than coplanar with it.
    mesh.cylinder(1, LIGHT_OFFSET, LIGHT_DIRECTION, 0.004, 0.060, 16,
                  back_cap=False)

    blob = bytearray()
    views: list[dict] = []
    accessors: list[dict] = []

    def add(values: list, fmt: str, component: int, kind: str, target: int) -> int:
        offset = len(blob)
        raw = b"".join(struct.pack("<" + fmt, *value) if isinstance(value, tuple)
                       else struct.pack("<" + fmt, value) for value in values)
        blob.extend(raw)
        blob.extend(b"\0" * ((4 - len(raw) % 4) % 4))
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(raw),
                      "target": target})
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
    triangles = 0
    for material in range(len(MATERIALS)):
        position = add(mesh.positions[material], "3f", 5126, "VEC3", 34962)
        normal = add(mesh.normals[material], "3f", 5126, "VEC3", 34962)
        uv = add(mesh.uvs[material], "2f", 5126, "VEC2", 34962)
        indices = add(mesh.indices[material], "H", 5123, "SCALAR", 34963)
        triangles += len(mesh.indices[material]) // 3
        primitives.append({"attributes": {"POSITION": position, "NORMAL": normal,
                                           "TEXCOORD_0": uv},
                           "indices": indices, "mode": 4, "material": material})

    positions = [position for group in mesh.positions for position in group]
    lows = [min(value[axis] for value in positions) for axis in range(3)]
    highs = [max(value[axis] for value in positions) for axis in range(3)]
    bounds = [round(highs[axis] - lows[axis], 6) for axis in range(3)]
    document = {
        "asset": {"version": "2.0", "generator": "cna-house tools/assets/facade_uplight.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "FacadeUplight", "mesh": 0}],
        "meshes": [{"name": "FacadeUplight", "primitives": primitives}],
        "materials": [
            {"name": "UplightMetal", "doubleSided": False,
             "pbrMetallicRoughness": {"baseColorFactor": [0.10, 0.055, 0.03, 1.0],
                                      "metallicFactor": 0.82, "roughnessFactor": 0.36}},
            {"name": "UplightLens", "doubleSided": False,
             "pbrMetallicRoughness": {"baseColorFactor": [1.0, 0.76, 0.43, 1.0],
                                      "metallicFactor": 0.0, "roughnessFactor": 0.86}},
        ],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    data = gltf_io.write_glb(path, document, bytes(blob))
    return triangles, bounds, hashlib.sha256(data).hexdigest()


def validate(path: Path) -> tuple[int, list[float], str]:
    document, _ = gltf_io.read_model(path)
    names = tuple(material["name"] for material in document["materials"])
    if names != MATERIALS:
        raise RuntimeError(f"facade-uplight material contract changed: {names}")
    primitives = document["meshes"][0]["primitives"]
    if len(primitives) != 2 or any(set(row["attributes"]) !=
                                   {"POSITION", "NORMAL", "TEXCOORD_0"}
                                   for row in primitives):
        raise RuntimeError("facade uplight must retain two complete visible material slots")
    positions = [document["accessors"][row["attributes"]["POSITION"]] for row in primitives]
    lows = [min(accessor["min"][axis] for accessor in positions) for axis in range(3)]
    highs = [max(accessor["max"][axis] for accessor in positions) for axis in range(3)]
    bounds = [round(highs[axis] - lows[axis], 6) for axis in range(3)]
    if not (0.19 <= bounds[0] <= 0.21 and 0.23 <= bounds[1] <= 0.26 and
            0.18 <= bounds[2] <= 0.22):
        raise RuntimeError(f"facade uplight is no longer a plausible landscape fixture: {bounds}")
    if abs(lows[1]) > 1.0e-6:
        raise RuntimeError(f"facade-uplight origin is not grounded: {lows}..{highs}")
    triangles = sum(document["accessors"][row["indices"]]["count"] // 3 for row in primitives)
    if triangles < 180 or triangles > 320:
        raise RuntimeError(f"facade-uplight detail budget changed unexpectedly: {triangles}")
    return triangles, bounds, hashlib.sha256(path.read_bytes()).hexdigest()


def validate_instances() -> None:
    props = {row["id"]: row for row in layout_io.load_file(PROPS, "props")["props"]}
    lights = {row["id"]: row for row in layout_io.load_file(LIGHTS, "lights")["lights"]}
    cells = {row["id"]: row for row in layout_io.load_file(CELLS, "cells")["cells"]}
    for prop_id, light_id, cell_id, position, bake_cells in PLACEMENTS:
        prop = props[prop_id]
        light = lights[light_id]
        if prop["asset"] != "MODEL_FIXTURE_FACADE_UPLIGHT" or prop["cell"] != cell_id or \
                prop["position"] != position or prop["yawDeg"] != 0 or prop["scale"] != 1 or \
                not prop["static"] or prop["collision"] != "none":
            raise RuntimeError(f"canonical facade-uplight placement changed: {prop_id}")
        expected_light = [round(position[index] + LIGHT_OFFSET[index], 3)
                          for index in range(3)]
        if light["cell"] != cell_id or light["group"] != GROUP or \
                light["type"] != "spot" or light["position"] != expected_light or \
                light["direction"] != list(LIGHT_DIRECTION) or \
                light["coneInnerDeg"] != 24.0 or light["coneOuterDeg"] != 44.0 or \
                light["colorK"] != 3000 or light["intensityLm"] != 2400.0 or \
                light["range"] != 9.0 or light["bakeLumensPerRadiantWatt"] != 5.0 or \
                light["bakeCells"] != bake_cells or light["fixtureProp"] != prop_id or \
                light["emissiveMaterialSlot"] != "UplightLens" or \
                light["castsBlobShadow"] or not light["bakedIntoLightmap"] or \
                light["defaultOn"] or not light["duskSensor"]:
            raise RuntimeError(f"canonical facade-uplight optics changed: {light_id}")
    for cell_id in ("EXT_FRONTYARD_W", "EXT_FRONTYARD_E"):
        if cells[cell_id].get("lightGroups") != [GROUP]:
            raise RuntimeError(f"{cell_id} no longer owns only the facade-uplight dusk group")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.check:
        if not TARGET.is_file():
            raise RuntimeError(f"missing committed facade uplight: {TARGET}")
        with tempfile.TemporaryDirectory(prefix="facade-uplight-") as scratch:
            candidate = Path(scratch) / TARGET.name
            author(candidate)
            if candidate.read_bytes() != TARGET.read_bytes():
                raise RuntimeError("committed facade uplight is not byte-identical to its generator")
        triangles, bounds, digest = validate(TARGET)
        validate_instances()
    elif args.selftest:
        with tempfile.TemporaryDirectory(prefix="facade-uplight-selftest-") as scratch:
            candidate = Path(scratch) / TARGET.name
            author(candidate)
            triangles, bounds, digest = validate(candidate)
    else:
        triangles, bounds, digest = author(TARGET)
        validate(TARGET)
    print(f"facade_uplight: {triangles} triangles, {bounds} m, sha256 {digest[:16]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
