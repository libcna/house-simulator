# Poly Haven — licence evidence

`HOUSE-00262`. Candidate source for HDRIs, PBR textures and models
(`cna-house.md` §19.2; used by `HOUSE-00296` and `HOUSE-00297`).

| | |
|---|---|
| Source | <https://polyhaven.com/license> |
| Retrieved | 2026-09-07 |
| Publisher | Poly Haven |
| Licence | **CC0 1.0 Universal**, archived at `licenses/cc0-1.0/LICENCE.txt` |
| **Verdict** | **APPROVED** for use, redistribution and commercial release, with one operational caveat below. |

---

## What the page says, verbatim

> **Asset License**
>
> All assets (HDRIs, textures and 3D models) on this site are the original work of Poly Haven staff,
> or artists who willingly and directly donate/sell their work to Poly Haven.
>
> Our assets are all licensed as CC0, which is effectively Public Domain even in jurisdictions that
> do not support the Public Domain.

> In other words:
>
> * You can use our assets **for any purpose**, including commercial work.
> * You **do not need to give credit or attribution** when using them (although it is appreciated).
> * You can **redistribute them**, share them around, include them when sharing your own work, or
>   even in a product you sell.

*(Bold as on the page.)*

## Rights, as recorded in the manifest

| Question | Answer |
|---|---|
| Redistribute source | **yes** |
| Redistribute derived / compiled | **yes** |
| Commercial use | **yes** |
| Modification | **yes** |
| Attribution required | **no** — appreciated, not required |

CC0 carries no attribution obligation, so `verify_licences.py` already omits the attribution field
for CC0 rows rather than printing an empty one. Poly Haven asks for support on Patreon from projects
that benefit financially; that is a request, not a licence term, and is recorded here so the
distinction stays visible.

## The provenance claim is unusually strong, and that matters

Most aggregators restate a licence supplied by an uploader. Poly Haven does not aggregate: the page
states the assets are "the original work of Poly Haven staff, or artists who willingly and directly
donate/sell their work to Poly Haven." That is a first-party warranty of chain of title, which is
the thing ADR-0012 says an aggregator's label cannot give. It is the reason this source ranks above
Sketchfab or Blend Swap for the same nominal licence.

## Caveat, and it is a real one: the Terms of Service forbid scraping

The same page carries a website ToS separate from the asset licence. §3.2:

> You must not engage in any activity that may disrupt or interfere with the proper functioning of
> the website, including but not limited to:
> * **Web scraping or data mining without express permission.**

**So the assets are CC0 but the *site* is not an open download target.** `HOUSE-00296` and
`HOUSE-00297` must fetch the specific assets they name — through the site or through the public API
under its own API terms (<https://api.polyhaven.com>, governed by separate API Terms) — and must not
bulk-mirror the library. This is consistent with the project's own rule against "enormous libraries
indiscriminately", so it costs nothing, but it is a term that would be easy to breach with a
well-meant download script.

ToS §4.1 also reserves everything on the site that is *not* a CC0 asset — logos, example renders,
user-submitted renders, page text. **Asset preview renders are not CC0 here** (unlike ambientCG,
which grants them explicitly). Do not use a Poly Haven preview image as a texture or as
documentation art.

## Notes

* CC0 is irrevocable by design, so a later change of policy on the site does not retroactively
  affect assets already obtained. Record the retrieval date per asset anyway — `SOURCE.md` and the
  manifest both require it — because it is the evidence that a given file was obtained under the
  terms quoted above.
* No archived copy of the ToS is kept here beyond the quoted clause: the licence is the operative
  document and it is archived in full at `licenses/cc0-1.0/LICENCE.txt`.
