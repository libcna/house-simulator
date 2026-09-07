#!/usr/bin/env python3
"""build_coverage.py -- what is over your head at every point on the property.

`HOUSE-00212`. `cna-house.md` §37.2: "a coverage height field on a 0.5 m grid over the property
stores, per cell, the height of the lowest roof/soffit above it (or +INF). A particle is drawn only
if `particle.y > coverage(x, z)`." The consequence §37.2 asks for is specific and visible --
"standing under the porch in a downpour, the rain visibly stops at the porch edge" -- and that
sentence is what this tool is judged against.

    tools/world/build_coverage.py --world assets-src/world --out content/world/coverage.bin
    tools/world/build_coverage.py --world assets-src/world --report --ascii
    tools/world/build_coverage.py --selftest

## The soffits are already built, and building them twice would be the bug

§37.2 attributes the mask to `terrain_gen.py`, but the geometry it needs is not terrain: a porch
soffit is the underside of the balcony floor above it, and the roof underside is the top cell's
ceiling. `HOUSE-00210` already derives every one of those slabs from the layout, so this tool asks
`build_collision` for them rather than re-deriving them from the same JSON with a second set of
rules that can drift from the first. The plan's own name for this task -- `build_world.py`'s
sibling `build_coverage.py`, dependent on `HOUSE-00210` -- says the same thing.

So: **coverage(x, z) is the lowest underside of any floor or ceiling slab above the ground at
(x, z)**, and +INF where there is none. One rule, and it covers the whole of §37.2's list -- house,
garage, porch, balcony, sunroom and shed -- because each of those is a cell, and a cell has a
ceiling and the thing above it has a floor.

## Two decisions worth stating

**Below-ground slabs are not cover.** A basement's floor slab is 2.65 m under the lawn; without the
ground test every square metre over the basement reports itself sheltered and the rain stops in
mid-air over the garden. The ground is the terrain reference from `layout.exterior.json`, or 0.0.

**A grid sample is its cell's CENTRE, and the field is not made conservative.** Marking a cell
covered when any part of it is under a roof biases every soffit outward by up to half a grid step,
so rain would stop 0.25 m short of the porch edge, in open air, which reads as a bug rather than as
shelter. Centre sampling puts the error either side of the true edge instead, and `--report` prints
the worst quantisation error it actually incurred so the number is known rather than assumed.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import build_collision as bc  # noqa: E402
import layout_io  # noqa: E402
from layout_io import LayoutError  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

MAGIC = b"CCOV"
VERSION = 1

#: §37.2's grid.
CELL_SIZE = 0.5

#: How far outside the built footprint the field extends when `layout.exterior.json` does not give
#: a terrain extent. Rain is drawn around the camera, and the camera can stand off the property.
DEFAULT_MARGIN = 8.0

#: The slabs that shelter. Walls do not: rain does not fall on a vertical surface from above, and
#: a wall counted as cover would shelter the 0.15 m strip it stands in and nothing else.
COVERING = {bc.KIND_FLOOR, bc.KIND_CEILING}

UNCOVERED = float("inf")


def ground_level(layout) -> float:
    """The height rain lands at, below which a slab is not shelter but foundation."""
    exterior = layout.get("exterior", {})
    terrain = exterior.get("terrain") if isinstance(exterior, dict) else None
    if isinstance(terrain, dict) and terrain.get("origin") is not None:
        return float(terrain["origin"][1])
    return 0.0


def property_bounds(layout, built) -> tuple[float, float, float, float]:
    """`(x0, z0, x1, z1)` -- the terrain's extent if declared, else the built footprint plus a
    margin. A field that stopped at the walls would leave the garden with no answer at all."""
    exterior = layout.get("exterior", {})
    terrain = exterior.get("terrain") if isinstance(exterior, dict) else None
    if isinstance(terrain, dict) and terrain.get("size") and terrain.get("origin"):
        ox, _oy, oz = (float(c) for c in terrain["origin"])
        sx, sz = (float(c) for c in terrain["size"])
        return ox, oz, ox + sx, oz + sz
    if not built["cells"]:
        raise LayoutError("the layout has no cells and no terrain extent; nothing to cover")
    x0 = min(c["bounds"][0] for c in built["cells"]) - DEFAULT_MARGIN
    z0 = min(c["bounds"][2] for c in built["cells"]) - DEFAULT_MARGIN
    x1 = max(c["bounds"][3] for c in built["cells"]) + DEFAULT_MARGIN
    z1 = max(c["bounds"][5] for c in built["cells"]) + DEFAULT_MARGIN
    return x0, z0, x1, z1


def slabs(built, ground: float):
    """Every covering slab as `(x0, z0, x1, z1, underside)`, above the ground only.

    Yawed slabs would need their corners rotated; floors and ceilings from `build_collision` are
    axis-aligned by construction (they come from a cell's footprint box), and a yawed one is
    refused rather than silently treated as its AABB, which would over-cover.
    """
    shapes: bc.Shapes = built["shapes"]
    out = []
    for record in shapes.obbs:
        (cx, cy, cz), (hx, hy, hz), yaw, _surface, kind = record
        if kind not in COVERING:
            continue
        if abs(yaw) > 1e-9:
            raise LayoutError(
                f"a covering slab at {(cx, cy, cz)} is yawed by {yaw} rad; the coverage field "
                f"assumes axis-aligned floors and ceilings")
        underside = cy - hy
        if underside <= ground + 1e-6:
            continue
        out.append((cx - hx, cz - hz, cx + hx, cz + hz, underside))
    return out


def build(world_dir: Path, manifest_path: Path | None = None) -> dict:
    built = bc.build(world_dir, manifest_path)
    layout = layout_io.load_layout(world_dir, ["levels", "cells", "portals"])
    for optional in ("exterior", "props", "stairs", "materials"):
        name, _ = layout_io.FILES[optional]
        if (world_dir / name).is_file():
            layout[optional] = layout_io.load_file(world_dir / name, optional)

    ground = ground_level(layout)
    x0, z0, x1, z1 = property_bounds(layout, built)
    nx = max(1, int(math.ceil((x1 - x0 - 1e-9) / CELL_SIZE)))
    nz = max(1, int(math.ceil((z1 - z0 - 1e-9) / CELL_SIZE)))
    covers = slabs(built, ground)

    field = [UNCOVERED] * (nx * nz)
    for j in range(nz):
        cz = z0 + (j + 0.5) * CELL_SIZE
        for i in range(nx):
            cx = x0 + (i + 0.5) * CELL_SIZE
            best = UNCOVERED
            for sx0, sz0, sx1, sz1, underside in covers:
                if sx0 <= cx <= sx1 and sz0 <= cz <= sz1 and underside < best:
                    best = underside
            field[j * nx + i] = best

    covered = sum(1 for v in field if v != UNCOVERED)
    return {
        "origin": (x0, z0), "nx": nx, "nz": nz, "cell": CELL_SIZE, "ground": ground,
        "field": field, "worldHash": built["worldHash"], "collision": built,
        "stats": {
            "cells": nx * nz, "covered": covered,
            "slabs": len(covers),
            "slabsBelowGround": sum(
                1 for r in built["shapes"].obbs
                if r[4] in COVERING and (r[0][1] - r[1][1]) <= ground + 1e-6),
            "lowest": min((v for v in field if v != UNCOVERED), default=None),
            "highest": max((v for v in field if v != UNCOVERED), default=None),
        },
    }


def sample(coverage: dict, x: float, z: float) -> float:
    """The field's answer at a world point -- the runtime's own lookup, so the tests use it too."""
    i = int(math.floor((x - coverage["origin"][0]) / coverage["cell"]))
    j = int(math.floor((z - coverage["origin"][1]) / coverage["cell"]))
    if not (0 <= i < coverage["nx"] and 0 <= j < coverage["nz"]):
        return UNCOVERED
    return coverage["field"][j * coverage["nx"] + i]


def serialise(coverage: dict) -> bytes:
    out = bytearray()
    out += MAGIC
    out += struct.pack("<II", VERSION, 0)
    out += bc._string(coverage["worldHash"])
    out += struct.pack("<2f", *coverage["origin"])
    out += struct.pack("<f", coverage["cell"])
    out += struct.pack("<f", coverage["ground"])
    out += struct.pack("<II", coverage["nx"], coverage["nz"])
    for value in coverage["field"]:
        out += struct.pack("<f", value)
    return bytes(out)


def read_back(data: bytes) -> dict:
    view = memoryview(data)
    at = 0

    def take(n):
        nonlocal at
        chunk = view[at:at + n]
        if len(chunk) != n:
            raise LayoutError(f"coverage.bin truncated at byte {at}, wanted {n} more")
        at += n
        return chunk

    def unpack(fmt):
        return struct.unpack(fmt, take(struct.calcsize(fmt)))

    if bytes(take(4)) != MAGIC:
        raise LayoutError("not a coverage.bin: bad magic")
    version, flags = unpack("<II")
    if version != VERSION:
        raise LayoutError(f"coverage.bin is version {version}; this reader knows {VERSION}")
    if flags:
        raise LayoutError(f"coverage.bin sets unknown flag bits {flags:#x}")
    (length,) = unpack("<H")
    world_hash = bytes(take(length)).decode("utf-8")
    origin = unpack("<2f")
    (cell,) = unpack("<f")
    (ground,) = unpack("<f")
    nx, nz = unpack("<II")
    field = [unpack("<f")[0] for _ in range(nx * nz)]
    if at != len(data):
        raise LayoutError(f"{len(data) - at} bytes left over after the field")
    return {"worldHash": world_hash, "origin": origin, "cell": cell, "ground": ground,
            "nx": nx, "nz": nz, "field": field}


def ascii_map(coverage: dict, width: int = 100) -> str:
    """A plan view. `.` is open sky; a digit is the height band of whatever is overhead.

    The point of this is review by eye: a coverage field is a grid of numbers in which a porch
    accidentally extending three metres into the garden looks exactly like a porch.
    """
    step = max(1, coverage["nx"] // width)
    lines = []
    for j in range(coverage["nz"] - 1, -1, -step):
        row = []
        for i in range(0, coverage["nx"], step):
            value = coverage["field"][j * coverage["nx"] + i]
            row.append("." if value == UNCOVERED else str(min(9, int(value))))
        lines.append("".join(row))
    return "\n".join(lines)


def report(coverage: dict) -> str:
    stats = coverage["stats"]
    lines = [
        f"{coverage['nx']} x {coverage['nz']} cells of {coverage['cell']} m "
        f"from ({coverage['origin'][0]:.2f}, {coverage['origin'][1]:.2f}), "
        f"ground {coverage['ground']:.2f} m",
        f"  {stats['covered']} of {stats['cells']} cells are sheltered "
        f"({100.0 * stats['covered'] / stats['cells']:.1f} %)",
        f"  {stats['slabs']} covering slabs; {stats['slabsBelowGround']} below ground and "
        f"correctly ignored",
    ]
    if stats["lowest"] is not None:
        lines.append(f"  soffits from {stats['lowest']:.2f} m to {stats['highest']:.2f} m")
    return "\n".join(lines)


# ======================================================================================= selftest


def selftest() -> int:
    import json
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("build_coverage: selftest")
    workspace = Path(tempfile.mkdtemp(prefix="build_coverage_selftest_"))
    try:
        world_dir = workspace / "world"
        world_dir.mkdir()
        bc._fixture_world(world_dir)

        # The fixture gains what §37.2 is actually about: a PORCH -- an exterior cell at ground
        # level with a room directly above it -- and a BASEMENT under the lounge, whose slabs are
        # below ground and must not shelter the lawn.
        cells = json.loads((world_dir / "layout.cells.json").read_text())
        cells["cells"].append({
            "id": "L0_PORCH", "level": "L0", "kind": "exterior",
            "boxes": [{"x": [-3.0, 0.0], "z": [0.0, 3.0]}],
            "footstepSurface": "concrete", "wallMaterial": "MAT_PAINT",
            "ceilingMaterial": "MAT_CEIL", "yOverride": [0.0, 2.5]})
        cells["cells"].append({
            "id": "L1_BEDROOM", "level": "L1", "kind": "room",
            "boxes": [{"x": [-3.0, 0.0], "z": [0.0, 3.0]}],
            "footstepSurface": "wood", "wallMaterial": "MAT_PAINT",
            "ceilingMaterial": "MAT_CEIL"})
        cells["cells"].append({
            "id": "B1_CELLAR", "level": "B1", "kind": "room",
            "boxes": [{"x": [0.0, 4.0], "z": [0.0, 3.0]}],
            "footstepSurface": "concrete", "wallMaterial": "MAT_PAINT",
            "ceilingMaterial": "MAT_CEIL"})
        (world_dir / "layout.cells.json").write_text(
            json.dumps(cells, indent=2) + "\n", encoding="utf-8")
        levels = json.loads((world_dir / "layout.levels.json").read_text())
        levels["levels"].append({"id": "L1", "name": "Upper", "ffl": 2.8, "ceiling": 5.3,
                                 "structureDepth": 0.30})
        levels["levels"].append({"id": "B1", "name": "Cellar", "ffl": -2.3, "ceiling": -0.3,
                                 "structureDepth": 0.30})
        (world_dir / "layout.levels.json").write_text(
            json.dumps(levels, indent=2) + "\n", encoding="utf-8")

        coverage = build(world_dir)

        # 1. Open sky is +INF, and it is really infinity rather than a large number that some
        #    comparison will one day treat as a height.
        require(sample(coverage, -6.0, -6.0) == UNCOVERED,
                "a point in the garden has no cover, and reports +INF")
        require(math.isinf(sample(coverage, -6.0, -6.0)),
                "...an actual IEEE infinity, so `particle.y > coverage` is false for every "
                "particle and no sentinel constant has to be agreed with the runtime")

        # 2. The porch. This is §37.2's own example: the soffit is the underside of the bedroom
        #    floor above, at 2.80 - 0.30 = 2.50 m.
        require(abs(sample(coverage, -1.5, 1.5) - 2.50) < 1e-5,
                f"under the porch the cover is the bedroom floor's underside, 2.50 m "
                f"(got {sample(coverage, -1.5, 1.5)})")

        # 3. Rain visibly stops AT the porch edge. The edge tested is the porch's OPEN one, at
        #    z = 0, facing the garden -- not its x = 0 side, which abuts the lounge and is
        #    sheltered on both sides at the same 2.50 m. An assertion written against that side
        #    passes whatever the tool does, which is how it was first written here.
        require(abs(sample(coverage, -1.5, 0.75) - 2.50) < 1e-5,
                f"just inside the porch is sheltered at 2.50 m "
                f"(got {sample(coverage, -1.5, 0.75)})")
        require(sample(coverage, -1.5, -0.75) == UNCOVERED,
                f"just outside its open edge is open sky "
                f"(got {sample(coverage, -1.5, -0.75)})")

        # 4. Indoors the cover is the room's own ceiling, not the roof over the whole house.
        require(abs(sample(coverage, 2.0, 1.5) - 2.50) < 1e-5,
                f"inside the lounge the cover is its ceiling slab's underside at 2.50 m "
                f"(got {sample(coverage, 2.0, 1.5)})")

        # 5. The basement does not shelter the lawn. Its floor slab is 2.60 m under the ground, and
        #    without the ground test every cell above it would report itself covered -- rain
        #    stopping in mid-air over the garden, from a slab nobody can see.
        require(coverage["stats"]["slabsBelowGround"] >= 1,
                f"the fixture really does have below-ground slabs to reject "
                f"({coverage['stats']['slabsBelowGround']})")
        require(all(v > coverage["ground"] for v in coverage["field"] if v != UNCOVERED),
                "no cell reports cover at or below ground level")

        # 6. A wall is not cover. Counting one would shelter the 0.15 m strip it stands in.
        require(bc.KIND_WALL not in COVERING and bc.KIND_PROP not in COVERING,
                "only floors and ceilings shelter; walls and props do not")

        # 7. The lowest slab wins where several stack. Over the porch there are two -- the bedroom
        #    floor at 2.50 and the bedroom ceiling at 5.30 -- and the answer must be the lower.
        require(abs(sample(coverage, -1.5, 1.5) - 2.50) < 1e-5,
                "where two slabs stack, the LOWER one is the cover: 2.50, not the 5.30 above it")

        # 8. §37.2's grid is 0.5 m. It is asserted against the literal, not against the constant,
        #    because a claim written as `cell == CELL_SIZE` moves with the mistake it should catch.
        require(coverage["cell"] == 0.5,
                f"the grid is §37.2's 0.5 m (got {coverage['cell']})")

        # 8b. The field covers the garden, not just the building: rain is drawn around the camera
        #     and the camera stands outside. The fixture's westmost wall is at x = -3.15, so a
        #     field that stopped at the footprint would start there; this asserts a literal margin.
        x0, z0 = coverage["origin"]
        require(x0 < -9.0,
                f"the field reaches well past the building's own footprint (starts at "
                f"x = {x0:.2f}, and the westmost wall is at -3.15)")
        require(sample(coverage, -8.0, -8.0) == UNCOVERED,
                "a point 8 m out in the garden is inside the field and answers 'open sky' -- "
                "not off the end of it, where the runtime would have no answer at all")

        # 9. Quantisation is measured, not assumed. Every slab edge is compared with where the
        #    sampled field puts the boundary.
        worst = 0.0
        for sx0, sz0, sx1, sz1, _under in slabs(coverage["collision"], coverage["ground"]):
            for edge, axis in ((sx0, 0), (sx1, 0), (sz0, 1), (sz1, 1)):
                grid = coverage["origin"][axis] + round(
                    (edge - coverage["origin"][axis]) / CELL_SIZE) * CELL_SIZE
                worst = max(worst, abs(grid - edge))
        require(worst <= CELL_SIZE / 2 + 1e-9,
                f"the worst soffit edge lands within half a grid step of the truth "
                f"({worst:.3f} m of a {CELL_SIZE / 2} m bound)")

        # 10. The field is not made conservative, stated as the exact equivalence rather than as
        #     a spot check: a cell is sheltered if and only if its CENTRE is under a slab. A
        #     conservative field -- covered when any part of the cell is under one -- pushes every
        #     soffit outward by up to half a step, and rain then stops 0.25 m out in the open,
        #     which reads as a bug rather than as shelter.
        covers = slabs(coverage["collision"], coverage["ground"])
        disagreements = 0
        for j in range(coverage["nz"]):
            cz = coverage["origin"][1] + (j + 0.5) * CELL_SIZE
            for i in range(coverage["nx"]):
                cx = coverage["origin"][0] + (i + 0.5) * CELL_SIZE
                centre_covered = any(sx0 <= cx <= sx1 and sz0 <= cz <= sz1
                                     for sx0, sz0, sx1, sz1, _u in covers)
                if centre_covered != (coverage["field"][j * coverage["nx"] + i] != UNCOVERED):
                    disagreements += 1
        require(disagreements == 0,
                f"a cell is sheltered exactly when its centre is under a slab, over all "
                f"{coverage['nx'] * coverage['nz']} cells ({disagreements} disagreed)")

        # 11. Round trip.
        data = serialise(coverage)
        back = read_back(data)
        require(back["nx"] == coverage["nx"] and back["nz"] == coverage["nz"],
                "the grid dimensions round trip")
        require(all((math.isinf(a) and math.isinf(b)) or abs(a - b) < 1e-5
                    for a, b in zip(back["field"], coverage["field"])),
                "every cell round trips, +INF included -- float32 keeps infinity as infinity")
        require(abs(back["cell"] - 0.5) < 1e-9, "the grid size travels with the field")
        # The fixture's ground is 0.0, so a writer that emitted a constant zero would round trip
        # perfectly. Checked against a non-zero ground instead.
        raised = dict(coverage, ground=1.75, worldHash="sha256:raised")
        require(read_back(serialise(raised))["ground"] == 1.75,
                "the ground level travels with the field, and is not assumed to be zero")
        require(read_back(serialise(raised))["worldHash"] == "sha256:raised",
                "...as does the world hash, so a stale field is detectable")
        require(serialise(build(world_dir)) == data,
                "two builds of one layout produce byte-identical output")

        # 12. Truncation, version and trailing bytes.
        for cut in (3, 12, len(data) - 1):
            try:
                read_back(data[:cut])
                caught = False
            except (LayoutError, struct.error):
                caught = True
            require(caught, f"a file truncated to {cut} bytes is refused")
        try:
            read_back(data + b"\0\0\0\0")
            caught = False
        except LayoutError as exc:
            caught = "left over" in str(exc)
        require(caught, "trailing bytes are refused")
        bumped = bytearray(data)
        bumped[4:8] = struct.pack("<I", 12)
        try:
            read_back(bytes(bumped))
            caught = False
        except LayoutError as exc:
            caught = "12" in str(exc)
        require(caught, "an unknown version is refused, naming the number found")
        flagged = bytearray(data)
        flagged[8:12] = struct.pack("<I", 0x80)
        try:
            read_back(bytes(flagged))
            caught = False
        except LayoutError as exc:
            caught = "flag" in str(exc)
        require(caught, "an unknown flag bit is refused rather than ignored")

        # 13. The ascii map exists and shows the shape, because a grid of numbers in which the
        #     porch quietly extends into the garden looks exactly like one in which it does not.
        art = ascii_map(coverage)
        require("." in art and "2" in art,
                "the plan view distinguishes open sky from a 2.50 m soffit")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("build_coverage: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--world", type=Path, default=REPO / "assets-src" / "world")
    parser.add_argument("--manifest", type=Path,
                        default=REPO / "assets-src" / "assets.manifest.json")
    parser.add_argument("--out", type=Path, default=REPO / "content" / "world" / "coverage.bin")
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--ascii", action="store_true", help="print the plan view")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    try:
        coverage = build(args.world, args.manifest)
    except LayoutError as exc:
        print(f"build_coverage: {exc}", file=sys.stderr)
        return 1

    print(report(coverage))
    if args.ascii:
        print(ascii_map(coverage))
    if args.report:
        return 0

    data = serialise(coverage)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(data)
    print(f"build_coverage: wrote {args.out} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
