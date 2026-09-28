# HOUSE-03636 — whole-house day/night lighting correction

Current-source Radeon 780M/OPENGLES3 review, 2026-09-28. Rendering uses SDL's
invisible `offscreen` surface with `DISPLAY` and `WAYLAND_DISPLAY` unset,
`EGL_PLATFORM=surfaceless` and CPU affinity 0–3. No desktop windows, new graphics
API, custom shader, exposure rewrite or global ambient increase were used.

## Reproduced causes and corrections

- Peak-normalised linear RGBA8 bakes devoted nearly all their range to tiny
  near-fixture peaks. The existing baker now uses the 99.5th percentile of lit
  receiver texels and clamps exceptional peaks. Its synthetic firefly regression
  is part of the main/attic checkpoint; this task regenerates the remaining
  receivers. All 78 indoor cells have current artificial/daylight metadata:
  **138 artificial atlases and 78 daylight atlases**. Counts/dimensions are
  unchanged. Raw atlas pixels occupy 14,155,776 bytes before mips, not a measured
  whole-game memory figure.
- B1_CINEMA, B1_GYM, B1_HOBBY, B1_WORKSHOP, L0_DINING, L0_KITCHEN,
  L1_MASTER_BED, L1_BED5, L2_BED7, L2_GAMES, L2_LIBRARY and L2_SITTING
  relied on occupancy-style schedules which left their main lamps off during
  the daytime walkthrough. Their main groups now use the existing all-day
  circulation class; accent/bedside schedules are preserved.
- Windowless receivers had a real LM_DAY atlas but a zero runtime daylight tint.
  The existing two-hop portal transfer now also evaluates daylight alone.
  It cannot turn neighbouring artificial light into daylight, and becomes zero
  at night or when the relevant portal is closed.
- Direct-only shell illumination left walls facing away from fixtures black
  while their adjacent props received bounce. The existing opaque ambient pass
  now includes restrained **owned-fixture-dependent** indirect light. With lamps
  off its existing 0.025 ambient floor is unchanged; the added term is capped at
  0.09 per channel. Daylight is not counted as artificial bounce.
- Local receiver calibrations correct B1_STOR1, B1_STOR2, B1_CELLAR,
  B1_LAUNDRY2, B1_UNDERSTAIR, L0_LAUNDRY, L0_STOR, L0_GARAGE,
  L0_GARAGE_LOFT, L1_WC3, L1_WC4, L2_WC5 and L3_STORE_W/N/S.
  The visible loft is not counted as a player-accessible cell.
  The west attic fixture/source was inside the sloping roof; lowering it from
  12.45 to 12.10 m makes its authored light reach receivers. The east store and
  attic-stair receivers were already corrected in the coupled stair checkpoint.
  No extra asset family or new light-authoring system was added.
- PROP_B1_LAUNDRY2_DRYER and PROP_B1_WORKSHOP_PAINT_TIN used the
  pure-specular MAT_APPLIANCE_STEEL override with diffuse tint (0,0,0).
  They now reuse MAT_KITCHEN_HARDWARE_STEEL. See the
  [dryer before](house-03636/dryer-before-material.webp) and
  [after](house-03636/dryer-after-material.webp): controls and body remain
  readable, rather than a black shape.
- The moving outdoor review found another renderer defect: when a moon/sun key
  occupied slot 0, sky-open receivers discarded their existing practical lights.
  The stock BasicEffect now retains two practical slots and the existing local
  spill term. A device integration regression inspects the actual production
  effect: moon stays on, practical slot 1 is on, then disables when its group
  switches off. No extra render pass is introduced. Outdoor lamp coverage is
  still local; unlit garden beds are not made daylight-bright.

Local measured bake receiver means (scalar irradiance, not screenshot luminance):

| Cell | Earlier local bake | Accepted local bake |
|---|---:|---:|
| B1_STOR2 | 0.033222 | 0.132887 |
| B1_LAUNDRY2 | 0.091968 | 0.204373 |
| L0_GARAGE (main group) | 0.130881 | 0.248636 |
| L0_GARAGE_LOFT | 0.041479 | 0.150362 |

The accepted numbers come from the final deployed metadata, not an assumed
linear scaling of earlier means. The final whole-source cache reconciliation
also accounts for the current shared geometry/proxy inputs.

The automated exposure model is unchanged. Audit BUG-009's nominal switch-fraction
heuristic was not rewritten or represented as fixed. These visible defects were
resolved through real fixture energy, schedules, materials and missing runtime
light contributions.

## Whole-house moving review and honest limits

The opt-in integration review uses the normal CnaHouseGame controller and
collision-resolved positions sampled from the existing GrandTour, with four
eye-height pan views at each arrival. It records four consecutive frames on
cell changes. It never substitutes a static score or collision-only PASS for
visual review, and refuses a live desktop display.

**All 90 intended-accessible cells have inspected clear 10:30 and scheduled
23:00 views.** The union of the ground/basement section (32 cells) and upper/
remaining-property section (58 cells) covers the complete property at both times.
Corrected service rooms and garage were then replayed separately at both times.
The accepted views retain doors, floor edges, fittings and navigation cues.
The red wine cellar and green bedrooms remain darker than work/service areas;
already-correct halls and hero areas have not been globally washed out.

This is **not** a claim of a human manual whole-house play session, nor of a
passing uninterrupted 90-cell GPU replay. The first full day replay stopped at
a main-stair trace corner (sample 3730); its sampled waypoint was too close to
the south bridge jamb. The trace now targets the centre of the clear entry.
The later full night replay stopped at sample 3623 in B1_STAIR, feet
(3.301483,-2.281139,-19.770359), target
(3.368035,-2.111022,-19.803802). This is a real diagnostic traversal mismatch to
investigate under **HOUSE-03639**, not an external blocker or a hidden success.
The upper 58-cell reviews pass separately. Splitting at the stuck approach
does not omit any room from the day/night visual inspection.

Retained four-view movement strips are in
[day](house-03636/moving-day) and [night](house-03636/moving-night).
The [capture index](house-03636/capture-index.json) lists every source PNG,
its SHA-256, review copy and the exact 90-cell coverage at each time.
Original consecutive frames and full-resolution PNGs remain in
`build/test-output/lighting-walk-*`.

## Matched before/after evidence

The BEFORE photographs use the **existing independent audit snapshot binary**
(`../house-simulator-audit/build/cna-house`, SHA-256
`fd7158468634e86bd4c36e7acdb5dd009c7364cf323cc78d3e8577b04fd01fea`)
and its existing worktree/content, read-only. This is historical audit data,
including its bake experiments, **not a newly built pristine HEAD baseline**.
AFTER uses this repository's current normal game and content. Both use identical
standing points from `docs/zones.json`, yaw 90°, pitch 0°, clear weather,
frozen 10:30/23:00 and frame 600. All 112 source pairs were inspected.
Some fixed views face a wall; the independent four-direction moving views above
are the evidence for whole-room navigation/readability.

| Corrected room / receiver | Day before → after | Night before → after |
|---|---|---|
| B1_CELLAR | [before](house-03636/before-day/B1_CELLAR.webp) → [after](house-03636/after-day/B1_CELLAR.webp) | [before](house-03636/before-night/B1_CELLAR.webp) → [after](house-03636/after-night/B1_CELLAR.webp) |
| B1_CINEMA | [before](house-03636/before-day/B1_CINEMA.webp) → [after](house-03636/after-day/B1_CINEMA.webp) | [before](house-03636/before-night/B1_CINEMA.webp) → [after](house-03636/after-night/B1_CINEMA.webp) |
| B1_GYM | [before](house-03636/before-day/B1_GYM.webp) → [after](house-03636/after-day/B1_GYM.webp) | [before](house-03636/before-night/B1_GYM.webp) → [after](house-03636/after-night/B1_GYM.webp) |
| B1_HOBBY | [before](house-03636/before-day/B1_HOBBY.webp) → [after](house-03636/after-day/B1_HOBBY.webp) | [before](house-03636/before-night/B1_HOBBY.webp) → [after](house-03636/after-night/B1_HOBBY.webp) |
| B1_LAUNDRY2 | [before](house-03636/before-day/B1_LAUNDRY2.webp) → [after](house-03636/after-day/B1_LAUNDRY2.webp) | [before](house-03636/before-night/B1_LAUNDRY2.webp) → [after](house-03636/after-night/B1_LAUNDRY2.webp) |
| B1_STOR1 | [before](house-03636/before-day/B1_STOR1.webp) → [after](house-03636/after-day/B1_STOR1.webp) | [before](house-03636/before-night/B1_STOR1.webp) → [after](house-03636/after-night/B1_STOR1.webp) |
| B1_STOR2 | [before](house-03636/before-day/B1_STOR2.webp) → [after](house-03636/after-day/B1_STOR2.webp) | [before](house-03636/before-night/B1_STOR2.webp) → [after](house-03636/after-night/B1_STOR2.webp) |
| B1_UNDERSTAIR | [before](house-03636/before-day/B1_UNDERSTAIR.webp) → [after](house-03636/after-day/B1_UNDERSTAIR.webp) | [before](house-03636/before-night/B1_UNDERSTAIR.webp) → [after](house-03636/after-night/B1_UNDERSTAIR.webp) |
| B1_WORKSHOP | [before](house-03636/before-day/B1_WORKSHOP.webp) → [after](house-03636/after-day/B1_WORKSHOP.webp) | [before](house-03636/before-night/B1_WORKSHOP.webp) → [after](house-03636/after-night/B1_WORKSHOP.webp) |
| L0_DINING | [before](house-03636/before-day/L0_DINING.webp) → [after](house-03636/after-day/L0_DINING.webp) | [before](house-03636/before-night/L0_DINING.webp) → [after](house-03636/after-night/L0_DINING.webp) |
| L0_GARAGE | [before](house-03636/before-day/L0_GARAGE.webp) → [after](house-03636/after-day/L0_GARAGE.webp) | [before](house-03636/before-night/L0_GARAGE.webp) → [after](house-03636/after-night/L0_GARAGE.webp) |
| L0_KITCHEN | [before](house-03636/before-day/L0_KITCHEN.webp) → [after](house-03636/after-day/L0_KITCHEN.webp) | [before](house-03636/before-night/L0_KITCHEN.webp) → [after](house-03636/after-night/L0_KITCHEN.webp) |
| L0_LAUNDRY | [before](house-03636/before-day/L0_LAUNDRY.webp) → [after](house-03636/after-day/L0_LAUNDRY.webp) | [before](house-03636/before-night/L0_LAUNDRY.webp) → [after](house-03636/after-night/L0_LAUNDRY.webp) |
| L0_STOR | [before](house-03636/before-day/L0_STOR.webp) → [after](house-03636/after-day/L0_STOR.webp) | [before](house-03636/before-night/L0_STOR.webp) → [after](house-03636/after-night/L0_STOR.webp) |
| L1_BED5 | [before](house-03636/before-day/L1_BED5.webp) → [after](house-03636/after-day/L1_BED5.webp) | [before](house-03636/before-night/L1_BED5.webp) → [after](house-03636/after-night/L1_BED5.webp) |
| L1_MASTER_BED | [before](house-03636/before-day/L1_MASTER_BED.webp) → [after](house-03636/after-day/L1_MASTER_BED.webp) | [before](house-03636/before-night/L1_MASTER_BED.webp) → [after](house-03636/after-night/L1_MASTER_BED.webp) |
| L1_WC3 | [before](house-03636/before-day/L1_WC3.webp) → [after](house-03636/after-day/L1_WC3.webp) | [before](house-03636/before-night/L1_WC3.webp) → [after](house-03636/after-night/L1_WC3.webp) |
| L1_WC4 | [before](house-03636/before-day/L1_WC4.webp) → [after](house-03636/after-day/L1_WC4.webp) | [before](house-03636/before-night/L1_WC4.webp) → [after](house-03636/after-night/L1_WC4.webp) |
| L2_BED7 | [before](house-03636/before-day/L2_BED7.webp) → [after](house-03636/after-day/L2_BED7.webp) | [before](house-03636/before-night/L2_BED7.webp) → [after](house-03636/after-night/L2_BED7.webp) |
| L2_GAMES | [before](house-03636/before-day/L2_GAMES.webp) → [after](house-03636/after-day/L2_GAMES.webp) | [before](house-03636/before-night/L2_GAMES.webp) → [after](house-03636/after-night/L2_GAMES.webp) |
| L2_LIBRARY | [before](house-03636/before-day/L2_LIBRARY.webp) → [after](house-03636/after-day/L2_LIBRARY.webp) | [before](house-03636/before-night/L2_LIBRARY.webp) → [after](house-03636/after-night/L2_LIBRARY.webp) |
| L2_SITTING | [before](house-03636/before-day/L2_SITTING.webp) → [after](house-03636/after-day/L2_SITTING.webp) | [before](house-03636/before-night/L2_SITTING.webp) → [after](house-03636/after-night/L2_SITTING.webp) |
| L2_STAIR_ATTIC | [before](house-03636/before-day/L2_STAIR_ATTIC.webp) → [after](house-03636/after-day/L2_STAIR_ATTIC.webp) | [before](house-03636/before-night/L2_STAIR_ATTIC.webp) → [after](house-03636/after-night/L2_STAIR_ATTIC.webp) |
| L2_WC5 | [before](house-03636/before-day/L2_WC5.webp) → [after](house-03636/after-day/L2_WC5.webp) | [before](house-03636/before-night/L2_WC5.webp) → [after](house-03636/after-night/L2_WC5.webp) |
| L3_STORE_E | [before](house-03636/before-day/L3_STORE_E.webp) → [after](house-03636/after-day/L3_STORE_E.webp) | [before](house-03636/before-night/L3_STORE_E.webp) → [after](house-03636/after-night/L3_STORE_E.webp) |
| L3_STORE_N | [before](house-03636/before-day/L3_STORE_N.webp) → [after](house-03636/after-day/L3_STORE_N.webp) | [before](house-03636/before-night/L3_STORE_N.webp) → [after](house-03636/after-night/L3_STORE_N.webp) |
| L3_STORE_S | [before](house-03636/before-day/L3_STORE_S.webp) → [after](house-03636/after-day/L3_STORE_S.webp) | [before](house-03636/before-night/L3_STORE_S.webp) → [after](house-03636/after-night/L3_STORE_S.webp) |
| L3_STORE_W | [before](house-03636/before-day/L3_STORE_W.webp) → [after](house-03636/after-day/L3_STORE_W.webp) | [before](house-03636/before-night/L3_STORE_W.webp) → [after](house-03636/after-night/L3_STORE_W.webp) |

Final matched-pose indoor pairs precede the last outdoor-only binding change,
which does not alter indoor receivers. Local outlier controls and movement pans
include the final source/material calibration. The outdoor controls below use
the same pose (0,0,-6), yaw 90°, pitch -10° and the latest renderer.

| Outdoor control | Before | After |
|---|---|---|
| Clear 10:30 | [day](house-03636/outdoor-before-day.webp) | [day](house-03636/outdoor-after-day.webp) |
| Scheduled 23:00 | [night](house-03636/outdoor-before-night.webp) | [night](house-03636/outdoor-after-night.webp) |

These controls preserve the intentionally dark night garden and are not a
claim that every outdoor pixel got brighter. The moon-plus-practical regression
proves the previously missing contribution at its authored local receiver.

## Validation

Passed:

- Existing Debug build, two workers, shared ccache; runtime remains XNA-only.
- **1504/1504 unit tests**, including schedule, daylight-only portal transfer,
  actual receiver-energy and zero-diffuse override regressions.
- **9/9 StaticGeometryPass device integration tests**, including fixture-dependent
  bounce bounds and moon-plus-practical actual renderer bindings.
- Upper moving day/night reviews: 58 unique cells each, both PASS.
- Final local B1_STOR2/B1_LAUNDRY2 and mudroom/garage movement slices:
  day and night, all four PASS.
- Corrected outdoor path moving day/night slices: both PASS.
- Post-lighting threshold movement matrix: **20 directional crossings / 720
  consecutive GPU frames, PASS** at 10:30 and 23:00. Minimum 5 draws, never zero;
  maximum consecutive HUD-cropped RGB mean difference 8.192/255 at the moving
  foyer/stair view. [Frame grids and numeric report](house-03636/threshold-regression)
  retain this exact run, not the earlier flash-fix photographs. No sky-only
  threshold frame reappears; differences alone are not a visual-quality metric.
- Full artificial/daylight source-cache verification: all 78 cells current.
- Final world-content rebuild/deployment and existing texture build: PASS;
  366 texture products current, zero failed.
- Full `tools/ci/run_checks.sh`: every task-owned gate passes, including
  **344 strict-XNA translation units** (109 destructor hits exempt), two workers.
  Its nonzero exit is exclusively the inherited root `.claude` layout entry.
- `git diff --check`, generated licence and manifest-report checks: PASS.
- Existing 90-cell collision GrandTour: 563 stops, 87,718 controller steps,
  16 collision detours, PASS. This does not replace the GPU/input evidence.

Known non-passing diagnostics:

- Full moving day and night replay stops described above; neither is called PASS.
- Full static gate retains the pre-existing owner-owned root `.claude` layout
  failure. No owner settings are moved/deleted and the gate is not weakened.
- The inherited collision-builder exterior-car census is stale after cars moved
  into ordinary props; the previously reported whole selftest is not called PASS.

No FPS, draw-count, memory or platform performance improvement is claimed.
Shared-machine visual timings are not an uncontended benchmark. No Android
emulator, audible-device check, release or OPTIONAL work is part of this change.
