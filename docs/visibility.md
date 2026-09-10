# §25 visibility — the algorithm, its parameters, its guarantees, and how to debug it

*`HOUSE-00696`. Descriptive, not normative: `cna-house.md` §25 is the design and this document is
what was built from it, with the numbers the building measured. Where the two differ, §25 wins and
this file is the bug. Every constant below is quoted from the header that owns it; none is repeated
in code.*

**The one-sentence version.** From the cell the camera is in, cross every opening that can be seen
through, narrowing the view to the shape of each opening as you go; the rooms you reach are what is
drawn, and inside each of them only the geometry one of its narrowed views can see.

---

## 1. The pipeline

§25.1's five steps, and the code that is each of them. Steps 1–4 are what §71.2's **visibility**
row budgets; step 5 is charged to draw submission, because the game builds the render list inside
`Draw`.

| Step | What it does | Where it lives |
|---|---|---|
| 1. locate the camera cell | §16.4's point query | `player::CellTracker` (not this subsystem) |
| 2. portal traversal | camera cell → visible cells, each with up to four cones | `visibility::PortalTraversal`, driven by `VisibilitySystem` |
| 3. per-cell chunk test | §17.4's chunks of a visible cell against that cell's own cones | `visibility::ChunkCuller`; instances by `visibility::InstanceCuller` |
| 3′. exterior | §25.6's loose BVH against the cones that reached the outdoors | `visibility::ExteriorCones` and `ExteriorCuller` |
| 4. distance cull and LOD | per-category distances, §68's view-distance scale, §26.4's detail sets | `visibility::CullDistanceFor`, `DetailSets` |
| 5. sort | by pass, then effect, then material; transparent back-to-front | `visibility::RenderList` |

One walk a frame, and everything downstream reads the same answer. Chunk culling, lighting, audio
and residency all consume `VisibilitySystem::Visible()`; a second walk in the same frame would
answer a slightly different question the moment the camera moved between the two calls.

## 2. Step 2 in detail — the walk

Breadth-first from the camera's cell. Each queue entry is a **cone**: a cell, the clipped frustum
the view arrived through, that frustum's screen rectangle, the world-space polygon it was reduced
through, the depth reached, the allowance left, and the flags it picked up.

For each cone popped, in order:

1. **Reach the cell.** A cell reached twice keeps the *shallowest* depth, the *largest* remaining
   allowance, and the *and* of the flags — one clear view of a room means it is not being seen
   through frosted glass.
2. **The containment skip.** If this cone's screen rectangle is inside one the cell has already
   been expanded with, everything it could reach has been reached. Skipped, counted.
3. **Keep the cone**, up to `kMaxFrustaPerCell`. Past that the cell stays visible and the fifth
   cone is dropped: the four kept are wider than the fifth would have narrowed to.
4. **For each portal of the cell**: a closed opaque leaf stops vision and closed glass does not
   (§25.3); a portal whose plane faces away is skipped; the chain's allowance is the **minimum**
   of every cap crossed to get here, so a window from outside admits one room and not that room's
   own open doors; the portal rectangle is clipped against the cone's planes (Sutherland–Hodgman,
   at most `kMaxClippedVertices`); a polygon smaller than `kMinPortalNdcArea` of the screen is not
   worth a room; and the cone that survives all that is reduced to the polygon and queued.

The work queue is a **fixed-capacity ring** (`ConeQueue`), so the walk allocates nothing at all in
the steady state. The visible list is reserved once at construction and only cleared.

**§25.3, apertures.** A door's aperture is a fraction with a latch: below `kClosedBelow` it is shut
and above `kOpenAbove` it is open, and between those two it stays as it was. That hysteresis is why
a door swinging past 0.06 does not make the room behind it flicker in and out of the frame. A leaf
occludes *inside* the target cell; it does not shrink the portal.

**§25.7, going outside.** Crossing an exterior door is an ordinary portal crossing. What is not
ordinary is the join: the cones that arrive in the yards are collected (`ExteriorCones`) and
§25.6's hierarchy is culled against **all of them at once**, in one descent carrying a bit per cone.
Past `kMaxCones` the collection throws its cones away and stands the camera's own frustum in their
place — a superset of every cone it could have produced, so the frame over-draws the garden and
never over-culls it.

## 3. The parameters

Every number the subsystem has, with the header that owns it. Change one here and nowhere else.

| Constant | Value | Header | Why that number |
|---|---|---|---|
| `kMaxFrustaPerCell` | 4 | `PortalTraversal.hpp` | A fifth cone costs another test of every chunk in the room for a view the first four nearly cover. |
| `kMaxVisibleCells` | 30 | `PortalTraversal.hpp` | §71.2 calls 30 visible cells a hard fail. Past it the walk keeps the 30 with the largest share of the screen — the one place §25 culls something the player can see, and it is counted. |
| `kMaxQueuedCones` | 128 | `PortalTraversal.hpp` | The ring's capacity: cones **waiting**, not cones in total. Measured worst frontier in this house, every cell at four headings with every door open: **13**. |
| `kMaxClippedVertices` | 12 | `ClipRect.hpp` | Sutherland–Hodgman adds at most one vertex per clipping plane; a four-sided doorway against eight side planes cannot exceed twelve. |
| `ClipFrustum::kMaxPlanes` | 10 | `ClipFrustum.hpp` | Eight sides from the clipped polygon, plus near and far. |
| `kMinPortalNdcArea` | 1.2e-5 | `PortalArea.hpp` | Of a full screen's 4.0, so **2.8 pixels at 1280×720** — §25.2's *"about 2 × 2"*, and the header records the arithmetic so nobody re-derives 2 × 2 into a different constant. Below it a doorway is not worth the room behind it, and it is what stops a corridor of eight open doors from exploding. |
| `kNoLimit` | 2²⁰ | `PortalTraversal.hpp` | The allowance a chain starts with: nothing has been crossed to reach the camera's own cell. |
| depth caps | 6/2 door, 3/1 glazed, 4/2 garage | `PortalDepth.hpp` | Inside/outside. The asymmetry is the point: from the garden a window is worth **one** room. |
| `kClosedBelow`, `kOpenAbove` | 0.05, 0.08 | `PortalRuntime.hpp` | §25.3's latch. A door drifting across one threshold must not flicker the room behind it. |
| `ExteriorCones::kMaxCones` | 8 | `ExteriorCulling.hpp` | Two rooms' worth of openings onto the garden; past it the camera frustum stands in. |
| BVH `kLevels`, `kBranching`, `kMinToSplit` | 3, 8, 8 | `ExteriorBvh.hpp` | §25.6 asks for three levels; eight ways puts ~4 100 instances in ~64 leaves of ~64. |
| cull distances | 45 / 70 / 120 / 180 / 90 / 160 / 300 / 420 / 420 m | `CullDistance.hpp` | §25.6's nine categories, in its own order. Measured to the instance's **box**, not its centre. The ninth is the GROUND (`HOUSE-00700`) and its 420 m is §10.3's far plane, which is to say no distance test at all: a lawn you cannot see is outside the frustum or behind the far plane, and both are already answered. |
| view-distance scale | 0.6×–1.4× | `CullDistance.hpp` | §68's setting, clamped so a settings file cannot turn the exterior off or reach past the far plane. |
| `kDressingDistance`, `kMicroDistance` | 18 m, 8 m | `DetailSets.hpp` | §26.4's detail sets: dressing dropped beyond 18 m, micro beyond 8 m. A cell every cone into which came through frosted glass adds §15.4's +1 to the LOD bias and drops both. |

## 4. What it guarantees

1. **It does not over-cull, except in three places, and all three are counted.** A room the player
   can see is drawn — unless `frustaDropped` (a fifth cone into one room), `cellsDropped` (§71.2's
   hard stop at 30 cells) or `queueDropped` (the work queue full) is non-zero. Each is a separate
   counter, on the `F3` overlay, silent at zero and loud when it is not.
2. **Two runs of one pose give the same answer, in the same order.** Not merely the same set: the
   render list sorts within it, so a different order is a different frame, and a render fixture
   compares pixel by pixel.
3. **The exterior answer is exactly a brute-force scan.** Every hierarchy test in the suite
   compares the culled set against a linear pass over all 4 100 instances; the hierarchy is an
   optimisation and is asserted to change nothing.
4. **No allocation in the steady state.** Proved directly, with a counting global `operator new`
   over §25.8's 24 poses (`VisibilityAllocationTests`), not inferred from the code.
5. **Reduced cones are a subset of the camera's frustum**, so what arrives in the garden through a
   window is a slice of the garden and not the garden.
6. **A closed opaque door stops vision; closed glass does not.** Frosted glass passes light and
   marks the chain diffuse, for ever after: a room seen through a room seen through frosted glass
   is still being seen through frosted glass.

## 5. What it costs

Measured by `tests/perf/VisibilityCostTests.cpp` over §70.6's ten worst-case scenarios, in an
optimised build, after `HOUSE-00695` and `HOUSE-00699`. §71.2 gives visibility **0.55 ms** typical
and **1.20 ms** worst in a 16.67 ms frame.

| | Measured | Of budget |
|---|---|---|
| Median scenario, steps 1–4 | 0.054 ms | **10 %** of 0.55 ms |
| Worst scenario (`L0_KITCHEN`, seven cones outdoors) | 0.120 ms | **10 %** of 1.20 ms |
| Worst frame of a walk through the whole house | 0.186 ms | 16 % of 1.20 ms |
| Median frame of that walk | 0.016 ms | 3 % of 0.55 ms |
| The portal walk alone | 0.003–0.013 ms | ≤ 2 % of 0.55 ms |
| Step 5, the sort | 0.0003 ms | of a 1.60 ms draw-submission row |

Four consecutive runs agreed to within 3 % on the reference machine of §71.1 — which runs ten
build agents, so a single run is a measurement of the afternoon as much as of the code. Take the
median of several, and compare a change against its own before-and-after built back to back;
absolute numbers taken weeks apart are not comparable.

**Where the time is: the exterior hierarchy, not the portal walk.** The walk is 1–2 % of its own
budget and there is nothing in it to win — `HOUSE-00695` measured before and after and found the
difference inside the noise. The 88–96 % is §25.6, and it scales with the number of **cones** into
the outdoors rather than the geometry in it: `L0_KITCHEN` sees the garden through seven openings.

The exterior instance set used for these numbers is **synthetic** — 4 100 instances in §25.6's
categories over §10.3's extents — because the real vegetation (`HOUSE-00772`) and neighbourhood
(`HOUSE-00852`) are Phase 10 content. Re-take these numbers when it lands.

The real set is no longer empty, though: `HOUSE-00700` put the **ground** in the hierarchy.
`visibility::BuildExteriorScene` turns every chunk of an exterior cell into an `ExteriorInstance`
once at load — 49 of them today, the terrain tiles, the road, the fences and the garden structures
`HOUSE-00780` had filed as per-cell chunks — and its category is read off the material's own
prefix, a chunk being one material (§17.4). A chunk's cell stays its **residency** key (§27.2);
what changed is which structure decides whether it is drawn. The two answers are unioned rather
than swapped, minus what the walk already found, because a chunk drawn twice is a chunk drawn
twice.

Nothing outdoors is drawn from a room with no view out: the cones §25.6 is tested against are the
cones of the exterior cells the walk **did** reach, so a basement with the yard behind it
contributes none and the outdoors costs nothing there.

## 6. How to debug it

### The overlays

| Key | What it shows |
|---|---|
| `F3` | The counters: cells visible of cells in the world, max depth, cones kept and dropped; every reason a portal was **not** crossed; chunks, instances and exterior nodes drawn of tested; draw calls and state changes against §71.2's budgets; then the visible cells with their depth, cone count and diffuse flag. |
| `F4` | The geometry those counts are about: the visible cells as coloured wireframe boxes, the portals crossed as filled quads, the reduced cones as wire pyramids — coloured by depth. A visibility bug is invisible by construction, because its symptom is geometry that is *not* there; this draws the decision instead. Read with `F3`, not instead of it. |
| `F5` | Freeze. The walk stops updating and the camera flies free, so the answer can be looked *at* from outside. `F3` says `FROZEN` and names the frame, in capitals, because every number above it then describes a frame that is not on screen. |

### The switches

* `cull off` in the console — the draw list becomes everything resident while §25's walk still runs
  and `F3` still reports it. `cull` with no argument reports; it never toggles, because the one
  thing anybody types it for is comparing two frames.
* `--no-cull` on the command line — the same switch, thrown before the first frame, for a render
  test that cannot type into a console.

### Symptom → what to look at

| Symptom | First thing to check |
|---|---|
| A room that should be visible is missing | `F3`'s portal line: which reason counted it — `closed`, `facing`, `deep`, `clipped`, `small`, `covered`. Each is a different bug. |
| …and none of them did | `CELLS DROPPED` or `QUEUE DROPPED` on the first line. Both mean something visible was culled by a cap. |
| Geometry missing inside a room that IS visible | Step 3, not step 2: `chunks n / m` on the `F3` geometry line, and `cull off` to confirm the chunk is resident at all. |
| A room appears through a shut door | Its aperture. §25.3's latch keeps a door shut only below 0.05 — check what wrote the fraction. |
| The garden flickers as the camera turns | The cone cap: `ExteriorCones::Degraded()`, when a ninth cone makes the collection fall back to the camera frustum. |
| Too much drawn from outdoors | The depth caps (`PortalDepth.hpp`): from outside a window is worth **one** room. |
| A frame hitches | `F3`'s queue peak. A frame that filled the ring is a frame that also dropped cones. |

### The tests, and what each one is for

* Set equality at 24 named poses, both door states — `VisibleSetTests`, `VisibilityPoseTests`.
* The budget at the 12 budget poses — `VisibilityBudgetTests`.
* One property each — `PortalDepthTests`, `PortalFacingTests`, `PortalAreaTests`, `ClipRectTests`,
  `ReduceFrustumTests`, `ClipFrustumTests`, `ConeQueueTests`.
* The house as a building — `ClosedHouseTests`, `DoorMatrixTests`, `AjarDoorTests`,
  `WindowDepthTests`, `OutdoorDepthTests`, `StairWellTests`, `StairTraversalTests`.
* The exterior against a brute-force scan — `ExteriorBvhTests`, `ExteriorCullingTests`,
  `ExteriorTraversalTests`.
* No allocation, and the ring — `VisibilityAllocationTests`.
* The cost — `tests/perf/VisibilityCostTests.cpp`. Never gating; run it on a quiet machine.
* The same pose drawn culled and unculled must be the same pixels —
  `tests/render/CullingSanityRenderTests.cpp` (currently disabled; see `HOUSE-00688`).

## 7. Where the code is

```
include/cnahouse/visibility/ + src/visibility/
  VisibilitySystem      step 2's driver: one walk a frame, apertures, the published set
  PortalTraversal       the walk itself, breadth-first over ConeQueue
  ConeQueue             the fixed-capacity ring
  ClipFrustum           up to ten planes, XNA outward normals
  ClipRect              Sutherland-Hodgman in world space
  ReduceFrustum         a cone from the eye and a clipped polygon
  PortalArea            the screen-area cutoff and the NDC rectangles
  PortalFacing          the plane test, against the box that touches the portal
  PortalDepth           §25.2's depth table
  PortalRuntime         a portal's world rectangle and its aperture latch
  ChunkCulling          step 3 for §17.4's chunks
  InstanceCulling       step 3 for dynamic instances
  ExteriorBvh           §25.6's loose 3-level hierarchy
  ExteriorCulling       its walk, and the cones that reach it
  CullDistance          §25.6's eight distances and §68's scale
  DetailSets            §26.4's dressing and micro-detail distances
  RenderList            step 5
src/debug/
  VisibilityOverlay          F3
  VisibilityGeometryOverlay  F4
  VisibilityCommands         the `cull` console command
```
