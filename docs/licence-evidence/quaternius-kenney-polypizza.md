# Quaternius, Kenney and Poly Pizza — licence evidence

`HOUSE-00265`. Three model sources from `cna-house.md` §19.2, verified together because the plan
groups them. They reached three different verdicts.

| Source | Verdict |
|---|---|
| **Quaternius** | **REJECTED — the publisher's own site contradicts itself.** Unusable until resolved. |
| **Kenney** | **APPROVED**, CC0, declared per asset. |
| **Poly Pizza** | **CONDITIONAL** — an aggregator with per-asset Creative Commons licences, mostly CC-BY, not CC0. Usable only with per-asset evidence, and never for a hero asset. |

All pages retrieved **2026-09-07**.

---

## Quaternius — REJECTED

| | |
|---|---|
| Licence page | <https://quaternius.com/license.html> |
| FAQ page | <https://quaternius.com/faq.html> |

### The contradiction

The licence page states, and dates itself "Last updated: 8/28/2026":

> **Quaternius Asset License (QAL) v1.0**
>
> In short: You can use these assets, free of charge, in personal, educational, and commercial games
> and other projects, with no credit required. **You just can't resell or redistribute the assets
> themselves as assets.**

> **3. Restrictions.** You may not:
>
> a) **Resell or redistribute the Assets themselves.** You may not extract, repackage, sublicense,
> sell, or otherwise redistribute the Assets (in original or modified form) as a standalone asset,
> asset pack, stock file, template, or similar product, whether for free or for payment, and whether
> alone or bundled with other assets. This restriction applies regardless of how much the Assets
> have been modified. It does not restrict distributing a completed Product that merely incorporates
> the Assets (see Section 2).

The FAQ page, live on the same site on the same day, states the opposite:

> **Can these assets be used in commercial projects?**
> Yes, these assets can be used for free without the need for attribution in commercial,
> educational, and personal projects. **All models are under the CC0 License.**

> **Is it necessary to give credit when using these assets?**
> No, attribution is not necessary. However, credit is always appreciated.
> **These assets are licensed under CC0**, allowing you to use them without giving credit, even for
> commercial purposes.

These cannot both be true. CC0 is an irrevocable waiver that permits redistribution of the work
itself; QAL §3(a) forbids exactly that. One of the two pages is stale, and **which one is stale
changes the answer completely.**

### Why this is fatal here rather than merely untidy

Even taking the QAL at face value and ignoring the FAQ, it is a poor fit for this project, and for a
reason specific to how `cna-house` is built: **`assets-src/` is published.** This repository
distributes raw asset files, not only a compiled game. QAL §2 clearly permits shipping a build that
incorporates the assets; §3(a) prohibits redistributing the asset files. A public git repository
containing `assets-src/Models/…/chair.glb` sits precisely on that line — it is not offered as an
asset pack, but anyone can download the individual file from it.

So there are two independent problems, and either alone is disqualifying under ADR-0012:

1. the publisher's own pages disagree about which licence applies at all;
2. under the licence that would apply, whether this project's own structure is permitted is
   genuinely unclear.

ADR-0012 and the session's standing instruction give the same rule for both: *if terms are unclear,
the source is unusable until resolved.* Marked accordingly.

### What would resolve it

A statement from the publisher saying which page governs. That is an owner action — an email to the
address on the site (`laulhet@gmail.com`) — not something a build gate can settle. Until then
Quaternius contributes nothing to this project and no Quaternius asset may enter `assets-src/`.

QAL §7 is worth noting for whoever asks: *"Changes will not apply retroactively to Assets you've
already obtained under an earlier version; the version in effect at the time you obtained the Assets
governs your use of them."* So a copy provably obtained under a CC0 release would stay CC0 — but
"provably" is the hard part, and this project holds no such copy and no such evidence. **Do not
treat a third-party mirror's CC0 label as that evidence**; it is the aggregator-restatement failure
ADR-0012 exists to prevent.

### Consequence for the plan

No plan task depends on Quaternius alone. `cna-house.md` §19.2 lists it among several model sources;
Kenney, ambientCG and Poly Haven cover the same ground with clean terms. Nothing is blocked.

---

## Kenney — APPROVED

| | |
|---|---|
| Licence declaration | Per asset, on each asset page, e.g. <https://kenney.nl/assets/furniture-kit> → "License — Creative Commons CC0" |
| Site ToS | <https://kenney.nl/terms-of-service> (dated January 6th, 2024) |

**The licence is declared per asset, not site-wide, and the site ToS is not the asset licence.** The
ToS is an ordinary website agreement — disclaimer, limitation of liability, governing law — and says
nothing about CC0. It even asserts that "The materials contained in this website are protected by
applicable copyright and trademark law **unless marked otherwise**", and the per-asset "License:
Creative Commons CC0" line is that marking. Reading only the ToS would produce the wrong answer;
recorded because that is the page a search leads to first.

| Question | Answer |
|---|---|
| Redistribute source | **yes** |
| Redistribute derived / compiled | **yes** |
| Commercial use | **yes** |
| Modification | **yes** |
| Attribution required | **no** |

**Requirement when a Kenney pack is actually acquired:** record in that directory's `SOURCE.md` both
the asset page URL showing the CC0 declaration *and* the `License.txt` bundled inside the downloaded
pack. This verification checked the asset page; the bundled file is the copy that travels with the
files and is the one a later reader will find beside them.

Caveat on suitability rather than licence: Kenney's models are deliberately stylised and low-poly.
`cna-house` aims at a realistic house, and the asset quality bar rejects assets "stylistically
incompatible with the environment" and "visibly low-poly where the player can approach closely". So
Kenney is licence-approved but will rarely pass the *quality* bar for interior props, and never for a
hero asset. It is a legitimate source for a placeholder, and placeholders here still need a manifest
row.

---

## Poly Pizza — CONDITIONAL

| | |
|---|---|
| Terms | <https://poly.pizza/docs/tos> |
| Assets sampled | <https://poly.pizza/m/08-z1qQrjz3>, <https://poly.pizza/m/0xIH2GVYBR3> |

Poly Pizza is an **aggregator**, and its terms say so plainly:

> **Content Ownership.** User Content includes all the 3D models, images and comments, uploaded by
> Users that make up the Services. **We do not claim ownership over any User Content.**
>
> You are solely responsible for ensuring that any User Content you submit to the Services complies
> with any applicable laws and third party rights […]
>
> **By downloading User Content made available by other Users, you agree to adhere to the terms of
> the Creative Commons license that applies at the time of download.**

So there is **no site-wide licence**. Each model carries its own Creative Commons licence, shown on
its page, and the site warrants nothing about chain of title — the uploader does.

**The default is CC-BY, not CC0.** Both sampled assets displayed "Creative Commons Attribution",
which is consistent with the site's origin in the Google Poly archive. Assuming CC0 from the site's
reputation would be exactly the error ADR-0012 forbids.

Conditions on any use:

1. The licence must be read **on the individual model's page** and archived in that directory's
   `SOURCE.md`, with the model URL and the retrieval date.
2. A CC-BY asset requires attribution. This project can meet that — attribution is generated into
   `licenses/THIRD-PARTY-ASSETS.md` from the manifest and shown on the credits screen — so CC-BY is
   usable, not disqualifying.
3. **Never for a hero asset.** The dog, cat, car, avatar and hero furniture need provenance stronger
   than an uploader's self-assertion; `HOUSE-00291`…`HOUSE-00295` should prefer first-party sources.
4. Nothing here overrides the quality bar. The archive is dominated by low-poly stylised models.

---

## What this task changed in how the remaining sources get judged

The Quaternius result is the first case of a source whose *licence label was correct once and is not
now*, and it generalises: **a licence is a property of a file at a moment, not of a website.** Every
`SOURCE.md` therefore records the retrieval date and the URL of the page that was read, and the
per-asset check is what carries the claim — not the source's reputation.
