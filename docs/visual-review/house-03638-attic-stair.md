# HOUSE-03638 — attic approach, landing, guards and exits

Current normal OPENGLES3 game, invisible Radeon 780M offscreen surface, 1600 × 900;
captures on 2026-09-28, clear 10:30 and scheduled 22:00. The WebP copies are reduced
to 1200 × 675 for review. No window was opened on the owner's physical monitor.
The views include the still-open whole-house lighting correction.

## Reproduced geometry defects

The old straight flight started flush against the south boundary, leaving no level
approach from the L2 side door. Its broad well removed almost the whole upper bay;
the west room opening lay beside a guard rather than on a usable turning landing.
The independent audit's `BUG-011-c05-attic-head-to-l3room-strip.png` records the
old blocked opening. Current probes independently caught a storage box in the new
turning route. The upper well had only a floating top rail, without visible infill.
The old west-wall artworks intersected the stair/well; an intermediate north-wall
placement also clipped the upper art into the sloping roof and was corrected.

## Correction

- `L2_STAIR_ATTIC` gains a 1.00 m level entry; `L2_BATH5` retains its bath, vanity
  and toilet in a 10.3 m² service bathroom, shifted clear of the enlarged stair bay.
- Fifteen 183.3 mm rises use 250 mm goings. The last tread meets the +9.30 m floor
  through the existing generated step-off strip: no jump or uncovered floor gap.
- `L3_STAIR_HEAD` gains a 1.15 m solid turning landing. The east attic store remains
  accessible and furnished. The west doorway lies wholly on that landing.
- The well is narrowed to the flight. Exposed level edges gain newels and vertical
  infill; walls do not acquire redundant rails. Collision guards remain continuous
  outside actual arrival gaps. Buried infill cap faces are omitted to preserve the
  existing shell triangle limit.
- Both artworks are completely visible on their walls. The upper art is lowered
  and scaled to 0.75 to remain below the roof. The storage shelf is moved clear.
- Retired IDs: `PROP_L3_STAIR_HEAD_STORAGE_BOX` obstructed the landing;
  `PROP_L3_STORE_E_DUCT_SOUTH` and `PROP_L3_STORE_E_CABLE_SOUTH` crossed the new
  service-store boundary. The north duct/cable pair remains. Their golden-list
  removals deliberately record this retirement; no room or asset family was removed.

## Actual-game views

| View | Evidence |
|---|---|
| Level L2 approach and first flight | [approach.webp](house-03638/approach.webp) |
| Flight approaching the upper join | [flight.webp](house-03638/flight.webp) |
| Solid upper floor and visible guard | [head.webp](house-03638/head.webp) |
| West room exit from the landing | [room-exit.webp](house-03638/room-exit.webp) |
| North service-store exit | [store-exit.webp](house-03638/store-exit.webp) |
| View down | [down.webp](house-03638/down.webp) |
| Complete L2 artwork | [art-l2.webp](house-03638/art-l2.webp) |
| Complete upper artwork | [art-l3.webp](house-03638/art-l3.webp) |
| Night entry | [night-approach.webp](house-03638/night-approach.webp) |
| Night upper head and exits | [night-head.webp](house-03638/night-head.webp) |

## Movement, not just static poses

Actual W-key delivery through `KeyboardMouseSource`, on isolated software Xvfb
`:222`, independently reached the following cells without steering or teleporting
after spawn. This is automation of normal input, not a claim of human play.

| Held input | Result | HUD |
|---|---|---|
| Up, 13 s | `L3_STORE_E`, feet `(5.850,9.300,-21.961)` | [ascent](house-03638/real-input-ascent.webp) |
| Down, 13 s | `L2_STAIR_ATTIC`, `(5.850,6.550,-14.750)` | [descent](house-03638/real-input-descent.webp) |
| West room, 2 s | `L3_ROOM`, `(4.712,9.300,-19.650)` | [room](house-03638/real-input-room.webp) |
| North store, 2 s | `L3_STORE_E`, still on +9.30 m floor | [store](house-03638/real-input-store.webp) |
| Into guard, 3 s | stopped at `(6.810,9.300,-18.000)`, no fall | [guard](house-03638/real-input-guard.webp) |

The separate real Radeon game regression repeats all five intents with production
cell tracking, fixed steps and rendering. It passes along with the main-stair
gameplay and guard tests and the complete 90-cell GrandTour. Unit coverage protects
ascent without steering, both standing door approaches, guard walk-off and all
eight flights in both directions. The full unit suite passes 1502/1502.

World validation, prop placement, ID retirement, room schedule, shell provenance
and strict XNA checks pass. The overall static script still reports the inherited
root `.claude` layout exception. Its stale slab-face coverage threshold failed after
the well resize despite zero reversed faces; it now follows the measured habitable-room
count, and the shell selftest passes. The refrigerator opening is the recorded
historical shell issue, not a new failing claim.
The collision builder's stair assertions pass; its legacy exterior-car assertion
still expects car OBBs from the former exterior list and reports zero. These are
not silently waived or described as passed. No uncontended performance measurement
was made, and audio was deliberately disabled during these visual tests.
