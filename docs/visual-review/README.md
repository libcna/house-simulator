# Visual review ledger

These captures answer “does the playable house look good?”, not “did a pixel change?”. Run
`python3 tools/visual/capture_review.py <short-head>-<round>` from the repository root. The tool
fixes the six player poses, Tier S/High, software Mesa, clear weather, 10:30, the session seed and
capture frame. Strict golden render tests remain separate.

## Round 0 — visual-convergence baseline

Commit: `ff788720` (`2026-09-14`)

Capture: [`captures/ff788720-before`](captures/ff788720-before)

Ranked defects:

1. Normal gameplay hashes material ids into saturated flat colours; architecture is cyan/green,
   brown, pink and blue instead of using the authored material textures.
2. No baked lighting reaches the shell, so rooms have no daylight gradient, fixture pools, bounce
   or useful depth.
3. Foyer, hall, living room, family room and kitchen are visibly empty.
4. Exterior shell, drive and ground read as test geometry rather than a coherent front approach.
5. Trim, frames and glass are geometry, but their debug colours keep them from reading as finishes.

Fixed in this round: baseline only.

Remaining: all five defects. `VISUAL-GATE-1` has not passed.

## Round 1 — production material binding

Commit: `HOUSE-00912` working tree (`2026-09-14`)

Capture: [`captures/house-00912-production-r2`](captures/house-00912-production-r2)

Ranked defects:

1. The interior views are severely underexposed. Real albedo and the daylight atlases are bound,
   but the single-pass daylight-only composition leaves most rooms nearly black.
2. Foyer, hall, living room, family room and kitchen remain empty, so the route has no domestic
   scale cues or room identity once the debug palette is removed.
3. The front approach is materially coherent but reads mostly as dark silhouette against the sky;
   façade, drive, ground and vegetation need useful daylight contrast.
4. The review command requests 10:30 and clear weather, but the HUD still reports 07:01. Those
   command-line review overrides are parsed but are not yet applied to the simulation.
5. Trim, frames and glass now use authored finishes but cannot be judged reliably at this exposure.

Fixed in this round: normal `--scene=walk` rendering no longer hashes material ids into saturated
colours. It resolves canonical albedo plus per-cell baked daylight textures through the production
`DualTextureEffect` path. `--scene=blockout` retains the old palette as an explicit diagnostic.
All 78 receiver cells own deterministic daylight bindings and 124 artificial-group bindings.

Remaining: additive artificial/daylight composition, ambient floor, exposure, fixed review time,
furniture and finish inspection. `VISUAL-GATE-1` has not passed.

## Round 2 — live daylight composition

Commit: `HOUSE-01264` working tree (`2026-09-14`)

Capture: [`captures/house-01264-daylight-r1`](captures/house-01264-daylight-r1)

Ranked defects:

1. Exposure is now unambiguously the largest defect: even at a fixed clear 10:30, wall and ceiling
   albedos are almost black outside the strongest baked texels.
2. Every selected interior is empty; the newly readable window and trim silhouettes have no
   furniture-scale context and do not yet form believable rooms.
3. The exterior has a plausible blue daylight background but the house, drive, planting and
   ground remain nearly black line work.
4. Bright window apertures clip against dark interiors, making the contrast look like an
   unexposed camera rather than adapted human vision.
5. Material scale and finish quality cannot be judged on most walls until exposure is present.

Fixed in this round: `LM_DAY` is a final sky-tinted additive, depth-equal/no-write pass after the
opaque and additive artificial groups. The canonical capture's `--time=10.5` and
`--weather=W_CLEAR` now drive the one shared simulation, and `--freeze-time` keeps all six views
at exactly 10:30; the live weather cloud cover reaches the daylight model each frame. Compared
with Round 1, windows and nearby trim/floor receive cool daylight and the exterior sky is blue.
The 12 first-person and 5 HUD/season-sun golden references were intentionally replaced after
inspection because their pre-production predecessors still showed the saturated blockout palette.
Geometry culling is independently compared across 18 paired views through explicit
`--debug-blockout-materials`; the worst pair remains under the unchanged 0.2% threshold.

Remaining: implement the approved exposure model next, then reassess lightmap calibration before
placing primary furniture. `VISUAL-GATE-1` has not passed.

## Round 3 — cell-aware adaptive exposure

Commit: `HOUSE-01266` working tree (`2026-09-14`)

Capture: [`captures/house-01266-exposure-r1`](captures/house-01266-exposure-r1)

Ranked defects:

1. The front façade, drive and surrounding ground still collapse into almost the same value as the
   sky; the exterior reads as sparse dark line work rather than a solid house.
2. Foyer and living-room bakes remain substantially darker than the kitchen and family-room bakes;
   increasing global exposure farther would flatten the rooms that are already readable.
3. Every selected interior remains empty, so there are no domestic scale cues, seating groups,
   kitchen work surfaces or lived-in detail.
4. The peeling `paint_warm_fine` surface on doors and maintained-room boundaries is stylistically
   wrong and competes with the otherwise cleaner interior finishes.
5. Window apertures retain harsh white/blue contrast and the route has no localized sun patches or
   fixture-visible pools to anchor the lighting.

Fixed in this round: each canonical room publishes a bounded exposure target from its final light
level, and the camera adapts asymmetrically when it crosses cells. The strict Tier-S path applies
the lift through existing world effects and the sub-unity exterior residual through a black tint
quad below the unaffected HUD. The kitchen, family room, central hall and foyer stair now expose
recognizable wall, floor, ceiling and trim materials where Round 2 quantised them close to black.
Six times was selected from the fixed views after four times proved insufficient; the adaptation
does not invent light or alter the baked hierarchy. Twelve first-person, one HUD/readout and four
season/sun references were inspected side by side and intentionally updated. All 18 culling pairs
remain equivalent, with the worst still below the unchanged 0.2% threshold.

Remaining: correct the exterior material/lighting collapse first, replace the inappropriate door
finish, calibrate the darkest main-floor bakes, then place primary furniture. `VISUAL-GATE-1` has
not passed.
