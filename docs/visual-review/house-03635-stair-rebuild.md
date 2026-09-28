# HOUSE-03635 — main stair rebuild review

Captured 2026-09-28 from the current `build/cna-house` OPENGLES3 game, 1600 × 900,
clear 10:30. The game rendered through an invisible GPU-backed offscreen surface;
the checked-in 1200 × 675 WebP copies below are downsized only for review. No test
window was opened on the owner's desktop.

| View | Normal-eye-height capture |
|---|---|
| Foyer approach | [foyer.webp](house-03635/foyer.webp) |
| Foot / bottom flight | [foot.webp](house-03635/foot.webp) |
| Lower flight | [lower.webp](house-03635/lower.webp) |
| Cross-landing | [landing.webp](house-03635/landing.webp) |
| Return flight | [return.webp](house-03635/return.webp) |
| L1 exit | [upper.webp](house-03635/upper.webp) |
| Back down | [down.webp](house-03635/down.webp) |
| Ordinary W/D and mouse, standing at L1 | [real-input-upper.webp](house-03635/real-input-upper.webp) |

The former floor-height return support no longer occupies the foyer entrance. The
17-riser L0→L1 climb now consists of a nine-riser lower flight, full-width landing,
eight-riser return, and a level exit bridge. A thin raked underside replaces the
old column, and a three-strip wool runner continues across the turn. The inner
balustrades terminate at the actual flight edges; the well is flush with both side
walls and the north landing wall, avoiding the former 100 mm overhead slab lips.
Collision guards still protect the exposed drops. The rebuilt
L1→L2 switchback uses the same restrained geometry.

A diagnostic run used the game's ordinary `KeyboardMouseSource` on an isolated
virtual X server. Starting in `L0_FOYER` at feet `(1.95,0.60,-14.90)`, short held
W/D inputs and mouse turns followed the visible two-flight route without a
memorised waypoint sequence. An off-centre foyer entry reached x=2.638 m; the
player crossed the landing at y=2.215 m, turned, and reached `L1_STAIR_MAIN`
standing at `(4.421,3.650,-14.820)`. HUD captures at the landing, return flight
and exit are under `/tmp/house-03635-virtual-controller-*.png`; this is real input
on a virtual display, not a claim that a human manual play session occurred.

The earlier near-wall run exposed two 100 mm slab lips and a flat wall-art
collision proxy. The holes now align with the stair-cell walls, and the three
stair wall artworks rely on their enclosing walls for collision rather than
blocking the walking lane. A focused collision regression checks standing
headroom at both formerly caught edge positions and three positions across the
lane. A repeated ordinary-input run on 2026-09-28 exposed another real defect at
x=2.973 m: touching the ramp and the inner rail spent all three slide iterations
without moving. Its deterministic before regression stops at z=-15.275 and feet
y=0.640, rather than reaching the turn. The existing slide now follows the shared
crease of a walkable sloping plane and a vertical wall; its iteration count and
slope limit are unchanged. A generic ramp/rail fixture and four clear-lane starts
protect this correction. The unsuccessful continuous-rail and sweep-tolerance
experiments were removed, not retained as speculative changes.

The post-fix ordinary W/D/mouse run entered the lower flight at x=2.973 m, crossed
the turn and reached L1 at `(4.202,3.650,-15.543)`. A current-source repeat reached
L1 at `(4.327,3.650,-14.750)`. These are real `KeyboardMouseSource` inputs on an
isolated **software-rendered** Xvfb surface, not a claim of human manual play or
GPU input delivery. The separate invisible **Radeon** game tests validate the
production controller/render path. The checked-in GPU views above were reviewed
from the foyer, both flights, turn, upper exit and looking down.

All eight automated flights pass in both directions. The complete unit suite is
1502/1502, and the current 90-cell GrandTour passes (563 stops, 87,707 controller
steps, 16 collision detours). No performance improvement is claimed: these are
correctness runs on a shared machine. Whole-house lighting remains a separate
open task; these views include that in-progress lighting work.
