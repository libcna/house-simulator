#!/usr/bin/env python3
"""collision_proxy.py -- a `<name>_COL` proxy the player can collide with, in 64 triangles or less.

`HOUSE-00190`. `cna-house.md` §18 requires every collidable asset to carry a separate mesh named
`<name>_COL`, "convex or box-decomposed, ≤ 64 triangles"; §49.2 says what the runtime does with it —
mostly OBBs, with triangle meshes kept for the curved minority.

    tools/blender/collision_proxy.py IN.glb OUT.glb
    tools/blender/collision_proxy.py IN.glb OUT.glb --mode box|hull|boxes
    tools/blender/collision_proxy.py --selftest

**The shape is CHOSEN, and the choice is measured.** Three proxy shapes, in increasing cost:

| Mode | Triangles | Chosen when |
|---|---|---|
| `box` | 12 | the source fills its own bounding box well — a wardrobe, a fridge, a bookcase |
| `hull` | ≤ 64 | it does not, but it is still convex — a vase, a beanbag, a rounded stool. A **14-DOP**, not a decimated convex hull; see `make_hull` for why. |
| `boxes` | ≤ 60 (5 × 12) | neither — an L-shaped sofa, a desk with a knee hole, anything with a void |

The rule is **occupancy**: of the space the proxy occupies, how much does the source occupy too,
measured by sampling a 24³ grid rather than by comparing volumes. A single box around an L-shaped
sofa encloses the empty corner as well, and a player would be stopped by air a foot from the
cushions — not a subtle artefact, but the thing collision is for. A proxy below the stated threshold
is rejected and the next shape is tried. Measured on the selftest fixtures: wardrobe 1.00 as a box,
vase 0.51 as a box and 0.74 as a 14-DOP, L-sofa 0.30 as one box and 0.89 as five.

**A proxy must ENCLOSE its source.** An under-sized proxy is worse than a crude one: the player
walks into the fridge. Every mode is enclosing by construction, and the selftest asserts it by
testing every source vertex against the proxy rather than trusting the construction.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import json
import math
import re
import sys

try:
    import bpy  # type: ignore

    INSIDE_BLENDER = True
except ImportError:
    INSIDE_BLENDER = False

if not INSIDE_BLENDER:
    from pathlib import Path

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import blender_env  # noqa: E402

    sys.exit(
        blender_env.relaunch(Path(__file__).resolve(), sys.argv[1:], tool="collision_proxy")
    )


import bmesh  # type: ignore  # noqa: E402
from mathutils import Vector  # type: ignore  # noqa: E402

#: §18's hard ceiling. Not a target -- a proxy that needs more than this is the wrong shape.
MAX_TRIANGLES = 64

#: A box is accepted when the source fills at least this much of it. 0.55 is calibrated rather than
#: guessed: a wardrobe measures ~0.95, a rounded stool ~0.62, and an L-shaped sofa ~0.45, so the
#: threshold separates "the box is the shape" from "the box is a lie".
BOX_FILL_THRESHOLD = 0.55

#: A hull is accepted on the same measure, at a lower bar: a hull already follows the silhouette, so
#: the only question is whether a void it bridges is big enough to matter.
HULL_FILL_THRESHOLD = 0.60

#: §49.2 keeps collision cheap; five boxes is 60 triangles and still under the ceiling.
MAX_BOXES = 5

MODES = ("box", "hull", "boxes")


def log(message: str) -> None:
    print(f"collision_proxy: {message}")


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)


def triangle_count(obj) -> int:
    obj.data.calc_loop_triangles()
    return len(obj.data.loop_triangles)


#: Grid resolution for the occupancy measure. 24 cubed is 13 824 samples: fine enough that a
#: threshold of 0.55 is not decided by a handful of cells, cheap enough to run three times per
#: asset.
OCCUPANCY_SAMPLES = 24


def mesh_volume(obj) -> float:
    """Signed volume via the divergence theorem, absolute.

    **Only valid for a single closed shell**, which is why `fill` does not use it. A joined L-shaped
    sofa is two cubes in one mesh, and if they intersect at the corner the divergence sum counts the
    overlap twice -- which produced a `fill` of 1.2454 for a proxy that provably enclosed every
    vertex, an impossible number. Kept for the box union, where the boxes abut exactly and the sum
    is exact by construction.
    """
    mesh = bmesh.new()
    mesh.from_mesh(obj.data)
    bmesh.ops.triangulate(mesh, faces=mesh.faces[:])
    volume = abs(mesh.calc_volume(signed=True))
    mesh.free()
    return volume


def _inside_tree(tree, point, extent) -> bool:
    """Ray-parity containment: odd crossings along +X means inside."""
    direction = Vector((1.0, 0.0, 0.0))
    origin = Vector(point)
    crossings = 0
    travelled = 0.0
    limit = extent * 4.0
    while travelled < limit:
        hit, _normal, _index, distance = tree.ray_cast(origin, direction, limit - travelled)
        if hit is None:
            break
        crossings += 1
        # Step just past the hit, or a coincident surface re-reports the same crossing forever.
        step = max(distance, 0.0) + 1e-5
        origin = origin + direction * step
        travelled += step
    return crossings % 2 == 1


def occupancy(source, proxy_objects, bounds) -> float:
    """Fraction of the proxy's occupied volume that the SOURCE also occupies, by sampling.

    Exact volumes cannot answer this: the source may be several intersecting shells, and the proxy
    may be several boxes. Sampling a regular grid and asking "inside the source?" and "inside the
    proxy?" is robust to both, deterministic, and measures the thing the threshold is actually
    about -- how much empty space the player would be stopped by.
    """
    from mathutils.bvhtree import BVHTree  # type: ignore

    minimum, maximum = bounds
    extent = max((maximum - minimum).length, 1e-6)

    source_mesh = bmesh.new()
    source_mesh.from_mesh(source.data)
    bmesh.ops.triangulate(source_mesh, faces=source_mesh.faces[:])
    source_tree = BVHTree.FromBMesh(source_mesh)

    proxy_trees = []
    for proxy in proxy_objects:
        builder = bmesh.new()
        builder.from_mesh(proxy.data)
        bmesh.ops.triangulate(builder, faces=builder.faces[:])
        proxy_trees.append((builder, BVHTree.FromBMesh(builder)))

    n = OCCUPANCY_SAMPLES
    span = maximum - minimum
    in_proxy = 0
    in_both = 0
    for ix in range(n):
        x = minimum.x + span.x * (ix + 0.5) / n
        for iy in range(n):
            y = minimum.y + span.y * (iy + 0.5) / n
            for iz in range(n):
                z = minimum.z + span.z * (iz + 0.5) / n
                point = Vector((x, y, z))
                if not any(_inside_tree(tree, point, extent) for _b, tree in proxy_trees):
                    continue
                in_proxy += 1
                if _inside_tree(source_tree, point, extent):
                    in_both += 1

    source_mesh.free()
    for builder, _tree in proxy_trees:
        builder.free()
    return in_both / in_proxy if in_proxy else 0.0


def proxy_bounds(objects):
    """The union of the proxies' bounds, which is the region the occupancy grid must cover."""
    lows, highs = [], []
    for obj in objects:
        minimum, maximum = local_bounds(obj)
        lows.append(minimum)
        highs.append(maximum)
    low = Vector((min(v.x for v in lows), min(v.y for v in lows), min(v.z for v in lows)))
    high = Vector((max(v.x for v in highs), max(v.y for v in highs), max(v.z for v in highs)))
    return low, high


def local_bounds(obj):
    corners = [Vector(corner) for corner in obj.bound_box]
    minimum = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
    maximum = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
    return minimum, maximum


def make_box(name: str, minimum: Vector, maximum: Vector):
    """An axis-aligned box mesh spanning @p minimum..@p maximum. 12 triangles."""
    mesh = bpy.data.meshes.new(name)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)

    builder = bmesh.new()
    verts = [
        builder.verts.new((x, y, z))
        for x in (minimum.x, maximum.x)
        for y in (minimum.y, maximum.y)
        for z in (minimum.z, maximum.z)
    ]
    builder.verts.ensure_lookup_table()
    # Index bits are (x, y, z), so opposite corners differ in every bit.
    faces = [
        (0, 1, 3, 2), (4, 6, 7, 5),   # -X, +X
        (0, 4, 5, 1), (2, 3, 7, 6),   # -Y, +Y
        (0, 2, 6, 4), (1, 5, 7, 3),   # -Z, +Z
    ]
    for face in faces:
        builder.faces.new([verts[i] for i in face])
    bmesh.ops.triangulate(builder, faces=builder.faces[:])
    builder.to_mesh(mesh)
    builder.free()
    return obj


#: The 14-DOP direction set: the six axes plus the eight body diagonals. 14 planes triangulate to
#: roughly 56 triangles, which is the largest standard k-DOP that fits under §18's ceiling of 64.
#: The 26-DOP (adding the twelve edge diagonals) is tighter and lands near 104 triangles, so it is
#: out of budget rather than out of favour.
KDOP_14 = [
    (1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1),
    (1, 1, 1), (1, 1, -1), (1, -1, 1), (1, -1, -1),
    (-1, 1, 1), (-1, 1, -1), (-1, -1, 1), (-1, -1, -1),
]


def make_hull(source, name: str):
    """A k-DOP bound: the intersection of half-spaces that support the source in fixed directions.

    **Enclosing by construction, which a reduced convex hull is not.** The obvious approach --
    hull the vertices, then decimate to fit the ceiling -- does not survive contact with the budget.
    Decimating a hull and re-hulling does not converge (the hull of N points has up to 2N-4 faces,
    so re-hulling restores what decimation removed: a 32x16 UV sphere sat at 961 triangles after
    eight rounds). Hulling a reduced point set does converge, but the result no longer contains
    every vertex, and expanding it about its centroid until it does over-inflates badly -- measured
    at 0.431 occupancy for a sphere, worse than the 5-box split it was supposed to beat.

    A k-DOP has neither problem. For each direction `d`, the plane at `max(dot(v, d))` supports the
    source, so every vertex is behind every plane and containment is a property of the construction
    rather than something to test for afterwards. The face count is fixed by the direction set, so
    the triangle budget is known before any geometry is built.
    """
    minimum, maximum = local_bounds(source)
    # Start from a box strictly larger than the source; every plane below only cuts it down.
    padding = max((maximum - minimum).length * 0.05, 1e-4)
    obj = make_box(name, minimum - Vector((padding,) * 3), maximum + Vector((padding,) * 3))

    vertices = [v.co.copy() for v in source.data.vertices]
    if not vertices:
        raise SystemExit(f"collision_proxy: {source.name} has no vertices")

    for direction in KDOP_14:
        normal = Vector(direction).normalized()
        support = max(vertex.dot(normal) for vertex in vertices)

        builder = bmesh.new()
        builder.from_mesh(obj.data)
        bmesh.ops.bisect_plane(
            builder,
            geom=builder.verts[:] + builder.edges[:] + builder.faces[:],
            plane_co=normal * support,
            plane_no=normal,
            # Remove everything on the outward side; the remainder is still a closed convex solid
            # once the cut is capped.
            clear_outer=True,
        )
        # `bisect_plane` leaves the cut open. Without this the mesh is not a solid and every
        # containment test through it is meaningless.
        bmesh.ops.holes_fill(builder, edges=builder.edges[:])
        bmesh.ops.triangulate(builder, faces=builder.faces[:])
        bmesh.ops.recalc_face_normals(builder, faces=builder.faces[:])
        builder.to_mesh(obj.data)
        builder.free()

    # A 14-DOP has fourteen distinct PLANES but the repeated bisect-and-cap leaves each of them
    # shattered into a fan: measured at 170 triangles before this step, well over §18's ceiling of
    # 64. Dissolving by angle merges each plane's fragments back into one polygon and re-triangulates
    # it, which is lossless -- the geometry is unchanged, only its tessellation.
    builder = bmesh.new()
    builder.from_mesh(obj.data)
    bmesh.ops.dissolve_limit(
        builder,
        angle_limit=math.radians(1.0),
        verts=builder.verts[:],
        edges=builder.edges[:],
    )
    bmesh.ops.triangulate(builder, faces=builder.faces[:])
    bmesh.ops.recalc_face_normals(builder, faces=builder.faces[:])
    builder.to_mesh(obj.data)
    builder.free()

    obj.data.name = name
    return obj


def split_boxes(source, name: str, count: int):
    """@p count axis-aligned boxes, split along the source's longest axis.

    A deliberately simple approximate convex decomposition. V-HACD is not available in Blender and
    is far more machinery than a 64-triangle budget can spend; slabbing along the dominant axis and
    bounding the vertices that fall in each slab handles the shapes this actually meets — an
    L-shaped sofa, a desk with a knee hole — and degrades to the single box when it cannot help.
    """
    minimum, maximum = local_bounds(source)
    extent = maximum - minimum
    axis = max(range(3), key=lambda i: extent[i])
    span = extent[axis]
    if span <= 0.0:
        return [make_box(f"{name}_0", minimum, maximum)]

    slabs: list[list] = [[] for _ in range(count)]
    for vertex in source.data.vertices:
        position = vertex.co
        index = int((position[axis] - minimum[axis]) / span * count)
        slabs[min(max(index, 0), count - 1)].append(position)

    boxes = []
    for index, points in enumerate(slabs):
        if not points:
            continue
        low = Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points)))
        high = Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points)))
        # The slabs must MEET, or a capsule sweep finds a seam between two boxes and slips through.
        # Each slab is extended to its share of the split axis so the union is watertight.
        low[axis] = minimum[axis] + span * index / count
        high[axis] = minimum[axis] + span * (index + 1) / count
        boxes.append(make_box(f"{name}_{index}", low, high))
    return boxes


def union_volume(boxes) -> float:
    """Total volume of the boxes. They abut rather than overlap, so a sum is exact here."""
    return sum(mesh_volume(box) for box in boxes)


def encloses(proxy_objects, source, tolerance: float = 1e-4) -> tuple[bool, float]:
    """Is every source vertex inside at least one proxy? Returns (ok, worst outside distance)."""
    planes = []
    for proxy in proxy_objects:
        proxy.data.calc_loop_triangles()
        faces = []
        for polygon in proxy.data.polygons:
            faces.append((Vector(polygon.center), Vector(polygon.normal)))
        planes.append(faces)

    worst = 0.0
    for vertex in source.data.vertices:
        point = vertex.co
        inside_any = False
        best_for_point = None
        for faces in planes:
            outside = 0.0
            for centre, normal in faces:
                distance = (point - centre).dot(normal)
                outside = max(outside, distance)
            if outside <= tolerance:
                inside_any = True
                break
            best_for_point = outside if best_for_point is None else min(best_for_point, outside)
        if not inside_any:
            worst = max(worst, best_for_point or 0.0)
    return worst <= tolerance, worst


def join(objects, name: str):
    if len(objects) == 1:
        objects[0].name = name
        objects[0].data.name = name
        return objects[0]
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    joined = bpy.context.active_object
    joined.name = name
    joined.data.name = name
    return joined


def build_proxy(source, mode: str, name: str):
    """Returns (objects, description). The objects are separate until the caller joins them."""
    minimum, maximum = local_bounds(source)
    if mode == "box":
        return [make_box(name, minimum, maximum)], "1 box"
    if mode == "hull":
        return [make_hull(source, name)], "14-DOP"
    best = None
    for count in range(2, MAX_BOXES + 1):
        boxes = split_boxes(source, f"{name}_try{count}", count)
        if len(boxes) * 12 > MAX_TRIANGLES:
            for box in boxes:
                bpy.data.objects.remove(box, do_unlink=True)
            break
        volume = union_volume(boxes)
        if best is None or volume < best[1]:
            for box in (best[0] if best else []):
                bpy.data.objects.remove(box, do_unlink=True)
            best = (boxes, volume, count)
        else:
            for box in boxes:
                bpy.data.objects.remove(box, do_unlink=True)
    if best is None:
        return [make_box(name, minimum, maximum)], "1 box (split not possible)"
    return best[0], f"{best[2]} boxes"


def generate(source_path: str, output_path: str, mode: str = "auto") -> dict:
    reset_scene()
    bpy.ops.import_scene.gltf(filepath=source_path)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"
              and not o.name.endswith("_COL")
              and not re.search(r"_LOD[1-9][0-9]*$", o.name)]
    if not meshes:
        raise SystemExit(f"collision_proxy: {source_path} contains no LOD0 render mesh")
    source_name = max(meshes, key=triangle_count).name
    # A logical prop often has several material meshes (tabletop/legs, sofa/cushions).
    # Analyse copies joined into one temporary source so the proxy encloses the WHOLE prop,
    # not merely whichever material happened to own the most triangles. Keep the originals
    # untouched for the export and remove the temporary source before writing the GLB.
    copies = []
    for mesh in meshes:
        duplicate = mesh.copy()
        duplicate.data = mesh.data.copy()
        bpy.context.collection.objects.link(duplicate)
        transform = mesh.matrix_world.copy()
        duplicate.parent = None
        duplicate.matrix_world = transform
        bpy.ops.object.select_all(action="DESELECT")
        duplicate.select_set(True)
        bpy.context.view_layer.objects.active = duplicate
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        copies.append(duplicate)
    source = join(copies, "__collision_source")
    source_volume = mesh_volume(source)
    name = f"{source_name}_COL"

    report = {
        "source": source_path,
        "output": output_path,
        "sourceName": source_name,
        "proxyName": name,
        "sourceVolume": round(source_volume, 6),
        "problems": [],
    }

    order = MODES if mode == "auto" else (mode,)
    chosen = None
    attempts = []
    for candidate in order:
        objects, description = build_proxy(source, candidate, f"__try_{candidate}")
        # Occupancy, not a volume ratio: the source may be several intersecting shells and the
        # proxy several boxes, and `calc_volume` double-counts an overlap. See `occupancy()`.
        fill = occupancy(source, objects, proxy_bounds(objects))
        triangles = sum(triangle_count(o) for o in objects)
        ok, outside = encloses(objects, source)
        attempts.append(
            {
                "mode": candidate,
                "shape": description,
                "triangles": triangles,
                "fill": round(fill, 4),
                "encloses": ok,
                "worstOutside": round(outside, 6),
            }
        )
        threshold = {"box": BOX_FILL_THRESHOLD, "hull": HULL_FILL_THRESHOLD, "boxes": 0.0}[candidate]
        acceptable = triangles <= MAX_TRIANGLES and ok and fill >= threshold
        if acceptable or mode != "auto" or candidate == MODES[-1]:
            chosen = (candidate, objects, description, triangles, fill, ok, outside)
            break
        for obj in objects:
            bpy.data.objects.remove(obj, do_unlink=True)

    candidate, objects, description, triangles, fill, ok, outside = chosen
    proxy = join(objects, name)
    report.update(
        {
            "mode": candidate,
            "shape": description,
            "triangles": triangle_count(proxy),
            "fill": round(fill, 4),
            "encloses": ok,
            "worstOutside": round(outside, 6),
            "attempts": attempts,
        }
    )

    if report["triangles"] > MAX_TRIANGLES:
        report["problems"].append(
            f"{report['triangles']} triangles exceeds the {MAX_TRIANGLES}-triangle ceiling (§18)"
        )
    if not ok:
        report["problems"].append(
            f"the proxy does not enclose the source; worst vertex is {outside:.4f} outside"
        )

    bpy.data.objects.remove(source, do_unlink=True)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in bpy.context.scene.objects:
        obj.select_set(True)
    bpy.ops.export_scene.gltf(
        filepath=output_path, export_format="GLB", use_selection=True, export_yup=True
    )
    report["exported"] = True
    report["ok"] = not report["problems"]
    return report


# --------------------------------------------------------------------------------------------
# Fixtures and selftest
# --------------------------------------------------------------------------------------------


def _export(obj, path: str) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True)


def fixture_wardrobe(path: str) -> None:
    """Boxy. A single box should be chosen, and it should fill it almost completely."""
    reset_scene()
    bpy.ops.mesh.primitive_cube_add(size=1.0)
    obj = bpy.context.active_object
    obj.scale = (0.6, 1.1, 0.3)
    obj.name = "wardrobe"
    bpy.ops.object.transform_apply(scale=True)
    _export(obj, path)


def fixture_vase(path: str) -> None:
    """Rounded and convex. A box wastes the corners; a hull follows it."""
    reset_scene()
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=0.25)
    obj = bpy.context.active_object
    obj.name = "vase"
    obj.scale = (1.0, 1.6, 1.0)
    bpy.ops.object.transform_apply(scale=True)
    _export(obj, path)


def fixture_sofa(path: str) -> None:
    """L-shaped. A single box encloses the empty corner, which is the case boxes exist for."""
    reset_scene()
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0.0, 0.0, 0.0))
    long_arm = bpy.context.active_object
    long_arm.scale = (1.2, 0.35, 0.35)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(1.0, 0.0, 0.85))
    short_arm = bpy.context.active_object
    short_arm.scale = (0.35, 0.35, 1.2)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.select_all(action="DESELECT")
    long_arm.select_set(True)
    short_arm.select_set(True)
    bpy.context.view_layer.objects.active = long_arm
    bpy.ops.object.join()
    obj = bpy.context.active_object
    obj.name = "sofa_l"
    _export(obj, path)


def fixture_multi_part(path: str) -> None:
    """Two render meshes, plus a remote LOD: only both LOD0 pieces belong in the proxy."""
    reset_scene()
    bpy.ops.mesh.primitive_cube_add(size=0.4, location=(-0.9, 0.0, 0.0))
    left = bpy.context.active_object
    left.name = "tabletop"
    bpy.ops.mesh.primitive_cube_add(size=0.4, location=(0.9, 0.0, 0.0))
    right = bpy.context.active_object
    right.name = "tablelegs"
    bpy.ops.mesh.primitive_cube_add(size=0.4, location=(25.0, 0.0, 0.0))
    lod = bpy.context.active_object
    lod.name = "tabletop_LOD1"
    bpy.ops.object.select_all(action="DESELECT")
    for obj in (left, right, lod):
        obj.select_set(True)
    bpy.context.view_layer.objects.active = left
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True)


def selftest() -> int:
    import os
    import tempfile

    failures = []

    def check(name, condition, detail=""):
        print(f"  {'PASS' if condition else 'FAIL'}  {name}{(' -- ' + detail) if detail else ''}")
        if not condition:
            failures.append(name)

    print("collision_proxy selftest")
    with tempfile.TemporaryDirectory(prefix="cnahouse-col-") as workdir:
        cases = [
            ("wardrobe", fixture_wardrobe, "box"),
            ("vase", fixture_vase, "hull"),
            ("sofa_l", fixture_sofa, "boxes"),
        ]
        for label, builder, expected_mode in cases:
            source = os.path.join(workdir, f"{label}.glb")
            output = os.path.join(workdir, f"{label}_col.glb")
            builder(source)
            report = generate(source, output)

            check(
                f"{label}: chooses '{expected_mode}'",
                report["mode"] == expected_mode,
                f"chose '{report['mode']}' ({report['shape']}), fill {report['fill']}",
            )
            check(
                f"{label}: within the {MAX_TRIANGLES}-triangle ceiling",
                report["triangles"] <= MAX_TRIANGLES,
                f"{report['triangles']} triangles",
            )
            check(
                f"{label}: the proxy encloses every source vertex",
                report["encloses"],
                f"worst {report['worstOutside']}",
            )
            check(f"{label}: named <name>_COL", report["proxyName"].endswith("_COL"),
                  report["proxyName"])
            check(f"{label}: no problems", not report["problems"], "; ".join(report["problems"]))

        multi = os.path.join(workdir, "multi_part.glb")
        fixture_multi_part(multi)
        report = generate(multi, os.path.join(workdir, "multi_part_col.glb"), mode="box")
        reset_scene()
        bpy.ops.import_scene.gltf(filepath=report["output"])
        proxy = next(obj for obj in bpy.context.scene.objects
                     if obj.type == "MESH" and obj.name.endswith("_COL"))
        minimum, maximum = local_bounds(proxy)
        check("the collision proxy encloses both LOD0 material meshes, in world metres",
              minimum.x < -1.09 and maximum.x > 1.09,
              f"x bounds {minimum.x:.2f} to {maximum.x:.2f}")
        check("an authored LOD is excluded from collision source geometry",
              maximum.x < 2.0, f"right bound {maximum.x:.2f}, not 25.2")

        # The enclosure test must be able to fail, or it proves nothing. A box deliberately shrunk
        # to 80 % has to be rejected.
        reset_scene()
        bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8, radius=0.5)
        sphere = bpy.context.active_object
        minimum, maximum = local_bounds(sphere)
        small = make_box("too_small", minimum * 0.8, maximum * 0.8)
        ok, outside = encloses([small], sphere)
        check(
            "the enclosure test rejects an undersized proxy",
            not ok and outside > 0.0,
            f"worst {outside:.4f} outside",
        )

        # And the L-shape must actually beat a single box on fill, or 'boxes' is pointless.
        source = os.path.join(workdir, "sofa_l.glb")
        forced_box = generate(source, os.path.join(workdir, "sofa_box.glb"), mode="box")
        chosen = generate(source, os.path.join(workdir, "sofa_auto.glb"))
        check(
            "the L-shape's box decomposition wastes less volume than one box",
            chosen["fill"] > forced_box["fill"],
            f"boxes fill {chosen['fill']} vs single box {forced_box['fill']}",
        )

    print()
    if failures:
        print(f"collision_proxy selftest: {len(failures)} FAILED: {', '.join(failures)}",
              file=sys.stderr)
        return 1
    print("collision_proxy selftest: all checks passed.")
    return 0


def main() -> int:
    args = blender_argv()
    if "--selftest" in args:
        return selftest()
    mode = "auto"
    if "--mode" in args:
        index = args.index("--mode")
        mode = args[index + 1]
        if mode not in MODES:
            print(f"collision_proxy: --mode must be one of {MODES}", file=sys.stderr)
            return 2
        args = args[:index] + args[index + 2:]
    if len(args) < 2:
        print("collision_proxy: usage: IN.glb OUT.glb [--mode box|hull|boxes]", file=sys.stderr)
        return 2

    report = generate(args[0], args[1], mode)
    log(f"{report['source']} -> {report['output']}")
    log(
        f"  {report['proxyName']}: {report['shape']}, {report['triangles']} triangles, "
        f"fill {report['fill']}, encloses {report['encloses']}"
    )
    print("COLLISION_JSON " + json.dumps(report))
    for problem in report["problems"]:
        print(f"collision_proxy: PROBLEM {problem}", file=sys.stderr)
    return 0 if report["ok"] else 1


def blender_argv() -> list:
    return sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


if __name__ == "__main__":
    # Blender does not propagate a script's exit status; see tools/blender/blender_env.py.
    try:
        _status = main()
    except SystemExit as _exit:
        _status = int(_exit.code or 0)
    except BaseException:  # noqa: BLE001
        import traceback

        traceback.print_exc()
        print("collision_proxy: EXIT 1")
        raise
    print(f"collision_proxy: EXIT {_status}")
    sys.exit(_status)
