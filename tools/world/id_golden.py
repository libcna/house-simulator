#!/usr/bin/env python3
"""id_golden.py -- the golden list of every id in the world data.

`HOUSE-00399`, for the ID-stability test `cna-house.md` §70.2 asks for: *"a golden list of every
id in the world data -- adding is fine, renaming fails"*. `HOUSE-02582` is the test; this is the
list and the tool that maintains it.

    tools/world/id_golden.py --emit      # record ids the world has and the list does not
    tools/world/id_golden.py --check
    tools/world/id_golden.py --selftest

## Why an id is worth a gate of its own

An id is the only durable name anything in this house has. A save file records the cells the player
has been in, the light groups that are on, the interactables it has changed and the props it has
moved, all by id (§68). Rename `L1_MASTER_BED` to `L1_BED_MASTER` and every save ever written
refers to a room that no longer exists -- and nothing else in the build notices, because the layout
is internally consistent either way: the new name resolves everywhere the old one did. That is the
defect this list exists to catch, and it is invisible to all eleven of §15.7's rules.

## Append-only, and that is the whole design

`--emit` **adds and never removes**. A rename shows up as one id gone and one id arrived, and only
the arrival is automatic: the departure stays in the file and keeps failing `--check` until a
person deletes the line and says why in the commit. If `--emit` regenerated the file the gate would
launder exactly the change it exists to catch.

So the two directions are not symmetric, deliberately:

* an id in the list and **not** in the world is an error -- renamed, deleted, or a typo;
* an id in the world and **not** in the list is also an error, but one `--emit` fixes, because a
  list that lags the data is not a list.

## What counts as an id

Every object anywhere in the sixteen world files that has an `id` field matching
`docs/conventions.md`'s `^[A-Z][A-Z0-9_]*$`, found by walking the documents rather than by naming
the arrays here -- a new array of things with ids is exactly the case a hand-maintained list of
kinds would miss. Plus two sets that are ids without being rows: the **light groups**, which exist
only as references from cells and fixtures (§70.2 names them), and the **asset ids** of
`assets.manifest.json`, which the task names and which a content name change would otherwise break
silently.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_io  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "assets-src" / "world"
GOLDEN = REPO / "tests" / "unit" / "reference" / "world-ids.golden.txt"

#: `docs/conventions.md`. A value that is not id-shaped is not an id, whatever it is called.
ID = re.compile(r"^[A-Z][A-Z0-9_]*$")

HEADER = """\
# Every id in the world data. `HOUSE-00399`; the test that reads it is `HOUSE-02582`.
#
# APPEND-ONLY. `tools/world/id_golden.py --emit` adds ids the world has gained and never removes
# one. A line here whose id is no longer in the world FAILS the gate, because that is what a rename
# looks like and a rename breaks every save ever written (`cna-house.md` §68, §70.2).
#
# To retire an id, delete its line by hand and say why in the commit message.
#
# Format: <kind><TAB><id>, sorted by id. The kind is where the id was found and is informational:
# moving a row between files is not a rename, so a changed kind is corrected by --emit.
"""


def light_groups(layout: dict) -> set[str]:
    """The ids that are only ever referenced, never declared as a row.

    A light group is a name a cell lists and a fixture claims; nothing in the sixteen files has a
    `{"id": "LG_..."}` row for it. §70.2 names light groups in the golden list all the same, and it
    is right to: a switch's state field IS the group id, so renaming one silently changes what a
    saved switch state means.
    """
    found = set()
    for cell in layout_io.rows(layout, "cells") if "cells" in layout else []:
        for group in cell.get("lightGroups") or []:
            if isinstance(group, str) and ID.match(group):
                found.add(group)
    for light in layout_io.rows(layout, "lights") if "lights" in layout else []:
        group = light.get("group")
        if isinstance(group, str) and ID.match(group):
            found.add(group)
    return found


def walk(node, kind: str, out: dict[str, str]) -> None:
    """Every `{"id": ...}` object under @p node, keyed by id, valued by the array it was found in."""
    if isinstance(node, dict):
        identifier = node.get("id")
        if isinstance(identifier, str) and ID.match(identifier):
            out.setdefault(identifier, kind)
        for key, value in node.items():
            if isinstance(value, (dict, list)):
                walk(value, f"{kind}/{key}" if kind else key, out)
    elif isinstance(node, list):
        for item in node:
            walk(item, kind, out)


def collect(directory: Path) -> dict[str, str]:
    """`{id: kind}` for the world in @p directory, plus the light groups and the asset ids."""
    layout = layout_io.load_layout(directory)
    found: dict[str, str] = {}
    for kind, document in sorted(layout.items()):
        if kind == "assets":
            continue  # named separately below, because it is not a world file
        walk(document, kind, found)
    for group in light_groups(layout):
        found.setdefault(group, "lights/groups")
    # `assets.manifest.json` is one of the sixteen but lives beside the world directory rather
    # than in it, so `load_layout` does not return it and it is read by name here.
    manifest = directory.parent / layout_io.FILES["assets"][0]
    if manifest.is_file():
        for row in (layout_io.load_file(manifest, "assets") or {}).get("assets") or []:
            identifier = row.get("id")
            if isinstance(identifier, str) and ID.match(identifier):
                found.setdefault(identifier, "assets")
    return found


def read_golden(path: Path) -> dict[str, str]:
    if not path.is_file():
        return {}
    recorded = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        kind, _, identifier = line.partition("\t")
        if identifier:
            recorded[identifier.strip()] = kind.strip()
    return recorded


def rendered(entries: dict[str, str]) -> str:
    body = "".join(f"{kind}\t{identifier}\n"
                   for identifier, kind in sorted(entries.items()))
    return HEADER + "\n" + body


def compare(world: dict[str, str], recorded: dict[str, str]) -> tuple[list[str], list[str]]:
    """`(gone, unrecorded)` -- the ids the list has and the world lost, and the other way round."""
    return (sorted(set(recorded) - set(world)), sorted(set(world) - set(recorded)))


def merged(recorded: dict[str, str], world: dict[str, str]) -> dict[str, str]:
    """What `--emit` writes: every id the world has, **plus** every id the list already had.

    One function rather than a few lines inside `main`, because the selftest has to be able to call
    the same code the tool runs. A claim that re-implemented the merge would pass while `--emit`
    regenerated the file, which is the one failure that would make the whole list worthless.
    """
    out = dict(recorded)
    out.update(world)   # a changed kind is not a rename, so the world's answer wins for it
    return out


# ======================================================================================= selftest


def selftest() -> int:
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("id_golden: selftest")
    workspace = Path(tempfile.mkdtemp(prefix="id_golden_selftest_"))
    try:
        # 1. The walk finds ids at any depth and refuses anything not id-shaped, because a lowercase
        #    `kind` or a `roomTone` string is not an id and a list that carried them would fail on
        #    the day somebody renamed a *material class*.
        found: dict[str, str] = {}
        walk({"cells": [{"id": "L0_HALL", "kind": "corridor",
                         "acoustic": {"id": "not an id", "roomTone": "AMB_HALL"},
                         "nested": [{"id": "L0_HALL_SUB"}]}]}, "cells", found)
        require(set(found) == {"L0_HALL", "L0_HALL_SUB"},
                f"the walk finds nested ids and only ids ({sorted(found)})")
        require(found["L0_HALL_SUB"] == "cells/cells/nested",
                f"and records where it found each one ({found['L0_HALL_SUB']})")

        # 2. The two directions are NOT symmetric. This is the whole design.
        gone, unrecorded = compare({"A_NEW": "cells", "B_KEPT": "cells"},
                                   {"B_KEPT": "cells", "C_OLD": "cells"})
        require(gone == ["C_OLD"] and unrecorded == ["A_NEW"],
                f"a rename reads as one id gone and one arrived ({gone}, {unrecorded})")

        # 3. `--emit` adds and never removes: a renamed id keeps failing until a person deletes it.
        golden = workspace / "golden.txt"
        golden.write_text(rendered({"C_OLD": "cells", "B_KEPT": "cells"}), encoding="utf-8")
        after_emit = merged(read_golden(golden), {"A_NEW": "cells", "B_KEPT": "cells"})
        golden.write_text(rendered(after_emit), encoding="utf-8")
        after = read_golden(golden)
        require("C_OLD" in after and "A_NEW" in after,
                f"--emit records the new id and leaves the old one to fail ({sorted(after)})")

        # 4. A round trip through the file is lossless, or the gate diffs itself.
        require(after == after_emit, "the file reads back exactly as it was written")

        # 5. Over the real world: every kind of thing the task names is actually in there. A golden
        #    list that quietly held only cells would pass every check and protect nothing.
        if not (SOURCE / "layout.cells.json").is_file():
            require(False, "the authored world is there to collect from")
        else:
            world = collect(SOURCE)
            for label, sample in (("a cell", "L0_FOYER"), ("a portal", "P_L0_HALL__L0_KITCHEN"),
                                  ("an interactable", "SWITCH_B1_CELLAR"),
                                  ("a light fixture", None), ("a light group", None),
                                  ("an asset", None)):
                if sample is not None:
                    require(sample in world, f"{label} is in the list ({sample})")
            require(any(kind == "lights/groups" for kind in world.values()),
                    "a light group is in the list, though no row declares one")
            require(any(kind == "lights/lights" for kind in world.values()),
                    "and a light fixture, which is a row")
            require(any(kind == "assets" for kind in world.values()),
                    "and an asset id, which is not in a world file at all")
            require(all(ID.match(identifier) for identifier in world),
                    "every id in the list is id-shaped")
            require(len(world) > 1000, f"the list is the whole house, not a sample ({len(world)})")
    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        print(f"\nid_golden: {len(failures)} claim(s) FAILED")
        return 1
    print("id_golden: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", nargs="?", type=Path, default=SOURCE)
    parser.add_argument("--golden", type=Path, default=GOLDEN)
    parser.add_argument("--emit", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if not (args.directory / "layout.cells.json").is_file():
        print(f"id_golden: no world in {args.directory} yet -- nothing to record.")
        return 0

    world = collect(args.directory)
    recorded = read_golden(args.golden)
    gone, unrecorded = compare(world, recorded)

    if args.emit and unrecorded:
        args.golden.parent.mkdir(parents=True, exist_ok=True)
        args.golden.write_text(rendered(merged(recorded, world)), encoding="utf-8")
        print(f"id_golden: recorded {len(unrecorded)} new id(s) in "
              f"{args.golden.relative_to(REPO)}")
        unrecorded = []

    for identifier in gone:
        print(f"id_golden: {identifier} is in the golden list and not in the world. If it was "
              f"renamed, every save that names it is broken; if it was retired, delete its line "
              f"by hand and say why", file=sys.stderr)
    for identifier in unrecorded:
        print(f"id_golden: {identifier} is in the world and not in the golden list. Run "
              f"tools/world/id_golden.py --emit", file=sys.stderr)
    if gone or unrecorded:
        return 1
    print(f"id_golden: {len(world)} id(s), all recorded.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
