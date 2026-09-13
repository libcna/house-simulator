#!/usr/bin/env python3
"""Prepare one Poly Haven vegetation source as a budgeted, grounded GLB.

This is the Blender half of ``polyhaven_vegetation.py``.  It imports one staged glTF, keeps one
tree variation (the upstream tree files often contain three alternatives), joins plant clumps,
normalises the support point and scale, and reduces the mesh to the requested LOD0 budget.
"""

from __future__ import annotations

import json
import math
import os
import sys

try:
    import bpy  # type: ignore
except ImportError:  # pragma: no cover - ordinary Python relaunches under Blender
    from pathlib import Path

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import blender_env

    sys.exit(blender_env.relaunch(Path(__file__).resolve(), sys.argv[1:], tool="vegetation_prepare"))

import bmesh  # type: ignore  # noqa: E402
from mathutils import Vector  # type: ignore  # noqa: E402


def arguments() -> list[str]:
    return sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def triangles(obj) -> int:
    obj.data.calc_loop_triangles()
    return len(obj.data.loop_triangles)


def triangulate(obj) -> None:
    mesh = bmesh.new()
    mesh.from_mesh(obj.data)
    bmesh.ops.triangulate(mesh, faces=mesh.faces[:])
    mesh.to_mesh(obj.data)
    mesh.free()


def apply_modifier(obj, modifier) -> None:
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.modifier_apply(modifier=modifier.name)


def world_bounds(obj) -> tuple[Vector, Vector]:
    """Bounds from referenced vertices, avoiding Blender's lazy object box and loose scan points.

    The glTF exporter omits vertices not referenced by a polygon. Including them here allowed a
    stray scan vertex to define 3.5 m during preparation and then disappear, yielding a 3.37 m GLB.
    """
    used = {index for polygon in obj.data.polygons for index in polygon.vertices}
    points = [obj.matrix_world @ obj.data.vertices[index].co for index in used]
    if not points:
        raise RuntimeError("mesh has no vertices")
    low = Vector(tuple(min(point[axis] for point in points) for axis in range(3)))
    high = Vector(tuple(max(point[axis] for point in points) for axis in range(3)))
    return low, high


def import_source(path: str, *, one_tree: bool):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if not meshes:
        raise RuntimeError(f"{path}: contains no mesh")

    # Poly Haven's conifer and Searsia sources contain several complete alternatives at the same
    # origin.  One logical tree per GLB is the project convention, so keep the largest alternative
    # (mesh 0 in the published file; verified by the acquisition report), not an overlapping grove.
    if one_tree and len(meshes) > 1:
        keep = meshes[0]
        for obj in meshes[1:]:
            bpy.data.objects.remove(obj, do_unlink=True)
        meshes = [keep]

    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    result = bpy.context.view_layer.objects.active
    # Imported glTF nodes may retain a scale or the Y-up -> Z-up rotation on the object.  Bake the
    # complete node transform before measuring; replacing an unbaked ``obj.scale`` below otherwise
    # loses part of that transform and makes the requested height source-dependent.
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    triangulate(result)
    return result


def ground_and_scale(obj, *, height: float, crown: float) -> None:
    low, high = world_bounds(obj)
    # Blender is Z-up even though the imported and exported glTF is Y-up.  The exporter converts
    # Z back to glTF +Y; measuring Blender Y here used to scale one horizontal axis as if it were
    # height and produced trees whose exported height depended on crown width.
    source_height = high.z - low.z
    if not math.isfinite(source_height) or source_height <= 0.0:
        raise RuntimeError("source has no positive height")
    uniform = height / source_height
    obj.scale = (uniform * crown, uniform * crown, uniform)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

    low, high = world_bounds(obj)
    centre_x = (low.x + high.x) * 0.5
    centre_y = (low.y + high.y) * 0.5
    floor = low.z
    obj.location -= Vector((centre_x, centre_y, floor))
    bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)


def crop_cluster(obj, max_ratio: float) -> None:
    """Keep the central authored clump when an upstream file lays variants out in a long row."""
    if max_ratio <= 0.0:
        return
    low, high = world_bounds(obj)
    height = high.z - low.z
    centre = (low + high) * 0.5
    half = height * max_ratio * 0.5
    if high.x - low.x <= half * 2.0 and high.z - low.z <= half * 2.0:
        return
    mesh = bmesh.new()
    mesh.from_mesh(obj.data)
    remove = []
    for face in mesh.faces:
        point = face.calc_center_median()
        if abs(point.x - centre.x) > half or abs(point.y - centre.y) > half:
            remove.append(face)
    if remove and len(remove) < len(mesh.faces):
        bmesh.ops.delete(mesh, geom=remove, context="FACES")
        bmesh.ops.delete(mesh, geom=[vertex for vertex in mesh.verts if not vertex.link_faces],
                         context="VERTS")
        mesh.to_mesh(obj.data)
        obj.data.update()
    mesh.free()


def reduce_component(obj, target: int) -> None:
    before = triangles(obj)
    if before <= target:
        return
    # A single extreme collapse often stops at one triangle per disconnected scanned leaf. Repeat
    # moderate passes first; they preserve UVs considerably better than asking one modifier for a
    # 0.2 % ratio.
    for pass_index in range(4):
        current = triangles(obj)
        if current <= target * 1.02:
            break
        modifier = obj.modifiers.new(name=f"vegetation-budget-{pass_index}", type="DECIMATE")
        modifier.decimate_type = "COLLAPSE"
        modifier.ratio = max(target / current, 0.08)
        modifier.use_collapse_triangulate = True
        apply_modifier(obj, modifier)
        triangulate(obj)
        if triangles(obj) >= current:
            break

    # Photogrammetric foliage can still have tens of thousands of disconnected, already-triangular
    # leaf islands: no edge-collapse algorithm can cross those islands. Keep a deterministic,
    # spatially distributed subset in each material so every crown region and material survives.
    # This is a foliage-specific last mile, not a generic decimator.
    current = triangles(obj)
    if current > target:
        mesh = bmesh.new()
        mesh.from_mesh(obj.data)
        bmesh.ops.triangulate(mesh, faces=mesh.faces[:])
        groups = {}
        for face in mesh.faces:
            groups.setdefault(face.material_index, []).append(face)
        keep = set()
        remaining = target
        ordered_groups = sorted(groups.items())
        total = sum(len(faces) for _material, faces in ordered_groups)
        for group_index, (_material, faces) in enumerate(ordered_groups):
            if group_index == len(ordered_groups) - 1:
                quota = min(len(faces), remaining)
            else:
                quota = min(len(faces), max(1, round(target * len(faces) / total)))
                remaining -= quota
            faces.sort(key=lambda face: (
                round(face.calc_center_median().z, 4),
                round(face.calc_center_median().x, 4),
                round(face.calc_center_median().y, 4),
                face.index,
            ))
            for index in range(quota):
                keep.add(faces[min(len(faces) - 1, int((index + 0.5) * len(faces) / quota))])
        bmesh.ops.delete(mesh, geom=[face for face in mesh.faces if face not in keep],
                         context="FACES")
        bmesh.ops.delete(mesh, geom=[vertex for vertex in mesh.verts if not vertex.link_faces],
                         context="VERTS")
        mesh.to_mesh(obj.data)
        mesh.free()
        obj.data.update()
        triangulate(obj)
    if triangles(obj) > math.ceil(target * 1.02):
        raise RuntimeError(f"decimation produced {triangles(obj)} triangles, target {target}")


def is_alpha_part(obj) -> bool:
    """Whether this separated material part is alpha-tested foliage."""
    material_indices = {polygon.material_index for polygon in obj.data.polygons}
    materials = [obj.material_slots[index].material for index in material_indices
                 if index < len(obj.material_slots)]
    for material in materials:
        if material is None:
            continue
        name = material.name.lower()
        if any(token in name for token in ("leaf", "leaves", "twig", "grass", "plant", "fern")):
            return True
        if getattr(material, "surface_render_method", "DITHERED") != "DITHERED":
            return True
        if material.use_nodes:
            for node in material.node_tree.nodes:
                alpha = node.inputs.get("Alpha") if node.type == "BSDF_PRINCIPLED" else None
                if alpha is not None and alpha.is_linked:
                    return True
    return False


def allocate(parts: list, budget: int) -> list[int]:
    counts = [triangles(part) for part in parts]
    total = sum(counts)
    remaining = budget
    quotas = []
    for index, count in enumerate(counts):
        if index == len(counts) - 1:
            quota = min(count, remaining)
        else:
            quota = min(count, max(1, round(budget * count / total)))
            remaining -= quota
        quotas.append(quota)
    return quotas


def reduce(obj, target: int):
    """Reduce structural and alpha materials independently, then rejoin them.

    A global decimation ratio is dominated by the millions of disconnected leaf triangles in a
    scanned tree. Repeating it collapses the connected trunk almost to nothing before the leaves
    reach their floor. Separating materials reserves a real structural budget and produces a tree,
    not a cloud of isolated bark triangles.
    """
    if triangles(obj) <= target:
        return obj
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.separate(type="MATERIAL")
    bpy.ops.object.mode_set(mode="OBJECT")
    parts = [part for part in bpy.context.selected_objects if part.type == "MESH"]
    for part in parts:
        triangulate(part)

    alpha = [part for part in parts if is_alpha_part(part)]
    solid = [part for part in parts if part not in alpha]
    if alpha and solid:
        solid_budget = min(sum(triangles(part) for part in solid), max(600, round(target * 0.35)))
    else:
        solid_budget = target if solid else 0
    alpha_budget = target - solid_budget

    for group, budget in ((solid, solid_budget), (alpha, alpha_budget)):
        if not group:
            continue
        for part, quota in zip(group, allocate(group, budget)):
            reduce_component(part, quota)

    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.join()
    return bpy.context.active_object


def wrap_uvs(obj) -> None:
    """Bake repeating coordinates into the canonical tile before LOD interpolation.

    Several scan meshes use very large tiled UVs. They render through REPEAT but violate the
    project's finite/sane LOD check after interpolation; wrapping by an integer is visually
    identical for a repeating texture and keeps every generated level in [0, 1].
    """
    for layer in obj.data.uv_layers:
        for datum in layer.data:
            datum.uv.x %= 1.0
            datum.uv.y %= 1.0


def export_one(source: str, output: str, name: str, height: float, crown: float,
               target: int, one_tree: bool, max_ratio: float) -> dict:
    obj = import_source(source, one_tree=one_tree)
    crop_cluster(obj, max_ratio)
    # Reduce before final scaling.  The foliage fallback deliberately samples disconnected leaf
    # faces and can remove the original topmost/bottommost card; scaling first therefore made the
    # exported height drift by more than a metre on sparse LOD0 trees.
    obj = reduce(obj, target)
    ground_and_scale(obj, height=height, crown=crown)
    wrap_uvs(obj)
    # The base mesh itself is LOD0; §18 names only the extra meshes ``<name>_LOD1`` and
    # ``<name>_LOD2``.  Giving this one an ``_LOD0`` suffix would make lod_gen emit
    # ``<name>_LOD0_LOD1``.
    obj.name = name
    obj.data.name = name
    for item in bpy.context.selected_objects:
        item.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    os.makedirs(os.path.dirname(output), exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=output,
        export_format="GLB",
        use_selection=True,
        export_apply=True,
        export_materials="EXPORT",
        export_image_format="AUTO",
    )
    low, high = world_bounds(obj)
    return {
        "output": output,
        "name": name,
        "triangles": triangles(obj),
        "boundsMetres": [round(high[i] - low[i], 6) for i in range(3)],
    }


def main() -> int:
    args = arguments()
    if len(args) != 2:
        print("usage: vegetation_prepare.py SOURCE.gltf SPECS.json", file=sys.stderr)
        return 2
    source, spec_path = args
    with open(spec_path, encoding="utf-8") as handle:
        specs = json.load(handle)
    reports = []
    for spec in specs:
        reports.append(export_one(source, one_tree=spec["oneTree"], **{
            key: spec[key] for key in ("output", "name", "height", "crown", "target", "max_ratio")
        }))
    print("VEGETATION_JSON " + json.dumps(reports, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        status = main()
    except BaseException:  # Blender otherwise exits zero after a Python exception.
        import traceback

        traceback.print_exc()
        status = 1
    print(f"vegetation_prepare: EXIT {status}")
    sys.exit(status)
