# ADR-0012 — Asset licensing policy, and the "no row, no build" rule

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-06 |
| **Task** | `HOUSE-00018` |
| **Owns** | `cna-house.md` §19, §20 |

## Context

A furnished 935 m² house needs a large asset library: models, textures, audio, fonts and video,
most of it sourced externally. Assets arrive one at a time, often quickly, often from an aggregator
that restates a licence rather than carrying it. The failure mode is well known and always the
same: six months later nobody can say where a particular chair came from, and the project cannot
ship.

Attribution has the same shape. A credits screen maintained by hand drifts from the asset set
within weeks.

## Decision

Provenance is a **build-enforced property of every source file**, and attribution is **generated**.

1. **No row, no build.** Every file under `assets-src/` has a row in
   `assets-src/assets.manifest.json`. `tools/ci/check_manifest.py` fails the build on any unlisted
   file. There is no "temporary" exemption.
2. **Every row carries the SHA-256 of the source file.** Changing a source file without updating
   its row fails the build, so a row cannot silently come to describe a different asset.
3. **Licence texts are copied verbatim**, never linked: `licenses/<slug>/LICENCE.txt`, beside a
   `source.url.txt` and a `retrieved.txt` timestamp. Upstream pages disappear; our copy does not.
4. **`licenses/THIRD-PARTY-ASSETS.md` is generated** from the manifest and shown in-game from the
   credits screen. Attribution is therefore impossible to forget and impossible to let drift.
5. **Redistribution is decided per asset, not per source.** `redistributeSource` and
   `redistributeDerived` are separate booleans, as are `commercialUse` and `modification`. A
   CC-BY-ND asset would have `redistributeDerived = false` and so could not be decimated for LODs —
   which means it would not be used at all, and the manifest says so before anyone spends an
   evening on it.
6. **Unknown provenance is loud, not fatal.** An asset without provable provenance is marked
   `PROVENANCE UNKNOWN — DO NOT SHIP`. It may sit in `assets-src/` as a development placeholder,
   the packaging target refuses to include it, and a build containing one stamps a **visible
   watermark on every frame**. Nobody can forget it is there.
7. **Nothing is asserted as verified until it has been verified.** No licence in `cna-house.md`
   §19.2 was checked during planning — network access was not used — so every source in that table
   is a *candidate carrying an expectation*. Phase 4 contains one research task per source and one
   per hero asset, each of which must record the actual licence text, URL, author and retrieval
   date before the asset is used.
8. **No identifiable real private people** in photographs or artwork (`cna-house.md` §59.4).

## Alternatives considered

**A spreadsheet or a wiki page of asset credits.** Rejected. Nothing enforces it, so it is wrong
within a month, and being wrong about a licence is the one kind of wrong this project cannot
absorb.

**Record the licence only for assets that ship.** Rejected — the distinction is not knowable in
advance, and a placeholder has a way of becoming a shipped asset without ceremony.

**Trust the aggregator's licence label.** Rejected. Aggregators restate licences and are sometimes
wrong. The rule is to record the licence *text* from the authoritative source, with the URL and the
retrieval date, so a later reader can check what we saw.

**Permit "CC-BY, attribution pending".** Rejected. Attribution is generated from the manifest; a
row without attribution data produces a build failure rather than a credits line reading
"pending".

**A blanket "CC0 only" policy.** Considered and rejected as *too* restrictive: CC-BY assets are
usable given generated attribution, which we have. The restriction that stays is on ND and NC
terms, which conflict with LOD generation and with the project's licence respectively — expressed
as per-asset booleans rather than as a source-level ban.

## Consequences

* Adding an asset is a small ritual: fetch, hash, write the row, copy the licence text if the slug
  is new. `tools/assets/fetch_asset.py` performs it so the ritual is one command.
* `verify_licences.py` and `check_manifest.py` run in CI on every commit, not only at packaging
  time, so a missing row is caught by the person who caused it.
* The credits screen is a view of the manifest, so it is correct by construction.
* The 1 644-file NOX audio collection at `/rv/tmp/Essentials_Series_NOX_SOUND/`, declared CC0 by
  its own bundled README, is treated like any other source: its licence text is copied into
  `licenses/`, and each imported file gets a row carrying both the original and the converted
  (16-bit PCM, BL-06) hashes.
