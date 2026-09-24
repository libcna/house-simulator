# Visual review ledger

> **Note (2026-09-21):** `captures/` is git-ignored and pruned. Older iteration captures (all `house-00*`, superseded `rN`, probes, candidates) were deleted; links marked *(removed)* point to captures that no longer exist. This ledger text is the record; regenerate captures with the tools if needed.

> **2026-09-21: protocol change (`HOUSE-03201`, [ADR-0014](../decisions/ADR-0014-showcase-scope.md)).**
> The Visual Convergence Sprint and `VISUAL-GATE-1` are retired. Rounds 0–102 below all reviewed
> the ground-floor route and the front approach. No round has ever captured the basement, the
> ground-floor service rooms, the garage interior, the upper-floor rooms or the attic. From Round
> 103 on, a round follows [`plan.md`](../../plan.md): it captures per-zone fixed view sets
> (`HOUSE-03202`); it classifies every defect by **severity S1–S4** and by **zone**; it updates the
> zone scoreboard; and it names the next task together with the **scheduling rule (R1–R11)** that
> chose it. Round 102's open ground-floor list is **frozen by rule R4 until gate G4**; after G3
> it is taken up by the house-wide `HOUSE-03402` and by `HOUSE-03407`. Log S3/S4 findings under a
> zone's heading as backlog; do not schedule them.

These captures answer “does the playable house look good?”, not “did a pixel change?”. Run
`python3 tools/visual/capture_review.py <short-head>-<round>` from the repository root. The tool
still writes `docs/visual-review/captures/`, but since `HOUSE-00043` those large human-review
images are local, ignored evidence rather than Git-tracked assets; links below work in a
workspace that retains the captures, not a fresh clone. This text ledger and the separate
`tests/render/reference/` strict goldens remain versioned. Keep the local captures on disk for
before/after inspection; the ignore rule is not permission to delete review evidence. The tool
fixes six route poses plus two composition poses (added for HOUSE-01037 after the original
living/family cameras proved to face away from their seating groups), Tier S/High, software Mesa,
clear weather, 10:30, the session seed and capture frame. Strict golden render tests remain
separate. The original six poses stayed unchanged through Round 16 for route comparison; the two
extra poses are stable review cameras from that round onward. `HOUSE-01038` adds a ninth, reverse
foyer pose and fixed noon, overcast and night review scenarios without moving any prior camera.
`HOUSE-01039` adds a tenth pose facing back across the kitchen; its original east-facing pose
mostly showed the neighboring family room and hid the kitchen's empty west side.
`HOUSE-01040` places a real island across the old east/west kitchen-camera position, so both
kitchen poses move to a measured 1.04 m east-side aisle and an eleventh fixed pose is added just
beyond the hall opening, angled toward the island and sink run. The other eight poses are unchanged.
`HOUSE-01041` retains all eleven poses and four conditions for an exact lighting comparison.
`HOUSE-00935` adds a twelfth pose on the driveway, looking squarely at the garage frontage; the
prior cameras stay unchanged and remain the exact comparison set for the main route.
`HOUSE-01050` adds the fourteenth, front-on family-media pose after the intervening exterior-approach
review camera; `HOUSE-01051` retains all fourteen unchanged for an exact furnishing comparison.
`HOUSE-00937` adds a fifteenth path-height front-door pose: the road camera retains the whole
arrival composition, while this closer frame exposes foundation planting scale and porch/driveway
overlap that the picket fence hides.
`HOUSE-00940` adds a sixteenth foyer-side view of the formal paired doors without moving any prior
camera. `HOUSE-01287` adds the seventeenth view on the front balcony, facing its over-door lantern;
the road and path cameras retain the wider arrival comparison. `HOUSE-01288` adds an eighteenth
downward approach view between the gate and steps, where the path fixtures and their circulation
clearance remain legible without moving any earlier camera. Later room and garden views expanded
the set to 28. `HOUSE-01297` adds the twenty-ninth view just inside the kitchen's open butler-pantry
passage; all earlier poses stay fixed for day/night comparison.

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

Capture: `captures/house-00912-production-r2` *(removed)*

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

Capture: `captures/house-00921-facade-r1` *(removed)*

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

Capture: `captures/house-00922-outdoor-sky-r1` *(removed)*

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

Before: Round 5 six-view route *(capture removed)*.
After: eight-view furnished route *(capture removed)*.
Intermediate defect proof: Round 6 pre-FFL composition *(capture removed)*.

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

Before: initial furnished composition *(capture removed)*.
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

## Round 9 — outdoor production material parity

Commit: `HOUSE-00923` checkpoint (`2026-09-15`)

Before: [Round 8 eight fixed gameplay views](captures/house-01280-detail-r2).
After: same eight gameplay views with the outdoor surfaces restored *(capture removed)*.

Ranked visible defects after the correction:

1. The front now has a real asphalt foreground, lawn, white fence/gate, walk and roof, but its broad
   façade remains an almost-flat dark slab; window openings show sky instead of convincing glass,
   sash and interior depth. This is still recognizably an engineering shell.
2. Living/family window apertures are nearly pure white, erasing the exterior view and drawing the
   eye before the new furniture. Their brightness/background needs an inside/outside diagnosis.
3. Foyer and central hall are mostly dark and empty; the connected entrance route has no human-scale
   furniture or attractive fixture pools.
4. Kitchen is an empty shell with no cabinetry, worktop, sink, appliance or table arrangement.
5. The two seating groups read as furniture, but walls/floors remain subdued, the palette/style is
   inconsistent, and plant foliage is too bright.

Fixed: the deployed 476-chunk baseline had 31 unknown generated outdoor roles and 17 mismatched
`Basic`/`DualTexture` bindings. A fresh shell audit exposed 20 unbaked base-finish slots, including
stairs and attic roof detail inside otherwise lightmapped files. Outdoor generators now emit stable
canonical `materialId`s; six unbaked shell finishes and generated outdoor roles use 17 deterministic
source-preserving stock-XNA `Basic` variants of already approved textures. The chunk builder refuses
unknown ids and effect/vertex disagreement. Four source-role groups merge, yielding 472 chunks and
**zero unknown or mismatched bindings**; the new integration gate checks every one. Outdoor Basic
surfaces use the existing live full sky/sun term rather than the conservative indoor bounce. No
normal-game path uses the hashed debug palette; diagnostic blockout is explicit.

The intentional material-id changes alter eight exterior and two attic diagnostic references, six
property views, two production garage/attic detail views and four seasonal exterior views. All 22
changed before/after pairs were inspected individually; only those refs were updated. The unchanged
route/interior poses and 18-pair culled/unculled check provide independent stability evidence.

Remaining: window/background and façade brightness, then entrance/hall/kitchen primary furnishing
and meaningful day/night fixture lighting. `VISUAL-GATE-1` has **not passed**.

## Round 10 — glass tint no longer emits adapted white

Commit: `HOUSE-00924` checkpoint (`2026-09-15`).

Before: Round 9 fixed gameplay views *(capture removed)*.
After: same eight views with glass composition corrected *(capture removed)*.

Ranked visible defects after the correction:

1. The tall white shape in the living composition is an **opaque chimney, not a pane**. The
   blockout diagnostic shows a solid cyan column, and `chunks.bin` names `CHIMNEY:exterior`,
   `MAT_OUTDOOR_BRICK`, `EXT_ROAD`. It and pale fence/lawn receivers clip when outdoor lighting
   inherits a dark room's eye exposure. Diagnose that cell/exposure boundary next.
2. The exterior façade is broad, flat, dark and visually open at the windows; it does not yet read
   like a finished house despite real road, grass, fence, gate and roof materials.
3. Foyer and central hall remain dark and mostly empty; living/family seating groups are present
   but the route still lacks depth and warm, human-scale lighting.
4. Kitchen has no cabinets, worktop, sink, appliances or dining composition.
5. The distant aqua room finish and fluorescent-looking foliage compete with the otherwise
   neutral route; both require material/asset inspection rather than more debug recolouring.

Fixed: `TransparentPass` no longer multiplies unlit glass tint by the room's up-to-6x adaptive
exposure against a background sky that has not received the same lift. Stock XNA `BasicEffect`
continues to supply the authored 0.12 alpha and premultiplied `AlphaBlend` with read-only depth;
other translucent classes retain their adapted path. At a fixed living composition pixel, the
left window changes from `(255,255,255)` to `(175,200,228)` without moving the adjacent wall;
the family windows now visibly contain blue sky and fence contours rather than solid white.
Exterior/front is unaffected. The three intentionally changed first-person strict refs were
inspected old/new and updated individually; the debug, property, sun/season and culling views
remain stable. The actual-software 15-registration render/content subset and isolated full
1,585-registration CTest are green; the initial concurrent strict-XNA/full-CTest attempt produced
only a world-load performance flake, which passed alone at 237 ms and in the isolated rerun.

Remaining: solve outdoor-receiver exposure and façade/window finish, then furnish and light the
entrance/hall/kitchen route. `VISUAL-GATE-1` has **not passed**.

## Round 11 — outdoor receivers no longer clip from a dark room

Commit: `HOUSE-00925` checkpoint (`2026-09-15`).

Before: Round 10 fixed gameplay views *(capture removed)*.
After: same eight views with receiver-domain exposure corrected *(capture removed)*.

Ranked visible defects after the correction:

1. The exterior start still has a broad, flat dark façade with conspicuously open-looking windows,
   thin roof silhouette and a front entry lacking finished depth. This is the worst full-frame
   normal-game view and does not read like a realistic home.
2. The chimney is now warm beige rather than white, but remains a 0.6 m by 14.3 m bare vertical
   box intruding visually into the living-room seating composition. Its approved brick source
   bitmap contains clear mortar; a 150x250 screen crop is exactly one colour, so source texture
   detail is not reaching the player at this pose. Diagnose UV/sampler/mip behaviour separately.
3. Foyer, central hall and kitchen remain empty or near-empty and underlit, making the connected
   route feel like an engineering shell despite believable authored wall/floor material ids.
4. Family/living primary seating groups are present but lighting depth is weak, the upholstery
   looks pale and the small plants are fluorescent green.

Fixed: the deployed `CHIMNEY:exterior` chunk owns approved `MAT_OUTDOOR_BRICK` in `EXT_ROAD`; it
was not a window. The common static pass formerly multiplied exterior-cell Basic sky/sun and
room-owned outer-skin DualTexture daylight by an adapted indoor camera up to 6x, but the sky and
glass in that same frame remained scene-referred. Exterior opaque receivers now use the sky's 1x
Tier-S effect domain while indoor receivers still adapt. A fixed living pixel moves from white
`(255,255,255)` to material-tinted `(178,164,150)`; sampled lawn moves from yellow-white
`(242,249,202)` to green `(110,156,92)`. Family windows reveal fence and grass instead of pale
bands. Exterior-front, entrance, hall and original living views are pixel-identical; the other
four views' changed regions were inspected. Three intentionally changed strict first-person refs
were inspected individually and updated; software culling pairs remain equivalent.
The 20-registration actual-software regression subset, 323-unit strict-XNA check and isolated
full 1,586-registration CTest (1,578 runnable cases) pass.

Remaining: finish the front façade and route furnishing/lighting, then audit why the brick bitmap
collapses to one screen colour. `VISUAL-GATE-1` has **not passed**.

## Round 12 — exterior windows join the playable façade

Commit: `HOUSE-00926` checkpoint (`2026-09-15`; exact HEAD in `docs/handoff.md`).

Before: Round 11 fixed gameplay views *(capture removed)*.
After: same eight views with exterior-window roles *(capture removed)*.

Ranked visible defects after inspecting all eight new images:

1. The front windows now have painted frames, sashes, meeting rails and tinted glazing, but the
   broad façade is still a nearly uniform dark slab. Its siding, wall form and daylight are the
   largest remaining exterior defect; the roof and front-door composition are thin and flat.
2. Foyer, central hall and kitchen are empty and severely underlit; crossing the threshold still
   feels like entering an unfinished shell. Their three fixed views did not materially improve.
3. The living-room chimney still presents as one untextured beige box beside dark seating, and
   living/family plant leaves remain fluorescent green against flat, pale upholstery. Exterior
   window rails also read too silver-bright in the fixed winter night sun/season views, because
   the unlit façade gives them no tonal context; the next exterior light/material pass must assess
   both surfaces together.

Fixed: canonical weather-facing window frames and panes now have their own licensed, approved
material identities and Basic shell chunks instead of sharing each room's indoor trim and glass.
The exterior BVH can show only those outside roles with the façade when a neighboring room is
portal-closed; its deployed count grows from 104 to 170 instances. The front and four interior
window-facing views visibly change; entrance, hall and the original living view are byte-identical.
The 64-window schedule, borrowed-light interior glass, skirting, collision and room-owned
residency remain intact. Both daylight and switch-group lightmaps for affected cells were
deterministically rebaked and their shell hashes/bindings agree. All 31 deliberately changed
golden pairs were inspected old/new before selective reference replacement: 17 explicit debug
blockout, three production first-person, seven property blockout and four production sun/season
views. The final content-build graph is 16/16 fresh; its second run rebuilt the navigation graph
after collision's manifest fingerprint changed. The direct actual-software 12-test render suite
passes, including all 18 culled/unculled view pairs. The first CTest attempt could not run image
tests because a concurrent sibling CNA source-glob change triggered a CMake reconfiguration in a
filesystem-restricted test sandbox; the same `world-content-current` build gate passed with
proper build access, and direct software-render comparison then proved the images. The complete
offscreen 1,587-registration CTest passes all 1,575 runnable tests after the exact canonical
material-count fixtures were extended. An initial full desktop-video parallel run moved the
headless camera through ambient mouse input; that test passed alone in software and hardware
offscreen runs, then in the full offscreen rerun. `tools/ci/run_checks.sh` is green with 323
strict-XNA translation units.

Remaining: make the approved façade material respond to useful exterior daylight/form, then
furnish and light the empty connected L0 route. `VISUAL-GATE-1` has **not passed**.

## Round 13 — repeat-UV materials become visible in the playable house

Commit: `HOUSE-00927` checkpoint (`2026-09-15`; exact HEAD in `docs/handoff.md`).

Before: Round 12's eight fixed gameplay views *(capture removed)*.
After: the same eight views *(capture removed)*, plus an identical
close approach before *(capture removed)* /
after *(capture removed)* pair.

Ranked visible defects after inspecting all eight new views:

1. The front now has warm, physically tiled wood siding instead of a dark grey slab, but its
   solid front-balcony parapet is a heavy black strip, the porch/front door are skeletal and the
   distressed paint pattern on the columns is noisy. The gate-to-door approach still lacks
   believable detail/form.
2. Foyer, central hall and kitchen remain nearly empty and markedly underlit. Walking inside
   would immediately trade the improved façade for rooms with no primary furniture.
3. The living chimney now actually reads as brick, but its undecorated vertical stack dominates
   a dim room; seating is pale/flat and plant leaves are fluorescent. The family-room furniture
   has the same flat lighting and conspicuous yellow-green plant.
4. The roof, grass and neighboring context still have simplified shapes; nighttime window rails
   remain disproportionately bright against the dark outside walls.

Fixed: the deployed `L0_FOYER` façade retains 48 distinct UV0s, but CNA's default HUD
`SpriteBatch::Begin()` left `LinearClamp` in sampler 0 after `End()`. At frame 3 this clamped the
physically repeated source to one texel across the siding and the living brick. The HUD's
existing strict-XNA Begin overload now explicitly selects `LinearWrap` while retaining XNA's
premultiplied blend; an attempted direct indexed assignment was rejected by the strict gate as
the known BL-16 `CNAEXT` overload and removed. Outdoor `LM_DAY` receivers now use the existing
sun/cloud diffuse energy and a less saturated, calibrated hemisphere tint instead of multiplying
the highly blue sky-display LUT into the material. The same close façade crop changes from
**1 to 111 colours**, a wall sample from `(70,69,66)` to `(139,105,71)`, and the identical
living-chimney crop from **1 to 12,001 colours**. The approved wood and brick bitmaps, canonical
geometry/UVs, UV2/lightmap ownership, materials and licences were not replaced. Close-front
clear, overcast, 17:30 and 22:00 software-Mesa captures were inspected: overcast is dimmer,
late-day siding stays readable and night walls darken without daytime clipping. The new
three-frame playable-facade render test asserts source variation and a useful daytime wall
level so HUD sampler leakage cannot silently turn the texture back into a slab.
The inspected strict daylight façade pairs in sun/season 01 and 02 were selectively replaced;
sun/season 03 and 04, the twelve first-person geometry references, eight property references
and twenty explicit blockout references were not. The dedicated 07:00 HUD reference was also
inspected old/new and refreshed: it had retained a pre-`HOUSE-00923` empty-property silhouette,
whereas the current normal game includes the previously approved fence/ground as well as this
material/daylight change. Title and font images remain intact. The affected actual-software
render suite is green across 20 tests, including all 18 culled/unculled comparisons; the CI
script passes 323 strict-XNA translation units and licence/content gates.
The relinked full offscreen CTest has 1,591 registrations, all 1,579 actual tests passing,
12 disabled/skipped and zero failures.

Remaining: form/material improvement at the porch/roof and actual furnishing plus lighting of
the empty connected L0 route. `VISUAL-GATE-1` has **not passed**.

## Round 14 — painted front entry and separated balcony metal

Commit: `HOUSE-00928` checkpoint (`2026-09-15`; exact HEAD in `docs/handoff.md`).

Before: Round 13's eight fixed gameplay views *(capture removed)* and its
exact close front *(capture removed)*.
After: the same eight views *(capture removed)* and the
same close front *(capture removed)*. The close approach
was also inspected overcast *(capture removed)* and
at 22:00 *(capture removed)*.

Ranked visible defects after inspecting all eight new gameplay views:

1. The front balcony is no longer a black strip and the porch supports are clean paint, but the
   roof/dormers, door and porch joinery still look skeletal; grass and neighbouring context are
   flat. The gate-to-door view does not yet read as a finished Colonial Revival entry.
2. The foyer, hall and kitchen remain dark and almost devoid of furniture. This is now the largest
   defect on the continuous playable route, even though the door/trim finish is less noisy.
3. Night exterior is nearly black and has no path or porch light; window rails remain relatively
   bright. Overcast is credibly dimmer than clear day but needs more local readability.
4. The living/family seating is present, but remains flat grey in weak light; bright lime plant
   leaves and the undecorated brick chimney still dominate their compositions.

Fixed: `MAT_METAL_BALCONY`'s approved brushed-metal albedo/normal had been multiplied by the
canonical zero tint, yielding a near-solid black full-width band in normal daylight. A nonzero
metal tint restores the source, but applying it to the whole solid guard made an industrial-looking
silver slab. The shell generator now assigns the existing painted-trim role to the **solid**
balcony parapet while the narrow railing retains the distinct metal role. The approved smooth
white paint replaces the chipped/brick-exposing bitmap previously assigned to
`MAT_DOOR_PAINTED`, so the porch columns/beams and indoor painted openings have a coherent
maintained finish. No collision, cell/portal, stable ID, UV layout, shell geometry, XNA runtime
or licence source changed. The same close-front balcony crop changes from **one black colour to
39 painted-shade colours**; the support crop changes from **1,138 distressed colours to 38
clean-paint shades**. Three affected balcony shell GLBs and `docs/shell-manifest.json` were
deterministically regenerated; canonical chunk roles now separate the painted guard from its
metal rail. The approved metal and white-paint bitmaps compile to CNA content products, and the
source/provenance/material gates remain in force. The new frame-3 normal-game render property
asserts a non-black entry band and readable, non-chipped porch support. The clear/overcast/night
review does not claim finished lighting: at 22:00 the façade properly darkens, but lacks visible
fixtures.
The direct software-Mesa render suite passes **45 tests**, including the fixed first-person,
property and explicit debug views plus all 18 culled/unculled comparisons. Only **22 inspected
strict reference images** were selectively refreshed: seven affected exterior debug poses, the
older base debug frame, four painted-trim first-person poses, five balcony-visible property poses,
four sun/season façade views and one HUD dawn façade view. The base debug frame's prior reference
dated from `HOUSE-00909`, before the other debug property views were brought to the current
material palette; its whole-frame palette change is not a claim that normal gameplay became
debug-coloured. The remaining references, including title/font and unaffected interior/sideyard
views, were untouched. The full relinked offscreen CTest has **1,592 registrations, 1,580 actual
passes, 12 disabled/skipped and zero failures**; strict-XNA and content/provenance gates are
recorded in the handoff.

Remaining: furnish and light foyer/hall/kitchen, finish the front door/roof/porch composition and
night approach, then calibrate living/family materials and light. `VISUAL-GATE-1` has **not
passed**.

## Round 15 — first foyer furniture and current room lightmaps

Commit: `HOUSE-01038` checkpoint (`2026-09-15`; exact HEAD in `docs/handoff.md`).

Before: Round 14's eight fixed normal-game views *(capture removed)*.
After: [nine fixed clear-day views](captures/house-01038-clear-day-r5), including the new
[reverse entrance](captures/house-01038-clear-day-r5/foyer-facing-front.png); the same nine
views were inspected at [noon](captures/house-01038-noon-r5),
[overcast 10:30](captures/house-01038-overcast-r5) and
[22:00](captures/house-01038-night-r5).

Ranked visible defects after the corrected content-root captures:

1. The foyer and nearest hall walls/floor now read under warm practicals, but the next unlit
   kitchen/stair connection is a black threshold at all four review conditions. The continuous
   route is not yet convincing.
2. The real upholstered chair and carved console have useful silhouettes and source texture, but
   are still darker than the surrounding plaster, and the foyer grouping needs more furniture/
   controlled decor. The front-door panel remains a broad, nearly featureless field.
3. Daytime front siding is coherent, but porch/roof form and grass/context are sparse; at night
   the front façade/path are nearly invisible without exterior practicals.
4. Living/family seating still looks pale and flat, with fluorescent plants and a bare brick
   chimney; the original living-route camera faces an empty wall, so its composition camera must
   also be judged.

Fixed: two pinned Poly Haven CC0 2K source-mapped models were imported and placed as canonical
foyer props with measured ≥1.2 m circulation, no front-door swing/portal conflict and authored
proxy collision. The chair's pinned off-axis source geometry was recentered by 3.4 cm before
proxy generation; `origin_check` and metre-scale gates now pass without an exception. The
foyer/hall practical groups and their selected deterministic artificial
atlas bakes now contribute at daytime, overcast and night, without making neighboring unlit rooms
uniformly bright. An important capture/runtime fault was found while investigating the black
hall: `TitleContainer` used fresh executable-relative `build/content/world`, but the default
relative `ContentManager` roots followed the review launcher's repository working directory and
loaded stale root-level texture/model products. The normal shell now resolves both roots from
XNA `TitleLocation`; rejected stale-root and UV-flip diagnostic captures were moved out of the
committed review set. The exact hall screenshot changes from a near-black foreground in Round 14
to visible warm painted walls, ceiling and trim in this round. Stock-XNA Basic detail gets
calibrated active-fixture bounce: the fixed chair pixel `(1210,710)` changes
`RGB(17,14,12)→(53,37,23)` while the artificial-mapped architectural pass stays separate.
Only six visually inspected strict references changed (five nearby production poses and one
debug pose containing the new console). The current nav bake of the furniture-bearing world
fell from 29 minutes to 12.5 seconds after conservative shape prefiltering and adaptive sampling
of large outdoor cells: all selected foyer/hall/living/family/kitchen/dining node IDs,
positions and 396 within-route edges match the pre-optimization graph exactly. The 29 shifted
outdoor candidate nodes and their recomputed edges are not claimed byte-identical. Direct software
render, full world/content and strict-XNA gate results are recorded in the handoff after final
verification.

Remaining: make the kitchen/stair transition and furniture styling believable, add coherent
front-door/porch/night exterior light, then finish living/family material and lighting. The
normal house no longer uses hashed blockout colours, but `VISUAL-GATE-1` still **FAILS**.

## Round 16 — kitchen main practicals, with the kitchen itself in view

Commit: `HOUSE-01039` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: [Round 15's nine fixed clear-day views](captures/house-01038-clear-day-r5), including
the near-black [hall threshold](captures/house-01038-clear-day-r5/central-hall.png).
After: [ten fixed clear-day views](captures/house-01039-entry-r4), including the newly fixed
[kitchen-facing-west](captures/house-01039-entry-r4/kitchen-facing-west.png). Matching ten-view
[noon](captures/house-01039-noon-r4), [overcast](captures/house-01039-overcast-r4) and
[22:00](captures/house-01039-night-r4) sets were captured; the changed route and exterior
frames were inspected at all four conditions.

Ranked defects remaining:

1. The kitchen is visibly empty: no cabinets, countertop, island, range, refrigerator or task
   fixtures. The new opposite camera makes this unmistakable. Real primary furnishing is now
   the largest value, not another lighting coefficient change.
2. From the hall, a neighboring dark opening/large partition still forms a black block on the
   right side of the kitchen view. Its material/ownership and connected-room light need review
   after furniture establishes the composition. Kitchen walls/ceiling also need stronger
   daylight/window depth; the current practicals are useful but subdued.
3. Night exterior/front remains almost invisible; roof, porch and front-door form are still thin.
4. Living/family seating and leaves remain pale/flat, with a bare brick chimney and little decor.

Fixed: the four authored `LG_L0_KITCHEN_MAIN` downlights now start on as a single switchable
group, use plausible 3000 K / 1000 lm each, and bake as downward 140° spot receivers from
3.02 m rather than omnidirectional points just beneath the ceiling. The first diagnostic bake
gave **four pure-white ceiling discs (peak 102.9)** while leaving the room dark; the narrow
spot removed those discs but underlit the floor (peak 0.068). The final wider beam yields
peak **0.144**, a readable porcelain floor and warm plaster at clear day, overcast and night,
without washing out adjacent windows or the already-lit hall. Only this selected cell's
artificial/daylight atlas products were promoted; normal gameplay uses the canonical data and
Tier-S paths. Three directly reviewed strict first-person references (hall, kitchen and hall
corner) changed intentionally; all 48 direct software-render tests pass with culling equivalence.
The default-on/switch-off and borrowed hall light are protected by a new lighting unit test.
The full CMake/CTest configure gate is temporarily upstream-blocked by CNA's in-progress Wayland
SDL audit (BL-17); direct verification and the exact build workaround are in the handoff.

`VISUAL-GATE-1` still **FAILS**. The route is less black, but a lit, empty kitchen is not a
realistic furnished house.

## Round 17 — first real L0 kitchen joinery, measured from the hall route

Commit: `HOUSE-01040` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: [Round 16's empty kitchen](captures/house-01039-entry-r4/kitchen-facing-west.png)
and unchanged [hall aperture](captures/house-01039-entry-r4/central-hall.png). The original
kitchen camera lay inside the new island's footprint, so this round moves its paired east/west
poses to the measured east aisle and adds a stable [entry-stride camera](captures/house-01040-clear-day-r4/kitchen-from-hall.png).
After: eleven-view [clear 10:30](captures/house-01040-clear-day-r4),
[noon](captures/house-01040-noon-r4),
[overcast](captures/house-01040-overcast-r4) and
[22:00](captures/house-01040-night-r4) sets. The route, cabinet, family and exterior frames
were actually inspected at relevant conditions; the other fixed images were retained for
comparison. The corrected [west kitchen view](captures/house-01040-clear-day-r4/kitchen-facing-west.png)
shows the sink above doors, visible marble veining and a clear lane beside the island.

Ranked defects remaining:

1. Kitchen still lacks upper cabinetry, refrigerator, range/hood, dishwasher, oven, splashback
   and controlled worktop objects. The now-recognizable island/base run is not a finished kitchen.
2. The nearby family room stays flat and partly dark: pale seating, bright leaf cards, blank art
   walls and little room-light depth. From the unchanged hall camera, a large dark kitchen-side
   block still dominates the opening until the player takes the entry stride.
3. The front facade is skeletal at clear day (thin roof/porch/balcony, little entrance depth),
   and [night front](captures/house-01040-night-r4/exterior-front.png) remains almost invisible.
4. The kitchen practicals read at night and overcast, but the cabinet paint is still low-contrast
   brown at distance; window/daylight contribution adds little room depth.

Fixed: two deterministic project-authored static built-ins through canonical `layout.props.json`
and the approved chunk/content route. The 2.8 m north run has a true 560 × 440 mm stone sink
cutout, steel basin/gooseneck, two under-sink doors, drawers and recessed oak toe. A 2.2 m island
anchors the room with five-piece fronts and a 0.94 m marble top. Both have real-metre UV repeats,
individual material-map splits, one 12-triangle collision proxy and support-centred origins.
The final measured wall-to-island working aisle is about 1.18 m; the hall-side clearance is
about 1.08 m. The imported paint/oak/stone/steel sources are already approved, with authored
Ms-PL geometry provenance; a CI check reproduces both GLBs and metadata byte-for-byte.
`L0_KITCHEN` rises only from its measured eight to eleven chunks, without a new Reach split.
Screenshot diagnostics with an island-intersecting camera and a protruding end-return were
moved recoverably to `/tmp/house01040-review-diagnostics.7mzwzB/`, not committed as quality
evidence. Three changed strict references were viewed individually: production kitchen pose
relocated for furniture, a tiny hall shading recomputation, and explicit debug kitchen pose
relocated away from the sink run. They were selectively replaced; normal gameplay still uses
production materials, never the colourful blockout reference.

`VISUAL-GATE-1` still **FAILS**. This is a credible primary kitchen built-in checkpoint, not a
completed or fully lit room.

## Round 18 — useful family practicals; fridge silhouette diagnosed

Commit: `HOUSE-01041` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: eleven-view [Round 17 clear](captures/house-01040-clear-day-r4) and
[night](captures/house-01040-night-r4). After: matching eleven-view
[clear](captures/house-01041-clear-day-r1),
[noon](captures/house-01041-noon-r1),
[overcast](captures/house-01041-overcast-r1) and
[22:00](captures/house-01041-night-r1) normal-game sets; the same family, hall, kitchen and
exterior frames were actually viewed at relevant conditions. No camera was moved. The
[family composition at clear day](captures/house-01041-clear-day-r1/family-composition.png)
and [22:00](captures/house-01041-night-r1/family-composition.png) now have useful warm light
without ceiling fireflies or clipped windows. A fixed sofa pixel changes from RGB(48,56,71)
to (77,62,52) at clear day, and from (34,34,34) to (81,60,41) at night; the room is still
visually sparse, not final furnished quality.

Ranked visible defects remaining:

1. From the unchanged [central hall](captures/house-01041-clear-day-r1/central-hall.png), a
   large almost-black appliance-shaped slab still masks the family/kitchen continuation.
   Full-chunk triangle picking identifies its first front face as the canonical, externally
   unfinished `CELL_FRIDGE_INTERIOR` door at (0.897,2.28,-26.415), not the newly authored
   island or a wall needing ambient. The 55° explicit debug camera misled the first visual
   inference against the normal game's 70° lens; the corrected diagnosis is recorded in plan.
2. The clear front still has a thin/skeletal roof, balcony and porch; the night façade is
   barely visible. It remains the player's first view.
3. Kitchen upper/tall cabinetry, refrigerator exterior, range/hood and decor are missing;
   the base run/island alone do not complete the room.
4. Family room seating and art remain pale/blank, the leaf-card plant is too bright and the
   room's new warm light does not replace needed material/decor work.

Fixed: the existing family main four-light group now starts on as 3.02 m downward 140° warm
spots using the selected Tier-S bake. The independently switched media accent moves from a
ceiling-centred point (selected calibration peak 7.4286) behind the canonical TV as a
wall-facing spot (peak 0.1161); the main group peak is 0.1977 at the same explicit 100 lm/W
household-light calibration already selected for the kitchen. Two existing east kitchen
downlights shift toward its rear painted receiver, changing that receiver's 128² atlas texel
(112,68) from RGB(56,56,55) to (121,122,121), while the black fridge front does not move or
brighten. Only L0_FAMILY/L0_KITCHEN selected atlas data, bindings and provenance are promoted;
other 76 receivers retain their products. Exactly two affected strict first-person reference
images were paired and viewed before selective replacement. Normal gameplay remains the
production-material path; blockout colours occur only in explicit debug mode.

`VISUAL-GATE-1` still **FAILS**. Next highest visible value: a believable measured exterior
refrigerator carcass/front aligned with the existing nested fridge cell, with collision and
material/provenance checks, then recapture the unchanged hall route. Do not boost global
ambient to hide the missing appliance.

## Round 19 — the refrigerator bay gets real closed frontage

Commit: `HOUSE-01042` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: unchanged eleven-view [Round 18 clear](captures/house-01041-clear-day-r1),
[noon](captures/house-01041-noon-r1), [overcast](captures/house-01041-overcast-r1)
and [22:00](captures/house-01041-night-r1). After: matching eleven-view
[clear](captures/house-01042-clear-day-r2), [noon](captures/house-01042-noon-r2),
[overcast](captures/house-01042-overcast-r2) and
[22:00](captures/house-01042-night-r2) normal-game sets; no camera or simulated
condition was moved. The affected hall/kitchen, unchanged front/night and family
frames were actually inspected. The first steel-front hall *(capture removed)*
and kitchen *(capture removed)* looked nearly as black as
the undressed portal; two representative rejected frames are kept, not the entire
transient eleven-frame set.

Ranked visible defects remaining:

1. The [clear hall→kitchen appliance](captures/house-01042-clear-day-r2/central-hall.png)
   now has identifiable doors, vertical pulls, dispenser, plinth and upper shaker
   fronts, but the white painted/steel Basic-effect pieces are still brown/dark,
   especially the upper fronts. At the same upper hall ray (900,380), the first
   hit is now project-authored `MAT_DOOR_PAINTED` frontage at z=-26.336 rather than
   the dark rear `MAT_PAINT_WARM_WHITE` wall at z=-27.025. Geometry/content alone
   cannot give the room's static furnishings sufficient practical-light depth.
2. The [front at clear day](captures/house-01042-clear-day-r2/exterior-front.png)
   still reads as a thin-roof/porch/balcony engineering shell; the
   [22:00 first view](captures/house-01042-night-r2/exterior-front.png) is almost
   invisible beyond pale window squares.
3. The kitchen still lacks west/north upper runs, range, hood, backsplash, other
   appliances and controlled decor. Its first island and base sink run are not a
   finished 62-container room.
4. The [family composition](captures/house-01042-clear-day-r2/family-composition.png)
   is warm enough to see but pale/sparse, with overly bright leaf-card foliage.

Fixed: a deterministic, project-authored 1.80 × 1.95 m large refrigerator exterior
with plausible dual closed fronts, real-scale paint/steel/oak UVs and an integrated
0.75 m bridge cabinet covers the original model-less black aperture and ceiling gap.
The source origin was recentered after an origin gate exposed 6.65 cm of front-pull
asymmetry; the canonical prop translation compensates exactly, with zero >2-channel
pixel differences below the HUD in a final paired hall capture.
The unchanged canonical nested cell, opaque portal and door ID remain the gameplay
source of truth. Its currently static closed exterior uses the measured box proxy:
the shell punches the door hole and no moving obstacle exists yet, so leaving the
new closed appliance non-collidable would let the player pass through it. A later
door-animation task must replace the static frontage/proxy without changing those
stable IDs; this furnishing is not falsely called a functional fridge. The opening's
declared finish now agrees with the
visible white front and already-painted generated shut leaf. The measured kitchen
material-chunk count remains 11/11. Exactly the changed strict hall reference was
paired/viewed and deliberately updated; no wholesale golden regeneration.

`VISUAL-GATE-1` still **FAILS**. The next dependency-valid visual priority is
fixture-aware lighting/readability for Basic-effect furniture and appliances in
the selected L0 route, then the skeletal/day and unlit/night front approach and
the kitchen's remaining real primary kit. Do not hide this with debug colouring.

## Round 20 — remove false weather skin from enclosed cells

Commit: `HOUSE-01043` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 19 eleven-view [clear](captures/house-01042-clear-day-r2),
[noon](captures/house-01042-noon-r2), [overcast](captures/house-01042-overcast-r2)
and [22:00](captures/house-01042-night-r2). After: the same eleven unchanged
poses/conditions in [clear](captures/house-01043-clear-day-r1),
[noon](captures/house-01043-noon-r1),
[overcast](captures/house-01043-overcast-r1) and
[22:00](captures/house-01043-night-r1). The hall, inside kitchen, front and
family frames were actually opened; the golden changes were inspected in pairs.

Ranked visible defects remaining:

1. The unchanged [clear front](captures/house-01043-clear-day-r1/exterior-front.png)
   still has a skeletal roof/balcony/porch, repeated pale window squares and weak
   landscaping; the [night first view](captures/house-01043-night-r1/exterior-front.png)
   is still nearly invisible.
2. The [inside kitchen](captures/house-01043-clear-day-r1/kitchen.png) now shows
   the overhead shaker faces, but static joinery/appliances remain brown/dark;
   west/north uppers, range/hood, backsplash and purposeful props are missing.
3. The [clear hall](captures/house-01043-clear-day-r1/central-hall.png) is no longer
   pierced by pure-black cabinet rectangles, but its distant refrigerator
   presentation still needs light and the connected rooms need furnishing depth.
4. Family seating still reads pale and sparse; its foliage is too bright.

Fixed: ray picking of the *new* upper front identified an enclosed fridge cell's
falsely weather-facing wall at z=-26.300, y up to 3.65, 7 mm ahead of a recessed
painted cabinet door at z=-26.307; its actual declared ceiling is y=2.45. The
previous Round 19 attribution to BasicEffect lighting alone was incomplete.
The same hall pixel (900,350) changes from RGB(10,10,10) to (47,36,26), exactly
matching the unchanged lower painted panel. Its 11-pose day/noon/overcast/night
captures verify this in normal gameplay without a global brightness tweak.
The physically contained fridge, freezer and garage loft no longer generate any
outer-skin polygons; real exterior walls retain their existing path. Only those
three receiver UV2/lightmap products were rebaked/promoted, preserving 100 lm/W
selected-room lighting. Debug colours remain explicit-only, not normal gameplay.

`VISUAL-GATE-1` still **FAILS**. Next visible value: finish the west/north kitchen
primary kit and make its static Basic-effect detail readable; then front façade,
porch, roof and first-view night illumination. Do not mistake this focused defect
repair for a completed kitchen or exterior.

## Round 21 — measured kitchen cooking bay

Commit: `HOUSE-01044` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 20 eleven-view [clear](captures/house-01043-clear-day-r1),
[noon](captures/house-01043-noon-r1), [overcast](captures/house-01043-overcast-r1)
and [22:00](captures/house-01043-night-r1). After: the same eleven unchanged
poses/conditions in [clear](captures/house-01044-clear-day-r1),
[noon](captures/house-01044-noon-r1),
[overcast](captures/house-01044-overcast-r1) and
[22:00](captures/house-01044-night-r1). The west kitchen, hall, family and front
frames were opened at all four conditions.

Ranked visible defects remaining:

1. The [clear front](captures/house-01044-clear-day-r1/exterior-front.png) remains
   the largest defect: a skeletal orange/brown façade and porch, simple roof and
   almost no readable vegetation. The [night front](captures/house-01044-night-r1/exterior-front.png)
   is nearly invisible.
2. The [west kitchen](captures/house-01044-clear-day-r1/kitchen-facing-west.png)
   now has a recognizable range, burners, splashback and hood, but the compact bay
   and existing cabinetry are still too dark and lack controlled countertop detail.
3. The [hall view](captures/house-01044-clear-day-r1/kitchen-from-hall.png) reads as
   a real working kitchen silhouette, but brown static-prop lighting and the empty
   adjacent room flatten its depth.
4. Family seating remains pale and sparse; its bright plant and broad empty surfaces
   read as staging rather than an inhabited room.

Fixed: canonical portal measurements replace the image's misleading apparent
2.55 m blank span. A first 2.40 m assembly was visibly fuller but failed the closed
door walk gate by covering the pantry portal, so it was rejected. The final
1.00 m bay fits between pantry and butler openings with two 50 mm scribes. Its
23 deterministic parts include a 900 mm range, oven/control face, four burners,
150 mm physically tiled splashback and a steel filter hood/chimney. The separate
0.94 m counter measurement, source finish map, origin, stable IDs and 12-triangle
collision proxy are checked. Oven glass remains opaque at Tier S to avoid a new
order-dependent blend and was brightened only after the four captures were opened.
The affected west frame changes 1.279–1.304% and the hall frame 0.828–0.847%
across the four conditions. Both circulation routes and culling equivalence pass;
normal gameplay still uses production materials, not debug colour.

`VISUAL-GATE-1` still **FAILS**. Highest visible value is now the front approach:
consume the already canonical/provenanced vegetation and improve façade/porch/roof
composition without bypassing the exterior hierarchy. Kitchen under-cabinet
readability and restrained clutter follow; the full 62-container kitchen remains
open rather than being falsely claimed by this bay.

## Round 22 — the front approach is planted

Commit: `HOUSE-00772` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 21's eleven-view [clear](captures/house-01044-clear-day-r1),
[noon](captures/house-01044-noon-r1),
[overcast](captures/house-01044-overcast-r1) and
[22:00](captures/house-01044-night-r1) normal-game sets. After: the same eleven
poses and fixed conditions in clear *(capture removed)*,
noon *(capture removed)*,
overcast *(capture removed)* and
22:00 *(capture removed)*. The first
clear iteration *(capture removed)* is retained as rejected
evidence: oversized property trees hid the central entrance rather than framing it.

Ranked visible defects remaining:

1. The night first view *(capture removed)* is
   still almost black while cutout foliage catches too much residual sky light.
   The porch lanterns and entrance hierarchy do not yet read as a destination.
2. The clear front *(capture removed)* is now
   recognizably landscaped, but its thin roof edges, open balcony/porch frame and
   broad flat timber façade still read as an engineering shell.
3. The east-front `tree_small_02` crown is visibly sparse at the fixed road camera;
   its tiny cutout fragments read as speckles against sky. Distance LOD/impostor
   selection and a better approved role asset remain open work, not hidden here.
4. The connected interior route is materially unchanged: kitchen practicals and
   restrained clutter, family-room finish depth and dining furnishing remain weak.

Fixed: 309 deterministic approved CC0 placements now flow through the production
material/chunk/exterior hierarchy: 41 trees, 60 shrubs, 169 hedge shrubs, 18 flowers
and 21 grass patches. Selected visible roles use source-exact bark/branch/leaf
atlases; other catalogue roles use explicit vegetation fallbacks. Opaque wood uses
`BasicEffect`, cutouts use `AlphaTestEffect`, and no material-name hash/debug colour
is involved. The final property-tree positions preserve the gate→porch sightline.
Tree trunks and the 2.1 m hedge use the established collision build; the 18-pose
culled/unculled gate passes at a worst 0.1603% difference. The fixed clear-front
capture changes from an empty lawn at 44.3 fps to a planted frame at 38.8 fps; later
LOD work must recover distance cost without removing the composition.

`VISUAL-GATE-1` still **FAILS**. The next highest visible value is the night entrance
and façade/porch/roof depth, followed by the sparse close-road tree/foliage quality
and `HOUSE-00773`'s grass-card jitter. Normal gameplay remains production-material
only; explicit debug blockout remains separate.

## Round 23 — deterministic dusk control reaches the exterior

Commit: `HOUSE-01269` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 22's eleven-view fixed 22:00 set *(capture removed)*.
After: the same eleven cameras, time, weather, exposure and Tier-S/High settings in
[the dusk-sensor set](captures/house-01269-night-r2). The day sets are intentionally
unchanged because the automatic groups are exactly off in daylight. The
front before *(capture removed)* and
[front after](captures/house-01269-night-r2/exterior-front.png) were opened at original
resolution rather than accepted from a pixel count alone.

Ranked visible defects remaining:

1. The foreground and porch frame are now readable, but the broad façade and windows remain
   nearly black. The automatic state has exposed the next missing layer: neither porch lantern
   has a fixture mesh/emissive shade or a spatially local pool on the porch receiver.
2. The road cell currently receives its street group through the existing cell-level BasicEffect
   approximation. At full night that gives the lawn/plot useful visibility but too little local
   falloff; `HOUSE-01261`/`HOUSE-01262` own the approved per-object directional approximation.
3. Thin roof edges, the skeletal balcony/porch frame and flat timber façade still read as an
   engineering shell even where night illumination now separates them from the sky.
4. The connected interior route is unchanged: kitchen practical detail, family-room material
   depth and dining furnishing remain important after the first-view lighting defect.

Fixed: the 2 house porch, 9 street and 4 neighbour-porch fixtures are explicit automatic data,
not group-name special cases. They cross the shared live sun at -4° with deterministic stable-id
offsets over ±8 simulated minutes. During the stagger the existing one-atlas-per-group Tier-S
contract receives the lumen-weighted active fraction; it is 0 at noon and exactly 1 by 22:00.
The fixed front changes 40.507% of pixels. Porch-crop mean rises from RGB(6.55,7.27,9.17) to
(12.67,11.04,11.02); foreground mean rises from RGB(3.30,4.38,4.08) to
(26.72,28.40,12.25). This is useful visual progress without debug colour, but it is not a
substitute for the still-missing visible fixtures and local porch light.

`VISUAL-GATE-1` still **FAILS**. Next highest visible value is a real porch-lantern presentation
and localized warm entrance pool within the existing Tier-S architecture, followed by façade/
roof depth. Do not raise global night exposure to conceal the missing source.

## Round 24 — temporal fixture prerequisite (no new still set)

Commit: `HOUSE-01258` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

The fixed Round 23 images remain authoritative: initial loaded/dusk state deliberately snaps, so
adding the 0.12 s filament rise, instant LED response and deterministic 0.4 s fluorescent
flicker-start should not perturb a settled 22:00 still. This is recorded as a direct dependency of
`HOUSE-01259`, not presented as visual progress by itself. The ranked defects are unchanged: the
front still lacks physical lantern bodies/emissive shades and a local warm entrance pool, followed
by façade/roof depth. Capture again when those emitters are actually visible.

The two dark `SunSeasonRenderTests` references had still described the pre-`HOUSE-01269` exterior.
Their 42.24%/41.46% deltas were opened and inspected: the only material change is the approved
dusk-sensor readability already measured in Round 23 (porch frame, fence, lawn and road), not this
task's settled-state transition. Those two references were intentionally advanced; all other 46
render tests passed without a golden change.

## Round 25 — physical entrance lanterns and live diffusers

Commit: `HOUSE-01259` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 23's fixed [22:00 set](captures/house-01269-night-r2). After: the same eleven
cameras and condition in the [physical-fixture set](captures/house-01259-night-r1), plus a
[close night entrance](captures/house-01259-night-r1/exterior-approach-close.png) and the exact
[10:30 off-state check](captures/house-01259-night-r1/exterior-approach-day-check.png). The front,
close night and close day images were opened at original resolution.

Ranked visible defects remaining:

1. The two real bronze lanterns and warm diffusers now identify the door, but they cast no
   exposure-aware halo and no spatially local pool on the siding, porch floor or steps. At the
   road camera they are therefore small amber rectangles rather than convincing light sources.
2. The broad night façade remains almost black; thin roof edges, skeletal porch/balcony framing
   and flat timber siding still read as an engineering shell.
3. The road-cell approximation illuminates too uniformly, while nearby foliage catches more
   residual light than the house face. The approved per-object falloff path remains unfinished.
4. Connected interiors still need kitchen practicals/clutter, family material depth and dining
   furnishing after the first-view source presentation is complete.

Fixed: both placeholder entrance strips are replaced by deterministic 232-triangle, 0.608 m
wall lanterns with separate dark-bronze frames and exact switched diffuser slots. The live shade
uses the existing group colour and filament transition; its daylight state remains a faint
physical surface instead of glowing. Against Round 23, the fixed front changes 0.0602% of pixels
and a tight crop around the pair rises from RGB(15.29,12.65,12.32) to (18.54,14.82,12.85). The
small whole-frame delta is expected for two correctly scaled fixtures and is why the close checks
are retained. This passes the physical-emitter milestone, not the local-light milestone.

Golden review was selective: five exterior references changed only where the fixture pair is
visible. Two additional bulk-regeneration candidates showed unrelated vegetation-edge noise and
were rejected. The resulting 49 active software-render cases, including culled/unculled
equivalence, all pass.

`VISUAL-GATE-1` still **FAILS**. `HOUSE-01260` is now the highest-value valid task: add restrained
exposure-aware glow to these visible sources. Then use the approved local approximation for the
porch receiver and address façade/roof depth; do not raise global night exposure.

## Round 26 — exposure-aware physical fixture halos

Commit: `HOUSE-01260` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 25's [physical-fixture night set](captures/house-01259-night-r1). After: the same
eleven fixed 22:00 cameras in the [fixture-glow set](captures/house-01260-night-r1), plus the exact
[close night entrance](captures/house-01260-night-r1/exterior-approach-close.png) and matching
[10:30 off-state](captures/house-01260-night-r1/exterior-approach-day-check.png). The fixed road,
close night and close day frames were opened at original resolution.

Ranked visible defects remaining:

1. The two entrance sources now have restrained soft halos, but still cast no spatially local pool
   over siding, door surround, porch floor or steps. The broad façade therefore remains almost
   black and flat beyond the source itself.
2. Thin roof edges and the skeletal balcony/porch frame dominate both day and night. The close
   daylight image still reads as a cleanly materialed engineering shell rather than finished
   joinery and layered eaves.
3. The road-cell illumination is broad and uniform while nearby foliage catches more light than
   the house face. The approved per-object distance-attenuated approximation remains unfinished.
4. The connected interior route is unchanged in this focused source task: kitchen practicals and
   clutter, family-room material depth and dining furnishing remain high-value visual work.

Fixed: each live linked porch lantern now contributes one generated 64 px soft radial billboard in
the existing transparent pass. Its radius and alpha follow 400 lm source flux, the exact switched
filament level and adapted exposure. Additive/read-only-depth rendering preserves wall occlusion;
only the presentation sprite is shifted far enough toward the eye to clear its own shade. Unlinked
light points and the daytime automatic-off state draw nothing. Against Round 25 at the exact close
camera, each 100 x 100 fixture neighbourhood changes 35.2% of pixels and rises from approximately
RGB(15,12,9) to (18,14,9); the full paired crop changes 18.0%. The fixed road view changes only
subtly, which is intentional: this is source bloom, not a substitute for receiver lighting.

The real-device test now reads the colour target rather than merely counting a draw, and the full
render suite needs no golden update. The 18-pose culled/unculled maximum remains 0.1603%.

`VISUAL-GATE-1` still **FAILS**. The next highest visible value is a local warm entrance pool using
the approved Tier-S light assignment/receiver path, followed by façade/roof/porch form depth. Do
not raise global exposure to conceal either defect.

## Round 27 — baked cross-cell porch spill

Commit: `HOUSE-01281` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 26's [fixture-glow set](captures/house-01260-night-r1). After: the same eleven fixed
22:00 cameras in the [cross-cell spill set](captures/house-01281-porch-spill-r1), plus the exact
[close night entrance](captures/house-01281-porch-spill-r1/exterior-approach-close.png) and matching
[10:30 off-state](captures/house-01281-porch-spill-r1/exterior-approach-day-check.png). The fixed
road, close-night and close-day frames were inspected at original resolution.

Ranked visible defects remaining:

1. The two wall pools are now spatially local and warm, but the unbaked `L0_PORCH` floor, steps and
   columns remain almost uniformly dark. The pool therefore stops at the foyer-owned facade rather
   than grounding the entrance as one illuminated architectural composition.
2. Thin roof edges and the skeletal balcony/porch frame still dominate both day and night. The
   close daylight view needs layered eaves, believable joinery and stronger facade depth.
3. The broad night facade is still under-readable away from the two local pools; vegetation and
   the road receive more obvious modelling than the house face.
4. This receiver-focused change intentionally leaves the connected interior route unchanged:
   kitchen practical clutter, family-room material depth and dining furnishing remain visible gaps.

Fixed: the two canonical 400 lm porch sources now sit at the measured centre of their physical
diffusers rather than 10 cm behind the facade. Their runtime ownership, dusk sensor and fixture
links remain in `L0_PORCH`, while an explicit offline `bakeCells` receiver produces one additional
128 px Tier-S atlas for the foyer shell. Only this foreign binding is admitted to the outside-facing
skin; the foyer's own lamp groups remain excluded. At the exact close camera the paired fixture
crop changes 54.64% of pixels and rises from mean RGB(15.68,13.19,11.70) to
RGB(17.69,14.32,12.79). The facade crop changes 13.87%, while the porch-floor crop changes 0%,
accurately exposing the next task instead of hiding it. The matching day world crop differs by only
four pixels, proving the dusk-off presentation is materially unchanged.

The generated atlas has measured peak 10.696699 and mean 0.00655984 before normalisation. Schema,
semantic validation, subset promotion and the real renderer test cover the ownership boundary and
reject an undeclared foreign binding.

`VISUAL-GATE-1` still **FAILS**. Next highest visible value is to light the Basic-effect porch
floor/steps/columns with the approved distance-attenuated approximation, then deepen the
facade/roof/porch construction. Do not broaden the wall atlas or raise global exposure to fake the
missing receiver classes.

## Round 28 — distance-aware stock-XNA object lights

Commit: `HOUSE-01262` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 27's fixed [22:00 set](captures/house-01281-porch-spill-r1). After: the same eleven
cameras and condition in the [distance-aware set](captures/house-01262-point-light-r1), plus the
exact [close night entrance](captures/house-01262-point-light-r1/exterior-approach-close.png) and
[matching day view](captures/house-01262-point-light-r1/exterior-approach-day-check.png). The road,
close night and close day frames were opened at original resolution.

Ranked visible defects remaining:

1. The close daylight view makes the largest defect unambiguous: a thin slab edge and oversized
   open rectangular porch frame read as structural blockout, not layered eaves, soffit, fascia,
   balcony guard and believable painted joinery.
2. At night, the broad facade outside the two baked wall pools remains almost black. The porch
   floor reads, but its energy-preserving local model is still too weak to make the whole entrance
   composition feel grounded; this should be addressed with form/receiver calibration, not global
   exposure or a larger glow billboard.
3. The front steps belong to the adjacent exterior walk rather than the porch cell, so the two
   cell-local fixtures do not model them. Any cross-cell live receiver rule needs a real visibility/
   ownership contract rather than a porch-only exception.
4. The connected interior route is unchanged: kitchen practical clutter, family-room finish depth
   and dining furnishing remain the largest interior gaps.

Fixed: positional fixtures now aim from their canonical source to each submitted object's bounds
centre and use the approved authored-range falloff before the two brightest are selected. At the
exact close night camera the porch crop changes 28.46%, the floor crop 29.85%, the column crop
30.99% and the facade crop 28.29%. The two 400 lm sources arrive from opposite sides; their
attenuated energy slightly lowers the broad floor mean from RGB(15.36,8.69,4.99) to
RGB(14.24,8.36,4.95), while local form raises the column crop from RGB(17.35,12.88,10.70) to
RGB(17.85,13.24,10.78). This is an honest directional/falloff correction, not an exposure lift.
The matching day world changes 6.57%, confined to the intended Basic-detail fill/bounce response.

Nine strict day references changed because Round 27 predates both `HOUSE-01261`'s three-slot
daylight fill/bounce and this bounds-centre submission. All nine pairs were opened; camera and
geometry remain fixed, and only material/vegetation lighting changes. They were intentionally
advanced. The 18-pose culled/unculled maximum remains 0.1603%.

`VISUAL-GATE-1` still **FAILS**. Highest visible value is now facade/roof/porch construction depth,
followed by a principled adjacent-step receiver and the already identified interior furnishing
gaps. Do not spend the next checkpoint on invisible lighting infrastructure.

## Round 29 — finished canonical covered porch

Commit: `HOUSE-00929` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 28's exact [day approach](captures/house-01262-point-light-r1/exterior-approach-day-check.png)
and [close night entrance](captures/house-01262-point-light-r1/exterior-approach-close.png). After:
the matching day *(capture removed)* and
night *(capture removed)*, plus the complete thirteen-frame
Round 29 set *(capture removed)*. All were opened at original resolution.

Ranked visible defects remaining:

1. The front-door leaf is present in the fixed foyer view but absent when viewed from the exterior;
   the approach looks through the opening. Its exterior face/visibility is now the largest defect.
   The upper balcony's broad solid parapet is the next conspicuously plain entrance shape.
2. At night the porch profile reads, but the wider facade and front steps remain too dark. The
   steps are an adjacent-cell receiver and still need an architecture-consistent lighting answer.
3. The front facade and roof remain under-detailed at the road start: flat wall spans, thin roof
   edges and the large balcony band keep the whole house at an early-game level.
4. Inside, kitchen practical clutter, family-room finish depth and dining furnishing remain the
   largest connected-route gaps.

Fixed: the data-driven covered-exterior rule now produces four five-part square columns, a
continuous downward-facing soffit and a beam/fascia/cornice edge closing the actual 300 mm balcony
floor zone. The fixed day world/porch/column/roof-edge crops change
8.112/19.942/18.954/30.207% at a greater-than-two-channel threshold; night changes
8.334/20.491/18.001/31.854%. The day porch mean shifts from RGB(123.79,120.00,117.71) to
RGB(122.41,123.40,127.00); the night mean rises from RGB(16.23,12.49,10.80) to
RGB(22.44,16.45,12.70). The entrance now reads as a grounded covered porch rather than an open
rectangular scaffold.

Thirteen strict references containing the porch were compared and intentionally advanced; the
unaffected first-person interior set was left unchanged. The focused 18-test render/culling suite
passes, with 0.1599% worst culled/unculled error. `VISUAL-GATE-1` still **FAILS**. Fix the missing
exterior door face next, then simplify/refine the balcony parapet and facade rather than starting
another invisible subsystem.

## Round 30 — exterior-resident canonical entry leaves

Commit: `HOUSE-00930` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 29's matching day *(capture removed)* and
night *(capture removed)* approaches showed the front
walk, grass and foyer through the nominally closed entrance. After: the identical
day *(capture removed)* and
night *(capture removed)* cameras show an opaque
textured hardwood leaf, while the fixed
reverse foyer view *(capture removed)* retains its lit
interior face. The directory also contains the unchanged eleven-camera clear-day review set.

Ranked visible defects remaining:

1. The broad solid upper-balcony parapet and flat front facade now dominate the entrance; from the
   road they still read as simplified blockout masses despite the finished porch beneath them.
2. The entry leaf is materially real but geometrically plain: it needs believable panel relief and
   restrained hardware rather than remaining a textured rectangular slab.
3. The night porch profile is readable, but the adjacent front steps and broad facade remain too
   dark for a convincing arrival sequence.
4. Inside, kitchen practical clutter, family-room finish depth and dining furnishing remain the
   largest connected-route gaps.

Fixed: the two authored `D_ENTRY` rows now select a distinct approved hardwood role. The generator
derives that role from the exterior portal, emits one closed six-face leaf in the owning interior
cell, and the existing bounded exterior BVH admits only that role. Ordinary skirting, interior
doors and borrowed glass stay portal-owned. The canonical build moves from 577 to 579 chunks and
from 232 to 234 exterior instances: exactly the front and upper-balcony leaves. Ten affected
strict references were opened and intentionally advanced; the first-person front-door change is
the intended replacement of a flat colour by licensed wood detail, and all other changed regions
are confined to the two entry apertures.

`VISUAL-GATE-1` still **FAILS**. The next highest visible value is the upper balcony/front-facade
silhouette, followed by entry-panel/hardware depth and the already identified night-step receiver.

## Round 31 — open canonical balcony guards

Commit: `HOUSE-00931` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 30's matching day *(capture removed)*
and night *(capture removed)* approaches put one broad
painted block across the landing windows. After: the identical
day *(capture removed)* and
night *(capture removed)* cameras expose the facade,
windows and upper door through a complete open guard. The directory contains the unchanged eleven
route/composition cameras plus this exact pair; all thirteen frames were opened and inspected.

Ranked visible defects remaining:

1. The entry leaf is now the most conspicuous close-range simplification: licensed hardwood makes
   it materially credible, but it is still a flat slab without panel relief, threshold or hardware.
2. At night the two real lanterns identify the doorway and guard silhouette, but the adjacent steps
   and most of the facade remain too dark for a confident arrival sequence.
3. From the road the house still has a broad box-like roof/facade mass, thin eaves outside the porch
   and little balcony/landscape dressing to break its scale.
4. Inside, kitchen practical clutter, family-room finish depth, dining furnishing and the empty
   straight living-room view remain the largest connected-route gaps.

Fixed: every elevated exterior deck now uses one deterministic measured guard rule: 80 mm lower
rails, 45 mm painted balusters, 120 mm end newels and the existing brushed-metal rail at the
authored 1.10 m height. The spacing solver keeps the worst clear opening to 94.5 mm across the
front, rear and Juliet geometries. The visual guard opens, but the existing 200 mm-wide full-height
collision band remains continuous behind it. The exact daylight guard crop changes 38.883% of
pixels over two channel levels, while the already finished porch crop changes only 0.447%; the
whole close day/night frames change 5.4022%/3.7632%. The world remains 579 chunks and 234 exterior
instances. The larger detail count remains within budget: 44,607 shell triangles overall and
1,742/3,500 in the worst cell.

Seventeen strict references with a visible front or rear guard were inspected and intentionally
advanced; unaffected side views were retained. A close gameplay test now measures more than sixty
luma transitions through the guard rather than asserting that a solid parapet pixel is bright.
The 18-pose culled/unculled comparison remains green.

`VISUAL-GATE-1` still **FAILS**. Highest visible value is entry-door panel/hardware depth, followed
by the principled adjacent-step/night receiver and the connected-room furnishing gaps. Do not hide
the night defect with global exposure or turn the door into an exterior-only billboard.

## Round 32 — finished canonical entry doors

Commit: `HOUSE-00932` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 31's matching day *(capture removed)* and
night *(capture removed)* approaches showed a dark,
flat timber rectangle. After: the identical day *(capture removed)*
and night *(capture removed)* cameras show a four-panel
entry with a lock stile. The new close day *(capture removed)*,
close night *(capture removed)* and
foyer-side *(capture removed)* frames verify both faces;
the complete Round 32 set *(capture removed)* contains all eleven fixed clear-day
cameras plus the two approach and two close views. Every frame was opened at original resolution.

Ranked visible defects remaining:

1. At night the door and its two lanterns read, but the front steps, lawn-side approach and most of
   the facade remain almost black. The adjacent-step receiver is now the clearest arrival defect.
2. From the road, the house still has a broad box-like facade/roof mass and sparse balcony and
   landscape dressing; the detailed entrance occupies too little of that silhouette to carry it.
3. The connected interior route still needs kitchen practical clutter, family-room finish depth,
   dining furnishing and a composition for the empty straight living-room view.
4. The close facade exposes very broad siding repeats and dark lantern housings. Later facade
   material/detail work should improve them without undoing the finished door grammar.

Fixed: both canonical `D_ENTRY` leaves keep their original portal box and now gain four raised
panel outlines on both faces, paired 0.98 m lever/backplate sets, paired 1.35 m deadbolts and one
thin bronze threshold cap. The panel relief shares the approved board maps with a darker hardwood
tint; hardware reuses the approved brushed-metal maps. These two narrowly prefixed roles join the
existing exterior hierarchy without exposing general indoor wood or metal. Daylight uses the
outdoor sky/celestial term. At night, only fixture groups with an explicit foreign bake binding on
the owning cell can supply the matching stock-XNA directional/bounce approximation, so the front
leaf remains readable under its real porch lanterns without leaking those lights into foyer props.

The canonical result is 583 chunks and 238 exterior instances. The 45,543-triangle shell remains
well inside budget; `L1_BALCONY_REAR` is still worst at 1,742/3,500. Ten strict references were
inspected and selectively advanced: five route/property views see the new door directly, while
the HUD and four seasonal road views also exposed the prior Round 31 guard change that their old
references had not recorded. The 18-pose culled/unculled comparison remains below its unchanged
0.2% threshold (0.1599% worst).

`VISUAL-GATE-1` still **FAILS**. Highest visible value is a principled night treatment for the
front steps and immediate approach, followed by broad facade/roof depth and the named connected-
room furnishing gaps. Do not raise global exposure or enlarge the lantern billboards to mask it.

## Round 33 — authored porch-light spill onto the front steps

Commit: `HOUSE-01282` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 32's fixed night approach *(capture removed)*
left the 72-triangle front stair almost black beneath two visibly live porch lanterns. After: the
identical [night approach](captures/house-01282-step-spill-r1/exterior-approach-night.png) makes its
treads, risers and bluestone texture readable. The matching
[day approach](captures/house-01282-step-spill-r1/exterior-approach-day.png) and complete eleven-view
[22:00 route set](captures/house-01282-step-spill-r1) were opened and inspected.

Ranked visible defects remaining:

1. The broad upper facade and roof are almost black at the road and approach cameras at night; by
   day their uninterrupted box-like mass and thin outer eaves still dominate the otherwise finished
   entrance.
2. The immediate lawn-side walk remains intentionally dark while its player-switched path lights
   are off. The now-readable destination needs a believable approach-lighting/composition decision,
   not a global exposure increase.
3. The straight living-room camera still faces a nearly empty wall, while the composition camera
   shows the real seating group. Its layout needs a focal treatment that reads from circulation.
4. Kitchen practical clutter, family-room finish depth and dining furnishing remain the largest
   connected-route interior gaps.

Fixed: lights can now explicitly name adjacent unbaked static-detail receivers without changing
cell ownership. The two 400 lm dusk-controlled porch sources remain in `L0_PORCH`; only fixed
stock-`BasicEffect` geometry in named `EXT_WALK` receives their range-bounded direct terms and a
restrained warm receiver bounce. Dynamic objects, path-light switching, room state and exposure
remain local. The stair crop changes 35.84% and its mean rises from RGB(4.35,3.48,3.06) to
RGB(7.71,5.93,3.84). The matching daylight world crop changes zero pixels, and no strict reference
needed advancing. All 48 software-render cases pass; culled/unculled remains 0.1599% worst.

`VISUAL-GATE-1` still **FAILS**. Highest visible value is broad front-facade/roof depth and night
readability, followed by the named interior composition/furnishing gaps. Preserve the explicit
lighting ownership boundary; do not turn this into neighbour-by-proximity lighting.

## Round 34 — Colonial Revival front-window finish

Commit: `HOUSE-00933` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 32's clear-day front *(capture removed)* used
plain blue-grey rectangles around the completed entrance. After: the identical
front camera *(capture removed)* shows a repeated white
6-over-6 grille rhythm and paired black shutters across the canonical front elevation. The
foyer-side view *(capture removed)* confirms that the
new outside detail does not intrude through the entrance or expose closed-room trim. The complete
Round 34 set *(capture removed)* contains all eleven fixed clear-day route and
composition cameras; every frame was opened and inspected.

Ranked visible defects remaining:

1. The broad uninterrupted siding planes still read as a flat box from the road; window rhythm is
   now coherent, but facade depth and hierarchy are too weak at the mansion's scale.
2. The roof/eave silhouette remains thin and unfinished, and the dormers do not yet carry enough
   mass or trim to balance the improved lower windows.
3. The large right garage wing is a blank mass with little opening, planting or material breakup.
4. The straight living-room view remains nearly empty, while dining, kitchen practical clutter and
   family-room finish depth remain the largest connected-route interior gaps.
5. The facade and lawn-side approach still become too dark beyond the localized porch-step
   treatment at night.

Fixed: seventeen canonical front `W_DH_*` opening rows explicitly select the two features. Each
sash receives one vertical and two horizontal muntins, producing exactly six lights, while every
paired shutter is assembled from two stiles, three rails and eighteen angled slats at the standard
size rather than a flat black slab. Bays, sidelights, transoms, hoppers, dormers and all side/rear
windows remain unchanged. The new narrow shutter role reuses approved paint textures and is the
only added role admitted to the exterior-window hierarchy. The deterministic build contains 596
chunks and 251 exterior instances.

Sixteen strict references actually containing the new front detail were compared and intentionally
advanced; unaffected references were retained. All 48 active software-render cases pass, and the
18-pose culled/unculled comparison remains under its unchanged 0.2% threshold at 0.1599% worst.

`VISUAL-GATE-1` still **FAILS**. Highest visible value is deeper roof/eave and facade massing, then
the blank garage wing and the named connected-room furnishing gaps. Do not turn the new grammar
into a front-cell heuristic or spread shutters to elevations and opening types that do not author
them.

## Round 35 — layered cornice and finished dormers

Commit: `HOUSE-00934` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 34's front *(capture removed)* ended both the
main and garage siding planes at a thin dark roof edge, and its five wall dormers read as grey
shingle boxes. After: the identical front camera *(capture removed)*
shows a continuous 280 mm white frieze and projecting 100 mm crown, with sided dormer fronts and
painted corner/header/rake outlines. The complete
Round 35 set *(capture removed)* contains all eleven fixed clear-day route and
composition cameras; every frame was opened and inspected.

Ranked visible defects remaining:

1. The broad siding planes still lack enough facade hierarchy and depth, especially across the
   blank garage/right wing.
2. The straight living-room view is almost empty despite the finished seating composition visible
   from the alternate camera.
3. Dining furnishing is missing; kitchen practical clutter and family-room finish depth remain
   visibly behind the completed primary furniture.
4. The night approach beyond the localized porch and step spill leaves most of the facade nearly
   black.
5. The dormers now read architecturally, but their small windows remain much simpler than the
   finished front-elevation double-hung windows below.

Fixed: the eave finish is generated from the settled canonical eave and wall faces, not a facade
overlay. Its two spatial layers also finish the garage mass. Dormer roof faces remain shingles;
only vertical faces switch to the approved wood-board siding source through a dedicated unbaked
Basic variant, with measured painted trim in front. Roof planes, holes, ridge, drainage, collision
and the lightmap receiver set are unchanged. The shell is 56,585 triangles and the world contains
597 chunks / 252 exterior hierarchy instances.

Seventeen strict references that actually see the finish were compared and intentionally
advanced; unaffected references were retained. The 18-pose culled/unculled comparison remains
under its unchanged 0.2% threshold at 0.1599% worst.

`VISUAL-GATE-1` still **FAILS**. Highest visible value is now the broad flat garage/facade mass or
the empty straight living-room composition; choose the shortest dependency-valid change with the
largest improvement in the fixed captures rather than adding more invisible infrastructure.

## Round 36 — closed and panelled sectional garage door

Commit: `HOUSE-00935` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 35's front *(capture removed)* looks through the
entire 4.90 × 2.40 m garage aperture because its closed room-owned leaf used ordinary indoor steel
and correctly vanished with `L0_GARAGE`. After: the identical
front camera *(capture removed)* shows a closed warm-charcoal
sectional door, and the new fixed driveway camera *(capture removed)*
makes its five rows of four raised panels directly reviewable. The complete
Round 36 set *(capture removed)* contains twelve fixed clear-day route,
composition and facade cameras; every frame was opened and inspected.

Ranked visible defects remaining:

1. The straight living-room camera is still almost empty and substantially darker than the
   alternate furnished composition; it is now the largest defect on the connected interior route.
2. Above the finished door, the broad garage siding plane has no window, planting hierarchy or
   other strong facade articulation and still reads as an oversized box.
3. Dining furnishing is absent; kitchen practical clutter and family-room finish depth remain
   visibly behind the primary furniture already present.
4. The facade outside the localized porch/step spill remains too dark in the night approach.
5. Several daylight interiors still have flat, dark ceiling/wall transitions despite valid baked
   contribution and would benefit from later calibrated finish/lighting depth.

Fixed: the canonical leaf remains in `L0_GARAGE`, but its body and panel finishes alone use the
narrow exterior-door prefix consumed by §25.6. Twenty shallow closed boxes provide real 14 mm
relief on both faces and match the future five-segment animation contract; a sibling fine-paint
tint makes the relief readable in strict Tier S without a billboard or new renderer. Aperture,
portal, threshold, collision and interaction data are untouched. The front frame changes 13,201
pixels (0.917%) and the close strict property-drive frame changes 7.176%. The shell is 57,065
triangles; the world contains 599 chunks and 254 exterior hierarchy instances.

Ten strict references that actually see the leaf were compared and intentionally advanced;
unaffected references were retained. All 48 active software-render cases pass, and the 18-pose
culled/unculled comparison remains under its unchanged 0.2% threshold at 0.1599% worst.

`VISUAL-GATE-1` still **FAILS**. Highest visible value is the empty straight living-room view,
followed by garage-facade articulation and the remaining connected-route furnishing gaps. Do not
undo the room ownership boundary or broaden the exterior-door prefix to ordinary indoor joinery.

## Round 37 — formal living-room upright piano

Commit: `HOUSE-01045` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 36's fixed straight living-room view *(capture removed)*
looked across the seating foreground to an almost completely blank east wall. After: the identical
[clear-day view](captures/house-01045-piano-r2/living-room.png) has a full-size walnut upright as a
focal object, with its inset case, 88-key keyboard, music desk, legs and pedals readable rather
than implied by a box. The same camera was inspected at [clear noon](captures/house-01045-piano-noon-r2/living-room.png),
[overcast 10:30](captures/house-01045-piano-overcast-r2/living-room.png) and
[clear 22:00](captures/house-01045-piano-night-r2/living-room.png). The complete
[Round 37 clear set](captures/house-01045-piano-r2) contains all twelve fixed route, composition
and facade cameras; every frame was opened.

Ranked visible defects remaining:

1. The broad garage wall above the finished sectional door still lacks facade hierarchy and is
   the largest defect in the exterior approach.
2. Dining remains unfurnished and breaks the otherwise increasingly credible connected L0 route.
3. Kitchen practical clutter and family-room secondary detail lag their completed primary pieces.
4. The piano wall still needs restrained art or a physical accent fixture; the existing authored
   piano light is useful, but its `fixtureProp` remains null.
5. Most of the facade outside the localized porch and stair spill remains too dark at night.

Fixed: one deterministic, project-authored 1.485 × 1.240 × 0.733 m upright adds 12,444 visible
triangles, a 12-triangle collision proxy and four approved stock-`BasicEffect` finish roles. Its
east-wall placement remains south of the foyer portal; the double leaves swing into the foyer and
the main route remains clear. The existing 450 lm `LG_L0_LIVING_PIANO` accent starts on, while the
main ceiling group stays off after an all-main-lights iteration visibly flattened the room. The
world is 603 chunks / 24 static props; `L0_LIVING` is 21 chunks / 20 materials. The unculled
diagnostic is 603 draws / 93 state changes, inside §71.2's worst-case envelope.

The only strict golden that changed is the explicit debug `blockout-l0-living` frame: its 10,800
changed pixels (4.6875%) are exactly the new piano silhouette, inspected before selective
advancement. All production first-person references remained valid, and the 18-pose
culled/unculled suite passed unchanged.

Final gates: deterministic generation and the full 31-stage content build pass; unit tests are
1,408/1,408, integration registrations 135/135, software render cases 49/49 active and strict-XNA
translation units 323/323. Compilation was capped at six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only; this round
closes its most obvious formal-living blank but does not claim the room or the whole route final.

## Round 38 — garage hip roof revealed

Commit: `HOUSE-01046` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 37's fixed [driveway view](captures/house-01045-piano-r2/garage-approach.png) had a
nine-metre-wide siding rectangle above the sectional door. The garage's authored head was already
the +4.30 m roof eave, but the generic outer-skin rule jumped to L2's +6.55 m floor and placed that
wall through the separate roof. After: the identical [clear-day camera](captures/house-01046-garage-roof-r1/garage-approach.png)
shows the approved charcoal-shingle hip and its real silhouette. Matching [clear noon](captures/house-01046-garage-roof-noon-r1/garage-approach.png),
[overcast](captures/house-01046-garage-roof-overcast-r1/garage-approach.png) and
[night](captures/house-01046-garage-roof-night-r1/garage-approach.png) views were opened, as were
all twelve frames in each complete scenario set.

Ranked visible defects remaining:

1. The garage and most of the facade are almost black at night; a plausible physical garage/front
   practical is now the largest exterior defect.
2. Dining remains unfurnished and breaks the connected L0 domestic route.
3. Kitchen practical clutter and family-room secondary detail trail their primary furniture.
4. The garage face is materially coherent but still broad and simple below the now-correct roof.
5. The piano wall needs restrained art or a physical accent fixture.

Fixed: the generic rule now adds a joist band only when a cell head coincides with a real level
ceiling. Ordinary L0 walls still bridge +3.30 to +3.65 m, while custom roof/stair-bound heads stop
where authored; no garage id, facade overlay or roof edit was introduced. Only `L0_GARAGE` and
`B1_UNDERSTAIR` changed among 99 source shell files. Both received fresh UV2 and promoted day/night
atlases. The world remains 603 chunks / 24 props.

Fourteen strict references that actually contain the corrected shell or its deterministic UV2
repack were compared and intentionally advanced; unaffected references were retained. All 49
active software-render cases pass, including 18-pose culled/unculled equivalence.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only. Highest visible
value is a real exterior garage/front practical with bounded warm spill, followed by dining
furniture and controlled kitchen/family detail.

## Round 39 — physical garage floodlight and bounded night spill

Commit: `HOUSE-01047` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 38's fixed [22:00 driveway](captures/house-01046-garage-roof-night-r1/garage-approach.png)
has no physical source and leaves the door and asphalt almost black. After: the identical camera
shows the compact wall pack in [clear daylight](captures/house-01047-garage-flood-day-r1/garage-approach.png),
its truthful [manual-off night](captures/house-01047-garage-flood-night-off-r1/garage-approach.png)
and its [manual-on night](captures/house-01047-garage-flood-night-on-r1/garage-approach.png). All
twelve frames in each fixed set were opened and inspected.

Ranked visible defects remaining:

1. Dining remains unfurnished and is the largest break in the connected L0 domestic route.
2. Kitchen practical clutter and family-room secondary detail still trail their primary pieces.
3. The night garage is now locally readable, but the broader facade remains correctly dark beyond
   the flood cone and still needs its own authored practical/landscape composition.
4. The garage elevation remains broad and simple even though its door, roof and practical now read.
5. The living-room piano wall needs restrained art or a linked physical accent fixture.

Fixed: the existing manual 4000 K / 3000 lm group now owns a deterministic 420 × 203 × 195 mm
dark-bronze wall pack, a linked neutral emissive lens and explicit fixed receivers for the
sectional door and the terrain chunk beneath the driveway. The authored source sits at the lens
and aims 25° outward from vertical through a 40°/70° feathered cone. Stock-`BasicEffect` fixture
assignment now respects that full-angle cone, preventing the former test iteration from lighting
the side fence and whole yard like a point source. Spot presentation glows expose less apparent
area than an equal-lumen globe while receiver energy remains unchanged.

The normal manual switch remains off. Repeatable `--light-on=<group>` is a deterministic
review-only override of the real group, so the on-state does not create a parallel lighting path
or change saved/default gameplay. In the fixed crop the door mean rises from approximately
RGB(5.28,4.55,4.85) to RGB(15.78,10.73,8.70), and the near asphalt from
RGB(2.33,2.35,3.82) to RGB(9.96,8.01,7.60), while the right fence changes only marginally.

Two strict first-person references were intentionally advanced after inspection: the kitchen's
four default-on downlights now obey their 140° cones, and the hall changes only where it looks
through that kitchen opening. A too-broad exterior-door fallback that initially warmed the front
door with the foyer ceiling light was rejected and narrowed to explicit spill before reference
updates. No season reference changed in its serial confirmation run.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only; the flood is
one finished practical, not permission to lift global night exposure. Highest visible value is
dining furniture, followed by controlled kitchen/family detail and a separate authored treatment
for the remaining dark facade.

## Round 40 — furnished and illuminated formal dining room

Commit: `HOUSE-01048` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: the fixed clear-day dining view *(capture removed)*
looks through a completely empty, almost black 6.0 × 2.8 m room between the finished kitchen and
living room. After: the identical [clear-day camera](captures/house-01048-dining-day-final/dining-room.png)
shows a measured walnut table for eight, eight upholstered chairs, an anchoring rug and a physical
six-shade chandelier. The matched [22:00 view](captures/house-01048-dining-night-final/dining-room.png)
proves the real 2700 K group drives both the exact shade emission and the existing baked room
contribution. Each linked directory is a complete thirteen-camera route set and every frame was
opened; the new dining camera looks along the table while retaining both kitchen and living edges.

Ranked visible defects remaining:

1. The kitchen has its primary cabinetry/appliances but still lacks purposeful countertop and
   island detail; it is the clearest remaining furnished-route gap.
2. Dining needs a restrained sideboard, wall art and table setting to progress from primary
   furnishing to a lived-in room without obstructing its unusually narrow circulation.
3. Family-room secondary objects and finish depth still lag its primary seating composition.
4. The 2700 K Tier-S practical reads more orange/saturated than a camera-white-balanced interior;
   later lighting calibration should address that locally rather than lifting global exposure.
5. The garage/front elevation remains broad and the facade outside authored practicals is still
   sparse and appropriately dark at night.

Fixed: three deterministic project-authored models add a 2.35 × 0.98 × 0.76 m breadboard table,
a 0.535 × 0.520 × 0.985 m chair reused eight times, and a 1.25 m six-shade bronze chandelier.
The table and chair carry explicit collision proxies; all three have validated UV0, material-slot,
origin, scale and deterministic-hash contracts. Existing approved walnut/brass/rug sources are
reused, with one new approved sage upholstery binding. The eleven placements preserve the four
portal routes and leave measured clearance at both table ends. The physical `ChandelierShade`
slot now belongs to the existing source, which starts on for a readable windowless new-game route
but remains controllable by its authored switch.

The review harness now runs every set in a clean temporary XDG profile, preventing an old saved
switch state from silently changing an otherwise fixed screenshot. The world is 611 chunks / 36
static props; `L0_DINING` has an explicit ten-chunk measured exception for its shell, four furniture
finishes, rug, fixture metal and switched emissive role. Only the two inspected kitchen references
that see the new chairs/warm spill through the dining opening were intentionally advanced. All 49
active software-render cases pass, including the unchanged 18-pose culled/unculled comparison.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and the connected
L0 route no longer crosses an empty dining blockout, but kitchen/family secondary detail and the
remaining exterior composition are not yet at the gate's credible-house bar.

## Round 41 — occupied kitchen island and worktops

Commit: `HOUSE-01049` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 40's fixed [hall-side kitchen view](captures/house-01048-dining-day-final/kitchen-from-hall.png)
shows a long bare island and empty worktops despite completed cabinetry and appliances. After: the
identical [final camera](captures/house-01049-kitchen-dressing-day-final/kitchen-from-hall.png)
shows three counter stools, a cutting-board/bowl/produce focal group, sink-run canisters and a
kettle at the cooking bay. The matching [reverse view](captures/house-01049-kitchen-dressing-day-final/kitchen-facing-west.png)
confirms the seats tuck under the island and the east aisle remains open. All thirteen images in
the final clear-day set were captured from the clean fixed profile and opened for review.

Ranked visible defects remaining:

1. Daylight in the kitchen and central hall remains flat and underexposed; the north cooking bay
   and ceiling/wall transitions lose depth while neighboring window apertures clip bright.
2. The family room still needs controlled secondary objects and more convincing finish/lighting
   depth around its existing primary seating and media composition.
3. Dining needs restrained art, a sideboard or table setting, but its narrow circulation makes
   indiscriminate clutter inappropriate.
4. The broad exterior/garage composition remains sparse beyond its completed architectural shell
   and bounded practicals.
5. Several selected-room furnishings still inherit simpler unbaked object lighting than their
   surrounding lightmapped shell.

Fixed: four deterministic project-authored GLBs add one collision-bearing 0.65 m-seat-height stool
reused three times and three deliberately composed countertop groups. Existing approved walnut,
sage upholstery, steel, paint and ebonite sources are reused; a single subdued lemon finish adds
produce colour without emission. The first stool line failed the unchanged full-house tour at the
south kitchen wall, so the final line moves 0.25 m under the island and retains approximately
0.72 m rear clearance. The world is 615 chunks / 42 static props, with only `L0_KITCHEN` raised to
its exact measured 17-chunk boundary.

Only `blockout-l0-kitchen` and `fp-l0-kitchen` changed beyond tolerance. Their actual/reference
pairs were inspected and selectively advanced; the complete 48-case software-render suite passes,
including the unchanged 18-pose culled/unculled equivalence. Unit and integration suites pass
1409/1409 and 134/134 respectively.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only. The kitchen no
longer reads as an empty fitted showroom, and the highest visible value has shifted from adding
objects to calibrating believable daylight and depth along the kitchen/hall route.

## Round 42 — physical kitchen pendants and bounded island light

Commit: `HOUSE-01283` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 41's [hall-side kitchen view](captures/house-01049-kitchen-dressing-day-final/kitchen-from-hall.png)
shows an occupied room whose island still has no physical lights and little focal depth. Directly
switching on its three old unlinked points exposed a larger defect: they ran along z beside the
long island and threw a broad orange patch onto the ceiling. After: the identical
[final hall-side camera](captures/house-01283-kitchen-pendants-day-final/kitchen-from-hall.png) and
[reverse kitchen camera](captures/house-01283-kitchen-pendants-day-final/kitchen-facing-west.png)
show three bronze bell pendants, bright frosted diffusers and a localized warm pool on the island.
All thirteen clear-day frames were opened at full resolution. Matching temporary 22:00 captures
were also inspected and confirmed that the physical shades and downlight composition remain
readable without an upward ceiling bloom.

Ranked visible defects remaining:

1. The north cooking bay remains dark and blocky beside the much clearer island composition.
2. The separate nominal under-cabinet group is authored at ceiling height; when explicitly
   switched on it creates ceiling spots instead of plausible worktop light.
3. Family-room secondary objects and finish/lighting depth still lag the completed kitchen and
   dining primary compositions.
4. The broad exterior/garage composition remains sparse beyond its finished architectural shell
   and bounded practicals.
5. The clear-day hall-to-kitchen exposure is improved compositionally but remains dark compared
   with the clipped exterior apertures.

Fixed: one deterministic project-authored 330 x 875 x 330 mm bronze/frosted model is reused at
three measured x positions over the island centreline. Each of the existing switch group's 700 lm,
2850 K sources now sits at its physical diffuser, aims down through a 48/92 degree feathered cone
and names the exact `PendantShade` emission slot. The group starts on in normal gameplay and
retains its authored wall switch. A selected 256-sample rebake lowers the island atlas peak from
the rejected point result of 0.8003 to 0.1518. The world is 617 chunks / 45 static props, with only
`L0_KITCHEN` raised from its exact 17-chunk boundary to 19 for shared bronze and switched diffuser
roles.

The explicit blockout kitchen reference and production kitchen reference were advanced only after
direct side-by-side inspection of the new silhouettes. The main-hall view into the kitchen and the
adjacent hall-corner reference also changed from the now-live group/exposure; the latter changed by
only 0.6589% with a maximum channel delta of 3. Both pairs were inspected before selective
advancement. No other committed golden was changed.

Verification passes 1409/1409 unit tests, 134/134 integration tests and all 48 active software-
render cases. The unchanged 18-pose culled-vs-unculled gate peaks at 0.1320%, and the complete
repository gate accepts all 323 strict-XNA translation units. Compilation and heavy tooling were
capped to CPU 0-5 / six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and the island is
now a believable lit focal point. Highest visible value is correcting and physically representing
the cooking-bay/under-cabinet light, followed by restrained family-room secondary detail.

## Round 43 — physical range-hood task lights

Commit: `HOUSE-01284` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: explicitly enabling the old nominal under-cabinet group in the fixed
[west kitchen](captures/house-01284-undercab-before-on/kitchen-facing-west.png) and
[hall-side kitchen](captures/house-01284-undercab-before-on/kitchen-from-hall.png) produces four
white ceiling hot spots. The sources were actually unlinked points at y 3.20 m distributed over
the room, not under cabinets or attached to any fixture. After: all thirteen normal clear-day
views are in [the final day set](captures/house-01284-range-task-day-final), and all thirteen
22:00 views with the real manual group explicitly switched on are in
[the final night set](captures/house-01284-range-task-night-on-final). Every frame was opened; the
matched kitchen views were also compared at full resolution.

Ranked visible defects remaining:

1. The cooking bay remains too dark and visually blocky even though its source is now physically
   correct; the unbaked black range/hood materials receive too little local contrast.
2. Family-room seating exists, but sparse secondary objects and flat grey surfaces make it lag the
   living, dining and kitchen compositions.
3. The broad exterior/garage frame remains sparse beyond its coherent shell, vegetation and
   bounded practicals.
4. Bright windows still overpower the hall-to-kitchen composition in clear daylight.
5. Several selected-room furniture chunks remain simpler under stock `BasicEffect` lighting than
   their lightmapped architectural surroundings.

Fixed: one deterministic project-authored 82 x 18 x 82 mm brushed-steel puck with a separate
frosted diffuser is reused four times across the existing range-hood filter. The four stable
400 lm, 3000 K sources now sit 10 mm below the lenses, aim downward through 50/100 degree cones,
stop at 2.40 m and link their exact `PuckDiffuser` slots. The selected 256-sample atlas peaks at
0.2341 rather than the malformed ceiling distribution's 8.6039. The manual group retains its
original default-off state and total 1600 lm weight: the fixed final west-day frame differs from
Round 42 by mean RGB only (0.0125, 0.0126, 0.0126)/255, confined to the new geometry. A rejected
always-on clear-day set *(capture removed)* visibly darkened the whole room
through correct exposure adaptation, so it was not promoted merely to make the new lights obvious.

The world is 618 chunks / 49 props. Puck steel reuses the existing kitchen-hardware finish; only
the independently switched warm diffuser raises `L0_KITCHEN` from nineteen to its exact measured
twenty-chunk boundary. No collision, circulation, portal or culling rule changed. No strict render
reference changed beyond tolerance: all 48 active render cases pass, and the eighteen-pose
culled/unculled comparison remains at 0.1320% worst case. Unit tests pass 1409/1409; the integration
run passes its 126 ordinary cases plus all 10 SaveStore cases with the required writable temporary
data root. Compilation and heavy tooling were capped to CPU 0-5 / six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only, and the bogus
ceiling spots are gone. The next work must produce a larger visible gain: first improve the dark
cooking-bay material/local-readability boundary if it can be done within the approved stock-XNA
path; otherwise move directly to the visibly sparse family-room composition.

## Round 44 — family-room focal materials

Commit: `HOUSE-01050` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: the new fixed front-on family-media view *(capture removed)*
shows the existing television as a cream blank rectangle because its sole imported palette slot
was mapped to upholstery. The media unit source's explicitly named `BlackMarble` mesh was flattened
into the same role. After: the identical [final view](captures/house-01050-family-media-day-final/family-media.png)
reads immediately as a dark inactive television over a pale surround with a contrasting dark
stone hearth/base. The final directory is the complete fourteen-camera clear-day route; its contact
sheet and every frame were opened, and the focal pair was compared at full resolution.

Ranked visible defects remaining:

1. The family room still lacks restrained secondary furniture and decor; the large pale casework,
   blank flanking doors/walls and isolated bright plant keep it behind the living/dining rooms.
2. The media-unit geometry is a coarse mantel-like source rather than convincing contemporary
   cabinetry; finish separation helps, but silhouette and object density remain weak.
3. The family-room light is flat around the focal wall and the window remains much brighter than
   its interior receivers.
4. The broad exterior/garage frame remains sparse beyond its coherent architectural materials,
   vegetation and bounded practicals.
5. Several selected-room furniture chunks remain visibly simpler under stock `BasicEffect` than
   their lightmapped architectural surroundings.

Fixed: two manifest bindings now preserve inspected source semantics without replacing the
approved CC-BY geometry. `TvScreen` and `TvBevel` share one coarse source slot and receive a dark
blue-black opaque screen finish with a restrained tight highlight. The media unit keeps its cream
painted carcass while its exact `BlackMarble` node receives dark marble. A permanent checker guards
the source nodes, mappings, finish bounds and canonical alignment. The new fixed camera closes the
review blind spot that allowed both focal objects to look blank in otherwise valid family captures.

The matched frame changes 33,574 pixels (2.33%) with mean absolute normalized RGB difference
0.00461 (about 1.18/255): a deliberately local but plainly visible correction. The world remains
49 props and rises from 618 to 620 chunks solely for the two restored family roles; the exact
`L0_FAMILY` boundary is seventeen. No collision, portal, light, exposure, placement, provenance or
renderer rule changes, and no pixel golden required advancement. Verification passes 1409/1409
unit tests, 135/135 serial offscreen integration tests and all 49 active software-render cases;
eight capture-only cases remain disabled. Compilation and heavy work were pinned to CPU 0-5 and
at most six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only. The next
highest-value change is a measured family-room secondary furnishing/decor composition around this
now-readable media focus, followed by local family lighting depth rather than global exposure.

## Round 45 — family-room secondary composition

Commit: `HOUSE-01051` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 44's [front media view](captures/house-01050-family-media-day-final/family-media.png)
has a corrected television and hearth but a blank north wall, one isolated floor plant and no
physical object for the canonical dog bed. After: the identical
[final media view](captures/house-01051-family-secondary-day-final/family-media.png) and
[reverse composition](captures/house-01051-family-secondary-day-final/family-composition.png)
show a book-and-plant reading wall, framed art, physical bolster bed and a sofa-side table with a
book. The final directory contains the complete unchanged fourteen-camera clear-day route; every
frame was captured and the three family views were inspected together and against Round 44.

Ranked visible defects remaining:

1. Family-room daylight is still flat and dark away from the clipped windows; the new objects add
   depth cues but do not receive the architectural shell's baked gradients.
2. The pale sofa and mantel-like media source remain geometrically simple compared with the new
   detailed casework and the formal living/dining compositions.
3. The broad exterior/garage view is materially coherent but still sparse at player-start scale.
4. Bright clear-day apertures overpower several hall-to-room views.
5. Secondary objects in the other selected rooms remain sparse, especially wall art and purposeful
   surface dressing outside the kitchen.

Fixed: four deterministic project-authored assets add measured domestic roles without new runtime
architecture or third-party licence risk. The 0.90 x 0.66 m dog bed is linked to
`BED_DOG_FAMILY`; a 1.15 x 1.55 m bookcase carries eighteen deliberately varied volumes; a 0.52 m
round table carries one reading book; and a 0.92 x 0.64 m framed abstract uses raised physical
shapes rather than a copied image. The existing plant now dresses the bookcase top. The first
visual iteration rejected an artwork embedded in the wall and an unreadably flat pet bed. The
first bookcase position then failed the unchanged tour recovery check; its final right-wall
position passes the full route. The wall-mounted instance does not enable its redundant proxy,
while the freestanding table remains collidable.

The matched media frame changes 99,234 pixels (6.89%) with normalized MAE 0.00393; the reverse
composition changes 46,512 pixels (3.23%) with normalized MAE 0.00209. The world reaches 624
chunks / 53 props, and only `L0_FAMILY` rises from seventeen to its exact measured 21-chunk
exception. The deliberately unculled diagnostic measures 624 draws / 91 state changes, still well
inside §71.2's 1,400 / 210 worst-case row; named visible-pose budgets and the 0.2% culling gate
remain unchanged.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only. Highest visible
value is now local family-room lighting/readability, followed by the simplified pale seating/media
silhouette and broader exterior composition—not additional invisible infrastructure.

## Round 46 — physical family-room practicals

Commit: `HOUSE-01285` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 45's complete unchanged
[clear-day set](captures/house-01051-family-secondary-day-final) shows a furnished but flat family
room under a blank ceiling. The new complete
explicit-reading 22:00 baseline *(capture removed)* also proves
that the visible floor-lamp shade stays dark while its nominal point lights the room from 3.15 m
away. After: all fourteen matching cameras were recaptured for
[normal clear day](captures/house-01285-family-fixtures-day-final) and
[explicit-reading 22:00](captures/house-01285-family-fixtures-night-final). All four sets and the
six matched family frames were opened at full resolution.

Ranked visible defects remaining:

1. The pale sofa and mantel-like media unit are still coarse, bright silhouettes compared with
   the detailed bookcase and the stronger living/dining compositions.
2. Clear-day apertures remain clipped and visually overpower interior receivers, especially in
   the reverse family composition and connected hall views.
3. The broad exterior/garage start frame is materially coherent but still sparse at player scale.
4. Family-room wall treatment and controlled surface dressing remain restrained enough that the
   large focal-wall bays feel unfinished.
5. Several furniture chunks remain flatter under stock `BasicEffect` than the lightmapped shell.

Fixed: one deterministic project-authored 420 x 180 x 420 mm semi-flush fixture is reused four
times. A dark-bronze canopy, trim and finial frame the separately switched opal diffuser. The four
stable main sources sit at their physical y 3.12 m optical faces, aim down through 72/140 degree
spots and retain a warm 3000 K domestic distribution at 1200 lm each. The existing family floor
lamp keeps its approved White Room geometry and texture provenance through a byte-identical
derived asset identity; only that instance's shade receives the switched-emitter role. Its 450 lm
reading source now sits at [8.05, 1.997, -26.45] and remains off until explicitly switched.

The selected 256-sample main atlas peaks at 0.2292 rather than 0.1977. The corrected reading atlas
peaks at 0.5822 rather than 0.1368 because it now originates at the shade. Day matched frames
change 81.59–96.85% of pixels with normalized MAE 0.01334–0.02098; night matched frames change
79.98–94.76% with normalized MAE 0.01181–0.01997. The broad pixel coverage is the expected selected
room lightmap change, while direct inspection confirms that new silhouettes and gradients remain
localized to the family room rather than a global exposure edit.

An initial after capture was rejected: it revealed that the freshly generated repository
`content/world/chunks.bin` had not yet been copied to the runtime's `build/content` tree and still
reported 624 chunks. The CMake content target synchronized the two roots and compiled both new
models; every promoted final frame explicitly reports 626 chunks. The rejected capture is not a
review result.

The world is 626 chunks / 57 props / 256 exterior hierarchy instances / 53.2886 MB. Only
`L0_FAMILY` rises from its exact 21-chunk boundary to 23 for the shared bronze role and independent
switched diffusers. The unculled diagnostic is 626 draws / 92 state changes, still far below
§71.2's 1,400 / 210 row. Verification passes 1409/1409 unit tests, 135/135 serial offscreen
integration tests and all 49 active software-render cases. The eighteen-pose culled/unculled gate
still peaks at 0.1320% (`l0-sunroom`), and no strict golden required advancement. Compilation and
heavy tooling were capped to CPU 0-5 / six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only. The family room
now has believable visible sources and nighttime depth; the next highest visible value is replacing
or materially improving its coarse pale sofa/media composition, followed by the sparse exterior
start framing—not more hidden infrastructure.

## Round 47 — verified close-range family sofa

Commit: `HOUSE-01052` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 46's [clear-day reverse composition](captures/house-01285-family-fixtures-day-final/family-composition.png)
is dominated by the shared White Room sofa's pale rectangular back. After: the identical camera in
the complete fourteen-view [final clear-day set](captures/house-01052-family-sofa-day-final/family-composition.png)
shows a grounded navy curved sofa with an upholstered silhouette and open metal legs. The same
composition and media angles were inspected in the complete
[explicit-reading 22:00 set](captures/house-01052-family-sofa-night-final); the richer silhouette
and blue/charcoal finish remain readable under the room's practicals. All six matched family frames
were opened at full resolution. An initial darker tint was rejected because it collapsed toward
black under the real room lighting.

Ranked visible defects remaining:

1. The pale mantel-like media unit and its broad blank surround are now the coarsest family-room
   focal geometry, particularly against the new curved sofa and detailed bookcase.
2. Clear-day apertures remain clipped and overpower the room's interior surfaces.
3. The broad exterior/garage start frame is materially coherent but still sparse at player scale.
4. The two foreground armchairs and several other older assets remain flatter and more geometric
   under stock `BasicEffect` than the new sofa.
5. The selected route still needs restrained wall/surface dressing outside the kitchen and family
   room before it feels consistently inhabited.

Fixed: the family-only sofa now uses Wayfair's 4,196-triangle `GlamVelvetSofa`, pinned to its exact
Khronos Sample Assets URL and SHA-256 under CC BY 4.0. A deterministic offline preparation keeps
all visible geometry and UV0, selects the authored navy variant, recentres/grounds the source,
removes unsupported material-variant/PBR metadata and unused embedded maps/light, and appends a
twelve-triangle collision box. Three inspected source roles map to canonical velvet, dark frame
and steel finishes. The formal living-room sofa and every placement outside `L0_FAMILY` remain
unchanged.

The day family-room/composition/media pairs change 0.080%, 5.421% and 5.979% of pixels with
normalized MAE 0.000242, 0.006947 and 0.006400. Their night counterparts change 2.823%, 8.213%
and 7.272% with normalized MAE 0.000495, 0.005772 and 0.005580. These localized differences match
the sofa's visibility in each fixed camera rather than an exposure or global-palette edit.

The prepared GLB is 128,624 bytes and regenerates byte-identically from the pinned 3,149,844-byte
source. The world is 628 chunks / 57 props / 256 exterior hierarchy instances / 52.6681 MB. Only
`L0_FAMILY` rises from its exact 23-chunk exception to 25 because the prior two-role sofa is replaced
by three exact roles while the steel feet reuse an existing finish. The unculled diagnostic is 628
draws / 94 state changes, still far below §71.2's 1,400 / 210 row.

One strict production reference required intentional advancement: the sofa affects eye adaptation
through the open family-room sightline in `fp-l0-hall`, changing 745 tolerance-filtered pixels
(0.3234%, maximum channel delta 7). The before/actual pair and amplified difference were inspected;
only that reference was replaced. Unit tests pass 1409/1409, serial offscreen integration tests
pass 135/135, and the complete software-render suite passes with eight capture-only cases disabled.
The eighteen-pose culled-vs-unculled gate remains 0.1320% worst case (`l0-sunroom`). Compilation
and heavy work were capped to CPU 0-5 / six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only. Highest visible
value is now replacing or substantially improving the family media-unit silhouette, followed by
the sparse exterior start framing and clipped clear-day apertures—not unrelated infrastructure.

## Round 48 — low family media console

Commit: `HOUSE-01053` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 47's [front media view](captures/house-01052-family-sofa-day-final/family-media.png)
is dominated by a tall pale fireplace-shaped unit under the approved television. After: the
identical camera in the complete fourteen-view
[final clear-day set](captures/house-01053-family-media-console-day-final) shows a low walnut and
steel console with three readable drawer divisions and the television lowered to a measured
61 mm gap. The complete [explicit-reading 22:00 set](captures/house-01053-family-media-console-night-final)
confirms that the darker case remains readable under the room practicals. Both sets and their six
family-room frames were opened at full resolution. An initial uniform-scale iteration was rejected:
at 0.92 m high it still read as a chest. The accepted console is 0.654 m high.

Ranked visible defects remaining:

1. The green dog bed is now the coarsest family-room object and reads as a low geometric slab.
2. Clear-day apertures remain clipped and overpower several connected-room views.
3. The broad exterior/garage start frame is materially coherent but still sparse at player scale.
4. The two older family armchairs and coffee table remain flatter than the sofa, bookcase and new
   console.
5. Blank focal-wall bays and sparse purposeful surface detail still limit the inhabited feeling.

Fixed: SlykDrako's CC0 Bedroom cabinet is pinned at the raw upstream and decoded/node-selected
stages. Deterministic preparation retains its 780 visible triangles, normals and UV0, removes all
source images and unsupported metadata, transforms positions and normals into a 1.540 x 0.654 x
0.465 m low-console proportion, and maps exact wood/hardware roles to existing family walnut and
kitchen steel. A two-box, 24-triangle proxy covers the plinth and central 1.097 m case while the
wider end overhangs do not narrow circulation. Only `PROP_FAMILY_TV_UNIT` changes asset; the
approved television remains separate and all non-family furniture is unchanged.

Against Round 47, the day family-room/composition/media pairs change 7.300%, 0.043% and 5.253% of
pixels with normalized MAE 0.007810, 0.000101 and 0.005674. Their night counterparts change
7.248%, 3.273% and 6.723% with normalized MAE 0.005251, 0.000148 and 0.003871. Inspection confirms
that the focal-wall changes are the shorter console and lowered TV rather than a global exposure
or lighting adjustment.

The committed GLB is 59,260 bytes at SHA-256
`ffd8bf4e8accb70abfa20760bfc67a29058e5c73421e7fc758484da630e48338`. The world is 626 chunks /
57 props / 256 exterior hierarchy instances / 52.3695 MB. `L0_FAMILY` drops from its exact
25-chunk exception to 23 because the replacement reuses two existing room roles and removes the
old unit's unique finishes. The deliberately unculled diagnostic is 626 draws / 93 state changes.

The altered physical route exposed a false assumption in the long random-walk assertion: a body
descending through the exact authored `L0_STAIR_MAIN` to `B1_STAIR` horizontal portal can have its
feet below L0 while its centre remains in L0. The corrected assertion exempts only positions inside
that data-defined downward stair-well rectangle; undeclared floor holes still fail. Unit tests pass
1409/1409, serial offscreen integration tests pass 135/135 and all 49 active software-render cases
pass with eight capture-only cases disabled. No strict golden moves. The eighteen-pose
culled-vs-unculled gate remains 0.1320% worst case (`l0-sunroom`). Compilation and heavy tooling
were capped to CPU 0-5 / six workers; the complete repository gate accepts all 323 strict-XNA
translation units.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only. Highest local
visible value is replacing the coarse dog bed or older flat chairs, while the larger route-level
priorities are clipped daylight apertures and sparse exterior start framing—not unrelated hidden
infrastructure.

## Round 49 — finished family dog bed

Commit: `HOUSE-01054` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 48's [front media view](captures/house-01053-family-media-console-day-final/family-media.png)
ends in a green five-box slab beside the console. After: the identical camera in the complete
fourteen-view [final clear-day set](captures/house-01054-family-dog-bed-day-final) shows a rounded
warm bolster bed with an inset blue cushion, continuous sewn edges and six restrained tuft points.
The complete [explicit-reading 22:00 set](captures/house-01054-family-dog-bed-night-final) confirms
that the silhouette remains distinct under the room practicals. Both sets and all six matched
family frames were opened at full resolution.

Ranked visible defects remaining:

1. Clear-day apertures remain clipped and visually overpower the selected connected rooms.
2. The broad exterior/garage start framing remains materially coherent but sparse at player scale.
3. The two older family armchairs and coffee table are now the flattest local furniture assets.
4. Blank wall bays and restrained surface dressing still limit the inhabited feeling.
5. The media wall and dog-bed corner are readable but remain darker than their ceiling practicals
   suggest, especially at night.

Fixed: the stable `MODEL_FAMILY_DOG_BED`, prop placement and linked nav role are preserved. Its
deterministic project-authored generator replaces 540 triangles of simple beveled boxes with a
0.960 x 0.310 x 0.705 m, 5,508-triangle soft composition: rounded support, inset cushion,
U-shaped bolsters, two continuous fabric cords and six covered buttons. The new warm woven outer
finish replaces green dining upholstery; the cushion and narrow piping reuse existing family
velvet/canvas roles. This keeps the visual identities separate while adding only one material
chunk. There is still no collision proxy, so pet navigation and player circulation are unchanged.

Against Round 48, the day family-room/composition/media pairs change 2.334%, 0.052% and 1.949% of
pixels with normalized MAE 0.001244, 0.000112 and 0.001023. Their night counterparts change
2.284%, 4.571% and 7.423% with normalized MAE 0.000904, 0.000271 and 0.001019. The first capture
was rejected because it exposed stale executable-relative deployed content; the CMake content
target synchronized both the model and world before either final set was accepted.

The committed GLB is 343,260 bytes at SHA-256
`e2b7324af59c72db316af52f54018c8a05529c6633ff9a73b56b4b8aec2d329c`. The world is 627 chunks /
57 props / 256 exterior hierarchy instances / 52.6674 MB. Only `L0_FAMILY` rises from its exact
23-chunk boundary to 24, and the unculled diagnostic is 627 draws / 94 state changes. Licence,
budget and 2,864 stable ids are current.

Unit tests pass 1409/1409, serial offscreen integration tests pass 134/134 and all 48 active
software-render cases pass with eight capture-only cases disabled. No strict pixel golden moves.
The eighteen-pose culling comparison remains 0.1320% worst case (`l0-sunroom`). Compilation and
heavy work were capped to CPU 0-5 / six workers; the complete repository gate accepts all 323
strict-XNA translation units.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only. The largest
route-level gains now lie in daylight aperture balance and the exterior start composition; the
largest contained family-room gain is replacing or materially improving the older armchairs and
coffee table—not adding invisible infrastructure.

## Round 50 — finished shared route armchair

Commit: `HOUSE-01055` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 49's [day route](captures/house-01054-family-dog-bed-day-final) uses two broad pale
formal-living chairs and one flat family chair. After: the identical fourteen cameras in the
[final clear-day set](captures/house-01055-sheen-chair-day-final) and
[explicit-reading 22:00 set](captures/house-01055-sheen-chair-night-final) show one coherent
household chair with a warm mango/rust velvet seat, open brown timber frame and steel fasteners.
All changed route frames and two temporary close inspections were opened at full resolution.

Ranked visible defects remaining:

1. Clear-day apertures remain clipped and visually overpower the selected connected rooms.
2. The broad exterior/garage start framing remains materially coherent but sparse at player scale.
3. The old shared coffee tables are now the flattest repeated close-range furniture assets.
4. The formal living sofa remains an oversized grey block beside the now finer chair silhouette.
5. Blank wall bays and restrained surface dressing still limit the inhabited feeling.

Fixed: only `PROP_LIVING_CHAIR_GREEN`, `PROP_LIVING_CHAIR_PURPLE` and `PROP_FAMILY_CHAIR` change
asset; their stable ids, positions, yaws, scales and collision semantics stay fixed. The official
Wayfair/Khronos CC0 source is pinned independently. Deterministic preparation removes only the
branded 128-triangle label, unused embedded maps and unsupported sheen/variant metadata, retaining
39,808 visible triangles, UV0 and exact fabric/wood/metal roles. The accepted chair is grounded
and recentred at 0.826558 x 0.686247 x 0.570265 m with a twelve-triangle collision proxy.

Against Round 49, the day living-room/composition and family-room/composition pairs change 4.881%,
3.646%, 2.680% and 1.476% of pixels with normalized MAE 0.000163, 0.007690, 0.003461 and 0.001659.
Their night counterparts change 29.151%, 8.221%, 3.605% and 2.990% with normalized MAE 0.001231,
0.005960, 0.003311 and 0.001887. The broad night masks have very low error magnitude; inspection
confirms localized chair silhouettes plus subpixel lighting differences, not an exposure change.
The smaller family chair remains at the camera edge because moving it inward would compromise the
existing coffee-table clearance merely to improve one screenshot.

The committed GLB is 1,142,052 bytes at SHA-256
`02843a11e116875b95f7d707796e33eecf0ef5381a8fd60fd8a3243944254ae7`. The world is 626 chunks /
57 props / 256 exterior hierarchy instances / 53.566591 MB. `L0_FAMILY` remains at its exact
24-chunk boundary, `L0_LIVING` is 20 against its unchanged 21-chunk allowance, and the unculled
diagnostic is 626 draws / 93 state changes. Licence, budget and 2,866 stable ids are current.

Unit tests pass 1409/1409, serial offscreen integration tests pass 134/134 and all 49 active
software-render cases pass with eight capture-only cases disabled. The inspected debug-blockout
living golden advances only for the replacement silhouette; production first-person references
stay accepted. The culled/unculled comparison remains 0.1320% worst case (`l0-sunroom`). The full
gate accepts all 323 strict-XNA translation units with compilation and heavy work capped to CPU
0-5 / six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and blockout
colour remains available solely as an explicit diagnostic. The next contained furniture gain is
the shared coffee table or formal living sofa; the highest route-level gains remain daylight
aperture balance and a denser exterior start composition.

## Round 51 — finished shared coffee-table material

Commit: `HOUSE-01056` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 50's [day route](captures/house-01055-sheen-chair-day-final) shows the same broad
white low table in the formal living and family seating groups. After: the identical cameras in
the [final clear-day set](captures/house-01056-coffee-table-day-final) and
[explicit-reading 22:00 set](captures/house-01056-coffee-table-night-final) show a warm lacquered
hardwood case whose rounded top, drawers and tapered legs remain readable. All eight affected
living/family frames and both complete fourteen-frame routes were opened at full resolution.

Ranked visible defects remaining:

1. The unchanged straight living-room view is dark and nearly empty despite the furnished seating
   group just outside its lens.
2. The formal living sofa is now the coarsest pale block beside the finer chairs and wood table.
3. Clear-day apertures remain clipped and overpower the family composition.
4. The broad exterior/garage start framing remains materially coherent but sparse at player scale.
5. Blank wall bays and restrained surface dressing still limit the inhabited feeling.

Fixed: no geometry, placement or licence changed. The 680,908-byte White Room CC BY 3.0 model
remains pinned at SHA-256
`a7c48305a4dc657c5def55c1f476f5476e1be7512ae97a75712c594cbbb3533b`, with its 1.211165 x
0.419303 x 0.523951 m bounds, 22,744-triangle visible LOD0, lower LODs and `TableLegs_COL` proxy.
Its sole exact source role is now bound to the approved piano hardwood instead of cream/white-room
palette; the manifest correctly declares both `L0_LIVING` and `L0_FAMILY` users.

The first complete capture was rejected because ordinary CMake had not rebuilt the manifest-
dependent canonical chunks. The world content graph rebuilt `chunks.bin` in 8.86 seconds and the
deployment target synchronized it before final capture. Against Round 50, day living/family
composition frames change 1.886% and 2.277% of pixels with normalized MAE 0.002934 and 0.003148;
night counterparts change 4.546% and 2.481% with MAE 0.001802 and 0.002835. Direct views that do
not show a table retain very low MAE, confirming a localized material correction rather than an
exposure change.

The deterministic world remains 626 chunks / 57 props / 256 exterior hierarchy instances /
53.566591 MB; `L0_FAMILY` stays at its exact 24-chunk boundary and `L0_LIVING` remains 20 against
its 21-chunk allowance. Unit tests pass 1409/1409, serial offscreen integration completes all 134
cases without failure (126 pass, 8 intentional skips), and all 48 active software-render cases
pass with eight capture-only cases disabled. No strict reference moves. The eighteen-pose culling
comparison remains under its 0.2% limit at 0.1342% worst case (`l0-sunroom`). The complete gate is
green across all 323 strict-XNA translation units. Compilation and heavy work were capped to CPU
0-5 / six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and blockout
colour remains explicit diagnostic output. The next contained furniture gain is the formal living
sofa; the route-level priorities remain the dark/empty straight living view, clipped daylight
apertures and sparse exterior start composition.

## Round 52 — finished formal living sofa

Commit: `HOUSE-01057` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 51's [day living composition](captures/house-01056-coffee-table-day-final/living-composition.png)
has a broad pale sofa block in the close foreground. After: the identical fourteen cameras in the
[final clear-day set](captures/house-01057-formal-sofa-day-final) and
[explicit-reading 22:00 set](captures/house-01057-formal-sofa-night-final) show a lower, detailed
brown leather/paisley sofa with distinct cushions, carved frame and domestic 2.20 m width. Both
complete routes and temporary close inspections were opened at full resolution.

Ranked visible defects remaining:

1. The unchanged straight living-room camera remains dark and nearly empty around its piano wall.
2. Clear-day apertures still clip and overpower the family composition and several route views.
3. The broad exterior/garage start framing is materially coherent but sparse at player scale.
4. Blank wall bays and restrained surface dressing still limit the inhabited feeling.
5. The formal room's very dark ambient light suppresses some of the new sofa's carved detail.

Fixed: `PROP_LIVING_SOFA` alone changes asset; its id, position, yaw, scale and collision semantics
stay fixed. The official 10,107,912-byte Khronos source and CC BY 4.0 provenance are pinned. A
weld-before-decimate preparation avoids the holes made by direct simplification and produces a
466,772-byte, 9,382-triangle LOD0, 3,278-triangle LOD1 and twelve-triangle proxy at
2.200 x 0.901392 x 0.744769 m. Six exact source slots resolve to five authored base-colour roles.

The first gameplay capture was rejected for pale alpha-cutout stipple. A true blend experiment
was also rejected: the layered fringe planes accumulated into a bright continuous outline in the
strict-XNA transparent pass. The accepted dark 0.35 alpha cutout preserves the trim without either
artefact. Against Round 51, day/night living compositions change 5.640% / 5.688% of pixels with
normalized MAE 0.006516 / 0.004382. At a 1% colour threshold the unchanged straight living view
changes only 675 / 503 pixels, confirming a localized asset change rather than exposure drift.

The world is 630 chunks / 57 props / 256 exterior instances / 53.057268 MB, with `L0_LIVING` at
its exact 24-chunk exception and the unculled diagnostic at 630 draws / 96 state changes. Licence,
budget and 2,877 stable ids are current. Unit tests pass 1409/1409, serial offscreen integration
passes 134/134 and all 48 active software-render cases pass with eight capture-only cases disabled.
Only the inspected explicit blockout living reference advances. The eighteen-pose culling check
passes at 0.1342% worst case (`l0-sunroom`), and the full gate accepts all 323 strict-XNA units.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only; the next highest
visible gain is to make the straight living/piano wall read as a furnished, naturally lit room,
followed by aperture balance and the sparse exterior start composition.

## Round 53 — illuminated formal piano-wall vignette

Commit: `HOUSE-01058` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 52's [day straight-living view](captures/house-01057-formal-sofa-day-final/living-room.png)
shows a small upright piano against a broad, dark and almost empty wall. After: the same camera in
the [final clear-day set](captures/house-01058-piano-vignette-day-final) and
[explicit-reading 22:00 set](captures/house-01058-piano-vignette-night-final) shows a grounded
tufted bench, original framed blue artwork, physical brass picture light and localized warm pool.
All fourteen views in both routes and a temporary pitched close view of the bench were opened at
full resolution.

Ranked visible defects remaining:

1. Clear-day apertures, especially in the family room, still clip and overpower adjacent finishes.
2. The broad exterior/garage start is materially coherent but sparse; its night composition is
   especially dark and empty.
3. Blank wall bays and restrained surface dressing elsewhere still limit the inhabited feeling.
4. The formal room remains dark overall, and the older upright piano has a comparatively blocky
   finish beside the new detailed vignette.
5. Several close furniture/shell contacts still lack the local shadowing needed to feel fully
   grounded.

Fixed: three deterministic project-authored assets add a 1.000 m tufted bench with grounded proxy,
a 1.150 m layered artwork and a 0.680 m physical picture light without external provenance. Their
wood, leather, brass, canvas and warm-emissive roles reuse approved canonical materials. The
existing stable 450 lm piano source now links to the visible diffuser as a 2,400 K spot with a
3.20 m range, and only the `L0_LIVING` daylight/artificial atlases were rebaked at 256 samples.
The local piano-atlas peak rises from 0.0188 to 0.0996; the main atlas peak remains 10.5507.

The first route capture was rejected because source content had been rebuilt but the runtime's
build-world deployment was stale: it showed the new pool without the objects. A negative-X cone
experiment was also rejected because the pool fell beside the artwork. The accepted positive-X
and downward direction centres the light over the composition. Against Round 52, the exact day /
night straight views change 81.672% / 90.762% of pixels, with normalized MAE 0.024866 / 0.034780;
the large full-frame difference is the intended replacement of a broad default-on point bake with
a localized directional one, not a camera or exposure drift.

The world is 632 chunks / 60 props / 256 exterior instances / 53.377344 MB, and the unculled
diagnostic is 632 draws / 585 opaque submissions / 47 alpha cutouts / 96 state changes. Unit tests
pass 1409/1409; all 135 labelled serial offscreen integration cases and all 49 active software
render cases pass, with eight capture-only cases disabled. The inspected explicit
`blockout-l0-living` reference advances for the three new silhouettes. The selected daylight bake
also changes only 0.2313% of the `sun-season-01` frame around L0 windows; that single reference was
inspected and accepted. Production first-person references remain unchanged. All eighteen
culled/unculled poses pass at 0.1342% worst case (`l0-sunroom`), and the full gate accepts all 323
strict-XNA translation units. Compilation and heavy work used CPU 0-5 / at most six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest route-level value is clear-day aperture balance,
followed by the sparse exterior/garage start and additional restrained wall/surface dressing.

## Round 54 — clear-day window-frame balance

Commit: `HOUSE-01059` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 53's [family composition](captures/house-01058-piano-vignette-day-final/family-composition.png)
has a picture-window frame whose full outdoor sky plus full celestial key clips most of its pale
paint to blue-white. After: the identical camera in the complete fourteen-view
[clear-day set](captures/house-01059-window-balance-day-final/family-composition.png) retains the
frame's casing, sash depth and cool-sky response without changing the exterior view or lowering the
room's exposure. Full-resolution family, façade, kitchen, foyer and living views were inspected;
targeted overcast and 22:00 family captures confirm plausible grey and dark aperture states.

Ranked visible defects remaining:

1. The broad exterior/garage start remains materially coherent but sparse, especially at night.
2. Blank wall bays and restrained secondary dressing still limit the inhabited feeling.
3. Several furniture/shell contacts lack local shadowing and read slightly ungrounded.
4. The formal living room remains dark overall and its older upright piano is comparatively blocky.
5. Distant landscape/neighbour context still exposes the finite property presentation.

Fixed: explicit debug-blockout inspection identified `MAT_WINDOW_FRAME_WHITE`, not glass or indoor
trim. Production now keeps scene-referred exposure 1.0 and the full outdoor sky ambient, but applies
0.30 of the direct celestial key to that opaque window role alone. Other open-sky detail keeps 1.0;
indoor Basic detail keeps the existing `0.10 * roomDaylight * exposure` policy. The clear family
A/B changes 91,623 pixels above two channel levels (6.3627%, normalized MAE 0.012199). Of those
pixels, near-white values fall from 52,843 to 15. A no-window living control changes only 166
HUD/timing pixels, so no room-wide exposure or lighting change is being hidden.

Three production first-person references containing the affected frame were opened and
intentionally advanced: foyer stair, kitchen and master bedroom. The other strict references remain
unchanged. Tests and final gate results are recorded in `docs/handoff.md`; compilation and heavy
work stayed on CPU 0-5 / at most six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest visible value is denser, better-framed exterior
arrival composition, followed by restrained wall dressing and contact/grounding improvement.

## Round 55 — natural production-lawn balance

Commit: `HOUSE-00936` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 54's [front](captures/house-01059-window-balance-day-final/exterior-front.png) and
[garage](captures/house-01059-window-balance-day-final/garage-approach.png) views are dominated by
a large saturated lime-green lawn. After: the identical cameras in the complete fourteen-view
clear-day set *(capture removed)* show a restrained olive/earth-balanced
lawn while preserving the approved source texture. All fourteen frames plus targeted fixed
overcast and 22:00 exterior captures were inspected at full resolution.

Ranked visible defects remaining:

1. Foundation planting and the garage arrival are sparse, leaving broad lawn and wall areas with
   little authored depth or human-scale composition.
2. Blank interior wall bays and restrained surface dressing still limit the inhabited feeling.
3. Several furniture/shell contacts lack local shadowing and read slightly ungrounded.
4. The formal living room remains dark overall and its older upright piano is comparatively blocky.
5. Distant landscape/neighbour context still exposes the finite property presentation.

Fixed: `MAT_OUTDOOR_GRASS` remains a deterministic stock-XNA variant of the approved
`MAT_GROUND_LAWN` row, with the same real albedo, UV scale, geometry and render path. Only its tint
changes to `(0.82, 0.66, 0.88)`; exposure, daylight, vegetation, collision and explicit debug
blockout remain unchanged. The front frame changes 291,539 pixels above two levels (20.2458%,
normalized MAE 0.010875), moving the changed-pixel mean from RGB (69.950, 111.296, 35.359) to
(68.251, 80.095, 43.025). The garage frame changes 159,554 pixels (11.0801%, MAE 0.006063), moving
its changed-pixel mean from (70.631, 112.143, 35.720) to (68.832, 80.781, 43.511).

The first generator write exposed fourteen later-authored vegetation material rows inside the
outdoor generator's replacement markers. No data was lost: those rows were restored byte-for-byte
and moved after the END marker. The generator now proves that its block contains exactly its 18
declared ordered ids and rejects an injected unowned row. Two successive writes produce the same
file SHA-256. Five affected seasonal/exterior goldens were compared pairwise and intentionally
advanced; all other references remain unchanged.

The rebuilt world remains 632 chunks / 96 cells / 256 exterior hierarchy instances / 53.377344 MB.
All 1,409 unit, 135 integration and 48 active software-render tests pass, with eight capture-only
render cases disabled. All eighteen culling pairs pass at 0.1342% worst case (`l0-sunroom`), and
the complete gate accepts all 323 strict-XNA translation units. Compilation and heavy work stayed
on CPU 0-5 / at most six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay continues to use real production materials. The
next highest-value exterior change is authored foundation planting/mulch and arrival-scale detail,
especially around the otherwise empty garage frontage.

## Round 56 — composed foundation planting

Commit: `HOUSE-00937` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 55's front *(capture removed)* and
garage *(capture removed)* frames show the approved
plants as a thin continuous line, including vegetation below the raised porch and one shrub in the
driveway, with lawn running directly to the foundation. After: the complete fifteen-view
clear-day set *(capture removed)* adds two readable mulched beds
on the facade flanks and a fixed path-height review boundary. All fifteen frames and targeted fixed
overcast/22:00 front-path captures were opened at full resolution.

Ranked visible defects remaining:

1. The night facade outside the two entrance sconces becomes almost black and loses architectural
   depth; exterior fixture/light composition is now the largest arrival defect.
2. The broad garage wall remains empty and lacks human-scale planting or useful facade detail.
3. The approved foundation plant meshes are correctly placed but still sparse and comparatively
   low-detail at close range.
4. Blank interior wall bays and weak local furniture grounding still limit the inhabited feeling.
5. Distant landscape/neighbour context continues to expose the finite presentation boundary.

Fixed: canonical exterior data now distinguishes sloped `groundCovers` from navigable/flat paths.
Two `MAT_OUTDOOR_MULCH` bands follow the original drainage slope and stop at the porch and driveway.
The existing twelve foundation shrubs, ten gazanias and eight periwinkles are recomposed into those
two flanks; the world still owns exactly sixty `VEG_SHRUB` instances and no new or unproven asset.
The unchanged front frame changes 0.2554% of pixels above two levels (normalized MAE 0.0002760),
while the driveway frame changes 1.1909% (0.0010399). Family views without the affected exterior
remain exactly unchanged.

The extra terrain role raises only the measured `EXT_FRONTYARD_E` and `EXT_SIDEYARD_W` ceilings to
13 and 12 chunks respectively; regrouping the same plants merges three alpha-test batches, so the
whole world remains 632 chunks / 96 cells / 256 exterior instances and is 53.378992 MB. Twelve
strict references whose changed regions show only mulch/plant placement were inspected pairwise
and intentionally advanced; all other goldens remain unchanged. The unculled integration contract
is now 44 cutout batches and 97 state changes.

All 1,409 unit and 135 integration tests pass. The 48-case software-render suite is green after
selective reference review, with eight capture-only cases disabled; all eighteen culling pairs
remain below 0.2% at 0.1342% worst case (`l0-sunroom`). Full repository gate results are recorded
in `docs/handoff.md`. Compilation and heavy work stayed on CPU 0-5 / at most six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay continues to use production materials and explicit
debug blockout remains available. The next highest-value exterior work is believable night facade
illumination and a better composed garage frontage; inside, wall dressing and contact grounding
remain the strongest visible opportunities.

## Round 57 — layered porch arrival lighting

Commit: `HOUSE-00938` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: the complete fixed 22:00
night set *(capture removed)* shows only the two hot door lanterns;
the outer porch bays, soffit and facade flanks collapse almost completely into black. After: the
matched fifteen-camera night set *(capture removed)* adds two physical
warm semi-flush fixtures and broad overlapping pools, while the complete
clear-day set *(capture removed)* proves that their off-state opal bowls
remain readable instead of becoming black emissive discs. All 45 frames were opened, including the
four family-room views affected by the refined shared fixture.

Ranked visible defects remaining:

1. The upper facade and garage frontage still become broad near-black planes at night; the garage
   wall also remains empty and lacks human-scale composition by day.
2. The close front steps stay underlit relative to the porch deck and door.
3. The approved foundation plants are correctly composed but remain sparse and low-detail nearby.
4. Blank interior wall bays and weak local furniture grounding still limit the inhabited feeling.
5. Distant landscape/neighbour context continues to expose the finite presentation boundary.

Fixed: `LG_L0_PORCH_LANTERN` remains one automatic dusk circuit but now owns two measured 1,000 lm,
2,700 K point sources in the outer porch bays as well as the existing wall lanterns. Their fixed
spill is explicitly limited to `EXT_WALK`; their lightmaps reach only `L0_FOYER` plus the physically
adjacent living/stair facade owners. A per-source, offline-only 100 lm/radiant-watt calibration
keeps one physical source consistent across receiver products that retain different historical
global calibrations. The foyer porch atlas peaks at 11.1136; the new living/stair atlases peak at
0.1755 / 0.1749. Runtime lumens, global exposure and the manual path/garage circuits are unchanged.

The first 700 lm spot calibration was rejected after inspection because it exposed the fixture but
left the facade black. The reused model was then rejected once more in its original form because
its entire bowl was the switched emissive role and therefore became a black disk by day. The final
636-triangle asset separates an always-visible opal bowl from a smaller switched optical disc,
without changing its 0.42 × 0.18 × 0.42 m bounds. Against the baseline, the night front-path frame
changes 656,975 pixels above two levels (45.6233%, normalized MAE 0.005527). The day frame changes
1.0713% (MAE 0.001336); family composition changes 0.8158% by day and 0.8014% at night, confined to
the refined fixture silhouette.

Four strict references were opened pairwise and intentionally advanced: foyer stair (the promoted
receiver product), blockout front walk (the two new fixture silhouettes), and the two dark seasonal
front views (the intended dusk pools). Six references rewritten by the all-pose generators despite
remaining within tolerance were restored byte-for-byte. The world is 634 chunks / 96 cells / 62
props / 257 exterior instances / 53.446726 MB; the unculled diagnostic is 590 opaque submissions,
44 cutouts and 98 state changes. Test and gate results are recorded in `docs/handoff.md`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest visible value is garage-front composition plus
a controlled facade/step lighting layer, not a global night-exposure increase.

## Round 58 — automatic garage carriage lights

Commit: `HOUSE-00939` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 57's clear-day driveway *(capture removed)*
has a broad garage wall with only one small central utility pack, while the matching
normal 22:00 view *(capture removed)* is almost
entirely black because that 4,000 K work light is deliberately manual. After: the complete matched
fifteen-camera day *(capture removed)* and
night *(capture removed)* sets add two physical carriage lanterns
at the sectional-door jambs. All thirty final frames were opened; the fixed manual-flood-on
driveway control was opened separately.

Ranked visible defects remaining:

1. The upper facade and side wings remain broad dark planes at night, and the close front steps
   are underlit relative to the porch deck.
2. The garage wall has human-scale fixtures now but remains sparse by day; the automatic lanterns
   intentionally do not replace the manual flood's apron-lighting role.
3. The approved foundation plants are correctly composed but remain sparse and low-detail nearby.
4. Blank interior wall bays and weak local furniture grounding still limit the inhabited feeling.
5. Distant landscape/neighbour context continues to expose the finite presentation boundary.

Fixed: two instances of the existing approved measured porch lantern sit symmetrically at x 10.2
and 16.2 m. Their separate dusk group owns two 800 lm / 2,400 K / 5.5 m point sources, explicit
garage/front-east/side-east receivers and a selected-cell Tier-S `L0_GARAGE` atlas peaking at
3.2271. The short range leaves the broad driveway, yard and side fence dark; the existing central
3,000 lm utility flood remains manual and independently supplies the apron when requested. Global
exposure, porch lighting, renderer architecture and strict-XNA API are unchanged.

Against Round 57, the fixed night driveway changes 94,586 pixels above two channel levels
(6.5685%, normalized MAE 0.002168); the door, two warm fixtures and their local jamb pools account
for the change. The day driveway changes 2,505 pixels (0.1740%, MAE 0.000405), confined to the two
fixture silhouettes. The road-front frame changes 1.4577% by night and 0.0917% by day. A first
physical-fixture-only attempt was rejected after inspection because its 400 lm sources left the
door and facade black; that non-final capture set was removed rather than preserved as evidence.

Two strict references were opened pairwise and intentionally advanced: the debug driveway frame
adds exactly the two fixture silhouettes, and the winter 06:00 seasonal frame adds their automatic
warm pool. No unrelated golden moved. The world is 635 chunks / 96 cells / 64 props / 258 exterior
instances / 53.480631 MB; the unculled diagnostic is 591 opaque submissions, 44 cutouts and 98
state changes. Test and gate results are recorded in `docs/handoff.md`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest visible exterior value is a bounded front-step
and upper-facade lighting layer; inside, restrained wall dressing and contact grounding remain the
strongest opportunities.

## Round 59 — paired formal interior joinery

Commit: `HOUSE-00940` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 58's living-room view *(capture removed)*
ends at a broad timber rectangle because the shell centred one declared 860 mm leaf inside each
1.80 m pair opening. After: the complete matched sixteen-camera
day *(capture removed)* and
night *(capture removed)* sets show real paired joinery. The added
focused view *(capture removed)* makes the
opaque panel geometry directly reviewable. All 32 final frames and all six strict-reference
comparisons were opened.

Ranked visible defects remaining:

1. The formal living room remains too dark and unevenly balanced in both fixed scenarios; its
   bright furniture edges and piano light compete with broad near-black walls and floor.
2. Several large wall bays still lack restrained art or domestic detail, and weak contact shading
   leaves some furniture insufficiently grounded.
3. The upper facade and side wings remain broad dark planes at night; the garage frontage remains
   sparse by day despite its new human-scale fixtures.
4. Foundation planting is coherent but still sparse and low-detail at close range.
5. Distant landscape and neighbour context expose the finite presentation boundary.

Fixed: every canonical `D_DOUBLE` schedule width now means one leaf of its unchanged full portal.
The generator centres both leaves around an 8 mm meeting clearance, covers it with a shallow
astragal and gives both the working and dummy leaf two-sided lever/backplate hardware. Opaque pairs
use two raised panels per leaf with real 55 mm moulding; the living/office portal's existing
`translucent` semantic instead produces two framed panes per leaf. The darker panel role reuses the
already approved broad-board maps, so no new bitmap or licence entered the repository. Portal
ownership, collision, animation data, room palette assignments and renderer architecture are
unchanged.

Against Round 58, the day living-room view changes 54,171 pixels above two channel levels
(normalized MAE 0.001088), living composition 38,213 (0.004818), entrance/foyer 59,728
(0.001002), dining 18,244 (0.000412) and central hall 6,293 (0.000320). Six strict references were
advanced only after pairwise inspection localized their differences to direct views of the doors
or views through adjoining rooms/Juliet facade. The world is 648 chunks / 96 cells / 64 props /
258 exterior instances / 53.689714 MB; the unculled diagnostic is 604 opaque submissions, 44
cutouts and 99 state changes. Tests and gates are recorded in `docs/handoff.md`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest interior value is to rebalance formal-living
light and contact grounding without lifting global exposure or inventing a new renderer.

## Round 60 — physical formal-living practicals

Commit: `HOUSE-01286` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 59's day living view *(capture removed)*
and night living view *(capture removed)* leave the
piano, paired doors, broad walls and most of the floor nearly black around one small picture-light
pool. Merely forcing the old main group on produced four ceiling hot spots because its bare points
sat 20 mm below the ceiling. After: the complete matched sixteen-camera
[day](captures/house-01286-living-practicals-day-final) and
[night](captures/house-01286-living-practicals-night-final) sets show four real semi-flush
practicals and a broad warm room contribution. The four-frame
[off controls](captures/house-01286-living-practicals-controls-final) isolate that contribution
at both times. All 36 frames were inspected, with the living and adjoining views opened at full
resolution.

Ranked visible defects remaining:

1. The upper facade, garage apron and front yard still collapse into broad black planes at night;
   exterior arrival lighting is now the largest visible route defect.
2. The formal living room is readable but remains dark and brown-heavy, with weak local contact
   shading below its seating and tables.
3. Several large interior wall bays still lack restrained art or domestic detail.
4. The garage frontage and foundation planting remain sparse and comparatively low-detail by day.
5. Distant landscape and neighbour context continue to expose the finite presentation boundary.

Fixed: the four existing stable main sources now link to four reused, approved 420 x 180 x 420 mm
bronze/opal fixtures at their unchanged plan positions. Their optical origins move from y 3.28 m
to the real lower diffusers at y 3.12 m; each becomes a broad 72/140-degree downward spot emitting
1,200 lm at 3,000 K. The group starts on in the normal new-game state. The selected 256-sample
`L0_LIVING` rebake lowers the pathological main-atlas peak from 10.5507 to 0.1995 while increasing
mean irradiance from 0.0110 to 0.0323. Global exposure, renderer architecture, neighbouring room
state, collision and portal visibility are unchanged.

Against Round 59, the day living-room frame changes about 1.430 million pixels above two channel
levels (normalized MAE 0.02413) and living composition changes 1.309 million (0.03320). The night
counterparts change 1.430 million (0.02342) and 1.307 million (0.03831). In the current exact
off/on control, normalized MAE is 0.02548 / 0.03431 by day and 0.02568 / 0.04085 at night. The
foyer/hall cameras change only through genuine sight lines; central hall remains within 0.018% MAE.

Only the strict debug `blockout-l0-living` reference advances, after inspection showed exactly two
new ceiling-fixture silhouettes; generated exterior references were restored byte-for-byte. The
world is 651 chunks / 96 cells / 68 props / 258 exterior instances / 53.787279 MB. The unculled
diagnostic is 607 opaque submissions, 44 cutouts and 99 state changes. Test and gate results are
recorded in `docs/handoff.md`; compilation and heavy work stayed on CPU 0-5 / at most six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and the explicit
debug blockout remains available. The next highest visible value is bounded night illumination for
the front facade/steps and garage apron, followed by formal-living contact grounding and restrained
wall dressing.

## Round 61 — physical front-balcony lantern

Commit: `HOUSE-01287` checkpoint (`2026-09-17`; exact HEAD in `docs/handoff.md`).

Before: Round 60's [night road view](captures/house-01286-living-practicals-night-final/exterior-front.png)
shows the central upper facade and balcony door as a continuous black plane. The authored balcony
source was a bare point 2.15 m above the door head and 1.35 m in front of the facade, with no
physical fixture or useful receiver. After: the complete matched seventeen-camera
[day](captures/house-01287-balcony-lantern-day-final) and
[night](captures/house-01287-balcony-lantern-night-final) sets add a real over-door lantern and
bounded warm arrival pool. Three exact [off controls](captures/house-01287-balcony-lantern-controls-final)
isolate its contribution. All 37 frames were inspected; both close controls and both road frames
were opened at full resolution.

Ranked visible defects remaining:

1. Most of the upper and side facade remains a broad black plane at night; one correct balcony
   practical should not fake-light the entire mansion.
2. The garage apron, front yard and close steps remain underlit beyond their real local fixtures.
3. The formal living room is readable but still brown-heavy, with weak furniture contact shading.
4. Several broad interior wall bays need restrained art and domestic detail.
5. Foundation planting and distant neighbour context remain sparse at close range.

Fixed: `PROP_L1_BALCONY_FRONT_LANTERN` reuses the approved deterministic 330 x 607.5 x
310.5 mm bronze/opal fixture directly above the balcony door. Its stable source moves to the real
optical centre [0,6.075,-14.073], becomes a 600 lm / 2,700 K downward spot with a 60/100-degree
feather and starts on while retaining the existing manual switch. Only `L1_LANDING` receives the
foreign group. The new 256-sample atlas peaks at 2.9429 with mean 0.000702; its per-source
100 lm/W calibration is baked under the receiver's historical global 683 lm/W setting, preserving
the two unrelated landing groups and every L2 product.

Against Round 60, road-front normalized MAE is 0.000444 by day and 0.000478 at night; the path
view is 0.001786 / 0.001926. The exact current night off/on comparisons change 7,148 road pixels,
43,167 path pixels and 152,994 close pixels above two channel levels, with normalized MAE
0.000400 / 0.001910 / 0.005104 respectively. A first point-light attempt was rejected because it
produced a large orange rectangle; an intermediate bake was also rejected after audit found its
global 100 lm/W setting amplified unrelated L1 and L2 groups. Neither rejected output remains in
the repository.

The world is 653 chunks / 96 cells / 69 props / 260 exterior instances / 53.804232 MB. The
unculled diagnostic is 609 opaque, 44 cutout and 99 state changes. Six strict references were
selectively advanced after pairwise inspection; all 48 active render tests pass, all eighteen
culling pairs stay below 0.2% at 0.1338% worst, and full gate results are in `docs/handoff.md`.
Compilation and heavy work stayed on CPU 0-5 / at most six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest-value work is a physically bounded upper/side
facade layer or close front-step/yard readability, then formal-living contact grounding and wall
dressing—not a global night-exposure increase.

## Round 62 — physical front-walk bollards

Commit: `HOUSE-01288` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 61's [day path](captures/house-01287-balcony-lantern-day-final/front-path.png) and
[night path](captures/house-01287-balcony-lantern-night-final/front-path.png) contain four
authored sources but no source geometry, so the long gate-to-step walk reads as an empty strip and
loses its manually switched wayfinding layer in the normal starting state. After: the complete
matched eighteen-camera [day](captures/house-01288-path-bollards-day-final) and
[night](captures/house-01288-path-bollards-night-final) sets show four physical low fixtures; the
new [close approach](captures/house-01288-path-bollards-night-final/front-walk-bollards.png) keeps
their scale and walking clearance reviewable. Three exact
[off controls](captures/house-01288-path-bollards-controls-final) isolate the current sources.
All 39 final frames and the four affected strict-reference pairs were inspected.

Ranked visible defects remaining:

1. Most of the upper and side facade remains a broad black plane at night; the bounded arrival
   fixtures correctly do not fake-light the whole mansion.
2. Terrain between the guidance points and the garage apron remains dark; any improvement needs a
   physical source and receiver boundary rather than global exposure.
3. The formal living room is readable but still brown-heavy, with weak furniture contact shading.
4. Several broad interior wall bays need restrained art and domestic detail.
5. The garage frontage, foundation planting and distant neighbour context remain sparse by day.

Fixed: a deterministic project-authored 144-triangle bollard supplies a 200 mm anchored bronze
foot/frame and a separately switched opal chamber within 200 x 544 x 200 mm bounds. Four static,
collision-free instances retain the exact stable source positions, alternating at x = ±0.75 m
outside the clear lane. Their linked optical centres sit at y = 0.43 m; each becomes a restrained
180 lm / 2,700 K, 70/120-degree downward spot with 3.2 m range. The existing switch and persistence
group stay manual while the selected normal-play arrival begins on. No renderer, exposure,
portal/collision, dusk-automation or broad terrain-lighting rule changes.

Against Round 61, the fixed road/path frames change 3,679 / 162,679 pixels above two channel
levels by day (normalized MAE 0.000234 / 0.002518) and 2,958 / 162,406 at night
(0.000241 / 0.002500). Exact night off/on controls change 2,775 road, 162,673 path and 45,669
close-view pixels (0.000233 / 0.002606 / 0.000950 MAE). Day inspection confirms credible half-
metre scale and a clear route; night inspection shows four warm guidance points and slightly
clearer steps without lawn flood. The world is 655 chunks / 96 cells / 73 props / 262 exterior
instances / 53.842684 MB; the unculled diagnostic is 611 opaque, 44 cutout and 109 state changes.
Four strict debug/property references were selectively advanced after pairwise inspection; full
test and gate results are recorded in `docs/handoff.md`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest visible value is a measured physical layer for
the remaining black facade/garage approach or formal-living grounding and wall dressing—not a
global night-exposure increase.

## Round 63 — physical front-facade uplights

Commit: `HOUSE-01289` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 62's [night front](captures/house-01288-path-bollards-night-final/exterior-front.png)
has useful door, porch, garage and path layers, but most of the two upper storeys collapse into one
black plane. After: the complete matched eighteen-camera
[clear-day](captures/house-01289-facade-uplights-day-final) and
[normal-night](captures/house-01289-facade-uplights-night-final) sets add four physical landscape
fixtures and selected warm L1/L2 pools. The [night front](captures/house-01289-facade-uplights-night-final/exterior-front.png),
[front path](captures/house-01289-facade-uplights-night-final/front-path.png) and
[close route](captures/house-01289-facade-uplights-night-final/front-walk-bollards.png) show the
effect most clearly. All 36 final frames and five affected strict-reference pairs were opened.

Ranked visible defects remaining:

1. L2 above the bounded pools, the roof/dormer plane and both far side elevations remain dark at
   night; they need their own real source/receiver logic, not more uplight flux.
2. The garage/side-yard terrain and distant neighbour context still lose depth outside the
   physically bounded arrival layers.
3. The formal living room remains brown-heavy, with weak furniture contact and broad bare walls.
4. Foyer/hall wall bays still need restrained art and domestic secondary detail.
5. The simple facade massing and repetitive window rhythm remain more obvious than the fixtures by
   day, despite coherent production materials.

Fixed: a deterministic project-authored 232-triangle fixture supplies a 200 mm anchored bronze
foot/yoke and a separately emissive lens within 200 x 259 x 200 mm bounds. Four collision-free
static instances sit inside the two existing foundation beds. Their linked optical centres drive
the new dusk-owned `LG_EXT_FACADE_UPLIGHT`: 2,400 lm / 3,000 K upward 24/44-degree spots with 9 m
range. Each source names exactly one front L1 and one front L2 shell; eight selected 256-sample
atlases peak at 0.3271–0.3465 on L1 and 0.0640–0.1562 on L2. The 5 lm-per-radiant-watt value is a
per-source offline-render calibration, not claimed fixture efficacy, and global exposure and the
683 default remain unchanged.

Against Round 62, the night exterior-front/front-path/garage-approach/close-route frames change
73,623 / 210,482 / 132,547 / 810,668 pixels above two channel levels, with normalized MAE
0.001965 / 0.005070 / 0.003492 / 0.020155. Day deltas are 10,914 / 1,198 / 8,656 / 10,534 pixels
(0.001189 / 0.000168 / 0.000827 / 0.001032 MAE), preserving the clear-day composition while making
the small ground hardware reviewable. A 700 lm / 100 lm-per-watt first bake was rejected because
its roughly 0.005 L1 peak was visually absent. The final normal-night facade gains a restrained
warm vertical rhythm; the close view also exposes some broad cell-local lawn warmth, accepted for
now as bounded to the two foundation-yard cells rather than a global exposure change.

An exact `--light-off` frame is not represented as a control: the shared dusk controller
intentionally reasserts this automatic group's state every frame at 22:00. Round 62 is therefore
the honest matched before scene. The current 256-sample daylight rebake also replaces stale broad
white receiver atlases with window/shutter/cornice occlusion. Its full facade was inspected before
one old direct-sun pixel guard was narrowed from R > 100 to R > 40; the shaded texel is R=52 and
the dark display-sky range is R=18. Five seasonal/HUD references were then advanced selectively.

The world is 659 chunks / 96 cells / 77 props / 266 exterior instances / 53.915377 MB. The
unculled diagnostic is 615 opaque, 44 cutout and 99 state changes. All eighteen culling pairs stay
below 0.2%; full test and gate results are recorded in `docs/handoff.md`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest visible value is restrained formal-living
grounding/wall dressing or a separate physical upper/side-facade layer—not global night exposure
and not unrelated subsystem work.

## Round 64 — formal-living fireplace composition

Commit: `HOUSE-01060` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 63's [living composition](captures/house-01289-facade-uplights-day-final/living-composition.png)
ends the formal room in one uninterrupted floor-to-ceiling structural brick stack. After: the
complete matched eighteen-camera [clear-day](captures/house-01060-fireplace-day-final) and
[normal-night](captures/house-01060-fireplace-night-final) sets add a real room-side hearth,
surround, mantel, firebox/grate/logs and original over-mantel relief. The
[day composition](captures/house-01060-fireplace-day-final/living-composition.png) and
[night composition](captures/house-01060-fireplace-night-final/living-composition.png) show the
change directly. All 36 final frames were opened; the canonical exterior, entrance, hall, kitchen,
dining and family views show no new visible defect.

Ranked visible defects remaining:

1. Formal living remains too brown and dark overall; the large leather sofa and chairs have weak
   contact with the floor and one another despite the new focal point.
2. Broad formal-living, foyer and hall wall bays still need restrained art and domestic detail.
3. The roof/dormer and far side elevations still collapse outside the bounded facade pools at
   night; any improvement needs its own physical source and receiver boundary.
4. Garage/side-yard terrain and distant neighbour context still lose depth at night.
5. The simple facade massing and repetitive window rhythm remain conspicuous by day.

Fixed: a deterministic project-authored 1.780 x 2.325 x 0.570 m suite contains 5,204 visible
triangles and reuses approved marble, iron, walnut, canvas and brass roles. Its projecting body
aligns to the canonical chimney/appliance focus. A 12-triangle proxy covers only the 170 mm-high
hearth; a rejected full-height proxy trapped the wall-recovery test and does not survive. The
origin validator now expresses the truthful floor-and-wall registration, and the scale validator
bounds the complete surround. No renderer, exposure, lightmap, portal, room id or live-fire state
changed.

Against Round 63, the fixed living composition changes 57,874 day pixels and 58,396 night pixels
above two channel levels (4.0190% / 4.0553% of the frame; normalized MAE 0.011887 / 0.003575).
The new silhouette and depth read at both times without crowding the route. The world is 661 chunks
/ 96 cells / 78 props / 266 exterior instances / 54.249842 MB; the unculled diagnostic is 617
opaque, 44 cutout and 100 state changes. No strict golden moved, all eighteen culling pairs pass,
and full test/gate results are recorded in `docs/handoff.md`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest visible value is formal-living contact/colour
balance and restrained wall dressing, followed by foyer/hall detail—not unrelated subsystem work.

## Round 65 — formal-living woven wool rug

Commit: `HOUSE-01061` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 64's [living composition](captures/house-01060-fireplace-day-final/living-composition.png)
has a nearly black 3 m-scale charcoal field that merges the brown seating into the walnut floor.
After: the complete matched eighteen-camera
[clear-day](captures/house-01061-living-rug-day-final) and
[normal-night](captures/house-01061-living-rug-night-final) sets give only that placement a muted
oatmeal wool finish. The [day composition](captures/house-01061-living-rug-day-final/living-composition.png)
and [night composition](captures/house-01061-living-rug-night-final/living-composition.png) show
the changed grounding plane directly. All 36 final frames were opened; family, dining and every
route/exterior view retain their prior presentation.

Ranked visible defects remaining:

1. The large formal sofa remains a dark brown foreground mass with weak local contrast; restrained
   textile dressing or a physically placed table/lamp grouping is now higher value than more rug
   brightness.
2. Broad formal-living, foyer and hall wall bays still need restrained art and domestic detail.
3. The roof/dormer and far side elevations still collapse outside bounded night sources.
4. Garage/side-yard terrain and distant neighbour context still lose depth at night.
5. The simple facade massing and repetitive window rhythm remain conspicuous by day.

Fixed: `PROP_LIVING_RUG` keeps the approved 688-triangle real rug geometry, 1.34 placed scale,
14 mm pile and collision-free placement. A canonical material override supplies the existing
neutral fabric weave/normal at 7x textile scale, restrained oatmeal tint and low specularity. The
shared source and charcoal material still serve family and dining; no asset byte, download, light,
exposure, renderer path, room id or collision changed.

Against Round 64, the fixed living composition changes 61,285 day pixels and 61,310 night pixels
above two channel levels (4.2559% / 4.2576%; normalized MAE 0.002707 / 0.003098). This is a broad,
low-amplitude correction: the rug reads clearly without becoming cream, flat or luminous. The
world remains 661 chunks / 96 cells / 78 props / 266 exterior instances / 54.249842 MB; 196
materials produce 617 opaque, 44 cutout and 101 unculled state changes. No strict golden moved,
all eighteen culling pairs pass, and full test/gate results are recorded in `docs/handoff.md`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest visible value is restrained formal-living soft
dressing/table detail, followed by foyer/hall wall dressing—not unrelated subsystem work.

## Round 66 — formal-sofa draped wool throw

Commit: `HOUSE-01062` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 65's [day living composition](captures/house-01061-living-rug-day-final/living-composition.png)
leaves the closest sofa arm as one broad, nearly black brown foreground mass. After: the complete
matched eighteen-camera [clear-day](captures/house-01062-sofa-throw-day-final) and
[normal-night](captures/house-01062-sofa-throw-night-final) sets add a tailored pale-blue woven
throw over that arm. The [day composition](captures/house-01062-sofa-throw-day-final/living-composition.png)
and [night composition](captures/house-01062-sofa-throw-night-final/living-composition.png) show
the accepted third placement. Two earlier scratch captures were rejected for clipping into the
sofa and reading as a dark slab. All 36 final frames were opened; no other route view regressed.

Ranked visible defects remaining:

1. Broad formal-living, foyer and hall wall bays still need restrained art and domestic detail.
2. The formal conversation group needs controlled coffee/side-table objects rather than more
   large furniture or another broad colour correction.
3. The roof/dormer and far side elevations still collapse outside bounded night sources.
4. Garage/side-yard terrain and distant neighbour context still lose depth at night.
5. The simple facade massing and repetitive window rhythm remain conspicuous by day.

Fixed: the deterministic 1,460-triangle throw has a curved folded body, 12 mm thickness and nine
physical fringe cords. At 0.75 placement scale it follows the real camera-near arm, adds neither
collision nor light and copies no licensed sofa geometry. `MAT_LIVING_THROW_WOOL` reuses the
approved fabric weave/normal with a low-sheen pale-blue tint. Its permanent gate checks byte-for-
byte regeneration, source hash, dimensions, components, role and exact canonical placement.

Against Round 65, the fixed composition changes 76,006 day pixels and 75,861 night pixels above
two channel levels (5.2782% / 5.2681%; normalized MAE 0.004745 / 0.004006). The world is 662 chunks
/ 96 cells / 79 props / 266 exterior instances / 54.282701 MB; 197 materials produce 618 opaque,
44 cutout and 102 unculled state changes. The one affected explicit-debug blockout reference was
visually inspected and advanced; all eighteen culling pairs pass. Full gate results are in
`docs/handoff.md`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest visible value is restrained formal-living wall
art and table-scale lived-in detail, followed by foyer/hall wall dressing—not unrelated systems.

## Round 67 — formal-living wall and table composition

Commit: `HOUSE-01063` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 66's [living composition](captures/house-01062-sofa-throw-day-final/living-composition.png)
leaves the broad south-wall bay and the entire coffee-table top blank. After: the complete matched
eighteen-camera [clear-day](captures/house-01063-surface-dressing-day-final) and
[normal-night](captures/house-01063-surface-dressing-night-final) sets add a paired botanical relief
and one controlled books/tray/bowl vignette. The [day composition](captures/house-01063-surface-dressing-day-final/living-composition.png)
and [night composition](captures/house-01063-surface-dressing-night-final/living-composition.png)
show both surfaces directly. All 36 frames were opened at full resolution; no route, exterior or
neighboring room view gained a new defect.

Ranked visible defects remaining:

1. The foyer and long central hall still have broad blank wall bays and limited domestic identity.
2. Formal living remains dark and brown-heavy despite the improved focal, textile and surface
   layers; its older upright piano is now the most visibly coarse hero object in that room.
3. The roof/dormer and far side elevations still collapse outside bounded night sources.
4. Garage/side-yard terrain and distant-neighbour context still lose depth at night.
5. The simple facade massing and repetitive window rhythm remain conspicuous by day.

Fixed: a deterministic 3,448-triangle paired relief uses layered mats, blue canvases, walnut frames
and original brass botanical stems/leaves; a separate 2,016-triangle tabletop suite contains two
finite-thickness hardbacks, an elliptical brass tray and a low sculptural stone bowl. The art is
within 20 mm of the canonical south wall and the vignette sits on the coffee table's exact
1.019303 m top. Both placements are collision-free. Existing approved room roles are reused except
for one fine-cream non-lightmapped paper role; close prop UV0 is therefore not misrepresented as
shell UV2. No download, light, renderer, portal, exposure or runtime room branch was added.

Against Round 66, the fixed composition changes 114,830 day pixels and 143,585 night pixels above
two channel levels (7.9743% / 9.9712%; normalized MAE 0.006476 / 0.006010). The world is 664 chunks
/ 96 cells / 81 props / 266 exterior instances / 54.590303 MB; 198 materials produce 620 opaque,
44 cutout and 103 unculled state changes. No strict golden moved. All eighteen culling pairs pass at
0.0558% worst (`l0-sunroom`), and full test/gate results are recorded in `docs/handoff.md`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest visible value is foyer/hall wall and console
dressing, followed by the coarse formal piano or another screenshot-ranked route defect—not an
unrelated subsystem.

## Round 68 — foyer-to-hall arrival composition

Commit: `HOUSE-01064` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 67's [entrance](captures/house-01063-surface-dressing-day-final/entrance-foyer.png),
[central hall](captures/house-01063-surface-dressing-day-final/central-hall.png) and
[reverse foyer](captures/house-01063-surface-dressing-day-final/foyer-facing-front.png) leave a
long bare floor, two blank kitchen-portal flanks and an empty console. After: the complete matched
eighteen-camera [clear-day](captures/house-01064-arrival-dressing-day-final) and
[normal-night](captures/house-01064-arrival-dressing-night-final) sets show a warm bordered runner,
paired reliefs and a supported vase/photograph/key-tray group. All 36 frames were opened at full
resolution; no exterior, neighboring-room or established furnishing view gained a visible defect.

Ranked visible defects remaining:

1. The foyer and hall light pools still lack convincing physical ceiling fixtures, and their broad
   side walls remain under-dressed beyond this deliberately bounded terminal composition.
2. Formal living remains dark and brown-heavy; its older upright piano is now the coarsest close
   hero object along the furnished route.
3. The roof, dormers and far-side elevations still collapse outside bounded night sources.
4. Garage/side-yard terrain and distant-neighbour context lose depth at night.
5. The simple facade massing and repetitive window rhythm remain conspicuous by day.

Fixed: a deterministic 3,856-triangle runner owns a 14 mm woven body, raised ochre borders,
five repeated diamonds and physical fringe while preserving at least 1.20 m side clearance. A
2,624-triangle paired original relief registers within 20 mm of the real kitchen-end wall without
covering the portal. A 2,644-triangle console vignette sits on the existing table's exact
1.207883 m top and uses an explicitly generated smooth ceramic silhouette, branch, photograph,
tray and keys. All three props are collision-free. Existing approved roles are reused except for
one pale ceramic and two runner-wool stock-Basic roles. No download, light, exposure, renderer,
portal or runtime room branch was added.

Against Round 67, `central-hall` changes 93,952 day / 93,407 night pixels above two channel levels
(6.5244% / 6.4866%; normalized MAE 0.006916 / 0.006834), `entrance-foyer` changes 27,542 /
27,119 (1.9126% / 1.8833%; 0.002513 / 0.002200), and `foyer-facing-front` changes 22,947 /
23,160 (1.5935% / 1.6083%; 0.001144 / 0.001185). The world is 675 chunks / 96 cells / 84 props /
266 exterior instances / 55.119074 MB; 201 materials produce 631 opaque, 44 cutout and 106
unculled state changes. No strict golden moved. All eighteen culling pairs pass at 0.0558% worst
(`l0-sunroom`); complete test and gate results are recorded in `docs/handoff.md`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest visible value is physical foyer/hall ceiling
fixtures and a restrained side-wall/gallery layer, followed by the coarse formal piano or another
screenshot-ranked route defect—not an unrelated subsystem.

## Round 69 — physical foyer and hall ceiling fixtures

Commit: `HOUSE-01290` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 68's [entrance](captures/house-01064-arrival-dressing-day-final/entrance-foyer.png)
and [central hall](captures/house-01064-arrival-dressing-night-final/central-hall.png) show warm
light pools with no physical ceiling source. After: the complete matched eighteen-camera
[clear-day](captures/house-01290-arrival-fixtures-day-final) and
[normal-night](captures/house-01290-arrival-fixtures-night-final) sets show one foyer and two hall
semi-flush practicals with broad warm distribution. All 36 final frames were opened at full
resolution. A first default-calibration bake was rejected because it made the walls nearly black;
only the corrected selected-cell bake was retained.

Ranked visible defects remaining:

1. Broad foyer and hall side-wall bays remain under-dressed; the gallery/circulation identity is
   still weaker than the completed terminal wall and console composition.
2. Formal living remains dark and brown-heavy; its older upright piano is the coarsest close hero
   object along the furnished route.
3. The roof, dormers and far-side elevations still collapse outside bounded night sources.
4. Garage/side-yard terrain and distant-neighbour context lose depth at night.
5. The simple facade massing and repetitive window rhythm remain conspicuous by day.

Fixed: the approved 636-triangle semi-flush asset is reused at the three exact source positions.
Its 0.18 m body meets the 3.30 m ceiling and its opal optical slot sits at 3.12 m. The former bare
points are linked 2,700 K 72/140-degree downward spots on the same groups, switches and default-on
state. Source-local offline bake calibrations preserve the established useful peaks while removing
the singularity: foyer main is 0.4852 peak / 0.0686 mean and hall main is 0.5133 / 0.1155. Only
complete `L0_FOYER` and `L0_HALL` receiver products were promoted; no exposure, renderer, collision,
portal, navigation or unrelated-cell change was made.

Against Round 68, `entrance-foyer` changes 1,025,453 day / 1,026,530 night pixels above two
channel levels (71.2120% / 71.2868%; normalized MAE 0.066187 / 0.067866), `central-hall` changes
864,055 / 866,342 (60.0038% / 60.1626%; 0.051109 / 0.051378), `foyer-facing-front` changes
1,114,931 / 1,115,481 (77.4258% / 77.4640%; 0.065195 / 0.066741), and
`foyer-living-doors` changes 1,081,521 / 1,083,132 (75.1056% / 75.2175%; 0.070736 / 0.072324).
The world is 681 chunks / 96 cells / 87 props / 266 exterior instances / 55.192247 MB; 201
materials produce 637 opaque, 44 cutout and 106 unculled state changes. No strict golden moved.
All eighteen culling pairs pass at 0.0558% worst (`l0-sunroom`); full test/gate results are in
`docs/handoff.md`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest visible value is a restrained foyer/hall
side-wall gallery layer, followed by the coarse formal piano or another screenshot-ranked route
defect—not an unrelated subsystem.

## Round 70 — bounded central-hall family gallery

Commit: `HOUSE-01065` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 69's [entrance](captures/house-01290-arrival-fixtures-day-final/entrance-foyer.png)
and [central hall](captures/house-01290-arrival-fixtures-day-final/central-hall.png) leave both long
side-wall bays blank even though the end wall, floor and physical lighting are composed. After:
the expanded twenty-camera [clear-day](captures/house-01065-hall-gallery-day-final) and
[normal-night](captures/house-01065-hall-gallery-night-final) sets include direct
[west](captures/house-01065-hall-gallery-day-final/hall-gallery-west.png) and
[east](captures/house-01065-hall-gallery-day-final/hall-gallery-east.png) cross-hall views. All 40
frames were opened at full resolution; every one of the nine frames and both doorway boundaries
were inspected in both lighting states.

Ranked visible defects remaining:

1. Formal living remains dark and brown-heavy; its older upright piano is the coarsest close hero
   object along the furnished route.
2. The roof, dormers and far-side elevations still collapse outside bounded night sources.
3. Garage/side-yard terrain and distant-neighbour context lose depth at night.
4. The simple facade massing and repetitive window rhythm remain conspicuous by day.
5. The gallery imagery is deliberately abstract route-2 relief; character-consistent route-3
   imagery and the six stair-wall frames remain later, explicitly separate work.

Fixed: a deterministic 9,160-triangle west cluster composes five differently proportioned frames
over 1.880 x 1.510 m; a 6,848-triangle east cluster composes four non-mirrored frames over
1.686492 x 1.500 m. Every frame has a physical back, mat, image layer and original anonymous
geometric family relief. Both collision-free placements sit 20 mm from the actual walls, remain
inside the measured portal-free z interval and preserve the 1.2 m circulation route. Existing
approved walnut, paper, canvas and brass roles are reused. No new material, light, exposure,
renderer path, collision, download or identifiable/AI likeness was introduced.

Against Round 69, `entrance-foyer` changes 10,088 day / 13,164 night pixels above two channel
levels (0.7006% / 0.9142%; normalized RGB MAE 0.001221 / 0.001192), `central-hall` changes 30,967 /
30,605 (2.1505% / 2.1253%; 0.000628 / 0.000612), and `foyer-living-doors` changes 19,226 / 19,250
(1.3351% / 1.3368%; 0.002479 / 0.002544). The software-render run exposed references not advanced
with Round 69's physical fixtures; all changed current-output references were regenerated with the
official disabled cases and their amplified diffs inspected. Material hall changes show the new
gallery/fixtures, while tiny exterior and seasonal differences remain confined to the front-door
view into that same changed interior. The world is 681 chunks / 96 cells / 89 props / 266 exterior
instances / 56.263094 MB; 201 materials produce 637 opaque, 44 cutout and 106 unculled state
changes. All 48 active software-render tests pass and all eighteen culling pairs remain at 0.0558%
worst (`l0-sunroom`); complete test and gate results are in `docs/handoff.md`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest visible value is replacing or substantially
refining the coarse formal-living piano composition, followed by the next screenshot-ranked route
or exterior defect—not an unrelated subsystem.

## Round 71 — formal-living upright hero refinement

Commit: `HOUSE-01066` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 70's [living-room](captures/house-01065-hall-gallery-day-final/living-room.png)
shows the approved upright as one coarse reddish cabinet despite its correct placement, 88-key
layout and physical accent light. After: the expanded twenty-one-camera
[clear-day](captures/house-01066-piano-refinement-day-final) and
[normal-night](captures/house-01066-piano-refinement-night-final) sets add a fixed
[piano detail](captures/house-01066-piano-refinement-day-final/living-piano-detail.png) view.
All 42 frames were opened at full size and as complete contact sheets.

Ranked visible defects remaining:

1. The roof, dormers and far-side elevations still collapse outside bounded night sources.
2. Garage/side-yard terrain and distant-neighbour context lose depth at night.
3. The simple facade massing and repetitive window rhythm remain conspicuous by day.
4. Formal living remains deliberately dark and still has weak local furniture/floor contact away
   from the piano's bounded pool.
5. Several secondary surfaces remain sparse; the hall gallery still awaits its later fictional
   character-consistent route-3 imagery and six stair-wall frames.

Fixed: the same 1.485 x 1.240 x 0.7325 m placement now presents an 18,852-triangle charcoal
lacquer and walnut upright, retaining 52 white and 36 black independent keys while adding finer
bevelled joinery, physical brass hinges and anonymous plaque, grounded front casters, and an open
original abstract practice folio. Bounds, origin, proxy, bench, wall art, accent light, collision
and circulation are unchanged. Its fifth approved stock-XNA material role is the only new finish;
no third-party mesh or texture, brand, copied music, renderer path or exposure change was added.

Against Round 70, `living-room` changes 85,952 day / 86,360 night pixels above two channel levels
(5.9689% / 5.9972%; normalized RGB MAE 0.004986 / 0.005235), while the distant
`living-composition` remains effectively stable at 164 / 801 pixels (0.0114% / 0.0556%;
0.000038 / 0.000188). The only strict reference advanced is `blockout-l0-living`; its amplified
5,228-pixel / 2.2691% diff is confined to the piano silhouette. A four-pixel unrelated
`blockout-ext-road` regeneration was inspected and rejected. The world is 682 chunks / 96 cells /
89 props / 266 exterior instances / 56.735979 MB; 202 materials produce 638 opaque, 44 cutout and
107 unculled state changes. All 48 active software-render tests pass and all eighteen culling pairs
remain at 0.0558% worst (`l0-sunroom`); complete test and gate results are in `docs/handoff.md`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest visible value is the screenshot-ranked night
exterior collapse—especially roof/dormer and garage/side-yard readability—without raising global
exposure or leaving the slice for an unrelated subsystem.

## Round 72 — physically scaled painted clapboard

Commit: `HOUSE-00941` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 71's [front exterior](captures/house-01066-piano-refinement-day-final/exterior-front.png)
maps the generic orange `Wood095` bare-board grain across every principal siding field, producing
broad furniture-like bands on the largest architectural surface in the view. After: the complete
twenty-one-camera clear-day *(capture removed)* and
normal-night *(capture removed)* sets show pale painted horizontal
clapboard at six 167 mm courses per world metre. All 42 frames were inspected as complete contact
sheets, and the front, path and garage approaches were also opened directly.

Ranked visible defects remaining:

1. The roof, dormers and far-side elevations still collapse outside bounded night sources; the
   canonical 22:00 review has the Moon below the horizon, so a fake moon key is not an honest fix.
2. Garage/side-yard terrain and distant-neighbour context remain sparse and lose depth at night.
3. The simple facade massing and repetitive window rhythm remain conspicuous even though the
   material identity and scale are now coherent.
4. Formal living remains deliberately dark with weak furniture/floor contact away from the
   piano's bounded practical pool.
5. Several secondary rooms and exterior surfaces remain sparse beyond the connected slice.

Fixed: a deterministic 512 px project-authored albedo/normal pair supplies restrained paint
variation, a clean tile boundary and a matching +Y tangent-space lap normal. All three dry siding
roles, their wet derivatives and the shared outdoor siding role use it. The outdoor fence role is
explicitly rebased to the still-approved bare-board material so its pickets do not become siding.
Both sources have permanent manifest ids, complete Ms-PL provenance and explicit mipmapped CNA
content recipes. No geometry, UV, lightmap, exposure, renderer, portal, collision or debug-palette
change was made.

Against Round 71, `exterior-front`, `garage-approach` and `front-path` respectively change
146,872 / 249,086 / 245,052 day pixels above two channel levels (10.1994% / 17.2976% / 17.0175%;
normalized RGB MAE 0.012966 / 0.022354 / 0.021640). The same night views change 139,989 / 241,675
/ 231,696 pixels (9.7215% / 16.7830% / 16.0900%) at only 0.000835–0.001291 normalized MAE.
`entrance-foyer` and `living-composition` stay effectively stable at 0.0131–0.0456%. Four seasonal
exterior references changed 10.3255–12.0686%; their amplified masks were inspected, confined to
the siding-bearing house silhouette and advanced through the official generators. Explicit
blockout references did not move. The world remains 682 chunks / 96 cells / 89 props / 266
exterior instances / 56.735979 MB; 202 materials, 906 manifest rows and 2,958 stable ids still
produce 638 opaque submissions, 44 cutouts and 107 unculled state changes. All 48 active software-
render tests pass, and all eighteen culling pairs remain at 0.0558% worst (`l0-sunroom`).

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest-value checkpoint is honest night-exterior
readability for the roof/dormers or garage/side yard, followed by the daytime facade massing—not
an unrelated subsystem or a global exposure lift.

## Round 73 — selected-route four-panel joinery

Commit: `HOUSE-00942` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 72's central hall *(capture removed)*,
family media wall *(capture removed)* and
kitchen *(capture removed)* show the close single-leaf doors as
featureless painted slabs without hardware. After: the complete twenty-one-camera
clear-day *(capture removed)* and
normal-night *(capture removed)* sets show restrained four-panel
relief and physical lock-side steel levers. All 42 frames were inspected as complete contact
sheets; the three changed views and both strict first-person pairs were also opened directly.

Ranked visible defects remaining:

1. The roof, dormers and far-side elevations still collapse outside bounded night sources; the
   canonical 22:00 review has the Moon below the horizon, so a fake moon key is not an honest fix.
2. Garage/side-yard terrain and distant-neighbour context remain sparse and lose depth at night.
3. The simple facade massing and repetitive window rhythm remain conspicuous by day.
4. Formal living remains deliberately dark with weak furniture/floor contact away from the
   piano's bounded practical pool.
5. Several secondary rooms and exterior surfaces remain sparse beyond the connected slice.

Fixed: five explicitly selected painted single doors now carry one data-driven `four_panel`
joinery selector and the already approved brushed-steel kitchen-hardware material. Both leaf faces
receive two columns by two rows of raised moulding; a physical backplate and lever are mirrored
from the authored hinge onto the lock stile. Unselected leaves stay plain. The slab, aperture,
portal, collision, swing/hinge contract and lightmap receiver geometry do not move, and the shell
generator contains no room-id branch. Only dining, hall and pantry require one measured material
chunk each; family and kitchen reuse an already resident steel role.

Against Round 72, `central-hall` changes 8,308 day / 7,996 night pixels above two channel levels
(0.5769% / 0.5553%; normalized RGB MAE 0.000278 / 0.000213), `family-media` changes 3,117 / 4,150
(0.2165% / 0.2882%; 0.000108 / 0.000190), and `kitchen` changes 3,325 / 3,744
(0.2309% / 0.2600%; 0.000178 / 0.000196). Exterior and unrelated-room controls remain below
0.0002 MAE. The world is 688 chunks / 96 cells / 89 props / 266 exterior instances / 57.024370
MB; its 202 materials retain 638 opaque, 44 cutout and 107 unculled state changes. The two strict
references that directly face the changed hall leaf (`fp-l0-hall` and `fp-l0-hall-corner`) were
inspected old/new and deliberately advanced; the explicit blockout reference suite did not move.
All eighteen culling pairs remain below 0.2%, at 0.0558% worst (`l0-sunroom`). Full test and gate
results are in `docs/handoff.md`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest-value checkpoint is honest night-exterior
readability for the roof/dormers or garage/side yard, followed by daytime facade depth—not an
unrelated subsystem or global exposure lift.

## Round 74 — physically scaled asphalt shingles

Commit: `HOUSE-00943` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 73's garage approach *(capture removed)*
and front exterior *(capture removed)* show the
acknowledged ambientCG `Tiles140` square ceramic floor map stretched across every roof. After: the
complete twenty-one-camera clear-day *(capture removed)* and
normal-night *(capture removed)* sets use a project-authored charcoal
asphalt pair at seven exposed courses per true sloping metre. All 42 frames were inspected as
complete contact sheets; both exterior views and enlarged garage-roof crops were opened directly.

Ranked visible defects remaining:

1. The roof/dormer silhouette still collapses at 22:00 outside bounded practical sources; the Moon
   is below the horizon in this canonical review, so a fake moon key remains the wrong fix.
2. Main-roof and dormer edges still lack convincing ridge/hip caps, flashing and gutter depth even
   though the surface material and scale are now coherent.
3. Garage/side-yard terrain and distant-neighbour context remain sparse and lose depth at night.
4. The simple facade massing and repetitive window rhythm remain conspicuous by day.
5. Formal living remains deliberately dark with weak furniture/floor contact away from the
   piano's bounded practical pool.

Fixed: one deterministic 512 px albedo/linear-normal pair supplies seven approximately 143 mm
courses, four staggered tabs, short exposed slots and restrained aggregate variation. Stable dry,
wet and unbaked roof ids retain asphalt/weather/audio/effect semantics. Roof UV0 now uses a
horizontal contour axis and a true surface-slope axis rather than plan projection. The five
pre-existing twisted dormer-transition quads are explicitly split along the same rendered export
diagonal before UV assignment; positions, silhouette, collision and lightmaps do not move. A
first in-game pass was rejected because its cool 0.82/0.85/0.90 tint looked flat and blue. The
retained warm-neutral 0.68/0.66/0.64 tint reads as restrained charcoal under the clear-day sky.

Against Round 73, `exterior-front` changes 17,722 day / 17,635 night pixels above two channel
levels (1.2307% / 1.2247%; normalized RGB MAE 0.001026 / 0.000396), and `garage-approach` changes
21,278 / 20,678 (1.4776% / 1.4360%; 0.001266 / 0.000400). The path and balcony controls change
only 0.09–0.14% at night. The strict `fp-l3-room`, HUD and four seasonal exterior pairs were opened
old/new and intentionally advanced; explicit debug-blockout references remain unchanged. The
world stays at 688 chunks / 96 cells / 89 props / 266 exterior instances / 57.024981 MB. All 48
active software-render tests pass and all eighteen culling pairs remain below 0.2%, at 0.0558%
worst (`l0-sunroom`).

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next highest-value checkpoint is physical roof-edge detail
(ridge/hip caps, flashing and gutters) followed by honest bounded night-exterior depth around the
garage/side yard—not a global exposure lift or an unrelated subsystem.

## Round 75 — physical roof edges and rainwater goods

Commit: `HOUSE-00944` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 74's front exterior *(capture removed)*
and garage approach *(capture removed)* have the
correct shingle surface but uncapped hip/dormer seams, a black metal ridge-vent bar and gutters
whose generated boxes omit the outward longitudinal face. After: the complete twenty-one-camera
clear-day *(capture removed)* and
normal-night *(capture removed)* sets add physical finish derived from the
same authoritative planes and drainage coordinates. Both 21-frame contact sheets, four full-size
exteriors and enlarged before/after roof crops were opened.

Ranked visible defects remaining:

1. The canonical 22:00 garage approach and side yard are still nearly black outside the two small
   carriage pools; the driveway, fence and neighbour depth disappear.
2. The main roof/dormer silhouette remains honestly dark at 22:00 with the Moon below the horizon;
   a fake moon key or global exposure lift remains the wrong fix.
3. Garage/side-yard terrain and distant-neighbour context remain sparse even by day.
4. Facade massing and its repeated window rhythm remain simple at road distance.
5. Formal living retains weak furniture/floor contact away from the bounded piano practical.

Fixed: all eight principal/garage hips carry four-face 220 mm shingle caps; all five dormers carry
180 mm ridge caps and two 55 mm cheek-flashing ribbons. The existing continuous ridge vent keeps
its canonical size and ventilation semantics but finally uses the shingle finish specified by the
architecture. Each roof's four incomplete square strips become closed six-fold K-profile gutters;
the six downspouts become closed tubes without moving their heads or terrain splash/audio points.
The first capture was rejected because the existing gutter material's literal zero diffuse tint
turned the complete profile into an ink outline. A restrained 0.18/0.20/0.22 charcoal calibration
keeps black painted metal while allowing its folds to read.

Against Round 74, `exterior-front` changes 11,161 day / 10,400 night pixels above two channel
levels (0.7751% / 0.7222%; normalized RGB MAE 0.002110 / 0.000513), and `garage-approach` changes
18,378 / 16,721 (1.2763% / 1.1612%; 0.003279 / 0.000637). `front-path`, the close walk and balcony
controls remain 0.15--0.47%; `family-media` remains at 0.0606% day / 0.0506% night capture noise.
The seven affected exterior blockout poses, overview blockout, HUD, four property poses and four
season references were inspected before deliberate advancement; two bulk-generator interior
drifts of 63 pixels and one pixel were rejected and restored. Normal gameplay remains production
materials and the explicit diagnostic palette remains available.

`VISUAL-GATE-1` still **FAILS**. The next highest-value checkpoint is bounded, physically sourced
night depth around the garage/side yard plus enough day context to stop the wing reading in an
empty plane—not unrelated systems or a global exposure change.

## Round 76 — planted driveway edge and bounded dusk layer

Commit: `HOUSE-00945` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 75's garage approach *(capture removed)*
ends the asphalt directly in a broad empty grass/fence strip, while its matching
22:00 view *(capture removed)* loses the complete east
edge outside the two carriage-lantern pools. After: the complete twenty-one-camera
clear-day *(capture removed)* and
normal-night *(capture removed)* sets add a measured planted edge and
physical wayfinding layer. Both 21-frame contact sheets and the direct day/night garage images
were opened at full size.

Ranked visible defects remaining:

1. The main roof/dormer silhouette remains honestly dark at 22:00 with the Moon below the horizon;
   any next improvement needs a physical source rather than fake moonlight or global exposure.
2. Garage/facade massing and the repeated window rhythm remain simple at road distance even though
   the approach now has human-scale detail and night depth.
3. The driveway is still a large uninterrupted asphalt plane and distant-neighbour context is
   sparse by day.
4. Formal living retains weak furniture/floor contact outside its bounded piano practical.
5. Secondary rooms and exterior elevations outside the connected slice remain sparse.

Fixed: a 2.1 m-wide authored mulch strip carries five approved shrub instances with restrained
scale/yaw variation and three reused 0.20 x 0.544 x 0.20 m bronze bollards. Their distinct dusk
group drives three 2,700 K / 160 lm downward spots, owns only `EXT_SIDEYARD_E` and explicitly
spills only to the adjacent drive. The 3,000 lm garage flood remains a separate manual/default-off
circuit. The drive and side-yard circulation stay clear. Repeated geometry shares four new
material roles in one exact nine-chunk landscape-cell ceiling; no downloaded asset, broad ambient
lift, celestial change or renderer branch was added.

Against Round 75, `garage-approach` changes 24,169 day / 568,044 night pixels above two channel
levels (1.6784% / 39.4475%; normalized RGB MAE 0.001541 / 0.015645). The much larger night mask is
the intended newly readable east landscape/fence boundary, not an exposure change. Night
`exterior-front` changes 18,286 pixels (1.2699%, 0.000785 MAE); `entrance-foyer`, `central-hall`
and `family-media` controls remain at 0.0495--0.0687% day and 0.0487--0.0612% night. Ten strict
views with a visible line to the new border were deliberately advanced after old/new inspection:
the overview plus five exterior blockout poses, the kitchen's narrow exterior glimpse, two
property poses and `sun-season-03`. The unrelated one-pixel `blockout-l3-store-w` regeneration
was discarded.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next checkpoint should address the screenshot-ranked roof /
dormer night silhouette with a truthful bounded source, or the large daytime garage/facade massing
if no such source is dependency-valid—not a global exposure lift or unrelated subsystem.

## Round 77 — constructed driveway panels and stone inlays

Commit: `HOUSE-00946` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 76's day garage approach *(capture removed)*
has a planted/lit edge but the two-car concrete surface is still one broad uninterrupted grey
plane. After: the complete twenty-one-camera
clear-day *(capture removed)* and
normal-night *(capture removed)* sets add measured construction
scale. Both contact sheets and the direct garage day/night pairs were opened at full size.

Ranked visible defects remaining:

1. Garage/facade massing and repeated window rhythm remain simple despite the stronger foreground.
2. The main roof/dormer silhouette remains honestly dark at 22:00 with the Moon below the horizon;
   any improvement still needs a physical bounded source.
3. The property lacks a parked vehicle and richer distant-neighbour context, so the improved drive
   remains visually empty.
4. Formal living retains weak furniture/floor contact outside its bounded piano practical.
5. Secondary rooms and elevations outside the connected slice remain sparse.

Fixed: six 30 mm dark control-joint runs divide the canonical concrete drive at approximately
2.8 m centres. A 150 mm bluestone edge and two 300 mm transverse bands reuse the same material
family as the front walk. Twelve authored strips compile into one asphalt-finish and one
bluestone-finish role for `EXT_SIDEYARD_E`; both use the terrain tile's existing lightmap island.
They are render-only path-bound detail, so height, material index, concrete footsteps, collision,
navigation and circulation remain unchanged. The first capture with only fine joints was rejected
because the lines disappeared into the concrete texture at review distance.

Against Round 76, `garage-approach` changes 20,771 day / 16,265 night pixels above two channel
levels (1.4424% / 1.1295%; normalized RGB MAE 0.000932 / 0.000317). Day now reads as bounded,
buildable panels; night changes are subdued and confined to existing carriage/bollard reach rather
than emissive stone or a new light. `entrance-foyer`, `central-hall` and `family-media` controls
remain at 0.0220--0.0472% day and 0.0467--0.0749% night. Nine strict views with a visible line to
the drive were deliberately advanced after inspection; the unrelated one-pixel attic regeneration
was discarded.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next checkpoint should improve garage/facade massing or add
a dependency-valid physical roof/dormer source—not global exposure or an unrelated subsystem.

## Round 78 — sectional-door glazing and operating hardware

Commit: `HOUSE-00947` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 77's day garage approach *(capture removed)*
and matching 22:00 view *(capture removed)* show a
materially coherent arrival whose largest facade object is still one dark, monotonous sectional
leaf. After: the complete twenty-one-camera
clear-day *(capture removed)* and
normal-night *(capture removed)* sets add four framed top lites and a
physical centre pull. Both 21-frame contact sheets and the direct garage pairs were inspected at
full size.

Ranked visible defects remaining:

1. Garage/facade massing and repeated window rhythm remain simple beyond the improved door detail.
2. The property lacks a parked vehicle and richer distant-neighbour context, leaving the finished
   driveway composition visually empty.
3. The main roof/dormer silhouette remains honestly dark at 22:00 while the Moon is below the
   horizon; any change still needs a bounded physical source.
4. Formal living retains weak furniture/floor contact outside its bounded piano practical.
5. Secondary rooms and elevations outside the connected slice remain sparse.

Fixed: `DOOR_GARAGE_SECTIONAL` explicitly selects `top_lites`, clear glass and the approved bronze
hardware finish. Its top raised-panel row becomes four 50 mm framed glass lites; the lower sixteen
raised panels remain unchanged. A 300 mm pull and two 40 x 85 mm mounts appear at a plausible
680 mm sill height on both visible faces. The lites share the garage's existing exterior-window
glass chunk, so only the new hardware role raises the exact `L0_GARAGE` ceiling, from ten to eleven.
The 4.86 x 2.35 m leaf, five-section contract, aperture, portal, collision, navigation and receiver
lightmaps do not move. No material or model was downloaded.

Against Round 77, `garage-approach` changes 5,290 day / 5,635 night pixels above two channel
levels (0.3674% / 0.3913%; normalized RGB MAE 0.000280 / 0.000676). The day view gains cool panes
and a readable upper rhythm instead of another painted band. At night the same details remain
subdued under existing carriage/bollard light and do not glow. Other review views stay at roughly
0.01--0.07% changed pixels. No strict golden changed; all 48 active render tests pass and all
eighteen culled/unculled pairs remain below 0.2%, at 0.0558% worst (`l0-sunroom`).

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next checkpoint should add the next largest truthful facade
or arrival cue—most likely a dependency-valid parked vehicle or stronger garage-wing massing and
neighbour context—not a global exposure lift or unrelated subsystem.

## Round 79 — layered garage-opening surround

Commit: `HOUSE-00948` checkpoint (`2026-09-18`; exact HEAD in `docs/handoff.md`).

Before: Round 78's day garage approach *(capture removed)*
and matching 22:00 view *(capture removed)* show a
finished leaf still cut directly into a broad flat siding plane. After: the complete twenty-one-
camera clear-day *(capture removed)* and
normal-night *(capture removed)* sets add a layered painted opening
surround. Both contact sheets, both direct garage images and all changed strict references were
inspected at full size.

Ranked visible defects remaining:

1. The broad upper garage wall and repeated facade/window rhythm remain simple beyond the new
   grounded opening edge.
2. The driveway still lacks a parked vehicle and the distant neighbour context is thin, leaving
   the otherwise coherent arrival composition visually empty.
3. The main roof/dormer silhouette remains honestly dark at 22:00 while the Moon is below the
   horizon; any change still needs a bounded physical source.
4. Formal living retains weak furniture/floor contact outside its bounded piano practical.
5. Secondary rooms and elevations outside the connected slice remain sparse.

Fixed: `DOOR_GARAGE_SECTIONAL` now explicitly selects a `colonial` surround in the approved
exterior-white frame finish. Two 240 mm pilasters rise from 300 mm plinths to 320 mm capitals; a
300 mm frieze and 120 mm projecting crown close the head. All eight closed pieces embed 6 mm into
the settled exterior skin and project only 55–95 mm, avoiding a floating applique or coplanar
z-fight. They share the existing `window_frame` material role, so `L0_GARAGE` remains exactly
eleven chunks. The aperture, five-section leaf, portal, collision, navigation and receiver
lightmaps are unchanged; no asset, material or renderer branch was added.

Against Round 78, `garage-approach` changes 18,549 day / 17,934 night pixels above two channel
levels (1.2881% / 1.2454%; normalized RGB MAE 0.001762 / 0.000299). Day now has a coherent pale
architectural frame and layered shadow edge around the dark door. At night the surround remains
subdued beneath the existing carriage lamps rather than glowing. The `entrance-foyer`,
`central-hall` and `family-media` controls remain below 0.00014 normalized MAE. Ten strict
references with a real line of sight to the surround were advanced after old/new inspection; an
unrelated one-pixel `blockout-l3-store-w` regeneration was discarded.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next checkpoint should reduce the broad upper garage-wing
mass or add another truthful facade-scale cue; the already-authored parked car is not useful until
its neighbourhood/prop runtime path actually renders it. Do not substitute global exposure or an
unrelated subsystem.

## Round 80 — measured open family-room curtains

Commit: `HOUSE-01067` checkpoint (`2026-09-19`; exact HEAD in `docs/handoff.md`).

Before: Round 79's day family composition *(capture removed)*
and matching 22:00 view *(capture removed)*
show an otherwise furnished and physically lit family room whose two dominant 2.40 x 1.60 m
picture windows remain bare framed apertures. After: the complete twenty-one-camera
[clear-day](captures/house-01067-family-curtains-day-final) and
[normal-night](captures/house-01067-family-curtains-night-final) sets add one measured treatment
reused on both real wall planes. Both contact sheets and the family composition/media/full-room
views were opened at full size.

Ranked visible defects remaining:

1. The family-room windows now read domestically, but their exterior view remains a simple lawn,
   fence and neighbouring mass with little depth or planting composition.
2. The broad upper garage wall and repeated facade/window rhythm remain simple beyond the finished
   opening, roof and driveway layers.
3. Formal living still has weak furniture/floor contact outside its bounded piano practical,
   especially in the room-wide night composition.
4. Secondary rooms and elevations outside the connected slice remain sparse.
5. Several interior hero furnishings still have simpler source geometry/material response than
   the newly finished architectural and textile context around them.

Fixed: one deterministic 3.040 x 2.463002 x 0.153991 m / 6,084-triangle authored GLB supplies
solidified seven-fold panels, gathered waists, ten physical header tabs, a 3.04 m rod, finials,
wall brackets and restrained fabric-covered holdback rosettes. The two static collision-free
placements register to the north/east picture-window wall and floor planes. A 2.06 m open centre
preserves each view. Oatmeal wool-linen uses the approved weave maps at real textile scale; all
hardware shares existing steel. Two rejected iterations corrected panels that were too narrow and
dark, then replaced a straight edge-on tieback that read as a projecting stick. This is static
open dressing only; phase 45 still owns interactive blinds, transmission and motion.

Against Round 79, `family-composition` changes 55,634 day / 55,814 night pixels above two channel
levels (3.8635% / 3.8760%; normalized RGB MAE 0.013778 / 0.003049). `family-media` changes only
4,059 / 4,373 pixels (0.2819% / 0.3037%; 0.000669 / 0.000262), as expected from its nearly
orthogonal glimpse. No strict golden changed; all 48 active software-render tests and all eighteen
culled/unculled pairs pass, with the latter still 0.0558% worst at `l0-sunroom`.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next checkpoint should address the screenshot-ranked simple
family exterior view or another large truthful route defect—not interactive-curtain architecture,
global exposure or an unrelated subsystem.

## Round 81 — inhabited rear terrace and lawn

Commit: `HOUSE-00771` checkpoint (`2026-09-19`; exact HEAD in `docs/handoff.md`).

Before: the four retained selected baseline views *(capture removed)*
show the rear terrace and lawn as empty grass/paving between shrubs. After: the complete
twenty-four-camera clear-day *(capture removed)* and
normal-night *(capture removed)* sets add one coherent garden
suite and three fixed reciprocal review cameras. Both contact sheets and the direct rear-terrace,
backyard-to-house and family-garden pairs were opened at full size.

Ranked visible defects remaining:

1. At 22:00 the terrace surface and new furniture are almost black; the two nominal terrace
   sources do not create a readable physical pool in the retained rear views.
2. The broad rear elevation, especially the open dark slider bay and long balcony rail, remains
   architecturally simple around the now-inhabited foreground.
3. The dining group and loungers are coherent and correctly scaled, but their source geometry and
   material response remain simpler than the strongest finished interior hero assets.
4. Formal living retains weak furniture/floor contact outside its bounded piano practical.
5. Secondary rooms, side elevations and distant-neighbour context remain sparse.

Fixed: six deterministic project-authored GLBs supply a four-seat dining group, two separately
angled loungers, one swing bench, one stone fire pit, one birdbath and four planted pots. Ten exact
canonical placements preserve the slider/step circulation route and reuse six approved outdoor
finish families. Solid components have generated named collision proxies; foliage remains a
separate cutout batch. No asset download, runtime renderer branch or new material was needed.

The day family-window view now has an obvious inhabited focal layer instead of lawn and fence, and
the reciprocal lawn view reads as a usable terrace composition. The night review deliberately
does not hide the next defect: foliage catches the existing exterior contribution while the
furniture and paving remain nearly black. The world measures 705 chunks / 104 props / 282 exterior
instances / 59.068129 MB; exact `EXT_BACKYARD` and `EXT_TERRACE` chunk exceptions preserve the
collidable and cutout subranges. Six strict references with a real line of sight were inspected
old/new and intentionally advanced: blockout east/northeast/north/west change 0.4301% / 0.6706% /
0.2174% / 0.2235%, while the near property terrace and orchard views change 3.4171% / 2.9787%.
Their differences are confined to the approved furniture silhouettes; unrelated references were
not regenerated.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next checkpoint should make the already-authored rear
lighting physical and useful, or fix the precise source/receiver defect if those sources are not
reaching the terrace—not lift global exposure or leave the vertical slice for unrelated work.

## Round 82 — physical rear-terrace dusk lanterns

Commit: `HOUSE-01291` checkpoint (`2026-09-19`; exact HEAD in `docs/handoff.md`).

Before: Round 81's complete clear-day *(capture removed)* and
normal-night *(capture removed)* sets show an inhabited rear
terrace whose furniture and paving disappear at 22:00. After: the complete twenty-four-camera
[clear-day](captures/house-01291-terrace-lanterns-day-final) and
[normal-night](captures/house-01291-terrace-lanterns-night-final) sets add two physical wall
lanterns and their bounded contribution. Both contact sheets and the direct `rear-terrace`,
`backyard-to-house` and `family-garden-view` pairs were opened at full size.

Ranked visible defects remaining:

1. The open slider and `L0_SUNROOM` volume behind the terrace remain black and visibly empty at
   night; this is now the largest continuous-route defect.
2. The broad upper rear elevation and long balcony rail remain architecturally simple outside the
   lanterns' deliberately local reach.
3. The garden furniture is correctly scaled and composed, but its geometry/material response is
   simpler than the strongest finished interior hero assets.
4. Formal living retains weak furniture/floor contact outside its bounded piano practical.
5. Secondary rooms, side elevations and distant-neighbour context remain sparse.

Fixed: two reused approved 232-triangle bronze/opal lanterns sit at x = ±2.50 m on the rear
sunroom wall. Their linked optical centres drive 1,600 lm / 2,700 K / 8.5 m point sources under the
one dusk-sensor owner. A selected cross-cell bake reaches only the `L0_SUNROOM` outer skin and an
explicit unbaked-detail spill reaches only `EXT_BACKYARD`; the obsolete terrace wall plate is
removed. A new C++/Python validation rule prevents any automatic dusk group from also acquiring a
wall switch. No exposure, renderer, collision, navigation, opening or material architecture
changed.

Against Round 81, `rear-terrace` changes 194 day / 387,605 night pixels above two channel levels
(0.013% / 26.917%; normalized RGB MAE 0.000034 / 0.010901), `backyard-to-house` changes 617 /
75,708 (0.043% / 5.257%; 0.000105 / 0.002194), and `family-garden-view` changes 300 / 11,185
(0.021% / 0.777%; 0.000055 / 0.000451). Day retains the established composition with small
credible fixture bodies and no orange glow. Night gains visible warm emitters, a restrained pool
over the terrace and more legible furniture without bleaching the lawn or facade. A wall-facing
spot iteration left the terrace black; a 900 lm/high-efficacy point iteration produced tiny hot
patches and daylight glow. Both were rejected before the retained set.

The world is 707 chunks / 106 props / 284 exterior instances / 59.102034 MB. The selected
`L0_SUNROOM` foreign atlas peaks at 0.778957 with mean 0.000364078. The only strict reference that
advances is the inspected explicit-debug `property-terrace` frame, where the physical lantern is
now present; the other seven property frames were not regenerated. All eighteen culled/unculled
pairs remain below 0.2%, at 0.0558% worst (`l0-sunroom`).

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next checkpoint should furnish and light the visible
`L0_SUNROOM`/slider volume or otherwise fix that specific black opening—not raise global exposure,
spread the rear lantern bake across unrelated cells or leave the vertical slice.

## Round 83 — furnished and physically lit sunroom

Commit: `HOUSE-01068` checkpoint (`2026-09-19`; exact HEAD in `docs/handoff.md`).

Before: the four retained [selected controls](captures/house-01068-sunroom-before-selected) show
an empty cold shell by day and either a black room or small white ceiling singularities when the
old nominal main group is forced on. After: the six retained
[selected results](captures/house-01068-sunroom-after-selected), complete 26-camera
[clear-day](captures/house-01068-sunroom-day-r1) and
[normal-night](captures/house-01068-sunroom-night-r1) sets add the room composition and two fixed
reciprocal review cameras. Both contact sheets and the direct breakfast, wet-bar, rear-terrace,
kitchen and family boundary views were opened at full size.

Ranked visible defects remaining:

1. The broad pale closed slider leaf reads as an opaque slab rather than a credible glazed rear
   door; it is the largest newly exposed defect in both direct sunroom views.
2. The rear exterior remains very dark beyond the deliberately bounded terrace lanterns, while
   foliage still catches disproportionately strong highlights.
3. The broad upper rear elevation and balcony rail remain architecturally simple.
4. Formal living retains weak furniture/floor contact outside its bounded piano practical.
5. Secondary rooms, side elevations and distant-neighbour context remain sparse.

Fixed: a deterministic 4,324-triangle round-oak four-seat wicker breakfast group includes woven
chair backs, cushions, plates, mugs and fruit; a separate 4,208-triangle fitted wet bar includes
shaker fronts, stone worktop, tiled upstand, sink/faucet, shelves, carafe, tumblers, bottles and
bowls. Two reused plants finish the south corners. Four approved linked semi-flush fixtures replace
the old point singularities with broad 1,200 lm / 3,000 K spots, and two linked 350 lm task pucks
give the bar a local pool. Selected 100 lm-calibrated artificial atlases bind only the sunroom
main/bar groups. The manual two-gang owner, exterior dusk circuit, portals, daylight and renderer
architecture remain intact.

Against the exact retained yaw-0 control, the finished normal-play room changes 1,265,751 day /
1,294,367 night pixels above two channel levels (87.8994% / 89.8866%; normalized RGB MAE
0.089862 / 0.041061). This is intentionally a composition-scale difference: the formerly empty
room now has visible physical fixtures, furniture and a different valid selected light state. Day
reads as an inhabited breakfast/bar room and night has warm layered depth without a global
exposure lift. Three strict references with a genuine line of sight were inspected and advanced:
`blockout-l0-kitchen` changes 4,923 pixels / 2.1367%, `fp-l0-hall` 9,888 / 4.2917% and
`fp-l0-kitchen` 49,415 / 21.4475%. Their amplified differences contain only the newly visible
breakfast silhouette, physical fixtures and valid selected-room lighting; no other golden moved.

The world is 720 chunks / 116 props / 59.826849 MB. `L0_SUNROOM` is exactly 20 chunks / 19
materials; collision is 1,654 shapes and navigation completes in seconds at 893 nodes / 3,954
edges with the authored terrace and kitchen links retained. Permanent validation pins both GLBs'
byte regeneration, hashes, bounds, triangles, UV0, roles, components, scale/origin, proxy names,
canonical placements, fixture optics and review cameras.

All 1,409 unit, 135 integration and 48 active software-render tests pass; eight capture-only
generators remain disabled. All eighteen culled/unculled pairs remain below 0.2%, at 0.0558% worst
(`l0-sunroom`). The 31-stage content graph, budget, provenance/licensing and all strict-XNA/static
gates pass with heavy work restricted to CPU 0-5.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay remains production-material only and explicit
debug blockout remains available. The next checkpoint should fix the visibly opaque rear slider
and its glass/frame composition—not lift exposure, redesign the renderer or leave the connected
route for unrelated work.

## Round 84 — real glazing in both patio sliders

Commit: `HOUSE-00949` checkpoint (`2026-09-19`; exact HEAD in `docs/handoff.md`).

Before: Round 83's complete 26-camera [clear-day](captures/house-01068-sunroom-day-r1) and
[normal-night](captures/house-01068-sunroom-night-r1) sets show the sunroom rear opening as a
broad opaque pale/brown slab. After: matched 26-camera
clear-day *(capture removed)* and
normal-night *(capture removed)* sets. Both contact sheets and direct
`sunroom-breakfast`, `backyard-to-house` and `rear-terrace` pairs were opened at full size.

Ranked visible defects remaining:

1. The garden beyond the bounded terrace lantern circuit is almost black at 22:00, while nearby
   foliage catches disproportionately bright points; a physical night-depth solution is needed.
2. The broad upper rear elevation and long balcony rail remain simple, and the flat distant
   fence/lawn is conspicuous through the newly transparent slider in daylight.
3. The sunroom floor/ceiling and its window-side dark wall are still coarse compared with its
   furnished breakfast/bar layer; the just-visible terrace lantern cuts into the upper left
   glazing edge from this fixed camera.
4. Formal living still has weak furniture/floor contact away from its piano practical, while
   secondary rooms/elevations remain sparse.

Fixed: both canonical `D_SLIDER` leaves now explicitly choose approved clear glazing. The shared
generator makes a full-depth aluminium perimeter, two overlapping shallow sashes on separate
tracks, two 8 mm panes and a two-sided pull, instead of using the generic opaque door slab.
Weather-facing aluminium is individually exterior-resident; it does not pull the room walls or
trim through the portal. The sunroom's terrace furniture and lawn are now visible through the
door by day; the night view retains a dark outdoor separation rather than becoming a bright
fake interior window. The master-balcony slider shares the same correction without a room branch.

Against the exact Round 83 frames, `sunroom-breakfast` changes 138,826 day / 138,658 night pixels
above two channel levels (9.64% / 9.63%; normalized RGB MAE 0.019784 / 0.008334);
`backyard-to-house` changes 14,864 / 15,157 (1.032% / 1.053%; MAE 0.000778 / 0.000864).
`rear-terrace`, facing away from the leaf, changes only 158 / 150 pixels (0.011% / 0.010%).
Seven strict references with a visible line of sight were each opened old/actual/diff and
intentionally advanced: exterior north/northeast blockout, master-bedroom blockout, first-person
hall/foyer-stair and property orchard/terrace. No other golden was regenerated.

The world is 722 chunks / 116 props / 286 exterior instances / 59.850882 MB. `L0_SUNROOM` is
21 chunks and `L1_MASTER_BED` eight, one new aluminium batch each; the clear panes share existing
glass batches. The selected unwrap holds its 350 receiver faces unchanged. World validation,
shell generation, material assignment, manifest, content, render/culling and XNA-only gates pass.
The independent shell-realism check requires both panes and the meeting stile rather than treating
the former opaque-slab predicate as an exemption. All 1,409 unit, 135 integration and 48 active
render tests pass; eighteen culling pairs stay below 0.2% and 323 strict-XNA translation units
are clean. Heavy work stays on CPU 0-5 / at most six workers.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay uses production materials, not the optional
debug blockout view. The next review should address the largest physical night-depth/nearby
garden defect or a better rear elevation, guided by these unchanged cameras—not by task numbers.

## Round 85 — measured warm tile in the sunroom

Commit: `HOUSE-00950` checkpoint (`2026-09-19`; exact HEAD in `docs/handoff.md`).

Before: Round 84's 26-camera clear-day *(capture removed)* and
normal-night *(capture removed)* sets. After: fixed, matched 26-camera
clear-day *(capture removed)* and
normal-night *(capture removed)* sets. Both contact sheets and direct
`sunroom-breakfast` / `sunroom-wet-bar` day/night pairs were opened at full resolution.

Ranked visible defects remaining:

1. The garden beyond the terrace wall lanterns is still nearly black at 22:00, with bright
   detached foliage and no convincing pool on the actual lawn. The 16 m Basic-terrain chunks
   select nearby fixtures by whole-chunk centre; merely scattering existing 3 m bollards would
   not correct the ground. Treat the receiver/light-assignment boundary deliberately.
2. The sunroom's broad sage window-side wall and ceiling are too dark even at clear 10:30; the
   new tile reveals the mismatch with the bright exterior. Daylight/receiver depth is the next
   high-value indoor correction, not another global exposure increase.
3. The upper rear elevation and balcony rail are still simple, and the fence/lawn read as flat
   planes through the newly clear glass. Formal living contact and secondary-room density remain.

Fixed: only `L0_SUNROOM` exchanges the cold continuous marble finish for a provenanced CC0
Tiles139 warm limestone-look ceramic. Its own DualTexture role maps four source tiles per
repeat at 0.45 repeats/world metre: 0.556 m modules with visible joints beneath the breakfast
group and wet bar. The foyer, cellar and master bath keep marble; the atlas, weather, lamps,
geometry, portals, collision and night garden are unchanged. The exact day `sunroom-breakfast`
and `sunroom-wet-bar` pairs change 0.002364 / 0.002881 normalized RGB MAE; at night they change
0.001333 / 0.001397. The reciprocal terrace camera is effectively unchanged (day MAE
0.000000014). This is a clearly visible material/scale improvement, not a lighting solution.

Three strict references with the floor in view were each inspected as old/actual/amplified
difference and intentionally advanced: `blockout-l0-kitchen` (debug material-id hue, 5,827
pixels), `fp-l0-hall` (the small distant floor strip, 2,695) and `fp-l0-kitchen` (the nearby
floor, 12,517). No other golden was moved. The verified test census and precise culling bound
are recorded in `plan.md` and the latest handoff.

`VISUAL-GATE-1` still **FAILS**. Normal gameplay uses only production materials; the debug
blockout remains explicit. The next work must make a meaningful daylight or physically grounded
night improvement rather than hiding the remaining defect behind global exposure.

## Round 86 — sunroom practicals light the room, not only themselves

Commit: `HOUSE-01292` checkpoint (`2026-09-19`; exact HEAD in `docs/handoff.md`).

Before: Round 85's complete clear-day *(capture removed)* and
normal-night *(capture removed)* controls. After: matched 26-camera
[clear-day](captures/house-01292-sunroom-day-r1) and
[normal-night](captures/house-01292-sunroom-night-r1) sets. Both contact sheets and the full-size
`sunroom-breakfast` and `sunroom-wet-bar` pairs were opened. An additional clear-day
sunroom-breakfast capture with both sunroom groups OFF proved the baseline anomaly: the same
sage wall at pixel (750,440) read RGB (22,27,22) with both ON but (39,58,59) with both OFF.

Ranked visible defects remaining:

1. At 22:00 the garden beyond the terrace is nearly black while isolated foliage glows. The
   16 m Basic terrain batch cannot correctly rank short-range garden fixtures by its single
   centre; lighting the ground calls for a receiver-aware solution, not scattered bulbs.
2. Across the open wet-bar/kitchen threshold, sunroom floor and wall are now warmly legible
   while the neighbouring kitchen recess is much darker. The per-room exposure and bake
   calibration mismatch deserves a separate measured investigation; do not lift all exposure.
3. Rear upper elevation, balcony rail and fence/lawn remain geometrically simple. The sunroom
   breakfast still has too little layered decor for a convincing inhabited space.

Fixed: only the six approved, physically placed sunroom fixtures use a lower source-local
offline lumens/radiant-watt calibration. At the same emitted 5,500 lm, the main 256-sample
atlas mean/peak rises 0.02946/0.17145 → 0.11499/0.67001; the wet-bar group rises
0.002777/0.19880 → 0.011037/0.79517. The night room becomes readable without turning the
outside lawn orange, and the clear-day wall, ceiling and tile regain depth. The matched
breakfast/wet-bar frames change by normalized RGB MAE 0.063509/0.064766 by day and
0.063545/0.064717 by night; `rear-terrace` is unchanged in daylight. This is a calibrated
local irradiance bake, not a global exposure, renderer, source-lumen or geometry change.

Only the `fp-l0-hall` and `fp-l0-kitchen` strict references advance after individual old/new/diff
inspection of the visible sunroom region. `VISUAL-GATE-1` still **FAILS**: the night garden,
cross-room contrast and rear exterior remain the highest-value next work.

## Round 87 — kitchen ceiling practicals and complete-L0 coverage review

Commit: `HOUSE-01293` checkpoint (`2026-09-19`; exact HEAD in `docs/handoff.md`). Before:
Round 86's matched [day](captures/house-01292-sunroom-day-r1) and
[night](captures/house-01292-sunroom-night-r1) sets. The first candidate
day *(capture removed)* / night *(capture removed)*
sets exposed a wrong halo in the bedroom above. Final compiled-game 26-camera
[day](captures/house-01293-kitchen-day-final) and
[night](captures/house-01293-kitchen-night-final) sets each have a contact sheet. The whole
foyer→hall→living→family→kitchen→dining route was inspected through fixed eye-height views,
not just the kitchen crop; a continuous walking check remains for the next coverage batch.
Four approved bronze/opal semi-flush fixtures are visible; the kitchen floor and
work zone gain warm depth. Main-bake mean/peak 0.02838/0.16987 → 0.07021/0.41957; the
independent island/sink/under-cab calibrations remain intact.

Ranked visible defects remaining across the route:

1. `L0_FAMILY` has recognisable seating/media, but the window-facing view retains a broad bare
   wall and sparse furniture composition. The complete-room `HOUSE-00992` task remains open.
2. `L0_DINING` has a table, chairs and pendants but dark, nearly empty side walls; foyer's large
   arrival wall and hall-to-kitchen transition also feel sparse. Close the real full-room
   furnishing tasks before adding detail to an already strong piano or facade screenshot.
3. The kitchen far recess is still less legible than its work floor, while at 22:00 the rear
   garden ground is almost black despite detached bright foliage. The 16 m terrain receiver
   mismatch remains; random short-range bulbs are not an answer.

The first strict render exposed a false two-spot halo in `fp-l1-master-bed`: a camera-facing
presentation billboard crossed the 0.35 m structural deck. A 4 cm lowering probe reduced but
did not solve it and was reverted. The accepted stock-XNA source fix clips presentation halos
outside their owning storey's vertical envelope; the original bedroom reference now passes
unchanged. Only the individually inspected `blockout-l0-kitchen`, `fp-l0-hall` and
`fp-l0-kitchen` references advance. Final day `central-hall` / `kitchen-from-hall` /
`kitchen-facing-west` normalised RGB MAE against Round 86 is 0.013219 / 0.035704 / 0.038979;
night is 0.013241 / 0.036761 / 0.040176. The rear terrace remains effectively unchanged.
All 1,409 unit, 135 integration and 48 active render tests, 18 culling pairs and strict-XNA
gates pass. **VISUAL-GATE-1 still fails**; the next sprint scheduler explicitly favours breadth
across the connected L0 rooms before micro-polish.

## Round 88 — dining service wall, reviewed against the whole L0 route

Commit: `HOUSE-01069` checkpoint (`2026-09-19`). Before: Round 87's 26-camera
[day](captures/house-01293-kitchen-day-final) and
[night](captures/house-01293-kitchen-night-final) sets. After: the built game's 27-camera
[day](captures/house-01069-dining-day-final-r6) and
[night](captures/house-01069-dining-night-final-r6) sets; each includes an eight-view
`l0-route-contact.png`. Both full sets and the dining/adjacent-room views were inspected at
normal eye height. Earlier `r1`/`r2` captures mixed a new bake with a stale deployed
`build/content/world/chunks.bin` and are rejected as visual evidence; later calibration
probes were also rejected rather than mistaken for the retained result.

Ranked visible defects before this checkpoint:

1. Dining's table and chandelier existed, but its side wall had no serving furniture and the
   two nominal side lights were bare, unlinked points. The room still read as a dark corridor.
2. Across the connected L0 route, living remains too dark, family has a sparse window-facing
   composition, the kitchen far recess is underdeveloped, and foyer/hall retain broad bare bays.
3. The exterior-to-interior route cannot yet be walked end to end in normal play: the physical
   road/gate check reached `EXT_ROAD`, but the phase-14 interaction dispatch is unfinished, so
   the pedestrian gate/front door cannot be opened by `E`. Interior visual cameras are fixed
   review spawns, not a false claim that the gate route is currently traversable.

Fixed: a deterministic 1.62 m walnut/brass/ceramic sideboard with a real collision proxy now
occupies the solid dining bay; two 0.616 m ceramic/brass lamps sit on its 0.855 m cabinet top.
Their exact `DiningSideShade` slots and optical positions link the existing side circuit, now
2 × 150 lm at 2400 K and default-on. The selected 256-sample artificial/daylight bakes were
promoted at 300 lm/radiant-watt, leaving every other receiver and global exposure unchanged.
This local calibration compensates the room's normalised artificial-light level when its side
circuit becomes active: in the unchanged long-axis day pose, the large left-wall crop mean is
0.5317 before and 0.5282 after, while the sideboard/lamp view gains the physical composition.
The deliberately narrow wall bay leaves about 0.63 m between the cabinet front and the end
chair's proxy rather than intersecting the table group.

Remaining: the cabinet and dining side wall are still very dark in close night view, so this
bounded checkpoint is not the full `HOUSE-00989` room task. The stronger whole-route gains now
lie in living-room readability, family-room furnishing density, kitchen recess completion and
the real gate/door interaction prerequisite for a continuous first-person walk. No further
driveway, gutter or single-hero-object polish outranks those. **VISUAL-GATE-1 still fails.**

## Round 89 — formal-living room-scale light, not another hero prop

Commit: `HOUSE-01294` checkpoint (`2026-09-19`). Before: Round 88's 27-camera
[day](captures/house-01069-dining-day-final-r6) and
[night](captures/house-01069-dining-night-final-r6) sets. After: the built game's same 27-camera
[day](captures/house-01294-living-day-r2) and
[night](captures/house-01294-living-night-r2) sets, each with the same eight-view
`l0-route-contact.png`. The retained `r1` sets are a rejected 40 lm/radiant-watt intermediate,
not the accepted 25-calibrated result. Both final contact sheets plus the full-size living,
foyer-door and dining-adjacent views were opened.

Ranked visible defects before this checkpoint:

1. The living room had sofa, rug, fireplace, piano and four real default-on ceiling lights,
   yet walls and floor were nearly black in both fixed day and night views. This made the
   connected hall-to-living transition visibly less finished than the hall.
2. Family remains sparse on its window-facing side, the kitchen far recess and dining's close
   side wall are dark, and broad foyer/hall bays still need purposeful furnishing.
3. The road-to-door route still cannot be walked through the closed pedestrian gate: the
   input is recorded but phase-14 interaction dispatch is unfinished.

Fixed: only the four existing 4 × 1,200 lm/3,000 K living-main sources change their offline
conversion from 100 to 25 lm/radiant-watt. The selected 256-sample main atlas mean/peak rises
0.03229/0.19948 → 0.12914/0.79792, with no new fixture, changed switch state, global
exposure or other receiver-cell promotion. Same-pose 22:00 wall/floor grayscale crops rise
0.5350/0.5316 → 0.6026/0.5626; the fireplace/seating composition now has readable warm
surface and floor depth without a clipped ceiling. Foyer's closed-door face and the hall stay
visually stable; dining gains only the expected view of a brighter adjacent living room.
The normalized PNG atlas itself stayed byte-identical; its physically applied scale and
provenance report changed. All active strict render references still pass unchanged.

Remaining: living's natural daylight still contributes little at this 10:30 winter/spring
camera, and the piano's dark body remains less legible than the room envelope. Do not respond
with another piano-only polish while family, kitchen recess, dining side wall and arrival bays
need broader coverage. The gate/door interaction prerequisite remains a separate gameplay
route blocker. **VISUAL-GATE-1 still fails.**

## Round 90 — family room gains full-room practical light

Commit: `HOUSE-01295` checkpoint (`2026-09-19`). Before: Round 89's 27-camera
[day](captures/house-01294-living-day-r2) and
[night](captures/house-01294-living-night-r2) sets. After: the same built-game 27-camera
[day](captures/house-01295-family-day-r1) and
[night](captures/house-01295-family-night-r1) sets; each has the eight-view
`l0-route-contact.png`. Both contact sheets and full-size family window, family-to-kitchen,
living, foyer/hall and reciprocal kitchen views were opened at normal eye height.

Ranked defects before this checkpoint:

1. The four physical, default-on family main fixtures emitted 4,800 lm but the original
   main atlas averaged only 0.04538 and peaked at 0.22923. The furnished room looked
   mostly unlit at night and weak beside its connected kitchen even in daytime.
2. Family's window-facing side remains sparse and the large glazing frames a bare, flat
   exterior. Kitchen's far recess, dining's close side wall and arrival bays still need
   broader furnishing/lighting attention.
3. The closed front pedestrian gate remains an interaction-system prerequisite before a
   genuine road-to-dining first-person walkthrough is possible.

Fixed: add a 30 lm/radiant-watt offline conversion only to the four existing family main
sources, retaining their 1,200 lm/3,000 K real fixtures and current switch ownership. The
selected 256-sample `L0_FAMILY` bake keeps its established 100 lm/radiant-watt receiver
baseline so independently switched media/reading groups are not silently dimmed. Main
mean/peak rises 0.04538/0.22923 → 0.15094/0.76469. Same-pose 22:00 grayscale wall/floor
crops rise 0.5625/0.5657 → 0.6575/0.6207. The room reads as a warm occupied family space
from both window and kitchen sides; ceiling and windows retain their shape rather than
washing out. No geometry, renderer exposure, other receiver cells or furniture moved. A
diagnostic bake with the wrong default 683 was rejected before promotion because it halved
the main contribution and dimmed both other groups; it is not evidence for this result.
The sole strict-reference change is `fp-l0-hall`: old/new/amplified-diff inspection confines
its 745 changed pixels (0.3234%) to the narrow family-room sliver beyond the kitchen opening.
The other active references and all 18 culled/unculled comparisons stayed within contract.

Remaining: window-side furnishing and believable exterior context are now the room's largest
visible deficits. Continue with connected L0 coverage, especially kitchen recess and dining
side wall; do not turn this into another sequence of tiny lamp/intensity tweaks.
**VISUAL-GATE-1 still fails.**

## Round 91 — family picture-window wall gains grounded storage

Commit: `HOUSE-01070` checkpoint (`2026-09-19`). Before: Round 90's 27-camera
[day](captures/house-01295-family-day-r1) and
[night](captures/house-01295-family-night-r1) sets. After: the same 27 fixed built-game
[day](captures/house-01070-family-cabinet-day-r2) and
[night](captures/house-01070-family-cabinet-night-r2) sets. Full-size family garden,
family composition, kitchen reciprocal, foyer, hall, living and dining views were opened.

Ranked defects before: (1) the family picture window floated over a broad empty lower wall;
(2) the kitchen's west recess and dining's close side wall remain dark, while foyer/hall have
broad bare bays; (3) family glazing frames a flat, sparse exterior. The interaction-disabled
pedestrian gate still prevents a genuine continuous road-to-dining first-person walk.

Fixed: reuse the approved detailed walnut/brass/ceramic cabinet once, in a different room,
below the family north picture window. Its uniformly scaled 1.426 × 0.823 × 0.456 m body
stands on the real floor with a collision proxy. The top is 27 mm below the 1.45 m sill,
the rear clears the curtain by about 68 mm, and the chair's x-bounds clear its left end by
about 124 mm; the sofa has separate z clearance. The first full-scale capture was rejected:
its top and ceramic bowl intruded into the window. At the accepted scale the window remains
open in both fixed family views, the former empty wall has useful domestic storage, and the
cabinet's repeated design is not visible alongside the dining original in any route frame.
It brings two truthful ceramic/brass material chunks into family (26 → 28; unculled world
726 → 728), not a general limit waiver. The collision push-out unit test records the exact
unreachable 222 mm wall/cabinet gap while its normal walking tour remains clean.

Remaining: the view beyond the family glass is still too flat, the kitchen far recess still
looks dark/empty, dining's close wall lacks room-scale bounce, and foyer/hall arrival bays
remain sparse. No lightmap or strict render golden changed. **VISUAL-GATE-1 still fails.**

## Round 92 — ground the foyer arrival bay

Commit: `HOUSE-01071` checkpoint (`2026-09-19`). Before: Round 91's 27-camera
[day](captures/house-01070-family-cabinet-day-r2) and
[night](captures/house-01070-family-cabinet-night-r2) sets. After: the 28-view fixed
[day](captures/house-01071-foyer-arrival-day-r1) and
[night](captures/house-01071-foyer-arrival-night-r1) built-game sets, adding a fixed
`foyer-entry-floor.png` downward view. The day floor pose was captured separately with
the same deterministic camera/time settings after it joined the standard review set.
The matched normal-height entrance, downward floor, hall, exterior, living, family,
kitchen and dining views were inspected at full size.

Ranked defects before this checkpoint:

1. The foyer's first two-and-a-half-metre stone arrival bay was empty in the ordinary
   entrance view, despite the finished hall runner, nearby chair and console.
2. Kitchen's west/north recess is dark and sparse, and dining's close wall and table
   remain substantially too dark in both day and night route views.
3. The family picture window looks onto a flat exterior; the pedestrian gate still
   lacks an interaction dispatcher for a genuine continuous first-person walk.

Fixed: a project-authored 2.12 × 2.50 m flatwoven wool entry rug with a distinct
light-centred medallion now anchors the foyer stone bay; a single already vetted,
provenanced potted plant balances the existing chair across the door sightline. The
rug clears the 0.96 m door leaf by 1.00 m, the hall threshold by 0.50 m and each side
by 1.14 m. In the normal entrance view the rug is visible at the lower frame edge;
the new downward view confirms its complete footprint, border, scale and no floating
or collision with the chair or doorway. Its wool finish differs from the hall runner
without inventing a new unlicensed texture. Three rug material roles and two plant
roles raise only the measured foyer chunk exception 20 → 25 (final resident 27);
world-wide unculled calls rise 728 → 733, alpha-test batches 47 → 48. The inspected
`blockout-l0-hall` strict-reference update changes 6,533/230,400 pixels (2.8355%),
confined to the entry rug through the hall portal and a tiny plant edge; the other
strict references remain unchanged.

Two exploratory kitchen alternatives were deliberately rejected before this task:
a short end cabinet was hidden by the range from normal route poses, and moving the
existing main lights west barely changed the dark recess. Neither was promoted or
committed. The kitchen/dining darkness is therefore still the highest-impact breadth
defect, not a problem claimed fixed by this foyer checkpoint. **VISUAL-GATE-1 fails.**

## Round 93 — lift the dining-room envelope, not its unlit furniture

Commit: `HOUSE-01296` checkpoint (`2026-09-19`). Before: Round 92's 28-camera
[day](captures/house-01071-foyer-arrival-day-r1) and
[night](captures/house-01071-foyer-arrival-night-r1) sets. After: matched 28-camera
day *(capture removed)* and
night *(capture removed)* built-game sets. Full-size
dining long-axis and sideboard, kitchen reciprocal, central hall, living, family,
foyer and exterior views were opened; the matched fixed time/weather/exposure and
camera poses are unchanged.

Ranked defects before:

1. The dining-room walls and ceiling were almost black from its own camera, despite
   the linked, default-on chandelier and two side lamps; the adjacent hall felt like
   a different, much more finished house.
2. The dining table and chair objects remain very dark even where shell lighting
   improves. The kitchen's west/north work recess is likewise sparse and dark.
3. Family glazing frames a flat exterior, and a real continuous gate-to-dining walk
   still awaits phase-14 interaction dispatch.

Fixed: only the selected `L0_DINING` artificial/daylight receiver pair was rebaked
at 256 samples with a 100 instead of 300 lm/radiant-watt calibration; its three
existing physical fixtures, 2,100 installed lumens, switches, geometry, exposure
and other rooms did not change. Main and side atlas mean/peak rise
0.02319/0.30894 → 0.06957/0.92683 and 0.00515/0.38377 →
0.01544/1.15132. The daylight atlas peak remains 0.68927 and all normalized PNG
bytes stay unchanged; the binding scales and auditable shell hash carry the change.
On the matched dining long-axis frame, normalized grayscale crops rise left wall
0.0568 → 0.1411, ceiling 0.1022 → 0.2475 and side wall 0.1729 → 0.2673 in both
day and night, without a clipped ceiling or visible spill into the reciprocal
kitchen/hall frames. At a channel delta above two, 50.07% of dining long-axis
pixels change, versus 0.51% of the kitchen-facing-west frame and under 0.11%
of each other tested exterior/foyer/hall/living/family/kitchen route view.
The table crop stays exactly 0.1120: these non-lightmapped
objects do not receive the changed shell bake. This is a meaningful envelope
improvement, not a claim that dining furnishing/lighting is finished. All active
strict render references pass unchanged; the separate visual-review views are
what reveal and judge this room-scale change.

Remaining highest visible breadth defects: dark dining furniture/table surface,
the kitchen's west/north working bay, weak living piano/body visibility and the
flat view beyond family glazing. **VISUAL-GATE-1 still fails.**

## Round 94 — light the open kitchen service transition

Commit: `HOUSE-01297` checkpoint (`2026-09-19`). Before: Round 93's 28-camera
day *(capture removed)* and
night *(capture removed)* sets, plus a close diagnostic
at the service threshold. After: 29 fixed built-game
[day](captures/house-01297-butlers-day-r1) and
[night](captures/house-01297-butlers-night-r1) views, including the new
`butlers-from-kitchen.png` threshold view. Kitchen west/hall, dining, living,
family, foyer and exterior poses remain unchanged.

Ranked defects before:

1. The west-kitchen recess reads as a black hole beside the range. A close view proves
   it is the open, unoccupied `L0_BUTLERS` room, not a missing cabinet.
2. Dining's walnut table and chairs are still dark despite the brighter room envelope.
3. The family garden view remains flat; the gate interaction prevents a genuine
   continuous road-to-dining walking inspection.

Fixed: two linked bronze/opal ceiling practicals replace bare, default-off point
sources. Their 1,400 lm switchable circuit starts on in normal play, and its
selected 256-sample atlas changes from a 0.01411 mean/9.93004 firefly peak to
a broad 0.06186 mean/0.32077 peak. The kitchen west view now shows a lit service
wall and floor rather than an unlit void, by day and night. The threshold view
confirms physical fixtures, finish continuity and no clipped ceiling. A separate
night forced-off capture verifies the independent switch. The strict
`fp-l0-kitchen` difference was inspected before its single-reference update;
an upper bedroom on/off control was byte-identical, so no switched-source halo
was accepted there. The room itself is conspicuously empty: this fixes lighting,
not the still-open full `HOUSE-00991` furnishing task.

Remaining highest visible breadth defects: equip the now-visible service room
with fitted storage/worktop, make the dining table readable, and improve the
flat family garden sightline. **VISUAL-GATE-1 still fails.**

## Round 95 — fit a real work run into the open service passage

Commit: `HOUSE-01072` checkpoint (`2026-09-19`). Before: Round 94's 29-camera
[day](captures/house-01297-butlers-day-r1) and
[night](captures/house-01297-butlers-night-r1) sets. After: matched 29-camera
[day](captures/house-01072-butlers-run-day-r1) and
[night](captures/house-01072-butlers-run-night-r1) built-game sets. The new
`butlers-from-kitchen.png` view and unchanged `kitchen-facing-west.png` frame
were opened full-size in both lighting conditions; the dining, family and other
route views were checked as the next breadth candidates.

Ranked defects before:

1. The lit `L0_BUTLERS` threshold is conspicuously empty, a whole-room gap
   visible beside the kitchen range in ordinary play.
2. The dining table, chairs and sideboard still read too dark even after its
   room envelope was recalibrated; the family garden beyond the glazing is flat.
3. The gate interaction still prevents a genuine continuous road-to-dining walk.

Fixed: a reproducible 1.62 m wide under-window shaker work run now visibly
equips the service room. It has a real stone sink cutout, compact steel tap,
three recessed drawers, a glass-front wine cooler, physical handles and a box
collision proxy. Its 0.893 m counter is 7 mm below the window sill, and the
136 mm faucet intrusion remains within the lower sash. The fitted geometry
clears both side doors and contacts the floor; from the kitchen it turns the
former bare recess into a readable work area without blocking the window.
Project-authored Blender source, source hash, approved existing material IDs,
manifest, licence and stable prop/model IDs remain auditable. Only the measured
butler's-pantry chunk exception rises 10→14 (world calls 736→740). All strict
first-person references and 18 culled/unculled pairs pass unchanged. The sole
strict property-pose difference is a narrow view through the west-sideyard
service window: 791/230,400 pixels above channel delta 2 (0.3433%), confined
to the new under-window cabinet and one edge pixel. The old/new and amplified
diff were inspected before deliberately advancing only
`property-sideyard-west.png`; all four software goldens then pass.

Remaining highest visible breadth defects: brighten/dress the dark dining
furniture at room scale, improve the flat family-garden sightline, then examine
the weak living piano/body and any remaining major route discontinuity. The
service room's side walls are still spare, so full `HOUSE-00991` remains open;
do not immediately spend another checkpoint micro-polishing it while the larger
connected rooms need work. **VISUAL-GATE-1 still fails.**

## Round 96 — make the dining table visibly occupied

Commit: `HOUSE-01073` checkpoint (`2026-09-19`). Before: Round 95's 29-view
[day](captures/house-01072-butlers-run-day-r1)/
[night](captures/house-01072-butlers-run-night-r1) route. After: matching
[day](captures/house-01073-dining-table-day-r1)/
[night](captures/house-01073-dining-table-night-r1) built-game captures.
The dining-room, sideboard, reciprocal kitchen, family and route-transition
views were inspected full-size; the normal daytime and 22:00 night presentation
both show the same designed table layout. These local capture PNGs are ignored
review evidence, not strict Git goldens.

Ranked defects before:

1. The otherwise furnished dining room has a bare, near-black eight-seat table
   dominating the ordinary dining camera.
2. The table, chairs and sideboard remain too dark against the now-readable
   room shell, by day and night.
3. The family garden sightline remains flat; the gate interaction still prevents
   a continuous road-to-dining first-person walk.

Fixed: a measured warm-walnut room finish and lighter upholstery improve the
dining furniture as a group, and an authored eight-place ceramic/linen setting
turns the blank tabletop into an unmistakable dining surface. The eight places,
runner, folded napkins and low bowl sit on the actual top, within its physical
footprint, with no new collision or circulation obstruction. The setting has
deterministic Blender source, exact hash and a geometry/UV/placement check;
approved existing texture sources and licence records are retained. The room's
measured opaque chunk exception rises 15→17, with one more shared-walnut batch
in `L0_FAMILY` (28→29); world unculled calls rise 740→743. The full day/night
route captures show no new obvious adjacent-room discontinuity. All strict
references, including four software goldens and 18 culling pairs, pass without
any reference update.

Remaining: the non-lightmapped furniture still receives insufficient daytime
and practical-light energy; the near-black sideboard and chair faces are now
the most visible dining defect. Address room-scale object lighting, not another
small table prop, and compare the entire connected route. The flat family
garden view, weak living piano visibility and unimplemented gate `E` action
remain. `HOUSE-00989` and **VISUAL-GATE-1 remain open.**

## Round 97 — doorway sky flash during an actual walk

Commit: `HOUSE-00701` checkpoint (`2026-09-19`). Reviewed the player's reported
hall→kitchen threshold at normal eye height with a 90-fixed-step headless walk;
the exact 5 cm transition band is sampled every frame. The fixed-pose
`build/test-output/door-hall_kitchen-*-{culled,unculled}.png` pair was also
inspected, but a teleport cannot reproduce a transient that exists only while
the gameplay cell lags the moving eye. The exterior road's normal and
unculled captures were separately inspected as a still-open exterior defect.

Ranked defects before:

1. For several frames at a doorway, the room ahead disappears and sky shows
   through because the render traversal starts in the previous gameplay cell.
2. The road view still shows under-rendered interiors behind some front glass;
   roof/edge details also differ from the unculled view. This is independent
   of the doorway crossing and is not claimed fixed here.
3. Main stair access and the Shift speed toggle remain reported gameplay gaps.

Fixed: the render and room-light camera cell now comes from the exact eye
position; §16.4's 5 cm body hysteresis remains for collision. The new test
failed before the fix while the eye was in `L0_KITCHEN` and the body still in
`L0_HALL`, then passed with the kitchen both root and visible. All 18 existing
culled/unculled poses remain under the unchanged 0.2% threshold, worst 0.0558%.
No strict golden was changed. The road-view experiment that seeded every
projected front window was rejected: it reached 25 cells from the road and
still left thin exterior geometry differences, so it was not shipped as a
broad overdraw workaround.

Remaining: solve the exterior glazing/envelope ownership with a bounded
render comparison, then fix the real stair approach and Shift toggle. The
complete L0 route and **VISUAL-GATE-1 remain open.**

## Round 98 — reachable main stair, full-route review

Commit: `HOUSE-00489` checkpoint (`2026-09-20`). Before: the old first-person
foyer/stair reference *(capture removed)*
showed a flight blocking the foyer-side approach. After: the fixed
31-view clear-day set *(capture removed)* includes the same L0
route plus new stair-foot *(capture removed)*
and L1-exit *(capture removed)* views.
The ignored capture files are local review evidence, not Git-tracked goldens.

Ranked defects before: (1) foyer-to-L1 stair physically inaccessible; (2)
exterior glazing/roof still sometimes under-renders from outdoors; (3) some
connected L0 transitions still need more believable lighting and dressing.

Fixed: the narrow basement opening returns real floor to the west approach;
mirrored U flights, short upper bridges and moved doorways agree in shell and
collision. A real player-controller walk reaches the L1 hall. Thirteen strict
references were advanced individually after inspecting before/after pairs:
two first-person (foyer stair and the small kitchen sightline), four explicit
debug blockout (kitchen, hall, stair and L2 landing), three property facade,
and four seasonal sun/facade views. Their changes follow the stair shell,
altered openings or re-baked receivers; no suite was regenerated blindly.
All 18 culled/unculled comparisons pass, worst 0.0558%.

Remaining, ranked across the route: (1) exterior front glazing/roof visibility
defect; (2) the stair foot is very dark even at 10:30, and its side-on foyer
view reads as a solid dark mass; (3) the L1 exit lacks visual dressing. The
foyer, hall, living, family, kitchen and dining route views remain coherent,
but **VISUAL-GATE-1 remains open**. Do not spend another round on stair trim
before solving the reported exterior under-rendering and L0 breadth.

## Round 99 — exterior glazing and weather-skin closure

Commit: `HOUSE-00702` checkpoint (`2026-09-20`). Before:
road/front *(capture removed)* and
garage approach *(capture removed)*.
After: the same 31-view clear-day set *(capture removed)*,
including front *(capture removed)*,
garage *(capture removed)* and the
foyer→hall→living→family→kitchen→dining route. Captures remain local and
Git-ignored; the text review and strict references are tracked.

Ranked before: (1) pale sky behind front glazing from the road; (2) a large
blue wedge in the main-house gable over the garage and thin front facade sky
slots; (3) uneven room-scale lighting along the connected L0 route.

Fixed: the ordinary outdoor portal walk now seeds only projected glazed
apertures of already visible exterior cells, at depth one. Weather skins meet
at exposed corners; partial-height garage cover no longer suppresses the
upper wall's return. The road view now contains the rooms behind its front
windows, while the garage wedge and front facade slots are gone. Both shell
selftests, the 24-pose exact visible sets and all 18 paired culling images
pass. Twenty-seven strict references were advanced individually after
reviewing each difference mask: exterior/debug changes sit on the corrected
slots, first-person differences follow rebaked UV2 receivers, and seasonal
changes show the newly visible front rooms. No reference suite was blindly
regenerated.

Remaining, ranked across L0: (1) the formal living piano stays almost black
at ordinary eye height; (2) the family→kitchen→dining stretch has insufficient
room/object-light balance and dark primary furniture; (3) the main stair foot
is still very dark. A hairline diagonal seam above the garage is visible only
when magnified; it is not the former open gable wedge. Continue L0 breadth,
not another long exterior micro-polish sequence. **VISUAL-GATE-1 is open.**

## Round 100 — occupy the sunroom's east reading bay

Commit: `HOUSE-01074` checkpoint (`2026-09-20`). Before: the
previous breakfast-side view *(capture removed)*
shows a broad empty floor beyond the occupied table and bar. After: the same
[32-view day](captures/house-01074-sunroom-day-r3) and
[32-view night](captures/house-01074-sunroom-night-r3) route sets, including a
new reciprocal [day](captures/house-01074-sunroom-day-r3/sunroom-lounge.png)
and [night](captures/house-01074-sunroom-night-r3/sunroom-lounge.png) camera.
The fixed hall, living, family, kitchen and dining views were also inspected;
the furniture addition is local, not a claimed route-wide lighting fix.

Ranked before: (1) the connected sunroom's east bay has no human-scale purpose;
(2) static furniture is much darker than nearby baked floor and walls across
L0; (3) the living piano and stair foot remain under-readable. Fixed: two
measured woven-cane reading chairs, cushions and a shared tea table now occupy
the bay without obstructing the slider-side lane. A three-box `_COL` proxy
avoids one solid collision slab across the whole group; two nearby pet nodes
were moved into the clear aisle. A content-graph omission that left
`collision.bin` fresh when a source `.glb` changed was corrected and guarded.
An attempted jute runner looked almost black against the lit tile and was
rejected; it is absent from the source and final captures. No strict golden
image was advanced for this furnishing-only change. The complete content graph,
1,412 unit tests, 140 integration entries and 49 active render tests pass;
all 18 paired culling poses pass, worst 0.0558%. The sunroom remains exactly
21 chunks because the new group reuses four established finishes.

Remaining, ranked across the *whole* L0 route: (1) reconcile Basic-lit static
objects with baked receivers, especially living piano, sunroom chairs and
kitchen/dining furniture; (2) improve the dark stair foot and room-to-room
day/night balance; (3) only then add more secondary dressing. The sunroom is
more recognizably occupied but not fully visually finished. **VISUAL-GATE-1
remains open.**

## Round 101 — give the open kitchen service room a storage purpose

Commit: `HOUSE-01075` checkpoint (`2026-09-20`). Before: the
[day](captures/house-01074-sunroom-day-r3/butlers-from-kitchen.png) and
[night](captures/house-01074-sunroom-night-r3/butlers-from-kitchen.png)
service-room threshold views show a finished sink run between two completely bare
long walls. After: the same fixed [32-view day](captures/house-01075-butlers-day-r1)
and [32-view night](captures/house-01075-butlers-night-r1) sets; inspect the
`butlers-from-kitchen` and `kitchen-facing-west` frames alongside foyer, hall,
living, family, kitchen and dining. Captures are local/Git-ignored.

Ranked before: (1) the kitchen's directly visible service room has no storage
purpose; (2) Basic-lit furniture remains dark relative to baked receivers across
L0; (3) the stair foot and living piano remain under-readable. Fixed: two
different, stocked shallow shelves now frame the under-window sink without
covering either side door. The measured centre aisle remains 1.718 m wide;
the 12-triangle proxies enclose the 2.24 m grounded cabinets. Their six reused
finish roles add only ceramic, linen and paper material chunks, bringing the
exact `L0_BUTLERS` exception from 14 to 17 and whole-world chunks 743 to 746.
At a 2% pixel threshold, the matched day service-room view changes 134,016 of
1,440,000 pixels, while the kitchen west glance changes 2,951 and other tested
route views stay under 900. The room reads as inhabited storage from its
threshold; its night exposure is still too dark for final quality.

Three renderer probes were rejected before this content change: halving the
multi-fixture denominator improved only night crops by ~4–10%; restoring
BasicEffect's unused specular input changed very few visible pixels; stronger
directional keys left the main dark masses intact. None is in source or a
strict golden. Remaining across L0: (1) reconcile furniture/receiver light
balance without flattening room contrast; (2) improve the dark stair foot and
living piano; (3) then add secondary dressing where the full route needs it.
`HOUSE-00991` and **VISUAL-GATE-1 remain open**.

The one strict reference update is `blockout-l0-kitchen.png`: the old versus
actual difference was inspected at original resolution, and its 776 changed
pixels are confined to the sliver of new shelf visible through the kitchen
service opening. No production-material or other blockout reference was
regenerated. Unit 1,412/1,412, integration 140/140 and active render 49/49
pass, including all 18 culled/unculled pairs.

## Round 102 — reconcile fixed-detail lighting across the daylit L0 route

Commit: `HOUSE-01076` checkpoint (`2026-09-20`). Before: the matched
[32-view day](captures/house-01075-butlers-day-r1) and
[32-view night](captures/house-01075-butlers-night-r1) sets. After: the same
[32-view day](captures/house-01076-fixture-day-r2) and
[32-view night](captures/house-01076-fixture-night-r2) cameras. Compare
`living-composition`, `kitchen-facing-west`, `entrance-foyer`, `central-hall`,
`family-composition`, `dining-room` and `exterior-front` in both conditions.
Captures remain local/Git-ignored.

Ranked before: (1) Basic-lit furniture, trim and cabinets are much darker than
their baked receiver walls and floors across several connected rooms; (2) the
living piano and stair foot are especially under-readable; (3) night and
transition lighting are uneven. Fixed: in sunlit indoor cells, the sun keeps
stock-effect key slot zero and the two available slots can receive active,
range-bounded local fixtures. An indoor-only, partly neutral bounced practical
fill makes the sofa, coffee table, kitchen cabinet doors and trim more legible
without flattening the architecture's baked gradients. A brighter all-room
probe looked orange, and a stair-runner/sconce probe made black treads and
large wall hotspots; both were rejected. The final outdoor detail path uses
its prior scale, and the exterior road/facade is not broadly lifted.

Only five first-person references changed: `fp-l0-hall`, `fp-l0-front-door`,
`fp-l0-kitchen`, `fp-l0-hall-corner` and `fp-l0-foyer-stair`. Each old/actual
image was inspected at full size; the changes are brighter indoor doors,
joinery, cabinets and stair-adjacent trim. They were individually replaced,
not regenerated as a suite. The remaining strict references and 18 paired
culling views still pass. Unit 1,413/1,413, integration 140/140 and active
render 49/49 pass.

Remaining, ranked across L0: (1) formal-living piano and some dark primary
furniture still fail normal-eye-height readability; (2) the main stair foot
and service-room night depth are poor; (3) the warm/flat object-versus-bake
balance needs a more physical solution before fine dressing. This is an
incremental cross-room correction, not the final room-lighting milestone.
**VISUAL-GATE-1 remains open.**

## Round 103 — reduced-plan whole-property baseline

Commit: `HOUSE-03203` working tree (`2026-09-21`). The fixed 56-view
[`clear-day`](captures/house-03202-all-day) and
[`clear-night`](captures/house-03202-all-night) sets cover all eleven planning zones; the eight
H1–H7 representatives also have a
[`clear-overcast`](captures/house-03202-heroes-overcast) set. Captures are local and Git-ignored.
Every per-zone contact sheet was inspected at its labelled fixed poses. This is the first formal
review of the basement, service rooms, garage, both upper floors, attic and upper circulation; it
establishes breadth evidence, not a request to polish the already mature ground floor.

**S1:** none visible in the fixed captures. The known house-wide traversal defect is **S2**: door
and gate leaves are visibly closed while the runtime lets the player pass through them. M1 owns it.

**S2 by zone:** `Z-B1` is nearly black by day and night and its empty, generic rooms do not yet
distinguish cinema/gym/workshop/service purposes; `Z-L0M` retains the under-readable living piano,
main-stair foot and uneven object-versus-bake balance; `Z-L0S` is empty and the office/mudroom lack
readable night depth; `Z-GAR` is an empty dark volume whose loft access/guard and interior finish
do not read; `Z-L1` and `Z-L2` are empty, dark after sunset and lack convincing room-specific
joinery/finishes; `Z-L3` is empty and lacks the structure/finished-room contrast needed to read as
an attic; `Z-STAIR` has an under-readable main foot and an almost black basement flight; `Z-EXR`
has sparse working areas and rear/side architectural finish below the front elevation's standard.
`Z-EXF` has no additional zone-specific S2 beyond the house-wide fixed-leaf problem. `Z-STR` has
no S1/S2 in this round.

**S3/S4:** `Z-STR`'s road foreground and repeated vegetation band are plain but remain believable
at walkthrough distance (**S3**, defer to the bounded M11 pass unless a cheap shared-kit change
solves them). No S4 item is scheduled. The day/night sets otherwise show intact major geometry,
materials and exterior weather presentation; darkness and emptiness are content deficiencies, not
camera failures.

The scoreboard's existing completion levels are confirmed without change and are no longer
provisional. The H/M/S/U classification is also confirmed: all 19 main targets and seven hero
areas merit their planned depth, while the service/storage cells remain utility or secondary
rather than hidden hero work. The concrete architecture subset of each S2 list is copied onto the
corresponding M2 task in `plan.md`. Next: `HOUSE-03221` by M1 dependency order and R1, not another
ground-floor polish round.

## Round 104 — whole-property traversal sweep

Commit: `HOUSE-03227` working tree (`2026-09-23`). The 56 fixed production views in the local,
Git-ignored [`clear-day`](captures/house-03227-traversal) set cover all eleven planning zones.
They were inspected as one labelled contact sheet and at the affected zone/tight-space frames.
The fixed `Z-STR` view is the correct evidence for that scenery-only zone: both of its cells are
deliberately inaccessible and are not player-walk targets.

The live pass used the production Linux executable on a private X11 display. Keyboard-driven
first-person movement covered each of the ten zones with accessible cells, with separate close
checks in `L1_MASTER_CLOSET`, `L2_CLOSET_4` and `B1_UNDERSTAIR`. A westbound walk from `L3_ROOM`
entered the low eaves, visibly lowered to the crouched eye, returned along the same route and
restored the standing eye without clipping. The all-cell grand tour remains the exhaustive route
proof; this pass supplied the human-scale checks for snagging, head clearance and the rendered eye.

The seed-618 random soak ran 144,000 fixed steps (20 simulated minutes) independently from
`B1_HALL`, `L0_HALL`, `L1_HALL`, `L2_HALL`, `L3_ROOM` and `EXT_ROAD`. The six walks covered
1,034–1,410 m and 3–18 named cells apiece, with 0 boundary escapes, 0 falls and no sampled
penetration deeper than the established 0.1 mm contact tolerance. The attic's coverage guard is
bounded by its five authored cells rather than requiring a chance descent to a different storey.

**S1/S2 traversal findings:** none. The dark/empty-room and architectural identity items from
Round 103 remain visible and remain assigned to their existing M2/M5 tasks; they are not new
movement defects. The scoreboard is unchanged pending the explicit G1 review. Next:
`HOUSE-03240`, chosen by M1 dependency order and R1 because all three of its traversal/integration
dependencies are now complete.

## Round 105 — first upper floor at C2

Commit: `HOUSE-03263` working tree (`2026-09-23`). Before: the six fixed production views in the
local, Git-ignored [`clear-day`](captures/house-03263-before) set. After: the matching
[`clear-day`](captures/house-03263-after) set and a second
[`lights-forced`](captures/house-03263-after-lit) set with the L1 main groups on. The normal-day
and lights-forced contact sheets and each affected full-size frame were inspected. The latter is
review evidence only: M5 still owns final day/night readability.

The floor already had authored room-type palettes, generated reveals and sills, continuous
skirting/cornice/casing, and one common measured balustrade grammar for its front and rear decks.
The remaining C2 gap was the plain circulation joinery. All eight painted or hardwood single
leaves facing `L1_HALL`/`L1_HALL_W` now explicitly reuse the existing `four_panel` geometry and
two-sided steel lever from `HOUSE-00942`; no room-specific generator branch or new asset family was
introduced. The fixed `first-hall` view changes 6,460 pixels; the other fixed controls change
0–499 pixels except for the adjoining landing at 296, consistent with the local shell change.

The lit sheet resolves the raised panels and lock-side handles at normal eye height while retaining
the bedroom, bathroom, closet and corridor material distinctions. Both balconies remain guarded
and match the established front-balcony language. No S1/S2 architecture issue remains in `Z-L1`;
empty-room furnishing and dark unlit circulation are still the already-scheduled M4/M5 work.
Next: `HOUSE-03264`, selected by R1/R2 because `Z-L2` has the most accessible cells still at C1.

## Round 106 — second upper floor at C2

Commit: `HOUSE-03264` working tree (`2026-09-23`). Before: the six fixed production views in the
local, Git-ignored [`clear-day`](captures/house-03264-before) set. After: the matching
[`clear-day`](captures/house-03264-after) and
[`lights-forced`](captures/house-03264-after-lit) sets. Both contact sheets and the affected
full-size frames were inspected; the forced main groups expose architecture for review without
claiming the M5 day/night-lighting requirement.

The existing L2 envelope already carries distinct room-family palettes and the shared physical
window reveal, sill, skirting, cornice and casing grammar. Its Juliet pair also retains the
finished double-door treatment. All seven single leaves facing the landing or either hall now
explicitly reuse `HOUSE-00942`'s four-panel moulding and two-sided lock-stile steel lever. The
fixed landing and hall frames change 31,573 and 12,323 pixels; library, games and bedroom controls
change 1,001–1,570 pixels, while the unaffected sitting-room frame is byte-identical.

The lit sheet makes the joinery and library/games/sitting/bedroom palette distinctions legible at
normal eye height. No S1/S2 architecture issue remains in `Z-L2`. Library shelving, other
furnishing and dark unlit circulation remain deliberately assigned to M4/M5. Next:
`HOUSE-03261`, selected by R1/R2 because `Z-B1` now has the most accessible cells still at C1.

## Round 107 — basement at C2

Commit: `HOUSE-03261` working tree (`2026-09-23`). Before: four fixed production views in local,
Git-ignored [`clear-day`](captures/house-03261-before-day) and
[`clear-night`](captures/house-03261-before-night) sets. After: matching
[`clear-day`](captures/house-03261-after-day),
[`clear-night`](captures/house-03261-after-night) and
[`lights-forced`](captures/house-03261-after-lit) sets. All contact sheets and the full-size lit
workshop frame were inspected.

The existing palettes already establish finished carpet/tile/paint room families, the
aged-plaster/concrete utility family and the stone/ochre cellar. The existing plywood utility trim
now selects exposed joists, perimeter rim/slab-edge members and unobtrusive wall-line posts with no
room-id special cases. Every B1 door retains its generated casing and selects the established
four-panel/two-sided steel-lever treatment. The eight hopper windows retain their physical exterior
light wells. The workshop frame changes 419,393 pixels; hall, cinema and gym controls change 9,220,
4,981 and 1,242 as affected adjoining leaves enter those views.

The forced-light workshop view resolves the exposed construction and door finish at normal eye
height. The production day/night sheets remain dim but preserve coherent room-family silhouettes;
their final readability and all furnishing remain owned by M5 and M4 respectively. No S1/S2
architecture issue remains in `Z-B1`. Next: `HOUSE-03262`, selected by R1/R2 because `Z-L0S` has
the most accessible cells still at C1; `Z-L0M` remains frozen by R4.

## Round 108 — service rooms and garage at C2

Commit: `HOUSE-03262` working tree (`2026-09-23`). Before: the two fixed service-room views and the
garage-interior view in local, Git-ignored [`Z-L0S`](captures/house-03262-before-l0s) and
[`Z-GAR`](captures/house-03262-before-gar) clear-day sets. After: matching
[`Z-L0S`](captures/house-03262-after-l0s) and [`Z-GAR`](captures/house-03262-after-gar) clear-day
sets, plus a [`lights-forced`](captures/house-03262-after-gar-lit) garage set. A second fixed garage
pose now faces the authored loft hatch so future reviews cannot miss its access and guard.

The existing service palettes and generated trim already establish finished utility/secondary
envelopes, and the garage retains its concrete/aged-plaster construction and detailed two-sided
sectional. Six remaining service leaves now select the established four-panel geometry and
two-sided steel lever. The horizontal loft portal derives a fixed ladder in its parent garage, and
the former four top-rail placeholders become a complete timber guard. The loft remains visible but
non-traversable under the walk-only scope; no interaction or ladder-climbing system was added.

The office and mudroom views change 108,059 and 372,079 pixels as detailed adjoining leaves enter
the frames. The garage-interior control changes only 8 pixels because it faces the sectional; the
new loft-access view directly resolves the two rails, ten rungs and guarded platform, especially
with the existing garage groups forced on. No S1/S2 architecture issue remains in `Z-L0S` or
`Z-GAR`. Their empty-room and normal-night readability gaps stay assigned to M4/M5. Next:
`HOUSE-03266`, selected by R1/R2 because `Z-STAIR` has the most accessible cells still at C1;
`Z-L0M` remains frozen by R4.

## Round 109 — vertical circulation at C2

Commit: `HOUSE-03266` working tree (`2026-09-23`). Before: nine fixed production views in the
local, Git-ignored [`clear-day`](captures/house-03266-before-complete) set. After: matching
[`clear-day`](captures/house-03266-after-final-day) and
[`lights-forced`](captures/house-03266-after-final-lit) sets, plus final
[`front`](captures/house-03266-after-final-exf) and
[`rear`](captures/house-03266-after-final-exr) exterior controls. All four contact sheets and the
affected full-size frames were inspected. The forced set exposes architecture for review without
claiming the M5 production-night requirement.

The existing shell flight grammar now supplies a raked closed stringer, raked handrail, end newels
and pitch-bounded vertical balusters on every exposed flight side. Existing treads retain one
nosing apiece, and turn, exit and cross landings gain shallow edge trim. The main stair uses its
finished timber trim; basement, attic, garage and exterior transitions use their deliberately
plainer authored finishes. Five added fixed poses complete the basement, attic and main-transition
foot/head evidence. Matching clear-day frames change 546–451,206 pixels where the new construction
enters view.

Every view retains a stepped silhouette rather than a ramp or solid mass, and the exterior controls
show guards aligned with the established facade treatment. Both focused traversal tests pass, so
the visual treads and rails did not change the authoritative walkable collision. No S1/S2
architecture issue remains in `Z-STAIR`; final night readability and furnishing remain assigned to
M5/M4. Next: `HOUSE-03265`, selected by R1/R2 because `Z-L3` is the only zone still below C2.

## Round 110 — attic at C2

Commit: `HOUSE-03265` working tree (`2026-09-23`). Before: the two inherited fixed production views
in local, Git-ignored [`clear-day`](captures/house-03265-before-day) and
[`lights-forced`](captures/house-03265-before-lit) sets. After: four-view matching-condition
[`clear-day`](captures/house-03265-after-day) and
[`lights-forced`](captures/house-03265-after-lit) sets. Both final contact sheets and every frame
were inspected. Utility cells remain covered by the zone walk rather than violating the review
protocol's rule against fixed poses in U-tier cells.

The existing rafter-bounded shell already clipped each cell under the roof, provided long-slope
rafters and purlins, cut dormer reveals, laid rough store walkways, and retained `L3_ROOM`'s flat
collar ceiling. Recessed insulation-finish faces now fill the unfinished long-slope bays between the
400 mm rafters, and one same-section transverse purlin under each hip gives the end stores visible
roof framing without a costly jack-rafter expansion. The two added west-store poses expose the low
hip eaves and look through to the insulated north bay. The inherited room/store controls change
194 / 365 pixels; direct new views carry the broader acceptance evidence.

The lit sheet clearly separates the finished plaster/collar envelope from exposed timber,
insulation and walkway boards. The all-cell grand tour reaches all five cells, while both automatic
crouch regressions pass and preserve the feet/eye transition under low rafters. No S1/S2
architecture issue remains in `Z-L3`; furnishing and final night readability remain M4/M5 work.
Next: `HOUSE-03267`, the sole remaining M2 zone task, selected by R2.

## Round 111 — rear and side elevations at C2

Commit: `HOUSE-03267` working tree (`2026-09-23`). Before: the three inherited fixed production
views in local, Git-ignored [`clear-day`](captures/house-03267-before-day) and
[`clear-night`](captures/house-03267-before-night) sets, plus a direct manual inspection of the
shed's raw door/window elevation. After: four-view [`clear-day`](captures/house-03267-after-day)
and [`clear-night`](captures/house-03267-after-night) sets; the latter forces only the existing
`LG_EXT_SHED_MAIN` group. Both contact sheets and every frame were inspected.

The existing shared house-shell rules already finish rear/side sills, frames, roof edges, gutters
and downspouts. Data selects six-over-six grilles and black shutters only on the 17 front
double-hungs, making their absence elsewhere deliberate; both rear sliders retain finished
aluminium joinery. The remaining defect was the single-material shed: its generator now reuses
approved outdoor siding, roof, painted trim, steel, clear glass and timber while adding four
corner boards, paired eaves fascia, a cased door and framed/glazed window. The new path-side pose
shows those features directly; utility side yards remain zone-walk evidence rather than fixed
targets.

The inherited garden frame changes 326,482 clear-day and 326,337 clear-night pixels, localized to
the shed; the two day rear controls change only 560 / 586 pixels. The finished shed remains at the
ordinary 6-chunk target. The property walk and all-cell grand tour remain green, and no S1/S2
architecture issue remains in `Z-EXR`. Final furnishing and baseline night balance remain M4/M5
work. Next: gate review `HOUSE-03280`, selected by R2/R10 because every zone is now at C2.

## Round 112 — G2 whole-property architecture review

Commit: `HOUSE-03280` working tree (`2026-09-23`). The local, Git-ignored
[`clear-day`](captures/house-03280-g2) set contains all 65 fixed production views, eleven labelled
zone sheets and one all-zone sheet. The combined sheet and the architecture-critical `Z-B1`,
`Z-L1`, `Z-L2`, `Z-L3`, `Z-STAIR` and `Z-EXR` sheets were inspected at full resolution; the other
zone sheets were checked in the combined pass.

The review confirms the finished palettes, reveals, sills, trim and joinery on every house level;
complete tier-appropriate stair silhouettes; the garage envelope and loft access; the attic's
finished/unfinished construction contrast; and the shared rear/side elevation and shed finish.
No clipping, missing shell face, unfinished opening, broken guard, stair mass or other S1/S2
architecture defect is visible. Several empty secondary rooms remain dark in the normal new-game
day state. That is the already-scheduled M5 baseline-lighting gap, while their absent props remain
M4 work; neither is recast as architecture scope at this gate.

`zone_scoreboard.py --check` assigns all 96 authored cells exactly once across eleven zones and two
explicit exclusions, and the plan scoreboard records every zone at least C2. G2 passes. Next:
`HOUSE-03301`, selected by the house-wide support exception and dependency order to begin the kit
path that unlocks broad furnishing.

## Round 113 — first-floor circulation dressing

Commit: `HOUSE-00999` working tree (`2026-09-23`). The local, Git-ignored
[`clear-day`](captures/house-00999-l1-circulation-day) set covers all six fixed `Z-L1` views. Its
contact sheet and the full-size landing/hall frames were inspected. Supplemental forced-light
detail views in [`circulation-detail`](captures/house-00999-l1-circulation-detail) directly inspect
the landing bench, both scaled runners and both framed images; they are evidence for prop geometry,
not a claim that M5's normal day/night balance is complete.

The three circulation cells reuse only established kit pieces: the upholstered piano bench,
hall-runner plane and portrait member of the acquired four-image art set. Initial rendered review
found the art buried in the wall finish, then found that its image was hidden behind the mat. The
final wall offsets are measured inside the shell, and the shared art generator now emits a
front-facing picture quad. Its runtime images are 512-square power-of-two resources, avoiding the
Reach wrap-sampler failure revealed by the first placed instance while the physical plane retains
the portrait aspect. Both final detail views show the multicolour image inside its mat and frame.

The landing bench is grounded beneath the window, both runners remain flat without z-fighting, and
all doorways and the landing route remain clear. There is no visible clipping, floating object,
portal obstruction or other S1/S2 defect in these cells. The production fixed hall frames remain
deliberately dark; final baseline readability is the existing M5 requirement. Next:
`HOUSE-01000`, selected by R2's tied-zone order and permitted as the second consecutive `Z-L1`
task under R3.

## Round 114 — first-floor main-bedroom dressing

Commit: `HOUSE-01000` working tree (`2026-09-23`). The local, Git-ignored
[`clear-day`](captures/house-01000-l1-master-day) set covers all six fixed `Z-L1` views. The
master-bedroom frame and zone sheet were inspected at full resolution. A forced-light set in
[`master-lit`](captures/house-01000-l1-master-lit) plus reciprocal room-side views checks the
furniture that the fixed west-side camera cannot see directly; forced groups are placement
evidence, not a claim that M5's normal light balance is complete.

The complete `R-BED-PRIMARY` main-tier set is visibly grounded and reads as one room composition:
the upholstered double bed and paired nightstands anchor the north wall, the wardrobe occupies the
north-east bay, the desk/seat and dresser stay clear of the west closet door, the armchair keeps
the south-east glazing approach open, and the rug remains flat beneath the bed. The reused open
curtain family frames the paired south windows without changing their glass opacity or blocking
the balcony slider. Reciprocal renders expose the desk, chair and dresser contacts and confirm the
bed group does not clip the portal, wall trim or other furniture.

No S1/S2 furnishing defect, floating piece, z-fighting, impossible intersection or blocked portal
is visible. The production daytime view remains deliberately dim; baseline day/night readability
is M5 scope. Next: `HOUSE-01001`, selected by R2's tied-zone order as R3's permitted third
consecutive `Z-L1` task.

## Round 115 — first-floor master wet/storage dressing

Commit: `HOUSE-01001` working tree (`2026-09-23`). The local, Git-ignored
[`clear-day`](captures/house-01001-l1-wet-storage-day) and
[`forced-light`](captures/house-01001-l1-wet-storage-lit) sets cover all six fixed `Z-L1` views.
The master-bath frame and zone sheet were inspected at full resolution. Supplemental forced-light
[`detail`](captures/house-01001-l1-wet-storage-detail) views look squarely at the bathroom's south
fixture wall and reciprocally across both ends of the narrow walk-in closet; these are placement
evidence, not a claim that M5's normal light balance is complete.

The secondary bathroom reads from the acquired WC, vanity-basin and bath plus the generated mirror
and towel rail. Moving the bath from the first draft's fixed camera position to the north-west wall
segment preserved its route, kept the obscured window clear and restored an unobstructed review
pose. The closet's stable-id wardrobe moved away from the north-wall push-out to the north-east
segment; a dresser and one bounded folded-textile cluster complete the opposite end. The two
closet portals and its central passage remain visibly clear.

All visible pieces are grounded or deliberately wall-mounted. The textile cluster rests on the
dresser, and no clipping, floating object, z-fighting, impossible intersection or blocked portal
is visible. Normal 10:30 bathroom surfaces are still very dark; that known house-wide readability
gap belongs to M5. R3 now requires a zone move after three consecutive `Z-L1` tasks, and R2 selects
`HOUSE-01009` in `Z-L2` next.

## Round 116 — second-upper-floor circulation dressing

Commit: `HOUSE-01009` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-01009-l2-circulation-day) and
[`forced-light`](captures/house-01009-l2-circulation-lit) sets cover all six fixed `Z-L2` views.
Supplemental forced-light [`detail`](captures/house-01009-l2-circulation-detail) views inspect the
landing bench and both framed images directly; these are placement evidence, not a claim that M5's
normal day/night balance is complete.

The three secondary circulation cells reuse only established kit pieces: the upholstered piano
bench, hall-runner plane and portrait member of the acquired four-image art set. The landing bench
is grounded on a short west-wall segment clear of the library portal, both runners remain flat,
and both frames sit on and face away from their supporting walls. The door, cased-opening and stair
routes remain visibly unobstructed. No clipping, floating object, z-fighting or other S1/S2
furnishing defect is visible. The normal fixed hall frames remain deliberately dark; final
baseline readability is the existing M5 requirement.

R2 now ties `Z-B1` and `Z-L1` at fourteen accessible cells still below target and selects the
least-recently worked `Z-B1`; `Z-L2` has twelve. Next: `HOUSE-01020`, dressing `B1_HALL`,
`B1_STAIR` and `B1_UNDERSTAIR`.

## Round 117 — basement circulation dressing

Commit: `HOUSE-01020` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-01020-b1-circulation-day) and
[`forced-light`](captures/house-01020-b1-circulation-lit) sets cover all four fixed `Z-B1` views.
Supplemental forced-light [`detail`](captures/house-01020-b1-circulation-detail) views inspect the
hall console and bench, the stair-landing bench and the low under-stair storage cluster directly;
these are placement evidence, not a claim that M5's normal basement balance is complete.

The hall's console is grounded at the closed north end and its bench occupies the short south-east
wall without approaching a door sweep. The second bench remains clear of both the basement flight
and its landing door. In the 1.75 m-high L-shaped store, the scaled shelf and box read as a bounded
utility composition beyond the crouched standing point. All five pieces are grounded, no portal or
route is obstructed, and no clipping, floating geometry, z-fighting or other S1/S2 furnishing
defect is visible. The normal clear-day hall is still dim; final baseline readability remains M5.

Rule R2 now selects `Z-L1`, with fourteen accessible cells still below target versus twelve each
in `Z-B1` and `Z-L2`. Next: `HOUSE-01002`, furnishing `L1_BED2` and `L1_BED5`.

## Round 118 — first-floor double-bedroom dressing

Commit: `HOUSE-01002` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-01002-l1-double-day) set covers all six fixed `Z-L1` views.
Together with the fixed bedroom frame, supplemental forced-light
[`detail`](captures/house-01002-l1-double-detail) views inspect the opposite side of the
`L1_BED2` bed group and the compact `L1_BED5` composition reciprocally; these are placement
evidence, not a claim that M5's normal day/night balance is complete.

Both secondary rooms reuse the same bounded `R-BED-DOUBLE` recipe: one acquired double bed and the
generated nightstand, wardrobe and dresser. `L1_BED2` uses its wider west bay for the bed group and
the east wall for both carcasses. In `L1_BED5`, the bed remains central, the carcasses occupy the
short west segments and the 0.75-scale dresser preserves the east-side route between both doors.
The fixed view and reciprocal details show all pieces grounded, separated from one another and
clear of windows, leaves and trim. No clipping, floating geometry, z-fighting, blocked route or
other S1/S2 furnishing defect is visible. The dark normal daytime interiors remain M5 work.

Rule R2 now ties `Z-L1`, `Z-L2` and `Z-B1` at twelve accessible cells still below target. The
least-recently worked zone is `Z-L2`, so the next task is `HOUSE-01010`, furnishing `L2_LIBRARY`.

## Round 119 — second-floor library main-tier dressing

Commit: `HOUSE-01010` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-01010-l2-library-day) and
[`forced-light`](captures/house-01010-l2-library-lit) sets cover all six fixed `Z-L2` views. A
supplemental reciprocal forced-light view from the clear east aisle directly inspects the complete
reading group and both shelving walls; forced light is placement evidence, not a claim that M5's
normal day/night balance is complete.

Five repeated cases and five bounded book clusters make the south/east shelving composition. The
paired armchairs and side table sit wholly on the rug with a clear route around them, while the
desk and open curtain keep the north-window bay readable. The fixed and reciprocal views show all
pieces grounded, separated from doors and trim, and visibly free of clipping, floating geometry
and z-fighting. Sparse shelf dressing, lamps, plants and art are deliberately not added in M4;
those are the bounded H5 C5 work in `HOUSE-03454`.

Rule R2 now ties `Z-B1` and `Z-L1` at twelve accessible cells still below target. The least-
recently worked zone is `Z-B1`, so the next task is `HOUSE-01021`, equipping its mechanical,
electrical and utility rooms.

## Round 120 — basement plant and service dressing

Commit: `HOUSE-01021` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-01021-b1-plant-day) set retains all four fixed `Z-B1` controls.
Supplemental forced-light [`detail`](captures/house-01021-b1-plant-detail) views inspect the
mechanical, electrical and utility rooms directly; forced light is placement evidence, not a claim
that M5's normal basement balance is complete.

The mechanical view shows both grounded plant bodies, the south-entry water-main run and two
supported duct sections without blocking the north door or window. The electrical view shows the
bounded floor-standing panel cabinet and its two wall/ceiling cable routes outside both door arcs.
The utility view shows both stack-landing pipe sections supported against the west/north service
walls. There is no clipping, floating geometry, z-fighting, blocked route or other S1/S2
furnishing defect. Utility-tier depth stops at this functional composition; cosmetic service-room
dressing is not revived.

Rule R2 now selects `Z-L1`, with twelve accessible cells still below target versus eleven in
`Z-L2` and nine in `Z-B1`. Next: `HOUSE-01003`, furnishing the two children's rooms.

## Round 121 — first-floor single-bedroom dressing

Commit: `HOUSE-01003` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-01003-l1-single-day) set retains all six fixed `Z-L1` controls.
Supplemental forced-light [`detail`](captures/house-01003-l1-single-detail) views inspect both
rooms in reciprocal directions; forced light is placement evidence, not a claim that M5's normal
day/night balance is complete.

Both secondary rooms stop at the cumulative `R-BED-SINGLE` recipe: one single bed, nightstand,
wardrobe, desk and reused dining chair. The child's 0.90-scale bed and compact north/south wall
composition leave all four cardinal route probes clear. The larger teenager's room uses its
existing cooler palette and a different arrangement rather than optional clutter. The reciprocal
views show grounded bed, work and storage groups, clear windows and door approaches, and no
furniture clipping, floating geometry or z-fighting. A west-facing supplemental view exposes an
existing exterior foliage card visually beyond the glazing; this is a `Z-L1` S3 backlog item for
M11's bounded defect pass, not a regression or reason to expand M4.

Rule R2 now selects `Z-L2`, with eleven accessible cells still below target versus ten in `Z-L1`
and nine in `Z-B1`. Next: `HOUSE-01011`, furnishing `L2_GAMES` and `L2_SITTING`.

## Round 122 — second-floor leisure-room dressing

Commit: `HOUSE-01011` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-01011-day) and
[`forced-light`](captures/house-01011-lit) sets retain all six fixed `Z-L2` controls. Supplemental
forced-light [`reciprocal`](captures/house-01011-detail) views inspect the games and sitting
compositions from their clear east-side standing points; forced light is placement evidence, not
a claim that M5's normal day/night balance is complete.

Both secondary rooms stop at their bounded cumulative recipes. The games room reuses the acquired
pool table, two dining chairs and a side table; the sitting room reuses two dining chairs, a side
table, floor lamp and rug. The fixed and reciprocal views show grounded, separated furniture,
clear doors and windows, and no clipping, floating geometry or z-fighting. One reciprocal sitting
view also exposes existing exterior foliage cards beyond the glazing; this is a `Z-L2` S3 item for
M11's bounded defect pass, not a furnishing regression. Arcade machines, a dartboard, jigsaw,
record player and extra clutter remain outside the reduced secondary-room requirement.

Rule R2 now selects `Z-L1`, with ten accessible cells still below target versus nine each in
`Z-L2` and `Z-B1`. Next: `HOUSE-01006`, furnishing `L1_BATH2`, `L1_BATH3`, `L1_WC3` and
`L1_WC4`.

## Round 123 — first-floor family/guest wet-room dressing

Commit: `HOUSE-01006` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-01006-day) set retains all six fixed `Z-L1` controls. Supplemental
forced-light [`detail`](captures/house-01006-detail) views inspect the four compact rooms directly;
forced light is placement evidence, not a claim that M5's normal day/night balance is complete.

The two bathrooms stop at bath/shower, vanity, toilet, mirror and towel rail; the two WCs reuse the
same vanity, toilet, mirror and rail family. Direct and reciprocal views show grounded fixture
groups, supported wall pieces and clear required routes without clipping, floating geometry or
z-fighting. The open WC3 door occludes part of its toilet from the central pose, so its reciprocal
view is read together with the exact door-sweep validator rather than moving a valid fixture for a
screenshot. No plumbing behaviour, clutter or bespoke variants were added.

Rule R2 now ties `Z-B1` and `Z-L2` at nine accessible cells still below target and selects the
least-recently worked `Z-B1` (Round 120 versus Round 122). Next: `HOUSE-01023`, furnishing
`B1_CINEMA` and `B1_WC7`.

## Round 124 — basement cinema and WC dressing

Commit: `HOUSE-01023` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-01023-b1-day) and final
[`forced-light`](captures/house-01023-b1-lit-final) sets retain all four fixed `Z-B1` controls.
Supplemental reciprocal cinema and hall-side WC views in
[`detail`](captures/house-01023-b1-lit) expose both seating rows, the screen/projector pair and the
compact fixture group; forced light is placement evidence, not a claim that M5's normal day/night
balance is complete.

The cinema stops at its required main-tier set: one static screen, one static projector, two
four-seat rows and four reused upholstered wall panels. The WC reuses the established toilet,
vanity, mirror and towel rail. Initial renders exposed the thin screen and panels buried within the
15 cm structural wall thickness; placing them on the measured interior faces makes every required
piece readable without adding an asset or runtime system. The final fixed and reciprocal views
show grounded, separated seats, a clear side aisle, correctly supported wall pieces and visible WC
fixtures without clipping, floating geometry, z-fighting or a blocked route. Playback, posters,
clutter and C5 hero dressing remain outside this task.

Rule R2 now selects `Z-L2`, with nine accessible cells still below target versus seven in `Z-B1`
and six in `Z-L1`. Next: `HOUSE-01013`, furnishing `L2_BED6` and `L2_BED7`.

## Round 125 — second-floor bedroom dressing

Commit: `HOUSE-01013` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-01013-l2-bedrooms-day) and
[`forced-light`](captures/house-01013-l2-bedrooms-lit) sets retain the six fixed `Z-L2` controls.
The latter also contains reciprocal BED6 and paired BED7 views; forced light is placement evidence,
not a claim that M5's normal day/night balance is complete.

`L2_BED6` reads as a compact guest/sewing room through the reused small-double frame,
nightstand/wardrobe, writing desk, chair and sewing-storage chest. `L2_BED7` has the bounded
four-piece double-bedroom recipe. The fixed and reciprocal views show grounded, separated pieces,
open doors/windows and continuous standing routes without clipping, floating geometry or
z-fighting. The small-double arrangement also keeps the north-wall collision push-out clear. A
bespoke dress form, optional decoration and deeper bedroom polish were not added.

Rule R2 now ties `Z-B1` and `Z-L2` at seven accessible cells below target and selects the
less-recently worked `Z-B1` (Round 124 versus Round 125). Next: `HOUSE-01024`, furnishing
`B1_GYM` and `B1_WORKSHOP`.

## Round 126 — basement gym and workshop dressing

Commit: `HOUSE-01024` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-01024-b1-day) and
[`forced-light`](captures/house-01024-b1-lit) sets retain all four fixed `Z-B1` controls. Direct
south-to-north gym and workshop views in [`detail`](captures/house-01024-b1-detail) expose the
complete mirror/bench/rack and paired-bench compositions. Forced light is placement evidence, not
a claim that M5's normal day/night balance is complete.

The gym stops at its secondary recipe: repeated generated mirror panels, a reused bench and
scaled shelf/box forms leave the required open exercise floor. The workshop reaches its main-tier
recipe with the retained and repeated workbenches, open shelf, toolbox, bounded tool/paint fill,
wall service run and bin. The first collidable mirror placement exposed a north-wall push-out
pocket; the final thin wall panels rely on the structural wall's collision and create no separate
unreachable sliver. Fixed, direct and reciprocal views show grounded pieces, supported clutter,
clear openings and no clipping, floating geometry or z-fighting. Exercise-machine acquisitions,
bespoke workshop pieces and cosmetic hero dressing were not added.

Rule R2 now selects `Z-L2`, with seven accessible cells below target versus six in `Z-L1` and five
in `Z-B1`. Next: `HOUSE-01014`, furnishing `L2_BATH4`, `L2_BATH5`, `L2_WC5` and `L2_WC6`.

## Round 127 — second-floor wet-room dressing

Commit: `HOUSE-01014` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-01014-l2-day) set retains all six fixed `Z-L2` controls. Direct
forced-light views in [`detail`](captures/house-01014-l2-detail) inspect the shower, bath and both
compact WC arrangements; forced light is placement evidence, not a claim that M5's normal
day/night balance is complete.

Both bathrooms stop at the secondary recipe's bath-or-shower, vanity, toilet, mirror and towel
rail; both WCs stop at the corresponding four-piece set. Direct and reciprocal views show the
grounded fixture bodies, clear central floor and separated wall groups without clipping, floating
geometry or z-fighting. Open leaves deliberately occlude parts of the small rooms in individual
frames, so the views were reviewed together with the exact door-sweep validator. No plumbing
behaviour, clutter or bespoke variant was added.

Rule R2 now selects `Z-L1`, with six accessible cells below target versus five in `Z-B1` and three
in `Z-L2`. Next: `HOUSE-01007`, dressing the first-floor stores and balconies.

## Round 128 — first-floor utility and balcony dressing

Commit: `HOUSE-01007` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-01007-l1-day) set retains all six fixed `Z-L1` controls. Reciprocal
forced-light and clear-day views in [`detail`](captures/house-01007-l1-detail) inspect both
balcony compositions and both hall-closet shelves. Forced groups are placement evidence, not a
claim that M5's normal day/night balance is complete.

Each secondary balcony stops at one reused outdoor lounger and side table, with a clear door and
standing route. Each utility closet stops at its essential shelf, while the store adds one box.
The balcony pieces and closet shelves are grounded and separated without clipping, floating
geometry or z-fighting. The linen/store pair is only 0.90 m deep and its open leaves occlude useful
room-wide camera views; their placement therefore relies on the exact support, overlap,
door-sweep and route validator plus the grand tour, rather than claiming more from the partial
frames. No optional planter, clutter or unique variant was added.

Rule R2 now selects `Z-L0S`, with eight accessible cells below target versus five each in `Z-B1`
and `Z-STAIR`. Next: `HOUSE-00991`, stocking `L0_PANTRY`.

## Round 129 — ground-floor pantry stocking

Commit: `HOUSE-00991` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-00991-l0s-day) set retains both fixed `Z-L0S` controls, and the
reciprocal [`detail`](captures/house-00991-l0s-detail) views inspect the compact pantry directly.
The authored pantry light was forced for the direct pair as placement evidence, not as a claim
that M5's normal day/night balance is complete.

Two repeated open shelves form the bounded utility composition, with the existing generated
crockery-and-jars fill supported on the south shelf. The direct pair shows grounded and separated
shelves, supported fill, clear openings and no clipping, floating geometry or z-fighting. The
room remains notably dark even with its group forced on; that is retained as M5 work. No extra
clutter, bespoke variant, material family or butler's-pantry rework was added.

Rule R2 keeps `Z-L0S` selected, with seven accessible cells below target versus five each in
`Z-B1` and `Z-STAIR`. Next: `HOUSE-00994`, furnishing the office and west closet.

## Round 130 — ground-floor study and west closet

Commit: `HOUSE-00994` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/house-00994-l0s-day) set retains both fixed `Z-L0S` controls, while the
direct reciprocal [`detail`](captures/house-00994-l0s-detail) views inspect the complete study
group and compact closet shelf. The relevant groups were forced for placement evidence, not as a
claim that M5's normal day/night balance is complete.

The study's acquired desk, reused dining chair, generated bookcase and desk-lamp form are
grounded, separated and clear of both tall windows and the two doorways. The utility closet stops
at one scaled shelf in a clear corner. The first centred bookcase and larger closet shelf exposed
two cardinal push-out pockets; the final corner placements remove both without weakening a test.
The renders and exact validator show no clipping, floating geometry, z-fighting or blocked route.
Dark Basic-lit furniture and closet readability remain M5 work; no decorative study tier or
closet clutter was pulled forward.

Rule R2 now ties `Z-L0S`, `Z-B1` and `Z-STAIR` at five accessible cells below target. Its
least-recently worked tie-break selects `Z-STAIR` (Round 117). Next: `HOUSE-03343`, dressing the
stair halls and landings.

## Round 131 — stair halls and landings

Commit: `HOUSE-03343` working tree (`2026-09-24`). The local, Git-ignored final
[`clear-day`](captures/house-03343-stairs-day-final) and
[`forced-light`](captures/house-03343-stairs-lit-final) sets retain all nine fixed `Z-STAIR`
views. Supplemental direct landing views inspect the two benches and three flat runners. Forced
groups are placement evidence only; normal day/night readability remains M5 work.

All six stair cells now have their recipe's reused framed art, and the broad L0 and attic
landings add one grounded bench each. The main landing surfaces use bounded non-colliding runner
props. The two principal flights also receive the finish schedule's continuous 760 mm wool
runner over every tread and riser, with 170 mm of oak exposed at each side. Generated-geometry
checks confirm the runner is a separate physical render surface while collision remains the
unchanged full-width ramp. The fixed and direct views show clear flights, openings and landing
routes without a new clipping or floating defect. The known dark basement/attic views are not
misreported as complete lighting.

Rule R2 now ties `Z-B1` and `Z-L0S` at five accessible cells below target. `Z-B1` was worked less
recently (Round 126 versus Round 130), so the exact next unblocked MUST task is `HOUSE-01025`,
dressing the remaining basement storage, cellar, laundry and hobby rooms.

## Round 132 — remaining basement rooms

Commit: `HOUSE-01025` working tree (`2026-09-24`). The local, Git-ignored
[`clear-day`](captures/aae385e-house-01025-day) set retains all four fixed `Z-B1` controls, and
its [`details`](captures/aae385e-house-01025-day/details) inspect every changed cell with the six
relevant light groups forced. Forced light is placement evidence, not a claim that M5's normal
day/night balance is complete.

The two stores stop at one open shelf and one box each. The cellar's two shelf runs support its
crockery fill and leave its crate and doorway clear. The narrow laundry shows distinct light and
dark static bodies without appliance behaviour. The hobby room's desk/chair and tool shelf occupy
opposite wall bays around a clear central route. Full-resolution views show grounded, separated
pieces, supported fills, clear openings and no clipping, floating geometry or z-fighting. The
first detail capture exposed that the measured `chunks.bin` had not yet been copied into the build
content root; the existing `cnahouse_world_content` target deployed it and all views plus traversal
checks were repeated against the corrected content.

Rule R2 now ties `Z-L3`, `Z-L0S` and `Z-EXR` at five accessible cells below target. Its
least-recently worked tie-break selects `Z-L3` (Round 110). Next: `HOUSE-01016`, furnishing the
finished attic room.

## Round 133 — finished attic room

Commit: `HOUSE-01016` working tree (`2026-09-24`). The local, Git-ignored
[`before`](captures/fac98be-house-01016-before) and
[`forced-light`](captures/fac98be-house-01016-after-lit) sets retain all four fixed `Z-L3`
controls and add reciprocal/direct room views plus a double-dormer curtain detail. Forced light is
placement evidence only; normal attic readability remains M5 work.

The old leather sofa and side table form a grounded sitting corner on the rug, while the desk,
chair and bookcase make a separate work corner without narrowing the three store routes. A chest,
full-size box and bounded small toy-box/children's-book cluster provide the reduced plan's signs of
habitation without reviving its removed toys fill kit. The scaled open treatment fits the two west
dormer windows and keeps their glazing exposed. Full-resolution views show grounded and separated
pieces, supported small props, clear openings and no clipping, floating geometry or z-fighting.

Rule R2 now ties `Z-L0S` and `Z-EXR` at five accessible cells below target. Its least-recently
worked tie-break selects `Z-EXR` (Round 111 versus Round 130). Next: `HOUSE-03342`, dressing the
shed, side yards, orchard corner and vegetable garden.

## Round 134 — rear and side exterior dressing

Commit: `HOUSE-03342` working tree (`2026-09-24`). The local, Git-ignored
[`before`](captures/43fdd77-house-03342-before),
[`clear-day`](captures/43fdd77-house-03342-after) and
[`forced-shed-light`](captures/43fdd77-house-03342-after-lit) sets retain the established broad
rear controls and add direct shed, potting-area and orchard-service views. Forced shed light is
placement evidence only; normal day/night readability remains M5 work.

The shed's workbench, supported tools, shelf, long-handled tools, bin, small pots and compact
mower abstraction are grounded and separated around a clear centre. The garden's potting group
and tools remain clear of the beds and fence. The side-yard service objects retain their walkable
routes, and the orchard's deliberately low-detail cart/wheelbarrow silhouette stops at the reused
kit rather than adding a one-off asset. Full-resolution frames show no clipping, floating
geometry, z-fighting or broad-view regression. Utility-tier side yards are covered by the zone
walk and exact validator as required by their tier, not promoted to dedicated polish views.

Rule R2 now selects `Z-L0S`, with five accessible cells below target versus four in `Z-L3` and
three in `Z-L2`. Next: `HOUSE-00995`, furnishing `L0_MUDROOM` and `L0_LAUNDRY`.

## Round 135 — mudroom and laundry furnishing

Commit: `HOUSE-00995` working tree (`2026-09-24`). The local, Git-ignored
[`final`](captures/9cab730-house-00995-final) set retains both fixed `Z-L0S` controls and adds a
direct laundry view. The mudroom and laundry groups were forced for placement evidence, not as a
claim that M5's normal day/night balance is complete.

The mudroom's scaled bench and supported cubbies occupy the short north bay, while its tall
storage and bin occupy the south bay without blocking the garage door. The laundry stops at two
static material variants of the acquired front-loading body. The first review exposed two stale
evidence problems rather than content defects: the runtime content copy predated the source JSON,
and `build_chunks.py --report` intentionally does not write its output. The final set was repeated
after a real write and deployment; the runtime then reported the measured 1,064 chunks. It also
exposed the inherited zero-tint `MAT_APPLIANCE_STEEL` as an unreadable black override, so the two
new affected props now reuse the existing visible steel-hardware Basic material. Final frames show
grounded and separated pieces, clear openings and no clipping, floating geometry or z-fighting.

Rule R2 now selects `Z-L3`, with four accessible cells below target versus three each in `Z-L0S`
and `Z-L2`. Next: `HOUSE-01017`, dressing the long-term attic stores.

## Round 136 — long-term attic storage

Commit: `HOUSE-01017` working tree (`2026-09-24`). The local, Git-ignored
[`final`](captures/dc557d7-house-01017-final) set retains the four fixed `Z-L3` controls and adds
direct views of the north, south and west stores plus the stair-head strip. Relevant attic groups
were forced for placement evidence only; normal attic readability remains M5 work.

The west store's old wardrobe, second low shelf, suitcase and box read as accumulated storage while
leaving the boarded centre route open. The north and south stores stop at low shelving and small
box groups under the roof slope. One scaled shelf and box use the narrow floor strip beyond the
stair opening without narrowing the stair route. Full-resolution frames show grounded and
separated pieces, low-headroom context, clear openings and no clipping, floating geometry or
z-fighting.

Rule R2 now ties `Z-L0S` and `Z-L2` at three accessible cells below target. Its least-recently
worked tie-break selects `Z-L2` (Round 127 versus Round 135). Next: `HOUSE-01015`, dressing the
second-floor stores and attic-stair landing.

## Round 137 — second-floor utility storage

Commit: `HOUSE-01015` working tree (`2026-09-24`). The local, Git-ignored
[`final`](captures/2c9c3e5-house-01015-final) set retains all six fixed `Z-L2` views and adds
direct views of the two hall closets and both ends of the long store. Relevant service groups were
forced for placement evidence only; normal readability remains M5 work.

Each utility closet stops at one grounded open shelf, mirroring the already accepted first-floor
recipe. The three-door store keeps its required through-route between the games room, sitting room
and bathroom while placing one low shelf along the south edge and one box at the closed east end.
The existing attic-stair bench and wall art remain its complete secondary-tier landing set. Full-
resolution frames and exact placement checks show clear openings and no clipping, floating
geometry or z-fighting; the fixed zone views show no broad regression.

Rule R2 now selects `Z-L0S`, with three accessible cells below target versus one each in `Z-L3`
and `Z-GAR`. Next: `HOUSE-00996`, furnishing the two ground-floor WCs and dressing `L0_STOR` and
`L0_STAIR_MAIN`.

## Round 138 — ground-floor WCs and hall storage

Commit: `HOUSE-00996` working tree (`2026-09-24`). The local, Git-ignored
[`fixed`](captures/house00996-round138) set retains both fixed `Z-L0S` controls; the
[`direct`](captures/house00996-round138-direct) set covers both WCs, the hall store and the retained
stair landing. Relevant room groups were forced for placement evidence only; normal readability
remains M5 work.

Both compact WCs reuse the established toilet, vanity, mirror and towel rail without variants or
behaviour. The hall store stops at the utility recipe's open shelf and box. The main stair keeps
the already accepted bench, runner and wall art rather than receiving another polish pass.
Full-resolution frames and exact placement checks show grounded, separated pieces, clear doors and
routes, and no clipping, floating geometry or z-fighting. The fixed zone views show no broad
regression.

Rule R2 ties `Z-GAR` and `Z-L3` at one accessible cell below target and selects the least-recently
worked `Z-GAR` (Round 108 versus Round 136). `HOUSE-00998` depends on the car family, so next is
`HOUSE-00847`, building and placing that bounded family for the garage and street.

## Round 139 — bounded vehicle family

Commit: `HOUSE-00847` working tree (`2026-09-24`). The local, Git-ignored
[`garage`](captures/house-00847-garage-r139) set covers the household estate in the right bay and
the retained loft access; the [`street`](captures/house-00847-street-r139) set contains the fixed
`Z-STR` view plus direct runtime views of all three estate tints and the delivery van.

The family is deliberately low-detail: one five-door estate body reused under blue, red and silver
paint, plus one boxy white van that shares its wheels, glazing, lamps and construction. Runtime
inspection shows every body grounded and recognizable with separated tyre/glass silhouettes and
no clipping, floating pieces or z-fighting. The fixed street view reads the red parked estate
through the retained vegetation band; direct views prove the other variants without pretending
that the existing hedge is transparent. The garage pair proves the blue household car remains
clear of the ladder and guard. Final normal-light balance remains M5 work.

Rule R2 keeps `Z-GAR` selected: its household-car prerequisite is now complete, while its main bay
and loft still need their bounded tier recipes. Next: `HOUSE-00998`, furnishing `L0_GARAGE` and
`L0_GARAGE_LOFT` from the existing kit.

## Round 140 — garage and loft furnishing

Commit: `HOUSE-00998` working tree (`2026-09-24`). The local, Git-ignored
[`garage`](captures/house-00998-garage-r140) set contains the two fixed `Z-GAR` controls plus
targeted diagnostic views of the workbench wall and loft storage.

The garage's existing household estate remains the main bay anchor. A reused workbench, supported
tools, open shelf, toolbox, long-handled garden tools and paired bins now make the bay read as a
working domestic garage while preserving the vehicle and door routes. Above it, varied boxes, a
seasonal suitcase, spare-lumber abstraction and three reused loungers form compact long-term
storage along the outer edges. The fixed loft-access view shows the hatch, ladder and guard remain
clear. Full-resolution review found no clipping, floating objects, z-fighting or blocked access.
Final normal-light balance remains M5 work.

Rule R2 now selects `Z-L3`, the only zone with a zero-prop accessible cell. Next:
`HOUSE-01018`, equipping `L3_STORE_E` with its bounded utility services from the existing kit.

## Round 141 — east attic services

Commit: `HOUSE-01018` working tree (`2026-09-24`). The local, Git-ignored
[`attic-services`](captures/house-01018-attic-services-r141) set contains a direct normal-light
view and targeted diagnostic views down the room and across both service walls.

The low header tank sits under the east eaves; two joined duct sections use the high west side;
two cable sections form one continuous wall run; and the slender internal aerial mast stays outside
the walking strip. The first diagnostic side view exposed both cable rows inside the wall thickness;
moving their mounting plane 110 mm to the measured inner face makes the run physically visible
without floating it. Full-resolution review shows every piece grounded or mounted beneath the roof,
both openings and the middle route clear, and no clipping, z-fighting or unintended intersections.
Final normal-light balance remains M5 work.

All accessible rooms now have at least one prop. M4's exact next unblocked MUST task is
`HOUSE-00770`, placing the bounded front exterior service props before checkpoint `HOUSE-03380`.

## Round 146 — automatic occupied-house light schedule

Commit: `HOUSE-03401` working tree (`2026-09-24`). The local, Git-ignored
[`Z-L1` night set](captures/house-03401-schedule-r146) contains the six established fixed views at
22:00 in clear weather. It was captured without `--light-on` or `--light-off`: the first-floor
hall and landing, master bedroom and bathroom, second bedroom and front balcony are visibly driven
by their authored automatic classes and stable per-group offsets.

The review demonstrates that a normal night run no longer needs hand-picked light flags. It does
not promote the zone to the final C3 lighting baseline: the deliberately bounded room/flight
luminance work remains in `HOUSE-03403` through `HOUSE-03407`. Exact schedule coverage, timing and
test evidence is in
[`house-03401-light-schedule.md`](house-03401-light-schedule.md).

M5's declared dependency order next selects `HOUSE-03403`, lighting the basement, attic and
stairwells to C3.
