# Lunar albedo map and star catalogue — licence evidence

`HOUSE-00274`. Feeds phase 24 (moon and stars). The plan's expectation was "public domain
expected". **That holds for the Moon and does not hold for the star catalogue**, which is the
finding.

| | |
|---|---|
| Retrieved | 2026-09-07 |
| **Moon** | **APPROVED** — NASA, not subject to US copyright |
| **Stars** | **CC BY-SA 4.0** for the obvious source. Usable, but share-alike, and not public domain |

---

## 1. Lunar albedo and elevation — NASA SVS CGI Moon Kit

| | |
|---|---|
| Source | <https://svs.gsfc.nasa.gov/4720> |
| Publisher | NASA's Scientific Visualization Studio |
| Data | Lunar Reconnaissance Orbiter camera and laser altimeter teams |
| Usage terms | <https://www.nasa.gov/nasa-brand-center/images-and-media/> |

The page describes exactly what phase 24 needs: *"These color and elevation maps are designed for
use in 3D rendering software. They are created from data assembled by the Lunar Reconnaissance
Orbiter camera and laser altimeter instrument teams."*

NASA's Media Usage Guidelines address this case in unusually specific words:

> NASA content – images, audio, video, and **media files used in the rendition of 3-dimensional
> models, such as texture maps and polygon data in any format** – generally are **not subject to
> copyright in the United States**. You may use this material for educational or informational
> purposes, including photo collections, textbooks, public exhibits, **computer graphical
> simulations** and Internet Web pages.

"Texture maps … in any format" is a lunar albedo map, and "computer graphical simulations" is this.

**Three constraints, none of which bites here:**

1. **No implied endorsement.** *"If the NASA material is to be used for commercial purposes […] it
   must not explicitly or implicitly convey NASA's endorsement of commercial goods or services."* A
   moon texture in a house game does not; using NASA imagery in store art or promotion would need
   care.
2. **The NASA insignia and logotype are NOT public domain** and are protected separately. Nothing
   from the brand set is used.
3. **Third-party material on NASA sites is marked.** *"NASA occasionally uses copyright-protected
   material of third parties […] Those images will be marked identified as copyright protected."*
   So the per-asset page must still be read — the blanket rule is not blanket.

**Attribution.** NASA asks to be acknowledged as the source, and the SVS page names the item's
credit: *"NASA's Scientific Visualization Studio"*, visualizer *Ernie Wright (USRA)*. Not a licence
condition, but carried in the manifest's `attribution` field anyway, so it reaches the credits.

| Manifest field | Value |
|---|---|
| `licence` | `NASA-PD` (not an SPDX id; "not subject to copyright in the United States") |
| `redistributeSource` / `redistributeDerived` | true / true |
| `commercialUse` | true, subject to the no-endorsement constraint above |
| `modification` | true |

### Acquired asset (`HOUSE-01605`)

The selected upstream file is the original 2019 RGB map `lroc_color_poles_2k.tif`, not the 2025
revision. It is 2048 × 1024, and its reviewed SHA-256 is
`13b797422e8c4b8607ff2b2623ac3a046a6da0132d567c2d272d92fad7052c4a`. The item page marks no
third-party copyright on it and gives the requested credit to NASA's Scientific Visualization
Studio, with Ernie Wright (USRA) as visualizer.

`tools/assets/moon_albedo.py --fetch` downloads only that official SVS URL, refuses bytes with any
other hash, then performs the project-owned derivation needed by the Tier-S quad: an orthographic
projection of the near side, 1024-square resampling, Rec.709 greyscale conversion and opaque
nearest-limb extension beneath the separately generated phase mask. The committed RGBA8 PNG is
885,108 bytes, spans 208 grey levels, and has SHA-256
`d7fa53d450a40b605ab29dd28a51b9be4b43cae1b10481d5f9a91f428b851e75`. Normal CI is offline: it
pins this output and tests the projection orientation independently.

## 2. Star catalogue — the expectation does NOT hold

The obvious source, and the one a search leads to, is the **HYG database**:

| | |
|---|---|
| Current home | <https://codeberg.org/astronexus/hyg> (**moved** from GitHub, which now carries only a pointer) |
| Version | HYG v4.4 |
| Licence | **Creative Commons Attribution-ShareAlike 4.0 International** |

Verified from both the repository's `README.md` and its `LICENSE` file:

> This work is licensed under a Creative Commons Attribution-ShareAlike 4.0 International License.

**That is not public domain, and share-alike matters here.** Phase 24 does not ship the CSV; it
bakes a table of positions and magnitudes for the naked-eye stars into a content file. That baked
table is a *derivative of a database*, and CC BY-SA 4.0 requires derivatives to be offered under
BY-SA. Workable — a star table can be published under BY-SA without touching the rest of the game,
exactly as `blendswap.md` describes for BY-SA models — but it is an obligation that has to be
recorded and honoured, not discovered at packaging time.

Also worth noting: the GitHub repository most links point at is **stale**, and its GitHub licence
metadata reads `NOASSERTION`. A reader who stopped there would have no licence at all.

**The alternative, and what still needs checking.** HYG is itself compiled from older scientific
catalogues — principally the Yale Bright Star Catalogue and Hipparcos/Tycho — distributed through
CDS/VizieR. Those upstream catalogues are the natural non-share-alike route, and 9 110 naked-eye
stars is exactly what phase 24 wants. **Their terms have not been verified here** and must be
before they are used; this file records the option, not an approval.

## Disposition

* The Moon map is cleared and may be acquired when phase 24 reaches it.
* For stars, either accept BY-SA on the derived table and record it, or verify a CDS/VizieR
  catalogue first. **Do not assume "astronomical data is public domain"** — it is a reasonable prior
  and it is false for the most convenient source.
