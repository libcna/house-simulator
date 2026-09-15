#!/usr/bin/env python3
"""Prepare two pinned CC0 Poly Haven period-furniture models for the playable L0 foyer.

The upstream glTF and three included 2K maps per model live only in a caller-owned cache. The
repository receives grounded GLBs with embedded source maps and a separate sRGB base-colour map
for the project's canonical static-material/chunk path. No source-host preview or legal mark is
packaged. `--check` proves the committed outputs still follow the pinned source bytes.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import struct
import subprocess
import sys
import tempfile
import urllib.request
from dataclasses import dataclass
from pathlib import Path

from PIL import Image

from gltf_io import read_model, write_glb
from gltf_sampler_compat import normalize_document

REPO = Path(__file__).resolve().parents[2]
API = "https://api.polyhaven.com/files/"
MODELS = REPO / "assets-src" / "Models" / "Furniture" / "Foyer"
TEXTURES = REPO / "assets-src" / "Textures" / "Furniture" / "Foyer"


@dataclass(frozen=True)
class Source:
    slug: str
    metadata_sha256: str
    model_name: str
    albedo_name: str


SOURCES = (
    Source("chinese_console_table",
           "d82960a421325ed32a273d4169a6f89e319821b171b55e9f4ae5b4eb5203f6cd",
           "console_table.glb", "console_table_albedo.png"),
    Source("ArmChair_01",
           "cb73da530292cad49b3d68521284f365ea80cf35df35e9afdf2f30c8d2d2b37d",
           "upholstered_armchair.glb", "upholstered_armchair_albedo.png"),
)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fetch(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "cna-house/HOUSE-01038"})
    with urllib.request.urlopen(request, timeout=30) as response:
        return response.read()


def cached_file(path: Path, entry: dict, download: bool) -> bytes:
    if path.is_file():
        data = path.read_bytes()
    elif download:
        data = fetch(entry["url"])
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    else:
        raise ValueError(f"{path}: missing pinned source; run --download with this cache")
    if len(data) != entry["size"] or ("md5" in entry and
                                      hashlib.md5(data).hexdigest() != entry["md5"]):
        raise ValueError(f"{path}: upstream size or MD5 differs from pinned metadata")
    return data


def pinned_source(source: Source, cache: Path, download: bool) -> tuple[dict, Path, dict]:
    root = cache / source.slug
    metadata_entry = {"url": API + source.slug}
    metadata_path = root / "files.json"
    if metadata_path.is_file():
        raw = metadata_path.read_bytes()
    elif download:
        raw = fetch(metadata_entry["url"])
        metadata_path.parent.mkdir(parents=True, exist_ok=True)
        metadata_path.write_bytes(raw)
    else:
        raise ValueError(f"{metadata_path}: missing pinned metadata; run --download")
    if digest(raw) != source.metadata_sha256:
        raise ValueError(f"{source.slug}: Poly Haven files metadata has changed")
    entry = json.loads(raw)["gltf"]["2k"]["gltf"]
    gltf_path = root / entry["url"].rsplit("/", 1)[1]
    cached_file(gltf_path, entry, download)
    for name, included in entry["include"].items():
        relative = Path(name)
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"{source.slug}: unsafe included path {name}")
        cached_file(root / relative, included, download)
    return json.loads(gltf_path.read_text()), gltf_path, entry


def recenter_armchair(document: dict, geometry: bytearray) -> None:
    """Move the pinned chair's 3.4 cm off-axis POSITION data, not just its scene node.

    `origin_check.py` correctly tests authored accessor bounds, because a translated scene node
    would still leave the source model rotating about an off-centre support point. The pinned
    ArmChair_01 has one visible FLOAT/VEC3 position stream and no sparse accessor.
    """
    position_indices = {primitive["attributes"]["POSITION"]
                        for mesh in document["meshes"]
                        for primitive in mesh["primitives"]}
    if len(position_indices) != 1:
        raise ValueError("ArmChair_01: expected exactly one visible POSITION accessor")
    accessor = document["accessors"][position_indices.pop()]
    if (accessor.get("componentType") != 5126 or accessor.get("type") != "VEC3" or
            "sparse" in accessor or "min" not in accessor or "max" not in accessor):
        raise ValueError("ArmChair_01: unsupported POSITION layout for origin correction")
    centre_z = (accessor["min"][2] + accessor["max"][2]) / 2.0
    if not 0.02 < abs(centre_z) < 0.05:
        raise ValueError(f"ArmChair_01: unexpected Z-origin offset {centre_z:+.4f} m")
    view = document["bufferViews"][accessor["bufferView"]]
    if view.get("buffer", 0) != 0:
        raise ValueError("ArmChair_01: POSITION is not in the pinned geometry buffer")
    stride = view.get("byteStride", 12)
    if stride < 12:
        raise ValueError("ArmChair_01: POSITION stride is shorter than VEC3")
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    for index in range(accessor["count"]):
        offset = start + index * stride + 8
        z = struct.unpack_from("<f", geometry, offset)[0]
        struct.pack_into("<f", geometry, offset, z - centre_z)
    accessor["min"][2] -= centre_z
    accessor["max"][2] -= centre_z


def prepared_bytes(document: dict, gltf_path: Path) -> tuple[bytes, bytes]:
    if len(document.get("buffers", [])) != 1:
        raise ValueError(f"{gltf_path}: expected one source geometry buffer")
    source_buffer = document["buffers"][0]
    relative_buffer = Path(source_buffer["uri"])
    if relative_buffer.is_absolute() or ".." in relative_buffer.parts:
        raise ValueError(f"{gltf_path}: unsafe source buffer path {relative_buffer}")
    geometry = (gltf_path.parent / relative_buffer).read_bytes()
    if len(geometry) != source_buffer["byteLength"]:
        raise ValueError(f"{gltf_path}: declared geometry length does not match source")
    blob = bytearray(geometry)
    if gltf_path.parent.name == "ArmChair_01":
        recenter_armchair(document, blob)
    images = document.get("images", [])
    if len(images) != 3:
        raise ValueError(f"{gltf_path}: expected exactly normal, base-colour and ARM maps")
    albedo: bytes | None = None
    for image in images:
        relative = Path(image["uri"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"{gltf_path}: unsafe image path {relative}")
        payload = (gltf_path.parent / relative).read_bytes()
        if image["name"].lower().endswith("_diff"):
            albedo = payload
        blob.extend(b"\0" * ((-len(blob)) % 4))
        offset = len(blob)
        blob.extend(payload)
        image["bufferView"] = len(document["bufferViews"])
        image.pop("uri")
        document["bufferViews"].append({"buffer": 0, "byteOffset": offset,
                                        "byteLength": len(payload)})
    if albedo is None:
        raise ValueError(f"{gltf_path}: no base-colour map in pinned source")
    document["buffers"] = [{"byteLength": len(blob)}]
    changed, removed = normalize_document(document)
    if changed != 1 or removed != 0:
        raise ValueError(f"{gltf_path}: unexpected sampler/extension compatibility changes")
    with Image.open(io.BytesIO(albedo)) as source_image:
        if source_image.size != (2048, 2048):
            raise ValueError(f"{gltf_path}: expected a 2K close-range albedo")
        png = io.BytesIO()
        source_image.convert("RGBA").save(png, format="PNG", optimize=True)
    with tempfile.TemporaryDirectory(prefix="house01038-glb-") as scratch:
        packed = Path(scratch) / "packed.glb"
        collidable = Path(scratch) / "collidable.glb"
        write_glb(packed, document, bytes(blob))
        result = subprocess.run((sys.executable, str(REPO / "tools" / "blender" /
                                                   "collision_proxy.py"), str(packed),
                                 str(collidable)), cwd=REPO, capture_output=True, text=True,
                                check=True)
        report_lines = [line.removeprefix("COLLISION_JSON ") for line in result.stdout.splitlines()
                        if line.startswith("COLLISION_JSON ")]
        if len(report_lines) != 1:
            raise ValueError(f"{gltf_path}: collision proxy produced no single JSON report")
        report = json.loads(report_lines[0])
        if not report["ok"] or not report["encloses"] or report["triangles"] > 64:
            raise ValueError(f"{gltf_path}: unacceptable collision proxy: {report}")
        collidable_document, collidable_blob = read_model(collidable)
        if len([node for node in collidable_document.get("nodes", [])
                if node.get("name", "").endswith("_COL")]) != 1:
            raise ValueError(f"{gltf_path}: expected one invisible _COL node")
        changed, removed = normalize_document(collidable_document)
        if changed != 1 or removed != 0:
            raise ValueError(f"{gltf_path}: collision exporter changed sampler compatibility")
        glb = write_glb(Path(scratch) / "prepared.glb", collidable_document, collidable_blob)
    return glb, png.getvalue()


def prepare(cache: Path, download: bool, check: bool) -> int:
    for source in SOURCES:
        document, gltf_path, _ = pinned_source(source, cache, download)
        if download:
            print(f"foyer_furniture_prepare: cached pinned {source.slug} source at {gltf_path}")
            continue
        glb, png = prepared_bytes(document, gltf_path)
        outputs = ((MODELS / source.model_name, glb),
                   (TEXTURES / source.albedo_name, png))
        for path, data in outputs:
            if check:
                if not path.is_file() or path.read_bytes() != data:
                    raise ValueError(f"{path}: differs from the pinned prepared source")
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            print(f"foyer_furniture_prepare: {'checked' if check else 'prepared'} {path.relative_to(REPO)} "
                  f"({len(data)} bytes, sha256:{digest(data)})")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--download", action="store_true",
                        help="fetch only the two pinned upstream 2K sources into --cache")
    parser.add_argument("--check", action="store_true",
                        help="compare committed GLBs and albedos with the cached pinned sources")
    args = parser.parse_args()
    if args.download and args.check:
        parser.error("--download and --check are mutually exclusive")
    try:
        return prepare(args.cache, args.download, args.check)
    except (OSError, KeyError, ValueError, subprocess.CalledProcessError,
            urllib.error.URLError) as error:
        print(f"foyer_furniture_prepare: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
