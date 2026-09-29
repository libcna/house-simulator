#!/usr/bin/env python3
"""web_budget.py -- the Web download against its two limits (`HOUSE-02850`).

    tools/ci/web_budget.py --content build-consumer/web-stage/content \
        --effects build-consumer/web-stage/content-fx --program build-consumer/cna-house.{html,js,wasm} \
        --data build-consumer/cna-house.data
    tools/ci/web_budget.py --selftest

`cna-house.md` §71.4 and §80.1: every pack at most 60 MB and the whole Web tier at most 180 MB,
both as the browser downloads them -- gzip-compressed, level 6 as a web server serves it. Each file
of the tree the Web build preloads is compressed on its own and attributed to a residency pack: a
compiled asset through its manifest row, `world/chunks.bin` chunk by chunk through its cell's pack,
and the rest of `world/`, the credits and the program itself (`--program`: page, script, wasm) to
`core`, which every session loads first. A file nothing claims is
counted in `(other)` and named, never dropped. Exit status 1 when either limit is exceeded.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import sys
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "ci"))
sys.path.insert(0, str(REPO / "tools" / "world"))

import budget_report  # noqa: E402
import build_chunks  # noqa: E402

PACK_LIMIT = 60_000_000
TOTAL_LIMIT = 180_000_000
LEVEL = 6
WORLD_PACK = "core"


def compressed(data: bytes) -> int:
    return len(zlib.compress(data, LEVEL))


def chunk_bytes_by_pack(chunks_file: Path, cells_file: Path) -> dict[str, int]:
    """`chunks.bin` split by residency pack, each part serialised and compressed on its own."""
    library = build_chunks.read_back(chunks_file.read_bytes())
    cells = {cell["id"]: cell for cell in json.loads(cells_file.read_text())["cells"]}
    by_pack: dict[str, list[dict]] = {}
    for chunk in library["chunks"]:
        pack = cells.get(chunk["cell"], {}).get("residencyPack") or WORLD_PACK
        by_pack.setdefault(pack, []).append(chunk)
    return {pack: compressed(build_chunks.serialise({**library, "chunks": chunks}))
            for pack, chunks in by_pack.items()}


def measure(content: Path, effects: Path | None) -> tuple[dict[str, int], list[str]]:
    manifest = json.loads((REPO / "assets-src" / "assets.manifest.json").read_text())
    owner: dict[Path, str] = {}
    for row in manifest.get("assets", []):
        if "notPackaged" in row:
            continue
        for path in budget_report.compiled_paths(row, content, effects):
            owner[path.resolve()] = row.get("residencyPack", "") or "(other)"

    packs: dict[str, int] = {}
    unclaimed: list[str] = []
    roots = [content] + ([effects] if effects is not None and effects.is_dir() else [])
    for root in roots:
        for path in sorted(p for p in root.rglob("*") if p.is_file()):
            relative = path.relative_to(root)
            if path.name in {".cna-content.lock", ".cna-content-manifest.json", ".gitkeep"}:
                continue
            if relative.parts[0] == "world" and root == content:
                if path.name == "chunks.bin":
                    for pack, size in chunk_bytes_by_pack(path, content / "world" / "layout.cells.json").items():
                        packs[pack] = packs.get(pack, 0) + size
                    continue
                pack = WORLD_PACK
            elif relative.parts[0] == "Credits":
                pack = WORLD_PACK
            else:
                pack = owner.get(path.resolve())
                if pack is None:
                    pack = "(other)"
                    unclaimed.append(str(relative))
            packs[pack] = packs.get(pack, 0) + compressed(path.read_bytes())
    return packs, unclaimed


def verdict(packs: dict[str, int], total: int | None = None) -> list[str]:
    problems = [f"pack {pack}: {size / 1e6:.1f} MB compressed against {PACK_LIMIT / 1e6:.0f} MB"
                for pack, size in sorted(packs.items()) if size > PACK_LIMIT]
    total = sum(packs.values()) if total is None else total
    if total > TOTAL_LIMIT:
        problems.append(f"total: {total / 1e6:.1f} MB compressed against {TOTAL_LIMIT / 1e6:.0f} MB")
    return problems


def selftest() -> int:
    ok = verdict({"core": PACK_LIMIT, "house-l0": PACK_LIMIT}) == []
    ok &= verdict({"core": PACK_LIMIT + 1}) != []
    ok &= verdict({pack: 50_000_000 for pack in ("a", "b", "c", "d")}) != []
    four = {pack: 50_000_000 for pack in ("a", "b", "c", "d")}
    ok &= verdict({"core": 1}, TOTAL_LIMIT + 1) != [] and verdict(four, TOTAL_LIMIT) == []
    ok &= compressed(b"\0" * 100_000) < 1_000
    print(f"web_budget: selftest {'passed' if ok else 'FAILED'}")
    return 0 if ok else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--content", type=Path, help="the content tree the Web build preloads")
    parser.add_argument("--effects", type=Path, help="its content-fx tree")
    parser.add_argument("--program", type=Path, nargs="*", default=[],
                        help="the page, script and wasm, counted in core: they are downloaded too")
    parser.add_argument("--data", type=Path,
                        help="the packaged preload itself: the total is then measured on it, "
                             "compressed whole, rather than summed file by file")
    parser.add_argument("--selftest", action="store_true")
    arguments = parser.parse_args()
    if arguments.selftest:
        return selftest()
    if arguments.content is None or not arguments.content.is_dir():
        parser.error("--content must name a built content tree")

    packs, unclaimed = measure(arguments.content, arguments.effects)
    programs = sum(compressed(program.read_bytes()) for program in arguments.program)
    packs[WORLD_PACK] = packs.get(WORLD_PACK, 0) + programs
    # One file compresses a little worse than its parts did; the download is the file.
    total = compressed(arguments.data.read_bytes()) + programs if arguments.data else None
    for pack, size in sorted(packs.items(), key=lambda item: -item[1]):
        print(f"web_budget: {pack:16} {size / 1e6:7.1f} MB")
    measured = sum(packs.values()) if total is None else total
    print(f"web_budget: {'total':16} {measured / 1e6:7.1f} MB compressed "
          f"(limits {PACK_LIMIT / 1e6:.0f} MB a pack, {TOTAL_LIMIT / 1e6:.0f} MB in all)")
    for name in unclaimed:
        print(f"web_budget: unclaimed by any manifest row: {name}")
    problems = verdict(packs, total)
    for problem in problems:
        print(f"web_budget: OVER -- {problem}", file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
