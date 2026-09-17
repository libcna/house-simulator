# Visual-sprint handoff — 2026-09-17 (`HOUSE-01285` checkpoint)

Branch `develop`. Task-start HEAD `c52b87d` (`HOUSE-01051`). This file belongs to the single
`HOUSE-01285` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The family room no longer obtains light from invisible practical points. One deterministic
project-authored 420 x 180 x 420 mm semi-flush fixture is reused at the four stable main-source
positions. Each physical fixture has dark-bronze trim and a separate opal diffuser, and its source
sits at the diffuser's y 3.12 m optical face with a broad downward 72/140 degree spot, 1200 lm and
3000 K. The visible family floor lamp now has its own byte-identical derived asset identity so its
shade can follow only `LG_L0_FAMILY_READING`; the approved White Room geometry/textures and licence
remain unchanged. The 450 lm reading source moves from the room centre onto that measured shade
and remains off by default.

Before: Round 45's complete [clear-day route](visual-review/captures/house-01051-family-secondary-day-final)
has a blank ceiling, while the complete
[explicit-reading night baseline](visual-review/captures/house-01285-family-fixtures-night-before)
shows a dark lamp shade illuminated from 3.15 m away. After: the complete unchanged fourteen-camera
[normal day set](visual-review/captures/house-01285-family-fixtures-day-final) and
[explicit-reading night set](visual-review/captures/house-01285-family-fixtures-night-final) show
four coherent ceiling practicals plus local warm light originating at the real shade. All six
matched family frames were opened at full resolution. Day normalized MAE is 0.01334–0.02098 and
night is 0.01181–0.01997; direct inspection confirms the broad changed-pixel coverage is the
selected family-room rebake rather than global exposure.

The selected 256-sample main atlas peaks at 0.2292 (formerly 0.1977); the corrected reading atlas
peaks at 0.5822 (formerly 0.1368). The ceiling asset regenerates byte-identically at 512 triangles
with metre scale, UV0, grounded optical origin and exact material slots. Its permanent checker also
guards the copied lamp identity, source hashes, canonical placements, linked slots, cone/flux/range
and switch defaults. Licence credits, budget report and 2,859 stable ids are current.

The built world is 626 chunks / 57 props / 256 exterior hierarchy instances / 53.2886 MB. Only
`L0_FAMILY` rises from its exact 21-chunk exception to 23. The unculled diagnostic is 626 draws /
92 state changes, inside §71.2's 1,400 / 210 row. An initial capture exposed a stale 624-chunk copy
in `build/content`; it was rejected, the CMake content target synchronized the generated world and
compiled both models, and every final frame explicitly reports 626. No long-running navigation
process remains.

No strict render golden required advancement. Unit tests pass 1409/1409, serial offscreen
integration tests 135/135 and all 49 active software-render cases pass. The eighteen-pose
culled-vs-unculled gate remains 0.1320% worst case. All compilation and heavy tooling were pinned
to CPU 0-5 and at most six workers; final complete gate details are recorded in `plan.md`.

The largest visible defects are now the coarse pale sofa/media silhouettes, clipped clear-day
apertures and sparse exterior/garage start framing. Improve the family focal composition or move
to the exterior start view according to the next fixed screenshots; do not add unrelated invisible
infrastructure or misreport this bounded fixture checkpoint as completed `HOUSE-00992`.

---

# Prior visual-sprint handoff — 2026-09-17 (`HOUSE-01051` checkpoint)

Branch `develop`. Task-start HEAD `50d8e45` (`HOUSE-01050`). This file belongs to the single
`HOUSE-01051` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The family room now has a coherent secondary domestic layer rather than a corrected TV surrounded
by blank wall. Four deterministic, project-authored Ms-PL models add a 0.90 x 0.66 m bolster dog
bed, a 1.15 x 1.55 m open bookcase with eighteen non-uniform volumes, a 0.52 m round sofa table
with a reading book, and a 0.92 x 0.64 m framed raised-shape abstract. The existing plant is composed
on the bookcase top, and `BED_DOG_FAMILY` now names the physical bed prop. Wood/cloth reuse approved
close-range finishes; two restrained book/canvas roles supply controlled accent colour.

Before: Round 44's fixed [media wall](visual-review/captures/house-01050-family-media-day-final/family-media.png)
is legible but sparse. After: the identical
[final media view](visual-review/captures/house-01051-family-secondary-day-final/family-media.png),
[reverse composition](visual-review/captures/house-01051-family-secondary-day-final/family-composition.png)
and the directory's complete fourteen-camera route were captured and inspected. The media pair
changes 99,234 pixels (6.89%, normalized MAE 0.00393); the reverse pair changes 46,512 (3.23%,
normalized MAE 0.00209). The first iteration embedded the art in the wall and made the bed too
flat; both were rejected. The first central bookcase placement failed the unchanged wall-recovery
tour and was moved to the measured right-wall bay.

The bookcase source retains a <=64-triangle proxy for reuse, but this flush wall instance relies
on the existing wall collision. Enabling the redundant proxy changed the deterministic long walk
and steered it into the project's already documented main-stair well gap; disabling only that
instance restores the 20-minute walk while the freestanding side table remains collidable. The
full focused camera/inside-geometry/property/cell walk set passes without exceptions.

The built world is 624 chunks / 53 props / 256 exterior hierarchy instances / 53.2100 MB. Only
`L0_FAMILY` rises from seventeen to its exact measured 21-chunk exception. The unculled diagnostic
is 624 draws / 91 state changes, inside §71.2's 1,400 / 210 worst-case row; named visible poses
continue to protect the typical row. Licence credits, content budget and 2,852 stable ids are
regenerated/current. Final complete gate results are recorded in `plan.md`.

No strict render golden requires advancement. The largest visible defect is now flat, dark
family-room lighting around the detailed objects and very bright apertures, followed by the pale
simple sofa/media silhouettes. The broad exterior/garage framing remains the next larger sparse
area. Do not misreport this bounded checkpoint as completed `HOUSE-00992`.

---

# Prior visual-sprint handoff — 2026-09-17 (`HOUSE-01050` checkpoint)

Branch `develop`. Task-start HEAD `9bfc4b7` (`HOUSE-01284`). This file belongs to the single
`HOUSE-01050` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The family-room focal wall no longer presents its approved television as a cream blank rectangle.
Inspection of the source glTF proves `TvScreen` and `TvBevel` deliberately share its sole coarse
palette slot; that role now receives a dark blue-black opaque glass finish with a tight, restrained
highlight. The separate media-unit source's named `BlackMarble` node now receives dark marble while
its painted carcass remains pale. Geometry, transform, collision, portal, light and provenance data
are unchanged.

Before: the new deterministic [front-on view](visual-review/captures/house-01050-family-media-before/family-media.png)
shows both focal roles flattened into cream upholstery. After: the identical
[final view](visual-review/captures/house-01050-family-media-day-final/family-media.png) reads as an
inactive TV above a contrasting stone base. The latter directory is the complete fourteen-camera
clear-day route after adding the focal-wall blind spot to `capture_review.py`; every frame and the
contact sheet were inspected. The matched frame changes 33,574 pixels (2.33%), with mean absolute
normalized RGB difference 0.00461 (about 1.18/255), so the gain is intentionally local rather than
an exposure or global-palette change.

A permanent `family-media-finish` gate protects the exact source nodes, manifest mappings, bounded
dark finishes and canonical prop alignment. The two restored material roles raise only
`L0_FAMILY` from fifteen to its exact measured seventeen-chunk exception. The built world is 620
chunks / 49 props / 256 exterior hierarchy instances / 52.9115 MB. Licence credits, content budget
and stable IDs are regenerated/current; no asset source or licence changes.

No strict render reference changed beyond tolerance. Unit tests pass 1409/1409. The complete
serial offscreen integration suite passes 135/135. All 49 active render tests pass with the eight
capture-only regeneration cases disabled; culled-vs-unculled equivalence remains green. The final
repository gate is green across all 323 strict-XNA translation units. Compilation and heavy tooling
were pinned to CPU 0-5 and no more than six workers.

The family room is still visibly sparse: the pale mantel-like source, blank flanking walls/doors,
isolated bright plant and flat focal-wall lighting now outrank further material tweaking. The next
highest-value task should add a measured secondary composition—side storage/books, a dog bed,
restrained art and one or two human-use objects—without obstructing the family/kitchen route, then
reassess local lighting depth. The broad exterior/garage framing remains the next larger sparse
area. Do not misreport this narrow focal correction as completed `HOUSE-00992`.

---

# Prior visual-sprint handoff — 2026-09-17 (`HOUSE-01284` checkpoint)

Branch `develop`. Task-start HEAD `5be2023` (`HOUSE-01283`). This file belongs to the single
`HOUSE-01284` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The kitchen's nominal under-cabinet group is no longer four invisible ceiling points. One
deterministic project-authored 82 x 18 x 82 mm brushed-steel/frosted puck is reused four times
across the real range-hood filter. Each stable 400 lm, 3000 K source sits 10 mm below its linked
`PuckDiffuser`, aims down through a bounded 50/100 degree spot and stops at 2.40 m. The group
retains its original manual/default-off behavior and exact 1600 lm exposure weight.

Before: explicitly enabling the malformed group creates four white ceiling hot spots in the fixed
[west kitchen](visual-review/captures/house-01284-undercab-before-on/kitchen-facing-west.png) and
[hall-side view](visual-review/captures/house-01284-undercab-before-on/kitchen-from-hall.png).
After: the complete [thirteen-view normal day set](visual-review/captures/house-01284-range-task-day-final)
preserves the bright-day presentation, while the complete
[explicit-on 22:00 set](visual-review/captures/house-01284-range-task-night-on-final) proves the
same real switch now controls hood-mounted sources without ceiling spots. Every frame was opened;
the matching kitchen views were compared at full resolution. The final west-day image differs
from `HOUSE-01283` by mean RGB only (0.0125,0.0126,0.0126)/255, confined to new geometry. A
rejected always-on iteration darkened the whole day room through legitimate exposure adaptation
and was not promoted merely to advertise the new fixture.

The selected 256-sample task atlas peaks at 0.2341 instead of the old ceiling distribution's
8.6039. The authored model regenerates byte-identically at 216 triangles with metre UV0, grounded
optical origin, exact source hash and approved project-authored provenance. Puck steel reuses the
kitchen hardware role; only the independent warm diffuser raises `L0_KITCHEN` from nineteen to
its exact measured twenty-chunk boundary. The world is 618 chunks / 49 props / 256 exterior
instances / 52.9115 MB. A complete incremental world build finished in 23.48 s, with navigation
in 7.20 s; no long-running `build_nav.py` remains.

No strict render reference changed beyond tolerance. Unit tests pass 1409/1409. The integration
suite passes 126 ordinary cases under offscreen SDL; its 8 sandbox-home write cases fail only when
the home directory is read-only, and all 10 `SaveStoreTest` cases pass with the required writable
temporary `XDG_DATA_HOME`. All 48 active render tests pass, including the unchanged eighteen-pose
culled/unculled worst case of 0.1320%. The complete repository gate is green with all 323
strict-XNA translation units clean. All compilation and heavy tooling were pinned to CPU 0-5 and
at most six workers.

The cooking bay remains too dark and blocky even though its source placement is now physically
correct; do not misreport that visual defect as solved. The next highest-value path is either a
narrow stock-XNA-compatible range/material readability correction or, if that cannot make a
clearly better fixed frame without global exposure tricks, restrained family-room secondary
furniture/decor. The broad exterior/garage composition remains the next larger sparse area.

---

# Prior visual-sprint handoff — 2026-09-17 (`HOUSE-01283` checkpoint)

Branch `develop`. Task-start HEAD `820ceaa` (`HOUSE-01049`). This section belongs to the single
`HOUSE-01283` commit; use that commit as its ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The kitchen island now has three real practicals rather than three invisible and incorrectly
placed light points. A deterministic project-authored bronze bell pendant with a separate frosted
diffuser is reused across the island's long x axis. Each existing 2850 K source sits at its physical
shade, links that exact emissive material and aims down through a bounded spot cone. The group
starts on for the normal connected route but retains its authored switch. The selected 256-sample
island bake peaks at 0.1518, replacing the rejected point-light iteration's 0.8003 ceiling-heavy
result without changing global exposure or renderer architecture.

Before: Round 41's fixed [hall-side kitchen](visual-review/captures/house-01049-kitchen-dressing-day-final/kitchen-from-hall.png)
has no physical island lights and little focal depth. After: the identical
[final view](visual-review/captures/house-01283-kitchen-pendants-day-final/kitchen-from-hall.png) and
[reverse kitchen view](visual-review/captures/house-01283-kitchen-pendants-day-final/kitchen-facing-west.png)
show three coherent shades, lit diffusers and a localized warm island pool without an orange ceiling
bloom. Every image in the complete
[thirteen-camera clear-day set](visual-review/captures/house-01283-kitchen-pendants-day-final) was
opened at full resolution; matched temporary 22:00 views were also captured and inspected.

The model regenerates byte-identically and validates at 330 x 875 x 330 mm / 476 triangles with
metre UV0, grounded support origin and two approved project-authored material roles. Three new
stable props bring the world to 617 chunks / 45 props / 256 exterior hierarchy instances; only
`L0_KITCHEN` rises from 17 to its exact measured 19-chunk boundary. A complete replacement world
build finished normally, including navigation in 6.96 s. The old reported 19-hour navigation
process was therefore a historical stuck bake, not a current requirement, and no new zombie was
created.

Only four strict references changed beyond tolerance. The explicit debug blockout kitchen gained
the fixture silhouettes; the production kitchen gained their physical/lighted presentation; and
the main-hall view into the kitchen plus adjacent hall corner reflect the now-live group/exposure.
The corner changes only 0.6589% of pixels with maximum channel delta 3. All four actual/reference
pairs were opened side by side and selectively advanced; no other golden was touched. All
compilation and heavy tooling remain pinned to CPU 0-5 and no more than six workers. Unit tests
pass 1409/1409, integration tests 134/134 and all 48 active software-render cases pass. The
18-pose culled-vs-unculled worst case is 0.1320%, below its unchanged 0.2% limit. The complete
repository gate is green with all 323 strict-XNA translation units clean.

The largest visible defect is now the dark, blocky north cooking bay. Its separate nominal
under-cabinet group is authored at ceiling height and makes ceiling spots when explicitly enabled;
correct and physically represent that group rather than widening the island cones or lifting global
exposure. After that, add restrained family-room secondary detail. The broad exterior composition
also remains sparse beyond its now-coherent materials and practicals.

---

# Prior visual-sprint handoff — 2026-09-17 (`HOUSE-01049` checkpoint)

Branch `develop`. Task-start HEAD `cf76d82` (`HOUSE-01048`). This file belongs to the single
`HOUSE-01049` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The primary kitchen now has human scale and controlled activity cues. Three project-authored
walnut/sage counter stools sit at the island's west side; a cutting board, bowl and produce anchor
the island; canisters dress the sink run; and a kettle identifies the cooking bay. The four source
GLBs are deterministic across clean processes and have checked metre scale, UV0, grounded origin,
material slots and stable ids. The stool has an explicit seat/frame collision proxy and a measured
0.65 m seat height. All models and texture bindings are project-authored or reuse already approved
sources; the generated credit and budget ledgers are current.

Before: Round 40's fixed [hall-side view](visual-review/captures/house-01048-dining-day-final/kitchen-from-hall.png)
shows a bare island and worktops. After: the identical [final view](visual-review/captures/house-01049-kitchen-dressing-day-final/kitchen-from-hall.png)
shows the complete composition, while the [reverse kitchen view](visual-review/captures/house-01049-kitchen-dressing-day-final/kitchen-facing-west.png)
proves the east aisle remains open. The complete
[thirteen-camera clear-day set](visual-review/captures/house-01049-kitchen-dressing-day-final) was
recaptured after final placement and every kitchen view was opened at full resolution.

The first stool placement at z=-23.70 failed the unchanged whole-house tour at the south kitchen
wall. Moving the line 0.25 m under the island to z=-23.95 leaves approximately 0.72 m behind the
seats and passes the original test; no collision tolerance or route exception was added. The
world is now 615 chunks / 42 static props / 256 exterior hierarchy instances. Only `L0_KITCHEN`
rises from 13 to its exact measured 17-chunk exception, corresponding to walnut/board, upholstery,
lemon-produce and black-appliance roles.

Only the two strict kitchen references changed beyond tolerance. Both actual/reference pairs were
opened and the intended stool/decor silhouettes were selectively advanced; all other committed
goldens remain untouched. Direct verification passes 1409/1409 unit tests, 134/134 integration
registrations and all 48 active software-render cases, including the 18-pose culled-vs-unculled
comparison. Both world rebuilds finish normally (navigation 6.93–9.04 s), and all compilation,
tests and heavy tooling were limited to CPU 0–5 / six workers. The complete repository gate is
green with all 323 strict-XNA translation units clean; `git diff --check` is clean.

The screenshot review changed the priority: more countertop clutter is no longer the largest
visible gain. Daytime kitchen/hall lighting is too flat and the north cooking bay remains dark
against bright windows. Next take the shortest dependency-valid lighting/calibration task that
adds believable depth to those fixed cameras while preserving the Tier-S/Tier-E architecture,
then add restrained family/dining secondary detail. Do not lift global exposure, widen unrelated
lights or obscure the now-clear circulation with extra props.

---

# Prior visual-sprint handoff — 2026-09-17 (`HOUSE-01048` checkpoint)

Branch `develop`. Task-start HEAD `43b6b40` (`HOUSE-01047`). This file belongs to the single
`HOUSE-01048` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The connected L0 route no longer crosses an empty formal dining blockout. `L0_DINING` now has a
project-authored 2.35 × 0.98 m walnut breadboard table for eight, eight correctly scaled sage
upholstered chairs, the existing charcoal rug and a physical six-shade bronze chandelier. The
eleven placements retain the permanent kitchen/living openings and the hall/storage portals, with
measured clearance at the table ends. The table and chair have explicit collision proxies; their
deterministic Blender generation, UV0, origins, bounds, material slots, manifest rows and stable
ids are checked by `tools/assets/dining_suite_prepare.py --check` and the permanent `dining-suite`
CI gate.

Before: the fixed [clear-day dining camera](visual-review/captures/house-01048-dining-before-r1/dining-room.png)
shows a dark empty room. The matched [final clear-day view](visual-review/captures/house-01048-dining-day-final/dining-room.png)
shows the complete seating composition and its readable 2700 K practical; the
[final 22:00 view](visual-review/captures/house-01048-dining-night-final/dining-room.png) proves the
same production group drives the exact `ChandelierShade` emission and baked room contribution.
All thirteen views in each canonical set were opened and inspected.

The existing windowless dining chandelier now starts on so a new-game walk from living through
dining to kitchen is readable; its authored wall switch can still turn it off. This is a data
default, not a review-only light or global exposure change. `capture_review.py` now gives every
set a clean temporary XDG profile, so an unrelated interactive save can no longer override
authored defaults and make the fixed review cameras nondeterministic.

The new dining finish boundary raises only `L0_DINING` to its measured ten-chunk exception. The
world is 611 chunks / 36 static props. Only `blockout-l0-kitchen.png` and `fp-l0-kitchen.png` were
intentionally advanced after direct before/actual inspection: each sees the new chair silhouettes
and localized warm spill through the dining opening. All other generated golden changes were
discarded. The complete render suite passes 49/49 active cases, including the unchanged 18-pose
culled-vs-unculled equivalence.

Verification is clean: deterministic asset/content checks and the full repository gate pass, as do
1,409/1,409 unit registrations, 135/135 integration registrations and all 49 active render cases.
The strict-XNA compiler accepts all 323 translation units. Compilation, tests and heavy content
work remained capped to CPU 0–5 / six workers, and `git diff --check` passes.

Next highest visible value is controlled kitchen countertop/island detail, followed by a dining
sideboard/art/table setting and family-room secondary detail. The dining fixture is visibly too
orange/saturated under the current Tier-S 2700 K approximation; tune that locally in later lighting
work rather than removing the real fixture, widening spill or lifting global exposure.

---

# Prior visual-sprint handoff — 2026-09-17 (`HOUSE-01047` checkpoint)

Branch `develop`. Task-start HEAD `f5aedc3` (`HOUSE-01046`). This file belongs to the single
`HOUSE-01047` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The already canonical manual `LG_EXT_DRIVEWAY_FLOOD` is now a complete practical rather than an
unlinked light row. A deterministic project-authored 420 × 203 × 195 mm bronze wall pack sits over
the sectional opening; its physical lens is the source position and its exact `FloodLens` slot
drives the neutral emissive/glow path. The source remains 4000 K, 3000 lm, manual and default off.
It explicitly spills only to `L0_GARAGE` and the `EXT_SIDEYARD_E` terrain chunk that actually owns
the driveway asphalt. The latter follows generated chunk residency, not a misleading logical cell
name.

Before: Round 38's [22:00 driveway](visual-review/captures/house-01046-garage-roof-night-r1/garage-approach.png)
has no visible source and an almost black door/apron. The fixed camera now records the fixture in
[daylight](visual-review/captures/house-01047-garage-flood-day-r1/garage-approach.png), its real
[manual-off night](visual-review/captures/house-01047-garage-flood-night-off-r1/garage-approach.png)
and the production [manual-on state](visual-review/captures/house-01047-garage-flood-night-on-r1/garage-approach.png).
All twelve frames in each set were opened. The lit door and nearby asphalt become readable while
the distant right fence remains dark.

`--light-on=<group>` is a repeatable deterministic visual-review override applied to the genuine
switch group after world construction. Unknown groups fail loudly; normal startup, save state and
the authored manual switch are unchanged. The review wrapper forwards the option. The fixture
assignment now also applies authored full-angle spot cones before ranking stock-XNA directional
approximations. This removed the first iteration's point-like yard flood. A second rejected
fallback had exposed the foyer's ceiling group to the weather-facing front door; the final path
admits only an actual explicit spill contribution.

The deterministic model is 156 triangles and contains no third-party mesh or texture. Provenance,
licence credits and three new stable ids are recorded. Only the two inspected first-person
references that see the corrected default-on kitchen spot cones were advanced; the hall changes
through its kitchen opening. Front-door and serial season references remain unchanged.

The fixture regenerates bit-for-bit and origin, manifest, material, licence, stable-ID and all
content checks pass. The forced 31-stage content build completes in 36.40 s (navigation 7.37 s).
Unit tests pass 1409/1409, integration registrations 135/135, and all 49 active software-render
cases pass, including first-person references and 18-pose culled-vs-unculled equivalence. Full
repository CI is green with 323 strict-XNA translation units clean. Compilation and strict-XNA
work remained capped at six workers throughout this checkpoint.

Next highest visible value is primary dining furniture, followed by controlled kitchen practical
clutter and family-room secondary detail. The remaining dark facade needs its own authored
practical/landscape composition rather than a wider garage cone or global exposure lift. Preserve
the manual group, explicit receiver boundary and physically bounded spot; do not turn it into an
automatic flood or neighbour-by-proximity lighting.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-01046` checkpoint)

Branch `develop`. Task-start HEAD `d76933d` (`HOUSE-01045`). This file belongs to the single
`HOUSE-01046` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The garage's authored +4.30 m head is its settled roof eave, not an ordinary storey ceiling. The
generic exterior-skin rule had nevertheless searched for the next FFL and extended the garage
siding to L2 at +6.55 m, placing a nine-metre-wide wall through `ROOF_GARAGE`. `outer_span` now
adds a floor-structure band only when a cell head matches an actual canonical ceiling. Standard
L0 walls still seal +3.30–+3.65 m; custom roof/stair-bound heads stop at their authored height.
The rule is data-driven and contains no garage identifier.

Before: Round 37's [driveway view](visual-review/captures/house-01045-piano-r2/garage-approach.png)
shows the false brown rectangle. After: the identical [clear 10:30 view](visual-review/captures/house-01046-garage-roof-r1/garage-approach.png)
shows the separate charcoal-shingle hip and its real silhouette. Matching
[clear noon](visual-review/captures/house-01046-garage-roof-noon-r1/garage-approach.png),
[overcast](visual-review/captures/house-01046-garage-roof-overcast-r1/garage-approach.png) and
[clear night](visual-review/captures/house-01046-garage-roof-night-r1/garage-approach.png) captures
were inspected, along with all twelve views in each scenario set.

Only `L0_GARAGE.glb` and `B1_UNDERSTAIR.glb` changed among 99 regenerated source shells. Their UV2
and daylight/artificial atlases were selectively rebuilt and promoted with fresh manifest, bake
and licence provenance. The compiled world remains 603 chunks / 24 props. Fourteen strict golden
references that actually contain the corrected geometry or deterministic UV2 repack were inspected
and selectively advanced; all other references remained untouched. The complete software-render
suite passes 49/49 active cases, including 18-pose culled/unculled equivalence.

Verification is clean: shell selftest and generated-manifest checks, the forced 31-stage content
build, 1,408/1,408 unit tests, 135/135 integration registrations, staged repository gates and all
323 strict-XNA translation units pass. The first sandboxed integration run denied only eight save
tests access to the user-data directory; the complete authorized rerun passed. Compilation was
capped at six workers throughout, and `git diff --check` passes.

The previous 19-hour `tools/world/build_nav.py` PID 936066 was already gone when checked; this
task's complete replacement navigation build finished normally in 10.83 seconds. It was therefore
a stuck historical bake, not required work. No process was killed.

Next highest visible value is a plausible physical garage/front practical: the corrected roof is
readable by day and overcast, but the garage face and most of the facade are almost black at 22:00.
Then furnish dining, add controlled kitchen clutter and family-room secondary detail, and give the
piano wall a restrained artwork/fixture. Preserve the new ceiling-derived span rule and separate
roof; do not reintroduce a facade patch or global night-exposure lift.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-01045` checkpoint)

Branch `develop`. Task-start HEAD `9907d45` (`HOUSE-00935`). This file belongs to the single
`HOUSE-01045` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The formal living room's formerly blank straight sightline now contains its architecture-specified
upright piano. The deterministic project-authored model is 1.485 × 1.240 × 0.733 m, support
grounded, and contains inset casework, a separately modelled 52/36-key keyboard, music desk, legs,
feet and three physical pedals. Its 12,444 visible triangles and 12-triangle collision box map to
four existing approved texture sources through stock `BasicEffect`; no third-party mesh or texture
was introduced. The double foyer leaves swing away into `L0_FOYER`, and the piano remains south of
their portal without narrowing the main living-room circulation below 1.2 m.

Before: Round 36's [straight view](visual-review/captures/house-00935-garage-door-r1/living-room.png)
faces an empty wall. After: the identical [clear-day view](visual-review/captures/house-01045-piano-r2/living-room.png)
has a recognizable walnut focal piece. Matching [clear noon](visual-review/captures/house-01045-piano-noon-r2/living-room.png),
[overcast](visual-review/captures/house-01045-piano-overcast-r2/living-room.png) and
[night](visual-review/captures/house-01045-piano-night-r2/living-room.png) captures were inspected,
as were all twelve frames in the complete [Round 37 clear set](visual-review/captures/house-01045-piano-r2).
An experiment enabling the four main ceiling lights made the room flatter and darker and was
rejected; only the existing 450 lm piano accent starts on, with the main group still off.

The generated world is 603 chunks / 24 static props. `L0_LIVING` is exactly its derived 21-chunk
ceiling over 20 materials; the deliberately unculled diagnostic is 603 draws / 93 state changes,
inside §71.2's worst-case envelope. The only intentionally changed strict reference is explicit
debug `blockout-l0-living`: its 4.6875% difference is the inspected piano silhouette. Production
first-person references remain unchanged, and the 18-pose culling-equivalence suite passes.

Verification is clean: deterministic piano regeneration, a forced 31-stage content build, all
1,408 unit tests, all 135 integration registrations, all 49 active software-render cases and all
323 strict-XNA translation units pass. The one initial `-j6` unit run saw the unrelated sky
recolour microbenchmark's maximum sample delayed to 9.46 ms; it passed immediately in isolation
and the complete `-j2` rerun passed 1,408/1,408. No compilation used more than six workers.

Next highest visible value is the broad, unarticulated garage wall above the finished door. Then
furnish dining, add kitchen practical clutter, deepen family-room detail, give the piano wall a
restrained artwork/physical accent fixture and improve the night facade. Preserve the piano's
measured placement and four meaningful finishes; do not collapse the keys into a texture or turn
its accent into a global living-room exposure lift.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-00935` checkpoint)

Branch `develop`. Task-start HEAD `9723a9d` (`HOUSE-00934`). This file belongs to the single
`HOUSE-00935` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The canonical 4.86 × 2.35 m garage leaf remains owned by `L0_GARAGE`, but its painted body and
panel roles now enter §25.6's exterior hierarchy independently of the culled room. Twenty shallow
closed boxes form five horizontal sections with four raised panels each, on both faces, using two
warm-charcoal variants of the approved fine-paint maps. The aperture, portal, threshold, collision,
closed interaction state and §54 five-segment animation contract do not move.

Before: Round 35's clear-day [front](visual-review/captures/house-00934-roofline-r1/exterior-front.png)
looks through the full garage aperture. After: the identical
[front](visual-review/captures/house-00935-garage-door-r1/exterior-front.png) is closed, while the
new fixed [driveway view](visual-review/captures/house-00935-garage-door-r1/garage-approach.png)
shows the 5 × 4 relief directly. The front changes 13,201 pixels (0.917%) and the strict close
property-drive view changes 7.176%. The full
[Round 36 set](visual-review/captures/house-00935-garage-door-r1) contains twelve fixed clear-day
route/composition/facade cameras and every frame was opened.

The shell is 57,065 triangles, with `L1_LANDING` worst at 2,362/3,500. The deterministic world now
contains 599 chunks and 254 exterior hierarchy instances; `L0_GARAGE` is exactly its documented
10-chunk exception. Ten actually changed strict references were inspected before selective
advancement. All 1,407 unit tests and all 134 serial integration tests pass. All 48 active strict
software-render cases pass, and the 18-pose culled/unculled comparison remains below its unchanged
0.2% threshold at 0.1599% worst. Schema, generator, world/content and repository gates pass with
`CNA_CNAEXT=OFF`.

Next highest visible value is the empty, dark straight living-room camera: the alternate view has
a real seating group, but circulation still sees an undecorated wall/chimney. After that, articulate
the broad garage wall above the finished door, furnish dining, add kitchen practical clutter,
deepen the family-room finish and improve the night facade. Preserve the bounded garage body/panel
roles and room ownership; do not expose general garage trim or replace the spatial leaf with a
facade billboard.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-00934` checkpoint)

Branch `develop`. Task-start HEAD `f1301d2` (`HOUSE-00933`). This file belongs to the single
`HOUSE-00934` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The unchanged canonical main and garage eaves now carry two real finish layers: a 280 mm painted
frieze and a 100 mm projecting crown below the existing fascia/soffit edge. The five settled wall
dormers keep their roof planes, holes and openings; their vertical fronts now use a dedicated
unbaked stock-`BasicEffect` siding variant sourced from the approved wood-board maps, with 65–75 mm
painted corner, header and rake trim. None of this changes the 7:12 pitch, ridge, eave coordinates,
drainage, collision or lightmap receiver set.

Before: Round 34's clear-day [front](visual-review/captures/house-00933-front-windows-r1/exterior-front.png)
ends broad siding planes at thin dark roof edges and shows grey box-like dormers. After: the
identical [front](visual-review/captures/house-00934-roofline-r1/exterior-front.png) has a continuous
layered white cornice across the main and garage masses plus recognizably sided, outlined dormer
gables. The main-eave crop changes 20.219% and the garage-eave crop 13.083%. The full
[Round 35 set](visual-review/captures/house-00934-roofline-r1) contains all eleven fixed clear-day
route/composition cameras and every frame was opened.

The shell is 56,585 triangles, with `L1_LANDING` worst at 2,362/3,500. The deterministic world now
contains 597 chunks and 252 exterior hierarchy instances; the established `EXT_ROAD` 17-chunk
exception is unchanged. Seventeen actually changed strict references were inspected before
selective advancement. All 1,407 unit tests and all 134 serial integration tests pass. The strict
software-render suite passes after the intentional reference advancement, and the 18-pose
culled/unculled comparison remains below its unchanged 0.2% threshold at 0.1599% worst. Schema,
generator, world/content and repository gates pass with `CNA_CNAEXT=OFF`.

Next highest visible value is the broad flat garage/right facade or the almost empty straight
living-room camera. Dining furnishing, kitchen practical clutter, family finish depth and the dark
night facade follow. Preserve the generator-derived eave/dormer finish and the narrow exterior
material role; do not move settled roof geometry or make ordinary indoor wood externally resident.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-00933` checkpoint)

Branch `develop`. Task-start HEAD `20716bd` (`HOUSE-01282`). This file belongs to the single
`HOUSE-00933` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

Seventeen canonical front-elevation `W_DH_*` openings now opt into the Colonial Revival vocabulary
already required by §12.1. Each double-hung sash has one vertical and two horizontal white muntins,
giving exactly six lights. Its paired black shutters are spatial joinery—stiles, rails and eighteen
angled slats per standard leaf—not dark rectangles painted beside a window. The choice lives on the
opening rows, so bays, sidelights, transoms, basement hoppers, dormers and side/rear elevations are
unchanged. The narrow shutter material reuses approved paint maps and is the only new role admitted
to the exterior-window hierarchy; ordinary interior trim remains portal-culled.

Before: Round 32's clear-day [front](visual-review/captures/house-00932-entry-detail-r1/exterior-front.png)
has plain blue-grey window rectangles around the finished entrance. After: the identical
[front](visual-review/captures/house-00933-front-windows-r1/exterior-front.png) has a coherent white
grille rhythm and black shutters across the elevation. The
[foyer-side frame](visual-review/captures/house-00933-front-windows-r1/foyer-facing-front.png) shows
no entrance/portal interference. The full [Round 34 set](visual-review/captures/house-00933-front-windows-r1)
contains all eleven fixed clear-day route/composition cameras and every frame was opened.

The deterministic world now contains 596 chunks and 251 exterior hierarchy instances. Sixteen
actually changed strict references were inspected before selective advancement; unaffected refs
were retained. All 1,407 unit tests, all 134 integration tests and all 48 active software-render
tests pass. The 18-pose culled/unculled comparison remains below its unchanged 0.2% threshold at
0.1599% worst. Schema, generator, world/content and repository gates pass with `CNA_CNAEXT=OFF`.

Next highest visible value is the broad flat facade and thin roof/eave silhouette, followed by the
blank garage/right wing. Interior priorities remain the empty straight living-room view, dining
furnishing, kitchen practical clutter and family-room finish depth. At night, the approach and
facade beyond the localized porch-step spill remain too dark. Preserve the row-authored window
grammar and bounded material role; do not generalize this into a front-cell heuristic.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-01282` checkpoint)

Branch `develop`. Task-start HEAD `dd8c223` (`HOUSE-00932`). This file belongs to the single
`HOUSE-01282` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The front stair remains one canonical Basic-detail chunk owned by `EXT_WALK`; the two real 400 lm
porch sources remain owned, switched and dusk-controlled by `L0_PORCH`. A new optional
`spillCells` list lets a fixed source explicitly name an adjacent unbaked static-detail receiver.
Only that rendering path admits the foreign, range-bounded candidates and their restrained warm
receiver bounce. Ordinary dynamic objects, room state, exposure, group control and the independent
default-off path lights do not inherit anything. Schema, loader and both semantic validators reject
unknown, same-cell and emissive-only receivers. There is no proximity search, cell-name exception,
custom shader, CNAEXT call or global exposure change.

Before: Round 32's fixed [night approach](visual-review/captures/house-00932-entry-detail-r1/exterior-approach-night.png)
shows the finished door and lanterns above a nearly black stair. After: the identical
[night approach](visual-review/captures/house-01282-step-spill-r1/exterior-approach-night.png) makes
the bluestone treads and risers readable; the crop changes 35.84% and rises from mean
RGB(4.35,3.48,3.06) to RGB(7.71,5.93,3.84). The matching
[day approach](visual-review/captures/house-01282-step-spill-r1/exterior-approach-day.png) changes
zero world pixels. The full [Round 33 set](visual-review/captures/house-01282-step-spill-r1)
contains all eleven fixed 22:00 route cameras plus the exact day/night approaches and was inspected
as a contact sheet and at full resolution where affected.

All 1,407 unit tests, all 134 integration tests and all 48 software-render tests pass. No golden
reference changed. The 18-pose culled/unculled comparison remains under its unchanged 0.2%
threshold at 0.1599% worst. World schema/selftests and static/XNA checks are green with
`CNA_CNAEXT=OFF` as recorded by the commit hook.

Next highest visible value is the broad front facade/roof: it is almost black at night and still
reads as a large thin-eaved box by day. Then improve the immediate approach without automatically
enabling the player-controlled path lights. Interior priority remains the empty straight
living-room composition, kitchen practical clutter, family finish depth and dining furnishing.
Do not replace the explicit receiver boundary with neighbour-by-proximity lighting or global
ambient exposure.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-00932` checkpoint)

Branch `develop`. Task-start HEAD `b22767d` (`HOUSE-00931`). This file belongs to the single
`HOUSE-00932` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

Both canonical `D_ENTRY` leaves retain their original portal/collision boxes and now carry the
same generated finish grammar on both faces: four raised panel outlines, a 0.98 m lever on a
backplate, a separate 1.35 m deadbolt and a thin cap over the existing timber threshold. The broad
leaf and relief share the already approved board maps; the relief uses a darker hardwood tint and
the hardware uses the approved brushed-metal maps with a restrained bronze response. Only the
narrow `MAT_EXTERIOR_DOOR_*` roles enter the exterior hierarchy, so ordinary room wood and metal
remain portal-owned. The first same-finish panel iteration was rejected after its close capture
merged into the leaf beneath the porch.

Daylight-facing door detail now uses the outdoor sky/celestial term rather than the owning foyer's
dim indirect level. At night it can use only fixture groups already present as explicit foreign
lightmap bindings on that receiver cell. This gives the entry the two real porch lanterns' local
stock-`BasicEffect` direction and restrained bounce without giving those sources to foyer props or
arbitrary neighbouring cells. No custom shader, CNAEXT call, new renderer or cell-name exception
was introduced.

Before: Round 31's matching [day](visual-review/captures/house-00931-balustrade-r1/exterior-approach-day.png)
and [night](visual-review/captures/house-00931-balustrade-r1/exterior-approach-night.png) views show
a flat dark slab. After: the identical [day](visual-review/captures/house-00932-entry-detail-r1/exterior-approach-day.png)
and [night](visual-review/captures/house-00932-entry-detail-r1/exterior-approach-night.png) approaches,
plus the new [close day](visual-review/captures/house-00932-entry-detail-r1/entry-door-close-day.png),
[close night](visual-review/captures/house-00932-entry-detail-r1/entry-door-close-night.png) and
[foyer side](visual-review/captures/house-00932-entry-detail-r1/foyer-facing-front.png), show a
recognizable finished entry from both sides. The full [Round 32 set](visual-review/captures/house-00932-entry-detail-r1)
contains all fifteen fixed review frames; every frame was opened at original resolution.

The world now has 583 chunks and 238 exterior instances. The shell is 45,543 triangles, with
`L1_BALCONY_REAR` still worst at 1,742/3,500. Ten strict references were inspected and selectively
advanced; five directly see the new door, and the HUD/four seasonal road frames also record the
prior open-guard change their stale references exposed. Generator, world validation, material,
shell, collision and chunk checks pass. All 1,407 unit tests and all 134 integration tests pass.
The focused production/debug/property references pass, and the 18-pose culled/unculled comparison
remains under its unchanged 0.2% threshold at 0.1599% worst. Project source/content/provenance and
strict-XNA gates are green with `CNA_CNAEXT=OFF`.

Next highest visible value is a principled night receiver treatment for the front steps and
immediate approach; they remain almost black even though the door and lanterns now read. Then
improve the broad facade/roof mass and sparse balcony/landscape dressing. Interior priority remains
kitchen practical clutter, family-room finish depth, dining furnishing and the empty straight
living-room composition. Do not hide the night defect with global exposure or larger glow sprites.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-00931` checkpoint)

Branch `develop`. Task-start HEAD `4aafc67` (`HOUSE-00930`). This file belongs to the single
`HOUSE-00931` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The general elevated-deck generator no longer draws its old 550 mm solid parapet. It now derives
one measured open guard from every real open side: an 80 mm lower rail, 45 mm painted balusters,
120 mm end newels and the existing brushed-metal rail at the authored 1.10 m height. The spacing
solver limits the measured worst clear gap to 94.5 mm on the front, rear and Juliet balconies
without naming any one cell. The visual openings do not weaken safety: collision retains its
independent continuous 200 mm-wide, full-height OBB behind the guard.

Before: Round 30's matching [day](visual-review/captures/house-00930-entry-door-r1/exterior-approach-day.png)
and [night](visual-review/captures/house-00930-entry-door-r1/exterior-approach-night.png) approaches
show one broad painted band masking the upper facade. After: the identical
[day](visual-review/captures/house-00931-balustrade-r1/exterior-approach-day.png) and
[night](visual-review/captures/house-00931-balustrade-r1/exterior-approach-night.png) views expose
the landing windows and upper entry through a complete balustrade. The full
[Round 31 set](visual-review/captures/house-00931-balustrade-r1) contains all eleven fixed clear-day
cameras plus this pair; every frame was opened. The exact day guard crop changes 38.883%, while
the finished porch crop changes only 0.447%; the close day/night frames change 5.4022%/3.7632%.

The world remains 579 chunks and 234 exterior instances. The shell is 44,607 triangles, with
`L1_BALCONY_REAR` worst at 1,742/3,500. Seventeen affected golden pairs were inspected and advanced
selectively; side views without a visible guard were left alone. The close-entry semantic test now
protects the open baluster rhythm rather than one pixel in a solid band. Generator, shell and
collision self-tests pass. All 1,407 unit tests, all 135 serial integration registrations and the
focused strict software-render suite pass, including the 18-pose culled/unculled comparison. A
known wall-clock-sensitive sky unit and a weather integration assertion each failed only under
high parallel load, passed isolated, and their complete suites passed at controlled parallelism.
Project source/content/provenance checks and the strict-XNA build with `CNA_CNAEXT=OFF` pass.

Next highest visible value: give the plain hardwood entry slab measured panel relief, threshold and
restrained hardware while keeping the existing portal leaf/collision/exterior-residency contract.
Then solve the adjacent front-step/night receiver without global exposure. Exterior roof/facade
mass and balcony/landscape dressing remain broad defects; interior priority remains kitchen
practical clutter, family finish depth, dining furnishing and the empty straight living view.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-00930` checkpoint)

Branch `develop`. Task-start HEAD `d2f0ea0` (`HOUSE-00929`). This file belongs to the single
`HOUSE-00930` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The canonical front and upper-balcony entry rows now choose a distinct weather-facing hardwood
role. The shell generator derives it only across an explicit exterior portal and leaves the single
closed six-face leaf in its owning interior cell; the existing bounded §25.6 hierarchy additionally
admits only that stable role. This closes the exterior view without making general room trim
externally resident, disabling back-face culling or naming a camera/cell in runtime code. The
approved wood map/tint is unchanged, and collision, portal ownership, entry clearance and strict-XNA
rendering remain intact. The world now has 579 chunks and 234 exterior instances: exactly the two
entry leaves added to Round 29's census.

Before: Round 29's matching [day](visual-review/captures/house-00929-porch-r1/exterior-approach-day.png)
and [night](visual-review/captures/house-00929-porch-r1/exterior-approach-night.png) approaches look
through the nominally closed doorway. After: the identical [day](visual-review/captures/house-00930-entry-door-r1/exterior-approach-day.png)
and [night](visual-review/captures/house-00930-entry-door-r1/exterior-approach-night.png) views show
an opaque textured leaf, while the fixed [foyer reverse](visual-review/captures/house-00930-entry-door-r1/foyer-facing-front.png)
remains properly lit. The full [Round 30 set](visual-review/captures/house-00930-entry-door-r1)
contains the eleven clear-day review cameras plus this fixed exterior pair. All frames were opened;
all ten changed strict-reference pairs were inspected and intentionally advanced.

Shell self-test and deterministic world content generation pass. All 1,407 unit tests, all 134
integration tests and the focused 18-test live render/culling set pass. The integration suite uses
dummy audio and a writable temporary `XDG_DATA_HOME`; one timing-sensitive weather case failed in
the first long run and passed both its isolated retry and the rebuilt complete suite. Project checks
pass source/content/provenance gates and the strict-XNA build with `CNA_CNAEXT=OFF`.

Next highest visible value: replace/refine the broad solid upper-balcony parapet and deepen the
front-facade silhouette. It now dominates the fixed approach as a simplified blockout mass. The
entry leaf then needs panel relief and restrained hardware; the night steps/facade need a principled
receiver treatment. Interior priority remains kitchen practical clutter, family finish depth and
dining furnishing. Do not hide these defects with global exposure, a facade billboard or a
two-sided-material exception.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-01262` checkpoint)

Branch `develop`. Task-start HEAD `6aa33b8` (`HOUSE-01261`). This file belongs to the single
`HOUSE-01262` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The stock-XNA key/fill/bounce contract is now spatial. `LightingSystem` retains each canonical
fixture position and range, aims positional sources at the submitted world-space object centre,
removes candidates beyond range and ranks by live attenuated lumens. The falloff is the approved
`1/(1+(d/range)^2)` with a hard authored-range bound. Directional sources retain their authored
direction. The static Basic path submits the centre of each resident cell/material/layout run's
combined chunk bounds; no draw-time allocation or CNAEXT path was introduced.

Before: [Round 27](visual-review/captures/house-01281-porch-spill-r1). After: the same eleven fixed
22:00 views in [Round 28](visual-review/captures/house-01262-point-light-r1), plus the exact
[close live entrance](visual-review/captures/house-01262-point-light-r1/exterior-approach-close.png)
and [matching day view](visual-review/captures/house-01262-point-light-r1/exterior-approach-day-check.png).
At the close night camera the porch/floor/column/facade crops change 28.46/29.85/30.99/28.29%.
The two sources now model opposite sides and raise column/facade form slightly; the approved
falloff lowers the broad floor mean rather than inventing extra energy. The result is correct and
visible, but deliberately modest.

The first full software-render run exposed nine expected production-day changes introduced by the
new three-slot daylight path and bounds-centre submission. Every before/after pair was opened;
only Basic material/vegetation lighting changed, so those nine references were intentionally
advanced. All 48 active software-render tests pass, including the 18-pose culled/unculled check at
its unchanged 0.1603% maximum. Focused LightingSystem unit tests and all 32 live-device
MaterialBinder/StaticGeometryPass integration cases pass. Full unit/static/XNA gates are green as
recorded by the commit hook.

The close daylight image now makes the next defect unmistakable: the porch is an oversized open
rectangular frame beneath a thin slab, with no convincing layered eave/soffit/fascia/balcony
construction. At night the broad facade remains too dark, and the steps are an adjacent cell that
does not receive the porch-local approximation. Highest visible value is a small, canonical
facade/roof/porch form-depth task, then a principled adjacent-step receiver; interior priority
remains kitchen practical clutter, family finish depth and dining furnishing. Do not raise global
exposure, enlarge glow billboards or divert into unrelated systems.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-01261` checkpoint)

Branch `develop`. Task-start HEAD `a13149e` (`HOUSE-01281`). This file belongs to the single
`HOUSE-01261` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The stock-XNA object-light contract now carries all three §28.5 directional slots rather than a
single Basic-effect key. `LightingSystem` chooses the existing celestial key plus a cell-facing
window fill by day, or the two brightest live local fixtures by night, and derives an opposite
0.18 bounce tinted by the authored dominant room surface. The assignment is value-shaped and
allocation-free during a draw. `MaterialBinder` applies the same slots to `BasicEffect` and
`SkinnedEffect`, explicitly disabling absent slots so its shared instances cannot leak light
between objects or cells. Static Basic detail already consumes the contract; future actor draws
do not need another lighting implementation.

This dependency step intentionally leaves point fixtures on their authored direction and
unattenuated lumen share. It therefore has no claimed visual-review round: the fixed porch pair
still points downward and the substantial entrance correction belongs to immediate successor
`HOUSE-01262`, which adds object-centre direction and the approved `1/(1+(d/range)^2)` attenuation.
Do not confuse effect plumbing with completion of the visible porch receiver defect.

All 1,405 unit tests pass. Thirty-two focused live-device `MaterialBinder` and
`StaticGeometryPass` integration tests pass with the offscreen driver, and the app/unit/integration/
render targets compile warning-clean. The first normal Ninja attempt encountered recorded BL-17
CMake regeneration state; the established existing-generated-build path was used without editing
CNA, FNA3D or another sibling source. No content or strict visual reference changed.

Next: complete `HOUSE-01262`, capture the identical 22:00 fixed set plus close entrance, inspect
the floor/step/column pixels against Round 27, then update the visual review. Roof/eave/balcony
construction remains the next defect after the approach reads as one lit composition.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-01281` checkpoint)

Branch `develop`. Task-start HEAD `7fec420` (`HOUSE-01260`). This file belongs to the single
`HOUSE-01281` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The real porch sources now illuminate the adjacent fixed facade rather than ending at their glow
sprites. Each source stays canonically owned, switched and exposure-accounted by `L0_PORCH`, but
names `L0_FOYER` as an additional offline receiver. The deterministic 256-sample bake adds one
128 px artificial atlas (peak 10.696699, mean 0.00655984) to the foyer shell. Tier S keeps the
outside skin's daylight atlas as its opaque base and admits only this explicitly foreign group as
an additive live pass; foyer-owned lamps still cannot leak outside. A semantic check now rejects a
foreign binding that has no matching `bakeCells` source declaration. The two point positions were
also corrected from 10 cm behind the facade to the measured centres of their physical diffusers.

Before: [Round 26 glow set](visual-review/captures/house-01260-night-r1). After: identical fixed
views in [Round 27](visual-review/captures/house-01281-porch-spill-r1), plus the exact
[close live entrance](visual-review/captures/house-01281-porch-spill-r1/exterior-approach-close.png)
and [matching day/off state](visual-review/captures/house-01281-porch-spill-r1/exterior-approach-day-check.png).
The exact close paired-source crop changes 54.64% and rises from mean RGB(15.68,13.19,11.70) to
RGB(17.69,14.32,12.79); the facade crop changes 13.87%. The porch-floor crop changes 0% because
that geometry remains unbaked Basic detail. The matching daylight world crop differs by only four
pixels. The result is a restrained pair of local wall pools, not a global exposure increase.

Verification is green for all 1,404 unit tests, all 10 SaveStore cases and all 48 active
software-render cases, including the 18-pose culled/unculled comparison; no golden reference
changed. The 124-case non-save integration run passed 123 cases and its one unrelated
timing-sensitive live-weather case passed on immediate isolated rerun. The generated schema,
semantic validator self-test, all thirteen authored-world rules, incremental world content build,
Python compilation, deterministic Blender bake self-test, manifest/content checks and focused
real-device renderer test also pass. `tools/ci/run_checks.sh` is green through all strict-XNA
translation units with `CNA_CNAEXT=OFF`.

Next highest visible value is the still-dark Basic-effect porch floor, steps and columns. Complete
the approved per-object/distance-attenuated light assignment (`HOUSE-01261`/`HOUSE-01262`) or the
smallest architecture-consistent receiver step, then deepen the skeletal roof/eaves, balcony and
porch joinery. After the approach reads as one composition, return to kitchen practical clutter,
family-room material depth and dining furnishing. Do not enlarge the glow billboards, broaden the
facade atlas or raise global night exposure to conceal the missing receiver classes.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-01260` checkpoint)

Branch `develop`. Task-start HEAD `03bf545` (`HOUSE-01259`). This file belongs to the single
`HOUSE-01260` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and explicit blockout mode is unchanged.

The two real porch lanterns now have the restrained soft source bloom §28.6 requires. The existing
transparent pass lazily owns one generated 64 px radial texture, static camera-facing quad and
stock-XNA `BasicEffect`; it appends the two live glows under additive blending with read-only depth.
Radius and alpha follow fixture lumens, the exact filament transition output and adapted exposure.
Only lights with a real `fixtureProp` plus exact emissive slot are eligible, so the many canonical
points whose physical bodies are not yet modelled do not become floating placeholder orbs. The
presentation centre alone moves toward the eye enough to clear its own wall-mounted shade; the
canonical light, baked lightmap receiver and collision data remain fixed. No custom shader,
CNAEXT call or second light state was introduced.

Before: [Round 25 night set](visual-review/captures/house-01259-night-r1). After: identical eleven
fixed views in [Round 26](visual-review/captures/house-01260-night-r1), plus the exact
[close live entrance](visual-review/captures/house-01260-night-r1/exterior-approach-close.png) and
[matching day/off state](visual-review/captures/house-01260-night-r1/exterior-approach-day-check.png).
The road, close-night and close-day images were opened at original resolution. At the exact close
camera each 100 x 100 source neighbourhood changes 35.2% of pixels and rises from approximately
RGB(15,12,9) to (18,14,9); the full pair crop changes 18.0%. The road view remains subtle by design:
this task adds source bloom, not fake global illumination.

The new real-device integration test reads back the colour target, proving the radial texture and
alpha reach pixels rather than merely incrementing a draw counter. Pure tests cover off/dim/full,
flux and exposure ordering. The test also exposed the transparent pass's old process-order counter
bug; handles are now rebound when their owning registry changes. All 1,404 unit tests pass; 124
non-save integration cases and all 10 SaveStore cases pass; all 48 active software-render tests
pass with no golden update, including the 18-pose culled/unculled comparison at its unchanged
0.1603% worst case. `tools/ci/run_checks.sh` is green through all 323 strict-XNA translation units
with `CNA_CNAEXT=OFF`.

Next highest visible value is the missing local warm receiver pool on the door surround, porch
floor and steps. Use the approved Tier-S per-object/distance-attenuated path (`HOUSE-01261` then
`HOUSE-01262`) or the smallest architecture-consistent static receiver step; do not turn the source
quad into a giant fake light. After that, deepen the still-flat façade, roof/eaves and porch joinery,
then return to kitchen practical detail, family material depth and dining furnishing. The broad
night façade remains almost black and the close daylight porch still reads as an engineering shell.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-01259` checkpoint)

Branch `develop`. Task-start HEAD `958f08f` (`HOUSE-01258`). This file belongs to the single
`HOUSE-01259` commit; use that commit as the ending HEAD. **VISUAL-GATE-1 still FAILS.** Normal
gameplay remains production-material only and the explicit blockout mode is untouched.

The entrance now has two real deterministic 0.608 m wall lanterns rather than placeholder strips.
Each 232-triangle model has a dark-bronze framed body and a separate warm diffuser; both source
slots map through the canonical manifest/material system with generated/Ms-PL provenance. The
two static props live in `L0_PORCH`. Their exact `LanternShade` slots link to the existing automatic
porch group, so the stock-BasicEffect production pass consumes its Kelvin colour and post-filament
`GroupOutputLevel`. The close 10:30 check shows non-glowing physical bodies and the matching 22:00
check shows warm live shades. No custom shader, CNAEXT path or second switch state was added.

The cross-file fixture contract is now enforced: a linked prop must exist in the light's cell,
name a source slot, and map that exact slot to an emissive canonical material. The chunk builder
keeps independently switched shades apart with an internal fifth key discriminator while retaining
the four-part on-disk format and prop sub-ranges. A missed shell-key update briefly split kitchen
paint into a four/five-tuple pair; it was diagnosed rather than budgeted away and kitchen is back
at 13/13 chunks. Porch is a measured 8/8: its existing six finish/plant chunks plus one shared
bronze-body and one shared switched-diffuser chunk.

Before: [Round 23 fixed night set](visual-review/captures/house-01269-night-r2). After: identical
eleven-view [Round 25 night set](visual-review/captures/house-01259-night-r1), plus the
[close live entrance](visual-review/captures/house-01259-night-r1/exterior-approach-close.png) and
[close day/off state](visual-review/captures/house-01259-night-r1/exterior-approach-day-check.png).
All three relevant frames were opened at original resolution. The road view changes 0.0602% of
pixels; its tight fixture crop rises RGB(15.29,12.65,12.32) → (18.54,14.82,12.85). This is a
correctly scaled source presentation, not yet a local-light solution.

Verification is green. The complete 1,404-test unit suite passes; 123 non-save integration cases
and all 10 save-store cases pass; all 49 active software render cases pass, including the
culled/unculled comparison. Five exterior references were advanced only after their diffs showed
the intended new fixtures; two unrelated low-level vegetation differences from bulk regeneration
were rejected. Chunk self-test, deterministic source-asset check, GLB scale/origin validation,
all 13 world rules and the incremental world content build pass. `tools/ci/run_checks.sh` is green
through all 323 strict-XNA translation units with `CNA_CNAEXT=OFF`. Its first run caught the GLB's
wall-centred pivot; the generator now emits the standard bottom-centred fixture origin while the
inverse prop translation preserves the inspected world pose. The earlier ~20-hour `build_nav.py`
was a runaway/stale session: the dirty nav build in this checkpoint finished in 9.22 seconds.

Next highest visible value is `HOUSE-01260`: add restrained exposure-aware glow to the two now
physical sources. The largest remaining visible defect is the missing local halo/pool, followed by
the broad black façade, flat/skeletal porch and roof, then kitchen practical detail, family material
depth and dining furnishing. Do not hide any of these by raising global exposure.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-01269` checkpoint)

Branch `develop`. Continuous visual-sprint start HEAD
`238d6aed27c1e8662f0d118013ef08bb801ae018`; clean task-start HEAD
`06afbea324dd474b4730d92e3ca9b326910c00b2` (`HOUSE-00772`). This file belongs
to the single `HOUSE-01269` commit; use that commit as the ending HEAD.
**VISUAL-GATE-1 still FAILS.** Normal gameplay remains production-material only,
with explicit debug blockout separate. The fixed 22:00 front is now readable at the
ground/plot/porch frame, but the façade is still nearly black and the two porch lights
still lack visible fixture bodies, emissive shades and a local entrance pool.

Round 22 before: the eleven-view [night set](visual-review/captures/house-00772-night-r2).
Round 23 after: identical cameras, condition and exposure in the
[dusk-sensor set](visual-review/captures/house-01269-night-r2). The paired
[front before](visual-review/captures/house-00772-night-r2/exterior-front.png) and
[front after](visual-review/captures/house-01269-night-r2/exterior-front.png) were
opened at original resolution. The front changes 40.507% of pixels; porch-crop mean
rises RGB(6.55,7.27,9.17) → (12.67,11.04,11.02), while foreground mean rises
RGB(3.30,4.38,4.08) → (26.72,28.40,12.25). Daylight output is intentionally
unchanged because every automatic group is exactly off by day.

`HOUSE-01269` corrects its dependency from interactive `HOUSE-01252` to the already
complete light-state/clock pair `HOUSE-01251` + `HOUSE-01531`. Fifteen fixtures now
author `duskSensor`: two house porch, nine street and four neighbour porch lights.
They cross live solar altitude -4° with stable-id whole-minute offsets in [-8,+8].
The combined Tier-S group state uses lumen-weighted active fraction during the short
stagger, then reaches exactly one at night. Loader/schema support and both validators
reject mixed-control groups and automatic `defaultOn`. Visible fixture emissives/glow
remain correctly owned by `HOUSE-01259`/`HOUSE-01260`; the neighbour/window-card
presentation remains `HOUSE-00850` rather than duplicating the solar controller.

The source/test build used the existing generated commands because upstream BL-17
still makes CMake regeneration try to write CNA's read-only FNA3D patch record. No CNA,
FNA3D, sharp-runtime or sibling source was modified. The world content graph rebuilt in
35.84 s, including nav in 14.02 s, independently reconfirming that the earlier ~20-hour
`build_nav.py` was a runaway, not required work. See
[Round 23 review](visual-review/README.md) for the visual defect ranking.

Generated schema, both validator self-tests and the real 13-rule world validation pass;
all 1,401 unit tests pass. Integration has 122 passes, eight configured skips and the
established process-order-only `TransparentPassTests` failure, which passes 1/1 in isolation.
The software render suite has 46 passes, two configured skips and eight disabled capture cases
with no golden update. `tools/ci/run_checks.sh` is fully green through all 323 strict-XNA
translation units, and `git diff --check` is clean. The final commit hook exposed one unrelated
environment compatibility defect: Pillow 11.1 lacks `Image.get_flattened_data`. The snow-material
self-test now uses its equivalent legacy `getdata` iterator on older Pillow releases; generated
bytes and validation thresholds are unchanged.

Next highest visible value: give the entrance real porch-lantern bodies/emissive shades
and a localized warm pool through the approved Tier-S/Tier-E paths, then deepen the
façade/roof/porch geometry. Do not solve the still-black house by raising global exposure.
Kitchen practical/clutter, family material depth and dining furnishing follow. Do not
resume unrelated Android/Web/pet/IK/astronomy work.

---

# Prior visual-sprint handoff — 2026-09-16 (`HOUSE-00772` checkpoint)

Branch `develop`. Continuous visual-sprint start HEAD
`238d6aed27c1e8662f0d118013ef08bb801ae018`; clean task-start HEAD
`5c2e32be3e107f984a4db2e7665f93ea821d244d` (`HOUSE-01044`). This file
belongs to the single `HOUSE-00772` commit; use that commit as the ending HEAD.
**VISUAL-GATE-1 still FAILS.** Normal gameplay remains production-material only,
with explicit debug blockout separate. The road→gate→porch view now has real trees,
shrubs, hedges, flowers and grass, but the façade/roof/porch and night entrance are
not yet a credible finished-house presentation.

Round 21 before: identical eleven-view [clear](visual-review/captures/house-01044-clear-day-r1),
[noon](visual-review/captures/house-01044-noon-r1),
[overcast](visual-review/captures/house-01044-overcast-r1) and
[night](visual-review/captures/house-01044-night-r1) normal-game sets. Round 22
after: matching [clear](visual-review/captures/house-00772-clear-day-r2),
[noon](visual-review/captures/house-00772-noon-r2),
[overcast](visual-review/captures/house-00772-overcast-r2) and
[night](visual-review/captures/house-00772-night-r2) sets; camera, time, weather
and exposure are unchanged. The clearest result is the
[planted clear front](visual-review/captures/house-00772-clear-day-r2/exterior-front.png),
while the [night front](visual-review/captures/house-00772-night-r2/exterior-front.png)
shows the next defect: the house remains nearly black and foliage is relatively
bright. The first [rejected clear set](visual-review/captures/house-00772-clear-day-r1)
records why mature trees were moved/scaled down after they obscured the entrance.

`HOUSE-00772` routes the already approved/pinned Poly Haven CC0 vegetation through
the established manifest/material/static-chunk/exterior-BVH path. The final canonical
file has 309 deterministic placements in 14 groups: 34 street trees, 3 property
trees, 4 young orchard trees, 60 shrubs, 169 hedge shrubs at 2.1 m group scale,
18 flowers and 21 grass patches. Ten selected roles retain source-exact <=256 px
runtime atlases; the other catalogue roles use explicit bark/foliage/flower/grass
fallbacks from the pinned sources. The stable `VEG_MAPLE`, `VEG_BIRCH` and
`VEG_FRUIT` role IDs remain, but comments/assets honestly name Searsia and
`tree_small_02`; the approved source set contains no exact maple/birch/fruit model.

The production library measures 574 chunks, 1,806,301 vertices and 41,618,420 packed
bytes: 44 AlphaTest foliage batches, 8 16-bit vertex splits, 17 Reach primitive
splits and no 32-bit-index chunk. Per-placement sub-ranges retain exterior BVH
bounds. Four measured exterior-cell budget exceptions are explicit rather than a
global relaxation. Tree trunks and the 2.1 m hedge enter canonical collision. The
content nav stages measured 18.14 s and 16.60 s, confirming the earlier 19-hour
`build_nav.py` was a runaway abandoned process, not required project work.

Verification is green: the 850-row asset manifest and all 13 world-validator rules
pass, the forced 31-stage content graph is fresh, and chunk/collision selftests pass.
The unit suite is **1398/1398**; the integration suite is **130/130** plus its known
order-sensitive GL-state test **1/1** in isolation; the complete software render is
**48/48**. All 18 culled-vs-unculled poses pass (worst 0.1603% < 0.2%). Sixteen
strict references were updated selectively only after paired inspection showed the
new vegetation as the intended difference. `tools/ci/run_checks.sh` is green through
all **323 strict-XNA translation units**, and `git diff --check` is clean. No CNA,
sharp-runtime or sibling source was modified, and `CNA_CNAEXT=OFF` remains forced.

Next highest visible value: make the 22:00 porch/entrance and the front façade/roof
read with the existing Tier-S/Tier-E lighting architecture, then improve the sparse
east-front small tree/cutout quality and complete `HOUSE-00773` grass jitter without
adding a special renderer. Kitchen practical readability/clutter, family-room
material depth and dining furnishing follow. Do not resume unrelated Android/Web,
pet, IK or astronomy work. See [Round 22 review](visual-review/README.md).

---

# Prior handoff — 2026-09-16 (`HOUSE-01044` checkpoint)

Branch `develop`. Continuous visual-sprint start HEAD
`238d6aed27c1e8662f0d118013ef08bb801ae018`; clean task-start HEAD
`2d993a66643d1c7a33093f5997313c0655b1115c` (`HOUSE-01043`). This file
belongs to the single `HOUSE-01044` commit; use that commit as the ending HEAD.
**VISUAL-GATE-1 still FAILS.** Normal gameplay remains production-material only,
with explicit debug blockout separate. The connected kitchen is more credible,
but the front approach is still unmistakably an engineering shell.

Round 20 before: identical eleven-view [clear](visual-review/captures/house-01043-clear-day-r1),
[noon](visual-review/captures/house-01043-noon-r1),
[overcast](visual-review/captures/house-01043-overcast-r1) and
[night](visual-review/captures/house-01043-night-r1) normal-game sets. Round 21
after: matching [clear](visual-review/captures/house-01044-clear-day-r1),
[noon](visual-review/captures/house-01044-noon-r1),
[overcast](visual-review/captures/house-01044-overcast-r1) and
[night](visual-review/captures/house-01044-night-r1) sets; camera, time, weather
and exposure are unchanged. The clearest result is the
[west kitchen](visual-review/captures/house-01044-clear-day-r1/kitchen-facing-west.png),
with the new bay also visible [from the hall](visual-review/captures/house-01044-clear-day-r1/kitchen-from-hall.png).
The [clear front](visual-review/captures/house-01044-clear-day-r1/exterior-front.png)
and [night front](visual-review/captures/house-01044-night-r1/exterior-front.png)
remain the largest defects.

`HOUSE-01044` adds one project-authored deterministic kitchen cooking bay through
the established manifest/material/prop/chunk path: 1.00 × 2.65 × 0.738 m,
23 components, 2,260 visible triangles, a separately measured 0.94 m counter,
900 mm range and oven, four burners, 150 mm physically tiled backsplash and
steel hood/filter/chimney. It sits at `[-7.75, 0.60, -25.075]`, yaw 270°, between
the pantry door and butler opening. A 12-triangle box proxy preserves collision.
Paint/stone/steel reuse approved materials; backsplash tile and deliberately
opaque Tier-S oven glass have permanent IDs, provenance and explicit loader/binder
coverage. The GLB SHA-256 is
`505f4b70e9a4976bda2b7e7b939c0db9e00d072f81a8174a829c1cd128b0c68d`.
`L0_KITCHEN` measures 13/13 chunks, exactly two above the prior ceiling for those
new finish groups, with no vertex/Reach split.

Important rejected iteration: the first 2.40 m west-wall assembly looked visually
stronger but covered `P_KITCHEN__PANTRY`. `ClosedDoorTests` caught the inaccessible
pantry before commit. It was discarded and regenerated as the final measured 1.00 m
bay; do not restore the wide version merely from the image. The final gate exercises
62 doors, 144 closed-door walks and 124 open controls. All four kitchen built-ins
regenerate bit-for-bit and the new scale selftest checks width, full depth and the
counter separately from the 2.65 m hood height.

Verification: asset manifest 837 rows; all 13 world-validator rules; forced 31-stage
content graph in 42.31 s with nav in 12.63 s; 186 focused unit tests; 34 focused
material/content integration tests; full software render **48/48**, including
culled-vs-unculled equivalence. Four 11-pose review sets were opened. The final bay
changes 1.279–1.304% of the west frame and 0.828–0.847% of the hall frame across
clear/noon/overcast/night. Repository CI is green, including **323 strict-XNA
translation units**; `git diff --check` passes. Two kitchen strict-reference PNGs
are intentionally updated and inspected. Full CMake/CTest remains unclaimed: a sibling BL-17 glob
forces regeneration and the CNA FNA3D patch step cannot write outside this sandbox.
The two changed test objects were compiled and linked with their existing generated
`build.ninja` commands; no CNA, FNA3D or sibling file was modified.

The previously reported 19 h `build_nav.py` was a runaway old session, not required
work: both PIDs disappeared, no replacement process exists, and the same nav stage
now completes in 12.63 s. No partial nav output is present.

Next highest visible value: the front approach is now decisively worse than the
kitchen in the fixed set. Investigate the already authored/provenanced vegetation
records and dependency-valid `HOUSE-00772`/`HOUSE-00773` path, then make the façade,
porch, roof and night entrance readable without bypassing the canonical exterior
hierarchy. Kitchen under-cabinet/static-prop readability and restrained clutter are
next. Do not resume unrelated Android/Web/pet/astronomy work. See
[Round 21 review](visual-review/README.md).

---

# Prior handoff — 2026-09-16 (`HOUSE-01043` checkpoint)

Branch `develop`. Continuous visual-sprint start HEAD
`238d6aed27c1e8662f0d118013ef08bb801ae018`; clean task-start HEAD
`7ae8c5e28d2f3b892b4e93c828e977ca74ca1e50` (`HOUSE-01042`). This file
belongs to the single `HOUSE-01043` commit; verify its ending HEAD and clean
tree before the next visual task. **VISUAL-GATE-1 still FAILS.** Normal walk
rendering already uses production materials, not hashed blockout colouring,
but the front façade/roof/porch and kitchen completeness/lighting are far from
a believable playable vertical slice.

Round 19 before: identical eleven-view [clear](visual-review/captures/house-01042-clear-day-r2),
[noon](visual-review/captures/house-01042-noon-r2),
[overcast](visual-review/captures/house-01042-overcast-r2) and
[night](visual-review/captures/house-01042-night-r2) normal-game sets. Round 20
after: matching [clear](visual-review/captures/house-01043-clear-day-r1),
[noon](visual-review/captures/house-01043-noon-r1),
[overcast](visual-review/captures/house-01043-overcast-r1) and
[night](visual-review/captures/house-01043-night-r1) sets; no camera, time,
weather or exposure setting moved. The [hall before](visual-review/captures/house-01042-clear-day-r2/central-hall.png)
had nearly black rectangles in the *new* painted upper refrigerator cabinet.
The [hall after](visual-review/captures/house-01043-clear-day-r1/central-hall.png)
shows the actual upper fronts. Fixed upper pixel (900,350) changes
RGB(10,10,10) → (47,36,26), exactly the unchanged lower painted-door value.
The [inside kitchen after](visual-review/captures/house-01043-clear-day-r1/kitchen.png)
also shows those faces, though all static detail is still too brown/dark. The
[unchanged clear front](visual-review/captures/house-01043-clear-day-r1/exterior-front.png)
and [night first view](visual-review/captures/house-01043-night-r1/exterior-front.png)
remain the largest visual defects; the family composition is pale/sparse.

Corrected diagnosis: `CELL_FRIDGE_INTERIOR` declares `yOverride` 0.70..2.45
inside `L0_KITCHEN`, but `side_intervals` matches only coincident *edges*. Its
parent's larger enclosing footprint was missed and the shell generator drew
a weather-facing `MAT_PAINT_SOFT_WHITE` outer wall at z=-26.300 through y=3.65.
The recessed upper cabinet panel is at z=-26.307, so the false skin lay **7 mm
in front** of it. This was depth occlusion, not merely an underpowered
BasicEffect. Fridge source `build/shell`/`shell-lm`/`chunks.bin` now caps the
wall at y=2.45 and emits no exterior polygon; the 20 mm door-head trim ends
at y=2.47. The same full-containment predicate covers the canonical freezer
interior and garage loft, with real exterior runs unchanged. Three-data-cell
selftests assert both containment and zero outer polygons, and the complete
99-GLB shell determinism check regenerated with zero differences. The full
shell manifest changes only those three child GLB hashes plus `L0_PORCH`'s
previously stale generated file; its small debug exterior/property pose
differences were paired and inspected before selective golden replacement.

`shell_unwrap.py` (receiver-only) regenerated all 78 UV2 receiver files, with
only affected nested geometry intended to differ; its report remains 78 cells,
1.28 million texels and approved densities. The first generic unwrap trial
was rejected because it included non-receiver detail and made the garage loft
atlas 256² rather than the canonical 128². A first **full** daylight bake
finished but correctly failed at the binding gate: it used default 683 lm/W
against four intentionally selected 100 lm/W L0 rooms and old artificial
signatures. Its transient manifest/report were moved recoverably to
`/tmp/house01043-manifest-backup-1WJHmg`, then the exact clean task-start
versions restored; no 100 lm/W selected-room map was committed from that
trial. Correct `--daylight/--artificial --cells` products for fridge/freezer/
garage loft were then baked at 683 lm/W and atomically promoted together.
The selected-promotion validator was narrowly corrected to permit an empty
artificial family **only** when a cell has no bakeable lamp. The fridge lamp is
runtime-only `emissive_only`, and the freezer has none; it would be wrong to
invent an artificial atlas for either. One garage-loft artificial and three
daylight PNGs, their sidecars, cell bindings, provenance/credits, world and
shell manifests changed. Portal/door/collision stable IDs and normal XNA-only
runtime source were not touched.

Verification at this checkpoint: full 31-stage content graph is green after
the targeted promotion and exact `build/content` copy; the fresh nav stage
took **12.76 s** under a 180-second whole-build timeout. The earlier reported
PID 936066 (`build_nav.py`, allegedly ~20 h CPU) and parent PID 4067428 no
longer existed when checked; no nav process was active and none was killed.
That long CPU burn, if accurate, was a pathological run, not required bake
work. Four 11-pose review sets were opened and compared. The 48-test render
run initially passed 44 and failed four *reference suites* covering ten exact
poses, all from inspected intended changed regions; culling equivalence
passed. Only these ten named PNG references were selectively copied from
their newly captured actual images, with original references saved under
`/tmp/house01043-golden-before-zWI022`; the complete software render rerun
passes **48/48**, including culling equivalence. Full repository CI is green
after provenance and budget-report generation; **323 strict-XNA translation
units** compiled clean. `git diff --check` passes. Fresh full CMake/CTest
remains unclaimed under sibling BL-17 MojoShader-series regeneration, without
any CNA/sibling edit.

Next highest visible value: complete west/north kitchen upper runs, range,
hood, backsplash and controlled props; make Basic-effect static furnishings
readable without debug colour or arbitrary global ambient; then improve the
front approach's skeletal façade/porch/roof and night visibility. Do not
resume Android/Web/pet/astronomy subsystem work while the first screenshot
still reads as an engineering shell. See [Round 20 review](visual-review/README.md).

---

# Prior handoff — 2026-09-16 (`HOUSE-01042` checkpoint)

Branch `develop`. Continuous sprint start HEAD:
`238d6aed27c1e8662f0d118013ef08bb801ae018`. Preceding clean HEAD:
`0bb8f499f8e8be8d079334f655fc999af77b215f` (`HOUSE-01041`). This file belongs
to the single `HOUSE-01042` commit; verify exact new HEAD and clean tree before the
next task. **VISUAL-GATE-1 still FAILS.** Normal first-person rendering uses real
production materials rather than hashed blockout colours, but furnishings,
static-prop light, kitchen completeness and the exterior/night entry are unfinished.

Before: unchanged eleven-view [Round 18 clear](visual-review/captures/house-01041-clear-day-r1),
[noon](visual-review/captures/house-01041-noon-r1),
[overcast](visual-review/captures/house-01041-overcast-r1) and
[night](visual-review/captures/house-01041-night-r1) sets. After: paired eleven-view
[Round 19 clear](visual-review/captures/house-01042-clear-day-r2),
[noon](visual-review/captures/house-01042-noon-r2),
[overcast](visual-review/captures/house-01042-overcast-r2) and
[night](visual-review/captures/house-01042-night-r2) sets; no pose/condition changed.
The [central hall before](visual-review/captures/house-01041-clear-day-r1/central-hall.png)
showed a nearly pure-black refrigerator-interior slab. The
[central hall after](visual-review/captures/house-01042-clear-day-r2/central-hall.png)
shows appliance fronts, pulls, dispenser, base vent and a bridge cupboard. The
[inside-kitchen view](visual-review/captures/house-01042-clear-day-r2/kitchen.png)
is still too brown/dark and its overhead cabinet panels are underlit. A first
steel-front try was actually inspected and rejected; only its
[hall](visual-review/captures/house-01042-rejected-r1/central-hall.png) and
[kitchen](visual-review/captures/house-01042-rejected-r1/kitchen.png) evidence was
kept. The transient other nine rejected frames were moved, recoverably for this
session, to `/tmp/house01042-rejected-wiulPN`. Unchanged
[clear front](visual-review/captures/house-01042-clear-day-r2/exterior-front.png),
[night front](visual-review/captures/house-01042-night-r2/exterior-front.png) and
[family](visual-review/captures/house-01042-clear-day-r2/family-composition.png)
were inspected, not inferred from regression pass status. See
[Round 19 review](visual-review/README.md) for ranked visible defects.

Implementation: Blender-authored deterministic `refrigerator.glb` is 59 detailed
pieces with 5,524 visible triangles, physical-repeat UV0, three approved reused
material IDs, an actual 1.80 × 1.95 × 0.70 m refrigerator and 0.75 m two-door
shaker cabinet joining the 3.30 m L0 ceiling. A 1.80 × 2.6995 × 0.833 m total
accessor AABB includes the overhead cabinet and front pulls. `origin_check`
found the first model's Z support axis off by 6.65 cm because of those pulls;
the glTF was recentered to ±0.4165 m Z and the canonical prop moves from
z=-26.6700 to -26.6035. A paired final 1600×900 hall capture has **zero**
pixels differing by >2 channels below the HUD from the Round 19 clear frame,
proving the photographed appliance did not shift. §70.5 now has separate
large-appliance body and integrated-bay scale bands; manifest-backed body-height
and source-mesh checks with synthetic selftests enforce both. It is Ms-PL
project-authored, bit-for-bit re-export checked and entered through the existing
manifest/CNA content/static batch paths, not copied from another project.
Canonical `CELL_FRIDGE_INTERIOR`, `P_FRIDGE_INTERIOR` and `FRIDGE_L0_KITCHEN` IDs,
portal and thermal data remain. Only the refrigerator opening's declared steel
finish changes to the painted white front; the freezer stays steel. The kitchen
stays measured at 11/11 material chunks. The two new asset/prop IDs were
recorded in the stable-ID golden. The existing `world.manifest.json` also
held stale hashes for already-committed `layout.cells`, `layout.lights` and
`layout.materials`; this task regenerated the authoritative member ledger for
those unchanged files along with its real opening/prop changes. No extra world
source file was silently rewritten.

Important collision finding: shell collision punches the fridge portal hole, and
the planned moving door obstacle is not installed in today's playable house.
The first tentative `collision: none` visual prop was therefore rejected: a
player could walk through the shut fridge. The authored 12-triangle box proxy is
now active for its **current static closed** frontage. Collision round-trip
contains prop OBB index 1190 at centre (1.20,1.94975,-26.6035), half
(0.90,1.34975,0.4165), indexed in `L0_KITCHEN` and
`CELL_FRIDGE_INTERIOR`. Future animated-door work must replace this static
frontage/proxy with moving parts while keeping the canonical IDs and portal,
not leave a solid box over an open door. This checkpoint does not claim the
fridge is functional or the kitchen finished. An upper hall ray at pixel
(900,380) confirms the old black ceiling-gap area was rear
`MAT_PAINT_WARM_WHITE` kitchen wall at z=-27.025; the new painted cabinet at
z=-26.336 occludes it but remains too dark due the Basic-effect prop-light path.

Verification: all 31 content stages pass after source, opening, origin and collision
changes; final navigation bake takes 12.58 seconds. The prop uses the existing
canonical collision writer, the fridge GLB and both previous kitchen built-ins
regenerate bit-for-bit, the manifest/credits/WorldManifest now agree, and the
strict hall first-person golden is the **only** changed >2-channel-tolerance
pose. That before/after was opened and the one intended hall reference was
selectively updated; no other golden was regenerated. The exact final build
content was copied/hash-checked into `build/content`, and **48/48 direct software
render tests** pass over it, including culled-vs-unculled equivalence. Full
repository CI was green with **323 strict XNA translation units** after the
four deterministic content-gate corrections (shell manifest, stable IDs,
measured appliance scale and support origin). The source checker additionally
protects physical UV repeat on both close fronts. `CNA_CNAEXT=OFF`
and the runtime XNA-only design were not touched. There were no CNA/FNA3D or
sibling edits. A fresh full CMake/CTest is still unclaimed because the external
BL-17 MojoShader-series regeneration writes outside this sandbox; use the
existing house binaries and generated commands without changing siblings.

Next highest visible work: calibrate approved Basic-effect furniture/appliance
lighting from canonical selected L0 fixtures so kitchen fronts and family seating
read naturally rather than as uniformly dark/tan slabs. Then prioritize the
skeletal front roof/porch/balcony and practically invisible night approach,
followed by remaining range/hood/upper kitchen kit. Preserve screenshots and
VISUAL-GATE-1 as a human visual review, not a pixel-only test.

## Prior `HOUSE-01041` handoff

Visual-sprint handoff — 2026-09-16 (`HOUSE-01041` checkpoint)

Branch `develop`. This continuous visual sprint began at
`238d6aed27c1e8662f0d118013ef08bb801ae018`; the preceding clean checkpoint was
`c040f5fe2e7866a273c80308eac50e85fadd012e` (`HOUSE-01040`). This file is committed
with the one-task `HOUSE-01041` change; verify the exact new HEAD/tree before another
checkpoint. **VISUAL-GATE-1 still FAILS.** The playable family/kitchen route has production
materials, primary kitchen built-ins and now useful family practical light, but the unchanged
hall image still contains a large black refrigerator-interior silhouette, the kitchen lacks
appliances/uppers, the family room is underdressed, and the front/night exterior is skeletal.

Before: matching [Round 17 clear-day set](visual-review/captures/house-01040-clear-day-r4)
and [family at night](visual-review/captures/house-01040-night-r4/family-composition.png).
After: eleven-view [Round 18 clear](visual-review/captures/house-01041-clear-day-r1),
[noon](visual-review/captures/house-01041-noon-r1),
[overcast](visual-review/captures/house-01041-overcast-r1) and
[22:00](visual-review/captures/house-01041-night-r1) sets, all at the same fixed cameras.
Changed family, kitchen, hall and exterior route frames were actually inspected. The
[new clear family composition](visual-review/captures/house-01041-clear-day-r1/family-composition.png)
has a warm readable sofa rather than bluish near-blockout shading; its fixed pixel rises from
RGB(48,56,71) to (77,62,52). The [night family composition](visual-review/captures/house-01041-night-r1/family-composition.png)
gains useful practical depth (fixed sofa pixel 34 grey → 81/60/41), without hot discs. The
[hall aperture](visual-review/captures/house-01041-clear-day-r1/central-hall.png) remains black.
See [Round 18 visual review](visual-review/README.md) for ranked defects.

Implementation: canonical `LG_L0_FAMILY_MAIN` is now a switchable default-on group of four
3.02 m downward 140° spots at 3000 K/1000 lm, using only the approved Tier-S receiver bake.
The independently switched TV media accent was no longer a ceiling-centred point: at the same
100 lm/W selected broadband calibration it would have peaked at 7.4286; its wall-facing spot
behind the authored TV peaks at 0.1161. Two existing kitchen east-row spots move toward its
rear wall; the selected 128² atlas sample (112,68) changes 56/56/55 → 121/122/121.
L0_FAMILY and L0_KITCHEN were deterministic selected-cell artificial/daylight bakes promoted
together with durable reports, material provenance hashes and per-cell bindings. Other 76
receiver products remain untouched. No new renderer, fixture family, external asset or CNA
exception was added. A unit test protects family default-on, switch-off and light borrowing
through the open kitchen portal. The subset-promotion provenance writer's stale hard-coded
HOUSE-01038 label was made task-neutral so this checkpoint is not falsely attributed.

Important corrected diagnosis: the initial green `MAT_DOOR_PAINTED` debug block and a
partial wall-only ray query misidentified the hall defect. Explicit blockout uses a 55° lens;
normal gameplay uses 70°. Full-chunk triangle picking of the actual hall pixel (900,450)
hits `CELL_FRIDGE_INTERIOR`'s front at (0.897,2.28,-26.415), before the painted kitchen
rear wall. That canonical nested refrigerator cell already has stable portal/thermal/collision
data but no exterior refrigerator model. Its dark inside is plausible **inside** a shut fridge;
as an exposed 1.8 × 1.95 × 0.7 m black mass in the hall sightline it is unfinished content.
Do not increase ambient to mask it. Next highest visible work is a legally/provenantly valid
measured refrigerator carcass/front aligned with the existing cell and its openable-door
architecture, then inspect the unchanged hall camera. Exterior roof/porch/night and kitchen
upper/range/hood/dressing remain major subsequent visible work.

Verification: 31-stage content dry run is fresh after the two-cell promotion; selected source
atlases/bindings were copied only into the existing build content directory. The changed unit
test was compiled/linked with the exact pre-generated house Ninja commands and required
shared ccache because BL-17 still blocks sibling CNA's MojoShader-series CMake regeneration.
**24/24 lighting unit tests pass.** Exactly two changed first-person strict references (hall,
kitchen) were paired, viewed and selectively updated; **48/48 direct software-render tests**
pass after replacement, including culled-vs-unculled equivalence. The complete repository CI
is green after deterministic budget-report regeneration, with 323 strict-XNA translation units;
`git diff --check` was verified before commit. Do not claim a fresh full CMake/CTest while BL-17 remains.
This repository did not edit CNA, FNA3D or another sibling. `CNA_CNAEXT=OFF` and strict
XNA-only remain unchanged. The earlier 19-hour navigation PID is gone; this checkpoint's
fresh graph navigation bake took 16.83 seconds, not a zombie.

## Prior `HOUSE-01040` handoff

Visual-sprint handoff — 2026-09-16 (`HOUSE-01040` checkpoint)

Branch `develop`. The continuous sprint began at
`238d6aed27c1e8662f0d118013ef08bb801ae018`; the preceding clean checkpoint was
`38e76860df4aa02a05b8d9d5df75d74eaf90a777` (`HOUSE-01039`). This file is committed
with the one-task `HOUSE-01040` change; verify the exact new HEAD and clean tree before the next
checkpoint. **VISUAL-GATE-1 still FAILS.** Production materials, useful kitchen practicals and
the first real built-ins are present in the normal first-person route, but the kitchen has no
upper cabinets/tall appliances/backsplash, the hall aperture is dark, family-room composition
is flat and the front roof/porch and night façade remain skeletal.

Before: [Round 16 west-facing empty kitchen](visual-review/captures/house-01039-entry-r4/kitchen-facing-west.png).
After: [Round 17 first-stride kitchen entry](visual-review/captures/house-01040-clear-day-r4/kitchen-from-hall.png),
[west-facing built-ins](visual-review/captures/house-01040-clear-day-r4/kitchen-facing-west.png)
and [night kitchen entry](visual-review/captures/house-01040-night-r4/kitchen-from-hall.png).
Eleven fixed normal-game views each were also captured at
[noon](visual-review/captures/house-01040-noon-r4),
[overcast](visual-review/captures/house-01040-overcast-r4) and
[22:00](visual-review/captures/house-01040-night-r4); changed route, family and exterior frames
were actually inspected. The before camera was inside the new collidable island, so the two
kitchen review poses and strict/debug kitchen poses were deliberately relocated to measured
walkable positions; other review views remained fixed. See [Round 17 review](visual-review/README.md).

Implementation: two deterministic project-authored Blender GLBs form a 2.8 m north sink base
run with stone counter, real cutout/basin/faucet, doors/drawers/oak toe and a 2.2 m island with
stone top/painted fronts/hardware. The canonical source model/manifest/prop/material data,
source support origins, collision proxies, exact glTF source hashes, measured counter height,
physical UV scale, legal project-authored provenance, and deterministic regeneration checker
are committed together. The north run aligns with canonical STACK-E; player/work aisles remain
at least ~1.04 m. Only the measured L0_KITCHEN static chunk ceiling changes from 8 to 11 for
three extra source finish groups; no portal, Reach/vertex or other-cell limit is weakened.
Normal materials use approved map-derived painted wood/oak/stone/brushed steel finishes, not
the explicit debug blockout path. The changed kitchen and nearby strict golden images were
paired and viewed before selective updates.

Verification: both authored source GLBs regenerate bit-for-bit and pass import/scale/origin/
height checks; the 31-stage full content dry run is fresh, and the canonical world products
were deployed only into the existing build directory. **48/48 direct software-render tests**
pass, including culling equivalence; **195/195 targeted unit** and **12/12 targeted
integration** tests pass. `tools/ci/run_checks.sh` is green with 323 strict-XNA translation
units and `CNA_CNAEXT=OFF`; `git diff --check` was verified before commit. Do **not** claim a
fresh full CMake/CTest pass: BL-17 now reaches sibling CNA's required MojoShader-series writer,
which cannot write in shared FNA3D under this workspace sandbox. The patcher also has a
destructive fallback for modified shared dependencies, so no unsandboxed regeneration was
attempted. Existing exact generated house compile/link commands and direct binaries passed;
no CNA, FNA3D or sibling repository was modified.

Next highest visible value: complete the kitchen silhouette with suitable upper/tall cabinetry,
appliances and restrained worktop details, then fix the dark hall opening and investigate the
flat blue-green family room. The first exterior view still needs credible roof/porch/front-door
form and usable night lighting; do not chase invisible systems while these fixed screenshots
remain weak. Keep the strict XNA/content/provenance/canonical room/portal/collision/culling
foundation. The original 19-hour navigation process previously queried by the user has ended;
this task's fresh navigation bake took about 12 seconds, not a persistent zombie.

## Prior `HOUSE-01039` handoff

Visual-sprint handoff — 2026-09-16 (`HOUSE-01039` checkpoint)

Branch `develop`. This continuous visual session began at
`238d6aed27c1e8662f0d118013ef08bb801ae018`; the preceding clean checkpoint was
`dcffd7c6f03d5baa24fef86cefb84f86ed1f0eac` (`HOUSE-01038`). This file is committed
with the one-task `HOUSE-01039` change; verify exact HEAD/tree before the next checkpoint.
**VISUAL-GATE-1 still FAILS.** Normal gameplay uses production materials and the
foyer/hall/kitchen floor now connects under practical light, but the kitchen has no real
cabinet/appliance/fixture furnishing, dark neighboring openings remain, the exterior night
façade is almost invisible and living/family light/decor are unfinished.

Before: [Round 15 nine-view clear-day set](visual-review/captures/house-01038-clear-day-r5),
especially the [dark hall threshold](visual-review/captures/house-01038-clear-day-r5/central-hall.png).
After: [Round 16 ten-view clear-day set](visual-review/captures/house-01039-entry-r4), including
the unchanged [hall camera](visual-review/captures/house-01039-entry-r4/central-hall.png) and a
new fixed [opposite kitchen camera](visual-review/captures/house-01039-entry-r4/kitchen-facing-west.png).
Ten matching views were captured at [noon](visual-review/captures/house-01039-noon-r4),
[overcast](visual-review/captures/house-01039-overcast-r4) and
[22:00](visual-review/captures/house-01039-night-r4); the changed route and exterior frames
were inspected in each set. The threshold's porcelain floor/warm
plaster now read at all four conditions without hot spots; the new kitchen view exposes the
unfurnished room. The [22:00 front](visual-review/captures/house-01039-night-r4/exterior-front.png)
is still practically black. See [Round 16 review](visual-review/README.md) for ranked defects.

Implementation: canonical `LG_L0_KITCHEN_MAIN` now starts on as one switchable four-light group.
Its original 3.28 m omnidirectional point placements nearly coincided with the ceiling and
produced four white discs (selected atlas peak 102.9) while leaving the room dark. A narrow
downward spot removed those discs but underlit the room. The final 3.02 m downward 140° spots,
3000 K and 1000 lm each, use only the already approved Tier-S path. Selected L0_KITCHEN
artificial/daylight receiver products were deterministically rebaked/promoted; the rest of the
78-room house was not churned. Three affected strict first-person goldens were paired, viewed
and selectively updated (hall, kitchen, hall corner). A data-driven unit test protects the
default-on state, kitchen switch and borrowed hall light. The new tenth fixed review pose faces
west across the actual kitchen; prior east-facing `kitchen` showed mostly the family room.

Verification: full 31-stage repository content dry run is fresh; nav selftest green; 23/23
lighting unit tests green; **48/48 direct software-render tests green**, including the existing
culling-equivalence checks; `tools/ci/run_checks.sh` green with 323 strict-XNA translation units,
`CNA_CNAEXT=OFF` and `git diff --check` verified before commit. The direct integration binary
gave 130/131 in one process; its synthetic `TransparentPass` counter test passes separately,
matching CTest's isolated per-case mode. The serial full unit binary was interrupted by the
tool/session SIGTERM while entering the long `RandomWalkTests` (not a reported assertion);
targeted lighting/nav checks are green. Do **not** claim a fresh full CMake/CTest pass:
the current sibling CNA worktree adds a Wayland SDL test unclassified by CNA's own
non-production audit, so Ninja's CMake regenerate fails before house compilation. BL-17 in
`cna-house.md` §6 records the exact upstream diagnostic. This repository did not edit CNA.
For the data-only house change, normal `tools/ci/build_content.py` compiled source products;
the five exact changed kitchen `.cnb` products and `content/world` were copied into existing
`build/content`, and the two already-generated Ninja commands for the changed unit TU/link were
executed in `build/` without new directories, a renderer workaround or audit weakening. Once
CNA fixes/classifies its own test, re-run normal `cmake --build build` and full CTest.

Next highest visible value: primary **L0_KITCHEN** cabinetry/countertop/island/appliance
furnishing using metred, suitable, legally approved/provenanced assets; keep the route clear
and actually inspect the new opposite camera. Avoid repeating lighting coefficient passes while
the room is bare. Then fix the adjacent dark opening if it remains a dominant image defect,
exterior front form/night practicals, and living/family lighting/decor. Preserve canonical
data/portal/collision/culling, strict XNA and deterministic content. The former 19-hour host
navigation PID queried by the user had exited before this session's own fast bake work;
current navigation bake is ~12.5 seconds, not a lingering zombie.

## Prior `HOUSE-01038` handoff

Branch `develop`. This continuous visual session began at
`238d6aed27c1e8662f0d118013ef08bb801ae018`; the preceding clean checkpoint was
`49d1f0959fce5f973c6f8dc5d37e4a046426e03f` (`HOUSE-00928`). This file is committed
with the one-task `HOUSE-01038` change; verify its exact HEAD and clean tree before the next
checkpoint. **VISUAL-GATE-1 still FAILS**: normal gameplay no longer uses hash/blockout colours,
but the foyer→hall→kitchen route falls into a black empty threshold, front roof/door/porch look
skeletal, and night exterior is nearly invisible.

Before: [Round 14's eight fixed normal-game views](visual-review/captures/house-00928-entry-r1).
After: [Round 15's nine fixed clear-day views](visual-review/captures/house-01038-clear-day-r5),
including the new [reverse foyer/console](visual-review/captures/house-01038-clear-day-r5/foyer-facing-front.png),
and matched nine-view [noon](visual-review/captures/house-01038-noon-r5),
[overcast](visual-review/captures/house-01038-overcast-r5) and
[22:00](visual-review/captures/house-01038-night-r5) sets. All were actually inspected.
The [forward entrance](visual-review/captures/house-01038-clear-day-r5/entrance-foyer.png) now
shows a real upholstered chair, painted/wood/tile materials and warm practicals; the
[central hall](visual-review/captures/house-01038-clear-day-r5/central-hall.png) is readable on
the near side of its unlit kitchen opening. The [living composition](visual-review/captures/house-01038-clear-day-r5/living-composition.png)
still has flat pale seating and a bare brick chimney, and the [front façade](visual-review/captures/house-01038-clear-day-r5/exterior-front.png)
still needs form/detail/night lighting. See [Round 15 review](visual-review/README.md) for the
ranked actual-image defects, not only pixel-regression results.

Implemented: two pinned Poly Haven CC0 2K-source-mapped, metred console/chair models with
deterministic GLB transformation, 3.4 cm chair POSITION recenter, manifest/licence credits and
canonical foyer prop/material rows. The chair and console respect measured ≥1.2 m circulation,
front-door swing and portals; static batching gets a measured two-group foyer ceiling exception.
Default-on canonical foyer/hall practicals and selected deterministic daylight/artificial atlas
bakes now illuminate those rooms at day/overcast/night. Stock-XNA Basic furniture/trim receives
calibrated fixture bounce while architectural lightmapped DualTexture rendering remains intact.
The normal shell now resolves both `ContentManager` roots through strict-XNA `TitleLocation`,
eliminating a stale-working-directory texture/lightmap mismatch that made a fresh hall seem dark.
Navigation content with the new furniture proxy is fresh: conservative OBB/swept-shape pruning
and bounded candidate sampling reduced its bake from 29 minutes to **12.5 seconds**, with all
selected foyer/hall/living/family/kitchen/dining nodes and 396 local edges unchanged; 29 outdoor
candidate positions were intentionally resampled. No CNA/sharp-runtime source was edited.

Verification: full world content 16/16 fresh, strict CI gates green including 323 XNA-strict
translation units, `CNA_CNAEXT=OFF`, 22 lighting unit tests green, 48/48 direct software-Mesa
render tests green (including culling and the new actual atlas-sampling tests). Exactly six
affected pixel references were visually inspected and updated, not regenerated wholesale.
Full offscreen CTest: 1,595 registrations, zero failures among 1,587 eligible cases; 8 disabled
and 4 capture-only pixel checks skipped on that runner. `git diff --check` passed before the
commit. The initially failed sandboxed CTest was an idempotent FNA3D configure write denied by
sandbox; `LIBGL_ALWAYS_SOFTWARE=1` conflicts with EGL device selection in a per-test CTest run.
The build-access offscreen rerun without that override passed; the direct actual-software render
binary was separately green. The original host `build_nav.py` PID 936066 queried by the user
had already exited and was not killed; only this session's exact stale child bake was
gracefully interrupted after its replacement input changed.

Next highest visible value: make the **L0_HALL→L0_KITCHEN** threshold and kitchen itself
visibly habitable with a credible practical/daylight mix and legally provenanced primary
fixtures/furniture, then recapture these same fixed views. Keep room/portal/collision, strict
XNA, deterministic content/licence and culling regressions. Exterior front roof/door/night
practicals and living/family lighting/decor remain the next large image defects. Do not chase
numerical tasks or claim VISUAL-GATE-1 until several connected L0 rooms genuinely read as a house.

## Prior `HOUSE-00928` handoff

Branch `develop`; verify HEAD and worktree before continuing. This visual-sprint session started
at `238d6aed27c1e8662f0d118013ef08bb801ae018` and now has one-task commits for
`HOUSE-01037`, `HOUSE-01280`, `HOUSE-00923`, `HOUSE-00924`, `HOUSE-00925`, `HOUSE-00926`,
`HOUSE-00927`, then `HOUSE-00928`. Normal gameplay continues to use real canonical production
materials; material-hash colours require explicit blockout/debug mode. **`VISUAL-GATE-1` still
FAILS**: the front roof/door/form are thin, foyer/hall/kitchen are almost empty and underlit,
living/family have flat seating light/fluorescent plants, and night exterior lacks visible
fixtures.

Before: [Round 13's eight fixed gameplay views](visual-review/captures/house-00927-siding-r1)
and the [exact close front](visual-review/captures/house-00927-siding-r1/exterior-approach-close-after.png).
After: [Round 14's same eight views](visual-review/captures/house-00928-entry-r1) and its
[close front](visual-review/captures/house-00928-entry-r1/exterior-approach-close.png), all
visually inspected. The full-width black balcony slab is now a readable painted solid parapet;
the narrow rail keeps the approved brushed-metal source with a nonzero canonical tint. The four
porch columns/beams now use the existing approved smooth white paint instead of exposed-brick,
chipped plaster. The close balcony crop changes **1 black colour→39 painted shades** and the
support crop **1,138 distressed colours→38 clean-paint shades**. Matching
[overcast](visual-review/captures/house-00928-entry-r1/exterior-approach-overcast.png) and
[22:00](visual-review/captures/house-00928-entry-r1/exterior-approach-night.png) close views were
also inspected. Overcast is dimmer but coherent; nighttime remains almost black without local
porch/path light, so the visual gate is not being claimed.

The source correction is two existing licensed/provenanced material rows and one generated shell
role: `MAT_DOOR_PAINTED` now points at the approved fine white paint; `MAT_METAL_BALCONY` keeps
the approved brushed source with nonzero tint; `build_balcony_edge` sends the *solid* guard to
painted trim while the thin rail remains metal. Exactly three affected balcony shell GLBs and
`docs/shell-manifest.json` were deterministically regenerated. The deployed world/chunks/content
pipeline is fresh, with **511 chunks** and **173** façade/outdoor BVH instances; no stable ID,
collision/portal, UV geometry, lightmap ownership, season/time, CNA runtime call, licence source
or `CNA_CNAEXT=OFF` rule changed. The metal and white-paint maps compile into CNA `.cnb` content.
The normal-game close-entry frame-3 render property now protects against returning to a black
balcony band or chipped porch support. An initial source-JSON deploy invalidated navigation and
required its deterministic ~20-minute rebuild; the later three-GLB role split only invalidated
chunks/shading, and the final 16-stage world dry run is fresh. Do not rerun navigation merely to
inspect a renderer or shell-material tweak.

The **full direct software-Mesa render suite passes 45/45** after inspecting and selectively
refreshing 22 reference images: affected exterior debug, painted-trim first-person, balcony-
visible property, sun/season and HUD views; the older base debug image was also brought forward
from its `HOUSE-00909` palette. No blanket reference regeneration was used, and unaffected
title/font/geometry views remain untouched. All 18 culled/unculled comparisons remain equivalent.
The relinked offscreen CTest has **1,592 registrations, 1,580 actual passes, 12 disabled/skipped,
zero failures**. `tools/ci/run_checks.sh` and `git diff --check` were run; verify final log after
the commit. The project itself did not edit CNA or sharp-runtime; CNA has an independently dirty
test-fixture deletion visible in its sibling worktree, which is not staged here.

Next highest visible value is the continuous interior route, **foyer→hall→kitchen**: acquire/use
approved suitable primary furniture and fixtures, place them in canonical JSON with actual
clearances, make daytime/evening light readable, capture the same eight review views and fix the
worst image defect. The roof/front door and porch/night approach also need real finished form,
but another material-palette pass alone will not cure the nearly empty L0 interior. See
[Round 14 review](visual-review/README.md) and `plan.md` for the exact ledger.

## Prior `HOUSE-00927` handoff

Branch `develop`; verify current HEAD and worktree before continuing. This long visual session
started at `238d6aed27c1e8662f0d118013ef08bb801ae018` and now has one-task commits for
`HOUSE-01037`, `HOUSE-01280`, `HOUSE-00923`, `HOUSE-00924`, `HOUSE-00925`, `HOUSE-00926`, then
`HOUSE-00927`. Normal playable rendering uses canonical production materials; the hash-coloured
blockout is explicit debug mode only. **`VISUAL-GATE-1` still FAILS** despite a marked improvement.

Before: [Round 12 eight fixed views](visual-review/captures/house-00926-windows-r1).
After: [Round 13 same eight views](visual-review/captures/house-00927-siding-r1), all inspected
under clear 10:30 Tier S/High software Mesa. An exact normal-game
[close approach before](visual-review/captures/house-00927-siding-r1/exterior-approach-close-before.png)
and [after](visual-review/captures/house-00927-siding-r1/exterior-approach-close-after.png) pair
shows the approved wood siding changing from a flat grey slab to tiled, warm boards. Its measured
40×70 crop changes **1→111 colours** and a wall pixel `(70,69,66)→(139,105,71)`. The living
chimney's identical brick crop changes **1→12,001 colours** without replacing its licensed source
asset. Floors, brick, stone and trim now expose real source detail in the normal house. Foyer,
hall and kitchen remain nearly empty/dark; the front porch/balcony rail remains black/heavy;
roof/detail and living/family lighting/plant materials still need visible work. Clear, overcast,
17:30 and 22:00 close-front captures were visually checked; overcast dims the exterior and night
walls go dark, while the authored window rails are still disproportionately bright.

Root cause: deployed canonical façade geometry had 48 valid distinct repeat UV0s; CNA's default
HUD `SpriteBatch::Begin()` left `LinearClamp` in sampler 0 after End, clamping that whole source
to one edge texel on later frames. A direct slot assignment is forbidden by BL-16's strict-XNA
overload gate and was removed; the existing XNA-shaped HUD Begin overload now selects
`LinearWrap` while retaining premultiplied blend. The outdoor stock-`DualTextureEffect` `LM_DAY`
receiver also uses `SunShadingFor`'s weather/daylight diffuse intensity and a less saturated
hemisphere tint instead of the blue sky-display RGB as energy. Interior lighting, canonical world,
portal visibility/collision, UV1/lightmap ownership, stable IDs/provenance and
`CNA_CNAEXT=OFF` remain intact. A new 3-frame first-person render test protects the previously
invisible sampler leak. The direct affected software-render suite passes 20 tests, including all
18 culled/unculled pairs. Only three inspected stale/affected strict references were changed:
sun/season 01 and 02 for the actual siding/bake and the HUD dawn exterior frame that had remained
at the pre-`HOUSE-00923` property layout; title/font/interior/property/debug references remain
untouched. `tools/ci/run_checks.sh` passes all gates and 323 strict-XNA translation units.
The relinked full offscreen CTest passes all 1,579 actual tests among 1,591 registrations
(12 disabled/skipped; zero failures). The sister CNA repository was not edited by this task;
its concurrent source-glob change only caused an authorized reconfigure of this repo's existing
`build/` directory. `git diff --check` is clean.

Next highest visible value: resolve the porch/front balcony's near-solid black band. The canonical
`MAT_METAL_BALCONY` row has `tint:[0,0,0]`, suppressing its approved brushed-metal source even
under real light; inspect its exact geometry and capture after a material-data correction rather
than rewriting the renderer. Then place licensed primary furnishings/fixtures and useful day/night
light in foyer, hall and kitchen so the gate-to-room route stops falling back into empty darkness.
See [Round 13 visual-review notes](visual-review/README.md) and `plan.md` for the durable ledger.

## Prior `HOUSE-00926` handoff

Branch `develop`; verify exact HEAD and worktree before proceeding. This sprint session started at
`238d6aed27c1e8662f0d118013ef08bb801ae018` and committed `HOUSE-01037`, `HOUSE-01280`,
`HOUSE-00923`, `HOUSE-00924`, `HOUSE-00925`, then this `HOUSE-00926` one-task window checkpoint.
Normal gameplay still uses canonical production materials; hashed debug/blockout colours require
an explicit scene or debug flag. **`VISUAL-GATE-1` has not passed.**

Before window-role split: [eight Round 11 fixed gameplay views](visual-review/captures/house-00925-exterior-exposure-r1).
After: [the same eight Round 12 views](visual-review/captures/house-00926-windows-r1), fixed to
clear 10:30 Tier S/High software Mesa. All eight were recaptured and visually inspected. The
front windows now show pale painted frames, sashes, meeting rails and glass instead of naked blue
holes, while living/family/kitchen window-facing views also improve. Entrance, central hall and
the original living-room camera are unchanged. The front façade remains a dark, almost featureless
slab with thin roof/front-entry composition; the foyer, hall and kitchen still look empty and
underlit. Living/family have primary seating groups but flat light, fluorescent plant leaves and
a bare beige chimney. The fixed winter night sun/season views expose silver-bright window rails
against a nearly unlit wall, to assess together with façade daylight. See Round 12 in
[the visual-review log](visual-review/README.md). Screenshot evidence is not a claim of final polish.

Implementation: three stable, approved-derived outside-window material ids split their frame and
pane shell classes from portal-owned indoor skirting/borrowed-light glass. The existing exterior
BVH includes only these exact weather-facing roles, not whole room trim/glass chunks; deployed
instances increase from 104 to 170. Measured chunk exceptions are explicit and retain the stale
budget gate. Affected cell lightmaps were deterministically rebaked (78 daylight/124 switch-group
products), the 508-chunk world and pet navigation were rebuilt, the 16-stage content graph is
fresh and new IDs/licence credits/budget reports are durable. Strict golden changes were limited
to 31 visually reviewed window-affected views, not a blanket regeneration. The direct 12-test
actual-software render suite passes, including 18 culled/unculled pairs. The full 1,587-case
offscreen CTest passes all 1,575 runnable tests; `tools/ci/run_checks.sh` passes 323 strict-XNA
translation units. `CNA_CNAEXT=OFF`, canonical room/portal/collision architecture, provenance and
stable IDs remain intact. A first desktop-video parallel run received mouse input and failed one
headless camera test; that test passed alone and in the full offscreen rerun. A separate software
CTest run was obstructed by a concurrent sibling CNA source-glob change causing CMake to write a
read-only third-party dependency; build-access content-current and direct software-image checks
provided the intended verification without modifying CNA.

Next highest visible value: diagnose why approved `MAT_SIDING_WARM_WHITE`/LM_DAY presents as a
uniform dark grey façade and why the central entry/roof composition appears skeletal in the
normal front camera; use actual pixels, material/UV/lightmap evidence and recapture rather than
recolouring. Then place licensed primary furnishing and visible room fixtures in the foyer/hall/
kitchen route and make their day/evening light credible. Do not detour into distant systems or
claim the first visual gate passed. The HUD's fixed 32.1-fps overlay is not a measured throughput
result. Sibling CNA remains independent and was not edited or staged here.

## Archived HOUSE-00925 checkpoint — 2026-09-15

Branch `develop`; verify HEAD and worktree before acting. This sprint session started at
`238d6aed27c1e8662f0d118013ef08bb801ae018` (`HOUSE-00922`) and has committed
`HOUSE-01037` (`86a80f705c10`), `HOUSE-01280` (`160fd7515ff2`), `HOUSE-00923`
(`3926af7af0ea`) and `HOUSE-00924` (`3eb1073cebfd`). The latest one-task change is
`HOUSE-00925`. Normal gameplay uses production materials; hashed blockout colours are explicit
debug only. `VISUAL-GATE-1` is **not passed**.

Before outdoor exposure: [`house-00924-glass-r1`](visual-review/captures/house-00924-glass-r1).
After: [`house-00925-exterior-exposure-r1`](visual-review/captures/house-00925-exterior-exposure-r1).
All eight fixed clear-10:30 Tier-S/High software-Mesa gameplay views were inspected. The white
column in the living composition was proved to be an opaque, exterior-cell `CHIMNEY:exterior` /
`MAT_OUTDOOR_BRICK` chunk, **not a window**. Its pixel changes from clipped white
`(255,255,255)` to source-tinted `(178,164,150)`; lawn visible through the window changes from
yellow-white `(242,249,202)` to green `(110,156,92)`. Outdoor opaque receivers share the sky's
scene-referred Tier-S effect domain when a dark indoor camera sees them; indoor receivers and
furniture still follow camera adaptation. The front, entrance, hall and original living review
views remain pixel-identical. Three intentionally changed first-person golden views (kitchen,
foyer stair, master bedroom) were inspected old/new before selective update. The 20-registration
actual-software render/content subset is green, including 18 culled/unculled pairs. See Round 11
in `visual-review/README.md` and HOUSE-00925 in `plan.md`: `tools/ci/run_checks.sh` is green
(323 strict-XNA units), and the isolated full 1,586-registration CTest passes all 1,578
runnable cases. The smallest evidence-backed §22.2/§25.7 architecture correction is recorded in
`cna-house.md`; XNA-only, canonical world, stable IDs and content licences are unchanged.

Largest remaining visible defect: the exterior-front façade is a dark slab with open-looking
window holes and little front-entry depth; foyer/hall/kitchen remain empty and underlit. A second
measured defect is the living-room chimney: although its brick bitmap has visible mortar, its
150x250 screen crop is exactly one colour (`unique=1`). The exposed vertical stack also looks
like a bare box rather than a fireplace composition. Do not conflate this source-texture
presentation problem with the now-correct outdoor exposure domain. Prioritize the largest visible
defect from fixed screenshots, then recapture. `VISUAL-GATE-1` is still FAILED; do not claim that
the fixed 32.1-fps HUD overlay measures renderer throughput. Sibling CNA work is independent;
never edit, stage or commit it from this repository.

## Archived HOUSE-00924 checkpoint — 2026-09-15

Branch `develop`; verify HEAD and worktree before acting. This sprint session started at
`238d6aed27c1e8662f0d118013ef08bb801ae018` and has committed `HOUSE-01037`, `HOUSE-01280`
and `HOUSE-00923`. The `HOUSE-00924` glass checkpoint is the latest one-task change. Normal
gameplay uses production materials; hashed blockout remains explicit debug only. `VISUAL-GATE-1`
is **not passed**.

Before glass: [`house-00923-outdoor-r1`](visual-review/captures/house-00923-outdoor-r1).
After glass: [`house-00924-glass-r1`](visual-review/captures/house-00924-glass-r1). All eight fixed
clear-10:30 Tier-S/High software-Mesa normal-game views were inspected. In the living composition
the left pane changes from clipped `(255,255,255)` to sky `(175,200,228)` while the adjacent wall
is identical; family panes now show sky/fence contours. The exterior-front camera is unaffected.
Three intentionally changed strict first-person references (kitchen, foyer stair, master bedroom)
were inspected individually before selective update; debug/property/sun/season/culling refs were
stable. The glass's authored tint and alpha, XNA BasicEffect premultiplication, AlphaBlend and
DepthRead are unchanged; only the spurious dark-room exposure multiplier on unlit glass tint was
removed. Non-glass translucent surfaces keep their existing adapted path. Focused transparency,
content and 15-registration actual-software render subset pass, including the 18 paired culling
views. `tools/ci/run_checks.sh` is green (323 strict-XNA units), and the isolated full
1,585-registration CTest passes all 1,577 runnable cases. The first concurrent run flaked only
the world-load performance case while strict XNA compilation was active; it passed alone at
237 ms and in the isolated full rerun. Check the plan's HOUSE-00924 row and Round 10 in
`visual-review/README.md`.

Largest next visible defect: the tall white shape in the living composition is an **opaque
chimney, not `WIN_L0_LIVING_2`**. An explicit blockout screenshot isolates its solid cyan mesh;
the deployed `chunks.bin` reports `CHIMNEY:exterior` using `MAT_OUTDOOR_BRICK` in `EXT_ROAD`.
It and pale fence/lawn behind other windows receive the dark interior camera's up-to-6x effect
exposure, clipping outdoor lighting that should remain scene-referred. The left bay and family
panes now expose blue sky, proving the glass path works. Trace the exterior-receiver exposure
boundary from an adapted indoor camera rather than altering glass opacity or bypassing the
canonical portal/world architecture. Keep strict XNA-only Tier-S.
After that, prioritize the broad dark façade and empty/dark foyer/hall/kitchen. The selected
living/family primary seating groups exist but do not yet constitute realistic interiors. Do not
claim the visual gate passed or that the fixed HUD 32.1-fps overlay measures GPU performance.

## Archived HOUSE-00923 checkpoint — 2026-09-15

Branch `develop`. Verify `git log -4`, `git status` and the current HEAD before work. This visual
sprint session started at `238d6aed27c1e8662f0d118013ef08bb801ae018` (`HOUSE-00922`) and
committed `HOUSE-01037` furniture and `HOUSE-01280` live receiver/furniture light correction
before this `HOUSE-00923` outdoor material-parity checkpoint. No ordinary gameplay path uses
hashed blockout colours; `--scene=blockout` and `--debug-blockout-materials` are explicit tools.
`VISUAL-GATE-1` is **not passed**.

Fixed-camera before: [`house-01280-detail-r2`](visual-review/captures/house-01280-detail-r2).
After: [`house-00923-outdoor-r1`](visual-review/captures/house-00923-outdoor-r1). All eight normal
gameplay views were captured at clear 10:30, Tier S/High, software Mesa and actually inspected.
The front screenshot changes from a façade floating over sky to asphalt, grass, a white fence and
gate, walk, garage roof and main roof around the same house. The façade itself remains a flat dark
slab; windows are still visually empty from outside and almost pure white from inside. Foyer/hall
are dark/empty, kitchen has no cabinetry or appliance, living/family have only their primary groups.
See [`visual-review/README.md`](visual-review/README.md), Round 9.

Cause/fix: the old deployed 476-chunk file had 31 unregistered outdoor generator roles and 17
`Basic`-packed shell slots bound to `DualTexture` materials. A fresh source-shell audit found 20
unbaked base-finish slots, including concrete stairs and attic roof detail **inside files with
other baked receivers**. Source generators now emit canonical `materialId` extras, 17 derived
stock-XNA `Basic` material rows reuse already approved albedos/provenance and preserve physical
source UV scale, and the chunk builder refuses unknown/mismatched rows. The fresh 472-chunk file
has **zero** unknown ids or effect/vertex mismatches; the new integration test checks every chunk.
The existing live sky/sun drives unbaked outdoor Basic surfaces while indoor Basic bounce remains
conservative. Terrain/road/fence/collision, stable ids, culling and XNA-only architecture were not
bypassed. All 22 intentionally affected pixel references were inspected old/new before selective
updates. The actual software render subset is 22/22 green, including the 18-pair culled/unculled
suite. `tools/ci/run_checks.sh` passed including 323 strict-XNA translation units. The full
1,584-registration CTest also exits green in the unforced-LIBGL configuration; its capture-only,
disabled and intentional skipped render cases remain as before, so the separate software pixel
subset is the strict visual-regression evidence.

Safety: sibling `cna` presently has substantial independent, uncommitted audio/platform/X11 work
and a deleted video fixture; this sprint did not edit, stage or commit it. `sharp-runtime` and
`living-room-simulator` were read-only and clean at the status check. Use the existing `build/`
and shared ccache; the four-core owner limit and graphical-test environment are recorded below.

Next highest visible defect: pure-white window apertures and dark main-floor route. There is a
measured compositing suspect, not yet a completed fix: `TransparentPass` multiplies unlit clear
glass tint by adapted interior exposure (often ×6) before its 0.12 XNA alpha while the sky behind
it is not exposed by the same pass. Do **not** apply a second alpha premultiply; CNA's stock
`BasicEffect` forwarding already applies `DiffuseColor × Alpha`. Compare identical in/out camera
views and correct this at the material-class/light-composition boundary, then recapture. More
foyer/hall/kitchen primary furnishing follows; do not detour into distant platform/features.

## Archived HOUSE-01280 checkpoint — 2026-09-15

Branch `develop`; verify HEAD and `git status` before acting. This session began at
`238d6aed27c1e8662f0d118013ef08bb801ae018` (`HOUSE-00922`) and committed the first real
living/family static kit as `86a80f705c10f146b71d70c63f36813860914053` (`HOUSE-01037`). The
second one-task checkpoint is `HOUSE-01280`: stock-XNA Tier-S receiver ambient and static Basic
detail now follow the live cell rather than a mostly black lamp bake or XNA's constructor-white
directional key. Check `git log -3` for the exact HOUSE-01280 commit. No ordinary gameplay route
uses the hashed blockout palette; `--scene=blockout` and `--debug-blockout-materials` remain
explicit diagnostics. `VISUAL-GATE-1` is **not passed**.

Before: [`house-01037-furniture-r7`](visual-review/captures/house-01037-furniture-r7).
After: [`house-01280-detail-r2`](visual-review/captures/house-01280-detail-r2). Both are the same
eight normal-game fixed cameras, clear 10:30, Tier S/High and software Mesa. R2 makes the walls,
floors and sofas appreciably more readable but is still visibly incomplete. Largest defects, in
review order: **missing production outdoor ground/road/fence/gate/roof geometry**; almost
pure-white windows; dark, empty foyer and hall; empty kitchen; peeling boundary paint, bright
leaves and sofa style/lighting mismatch. See [`visual-review/README.md`](visual-review/README.md),
Round 8. The first high-value follow-up is the outdoor production material assignment: at the
same `EXT_ROAD` player pose looking down 10°, explicit `--debug-blockout-materials` visibly draws
road, fence, gate, landscaping and roof while normal production does not. The generated chunk
string table still contains role names (`TERRAIN_asphalt`, `FENCE_board`, `ROAD_paint`) absent from
canonical `layout.materials.json`. Outdoor generators omit `materialId` and `build_chunks.py` still
accepts those legacy roles, while unbaked outdoor receivers are deliberately `Basic` layout.
The stock-XNA production pass correctly skips a chunk whose material cannot be resolved. Fix the
generator/material-table parity through canonical data and existing content stages, not a
diagnostic colour fallback. Then diagnose **window/background brightness** with actual
inside/outside captures; do not assume the clear-glass alpha is wrong. CNA's local BasicEffect
forwarding already multiplies diffuse RGB by `Alpha`, matching XNA premultiplied
`BlendState::AlphaBlend`, so blindly premultiplying it again would darken the pane. Primary
`L0_FOYER`/`L0_HALL`/`L0_KITCHEN` furnishings remain a priority; the existing 15 static props only
furnish `L0_LIVING`/`L0_FAMILY`.

`HOUSE-01280` renderer integration proves two live lamp groups at noon use four receiver passes,
the same room with both off uses two, and canonical four-group `L0_KITCHEN` at noon uses six
(neutral opaque floor + four authored artificial maps + daylight). The extra pass is real;
GPU frame-time budget has **not** been proven by the fixed-step screenshot HUD. Do not claim that
32.1-fps overlay is a measured renderer result. Static Basic detail gets one cell-derived key and
coarse sky/fixture ambient; full per-dynamic-object key/fill/bounce (`HOUSE-01261`) remains open.

Critical test correction: a green hardware CTest render registration may be **capture-only** if
`RenderingInSoftware()` is false. Under `LIBGL_ALWAYS_SOFTWARE=1`, the first strict comparison
found three stale refs after final HOUSE-01037 furniture shifts. The actual silhouettes were
inspected and only `blockout-l0-kitchen`, `blockout-l0-living` and `fp-l0-kitchen` were corrected.
The new ambient/key composition then changed all 12 production first-person views and four
season/sun outer-skin detail views; all 16 before/after pairs were inspected and their refs
deliberately updated. A subsequent **actual software** test passed production, explicit debug,
property, season/sun and 18-pair culling suites. Run these software pixel checks in the default
sandbox with `LIBGL_ALWAYS_SOFTWARE=1`, offscreen video and dummy audio. The complete broader
CTest suite should use the escalated four-core environment below **without** forcing LIBGL
software, which otherwise makes this machine's hardware-EGL cases fail for environmental reasons.
The full 1,583-registration CTest run passed in that broader configuration; `git diff --check`
was clean. `tools/ci/run_checks.sh` passed, including 323 strict-XNA translation units, before the
HOUSE-01280 commit. Sibling CNA had an unrelated deleted video fixture at the final read-only
status check; neither CNA nor sharp-runtime was changed by this task.

## Archived HOUSE-01037 checkpoint — 2026-09-15

The 2026-09-14 handoff below is an **archive**, not the present repository state. Verify the
branch, HEAD and worktree before acting. Current starting HEAD is
`238d6aed27c1e8662f0d118013ef08bb801ae018` (`HOUSE-00922`) on `develop`.

`HOUSE-01037` completes this first static-furniture checkpoint: nine licensed/attributed furniture
GLBs and eight sRGB albedos, 15 static placements in `L0_LIVING` and `L0_FAMILY`, five heavy-model
LOD/collision preparations, source-material-preserving chunks, Reach primitive splitting and
power-of-two static textures. Current reviewed normal-game captures are
[`house-01037-furniture-r7`](visual-review/captures/house-01037-furniture-r7), with the earlier
empty route at [`house-00922-outdoor-sky-r1`](visual-review/captures/house-00922-outdoor-sky-r1).
`VISUAL-GATE-1` is **not passed**: furnishings are now visible but white upholstery clips against
almost-black walls/floors, kitchen and foyer remain empty, and the front façade lacks convincing
ground/vegetation context. The concise ranked review is in
[`docs/visual-review/README.md`](visual-review/README.md), Rounds 6–7.

The source glTF importer, scale/origin, manifest, packaging licence and exact credit-generation
checks pass. One important credit-generator correction now includes CC BY `derived` furniture in
the in-game credits. The initial furniture-aware `nav.bin` build reached roughly 50 minutes before
signal 143; an **exact conservative mesh-AABB lower bound** then reduced a full build to 19 minutes
without changing the clearance answer. Its selftest passed, including near/far equivalence. After
the first full unit suite found authored furniture crowding at two room midpoints and wedges at
three wall poses, the source placements were adjusted, **not the collision limits**. The final
world build completed in 1,282.09 s (nav 1,267.51 s), the full content graph is fresh, the full
CMake build passes and the final eight views have been inspected. Strict interior refs, the
18-pose culled/unculled comparison and the complete 1,583-registration CTest run return green
(existing intentional skips/disabled cases preserved). Two old integration
tests needed corrected pass-slice/material-count expectations for the real plant leaves and eight
new furniture materials; their stronger invariants also pass. `tools/ci/run_checks.sh` passed again
after those two test edits, including 323 strict-XNA translation units. The task checkbox is
complete in the one `HOUSE-01037` commit.
Build/test CPU usage stays pinned to cores 4,5,7,9 with the env block below. Never bypass the
Python 3.11 `jsonschema` commit hook. Do not modify CNA or sharp-runtime. CNA acquired concurrent,
unrelated X11/platform edits during this session; they are outside this repository/task and were
not touched here. `sharp-runtime` and `living-room-simulator` remain clean.

Once this checkpoint is committed, the next highest-visible-value work is to reconcile the
stock `BasicEffect`'s default white downward furniture key with the live room lighting and to
restore a plausible ambient/daylight base to L0 receiver surfaces. The artificial main atlas for
`L0_LIVING` has a 10.55 peak against 0.011 mean irradiance; using its mostly dark spatial pattern
as the sole opaque ambient-floor carrier is a measured source of black walls. Keep the Tier-S/XNA
architecture and validate any correction against the fixed eight review cameras.

## Archived handoff — 2026-09-14

This is the authoritative handoff for the next agent. The owner explicitly wants a **fresh Sol
High context with a new optimisation target: make House Simulator visually convincing**. Do not
resume this session's habit of mechanically following the dependency DAG. Read the visual brief in
§5 first, inspect the current game on the real screen, and then select the highest-leverage
dependency-valid work.

## 1. Exact repository state

| Item | State |
|---|---|
| Repository | `/rv/data/development/github.com/libcna/house-simulator` |
| Branch | `develop` |
| Parent baseline | `8c40790` — `world: author all room palettes (HOUSE-00908)` |
| Handoff commit | The commit containing this file completes `HOUSE-00907` |
| Working tree | Clean after that commit |
| Active unfinished task | None |

Verify with `git branch --show-current`, `git rev-parse HEAD`, `git status --short`, and
`git log --oneline -5` before changing anything.

The active `build/` cache is usable and configured with:

```text
CNA_SOURCE_DIR=/rv/data/development/github.com/libcna/cna
CNAHOUSE_SHARP_RUNTIME_ROOT=/rv/data/development/github.com/libcna/sharp-runtime
CNA_CNAEXT=OFF
CMAKE_C_COMPILER_LAUNCHER=ccache
CMAKE_CXX_COMPILER_LAUNCHER=ccache
```

`../sharp-runtime` is on `next` at `0c82d9b888bd` and clean. At the last check, `../cna` was on
`native-platforms-integration` at `5f583a2bece0` with an unrelated removed video fixture from
another session. That sibling changed underneath this session, so verify it again but do not touch,
restore, stage or commit its state. This session did not modify either sibling.

## 2. Mandatory display and build rules

The owner requires **at most four CPU cores** for compilation and tests. Use the existing `build/`
directory and pin every expensive command to four cores:

```bash
PATH=/home/robertvokac/.pyenv/versions/3.11.9/bin:/usr/local/bin:/usr/bin:/bin \
CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv \
CMAKE_BUILD_PARALLEL_LEVEL=4 CTEST_PARALLEL_LEVEL=4 MAKEFLAGS=-j4 \
OMP_NUM_THREADS=1 PYTHON_CPU_COUNT=4 \
taskset -c 4,5,7,9 <command>
```

Keep Python 3.11.9 first in `PATH` when committing because the hook needs `jsonschema`. Never
bypass the hook. Graphical tests must add:

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
```

Do not use `DISPLAY=:99`; it is a real visible display here. Conversely, the next session should
launch `cna-house` normally on the real screen for the owner because that visual audit is explicitly
requested. Stop the game cleanly after inspection.

All project rules remain absolute: XNA-only runtime, `CNA_CNAEXT=OFF`, no sibling-repository edits,
the house is data rather than C++, and one completed task plus its `plan.md` checkbox per commit.

## 3. Work completed by HOUSE-00907

`HOUSE-00907` replaced the generated house shell's diagnostic placeholder materials with real
authored ids:

* each cell's floor, wall, ceiling and trim come directly from the `HOUSE-00908` palette fields;
* window glass comes from `layout.openings.json`, including obscured bathroom glazing;
* stair finish comes from `layout.stairs.json`;
* basement outer skin and chimney use brick water table, upper outer skin uses warm-white siding,
  roofs use shingles, eaves use white soffit, and metal uses balcony/gutter finishes;
* all 99 generated GLBs use 43 real material ids, with no surviving `BLOCKOUT_*` material;
* each glTF slot retains structured `materialId`, `surfaceClass`, and `lightmapReceiver` extras;
* `build_chunks.py` validates shell ids against `layout.materials.json` and takes `alphaMode` from
  the real row rather than guessing that glass is transparent from a class name;
* one real finish can be both a receiver and detail, so keeping `surfaceClass` separately prevents
  bluestone floor/step and plywood trim/structure from being incorrectly merged;
* real grouping reduced chunk-budget exceptions from seven cells to five.

The final generated measurements are:

```text
Material gate: 96 cell maps, 99 GLBs, 115 authored material rows
Unwrap:        78 cells, 5,286 receiver faces, 35,306 detail faces, 78 atlases
Chunks:        459 chunks / 93 cells, 108,023 vertices
Packed bytes:  3,428,956 (raw 5,185,104), five documented exceptions, no problems
```

The generated shell is under `build/shell`, its unwrapped form is under `build/shell-lm`, and the
deployed chunk file is `content/world/chunks.bin`. `docs/shell-manifest.json` matches the final
generator and shell bytes. `tools/ci/run_checks.sh` now includes a `shell-materials` gate.

## 4. Verification paid by this commit

All builds and tests were limited to four cores. Graphical tests used offscreen video and dummy
audio.

```text
Generator self-test:                         passed
Shell material assignment gate:             96 maps / 99 GLBs passed
Chunk builder self-test and production run: passed
Build:                                       complete, warning-clean
Complete ctest suite:                        1,555 non-failures; 9 initial failures
Focused retry, headless weather:             passed
Focused retry, SaveStoreTest.*:              10 / 10 passed outside sandbox
Static gates:                                tools/ci/run_checks.sh all green
Whitespace:                                  git diff --check clean
```

The first eight SaveStore failures were sandbox permission failures when writing the measured user
save directory. All SaveStore tests passed outside the sandbox. The remaining headless weather
case passed immediately in isolation. No HOUSE-00907 failure remained.

## 5. The next session's visual objective

Open a **new clean Sol High context**. Its objective is not “continue the dependency DAG”; it is:

> Make House Simulator finally look visually convincing.

Begin by launching the current `cna-house` binary on the owner's real screen and walking a useful
exterior/interior route. Record the largest visible blockers with screenshots or precise poses.
Then map those blockers to the smallest dependency-valid tasks and execute the highest-impact one.
Architecture and the ledger still constrain implementation, but task ordering should serve the
visual result.

The strongest known blocker before that audit is that real material ids now reach `chunks.bin`,
but `StaticGeometryPass` still computes a hashed diagnostic `BlockoutColour` instead of binding the
authored albedo/tint. The canonical DAG offers `HOUSE-00909`–`HOUSE-00911` for lightmap baking and
`HOUSE-00912` for lightmap loading plus the shell `DualTextureEffect` draw path. A visually driven
agent should inspect the real frame first, then determine whether accelerating the real material
binding/render path (while preserving honest dependencies and task acceptance criteria) is the
highest-leverage route. Do not claim visual completion merely because offline material ids exist.

The previous real-screen inspection, before HOUSE-00907, showed a functional 60 fps walk/HUD but
mostly diagnostic magenta/green blockout rendering. HOUSE-00907 fixes the offline identities, not
that runtime presentation. The new context should use this as the baseline to beat.
