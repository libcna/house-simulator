#!/usr/bin/env python3
"""origin_check.py -- the model's origin must be where the placement code assumes it is.

`HOUSE-00188`: the origin sits at the **support point** — the point the object rests on — within
2 cm, or at the **wall plane** for something wall-mounted.

Why 2 cm and why it matters: every placement in `assets-src/world/*.json` is a position for the
origin, so an origin 30 cm above the floor puts the whole prop 30 cm in the air, and an origin at
the centre of a fridge buries half of it in the floor. Both look like a physics or a layout bug and
are neither. 2 cm is the tolerance because it is below what reads as wrong at eye height and above
what a decimation or a normal-transfer can move a vertex by.

    tools/assets/origin_check.py                    # every model with a manifest row
    tools/assets/origin_check.py PATH ...
    tools/assets/origin_check.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gltf_validate  # noqa: E402
import manifest as manifest_tool  # noqa: E402
import scale_check  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

#: Metres. Below what reads as wrong at eye height, above what a decimation can move a vertex by.
TOLERANCE = 0.02

#: Categories mounted on a wall rather than standing on a floor. Their origin belongs on the WALL
#: PLANE -- the back face, at -Z in glTF's convention (`HOUSE-00070`: -Z is forward, so the face
#: touching the wall is the one at maximum +Z... which is exactly the kind of sign error this list
#: exists to make explicit rather than leave to each author.
#:
#: The wall plane is the back of the object, which faces AWAY from the viewer: +Z.
WALL_MOUNTED = {
    "cabinet-upper",
    "switch",
    "socket",
    "picture",
    "radiator",
    "wall-light",
    "mirror",
}

#: Floor-supported architectural objects which also register against a wall plane. Unlike a
#: cabinet, a fireplace must satisfy BOTH conditions: moving its origin to the depth centre makes
#: canonical placement opaque, while treating it only as wall-mounted would stop checking that
#: the hearth is actually grounded.
FLOOR_AND_WALL = {
    "fireplace-surround",
}

#: Categories with no meaningful support point.
EXEMPT = {
    "fallback": "a placeholder whose origin is not used for placement",
    "effect": "not geometry",
    "font": "not geometry",
    "audio": "not geometry",
    "video": "not geometry",
    "avatar": "placed by the skeleton's root, not by the mesh bounds",
    "dog": "placed by the skeleton's root",
    "cat": "placed by the skeleton's root",
}


def check(path: Path, category: str) -> list[str]:
    if category in EXEMPT:
        return []

    document, error = gltf_validate.read_gltf_json(path)
    if error is not None:
        return [error]
    assert document is not None

    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        return [
            "no POSITION accessor declares min/max, so the bounds are unknown and the origin "
            "cannot be checked without decoding the buffers"
        ]
    low, high = bounds
    problems: list[str] = []

    wall_plane = category in WALL_MOUNTED or category in FLOOR_AND_WALL

    # X and Z centred, except depth for anything registered to a wall plane. An object whose
    # origin is off to one side rotates about that side, which is visible the moment it is turned.
    for axis, index in (("X", 0), ("Z", 2)):
        if wall_plane and axis == "Z":
            continue
        centre = (low[index] + high[index]) / 2.0
        if abs(centre) > TOLERANCE:
            problems.append(
                f"the {axis} centre is {centre:+.3f} m from the origin (tolerance "
                f"±{TOLERANCE:.2f} m); the object will rotate about a point outside itself"
            )

    if wall_plane:
        # The back face -- the one against the wall -- at Z = 0.
        if abs(high[2]) > TOLERANCE:
            problems.append(
                f"wall-mounted: the back face is at Z {high[2]:+.3f} m, not 0 (tolerance "
                f"±{TOLERANCE:.2f} m); it will float off the wall or sink into it by that much"
            )
    if category not in WALL_MOUNTED or category in FLOOR_AND_WALL:
        # The support point: the lowest vertex, at Y = 0.
        if abs(low[1]) > TOLERANCE:
            direction = "above" if low[1] > 0 else "below"
            problems.append(
                f"the support point is {abs(low[1]):.3f} m {direction} the origin (tolerance "
                f"±{TOLERANCE:.2f} m); every placement of this asset will be wrong by that much"
            )

    return problems


def selftest() -> int:
    import tempfile

    def make(low: list[float], high: list[float]) -> bytes:
        payload = json.dumps(
            {
                "asset": {"version": "2.0"},
                "accessors": [
                    {"type": "VEC3", "componentType": 5126, "count": 8, "min": low, "max": high}
                ],
                "meshes": [{"name": "m", "primitives": [{"attributes": {"POSITION": 0}}]}],
            }
        ).encode("utf-8")
        payload += b" " * ((4 - len(payload) % 4) % 4)
        header = struct.pack("<III", gltf_validate.GLB_MAGIC, 2, 12 + 8 + len(payload))
        return header + struct.pack("<II", len(payload), gltf_validate.CHUNK_JSON) + payload

    failures = 0
    with tempfile.TemporaryDirectory(prefix="origin-selftest-", dir=str(REPO / "build")) as work:
        cases = [
            ("a chair resting on the origin", [-0.25, 0.0, -0.25], [0.25, 0.45, 0.25], "chair", 0),
            (
                "a chair modelled about its centre",
                [-0.25, -0.225, -0.25],
                [0.25, 0.225, 0.25],
                "chair",
                1,
            ),
            (
                "a chair 30 cm in the air",
                [-0.25, 0.30, -0.25],
                [0.25, 0.75, 0.25],
                "chair",
                1,
            ),
            (
                "a chair 1.5 cm out, inside the tolerance",
                [-0.25, 0.015, -0.25],
                [0.25, 0.465, 0.25],
                "chair",
                0,
            ),
            (
                "a chair offset in X",
                [0.10, 0.0, -0.25],
                [0.60, 0.45, 0.25],
                "chair",
                1,
            ),
            (
                "a wall cabinet with its back on the wall plane",
                [-0.30, 1.45, -0.35],
                [0.30, 2.05, 0.0],
                "cabinet-upper",
                0,
            ),
            (
                "a wall cabinet 10 cm off the wall",
                [-0.30, 1.45, -0.25],
                [0.30, 2.05, 0.10],
                "cabinet-upper",
                1,
            ),
            (
                "a grounded fireplace registered to a wall",
                [-0.90, 0.0, -0.60],
                [0.90, 2.30, 0.0],
                "fireplace-surround",
                0,
            ),
            (
                "a floating fireplace registered to a wall",
                [-0.90, 0.10, -0.60],
                [0.90, 2.40, 0.0],
                "fireplace-surround",
                1,
            ),
        ]
        for name, low, high, category, expect_problems in cases:
            path = Path(work) / f"{name.replace(' ', '_')}.glb"
            path.write_bytes(make(low, high))
            problems = check(path, category)
            if bool(problems) != bool(expect_problems):
                verb = "rejected" if problems else "accepted"
                print(f"  SELFTEST FAILED: '{name}' was {verb}: {problems}", file=sys.stderr)
                failures += 1
            elif problems:
                print(f"  caught '{name}': {problems[0]}")
            else:
                print(f"  '{name}' passes")
    if failures:
        return 1
    print("origin_check: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="*", type=Path)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    document = manifest_tool.load()
    by_file = {row.get("sourceFile", ""): row for row in document.get("assets", [])}
    targets = [
        (REPO / relative, row)
        for relative, row in sorted(by_file.items())
        if relative.lower().endswith((".glb", ".gltf"))
    ]
    if args.paths:
        targets = [
            (p, by_file.get(p.relative_to(REPO).as_posix() if p.is_relative_to(REPO) else str(p)))
            for p in args.paths
        ]
    if not targets:
        print("origin_check: no models with a manifest row.")
        return 0

    total = 0
    for path, row in targets:
        relative = path.relative_to(REPO) if path.is_relative_to(REPO) else path
        if row is None:
            print(f"origin_check: {relative} has no manifest row", file=sys.stderr)
            total += 1
            continue
        category = row.get("category", "")
        problems = check(path, category)
        if problems:
            print(f"origin_check: {relative} (category '{category}')", file=sys.stderr)
            for problem in problems:
                print(f"  {problem}", file=sys.stderr)
            total += len(problems)
        else:
            note = " -- exempt" if category in EXEMPT else ""
            print(f"origin_check: {relative} OK (category '{category}'){note}")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
