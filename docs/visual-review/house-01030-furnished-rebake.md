# HOUSE-01030 furnished-house rebake — Round 145

## Decision

The production lightmap baker now reuses the authored static-prop distinction instead of adding a
new lighting path. In each lightmapped cell it imports visible LOD0 geometry only for static props
with `collision: proxy`. Those solid furniture and appliance meshes cast into the architectural
receivers but are excluded from the receiver set. `_COL`, lower LOD and thin `collision: none`
dressing are excluded. This bounds bake cost while satisfying the required furnishing shadows.

Each cell product records `cellPropsSha256`, a deterministic hash of the selected authored rows and
the referenced GLB bytes. The driver checks it along with the shell, lights, baker, seed, sample
count and size before either full-house resume or subset promotion. `HOUSE-03402` supplies the
receive side: non-lightmapped furniture consumes the refreshed per-cell receiver means.

## Chunk and bake evidence

| Evidence | Result |
|---|---:|
| Authored props | 440 static, 0 dynamic |
| World chunks | 1,114 over 96 cells |
| Packed vertices | 3,010,601 / 79,744,552 bytes |
| Raw CNA vertex bytes | 144,508,848 (45% saved) |
| Limit splits | 8 for 16-bit vertices; 15 for Reach primitives; 0 using 32-bit indices |
| Cells above six chunks | 79; all exactly equal a measured existing exception |
| `chunks.bin` SHA-256 | `1c136cc070539bdec5897be5909ff8c3c0f21e50d83683f3806735369c583bc2` |
| Furnished bake inputs | 252 solid props / 2,585 visible meshes across 78 receiver cells |
| Daylight | 78 atlases, 256 samples, 206.455 s full bake |
| Artificial | 138 atlases/groups, 256 samples, 477.534 s full bake |
| Source / decoded bytes | daylight 1,125,476 / 5,111,808; artificial 1,001,442 / 9,043,968 |
| Resume proof | 78/78 cells reused in each mode |

All 78 products in each family have distinct current shell hashes and furnished-prop signatures;
the daylight/artificial cell sets, shell hashes, prop signatures and generated layout bindings
agree. Daylight receiver means span 0.035286–0.608257. The 138 artificial receiver means span
0.000142–0.236604 and their measured peaks span 0.002249–36.642830. No receiver mean is zero.
The unwrap report covers 78 cells and 78 atlases with `problems: []` and `needsAttention: []`.

## Visual inspection

The fixed clear-day controls below compare the prior Round 144 products with the furnished Round
145 bake. The reviewed capture directory is
`docs/visual-review/captures/house-01030-furnished-bake-r145/`; its matching before set is
`docs/visual-review/captures/house-03402-final-day-r144/`.

| Zone control | Pixels changed above 2% fuzz | Finding |
|---|---:|---|
| Attic | 535 | no visible defect |
| Basement | 7,377 | bounded furnished occlusion; no seam |
| Family / ground main | 852,797 | expected furniture/contact response; no bleed |
| Garage approach | 1,241 | exterior visually unchanged |
| Garage interior | 57,702 | expected furniture/contact response |
| Garden / potting | 1,388 | exterior visually unchanged |
| Mudroom / ground service | 705,790 | expected furnished-bake response; no bleed |
| Library / level 2 | 847,438 | expected furnished-bake response; no seam |
| Main stair | 298,864 | expected receiver response; no new discontinuity |
| Master bedroom / level 1 | 865,920 | expected furniture/contact response; no bleed |
| Street | 1,114 | exterior visually unchanged |

The exterior deltas are HUD/frame-timing pixels rather than scene changes. Full-size inspection of
the family room, library, master bedroom, mudroom and complete contact sheet found no black atlas,
S1/S2 seam, gutter bleed, clipping, z-fighting or new UV discontinuity. No render golden failed or
needed replacement. This is visual-correctness evidence only; no performance improvement is
claimed.

## Validation

Passed:

- `lightmap_bake.py --selftest`, including exact prop transform and auxiliary-node exclusion.
- Full daylight and artificial bakes, then full `--resume` checks.
- Chunk budget build and byte-identical source/deployed chunk comparison.
- Asset manifest, licences, budget report, deployed world, world schema and all 15 authored-world
  validation rules.
- Normal `build/` build with the refreshed 210 changed lightmap textures.
- Unit label: 1,420/1,420.
- Render label: 49/49 runnable tests; seven rasterizer-specific comparisons skipped and four
  reference-regeneration tests disabled by their existing contracts.
- Integration: 140/141 under load; the one weather-clock timing threshold miss passed its exact
  focused retry in 4.988 s.

Every compilation and test invocation used at most four workers pinned to CPUs 4,5,7,9. The
user-owned root `.claude` remains the known pre-existing layout-check failure and is unrelated to
this task.
