#!/usr/bin/env python3
"""Retarget the six fixed HOUSE-00295 CMU trials onto an MPFB research body.

Run with Blender 4.3 after installing MPFB 2.0.17::

    blender --background --python tools/assets/retarget_cmu_locomotion.py -- \
        --source-dir /home/robertvokac/deps/cmu-mocap/91 \
        --bvh-dir /home/robertvokac/deps/cmu-mocap/91-bvh \
        --output /tmp/house-00295-cmu-retarget-research.glb \
        --review-dir docs/asset-review/human-locomotion \
        --report docs/asset-review/human-locomotion/measurements.json

The raw ASF/AMC and MotionBuilder BVH files remain outside the repository.  CMU expressly permits
including the retargeted data in a product but asks that the database itself not be redistributed;
the fixed URLs and hashes below make the derivation auditable without republishing those inputs.

This is Blender tooling, not runtime code, and is outside the XNA-only boundary.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

import bpy
from mathutils import Vector
from PIL import Image, ImageDraw


EXPECTED_MPFB_VERSION = (2, 0, 17)
EXPECTED_MPFB_BUILD = "20260722"
SUBJECT = 91
SOURCE_FPS = 120
OUTPUT_FPS = 30
SOURCE_STEP = SOURCE_FPS // OUTPUT_FPS
OUTPUT_FRAMES = 120
CMU_ACKNOWLEDGEMENT = (
    "The data used in this project was obtained from mocap.cs.cmu.edu. "
    "The database was created with funding from NSF EIA-0196217."
)


@dataclass(frozen=True)
class Trial:
    number: int
    clip: str
    description: str
    frames: int
    sha256: str
    bvh_sha256: str


TRIALS = (
    Trial(2, "walk_straight", "WalkStraight", 1748,
          "e9fb90488a44dcd2d2480022e320bd75e5cc71edcb7dc48cf316aec10e6c9074",
          "c1efc11d7112d411c46e56280272062c6ecf7278652e13c553254c1ec49a8e2b"),
    Trial(10, "walk_slow", "SlowWalk", 3175,
          "1067d0edf7e662c1d497251b6b2319484f1c652cd057e5e183d68f316d1d70ce",
          "ed74f3531857f24e9b8e4010d08e975d6e476ed87459c021d1ab82cf839f738a"),
    Trial(17, "walk_quick", "QuickWalk", 1520,
          "d596f0bc01df894601299a88986da05e4db7edd15887eef79e51787bf60b646f",
          "12ac9a8f25c99c8d38b97523770428fdf22d3e365d95e739e2f29f07b94910b1"),
    Trial(22, "walk_casual_quick", "CasualQuickWalk", 2106,
          "5de00b5265c232496cec489869d8b0097e1fb6e32c4be377363174cf971b950c",
          "d61d2e9a2845441778935dfbfd67a488515a8e828f25f050eee694ab453e2a5f"),
    Trial(29, "walk_normal_a", "NormalWalk", 2181,
          "a1dd0136f60022c7f6189fc04c3591fb251b691c5edaf7337f4daddea6ae53c2",
          "378688b1619a87fcdd04d7cf8f5c04273c5057917f0c5f8211c788eec447b89d"),
    Trial(31, "walk_normal_b", "NormalWalk", 1992,
          "b7878cddc6ac028a07c1c97f0fb2df8c160736cc62a89f1e4e14bb20ce0d2482",
          "cf3a60d33345b22efa1008e6cda1614724598fa9bf9a5f7ab88674e6d6d7027d"),
)

ASF_SHA256 = "de06a1ee5d917e4bd23461e22e49b43591ed8d9bd84a92d6b6927f423a972b30"

BVH_NAME_FIXES = {
    "LeftHandIndex1": "LeftHandFinger1",
    "RightHandIndex1": "RightHandFinger1",
}


def arguments() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--bvh-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--review-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    return parser.parse_args(argv)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_sources(args: argparse.Namespace) -> None:
    expected = {f"{SUBJECT}.asf": ASF_SHA256}
    expected.update({f"{SUBJECT}_{trial.number:02d}.amc": trial.sha256 for trial in TRIALS})
    for name, wanted in expected.items():
        path = args.source_dir / name
        if not path.is_file():
            raise RuntimeError(f"missing CMU source {path}")
        actual = sha256(path)
        if actual != wanted:
            raise RuntimeError(f"{path}: expected SHA-256 {wanted}, got {actual}")

    for trial in TRIALS:
        path = args.bvh_dir / f"{SUBJECT}_{trial.number:02d}.bvh"
        if not path.is_file():
            raise RuntimeError(f"missing MotionBuilder-friendly CMU BVH {path}")
        actual = sha256(path)
        if actual != trial.bvh_sha256:
            raise RuntimeError(f"{path}: expected SHA-256 {trial.bvh_sha256}, got {actual}")


def services():
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
    for action in list(bpy.data.actions):
        bpy.data.actions.remove(action)


def create_body(mpfb_services):
    export_service, human_service, target_service = mpfb_services
    macro = target_service.get_default_macro_info_dict()
    macro.update(
        {
            "gender": 0.0,
            "age": 0.5,
            "muscle": 0.5,
            "weight": 0.5,
            "proportions": 0.5,
            "height": 0.55938721,
            "cupsize": 0.5,
            "firmness": 0.5,
            "race": {"asian": 1.0 / 3.0, "caucasian": 1.0 / 3.0,
                     "african": 1.0 / 3.0},
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
    body.name = "human_female_cmu_retarget_research"
    export_service.bake_modifiers_remove_helpers(
        body, bake_masks=False, bake_subdiv=False, remove_helpers=True, also_proxy=False
    )
    bpy.context.view_layer.objects.active = body
    body.select_set(True)
    bpy.ops.object.shape_key_remove(all=True, apply_mix=True)

    armature = human_service.add_builtin_rig(body, "cmu_mb", import_weights=True)
    armature.name = "human_research_rig"
    armature.data.name = "human_research_rig"

    bpy.context.view_layer.objects.active = body
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    modifier = body.modifiers.new("LOD0 topology reduction", "DECIMATE")
    modifier.decimate_type = "UNSUBDIV"
    modifier.iterations = 1
    while body.modifiers.find(modifier.name) > 0:
        bpy.ops.object.modifier_move_up(modifier=modifier.name)
    bpy.ops.object.modifier_apply(modifier=modifier.name)

    # MPFB's source weights deliberately overlap around shoulders and hips.  XNA's SkinnedEffect
    # accepts four influences, so keep the strongest four and renormalise before reviewing the
    # deformation.  The operation is deterministic for the fixed source weights.
    bpy.ops.object.vertex_group_limit_total(group_select_mode="ALL", limit=4)
    bpy.ops.object.vertex_group_normalize_all(group_select_mode="ALL", lock_active=False)

    for vertex in body.data.vertices:
        if len(vertex.groups) > 4:
            raise RuntimeError(
                f"vertex {vertex.index} has {len(vertex.groups)} weights after MPFB rigging; "
                "the research rig must respect the four-influence runtime bar"
            )
    return body, armature


def renamed_bvh(source: Path, destination: Path) -> None:
    lines = source.read_text(encoding="utf-8").splitlines()
    renamed = []
    found = set()
    for line in lines:
        stripped = line.strip()
        words = stripped.split()
        if len(words) == 2 and words[0] in {"ROOT", "JOINT"}:
            old = words[1]
            replacement = BVH_NAME_FIXES.get(old, old)
            line = line[: len(line) - len(line.lstrip())] + words[0] + " " + replacement
            if old in BVH_NAME_FIXES:
                found.add(old)
        renamed.append(line)
    missing = set(BVH_NAME_FIXES) - found
    if missing:
        raise RuntimeError(f"{source}: missing expected joints {sorted(missing)}")
    destination.write_text("\n".join(renamed) + "\n", encoding="utf-8")


def prepare_trial(args: argparse.Namespace, trial: Trial, directory: Path) -> Path:
    named_bvh = directory / f"{SUBJECT}_{trial.number:02d}.bvh"
    renamed_bvh(args.bvh_dir / named_bvh.name, named_bvh)
    return named_bvh


def remove_object(obj) -> None:
    data = obj.data
    bpy.data.objects.remove(obj, do_unlink=True)
    if data.users == 0:
        bpy.data.armatures.remove(data)


def retarget_trial(armature, bvh: Path, trial: Trial) -> dict[str, object]:
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.import_anim.bvh(
        filepath=str(bvh),
        axis_forward="-Z",
        axis_up="Y",
        global_scale=0.1,
        frame_start=1,
        use_fps_scale=False,
        update_scene_fps=False,
        rotate_mode="XYZ",
    )
    source = bpy.context.object
    source_action = source.animation_data.action
    target_names = {bone.name for bone in armature.data.bones}
    source_names = {bone.name for bone in source.data.bones}
    if target_names != source_names or len(target_names) != 31:
        raise RuntimeError(
            "CMU research skeleton changed: expected the same fixed 31-joint hierarchy in "
            f"source and target; target-only {sorted(target_names - source_names)}, "
            f"source-only {sorted(source_names - target_names)}"
        )

    source_to_target = armature.matrix_world.inverted() @ source.matrix_world
    bpy.context.scene.frame_set(1)
    ordered_names = sorted(
        target_names,
        key=lambda name: len(armature.data.bones[name].parent_recursive),
    )
    source_reference_rotation = {
        name: (source_to_target @ source.pose.bones[name].matrix).to_quaternion().normalized()
        for name in target_names
    }
    target_rest_rotation = {
        name: armature.data.bones[name].matrix_local.to_quaternion().normalized()
        for name in target_names
    }

    action = bpy.data.actions.new(trial.clip)
    armature.animation_data_create()
    armature.animation_data.action = action
    for pose_bone in armature.pose.bones:
        pose_bone.rotation_mode = "QUATERNION"

    # Bruce Hahn's MotionBuilder conversion prepends a T-pose as frame 1.  Keep it out of the
    # centered four-second research window while preserving the source's 120 Hz sampling.
    first_source = 1 + (trial.frames - (OUTPUT_FRAMES - 1) * SOURCE_STEP) // 2
    bpy.context.scene.frame_set(first_source)
    initial_root_height = (
        source_to_target @ source.pose.bones["Hips"].matrix.translation
    ).z

    left_heights = []
    right_heights = []
    hip_heights = []
    for output_frame in range(1, OUTPUT_FRAMES + 1):
        source_frame = first_source + (output_frame - 1) * SOURCE_STEP
        bpy.context.scene.frame_set(source_frame)
        for name in ordered_names:
            target_bone = armature.pose.bones[name]
            source_bone = source.pose.bones[name]
            source_rotation = (
                source_to_target @ source_bone.matrix
            ).to_quaternion().normalized()
            desired_rotation = (
                target_rest_rotation[name]
                @ source_reference_rotation[name].inverted()
                @ source_rotation
            ).normalized()
            desired_matrix = desired_rotation.to_matrix().to_4x4()
            if name == "Hips":
                vertical = (
                    source_to_target @ source_bone.matrix.translation
                ).z - initial_root_height
                desired_matrix.translation = armature.data.bones[name].head_local + Vector(
                    (0.0, 0.0, vertical)
                )
            else:
                desired_matrix.translation = target_bone.head
            target_bone.matrix = desired_matrix
            target_bone.scale = (1.0, 1.0, 1.0)
            if name == "Hips":
                target_bone.keyframe_insert("location", frame=output_frame, group=name)
            target_bone.keyframe_insert("rotation_quaternion", frame=output_frame, group=name)

        bpy.context.scene.frame_set(output_frame)
        left_heights.append(float(armature.pose.bones["LeftToeBase"].head.z))
        right_heights.append(float(armature.pose.bones["RightToeBase"].head.z))
        hip_heights.append(float(armature.pose.bones["Hips"].head.z))

    for fcurve in action.fcurves:
        for point in fcurve.keyframe_points:
            point.interpolation = "LINEAR"

    if source_action is not None:
        source.animation_data.action = None
        bpy.data.actions.remove(source_action)
    remove_object(source)
    armature.animation_data.action = action
    bpy.context.scene.frame_set(1)

    return {
        "subject": SUBJECT,
        "trial": trial.number,
        "sourceDescription": trial.description,
        "sourceUrl": (
            f"http://mocap.cs.cmu.edu/subjects/{SUBJECT}/"
            f"{SUBJECT}_{trial.number:02d}.amc"
        ),
        "sourceSha256": trial.sha256,
        "retargetBvhUrl": (
            "https://raw.githubusercontent.com/una-dinosauria/cmu-mocap/"
            f"09a07f54f3bbb58797325f009282d0b2048a2871/data/091/{SUBJECT}_{trial.number:02d}.bvh"
        ),
        "retargetBvhSha256": trial.bvh_sha256,
        "sourceFrames": trial.frames,
        "sourceWindow": [first_source, first_source + (OUTPUT_FRAMES - 1) * SOURCE_STEP],
        "outputFrames": OUTPUT_FRAMES,
        "outputFps": OUTPUT_FPS,
        "durationSeconds": (OUTPUT_FRAMES - 1) / OUTPUT_FPS,
        "rootMotion": "horizontal displacement removed; vertical motion retained",
        "qualityVerdict": "reject",
        "observedDefects": [
            "catastrophic surface tearing at shoulders, pelvis, hands, and feet",
            "joint orientation mismatch between CMU T-pose and MPFB cmu_mb skin bind",
            "not suitable for loop cleanup, stride measurement, or runtime extraction",
        ],
        "leftToeHeightRange": [min(left_heights), max(left_heights)],
        "rightToeHeightRange": [min(right_heights), max(right_heights)],
        "hipHeightRange": [min(hip_heights), max(hip_heights)],
    }


def add_material(body) -> None:
    material = bpy.data.materials.new("MAT_HUMAN_CMU_RESEARCH")
    material.diffuse_color = (0.42, 0.31, 0.25, 1.0)
    body.data.materials.append(material)
    for polygon in body.data.polygons:
        polygon.use_smooth = True


def point_camera(camera, position: tuple[float, float, float], target: Vector) -> None:
    camera.location = position
    camera.rotation_euler = (target - camera.location).to_track_quat("-Z", "Y").to_euler()


def render_review(body, armature, trial: Trial, path: Path) -> None:
    action = bpy.data.actions[trial.clip]
    armature.animation_data.action = action
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.resolution_x = 360
    scene.render.resolution_y = 620
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = "WORLD"
    scene.display.shading.background_type = "VIEWPORT"
    scene.display.shading.background_color = (0.055, 0.065, 0.08)

    camera_data = bpy.data.cameras.new("LocomotionReviewCamera")
    camera = bpy.data.objects.new("LocomotionReviewCamera", camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = 1.95
    point_camera(camera, (3.6, -5.2, 1.0), Vector((0.0, 0.0, 0.86)))

    review_frames = (1, 31, 61, 91)
    with tempfile.TemporaryDirectory(prefix=f"cmu-{trial.number:02d}-review-") as temporary:
        images = []
        for frame in review_frames:
            scene.frame_set(frame)
            render_path = Path(temporary) / f"{frame:03d}.png"
            scene.render.filepath = str(render_path)
            bpy.ops.render.render(write_still=True)
            images.append(Image.open(render_path).convert("RGB"))

        title_height = 42
        sheet = Image.new("RGB", (2 * 360, 2 * (620 + title_height)), (14, 17, 21))
        draw = ImageDraw.Draw(sheet)
        for index, (frame, image) in enumerate(zip(review_frames, images)):
            column = index % 2
            row = index // 2
            x = column * 360
            y = row * (620 + title_height)
            sheet.paste(image, (x, y + title_height))
            draw.text((x + 10, y + 7),
                      f"{trial.clip}  frame {frame}/{OUTPUT_FRAMES}",
                      fill=(230, 234, 240))
        sheet.save(path, optimize=True)

    bpy.data.objects.remove(camera, do_unlink=True)
    bpy.data.cameras.remove(camera_data)


def export_glb(body, armature, path: Path) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    result = bpy.ops.export_scene.gltf(
        filepath=str(path),
        export_format="GLB",
        use_selection=True,
        export_apply=False,
        export_yup=True,
        export_materials="EXPORT",
        export_animations=True,
        export_animation_mode="ACTIONS",
        export_force_sampling=True,
        export_optimize_animation_size=True,
        export_morph=False,
        export_extras=False,
    )
    if result != {"FINISHED"}:
        raise RuntimeError(f"glTF export failed for {path}: {result}")


def main() -> int:
    args = arguments()
    verify_sources(args)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.review_dir.mkdir(parents=True, exist_ok=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)

    mpfb_services = services()
    clear_scene()
    body, armature = create_body(mpfb_services)
    add_material(body)
    bpy.context.scene.render.fps = OUTPUT_FPS

    rest_action = bpy.data.actions.new("rest_pose")
    armature.animation_data_create()
    armature.animation_data.action = rest_action
    render_review(
        body,
        armature,
        Trial(0, "rest_pose", "MPFB bind pose", 0, "", ""),
        args.review_dir / "rest_pose.png",
    )
    armature.animation_data.action = None
    bpy.data.actions.remove(rest_action)

    report = {
        "schema": "cna-house/cmu-retarget-research/2",
        "generator": "tools/assets/retarget_cmu_locomotion.py",
        "blender": bpy.app.version_string,
        "mpfb": {"version": "2.0.17", "build": EXPECTED_MPFB_BUILD},
        "rig": "MPFB cmu_mb research rig",
        "boneCount": len(armature.data.bones),
        "bones": [bone.name for bone in armature.data.bones],
        "asf": {
            "subject": SUBJECT,
            "sourceUrl": f"http://mocap.cs.cmu.edu/subjects/{SUBJECT}/{SUBJECT}.asf",
            "sourceSha256": ASF_SHA256,
        },
        "retargetBvh": {
            "conversion": "Bruce Hahn MotionBuilder-friendly CMU BVH release",
            "repository": "https://github.com/una-dinosauria/cmu-mocap",
            "revision": "09a07f54f3bbb58797325f009282d0b2048a2871",
        },
        "acknowledgement": CMU_ACKNOWLEDGEMENT,
        "retargetMethod": (
            "calibrated global bone orientation transfer from the prepended CMU T-pose to "
            "the MPFB cmu_mb bind pose; source object axes transformed into target space"
        ),
        "decision": "reject all six research retargets; author the locomotion set in HOUSE-02219",
        "clips": [],
    }

    with tempfile.TemporaryDirectory(prefix="house-00295-") as temporary:
        temp = Path(temporary)
        for trial in TRIALS:
            bvh = prepare_trial(args, trial, temp)
            report["clips"].append(retarget_trial(armature, bvh, trial))

    for trial in TRIALS:
        render_review(body, armature, trial, args.review_dir / f"{trial.clip}.png")
    export_glb(body, armature, args.output)
    report["output"] = args.output.name
    report["outputDisposition"] = "rejected research artefact; deliberately not committed"
    report["outputSha256"] = sha256(args.output)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        f"{args.output}: {len(armature.data.bones)} bones, {len(TRIALS)} clips, "
        f"sha256 {report['outputSha256']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
