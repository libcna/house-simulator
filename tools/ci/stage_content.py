#!/usr/bin/env python3
"""stage_content.py -- the content tree a platform ships (`HOUSE-02850`).

    tools/ci/stage_content.py --content build/content --effects build/content-fx --out DIR
    tools/ci/stage_content.py ... --lod-level 1        # the Web's chunks only

The content build compiles every file under an `assets-src/` directory, including the offline
inputs the manifest marks `notPackaged` (ORM maps, unrigged character bases), the unshipped `dev`
pack's test fixtures and its own bookkeeping. This writes what a package or a Web preload actually carries: `DIR/content` and
`DIR/content-fx`, hard-linked where the filesystem allows so staging writes almost nothing, without
those files, and with `world/chunks.bin` reduced to one preset's LOD level when `--lod-level` is
given. A file is rewritten only when it changed, so a repeated stage is a no-op.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "ci"))
sys.path.insert(0, str(REPO / "tools" / "world"))

import budget_report  # noqa: E402
import build_chunks  # noqa: E402

BOOKKEEPING = {".cna-content.lock", ".cna-content-manifest.json", ".gitkeep"}


def unshipped(content: Path) -> set[Path]:
    """What the content build made that the manifest does not ship.

    Rows of a pack marked `shipped: false` (the `dev` fixtures of `--scene=content-smoke`) name
    their content. Rows marked `notPackaged` name none, so their output is found the way the build
    found the input: the source's path under `assets-src/` with `.cnb` for its suffix (`Media/`
    builds into the root).
    """
    manifest = json.loads((REPO / "assets-src" / "assets.manifest.json").read_text())
    kept_packs = {pack["id"] for pack in manifest.get("packs", []) if pack.get("shipped")}
    outputs: set[Path] = {path.resolve()
                          for row in manifest.get("assets", [])
                          if "notPackaged" not in row and row.get("residencyPack") not in kept_packs
                          for path in budget_report.compiled_paths(row, content, None)}
    for row in manifest.get("assets", []):
        source = Path(row.get("sourceFile", ""))
        if "notPackaged" not in row or source.parts[:1] != ("assets-src",) or len(source.parts) < 3:
            continue
        relative = Path(*source.parts[1:]).with_suffix(".cnb")
        if relative.parts[0] == "Media":
            relative = Path(*relative.parts[1:])
        outputs.add((content / relative).resolve())
    return outputs


def place(origin: Path, destination: Path) -> None:
    if destination.exists():
        if os.path.samefile(origin, destination):
            return
        destination.unlink()
    destination.parent.mkdir(parents=True, exist_ok=True)
    try:
        os.link(origin, destination)
    except OSError:
        shutil.copy2(origin, destination)


def write_if_changed(destination: Path, data: bytes) -> None:
    if destination.is_file() and not destination.is_symlink() and destination.read_bytes() == data:
        return
    if destination.exists():
        destination.unlink()
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)


def stage(content: Path, effects: Path | None, out: Path, lod_level: int | None) -> int:
    """Stages both trees under `out`; returns the number of files it holds."""
    skip = unshipped(content)
    count = 0
    for source, target in ((content, out / "content"), (effects, out / "content-fx")):
        if source is None or not source.is_dir():
            continue
        wanted: set[Path] = set()
        for path in sorted(p for p in source.rglob("*") if p.is_file()):
            if path.name in BOOKKEEPING or path.resolve() in skip:
                continue
            relative = path.relative_to(source)
            destination = target / relative
            wanted.add(destination)
            if lod_level is not None and source == content and relative == Path("world/chunks.bin"):
                write_if_changed(destination, build_chunks.select_level(path.read_bytes(), lod_level))
            else:
                place(path, destination)
            count += 1
        # A file the build no longer produces must not linger in what ships.
        if target.is_dir():
            for stale in [p for p in target.rglob("*") if p.is_file() and p not in wanted]:
                stale.unlink()
            for directory in sorted((p for p in target.rglob("*") if p.is_dir()), reverse=True):
                if not any(directory.iterdir()):
                    directory.rmdir()
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--content", type=Path, required=True)
    parser.add_argument("--effects", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--lod-level", type=int, choices=(0, 1, 2))
    arguments = parser.parse_args()
    files = stage(arguments.content.resolve(), arguments.effects.resolve() if arguments.effects else None,
                  arguments.out, arguments.lod_level)
    level = "every LOD level" if arguments.lod_level is None else f"LOD level {arguments.lod_level}"
    print(f"stage_content: {arguments.out} holds {files} files ({level})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
