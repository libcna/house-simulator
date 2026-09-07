#!/usr/bin/env python3
"""check_manifest.py -- no unlisted file under `assets-src/`, and no hash mismatch.

`HOUSE-00196`, enforcing `cna-house.md` §20.1: **no row, no build**. A file that reaches
`assets-src/` without a manifest row is a file whose licence nobody has looked at, and the only
moment anyone reliably looks is when a gate refuses to build.

    tools/ci/check_manifest.py            # check everything
    tools/ci/check_manifest.py --list     # show what is exempt and why

Exit status: 0 clean, 1 at least one problem.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "assets"))
import manifest as manifest_tool  # noqa: E402  (after the path insert, deliberately)

REPO = Path(__file__).resolve().parents[2]
ASSETS_SRC = REPO / "assets-src"

#: Files under `assets-src/` that are NOT assets, each with the reason it is exempt.
#:
#: **Every entry is an exact name or an exact rule, never a wildcard over a whole extension.** A
#: blanket `*.json` exemption would silently exempt a future `world/*.json`, which very much is an
#: asset. The point of writing the reason beside each one is that adding an entry has to be an
#: argument rather than a convenience.
EXEMPT_NAMES = {
    "README.md": "documentation, not an asset",
    ".cna-content.json": "cna-content configuration authored in this repository",
    ".gitkeep": "a placeholder that keeps an empty directory in git",
    "COMPILER.txt": "a provenance record this repository generates (HOUSE-00184)",
    "assets.manifest.json": "the manifest itself; it cannot list itself",
    "SOURCE.md": "the directory's own provenance record (HOUSE-00261); required BY "
                 "verify_licences.py wherever a downloaded asset lives, so it is enforced rather "
                 "than merely exempt",
}


def is_generated_effect_baseline(path: Path) -> bool:
    """`assets-src/Effects/<name>.xnb` beside `<name>.fx`.

    Exempt because its provenance IS the `.fx`'s: it is produced from a source that has a row, by a
    tool in this repository, and `tools/effects/build_effects.sh --check` already fails the build if
    the two disagree. Giving it a row of its own would mean a second hash to update on every effect
    change, which is a rule people work around rather than follow.
    """
    return path.suffix == ".xnb" and path.with_suffix(".fx").is_file()


def exemption_for(path: Path) -> str | None:
    if path.name in EXEMPT_NAMES:
        return EXEMPT_NAMES[path.name]
    if is_generated_effect_baseline(path):
        return "generated from the .fx beside it, which has a row (HOUSE-00185)"
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--list", action="store_true", help="show every file and its status")
    args = parser.parse_args()

    if not ASSETS_SRC.is_dir():
        print("check_manifest: assets-src/ does not exist", file=sys.stderr)
        return 1

    document = manifest_tool.load()
    problems = manifest_tool.validate(document)

    listed = {row.get("sourceFile", "") for row in document.get("assets", [])}
    on_disk = sorted(p for p in ASSETS_SRC.rglob("*") if p.is_file())

    unlisted: list[Path] = []
    exempt: list[tuple[Path, str]] = []
    for path in on_disk:
        relative = path.relative_to(REPO).as_posix()
        if relative in listed:
            continue
        reason = exemption_for(path)
        if reason is not None:
            exempt.append((path, reason))
        else:
            unlisted.append(path)

    # A row pointing at a file that has been deleted is the mirror image of an unlisted file, and it
    # is just as wrong: the manifest would keep asserting a licence for something that is gone.
    missing = sorted(f for f in listed if not (REPO / f).is_file())

    if args.list:
        for path in on_disk:
            relative = path.relative_to(REPO).as_posix()
            if relative in listed:
                print(f"  listed  {relative}")
            elif (reason := exemption_for(path)) is not None:
                print(f"  exempt  {relative}  -- {reason}")
            else:
                print(f"  UNLISTED {relative}")

    for path in unlisted:
        problems.append(
            f"{path.relative_to(REPO).as_posix()}: no manifest row. "
            f"Add one with tools/assets/manifest.py add, or explain the exemption in "
            f"tools/ci/check_manifest.py."
        )
    for relative in missing:
        problems.append(f"{relative}: has a manifest row but does not exist on disk")

    if problems:
        print(f"check_manifest: {len(problems)} problem(s):", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1

    print(
        f"check_manifest: clean -- {len(listed)} listed, {len(exempt)} exempt, "
        f"{len(on_disk)} file(s) under assets-src/."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
