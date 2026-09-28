# HOUSE-03642 — slow-step support and porch deck

Final correction evidence, 2026-09-28. HOUSE-03642 and HOUSE-03643 share the
controller-regression checkpoint: the complete tour exposed both problems.
The separately requested filming tour remains OPEN until its full GPU review.

## Reproduction and correction

* Garage → mudroom: the slow filming policy stalls after 36 views at point 850,
  feet approximately `(9.10, 0.44, -16.36266)`. Fixed-step diagnostics show a
  few millimetres of forward progress followed by the existing 20 mm
  depenetration. The nearest capsule contact has a steep edge/wall normal even
  though the real ramp face directly beneath its centre is walkable.
* `GroundProbe` now confirms a genuinely sloping stair-ramp face with the existing
  `RayCastCell`, only after a non-walkable constructed contact. The ray's 2 mm
  origin lift does not increase the 50 mm reach beneath the original foot.
  Slope remains 46°, step remains 220 mm. Terrain and flat-step contact behaviour
  are unchanged; neither can become replacement support. Ground material/kind
  come from the actual supporting shape, not the first edge hit.
* Porch: the visible flat timber deck is at +0.57 m, but the open-cell floor
  collider was omitted in favour of the smoothed heightfield. Normal forward
  walking stalls at the front edge around z=-11.79. L0_PORCH now declares
  `floorSupport: "slab"`; the existing floor rectangle, depth and aperture cuts
  produce its real support. Other open ground cells retain terrain by default.
  No visible geometry, ambient light, collision tolerance or new system changes.

The recorded garage unit pose fails with the original Ground.cpp and passes
with the correction. Its accompanying wall/floor/hole/reach control case already
passed before; it is not claimed as a second before-failing regression. Old-ground
production GPU input likewise cannot reach the mudroom. Evidence logs:
`/tmp/house-03642-negative.log`, `/tmp/house-03642-gpu-before.log`.

## Actual application evidence

Radeon 780M, stock OPENGLES3/Tier S+E, SDL offscreen/EGL surfaceless. DISPLAY and
WAYLAND_DISPLAY are unset: **no physical monitor window**. Ordinary real game
time and the production player controller consume fixed W-equivalent intent;
this is not a claim that a human manually pressed keys in these focused cases.
No camera/body teleport, noclip, jump or fast-walk workaround during a crossing.

`/tmp/house-03643-final-gpu.log`: six crossings pass — garage slow up (35% normal
input), garage normal up, garage normal down, and normal porch-to-foyer. The
pedestrian gate passes both directions at normal walk. The short C toggle/control-return
case passes separately in the same invocation; it is not full filming acceptance.
The retained images visibly advance into the mudroom/garage/foyer; before frames
repeat at the blocked edge. [Frame index](house-03642/index.json) retains original
PNG and lossless WebP hashes, complete focused capture sequences and source paths.

Porch **before x=0.5 / after x=0** are approach evidence, not pixel-matched images.
The earlier final test aimed x=0.5 into the right jamb of the 1 m doorway and was
rejected. The centred test now continues into the foyer instead of stopping at
the deck. Earlier rejected stair-only grounding and reverse-filming evidence
remain in build/test-output; they are not acceptance evidence.

## Gate-approach content correction — HOUSE-03643

Measured global OBB 1966 is NB_POLE_03: centre `(0,5.25005,0.8)`, half extents
`(0.13,5.25,0.13)`, yaw 0. Its front face stops the capsule at z=1.23, directly
on the pedestrian gate axis. Replacing Ground.cpp with its original version
still failed there before the pole moved. Actual W-equivalent GPU input likewise
stops at `(0,0.040475249,1.230000019)`; this is not a kerb or a grounding defect.
The same starting pose now reaches EXT_WALK at z<-2; the return reaches EXT_ROAD
at z>4. Neither uses fast walk, a detour, collision exemption or teleport.

The five poles, four intermediate lanterns and four spans translate +2.4 m in X,
and all nine linked light emitters follow. Existing assets, 25/50 m spacing,
50 m spans, Z positions and full-height solid columns remain. The generator
selftest checks all emitter/lantern pairs and span endpoints. The collision claim
checks the full 1.20 m approach corridor and rejects the original pole placement.
The initial collision selftest also exposed two stale claims: vehicles have used
ordinary prop proxies since HOUSE-00847 (not the empty legacy vehicle table), and
the authored porch slab is now the deliberate exception to terrain-supported
ground cells. Both checks now protect the real owner/proxy/yaw and deck support.

The gate's former obstruction is **invisible** in the actual before image. Source
inspection confirms audit BUG-023: the current game never reads/draws the separate
neighbourhood mesh library. The authoritative plan explicitly identifies that
renderer as OPTIONAL (HOUSE-00847's completion note). This correction does not
activate it, invent a draw system, or claim to make street furniture visible.
It removes the already-existing invisible barrier from the required normal path.
All retained gate
frames show the real clear approach and continuous movement after the correction.

## Broader verification and rejected drafts

`/tmp/house-03643-sloping-tours.log`: fast GrandTour passes **90 cells, 564 stops,
92798 fixed steps, 19 collision detours**. Slow filming policy passes **90 views,
294080 steps** through the real collision/controller; this is additional route
evidence, not a substitute for full GPU filming acceptance. No GrandTour route,
steering allowance, tolerance or assertion was changed.

Broader constructed-face and stair-top replacements were rejected: they changed
the terrace slider response and stopped the fast tour at stop 51 in EXT_TERRACE.
Original Ground.cpp passes there after the pole moves, confirming the regression.
The final correction only accepts genuinely sloping stair faces, and the new
flat-step/wall control guards that boundary. Rejected sequences/logs are retained
locally, not presented as final captures. Temporary detour/query experiments were
fully removed; the tracked GrandTour test remains byte-for-byte unchanged.

Final unit/static results are recorded in plan.md/handoff.md at the checkpoint.
Slope, holes, probe reach, stair traversal and 20-minute random walking remain
covered. No performance improvement or uncontended FPS result is claimed from
captured HUD values. No CNA/sharp-runtime source or build configuration changed.
