# Bounded fix pass — HOUSE-03631, Round 180

The four S1/S2 findings of [Round 179](house-02714-final.md), and one S2 found while
verifying them, are corrected at their root causes with the existing generator, content
pipeline, schedule classes, bake tool and stock XNA effects. Captures are real hardware-GPU
frames: Radeon/OPENGLES3 (amdgpu, PCI 0000:c3:00.0, per the capture process's DRM fdinfo)
on SDL's invisible `offscreen` surface, `EGL_PLATFORM=surfaceless`, `DISPLAY`/
`WAYLAND_DISPLAY` unset, dummy audio, an isolated `XDG_DATA_HOME`, Tier S, High, clear
weather, frozen 10:30 and 22:00. No monitor window was opened. Captures are local and
Git-ignored under `build/test-output/house-03631/`: `before/`, `after/`, `tree-before/`,
`tree-after/`, `lantern-before/`, `lantern-after/` and `final/`.

## S1 Z-EXR — the shed showed the sky through its roof

**Cause.** `fence_gen.py` emitted the roof and both gables as outward faces
only. Under ordinary back-face culling a single-sided roof has nothing to draw
from inside, so every upward view in `EXT_SHED` showed the sky, by day and at
night (`before/in-*`).

**Fix.** The generator adds the roof's underside: two pitched timber faces
parallel to the roof, running from the top of the room's own wall face to one
shared apex line below the ridge, plus inner gable triangles that close exactly
that section. Global culling is unchanged.

**Regression.** `fence_gen.py --selftest` now casts sight lines from twelve
interior points (96 directions each) and from 36 exterior points at every
target of a 27-point grid over the shed. The first surface along each ray must
face the eye; an interior ray may otherwise leave only through the door or
window rectangle. Before the fix: **371 interior and 14 exterior leaks**
(the latter looking up through the doorway); after: **0 and 0**. The
existing winding claim covers the new faces.

## S2 Z-EXR — the shed was dark by day, inside and out

**Cause.** Two separate defects. (1) `build_chunks.place_outdoors` filed the
whole generated shed by largest plan overlap, which is the enclosed
`EXT_SHED` (10.24 m² against the garden ring's 2.72 m²). The runtime lights an
enclosed cell's Basic surfaces as an interior — 18 % sky bounce and no sun key —
so the sunny east elevation drew black at 10:30 (visible in Round 111's
`house-03267-after-day` capture too). (2) The shed's lining is unbaked Basic
geometry with no `LM_DAY` receiver, and its one practical lamp used the dusk
class, so nothing lit it during the day.

**Fix.** The generator now writes two files: the weather skin `STRUCT_SHED.glb`
(outer cladding, reveals, trim, glass, roof, slab edge) and the lining
`EXT_SHED.glb`, named for its cell (inner wall faces, inner gables, underside,
floor, the leaf posed inside). `place_outdoors` no longer lets an enclosed
(`visibilityHint: opaque`) cell compete for placed outdoor geometry, so the
skin is filed with `EXT_GARDEN` and lit as outdoors; the lining is filed by name.
`LG_EXT_SHED_MAIN` moves from `SC-DUSK` to the all-day `SC-CIRC` class already
used by the other service and store rooms (`HOUSE-03634`/`03636`). No
material, texture, light, exposure or ambient constant changed.

**Measured.** Shed facade, linear luminance of the 750×520 facade crop at 10:30:
**0.0406 → 0.2929**; at 22:00 0.0128 → 0.0140 (the night look is retained).
The interior now reads by day (`after/in-west-t10.5`, `after/in-door-up-t10.5`).
Chunk budget: `EXT_SHED` 15 → 12 (declared ceiling tightened to the measured
count), `EXT_GARDEN` 6 → 10 (new declared exception: siding, trim, roof and glass
join it; the slab edge reuses the beds' timber). Net +2 draws. Only
`chunks.bin`, `layout.lights.json` and the world manifest changed for this item;
collision, navigation, coverage and sky exposure were byte-identical.
`build_chunks.py --selftest` pins the placement rule with an open-ring/enclosed-hut
fixture that fails with the old rule; unit
`LightScheduleTests.TheEnclosedShedIsLitByItsOwnLampThroughTheDay` pins the lamp.

## S2 Z-L1 — a mature crown inside L1_BED3

**Measured cause.** The second `VEG_MAPLE` instance at (-13.50, 0, -12.00),
yaw 197°, scale 0.78, has a transformed crown spanning x -16.67..-9.71,
z -15.97..-7.28 and y 0..9.36. **232 of its vertices lie inside L1_BED3's
finished walls** (the room box inset by the thinnest wall half, see below).
Its trunk stood 0.8 m from the house corner.

**Fix.** A search over the placement grid (0.2 m steps) found the nearest
position whose whole crown stays 0.6 m clear of every room box in plan (wall
plus eaves) without landing on a path or crowding a trunk: **(-13.50, 0, -9.80)**,
2.2 m toward the road. Yaw, scale, asset and group are unchanged.

**Regression.** `validate_props.py` (run by `run_checks.sh`) now places every
exterior planting with `build_chunks.place` over the same LOD0 meshes, and every
prop in an exterior cell with its own transform, and rejects any vertex inside a
room. Room boxes run to their walls' centre lines, so the rule insets them by
0.075 m, the thinnest wall half: a leaf tip 12 mm past an exterior wall's centre
line (`VEG_SHRUB` #33 at B1_STOR1) is inside the wall, not the room, and is not
reported. `tree-before/` vs `tree-after/bed3-corner*` shows the leaves gone from
the bedroom corner at both times; `front-yard-tree` and `side-yard-corner` show
the tree free-standing.

## S2 Z-L1/Z-L0M/Z-EXR — wall lanterns inside the master bedroom and sunroom

Found while verifying this pass: the render suite's culled/unculled comparison
failed at `l1-master-bed` (588 pixels, also failing on the unchanged HEAD
content), and the new placement rule reported four props. The lantern model's
origin is its centre, and the rear balcony's two and the terrace's two stood on
their wall's centre line: each showed a dark half-dome through the finished wall
into L1_MASTER_BED or L0_SUNROOM (glowing inside the sunroom at night), while
barely 5 mm reached the outside. Two also sat on openings: balcony lantern 1 on
window W2's head, terrace lantern 1 on the slider head.

**Fix.** All four move onto the outside face with the front lanterns' measured
offset (centre 0.18725 m outside the wall line), and each light keeps the front
lanterns' relation to its fixture. Balcony lantern 1 moves over the slider
(W2 leaves no wall left of it); terrace lantern 1 moves 0.3 m clear of the
slider, mirroring lantern 2's clearance from window W5. The terrace pair is
baked onto the sunroom's skin: its 0.03 calibration only compensated a source
buried in the wall, and 29 keeps the accepted pool energy (receiver mean
0.1325 → 0.1345). L0_SUNROOM alone was re-baked and promoted with the existing
tool; its daylight atlas is byte-identical. `lantern-before/` vs
`lantern-after/` shows the bedroom and sunroom walls clean at both times and
complete, lit lanterns on both facades.

## S2 Z-EXF/Z-EXR/Z-STR — crowns bright at night

**Cause.** Cutout foliage is drawn with stock `AlphaTestEffect`, which has no
lights in XNA: `DiffuseColor` is all the light a leaf gets. `AlphaTestPass` set
it to the camera exposure alone (0.82 outdoors), identical at 10:30 and 22:00.
The whole tree — bark included, one material — therefore stayed at morning
brightness at night against a dark garden and facade.

**Fix.** In sky-open exterior cells the pass now scales the albedo by the light
a BasicEffect receiver in the same cell gets: the ambient floor, the open sky,
half the sun/moon key (a crown's leaves face every way) and the active fixtures'
key and spill, at the exterior's unit exposure, clamped to one per channel
(`OutdoorFoliageMultiplier`). Interior cutouts keep the camera exposure. The
two Basic fixture shares moved from a source-local namespace into
`StaticGeometryPass.hpp` so both passes read one value. No shader, pass,
texture or exposure model was added or changed.

**Measured.** Unit `LightingSystemTests.OutdoorFoliageFollowsTheSkyFromMorningToNight`
(real content, clear, 15 January): the front-yard crown's light is **0.966 at
10:30 and 0.042 at 22:00** (previously 0.82 at both). Rendered crown crop,
linear luminance: 22:00 **0.0262 → 0.0022**; 10:30 0.1526 → 0.1516, so the
reviewed daytime appearance is retained. Front path, porch and shed views at
22:00 keep their practical lighting (`final/`).

## Checks and limitations

Against CNA `2c70eaf0f` (which fixes BL-18): unit 1542/1542; hardware-GPU integration
172 PASS / 3 opt-in SKIP / 0 FAIL, including the 90-cell GrandTour and filming tour after
the tree moved; strict XNA 356 units clean; `fence_gen`, `build_chunks`, `validate_props`,
`porch_lantern` (its pinned terrace placement updated to the corrected one) and every other
static gate green. The software-GL render suite on a private Xvfb: the culled/unculled check
now passes (worst 0.022 %; it failed on unchanged HEAD content at l1-master-bed's lantern),
45 cases pass, and 14 reference comparisons fail. Those references were last refreshed on
2026-09-26, before several 2026-09-27/28 geometry, lighting and UI corrections, and this
pass changes exterior foliage, the shed and the lanterns on purpose; the inspected refresh is
`HOUSE-02713`. This round is fixed-pose GPU evidence plus the whole-tour transport tests, not
a new whole-property walkthrough. The S3 attic mottling and plain street remain logged S3
backlog; no S4 work was scheduled.

Time: S1 about 0.75 h; S2 about 3.25 h across the four S2 items, within R12's 4.5 h S2/S3
limit.
