#!/usr/bin/env python3
"""manifest.py -- add, update and validate rows in `assets-src/assets.manifest.json`.

`HOUSE-00195`. The manifest is the answer to "where did this file come from and may we ship it",
and `cna-house.md` §20.1's rule is absolute: **no row, no build**. This tool is how a row is
created and kept correct; `tools/ci/check_manifest.py` is what refuses a build without one.

    tools/assets/manifest.py validate                     # schema + hashes, changes nothing
    tools/assets/manifest.py rehash [PATH ...]            # update sourceSha256 after an edit
    tools/assets/manifest.py add PATH --id ID --category C --origin-kind generated ...
    tools/assets/manifest.py list [--unknown-provenance]

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
SCHEMA = "cna-house/assets/1"

# `cna-house.md` §20.3. `origin.kind` decides which of the rest are required, because a file this
# project generated has no URL and no author to record, and demanding one would produce a manifest
# full of "n/a" -- which is how a provenance record stops meaning anything.
ORIGIN_KINDS = ("downloaded", "generated", "authored", "derived")

REQUIRED_ROW_FIELDS = ("id", "category", "sourceFile", "sourceSha256", "origin")
REQUIRED_ORIGIN_FIELDS = (
    "kind",
    "name",
    "licence",
    "licenceFile",
    "redistributeSource",
    "redistributeDerived",
    "commercialUse",
    "modification",
)
# Only a downloaded asset has an upstream to point at. An authored or generated one came from here.
REQUIRED_FOR_DOWNLOADED = ("url", "author", "retrieved")

#: The exact string §20.1 reserves. Matched literally, never by substring, so a licence that merely
#: mentions the words cannot be mistaken for the marker.
PROVENANCE_UNKNOWN = "PROVENANCE UNKNOWN — DO NOT SHIP"


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        # Chunked, because a manifest will eventually cover multi-hundred-megabyte assets and
        # reading one into memory to hash it is avoidable.
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load() -> dict:
    if not MANIFEST.exists():
        return {"schema": SCHEMA, "assets": []}
    with MANIFEST.open(encoding="utf-8") as handle:
        return json.load(handle)


def save(document: dict) -> None:
    # Sorted by id, two-space indent, trailing newline, `ensure_ascii=False`. Every one of those is
    # about the DIFF: a manifest whose row order depended on when a row was added would produce a
    # reordering diff on every change, and an escaped non-ASCII author name is unreadable in review.
    document["assets"].sort(key=lambda row: row.get("id", ""))
    text = json.dumps(document, indent=2, ensure_ascii=False, sort_keys=False)
    MANIFEST.write_text(text + "\n", encoding="utf-8")


def validate(document: dict, *, check_hashes: bool = True) -> list[str]:
    """Returns a list of problems. Empty means valid."""
    problems: list[str] = []

    if document.get("schema") != SCHEMA:
        problems.append(f"schema is {document.get('schema')!r}, expected {SCHEMA!r}")

    rows = document.get("assets")
    if not isinstance(rows, list):
        return problems + ["'assets' is missing or is not a list"]

    seen_ids: dict[str, str] = {}
    seen_files: dict[str, str] = {}
    for index, row in enumerate(rows):
        where = row.get("id") or row.get("sourceFile") or f"row {index}"

        for field in REQUIRED_ROW_FIELDS:
            if field not in row:
                problems.append(f"{where}: missing required field '{field}'")

        row_id = row.get("id", "")
        if row_id in seen_ids:
            problems.append(f"{where}: id is already used by {seen_ids[row_id]}")
        elif row_id:
            seen_ids[row_id] = row.get("sourceFile", "?")

        source = row.get("sourceFile", "")
        if source in seen_files:
            problems.append(f"{where}: sourceFile is already claimed by {seen_files[source]}")
        elif source:
            seen_files[source] = row_id

        if source and not source.startswith("assets-src/"):
            problems.append(f"{where}: sourceFile '{source}' is not under assets-src/")

        path = REPO / source if source else None
        if path is not None and not path.is_file():
            problems.append(f"{where}: sourceFile '{source}' does not exist")
        elif path is not None and check_hashes:
            actual = sha256_of(path)
            if actual != row.get("sourceSha256"):
                # The single most important check in the file. A source edited without updating the
                # row means the recorded licence, bounds and review status describe a different
                # asset than the one on disk.
                problems.append(
                    f"{where}: sourceSha256 does not match the file\n"
                    f"    on disk  {actual}\n"
                    f"    recorded {row.get('sourceSha256')}"
                )

        origin = row.get("origin")
        if not isinstance(origin, dict):
            problems.append(f"{where}: 'origin' is missing or is not an object")
            continue

        for field in REQUIRED_ORIGIN_FIELDS:
            if field not in origin:
                problems.append(f"{where}: origin is missing '{field}'")

        kind = origin.get("kind")
        if kind not in ORIGIN_KINDS:
            problems.append(f"{where}: origin.kind {kind!r} is not one of {ORIGIN_KINDS}")
        if kind == "downloaded":
            for field in REQUIRED_FOR_DOWNLOADED:
                if not origin.get(field):
                    problems.append(
                        f"{where}: a downloaded asset must record origin.{field}"
                    )

        for flag in ("redistributeSource", "redistributeDerived", "commercialUse", "modification"):
            if flag in origin and not isinstance(origin[flag], bool):
                # A string "true" is the classic way a permission check silently passes.
                problems.append(f"{where}: origin.{flag} must be a JSON boolean, not {origin[flag]!r}")

        licence_file = origin.get("licenceFile", "")
        if licence_file and licence_file != PROVENANCE_UNKNOWN:
            if not (REPO / licence_file).is_file():
                problems.append(f"{where}: origin.licenceFile '{licence_file}' does not exist")

    return problems


def command_validate(args: argparse.Namespace) -> int:
    document = load()
    problems = validate(document, check_hashes=not args.no_hashes)
    if problems:
        print(f"manifest.py: {len(problems)} problem(s) in {MANIFEST.relative_to(REPO)}:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1
    print(f"manifest.py: {len(document['assets'])} row(s) valid.")
    return 0


def command_rehash(args: argparse.Namespace) -> int:
    document = load()
    wanted = set(args.paths)
    changed = 0
    for row in document["assets"]:
        source = row.get("sourceFile", "")
        if wanted and source not in wanted:
            continue
        path = REPO / source
        if not path.is_file():
            print(f"manifest.py: {source} does not exist; not rehashed", file=sys.stderr)
            continue
        actual = sha256_of(path)
        if actual != row.get("sourceSha256"):
            print(f"manifest.py: {source}\n    {row.get('sourceSha256')} -> {actual}")
            row["sourceSha256"] = actual
            changed += 1
    if changed:
        save(document)
    print(f"manifest.py: {changed} row(s) rehashed.")
    return 0


def command_add(args: argparse.Namespace) -> int:
    document = load()
    source = args.path
    if not source.startswith("assets-src/"):
        print(f"manifest.py: '{source}' is not under assets-src/", file=sys.stderr)
        return 2
    path = REPO / source
    if not path.is_file():
        print(f"manifest.py: '{source}' does not exist", file=sys.stderr)
        return 2
    if any(row.get("sourceFile") == source for row in document["assets"]):
        print(f"manifest.py: '{source}' already has a row; use rehash", file=sys.stderr)
        return 2

    row = {
        "id": args.id,
        "category": args.category,
        "sourceFile": source,
        "sourceSha256": sha256_of(path),
        "origin": {
            "kind": args.origin_kind,
            "name": args.name,
            "licence": args.licence,
            "licenceFile": args.licence_file,
            "attribution": args.attribution,
            "redistributeSource": args.redistribute_source,
            "redistributeDerived": args.redistribute_derived,
            "commercialUse": args.commercial_use,
            "modification": args.modification,
        },
    }
    for field in ("url", "author", "retrieved"):
        value = getattr(args, field, None)
        if value:
            row["origin"][field] = value
    if args.note:
        row["origin"]["note"] = args.note

    document["assets"].append(row)
    problems = validate(document)
    if problems:
        print("manifest.py: the new row is invalid; nothing was written:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1
    save(document)
    print(f"manifest.py: added {args.id} for {source}")
    return 0


def command_list(args: argparse.Namespace) -> int:
    document = load()
    rows = document["assets"]
    if args.unknown_provenance:
        rows = [r for r in rows if r.get("origin", {}).get("licence") == PROVENANCE_UNKNOWN]
    for row in rows:
        origin = row.get("origin", {})
        print(f"{row.get('id','?'):40s} {origin.get('licence','?'):24s} {row.get('sourceFile','?')}")
    print(f"-- {len(rows)} row(s)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("validate", help="check the schema and every hash")
    p.add_argument("--no-hashes", action="store_true", help="schema only; do not read the files")
    p.set_defaults(func=command_validate)

    p = sub.add_parser("rehash", help="update sourceSha256 from the files on disk")
    p.add_argument("paths", nargs="*", help="repository-relative paths; all rows if omitted")
    p.set_defaults(func=command_rehash)

    p = sub.add_parser("add", help="add a row for a file")
    p.add_argument("path")
    p.add_argument("--id", required=True)
    p.add_argument("--category", required=True)
    p.add_argument("--origin-kind", required=True, choices=ORIGIN_KINDS)
    p.add_argument("--name", required=True)
    p.add_argument("--licence", required=True)
    p.add_argument("--licence-file", required=True)
    p.add_argument("--attribution", default="")
    p.add_argument("--url", default="")
    p.add_argument("--author", default="")
    p.add_argument("--retrieved", default="")
    p.add_argument("--note", default="")
    # Defaulting the four permissions to FALSE is deliberate: a row added without thinking about
    # them says "we may not do any of this", which is the safe direction to be wrong in.
    p.add_argument("--redistribute-source", action="store_true")
    p.add_argument("--redistribute-derived", action="store_true")
    p.add_argument("--commercial-use", action="store_true")
    p.add_argument("--modification", action="store_true")
    p.set_defaults(func=command_add)

    p = sub.add_parser("list", help="list the rows")
    p.add_argument("--unknown-provenance", action="store_true")
    p.set_defaults(func=command_list)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
