# MakeHuman / MPFB2 — licence evidence

`HOUSE-00269`, which is **Q-03**: *may generated meshes be redistributed, and under what terms?*
The answer gates `HOUSE-00294` (the human base meshes) and, through it, `HOUSE-00295` (locomotion)
and the whole avatar phase. `cna-house.md` records R-04 as the fallback if the answer is negative.

| | |
|---|---|
| Current licence page | <https://static.makehumancommunity.org/about/license.html> |
| Superseded page | <http://www.makehumancommunity.org/content/license.html> |
| Retrieved | 2026-09-07 |
| **Verdict** | **YES — core assets and their derivatives are CC0.** Generated meshes may be redistributed, commercially, without attribution. Two conditions below. |

---

## The answer, verbatim from the current page

> Both MakeHuman and MPFB are open source projects with a **split license**, with the goal that
> there should be as close to no restrictions whatsoever on the graphics parts.
>
> **The asset license** — All core assets are shared under Creative Commons, **CC0**. The effective
> consequence of this is that you are free to do as you see fit with the asset or **derivates of the
> assets**. You do not need to pay anything, include any copyright notices nor give attribution.
>
> **The source code license** — The source code of MPFB is shared under GPL and the source code of
> MakeHuman is shared under AGPL.

The split is the whole answer. **We ship meshes, not their code.** The GPL/AGPL covers the
application; it does not reach a mesh exported from it, and "derivates of the assets" is explicit.
So a MakeHuman or MPFB2 body, retopologised, rigged, decimated for LODs and compiled into `.cnb`,
is CC0 throughout.

This also settles MPFB2 specifically, which matters: MPFB2 is a **Blender add-on**, not MakeHuman,
and the older licence text below would not obviously have covered it.

## The older page says something narrower, and it is superseded

`makehumancommunity.org/content/license.html` — the old Drupal site, whose copyright line reads
"Copyright (C) 2001-2015 MakeHuman Team" — states that assets are **AGPL**, with a limited
exception:

> As a special and limited exception, the copyright holders of the MakeHuman assets grants the
> option to use CC0 1.0 Universal … as a license for the MakeHuman characters exported under the
> conditions that a) The assets were bundled in an export that was made using the file export
> functionality inside an **OFFICIAL and UNMODIFIED** version of MakeHuman and/or b) the asset
> solely consists of a 2D binary image in PNG, BMP or JPG format.

Under that text, an MPFB2 export is not obviously covered at all — MPFB2 is neither "an official and
unmodified version of MakeHuman" nor a 2D image.

**Two pages, two statements — but unlike the Quaternius case this is not an unresolved
contradiction**, and the difference matters enough to say why:

* the divergence is a **liberalisation over time**, not a restriction: the newer text grants strictly
  more than the older one;
* the newer page is plainly the current site — it is the one the project's own navigation serves,
  it covers MPFB (which post-dates the old text), and the old page's copyright line stops at 2015;
* **CC0 is irrevocable.** Even if the project later narrowed its terms again, a grant that stood
  when an asset was obtained continues to apply to that asset.

Where Quaternius has a live FAQ and a live licence page flatly contradicting each other on the same
day, this is a current page superseding an archived one. Recorded rather than glossed, because the
old page is still reachable and a future reader who finds it first will conclude the opposite.

## Two conditions on use

**1. "Core" assets only.** The CC0 statement covers the *core* assets — the base meshes, the morph
targets, the standard rigs. The community asset repository (clothes, hair, skins, poses contributed
by users) carries **per-asset licences**, and a character dressed from it is not automatically CC0.
`HOUSE-00294` must record, per asset used, whether it is core or community, and for a community
asset its own licence page and terms. A body built only from core assets needs no such record.

**2. The evidence travels with the asset.** `assets-src/Models/Characters/SOURCE.md` records the
MakeHuman or MPFB2 version, the date, this licence page, and the list of assets used, so a later
reader can tell a core-only body from a dressed one without re-deriving it.

## What this unblocks

R-04's fallback — building base meshes by hand — is **not triggered**. `HOUSE-00294` may proceed to
generate two bodies and evaluate topology and silhouette on their merits rather than their licence.
