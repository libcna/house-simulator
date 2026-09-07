#!/usr/bin/env python3
"""cnb_model.py -- read a compiled `.cnb` Model's bone table, from the bytes.

`HOUSE-00225`. `ClipLibrary::BindTo` resolves every `.chanim` joint name against the compiled
model's `Model::Bones`, and a name it cannot find is a **fatal content error at load**
(`HOUSE-00167`). A gate that only compared the sidecar against the *source* `.glb` would check the
wrong artefact: what ships is the `.cnb`, and what the game binds against is the bone table inside
it.

So this reads that table. Only what the check needs -- the header, the table of contents, the
string table (`MSTR`) and the bone rows (`MBON`) -- against `cnanext/docs/cnb-format.md`, which is
normative and states that `CnbSpecConformanceTests.cpp` fails if the document and the code drift
apart.

**Both CRC-32Cs are verified**, the header's and each chunk's. A reader that skipped them would
report a corrupt file as a wrong bone list, which is the least useful diagnosis available. CRC-32C
is Castagnoli, polynomial `0x1EDC6F41` reflected as `0x82F63B78` -- *not* zlib's CRC-32, and using
the wrong one is a mistake that produces a plausible-looking number for every input.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path


class CnbError(Exception):
    """A refusal with a reason a human can act on."""


MAGIC = b"CNB\x1a"
HEADER_SIZE = 64
TOC_ENTRY_SIZE = 48
#: §7's asset type for a Model.
ASSET_TYPE_MODEL = 5
NO_STRING = 0xFFFFFFFF
#: §6.5: `MBON` rows are exactly 72 bytes -- a name index, a parent, and sixteen floats.
BONE_ROW_SIZE = 72

_CRC32C_TABLE: list[int] = []


def crc32c(data: bytes) -> int:
    """Castagnoli CRC-32C, reflected. NOT `zlib.crc32`, which is a different polynomial."""
    if not _CRC32C_TABLE:
        for index in range(256):
            value = index
            for _ in range(8):
                value = (value >> 1) ^ (0x82F63B78 if value & 1 else 0)
            _CRC32C_TABLE.append(value)
    crc = 0xFFFFFFFF
    for byte in data:
        crc = _CRC32C_TABLE[(crc ^ byte) & 0xFF] ^ (crc >> 8)
    return crc ^ 0xFFFFFFFF


def _string(data: bytes, offset: int) -> tuple[str, int]:
    (length,) = struct.unpack_from("<I", data, offset)
    offset += 4
    if offset + length > len(data):
        raise CnbError("a string runs past the end of its chunk")
    return data[offset:offset + length].decode("utf-8"), offset + length


def read_bones(path: Path) -> list[dict]:
    """The model's bone table: name, parent and position in `Model::Bones` order.

    Order matters and is checked: §6.5 requires the table to be parent-before-child, because
    `Model::CopyAbsoluteBoneTransformsTo` composes world transforms in one ascending pass and a
    bone whose parent came later would be composed against a slot not yet written -- quietly.
    """
    data = path.read_bytes()
    if len(data) < HEADER_SIZE:
        raise CnbError(f"{path.name} is shorter than a CNB header")
    if data[:4] != MAGIC:
        raise CnbError(f"{path.name} does not start with the CNB magic")

    (major, minor, header_flags, asset_type, schema_version, chunk_count) = \
        struct.unpack_from("<HHIIII", data, 4)
    (file_size, toc_offset) = struct.unpack_from("<QQ", data, 24)
    (toc_checksum, header_checksum) = struct.unpack_from("<II", data, 40)

    # Verified BEFORE any offset in the header is used for arithmetic (§3): a corrupt header is
    # precisely the case in which the offsets cannot be believed.
    if crc32c(data[:44]) != header_checksum:
        raise CnbError(f"{path.name}: the header checksum does not match; the file is corrupt")
    if major != 1:
        raise CnbError(f"{path.name}: container version {major}.{minor}; this reader knows 1.x")
    if header_flags != 0:
        raise CnbError(f"{path.name}: headerFlags is {header_flags:#x}, which must be 0")
    if asset_type != ASSET_TYPE_MODEL:
        raise CnbError(f"{path.name}: asset type {asset_type}, not a Model ({ASSET_TYPE_MODEL})")
    if file_size != len(data):
        raise CnbError(f"{path.name}: declares {file_size} bytes but the file is {len(data)}")

    toc = data[toc_offset:toc_offset + TOC_ENTRY_SIZE * chunk_count]
    if len(toc) != TOC_ENTRY_SIZE * chunk_count:
        raise CnbError(f"{path.name}: the table of contents runs past the end of the file")
    if crc32c(toc) != toc_checksum:
        raise CnbError(f"{path.name}: the table-of-contents checksum does not match")

    chunks: dict[bytes, bytes] = {}
    for index in range(chunk_count):
        entry = toc[index * TOC_ENTRY_SIZE:(index + 1) * TOC_ENTRY_SIZE]
        kind = entry[:4]
        (offset, stored, uncompressed) = struct.unpack_from("<QQQ", entry, 8)
        (checksum, compression) = struct.unpack_from("<II", entry, 32)
        payload = data[offset:offset + stored]
        if len(payload) != stored:
            raise CnbError(f"{path.name}: chunk {kind!r} runs past the end of the file")
        if crc32c(payload) != checksum:
            raise CnbError(f"{path.name}: chunk {kind!r} fails its checksum")
        if compression != 0:
            # Zstandard is legal (§8) and this reader does not need it yet. Refusing beats
            # returning an empty bone list for a file that has one.
            raise CnbError(f"{path.name}: chunk {kind!r} is compressed (method {compression}); "
                           f"this reader handles stored chunks only")
        if stored != uncompressed:
            raise CnbError(f"{path.name}: chunk {kind!r} declares different stored and "
                           f"uncompressed sizes without compression")
        chunks[kind] = payload

    if b"MDLH" not in chunks:
        raise CnbError(f"{path.name}: has no MDLH chunk, so it is not a Model")
    (flags, bone_count, part_count, mesh_count, light_count, animation_count) = \
        struct.unpack_from("<IIIIII", chunks[b"MDLH"], 0)
    del flags, part_count, mesh_count, light_count, animation_count

    if bone_count == 0:
        return []
    if b"MBON" not in chunks:
        raise CnbError(f"{path.name}: declares {bone_count} bone(s) but has no MBON chunk")

    names: list[str] = []
    if b"MSTR" in chunks:
        strings = chunks[b"MSTR"]
        (count,) = struct.unpack_from("<I", strings, 0)
        offset = 4
        for _ in range(count):
            value, offset = _string(strings, offset)
            names.append(value)

    bones_chunk = chunks[b"MBON"]
    if len(bones_chunk) != bone_count * BONE_ROW_SIZE:
        raise CnbError(f"{path.name}: MBON is {len(bones_chunk)} bytes for {bone_count} bones; "
                       f"the row size is exactly {BONE_ROW_SIZE}")

    bones = []
    for index in range(bone_count):
        base = index * BONE_ROW_SIZE
        (name_index,) = struct.unpack_from("<I", bones_chunk, base)
        (parent,) = struct.unpack_from("<i", bones_chunk, base + 4)
        if name_index != NO_STRING and name_index >= len(names):
            raise CnbError(f"{path.name}: bone {index} names string {name_index}, past the "
                           f"{len(names)}-entry table")
        if parent < -1 or parent >= index:
            raise CnbError(f"{path.name}: bone {index} has parent {parent}; §6.5 requires "
                           f"parent-before-child, and Model::CopyAbsoluteBoneTransformsTo "
                           f"composes in one ascending pass")
        bones.append({"index": index,
                      "name": names[name_index] if name_index != NO_STRING else "",
                      "parent": parent})
    return bones


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__.splitlines()[0], file=sys.stderr)
        print("usage: cnb_model.py <model.cnb> ...", file=sys.stderr)
        return 2
    status = 0
    for argument in sys.argv[1:]:
        path = Path(argument)
        try:
            bones = read_bones(path)
        except CnbError as error:
            print(f"cnb_model: {error}", file=sys.stderr)
            status = 1
            continue
        print(f"{path.name}: {len(bones)} bone(s)")
        for bone in bones:
            parent = bones[bone["parent"]]["name"] if bone["parent"] >= 0 else "-"
            print(f"  {bone['index']:>3}  {bone['name']:<28} parent {parent}")
    return status


if __name__ == "__main__":
    sys.exit(main())
