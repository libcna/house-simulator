# `SOURCE.md` template

`HOUSE-00261`. Copy this into any `assets-src/` directory that holds a **downloaded** asset, and
fill it in. `tools/assets/verify_licences.py` fails the build when a directory containing a
downloaded asset has no `SOURCE.md`, or when one exists but does not mention every downloaded asset
in that directory by its manifest id.

**This file is not a substitute for the manifest, and not a duplicate of it.** The manifest is the
machine-readable truth — hashes, the four rights booleans, the licence file — and it is what the
gates and the credits generator read. `SOURCE.md` is the part a JSON field cannot hold: *how* the
licence was established, *what was checked*, *what was rejected and why*, and anything a later
reader would otherwise have to rediscover. If the two ever disagree, the manifest is authoritative
about facts and `SOURCE.md` is authoritative about reasoning.

**Never equate "free download" with "redistributable."** Record the licence you read on the
publisher's own page, not the label an aggregator put on a listing, and archive the licence text
under `licenses/<slug>/` rather than linking to it. If the terms are unclear, the asset is unusable
until they are resolved — say so here and mark the row `PROVENANCE UNKNOWN — DO NOT SHIP`.

---

Everything below the line is the template. Delete this header when you copy it.

---

# `assets-src/<Directory>/` — provenance

## Assets

One row per downloaded asset in this directory. The id must match the manifest exactly; that is
what the gate checks.

| Manifest id | File | Upstream name / version |
|---|---|---|
| `CATEGORY_NAME` | `file.ext` | |

## Source

| | |
|---|---|
| Publisher / author | |
| Canonical URL | |
| Retrieved | *(ISO date)* |
| Upstream release / tag / version | |
| Second source cross-checked? | *(URL, and whether the bytes matched)* |

## Licence

| | |
|---|---|
| Licence | *(SPDX id where one exists)* |
| Archived at | `licenses/<slug>/LICENCE.txt` |
| How it was established | *(the publisher page read, the file inspected, the metadata field — be specific)* |
| Redistribute source | yes / no |
| Redistribute derived / compiled | yes / no |
| Commercial use | yes / no |
| Modification | yes / no |
| Attribution required | *(the exact string, or "none")* |
| Other obligations | *(share-alike, notice placement, name restrictions, trademark)* |

## Modification

| | |
|---|---|
| Modified from upstream? | **no** / **yes** |
| If yes, what and why | |
| Conversion history | *(format changes, resampling, decimation — each one is a derivation)* |

## Notes

*What was checked, what was rejected and why, and anything that would otherwise have to be
rediscovered. Negative findings belong here too: a variant considered and dropped is worth more to
the next reader than a bare statement of what was chosen.*
