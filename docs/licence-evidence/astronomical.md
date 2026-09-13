# Lunar albedo map and star catalogue — licence evidence

`HOUSE-00274`, completed for the Moon by `HOUSE-01605` and revisited for the catalogue by
`HOUSE-01609`. Feeds phase 24 (moon and stars). The plan's expectation was "public domain
expected". That holds for the NASA Moon map and, after checking the specific federal dataset
record rather than assuming it from the subject matter, for NASA HEASARC's BSC5P distribution.

| | |
|---|---|
| Retrieved | 2026-09-07 (Moon), 2026-09-13 (stars) |
| **Moon** | **APPROVED** — NASA, not subject to US copyright |
| **Stars** | **APPROVED** — NASA HEASARC BSC5P; Data.gov explicitly records the government-work licence |

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

## 2. Star catalogue — NASA HEASARC BSC5P

The first review correctly rejected an unsupported shortcut: the convenient **HYG v4.4** database
at <https://codeberg.org/astronexus/hyg> is CC BY-SA 4.0, not public domain. It also correctly
refused to infer a licence for the upstream Yale/ADC/CDS data merely because astronomical facts are
not copyrightable. `HOUSE-01609` closes that open evidence question through a specific publisher
record:

| | |
|---|---|
| Dataset | <https://catalog.data.gov/dataset/bright-star-catalog> |
| Publisher | NASA High Energy Astrophysics Science Archive Research Center (HEASARC) |
| Identifier | `ivo://nasa.heasarc/bsc5p` |
| HEASARC table | <https://heasarc.gsfc.nasa.gov/W3Browse/catalog/bsc5p.html> |
| Licence recorded by Data.gov | <https://www.usa.gov/government-works> |

This is not a blanket “NASA website means public domain” inference. The federal Data.gov record
names this exact dataset, its HEASARC publisher and distribution URLs, and explicitly assigns the
government-work licence. It also documents the chain honestly: HEASARC created its table in 1995
from an ADC or CDS file and subsequently revised it, including positions for 14 non-stellar HR
objects and later corrections. The catalogue generator excludes those 14 objects.

17 U.S.C. §105(a), archived verbatim in `licenses/us-gov-pd/LICENCE.txt`, says US copyright
protection is unavailable for a work of the United States Government. USA.gov adds the limits that
matter to use here: not every item merely hosted on a federal site is a government work; agency
logos and implied endorsement remain restricted; and the United States may assert copyright in
other jurisdictions. The per-dataset Data.gov licence field is therefore the permission evidence,
while the statutory text explains the US public-domain status. No NASA mark or endorsement appears
in the catalogue or game.

| Manifest field | Value |
|---|---|
| `licence` | `US-GOV-PD` (not an SPDX id; US government work) |
| `redistributeSource` / `redistributeDerived` | true / true |
| `commercialUse` | true, without implied endorsement |
| `modification` | true |

### Acquired source and generated subset (`HOUSE-01609`)

`assets-src/world/bsc5p.psv` is the complete 9,110-row response from HEASARC's public Xamin
interface, restricted to `name`, J2000 `ra`, J2000 `dec`, `vmag` and `bv_color`, and sorted by
`name`. It is 376,801 bytes with SHA-256
`31464f3928a834a44c1a7b1c960081550357b6e4134c2ff568223d6b03e865b7`. The exact query is retained
in `assets-src/world/SOURCE.md`; `tools/world/build_stars.py --fetch` is the only network route and
refuses a response whose bytes differ from that pin.

The source has 14 missing V magnitudes and 324 missing B−V values. After removing incomplete rows
and HEASARC's named non-stellar objects, 8,786 complete stars remain. The generator orders them by
`(V magnitude, HR number)` and writes the brightest 1,500. Nine otherwise-brighter rows lack B−V;
they are excluded rather than assigned an invented colour. The selected range is V −1.46 through
4.94. Its 24,016-byte `content/world/stars.bin` has SHA-256
`cf1145ec49855acced63cbb509f044b02977cd3a7233c3d0921145d5a30d4a20`.

## Disposition

* The Moon map and NASA HEASARC BSC5P distribution are cleared and acquired.
* HYG remains rejected for this asset because its BY-SA obligation is unnecessary when the
  specifically licensed federal distribution supplies the required fields.
* **Do not generalise this result to other astronomical catalogues.** The approval rests on the
  exact Data.gov record for `ivo://nasa.heasarc/bsc5p`, not on a claim that scientific data or files
  on NASA servers are automatically public domain.
