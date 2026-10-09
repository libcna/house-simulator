#!/usr/bin/env python3
"""Acquire and verify HOUSE-00297's fixed Poly Haven vegetation set.

Only the 22 named CC0 assets below are fetched through Poly Haven's public API.  Source glTFs live
in a caller-supplied cache; the repository receives 34 derived, grounded and budgeted GLBs:
six botanically distinct tree species at three ages, nine shrubs, five flowers and two grass-card
sets.  Tree ages are separate meshes, not placement-time scale aliases.

Usage:
    tools/assets/polyhaven_vegetation.py --download --cache /tmp/house-00297
    tools/assets/polyhaven_vegetation.py --download --cache /tmp/house-00297 --source periwinkle_plant
    tools/assets/polyhaven_vegetation.py --emit-runtime
    tools/assets/polyhaven_vegetation.py --check
    tools/assets/polyhaven_vegetation.py --selftest
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from dataclasses import dataclass
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gltf_io  # noqa: E402
import manifest as manifest_tool  # noqa: E402
import scale_check  # noqa: E402


REPO = Path(__file__).resolve().parents[2]
OUTPUT = REPO / "assets-src" / "Models" / "Vegetation"
REPORT = REPO / "docs" / "asset-selection" / "vegetation-set.json"
BLENDER_PREPARE = REPO / "tools" / "blender" / "vegetation_prepare.py"
LOD_GEN = REPO / "tools" / "blender" / "lod_gen.py"
API = "https://api.polyhaven.com/files/"
MAX_TEXTURE_EDGE = 256
RETRIEVED = "2026-09-13"
RUNTIME_TEXTURE_DIR = REPO / "assets-src" / "Textures" / "Vegetation"
RUNTIME_TEXTURES = (
    ("tree_foliage.png", "tree_jacaranda_mature", "jacaranda_tree_leaves",
     "TEXTURE_VEGETATION_TREE_FOLIAGE", "vegetation-tree-albedo", "Jacaranda leaf atlas"),
    ("shrub_foliage.png", "shrub_01", "shrub_01",
     "TEXTURE_VEGETATION_SHRUB_FOLIAGE", "vegetation-shrub-albedo", "Shrub 01 leaf atlas"),
    ("flower.png", "flower_gazania", "flower_gazania",
     "TEXTURE_VEGETATION_FLOWER", "vegetation-flower-albedo", "Gazania flower atlas"),
    ("grass.png", "grass_bermuda_01", "grass_bermuda_01",
     "TEXTURE_VEGETATION_GRASS", "vegetation-grass-albedo", "Bermuda grass atlas"),
    ("jacaranda_branches.png", "tree_jacaranda_mature", "jacaranda_tree_branches",
     "TEXTURE_VEGETATION_JACARANDA_BRANCHES", "vegetation-tree-albedo",
     "Jacaranda branch atlas"),
    ("jacaranda_trunk.png", "tree_jacaranda_mature", "jacaranda_tree_trunk",
     "TEXTURE_VEGETATION_JACARANDA_TRUNK", "vegetation-tree-albedo",
     "Jacaranda trunk atlas"),
    ("searsia_lucida.png", "tree_searsia_lucida_mature", "searsia_lucida_leaves",
     "TEXTURE_VEGETATION_SEARSIA_LUCIDA", "vegetation-tree-albedo",
     "Searsia lucida cutout atlas"),
    ("tree_small_02_trunk.png", "tree_tree_small_02_mature", "tree_small_02_trunk",
     "TEXTURE_VEGETATION_TREE_SMALL_02_TRUNK", "vegetation-tree-albedo",
     "Wild syringa trunk atlas"),
    ("tree_small_02_branches.png", "tree_tree_small_02_mature", "tree_small_02_branches",
     "TEXTURE_VEGETATION_TREE_SMALL_02_BRANCHES", "vegetation-tree-albedo",
     "Wild syringa branch atlas"),
    ("tree_small_02_leaves.png", "tree_tree_small_02_mature", "tree_small_02_leaves",
     "TEXTURE_VEGETATION_TREE_SMALL_02_LEAVES", "vegetation-tree-albedo",
     "Wild syringa leaf atlas"),
    ("shrub_04.png", "shrub_04", "shrub_04",
     "TEXTURE_VEGETATION_SHRUB_04", "vegetation-shrub-albedo", "Shrub 04 cutout atlas"),
    ("periwinkle.png", "periwinkle_plant", "periwinkle_plant",
     "TEXTURE_VEGETATION_PERIWINKLE", "vegetation-flower-albedo",
     "Periwinkle cutout atlas"),
    ("grass_medium_02.png", "grass_medium_02", "grass_medium_02",
     "TEXTURE_VEGETATION_GRASS_MEDIUM_02", "vegetation-grass-albedo",
     "Grass medium 02 cutout atlas"),
)

# Source-specific slots used by HOUSE-00772's playable planting. Four deliberately share the
# compact role textures above because those textures come from the exact same source material;
# the remaining nine retain their own atlas. Catalogue assets not placed yet keep the generic
# fallback roles until their own visual slice gives them a reason to enter runtime content.
EXACT_RUNTIME_MATERIALS = {
    ("jacaranda_tree", "jacaranda_tree_leaves"): "MAT_VEGETATION_TREE_FOLIAGE",
    ("jacaranda_tree", "jacaranda_tree_branches"): "MAT_VEGETATION_JACARANDA_BRANCHES",
    ("jacaranda_tree", "jacaranda_tree_trunk"): "MAT_VEGETATION_JACARANDA_TRUNK",
    ("searsia_lucida", "searsia_lucida_twigs"): "MAT_VEGETATION_SEARSIA_LUCIDA",
    ("searsia_lucida", "searsia_lucida"): "MAT_VEGETATION_SEARSIA_LUCIDA",
    ("searsia_lucida", "searsia_lucida_leaves"): "MAT_VEGETATION_SEARSIA_LUCIDA",
    ("tree_small_02", "tree_small_02_trunk"): "MAT_VEGETATION_TREE_SMALL_02_TRUNK",
    ("tree_small_02", "tree_small_02_branches"): "MAT_VEGETATION_TREE_SMALL_02_BRANCHES",
    ("tree_small_02", "tree_small_02_leaves"): "MAT_VEGETATION_TREE_SMALL_02_LEAVES",
    ("shrub_01", "shrub_01"): "MAT_VEGETATION_SHRUB_FOLIAGE",
    ("shrub_04", "shrub_04"): "MAT_VEGETATION_SHRUB_04",
    ("flower_gazania", "flower_gazania"): "MAT_VEGETATION_FLOWER",
    ("periwinkle_plant", "periwinkle_plant"): "MAT_VEGETATION_PERIWINKLE",
    ("grass_bermuda_01", "grass_bermuda_01"): "MAT_VEGETATION_GRASS",
    ("grass_medium_02", "grass_medium_02"): "MAT_VEGETATION_GRASS_MEDIUM_02",
}


@dataclass(frozen=True)
class Source:
    slug: str
    role: str
    name: str
    metadata_sha256: str
    height: float


@dataclass(frozen=True)
class Output:
    slug: str
    source: str
    role: str
    height: float
    crown: float
    triangles: int
    age: str = ""

    @property
    def asset_id(self) -> str:
        return "MODEL_VEGETATION_" + self.slug.upper()

    @property
    def category(self) -> str:
        return f"tree-{self.age}" if self.role == "tree" else self.role


SOURCES = (
    Source("fir_sapling", "tree", "Fir", "4a87921ef6b878af2d0789ea9b9da4855c76f98a5c8ad6ca68bf263fee4e89c6", 12.0),
    Source("pine_sapling_small", "tree", "Pine", "8bb21a7caba0440844f6c23aeb2d9514212100e0ae4633682c6aabe3d256247d", 12.0),
    Source("tree_small_02", "tree", "Burkea africana / wild syringa", "48b4580b9e3d30b6e4624e12ba3676b757ca6e213bbcd49b2deea067dcd0213e", 12.0),
    Source("jacaranda_tree", "tree", "Jacaranda", "1c30ebc67d9db03cd4b919f6402f02fac1e1a9eacd137512e23d5fc437dcd335", 12.0),
    Source("searsia_burchellii", "tree", "Searsia burchellii", "56c3671c989d41a7ff478f215f79ee6ebdfaa6915ba9f81d78ffa8afc5781497", 12.0),
    Source("searsia_lucida", "tree", "Searsia lucida", "a7141bfcec3a9fa1c89d3687e214509c23393e1026b30f0b257713d74c2a43cb", 12.0),
    Source("shrub_01", "shrub", "Shrub 01", "dea1133f7d26d43585196a444a40fe09e47a373237c040a2b7d42ad1056cb25e", 1.40),
    Source("shrub_02", "shrub", "Shrub 02", "5b9ad5b93dddf1b439ab55c911fa33d365ce83e2b98308e67e257d3dfb74a9fa", 1.80),
    Source("shrub_03", "shrub", "Shrub 03", "01c34bcc8da5e97b9a84350254de868016d348f067df98782a368e7fca088a03", 0.90),
    Source("shrub_04", "shrub", "Shrub 04", "982d40288050ce2d06fdbe3518d444ef177a7d6defd3362aa8dec109c9a8f79a", 0.75),
    Source("fern_02", "shrub", "Fern 02", "a3e43a1dad9a175defdc5816daed6ab1192d4dcd4c8f8a5becd44e63616ba0b3", 0.65),
    Source("nettle_plant", "shrub", "Nettle plant", "1d4d781ce038ba3ab03c41eadce1f4765a5f97ba648644458a79ab22e13c9c41", 0.90),
    Source("weed_plant_02", "shrub", "Weed plant 02", "df40d0d263ecb21f47ff67ccbe93f161ebcba3db186c8d74acbf5165a5557d6b", 0.70),
    Source("shrub_sorrel_01", "shrub", "Shrub sorrel 01", "edf9c67a2bdec6f9b566a01e0d81d3e313b1421348b30cb9ebb4a8735309254a", 0.50),
    Source("wild_rooibos_bush", "shrub", "Wild rooibos bush", "69e7d02fd3b6e672011f6e3bc21e468804140092208e6423afcac9ec5f0dd165", 1.30),
    Source("celandine_01", "flower", "Celandine 01", "3f90d3ad3e4e9d86df8ac7b5be5959f69a5606172cfbd99e0027d58beed63992", 0.35),
    Source("dandelion_01", "flower", "Dandelion 01", "051928b3dd73311002fb3733fbc96c7817c9c593f34dac5e7f287718b83b71c8", 0.25),
    Source("periwinkle_plant", "flower", "Periwinkle plant", "42a5696a46c9f77a1271edcf05494da138beac99f96b6afb0c420f6ce5836fd4", 0.30),
    Source("flower_gazania", "flower", "Gazania", "ed8838dfded6d0bb367e707bf1e3328824857eff3a2e807babdbe447fabfa2af", 0.30),
    Source("flower_stinkkruid", "flower", "Stinkkruid", "0ae95bb8b60f8373ecc1e9426ea20942fd3b0929b6115d40898bba7b0e1e766a", 0.45),
    Source("grass_bermuda_01", "grass-card", "Bermuda grass 01", "3f3e9caed69948b36bc58b06163216b81c1bdb1d26dc7ec8040cf1d5e8cb58d3", 0.28),
    Source("grass_medium_02", "grass-card", "Grass medium 02", "91aea80de8b7f267adf168784cd3b608ca5e70e7948e74799a0a74b20ef8baee", 0.55),
)


def outputs() -> tuple[Output, ...]:
    result: list[Output] = []
    for source in SOURCES:
        if source.role == "tree":
            stem = source.slug.removesuffix("_sapling").removesuffix("_sapling_small").removesuffix("_tree")
            for age, height, crown, triangles in (
                ("sapling", 3.5, 0.55, 4800),
                ("young", 7.0, 0.78, 7000),
                ("mature", 12.0, 1.0, 9000),
            ):
                result.append(Output(f"tree_{stem}_{age}", source.slug, "tree", height, crown,
                                     triangles, age))
        else:
            target = 4800 if source.role == "shrub" else 1200 if source.role == "flower" else 900
            result.append(Output(source.slug, source.slug, source.role, source.height, 1.0, target))
    return tuple(result)


OUTPUTS = outputs()
BY_SOURCE = {source.slug: source for source in SOURCES}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def download_bytes(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "cna-house/HOUSE-00297"})
    with urllib.request.urlopen(request) as response:
        return response.read()


def cached_download(path: Path, url: str, *, md5: str | None = None,
                    sha256: str | None = None) -> bytes:
    data = path.read_bytes() if path.is_file() else download_bytes(url)
    if md5 and hashlib.md5(data).hexdigest() != md5:  # noqa: S324 - upstream supplies MD5
        raise ValueError(f"{url}: MD5 differs from the pinned Poly Haven metadata")
    if sha256 and digest(data) != sha256:
        raise ValueError(f"{url}: SHA-256 differs from the pinned metadata snapshot")
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.is_file():
        path.write_bytes(data)
    return data


def alpha_entry(metadata: dict, image_name: str) -> dict | None:
    candidates = []
    for key, value in metadata.items():
        lowered = key.lower()
        if lowered in ("alpha", "opacity") or lowered.endswith(("_alpha", "_opacity")):
            prefix = lowered.removesuffix("_alpha").removesuffix("_opacity")
            if lowered in ("alpha", "opacity") or prefix in image_name.lower():
                entry = value.get("1k", {}).get("png")
                if entry:
                    candidates.append(entry)
    return candidates[0] if len(candidates) == 1 else None


def resize(image: Image.Image) -> Image.Image:
    scale = min(1.0, MAX_TEXTURE_EDGE / max(image.size))
    size = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
    return image.resize(size, Image.Resampling.LANCZOS) if size != image.size else image.copy()


def stage_source(source: Source, cache: Path, stage: Path) -> Path:
    metadata_path = cache / source.slug / "files.json"
    raw = cached_download(metadata_path, API + source.slug, sha256=source.metadata_sha256)
    metadata = json.loads(raw)
    entry = metadata["gltf"]["1k"]["gltf"]
    gltf_data = cached_download(cache / source.slug / "source.gltf", entry["url"], md5=entry["md5"])
    document = json.loads(gltf_data)

    # Geometry and all original includes are cached byte-for-byte.  Only colour images are staged:
    # stock XNA materials cannot consume glTF normal/ARM maps, and embedding them would spend the
    # exterior budget on data the runtime never samples.
    includes = entry.get("include", {})
    for relative, item in includes.items():
        if relative.endswith(".bin"):
            data = cached_download(cache / source.slug / relative, item["url"], md5=item["md5"])
            target = stage / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)

    old_images = document.get("images", [])
    old_textures = document.get("textures", [])
    new_images = []
    new_textures = []
    texture_map: dict[int, int] = {}
    for material in document.get("materials", []):
        pbr = material.setdefault("pbrMetallicRoughness", {})
        base = pbr.get("baseColorTexture")
        material.pop("normalTexture", None)
        pbr.pop("metallicRoughnessTexture", None)
        pbr["metallicFactor"] = 0.0
        pbr["roughnessFactor"] = 0.82
        material.pop("extensions", None)
        if not base:
            continue
        old_texture = base["index"]
        if old_texture not in texture_map:
            image = old_images[old_textures[old_texture]["source"]]
            relative = image["uri"]
            item = includes[relative]
            raw_image = cached_download(cache / source.slug / relative, item["url"], md5=item["md5"])
            with Image.open(io.BytesIO(raw_image)) as decoded:
                colour = resize(decoded.convert("RGB"))
            alpha = alpha_entry(metadata, image.get("name", relative))
            if alpha:
                alpha_raw = cached_download(cache / source.slug / "alpha" / Path(alpha["url"]).name,
                                            alpha["url"], md5=alpha["md5"])
                with Image.open(io.BytesIO(alpha_raw)) as decoded:
                    mask = resize(decoded.convert("L")).resize(colour.size, Image.Resampling.LANCZOS)
                colour = colour.convert("RGBA")
                colour.putalpha(mask)
                material["alphaMode"] = "MASK"
                material["alphaCutoff"] = 0.45
            filename = f"textures/colour_{len(new_images):02}.png"
            target = stage / filename
            target.parent.mkdir(parents=True, exist_ok=True)
            colour.save(target, format="PNG", optimize=True)
            texture_map[old_texture] = len(new_textures)
            new_images.append({"uri": filename, "mimeType": "image/png", "name": image.get("name")})
            new_textures.append({"source": len(new_images) - 1})
        base["index"] = texture_map[old_texture]
    document["images"] = new_images
    document["textures"] = new_textures
    document["extensionsUsed"] = [value for value in document.get("extensionsUsed", [])
                                    if value == "KHR_texture_transform"]
    if not document["extensionsUsed"]:
        document.pop("extensionsUsed", None)
    target = stage / "source.gltf"
    target.write_text(json.dumps(document, separators=(",", ":")), encoding="utf-8")
    return target


def run(command: list[str]) -> str:
    result = subprocess.run(command, cwd=REPO, text=True, capture_output=True)
    if result.returncode:
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        raise RuntimeError(f"command failed ({result.returncode}): {' '.join(command)}")
    for line in result.stdout.splitlines():
        if line.startswith(("vegetation_prepare:", "lod_gen:")) or "_JSON " in line:
            print(line, flush=True)
    return result.stdout


def normalise_samplers(path: Path) -> None:
    """Declare non-mip linear sampling for embedded single-level PNG textures.

    Blender exports ``LINEAR_MIPMAP_LINEAR`` even though a GLB image carries only level zero. CNA
    correctly warns about that contradiction. The content pipeline owns runtime mip generation;
    the source declaration must describe the source that actually exists.
    """
    document, blob = gltf_io.read_model(path)
    changed = False
    for sampler in document.get("samplers", []):
        if sampler.get("minFilter") != 9729 or sampler.get("magFilter") != 9729:
            sampler["minFilter"] = 9729  # LINEAR, no mip lookup
            sampler["magFilter"] = 9729
            changed = True
    if changed:
        gltf_io.write_glb(path, document, blob)


def runtime_material_map(item: Output) -> dict[str, str]:
    """Map every retained source slot to a stock-XNA vegetation material.

    The GLBs keep their complete embedded source textures for the ordinary CNA model pipeline.
    Static chunks cannot reach a model-owned texture, so the playable exterior extracts the exact
    atlas for every model HOUSE-00772 places. The rest of the prepared catalogue uses four compact
    fallback roles plus outdoor wood until it is selected for a later visual slice.
    """
    path = OUTPUT / f"{item.slug}.glb"
    document, _blob = gltf_io.read_model(path)
    names = [material.get("name") for material in document.get("materials", [])]
    if any(not isinstance(name, str) or not name for name in names):
        raise ValueError(f"{path}: every source material needs a stable name")
    exact = {name: EXACT_RUNTIME_MATERIALS[(item.source, name)]
             for name in names if (item.source, name) in EXACT_RUNTIME_MATERIALS}
    if item.role == "tree":
        return {
            name: exact.get(name, "MAT_VEGETATION_TREE_FOLIAGE"
                            if "leaf" in name.lower() or "leaves" in name.lower() or
                            "twig" in name.lower() else "MAT_VEGETATION_BARK")
            for name in names
        }
    role = {
        "shrub": "MAT_VEGETATION_SHRUB_FOLIAGE",
        "flower": "MAT_VEGETATION_FLOWER",
        "grass-card": "MAT_VEGETATION_GRASS",
    }[item.role]
    return {name: exact.get(name, role) for name in names}


def embedded_material_image(model_slug: str, material_name: str) -> bytes:
    path = OUTPUT / f"{model_slug}.glb"
    document, blob = gltf_io.read_model(path)
    buffers = gltf_io.buffer_bytes(document, blob, path.parent)
    material = next((row for row in document.get("materials", [])
                     if row.get("name") == material_name), None)
    if material is None:
        raise ValueError(f"{path}: no material {material_name!r}")
    texture_index = material.get("pbrMetallicRoughness", {}).get("baseColorTexture", {}).get("index")
    if not isinstance(texture_index, int):
        raise ValueError(f"{path}: material {material_name!r} has no base-colour texture")
    textures = document.get("textures", [])
    if texture_index < 0 or texture_index >= len(textures):
        raise ValueError(f"{path}: material {material_name!r} has an invalid texture index")
    image_index = textures[texture_index].get("source")
    if not isinstance(image_index, int):
        raise ValueError(f"{path}: texture {texture_index} has no source image")
    return gltf_io.image_bytes(document, buffers, path.parent, image_index)


def emit_runtime_textures() -> None:
    RUNTIME_TEXTURE_DIR.mkdir(parents=True, exist_ok=True)
    for filename, model_slug, material_name, _asset_id, _category, _display in RUNTIME_TEXTURES:
        data = embedded_material_image(model_slug, material_name)
        with Image.open(io.BytesIO(data)) as image:
            if (image.format != "PNG" or image.mode not in ("RGB", "RGBA") or
                    max(image.size) > MAX_TEXTURE_EDGE):
                raise ValueError(
                    f"{model_slug}/{material_name}: expected <= {MAX_TEXTURE_EDGE}px RGB(A) PNG, "
                    f"got {image.format} {image.mode} {image.size}")
        (RUNTIME_TEXTURE_DIR / filename).write_bytes(data)


def runtime_texture_rows() -> list[dict]:
    rows = []
    output_by_slug = {item.slug: item for item in OUTPUTS}
    for filename, model_slug, _material_name, asset_id, category, display in RUNTIME_TEXTURES:
        path = RUNTIME_TEXTURE_DIR / filename
        item = output_by_slug[model_slug]
        source = BY_SOURCE[item.source]
        with Image.open(path) as image:
            width, height = image.size
            channels = image.mode
        rows.append({
            "id": asset_id,
            "category": category,
            "sourceFile": path.relative_to(REPO).as_posix(),
            "sourceSha256": digest(path.read_bytes()),
            "texture": {"width": width, "height": height,
                        "colourSpace": "sRGB", "channelLayout": channels},
            "contentName": f"Textures/Vegetation/{path.stem}",
            "kind": "texture",
            "residencyPack": "exterior",
            "origin": {
                "kind": "derived",
                "name": display,
                "url": f"https://polyhaven.com/a/{source.slug}",
                "retrieved": RETRIEVED,
                "licence": "CC0-1.0",
                "licenceFile": "licenses/cc0-1.0/LICENCE.txt",
                "attribution": "",
                "redistributeSource": True,
                "redistributeDerived": True,
                "commercialUse": True,
                "modification": True,
                "note": ("Byte-identical stock-XNA runtime extraction from the pinned prepared "
                         "vegetation GLB; the originating model and metadata remain recorded by "
                         "polyhaven_vegetation.py and SOURCE.md."),
            },
        })
    return rows


def update_runtime_bridge() -> None:
    emit_runtime_textures()
    document = manifest_tool.load()
    rows = {row.get("id"): row for row in document["assets"]}
    for item in OUTPUTS:
        row = rows.get(item.asset_id)
        if row is None:
            raise ValueError(f"manifest is missing {item.asset_id}; prepare the model set first")
        row["materialMap"] = runtime_material_map(item)
    texture_ids = {row[3] for row in RUNTIME_TEXTURES}
    document["assets"] = [row for row in document["assets"] if row.get("id") not in texture_ids]
    document["assets"].extend(runtime_texture_rows())
    manifest_tool.save(document)


def prepare_all(cache: Path, sources: tuple[Source, ...] = SOURCES) -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    # build/ is where a developer tree keeps scratch; a fresh checkout (CI) has none.
    (REPO / "build").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="p2-vegetation-", dir=REPO / "build") as work_text:
        work = Path(work_text)
        for source in sources:
            stage = work / source.slug
            staged = stage_source(source, cache, stage)
            selected = [item for item in OUTPUTS if item.source == source.slug]
            specs = []
            for item in selected:
                specs.append({
                    "output": str(work / f"{item.slug}.raw.glb"),
                    "name": item.slug,
                    "height": item.height,
                    "crown": item.crown,
                    "target": item.triangles,
                    "oneTree": item.role == "tree",
                    "max_ratio": 0.0 if item.role == "tree" else 2.0,
                })
            spec_path = stage / "specs.json"
            spec_path.write_text(json.dumps(specs), encoding="utf-8")
            run([sys.executable, str(BLENDER_PREPARE), str(staged), str(spec_path)])
            for item in selected:
                raw = work / f"{item.slug}.raw.glb"
                target = OUTPUT / f"{item.slug}.glb"
                if item.role in ("tree", "shrub") and item.triangles > 4000:
                    run([sys.executable, str(LOD_GEN), "--non-strict", str(raw), str(target)])
                else:
                    shutil.copyfile(raw, target)
    for item in OUTPUTS:
        normalise_samplers(OUTPUT / f"{item.slug}.glb")
    emit_runtime_textures()
    register_manifest()
    write_report()
    write_source()


def mesh_triangles(document: dict) -> dict[str, int]:
    result = {}
    for mesh in document.get("meshes", []):
        total = 0
        for primitive in mesh.get("primitives", []):
            index = primitive.get("indices")
            if index is not None:
                total += document["accessors"][index]["count"] // 3
        result[mesh.get("name", "unnamed")] = total
    return result


def inspect(item: Output) -> dict:
    path = OUTPUT / f"{item.slug}.glb"
    document, blob = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise ValueError(f"{path}: no POSITION bounds")
    low, high = bounds
    buffers = gltf_io.buffer_bytes(document, blob, path.parent)
    edges = []
    for index in range(len(document.get("images", []))):
        with Image.open(io.BytesIO(gltf_io.image_bytes(document, buffers, path.parent, index))) as image:
            edges.append(max(image.size))
    return {
        "id": item.asset_id,
        "file": path.relative_to(REPO).as_posix(),
        "source": item.source,
        "role": item.role,
        "age": item.age or None,
        "boundsMetres": [round(high[i] - low[i], 4) for i in range(3)],
        "triangles": mesh_triangles(document),
        "maxTextureEdge": max(edges, default=0),
        "bytes": path.stat().st_size,
        "sha256": digest(path.read_bytes()),
    }


def report_document() -> dict:
    return {
        "schema": "cna-house/vegetation-set/1",
        "source": "Poly Haven CC0",
        "retrieved": RETRIEVED,
        "sourceAssetCount": len(SOURCES),
        "outputAssetCount": len(OUTPUTS),
        "assets": [inspect(item) for item in OUTPUTS],
    }


def write_report() -> None:
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report_document(), indent=2) + "\n", encoding="utf-8")


def register_manifest() -> None:
    document = manifest_tool.load()
    wanted = {item.asset_id for item in OUTPUTS}
    document["assets"] = [row for row in document["assets"] if row.get("id") not in wanted]
    for item in OUTPUTS:
        path = OUTPUT / f"{item.slug}.glb"
        source = BY_SOURCE[item.source]
        document["assets"].append({
            "id": item.asset_id,
            "category": item.category,
            "sourceFile": path.relative_to(REPO).as_posix(),
            "sourceSha256": digest(path.read_bytes()),
            "contentName": f"Models/Vegetation/{item.slug}",
            "kind": "model",
            "residencyPack": "exterior",
            "materialMap": runtime_material_map(item),
            "origin": {
                "kind": "derived",
                "name": f"{source.name} — {item.age or item.role}",
                "url": f"https://polyhaven.com/a/{source.slug}",
                "retrieved": RETRIEVED,
                "licence": "CC0-1.0",
                "licenceFile": "licenses/cc0-1.0/LICENCE.txt",
                "attribution": "",
                "redistributeSource": True,
                "redistributeDerived": True,
                "commercialUse": True,
                "modification": True,
                "note": "Poly Haven 1K glTF; textures reduced to 256 px, stock-XNA-unused PBR maps removed, mesh grounded/scaled/decimated. Exact source metadata is SHA-256 pinned by polyhaven_vegetation.py; see SOURCE.md.",
            },
        })
    texture_ids = {row[3] for row in RUNTIME_TEXTURES}
    document["assets"] = [row for row in document["assets"] if row.get("id") not in texture_ids]
    document["assets"].extend(runtime_texture_rows())
    manifest_tool.save(document)


def write_source() -> None:
    lines = [
        "# Vegetation — provenance", "",
        "`HOUSE-00297`. All outputs in this directory are derived from the specifically named",
        "Poly Haven assets below. The source 1K glTFs are cache inputs and are not committed.", "",
        "| Output manifest ids | Upstream asset | Canonical URL |", "|---|---|---|",
    ]
    for source in SOURCES:
        ids = ", ".join(f"`{item.asset_id}`" for item in OUTPUTS if item.source == source.slug)
        lines.append(f"| {ids} | {source.name} (`{source.slug}`) | <https://polyhaven.com/a/{source.slug}> |")
    lines += [
        "", "## Licence", "",
        "Poly Haven declares every asset CC0 1.0 and warrants that assets are original work of its",
        "staff or artists who directly donated/sold the work. The project verified those terms in",
        "`docs/licence-evidence/polyhaven.md`; the full licence is",
        "`licenses/cc0-1.0/LICENCE.txt`. Source and derivatives may be redistributed, modified and",
        "used commercially; attribution is not required. Retrieved 2026-09-13.", "",
        "## Modification", "",
        "`tools/assets/polyhaven_vegetation.py` pins every API metadata response, verifies every",
        "downloaded byte against its declared MD5, keeps only base-colour/alpha data needed by the",
        "stock XNA path, reduces textures to 256 px, and invokes Blender to centre, ground, scale",
        "and decimate each model. Tree ages are separate derived meshes; LOD1 and LOD2 use §26.1's",
        "0.35 and 0.12 triangle ratios. The exact measurements are in",
        "`docs/asset-selection/vegetation-set.json`.", "",
        "Poly Haven preview renders are deliberately absent: its ToS reserves them. Review images",
        "are rendered locally from these CC0-derived GLBs.", "",
    ]
    (OUTPUT / "SOURCE.md").write_text("\n".join(lines), encoding="utf-8")


def check() -> int:
    problems = []
    expected = {f"{item.slug}.glb" for item in OUTPUTS}
    actual = {path.name for path in OUTPUT.glob("*.glb")} if OUTPUT.is_dir() else set()
    problems += [f"missing {name}" for name in sorted(expected - actual)]
    problems += [f"unexpected {name}" for name in sorted(actual - expected)]
    if REPORT.is_file() and not problems:
        recorded = json.loads(REPORT.read_text(encoding="utf-8"))
        current = report_document()
        if recorded != current:
            problems.append(f"{REPORT.relative_to(REPO)} is stale; rerun --download")
    elif not REPORT.is_file():
        problems.append(f"missing {REPORT.relative_to(REPO)}")
    manifest = manifest_tool.load()
    rows = {row.get("id"): row for row in manifest["assets"]}
    for item in OUTPUTS:
        row = rows.get(item.asset_id)
        if row is None:
            problems.append(f"manifest is missing {item.asset_id}")
        elif row.get("category") != item.category:
            problems.append(f"{item.asset_id}: category is {row.get('category')}, expected {item.category}")
        elif row.get("materialMap") != runtime_material_map(item):
            problems.append(f"{item.asset_id}: runtime materialMap is stale; rerun --emit-runtime")
    for filename, model_slug, material_name, asset_id, category, _display in RUNTIME_TEXTURES:
        path = RUNTIME_TEXTURE_DIR / filename
        if not path.is_file():
            problems.append(f"missing {path.relative_to(REPO)}; rerun --emit-runtime")
            continue
        expected = embedded_material_image(model_slug, material_name)
        if path.read_bytes() != expected:
            problems.append(f"{path.relative_to(REPO)} is stale; rerun --emit-runtime")
        row = rows.get(asset_id)
        if row is None:
            problems.append(f"manifest is missing {asset_id}")
        elif row.get("category") != category or row.get("sourceSha256") != digest(expected):
            problems.append(f"{asset_id}: runtime texture manifest row is stale")
    source_text = (OUTPUT / "SOURCE.md").read_text(encoding="utf-8") if (OUTPUT / "SOURCE.md").is_file() else ""
    for item in OUTPUTS:
        if item.asset_id not in source_text:
            problems.append(f"SOURCE.md does not name {item.asset_id}")
    if not problems:
        for record in report_document()["assets"]:
            if record["maxTextureEdge"] > MAX_TEXTURE_EDGE:
                problems.append(f"{record['id']}: texture edge {record['maxTextureEdge']} > {MAX_TEXTURE_EDGE}")
            document, _blob = gltf_io.read_model(REPO / record["file"])
            for sampler in document.get("samplers", []):
                if sampler.get("minFilter") not in (None, 9728, 9729):
                    problems.append(f"{record['id']}: source declares a mipmapped minFilter")
            base = next((count for name, count in record["triangles"].items()
                         if not name.endswith(("_LOD1", "_LOD2"))), None)
            item = next(value for value in OUTPUTS if value.asset_id == record["id"])
            if base is None or base > item.triangles * 1.02:
                problems.append(f"{record['id']}: LOD0 triangle budget is not met")
            needs_lods = item.role in ("tree", "shrub") and base is not None and base > 4000
            levels = {name.rsplit("_", 1)[-1]: count
                      for name, count in record["triangles"].items()
                      if name.endswith(("_LOD1", "_LOD2"))}
            if needs_lods and set(levels) != {"LOD1", "LOD2"}:
                problems.append(f"{record['id']}: expected exactly LOD1 and LOD2")
            if base and needs_lods:
                for level, ratio in (("LOD1", 0.35), ("LOD2", 0.12)):
                    achieved = levels.get(level, 0) / base
                    if not ratio * 0.8 <= achieved <= ratio * 1.2:
                        problems.append(
                            f"{record['id']}: {level} ratio {achieved:.3f}, expected {ratio:.2f} +/-20%"
                        )
            path = REPO / record["file"]
            problems.extend(f"{record['id']}: {problem}"
                            for problem in scale_check.check(path, item.category))
    if problems:
        for problem in problems:
            print(f"polyhaven_vegetation: {problem}", file=sys.stderr)
        return 1
    roles = {role: sum(item.role == role for item in OUTPUTS)
             for role in ("tree", "shrub", "flower", "grass-card")}
    print(f"polyhaven_vegetation: clean -- {len(SOURCES)} pinned sources, {len(OUTPUTS)} GLBs "
          f"({roles}), textures <= {MAX_TEXTURE_EDGE}px")
    return 0


def selftest() -> int:
    counts = {role: sum(item.role == role for item in OUTPUTS)
              for role in ("tree", "shrub", "flower", "grass-card")}
    ages = {(item.source, item.age) for item in OUTPUTS if item.role == "tree"}
    valid_hashes = all(re.fullmatch(r"[0-9a-f]{64}", source.metadata_sha256) for source in SOURCES)
    tests = {
        "six tree species times three ages": len([s for s in SOURCES if s.role == "tree"]) == 6
        and counts["tree"] == 18 and len(ages) == 18,
        "nine shrubs, five flowers and two grass-card sets": counts == {
            "tree": 18, "shrub": 9, "flower": 5, "grass-card": 2},
        "all 22 metadata snapshots have pinned SHA-256": valid_hashes and len(SOURCES) == 22,
        "all 34 ids and filenames are unique": len({x.asset_id for x in OUTPUTS}) == 34
        and len({x.slug for x in OUTPUTS}) == 34,
        "tree triangle ceiling is 9000": all(x.triangles <= 9000 for x in OUTPUTS if x.role == "tree"),
        "opacity metadata is accepted as an alpha mask": alpha_entry(
            {"opacity": {"1k": {"png": {"url": "mask.png"}}}}, "plant_diff"
        ) == {"url": "mask.png"},
    }
    for name, passed in tests.items():
        print(("PASS" if passed else "FAIL") + f"  {name}")
    return 0 if all(tests.values()) else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--download", action="store_true")
    group.add_argument("--emit-runtime", action="store_true")
    group.add_argument("--check", action="store_true")
    group.add_argument("--selftest", action="store_true")
    parser.add_argument("--cache", type=Path)
    parser.add_argument("--source", choices=[source.slug for source in SOURCES],
                        help="rebuild one pinned source and then revalidate the complete set")
    args = parser.parse_args()
    if args.download:
        if args.cache is None:
            parser.error("--download requires --cache")
        selected = tuple(source for source in SOURCES if args.source in (None, source.slug))
        prepare_all(args.cache, selected)
        return check()
    if args.source:
        parser.error("--source requires --download")
    if args.emit_runtime:
        update_runtime_bridge()
        return check()
    return selftest() if args.selftest else check()


if __name__ == "__main__":
    sys.exit(main())
