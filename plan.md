# CNA House — Showcase Completion Plan

`STATUS: APPROVED — IMPLEMENTATION IN PROGRESS · SCOPE REDUCED 2026-09-21 (HOUSE-03201, ADR-0014)`

This file is the **execution ledger for the remaining work**. A checkbox moves to `[x]` only when
the task's acceptance criteria are genuinely satisfied and its `verify:` step has been run. Task ids
are permanent and are never renumbered.

| | |
|---|---|
| Goal | A polished, atmospheric, multiplatform **architectural showcase for CNA**. See [Direction](#direction) |
| Scope decision | [ADR-0014](docs/decisions/ADR-0014-showcase-scope.md), 2026-09-21 |
| Legacy ledger | [`docs/history/plan-legacy-2026-09-21.md`](docs/history/plan-legacy-2026-09-21.md) is the original 53-phase plan, frozen at `7499770`. It is the authoritative record of every task completed before 2026-09-21 and keeps the verbatim text of every cancelled task |
| Legacy tasks | 674 completed · 373 carried here with their ids · 385 cancelled with reasons · 6 deferred until after DONE |
| This plan | 424 open tasks in 17 milestones (51 new, 373 carried) |
| Current gate | **G1, not passed.** Start with M0 (review tooling), then M1 (traversal) |
| Estimate to DONE | **≈ 635–986 agent-hours** (see [Remaining-work estimate](#remaining-work-estimate)) |

**Read in this order:** [Direction](#direction) → [Definition of DONE](#definition-of-done) →
[Scheduling rules](#scheduling-rules--time-discipline) → [Zone scoreboard](#zone-scoreboard) →
the milestone you are working in.

---

## Direction

House Simulator (`cna-house`) is **a polished, atmospheric, multiplatform architectural and
graphics showcase for CNA.** The player explores a large, detailed house on foot: the basement,
the ground floor, both upper floors and the attic. The garage, the garden, the street and the
neighbourhood are part of the same walk. The core experience is **walking and looking**. It
demonstrates that CNA, through the XNA 4.0 API alone (ADR-0001), can carry a substantial realistic
3-D environment on Linux desktop, in a browser and on Android.

It is **not** a life simulator. There are no animals and no visible avatar. There is no
interaction gameplay: no picking up, no containers, no appliance, plumbing, toilet or television
behaviour, and no household-state persistence. Objects are **static dressing**. A toilet looks like
a toilet, a refrigerator looks like a refrigerator, and cupboards stay closed. Doors and gates have
**static poses** so that every area stays reachable. Lights follow an **automatic schedule**.

What matters, in priority order:

1. **Complete architectural coverage.** Every zone of the property is present and walkable:
   `B1`, `L0`, `L1`, `L2`, `L3`, the stairs, the garage, the front and rear exterior, and the
   street and neighbourhood.
2. **Static furnishing and dressing** that give every room an identity and believable placement.
3. **Visual quality:** materials, baked and dynamic lighting, sky, sun, moon, stars, day and night,
   seasons, weather, windows, reflections where supported, LOD and impostors.
4. **Environmental simulation that is visible:** time, sun, moon, weather (rain, snow, storm,
   hail, wind) and seasons. Each piece must show on screen and must not grow into a large
   simulation of its own.
5. **Lightweight atmospheric audio:** footsteps, room tone, exterior ambience and weather.
6. **A robust first-person controller:** walking, mouse look, Web and touch controls, collision,
   stairs, steps, headroom, depenetration.
7. **Measured performance** on every platform.
8. **Tests** that protect all of the above.

When the choice is between a hidden simulation system and finishing an unfinished floor, **finish
the floor**. When the choice is between the twentieth polish pass on a good ground-floor room and
bringing the basement, an upper floor, the attic or the garden up to the same baseline, **choose
the neglected area**.

---

## Definition of DONE

House Simulator is **done** when every line below is true. Nothing else is required. In particular
no animal, avatar, household interaction, appliance behaviour, openable container or household
save is required.

| # | Criterion | Proved by |
|---|---|---|
| **D1** | **The whole property is present.** Every zone in the [scoreboard](#zone-scoreboard) (basement, ground floor, garage, both upper floors, attic, vertical circulation, front exterior, rear and side exterior, street and neighbourhood) is at **C5** or better | Scoreboard at G5 (`HOUSE-03480`) |
| **D2** | **Every intended area is reliably explorable.** The grand tour walks the real controller from the spawn to every intended-accessible cell and back; the random walk and inside-geometry guarantees pass; no S1/S2 traversal or collision defect is open | `HOUSE-03226`, `HOUSE-00617`, `HOUSE-00618`, `HOUSE-03227` |
| **D3** | **The spaces are believably dressed.** Every accessible room has its recipe's primary and secondary dressing; the density check and the anti-repetition validator pass | `HOUSE-01027`, `HOUSE-01028`, `HOUSE-01033` |
| **D4** | **Showcase visual quality.** No S1/S2 visual defect open anywhere; the whole-project polish pass is complete; the S3 backlog is triaged | `HOUSE-03631`, `HOUSE-02714` |
| **D5** | **The environment works and is visible.** Day and night with the real sun, moon phases and stars; six sky states and fog; rain, snow with accumulation and melt, storm with lightning and delayed thunder, wind moving vegetation; seasons visibly change vegetation and ground; hail where the archetypes produce it | `HOUSE-03520` |
| **D6** | **Lighting is showcase quality.** Furniture is part of the baked lighting or of the consistency solution; interior lights follow the automatic schedule; exterior dusk lighting and sun patches work | `HOUSE-03420`, `HOUSE-03401`, `HOUSE-01268` |
| **D7** | **Atmospheric audio.** Footsteps on every surface; room tone per cell; exterior ambience varies with time of day and season; rain, wind and thunder; audio settings | `HOUSE-01939` |
| **D8** | **A complete application.** Main and pause menus; settings (display, graphics, audio, controls, environment) that persist; credits showing `THIRD-PARTY-ASSETS.md`; optional session resume | `HOUSE-02528`, `HOUSE-02523`, `HOUSE-03571` |
| **D9** | **Performance within documented budgets.** Every performance scenario (≥ 1 per zone) meets the High budget on the reference machine; the Web and Android tiers meet theirs | `HOUSE-02404`, `HOUSE-02898`, `HOUSE-03037` |
| **D10** | **Platforms validated to the level realistically possible.** Linux: a packaged build passes on a clean profile. Web: the full walkthrough runs in Chrome and Firefox, and a headless smoke test runs in CI. Android: the walkthrough runs on ≥ 1 device or emulator with touch controls. If CNA's Android graphics path is still blocked upstream (BL-13), the blocker is re-verified with evidence and every desktop-side Android readiness criterion (`--force-touch` HUD, Android tier, lifecycle mapping) passes | `HOUSE-02790`, `HOUSE-02903`, `HOUSE-03040` or `HOUSE-02951` |
| **D11** | **Green gates.** Unit, integration, render and performance suites; XNA-only and strict-XNA gates; ASAN/UBSAN clean; a 2-hour soak without leak or crash | `HOUSE-02783`, `HOUSE-02784`, `HOUSE-03076` |
| **D12** | **Current documentation and licences.** `cna-house.md`, `README.md` and `docs/*` describe the shipped software; every asset is manifested and licensed | `HOUSE-02795`, `HOUSE-03073`, `HOUSE-03075` |
| **D13** | **What remains is genuinely minor.** Every open issue is S3/S4 polish, not a missing area or a missing core system | `HOUSE-03078` |

Tier E (compiled effects) is **optional** (M11b). DONE requires Tier S to be complete on every
platform, and any Tier E path that ships must have a proven Tier S fallback.

---

## Scheduling rules — time discipline

These rules exist because the project spent about 200 agent-hours polishing one ground-floor route
while four levels stayed empty. They **override task-number order**, and they override any "largest
visible defect" heuristic applied to a single area. `docs/workflow.md`'s "pick the next unfinished
task whose dependencies are complete" means: pick it **by these rules**.

**Tracks.** *Track A* is the breadth path M0–M6 (traversal, architecture, the furnishing kit,
dressing, lighting, showcase baseline). *Track B* is the systems work that interleaves with it:
M7 environment, M8 audio, M9 application shell, M10 performance, M12 tests, and the Web canary
(`HOUSE-02843`, `HOUSE-02855`). M11 polish, and M13–M16 (release and platforms), follow G5.

| Rule | Statement |
|---|---|
| **R1: one-level lead** | A task may raise a zone to completion level *k* only if **every** zone is already at level ≥ *k − 1*. While any zone is at C1, nobody works towards C3; while any zone is at C2, nobody works towards C4; and so on. Exceptions, and only these: (a) S1-severity defects, in any zone at any time; (b) house-wide system work (renderer, pipeline, kit, validator, tooling) whose acceptance is verified on ≥ 3 zones including the least complete; (c) a regression caused by the current task |
| **R2: least complete first** | Among eligible Track A tasks, pick the zone with the lowest level. Break ties by the most accessible cells still below the next level, then by the zone least recently worked. `Z-L0M` never wins a tie before G5 |
| **R3: consecutive cap** | At most **3 consecutive** Track A tasks on one zone, unless that zone is still *strictly* the least complete. After the cap, the next Track A task targets another zone |
| **R4: the ground-floor freeze** | Until **G4** passes, no task targets `Z-L0M` (foyer, hall, living, family, kitchen, dining, sunroom, butler's pantry, porch) except under R1's exceptions. Its S2/S3/S4 findings go to the polish backlog in `docs/visual-review/README.md`, not into the task queue. **A defect in a finished ground-floor room never outranks an unfinished floor.** Between G4 and G5, `Z-L0M` gets its C5 tasks in normal R2 order, like every other zone |
| **R5: track share** | Until G5, at least **two of every three** completed tasks are Track A. Track B fills the rest, MUST before SHOULD. If every eligible Track A task is blocked, record the blockers in the task entries; Track B may then proceed |
| **R6: bounded review** | A visual review round captures the zone sets it concerns and schedules fixes only for **S1/S2** defects in zones at or below the current target level. At most **2 consecutive** review→fix cycles may target one zone. S3 findings are scheduled only in that zone's C5 task or in M11; S4 findings only in M11. A round that finds nothing above S3 in a zone at its target level **stops** |
| **R7: every task pays its way** | Every new task states `adv:` (the DONE item or gate it advances) and `est:` (agent-hours). A task estimated above 4 h is split. A task that reaches **2× its estimate** stops, records what it learned in a `note:`, and splits the rest into a new task with a new estimate |
| **R8: kit first** | Furniture and fixtures come from the shared kit: the `HOUSE-00985` generator, the M3 acquisitions, and the existing ground-floor pieces reused. Bespoke per-object Blender authoring is limited to **≤ 2 hero pieces per zone**, each ≤ 3 h, named in the task |
| **R9: no new systems** | A new runtime subsystem, interaction or simulation requires a [Planning corrections](#planning-corrections) entry showing which DONE item cannot be met without it. "It would be nicer" is not a reason. Offline tooling that speeds up breadth work is allowed under R7 |
| **R10: reassess** | At every gate, and after every 10 completed tasks: update the [scoreboard](#zone-scoreboard) with evidence (a review round and `zone_scoreboard.py` output), compare hours spent with each milestone's budget, and record in `docs/handoff.md` which rule chose the next task. A milestone that overruns its budget by 50 % needs a Planning corrections entry deciding to cut, simplify or continue |
| **R11: unfinished beats finished** | When the choice is between completing an unfinished area and improving a finished one, complete the unfinished area. Do not invent scope |

**Gates.** A gate passes when its review task is ticked with the scoreboard as evidence.

| Gate | Passes when | Review task |
|---|---|---|
| **G1** Traversable | every zone ≥ C1 (no `*`) | `HOUSE-03240` |
| **G2** Architecture | every zone ≥ C2 | `HOUSE-03280` |
| **G3** Dressed | every zone ≥ C3 | `HOUSE-03380` |
| **G4** Lit | every zone ≥ C4 | `HOUSE-03420` |
| **G5** Showcase baseline | every zone ≥ C5 (DONE item D1) | `HOUSE-03480` |
| **DONE** | D1–D13 | `HOUSE-03078` |

---

## Completion levels and zones

### Completion levels

A zone's level is the highest *k* for which it meets C1 … C*k*, all of them.

| Level | Name | A zone at this level … |
|---|---|---|
| C0 | Absent | is missing, unreachable or not rendered |
| **C1** | Traversable | has every intended-accessible cell reached by the grand tour on foot; posed doors and gates block exactly where they are drawn; no stuck point, fall-through, or clipping through important geometry; collision matches what is drawn |
| **C2** | Architecture | shows finished primary architecture at eye height: production materials on floors, walls, ceilings and roofs; doors, windows, trim and hardware; stairs with handrails, balusters and nosings; zone-specific structure (attic rafters, insulation and walkway; unfinished-basement structure; garage slab and walls; garden structures); no blockout surface; no S1/S2 architectural defect |
| **C3** | Dressed (baseline) | gives every accessible room the primary furniture and fixtures of its room recipe (`HOUSE-03302`), so its purpose reads at a glance; placement is believable (validator green); exterior spaces have their primary objects; no room reads as empty |
| **C4** | Lit and finished (baseline) | has furniture in the baked lighting or the consistency solution (`HOUSE-03402`); no props reading visibly darker or brighter than the surfaces they stand on; every room readable at 10:30 clear and at night under its scheduled lights; exterior night lighting; no debug or placeholder material visible anywhere |
| **C5** | Showcase baseline | has fixed views (day, night, overcast) with no S1/S2 defect; secondary dressing in its rooms (textiles, wall art, shelf contents, curtains); a representative view within the High budget |
| C6 | Polished | has had the M11 polish rotation, which starts only after G5 |

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

**As of 2026-09-21 (`HOUSE-03201`).** Update per rule R10. The levels of never-reviewed zones are
provisional until `HOUSE-03203` captures them.

`*` means reachable, but pending the **house-wide door/gate defect**: every leaf is drawn *closed*,
yet nothing collides with it (`DynamicObstacles` is never filled) and `PortalRuntime` starts every
leafed portal closed. The player therefore walks through closed doors and gates. M1 fixes this for
every zone at once.

| Zone | Level | Props (133 total) | Review views (32 total) | Evidence | Largest gaps |
|---|---|---|---|---|---|
| `Z-B1` | **C1\*** | **0** in 14 cells | **0** | Flights walk both ways (`HOUSE-00615`). Every cell reachable (world rule 5). Materials and both bakes assigned (`HOUSE-00907`–`00910`) | Empty. Never reviewed. Generic slab doors. No services or structure dressing |
| `Z-L0M` | **C3** (C4 in progress) | 106 | 23 | 102 review rounds. Primary furniture in all 9 cells (`HOUSE-01037`–`01076`) | S2: Basic-lit furniture much darker than baked receivers (piano, sofas); service-room night depth. **Frozen by R4** |
| `Z-L0S` | **C1\*** | **0** in 8 cells | **0** | Shell, materials, bakes | Empty. Never reviewed |
| `Z-GAR` | **C1\*** | **0** in 2 cells | **0** (the approach view counts for `Z-EXF`) | Finished sectional door outside (`HOUSE-00935`, `00947`, `00948`) | 70.6 m² empty garage. The loft had no furnish task (now `HOUSE-03341`) |
| `Z-L1` | **C1\*** | **1** (front-balcony lantern) in 20 cells | 1 (balcony) | Reached on foot (`HOUSE-00489`) | 5 bedrooms, the master suite and 4 baths/WCs empty. Never reviewed |
| `Z-L2` | **C1\*** | **0** in 15 cells | **0** | Flights walkable | Library, games room, sitting room, 2 bedrooms and 4 baths/WCs empty. Never reviewed |
| `Z-L3` | **C1\*** | **0** in 5 cells | **0** | Automatic attic crouch (`HOUSE-00558`) | ≈ 258 m² of attic never seen. No structure dressing |
| `Z-STAIR` | **C1\*** | 0 | 2 | 8 flights walk up and down. Balustrade geometry and collision exist | S2: the main stair foot is very dark (Rounds 98, 102). Upper flights never reviewed |
| `Z-EXF` | **C4\*** | 11 | 4 | ≈ 25 facade, roof, entry and approach tasks. Property bot walk (`HOUSE-00782`) | Mailbox, bins and meters (`HOUSE-00770`). Gate leaves not solid (the `*`) |
| `Z-EXR` | **C2\*** (C3 partial) | 15 | 2 (+1 from inside the family room) | Terrace group, loungers, fire pit, swing bench, lanterns (`HOUSE-00771`, `01291`). Shed shell (`HOUSE-00768`). Beds and trellis (`HOUSE-00769`) | Shed interior, side yards, orchard and vegetable garden bare. Rear and side elevations less finished than the front |
| `Z-STR` | **C2** (C3 partial) | — | 0 dedicated | N1–N60 generated at LOD0–2 plus impostor cards, street furniture, barriers (`HOUSE-00841`–`00849`, `00856`, `00857`) | Impostor rendering (`HOUSE-00851`), neighbour lights (`00850`), parked cars (`00847`) |

**Minimum level today: C1\*** (seven zones). By rule R1, the only permitted zone-raising work is
M1 (to C1) and then M2 (to C2). M3 tooling may run alongside.

---

## Visual review protocol and defect severity

The review ledger stays in [`docs/visual-review/README.md`](docs/visual-review/README.md). Captures
stay local and Git-ignored. Strict golden references (`tests/render/reference/`) are a separate
thing: never refresh a golden as part of a review round unless the round's task changed that image
on purpose.

A round records: the zones captured (`HOUSE-03202` sets); every defect with its **severity** and
**zone**; the scoreboard change; and **the next task together with the rule (R1–R11) that chose
it**. The fixed ground-floor route is no longer the default review. Each round captures the zones it
concerns, and every gate review captures all zones on one contact sheet.

| Severity | Meaning | Examples | Scheduling |
|---|---|---|---|
| **S1 Blocker** | Breaks the walk or the image outright | crash; cannot reach an area; stuck or falling through a floor; a missing wall, floor or roof (a hole to the void); debug colours; content failing to load; a black or blown-out room at noon | Fix now, any zone, any time (R1 exception a) |
| **S2 Major** | Breaks believability at normal eye height | a room that reads empty or as the wrong type; wrong scale; floating or intersecting furniture; clipping through important geometry; large z-fighting; a missing texture; collision disagreeing with visible geometry; a room unreadable by day or at night; furniture far darker or brighter than its surroundings | Fix within the zone's current target level, subject to R1/R4 |
| **S3 Moderate** | Noticeable, but believability holds | visible tiling; imperfect colour or brightness balance; sparse secondary dressing; small seams; dull corners | Backlog. Scheduled only in the zone's C5 task or in M11 |
| **S4 Minor polish** | Visible only on close inspection | tiny gaps; low texture resolution on small props; hardware detail; subtle hue; micro-clutter | Backlog. M11 only. Never blocks a gate |

**Diminishing returns.** Once a zone meets its target level with no S1/S2 open, further findings
there are logged and **not** scheduled until the rules allow that zone's next level, or M11. Two
consecutive rounds on one zone (R6) is the limit.

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
| `dep` | Tasks that must be complete first. A dependency on a **cancelled** task is satisfied by the cancellation; a range `A…B` means the non-cancelled tasks in it. `—` means none |
| `sys` | Owning subsystem, matching `src/` or `tools/` |
| `plat` | `LNX`, `WEB`, `AND`, `ALL`, `TOOL`, `CI` |
| `pri` | `MUST` (required for DONE), `SHOULD` (wanted, cut first under R10), `OPT` (only after DONE) |
| `zone` | The zone a Track A task raises (rule R2/R3). `all` for house-wide work |
| `adv` | The DONE item or gate the task advances (rule R7). Required on every new task |
| `est` | Estimated agent-hours (rule R7). Required on every new task |
| `amended:` | A 2026-09-21 change to a carried task's scope, dependency or priority, with the reason. The original text above it is unchanged |

**Ids.** Carried tasks keep their legacy ids. New work takes the next free id in its **milestone's**
reserved range (milestones are this plan's phases for that rule). Cancelled ids are listed in
[Cancelled by the scope reduction](#cancelled-by-the-scope-reduction) and struck through, with the
reason, in the legacy ledger. **No id is ever reused.**

**Carried tasks read as written, plus their amendments.** Their original notes may still say
"phase N" and refer to legacy phases. The milestone they now sit in governs their scheduling.

---

## Milestone index and ID ranges

| Milestone | Track | ID range (new tasks) | Open tasks | Gate / exit | Advances | Budget (agent-h) |
|---|---|---|---|---|---|---|
| [M0](#m0--scope-reset-and-breadth-instruments) — Scope reset and breadth instruments | A (support) | 03201–03220 | 3 | review instruments ready | R10, G1–G5 | 8 |
| [M1](#m1--whole-property-traversal-c1-everywhere--gate-g1) — Whole-property traversal | A | 03221–03260 | 8 | **G1** · `HOUSE-03240` | D2 | 23 |
| [M2](#m2--architectural-completion-c2-everywhere--gate-g2) — Architectural completion | A | 03261–03300 | 9 | **G2** · `HOUSE-03280` | D1 | 32 |
| [M3](#m3--furnishing-toolkit) — Furnishing toolkit | A (support) | 03301–03340 | 17 | kit ready for M4 | D3 | 58 |
| [M4](#m4--baseline-dressing-everywhere-c3--gate-g3) — Baseline dressing everywhere | A | 03341–03400 | 42 | **G3** · `HOUSE-03380` | D1, D3 | 110 |
| [M5](#m5--baseline-materials-and-lighting-everywhere-c4--gate-g4) — Baseline materials and lighting everywhere | A | 03401–03440 | 22 | **G4** · `HOUSE-03420` | D6 | 58 |
| [M6](#m6--showcase-baseline-everywhere-c5--gate-g5) — Showcase baseline everywhere | A | 03441–03500 | 26 | **G5** · `HOUSE-03480` | D1, D3 | 80 |
| [M7](#m7--environment-systems) — Environment systems | B | 03501–03540 | 66 | `HOUSE-03520` | D5 | 80 |
| [M8](#m8--atmospheric-audio) — Atmospheric audio | B | 03541–03570 | 28 | `HOUSE-01939` | D7 | 35 |
| [M9](#m9--application-shell-settings-menus-session-debug) — Application shell | B | 03571–03600 | 26 | `HOUSE-02528` | D8 | 34 |
| [M10](#m10--performance-and-budgets-measurement-driven) — Performance and budgets | B | 03601–03630 | 38 | `HOUSE-02404` | D9 | 42 |
| [M11](#m11--whole-project-polish-c6--after-g5-only) — Whole-project polish (+ optional M11b Tier E) | after G5 | 03631–03680 | 27 | `HOUSE-02714` | D4 | 55 |
| [M12](#m12--test-completion) — Test completion | B | 03681–03700 | 23 | `HOUSE-02600` | D11 | 33 |
| [M13](#m13--linux-desktop-release) — Linux desktop release | after M11 | 03701–03720 | 17 | `HOUSE-02797` | D9–D12 | 26 |
| [M14](#m14--web) — Web | after M13 (canary: B) | 03721–03750 | 30 | `HOUSE-02905` | D10 | 58 |
| [M15](#m15--android) — Android | after M13 (desktop readiness: B) | 03751–03780 | 33 | `HOUSE-03042` or BL-13 record | D10 | 60 |
| [M16](#m16--final-release) — Final release | last | 03781–03800 | 9 | **DONE** · `HOUSE-03078` | D11–D13 | 19 |

Legacy ids 03121–03200 are unallocated and stay unused.

---

## M0 — Scope reset and breadth instruments

Track A support. Gives every zone the review instruments the ground-floor route had, before anyone
works on the zones. The Web canary (`HOUSE-02843`, `HOUSE-02855` in M14) may be scheduled any time as
Track B. It is cheap risk reduction for D10.

- [x] HOUSE-03201 — Scope reduction: rewrite the plan around the architectural showcase, archive the legacy ledger, record ADR-0014
      dep: — · sys: — · plat: ALL · pri: MUST · zone: all · adv: all · est: 6
      files: plan.md, docs/history/plan-legacy-2026-09-21.md, docs/decisions/ADR-0014-showcase-scope.md (+ status pointers in ADR-0005/0008/0010 and the ADR index), cna-house.md, README.md, CLAUDE.md, AGENTS.md, docs/workflow.md, docs/versioning.md, docs/conventions.md, docs/handoff.md, docs/visual-review/README.md
      accept: (1) every task open on 2026-09-21 is carried, deferred or cancelled exactly once, each cancellation with its reason; (2) DONE, the scheduling rules, the zones, the completion levels and the gates are defined; (3) every document that contradicted the new scope points at ADR-0014; (4) no runtime code, data or asset changed
      verify: `tools/ci/run_checks.sh`; the builder's audit (all 764 legacy open ids accounted for once); `git diff --stat` shows documentation only
      note: (2026-09-21) the legacy `plan.md` moved to `docs/history/plan-legacy-2026-09-21.md`. Git records the move as a copy, so `git blame -C` keeps every line's history. Cancelled entries there are struck through with a `cancelled:` line; carried entries carry a `moved:` line pointing here.

- [ ] HOUSE-03202 — Give every zone a fixed review view set and a whole-property contact sheet
      dep: HOUSE-03201 · sys: tools · plat: TOOL · pri: MUST · zone: all · adv: G1–G5 · est: 3
      files: tools/visual/capture_review.py, docs/visual-review/README.md
      accept: (1) each of the 11 zones has ≥ 4 fixed eye-height poses (at least one per room type present in the zone; `Z-STAIR` has one per flight group), framed on the room, not a wall; (2) `--zone <Z-…>` captures one zone and `--all-zones` captures them all, in `clear-day` and `clear-night`; (3) one labelled contact sheet per zone is written beside the captures; (4) the 32 existing poses are kept unchanged and assigned to their zones
      verify: run `--all-zones` in both scenarios; inspect every contact sheet

- [ ] HOUSE-03203 — Baseline review of the whole property: level every zone with evidence and seed the defect lists
      dep: HOUSE-03202, HOUSE-03204 · sys: — · plat: LNX · pri: MUST · zone: all · adv: R10 · est: 3
      accept: (1) one ledger round covers all 11 zones, day and night; (2) every observed defect is classified S1–S4 and filed under its zone; (3) the scoreboard levels are confirmed or corrected with the round and `zone_scoreboard.py` evidence, and "provisional" is removed; (4) each M2 zone task gets its concrete S1/S2 list as a `note:`
      verify: the ledger round; the updated scoreboard

- [ ] HOUSE-03204 — `tools/world/zone_scoreboard.py` and `docs/zones.json`: objective per-zone completeness numbers
      dep: HOUSE-03201 · sys: tools · plat: TOOL · pri: MUST · zone: all · adv: R10 · est: 2
      files: docs/zones.json, tools/world/zone_scoreboard.py, tools/ci/run_checks.sh
      accept: (1) `docs/zones.json` puts each of the 96 cells in exactly one zone, or in an explicit `none` group with a reason; (2) the tool prints per zone: cells, accessible cells, props, props per accessible cell, cells with zero props, cells with ≥ 1 light group, and review poses in the zone; (3) `--check` fails on a cell in zero or two zones and runs in `run_checks.sh`
      verify: run it; paste its table into the scoreboard

---

## M1 — Whole-property traversal (C1 everywhere) · gate G1

Track A. **The one gameplay acceptance criterion:** the player reliably walks through and inspects
every intended area without getting stuck, clipping through important geometry, or meeting broken
navigation. No interaction framework is built for doors (ADR-0014).

- [ ] HOUSE-03221 — Author a static pose for every door leaf, gate and the garage door
      dep: HOUSE-03201 · sys: world · plat: TOOL · pri: MUST · zone: all · adv: D2, G1 · est: 3
      files: assets-src/world/*.json (the chosen home), tools/world/world_schema.py, tools/world/validate_world.py, docs/world-format.md
      accept: (1) every leafed portal (63 doors, 3 gates, the garage door) has an authored static open fraction; (2) interior doors default to resting open (≥ 0.85) on their swing side, and a door stays closed only where no intended-accessible space lies behind it, each such case listed; (3) every door on an accessible route leaves ≥ 0.70 m clear width; (4) the validator rejects a missing pose, a closed door on an accessible route, and a swing arc that intersects walls or placed props; (5) the schema home is recorded in `docs/world-format.md`. Reusing `openFraction` from the existing door rows and `initialstate.json` is allowed; no behaviour code is added
      verify: `validate_world.py` (new rule); `--selftest` cases for each rejection
      note: today every leaf is drawn closed while nothing collides with it and culling treats it as closed. This is the data half of the fix.

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
      dep: HOUSE-03226 · sys: physics · plat: LNX · pri: MUST · zone: all · adv: D2, G1 · est: 3
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
that is really dressing (ducts, pipes, boxes) belongs to M4.

- [ ] HOUSE-03261 — Bring the basement's architecture to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G2, D1 · est: 4
      accept: C2 for all 14 `Z-B1` cells: finished rooms (cinema, gym, hobby, WC, laundry) and unfinished rooms (mechanical, electrical, utility, workshop, storage, cellar) read as such through their finishes; exposed structure (joists, columns, a slab edge) where unfinished, from the shell generator or `prop_kit_gen.py`, whichever is cheaper; door leaves have casing and hardware; basement windows or light wells as designed; the zone's S1/S2 architecture findings closed
      verify: `Z-B1` zone capture, day and night; `verify_shell.py`; grand tour

- [ ] HOUSE-03262 — Bring the ground-floor service rooms and the garage to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-L0S, Z-GAR · adv: G2, D1 · est: 3
      accept: C2 for `Z-L0S` and `Z-GAR`: the garage interior's slab, walls, ceiling and the inside face of the sectional door read as finished garage construction; the loft has a real means of access as designed and a guard; mudroom, laundry, WCs, pantry, office and closets have finished trim and door hardware
      verify: `Z-L0S` and `Z-GAR` captures; grand tour

- [ ] HOUSE-03263 — Bring the first upper floor's architecture to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G2, D1 · est: 3
      accept: C2 for all 20 `Z-L1` cells: bedroom, bathroom, closet and corridor finishes match their palette; window reveals, sills and interior trim finished; door joinery at least at the `HOUSE-00942` standard on landing- and hall-facing doors; balconies' guards and finishes consistent with the front balcony
      verify: `Z-L1` capture; grand tour

- [ ] HOUSE-03264 — Bring the second upper floor's architecture to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G2, D1 · est: 3
      accept: as `HOUSE-03263`, for the 15 accessible `Z-L2` cells; the library's shelving walls are left to M4 unless the shell carries them
      verify: `Z-L2` capture; grand tour

- [ ] HOUSE-03265 — Bring the attic's architecture to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-L3 · adv: G2, D1 · est: 4
      accept: C2 for the 5 `Z-L3` cells: visible roof structure in the stores (rafters, ridge, collar ties or purlins), insulation between joists where unfinished, a boarded walkway through the stores, dormer reveals, knee walls and a finished ceiling in `L3_ROOM`; the crouch zones read as low headroom rather than as a collision surprise
      verify: `Z-L3` capture; grand tour (crouch)

- [ ] HOUSE-03266 — Bring the vertical circulation to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-STAIR · adv: G2, D1 · est: 3
      accept: all 8 flights have handrails, balusters, newels, nosings, stringers and landing trim consistent with their stair's character (the main stair finished; the basement and attic stairs plainer); no flight reads as a ramp or a solid mass from its foot or its head
      verify: `Z-STAIR` capture from every foot and head; stair traversal tests

- [ ] HOUSE-03267 — Bring the rear and side elevations up to the front elevation's architectural standard
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-EXR · adv: G2, D1 · est: 3
      accept: from anywhere accessible in the garden and side yards: window grilles and shutters (or their deliberate absence) are consistent with `HOUSE-00933`; sills, trim, corner boards, roof edges and rainwater goods match `HOUSE-00934`/`00944`; rear doors are finished joinery; the shed's exterior reads as finished
      verify: `Z-EXR` capture, day and night

- [ ] HOUSE-01197 — Author and place the door and window hardware props (handles, latches, hinges, sash locks)
      dep: HOUSE-00378 · sys: world · plat: TOOL · pri: MUST

- [ ] HOUSE-03280 — **Gate G2 review: every zone at C2**
      dep: HOUSE-03261, HOUSE-03262, HOUSE-03263, HOUSE-03264, HOUSE-03265, HOUSE-03266, HOUSE-03267 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G2 · est: 1.5
      accept: an all-zone capture round with no S1/S2 architecture defect open; the scoreboard shows every zone ≥ C2; the handoff records the gate
      verify: ledger round; scoreboard

---

## M3 — Furnishing toolkit

Track A support: house-wide tooling, allowed at any time by R1 exception (b). It exists to make
furnishing ≈ 60 empty rooms tractable. Before this plan, every ground-floor piece was authored with
its own bespoke Blender and prepare script (`tools/assets/*_prepare.py`, `tools/blender/*.py`), and
each one added its own `--check` gate to `run_checks.sh` (28 such gates today). That cannot scale to
60 rooms. Kit pieces are generated or acquired in groups and validated by shared gates. **Budget discipline:** acquisition groups deliver what the room
recipes need, not the legacy counts.

- [ ] HOUSE-03301 — Stop furnishing from paying for the pet navigation graph
      dep: HOUSE-03201 · sys: world · plat: TOOL · pri: MUST · zone: all · adv: D3 (cost) · est: 1.5
      files: tools/world/validate_world.py, tools/ci/run_checks.sh, src/world/WorldLoader.cpp (only if it rejects a stale graph)
      accept: (1) no gate or world rule requires `layout.nav.json`'s waypoints, perches, beds or bowls to stay consistent with props, so placing furniture never requires moving a pet waypoint again; (2) the data files and loader stay until the cleanup candidates are actioned: this task removes the coupling only; (3) the gates and unit tests are green
      verify: `run_checks.sh`; a probe prop placed across an old waypoint passes validation
      note: `HOUSE-01074` had to shift two sunroom pet waypoints to place two chairs. That cost now buys nothing.

- [ ] HOUSE-03302 — Room recipes and the furnishing kit catalogue
      dep: HOUSE-03201 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3, G3 · est: 3
      files: docs/furnishing-kit.md
      accept: (1) a recipe for every room type in the house (master, double, child's, teenager's, guest and sewing bedrooms; bathroom; WC; walk-in closet; linen and storage closets; study; library; games room; sitting room; mudroom; laundry; pantry; garage; garage loft; basement hall; mechanical; electrical; utility; cinema; gym; workshop; storage; wine cellar; hobby room; attic room; attic store; attic services; shed; stair landing; balcony), each listing its **primary (C3)** and **secondary (C5)** pieces with typical dimensions and clearances; (2) a catalogue of existing reusable pieces (the ground-floor kit from `HOUSE-01037`–`01076`) and the recipes they serve; (3) the reuse limits of `HOUSE-00972` and the hero-piece rule R8; (4) the needed count per M3 acquisition group, derived from the recipes
      verify: review against `layout.cells.json`: every accessible cell maps to a recipe

- [ ] HOUSE-03303 — Placement validator for static props
      dep: HOUSE-03225 · sys: world · plat: TOOL · pri: MUST · zone: all · adv: D2, D3 · est: 3
      files: tools/world/validate_world.py (or a sibling checker run by it), tests
      accept: every row in `layout.props.json` (1) rests on a floor or support surface within 1 cm; (2) lies inside its cell; (3) does not intersect walls, openings or other props' collision proxies beyond a stated tolerance; (4) keeps door-swing arcs, window fronts and a ≥ 0.70 m circulation path clear; the check runs in `run_checks.sh`, and the grand tour remains the end-to-end proof
      verify: `--selftest` cases for each rejection; the current 133 props pass or are fixed

- [ ] HOUSE-00971 — Implement the placement pipeline: a prop row in `layout.props.json` → a chunk entry or a dynamic instance, with per-instance jitter and tint
      dep: HOUSE-00215, HOUSE-00891 · sys: content · plat: TOOL · pri: MUST

- [ ] HOUSE-00972 — Implement the anti-repetition validator (≤ 14 uses per model overall, ≤ 6 per cell, whitelist)
      dep: HOUSE-00971 · sys: world · plat: TOOL · pri: MUST

- [ ] HOUSE-00973 — Implement the fill-kit generator: `FILL_CUTLERY`, `FILL_TOWELS`, `FILL_TOOLS`, `FILL_TSHIRTS`, `FILL_PAPERS`, `FILL_BOOKS`, `FILL_CROCKERY`, `FILL_TOYS`, `FILL_PAINT`, `FILL_JARS`, `FILL_SHOES`, `FILL_LINEN`
      dep: HOUSE-00971 · sys: content · plat: TOOL · pri: MUST
      accept: seeded, varied count/spacing/lean/colour; two invocations never look identical
      amended: (2026-09-21, `HOUSE-03201`) fill kits dress open shelves, visible surfaces and the basement, attic and garage storage. Container interiors are out of scope because containers are removed

- [ ] HOUSE-00985 — `prop_kit_gen.py`: generate cabinet carcasses, shelving, boxes, radiators, ducts, pipe runs and wire runs from data
      dep: HOUSE-00971 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) this is the main breadth enabler: generated carcasses, shelving, boxes, radiators, ducts, pipe and wire runs serve the basement, attic, garage and every storage room

- [ ] HOUSE-00975 — Grouped: acquire and prepare the 24 kitchen appliance and fixture models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) static visual props only (closed doors, no interiors, no moving parts), each with a collision proxy and a manifest row. The stated count is an upper bound: acquire, generate (`HOUSE-00985`) or reuse what the room recipes of `HOUSE-03302` actually need
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`

- [ ] HOUSE-00976 — Grouped: acquire and prepare the 16 bathroom fixture models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) static visual props only (closed doors, no interiors, no moving parts), each with a collision proxy and a manifest row. The stated count is an upper bound: acquire, generate (`HOUSE-00985`) or reuse what the room recipes of `HOUSE-03302` actually need
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`

- [ ] HOUSE-00977 — Grouped: acquire and prepare the 20 seating models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) static visual props only (closed doors, no interiors, no moving parts), each with a collision proxy and a manifest row. The stated count is an upper bound: acquire, generate (`HOUSE-00985`) or reuse what the room recipes of `HOUSE-03302` actually need
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`

- [ ] HOUSE-00978 — Grouped: acquire and prepare the 14 table and desk models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) static visual props only (closed doors, no interiors, no moving parts), each with a collision proxy and a manifest row. The stated count is an upper bound: acquire, generate (`HOUSE-00985`) or reuse what the room recipes of `HOUSE-03302` actually need
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`

- [ ] HOUSE-00979 — Grouped: acquire and prepare the 12 bed and bedding models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) static visual props only (closed doors, no interiors, no moving parts), each with a collision proxy and a manifest row. The stated count is an upper bound: acquire, generate (`HOUSE-00985`) or reuse what the room recipes of `HOUSE-03302` actually need
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`

- [ ] HOUSE-00980 — Grouped: acquire and prepare the 22 storage-furniture models, plus the generated cabinet carcasses and shelving
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) static visual props only (closed doors, no interiors, no moving parts), each with a collision proxy and a manifest row. The stated count is an upper bound: acquire, generate (`HOUSE-00985`) or reuse what the room recipes of `HOUSE-03302` actually need
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`

- [ ] HOUSE-00981 — Grouped: acquire and prepare the 26 small-appliance and lamp models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) static visual props only (closed doors, no interiors, no moving parts), each with a collision proxy and a manifest row. The stated count is an upper bound: acquire, generate (`HOUSE-00985`) or reuse what the room recipes of `HOUSE-03302` actually need
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`

- [ ] HOUSE-00982 — Grouped: acquire and prepare the 55 decoration models (rugs, curtains, mirrors, clocks, vases, plants, frames)
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) static visual props only (closed doors, no interiors, no moving parts), each with a collision proxy and a manifest row. The stated count is an upper bound: acquire, generate (`HOUSE-00985`) or reuse what the room recipes of `HOUSE-03302` actually need
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`

- [ ] HOUSE-00983 — Grouped: acquire and prepare the 40 kitchenware and food prop models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) static visual props only (closed doors, no interiors, no moving parts), each with a collision proxy and a manifest row. The stated count is an upper bound: acquire, generate (`HOUSE-00985`) or reuse what the room recipes of `HOUSE-03302` actually need
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`

- [ ] HOUSE-00984 — Grouped: acquire and prepare the 45 box, crate, bin, tool and clutter models
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) static visual props only (closed doors, no interiors, no moving parts), each with a collision proxy and a manifest row. The stated count is an upper bound: acquire, generate (`HOUSE-00985`) or reuse what the room recipes of `HOUSE-03302` actually need
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00296`

---

## M4 — Baseline dressing everywhere (C3) · gate G3

Track A. Every accessible room gets its recipe's **primary** furniture and fixtures, and nothing
more yet. Work the zones **least complete first** (R2), at most three consecutive tasks per zone
(R3). The expected order today: `Z-L1` → `Z-L2` → `Z-B1` → `Z-L3` → `Z-L0S` → `Z-GAR` →
`Z-STAIR` → `Z-EXR` → `Z-STR` → `Z-EXF`. `Z-L0M` is already at C3 and gets **no** M4 task (R4).
Every task below depends on gate G2, the placement pipeline, the recipes and the validator. Each one
keeps the grand tour green.

### `Z-L1` — first upper floor

- [ ] HOUSE-00999 — Furnish `L1_LANDING` and `L1_HALL`, `L1_HALL_W`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00982 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`

- [ ] HOUSE-01000 — Furnish `L1_MASTER_BED`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00979, HOUSE-00980 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00979`

- [ ] HOUSE-01001 — Furnish `L1_MASTER_BATH` and `L1_MASTER_CLOSET`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00976, HOUSE-00980 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00976`

- [ ] HOUSE-01002 — Furnish `L1_BED2` (double, desk, wardrobe)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00978, HOUSE-00979, HOUSE-00980 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00979`

- [ ] HOUSE-01003 — Furnish `L1_BED3` (child's room)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00979, HOUSE-00980, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00979`

- [ ] HOUSE-01004 — Furnish `L1_BED4` (teenager's room, deliberately untidy)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00978, HOUSE-00979 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00979`

- [ ] HOUSE-01005 — Furnish `L1_BED5` (guest, made up, unused)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00979 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00979`

- [ ] HOUSE-01006 — Furnish `L1_BATH2`, `L1_BATH3`, `L1_WC3`, `L1_WC4`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00976 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00976`

- [ ] HOUSE-01007 — Furnish `L1_LINEN`, `L1_STOR`, `L1_CLOSET_2`, `L1_CLOSET_3`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00980 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00973`

- [ ] HOUSE-01008 — Furnish `L1_BALCONY_REAR` and `L1_BALCONY_FRONT`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00771 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00771`

### `Z-L2` — second upper floor

- [ ] HOUSE-01009 — Furnish `L2_LANDING`, `L2_HALL`, `L2_HALL_W`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00982 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00999`

- [ ] HOUSE-01010 — Furnish `L2_LIBRARY` (floor-to-ceiling shelves, ladder, reading chairs)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00977, HOUSE-00980 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00980`

- [ ] HOUSE-01011 — Furnish `L2_GAMES` (pool table, dartboard, arcade cabinet, jigsaw)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00978 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00977`

- [ ] HOUSE-01012 — Furnish `L2_SITTING` (record player, plants)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00981 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00977`

- [ ] HOUSE-01013 — Furnish `L2_BED6` (sewing room) and `L2_BED7`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00978, HOUSE-00979 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00979`

- [ ] HOUSE-01014 — Furnish `L2_BATH4`, `L2_BATH5`, `L2_WC5`, `L2_WC6`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00976 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00976`

- [ ] HOUSE-01015 — Furnish `L2_STOR2`, `L2_CLOSET_4`, `L2_LINEN2`, `L2_STAIR_ATTIC`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00973`

### `Z-B1` — basement

- [ ] HOUSE-01020 — Furnish `B1_HALL`, `B1_STAIR`, `B1_UNDERSTAIR`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00984`

- [ ] HOUSE-01021 — Furnish `B1_MECHANICAL` (furnace, air handler, water heater, expansion tank, water main) from the plumbing/HVAC data
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00985, HOUSE-00386, HOUSE-00387 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) the equipment is static; the plumbing/HVAC data only positions it
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00386, HOUSE-00387`

- [ ] HOUSE-01022 — Furnish `B1_ELECTRICAL` and `B1_UTILITY`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01021`

- [ ] HOUSE-01023 — Furnish `B1_CINEMA` (projector, screen, 6 seats, acoustic panels) and `B1_WC7`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00976, HOUSE-00977 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) the projector and screen are static; there is no playback
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00977`

- [ ] HOUSE-01024 — Furnish `B1_GYM` and `B1_WORKSHOP`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00984`

- [ ] HOUSE-01025 — Furnish `B1_STOR1`, `B1_STOR2`, `B1_CELLAR`, `B1_LAUNDRY2`, `B1_HOBBY`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00980, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00984`

### `Z-L3` — attic

- [ ] HOUSE-01016 — Furnish `L3_ROOM` (the finished attic: old sofa, desk, boxes, rocking horse, train set)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00984`

- [ ] HOUSE-01017 — Furnish `L3_STORE_W` (40 boxes, wardrobe, suitcases, insulation, walkway, the spider's web)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) the spider's web is optional dressing, not a requirement
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00984`

- [ ] HOUSE-01018 — Furnish `L3_STORE_E` (header tank, ducts, aerial mast, cable runs)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00985`

- [ ] HOUSE-01019 — Furnish `L3_STORE_N`, `L3_STORE_S`, `L3_STAIR_HEAD`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00984`

### `Z-L0S` — ground-floor service rooms

- [ ] HOUSE-00991 — Furnish `L0_PANTRY` and `L0_BUTLERS`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00975, HOUSE-00980, HOUSE-00973 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00990`

- [ ] HOUSE-00994 — Furnish `L0_OFFICE`, `L0_CLOSET_W`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00978, HOUSE-00980 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`

- [ ] HOUSE-00995 — Furnish `L0_MUDROOM`, `L0_LAUNDRY`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00975, HOUSE-00980, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`

- [ ] HOUSE-00996 — Furnish `L0_WC1`, `L0_WC2`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00976 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00976`

- [ ] HOUSE-00997 — Furnish `L0_STOR`, `L0_STAIR_MAIN`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`

### `Z-GAR` — garage

- [ ] HOUSE-00998 — Furnish `L0_GARAGE` (car, workbench, shelving, tools, bikes, bins, 28 containers, the door mechanism)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) no containers and no door mechanism (both removed). The household car is placed when `HOUSE-01036` lands; the garage reaches C3 without it
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01036, HOUSE-00984`

- [ ] HOUSE-03341 — Furnish `L0_GARAGE_LOFT`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST · zone: Z-GAR · adv: G3, D3 · est: 2
      accept: C3 for the loft (no legacy furnish task covered it): long-term storage of boxes, seasonal decorations, spare lumber and a garden-furniture stack from the kit, clear of the access and guard
      verify: `Z-GAR` capture; placement validator; grand tour

- [ ] HOUSE-01035 — Author the unbranded player-car model, LOD1/LOD2 and collision proxy to the fixed acceptance bar; complete the four-view visual sign-off
      dep: HOUSE-00293, HOUSE-00298 · sys: content · plat: TOOL · pri: SHOULD
      note: (2026-09-13) **New tasks `HOUSE-01035` and `HOUSE-01036`, the next free ids in phase
            13's reserved 00971–01120 range.** `HOUSE-00293` exhausted the three R-03 sources and
            selected a project-authored, unbranded 4.65 × 1.84 × 1.48 m five-door estate. A hero
            source model cannot be hidden inside `HOUSE-00998`'s 55-prop room-furnishing scope.
      accept: LOD0 ≤ 45,000 triangles; real-scale exterior and simplified visible cabin; clean
            winding, normals and topology; support-point origin; four reference-photo comparisons
            in the actual garage lighting; the car and all parts remain static
      amended: (2026-09-21, `HOUSE-03201`) priority MUST → SHOULD and time-boxed to 8 agent-hours. It is a static garage/driveway prop, not a hero asset. If the box runs out, a simpler cleanly licensed or generated car at the same scale satisfies `HOUSE-00998`'s note

- [ ] HOUSE-01036 — Export, manifest, validate and build the authored player car through the real `.glb`/`.cnb` pipeline
      dep: HOUSE-01035, HOUSE-00220, HOUSE-00222 · sys: content · plat: TOOL · pri: SHOULD
      accept: `gltf_validate.py`, `scale_check.py`, `origin_check.py`, the manifest/licence gates and
            the content build pass for all LOD and collision outputs
      amended: (2026-09-21, `HOUSE-03201`) priority MUST → SHOULD with `HOUSE-01035`

### `Z-STAIR`, `Z-EXR`, `Z-STR`, `Z-EXF`

- [ ] HOUSE-03343 — Dress the stair halls and landings
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303 · sys: world · plat: TOOL · pri: MUST · zone: Z-STAIR · adv: G3, D3 · est: 2
      accept: C3 for the six stair cells: stair-wall frames where the recipe places them, a runner where the finish schedule has one, and landing pieces (the `L1` landing window seat); nothing narrows a flight below its walkable width
      verify: `Z-STAIR` capture; stair traversal tests; grand tour

- [ ] HOUSE-01026 — Furnish `EXT_SHED` (tools, mower, pots, bags, bench)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C3 (dressed baseline)** for the named cells: (1) the primary furniture and fixtures of the cell's room recipe (`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`) stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first: bespoke per-object authoring only for the zone's ≤ 2 hero pieces (rule R8). Secondary dressing is C5 work (`HOUSE-03441`–`HOUSE-03451`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00984`

- [ ] HOUSE-03342 — Dress the side yards, the orchard corner and the vegetable garden
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03303, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST · zone: Z-EXR · adv: G3, D3 · est: 2
      accept: C3 for `EXT_SIDEYARD_W`, `EXT_SIDEYARD_E`, `EXT_ORCHARD` and `EXT_GARDEN`: service-side objects (AC condenser and meters if not placed by `HOUSE-00770`, a firewood store, a hose reel, a wheelbarrow, garden tools, a compost area, a potting bench and a rain barrel) placed with purpose and collision
      verify: `Z-EXR` capture; placement validator; grand tour

- [ ] HOUSE-00770 — Place the exterior props: mailbox, bins ×3, hose reel, AC condenser, gas meter, water tap, downspout splash blocks
      dep: HOUSE-00764, HOUSE-03280, HOUSE-03303 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) all static props with collision proxies; dependency on gate G2 and the placement validator added (breadth rule R1)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00764`

- [ ] HOUSE-00847 — Place the 3 parked neighbour cars and the delivery van
      dep: HOUSE-00293, HOUSE-03280, HOUSE-03303 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) static parked vehicles; any cleanly licensed or generated low-detail car/van is acceptable at street distance
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00293`

- [ ] HOUSE-03380 — **Gate G3 review: every zone at C3**
      dep: HOUSE-00991, HOUSE-00994…HOUSE-01026, HOUSE-00770, HOUSE-00847, HOUSE-03341, HOUSE-03342, HOUSE-03343 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G3 · est: 1.5
      accept: an all-zone capture round and `zone_scoreboard.py` show no accessible room without its primary pieces (zero-prop accessible rooms = 0, except named empty-by-design spaces); no S1/S2 dressing defect open; the scoreboard shows every zone ≥ C3
      verify: ledger round; scoreboard; `zone_scoreboard.py`

---

## M5 — Baseline materials and lighting everywhere (C4) · gate G4

Track A. Materials and both bakes already cover the whole shell (`HOUSE-00907`–`00912`). What
remains: bring the dressing into the lighting, fix the object-versus-bake mismatch **once, for the
whole house**, and light every zone by day and night. The ground-floor route's S2 lighting list is
closed here by `HOUSE-03402`/`HOUSE-03407`, after the other zones are dressed, not before.

- [ ] HOUSE-03401 — Automatic interior lighting schedule ("an occupied house")
      dep: HOUSE-03201 · sys: lighting · plat: ALL · pri: MUST · zone: all · adv: D6 · est: 3
      files: src/lighting/LightingSystem.cpp|hpp (or a small `LightSchedule`), assets-src/world/layout.lights.json, tests
      accept: (1) a data-driven schedule per light group, keyed to the sun and clock: living spaces on from dusk until a late hour; bedroom lamps in the evening; circulation lights on at night; bathrooms, closets and storage off unless the schedule names them; (2) seeded, deterministic per-group offsets so windows do not switch in unison; (3) `--light-on/--light-off` and `light <group>` (`HOUSE-01272`) still override; (4) the review capture at night uses the schedule, not hand-picked flags; (5) no switch plate, prompt or interaction code
      verify: unit LightScheduleTests.* (times, determinism, override); night zone captures
      note: until now interior lights change only through `--light-on`. A night walkthrough is dark indoors except for dusk-sensor fixtures.

- [ ] HOUSE-03402 — Light static props consistently with their baked receivers, house-wide
      dep: HOUSE-03380 · sys: rendering · plat: ALL · pri: MUST · zone: all · adv: D6, G4 · est: 4
      files: src/rendering/*, src/lighting/*, tools/blender/lightmap_bake.py (as chosen)
      accept: (1) choose, by matched captures, between props receiving per-cell irradiance from the bake (probe or sampled lightmap), baked prop lightmaps, or baked vertex occlusion, and record the decision with its numbers; (2) Basic-lit furniture no longer reads markedly darker or warmer than the floor and wall it stands against: measured luminance ratios within a stated band on ≥ 1 view per zone, day and night; (3) the rejected global-gain probes of `HOUSE-01076` stay rejected; outdoor props keep their calibration
      verify: matched before/after captures of ≥ 1 view per zone; the ratio measurement; render goldens refreshed only where inspected
      note: generalises Round 102's top ground-floor defect (piano, sofas, sunroom chairs) to the whole house. With every zone dressed, this is a system fix, not a room fix (R1 exception b).

- [ ] HOUSE-01029 — Rebuild chunks over the furnished house; verify the ≤ 6-chunks-per-cell target still holds
      dep: HOUSE-03380, HOUSE-00473 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) runs after gate G3 (every zone dressed), not after full density
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01028, HOUSE-00473`

- [ ] HOUSE-01030 — Re-bake lightmaps over the furnished house (furniture casts and receives baked light)
      dep: HOUSE-01029, HOUSE-00909 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01029, HOUSE-00909`

- [ ] HOUSE-00913 — Fix lightmap seams and gutter bleed found on inspection
      dep: HOUSE-01030 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) seams are fixed on the furnished re-bake (`HOUSE-01030`) so the fix is not undone by it
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00912`

- [ ] HOUSE-03403 — Light the basement, the attic and the stairwells to C4
      dep: HOUSE-03402, HOUSE-01030, HOUSE-03401 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-B1, Z-L3, Z-STAIR · adv: G4, D6 · est: 4
      accept: C4 for `Z-B1`, `Z-L3` and `Z-STAIR`: each room and flight has physical fixtures on its authored groups; windowless rooms are readable under their scheduled lights and not black; the attic reads by dormer daylight plus bare-bulb fixtures; **the main stair foot's S2 darkness (Rounds 98, 102) is closed**
      verify: zone captures day and night; lighting tests

- [ ] HOUSE-03404 — Light the two upper floors to C4
      dep: HOUSE-03402, HOUSE-01030, HOUSE-03401 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-L1, Z-L2 · adv: G4, D6 · est: 4
      accept: C4 for `Z-L1` and `Z-L2`: physical fixtures (ceiling lights, bedside and desk lamps from the kit) on their groups; daylight readable at 10:30 in every room; evening schedule readable at night
      verify: zone captures day and night

- [ ] HOUSE-03405 — Light the ground-floor service rooms and the garage to C4
      dep: HOUSE-03402, HOUSE-01030, HOUSE-03401 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-L0S, Z-GAR · adv: G4, D6 · est: 3
      accept: C4 for `Z-L0S` and `Z-GAR`, including the service room's night depth from Round 102 and the garage's fluorescent/utility character
      verify: zone captures day and night

- [ ] HOUSE-03406 — Light the rear and side exterior at night to C4
      dep: HOUSE-03380 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-EXR, Z-EXF · adv: G4, D6 · est: 2
      accept: the rear garden, side yards and shed read at night by lanterns, the dusk-sensor fixtures and window spill, without a lifted global exposure; the front is re-checked, not re-worked
      verify: `Z-EXR` and `Z-EXF` night captures

- [ ] HOUSE-03407 — Bring the ground-floor principal route to C4
      dep: HOUSE-03402, HOUSE-01030, HOUSE-03401, HOUSE-03403, HOUSE-03404, HOUSE-03405 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G4 · est: 3
      accept: Round 102's ranked S2 list is closed on the fixed route views by day and night under the schedule; no new ground-floor-only lighting constant is introduced unless the house-wide solution fails there, and then it is recorded
      verify: `Z-L0M` captures day and night
      note: deliberately **last** in M5. It is the only permitted `Z-L0M` work before G4, and it comes after every other zone's lighting.

- [ ] HOUSE-01268 — Implement the sun-patch decals from the precomputed polygons, interpolated between grid entries
      dep: HOUSE-00208, HOUSE-01263 · sys: rendering · plat: ALL · pri: MUST
      dep-note: static until phase 23; then it moves with the sun

- [ ] HOUSE-01565 — Make the sun-patch decals follow the real sun by interpolating the 12 × 24 grid
      dep: HOUSE-01564, HOUSE-01268 · sys: rendering · plat: ALL · pri: MUST
      accept: a patch visibly crosses a bedroom floor over a simulated afternoon

- [ ] HOUSE-00850 — Implement the neighbour porch lights and street lights on the dusk sensor with per-fixture offsets
      dep: HOUSE-00849, HOUSE-01269, HOUSE-03380 · sys: lighting · plat: ALL · pri: MUST
      note: (2026-09-10) **Two `dep`s added, no scope changed.** Reached in DAG order after
            `HOUSE-00849` and found to have nothing to switch and nothing to sense: §28's light
            groups are `HOUSE-01251`'s `LightingSystem` skeleton in phase 16, and §35's clock --
            which is what a dusk SENSOR senses -- is `HOUSE-01531` in phase 22.
            `include/cnahouse/lighting/` and `include/cnahouse/environment/` are both empty
            directories today. The offset rule itself (§35.3's ±8 simulated minutes per fixture) is
            a pure function and could be written now; a task that says "implement the lights on the
            sensor" is not done while no light exists to be on it, and `HOUSE-00849`'s own
            `WindowGlow` already shows the shape it will take.
      correction: (2026-09-16) `HOUSE-01269` now owns the shared clock-to-group dusk controller,
            so this visible-neighbour consumer depends on that completed contract rather than
            repeating its two transitive dependencies. Its remaining scope is the neighbour porch
            and window-card presentation; the canonical street and porch group states already
            come from `HOUSE-01269`.
      amended: (2026-09-21, `HOUSE-03201`) gate G3 added: this is C4 work for `Z-STR` (rule R1)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00849, HOUSE-01269`

- [ ] HOUSE-00851 — Implement impostor rendering: yaw slice selection, sky tinting, vertical-axis billboarding for trees
      dep: HOUSE-00845, HOUSE-00204, HOUSE-00856, HOUSE-00772 · sys: rendering · plat: ALL · pri: MUST
      note: (2026-09-10) **One `dep` added, no scope changed.** "Vertical-axis billboarding for
            trees" needs trees, and §11.6's are `HOUSE-00772`'s, which is itself blocked on
            `HOUSE-00297`'s vegetation set. Two of this task's three parts are also waiting on
            data rather than on code: `HOUSE-00204` built the atlas RENDERER and no atlas has been
            baked for these cards -- `HOUSE-00845` drew them as geometric silhouettes and said so
            -- so there are no yaw slices to select between yet.

- [ ] HOUSE-00852 — Wire the neighbourhood into the exterior BVH with the correct LOD distances
      dep: HOUSE-00851, HOUSE-00678, HOUSE-00856 · sys: visibility · plat: ALL · pri: MUST

- [ ] HOUSE-01271 — Implement the `F6` lighting overlay
      dep: HOUSE-01251, HOUSE-00147 · sys: debug · plat: ALL · pri: SHOULD
      amended: (2026-09-21, `HOUSE-03201`) absorbs `HOUSE-02504` (cancelled duplicate); priority MUST → SHOULD (a debug aid)

- [ ] HOUSE-01272 — Implement the console `light <group> <on|off>` command
      dep: HOUSE-01251 · sys: debug · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) together with `--light-on`/`--light-off` this is the manual override for the automatic schedule (`HOUSE-03401`)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01252`

- [ ] HOUSE-01273 — Test: each of the 84 groups switches, changes its room's level, and changes the neighbour's borrowed light
      dep: HOUSE-01265 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) groups are switched through the lighting API or the console (there are no switch plates)

- [ ] HOUSE-01274 — Test: the 12 observable-behaviour rows of `cna-house.md` §30, each asserted on a measurable proxy
      dep: HOUSE-01266 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) rows that assumed a player-operated switch or door use the console/schedule and the static door poses instead

- [ ] HOUSE-01275 — Render tests: 20 rooms, lights on and off, at noon and at midnight
      dep: HOUSE-01266, HOUSE-03380 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) the 20 rooms span every level (≥ 3 per level)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01266`

- [ ] HOUSE-01276 — Measure the lighting system and the extra additive passes against budget
      dep: HOUSE-01256 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-03420 — **Gate G4 review: every zone at C4**
      dep: HOUSE-03402, HOUSE-03403, HOUSE-03404, HOUSE-03405, HOUSE-03406, HOUSE-03407, HOUSE-01030, HOUSE-00851, HOUSE-00850 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G4 · est: 1.5
      accept: an all-zone day and night round with no S1/S2 lighting or material defect open; the scoreboard shows every zone ≥ C4
      verify: ledger round; scoreboard

---

## M6 — Showcase baseline everywhere (C5) · gate G5

Track A. Secondary dressing, the S3 items of the zone being raised, and a per-zone performance
check, zone by zone, least complete first. The ground-floor principal route's legacy full-furnishing
tasks live here and **only here**, with the same standing as every other zone.

- [ ] HOUSE-03441 — Bring the basement to C5
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G5, D1, D3 · est: 5
      accept: `Z-B1` meets C5: secondary dressing per recipe (textiles, wall items, shelf contents, workshop and storage clutter from fill kits); fixed views day, night and overcast with no S1/S2; the zone's S3 list worked down to what R6 allows; the representative view within the High budget
      verify: `Z-B1` round; the zone's performance scenario

- [ ] HOUSE-03442 — Bring the ground-floor service rooms to C5
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0S · adv: G5, D1, D3 · est: 4
      accept: as `HOUSE-03441`, for `Z-L0S`
      verify: `Z-L0S` round; performance scenario

- [ ] HOUSE-03443 — Bring the garage to C5
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-GAR · adv: G5, D1, D3 · est: 3
      accept: as `HOUSE-03441`, for `Z-GAR`
      verify: `Z-GAR` round; performance scenario

- [ ] HOUSE-03444 — Bring the first upper floor to C5
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G5, D1, D3 · est: 6
      accept: as `HOUSE-03441`, for `Z-L1`: bedding, curtains, rugs, wall art and personal items per bedroom character (child's, teenager's untidy, guest made-up-and-unused); bathroom dressing (towels, toiletries, mirrors); no S1/S2
      verify: `Z-L1` round; performance scenario
      note: split by R7 if it exceeds 4 h (for example master suite and bedrooms, then bathrooms and closets).

- [ ] HOUSE-03445 — Bring the second upper floor to C5
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G5, D1, D3 · est: 6
      accept: as `HOUSE-03444`, for `Z-L2`: the library's filled shelves, the games room, the sitting room, both bedrooms, bathrooms
      verify: `Z-L2` round; performance scenario

- [ ] HOUSE-03446 — Bring the attic to C5
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-L3 · adv: G5, D1, D3 · est: 4
      accept: as `HOUSE-03441`, for `Z-L3`: the finished attic room's lived-in dressing and the stores' decades of accumulation (boxes, luggage, old furniture, decorations) without repetition beyond `HOUSE-00972`'s limits
      verify: `Z-L3` round; performance scenario

- [ ] HOUSE-03447 — Bring the vertical circulation to C5
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-STAIR · adv: G5, D1 · est: 2
      accept: as `HOUSE-03441`, for `Z-STAIR`, from every foot and head
      verify: `Z-STAIR` round

- [ ] HOUSE-03448 — Bring the rear and side exterior to C5
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-EXR · adv: G5, D1 · est: 4
      accept: as `HOUSE-03441`, for `Z-EXR`, including the shed interior's secondary dressing and the planting density along the fences
      verify: `Z-EXR` round; performance scenario

- [ ] HOUSE-03449 — Bring the street and neighbourhood to C5
      dep: HOUSE-03420, HOUSE-00851, HOUSE-00852 · sys: world · plat: TOOL · pri: MUST · zone: Z-STR · adv: G5, D1 · est: 3
      accept: as `HOUSE-03441`, for `Z-STR`: from the road and the upper-floor windows the neighbourhood reads as a street, not a backdrop, by day and night
      verify: `Z-STR` round; the neighbourhood performance measurement (`HOUSE-00853`)

- [ ] HOUSE-03450 — Bring the front exterior to C5
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-EXF · adv: G5, D1 · est: 2
      accept: as `HOUSE-03441`, for `Z-EXF`. This is a check against C5 plus S1/S2 fixes, not a new polish campaign on an area that has already had about 25 tasks
      verify: `Z-EXF` round

- [ ] HOUSE-03451 — Bring the ground-floor principal route to C5
      dep: HOUSE-00986, HOUSE-00987, HOUSE-00988, HOUSE-00989, HOUSE-00990, HOUSE-00992, HOUSE-00993 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G5, D1 · est: 2
      accept: the zone review closing the seven carried full-furnishing tasks below: `Z-L0M` meets C5
      verify: `Z-L0M` round

- [ ] HOUSE-00986 — Furnish `L0_FOYER` and `L0_PORCH`
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C5 (showcase baseline)** for the named cells: primary and secondary dressing, no S1/S2 in the zone's fixed views by day, night and overcast, and the room's representative view within the High budget. These rooms already reached C3 in the Visual Convergence Sprint. **Frozen until G4 (`HOUSE-03420`)** by rule R4
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00973…HOUSE-00985`

- [ ] HOUSE-00987 — Furnish `L0_HALL` (including the gallery wall placement)
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C5 (showcase baseline)** for the named cells: primary and secondary dressing, no S1/S2 in the zone's fixed views by day, night and overcast, and the room's representative view within the High budget. These rooms already reached C3 in the Visual Convergence Sprint. **Frozen until G4 (`HOUSE-03420`)** by rule R4
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`

- [ ] HOUSE-00988 — Furnish `L0_LIVING` (fireplace, piano, seating, bay window seat)
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C5 (showcase baseline)** for the named cells: primary and secondary dressing, no S1/S2 in the zone's fixed views by day, night and overcast, and the room's representative view within the High budget. These rooms already reached C3 in the Visual Convergence Sprint. **Frozen until G4 (`HOUSE-03420`)** by rule R4
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`

- [ ] HOUSE-00989 — Furnish `L0_DINING`
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C5 (showcase baseline)** for the named cells: primary and secondary dressing, no S1/S2 in the zone's fixed views by day, night and overcast, and the room's representative view within the High budget. These rooms already reached C3 in the Visual Convergence Sprint. **Frozen until G4 (`HOUSE-03420`)** by rule R4
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`

- [ ] HOUSE-00990 — Furnish `L0_KITCHEN` (the densest room: island, run, appliances, 62 containers)
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C5 (showcase baseline)** for the named cells: primary and secondary dressing, no S1/S2 in the zone's fixed views by day, night and overcast, and the room's representative view within the High budget. These rooms already reached C3 in the Visual Convergence Sprint. **Frozen until G4 (`HOUSE-03420`)** by rule R4
      amended: (2026-09-21, `HOUSE-03201`) cabinets are static closed joinery; the 62 containers are removed
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00975`

- [ ] HOUSE-00992 — Furnish `L0_FAMILY` (television, sectional, dog bed, media unit)
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C5 (showcase baseline)** for the named cells: primary and secondary dressing, no S1/S2 in the zone's fixed views by day, night and overcast, and the room's representative view within the High budget. These rooms already reached C3 in the Visual Convergence Sprint. **Frozen until G4 (`HOUSE-03420`)** by rule R4
      amended: (2026-09-21, `HOUSE-03201`) the television is a static screen (off); the dog bed is a static prop
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`

- [ ] HOUSE-00993 — Furnish `L0_SUNROOM` (breakfast table, wicker, plants, wet bar)
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) acceptance is now **C5 (showcase baseline)** for the named cells: primary and secondary dressing, no S1/S2 in the zone's fixed views by day, night and overcast, and the room's representative view within the High budget. These rooms already reached C3 in the Visual Convergence Sprint. **Frozen until G4 (`HOUSE-03420`)** by rule R4
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986`

- [ ] HOUSE-01027 — Run the anti-repetition validator over the whole house and fix every violation
      dep: HOUSE-03380 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) runs over the whole house after gate G3 instead of after every room's full furnishing
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00986…HOUSE-01026`

- [ ] HOUSE-01028 — Run the density check: every cell within its §59.1 band
      dep: HOUSE-01027, HOUSE-03420 · sys: world · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) the pass mark is the showcase band: every accessible room in at least the lower half of its §59.1 density band
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01027`

- [ ] HOUSE-01032 — Render tests: one pose per interior cell (78 scenes) at a fixed time and lighting
      dep: HOUSE-01030, HOUSE-03420 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) 76 scenes: the two nested appliance-interior cells are excluded
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01030`

- [ ] HOUSE-00917 — Render tests: a materials sheet scene plus 12 room poses at the new materials
      dep: HOUSE-00912, HOUSE-03380 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) the 12 room poses span every level, `B1` to `L3` (≥ 2 per level), not only the ground floor
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00912`

- [ ] HOUSE-02681 — Implement curtains and blinds: meshes, `blindFraction` interaction, the `translucent` portal mode, the transmission cut
      dep: HOUSE-00971, HOUSE-03420 · sys: interaction · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) static curtains and blinds: meshes and materials only; the portals keep their glass opacity; no `blindFraction` interaction and no `translucent` mode switching
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01196`

- [ ] HOUSE-02682 — Author curtains and blinds for the 34 windows that have them
      dep: HOUSE-02681 · sys: content · plat: TOOL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) static (see `HOUSE-02681`)

- [ ] HOUSE-02688 — Acquire and place the remaining wall art (landscapes, abstracts, botanicals, posters), all CC0, all manifested
      dep: HOUSE-00262 · sys: content · plat: TOOL · pri: MUST

- [ ] HOUSE-03480 — **Gate G5 review: every zone at C5 (DONE item D1)**
      dep: HOUSE-03441, HOUSE-03442, HOUSE-03443, HOUSE-03444, HOUSE-03445, HOUSE-03446, HOUSE-03447, HOUSE-03448, HOUSE-03449, HOUSE-03450, HOUSE-03451, HOUSE-01027, HOUSE-01028, HOUSE-01032 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G5, D1 · est: 2
      accept: an all-zone round (day, night, overcast) with no S1/S2 open anywhere; density and anti-repetition checks pass; the scoreboard shows every zone ≥ C5; the S3/S4 backlog is handed to M11
      verify: ledger round; scoreboard

---

## M7 — Environment systems

Track B. The legacy phases 12 (seasonal vegetation), 10 (grass, snow shells), 23 (glare), 25
(sky), 26 (wiring) and 27–30 (rain, snow, storm, hail, wind), minus everything that needed
openable windows, moving doors or water surfaces. Each piece must be **visible**. None may grow into
a simulation of its own; the weather state, seasons, astronomy and sky dome already exist.

- [ ] HOUSE-03520 — Environment review: every D5 item shown in fixed environment captures
      dep: HOUSE-01696, HOUSE-01754, HOUSE-01800, HOUSE-01843, HOUSE-01889, HOUSE-01653, HOUSE-00919 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D5 · est: 3
      accept: (1) `capture_review.py` gains an environment scenario set (clear, overcast, rain, snow, storm, windy × day, dusk, night × one view each of `Z-EXF`, `Z-EXR`, an upper-floor window and the attic) plus one seasonal quartet; (2) every D5 item is visible in it; (3) S1/S2 environment defects are fixed or filed
      verify: the capture set; a ledger round

### Sky, sun and glare

- [ ] HOUSE-01696 — Wire the weather into the sky (cloud cover, thunder), lighting (cloud modulation) and fog
      dep: HOUSE-01686, HOUSE-01648, HOUSE-01706 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01650 — Implement fog: colour from the horizon in the view direction, start/end from `fogDensity` and precipitation, enabled only for exterior batches
      dep: HOUSE-01649, HOUSE-00892 · sys: rendering · plat: ALL · pri: MUST

- [ ] HOUSE-01652 — Tune the six sky states against reference photographs; record the final LUTs
      dep: HOUSE-01650 · sys: content · plat: TOOL · pri: MUST

- [ ] HOUSE-01653 — Render tests: the six sky states at four times of day (24 scenes)
      dep: HOUSE-01652 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-01654 — Measure the sky and cloud cost
      dep: HOUSE-01650 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-01567 — Implement the `OcclusionQuery` pool and its per-frame lifecycle (create once, reuse, read one frame late)
      dep: HOUSE-00090 · sys: rendering · plat: ALL · pri: MUST

- [ ] HOUSE-01568 — Implement the 3 × 3 glare sample grid with colour writes disabled and depth test on
      dep: HOUSE-01567, HOUSE-01566 · sys: rendering · plat: ALL · pri: MUST

- [ ] HOUSE-01569 — Implement the coverage computation with the boolean/count refinement and a 0.25 s smoothing
      dep: HOUSE-01568 · sys: rendering · plat: ALL · pri: MUST
      accept: 10 quantised levels on a boolean driver; continuous on `OPENGL33`

- [ ] HOUSE-01570 — Implement the 7-sprite additive lens flare through `SpriteBatch`
      dep: HOUSE-01569, HOUSE-00145 · sys: rendering · plat: ALL · pri: MUST

- [ ] HOUSE-01571 — Author the flare sprite set and tune its colours, scales and offsets
      dep: HOUSE-01570 · sys: content · plat: TOOL · pri: MUST

- [ ] HOUSE-01575 — Validate the OPENGLES3 boolean-grid coverage against the OPENGL33 true count on the same scene
      dep: HOUSE-01569, HOUSE-00091 · sys: ci · plat: LNX · pri: MUST
      accept: the two agree within 0.15 coverage across 20 sampled camera angles

- [ ] HOUSE-01576 — Render tests: sunrise, morning, noon, afternoon, sunset, and the glare at 6 angles
      dep: HOUSE-01572 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-01577 — Measure the glare queries' cost
      dep: HOUSE-01569 · sys: — · plat: LNX · pri: MUST

### Rain

- [ ] HOUSE-01741 — Implement the shared particle renderer: pooled camera-facing quads in a `DynamicVertexBuffer`, one draw per material, ≤ 6 materials
      dep: HOUSE-00092 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01742 — Implement the camera-relative precipitation volume with wrap-around and the wind offset
      dep: HOUSE-01741 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01743 — Implement rain particle motion: gravity, wind, intensity-driven count, velocity-elongated quads
      dep: HOUSE-01742 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01744 — Implement the roof/coverage mask test and the teleport-to-top behaviour for sheltered particles
      dep: HOUSE-01743, HOUSE-00777 · sys: weather · plat: ALL · pri: MUST
      accept: standing under the porch, rain visibly stops at the porch edge; at an open upstairs window it passes but does not enter

- [ ] HOUSE-01745 — Author the rain streak texture and material
      dep: HOUSE-01743 · sys: content · plat: TOOL · pri: MUST

- [ ] HOUSE-01746 — Implement splash particles on exposed surfaces within 6 m, capped at 60
      dep: HOUSE-01744 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01748 — Implement the wet-material swap driven by `surfaceWetness` (Tier S)
      dep: HOUSE-01690, HOUSE-00905 · sys: rendering · plat: ALL · pri: MUST

- [ ] HOUSE-01749 — Implement puddle decals at the height field's local minima above `wetness > 0.55`
      dep: HOUSE-01748 · sys: rendering · plat: ALL · pri: SHOULD

- [ ] HOUSE-01750 — Implement the four rain audio layers (open air, roof, windows, downspouts) with their gain drivers
      dep: HOUSE-00286, HOUSE-00779 · sys: audio · plat: ALL · pri: MUST
      dep-note: full routing lands in phase 32; the gains are defined here
      amended: (2026-09-21, `HOUSE-03201`) the window layer is driven by sky exposure and window area, not by open windows

- [ ] HOUSE-01753 — Measure the rain system at intensity 1.0 against the particle and CPU budgets
      dep: HOUSE-01746 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-01754 — Render tests: light rain, heavy rain, rain seen from indoors through a window, rain under the porch, wet driveway
      dep: HOUSE-01749 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-01755 — Test: no rain particle is ever drawn below a covered surface
      dep: HOUSE-01744 · sys: ci · plat: CI · pri: MUST

### Snow

- [ ] HOUSE-00778 — Build the snow shells (`build_snowshell.py`) for terrain, roofs, decks, rails, furniture and the car
      dep: HOUSE-00214, HOUSE-00777, HOUSE-03380 · sys: content · plat: TOOL · pri: MUST
      dep-note: the asset dependencies were added by `HOUSE-00293`: derived furniture and car
            shells cannot be generated before the source models exist and pass the content
            pipeline. Depending only on `HOUSE-00777` could never satisfy the task's own noun list.
      amended: (2026-09-21, `HOUSE-03201`) the dependency on every furniture acquisition group and the player car is replaced by gate G3 (the furniture that needs a shell exists by then); the car's shell follows `HOUSE-01798` if the car ships
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00214, HOUSE-00777, HOUSE-00975…HOUSE-00985, HOUSE-01036`

- [ ] HOUSE-01791 — Implement snow particle motion: slow fall, lateral drift, per-particle flutter, rotation
      dep: HOUSE-01742 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01792 — Author the snowflake texture and material (alpha-blended, occluding)
      dep: HOUSE-01791 · sys: content · plat: TOOL · pri: MUST

- [ ] HOUSE-01793 — Implement the snow-shell rendering driven by `snowDepth` with a smoothstep alpha fade
      dep: HOUSE-00778, HOUSE-01691 · sys: rendering · plat: ALL · pri: MUST
      accept: accumulation fades in and out continuously; no snow on faces steeper than the material's slope limit

- [ ] HOUSE-01794 — Implement the snow displacement of the shell along the surface normal by `snowDepth`
      dep: HOUSE-01793 · sys: rendering · plat: ALL · pri: MUST

- [ ] HOUSE-01795 — Implement the footstep surface switch to `snow` and the deep-snow speed penalty
      dep: HOUSE-01691, HOUSE-00557 · sys: player · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) the deep-snow speed penalty is dropped (a gameplay rule); the footstep surface switch stays

- [ ] HOUSE-01796 — Implement the snow ambience change (quieter, absorbed) and the exterior reverb hint change
      dep: HOUSE-01691 · sys: audio · plat: ALL · pri: MUST

- [ ] HOUSE-01797 — Implement melt: `snowDepth` decay from temperature and direct sun, faster on dark materials
      dep: HOUSE-01691 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01798 — Implement snow on the car, the bins, the fence rails and the garden furniture via their shell entries
      dep: HOUSE-01793 · sys: rendering · plat: ALL · pri: SHOULD

- [ ] HOUSE-01802 — Implement accumulating seasonal snow cover on the garden, the roof, the fence and the cars, building and melting with the outdoor temperature rather than with the season index
      dep: HOUSE-01704, HOUSE-01793 · sys: weather · plat: ALL · pri: MUST
      accept: (1) cover builds over simulated hours of snowfall and melts over simulated hours
              above freezing; (2) a warm spell mid-winter visibly clears the drive; (3) the depth
              is saved; (4) the four surfaces accumulate independently
      verify: render test snow-cover-01..04; integration SnowCoverTests.MeltCycle

- [ ] HOUSE-01799 — Measure the snow system at intensity 1.0 against budget
      dep: HOUSE-01794 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-01800 — Render tests: light snow, blizzard, 5 cm accumulation, 30 cm accumulation, melt in progress
      dep: HOUSE-01797 · sys: ci · plat: CI · pri: MUST

### Storm, lightning and thunder

- [ ] HOUSE-01831 — Implement the Poisson lightning process driven by `thunderIntensity`
      dep: HOUSE-01686 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01832 — Implement strike distance and azimuth sampling, biased by intensity and wind
      dep: HOUSE-01831 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01833 — Implement the 3-lobed flash envelope (90–160 ms)
      dep: HOUSE-01832 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01834 — Implement the flash's effect on `daylight` for every cell with an exterior window
      dep: HOUSE-01833, HOUSE-01263 · sys: lighting · plat: ALL · pri: MUST
      accept: rooms the player is not in also light up, and the light spills through open doors via the flood

- [ ] HOUSE-01835 — Implement the flash's `DirectionalLight0` override for dynamic objects
      dep: HOUSE-01834, HOUSE-01261 · sys: lighting · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) "dynamic objects" now means the Basic-lit static props and fixed detail; there are no dynamic actors

- [ ] HOUSE-01836 — Implement the full-screen additive flash quad scaled by how much sky the camera can see
      dep: HOUSE-01833, HOUSE-00145 · sys: rendering · plat: ALL · pri: MUST

- [ ] HOUSE-01837 — Implement the visible bolt for strikes under 2 km: a 3-level midpoint-displaced polyline with 2 forks, drawn as additive quads
      dep: HOUSE-01836 · sys: rendering · plat: ALL · pri: SHOULD

- [ ] HOUSE-01838 — Implement thunder scheduling at `distance / 343 m/s` and the 3-class sample selection
      dep: HOUSE-00286, HOUSE-01832 · sys: audio · plat: ALL · pri: MUST
      accept: counting the seconds between flash and thunder gives the sampled distance

- [ ] HOUSE-01839 — Implement thunder's indoor attenuation and its dull cross-fade
      dep: HOUSE-01838 · sys: audio · plat: ALL · pri: MUST
      dep-note: uses the phase-32 room-aware model

- [ ] HOUSE-01840 — Implement the gust front before a near strike (+3…8 m/s for 4–9 s)
      dep: HOUSE-01832 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01841 — Test: the flash-to-thunder delay matches the sampled distance within 50 ms
      dep: HOUSE-01838 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-01842 — Test: a strike raises `daylight` in exactly the cells with exterior windows
      dep: HOUSE-01834 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-01843 — Render tests: a frozen flash from outdoors, from a lit room, from a dark room, and from the basement
      dep: HOUSE-01836 · sys: ci · plat: CI · pri: MUST

### Wind, vegetation, seasons and hail

- [ ] HOUSE-01876 — Implement the wind model: base + 3-octave gust noise + direction wander
      dep: HOUSE-01686, HOUSE-00027 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-00773 — Implement grass-card rendering with `AlphaTestEffect` and per-instance jitter
      dep: HOUSE-00772, HOUSE-00080 · sys: rendering · plat: ALL · pri: MUST

- [ ] HOUSE-01877 — Implement vegetation sway: the 15 Hz CPU vertex pass for the ≤ 40 nearest instances, static beyond
      dep: HOUSE-01876, HOUSE-00772 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01878 — Implement the gust-synchronised whole-canopy lean
      dep: HOUSE-01877 · sys: weather · plat: ALL · pri: SHOULD

- [ ] HOUSE-01879 — Implement grass and hedge sway (faster, smaller)
      dep: HOUSE-01877 · sys: weather · plat: ALL · pri: SHOULD

- [ ] HOUSE-00919 — Author the seasonal vegetation material set — spring light green with blossom, summer dense dark green, autumn yellow/orange/red, winter bare — and blend between them from `SeasonPhase`
      dep: HOUSE-01543, HOUSE-00897 · sys: rendering · plat: ALL · pri: MUST
      accept: (1) four looks per species, reached by blending and never by switching; (2) the
              transition is imperceptible frame to frame; (3) Tier S only — a tint and a texture
              swap, no custom effect
      verify: render test vegetation-season-01..04 at the four solstice/equinox points

- [ ] HOUSE-00920 — Implement the autumn leaf-fall transition: foliage density ramps down over the last 20 % of autumn, leaf litter accumulates on the ground, and the branches finish bare
      dep: HOUSE-00919 · sys: rendering · plat: ALL · pri: MUST
      accept: (1) density is a continuous function of `SeasonPhase`, not a step; (2) litter
              accumulates then thins; (3) by the start of winter the deciduous trees are bare
      verify: render test vegetation-leaffall-01..05 sampling the last fifth of autumn

- [ ] HOUSE-01883 — Implement the wind ambience gain and low-pass cross-fade by speed
      dep: HOUSE-01876, HOUSE-00290 · sys: audio · plat: ALL · pri: MUST

- [ ] HOUSE-01885 — Implement the roof and eaves whistle in the attic cells
      dep: HOUSE-01883 · sys: audio · plat: ALL · pri: SHOULD

- [ ] HOUSE-01875 — Implement the hail-with-thunder-and-temperature-drop coupling in the archetype table
      dep: HOUSE-01688 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01871 — Implement hail particle motion: fast fall, small bright quads, motion-blur streak
      dep: HOUSE-01742 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01872 — Implement the hail bounce (0.35 restitution, randomised tangential, 0.5 s extra life)
      dep: HOUSE-01871, HOUSE-00777 · sys: weather · plat: ALL · pri: MUST

- [ ] HOUSE-01874 — Implement hail audio: the density-driven impact layer plus roof, window and car layers
      dep: HOUSE-00286, HOUSE-01872 · sys: audio · plat: ALL · pri: MUST

- [ ] HOUSE-01888 — Measure hail at intensity 1.0 and wind at 20 m/s against budget
      dep: HOUSE-01877 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-01889 — Render tests: a hail squall, trees at 4 wind speeds, a door mid-slam
      dep: HOUSE-01881 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) the door-mid-slam scene is dropped (doors are static)

---

## M8 — Atmospheric audio

Track B. Lightweight audio that makes the walk feel inhabited: footsteps, room tone, exterior
ambience by time and season, weather, and a few static positional loops. There is no portal-path
solver and no interaction sound table (ADR-0014). The audio assets largely exist already, because
the NOX collection is imported (`content/Audio/ambience`, `footstep`, `footstep-exterior`,
`appliance`).

- [ ] HOUSE-03541 — Cell-gated positional emitters
      dep: HOUSE-01912, HOUSE-01915 · sys: audio · plat: ALL · pri: MUST · zone: all · adv: D7 · est: 2
      files: src/audio/*, tests
      accept: (1) a static positional loop (clock, furnace, refrigerator hum) is audible at its `Apply3D` gain in its own cell, attenuated by one fixed per-hop gain through at most one open portal, and silent beyond; (2) re-evaluated only on a listener cell change; (3) no path solver, no occlusion ray, no muffle cross-fade
      verify: unit CellGatedEmitterTests.*

- [ ] HOUSE-00281 — Grouped: source the 8 missing footstep surfaces (carpet, concrete, interior hardwood, 3 stair variants, asphalt, bluestone) — ≥ 6 walk + 6 run + 2 land each, CC0
      dep: HOUSE-00272, HOUSE-00280 · sys: audio · plat: TOOL · pri: MUST
      accept: 8 surfaces × ≥ 14 samples, manifested, converted, auditioned
      note: (2026-09-07) `HOUSE-00280` measured the shortfall and it is **113 samples, not
            112**: `water` needs one more run variant as well. It has a pack, 6 walk and
            6 land, and NOX ships only 5 run samples in the entire collection, so no
            selection could have reached §62.4's six. `docs/asset-selection/footstep-surfaces.md`
            is the generated shopping list and is kept current by a gate.
      note: `audio-core` is at **99.8 % of its 30 MB budget** with the NOX subset alone
            (`HOUSE-00278`), and these samples land in that pack — about 6 MB at the
            measured 0.052 MB a clip. The budget question is recorded in §72.
      blocked: (2026-09-07) **Freesound originals require an account, and creating one means
            accepting their ToS — which is the project owner's to accept, not a tool's.** Measured
            rather than assumed: `https://freesound.org/apiv2/search/text/?query=…` answers
            **401 "Authentication credentials were not provided"**, and a sound's `/download/` URL
            **302s to `https://freesound.org/home/login/`**. The search pages and the CC0 filter
            read fine anonymously, so the *selection* work could be done; only the bytes cannot be
            fetched. The lossy CDN previews are readable without a login and are **not** a
            substitute — this project converts 24-bit originals and manifests their hashes.
            `HOUSE-00272` established that the licence assurance rests on ToS §87b/§87c, whose
            warranty and indemnity bind **the account holder**, which is the same reason the
            account cannot be created on the owner's behalf.

- [ ] HOUSE-00286 — Grouped: source weather sounds NOX lacks — thunder ×3 distances (≥ 9 samples), hail on roof/window/car, rain on roof/window, gutter trickle — ≥ 18 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
      blocked: (2026-09-07) **Freesound originals require an account, and creating one means
            accepting their ToS — which is the project owner's to accept, not a tool's.** Measured
            rather than assumed: `https://freesound.org/apiv2/search/text/?query=…` answers
            **401 "Authentication credentials were not provided"**, and a sound's `/download/` URL
            **302s to `https://freesound.org/home/login/`**. The search pages and the CC0 filter
            read fine anonymously, so the *selection* work could be done; only the bytes cannot be
            fetched. The lossy CDN previews are readable without a login and are **not** a
            substitute — this project converts 24-bit originals and manifests their hashes.
            `HOUSE-00272` established that the licence assurance rests on ToS §87b/§87c, whose
            warranty and indemnity bind **the account holder**, which is the same reason the
            account cannot be created on the owner's behalf.

- [ ] HOUSE-00289 — Grouped: source house sounds — HVAC burner/blower/duct tick, clock tick, clock chime, doorbell, garage motor, gate motor, creaks ×8 — ≥ 18 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
      blocked: (2026-09-07) **Freesound originals require an account, and creating one means
            accepting their ToS — which is the project owner's to accept, not a tool's.** Measured
            rather than assumed: `https://freesound.org/apiv2/search/text/?query=…` answers
            **401 "Authentication credentials were not provided"**, and a sound's `/download/` URL
            **302s to `https://freesound.org/home/login/`**. The search pages and the CC0 filter
            read fine anonymously, so the *selection* work could be done; only the bytes cannot be
            fetched. The lossy CDN previews are readable without a login and are **not** a
            substitute — this project converts 24-bit originals and manifests their hashes.
            `HOUSE-00272` established that the licence assurance rests on ToS §87b/§87c, whose
            warranty and indemnity bind **the account holder**, which is the same reason the
            account cannot be created on the owner's behalf.
      amended: (2026-09-21, `HOUSE-03201`) scope reduced to a small house-ambience set: clock tick, one hall-clock chime, a furnace/air-handler hum loop and ≥ 6 structural creaks (≥ 10 samples). Doorbell, garage motor and gate motor are dropped with their systems

- [ ] HOUSE-00290 — Grouped: source outdoor ambience NOX lacks — distant road, suburban day, suburban night, lawnmower, aircraft, neighbourhood dog — ≥ 10 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
      blocked: (2026-09-07) **Freesound originals require an account, and creating one means
            accepting their ToS — which is the project owner's to accept, not a tool's.** Measured
            rather than assumed: `https://freesound.org/apiv2/search/text/?query=…` answers
            **401 "Authentication credentials were not provided"**, and a sound's `/download/` URL
            **302s to `https://freesound.org/home/login/`**. The search pages and the CC0 filter
            read fine anonymously, so the *selection* work could be done; only the bytes cannot be
            fetched. The lossy CDN previews are readable without a login and are **not** a
            substitute — this project converts 24-bit originals and manifests their hashes.
            `HOUSE-00272` established that the licence assurance rests on ToS §87b/§87c, whose
            warranty and indemnity bind **the account holder**, which is the same reason the
            account cannot be created on the owner's behalf.

- [ ] HOUSE-00302 — Phase-4 review and commit; update `cna-house.md` §19/§20 with what was actually found
      dep: HOUSE-00281, HOUSE-00286, HOUSE-00289, HOUSE-00290 · sys: — · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) is now the close-out of the **retained** sourcing groups (`HOUSE-00281`, `00286`, `00289`, `00290`); the cancelled groups (`00282`–`00285`, `00287`, `00288`) are recorded as cancelled, not sourced
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00261…HOUSE-00301`

- [ ] HOUSE-01911 — Implement `AudioSystem` proper: category volumes, master, mute, device-loss handling
      dep: HOUSE-00154 · sys: audio · plat: ALL · pri: MUST

- [ ] HOUSE-01912 — Implement `EmitterPool`: up to 64 logical emitters with lifetime, position, category and priority
      dep: HOUSE-01911 · sys: audio · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) cap reduced to 32 logical emitters, with a hard voice cap: the lowest-priority, quietest emitters are culled. There is no virtualisation because `HOUSE-01913` is cancelled

- [ ] HOUSE-01914 — Implement the `AudioListener` update from the camera, with velocity pinned to zero
      dep: HOUSE-01911 · sys: audio · plat: ALL · pri: MUST

- [ ] HOUSE-01915 — Implement `Apply3D` invocation per positional emitter, with `DopplerScale = 0` by default
      dep: HOUSE-01914, HOUSE-00096 · sys: audio · plat: ALL · pri: MUST

- [ ] HOUSE-01917 — Implement the sound bank loader from `layout.audio.json` and the manifest
      dep: HOUSE-01911, HOUSE-00279 · sys: audio · plat: ALL · pri: MUST

- [ ] HOUSE-01918 — Implement `Bag<T>` round-robin sample selection with a no-repeat-within-4 rule, pitch ±4 % and volume ±10 %
      dep: HOUSE-01917, HOUSE-00027 · sys: audio · plat: ALL · pri: MUST

- [ ] HOUSE-01919 — Implement `FootstepDirector`: the stride accumulator, surface lookup, per-surface bag, and the stair riser-crossing trigger
      dep: HOUSE-01918, HOUSE-00560 · sys: audio · plat: ALL · pri: MUST
      accept: cadence matches speed; stairs give one step per riser

- [ ] HOUSE-01920 — Wire the 20 footstep surface sets (12 from NOX, 8 sourced)
      dep: HOUSE-01919, HOUSE-00280, HOUSE-00281 · sys: audio · plat: TOOL · pri: MUST

- [ ] HOUSE-01922 — Implement `AmbienceDirector`: per-cell room tone with an 0.8 s cross-fade on cell change
      dep: HOUSE-01917, HOUSE-00559 · sys: audio · plat: ALL · pri: MUST

- [ ] HOUSE-01923 — Author and wire the 6 room-tone beds
      dep: HOUSE-01922 · sys: audio · plat: TOOL · pri: MUST

- [ ] HOUSE-01924 — Implement the exterior ambience bed with time-of-day variation (dawn chorus, day traffic, evening crickets, night quiet)
      dep: HOUSE-01922, HOUSE-00290 · sys: audio · plat: ALL · pri: MUST

- [ ] HOUSE-01925 — Implement the seasonal and weather layers over the exterior bed
      dep: HOUSE-01924 · sys: audio · plat: ALL · pri: MUST

- [ ] HOUSE-01941 — Make the exterior ambience bed seasonal: spring birdsong and dawn chorus, summer insects, autumn wind in bare branches, winter muffled stillness — cross-faded on `SeasonPhase`
      dep: HOUSE-01543, HOUSE-01925 · sys: audio · plat: ALL · pri: MUST
      accept: (1) the bed is the blend-weighted mix of the two neighbouring seasons, cross-faded
              rather than switched; (2) birdsong is absent in winter and densest in spring;
              (3) it respects the room-aware attenuation of §64 — the bed is quieter indoors
      verify: manual listen across one simulated year; unit AmbienceTests.SeasonalWeights

- [ ] HOUSE-02000 — Implement sky-exposure-driven ambience routing for the weather layers
      dep: HOUSE-00779, HOUSE-01925 · sys: audio · plat: ALL · pri: MUST
      accept: `L3_STORE_W` gets full rain-on-roof; `B1_CINEMA` gets essentially nothing; an open slider in the sunroom nearly matches outdoors
      amended: (2026-09-21, `HOUSE-03201`) this is the retained piece of room-aware audio: the weather beds follow the listener cell's sky exposure

- [ ] HOUSE-01927 — Implement the house's own sound emitters from the plumbing and HVAC data (furnace, blower, duct ticks, water heater, pipe hiss)
      dep: HOUSE-01912, HOUSE-03541 · sys: audio · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) scope is static positional ambient loops only: furnace/air-handler hum in `B1_MECHANICAL`, refrigerator and freezer hum in the kitchen and pantry, a quiet electrical hum in `B1_ELECTRICAL`. It absorbs `HOUSE-01931`. There is no thermostat, no pipe hiss and no plumbing
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00387, HOUSE-01912`

- [ ] HOUSE-01929 — Implement the house-creak system: 14 placed points, rate weighted by the rate of change of outdoor temperature
      dep: HOUSE-01927, HOUSE-00289 · sys: audio · plat: ALL · pri: SHOULD

- [ ] HOUSE-01930 — Implement the clocks: a tick per simulated minute in 3 rooms, and the hall clock's hourly chime
      dep: HOUSE-01927, HOUSE-01531 · sys: audio · plat: ALL · pri: MUST

- [ ] HOUSE-01934 — Implement the `F7` audio overlay (voices by category, emitters, gains)
      dep: HOUSE-01913, HOUSE-00147 · sys: debug · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) voices by category, emitters and gains; no path visualisation

- [ ] HOUSE-01935 — Implement the audio settings tab and its live application
      dep: HOUSE-01911, HOUSE-00131 · sys: audio · plat: ALL · pri: MUST

- [ ] HOUSE-01936 — Measure the audio update cost against the 0.25/0.60 ms budget
      dep: HOUSE-01913 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-01938 — Test: footstep cadence matches speed within 5 % across all speeds and surfaces
      dep: HOUSE-01919 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-01939 — Listening review: walk the house and list every sound that is wrong, missing or too loud; fix
      dep: HOUSE-01927, HOUSE-01925, HOUSE-01920 · sys: audio · plat: LNX · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) walk every zone, not only the ground floor
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01931`

---

## M9 — Application shell: settings, menus, session, debug

Track B. What a finished application needs around the walk. Settings persistence already exists
(`HOUSE-00131`, `HOUSE-00152`).

- [ ] HOUSE-03571 — Optional session resume: player pose, clock and weather in one small file
      dep: HOUSE-00152 · sys: persistence · plat: ALL · pri: SHOULD · zone: all · adv: D8 · est: 3
      files: src/persistence/SessionFile.cpp|hpp, tests/unit/SessionFileTests.cpp
      accept: (1) one versioned JSON through `ISaveStore` holds the player cell, position and yaw, the clock (`SimClock` epoch and scale) and the weather state including the RNG state (`HOUSE-01692`); (2) written on quit and on opening the pause menu; (3) *Continue* resumes it, and *Start on the street* ignores it; (4) a corrupt, newer-version or unknown-cell file starts fresh at the canonical spawn with one log line, never a crash; (5) no household state of any kind
      verify: unit SessionFileTests.RoundTrip, .CorruptStartsFresh, .UnknownCellStartsFresh

- [ ] HOUSE-02516 — Implement the settings menu shell with the 5 tabs and keyboard-only navigation
      dep: HOUSE-00156, HOUSE-00131 · sys: ui · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) the five tabs are Display, Graphics, Audio, Controls and Environment

- [ ] HOUSE-02517 — Implement the Display tab (resolution, window mode, v-sync, frame cap, FOV, UI scale)
      dep: HOUSE-02516 · sys: ui · plat: ALL · pri: MUST

- [ ] HOUSE-02518 — Implement the Graphics tab, with options filtered by the project-owned **effective feature set** (`cna-house.md` §68) — `RenderTier` + build/platform profile + validated standard-XNA behaviour, all of it known to the application already
      dep: HOUSE-02516, HOUSE-00160 · sys: ui · plat: ALL · pri: MUST
      accept: no meaningless toggle is ever shown; a build whose profile cannot draw shadow maps does not offer them; the tab calls `SupportsCapability()` or another CNA extension query nowhere, and `check_xna_only.py` passes on the UI sources

- [ ] HOUSE-02519 — Implement the Audio tab with live application
      dep: HOUSE-02516, HOUSE-01935 · sys: ui · plat: ALL · pri: MUST

- [ ] HOUSE-02520 — Implement the Controls tab including full key remapping through `IInputSource`
      dep: HOUSE-02516, HOUSE-00140 · sys: ui · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) sensitivity, invert-Y, smoothing and FOV are MUST; full key remapping is SHOULD

- [ ] HOUSE-02521 — Implement the Simulation tab (day length, weather mode, moon speed, location, pets, pause-on-menu, slots)
      dep: HOUSE-02516 · sys: ui · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) becomes the **Environment** tab: day length, weather mode and fixed archetype, time-of-day and season jump, location. No pets and no save slots

- [ ] HOUSE-02522 — Implement settings validation, clamping and migration
      dep: HOUSE-02521, HOUSE-00131 · sys: ui · plat: ALL · pri: MUST

- [ ] HOUSE-02523 — Implement the main menu, the pause menu and the credits screen (showing `THIRD-PARTY-ASSETS.md`)
      dep: HOUSE-02516, HOUSE-00198 · sys: ui · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) main menu: *Start on the street* · *Continue* (only when a session file exists, `HOUSE-03571`) · *Settings* · *Credits* · *Quit*

- [ ] HOUSE-02524 — Implement the pause-on-menu setting and its default (D-29)
      dep: HOUSE-02523 · sys: ui · plat: ALL · pri: SHOULD
      amended: (2026-09-21, `HOUSE-03201`) priority MUST → SHOULD

- [ ] HOUSE-02525 — Implement the first-run hint line and its 12 s fade
      dep: HOUSE-02523 · sys: ui · plat: ALL · pri: MUST

- [ ] HOUSE-02526 — Implement the toast system and the vignette
      dep: HOUSE-02523 · sys: ui · plat: ALL · pri: MUST

- [ ] HOUSE-02527 — Implement UI virtual-unit layout with safe-area insets, and prove it at 4:3, 16:9, 21:9 and 18:9
      dep: HOUSE-02523, HOUSE-00145 · sys: ui · plat: ALL · pri: MUST

- [ ] HOUSE-02528 — Test: every settings value round-trips, clamps and applies live
      dep: HOUSE-02522 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02529 — Test: every menu control is reachable and operable by keyboard alone
      dep: HOUSE-02523 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02530 — Render tests: every menu and overlay at three aspect ratios
      dep: HOUSE-02527 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02501 — Complete the `F1` performance overlay (GPU timing where available, per-pass draw counts)
      dep: HOUSE-00150 · sys: debug · plat: ALL · pri: MUST

- [ ] HOUSE-02502 — Complete the `F2` world overlay
      dep: HOUSE-00631 · sys: debug · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) the target section of `HOUSE-01140` is gone with the interaction framework
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00631, HOUSE-01140`

- [ ] HOUSE-02503 — Complete the `F3`/`F4`/`F5` visibility overlays
      dep: HOUSE-00683 · sys: debug · plat: ALL · pri: MUST

- [ ] HOUSE-02506 — Complete the `F8` environment overlay
      dep: HOUSE-01616, HOUSE-01695 · sys: debug · plat: ALL · pri: MUST

- [ ] HOUSE-02507 — Complete the `F9` physics overlay with the pets' capsules and paths
      dep: HOUSE-00562 · sys: debug · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) shows the capsule, probes, sweeps and posed-leaf obstacles; no pets
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00562, HOUSE-02047`

- [ ] HOUSE-02510 — Implement the console with history, completion and the full command set of `cna-house.md` §69
      dep: HOUSE-00145 · sys: debug · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) the command set is §69's minus commands for removed systems (interact, pet, save slots, reset)

- [ ] HOUSE-02511 — Implement the screenshot harness driven by a named-pose JSON, with fixed time and weather
      dep: HOUSE-00151, HOUSE-01694 · sys: debug · plat: ALL · pri: MUST

- [ ] HOUSE-02513 — Implement `validate world` as a runtime command
      dep: HOUSE-00357 · sys: debug · plat: ALL · pri: SHOULD

- [ ] HOUSE-02514 — Implement `budget report` writing the current frame's numbers against the budget table
      dep: HOUSE-02403 · sys: debug · plat: ALL · pri: MUST

- [ ] HOUSE-02515 — Verify every debug tool compiles out with `CNAHOUSE_DEBUG_TOOLS=OFF` and costs nothing in a release build
      dep: HOUSE-02501…HOUSE-02514 · sys: ci · plat: CI · pri: MUST

---

## M10 — Performance and budgets (measurement-driven)

Track B. Optimise **only what measurement shows matters**. A task whose gate measurement comes back
green closes with the numbers and no code. The streaming block (`HOUSE-02452`–`02462`) exists only
if `HOUSE-02451` demands it.

- [ ] HOUSE-02402 — Implement the 10 performance scenarios as a runnable harness
      dep: HOUSE-00150, HOUSE-03202 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) the scenarios include ≥ 1 representative view per zone (the `HOUSE-03202` sets) plus the worst exterior and stairwell views, not only ground-floor views
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-00150`

- [ ] HOUSE-02403 — Measure all 10 scenarios and record the baseline in `docs/performance-log.md`
      dep: HOUSE-02402 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-02404 — Optimise against whichever scenario is furthest from budget; repeat until all pass
      dep: HOUSE-02403 · sys: — · plat: LNX · pri: MUST
      accept: every budget in `cna-house.md` §71.2 met at the *High* tier

- [ ] HOUSE-02405 — Implement the quality-tier presets and verify each reaches its own budget
      dep: HOUSE-02404, HOUSE-00157 · sys: rendering · plat: ALL · pri: MUST

- [ ] HOUSE-02406 — Verify the Web tier budget is reachable on desktop via `--quality web`
      dep: HOUSE-02405 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-02407 — Verify the Android tier budget is reachable via `--quality android`
      dep: HOUSE-02405 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-02408 — Add the nightly performance CI job asserting every budget
      dep: HOUSE-02403 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-01031 — Measure draw calls, triangles and texture memory over the furnished house against budget
      dep: HOUSE-01030 · sys: — · plat: LNX · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01030`

- [ ] HOUSE-00853 — Measure the neighbourhood's draw-call and triangle cost from the road pose against budget
      dep: HOUSE-00852 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-00854 — Render tests: 6 poses covering the neighbourhood at 3 LOD distances, day and night
      dep: HOUSE-00852 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-00911 — Measure the lightmap atlas count, size and bake time; adjust texel density to fit the 60 MB budget
      dep: HOUSE-00910 · sys: — · plat: TOOL · pri: MUST

- [ ] HOUSE-00914 — Implement the texture streaming-free residency for the `house-*` packs (load-at-start for now)
      dep: HOUSE-00142 · sys: content · plat: ALL · pri: MUST

- [ ] HOUSE-00915 — Measure texture memory against the 300 MB budget; adjust sizes where over
      dep: HOUSE-00914, HOUSE-00203 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-00916 — Implement anisotropic filtering selection from the project-owned effective feature set (`cna-house.md` §68): request `SamplerState::MaxAnisotropy` only on a build/platform profile validated to permit it (HOUSE-00109), and sample **trilinear** (`TextureFilter::Linear` with mips) everywhere else. No CNA-specific runtime capability query — the setting row is simply not offered on a profile that does not permit it.
      dep: HOUSE-00109 · sys: rendering · plat: ALL · pri: SHOULD
      accept: on a profile without anisotropy the renderer samples trilinear and the settings row is absent; `check_xna_only.py` proves no file calls `SupportsCapability`

- [ ] HOUSE-00974 — Implement the detail-set tagging (`essential`/`dressing`/`micro`) and its culling
      dep: HOUSE-00971, HOUSE-00674 · sys: visibility · plat: ALL · pri: MUST

- [ ] HOUSE-02392 — Implement per-category LOD assignment from the manifest
      dep: HOUSE-02391 · sys: visibility · plat: ALL · pri: MUST

- [ ] HOUSE-02393 — Implement the global `lodBias` from the quality tier and settings
      dep: HOUSE-02392 · sys: visibility · plat: ALL · pri: MUST

- [ ] HOUSE-02394 — Generate LOD1/LOD2 for every asset above the triangle threshold and verify the silhouettes
      dep: HOUSE-00189, HOUSE-02392 · sys: content · plat: TOOL · pri: MUST

- [ ] HOUSE-02395 — Generate the impostor atlases for trees, shrubs and the neighbourhood houses
      dep: HOUSE-00204, HOUSE-02394 · sys: content · plat: TOOL · pri: MUST

- [ ] HOUSE-02396 — Implement the detail-set (`essential`/`dressing`/`micro`) culling by quality, distance and portal kind
      dep: HOUSE-00974 · sys: visibility · plat: ALL · pri: MUST

- [ ] HOUSE-02397 — Implement static-chunk sub-range culling for large chunks
      dep: HOUSE-00672 · sys: visibility · plat: ALL · pri: SHOULD

- [ ] HOUSE-02398 — Optimise the render list: sort key packing, stable partitioning, no per-frame allocation
      dep: HOUSE-00675 · sys: rendering · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) measurement-gated: implement only if `HOUSE-02403` shows render-list building ≥ 10 % of CPU frame time in some scenario; otherwise record the numbers and close as not needed

- [ ] HOUSE-02399 — Optimise the state tracker and measure the state-change reduction
      dep: HOUSE-00158, HOUSE-02398 · sys: rendering · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) measurement-gated like `HOUSE-02398`

- [ ] HOUSE-02400 — Evaluate `DrawInstancedPrimitives` for vegetation and the neighbourhood; adopt if it measures better (from HOUSE-00093)
      dep: HOUSE-00093, HOUSE-02395 · sys: rendering · plat: ALL · pri: SHOULD

- [ ] HOUSE-02451 — **Gate: measure cold-start load time and total resident footprint with everything loaded**
      dep: HOUSE-02403 · sys: — · plat: LNX · pri: MUST
      accept: record the numbers. If cold start ≤ 4.0 s **and** GPU ≤ 550 MB **and** RSS ≤ 1.6 GB, mark HOUSE-02452…02470 as *not required* with the measurement as the justification, and skip to phase 43.

- [ ] HOUSE-02452 — Implement `ResidencySystem`: the request set, the tier rules of §27.3 and the LRU with a 20 s grace
      dep: HOUSE-02451 · sys: content · plat: ALL · pri: SHOULD

- [ ] HOUSE-02453 — Implement pack-level load/unload over `ContentManager`
      dep: HOUSE-02452, HOUSE-00202 · sys: content · plat: ALL · pri: SHOULD

- [ ] HOUSE-02454 — Implement the 4 MB-per-frame GPU upload budget and the zone-transition promotion point
      dep: HOUSE-02453, HOUSE-00107 · sys: content · plat: ALL · pri: SHOULD

- [ ] HOUSE-02455 — Implement T1 level residency driven by the stair-well portals
      dep: HOUSE-02454 · sys: content · plat: ALL · pri: SHOULD

- [ ] HOUSE-02456 — Implement T2 proximity residency (2 portal hops) for detail sets
      dep: HOUSE-02455 · sys: content · plat: ALL · pri: SHOULD

- [ ] HOUSE-02457 — Implement T3 exterior residency
      dep: HOUSE-02456 · sys: content · plat: ALL · pri: SHOULD

- [ ] HOUSE-02458 — Implement the promotion-failure policy (essential set, one diagnostic, permanent high-water reduction, one toast)
      dep: HOUSE-02454 · sys: content · plat: ALL · pri: SHOULD

- [ ] HOUSE-02459 — Implement the `F10` content overlay (tiers, packs, memory, cache hits, recent loads)
      dep: HOUSE-02452, HOUSE-00147 · sys: debug · plat: ALL · pri: SHOULD

- [ ] HOUSE-02508 — Complete the `F10` content overlay
      dep: HOUSE-02459 · sys: debug · plat: ALL · pri: SHOULD

- [ ] HOUSE-02460 — Test: no frame during a scripted whole-house tour exceeds the frame budget because of a promotion
      dep: HOUSE-02454 · sys: ci · plat: CI · pri: SHOULD

- [ ] HOUSE-02461 — Test: walking back and forth through a door 100 times causes no thrash (the grace period holds)
      dep: HOUSE-02452 · sys: ci · plat: CI · pri: SHOULD

- [ ] HOUSE-02462 — **Q-07 decision: is a background loading thread needed?** Measure with residency in place; if promotions still hitch, plan the thread; otherwise record the decision not to
      dep: HOUSE-02460 · sys: — · plat: LNX · pri: SHOULD

- [ ] HOUSE-02463 — Phase-42 review and commit (or record the skip)
      dep: HOUSE-02451 · sys: — · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) closes M10's streaming block: record the decision whichever way `HOUSE-02451` measured

---

## M11 — Whole-project polish (C6) · after G5 only

**No M11 task starts before `HOUSE-03480` (G5).** Polish rotates zone by zone. Each round takes one
zone, schedules S3 before S4, and allows at most 2 rounds per zone per rotation (R6), so no single
room can absorb the polish budget.

- [ ] HOUSE-03631 — Work the polish backlog in zone rotation
      dep: HOUSE-03480 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D4 · est: 12
      accept: (1) every zone gets one rotation: its logged S3 findings are fixed or explicitly waived with a reason, and S4 only if the rotation's time box remains; (2) the rotation order is least recently polished first, with `Z-L0M` last; (3) after the rotation no S1/S2 is open and the S3 backlog is triaged (D4)
      verify: one ledger round per zone rotation
      note: split per zone by R7 when scheduled (≈ 1–1.5 h per zone).

- [ ] HOUSE-02686 — Grouped: the habitation pass — place the ~25 signs-of-habitation items of `cna-house.md` §59.3
      dep: HOUSE-01033 · sys: world · plat: TOOL · pri: MUST

- [ ] HOUSE-02687 — Render the fictional family photographs in Blender using the game's own characters and rooms (9 gallery frames + 6 stair frames)
      dep: HOUSE-03480 · sys: content · plat: TOOL · pri: MUST
      accept: no real person is depicted; the images are ours outright (D-26)
      amended: (2026-09-21, `HOUSE-03201`) the images are rendered from the project's own house, garden and neighbourhood (no human figures, since the avatar is removed); CC0/project-owned
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-02183`

- [ ] HOUSE-02689 — Author the corkboard, whiteboard, timetable, height chart and child's drawing as small texture props
      dep: HOUSE-02686 · sys: content · plat: TOOL · pri: MUST

- [ ] HOUSE-02684 — Implement the mirror cube maps and the `EnvironmentMapEffect` mirror material
      dep: HOUSE-00209, HOUSE-00896 · sys: rendering · plat: ALL · pri: SHOULD

- [ ] HOUSE-02691 — Implement fireplace embers, flame and the fire loop audio in `L0_LIVING`, and the garden fire pit
      dep: HOUSE-01741, HOUSE-00279 · sys: weather · plat: ALL · pri: SHOULD

- [ ] HOUSE-02693 — Polish pass: interior lighting balance room by room, re-baking where needed
      dep: HOUSE-01030 · sys: content · plat: TOOL · pri: MUST

- [ ] HOUSE-02694 — Polish pass: exterior lighting and the night-time neighbourhood
      dep: HOUSE-00850 · sys: content · plat: TOOL · pri: MUST

- [ ] HOUSE-02695 — Polish pass: material tuning against reference photographs, room by room
      dep: HOUSE-02693 · sys: content · plat: TOOL · pri: MUST

- [ ] HOUSE-02696 — Polish pass: exposure and adaptation tuning across the 8 indoor↔outdoor transitions
      dep: HOUSE-01266 · sys: lighting · plat: LNX · pri: MUST

- [ ] HOUSE-02697 — Polish pass: the six sky states and the sunrise/sunset colour ramp
      dep: HOUSE-01652 · sys: content · plat: TOOL · pri: MUST

- [ ] HOUSE-02698 — Polish pass: particle look for rain, snow and hail at every intensity
      dep: HOUSE-01754, HOUSE-01800 · sys: weather · plat: LNX · pri: MUST

- [ ] HOUSE-02699 — Polish pass: audio mix across every category, every room and every weather state
      dep: HOUSE-01939 · sys: audio · plat: LNX · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-02008`

- [ ] HOUSE-02701 — Polish pass: UI typography, spacing, colour and the prompt's readability against bright and dark backgrounds
      dep: HOUSE-02530 · sys: ui · plat: LNX · pri: MUST

- [ ] HOUSE-01033 — Furnishing review: walk the whole house and list what reads as fake; fix it
      dep: HOUSE-01032, HOUSE-03480 · sys: — · plat: LNX · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) input to the whole-project polish (M11); every finding is classified S1–S4 and S3/S4 go to the polish backlog
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-01032`

- [ ] HOUSE-02713 — Grouped: re-render the whole render-test corpus after polish and accept the new references
      dep: HOUSE-02701 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02714 — Phase-45 review: a full walkthrough with fresh eyes; list everything that still reads as fake; fix or schedule
      dep: HOUSE-02713 · sys: — · plat: LNX · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) this is the whole-property walkthrough with every zone at C5 or better, not a ground-floor route

### M11b — Tier E showcase effects (optional)

SHOULD, after G5, and only while the High-tier budget holds (`HOUSE-02404`). Compiled `.fx` effects
through CNA's content pipeline are a genuine CNA showcase, but Tier S must stay complete without
them (ADR-0003).

- [ ] HOUSE-02702 — Implement the Tier E `RoomLit.fx` and its four techniques
      dep: HOUSE-00087, HOUSE-01256 · sys: rendering · plat: ALL · pri: SHOULD
      amended: (2026-09-21, `HOUSE-03201`) optional Tier E block (M11b): SHOULD, after G5, only while the High-tier budget holds; DONE requires Tier S complete, and any shipped Tier E path must have its Tier S fallback proved

- [ ] HOUSE-02703 — Implement the Tier E `ShadowDepth.fx` and the shadow map with the frustum-fitted light projection and texel snapping
      dep: HOUSE-02702, HOUSE-00083 · sys: rendering · plat: ALL · pri: SHOULD
      amended: (2026-09-21, `HOUSE-03201`) optional Tier E block (M11b): SHOULD, after G5, only while the High-tier budget holds; DONE requires Tier S complete, and any shipped Tier E path must have its Tier S fallback proved

- [ ] HOUSE-02704 — Implement 3×3 PCF and the shadow's indoor disable rule
      dep: HOUSE-02703 · sys: rendering · plat: ALL · pri: SHOULD
      amended: (2026-09-21, `HOUSE-03201`) optional Tier E block (M11b): SHOULD, after G5, only while the High-tier budget holds; DONE requires Tier S complete, and any shipped Tier E path must have its Tier S fallback proved

- [ ] HOUSE-02706 — Implement the Tier E `SkyDome.fx` with the per-pixel gradient, three cloud layers, sun disc and dithering
      dep: HOUSE-02702, HOUSE-01650 · sys: rendering · plat: ALL · pri: SHOULD
      amended: (2026-09-21, `HOUSE-03201`) optional Tier E block (M11b): SHOULD, after G5, only while the High-tier budget holds; DONE requires Tier S complete, and any shipped Tier E path must have its Tier S fallback proved

- [ ] HOUSE-02707 — Implement the Tier E `Precip.fx` with soft-edge depth fade and wind shear
      dep: HOUSE-02702, HOUSE-01741 · sys: rendering · plat: ALL · pri: SHOULD
      amended: (2026-09-21, `HOUSE-03201`) optional Tier E block (M11b): SHOULD, after G5, only while the High-tier budget holds; DONE requires Tier S complete, and any shipped Tier E path must have its Tier S fallback proved

- [ ] HOUSE-02708 — Implement the Tier E `SurfaceBlend.fx` for continuous wetness and snow
      dep: HOUSE-02702, HOUSE-01748 · sys: rendering · plat: ALL · pri: SHOULD
      amended: (2026-09-21, `HOUSE-03201`) optional Tier E block (M11b): SHOULD, after G5, only while the High-tier budget holds; DONE requires Tier S complete, and any shipped Tier E path must have its Tier S fallback proved

- [ ] HOUSE-02709 — Implement the Tier E `PostComposite.fx` with exposure adaptation and the glare composite
      dep: HOUSE-02702, HOUSE-01266 · sys: rendering · plat: ALL · pri: SHOULD
      amended: (2026-09-21, `HOUSE-03201`) optional Tier E block (M11b): SHOULD, after G5, only while the High-tier budget holds; DONE requires Tier S complete, and any shipped Tier E path must have its Tier S fallback proved

- [ ] HOUSE-02595 — Implement the Tier S vs Tier E content-identity test (same scene content, different pixels)
      dep: HOUSE-02593 · sys: ci · plat: CI · pri: SHOULD
      amended: (2026-09-21, `HOUSE-03201`) priority MUST → SHOULD; needed only if a Tier E path ships (M11b)

- [ ] HOUSE-02711 — Verify every Tier E path has a working Tier S fallback and that forcing Tier S loses no scene content
      dep: HOUSE-02702, HOUSE-02703, HOUSE-02704, HOUSE-02706, HOUSE-02707, HOUSE-02708, HOUSE-02709, HOUSE-02595 · sys: ci · plat: CI · pri: SHOULD
      amended: (2026-09-21, `HOUSE-03201`) optional Tier E block (M11b): SHOULD, after G5, only while the High-tier budget holds; DONE requires Tier S complete, and any shipped Tier E path must have its Tier S fallback proved
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-02702…HOUSE-02710, HOUSE-02595`; priority was MUST

- [ ] HOUSE-02712 — Measure Tier E against budget and against Tier S
      dep: HOUSE-02711 · sys: — · plat: LNX · pri: SHOULD
      amended: (2026-09-21, `HOUSE-03201`) optional Tier E block (M11b): SHOULD, after G5, only while the High-tier budget holds; DONE requires Tier S complete, and any shipped Tier E path must have its Tier S fallback proved
      amended: (2026-09-21, `HOUSE-03201`) priority was MUST

---

## M12 — Test completion

Track B. Keep the regression net aligned with the reduced scope. Tests for removed systems are not
gaps (ADR-0014).

- [ ] HOUSE-00493 — Two gates failed once each under load and did not reproduce
      dep: HOUSE-00483 · sys: ci · plat: CI · pri: SHOULD
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

- [ ] HOUSE-02571 — Audit the existing suite against §70 and produce a gap list
      dep: HOUSE-03240 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) the audit is against the reduced scope (ADR-0014): suites for removed systems are not gaps
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-02531`

- [ ] HOUSE-02572 — Grouped: complete the world-data unit tests (schemas, loader, ids, adjacency, plane membership, stair maths)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02573 — Grouped: complete the visibility unit tests (clip, reduce, containment property, area cutoff, depth caps, hysteresis)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02574 — Grouped: complete the collision unit tests (every primitive against analytic answers, 200 cases each)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02576 — Grouped: complete the weather unit tests (rate limits, determinism, seasons, type derivation, integration channels)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02577 — Grouped: complete the astronomy unit tests (sun, moon, phase, sidereal, twilight)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02580 — Grouped: complete the audio unit tests (portal path, voice manager, footstep cadence, bags)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) covers footstep cadence, bags, ambience routing and cell-gated emitters (no portal path, no voice manager)

- [ ] HOUSE-02581 — Grouped: complete the lighting unit tests (daylight model, flood, exposure, colour temperature)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02582 — Implement the ID-stability test against the golden list (adding is fine, renaming fails)
      dep: HOUSE-00399 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02583 — Grouped: complete the integration tests for doors and visibility (all 62, both directions, both sides)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) over the static door and gate poses: every leafed portal's visibility and collision agree with its pose, from both sides

- [ ] HOUSE-02584 — Grouped: complete the integration tests for lights and illumination (all 84 groups, plus the 12 §30 rows)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02588 — Grouped: complete the long-run integration tests (30 simulated days: lunation, weather variety, no leak, no drift)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02590 — Grouped: complete the render regression scenes for time of day (64 scenes)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02591 — Grouped: complete the render regression scenes for weather (48 scenes)
      dep: HOUSE-02590 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02592 — Grouped: complete the render regression scenes for room lighting (80 scenes across 20 rooms)
      dep: HOUSE-02590 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) the 20 rooms span every level

- [ ] HOUSE-02593 — Grouped: complete the render regression scenes for doors, culling sanity, tiers, characters, exterior and UI
      dep: HOUSE-02590 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) covers door poses, culling sanity, tiers, exterior and UI; the characters are removed

- [ ] HOUSE-02594 — Implement the render-test tolerance policy (per-pixel and mean-absolute-difference budgets) and the difference-image artefact
      dep: HOUSE-02590 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02596 — Complete the realism-validation suite over the layout and every asset
      dep: HOUSE-00360, HOUSE-00187 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02597 — Complete the performance test suite (10 scenarios, all budgets, nightly, logged)
      dep: HOUSE-02408 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02598 — Implement the soak test: 2 hours unattended at 60 FPS, RSS growth < 20 MB/hour, no crash, no audio starvation, 200 autosaves
      dep: HOUSE-03226 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) the 200 autosaves become ≥ 200 settings/session-file writes
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-02589`

- [ ] HOUSE-02599 — Measure and record test-suite runtime; keep the `unit` target under 60 s
      dep: HOUSE-02597 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02600 — Write `docs/testing.md`: what each suite covers, how to run it, how to add a case, how to accept a render-test change
      dep: HOUSE-02599 · sys: — · plat: ALL · pri: MUST

---

## M13 — Linux desktop release

After M11. The DONE audit for the desktop build.

- [ ] HOUSE-02781 — Work through the §79 feature-complete checklist item by item; open a task for every gap
      dep: HOUSE-02714 · sys: — · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) the checklist is `plan.md`'s **Definition of DONE** (desktop items D1–D9, D11–D13), not the legacy §79 list

- [ ] HOUSE-02782 — Fix every gap found by HOUSE-02781
      dep: HOUSE-02781 · sys: — · plat: ALL · pri: MUST

- [ ] HOUSE-02783 — Run the full suite under ASAN and UBSAN; fix every report
      dep: HOUSE-02782, HOUSE-00137 · sys: — · plat: CI · pri: MUST

- [ ] HOUSE-02784 — Run the 2-hour soak test; fix every leak and every drift
      dep: HOUSE-02598 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-02785 — Verify the failure-handling table of `cna-house.md` §73 row by row by inducing each failure
      dep: HOUSE-02782 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) rows for removed systems (household saves, pets, television) are dropped

- [ ] HOUSE-02786 — Verify the "zero TODO/TBD/FIXME in shipping paths" rule
      dep: HOUSE-02782 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02787 — Verify `docs/xna-deviations.md` is complete, that it still grants no permission to call a CNA symbol, and that the lint enforces the §70.1 rule with no allowlist
      dep: HOUSE-02782, HOUSE-00020 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02788 — Verify every asset is manifested and licensed, and regenerate `THIRD-PARTY-ASSETS.md`
      dep: HOUSE-00299 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02789 — Package: a distributable Linux build with its content, licences and a launch script
      dep: HOUSE-02788 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-02790 — Verify the packaged build runs on a clean machine profile (no dev environment, no sibling checkouts)
      dep: HOUSE-02789 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-02791 — Verify the packaged build with `CNA_ENABLE_VIDEO=OFF` and with no audio device
      dep: HOUSE-02790 · sys: — · plat: LNX · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) the game no longer uses video; `CNA_ENABLE_VIDEO=OFF` remains a build variant to prove harmless

- [ ] HOUSE-02792 — Verify the game at 4 quality tiers, 5 resolutions and 3 aspect ratios
      dep: HOUSE-02790 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-02793 — **Q-11 re-evaluation: is 24 minutes the right day length after playtesting?** Record the decision
      dep: HOUSE-02790 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-02794 — **Q-10 decision: pause-on-menu default** after playtesting
      dep: HOUSE-02790 · sys: — · plat: LNX · pri: MUST

- [ ] HOUSE-02795 — Update `cna-house.md` to match what was actually built; every divergence either fixed or documented
      dep: HOUSE-02792 · sys: — · plat: ALL · pri: MUST

- [ ] HOUSE-02796 — Write the release notes and the known-limitations list (including the mirror reflection, D-19)
      dep: HOUSE-02795 · sys: — · plat: ALL · pri: MUST

- [ ] HOUSE-02797 — Tag the feature-complete desktop release
      dep: HOUSE-02796 · sys: — · plat: ALL · pri: MUST

---

## M14 — Web

**Web stays a target platform.** Every task here is MUST for DONE (D10) except `HOUSE-02902`. The
canary tasks `HOUSE-02843` and `HOUSE-02855` may run early as Track B. The rest follows M13's
stabilisation. The design is the same walkthrough, scaled by the Web quality tier (`cna-house.md`
§9.1). No separate Web game design is planned.

- [ ] HOUSE-03721 — Web controls: pointer-lock mouse look, keyboard walk, focus handling
      dep: HOUSE-02892 · sys: player · plat: WEB · pri: MUST · zone: all · adv: D10 · est: 2
      accept: (1) a click on the canvas acquires pointer lock and mouse look works without drift; (2) Esc and focus loss release the pointer and open the pause menu; (3) WASD/arrows and the walk-speed modifier work; (4) verified in Chrome and Firefox; (5) on a touch-only browser the `TouchSource` scheme of M15 is used if `TouchPanel` reports touch under Emscripten, and otherwise the limitation is recorded
      verify: manual browser check recorded in `docs/portability.md`; headless smoke (`HOUSE-02901`)

- [ ] HOUSE-02843 — Audit and fix any custom loop or `Game::Run` misuse
      dep: HOUSE-00127 · sys: app · plat: ALL · pri: MUST

- [ ] HOUSE-02855 — Spike: an Emscripten build of a minimal scene (one room, one prop, one sound) to validate the toolchain and the Asyncify interaction
      dep: HOUSE-02843 · sys: app · plat: WEB · pri: MUST
      accept: it runs in Chrome; this is the gate for phase 48

- [ ] HOUSE-02842 — Audit and fix every filesystem access outside `DesktopSaveStore`
      dep: HOUSE-02841 · sys: — · plat: ALL · pri: MUST

- [ ] HOUSE-02844 — Implement the desktop context-loss test using EasyGL's `DebugSimulateContextLoss`, destroying and rebuilding every GPU resource and comparing a rendered frame
      dep: HOUSE-00108 · sys: ci · plat: CI · pri: MUST
      accept: **every GPU resource is reconstructible from CPU-side data** — the single hardest Web requirement, proved on Linux

- [ ] HOUSE-02845 — Fix every resource that fails HOUSE-02844
      dep: HOUSE-02844 · sys: rendering · plat: ALL · pri: MUST

- [ ] HOUSE-02846 — Verify the user-gesture audio gate is on the desktop path too
      dep: HOUSE-00155 · sys: audio · plat: ALL · pri: MUST

- [ ] HOUSE-02848 — Verify every runtime texture is `SurfaceFormat::Color` with pre-generated mips (lint over the material data)
      dep: HOUSE-00110 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02849 — Verify no MRT, no stencil, no geometry/tessellation stage is used anywhere
      dep: HOUSE-00085, HOUSE-00086 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02850 — Verify every pack is ≤ 60 MB and the Web-tier total is ≤ 180 MB compressed
      dep: HOUSE-00203, HOUSE-02406 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-02851 — Reduce content where HOUSE-02850 fails: Web-tier texture sizes, audio bitrates, LOD-only packs
      dep: HOUSE-02850 · sys: content · plat: TOOL · pri: MUST

- [ ] HOUSE-02852 — Implement the Web quality tier fully and verify it on Linux via `--quality web`
      dep: HOUSE-02406 · sys: rendering · plat: ALL · pri: MUST

- [ ] HOUSE-02853 — Implement `WebSaveStore` against `System::IO::IsolatedStorage`, and test it on Linux behind a flag
      dep: HOUSE-00152 · sys: persistence · plat: ALL · pri: MUST

- [ ] HOUSE-02854 — Implement the loading-screen progress model driven by pack loads
      dep: HOUSE-00156, HOUSE-02453 · sys: ui · plat: ALL · pri: MUST

- [ ] HOUSE-02856 — Record the phase-47 readiness matrix in `docs/portability.md`
      dep: HOUSE-02855 · sys: — · plat: ALL · pri: MUST

- [ ] HOUSE-02857 — Phase-47 review and commit
      dep: HOUSE-02842, HOUSE-02843, HOUSE-02844, HOUSE-02845, HOUSE-02846, HOUSE-02848, HOUSE-02849, HOUSE-02850, HOUSE-02851, HOUSE-02852, HOUSE-02853, HOUSE-02854, HOUSE-02855, HOUSE-02856 · sys: — · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-02841…HOUSE-02856`

- [ ] HOUSE-02891 — CMake: the Emscripten preset with `WEBGL2`, the exception ABI, Asyncify and the pack preloads
      dep: HOUSE-02855 · sys: app · plat: WEB · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority SHOULD → MUST for every M14 task except `HOUSE-02902` (DONE item D10 requires the Web build)

- [ ] HOUSE-02892 — Build the full game for Emscripten and fix every compile and link error
      dep: HOUSE-02891 · sys: — · plat: WEB · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02893 — Wire `WebSaveStore` and verify persistence across a page reload
      dep: HOUSE-02892, HOUSE-02853 · sys: persistence · plat: WEB · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02895 — Wire the canvas-as-display model into the Display settings
      dep: HOUSE-02892 · sys: ui · plat: WEB · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02896 — Verify the audio gesture gate in a real browser
      dep: HOUSE-02892 · sys: audio · plat: WEB · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02897 — Verify context loss and restore in a real browser using `WEBGL_lose_context`
      dep: HOUSE-02892, HOUSE-02844 · sys: rendering · plat: WEB · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02898 — Measure the Web build against its budget; reduce content until it fits
      dep: HOUSE-02892 · sys: — · plat: WEB · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02899 — Implement the browser loading screen and the progressive pack fetch
      dep: HOUSE-02854, HOUSE-02892 · sys: ui · plat: WEB · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02900 — Verify the whole feature set in a browser: every phase's headline feature, tested by hand against a checklist
      dep: HOUSE-02898 · sys: — · plat: WEB · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) checked against the DONE checklist's Web items (the showcase walkthrough), not the legacy feature set
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02901 — Implement a headless-Chrome smoke test in CI (menu, load, 300 frames, one interaction, one save)
      dep: HOUSE-02900 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) menu, load, 300 frames of the walk, one settings save; there is no interaction
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02902 — Evaluate whether threads are needed (COOP/COEP cost vs. load time); decide and record
      dep: HOUSE-02898 · sys: — · plat: WEB · pri: OPT

- [ ] HOUSE-02903 — Multi-browser check: Chrome, Firefox; record what differs
      dep: HOUSE-02900 · sys: — · plat: WEB · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02904 — Package and document the Web build
      dep: HOUSE-02903 · sys: — · plat: WEB · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02905 — Phase-48 review and commit
      dep: HOUSE-02891…HOUSE-02904 · sys: — · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

---

## M15 — Android

**Android stays a target platform.** The device tasks are gated on CNA's Android graphics path
(BL-13, upstream; `cna-house` never modifies CNA). The **desktop-side readiness tasks do not wait**
for that gate: input abstraction, `--force-touch`, the Android tier, safe areas and lifecycle
mapping. The touch scheme is a floating movement stick, a look region, a walk-speed toggle and a
menu button. There is no interact button and no camera toggle. Every task here is MUST for DONE
(D10), to the extent the upstream gate allows.

- [ ] HOUSE-02954 — Verify the `IInputSource` abstraction is complete (no direct `Keyboard`/`Mouse` reads anywhere)
      dep: HOUSE-00140 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority SHOULD → MUST for every M15 task (DONE item D10); the device tasks remain gated on `HOUSE-02951`–`HOUSE-02953`, and a verified upstream block is handled as a recorded blocker, never a tick

- [ ] HOUSE-02955 — Verify the `Platform` capability struct drives HUD and defaults, using a forced-touch desktop mode
      dep: HOUSE-00141 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02956 — Verify the UI at 18:9 and 20:9 with safe-area insets on desktop
      dep: HOUSE-02527 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02957 — Implement the Android quality tier and verify it on desktop via `--quality android`
      dep: HOUSE-02407 · sys: rendering · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02958 — Verify the content packs fit the APK + OBB budget
      dep: HOUSE-00203, HOUSE-02957 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02991 — Implement `TouchSource` over `TouchPanel`, with multi-touch tracking and gesture recognition
      dep: HOUSE-00101, HOUSE-00140 · sys: player · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02992 — Implement the floating movement stick (bottom-left, 180 vu, analogue direction and magnitude)
      dep: HOUSE-02991 · sys: ui · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02993 — Implement the look region (right half minus buttons) with its own sensitivity setting
      dep: HOUSE-02991 · sys: ui · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02995 — Implement the walk-mode, camera and menu buttons
      dep: HOUSE-02992 · sys: ui · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) walk-speed toggle and menu buttons only (the camera toggle is removed: first person only)
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-02994`; priority was SHOULD

- [ ] HOUSE-02996 — Implement the touch-HUD visibility rule (`hasTouch && !hasKeyboard`) so desktop never shows it
      dep: HOUSE-02995, HOUSE-00141 · sys: ui · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02997 — Implement the `--force-touch` desktop flag so the touch HUD is testable and screenshot-able on Linux
      dep: HOUSE-02996 · sys: debug · plat: LNX · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02998 — Implement touch-friendly menu hit targets (≥ 88 vu) without changing the desktop layout
      dep: HOUSE-02996 · sys: ui · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03000 — Render tests: the touch HUD at 3 aspect ratios via `--force-touch`
      dep: HOUSE-02997 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03001 — Playtest the touch controls on desktop with a touchscreen or a simulator; tune
      dep: HOUSE-03000 · sys: ui · plat: LNX · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03002 — Phase-50 review
      dep: HOUSE-02991…HOUSE-03001 · sys: — · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02951 — **Gate: confirm CNA's Android cross-compile succeeds** — `sharp-runtime`'s two NDK-portability bugs fixed upstream (CNA Task 920)
      dep: HOUSE-02905 · sys: — · plat: AND · pri: MUST
      accept: `cmake --build` completes for `arm64-v8a`; if not, this phase stops here and is revisited later
      amended: (2026-09-21, `HOUSE-03201`) re-verify BL-13 at the time this task is picked (the last recorded state is *blocked*, 2026-09-06); desktop-side readiness tasks (`HOUSE-02954`–`02958`, `02960`, `02991`–`02998`, `03000`, `03001`) do not wait for this gate
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02952 — **Gate: confirm `CNA_GRAPHICS_RENDERER=OPENGLES3` is selectable and buildable for Android**
      dep: HOUSE-02951 · sys: — · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02953 — **Gate: confirm a CNA graphics sample runs on a device or emulator**
      dep: HOUSE-02952 · sys: — · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02959 — Design the Android lifecycle mapping (pause, resume, background, surface loss) onto `Game`'s events
      dep: HOUSE-02953 · sys: app · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02960 — Verify the desktop focus-loss path exercises the same lifecycle code
      dep: HOUSE-02959 · sys: ci · plat: CI · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02961 — Record the phase-49 readiness matrix in `docs/portability.md`
      dep: HOUSE-02960 · sys: — · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-02962 — Phase-49 review
      dep: HOUSE-02951…HOUSE-02961 · sys: — · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03031 — Create the Gradle/NDK project producing a shared library plus `SDLActivity`, following CNA's own devices-demo precedent
      dep: HOUSE-02962 · sys: app · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03032 — Build the full game for `arm64-v8a` and fix every compile and link error
      dep: HOUSE-03031 · sys: — · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03033 — Wire content delivery (APK assets or OBB) and the content root
      dep: HOUSE-03032, HOUSE-02958 · sys: content · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03034 — Wire the save store to Android's app-private storage
      dep: HOUSE-03032, HOUSE-00152 · sys: persistence · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03036 — Wire the lifecycle mapping and verify pause/resume/background on a device
      dep: HOUSE-03032, HOUSE-02959 · sys: app · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03037 — Run on a real device; measure against the Android budget; reduce until it fits
      dep: HOUSE-03036 · sys: — · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03038 — Verify the whole feature set on a device against the checklist
      dep: HOUSE-03037 · sys: — · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) checked against the DONE checklist's Android items
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03039 — Verify the touch controls on a device and tune
      dep: HOUSE-03038, HOUSE-03001 · sys: ui · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03040 — Test on at least 3 devices spanning GPU vendors; record what differs
      dep: HOUSE-03039 · sys: — · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) ≥ 1 physical device plus an emulator; more GPU vendors when available; record what differs
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03041 — Package and document the Android build
      dep: HOUSE-03040 · sys: — · plat: AND · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

- [ ] HOUSE-03042 — Phase-51 review and commit
      dep: HOUSE-03031…HOUSE-03041 · sys: — · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) priority was SHOULD

---

## M16 — Final release

- [ ] HOUSE-03071 — Profile the release build on all three platforms and produce a prioritised optimisation list
      dep: HOUSE-02797, HOUSE-02905, HOUSE-03042 · sys: — · plat: ALL · pri: MUST
      amended: (2026-09-21, `HOUSE-03201`) if BL-13 is still blocked upstream when every other milestone is done, `HOUSE-03042` is satisfied by the recorded blocker per DONE item D10
      amended: (2026-09-21, `HOUSE-03201`) dependencies were `HOUSE-03042`

- [ ] HOUSE-03072 — Work the optimisation list until every platform meets its budget with 15 % headroom
      dep: HOUSE-03071 · sys: — · plat: ALL · pri: MUST

- [ ] HOUSE-03073 — Final content audit: every asset used, no orphans, every licence recorded, `THIRD-PARTY-ASSETS.md` regenerated
      dep: HOUSE-03072 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-03074 — Final XNA-only audit: the lint, the `nm -C` symbol check, `check_anim_assets.py`, and a manual review confirming that no runtime source calls a CNA-specific graphics or model API — no `SupportsCapability`, no `*EXT*` call, no `Model::Tag` read
      dep: HOUSE-03073 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-03075 — Final documentation pass: `cna-house.md`, `README.md`, `docs/*` all reflect the shipped software
      dep: HOUSE-03074 · sys: — · plat: ALL · pri: MUST

- [ ] HOUSE-03076 — Final test pass: the whole suite green on all platforms; the soak test on Linux and Web
      dep: HOUSE-03075 · sys: ci · plat: CI · pri: MUST

- [ ] HOUSE-03077 — Write the final release notes, the known-limitations list and the credits
      dep: HOUSE-03076 · sys: — · plat: ALL · pri: MUST

- [ ] HOUSE-03078 — Tag the release
      dep: HOUSE-03077 · sys: — · plat: ALL · pri: MUST

- [ ] HOUSE-03079 — Post-release retrospective: what the plan got wrong, recorded for the next project
      dep: HOUSE-03078 · sys: — · plat: ALL · pri: SHOULD

---

## Deferred until after DONE

Real ideas with real ids, **never scheduled before DONE** (`pri: OPT`). They do not block anything,
and no agent should pick one while any MUST task is open.

- [ ] HOUSE-01873 — Implement the short-lived hail-stone ground scatter above intensity 0.5
      dep: HOUSE-01872 · sys: weather · plat: ALL · pri: OPT
      amended: (2026-09-21, `HOUSE-03201`) deferred until after DONE; never scheduled while a MUST task is open

- [ ] HOUSE-01886 — Implement the weather vane on the shed pointing into the wind
      dep: HOUSE-01876 · sys: rendering · plat: ALL · pri: OPT
      amended: (2026-09-21, `HOUSE-03201`) deferred until after DONE; never scheduled while a MUST task is open

- [ ] HOUSE-01887 — Implement the swing bench and wind chimes ambient motion
      dep: HOUSE-01876 · sys: rendering · plat: ALL · pri: OPT
      amended: (2026-09-21, `HOUSE-03201`) deferred until after DONE; never scheduled while a MUST task is open

- [ ] HOUSE-02401 — **Q-08 experiment: `OcclusionQuery` for exterior occlusion.** Queries around the 24 neighbourhood LOD groups, a 60-second recorded camera path, A/B GPU timing
      dep: HOUSE-01567, HOUSE-00852 · sys: rendering · plat: LNX · pri: OPT
      accept: adopt only if it saves ≥ 0.6 ms GPU at no visual cost; otherwise record the measurement and drop the idea
      amended: (2026-09-21, `HOUSE-03201`) deferred until after DONE; never scheduled while a MUST task is open

- [ ] HOUSE-02685 — **Q-05 decision: measure a planar reflection pass for one mirror; adopt or record the rejection**
      dep: HOUSE-02684 · sys: rendering · plat: LNX · pri: OPT
      amended: (2026-09-21, `HOUSE-03201`) deferred until after DONE; never scheduled while a MUST task is open

- [ ] HOUSE-02690 — Implement the dust-motes-in-sunbeam particle effect where a sun patch is strong
      dep: HOUSE-01565, HOUSE-01741 · sys: weather · plat: ALL · pri: OPT
      amended: (2026-09-21, `HOUSE-03201`) deferred until after DONE; never scheduled while a MUST task is open

Without ids (would need an R9 entry first): an attract mode that auto-walks the grand-tour route
for demonstrations; seasonal decorations; a drivable car (still Q-06, a different project).

---

## Cancelled by the scope reduction

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

---

## Legacy phases → milestones

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

Agent-hours, estimated from the task count and type per milestone. They are calibrated on the
2026-09-14 → 2026-09-20 sprint, where ≈ 115 visual tasks took ≈ 200 agent-hours (≈ 1.7 h each),
and on the per-task `est:` of new tasks. These are planning numbers for rule R10, not promises.

| Milestone | Open tasks | Low | High |
|---|---|---|---|
| M0 Scope reset and breadth instruments | 3 | 7 | 10 |
| M1 Whole-property traversal | 8 | 18 | 28 |
| M2 Architectural completion | 9 | 25 | 40 |
| M3 Furnishing toolkit | 17 | 45 | 70 |
| M4 Baseline dressing everywhere | 42 | 85 | 135 |
| M5 Baseline materials and lighting everywhere | 22 | 45 | 70 |
| M6 Showcase baseline everywhere | 26 | 65 | 95 |
| M7 Environment systems | 66 | 65 | 95 |
| M8 Atmospheric audio | 28 | 28 | 42 |
| M9 Application shell | 26 | 27 | 40 |
| M10 Performance and budgets | 38 | 30 | 55 |
| M11 Whole-project polish (+ optional M11b Tier E) | 27 (10 of them M11b, estimated below) | 45 | 65 |
| M12 Test completion | 23 | 26 | 40 |
| M13 Linux desktop release | 17 | 20 | 32 |
| M14 Web | 30 | 45 | 70 |
| M15 Android | 33 | 45 | 75 |
| M16 Final release | 9 | 14 | 24 |
| **Desktop DONE-candidate (M0–M13)** | | **531** | **817** |
| **Full DONE, all three platforms (M0–M16)** | **424** | **635** | **986** |
| *M11b Tier E, optional, not in DONE* | | *25* | *35* |

**Critical path to a desktop DONE:** M0 → M1 (G1) → M2 (G2) → M3 → M4 (G3) → M5 (G4) → M6 (G5) →
M11 → M13, with M7–M10 and M12 interleaved under rule R5. Then M14 and M15 (the Android device
half is gated on BL-13) and M16.

For comparison, the 764 legacy open tasks, including 286 in the removed systems, several of
which (avatar, pets, 640 interactables) were projects of their own, would have been roughly
**1 500–1 900 agent-hours** at the same rates.

---

## Planning corrections

Corrections made to `cna-house.md` or to this file during implementation, with the evidence that
forced each one. Nothing is changed silently. The 2026-09-06 → 2026-09-20 corrections (about 85 KB)
are in the legacy ledger's *Planning corrections* section.

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
