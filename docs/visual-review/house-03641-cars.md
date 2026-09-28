# HOUSE-03641 — bounded parked-car correction, 2026-09-28

Accepted: the existing estate/van family is corrected, with no new variants,
placements, textures, materials, driving or runtime system. The old bodies had
box glazing, full-width axle-like tyres and reversed winding on 40 visible faces
per source. Separate rounded tyres/rims, real wheel-arch voids/liners, tapered
cabins, sloping glass, body shoulders and small existing-material details replace
that geometry. Rear lamps reuse the existing red finish. Collision remains the
same 12-triangle proxy per model.

Visible triangles: estate **248→1980**, van **212→1852**, bounded by a 2200-source
triangle claim. The estate's mirror width is **1.98 m**, below the unchanged 2 m
placement gate. A first wider draft failed that gate and was rejected. Actual GPU
review rejected another draft's full-height van side glazing: the final lower cab
door is painted, and the estate roof/glazing extends over its cargo space. These
are static secondary assets, not hero vehicle modelling.

## Actual GPU evidence

All captures use the normal game binary, Radeon 780M/OPENGLES3, SDL offscreen and
surfaceless EGL, with DISPLAY/WAYLAND_DISPLAY unset. **No physical monitor window.**
Fourteen matched day/night pairs cover garage front/rear, every street tint, van
and house approach; two extra final van views make the cab/body legible without
the original foreground hedge. All final views were inspected. The old baseline
is 9ecd655, final scene base b297bb4: the intervening foyer-corner fix is not a car
change, so these are matched vehicle views, not an isolated whole-frame benchmark.
No FPS improvement is claimed.

Full poses, original PNG SHA-256 hashes and retained pairs:
[index](house-03641/index.json). Representative views:

- [Garage day](house-03641/garage-front-quarter-day-pair.webp)
- [Red estate day](house-03641/road-red-day-pair.webp)
- [Blue estate day](house-03641/road-blue-day-pair.webp)
- [Silver estate day](house-03641/road-silver-day-pair.webp)
- [Property approach](house-03641/approach-property-day-pair.webp)
- [Final van day](house-03641/van-unobstructed-day.webp)
- [Final van night](house-03641/van-unobstructed-night.webp)

Garage main/opener artificial and daylight atlases were freshly rebaked/promoted
sequentially against the final models; three PNGs change. Source manifests,
lightmap provenance, licence and asset-budget reports are current.

## Measurements and regression

Resident chunks **1389→1391**, measured uploaded bytes **93.636178970→94.231348038
MiB**. Reused red tail lamps add exactly one material group in L0_GARAGE (20→21)
and EXT_WORLD (30→31); EXT_ROAD already batches that finish. Exact exceptions are
documented/tested, including rejection at one above their ceilings; no blanket
budget increase, new texture or vertex/Reach split. These are measured content
counts, not an uncontended performance result.

Passed: generator selftest; all four scale checks; chunk selftest; complete world
build/deployment; final manifest/licence/asset-budget checks; **1507/1507** unit
tests; complete controller tour (**90 cells, 564 stops, 86635 steps, 14 detours**);
**344** strict-XNA translation units with two workers. Negative old-generator
probe fails 17 new geometry claims (glass, arches, winding, cargo proportions,
separate tyres) plus four expected source-byte pins; no hardcoded-success test.
Full static checks fail only the pre-existing owner-owned root `.claude` layout
entry. Nothing in that directory or a sibling repository was changed.

Raw images/logs: `/tmp/house-03641/{before,after}`. Final focused logs:
`/tmp/house-03641-{selftest,scale,chunks-selftest,unit,tour}-final.log`,
`/tmp/house-03641-static-gates.log`,
`/tmp/house-03641-world-final-geometry.log`,
`/tmp/house-03641-deploy-final-geometry.log`.
