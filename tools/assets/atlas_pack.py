#!/usr/bin/env python3
"""atlas_pack.py -- pack small-prop textures into shared atlases and rewrite the models' UVs.

`HOUSE-00192`. `cna-house.md` §22.4 wants `MAT_ATLAS_KITCHEN_SMALL` and its siblings: one 2048²
material shared by sixty-odd small props, so the renderer changes texture once instead of once per
prop. Packing the images is the easy half. The half that goes wrong silently is the UVs, and this
tool exists because the failure modes are not obvious.

## What the tool actually does

Given a set of `.glb`/`.gltf` props it

1. collects each **material's whole texture set** -- base colour, metallic-roughness, normal,
   occlusion, emissive -- and treats that set as one packing unit;
2. lays those units out once, deterministically, into a square power-of-two atlas;
3. writes **one atlas image per channel, all sharing that single layout**;
4. rewrites every affected `TEXCOORD_0` so a prop's UVs land inside its own region;
5. writes a metadata sidecar naming every region, every source hash and every output hash.

## Why the packing unit is the material, not the image

A material's normal map must land at the same place in the normal atlas as its base colour lands in
the base-colour atlas, because **one set of UVs addresses both**. Packing each image independently
by size -- the obvious implementation -- puts a 64² normal somewhere different from its 64² albedo
and the prop samples another prop's normals. So the layout is computed once over material texture
sets, and every channel atlas is stamped from that same layout.

The consequence: a material's maps must all have the **same dimensions**. A 64² albedo with a 32²
normal has no single region size, and rather than silently rescaling somebody's normal map the tool
refuses and names the material.

## Why UVs outside [0, 1] are refused rather than handled

A prop whose UVs run 0..3 is asking the sampler to repeat its texture three times. Inside an atlas
`REPEAT` does not repeat the region, it repeats the **whole atlas** -- the prop tiles its
neighbours' texels across itself. There is no UV transform that fixes this; the only real fix is to
bake the repeat into a bigger texture offline, which is a different task. So the tool measures the
UV range of every primitive it is asked to atlas and refuses the ones that tile.

The atlased samplers are written as `CLAMP_TO_EDGE` for the same reason: after packing, a UV that
strays outside its region must clamp to the atlas edge, not wrap to the far side of it.

## The gutter, and the mip level it actually buys

Each region is surrounded by a gutter (default 4 texels) filled by **replicating the region's edge
texels outward**, not with transparent black. Black would darken the border under bilinear
filtering, which is the classic atlas seam. Region boxes are additionally rounded up to a multiple
of `--align` (default 4), so region origins and sizes are aligned too.

That buys a specific, finite number of mip levels and the tool **computes** which rather than
asserting one. The condition it checks is that no reduced texel averages two different props'
content: each region's rectangle is snapped outward to the `2**L` grid and must still touch no
other region. `mipSafeLevels` in the metadata is the largest level that survives. Above it a mip
texel genuinely straddles two props and no gutter can help -- the honest answer there is one
texture per prop, and this tool does not pretend otherwise.

(The first version of this used the obvious rule -- `2**L` divides every origin and size -- and it
under-reported by two levels, because averaging a region's edge with its own replicated gutter is
harmless. The measurement is in `plan.md` under `HOUSE-00192`.)

## Determinism

Sources are sorted by content, never by directory order; the shelf packer is a pure function of the
sorted box list; JSON is emitted with sorted keys and fixed separators; the PNG encoder is this
file's own, so the output does not depend on which Pillow is installed. **The one thing that is not
guaranteed across toolchains is the DEFLATE stream**: `zlib.compress(level=9)` is deterministic for
a given zlib but not across zlib versions. `--verify` therefore checks decoded pixels, and the
selftest asserts byte-identity on this machine while saying what that does and does not cover.

    tools/assets/atlas_pack.py --name MAT_ATLAS_KITCHEN_SMALL --out content-src/atlas props/*.glb
    tools/assets/atlas_pack.py --verify content-src/atlas
    tools/assets/atlas_pack.py --make-fixture <dir>
    tools/assets/atlas_pack.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import io
import json
import shutil
import struct
import sys
import tempfile
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

GLB_MAGIC = 0x46546C67  # 'glTF'
CHUNK_JSON = 0x4E4F534A  # 'JSON'
CHUNK_BIN = 0x004E4942  # 'BIN\0'

CLAMP_TO_EDGE = 33071

#: glTF component types this tool can read out of an accessor, and their struct code and size.
COMPONENTS = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2), 5123: ("H", 2),
              5125: ("I", 4), 5126: ("f", 4)}
#: Divisor that turns a normalized integer component back into its float value (glTF 3.11).
NORMALIZE = {5120: 127.0, 5121: 255.0, 5122: 32767.0, 5123: 65535.0}
TYPE_COUNTS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4,
               "MAT2": 4, "MAT3": 9, "MAT4": 16}

#: The five texture slots a glTF 2.0 core material can carry, in a fixed order so that atlas file
#: names and metadata keys never depend on dictionary insertion order.
CHANNELS = ("baseColor", "metallicRoughness", "normal", "occlusion", "emissive")

#: File-name suffix per channel. Short because these end up in content names.
CHANNEL_SUFFIX = {"baseColor": "BC", "metallicRoughness": "MR", "normal": "NRM",
                  "occlusion": "AO", "emissive": "EM"}

#: What a region is filled with when its material does not use that channel. NOT black: a black
#: normal is not "no normal", it is a normal pointing into the surface, and a black occlusion map
#: is full occlusion. Each of these is the value that makes the channel a no-op, so a material
#: without a normal map behaves in the atlas exactly as it did outside it.
NEUTRAL = {"baseColor": (255, 255, 255, 255),
           "metallicRoughness": (255, 255, 255, 255),
           "normal": (128, 128, 255, 255),
           "occlusion": (255, 255, 255, 255),
           "emissive": (0, 0, 0, 255)}

#: Extensions that change how the bytes this tool rewrites must be read or written. Meeting one
#: means the safe answer is to stop, not to guess.
REFUSED_EXTENSIONS = {
    "KHR_texture_transform": "applies a second UV transform this tool would have to compose with",
    "KHR_draco_mesh_compression": "hides the UVs inside a compressed buffer",
    "EXT_meshopt_compression": "hides the UVs inside a compressed buffer",
    "KHR_texture_basisu": "carries the image as a supercompressed format this tool cannot blit",
}


class PackError(Exception):
    """A refusal with a reason a human can act on."""


# --------------------------------------------------------------------------------------- PNG ----

def png_encode(width: int, height: int, pixels: bytes) -> bytes:
    """RGBA8 to PNG, with this file's own encoder.

    Written here rather than handed to Pillow so the output bytes do not depend on which Pillow is
    installed -- an atlas is content, and content whose hash moves when a build machine upgrades is
    content nobody can verify. Row filters are chosen by the standard minimum-sum-of-absolute-
    differences heuristic, which is a pure function of the pixels.
    """
    import numpy as np

    stride = width * 4
    rows = np.frombuffer(pixels, dtype=np.uint8).reshape(height, stride).astype(np.int16)
    out = bytearray()
    previous = np.zeros(stride, dtype=np.int16)
    zero = np.zeros(4, dtype=np.int16)

    for y in range(height):
        row = rows[y]
        left = np.concatenate((zero, row[:-4]))
        up = previous
        up_left = np.concatenate((zero, previous[:-4]))
        paeth_base = left + up - up_left
        pa = np.abs(paeth_base - left)
        pb = np.abs(paeth_base - up)
        pc = np.abs(paeth_base - up_left)
        paeth = np.where((pa <= pb) & (pa <= pc), left, np.where(pb <= pc, up, up_left))

        candidates = ((0, row), (1, row - left), (2, row - up),
                      (3, row - ((left + up) >> 1)), (4, row - paeth))
        best_type, best = 0, None
        best_cost = None
        for filter_type, value in candidates:
            byte = (value & 0xFF).astype(np.int32)
            # The heuristic sums the magnitude of each byte read as a SIGNED value, which is what
            # makes -1 (0xFF) cost 1 rather than 255.
            cost = int(np.minimum(byte, 256 - byte).sum())
            if best_cost is None or cost < best_cost:
                best_cost, best_type, best = cost, filter_type, byte
        out.append(best_type)
        out += bytes(best.astype(np.uint8))
        previous = row

    def chunk(tag: bytes, data: bytes) -> bytes:
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(out), 9))
            + chunk(b"IEND", b""))


def png_decode(data: bytes) -> tuple[int, int, bytes]:
    """Any PNG (or anything else Pillow reads) to RGBA8. Reading is where format variety lives."""
    try:
        from PIL import Image
    except ImportError as error:  # pragma: no cover - environment problem, not a data problem
        raise PackError(f"Pillow is required to read source textures: {error}") from error
    with Image.open(io.BytesIO(data)) as image:
        rgba = image.convert("RGBA")
        return rgba.width, rgba.height, rgba.tobytes()


# -------------------------------------------------------------------------------------- glTF ----

def read_model(path: Path) -> tuple[dict, bytes]:
    """The JSON and the binary chunk of a `.glb`, or the JSON and external buffer of a `.gltf`."""
    data = path.read_bytes()
    if path.suffix.lower() != ".glb":
        document = json.loads(data.decode("utf-8"))
        return document, b""

    if len(data) < 20:
        raise PackError(f"{path}: shorter than a glTF binary header")
    magic, version, total = struct.unpack_from("<III", data, 0)
    if magic != GLB_MAGIC:
        raise PackError(f"{path}: magic is {magic:#010x}, not 'glTF'")
    if version != 2:
        raise PackError(f"{path}: declares glTF version {version}; this project uses glTF 2.0")
    if total != len(data):
        raise PackError(f"{path}: declares {total} bytes but the file is {len(data)}")

    document: dict | None = None
    blob = b""
    offset = 12
    while offset + 8 <= len(data):
        length, kind = struct.unpack_from("<II", data, offset)
        offset += 8
        if offset + length > len(data):
            raise PackError(f"{path}: a chunk runs past the end of the file")
        payload = data[offset:offset + length]
        if kind == CHUNK_JSON:
            document = json.loads(payload.decode("utf-8"))
        elif kind == CHUNK_BIN:
            blob = payload
        offset += length
    if document is None:
        raise PackError(f"{path}: has no JSON chunk")
    return document, blob


def write_glb(path: Path, document: dict, blob: bytes) -> bytes:
    """Serialise to `.glb`. Sorted JSON keys so the same document always produces the same bytes."""
    text = json.dumps(document, separators=(",", ":"), sort_keys=True).encode("utf-8")
    text += b" " * ((4 - len(text) % 4) % 4)
    padded = blob + b"\0" * ((4 - len(blob) % 4) % 4)
    out = (struct.pack("<III", GLB_MAGIC, 2, 12 + 8 + len(text) + (8 + len(padded) if padded else 0))
           + struct.pack("<II", len(text), CHUNK_JSON) + text)
    if padded:
        out += struct.pack("<II", len(padded), CHUNK_BIN) + padded
    path.write_bytes(out)
    return out


def buffer_bytes(document: dict, blob: bytes, base: Path) -> list[bytes]:
    """The bytes of every declared buffer: the GLB chunk, a `data:` URI, or a sibling file."""
    buffers = []
    for index, buffer in enumerate(document.get("buffers", [])):
        uri = buffer.get("uri")
        if uri is None:
            buffers.append(blob)
        elif uri.startswith("data:"):
            buffers.append(base64.b64decode(uri.split(",", 1)[1]))
        else:
            buffers.append((base / uri).read_bytes())
        if len(buffers[index]) < int(buffer.get("byteLength", 0)):
            raise PackError(f"buffer {index} is shorter than its declared byteLength")
    return buffers


def read_accessor(document: dict, buffers: list[bytes], index: int) -> list[tuple[float, ...]]:
    """One accessor as a list of tuples, honouring byteStride and the normalized flag."""
    accessor = document["accessors"][index]
    if "sparse" in accessor:
        raise PackError(f"accessor {index} is sparse; this tool does not rewrite sparse accessors")
    code, size = COMPONENTS[accessor["componentType"]]
    count = TYPE_COUNTS[accessor["type"]]
    element = size * count

    view_index = accessor.get("bufferView")
    if view_index is None:
        return [tuple([0.0] * count) for _ in range(accessor["count"])]
    view = document["bufferViews"][view_index]
    data = buffers[view.get("buffer", 0)]
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    stride = view.get("byteStride") or element

    divisor = NORMALIZE.get(accessor["componentType"]) if accessor.get("normalized") else None
    values = []
    for i in range(accessor["count"]):
        raw = struct.unpack_from("<" + code * count, data, start + i * stride)
        if divisor is not None:
            raw = tuple(max(v / divisor, -1.0) for v in raw)
        values.append(tuple(float(v) for v in raw))
    return values


def image_bytes(document: dict, buffers: list[bytes], base: Path, index: int) -> bytes:
    image = document["images"][index]
    uri = image.get("uri")
    if uri is not None:
        if uri.startswith("data:"):
            return base64.b64decode(uri.split(",", 1)[1])
        return (base / uri).read_bytes()
    view = document["bufferViews"][image["bufferView"]]
    start = view.get("byteOffset", 0)
    return buffers[view.get("buffer", 0)][start:start + view["byteLength"]]


def material_slots(material: dict) -> dict[str, dict]:
    """The texture references a material carries, keyed by this tool's channel names."""
    slots: dict[str, dict] = {}
    pbr = material.get("pbrMetallicRoughness", {})
    if "baseColorTexture" in pbr:
        slots["baseColor"] = pbr["baseColorTexture"]
    if "metallicRoughnessTexture" in pbr:
        slots["metallicRoughness"] = pbr["metallicRoughnessTexture"]
    for channel, key in (("normal", "normalTexture"), ("occlusion", "occlusionTexture"),
                         ("emissive", "emissiveTexture")):
        if key in material:
            slots[channel] = material[key]
    return slots


# ------------------------------------------------------------------------------------ packing ----

def align_up(value: int, alignment: int) -> int:
    return ((value + alignment - 1) // alignment) * alignment


def shelf_pack(boxes: list[tuple[str, int, int]], side: int) -> dict[str, tuple[int, int]] | None:
    """Next-fit-decreasing-height shelf packing into a `side`x`side` square.

    Shelves rather than MaxRects because the input is a few dozen boxes whose sizes are nearly all
    powers of two, where the two algorithms land within a few percent of each other, and because a
    shelf packer is short enough to read and obviously a pure function of its input -- which is the
    property the determinism requirement actually needs.
    """
    placed: dict[str, tuple[int, int]] = {}
    x = y = shelf_height = 0
    for key, width, height in boxes:
        if width > side or height > side:
            return None
        if x + width > side:
            x = 0
            y += shelf_height
            shelf_height = 0
        if y + height > side:
            return None
        placed[key] = (x, y)
        x += width
        shelf_height = max(shelf_height, height)
    return placed


def mip_safe_levels(regions: list[dict], side: int) -> int:
    """How many mip levels the layout survives, computed rather than asserted.

    The condition that matters is NOT that `2**L` divides the region origins. Averaging a region's
    edge texels with its own gutter is harmless -- the gutter is a copy of those very texels, which
    is the whole reason it is edge-replicated. What is fatal is a mip texel that averages **two
    different props' content**, because then one prop's colour appears on the other at distance and
    no amount of gutter removes it.

    So the test is exactly that: at level `L`, snap each region's content rectangle outward to the
    `2**L` grid -- that is the set of full-resolution texels the reduced texels are built from --
    and check the result touches no other region's content. Divisibility was the first rule tried
    here and it under-reports badly: a 30-texel-wide source capped a 4-texel-gutter atlas at level
    1 when nothing was actually bleeding until level 3.
    """
    level = 0
    while (1 << (level + 1)) <= side:
        step = 1 << (level + 1)
        clean = True
        for index, region in enumerate(regions):
            x0 = (region["x"] // step) * step
            y0 = (region["y"] // step) * step
            x1 = -(-(region["x"] + region["w"]) // step) * step
            y1 = -(-(region["y"] + region["h"]) // step) * step
            for other_index, other in enumerate(regions):
                if other_index == index:
                    continue
                if (x0 < other["x"] + other["w"] and other["x"] < x1
                        and y0 < other["y"] + other["h"] and other["y"] < y1):
                    clean = False
                    break
            if not clean:
                break
        if not clean:
            break
        level += 1
    return level


# ----------------------------------------------------------------------------------- the pack ----

def collect(paths: list[Path]) -> tuple[list[dict], dict[tuple, dict]]:
    """Read every model and group its materials into unique texture sets.

    Returns the per-model records and a map from set identity (the tuple of channel content hashes)
    to the set. Identity is content, so two props exported with the same texture share one region
    rather than paying for it twice.
    """
    models: list[dict] = []
    sets: dict[tuple, dict] = {}

    for path in sorted(paths, key=lambda p: str(p)):
        document, blob = read_model(path)
        for extension in document.get("extensionsUsed", []):
            if extension in REFUSED_EXTENSIONS:
                raise PackError(f"{path.name}: uses {extension}, which "
                                f"{REFUSED_EXTENSIONS[extension]}")
        buffers = buffer_bytes(document, blob, path.parent)
        textures = document.get("textures", [])

        assignments: dict[int, tuple | None] = {}
        for index, material in enumerate(document.get("materials", [])):
            name = material.get("name", f"material{index}")
            slots = material_slots(material)
            if not slots:
                assignments[index] = None
                continue

            images: dict[str, tuple[int, int, bytes]] = {}
            digests: dict[str, str] = {}
            for channel in CHANNELS:
                if channel not in slots:
                    continue
                reference = slots[channel]
                if reference.get("texCoord", 0) != 0:
                    raise PackError(f"{path.name}: material '{name}' samples {channel} from "
                                    f"TEXCOORD_{reference['texCoord']}; this tool rewrites "
                                    f"TEXCOORD_0 only")
                source = textures[reference["index"]].get("source")
                if source is None:
                    raise PackError(f"{path.name}: material '{name}' has a {channel} texture with "
                                    f"no image source")
                raw = image_bytes(document, buffers, path.parent, source)
                width, height, pixels = png_decode(raw)
                images[channel] = (width, height, pixels)
                digests[channel] = hashlib.sha256(pixels).hexdigest()

            sizes = {(w, h) for w, h, _ in images.values()}
            if len(sizes) != 1:
                detail = ", ".join(f"{c}={images[c][0]}x{images[c][1]}" for c in CHANNELS
                                   if c in images)
                raise PackError(f"{path.name}: material '{name}' mixes texture sizes ({detail}). "
                                f"All maps of one material share one set of UVs and therefore one "
                                f"region; resize them to match before atlasing.")

            identity = tuple(sorted(digests.items()))
            if identity not in sets:
                width, height = next(iter(sizes))
                sets[identity] = {"identity": identity, "w": width, "h": height,
                                  "images": images, "digests": digests, "materials": []}
            sets[identity]["materials"].append(f"{path.name}:{name}")
            assignments[index] = identity

        models.append({"path": path, "document": document, "blob": blob, "buffers": buffers,
                       "assignments": assignments,
                       "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    return models, sets


def lay_out(sets: dict[tuple, dict], gutter: int, alignment: int,
            max_size: int) -> tuple[int, list[dict]]:
    """One layout, shared by every channel atlas. Sorted by content, so the result is a function of
    the textures and not of the order the models were listed on the command line."""
    boxes = []
    for identity, entry in sets.items():
        padded_w = align_up(entry["w"] + 2 * gutter, alignment)
        padded_h = align_up(entry["h"] + 2 * gutter, alignment)
        boxes.append((identity, padded_w, padded_h))
    # Tallest first is what makes shelf packing competitive; the digest tuple breaks ties so equal
    # boxes have one fixed order.
    boxes.sort(key=lambda box: (-box[2], -box[1], box[0]))
    keyed = [(json.dumps(identity), w, h) for identity, w, h in boxes]

    side = 16
    placement = None
    while side <= max_size:
        placement = shelf_pack(keyed, side)
        if placement is not None:
            break
        side *= 2
    if placement is None:
        raise PackError(f"the texture sets do not fit a {max_size}x{max_size} atlas; split the "
                        f"prop set or raise --max-size")

    regions = []
    for identity, padded_w, padded_h in boxes:
        x, y = placement[json.dumps(identity)]
        entry = sets[identity]
        regions.append({"x": x + gutter, "y": y + gutter, "w": entry["w"], "h": entry["h"],
                        "boxX": x, "boxY": y, "boxW": padded_w, "boxH": padded_h,
                        "identity": identity})
    regions.sort(key=lambda region: (region["y"], region["x"]))

    # The packer's contract, enforced rather than trusted. A packer bug that overlaps two boxes
    # produces an atlas in which one prop wears another's texture -- a wrong picture, not a crash,
    # and therefore the kind of bug that ships. O(n^2) over a few dozen boxes costs nothing.
    for index, region in enumerate(regions):
        if (region["boxX"] + region["boxW"] > side or region["boxY"] + region["boxH"] > side
                or region["boxX"] < 0 or region["boxY"] < 0):
            raise PackError(f"internal: a packed box runs off the {side}x{side} atlas")
        for other in regions[index + 1:]:
            if (region["boxX"] < other["boxX"] + other["boxW"]
                    and other["boxX"] < region["boxX"] + region["boxW"]
                    and region["boxY"] < other["boxY"] + other["boxH"]
                    and other["boxY"] < region["boxY"] + region["boxH"]):
                raise PackError("internal: the packer produced two overlapping boxes")
    return side, regions


def stamp(side: int, channel: str, regions: list[dict], sets: dict[tuple, dict]) -> bytes:
    """One channel atlas: neutral background, then every region blitted with its gutter."""
    import numpy as np

    atlas = np.empty((side, side, 4), dtype=np.uint8)
    atlas[:, :] = np.array(NEUTRAL[channel], dtype=np.uint8)

    for region in regions:
        entry = sets[region["identity"]]
        source = entry["images"].get(channel)
        if source is None:
            continue  # The neutral fill is the correct content for a channel this material lacks.
        width, height, texels = source
        pixels = np.frombuffer(texels, dtype=np.uint8).reshape(height, width, 4)
        # The whole padded box is written, not just the region: the gutter (and the slop that
        # alignment added) is the region's edge texel replicated outward, so bilinear filtering at
        # the boundary reads the prop's own colour instead of a dark seam or a neighbour. Clamping
        # the source index is exactly that replication, and doing it as an index array rather than
        # a per-texel loop is what keeps a full 2048 atlas under a second instead of over ten.
        rows = np.clip(np.arange(region["boxY"], region["boxY"] + region["boxH"]) - region["y"],
                       0, height - 1)
        columns = np.clip(np.arange(region["boxX"], region["boxX"] + region["boxW"]) - region["x"],
                          0, width - 1)
        atlas[region["boxY"]:region["boxY"] + region["boxH"],
              region["boxX"]:region["boxX"] + region["boxW"]] = pixels[np.ix_(rows, columns)]
    return atlas.tobytes()


def uv_transform(region: dict, side: int) -> tuple[float, float, float, float]:
    """(scaleU, scaleV, offsetU, offsetV) taking a source UV into the atlas."""
    return (region["w"] / side, region["h"] / side, region["x"] / side, region["y"] / side)


def rewrite_model(model: dict, regions_by_identity: dict[tuple, dict], side: int,
                  channels: list[str], atlas_names: dict[str, str]) -> tuple[dict, bytes, list]:
    """Rewrite one model's UVs, materials, images and buffer. Returns (document, blob, report)."""
    document = json.loads(json.dumps(model["document"]))  # deep copy; the original stays readable
    buffers = model["buffers"]
    assignments = model["assignments"]
    accessors = document.get("accessors", [])
    report = []

    # A TEXCOORD_0 accessor shared by two primitives with DIFFERENT materials cannot be rewritten
    # in place -- the two need different transforms. Each (accessor, transform) pair therefore gets
    # its own accessor, and the accessor is only reused when the transform is genuinely the same.
    new_uv: dict[tuple[int, tuple], int] = {}
    extra_views: list[tuple[bytes, dict]] = []

    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            material_index = primitive.get("material")
            if material_index is None:
                continue
            identity = assignments.get(material_index)
            if identity is None:
                continue
            attributes = primitive.get("attributes", {})
            if "TEXCOORD_0" not in attributes:
                raise PackError(f"{model['path'].name}: a primitive using an atlased material has "
                                f"no TEXCOORD_0")
            old = attributes["TEXCOORD_0"]
            region = regions_by_identity[identity]
            transform = uv_transform(region, side)

            cache_key = (old, transform)
            if cache_key not in new_uv:
                values = read_accessor(document, buffers, old)
                lo_u = min(v[0] for v in values)
                hi_u = max(v[0] for v in values)
                lo_v = min(v[1] for v in values)
                hi_v = max(v[1] for v in values)
                if lo_u < -1e-5 or hi_u > 1 + 1e-5 or lo_v < -1e-5 or hi_v > 1 + 1e-5:
                    material_name = document["materials"][material_index].get(
                        "name", f"material{material_index}")
                    raise PackError(
                        f"{model['path'].name}: material '{material_name}' has UVs outside [0, 1] "
                        f"(u {lo_u:.3f}..{hi_u:.3f}, v {lo_v:.3f}..{hi_v:.3f}). Inside an atlas a "
                        f"repeat wraps the whole atlas, not the region, so the prop would sample "
                        f"its neighbours. Bake the tiling into a single texture first.")

                scale_u, scale_v, offset_u, offset_v = transform
                packed = bytearray()
                us, vs = [], []
                for u, v in ((value[0], value[1]) for value in values):
                    nu = offset_u + u * scale_u
                    nv = offset_v + v * scale_v
                    us.append(nu)
                    vs.append(nv)
                    packed += struct.pack("<ff", nu, nv)
                # Always FLOAT on the way out, whatever came in: an atlased UV occupies a small
                # slice of [0, 1], and a normalized ushort that had 1/65535 of the source texture
                # would have 1/65535 of the WHOLE ATLAS after the rewrite -- visible drift.
                accessors.append({"bufferView": None, "componentType": 5126, "count": len(values),
                                  "type": "VEC2", "min": [min(us), min(vs)],
                                  "max": [max(us), max(vs)]})
                new_uv[cache_key] = len(accessors) - 1
                extra_views.append((bytes(packed), accessors[-1]))
            attributes["TEXCOORD_0"] = new_uv[cache_key]
            report.append({"mesh": mesh.get("name", ""), "material": material_index,
                           "region": [region["x"], region["y"], region["w"], region["h"]]})

    document["accessors"] = accessors

    # New images/textures/samplers: one atlas texture per channel, sampled CLAMP_TO_EDGE.
    document["images"] = [{"uri": atlas_names[channel]} for channel in channels]
    document["samplers"] = [{"wrapS": CLAMP_TO_EDGE, "wrapT": CLAMP_TO_EDGE}]
    document["textures"] = [{"sampler": 0, "source": i} for i in range(len(channels))]
    channel_texture = {channel: i for i, channel in enumerate(channels)}

    for index, material in enumerate(document.get("materials", [])):
        if assignments.get(index) is None:
            continue
        for channel, reference in material_slots(material).items():
            reference["index"] = channel_texture[channel]

    document, blob = compact(document, buffers, extra_views)
    return document, blob, report


def live_accessors(document: dict) -> set[int]:
    """Every accessor still reachable. Used to drop the pre-atlas UV accessors."""
    live: set[int] = set()
    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            live.update(primitive.get("attributes", {}).values())
            if "indices" in primitive:
                live.add(primitive["indices"])
            for target in primitive.get("targets", []):
                live.update(target.values())
    for skin in document.get("skins", []):
        if "inverseBindMatrices" in skin:
            live.add(skin["inverseBindMatrices"])
    for animation in document.get("animations", []):
        for sampler in animation.get("samplers", []):
            live.add(sampler["input"])
            live.add(sampler["output"])
    return live


def compact(document: dict, buffers: list[bytes],
            extra_views: list[tuple[bytes, dict]]) -> tuple[dict, bytes]:
    """Rebuild the buffer from the live views only, so the replaced textures actually leave.

    Without this the atlased prop keeps its original embedded texture as an orphan bufferView and
    the file gets BIGGER, which would make the whole exercise pointless.
    """
    extra_by_accessor = {id(accessor): payload for payload, accessor in extra_views}
    keep = sorted(live_accessors(document))
    remap = {old: new for new, old in enumerate(keep)}

    old_accessors = document["accessors"]
    new_accessors: list[dict] = []
    blob = bytearray()
    views: list[dict] = []
    view_remap: dict[int, int] = {}

    def append(payload: bytes, stride: int | None, target: int | None) -> int:
        while len(blob) % 4:
            blob.append(0)
        view = {"buffer": 0, "byteOffset": len(blob), "byteLength": len(payload)}
        if stride is not None:
            view["byteStride"] = stride
        if target is not None:
            view["target"] = target
        blob.extend(payload)
        views.append(view)
        return len(views) - 1

    for old in keep:
        accessor = dict(old_accessors[old])
        payload = extra_by_accessor.get(id(old_accessors[old]))
        if payload is not None:
            accessor["bufferView"] = append(payload, None, 34962)
            accessor.pop("byteOffset", None)
        elif accessor.get("bufferView") is not None:
            source_index = accessor["bufferView"]
            if source_index not in view_remap:
                view = document["bufferViews"][source_index]
                start = view.get("byteOffset", 0)
                data = buffers[view.get("buffer", 0)][start:start + view["byteLength"]]
                view_remap[source_index] = append(data, view.get("byteStride"), view.get("target"))
            accessor["bufferView"] = view_remap[source_index]
        new_accessors.append(accessor)

    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            primitive["attributes"] = {k: remap[v] for k, v in primitive["attributes"].items()}
            if "indices" in primitive:
                primitive["indices"] = remap[primitive["indices"]]
            if "targets" in primitive:
                primitive["targets"] = [{k: remap[v] for k, v in t.items()}
                                        for t in primitive["targets"]]
    for skin in document.get("skins", []):
        if "inverseBindMatrices" in skin:
            skin["inverseBindMatrices"] = remap[skin["inverseBindMatrices"]]
    for animation in document.get("animations", []):
        for sampler in animation.get("samplers", []):
            sampler["input"] = remap[sampler["input"]]
            sampler["output"] = remap[sampler["output"]]

    document["accessors"] = new_accessors
    document["bufferViews"] = views
    document["buffers"] = [{"byteLength": len(blob)}] if blob else []
    document.setdefault("asset", {})["generator"] = "cna-house tools/assets/atlas_pack.py"
    return document, bytes(blob)


def pack(paths: list[Path], out: Path, name: str, gutter: int, alignment: int,
         max_size: int) -> dict:
    models, sets = collect(paths)
    if not sets:
        raise PackError("none of the given models has a material with a texture")

    side, regions = lay_out(sets, gutter, alignment, max_size)
    channels = [c for c in CHANNELS if any(c in entry["images"] for entry in sets.values())]
    atlas_names = {c: f"{name}_{CHANNEL_SUFFIX[c]}.png" for c in channels}

    out.mkdir(parents=True, exist_ok=True)
    written: dict[str, str] = {}
    for channel in channels:
        pixels = stamp(side, channel, regions, sets)
        data = png_encode(side, side, pixels)
        (out / atlas_names[channel]).write_bytes(data)
        written[atlas_names[channel]] = hashlib.sha256(data).hexdigest()

    regions_by_identity = {region["identity"]: region for region in regions}
    model_rows = []
    for model in models:
        document, blob, report = rewrite_model(model, regions_by_identity, side, channels,
                                               atlas_names)
        target = out / model["path"].name
        data = write_glb(target, document, blob)
        model_rows.append({"source": model["path"].name,
                           "sourceSha256": model["sha256"],
                           "output": target.name,
                           "outputSha256": hashlib.sha256(data).hexdigest(),
                           "outputBytes": len(data),
                           "primitives": report})

    metadata = {
        "tool": "tools/assets/atlas_pack.py",
        "formatVersion": 1,
        "atlas": name,
        "gutter": gutter,
        "align": alignment,
        "size": [side, side],
        "mipSafeLevels": mip_safe_levels(regions, side),
        "channels": {c: atlas_names[c] for c in channels},
        "atlasSha256": written,
        "regions": [{"x": r["x"], "y": r["y"], "w": r["w"], "h": r["h"],
                     "boxX": r["boxX"], "boxY": r["boxY"],
                     "boxW": r["boxW"], "boxH": r["boxH"],
                     "sources": dict(sets[r["identity"]]["digests"]),
                     "materials": sorted(sets[r["identity"]]["materials"])}
                    for r in regions],
        "models": model_rows,
    }
    (out / f"{name}.atlas.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return metadata


# ------------------------------------------------------------------------------------- verify ----

def verify(directory: Path) -> list[str]:
    """Read the written atlas back and check every claim the metadata makes.

    This deliberately reads the OUTPUT files rather than the in-memory state the pack produced: a
    transform computed correctly and then written to the wrong accessor is exactly the bug a check
    against in-memory state cannot see.
    """
    problems: list[str] = []
    candidates = sorted(directory.glob("*.atlas.json"))
    if not candidates:
        return [f"{directory}: no *.atlas.json metadata"]
    metadata = json.loads(candidates[0].read_text(encoding="utf-8"))
    side = metadata["size"][0]

    for filename, digest in sorted(metadata["atlasSha256"].items()):
        path = directory / filename
        if not path.is_file():
            problems.append(f"{filename}: missing")
            continue
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            problems.append(f"{filename}: sha256 does not match the metadata")
        width, height, _ = png_decode(path.read_bytes())
        if (width, height) != (side, side):
            problems.append(f"{filename}: is {width}x{height}, metadata says {side}x{side}")

    regions = metadata["regions"]
    for region in regions:
        if region["x"] + region["w"] > side or region["y"] + region["h"] > side:
            problems.append(f"region at ({region['x']}, {region['y']}) runs off the atlas")
    for i, a in enumerate(regions):
        for b in regions[i + 1:]:
            if (a["x"] < b["x"] + b["w"] and b["x"] < a["x"] + a["w"]
                    and a["y"] < b["y"] + b["h"] and b["y"] < a["y"] + a["h"]):
                problems.append(f"regions at ({a['x']}, {a['y']}) and ({b['x']}, {b['y']}) overlap")

    for row in metadata["models"]:
        path = directory / row["output"]
        if not path.is_file():
            problems.append(f"{row['output']}: missing")
            continue
        if hashlib.sha256(path.read_bytes()).hexdigest() != row["outputSha256"]:
            problems.append(f"{row['output']}: sha256 does not match the metadata")
        document, blob = read_model(path)
        buffers = buffer_bytes(document, blob, path.parent)
        material_region = {}
        for entry in row["primitives"]:
            material_region[entry["material"]] = entry["region"]
        for mesh in document.get("meshes", []):
            for primitive in mesh.get("primitives", []):
                material_index = primitive.get("material")
                if material_index not in material_region:
                    continue
                x, y, w, h = material_region[material_index]
                uv_accessor = primitive["attributes"]["TEXCOORD_0"]
                position = primitive["attributes"].get("POSITION")
                if position is not None:
                    expected = document["accessors"][position]["count"]
                    actual = document["accessors"][uv_accessor]["count"]
                    if actual != expected:
                        problems.append(
                            f"{row['output']}: a rewritten TEXCOORD_0 has {actual} elements but "
                            f"POSITION has {expected}")
                values = read_accessor(document, buffers, uv_accessor)
                lo_u, hi_u = x / side, (x + w) / side
                lo_v, hi_v = y / side, (y + h) / side
                for u, v in ((value[0], value[1]) for value in values):
                    if not (lo_u - 1e-6 <= u <= hi_u + 1e-6 and lo_v - 1e-6 <= v <= hi_v + 1e-6):
                        problems.append(
                            f"{row['output']}: a UV ({u:.6f}, {v:.6f}) of material "
                            f"{material_index} falls outside its region "
                            f"[{lo_u:.6f}, {hi_u:.6f}] x [{lo_v:.6f}, {hi_v:.6f}]")
                        break
        for sampler in document.get("samplers", []):
            if sampler.get("wrapS") != CLAMP_TO_EDGE or sampler.get("wrapT") != CLAMP_TO_EDGE:
                problems.append(f"{row['output']}: an atlas sampler does not clamp")
    return problems


# ------------------------------------------------------------------------------------ fixture ----

def _checker(width: int, height: int, cell: int, a: tuple, b: tuple) -> bytes:
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            pixels += bytes(a if ((x // cell) + (y // cell)) % 2 == 0 else b)
    return bytes(pixels)


def _gradient(width: int, height: int, tint: int) -> bytes:
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            pixels += bytes((x * 255 // max(width - 1, 1), y * 255 // max(height - 1, 1),
                             tint, 255))
    return bytes(pixels)


def _fixture_glb(path: Path, materials: list[dict], primitives: list[dict],
                 images: list[tuple[str, bytes]], embed: bool) -> None:
    """Build a small deterministic prop. `primitives` is a list of dicts with `material`, `uv`
    (list of (u, v)) and `normalized` (write TEXCOORD_0 as a normalized ushort)."""
    blob = bytearray()
    views: list[dict] = []
    accessors: list[dict] = []

    def add(payload: bytes, accessor: dict, stride: int | None = None,
            target: int | None = 34962) -> int:
        while len(blob) % 4:
            blob.append(0)
        view = {"buffer": 0, "byteOffset": len(blob), "byteLength": len(payload)}
        if stride is not None:
            view["byteStride"] = stride
        if target is not None:
            view["target"] = target
        blob.extend(payload)
        views.append(view)
        accessor["bufferView"] = len(views) - 1
        accessors.append(accessor)
        return len(accessors) - 1

    # One quad's worth of positions and indices, shared by every primitive in the fixture: the
    # point of the fixture is the UVs and the material association, not the geometry.
    quad = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (1.0, 1.0, 0.0), (0.0, 1.0, 0.0)]
    a_pos = add(b"".join(struct.pack("<3f", *p) for p in quad),
                {"componentType": 5126, "count": 4, "type": "VEC3",
                 "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0]})
    a_idx = add(struct.pack("<6H", 0, 1, 2, 0, 2, 3),
                {"componentType": 5123, "count": 6, "type": "SCALAR", "min": [0], "max": [5]},
                target=34963)

    uv_accessors: dict[tuple, int] = {}
    out_primitives = []
    for primitive in primitives:
        key = (tuple(primitive["uv"]), primitive.get("normalized", False))
        if key not in uv_accessors:
            if primitive.get("normalized"):
                payload = b"".join(struct.pack("<2H", round(u * 65535), round(v * 65535))
                                   for u, v in primitive["uv"])
                accessor = {"componentType": 5123, "normalized": True, "count": len(primitive["uv"]),
                            "type": "VEC2"}
            else:
                payload = b"".join(struct.pack("<2f", u, v) for u, v in primitive["uv"])
                accessor = {"componentType": 5126, "count": len(primitive["uv"]), "type": "VEC2"}
            accessor["min"] = [min(u for u, _ in primitive["uv"]),
                               min(v for _, v in primitive["uv"])]
            accessor["max"] = [max(u for u, _ in primitive["uv"]),
                               max(v for _, v in primitive["uv"])]
            uv_accessors[key] = add(payload, accessor)
        out_primitives.append({"attributes": {"POSITION": a_pos, "TEXCOORD_0": uv_accessors[key]},
                               "indices": a_idx, "mode": 4, "material": primitive["material"]})

    gltf_images = []
    if embed:
        for _, data in images:
            while len(blob) % 4:
                blob.append(0)
            views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(data)})
            blob.extend(data)
            gltf_images.append({"bufferView": len(views) - 1, "mimeType": "image/png"})
    else:
        for filename, data in images:
            (path.parent / filename).write_bytes(data)
            gltf_images.append({"uri": filename})

    document = {
        "asset": {"version": "2.0", "generator": "cna-house atlas_pack.py --make-fixture"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [{"name": path.stem, "mesh": 0}],
        "meshes": [{"name": path.stem, "primitives": out_primitives}],
        "materials": materials,
        "images": gltf_images,
        "samplers": [{"wrapS": 10497, "wrapT": 10497}],
        "textures": [{"sampler": 0, "source": i} for i in range(len(gltf_images))],
        "accessors": accessors, "bufferViews": views, "buffers": [{"byteLength": len(blob)}],
    }
    write_glb(path, document, bytes(blob))


def make_fixture(directory: Path) -> list[Path]:
    """A prop set that exercises every hard part, and nothing that does not.

    Two good props and two bad ones. The good pair covers: different source dimensions between
    materials, a non-square texture, a material with a normal map and one without, a TEXCOORD_0
    accessor SHARED by two primitives with different materials, a normalized-ushort TEXCOORD_0, and
    two materials whose texture sets are byte-identical and must therefore share one region.
    """
    directory.mkdir(parents=True, exist_ok=True)

    albedo_a = png_encode(64, 64, _checker(64, 64, 8, (200, 40, 40, 255), (250, 250, 250, 255)))
    normal_a = png_encode(64, 64, _gradient(64, 64, 255))
    # 30x18 on purpose: not a power of two and not a multiple of --align, so the padding and the
    # mip-safety arithmetic are exercised rather than being accidentally satisfied by the source.
    albedo_b = png_encode(30, 18, _checker(30, 18, 4, (40, 200, 40, 255), (10, 10, 10, 255)))
    albedo_c = png_encode(128, 128, _gradient(128, 128, 90))
    normal_c = png_encode(128, 128, _checker(128, 128, 16, (128, 128, 255, 255), (140, 120, 250, 255)))

    def pbr(name, base_texture, normal_texture=None):
        material = {"name": name,
                    "pbrMetallicRoughness": {"baseColorTexture": {"index": base_texture},
                                             "metallicFactor": 0.0, "roughnessFactor": 0.8}}
        if normal_texture is not None:
            material["normalTexture"] = {"index": normal_texture, "scale": 1.0}
        return material

    paths = []

    # prop_one: images embedded in the GLB. Materials A (albedo + normal, 64^2) and B (albedo only,
    # 30x18, non-square). BOTH primitives share one TEXCOORD_0 accessor -- the accessor-split case.
    shared_uv = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
    one = directory / "prop_one.glb"
    _fixture_glb(one,
                 materials=[pbr("MatA", 0, 1), pbr("MatB", 2)],
                 primitives=[{"material": 0, "uv": shared_uv},
                             {"material": 1, "uv": shared_uv}],
                 images=[("a_bc.png", albedo_a), ("a_nrm.png", normal_a), ("b_bc.png", albedo_b)],
                 embed=True)
    paths.append(one)

    # prop_two: images as external files. Material C (128^2 pair) with normalized-ushort UVs that
    # do not span the full square, and material D whose textures are byte-identical to A's and must
    # therefore be deduplicated into A's region.
    inset_uv = [(0.25, 0.125), (0.75, 0.125), (0.75, 0.875), (0.25, 0.875)]
    two = directory / "prop_two.glb"
    _fixture_glb(two,
                 materials=[pbr("MatC", 0, 1), pbr("MatD", 2, 3)],
                 primitives=[{"material": 0, "uv": inset_uv, "normalized": True},
                             {"material": 1, "uv": shared_uv}],
                 images=[("c_bc.png", albedo_c), ("c_nrm.png", normal_c),
                         ("d_bc.png", albedo_a), ("d_nrm.png", normal_a)],
                 embed=False)
    paths.append(two)

    # bad_tiled: UVs run to 2.0. Must be refused.
    tiled = directory / "bad_tiled.glb"
    _fixture_glb(tiled, materials=[pbr("MatTiled", 0)],
                 primitives=[{"material": 0, "uv": [(0.0, 0.0), (2.0, 0.0), (2.0, 2.0), (0.0, 2.0)]}],
                 images=[("t_bc.png", albedo_a)], embed=True)

    # bad_mismatch: a 64^2 albedo with a 30x18 normal. Must be refused.
    mismatch = directory / "bad_mismatch.glb"
    _fixture_glb(mismatch, materials=[pbr("MatMismatch", 0, 1)],
                 primitives=[{"material": 0, "uv": shared_uv}],
                 images=[("m_bc.png", albedo_a), ("m_nrm.png", albedo_b)], embed=True)

    return paths


# ----------------------------------------------------------------------------------- selftest ----

def selftest() -> int:
    failures = 0

    def require(condition: bool, message: str) -> None:
        nonlocal failures
        if condition:
            print(f"  {message}")
        else:
            print(f"  SELFTEST FAILED: {message}", file=sys.stderr)
            failures += 1

    workspace = Path(tempfile.mkdtemp(prefix="atlas_pack_selftest_"))
    try:
        source = workspace / "src"
        props = make_fixture(source)
        first = workspace / "out1"
        metadata = pack(props, first, "MAT_ATLAS_TEST", gutter=4, alignment=4, max_size=2048)

        # 1. The written files pass their own verification -- read back from disk, not from memory.
        problems = verify(first)
        require(not problems, f"the written atlas verifies ({len(problems)} problems)")
        for problem in problems:
            print(f"      {problem}", file=sys.stderr)

        # 2. Deduplication: MatD's textures are byte-identical to MatA's, so four materials must
        #    occupy three regions.
        require(len(metadata["regions"]) == 3,
                f"4 materials with 3 distinct texture sets pack into "
                f"{len(metadata['regions'])} regions")
        shared = [r for r in metadata["regions"] if len(r["materials"]) == 2]
        require(len(shared) == 1 and shared[0]["materials"] == ["prop_one.glb:MatA",
                                                                "prop_two.glb:MatD"],
                "two materials with identical textures share one region")

        # 3. Both channels present, both atlases written, the layout shared.
        require(sorted(metadata["channels"]) == ["baseColor", "normal"],
                f"one atlas per used channel: {sorted(metadata['channels'])}")

        # 4. The UVs actually moved. A tool that returned its input unchanged would pass every
        #    containment check below if the region happened to be the whole atlas.
        document, blob = read_model(first / "prop_one.glb")
        buffers = buffer_bytes(document, blob, first)
        uvs = read_accessor(document, buffers, document["meshes"][0]["primitives"][0]
                            ["attributes"]["TEXCOORD_0"])
        require(max(u for u, _ in uvs) < 0.99, f"UVs were rewritten into a sub-region "
                                               f"(max u = {max(u for u, _ in uvs):.4f})")

        # 5. The accessor shared by two primitives with different materials was SPLIT.
        primitives = document["meshes"][0]["primitives"]
        require(primitives[0]["attributes"]["TEXCOORD_0"]
                != primitives[1]["attributes"]["TEXCOORD_0"],
                "a TEXCOORD_0 shared by two materials is split into two accessors")

        # 6. ...and each half landed in its OWN region, not both in the first one.
        side = metadata["size"][0]
        region_of = {}
        for region in metadata["regions"]:
            for material in region["materials"]:
                region_of[material] = region
        uvs_b = read_accessor(document, buffers, primitives[1]["attributes"]["TEXCOORD_0"])
        region_b = region_of["prop_one.glb:MatB"]
        require(all(region_b["x"] / side - 1e-6 <= u <= (region_b["x"] + region_b["w"]) / side + 1e-6
                    for u, _ in uvs_b),
                "the split accessor's second half lands in the second material's region")

        # 7. UV 0 and 1 map exactly onto the region edges -- off by half a texel here is the
        #    classic atlas bug and it is invisible until something is sampled at a grazing angle.
        region_a = region_of["prop_one.glb:MatA"]
        require(abs(min(u for u, _ in uvs) - region_a["x"] / side) < 1e-9
                and abs(max(u for u, _ in uvs) - (region_a["x"] + region_a["w"]) / side) < 1e-9,
                "u = 0 and u = 1 map exactly onto the region's left and right edges")

        # 8. TEXCOORD_0 is FLOAT on the way out even though prop_two's input was normalized ushort.
        doc_two, blob_two = read_model(first / "prop_two.glb")
        accessor = doc_two["accessors"][doc_two["meshes"][0]["primitives"][0]["attributes"]
                                       ["TEXCOORD_0"]]
        require(accessor["componentType"] == 5126 and not accessor.get("normalized"),
                "a normalized-ushort TEXCOORD_0 is rewritten as FLOAT")

        # 9. The atlas carries the source texels unchanged: no resampling, no colour management.
        atlas = first / metadata["channels"]["baseColor"]
        width, _, texels = png_decode(atlas.read_bytes())

        def texel(x: int, y: int) -> tuple:
            offset = (y * width + x) * 4
            return tuple(texels[offset:offset + 4])

        require(texel(region_a["x"], region_a["y"]) == (200, 40, 40, 255),
                "the region's top-left texel is the source's top-left texel, unmodified")

        # 10. The gutter is the edge texel replicated, not black -- the seam bug.
        require(texel(region_a["x"] - 1, region_a["y"]) == texel(region_a["x"], region_a["y"])
                and texel(region_a["x"], region_a["y"] - 1) == texel(region_a["x"],
                                                                     region_a["y"]),
                "the gutter replicates the edge texel rather than filling with black")
        require(texel(region_a["x"] + region_a["w"], region_a["y"])
                == texel(region_a["x"] + region_a["w"] - 1, region_a["y"]),
                "the right-hand gutter replicates the right-hand edge texel")

        # 11. A material with no normal map gets the NEUTRAL normal, not black.
        normal_atlas = first / metadata["channels"]["normal"]
        n_width, _, n_texels = png_decode(normal_atlas.read_bytes())
        offset = ((region_b["y"] + 1) * n_width + region_b["x"] + 1) * 4
        # The literal is written out rather than compared against NEUTRAL["normal"]: comparing a
        # value against the constant that produced it is a tautology, and an injected change to
        # that constant went undetected until this line stopped referring to it.
        require(tuple(n_texels[offset:offset + 4]) == (128, 128, 255, 255),
                "a material without a normal map gets the neutral normal (128, 128, 255, 255)")
        require(NEUTRAL == {"baseColor": (255, 255, 255, 255),
                            "metallicRoughness": (255, 255, 255, 255),
                            "normal": (128, 128, 255, 255),
                            "occlusion": (255, 255, 255, 255),
                            "emissive": (0, 0, 0, 255)},
                "the neutral fill of every channel is still the value that makes it a no-op")

        # 12. The reported mip-safe level is EXACT, not merely a lower bound: 2**L divides every
        #     origin and size, and 2**(L+1) does not (or would exceed the gutter). A tool that
        #     always reported 0 would satisfy a >= check and tell a reader nothing.
        # Checked by a DIFFERENT rule than the one that produced it: at the reported level, each
        # region's grid-snapped rectangle must stay inside its own padded box. The boxes are
        # disjoint by construction, so box containment implies no foreign content -- and it is a
        # stricter statement than mip_safe_levels() itself makes, so it cannot be circular.
        level = metadata["mipSafeLevels"]
        step = 1 << level
        contained = all((r["x"] // step) * step >= r["boxX"]
                        and (r["y"] // step) * step >= r["boxY"]
                        and -(-(r["x"] + r["w"]) // step) * step <= r["boxX"] + r["boxW"]
                        and -(-(r["y"] + r["h"]) // step) * step <= r["boxY"] + r["boxH"]
                        for r in metadata["regions"])
        require(level >= 2 and contained,
                f"mipSafeLevels = {level}: at that level every region's grid-snapped rectangle is "
                f"still inside its own padded box")
        # ...and it is a maximum, not a lower bound the tool could have got by always saying 0.
        bigger = 1 << (level + 1)
        require(not all((r["x"] // bigger) * bigger >= r["boxX"]
                        and -(-(r["x"] + r["w"]) // bigger) * bigger <= r["boxX"] + r["boxW"]
                        and (r["y"] // bigger) * bigger >= r["boxY"]
                        and -(-(r["y"] + r["h"]) // bigger) * bigger <= r["boxY"] + r["boxH"]
                        for r in metadata["regions"]),
                f"level {level + 1} would take a region past its own box, so {level} is a maximum")

        # 12b. --align is doing work: every padded box origin and size is a multiple of it, even
        #      though the fixture deliberately contains a 30x18 source that is not.
        require(all(r[k] % metadata["align"] == 0 for r in metadata["regions"]
                    for k in ("boxX", "boxY", "boxW", "boxH")),
                f"every padded box is aligned to {metadata['align']} texels")
        require(any(r["w"] % metadata["align"] or r["h"] % metadata["align"]
                    for r in metadata["regions"]),
                "the fixture really does contain a source that alignment has to round up")

        # 13. Regions do not overlap and every one fits. (verify() checks this; assert it moved.)
        require(all(r["x"] + r["w"] <= side and r["y"] + r["h"] <= side
                    for r in metadata["regions"]), "every region fits inside the atlas")

        # 14. The output prop got SMALLER: the orphaned embedded texture actually left the file.
        require((first / "prop_one.glb").stat().st_size < props[0].stat().st_size,
                f"the atlased prop is smaller than its source "
                f"({(first / 'prop_one.glb').stat().st_size} < {props[0].stat().st_size} bytes)")

        # 15. Determinism: the whole pack again, into a different directory, byte for byte.
        second = workspace / "out2"
        pack(props, second, "MAT_ATLAS_TEST", gutter=4, alignment=4, max_size=2048)
        names = sorted(p.name for p in first.iterdir())
        require(names == sorted(p.name for p in second.iterdir()),
                "a second run writes the same set of files")
        differing = [n for n in names
                     if (first / n).read_bytes() != (second / n).read_bytes()]
        require(not differing, f"every output file is byte-identical on a second run "
                               f"(differing: {differing})")

        # 16. The layout is a function of texture CONTENT, not of file names. Reversing the command
        #     line proves nothing on its own -- collect() sorts the paths, so the two runs are the
        #     same run. Renaming the models is the test that discriminates: it changes the order the
        #     texture sets are discovered in, and only the layout sort keeps the atlas identical.
        third = workspace / "out3"
        renamed_dir = workspace / "renamed"
        renamed_dir.mkdir()
        renamed = []
        # The prefixes REVERSE the sorted order: prop_one becomes zz_, prop_two becomes aa_. A
        # rename that preserved the order would prove nothing, because collect() sorts the paths.
        for prefix, path in zip(("zz_", "aa_"), props):
            for sibling in sorted(path.parent.glob("*.png")):
                shutil.copy2(sibling, renamed_dir / sibling.name)
            target = renamed_dir / f"{prefix}{path.name}"
            shutil.copy2(path, target)
            renamed.append(target)
        renamed.reverse()
        pack(renamed, third, "MAT_ATLAS_TEST", gutter=4, alignment=4, max_size=2048)
        require(all((first / n).read_bytes() == (third / n).read_bytes()
                    for n in metadata["channels"].values()),
                "renaming and reordering the models leaves every atlas image byte-identical")
        renamed_meta = json.loads((third / "MAT_ATLAS_TEST.atlas.json").read_text())
        require([[r["x"], r["y"], r["w"], r["h"]] for r in renamed_meta["regions"]]
                == [[r["x"], r["y"], r["w"], r["h"]] for r in metadata["regions"]],
                "the region layout is identical after renaming and reordering the models")

        # 17. Nothing dead survives the rewrite: every bufferView is reached by an accessor and
        #     every accessor by a mesh, skin or animation. Otherwise the pre-atlas UVs stay in the
        #     file, invisibly, and the atlas costs size instead of saving it.
        for name in ("prop_one.glb", "prop_two.glb"):
            doc, _ = read_model(first / name)
            live = live_accessors(doc)
            used_views = {a["bufferView"] for a in doc["accessors"]
                          if a.get("bufferView") is not None}
            require(live == set(range(len(doc["accessors"])))
                    and used_views == set(range(len(doc["bufferViews"]))),
                    f"{name} keeps no dead accessor or bufferView "
                    f"({len(doc['accessors'])} accessors, {len(doc['bufferViews'])} views)")

        # 17. Tiling UVs are refused, and the message says why.
        try:
            pack([source / "bad_tiled.glb"], workspace / "bad1", "X", 4, 4, 2048)
            require(False, "a prop with UVs outside [0, 1] is refused")
        except PackError as error:
            require("outside [0, 1]" in str(error), f"a prop with tiling UVs is refused: {error}")

        # 18. Mismatched map sizes within one material are refused.
        try:
            pack([source / "bad_mismatch.glb"], workspace / "bad2", "X", 4, 4, 2048)
            require(False, "a material whose maps differ in size is refused")
        except PackError as error:
            require("mixes texture sizes" in str(error),
                    f"a material whose maps differ in size is refused: {error}")

        # 19. verify() must FAIL on a damaged output, or it is decoration. Move one UV out of its
        #     region and check the checker notices.
        damaged = workspace / "damaged"
        shutil.copytree(first, damaged)
        doc, bin_blob = read_model(damaged / "prop_one.glb")
        accessor = doc["accessors"][doc["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_0"]]
        view = doc["bufferViews"][accessor["bufferView"]]
        mutable = bytearray(bin_blob)
        struct.pack_into("<f", mutable, view["byteOffset"], 0.99)
        write_glb(damaged / "prop_one.glb", doc, bytes(mutable))
        damaged_problems = verify(damaged)
        require(any("falls outside its region" in p for p in damaged_problems),
                "verify() catches a UV moved outside its region")

        # 20. An atlas whose textures cannot fit is refused rather than silently truncated.
        try:
            pack(props, workspace / "bad3", "X", gutter=4, alignment=4, max_size=64)
            require(False, "an over-full atlas is refused")
        except PackError as error:
            require("do not fit" in str(error), f"an over-full atlas is refused: {error}")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("atlas_pack: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("models", nargs="*", type=Path, help="the props to atlas")
    parser.add_argument("--name", default="MAT_ATLAS", help="atlas name; prefixes the output files")
    parser.add_argument("--out", type=Path, help="output directory")
    parser.add_argument("--gutter", type=int, default=4,
                        help="texels of edge-replicated border around each region (default 4)")
    parser.add_argument("--align", type=int, default=4,
                        help="round region boxes up to this multiple (default 4)")
    parser.add_argument("--max-size", type=int, default=2048,
                        help="largest square atlas to try (default 2048)")
    parser.add_argument("--verify", type=Path, metavar="DIR",
                        help="check an already-written atlas directory")
    parser.add_argument("--make-fixture", type=Path, metavar="DIR",
                        help="write the deterministic test prop set and exit")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    if args.make_fixture is not None:
        for path in make_fixture(args.make_fixture):
            print(f"{path}  {path.stat().st_size} bytes")
        return 0

    if args.verify is not None:
        problems = verify(args.verify)
        for problem in problems:
            print(f"atlas_pack: {problem}", file=sys.stderr)
        if problems:
            return 1
        print(f"atlas_pack: {args.verify} verifies.")
        return 0

    if not args.models or args.out is None:
        parser.print_help()
        return 2

    try:
        metadata = pack(args.models, args.out, args.name, args.gutter, args.align, args.max_size)
    except PackError as error:
        print(f"atlas_pack: {error}", file=sys.stderr)
        return 1

    print(f"{metadata['atlas']}: {metadata['size'][0]}x{metadata['size'][1]}, "
          f"{len(metadata['regions'])} regions, "
          f"{len(metadata['channels'])} channel atlas(es), "
          f"mip-safe to level {metadata['mipSafeLevels']}")
    for name in sorted(metadata["channels"].values()):
        print(f"  {name}")
    for row in metadata["models"]:
        print(f"  {row['source']} -> {row['output']}  {row['outputBytes']} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
