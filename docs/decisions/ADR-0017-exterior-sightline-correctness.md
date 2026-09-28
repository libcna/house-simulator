# ADR-0017 — Preserve real exterior doorway sightlines

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-28 |
| **Task** | `HOUSE-03640` (still open for wall-seam investigation) |
| **Supersedes in part** | [ADR-0004](ADR-0004-portal-visibility.md): the blanket shallow cap for clear glazed doors |
| **Owns** | `cna-house.md` §16.4/§25.2; existing `SpatialIndex`, `PortalTraversal` and `PortalDepth` |

## Context

The normal Radeon/OPENGLES3 game shows the rear landscape through the enclosed garage
from west road pose `(0,0,3)`, yaw 39°. Its open opaque doorway is excluded from direct
exterior seeding; the narrow driveway/gate graph does not reach it at that angle.
The upper Juliet inspection pose is assigned to the lower balcony's tall open-air
volume. Correcting that exposes another failure: a one-room clear-glass-door cap drops
the hall behind the landing, showing sky and floating facade frames through its opening.
Current before/after GPU images and failing-then-passing regressions establish these
House-owned D13 failures. This does not make the facade-only Juliet player-accessible.

## Decision

Keep portal traversal, reduced cones, the exterior BVH and all existing budgets.

* Neighbour/grid membership selects the highest containing declared storey when open
  volumes overlap across floors. Same-storey authored order and incremental hysteresis
  remain unchanged.
* A facing, clipped vertical exterior doorway may seed its interior without an already
  visited outdoor movement cell. Closed opaque doors, projected-area and depth checks
  still apply. Direct windows keep their outdoor-walk and attic-receiver safeguards.
* Clear glazed doors use the existing six-hop door allowance, shut or open. Open
  frosted doors also use that allowance. Ordinary windows and shut frosted glazing
  retain their previous shallow allowances. Actual leaf geometry still occludes via
  the existing depth buffer; no door/collision state is changed.

## Alternatives and consequences

Drawing the entire house or removing culling would conceal the defects at unnecessary
cost; adding a separate occluder renderer would introduce infrastructure these fixes
do not need. Raising every window's cap is not necessary for the demonstrated doorway
failures. Deleting balconies or calling the garage a carport would change the product.

Some sightlines now retain more genuinely visible rooms. Focused closed-door/window
tests and the exact 24-pose fixture remain bounded; only one pose gains one exterior
cell (104 → 105 total sightings). No new renderer, content family or sibling change.
Acceptance of the complete task still requires its remaining wall-seam investigation;
this decision and passing unit tests alone do not close it.
