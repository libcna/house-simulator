#!/usr/bin/env python3
"""build_nav.py -- the pet waypoint graph, generated where it can be and authored where it must be.

`HOUSE-00211`. `cna-house.md` §60.3 specifies the graph: "nodes at every doorway centre, every room
centre, and 2-6 additional nodes per room chosen to avoid furniture; edges between mutually visible
nodes within a cell and across portals". §61 adds the cat's 22 perches. ADR-0011 rejects
Recast/Detour for it -- a 300-node graph with A* is about 150 lines -- so this is the whole of
navigation authoring, and `content/world/nav.bin` is what it writes.

    tools/world/build_nav.py --world assets-src/world --out content/world/nav.bin
    tools/world/build_nav.py --world assets-src/world --report
    tools/world/build_nav.py --selftest

## Why this depends on `HOUSE-00210`

"Mutually visible" and "chosen to avoid furniture" are questions about the *collision* world, not
about the layout: a room centre is a point in a box until you know the sofa is in it. So this tool
builds the collision world in process, through `build_collision.build`, rather than reading
`collision.bin` from disk. One source of truth, and no way to generate a graph against a stale file
that the shipped collision no longer matches.

## The edge test is a CLEARANCE test, not a ray

This is the decision the whole tool turns on. A ray from one node to another passes through a 10 cm
gap between the sofa and the wall, and the graph then contains an edge a dog physically cannot
take: §60.3 says the dog "is a capsule (r 0.22, h 0.60) and is genuinely collided", so the steering
jams against the sofa while the planner insists the route is fine. What is tested here is therefore
whether the animal's own radius **fits** all the way along the segment -- the distance from the
segment to the nearest static shape must exceed the species' radius.

Two species, two radii, and therefore two graphs sharing one node set. The dog's capsule is §60.3's,
verbatim. The cat has no capsule in the architecture, only a size -- 0.25 m at the shoulder, 0.46 m
body (§61) -- so its radius is derived from that and recorded here rather than invented silently:
**r 0.11, h 0.25**. The consequence is real and is the point of doing it per species: gaps the dog
cannot use, the cat can.

## What is generated and what is authored

Generated, because 300 nodes is not a thing to type: the room-centre node, the doorway node at each
portal's centre, and up to 6 clearance-tested nodes per cell chosen by **farthest-point sampling**
over the candidates that pass -- the same idiom `nox_select.py` uses, and for the same reason: the
first six of a scanline are six nodes in a row along one wall.

Authored, because no rule produces them: the 22 perches, the beds and bowls, the forbidden zones,
and any node an author adds by hand. `layout.nav.json`'s rows are merged in and never overwritten;
a generated node that lands within `MERGE_RADIUS` of an authored one is dropped, because the author
put theirs there on purpose.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import build_collision as bc  # noqa: E402
import layout_io  # noqa: E402
from layout_io import LayoutError  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

MAGIC = b"CNAV"
VERSION = 1

#: Species, in bit order. The masks are `u8` in the file, so there is room for a third animal
#: without a version bump.
SPECIES = ["dog", "cat"]
DOG, CAT = 1, 2

#: `(radius, height)` in metres. The dog is §60.3 verbatim. The cat is derived from §61's stated
#: size -- 0.25 m at the shoulder, 0.46 m body -- because the architecture gives the cat parameters
#: and clips but never a capsule. Deriving it here, once, in the open, beats three subsystems each
#: guessing a different one.
CAPSULE = {"dog": (0.22, 0.60), "cat": (0.11, 0.25)}

#: Node kinds.
KIND_ROOM, KIND_DOORWAY, KIND_OPEN, KIND_PERCH, KIND_BED, KIND_BOWL = range(6)
KIND_NAMES = ["room", "doorway", "open", "perch", "bed", "bowl"]

#: At most this many generated open nodes per cell (§60.3's "2-6 additional nodes per room").
MAX_OPEN_NODES = 6
MIN_OPEN_NODES = 2

#: Candidate open nodes are taken from a grid this fine inside each cell box.
CANDIDATE_SPACING = 0.75

#: A generated node closer than this to an existing one is dropped as a duplicate.
MERGE_RADIUS = 0.60

#: How far apart the clearance test samples a segment. A sampled test can only miss an obstacle
#: thinner than its spacing; the thinnest static shape in this world is a 0.15 m partition
#: (`layout.levels.json`'s `wallPartition`), so 0.05 m has a threefold margin. `selftest` asserts
#: the margin rather than trusting this comment.
SAMPLE_SPACING = 0.05

#: How finely a triangle mesh's clearance is sampled along the animal's axis. OBBs are answered
#: exactly and need none of this; meshes are the small minority (§49.2) and are sampled.
MESH_HEIGHT_SAMPLES = 5


def capsule_axis(species: str) -> tuple[float, float]:
    """The animal's capsule AXIS above its feet, which is not its full height.

    A capsule of radius `r` and total height `h` is the set of points within `r` of a segment
    running from `r` to `h - r` -- the hemispherical caps make up the rest. Testing the segment
    `0 … h` instead and then comparing against `r` inflates the animal by `r` at both ends: the dog
    would be refused a doorway it walks under and stopped by a rug. The distinction is worth the
    two lines because every clearance decision in this file is `distance > r`, and `distance` has
    to be measured from the right thing for that comparison to mean what it says.
    """
    radius, height = CAPSULE[species]
    if height <= 2 * radius:
        return height / 2, height / 2
    return radius, height - radius


# ==================================================================== distance to the static world


def _axis_obb_distance(x, z, y_low, y_high, record) -> float:
    """Distance from a **vertical segment** `y_low…y_high` at `(x, z)` to an OBB, 0 if they meet.

    The animal is a capsule, not a point, and this is the difference between the two. A point
    probe has to pick a height: at its feet it misses the wall-mounted shelf, at its chest it sails
    over the 0.25 m bench that a dog plainly cannot walk through. Testing the capsule's whole axis
    has no height to pick and no such gap.

    It is also **exact and closed-form**, not sampled, which sampling the axis would not be: an
    OBB from `build_collision` is yawed about Y only, so undoing the yaw leaves the query segment
    still vertical, and the distance separates into a 2-D clamp in x/z and a 1-D interval gap in y.
    """
    (cx, cy, cz), (hx, hy, hz), yaw, _surface, _kind = record
    dx, dz = x - cx, z - cz
    if abs(yaw) > 1e-9:
        c, s = math.cos(yaw), math.sin(yaw)
        # The inverse of build_collision's placement rotation.
        dx, dz = dx * c - dz * s, dx * s + dz * c
    ex = max(abs(dx) - hx, 0.0)
    ez = max(abs(dz) - hz, 0.0)
    ey = max((cy - hy) - y_high, y_low - (cy + hy), 0.0)
    return math.sqrt(ex * ex + ey * ey + ez * ez)


def _point_triangle_distance(point, a, b, c) -> float:
    """Distance from a point to a triangle, by projection with a clamp to the triangle's region."""
    ax, ay, az = a
    ab = (b[0] - ax, b[1] - ay, b[2] - az)
    ac = (c[0] - ax, c[1] - ay, c[2] - az)
    ap = (point[0] - ax, point[1] - ay, point[2] - az)
    d1 = sum(ab[i] * ap[i] for i in range(3))
    d2 = sum(ac[i] * ap[i] for i in range(3))
    if d1 <= 0 and d2 <= 0:
        return math.dist(point, a)
    bp = (point[0] - b[0], point[1] - b[1], point[2] - b[2])
    d3 = sum(ab[i] * bp[i] for i in range(3))
    d4 = sum(ac[i] * bp[i] for i in range(3))
    if d3 >= 0 and d4 <= d3:
        return math.dist(point, b)
    cp = (point[0] - c[0], point[1] - c[1], point[2] - c[2])
    d5 = sum(ab[i] * cp[i] for i in range(3))
    d6 = sum(ac[i] * cp[i] for i in range(3))
    if d6 >= 0 and d5 <= d6:
        return math.dist(point, c)
    vc = d1 * d4 - d3 * d2
    if vc <= 0 and d1 >= 0 and d3 <= 0:
        t = d1 / (d1 - d3) if d1 != d3 else 0.0
        return math.dist(point, tuple(a[i] + ab[i] * t for i in range(3)))
    vb = d5 * d2 - d1 * d6
    if vb <= 0 and d2 >= 0 and d6 <= 0:
        t = d2 / (d2 - d6) if d2 != d6 else 0.0
        return math.dist(point, tuple(a[i] + ac[i] * t for i in range(3)))
    va = d3 * d6 - d5 * d4
    if va <= 0 and (d4 - d3) >= 0 and (d5 - d6) >= 0:
        t = (d4 - d3) / ((d4 - d3) + (d5 - d6))
        return math.dist(point, tuple(b[i] + (c[i] - b[i]) * t for i in range(3)))
    denom = va + vb + vc
    v, w = vb / denom, vc / denom
    return math.dist(point, tuple(a[i] + ab[i] * v + ac[i] * w for i in range(3)))


class World:
    """The static collision world, answering one question: does the animal fit here?

    Blocking shapes only. A floor is not an obstacle to something standing on it and a ceiling is
    not an obstacle to something 0.6 m tall, so both are excluded; including them would measure
    every animal's clearance against the ground it stands on and nothing would ever pass.

    Queries are the animal's **capsule axis** -- a vertical segment from its feet to the top of its
    head -- and never a point. See `_axis_obb_distance`.
    """

    BLOCKING = {bc.KIND_WALL, bc.KIND_PROP, bc.KIND_EXTERIOR}

    def __init__(self, built: dict) -> None:
        shapes: bc.Shapes = built["shapes"]
        self.obbs = [o for o in shapes.obbs if o[4] in self.BLOCKING]
        self.meshes = [m for m in shapes.meshes if m["kind"] in self.BLOCKING]
        self.cells = {c["id"]: c for c in built["cells"]}

    def clearance(self, foot, species: str) -> float:
        """How far the nearest blocking shape is from `species`' capsule axis standing at `foot`."""
        x, y, z = foot
        low, high = capsule_axis(species)
        y_low, y_high = y + low, y + high
        best = float("inf")
        for record in self.obbs:
            best = min(best, _axis_obb_distance(x, z, y_low, y_high, record))
            if best == 0.0:
                return 0.0
        for mesh in self.meshes:
            vertices = mesh["vertices"]
            for i in range(MESH_HEIGHT_SAMPLES):
                point = (x, y_low + (y_high - y_low) * i / (MESH_HEIGHT_SAMPLES - 1), z)
                for a, b, c in mesh["triangles"]:
                    best = min(best, _point_triangle_distance(
                        point, vertices[a], vertices[b], vertices[c]))
                    if best == 0.0:
                        return 0.0
        return best

    def segment_clearance(self, start, end, species: str) -> float:
        """The worst clearance along a horizontal walk, sampled at `SAMPLE_SPACING`."""
        length = math.dist(start, end)
        steps = max(1, int(math.ceil(length / SAMPLE_SPACING)))
        worst = float("inf")
        for i in range(steps + 1):
            t = i / steps
            foot = tuple(start[k] + (end[k] - start[k]) * t for k in range(3))
            worst = min(worst, self.clearance(foot, species))
            if worst == 0.0:
                break
        return worst


# ============================================================================== node generation


def _cell_floor(cell: dict, levels: dict) -> float:
    return layout_io.cell_extent(cell, levels[cell["level"]])[0]


#: How far above the floor a portal's bottom edge may sit and still be walked through. A pet does
#: not climb: a sill is not a route.
SILL_TOLERANCE = 0.05


def aperture_mask(portal: dict, floor: float) -> int:
    """Which species can walk through this portal, from its rectangle alone.

    Not every portal is a route, and the test is **geometry, not `kind`**. A window's rectangle
    starts 0.9 m up, and a node placed at floor level under it is not merely useless -- it is
    inside the sill, which is a wall piece, so it fails its own clearance test and takes the two
    cells' subgraphs apart. Geometry rather than `kind` because the same three numbers decide it
    for a cased opening, a door, a slider and a garage door, and because a `kind` list is a list
    someone has to remember to extend.

    Three clauses: the bottom edge is at the floor, the opening is taller than the animal, and it
    is wider than the animal. Width is the portal's own, not the clearance around it -- the jambs
    are half a wall thickness away by construction and would reject everything.
    """
    rect = portal["rect"]
    v0, v1 = float(rect["v"][0]), float(rect["v"][1])
    width = abs(float(rect["u"][1]) - float(rect["u"][0]))
    if v0 > floor + SILL_TOLERANCE:
        return 0
    mask = 0
    for bit, name in enumerate(SPECIES):
        radius, height = CAPSULE[name]
        if width > 2 * radius and (v1 - v0) > height:
            mask |= 1 << bit
    return mask


def generate_nodes(layout, world: World, stats: dict):
    """Room centres, doorway centres and the clearance-tested open nodes, plus authored rows."""
    levels = layout_io.by_id(layout_io.rows(layout, "levels"), "level")
    cells = layout_io.rows(layout, "cells")
    cells_by_id = layout_io.by_id(cells, "cell")
    nodes: list[dict] = []

    def species_mask(point) -> int:
        mask = 0
        for bit, name in enumerate(SPECIES):
            if world.clearance(point, name) > CAPSULE[name][0]:
                mask |= 1 << bit
        return mask

    # Authored nodes first, so a generated one never displaces one an author placed.
    nav = layout.get("nav", {})
    for row in nav.get("nodes", []):
        position = tuple(float(c) for c in row["position"])
        nodes.append({"id": row["id"], "cell": row["cell"], "position": position,
                      "kind": KIND_ROOM if row.get("kind") == "floor" else KIND_OPEN,
                      "species": species_mask(position), "authored": True})
    for kind, key, id_field in ((KIND_PERCH, "perches", "id"), (KIND_BED, "beds", "id"),
                                (KIND_BOWL, "bowls", "id")):
        for row in nav.get(key, []):
            if "position" not in row:
                # A bed or bowl may name a prop instead of a position (`world-format.md`). The
                # prop's placement is the position; resolving it needs layout.props.json.
                prop = layout_io.by_id(layout_io.rows(layout, "props"), "prop").get(row.get("prop"))
                if prop is None:
                    raise LayoutError(
                        f"{key[:-1]} {row.get(id_field)!r} names neither a position nor a prop "
                        f"that exists")
                position = tuple(float(c) for c in prop["position"])
                cell = prop["cell"]
            else:
                position = tuple(float(c) for c in row["position"])
                cell = row["cell"]
            mask = 0
            for bit, name in enumerate(SPECIES):
                if name in row.get("species", SPECIES):
                    mask |= 1 << bit
            nodes.append({"id": row[id_field], "cell": cell, "position": position,
                          "kind": kind, "species": mask, "authored": True})

    def too_close(position) -> bool:
        return any(math.dist(position, n["position"]) < MERGE_RADIUS for n in nodes)

    # Doorway nodes, one per portal an animal can actually walk through, at the rectangle's
    # centre on its own plane.
    for portal in sorted(layout_io.rows(layout, "portals"), key=lambda p: p["id"]):
        plane, rect = portal["plane"], portal["rect"]
        value = float(plane["value"])
        u = (float(rect["u"][0]) + float(rect["u"][1])) / 2
        cell = cells_by_id.get(portal["cellA"])
        if cell is None:
            raise LayoutError(f"portal {portal['id']!r} names cell {portal['cellA']!r}, "
                              f"which does not exist")
        floor = _cell_floor(cell, levels)
        mask = aperture_mask(portal, floor)
        if not mask:
            stats["portalsNotARoute"].append(portal["id"])
            continue
        position = (value, floor, u) if plane["axis"] == "x" else (u, floor, value)
        if too_close(position):
            stats["doorwayMerged"] += 1
            continue
        nodes.append({"id": f"NAV_{portal['id']}", "cell": portal["cellA"], "position": position,
                      "kind": KIND_DOORWAY, "species": mask, "authored": False,
                      "portal": portal["id"], "cellB": portal["cellB"]})
        stats["doorwayNodes"] += 1

    # Room centres and open nodes.
    for cell in sorted(cells, key=lambda c: c["id"]):
        if cell.get("kind") == "void":
            continue
        floor = _cell_floor(cell, levels)
        boxes = layout_io.cell_boxes(cell)
        candidates = []
        for x0, x1, z0, z1 in boxes:
            nx = max(1, int((x1 - x0) / CANDIDATE_SPACING))
            nz = max(1, int((z1 - z0) / CANDIDATE_SPACING))
            for i in range(nx):
                for j in range(nz):
                    candidates.append((x0 + (i + 0.5) * (x1 - x0) / nx, floor,
                                       z0 + (j + 0.5) * (z1 - z0) / nz))
        passable = []
        for point in candidates:
            mask = species_mask(point)
            if mask:
                passable.append((point, mask))
        if not passable:
            stats["cellsWithNoNode"].append(cell["id"])
            continue

        # The room centre is the passable candidate nearest the cell's own centroid, not the
        # centroid itself: a centroid inside the sofa is not somewhere an animal can stand, and an
        # L-shaped room's centroid can be outside the room entirely.
        area = sum((x1 - x0) * (z1 - z0) for x0, x1, z0, z1 in boxes)
        cx = sum((x0 + x1) / 2 * (x1 - x0) * (z1 - z0) for x0, x1, z0, z1 in boxes) / area
        cz = sum((z0 + z1) / 2 * (x1 - x0) * (z1 - z0) for x0, x1, z0, z1 in boxes) / area
        centre, centre_mask = min(passable, key=lambda p: math.dist(p[0], (cx, floor, cz)))
        chosen = [(centre, centre_mask, KIND_ROOM)]

        # Farthest-point sampling for the rest: the first six of a scanline are six nodes in a row
        # along one wall, which is the one layout that adds no connectivity.
        remaining = [p for p in passable if p[0] != centre]
        while len(chosen) < MAX_OPEN_NODES and remaining:
            best = max(remaining,
                       key=lambda p: (min(math.dist(p[0], c[0]) for c in chosen), p[0]))
            if min(math.dist(best[0], c[0]) for c in chosen) < CANDIDATE_SPACING:
                break
            chosen.append((best[0], best[1], KIND_OPEN))
            remaining.remove(best)

        for index, (position, mask, kind) in enumerate(chosen):
            if too_close(position):
                stats["openMerged"] += 1
                continue
            suffix = "C" if kind == KIND_ROOM else str(index)
            nodes.append({"id": f"NAV_{cell['id']}_{suffix}", "cell": cell["id"],
                          "position": position, "kind": kind, "species": mask,
                          "authored": False})
        if len([n for n in nodes if n["cell"] == cell["id"]]) < MIN_OPEN_NODES:
            stats["cellsBelowMinimum"].append(cell["id"])

    # Forbidden zones remove a species from every node in a cell. A cat that may not enter the
    # garage must not have a node there for A* to aim at.
    for zone in nav.get("forbidden", []):
        for bit, name in enumerate(SPECIES):
            if name in zone.get("species", []):
                for node in nodes:
                    if node["cell"] == zone["cell"]:
                        node["species"] &= ~(1 << bit)
    return nodes


# ================================================================================ edge generation


def generate_edges(nodes, layout, world: World, stats: dict):
    """Edges within a cell where the animal fits, and across every portal through its doorway."""
    portals = layout_io.by_id(layout_io.rows(layout, "portals"), "portal")
    by_cell: dict[str, list[int]] = {}
    for index, node in enumerate(nodes):
        by_cell.setdefault(node["cell"], []).append(index)

    edges = []
    tested = 0
    for cell_id in sorted(by_cell):
        members = by_cell[cell_id]
        for a_at in range(len(members)):
            for b_at in range(a_at + 1, len(members)):
                a, b = members[a_at], members[b_at]
                shared = nodes[a]["species"] & nodes[b]["species"]
                if not shared:
                    continue
                mask = 0
                for bit, name in enumerate(SPECIES):
                    if not shared & (1 << bit):
                        continue
                    tested += 1
                    if world.segment_clearance(nodes[a]["position"], nodes[b]["position"],
                                               name) > CAPSULE[name][0]:
                        mask |= 1 << bit
                if mask:
                    edges.append({"a": a, "b": b, "portal": None, "species": mask,
                                  "cost": math.dist(nodes[a]["position"], nodes[b]["position"])})

    # Across portals: the doorway node belongs to cellA, so it needs an edge into cellB. It joins
    # the nearest node there that its own mask allows, which is what "across portals" means -- a
    # room-centre-to-room-centre edge would cut the corner through the wall.
    for index, node in enumerate(nodes):
        if node["kind"] != KIND_DOORWAY:
            continue
        portal = portals[node["portal"]]
        others = [i for i in by_cell.get(node["cellB"], []) if nodes[i]["species"] & node["species"]]
        # The NEAREST node in the far cell is not necessarily one the animal can reach from the
        # doorway: a node just around the corner is close in metres and behind a wall. So the
        # candidates are tried in distance order and the first that the animal actually fits
        # through wins, exactly as inside a cell. Taking the nearest unconditionally is how a
        # planner ends up routing a dog into a doorjamb.
        chosen = None
        for candidate in sorted(others,
                                key=lambda i: (math.dist(nodes[i]["position"], node["position"]),
                                               nodes[i]["id"])):
            mask = 0
            for bit, name in enumerate(SPECIES):
                if not (node["species"] & nodes[candidate]["species"] & (1 << bit)):
                    continue
                stats["clearanceTests"] += 1
                if world.segment_clearance(node["position"], nodes[candidate]["position"],
                                           name) > CAPSULE[name][0]:
                    mask |= 1 << bit
            if mask:
                chosen = (candidate, mask)
                break
        if chosen is None:
            stats["portalsWithNoTarget"].append(portal["id"])
            continue
        target, mask = chosen
        edges.append({"a": index, "b": target, "portal": portal["id"], "species": mask,
                      "cost": math.dist(node["position"], nodes[target]["position"])})

    stats["clearanceTests"] = tested
    return edges


def components(nodes, edges, bit: int) -> list[set[int]]:
    """Connected components of one species' subgraph -- what the reachability report is built on."""
    adjacency: dict[int, set[int]] = {
        i: set() for i, n in enumerate(nodes) if n["species"] & (1 << bit)}
    for edge in edges:
        if edge["species"] & (1 << bit) and edge["a"] in adjacency and edge["b"] in adjacency:
            adjacency[edge["a"]].add(edge["b"])
            adjacency[edge["b"]].add(edge["a"])
    seen: set[int] = set()
    out = []
    for start in sorted(adjacency):
        if start in seen:
            continue
        stack, group = [start], set()
        while stack:
            current = stack.pop()
            if current in group:
                continue
            group.add(current)
            stack.extend(adjacency[current] - group)
        seen |= group
        out.append(group)
    return out


# ========================================================================================== build


def build(world_dir: Path, manifest_path: Path | None = None) -> dict:
    built = bc.build(world_dir, manifest_path)
    layout = layout_io.load_layout(world_dir, ["levels", "cells", "portals"])
    for optional in ("nav", "props", "stairs", "materials"):
        name, _ = layout_io.FILES[optional]
        if (world_dir / name).is_file():
            layout[optional] = layout_io.load_file(world_dir / name, optional)

    world = World(built)
    stats = {"doorwayNodes": 0, "doorwayMerged": 0, "openMerged": 0, "clearanceTests": 0,
             "cellsWithNoNode": [], "cellsBelowMinimum": [], "portalsWithNoTarget": [],
             "portalsNotARoute": []}
    nodes = generate_nodes(layout, world, stats)
    edges = generate_edges(nodes, layout, world, stats)

    reach = {}
    for bit, name in enumerate(SPECIES):
        groups = components(nodes, edges, bit)
        reach[name] = {
            "nodes": sum(1 for n in nodes if n["species"] & (1 << bit)),
            "edges": sum(1 for e in edges if e["species"] & (1 << bit)),
            "components": len(groups),
            "largest": max((len(g) for g in groups), default=0),
        }
    stats["reach"] = reach
    return {"nodes": nodes, "edges": edges, "stats": stats,
            "worldHash": built["worldHash"], "collision": built}


# ========================================================================================= writer


def serialise(graph: dict) -> bytes:
    """`nav.bin`, per `docs/nav-format.md`. Adjacency is NOT written: it is derivable from the
    edges, and a second description of the same thing can disagree with the first."""
    cells = sorted({n["cell"] for n in graph["nodes"]})
    cell_index = {name: i for i, name in enumerate(cells)}
    if len(cells) > 0xFFFF:
        raise LayoutError(f"{len(cells)} cells exceeds the format's u16 cell index")

    out = bytearray()
    out += MAGIC
    out += struct.pack("<II", VERSION, 0)
    out += bc._string(graph["worldHash"])

    out += struct.pack("<I", len(SPECIES))
    for name in SPECIES:
        out += bc._string(name)
        out += struct.pack("<2f", *CAPSULE[name])

    out += struct.pack("<I", len(cells))
    for name in cells:
        out += bc._string(name)

    out += struct.pack("<I", len(graph["nodes"]))
    for node in graph["nodes"]:
        out += bc._string(node["id"])
        out += struct.pack("<H", cell_index[node["cell"]])
        out += struct.pack("<3f", *node["position"])
        out += struct.pack("<BB", node["kind"], node["species"])

    out += struct.pack("<I", len(graph["edges"]))
    for edge in graph["edges"]:
        out += struct.pack("<IIf", edge["a"], edge["b"], edge["cost"])
        out += struct.pack("<B", edge["species"])
        out += bc._string(edge["portal"] or "")
    return bytes(out)


def read_back(data: bytes) -> dict:
    view = memoryview(data)
    at = 0

    def take(n):
        nonlocal at
        chunk = view[at:at + n]
        if len(chunk) != n:
            raise LayoutError(f"nav.bin truncated at byte {at}, wanted {n} more")
        at += n
        return chunk

    def unpack(fmt):
        return struct.unpack(fmt, take(struct.calcsize(fmt)))

    def text():
        (length,) = unpack("<H")
        return bytes(take(length)).decode("utf-8")

    if bytes(take(4)) != MAGIC:
        raise LayoutError("not a nav.bin: bad magic")
    version, flags = unpack("<II")
    if version != VERSION:
        raise LayoutError(f"nav.bin is version {version}; this reader knows {VERSION}")
    if flags:
        raise LayoutError(f"nav.bin sets unknown flag bits {flags:#x}")
    world_hash = text()

    (species_count,) = unpack("<I")
    species = []
    for _ in range(species_count):
        name = text()
        radius, height = unpack("<2f")
        species.append({"name": name, "radius": radius, "height": height})

    (cell_count,) = unpack("<I")
    cells = [text() for _ in range(cell_count)]

    (node_count,) = unpack("<I")
    nodes = []
    for _ in range(node_count):
        node_id = text()
        (cell,) = unpack("<H")
        position = unpack("<3f")
        kind, mask = unpack("<BB")
        nodes.append({"id": node_id, "cell": cells[cell], "position": position,
                      "kind": KIND_NAMES[kind], "species": mask})

    (edge_count,) = unpack("<I")
    edges = []
    for _ in range(edge_count):
        a, b, cost = unpack("<IIf")
        (mask,) = unpack("<B")
        portal = text()
        edges.append({"a": a, "b": b, "cost": cost, "species": mask, "portal": portal or None})

    if at != len(data):
        raise LayoutError(f"{len(data) - at} bytes left over after the last edge")
    return {"worldHash": world_hash, "species": species, "cells": cells,
            "nodes": nodes, "edges": edges}


def report(graph: dict) -> str:
    stats = graph["stats"]
    kinds = {name: 0 for name in KIND_NAMES}
    for node in graph["nodes"]:
        kinds[KIND_NAMES[node["kind"]]] += 1
    lines = [
        f"{len(graph['nodes'])} nodes, {len(graph['edges'])} edges",
        "  by kind: " + ", ".join(f"{k} {v}" for k, v in kinds.items() if v),
        f"  {stats['clearanceTests']} clearance tests; "
        f"{stats['doorwayMerged']} doorway and {stats['openMerged']} open nodes merged into "
        f"authored ones",
    ]
    for name in SPECIES:
        entry = stats["reach"][name]
        lines.append(f"  {name:<4} {entry['nodes']:>4} nodes, {entry['edges']:>4} edges, "
                     f"{entry['components']} component(s), largest {entry['largest']}")
    for cell in stats["cellsWithNoNode"]:
        lines.append(f"  no node fits in {cell}")
    for portal in stats["portalsWithNoTarget"]:
        lines.append(f"  portal {portal} leads to a cell with no reachable node")
    if stats["portalsNotARoute"]:
        lines.append(f"  {len(stats['portalsNotARoute'])} portal(s) are not a route for either "
                     f"species (a sill, or too small): "
                     f"{', '.join(stats['portalsNotARoute'][:4])}")
    return "\n".join(lines)


# ======================================================================================= selftest


def selftest() -> int:
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("build_nav: selftest")
    workspace = Path(tempfile.mkdtemp(prefix="build_nav_selftest_"))
    try:
        world_dir = workspace / "world"
        world_dir.mkdir()
        bc._fixture_world(world_dir)
        # `build_collision`'s fixture has no route into the garage, because collision does not care
        # whether a room can be reached. Navigation does, so one door is added here rather than
        # changing a fixture whose counts that tool's own claims are written against.
        portals = json.loads((world_dir / "layout.portals.json").read_text())
        portals["portals"].append({
            "id": "P_L0_HALL__L0_GARAGE", "cellA": "L0_HALL", "cellB": "L0_GARAGE",
            "plane": {"axis": "x", "value": 6.0},
            "rect": {"u": [1.0, 1.9], "v": [0.0, 2.04]},
            "kind": "door", "opacity": "opaque_when_closed"})
        (world_dir / "layout.portals.json").write_text(
            json.dumps(portals, indent=2) + "\n", encoding="utf-8")

        # Furniture, for the same reason: collision does not care whether a room is walkable, so
        # `build_collision`'s fixture is empty. "2-6 additional nodes per room chosen to avoid
        # furniture" (§60.3) cannot be tested in an empty room -- every candidate passes and the
        # arithmetic centroid is as good as any measured point. The sofa sits ON the lounge's
        # centroid (1.75, 2.125) precisely so that the centroid is the one place a node may not go.
        assets_dir = workspace / "assets"
        assets_dir.mkdir()
        bc._fixture_proxy(assets_dir / "sofa.glb", [((0.0, 0.4, 0.0), (0.9, 0.4, 0.45))])
        (world_dir / "layout.props.json").write_text(json.dumps({
            "schema": "cna-house/props/1",
            "props": [{"id": "PROP_SOFA", "asset": "MODEL_SOFA", "cell": "L0_LOUNGE",
                       "position": [1.75, 0.0, 2.125], "yawDeg": 0.0, "scale": 1.0,
                       "static": True, "collision": "proxy"}]}, indent=2) + "\n",
            encoding="utf-8")
        manifest = workspace / "assets.manifest.json"
        manifest.write_text(json.dumps({
            "schema": "cna-house/assets/1",
            "assets": [{"id": "MODEL_SOFA", "sourceFile": str(assets_dir / "sofa.glb")}]},
            indent=2) + "\n", encoding="utf-8")

        # 1. Point-to-shape distance, against answers arithmetic gives.
        box = ((0.0, 0.0, 0.0), (1.0, 1.0, 1.0), 0.0, 0, bc.KIND_PROP)
        require(abs(_axis_obb_distance(3.0, 0.0, -1.0, 1.0, box) - 2.0) < 1e-9,
                "a capsule axis 2 m to the side of a box measures 2 m")
        require(_axis_obb_distance(0.5, 0.5, 0.0, 1.0, box) == 0.0,
                "an axis passing through a box measures 0")
        require(abs(_axis_obb_distance(0.0, 0.0, 4.0, 5.0, box) - 3.0) < 1e-9,
                "an axis entirely above a box measures the vertical gap, 3 m")
        require(_axis_obb_distance(0.0, 0.0, -5.0, 5.0, box) == 0.0,
                "...and an axis spanning it measures 0, which a point at either end would not")
        turned = ((0.0, 0.0, 0.0), (2.0, 1.0, 0.5), math.pi / 2, 0, bc.KIND_PROP)
        require(abs(_axis_obb_distance(0.0, 3.0, -1.0, 1.0, turned) - 1.0) < 1e-6,
                "a yawed box is measured in its own frame: the long axis is now along z, so an "
                "axis at z=3 is 1 m away, not 2.5")
        tri = ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0))
        require(abs(_point_triangle_distance((0.25, 2.0, 0.25), *tri) - 2.0) < 1e-9,
                "a point above a triangle's interior measures its height")
        require(abs(_point_triangle_distance((5.0, 0.0, 0.0), *tri) - 4.0) < 1e-9,
                "a point beyond an edge measures to the nearest vertex")

        def only(*records):
            shapes = bc.Shapes()
            for centre, half, kind in records:
                shapes.obb(centre, half, 0.0, None, kind)
            return World({"shapes": shapes, "cells": [], "worldHash": ""})

        # 2. The clearance test is the whole point of the tool: a gap a ray passes and a dog does
        #    not. Two blocks 0.30 m apart -- a ray goes straight through, a 0.22 m radius does not.
        narrow = only(((0.0, 0.5, -0.65), (2.0, 0.5, 0.5), bc.KIND_PROP),
                      ((0.0, 0.5, +0.65), (2.0, 0.5, 0.5), bc.KIND_PROP))
        walk = ((-3.0, 0.0, 0.0), (3.0, 0.0, 0.0))
        require(abs(narrow.segment_clearance(*walk, "dog") - 0.15) < 1e-6,
                "a 0.30 m gap measures 0.15 m of clearance down its centre line")
        require(narrow.segment_clearance(*walk, "dog") < CAPSULE["dog"][0],
                "...so the dog (r 0.22) is refused the edge")
        require(narrow.segment_clearance(*walk, "cat") > CAPSULE["cat"][0],
                "...and the cat (r 0.11) is allowed it -- two species, two graphs, one node set")

        # 2b. The query is the animal's whole capsule, not a probe at one height. A 0.25 m bench
        #     stops a dog; a point probe at its chest sails over it and a point probe at its feet
        #     would miss a shelf instead. There is no height to pick that is right for both.
        bench = only(((0.0, 0.125, 0.0), (2.0, 0.125, 0.4), bc.KIND_PROP))
        require(bench.segment_clearance(*walk, "dog") <= CAPSULE["dog"][0],
                "a 0.25 m bench stops the dog: its capsule axis starts 0.22 m up and runs into it")
        shelf = only(((0.0, 0.65, 0.0), (2.0, 0.15, 0.4), bc.KIND_PROP))
        require(shelf.segment_clearance(*walk, "dog") <= CAPSULE["dog"][0],
                "a shelf 0.50-0.80 m up also stops the dog, whose 0.60 m head does not clear it")
        require(shelf.segment_clearance(*walk, "cat") > CAPSULE["cat"][0],
                "...and does not stop the cat, 0.25 m tall, which walks under it")
        require(capsule_axis("dog") == (0.22, 0.38) and capsule_axis("cat") == (0.11, 0.14),
                f"the axis runs from r to h-r, not 0 to h: dog {capsule_axis('dog')}, cat "
                f"{capsule_axis('cat')} -- testing 0..h and comparing to r inflates the animal "
                f"by its own radius at each end")

        # 2c. A floor is not an obstacle to something standing on it, and neither is the ceiling.
        #     Counting them would give every node in the house zero clearance.
        slabs = only(((0.0, -0.15, 0.0), (5.0, 0.15, 5.0), bc.KIND_FLOOR),
                     ((0.0, 2.65, 0.0), (5.0, 0.15, 5.0), bc.KIND_CEILING))
        require(slabs.segment_clearance(*walk, "dog") == float("inf"),
                "the floor underfoot and the ceiling overhead are not obstacles")

        # 3. The sampling margin. A sampled walk can only miss an obstacle thinner than its
        #    spacing, and the thinnest static shape in this world is a 0.15 m partition. The
        #    obstacle is placed OFF the midpoint on purpose: a two-sample walk happens to hit a
        #    centred one, which is how a coarse sampler passes a test written the obvious way.
        thin = only(((-1.5, 0.5, 0.0), (0.075, 0.5, 2.0), bc.KIND_WALL))
        require(thin.segment_clearance(*walk, "dog") == 0.0,
                f"a 0.15 m partition a quarter of the way along -- the thinnest shape the layout "
                f"can produce -- is found by {SAMPLE_SPACING} m sampling, with a threefold margin")

        # 3b. Triangle meshes block too. Nothing generated so far is one, so it is built by hand:
        #     a mesh obstacle the dog cannot pass and the cat can walk under.
        mesh_only = bc.Shapes()
        mesh_only.mesh([(-0.2, 0.30, -1.0), (0.2, 0.30, -1.0), (0.2, 0.30, 1.0),
                        (-0.2, 0.30, 1.0), (0.0, 0.90, 0.0)],
                       [(0, 1, 2), (0, 2, 3), (0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)],
                       None, bc.KIND_PROP)
        pyramid = World({"shapes": mesh_only, "cells": [], "worldHash": ""})
        require(pyramid.segment_clearance(*walk, "dog") == 0.0,
                "a triangle mesh is an obstacle: the dog's 0.60 m capsule meets it")
        require(pyramid.segment_clearance(*walk, "cat") > CAPSULE["cat"][0],
                "...and the cat, 0.25 m tall, passes under its 0.30 m underside")

        graph = build(world_dir, manifest)

        # 4. A doorway node per portal that is a ROUTE, and not one for the window. This is the
        #    failure that cost the most: a node at floor level under a 0.9 m sill is inside the
        #    sill wall piece, so it fails its own clearance test and silently cuts the two cells'
        #    subgraphs apart -- a disconnected graph, reported as a navigation bug three phases
        #    later, whose cause is a node nobody asked for.
        doorways = [n for n in graph["nodes"] if n["kind"] == KIND_DOORWAY]
        require(len(doorways) == 2,
                f"two doorway nodes for the two doors (got {len(doorways)})")
        require(graph["stats"]["portalsNotARoute"] == ["P_L0_HALL__WINDOW"],
                f"the window is reported as not a route, by name, rather than silently skipped "
                f"({graph['stats']['portalsNotARoute']})")
        require(all(abs(n["position"][0] - 4.0) < 1e-6 or abs(n["position"][0] - 6.0) < 1e-6
                    for n in doorways),
                "...and each sits on the plane its portal declares")

        # 5. The three clauses of the aperture rule, each on its own.
        floor = 0.0
        door = {"rect": {"u": [1.0, 1.9], "v": [0.0, 2.04]}}
        require(aperture_mask(door, floor) == DOG | CAT, "a 0.9 x 2.04 m door admits both")
        require(aperture_mask({"rect": {"u": [1.0, 1.9], "v": [0.9, 2.1]}}, floor) == 0,
                "a sill 0.9 m up admits neither -- a pet does not climb through a window")
        require(aperture_mask({"rect": {"u": [1.0, 1.3], "v": [0.0, 2.0]}}, floor) == CAT,
                "a 0.30 m gap admits the cat (r 0.11) and not the dog (r 0.22): width > 2r")
        require(aperture_mask({"rect": {"u": [1.0, 1.9], "v": [0.0, 0.4]}}, floor) == CAT,
                "a 0.40 m high hatch admits the cat (h 0.25) and not the dog (h 0.60)")

        # 6. Room centres avoid furniture rather than being the arithmetic centroid. The L-shaped
        #    lounge's centroid is at x 1.4, z 2.1; a node there is fine, but the node must be a
        #    candidate that PASSED, and passing is what the assertion is about.
        room = [n for n in graph["nodes"] if n["kind"] == KIND_ROOM]
        require(len(room) == 3, f"one room-centre node per cell (got {len(room)})")
        for node in room:
            clear = World(graph["collision"]).clearance(node["position"], "dog")
            require(clear > CAPSULE["dog"][0],
                    f"{node['id']} stands where a dog fits ({clear:.3f} m clear)")
        lounge_centre = next(n for n in room if n["cell"] == "L0_LOUNGE")
        require(math.dist(lounge_centre["position"], (1.75, 0.0, 2.125)) > 0.5,
                f"the lounge's room node is NOT its arithmetic centroid, because the sofa is "
                f"there: it moved to {tuple(round(c, 2) for c in lounge_centre['position'])}")

        # 7. Open nodes are spread, not queued along one wall. Farthest-point sampling is what
        #    makes the graph connect the room rather than trace its edge.
        lounge_open = [n for n in graph["nodes"]
                       if n["cell"] == "L0_LOUNGE" and n["kind"] == KIND_OPEN]
        require(2 <= len(lounge_open) + 1 <= MAX_OPEN_NODES,
                f"the lounge gets between {MIN_OPEN_NODES} and {MAX_OPEN_NODES} nodes "
                f"(got {len(lounge_open) + 1})")
        if len(lounge_open) >= 2:
            separation = min(math.dist(a["position"], b["position"])
                             for a in lounge_open for b in lounge_open
                             if a is not b)
            require(separation > 2 * CANDIDATE_SPACING,
                    f"...and the CLOSEST two are {separation:.2f} m apart, more than the "
                    f"{CANDIDATE_SPACING} m candidate grid: farthest-point sampling maximises the "
                    f"minimum separation, where taking the first few of the scan would leave "
                    f"neighbours one grid step apart")

        # 8. No edge crosses a wall. This is the claim the whole clearance test exists to make.
        world = World(graph["collision"])
        crossing = []
        for edge in graph["edges"]:
            a, b = graph["nodes"][edge["a"]], graph["nodes"][edge["b"]]
            for bit, name in enumerate(SPECIES):
                if not edge["species"] & (1 << bit):
                    continue
                if world.segment_clearance(a["position"], b["position"],
                                           name) <= CAPSULE[name][0]:
                    crossing.append((a["id"], b["id"], name))
        require(not crossing,
                f"no edge asks an animal to walk where it does not fit ({crossing[:3]})")

        # 9. Two rooms are joined through the doorway, never directly. A room-centre-to-room-centre
        #    edge would cut the corner through the wall, and the clearance test rejects it -- but
        #    the graph must still be CONNECTED, which is what makes the doorway edge necessary.
        direct = [e for e in graph["edges"]
                  if graph["nodes"][e["a"]]["cell"] != graph["nodes"][e["b"]]["cell"]]
        require(all(e["portal"] for e in direct),
                "every edge between two cells names the portal it crosses")
        for name, bit in (("dog", 0), ("cat", 1)):
            groups = components(graph["nodes"], graph["edges"], bit)
            require(len(groups) == 1,
                    f"the {name}'s graph is connected: {len(groups)} component(s) over "
                    f"{sum(len(g) for g in groups)} nodes")

        # 10. Authored rows win, and forbidden zones remove a species. The authored node is placed
        #     exactly where generation just put one, so the merge is provoked rather than hoped
        #     for -- an earlier version of this claim read a global counter that a completely
        #     unrelated merge elsewhere in the house happened to satisfy.
        collides_with = next(n for n in graph["nodes"]
                             if n["cell"] == "L0_LOUNGE" and n["kind"] == KIND_OPEN)
        (world_dir / "layout.nav.json").write_text(json.dumps({
            "schema": "cna-house/nav/1",
            "nodes": [{"id": "NAV_AUTHORED", "cell": "L0_LOUNGE",
                       "position": list(collides_with["position"]), "kind": "floor"}],
            "edges": [],
            "perches": [{"id": "PERCH_SILL", "cell": "L0_LOUNGE",
                         "position": [0.5, 1.1, 2.5], "species": ["cat"]}],
            "beds": [], "bowls": [],
            "forbidden": [{"cell": "L0_GARAGE", "species": ["cat"]}],
        }, indent=2) + "\n", encoding="utf-8")
        graph = build(world_dir, manifest)
        require(any(n["id"] == "NAV_AUTHORED" for n in graph["nodes"]),
                "an authored node survives generation")
        require(graph["stats"]["openMerged"] >= 1,
                f"...and the generated node that would have landed on top of it is dropped "
                f"({graph['stats']['openMerged']} merged)")
        here = [n for n in graph["nodes"]
                if math.dist(n["position"], collides_with["position"]) < MERGE_RADIUS]
        require([n["id"] for n in here] == ["NAV_AUTHORED"],
                f"...leaving exactly one node there, the authored one "
                f"({[n['id'] for n in here]})")
        # The invariant behind the counter: no two nodes anywhere are closer than the merge radius.
        close = [(a["id"], b["id"]) for i, a in enumerate(graph["nodes"])
                 for b in graph["nodes"][i + 1:]
                 if math.dist(a["position"], b["position"]) < MERGE_RADIUS
                 and not (a["authored"] and b["authored"])]
        require(not close, f"no generated node sits within {MERGE_RADIUS} m of any other "
                           f"({close[:3]})")
        perch = next(n for n in graph["nodes"] if n["id"] == "PERCH_SILL")
        require(perch["species"] == CAT and perch["kind"] == KIND_PERCH,
                "a perch is the cat's alone -- §61's addition, and the dog has no jump_up")
        garage = [n for n in graph["nodes"] if n["cell"] == "L0_GARAGE"]
        require(garage and all(not (n["species"] & CAT) for n in garage),
                f"a forbidden zone removes the cat from every node in the garage "
                f"({len(garage)} nodes)")
        require(any(n["species"] & DOG for n in garage),
                "...and leaves the dog's alone")

        # 11. Round trip and determinism.
        data = serialise(graph)
        back = read_back(data)
        require(len(back["nodes"]) == len(graph["nodes"])
                and len(back["edges"]) == len(graph["edges"]),
                "the file reads back with the same node and edge counts")
        require([n["id"] for n in back["nodes"]] == [n["id"] for n in graph["nodes"]],
                "node ids and their order survive")
        require(all(a["cell"] == b["cell"] for a, b in zip(back["nodes"], graph["nodes"])),
                "every node's cell round trips through the cell table")
        require(all(abs(a["cost"] - b["cost"]) < 1e-5 and a["species"] == b["species"]
                    and a["portal"] == b["portal"]
                    for a, b in zip(back["edges"], graph["edges"])),
                "every edge's cost, species mask and portal round trip")
        require(back["species"][1]["name"] == "cat"
                and abs(back["species"][1]["radius"] - 0.11) < 1e-6,
                "the capsule table travels with the graph, so the runtime does not re-derive it")
        require(serialise(build(world_dir, manifest)) == data,
                "two builds of one layout produce byte-identical output")

        # 12. Truncation, version and trailing bytes.
        for cut in (3, 12, len(data) - 1):
            try:
                read_back(data[:cut])
                caught = False
            except LayoutError:
                caught = True
            require(caught, f"a file truncated to {cut} bytes is refused")
        try:
            read_back(data + b"\0")
            caught = False
        except LayoutError as exc:
            caught = "left over" in str(exc)
        require(caught, "trailing bytes are refused")
        bumped = bytearray(data)
        bumped[4:8] = struct.pack("<I", 7)
        try:
            read_back(bytes(bumped))
            caught = False
        except LayoutError as exc:
            caught = "7" in str(exc)
        require(caught, "an unknown version is refused, naming the number found")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("build_nav: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--world", type=Path, default=REPO / "assets-src" / "world")
    parser.add_argument("--manifest", type=Path,
                        default=REPO / "assets-src" / "assets.manifest.json")
    parser.add_argument("--out", type=Path, default=REPO / "content" / "world" / "nav.bin")
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    try:
        graph = build(args.world, args.manifest)
    except LayoutError as exc:
        print(f"build_nav: {exc}", file=sys.stderr)
        return 1

    print(report(graph))
    if args.report:
        return 0

    data = serialise(graph)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(data)
    print(f"build_nav: wrote {args.out} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
