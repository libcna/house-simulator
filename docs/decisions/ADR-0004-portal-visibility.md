# ADR-0004 — Portal visibility with frustum reduction

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-06 |
| **Task** | `HOUSE-00010` |
| **Depends on** | [ADR-0001](ADR-0001-xna-only.md), [ADR-0002](ADR-0002-renderer-selection.md) |
| **Owns** | `cna-house.md` §25 |

## Context

The world is a five-level house of ~935 m² above grade with ~95 cells and ~186 portals, ~2 400
static prop placements, a garage, a lot and a neighbourhood. Drawing it all and letting the depth
buffer sort it out is not affordable at 1920×1080/60 Hz on the reference GPU, and a bounding-box
frustum sweep barely helps indoors: from the middle of the house, most of the house is inside the
frustum and behind a wall.

The visibility question in a house is *dynamic*: it depends on which doors are open. Closing the
kitchen door must genuinely remove the kitchen from the frame.

XNA gives us `BoundingFrustum` (with `GetCorners` and `Intersects(BoundingBox)`), `BoundingBox`,
`Ray` and `OcclusionQuery`, and nothing above them.

## Decision

**A portal system: cell-to-cell traversal from the camera's cell, reducing the frustum at every
portal it crosses.** Doors and windows are portals whose aperture *is* the door/window state.

* A portal is always an **axis-aligned rectangle on an axis-aligned plane**. Every door and window
  in this house sits in a rectilinear wall, and the restriction makes the clipping test exact and
  cheap.
* Traversal starts in the camera's cell with the camera frustum, and for each portal on the
  boundary intersects the portal rectangle with the current frustum, producing a **reduced
  frustum** for the neighbouring cell. Empty intersection prunes the branch.
* A portal's `opacity` decides what passes: `open`, `opaque_when_closed` (vision, passage and most
  sound blocked when shut), `glass` (vision passes, passage blocked, `maxDepth`-capped) and
  `translucent` (vision passes but the reduced frustum is marked *diffuse*, so the target cell
  renders at LOD+1 with no small props).
* A partially open door yields a partially open aperture rectangle, so a door ajar reveals exactly
  the wedge it should.
* The result is a visible cell set, each with its own reduced frustum, which the render list uses
  for per-chunk and per-instance culling.
* `OcclusionQuery` is **not** used for general culling; see below.

Correctness is provable and is proved: unit tests assert that a reduced frustum contains every
point the portal rectangle can see and no point outside the parent frustum (10⁵ random samples),
and the render-regression suite renders the 12 budget poses with culling on and with culling
disabled and requires the images to match (`cna-house.md` §70.4, family *culling sanity*).

## Alternatives considered

**Stencil portals.** *Impossible here.* `GraphicsDevice::Clear` ignores `ClearOptions::Stencil` and
`ReferenceStencil` has no renderer connection on EasyGL, and EasyGL render targets allocate
`DepthComponent24` with no stencil at all (BL-02). Frustum reduction needs no stencil and is
cheaper anyway.

**Precomputed PVS / BSP.** Rejected. A PVS would give a perfect potentially-visible set for a
*static* world. Here visibility depends on door state: with ~62 door-bearing portals, a
door-state-indexed PVS would need 2⁶² tables. Portal traversal computes the exact answer at
runtime, for the actual door states, in well under a millisecond.

**Bounding-volume-hierarchy frustum culling only.** Rejected as insufficient: it culls what is
outside the frustum, and the expensive case here is what is inside the frustum and behind a wall.
A BVH remains useful *within* a visible cell and is not excluded by this decision.

**Per-object `OcclusionQuery` culling.** Rejected, for four reasons worth recording: portal
visibility already answers the indoor question exactly and for free; a query result arrives at
least one frame late, which produces popping; on the ES 3.x profile `PixelCount` degrades to a
boolean (BL-07), so "how much is visible" is unavailable; and each query costs a draw call and a
GPU→CPU sync. `OcclusionQuery` is used for exactly one thing in the main path — sun and moon glare
(`cna-house.md` §32.4). One further use is *permitted only if measured*: the house body occluding
the neighbourhood from the back garden, gated by `HOUSE-02401` on a hard threshold of ≥ 0.6 ms GPU
saved at no visual cost, and dropped if the measurement does not clear it.

**Render-to-texture portals (as for mirrors).** Not a general visibility mechanism — it costs a
full scene pass per portal. Retained only for mirrors, where a single low-resolution pass is
affordable, and only because stencil mirrors are impossible (BL-02).

## Consequences

* The world data must carry a correct portal graph. `validate_world.py` and the mirrored C++
  validator enforce rules 4, 5 and 7 of `cna-house.md` §15.7: every portal rectangle lies in both
  cells' boundary planes within 1 cm, the graph is connected from `L0_FOYER`, and every door and
  window has exactly one portal.
* Visibility, audio (ADR-0010) and pet navigation all read the same graph. One authored structure,
  three consumers.
* A debug overlay reports visible cells, culled cells, draw calls and portal traversals every
  frame — this is goal G4 and it is testable, not decorative.
* Cells must be convex-ish boxes for the traversal to be sound. The floor-plan schema enforces
  axis-aligned boxes per cell, with multi-box cells for L-shaped rooms.
