# CNA House — Showcase Completion Plan

`STATUS: APPROVED — IMPLEMENTATION IN PROGRESS · SCOPE REDUCED 2026-09-21 (HOUSE-03201, ADR-0014) · SECOND REDUCTION 2026-09-21 (HOUSE-03205, ADR-0015)`

This file is the **execution ledger for the remaining work**. A checkbox moves to `[x]` only when
the task's acceptance criteria are genuinely satisfied and its `verify:` step has been run. Task ids
are permanent and are never renumbered.

| | |
|---|---|
| Goal | A polished, atmospheric, multiplatform **architectural and graphics walkthrough showcase for CNA**. See [Direction](#direction) |
| Scope decisions | [ADR-0014](docs/decisions/ADR-0014-showcase-scope.md) (first reduction: a showcase, not a life simulator) and [ADR-0015](docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md) (second reduction: quality tiers, a reusable kit, a compact feature set), both 2026-09-21 |
| Legacy ledger | [`docs/history/plan-legacy-2026-09-21.md`](docs/history/plan-legacy-2026-09-21.md) is the original 53-phase plan, frozen at `7499770`. It is the authoritative record of every task completed before 2026-09-21 and keeps the verbatim text of every task cancelled by the first reduction |
| Legacy tasks | 674 completed · 373 carried by the first reduction · 385 cancelled by it · 6 deferred |
| First reduction (`HOUSE-03201`) | 424 open tasks in 17 milestones, estimated at ≈ 635–986 agent-hours. Its full text is `plan.md` at commit `5927073` |
| This plan (`HOUSE-03205`) | **212 open tasks** (208 kept, 4 new) in 17 milestones. 146 cancelled with reasons, 70 moved to [Optional after DONE](#optional-after-done) (76 optional tasks in all) |
| Current gate | **G1, not passed.** Start with M0 (review tooling), then M1 (traversal) |
| Estimate to DONE | **≈ 272 / 336 / 411 agent-hours** (optimistic / expected / conservative), desktop, Web and Android included. See [Remaining-work estimate](#remaining-work-estimate) |

**Read in this order:** [Direction](#direction) → [Non-goals](#non-goals--not-required-for-done) →
[Definition of DONE](#definition-of-done) → [Quality tiers](#quality-tiers-and-room-classification) →
[Scheduling rules](#scheduling-rules--time-discipline) → [Zone scoreboard](#zone-scoreboard) → the
milestone you are working in.

---

## Direction

House Simulator (`cna-house`) is **a polished, atmospheric, multiplatform architectural and
graphics walkthrough showcase for CNA.** It succeeds when someone can start it, walk from the street
through a convincing large property, explore the house from the basement to the attic, see good CNA
rendering and atmosphere, and run the same showcase on Linux desktop, in a browser and on Android. It
demonstrates that CNA, through the XNA 4.0 API alone (ADR-0001), carries a substantial realistic 3-D
environment.

It is **not** a life, household, avatar, animal, weather or audio simulator, not a
content-production competition and not a certification project. Objects are **static dressing**.
Doors and gates have **static poses** so that every area stays reachable. Lights follow an
**automatic schedule**.

**Cut depth, preserve breadth. Finish the house.** Every floor, the basement, the attic, the garage,
the garden, the exterior, the Web build and the Android build stay. What is cut is excessive polish,
unique content, simulation detail, test matrices and supporting systems. The shape of the project is:
**complete the whole house to a common baseline first, then beautify a few hero areas.**

What matters, in priority order:

1. **Complete architecture and reliable traversal.** Every zone is present and walkable, proved by
   the grand tour: street → property → house → basement → ground floor → both upper floors → attic
   → exterior areas → back.
2. **Every room furnished enough to read as its purpose**, from a reusable modular kit. A believable
   reused asset is better than an unfinished room.
3. **Visual quality in tiers:** a baseline everywhere (C3), presentation-ready main rooms (C4) and
   seven showcase hero areas (C5).
4. **A compact, visible environment:** time of day, day and night, sun, moon and stars, clear,
   overcast and rain, and simple falling snow.
5. **Light atmospheric audio:** footsteps, interior tone, exterior day and night, rain and wind.
6. **A small application shell:** start, pause, settings, credits, quit, and Web and touch controls.
7. **Performance that is good enough and stable** on documented representative targets.
8. **Representative tests** that protect the above.

When the choice is between a hidden system and finishing an unfinished area, **finish the area**.
When it is between another pass on a good ground-floor room and bringing any other area up to its
baseline, **choose the other area**.

---

## Non-goals — not required for DONE

**These are not postponed prerequisites for DONE.** Nothing below may appear on the critical path,
block a gate, or be written into a task's acceptance. An idea from this list that is still
attractive goes to [Optional after DONE](#optional-after-done), which never counts towards the
estimate (rule R13).

| Not required for DONE | What the plan does instead |
|---|---|
| Animals (dog, cat), animal AI, animal animation | nothing (ADR-0014) |
| A player avatar, avatar customisation, character animation, foot IK | first person only |
| A gameplay interaction framework; item pickup, carry, inventory | static dressing |
| Openable containers, cupboards and drawers | closed static joinery |
| Detailed door, window and gate interaction | static poses (M1) |
| Appliance, kitchen, plumbing, toilet and television simulation | static fixtures |
| Detailed household persistence; Reset House; session resume | settings only; session resume is optional |
| C5 quality in every room; C6 as a project-wide requirement | quality tiers: C3 everywhere, C4 main rooms, C5 in seven hero areas |
| Unique bespoke furniture for every room; uniqueness or density quotas; huge clutter density | a reusable kit, reused freely (rule R8) |
| Detailed snow accumulation and melt; hail; storm, lightning and thunder simulation; complex puddles; lens flare and occlusion-query glare; many tuned sky states; season permutations | clear, overcast and rain, day and night, simple snowfall (M7) |
| Exhaustive audio simulation: room-aware routing, unique room tone per room, positional hums, clocks, creaks, seasonal ambience, mixing matrices | a few loops and six footstep categories (M8) |
| A developer console or overlay suite beyond what exists; runtime tweak UI; toasts; vignettes; key remapping; several settings pages | the existing debug tools; one settings screen (M9) |
| Exhaustive browser, device or GPU matrices | Chrome and Firefox; one representative Android device |
| Exhaustive render-test matrices (times × rooms × weather × light groups); 30-day runs; exhaustive unit-suite completion | representative sets (M5, M7, M9) and the existing suites |
| Exhaustive performance matrices; a fixed headroom margin everywhere; optimising costs that already meet the target | eight representative scenarios (M10) |
| Recurring multi-hour soak tests | a 20-minute stability run; one 2-hour run before DONE |
| Perfect visual polish in every corner | a bounded final defect pass (M11) |
| Tier E compiled effects | optional; Tier S is complete on its own (ADR-0003) |

---

## Definition of DONE

House Simulator is **done** when every line below is true. Nothing else is required.

| # | Area | Criterion | Proved by |
|---|---|---|---|
| **D1** | Architecture | The intended property exists: the basement, the ground floor, the garage, both upper floors, the attic, the stairs, the front and rear exterior with the garden, and the street. Every zone is at C2 or better with no S1/S2 architecture defect open | G2 `HOUSE-03280`, G3 `HOUSE-03420` |
| **D2** | Traversal | Every intended-accessible area can be reliably visited. The grand tour walks the real controller from the street through the property and the house, basement to attic, through the exterior areas and back, with no inaccessible room, blocked door, broken stair, trap or clipping through critical geometry. The random walk and the inside-geometry guarantees pass. No S1/S2 traversal or collision defect is open | `HOUSE-03226`, `HOUSE-03227`, `HOUSE-03240`, `HOUSE-03631` |
| **D3** | Furnishing | Every accessible room has enough static content to communicate its purpose (C3). Utility rooms use simple, efficient dressing. No major room looks like an unfinished placeholder | G3 `HOUSE-03420` |
| **D4** | Visual quality | Every accessible room is at C3, every main room and main area at C4, and the seven hero areas at C5. No higher level is required anywhere else | G3, G4 `HOUSE-03452`, G5 `HOUSE-03480` |
| **D5** | Environment | Time of day runs automatically and can be set; day and night show the sun, moon and stars; the sky shows clear, overcast and rain, with rain kept out of covered areas, wet surfaces and fog; simple snow falls. Detailed weather simulation is not required | `HOUSE-03520`, `HOUSE-01653`, `HOUSE-02521` |
| **D6** | Lighting | Interior lights follow the automatic schedule; furniture is consistent with the baked lighting; exterior night lighting is readable | `HOUSE-03401`, `HOUSE-03402`, G3 |
| **D7** | Audio | Footsteps on six broad surface categories; an interior tone; exterior day and night ambience; rain and wind where relevant; volume settings | `HOUSE-01939`, `HOUSE-02519` |
| **D8** | Application | *Start*, pause, one settings screen (graphics, audio, controls, environment), credits and quit; a controls hint; Web controls; Android touch controls | `HOUSE-02523`, `HOUSE-02528`, `HOUSE-03721`, `HOUSE-03039` |
| **D9** | Performance | The representative indoor and outdoor scenarios meet the documented target on the reference desktop; the Web and Android presets meet their targets in their representative scenes; no major memory or performance defect remains | `HOUSE-02404`, `HOUSE-00915`, `HOUSE-02898`, `HOUSE-03037`, `HOUSE-03072` |
| **D10** | Platforms | **Linux:** a packaged build runs on a clean profile. **Web:** it builds and loads, its controls work, and a representative traversal completes in Chrome and Firefox without major rendering corruption and with acceptable performance; a headless smoke test runs in CI. **Android:** it builds, installs and runs on one representative device (or the best available emulator, recorded as such); touch controls suffice for the walk; the representative traversal completes without major corruption and with acceptable performance. If CNA's Android graphics path is still blocked upstream (BL-13) when everything else is done, the blocker is re-verified with evidence, every desktop-side Android readiness task passes, and the device tasks stay open until CNA lands the fix | `HOUSE-02790`, `HOUSE-02900`, `HOUSE-02901`, `HOUSE-03038` or `HOUSE-02951` |
| **D11** | Testing | The existing gates and suites pass; the grand tour passes; the representative render sets pass; the XNA-only and strict-XNA gates pass; ASAN and UBSAN are clean once; the 20-minute stability run and the one final 2-hour run pass; the Web and Android smoke checks pass | `HOUSE-02783`, `HOUSE-02784`, `HOUSE-03076` |
| **D12** | Documentation | `plan.md`, `cna-house.md`, `README.md` and `docs/*` describe the shipped, reduced scope; every asset is manifested and licensed | `HOUSE-03073`, `HOUSE-03075` |
| **D13** | Remaining defects | No known S1 or S2 defect. Every open issue is S3/S4 | `HOUSE-03631`, `HOUSE-03078` |

Tier E (compiled effects) is optional after DONE. DONE requires Tier S to be complete on every
platform.

---

## Quality tiers and room classification

`HOUSE-03201` required every zone to reach C5. `HOUSE-03205` replaces that with **tiers**: each
accessible cell has a tier, and the tier sets its target level. Not every storage room, WC, closet,
plant room, loft corner, utility room or side yard deserves the living room's budget.
`HOUSE-03204` copies this classification into `docs/zones.json`. Until then this section is the
definition, and `HOUSE-03203` may correct it with evidence.

| Tier | Target | Furnishing depth (rule R8) | Review coverage (rule R6) |
|---|---|---|---|
| **H** Hero | **C5** | the complete recipe, at most one bespoke piece, secondary dressing, signs of habitation | fixed views by day, at night and overcast |
| **M** Main | **C4** | the complete main-tier recipe | fixed views by day and at night |
| **S** Secondary | **C3** | enough furniture and props to be believable | one view in the zone set |
| **U** Utility | **C3** | essential pieces only; a mechanical room gets its equipment, pipes, a cabinet, a light and perhaps shelves | the zone walk at G3, by day |

**The seven hero areas**, spread across the levels so that no floor is the only beautiful one:

| Id | Hero area | Cells | C5 task |
|---|---|---|---|
| H1 | Front approach and porch | `EXT_WALK`, `EXT_FRONTYARD_W`, `EXT_FRONTYARD_E`, `EXT_DRIVEWAY`, the front elevation, `L0_PORCH` | `HOUSE-03450`, `HOUSE-00986` |
| H2 | Entry and living room | `L0_FOYER`, `L0_HALL`, `L0_LIVING` | `HOUSE-00986`, `HOUSE-00987`, `HOUSE-00988` |
| H3 | Kitchen | `L0_KITCHEN` | `HOUSE-00990` |
| H4 | Master bedroom | `L1_MASTER_BED` | `HOUSE-03453` |
| H5 | Library | `L2_LIBRARY` | `HOUSE-03454` |
| H6 | Basement cinema | `B1_CINEMA` | `HOUSE-03455` |
| H7 | Attic room | `L3_ROOM` | `HOUSE-03446` |

**Every accessible cell by tier** (13 hero cells, 19 main, the rest secondary or utility):

| Zone | H | M | S | U |
|---|---|---|---|---|
| `Z-B1` | `B1_CINEMA` | `B1_HALL`, `B1_WORKSHOP` | `B1_WC7`, `B1_GYM`, `B1_HOBBY`, `B1_CELLAR` | `B1_MECHANICAL`, `B1_ELECTRICAL`, `B1_UTILITY`, `B1_STOR1`, `B1_STOR2`, `B1_LAUNDRY2`, `B1_UNDERSTAIR` |
| `Z-L0M` | `L0_FOYER`, `L0_HALL`, `L0_LIVING`, `L0_KITCHEN`, `L0_PORCH` | `L0_FAMILY`, `L0_DINING`, `L0_SUNROOM` | `L0_BUTLERS` | — |
| `Z-L0S` | — | `L0_OFFICE` | `L0_MUDROOM`, `L0_WC1`, `L0_WC2`, `L0_PANTRY` | `L0_LAUNDRY`, `L0_CLOSET_W`, `L0_STOR` |
| `Z-GAR` | — | `L0_GARAGE` | — | `L0_GARAGE_LOFT` |
| `Z-L1` | `L1_MASTER_BED` | `L1_LANDING`, `L1_HALL`, `L1_MASTER_BATH` | `L1_HALL_W`, `L1_MASTER_CLOSET`, `L1_BED2`, `L1_BED3`, `L1_BED4`, `L1_BED5`, `L1_BATH2`, `L1_BATH3`, `L1_WC3`, `L1_WC4`, `L1_BALCONY_REAR`, `L1_BALCONY_FRONT` | `L1_LINEN`, `L1_STOR`, `L1_CLOSET_2`, `L1_CLOSET_3` |
| `Z-L2` | `L2_LIBRARY` | `L2_LANDING`, `L2_HALL`, `L2_GAMES`, `L2_SITTING` | `L2_HALL_W`, `L2_BED6`, `L2_BED7`, `L2_BATH4`, `L2_BATH5`, `L2_WC5`, `L2_WC6` | `L2_CLOSET_4`, `L2_LINEN2`, `L2_STOR2` |
| `Z-L3` | `L3_ROOM` | — | `L3_STORE_W` | `L3_STORE_E`, `L3_STORE_N`, `L3_STORE_S` |
| `Z-STAIR` | — | `L0_STAIR_MAIN`, `L1_STAIR_MAIN`, `L2_STAIR_MAIN` | `B1_STAIR`, `L2_STAIR_ATTIC`, `L3_STAIR_HEAD` | — |
| `Z-EXF` | `EXT_WALK`, `EXT_FRONTYARD_W`, `EXT_FRONTYARD_E`, `EXT_DRIVEWAY` | — | `EXT_ROAD` (the accessible part) | — |
| `Z-EXR` | — | `EXT_TERRACE`, `EXT_BACKYARD` | `EXT_GARDEN`, `EXT_ORCHARD`, `EXT_SHED` | `EXT_SIDEYARD_W`, `EXT_SIDEYARD_E` |
| `Z-STR` | — | — | the street and the neighbourhood, as scenery | — |

The elevations follow their zone's C2 architecture (`HOUSE-03267` for the rear and sides); they are
not tiered separately.

---

## Scheduling rules — time discipline

These rules exist because the project spent about 200 agent-hours polishing one ground-floor route
while four levels stayed empty. They **override task-number order**, and they override any "largest
visible defect" heuristic applied to a single area. `docs/workflow.md`'s "pick the next unfinished
task whose dependencies are complete" means: pick it **by these rules**.

**Tracks.** *Track A* is the breadth path M0–M6 (review tooling, traversal, architecture, the kit,
dressing, baseline lighting, then main rooms and hero areas). *Track B* interleaves with it: M7
environment, M8 audio, M9 application shell, M10 performance, M12 tests, M14's Web bring-up block and
M15's Android desktop-side readiness block. M11 follows G5; M13 follows M11; the Web and Android
verification blocks follow M13; M16 is last.

| Rule | Statement |
|---|---|
| **R1: breadth floor** | Up to C3, a task may raise a zone to level *k* only if **every** zone is already at level ≥ *k − 1*. C4 work (M6a, main rooms) starts only after **G3**; C5 work (M6b, hero areas) starts only after **G4**. No cell is raised above its tier's target. Exceptions, and only these: (a) S1 defects, in any zone at any time; (b) house-wide system work (renderer, pipeline, kit, validator, tooling) whose acceptance is verified on ≥ 3 zones including the least complete; (c) a regression caused by the current task |
| **R2: least complete first** | Among eligible Track A tasks, pick the zone with the lowest C3 floor; in M6a the zone with the fewest main rooms at C4; in M6b the hero area in the least-served level. Break ties by the most accessible cells still below target, then by the zone least recently worked. `Z-L0M` never wins a tie |
| **R3: consecutive cap** | At most **3 consecutive** Track A tasks on one zone, unless that zone is still *strictly* the least complete. After the cap, the next Track A task targets another zone |
| **R4: the ground floor goes last** | Until **G3**, no task targets `Z-L0M` (foyer, hall, living, family, kitchen, dining, sunroom, butler's pantry, porch) except S1 fixes and `HOUSE-03407`, which runs last in M5. Its C4 tasks (`HOUSE-00989`, `00992`, `00993`) run after every other zone's C4 task, and its hero tasks (`HOUSE-00986`, `00987`, `00988`, `00990`) after every other hero task; the dependencies encode both. Its S2/S3/S4 findings go to the backlog in `docs/visual-review/README.md`, not into the task queue. **A defect in a finished ground-floor room never outranks an unfinished area elsewhere** |
| **R5: track share** | Until G5, at least **two of every three** completed tasks are Track A. Track B fills the rest, MUST first. If every eligible Track A task is blocked, record the blockers in the task entries; Track B may then proceed |
| **R6: bounded review** | A review round captures only the views of the areas it concerns, at their tier's coverage (utility rooms once, by day; secondary rooms in the zone set; main rooms day and night; only hero areas day, night and overcast). It schedules fixes only for **S1/S2** in areas at or below their current target, and S3 only as the severity table allows. At most **2 consecutive** review → fix cycles target one area. **No zone gets a third dedicated visual-polish pass until every zone has had at least one review round. No non-hero area gets more than two dedicated polish passes unless an S1/S2 remains.** A round that finds nothing above S3 in an area at its target **stops** |
| **R7: every task pays its way** | Every task states `adv:` (the DONE item or gate it advances) and `est:` (agent-hours). A task estimated above 4 h is split. A task that reaches **2× its estimate** stops, records what it learned in a `note:`, and splits the rest into a new task with a new estimate |
| **R8: kit first, reuse freely** | Furniture and fixtures come from the shared kit: the M3 acquisition groups, the generators (`HOUSE-00985`, `HOUSE-00973`, `HOUSE-02681`) and the existing ground-floor pieces. **The same model may appear any number of times**, varied by tint, scale, dimensions, arrangement and small accessory swaps; there is no uniqueness or density quota. Depth follows the tier: utility essential only, secondary believable, main complete, hero full. Bespoke per-object Blender authoring is allowed **only in a hero area**: at most one piece per hero area, ≤ 3 h, named in its C5 task |
| **R9: no new systems** | A new runtime subsystem, interaction or simulation requires a [Planning corrections](#planning-corrections) entry showing which DONE item cannot be met without it. "It would be nicer" is not a reason, and nothing on the [Non-goals](#non-goals--not-required-for-done) list qualifies. Offline tooling that speeds up breadth work is allowed under R7 |
| **R10: reassess** | At every gate, and after every 10 completed tasks: update the [scoreboard](#zone-scoreboard) with evidence (a review round and `zone_scoreboard.py` output), compare the hours spent with each milestone's budget, and record in `docs/handoff.md` which rule chose the next task. A milestone that overruns its budget by 50 % needs a Planning corrections entry that **cuts or simplifies** the rest; continuing unchanged is allowed only for S1/S2 fixes |
| **R11: unfinished beats finished** | When the choice is between completing an unfinished area and improving a finished one, complete the unfinished area. Do not invent scope |
| **R12: bounded polish** | Visual polish outside the hero C5 tasks happens only in M11's final defect pass, which has a hard budget (8 h of S2/S3 work plus S1 fixes; about 11 h with its review and golden refresh; never more than 20 h unless S1/S2 remain). No screenshot → tweak → screenshot loop runs more than two rounds on one area |
| **R13: optional stays optional** | An `OPT` task is never scheduled while a MUST task is open, and its hours are not part of the estimate. Promoting one to MUST needs a Planning corrections entry naming the DONE item that cannot be met without it |

**Gates.** A gate passes when its review task is ticked with the scoreboard as evidence.

| Gate | Passes when | Review task |
|---|---|---|
| **G1** Traversable | every zone ≥ C1 (no `*`) | `HOUSE-03240` |
| **G2** Architecture | every zone ≥ C2 | `HOUSE-03280` |
| *(checkpoint)* Dressed | every accessible room has its tier's essential props; the house-wide lighting work waits for it | `HOUSE-03380` |
| **G3** Baseline complete | every accessible room ≥ C3 (dressed and lit) | `HOUSE-03420` |
| **G4** Presentation | every main room and main area ≥ C4 | `HOUSE-03452` |
| **G5** Showcase | every hero area at C5 | `HOUSE-03480` |
| **DONE** | D1–D13 | `HOUSE-03078` |

The gates changed meaning with `HOUSE-03205`. Under `HOUSE-03201`, G3 was "dressed" (`HOUSE-03380`,
now the checkpoint), G4 was "lit" (`HOUSE-03420`, now G3) and G5 was "every zone at C5"
(`HOUSE-03480`, now the hero areas only). Older notes, the handoff's earlier sections and the review
ledger's 2026-09-21 header use the old numbering. Read "G3" there as the dressing checkpoint and
"G4" as the new G3.

---

## Completion levels and zones

### Completion levels

A cell's level is the highest *k* for which it meets C1 … C*k*, all of them. A zone's **C3 floor**
is the lowest level among its accessible cells, capped at C3. `HOUSE-03205` merged `HOUSE-03201`'s
C3 (dressed) and C4 (lit) into the new C3, split its C5 into C4 (main rooms) and C5 (hero areas),
and retired C6.

| Level | Name | A cell at this level … |
|---|---|---|
| C0 | Absent | is missing, unreachable or not rendered |
| **C1** | Traversable | is reached by the grand tour on foot; posed doors and gates block exactly where they are drawn; no stuck point, fall-through, or clipping through important geometry; collision matches what is drawn |
| **C2** | Architecture | shows finished primary architecture at eye height: production materials on floors, walls, ceilings and roofs; doors, windows, trim and hardware; stairs with handrails, balusters and nosings; zone-specific structure where the room type needs it (attic rafters, insulation and walkway; unfinished-basement structure; garage slab and walls; garden structures); no blockout surface; no S1/S2 architectural defect |
| **C3** | Baseline complete | **the floor for every accessible cell.** Has its tier's essential furniture and fixtures from its recipe (`HOUSE-03302`), so its purpose reads at a glance; believable placement (validator green); production materials and no placeholder or debug surface; is readable at 10:30 clear and at night under its scheduled lights, is never black, and its props are not visibly darker or brighter than the surfaces they stand on (`HOUSE-03402`); exterior spaces have their primary objects and night lighting; no S1/S2 in its review view |
| **C4** | Presentation-ready | **the target for main rooms and main areas.** C3, plus the complete main-tier recipe (textiles, wall items, shelf contents and curtains where the recipe has them); lighting balanced by day and at night; its fixed views, day and night, have no S1/S2 |
| **C5** | Showcase | **hero areas only.** C4, plus at most one bespoke hero piece, secondary dressing and signs of habitation; views by day, at night and overcast with no S1/S2; S3 closed where cheap; the representative view within the High budget |
| ~~C6~~ | ~~Polished~~ | retired as a target by `HOUSE-03205`. Further polish is optional after DONE |

### Zones

Zone membership is data: `docs/zones.json`, written by `HOUSE-03204`. Until then this table is the
definition.

| Zone | Cells |
|---|---|
| `Z-B1` Basement | `B1_HALL`, `B1_MECHANICAL`, `B1_ELECTRICAL`, `B1_UTILITY`, `B1_CINEMA`, `B1_WC7`, `B1_GYM`, `B1_WORKSHOP`, `B1_STOR1`, `B1_STOR2`, `B1_HOBBY`, `B1_CELLAR`, `B1_LAUNDRY2`, `B1_UNDERSTAIR` (14) |
| `Z-L0M` Ground-floor principal route | `L0_FOYER`, `L0_HALL`, `L0_LIVING`, `L0_FAMILY`, `L0_KITCHEN`, `L0_DINING`, `L0_SUNROOM`, `L0_BUTLERS`, `L0_PORCH` (9) |
| `Z-L0S` Ground-floor service and secondary rooms | `L0_MUDROOM`, `L0_LAUNDRY`, `L0_WC1`, `L0_WC2`, `L0_OFFICE`, `L0_CLOSET_W`, `L0_STOR`, `L0_PANTRY` (8) |
| `Z-GAR` Garage | `L0_GARAGE`, `L0_GARAGE_LOFT` (2) |
| `Z-L1` First upper floor | `L1_LANDING`, `L1_HALL`, `L1_HALL_W`, `L1_MASTER_BED`, `L1_MASTER_BATH`, `L1_MASTER_CLOSET`, `L1_BED2`, `L1_BED3`, `L1_BED4`, `L1_BED5`, `L1_BATH2`, `L1_BATH3`, `L1_WC3`, `L1_WC4`, `L1_LINEN`, `L1_STOR`, `L1_CLOSET_2`, `L1_CLOSET_3`, `L1_BALCONY_REAR`, `L1_BALCONY_FRONT` (20) |
| `Z-L2` Second upper floor | `L2_LANDING`, `L2_HALL`, `L2_HALL_W`, `L2_LIBRARY`, `L2_GAMES`, `L2_SITTING`, `L2_BED6`, `L2_BED7`, `L2_BATH4`, `L2_BATH5`, `L2_WC5`, `L2_WC6`, `L2_CLOSET_4`, `L2_LINEN2`, `L2_STOR2` (15), plus the non-accessible `L2_BALCONY_JULIET` |
| `Z-L3` Attic | `L3_ROOM`, `L3_STORE_W`, `L3_STORE_E`, `L3_STORE_N`, `L3_STORE_S` (5) |
| `Z-STAIR` Vertical circulation | `B1_STAIR`, `L0_STAIR_MAIN`, `L1_STAIR_MAIN`, `L2_STAIR_MAIN`, `L2_STAIR_ATTIC`, `L3_STAIR_HEAD` (6 cells, 8 flights) |
| `Z-EXF` Front exterior | `EXT_ROAD` (the accessible part), `EXT_WALK`, `EXT_FRONTYARD_W`, `EXT_FRONTYARD_E`, `EXT_DRIVEWAY`, and the front and garage elevations and roof |
| `Z-EXR` Rear and side exterior | `EXT_TERRACE`, `EXT_BACKYARD`, `EXT_GARDEN`, `EXT_SHED`, `EXT_ORCHARD`, `EXT_SIDEYARD_W`, `EXT_SIDEYARD_E`, and the rear and side elevations |
| `Z-STR` Street and neighbourhood | the road beyond the lot, the 60 generated neighbour buildings, the street furniture, the road-end barriers and the horizon (`EXT_WORLD`, `EXT_NORTHSTRIP` as scenery) |

Not in any zone: `CELL_FRIDGE_INTERIOR` and `CELL_FREEZER_INTERIOR`, which are nested,
non-accessible appliance interiors.

---

## Zone scoreboard

**As of 2026-09-21 (`HOUSE-03203`).** Round 103's whole-property day/night review confirms the
levels below and the H/M/S/U classification without change. Update per rule R10.

`*` means reachable, but pending the **house-wide door/gate defect**: every leaf is drawn *closed*,
yet nothing collides with it (`DynamicObstacles` is never filled) and `PortalRuntime` starts every
leafed portal closed. The player therefore walks through closed doors and gates. M1 fixes this for
every zone at once.

| Zone | C3 floor | Main at C4 | Hero cells at C5 | Props (133 total) | Review views (56 total) | Evidence | Largest gaps |
|---|---|---|---|---|---|---|---|
| `Z-B1` | **C1\*** | 0 / 2 | 0 / 1 | **0** in 14 cells | 4 | Round 103 day/night. Flights walk both ways (`HOUSE-00615`). Every cell reachable (world rule 5) | S2: nearly black; empty; generic slab doors; no services or structure dressing |
| `Z-L0M` | **C2** (dressing of C3 done; lighting S2 open) | 0 / 3 | 0 / 5 | 106 | 23 | Round 103 day/night plus 102 prior rounds. Primary furniture in all 9 cells (`HOUSE-01037`–`01076`) | S2: Basic-lit furniture much darker than the baked receivers (piano, sofas); service-room night depth. **Goes last by R4** |
| `Z-L0S` | **C1\*** | 0 / 1 | — | **0** in 8 cells | 2 | Round 103 day/night; shell, materials, bakes | S2: empty; office/mudroom lack readable night depth |
| `Z-GAR` | **C1\*** | 0 / 1 | — | **0** in 2 cells | 1 | Round 103 day/night; finished sectional door outside (`HOUSE-00935`, `00947`, `00948`) | S2: 70.6 m² empty dark volume; loft access/guard and interior finish do not read |
| `Z-L1` | **C1\*** | 0 / 3 | 0 / 1 | **1** (front-balcony lantern) in 20 cells | 6 | Round 103 day/night; reached on foot (`HOUSE-00489`) | S2: empty; dark after sunset; room-specific joinery/finishes absent |
| `Z-L2` | **C1\*** | 0 / 4 | 0 / 1 | **0** in 15 cells | 6 | Round 103 day/night; flights walkable | S2: empty; dark after sunset; room-specific joinery/finishes absent |
| `Z-L3` | **C1\*** | — | 0 / 1 | **0** in 5 cells | 2 | Round 103 day/night; automatic attic crouch (`HOUSE-00558`) | S2: empty; no structure/finished-room contrast |
| `Z-STAIR` | **C1\*** | 0 / 3 | — | 0 | 4 | Round 103 day/night; 8 flights walk up and down | S2: main foot under-readable; basement flight almost black |
| `Z-EXF` | **C3\*** (C4-level content in place) | — | 0 / 4 | 11 | 4 | Round 103 day/night/overcast; ≈ 25 facade, roof, entry and approach tasks; property bot walk (`HOUSE-00782`) | No zone-specific S2; gate leaves retain the house-wide fixed-leaf defect (`*`) |
| `Z-EXR` | **C2\*** (C3 partial) | 0 / 2 | — | 15 | 3 | Round 103 day/night; terrace, shed shell, beds and trellis (`HOUSE-00768`, `00769`, `00771`, `01291`) | S2: sparse working areas; rear/side architectural finish below the front standard |
| `Z-STR` | **C2** (C3 partial) | — | — | — | 1 | Round 103 day/night; N1–N60, street furniture, barriers (`HOUSE-00841`–`00849`, `00856`, `00857`) | S3: plain road foreground and repeated vegetation band; defer to M11 |

`HOUSE-03204` adds the following objective inputs, generated by
`python3 tools/world/zone_scoreboard.py`. `Lit accessible` counts cells with at least one authored
light group; it does not claim that their baseline lighting is visually complete. C4 and C5 are
target counts from the approved tier classification, not completion claims.

| Zone | Cells | Accessible | Props | Props/access. | Zero-prop accessible | Lit accessible | Review poses | C4 main targets | C5 hero targets |
|---|---:|---:|---:|---:|---|---|---:|---|---|
| `Z-B1` | 14 | 14 | 0 | 0.00 | 14 | 14 / 14 | 4 | 2 | 1 |
| `Z-L0M` | 9 | 9 | 106 | 11.78 | 0 | 9 / 9 | 23 | 3 | 5 |
| `Z-L0S` | 8 | 8 | 0 | 0.00 | 8 | 8 / 8 | 2 | 1 | 0 |
| `Z-GAR` | 2 | 2 | 0 | 0.00 | 2 | 2 / 2 | 1 | 1 | 0 |
| `Z-L1` | 20 | 20 | 1 | 0.05 | 19 | 20 / 20 | 6 | 3 | 1 |
| `Z-L2` | 16 | 15 | 0 | 0.00 | 15 | 15 / 15 | 6 | 4 | 1 |
| `Z-L3` | 5 | 5 | 0 | 0.00 | 5 | 5 / 5 | 2 | 0 | 1 |
| `Z-STAIR` | 6 | 6 | 0 | 0.00 | 6 | 6 / 6 | 4 | 3 | 0 |
| `Z-EXF` | 5 | 5 | 11 | 2.20 | 1 | 5 / 5 | 4 | 0 | 4 |
| `Z-EXR` | 7 | 7 | 15 | 2.14 | 4 | 3 / 7 | 3 | 2 | 0 |
| `Z-STR` | 2 | 0 | 0 | — | 0 | 0 / 0 | 1 | 0 | 0 |

**Minimum level today: C1\*** (seven zones). By rule R1, the only permitted zone-raising work is
M1 (to C1) and then M2 (to C2). M3 tooling may run alongside.

---

## Visual review protocol and defect severity

The review ledger stays in [`docs/visual-review/README.md`](docs/visual-review/README.md). Captures
stay local and Git-ignored. Strict golden references (`tests/render/reference/`) are a separate
thing: never refresh a golden as part of a review round unless the round's task changed that image
on purpose.

A round records: the zones and areas captured (`HOUSE-03202` sets, at the tier's coverage of rule
R6); every defect with its **severity**, **zone** and **tier**; the scoreboard change; and **the
next task together with the rule (R1–R13) that chose it**. The fixed ground-floor route is not the
default review. Gate reviews capture every zone, each at its tier's coverage, on one contact sheet.

| Severity | Meaning | Examples | Scheduling |
|---|---|---|---|
| **S1 Blocker** | Must fix. Breaks the walk or the image outright | crash; broken traversal or an unreachable area; stuck or falling through a floor; missing major geometry (a hole to the void); major rendering corruption; debug colours; content failing to load; a black or blown-out room at noon; a broken platform build | Fix now, in any zone, at any time (R1 exception a) |
| **S2 Major** | Fix before DONE. Breaks believability at normal eye height | a room that reads empty or as the wrong type; clearly visible bad collision; a major prop placement problem (floating, intersecting, wrong scale); obvious clipping in important areas; a large missing material or texture; large z-fighting; a severe lighting artefact; a room unreadable by day or at night | Fix within the area's current target level, subject to R1/R4 |
| **S3 Moderate** | Noticeable, but believability holds | visible tiling; imperfect colour or brightness balance; sparse secondary dressing; small seams; dull corners | Fix **only** when it is cheap (about 30 minutes or less), repeatedly visible, in a hero area, or in a release capture. Otherwise logged. Scheduled only in a hero C5 task or in M11's bounded pass |
| **S4 Minor polish** | Visible only on close inspection | a tiny prop placement issue; a subtle material mismatch; a small repetitive detail; a minor shadow oddity not obvious during a normal walk; low texture resolution on small props; micro-clutter | **Never blocks DONE.** Logged only; not scheduled before DONE |

**Diminishing returns.** Once an area meets its tier's target with no S1/S2 open, further findings
there are logged and **not** scheduled until M11. Two consecutive rounds on one area (R6) is the
limit. The review ledger's 2026-09-21 header predates `HOUSE-03205`: where it says "G3" and "G4",
read the dressing checkpoint and the new G3 (see [Gates](#scheduling-rules--time-discipline)).

---

## How to read a task

```
- [ ] HOUSE-09999 — Objective, in one line
      dep: HOUSE-09991, HOUSE-09992 · sys: visibility · plat: LNX · pri: MUST · zone: Z-L1 · adv: D2, G1 · est: 3
      files: src/visibility/PortalTraversal.cpp|hpp
      accept: (1) …; (2) …
      verify: unit PortalTraversalTests.ClosedDoorCulls; zone capture Z-L1
```

| Field | Meaning |
|---|---|
| `dep` | Tasks that must be complete first. A dependency on a **cancelled** task is satisfied by the cancellation; a range `A…B` means the non-cancelled, non-optional tasks in it. `—` means none |
| `sys` | Owning subsystem, matching `src/` or `tools/` |
| `plat` | `LNX`, `WEB`, `AND`, `ALL`, `TOOL`, `CI` |
| `pri` | `MUST` (required for DONE), `SHOULD` (wanted, cut first under R10), `OPT` (only after DONE, rule R13) |
| `zone` | The zone a Track A task raises (rule R2/R3). `all` for house-wide work |
| `adv` | The DONE item or gate the task advances (rule R7) |
| `est` | Estimated agent-hours (rule R7). Since `HOUSE-03205` every open task has one, and the [estimate](#remaining-work-estimate) is their sum |
| `amended:` | A 2026-09-21 change to a carried task's scope, dependency or priority, with the reason, by `HOUSE-03201` or `HOUSE-03205`. The original text above it is unchanged, except where `HOUSE-03205` re-authored a `HOUSE-03201` task and says so (the earlier text is `plan.md` at `5927073`) |

**Ids.** Carried tasks keep their legacy ids. New work takes the next free id in its **milestone's**
reserved range (milestones are this plan's phases for that rule). Cancelled ids are listed, with
their reasons, in [Cancelled by the second scope reduction](#cancelled-by-the-second-scope-reduction-house-03205)
and [Cancelled by the first scope reduction](#cancelled-by-the-first-scope-reduction-house-03201),
and the latter are also struck through in the legacy ledger. **No id is ever reused.**

**Carried tasks read as written, plus their amendments.** Their original notes may still say
"phase N" and refer to legacy phases. The milestone they now sit in governs their scheduling.

---

## Milestone index and ID ranges

| Milestone | Track | ID range (new tasks) | Open tasks | Gate / exit | Advances | Budget (expected agent-h) |
|---|---|---|---|---|---|---|
| [M0](#m0--scope-reset-and-breadth-instruments) — Scope reset and breadth instruments | A (support) | 03201–03220 | 3 | review instruments ready | R10, G1–G5 | 7 |
| [M1](#m1--whole-property-traversal-c1-everywhere--gate-g1) — Whole-property traversal | A | 03221–03260 | 8 | **G1** · `HOUSE-03240` | D2 | 19 |
| [M2](#m2--architectural-completion-c2-everywhere--gate-g2) — Architectural completion | A | 03261–03300 | 8 | **G2** · `HOUSE-03280` | D1 | 18 |
| [M3](#m3--the-reusable-furnishing-kit) — The reusable furnishing kit | A (support) | 03301–03340 | 16 | kit ready for M4 | D3 | 33 |
| [M4](#m4--dressing-everywhere-the-furnishing-half-of-c3--checkpoint) — Dressing everywhere | A | 03341–03400 | 40 | checkpoint · `HOUSE-03380` | D3 | 49 |
| [M5](#m5--baseline-lighting-everywhere-the-lighting-half-of-c3--gate-g3) — Baseline lighting everywhere | A | 03401–03440 | 12 | **G3** · `HOUSE-03420` | D3, D6 | 24.5 |
| [M6](#m6--main-rooms-and-hero-areas-c4-c5--gates-g4-and-g5) — Main rooms and hero areas | A | 03441–03500 | 20 | **G4** · `HOUSE-03452`, **G5** · `HOUSE-03480` | D4 | 32.5 |
| [M7](#m7--a-compact-environment) — A compact environment | B | 03501–03540 | 13 | `HOUSE-03520` | D5 | 21 |
| [M8](#m8--atmospheric-audio-essentials) — Atmospheric audio essentials | B | 03541–03570 | 11 | `HOUSE-01939` | D7 | 14.5 |
| [M9](#m9--application-shell-what-a-polished-demo-needs) — Application shell | B | 03571–03600 | 10 | `HOUSE-02528` | D8 | 12.5 |
| [M10](#m10--performance-measurement-driven) — Performance, measurement-driven | B | 03601–03630 | 6 | `HOUSE-02404` | D9 | 13 |
| [M11](#m11--final-defect-pass-bounded--after-g5-only) — Final defect pass | after G5 | 03631–03680 | 3 | `HOUSE-02713` | D2, D4, D13 | 11 |
| [M12](#m12--representative-tests) — Representative tests | B | 03681–03700 | 4 | `HOUSE-02600` | D11 | 5 |
| [M13](#m13--linux-desktop-release) — Linux desktop release | after M11 | 03701–03720 | 10 | `HOUSE-02797` | D9–D13 | 13 |
| [M14](#m14--web) — Web | bring-up: B · verification: after M13 | 03721–03750 | 17 | `HOUSE-02905` | D10 | 25 |
| [M15](#m15--android) — Android | readiness: B · device: after M13 | 03751–03780 | 23 | `HOUSE-03042` or BL-13 record | D10 | 29 |
| [M16](#m16--final-release) — Final release | last | 03781–03800 | 8 | **DONE** · `HOUSE-03078` | D9–D13 | 9 |

Legacy ids 03121–03200 are unallocated and stay unused. The budgets are the sums of the tasks'
`est:` values; rule R10 compares the hours spent against them.

---

## M0 — Scope reset and breadth instruments

Track A support. Gives every zone the review instruments the ground-floor route had, before anyone
works on the zones. M14's bring-up block and M15's desktop-side readiness block may run any time
after G1 as Track B.

- [x] HOUSE-03201 — Scope reduction: rewrite the plan around the architectural showcase, archive the legacy ledger, record ADR-0014
      dep: — · sys: — · plat: ALL · pri: MUST · zone: all · adv: all · est: 6
      files: plan.md, docs/history/plan-legacy-2026-09-21.md, docs/decisions/ADR-0014-showcase-scope.md (+ status pointers in ADR-0005/0008/0010 and the ADR index), cna-house.md, README.md, CLAUDE.md, AGENTS.md, docs/workflow.md, docs/versioning.md, docs/conventions.md, docs/handoff.md, docs/visual-review/README.md
      accept: (1) every task open on 2026-09-21 is carried, deferred or cancelled exactly once, each cancellation with its reason; (2) DONE, the scheduling rules, the zones, the completion levels and the gates are defined; (3) every document that contradicted the new scope points at ADR-0014; (4) no runtime code, data or asset changed
      verify: `tools/ci/run_checks.sh`; the builder's audit (all 764 legacy open ids accounted for once); `git diff --stat` shows documentation only
      note: (2026-09-21) the legacy `plan.md` moved to `docs/history/plan-legacy-2026-09-21.md`. Git records the move as a copy, so `git blame -C` keeps every line's history. Cancelled entries there are struck through with a `cancelled:` line; carried entries carry a `moved:` line pointing here.

- [x] HOUSE-03205 — Second scope reduction: quality tiers, a reusable kit, a compact environment and audio set, representative tests, a bounded polish pass
      dep: HOUSE-03201 · sys: — · plat: ALL · pri: MUST · zone: all · adv: all · est: 5
      files: plan.md, docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md (+ status pointers in ADR-0014, ADR-0010 and the ADR index), cna-house.md, README.md, CLAUDE.md, AGENTS.md, docs/workflow.md, docs/handoff.md
      accept: (1) every task open at `5927073` is kept, merged, cancelled with its reason, or moved to Optional after DONE, exactly once; (2) "C5 everywhere" is replaced by quality tiers (C3 for every accessible room, C4 for main rooms, C5 for seven hero areas) with every cell classified; (3) the Definition of DONE, the non-goals and the scheduling rules state the reduced scope; (4) every open task carries `est:`, and the estimate is recomputed from them; (5) no runtime code, data or asset changed
      verify: the builder's audit (every id open at `5927073` accounted for once; no open task depends on an optional one); `tools/ci/run_checks.sh`; `git diff --stat` shows documentation only

- [x] HOUSE-03202 — Give every zone a fixed review view set and a whole-property contact sheet
      dep: HOUSE-03201 · sys: tools · plat: TOOL · pri: MUST · zone: all · adv: G1–G5 · est: 2.5
      files: tools/visual/capture_review.py, docs/visual-review/README.md
      accept: (1) each of the 11 zones has ≥ 4 fixed eye-height poses (at least one per room type present in the zone; `Z-STAIR` has one per flight group), framed on the room, not a wall; (2) `--zone <Z-…>` captures one zone and `--all-zones` captures them all, in `clear-day` and `clear-night`; (3) one labelled contact sheet per zone is written beside the captures; (4) the 32 existing poses are kept unchanged and assigned to their zones
      verify: run `--all-zones` in both scenarios; inspect every contact sheet
      amended: (2026-09-21, `HOUSE-03205`) accept (1) is reduced: every hero area and every main room has one fixed pose (two for the front approach), and each zone has at least one pose in a secondary room; utility rooms have no pose of their own and are judged from the zone walk at G3; `clear-overcast` is captured only for hero areas (rule R6)

- [x] HOUSE-03203 — Baseline review of the whole property: level every zone with evidence and seed the defect lists
      dep: HOUSE-03202, HOUSE-03204 · sys: — · plat: LNX · pri: MUST · zone: all · adv: R10 · est: 2.5
      accept: (1) one ledger round covers all 11 zones, day and night; (2) every observed defect is classified S1–S4 and filed under its zone; (3) the scoreboard levels are confirmed or corrected with the round and `zone_scoreboard.py` evidence, and "provisional" is removed; (4) each M2 zone task gets its concrete S1/S2 list as a `note:`
      verify: the ledger round; the updated scoreboard
      amended: (2026-09-21, `HOUSE-03205`) adds (5): confirm or correct the room tiers of [Quality tiers](#quality-tiers-and-room-classification) from what the round sees, and record any change here
      note: (2026-09-21) Round 103 confirms every existing zone level and every H/M/S/U classification without change; S1 none; the house-wide fixed-leaf/pass-through mismatch is S2 and belongs to M1

- [x] HOUSE-03204 — `tools/world/zone_scoreboard.py` and `docs/zones.json`: objective per-zone completeness numbers
      dep: HOUSE-03201 · sys: tools · plat: TOOL · pri: MUST · zone: all · adv: R10 · est: 2
      files: docs/zones.json, tools/world/zone_scoreboard.py, tools/ci/run_checks.sh
      accept: (1) `docs/zones.json` puts each of the 96 cells in exactly one zone, or in an explicit `none` group with a reason; (2) the tool prints per zone: cells, accessible cells, props, props per accessible cell, cells with zero props, cells with ≥ 1 light group, and review poses in the zone; (3) `--check` fails on a cell in zero or two zones and runs in `run_checks.sh`
      verify: run it; paste its table into the scoreboard
      amended: (2026-09-21, `HOUSE-03205`) `docs/zones.json` also records each cell's tier (`H`, `M`, `S`, `U`) and hero-area id (`H1`–`H7`), and the tool prints per zone the main rooms at C4 and the hero cells at C5 alongside the C3 inputs

---

## M1 — Whole-property traversal (C1 everywhere) · gate G1

Track A. **The one gameplay acceptance criterion:** the player reliably walks through and inspects
every intended area without getting stuck, clipping through important geometry, or meeting broken
navigation. No interaction framework is built for doors (ADR-0014). The grand tour
(`HOUSE-03226`) is the most valuable completion proof in this plan: from here on every task keeps it
green.

- [x] HOUSE-03221 — Author a static pose for every door leaf, gate and the garage door
      dep: HOUSE-03201 · sys: world · plat: TOOL · pri: MUST · zone: all · adv: D2, G1 · est: 3
      files: assets-src/world/*.json (the chosen home), tools/world/world_schema.py, tools/world/validate_world.py, docs/world-format.md
      accept: (1) every leafed portal (63 doors, 3 gates, the garage door) has an authored static open fraction; (2) interior doors default to resting open (≥ 0.85) on their swing side, and a door stays closed only where no intended-accessible space lies behind it, each such case listed; (3) every door on an accessible route leaves ≥ 0.70 m clear width; (4) the validator rejects a missing pose, a closed door on an accessible route, and a swing arc that intersects walls or placed props; (5) the schema home is recorded in `docs/world-format.md`. Reusing `openFraction` from the existing door rows and `initialstate.json` is allowed; no behaviour code is added
      verify: `validate_world.py` (new rule); `--selftest` cases for each rejection
      note: today every leaf is drawn closed while nothing collides with it and culling treats it as closed. This is the data half of the fix.
      note: (2026-09-21) all 63 walkthrough doors, the garage door and three gates now have static poses; only the facade-only Juliet door and rear non-traversal gate are closed and record why. Rule 14 proves route clearance and wall/prop arc clearance. Its measured arc moved the hall-family swing into the family room and the low dog bed 0.98 m north, the smallest correction that clears both sides' existing dressing

- [ ] HOUSE-03222 — Draw every leaf at its static pose
      dep: HOUSE-03221 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D2, G1 · est: 3
      files: tools/blender/house_shell_gen.py, tools/world/build_chunks.py, tools/world/verify_shell.py
      accept: (1) the shell builds each hinged leaf rotated about its hinge, and each sliding leaf translated, to the authored pose; (2) `HOUSE-00486`'s "a leaf on both sides" rule is replaced by one leaf owned by the cell it swings into, plus whatever the other cell needs so culling never shows a hole; (3) `verify_shell.py` asserts pose, hinge side and no wall intersection; (4) the finished joinery of `HOUSE-00932`, `00940` and `00942` keeps its detail at the new pose; (5) chunk budgets hold, or their exceptions are re-measured and recorded
      verify: `verify_shell.py`; zone captures show doors resting open on every floor

- [ ] HOUSE-03223 — Make every posed leaf solid
      dep: HOUSE-03222 · sys: physics · plat: ALL · pri: MUST · zone: all · adv: D2, G1 · est: 2
      files: tools/world/build_collision.py (static) or src/physics/DynamicObstacles.cpp (filled once at load), tests/unit/PosedLeafCollisionTests.cpp
      accept: (1) every posed door leaf, gate leaf and the garage door has a collision proxy at its pose; (2) a closed leaf blocks the capsule; (3) `PortalClearanceTests`, `InsideGeometryTests` and the stair traversal tests pass with the leaves present
      verify: unit PosedLeafCollisionTests.*, PortalClearanceTests.*, InsideGeometryTests.*

- [ ] HOUSE-03224 — Drive portal apertures from the static poses
      dep: HOUSE-03221 · sys: visibility · plat: ALL · pri: MUST · zone: all · adv: D2, G1 · est: 2
      files: src/visibility/PortalRuntime.cpp|hpp, src/visibility/VisibilitySystem.cpp, tests
      accept: (1) at load each leafed portal's aperture equals its authored pose (open or closed per `HOUSE-00665`'s hysteresis), so culling matches what is drawn; (2) the door-state matrix test (`HOUSE-00687`) is re-expressed over the posed states and still proves both directions; (3) the 18 culled/unculled render pairs stay within their bound
      verify: integration door-state matrix; render culling pairs
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02583`: the door-state matrix is the integration proof that every leafed portal's visibility and collision agree with its pose, from both sides

- [ ] HOUSE-03225 — Accessibility manifest: the intended-accessible cells and a standing point in each
      dep: HOUSE-03204 · sys: world · plat: TOOL · pri: MUST · zone: all · adv: D2, G1 · est: 2
      files: docs/zones.json or assets-src/world (recorded in docs/world-format.md), tools/world/validate_world.py
      accept: (1) data lists every intended-accessible cell with a standing point: every interior cell except the two appliance interiors, the exterior cells inside the fences, the porch and balconies, and the accessible part of the road; (2) excluded cells (`EXT_WORLD`, `EXT_NORTHSTRIP`, `L2_BALCONY_JULIET`, `CELL_FRIDGE_INTERIOR`, `CELL_FREEZER_INTERIOR`) are listed with reasons; (3) the validator proves each standing point is on walkable collision inside its cell with headroom for the standing or crouched capsule (`HOUSE-00558`)
      verify: `validate_world.py`

- [ ] HOUSE-03226 — Grand-tour test: walk the real controller from the spawn to every accessible cell and back
      dep: HOUSE-03223, HOUSE-03224, HOUSE-03225 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D2, G1 · est: 4
      files: tests/integration/GrandTourTests.cpp
      accept: (1) a deterministic headless run from the `EXT_ROAD` spawn through posed doors and gates and all 8 flights to every manifest standing point, and back; (2) arrival within 0.60 m (`HOUSE-00615`'s tolerance); (3) no step ends inside geometry, the boundary counter stays 0, and nothing falls below a floor; (4) the route is computed from the portal graph and the standing points, never a hand-written script per room; (5) runtime < 120 s so it runs in CI
      verify: integration GrandTourTests.EveryAccessibleCellIsReachedOnFoot
      note: from here on every architecture and furnishing task keeps this test green. A prop that blocks a route fails its own task; no later review has to catch it.

- [ ] HOUSE-03227 — Traversal sweep of every zone, and fix what it finds
      dep: HOUSE-03226 · sys: physics · plat: LNX · pri: MUST · zone: all · adv: D2, G1 · est: 2
      accept: (1) the 20-minute seeded random walk (`HOUSE-00618`) is run from a start on each level and outside; (2) a manual first-person walk of every zone checks stuck points, snagging edges, head bumps, stair transitions, attic crouch entry and exit, and eye clipping in the tightest spaces (closets, under the stair, the eaves); (3) every S1/S2 finding is fixed or filed as a task in the right milestone; (4) findings are recorded in the ledger
      verify: the seeded walks; the ledger round

- [ ] HOUSE-03240 — **Gate G1 review: every zone at C1**
      dep: HOUSE-03226, HOUSE-03227 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G1 · est: 1
      accept: the scoreboard shows every zone ≥ C1 with no `*`; the grand tour is green; the open S1 list is empty; the handoff records the gate
      verify: scoreboard update in this file; `docs/handoff.md`

---

## M2 — Architectural completion (C2 everywhere) · gate G2

Track A. Every zone's architecture is finished to eye-height quality before anyone furnishes
anything (rule R1). Each zone task starts from `HOUSE-03203`'s S1/S2 list for that zone. Structure
that is really dressing (ducts, pipes, boxes) belongs to M4. Depth follows the tier: utility rooms
get plain, correct construction, not decoration. Where a door lacks hardware, the zone task reuses
the lever set of the finished joinery (`HOUSE-00932`, `00940`, `00942`); `HOUSE-01197` is cancelled.

- [ ] HOUSE-03261 — Bring the basement's architecture to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G2, D1 · est: 3
      accept: C2 for all 14 `Z-B1` cells: finished rooms (cinema, gym, hobby, WC, laundry) and unfinished rooms (mechanical, electrical, utility, workshop, storage, cellar) read as such through their finishes; exposed structure (joists, columns, a slab edge) where unfinished, from the shell generator or `prop_kit_gen.py`, whichever is cheaper; door leaves have casing and hardware; basement windows or light wells as designed; the zone's S1/S2 architecture findings closed
      verify: `Z-B1` zone capture, day and night; `verify_shell.py`; grand tour
      note: Round 103 S2 architecture list: generic slab doors/casings; finished and unfinished room types do not read distinctly; the zone is nearly black even by day

- [ ] HOUSE-03262 — Bring the ground-floor service rooms and the garage to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-L0S, Z-GAR · adv: G2, D1 · est: 2.5
      accept: C2 for `Z-L0S` and `Z-GAR`: the garage interior's slab, walls, ceiling and the inside face of the sectional door read as finished garage construction; the loft has a real means of access as designed and a guard; mudroom, laundry, WCs, pantry, office and closets have finished trim and door hardware
      verify: `Z-L0S` and `Z-GAR` captures; grand tour
      note: Round 103 S2 architecture list: office/mudroom night depth is unreadable; garage interior finish and inside door face do not read; loft access and guard are absent

- [ ] HOUSE-03263 — Bring the first upper floor's architecture to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G2, D1 · est: 2.5
      accept: C2 for all 20 `Z-L1` cells: bedroom, bathroom, closet and corridor finishes match their palette; window reveals, sills and interior trim finished; door joinery at least at the `HOUSE-00942` standard on landing- and hall-facing doors; balconies' guards and finishes consistent with the front balcony
      verify: `Z-L1` capture; grand tour
      note: Round 103 S2 architecture list: generic dark door openings and insufficient room-specific joinery/finish distinction across the empty floor

- [ ] HOUSE-03264 — Bring the second upper floor's architecture to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G2, D1 · est: 2
      accept: as `HOUSE-03263`, for the 15 accessible `Z-L2` cells; the library's shelving walls are left to M4 unless the shell carries them
      verify: `Z-L2` capture; grand tour
      note: Round 103 S2 architecture list: as L1, with the library/games/sitting identities not yet supported by their architectural finish

- [ ] HOUSE-03265 — Bring the attic's architecture to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-L3 · adv: G2, D1 · est: 3
      accept: C2 for the 5 `Z-L3` cells: visible roof structure in the stores (rafters, ridge, collar ties or purlins), insulation between joists where unfinished, a boarded walkway through the stores, dormer reveals, knee walls and a finished ceiling in `L3_ROOM`; the crouch zones read as low headroom rather than as a collision surprise
      verify: `Z-L3` capture; grand tour (crouch)
      note: Round 103 S2 architecture list: stores lack readable roof structure/unfinished construction and `L3_ROOM` lacks a clearly finished envelope

- [ ] HOUSE-03266 — Bring the vertical circulation to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-STAIR · adv: G2, D1 · est: 2
      accept: all 8 flights have handrails, balusters, newels, nosings, stringers and landing trim consistent with their stair's character (the main stair finished; the basement and attic stairs plainer); no flight reads as a ramp or a solid mass from its foot or its head
      verify: `Z-STAIR` capture from every foot and head; stair traversal tests
      note: Round 103 S2 architecture list: main stair foot is under-readable and the basement flight is almost black; verify every flight's trim/silhouette from both ends

- [ ] HOUSE-03267 — Bring the rear and side elevations up to the front elevation's architectural standard
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-EXR · adv: G2, D1 · est: 2
      accept: from anywhere accessible in the garden and side yards: window grilles and shutters (or their deliberate absence) are consistent with `HOUSE-00933`; sills, trim, corner boards, roof edges and rainwater goods match `HOUSE-00934`/`00944`; rear doors are finished joinery; the shed's exterior reads as finished
      verify: `Z-EXR` capture, day and night
      note: Round 103 S2 architecture list: rear and side elevation trim/rainwater/joinery treatment reads materially less finished than the front

- [ ] HOUSE-03280 — **Gate G2 review: every zone at C2**
      dep: HOUSE-03261, HOUSE-03262, HOUSE-03263, HOUSE-03264, HOUSE-03265, HOUSE-03266, HOUSE-03267 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G2 · est: 1
      accept: an all-zone capture round with no S1/S2 architecture defect open; the scoreboard shows every zone ≥ C2; the handoff records the gate
      verify: ledger round; scoreboard

---

## M3 — The reusable furnishing kit

Track A support: house-wide tooling, allowed at any time by R1 exception (b). It makes furnishing
about 60 empty rooms tractable with **a reusable modular kit**: beds, tables, desks, chairs,
cabinets, shelving, wardrobes, couches, bathroom fixtures, kitchen and utility modules, lamps,
generic storage and generic clutter. Pieces are generated (`HOUSE-00985`, `HOUSE-00973`,
`HOUSE-02681`) or acquired in **capped** groups, and they are reused freely across the house, varied
by tint, scale, dimensions, arrangement and small accessory swaps (rule R8). Before this plan every
ground-floor piece had its own bespoke Blender and prepare script and its own `--check` gate in
`run_checks.sh` (28 today); new kit pieces are covered by group-level gates instead.

Every acquisition group delivers **static visual props only** (closed doors, no interiors, no
moving parts), each with a collision proxy and a manifest row. Its cap is an upper bound, not a
target: acquire, generate or reuse only what the tiered recipes of `HOUSE-03302` need. (`HOUSE-03201`
stated this in each group; it is stated once here.) The ground-floor pieces are reused before
anything is acquired.

- [ ] HOUSE-03301 — Stop furnishing from paying for the pet navigation graph
      dep: HOUSE-03201 · sys: world · plat: TOOL · pri: MUST · zone: all · adv: D3 (cost) · est: 1.5
      files: tools/world/validate_world.py, tools/ci/run_checks.sh, src/world/WorldLoader.cpp (only if it rejects a stale graph)
      accept: (1) no gate or world rule requires `layout.nav.json`'s waypoints, perches, beds or bowls to stay consistent with props, so placing furniture never requires moving a pet waypoint again; (2) the data files and loader stay until the cleanup candidates are actioned: this task removes the coupling only; (3) the gates and unit tests are green
      verify: `run_checks.sh`; a probe prop placed across an old waypoint passes validation
      note: `HOUSE-01074` had to shift two sunroom pet waypoints to place two chairs. That cost now buys nothing.

- [ ] HOUSE-03302 — Room recipes and the furnishing kit catalogue
      dep: HOUSE-03201 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3, G3 · est: 2.5
      files: docs/furnishing-kit.md
      accept: (1) a recipe for every room type in the house (master, double, child's, teenager's, guest and sewing bedrooms; bathroom; WC; walk-in closet; linen and storage closets; study; library; games room; sitting room; mudroom; laundry; pantry; garage; garage loft; basement hall; mechanical; electrical; utility; cinema; gym; workshop; storage; wine cellar; hobby room; attic room; attic store; attic services; shed; stair landing; balcony), each listing its **primary (C3)** and **secondary (C5)** pieces with typical dimensions and clearances; (2) a catalogue of existing reusable pieces (the ground-floor kit from `HOUSE-01037`–`01076`) and the recipes they serve; (3) the reuse limits of `HOUSE-00972` and the hero-piece rule R8; (4) the needed count per M3 acquisition group, derived from the recipes
      verify: review against `layout.cells.json`: every accessible cell maps to a recipe
      amended: (2026-09-21, `HOUSE-03205`) accept (3) and (4) are replaced: (3) the reuse policy of rule R8 (reuse freely; vary by tint, scale, dimensions, arrangement and small accessory swaps; no uniqueness quota, `HOUSE-00972` is cancelled); (4) every recipe is tiered: **utility** lists only the essential pieces, **secondary** enough to read as the room, **main** the complete set, **hero** the complete set plus secondary dressing and signs of habitation; (5) the needed count per acquisition group, within the caps below. The primary/secondary split of (1) becomes the tier split

- [ ] HOUSE-03303 — Placement validator for static props
      dep: HOUSE-03225 · sys: world · plat: TOOL · pri: MUST · zone: all · adv: D2, D3 · est: 2.5
      files: tools/world/validate_world.py (or a sibling checker run by it), tests
      accept: every row in `layout.props.json` (1) rests on a floor or support surface within 1 cm; (2) lies inside its cell; (3) does not intersect walls, openings or other props' collision proxies beyond a stated tolerance; (4) keeps door-swing arcs, window fronts and a ≥ 0.70 m circulation path clear; the check runs in `run_checks.sh`, and the grand tour remains the end-to-end proof
      verify: `--selftest` cases for each rejection; the current 133 props pass or are fixed

- [ ] HOUSE-00971 — Implement the placement pipeline: a prop row in `layout.props.json` → a chunk entry or a dynamic instance, with per-instance jitter and tint
      dep: HOUSE-00215, HOUSE-00891 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 2.5
      amended: (2026-09-21, `HOUSE-03205`) per-instance tint and scale are the variation mechanism that makes reuse acceptable (rule R8)

- [ ] HOUSE-00973 — Implement the fill-kit generator: `FILL_CUTLERY`, `FILL_TOWELS`, `FILL_TOOLS`, `FILL_TSHIRTS`, `FILL_PAPERS`, `FILL_BOOKS`, `FILL_CROCKERY`, `FILL_TOYS`, `FILL_PAINT`, `FILL_JARS`, `FILL_SHOES`, `FILL_LINEN`
      dep: HOUSE-00971 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 2
      accept: seeded, varied count/spacing/lean/colour; two invocations never look identical
      amended: (2026-09-21, `HOUSE-03201`) fill kits dress open shelves, visible surfaces and the basement, attic and garage storage. Container interiors are out of scope because containers are removed
      amended: (2026-09-21, `HOUSE-03205`) at most six fill kits: books; folded textiles (towels, linen, clothes); crockery and jars; tools and paint tins; toys; shoes. The other six are cancelled; lower clutter density is intended

- [ ] HOUSE-00985 — `prop_kit_gen.py`: generate cabinet carcasses, shelving, boxes, radiators, ducts, pipe runs and wire runs from data
      dep: HOUSE-00971 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 3.5
      amended: (2026-09-21, `HOUSE-03201`) this is the main breadth enabler: generated carcasses, shelving, boxes, radiators, ducts, pipe and wire runs serve the basement, attic, garage and every storage room
      amended: (2026-09-21, `HOUSE-03205`) scope: cabinet carcasses (which also serve as wardrobes, dressers and kitchen/utility modules), shelving, boxes, ducts, and pipe and cable runs from one run generator; radiators only where the house has them

- [ ] HOUSE-02681 — Implement curtains and blinds: meshes, `blindFraction` interaction, the `translucent` portal mode, the transmission cut
      dep: HOUSE-00971, HOUSE-03302 · sys: interaction · plat: ALL · pri: MUST · zone: all · adv: D3 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) static curtains and blinds: meshes and materials only; the portals keep their glass opacity; no `blindFraction` interaction and no `translucent` mode switching
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01196`
      amended: (2026-09-21, `HOUSE-03205`) moved from M6 to the kit: one generated curtain family and one blind family with size and tint variants, placed by the room recipes in M4 and M6; absorbs `HOUSE-02682`
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-00971, HOUSE-03420`

- [ ] HOUSE-00975 — Grouped: acquire and prepare the 24 kitchen appliance and fixture models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) static visual props only, each with a collision proxy and a manifest row; the count is an upper bound (restated once in the M3 preamble by `HOUSE-03205`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`
      amended: (2026-09-21, `HOUSE-03205`) cap 4: the laundry pair, a chest freezer and any mechanical-room unit `HOUSE-00985` does not generate

- [ ] HOUSE-00976 — Grouped: acquire and prepare the 16 bathroom fixture models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 2.5
      amended: (2026-09-21, `HOUSE-03201`) static visual props only, each with a collision proxy and a manifest row; the count is an upper bound (restated once in the M3 preamble by `HOUSE-03205`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`
      amended: (2026-09-21, `HOUSE-03205`) cap 6: toilet, basin with vanity, bath, shower enclosure, towel rail, mirror; reused across all bathrooms and WCs

- [ ] HOUSE-00977 — Grouped: acquire and prepare the 20 seating models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) static visual props only, each with a collision proxy and a manifest row; the count is an upper bound (restated once in the M3 preamble by `HOUSE-03205`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`
      amended: (2026-09-21, `HOUSE-03205`) cap 6: the ground-floor sofas and chairs are reused first (tinted); acquire an armchair, a desk chair, a stool or bench, a cinema seat and at most one more sofa family

- [ ] HOUSE-00978 — Grouped: acquire and prepare the 14 table and desk models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) static visual props only, each with a collision proxy and a manifest row; the count is an upper bound (restated once in the M3 preamble by `HOUSE-03205`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`
      amended: (2026-09-21, `HOUSE-03205`) cap 5: desk, side and coffee tables, a games table; worktops and workbenches come from `HOUSE-00985`

- [ ] HOUSE-00979 — Grouped: acquire and prepare the 12 bed and bedding models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) static visual props only, each with a collision proxy and a manifest row; the count is an upper bound (restated once in the M3 preamble by `HOUSE-03205`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`
      amended: (2026-09-21, `HOUSE-03205`) cap 3 frames (double, single, child's); bedding varies by tint

- [ ] HOUSE-00980 — Grouped: acquire and prepare the 22 storage-furniture models, plus the generated cabinet carcasses and shelving
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) static visual props only, each with a collision proxy and a manifest row; the count is an upper bound (restated once in the M3 preamble by `HOUSE-03205`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`
      amended: (2026-09-21, `HOUSE-03205`) cap 5 (wardrobe, dresser, bookcase, nightstand, chest); everything else is a generated carcass

- [ ] HOUSE-00981 — Grouped: acquire and prepare the 26 small-appliance and lamp models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) static visual props only, each with a collision proxy and a manifest row; the count is an upper bound (restated once in the M3 preamble by `HOUSE-03205`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`
      amended: (2026-09-21, `HOUSE-03205`) cap 6: table, floor and desk lamps, a bare utility fitting, one screen and one projector (static)

- [ ] HOUSE-00982 — Grouped: acquire and prepare the 55 decoration models (rugs, curtains, mirrors, clocks, vases, plants, frames)
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 2.5
      amended: (2026-09-21, `HOUSE-03201`) static visual props only, each with a collision proxy and a manifest row; the count is an upper bound (restated once in the M3 preamble by `HOUSE-03205`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`
      amended: (2026-09-21, `HOUSE-03205`) cap 15, including a small CC0 wall-art set (about 10 images) with frame variants; absorbs `HOUSE-02688`. Rugs may be generated planes with CC0 textures

- [ ] HOUSE-00984 — Grouped: acquire and prepare the 45 box, crate, bin, tool and clutter models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) static visual props only, each with a collision proxy and a manifest row; the count is an upper bound (restated once in the M3 preamble by `HOUSE-03205`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`
      amended: (2026-09-21, `HOUSE-03205`) cap 12 plus generated boxes; absorbs `HOUSE-00983`: pantry and kitchen food items come from the fill kits

---

## M4 — Dressing everywhere (the furnishing half of C3) · checkpoint

Track A. Every accessible room gets its recipe's pieces **for its tier** (rule R8). For secondary
and utility rooms this is their final furnishing. Main rooms get the main-tier set here and are
finished to C4 in M6a; hero areas get the main-tier set here and their C5 pass in M6b. Work the
zones **least complete first** (R2), at most three consecutive tasks per zone (R3). The expected
order today: `Z-L1` → `Z-L2` → `Z-B1` → `Z-L3` → `Z-L0S` → `Z-GAR` → `Z-STAIR` → `Z-EXR` → `Z-STR` →
`Z-EXF`. `Z-L0M` is already dressed and gets **no** M4 task (R4). Lighting is not judged here; it is
M5's, and C3 is complete only at G3.

**Acceptance of every furnish task below** (`HOUSE-03201` repeated it in each task; it is stated
once here, with the tier added): (1) the recipe's pieces for the cell's tier (`HOUSE-03302`) are
present, so the room's purpose reads at a glance; (2) all props are static with collision proxies;
(3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no
S1/S2 defect in the zone's fixed views for these cells; (5) kit first and reuse freely: bespoke
authoring only in hero areas, and only in their C5 task (rule R8). Every task depends on gate G2,
the placement pipeline, the recipes and the validator.

### `Z-L1` — first upper floor

- [ ] HOUSE-00999 — Furnish `L1_LANDING` and `L1_HALL`, `L1_HALL_W`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00982 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`
      amended: (2026-09-21, `HOUSE-03205`) `L1_LANDING` and `L1_HALL` are main (C4 in `HOUSE-03444`); `L1_HALL_W` is secondary

- [ ] HOUSE-01000 — Furnish `L1_MASTER_BED`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00979, HOUSE-00980 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00979`
      amended: (2026-09-21, `HOUSE-03205`) `L1_MASTER_BED` is hero area H4: furnished here with the main-tier set; its C5 pass is `HOUSE-03453`

- [ ] HOUSE-01001 — Furnish `L1_MASTER_BATH` and `L1_MASTER_CLOSET`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00976, HOUSE-00980 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00976`
      amended: (2026-09-21, `HOUSE-03205`) `L1_MASTER_BATH` is main (C4 in `HOUSE-03444`); the closet is secondary

- [ ] HOUSE-01002 — Furnish `L1_BED2` (double, desk, wardrobe)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00978, HOUSE-00979, HOUSE-00980 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00979`

- [ ] HOUSE-01003 — Furnish `L1_BED3` (child's room)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00979, HOUSE-00980, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 1.25
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00979`

- [ ] HOUSE-01004 — Furnish `L1_BED4` (teenager's room, deliberately untidy)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00978, HOUSE-00979 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 1.25
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00979`

- [ ] HOUSE-01005 — Furnish `L1_BED5` (guest, made up, unused)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00979 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00979`

- [ ] HOUSE-01006 — Furnish `L1_BATH2`, `L1_BATH3`, `L1_WC3`, `L1_WC4`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00976 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00976`

- [ ] HOUSE-01007 — Furnish `L1_LINEN`, `L1_STOR`, `L1_CLOSET_2`, `L1_CLOSET_3`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00980 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 0.75
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00973`

- [ ] HOUSE-01008 — Furnish `L1_BALCONY_REAR` and `L1_BALCONY_FRONT`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00771 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 0.75
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00771`

### `Z-L2` — second upper floor

- [ ] HOUSE-01009 — Furnish `L2_LANDING`, `L2_HALL`, `L2_HALL_W`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00982 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G3, D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00999`
      amended: (2026-09-21, `HOUSE-03205`) `L2_LANDING` and `L2_HALL` are main (C4 in `HOUSE-03445`)

- [ ] HOUSE-01010 — Furnish `L2_LIBRARY` (floor-to-ceiling shelves, ladder, reading chairs)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00977, HOUSE-00980 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G3, D3 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00980`
      amended: (2026-09-21, `HOUSE-03205`) `L2_LIBRARY` is hero area H5: furnished here with the main-tier set (shelving and `FILL_BOOKS`); its C5 pass is `HOUSE-03454`

- [ ] HOUSE-01011 — Furnish `L2_GAMES` (pool table, dartboard, arcade cabinet, jigsaw)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00978 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G3, D3 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00977`
      amended: (2026-09-21, `HOUSE-03205`) `L2_GAMES` is main (C4 in `HOUSE-03445`)

- [ ] HOUSE-01012 — Furnish `L2_SITTING` (record player, plants)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00981 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G3, D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00977`
      amended: (2026-09-21, `HOUSE-03205`) `L2_SITTING` is main (C4 in `HOUSE-03445`)

- [ ] HOUSE-01013 — Furnish `L2_BED6` (sewing room) and `L2_BED7`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00978, HOUSE-00979 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G3, D3 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00979`

- [ ] HOUSE-01014 — Furnish `L2_BATH4`, `L2_BATH5`, `L2_WC5`, `L2_WC6`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00976 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G3, D3 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00976`

- [ ] HOUSE-01015 — Furnish `L2_STOR2`, `L2_CLOSET_4`, `L2_LINEN2`, `L2_STAIR_ATTIC`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G3, D3 · est: 0.75
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00973`

### `Z-B1` — basement

- [ ] HOUSE-01020 — Furnish `B1_HALL`, `B1_STAIR`, `B1_UNDERSTAIR`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G3, D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00984`
      amended: (2026-09-21, `HOUSE-03205`) `B1_HALL` is main (C4 in `HOUSE-03441`)

- [ ] HOUSE-01021 — Furnish `B1_MECHANICAL` (furnace, air handler, water heater, expansion tank, water main) from the plumbing/HVAC data
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00985, HOUSE-00386, HOUSE-00387 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G3, D3 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) the equipment is static; the plumbing/HVAC data only positions it
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00386, HOUSE-00387`

- [ ] HOUSE-01022 — Furnish `B1_ELECTRICAL` and `B1_UTILITY`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G3, D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01021`

- [ ] HOUSE-01023 — Furnish `B1_CINEMA` (projector, screen, 6 seats, acoustic panels) and `B1_WC7`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00976, HOUSE-00977 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G3, D3 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) the projector and screen are static; there is no playback
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00977`
      amended: (2026-09-21, `HOUSE-03205`) `B1_CINEMA` is hero area H6: furnished here with the main-tier set; its C5 pass is `HOUSE-03455`

- [ ] HOUSE-01024 — Furnish `B1_GYM` and `B1_WORKSHOP`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G3, D3 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00984`
      amended: (2026-09-21, `HOUSE-03205`) `B1_WORKSHOP` is main (C4 in `HOUSE-03441`)

- [ ] HOUSE-01025 — Furnish `B1_STOR1`, `B1_STOR2`, `B1_CELLAR`, `B1_LAUNDRY2`, `B1_HOBBY`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00980, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G3, D3 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00984`

### `Z-L3` — attic

- [ ] HOUSE-01016 — Furnish `L3_ROOM` (the finished attic: old sofa, desk, boxes, rocking horse, train set)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST · zone: Z-L3 · adv: G3, D3 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00984`
      amended: (2026-09-21, `HOUSE-03205`) `L3_ROOM` is hero area H7: furnished here with the main-tier set; its C5 pass is `HOUSE-03446`

- [ ] HOUSE-01017 — Furnish `L3_STORE_W` (40 boxes, wardrobe, suitcases, insulation, walkway, the spider's web)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST · zone: Z-L3 · adv: G3, D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) the spider's web is optional dressing, not a requirement
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00984`
      amended: (2026-09-21, `HOUSE-03205`) secondary tier: the 40-box count is not a target; enough generated boxes to read as decades of storage

- [ ] HOUSE-01018 — Furnish `L3_STORE_E` (header tank, ducts, aerial mast, cable runs)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L3 · adv: G3, D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00985`

- [ ] HOUSE-01019 — Furnish `L3_STORE_N`, `L3_STORE_S`, `L3_STAIR_HEAD`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST · zone: Z-L3 · adv: G3, D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00984`

### `Z-L0S` — ground-floor service rooms

- [ ] HOUSE-00991 — Furnish `L0_PANTRY` and `L0_BUTLERS`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00975, HOUSE-00980, HOUSE-00973 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0S · adv: G3, D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00990`

- [ ] HOUSE-00994 — Furnish `L0_OFFICE`, `L0_CLOSET_W`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00978, HOUSE-00980 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0S · adv: G3, D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`
      amended: (2026-09-21, `HOUSE-03205`) `L0_OFFICE` is main (C4 in `HOUSE-03442`)

- [ ] HOUSE-00995 — Furnish `L0_MUDROOM`, `L0_LAUNDRY`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00975, HOUSE-00980, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0S · adv: G3, D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`

- [ ] HOUSE-00996 — Furnish `L0_WC1`, `L0_WC2`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00976 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0S · adv: G3, D3 · est: 0.75
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00976`

- [ ] HOUSE-00997 — Furnish `L0_STOR`, `L0_STAIR_MAIN`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0S · adv: G3, D3 · est: 0.75
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`
      amended: (2026-09-21, `HOUSE-03205`) `L0_STAIR_MAIN` is main (C4 in `HOUSE-03447`)

### `Z-GAR` — garage

- [ ] HOUSE-00998 — Furnish `L0_GARAGE` (car, workbench, shelving, tools, bikes, bins, 28 containers, the door mechanism)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-GAR · adv: G3, D3 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) no containers and no door mechanism (both removed). The household car is placed when `HOUSE-01036` lands; the garage reaches C3 without it
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01036, HOUSE-00984`
      amended: (2026-09-21, `HOUSE-03205`) `L0_GARAGE` is main (C4 in `HOUSE-03442`); the household car comes from `HOUSE-00847`'s car family, not from the cancelled `HOUSE-01036`

- [ ] HOUSE-03341 — Furnish `L0_GARAGE_LOFT`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST · zone: Z-GAR · adv: G3, D3 · est: 0.75
      accept: C3 for the loft (no legacy furnish task covered it): long-term storage of boxes, seasonal decorations, spare lumber and a garden-furniture stack from the kit, clear of the access and guard
      verify: `Z-GAR` capture; placement validator; grand tour

### `Z-STAIR`, `Z-EXR`, `Z-STR`, `Z-EXF`

- [ ] HOUSE-03343 — Dress the stair halls and landings
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303 · sys: world · plat: TOOL · pri: MUST · zone: Z-STAIR · adv: G3, D3 · est: 1
      accept: C3 for the six stair cells: stair-wall frames where the recipe places them, a runner where the finish schedule has one, and landing pieces (the `L1` landing window seat); nothing narrows a flight below its walkable width
      verify: `Z-STAIR` capture; stair traversal tests; grand tour
      amended: (2026-09-21, `HOUSE-03205`) the main flights are main (C4 in `HOUSE-03447`); the basement and attic stairs are secondary

- [ ] HOUSE-01026 — Furnish `EXT_SHED` (tools, mower, pots, bags, bench)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST · zone: Z-EXR · adv: G3, D3 · est: 0.75
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)**; the criteria, stated in every M4 task by `HOUSE-03201`, are stated once in the M4 preamble by `HOUSE-03205`
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00984`

- [ ] HOUSE-03342 — Dress the side yards, the orchard corner and the vegetable garden
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03303, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST · zone: Z-EXR · adv: G3, D3 · est: 1.5
      accept: C3 for `EXT_SIDEYARD_W`, `EXT_SIDEYARD_E`, `EXT_ORCHARD` and `EXT_GARDEN`: service-side objects (AC condenser and meters if not placed by `HOUSE-00770`, a firewood store, a hose reel, a wheelbarrow, garden tools, a compost area, a potting bench and a rain barrel) placed with purpose and collision
      verify: `Z-EXR` capture; placement validator; grand tour

- [ ] HOUSE-00770 — Place the exterior props: mailbox, bins ×3, hose reel, AC condenser, gas meter, water tap, downspout splash blocks
      dep: HOUSE-00764, HOUSE-03280, HOUSE-03303 · sys: world · plat: TOOL · pri: MUST · zone: Z-EXF · adv: G3, D3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) all static props with collision proxies; dependency on gate G2 and the placement validator added (breadth rule R1)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00764`

- [ ] HOUSE-00847 — Place the 3 parked neighbour cars and the delivery van
      dep: HOUSE-00293, HOUSE-03280, HOUSE-03303 · sys: world · plat: TOOL · pri: MUST · zone: Z-STR, Z-GAR · adv: G3, D3 · est: 2.5
      amended: (2026-09-21, `HOUSE-03201`) static parked vehicles; any cleanly licensed or generated low-detail car/van is acceptable at street distance
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00293`
      amended: (2026-09-21, `HOUSE-03205`) one car family (an estate and a van), acquired or generated, varied by tint: the household car in the garage (absorbs the cancelled `HOUSE-01035`/`01036`), the 3 parked neighbour cars and the delivery van

### Checkpoint

- [ ] HOUSE-03380 — **Dressing checkpoint: every accessible room has its tier's essential props**
      dep: HOUSE-00991, HOUSE-00994…HOUSE-01026, HOUSE-00770, HOUSE-00847, HOUSE-03341, HOUSE-03342, HOUSE-03343 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G3 · est: 1
      accept: `zone_scoreboard.py` shows zero-prop accessible rooms = 0 (except named empty-by-design spaces); no S1/S2 dressing defect open; each zone's own views captured once by day. This is the point the house-wide lighting work of M5 waits for; it is **not** a gate
      verify: `zone_scoreboard.py`; one capture per zone
      amended: (2026-09-21, `HOUSE-03205`) was *Gate G3 review: every zone at C3*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`

---

## M5 — Baseline lighting everywhere (the lighting half of C3) · gate G3

Track A. Brings every accessible room's lighting up to the C3 floor and ends with **gate G3**:
the whole house is dressed and lit at its baseline. The dressing is in place (checkpoint
`HOUSE-03380`), so the object-versus-bake mismatch is fixed **once, for the whole house**
(`HOUSE-03402`), the house is re-baked with its furniture, and each zone is then checked by day and
at night. The ground-floor route's Round 102 S2 list is closed by `HOUSE-03402` and, last,
`HOUSE-03407`. Sun patches, neighbour porch lights, impostor rendering and the extra light-group
tests are optional or cancelled.

- [ ] HOUSE-03401 — Automatic interior lighting schedule ("an occupied house")
      dep: HOUSE-03201 · sys: lighting · plat: ALL · pri: MUST · zone: all · adv: D6 · est: 2
      files: src/lighting/LightingSystem.cpp|hpp (or a small `LightSchedule`), assets-src/world/layout.lights.json, tests
      accept: (1) a data-driven schedule per light group, keyed to the sun and clock: living spaces on from dusk until a late hour; bedroom lamps in the evening; circulation lights on at night; bathrooms, closets and storage off unless the schedule names them; (2) seeded, deterministic per-group offsets so windows do not switch in unison; (3) `--light-on/--light-off` and `light <group>` (`HOUSE-01272`) still override; (4) the review capture at night uses the schedule, not hand-picked flags; (5) no switch plate, prompt or interaction code
      verify: unit LightScheduleTests.* (times, determinism, override); night zone captures
      note: until now interior lights change only through `--light-on`. A night walkthrough is dark indoors except for dusk-sensor fixtures.
      amended: (2026-09-21, `HOUSE-03205`) accept (3) no longer names `HOUSE-01272` (cancelled): `--light-on`/`--light-off` remain the override; the unit tests also prove one light group per level switches and changes its room's level (absorbs the representative part of `HOUSE-01273`)

- [ ] HOUSE-03402 — Light static props consistently with their baked receivers, house-wide
      dep: HOUSE-03380 · sys: rendering · plat: ALL · pri: MUST · zone: all · adv: D6, G3 · est: 4
      files: src/rendering/*, src/lighting/*, tools/blender/lightmap_bake.py (as chosen)
      accept: (1) choose, by matched captures, between props receiving per-cell irradiance from the bake (probe or sampled lightmap), baked prop lightmaps, or baked vertex occlusion, and record the decision with its numbers; (2) Basic-lit furniture no longer reads markedly darker or warmer than the floor and wall it stands against: measured luminance ratios within a stated band on ≥ 1 view per zone, day and night; (3) the rejected global-gain probes of `HOUSE-01076` stay rejected; outdoor props keep their calibration
      verify: matched before/after captures of ≥ 1 view per zone; the ratio measurement; render goldens refreshed only where inspected
      note: generalises Round 102's top ground-floor defect (piano, sofas, sunroom chairs) to the whole house. With every zone dressed, this is a system fix, not a room fix (R1 exception b).

- [ ] HOUSE-01029 — Rebuild chunks over the furnished house; verify the ≤ 6-chunks-per-cell target still holds
      dep: HOUSE-03380, HOUSE-00473 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: G3 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) runs after gate G3 (every zone dressed), not after full density
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01028, HOUSE-00473`
      amended: (2026-09-21, `HOUSE-03205`) "gate G3" in the line above is the dressing checkpoint `HOUSE-03380` in the numbering of `HOUSE-03205`

- [ ] HOUSE-01030 — Re-bake lightmaps over the furnished house (furniture casts and receives baked light)
      dep: HOUSE-01029, HOUSE-00909 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: G3, D6 · est: 3
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01029, HOUSE-00909`

- [ ] HOUSE-00913 — Fix lightmap seams and gutter bleed found on inspection
      dep: HOUSE-01030 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: G3 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) seams are fixed on the furnished re-bake (`HOUSE-01030`) so the fix is not undone by it
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00912`
      amended: (2026-09-21, `HOUSE-03205`) S1/S2 seams only; smaller seams are S3/S4 (severity table)

- [ ] HOUSE-03403 — Light the basement, the attic and the stairwells to the C3 baseline
      dep: HOUSE-03402, HOUSE-01030, HOUSE-03401 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-B1, Z-L3, Z-STAIR · adv: G3, D6 · est: 2.5
      accept: the lighting half of C3 for `Z-B1`, `Z-L3` and `Z-STAIR`: each room and flight has physical fixtures on its authored groups; windowless rooms are readable under their scheduled lights and not black; the attic reads by dormer daylight plus bare-bulb fixtures; **the main stair foot's S2 darkness (Rounds 98, 102) is closed**
      verify: zone captures day and night (utility rooms by day only); lighting tests
      amended: (2026-09-21, `HOUSE-03205`) was *Light the basement, the attic and the stairwells to C4*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`

- [ ] HOUSE-03404 — Light the two upper floors to the C3 baseline
      dep: HOUSE-03402, HOUSE-01030, HOUSE-03401 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-L1, Z-L2 · adv: G3, D6 · est: 2.5
      accept: the lighting half of C3 for `Z-L1` and `Z-L2`: physical fixtures (ceiling lights, bedside and desk lamps from the kit) on their groups; daylight readable at 10:30 in every room; the evening schedule readable at night
      verify: zone captures day and night
      amended: (2026-09-21, `HOUSE-03205`) was *Light the two upper floors to C4*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`

- [ ] HOUSE-03405 — Light the ground-floor service rooms and the garage to the C3 baseline
      dep: HOUSE-03402, HOUSE-01030, HOUSE-03401 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-L0S, Z-GAR · adv: G3, D6 · est: 1.5
      accept: the lighting half of C3 for `Z-L0S` and `Z-GAR`, including the service room's night depth from Round 102 and the garage's fluorescent/utility character
      verify: zone captures day and night
      amended: (2026-09-21, `HOUSE-03205`) was *Light the ground-floor service rooms and the garage to C4*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`

- [ ] HOUSE-03406 — Light the rear and side exterior at night to the C3 baseline
      dep: HOUSE-03380 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-EXR, Z-EXF · adv: G3, D6 · est: 1.5
      accept: the rear garden, side yards and shed read at night by lanterns, the dusk-sensor fixtures and window spill, without a lifted global exposure; the front is re-checked, not re-worked
      verify: `Z-EXR` and `Z-EXF` night captures
      amended: (2026-09-21, `HOUSE-03205`) was *Light the rear and side exterior at night to C4*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`

- [ ] HOUSE-03407 — Close the ground-floor route's lighting S2 list (C3 baseline)
      dep: HOUSE-03402, HOUSE-01030, HOUSE-03401, HOUSE-03403, HOUSE-03404, HOUSE-03405 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G3 · est: 1.5
      accept: Round 102's ranked S2 list is closed on the fixed route views by day and night under the schedule; no new ground-floor-only lighting constant is introduced unless the house-wide solution fails there, and then it is recorded
      verify: `Z-L0M` captures day and night
      note: deliberately **last** in M5. It is the only permitted `Z-L0M` work before G3 (rule R4).
      amended: (2026-09-21, `HOUSE-03205`) was *Bring the ground-floor principal route to C4*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`

- [ ] HOUSE-01275 — Render tests: 20 rooms, lights on and off, at noon and at midnight
      dep: HOUSE-03403, HOUSE-03404, HOUSE-03405, HOUSE-03406, HOUSE-03407 · sys: ci · plat: CI · pri: MUST · zone: all · adv: G3, D11 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) the 20 rooms span every level (≥ 3 per level)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01266`
      amended: (2026-09-21, `HOUSE-03205`) becomes the **representative interior render set**: one pose per zone plus one per hero area, at 10:30 clear and at 22:00 under the schedule (about 36 scenes); absorbs `HOUSE-00917`, `HOUSE-01032`, `HOUSE-02590` and `HOUSE-02592`. No lights-on/lights-off matrix
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-01266, HOUSE-03380`

- [ ] HOUSE-03420 — **Gate G3 review: every accessible room at C3 (dressed and lit baseline)**
      dep: HOUSE-03402, HOUSE-03403, HOUSE-03404, HOUSE-03405, HOUSE-03406, HOUSE-03407, HOUSE-01030, HOUSE-00913, HOUSE-01275 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G3, D3, D6 · est: 1.5
      accept: one all-zone round, day and night (utility rooms by day only), with no S1/S2 open in any accessible room; `zone_scoreboard.py` shows zero-prop accessible rooms = 0; the scoreboard shows every zone's C3 floor reached; the handoff records the gate
      verify: ledger round; scoreboard; `zone_scoreboard.py`
      amended: (2026-09-21, `HOUSE-03205`) was *Gate G4 review: every zone at C4*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03402, HOUSE-03403, HOUSE-03404, HOUSE-03405, HOUSE-03406, HOUSE-03407, HOUSE-01030, HOUSE-00851, HOUSE-00850`

---

## M6 — Main rooms and hero areas (C4, C5) · gates G4 and G5

Track A, after G3. **M6a** raises the main rooms and main areas to C4 and ends with gate G4. **M6b**
raises the seven hero areas to C5 and ends with gate G5. Nothing is raised above its tier's target.
The ground floor goes last at each stage (rule R4), and the dependencies encode it: its C4 rooms wait
for every other zone's C4 task, and its hero tasks wait for every other hero task. The seven legacy
ground-floor furnishing tasks live here and only here: three as C4 main rooms, four as the C5 passes
of hero areas H1–H3.

### M6a — main rooms to C4 · gate G4

- [ ] HOUSE-03441 — Bring the basement's main rooms to C4
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G4, D4 · est: 1.5
      accept: `B1_HALL` and `B1_WORKSHOP`; C4: the recipe's main-tier set complete (textiles, wall items, shelf contents and curtains where the recipe has them); lighting balanced by day and at night; the rooms' fixed views, day and night, have no S1/S2
      verify: the rooms' fixed views, day and night
      amended: (2026-09-21, `HOUSE-03205`) was *Bring the basement to C5*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`

- [ ] HOUSE-03442 — Bring the office and the garage to C4
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0S, Z-GAR · adv: G4, D4 · est: 2
      accept: `L0_OFFICE` and `L0_GARAGE` (absorbs `HOUSE-03443`); C4: the recipe's main-tier set complete (textiles, wall items, shelf contents and curtains where the recipe has them); lighting balanced by day and at night; the rooms' fixed views, day and night, have no S1/S2
      verify: the rooms' fixed views, day and night
      amended: (2026-09-21, `HOUSE-03205`) was *Bring the ground-floor service rooms to C5*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`

- [ ] HOUSE-03444 — Bring the first upper floor's main rooms to C4
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G4, D4 · est: 1.5
      accept: `L1_LANDING`, `L1_HALL` and `L1_MASTER_BATH`; C4: the recipe's main-tier set complete (textiles, wall items, shelf contents and curtains where the recipe has them); lighting balanced by day and at night; the rooms' fixed views, day and night, have no S1/S2
      verify: the rooms' fixed views, day and night
      amended: (2026-09-21, `HOUSE-03205`) was *Bring the first upper floor to C5*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`

- [ ] HOUSE-03445 — Bring the second upper floor's main rooms to C4
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G4, D4 · est: 2
      accept: `L2_LANDING`, `L2_HALL`, `L2_GAMES` and `L2_SITTING`; C4: the recipe's main-tier set complete (textiles, wall items, shelf contents and curtains where the recipe has them); lighting balanced by day and at night; the rooms' fixed views, day and night, have no S1/S2
      verify: the rooms' fixed views, day and night
      amended: (2026-09-21, `HOUSE-03205`) was *Bring the second upper floor to C5*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`

- [ ] HOUSE-03447 — Bring the main stair to C4
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-STAIR · adv: G4, D4 · est: 1
      accept: `L0_STAIR_MAIN`, `L1_STAIR_MAIN` and `L2_STAIR_MAIN`, seen from every foot and head; C4: the recipe's main-tier set complete (textiles, wall items, shelf contents and curtains where the recipe has them); lighting balanced by day and at night; the rooms' fixed views, day and night, have no S1/S2
      verify: the rooms' fixed views, day and night
      amended: (2026-09-21, `HOUSE-03205`) was *Bring the vertical circulation to C5*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`

- [ ] HOUSE-03448 — Bring the rear terrace and back garden to C4
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-EXR · adv: G4, D4 · est: 1.5
      accept: `EXT_TERRACE` and `EXT_BACKYARD`, including the planting along the fences seen from them; C4: the recipe's main-tier set complete (textiles, wall items, shelf contents and curtains where the recipe has them); lighting balanced by day and at night; the rooms' fixed views, day and night, have no S1/S2
      verify: the rooms' fixed views, day and night
      amended: (2026-09-21, `HOUSE-03205`) was *Bring the rear and side exterior to C5*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`

- [ ] HOUSE-00989 — Furnish `L0_DINING`
      dep: HOUSE-03441, HOUSE-03442, HOUSE-03444, HOUSE-03445, HOUSE-03447, HOUSE-03448 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G4, D4 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C5 (showcase baseline)** for the named cells: primary and secondary dressing, no S1/S2 in the zone's fixed views by day, night and overcast, and the room's representative view within the High budget. These rooms already reached C3 in the Visual Convergence Sprint. **Frozen until G4 (`HOUSE-03420`)** by rule R4
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`
      amended: (2026-09-21, `HOUSE-03205`) acceptance is **C4** (main room), not C5: `L0_DINING` is a main room in the tier table. Rule R4: it runs after every other zone's C4 task, which the dependencies encode
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03420`

- [ ] HOUSE-00992 — Furnish `L0_FAMILY` (television, sectional, dog bed, media unit)
      dep: HOUSE-03441, HOUSE-03442, HOUSE-03444, HOUSE-03445, HOUSE-03447, HOUSE-03448 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G4, D4 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C5 (showcase baseline)** for the named cells: primary and secondary dressing, no S1/S2 in the zone's fixed views by day, night and overcast, and the room's representative view within the High budget. These rooms already reached C3 in the Visual Convergence Sprint. **Frozen until G4 (`HOUSE-03420`)** by rule R4
      amended: (2026-09-21, `HOUSE-03201`) the television is a static screen (off); the dog bed is a static prop
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`
      amended: (2026-09-21, `HOUSE-03205`) acceptance is **C4** (main room), not C5: `L0_FAMILY` is a main room in the tier table. Rule R4: it runs after every other zone's C4 task, which the dependencies encode
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03420`

- [ ] HOUSE-00993 — Furnish `L0_SUNROOM` (breakfast table, wicker, plants, wet bar)
      dep: HOUSE-03441, HOUSE-03442, HOUSE-03444, HOUSE-03445, HOUSE-03447, HOUSE-03448 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G4, D4 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C5 (showcase baseline)** for the named cells: primary and secondary dressing, no S1/S2 in the zone's fixed views by day, night and overcast, and the room's representative view within the High budget. These rooms already reached C3 in the Visual Convergence Sprint. **Frozen until G4 (`HOUSE-03420`)** by rule R4
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`
      amended: (2026-09-21, `HOUSE-03205`) acceptance is **C4** (main room), not C5: `L0_SUNROOM` is a main room in the tier table. Rule R4: it runs after every other zone's C4 task, which the dependencies encode
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03420`

- [ ] HOUSE-03452 — **Gate G4 review: every main room and main area at C4**
      dep: HOUSE-03441, HOUSE-03442, HOUSE-03444, HOUSE-03445, HOUSE-03447, HOUSE-03448, HOUSE-00989, HOUSE-00992, HOUSE-00993 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G4, D4 · est: 1
      accept: one round over the main-tier views, day and night, with no S1/S2 open in any main room or area; the scoreboard's *Main at C4* column is complete in every zone; every hero area is at least C3; the handoff records the gate
      verify: ledger round; scoreboard

### M6b — hero areas to C5 · gate G5

- [ ] HOUSE-03450 — Bring the front approach to C5 (hero area H1)
      dep: HOUSE-03452 · sys: world · plat: TOOL · pri: MUST · zone: Z-EXF · adv: G5, D4 · est: 1.5
      accept: `EXT_WALK`, `EXT_FRONTYARD_W`, `EXT_FRONTYARD_E`, `EXT_DRIVEWAY` and the front elevation (the porch is `HOUSE-00986`). This is a check against C5 plus S1/S2 fixes, not a new campaign on an area that has had about 25 tasks; C5: at most one bespoke hero piece (rule R8, ≤ 3 h, named here when chosen); the full recipe with secondary dressing and signs of habitation; views by day, at night and overcast with no S1/S2; S3 closed where cheap (severity table); the representative view within the High budget
      verify: the hero area's views (day, night, overcast); the performance scenario that covers it
      amended: (2026-09-21, `HOUSE-03205`) was *Bring the front exterior to C5*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03420`

- [ ] HOUSE-03446 — Bring the attic room to C5 (hero area H7)
      dep: HOUSE-03452 · sys: world · plat: TOOL · pri: MUST · zone: Z-L3 · adv: G5, D4 · est: 2.5
      accept: `L3_ROOM`: the finished attic's lived-in dressing (old sofa, desk, rocking horse, train set) under dormer daylight and its evening lights; C5: at most one bespoke hero piece (rule R8, ≤ 3 h, named here when chosen); the full recipe with secondary dressing and signs of habitation; views by day, at night and overcast with no S1/S2; S3 closed where cheap (severity table); the representative view within the High budget
      verify: the hero area's views (day, night, overcast); the performance scenario that covers it
      amended: (2026-09-21, `HOUSE-03205`) was *Bring the attic to C5*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03420`

- [ ] HOUSE-03453 — Bring the master bedroom to C5 (hero area H4)
      dep: HOUSE-03452 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G5, D4 · est: 2.5
      accept: `L1_MASTER_BED`; C5: at most one bespoke hero piece (rule R8, ≤ 3 h, named here when chosen); the full recipe with secondary dressing (bedding, curtains, rug, wall art, bedside lamps) and signs of habitation; views by day, at night and overcast with no S1/S2; S3 closed where cheap (severity table); the representative view within the High budget
      verify: the hero area's views (day, night, overcast); the performance scenario that covers it

- [ ] HOUSE-03454 — Bring the library to C5 (hero area H5)
      dep: HOUSE-03452 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G5, D4 · est: 2.5
      accept: `L2_LIBRARY`: filled shelves (`FILL_BOOKS`), the ladder, reading chairs, a desk and lamps; C5: at most one bespoke hero piece (rule R8, ≤ 3 h, named here when chosen); the full recipe with secondary dressing and signs of habitation; views by day, at night and overcast with no S1/S2; S3 closed where cheap; the representative view within the High budget
      verify: the hero area's views (day, night, overcast); the performance scenario that covers it

- [ ] HOUSE-03455 — Bring the basement cinema to C5 (hero area H6)
      dep: HOUSE-03452 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G5, D4 · est: 2.5
      accept: `B1_CINEMA`: the screen and projector (static, no playback), seating rows, acoustic panels and low step lights, readable in the dark under its scheduled lights; C5: at most one bespoke hero piece (rule R8, ≤ 3 h, named here when chosen); secondary dressing and signs of habitation; views by day and at night with no S1/S2 (it has no windows, so overcast does not apply); S3 closed where cheap; the representative view within the High budget
      verify: the hero area's views (day, night); the performance scenario that covers it

- [ ] HOUSE-00986 — Furnish `L0_FOYER` and `L0_PORCH`
      dep: HOUSE-03450, HOUSE-03446, HOUSE-03453, HOUSE-03454, HOUSE-03455 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G5, D4 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C5 (showcase baseline)** for the named cells: primary and secondary dressing, no S1/S2 in the zone's fixed views by day, night and overcast, and the room's representative view within the High budget. These rooms already reached C3 in the Visual Convergence Sprint. **Frozen until G4 (`HOUSE-03420`)** by rule R4
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00973…HOUSE-00985`
      amended: (2026-09-21, `HOUSE-03205`) `L0_FOYER` (hero area H2) and `L0_PORCH` (part of hero area H1): acceptance is C5 as redefined in [Completion levels](#completion-levels) (hero). Rule R4: the ground floor's hero tasks run after every other hero task, which the dependencies encode
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03420`

- [ ] HOUSE-00987 — Furnish `L0_HALL` (including the gallery wall placement)
      dep: HOUSE-03450, HOUSE-03446, HOUSE-03453, HOUSE-03454, HOUSE-03455 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G5, D4 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C5 (showcase baseline)** for the named cells: primary and secondary dressing, no S1/S2 in the zone's fixed views by day, night and overcast, and the room's representative view within the High budget. These rooms already reached C3 in the Visual Convergence Sprint. **Frozen until G4 (`HOUSE-03420`)** by rule R4
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`
      amended: (2026-09-21, `HOUSE-03205`) `L0_HALL` (hero area H2): acceptance is C5 as redefined in [Completion levels](#completion-levels) (hero). Rule R4: the ground floor's hero tasks run after every other hero task, which the dependencies encode
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03420`

- [ ] HOUSE-00988 — Furnish `L0_LIVING` (fireplace, piano, seating, bay window seat)
      dep: HOUSE-03450, HOUSE-03446, HOUSE-03453, HOUSE-03454, HOUSE-03455 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G5, D4 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C5 (showcase baseline)** for the named cells: primary and secondary dressing, no S1/S2 in the zone's fixed views by day, night and overcast, and the room's representative view within the High budget. These rooms already reached C3 in the Visual Convergence Sprint. **Frozen until G4 (`HOUSE-03420`)** by rule R4
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`
      amended: (2026-09-21, `HOUSE-03205`) `L0_LIVING` (hero area H2): acceptance is C5 as redefined in [Completion levels](#completion-levels) (hero). Rule R4: the ground floor's hero tasks run after every other hero task, which the dependencies encode
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03420`

- [ ] HOUSE-00990 — Furnish `L0_KITCHEN` (the densest room: island, run, appliances, 62 containers)
      dep: HOUSE-03450, HOUSE-03446, HOUSE-03453, HOUSE-03454, HOUSE-03455 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G5, D4 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C5 (showcase baseline)** for the named cells: primary and secondary dressing, no S1/S2 in the zone's fixed views by day, night and overcast, and the room's representative view within the High budget. These rooms already reached C3 in the Visual Convergence Sprint. **Frozen until G4 (`HOUSE-03420`)** by rule R4
      amended: (2026-09-21, `HOUSE-03201`) cabinets are static closed joinery; the 62 containers are removed
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00975`
      amended: (2026-09-21, `HOUSE-03205`) `L0_KITCHEN` (hero area H3): acceptance is C5 as redefined in [Completion levels](#completion-levels) (hero). Rule R4: the ground floor's hero tasks run after every other hero task, which the dependencies encode
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03420`

- [ ] HOUSE-03480 — **Gate G5 review: every hero area at C5**
      dep: HOUSE-03450, HOUSE-03446, HOUSE-03453, HOUSE-03454, HOUSE-03455, HOUSE-00986, HOUSE-00987, HOUSE-00988, HOUSE-00990 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G5, D4 · est: 1.5
      accept: one round over the seven hero areas (day, night, overcast where it applies) with no S1/S2 open anywhere; the scoreboard's *Hero at C5* column is complete; the S3/S4 backlog is handed to M11
      verify: ledger round; scoreboard
      amended: (2026-09-21, `HOUSE-03205`) was *Gate G5 review: every zone at C5 (DONE item D1)*; C5 is now required of the hero areas only, and density and anti-repetition checks are cancelled

---

## M7 — A compact environment

Track B. A compact, visible environment: **clear → overcast → rain** and **day → sunset → night**,
with simple falling snow. The weather state and its archetypes, the seasons, the astronomy, the sky
dome, the clouds, the sun, the moon and the stars already exist (legacy phases 22–26). What remains
is the weather wired into the sky, lighting and fog, and the precipitation particles. Storm and
lightning, hail, snow cover, glare and lens flare, vegetation sway, grass cards, puddles, splashes and
seasonal vegetation are [optional after DONE](#optional-after-done). The storm and hail archetypes
stay in the weather data and render as heavy rain (`HOUSE-01743`). No piece may grow into a
simulation of its own.

- [ ] HOUSE-03520 — Environment review: the DONE environment shown in fixed captures
      dep: HOUSE-01696, HOUSE-01650, HOUSE-01744, HOUSE-01748, HOUSE-01792 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D5 · est: 2.5
      accept: (1) `capture_review.py` gains an environment scenario set: clear, overcast and rain, each at noon, dusk and night, from two views (the front approach and an upper-floor window looking out), plus snowfall at noon from one view; (2) every D5 item is visible in it; (3) sky and fog are tuned where the set shows an S1/S2 (absorbs `HOUSE-01652`, bounded to 1.5 h); (4) S1/S2 environment defects are fixed or filed
      verify: the capture set; a ledger round
      amended: (2026-09-21, `HOUSE-03205`) was *Environment review: every D5 item shown in fixed environment captures*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-01696, HOUSE-01754, HOUSE-01800, HOUSE-01843, HOUSE-01889, HOUSE-01653, HOUSE-00919`

### Sky and fog

- [ ] HOUSE-01696 — Wire the weather into the sky (cloud cover, thunder), lighting (cloud modulation) and fog
      dep: HOUSE-01686, HOUSE-01648, HOUSE-01706 · sys: weather · plat: ALL · pri: MUST · zone: all · adv: D5 · est: 2
      amended: (2026-09-21, `HOUSE-03205`) thunder intensity darkens the clouds and the light only; there is no lightning (storm is optional after DONE)

- [ ] HOUSE-01650 — Implement fog: colour from the horizon in the view direction, start/end from `fogDensity` and precipitation, enabled only for exterior batches
      dep: HOUSE-01649, HOUSE-00892 · sys: rendering · plat: ALL · pri: MUST · zone: all · adv: D5 · est: 2

- [ ] HOUSE-01653 — Render tests: the six sky states at four times of day (24 scenes)
      dep: HOUSE-03520 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D5, D11 · est: 1.5
      amended: (2026-09-21, `HOUSE-03205`) becomes the **environment render set**: clear, overcast and rain at noon, dusk and night from one exterior pose (9 scenes), plus heavy rain from indoors through a window, rain under the porch, the wet driveway and snowfall (4 scenes); absorbs `HOUSE-01754` and `HOUSE-02591`. Six sky states × four times is not required
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-01652`

### Rain and simple snow

- [ ] HOUSE-01741 — Implement the shared particle renderer: pooled camera-facing quads in a `DynamicVertexBuffer`, one draw per material, ≤ 6 materials
      dep: HOUSE-00092 · sys: weather · plat: ALL · pri: MUST · zone: all · adv: D5 · est: 2.5

- [ ] HOUSE-01742 — Implement the camera-relative precipitation volume with wrap-around and the wind offset
      dep: HOUSE-01741 · sys: weather · plat: ALL · pri: MUST · zone: all · adv: D5 · est: 2
      amended: (2026-09-21, `HOUSE-03205`) the wind offset reads the weather state's smoothed wind vector; there is no gust model (`HOUSE-01876` is optional)

- [ ] HOUSE-01743 — Implement rain particle motion: gravity, wind, intensity-driven count, velocity-elongated quads
      dep: HOUSE-01742 · sys: weather · plat: ALL · pri: MUST · zone: all · adv: D5 · est: 1.5
      amended: (2026-09-21, `HOUSE-03205`) every precipitation type except snow (rain, sleet and the hail archetype) draws rain streaks; the thunderstorm archetype is heavy rain under dark cloud, with no lightning (storm and hail are optional after DONE)

- [ ] HOUSE-01744 — Implement the roof/coverage mask test and the teleport-to-top behaviour for sheltered particles
      dep: HOUSE-01743, HOUSE-00777 · sys: weather · plat: ALL · pri: MUST · zone: all · adv: D5 · est: 2
      accept: standing under the porch, rain visibly stops at the porch edge; at an open upstairs window it passes but does not enter
      amended: (2026-09-21, `HOUSE-03205`) windows are closed (ADR-0014): the acceptance is the porch edge and the other covered outdoor areas; no rain particle enters through a window

- [ ] HOUSE-01745 — Author the rain streak texture and material
      dep: HOUSE-01743 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D5 · est: 1

- [ ] HOUSE-01748 — Implement the wet-material swap driven by `surfaceWetness` (Tier S)
      dep: HOUSE-01690, HOUSE-00905 · sys: rendering · plat: ALL · pri: MUST · zone: all · adv: D5 · est: 1.5
      amended: (2026-09-21, `HOUSE-03205`) the 14 wet variants exist (`HOUSE-00905`) and `surfaceWetness` is integrated (`HOUSE-01690`): this is the swap only; puddles are optional

- [ ] HOUSE-01755 — Test: no rain particle is ever drawn below a covered surface
      dep: HOUSE-01744 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D5, D11 · est: 0.75

- [ ] HOUSE-01791 — Implement snow particle motion: slow fall, lateral drift, per-particle flutter, rotation
      dep: HOUSE-01742 · sys: weather · plat: ALL · pri: MUST · zone: all · adv: D5 · est: 1
      amended: (2026-09-21, `HOUSE-03205`) **simple snow**: falling flakes only. No accumulation is rendered; `snowDepth` keeps integrating but nothing draws it (snow cover is optional after DONE)

- [ ] HOUSE-01792 — Author the snowflake texture and material (alpha-blended, occluding)
      dep: HOUSE-01791 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D5 · est: 0.75

---

## M8 — Atmospheric audio essentials

Track B. Lightweight audio that supports the atmosphere: footsteps on **six broad surfaces**, one
**interior tone**, exterior **day and night** beds, **rain** and **wind**, at most two special zone
loops, and volume settings. Everything comes from the imported NOX collection
(`content/Audio/ambience`, `footstep`, `footstep-exterior`) plus offline derivatives, so the
Freesound blocker of `HOUSE-00272` no longer affects DONE. There are no positional sources and no
room-aware routing beyond the sky-exposure gain of the weather layers.

- [ ] HOUSE-01911 — Implement `AudioSystem` proper: category volumes, master, mute, device-loss handling
      dep: HOUSE-00154 · sys: audio · plat: ALL · pri: MUST · zone: all · adv: D7 · est: 1.5
      amended: (2026-09-21, `HOUSE-03205`) categories: master, footsteps, ambience, weather

- [ ] HOUSE-01917 — Implement the sound bank loader from `layout.audio.json` and the manifest
      dep: HOUSE-01911, HOUSE-00279 · sys: audio · plat: ALL · pri: MUST · zone: all · adv: D7 · est: 1

- [ ] HOUSE-01918 — Implement `Bag<T>` round-robin sample selection with a no-repeat-within-4 rule, pitch ±4 % and volume ±10 %
      dep: HOUSE-01917, HOUSE-00027 · sys: audio · plat: ALL · pri: MUST · zone: all · adv: D7 · est: 0.75

- [ ] HOUSE-01919 — Implement `FootstepDirector`: the stride accumulator, surface lookup, per-surface bag, and the stair riser-crossing trigger
      dep: HOUSE-01918, HOUSE-00560 · sys: audio · plat: ALL · pri: MUST · zone: all · adv: D7 · est: 2
      accept: cadence matches speed; stairs give one step per riser

- [ ] HOUSE-01920 — Wire the 20 footstep surface sets (12 from NOX, 8 sourced)
      dep: HOUSE-01919, HOUSE-00280 · sys: audio · plat: TOOL · pri: MUST · zone: all · adv: D7 · est: 1.5
      amended: (2026-09-21, `HOUSE-03205`) **six broad categories**, not 20 sets: wood (hardwood and wooden stairs, NOX `wood`), carpet (derived offline from NOX samples by low-pass and level and manifested as a project-owned derivative; the softest NOX pack if the derivative fails the listening check), tile, stone and concrete (NOX `rock`), gravel, grass. Every §62.4 surface maps to one of them. No new sourcing
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-01919, HOUSE-00280, HOUSE-00281`

- [ ] HOUSE-01922 — Implement `AmbienceDirector`: per-cell room tone with an 0.8 s cross-fade on cell change
      dep: HOUSE-01917, HOUSE-00559 · sys: audio · plat: ALL · pri: MUST · zone: all · adv: D7 · est: 2
      amended: (2026-09-21, `HOUSE-03205`) the director selects the **interior** bed or the **exterior** bed from the listener cell and cross-fades on an indoor↔outdoor change; one interior room tone (generated offline or from NOX; absorbs `HOUSE-01923`), plus at most two special zone loops (for example the basement's mechanical hum and attic wind). No per-cell room tone

- [ ] HOUSE-01924 — Implement the exterior ambience bed with time-of-day variation (dawn chorus, day traffic, evening crickets, night quiet)
      dep: HOUSE-01922 · sys: audio · plat: ALL · pri: MUST · zone: all · adv: D7 · est: 1
      amended: (2026-09-21, `HOUSE-03205`) two beds from NOX, day (`forest-birds`) and night (`night`, `cicadas`), cross-faded by sun elevation; no traffic, crickets or dawn-chorus matrix
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-01922, HOUSE-00290`

- [ ] HOUSE-01925 — Implement the seasonal and weather layers over the exterior bed
      dep: HOUSE-01924 · sys: audio · plat: ALL · pri: MUST · zone: all · adv: D7 · est: 1.5
      amended: (2026-09-21, `HOUSE-03205`) weather layers only: rain (NOX `rain-calm`/`rain-strong` by intensity) and wind (`wind-calm`/`wind-forest` by wind speed, with a low-pass cross-fade); absorbs `HOUSE-01750` and `HOUSE-01883`. No seasonal layer

- [ ] HOUSE-02000 — Implement sky-exposure-driven ambience routing for the weather layers
      dep: HOUSE-00779, HOUSE-01925 · sys: audio · plat: ALL · pri: MUST · zone: all · adv: D7 · est: 1
      accept: `L3_STORE_W` gets full rain-on-roof; `B1_CINEMA` gets essentially nothing; an open slider in the sunroom nearly matches outdoors
      amended: (2026-09-21, `HOUSE-03201`) this is the retained piece of room-aware audio: the weather beds follow the listener cell's sky exposure
      amended: (2026-09-21, `HOUSE-03205`) indoors the weather layers are attenuated and low-passed by the listener cell's sky exposure; there is no per-window or per-roof layer. The sunroom's check becomes: with its large glazed area it is clearly louder than an interior room (its sliders stay closed)

- [ ] HOUSE-01938 — Test: footstep cadence matches speed within 5 % across all speeds and surfaces
      dep: HOUSE-01919 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D7, D11 · est: 0.75

- [ ] HOUSE-01939 — Listening review: walk the house and list every sound that is wrong, missing or too loud; fix
      dep: HOUSE-01920, HOUSE-01925, HOUSE-02000 · sys: audio · plat: LNX · pri: MUST · zone: all · adv: D7 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) walk every zone, not only the ground floor
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01931`
      amended: (2026-09-21, `HOUSE-03205`) one walk of each zone; fix what is wrong, missing or too loud; no per-room or per-weather mixing matrix
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-01927, HOUSE-01925, HOUSE-01920`

---

## M9 — Application shell: what a polished demo needs

Track B. *Start*, pause, one settings screen, credits, quit, a controls hint, and layouts that work
at desktop and phone aspects. Settings persistence already exists (`HOUSE-00131`, `HOUSE-00152`).
The existing debug overlays (`F1`–`F5`, `F8`, `F9`) and the console stay as they are; none is
extended.

- [ ] HOUSE-02516 — Implement the settings menu shell with the 5 tabs and keyboard-only navigation
      dep: HOUSE-00156, HOUSE-00131 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D8 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) the five tabs are Display, Graphics, Audio, Controls and Environment
      amended: (2026-09-21, `HOUSE-03205`) **one settings screen** with four sections (Graphics, Audio, Controls, Environment), navigable by keyboard, mouse and touch; no tabs, no second page. Keyboard reachability of every control is part of this acceptance (absorbs `HOUSE-02529`)

- [ ] HOUSE-02518 — Implement the Graphics tab, with options filtered by the project-owned **effective feature set** (`cna-house.md` §68) — `RenderTier` + build/platform profile + validated standard-XNA behaviour, all of it known to the application already
      dep: HOUSE-02516, HOUSE-00160 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D8 · est: 1.5
      accept: no meaningless toggle is ever shown; a build whose profile cannot draw shadow maps does not offer them; the tab calls `SupportsCapability()` or another CNA extension query nowhere, and `check_xna_only.py` passes on the UI sources
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02517`'s display rows: resolution (or canvas size on the Web), fullscreen, v-sync and field of view, plus the quality preset (High, Web, Android)

- [ ] HOUSE-02519 — Implement the Audio tab with live application
      dep: HOUSE-02516, HOUSE-01911 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D8 · est: 0.75
      amended: (2026-09-21, `HOUSE-03205`) master, footsteps, ambience and weather volumes; absorbs `HOUSE-01935`
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02516, HOUSE-01935`

- [ ] HOUSE-02520 — Implement the Controls tab including full key remapping through `IInputSource`
      dep: HOUSE-02516, HOUSE-00140 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D8 · est: 0.75
      amended: (2026-09-21, `HOUSE-03201`) sensitivity, invert-Y, smoothing and FOV are MUST; full key remapping is SHOULD
      amended: (2026-09-21, `HOUSE-03205`) look sensitivity, invert-Y and the walk-speed toggle behaviour only. Key remapping is cut, not deferred

- [ ] HOUSE-02521 — Implement the Simulation tab (day length, weather mode, moon speed, location, pets, pause-on-menu, slots)
      dep: HOUSE-02516 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D5, D8 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) becomes the **Environment** tab: day length, weather mode and fixed archetype, time-of-day and season jump, location. No pets and no save slots
      amended: (2026-09-21, `HOUSE-03205`) the Environment section: time of day (automatic, or a fixed hour), time speed, and weather (automatic, clear, overcast, rain, snow). No location, no season jump, no archetype list

- [ ] HOUSE-02523 — Implement the main menu, the pause menu and the credits screen (showing `THIRD-PARTY-ASSETS.md`)
      dep: HOUSE-02516, HOUSE-00198 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D8 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) main menu: *Start on the street* · *Continue* (only when a session file exists, `HOUSE-03571`) · *Settings* · *Credits* · *Quit*
      amended: (2026-09-21, `HOUSE-03205`) main menu: *Start* · *Settings* · *Credits* · *Quit*. There is no *Continue*: session resume is optional after DONE (`HOUSE-03571`). Pause menu: *Resume* · *Settings* · *Main menu* · *Quit*

- [ ] HOUSE-02525 — Implement the first-run hint line and its 12 s fade
      dep: HOUSE-02523 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D8 · est: 0.75
      amended: (2026-09-21, `HOUSE-03205`) the hint names the controls of the active input scheme (keyboard and mouse, or touch)

- [ ] HOUSE-02527 — Implement UI virtual-unit layout with safe-area insets, and prove it at 4:3, 16:9, 21:9 and 18:9
      dep: HOUSE-02523, HOUSE-00145 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D8, D10 · est: 1.5
      amended: (2026-09-21, `HOUSE-03205`) proved at 16:9, 4:3 and 20:9 (the phone aspect of M15) with safe-area insets; absorbs `HOUSE-02956`

- [ ] HOUSE-02528 — Test: every settings value round-trips, clamps and applies live
      dep: HOUSE-02516, HOUSE-02518, HOUSE-02519, HOUSE-02520, HOUSE-02521 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D8, D11 · est: 1
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02522`

- [ ] HOUSE-02530 — Render tests: every menu and overlay at three aspect ratios
      dep: HOUSE-02527 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D8, D11 · est: 0.75
      amended: (2026-09-21, `HOUSE-03205`) the main menu, the pause menu and the settings screen at 16:9 and 20:9, plus the forced-touch HUD (`HOUSE-02997`) at 20:9: about 7 scenes

---

## M10 — Performance (measurement-driven)

Track B. Optimise **only what measurement shows matters**, against the documented targets, with no
headroom margin. A task whose measurement comes back green closes with the numbers and no code.
The legacy LOD, culling, render-list and streaming blocks are cancelled: `HOUSE-02404` names their
techniques as candidates, planned as new tasks only when a measurement chooses one.

- [ ] HOUSE-02402 — Implement the 10 performance scenarios as a runnable harness
      dep: HOUSE-00150, HOUSE-03202 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D9 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) the scenarios include ≥ 1 representative view per zone (the `HOUSE-03202` sets) plus the worst exterior and stairwell views, not only ground-floor views
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00150`
      amended: (2026-09-21, `HOUSE-03205`) **8 representative scenarios**, not one per zone: three interiors (the kitchen, an upper-floor hero room, the main stair looking up), three exteriors (the street approach towards the house and the neighbourhood, the rear garden, an upper-floor window looking out), heavy rain outside, and night outside

- [ ] HOUSE-02403 — Measure all 10 scenarios and record the baseline in `docs/performance-log.md`
      dep: HOUSE-02402, HOUSE-03380 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D9 · est: 1.5
      amended: (2026-09-21, `HOUSE-03205`) measured over the furnished house (after the dressing checkpoint): draw calls, triangles, CPU and GPU milliseconds, texture and lightmap memory, RSS. Absorbs `HOUSE-01031`, `HOUSE-00853`, `HOUSE-01276`, `HOUSE-01654` and `HOUSE-01753`
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02402`

- [ ] HOUSE-02404 — Optimise against whichever scenario is furthest from budget; repeat until all pass
      dep: HOUSE-02403 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D9 · est: 5
      accept: every budget in `cna-house.md` §71.2 met at the *High* tier
      amended: (2026-09-21, `HOUSE-03205`) optimise only scenarios that miss the documented target; stop as soon as all meet it; **no headroom margin**. The candidate techniques are chosen by the measured bottleneck and planned as new M10 tasks (rule R7) only when chosen: per-category LOD (the approach of `HOUSE-02392`–`02394`), detail-set culling (`HOUSE-00974`/`02396`), chunk sub-range culling (`HOUSE-02397`), render-list and state work (`HOUSE-02398`/`02399`), instancing (`HOUSE-02400`), impostors (`HOUSE-00851`/`02395`)

- [ ] HOUSE-02405 — Implement the quality-tier presets and verify each reaches its own budget
      dep: HOUSE-02404, HOUSE-00157 · sys: rendering · plat: ALL · pri: MUST · zone: all · adv: D9, D10 · est: 2
      amended: (2026-09-21, `HOUSE-03205`) three presets, High (desktop), Web and Android, each verified on desktop with `--quality`; absorbs `HOUSE-02406`, `HOUSE-02407`, `HOUSE-02852` and `HOUSE-02957`

- [ ] HOUSE-00915 — Measure texture memory against the 300 MB budget; adjust sizes where over
      dep: HOUSE-00203, HOUSE-01030 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D9 · est: 1.5
      amended: (2026-09-21, `HOUSE-03205`) texture and lightmap memory together; absorbs `HOUSE-00911` (lightmap texel density changes only if the lightmaps exceed their 60 MB budget)
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-00914, HOUSE-00203`

- [ ] HOUSE-02451 — **Gate: measure cold-start load time and total resident footprint with everything loaded**
      dep: HOUSE-02403 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D9 · est: 1
      accept: record the numbers. If cold start ≤ 4.0 s **and** GPU ≤ 550 MB **and** RSS ≤ 1.6 GB, mark HOUSE-02452…02470 as *not required* with the measurement as the justification, and skip to phase 43.
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02463`'s close-out. If a limit is exceeded, a Planning corrections entry plans the minimum residency the numbers demand, under R9 and with new ids; the legacy streaming block (`HOUSE-02452`–`02462`) is cancelled

---

## M11 — Final defect pass (bounded) · after G5 only

**No M11 task starts before `HOUSE-03480` (G5).** One whole-property review, one bounded fix pass
and one golden refresh. This is where rule R12's budget lives: 8 agent-hours of S2/S3 work plus
whatever S1 fixes need (about 11 h for the milestone; never more than 20 h unless S1/S2 remain).
There is no zone rotation, no "polish everything" pass and no screenshot → tweak → screenshot loop.

- [ ] HOUSE-02714 — Phase-45 review: a full walkthrough with fresh eyes; list everything that still reads as fake; fix or schedule
      dep: HOUSE-03480, HOUSE-01939, HOUSE-03520 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D4, D13 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) this is the whole-property walkthrough with every zone at C5 or better, not a ground-floor route
      amended: (2026-09-21, `HOUSE-03205`) runs **first** in M11 (it used to follow the re-render), with every area at its tier's target rather than every zone at C5: the grand-tour route walked once by day and once at night in first person, plus each hero area's views; every finding classified S1–S4 by the severity table and filed under its zone. Absorbs `HOUSE-01033` (the furnishing review)
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02713`

- [ ] HOUSE-03631 — Final defect pass (bounded)
      dep: HOUSE-02714 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D4, D13 · est: 8
      accept: (1) every S1 and S2 from `HOUSE-02714` and the zone backlogs is fixed; (2) S3 only where the severity table allows it (cheap, repeatedly visible, in a hero area or in a release capture); (3) S4 is not scheduled; (4) **hard budget: 8 agent-hours of S2/S3 work**, plus whatever S1 fixes need; when the budget is spent, the remaining S3 items are waived with a reason; (5) no area gets more than two fix rounds (rule R6)
      verify: one ledger round over the fixed items
      note: split into ≤ 4 h slices when scheduled (R7). This replaces the zone polish rotation and the eleven legacy polish passes.
      amended: (2026-09-21, `HOUSE-03205`) was *Work the polish backlog in zone rotation*; re-scoped by the second reduction (ADR-0015). The earlier text is at `5927073`
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03480`

- [ ] HOUSE-02713 — Grouped: re-render the whole render-test corpus after polish and accept the new references
      dep: HOUSE-03631 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D11 · est: 1
      amended: (2026-09-21, `HOUSE-03205`) only the representative sets whose images changed on purpose
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02701`

---

## M12 — Representative tests

Track B. Keep the regression net aligned with the reduced scope. The existing suites and gates stay.
Tests for removed or finished systems are not gaps, and new tests are representative, not
exhaustive. The representative render sets live with their features: `HOUSE-01275` (interiors, M5),
`HOUSE-01653` (environment, M7) and `HOUSE-02530` (UI, M9). The grand tour is `HOUSE-03226` (M1).

- [ ] HOUSE-00493 — Two gates failed once each under load and did not reproduce
      dep: HOUSE-00483 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D11 · est: 1
      note: (2026-09-09) **New task, next free id in phase 6's reserved 00451–00540 range.**
            Recorded from `HOUSE-00779`'s verification, where two tests failed once each on a
            machine running about eight agents:
            `AudioGateTests.NoAudioRunsAFullSessionAndNeverOpensTheDevice` (integration) and
            `BlockoutPoseRenderTests.TheTwelveInteriorPosesMatchTheirReferences` (render).
      note: **neither reproduces on demand.** The audio gate passed 10 targeted runs, 3 full
            integration runs and in isolation. The interior poses then failed **2 of 5** runs in
            one burst and passed 8 consecutive runs afterwards, with no code change between --
            which is what says it is load and not content: the failing burst ran while a Blender
            shell regeneration and another session's build were both on the machine.
      note: (2026-09-09, third sighting) `BlockoutPoseRenderTests.TheTwelveInteriorPosesMatchTheirReferences`
            failed once more during `HOUSE-00781`'s verification, in a full-suite run, and passed
            alone immediately afterwards and in the next full run. Three sightings now, all in
            full-suite runs on a loaded machine, none reproducible alone.
      note: (2026-09-10, fourth sighting) `BlockoutPoseRenderTests.TheEightExteriorPosesMatchTheirReferences`
            -- the EXTERIOR eight this time, not the interior twelve -- failed once during
            `HOUSE-00846`'s verification, in a full-suite run started the moment a `-j3` build and
            a `ctest -j2` had finished, and passed in the targeted run immediately after and in
            three consecutive full runs. Four sightings, two poses, all under load, none
            reproducible alone.
      note: neither failure's output was captured, which is the first thing to fix: `ctest`
            needs `--output-on-failure` in the wrapper so a sighting is not lost, and the pose
            comparison should say WHICH pose and by how many pixels. §46's own words apply --
            *"a flaky render test is worse than none, because it teaches people to ignore it"*.
      amended: (2026-09-21, `HOUSE-03205`) accept: `ctest` runs with `--output-on-failure` in the wrapper, and the pose comparison names the pose and the differing pixel count, so the next sighting is diagnosable. A flaky gate undermines D11
      amended: (2026-09-21, `HOUSE-03205`) priority was SHOULD

- [ ] HOUSE-02594 — Implement the render-test tolerance policy (per-pixel and mean-absolute-difference budgets) and the difference-image artefact
      dep: HOUSE-00483 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D11 · est: 1
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02590`

- [ ] HOUSE-02598 — Implement the soak test: 2 hours unattended at 60 FPS, RSS growth < 20 MB/hour, no crash, no audio starvation, 200 autosaves
      dep: HOUSE-03226 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D11 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) the 200 autosaves become ≥ 200 settings/session-file writes
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-02589`
      amended: (2026-09-21, `HOUSE-03205`) **a 20-minute automated stability run**, not 2 hours: the grand tour looped plus the seeded random walk, clock at 60× and weather cycling clear → overcast → rain; RSS growth below 20 MB/hour extrapolated, no crash, no audio starvation, ≥ 200 settings writes. Runnable locally and in CI. The single 2-hour run is `HOUSE-02784`

- [ ] HOUSE-02600 — Write `docs/testing.md`: what each suite covers, how to run it, how to add a case, how to accept a render-test change
      dep: HOUSE-02594, HOUSE-02598, HOUSE-01275 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D11, D12 · est: 1
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02599`

---

## M13 — Linux desktop release

After M11. The DONE audit and the packaged desktop build.

- [ ] HOUSE-02781 — Work through the §79 feature-complete checklist item by item; open a task for every gap
      dep: HOUSE-02713 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D1–D13 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) the checklist is `plan.md`'s **Definition of DONE** (desktop items D1–D9, D11–D13), not the legacy §79 list
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02793`: the day-length decision (Q-11) is recorded in this audit
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02714`

- [ ] HOUSE-02782 — Fix every gap found by HOUSE-02781
      dep: HOUSE-02781 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D1–D13 · est: 3

- [ ] HOUSE-02783 — Run the full suite under ASAN and UBSAN; fix every report
      dep: HOUSE-02782, HOUSE-00137 · sys: — · plat: CI · pri: MUST · zone: all · adv: D11 · est: 1.5
      amended: (2026-09-21, `HOUSE-03205`) in the existing `build-asan/` and `build-ubsan/` directories only

- [ ] HOUSE-02784 — Run the 2-hour soak test; fix every leak and every drift
      dep: HOUSE-02598, HOUSE-02782 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D11 · est: 2
      amended: (2026-09-21, `HOUSE-03205`) the **one** 2-hour run before DONE; it is not repeated at other milestones
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02598`

- [ ] HOUSE-02786 — Verify the "zero TODO/TBD/FIXME in shipping paths" rule
      dep: HOUSE-02782 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D13 · est: 0.5

- [ ] HOUSE-02788 — Verify every asset is manifested and licensed, and regenerate `THIRD-PARTY-ASSETS.md`
      dep: HOUSE-00299 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D12 · est: 0.75

- [ ] HOUSE-02789 — Package: a distributable Linux build with its content, licences and a launch script
      dep: HOUSE-02788 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D10 · est: 2
      amended: (2026-09-21, `HOUSE-03205`) the package is a Release build with `CNAHOUSE_DEBUG_TOOLS=OFF`, which proves the debug tools compile out (absorbs `HOUSE-02515`)

- [ ] HOUSE-02790 — Verify the packaged build runs on a clean machine profile (no dev environment, no sibling checkouts)
      dep: HOUSE-02789 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D10 · est: 1.25
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02792`: the clean-profile run uses the High preset at 1920×1080 and 2560×1080 and the Web and Android presets at 1920×1080

- [ ] HOUSE-02791 — Verify the packaged build with `CNA_ENABLE_VIDEO=OFF` and with no audio device
      dep: HOUSE-02790 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D10 · est: 0.75
      amended: (2026-09-21, `HOUSE-03201`) the game no longer uses video; `CNA_ENABLE_VIDEO=OFF` remains a build variant to prove harmless
      amended: (2026-09-21, `HOUSE-03205`) no audio device only; the `CNA_ENABLE_VIDEO=OFF` variant is dropped because the showcase plays no video

- [ ] HOUSE-02797 — Tag the feature-complete desktop release
      dep: HOUSE-02783, HOUSE-02784, HOUSE-02786, HOUSE-02790, HOUSE-02791 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D10 · est: 0.5
      amended: (2026-09-21, `HOUSE-03205`) the notes are the changelog since the last tag; the full release notes are `HOUSE-03077` (absorbs `HOUSE-02796`)
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02796`

---

## M14 — Web

**Web stays a target platform** (D10). The bring-up block may run as Track B any time after G1: it
de-risks the toolchain and exposes Web-only content limits early. The verification block follows
M13. Validation breadth is deliberately small: Chrome and Firefox, one representative traversal,
acceptable performance, and a headless smoke test in CI. Context-loss recovery, persistence across a
page reload and threads are optional after DONE; losing the WebGL context is a known limitation
(reload the page), recorded by `HOUSE-02904`.

### Bring-up (Track B, any time after G1)

- [ ] HOUSE-02843 — Audit and fix any custom loop or `Game::Run` misuse
      dep: HOUSE-00127 · sys: app · plat: ALL · pri: MUST · zone: all · adv: D10 · est: 0.75

- [ ] HOUSE-02855 — Spike: an Emscripten build of a minimal scene (one room, one prop, one sound) to validate the toolchain and the Asyncify interaction
      dep: HOUSE-02843 · sys: app · plat: WEB · pri: MUST · zone: all · adv: D10 · est: 2
      accept: it runs in Chrome; this is the gate for phase 48

- [ ] HOUSE-02842 — Audit and fix every filesystem access outside `DesktopSaveStore`
      dep: HOUSE-02841 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D10 · est: 0.75

- [ ] HOUSE-02848 — Verify every runtime texture is `SurfaceFormat::Color` with pre-generated mips (lint over the material data)
      dep: HOUSE-00110 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D10 · est: 0.75
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02849`: the same lint rejects MRT, stencil and geometry or tessellation stages

- [ ] HOUSE-02891 — CMake: the Emscripten preset with `WEBGL2`, the exception ABI, Asyncify and the pack preloads
      dep: HOUSE-02855 · sys: app · plat: WEB · pri: MUST · zone: all · adv: D10 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) priority SHOULD → MUST for every M14 task except `HOUSE-02902` (DONE item D10 requires the Web build)

- [ ] HOUSE-02892 — Build the full game for Emscripten and fix every compile and link error
      dep: HOUSE-02891 · sys: — · plat: WEB · pri: MUST · zone: all · adv: D10 · est: 5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03721 — Web controls: pointer-lock mouse look, keyboard walk, focus handling
      dep: HOUSE-02892 · sys: player · plat: WEB · pri: MUST · zone: all · adv: D10 · est: 1.5
      accept: (1) a click on the canvas acquires pointer lock and mouse look works without drift; (2) Esc and focus loss release the pointer and open the pause menu; (3) WASD/arrows and the walk-speed modifier work; (4) verified in Chrome and Firefox; (5) on a touch-only browser the `TouchSource` scheme of M15 is used if `TouchPanel` reports touch under Emscripten, and otherwise the limitation is recorded
      verify: manual browser check recorded in `docs/portability.md`; headless smoke (`HOUSE-02901`)

### Verification (after M13)

- [ ] HOUSE-02850 — Verify every pack is ≤ 60 MB and the Web-tier total is ≤ 180 MB compressed
      dep: HOUSE-00203, HOUSE-02405 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D10 · est: 0.5
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-00203, HOUSE-02406`

- [ ] HOUSE-02851 — Reduce content where HOUSE-02850 fails: Web-tier texture sizes, audio bitrates, LOD-only packs
      dep: HOUSE-02850 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D10 · est: 2

- [ ] HOUSE-02895 — Wire the canvas-as-display model into the Display settings
      dep: HOUSE-02892 · sys: ui · plat: WEB · pri: MUST · zone: all · adv: D10 · est: 0.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) canvas resize and the fullscreen toggle

- [ ] HOUSE-02896 — Verify the audio gesture gate in a real browser
      dep: HOUSE-02892 · sys: audio · plat: WEB · pri: MUST · zone: all · adv: D10 · est: 0.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02846` (the desktop half of the gesture gate)

- [ ] HOUSE-02898 — Measure the Web build against its budget; reduce content until it fits
      dep: HOUSE-02892 · sys: — · plat: WEB · pri: MUST · zone: all · adv: D9, D10 · est: 2.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) "acceptable performance": the Web preset's representative scenes (the street approach, one interior, the rear garden) in Chrome on the reference machine meet the documented Web target

- [ ] HOUSE-02899 — Implement the browser loading screen and the progressive pack fetch
      dep: HOUSE-02892, HOUSE-00156 · sys: ui · plat: WEB · pri: MUST · zone: all · adv: D10 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02854`: progress comes from the pack preload and fetch
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02854, HOUSE-02892`

- [ ] HOUSE-02900 — Verify the whole feature set in a browser: every phase's headline feature, tested by hand against a checklist
      dep: HOUSE-02898, HOUSE-02899, HOUSE-02896, HOUSE-02895, HOUSE-03721, HOUSE-02851, HOUSE-02797 · sys: — · plat: WEB · pri: MUST · zone: all · adv: D10 · est: 2
      amended: (2026-09-21, `HOUSE-03201`) checked against the DONE checklist's Web items (the showcase walkthrough), not the legacy feature set
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) the **Web DONE checklist**, in Chrome and in Firefox (absorbs `HOUSE-02903`): the build loads; the controls work (`HOUSE-03721`); a representative traversal (street → foyer → main stair → an upper floor → the garden) completes; no major rendering corruption; acceptable performance; saving settings does not fail (persistence across a reload is optional). Differences between the browsers are recorded
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02898`

- [ ] HOUSE-02901 — Implement a headless-Chrome smoke test in CI (menu, load, 300 frames, one interaction, one save)
      dep: HOUSE-02900 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D10 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) menu, load, 300 frames of the walk, one settings save; there is no interaction
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02904 — Package and document the Web build
      dep: HOUSE-02900 · sys: — · plat: WEB · pri: MUST · zone: all · adv: D10, D12 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02856`: the readiness matrix and the known limitations go to `docs/portability.md`
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02903`

- [ ] HOUSE-02905 — Phase-48 review and commit
      dep: HOUSE-03721, HOUSE-02843, HOUSE-02855, HOUSE-02842, HOUSE-02848, HOUSE-02850, HOUSE-02851, HOUSE-02891, HOUSE-02892, HOUSE-02895, HOUSE-02896, HOUSE-02898, HOUSE-02899, HOUSE-02900, HOUSE-02901, HOUSE-02904 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D10 · est: 0.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02891…HOUSE-02904`

---

## M15 — Android

**Android stays a target platform** (D10). The desktop-side readiness block waits only for G1. The
device path is gated on CNA's Android graphics path (BL-13, upstream; `cna-house` never modifies
CNA), and `HOUSE-02951` re-verifies that whenever it is picked. The touch scheme is a floating
movement stick, a look region, a walk-speed toggle and a menu button. There is no interact button
and no camera toggle. Validation is one representative device, or the best available emulator,
not a compatibility lab.

### Desktop-side readiness (Track B, any time after G1)

- [ ] HOUSE-02954 — Verify the `IInputSource` abstraction is complete (no direct `Keyboard`/`Mouse` reads anywhere)
      dep: HOUSE-00140 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D10 · est: 0.5
      amended: (2026-09-21, `HOUSE-03201`) priority SHOULD → MUST for every M15 task (DONE item D10); the device tasks remain gated on `HOUSE-02951`–`HOUSE-02953`, and a verified upstream block is handled as a recorded blocker, never a tick

- [ ] HOUSE-02991 — Implement `TouchSource` over `TouchPanel`, with multi-touch tracking and gesture recognition
      dep: HOUSE-00101, HOUSE-00140 · sys: player · plat: AND · pri: MUST · zone: all · adv: D8, D10 · est: 2.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) gestures limited to the stick, the look drag and taps

- [ ] HOUSE-02992 — Implement the floating movement stick (bottom-left, 180 vu, analogue direction and magnitude)
      dep: HOUSE-02991 · sys: ui · plat: AND · pri: MUST · zone: all · adv: D8, D10 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) tuned on desktop with `--force-touch` (absorbs the desktop half of `HOUSE-03001`)

- [ ] HOUSE-02993 — Implement the look region (right half minus buttons) with its own sensitivity setting
      dep: HOUSE-02991 · sys: ui · plat: AND · pri: MUST · zone: all · adv: D10 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02995 — Implement the walk-mode, camera and menu buttons
      dep: HOUSE-02992 · sys: ui · plat: AND · pri: MUST · zone: all · adv: D10 · est: 0.75
      amended: (2026-09-21, `HOUSE-03201`) walk-speed toggle and menu buttons only (the camera toggle is removed: first person only)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-02994`; priority was SHOULD

- [ ] HOUSE-02996 — Implement the touch-HUD visibility rule (`hasTouch && !hasKeyboard`) so desktop never shows it
      dep: HOUSE-02995, HOUSE-00141 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D10 · est: 0.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02997 — Implement the `--force-touch` desktop flag so the touch HUD is testable and screenshot-able on Linux
      dep: HOUSE-02996 · sys: debug · plat: LNX · pri: MUST · zone: all · adv: D10 · est: 0.75
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02955` and `HOUSE-03000`: the forced-touch mode proves that the `Platform` struct drives the HUD, and its 20:9 screenshot joins `HOUSE-02530`'s set

- [ ] HOUSE-02998 — Implement touch-friendly menu hit targets (≥ 88 vu) without changing the desktop layout
      dep: HOUSE-02996 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D10 · est: 0.75
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02958 — Verify the content packs fit the APK + OBB budget
      dep: HOUSE-00203, HOUSE-02405 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D10 · est: 0.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-00203, HOUSE-02957`

### Device path (gated on BL-13)

- [ ] HOUSE-02951 — **Gate: confirm CNA's Android cross-compile succeeds** — `sharp-runtime`'s two NDK-portability bugs fixed upstream (CNA Task 920)
      dep: — · sys: — · plat: AND · pri: MUST · zone: all · adv: D10 · est: 1
      accept: `cmake --build` completes for `arm64-v8a`; if not, this phase stops here and is revisited later
      amended: (2026-09-21, `HOUSE-03201`) re-verify BL-13 at the time this task is picked (the last recorded state is *blocked*, 2026-09-06); desktop-side readiness tasks (`HOUSE-02954`–`02958`, `02960`, `02991`–`02998`, `03000`, `03001`) do not wait for this gate
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) may be picked at any time as Track B; it no longer waits for the Web milestone
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02905`

- [ ] HOUSE-02952 — **Gate: confirm `CNA_GRAPHICS_RENDERER=OPENGLES3` is selectable and buildable for Android**
      dep: HOUSE-02951 · sys: — · plat: AND · pri: MUST · zone: all · adv: D10 · est: 0.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02953 — **Gate: confirm a CNA graphics sample runs on a device or emulator**
      dep: HOUSE-02952 · sys: — · plat: AND · pri: MUST · zone: all · adv: D10 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02959 — Design the Android lifecycle mapping (pause, resume, background, surface loss) onto `Game`'s events
      dep: HOUSE-02953 · sys: app · plat: AND · pri: MUST · zone: all · adv: D10 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02960`: the desktop focus-loss path is proved to run the same lifecycle code

- [ ] HOUSE-03031 — Create the Gradle/NDK project producing a shared library plus `SDLActivity`, following CNA's own devices-demo precedent
      dep: HOUSE-02959 · sys: app · plat: AND · pri: MUST · zone: all · adv: D10 · est: 2.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-02962`

- [ ] HOUSE-03032 — Build the full game for `arm64-v8a` and fix every compile and link error
      dep: HOUSE-03031 · sys: — · plat: AND · pri: MUST · zone: all · adv: D10 · est: 4
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03033 — Wire content delivery (APK assets or OBB) and the content root
      dep: HOUSE-03032, HOUSE-02958 · sys: content · plat: AND · pri: MUST · zone: all · adv: D10 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03034 — Wire the save store to Android's app-private storage
      dep: HOUSE-03032, HOUSE-00152 · sys: persistence · plat: AND · pri: MUST · zone: all · adv: D10 · est: 0.75
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03036 — Wire the lifecycle mapping and verify pause/resume/background on a device
      dep: HOUSE-03032, HOUSE-02959 · sys: app · plat: AND · pri: MUST · zone: all · adv: D10 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03037 — Run on a real device; measure against the Android budget; reduce until it fits
      dep: HOUSE-03036 · sys: — · plat: AND · pri: MUST · zone: all · adv: D9, D10 · est: 2.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) "representative hardware or the best available realistic environment": one physical device; if none is available, the best available emulator with GPU acceleration, recorded as such

- [ ] HOUSE-03038 — Verify the whole feature set on a device against the checklist
      dep: HOUSE-03037, HOUSE-02797 · sys: — · plat: AND · pri: MUST · zone: all · adv: D10 · est: 1.5
      amended: (2026-09-21, `HOUSE-03201`) checked against the DONE checklist's Android items
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) the **Android DONE checklist** (absorbs `HOUSE-03040`): install and run; touch controls sufficient for the walk; the representative traversal of `HOUSE-02900`; no major corruption; acceptable performance. One device is enough; more are recorded when available
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03037`

- [ ] HOUSE-03039 — Verify the touch controls on a device and tune
      dep: HOUSE-03038, HOUSE-02993 · sys: ui · plat: AND · pri: MUST · zone: all · adv: D8, D10 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03038, HOUSE-03001`

- [ ] HOUSE-03041 — Package and document the Android build
      dep: HOUSE-03039 · sys: — · plat: AND · pri: MUST · zone: all · adv: D10, D12 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02961` (the readiness matrix in `docs/portability.md`)
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03040`

- [ ] HOUSE-03042 — Phase-51 review and commit
      dep: HOUSE-02954, HOUSE-02958, HOUSE-02991, HOUSE-02992, HOUSE-02993, HOUSE-02995, HOUSE-02996, HOUSE-02997, HOUSE-02998, HOUSE-02951, HOUSE-02952, HOUSE-02953, HOUSE-02959, HOUSE-03031, HOUSE-03032, HOUSE-03033, HOUSE-03034, HOUSE-03036, HOUSE-03037, HOUSE-03038, HOUSE-03039, HOUSE-03041 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D10 · est: 0.5
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD
      amended: (2026-09-21, `HOUSE-03205`) dependencies were `HOUSE-03031…HOUSE-03041`

---

## M16 — Final release

Last. Measure each platform once more, fix only real misses, audit, document, tag.

- [ ] HOUSE-03071 — Profile the release build on all three platforms and produce a prioritised optimisation list
      dep: HOUSE-02797, HOUSE-02905, HOUSE-03042 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D9 · est: 1
      amended: (2026-09-21, `HOUSE-03201`) if BL-13 is still blocked upstream when every other milestone is done, `HOUSE-03042` is satisfied by the recorded blocker per DONE item D10
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-03042`
      amended: (2026-09-21, `HOUSE-03205`) the representative scenarios of `HOUSE-02402` on each platform, not a full profile

- [ ] HOUSE-03072 — Work the optimisation list until every platform meets its budget with 15 % headroom
      dep: HOUSE-03071 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D9 · est: 2
      amended: (2026-09-21, `HOUSE-03205`) fix only misses of a platform's documented target and platform-specific catastrophic regressions; **the 15 % headroom requirement is removed**

- [ ] HOUSE-03073 — Final content audit: every asset used, no orphans, every licence recorded, `THIRD-PARTY-ASSETS.md` regenerated
      dep: HOUSE-03072 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D12 · est: 0.75

- [ ] HOUSE-03074 — Final XNA-only audit: the lint, the `nm -C` symbol check, `check_anim_assets.py`, and a manual review confirming that no runtime source calls a CNA-specific graphics or model API — no `SupportsCapability`, no `*EXT*` call, no `Model::Tag` read
      dep: HOUSE-03073 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D11 · est: 0.75
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02787` (`docs/xna-deviations.md` grants nothing; the lint has no allowlist)

- [ ] HOUSE-03075 — Final documentation pass: `cna-house.md`, `README.md`, `docs/*` all reflect the shipped software
      dep: HOUSE-03074 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D12 · est: 2
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02795`: the one documentation pass that makes `cna-house.md`, `README.md` and `docs/*` match what shipped

- [ ] HOUSE-03076 — Final test pass: the whole suite green on all platforms; the soak test on Linux and Web
      dep: HOUSE-03075 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D11 · est: 1.5
      amended: (2026-09-21, `HOUSE-03205`) the suites green; the Web smoke (`HOUSE-02901`) and the Android checklist's smoke where the device path is open. No Web soak and no second long run

- [ ] HOUSE-03077 — Write the final release notes, the known-limitations list and the credits
      dep: HOUSE-03076 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D12 · est: 0.75
      amended: (2026-09-21, `HOUSE-03205`) absorbs `HOUSE-02796`

- [ ] HOUSE-03078 — Tag the release
      dep: HOUSE-03077 · sys: — · plat: ALL · pri: MUST · zone: all · adv: DONE · est: 0.25

---

## Optional after DONE

**`pri: OPT`. Never scheduled while a MUST task is open (rule R13), and not counted in the
estimate.** These are real ideas with real ids, moved here by `HOUSE-03205` from the milestones
named on each entry, or deferred by `HOUSE-03201`. Each entry shows its title and its dependencies
only; the full text is in the legacy ledger (every id here is a legacy id) and, with the
`HOUSE-03201` amendments, in `plan.md` at `5927073`. A dependency on a cancelled task is replanned
when the optional task is picked.

### Sun glare and lens flare (occlusion queries)

- [ ] HOUSE-01567 — Implement the `OcclusionQuery` pool and its per-frame lifecycle (create once, reuse, read one frame late)
      dep: HOUSE-00090 · sys: rendering · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01568 — Implement the 3 × 3 glare sample grid with colour writes disabled and depth test on
      dep: HOUSE-01567, HOUSE-01566 · sys: rendering · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01569 — Implement the coverage computation with the boolean/count refinement and a 0.25 s smoothing
      dep: HOUSE-01568 · sys: rendering · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01570 — Implement the 7-sprite additive lens flare through `SpriteBatch`
      dep: HOUSE-01569, HOUSE-00145 · sys: rendering · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01571 — Author the flare sprite set and tune its colours, scales and offsets
      dep: HOUSE-01570 · sys: content · plat: TOOL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01575 — Validate the OPENGLES3 boolean-grid coverage against the OPENGL33 true count on the same scene
      dep: HOUSE-01569, HOUSE-00091 · sys: ci · plat: LNX · pri: OPT · was: MUST, M7
- [ ] HOUSE-01576 — Render tests: sunrise, morning, noon, afternoon, sunset, and the glare at 6 angles
      dep: HOUSE-01572 · sys: ci · plat: CI · pri: OPT · was: MUST, M7
- [ ] HOUSE-01577 — Measure the glare queries' cost
      dep: HOUSE-01569 · sys: — · plat: LNX · pri: OPT · was: MUST, M7
- [ ] HOUSE-02401 — **Q-08 experiment: `OcclusionQuery` for exterior occlusion.** Queries around the 24 neighbourhood LOD groups, a 60-second recorded camera path, A/B GPU timing
      dep: HOUSE-01567, HOUSE-00852 · sys: rendering · plat: LNX · pri: OPT · was: OPT, Deferred

### Storm: lightning and thunder

- [ ] HOUSE-01831 — Implement the Poisson lightning process driven by `thunderIntensity`
      dep: HOUSE-01686 · sys: weather · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01832 — Implement strike distance and azimuth sampling, biased by intensity and wind
      dep: HOUSE-01831 · sys: weather · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01833 — Implement the 3-lobed flash envelope (90–160 ms)
      dep: HOUSE-01832 · sys: weather · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01834 — Implement the flash's effect on `daylight` for every cell with an exterior window
      dep: HOUSE-01833, HOUSE-01263 · sys: lighting · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01835 — Implement the flash's `DirectionalLight0` override for dynamic objects
      dep: HOUSE-01834, HOUSE-01261 · sys: lighting · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01836 — Implement the full-screen additive flash quad scaled by how much sky the camera can see
      dep: HOUSE-01833, HOUSE-00145 · sys: rendering · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01837 — Implement the visible bolt for strikes under 2 km: a 3-level midpoint-displaced polyline with 2 forks, drawn as additive quads
      dep: HOUSE-01836 · sys: rendering · plat: ALL · pri: OPT · was: SHOULD, M7
- [ ] HOUSE-01838 — Implement thunder scheduling at `distance / 343 m/s` and the 3-class sample selection
      dep: HOUSE-00286, HOUSE-01832 · sys: audio · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01839 — Implement thunder's indoor attenuation and its dull cross-fade
      dep: HOUSE-01838 · sys: audio · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01840 — Implement the gust front before a near strike (+3…8 m/s for 4–9 s)
      dep: HOUSE-01832 · sys: weather · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01841 — Test: the flash-to-thunder delay matches the sampled distance within 50 ms
      dep: HOUSE-01838 · sys: ci · plat: CI · pri: OPT · was: MUST, M7
- [ ] HOUSE-01842 — Test: a strike raises `daylight` in exactly the cells with exterior windows
      dep: HOUSE-01834 · sys: ci · plat: CI · pri: OPT · was: MUST, M7
- [ ] HOUSE-01843 — Render tests: a frozen flash from outdoors, from a lit room, from a dark room, and from the basement
      dep: HOUSE-01836 · sys: ci · plat: CI · pri: OPT · was: MUST, M7

### Snow cover: accumulation and melt

- [ ] HOUSE-00778 — Build the snow shells (`build_snowshell.py`) for terrain, roofs, decks, rails, furniture and the car
      dep: HOUSE-00214, HOUSE-00777, HOUSE-03380 · sys: content · plat: TOOL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01793 — Implement the snow-shell rendering driven by `snowDepth` with a smoothstep alpha fade
      dep: HOUSE-00778, HOUSE-01691 · sys: rendering · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01794 — Implement the snow displacement of the shell along the surface normal by `snowDepth`
      dep: HOUSE-01793 · sys: rendering · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01795 — Implement the footstep surface switch to `snow` and the deep-snow speed penalty
      dep: HOUSE-01691, HOUSE-00557 · sys: player · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01797 — Implement melt: `snowDepth` decay from temperature and direct sun, faster on dark materials
      dep: HOUSE-01691 · sys: weather · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01798 — Implement snow on the car, the bins, the fence rails and the garden furniture via their shell entries
      dep: HOUSE-01793 · sys: rendering · plat: ALL · pri: OPT · was: SHOULD, M7
- [ ] HOUSE-01802 — Implement accumulating seasonal snow cover on the garden, the roof, the fence and the cars, building and melting with the outdoor temperature rather than with the season index
      dep: HOUSE-01704, HOUSE-01793 · sys: weather · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01800 — Render tests: light snow, blizzard, 5 cm accumulation, 30 cm accumulation, melt in progress
      dep: HOUSE-01797 · sys: ci · plat: CI · pri: OPT · was: MUST, M7

### Hail

- [ ] HOUSE-01875 — Implement the hail-with-thunder-and-temperature-drop coupling in the archetype table
      dep: HOUSE-01688 · sys: weather · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01871 — Implement hail particle motion: fast fall, small bright quads, motion-blur streak
      dep: HOUSE-01742 · sys: weather · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01872 — Implement the hail bounce (0.35 restitution, randomised tangential, 0.5 s extra life)
      dep: HOUSE-01871, HOUSE-00777 · sys: weather · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01873 — Implement the short-lived hail-stone ground scatter above intensity 0.5
      dep: HOUSE-01872 · sys: weather · plat: ALL · pri: OPT · was: OPT, Deferred
- [ ] HOUSE-01874 — Implement hail audio: the density-driven impact layer plus roof, window and car layers
      dep: HOUSE-00286, HOUSE-01872 · sys: audio · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01889 — Render tests: a hail squall, trees at 4 wind speeds, a door mid-slam
      dep: HOUSE-01881 · sys: ci · plat: CI · pri: OPT · was: MUST, M7

### Wind detail and vegetation

- [ ] HOUSE-01876 — Implement the wind model: base + 3-octave gust noise + direction wander
      dep: HOUSE-01686, HOUSE-00027 · sys: weather · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01877 — Implement vegetation sway: the 15 Hz CPU vertex pass for the ≤ 40 nearest instances, static beyond
      dep: HOUSE-01876, HOUSE-00772 · sys: weather · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01878 — Implement the gust-synchronised whole-canopy lean
      dep: HOUSE-01877 · sys: weather · plat: ALL · pri: OPT · was: SHOULD, M7
- [ ] HOUSE-01879 — Implement grass and hedge sway (faster, smaller)
      dep: HOUSE-01877 · sys: weather · plat: ALL · pri: OPT · was: SHOULD, M7
- [ ] HOUSE-00773 — Implement grass-card rendering with `AlphaTestEffect` and per-instance jitter
      dep: HOUSE-00772, HOUSE-00080 · sys: rendering · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01886 — Implement the weather vane on the shed pointing into the wind
      dep: HOUSE-01876 · sys: rendering · plat: ALL · pri: OPT · was: OPT, Deferred
- [ ] HOUSE-01887 — Implement the swing bench and wind chimes ambient motion
      dep: HOUSE-01876 · sys: rendering · plat: ALL · pri: OPT · was: OPT, Deferred

### Seasonal vegetation

- [ ] HOUSE-00919 — Author the seasonal vegetation material set — spring light green with blossom, summer dense dark green, autumn yellow/orange/red, winter bare — and blend between them from `SeasonPhase`
      dep: HOUSE-01543, HOUSE-00897 · sys: rendering · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-00920 — Implement the autumn leaf-fall transition: foliage density ramps down over the last 20 % of autumn, leaf litter accumulates on the ground, and the branches finish bare
      dep: HOUSE-00919 · sys: rendering · plat: ALL · pri: OPT · was: MUST, M7

### Rain detail

- [ ] HOUSE-01746 — Implement splash particles on exposed surfaces within 6 m, capped at 60
      dep: HOUSE-01744 · sys: weather · plat: ALL · pri: OPT · was: MUST, M7
- [ ] HOUSE-01749 — Implement puddle decals at the height field's local minima above `wetness > 0.55`
      dep: HOUSE-01748 · sys: rendering · plat: ALL · pri: OPT · was: SHOULD, M7

### Interior light detail

- [ ] HOUSE-01268 — Implement the sun-patch decals from the precomputed polygons, interpolated between grid entries
      dep: HOUSE-00208, HOUSE-01263 · sys: rendering · plat: ALL · pri: OPT · was: MUST, M5
- [ ] HOUSE-01565 — Make the sun-patch decals follow the real sun by interpolating the 12 × 24 grid
      dep: HOUSE-01564, HOUSE-01268 · sys: rendering · plat: ALL · pri: OPT · was: MUST, M5
- [ ] HOUSE-02690 — Implement the dust-motes-in-sunbeam particle effect where a sun patch is strong
      dep: HOUSE-01565, HOUSE-01741 · sys: weather · plat: ALL · pri: OPT · was: OPT, Deferred

### Street and neighbourhood detail

- [ ] HOUSE-00850 — Implement the neighbour porch lights and street lights on the dusk sensor with per-fixture offsets
      dep: HOUSE-00849, HOUSE-01269, HOUSE-03380 · sys: lighting · plat: ALL · pri: OPT · was: MUST, M5
- [ ] HOUSE-00851 — Implement impostor rendering: yaw slice selection, sky tinting, vertical-axis billboarding for trees
      dep: HOUSE-00845, HOUSE-00204, HOUSE-00856, HOUSE-00772 · sys: rendering · plat: ALL · pri: OPT · was: MUST, M5
- [ ] HOUSE-00852 — Wire the neighbourhood into the exterior BVH with the correct LOD distances
      dep: HOUSE-00851, HOUSE-00678, HOUSE-00856 · sys: visibility · plat: ALL · pri: OPT · was: MUST, M5
- [ ] HOUSE-02395 — Generate the impostor atlases for trees, shrubs and the neighbourhood houses
      dep: HOUSE-00204, HOUSE-02394 · sys: content · plat: TOOL · pri: OPT · was: MUST, M10

### Reflections, fire and filtering

- [ ] HOUSE-02684 — Implement the mirror cube maps and the `EnvironmentMapEffect` mirror material
      dep: HOUSE-00209, HOUSE-00896 · sys: rendering · plat: ALL · pri: OPT · was: SHOULD, M11
- [ ] HOUSE-02685 — **Q-05 decision: measure a planar reflection pass for one mirror; adopt or record the rejection**
      dep: HOUSE-02684 · sys: rendering · plat: LNX · pri: OPT · was: OPT, Deferred
- [ ] HOUSE-02691 — Implement fireplace embers, flame and the fire loop audio in `L0_LIVING`, and the garden fire pit
      dep: HOUSE-01741, HOUSE-00279 · sys: weather · plat: ALL · pri: OPT · was: SHOULD, M11
- [ ] HOUSE-00916 — Implement anisotropic filtering selection from the project-owned effective feature set (`cna-house.md` §68): request `SamplerState::MaxAnisotropy` only on a build/platform profile validated to permit it (HOUSE-00109), and sample **trilinear** (`TextureFilter::Linear` with mips) everywhere else. No CNA-specific runtime capability query — the setting row is simply not offered on a profile that does not permit it.
      dep: HOUSE-00109 · sys: rendering · plat: ALL · pri: OPT · was: SHOULD, M10

### Tier E compiled effects (was M11b)

- [ ] HOUSE-02702 — Implement the Tier E `RoomLit.fx` and its four techniques
      dep: HOUSE-00087, HOUSE-01256 · sys: rendering · plat: ALL · pri: OPT · was: SHOULD, M11
- [ ] HOUSE-02703 — Implement the Tier E `ShadowDepth.fx` and the shadow map with the frustum-fitted light projection and texel snapping
      dep: HOUSE-02702, HOUSE-00083 · sys: rendering · plat: ALL · pri: OPT · was: SHOULD, M11
- [ ] HOUSE-02704 — Implement 3×3 PCF and the shadow's indoor disable rule
      dep: HOUSE-02703 · sys: rendering · plat: ALL · pri: OPT · was: SHOULD, M11
- [ ] HOUSE-02706 — Implement the Tier E `SkyDome.fx` with the per-pixel gradient, three cloud layers, sun disc and dithering
      dep: HOUSE-02702, HOUSE-01650 · sys: rendering · plat: ALL · pri: OPT · was: SHOULD, M11
- [ ] HOUSE-02707 — Implement the Tier E `Precip.fx` with soft-edge depth fade and wind shear
      dep: HOUSE-02702, HOUSE-01741 · sys: rendering · plat: ALL · pri: OPT · was: SHOULD, M11
- [ ] HOUSE-02708 — Implement the Tier E `SurfaceBlend.fx` for continuous wetness and snow
      dep: HOUSE-02702, HOUSE-01748 · sys: rendering · plat: ALL · pri: OPT · was: SHOULD, M11
- [ ] HOUSE-02709 — Implement the Tier E `PostComposite.fx` with exposure adaptation and the glare composite
      dep: HOUSE-02702, HOUSE-01266 · sys: rendering · plat: ALL · pri: OPT · was: SHOULD, M11
- [ ] HOUSE-02595 — Implement the Tier S vs Tier E content-identity test (same scene content, different pixels)
      dep: HOUSE-02593 · sys: ci · plat: CI · pri: OPT · was: SHOULD, M11
- [ ] HOUSE-02711 — Verify every Tier E path has a working Tier S fallback and that forcing Tier S loses no scene content
      dep: HOUSE-02702, HOUSE-02703, HOUSE-02704, HOUSE-02706, HOUSE-02707, HOUSE-02708, HOUSE-02709, HOUSE-02595 · sys: ci · plat: CI · pri: OPT · was: SHOULD, M11
- [ ] HOUSE-02712 — Measure Tier E against budget and against Tier S
      dep: HOUSE-02711 · sys: — · plat: LNX · pri: OPT · was: SHOULD, M11

### Web extras

- [ ] HOUSE-02844 — Implement the desktop context-loss test using EasyGL's `DebugSimulateContextLoss`, destroying and rebuilding every GPU resource and comparing a rendered frame
      dep: HOUSE-00108 · sys: ci · plat: CI · pri: OPT · was: MUST, M14
- [ ] HOUSE-02845 — Fix every resource that fails HOUSE-02844
      dep: HOUSE-02844 · sys: rendering · plat: ALL · pri: OPT · was: MUST, M14
- [ ] HOUSE-02897 — Verify context loss and restore in a real browser using `WEBGL_lose_context`
      dep: HOUSE-02892, HOUSE-02844 · sys: rendering · plat: WEB · pri: OPT · was: MUST, M14
- [ ] HOUSE-02853 — Implement `WebSaveStore` against `System::IO::IsolatedStorage`, and test it on Linux behind a flag
      dep: HOUSE-00152 · sys: persistence · plat: ALL · pri: OPT · was: MUST, M14
- [ ] HOUSE-02893 — Wire `WebSaveStore` and verify persistence across a page reload
      dep: HOUSE-02892, HOUSE-02853 · sys: persistence · plat: WEB · pri: OPT · was: MUST, M14
- [ ] HOUSE-02902 — Evaluate whether threads are needed (COOP/COEP cost vs. load time); decide and record
      dep: HOUSE-02898 · sys: — · plat: WEB · pri: OPT · was: OPT, M14

### Application extras

- [ ] HOUSE-03571 — Optional session resume: player pose, clock and weather in one small file
      dep: HOUSE-00152 · sys: persistence · plat: ALL · pri: OPT · was: SHOULD, M9

### Project

- [ ] HOUSE-03079 — Post-release retrospective: what the plan got wrong, recorded for the next project
      dep: HOUSE-03078 · sys: — · plat: ALL · pri: OPT · was: SHOULD, M16

Without ids (each would need an R9 entry first): an attract mode that auto-walks the grand-tour route
for demonstrations; seasonal decorations; additional unique furniture packs; deeper ambient audio;
additional browsers and devices; additional hero-area polish; more hero areas; a drivable car (still
Q-06, a different project).

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

## Scope-reduction cleanup candidates

Infrastructure that was built or imported for scope that is now removed. **Nothing here is deleted
by `HOUSE-03201`.** Classification: **keep** (harmless or still useful), **deprecate** (do not extend;
remove only if it starts to cost), **remove later** (clearly unnecessary and costly to maintain;
schedule with an R7 estimate after G3, or earlier if it blocks breadth work, as `HOUSE-03301` does).

| Candidate | What it is | Class | Reason |
|---|---|---|---|
| `layout.nav.json`, `tools/world/build_nav.py`, `docs/nav-format.md`, the nav loading and validation in `WorldLoader.cpp` | pet waypoint graph, perches, beds, bowls (122 KB) | **remove later** (coupling removed early by `HOUSE-03301`) | every furnishing change had to keep pet waypoints valid |
| `initialstate.json` `pets` block | canonical dog and cat state | **remove later** | no pets |
| `src/animation/` (`Animation`, `AnimationCache`, `ChanimReader`, 576 lines), `ChanimReaderTests`, `ChanimRoundTripTests`, `AnimationBindTests`, `tools/ci/check_anim_assets.py` (a `run_checks.sh` gate), `tools/assets/anim_extract.py`, `docs/anim-format.md` | the `.chanim` skeleton/clip sidecar for avatar and pets | **remove later** | a gate and three test files maintained for nothing that ships |
| `content/Models/Characters/*`, `assets-src/Models/Characters/*`, `tools/assets/generate_human_bases.py`, `retarget_cmu_locomotion.py`, `measure_stride.py`, `skin_split.py`, their manifest and licence rows | MPFB human base meshes and locomotion tooling | **remove later** | avatar removed; manifest/licence rows must go with the files |
| `content/Audio/human` (9.9 MB) | human breath and voice samples | **remove later** | no avatar |
| `content/Audio/appliance` (29 MB) | NOX electromagnetic pack (consoles, laptops, phones, router, fan…) | **deprecate** | keep only the loops `HOUSE-01927` uses; drop the rest with their manifest rows |
| `content/Audio/car` (4.4 MB) | car samples | **deprecate** | may serve distant-road ambience (`HOUSE-00290`); remove if unused at G5 |
| `interactables.json`: 26 faucet, 12 toilet, 11 appliance, 3 television rows | behaviour data for removed systems | **remove later** | nothing reads them for behaviour |
| `interactables.json`: 63 door, 54 window, 3 gate, 1 garage-door, 79 light-switch rows | referenced by 177 portal `aperture`s and by lighting groups | **keep** (review in `HOUSE-03221`) | the static-pose work may reuse `openFraction`; lighting uses the groups |
| `src/world/InteractableExpr.cpp`, `InteractableExprTests` | the closed predicate/effect vocabulary (ADR-0005) | **deprecate** | frozen, not extended; remove if no retained row uses an expression |
| `src/interaction/behaviours/`, `include/cnahouse/interaction/behaviours/`, `src/animals/` (empty directories required by `check_layout.py`) | placeholders from the layout | **remove later** | a one-line `check_layout.py` change each |
| `audio::Category::Animals`, `Category::Media` | enum values | **deprecate** | trivial; drop with the next audio change |
| `CELL_FRIDGE_INTERIOR`, `CELL_FREEZER_INTERIOR` and their portals | nested appliance interiors | **keep** | harmless; always culled while closed |
| `content/Video/smoke_clip.*`, `assets-src/Media/Video`, `CNA_ENABLE_VIDEO` wiring | the capability smoke clip | **keep** | proves the video path compiles; costs nothing |
| `src/persistence/DesktopSaveStore.cpp`, ADR-0008's versioning | settings and the session file | **keep** | used by `HOUSE-00131` and `HOUSE-03571` |
| The 28 per-piece `--check` gates in `run_checks.sh` (`kitchen-builtins`, `living-piano`, `dining-suite`, …) | reproducibility proofs for the bespoke ground-floor pieces | **keep**; **deprecate the pattern** | they prove real assets regenerate byte for byte; new kit pieces are covered by group-level gates (`HOUSE-00985`, `HOUSE-03303`), not one gate each |
| `docs/asset-review/{dog,cat,human-avatar,human-bases,human-locomotion,player-car}`, `docs/asset-selection/*-hero-research.md` | sourcing evidence | **keep** | historical record; no maintenance cost |
| `cna-house.md` §§ 45–47, 50–58, 60–61, 64–66 | design of removed systems | **keep** (under the scope banner) | design history; not requirements |
| `src/weather/SnowAccumulation.cpp` (`snowDepth`), `tools/world/build_snowshell.py`, `docs/snowshell-format.md`, the `snow-materials` gates | snow cover | **keep**, not extended (`HOUSE-03205`) | finished work the optional snow-cover tasks would use; it costs nothing today |
| The thunderstorm and hail archetypes and couplings in the weather data | storm and hail weather states | **keep** (`HOUSE-03205`) | they render as heavy rain (`HOUSE-01743`) |
| `docs/sunpatch-format.md` and the precomputed sun-patch polygons | sun patches | **keep** (`HOUSE-03205`) | used by the optional `HOUSE-01268` |
| `tools/blender/cubemap_bake.py`, the `EnvironmentMapEffect` binder (`HOUSE-00896`) | mirror reflections | **keep** (`HOUSE-03205`) | used by the optional `HOUSE-02684` |
| `tools/blender/impostor_render.py` | impostor atlases | **keep** (`HOUSE-03205`) | used by the optional `HOUSE-02395`/`00851` |
| Debug overlays `F1`–`F5`, `F8`, `F9` and `src/debug/Console.cpp` | debug tools | **keep, frozen** (`HOUSE-03205`) | the overlay and console extension tasks are cancelled; nothing is removed |
| `tools/assets/footstep_map.py`, `docs/asset-selection/footstep-surfaces.md` | the 20-surface footstep map | **keep** (`HOUSE-03205`) | still documents the NOX coverage; its "unserved" list no longer drives work (`HOUSE-00281` is cancelled) |
| `content/Audio/appliance` (29 MB), `content/Audio/car` (4.4 MB) | NOX packs | **remove later** (was *deprecate*; `HOUSE-03205`) | `HOUSE-01927` is cancelled and no traffic bed is planned; keep only a loop `HOUSE-01922` actually uses |

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

## Remaining-work estimate

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

## Planning corrections

Corrections made to `cna-house.md` or to this file during implementation, with the evidence that
forced each one. Nothing is changed silently. The 2026-09-06 → 2026-09-20 corrections (about 85 KB)
are in the legacy ledger's *Planning corrections* section.

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

## Deferred and explicitly out of scope

Recorded so nobody has to re-derive the decision. The legacy rows are kept, and the 2026-09-21 rows
are added.

| Item | Status | Reason |
|---|---|---|
| Animals (dog, cat) and their AI, navigation, audio and animation | **Removed** 2026-09-21 | ADR-0014 |
| Visible player avatar, third person, customisation, character animation, foot IK | **Removed** 2026-09-21 | ADR-0014: first person only |
| Interaction gameplay (targeting, prompts, pick-up, carry, seats, containers, inventories) | **Removed** 2026-09-21 | ADR-0014: static dressing |
| Appliance, kitchen, plumbing, toilet and television simulation | **Removed** 2026-09-21 | ADR-0014 |
| Door, window and switch gameplay | **Removed** 2026-09-21 | ADR-0014: static poses, automatic lighting schedule |
| Household-state persistence, Reset House | **Removed** 2026-09-21 | ADR-0014: settings plus the optional session file only |
| Portal-path room-aware audio | **Removed** 2026-09-21 | ADR-0014 supersedes ADR-0010 in part |
| Drivable car | **Deferred** (Q-06; no task) | a different project; the car is a prop |
| Player reflection in mirrors | **Deferred** (Q-05; `HOUSE-02685` after DONE) | budget; D-19 |
| Swimming pool | **Rejected** | D-22 |
| Music score | **Rejected** | D-21 |
| Rigid-body physics engine | **Rejected** | D-07 |
| ECS | **Rejected** | D-06 |
| Deferred rendering / G-buffer | **Not planned** | Tier S is forward; MRT is possible for Tier E (BL-03 disproved) but not needed |
| Stencil-based techniques | **Not planned** | BL-02 was disproved (stencil works); nothing in the showcase needs it |
| Real-time global illumination | **Out of scope** | §3 |
| Multiplayer | **Out of scope** | §3 |
| Modding API / level editor GUI | **Out of scope** | §3; the JSON is hand-editable |
| Cooking, hunger, spoilage, inventory grid | **Rejected** | §55.3; now moot |
| Air-flow / temperature / humidity simulation | **Rejected** | §52 |
| Water damage / flooding | **Rejected** | §56.3; now moot |
| HRTF / convolution reverb | **Impossible** | BL-11 |
| iOS, macOS, Windows, console targets | **Out of scope** | not requested |
| C5 quality in every room; C6 as a project-wide level | **Removed** 2026-09-21 | ADR-0015: quality tiers |
| Uniqueness and density quotas for furniture; bespoke furniture outside hero areas | **Removed** 2026-09-21 | ADR-0015: a reusable kit (rule R8) |
| Snow cover (accumulation, melt), hail, lightning and thunder, lens flare and glare, vegetation sway, grass cards, seasonal vegetation, puddles, splashes, sun patches, impostor rendering, neighbour porch lights, mirrors, fireplace fire | **Optional after DONE** 2026-09-21 | ADR-0015 |
| Positional emitters, per-room room tone, appliance hums, clocks, creaks, seasonal ambience | **Removed** 2026-09-21 | ADR-0015 |
| Session resume; Web persistence across a reload; Web context-loss recovery; Web threads | **Optional after DONE** 2026-09-21 | ADR-0015 |
| Key remapping, toasts, vignette, several settings pages, further debug overlays and console work | **Removed** 2026-09-21 | ADR-0015 |
| Exhaustive render, unit, browser, device and performance matrices; 30-day runs; recurring 2-hour soaks; a 15 % headroom target | **Removed** 2026-09-21 | ADR-0015 |
| Tier E compiled effects | **Optional after DONE** 2026-09-21 | ADR-0003 (Tier S is complete alone), ADR-0015 |
