# Asset review — player car

Prepared by `HOUSE-00298` on 2026-09-13. `HOUSE-00293` rejected all three sourcing routes and fixed
the replacement as an unbranded, contemporary five-door family estate authored by the project.
No car model exists yet, so this record defines rather than asserts its approval.

## Identity

| | |
|---|---|
| Asset id | Not assigned; `HOUSE-01035` must create the manifest row |
| Manifest row | Not present |
| Source file | Not present; project-authored Blender source and exported GLB required |
| Category | vehicle / static player-car hero |
| Used in | garage; static, doors closed, not drivable |
| Review state | **prepared** |
| Completion owner | `HOUSE-01035` |
| Reviewer | Not reviewed; must not be the asset author |
| Date | Not reviewed |
| Verdict | **pending** — asset not authored |

## Provenance

Origin must be `authored`; the project author and creation date must be recorded in the final
manifest row. No rejected candidate from
[`car-hero-research.md`](../../asset-selection/car-hero-research.md) may be used as source geometry
or texture. The shape must remain unbranded and must not reproduce a protected production design.
External reference photographs need their author, URL and licence recorded below.

## Required views

| View | Required file |
|---|---|
| Front (+Z) | `front.png` |
| Side (+X) | `side.png` |
| Three-quarter | `three-quarter.png` |
| Close-up in the actual garage at eye height | `close-up.png` |
| Reference photograph or drawing | `reference.png` |

All five files are intentionally absent until `HOUSE-01035` creates the model and review. The
reference set must establish ordinary contemporary estate proportions without becoming a blueprint
for one identifiable make and model.

## Acceptance checks

- [ ] Bounds are 4.65 × 1.84 × 1.48 m and within §70.5's 4.2–5.2 × 1.7–2.0 × 1.4–1.9 m range.
- [ ] Origin is at the tyre support plane; Y is up, +Z is front and winding is counter-clockwise.
- [ ] LOD0 is at most 45,000 triangles; LOD1/LOD2 counts and ratios are recorded.
- [ ] A silhouette-matching collision proxy of at most 64 triangles is present.
- [ ] Exterior, wheels, glass and simplified visible cabin have clean topology, normals and
      material assignments; textures and colour spaces are recorded.
- [ ] The result is visibly unbranded and not a close copy of an identifiable production car.
- [ ] The surface looks used but maintained, consistent with the inhabited garage.
- [ ] Tier S and Tier E render correctly in the actual dim garage lighting at distances down to
      the specified six-metre viewing range.
- [ ] The four rendered views and licensed reference exist and were reviewed.

## Changes required before approval

1. `HOUSE-01035` must author, export, manifest and measure the static model and fill every missing
   identity and provenance value.
2. Render the five required images in the garage and obtain an independent verdict.
