#!/usr/bin/env python3
"""world_manifest.py -- write and check `world.manifest.json`, and the `worldHash` a save carries.

`HOUSE-00364`. The manifest is the index of the world: every member file with its SHA-256, and one
`worldHash` over the list. `WorldLoader` verifies it at load; this writes it, and `--check` is the
gate that stops it going stale.

    tools/world/world_manifest.py --emit content/world
    tools/world/world_manifest.py --check content/world

`deploy_world.py` calls `emit` for you as its last step, which is where this normally runs: the
manifest hashes the **deployed** bytes, because those are the ones `WorldLoader` reads back.
    tools/world/world_manifest.py --selftest

## The definition, exactly, because two implementations have to agree

A member's hash is `sha256:` followed by the SHA-256 of the file's **bytes**, lower-case hex. Not
of its parsed JSON: a reformatted file is a different file, the deployed copy is what the loader
reads, and "the bytes on disk" is the only definition a C++ reader and a Python writer can both
implement without agreeing on a JSON canonicalisation first.

`worldHash` is `sha256:` followed by the SHA-256 of

    for each member, in the order the manifest lists them:
        <file name> "\\n" <the member's sha256 field, including the "sha256:" prefix> "\\n"

encoded UTF-8. In the order given, so a manifest that lists the same files in another order is a
different world. That is deliberate: the list is part of the hash, and a member order that could
differ between two machines would make a save written on one look stale on the other. `build`
writes them in `cna-house.md` §15.1's order for the same reason -- never the filesystem's.

(§15.1's order is not `WorldLoader`'s read order, which is the dependency order. Nothing needs them
to be the same: the manifest is an index, and the loader opens what it likes in the order it
likes.)

## Why a save carries it

`cna-house.md` §65: a save is a delta against `initialstate.json`. `worldHash` is what tells the
loader the world moved under a save (`HOUSE-02317` decides what to do about it). It has to change
when **any** member changes and to be stable when none does, which is exactly what a hash over the
member list gives -- and it costs one hash of a few hundred bytes rather than a rehash of the
whole world.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_io  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

#: The manifest indexes the world files and not itself. `assets.manifest.json` is the ASSET
#: manifest (`cna-house.md` §20.3) and belongs to `check_manifest.py`.
MEMBER_KINDS = [k for k in layout_io.FILES if k not in ("manifest", "assets")]

SCHEMA = "cna-house/manifest/1"


def file_hash(path: Path) -> str:
    """`sha256:<64 lower-case hex>` over the file's bytes."""
    return "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()


def world_hash(members: list[dict]) -> str:
    """`sha256:` over `<file>\\n<sha256>\\n` for each member, in the order given."""
    joined = "".join(f"{member['file']}\n{member['sha256']}\n" for member in members)
    return "sha256:" + hashlib.sha256(joined.encode("utf-8")).hexdigest()


def build(directory: Path) -> dict:
    """The manifest a world directory implies.

    Members are listed in `layout_io.FILES` order -- the order `cna-house.md` §15.1 writes them --
    rather than in whatever order the filesystem returns, because the order is part of the hash and
    a manifest that reshuffled on a different machine would invalidate every save.
    """
    members = []
    for kind in MEMBER_KINDS:
        name, _ = layout_io.FILES[kind]
        path = directory / name
        if not path.is_file():
            continue
        members.append({"file": name, "sha256": file_hash(path)})
    return {"schema": SCHEMA, "worldHash": world_hash(members), "members": members}


def rendered(manifest: dict) -> str:
    return json.dumps(manifest, indent=2) + "\n"


def emit(directory: Path, *, dry_run: bool = False) -> tuple[bool, str]:
    """`(changed, reason)`. Writes `world.manifest.json` unless @p dry_run."""
    manifest = build(directory)
    path = directory / layout_io.FILES["manifest"][0]
    text = rendered(manifest)

    if not path.is_file():
        if not dry_run:
            path.write_text(text, encoding="utf-8")
        return True, "there was no manifest"

    current = path.read_text(encoding="utf-8")
    if current == text:
        return False, ""

    # Say WHAT moved, not just that something did. "the manifest is stale" sends an author to diff
    # a file of hashes; naming the member sends them to the file they changed.
    reason = "the manifest is stale"
    try:
        before = json.loads(current)
        was = {member["file"]: member["sha256"] for member in before.get("members", [])}
        now = {member["file"]: member["sha256"] for member in manifest["members"]}
        added = sorted(set(now) - set(was))
        removed = sorted(set(was) - set(now))
        changed = sorted(name for name in set(was) & set(now) if was[name] != now[name])
        parts = []
        if changed:
            parts.append("changed: " + ", ".join(changed))
        if added:
            parts.append("added: " + ", ".join(added))
        if removed:
            parts.append("removed: " + ", ".join(removed))
        if parts:
            reason = "; ".join(parts)
    except (json.JSONDecodeError, KeyError, TypeError):
        reason = "the existing manifest does not parse"

    if not dry_run:
        path.write_text(text, encoding="utf-8")
    return True, reason


# ======================================================================================= selftest


def selftest() -> int:
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("world_manifest: selftest")
    workspace = Path(tempfile.mkdtemp(prefix="world_manifest_selftest_"))
    try:
        world = workspace / "world"
        world.mkdir()
        (world / "layout.levels.json").write_text(
            '{"schema": "cna-house/levels/1", "levels": [], "construction": {}}\n',
            encoding="utf-8")
        (world / "layout.cells.json").write_text(
            '{"schema": "cna-house/cells/1", "cells": []}\n', encoding="utf-8")
        # A third file whose name sorts BEFORE the other two, so the order claim below is about
        # §15.1's order and not about an alphabetical one that happens to agree with it.
        (world / "layout.audio.json").write_text(
            '{"schema": "cna-house/audio/1", "zones": []}\n', encoding="utf-8")

        changed, reason = emit(world)
        require(changed and "no manifest" in reason, f"a world with no manifest gets one ({reason})")
        manifest = json.loads((world / "world.manifest.json").read_text(encoding="utf-8"))
        require([m["file"] for m in manifest["members"]]
                == ["layout.levels.json", "layout.cells.json", "layout.audio.json"],
                f"members are in §15.1's order, not the filesystem's and not alphabetical "
                f"({[m['file'] for m in manifest['members']]})")

        # 1. A hash of the BYTES. A reformatted file is a different file: the deployed copy is what
        #    the loader reads, and "the bytes on disk" is the only definition a C++ reader and a
        #    Python writer can both implement without first agreeing on a JSON canonicalisation.
        before = manifest["worldHash"]
        (world / "layout.cells.json").write_text(
            '{"schema": "cna-house/cells/1",  "cells": []}\n', encoding="utf-8")
        changed, reason = emit(world)
        require(changed and "layout.cells.json" in reason,
                f"one extra space in one file changes its hash, and the reason NAMES it ({reason})")
        after = json.loads((world / "world.manifest.json").read_text(encoding="utf-8"))
        require(after["worldHash"] != before,
                "and the worldHash moves with it, which is what a save compares")

        # 2. Stable when nothing moved. A hash that churned would make every save look stale.
        changed, _ = emit(world)
        require(not changed, "a second run over an unchanged world writes nothing")
        require(emit(world, dry_run=True)[0] is False, "and --check agrees")

        # 3. The order is part of the hash, because the load order is part of what the save was
        #    taken against.
        members = after["members"]
        require(world_hash(members) != world_hash(list(reversed(members))),
                "the same files in another order are a different worldHash")

        # 4. The worldHash is over the LIST, not over the contents, so it costs one hash of a few
        #    hundred bytes rather than a rehash of the world.
        require(world_hash([{"file": "a", "sha256": "sha256:" + "0" * 64}])
                == "sha256:" + hashlib.sha256(
                    ("a\nsha256:" + "0" * 64 + "\n").encode("utf-8")).hexdigest(),
                "worldHash is sha256 over `<file>\\n<sha256>\\n` per member, exactly")

        # 5. Adding and removing a file are both reported, and both move the hash.
        (world / "layout.portals.json").write_text(
            '{"schema": "cna-house/portals/1", "portals": []}\n', encoding="utf-8")
        changed, reason = emit(world)
        require(changed and "added: layout.portals.json" in reason,
                f"a new world file is added and named ({reason})")
        (world / "layout.portals.json").unlink()
        changed, reason = emit(world)
        require(changed and "removed: layout.portals.json" in reason,
                f"and a deleted one is reported as removed ({reason})")

        # 6. The manifest does not index itself, and the ASSET manifest is not its business.
        (world / "assets.manifest.json").write_text(
            '{"schema": "cna-house/assets/1", "assets": []}\n', encoding="utf-8")
        emit(world)
        listed = [m["file"] for m in
                  json.loads((world / "world.manifest.json").read_text(encoding="utf-8"))["members"]]
        require("world.manifest.json" not in listed, "the manifest does not list itself")
        require("assets.manifest.json" not in listed,
                "nor the ASSET manifest, which is §20.3's and check_manifest.py's")

        # 7. Every hash is the documented shape, or a C++ reader parsing it has nothing to match.
        require(all(m["sha256"].startswith("sha256:") and len(m["sha256"]) == 71
                    for m in json.loads(
                        (world / "world.manifest.json").read_text(encoding="utf-8"))["members"]),
                "every member hash is `sha256:` plus 64 hex characters")
    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        print(f"\nworld_manifest: {len(failures)} claim(s) FAILED")
        return 1
    print("world_manifest: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    # The DEPLOYED world, not the authored one: `WorldLoader::VerifyManifest` rehashes the files
    # it is about to read, and those are the copies `deploy_world.py` wrote (`HOUSE-00421`). A
    # manifest over the authored bytes would fail at load on every file that had a comment in it.
    parser.add_argument("directory", nargs="?", type=Path,
                        default=REPO / "content" / "world")
    parser.add_argument("--emit", action="store_true", help="write world.manifest.json")
    parser.add_argument("--check", action="store_true", help="fail if it is stale")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    # Not an error while the world is unauthored: `HOUSE-00366` writes the first file, and a gate
    # that failed until then would be a gate somebody turned off. Said out loud rather than
    # returning a silent zero, so "nothing to index" never reads as "the index is fine".
    if not args.directory.is_dir() or not build(args.directory)["members"]:
        print(f"world_manifest: no world files in {args.directory} yet "
              f"(HOUSE-00366 writes the first) -- nothing to index.")
        return 0

    changed, reason = emit(args.directory, dry_run=args.check)
    if args.check:
        if changed:
            print(f"world_manifest: {args.directory / 'world.manifest.json'} is stale -- {reason}. "
                  f"Run tools/world/world_manifest.py --emit", file=sys.stderr)
            return 1
        print(f"world_manifest: {args.directory / 'world.manifest.json'} matches the world.")
        return 0

    print(f"world_manifest: {'wrote' if changed else 'unchanged'} "
          f"{args.directory / 'world.manifest.json'}" + (f" -- {reason}" if reason else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
