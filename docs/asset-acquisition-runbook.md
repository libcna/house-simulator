# Adding a third-party asset

`HOUSE-00301`. Every asset in this repository arrived the same way, and this is that way written
down so the next session does not have to reconstruct it from the tooling.

The rule the whole runbook exists to serve is `CLAUDE.md`'s: **never use an asset without provable
provenance.** Not "probably CC0", not "the search results said free" — a licence you can point at,
archived, with the words that grant the rights quoted verbatim.

---

## 0. Before anything: is it needed?

`cna-house.md` §20's budgets are per pack, and `tools/ci/budget_report.py` prints how much of each
is spoken for. `audio-core` was at 99.8 % of its 30 MB on 2026-09-07. An asset that will not fit is
not a decision to defer until after it is downloaded.

```bash
python3 tools/ci/budget_report.py            # what is left, per pack
```

---

## 1. The source's licence, in writing, before the download

One file per source in `docs/licence-evidence/`, following the shape of the ones already there
(`ambientcg.md` is the model). It records:

* the URL of the licence page **and** where it redirects to, because a licence that moved is a
  licence that can move again;
* the date retrieved;
* the publisher;
* the licence, and the path of the archived full text under `licenses/`;
* a **verdict** — `APPROVED`, `APPROVED WITH CAVEATS` (naming them), or `REJECTED`;
* the licence's own words, quoted, for the clauses the verdict rests on.

If the licence text is not already under `licenses/`, add it there as a directory holding
`LICENCE.txt`. A licence referenced but not archived is a licence that disappears when the site
does.

**Attribution-required is not a blocker; attribution-forgotten is.** The credits document is
generated (step 4), so a CC-BY asset is fine as long as its manifest row carries the attribution
string.

---

## 2. Download to `assets-src/<Category>/<group>/`

Sources live beside the assets, never in `/tmp` and never in the build tree. One directory per
coherent group, and each downloaded directory gets a `SOURCE.md` — the template is
[`asset-review/SOURCE-TEMPLATE.md`](asset-review/SOURCE-TEMPLATE.md) and
[`../assets-src/Fonts/SOURCE.md`](../assets-src/Fonts/SOURCE.md) is a worked example. It names
every manifest id in the directory, the upstream name and version, and the source. The
`verify_licences` gate reads it: a downloaded directory without one fails the build.

Large third-party *dependencies* are different and are covered by `../AGENTS.md` rule 4 — a shared
checkout under `~/deps/`, never a per-session clone.

---

## 3. A manifest row per asset

`assets-src/assets.manifest.json`. Every row carries its id, category, source file, the SHA-256 of
that file, the content name it compiles to, its residency pack, and an `origin` block with the
licence, the licence file, the attribution string and the four redistribution booleans —
`redistributeSource`, `redistributeDerived`, `commercialUse`, `modification`.

The hash is not decoration: it is what makes "the file we checked" and "the file in the tree" the
same claim.

```bash
python3 tools/assets/manifest.py --check      # schema, hashes, and no orphan files
```

---

## 4. Verify, then generate the credits

One tool does both on purpose: a generator that could run on unverified rows would produce a
credits page asserting licences nobody checked.

```bash
python3 tools/assets/verify_licences.py             # development rules
python3 tools/assets/verify_licences.py --packaging # shipping rules: refuses PROVENANCE UNKNOWN
python3 tools/assets/verify_licences.py --emit      # regenerate licenses/THIRD-PARTY-ASSETS.md
```

`licenses/THIRD-PARTY-ASSETS.md` is **generated** and a hand edit fails `--check`. It is what the
in-game credits screen shows.

---

## 5. The asset's own gates

Which apply depends on what it is, and all of them run in `tools/ci/run_checks.sh`:

| Asset | Gate | What it refuses |
|---|---|---|
| Model (`.glb`) | `gltf_validate.py` | a file the glTF validator rejects |
| Model | `scale_check.py` | a chair 2.4 m tall — §70.5's scale table, per category |
| Model | `origin_check.py` | a model whose origin is not where its category says |
| Model with a skin | `check_anim_assets.py` | more than one skin, or a sidecar binding nothing |
| Audio | `footstep_map.py` | a surface map that no longer matches the manifest |
| Font | `check_fonts.py` | a descriptor whose face is missing, or a repertoire gap |
| Anything | `budget_report.py` | a pack over its §20 budget |

A new *kind* of asset needs a new gate before it needs a second instance. The gates are the reason
461 assets can be trusted without re-reviewing them.

---

## 6. Record the working, not just the result

`docs/asset-selection/` holds the "why this one" for choices that had alternatives, and
`docs/asset-review/` the sign-offs for the hero assets. A rejected candidate is worth a line: the
next session that finds the same asset and wonders why it is not here should find the answer rather
than repeat the evaluation.

---

## What this looks like when it goes wrong

Two failures this project has actually had, kept here because they are the ones to expect:

* **A licence that says one thing on the store page and another in the download.** The evidence
  file quotes the page; the download's own `LICENSE.txt` is what ships. Check both, and record the
  discrepancy rather than picking the convenient one.
* **A pack that fits until it does not.** `audio-core` reached 99.8 % of 30 MB with the ambience
  beds alone. Run `budget_report.py` **before** the download, not after — a 40 MB folder that has
  to be culled is an hour spent choosing what to delete.
