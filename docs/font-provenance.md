# Font provenance — the two vendored Noto faces

`HOUSE-00200`. This is the evidence file for the two font binaries committed under
`assets-src/Fonts/`. It exists so that a future developer preparing a commercial package — Steam or
otherwise — does not have to re-derive any of it, and does not have to trust a web page that may no
longer say what it said on the retrieval date.

The machine-readable truth is `assets-src/assets.manifest.json`. This document is the working, and
the parts of the working that a JSON field cannot hold.

**Retrieval date for everything below: 2026-09-07.**

---

## 1. What was chosen, and its identity

| | Noto Sans | Noto Sans Mono |
|---|---|---|
| Committed as | `assets-src/Fonts/NotoSans-Regular.ttf` | `assets-src/Fonts/NotoSansMono-Regular.ttf` |
| Manifest row | `FONT_NOTO_SANS_REGULAR` | `FONT_NOTO_SANS_MONO_REGULAR` |
| Upstream release tag | `NotoSans-v2.015` | `NotoSansMono-v2.014` |
| Release date | 2024-11-20 | 2023-09-30 |
| Member extracted | `NotoSans/hinted/ttf/NotoSans-Regular.ttf` | `NotoSansMono/hinted/ttf/NotoSansMono-Regular.ttf` |
| Size | 621 572 bytes | 596 428 bytes |
| **SHA-256** | `478c558ea716033cd60c03438f628dfa75694dcf6b5f6d505a2f05fd2b4f3823` | `65b5e2b2c4a1fba9ae8be1f026cb35b03dcb8886d9b2a4147054fde12f7e767d` |
| `name` ID 5 | `Version 2.015; ttfautohint (v1.8.4.7-5d5b)` | `Version 2.014; ttfautohint (v1.8.4.7-5d5b)` |
| Glyphs | 3 884 | 3 920 |
| Units per em | 1 000 | 1 000 |
| **Modified?** | **No.** Byte-for-byte the upstream artifact. | **No.** Byte-for-byte the upstream artifact. |

The two families are at different version numbers because upstream releases them independently from
the same repository. That is not a mistake here, and pinning both to one number would mean shipping
something upstream never released.

### Canonical source URLs

```
https://github.com/notofonts/latin-greek-cyrillic/releases/download/NotoSans-v2.015/NotoSans-v2.015.zip
https://github.com/notofonts/latin-greek-cyrillic/releases/download/NotoSansMono-v2.014/NotoSansMono-v2.014.zip
```

`notofonts` is the Noto project's own GitHub organisation, and `latin-greek-cyrillic` is the
repository that owns these two families. This is upstream, not an aggregator restating a licence.

### Cross-checked against a second official distribution point

The same two paths are also published in the repository that serves <https://notofonts.github.io>:

```
https://raw.githubusercontent.com/notofonts/notofonts.github.io/main/fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf
https://raw.githubusercontent.com/notofonts/notofonts.github.io/main/fonts/NotoSansMono/hinted/ttf/NotoSansMono-Regular.ttf
```

Both were downloaded and hashed. **Both are byte-identical to the release-zip members**, so two
independent official distribution points agree on exactly these bytes. That is stronger evidence of
identity than either source alone, and it is the reason the hashes above can be treated as the
identity of "Noto Sans 2.015 Regular, hinted" rather than merely as the hash of a file someone
downloaded once.

---

## 2. Licence

**SIL Open Font License, Version 1.1.** SPDX: `OFL-1.1`.

The text is archived verbatim at `licenses/ofl-1.1/LICENCE.txt`, with `source.url.txt` and
`retrieved.txt` beside it, per `licenses/README.md`.

Three things were checked rather than assumed:

1. **The archived text is the real OFL 1.1.** The licence body in the Noto `OFL.txt` was compared,
   whitespace-normalised, against SIL's own published text at
   <https://openfontlicense.org/documents/OFL.txt>. **They are identical** from `PREAMBLE` onward.
   The only difference is the copyright line above it, which is where the OFL is designed to carry
   the licensor's own notice.
2. **The two families ship the same licence file.** `OFL.txt` in the `NotoSans-v2.015` zip and in
   the `NotoSansMono-v2.014` zip are byte-identical (`cee9892f9f0cc8fe882c9e9537ee6a89621d86ee7ceaf70b02e2b2b1c25c061a`),
   so one `licenses/ofl-1.1/` directory correctly serves both rows.
3. **The fonts say so themselves.** `name` ID 13 in both files reads "This Font Software is licensed
   under the SIL Open Font License, Version 1.1", and ID 14 points at <https://scripts.sil.org/OFL>.
   The licence claim therefore does not rest on the zip's `OFL.txt` alone; it is inside the binaries
   being shipped.

### The copyright notice, verbatim

```
Copyright 2022 The Noto Project Authors (https://github.com/notofonts/latin-greek-cyrillic)
```

### Reserved Font Names: none

The string "Reserved Font Name" appears in the archived `OFL.txt` **only inside the licence's own
definitions section** — never after the copyright statement, which is where OFL 1.1 requires an RFN
to be declared. Per SIL's FAQ 5.7, no names are reserved by default in version 1.1. So clause 5
places no naming restriction on this project.

### Trademark, which is a separate question from licence

`name` ID 7 in both files reads **"Noto is a trademark of Google LLC."** A trademark is not affected
by the OFL (FAQ 3.7 addresses this for modified fonts). It does not bite here for two reasons: the
files are shipped **unmodified**, and "Noto" is not used as branding for this product — it appears
only as the factual name of the upstream files, in this document and in the credits.

### Rights, decided per the four manifest booleans

| Question | Answer | Basis |
|---|---|---|
| Redistribute the source `.ttf`? | **Yes** | OFL 1.1 clause 1, with the copyright and licence included — which `licenses/ofl-1.1/` and `NOTICE.md` do. |
| Redistribute the compiled `.cnb`? | **Yes** | The rasterised atlas is a derived work of the Font Software; the OFL permits derivation and redistribution, and the notice obligation is met the same way. |
| Commercial use, including a paid Steam release? | **Yes** | FAQ 1.4, verbatim: "Can I sell a software package that includes these fonts? Yes… Examples of bundling made possible by the OFL would include: … **games and entertainment software** …". The single prohibition, clause 1, is on selling the fonts *by themselves*; a game is not that. |
| Modification? | **Permitted, and not exercised.** | No RFN is declared, so even a renamed derivative would be allowed. Nothing was modified, which keeps the question academic. |

### What the OFL does **not** do

It does not reach the rest of this project. OFL 1.1 says so in its own conditions — *"The
requirement for fonts to remain under this license does not apply to any document created using the
Font Software"* — and FAQ 1.3 states plainly that bundling an OFL font with a program does not
require the program to be free software, while FAQ 1.13 states that embedding one changes the
licence of neither the document nor artwork made with it.

`cna-house` is Ms-PL. The OFL governs the two `.ttf` files and the glyph data derived from them, and
nothing else. `NOTICE.md` says this in the form a distribution reads.

### Obligations this project must keep meeting

Per OFL clause 2 and FAQ 1.20, a bundled font must travel with its copyright statement, licence
notice and licence text. All three are satisfied and are load-bearing, not decorative:

* `licenses/ofl-1.1/LICENCE.txt` — the full text, including the copyright line;
* `NOTICE.md` — the notice in the place a distribution looks for one;
* `licenses/THIRD-PARTY-ASSETS.md` — generated from the manifest and shown on the in-game credits
  screen, so the attribution cannot silently drift away from the assets actually shipped.

**A packaging step that omits `licenses/` would breach the OFL.** That is the one way this
arrangement can be broken by accident, and it is written here because it is not otherwise obvious.

---

## 3. Why the *hinted* static TTF, and not the other officially provided ones

The `NotoSans-v2.015` release provides three officially built static Regular TTFs — `unhinted/`,
`hinted/` and `full/` — plus variable fonts. No font was manufactured, converted or subsetted here;
the choice was only *which official artifact to commit*, so nothing in this section creates a
"Modified Version" under the OFL.

**`unhinted/` was rejected on measured grounds, not on preference.** The hinted and unhinted builds
were compared table by table:

* **identical outlines** — 0 of 3 884 glyphs differ in coordinates;
* **identical metrics** — `hmtx` matches for every glyph;
* the difference is exactly the four hinting tables (`cvt `, `fpgm`, `gasp`, `prep`) and the hinting
  programs of 2 995 glyphs.

Compiling the same descriptor against each produced **different `.cnb` bytes**. Since the outlines
are identical, that difference can only come from the hinting instructions — which proves FreeType's
TrueType bytecode interpreter is live in this toolchain and that the project's own ttfautohint
instructions are genuinely being consumed. At 13–30 px, which is the entire range these five assets
use, that is the difference between aligned stems and mush. Choosing `unhinted/` would instead hand
the job to FreeType's autohinter — a guess, where upstream shipped a tested answer.

**`full/` was rejected** because it is the same hinted font plus 631 glyphs outside the repertoire
this project rasterises: 204 KB of repository weight that never reaches a pixel.

Noto Sans Mono publishes no `full/` tree; its `googlefonts/ttf` and `hinted/ttf` members are the
same size, and `hinted/` was taken for symmetry with the UI face.

---

## 4. Measured facts that the descriptors depend on

Each of these was measured during `HOUSE-00200` and is enforced somewhere, so that a future change
that invalidates it fails a check rather than surprising someone.

### 4.1 The repository file wins, and the fallback is silent

`FontDescriptionImporter` resolves `<FontName>` to a file beside the `.spritefont` first — trying
the bare name, then `.ttf`, `.otf`, `.ttc` — and only then searches `/usr/share/fonts` and friends
by filename stem. There is no Fontconfig involvement and no family-name lookup.

**The fallback emits a warning and lets the build succeed.** This machine carries
`/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf` at version **2.004** and
`NotoSansMono-Regular.ttf` at **2.006** — the stems collide exactly with ours. Removing the vendored
files and rebuilding produced five successful outputs, five warnings, and **five different hashes**.

That is why `tools/ci/check_fonts.py` exists and why it is a gate: a warning is not a defence.

### 4.2 The compiled output does not depend on the host's fonts

The `Fonts` root was built twice: normally, and inside a mount namespace with `/usr/share/fonts` and
`/usr/local/share/fonts` bind-mounted over by an empty directory (0 font files visible).

```
0a5a565b77603fabe8817db36b81fa4c264260821b2351c0305a34019bfa9f3c  ui-30.cnb
2c514a9e49dd61ab13685a99f2d579ad8d721ce3878ccc9e998f5c851a084986  mono-16.cnb
42f98785b36af02f3607a91a8538ed47daf347220f22b7de98e46f4e79f931cf  ui-16.cnb
b9349879db74e840c69e342372e62f0a979f99125954693a94e43cb5b033cfd8  mono-13.cnb
cadac49fae1a9cece64c9e58d7d31781118323dd6f333719372562102334570a  ui-22.cnb
```

**All five identical in both builds**, and identical again under `tools/ci/content_verify.py`. With
the vendored files removed, all five change. The repository TTF is the source of truth.

No host-dependent string reaches the output either: the only printable string in the five `.cnb`
files is the type name `Microsoft.Xna.Framework.Graphics.SpriteFont`. No path, no font filename, no
user name.

### 4.3 `<Size>` is POINTS AT 96 DPI, not pixels

`FT_Set_Char_Size(face, 0, size × 64, 96, 96)`. So the em box in pixels is `size × 96/72`, and the
five assets are really:

| Asset | `<Size>` | Em box | `LineSpacing` |
|---|---|---|---|
| `ui-16` | 16 | 21.33 px | 29 px |
| `ui-22` | 22 | 29.33 px | 40 px |
| `ui-30` | 30 | 40.00 px | 54 px |
| `mono-13` | 13 | 17.33 px | 24 px |
| `mono-16` | 16 | 21.33 px | 29 px |

`cna-house.md` §67.1 describes these as "a UI face at 16/22/30 **px** and a monospace face at 13/16
**px**". The numbers are right and the unit is not: they are the `.spritefont` `<Size>` values, which
is the XNA convention, and the rendered text is about a third larger than "px" suggests. Anyone
laying out HUD elements against §67.2's 1600 × 900 virtual canvas should size against the
`LineSpacing` column above rather than against the `<Size>` number.

### 4.4 Noto Sans Mono has no U+00AD, and a missing glyph is fatal

`FontDescriptionProcessor` **fails the build** — not warns — when a `<CharacterRegion>` asks for a
character the face cannot draw. Noto Sans Mono 2.014 carries no glyph for U+00AD SOFT HYPHEN.

The repertoire is therefore U+0020–U+007E plus U+00A0–U+00FF **minus U+00AD**: 190 characters. Soft
hyphen is an invisible line-break hint, there is no line-breaking engine here to honour one, and a
`SpriteFont` has nothing to draw for it. Both faces declare the same 190 so they stay
diff-comparable, even though Noto Sans could carry the extra glyph.

### 4.5 The mono face is monospaced — exactly at 16, to within a pixel at 13

Every one of the 190 characters advances exactly 600/1000 em in the source font, and
`post.isFixedPitch` is nonetheless **0** in both files, so the metadata would mislead anyone who
read it.

After rasterisation the advance is taken as `slot->advance.x >> 6` — truncated to whole pixels after
grid-fitting. At `<Size>16` the advance is 0.6 × 21.33 = **12.8 px**, which lands on 13 for all 190.
At `<Size>13` it is 0.6 × 17.33 = **10.4 px**, which cannot be whole: **176 characters measure 10 px
and 14 measure 11 px.**

So debug-overlay columns are exact at `mono-16` and out by at most one pixel at `mono-13`.
`FontMetricsTests.MonoAdvancesAreUniformToWithinTheGridFittingError` asserts exactly that — spread 0
at 16, spread ≤ 1 at 13 — rather than a loose tolerance that a genuinely proportional face could
also pass.

---

## 5. What is verified, and by what

| Claim | Verified by |
|---|---|
| Every descriptor resolves to a repository file, never a system font | `tools/ci/check_fonts.py` (a CI gate) |
| Every descriptor declares the full 190-character repertoire | `tools/ci/check_fonts.py` |
| The vendored face can actually draw everything asked of it | `tools/ci/check_fonts.py` |
| Every source file has a manifest row and a matching hash | `tools/ci/check_manifest.py` |
| The licence, licence file and the four rights booleans are present | `tools/assets/verify_licences.py` |
| The credits document matches the manifest | `verify_licences.py --check` |
| The compiled output is byte-identical across rebuilds | `tools/ci/content_verify.py` |
| The five fonts load and carry the 190 characters | `FontMetricsTests` (integration) |
| `MeasureString` is sane; the sizes are distinct and ordered | `FontMetricsTests` |
| The mono face is monospaced to the measured tolerance | `FontMetricsTests` |
| The glyph atlas actually renders ink, and Latin-1 is not the `?` fallback | `FontRenderTests` (render, nightly) |

Each of those tests was run against a deliberately introduced fault and shown to fail before being
trusted: a proportional face given to the monospace assertion, a repertoire including U+00AD, a
character outside the declared regions given to the Latin-1 render check, and a deleted vendored TTF
given to the gate.

---

## 6. Known limitation: FreeType version, not font version

Same-machine and same-toolchain reproducibility is proven above. **Cross-*toolchain*
reproducibility is not**, and cannot be by vendoring a font: the compiled atlas is what a particular
FreeType rasterised, and FreeType's hinting and rasterisation change between releases.

This build used **FreeType 2.13.3** (`cna-content` reports it at configure time: "SpriteFont source
pipeline enabled (AUTO; FreeType 2.13.3)").

This is a much smaller exposure than the one `HOUSE-00200` closed — a FreeType upgrade shifts
antialiasing slightly, where the old system-font fallback substituted a different typeface at a
different version — and it affects no licence or provenance claim. It is recorded so that a future
byte-difference in `Fonts/*.cnb` is diagnosed as a toolchain change rather than misread as asset
drift. If cross-toolchain byte-identity is ever required, the FreeType version has to be pinned
alongside the fonts.
