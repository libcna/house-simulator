#!/usr/bin/env python3
"""Author the measured front-porch lantern used by the visual vertical slice.

The fixture is project-authored and deterministic: a dark-bronze wall plate, arm, framed canopy
and four-panel warm diffuser.  Its asset origin follows the repository fixture contract: centred in
X/Z at the lowest support point.  The canonical prop transform compensates for that authored pivot,
so the optical centre still coincides with the light position.  The source material names are part
of the runtime contract: ``LanternShade`` is the slot named by ``layout.lights.json`` and is kept
separate from ``LanternMetal`` by the chunk builder.

No network input or third-party model is involved.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import sys
import tempfile
from pathlib import Path

import gltf_io

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "world"))
import layout_io  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
TARGET = REPO / "assets-src" / "Models" / "Fixtures" / "porch_lantern.glb"
MATERIALS = ("LanternMetal", "LanternShade")
PROPS = REPO / "assets-src" / "world" / "layout.props.json"
LIGHTS = REPO / "assets-src" / "world" / "layout.lights.json"


class Mesh:
    def __init__(self) -> None:
        self.positions: list[list[tuple[float, float, float]]] = [[], []]
        self.normals: list[list[tuple[float, float, float]]] = [[], []]
        self.uvs: list[list[tuple[float, float]]] = [[], []]
        self.indices: list[list[int]] = [[], []]

    def quad(self, material: int, corners: list[tuple[float, float, float]],
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
            ((0.0, 0.0, 1.0), [(lo[0], lo[1], hi[2]), (hi[0], lo[1], hi[2]),
                                (hi[0], hi[1], hi[2]), (lo[0], hi[1], hi[2])]),
            ((0.0, 0.0, -1.0), [(hi[0], lo[1], lo[2]), (lo[0], lo[1], lo[2]),
                                 (lo[0], hi[1], lo[2]), (hi[0], hi[1], lo[2])]),
            ((1.0, 0.0, 0.0), [(hi[0], lo[1], hi[2]), (hi[0], lo[1], lo[2]),
                                (hi[0], hi[1], lo[2]), (hi[0], hi[1], hi[2])]),
            ((-1.0, 0.0, 0.0), [(lo[0], lo[1], lo[2]), (lo[0], lo[1], hi[2]),
                                 (lo[0], hi[1], hi[2]), (lo[0], hi[1], lo[2])]),
            ((0.0, 1.0, 0.0), [(lo[0], hi[1], hi[2]), (hi[0], hi[1], hi[2]),
                                (hi[0], hi[1], lo[2]), (lo[0], hi[1], lo[2])]),
            ((0.0, -1.0, 0.0), [(lo[0], lo[1], lo[2]), (hi[0], lo[1], lo[2]),
                                 (hi[0], lo[1], hi[2]), (lo[0], lo[1], hi[2])]),
        )
        for normal, corners in faces:
            self.quad(material, corners, normal)

    def elliptical_plate(self, radius_x: float, radius_y: float, z0: float,
                         depth: float, segments: int = 24) -> None:
        material = 0
        front = z0 + depth
        for index in range(segments):
            a = 2.0 * math.pi * index / segments
            b = 2.0 * math.pi * (index + 1) / segments
            pa = (radius_x * math.cos(a), radius_y * math.sin(a), z0)
            pb = (radius_x * math.cos(b), radius_y * math.sin(b), z0)
            fa = (pa[0], pa[1], front)
            fb = (pb[0], pb[1], front)
            nx = math.cos((a + b) * 0.5) / radius_x
            ny = math.sin((a + b) * 0.5) / radius_y
            length = math.hypot(nx, ny)
            self.quad(material, [pa, pb, fb, fa], (nx / length, ny / length, 0.0))

            base = len(self.positions[material])
            self.positions[material].extend(((0.0, 0.0, front), fa, fb,
                                             (0.0, 0.0, z0), pb, pa))
            self.normals[material].extend(((0.0, 0.0, 1.0),) * 3 + ((0.0, 0.0, -1.0),) * 3)
            self.uvs[material].extend(((0.5, 0.5), (0.5 + 0.5 * math.cos(a),
                                                   0.5 + 0.5 * math.sin(a)),
                                       (0.5 + 0.5 * math.cos(b), 0.5 + 0.5 * math.sin(b))) * 2)
            self.indices[material].extend((base, base + 1, base + 2,
                                           base + 3, base + 4, base + 5))

    def roof(self) -> None:
        y0, y1 = 0.195, 0.285
        xb, xt = 0.165, 0.075
        zb0, zb1 = 0.065, 0.285
        zt0, zt1 = 0.105, 0.245
        faces = (
            [(-xb, y0, zb1), (xb, y0, zb1), (xt, y1, zt1), (-xt, y1, zt1)],
            [(xb, y0, zb0), (-xb, y0, zb0), (-xt, y1, zt0), (xt, y1, zt0)],
            [(xb, y0, zb1), (xb, y0, zb0), (xt, y1, zt0), (xt, y1, zt1)],
            [(-xb, y0, zb0), (-xb, y0, zb1), (-xt, y1, zt1), (-xt, y1, zt0)],
        )
        for corners in faces:
            ax, ay, az = corners[0]
            bx, by, bz = corners[1]
            cx, cy, cz = corners[2]
            ux, uy, uz = bx - ax, by - ay, bz - az
            vx, vy, vz = cx - ax, cy - ay, cz - az
            normal = (uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx)
            length = math.sqrt(sum(value * value for value in normal))
            self.quad(0, corners, tuple(value / length for value in normal))


def author(path: Path) -> tuple[int, list[float], str]:
    mesh = Mesh()
    mesh.elliptical_plate(0.125, 0.235, -0.018, 0.040)
    mesh.box(0, (0.0, 0.09, 0.075), (0.060, 0.055, 0.125))
    mesh.box(0, (0.0, 0.17, 0.145), (0.155, 0.035, 0.085))
    mesh.box(0, (0.0, -0.205, 0.175), (0.300, 0.040, 0.235))
    mesh.box(0, (0.0, 0.185, 0.175), (0.300, 0.040, 0.235))
    for x in (-0.137, 0.137):
        for z in (0.078, 0.272):
            mesh.box(0, (x, -0.010, z), (0.026, 0.365, 0.026))
    mesh.roof()
    mesh.box(0, (0.0, 0.305, 0.175), (0.035, 0.055, 0.035))
    mesh.box(0, (0.0, -0.250, 0.175), (0.075, 0.050, 0.075))

    # Four frosted panels sit just inside the cage.  The material is two-sided at runtime, so each
    # is a single physical sheet rather than a fake glowing solid.
    mesh.quad(1, [(-0.115, -0.175, 0.258), (0.115, -0.175, 0.258),
                  (0.115, 0.155, 0.258), (-0.115, 0.155, 0.258)], (0.0, 0.0, 1.0))
    mesh.quad(1, [(0.115, -0.175, 0.245), (0.115, -0.175, 0.105),
                  (0.115, 0.155, 0.105), (0.115, 0.155, 0.245)], (1.0, 0.0, 0.0))
    mesh.quad(1, [(-0.115, -0.175, 0.105), (-0.115, -0.175, 0.245),
                  (-0.115, 0.155, 0.245), (-0.115, 0.155, 0.105)], (-1.0, 0.0, 0.0))
    mesh.quad(1, [(0.105, -0.175, 0.095), (-0.105, -0.175, 0.095),
                  (-0.105, 0.155, 0.095), (0.105, 0.155, 0.095)], (0.0, 0.0, -1.0))

    # The design coordinates above are relative to the optical centre on the wall.  Store the GLB
    # in the repository-wide prop convention instead: Y=0 is its support point and X/Z are centred.
    # `layout.props.json` applies the inverse translation, preserving the designed world pose.
    origin_shift = (0.0, 0.275, -0.13725)
    for group in mesh.positions:
        for index, position in enumerate(group):
            group[index] = tuple(position[axis] + origin_shift[axis] for axis in range(3))

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
        accessor["min"] = [min(value[i] for value in values) for i in range(width)] \
            if width > 1 else [min(values)]
        accessor["max"] = [max(value[i] for value in values) for i in range(width)] \
            if width > 1 else [max(values)]
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
        "asset": {"version": "2.0", "generator": "cna-house tools/assets/porch_lantern.py"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "PorchLantern", "mesh": 0}],
        "meshes": [{"name": "PorchLantern", "primitives": primitives}],
        "materials": [
            {"name": "LanternMetal", "doubleSided": False,
             "pbrMetallicRoughness": {"baseColorFactor": [0.16, 0.09, 0.045, 1.0],
                                      "metallicFactor": 0.75, "roughnessFactor": 0.34}},
            {"name": "LanternShade", "doubleSided": True,
             "pbrMetallicRoughness": {"baseColorFactor": [1.0, 0.74, 0.42, 1.0],
                                      "metallicFactor": 0.0, "roughnessFactor": 0.88}},
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
        raise RuntimeError(f"porch lantern material contract changed: {names}")
    primitives = document["meshes"][0]["primitives"]
    if len(primitives) != 2 or any(set(primitive["attributes"]) !=
                                   {"POSITION", "NORMAL", "TEXCOORD_0"}
                                   for primitive in primitives):
        raise RuntimeError("porch lantern must retain two complete visible material slots")
    positions = [document["accessors"][primitive["attributes"]["POSITION"]]
                 for primitive in primitives]
    lows = [min(accessor["min"][axis] for accessor in positions) for axis in range(3)]
    highs = [max(accessor["max"][axis] for accessor in positions) for axis in range(3)]
    bounds = [round(highs[axis] - lows[axis], 6) for axis in range(3)]
    if not (0.32 <= bounds[0] <= 0.34 and 0.60 <= bounds[1] <= 0.62 and
            0.30 <= bounds[2] <= 0.32):
        raise RuntimeError(f"porch lantern is no longer a plausible wall fixture: {bounds}")
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in primitives)
    if triangles < 150 or triangles > 700:
        raise RuntimeError(f"porch lantern detail budget changed unexpectedly: {triangles}")
    return triangles, bounds, hashlib.sha256(path.read_bytes()).hexdigest()


def validate_front_balcony_instance() -> None:
    """Keep HOUSE-01287's physical source, receiver and manual-control boundary pinned."""
    props = {row["id"]: row for row in layout_io.load_file(PROPS, "props")["props"]}
    lights = {row["id"]: row for row in layout_io.load_file(LIGHTS, "lights")["lights"]}
    prop = props["PROP_L1_BALCONY_FRONT_LANTERN"]
    light = lights["LIGHT_L1_BALCONY_FRONT_MAIN_1"]

    if prop["asset"] != "MODEL_FIXTURE_PORCH_LANTERN" or \
            prop["cell"] != "L1_BALCONY_FRONT" or \
            prop["position"] != [0.0, 5.8, -14.11275] or prop["yawDeg"] != 0 or \
            prop["scale"] != 1 or not prop["static"] or prop["collision"] != "none":
        raise RuntimeError("canonical front-balcony lantern placement changed")
    if light["cell"] != "L1_BALCONY_FRONT" or \
            light["group"] != "LG_L1_BALCONY_FRONT_MAIN" or \
            light["type"] != "spot" or light["position"] != [0.0, 6.075, -14.073] or \
            light["direction"] != [0.0, -1.0, 0.0] or \
            light["coneInnerDeg"] != 60.0 or light["coneOuterDeg"] != 100.0 or \
            light["colorK"] != 2700 or light["intensityLm"] != 600.0 or \
            light["range"] != 6.27 or light["bakeLumensPerRadiantWatt"] != 100.0 or \
            light["bakeCells"] != ["L1_LANDING"] or \
            light["fixtureProp"] != prop["id"] or \
            light["emissiveMaterialSlot"] != "LanternShade" or \
            not light["castsBlobShadow"] or not light["bakedIntoLightmap"] or \
            not light["defaultOn"]:
        raise RuntimeError("front-balcony lantern optical linkage changed")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.check:
        if not TARGET.is_file():
            raise RuntimeError(f"missing committed lantern: {TARGET}")
        with tempfile.TemporaryDirectory(prefix="porch-lantern-") as scratch:
            candidate = Path(scratch) / TARGET.name
            author(candidate)
            expected = TARGET.read_bytes()
            if candidate.read_bytes() != expected:
                raise RuntimeError("committed porch lantern is not byte-identical to its generator")
        triangles, bounds, digest = validate(TARGET)
        validate_front_balcony_instance()
    elif args.selftest:
        with tempfile.TemporaryDirectory(prefix="porch-lantern-selftest-") as scratch:
            target = Path(scratch) / TARGET.name
            triangles, bounds, digest = author(target)
            validate(target)
    else:
        triangles, bounds, digest = author(TARGET)
        validate(TARGET)
    print(f"porch_lantern: {triangles} triangles, {bounds} m, sha256 {digest[:16]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
