# SIL OFL 1.1 — FAQ evidence

Supporting evidence for the OFL-1.1 decision recorded in `assets-src/Fonts/SOURCE.md` and
`docs/font-provenance.md`. The licence text itself is archived at `licenses/ofl-1.1/LICENCE.txt`;
this file holds the FAQ passages the reasoning relies on.

| | |
|---|---|
| Source | <https://openfontlicense.org/ofl-faq/> |
| Document version | **OFL-FAQ version 1.1-update7 (November 2023)** |
| Publisher | SIL International — the licence's author and steward |
| Retrieved | 2026-09-07 |
| Copyright of the FAQ | © 2007–2023 SIL International. "Everyone is permitted to copy and distribute verbatim copies of this license document, but changing it is not allowed." |

The FAQ is not itself the licence and does not bind a court. It is the licensor's own published
interpretation, which is the best available evidence of intent and is what SIL directs licensees to
read. Quotes below are verbatim; the "what this means here" lines are this project's reading and are
kept separate so the two are never confused.

---

## 1.3 — Bundling does not make the program open source

> **I want to distribute the fonts with my program, does this mean my program also has to be
> Free/Libre and Open Source Software?**
>
> No. Only the portions based on the Font Software are required to be released under the OFL. The
> intent of the license is to allow aggregation or bundling with software under restricted
> licensing as well.

*What this means here:* `cna-house` stays Ms-PL. The OFL reaches the two `.ttf` files and the glyph
data derived from them, and nothing else.

## 1.4 — Selling a package containing the fonts, games named explicitly

> **Can I sell a software package that includes these fonts?**
>
> Yes, you can do this with both the Original Version and a Modified Version of the fonts. Examples
> of bundling made possible by the OFL would include: text editors, word processors, design and
> publishing applications, training and educational software, **games and entertainment software**,
> mobile device applications, etc.

*(Emphasis added.)*

*What this means here:* a paid Steam release is permitted. The prohibition in OFL clause 1 is on
selling the **Font Software by itself**; a game that bundles it is the case this answer describes.

## 1.13 — Embedding changes the licence of nothing else

> **Does embedding alter the license of the document itself?**
>
> No. Referencing or embedding an OFL font in any document does not change the license of the
> document itself. The requirement for fonts to remain under the OFL does not apply to any document
> created using the fonts and their derivatives. Similarly, creating any kind of graphic using a
> font under the OFL does not make the resulting artwork subject to the OFL.

*What this means here:* screenshots, trailers and store art containing text set in Noto are not
encumbered. The compiled `Fonts/*.cnb` atlas is treated more conservatively — as a derivative of the
Font Software rather than as mere artwork — which costs nothing, because the OFL permits derivation
and redistribution anyway and the notice obligation is met either way.

## 1.20 — The minimum that must travel with a bundled font

> **I'm writing a small app for mobile platforms, do I need to include the whole package?**
>
> If you bundle a font under the OFL with your mobile app you must comply with the terms of the
> license. At a minimum you must include the copyright statement, the license notice and the license
> text. […] You do not, however, need to include the full contents of the font package - only the
> fonts you use and the copyright and license that apply to them. For example, if you only use the
> regular weight in your app, you do not need to include the italic and bold versions.

*What this means here:* shipping only the two Regular faces is correct and expected. The three
obligations are met by `licenses/ofl-1.1/LICENCE.txt` (which contains the copyright statement),
`NOTICE.md`, and the generated `licenses/THIRD-PARTY-ASSETS.md` shown on the in-game credits screen.
**A packaging step that omitted `licenses/` would breach the licence** — that is the one way this
arrangement can be broken by accident, and it is why it is written down in three places.

## 5.7 — No names are reserved by default

> **Are any names (such as the main font name) reserved by default?**
>
> No. That is a change to the license as of version 1.1. If you want any names to be Reserved Font
> Names, they must be specified after the copyright statement(s).

*What this means here:* Noto's `OFL.txt` declares nothing after its copyright statement — the phrase
"Reserved Font Name" appears in the archived text only inside the licence's own definitions section.
So no RFN exists for either family and clause 5 restricts nothing. The point is moot in practice
because the files ship unmodified, but it is what makes any future subsetting or renaming safe.

## 3.7 — Trademarks sit outside the licence

> **If a trademark is claimed in the OFL font, does that trademark need to remain in modified fonts?**
>
> Yes. Any trademark notices must remain in any derivative fonts to respect trademark laws […]
> Trademarks work alongside the OFL and are not subject to the terms of the licensing agreement. The
> OFL does not grant any rights under trademark law.

*What this means here:* both binaries carry `name` ID 7 "Noto is a trademark of Google LLC." The
files are shipped **unmodified**, so the notice remains where it is by construction. "Noto" is not
used as branding for this product; it appears only as the factual name of the upstream files, in the
credits and in the provenance documents. Nothing here relies on a trademark right the OFL cannot
grant.

---

## Cross-check of the licence text itself

Separately from the FAQ, the archived `licenses/ofl-1.1/LICENCE.txt` was compared against SIL's own
published licence text at <https://openfontlicense.org/documents/OFL.txt> (retrieved 2026-09-07).
**The licence body is identical from `PREAMBLE` onward**, whitespace-normalised. The only difference
is the copyright line above it, which is exactly where OFL 1.1 expects the licensor's own notice.
