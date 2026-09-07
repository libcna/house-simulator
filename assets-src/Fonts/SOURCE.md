# `assets-src/Fonts/` — provenance

Template and contract: [`docs/asset-review/SOURCE-TEMPLATE.md`](../../docs/asset-review/SOURCE-TEMPLATE.md).
The full working for this directory — every check, every rejected alternative — is
[`docs/font-provenance.md`](../../docs/font-provenance.md); this file is the summary the gate reads.

## Assets

| Manifest id | File | Upstream name / version |
|---|---|---|
| `FONT_NOTO_SANS_REGULAR` | `NotoSans-Regular.ttf` | Noto Sans Regular, release `NotoSans-v2.015` |
| `FONT_NOTO_SANS_MONO_REGULAR` | `NotoSansMono-Regular.ttf` | Noto Sans Mono Regular, release `NotoSansMono-v2.014` |

The five `.spritefont` descriptors beside them are **authored here**, not downloaded, and are listed
in the manifest as such. They rasterise these two faces; the glyph data in the compiled
`Fonts/*.cnb` is therefore derived from OFL-licensed Font Software and is covered by the same terms.

## Source

| | |
|---|---|
| Publisher / author | The Noto Project Authors (the Noto project's own `notofonts` GitHub organisation) |
| Canonical URL | `https://github.com/notofonts/latin-greek-cyrillic/releases/download/NotoSans-v2.015/NotoSans-v2.015.zip` |
| | `https://github.com/notofonts/latin-greek-cyrillic/releases/download/NotoSansMono-v2.014/NotoSansMono-v2.014.zip` |
| Retrieved | 2026-09-07 |
| Upstream release / tag | `NotoSans-v2.015` (2024-11-20) · `NotoSansMono-v2.014` (2023-09-30) |
| Second source cross-checked? | Yes — `https://raw.githubusercontent.com/notofonts/notofonts.github.io/main/fonts/…/hinted/ttf/…` . **Byte-identical to the release-zip members for both files.** Two official distribution points agree on exactly these bytes. |

This is upstream, not an aggregator restating a licence. The two families carry different version
numbers because upstream releases them independently from the same repository.

SHA-256 of each file is in `assets-src/assets.manifest.json` and is enforced by
`tools/ci/check_manifest.py`; it is deliberately not duplicated here, where it could drift.

## Licence

| | |
|---|---|
| Licence | `OFL-1.1` — SIL Open Font License, Version 1.1 |
| Archived at | `licenses/ofl-1.1/LICENCE.txt` |
| How it was established | Three independent checks, not one. (1) The licence body in the release's `OFL.txt` is identical, whitespace-normalised, to SIL's own published text at `https://openfontlicense.org/documents/OFL.txt`. (2) `OFL.txt` is byte-identical between the two release zips, so one archived copy correctly serves both. (3) `name` ID 13 inside **both binaries** reads "This Font Software is licensed under the SIL Open Font License, Version 1.1", with ID 14 pointing at `https://scripts.sil.org/OFL` — so the claim does not rest on the zip alone. |
| Redistribute source | **yes** — OFL clause 1, with the copyright notice and licence included, which `licenses/ofl-1.1/` and `NOTICE.md` do |
| Redistribute derived / compiled | **yes** — the rasterised `.cnb` atlas is a derived work; the OFL permits derivation and redistribution on the same notice terms |
| Commercial use | **yes** — SIL FAQ 1.4 permits selling a software package containing OFL fonts and names "games and entertainment software" among its examples. The one prohibition, clause 1, is on selling the fonts **by themselves**; a game is not that. |
| Modification | **permitted, and not exercised** |
| Attribution required | `Copyright 2022 The Noto Project Authors (https://github.com/notofonts/latin-greek-cyrillic)` — carried in the manifest, emitted into `licenses/THIRD-PARTY-ASSETS.md`, and shown on the in-game credits screen |
| Other obligations | The copyright statement, licence notice and licence text must travel with any distribution (OFL clause 2; FAQ 1.20). **A packaging step that omits `licenses/` would breach the OFL** — that is the one way this can be broken by accident. No Reserved Font Name is declared, so clause 5 restricts nothing: the phrase appears in the archived `OFL.txt` only inside the licence's own definitions, never after the copyright statement, and FAQ 5.7 confirms no names are reserved by default in 1.1. |

**The OFL does not reach the rest of this project.** It says so itself — "The requirement for fonts
to remain under this license does not apply to any document created using the Font Software" — and
FAQ 1.3 and 1.13 confirm that bundling one with a program makes neither the program nor anything
drawn with it subject to the OFL. `cna-house` remains Ms-PL.

**Trademark, which is a separate question from licence.** `name` ID 7 in both files reads "Noto is a
trademark of Google LLC." That is unaffected by the OFL. It does not bite here because the files are
shipped unmodified and "Noto" is not used as this product's branding — only as the factual name of
the upstream files, here and in the credits.

## Modification

| | |
|---|---|
| Modified from upstream? | **no** — both files are byte-for-byte the upstream release artifacts |
| If yes, what and why | n/a |
| Conversion history | none. No subsetting, no format conversion, no static font manufactured from a variable one. The only choice made was *which* officially provided static TTF to commit. |

## Notes

**Rejected alternatives, recorded because the reasoning is not obvious.**

* `unhinted/ttf/` — rejected on measurement. It and `hinted/` carry **identical outlines** (0 of
  3 884 glyphs differ) and identical `hmtx`; they differ only in the four hinting tables and the
  hinting programs of 2 995 glyphs. Compiling the same descriptor against each produced *different*
  `.cnb` bytes, which proves FreeType's bytecode interpreter is live in this toolchain and that
  upstream's ttfautohint instructions are genuinely consumed. At 13–30 px, which is the whole range
  used here, that is the difference between aligned stems and mush. `unhinted/` would hand the job
  to FreeType's autohinter — a guess, where upstream shipped a tested answer.
* `full/ttf/` — rejected as 631 glyphs beyond the repertoire this project rasterises: 204 KB of
  repository weight that never reaches a pixel.
* The variable fonts — rejected: `HOUSE-00200` calls for deterministic static TTFs, and a variable
  source would add instancing behaviour to the content pipeline for no benefit at five fixed sizes.
* The host's installed fonts — rejected by policy and now by a gate. This machine carries Noto Sans
  **2.004** and Noto Sans Mono **2.006** under exactly our filename stems; the importer's fallback
  to them is a *warning*, not an error, so `tools/ci/check_fonts.py` fails a descriptor whose font
  file is missing.

**Noto Sans Mono has no glyph for U+00AD**, and the content processor treats a requested-but-absent
character as a hard build failure. The shipped repertoire is therefore ASCII plus Latin-1 *minus
soft hyphen* — 190 characters — for both faces.
