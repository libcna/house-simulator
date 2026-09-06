# ADR-0007 — Project-owned kinematic collision, not a physics engine

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-06 |
| **Task** | `HOUSE-00013` |
| **Depends on** | [ADR-0001](ADR-0001-xna-only.md), [ADR-0011](ADR-0011-no-runtime-dependencies.md) |
| **Owns** | `cna-house.md` §49 |

## Context

XNA 4.0 has no collision and no physics. It gives `BoundingBox`, `BoundingSphere`,
`BoundingFrustum`, `Ray` and `Plane`, and stops there. Everything the player does — walking, going
up stairs, being stopped by a wall, being pushed by a swinging door, standing on a floor — has to
be written, either by us or by a middleware library.

## What the world actually needs

| Requirement | Needs a rigid-body engine? |
|---|---|
| Player capsule vs. static world, slide, step, slope | No — a swept capsule and ~4 depenetration iterations |
| Camera sphere sweep (third person) | No |
| Door swing volumes affecting the player | No — an animated OBB, tested per frame |
| Pets on a navigation graph | No |
| The car sitting in the garage | No — it never moves |
| Nudgeable small props (a chair, a ball, a box) | A *very* small amount: gravity, a support plane, friction and a push impulse. ~200 lines. |
| Stacking, joints, ragdolls, vehicles, cloth, destruction | **Nothing in the brief needs any of it** |

## Decision

**A project-owned deterministic kinematic collision system.** Bullet, Jolt, PhysX and friends are
rejected.

* Static collision is built **offline** from the layout and the per-asset `_COL` proxies into
  `content/world/collision.bin`: per cell, a list of OBBs (walls, floors, ceilings, furniture
  bounds) and a small list of triangle meshes (stair ramps, the terrain patch, the attic roof
  underside, curved props), indexed by a per-cell 1 m loose grid so a capsule sweep tests ~6 shapes
  rather than 900. Total ~4 300 OBBs, 18 triangle meshes (~9 000 triangles) and the exterior height
  field — under 3 MB.
* The player is a capsule integrated at a **fixed 1/120 s step** accumulated from `GameTime`, at
  most 4 steps per frame, with up to 3 sweep-and-slide iterations per step. Fixed-step integration
  is what makes the result reproducible for tests and for saves.
* The public interface is deliberately narrow: `SweepCapsule`, `SweepSphere`, `RayCast`, `Overlap`,
  `GroundProbe`. Nothing else may be added without a reason recorded here.

**The swap-in seam is named.** If a future feature genuinely needs rigid bodies, those five
functions are the seam: `PhysicsSystem` would be reimplemented over Jolt behind them, and no
caller would change. That is the *only* sanctioned route back to middleware, and it must come with
a feature that needs it — not with a preference.

## Alternatives considered

| Candidate | Verdict |
|---|---|
| **Bullet** | Rejected. Mature, but the largest of the three in binary and API surface, and its determinism story requires care we would have to establish anyway. |
| **Jolt** | Rejected *for now*, and the named swap-in if the decision is ever reversed: modern, deterministic by design, good Emscripten story. Still 2–8 MB of binary and a second allocator for a world with three moving characters. |
| **PhysX** | Rejected. Heaviest integration, least appropriate licence/footprint fit for this project. |
| **Write it ourselves** | **Chosen.** |

Costs avoided: 2–8 MB of binary, an Emscripten build integration, a second memory allocator, a
determinism story that would have to be re-established for saves and tests, and a large API surface
that invites over-use — the last being the real risk, because an available rigid-body solver is an
invitation to solve a nudgeable chair with a general constraint solver.

Cost incurred: ~1 200 lines of well-tested collision code that we fully understand and can make
deterministic by construction.

## Consequences

* Determinism is a property we own and must test: the same input sequence from the same state
  produces the same positions, asserted by unit tests, and the fixed step is what makes that
  possible.
* Guarantees are tested, not assumed (`cna-house.md` §49.5): the player can never leave the world,
  never pass a closed door, never fall through a floor, can traverse every stair, and fits through
  every portal — the last checked by the world validator's capsule-clearance rule, offline, for all
  186 portals.
* Anything genuinely needing stacked rigid bodies is out of scope by decision, not by accident.
* Collision proxies become an authoring obligation: every model carries `<name>_COL`, convex or
  box-decomposed, ≤ 64 triangles, which the content build extracts and strips from the runtime
  model (`docs/content-authoring.md`).
