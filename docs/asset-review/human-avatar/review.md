# Asset review — human avatar

Prepared by `HOUSE-00298` on 2026-09-13. This review covers the two playable body variants as one
hero category. `HOUSE-00294` approved the neutral MPFB-derived meshes as research inputs; it did
not approve a final textured, rigged avatar in the house. `HOUSE-00295` also rejected all six
experimental retargets, so none of those animations is evidence for this sign-off.

## Identity

| | |
|---|---|
| Asset id | Final runtime ids not assigned; current inputs are `MODEL_HUMAN_FEMALE_BASE_RESEARCH` and `MODEL_HUMAN_MALE_BASE_RESEARCH` |
| Manifest row | `assets-src/assets.manifest.json` (research inputs only) |
| Source file | `assets-src/Models/Characters/human_female_base.glb`; `human_male_base.glb` |
| Category | character / playable avatar |
| Used in | player-controlled; not tied to one cell |
| Review state | **prepared** |
| Completion owner | `HOUSE-02131` |
| Reviewer | Not reviewed; must not be the asset author |
| Date | Not reviewed |
| Verdict | **pending** — final runtime bodies do not yet exist |

## Provenance

The current neutral meshes are generated derivatives of the core MPFB 2.0.17/build 20260722
basemesh and macro targets, authored by the MakeHuman/MPFB project and licensed CC0-1.0 for asset
output. The reproducible recipe, canonical URL, retrieval date, hash and archived licence are in
[`human-base-mesh-research.md`](../../asset-selection/human-base-mesh-research.md), the manifest
and `assets-src/Models/Characters/SOURCE.md`. No community asset may enter the final variants
without its own separately acceptable provenance.

## Required views

Each final body variant needs its own set; every path is intentionally absent until `HOUSE-02131`.

| View | Female | Male |
|---|---|---|
| Front (+Z) | `front-female.png` | `front-male.png` |
| Side (+X) | `side-female.png` | `side-male.png` |
| Three-quarter | `three-quarter-female.png` | `three-quarter-male.png` |
| Close-up in the actual scene at eye height | `close-up-female.png` | `close-up-male.png` |
| Reference photograph or drawing | `reference-female.png` | `reference-male.png` |

The reference files must be redistributable and their author, URL and licence recorded here. The
renders must use the final textures, skin, rig and normal gameplay lighting, not Blender's neutral
research lighting.

## Acceptance checks

- [ ] Female and male final bounds remain near the measured 1.660067 m and 1.779991 m heights and
      within §70.5's 1.55–1.90 m avatar range.
- [ ] Origin is at the floor-contact point; Y is up, +Z is front and winding is counter-clockwise.
- [ ] Each body plus head is at most 22,000 LOD0 triangles; clothing and hair are reported
      separately against their 4,000 and 6,000 limits.
- [ ] Final manifest ids, source paths, texture dimensions, material classes and LOD counts are
      recorded above.
- [ ] The final 32-bone skeleton binds, weights use at most four influences and representative
      movement does not tear shoulders, pelvis, hands or feet.
- [ ] Geometry is clean; skin and clothing do not expose holes in normal gameplay poses.
- [ ] No identifiable real private person is depicted.
- [ ] Both tiers render correctly in actual scene lighting.
- [ ] All ten model views and both licensed references exist and were reviewed.

## Changes required before approval

1. `HOUSE-02131` must build the final textured and rigged variants and replace the research-only
   identities with their exact runtime manifest rows.
2. Render and review the two complete five-image comparison sets above.
