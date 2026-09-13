# Asset review — dog

Prepared by `HOUSE-00298` on 2026-09-13. `HOUSE-00291` rejected all three sourcing routes and
selected a project-authored, realistically proportioned Labrador-type dog. No dog model exists
yet, so this record fixes the bar without claiming a verdict.

## Identity

| | |
|---|---|
| Asset id | Not assigned; `HOUSE-02041` must create the manifest row |
| Manifest row | Not present |
| Source file | Not present; project-authored Blender source and exported GLB required |
| Category | character / dog hero |
| Used in | starts as `PET_DOG` in `L0_FAMILY`; mobile through the house |
| Review state | **prepared** |
| Completion owner | `HOUSE-02041` |
| Reviewer | Not reviewed; must not be the asset author |
| Date | Not reviewed |
| Verdict | **pending** — asset not authored |

## Provenance

Origin must be `authored`; the project author and creation date must be recorded in the final
manifest row. No rejected candidate from
[`dog-hero-research.md`](../../asset-selection/dog-hero-research.md) may be used as source geometry
or texture. Any external reference image is visual reference only and needs its own author, URL and
licence recorded below.

## Required views

| View | Required file |
|---|---|
| Front (+Z) | `front.png` |
| Side (+X) | `side.png` |
| Three-quarter | `three-quarter.png` |
| Close-up in `L0_FAMILY` at eye height | `close-up.png` |
| Reference photograph or drawing | `reference.png` |

All five files are intentionally absent until `HOUSE-02041` creates the model and review. The
reference must depict a medium Labrador-retriever type rather than a stylised or unrelated breed.

## Acceptance checks

- [ ] Bounds measure about 0.60 m at the withers and 1.05 m nose to tail, within §70.5's
      0.50–0.70 m dog range.
- [ ] Origin is at the paw support plane; Y is up, +Z is front and winding is counter-clockwise.
- [ ] LOD0 is at most 18,000 triangles; LOD1/LOD2 counts and the collision proxy are recorded.
- [ ] Textures, material classes and collision proxy (at most 64 triangles) are recorded and clean.
- [ ] Silhouette and anatomy survive close inspection: face, paws, shoulders, hips and tail are
      plausible, with no floating or intersecting fragments.
- [ ] The coat looks lived-in rather than plastic or showroom-new.
- [ ] No identifiable real private person's animal is reproduced from a private reference.
- [ ] Tier S and Tier E render correctly in actual `L0_FAMILY` lighting.
- [ ] The four rendered views and licensed reference exist and were reviewed.

## Changes required before approval

1. `HOUSE-02041` must author, export, manifest and measure the model and fill every missing identity
   and provenance value.
2. Render the five required images and obtain an independent verdict.
