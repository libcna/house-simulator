#!/usr/bin/env python3
"""lod_gen.py -- LOD1 and LOD2 for a glTF model, by decimation, inside Blender.

`HOUSE-00189`. `cna-house.md` §26.1 fixes the ratios -- LOD1 at 0.35 of LOD0's triangles, LOD2 at
0.12 -- and §18 fixes the naming: the reduced meshes are extra meshes in the same `.glb`, called
`<name>_LOD1` and `<name>_LOD2`, so one file carries the whole ladder and nothing has to look for a
sibling that might be missing.

    blender --background --factory-startup --python tools/blender/lod_gen.py -- IN.glb OUT.glb
    blender --background --factory-startup --python tools/blender/lod_gen.py -- --selftest
    tools/blender/lod_gen.py IN.glb OUT.glb        # re-executes itself under Blender

**Run it either way.** Invoked as a plain script it finds `blender` and re-runs itself inside it,
so a caller does not have to know that this is a Blender tool; invoked by Blender it does the work.

Three things this does that a bare Decimate modifier does not:

* **Normal transfer.** Collapsing edges rotates the normals of the faces that survive, and on a
  smooth-shaded curve that reads as faceting the moment the LOD switches. The reduced mesh gets its
  normals transferred from LOD0 by nearest corner, so the shading at LOD1 matches LOD0's even where
  the geometry no longer does. Verified in the selftest by comparing mean normal deviation with the
  transfer on and off, because a `Data Transfer` modifier that silently did nothing would look
  exactly like one that worked.

* **UV preservation, checked rather than hoped for.** `COLLAPSE` interpolates UVs, but a mesh whose
  UV layer is dropped still decimates happily and only looks wrong once it is textured. Every output
  is asserted to carry the same UV layer names as its input, with in-range coordinates.

* **A silhouette measurement.** The ratios are a triangle budget, not a quality statement. A
  decimation that hits 0.35 exactly while destroying the outline is a failure the triangle count
  cannot see, so the outline is rendered from 8 yaws and compared against LOD0's; the error is
  reported and gated.

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

    sys.exit(blender_env.relaunch(Path(__file__).resolve(), sys.argv[1:], tool="lod_gen"))


# --------------------------------------------------------------------------------------------
# Everything below runs inside Blender.
# --------------------------------------------------------------------------------------------
import bmesh  # type: ignore  # noqa: E402
from mathutils import Vector  # type: ignore  # noqa: E402

#: `cna-house.md` §26.1. Not parameters with defaults -- the renderer's LOD selection is built
#: around these numbers, so a per-asset override would make the distance thresholds wrong.
LOD_RATIOS = {"LOD1": 0.35, "LOD2": 0.12}

#: The triangle count below which §18 says a model carries no LODs at all: decimating a 200-triangle
#: chair costs a draw call's worth of switching to save nothing.
LOD_THRESHOLD_TRIANGLES = 4000

#: Silhouette error budget, as a fraction of the LOD0 silhouette area, measured over 8 yaws.
#: LOD2 is allowed more because it is only ever seen beyond 90 px of screen height (§26.1).
SILHOUETTE_BUDGET = {"LOD1": 0.03, "LOD2": 0.10}

#: Rasterisation resolution for the silhouette comparison. High enough that a 3 % error is tens of
#: thousands of pixels rather than a handful, low enough to stay instant.
SILHOUETTE_RESOLUTION = 256
SILHOUETTE_YAWS = 8


def log(message: str) -> None:
    print(f"lod_gen: {message}")


def argv_after_ddash() -> list[str]:
    return sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)


def triangle_count(obj) -> int:
    mesh = obj.data
    mesh.calc_loop_triangles()
    return len(mesh.loop_triangles)


def import_glb(path: str) -> list:
    bpy.ops.import_scene.gltf(filepath=path)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    if not meshes:
        raise SystemExit(f"lod_gen: {path} contains no mesh")
    return meshes


def triangulate(obj) -> None:
    """Triangulate in place, so triangle counts mean one thing throughout."""
    mesh = bmesh.new()
    mesh.from_mesh(obj.data)
    bmesh.ops.triangulate(mesh, faces=mesh.faces[:])
    mesh.to_mesh(obj.data)
    mesh.free()


def decimate(source, name: str, ratio: float):
    """A COLLAPSE-decimated copy of @p source at @p ratio, with normals transferred back."""
    copy = source.copy()
    copy.data = source.data.copy()
    copy.name = name
    copy.data.name = name
    bpy.context.collection.objects.link(copy)

    modifier = copy.modifiers.new(name="lod-decimate", type="DECIMATE")
    modifier.decimate_type = "COLLAPSE"
    modifier.ratio = ratio
    # UVs and any other loop data are interpolated rather than dropped. Off by default, and the
    # single most important line in this function for a textured asset.
    modifier.use_collapse_triangulate = True

    # The normal transfer has to happen AFTER decimation, so it is a second modifier reading the
    # original object rather than something applied to the source.
    transfer = copy.modifiers.new(name="lod-normals", type="DATA_TRANSFER")
    transfer.object = source
    transfer.use_loop_data = True
    transfer.data_types_loops = {"CUSTOM_NORMAL"}
    transfer.loop_mapping = "NEAREST_POLYNOR"

    # NOTE: no `use_auto_smooth`. It was removed in Blender 4.1 -- custom split normals are always
    # honoured now -- and setting it on 4.3 raises AttributeError rather than being ignored.

    context = bpy.context.evaluated_depsgraph_get()
    evaluated = copy.evaluated_get(context)
    baked = bpy.data.meshes.new_from_object(evaluated)
    copy.modifiers.clear()
    old = copy.data
    copy.data = baked
    # ORDER MATTERS: the old datablock is removed BEFORE the new one is renamed. Blender makes
    # datablock names unique on assignment, so renaming first -- while `old` still holds the name --
    # yields `armchair_LOD1.001`, and that suffix travels all the way into the exported glTF mesh
    # name. §18 names these meshes exactly; a consumer matching on `<name>_LOD1` would miss them.
    bpy.data.meshes.remove(old)
    copy.data.name = name
    return copy


def uv_layer_names(obj) -> list[str]:
    return [layer.name for layer in obj.data.uv_layers]


def uv_in_range(obj) -> bool:
    """Every UV finite and inside a sane range. Catches an interpolation that produced NaNs."""
    for layer in obj.data.uv_layers:
        for datum in layer.data:
            u, v = datum.uv
            if not (math.isfinite(u) and math.isfinite(v)) or not (-10.0 < u < 10.0 and -10.0 < v < 10.0):
                return False
    return True


def mean_normal_deviation(obj, reference) -> float:
    """Mean angle, in degrees, between each CORNER normal and the nearest reference surface normal.

    **Corner normals, not vertex normals**, and the difference is the whole point. A `Data Transfer`
    modifier writes CUSTOM SPLIT normals, which live per loop; `MeshVertex.normal` is recomputed
    from the geometry and never shows them. Measuring the wrong one made a working transfer and a
    silently-inert one produce byte-identical numbers -- which is exactly what happened the first
    time this was written, and is why the selftest compares transfer-on against transfer-off rather
    than checking a single absolute figure.
    """
    from mathutils.bvhtree import BVHTree  # type: ignore

    reference_mesh = bmesh.new()
    reference_mesh.from_mesh(reference.data)
    bmesh.ops.triangulate(reference_mesh, faces=reference_mesh.faces[:])
    tree = BVHTree.FromBMesh(reference_mesh)

    mesh = obj.data
    corner_normals = mesh.corner_normals
    total = 0.0
    count = 0
    for index, loop in enumerate(mesh.loops):
        position = mesh.vertices[loop.vertex_index].co
        location, normal, _index, _distance = tree.find_nearest(position)
        if location is None or normal.length == 0.0:
            continue
        own = Vector(corner_normals[index].vector)
        if own.length == 0.0:
            continue
        cosine = max(-1.0, min(1.0, own.normalized().dot(normal.normalized())))
        total += math.degrees(math.acos(cosine))
        count += 1
    reference_mesh.free()
    return total / count if count else 0.0


def silhouette_mask(obj, yaw: float, bounds) -> set:
    """Orthographic silhouette as a set of occupied pixels, rasterised from the triangles.

    Rendering through EEVEE would drag a render engine, lighting and colour management into a
    question that is purely geometric. Projecting the triangles and filling them answers exactly
    "what outline does this present", which is what §26.1's LOD switch actually trades away.
    """
    centre, radius = bounds
    cos_y, sin_y = math.cos(yaw), math.sin(yaw)
    resolution = SILHOUETTE_RESOLUTION
    scale = (resolution - 2) / (2.0 * radius)

    mesh = obj.data
    mesh.calc_loop_triangles()
    occupied: set = set()

    for triangle in mesh.loop_triangles:
        points = []
        for index in triangle.vertices:
            co = mesh.vertices[index].co - centre
            # Yaw about Y, then orthographic onto XY. Y is up (§9's convention).
            x = co.x * cos_y - co.z * sin_y
            y = co.y
            points.append((x * scale + resolution / 2.0, y * scale + resolution / 2.0))
        _fill_triangle(points, occupied, resolution)
    return occupied


def _fill_triangle(points, occupied: set, resolution: int) -> None:
    (x0, y0), (x1, y1), (x2, y2) = points
    min_x = max(0, int(math.floor(min(x0, x1, x2))))
    max_x = min(resolution - 1, int(math.ceil(max(x0, x1, x2))))
    min_y = max(0, int(math.floor(min(y0, y1, y2))))
    max_y = min(resolution - 1, int(math.ceil(max(y0, y1, y2))))
    area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
    if abs(area) < 1e-12:
        return
    for py in range(min_y, max_y + 1):
        for px in range(min_x, max_x + 1):
            cx, cy = px + 0.5, py + 0.5
            w0 = ((x1 - x0) * (cy - y0) - (cx - x0) * (y1 - y0)) / area
            w1 = ((cx - x0) * (y2 - y0) - (x2 - x0) * (cy - y0)) / area
            if w0 >= 0.0 and w1 >= 0.0 and w0 + w1 <= 1.0:
                occupied.add((px, py))


def object_bounds(obj):
    coordinates = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    minimum = Vector((min(c.x for c in coordinates), min(c.y for c in coordinates), min(c.z for c in coordinates)))
    maximum = Vector((max(c.x for c in coordinates), max(c.y for c in coordinates), max(c.z for c in coordinates)))
    centre = (minimum + maximum) * 0.5
    radius = max((maximum - minimum).length * 0.5, 1e-6)
    return centre, radius


def silhouette_error(candidate, reference) -> float:
    """Symmetric difference over union of the two silhouettes, averaged over 8 yaws."""
    bounds = object_bounds(reference)
    total = 0.0
    for step in range(SILHOUETTE_YAWS):
        yaw = 2.0 * math.pi * step / SILHOUETTE_YAWS
        a = silhouette_mask(reference, yaw, bounds)
        b = silhouette_mask(candidate, yaw, bounds)
        if not a:
            continue
        total += len(a ^ b) / float(len(a))
    return total / SILHOUETTE_YAWS


def generate(source_path: str, output_path: str, *, strict: bool = True) -> dict:
    reset_scene()
    meshes = import_glb(source_path)
    if len(meshes) > 1:
        log(f"note: {len(meshes)} meshes; LODs are generated for the largest, '{meshes[0].name}'")
    base = max(meshes, key=triangle_count)
    triangulate(base)

    base_triangles = triangle_count(base)
    base_uvs = uv_layer_names(base)
    report = {
        "source": source_path,
        "output": output_path,
        "base": {"name": base.name, "triangles": base_triangles, "uvLayers": base_uvs},
        "levels": {},
        "problems": [],
    }

    if base_triangles <= LOD_THRESHOLD_TRIANGLES:
        # §18: below the threshold a model carries LOD0 only. Said out loud rather than silently
        # producing LODs nobody asked for.
        report["skipped"] = (
            f"{base_triangles} triangles is at or below the {LOD_THRESHOLD_TRIANGLES}-triangle "
            f"threshold; cna-house.md §18 gives such a model LOD0 only"
        )
        log(report["skipped"])

    if "skipped" not in report:
        for level, ratio in LOD_RATIOS.items():
            name = f"{base.name}_{level}"
            reduced = decimate(base, name, ratio)
            triangles = triangle_count(reduced)
            achieved = triangles / base_triangles
            uvs = uv_layer_names(reduced)
            error = silhouette_error(reduced, base)

            entry = {
                "name": name,
                "triangles": triangles,
                "targetRatio": ratio,
                "achievedRatio": round(achieved, 4),
                "uvLayers": uvs,
                "silhouetteError": round(error, 4),
                "silhouetteBudget": SILHOUETTE_BUDGET[level],
            }
            report["levels"][level] = entry

            # The ratio is a target that the collapse algorithm approaches, not a promise it can
            # keep exactly -- a mesh whose topology forbids a collapse keeps the triangle. 20 %
            # relative slack; outside that, something is wrong with the source, not the ratio.
            if not (ratio * 0.8 <= achieved <= ratio * 1.2):
                report["problems"].append(
                    f"{level}: {achieved:.3f} of LOD0 triangles, target {ratio:.2f} (+/-20%)"
                )
            if uvs != base_uvs:
                report["problems"].append(f"{level}: UV layers {uvs} != LOD0's {base_uvs}")
            elif not uv_in_range(reduced):
                report["problems"].append(f"{level}: UV coordinates are not finite / in range")
            if error > SILHOUETTE_BUDGET[level]:
                report["problems"].append(
                    f"{level}: silhouette error {error:.4f} over the "
                    f"{SILHOUETTE_BUDGET[level]:.2f} budget"
                )

    for obj in bpy.context.scene.objects:
        obj.select_set(True)
    bpy.ops.export_scene.gltf(
        filepath=output_path,
        export_format="GLB",
        use_selection=True,
        export_apply=False,
        # Determinism: no timestamps, no per-run names in the output.
        export_yup=True,
    )
    report["exported"] = os.path.isfile(output_path)
    if strict and report["problems"]:
        report["ok"] = False
    else:
        report["ok"] = not report["problems"]
    return report


# --------------------------------------------------------------------------------------------
# The fixture, and the selftest that uses it
# --------------------------------------------------------------------------------------------


def build_fixture(path: str) -> None:
    """A deterministic armchair: the 'large furniture' category §26.2 gives LOD0/1.

    Not a sphere and not the fallback box. Decimation has to be shown to cope with what it will
    actually meet -- a mix of flat panels that must survive and curved cushions that must not
    collapse into facets, a real UV unwrap, and enough triangles for a 0.12 ratio to mean anything.
    Everything here is parametric and seeded, so the fixture is the same on every machine.
    """
    reset_scene()
    parts = []

    def box(name, location, scale, bevel=0.0, subdivide=0):
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
        obj = bpy.context.active_object
        obj.name = name
        obj.scale = scale
        if subdivide:
            modifier = obj.modifiers.new("sub", "SUBSURF")
            modifier.levels = subdivide
            modifier.render_levels = subdivide
            bpy.ops.object.modifier_apply(modifier="sub")
        if bevel:
            modifier = obj.modifiers.new("bevel", "BEVEL")
            modifier.width = bevel
            modifier.segments = 4
            bpy.ops.object.modifier_apply(modifier="bevel")
        parts.append(obj)
        return obj

    # Seat, back and two arms: flat-ish panels with rounded edges -- the shapes whose silhouette a
    # bad decimation ruins first.
    box("seat", (0.0, 0.42, 0.0), (0.90, 0.16, 0.85), bevel=0.03, subdivide=2)
    box("back", (0.0, 0.75, -0.36), (0.90, 0.62, 0.14), bevel=0.03, subdivide=2)
    box("arm_l", (-0.46, 0.60, 0.0), (0.14, 0.34, 0.85), bevel=0.03, subdivide=2)
    box("arm_r", (0.46, 0.60, 0.0), (0.14, 0.34, 0.85), bevel=0.03, subdivide=2)
    # Four legs: thin features that a naive decimation deletes outright, which the silhouette
    # measurement is there to notice.
    for index, (x, z) in enumerate([(-0.38, -0.34), (0.38, -0.34), (-0.38, 0.34), (0.38, 0.34)]):
        box(f"leg_{index}", (x, 0.17, z), (0.08, 0.34, 0.08), bevel=0.01, subdivide=1)

    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    chair = bpy.context.active_object
    chair.name = "armchair"
    chair.data.name = "armchair"

    triangulate(chair)
    bpy.ops.object.select_all(action="DESELECT")
    chair.select_set(True)
    bpy.context.view_layer.objects.active = chair
    # Smooth-shaded with an angle threshold, which is how upholstery is actually authored: the
    # cushion curves interpolate and the panel edges stay sharp. It also makes split normals carry
    # information -- on a wholly flat-shaded mesh a normal transfer has nothing to preserve, and the
    # selftest's comparison would be vacuous.
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(40.0))
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    # A real unwrap, not a projection from view: the UVs have to be worth preserving for
    # "UV preservation" to be a claim about anything.
    bpy.ops.uv.smart_project(angle_limit=1.15, island_margin=0.02)
    bpy.ops.object.mode_set(mode="OBJECT")

    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True)
    log(f"fixture: {path}, {triangle_count(chair)} triangles, UV layers {uv_layer_names(chair)}")


def _glb_mesh_names(path: str) -> list:
    """Mesh names as they appear in the exported file, read without Blender's help."""
    import struct

    data = open(path, "rb").read()
    length, _kind = struct.unpack_from("<II", data, 12)
    document = json.loads(data[20:20 + length])
    return [mesh.get("name", "") for mesh in document.get("meshes", [])]


def selftest() -> int:
    import tempfile

    failures = []

    def check(name, condition, detail=""):
        print(f"  {'PASS' if condition else 'FAIL'}  {name}{(' -- ' + detail) if detail else ''}")
        if not condition:
            failures.append(name)

    print("lod_gen selftest")
    with tempfile.TemporaryDirectory(prefix="cnahouse-lod-") as workdir:
        fixture = os.path.join(workdir, "armchair.glb")
        build_fixture(fixture)

        output = os.path.join(workdir, "armchair_lods.glb")
        report = generate(fixture, output, strict=False)

        base = report["base"]["triangles"]
        check("the fixture is big enough for LODs to mean anything",
              base > LOD_THRESHOLD_TRIANGLES, f"{base} triangles")
        check("LOD0 has a UV layer", bool(report["base"]["uvLayers"]),
              str(report["base"]["uvLayers"]))

        for level, ratio in LOD_RATIOS.items():
            entry = report["levels"][level]
            check(f"{level} hits ~{int(ratio * 100)}% of LOD0 triangles",
                  ratio * 0.8 <= entry["achievedRatio"] <= ratio * 1.2,
                  f"{entry['triangles']} tris = {entry['achievedRatio']:.3f}")
            check(f"{level} keeps the UV layers",
                  entry["uvLayers"] == report["base"]["uvLayers"],
                  str(entry["uvLayers"]))
            check(f"{level} silhouette error within budget",
                  entry["silhouetteError"] <= entry["silhouetteBudget"],
                  f"{entry['silhouetteError']:.4f} <= {entry['silhouetteBudget']}")

        check("the output .glb was written", report["exported"])

        # The exported MESH names, not just the node names. Blender's datablock uniquifying put a
        # `.001` on these until the removal order in `decimate()` was fixed, and it is invisible
        # from inside Blender -- it only shows up in the file §18's naming rule is about.
        exported_meshes = _glb_mesh_names(output)
        expected = [report["base"]["name"]] + [
            f"{report['base']['name']}_{level}" for level in LOD_RATIOS
        ]
        check(
            "exported mesh names match §18's <name>_LOD1 / _LOD2 exactly",
            sorted(exported_meshes) == sorted(expected),
            f"{sorted(exported_meshes)} vs {sorted(expected)}",
        )
        check("no problems reported", not report["problems"], "; ".join(report["problems"]))

        # The normal transfer is proved by comparing WITH it against WITHOUT it. A Data Transfer
        # modifier that silently did nothing would otherwise pass every check above.
        reset_scene()
        meshes = import_glb(fixture)
        base_obj = max(meshes, key=triangle_count)
        triangulate(base_obj)
        with_transfer = decimate(base_obj, "with", LOD_RATIOS["LOD1"])
        plain = base_obj.copy()
        plain.data = base_obj.data.copy()
        plain.name = "plain"
        bpy.context.collection.objects.link(plain)
        modifier = plain.modifiers.new("d", "DECIMATE")
        modifier.decimate_type = "COLLAPSE"
        modifier.ratio = LOD_RATIOS["LOD1"]
        depsgraph = bpy.context.evaluated_depsgraph_get()
        plain.data = bpy.data.meshes.new_from_object(plain.evaluated_get(depsgraph))
        plain.modifiers.clear()

        deviation_with = mean_normal_deviation(with_transfer, base_obj)
        deviation_without = mean_normal_deviation(plain, base_obj)
        check(
            "normal transfer reduces deviation from LOD0's shading",
            deviation_with < deviation_without,
            f"{deviation_with:.3f} deg with, {deviation_without:.3f} deg without",
        )

        # A model at or below the threshold must be left alone, and say so.
        small = os.path.join(workdir, "small.glb")
        reset_scene()
        bpy.ops.mesh.primitive_cube_add(size=1.0)
        cube = bpy.context.active_object
        cube.select_set(True)
        bpy.ops.export_scene.gltf(filepath=small, export_format="GLB", use_selection=True)
        small_report = generate(small, os.path.join(workdir, "small_out.glb"), strict=False)
        check("a small model gets no LODs", "skipped" in small_report,
              small_report.get("skipped", "it generated some"))

    print()
    if failures:
        print(f"lod_gen selftest: {len(failures)} FAILED: {', '.join(failures)}", file=sys.stderr)
        return 1
    print("lod_gen selftest: all checks passed.")
    return 0


def main() -> int:
    args = argv_after_ddash()
    if "--selftest" in args:
        return selftest()
    if "--make-fixture" in args:
        index = args.index("--make-fixture")
        build_fixture(args[index + 1])
        return 0
    if len(args) < 2:
        print(__doc__.split("\n\n")[1], file=sys.stderr)
        return 2

    report = generate(args[0], args[1])
    log(f"{report['source']} -> {report['output']}")
    log(f"  LOD0 {report['base']['triangles']} triangles, UV {report['base']['uvLayers']}")
    for level, entry in report["levels"].items():
        log(
            f"  {level} {entry['triangles']} triangles "
            f"({entry['achievedRatio']:.3f} of LOD0), "
            f"silhouette error {entry['silhouetteError']:.4f} "
            f"(budget {entry['silhouetteBudget']})"
        )
    if "skipped" in report:
        log(f"  {report['skipped']}")
    print("LODGEN_JSON " + json.dumps(report))
    for problem in report["problems"]:
        print(f"lod_gen: PROBLEM {problem}", file=sys.stderr)
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    # Blender does NOT propagate a script's exit status: `sys.exit(1)` here still leaves `blender`
    # returning 0, and an uncaught exception does too. Measured on 4.3.2, and it would make every
    # Blender gate in this project silently pass. The status is therefore printed on a sentinel line
    # that `blender_env.relaunch` reads, and any exception is turned into one as well.
    try:
        _status = main()
    except SystemExit as _exit:                       # a nested sys.exit() inside the tool
        _status = int(_exit.code or 0)
    except BaseException:                             # noqa: BLE001 - report it, then re-raise
        import traceback

        traceback.print_exc()
        print("lod_gen: EXIT 1")
        raise
    print(f"lod_gen: EXIT {_status}")
    sys.exit(_status)
