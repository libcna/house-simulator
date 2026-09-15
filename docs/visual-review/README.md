# Visual review ledger

These captures answer “does the playable house look good?”, not “did a pixel change?”. Run
`python3 tools/visual/capture_review.py <short-head>-<round>` from the repository root. The tool
fixes six route poses plus two composition poses (added for HOUSE-01037 after the original
living/family cameras proved to face away from their seating groups), Tier S/High, software Mesa,
clear weather, 10:30, the session seed and capture frame. Strict golden render tests remain
separate. The original six poses stay unchanged for the route before/after comparison; the two
extra poses are stable review cameras from this round onward.

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

## Round 4 — complete exterior-skin visibility

Commit: `HOUSE-00921` working tree (`2026-09-14`)

Capture: [`captures/house-00921-facade-r1`](captures/house-00921-facade-r1)

Ranked defects:

1. The complete façade is now visible but nearly black. Its siding and brick receivers use the
   adjacent room's interior bake even when viewed from outdoors, so real exterior daylight does
   not yet reveal the material.
2. Drive, lawn and other foreground finishes still have too little separation from one another;
   the black building makes this flat exterior balance especially obvious.
3. Every selected interior remains empty, with no furniture, kitchen fixtures or domestic scale.
4. Foyer and living room remain markedly darker than the other selected rooms.
5. The peeling `paint_warm_fine` door/boundary finish remains inappropriate for maintained rooms.

Fixed in this round: 57 room-owned siding and brick water-table chunks now join §25.6's 47
exterior-cell instances while retaining their canonical room residency keys. The front camera
therefore sees a solid house with doors and windows shut instead of the sky through an outline;
ordinary interior walls remain portal-culled. The exterior BVH now contains 104 instances. All 18
culled/unculled pairs remain below the unchanged 0.2% threshold. Five wide exterior regression
references were inspected and intentionally updated because they now contain the previously
missing building; interior and property references did not change.

Remaining: give the outer skin a correct outdoor daylight path, then reassess exterior material
scale/contrast before moving to primary furnishings. `VISUAL-GATE-1` has not passed.

## Round 5 — live outdoor sky on the house skin

Commit: `HOUSE-00922` working tree (`2026-09-14`)

Capture: [`captures/house-00922-outdoor-sky-r1`](captures/house-00922-outdoor-sky-r1)

Ranked defects:

1. The façade is now materially solid and readable, but broad siding areas remain too uniformly
   dark and flat. They need directional daylight/form separation and a more convincing siding
   scale before the exterior reads as a finished home.
2. Drive, lawn and surrounding ground still merge into a low-contrast foreground, while the
   property has almost no vegetation or domestic exterior detail in this view.
3. Every selected interior is empty: furniture and kitchen fixtures are now the largest repeated
   defect across five of the six canonical views.
4. Foyer and living room remain markedly darker than the other selected interiors.
5. The peeling `paint_warm_fine` door/boundary finish remains inappropriate for maintained rooms.

Fixed in this round: outer-skin chunks retain their per-cell uniform-sky bake for eave/reveal
occlusion, but now tint it with the live unattenuated outdoor sky in one opaque Tier-S draw. They
no longer receive the adjacent room's lamp atlases or window-transmission attenuation. The fixed
front capture changed 19.02% of pixels and its façade crop mean rose from RGB
(35.3, 39.8, 45.9) to (73.1, 77.5, 83.3); all five interior captures are pixel-identical to Round
4. The five affected exterior regression references were inspected side by side and deliberately
updated; culling equivalence remains green.

Remaining: add believable exterior form/daylight separation and correct the large-scale siding
read, then start primary furniture placement because emptiness dominates the connected-room
views. `VISUAL-GATE-1` has not passed.

## Round 6 — first real L0 seating groups

Commit: `HOUSE-01037` working tree (`2026-09-15`)

Before: [Round 5 six-view route](captures/house-00922-outdoor-sky-r1).
After: [eight-view furnished route](captures/house-01037-furniture-r6).
Intermediate defect proof: [Round 6 pre-FFL composition](captures/house-01037-furniture-r5).

Ranked visible defects after the fix:

1. White seating/table surfaces clip against very dark wall/floor finishes; the room lighting and
   material balance still feel like a staged scene, not daylight in a lived-in house.
2. The original route cameras face away from most of the seating, so the two new fixed composition
   views are essential to judging these rooms. Kitchen and foyer remain empty.
3. The living room remains too dark away from its white furnishings; source leather/palette
   contrast and room ambient need a purposeful rebalance.
4. The façade is a broad flat gray silhouette above nearly empty blue-gray foreground; the
   driveway/ground/vegetation do not read at this front-review pose. Exterior start quality is
   unchanged.
5. The peeling warm paint and blown-out window apertures still distract from real finishes.

Fixed: nine metre-scaled, CC BY-attributed source models and eight intact source albedo slots now
form two measured sofa/chair/table/rug/lamp/plant groups plus a family media wall. The first real
captures found NPOT sampler failure, an unsplit 65,535-primitive Reach draw and, most visibly,
`y=0` furniture sunk 0.60 m below the L0 finished floor; all three were corrected and recaptured.
The basement cinema artefact vanished when props moved to `L0.ffl=0.60`. The two groups now read
as furniture in normal gameplay rather than debug blocks. Three strict references changed only
where the new furnishings enter the debug kitchen/living and production kitchen views; their
before/after images were inspected before the intentional update. The 18-pose culled/unculled
comparison remained below 0.2%.

Remaining: daylight/fixture depth and a less clipped furnishing finish, then primary kitchen and
foyer fixtures. This is a **primary-kit checkpoint**, not fully furnished rooms; `VISUAL-GATE-1`
has not passed.

Diagnosis for the next round: the unbaked BasicEffect props still inherit its 1.0 white downward
key while the shell uses cell lightmaps. The `L0_LIVING` main artificial atlas has peak 10.55 but
mean 0.011 irradiance; binding its dark spatial pattern as the only opaque ambient-floor carrier
leaves most walls nearly black with that group off. Correct these two mismatched light paths before
judging the sofa's final colour or adding more small clutter.

## Round 7 — collision-safe final furniture placement

Commit: `HOUSE-01037` working tree (`2026-09-15`)

Before: [initial furnished composition](captures/house-01037-furniture-r6).
After: [final eight-view capture](captures/house-01037-furniture-r7).

Ranked visible defects after the correction:

1. White/cyan seating remains overexposed against dark plaster, wood and ceiling; light-path
   mismatch still dominates the two furnished views.
2. Large window apertures are almost pure white; both interior depth and exterior view are lost.
3. The family composition pose now crops the shifted chair/table in the foreground, though the
   kitchen-to-family route pose shows the seating group and clear opening.
4. Foyer, hall and kitchen lack visible primary furniture and their shells remain too dark.
5. The exterior-front capture is bit-identical to the prior empty route: flat gray façade and
   unconvincing ground/context remain.

Fixed: moved the family sofa, coffee table, rug and armchair and the living purple armchair after
actual depenetration/tour tests found a cell-midpoint overlap and two wall-recovery wedges. The
collision limits were preserved. Both collision tests and the 18-pose culled/unculled check pass
with the final placement. The source and build world content now include the matching rebuilt pet
navigation graph. Correction recorded in Round 8: the ordinary hardware CTest run at this
checkpoint was capture-only for reference tests, so its green result was not proof of a strict
pixel comparison after these final placement shifts.

Remaining: correct visible room/furniture lighting before clutter or distant-house polish.
`VISUAL-GATE-1` has not passed.

## Round 8 — ambient receiver floor and live-cell furniture key

Commit: `HOUSE-01280` checkpoint (`2026-09-15`)

Before: [Round 7 eight fixed gameplay views](captures/house-01037-furniture-r7).
Intermediate: [receiver-floor correction only](captures/house-01280-ambient-r1).
After: [receiver floor plus Basic detail correction](captures/house-01280-detail-r2).

Ranked visible defects after the correction:

1. The normal-game exterior start is missing major ground, road, fence, gate and roof detail that
   the identical player pose shows in explicit debug blockout. This is absent geometry, not just
   a flat gray material or weak planting. Exterior generated chunks still carry unregistered role
   names such as `TERRAIN_asphalt`, `FENCE_board` and `ROAD_paint` instead of canonical material
   ids; their unbaked outdoor `Basic` layout also needs matching stock-XNA material rows.
2. Large window apertures are still nearly pure white. Views through the glass disappear and the
   extreme window/room contrast continues to dominate the furnished compositions.
3. Foyer and central hall remain both dark and empty at the fixed clear 10:30 pose. The foyer's
   measured daylight level is only 0.027, and the current fixtures do not make the route readable.
4. The kitchen has no worktop, cabinets, appliances or domestic layout; from the hall it still
   reads as an empty shell rather than the end of a real connected route.
5. The furnished rooms are now less clipped, but sofa/source style, subdued floor/wall balance,
   peeling boundary paint and overly bright plant leaves still prevent a believable finish.

Fixed: Tier-S receivers write a neutral 0.025 ambient floor instead of multiplying that floor by
the primary fixture's mostly black atlas. Each switched-on artificial group keeps its authored
UV2 bake in an additive depth-equal pass, with `LM_DAY` last; the all-off case still shows wall and
floor silhouettes. Stock `BasicEffect` no longer inherits its constructor's white downward light:
static detail receives an explicit conservative key and sky/fixture ambient derived from the live
cell. The R1 image showed brighter walls but still-white sofas; R2 removes much of the white/cyan
clipping without replacing source textures or resorting to diagnostic colouring. This is not the
later three-light dynamic-object assignment (`HOUSE-01261`). The source-light correction adds one
receiver pass whenever the formerly opaque primary group is on: up to six at four active groups
plus daylight, rather than five. The all-off/daylit integration case uses two passes.

Strict-reference correction: the earlier hardware render test configuration was capture-only, so
the Round 7 statement that final-placement pixels were green was too strong. The first true
software comparison found only three stale pose references from the final collision-safe shifts
(production `fp-l0-kitchen`, diagnostic `blockout-l0-kitchen` and `blockout-l0-living`). Their
before/after silhouettes were inspected and only these three were corrected. The new ambient/key
rendering then deliberately changes every production first-person pose and four season/sun outer
skin details; all 12 interior and four seasonal pairs were inspected before selective reference
updates. Diagnostic blockout, property poses and the 18 culled/unculled pairs remain unchanged.

Remaining: register and render the missing authored outdoor surfaces first, then window/background
brightness, primary foyer/hall/kitchen furnishing and exterior form/context. `VISUAL-GATE-1` has
not passed.
