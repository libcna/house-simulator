# HOUSE-03640 — Exterior sightline correction checkpoint

**STILL OPEN.** Garage/balcony culling failures are reproduced and corrected here;
the owner's striped wall-hole report is not yet located/reproduced sufficiently to
claim it fixed. Passing shell checks do not supersede that report. No completion
checkbox, optional work or unrelated Android/audio/performance change.

## Reproduction and exact causes

Normal Release `build/cna-house`, Radeon 780M / OPENGLES3, clear weather, frozen
10:30/23:00. All GPU runs unset `DISPLAY` and `WAYLAND_DISPLAY`, use SDL offscreen /
surfaceless EGL, CPU affinity 0–3 and four Mesa workers. No software-renderer substitute
or physical-display window. Compiler and strict-XNA fan-out is two workers.

1. **Enclosed garage, west road:** feet `(0,0,3)`, yaw 39°, pitch 0°. The normal game
   shows the rear fence/yard through the opening. A matched no-cull diagnostic shows
   the car, enclosing wall and garage fittings. The exterior aperture seed excludes
   open opaque doors, and the narrow driveway/gate graph misses the adjacent outdoor
   cell from this oblique view. Straight-on and east road poses already show the
   garage; those were controls, not successful reproductions.
2. **Upper Juliet inspection pose:** the lower front balcony's open-air volume extends
   to y=9 and overlaps the upper Juliet. First-hit grid lookup assigns the camera to
   `L1_BALCONY_FRONT`, dropping the upper landing. Highest-containing-storey selection
   fixes neighbour/grid lookup, preserving incremental hysteresis and same-storey
   nesting. The Juliet remains facade-only/non-player-accessible and its door stays shut.
3. **Clear balcony glazing:** after fixing membership, an off-centre Juliet view still
   shows sky and floating facade frames through the landing's hall opening. The shallow
   clear-door allowance stops at the landing and culls `L2_HALL`. Clear glazed doors
   now use the existing six-hop door allowance in both leaf states; ordinary windows
   and shut frosted glazing retain their old caps. Actual leaf geometry still occludes.

Changes are in `SpatialIndex.cpp`, `PortalTraversal.cpp`, `PortalDepth.cpp` and its
header; there are no model/material/collision changes or new systems. The focused
garage, balcony and overlapping-storey regressions first fail on the reproduced code.
The garage test also closes its opaque leaf and requires the garage to disappear.
The balcony test checks both clear-glass leaf states. See
[ADR-0017](../decisions/ADR-0017-exterior-sightline-correctness.md) and the R15 entry.

## Matched real-GPU evidence

| View | Before / intermediate | Corrected |
|---|---|---|
| West-road garage, day | [missing interior](house-03640/garage-west-before.webp), [no-cull diagnostic](house-03640/garage-west-nocull.webp) | [normal culling](house-03640/garage-west-after.webp) |
| Juliet landing/hall, day, x=-0.55 | [membership fixed but hall still missing](house-03640/juliet-depth-before.webp) | [hall retained](house-03640/juliet-left-day.webp) |
| Juliet, scheduled night | — | [left view](house-03640/juliet-left-night.webp) |
| Rear balcony, day/night | — | [day](house-03640/rear-balcony-in-day.webp), [night](house-03640/rear-balcony-in-night.webp) |
| Garage, opposite road angle | — | [day](house-03640/garage-east-day.webp), [night](house-03640/garage-east-night.webp) |

Raw matched garage images use frame 300; settled final pose controls use frame 180.
The Juliet intermediate/final pair uses frame 180 and the identical requested pose
`(-0.55,6.55,-14.10,0,0)`; collision settles its feet to z≈-14.18 in both runs.
The intermediate picture is specifically the membership-only correction, not pristine
HEAD. The retained WebP copies are resized real screenshots, not generated illustrations.
Local originals/logs are `/tmp/house-03640*` and `build/test-output/threshold-movement/`.

## Validation discipline and remaining work

An initial motion matrix was **not PASS**: three new capture starts failed their
arrival assertions. They were too far from the threshold for the short capture or
aimed at the rear slider's fixed/meeting leaf. Source geometry establishes the open
high-U half; final starts use x=-1.65. No collision/controller tolerance was changed.
Two simultaneously running capture processes also mixed screenshot files in the
shared build root; those sequences are invalid evidence and are not accepted.
The final matrix runs serially. The actual 90-cell collision tour remains PASS:
564 stops, 86,635 controller steps, 14 detours.

Legacy cap fixtures are corrected deliberately rather than blanket-relaxed: windows
retain depth 1 outside, clear-door sightlines retain the existing door cap, and the
exact 24-pose list gains only `EXT_BACKYARD` for the hall view (104→105 sightings).
The old best-of-two test inferred all arrival allowances from one graph edge and
shallowest depth, which is invalid with independently seeded exterior doors. Its
replacement isolates two contributing apertures and requires allowance 6 with the
opaque door open, 1 with it shut. It asserts both cones actually contribute.

## Final checkpoint checks

* Unit suite **1507/1507 PASS**, `/tmp/house-03640-unit-accepted.log`. Earlier runs
  failed six superseded cap/golden expectations, then the incomplete two-aperture
  fixture (missing leaf ID); these were corrected, not recorded as pre-existing failures.
* Focused visibility suite **36/36 PASS**, `/tmp/house-03640-focused-accepted.log`.
* Actual serial GPU motion matrix **34 cases / 1224 consecutive frames PASS**,
  `/tmp/house-03640-movement-serial-final.log`. All new front/rear balcony arrivals
  pass in both directions at both times. Juliet captures are blocked-door inspection,
  not a newly opened route. All original room/stair/basement/porch cases remain green.
* No zero-draw frame; measured draws **5–433** across the matrix. The 14 new contact
  sequences are retained beside the matched views and were visually inspected. Every
  sequence's raw-frame RGB comparison is in [capture-index.json](house-03640/capture-index.json).
  Largest HUD-cropped consecutive mean difference: **6.893/255**, front-balcony exit
  in daylight; new doorway views do not expose the sky instead of the room/hall.
* StaticGeometryPass integration **9/9 PASS** and complete collision GrandTour PASS,
  `/tmp/house-03640-static-geometry.log`, `/tmp/house-03640-tour.log`.
* Existing background shell selftest PASS, including floor/wall joins, outside corner
  coverage and storey bands, `/tmp/house-03640-shell-selftest.log`. This is not a
  reproduction or dismissal of the owner's as-yet-unlocated striped hole.
* Full `tools/ci/run_checks.sh`: strict XNA **344 units clean**, two workers; all
  gates pass except the inherited owner-owned `.claude` root-layout entry. No gate
  was weakened and the settings were preserved. Final full strict rerun is retained
  at `/tmp/house-03640-strict-final.log`.
* Post-build visibility/spatial subset **49/49 PASS**,
  `/tmp/house-03640-post-build-focused.log`; the capture binary hash is unchanged.

No uncontended CPU/GPU performance measurement or before/after FPS improvement is
claimed. Some newly retained rooms necessarily cost draws; the exact 24-pose regression
remains bounded at 105 total sightings, with its previous cap and frustum limits intact.
Remaining work is the wall-strip reproduction, actual affected joins and day/night
movement verification there. Continue this task before cars, cinema, audio or performance.
