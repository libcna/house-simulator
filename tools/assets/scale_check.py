#!/usr/bin/env python3
"""scale_check.py -- assert an asset's real-world size against its category.

`HOUSE-00187`, enforcing the table in `cna-house.md` §70.5. The house is the point of this project,
and a house whose door is 1.7 m tall reads as wrong before anyone can say why. Scale errors are also
the single most common defect in downloaded assets: a model authored in centimetres imports a
hundred times too large, and a model authored in inches imports 2.54 times too small -- both of
which look plausible in isolation and impossible next to a chair.

The category comes from the manifest row (`category`), so the check is driven by the same record
that carries the licence, and an asset without a row is caught by `check_manifest.py` first.

    tools/assets/scale_check.py                      # every model with a manifest row
    tools/assets/scale_check.py PATH ...
    tools/assets/scale_check.py --list-categories
    tools/assets/scale_check.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gltf_validate  # noqa: E402  (shared .glb reader; one parser, not two)
import manifest as manifest_tool  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

#: `cna-house.md` §70.5, transcribed. Each entry is (axis, low, high, what the number means).
#:
#: The AXIS matters and is not always height: a door leaf is checked on both its height and its
#: width, and checking a bounding box's largest dimension instead would pass a door lying on its
#: side. Axes are glTF's: +Y up, -Z forward (`HOUSE-00070`).
CATEGORIES: dict[str, list[tuple[str, float, float, str]]] = {
    "door-interior": [
        ("y", 1.98, 2.10, "interior door leaf height"),
        ("x", 0.76, 0.95, "interior door leaf width"),
    ],
    "counter-kitchen": [("y", 0.88, 0.95, "kitchen counter height")],
    "cabinet-upper": [("y", 1.40, 1.55, "upper cabinet underside")],
    "table-dining": [("y", 0.72, 0.78, "dining table top")],
    "desk": [("y", 0.72, 0.78, "desk top")],
    "chair": [("y", 0.42, 0.48, "chair seat")],
    "sofa": [("y", 0.38, 0.45, "sofa seat")],
    "bed": [("y", 0.48, 0.62, "bed mattress top")],
    "wc": [("y", 0.38, 0.45, "WC seat")],
    "basin": [("y", 0.80, 0.90, "basin rim")],
    "bath": [("y", 0.50, 0.60, "bath rim")],
    "handrail": [("y", 0.85, 0.95, "handrail height")],
    "avatar": [("y", 1.55, 1.90, "human avatar height")],
    "dog": [("y", 0.50, 0.70, "dog withers height")],
    "cat": [("y", 0.20, 0.32, "cat shoulder height")],
    # A pipeline fixture is not a real-world object, but it is still geometry with a size, and the
    # bug this table exists to catch -- a model authored in centimetres -- is exactly as possible
    # here as anywhere. The band is wide because the size is chosen to be legible in a screenshot;
    # it is a band rather than nothing because 80 would be as wrong as 0.008.
    "fixture": [("x", 0.05, 3.0, "content-pipeline fixture, legible at a few metres"),
                ("y", 0.05, 3.0, "content-pipeline fixture, legible at a few metres"),
                ("z", 0.05, 3.0, "content-pipeline fixture, legible at a few metres")],
}

#: Categories that exist in the manifest but carry no size expectation. Listed explicitly so that a
#: NEW category is an error rather than a silent skip -- the failure this whole check exists to
#: prevent is a model nobody looked at, and "unknown category, so no check" is exactly that.
UNSIZED_CATEGORIES = {
    "fallback": "a deliberately ugly placeholder; its size is meaningless",
    "effect": "not geometry",
    "font": "not geometry",
    "audio": "not geometry",
    "video": "not geometry",
}


def accessor_bounds(document: dict) -> tuple[list[float], list[float]] | None:
    """The model's bounds, from the glTF accessors' own declared `min`/`max`.

    **Read from the accessors rather than from the vertex data.** glTF requires `min` and `max` on a
    POSITION accessor, so they are authoritative, already in metres, and available without decoding
    a buffer -- which is what keeps this tool free of a glTF library. A file that omits them is
    rejected rather than guessed at.
    """
    accessors = document.get("accessors", [])
    low = [float("inf")] * 3
    high = [float("-inf")] * 3
    found = False
    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            index = primitive.get("attributes", {}).get("POSITION")
            if index is None or index >= len(accessors):
                continue
            accessor = accessors[index]
            a_min, a_max = accessor.get("min"), accessor.get("max")
            if not (isinstance(a_min, list) and isinstance(a_max, list)):
                continue
            if len(a_min) < 3 or len(a_max) < 3:
                continue
            found = True
            for axis in range(3):
                low[axis] = min(low[axis], float(a_min[axis]))
                high[axis] = max(high[axis], float(a_max[axis]))
    return (low, high) if found else None


AXIS_INDEX = {"x": 0, "y": 1, "z": 2}


def check(path: Path, category: str) -> list[str]:
    if category in UNSIZED_CATEGORIES:
        return []
    rules = CATEGORIES.get(category)
    if rules is None:
        return [
            f"category '{category}' has no entry in §70.5's table and is not listed as unsized. "
            f"Add it to CATEGORIES or to UNSIZED_CATEGORIES with a reason -- an unknown category "
            f"must not mean 'no check'."
        ]

    document, error = gltf_validate.read_gltf_json(path)
    if error is not None:
        return [error]
    assert document is not None

    bounds = accessor_bounds(document)
    if bounds is None:
        return [
            "no POSITION accessor declares min/max, which glTF requires. The size cannot be "
            "checked without decoding the buffers, and a guess is worse than a refusal."
        ]
    low, high = bounds
    size = [high[i] - low[i] for i in range(3)]

    problems: list[str] = []
    for axis, minimum, maximum, what in rules:
        value = size[AXIS_INDEX[axis]]
        if not (minimum <= value <= maximum):
            # The message names the RATIO, because that is what identifies the mistake: 100x is
            # centimetres, 2.54x is inches, 0.01x is a model authored in a scene scaled down.
            ratio = value / maximum if value > maximum else value / minimum
            problems.append(
                f"{what}: {value:.3f} m is outside {minimum:.2f}-{maximum:.2f} m "
                f"({ratio:.2f}x the nearest bound -- 100x is centimetres, 2.54x is inches)"
            )
    return problems


def selftest() -> int:
    """A synthetic glTF at the right size, and the same one in centimetres."""
    import tempfile

    def make(width: float, height: float, depth: float) -> bytes:
        """A door-shaped box. NOT a cube: the first version of this fixture was a cube, and the
        width rule correctly rejected a 2.04 m one -- the checker was right and the fixture was
        wrong, which is exactly the confusion a selftest is supposed to remove rather than create.
        """
        payload = json.dumps(
            {
                "asset": {"version": "2.0"},
                "accessors": [
                    {
                        "type": "VEC3",
                        "componentType": 5126,
                        "count": 8,
                        "min": [-width / 2.0, 0.0, -depth / 2.0],
                        "max": [width / 2.0, height, depth / 2.0],
                    }
                ],
                "meshes": [{"name": "m", "primitives": [{"attributes": {"POSITION": 0}}]}],
            }
        ).encode("utf-8")
        payload += b" " * ((4 - len(payload) % 4) % 4)
        header = struct.pack("<III", gltf_validate.GLB_MAGIC, 2, 12 + 8 + len(payload))
        return header + struct.pack("<II", len(payload), gltf_validate.CHUNK_JSON) + payload

    failures = 0
    with tempfile.TemporaryDirectory(prefix="scale-selftest-", dir=str(REPO / "build")) as work:
        good = Path(work) / "door_ok.glb"
        good.write_bytes(make(0.85, 2.04, 0.05))
        if check(good, "door-interior"):
            print(f"  SELFTEST FAILED: a 2.04 m door was rejected: {check(good, 'door-interior')}",
                  file=sys.stderr)
            failures += 1
        else:
            print("  a 2.04 m door passes")

        # The classic: authored in centimetres, so a hundred times too large.
        centimetres = Path(work) / "door_cm.glb"
        centimetres.write_bytes(make(85.0, 204.0, 5.0))
        problems = check(centimetres, "door-interior")
        # Asserting on the MEASURED value, not on the hint text. The first version looked for
        # "100" and matched the constant advice string "100x is centimetres" rather than anything
        # the checker had computed -- a test that passes on its own error message.
        if not problems or not any("204.000 m" in p for p in problems):
            print(f"  SELFTEST FAILED: a 204 m door was not caught as centimetres: {problems}",
                  file=sys.stderr)
            failures += 1
        else:
            print(f"  caught centimetres: {problems[0]}")

        # And one just outside the tolerance, which is the case a loose check would miss.
        near = Path(work) / "door_near.glb"
        near.write_bytes(make(0.85, 1.90, 0.05))
        if not check(near, "door-interior"):
            print("  SELFTEST FAILED: a 1.90 m door was accepted", file=sys.stderr)
            failures += 1
        else:
            print("  a 1.90 m door is rejected, so the tolerance is a tolerance")

        # An unknown category must be an ERROR, never a silent skip.
        if not check(good, "spaceship"):
            print("  SELFTEST FAILED: an unknown category was silently skipped", file=sys.stderr)
            failures += 1
        else:
            print("  an unknown category is an error, not a skip")

    if failures:
        return 1
    print("scale_check: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="*", type=Path)
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--list-categories", action="store_true")
    args = parser.parse_args()

    if args.list_categories:
        for name, rules in sorted(CATEGORIES.items()):
            for axis, low, high, what in rules:
                print(f"  {name:20s} {axis}  {low:.2f}-{high:.2f} m   {what}")
        for name, reason in sorted(UNSIZED_CATEGORIES.items()):
            print(f"  {name:20s} -  unsized -- {reason}")
        return 0

    if args.selftest:
        return selftest()

    document = manifest_tool.load()
    by_file = {row.get("sourceFile", ""): row for row in document.get("assets", [])}

    if args.paths:
        targets = [(p, by_file.get(p.relative_to(REPO).as_posix() if p.is_relative_to(REPO)
                                   else str(p))) for p in args.paths]
    else:
        targets = [
            (REPO / relative, row)
            for relative, row in sorted(by_file.items())
            if relative.lower().endswith((".glb", ".gltf"))
        ]

    if not targets:
        print("scale_check: no models with a manifest row.")
        return 0

    total = 0
    for path, row in targets:
        relative = path.relative_to(REPO) if path.is_relative_to(REPO) else path
        if row is None:
            print(f"scale_check: {relative} has no manifest row, so its category is unknown",
                  file=sys.stderr)
            total += 1
            continue
        category = row.get("category", "")
        problems = check(path, category)
        if problems:
            print(f"scale_check: {relative} (category '{category}')", file=sys.stderr)
            for problem in problems:
                print(f"  {problem}", file=sys.stderr)
            total += len(problems)
        else:
            note = " -- unsized" if category in UNSIZED_CATEGORIES else ""
            print(f"scale_check: {relative} OK (category '{category}'){note}")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
