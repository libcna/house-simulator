# Freesound — licence evidence and the CC0 filter's real semantics

`HOUSE-00272`. The source `HOUSE-00281`…`HOUSE-00290` draw on for every sound the NOX collection
does not cover — roughly 150 samples across footsteps, doors, switches, water, appliances, weather,
the dog, the cat, house noises and outdoor ambience.

| | |
|---|---|
| FAQ / licence explanation | <https://freesound.org/help/faq/> |
| Terms of Service | <https://freesound.org/help/tos_web/> |
| Retrieved | 2026-09-07 |
| Site owner | Universitat Pompeu Fabra (UPF), Barcelona |
| **Verdict** | **CONDITIONAL — APPROVED for `CC0` sounds only**, with per-sound evidence archived. `CC-BY` is usable but adds obligations; `CC-BY-NC` and `Sampling+` are **excluded outright**. |

---

## The question the task asks: does the CC0 filter *guarantee* CC0?

**No, and it cannot.** The filter reflects a field the uploader chose. What stands behind it is not
Freesound's verification but the uploader's warranty, and the ToS is explicit about the arrangement:

> a) the User grants UPF (as operator of Freesound), UPF's licensees and Freesound's users the right
> to use and exploit such sound and metadata **in accordance with the Creative Commons license/s
> indicated by the User**. The license applied to a sound applies also to associated metadata.
>
> b) the User **warrants** to UPF (as operator of Freesound) that he/she has all necessary rights in
> the sound and metadata to grant such license to UPF and the public and that the posting for
> publishing does not infringe any third-party rights of any nature.
>
> c) the User will **indemnify and hold UPF, UPF's licensees and users harmless** against all loss
> or damage suffered […] as a consequence of any breach of the above warranty […]

That is a real but *indirect* guarantee, and it is worth being precise about what it gives us:

* the licence is granted **directly to downstream users**, not merely to UPF — so this project's
  right to the sound comes from the uploader, and does not depend on Freesound continuing to exist;
* the uploader warrants chain of title, and the indemnity in (c) explicitly extends to **users**,
  not only to UPF;
* but nobody at Freesound checks. A mislabelled upload is a breach by the uploader, discovered — if
  ever — after the fact.

So Freesound sits **below** Poly Haven and ambientCG, which warrant their own chain of title
first-hand, and **above** a bare aggregator, which warrants nothing. For short sound effects that is
an acceptable risk; it would not be for a hero model.

## The four licences, and which this project may use

Freesound lets the uploader pick one of three current licences, plus one retired one that still
exists on old uploads:

| Licence | Usable here? | Why |
|---|---|---|
| **CC0 1.0** | **yes — preferred** | No attribution, no restriction, commercial use fine. `licenses/cc0-1.0/LICENCE.txt`. |
| **CC-BY 4.0** | yes, with obligations | Attribution is generated from the manifest into `licenses/THIRD-PARTY-ASSETS.md` and shown on the credits screen, so the obligation is met by construction. Needs its own `licenses/cc-by-4.0/` archive before the first such sound is committed. |
| **CC-BY-NC 4.0** | **NO** | Non-commercial. `cna-house` may become a paid Steam release, and ADR-0012 already rejects NC terms outright. |
| **Sampling+ 1.0** (retired) | **NO** | Retired by Creative Commons as too hard to interpret, and Freesound's own reading is that it forbids commercial advertising use. An ambiguous licence is an unusable one under ADR-0012. Old uploads still carry it. |

**Prefer CC0 and default to it.** `HOUSE-00281`…`HOUSE-00290` should filter to CC0 first and only
reach for CC-BY when CC0 offers nothing suitable — not because CC-BY is risky, but because 150
CC-BY sounds means 150 credit lines for material that is a fraction of a second long.

## One clause that is easy to misread

ToS §65 says:

> Unless otherwise agreed in writing with us, **you may not use the Freesound website portal for
> commercial purposes.**

This restricts commercial use of *the portal* — reselling access, running a business off the site's
services. It does **not** restrict using a CC0 or CC-BY sound in a commercial product; that is
governed by the sound's own Creative Commons licence, which §87(a) grants directly to users. Written
down because the sentence, read alone, looks like it forbids exactly what this project intends, and
someone will find it.

Note also that even under CC0 "you can't claim you are the author" (Freesound's FAQ). This project
claims authorship of nothing it did not make, and the generated credits document is what evidences
that.

## Per-sound evidence this project must archive

The filter is not the evidence. For **every** sound committed, the directory's `SOURCE.md` records:

1. the **sound page URL**, in the stable form `https://freesound.org/s/<id>/`;
2. the numeric **sound id**;
3. the **uploader's username**, which is the attribution subject for CC-BY and the warrantor for all
   licences;
4. the **licence exactly as displayed on that page** on the day it was downloaded — never inferred
   from a search filter, never from a mirror;
5. the **retrieval date**;
6. the **SHA-256 of the file as downloaded**, which goes in the manifest as `sourceSha256`, plus the
   converted file's hash once `convert_audio.py` has run (`HOUSE-00279` requires both).

Freesound also keeps a per-account **attribution list** of everything downloaded, which is a useful
cross-check but is not a substitute: it lives behind someone's login and cannot be read by a future
developer or by a gate.

## Practical notes for `HOUSE-00281`…`HOUSE-00290`

* Sample rates and bit depths vary wildly, and many uploads are stereo field recordings.
  `cna-house.md` §61 wants mono for anything positioned through `Apply3D`. `convert_audio.py`
  (`HOUSE-00193`) is where that is decided, per file, not by a blanket rule.
* Sound quality varies as much as licensing does. A CC0 label is not a quality bar, and the
  listening check `HOUSE-00278` requires applies here too.
* Do not bulk-download. This project's own rule is to fetch the exact assets a task names, and the
  grouped tasks name their counts precisely for that reason.
