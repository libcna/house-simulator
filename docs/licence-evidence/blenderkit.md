# BlenderKit (now "Blendkit") — licence evidence

`HOUSE-00266` asks two things: **what is the free tier's licensing model**, and **are per-asset
licences machine-readable?** Both are answered, the second by measurement.

| | |
|---|---|
| Licences page | <https://www.blenderkit.com/docs/licenses/> |
| Public API | `https://www.blenderkit.com/api/v1/search/?query=<q>&asset_type=model` |
| Retrieved | 2026-09-07 |
| **Verdict** | **CC0 assets only — and they are ~3 % of the library.** Effectively unusable at scale for this project, for a structural reason rather than a legal one. |

---

## The model: exactly two licences

> Blendkit (formerly BlenderKit) only has **2 available licenses**, and these are quite simple.
> Everything you download is available for commercial use. Both allow you to sell higher-level-derivative
> works, but **royalty free license doesn't allow to re-sell 3D models even if modified**.
>
> **Royalty Free** — This license protects the work in the way that it allows commercial use without
> mentioning the author, but **doesn't allow for re-sale of the asset in the same form** (eg. a 3D
> model sold as a 3D model or part of assetpack or game level on a marketplace).
>
> **CC0 - No Rights Reserved** — […] place them as completely as possible in the public domain […]

That is admirably clear, and for a *closed-source* game both licences would be fine: shipping a
model inside a game is the "higher-level derivative work" both permit.

## Why it does not work here: `assets-src/` is published

This repository distributes its **asset sources**, not only a compiled game. A Royalty-Free model
committed to `assets-src/Models/…` is a 3D model, offered as a 3D model, to anyone who clones the
repository. Whether that counts as "re-sale" when the repository is free is arguable — and ADR-0012
does not let this project rely on the arguable reading.

So, in manifest terms, a Royalty-Free BlenderKit asset is `redistributeSource: false`, and this
project has nowhere to put such a file. It is the same shape as Quaternius (`HOUSE-00265`) and CMU
mocap (`HOUSE-00270`) — except that with CMU the *derivative* is what we need and can be committed,
whereas here the model itself is the deliverable.

**CC0 BlenderKit assets are fully usable.** There is nothing wrong with them.

## Are per-asset licences machine-readable? Yes — verified

The public search API returns a `license` field per result, alongside `isFree`:

```
GET https://www.blenderkit.com/api/v1/search/?query=chair&asset_type=model
  → results[].license      "royalty_free" | "cc_zero"
  → results[].isFree       true | false
```

That is a better answer than most sources give: no page scraping, no per-asset browsing, and a
filter that can be applied before anything is downloaded.

## And the measurement that decides it

Sampling five furniture queries — `chair`, `sofa`, `lamp`, `fridge`, `table` — over the model
endpoint, 75 results:

| `license` | Count | of which `isFree` |
|---|---|---|
| `royalty_free` | **73** | 36 |
| `cc_zero` | **2** | 2 |

**Roughly 3 % of the library is usable here.** Note also that "free" and "CC0" are independent: 36 of
the free assets are Royalty Free, so the free tier is *not* a CC0 tier, which is precisely the
"free download ≠ redistributable" confusion this project is required to avoid.

## Disposition

* **Do not** use BlenderKit as a general furniture source. Poly Haven and ambientCG cover the same
  ground with CC0 throughout and first-party provenance.
* A specific `cc_zero` asset may be used, filtered through the API field above and recorded per
  asset in `SOURCE.md` with the API response's `license` value and the date.
* If this project ever stopped publishing `assets-src/`, the Royalty Free tier would open up. Noted
  because it is the only thing that would change the verdict.
