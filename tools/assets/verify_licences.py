#!/usr/bin/env python3
"""verify_licences.py -- every asset has a licence we can point at, and the credits say so.

`HOUSE-00197` and `HOUSE-00198`. Two jobs that must not be separated:

* **verify** -- every manifest row has a licence, a licence file that exists, and the four
  redistribution booleans; every directory holding a DOWNLOADED asset has a `SOURCE.md` that names
  it (`HOUSE-00261`); a packaging build refuses `PROVENANCE UNKNOWN`.
* **emit** -- generate `licenses/THIRD-PARTY-ASSETS.md` from those same rows, so attribution is
  impossible to forget (`cna-house.md` §20.1) and impossible to drift.

They are one tool because a generator that could run on unverified rows would produce a credits page
asserting licences nobody checked.

    tools/assets/verify_licences.py                 # verify, development rules
    tools/assets/verify_licences.py --packaging     # verify, shipping rules -- refuses UNKNOWN
    tools/assets/verify_licences.py --emit          # verify, then write the document
    tools/assets/verify_licences.py --check         # verify, then fail if the document is stale

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import manifest as manifest_tool  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
DOCUMENT = REPO / "licenses" / "THIRD-PARTY-ASSETS.md"
PROVENANCE_UNKNOWN = manifest_tool.PROVENANCE_UNKNOWN

HEADER = """# Third-party assets

<!--
    GENERATED FILE — DO NOT EDIT.

    Produced by `tools/assets/verify_licences.py --emit` from `assets-src/assets.manifest.json`.
    Edit the manifest and regenerate; a hand edit is detected by `verify_licences.py --check`,
    which fails the build. The generator's contract is `licenses/README.md`.
-->

This document lists every third-party asset shipped with CNA House, its author, its source and its
licence, grouped by licence. It is generated from the asset manifest, so it cannot drift from the
assets actually present. It is shown in-game from the credits screen.

The **program's** own licence is Ms-PL — see [`../LICENSE`](../LICENSE) — and the runtime
dependency notices are in [`../NOTICE.md`](../NOTICE.md). This file covers **content assets only**.

---
"""


def verify_source_records(document: dict) -> list[str]:
    """`HOUSE-00261`: a downloaded asset lives in a directory that says where it came from.

    The manifest already carries the licence, the URL and the hash. What it cannot carry is the
    *reasoning* -- which publisher page was read, what was checked, what was rejected and why -- and
    that is exactly what gets lost between the session that acquired an asset and the person asking
    six months later whether it can ship.

    **The check is that the record NAMES each asset**, not merely that a file exists. A `SOURCE.md`
    written for one font and never updated when a second arrived would otherwise pass forever, which
    is the failure mode of every "please document it" convention that is not enforced.
    """
    problems: list[str] = []
    by_directory: dict[Path, list[str]] = {}
    for row in document.get("assets", []):
        origin = row.get("origin") or {}
        if origin.get("kind") != "downloaded":
            continue
        source = row.get("sourceFile", "")
        if not source:
            continue
        by_directory.setdefault(Path(source).parent, []).append(row.get("id", ""))

    for directory, ids in sorted(by_directory.items()):
        record = REPO / directory / "SOURCE.md"
        if not record.is_file():
            problems.append(
                f"{directory}/: holds downloaded asset(s) {', '.join(sorted(ids))} but has no "
                f"SOURCE.md. Copy docs/asset-review/SOURCE-TEMPLATE.md and fill it in (HOUSE-00261)."
            )
            continue
        text = record.read_text(encoding="utf-8")
        for asset_id in sorted(ids):
            if asset_id and asset_id not in text:
                problems.append(
                    f"{directory}/SOURCE.md does not mention '{asset_id}', so that asset's "
                    f"provenance was never written down (HOUSE-00261)."
                )
    return problems


def verify(document: dict, *, packaging: bool) -> list[str]:
    problems: list[str] = verify_source_records(document)
    for row in document.get("assets", []):
        where = row.get("id") or row.get("sourceFile") or "<row>"
        origin = row.get("origin") or {}

        licence = origin.get("licence")
        if not licence:
            problems.append(f"{where}: no origin.licence; the asset cannot be shipped or credited")
            continue

        licence_file = origin.get("licenceFile")
        if not licence_file:
            problems.append(f"{where}: no origin.licenceFile")
        elif licence != PROVENANCE_UNKNOWN and not (REPO / licence_file).is_file():
            problems.append(
                f"{where}: origin.licenceFile '{licence_file}' does not exist; "
                f"the licence text was never copied"
            )

        for flag in ("redistributeSource", "redistributeDerived", "commercialUse", "modification"):
            if not isinstance(origin.get(flag), bool):
                problems.append(f"{where}: origin.{flag} must be present and a JSON boolean")

        if packaging:
            # The shipping gate. Each of these is a separate refusal because each has a different
            # remedy, and "cannot ship" without saying why is a message that gets ignored.
            if licence == PROVENANCE_UNKNOWN:
                problems.append(f"{where}: PROVENANCE UNKNOWN cannot be packaged (ADR-0012)")
            if origin.get("redistributeDerived") is False:
                problems.append(
                    f"{where}: redistributeDerived is false, so the COMPILED asset cannot ship. "
                    f"{origin.get('note', '')}".rstrip()
                )
            if origin.get("redistributeSource") is False and origin.get("kind") != "downloaded":
                problems.append(f"{where}: redistributeSource is false for an asset we author")
    return problems


def third_party_rows(rows: list[dict]) -> list[dict]:
    """Downloaded sources AND their adaptations still need the upstream licence credit."""
    return [row for row in rows
            if (row.get("origin") or {}).get("kind") in {"downloaded", "derived"}]


def render(document: dict) -> str:
    rows = list(document.get("assets", []))

    # Third-party ONLY. Project-authored/generated assets are covered by its own licence, but
    # extracting or resampling a downloaded model/texture does not erase its upstream licence.
    # Excluding `derived` would leave the first CC BY furniture absent from the in-game credits.
    third_party = third_party_rows(rows)

    body: list[str] = []
    if not third_party:
        body.append("")
        body.append("*No third-party assets are present yet.*")
        body.append("")
        if rows:
            body.append(
                f"All {len(rows)} asset(s) in the manifest were authored or generated by this "
                f"project and are covered by the program's own licence."
            )
            body.append("")
    else:
        # Sorted by licence, then author, then id -- so a diff shows what changed rather than a
        # reordering (`licenses/README.md`, the generator's contract).
        third_party.sort(
            key=lambda r: (
                (r.get("origin") or {}).get("licence", ""),
                (r.get("origin") or {}).get("author", ""),
                r.get("id", ""),
            )
        )
        current = None
        for row in third_party:
            origin = row["origin"]
            licence = origin.get("licence", "")
            if licence != current:
                current = licence
                body.append("")
                licence_file = origin.get("licenceFile", "")
                if licence_file:
                    relative = Path(licence_file)
                    try:
                        link = relative.relative_to("licenses").as_posix()
                    except ValueError:
                        link = "../" + relative.as_posix()
                    body.append(f"## {licence}")
                    body.append("")
                    body.append(f"Full text: [`{licence_file}`]({link})")
                else:
                    body.append(f"## {licence}")
                body.append("")
            name = origin.get("name", row.get("id", ""))
            author = origin.get("author", "")
            url = origin.get("url", "")
            retrieved = origin.get("retrieved", "")
            parts = [f"**{name}**"]
            if author:
                parts.append(f"by {author}")
            if url:
                parts.append(f"— <{url}>")
            if retrieved:
                parts.append(f"(retrieved {retrieved})")
            body.append(f"- `{row.get('id','')}` — " + " ".join(parts))
            # CC0 carries no attribution requirement, so an empty attribution column is omitted
            # rather than printed as an empty field.
            attribution = origin.get("attribution", "")
            if attribution and not licence.upper().startswith("CC0"):
                body.append(f"  - Attribution: {attribution}")
        body.append("")

    unknown = [
        r for r in rows if (r.get("origin") or {}).get("licence") == PROVENANCE_UNKNOWN
    ]
    if unknown:
        body.append("---")
        body.append("")
        body.append("## Provenance unknown — NOT SHIPPABLE")
        body.append("")
        body.append(
            "These assets are present for development only. A packaging build refuses them "
            "(ADR-0012) and a development build stamps a visible watermark on every frame."
        )
        body.append("")
        for row in sorted(unknown, key=lambda r: r.get("id", "")):
            body.append(f"- `{row.get('id','')}` — `{row.get('sourceFile','')}`")
        body.append("")

    restricted = [
        r
        for r in rows
        if (r.get("origin") or {}).get("redistributeDerived") is False
        and (r.get("origin") or {}).get("licence") != PROVENANCE_UNKNOWN
    ]
    if restricted:
        body.append("---")
        body.append("")
        body.append("## Source is redistributable, compiled output is not")
        body.append("")
        body.append(
            "`redistributeSource` and `redistributeDerived` are separate booleans "
            "(`cna-house.md` §20.1) because they are separate questions. These rows can be "
            "committed but their **compiled** form cannot be shipped, and a packaging build "
            "refuses them."
        )
        body.append("")
        for row in sorted(restricted, key=lambda r: r.get("id", "")):
            note = (row.get("origin") or {}).get("note", "")
            body.append(f"- `{row.get('id','')}` — `{row.get('sourceFile','')}`")
            if note:
                body.append(f"  - {note}")
        body.append("")

    # The stamp names the manifest's own hash, so a stale document is detectable without rerunning
    # the generator. No timestamp: it would change on every run and make every diff noise.
    manifest_bytes = manifest_tool.MANIFEST.read_bytes() if manifest_tool.MANIFEST.exists() else b""
    stamp = hashlib.sha256(manifest_bytes).hexdigest()
    body.append("---")
    body.append("")
    body.append(f"*Generated from `assets-src/assets.manifest.json`, sha256 `{stamp}`.*")

    return HEADER + "\n".join(body) + "\n"


def selftest() -> int:
    fixture = {"assets": [
        {"id": "DOWNLOADED", "origin": {"kind": "downloaded", "licence": "CC0-1.0"}},
        {"id": "DERIVED", "origin": {"kind": "derived", "licence": "CC-BY-3.0",
                                      "name": "A sourced model", "author": "An artist",
                                      "attribution": "A sourced model by An artist"}},
        {"id": "AUTHORED", "origin": {"kind": "authored", "licence": "Ms-PL"}},
        {"id": "GENERATED", "origin": {"kind": "generated", "licence": "Ms-PL"}},
    ]}
    selected = [row["id"] for row in third_party_rows(fixture["assets"])]
    if selected != ["DOWNLOADED", "DERIVED"]:
        print(f"verify_licences: wrong third-party partition: {selected}", file=sys.stderr)
        return 1
    credits = render(fixture)
    if not all(mark in credits for mark in ("`DERIVED`", "A sourced model by An artist")):
        print("verify_licences: a sourced adaptation lost its attribution", file=sys.stderr)
        return 1
    if "`AUTHORED`" in credits or "`GENERATED`" in credits:
        print("verify_licences: project-owned rows polluted third-party credits", file=sys.stderr)
        return 1
    print("verify_licences: selftest passed (downloaded and derived credited)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--packaging", action="store_true", help="apply the shipping rules")
    parser.add_argument("--emit", action="store_true", help="write licenses/THIRD-PARTY-ASSETS.md")
    parser.add_argument("--check", action="store_true", help="fail if the document is stale")
    parser.add_argument("--selftest", action="store_true", help="check third-party credit selection")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    document = manifest_tool.load()

    # The schema comes first. Verifying licences on a manifest that does not parse would report the
    # second problem and hide the first.
    problems = manifest_tool.validate(document)
    problems += verify(document, packaging=args.packaging)
    if problems:
        print(f"verify_licences: {len(problems)} problem(s):", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1

    rendered = render(document)
    if args.emit:
        DOCUMENT.write_text(rendered, encoding="utf-8")
        print(f"verify_licences: wrote {DOCUMENT.relative_to(REPO)}")
        return 0
    if args.check:
        current = DOCUMENT.read_text(encoding="utf-8") if DOCUMENT.exists() else ""
        if current != rendered:
            print(
                "verify_licences: licenses/THIRD-PARTY-ASSETS.md is stale or hand-edited.\n"
                "  Regenerate it with tools/assets/verify_licences.py --emit and commit the result.",
                file=sys.stderr,
            )
            return 1
        print("verify_licences: the credits document matches the manifest.")
        return 0

    counts: dict[str, int] = {}
    for row in document.get("assets", []):
        licence = (row.get("origin") or {}).get("licence", "?")
        counts[licence] = counts.get(licence, 0) + 1
    mode = "packaging" if args.packaging else "development"
    print(f"verify_licences: {len(document.get('assets', []))} row(s) pass the {mode} rules.")
    for licence, count in sorted(counts.items()):
        print(f"  {count:3d}  {licence}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
