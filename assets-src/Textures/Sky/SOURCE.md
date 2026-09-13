# `assets-src/Textures/Sky/` — provenance

## Assets

| Manifest id | File | Upstream name / version |
|---|---|---|
| `TEX_SKY_MOON_ALBEDO` | `moon_albedo.png` | NASA SVS CGI Moon Kit, `lroc_color_poles_2k.tif` (2019 map) |

The other files in this directory are deterministic project-authored cloud textures and therefore
do not need downloaded-asset provenance rows here.

## Source

| | |
|---|---|
| Publisher / author | NASA Scientific Visualization Studio; visualizer Ernie Wright (USRA) |
| Canonical URL | <https://svs.gsfc.nasa.gov/4720> |
| Retrieved | 2026-09-13 |
| Upstream release / tag / version | CGI Moon Kit 2019 RGB colour map, `lroc_color_poles_2k.tif` |
| Second source cross-checked? | NASA Media Usage Guidelines: <https://www.nasa.gov/nasa-brand-center/images-and-media/> |

The reviewed upstream TIFF has SHA-256
`13b797422e8c4b8607ff2b2623ac3a046a6da0132d567c2d272d92fad7052c4a`. The preparation tool
refuses any other bytes, so an upstream replacement cannot silently enter the game under this
record.

## Licence

| | |
|---|---|
| Licence | `NASA-PD` — NASA material generally not subject to copyright in the United States |
| Archived at | `licenses/nasa-pd/LICENCE.txt` |
| How it was established | The item page identifies NASA SVS and LRO sources and marks no third-party copyright; NASA's official media guidelines expressly include texture maps and computer graphical simulations. |
| Redistribute source | yes |
| Redistribute derived / compiled | yes |
| Commercial use | yes, without explicit or implicit NASA endorsement |
| Modification | yes |
| Attribution required | NASA asks to be acknowledged: “NASA's Scientific Visualization Studio; visualization by Ernie Wright (USRA)” |
| Other obligations | Do not use NASA insignia/logotype or imply endorsement; neither appears in the texture. |

## Modification

| | |
|---|---|
| Modified from upstream? | **yes** |
| If yes, what and why | The 2048×1024 equirectangular full-globe map is orthographically projected to the visible near-side disc, resampled to 1024², and converted to greyscale for §33.3's camera-facing quad. |
| Conversion history | `tools/assets/moon_albedo.py --fetch`; deterministic RGBA8 PNG, with the nearest limb extended outside the disc because `MoonMask` owns alpha. |

## Notes

The 2025 colour-map revision was not selected. The already-reviewed 2019 RGB map has a simpler,
stable provenance chain and more than enough source resolution for a 1024-pixel disc. The
displacement map is also unnecessary for the flat Tier-S moon quad.
