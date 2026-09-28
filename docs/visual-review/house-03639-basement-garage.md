# HOUSE-03639 — basement approaches and garage platform

2026-09-28, current normal OPENGLES3 game on the invisible Radeon 780M surface.
All GPU captures unset DISPLAY/WAYLAND_DISPLAY, use SDL offscreen/EGL surfaceless
and CPU affinity 0–3. Additional actual keyboard/mouse events use isolated
software Xvfb :229/:230. No window was opened on the owner's physical display.

## Reproduction and correction

The basement flight originally started against the north wall at z=-20.20.
Holding forward downhill stopped at feet y=-1.994 instead of -2.30. Adding a
1 m level north approach with the old 275 mm going left only a 0.50 m head
strip: rule 15 rejected a standing capsule there and the grand tour could not
turn across it. The final sixteen 181.25 mm rises use 250 mm goings, meeting
`2r+g=612.5 mm`. The 1.00 m wide run occupies x=3.60…4.60,
z=-19.20…-15.20, leaving a 1.00 m north foot and 0.90 m south exit in the
unchanged bay. The well follows the new head and the standing point moves to
the clear south strip. No player clearance or movement tolerance was relaxed.

The nested garage loft had an upward one-sided floor, no parent-owned
underside, and floor/guard collision shared with the garage only near its
hatch. From below, boxes and the upper ceiling appeared through the deck.
An above-floor probe fell 2.75 m to the garage slab. The existing generator
now gives raised nested platforms a downward underside, outer fascia and
hatch reveals matching the existing authored 0.35 m collision slab. It does
not duplicate the child's upward finish. Existing floor/guard OBB indices
are shared fully with the parent, not duplicated or limited to hatch reach.
The 0.90 m hatch remains open. Containers under 1 m above their parents are
unchanged. The loft remains non-player-accessible as §13.3 and zones already
declare; there is no new ladder mechanic.

## Normal GPU before/after views

Each pair uses the same CLI feet/yaw/pitch, clear 10:30 or scheduled 23:00,
frame 600, 1600×900. Review copies are 1200×675 WebP. Originals and exact
commands/logs remain under `/tmp/house-03639/{before,after}/`.
The before loft-above/hatch spawn fell through the faulty parent collision;
those are identical requested poses, **not identical settled camera heights**.

| View | Day before → after | Night before → after |
|---|---|---|
| Hall approach | [before](house-03639/before-basement-hall-day.webp) → [after](house-03639/after-basement-hall-day.webp) | [before](house-03639/before-basement-hall-night.webp) → [after](house-03639/after-basement-hall-night.webp) |
| Level foot / flight | [before](house-03639/before-basement-foot-day.webp) → [after](house-03639/after-basement-foot-day.webp) | [before](house-03639/before-basement-foot-night.webp) → [after](house-03639/after-basement-foot-night.webp) |
| Upper join / view down | [before](house-03639/before-basement-head-day.webp) → [after](house-03639/after-basement-head-day.webp) | [before](house-03639/before-basement-head-night.webp) → [after](house-03639/after-basement-head-night.webp) |
| Loft underside | [before](house-03639/before-loft-below-day.webp) → [after](house-03639/after-loft-below-day.webp) | [before](house-03639/before-loft-below-night.webp) → [after](house-03639/after-loft-below-night.webp) |
| Loft upper floor | [before](house-03639/before-loft-above-day.webp) → [after](house-03639/after-loft-above-day.webp) | [before](house-03639/before-loft-above-night.webp) → [after](house-03639/after-loft-above-night.webp) |
| Hatch join | [before](house-03639/before-loft-hatch-day.webp) → [after](house-03639/after-loft-hatch-day.webp) | [before](house-03639/before-loft-hatch-night.webp) → [after](house-03639/after-loft-hatch-night.webp) |

Inspection: the approach, steps and underside are coherent and readable at both
times; the floor is opaque above/below, the hatch remains visible, and the
deck's top finish is not duplicated. No benchmark improvement is claimed from
the HUD frame times on this shared machine.

## Current-controller regression

`HeadlessRunTests.BasementApproachesAndGarageLoftHaveContinuousSupport` runs the
production game/controller on the invisible GPU path with held-forward intent,
not waypoint steering or test-supplied cell changes. All five probes pass:

| Probe | Actual settled result |
|---|---|
| Ascent, x=3.97 and 4.23 | L0_STAIR_MAIN, floor +0.60 |
| Descent, x=4.10 | B1_STAIR, floor -2.30 |
| Loft stationary support | L0_GARAGE tracker, feet `(14,2.90,-19.50)` |
| Loft exposed edge | stopped at `(14,2.90,-18.00)`, no fall |

The parent tracker may retain L0_GARAGE while inside its nested loft; that is
why whole parent collision must be correct. The test intentionally does not
claim this proves a walkable ladder.

Actual `KeyboardMouseSource` W-key runs on Xvfb reach
[L0 at `(4.10,0.60,-14.75)`](house-03639/house-03639-controller-ascent-after.webp)
and [B1 at `(4.10,-2.30,-19.825)`](house-03639/house-03639-controller-descent-after.webp).
Starting in the hall, walking east, [turning south](house-03639/real-input-hall-turn.webp)
with two ordinary mouse motions and holding W reaches
[L0 at `(4.046,0.600,-14.750)`](house-03639/house-03639-controller-hall-upstairs.webp)
on :230. The actual off-centre entry at x=3.937 is not a waypoint-generated path.
This is real input automation, not a claim that a human manually played it.
Early 12-second software runs did not finish the flight; the accepted runs
hold W for 30 seconds. A redundant subsequent OCR-driven retry misread HUD
coordinates and failed to enter the flight; it is not accepted evidence. The
accepted :230 run was inspected directly without OCR-driven steering.

## Checks and evidence

- Whole property collision tour: **90 cells, 564 stops, 86,635 controller steps,
  14 collision detours, PASS** (`/tmp/house-03639-tour-final.log`).
- **1504/1504 unit tests PASS** (`/tmp/house-03639-unit-final.log`); all eight
  flights ascend/descend. An earlier invocation from build/ lacked repo-relative
  fixtures; that invocation is not accepted validation.
- Final shell and shared-stair selftests PASS; new claims protect complete
  parent underside area excluding the hatch and forbid a duplicate top face.
  Main flight census now holds incoming basement arrival gaps constant rather
  than depending on how many unrelated well-infill posts a changed head removes.
- Both selected world/collision/stair integration tests PASS, in addition to
  the new five-probe production-game regression and the complete property tour.
- World validator: all 15 rules PASS; final world build: 14 stages rebuilt,
  two fresh; texture deployment: six rebuilt, 360 cached, no failures.
- Full static checks: 344 strict-XNA translation units clean, two workers.
  Inherited owner-owned `.claude` fails layout; generated floor-plan drift was
  corrected and its individual gate passes. No owner's settings were changed.
  Final licence and resource-budget-report consistency checks pass.

Selection: explicit owner S1/S2 priority override and R1 exception, before
unrelated platform/performance/audio work. The next correction is HOUSE-03640.
