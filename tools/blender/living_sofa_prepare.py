#!/usr/bin/env python3
"""Prepare and verify HOUSE-01057's formal-living SheenWoodLeatherSofa.

The pinned Khronos delivery is an intentionally demanding material sample: 46,492 triangles,
thirteen embedded WebP maps, Z-up source streams and unsupported sheen/specular metadata.  The
normal house path needs a metre-scale Y-up model and stock-XNA materials instead.  This tool:

* welds the source's duplicated triangle vertices before simplification;
* scales the 2.726 m delivery to the publisher catalogue's 2.20 m domestic fit;
* produces a visually reviewed 9k-triangle LOD0 plus a 0.35-ratio LOD1 and box proxy;
* keeps all six source material names while removing runtime extensions and embedded maps; and
* extracts the five authored base-colour maps as deterministic <=256 px PNGs.

The weld is essential.  A direct Blender collapse treats the duplicated corners as disconnected
triangles and tears holes through the arms and back.  HOUSE-01057 retains the rejected render as
review evidence rather than accepting a triangle count that destroyed the sofa.

Run as a normal Python tool; it re-launches only the geometry write inside Blender:

    tools/blender/living_sofa_prepare.py --write --source SheenWoodLeatherSofa.glb
    tools/blender/living_sofa_prepare.py --check
    tools/blender/living_sofa_prepare.py --check --source SheenWoodLeatherSofa.glb
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import sys
import tempfile
from pathlib import Path

try:
    import bpy  # type: ignore
    import bmesh  # type: ignore
    from mathutils import Vector  # type: ignore

    INSIDE_BLENDER = True
except ImportError:
    INSIDE_BLENDER = False

REPO = Path(__file__).resolve().parents[2]
SOURCE_URL = (
    "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/"
    "SheenWoodLeatherSofa/glTF-Binary/SheenWoodLeatherSofa.glb"
)
SOURCE_SHA256 = "5349e042ad41e695e89f1110230c4ee0c75b2bc62ef830c7016be6ecf665bfb6"
SOURCE_BYTES = 10_107_912
TARGET = REPO / "assets-src/Models/Furniture/LivingRoom/formal_sofa.glb"
MANIFEST = REPO / "assets-src/assets.manifest.json"
ASSET_ID = "MODEL_FURNITURE_FORMAL_SOFA"
COLLISION_NAME = "FormalSofa_COL"
VISIBLE_NODES = ("Fringe", "Frame", "Frame_Fabric", "Paisley", "Stripes", "Brown")
SOURCE_MATERIALS = ("Brown", "Paisley", "Striped", "Fringe", "Frame", "Frame_Fabric")
TARGET_WIDTH_METRES = 2.20
LOD0_RATIO = 0.25
LOD1_RATIO = 0.35
EXPECTED_LOD0_TRIANGLES = 9_382
EXPECTED_LOD1_TRIANGLES = 3_278
EXPECTED_BOUNDS = [2.2, 0.901392, 0.744769]
EXPECTED_MATERIAL_MAP = {
    "Brown": "MAT_LIVING_SOFA_LEATHER",
    "Paisley": "MAT_LIVING_SOFA_PAISLEY",
    "Striped": "MAT_LIVING_SOFA_STRIPE",
    "Fringe": "MAT_LIVING_SOFA_FRINGE",
    "Frame": "MAT_LIVING_SOFA_FRAME",
    "Frame_Fabric": "MAT_LIVING_SOFA_LEATHER",
}
TEXTURES = {
    "TEXTURE_LIVING_SOFA_LEATHER": (
        0, REPO / "assets-src/Textures/Furniture/LivingRoom/living_sofa_leather.png", (256, 256),
    ),
    "TEXTURE_LIVING_SOFA_PAISLEY": (
        3, REPO / "assets-src/Textures/Furniture/LivingRoom/living_sofa_paisley.png", (256, 256),
    ),
    "TEXTURE_LIVING_SOFA_STRIPE": (
        5, REPO / "assets-src/Textures/Furniture/LivingRoom/living_sofa_stripe.png", (256, 256),
    ),
    "TEXTURE_LIVING_SOFA_FRINGE": (
        7, REPO / "assets-src/Textures/Furniture/LivingRoom/living_sofa_fringe.png", (256, 128),
    ),
    "TEXTURE_LIVING_SOFA_FRAME": (
        10, REPO / "assets-src/Textures/Furniture/LivingRoom/living_sofa_frame.png", (256, 256),
    ),
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


if INSIDE_BLENDER:

    def triangles(obj) -> int:
        obj.data.calc_loop_triangles()
        return len(obj.data.loop_triangles)


    def select_only(obj) -> None:
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj


    def apply_object_transform(obj) -> None:
        select_only(obj)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


    def weld_and_triangulate(obj) -> None:
        mesh = bmesh.new()
        mesh.from_mesh(obj.data)
        bmesh.ops.triangulate(mesh, faces=mesh.faces[:])
        bmesh.ops.remove_doubles(mesh, verts=mesh.verts[:], dist=0.000001)
        mesh.to_mesh(obj.data)
        mesh.free()
        obj.data.update()


    def decimate(obj, ratio: float) -> None:
        select_only(obj)
        modifier = obj.modifiers.new(name="cna-house-decimate", type="DECIMATE")
        modifier.decimate_type = "COLLAPSE"
        modifier.ratio = ratio
        modifier.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier=modifier.name)


    def bounds(objects) -> tuple[Vector, Vector]:
        points = [obj.matrix_world @ vertex.co for obj in objects for vertex in obj.data.vertices]
        if not points:
            raise RuntimeError("prepared sofa has no vertices")
        low = Vector(tuple(min(point[axis] for point in points) for axis in range(3)))
        high = Vector(tuple(max(point[axis] for point in points) for axis in range(3)))
        return low, high


    def shift_vertices(objects, delta: Vector, scale: float = 1.0) -> None:
        for obj in objects:
            for vertex in obj.data.vertices:
                vertex.co = (vertex.co + delta) * scale
            obj.data.update()


    def clean_materials(objects) -> None:
        names_by_object = {}
        for obj in objects:
            names = {slot.material.name for slot in obj.material_slots if slot.material is not None}
            if len(names) != 1:
                raise RuntimeError(f"{obj.name}: expected one source material, found {sorted(names)}")
            names_by_object[obj.name] = names.pop()
            obj.data.materials.clear()
        for material in list(bpy.data.materials):
            bpy.data.materials.remove(material)
        materials = {}
        for name in SOURCE_MATERIALS:
            material = bpy.data.materials.new(name=name)
            material.diffuse_color = (0.5, 0.5, 0.5, 1.0)
            material.use_nodes = False
            materials[name] = material
        for obj in objects:
            name = names_by_object[obj.name]
            if name not in materials:
                raise RuntimeError(f"{obj.name}: unexpected source material {name}")
            obj.data.materials.clear()
            obj.data.materials.append(materials[name])


    def duplicate_lod1(objects):
        copies = []
        for source in objects:
            copy = source.copy()
            copy.data = source.data.copy()
            copy.name = f"{source.name}_LOD1"
            copy.data.name = copy.name
            bpy.context.scene.collection.objects.link(copy)
            decimate(copy, LOD1_RATIO)
            copies.append(copy)
        source_low, source_high = bounds(objects)
        low, high = bounds(copies)
        source_centre = (source_low + source_high) * 0.5
        centre = (low + high) * 0.5
        fit = min(
            1.0,
            (source_high.x - source_low.x) / (high.x - low.x),
            (source_high.y - source_low.y) / (high.y - low.y),
            (source_high.z - source_low.z) / (high.z - low.z),
        )
        for obj in copies:
            for vertex in obj.data.vertices:
                vertex.co.x = (vertex.co.x - centre.x) * fit + source_centre.x
                vertex.co.y = (vertex.co.y - centre.y) * fit + source_centre.y
                vertex.co.z = (vertex.co.z - low.z) * fit + source_low.z
            obj.data.update()
        return copies


    def add_collision(low: Vector, high: Vector):
        vertices = [
            (low.x, low.y, low.z), (high.x, low.y, low.z),
            (high.x, high.y, low.z), (low.x, high.y, low.z),
            (low.x, low.y, high.z), (high.x, low.y, high.z),
            (high.x, high.y, high.z), (low.x, high.y, high.z),
        ]
        faces = [
            (0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7),
            (0, 1, 5), (0, 5, 4), (3, 7, 6), (3, 6, 2),
            (0, 4, 7), (0, 7, 3), (1, 2, 6), (1, 6, 5),
        ]
        mesh = bpy.data.meshes.new(COLLISION_NAME)
        mesh.from_pydata(vertices, [], faces)
        mesh.update()
        obj = bpy.data.objects.new(COLLISION_NAME, mesh)
        bpy.context.scene.collection.objects.link(obj)
        return obj


    def generate(source: Path, output: Path) -> None:
        raw = source.read_bytes()
        if len(raw) != SOURCE_BYTES or digest(raw) != SOURCE_SHA256:
            raise RuntimeError(
                f"{source}: not pinned source {SOURCE_SHA256} ({SOURCE_BYTES} bytes); {SOURCE_URL}"
            )
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=str(source))
        objects = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
        found = tuple(obj.name for obj in objects)
        if found != VISIBLE_NODES:
            raise RuntimeError(f"pinned source nodes changed: {found}")
        source_triangles = sum(triangles(obj) for obj in objects)
        if source_triangles != 46_492:
            raise RuntimeError(f"pinned source has {source_triangles} triangles, expected 46492")

        for obj in objects:
            apply_object_transform(obj)
            weld_and_triangulate(obj)
            decimate(obj, LOD0_RATIO)
        lod0_triangles = sum(triangles(obj) for obj in objects)
        if lod0_triangles != EXPECTED_LOD0_TRIANGLES:
            raise RuntimeError(
                f"prepared LOD0 has {lod0_triangles} triangles, expected {EXPECTED_LOD0_TRIANGLES}"
            )

        low, high = bounds(objects)
        centre = (low + high) * 0.5
        scale = TARGET_WIDTH_METRES / (high.x - low.x)
        shift_vertices(objects, Vector((-centre.x, -centre.y, -low.z)), scale)
        clean_materials(objects)
        lod1 = duplicate_lod1(objects)
        lod1_triangles = sum(triangles(obj) for obj in lod1)
        if lod1_triangles != EXPECTED_LOD1_TRIANGLES:
            raise RuntimeError(
                f"prepared LOD1 has {lod1_triangles} triangles, expected {EXPECTED_LOD1_TRIANGLES}"
            )
        low, high = bounds(objects)
        collision = add_collision(low, high)

        bpy.ops.object.select_all(action="DESELECT")
        for obj in [*objects, *lod1, collision]:
            obj.select_set(True)
        output.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.export_scene.gltf(
            filepath=str(output), export_format="GLB", use_selection=True,
            export_yup=True, export_apply=True, export_animations=False,
            export_cameras=False, export_lights=False, export_extras=False,
            export_texcoords=True, export_normals=True,
        )
        print(
            f"living_sofa_prepare: wrote {output} -- LOD0 {lod0_triangles}, "
            f"LOD1 {lod1_triangles}, proxy 12 triangles"
        )


    def blender_main() -> int:
        args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
        if len(args) != 3 or args[0] != "--blender-write":
            print("living_sofa_prepare: Blender mode expects --blender-write SOURCE OUTPUT")
            return 2
        try:
            generate(Path(args[1]), Path(args[2]))
        except (OSError, RuntimeError) as error:
            print(f"living_sofa_prepare: {error}", file=sys.stderr)
            return 1
        return 0


else:
    sys.path.insert(0, str(REPO / "tools" / "assets"))
    import gltf_io  # noqa: E402
    import gltf_validate  # noqa: E402
    import origin_check  # noqa: E402
    import scale_check  # noqa: E402
    sys.path.insert(0, str(REPO / "tools" / "world"))
    import layout_io  # noqa: E402
    sys.path.insert(0, str(REPO / "tools" / "blender"))
    import blender_env  # noqa: E402
    from PIL import Image  # noqa: E402


    def source_document(source: Path) -> tuple[dict, bytes]:
        raw = source.read_bytes()
        if len(raw) != SOURCE_BYTES or digest(raw) != SOURCE_SHA256:
            raise ValueError(
                f"{source}: not pinned source {SOURCE_SHA256} ({SOURCE_BYTES} bytes); {SOURCE_URL}"
            )
        return gltf_io.read_model(source)


    def texture_bytes(source: Path) -> dict[str, bytes]:
        document, blob = source_document(source)
        results = {}
        for asset_id, (image_index, _path, target_size) in TEXTURES.items():
            source_image = document["images"][image_index]
            view = document["bufferViews"][source_image["bufferView"]]
            start = view.get("byteOffset", 0)
            raw = blob[start:start + view["byteLength"]]
            image = Image.open(io.BytesIO(raw))
            mode = "RGBA" if asset_id.endswith("FRINGE") else "RGB"
            image = image.convert(mode)
            if image.size != target_size:
                image = image.resize(target_size, Image.Resampling.LANCZOS)
            encoded = io.BytesIO()
            image.save(encoded, format="PNG", compress_level=9, optimize=False)
            results[asset_id] = encoded.getvalue()
        return results


    def write_assets(source: Path) -> None:
        status = blender_env.relaunch(
            Path(__file__).resolve(), ["--blender-write", str(source), str(TARGET)],
            tool="living_sofa_prepare",
        )
        if status != 0:
            raise ValueError("Blender geometry preparation failed")
        generated = texture_bytes(source)
        for asset_id, data in generated.items():
            path = TEXTURES[asset_id][1]
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            print(
                f"living_sofa_prepare: wrote {path.relative_to(REPO)} "
                f"{len(data)} bytes sha256:{digest(data)}"
            )


    def primitives_for(document: dict, suffix: str | None) -> list[dict]:
        result = []
        for node in document["nodes"]:
            name = node.get("name", "")
            if "mesh" not in node or name == COLLISION_NAME:
                continue
            is_lod1 = name.endswith("_LOD1")
            if (suffix == "LOD1") != is_lod1:
                continue
            result.extend(document["meshes"][node["mesh"]]["primitives"])
        return result


    def triangle_count(document: dict, primitives: list[dict]) -> int:
        return sum(document["accessors"][primitive["indices"]]["count"] // 3
                   for primitive in primitives)


    def validate_model(row: dict) -> None:
        data = TARGET.read_bytes()
        if digest(data) != row["sourceSha256"]:
            raise ValueError("committed formal sofa hash differs from assets.manifest.json")
        if row.get("materialMap") != EXPECTED_MATERIAL_MAP:
            raise ValueError("manifest does not preserve the six verified sofa roles")
        origin = row.get("origin", {})
        if (origin.get("author") != "Darmstadt Graphics Group GmbH" or
                origin.get("licence") != "CC-BY-4.0" or
                origin.get("licenceFile") != "licenses/cc-by-4.0/LICENCE.txt" or
                SOURCE_SHA256 not in origin.get("note", "")):
            raise ValueError("manifest does not preserve pinned Darmstadt/CC-BY-4.0 provenance")
        document, _blob = gltf_io.read_model(TARGET)
        if any(key in document for key in (
                "extensions", "extensionsUsed", "extensionsRequired", "images", "textures",
                "samplers")):
            raise ValueError("prepared formal sofa retains unsupported runtime metadata")
        visible_names = {node.get("name", "") for node in document["nodes"]
                         if "mesh" in node and not node.get("name", "").endswith("_LOD1") and
                         node.get("name") != COLLISION_NAME}
        if visible_names != set(VISIBLE_NODES):
            raise ValueError(f"unexpected LOD0 nodes {sorted(visible_names)}")
        lod1_names = {node.get("name", "") for node in document["nodes"]
                      if node.get("name", "").endswith("_LOD1")}
        if lod1_names != {f"{name}_LOD1" for name in VISIBLE_NODES}:
            raise ValueError(f"unexpected LOD1 nodes {sorted(lod1_names)}")
        materials = {material.get("name", "") for material in document.get("materials", [])}
        if materials != set(SOURCE_MATERIALS):
            raise ValueError(f"unexpected prepared material roles {sorted(materials)}")
        lod0 = primitives_for(document, None)
        lod1 = primitives_for(document, "LOD1")
        for primitive in [*lod0, *lod1]:
            if "TEXCOORD_0" not in primitive.get("attributes", {}):
                raise ValueError("a prepared close-range sofa primitive lacks UV0")
        measured_lod0 = triangle_count(document, lod0)
        measured_lod1 = triangle_count(document, lod1)
        declared = row["geometry"]["triangles"]
        if (measured_lod0 != EXPECTED_LOD0_TRIANGLES or
                declared.get("LOD0") != measured_lod0):
            raise ValueError(f"LOD0 triangles are {measured_lod0}, expected {EXPECTED_LOD0_TRIANGLES}")
        if (measured_lod1 != EXPECTED_LOD1_TRIANGLES or
                declared.get("LOD1") != measured_lod1):
            raise ValueError(f"LOD1 triangles are {measured_lod1}, expected {EXPECTED_LOD1_TRIANGLES}")
        collision = next((node for node in document["nodes"]
                          if node.get("name") == COLLISION_NAME), None)
        if collision is None or row["geometry"].get("collision") != COLLISION_NAME:
            raise ValueError("formal sofa collision proxy is missing")
        proxy = document["meshes"][collision["mesh"]]["primitives"]
        if triangle_count(document, proxy) != 12:
            raise ValueError("formal sofa collision proxy is not the twelve-triangle box")
        bounds = scale_check.accessor_bounds(document)
        measured_bounds = [round(high - low, 6) for low, high in zip(*bounds)]
        if any(abs(actual - expected) > 0.00001
               for actual, expected in zip(measured_bounds, EXPECTED_BOUNDS)):
            raise ValueError(f"prepared bounds {measured_bounds} differ from {EXPECTED_BOUNDS}")
        if row["geometry"].get("boundsMetres") != EXPECTED_BOUNDS:
            raise ValueError("manifest formal-sofa bounds are stale")
        problems = scale_check.check(TARGET, row["category"], row["geometry"])
        if problems:
            raise ValueError("; ".join(problems))
        problems = origin_check.check(TARGET, row["category"])
        if problems:
            raise ValueError("; ".join(problems))
        problems = gltf_validate.validate(TARGET, gltf_validate.find_cna_content())
        if problems:
            raise ValueError("; ".join(problems))


    def validate_textures(rows: dict[str, dict], source: Path | None) -> None:
        generated = texture_bytes(source) if source is not None else None
        for asset_id, (_index, path, expected_size) in TEXTURES.items():
            data = path.read_bytes()
            row = rows[asset_id]
            if digest(data) != row.get("sourceSha256"):
                raise ValueError(f"{asset_id}: committed PNG hash differs from manifest")
            if generated is not None and data != generated[asset_id]:
                raise ValueError(f"{asset_id}: committed PNG differs from pinned-source extraction")
            image = Image.open(io.BytesIO(data))
            if image.size != expected_size:
                raise ValueError(f"{asset_id}: {image.size} differs from {expected_size}")
            if row.get("texture", {}).get("colourSpace") != "sRGB":
                raise ValueError(f"{asset_id}: albedo is not declared sRGB")


    def validate_world() -> None:
        layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props", "materials"])
        props = layout_io.by_id(layout_io.rows(layout, "props"), "prop")
        sofa = props["PROP_LIVING_SOFA"]
        if (sofa["asset"] != ASSET_ID or sofa["cell"] != "L0_LIVING" or
                sofa["position"] != [-5.2, 0.6, -19.15] or sofa["yawDeg"] != 0 or
                sofa["scale"] != 1 or sofa["collision"] != "proxy"):
            raise ValueError("formal living sofa placement or collision changed")
        if props["PROP_FAMILY_SOFA"]["asset"] != "MODEL_FAMILY_GLAM_VELVET_SOFA":
            raise ValueError("formal sofa replacement changed the distinct family sofa")
        materials = layout_io.by_id(layout_io.rows(layout, "materials"), "material")
        for material_id in set(EXPECTED_MATERIAL_MAP.values()):
            if material_id not in materials:
                raise ValueError(f"canonical formal sofa material {material_id} is missing")
        fringe = materials["MAT_LIVING_SOFA_FRINGE"]
        if (fringe.get("alphaMode") != "mask" or fringe.get("alphaCutoff") != 0.35 or
                fringe.get("effectTierS") != "AlphaTest" or
                fringe.get("tint") != [0.45, 0.30, 0.18] or not fringe.get("twoSided")):
            raise ValueError(
                "formal sofa fringe must retain the reviewed dark Tier-S cutout adaptation"
            )


    def validate_committed(source: Path | None) -> None:
        rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
        validate_model(rows[ASSET_ID])
        validate_textures(rows, source)
        validate_world()
        if source is not None:
            with tempfile.TemporaryDirectory(prefix="house01057-sofa-", dir="/tmp") as scratch:
                candidate = Path(scratch) / "formal_sofa.glb"
                status = blender_env.relaunch(
                    Path(__file__).resolve(),
                    ["--blender-write", str(source), str(candidate)],
                    tool="living_sofa_prepare",
                )
                if status != 0 or candidate.read_bytes() != TARGET.read_bytes():
                    raise ValueError("committed formal sofa differs from deterministic preparation")
        print(
            f"living_sofa_prepare: {TARGET.stat().st_size} bytes "
            f"{digest(TARGET.read_bytes())[:16]} deterministic, "
            f"LOD0 {EXPECTED_LOD0_TRIANGLES}, LOD1 {EXPECTED_LOD1_TRIANGLES}, proxy 12"
        )


    def outer_main() -> int:
        parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
        mode = parser.add_mutually_exclusive_group(required=True)
        mode.add_argument("--write", action="store_true")
        mode.add_argument("--check", action="store_true")
        parser.add_argument("--source", type=Path)
        options = parser.parse_args()
        try:
            if options.write:
                if options.source is None:
                    parser.error("--write requires --source")
                write_assets(options.source)
            else:
                validate_committed(options.source)
        except (KeyError, OSError, ValueError, gltf_io.GltfError) as error:
            print(f"living_sofa_prepare: {error}", file=sys.stderr)
            return 1
        return 0


if __name__ == "__main__":
    if INSIDE_BLENDER:
        try:
            _status = blender_main()
        except BaseException:  # noqa: BLE001 - Blender does not propagate script failures
            import traceback

            traceback.print_exc()
            _status = 1
        print(f"living_sofa_prepare: EXIT {_status}")
        raise SystemExit(_status)
    raise SystemExit(outer_main())
