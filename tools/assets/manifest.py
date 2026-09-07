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

#: `cna-house.md` §27.2's partition, plus `dev`. **Every packaged asset belongs to exactly one of
#: these** (`HOUSE-00202`), and the list lives in the manifest itself -- this is only the set the
#: tool writes when a manifest has none, so that the file remains the single authority and the tool
#: does not quietly overrule it.
#:
#: `dev` is the thirteenth and is NOT in §27.2, which lists the shipping packs. It exists because
#: the content smoke scene's four fixtures (`HOUSE-00201`) really are runtime assets -- they are
#: compiled, loaded and drawn -- and calling them "not packaged" would be false. Marking them
#: `shipped: false` says the true thing instead, and gives `budget_report.py` a shipped/total split
#: that means something.
DEFAULT_PACKS = [
    {"id": "core", "budgetBytes": 55 * 1000 * 1000, "shipped": True,
     "contents": "fonts, HUD, shared materials, player avatar, sky, weather, effects"},
    {"id": "exterior", "budgetBytes": 90 * 1000 * 1000, "shipped": True,
     "contents": "terrain, road, fences, garden, shed, vehicles, vegetation LOD0/1"},
    {"id": "neighbourhood", "budgetBytes": 45 * 1000 * 1000, "shipped": True,
     "contents": "LOD1/LOD2 houses, impostors, distant vegetation"},
    {"id": "house-l0", "budgetBytes": 110 * 1000 * 1000, "shipped": True,
     "contents": "L0 shell, lightmaps, props, sunroom, garage, porch"},
    {"id": "house-l1", "budgetBytes": 75 * 1000 * 1000, "shipped": True,
     "contents": "L1 shell, lightmaps, props, balconies"},
    {"id": "house-l2", "budgetBytes": 70 * 1000 * 1000, "shipped": True,
     "contents": "L2 shell, lightmaps, props"},
    {"id": "house-b1", "budgetBytes": 55 * 1000 * 1000, "shipped": True,
     "contents": "B1 shell, lightmaps, props"},
    {"id": "house-l3", "budgetBytes": 40 * 1000 * 1000, "shipped": True,
     "contents": "L3 shell, lightmaps, props"},
    {"id": "pets", "budgetBytes": 22 * 1000 * 1000, "shipped": True,
     "contents": "dog, cat, their clips and sounds"},
    {"id": "audio-core", "budgetBytes": 30 * 1000 * 1000, "shipped": True,
     "contents": "footsteps, interaction sounds, UI"},
    {"id": "audio-ambience", "budgetBytes": 55 * 1000 * 1000, "shipped": True,
     "contents": "room tones, weather, exterior ambience"},
    {"id": "video", "budgetBytes": 25 * 1000 * 1000, "shipped": True,
     "contents": "television media"},
    {"id": "dev", "budgetBytes": 5 * 1000 * 1000, "shipped": False,
     "contents": "content-pipeline fixtures loaded by --scene=content-smoke; never shipped"},
]

#: What the runtime `ContentRegistry` can load. The strings are the ones `ParseAssetKind` accepts,
#: and they are checked here rather than at load so a typo is a build failure and not a startup one.
ASSET_KINDS = ("model", "texture", "sound", "font", "effect", "video")

#: The three fields that make a row a RUNTIME asset. All three or none.
PACKAGED_FIELDS = ("contentName", "kind", "residencyPack")

#: What a row records when its asset is one PART of a character that was split per skin
#: (`HOUSE-00224`). Optional, and validated whenever it is present.
#:
#: `joints` is the part's skin's joint names **in blend-index order**. `HOUSE-00074` measured that
#: vertex blend indices are skin-local, so that list IS the binding between this part's vertices
#: and the shared skeleton -- it is the same list the `.chanim` sidecar carries, and the reason the
#: runtime never calls `getSkinsEXTProperty()`.
REQUIRED_ATTACHMENT_FIELDS = ("source", "skeletonRoot", "attachmentBone", "joints")
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
        return {"schema": SCHEMA, "packs": [dict(pack) for pack in DEFAULT_PACKS], "assets": []}
    with MANIFEST.open(encoding="utf-8") as handle:
        return json.load(handle)


def save(document: dict) -> None:
    # Sorted by id, two-space indent, trailing newline, `ensure_ascii=False`. Every one of those is
    # about the DIFF: a manifest whose row order depended on when a row was added would produce a
    # reordering diff on every change, and an escaped non-ASCII author name is unreadable in review.
    document["assets"].sort(key=lambda row: row.get("id", ""))
    # The packs are NOT sorted: §27.2's order is `core` first and then coarse-to-fine, which is how
    # a reader thinks about residency, and alphabetising it would put `audio-ambience` first.
    text = json.dumps(document, indent=2, ensure_ascii=False, sort_keys=False)
    MANIFEST.write_text(text + "\n", encoding="utf-8")


def validate_packs(document: dict) -> tuple[list[str], dict[str, dict]]:
    """The pack partition itself: declared once, at the top of the manifest (`HOUSE-00202`)."""
    problems: list[str] = []
    packs = document.get("packs")
    if not isinstance(packs, list) or not packs:
        return ["'packs' is missing or empty; the partition has to be declared before a row can "
                "name a pack"], {}

    by_id: dict[str, dict] = {}
    for index, pack in enumerate(packs):
        where = pack.get("id") or f"packs[{index}]"
        for field in ("id", "budgetBytes", "shipped", "contents"):
            if field not in pack:
                problems.append(f"pack {where}: missing '{field}'")
        pack_id = pack.get("id", "")
        if pack_id in by_id:
            problems.append(f"pack {where}: declared twice")
        elif pack_id:
            by_id[pack_id] = pack
        budget = pack.get("budgetBytes")
        if not isinstance(budget, int) or budget <= 0:
            problems.append(f"pack {where}: budgetBytes must be a positive integer, not {budget!r}")
        if "shipped" in pack and not isinstance(pack["shipped"], bool):
            # The same reasoning as the four permission booleans: a string "false" is how a check
            # silently passes.
            problems.append(f"pack {where}: 'shipped' must be a JSON boolean, not "
                            f"{pack['shipped']!r}")
        if not str(pack.get("contents", "")).strip():
            problems.append(f"pack {where}: 'contents' must say what belongs in it; a pack nobody "
                            f"described is a pack nobody can assign to")
    return problems, by_id


def validate(document: dict, *, check_hashes: bool = True) -> list[str]:
    """Returns a list of problems. Empty means valid."""
    problems: list[str] = []

    if document.get("schema") != SCHEMA:
        problems.append(f"schema is {document.get('schema')!r}, expected {SCHEMA!r}")

    pack_problems, packs = validate_packs(document)
    problems += pack_problems

    rows = document.get("assets")
    if not isinstance(rows, list):
        return problems + ["'assets' is missing or is not a list"]

    seen_ids: dict[str, str] = {}
    seen_files: dict[str, str] = {}
    seen_content_names: dict[str, str] = {}
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

        # --- the pack partition (`HOUSE-00202`) ---------------------------------------------
        #
        # A row is either a RUNTIME asset -- in which case it has a content name, a kind and
        # exactly one pack -- or it is not, in which case it has to say WHY in `notPackaged`.
        # There is no third state, and that is the whole point: "no pack" as a silent default is
        # exactly how an asset ends up in no download and in nobody's budget.
        packaged = [field for field in PACKAGED_FIELDS if field in row]
        reason = row.get("notPackaged")
        if reason is not None:
            if packaged:
                problems.append(f"{where}: declares notPackaged AND {sorted(packaged)}; a row is "
                                f"either a runtime asset or it is not")
            if not str(reason).strip():
                problems.append(f"{where}: notPackaged must say why, not just be present")
        elif len(packaged) != len(PACKAGED_FIELDS):
            missing = [field for field in PACKAGED_FIELDS if field not in row]
            problems.append(
                f"{where}: missing {missing}. Every asset belongs to exactly ONE pack "
                f"(cna-house.md §27.2). If this file is not itself a runtime asset -- a .ttf a "
                f"SpriteFont rasterises, say -- set 'notPackaged' to the reason instead.")
        else:
            kind = row.get("kind")
            if kind not in ASSET_KINDS:
                problems.append(f"{where}: kind {kind!r} is not one of {ASSET_KINDS}")
            pack_id = row.get("residencyPack")
            if packs and pack_id not in packs:
                problems.append(f"{where}: residencyPack {pack_id!r} is not a declared pack "
                                f"({sorted(packs)})")
            content_name = str(row.get("contentName", ""))
            if content_name.startswith("/") or content_name.endswith((".cnb", ".xnb")):
                # §8.3: a CONTENT NAME, never an OS path and never an extension. The runtime
                # appends the container extension itself, and a name that carries one loads
                # nothing with a message about a file that does exist.
                problems.append(f"{where}: contentName '{content_name}' looks like a path; it must "
                                f"be a content name such as 'Models/Smoke/marker'")
            if content_name in seen_content_names:
                problems.append(f"{where}: contentName '{content_name}' is already used by "
                                f"{seen_content_names[content_name]}")
            elif content_name:
                seen_content_names[content_name] = row_id

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

        attachment = row.get("attachment")
        if attachment is not None:
            if not isinstance(attachment, dict):
                problems.append(f"{where}: 'attachment' is not an object")
            else:
                for field in REQUIRED_ATTACHMENT_FIELDS:
                    if not attachment.get(field):
                        problems.append(f"{where}: attachment is missing '{field}'")
                joints = attachment.get("joints")
                if joints is not None and (not isinstance(joints, list)
                                           or not all(isinstance(j, str) for j in joints)):
                    problems.append(f"{where}: attachment.joints must be a list of joint NAMES in "
                                    f"blend-index order")
                elif isinstance(joints, list) and len(set(joints)) != len(joints):
                    # The list is the binding; a duplicate makes two blend indices the same joint.
                    problems.append(f"{where}: attachment.joints repeats a name, so two blend "
                                    f"indices would mean one joint")

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
    }
    if args.not_packaged:
        row["notPackaged"] = args.not_packaged
    else:
        row["contentName"] = args.content_name or ""
        row["kind"] = args.kind or ""
        row["residencyPack"] = args.pack or ""
    row.update({
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
    })
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


def command_packs(args: argparse.Namespace) -> int:
    del args
    document = load()
    problems, packs = validate_packs(document)
    for problem in problems:
        print(f"manifest.py: {problem}", file=sys.stderr)

    counts: dict[str, int] = {pack_id: 0 for pack_id in packs}
    unpackaged = 0
    for row in document.get("assets", []):
        if "notPackaged" in row:
            unpackaged += 1
            continue
        pack_id = row.get("residencyPack", "")
        counts[pack_id] = counts.get(pack_id, 0) + 1

    print(f"{'pack':<16} {'assets':>6}  {'budget':>9}  ship  contents")
    for pack_id, pack in packs.items():
        print(f"{pack_id:<16} {counts.get(pack_id, 0):>6}  "
              f"{pack['budgetBytes'] / 1e6:>7.0f} MB  "
              f"{'yes' if pack.get('shipped') else 'NO ':>4}  {pack.get('contents', '')}")
    # Reported, never hidden: an empty pack early in a project is normal, and an asset that belongs
    # to no pack is the thing this partition exists to make impossible.
    print(f"\n{sum(counts.values())} packaged asset(s) across {len(packs)} pack(s); "
          f"{unpackaged} build input(s) not packaged")
    return 1 if problems else 0


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
    # The pack partition (`HOUSE-00202`). Not defaulted: a row added without a pack is a row in no
    # download and in nobody's budget, so `validate` refuses it and this refuses to guess.
    p.add_argument("--content-name", help="the runtime content name, e.g. Models/Smoke/marker")
    p.add_argument("--kind", choices=ASSET_KINDS)
    p.add_argument("--pack", help=f"residency pack; one of the declared packs")
    p.add_argument("--not-packaged", metavar="REASON",
                   help="this file is a build input, not a runtime asset; say why")
    p.add_argument("--redistribute-source", action="store_true")
    p.add_argument("--redistribute-derived", action="store_true")
    p.add_argument("--commercial-use", action="store_true")
    p.add_argument("--modification", action="store_true")
    p.set_defaults(func=command_add)

    p = sub.add_parser("packs", help="list the pack partition and what is in each")
    p.set_defaults(func=command_packs)

    p = sub.add_parser("list", help="list the rows")
    p.add_argument("--unknown-provenance", action="store_true")
    p.set_defaults(func=command_list)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
