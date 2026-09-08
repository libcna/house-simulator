#!/usr/bin/env python3
"""lightmap_unwrap.py -- a second UV channel for the shell, packed at a fixed texel density.

`HOUSE-00205`. `cna-house.md` §18.3 step 2: every shell face gets a second UV channel, packed per
cell into a **texel-density-uniform** atlas -- 4 texels/metre for rooms, 8 for small rooms, 2 for
the attic and basement -- with a gutter between islands. §22's lightmaps are then baked into it
(`HOUSE-00206`), so everything that can go wrong here goes wrong invisibly: a bake looks fine and
the game shows light bleeding across a corner, or a wall lit at half the resolution of the floor
beside it.

    blender --background --factory-startup --python lightmap_unwrap.py -- IN.glb OUT.glb
    tools/blender/lightmap_unwrap.py IN.glb OUT.glb --density 8 --gutter 4
    tools/blender/lightmap_unwrap.py --selftest

**Run it either way.** Invoked as a plain script it finds `blender` and re-runs itself inside it.

## What is checked, and why a UV tool has to check it

None of this is visible in a UV editor at a glance, and all of it ruins a bake:

* **No two faces share a texel.** Not "islands do not overlap" -- *faces*. A lightmap texel is one
  light sample, so two faces over one texel are two surfaces lit by one measurement, and the
  brighter one wins. Checked by rasterising every face's UV polygon at atlas resolution and
  sampling texel CENTRES, which is what the baker does, so faces that merely share an edge do not
  count as overlapping and a real overlap does.
* **The gutter is between ISLANDS, never within one.** Faces inside an island must stay seamless
  -- a gutter there would put a hard line across a flat wall. Islands are found by union-find over
  UV-shared edges, and the minimum separation between two islands' texels is measured.
* **The texel density is the one that was asked for.** The atlas size is *derived* from the target
  density and the worst face's packing, not chosen and hoped over: `size = density / sqrt(uvArea /
  worldArea)` for the least-dense face, rounded up to a power of two. A size chosen first and a
  density measured afterwards is how a wall ends up at half the floor's resolution.
* **UV0 is untouched, byte for byte.** The albedo channel is what `DualTextureEffect` samples first
  (§21.3), and an unwrapper that overwrote it would produce a correct lightmap on a model whose
  textures had moved.
* **No degenerate face.** A face with zero UV area receives no texel at all and bakes black.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import json
import math
import os
import sys

# --------------------------------------------------------------------------------------------
# Re-exec under Blender when run as a plain script, so callers need not know how this works.
# --------------------------------------------------------------------------------------------
try:
    import bpy  # type: ignore

    INSIDE_BLENDER = True
except ImportError:  # pragma: no cover - the relauncher path
    INSIDE_BLENDER = False

if not INSIDE_BLENDER:
    from pathlib import Path

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import blender_env  # noqa: E402

    sys.exit(blender_env.relaunch(Path(__file__).resolve(), sys.argv[1:],
                                  tool="lightmap_unwrap"))


# --------------------------------------------------------------------------------------------
# Everything below runs inside Blender.
# --------------------------------------------------------------------------------------------
import bmesh  # type: ignore  # noqa: E402

#: §18.3's densities, in texels per metre. Named rather than a bare number, because the reason
#: `attic` is 2 and `small_room` is 8 is architectural and not a tuning knob: the attic is seen
#: from far away and dimly, a bathroom is seen from a metre.
DENSITIES = {"room": 4.0, "small_room": 8.0, "attic": 2.0, "basement": 2.0}
DEFAULT_DENSITY = DENSITIES["room"]

#: Texels of empty atlas between two islands. Four, because a lightmap is sampled bilinearly and
#: mipped one level for the distant view; three would bleed at that level.
DEFAULT_GUTTER = 4

#: The second UV channel's name. One name, in one place: the baker looks it up by name and a
#: mismatch is a bake into the albedo channel.
LIGHTMAP_UV = "Lightmap"

MIN_ATLAS = 32
DEFAULT_MAX_ATLAS = 2048

#: Below this many texels in its bounding box, a face is rasterised by the scalar loop rather than
#: by numpy: setting up an array costs more than testing a handful of points (`HOUSE-00471`).
SCALAR_TEXELS = 256

#: How far below the target density the chosen atlas may land. A power-of-two atlas cannot hit an
#: arbitrary density exactly: the next size up costs FOUR times the memory for at most twice the
#: density, so refusing a 2 % shortfall would quadruple the lightmap budget to buy nothing anyone
#: can see. The achieved density is reported either way.
DENSITY_TOLERANCE = 0.05

#: The atlas must be at least this many times the gutter. At 32 texels a 4-texel gutter is an
#: eighth of the whole atlas per island edge and `pack_islands` collapses islands to nothing trying
#: to honour it -- measured, and it reports FINISHED while doing so.
GUTTER_HEADROOM = 20

#: Below this a face has no usable area in UV space and would bake black.
MIN_UV_AREA = 1e-9


def report(message: str) -> None:
    print(f"  {message}")


# ------------------------------------------------------------------------------------ geometry ----

def _polygon_area_2d(points: list[tuple[float, float]]) -> float:
    total = 0.0
    for index in range(len(points)):
        x0, y0 = points[index]
        x1, y1 = points[(index + 1) % len(points)]
        total += x0 * y1 - x1 * y0
    return abs(total) * 0.5


def _point_in_polygon(x: float, y: float, points: list[tuple[float, float]]) -> bool:
    inside = False
    count = len(points)
    for index in range(count):
        x0, y0 = points[index]
        x1, y1 = points[(index + 1) % count]
        if (y0 > y) != (y1 > y):
            crossing = x0 + (y - y0) * (x1 - x0) / (y1 - y0)
            if x < crossing:
                inside = not inside
    return inside


def face_records(objects) -> list[dict]:
    """Every face's world area, its lightmap UV polygon, and which object and face it is."""
    records = []
    for obj in objects:
        matrix = obj.matrix_world
        mesh = obj.data
        layer = mesh.uv_layers.get(LIGHTMAP_UV)
        if layer is None:
            continue
        for polygon in mesh.polygons:
            world = [matrix @ mesh.vertices[mesh.loops[i].vertex_index].co
                     for i in polygon.loop_indices]
            area = 0.0
            for index in range(1, len(world) - 1):
                area += (world[index] - world[0]).cross(world[index + 1] - world[0]).length * 0.5
            uvs = [tuple(layer.data[i].uv) for i in polygon.loop_indices]
            records.append({"object": obj.name, "polygon": polygon.index,
                            "worldArea": area, "uv": uvs, "uvArea": _polygon_area_2d(uvs),
                            "loops": [mesh.loops[i].vertex_index for i in polygon.loop_indices]})
    return records


def island_of_face(objects) -> dict[tuple[str, int], int]:
    """Union-find over faces that share a UV edge. Two faces are one island when they share a mesh
    edge AND both of that edge's vertices have the same UV in both faces -- which is exactly the
    condition under which a bake must be seamless across them."""
    parent: dict[tuple[str, int], tuple[str, int]] = {}

    def find(key):
        while parent[key] != key:
            parent[key] = parent[parent[key]]
            key = parent[key]
        return key

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[max(ra, rb)] = min(ra, rb)

    for obj in objects:
        mesh = obj.data
        layer = mesh.uv_layers.get(LIGHTMAP_UV)
        if layer is None:
            continue
        for polygon in mesh.polygons:
            parent[(obj.name, polygon.index)] = (obj.name, polygon.index)

        # vertex -> [(face, uv)], so a shared vertex with a shared UV can be found in one pass.
        by_edge: dict[tuple[int, int], list[tuple[int, tuple, tuple]]] = {}
        for polygon in mesh.polygons:
            corners = list(polygon.loop_indices)
            for index, loop in enumerate(corners):
                nxt = corners[(index + 1) % len(corners)]
                a = mesh.loops[loop].vertex_index
                b = mesh.loops[nxt].vertex_index
                key = (min(a, b), max(a, b))
                uv_a = tuple(round(v, 6) for v in layer.data[loop].uv)
                uv_b = tuple(round(v, 6) for v in layer.data[nxt].uv)
                pair = (uv_a, uv_b) if a < b else (uv_b, uv_a)
                by_edge.setdefault(key, []).append((polygon.index, pair[0], pair[1]))

        for entries in by_edge.values():
            for i in range(len(entries)):
                for j in range(i + 1, len(entries)):
                    if entries[i][1] == entries[j][1] and entries[i][2] == entries[j][2]:
                        union((obj.name, entries[i][0]), (obj.name, entries[j][0]))

    roots: dict[tuple[str, int], int] = {}
    numbered: dict[tuple[str, int], int] = {}
    for key in sorted(parent):
        root = find(key)
        if root not in roots:
            roots[root] = len(roots)
        numbered[key] = roots[root]
    return numbered


# ------------------------------------------------------------------------------------ checking ----

def rasterise(records: list[dict], islands: dict, size: int) -> tuple[list[int], list[str]]:
    """A `size`x`size` grid of island ids, and the overlaps found while filling it.

    Texel CENTRES, because that is what the baker samples. Two faces that merely share an edge do
    not both claim a texel; two faces that genuinely overlap do.
    """
    import numpy  # noqa: PLC0415  (Blender's interpreter has it; see blender_env.py)

    # Vectorised over each face's own bounding box. The test is the same crossing-number rule
    # `_point_in_polygon` applies one texel at a time -- and that version is kept, and claimed
    # against this one, because a rewrite for speed that changes an answer is a rewrite that
    # ruins bakes silently (`HOUSE-00471`: at 2048² the loop version is minutes a cell).
    grid = numpy.full(size * size, -1, dtype=numpy.int32)
    owner = numpy.full(size * size, -1, dtype=numpy.int32)
    keys: list[tuple[str, int]] = []
    problems: list[str] = []
    for index, record in enumerate(records):
        uvs = record["uv"]
        keys.append((record["object"], record["polygon"]))
        xs = [u for u, _ in uvs]
        ys = [v for _, v in uvs]
        x0 = max(int(math.floor(min(xs) * size)), 0)
        x1 = min(int(math.ceil(max(xs) * size)), size)
        y0 = max(int(math.floor(min(ys) * size)), 0)
        y1 = min(int(math.ceil(max(ys) * size)), size)
        if x1 <= x0 or y1 <= y0:
            continue
        island = islands.get(keys[-1], -1)

        # Small faces stay on the scalar path. A numpy pass costs tens of microseconds to set up
        # whatever it covers, and most of a shell's faces cover a handful of texels: measured over
        # the fixture, vectorising everything was SLOWER than the loop it replaced. The threshold
        # is where the two meet.
        if (x1 - x0) * (y1 - y0) <= SCALAR_TEXELS:
            for y in range(y0, y1):
                centre_y = (y + 0.5) / size
                for x in range(x0, x1):
                    if not _point_in_polygon((x + 0.5) / size, centre_y, uvs):
                        continue
                    cell = y * size + x
                    if owner[cell] >= 0 and owner[cell] != index:
                        other = keys[int(owner[cell])]
                        problems.append(
                            f"texel ({x}, {y}) is claimed by both {other[0]}#{other[1]} and "
                            f"{keys[-1][0]}#{keys[-1][1]}: two surfaces would share one light "
                            f"sample")
                        continue
                    grid[cell] = island
                    owner[cell] = index
            continue

        centres_x = (numpy.arange(x0, x1) + 0.5) / size
        centres_y = (numpy.arange(y0, y1) + 0.5) / size
        px, py = numpy.meshgrid(centres_x, centres_y)
        inside = numpy.zeros(px.shape, dtype=bool)
        count = len(uvs)
        for corner in range(count):
            ax, ay = uvs[corner]
            bx, by = uvs[(corner + 1) % count]
            crosses = (ay > py) != (by > py)
            with numpy.errstate(divide="ignore", invalid="ignore"):
                boundary_x = (bx - ax) * (py - ay) / numpy.where(by - ay == 0, 1.0, by - ay) + ax
            inside ^= crosses & (px < boundary_x)

        cells = ((numpy.arange(y0, y1)[:, None] * size)
                 + numpy.arange(x0, x1)[None, :])[inside]
        if cells.size == 0:
            continue
        taken = owner[cells]
        clash = cells[(taken >= 0) & (taken != index)]
        if clash.size:
            first = int(clash[0])
            other = keys[int(owner[first])]
            problems.append(
                f"texel ({first % size}, {first // size}) is claimed by both {other[0]}#{other[1]} "
                f"and {keys[-1][0]}#{keys[-1][1]}: two surfaces would share one light sample")
        free = cells[(taken < 0) | (taken == index)]
        grid[free] = island
        owner[free] = index
    return grid.tolist(), problems


def measure_gutter_reference(grid: list[int], size: int, gutter: int):
    """The loop-for-loop version of `measure_gutter`, kept as the thing the fast one is checked
    against. Slow by construction -- it touches every texel and every neighbour -- and never used
    outside the selftest (`HOUSE-00471`)."""
    problems: list[str] = []
    smallest: int | None = None
    reach = gutter + 1
    for y in range(size):
        for x in range(size):
            island = grid[y * size + x]
            if island < 0:
                continue
            for dy in range(-reach, reach + 1):
                ny = y + dy
                if not 0 <= ny < size:
                    continue
                for dx in range(-reach, reach + 1):
                    nx = x + dx
                    if not 0 <= nx < size or (dx == 0 and dy == 0):
                        continue
                    other = grid[ny * size + nx]
                    if other < 0 or other == island:
                        continue
                    between = max(abs(dx), abs(dy)) - 1
                    if smallest is None or between < smallest:
                        smallest = between
    return smallest, problems


def measure_gutter(grid: list[int], size: int, gutter: int) -> tuple[int | None, list[str]]:
    """The fewest EMPTY texels between two different islands, and any shortfall against `gutter`.

    Empty texels between, not centre-to-centre distance: "a 4-texel gutter" means four texels of
    nothing, so two islands one texel apart in Chebyshev distance have a gutter of **zero**. The
    off-by-one matters -- it is the difference between a bilinear tap at the mip level reaching a
    neighbour's light and not.

    Chebyshev because a bilinear tap and a mip reduction both read a square neighbourhood.

    Only island BOUNDARY texels are scanned -- one whose four-neighbourhood is entirely its own
    island cannot be the closest to anything. Over the whole atlas this is the difference between
    scanning 21 million neighbourhoods at 512² and scanning a few thousand.
    """
    problems: list[str] = []
    smallest: int | None = None
    reach = gutter + 1

    import numpy  # noqa: PLC0415

    # The boundary scan is the one place this tool touches every texel, so it is vectorised: at
    # 2048² the Python loop is four million iterations before any measuring starts. Same rule --
    # a texel whose four-neighbourhood is entirely its own island is not a boundary texel.
    field = numpy.asarray(grid, dtype=numpy.int32).reshape(size, size)
    filled = field >= 0
    edge = numpy.zeros_like(filled)
    for axis, shift in ((1, 1), (1, -1), (0, 1), (0, -1)):
        rolled = numpy.roll(field, shift, axis=axis)
        differs = rolled != field
        if axis == 1 and shift == 1:
            differs[:, 0] = True
        elif axis == 1:
            differs[:, -1] = True
        elif shift == 1:
            differs[0, :] = True
        else:
            differs[-1, :] = True
        edge |= differs
    ys, xs = numpy.nonzero(filled & edge)
    boundary = [(int(x), int(y), int(field[y, x])) for x, y in zip(xs, ys)]

    if not boundary:
        return None, problems

    # One vectorised pass per offset in the (2·reach+1)² neighbourhood, over EVERY boundary texel
    # at once, instead of a Python loop per texel per offset. Same neighbourhood, same Chebyshev
    # rule, same answer -- the tool's own selftest is the contract (`HOUSE-00471`).
    bx = numpy.array([x for x, _y, _island in boundary], dtype=numpy.int64)
    by = numpy.array([y for _x, y, _island in boundary], dtype=numpy.int64)
    mine = numpy.array([island for _x, _y, island in boundary], dtype=numpy.int32)
    for ring in range(0, reach + 1):
        between = ring - 1
        offsets = [(dx, dy) for dy in range(-ring, ring + 1) for dx in range(-ring, ring + 1)
                   if max(abs(dx), abs(dy)) == ring]
        for dx, dy in offsets:
            if dx == 0 and dy == 0:
                continue
            nx, ny = bx + dx, by + dy
            valid = (nx >= 0) & (nx < size) & (ny >= 0) & (ny < size)
            if not valid.any():
                continue
            other = field[ny[valid], nx[valid]]
            differs = (other >= 0) & (other != mine[valid])
            if not differs.any():
                continue
            if smallest is None or between < smallest:
                smallest = between
            if between < gutter and len(problems) < 5:
                first = int(numpy.nonzero(differs)[0][0])
                where = numpy.nonzero(valid)[0][first]
                problems.append(
                    f"islands {int(mine[where])} and {int(other[differs][0])} have {between} "
                    f"empty texel(s) between them at ({int(bx[where])}, {int(by[where])}); the "
                    f"gutter is {gutter}")
        if smallest is not None:
            break        # the rings are searched outward, so the first hit IS the smallest
    return smallest, problems[:5]


def densities(records: list[dict], size: int) -> list[float]:
    values = []
    for record in records:
        if record["worldArea"] <= 0 or record["uvArea"] <= MIN_UV_AREA:
            continue
        values.append(size * math.sqrt(record["uvArea"] / record["worldArea"]))
    return sorted(values)


# ------------------------------------------------------------------------------------ unwrapping ----

def _require(result, operator: str) -> None:
    """A Blender operator that returns `CANCELLED` has done NOTHING, and says so only in its
    return value.

    Measured the hard way (`HOUSE-00205`): in background mode `uv.average_islands_scale` and
    `uv.pack_islands` both cancel unless `use_uv_select_sync` is on, because they act on the UV
    selection and there is no UV editor to have made one. The unwrap then completed, produced
    plausible UVs, reported a uniform density -- smart_project's own output happens to be nearly
    uniform -- and the gutter was one texel. Nothing raised. Every UV operator here is checked.
    """
    if "FINISHED" not in result:
        raise RuntimeError(f"bpy.ops.{operator} returned {sorted(result)} rather than FINISHED, "
                           f"so it did nothing. In background mode this is usually a missing "
                           f"scene.tool_settings.use_uv_select_sync.")


def _mesh_objects():
    return [o for o in bpy.data.objects if o.type == "MESH" and len(o.data.polygons) > 0]


def unwrap(density: float, gutter: int, max_atlas: int) -> dict:
    objects = _mesh_objects()
    if not objects:
        raise RuntimeError("the file has no mesh with faces")

    # A face with no area in the WORLD is a content error, and it must be reported as that rather
    # than as "every face has zero UV area" three steps later. It also cannot be unwrapped: there
    # is nothing to unwrap.
    degenerate_input = []
    for obj in objects:
        matrix = obj.matrix_world
        mesh = obj.data
        for polygon in mesh.polygons:
            world = [matrix @ mesh.vertices[mesh.loops[i].vertex_index].co
                     for i in polygon.loop_indices]
            area = sum((world[i] - world[0]).cross(world[i + 1] - world[0]).length * 0.5
                       for i in range(1, len(world) - 1))
            if area <= 1e-9:
                degenerate_input.append(f"{obj.name}#{polygon.index}")
    if degenerate_input:
        raise RuntimeError(f"{len(degenerate_input)} face(s) have no area in the world and cannot "
                           f"be unwrapped: {degenerate_input[:5]}")

    original_uv0: dict[str, list[tuple[float, float]]] = {}
    for obj in objects:
        mesh = obj.data
        if not mesh.uv_layers:
            raise RuntimeError(f"{obj.name} has no UV layer at all; the shell's albedo channel "
                               f"must exist before a second one is added")
        first = mesh.uv_layers[0]
        original_uv0[obj.name] = [tuple(item.uv) for item in first.data]
        if LIGHTMAP_UV in mesh.uv_layers:
            mesh.uv_layers.remove(mesh.uv_layers[LIGHTMAP_UV])
        layer = mesh.uv_layers.new(name=LIGHTMAP_UV)
        mesh.uv_layers.active = layer
        # `active_render` stays on UV0: it is what an exporter writes as TEXCOORD_0, and moving it
        # would silently swap the albedo and lightmap channels in the .glb.
        first.active_render = True

    # WITHOUT THIS every UV operator below silently cancels in background mode. See `_require`.
    bpy.context.scene.tool_settings.use_uv_select_sync = True

    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]

    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    # A 66-degree angle limit is Blender's own default and it is right for architecture: a wall
    # meeting a floor is 90 degrees and must split, a bevelled reveal at 30 must not.
    _require(bpy.ops.uv.smart_project(angle_limit=math.radians(66.0), island_margin=0.0,
                                      correct_aspect=True, scale_to_bounds=False),
             "uv.smart_project")
    # THIS is what makes the density uniform. Without it every island is scaled to its own bounds
    # and a small wall gets the same UV area as a large floor -- which is the failure this whole
    # tool exists to prevent, and it is invisible until the bake.
    _require(bpy.ops.uv.average_islands_scale(), "uv.average_islands_scale")
    bpy.ops.object.mode_set(mode="OBJECT")

    # The atlas size is DERIVED, by PACKING at each candidate rather than by extrapolating from
    # one. The gutter is a fixed number of TEXELS, so its cost as a fraction of the atlas halves
    # every time the size doubles -- an extrapolation from a single pack would be wrong by that
    # factor, badly at small sizes where the gutter dominates.
    # The search starts where the gutter is a sane fraction of the atlas. At 32 texels a 4-texel
    # gutter is 12.5 % of the whole atlas per island edge, and `pack_islands` collapses islands to
    # nothing trying to fit them -- measured. `GUTTER_HEADROOM` keeps the margin under 5 %.
    size = MIN_ATLAS
    while size < max_atlas and gutter / size > 1.0 / GUTTER_HEADROOM:
        size *= 2

    records: list[dict] = []
    measured: list[float] = []
    while True:
        _pack(objects, gutter, size)
        records = face_records(objects)
        measured = densities(records, size)
        collapsed = [r for r in records if r["uvArea"] <= MIN_UV_AREA]
        if measured and not collapsed and (measured[0] >= density * (1.0 - DENSITY_TOLERANCE)):
            break
        if size >= max_atlas:
            if collapsed:
                raise RuntimeError(f"{len(collapsed)} face(s) pack to no UV area even at "
                                   f"{max_atlas}²; the gutter of {gutter} texels does not fit")
            break
        size *= 2

    islands = island_of_face(objects)
    grid, overlaps = rasterise(records, islands, size)
    smallest, gutter_problems = measure_gutter(grid, size, gutter)

    degenerate = [f"{r['object']}#{r['polygon']}" for r in records
                  if r["uvArea"] <= MIN_UV_AREA]
    outside = [f"{r['object']}#{r['polygon']}" for r in records
               if any(not (-1e-6 <= u <= 1 + 1e-6 and -1e-6 <= v <= 1 + 1e-6) for u, v in r["uv"])]

    problems = list(overlaps[:5]) + gutter_problems
    if degenerate:
        problems.append(f"{len(degenerate)} face(s) have no UV area and would bake black: "
                        f"{degenerate[:5]}")
    if outside:
        problems.append(f"{len(outside)} face(s) have UVs outside [0, 1]: {outside[:5]}")

    # UV0, compared value by value. An unwrapper that touched the albedo channel would produce a
    # correct lightmap on a model whose textures had moved.
    for obj in objects:
        before = original_uv0[obj.name]
        after = [tuple(item.uv) for item in obj.data.uv_layers[0].data]
        if before != after:
            moved = sum(1 for a, b in zip(before, after) if a != b)
            problems.append(f"{obj.name}: UV0 changed in {moved} of {len(before)} loops")
        if obj.data.uv_layers[0].name == LIGHTMAP_UV:
            problems.append(f"{obj.name}: the lightmap layer became UV0")

    covered = sum(1 for cell in grid if cell >= 0)
    return {
        "atlasSize": size,
        "targetDensity": density,
        "gutter": gutter,
        "faces": len(records),
        "islands": len(set(islands.values())),
        "minDensity": round(measured[0], 3) if measured else 0.0,
        "medianDensity": round(measured[len(measured) // 2], 3) if measured else 0.0,
        "maxDensity": round(measured[-1], 3) if measured else 0.0,
        "worldArea": round(sum(r["worldArea"] for r in records), 4),
        "coveredTexels": covered,
        "utilisation": round(covered / (size * size), 4),
        "minIslandGutter": smallest,
        "densityShortfall": round(1.0 - measured[0] / density, 4) if measured else None,
        "uvLayers": [layer.name for layer in objects[0].data.uv_layers],
        "problems": problems,
    }


def _pack(objects, gutter: int, size: int) -> None:
    bpy.context.scene.tool_settings.use_uv_select_sync = True
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    # `FRACTION` makes the margin a fraction of the final atlas, so a gutter in TEXELS is exactly
    # `gutter / size` -- which `SCALED`, the default, is not: it scales the margin by island size
    # and gives a different gap around every island.
    _require(bpy.ops.uv.pack_islands(rotate=True, scale=True, margin_method="FRACTION",
                                     margin=gutter / size, shape_method="CONCAVE"),
             "uv.pack_islands")
    bpy.ops.object.mode_set(mode="OBJECT")


# ------------------------------------------------------------------------------------- fixture ----

def build_room_fixture() -> None:
    """A room shell: floor, ceiling, four walls, a door opening and a window opening.

    A real architectural fixture rather than a synthetic triangle, because the failures this tool
    guards are architectural. A wall with a hole in it is four quads round an opening, and those
    four are what a naive unwrapper packs as one island with the hole inside it -- which bakes
    light through a doorway onto a wall that is not there.
    """
    bpy.ops.wm.read_factory_settings(use_empty=True)
    width, depth, height = 4.0, 3.0, 2.6
    verts: list[tuple[float, float, float]] = []
    faces: list[list[int]] = []

    def quad(a, b, c, d) -> None:
        base = len(verts)
        verts.extend([a, b, c, d])
        faces.append([base, base + 1, base + 2, base + 3])

    half_w, half_d = width / 2, depth / 2
    quad((-half_w, 0, -half_d), (half_w, 0, -half_d), (half_w, 0, half_d), (-half_w, 0, half_d))
    quad((-half_w, height, half_d), (half_w, height, half_d),
         (half_w, height, -half_d), (-half_w, height, -half_d))

    def wall_with_opening(sign_z: float, hole: tuple[float, float, float, float] | None) -> None:
        """A wall on the +Z or -Z side, as one quad or as four around `hole` (x0, x1, y0, y1)."""
        z = sign_z * half_d
        if hole is None:
            quad((-half_w, 0, z), (half_w, 0, z), (half_w, height, z), (-half_w, height, z))
            return
        x0, x1, y0, y1 = hole
        if y0 > 1e-6:
            # A doorway reaches the floor and has NO wall below it. Emitting the quad anyway gives
            # a zero-area face, which is exactly the degenerate this tool refuses -- and the first
            # version of this fixture did emit it, which is how that check was first exercised.
            quad((-half_w, 0, z), (half_w, 0, z), (half_w, y0, z), (-half_w, y0, z))      # below
        quad((-half_w, y1, z), (half_w, y1, z), (half_w, height, z), (-half_w, height, z))  # above
        quad((-half_w, y0, z), (x0, y0, z), (x0, y1, z), (-half_w, y1, z))                # left
        quad((x1, y0, z), (half_w, y0, z), (half_w, y1, z), (x1, y1, z))                  # right

    wall_with_opening(1.0, (-0.45, 0.45, 0.0, 2.05))    # a doorway
    wall_with_opening(-1.0, (-0.6, 0.6, 0.9, 2.1))      # a window
    quad((-half_w, 0, -half_d), (-half_w, 0, half_d), (-half_w, height, half_d),
         (-half_w, height, -half_d))
    quad((half_w, 0, half_d), (half_w, 0, -half_d), (half_w, height, -half_d),
         (half_w, height, half_d))

    mesh = bpy.data.meshes.new("shell")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    # UV0 exists and carries something identifiable, so "UV0 is untouched" is a claim with content.
    layer = mesh.uv_layers.new(name="UVMap")
    for index, item in enumerate(layer.data):
        item.uv = ((index % 7) / 7.0, (index % 5) / 5.0)
    obj = bpy.data.objects.new("shell", mesh)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj


# ------------------------------------------------------------------------------------ selftest ----

def selftest() -> int:
    failures = 0

    def require(condition: bool, message: str) -> None:
        nonlocal failures
        if condition:
            print(f"  PASS  {message}")
        else:
            print(f"  FAIL  {message}")
            failures += 1

    # ---- the fast paths agree with the slow ones (`HOUSE-00471`) -------------------------------
    #
    # `rasterise` and `measure_gutter` are the two places this tool touches every texel, and both
    # were rewritten to run over arrays instead of over Python loops. A rewrite for speed that
    # changes an answer is a rewrite that ruins bakes silently, so both are checked against the
    # loop they replaced, on shapes chosen to exercise the awkward parts: a rectangle, a triangle
    # whose edges cut texels diagonally, and two islands a known distance apart.
    global SCALAR_TEXELS  # noqa: PLW0603  (the point is to force each path in turn)
    keep_threshold = SCALAR_TEXELS
    probe_records = [
        {"object": "a", "polygon": 0, "uv": [(0.10, 0.10), (0.40, 0.10), (0.40, 0.40),
                                             (0.10, 0.40)], "uvArea": 0.09, "worldArea": 1.0},
        {"object": "a", "polygon": 1, "uv": [(0.55, 0.12), (0.92, 0.33), (0.61, 0.78)],
         "uvArea": 0.08, "worldArea": 1.0},
        {"object": "a", "polygon": 2, "uv": [(0.05, 0.80), (0.20, 0.80), (0.20, 0.95),
                                             (0.05, 0.95)], "uvArea": 0.02, "worldArea": 1.0},
    ]
    probe_islands = {("a", 0): 0, ("a", 1): 1, ("a", 2): 2}
    for probe_size in (32, 64, 128):
        SCALAR_TEXELS = 10 ** 9
        slow_grid, slow_problems = rasterise(probe_records, probe_islands, probe_size)
        SCALAR_TEXELS = 0
        fast_grid, fast_problems = rasterise(probe_records, probe_islands, probe_size)
        SCALAR_TEXELS = keep_threshold
        require(slow_grid == fast_grid,
                f"at {probe_size}² the vectorised rasteriser fills exactly the texels the loop "
                f"does ({sum(1 for c in slow_grid if c >= 0)} of {probe_size * probe_size}, "
                f"{sum(1 for a, b in zip(slow_grid, fast_grid) if a != b)} differ)")
        require(len(slow_problems) == len(fast_problems),
                f"and finds the same overlaps ({len(slow_problems)}, {len(fast_problems)})")
        slow_gap, _ = measure_gutter_reference(slow_grid, probe_size, DEFAULT_GUTTER)
        fast_gap, _ = measure_gutter(slow_grid, probe_size, DEFAULT_GUTTER)
        require(slow_gap == fast_gap,
                f"and the vectorised gutter measures the same smallest gap at {probe_size}² "
                f"({slow_gap}, {fast_gap})")

    build_room_fixture()
    result = unwrap(DENSITIES["small_room"], DEFAULT_GUTTER, DEFAULT_MAX_ATLAS)

    require(not result["problems"],
            f"the unwrap has no problems ({len(result['problems'])}): {result['problems'][:3]}")

    # 1. The density is the one that was ASKED for, across every face -- which is the whole point
    #    of `average_islands_scale`, and the failure a size chosen first would hide.
    target = DENSITIES["small_room"]
    require(result["minDensity"] >= target * (1.0 - DENSITY_TOLERANCE),
            f"the least-dense face is {result['minDensity']:.2f} texels/m against a target of "
            f"{target} (shortfall {result['densityShortfall']:.3f}, tolerance "
            f"{DENSITY_TOLERANCE})")
    spread = result["maxDensity"] / max(result["minDensity"], 1e-6)
    require(spread < 1.05,
            f"density is uniform to {spread:.3f}x across every face "
            f"({result['minDensity']:.2f} to {result['maxDensity']:.2f} texels/m)")

    # 2. Islands, and a gutter between them. `minIslandSeparation` is None when nothing came within
    #    the gutter at all, which is what a correct pack looks like.
    require(result["islands"] > 1,
            f"the shell packs into {result['islands']} islands, so the gutter has something to "
            f"separate")
    closest = result["minIslandGutter"]
    require(closest is None or closest >= DEFAULT_GUTTER,
            f"the gutter holds: "
            + (f"no two islands come within {DEFAULT_GUTTER + 1} texels of each other at all"
               if closest is None
               else f"the closest pair has {closest} empty texels between them, against the "
                    f"{DEFAULT_GUTTER} asked for"))

    # 3. UV0 survives, and stays UV0. `lightmap_unwrap` writes the SECOND channel.
    require(result["uvLayers"] == ["UVMap", LIGHTMAP_UV],
            f"the layers are {result['uvLayers']}: the albedo channel first, the lightmap second")

    # 4. The atlas is derived, not chosen. 57 m² at 8 texels/m needs about 61² texels of content,
    #    so anything below 64 cannot hold it and 2048 would be absurd.
    require(MIN_ATLAS <= result["atlasSize"] <= 512,
            f"the atlas is {result['atlasSize']}² for {result['worldArea']:.1f} m² at {target} "
            f"texels/m")
    require(0.2 < result["utilisation"] < 0.95,
            f"{result['utilisation'] * 100:.0f} % of the atlas carries surface")

    # 5. Asking for MORE density must give a bigger atlas. A tool that returned the same size
    #    whatever it was asked for would pass every check above.
    build_room_fixture()
    fine = unwrap(32.0, DEFAULT_GUTTER, DEFAULT_MAX_ATLAS)
    require(fine["atlasSize"] > result["atlasSize"],
            f"32 texels/m gives a {fine['atlasSize']}² atlas against "
            f"{DENSITIES['small_room']}'s {result['atlasSize']}²")
    require(fine["minDensity"] >= 32.0 * (1.0 - DENSITY_TOLERANCE),
            f"...and reaches its own target ({fine['minDensity']:.2f} texels/m)")

    # 5b. MEASURED, and it is not the obvious answer: asking for LESS density does NOT give a
    #     smaller atlas here. A 4-texel gutter around eleven islands needs a certain atlas
    #     whatever the density is, so the attic's 2 texels/m lands on the same 128² as the
    #     bathroom's 8. Lightmaps for coarse cells are therefore not as cheap as their density
    #     suggests, and §22's 21-atlas budget should be read with that in mind.
    build_room_fixture()
    coarse = unwrap(DENSITIES["attic"], DEFAULT_GUTTER, DEFAULT_MAX_ATLAS)
    require(coarse["atlasSize"] == result["atlasSize"],
            f"{DENSITIES['attic']} texels/m lands on the same {coarse['atlasSize']}² atlas: below "
            f"about {DEFAULT_GUTTER * GUTTER_HEADROOM} texels the GUTTER sets the size, not the "
            f"density")
    require(coarse["minDensity"] > DENSITIES["attic"] * 2,
            f"...so the attic is over-resolved at {coarse['minDensity']:.2f} texels/m against the "
            f"{DENSITIES['attic']} asked for")

    # 6. The overlap check must FIRE. Two faces are moved on top of each other in UV space, which
    #    is exactly what a bad pack produces and what no other check here can see.
    objects = _mesh_objects()
    layer = objects[0].data.uv_layers[LIGHTMAP_UV]
    first = list(objects[0].data.polygons)[0]
    second = list(objects[0].data.polygons)[1]
    for a, b in zip(first.loop_indices, second.loop_indices):
        layer.data[b].uv = layer.data[a].uv
    records = face_records(objects)
    islands = island_of_face(objects)
    _, overlaps = rasterise(records, islands, coarse["atlasSize"])
    require(any("share one light sample" in p for p in overlaps),
            f"two faces stacked in UV space are caught ({len(overlaps)} texel(s))")

    # 7. The gutter check must fire too, on a layout where two islands touch.
    build_room_fixture()
    tight = unwrap(DENSITIES["small_room"], 1, DEFAULT_MAX_ATLAS)
    objects = _mesh_objects()
    records = face_records(objects)
    islands = island_of_face(objects)
    grid, _ = rasterise(records, islands, tight["atlasSize"])
    _, violations = measure_gutter(grid, tight["atlasSize"], DEFAULT_GUTTER)
    require(violations,
            f"a 1-texel gutter is reported as violating a 4-texel requirement "
            f"({len(violations)} place(s))")

    # 8. Repeatability: the same input twice gives the same UVs, to the bit.
    build_room_fixture()
    a = unwrap(DENSITIES["small_room"], DEFAULT_GUTTER, DEFAULT_MAX_ATLAS)
    first_uvs = [tuple(item.uv) for item in _mesh_objects()[0].data.uv_layers[LIGHTMAP_UV].data]
    build_room_fixture()
    b = unwrap(DENSITIES["small_room"], DEFAULT_GUTTER, DEFAULT_MAX_ATLAS)
    second_uvs = [tuple(item.uv) for item in _mesh_objects()[0].data.uv_layers[LIGHTMAP_UV].data]
    require(a["atlasSize"] == b["atlasSize"] and first_uvs == second_uvs,
            f"two runs produce identical UVs ({len(first_uvs)} loops, "
            f"{a['atlasSize']}² both times)")

    # 9. A mesh with no UV layer at all is refused: the albedo channel must exist first.
    bpy.ops.wm.read_factory_settings(use_empty=True)
    mesh = bpy.data.meshes.new("bare")
    mesh.from_pydata([(0, 0, 0), (1, 0, 0), (1, 1, 0)], [], [[0, 1, 2]])
    mesh.update()
    bpy.context.collection.objects.link(bpy.data.objects.new("bare", mesh))
    try:
        unwrap(DEFAULT_DENSITY, DEFAULT_GUTTER, DEFAULT_MAX_ATLAS)
        require(False, "a mesh with no UV layer is refused")
    except RuntimeError as error:
        require("no UV layer" in str(error), f"a mesh with no UV layer is refused: {error}")

    print(f"lightmap_unwrap: {'FAILED' if failures else 'selftest passed'}.")
    return 1 if failures else 0


# ---------------------------------------------------------------------------------------- main ----

def main() -> int:
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if "--selftest" in arguments:
        return selftest()

    positional = [a for a in arguments if not a.startswith("--")]
    options = {a.split("=", 1)[0]: a.split("=", 1)[1] for a in arguments
               if a.startswith("--") and "=" in a}
    if len(positional) != 2:
        print(__doc__.splitlines()[0], file=sys.stderr)
        print("usage: lightmap_unwrap.py IN.glb OUT.glb [--density=N] [--gutter=N]",
              file=sys.stderr)
        return 2

    source, destination = positional
    density = float(options.get("--density", DEFAULT_DENSITY))
    gutter = int(options.get("--gutter", DEFAULT_GUTTER))
    max_atlas = int(options.get("--max-atlas", DEFAULT_MAX_ATLAS))

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=source)
    result = unwrap(density, gutter, max_atlas)
    bpy.ops.export_scene.gltf(filepath=destination, export_format="GLB",
                              export_apply=False, export_yup=True)
    result["output"] = os.path.basename(destination)

    report(f"{result['faces']} face(s) in {result['islands']} island(s), "
           f"{result['worldArea']:.1f} m² -> {result['atlasSize']}² atlas "
           f"at {result['minDensity']:.2f}-{result['maxDensity']:.2f} texels/m "
           f"({result['utilisation'] * 100:.0f} % used)")
    if options.get("--json"):
        with open(options["--json"], "w", encoding="utf-8") as handle:
            json.dump(result, handle, indent=2, sort_keys=True)
            handle.write("\n")
    for problem in result["problems"]:
        print(f"  FAIL  {problem}")
    return 1 if result["problems"] else 0


if __name__ == "__main__":
    # Blender does not propagate a script's exit status; see tools/blender/blender_env.py, which
    # takes this sentinel as the authority and treats a missing one as a failure. Without it this
    # tool reports failure on a clean run -- so its CI gate was red for a passing selftest, and
    # could never have gone red for a failing one either.
    try:
        _status = main()
    except SystemExit as _exit:
        _status = int(_exit.code or 0)
    except BaseException:  # noqa: BLE001
        import traceback

        traceback.print_exc()
        print("lightmap_unwrap: EXIT 1")
        raise
    print(f"lightmap_unwrap: EXIT {_status}")
    sys.exit(_status)
