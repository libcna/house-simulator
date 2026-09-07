# Sketchfab — licence evidence, and the limits of what was verifiable

`HOUSE-00267` asks two questions: **does the CC0 filter guarantee CC0**, and **what evidence do we
archive per asset?** Both are answered. A third thing — the help-centre article that used to
document the filter — could not be retrieved, and that is recorded rather than papered over.

| | |
|---|---|
| Store licence page (retrieved) | <https://sketchfab.com/licenses> |
| CC filter (exists, JS-rendered) | `https://sketchfab.com/3d-models?features=downloadable&licenses=<uuid>` |
| Retrieved | 2026-09-07 |
| **Verdict** | **CONDITIONAL.** Per-asset only, with evidence archived from the model page. **Never for a hero asset.** |

---

## 1. Does the filter guarantee CC0? No.

A filter is a query over a metadata field. The field is set by the uploader when they publish, and
selecting a value in a search URL cannot turn a self-declaration into a warranty. That is the same
structure as Freesound's CC0 filter (`HOUSE-00272`) and Poly Pizza's per-asset licences
(`HOUSE-00265`), and it is why this project records the licence **as displayed on the model's own
page on the day it was downloaded**, never the query that found it.

Sketchfab is an uploader-driven aggregator hosting millions of models, and re-uploaded third-party
work is a well-known hazard on any such platform. It therefore sits with Blend Swap at the bottom of
this project's provenance ranking — below Freesound (whose terms at least make the uploader warrant
and indemnify) and far below Poly Haven and ambientCG (which warrant their own chain of title).

## 2. A trap: the store licences are a different product entirely

`sketchfab.com/licenses` is about **purchased, royalty-free** assets, not the free Creative Commons
downloads, and it defines two tiers:

> Certain 3D assets are available under only an **"editorial" license**, which has certain
> restrictions. In particular, those assets (a) **cannot be used for any commercial or promotional
> use**; (b) cannot be used to suggest sponsorship, affiliation, or association with any person,
> brand, or company; and (c) can be used in only works that comment on or criticize the subject
> matter of the assets or newsworthy or public interest events associated with them […]

An "editorial" asset is unusable here under any circumstances — it is NC by another name, and
ADR-0012 rejects NC. The trap is that this page is what a search for "Sketchfab license" returns, so
a future reader may apply store terms to a CC download or, worse, assume a purchased asset is
unrestricted. **The two systems must never be conflated.**

## 3. Per-asset evidence to archive

For every Sketchfab asset that enters `assets-src/`, its directory's `SOURCE.md` records:

1. the **model page URL** and the model's Sketchfab id;
2. the **uploader's name**, which is both the attribution subject and the only party asserting
   rights;
3. the **licence exactly as displayed on that model page**, on the day of download — not the filter,
   not a search result;
4. the **retrieval date**;
5. the **`license.txt`** that Sketchfab includes in a Creative Commons download, which is the copy
   that travels with the files;
6. the **SHA-256** of the downloaded archive and of the extracted asset.

CC BY is acceptable — attribution is generated from the manifest. CC BY-NC and CC BY-ND are
rejected by ADR-0012. CC BY-SA needs the deliberate decision described in `blendswap.md`.

## 4. What could NOT be verified, and why it is recorded

The help-centre articles that formerly documented the Creative Commons filter now return **404**
(`help.sketchfab.com/hc/en-us/articles/201441546-Creative-Commons-licenses` and the downloading
article), the help-centre search endpoint 404s, and `sketchfab.com/tos` renders its terms in
JavaScript so no automated read returns the licensing clauses. Sketchfab has also been absorbed into
Epic's Fab marketplace, which is where terms are increasingly published.

So the conclusions in §1 are reasoned from the structure of an uploader-declared field and from
this project's own rules — **they are not quotations from a Sketchfab terms page, because none could
be retrieved.** That is the honest state of this verification.

It changes nothing operationally: the disposition above is *conservative* in every direction, and
**no current plan task requires Sketchfab.** `HOUSE-00291`–`HOUSE-00293` list three sourcing
attempts each and can satisfy them from sources with stronger provenance. If Sketchfab is ever
actually reached for, the missing terms page must be read first — by a human with a browser if
necessary — and this file updated.
