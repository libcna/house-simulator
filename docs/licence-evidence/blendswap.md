# Blend Swap — licence evidence

`HOUSE-00268`. A candidate model source, and one of the two that gate `HOUSE-00291` (the dog).

| | |
|---|---|
| FAQ | <https://blendswap.com/faq> |
| Terms of Use | <https://blendswap.com/terms> |
| Retrieved | 2026-09-07 |
| **Verdict** | **CONDITIONAL and weak.** Per-asset only, CC0 preferred. **Non-commercial assets are present and are rejected outright.** Never for a hero asset. |

---

## There is no single licence, and the default is share-alike

The FAQ:

> **What licenses are available?**
> Assets are shared under Creative Commons licenses including **CC0, CC BY, and CC BY-SA**, as well
> as our **General Asset License**. **Always check the license on the asset page before using it.**

The Terms of Use go further and name a site-wide default:

> You are allowed to access, modify, use and redistribute our content under **Creative Commons
> BY-SA 3.0 USA (or under the license the content is marked with)** […] provided that you adhere to
> the license attached to the content you access.

So the *baseline* here is share-alike, not permissive — the opposite of Poly Haven or ambientCG.

## Non-commercial assets are present, and ADR-0012 rejects them

Terms §7, in full, because this is the clause that decides the verdict:

> **7. Use of assets under a Non Commercial CC license**
> Users may upload content under the Creative Commons Attribution-NonCommercial-ShareAlike License,
> so other users may download the work and use it as long as they do not resell it or **use it for
> any kind of monetary profit, even through derivatives of the work**.

`cna-house` may become a paid release, and ADR-0012 rejects NC terms as a standing rule. An
NC-marked Blend Swap asset is therefore unusable here — and, unlike a source that simply has no NC
content, this one has it mixed into the same library behind the same download button. **The filter
is not optional.**

## The site warrants nothing about chain of title

> neither BlendSwap nor our Affiliates make warranties that the files you download from the Site are
> fit for any particular purpose, and your use of those files is your sole responsibility.

> Neither we or our Affiliates warrant or represent that **your use of materials displayed on, or
> obtained through, this site will not infringe the rights of third parties.**

That second sentence is an explicit disclaimer of exactly the assurance a hero asset needs. Compare
Poly Haven, which warrants that its assets are its own staff's work or directly donated. Blend Swap
sits at the bottom of this project's provenance ranking, alongside Sketchfab.

## Per-licence disposition

| Licence found on Blend Swap | Usable here? |
|---|---|
| **CC0** | yes — preferred, and the only one to look for by default |
| **CC BY** | yes; attribution is generated from the manifest |
| **CC BY-SA** | **only with a deliberate decision.** Share-alike attaches to derivatives, and this project derives from every asset it ships — LOD1/LOD2, the collision proxy, the compiled `.cnb`. Those derivatives would have to be offered under BY-SA. It does not reach the rest of the game, but it is an obligation that must be recorded in `SOURCE.md` and honoured, so prefer CC0. |
| **CC BY-NC-SA** | **NO.** ADR-0012, and Terms §7 confirms such assets exist here. |
| **"General Asset License"** | **NO, pending.** The FAQ names it; its text is not on `/faq`, `/terms`, `/license`, `/general-asset-license` or `/legal` (all 404 except `/terms`, which does not contain it). An asset under a licence nobody has read is unusable — ADR-0012's rule for unclear terms. |

## Practical notes

* Downloading needs an account, and free accounts are limited to five assets per day. That suits
  this project's rule of fetching only what a task names.
* The site is opening a **paid marketplace** ("The marketplace is opening for paid Blender 3D
  models, addons, and tools. The free Creative Commons library is staying."), so "downloadable from
  Blend Swap" will increasingly not imply Creative Commons at all. Another reason the per-asset
  licence, read on the day, is the only evidence that counts.
* Assets are `.blend` files, so anything used here passes through Blender and is exported to glTF —
  a conversion that must be recorded in `SOURCE.md` as a modification.
