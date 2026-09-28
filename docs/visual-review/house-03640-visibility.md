# HOUSE-03640 — Exterior sightlines and wall-join correctness

**FIXED after current moving GPU review, 2026-09-28.** Garage/balcony culling,
missing wall-thickness returns and false internal facades are reproduced and
corrected here. The intermediate investigations below remain explicitly labelled;
passing old shell checks or capture assertions did not supersede a visible defect.
No optional work or unrelated Android/audio/performance change.

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
## Wall-strip reproduction and correction in progress

The normal game at feet `(7.3,0.6,-21.6)`, yaw 90°, exposes a vertical sky stripe
through the laundry's east wall at z=-21.7. The matched upper bathroom view at
`(7.3,3.65,-21.6)` does the same. The basement utility view at
`(7.3,-2.3,-21.6)` also shows the slit. Disabling culling retains it and reveals
pieces of the outside scene through it: this failure is missing shell geometry,
not the portal depth bug above. Baselines are `/tmp/house-03640/wall-seams-before/`.

`side_intervals` correctly changes from a 300 mm exterior wall to a 250 mm garage
wall, but the generator only draws the two offset inner planes. It omits the
25 mm perpendicular return between them. The same omission applies to the
300/150 mm transitions elsewhere. The initial audit finds eight apparent transitions in
seven cells: `B1_UTILITY`, `B1_UNDERSTAIR`, `L0_LAUNDRY`, `L0_FAMILY`, `L0_KITCHEN`,
`L1_BATH3` and both south-wall transitions in `L3_ROOM`.

The generator now joins adjacent different-thickness runs with their own wall
finish, faces the recessed side, preserves any portal crossing the seam and
clips the return under the existing roof planes. No wall dimensions, openings,
collision tolerance or runtime subsystem change. The new laundry coverage claim
fails before the fix; the full shell selftest passes after it, including aperture,
roof and reversed-thickness/winding claims. Only those seven shells and their
14 artificial / seven daylight atlases are regenerated; all 78 receivers are
unwrapped to retain the complete atlas report.

Day/night matched poses and strafing sequences are still being reviewed before
closing the task. Continue this task before cars, cinema, audio or performance.

Matched frame-90 captures below use the same normal GPU path, requested poses,
clock and authored collision in both versions. The laundry's open door displaces
the requested x=7.3 pose to approximately x=7.52 in both; no collision tolerance
was changed. Each comparison contains day before/after, then night before/after.

| Cell / transition | Matched day and night comparison |
|---|---|
| `L0_LAUNDRY`, east z=-21.7 | [comparison](house-03640/wall-joins/laundry-east-comparison.webp) |
| `L1_BATH3`, east z=-21.7 | [comparison](house-03640/wall-joins/bath3-east-comparison.webp) |
| `B1_UTILITY`, east z=-21.7 | [comparison](house-03640/wall-joins/utility-east-comparison.webp) |
| `B1_UNDERSTAIR`, west z=-21.2 | [comparison](house-03640/wall-joins/understair-west-comparison.webp) |
| `L0_KITCHEN`, north x=-6.7 | [comparison](house-03640/wall-joins/kitchen-north-comparison.webp) |
| `L0_FAMILY`, north x=2.7 | [comparison](house-03640/wall-joins/family-north-comparison.webp) |
| `L3_ROOM`, south x=-3.6 | [comparison](house-03640/wall-joins/attic-south-west-comparison.webp) |
| `L3_ROOM`, south x=3.6 | [comparison](house-03640/wall-joins/attic-south-east-comparison.webp) |

In the three directly reproduced day views, blue-sky pixels in the identical
PNG wall region x=650..819, y=170..779 fall **1220→0** (laundry), **610→0**
(bathroom) and **610→0** (utility). Predicate: B>110, B>R+30, G>R+15.
It is applicable to these windowless brown wall regions only, not a general sky
detector for the blue-painted family wall or real windows. Other rows are coverage
controls for the same geometry omission; no visual improvement is claimed where
the initial view already hides the seam. Raw pairs are in
`/tmp/house-03640/wall-joins-{before,after}/`.

The initial motion attempt was **not accepted**: a strafe start within the room's
capsule-radius boundary cannot cross its near-corner seam, and the laundry's open
leaf displaces a second start. Final near-corner cases use approach/retreat from
valid free space, leaving the authored geometry/controller intact. Other joins
retain opposite strafes. A positive projected displacement greater than 0.20 m
is required, so collision correction alone cannot masquerade as successful motion.
The interrupted first matrix (exit 143) and all failed starts are not PASS evidence.

Full unit suite **1507/1507 PASS** after the first (wall-return) correction
(`/tmp/house-03640-returns-unit.log`); shell selftest PASS
(`/tmp/house-03640-shell-selftest-returns.log`). The extra measured-shell diagnostic
reports **85,409 triangles**, worst cell **3174/3500**, zero degenerate triangles
and zero reversed floor/ceiling faces. Its sole failure is the already recorded
`FRIDGE_L0_KITCHEN` nested-container opening limitation, not these wall joins
(`/tmp/house-03640-returns-verify-shell.log`; historical handoff round 110 records
the same limitation). Do not describe that diagnostic as fully green.

## Second wall root found by movement — task still open

The return-only review's 32 wall cases / 1296 frames pass their input/arrival
assertions, but this does **not** accept the visual result. The attic west join
alternates brown and black in both directions, with a maximum consecutive cropped
RGB difference **24.983/255** by day and **14.937/255** at night. The retained
[left-moving sheet](house-03640/wall-joins/wall-attic-west-left-day.webp),
[right-moving sheet](house-03640/wall-joins/wall-attic-west-right-day.webp) and
[intermediate index](house-03640/wall-joins/index.json) are reproduction evidence,
not successful final validation.

The finished attic room contains three adjacent footprint boxes. The shell's
side lookup excludes all boxes with the same cell ID, so it labels their shared
edges as exterior walls. The main box's lit inward face and the dormer bay's unlit
outward face occupy the identical z=-17.15 plane with the same winding. Their
different materials z-fight as the camera moves. The floor finishes also stop
short of the shared edge. The same mistake divides `B1_UNDERSTAIR`'s two boxes.
Collision already subtracts these same-room edges: preserving fake visible walls
would contradict both that controller geometry and the authored room footprint.

The existing side lookup now returns an open run at a same-cell continuation.
No inner wall, weather face, skirting or cornice is emitted across it; its zero
inset lets both floor finishes reach the common centre line. At a concave corner,
real bounding panels reach the other inner face. Straight collinear continuation
panels still meet edge-to-edge, never overlap. Different-thickness returns remain
for actual walls, not same-room openings. Four synthetic joined-room regression
claims fail before this correction and the complete shell selftest passes after:
open passage, continuous floor, closed concave corner and non-overlapping straight
wall. Logs: `/tmp/house-03640-joined-regression-{before,after}.log`.

Only these two affected rooms needed another shell/lightmap bake. The final
acceptance below follows that fresh bake/deployment, not the intermediate PASS.

## Final current GPU acceptance

The same Radeon/OPENGLES3 offscreen game, authored collision, requested poses and
10:30/23:00 clocks are used after the joined-room fix. Eight matched day/night
comparisons are in [joined-room-fix](house-03640/joined-room-fix/); in particular
the [west attic bay](house-03640/joined-room-fix/attic-south-west-comparison.webp),
[east attic bay](house-03640/joined-room-fix/attic-south-east-comparison.webp) and
[under-stair passage](house-03640/joined-room-fix/understair-west-comparison.webp)
now show continuous floors and the actual open room, not black false facades.
The five real thickness transitions remain closed. The previous three sky-pixel
counts are still zero in the final matched images.

All **32 wall-motion day/night cases / 1296 consecutive frames** pass after final
deployment and were visually reviewed. Both directions use the normal controller,
require >0.20 m of projected movement, and never replace capture with teleporting.
The low under-stair head triggers normal auto-crouch; its two directions each
capture 72 frames instead of speeding the controller up. Other cases use 36.
The [final index](house-03640/joined-room-fix/index.json) retains sequence/contact
hashes, raw filenames and every cropped RGB measurement. Six moving brown-wall
day cases (laundry/bath/utility, both directions) have **zero sky-coloured pixels**
in their review region throughout. No zero-draw frames; draws range **3–80** in
these wall cases. This does not replace the earlier **34 doorway/balcony/garage
cases / 1224 frames**, which validated the culling correction; the first eight
door cases were additionally rerun after the wall-return deployment. These are
distinct serial runs, not a claim that all 66 were captured in one final run.

The west attic join's same-trajectory consecutive ROI difference falls
**24.983→4.266/255 by day**, **14.937→2.716 at night** (left direction), and
**24.444→3.108**, **14.640→1.989** (right direction). The final sheets show ordinary
parallax of a stable brown wall and visible dormer opening, not brown/black swaps.
The largest final wall-matrix ROI difference is **6.568/255**, utility-room
parallax; a low numeric score alone is not the visual acceptance criterion.

Final supporting checks:

* Complete unit suite **1507/1507 PASS**, `/tmp/house-03640-joined-unit.log`.
* Complete collision GrandTour **90 cells / 564 stops / 86635 steps / 14 detours
  PASS**, `/tmp/house-03640-joined-tour.log`.
* Full shell selftest, including four return and four joined-room claims, PASS;
  78-receiver unwrap, two fresh-room bakes/promotion, 14-stage world rebuild and
  deployment of the five updated textures PASS. The other five corrected rooms'
  preceding bakes remain in the same durable reports; total changed atlas count is
  still **14 artificial / seven daylight**, with no new texture slots.
* `tools/ci/run_checks.sh`: **344 strict-XNA units clean with two workers**; every
  gate passes except inherited owner-owned `.claude` root layout. No gate weakened
  and no owner settings altered. `/tmp/house-03640-joined-static-gates.log`.
* Extra `tools/world/verify_shell.py --report` has the same known nested refrigerator
  opening failure only; **85235 triangles**, worst **3174/3500**, zero degenerate
  faces and zero reversed floor/ceiling faces. It is not claimed fully green.
* Against the return-only intermediate, removing false walls/trim reduces **85409
  to 85235 triangles**, **1390 to 1389 resident chunks**, reported uploaded geometry
  **93.647856→93.636179 MiB**. No FPS or uncontended CPU/GPU timing improvement is
  claimed. Visibility/frustum/closed-door budgets and collision data remain intact.

Final wall logs are `/tmp/house-03640-joined-motion-*.log`, matched raw images are
`/tmp/house-03640/wall-joins-joined-final/`, and raw moving frames remain in
`build/test-output/threshold-movement/` with the final index's exact filenames.
No physical-display window, Android run, new build directory or sibling write.
