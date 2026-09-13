# Asset review — cat

Prepared by `HOUSE-00298` on 2026-09-13. `HOUSE-00292` rejected all three sourcing routes and
selected a project-authored realistic domestic cat. No cat model exists yet, so the review remains
explicitly pending.

## Identity

| | |
|---|---|
| Asset id | Not assigned; `HOUSE-02091` must create the manifest row |
| Manifest row | Not present |
| Source file | Not present; project-authored Blender source and exported GLB required |
| Category | character / cat hero |
| Used in | starts as `PET_CAT` on `PERCH_L1_WINDOWSEAT`; mobile through the house |
| Review state | **prepared** |
| Completion owner | `HOUSE-02091` |
| Reviewer | Not reviewed; must not be the asset author |
| Date | Not reviewed |
| Verdict | **pending** — asset not authored |

## Provenance

Origin must be `authored`; the project author and creation date must be recorded in the final
manifest row. No rejected candidate from
[`cat-hero-research.md`](../../asset-selection/cat-hero-research.md) may be used as source geometry
or texture. Any external reference image is visual reference only and needs its own author, URL and
licence recorded below.

## Required views

| View | Required file |
|---|---|
| Front (+Z) | `front.png` |
| Side (+X) | `side.png` |
| Three-quarter | `three-quarter.png` |
| Close-up on `PERCH_L1_WINDOWSEAT` at eye height | `close-up.png` |
| Reference photograph or drawing | `reference.png` |

All five files are intentionally absent until `HOUSE-02091` creates the model and review. The
reference must show a realistic domestic cat in a neutral standing pose, with facial and paw detail
visible.

## Acceptance checks

- [ ] Shoulder height is within §70.5's 0.20–0.32 m range and the remaining proportions agree
      with the recorded reference.
- [ ] Origin is at the paw support plane; Y is up, +Z is front and winding is counter-clockwise.
- [ ] LOD0 is at most 14,000 triangles; LOD1/LOD2 counts and the collision proxy are recorded.
- [ ] Textures, material classes and collision proxy (at most 64 triangles) are recorded and clean.
- [ ] Silhouette and anatomy survive close inspection: face, ears, paws, shoulders, hips and tail
      are plausible, with no floating or intersecting fragments.
- [ ] The coat reads naturally in both diffuse room light and the window-seat highlight.
- [ ] No identifiable real private person's animal is reproduced from a private reference.
- [ ] Tier S and Tier E render correctly in actual landing lighting.
- [ ] The four rendered views and licensed reference exist and were reviewed.

## Changes required before approval

1. `HOUSE-02091` must author, export, manifest and measure the model and fill every missing identity
   and provenance value.
2. Render the five required images and obtain an independent verdict.
