# Scope-reduction record, 2026-09-21 (`HOUSE-03201`, `HOUSE-03205`)

**Frozen history. Nothing here is a requirement, and nothing here is scheduled.** The current plan
is [`plan.md`](../../plan.md); read it, not this file, to decide what to do.

`HOUSE-03206` (2026-09-22, [ADR-0016](../decisions/ADR-0016-final-scope-reduction.md)) moved these
sections out of `plan.md` verbatim so that the active ledger holds only current requirements. They
record, for traceability, what the first and second scope reductions cancelled and why, where
every legacy phase's open work went, how the second reduction was estimated, and its planning
corrections. Anchors inside the text may still point at `plan.md` sections that were renamed; the
complete `plan.md` these sections came from is at commit `174f2ba`. Task ids named here as
cancelled stay cancelled and are never reused. The cancellations and merges of the third reduction
are in `plan.md` itself, under *Cancelled or merged by the final scope reduction*.

---

## Cancelled by the second scope reduction (`HOUSE-03205`)

146 tasks, cancelled on 2026-09-21 by `HOUSE-03205` under
[ADR-0015](docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md). "Merged into X" means X's
acceptance now covers the cancelled task's purpose, and X carries an `amended:` line saying so. Each
entry names the milestone it came from. Its full text is in the legacy ledger (legacy ids) or in
`plan.md` at `5927073` (ids created by `HOUSE-03201`). Ids are not reused.

### Quality tiers replace "C5 everywhere"

- ~~HOUSE-03443 — Bring the garage to C5~~ (was M6) — merged into `HOUSE-03442`: the garage is a main room, raised to C4 with the office
- ~~HOUSE-03449 — Bring the street and neighbourhood to C5~~ (was M6) — `Z-STR` is scenery with a C3 target (tier table); the parked cars (`HOUSE-00847`) complete it, and neighbour lights and impostors are optional
- ~~HOUSE-03451 — Bring the ground-floor principal route to C5~~ (was M6) — a redundant wrapper: the ground-floor rooms are judged by G4 (`HOUSE-03452`) and G5 (`HOUSE-03480`) with every other zone's

### Reuse is allowed: uniqueness and density quotas removed

- ~~HOUSE-00972 — Implement the anti-repetition validator (≤ 14 uses per model overall, ≤ 6 per cell, whitelist)~~ (was M3) — a uniqueness quota (≤ 14 uses per model, ≤ 6 per cell); rule R8 now allows reuse, varied by tint, scale and arrangement
- ~~HOUSE-01027 — Run the anti-repetition validator over the whole house and fix every violation~~ (was M6) — runs the cancelled anti-repetition validator
- ~~HOUSE-01028 — Run the density check: every cell within its §59.1 band~~ (was M6) — the §59.1 density bands are replaced by tiered recipes; D3 is proved by the G3 review and `zone_scoreboard.py`

### Furnishing kit consolidation

- ~~HOUSE-00983 — Grouped: acquire and prepare the 40 kitchenware and food prop models~~ (was M3) — merged into `HOUSE-00984` and the fill kits (`HOUSE-00973`); the kitchen's own dressing already exists
- ~~HOUSE-01035 — Author the unbranded player-car model, LOD1/LOD2 and collision proxy to the fixed acceptance bar; complete the four-view visual sign-off~~ (was M4) — a bespoke car is not needed; the household car comes from `HOUSE-00847`'s shared car family
- ~~HOUSE-01036 — Export, manifest, validate and build the authored player car through the real `.glb`/`.cnb` pipeline~~ (was M4) — the pipeline for the cancelled bespoke car
- ~~HOUSE-01197 — Author and place the door and window hardware props (handles, latches, hinges, sash locks)~~ (was M2) — the finished joinery already carries hardware (`HOUSE-00932`, `00940`, `00942`); where a door lacks it, its zone's C2 task reuses that set; hinges, latches and sash locks are S4 detail
- ~~HOUSE-02682 — Author curtains and blinds for the 34 windows that have them~~ (was M6) — merged into `HOUSE-02681` (one generated curtain family and one blind family, placed by recipe)
- ~~HOUSE-02688 — Acquire and place the remaining wall art (landscapes, abstracts, botanicals, posters), all CC0, all manifested~~ (was M6) — merged into `HOUSE-00982` (a small CC0 wall-art set with frame variants)

### Representative tests replace permutation matrices and exhaustive suites

- ~~HOUSE-00854 — Render tests: 6 poses covering the neighbourhood at 3 LOD distances, day and night~~ (was M10) — the neighbourhood at three LOD distances; the street approach is in `HOUSE-01653` and `HOUSE-02402`
- ~~HOUSE-00917 — Render tests: a materials sheet scene plus 12 room poses at the new materials~~ (was M6) — merged into `HOUSE-01275`
- ~~HOUSE-01032 — Render tests: one pose per interior cell (78 scenes) at a fixed time and lighting~~ (was M6) — one pose per interior cell (76 scenes); merged into `HOUSE-01275`
- ~~HOUSE-01273 — Test: each of the 84 groups switches, changes its room's level, and changes the neighbour's borrowed light~~ (was M5) — the every-group switching test (84 groups); `HOUSE-03401`'s tests and `HOUSE-01275` cover baseline behaviour
- ~~HOUSE-01274 — Test: the 12 observable-behaviour rows of `cna-house.md` §30, each asserted on a measurable proxy~~ (was M5) — the 12 §30 rows; baseline day/night lighting is covered by `HOUSE-03401`'s tests and `HOUSE-01275`
- ~~HOUSE-01754 — Render tests: light rain, heavy rain, rain seen from indoors through a window, rain under the porch, wet driveway~~ (was M7) — merged into `HOUSE-01653`
- ~~HOUSE-02571 — Audit the existing suite against §70 and produce a gap list~~ (was M12) — this plan names the kept tests; suites for cut or finished systems are not gaps
- ~~HOUSE-02572 — Grouped: complete the world-data unit tests (schemas, loader, ids, adjacency, plane membership, stair maths)~~ (was M12) — world loading is covered by the phase-5 loader and validator tests and the grand tour
- ~~HOUSE-02573 — Grouped: complete the visibility unit tests (clip, reduce, containment property, area cutoff, depth caps, hysteresis)~~ (was M12) — visibility shipped with its tests (phase 9); `HOUSE-03224` re-proves the door states
- ~~HOUSE-02574 — Grouped: complete the collision unit tests (every primitive against analytic answers, 200 cases each)~~ (was M12) — collision is covered by the phase-7 tests, the grand tour, the random walk and the inside-geometry tests
- ~~HOUSE-02576 — Grouped: complete the weather unit tests (rate limits, determinism, seasons, type derivation, integration channels)~~ (was M12) — the weather core shipped with its tests (phase 26)
- ~~HOUSE-02577 — Grouped: complete the astronomy unit tests (sun, moon, phase, sidereal, twilight)~~ (was M12) — astronomy shipped with its tests (phases 22–24)
- ~~HOUSE-02580 — Grouped: complete the audio unit tests (portal path, voice manager, footstep cadence, bags)~~ (was M12) — audio tests live with their tasks (`HOUSE-01938` and each M8 task's `verify:`)
- ~~HOUSE-02581 — Grouped: complete the lighting unit tests (daylight model, flood, exposure, colour temperature)~~ (was M12) — lighting shipped with its tests (phase 16); `HOUSE-03401` adds the schedule's
- ~~HOUSE-02582 — Implement the ID-stability test against the golden list (adding is fine, renaming fails)~~ (was M12) — superseded by the `world-ids` gate (`tools/world/id_golden.py --check`, `HOUSE-00399`), which fails on a renamed id
- ~~HOUSE-02583 — Grouped: complete the integration tests for doors and visibility (all 62, both directions, both sides)~~ (was M12) — merged into `HOUSE-03224` (the door-state matrix over the static poses)
- ~~HOUSE-02584 — Grouped: complete the integration tests for lights and illumination (all 84 groups, plus the 12 §30 rows)~~ (was M12) — covered by `HOUSE-03401` and `HOUSE-01275`
- ~~HOUSE-02588 — Grouped: complete the long-run integration tests (30 simulated days: lunation, weather variety, no leak, no drift)~~ (was M12) — a 30-simulated-day run is not a DONE requirement; `HOUSE-02598` and `HOUSE-02784` cover stability
- ~~HOUSE-02590 — Grouped: complete the render regression scenes for time of day (64 scenes)~~ (was M12) — 64 time-of-day scenes; merged into `HOUSE-01275` and `HOUSE-01653`
- ~~HOUSE-02591 — Grouped: complete the render regression scenes for weather (48 scenes)~~ (was M12) — 48 weather scenes; merged into `HOUSE-01653`
- ~~HOUSE-02592 — Grouped: complete the render regression scenes for room lighting (80 scenes across 20 rooms)~~ (was M12) — 80 room-lighting scenes; merged into `HOUSE-01275`
- ~~HOUSE-02593 — Grouped: complete the render regression scenes for doors, culling sanity, tiers, characters, exterior and UI~~ (was M12) — door poses and culling sanity are proved by `HOUSE-03224`, the exterior by `HOUSE-01653`, the UI by `HOUSE-02530`
- ~~HOUSE-02596 — Complete the realism-validation suite over the layout and every asset~~ (was M12) — the realism checks exist (`HOUSE-00360`, `HOUSE-00187`); `HOUSE-03303` covers props
- ~~HOUSE-02597 — Complete the performance test suite (10 scenarios, all budgets, nightly, logged)~~ (was M12) — a nightly performance suite on this shared machine would measure the other agents; `HOUSE-02403` and `HOUSE-03071` measure at milestones
- ~~HOUSE-02599 — Measure and record test-suite runtime; keep the `unit` target under 60 s~~ (was M12) — suite-runtime bookkeeping is not a DONE requirement

### Measurements folded into the representative performance measurement

- ~~HOUSE-00853 — Measure the neighbourhood's draw-call and triangle cost from the road pose against budget~~ (was M10) — merged into the representative measurement `HOUSE-02403`
- ~~HOUSE-00911 — Measure the lightmap atlas count, size and bake time; adjust texel density to fit the 60 MB budget~~ (was M10) — merged into `HOUSE-00915`
- ~~HOUSE-01031 — Measure draw calls, triangles and texture memory over the furnished house against budget~~ (was M10) — merged into the representative measurement `HOUSE-02403`
- ~~HOUSE-01276 — Measure the lighting system and the extra additive passes against budget~~ (was M5) — merged into the representative measurement `HOUSE-02403`
- ~~HOUSE-01654 — Measure the sky and cloud cost~~ (was M7) — merged into the representative measurement `HOUSE-02403`
- ~~HOUSE-01753 — Measure the rain system at intensity 1.0 against the particle and CPU budgets~~ (was M7) — merged into the representative measurement `HOUSE-02403`
- ~~HOUSE-01799 — Measure the snow system at intensity 1.0 against budget~~ (was M7) — measures a feature that is now optional; heavy rain is one of `HOUSE-02402`'s scenarios
- ~~HOUSE-01888 — Measure hail at intensity 1.0 and wind at 20 m/s against budget~~ (was M7) — measures a feature that is now optional; heavy rain is one of `HOUSE-02402`'s scenarios
- ~~HOUSE-01936 — Measure the audio update cost against the 0.25/0.60 ms budget~~ (was M8) — audio is now a handful of loops and one-shots; its cost is visible in `HOUSE-02403`'s CPU numbers

### Optimisation only where a measurement demands it

- ~~HOUSE-00914 — Implement the texture streaming-free residency for the `house-*` packs (load-at-start for now)~~ (was M10) — every pack loads at start today; `HOUSE-02451` decides whether anything else is needed
- ~~HOUSE-00974 — Implement the detail-set tagging (`essential`/`dressing`/`micro`) and its culling~~ (was M10) — folded into `HOUSE-02404` as a candidate technique, planned as a new M10 task only when a measurement chooses it
- ~~HOUSE-02392 — Implement per-category LOD assignment from the manifest~~ (was M10) — folded into `HOUSE-02404` as a candidate technique, planned as a new M10 task only when a measurement chooses it
- ~~HOUSE-02393 — Implement the global `lodBias` from the quality tier and settings~~ (was M10) — folded into `HOUSE-02404` as a candidate technique, planned as a new M10 task only when a measurement chooses it
- ~~HOUSE-02394 — Generate LOD1/LOD2 for every asset above the triangle threshold and verify the silhouettes~~ (was M10) — folded into `HOUSE-02404` as a candidate technique, planned as a new M10 task only when a measurement chooses it
- ~~HOUSE-02396 — Implement the detail-set (`essential`/`dressing`/`micro`) culling by quality, distance and portal kind~~ (was M10) — folded into `HOUSE-02404` as a candidate technique, planned as a new M10 task only when a measurement chooses it
- ~~HOUSE-02397 — Implement static-chunk sub-range culling for large chunks~~ (was M10) — folded into `HOUSE-02404` as a candidate technique, planned as a new M10 task only when a measurement chooses it
- ~~HOUSE-02398 — Optimise the render list: sort key packing, stable partitioning, no per-frame allocation~~ (was M10) — folded into `HOUSE-02404` as a candidate technique, planned as a new M10 task only when a measurement chooses it
- ~~HOUSE-02399 — Optimise the state tracker and measure the state-change reduction~~ (was M10) — folded into `HOUSE-02404` as a candidate technique, planned as a new M10 task only when a measurement chooses it
- ~~HOUSE-02400 — Evaluate `DrawInstancedPrimitives` for vegetation and the neighbourhood; adopt if it measures better (from HOUSE-00093)~~ (was M10) — folded into `HOUSE-02404` as a candidate technique, planned as a new M10 task only when a measurement chooses it
- ~~HOUSE-02406 — Verify the Web tier budget is reachable on desktop via `--quality web`~~ (was M10) — merged into `HOUSE-02405`
- ~~HOUSE-02407 — Verify the Android tier budget is reachable via `--quality android`~~ (was M10) — merged into `HOUSE-02405`
- ~~HOUSE-02408 — Add the nightly performance CI job asserting every budget~~ (was M10) — a nightly performance job on this shared machine is noise (see `HOUSE-02597`)
- ~~HOUSE-02452 — Implement `ResidencySystem`: the request set, the tier rules of §27.3 and the LRU with a 20 s grace~~ (was M10) — contingent on `HOUSE-02451`; if it fails, the minimum residency is re-planned under R9 with new ids
- ~~HOUSE-02453 — Implement pack-level load/unload over `ContentManager`~~ (was M10) — contingent on `HOUSE-02451`; if it fails, the minimum residency is re-planned under R9 with new ids
- ~~HOUSE-02454 — Implement the 4 MB-per-frame GPU upload budget and the zone-transition promotion point~~ (was M10) — contingent on `HOUSE-02451`; if it fails, the minimum residency is re-planned under R9 with new ids
- ~~HOUSE-02455 — Implement T1 level residency driven by the stair-well portals~~ (was M10) — contingent on `HOUSE-02451`; if it fails, the minimum residency is re-planned under R9 with new ids
- ~~HOUSE-02456 — Implement T2 proximity residency (2 portal hops) for detail sets~~ (was M10) — contingent on `HOUSE-02451`; if it fails, the minimum residency is re-planned under R9 with new ids
- ~~HOUSE-02457 — Implement T3 exterior residency~~ (was M10) — contingent on `HOUSE-02451`; if it fails, the minimum residency is re-planned under R9 with new ids
- ~~HOUSE-02458 — Implement the promotion-failure policy (essential set, one diagnostic, permanent high-water reduction, one toast)~~ (was M10) — contingent on `HOUSE-02451`; if it fails, the minimum residency is re-planned under R9 with new ids
- ~~HOUSE-02459 — Implement the `F10` content overlay (tiers, packs, memory, cache hits, recent loads)~~ (was M10) — contingent on `HOUSE-02451`; if it fails, the minimum residency is re-planned under R9 with new ids
- ~~HOUSE-02460 — Test: no frame during a scripted whole-house tour exceeds the frame budget because of a promotion~~ (was M10) — contingent on `HOUSE-02451`; if it fails, the minimum residency is re-planned under R9 with new ids
- ~~HOUSE-02461 — Test: walking back and forth through a door 100 times causes no thrash (the grace period holds)~~ (was M10) — contingent on `HOUSE-02451`; if it fails, the minimum residency is re-planned under R9 with new ids
- ~~HOUSE-02462 — **Q-07 decision: is a background loading thread needed?** Measure with residency in place; if promotions still hitch, plan the thread; otherwise record the decision not to~~ (was M10) — contingent on `HOUSE-02451`; if it fails, the minimum residency is re-planned under R9 with new ids
- ~~HOUSE-02463 — Phase-42 review and commit (or record the skip)~~ (was M10) — merged into `HOUSE-02451`
- ~~HOUSE-02508 — Complete the `F10` content overlay~~ (was M10) — contingent on `HOUSE-02451`; if it fails, the minimum residency is re-planned under R9 with new ids

### Debug overlays and tools are complete enough

- ~~HOUSE-01271 — Implement the `F6` lighting overlay~~ (was M5) — the existing overlays (`F1`–`F5`, `F8`, `F9`) stay as they are; no further overlay work
- ~~HOUSE-01272 — Implement the console `light <group> <on|off>` command~~ (was M5) — `--light-on`/`--light-off` and the schedule suffice; no new console command
- ~~HOUSE-01934 — Implement the `F7` audio overlay (voices by category, emitters, gains)~~ (was M8) — the existing overlays (`F1`–`F5`, `F8`, `F9`) stay as they are; no further overlay work
- ~~HOUSE-02501 — Complete the `F1` performance overlay (GPU timing where available, per-pass draw counts)~~ (was M9) — the existing overlays (`F1`–`F5`, `F8`, `F9`) stay as they are; no further overlay work
- ~~HOUSE-02502 — Complete the `F2` world overlay~~ (was M9) — the existing overlays (`F1`–`F5`, `F8`, `F9`) stay as they are; no further overlay work
- ~~HOUSE-02503 — Complete the `F3`/`F4`/`F5` visibility overlays~~ (was M9) — the existing overlays (`F1`–`F5`, `F8`, `F9`) stay as they are; no further overlay work
- ~~HOUSE-02506 — Complete the `F8` environment overlay~~ (was M9) — the existing overlays (`F1`–`F5`, `F8`, `F9`) stay as they are; no further overlay work
- ~~HOUSE-02507 — Complete the `F9` physics overlay with the pets' capsules and paths~~ (was M9) — the existing overlays (`F1`–`F5`, `F8`, `F9`) stay as they are; no further overlay work
- ~~HOUSE-02510 — Implement the console with history, completion and the full command set of `cna-house.md` §69~~ (was M9) — the existing console is kept as it is; no history, completion or full command set
- ~~HOUSE-02511 — Implement the screenshot harness driven by a named-pose JSON, with fixed time and weather~~ (was M9) — render tests keep their scene definitions in C++ beside each test (`docs/screenshot-scenes.md`); `tools/visual/capture_review.py` serves reviews
- ~~HOUSE-02513 — Implement `validate world` as a runtime command~~ (was M9) — debug commands not needed for DONE
- ~~HOUSE-02514 — Implement `budget report` writing the current frame's numbers against the budget table~~ (was M9) — debug commands not needed for DONE
- ~~HOUSE-02515 — Verify every debug tool compiles out with `CNAHOUSE_DEBUG_TOOLS=OFF` and costs nothing in a release build~~ (was M9) — merged into `HOUSE-02789`: the packaged Release build has `CNAHOUSE_DEBUG_TOOLS=OFF`, which proves the compile-out

### Application shell trimmed to what a demo needs

- ~~HOUSE-01935 — Implement the audio settings tab and its live application~~ (was M8) — merged into `HOUSE-02519`
- ~~HOUSE-02517 — Implement the Display tab (resolution, window mode, v-sync, frame cap, FOV, UI scale)~~ (was M9) — merged into `HOUSE-02518`
- ~~HOUSE-02522 — Implement settings validation, clamping and migration~~ (was M9) — `Settings` (`HOUSE-00131`) already validates, clamps and migrates; `HOUSE-02528` proves the round trip
- ~~HOUSE-02524 — Implement the pause-on-menu setting and its default (D-29)~~ (was M9) — the menu always pauses; no setting
- ~~HOUSE-02526 — Implement the toast system and the vignette~~ (was M9) — toasts and the vignette are decoration
- ~~HOUSE-02529 — Test: every menu control is reachable and operable by keyboard alone~~ (was M9) — keyboard reachability is part of `HOUSE-02516`'s acceptance
- ~~HOUSE-02793 — **Q-11 re-evaluation: is 24 minutes the right day length after playtesting?** Record the decision~~ (was M13) — merged into `HOUSE-02781` (the day-length decision is recorded in the DONE audit)
- ~~HOUSE-02794 — **Q-10 decision: pause-on-menu default** after playtesting~~ (was M13) — moot: the menu always pauses (`HOUSE-02524` is cancelled)

### Audio reduced to atmospheric essentials

- ~~HOUSE-00281 — Grouped: source the 8 missing footstep surfaces (carpet, concrete, interior hardwood, 3 stair variants, asphalt, bluestone) — ≥ 6 walk + 6 run + 2 land each, CC0~~ (was M8) — six broad footstep categories are served by the NOX packs (`HOUSE-01920`), with carpet derived offline; the Freesound blocker no longer matters
- ~~HOUSE-00286 — Grouped: source weather sounds NOX lacks — thunder ×3 distances (≥ 9 samples), hail on roof/window/car, rain on roof/window, gutter trickle — ≥ 18 samples~~ (was M8) — rain uses the NOX `rain-calm`/`rain-strong` loops; thunder and hail sounds belong to optional work
- ~~HOUSE-00289 — Grouped: source house sounds — HVAC burner/blower/duct tick, clock tick, clock chime, doorbell, garage motor, gate motor, creaks ×8 — ≥ 18 samples~~ (was M8) — clocks, chimes, hums and creaks are cut
- ~~HOUSE-00290 — Grouped: source outdoor ambience NOX lacks — distant road, suburban day, suburban night, lawnmower, aircraft, neighbourhood dog — ≥ 10 samples~~ (was M8) — the exterior day and night beds come from NOX (`forest-birds`, `night`, `cicadas`)
- ~~HOUSE-00302 — Phase-4 review and commit; update `cna-house.md` §19/§20 with what was actually found~~ (was M8) — nothing remains to source
- ~~HOUSE-01750 — Implement the four rain audio layers (open air, roof, windows, downspouts) with their gain drivers~~ (was M7) — merged into `HOUSE-01925` and `HOUSE-02000`
- ~~HOUSE-01796 — Implement the snow ambience change (quieter, absorbed) and the exterior reverb hint change~~ (was M7) — snow ambience goes with snow cover, which is optional
- ~~HOUSE-01883 — Implement the wind ambience gain and low-pass cross-fade by speed~~ (was M7) — merged into `HOUSE-01925`
- ~~HOUSE-01885 — Implement the roof and eaves whistle in the attic cells~~ (was M7) — the attic whistle is cut
- ~~HOUSE-01912 — Implement `EmitterPool`: up to 64 logical emitters with lifetime, position, category and priority~~ (was M8) — footsteps and a few beds need no emitter pool
- ~~HOUSE-01914 — Implement the `AudioListener` update from the camera, with velocity pinned to zero~~ (was M8) — no positional sources remain
- ~~HOUSE-01915 — Implement `Apply3D` invocation per positional emitter, with `DopplerScale = 0` by default~~ (was M8) — no positional sources remain
- ~~HOUSE-01923 — Author and wire the 6 room-tone beds~~ (was M8) — merged into `HOUSE-01922`
- ~~HOUSE-01927 — Implement the house's own sound emitters from the plumbing and HVAC data (furnace, blower, duct ticks, water heater, pipe hiss)~~ (was M8) — appliance and service hums are cut; at most one special zone loop remains in `HOUSE-01922`
- ~~HOUSE-01929 — Implement the house-creak system: 14 placed points, rate weighted by the rate of change of outdoor temperature~~ (was M8) — creaks and clocks are cut
- ~~HOUSE-01930 — Implement the clocks: a tick per simulated minute in 3 rooms, and the hall clock's hourly chime~~ (was M8) — creaks and clocks are cut
- ~~HOUSE-01941 — Make the exterior ambience bed seasonal: spring birdsong and dawn chorus, summer insects, autumn wind in bare branches, winter muffled stillness — cross-faded on `SeasonPhase`~~ (was M8) — seasonal ambience is cut
- ~~HOUSE-03541 — Cell-gated positional emitters~~ (was M8) — no positional loops remain, so there is nothing to gate

### Environment kept compact

- ~~HOUSE-01652 — Tune the six sky states against reference photographs; record the final LUTs~~ (was M7) — sky tuning for the three DONE states is bounded inside `HOUSE-03520`

### Polish phases replaced by the bounded final defect pass

- ~~HOUSE-01033 — Furnishing review: walk the whole house and list what reads as fake; fix it~~ (was M11) — merged into `HOUSE-02714`
- ~~HOUSE-02686 — Grouped: the habitation pass — place the ~25 signs-of-habitation items of `cna-house.md` §59.3~~ (was M11) — signs of habitation are part of each hero task's C5 criteria
- ~~HOUSE-02687 — Render the fictional family photographs in Blender using the game's own characters and rooms (9 gallery frames + 6 stair frames)~~ (was M11) — the hall gallery already has its frames and images; bespoke renders are not needed
- ~~HOUSE-02689 — Author the corkboard, whiteboard, timetable, height chart and child's drawing as small texture props~~ (was M11) — small texture props are S4 detail
- ~~HOUSE-02693 — Polish pass: interior lighting balance room by room, re-baking where needed~~ (was M11) — a "polish everything" pass; what it would find is classified in `HOUSE-02714` and fixed in `HOUSE-03631` by severity
- ~~HOUSE-02694 — Polish pass: exterior lighting and the night-time neighbourhood~~ (was M11) — a "polish everything" pass; what it would find is classified in `HOUSE-02714` and fixed in `HOUSE-03631` by severity
- ~~HOUSE-02695 — Polish pass: material tuning against reference photographs, room by room~~ (was M11) — a "polish everything" pass; what it would find is classified in `HOUSE-02714` and fixed in `HOUSE-03631` by severity
- ~~HOUSE-02696 — Polish pass: exposure and adaptation tuning across the 8 indoor↔outdoor transitions~~ (was M11) — transition exposure is judged in `HOUSE-02714` (an S2 if a transition blinds or blacks out)
- ~~HOUSE-02697 — Polish pass: the six sky states and the sunrise/sunset colour ramp~~ (was M11) — a "polish everything" pass; what it would find is classified in `HOUSE-02714` and fixed in `HOUSE-03631` by severity
- ~~HOUSE-02698 — Polish pass: particle look for rain, snow and hail at every intensity~~ (was M11) — a "polish everything" pass; what it would find is classified in `HOUSE-02714` and fixed in `HOUSE-03631` by severity
- ~~HOUSE-02699 — Polish pass: audio mix across every category, every room and every weather state~~ (was M11) — a "polish everything" pass; what it would find is classified in `HOUSE-02714` and fixed in `HOUSE-03631` by severity
- ~~HOUSE-02701 — Polish pass: UI typography, spacing, colour and the prompt's readability against bright and dark backgrounds~~ (was M11) — a "polish everything" pass; what it would find is classified in `HOUSE-02714` and fixed in `HOUSE-03631` by severity

### Web validation breadth

- ~~HOUSE-02846 — Verify the user-gesture audio gate is on the desktop path too~~ (was M14) — merged into `HOUSE-02896`
- ~~HOUSE-02849 — Verify no MRT, no stencil, no geometry/tessellation stage is used anywhere~~ (was M14) — merged into `HOUSE-02848`
- ~~HOUSE-02852 — Implement the Web quality tier fully and verify it on Linux via `--quality web`~~ (was M14) — merged into `HOUSE-02405`
- ~~HOUSE-02854 — Implement the loading-screen progress model driven by pack loads~~ (was M14) — merged into `HOUSE-02899`
- ~~HOUSE-02856 — Record the phase-47 readiness matrix in `docs/portability.md`~~ (was M14) — merged into `HOUSE-02904`
- ~~HOUSE-02857 — Phase-47 review and commit~~ (was M14) — superseded by `HOUSE-02905`
- ~~HOUSE-02903 — Multi-browser check: Chrome, Firefox; record what differs~~ (was M14) — merged into `HOUSE-02900` (Chrome and Firefox in the same checklist pass)

### Android validation breadth

- ~~HOUSE-02955 — Verify the `Platform` capability struct drives HUD and defaults, using a forced-touch desktop mode~~ (was M15) — merged into `HOUSE-02997`
- ~~HOUSE-02956 — Verify the UI at 18:9 and 20:9 with safe-area insets on desktop~~ (was M15) — merged into `HOUSE-02527`
- ~~HOUSE-02957 — Implement the Android quality tier and verify it on desktop via `--quality android`~~ (was M15) — merged into `HOUSE-02405`
- ~~HOUSE-02960 — Verify the desktop focus-loss path exercises the same lifecycle code~~ (was M15) — merged into `HOUSE-02959`
- ~~HOUSE-02961 — Record the phase-49 readiness matrix in `docs/portability.md`~~ (was M15) — merged into `HOUSE-03041`
- ~~HOUSE-02962 — Phase-49 review~~ (was M15) — superseded by `HOUSE-03042`
- ~~HOUSE-03000 — Render tests: the touch HUD at 3 aspect ratios via `--force-touch`~~ (was M15) — merged into `HOUSE-02997` and `HOUSE-02530`
- ~~HOUSE-03001 — Playtest the touch controls on desktop with a touchscreen or a simulator; tune~~ (was M15) — merged into `HOUSE-02992` and `HOUSE-03039`
- ~~HOUSE-03002 — Phase-50 review~~ (was M15) — superseded by `HOUSE-03042`
- ~~HOUSE-03040 — Test on at least 3 devices spanning GPU vendors; record what differs~~ (was M15) — merged into `HOUSE-03038` (one representative device; more when available)

### Release duplication

- ~~HOUSE-02785 — Verify the failure-handling table of `cna-house.md` §73 row by row by inducing each failure~~ (was M13) — covered by `HOUSE-02791` (no audio device), the settings tests (`HOUSE-00131`, `HOUSE-02528`) and the Web and Android checklists
- ~~HOUSE-02787 — Verify `docs/xna-deviations.md` is complete, that it still grants no permission to call a CNA symbol, and that the lint enforces the §70.1 rule with no allowlist~~ (was M13) — merged into `HOUSE-03074`
- ~~HOUSE-02792 — Verify the game at 4 quality tiers, 5 resolutions and 3 aspect ratios~~ (was M13) — merged into `HOUSE-02790`
- ~~HOUSE-02795 — Update `cna-house.md` to match what was actually built; every divergence either fixed or documented~~ (was M13) — merged into `HOUSE-03075`
- ~~HOUSE-02796 — Write the release notes and the known-limitations list (including the mirror reflection, D-19)~~ (was M13) — merged into `HOUSE-03077`

---

## Cancelled by the first scope reduction (`HOUSE-03201`)

385 legacy tasks, cancelled on 2026-09-21 by `HOUSE-03201` under ADR-0014. Their full
original text is in the legacy ledger, struck through, with a `cancelled:` line carrying the same
reason. Ids are not reused.

| Reason | Ids (`HOUSE-…`) | Count |
|---|---|---|
| Superseded: mouse look shipped (`HOUSE-00622`–`HOUSE-00625`) and was tuned by walking the house (`HOUSE-00632`) with no drift defect reported; Web pointer-lock is verified by `HOUSE-03721` | `00100` | 1 |
| Door sounds serve removed door interaction; doors have static poses (ADR-0014) | `00282` | 1 |
| Switch, cabinet and drawer sounds serve removed interactions (ADR-0014) | `00283` | 1 |
| Water sounds serve removed plumbing (ADR-0014) | `00284` | 1 |
| Appliance-programme sounds serve removed appliance simulation; steady hums come from the imported NOX electromagnetic pack (`HOUSE-01927`) | `00285` | 1 |
| Animals removed from scope (ADR-0014) | `00287`, `00288`, `02041`–`02061`, `02091`–`02106`, `02512`, `02587` | 41 |
| Interactable data for containers, pickups, seats and placement surfaces is not needed: objects are static dressing (ADR-0014) | `00404`–`00411`, `00417`–`00420` | 12 |
| Legacy phase review superseded by the milestone gate reviews (`HOUSE-03240`, `03280`, `03380`, `03420`, `03480`) and the DONE audit (`HOUSE-02781`) | `00783`, `00855`, `00918`, `01034`, `01541`, `01578`, `01655`, `01702`, `01756`, `01801`, `01844`, `01890`, `01940`, `02409`, `02531`, `02601` | 16 |
| Gameplay interaction framework removed (ADR-0014); objects are static dressing | `01121`–`01146`, `02578`, `02994`, `02999` | 29 |
| Door, window and gate behaviours replaced by static poses (`HOUSE-03221`–`HOUSE-03224`); windows stay closed (ADR-0014) | `01181`–`01196`, `01198`–`01202` | 21 |
| Player light switching removed; lights follow the automatic schedule (`HOUSE-03401`) and the console (`HOUSE-01272`) | `01252`–`01254`, `01270` | 4 |
| Blob shadows served dynamic actors (avatar, pets), which are removed; static props are grounded by the baked lighting (`HOUSE-01030`, `HOUSE-03402`) | `01267` | 1 |
| The first-playable milestone is superseded by gates G1–G5; its measurable exit criteria live on in `HOUSE-03226`, `HOUSE-02403` and `HOUSE-02404` | `01277`, `01278` | 2 |
| Containers and their contents removed (ADR-0014); cupboards stay closed | `01311`–`01323` | 13 |
| Kitchen and appliance simulation removed (ADR-0014); appliances are static props | `01361`–`01383` | 23 |
| Plumbing and water simulation removed (ADR-0014) | `01411`–`01429` | 19 |
| Toilet simulation removed (ADR-0014); toilets are static fixtures | `01461`–`01474` | 14 |
| Television and video playback removed (ADR-0014); screens are static | `01491`–`01504`, `02847`, `02894`, `03035` | 17 |
| Household-state persistence removed (ADR-0014); settings (`HOUSE-00131`) and the optional session file (`HOUSE-03571`) remain | `01538`, `02301`–`02333`, `02509`, `02575`, `02586` | 37 |
| The sun/moon-clock overlay is a novelty UI; the environment readout (`HOUSE-01546`) already shows the time; glare and lens flare are kept | `01572`, `01573` | 2 |
| No significant water surface exists (the pool is rejected, D-22); ripples would serve only the birdbath | `01747` | 1 |
| Depends on openable windows, which are removed; windows stay closed (ADR-0014) | `01751`, `01752`, `01880`, `01884`, `02683` | 5 |
| Wind-driven door motion needs moving doors; doors have static poses (ADR-0014) | `01881` | 1 |
| Gate rattle is gate-interaction audio of negligible showcase value; gates have static poses | `01882` | 1 |
| The full voice manager is not needed for a few ambience beds, weather layers, footsteps and static loops; `HOUSE-01912` keeps a hard emitter cap | `01913` | 1 |
| Doppler has no use without moving sources; `DopplerScale = 0` stays the fixed default (BL-11) | `01916` | 1 |
| Visible avatar, customisation and character animation removed (ADR-0014); the player is a first-person controller only | `01921`, `02131`–`02146`, `02171`–`02187`, `02211`–`02238`, `02271`–`02284`, `02579`, `02700`, `02705` | 79 |
| Audio for a removed system (ADR-0014) | `01926`, `01932`, `01933` | 3 |
| Hidden thermostat simulation; the furnace is a steady ambient loop (`HOUSE-01927`) | `01928` | 1 |
| Merged into `HOUSE-01927` (static ambient loops) | `01931` | 1 |
| Tests the cancelled voice manager (`HOUSE-01913`) | `01937` | 1 |
| Portal-path audio solver removed (ADR-0014 supersedes ADR-0010 in part); cell-gated emitters (`HOUSE-03541`) and sky-exposure routing (`HOUSE-02000`) remain | `01991`–`01999`, `02001`–`02009`, `02505` | 19 |
| Reset House removed together with household state (ADR-0014) | `02371`–`02380` | 10 |
| Duplicate: `HOUSE-01271` delivers the complete `F6` lighting overlay | `02504` | 1 |
| Tests removed systems (water, toilets, fridge, TV, containers, pickups) | `02585` | 1 |
| Superseded by the completed seeded random walk (`HOUSE-00618`) and the grand-tour test (`HOUSE-03226`); there is no interaction to drive | `02589` | 1 |
| Kettle and shower steam belong to removed appliance and plumbing simulation | `02692` | 1 |
| `WaterFlow.fx` served removed plumbing | `02710` | 1 |

---

## Legacy phases → milestones

`HOUSE-03201`'s mapping of every legacy phase's open work. Many of these carried tasks were then
cancelled or made optional by `HOUSE-03205`; see
[Cancelled by the second scope reduction](#cancelled-by-the-second-scope-reduction-house-03205) and
[Optional after DONE](#optional-after-done).

Where every legacy phase's open work went. Completed tasks stay in the legacy ledger.

| Legacy phase | Done | Carried → milestone | Cancelled | Deferred |
|---|---|---|---|---|
| Phase 0 — Repository, conventions and decisions | 44 | — | — | — |
| Phase 1 — CNA capability verification | 59 | — | 1 | — |
| Phase 2 — Build skeleton and CI | 48 | — | — | — |
| Phase 3 — Content pipeline | 47 | — | — | — |
| Phase 4 — Asset provenance and licensing | 31 | 5 → M8 | 6 | — |
| Phase 5 — World and floor-plan data | 70 | — | 12 | — |
| Phase 6 — Blockout house geometry | 45 | 1 → M12 | — | — |
| Phase 7 — Collision and player controller | 39 | — | — | — |
| Phase 8 — First-person camera | 14 | — | — | — |
| Phase 9 — Room/portal visibility | 42 | — | — | — |
| Phase 10 — Exterior and property | 21 | 1 → M4, 2 → M7 | 1 | — |
| Phase 11 — Neighbourhood background | 11 | 1 → M4, 3 → M5, 2 → M10 | 1 | — |
| Phase 12 — Materials and textures | 51 | 1 → M5, 1 → M6, 2 → M7, 4 → M10 | 1 | — |
| Phase 13 — Static furniture and dressing | 40 | 14 → M3, 36 → M4, 2 → M5, 10 → M6, 2 → M10, 1 → M11 | 1 | — |
| Phase 14 — Interactable framework | 0 | — | 26 | — |
| Phase 15 — Doors and windows | 0 | 1 → M2 | 21 | — |
| Phase 16 — Lights and switches | 33 | 7 → M5 | 7 | — |
| Phase 17 — Containers | 0 | — | 13 | — |
| Phase 18 — Kitchen and refrigerator | 0 | — | 23 | — |
| Phase 19 — Plumbing and water | 0 | — | 19 | — |
| Phase 20 — Toilets | 0 | — | 14 | — |
| Phase 21 — Television and video | 0 | — | 14 | — |
| Phase 22 — Time | 15 | — | 2 | — |
| Phase 23 — Sun, glare and the sun-clock | 6 | 1 → M5, 8 → M7 | 3 | — |
| Phase 24 — Moon and stars | 20 | — | — | — |
| Phase 25 — Sky and clouds | 10 | 4 → M7 | 1 | — |
| Phase 26 — Weather core | 25 | 1 → M7 | 1 | — |
| Phase 27 — Rain | 0 | 12 → M7 | 4 | — |
| Phase 28 — Snow | 0 | 11 → M7 | 1 | — |
| Phase 29 — Storm, lightning and thunder | 0 | 13 → M7 | 1 | — |
| Phase 30 — Hail and wind | 0 | 12 → M7 | 5 | 3 |
| Phase 31 — Audio foundation | 0 | 21 → M8 | 10 | — |
| Phase 32 — Room-aware 3-D audio | 0 | 1 → M8 | 18 | — |
| Phase 33 — Dog | 0 | — | 21 | — |
| Phase 34 — Cat | 0 | — | 16 | — |
| Phase 35 — Third-person avatar | 0 | — | 16 | — |
| Phase 36 — Character customisation | 0 | — | 17 | — |
| Phase 37 — Character animation | 0 | — | 28 | — |
| Phase 38 — Stair animation and foot IK | 0 | — | 14 | — |
| Phase 39 — Persistence | 0 | — | 33 | — |
| Phase 40 — Reset House | 0 | — | 10 | — |
| Phase 41 — Culling and LOD optimisation | 1 | 16 → M10 | 1 | 1 |
| Phase 42 — Streaming and loading | 0 | 13 → M10 | — | — |
| Phase 43 — Debug tools and settings | 1 | 25 → M9, 1 → M10 | 5 | — |
| Phase 44 — Automated tests | 0 | 1 → M11, 22 → M12 | 8 | — |
| Phase 45 — Visual polish and habitation | 0 | 3 → M6, 24 → M11 | 5 | 2 |
| Phase 46 — Linux desktop stabilisation | 0 | 17 → M13 | — | — |
| Phase 47 — Web preparation | 1 | 15 → M14 | 1 | — |
| Phase 48 — Web implementation | 0 | 14 → M14 | 1 | — |
| Phase 49 — Android preparation | 0 | 12 → M15 | — | — |
| Phase 50 — Android touch UI | 0 | 10 → M15 | 2 | — |
| Phase 51 — Android implementation | 0 | 11 → M15 | 1 | — |
| Phase 52 — Final optimisation and release | 0 | 9 → M16 | — | — |

---

## Remaining-work estimate of the second reduction (`HOUSE-03205`)

Superseded by `plan.md`'s *Remaining-work estimate* (`HOUSE-03206`). Kept for the comparison.


Agent-hours. Since `HOUSE-03205` every open task carries its own `est:`, and the **expected** column
is their sum. The estimates assume the kit (M3) exists before M4 starts and that each task follows
its acceptance, not a larger idea of it; they are consistent with the 2026-09-14 → 2026-09-20 sprint
(≈ 1.7 h per bespoke visual task) once kit reuse replaces bespoke authoring. **Optimistic** is
0.82 × expected (0.75 × for M10 and M15, where a green measurement or the upstream gate can make work
disappear). **Conservative** is 1.10 × expected for ordinary estimation error **plus** the named risk
reserves below, each charged to its milestone. Optional work is not counted.

| Milestone | Open tasks | Optimistic | **Expected** (Σ `est:`) | Conservative | Reserve in conservative |
|---|---|---|---|---|---|
| M0 Scope reset and breadth instruments | 3 | 5.5 | **7** | 7.5 | — |
| M1 Whole-property traversal | 8 | 15.5 | **19** | 26 | +5 (R-E) |
| M2 Architectural completion | 8 | 15 | **18** | 20 | — |
| M3 The reusable furnishing kit | 16 | 27 | **33** | 41.5 | +5 (R-D) |
| M4 Dressing everywhere | 40 | 40 | **49** | 57.5 | +4 (R-G) |
| M5 Baseline lighting everywhere | 12 | 20 | **24.5** | 31 | +4 (R-F) |
| M6 Main rooms and hero areas | 20 | 26.5 | **32.5** | 36 | — |
| M7 A compact environment | 13 | 17 | **21** | 23 | — |
| M8 Atmospheric audio essentials | 11 | 12 | **14.5** | 16 | — |
| M9 Application shell | 10 | 10 | **12.5** | 14 | — |
| M10 Performance | 6 | 10 | **13** | 20.5 | +6 (R-C) |
| M11 Final defect pass | 3 | 9 | **11** | 12 | — |
| M12 Representative tests | 4 | 4 | **5** | 5.5 | — |
| M13 Linux desktop release | 10 | 11 | **13** | 14.5 | — |
| M14 Web | 17 | 20.5 | **25** | 35 | +8 (R-B) |
| M15 Android | 23 | 22 | **29** | 42 | +10 (R-A) |
| M16 Final release | 8 | 7.5 | **9** | 10 | — |
| **Desktop DONE candidate (M0–M13)** | 164 | **223** | **273** | **324.5** | |
| **Full DONE, all three platforms (M0–M16)** | **212** | **272.5** | **336** | **411.5** | +42 |

| Reserve | Milestone | Hours | Risk |
|---|---|---|---|
| R-A | M15 | 10 | the device path after BL-13 is fixed upstream: the first CNA graphics ever run on Android |
| R-B | M14 | 8 | Web memory or package size forces a minimal residency scheme or deeper content cuts; Emscripten workarounds |
| R-C | M10 | 6 | performance needs two or more of `HOUSE-02404`'s candidate techniques |
| R-D | M3 | 5 | kit acquisition falls short on licence or quality, so more pieces are generated |
| R-E | M1 | 5 | the grand tour and the sweep expose geometry and collision fixes beyond M1's budget |
| R-F | M5 | 4 | the furnished re-bake or the props-versus-bake solution needs a second iteration |
| R-G | M4 | 4 | a few rooms (library shelving, cinema, workshop) need more than their recipe estimate |

**Critical path.** M0 (`03204` → `03202` → `03203`) → M1 (static poses `03221`–`03224`, the grand
tour `03226`, the sweep, **G1**) → M2 (seven zone tasks, **G2**) → M4 (40 furnishing tasks on the M3
kit, the dressing checkpoint) → M5 (props versus bake, the furnished re-bake, zone lighting, **G3**)
→ M6a (**G4**) → M6b (**G5**) → M11 → M13 → the Web and Android verification blocks → M16. M3 must be
ready before M4 starts; it may begin at any time under R1 exception (b), so it is on the critical path
only if it is left late. M7–M10, M12, the Web bring-up and the Android readiness block interleave as
Track B under R5. With one agent working at a time, the calendar is the expected total; the M0–M6 breadth
chain alone is about 54 % of it.

**The five largest remaining time sinks** (expected):

1. **M4, dressing every accessible room** (49 h). Forty tasks. It is the breadth of the house and
   is protected; the kit and the tiers are what keep it at about 1.2 h per task.
2. **M3, the kit** (33 h). Generators plus nine capped acquisition groups.
3. **M6, main rooms and hero areas** (32.5 h). Ten C4 tasks and ten C5 tasks, gates included.
4. **M15, Android** (29 h, with the largest reserve). The device path depends on BL-13.
5. **M14, Web** (25 h). The full Emscripten build (`HOUSE-02892`, 5 h) is the largest single
   platform task. M5, baseline lighting (24.5 h), is a close sixth.

**Where this pass saved the most.** Against `HOUSE-03201`'s midpoint of 810.5 h, the expected
total fell by about 475 h. Roughly 405 h of that is removed or merged work (212 of 424 tasks
remain open), and roughly 70 h is the re-estimate of the kept tasks with the kit and the tiers in place
(`HOUSE-03201` priced every task at its milestone's average, about 1.9 h). By milestone:

| Milestone | `HOUSE-03201` open tasks | `HOUSE-03201` estimate (mid) | Now open | Now expected | Saved (mid → expected) |
|---|---|---|---|---|---|
| M0 Scope reset and breadth instruments | 3 | 8.5 | 3 | 7 | 1.5 |
| M1 Whole-property traversal | 8 | 23 | 8 | 19 | 4 |
| M2 Architectural completion | 9 | 32.5 | 8 | 18 | 14.5 |
| M3 The reusable furnishing kit | 17 | 57.5 | 16 | 33 | 24.5 |
| M4 Dressing everywhere | 42 | 110 | 40 | 49 | 61 |
| M5 Baseline lighting everywhere | 22 | 57.5 | 12 | 24.5 | 33 |
| M6 Main rooms and hero areas | 26 | 80 | 20 | 32.5 | 47.5 |
| M7 A compact environment | 66 | 80 | 13 | 21 | 59 |
| M8 Atmospheric audio essentials | 28 | 35 | 11 | 14.5 | 20.5 |
| M9 Application shell | 26 | 33.5 | 10 | 12.5 | 21 |
| M10 Performance | 38 | 42.5 | 6 | 13 | 29.5 |
| M11 Final defect pass | 27 | 55 | 3 | 11 | 44 |
| M12 Representative tests | 23 | 33 | 4 | 5 | 28 |
| M13 Linux desktop release | 17 | 26 | 10 | 13 | 13 |
| M14 Web | 30 | 57.5 | 17 | 25 | 33 |
| M15 Android | 33 | 60 | 23 | 29 | 31 |
| M16 Final release | 9 | 19 | 8 | 9 | 10 |
| **Total** | **424** | **810.5** | **212** | **336** | **475** |

By theme, the hours saved rank as follows. **(1)** The reusable kit instead of bespoke,
uniqueness-checked furnishing (M3 and M4, about 86 h). M4 keeps 40 of its 42 tasks, so this is
almost entirely the re-estimate: every room is still furnished, each one more cheaply. **(2)** Quality
tiers instead of C5 everywhere (M6 and the lighting half of M5, about 80 h). **(3)** Limited
validation breadth on Web and Android (M14 and M15, about 64 h), with both platforms kept.
**(4)** The compact environment (M7, 59 h): storm, hail, snow cover, glare and lens flare, sway,
seasons, splashes and puddles. This is the largest cut of actual work. **(5)** The bounded final
defect pass instead of a C6 rotation and eleven polish passes (M11, 44 h). After those come
measurement-only optimisation (M10), representative tests (M12), the trimmed shell (M9) and the
audio essentials (M8).

For comparison, `HOUSE-03201` estimated ≈ 635–986 h for the same three platforms, and the 764 legacy
open tasks would have been roughly 1 500–1 900 h.

---

## Planning corrections of 2026-09-21

### PC-2026-09-21b — Second scope reduction: tiers, kit, compact scope (`HOUSE-03205`, ADR-0015)

* **Evidence:** the first reduction's plan still estimated ≈ 635–986 agent-hours. It required every
  one of eleven zones to reach C5 (secondary dressing, day/night/overcast views) with a C6 polish
  rotation after it; it kept a full weather stack (snow accumulation and melt, hail, a lightning and
  thunder model, occlusion-query glare and lens flare, seasonal vegetation) of 66 tasks; 28 audio
  tasks including positional hums, clocks, creaks, per-cell room tone and seasonal beds, four of them
  blocked on a Freesound account; render-test matrices of 64 + 48 + 80 + 76 scenes; a 2-hour soak
  as a recurring requirement; a 15 % headroom target on every platform; three-device Android testing;
  and a uniqueness quota of ≤ 14 uses per model that forced bespoke content.
* **Correction:** "C5 everywhere" becomes quality tiers (C3 for every accessible room, C4 for 19 main
  cells, C5 for seven hero areas), with every cell classified; the ladder's C3 now includes baseline
  lighting and the gates are renumbered to match (G3 baseline complete, G4 main rooms, G5 hero
  areas); C6 is retired. Furnishing uses a reusable kit with capped groups and free reuse. The
  environment is clear/overcast/rain, day/night and simple snowfall. Audio is six footstep
  categories and a few loops from NOX. The shell is one settings screen. Tests and performance work
  are representative and measurement-driven. Final polish is one bounded pass. Web and Android keep
  their full DONE standing with a limited validation breadth. A **Non-goals** section and an
  **Optional after DONE** section keep the cuts from creeping back (rules R9, R13). Every open task
  now carries `est:`.
* **Result:** 212 open tasks (from 424); 146 cancelled with reasons; 70
  moved to Optional after DONE. Expected 336 h (optimistic 272, conservative
  411), from ≈ 635–986 h. Nothing was cut from breadth: every floor, the basement, the
  attic, the garage, the garden and exterior, the street, the Web build and the Android build stay.
* **Preserved:** completed work stays. Finished systems that are no longer extended (snow depth, the
  storm and hail archetypes, the debug overlays, the cube-map tooling) stay in the repository and are
  listed under *Scope-reduction cleanup candidates*.
* **`cna-house.md`:** a second scope-amendment block at the top and banners on the sections whose
  designs are now optional (§32.4, §38–§41, §59.1–§59.3, §62, §64); the body is not rewritten.
* **ADRs:** ADR-0015 records the decision and supersedes ADR-0014 in part (its "retained in full"
  weather list and its "showcase baseline everywhere" ladder). ADR-0010's status notes that the
  cell-gated positional loops are cut too.

### PC-2026-09-21 — Scope reduction to an architectural showcase (`HOUSE-03201`, ADR-0014)

* **Evidence:** 132 of the 133 placed props stand on the ground floor or in the grounds. The
  133rd is the lantern on `L1`'s front balcony. The basement, the `L1` rooms, `L2` and the attic
  hold 0. 102 visual-review rounds, all on the ground-floor
  route. 0 review views in `B1`, the `L0` service rooms, the garage interior, `L1` rooms, `L2` and
  `L3`. 286 of 764 open tasks in systems that do not make the house more complete to walk
  through. Every leaf drawn closed while the player passes through it.
* **Correction:** the project goal, the Definition of DONE, the scheduling rules, zones, levels,
  gates and the milestone structure above. Legacy per-phase structure retired, legacy ledger
  archived verbatim, cancelled tasks recorded with reasons, carried tasks amended in place.
* **`cna-house.md`:** a scope-amendment block at the top; §1–§3, §74, §78, §79 and the section
  banners on removed systems point here. The body is not rewritten (§0 of `CLAUDE.md`: smallest
  correction).
* **Superseded markers:** the *Active priority override — Visual Convergence Sprint* and
  `VISUAL-GATE-1` of the legacy plan are retired. Their concerns (production materials, real
  furniture, lighting depth) are C3–C5 of `Z-L0M` and `Z-EXF`, scheduled like every other zone.

---

