#!/usr/bin/env python3
"""gltf_io.py -- the byte-level glTF/GLB reading and writing every asset tool here shares.

Extracted for `HOUSE-00194`, when `measure_stride.py` became the second tool needing to walk a
`.glb`'s accessors. `gltf_validate.py` reads only the JSON chunk and deliberately keeps its own
dependency-free parser for that; anything that needs the BINARY chunk -- vertex data, animation
samplers, embedded images -- comes here instead of growing a third copy.

Nothing in this module knows what the data means. It reads accessors honouring `byteStride` and the
`normalized` flag, resolves buffers whether they are a GLB chunk, a `data:` URI or a sibling file,
and writes a `.glb` back with sorted JSON keys so the same document always produces the same bytes.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import base64
import json
import struct
from pathlib import Path

GLB_MAGIC = 0x46546C67  # 'glTF'
CHUNK_JSON = 0x4E4F534A  # 'JSON'
CHUNK_BIN = 0x004E4942  # 'BIN\0'

#: glTF component types, and their struct code and size.
COMPONENTS = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2), 5123: ("H", 2),
              5125: ("I", 4), 5126: ("f", 4)}
#: Divisor that turns a normalized integer component back into its float value (glTF 3.11).
NORMALIZE = {5120: 127.0, 5121: 255.0, 5122: 32767.0, 5123: 65535.0}
TYPE_COUNTS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4,
               "MAT2": 4, "MAT3": 9, "MAT4": 16}


class GltfError(Exception):
    """A refusal with a reason a human can act on."""


def read_model(path: Path) -> tuple[dict, bytes]:
    """The JSON and the binary chunk of a `.glb`, or the JSON and external buffer of a `.gltf`."""
    data = path.read_bytes()
    if path.suffix.lower() != ".glb":
        document = json.loads(data.decode("utf-8"))
        return document, b""

    if len(data) < 20:
        raise GltfError(f"{path}: shorter than a glTF binary header")
    magic, version, total = struct.unpack_from("<III", data, 0)
    if magic != GLB_MAGIC:
        raise GltfError(f"{path}: magic is {magic:#010x}, not 'glTF'")
    if version != 2:
        raise GltfError(f"{path}: declares glTF version {version}; this project uses glTF 2.0")
    if total != len(data):
        raise GltfError(f"{path}: declares {total} bytes but the file is {len(data)}")

    document: dict | None = None
    blob = b""
    offset = 12
    while offset + 8 <= len(data):
        length, kind = struct.unpack_from("<II", data, offset)
        offset += 8
        if offset + length > len(data):
            raise GltfError(f"{path}: a chunk runs past the end of the file")
        payload = data[offset:offset + length]
        if kind == CHUNK_JSON:
            document = json.loads(payload.decode("utf-8"))
        elif kind == CHUNK_BIN:
            blob = payload
        offset += length
    if document is None:
        raise GltfError(f"{path}: has no JSON chunk")
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
            raise GltfError(f"buffer {index} is shorter than its declared byteLength")
    return buffers


def read_accessor(document: dict, buffers: list[bytes], index: int) -> list[tuple[float, ...]]:
    """One accessor as a list of tuples, honouring byteStride and the normalized flag."""
    accessor = document["accessors"][index]
    if "sparse" in accessor:
        raise GltfError(f"accessor {index} is sparse; this tool does not rewrite sparse accessors")
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
