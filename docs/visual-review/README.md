# Visual review ledger

These captures answer “does the playable house look good?”, not “did a pixel change?”. Run
`python3 tools/visual/capture_review.py <short-head>-<round>` from the repository root. The tool
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

## Round 9 — outdoor production material parity

Commit: `HOUSE-00923` checkpoint (`2026-09-15`)

Before: [Round 8 eight fixed gameplay views](captures/house-01280-detail-r2).
After: [same eight gameplay views with the outdoor surfaces restored](captures/house-00923-outdoor-r1).

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

Before: [Round 9 fixed gameplay views](captures/house-00923-outdoor-r1).
After: [same eight views with glass composition corrected](captures/house-00924-glass-r1).

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

Before: [Round 10 fixed gameplay views](captures/house-00924-glass-r1).
After: [same eight views with receiver-domain exposure corrected](captures/house-00925-exterior-exposure-r1).

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

Before: [Round 11 fixed gameplay views](captures/house-00925-exterior-exposure-r1).
After: [same eight views with exterior-window roles](captures/house-00926-windows-r1).

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

Before: [Round 12's eight fixed gameplay views](captures/house-00926-windows-r1).
After: [the same eight views](captures/house-00927-siding-r1), plus an identical
[close approach before](captures/house-00927-siding-r1/exterior-approach-close-before.png) /
[after](captures/house-00927-siding-r1/exterior-approach-close-after.png) pair.

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

Before: [Round 13's eight fixed gameplay views](captures/house-00927-siding-r1) and its
[exact close front](captures/house-00927-siding-r1/exterior-approach-close-after.png).
After: [the same eight views](captures/house-00928-entry-r1) and the
[same close front](captures/house-00928-entry-r1/exterior-approach-close.png). The close approach
was also inspected [overcast](captures/house-00928-entry-r1/exterior-approach-overcast.png) and
[at 22:00](captures/house-00928-entry-r1/exterior-approach-night.png).

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

Before: [Round 14's eight fixed normal-game views](captures/house-00928-entry-r1).
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
frames were actually inspected. The first [steel-front hall](captures/house-01042-rejected-r1/central-hall.png)
and [kitchen](captures/house-01042-rejected-r1/kitchen.png) looked nearly as black as
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
poses and fixed conditions in [clear](captures/house-00772-clear-day-r2),
[noon](captures/house-00772-noon-r2),
[overcast](captures/house-00772-overcast-r2) and
[22:00](captures/house-00772-night-r2). The first
[clear iteration](captures/house-00772-clear-day-r1) is retained as rejected
evidence: oversized property trees hid the central entrance rather than framing it.

Ranked visible defects remaining:

1. The [night first view](captures/house-00772-night-r2/exterior-front.png) is
   still almost black while cutout foliage catches too much residual sky light.
   The porch lanterns and entrance hierarchy do not yet read as a destination.
2. The [clear front](captures/house-00772-clear-day-r2/exterior-front.png) is now
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

Before: Round 22's eleven-view fixed [22:00 set](captures/house-00772-night-r2).
After: the same eleven cameras, time, weather, exposure and Tier-S/High settings in
[the dusk-sensor set](captures/house-01269-night-r2). The day sets are intentionally
unchanged because the automatic groups are exactly off in daylight. The
[front before](captures/house-00772-night-r2/exterior-front.png) and
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
