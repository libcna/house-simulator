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
`HOUSE-00935` adds a twelfth pose on the driveway, looking squarely at the garage frontage; the
prior cameras stay unchanged and remain the exact comparison set for the main route.
`HOUSE-01050` adds the fourteenth, front-on family-media pose after the intervening exterior-approach
review camera; `HOUSE-01051` retains all fourteen unchanged for an exact furnishing comparison.

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

## Round 29 — finished canonical covered porch

Commit: `HOUSE-00929` checkpoint (`2026-09-16`; exact HEAD in `docs/handoff.md`).

Before: Round 28's exact [day approach](captures/house-01262-point-light-r1/exterior-approach-day-check.png)
and [close night entrance](captures/house-01262-point-light-r1/exterior-approach-close.png). After:
the matching [day](captures/house-00929-porch-r1/exterior-approach-day.png) and
[night](captures/house-00929-porch-r1/exterior-approach-night.png), plus the complete thirteen-frame
[Round 29 set](captures/house-00929-porch-r1). All were opened at original resolution.

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

Before: Round 29's matching [day](captures/house-00929-porch-r1/exterior-approach-day.png) and
[night](captures/house-00929-porch-r1/exterior-approach-night.png) approaches showed the front
walk, grass and foyer through the nominally closed entrance. After: the identical
[day](captures/house-00930-entry-door-r1/exterior-approach-day.png) and
[night](captures/house-00930-entry-door-r1/exterior-approach-night.png) cameras show an opaque
textured hardwood leaf, while the fixed
[reverse foyer view](captures/house-00930-entry-door-r1/foyer-facing-front.png) retains its lit
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

Before: Round 30's matching [day](captures/house-00930-entry-door-r1/exterior-approach-day.png)
and [night](captures/house-00930-entry-door-r1/exterior-approach-night.png) approaches put one broad
painted block across the landing windows. After: the identical
[day](captures/house-00931-balustrade-r1/exterior-approach-day.png) and
[night](captures/house-00931-balustrade-r1/exterior-approach-night.png) cameras expose the facade,
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

Before: Round 31's matching [day](captures/house-00931-balustrade-r1/exterior-approach-day.png) and
[night](captures/house-00931-balustrade-r1/exterior-approach-night.png) approaches showed a dark,
flat timber rectangle. After: the identical [day](captures/house-00932-entry-detail-r1/exterior-approach-day.png)
and [night](captures/house-00932-entry-detail-r1/exterior-approach-night.png) cameras show a four-panel
entry with a lock stile. The new [close day](captures/house-00932-entry-detail-r1/entry-door-close-day.png),
[close night](captures/house-00932-entry-detail-r1/entry-door-close-night.png) and
[foyer-side](captures/house-00932-entry-detail-r1/foyer-facing-front.png) frames verify both faces;
the [complete Round 32 set](captures/house-00932-entry-detail-r1) contains all eleven fixed clear-day
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

Before: Round 32's fixed [night approach](captures/house-00932-entry-detail-r1/exterior-approach-night.png)
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

Before: Round 32's clear-day [front](captures/house-00932-entry-detail-r1/exterior-front.png) used
plain blue-grey rectangles around the completed entrance. After: the identical
[front camera](captures/house-00933-front-windows-r1/exterior-front.png) shows a repeated white
6-over-6 grille rhythm and paired black shutters across the canonical front elevation. The
[foyer-side view](captures/house-00933-front-windows-r1/foyer-facing-front.png) confirms that the
new outside detail does not intrude through the entrance or expose closed-room trim. The complete
[Round 34 set](captures/house-00933-front-windows-r1) contains all eleven fixed clear-day route and
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

Before: Round 34's [front](captures/house-00933-front-windows-r1/exterior-front.png) ended both the
main and garage siding planes at a thin dark roof edge, and its five wall dormers read as grey
shingle boxes. After: the identical [front camera](captures/house-00934-roofline-r1/exterior-front.png)
shows a continuous 280 mm white frieze and projecting 100 mm crown, with sided dormer fronts and
painted corner/header/rake outlines. The complete
[Round 35 set](captures/house-00934-roofline-r1) contains all eleven fixed clear-day route and
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

Before: Round 35's [front](captures/house-00934-roofline-r1/exterior-front.png) looks through the
entire 4.90 × 2.40 m garage aperture because its closed room-owned leaf used ordinary indoor steel
and correctly vanished with `L0_GARAGE`. After: the identical
[front camera](captures/house-00935-garage-door-r1/exterior-front.png) shows a closed warm-charcoal
sectional door, and the new fixed [driveway camera](captures/house-00935-garage-door-r1/garage-approach.png)
makes its five rows of four raised panels directly reviewable. The complete
[Round 36 set](captures/house-00935-garage-door-r1) contains twelve fixed clear-day route,
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

Before: Round 36's fixed [straight living-room view](captures/house-00935-garage-door-r1/living-room.png)
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

Before: the fixed [clear-day dining view](captures/house-01048-dining-before-r1/dining-room.png)
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
[always-on clear-day set](captures/house-01284-range-task-day-r1) visibly darkened the whole room
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

Before: the new fixed [front-on family-media view](captures/house-01050-family-media-before/family-media.png)
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
[explicit-reading 22:00 baseline](captures/house-01285-family-fixtures-night-before) also proves
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
