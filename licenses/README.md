# `licenses/`

Upstream licence texts, verbatim, plus the generated attribution document.

```
licenses/
├── README.md                     this file — the generator's contract
├── THIRD-PARTY-ASSETS.md         GENERATED. Do not edit.
└── <slug>/                       one directory per distinct licence, e.g. cc0-1.0/, cc-by-4.0/
    ├── LICENCE.txt               the upstream text, copied verbatim, never summarised
    ├── source.url.txt            where that text was obtained
    └── retrieved.txt             ISO date of retrieval
```

A slug is lower-case, hyphenated, and is the SPDX identifier where one exists: `cc0-1.0`,
`cc-by-4.0`, `ofl-1.1`, `ms-pl`, `apache-2.0`. Where no SPDX identifier exists, the slug is the
licence's own name, slugified, and `source.url.txt` is what makes it identifiable.

**Licence texts are copied, never linked.** Upstream pages disappear, get edited, and get
reorganised; our copy does not. `retrieved.txt` is what lets a later reader tell whether they are
looking at the same text we were.

---

## The generator's contract

`licenses/THIRD-PARTY-ASSETS.md` is produced by `tools/assets/verify_licences.py --emit` from
`assets-src/assets.manifest.json`. It is **generated output committed to the repository**, because
it is displayed in-game from the credits screen and must exist in a checkout that has not run the
tooling.

**Inputs**

* `assets-src/assets.manifest.json` — every asset row (`cna-house.md` §20.3).
* `licenses/<slug>/` — the licence texts the rows reference.

**Output**

`licenses/THIRD-PARTY-ASSETS.md`, deterministic byte-for-byte for a given manifest:

* rows sorted by `origin.licence`, then `origin.author`, then asset `id` — so a diff shows what
  actually changed rather than a reordering;
* grouped by licence, each group headed by the licence name and a link to its `LICENCE.txt`;
* one line per asset: id, human name, author, source URL, retrieval date;
* CC0 rows omit the attribution field, which is empty by definition, rather than printing an empty
  column;
* a trailing generation stamp naming the manifest's own hash, so a stale file is detectable.

**Failure conditions** — each one fails the build rather than producing a partial document:

| Condition | Why it is fatal |
|---|---|
| A manifest row has no `origin.licence` | The asset cannot be shipped or credited |
| `origin.licenceFile` names a path that does not exist | The licence text was never copied |
| A row is marked `PROVENANCE UNKNOWN` and this is a **packaging** build | Unshippable ([ADR-0012](../docs/decisions/ADR-0012-asset-licensing.md)) |
| A file under `assets-src/` has no manifest row | **No row, no build** |
| A row's `sourceSha256` does not match the file on disk | The row no longer describes the asset |

A development build tolerates `PROVENANCE UNKNOWN`, stamps a **visible watermark on every frame**,
and lists the offending assets on the credits screen so that nobody can forget they are there.

**Verification.** `tools/ci/check_manifest.py` and `tools/assets/verify_licences.py` run in CI on
every commit. `verify_licences.py --check` regenerates the document into a buffer and fails if it
differs from the committed file, so the committed copy can never drift from the manifest.

---

## Adding a licence

1. Copy the licence text verbatim into `licenses/<slug>/LICENCE.txt`.
2. Write `source.url.txt` (one URL) and `retrieved.txt` (one ISO date).
3. Point the manifest rows at it with `"licenceFile": "licenses/<slug>/LICENCE.txt"`.
4. Regenerate: `tools/assets/verify_licences.py --emit`.
5. Commit the licence directory, the manifest change and the regenerated document together.
