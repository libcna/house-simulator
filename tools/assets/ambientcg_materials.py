#!/usr/bin/env python3
"""Acquire and prepare HOUSE-00296's fixed ambientCG base-material set.

The upstream 1K-JPG archives are download inputs, not repository assets.  This tool fetches only
the 34 explicitly selected archives, then writes three equally-sized runtime textures per material:

* ``*_albedo.png`` -- sRGB base colour;
* ``*_normal.png`` -- linear, OpenGL/glTF (+Y) tangent-space normal;
* ``*_orm.png`` -- linear occlusion/roughness/metalness in R/G/B.

The longest output edge is 256 pixels and the source aspect ratio is preserved.  Albedo and normal
are runtime textures; ORM stays manifested source for HOUSE-00900's PBR-to-stock scalar mapping,
because the application-owned material record has no ORM texture slot.  The 68 runtime maps cost
at most 22.4 MiB as RGBA8 with complete mip chains.  A 512-pixel edge would cost four times that
and exceed the 55 MB ``core`` pack on the currently supported uncompressed CNB path.

Usage:
    tools/assets/ambientcg_materials.py --download --cache /tmp/house-00296
    tools/assets/ambientcg_materials.py --check
    tools/assets/ambientcg_materials.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import shutil
import sys
import tempfile
import urllib.request
import zipfile
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageOps

sys.path.insert(0, str(Path(__file__).resolve().parent))
import atlas_pack  # noqa: E402


REPO = Path(__file__).resolve().parents[2]
OUTPUT = REPO / "assets-src" / "Textures" / "Materials"
CONTENT_CONFIG = REPO / "assets-src" / "Textures" / ".cna-content.json"
SIZE = 256
SOURCE_FORMAT = "1K-JPG"
BASE_URL = "https://ambientcg.com/get?file="


@dataclass(frozen=True)
class Material:
    category: str
    slug: str
    asset_id: str


MATERIALS = (
    Material("paint", "paint_white_fine", "PaintedPlaster017"),
    Material("paint", "paint_warm_fine", "PaintedPlaster016"),
    Material("paint", "paint_grey_fine", "PaintedPlaster015"),
    Material("paint", "paint_cream_rough", "PaintedPlaster010"),
    Material("paint", "paint_cool_rough", "PaintedPlaster014"),
    Material("paint", "paint_aged", "PaintedPlaster013"),
    Material("wood", "wood_oak_floor", "WoodFloor051"),
    Material("wood", "wood_light_floor", "WoodFloor064"),
    Material("wood", "wood_dark_floor", "WoodFloor070"),
    Material("wood", "wood_parquet_floor", "WoodFloor043"),
    Material("wood", "wood_worn_floor", "WoodFloor040"),
    Material("wood", "wood_board", "Wood095"),
    Material("tile", "tile_warm_square", "Tiles141"),
    Material("tile", "tile_grey_square", "Tiles139"),
    Material("tile", "tile_light_square", "Tiles140"),
    Material("tile", "tile_dark_square", "Tiles143"),
    Material("carpet", "carpet_beige", "Carpet016"),
    Material("carpet", "carpet_grey", "Carpet012"),
    Material("carpet", "carpet_brown", "Carpet001"),
    Material("stone", "stone_marble", "Marble012"),
    Material("stone", "stone_onyx", "Onyx015"),
    Material("stone", "stone_rough", "Rock064"),
    Material("brick", "brick_red", "Bricks097"),
    Material("plaster", "plaster_natural", "Plaster007"),
    Material("concrete", "concrete_smooth", "Concrete031"),
    Material("asphalt", "asphalt_road", "Asphalt033"),
    Material("gravel", "gravel_mixed", "Gravel043"),
    Material("grass", "grass_lawn", "Grass005"),
    Material("soil", "soil_garden", "Ground109"),
    Material("fabric", "fabric_plain", "Fabric081C"),
    Material("fabric", "fabric_weave", "Fabric061"),
    Material("fabric", "fabric_coarse", "Fabric066"),
    Material("metal", "metal_brushed", "Metal009"),
    Material("metal", "metal_chrome", "Metal049A"),
)

# Hashes of the exact upstream archives used on 2026-09-13.  ambientCG may regenerate package
# metadata without changing an asset id; pinning the bytes makes a future rerun fail instead of
# silently producing a different committed texture under an old provenance record.
ARCHIVE_SHA256 = {
    "PaintedPlaster017": "bae0380d82b6ccac8943b4980417cb3c9de5c896218339c108056f1dfb8e886c",
    "PaintedPlaster016": "7cb21ab2f0dbe7b599ba31c0140c0b85a100e30a0406e66502c18d067ec8f60e",
    "PaintedPlaster015": "792b97898cd4f583059de5eafd62293a11a44834ec9b771bf1a515d98c464d03",
    "PaintedPlaster010": "8755d4560396c038f0e924ad52463ca8830f3f7ac9f6aafd008ffc96df6df5f4",
    "PaintedPlaster014": "064347e8cb994ea8b4e1b504883f98917b00a8170d81d01ad39d6ad161cdcf02",
    "PaintedPlaster013": "6af62729cbea466d35bd2482d3b20c989518e32d7633928e301caf423b2795db",
    "WoodFloor051": "3f493484eab1ec5e1c466b90e515003b286fc6d7f84ff8ff6485900bfc26cef5",
    "WoodFloor064": "edd177798c9fa641531ef32970418ff9781e3bf3ceb7808257d2b6c09d7d3cc1",
    "WoodFloor070": "a8b11f1d6632faa674a5dcd903d7a932fd328e1239ef598f2b38014424547137",
    "WoodFloor043": "0bd1309b36c9a0add6fd9b4108f8cba73ba475afb6fb824c42bb05080c01403b",
    "WoodFloor040": "c83a01f1733f2a1f6e95ad6703689eb12fe259e77245d4c0a5652728d07c87cb",
    "Wood095": "04acff5a13e8ada6749b58ff5f6c49402692aadbfde87d65b06d35d70a9601d8",
    "Tiles141": "6ee56eba21592b9c02c6ee78dd93c6b585b62a2558d143d53e19457c82379d52",
    "Tiles139": "0ac4f6e094874948027a1477ad4b9e4986d48ccede608ae59874672ab46265db",
    "Tiles140": "6eb73dd78bcfff358f09720242fed543edd9fc6ea3a6c0d4f46ecef59fa08308",
    "Tiles143": "afbda7adaf4c0048a9224a09ab326fe0f8313e9add1343ad8c09c157a98d881f",
    "Carpet016": "b9e67b5d42cb47cead082dedae3767ae228175c857b6b3af6fe7bd4e69237b81",
    "Carpet012": "f5c459f278a3b50dc5c2bf535b8c3b6fd4bb6fc458c836b64023961b167fe4f6",
    "Carpet001": "f06d551e40d4d4d31a84df310bad2a1ec70753cd330342c7d8bcff3587fccbd1",
    "Marble012": "b04384c6d49ce73baeb70db7ea4abfe9948fc9b74b9fd92c3766970662bf2c43",
    "Onyx015": "6d46ffbd0800b3153588feeff81861561e2b2932ce1400102d5c58f54f7e1d89",
    "Rock064": "560f23fdd24f4abe27f5c272b86e69fd026c3d111e22f60d7fb647306830367b",
    "Bricks097": "97b5df360161e48bcfed609aef361e8115b680ddc0aea4ad9321d4b00e222ad0",
    "Plaster007": "75c7d96a8e60c4c5073767508e34e88c73677a3641e40bb8aaa0f1d6674391c0",
    "Concrete031": "3068e551435d3cdae3942017799d8871b940c88ebfaf5c2b87376972ae448fd6",
    "Asphalt033": "c71801b342dbea594dbdd0bd2ddc0a6d13f813c923fca408b9f5b9ee5e58aba2",
    "Gravel043": "bca54835c7c71bea4b4b9f27fbef127d666fa1cf7de16f2b04f0ceb4294e784c",
    "Grass005": "58a7fb7f32c44a86879483325b62781d04e0ab8f70b0f70ede36c8f1726f8a22",
    "Ground109": "be2cdb97845d895bddcf75f3d7a307301e1cb22dee2c41c47b9af0b64ee0db3d",
    "Fabric081C": "54cb93482845a9b74b34b1294ce40ff9427df88ec700c7fcfec72125551dca31",
    "Fabric061": "537a84d3a91d50bc907d2e86f4f024041643eb6a04c10b115cdf308f29508eb2",
    "Fabric066": "bcd274190979010b2056cab1b10d0420da6e2f58a8e1cf37bc527d8a67772684",
    "Metal009": "1b1970219b40fe707e7194b28702f1a65b0d227bbea56da69dfbc8d8b5cfcb5e",
    "Metal049A": "4f6a79e535261ab55cc6feefed0037124885720ce7b781fc2efa3fa22792af42",
}

EXPECTED_COUNTS = {
    "paint": 6, "wood": 6, "tile": 4, "carpet": 3, "stone": 3,
    "brick": 1, "plaster": 1, "concrete": 1, "asphalt": 1, "gravel": 1,
    "grass": 1, "soil": 1, "fabric": 3, "metal": 2,
}

# The directory and its content configuration are shared with project-generated material maps.
# Keep this list explicit: silently accepting every non-ambientCG PNG would weaken HOUSE-00296's
# exact-set gate, while claiming another generator's output would make the two ownership checks
# contradict each other.  Their dedicated material generators own these bytes and entries.
FOREIGN_TEXTURES = {
    "siding_clapboard_albedo.png",
    "siding_clapboard_normal.png",
    "snow_shell_albedo.png",
    "snow_shell_normal.png",
    "water_flow_albedo.png",
    "water_flow_normal.png",
}


def archive_name(material: Material) -> str:
    return f"{material.asset_id}_{SOURCE_FORMAT}.zip"


def member_bytes(archive: zipfile.ZipFile, asset_id: str, suffix: str,
                 *, required: bool = True) -> bytes | None:
    wanted = f"{asset_id}_{SOURCE_FORMAT}_{suffix}.jpg"
    matches = [name for name in archive.namelist() if Path(name).name == wanted]
    if len(matches) == 1:
        return archive.read(matches[0])
    if required:
        raise ValueError(f"{archive.filename}: expected exactly one {wanted}, found {len(matches)}")
    return None


def decode_resized(data: bytes, *, label: str) -> Image.Image:
    with Image.open(io.BytesIO(data)) as source:
        source.load()
        source = ImageOps.exif_transpose(source).convert("RGB")
        scale = SIZE / max(source.width, source.height)
        target = (round(source.width * scale), round(source.height * scale))
        if any(value <= 0 or value & (value - 1) for value in target):
            raise ValueError(
                f"{label}: {source.width}x{source.height} does not scale to power-of-two {target}")
        return source.resize(target, Image.Resampling.LANCZOS)


def solid_channel(value: int, size: tuple[int, int]) -> Image.Image:
    return Image.new("L", size, color=value)


def encode_rgba(image: Image.Image) -> bytes:
    rgba = image.convert("RGBA")
    return atlas_pack.png_encode(rgba.width, rgba.height, rgba.tobytes())


def prepare(material: Material, archive_path: Path, output: Path) -> dict:
    with zipfile.ZipFile(archive_path) as archive:
        albedo = decode_resized(member_bytes(archive, material.asset_id, "Color"),
                                label=f"{material.asset_id} Color")
        normal = decode_resized(member_bytes(archive, material.asset_id, "NormalGL"),
                                label=f"{material.asset_id} NormalGL")
        roughness = decode_resized(member_bytes(archive, material.asset_id, "Roughness"),
                                   label=f"{material.asset_id} Roughness").convert("L")
        ao_data = member_bytes(archive, material.asset_id, "AmbientOcclusion", required=False)
        metal_data = member_bytes(archive, material.asset_id, "Metalness", required=False)
        ao = (decode_resized(ao_data, label=f"{material.asset_id} AmbientOcclusion").convert("L")
              if ao_data is not None else solid_channel(255, roughness.size))
        metalness = (decode_resized(metal_data, label=f"{material.asset_id} Metalness").convert("L")
                     if metal_data is not None else solid_channel(0, roughness.size))
        sizes = {albedo.size, normal.size, roughness.size, ao.size, metalness.size}
        if len(sizes) != 1:
            raise ValueError(f"{material.asset_id}: maps do not share one size: {sorted(sizes)}")
        orm = Image.merge("RGB", (ao, roughness, metalness))

    output.mkdir(parents=True, exist_ok=True)
    paths = {}
    for channel, image in (("albedo", albedo), ("normal", normal), ("orm", orm)):
        path = output / f"{material.slug}_{channel}.png"
        path.write_bytes(encode_rgba(image))
        paths[channel] = path
    return {
        "assetId": material.asset_id,
        "category": material.category,
        "slug": material.slug,
        "archive": archive_name(material),
        "archiveSha256": hashlib.sha256(archive_path.read_bytes()).hexdigest(),
        "outputs": {channel: hashlib.sha256(path.read_bytes()).hexdigest()
                    for channel, path in paths.items()},
    }


def download(material: Material, cache: Path) -> Path:
    cache.mkdir(parents=True, exist_ok=True)
    path = cache / archive_name(material)
    expected = ARCHIVE_SHA256[material.asset_id]
    if path.is_file():
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise ValueError(f"{path}: sha256 {actual}, expected {expected}")
        return path
    temporary = path.with_suffix(".part")
    url = BASE_URL + archive_name(material)
    print(f"download {material.asset_id}: {url}", flush=True)
    request = urllib.request.Request(
        url,
        headers={
            "User-Agent": "curl/8.0 (cna-house HOUSE-00296 asset acquisition)",
            "Accept": "application/zip, application/octet-stream;q=0.9, */*;q=0.1",
        },
    )
    with urllib.request.urlopen(request, timeout=180) as response, temporary.open("wb") as target:
        shutil.copyfileobj(response, target, length=1 << 20)
    if not zipfile.is_zipfile(temporary):
        temporary.unlink(missing_ok=True)
        raise ValueError(f"{url}: response is not a ZIP archive")
    actual = hashlib.sha256(temporary.read_bytes()).hexdigest()
    if actual != expected:
        temporary.unlink(missing_ok=True)
        raise ValueError(f"{url}: sha256 {actual}, expected {expected}")
    temporary.replace(path)
    return path


def validate_selection() -> list[str]:
    problems = []
    counts = {category: 0 for category in EXPECTED_COUNTS}
    ids: set[str] = set()
    slugs: set[str] = set()
    for material in MATERIALS:
        if material.category not in counts:
            problems.append(f"{material.asset_id}: unexpected category {material.category}")
        else:
            counts[material.category] += 1
        if material.asset_id in ids:
            problems.append(f"duplicate ambientCG id {material.asset_id}")
        if material.slug in slugs:
            problems.append(f"duplicate output slug {material.slug}")
        ids.add(material.asset_id)
        slugs.add(material.slug)
    if counts != EXPECTED_COUNTS:
        problems.append(f"category counts are {counts}, expected {EXPECTED_COUNTS}")
    if len(MATERIALS) != 34:
        problems.append(f"selection has {len(MATERIALS)} materials, expected 34")
    if set(ARCHIVE_SHA256) != ids:
        problems.append(
            "archive hash ids differ from the selection: "
            f"missing={sorted(ids - set(ARCHIVE_SHA256))}, "
            f"extra={sorted(set(ARCHIVE_SHA256) - ids)}")
    return problems


def manifest_id(material: Material, channel: str) -> str:
    return f"TEXTURE_MATERIAL_{material.slug.upper()}_{channel.upper()}"


def register_outputs() -> int:
    problems = check_outputs()
    if problems:
        for problem in problems:
            print(f"ambientcg_materials: {problem}", file=sys.stderr)
        return 1

    import manifest as manifest_tool

    document = manifest_tool.load()
    owned_ids = {manifest_id(material, channel) for material in MATERIALS
                 for channel in ("albedo", "normal", "orm")}
    legacy_ids = {f"TEXTURE_MATERIAL_METAL_SCRATCHED_{channel.upper()}"
                  for channel in ("albedo", "normal", "orm")}
    document["assets"] = [row for row in document["assets"] if row.get("id") not in owned_ids]
    document["assets"] = [row for row in document["assets"] if row.get("id") not in legacy_ids]
    for material in MATERIALS:
        for channel in ("albedo", "normal", "orm"):
            path = OUTPUT / f"{material.slug}_{channel}.png"
            width, height, _ = atlas_pack.png_decode(path.read_bytes())
            channel_note = {
                "albedo": "sRGB base colour",
                "normal": "linear OpenGL/glTF (+Y) tangent-space normal",
                "orm": "linear packed R=ambient occlusion, G=roughness, B=metalness",
            }[channel]
            row = {
                "id": manifest_id(material, channel),
                "category": "material",
                "sourceFile": path.relative_to(REPO).as_posix(),
                "sourceSha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "texture": {
                    "width": width,
                    "height": height,
                    "colourSpace": "sRGB" if channel == "albedo" else "linear",
                    "channelLayout": channel_note,
                    "sourceArchiveSha256": ARCHIVE_SHA256[material.asset_id],
                },
                "origin": {
                    "kind": "downloaded",
                    "name": f"ambientCG {material.asset_id} {SOURCE_FORMAT}",
                    "licence": "CC0-1.0",
                    "licenceFile": "licenses/cc0-1.0/LICENCE.txt",
                    "attribution": "",
                    "redistributeSource": True,
                    "redistributeDerived": True,
                    "commercialUse": True,
                    "modification": True,
                    "url": f"https://ambientcg.com/a/{material.asset_id}",
                    "author": "ambientCG (Lennart Demes)",
                    "retrieved": "2026-09-13",
                    "note": (
                        f"{channel_note}, prepared by tools/assets/ambientcg_materials.py from "
                        f"{archive_name(material)} (sha256 "
                        f"{ARCHIVE_SHA256[material.asset_id]}). Resized with Lanczos to a maximum "
                        "256-pixel edge while preserving aspect ratio; licence verified in "
                        "docs/licence-evidence/ambientcg.md."
                    ),
                },
            }
            if channel == "orm":
                row["notPackaged"] = (
                    "Offline PBR authoring input. cna-house.md §22.1 has no ORM texture slot; "
                    "HOUSE-00900 mapped its roughness and metalness to the stock XNA effect's "
                    "specularColor/specularPower scalars through pbr_to_stock.py; this map is "
                    "retained as the reproducibility input."
                )
            else:
                row.update({
                    "contentName": path.relative_to(REPO / "assets-src").with_suffix("").as_posix(),
                    "kind": "texture",
                    "residencyPack": "core",
                })
            document["assets"].append(row)
    manifest_tool.save(document)

    config = json.loads(CONTENT_CONFIG.read_text(encoding="utf-8"))
    assets = config["assets"]
    for channel in ("albedo", "normal", "orm"):
        assets.pop(f"Materials/metal_scratched_{channel}.png", None)
    for material in MATERIALS:
        assets.pop(f"Materials/{material.slug}_orm.png", None)
    for material in MATERIALS:
        for channel in ("albedo", "normal"):
            assets[f"Materials/{material.slug}_{channel}.png"] = {
                "parameters": {
                    "generateMipmaps": {"type": "bool", "value": True},
                    "premultiplyAlpha": {"type": "bool", "value": False},
                }
            }
    config["assets"] = dict(sorted(assets.items()))
    CONTENT_CONFIG.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")

    source_lines = [
        "# ambientCG base materials",
        "",
        "`HOUSE-00296`, retrieved 2026-09-13 from the official ambientCG per-asset download "
        "endpoint. The source and its preview renders are CC0; the evidence and quoted grant are "
        "in `docs/licence-evidence/ambientcg.md`, with the full text in "
        "`licenses/cc0-1.0/LICENCE.txt`.",
        "",
        "The upstream ZIP files are hash-pinned below and cached only outside the repository. "
        "`tools/assets/ambientcg_materials.py` extracts Color, NormalGL, Roughness, optional "
        "AmbientOcclusion and optional Metalness; resizes them to a maximum 256-pixel edge; and "
        "writes albedo, normal and packed ORM PNGs. Albedo and normal compile as runtime textures; "
        "ORM remains a manifested offline input because HOUSE-00900 mapped it to §22.1's stock-"
        "effect specular scalars. Displacement and DirectX "
        "normals are rejected because the runtime material contract uses neither.",
        "",
        "| Class | ambientCG id | Local slug | Upstream archive SHA-256 | Manifest ids |",
        "|---|---|---|---|---|",
    ]
    for material in MATERIALS:
        ids = ", ".join(f"`{manifest_id(material, channel)}`"
                        for channel in ("albedo", "normal", "orm"))
        source_lines.append(
            f"| {material.category} | [{material.asset_id}]"
            f"(https://ambientcg.com/a/{material.asset_id}) | `{material.slug}` | "
            f"`{ARCHIVE_SHA256[material.asset_id]}` | {ids} |")
    source_lines += [
        "",
        "HOUSE-00900 mapped every set through `pbr_to_stock.py`, then rendered the authored stock "
        "parameters on a sphere, tiled floor and wall at low, medium and high light levels. The "
        "34 retained renders and visual findings are in `docs/asset-review/materials/`.",
    ]
    (OUTPUT / "SOURCE.md").write_text("\n".join(source_lines) + "\n", encoding="utf-8")
    print(f"ambientcg_materials: registered {len(owned_ids)} textures and wrote SOURCE.md")
    return 0


def check_outputs(root: Path = OUTPUT) -> list[str]:
    problems = validate_selection()
    expected = {f"{material.slug}_{channel}.png"
                for material in MATERIALS for channel in ("albedo", "normal", "orm")}
    actual = ({path.name for path in root.glob("*.png")} - FOREIGN_TEXTURES
              if root.is_dir() else set())
    for missing in sorted(expected - actual):
        problems.append(f"missing {root / missing}")
    for extra in sorted(actual - expected):
        problems.append(f"unexpected {root / extra}")
    sizes_by_slug: dict[str, set[tuple[int, int]]] = {}
    for name in sorted(expected & actual):
        path = root / name
        try:
            width, height, pixels = atlas_pack.png_decode(path.read_bytes())
        except (OSError, ValueError, atlas_pack.PngError) as error:
            problems.append(f"{path}: {error}")
            continue
        if max(width, height) != SIZE or any(value & (value - 1) for value in (width, height)):
            problems.append(f"{path}: {width}x{height}, expected power-of-two with max edge {SIZE}")
        if len(pixels) != width * height * 4:
            problems.append(f"{path}: decoded byte count is {len(pixels)}")
        if any(pixels[index] != 255 for index in range(3, len(pixels), 4)):
            problems.append(f"{path}: material inputs must be fully opaque")
        slug = name.rsplit("_", 1)[0]
        sizes_by_slug.setdefault(slug, set()).add((width, height))
    for slug, sizes in sorted(sizes_by_slug.items()):
        if len(sizes) != 1:
            problems.append(f"{slug}: channel dimensions differ: {sorted(sizes)}")
    return problems


def check_registration() -> list[str]:
    problems = []
    config = json.loads(CONTENT_CONFIG.read_text(encoding="utf-8"))
    foreign_config = {f"Materials/{name}" for name in FOREIGN_TEXTURES}
    configured = {name: entry for name, entry in config.get("assets", {}).items()
                  if name.startswith("Materials/") and name not in foreign_config}
    expected_config = {
        f"Materials/{material.slug}_{channel}.png"
        for material in MATERIALS for channel in ("albedo", "normal")
    }
    for name in sorted(expected_config - set(configured)):
        problems.append(f"{CONTENT_CONFIG}: missing runtime entry {name}")
    for name in sorted(set(configured) - expected_config):
        problems.append(f"{CONTENT_CONFIG}: unexpected material entry {name}")
    expected_parameters = {
        "generateMipmaps": {"type": "bool", "value": True},
        "premultiplyAlpha": {"type": "bool", "value": False},
    }
    for name in sorted(expected_config & set(configured)):
        if configured[name].get("parameters") != expected_parameters:
            problems.append(f"{CONTENT_CONFIG}: {name} does not pin mips=true, premultiply=false")

    import manifest as manifest_tool

    rows = {row.get("id"): row for row in manifest_tool.load().get("assets", [])}
    for material in MATERIALS:
        for channel in ("albedo", "normal", "orm"):
            row_id = manifest_id(material, channel)
            row = rows.get(row_id)
            if row is None:
                problems.append(f"manifest is missing {row_id}")
                continue
            path = OUTPUT / f"{material.slug}_{channel}.png"
            if row.get("sourceFile") != path.relative_to(REPO).as_posix():
                problems.append(f"{row_id}: sourceFile does not name {path}")
            if channel == "orm":
                if "notPackaged" not in row:
                    problems.append(f"{row_id}: ORM must be a non-packaged authoring input")
            elif (row.get("kind"), row.get("residencyPack")) != ("texture", "core"):
                problems.append(f"{row_id}: runtime map must be a core texture")
    return problems


def selftest() -> int:
    problems = validate_selection()
    with tempfile.TemporaryDirectory(prefix="house-00296-") as temporary:
        root = Path(temporary)
        material = Material("metal", "fixture", "Fixture001")
        archive_path = root / archive_name(material)
        with zipfile.ZipFile(archive_path, "w") as archive:
            colours = {
                "Color": (120, 80, 40), "NormalGL": (128, 128, 255),
                "Roughness": (64, 64, 64), "AmbientOcclusion": (192, 192, 192),
                "Metalness": (224, 224, 224),
            }
            for suffix, colour in colours.items():
                image = Image.new("RGB", (32, 32), colour)
                encoded = io.BytesIO()
                image.save(encoded, format="JPEG", quality=100, subsampling=0)
                archive.writestr(f"Fixture001_{SOURCE_FORMAT}_{suffix}.jpg", encoded.getvalue())
        result = prepare(material, archive_path, root / "out")
        if set(result["outputs"]) != {"albedo", "normal", "orm"}:
            problems.append("prepare did not report all three outputs")
        width, height, pixels = atlas_pack.png_decode((root / "out/fixture_orm.png").read_bytes())
        centre = pixels[((height // 2) * width + width // 2) * 4:][:4]
        if not (centre[2] > centre[0] > centre[1]):
            problems.append(f"ORM channel packing is wrong: centre pixel {tuple(centre)}")
        (root / "out/fixture_orm.png").unlink()
        if not any("missing" in problem for problem in check_outputs(root / "out")):
            # check_outputs uses the production selection, so it must certainly diagnose missing
            # files in this deliberately incomplete fixture directory.
            problems.append("check_outputs did not diagnose an incomplete directory")
    if problems:
        for problem in problems:
            print(f"FAIL: {problem}", file=sys.stderr)
        return 1
    print("ambientcg_materials selftest: 34 selections and conversion checks passed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--download", action="store_true",
                        help="download the fixed 1K archives and generate repository textures")
    parser.add_argument("--cache", type=Path, help="archive cache (required with --download)")
    parser.add_argument("--catalog", type=Path,
                        help="write conversion hashes as JSON (only with --download)")
    parser.add_argument("--check", action="store_true", help="validate the committed outputs")
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--register", action="store_true",
                        help="add the checked outputs to the manifest and content configuration")
    args = parser.parse_args()
    if sum((args.download, args.check, args.selftest, args.register)) != 1:
        parser.error("choose exactly one of --download, --check, --selftest or --register")
    if args.selftest:
        return selftest()
    if args.check:
        problems = check_outputs() + check_registration()
        if problems:
            for problem in problems:
                print(f"ambientcg_materials: {problem}", file=sys.stderr)
            return 1
        print(f"ambientcg_materials: clean -- {len(MATERIALS)} materials, "
              f"{len(MATERIALS) * 3} opaque power-of-two textures, max edge {SIZE}")
        return 0
    if args.register:
        return register_outputs()
    if args.cache is None:
        parser.error("--download requires --cache")
    catalog = [prepare(material, download(material, args.cache), OUTPUT)
               for material in MATERIALS]
    if args.catalog:
        args.catalog.write_text(json.dumps({"schema": "cna-house/ambientcg-materials/1",
                                           "sourceFormat": SOURCE_FORMAT,
                                           "outputSize": SIZE,
                                           "materials": catalog}, indent=2) + "\n",
                                encoding="utf-8")
    print(f"ambientcg_materials: wrote {len(catalog) * 3} textures for {len(catalog)} materials")
    return 0


if __name__ == "__main__":
    sys.exit(main())
