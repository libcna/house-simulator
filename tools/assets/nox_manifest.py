#!/usr/bin/env python3
"""nox_manifest.py -- give every converted NOX file a manifest row, with its audit trail.

`HOUSE-00279`. `docs/licence-evidence/nox-sound-essentials.md` §5 writes this task's requirements
out in full, so they are not invented here: every imported row records `CC0-1.0`,
`licenceFile: licenses/cc0-1.0/LICENCE.txt`,
`url: https://nox-sound-design.itch.io/essentials-series-sfx-nox-sound`, `retrieved: 2026-09-07`,
`author: Nox_Sound`, **and both the original and converted hashes**.

    tools/assets/nox_manifest.py            # add or update every row
    tools/assets/nox_manifest.py --check    # fail if any row is missing or stale
    tools/assets/nox_manifest.py --selftest

## Both hashes, and why the original one is the point

The NOX collection lives in `/rv/tmp/`, which `AGENTS.md` treats as scratch and which will be
deleted. Once it is, the only evidence that
`assets-src/Audio/footstep/grass/walk/Footstep_Grass_Walk_01.wav` came from the file the licence
evidence was written about is the **original's SHA-256**, recorded here. The converted file's hash
is what `check_manifest.py` verifies day to day; the original's is what an auditor would need in a
year, and it cannot be recovered later.

`seconds`, `channels`, `sampleRate` and the source duration go in the same block. They are the
numbers `HOUSE-00280` maps onto surfaces and `budget_report.py` sums into §72's audio row, and
reading 445 files to recover them is slower than storing them.

## Packs

`ambience/*` is `audio-ambience`; everything else is `audio-core`. §72 gives the first 55 MB and
the second 30, and the split is what those two budgets are for -- ambience beds are long and
stream, one-shots are short and are held.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import manifest as manifest_tool  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
CONVERSION = REPO / "docs" / "asset-selection" / "nox-conversion.json"

#: `docs/licence-evidence/nox-sound-essentials.md` §5, verbatim. Not a judgement made here.
ORIGIN = {
    "kind": "downloaded",
    "name": "NOX Sound — Essentials Series SFX",
    "licence": "CC0-1.0",
    "licenceFile": "licenses/cc0-1.0/LICENCE.txt",
    "url": "https://nox-sound-design.itch.io/essentials-series-sfx-nox-sound",
    "author": "Nox_Sound",
    "retrieved": "2026-09-07",
    "attribution": "",
    "redistributeSource": True,
    "redistributeDerived": True,
    "commercialUse": True,
    "modification": True,
}

#: §71's two audio packs, and its own descriptions of them: `audio-core` is "footsteps,
#: interaction sounds, UI" at 30 MB, `audio-ambience` is "room tones, weather, exterior ambience"
#: at 55 MB.
AMBIENCE_PACK = "audio-ambience"
DEFAULT_PACK = "audio-core"

#: Top-level categories whose LOOPS are room tone. `ambience` is weather and exterior;
#: `appliance` is the fridge, the computer and the extractor, which are room tone by any reading of
#: §71's phrase -- a fridge hum is exactly what "room tone" names.
AMBIENCE_LOOP_CATEGORIES = ("ambience", "appliance")

ID_PREFIX = "SOUND_NOX_"


def pack_for(category: str, loop: bool) -> str:
    """Which pack a clip belongs in, from §71's descriptions rather than from its name.

    **A one-shot is never ambience, and a loop is not automatically ambience either.** The first
    version of this split on the category name alone -- `ambience/*` to `audio-ambience`, the rest
    to `audio-core` -- and put 58 of the 72 loops in the pack meant for footsteps and UI clicks:
    `audio-core` came out at **66 MB against its 30 MB budget** while `audio-ambience` sat at 24 of
    its 55. The 39 appliance loops are the bulk of it, and they are room tone.

    A human breath sequence and a car engine loop are *loops* but they are not room tone -- they
    are interaction sounds, which is `audio-core`'s own description -- so length alone is not the
    rule either.
    """
    top = category.split("/")[0]
    if loop and top in AMBIENCE_LOOP_CATEGORIES:
        return AMBIENCE_PACK
    return DEFAULT_PACK


def identifier_for(entry: dict) -> str:
    """`SOUND_NOX_<CATEGORY>_<NAME>`, matching `conventions.md` §1's `^[A-Z][A-Z0-9_]*$`.

    Built from the category *and* the file name because neither alone is unique: the publisher
    reuses names across packs, and a category holds up to eight variants.
    """
    stem = Path(entry["output"]).stem
    raw = f"{entry['category']}_{stem}"
    identifier = ID_PREFIX + re.sub(r"[^A-Za-z0-9]+", "_", raw).upper().strip("_")
    return re.sub(r"_+", "_", identifier)


def content_name_for(entry: dict) -> str:
    """`Audio/<category>/<stem>` -- the path `ContentManager::Load` is given, without a suffix."""
    return Path(entry["output"]).relative_to("assets-src").with_suffix("").as_posix()


def row_for(entry: dict) -> dict:
    return {
        "id": identifier_for(entry),
        "category": "sound",
        "sourceFile": entry["output"],
        "sourceSha256": entry["outputSha256"],
        "contentName": content_name_for(entry),
        "kind": "sound",
        "residencyPack": pack_for(entry["category"], entry["loop"]),
        "audio": {
            "noxCategory": entry["category"],
            "seconds": entry["outputSeconds"],
            "sourceSeconds": entry["sourceSeconds"],
            "channels": entry["channels"],
            "sampleRate": entry["sampleRate"],
            "loop": entry["loop"],
            "shortenedToSeconds": entry["shortened"],
            "trimmed": entry["trimmed"],
            # The audit trail. `/rv/tmp` is scratch and will be deleted; after that this is the
            # only link between the file in the tree and the licence evidence written about it.
            "originalSha256": entry["sourceSha256"],
        },
        "origin": dict(ORIGIN, note=(
            "Selected by tools/assets/nox_select.py (HOUSE-00277) and converted by "
            "tools/assets/nox_convert.py (HOUSE-00278). Licence verified in "
            "docs/licence-evidence/nox-sound-essentials.md (HOUSE-00276). CC0 requires no "
            "attribution and the credits generator omits it.")),
    }


def apply(document: dict, entries: list[dict]) -> tuple[int, int]:
    """Add or update a row per converted file. Returns `(added, updated)`."""
    by_source = {row.get("sourceFile"): row for row in document["assets"]}
    added = updated = 0
    for entry in entries:
        row = row_for(entry)
        existing = by_source.get(row["sourceFile"])
        if existing is None:
            document["assets"].append(row)
            added += 1
        elif existing != row:
            existing.clear()
            existing.update(row)
            updated += 1
    document["assets"].sort(key=lambda r: (r.get("category", ""), r.get("id", "")))
    return added, updated


SOURCE_TEMPLATE = """# `{directory}/` — provenance

*Generated by `tools/assets/nox_manifest.py` (`HOUSE-00279`). All 445 imported NOX files share one
publisher, one licence and one retrieval, so the 110 directories share one record differing only in
the table below. Regenerate rather than edit; `--check` fails if this file is stale.*

## Assets

| Manifest id | File | Upstream name / version |
|---|---|---|
{rows}

## Source

| | |
|---|---|
| Publisher / author | Nox_Sound / Nox_Sound_Design |
| Canonical URL | <{url}> |
| Retrieved | {retrieved} |
| Upstream release / tag / version | Essentials Series SFX, 1 644 files as downloaded |
| Second source cross-checked? | Yes — the publisher's Freesound account (<https://freesound.org/people/Nox_Sound/>) and A Sound Effect (<https://www.asoundeffect.com/sounddesigner/nox-sound/>); see `docs/licence-evidence/nox-sound-essentials.md` §3 |

## Licence

| | |
|---|---|
| Licence | `CC0-1.0` |
| Archived at | `licenses/cc0-1.0/LICENCE.txt` |
| How it was established | The publisher's own itch.io product page states the release is CC0 verbatim; read, quoted and archived in `docs/licence-evidence/nox-sound-essentials.md` §1 (`HOUSE-00276`). Not an aggregator's label. |
| Redistribute source | yes |
| Redistribute derived / compiled | yes |
| Commercial use | yes |
| Modification | yes |
| Attribution required | none — CC0 waives it, and the credits generator omits it |
| Other obligations | none |

## Modification

| | |
|---|---|
| Modified from upstream? | **yes** |
| If yes, what and why | Converted to the runtime's audio profile. |
| Conversion history | 24-bit → 16-bit; sample rate and channel count preserved (`HOUSE-00069` measured that resampling 48 → 44.1 kHz costs 0.889 dB RMS and buys nothing). One-shots have leading and trailing silence below −60 dBFS trimmed. Loops longer than 10.25 s are shortened to 10 s with a 0.25 s head crossfade, so the wrap stays continuous — §72's audio budget only closes with that trim. Performed by `tools/assets/nox_convert.py` (`HOUSE-00278`); every file's original SHA-256 is in the manifest under `audio.originalSha256`. |

## Notes

The selection is 445 of 1 644 files, chosen by rule rather than by ear — see
`docs/asset-selection/nox-subset.md` (`HOUSE-00277`), which also records **every file that was not
selected and why**. Ten of the 1 644 are excluded from the CC0 verdict entirely
(`docs/licence-evidence/nox-sound-essentials.md` §4) and none of them is here.

The downloaded collection lives in `/rv/tmp/`, which `AGENTS.md` treats as scratch and which will be
deleted. After that, the `audio.originalSha256` recorded per row is the only link between a file in
this directory and the evidence above.
"""


def source_document(directory: str, rows: list[dict]) -> str:
    table = "\n".join(
        f"| `{row['id']}` | `{Path(row['sourceFile']).name}` | "
        f"NOX Essentials Series, {row['audio']['noxCategory']} |"
        for row in sorted(rows, key=lambda r: r["id"]))
    return SOURCE_TEMPLATE.format(directory=directory, rows=table,
                                  url=ORIGIN["url"], retrieved=ORIGIN["retrieved"])


def write_source_documents(document: dict, root: Path, *, dry_run: bool = False) -> list[str]:
    """One `SOURCE.md` per directory holding NOX rows. Returns the paths that were stale."""
    by_directory: dict[str, list[dict]] = {}
    for row in document["assets"]:
        if not row.get("id", "").startswith(ID_PREFIX):
            continue
        by_directory.setdefault(Path(row["sourceFile"]).parent.as_posix(), []).append(row)

    stale = []
    for directory, rows in sorted(by_directory.items()):
        path = root / directory / "SOURCE.md"
        wanted = source_document(directory, rows)
        if path.is_file() and path.read_text(encoding="utf-8") == wanted:
            continue
        stale.append(path.relative_to(root).as_posix())
        if not dry_run:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(wanted, encoding="utf-8")
    return stale


def load_entries(path: Path) -> list[dict]:
    if not path.is_file():
        raise SystemExit(
            f"nox_manifest: {path} is not there; run tools/assets/nox_convert.py first")
    return json.loads(path.read_text(encoding="utf-8"))["files"]


# ======================================================================================= selftest


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("nox_manifest: selftest")

    entry = {
        "file": "Nature_Essentials_NOX_SOUND/Ambiance_Rain_Calm_Loop_Stereo.wav",
        "category": "ambience/rain-calm",
        "output": "assets-src/Audio/ambience/rain-calm/Ambiance_Rain_Calm_Loop_Stereo.wav",
        "sourceSha256": "a" * 64, "outputSha256": "b" * 64,
        "sourceSeconds": 30.0, "outputSeconds": 10.0,
        "channels": 2, "sampleRate": 48000, "loop": True,
        "shortened": 10.0, "trimmed": False,
    }
    step = dict(entry,
                file="Footsteps_NOX_SOUND/Footstep_Grass_Walk_01.wav",
                category="footstep/grass/walk",
                output="assets-src/Audio/footstep/grass/walk/Footstep_Grass_Walk_01.wav",
                outputSeconds=0.6, sourceSeconds=0.9, channels=1, loop=False,
                shortened=None, trimmed=True)

    # 1. Ids are legal, unique, and built from both the category and the name.
    identifier = identifier_for(entry)
    require(re.fullmatch(r"[A-Z][A-Z0-9_]*", identifier) is not None,
            f"the id matches conventions.md §1's ^[A-Z][A-Z0-9_]*$ ({identifier})")
    require(identifier != identifier_for(step),
            "two files in different categories get different ids")
    same_name = dict(step, category="footstep/gravel/walk",
                     output="assets-src/Audio/footstep/gravel/walk/Footstep_Grass_Walk_01.wav")
    require(identifier_for(step) != identifier_for(same_name),
            "and the SAME file name in two categories does too -- the publisher reuses names "
            "across packs, so the name alone is not unique")

    # 2. The content name is what ContentManager::Load is given: no assets-src/, no suffix.
    require(content_name_for(step) == "Audio/footstep/grass/walk/Footstep_Grass_Walk_01",
            f"the content name drops assets-src/ and the suffix ({content_name_for(step)})")

    # 3. §72's two audio packs, split by what the budgets are for.
    require(pack_for("ambience/rain-calm", True) == AMBIENCE_PACK,
            "a weather bed goes to audio-ambience -- §71's 'room tones, weather, exterior'")
    require(pack_for("appliance/fridge", True) == AMBIENCE_PACK,
            "and so does a fridge hum: it is room tone by any reading of that phrase, and the 39 "
            "appliance loops are 35 MB")
    require(pack_for("footstep/grass/walk", False) == DEFAULT_PACK
            and pack_for("door/front/open", False) == DEFAULT_PACK,
            "a one-shot goes to audio-core -- 'footsteps, interaction sounds, UI'")
    require(pack_for("human/male/breath", True) == DEFAULT_PACK
            and pack_for("car/engine", True) == DEFAULT_PACK,
            "a breath sequence and an engine loop are LOOPS but not room tone, so length alone is "
            "not the rule -- they are interaction sounds and stay in audio-core")
    require(pack_for("appliance/microwave", False) == DEFAULT_PACK,
            "and an appliance ONE-SHOT -- a microwave ping -- is an interaction sound, so the "
            "category alone is not the rule either")

    # 4. BOTH hashes. The pool is scratch and will be deleted; after that the original's hash is
    #    the only link between the file in the tree and the licence evidence written about it.
    row = row_for(entry)
    require(row["sourceSha256"] == "b" * 64,
            "sourceSha256 is the CONVERTED file's -- that is what check_manifest verifies")
    require(row["audio"]["originalSha256"] == "a" * 64,
            "and audio.originalSha256 is the NOX original's, which cannot be recovered once "
            "/rv/tmp is deleted")
    require(row["audio"]["seconds"] == 10.0 and row["audio"]["sourceSeconds"] == 30.0,
            "both durations are recorded, so the shortening is visible in the manifest itself")
    require(row["audio"]["channels"] == 2 and row["audio"]["sampleRate"] == 48000,
            "with the channel count and sample rate HOUSE-00280 and budget_report.py read")

    # 5. The licence block is the evidence document's, not a judgement made here.
    require(row["origin"]["licence"] == "CC0-1.0"
            and row["origin"]["licenceFile"] == "licenses/cc0-1.0/LICENCE.txt"
            and row["origin"]["author"] == "Nox_Sound"
            and row["origin"]["retrieved"] == "2026-09-07",
            "the origin block is nox-sound-essentials.md §5's, verbatim")
    require(row["origin"]["attribution"] == "",
            "attribution is empty: CC0 requires none and the credits generator omits it")
    require(all(row["origin"][k] for k in ("redistributeSource", "redistributeDerived",
                                           "commercialUse", "modification")),
            "all four permissions are granted, which is what CC0 means")

    # 6. Applying twice changes nothing the second time.
    document = {"schema": manifest_tool.SCHEMA, "packs": [], "assets": []}
    added, updated = apply(document, [entry, step])
    require((added, updated) == (2, 0), f"two rows added ({added}, {updated})")
    added, updated = apply(document, [entry, step])
    require((added, updated) == (0, 0),
            f"and a second pass changes nothing ({added} added, {updated} updated)")
    changed = dict(entry, outputSha256="c" * 64)
    added, updated = apply(document, [changed, step])
    require((added, updated) == (0, 1),
            f"a reconverted file updates its row rather than duplicating it ({added}, {updated})")
    require(len(document["assets"]) == 2, "and the row count does not grow")

    # 7. The rows the real schema requires are all present.
    for field in manifest_tool.REQUIRED_ROW_FIELDS:
        require(field in row, f"the row carries the required field {field!r}")
    for field in manifest_tool.REQUIRED_ORIGIN_FIELDS:
        require(field in row["origin"], f"the origin carries the required field {field!r}")
    for field in manifest_tool.REQUIRED_FOR_DOWNLOADED:
        require(field in row["origin"],
                f"a downloaded origin carries {field!r}, which manifest.py requires of it")

    if failures:
        return 1
    print("nox_manifest: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--conversion", type=Path, default=CONVERSION)
    parser.add_argument("--check", action="store_true",
                        help="fail if a row is missing or stale, and change nothing")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    entries = load_entries(args.conversion)
    document = manifest_tool.load()
    added, updated = apply(document, entries)

    if args.check:
        stale = write_source_documents(document, REPO, dry_run=True)
        if added or updated or stale:
            print(f"nox_manifest: {added} row(s) missing, {updated} stale, "
                  f"{len(stale)} SOURCE.md out of date; run tools/assets/nox_manifest.py",
                  file=sys.stderr)
            for path in stale[:5]:
                print(f"  {path}", file=sys.stderr)
            return 1
        print(f"nox_manifest: all {len(entries)} converted NOX rows and their SOURCE.md records "
              f"are present and current.")
        return 0

    problems = manifest_tool.validate(document, check_hashes=False)
    if problems:
        print("nox_manifest: the result would not validate:", file=sys.stderr)
        for problem in problems[:20]:
            print(f"  {problem}", file=sys.stderr)
        return 1
    manifest_tool.save(document)
    written = write_source_documents(document, REPO)
    print(f"nox_manifest: {added} row(s) added, {updated} updated; "
          f"{len(document['assets'])} rows in the manifest; "
          f"{len(written)} SOURCE.md written.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
