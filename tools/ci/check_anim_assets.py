#!/usr/bin/env python3
"""check_anim_assets.py -- every `.chanim` binds to its model, and no source `.glb` has two skins.

`HOUSE-00225`. Two failures this gate exists to make impossible, both of which are silent:

**A sidecar that does not bind.** `ClipLibrary::BindTo` resolves every joint name against
`Model::Bones` and a name it cannot find is a fatal content error *at load* (`HOUSE-00167`) — in
the game, on a player's machine, for an asset that built cleanly. `HOUSE-00074` measured why the
names are the binding at all: **vertex blend indices are skin-local**, not `Model::Bones` indices,
so nothing in the compiled model reproduces which bone slot *i* is and the sidecar's name list is
the only thing that does.

The comparison is made against the **compiled `.cnb`**, not against the source `.glb`. What ships
is the `.cnb` and what the game binds against is the bone table inside it; a check against the
source would pass while an importer change, a renamed node or a stale build made the shipped pair
disagree. `tools/assets/cnb_model.py` reads that table from the bytes.

**A source with two skins.** §47.0's one-skin rule is offline and absolute, and `gltf_validate.py`
already enforces it for anything it is pointed at. This gate re-states it over the whole
`assets-src/` tree so a `.glb` added without being run through that tool is still caught, and it is
cheap — the skin count is in the JSON chunk.

## Pairing

A sidecar at `<content>/Anim/<name>.chanim` belongs to the model compiled from `<name>`, wherever
under `<content>/Models/` it lives. Exactly one match is required: **none** is a sidecar for an
asset that is not in the build, and **several** is an ambiguity a convention cannot resolve — both
are reported rather than guessed at. `--pair` names a pair explicitly for anything that does not
fit the convention.

    tools/ci/check_anim_assets.py                          the source tree; and build/content if present
    tools/ci/check_anim_assets.py --content build/content
    tools/ci/check_anim_assets.py --pair a.chanim b.cnb
    tools/ci/check_anim_assets.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "assets"))
import cnb_model  # noqa: E402
import gltf_validate  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
ASSETS_SRC = REPO / "assets-src"
DEFAULT_CONTENT = REPO / "build" / "content"

CHANIM_MAGIC = b"CHAN"


def read_chanim_joints(path: Path) -> tuple[list[str], int]:
    """The joint names in blend-index order, and the root joint's index.

    Only the header and the skeleton are read: the clips are after them and this gate has no
    question about them. Parsed from `docs/anim-format.md` rather than by importing the writer, so
    a claim made here is a claim about the FORMAT and not about the writer agreeing with itself.
    """
    data = path.read_bytes()
    if len(data) < 20 or data[:4] != CHANIM_MAGIC:
        raise ValueError(f"{path.name} does not start with the CHAN magic")
    version, flags, bone_count, _clip_count = struct.unpack_from("<IIII", data, 4)
    if version != 1:
        raise ValueError(f"{path.name} declares version {version}; this gate knows version 1")
    if flags != 0:
        raise ValueError(f"{path.name} sets reserved flag bits {flags:#010x}")

    offset = 20
    names: list[str] = []
    for index in range(bone_count):
        if offset + 2 > len(data):
            raise ValueError(f"{path.name} ends inside joint {index}")
        (length,) = struct.unpack_from("<H", data, offset)
        offset += 2
        names.append(data[offset:offset + length].decode("utf-8"))
        # parent (i32) + bindPose (16 f32) + inverseBindPose (16 f32)
        offset += length + 4 + 64 + 64
    if offset + 4 > len(data):
        raise ValueError(f"{path.name} ends before its rootBone")
    (root,) = struct.unpack_from("<i", data, offset)
    return names, root


def check_pair(sidecar: Path, model: Path) -> list[str]:
    """What `ClipLibrary::BindTo` will do at load, done now."""
    problems: list[str] = []
    try:
        joints, root = read_chanim_joints(sidecar)
    except (ValueError, OSError) as error:
        return [f"{sidecar.name}: {error}"]
    try:
        bones = cnb_model.read_bones(model)
    except cnb_model.CnbError as error:
        return [str(error)]
    except OSError as error:
        return [f"{model.name}: {error}"]

    by_name: dict[str, list[int]] = {}
    for bone in bones:
        by_name.setdefault(bone["name"], []).append(bone["index"])

    for slot, name in enumerate(joints):
        found = by_name.get(name, [])
        if not found:
            # The same shape of message `BindTo` produces, and for the same reason: a report saying
            # only "the skeleton does not match" leaves someone comparing two lists of sixty-two
            # names by eye.
            problems.append(
                f"{sidecar.name}: skin joint {slot} is named '{name}', which "
                f"{model.name} has no bone for")
        elif len(found) > 1:
            # `ModelBoneCollection::TryGetValue` resolves by name; two bones with one name make
            # which one it returns an implementation detail, and the wrong one deforms silently.
            problems.append(
                f"{model.name}: {len(found)} bones are named '{name}' (indices {found}), so "
                f"binding joint {slot} of {sidecar.name} is ambiguous")

    if not (-1 <= root < len(joints)):
        problems.append(f"{sidecar.name}: rootBone is {root}, outside -1..{len(joints) - 1}")
    duplicates = sorted({name for name in joints if joints.count(name) > 1})
    if duplicates:
        problems.append(f"{sidecar.name}: joint name(s) {duplicates} appear twice; the name list "
                        f"IS the binding, so a duplicate makes two blend indices one joint")
    return problems


def find_pairs(content: Path) -> tuple[list[tuple[Path, Path]], list[str]]:
    pairs: list[tuple[Path, Path]] = []
    problems: list[str] = []
    anim = content / "Anim"
    if not anim.is_dir():
        return pairs, problems
    for sidecar in sorted(anim.rglob("*.chanim")):
        candidates = sorted((content / "Models").rglob(f"{sidecar.stem}.cnb"))
        if not candidates:
            problems.append(f"{sidecar.name} has no compiled model: nothing under "
                            f"{(content / 'Models')} is named {sidecar.stem}.cnb")
        elif len(candidates) > 1:
            problems.append(f"{sidecar.name} matches {len(candidates)} models "
                            f"({', '.join(str(c.relative_to(content)) for c in candidates)}); "
                            f"name them with --pair")
        else:
            pairs.append((sidecar, candidates[0]))
    return pairs, problems


def check_sources(root: Path) -> tuple[list[str], int]:
    """No source `.glb` declares more than one skin. §47.0, restated over the whole tree."""
    problems: list[str] = []
    count = 0
    if not root.is_dir():
        return problems, count

    def name(path: Path) -> str:
        return str(path.relative_to(REPO)) if path.is_relative_to(REPO) else str(path)

    for path in sorted(root.rglob("*.glb")) + sorted(root.rglob("*.gltf")):
        count += 1
        document, error = gltf_validate.read_gltf_json(path)
        if error is not None:
            problems.append(f"{name(path)} {error}")
            continue
        assert document is not None
        skins = document.get("skins", [])
        if len(skins) > 1:
            problems.append(
                f"{name(path)} declares {len(skins)} skins; cna-house.md §47.0 allows "
                f"exactly one per runtime file. Split it with tools/assets/skin_split.py.")
    return problems, count


# ----------------------------------------------------------------------------------- selftest ----

def _synthetic_model(path: Path, names: list[str], parents: list[int]) -> None:
    """A `.cnb` carrying just MDLH, MSTR and MBON.

    Not a loadable Model -- a real one needs MMSH, MMAT and the geometry chunks -- and that is
    fine: what is under test is the bone-table check, and building the bytes by hand is how a
    fixture with a DUPLICATE bone name can exist at all. No importer would produce one, and a
    check for it that could never be exercised is a check nobody should trust. The selftest also
    runs the real `cna-content` when it is available, so both are covered.
    """
    strings = struct.pack("<I", len(names))
    for name in names:
        encoded = name.encode("utf-8")
        strings += struct.pack("<I", len(encoded)) + encoded
    header_chunk = struct.pack("<IIIIII", 0, len(names), 0, 0, 0, 0)
    bones = b""
    for index, parent in enumerate(parents):
        bones += struct.pack("<Ii", index, parent) + b"\0" * 64

    payloads = [(b"MDLH", header_chunk), (b"MSTR", strings), (b"MBON", bones)]
    toc_offset = cnb_model.HEADER_SIZE
    offset = toc_offset + cnb_model.TOC_ENTRY_SIZE * len(payloads)
    toc = b""
    body = b""
    for kind, payload in payloads:
        toc += (kind + struct.pack("<I", 1) + struct.pack("<QQQ", offset, len(payload),
                                                          len(payload))
                + struct.pack("<IIII", cnb_model.crc32c(payload), 0, 1, 0))
        body += payload
        offset += len(payload)

    file_size = offset
    header = (cnb_model.MAGIC + struct.pack("<HHIII", 1, 0, 0, cnb_model.ASSET_TYPE_MODEL, 1)
              + struct.pack("<I", len(payloads)) + struct.pack("<QQ", file_size, toc_offset)
              + struct.pack("<I", cnb_model.crc32c(toc)))
    header += struct.pack("<I", cnb_model.crc32c(header))
    header += b"\0" * (cnb_model.HEADER_SIZE - len(header))
    path.write_bytes(header + toc + body)


def selftest() -> int:
    failures = 0

    def require(condition: bool, message: str) -> None:
        nonlocal failures
        if condition:
            print(f"  {message}")
        else:
            print(f"  SELFTEST FAILED: {message}", file=sys.stderr)
            failures += 1

    sys.path.insert(0, str(REPO / "tools" / "assets"))
    import anim_extract
    import skin_split

    workspace = Path(tempfile.mkdtemp(prefix="check_anim_selftest_"))
    try:
        source = workspace / "src"
        anim_extract.make_fixture(source)
        data, report = anim_extract.extract(source / "character.glb")
        sidecar = workspace / "character.chanim"
        sidecar.write_bytes(data)

        joints, root = read_chanim_joints(sidecar)
        require(joints == report["bones"],
                f"the sidecar's joints are read back from the bytes: {joints}")
        require(root == 0, f"the root joint index is {root}")

        # 1. The happy path, against a model whose bones are a SUPERSET of the joints -- which is
        #    the real shape: `Model::Bones` is the whole scene graph, the skin is part of it.
        good = workspace / "good.cnb"
        _synthetic_model(good, ["Root"] + joints + ["Body"],
                         [-1] + list(range(len(joints) + 1)))
        require(not check_pair(sidecar, good),
                "a model whose bones include every joint binds")

        # 2. THE failure this gate exists for, and it names the joint the way `BindTo` does.
        renamed = workspace / "renamed.cnb"
        _synthetic_model(renamed, ["Root", "Hips", "LeftUpLeg", "LeftAnkle", "RightUpLeg",
                                   "RightFoot"], [-1, 0, 1, 2, 1, 4])
        problems = check_pair(sidecar, renamed)
        require(any("'LeftFoot'" in p and "no bone for" in p for p in problems),
                f"a joint the model has no bone for is caught, by name: {problems}")

        # 3. Two bones with one name. `TryGetValue` resolves by name, so which it returns is an
        #    implementation detail and the wrong one deforms silently. No importer produces this,
        #    which is exactly why the fixture is hand-built.
        ambiguous = workspace / "ambiguous.cnb"
        _synthetic_model(ambiguous, ["Root"] + joints + ["LeftFoot"],
                         [-1] + list(range(len(joints) + 1)))
        problems = check_pair(sidecar, ambiguous)
        require(any("ambiguous" in p for p in problems),
                f"two bones with one name are caught: {problems}")

        # 4. A corrupt `.cnb` is reported as corrupt, not as a wrong bone list -- the least useful
        #    diagnosis available. Flipping one byte of a chunk must fail its CRC-32C.
        corrupt = workspace / "corrupt.cnb"
        raw = bytearray(good.read_bytes())
        raw[-1] ^= 0xFF
        corrupt.write_bytes(bytes(raw))
        problems = check_pair(sidecar, corrupt)
        require(any("checksum" in p for p in problems),
                f"a corrupt chunk is reported as corrupt: {problems}")

        # 5. CRC-32C is Castagnoli, not zlib's CRC-32. Using the wrong one produces a
        #    plausible-looking number for every input, so the constant is pinned here.
        require(cnb_model.crc32c(b"123456789") == 0xE3069283,
                f"CRC-32C of '123456789' is the standard check value "
                f"({cnb_model.crc32c(b'123456789'):#010x})")

        # 6. The multi-skin half, over a tree.
        skin_split.make_fixture(workspace / "twoskin")
        problems, count = check_sources(workspace / "twoskin")
        require(count == 1 and any("2 skins" in p for p in problems),
                f"a two-skin source in the tree is caught ({count} file(s) scanned): {problems}")
        parts = workspace / "twoskin" / "parts"
        skin_split.split(workspace / "twoskin" / "character.glb", parts)
        problems, count = check_sources(parts)
        require(count == 2 and not problems,
                f"...and the split parts pass ({count} file(s), {len(problems)} problem(s))")

        # 7. Pairing: none and several are both reported rather than guessed at.
        content = workspace / "content"
        (content / "Anim").mkdir(parents=True)
        (content / "Models" / "A").mkdir(parents=True)
        (content / "Models" / "B").mkdir(parents=True)
        (content / "Anim" / "orphan.chanim").write_bytes(data)
        _, problems = find_pairs(content)
        require(any("no compiled model" in p for p in problems),
                f"a sidecar with no model is reported: {problems}")
        (content / "Anim" / "twin.chanim").write_bytes(data)
        _synthetic_model(content / "Models" / "A" / "twin.cnb", ["Root"], [-1])
        _synthetic_model(content / "Models" / "B" / "twin.cnb", ["Root"], [-1])
        _, problems = find_pairs(content)
        require(any("matches 2 models" in p for p in problems),
                f"a sidecar matching two models is reported as ambiguous: {problems}")

        # 8. END TO END against a REAL compiled model, when `cna-content` is available. The
        #    synthetic fixtures above test the check; this tests that the check is about the same
        #    bytes the game will load.
        tool = gltf_validate.find_cna_content()
        if tool is None:
            print("  cna-content is not built here, so the end-to-end pair is not checked")
        else:
            real = workspace / "real.cnb"
            result = subprocess.run([str(tool), "build", str(source / "character.glb"),
                                     "-o", str(real), "--format", "cnb"],
                                    capture_output=True, text=True, check=False)
            require(result.returncode == 0 and real.is_file(),
                    f"the fixture compiles through cna-content ({result.returncode})")
            if real.is_file():
                bones = cnb_model.read_bones(real)
                require([b["name"] for b in bones][:1] == ["Root"] and len(bones) == 7,
                        f"the compiled model's bone table reads back: "
                        f"{[b['name'] for b in bones]}")
                require(not check_pair(sidecar, real),
                        "the sidecar binds to the REAL compiled model, checksums and all")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("check_anim_assets: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--content", type=Path, default=DEFAULT_CONTENT,
                        help="a built .cnb tree (default build/content)")
    parser.add_argument("--assets", type=Path, default=ASSETS_SRC,
                        help="the source tree to scan for multi-skin files")
    parser.add_argument("--pair", nargs=2, action="append", metavar=("CHANIM", "CNB"),
                        default=[], help="check this sidecar against this model")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    problems, scanned = check_sources(args.assets)

    pairs = [(Path(a), Path(b)) for a, b in args.pair]
    if args.content.is_dir():
        found, pairing_problems = find_pairs(args.content)
        pairs += found
        problems += pairing_problems
    elif args.content != DEFAULT_CONTENT:
        problems.append(f"{args.content} is not a directory")

    for sidecar, model in pairs:
        problems += check_pair(sidecar, model)

    for problem in problems:
        print(f"check_anim_assets: {problem}", file=sys.stderr)
    if problems:
        return 1

    where = "" if args.content.is_dir() else f"; {args.content} does not exist yet"
    print(f"check_anim_assets: {scanned} source model(s) scanned, all single-skin; "
          f"{len(pairs)} sidecar(s) bind{where}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
