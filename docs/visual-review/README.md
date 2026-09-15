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
