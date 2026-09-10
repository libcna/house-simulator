#!/usr/bin/env python3
"""deploy_world.py -- the authored JSONC world, as plain JSON the runtime can read.

`HOUSE-00421`. `docs/world-format.md` describes both halves of the format and, until this existed,
only one of them was built:

> The authored files are JSONC (comments permitted); the build strips comments and deploys plain
> JSON beside the compiled content, where the runtime reads them with `System::IO::File` +
> `System::Text::Json`.

`System::Text::Json` refuses the first comment, so `WorldLoader` could not read a file anybody had
authored. This is the stripping step.

    tools/world/deploy_world.py                       # assets-src/world -> content/world
    tools/world/deploy_world.py --check
    tools/world/deploy_world.py --selftest

## Comments are stripped, not reformatted

The output is the parsed document written back with a stable two-space indent, **not** the source
with the comment characters blanked. Two reasons, and the second is the one that matters:

* a blanked comment leaves the whitespace it occupied, so the deployed file carries the shape of a
  comment that is no longer there;
* the deployed file is what `WorldLoader` reads, and rewriting it through a parser is what proves
  the authored file *parses at all* before it reaches the game. A deploy that copied bytes would
  ship a syntax error and let the runtime find it.

The **parsed content** is identical -- `--check` compares parsed documents, not text -- so nothing
about the value of a field depends on how it was laid out.

## Why this is a deploy and not a compile

The world files are data the *game* owns, not framework content: no `ContentTypeReader` is
registered and no CNA pipeline extension is needed (`cna-house.md` §15.1). Compiling them into
`.cnb` would put a CNA-owned format between the author and the loader for no gain, and would make
`world.manifest.json`'s hashes -- which `WorldLoader` verifies at load -- describe files nothing
reads.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_io  # noqa: E402
import world_manifest  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "assets-src" / "world"
TARGET = REPO / "content" / "world"

#: The authored world files. `world.manifest.json` is **not** among them: it is written here, over
#: what was just deployed, and never authored.
#:
#: That is `HOUSE-00421`'s correction to `HOUSE-00364`. `WorldLoader::VerifyManifest` rehashes the
#: files it is about to read, and after this step those are the *deployed* bytes -- a manifest over
#: the authored bytes would fail at load on every file that had a comment in it, which is every
#: file. So the manifest describes the deployed copy, is generated rather than authored, and lives
#: only beside the files it describes.
DEPLOYED_KINDS = [k for k in layout_io.FILES if k not in ("assets", "manifest")]


def rendered(document: dict) -> str:
    """The document as plain JSON: two-space indent, keys in the order the source wrote them.

    Not sorted. `world.manifest.json` lists its members in an order that is part of a hash
    (`HOUSE-00364`), and a writer that sorted keys here would be one more thing that could reorder
    something load-bearing without saying so.
    """
    return json.dumps(document, indent=2, ensure_ascii=False) + "\n"


def deploy(source: Path, target: Path, *, dry_run: bool = False) -> tuple[list[str], list[str]]:
    """`(written, problems)`. Strips comments from every world file present in @p source."""
    written: list[str] = []
    problems: list[str] = []
    if not source.is_dir():
        return written, problems

    for kind in DEPLOYED_KINDS:
        name, _ = layout_io.FILES[kind]
        path = source / name
        if not path.is_file():
            continue
        try:
            document = layout_io.load_file(path, kind)
        except layout_io.LayoutError as error:
            # Reported rather than raised, so one broken file does not hide the state of the other
            # fifteen -- the same reason `validate_world.py` reports every rule.
            problems.append(f"{name}: {error}")
            continue

        text = rendered(document)
        out = target / name
        if out.is_file() and out.read_text(encoding="utf-8") == text:
            continue
        written.append(name)
        if not dry_run:
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_text(text, encoding="utf-8")

    # The two binary companions, copied verbatim. There is nothing to strip from a PNG, and
    # `layout.exterior.json` names them as paths into this directory exactly as it names nothing
    # else -- so a deploy that left them behind would ship an exterior pointing at a height field
    # that is not there (`HOUSE-00761`).
    for name in world_manifest.COMPANION_FILES:
        path = source / name
        if not path.is_file():
            continue
        data = path.read_bytes()
        out = target / name
        if out.is_file() and out.read_bytes() == data:
            continue
        written.append(name)
        if not dry_run:
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_bytes(data)

    # The manifest, over what was just written. Last, because it hashes the deployed bytes; and
    # here rather than in a separate stage, because a manifest that described a different set of
    # bytes than the one beside it is the exact failure this whole step exists to avoid.
    if not dry_run and target.is_dir():
        changed, _ = world_manifest.emit(target)
        if changed:
            written.append(layout_io.FILES["manifest"][0])
    elif dry_run and target.is_dir():
        changed, _ = world_manifest.emit(target, dry_run=True)
        if changed:
            written.append(layout_io.FILES["manifest"][0])

    # A file the source no longer has must not survive in the deployed copy: the manifest would
    # not list it, `WorldLoader` would not read it, and the next person to look would find a room
    # that exists in the game's directory and nowhere else.
    if target.is_dir():
        # The manifest is written by this step and has no source, so it is expected without one.
        manifest_name = layout_io.FILES["manifest"][0]
        expected = {layout_io.FILES[k][0] for k in DEPLOYED_KINDS}
        expected.update(world_manifest.COMPANION_FILES)
        stale_candidates = sorted(target.glob("*.json")) + [
            target / name for name in world_manifest.COMPANION_FILES if (target / name).is_file()]
        for stale in stale_candidates:
            if stale.name == manifest_name:
                continue
            # A DOTFILE in the deployed tree belongs to whoever writes it and not to this step
            # (`HOUSE-00422`). `pathlib.Path.glob` matches hidden names where a shell glob would
            # not, so `content/world/.cna-content-manifest.json` -- the CNB pipeline's index,
            # written by the `cnb-world` stage that runs AFTER this one -- was picked up as a world
            # file that had lost its source and reported for deletion. Deleting it would have
            # broken the very stage that wrote it, and the two stages would have taken turns
            # undoing each other on every build.
            if stale.name.startswith("."):
                continue
            if stale.name in expected and (source / stale.name).is_file():
                continue
            problems.append(f"{stale.name}: deployed and no longer authored; delete it")

    return written, problems


# ======================================================================================= selftest


def selftest() -> int:
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("deploy_world: selftest")
    workspace = Path(tempfile.mkdtemp(prefix="deploy_world_selftest_"))
    try:
        source = workspace / "src"
        target = workspace / "out"
        source.mkdir()

        authored = (
            "{\n"
            "  // The five levels. This comment is the whole point of the exercise.\n"
            '  "schema": "cna-house/levels/1",\n'
            '  "levels": [\n'
            '    { "id": "L0", "ffl": 0.60, "ceiling": 3.30 }  // the ground floor\n'
            "  ],\n"
            '  "construction": { "wallExterior": 0.30 }\n'
            "}\n")
        (source / "layout.levels.json").write_text(authored, encoding="utf-8")

        written, problems = deploy(source, target)
        require(written == ["layout.levels.json", "world.manifest.json"] and not problems,
                f"the authored file is deployed, and a manifest written beside it "
                f"({written}, {problems})")

        deployed = (target / "layout.levels.json").read_text(encoding="utf-8")
        require("//" not in deployed, "and the comments are gone")

        # 1. `System::Text::Json` is a strict JSON parser. `json.loads` is the same test: if this
        #    passes and the runtime still cannot read it, the difference is a bug in one of them
        #    and not in the deploy.
        try:
            parsed = json.loads(deployed)
            strict = True
        except json.JSONDecodeError as error:
            parsed, strict = None, False
            print(f"        {error}")
        require(strict, "the deployed file is strict JSON, which is what the runtime parses")
        require(parsed == layout_io.load_file(source / "layout.levels.json", "levels"),
                "and its parsed content is identical to the authored file's")

        # 2. Stable: a second deploy of an unchanged world writes nothing. A deploy that rewrote
        #    every file every time would make `--check` useless and every build dirty.
        written, _ = deploy(source, target)
        require(not written, "a second deploy over an unchanged world writes nothing")
        require(deploy(source, target, dry_run=True)[0] == [], "and --check agrees")

        # 3. A change to the source is a change to the deployed copy.
        (source / "layout.levels.json").write_text(
            authored.replace("0.30", "0.25"), encoding="utf-8")
        written, _ = deploy(source, target, dry_run=True)
        require(written[:1] == ["layout.levels.json"],
                f"--check sees a changed source before anything is written ({written})")

        # 4. A file that no longer exists must not survive in the deployed copy: the manifest would
        #    not list it and the loader would not read it, so it would be a room that exists in the
        #    game's directory and nowhere else.
        (target / "layout.portals.json").write_text('{"schema": "cna-house/portals/1"}\n',
                                                    encoding="utf-8")
        _, problems = deploy(source, target)
        require(any("no longer authored" in problem for problem in problems),
                f"a deployed file with no source is reported ({problems})")
        (target / "layout.portals.json").unlink()

        # 4b. ...but a DOTFILE is somebody else's (`HOUSE-00422`). The CNB pipeline writes
        #     `.cna-content-manifest.json` into the same directory from a stage that runs after
        #     this one, and `pathlib`'s glob -- unlike a shell's -- matches hidden names, so it was
        #     reported as a world file that had lost its source. Reporting it was harmless only
        #     until somebody obeyed: deleting it breaks the stage that wrote it, and the two would
        #     then undo each other on every build.
        (target / ".cna-content-manifest.json").write_text('{"entries": []}\n', encoding="utf-8")
        _, problems = deploy(source, target)
        require(not any("cna-content-manifest" in problem for problem in problems),
                f"another tool's dotfile in the deployed tree is not this step's to delete "
                f"({problems})")
        require(not problems, f"...and nothing else was reported either ({problems})")
        (target / ".cna-content-manifest.json").unlink()

        # 5. A broken source file is reported and does not stop the rest. One typo hiding the state
        #    of the other fifteen files is the failure mode `validate_world.py` also refuses.
        (source / "layout.cells.json").write_text('{"schema": "cna-house/cells/1", "cells": [',
                                                  encoding="utf-8")
        written, problems = deploy(source, target)
        require(any("layout.cells.json" in problem for problem in problems),
                f"a source that does not parse is reported by name ({problems})")
        require((target / "layout.levels.json").is_file(),
                "and the files that do parse are still deployed -- one typo must not hide the "
                "state of the other fifteen")

        # 6. The manifest is written HERE, over the deployed bytes, and never authored.
        #    `WorldLoader::VerifyManifest` rehashes the files it is about to read; a manifest over
        #    the authored bytes would fail at load on every file that had a comment in it, which is
        #    every file. This is `HOUSE-00421`'s correction to `HOUSE-00364`.
        (source / "layout.cells.json").unlink()
        (target / "layout.cells.json").unlink(missing_ok=True)
        deploy(source, target)
        manifest_path = target / "world.manifest.json"
        require(manifest_path.is_file(), "a manifest is written beside the deployed files")
        require(not (source / "world.manifest.json").is_file(),
                "and none is authored: it is generated data, not a source")

        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        listed = {row["file"]: row["sha256"] for row in manifest["members"]}
        require(listed.keys() == {"layout.levels.json"},
                f"it lists what was deployed ({sorted(listed)})")
        require(listed["layout.levels.json"]
                == world_manifest.file_hash(target / "layout.levels.json"),
                "and hashes the DEPLOYED bytes, which are the ones the loader reads")
        require(listed["layout.levels.json"]
                != world_manifest.file_hash(source / "layout.levels.json"),
                "not the authored ones -- they differ by exactly the comments that were stripped")

        # 7. The binary companions. `layout.exterior.json` points the runtime at `terrain.png` in
        #    this directory, so a deploy that only copied JSON would ship an exterior naming a
        #    height field that is not there (`HOUSE-00761`).
        blob = b"\x89PNG\r\n\x1a\n" + bytes(range(64))
        (source / "terrain.png").write_bytes(blob)
        written, _ = deploy(source, target)
        landed = target / "terrain.png"
        require("terrain.png" in written and landed.is_file() and landed.read_bytes() == blob,
                f"a companion PNG is deployed byte for byte ({written})")
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        names = [row["file"] for row in manifest["members"]]
        require(names == ["layout.levels.json", "terrain.png"],
                f"the manifest covers it, after the JSON ({names})")
        require([row for row in manifest["members"] if row["file"] == "terrain.png"][0]["sha256"]
                == world_manifest.file_hash(target / "terrain.png"),
                "with the hash of the deployed bytes, so a swapped ground fails at load")
        require(not deploy(source, target)[0],
                "and an unchanged companion is not rewritten every deploy")

        # ...and the stale rule applies to it too, which the `*.json` glob alone would miss.
        (source / "terrain.png").unlink()
        _, problems = deploy(source, target)
        require(any("terrain.png" in problem for problem in problems),
                f"a deployed companion with no source is reported ({problems})")
    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        print(f"\ndeploy_world: {len(failures)} claim(s) FAILED")
        return 1
    print("deploy_world: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--source", type=Path, default=SOURCE)
    parser.add_argument("--target", type=Path, default=TARGET)
    parser.add_argument("--check", action="store_true",
                        help="fail if the deployed copy is stale; write nothing")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    if not args.source.is_dir() or not any(
            (args.source / layout_io.FILES[k][0]).is_file() for k in DEPLOYED_KINDS):  # noqa: SIM102
        print(f"deploy_world: no world files in {args.source} yet -- nothing to deploy.")
        return 0

    written, problems = deploy(args.source, args.target, dry_run=args.check)
    for problem in problems:
        print(f"deploy_world: {problem}", file=sys.stderr)
    if problems:
        return 1

    if args.check:
        if written:
            print(f"deploy_world: {args.target} is stale ({', '.join(written)}). "
                  f"Run tools/world/deploy_world.py", file=sys.stderr)
            return 1
        print(f"deploy_world: {args.target} matches {args.source}.")
        return 0

    print(f"deploy_world: {len(written)} file(s) written to {args.target}"
          + (f" -- {', '.join(written)}" if written else " (already current)"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
