#!/usr/bin/env python3
"""content_verify.py -- rebuild the content tree twice and prove the output is byte-identical.

`HOUSE-00199`, enforcing `cna-house.md` §18.4: "a `make content-verify` target rebuilds everything
and asserts the hashes match; drift is a build failure."

**What this proves and what it does not.** It builds each source root into two fresh output
directories in the same process, on the same machine, and compares every produced file byte for
byte. That is *same-machine determinism*: it catches a pipeline that embeds a timestamp, a path, a
hash-map iteration order or a random seed. It does **not** prove cross-machine reproducibility,
which is a different property. As of `HOUSE-00200` no asset in the tree is known to break it: the
last one that did was `Fonts/Hud.spritefont`, which resolved `<FontName>` to whatever font was
installed (`HOUSE-00181`) and has been replaced by five descriptors naming TTFs vendored beside
them. `KNOWN_CROSS_MACHINE_HAZARDS` is therefore empty, and deliberately kept rather than deleted --
it is where the next such asset gets recorded. The per-asset hashes are printed so that a
cross-machine comparison can be made deliberately rather than accidentally.

    tools/ci/content_verify.py                      # build twice, compare
    tools/ci/content_verify.py --hashes             # also print every output hash
    tools/ci/content_verify.py --cna-content PATH

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ASSETS_SRC = REPO / "assets-src"

#: The roots the main tree is built from. Mirrors `CNAHOUSE_CNB_ASSET_DIRS` in `CMakeLists.txt`;
#: `Effects/` is not here because it is `.xnb` and is verified by `build_effects.sh --check`.
CNB_ROOTS = ("Models", "Textures", "Audio", "Fonts", "Video", "world")

#: Assets whose output is known NOT to be reproducible across machines, with the reason and the
#: task that will fix it. They are still checked for SAME-machine determinism -- an asset that
#: differs between two runs on one machine is broken in a way no amount of vendoring fixes.
#: EMPTY, and that is the finding rather than an oversight. `Fonts/Hud.cnb` lived here until
#: `HOUSE-00200`; it was removed when the five descriptors that replaced it were measured to
#: produce byte-identical output with every system font directory hidden behind a bind mount.
KNOWN_CROSS_MACHINE_HAZARDS: dict[str, str] = {}


def find_cna_content() -> Path | None:
    # AM4-199: an explicit `CNA_CONTENT`, then the three configured-build locations, then PATH --
    # a build tree outside the repository (an out-of-source build root) is otherwise never found.
    explicit = os.environ.get("CNA_CONTENT")
    candidates = [Path(explicit)] if explicit else []
    candidates += [REPO / "build" / "CNA_BUILD" / "cna-content",
                   REPO / "build-consumer" / "CNA_BUILD" / "cna-content",
                   REPO.parent / "cna" / "build" / "cna-content"]
    on_path = shutil.which("cna-content")
    if on_path:
        candidates.append(Path(on_path))
    for candidate in candidates:
        if candidate.is_file() and candidate.stat().st_mode & 0o111:
            return candidate
    return None


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_into(tool: Path, root: Path, output: Path) -> bool:
    # A FRESH output directory each time. Reusing one would let the pipeline's own incremental
    # skip answer the question instead of the pipeline -- "identical because nothing was rebuilt"
    # is not the claim being tested.
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        [str(tool), "build", str(root), "-o", str(output), "--quiet"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(f"content_verify: building {root.name} failed:", file=sys.stderr)
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        return False
    return True


def tree_hashes(root: Path) -> dict[str, str]:
    # The pipeline's own bookkeeping is excluded: the manifest records absolute output paths and a
    # build ordering, so it differs between two runs by design and is not content.
    return {
        path.relative_to(root).as_posix(): sha256_of(path)
        for path in sorted(root.rglob("*"))
        if path.is_file() and not path.name.startswith(".cna-content")
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--cna-content", type=Path, default=None)
    parser.add_argument("--work-dir", type=Path, default=REPO / "build" / "content-verify")
    parser.add_argument("--hashes", action="store_true", help="print every output hash")
    parser.add_argument(
        "--selftest",
        action="store_true",
        help="corrupt one output byte and require the comparison to catch it",
    )
    args = parser.parse_args()

    tool = args.cna_content or find_cna_content()
    if tool is None:
        print(
            "content_verify: no cna-content found. Configure a build first, or pass "
            "--cna-content <path>.",
            file=sys.stderr,
        )
        return 1

    # Inside the build tree, never /tmp and never the session scratchpad (openeggbert build rule 3).
    work = args.work_dir
    work.mkdir(parents=True, exist_ok=True)

    problems: list[str] = []
    all_hashes: dict[str, str] = {}
    checked = 0

    for name in CNB_ROOTS:
        root = ASSETS_SRC / name
        if not root.is_dir():
            continue
        sources = [p for p in root.rglob("*") if p.is_file() and p.name != ".gitkeep"]
        if not sources:
            continue

        first = work / f"{name}-a"
        second = work / f"{name}-b"
        if not build_into(tool, root, first) or not build_into(tool, root, second):
            problems.append(f"{name}: the tree could not be built")
            continue

        if args.selftest:
            # A checker that has never been shown to FAIL is a checker that reports green forever.
            # One byte of one output is flipped in the second tree; the comparison below must
            # report it, and `--selftest` inverts the exit status so that catching it is success.
            victims = sorted(p for p in second.rglob("*") if p.is_file() and p.suffix == ".cnb")
            if victims:
                data = bytearray(victims[0].read_bytes())
                data[len(data) // 2] ^= 0xFF
                victims[0].write_bytes(bytes(data))

        left = tree_hashes(first)
        right = tree_hashes(second)
        checked += len(left)

        for relative in sorted(set(left) | set(right)):
            a = left.get(relative)
            b = right.get(relative)
            key = f"{name}/{relative}"
            if a is None:
                problems.append(f"{key}: produced by the second build only")
            elif b is None:
                problems.append(f"{key}: produced by the first build only")
            elif a != b:
                problems.append(f"{key}: NOT deterministic\n    run 1 {a}\n    run 2 {b}")
            else:
                all_hashes[key] = a

    shutil.rmtree(work, ignore_errors=True)

    if args.hashes:
        print("content_verify: output hashes (same-machine)")
        for key, digest in sorted(all_hashes.items()):
            hazard = KNOWN_CROSS_MACHINE_HAZARDS.get(key)
            suffix = f"   <- machine-dependent: {hazard}" if hazard else ""
            print(f"  {digest}  {key}{suffix}")

    if args.selftest:
        if problems:
            print(
                f"content_verify: selftest passed -- the comparison caught "
                f"{len(problems)} corrupted output(s)."
            )
            for problem in problems:
                print(f"  detected: {problem.splitlines()[0]}")
            return 0
        print(
            "content_verify: SELFTEST FAILED -- one output byte was flipped and the comparison "
            "did not notice. The check is not checking anything.",
            file=sys.stderr,
        )
        return 1

    if problems:
        print(f"content_verify: {len(problems)} problem(s):", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1

    print(
        f"content_verify: {checked} output file(s) are byte-identical across two builds "
        f"({len(CNB_ROOTS)} root(s) considered)."
    )
    if KNOWN_CROSS_MACHINE_HAZARDS:
        print("content_verify: known CROSS-machine hazards, which this check cannot see:")
        for key, reason in sorted(KNOWN_CROSS_MACHINE_HAZARDS.items()):
            print(f"  {key} -- {reason}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
