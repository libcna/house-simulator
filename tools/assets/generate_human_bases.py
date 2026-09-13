#!/usr/bin/env python3
"""Generate the two core-only MPFB research bodies for HOUSE-00294.

Run this script with Blender, after enabling the pinned MPFB extension::

    blender --background --python tools/assets/generate_human_bases.py -- \
        --output-dir assets-src/Models/Characters \
        --review-dir docs/asset-review/human-bases

Only MPFB's bundled basemesh and macro targets are used.  No community skin, body-part,
clothing, hair, pose or proxy asset is loaded.  The generated body surface is baked, helper
geometry is removed, and one topology-preserving unsubdivide pass brings LOD0 under the
project's 22,000-triangle character budget without a free-form decimation of joint loops.

This is Blender tooling, not runtime code, and is therefore outside the XNA-only boundary.
"""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector
from PIL import Image, ImageDraw

EXPECTED_MPFB_VERSION = (2, 0, 17)
EXPECTED_MPFB_BUILD = "20260722"
HEIGHT_TOLERANCE_METRES = 0.0005


@dataclass(frozen=True)
class BodySpec:
    key: str
    gender: float
    height_metres: float


BODY_SPECS = (
    BodySpec("female", 0.0, 1.66),
    BodySpec("male", 1.0, 1.78),
)


def arguments() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--review-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    return parser.parse_args(argv)


def mpfb_services():
    try:
        from bl_ext.user_default import mpfb
        from bl_ext.user_default.mpfb.services import ExportService, HumanService, TargetService
    except ImportError as error:
        raise RuntimeError(
            "MPFB must be installed and enabled in Blender's user_default extension repository"
        ) from error

    if tuple(mpfb.VERSION) != EXPECTED_MPFB_VERSION or mpfb.BUILD_INFO != EXPECTED_MPFB_BUILD:
        raise RuntimeError(
            f"expected MPFB {EXPECTED_MPFB_VERSION} build {EXPECTED_MPFB_BUILD}, got "
            f"{tuple(mpfb.VERSION)} build {mpfb.BUILD_INFO}"
        )
    return ExportService, HumanService, TargetService


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)


def clean_body(spec: BodySpec, macro_height: float, services):
    export_service, human_service, target_service = services
    macro = target_service.get_default_macro_info_dict()
    macro.update(
        {
            "gender": spec.gender,
            "age": 0.5,
            "muscle": 0.5,
            "weight": 0.5,
            "proportions": 0.5,
            "height": macro_height,
            "cupsize": 0.5,
            "firmness": 0.5,
            "race": {"asian": 1.0 / 3.0, "caucasian": 1.0 / 3.0, "african": 1.0 / 3.0},
        }
    )
    body = human_service.create_human(
        mask_helpers=False,
        detailed_helpers=False,
        extra_vertex_groups=False,
        feet_on_ground=True,
        scale=0.1,
        macro_detail_dict=macro,
    )
    body.name = f"human_{spec.key}_base"
    export_service.bake_modifiers_remove_helpers(
        body,
        bake_masks=False,
        bake_subdiv=False,
        remove_helpers=True,
        also_proxy=False,
    )

    bpy.context.view_layer.objects.active = body
    body.select_set(True)
    # Bake the selected phenotype before changing topology.  apply_mix=True preserves the visible
    # result; deleting the keys without it would silently return the neutral androgynous body.
    bpy.ops.object.shape_key_remove(all=True, apply_mix=True)

    modifier = body.modifiers.new("LOD0 topology reduction", "DECIMATE")
    modifier.decimate_type = "UNSUBDIV"
    modifier.iterations = 1
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    bpy.context.view_layer.update()
    return body


def dispose(body) -> None:
    mesh = body.data
    bpy.data.objects.remove(body, do_unlink=True)
    if mesh.users == 0:
        bpy.data.meshes.remove(mesh)


def fit_height(spec: BodySpec, services) -> tuple[object, float]:
    low = 0.25
    high = 0.75
    for _ in range(12):
        candidate = (low + high) / 2.0
        body = clean_body(spec, candidate, services)
        height = float(body.dimensions.z)
        dispose(body)
        if height < spec.height_metres:
            low = candidate
        else:
            high = candidate

    macro_height = (low + high) / 2.0
    body = clean_body(spec, macro_height, services)
    measured = float(body.dimensions.z)
    if abs(measured - spec.height_metres) > HEIGHT_TOLERANCE_METRES:
        raise RuntimeError(
            f"{spec.key} height {measured:.6f} m misses {spec.height_metres:.3f} m target"
        )
    return body, macro_height


def add_review_material(body) -> None:
    material = bpy.data.materials.new(f"MAT_{body.name.upper()}_REVIEW")
    material.diffuse_color = (0.54, 0.34, 0.25, 1.0)
    body.data.materials.append(material)
    for polygon in body.data.polygons:
        polygon.use_smooth = True


def topology(body) -> dict[str, object]:
    mesh = body.data
    bm = bmesh.new()
    bm.from_mesh(mesh)
    boundary_edges = sum(1 for edge in bm.edges if edge.is_boundary)
    non_manifold_edges = sum(1 for edge in bm.edges if not edge.is_manifold)
    bm.free()

    faces_by_size: dict[str, int] = {}
    for polygon in mesh.polygons:
        key = str(len(polygon.vertices))
        faces_by_size[key] = faces_by_size.get(key, 0) + 1

    return {
        "vertices": len(mesh.vertices),
        "edges": len(mesh.edges),
        "faces": len(mesh.polygons),
        "triangles": sum(max(0, len(polygon.vertices) - 2) for polygon in mesh.polygons),
        "facesByVertexCount": faces_by_size,
        "boundaryEdges": boundary_edges,
        "nonManifoldEdges": non_manifold_edges,
        "uvLayers": len(mesh.uv_layers),
    }


def export_glb(body, path: Path) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    result = bpy.ops.export_scene.gltf(
        filepath=str(path),
        export_format="GLB",
        use_selection=True,
        export_apply=True,
        export_yup=True,
        export_materials="EXPORT",
        export_animations=False,
        export_morph=False,
        export_extras=False,
    )
    if result != {"FINISHED"}:
        raise RuntimeError(f"glTF export failed for {path}: {result}")


def point_camera(camera, position: tuple[float, float, float], target: Vector) -> None:
    camera.location = position
    camera.rotation_euler = (target - camera.location).to_track_quat("-Z", "Y").to_euler()


def configure_render(body, height: float):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.resolution_x = 420
    scene.render.resolution_y = 720
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = "WORLD"
    scene.display.shading.curvature_ridge_factor = 1.5
    scene.display.shading.curvature_valley_factor = 1.0
    scene.display.shading.background_type = "VIEWPORT"
    scene.display.shading.background_color = (0.055, 0.065, 0.08)

    camera_data = bpy.data.cameras.new("ReviewCamera")
    camera = bpy.data.objects.new("ReviewCamera", camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = height * 1.10
    body.hide_render = False
    return scene, camera


def add_wire_overlay(body):
    overlay = body.copy()
    overlay.data = body.data.copy()
    overlay.name = f"{body.name}_topology_overlay"
    bpy.context.scene.collection.objects.link(overlay)
    overlay.data.materials.clear()
    material = bpy.data.materials.new(f"MAT_{body.name.upper()}_TOPOLOGY")
    material.diffuse_color = (0.015, 0.02, 0.025, 1.0)
    overlay.data.materials.append(material)
    modifier = overlay.modifiers.new("Topology edges", "WIREFRAME")
    modifier.thickness = 0.0012
    modifier.offset = 1.0
    modifier.use_even_offset = True
    return overlay


def render_contact_sheet(body, spec: BodySpec, path: Path, wire: bool) -> None:
    scene, camera = configure_render(body, spec.height_metres)
    overlay = add_wire_overlay(body) if wire else None
    target = Vector((0.0, 0.0, spec.height_metres * 0.49))
    distance = 4.0
    views = (
        ("front", (0.0, -distance, target.z)),
        ("left", (-distance, 0.0, target.z)),
        ("back", (0.0, distance, target.z)),
        ("right", (distance, 0.0, target.z)),
    )

    with tempfile.TemporaryDirectory(prefix=f"human-{spec.key}-review-") as temporary:
        rendered: list[tuple[str, Image.Image]] = []
        for label, position in views:
            point_camera(camera, position, target)
            render_path = Path(temporary) / f"{label}.png"
            scene.render.filepath = str(render_path)
            bpy.ops.render.render(write_still=True)
            rendered.append((label, Image.open(render_path).convert("RGB")))

        label_height = 28
        sheet = Image.new("RGB", (840, 2 * (720 + label_height)), (14, 17, 21))
        draw = ImageDraw.Draw(sheet)
        for index, (label, image) in enumerate(rendered):
            column = index % 2
            row = index // 2
            x = column * 420
            y = row * (720 + label_height)
            sheet.paste(image, (x, y + label_height))
            draw.text((x + 10, y + 7), label, fill=(230, 234, 240))
        sheet.save(path, optimize=True)

    camera_data = camera.data
    bpy.data.objects.remove(camera, do_unlink=True)
    bpy.data.cameras.remove(camera_data)
    if overlay is not None:
        dispose(overlay)


def main() -> int:
    args = arguments()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    args.review_dir.mkdir(parents=True, exist_ok=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)

    services = mpfb_services()
    clear_scene()
    report = {
        "schema": "cna-house/human-base-research/1",
        "generator": "tools/assets/generate_human_bases.py",
        "blender": bpy.app.version_string,
        "mpfb": {"version": "2.0.17", "build": EXPECTED_MPFB_BUILD},
        "assetsUsed": ["MPFB bundled basemesh", "MPFB bundled macro targets"],
        "communityAssetsUsed": [],
        "bodies": {},
    }

    for spec in BODY_SPECS:
        body, macro_height = fit_height(spec, services)
        add_review_material(body)
        stats = topology(body)
        stats["macroHeight"] = round(macro_height, 8)
        stats["boundsMetresBlenderXYZ"] = [round(float(value), 6) for value in body.dimensions]
        stats["targetHeightMetres"] = spec.height_metres
        stats["measuredHeightMetres"] = round(float(body.dimensions.z), 6)
        stats["sourceTopology"] = "MPFB body surface followed by one Blender unsubdivide pass"
        report["bodies"][spec.key] = stats

        export_glb(body, args.output_dir / f"human_{spec.key}_base.glb")
        render_contact_sheet(
            body,
            spec,
            args.review_dir / f"human_{spec.key}_silhouette.png",
            wire=False,
        )
        render_contact_sheet(
            body,
            spec,
            args.review_dir / f"human_{spec.key}_topology.png",
            wire=True,
        )
        dispose(body)

    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
