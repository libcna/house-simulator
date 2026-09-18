#!/usr/bin/env python3
"""Author and validate the four low luminaires along the canonical front walk.

The model is a project-authored, deterministic low-voltage bollard: a compact dark-bronze base
and shaft support a framed opal light chamber below a rain cap.  ``BollardShade`` remains a
separate material slot so the existing stock-XNA fixture path can switch only the optical chamber.
No network input, third-party mesh or embedded texture is involved.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import tempfile
from pathlib import Path

import gltf_io

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "world"))
import layout_io  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
TARGET = REPO / "assets-src" / "Models" / "Fixtures" / "path_bollard.glb"
PROPS = REPO / "assets-src" / "world" / "layout.props.json"
LIGHTS = REPO / "assets-src" / "world" / "layout.lights.json"
INTERACTABLES = REPO / "assets-src" / "world" / "interactables.json"
MATERIALS = ("BollardMetal", "BollardShade")
PLACEMENTS = (
    ("PROP_EXT_WALK_BOLLARD_1", "LIGHT_EXT_WALK_PATH_1", [-0.75, 0.0, -9.50]),
    ("PROP_EXT_WALK_BOLLARD_2", "LIGHT_EXT_WALK_PATH_2", [0.75, 0.0, -6.60]),
    ("PROP_EXT_WALK_BOLLARD_3", "LIGHT_EXT_WALK_PATH_3", [-0.75, 0.0, -3.70]),
    ("PROP_EXT_WALK_BOLLARD_4", "LIGHT_EXT_WALK_PATH_4", [0.75, 0.0, -0.80]),
)


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


def author(path: Path) -> tuple[int, list[float], str]:
    mesh = Mesh()

    # A 200 mm anchored foot, 120 mm post and stepped cap give the half-metre fixture a credible
    # outdoor silhouette without exaggerating an object normally seen from several metres away.
    mesh.box(0, (0.0, 0.018, 0.0), (0.200, 0.036, 0.200))
    mesh.box(0, (0.0, 0.063, 0.0), (0.150, 0.054, 0.150))
    mesh.box(0, (0.0, 0.235, 0.0), (0.120, 0.300, 0.120))

    # The diffuser is physically bounded by four corner mullions and top/bottom rails.  It is a
    # real volume rather than an emissive billboard, so every approach angle retains the frame.
    mesh.box(1, (0.0, 0.430, 0.0), (0.126, 0.110, 0.126))
    for x in (-0.065, 0.065):
        for z in (-0.065, 0.065):
            mesh.box(0, (x, 0.430, z), (0.018, 0.128, 0.018))
    mesh.box(0, (0.0, 0.369, 0.0), (0.158, 0.022, 0.158))
    mesh.box(0, (0.0, 0.491, 0.0), (0.158, 0.022, 0.158))
    mesh.box(0, (0.0, 0.515, 0.0), (0.188, 0.026, 0.188))
    mesh.box(0, (0.0, 0.536, 0.0), (0.148, 0.016, 0.148))

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
        "asset": {"version": "2.0", "generator": "cna-house tools/assets/path_bollard.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "PathBollard", "mesh": 0}],
        "meshes": [{"name": "PathBollard", "primitives": primitives}],
        "materials": [
            {"name": "BollardMetal", "doubleSided": False,
             "pbrMetallicRoughness": {"baseColorFactor": [0.11, 0.065, 0.035, 1.0],
                                      "metallicFactor": 0.78, "roughnessFactor": 0.38}},
            {"name": "BollardShade", "doubleSided": False,
             "pbrMetallicRoughness": {"baseColorFactor": [1.0, 0.73, 0.38, 1.0],
                                      "metallicFactor": 0.0, "roughnessFactor": 0.9}},
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
        raise RuntimeError(f"path-bollard material contract changed: {names}")
    primitives = document["meshes"][0]["primitives"]
    if len(primitives) != 2 or any(set(row["attributes"]) !=
                                   {"POSITION", "NORMAL", "TEXCOORD_0"}
                                   for row in primitives):
        raise RuntimeError("path bollard must retain two complete visible material slots")
    positions = [document["accessors"][row["attributes"]["POSITION"]] for row in primitives]
    lows = [min(accessor["min"][axis] for accessor in positions) for axis in range(3)]
    highs = [max(accessor["max"][axis] for accessor in positions) for axis in range(3)]
    bounds = [round(highs[axis] - lows[axis], 6) for axis in range(3)]
    if not (0.19 <= bounds[0] <= 0.21 and 0.53 <= bounds[1] <= 0.55 and
            0.19 <= bounds[2] <= 0.21):
        raise RuntimeError(f"path bollard is no longer a plausible low fixture: {bounds}")
    triangles = sum(document["accessors"][row["indices"]]["count"] // 3 for row in primitives)
    if triangles < 120 or triangles > 220:
        raise RuntimeError(f"path-bollard detail budget changed unexpectedly: {triangles}")
    return triangles, bounds, hashlib.sha256(path.read_bytes()).hexdigest()


def validate_instances() -> None:
    props = {row["id"]: row for row in layout_io.load_file(PROPS, "props")["props"]}
    lights = {row["id"]: row for row in layout_io.load_file(LIGHTS, "lights")["lights"]}
    for prop_id, light_id, position in PLACEMENTS:
        prop = props[prop_id]
        light = lights[light_id]
        if prop["asset"] != "MODEL_FIXTURE_PATH_BOLLARD" or prop["cell"] != "EXT_WALK" or \
                prop["position"] != position or prop["yawDeg"] != 0 or prop["scale"] != 1 or \
                not prop["static"] or prop["collision"] != "none":
            raise RuntimeError(f"canonical path-bollard placement changed: {prop_id}")
        expected_light = [position[0], 0.43, position[2]]
        if light["cell"] != "EXT_WALK" or light["group"] != "LG_EXT_WALK_PATH" or \
                light["type"] != "spot" or light["position"] != expected_light or \
                light["direction"] != [0.0, -1.0, 0.0] or light["coneInnerDeg"] != 70.0 or \
                light["coneOuterDeg"] != 120.0 or light["colorK"] != 2700 or \
                light["intensityLm"] != 180.0 or light["range"] != 3.2 or \
                light["fixtureProp"] != prop_id or \
                light["emissiveMaterialSlot"] != "BollardShade" or \
                light["castsBlobShadow"] or not light["bakedIntoLightmap"] or \
                not light["defaultOn"]:
            raise RuntimeError(f"canonical path-bollard optics changed: {light_id}")

    switches = {row["id"]: row for row in
                layout_io.load_file(INTERACTABLES, "interactables")["interactables"]}
    switch = switches["SWITCH_EXT_WALK"]
    if switch["cell"] != "EXT_WALK" or switch["state"] != {"LG_EXT_WALK_PATH": False} or \
            switch["persist"] != ["LG_EXT_WALK_PATH"] or \
            "toggle(state.LG_EXT_WALK_PATH)" not in switch["actions"][0]["do"]:
        raise RuntimeError("path bollards no longer retain their independent manual switch")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.check:
        if not TARGET.is_file():
            raise RuntimeError(f"missing committed path bollard: {TARGET}")
        with tempfile.TemporaryDirectory(prefix="path-bollard-") as scratch:
            candidate = Path(scratch) / TARGET.name
            author(candidate)
            if candidate.read_bytes() != TARGET.read_bytes():
                raise RuntimeError("committed path bollard is not byte-identical to its generator")
        triangles, bounds, digest = validate(TARGET)
        validate_instances()
    elif args.selftest:
        with tempfile.TemporaryDirectory(prefix="path-bollard-selftest-") as scratch:
            candidate = Path(scratch) / TARGET.name
            author(candidate)
            triangles, bounds, digest = validate(candidate)
    else:
        triangles, bounds, digest = author(TARGET)
        validate(TARGET)
    print(f"path_bollard: {triangles} triangles, {bounds} m, sha256 {digest[:16]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
