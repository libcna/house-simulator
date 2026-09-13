# Asset review — `<ASSET_ID>`

Copy this file to `docs/asset-review/<asset_slug>/review.md`, fill it in, and commit it with the
asset. **Hero assets do not enter the game without a completed sign-off** — the ones a player looks
at directly and for a long time: the front door, the kitchen, the staircase, the car, the dog, the
cat, the avatar, the sofa the family room is built around.

Ordinary props do not need one. The test is whether a wrong proportion would be noticed.

---

## Identity

| | |
|---|---|
| Asset id | `MODEL_…` |
| Manifest row | `assets-src/assets.manifest.json` |
| Source file | `assets-src/Models/…/….glb` |
| Category | *(appliance, seating, architecture, character, vehicle, vegetation, …)* |
| Used in | *(cell ids)* |
| Review state | **prepared** / **ready for review** / **complete** |
| Completion owner | `HOUSE-…` |
| Reviewer | |
| Date | *(ISO)* |
| Verdict | **pending** / **approved** / **approved with changes** / **rejected** |

`prepared` means that the acceptance record exists before the asset does. It is not an approval.
Only change the state to `complete` when every required file and check below has real evidence,
and never put a reviewer or date on a review they did not perform.

## Provenance

| | |
|---|---|
| Origin | downloaded / generated / authored / derived |
| Author | |
| Source URL | |
| Retrieved | *(ISO date)* |
| Licence | *(SPDX id)* |
| Licence file | `licenses/<slug>/LICENCE.txt` |
| Redistribute source / derived | yes / no · yes / no |
| Commercial use / modification | yes / no · yes / no |

A `no` in the *derived* column means LODs cannot be generated, which means the asset cannot be used
here at all ([ADR-0012](../decisions/ADR-0012-asset-licensing.md)). Say so in the verdict rather
than working around it.

## Views

Four renders at a consistent camera distance and the same neutral lighting, plus the reference.

| View | File |
|---|---|
| Front (+Z) | `front.png` |
| Three-quarter | `three-quarter.png` |
| Side (+X) | `side.png` |
| Close-up, in its actual cell at eye height | `close-up.png` |
| Reference photograph or drawing | `reference.png` (record where it came from) |

## Checks

- [ ] **Scale is real** — dimensions in metres, checked against the reference and against
      `cna-house.md`'s dimension tables. State the measured bounds: `x × y × z` m.
- [ ] **Origin** is at the support point (floor contact), or at the wall plane for wall-mounted
      assets.
- [ ] **Orientation** — Y up, +Z front, counter-clockwise front faces.
- [ ] **Triangle counts** — `LOD0`, and `LOD1`/`LOD2` where `LOD0` > 4 000. State them, and state
      the budget from `cna-house.md` §72.
- [ ] **Textures** — sizes, and base colour sRGB with normal/roughness linear.
- [ ] **Materials** map onto the project's material classes; no unassigned slots.
- [ ] **Collision proxy** `<name>_COL` exists, is ≤ 64 triangles, and matches the silhouette a
      player would collide with.
- [ ] **Geometry hygiene** — manifold, no inverted normals, no stray loose vertices, no n-gons
      that triangulate badly.
- [ ] **Habitation** — it looks used rather than showroom-new, where the category calls for it
      (`cna-house.md` §59.3).
- [ ] **No identifiable real private person** depicted (§59.4).
- [ ] Renders correctly in **both tiers** — Tier S and, where relevant, Tier E.

## Notes

*What is good, what is wrong, and what was changed. Be specific: "the counter is 0.87 m, should be
0.92 m" is useful; "proportions look off" is not.*

## Changes required before approval

1.
2.
