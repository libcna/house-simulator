# Current owner stair screenshots, walking/running and terrace correction

2026-09-28: HOUSE-03638 reopened by the owner's two 12:01/12:03 screenshots;
HOUSE-03573 refined by the explicit removal of Normal/Fast; HOUSE-03646 split
out under R7/R15 when traversal exposed the constructed terrace dependency.
This review supersedes the earlier main-guard/art acceptance, not the owner's
observations. Earlier attic reconstruction evidence remains historical context.

## Actual causes and bounded corrections

* The main floor guards followed the raw visibility aperture at Z=-14.8,
  crossing solid floor in front of the south window and omitting the actual
  exposed edge. Shell and collision now share a contour of the slab void minus
  the real step-off strips. L1's edge is Z=-15.58; L2's is Z=-15.30. Only actual
  arrival/departure lanes interrupt guards. There is no wall-side duplicate.
* L0/L1 artworks use a bottom origin, not a centre origin. Old bottoms 1.80/4.85
  buried the images beneath half-landings. Bottoms 2.60/5.55 clear them without
  penetrating ceilings. Rejected 3.00/6.00 placements exceeded room bounds.
* Visible cross-landing boxes now stop at their arrival aperture instead of
  duplicating the upper floor's coplanar faces. Their physical bridge remains
  continuous. The changed stair receivers were regenerated, unwrapped and baked
  with the existing tools, not a new lighting system or global ambient change.
* The +0.45 terrace now owns its constructed slab. Ground/fall use the same
  terrain-ownership flag as depenetration. The single visible 150 mm/350 mm step
  and existing collision ramp centre on the actual slider at X=-2, meeting the
  sunroom slab at Z=-32.25. Two loungers move south 0.80 m to clear the approach.
  Fixed glazing remains fixed: X=-2 points into its sash, not the open lane.
* Step-down may not settle through owned terrain onto a farther walkable slab.
  Preserve rounded/unwalkable edge contacts. A broader terrain replacement was
  rejected: the road curb repeatedly returned the camera to feet Y≈0.0137;
  measured original support normals Y=0.437–0.498 were incorrectly replaced by
  flat terrain. A tiny rounded-kerb fixture protects that exact boundary.

No step/slope/probe/slide/depenetration limit was enlarged. No new asset family,
runtime manager, interaction, optional effect or sibling source change was made.

## Current views and real input

Normal OPENGLES3 Tier S+E game, virtual offscreen/surfaceless GPU, 1600×900;
clear 12:00 and scheduled 23:00. Reviewed WebP copies are 1200×675. No monitor
window. [Source and image hashes](house-03638-reopened/evidence.sha256).
The selected surfaceless EGL driver reports AMD Radeon 780M (radeonsi/phoenix),
EGL 1.5 and OpenGL ES 3.2 Mesa 25.0.7; driver evidence is in
`/tmp/house-03638-egl-driver.log`. The separate GBM fallback was not selected.

| Owner-visible correction | Before | Current day | Current night |
|---|---|---|---|
| False window-side guard | [before](house-03638-reopened/window-before.webp) | [window](house-03638-reopened/window-after-day.webp) | [window](house-03638-reopened/window-after-night.webp) |
| L0 buried painting | [before](house-03638-reopened/l0-art-before.webp) | [complete art](house-03638-reopened/l0-art-after-day.webp) | [complete art](house-03638-reopened/l0-art-after-night.webp) |
| L1 buried painting | [before](house-03638-reopened/l1-art-before.webp) | [complete art](house-03638-reopened/l1-art-after-day.webp) | [complete art](house-03638-reopened/l1-art-after-night.webp) |
| Actual L1 edge | owner report | [guard](house-03638-reopened/l1-edge-after-day.webp) | [guard](house-03638-reopened/l1-edge-after-night.webp) |
| Actual L2 edge | owner report | [guard](house-03638-reopened/l2-edge-after-day.webp) | [guard](house-03638-reopened/l2-edge-after-night.webp) |
| Attic level entry | previous review | [entry](house-03638-reopened/attic-approach-after-day.webp) | [entry](house-03638-reopened/attic-approach-after-night.webp) |
| Attic head guard | previous review | [head](house-03638-reopened/attic-edge-after-day.webp) | [head](house-03638-reopened/attic-edge-after-night.webp) |

The additional L2 artwork day/night views are retained alongside those images.
Six current main-stair eye-height views: [foyer](house-03638-reopened/main-foyer-final.webp),
[bottom](house-03638-reopened/main-bottom-final.webp),
[landing/turn](house-03638-reopened/main-landing-final.webp),
[second flight](house-03638-reopened/main-second-final.webp),
[upper exit](house-03638-reopened/main-upper-final.webp),
[back down](house-03638-reopened/main-down-final.webp).
The landing uses the position actually reached by ordinary input, not an
unsettled capsule dropped into the centre of the well; the latter draft was rejected.

Actual KeyboardMouseSource W/D and mouse delivery on private software Xvfb is
**additional input evidence, not substituted GPU visual acceptance**. Main route
reaches L1 at feet (4.525,3.650,-15.533), with no waypoint steering or noclip:
[main exit HUD](house-03638-reopened/input-main-upper.webp). Ordinary W also
enters the [west attic room](house-03638-reopened/input-room.webp) at
(1.773,9.300,-19.650), enters the [north store](house-03638-reopened/input-store.webp),
[ascends](house-03638-reopened/input-ascent.webp) through the head to that store,
and [descends](house-03638-reopened/input-descent.webp) to L2 at
(5.850,6.550,-14.750). Black outer padding in these Xvfb root captures is outside
the game window. A too-early black capture and a wait on buffered logs were
rejected; the final script waits for an actual rendered frame before input.

GPU-backed ordinary/slow W movement crosses the open sunroom slider in both
directions. Five slow-up and two images per normal direction are under
`build/test-output/house-03646-after/`; all sequences were reviewed. Unit coverage
also crosses at 0.35, 2.05 and 4.00 m/s both ways without a controller bypass.

## Walking/running and filming

Settings has no Normal/Fast selector. Default walk is 2.05 m/s; either Shift
toggles session running at 4.00 m/s. Holding both cannot double-toggle. New/reset
players walk; old JSON remains readable and its retired mode is ignored. Settings
edits do not reset running. Touch's existing button says WALK/RUN; no Android
device validation is claimed here. [Actual Settings page](house-03638-reopened/settings-page.webp)
proves selector removal; it is not a typography/low-resolution polish acceptance.

Measured 20 m travel: walking 2.049989 m/s, running 4.000013 m/s. Existing
acceleration/deceleration and stair/crouch factors are unchanged. Cinema's bounded
pace is 0.90 m/s, previously 0.70: complete fixed-step route 256959 steps,
90 views, approximately 35.69 simulated minutes versus the accepted old
294140-step/40.85-minute route. This is route duration, not an FPS benchmark.
Only obstructed L2/slider transit points move; 2422 points, all 90 rooms/grounds
views and stopping/turn tolerances remain. The GrandTour robot preserves its
original 2.05 m/s pace (now walking); running is separately exercised on guards,
stairs and thresholds, not falsely certified by an unbraked corner-cutting robot.

## Validation and traceability

* Fresh ordinary build passes, shared ccache, existing build/, -j2; no probe-build
  substitution. An intermediate concurrent CNA Game.cpp syntax error was later
  fixed in that other session; no sibling changes were made here.
* Focused step-assist/closed-door suite: 14/14 PASS. Both complete controller routes
  PASS: 90-cell GrandTour, 566 stops/93118 steps/19 detours; filming 90 views.
* Full unit suite: 1529 PASS, one unchanged SkySystem wall-clock assertion FAIL
  under shared contention (max 9.05 ms versus 5 ms). The exact test rerun alone
  PASS; no assertion or production sky code weakened.
* Full virtual GPU integration: 167 PASS, three opt-in long-review SKIP, zero FAIL.
  Actual Settings edit/restart rerun with page capture PASS.
* Required run_checks.sh: strict XNA 353 clean, 109 destructor exemptions; all
  gates PASS except inherited owner-owned `.claude` root-layout violation. Our
  intermediate Vector3 += test violation was fixed, not exempted.
* Prop placement (623 rows), shell realism/material/manifest, content pipeline,
  licence/budget consistency and whitespace checks pass. Wrong generation-only
  manifest invocation was corrected; stale geometry captures are not acceptance.

Logs: `/tmp/house-03573-{focused12-units,focused12-gpu,final12-units,final12-integration,
final13-focused,static13-final,settings-page}.log`; current content pipeline and
bakes `/tmp/house-03638-*-generated-final.log`; input
`/tmp/house-03638-main-input-final.log` and `house-03638-attic-input-final3.log`.
Durable images/hashes survive those temporary logs. No uncontended performance,
whole-house new-speed video review or subjective audio listening is claimed.

These three corrections share geometry, collision, receiver manifests and route
data. Splitting acceptance commits would leave the changed route crossing old
guards/glass or a slab without consistent support. They use the workflow's
inseparable-batch exception, not unrelated performance/Android/optional work.
