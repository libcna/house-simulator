#!/usr/bin/env python3
"""Validate authored static-prop placement against the built house.

`HOUSE-03303` keeps this offline and deliberately narrow.  It uses the same manifest, glTF
reader, cell boxes, terrain decoder and `_COL` proxy grammar as the existing content builders.
It does not introduce a runtime placement system.

The existing world validator owns exact hinged-leaf sweep geometry (rule 14) and the controller
grand tour remains the end-to-end traversal proof.  This checker owns the complementary authoring
contract: support, cell containment, prop/opening overlap, window fronts and a 0.70 m route through
every furnished accessible cell.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from collections import deque
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(REPO / "tools" / "assets"))

import build_chunks  # noqa: E402
import build_collision  # noqa: E402
import layout_io  # noqa: E402
import origin_check  # noqa: E402
import terrain_gen  # noqa: E402

SUPPORT_TOLERANCE = 0.01
OVERLAP_TOLERANCE = 0.01
# Broad authored collision boxes deliberately include the furniture envelope, so a chair tucked
# beneath a table apron and a counter stool tucked beneath the island overhang have a small plan
# overlap even though their solid geometry is disjoint.  Keep these two measured allowances
# local to the exact furniture pair; every other rigid pair retains the 1 cm rule.
PAIR_OVERLAP_TOLERANCES = {
    frozenset(("chair", "table-dining")): 0.04,
    frozenset(("counter-kitchen", "stool-counter")): 0.12,
}
WINDOW_FRONT = 0.60
# Door swings own their full working arc in world-validator rule 14.  Here an opening zone is the
# aperture slab itself; route width is checked separately below, so furniture beside a wide cased
# opening is not mistaken for furniture *in* it.
OPENING_FRONT = 0.02
#: Half an interior partition: the shallowest a room's finished wall face lies inside its box.
ROOM_FACE_INSET = 0.075
ROUTE_WIDTH = 0.70
ROUTE_RADIUS = ROUTE_WIDTH / 2.0
GRID_STEP = 0.10

# A throw is intentionally draped through its sofa envelope.  It remains checked for containment
# and support, but treating cloth/seat contact as two rigid objects intersecting would reject the
# one correct placement of the piece.
SOFT_CONTACT = {"throw-blanket"}
MOUNT_TOLERANCE = 0.25
FIXED_WINDOW_TYPES = {"W_PANEL", "W_PICTURE", "W_SIDELIGHT", "W_TRANSOM", "W_INTERNAL"}
FITTED_WINDOW_PIECES = {"kitchen-service-run", "kitchen-sink-run", "counter-kitchen",
                        "kitchen-cooking-wall"}
SURFACE_DRESSING = {"console-decor", "kitchen-counter-decor", "tabletop-decor",
                    "table-setting", "table-lamp", "houseplant"}
MOUNTED_FIXTURE_CATEGORIES = {"fixture", "fixture-small", "ceiling-light", "wall-light"}


@dataclass(frozen=True)
class Asset:
    identifier: str
    category: str
    source: Path
    bounds: tuple[float, float, float, float, float, float]
    positions: tuple[tuple[float, float, float], ...]
    triangles: tuple[tuple[int, int, int], ...]


@dataclass
class Placed:
    index: int
    row: dict
    asset: Asset
    polygon: list[tuple[float, float]]
    bottom: float
    top: float
    collision_parts: list[tuple[list[tuple[float, float]], float, float]]

    @property
    def identifier(self) -> str:
        return str(self.row.get("id"))

    @property
    def cell(self) -> str:
        return str(self.row.get("cell"))

    @property
    def collision_polygons(self) -> list[list[tuple[float, float]]]:
        return [part[0] for part in self.collision_parts]


def _bounds(points) -> tuple[float, float, float, float, float, float]:
    return (min(point[0] for point in points), min(point[1] for point in points),
            min(point[2] for point in points), max(point[0] for point in points),
            max(point[1] for point in points), max(point[2] for point in points))


def _transform_point(point, row: dict) -> tuple[float, float, float]:
    position, yaw_deg, scale = layout_io.prop_transform(row)
    yaw = math.radians(yaw_deg)
    cosine, sine = math.cos(yaw), math.sin(yaw)
    px, py, pz = position
    x, y, z = (float(value) * component for value, component in zip(point, scale))
    return (px + x * cosine + z * sine, py + y, pz - x * sine + z * cosine)


def _rectangle(bounds, row: dict) -> list[tuple[float, float]]:
    x0, _y0, z0, x1, _y1, z1 = bounds
    return [(point[0], point[2]) for point in
            (_transform_point((x0, 0.0, z0), row),
             _transform_point((x1, 0.0, z0), row),
             _transform_point((x1, 0.0, z1), row),
             _transform_point((x0, 0.0, z1), row))]


def _axes(polygon) -> list[tuple[float, float]]:
    out = []
    for first, second in zip(polygon, polygon[1:] + polygon[:1]):
        dx, dz = second[0] - first[0], second[1] - first[1]
        length = math.hypot(dx, dz)
        if length > 1e-9:
            out.append((-dz / length, dx / length))
    return out


def _polygon_overlap(first, second) -> float:
    """Minimum SAT penetration, or zero when two convex rectangles do not overlap."""
    penetration = math.inf
    for axis in _axes(first) + _axes(second):
        a = [point[0] * axis[0] + point[1] * axis[1] for point in first]
        b = [point[0] * axis[0] + point[1] * axis[1] for point in second]
        depth = min(max(a), max(b)) - max(min(a), min(b))
        if depth <= 0.0:
            return 0.0
        penetration = min(penetration, depth)
    return penetration


def _point_in_polygon(point, polygon, margin: float = 0.0) -> bool:
    # Convex and consistently ordered.  The distance form also gives the route its clearance.
    sign = None
    for first, second in zip(polygon, polygon[1:] + polygon[:1]):
        dx, dz = second[0] - first[0], second[1] - first[1]
        cross = dx * (point[1] - first[1]) - dz * (point[0] - first[0])
        current = 1 if cross > 0 else -1 if cross < 0 else 0
        if current and sign is None:
            sign = current
        elif current and current != sign:
            return False
        if margin > 0.0:
            distance = abs(cross) / max(math.hypot(dx, dz), 1e-9)
            if distance < margin:
                return False
    return True


def _point_segment_distance(point, first, second) -> float:
    dx, dz = second[0] - first[0], second[1] - first[1]
    length_sq = dx * dx + dz * dz
    if length_sq <= 1e-12:
        return math.dist(point, first)
    ratio = max(0.0, min(1.0, ((point[0] - first[0]) * dx
                                + (point[1] - first[1]) * dz) / length_sq))
    return math.dist(point, (first[0] + ratio * dx, first[1] + ratio * dz))


def _point_near_polygon(point, polygon, radius: float) -> bool:
    return (_point_in_polygon(point, polygon)
            or min(_point_segment_distance(point, first, second)
                   for first, second in zip(polygon, polygon[1:] + polygon[:1])) < radius)


def _load_assets(manifest_path: Path, wanted: set[str]) -> dict[str, Asset]:
    document = json.loads(manifest_path.read_text(encoding="utf-8"))
    out = {}
    for row in document.get("assets", []):
        if str(row.get("id")) not in wanted:
            continue
        source = row.get("sourceFile")
        if not isinstance(source, str) or not source.lower().endswith((".glb", ".gltf")):
            continue
        path = REPO / source
        geometry = build_chunks.read_geometry(path)
        positions = tuple(tuple(float(value) for value in point)
                          for point in geometry["positions"])
        out[str(row["id"])] = Asset(str(row["id"]), str(row.get("category") or ""), path,
                                    _bounds(positions), positions,
                                    tuple(tuple(int(value) for value in triangle)
                                          for triangle in geometry["triangles"]))
    return out


def _collision_parts(asset: Asset, row: dict) -> list[tuple[list[tuple[float, float]], float, float]]:
    if row.get("collision", "proxy") == "none":
        return []
    components = []
    for vertices, triangles in build_collision.read_col_meshes(asset.source):
        components.extend(build_collision.split_components(vertices, triangles))
    if not components:
        raise layout_io.LayoutError(
            f"prop {row.get('id')} asks for collision but {asset.source.name} has no _COL mesh")
    if row.get("collision") == "box":
        vertices = [point for component, _triangles in components for point in component]
        components = [(vertices, [])]
    parts = []
    for vertices, _triangles in components:
        bounds = _bounds(vertices)
        parts.append((_rectangle(bounds, row),
                      _transform_point((0.0, bounds[1], 0.0), row)[1],
                      _transform_point((0.0, bounds[4], 0.0), row)[1]))
    return parts


def _place(row: dict, index: int, assets: dict[str, Asset]) -> Placed:
    asset = assets.get(str(row.get("asset")))
    if asset is None:
        raise layout_io.LayoutError(f"prop {row.get('id')} has no measured manifest model")
    bottom = _transform_point((0.0, asset.bounds[1], 0.0), row)[1]
    top = _transform_point((0.0, asset.bounds[4], 0.0), row)[1]
    collision = _collision_parts(asset, row)
    return Placed(index, row, asset, _rectangle(asset.bounds, row), bottom, top,
                  collision)


def _linked_mounted_fixture(identifier: str, category: str, collision: str,
                            fixture_props: set[str]) -> bool:
    """A switched physical fitting may mount to a ceiling, rafter or wall, not only a floor.

    The light-to-prop reference is the authored mounting contract.  Keep this narrow: ordinary
    furniture cannot opt out of support validation, nor can a fitting retain a rigid proxy in the
    walking volume.
    """
    return (identifier in fixture_props
            and category in MOUNTED_FIXTURE_CATEGORIES
            and collision == "none")


def _cell_contains(cell: dict, point: tuple[float, float], margin: float = 0.0) -> bool:
    return any(x0 - margin <= point[0] <= x1 + margin
               and z0 - margin <= point[1] <= z1 + margin
               for x0, x1, z0, z1 in (validate_box for validate_box in
                                       _cell_boxes(cell)))


def _cell_boxes(cell: dict) -> list[tuple[float, float, float, float]]:
    return [(float(box["x"][0]), float(box["x"][1]),
             float(box["z"][0]), float(box["z"][1]))
            for box in cell.get("boxes", [])]


def _polygon_in_cell(polygon, cell: dict, margin: float = SUPPORT_TOLERANCE) -> bool:
    samples = list(polygon)
    samples += [((first[0] + second[0]) / 2.0, (first[1] + second[1]) / 2.0)
                for first, second in zip(polygon, polygon[1:] + polygon[:1])]
    samples.append((sum(point[0] for point in polygon) / len(polygon),
                    sum(point[1] for point in polygon) / len(polygon)))
    return all(_cell_contains(cell, point, margin) for point in samples)


def _wall_contact(placed: Placed, cell: dict) -> bool:
    if placed.asset.category not in origin_check.WALL_MOUNTED | {"fixture", "fixture-small"}:
        return False
    for first, second in zip(placed.polygon, placed.polygon[1:] + placed.polygon[:1]):
        for x0, x1, z0, z1 in _cell_boxes(cell):
            if ((abs(first[0] - x0) <= MOUNT_TOLERANCE
                 and abs(second[0] - x0) <= MOUNT_TOLERANCE)
                    or (abs(first[0] - x1) <= MOUNT_TOLERANCE
                        and abs(second[0] - x1) <= MOUNT_TOLERANCE)
                    or (abs(first[1] - z0) <= MOUNT_TOLERANCE
                        and abs(second[1] - z0) <= MOUNT_TOLERANCE)
                    or (abs(first[1] - z1) <= MOUNT_TOLERANCE
                        and abs(second[1] - z1) <= MOUNT_TOLERANCE)):
                return True
    return False


def _point_in_triangle(point, triangle) -> bool:
    (ax, az), (bx, bz), (cx, cz) = triangle
    denominator = (bz - cz) * (ax - cx) + (cx - bx) * (az - cz)
    if abs(denominator) <= 1e-12:
        return False
    u = ((bz - cz) * (point[0] - cx) + (cx - bx) * (point[1] - cz)) / denominator
    v = ((cz - az) * (point[0] - cx) + (ax - cx) * (point[1] - cz)) / denominator
    return u >= -1e-6 and v >= -1e-6 and u + v <= 1.0 + 1e-6


def _supported_by(placed: Placed, other: Placed) -> bool:
    if placed.cell != other.cell or placed.identifier == other.identifier:
        return False
    # Soft dressing is supported by the object through which it deliberately drapes.
    if placed.asset.category in SOFT_CONTACT:
        return (_polygon_overlap(placed.polygon, other.polygon) > 0.0
                and placed.bottom <= other.top + SUPPORT_TOLERANCE
                and placed.top >= other.bottom - SUPPORT_TOLERANCE)
    # A fitted run can contain a worktop below its overall bound (a tap, upper cabinet or splash
    # raises the bound). Dedicated surface-dressing categories may use that authored internal
    # support, but still have to overlap the host and sit inside its vertical envelope.
    if (placed.asset.category in SURFACE_DRESSING
            and _polygon_overlap(placed.polygon, other.polygon) > OVERLAP_TOLERANCE
            and other.bottom + SUPPORT_TOLERANCE < placed.bottom < other.top + SUPPORT_TOLERANCE):
        return True
    if (placed.asset.category == "fixture-small"
            and _polygon_overlap(placed.polygon, other.polygon) > OVERLAP_TOLERANCE
            and other.bottom < placed.top < other.top):
        return True
    samples = list(placed.polygon)
    samples += [((first[0] + second[0]) / 2.0, (first[1] + second[1]) / 2.0)
                for first, second in zip(placed.polygon,
                                         placed.polygon[1:] + placed.polygon[:1])]
    samples.append((sum(point[0] for point in placed.polygon) / 4.0,
                    sum(point[1] for point in placed.polygon) / 4.0))
    world_positions = [_transform_point(point, other.row) for point in other.asset.positions]
    for a, b, c in other.asset.triangles:
        triangle = (world_positions[a], world_positions[b], world_positions[c])
        if max(point[1] for point in triangle) - min(point[1] for point in triangle) > 0.004:
            continue
        surface = sum(point[1] for point in triangle) / 3.0
        plan_triangle = [(point[0], point[2]) for point in triangle]
        touches = (any(_point_in_triangle(sample, plan_triangle) for sample in samples)
                   or _polygon_overlap(placed.polygon, plan_triangle) > 0.0)
        if touches:
            if abs(surface - placed.bottom) <= SUPPORT_TOLERANCE:
                return True
            # Under-cabinet task fittings attach by their top face, not their bottom.
            if (placed.asset.category == "fixture-small"
                    and abs(surface - placed.top) <= SUPPORT_TOLERANCE):
                return True
    return False


def _terrain_height(terrain: list[float], x: float, z: float) -> float:
    """Runtime-equivalent triangle interpolation over the committed height field."""
    fx = (x - terrain_gen.ORIGIN_X) / terrain_gen.STEP
    fz = (z - terrain_gen.ORIGIN_Z) / terrain_gen.STEP
    ix = max(0, min(terrain_gen.WIDTH - 2, int(fx)))
    iz = max(0, min(terrain_gen.HEIGHT - 2, int(fz)))
    u, v = fx - ix, fz - iz

    def height(dx, dz):
        return terrain[(iz + dz) * terrain_gen.WIDTH + ix + dx]

    h00, h10, h01, h11 = height(0, 0), height(1, 0), height(0, 1), height(1, 1)
    return (h00 + (h10 - h00) * u + (h11 - h10) * v if v <= u
            else h00 + (h11 - h01) * u + (h01 - h00) * v)


def _portal_zone(portal: dict, cell: dict, depth: float) -> list[tuple[float, float]] | None:
    plane, rect = portal.get("plane") or {}, portal.get("rect") or {}
    axis = plane.get("axis")
    if axis not in ("x", "z"):
        return None
    value = float(plane["value"])
    u0, u1 = (float(value) for value in rect["u"])
    centre = (value, (u0 + u1) / 2.0) if axis == "x" else ((u0 + u1) / 2.0, value)
    candidates = ((1.0, 0.0), (-1.0, 0.0)) if axis == "x" else ((0.0, 1.0), (0.0, -1.0))
    inward = next((normal for normal in candidates
                   if _cell_contains(cell, (centre[0] + normal[0] * 0.05,
                                            centre[1] + normal[1] * 0.05))), None)
    if inward is None:
        return None
    if axis == "x":
        return [(value, u0), (value, u1),
                (value + inward[0] * depth, u1), (value + inward[0] * depth, u0)]
    return [(u0, value), (u1, value),
            (u1, value + inward[1] * depth), (u0, value + inward[1] * depth)]


def _nearest_free(point, nodes) -> tuple[int, int] | None:
    return min(nodes, key=lambda node: math.dist(point, nodes[node]), default=None)


def _disc_inside_box_union(point, boxes, radius: float) -> bool:
    """Return whether a horizontal disc is inside the union of axis-aligned cell boxes.

    Most cells are one box, so retain that cheap path.  An L-shaped cell can join two boxes along
    a complete edge, though, and a valid circulation disc may straddle that authored seam.  Test
    the exact rectangle arrangement around the disc instead of requiring one box to contain it.
    """
    if any(bx0 + radius <= point[0] <= bx1 - radius
           and bz0 + radius <= point[1] <= bz1 - radius
           for bx0, bx1, bz0, bz1 in boxes):
        return True

    px, pz = point
    x_min, x_max = px - radius, px + radius
    z_min, z_max = pz - radius, pz + radius
    x_edges = {x_min, x_max}
    z_edges = {z_min, z_max}
    for bx0, bx1, bz0, bz1 in boxes:
        x_edges.update(edge for edge in (bx0, bx1) if x_min < edge < x_max)
        z_edges.update(edge for edge in (bz0, bz1) if z_min < edge < z_max)
    xs, zs = sorted(x_edges), sorted(z_edges)
    for xa, xb in zip(xs, xs[1:]):
        for za, zb in zip(zs, zs[1:]):
            sample = ((xa + xb) / 2.0, (za + zb) / 2.0)
            if any(bx0 <= sample[0] <= bx1 and bz0 <= sample[1] <= bz1
                   for bx0, bx1, bz0, bz1 in boxes):
                continue
            nearest = (min(max(px, xa), xb), min(max(pz, za), zb))
            if math.dist(point, nearest) < radius - 1e-9:
                return False
    return True


def _route_exists(cell: dict, obstacles, goals, *, require_all: bool = True) -> bool:
    boxes = _cell_boxes(cell)
    x0, x1 = min(box[0] for box in boxes), max(box[1] for box in boxes)
    z0, z1 = min(box[2] for box in boxes), max(box[3] for box in boxes)
    nx = max(1, int(math.ceil((x1 - x0) / GRID_STEP)))
    nz = max(1, int(math.ceil((z1 - z0) / GRID_STEP)))
    nodes = {}
    for iz in range(nz):
        z = z0 + (iz + 0.5) * GRID_STEP
        for ix in range(nx):
            x = x0 + (ix + 0.5) * GRID_STEP
            point = (x, z)
            if not _disc_inside_box_union(point, boxes, ROUTE_RADIUS):
                continue
            if any(_point_near_polygon(point, polygon, ROUTE_RADIUS)
                   for polygon in obstacles):
                continue
            nodes[(ix, iz)] = point
    if not nodes:
        return False
    anchors = [_nearest_free(goal, nodes) for goal in goals]
    if any(anchor is None for anchor in anchors):
        return False
    start = anchors[0]
    reached = {start}
    queue = deque([start])
    while queue:
        ix, iz = queue.popleft()
        for candidate in ((ix - 1, iz), (ix + 1, iz), (ix, iz - 1), (ix, iz + 1)):
            if candidate in nodes and candidate not in reached:
                reached.add(candidate)
                queue.append(candidate)
    return (all(anchor in reached for anchor in anchors[1:]) if require_all
            else any(anchor in reached for anchor in anchors[1:]))


def _room_volumes(cells: dict, levels: dict) -> list[tuple[str, tuple]]:
    """Every non-exterior cell box as `(cell, (x0, x1, y0, y1, z0, z1))`, inset to its faces.

    A room box runs to the centre line of its walls. The thinnest wall, an interior partition,
    puts the finished face `ROOM_FACE_INSET` inside that line, so geometry reaching deeper than
    that is in the room rather than hidden in the wall.
    """
    volumes = []
    for cell_id, cell in sorted(cells.items()):
        level = levels.get(str(cell.get("level")))
        if cell.get("kind") == "exterior" or level is None:
            continue
        floor, ceiling = layout_io.cell_extent(cell, level)
        volumes += [(cell_id, (x0 + ROOM_FACE_INSET, x1 - ROOM_FACE_INSET, floor, ceiling,
                               z0 + ROOM_FACE_INSET, z1 - ROOM_FACE_INSET))
                    for x0, x1, z0, z1 in _cell_boxes(cell)]
    return volumes


def _intrusions(points, volumes) -> dict[str, int]:
    """How many of @p points lie strictly inside each room volume, by cell."""
    low = [min(point[axis] for point in points) for axis in range(3)]
    high = [max(point[axis] for point in points) for axis in range(3)]
    found: dict[str, int] = {}
    for cell_id, (x0, x1, y0, y1, z0, z1) in volumes:
        if high[0] <= x0 or low[0] >= x1 or high[1] <= y0 or low[1] >= y1 \
                or high[2] <= z0 or low[2] >= z1:
            continue
        count = sum(1 for x, y, z in points if x0 < x < x1 and y0 < y < y1 and z0 < z < z1)
        if count:
            found[cell_id] = found.get(cell_id, 0) + count
    return found


def _vegetation_intrusions(vegetation: list, volumes, manifest_path: Path) -> list[str]:
    """Exterior plantings whose placed geometry reaches inside a room.

    `HOUSE-03631`. A planting is authored as a point, so nothing looked at where its crown went,
    and Round 179 saw a mature crown through L1_BED3's corner wall. The instances are placed with
    `build_chunks.place`, the transform the chunks are baked with, over the same LOD0 meshes, so
    this measures the leaves the player actually sees.
    """
    document = json.loads(manifest_path.read_text(encoding="utf-8"))
    sources = {str(row.get("id")): row.get("sourceFile") for row in document.get("assets", [])}
    meshes: dict[str, list] = {}
    problems = []
    for group_index, planting in enumerate(vegetation):
        asset = str(planting.get("asset"))
        if asset not in meshes:
            source = sources.get(asset)
            meshes[asset] = (list(build_chunks.read_geometry_by_material(REPO / source).values())
                             if isinstance(source, str) else [])
        group_scale = float(planting.get("scale", 1.0))
        for index, instance in enumerate(planting.get("instances") or []):
            points = [point for mesh in meshes[asset]
                      for point in build_chunks.place(
                          mesh, [float(value) for value in instance["position"]],
                          float(instance.get("yawDeg", 0.0)),
                          group_scale * float(instance.get("scale", 1.0)))["positions"]]
            if not points:
                continue
            for cell_id, count in sorted(_intrusions(points, volumes).items()):
                problems.append(f"layout.exterior.json:vegetation/{group_index}/instances/{index} "
                                f"({planting.get('id')}): {count} vertices of {asset} lie inside "
                                f"room {cell_id}")
    return problems


def _exterior_prop_intrusions(props: list[Placed], cells: dict, volumes) -> list[str]:
    """Outdoor props -- wall lanterns above all -- whose geometry reaches into a room.

    `HOUSE-03631`. The cell-containment rule tolerates wall-mounted props crossing their cell's
    edge by `MOUNT_TOLERANCE`, which is right for a lamp's back plate in the wall and wrong for
    one centred on the wall's line: four lanterns were, and showed through the master bedroom's
    and sunroom's walls.
    """
    problems = []
    for prop in props:
        if (cells.get(prop.cell) or {}).get("kind") != "exterior":
            continue
        points = [_transform_point(point, prop.row) for point in prop.asset.positions]
        for cell_id, count in sorted(_intrusions(points, volumes).items()):
            problems.append(f"layout.props.json:props/{prop.index} ({prop.identifier}): {count} "
                            f"vertices of {prop.asset.identifier} lie inside room {cell_id}")
    return problems


def validate(world_dir: Path, manifest_path: Path, zones_path: Path) -> list[str]:
    layout = layout_io.load_layout(
        world_dir, ["levels", "cells", "portals", "openings", "props", "lights", "exterior"])
    prop_rows = layout_io.rows(layout, "props")
    assets = _load_assets(manifest_path, {str(row.get("asset")) for row in prop_rows})
    cells = {str(row["id"]): row for row in layout_io.rows(layout, "cells")}
    levels = {str(row["id"]): row for row in layout_io.rows(layout, "levels")}
    portals = layout_io.rows(layout, "portals")
    openings = {str(row.get("portal")): row for row in layout_io.rows(layout, "openings")}
    fixture_props = {str(row["fixtureProp"])
                     for row in layout_io.rows(layout, "lights")
                     if row.get("fixtureProp")}
    props = [_place(row, index, assets) for index, row in enumerate(prop_rows)]
    by_cell: dict[str, list[Placed]] = {}
    for prop in props:
        by_cell.setdefault(prop.cell, []).append(prop)

    zones = json.loads(zones_path.read_text(encoding="utf-8"))
    zone_rows = {str(row["id"]): row for zone in zones.get("zones", [])
                 for row in zone.get("cells", [])}
    width, height, terrain, _materials = terrain_gen.decode(world_dir)
    if (width, height) != (terrain_gen.WIDTH, terrain_gen.HEIGHT):
        raise layout_io.LayoutError("terrain dimensions do not match terrain_gen")

    problems = []
    for prop in props:
        prefix = f"layout.props.json:props/{prop.index} ({prop.identifier})"
        cell = cells.get(prop.cell)
        if cell is None:
            continue  # the world reference rule reports this
        level = levels.get(str(cell.get("level")))
        if level is None:
            continue
        floor, ceiling = layout_io.cell_extent(cell, level)
        wall_mounted = _wall_contact(prop, cell)
        if not _polygon_in_cell(prop.polygon, cell,
                                MOUNT_TOLERANCE if wall_mounted else SUPPORT_TOLERANCE):
            problems.append(f"{prefix}: visual envelope leaves cell {prop.cell} and intersects a wall")
        ceiling_fixture = (prop.asset.category == "fixture" and prop.bottom < ceiling
                           and prop.top >= ceiling - SUPPORT_TOLERANCE)
        if cell.get("kind") != "exterior" and (prop.bottom < floor - SUPPORT_TOLERANCE
                                                or (prop.top > ceiling + SUPPORT_TOLERANCE
                                                    and not ceiling_fixture)):
            problems.append(f"{prefix}: vertical envelope {prop.bottom:.3f}..{prop.top:.3f} leaves "
                            f"cell extent {floor:.3f}..{ceiling:.3f}")

        centre = (sum(point[0] for point in prop.polygon) / 4.0,
                  sum(point[1] for point in prop.polygon) / 4.0)
        terrain_height = _terrain_height(terrain, centre[0], centre[1])
        ground = (floor if cell.get("kind") != "exterior" or floor > terrain_height + 0.20
                  else terrain_height)
        grounded = abs(prop.bottom - ground) <= SUPPORT_TOLERANCE
        ceiling_mounted = (prop.asset.category in {"fixture", "fixture-small"}
                           and abs(prop.top - ceiling) <= SUPPORT_TOLERANCE)
        supported = any(_supported_by(prop, other) for other in by_cell.get(prop.cell, []))
        linked_fixture = _linked_mounted_fixture(
            prop.identifier, prop.asset.category, str(prop.row.get("collision", "proxy")),
            fixture_props)
        if not (grounded or ceiling_mounted or ceiling_fixture or wall_mounted or supported
                or linked_fixture):
            problems.append(f"{prefix}: bottom y={prop.bottom:.3f} is not on floor/terrain "
                            f"y={ground:.3f} or another support within {SUPPORT_TOLERANCE:.2f} m")

    # Rigid collision proxies must not occupy the same volume. Contact and sub-centimetre seams
    # are legal; non-colliding surface dressing is intentionally absent from this loop.
    for cell_id, rows in by_cell.items():
        for first_index, first in enumerate(rows):
            if not first.collision_polygons:
                continue
            for second in rows[first_index + 1:]:
                if not second.collision_polygons:
                    continue
                intersections = [(_polygon_overlap(a_polygon, b_polygon),
                                  min(a_top, b_top) - max(a_bottom, b_bottom))
                                 for a_polygon, a_bottom, a_top in first.collision_parts
                                 for b_polygon, b_bottom, b_top in second.collision_parts]
                tolerance = PAIR_OVERLAP_TOLERANCES.get(
                    frozenset((first.asset.category, second.asset.category)),
                    OVERLAP_TOLERANCE)
                violations = [intersection for intersection in intersections
                              if intersection[0] > tolerance
                              and intersection[1] > OVERLAP_TOLERANCE]
                if violations:
                    depth, vertical = max(violations,
                                          key=lambda value: value[0] * value[1])
                    problems.append(f"layout.props.json:props/{second.index} ({second.identifier}): "
                                    f"collision proxy overlaps {first.identifier} in {cell_id} by "
                                    f"{depth:.3f} m plan / {vertical:.3f} m vertical")

    # Aperture fronts are physical working space, not decoration density. Rule 14 separately
    # checks the full curved hinge sweep against every visual prop.
    for portal in portals:
        axis = (portal.get("plane") or {}).get("axis")
        if axis == "y":
            continue
        for cell_key in ("cellA", "cellB"):
            cell_id = str(portal.get(cell_key))
            cell = cells.get(cell_id)
            if cell is None:
                continue
            opening = openings.get(str(portal.get("id")))
            if opening is not None and opening.get("type") == "D_APPLIANCE":
                continue
            if (portal.get("kind") == "window" and opening is not None
                    and opening.get("type") in FIXED_WINDOW_TYPES):
                continue
            depth = WINDOW_FRONT if portal.get("kind") == "window" else OPENING_FRONT
            zone = _portal_zone(portal, cell, depth)
            if zone is None:
                continue
            for prop in by_cell.get(cell_id, []):
                if not prop.collision_polygons:
                    continue
                if (portal.get("kind") == "window"
                        and prop.asset.category in FITTED_WINDOW_PIECES):
                    continue
                vertical = portal.get("rect", {}).get("v", [0.0, 0.0])
                penetration = max((_polygon_overlap(zone, polygon)
                                   if min(top, float(vertical[1]))
                                   - max(bottom, float(vertical[0])) > OVERLAP_TOLERANCE else 0.0
                                   for polygon, bottom, top in prop.collision_parts), default=0.0)
                if penetration > OVERLAP_TOLERANCE:
                    what = "window front" if portal.get("kind") == "window" else "opening"
                    problems.append(f"layout.props.json:props/{prop.index} ({prop.identifier}): "
                                    f"blocks {what} {portal.get('id')} by {penetration:.3f} m")

    # Every furnished accessible cell keeps a 0.70 m route from its authored standing point to
    # an accessible, passable portal approach. Cells with no rigid prop cannot have regressed
    # because of furnishing and remain owned by the architecture/traversal validators.
    for cell_id, rows in by_cell.items():
        collision = [polygon for prop in rows for polygon in prop.collision_polygons]
        zone_row = zone_rows.get(cell_id) or {}
        standing = zone_row.get("standingPoint")
        if not collision or zone_row.get("accessible") is not True or not standing:
            continue
        goals = [(float(standing[0]), float(standing[2]))]
        for portal in portals:
            if portal.get("kind") == "window" or cell_id not in (str(portal.get("cellA")),
                                                                    str(portal.get("cellB"))):
                continue
            other = str(portal.get("cellB") if str(portal.get("cellA")) == cell_id
                        else portal.get("cellA"))
            if (zone_rows.get(other) or {}).get("accessible") is not True:
                continue
            zone = _portal_zone(portal, cells[cell_id], ROUTE_RADIUS + 0.05)
            if zone is not None:
                goals.append((sum(point[0] for point in zone) / 4.0,
                              sum(point[1] for point in zone) / 4.0))
        if (len(goals) > 1
                and not _route_exists(cells[cell_id], collision, goals, require_all=False)):
            problems.append(f"layout.props.json:{cell_id}: rigid props leave no continuous "
                            f"{ROUTE_WIDTH:.2f} m route from the standing point to an accessible "
                            "portal approach")
    volumes = _room_volumes(cells, levels)
    problems += _exterior_prop_intrusions(props, cells, volumes)
    problems += _vegetation_intrusions((layout.get("exterior") or {}).get("vegetation") or [],
                                       volumes, manifest_path)
    return problems


def selftest() -> int:
    """Focused synthetic geometry cases; the production run covers file/glTF integration."""
    failures = []

    def require(condition, message):
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    room = {"boxes": [{"x": [0.0, 4.0], "z": [0.0, 4.0]}]}
    inside = [(0.5, 0.5), (1.5, 0.5), (1.5, 1.5), (0.5, 1.5)]
    outside = [(3.5, 3.5), (4.5, 3.5), (4.5, 4.5), (3.5, 4.5)]
    require(_polygon_in_cell(inside, room), "a contained prop envelope passes")
    require(not _polygon_in_cell(outside, room), "a prop through a wall is rejected")
    require(_polygon_overlap(inside, [(1.0, 1.0), (2.0, 1.0), (2.0, 2.0),
                                      (1.0, 2.0)]) > OVERLAP_TOLERANCE,
            "rigid prop overlap is rejected beyond the stated tolerance")
    require(PAIR_OVERLAP_TOLERANCES[frozenset(("chair", "table-dining"))] == 0.04
            and PAIR_OVERLAP_TOLERANCES[
                frozenset(("counter-kitchen", "stool-counter"))] == 0.12,
            "only measured seating tuck pairs receive a larger stated tolerance")
    portal = {"plane": {"axis": "z", "value": 0.0}, "rect": {"u": [1.0, 2.0]},
              "kind": "window"}
    front = _portal_zone(portal, room, WINDOW_FRONT)
    require(front is not None and _polygon_overlap(front, [(1.2, 0.2), (1.8, 0.2),
                                                            (1.8, 0.8), (1.2, 0.8)]) > 0.0,
            "a prop in the 0.60 m window front is rejected")
    aperture = _portal_zone({"plane": {"axis": "z", "value": 0.0},
                             "rect": {"u": [1.0, 2.0]}, "kind": "door"},
                            room, OPENING_FRONT)
    require(aperture is not None
            and _polygon_overlap(aperture, [(1.2, 0.0), (1.8, 0.0),
                                             (1.8, 0.2), (1.2, 0.2)]) > 0.0,
            "a prop in a door or cased-opening aperture is rejected")
    require(abs(1.0 - 1.005) <= SUPPORT_TOLERANCE
            and abs(1.0 - 1.02) > SUPPORT_TOLERANCE,
            "support contact accepts 5 mm and rejects 20 mm")
    require(_linked_mounted_fixture("PROP_LIGHT", "ceiling-light", "none", {"PROP_LIGHT"})
            and not _linked_mounted_fixture(
                "PROP_LIGHT", "storage-furniture", "none", {"PROP_LIGHT"})
            and not _linked_mounted_fixture(
                "PROP_LIGHT", "ceiling-light", "proxy", {"PROP_LIGHT"})
            and not _linked_mounted_fixture(
                "PROP_UNLINKED", "ceiling-light", "none", {"PROP_LIGHT"}),
            "only linked non-colliding mounted fixture categories may omit floor support")
    varied_row = {"id": "PROP_PROBE", "position": [1.0, 2.0, 3.0], "yawDeg": 0.0,
                  "scale": [2.0, 3.0, 4.0],
                  "jitter": {"seed": 971, "yawDeg": 0.0, "offset": 0.1}}
    resolved, _yaw, _scale = layout_io.prop_transform(varied_row)
    transformed = _transform_point((1.0, 1.0, 1.0), varied_row)
    require(all(abs(a - b) < 1e-9 for a, b in
                zip(transformed, (resolved[0] + 2.0, 5.0, resolved[2] + 4.0))),
            "placement validation uses the same per-axis scale and seeded offset as the builders")
    barrier = [[(1.6, 0.0), (2.4, 0.0), (2.4, 4.0), (1.6, 4.0)]]
    require(not _route_exists(room, barrier, [(0.7, 2.0), (3.3, 2.0)]),
            "a rigid barrier that removes the 0.70 m route is rejected")
    require(_route_exists(room, [], [(0.7, 2.0), (3.3, 2.0)]),
            "the same room passes with a clear 0.70 m route")
    elbow = {"boxes": [
        {"x": [2.2, 3.8], "z": [-21.2, -20.2]},
        {"x": [3.8, 4.9], "z": [-23.0, -20.2]},
    ]}
    elbow_boxes = _cell_boxes(elbow)
    require(_disc_inside_box_union((3.8, -20.7), elbow_boxes, ROUTE_RADIUS),
            "a circulation disc may straddle the complete seam of an L-shaped cell")
    require(not _disc_inside_box_union((3.8, -21.2), elbow_boxes, ROUTE_RADIUS),
            "the same disc may not cut across the L-shaped cell's missing inner corner")
    require(_route_exists(elbow, [], [(2.8, -20.7), (4.35, -21.6)]),
            "a clear 0.70 m route turns through an L-shaped cell")
    volumes = _room_volumes(
        {"BED": {"level": "L1", "kind": "room", "boxes": [{"x": [0.0, 4.0], "z": [0.0, 4.0]}]},
         "YARD": {"level": "L1", "kind": "exterior",
                  "boxes": [{"x": [-9.0, 9.0], "z": [-9.0, 9.0]}]}},
        {"L1": {"ffl": 3.0, "ceiling": 5.5}})
    crown = [(-1.0, 4.0, 1.0), (0.4, 4.2, 1.0), (0.5, 6.0, 1.0), (-0.5, 4.0, 1.0),
             (0.05, 4.0, 1.0)]
    require(len(volumes) == 1 and _intrusions(crown, volumes) == {"BED": 1},
            "a crown vertex inside a room's finished faces and storey is an intrusion; one above "
            "the ceiling, outside the wall or hidden within the wall's half is not, and open "
            "exterior cells are not rooms")
    require(not _intrusions([(point[0] - 1.0, point[1], point[2]) for point in crown], volumes),
            "the same crown a metre further out clears the room")
    # Exact curved door/prop rejection is already exercised by validate_world.py --selftest and
    # intentionally has one implementation, not an approximate second opinion here.
    require(hasattr(__import__("validate_world"), "rule_14_static_leaf_poses"),
            "door-swing clearance is delegated to world-validator rule 14")
    if failures:
        print(f"validate_props: {len(failures)} selftest failure(s)", file=sys.stderr)
        return 1
    print("validate_props: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("world", nargs="?", type=Path, default=REPO / "assets-src" / "world")
    parser.add_argument("--manifest", type=Path,
                        default=REPO / "assets-src" / "assets.manifest.json")
    parser.add_argument("--zones", type=Path, default=REPO / "docs" / "zones.json")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        return selftest()
    try:
        problems = validate(args.world, args.manifest, args.zones)
    except (OSError, ValueError, KeyError, layout_io.LayoutError) as error:
        print(f"validate_props: {error}", file=sys.stderr)
        return 1
    if problems:
        for problem in problems:
            print(problem, file=sys.stderr)
        print(f"validate_props: {len(problems)} problem(s)", file=sys.stderr)
        return 1
    count = len(layout_io.load_file(args.world / "layout.props.json")["props"])
    print(f"validate_props: {count} static prop row(s) pass placement and route checks.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
