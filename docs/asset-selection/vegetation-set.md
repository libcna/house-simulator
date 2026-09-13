# Vegetation acquisition and review

`HOUSE-00297`, acquired and reviewed 2026-09-13. The delivered set is 34 GLBs: six tree species
at three separately generated ages, nine shrubs, five flowers and two grass-card sets.

## Source and selection

All source geometry comes from the 22 fixed Poly Haven assets below. Poly Haven is the project's
already-approved CC0 source (`HOUSE-00262`); its current terms and authorship warranty are retained
in `docs/licence-evidence/polyhaven.md`. The acquisition tool uses only the official per-asset API,
pins each complete metadata response by SHA-256 and verifies every downloaded include against the
MD5 published in that response.

| Role | Retained upstream assets | Outputs |
|---|---|---:|
| Trees | Fir, pine, *Burkea africana* / wild syringa, jacaranda, *Searsia burchellii*, *Searsia lucida* | 18 |
| Shrubs | Shrub 01–04, fern 02, nettle, weed plant 02, shrub sorrel 01, wild rooibos bush | 9 |
| Flowers | Celandine 01, dandelion 01, periwinkle, gazania, stinkkruid | 5 |
| Grass cards | Bermuda grass 01, grass medium 02 | 2 |

The catalogue did not provide reliably identified maple, birch or fruit-tree models under the
required terms. This task therefore keeps the upstream taxonomy instead of relabelling a tropical
species as one of them. `HOUSE-00772` owns placement and must choose these as visual garden/street
roles, not botanical aliases; if its in-world review needs an exact species, that is a focused
acquisition gap rather than licence or identity debt hidden here.

## Conversion and measured constraints

`tools/assets/polyhaven_vegetation.py` keeps the source 1K glTFs in an external cache and commits
only the prepared GLBs. It embeds base colour plus alpha at a maximum 256-pixel edge, removes
normal/ARM data that the stock-XNA vegetation path cannot sample, grounds every mesh at Y=0 and
centres it in plan. Long rows of scanned plant variants are cropped to one central clump.

The three tree ages are separately reduced pieces of geometry, not placement-time scale aliases:

| Age | Height | LOD0 ceiling | LOD1 | LOD2 |
|---|---:|---:|---:|---:|
| Sapling | 3.5 m | 4,800 triangles | 35% | 12% |
| Young | 7.0 m | 7,000 triangles | 35% | 12% |
| Mature | 12.0 m | 9,000 triangles | 35% | 12% |

Shrubs target at most 4,800 triangles, flowers 1,200 and grass cards 900. Models above §18.2's
4,000-triangle threshold carry exactly named LOD1/LOD2 meshes. Disconnected leaf cards sometimes
cannot be collapsed to the required ratio, so the foliage-only final pass retains a deterministic,
spatially distributed subset per material. Every reduced level is then aligned back to LOD0's plan
centre and support plane, preventing a decimator-created extreme from sinking or popping at a LOD
switch. Exact bounds, triangle counts, hashes and byte sizes are generated in
[`vegetation-set.json`](vegetation-set.json).

The complete source set is 26.69 MB and 231,546 triangles including LODs. CNA's real GLB-to-CNB
pipeline produces 27.39 MB, 30.4% of the 90 MB `exterior` pack budget.

## Local render review

The four local contact sheets are [trees](../asset-review/vegetation/trees.jpg),
[shrubs](../asset-review/vegetation/shrubs.jpg),
[flowers and grass](../asset-review/vegetation/flowers-and-grass.jpg), and the
[mature-tree LOD comparison](../asset-review/vegetation/tree-lods.jpg). They render the committed
GLBs with their embedded alpha; Poly Haven's reserved website previews are not copied.

The review checks that each result is recognisably distinct, upright, grounded, free of duplicated
overlapping variants and readable at its final aspect ratio. The LOD1 and LOD2 sheet rows are
framed near their 90 px and 30 px hand-off sizes instead of being enlarged into misleading
close-ups. This sheet is deliberately alpha-aware: `lod_gen.py`'s dependency-free geometric
silhouette sees the transparent corners of every leaf card as solid and its numeric outline score
is therefore not meaningful for this one material class. Geometry ratios, UV retention and exact
mesh names remain hard automated gates.

The set is approved as placement input. Seasonal foliage variants, grass-card runtime rendering,
impostors and the final choice of each instance remain owned by `HOUSE-00919`, `HOUSE-00773`,
`HOUSE-00851` and `HOUSE-00772` respectively.
