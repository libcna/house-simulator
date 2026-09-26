# CNA House — Showcase Completion Plan

`STATUS: APPROVED — IMPLEMENTATION IN PROGRESS · FINAL SCOPE 2026-09-22 (HOUSE-03206, ADR-0016) · earlier reductions 2026-09-21 (HOUSE-03201, ADR-0014; HOUSE-03205, ADR-0015)`

This file is the **execution ledger for the remaining work, and the only current source of truth
for what that work is.** A checkbox moves to `[x]` only when the task's acceptance criteria are
genuinely satisfied and its `verify:` step has been run. Task ids are permanent and are never
renumbered.

| | |
|---|---|
| Goal | A finished, multiplatform **architectural and graphics walkthrough showcase for CNA**. See [Direction](#direction) |
| Scope decisions | [ADR-0014](docs/decisions/ADR-0014-showcase-scope.md) (a showcase, not a life simulator), [ADR-0015](docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md) (quality tiers, a reusable kit, a compact feature set), [ADR-0016](docs/decisions/ADR-0016-final-scope-reduction.md) (**the final reduction**: canonical tasks, five hero areas, a 280-hour ceiling, maintenance mode after DONE) |
| **This is the final proactive scope reduction** | No further broad replanning pass is scheduled. Scope changes from here on are the targeted corrections of rule [R15](#scheduling-rules--time-discipline) only |
| Active plan | **33 open MUST tasks** in 16 milestones. Every active task's title and acceptance state the current work; nothing has to be read "plus its amendments" |
| Estimate to DONE | **≈ 38.5 / 45.25 / 67.78 agent-hours** (optimistic / realistic / pessimistic), Linux, Web and Android included. **Hard ceiling 280 h** (rule R14). See [Remaining-work estimate](#remaining-work-estimate) |
| Current gate | **G5, M7, M9 and M12 passed; CNA's Android graphics gate and House's arm64 APK/settings/lifecycle path passed on the `Medium_Phone` emulator.** The Release desktop baseline also passes; `HOUSE-02404` is next, followed by `HOUSE-02405` before Android content delivery. `HOUSE-01920`/`HOUSE-01922` remain independently open (listening unavailable here) |
| History | [`docs/history/plan-legacy-2026-09-21.md`](docs/history/plan-legacy-2026-09-21.md): the original 53-phase ledger (674 tasks completed before 2026-09-21). [`docs/history/scope-reductions-2026-09-21.md`](docs/history/scope-reductions-2026-09-21.md): what the first two reductions cancelled and why. Neither is a requirement |

**Read in this order:** [Direction](#direction) → [Non-goals](#non-goals--not-required-for-done) →
[Definition of DONE](#definition-of-done) → [Quality tiers](#quality-tiers-and-room-classification) →
[Scheduling rules](#scheduling-rules--time-discipline) → [Zone scoreboard](#zone-scoreboard) → the
milestone you are working in.

---

## Direction

House Simulator (`cna-house`) is **a CNA architectural and graphics showcase and stress test.** It
succeeds when someone can start it, walk from the street through a convincing furnished property,
explore the house from the basement to the attic and the garage, see good CNA rendering and
atmosphere, and run the same showcase on Linux desktop, in a browser and on Android. It
demonstrates that CNA, through the XNA 4.0 API alone (ADR-0001), carries a substantial realistic 3-D
environment on every platform it targets.

It is **not** a life, household, character, pet, weather or audio simulator, not an interaction
sandbox, not a content-production competition and not a certification project. Objects are
**static dressing**. Doors and gates have **static poses** so that every area stays reachable.
Lights follow an **automatic schedule**.

**Breadth is fixed; depth is what gets cut.** Every floor, the basement, the attic, the garage, the
garden and approach, the Linux build, the Web build and the Android build stay. What is cut is
unique content, polish, simulation detail, validation bureaucracy and supporting systems. The shape
of the project is: **complete the whole house to a common baseline first, then beautify five hero
areas, then ship on three platforms and stop.**

What matters, in priority order:

1. **Complete architecture and reliable traversal.** Every zone is present and walkable, proved by
   the grand tour: street → property → house → basement → ground floor → both upper floors → attic
   → exterior areas → back.
2. **Every room furnished enough to read as its purpose**, from a small reusable kit. A believable
   reused asset is better than an unfinished room.
3. **Visual quality in tiers:** a baseline everywhere (C3), presentation-ready main rooms (C4) and
   five showcase hero areas (C5).
4. **A compact, visible environment:** time of day, day and night, sun, moon and stars, clear,
   overcast and rain with wet surfaces, and fog.
5. **Light atmospheric audio:** footsteps on six broad surfaces, an interior tone, exterior day and
   night, rain and wind.
6. **A small application shell:** start, pause, one settings screen, credits, quit, and Web and
   touch controls.
7. **Performance that is good enough and stable** on documented representative targets.
8. **Linux, Web and Android** running the same showcase correctly.

**The final decision rule.** Between more content, detail or polish and finishing the complete house
reliably on Linux, Web and Android within the budget, **choose finishing**. Between a new subsystem
and simpler content on existing CNA capabilities, **choose the simpler content**. Between making every
room unique and intelligent reuse with strong hero areas, **choose reuse**. Between another pass on a
good ground-floor room and bringing any other area up to its baseline, **choose the other area**.

---

## Non-goals — not required for DONE

**These are not postponed prerequisites for DONE.** Nothing below may appear on the critical path,
block a gate, or be written into a task's acceptance (rule R9). An idea from this list that is still
attractive goes to the [optional backlog](#optional-and-conditional-backlog), which never counts
towards the estimate (rule R13). **None of the first seven rows may ever return** (ADR-0014).

| Not required for DONE | What the plan does instead |
|---|---|
| Animals and pets, animal AI, animal animation | nothing (ADR-0014) |
| A player avatar, NPCs, characters, character animation, foot IK | first person only |
| Life or household simulation: needs, family, economy, procedural life | nothing |
| A gameplay interaction framework; object use; item pickup, carry, inventory; quests | static dressing |
| Openable containers, cupboards and drawers; door, window and gate interaction | closed static joinery; static poses (M1) |
| Appliance, kitchen, plumbing, toilet and television simulation | static fixtures |
| Physics gameplay; speculative "future-proof" systems | the kinematic controller only (ADR-0007) |
| Household persistence; Reset House; session resume | settings only; session resume is optional |
| C5 quality outside the five hero areas; C4 outside the twelve main cells; C6 anywhere | quality tiers (C3 everywhere) |
| Unique bespoke furniture per room; uniqueness or density quotas; large clutter catalogues | a small reusable kit, reused freely (rule R8) |
| Falling snow; snow cover; hail; storm, lightning and thunder; lens flare and glare; puddles and splashes; vegetation sway; seasonal vegetation; many tuned sky states | clear, overcast and rain with wet surfaces and fog, day and night (M7) |
| Exhaustive audio: room-aware routing, per-room tone, positional hums, special zone loops, clocks, creaks, seasonal ambience, offline-derived sample sets | a few loops and six footstep categories from NOX (M8) |
| New debug overlays or console work; runtime tweak UI; toasts; vignettes; key remapping; several settings pages; progressive Web pack streaming | the existing debug tools; one settings screen (M9); one Web preload |
| Exhaustive browser, device or GPU matrices | Chrome and Firefox; one representative Android device or emulator |
| Exhaustive render-test, unit-suite or performance matrices; 30-day runs; recurring multi-hour soaks; headroom margins | representative sets, a 20-minute stability run, one 2-hour run, targets without margin |
| Perfect visual polish in every corner | one bounded final defect pass (M11) |
| Tier E compiled effects | optional; Tier S is complete on its own (ADR-0003) |
| Redesigning or extending CNA | CNA defects are recorded (`cna-house.md` §6) and worked around here only where reasonable (rule R17) |

---

## Definition of DONE

House Simulator is **done** when every line below is true. Nothing else is required, and nothing in
the [optional backlog](#optional-and-conditional-backlog) is required.

| # | Area | Criterion | Proved by |
|---|---|---|---|
| **D1** | Architecture | The intended property exists: the basement, the ground floor, the garage, both upper floors, the attic, the stairs, the front and rear exterior with the garden, and the street. Every zone is at C2 or better with no S1/S2 architecture defect open | G2 `HOUSE-03280`, G3 `HOUSE-03420` |
| **D2** | Traversal | Every intended-accessible area can be reliably visited. The grand tour walks the real controller from the street through the property and the house, basement to attic, through the exterior areas and back, with no inaccessible room, blocked door, broken stair, trap or clipping through critical geometry. The random walk and the inside-geometry guarantees pass. No S1/S2 traversal or collision defect is open | `HOUSE-03226`, `HOUSE-03227`, `HOUSE-03240`, `HOUSE-03632`, `HOUSE-03631` |
| **D3** | Furnishing | Every accessible room has enough static content to communicate its purpose (C3). Secondary and utility rooms are complete without hero polish. No room looks like an unfinished placeholder | checkpoint `HOUSE-03380`, G3 `HOUSE-03420` |
| **D4** | Visual quality | Every accessible room is at C3, the twelve main cells at C4, and the five hero areas at C5. No higher level is required anywhere | G3, G4 `HOUSE-03452`, G5 `HOUSE-03480` |
| **D5** | Environment | Time of day runs automatically and can be set; day and night show the sun, moon and stars; the sky shows clear, overcast and rain; rain stays out of covered areas; surfaces are wet in rain; fog follows the weather. A snow weather state shows its overcast sky and fog without particles (falling snow is optional). Detailed weather simulation is not required | `HOUSE-03520`, `HOUSE-02521` |
| **D6** | Lighting | Interior lights follow the automatic schedule; furniture is consistent with the baked lighting; every accessible room is readable by day and at night; exterior night lighting is readable | `HOUSE-03401`, `HOUSE-03402`, `HOUSE-03633`, G3 |
| **D7** | Audio | Footsteps on six broad surface categories; one interior tone; exterior day and night beds; rain and wind layers, quieter indoors by sky exposure; volume settings | `HOUSE-01939`, `HOUSE-02516` |
| **D8** | Application | *Start*, pause, one settings screen (graphics, audio, controls, environment), credits and quit; a controls hint; Web controls; Android touch controls | `HOUSE-02523`, `HOUSE-02528`, `HOUSE-03721`, `HOUSE-03723`, `HOUSE-03039` |
| **D9** | Performance | The representative scenarios meet the documented target on the reference desktop; the Web and Android presets meet their targets in their representative scenes; no major memory or performance defect remains | `HOUSE-02404`, `HOUSE-02898`, `HOUSE-03037`, `HOUSE-03071` |
| **D10a** | Linux | A packaged Release build runs on a clean profile, with and without an audio device | `HOUSE-02790` |
| **D10b** | Web | It builds and loads; its controls work; the representative traversal completes in Chrome and Firefox without major rendering corruption and with acceptable performance; a headless smoke test runs in CI | `HOUSE-02900`, `HOUSE-02901` |
| **D10c** | Android | It builds, installs and runs on one representative device (or the best available emulator, recorded as such); touch controls suffice for the walk; the representative traversal completes without major corruption and with acceptable performance. **Only a working device path satisfies D10c.** If CNA's Android graphics path is still blocked upstream (BL-13) when everything else is done, the blocker is re-verified with evidence, every desktop-side Android task passes, and the device tasks stay open until CNA lands the fix: House Simulator is then **not DONE**, it waits | `HOUSE-03038`. `HOUSE-02951` may establish an upstream blocker, but **a blocker does not satisfy D10c**, and House Simulator does not reach DONE until the Android device path succeeds |
| **D11** | Testing | The existing gates and suites pass; the grand tour passes; the representative render sets pass; the XNA-only and strict-XNA gates pass; ASAN and UBSAN are clean once; the 20-minute stability run and the one final 2-hour run pass; the Web smoke test and the Android checklist pass | `HOUSE-02783`, `HOUSE-02784`, `HOUSE-03076` |
| **D12** | Documentation | `plan.md`, `cna-house.md`, `README.md` and `docs/*` describe the shipped scope; every asset is manifested and licensed | `HOUSE-02788`, `HOUSE-03075` |
| **D13** | Defects | No known S1 or S2 defect; every open issue is S3/S4 | `HOUSE-03632`, `HOUSE-03633`, `HOUSE-03631`, `HOUSE-03078` |
| **D14** | Bounded polish | M11's one final review and its bounded fix pass are complete, within the budget of rule R12 | `HOUSE-02714`, `HOUSE-03631` |

Tier E (compiled effects) is optional. DONE requires Tier S to be complete on every platform.

**After DONE: maintenance mode.** When `HOUSE-03078` is ticked, House Simulator feature development
stops (rule R17). Later work is bug fixes, CNA compatibility fixes and narrowly justified changes
motivated by actual use. It is not another feature roadmap, and the optional backlog is not one.

---

## Quality tiers and room classification

Each accessible cell has a **tier**, and the tier sets its target level. `docs/zones.json` records
every cell's tier and hero-area id; `tools/world/zone_scoreboard.py --check` enforces the counts
below. Not every storage room, WC, closet, plant room, loft corner, landing or side yard deserves the
living room's budget.

| Tier | Target | Furnishing depth (rule R8) | Review coverage (rule R6) |
|---|---|---|---|
| **H** Hero | **C5** | the complete recipe, secondary dressing, signs of habitation; at most one bespoke piece, inside its C5 task's estimate | fixed views by day, at night and overcast (where it has windows) |
| **M** Main | **C4** | the complete main-tier recipe | fixed views by day and at night |
| **S** Secondary | **C3** | enough furniture and props to be believable | one view in the zone set |
| **U** Utility | **C3** | essential pieces only; a mechanical room gets its equipment, pipes, a cabinet, a light and perhaps shelves | the zone walk at G3, by day |

**The five hero areas.** They cover the arrival, the principal rooms, one upper floor and the
basement. Hero ids are permanent: `H4` (master bedroom) and `H7` (attic room) were retired by
`HOUSE-03206` and their cells became main cells; the ids are not reused.

| Id | Hero area | Cells | C5 task |
|---|---|---|---|
| H1 | Front approach and porch | `EXT_WALK`, `EXT_FRONTYARD_W`, `EXT_FRONTYARD_E`, `EXT_DRIVEWAY`, the front elevation, `L0_PORCH` | `HOUSE-03450`, `HOUSE-00986` |
| H2 | Entry and living room | `L0_FOYER`, `L0_HALL`, `L0_LIVING` | `HOUSE-00986`, `HOUSE-00988` |
| H3 | Kitchen | `L0_KITCHEN` | `HOUSE-00990` |
| H5 | Library (second upper floor) | `L2_LIBRARY` | `HOUSE-03454` |
| H6 | Basement cinema | `B1_CINEMA` | `HOUSE-03455` |

**Every accessible cell by tier** (11 hero cells, 12 main, the rest secondary or utility):

| Zone | H | M | S | U |
|---|---|---|---|---|
| `Z-B1` | `B1_CINEMA` | `B1_WORKSHOP` | `B1_HALL`, `B1_WC7`, `B1_GYM`, `B1_HOBBY`, `B1_CELLAR` | `B1_MECHANICAL`, `B1_ELECTRICAL`, `B1_UTILITY`, `B1_STOR1`, `B1_STOR2`, `B1_LAUNDRY2`, `B1_UNDERSTAIR` |
| `Z-L0M` | `L0_FOYER`, `L0_HALL`, `L0_LIVING`, `L0_KITCHEN`, `L0_PORCH` | `L0_FAMILY`, `L0_DINING`, `L0_SUNROOM` | `L0_BUTLERS` | — |
| `Z-L0S` | — | — | `L0_OFFICE`, `L0_MUDROOM`, `L0_WC1`, `L0_WC2`, `L0_PANTRY` | `L0_LAUNDRY`, `L0_CLOSET_W`, `L0_STOR` |
| `Z-GAR` | — | `L0_GARAGE` | — | `L0_GARAGE_LOFT` |
| `Z-L1` | — | `L1_MASTER_BED` | `L1_LANDING`, `L1_HALL`, `L1_HALL_W`, `L1_MASTER_BATH`, `L1_MASTER_CLOSET`, `L1_BED2`, `L1_BED3`, `L1_BED4`, `L1_BED5`, `L1_BATH2`, `L1_BATH3`, `L1_WC3`, `L1_WC4`, `L1_BALCONY_REAR`, `L1_BALCONY_FRONT` | `L1_LINEN`, `L1_STOR`, `L1_CLOSET_2`, `L1_CLOSET_3` |
| `Z-L2` | `L2_LIBRARY` | — | `L2_LANDING`, `L2_HALL`, `L2_HALL_W`, `L2_GAMES`, `L2_SITTING`, `L2_BED6`, `L2_BED7`, `L2_BATH4`, `L2_BATH5`, `L2_WC5`, `L2_WC6` | `L2_CLOSET_4`, `L2_LINEN2`, `L2_STOR2` |
| `Z-L3` | — | `L3_ROOM` | `L3_STORE_W` | `L3_STORE_E`, `L3_STORE_N`, `L3_STORE_S` |
| `Z-STAIR` | — | `L0_STAIR_MAIN`, `L1_STAIR_MAIN`, `L2_STAIR_MAIN` | `B1_STAIR`, `L2_STAIR_ATTIC`, `L3_STAIR_HEAD` | — |
| `Z-EXF` | `EXT_WALK`, `EXT_FRONTYARD_W`, `EXT_FRONTYARD_E`, `EXT_DRIVEWAY` | — | `EXT_ROAD` (the accessible part) | — |
| `Z-EXR` | — | `EXT_TERRACE`, `EXT_BACKYARD` | `EXT_GARDEN`, `EXT_ORCHARD`, `EXT_SHED` | `EXT_SIDEYARD_W`, `EXT_SIDEYARD_E` |
| `Z-STR` | — | — | the street and the neighbourhood, as scenery | — |

The elevations follow their zone's C2 architecture (`HOUSE-03267` for the rear and sides); they are
not tiered separately.

---

## Scheduling rules — time discipline

These rules exist because the project once spent about 200 agent-hours polishing one ground-floor
route while four levels stayed empty. They **override task-number order**, and they override any
"largest visible defect" heuristic applied to a single area. `docs/workflow.md`'s "pick the next
unfinished task whose dependencies are complete" means: pick it **by these rules**.

**Tracks.** *Track A* is the breadth path M1–M6 (traversal, architecture, the kit, dressing,
baseline lighting, then main rooms and hero areas). *Track B* interleaves with it: M7 environment,
M8 audio, M9 application shell, M10 performance, M12 tests, M14's Web bring-up block and M15's
Android readiness block. M11 follows G5; M13 follows M11; the Web and Android verification blocks
follow M13; M16 is last.

| Rule | Statement |
|---|---|
| **R1: breadth floor** | Up to C3, a task may raise a zone to level *k* only if **every** zone is already at level ≥ *k − 1*. C4 work (M6a) starts only after **G3**; C5 work (M6b) starts only after **G4**. No cell is raised above its tier's target. Exceptions, and only these: (a) S1 defects, in any zone at any time; (b) house-wide system work (renderer, pipeline, kit, validator, tooling) whose acceptance is verified on ≥ 3 zones including the least complete; (c) a regression caused by the current task |
| **R2: least complete first** | Among eligible Track A tasks, pick the zone with the lowest C3 floor; in M6a the zone with the fewest main cells at C4; in M6b the hero area in the least-served level. Break ties by the most accessible cells still below target, then by the zone least recently worked. `Z-L0M` never wins a tie |
| **R3: consecutive cap** | At most **3 consecutive** Track A tasks on one zone, unless that zone is still *strictly* the least complete. After the cap, the next Track A task targets another zone |
| **R4: the ground floor goes last** | Until **G3**, no task targets `Z-L0M` (foyer, hall, living, family, kitchen, dining, sunroom, butler's pantry, porch) except S1 fixes and `HOUSE-03407`, which runs last in M5. Its C4 task (`HOUSE-00989`) runs after every other zone's C4 task, and its hero tasks (`HOUSE-00986`, `00988`, `00990`) after every other hero task; the dependencies encode both. Its S2/S3/S4 findings go to the backlog in `docs/visual-review/README.md`, not into the task queue. **A defect in a finished ground-floor room never outranks an unfinished area elsewhere** |
| **R5: track share** | Until G5, at least **two of every three** completed tasks are Track A. Track B fills the rest. If every eligible Track A task is blocked, record the blockers in the task entries; Track B may then proceed |
| **R6: bounded review** | A review round captures only the views of the areas it concerns, at their tier's coverage. It schedules fixes only for **S1/S2** in areas at or below their current target, and S3 only as the severity table allows. At most **2 consecutive** review → fix cycles target one area. **No non-hero area gets more than two dedicated polish passes unless an S1/S2 remains.** A round that finds nothing above S3 in an area at its target **stops** |
| **R7: every task pays its way** | Every task states `adv:` (the DONE item or gate it advances) and `est:` (agent-hours). A task estimated above 4 h is split into ≤ 4 h slices when it is scheduled. A task that reaches **2× its estimate** stops, records what it learned in a `note:`, and either splits the rest into a new task with a new estimate or, if the rest is not needed for DONE, moves it to the optional backlog (rule R15) |
| **R8: kit first, reuse freely** | Furniture and fixtures come from the shared kit: the generators (`HOUSE-00985`, `HOUSE-00973`, `HOUSE-02681`), the M3 acquisition groups and the existing ground-floor pieces. **The same model may appear any number of times**, varied by tint, scale, dimensions, arrangement and small accessory swaps; there is no uniqueness or density quota. **The acquired catalogue is capped at 32 models plus one wall-art set of at most 8 images** (M3); a model beyond a group's cap needs a Planning corrections entry and an equal cut elsewhere. Depth follows the tier: utility essential only, secondary believable, main complete, hero full. Bespoke per-object authoring is allowed **only in a hero area**, at most one piece, inside its C5 task's estimate. Once a room reads as its purpose during normal exploration, stop adding detail unless it is a main or hero cell |
| **R9: no new systems** | **No new system unless it is strictly necessary to satisfy an already-authorised DONE item.** A new runtime subsystem, interaction, simulation or framework requires a [Planning corrections](#planning-corrections) entry showing which DONE item cannot be met with existing code, existing CNA capabilities or simpler content. "It would look nicer" is not a reason, and nothing on the [Non-goals](#non-goals--not-required-for-done) list qualifies. Offline tooling that speeds up breadth work is allowed under R7 |
| **R10: reassess** | At every gate, and after every 10 completed tasks: update the [scoreboard](#zone-scoreboard) with evidence (a review round and `zone_scoreboard.py` output), compare the hours spent with each milestone's budget and with R14's ceiling, and record in `docs/handoff.md` which rule chose the next task. A milestone that overruns its budget by 50 % needs a Planning corrections entry that **cuts or simplifies** the rest; continuing unchanged is allowed only for S1/S2 fixes |
| **R11: unfinished beats finished** | When the choice is between completing an unfinished area and improving a finished one, complete the unfinished area. Do not invent scope |
| **R12: bounded polish** | Visual polish outside the hero C5 tasks happens only in M11's final defect pass: **4.5 agent-hours of S2/S3 work** plus whatever S1 fixes need (about 7 h for the milestone with its review and golden refresh; never more than 12 h unless S1/S2 remain). No screenshot → tweak → screenshot loop runs more than two rounds on one area. When the pass is spent and no S1/S2 remains, polish is **done** |
| **R13: optional stays optional** | A task in the [optional backlog](#optional-and-conditional-backlog) is never scheduled while a MUST task is open, never counts towards the estimate, never blocks a gate or DONE, and is never implemented because it "looks cheap". It never moves into the active plan silently: promotion needs a Planning corrections entry naming the DONE item that cannot be met without it, and must fit under R14's ceiling |
| **R14: budget ceiling** | The realistic remaining estimate at `HOUSE-03206` is **226 agent-hours**; the **hard ceiling is 280 h**, measured as *hours spent since 2026-09-22 plus the remaining pessimistic estimate*. The ceiling is not a target. When the projection would exceed it, the next task is a Planning corrections entry that **cuts depth or moves work to the optional backlog** until it fits. The ceiling is never raised, and an overrun is never resolved by re-estimating without cutting. Breadth (every floor, the basement, the attic, the garage, the exterior, Linux, Web, Android) is not cut to meet it |
| **R15: final scope, targeted corrections only** | `HOUSE-03206` is the **final proactive scope reduction**. No further broad replanning, re-scoping or re-estimation pass is scheduled. A later reduction is allowed only when a task proves unexpectedly expensive (R7), a platform constraint makes a feature disproportionately costly, or R14 is threatened; each is one targeted Planning corrections entry. **Expansion is never automatic**: adding MUST work needs a Planning corrections entry naming a DONE item that cannot be met otherwise, and an equal cut elsewhere |
| **R16: validation lives in the task** | Every task proves its own acceptance through its `verify:` step. There are no separate "validate X", "re-validate X", "cross-check X" or "review X" tasks for one deliverable. The only standalone review tasks are the gates G1–G5, the dressing checkpoint, M11's final walkthrough, M13's DONE audit and the Web and Android DONE checklists |
| **R17: maintenance mode; CNA stays CNA** | After `HOUSE-03078`, feature development stops. A CNA defect found here is proved, recorded in `cna-house.md` §6 with enough evidence to fix it in CNA, and worked around here only where a local workaround is reasonable; CNA changes happen in CNA under their own task. A missing CNA feature is never a reason to grow this plan |

**Gates.** A gate passes when its review task is ticked with the scoreboard as evidence.

| Gate | Passes when | Review task |
|---|---|---|
| **G1** Traversable | every zone ≥ C1 (no `*`) | `HOUSE-03240` |
| **G2** Architecture | every zone ≥ C2 | `HOUSE-03280` |
| *(checkpoint)* Dressed | every accessible room has its tier's essential props; the house-wide lighting work waits for it | `HOUSE-03380` |
| **G3** Baseline complete | every accessible room ≥ C3 (dressed and lit) | `HOUSE-03420` |
| **G4** Presentation | every main cell ≥ C4 | `HOUSE-03452` |
| **G5** Showcase | every hero area at C5 | `HOUSE-03480` |
| **DONE** | D1–D14 | `HOUSE-03078` |

Notes written before 2026-09-21's second reduction (older handoff sections, the review ledger's
2026-09-21 header) use an older gate numbering: read their "G3" as the dressing checkpoint and their
"G4" as today's G3.

---

## Completion levels and zones

### Completion levels

A cell's level is the highest *k* for which it meets C1 … C*k*, all of them. A zone's **C3 floor**
is the lowest level among its accessible cells, capped at C3.

| Level | Name | A cell at this level … |
|---|---|---|
| C0 | Absent | is missing, unreachable or not rendered |
| **C1** | Traversable | is reached by the grand tour on foot; posed doors and gates block exactly where they are drawn; no stuck point, fall-through, or clipping through important geometry; collision matches what is drawn |
| **C2** | Architecture | shows finished primary architecture at eye height: production materials on floors, walls, ceilings and roofs; doors, windows, trim and hardware; stairs with handrails, balusters and nosings; zone-specific structure where the room type needs it (attic rafters, insulation and walkway; unfinished-basement structure; garage slab and walls; garden structures); no blockout surface; no S1/S2 architectural defect. Utility rooms get plain, correct construction, not decoration |
| **C3** | Baseline complete | **the floor for every accessible cell.** Has its tier's essential furniture and fixtures from its recipe (`HOUSE-03302`), so its purpose reads at a glance; believable placement (validator green); production materials and no placeholder or debug surface; is readable at 10:30 clear and at night under its scheduled lights, is never black, and its props are not visibly darker or brighter than the surfaces they stand on (`HOUSE-03402`); exterior spaces have their primary objects and night lighting; no S1/S2 in its review view |
| **C4** | Presentation-ready | **main cells only.** C3, plus the complete main-tier recipe (textiles, wall items, shelf contents and curtains where the recipe has them); lighting balanced by day and at night; its fixed views, day and night, have no S1/S2 |
| **C5** | Showcase | **hero areas only.** C4, plus secondary dressing and signs of habitation (at most one bespoke piece); views by day, at night and overcast with no S1/S2; S3 closed where cheap; the representative view within the High budget |

There is no C6.

### Zones

Zone membership is data: `docs/zones.json`.

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

**As of 2026-09-25 (Round 168; G5 passed).** Levels from Round 103's whole-property day/night review
(`HOUSE-03203`), Round 104's traversal sweep and subsequent completed zone work; main and hero
targets re-counted after `HOUSE-03206`'s tier change. Update per rule R10.

The former `*` marked the **house-wide door/gate defect** from Round 103. `HOUSE-03221`–`03224`
made every leaf's static render, collision and portal aperture agree; `HOUSE-03226`/`03227` then
proved all 90 intended-accessible cells and the tight-space traversal. G1 clears the marker.

| Zone | C3 floor | Main at C4 | Hero cells at C5 | Evidence | Largest gaps |
|---|---|---|---|---|---|
| `Z-B1` | **C3** | 1 / 1 | 1 / 1 | Round 163; the complete cinema is readable under its scheduled lights with no S1/S2 (`HOUSE-03455`) | No zone-specific S2 |
| `Z-L0M` | **C3** | 3 / 3 | 5 / 5 | Round 167; the complete kitchen closes H3 by day, scheduled night and overcast (`HOUSE-00990`) | No zone-specific S2 |
| `Z-L0S` | **C3** | — | — | Round 149; all eight service cells have physical fixtures and readable day/scheduled-night lighting (`HOUSE-03405`) | No zone-specific S2 |
| `Z-GAR` | **C3** | 1 / 1 | — | Round 159; the complete vehicle/workshop/storage composition is balanced in both fixed views by day and scheduled night (`HOUSE-03442`) | No zone-specific S2 |
| `Z-L1` | **C3** | 1 / 1 | — | Round 158; the complete master-bedroom recipe, including its wall treatment, is balanced by day and scheduled night (`HOUSE-03444`) | No zone-specific S2 |
| `Z-L2` | **C3** | — | 1 / 1 | Round 162; the complete library composition is readable by day, scheduled night and overcast, with no S1/S2 (`HOUSE-03454`) | No zone-specific S2 |
| `Z-L3` | **C3** | 1 / 1 | — | Round 157; the lived-in attic-room composition reads under dormer daylight and scheduled evening lights (`HOUSE-03446`) | No zone-specific S2 |
| `Z-STAIR` | **C3** | 3 / 3 | — | Round 154; the three main-stair cells retain their complete main-tier composition and are balanced in every fixed foot/head view by day and scheduled night (`HOUSE-03447`) | No zone-specific S2; main-stair-foot darkness closed |
| `Z-EXF` | **C3** (C5 hero content in place) | — | 4 / 4 | Round 164; day/night/overcast review closes the finished front approach and elevation with no S1/S2 (`HOUSE-03450`) | No zone-specific S2 |
| `Z-EXR` | **C3** | 2 / 2 | — | Round 155; terrace/backyard compositions and fence planting are presentation-ready under clear day and the bounded scheduled-night hierarchy (`HOUSE-03448`) | No zone-specific S2 |
| `Z-STR` | **C2** (C3 scenery dressing done) | — | — | Round 139; N1–N60, street furniture, barriers and the three parked estates plus delivery van (`HOUSE-00841`–`00849`, `00856`, `00857`) | S3: plain road foreground and repeated vegetation band; logged for M11 |

Objective inputs, generated by `python3 tools/world/zone_scoreboard.py`. `Lit accessible` counts
cells with at least one authored light group; it does not claim that their baseline lighting is
visually complete. C4 and C5 are target counts from the tier classification, not completion claims.

| Zone | Cells | Accessible | Props | Props/access. | Zero-prop accessible | Lit accessible | Review poses | C4 main targets | C5 hero targets |
|---|---:|---:|---:|---:|---|---|---:|---|---|
| `Z-B1` | 14 | 14 | 83 | 5.93 | 0 | 14 / 14 | 4 | 1 | 1 |
| `Z-L0M` | 9 | 9 | 106 | 11.78 | 0 | 9 / 9 | 23 | 3 | 5 |
| `Z-L0S` | 8 | 8 | 35 | 4.38 | 0 | 8 / 8 | 2 | 0 | 0 |
| `Z-GAR` | 2 | 1 | 13 | 13.00 | 0 | 1 / 1 | 2 | 1 | 0 |
| `Z-L1` | 20 | 20 | 132 | 6.60 | 0 | 20 / 20 | 6 | 1 | 0 |
| `Z-L2` | 16 | 15 | 124 | 8.27 | 0 | 15 / 15 | 6 | 0 | 1 |
| `Z-L3` | 5 | 5 | 38 | 7.60 | 0 | 5 / 5 | 4 | 1 | 0 |
| `Z-STAIR` | 6 | 6 | 22 | 3.67 | 0 | 6 / 6 | 9 | 3 | 0 |
| `Z-EXF` | 5 | 5 | 17 | 3.40 | 0 | 5 / 5 | 4 | 0 | 4 |
| `Z-EXR` | 7 | 7 | 44 | 6.29 | 0 | 3 / 7 | 7 | 2 | 0 |
| `Z-STR` | 2 | 0 | 0 | — | 0 | 0 / 0 | 1 | 0 | 0 |

**Every zone containing accessible rooms is at C3; `Z-STR` remains a C2 scenery-only zone with no
accessible cells.** Round 153 inspected all 68 fixed views by day and scheduled night plus all 22
accessible utility cells by day. Every accessible cell has its tier's essential props, readable
lighting at its required review times and no open S1/S2. `zone_scoreboard.py` reports zero
zero-prop accessible rooms and assigns all 96 cells exactly once. G3 passes.

---

## Visual review protocol and defect severity

The review ledger stays in [`docs/visual-review/README.md`](docs/visual-review/README.md). Captures
stay local and Git-ignored. Strict golden references (`tests/render/reference/`) are a separate
thing: never refresh a golden as part of a review round unless the round's task changed that image
on purpose.

A round records: the zones and areas captured (`tools/visual/capture_review.py` sets, at the tier's
coverage of rule R6); every defect with its **severity**, **zone** and **tier**; the scoreboard
change; and **the next task together with the rule that chose it**. The fixed ground-floor route is
not the default review. Gate reviews capture every zone, each at its tier's coverage, on one
contact sheet.

| Severity | Meaning | Examples | Scheduling |
|---|---|---|---|
| **S1 Blocker** | Must fix. Breaks the walk or the image outright | crash; broken traversal or an unreachable area; stuck or falling through a floor; missing major geometry (a hole to the void); major rendering corruption; debug colours; content failing to load; a black or blown-out room at noon; a broken platform build | Fix now, in any zone, at any time (R1 exception a) |
| **S2 Major** | Fix before DONE. Breaks believability at normal eye height | a room that reads empty or as the wrong type; clearly visible bad collision; a major prop placement problem (floating, intersecting, wrong scale); obvious clipping in important areas; a large missing material or texture; large z-fighting; a severe lighting artefact; a room unreadable by day or at night | Fix within the area's current target level, subject to R1/R4 |
| **S3 Moderate** | Noticeable, but believability holds | visible tiling; imperfect colour or brightness balance; sparse secondary dressing; small seams; dull corners | Fix **only** when it is cheap (about 30 minutes or less), repeatedly visible, in a hero area, or in a release capture. Otherwise logged. Scheduled only in a hero C5 task or in M11's bounded pass |
| **S4 Minor polish** | Visible only on close inspection | a tiny prop placement issue; a subtle material mismatch; a small repetitive detail; a minor shadow oddity; low texture resolution on small props; micro-clutter | **Never blocks DONE.** Logged only; never scheduled |

**Diminishing returns.** Once an area meets its tier's target with no S1/S2 open, further findings
there are logged and **not** scheduled until M11. Two consecutive rounds on one area (R6) is the
limit.

---

## How to read a task

```
- [ ] HOUSE-09999 — Objective, in one line
      dep: HOUSE-09991, HOUSE-09992 · sys: visibility · plat: LNX · pri: MUST · zone: Z-L1 · adv: D2, G1 · est: 3
      files: src/visibility/PortalTraversal.cpp|hpp
      accept: (1) …; (2) …
      verify: unit PortalTraversalTests.ClosedDoorCulls; zone capture Z-L1
      trace: absorbs HOUSE-09993; wording canonicalised by HOUSE-03206
```

| Field | Meaning |
|---|---|
| `dep` | Tasks that must be complete first. A dependency on a cancelled or merged task is satisfied by the task that absorbed it (see [Cancelled or merged](#cancelled-or-merged-by-the-final-scope-reduction-house-03206)). `—` means none |
| `sys` | Owning subsystem, matching `src/` or `tools/` |
| `plat` | `LNX`, `WEB`, `AND`, `ALL`, `TOOL`, `CI` |
| `pri` | `MUST` (required for DONE) or `OPT` (optional backlog only, rule R13). There is no SHOULD tier |
| `zone` | The zone a Track A task raises (rules R2/R3). `all` for house-wide work |
| `adv` | The DONE item or gate the task advances (rule R7) |
| `est` | Estimated agent-hours (rule R7). The [estimate](#remaining-work-estimate) is their sum |
| `accept` | **The current, complete requirement.** Where a task has no `accept:` line, its title and its milestone's preamble are the requirement |
| `trace` | Traceability only, never a requirement: which ids it absorbed and where its earlier wording lives. **Earlier wording is superseded; do not implement it** |
| `note` | Evidence and history recorded while working |

**Canonical text.** `HOUSE-03206` rewrote every open task so that its title and `accept:` state the
currently authorised work. The earlier titles, the `amended:` chains of the first two reductions and
their larger quotas are superseded; the full earlier text is `plan.md` at commit `174f2ba`, and
earlier still the legacy ledger. When a note, a commit message or an old document describes more
than the active task does, **the active task governs**.

**Ids.** New work takes the next free id in its **milestone's** reserved range. Ids that left the
active plan are listed with their destination in
[Cancelled or merged by the final scope reduction](#cancelled-or-merged-by-the-final-scope-reduction-house-03206),
and earlier ones in [`docs/history/scope-reductions-2026-09-21.md`](docs/history/scope-reductions-2026-09-21.md).
**No id is ever reused.**

---

## Milestone index and ID ranges

| Milestone | Track | ID range (new tasks) | Open tasks | Gate / exit | Advances | Budget (realistic agent-h) |
|---|---|---|---|---|---|---|
| [M0](#m0--scope-reset-and-breadth-instruments) — Scope reset and breadth instruments | A (support) | 03201–03220 | 0 | done | R10, G1–G5 | 0 |
| [M1](#m1--whole-property-traversal-c1-everywhere--gate-g1) — Whole-property traversal | A | 03221–03260 | 0 | **G1 passed** · `HOUSE-03240` | D2 | 0 |
| [M2](#m2--architectural-completion-c2-everywhere--gate-g2) — Architectural completion | A | 03261–03300 | 0 | **G2 passed** · `HOUSE-03280` | D1 | 0 |
| [M3](#m3--the-reusable-furnishing-kit) — The reusable furnishing kit | A (support) | 03301–03340 | 0 | **kit ready for M4** | D3 | 0 |
| [M4](#m4--dressing-everywhere-the-furnishing-half-of-c3--checkpoint) — Dressing everywhere | A | 03341–03400 | 0 | **checkpoint passed** · `HOUSE-03380` | D3 | 0 |
| [M5](#m5--baseline-lighting-everywhere-the-lighting-half-of-c3--gate-g3) — Baseline lighting everywhere | A | 03401–03440 | 0 | **G3 passed** · `HOUSE-03420` | D3, D6 | 0 |
| [M6](#m6--main-cells-and-hero-areas-c4-c5--gates-g4-and-g5) — Main cells and hero areas | A | 03441–03500 | 0 | **G4** · `HOUSE-03452`, **G5** · `HOUSE-03480` | D4 | 0 |
| [M7](#m7--a-compact-environment) — A compact environment | B | 03501–03540 | 0 | `HOUSE-03520` | D5 | 0 |
| [M8](#m8--atmospheric-audio-essentials) — Atmospheric audio essentials | B | 03541–03570 | 4 | `HOUSE-01939` | D7 | 5 |
| [M9](#m9--application-shell) — Application shell | B | 03571–03600 | 0 | `HOUSE-02528` | D8 | — |
| [M10](#m10--performance-measurement-driven) — Performance, measurement-driven | B | 03601–03630 | 2 | `HOUSE-02405` | D9 | 6 |
| [M11](#m11--final-defect-pass-bounded--after-g5-only) — Final defect pass | after G5 | 03631–03680 | 3 | `HOUSE-02713` | D2, D4, D13, D14 | 3.25 |
| [M12](#m12--representative-tests) — Representative tests | B | 03681–03700 | 0 | **done** · `HOUSE-02598` | D11 | 0 |
| [M13](#m13--linux-desktop-release) — Linux desktop release | after M11 | 03701–03720 | 8 | `HOUSE-02797` | D9–D13 | 10.25 |
| [M14](#m14--web) — Web | bring-up: B · verification: after M13 | 03721–03750 | 6 | `HOUSE-02904` | D10b | 8 |
| [M15](#m15--android) — Android | readiness: B · device: after M13 | 03751–03780 | 9 | `HOUSE-03041` (a BL-13 record does not close it) | D10c | 16 |
| [M16](#m16--final-release) — Final release | last | 03781–03800 | 6 | **DONE** · `HOUSE-03078` | D9–D14 | 6.5 |

Legacy ids 03121–03200 are unallocated and stay unused. The budgets are the sums of the tasks'
`est:` values; rules R10 and R14 compare the hours spent against them.

---

## M0 — Scope reset and breadth instruments

Complete. The review instruments every zone needs exist, and the scope is final.

- [x] HOUSE-03201 — Scope reduction: rewrite the plan around the architectural showcase, archive the legacy ledger, record ADR-0014
      dep: — · sys: — · plat: ALL · pri: MUST · zone: all · adv: all · est: 6
      files: plan.md, docs/history/plan-legacy-2026-09-21.md, docs/decisions/ADR-0014-showcase-scope.md (+ status pointers in ADR-0005/0008/0010 and the ADR index), cna-house.md, README.md, CLAUDE.md, AGENTS.md, docs/workflow.md, docs/versioning.md, docs/conventions.md, docs/handoff.md, docs/visual-review/README.md
      accept: (1) every task open on 2026-09-21 is carried, deferred or cancelled exactly once, each cancellation with its reason; (2) DONE, the scheduling rules, the zones, the completion levels and the gates are defined; (3) every document that contradicted the new scope points at ADR-0014; (4) no runtime code, data or asset changed
      verify: `tools/ci/run_checks.sh`; the builder's audit (all 764 legacy open ids accounted for once); `git diff --stat` shows documentation only
      note: (2026-09-21) the legacy `plan.md` moved to `docs/history/plan-legacy-2026-09-21.md`. Git records the move as a copy, so `git blame -C` keeps every line's history.

- [x] HOUSE-03205 — Second scope reduction: quality tiers, a reusable kit, a compact environment and audio set, representative tests, a bounded polish pass
      dep: HOUSE-03201 · sys: — · plat: ALL · pri: MUST · zone: all · adv: all · est: 5
      files: plan.md, docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md (+ status pointers in ADR-0014, ADR-0010 and the ADR index), cna-house.md, README.md, CLAUDE.md, AGENTS.md, docs/workflow.md, docs/handoff.md
      accept: (1) every task open at `5927073` is kept, merged, cancelled with its reason, or moved to Optional after DONE, exactly once; (2) "C5 everywhere" is replaced by quality tiers with every cell classified; (3) the Definition of DONE, the non-goals and the scheduling rules state the reduced scope; (4) every open task carries `est:`; (5) no runtime code, data or asset changed
      verify: the builder's audit; `tools/ci/run_checks.sh`; `git diff --stat` shows documentation only

- [x] HOUSE-03206 — Final scope reduction: canonical active tasks, five hero areas, a 280-hour ceiling, maintenance mode after DONE
      dep: HOUSE-03205 · sys: — · plat: ALL · pri: MUST · zone: all · adv: all · est: 4
      files: plan.md, docs/history/scope-reductions-2026-09-21.md, docs/decisions/ADR-0016-final-scope-reduction.md (+ status pointers in ADR-0015 and the ADR index), docs/zones.json, tools/world/zone_scoreboard.py, tools/visual/capture_review.py, cna-house.md, README.md, CLAUDE.md, AGENTS.md, docs/workflow.md, docs/handoff.md
      accept: (1) every task open at `174f2ba` is kept, merged into a named task, cancelled with its reason, or moved to the optional backlog, exactly once; (2) every active task's title and `accept:` state the current work, with no `amended:` chain; (3) hero areas 7 → 5 and main cells 19 → 12, recorded in `docs/zones.json` and enforced by the two tools; (4) the realistic estimate lies in 210–250 h and the pessimistic one under the 280 h ceiling; (5) the plan states that this is the final proactive reduction; (6) no runtime code, content or asset changed
      verify: the builder's audit (205 open MUST tasks at `174f2ba` accounted for once; no active task depends on an optional one); `zone_scoreboard.py --check`; `capture_review.py --check`; `tools/ci/run_checks.sh`
      note: (2026-09-22) 205 open tasks and 318.75 realistic hours before; 152 tasks and 226.25 h after (51 merged, 1 cancelled, 2 moved to the optional backlog, 1 added). The history sections of the first two reductions moved verbatim to `docs/history/scope-reductions-2026-09-21.md`

- [x] HOUSE-03202 — Give every zone a fixed review view set and a whole-property contact sheet
      dep: HOUSE-03201 · sys: tools · plat: TOOL · pri: MUST · zone: all · adv: G1–G5 · est: 2.5
      files: tools/visual/capture_review.py, docs/visual-review/README.md
      accept: every hero area and every main cell has one fixed pose (two for the front approach); each zone has at least one pose in a secondary cell; utility cells are judged from the zone walk; `--zone` and `--all-zones` capture in `clear-day` and `clear-night`, and `clear-overcast` for hero areas; one labelled contact sheet per zone; the 32 legacy poses are kept and assigned to zones
      verify: run `--all-zones` in both scenarios; inspect every contact sheet

- [x] HOUSE-03203 — Baseline review of the whole property: level every zone with evidence and seed the defect lists
      dep: HOUSE-03202, HOUSE-03204 · sys: — · plat: LNX · pri: MUST · zone: all · adv: R10 · est: 2.5
      accept: (1) one ledger round covers all 11 zones, day and night; (2) every observed defect is classified S1–S4 and filed under its zone; (3) the scoreboard levels are confirmed or corrected with the round and `zone_scoreboard.py` evidence; (4) each M2 zone task gets its concrete S1/S2 list as a `note:`; (5) the room tiers are confirmed or corrected
      verify: the ledger round; the updated scoreboard
      note: (2026-09-21) Round 103 confirms every existing zone level and every H/M/S/U classification without change; S1 none; the house-wide fixed-leaf/pass-through mismatch is S2 and belongs to M1

- [x] HOUSE-03204 — `tools/world/zone_scoreboard.py` and `docs/zones.json`: objective per-zone completeness numbers
      dep: HOUSE-03201 · sys: tools · plat: TOOL · pri: MUST · zone: all · adv: R10 · est: 2
      files: docs/zones.json, tools/world/zone_scoreboard.py, tools/ci/run_checks.sh
      accept: (1) `docs/zones.json` puts each of the 96 cells in exactly one zone, or in an explicit `none` group with a reason, and records each cell's tier and hero-area id; (2) the tool prints per zone: cells, accessible cells, props, props per accessible cell, cells with zero props, cells with ≥ 1 light group, review poses, main targets and hero targets; (3) `--check` fails on a cell in zero or two zones and runs in `run_checks.sh`
      verify: run it; paste its table into the scoreboard

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
      accept: (1) every leafed portal (63 doors, 3 gates, the garage door) has an authored static open fraction; (2) interior doors default to resting open (≥ 0.85) on their swing side, and a door stays closed only where no intended-accessible space lies behind it, each such case listed; (3) every door on an accessible route leaves ≥ 0.70 m clear width; (4) the validator rejects a missing pose, a closed door on an accessible route, and a swing arc that intersects walls or placed props; (5) the schema home is recorded in `docs/world-format.md`; no behaviour code is added
      verify: `validate_world.py` (new rule); `--selftest` cases for each rejection
      note: (2026-09-21) all 63 walkthrough doors, the garage door and three gates now have static poses; only the facade-only Juliet door and rear non-traversal gate are closed and record why. Rule 14 proves route clearance and wall/prop arc clearance. Its measured arc moved the hall-family swing into the family room and the low dog bed 0.98 m north, the smallest correction that clears both sides' existing dressing

- [x] HOUSE-03222 — Draw every leaf at its static pose
      dep: HOUSE-03221 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D2, G1 · est: 3
      files: tools/blender/house_shell_gen.py, tools/world/build_chunks.py, tools/world/verify_shell.py
      accept: (1) the shell builds each hinged leaf rotated about its hinge, and each sliding leaf translated, to the authored pose; (2) one leaf owned by the cell it swings into, plus whatever the other cell needs so culling never shows a hole; (3) `verify_shell.py` asserts pose, hinge side and no wall intersection; (4) the finished joinery of `HOUSE-00932`, `00940` and `00942` keeps its detail at the new pose; (5) chunk budgets hold, or their exceptions are re-measured and recorded
      verify: `verify_shell.py`; zone captures show doors resting open on every floor
      note: (2026-09-22) the shell rigidly poses each hinged body and its joinery around the authored hinge, translates slider sashes, and places the sectional garage leaf overhead; gates and the shed leaf use the same authored fractions in `fence_gen.py`. Each of the 63 wall doors now has one swing-cell owner while both cells retain casing/reveal/threshold detail. Independent verification finds 63/63 at the expected pose, none duplicated and none intersecting the neighbouring wall. The rebuilt world stays at 740 chunks with every existing ceiling holding and no new exception. The 56-view `house-03222-static-leaves-final` clear-day capture covers all eleven zones without a leaf-caused S1/S2 or clear-colour hole

- [x] HOUSE-03223 — Make every posed leaf solid
      dep: HOUSE-03222 · sys: physics · plat: ALL · pri: MUST · zone: all · adv: D2, G1 · est: 2
      files: tools/world/build_collision.py (static) or src/physics/DynamicObstacles.cpp (filled once at load), tests/unit/PosedLeafCollisionTests.cpp
      accept: (1) every posed door leaf, gate leaf and the garage door has a collision proxy at its pose; (2) a closed leaf blocks the capsule; (3) `PortalClearanceTests`, `InsideGeometryTests` and the stair traversal tests pass with the leaves present
      verify: unit PosedLeafCollisionTests.*, PortalClearanceTests.*, InsideGeometryTests.*
      note: (2026-09-22) `collision.bin` now carries 69 yaw-only door OBBs, one pitched overhead-garage mesh and three gate OBBs at the authored fixed poses, each traceable through `door_leaf:<opening>:<leaf>` / `gate_leaf:<gate>:<leaf>`. Reversing the L2 attic-stair leaf's hinge was the smallest valid authored correction for the flight it blocked. All 43 representative collision/traversal tests pass, including the 20-minute random walk and the whole-lot walk; the full 1418-test unit suite passes. No runtime door behaviour or interaction framework was added

- [x] HOUSE-03224 — Drive portal apertures from the static poses
      dep: HOUSE-03221 · sys: visibility · plat: ALL · pri: MUST · zone: all · adv: D2, G1 · est: 2
      files: src/visibility/PortalRuntime.cpp|hpp, src/visibility/VisibilitySystem.cpp, tests
      accept: (1) at load each leafed portal's aperture equals its authored pose (open or closed per `HOUSE-00665`'s hysteresis), so culling matches what is drawn; (2) the door-state matrix test (`HOUSE-00687`) is re-expressed over the posed states and proves, from both sides, that every leafed portal's visibility and collision agree with its pose (absorbs `HOUSE-02583`); (3) the 18 culled/unculled render pairs stay within their bound
      verify: integration door-state matrix; render culling pairs
      note: (2026-09-22) `WorldLoader` carries each authored `openFraction` into `VisibilitySystem`; the production matrix proves all 63+ leafed portals start at that pose and retains the shut/open both-side checks. Opaque doors now retain the depth-6 sightline from either side while exterior glazing remains capped at 1 and the garage at 2. All 18 culled/unculled pairs pass (worst 0.0531% at `l1-landing`, bound 0.2%)

- [x] HOUSE-03225 — Accessibility manifest: the intended-accessible cells and a validated standing point in each
      dep: HOUSE-03204 · sys: world · plat: TOOL · pri: MUST · zone: all · adv: D2, G1 · est: 2
      files: docs/zones.json or assets-src/world (recorded in docs/world-format.md), tools/world/validate_world.py
      accept: (1) data lists every intended-accessible cell with a standing point: every walk-accessible interior cell except the two appliance interiors and garage loft, the exterior cells inside the fences, the porch and balconies, and the accessible part of the road; (2) excluded cells (`EXT_WORLD`, `EXT_NORTHSTRIP`, `L2_BALCONY_JULIET`, `L0_GARAGE_LOFT`, `CELL_FRIDGE_INTERIOR`, `CELL_FREEZER_INTERIOR`) are listed with reasons; (3) the validator proves each standing point is on walkable collision inside its cell with headroom for the standing or crouched capsule (`HOUSE-00558`)
      verify: `validate_world.py`
      note: (2026-09-23) `docs/zones.json` now records collision-proven feet positions for all 90 intended-accessible cells; the under-stair store, main-stair lower pocket and attic stair head explicitly use the crouched capsule. Rule 15 rebuilds the real static collision (including props and terrain), requires the exact six documented exclusions, and proves footprint containment, walkable support and posture headroom. `HOUSE-03226`'s controller proof established that the ladder-only garage loft is visible architecture outside the walk-only accessibility contract (PC-2026-09-23). The full authored world and focused missing/outside/low-headroom selftests pass

- [x] HOUSE-03228 — Repair the two integration tests left failing by the fixture-prop and posed-leaf changes
      dep: HOUSE-03223 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D11, G1 · est: 1
      files: tests/integration/WorldLoadTests.cpp, tests/integration/HeadlessRunTests.cpp (as found)
      accept: (1) `WorldLoadTests.ValidatingTheHouseCostsLessThanReadingIt` validates a world with its props loaded, so it no longer rejects every light `fixtureProp` (broken since `HOUSE-01259`), and still measures what its name says; (2) `HeadlessRunTests.PressingF3ShowsTheWalkTheFrameActuallyDid` meets a draw-call bound re-measured against the posed leaves of `HOUSE-03222`/`03224` (115 calls from the road against `< 110`), with the measured number recorded beside the bound, or the extra draws are removed; no bound is loosened without its measurement; (3) the full integration suite passes
      verify: integration suite
      note: (2026-09-22) recorded in `docs/handoff.md` by `HOUSE-03223`; neither failure is caused by collision
      note: (2026-09-23) the validation benchmark now loads `layout.props.json`, so Fast and Full validation both pass while measuring 2.09 ms and 2.50 ms median respectively. The posed-leaf frame is a repeatable 115 / 620 draws across three runs; its narrow guard is now `< 120`, with that measurement beside the assertion. The full integration label passes all 140 invoked tests

- [x] HOUSE-03226 — Grand-tour test: walk the real controller from the spawn to every accessible cell and back
      dep: HOUSE-03223, HOUSE-03224, HOUSE-03225 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D2, G1 · est: 4
      files: tests/integration/GrandTourTests.cpp
      accept: (1) a deterministic headless run from the `EXT_ROAD` spawn through posed doors and gates and all 8 flights to every manifest standing point, and back; (2) arrival within 0.60 m (`HOUSE-00615`'s tolerance); (3) no step ends inside geometry, the boundary counter stays 0, and nothing falls below a floor; (4) the route is computed from the portal graph and the standing points, never a hand-written script per room; (5) runtime < 120 s so it runs in CI
      verify: integration GrandTourTests.EveryAccessibleCellIsReachedOnFoot
      note: from here on every architecture and furnishing task keeps this test green. A prop that blocks a route fails its own task; no later review has to catch it
      note: (2026-09-23) the deterministic portal-graph tour drives production `PlayerStep` at 120 Hz from the road spawn through all eight flights and 90 manifest cells, then returns to the road. Its 558 derived stops completed in 90,614 controller steps with 33 local collision detours and 0 boundary, floor-loss or obstacle-penetration failures in 10.28 s. The real traversal exposed and fixed five minimal data defects: a garage-step offset, two basement service doors behind the stair wedge, the blocked L2 store connector, the attic flight's terminal rise and its misaligned landing opening. PC-2026-09-23 records the evidence and retained scope

- [x] HOUSE-03227 — Traversal sweep of every zone, and fix what it finds
      dep: HOUSE-03226 · sys: physics · plat: LNX · pri: MUST · zone: all · adv: D2, G1 · est: 2
      accept: (1) the 20-minute seeded random walk (`HOUSE-00618`) is run from a start on each level and outside; (2) a manual first-person walk of every zone checks stuck points, snagging edges, head bumps, stair transitions, attic crouch entry and exit, and eye clipping in the tightest spaces (closets, under the stair, the eaves); (3) every S1/S2 finding is fixed or filed as a task in the right milestone; (4) findings are recorded in the ledger
      verify: the seeded walks; the ledger round
      note: (2026-09-23) the reproducible seed-618 soak now runs the full 144,000 steps from B1, L0, L1, L2, L3 and `EXT_ROAD`: each start walked 1,034–1,410 m with 0 boundary escapes, falls or geometry penetrations. Round 104 records the 56-pose production review plus keyboard-driven walks in every accessible planning zone and focused closet, under-stair and attic-eaves checks. The attic camera lowered on entry and restored its standing height on exit. No new S1/S2 traversal finding was found; the already-filed architecture/lighting gaps remain owned by M2/M5

- [x] HOUSE-03240 — **Gate G1 review: every zone at C1**
      dep: HOUSE-03226, HOUSE-03227, HOUSE-03228 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G1 · est: 1
      accept: the scoreboard shows every zone ≥ C1 with no `*`; the grand tour and the integration suite are green; the open S1 list is empty; the handoff records the gate
      verify: scoreboard update in this file; `docs/handoff.md`
      note: (2026-09-23) G1 passed. Round 104 and `zone_scoreboard.py --check` cover all 11 zones and 96 cells; the seven traversal markers are cleared and the open S1 list is empty. The correctly configured integration label passes all 141 registrations together, including the 90-cell grand tour in 16.84 s under four-way load

---

## M2 — Architectural completion (C2 everywhere) · gate G2

Track A. Every zone's architecture is finished to eye-height quality before anyone furnishes
anything (rule R1). Each zone task starts from its Round 103 S2 list (its `note:`). Structure that is
really dressing (ducts, pipes, boxes) belongs to M4. **Depth follows the tier:** utility rooms get
plain, correct construction, not decoration. Where a door lacks hardware, the zone task reuses the
lever set of the finished joinery (`HOUSE-00932`, `00940`, `00942`). Every task keeps the grand tour
green.

- [x] HOUSE-03261 — Bring the basement's architecture to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G2, D1 · est: 2
      accept: C2 for all 14 `Z-B1` cells: finished rooms (cinema, gym, hobby, WC, laundry) and unfinished rooms (mechanical, electrical, utility, workshop, storage, cellar) read as such through their finishes; exposed structure (joists, columns, a slab edge) where unfinished, from the shell generator or `prop_kit_gen.py`, whichever is cheaper; door leaves have casing and hardware; basement windows or light wells as designed; the zone's S1/S2 architecture findings closed
      verify: `Z-B1` zone capture, day and night; `verify_shell.py`; grand tour
      note: Round 103 S2 architecture list: generic slab doors/casings; finished and unfinished room types do not read distinctly; the zone is nearly black even by day
      note: (2026-09-23) Round 107 confirms C2 across all 14 cells. Existing authored palettes distinguish finished rooms from the aged-plaster/concrete utility family and the stone/ochre cellar. The plywood-trim utility/storage palette now drives exposed ceiling joists, rim/slab-edge members and wall-line posts without room-id logic; all 14 B1 leaves reuse the existing four-panel/two-sided steel-lever grammar and their existing generated casings. All eight basement hoppers retain generated exterior light wells. Clear-day, clear-night and lights-forced zone sets were inspected; the focused grand tour and inside-geometry route pass. `verify_shell.py` confirms 8 flights, 128/129 wall cuts and all 63 physical leaves, with only the established nested refrigerator-container limitation unrelated to B1. Furnishing and final night readability remain assigned to M4/M5

- [x] HOUSE-03262 — Bring the ground-floor service rooms and the garage to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-L0S, Z-GAR · adv: G2, D1 · est: 2
      accept: C2 for `Z-L0S` and `Z-GAR`: the garage interior's slab, walls, ceiling and the inside face of the sectional door read as finished garage construction; the loft has a real means of access as designed and a guard; mudroom, laundry, WCs, pantry, office and closets have finished trim and door hardware
      verify: `Z-L0S` and `Z-GAR` captures; grand tour
      note: Round 103 S2 architecture list: office/mudroom night depth is unreadable; garage interior finish and inside door face do not read; loft access and guard are absent
      note: (2026-09-23) Round 108 confirms C2 for both zones. The existing authored service palettes, generated skirting/cornice/casing and finished garage envelope/inside sectional face remain intact; six remaining service leaves now explicitly reuse the established four-panel/two-sided steel-lever joinery. The garage loft gains a data-derived fixed ladder beneath its authored `H_LOFT` hatch and its four placeholder top rails become a complete timber guard with bottom/top rails, newels and balusters. Clear-day service/garage and lights-forced garage sets were inspected, including a new fixed loft-access view. The focused grand tour, inside-geometry, stair traversal and content-current checks pass. Furnishing and final night readability remain assigned to M4/M5

- [x] HOUSE-03263 — Bring the first upper floor's architecture to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G2, D1 · est: 2.5
      accept: C2 for all 20 `Z-L1` cells: bedroom, bathroom, closet and corridor finishes match their palette; window reveals, sills and interior trim finished; door joinery at least at the `HOUSE-00942` standard on landing- and hall-facing doors; balconies' guards and finishes consistent with the front balcony
      verify: `Z-L1` capture; grand tour
      note: Round 103 S2 architecture list: generic dark door openings and insufficient room-specific joinery/finish distinction across the empty floor
      note: (2026-09-23) Round 105 confirms all 20 cells' existing authored bedroom/bathroom/closet/corridor palettes, generated reveals, sills and continuous trim, and the common measured balcony-guard grammar. All eight painted or hardwood single leaves facing `L1_HALL`/`L1_HALL_W` now explicitly reuse `HOUSE-00942`'s four-panel, two-sided steel-lever joinery. The clear-day and lights-forced zone sets were inspected; the grand tour and both inside-geometry route tests pass. Furnishing and night readability remain assigned to M4/M5, not this C2 task

- [x] HOUSE-03264 — Bring the second upper floor's architecture to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G2, D1 · est: 2
      accept: as `HOUSE-03263`, for the 15 accessible `Z-L2` cells; the library's shelving walls are M4's unless the shell carries them
      verify: `Z-L2` capture; grand tour
      note: Round 103 S2 architecture list: as L1, with the library/games/sitting identities not yet supported by their architectural finish
      note: (2026-09-23) Round 106 confirms all 15 accessible cells' authored room palettes and the shared reveal/sill/trim grammar. The seven single leaves facing `L2_LANDING`, `L2_HALL` and `L2_HALL_W` now explicitly reuse the four-panel, two-sided steel-lever joinery; the landing's already-finished Juliet pair remains unchanged. Clear-day and lights-forced zone sets were inspected, and the grand tour plus both inside-geometry route tests pass. Library shelving stays correctly assigned to M4; furnishing and night readability remain M4/M5 work

- [x] HOUSE-03265 — Bring the attic's architecture to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-L3 · adv: G2, D1 · est: 2
      accept: C2 for the 5 `Z-L3` cells: visible roof structure in the stores (rafters, ridge, collar ties or purlins), insulation between joists where unfinished, a boarded walkway through the stores, dormer reveals, knee walls and a finished ceiling in `L3_ROOM`; the crouch zones read as low headroom rather than as a collision surprise
      verify: `Z-L3` capture; grand tour (crouch)
      note: Round 103 S2 architecture list: stores lack readable roof structure/unfinished construction and `L3_ROOM` lacks a clearly finished envelope
      note: (2026-09-23) Round 110 confirms C2 across all five attic cells. The existing rafter-bounded shell, dormer reveals, store walkways and `L3_ROOM` collar ceiling remain authoritative. Unfinished long-slope bays now expose recessed insulation between the existing 400 mm rafters, and one existing-section transverse purlin under each hip makes the end stores read as framed roof space without a full jack-rafter expansion. Four fixed views inspect the finished room, hip end, low eaves and insulated north bay; utility cells remain covered by the grand-tour zone walk as required by the review protocol. The all-cell grand tour and both automatic-crouch regressions pass. Furnishing and final night readability remain assigned to M4/M5

- [x] HOUSE-03266 — Bring the vertical circulation to C2
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-STAIR · adv: G2, D1 · est: 2
      accept: all 8 flights have handrails, balusters, newels, nosings, stringers and landing trim consistent with their stair's character (the main stair finished; the basement and attic stairs plainer); no flight reads as a ramp or a solid mass from its foot or its head
      verify: `Z-STAIR` capture from every foot and head; stair traversal tests
      note: Round 103 S2 architecture list: main stair foot is under-readable and the basement flight is almost black; verify every flight's trim/silhouette from both ends
      note: (2026-09-23) Round 109 confirms C2 across all eight authored flights. The existing flight builder now gives every exposed side a raked handrail and closed stringer, end newels and pitch-bounded vertical balusters; every tread retains its authored nosing and every landing gains shallow edge trim. Main-stair details reuse the finished timber trim, while the basement, attic, garage and exterior transitions use their deliberately plainer authored finishes. Five additional fixed views complete foot/head evidence alongside the four existing controls. Clear-day and lights-forced `Z-STAIR` sets plus front/rear exterior controls were inspected; both focused stair-traversal tests, the complete camera route and inside-geometry route pass. Final night readability remains assigned to M5

- [x] HOUSE-03267 — Bring the rear and side elevations up to the front elevation's architectural standard
      dep: HOUSE-03240, HOUSE-03203 · sys: content · plat: TOOL · pri: MUST · zone: Z-EXR · adv: G2, D1 · est: 1.5
      accept: from anywhere accessible in the garden and side yards: window grilles and shutters (or their deliberate absence) are consistent with `HOUSE-00933`; sills, trim, corner boards, roof edges and rainwater goods match `HOUSE-00934`/`00944`; rear doors are finished joinery; the shed's exterior reads as finished
      verify: `Z-EXR` capture, day and night
      note: Round 103 S2 architecture list: rear and side elevation trim/rainwater/joinery treatment reads materially less finished than the front
      note: (2026-09-23) Round 111 confirms C2 across the rear and side elevations. The shared shell grammar already applies framed/silled windows, painted eaves finish, hip/ridge caps, K-profile gutters and the six data-derived downspouts on every elevation; only the 17 front double-hungs opt into `HOUSE-00933`'s six-over-six grille and shutters, so the deliberately plainer rear/side windows remain consistent. Both rear sliders retain their aluminium two-panel joinery. The existing generated shed now uses the approved siding, roof, paint, steel, glass and timber families, with four corner boards, paired eaves fascia, a cased steel door and a framed/glazed double-hung window. Clear-day and clear-night four-view sets were inspected; the night set forces only the existing shed group. `EXT_SHED` remains exactly at the ordinary six-chunk target, and the property walk plus all-cell grand tour pass

- [x] HOUSE-03280 — **Gate G2 review: every zone at C2**
      dep: HOUSE-03261, HOUSE-03262, HOUSE-03263, HOUSE-03264, HOUSE-03265, HOUSE-03266, HOUSE-03267 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G2 · est: 1
      accept: an all-zone capture round with no S1/S2 architecture defect open; the scoreboard shows every zone ≥ C2; the handoff records the gate
      verify: ledger round; scoreboard
      note: (2026-09-23) G2 passed. Round 112 inspected all 65 fixed clear-day views across all eleven zones after the seven M2 completion tasks. No S1/S2 architecture defect remains open; the dark unlit secondary spaces visible in the review remain explicitly owned by M5. The scoreboard records every zone at least C2, and `zone_scoreboard.py --check` assigns all 96 authored cells exactly once across the eleven zones plus two explicit exclusions

---

## M3 — The reusable furnishing kit

Track A support: house-wide tooling, allowed at any time by R1 exception (b). It makes furnishing
about 60 empty rooms tractable with **a small reusable kit**. Most pieces are **generated**
(`HOUSE-00985` carcasses, shelving, boxes and runs; `HOUSE-00973` fill kits; `HOUSE-02681` curtains
and blinds); a few are **acquired** in capped groups; the existing ground-floor pieces are reused
before anything is acquired. Everything is reused freely across the house, varied by tint, scale,
dimensions, arrangement and small accessory swaps (rule R8).

**The kit's caps (the acquired catalogue, all groups together): 32 models plus one wall-art set of
at most 8 images.** A cap is an upper bound, not a target: acquire only what the tiered recipes of
`HOUSE-03302` need. Every acquired piece is a **static visual prop** (closed doors, no interiors,
no moving parts) with a collision proxy and a manifest and licence row, and is covered by a
group-level gate, not a gate of its own. New per-piece `--check` gates are not added.

| Group | Task | Cap | What it covers | Everything else comes from |
|---|---|---|---|---|
| Kitchen and utility appliances | `HOUSE-00975` | 3 | a washer/dryer body (one model, two variants), a chest freezer, a boiler or water heater | `HOUSE-00985` for the rest of the mechanical room |
| Bathroom fixtures | `HOUSE-00976` | 4 | toilet, basin with vanity, bath, shower tray with glass screen | `HOUSE-00985`: mirrors, towel rails, cabinets |
| Seating and tables | `HOUSE-00977` | 7 | armchair, desk or dining chair, stool, cinema seat, desk, side or coffee table, games (pool) table | the tinted ground-floor sofas and chairs |
| Beds | `HOUSE-00979` | 2 | a double and a single frame; bedding by tint | — |
| Lamps | `HOUSE-00981` | 4 | table, floor and desk lamps, a bare utility fitting | `HOUSE-00985`: the static screen and projector |
| Decoration | `HOUSE-00982` | 6 + art set | two plants, a mirror frame, a clock, a vase, a picture-frame family; one CC0 wall-art set of ≤ 8 images | generated rug planes with CC0 textures |
| Clutter | `HOUSE-00984` | 6 | a bin, a suitcase, a toolbox, a garden-tool set, a paint tin | `HOUSE-00985` boxes replace the crate; `HOUSE-00973` fill kits |
| Storage furniture | — | 0 | — | `HOUSE-00985`: wardrobe, dresser, chest, nightstand and bookcase carcasses with door and drawer-front variants |

For comparison: the legacy plan asked for about 234 acquired models (24 kitchen, 16 bathroom, 20
seating, 14 table, 12 bed, 22 storage, 26 lamp, 55 decoration, 45 clutter) and 12 fill kits; the
second reduction capped them at 62 and 6.

- [x] HOUSE-03301 — Stop furnishing from paying for the pet navigation graph
      dep: HOUSE-03201 · sys: world · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 1
      files: tools/world/validate_world.py, tools/ci/run_checks.sh, src/world/WorldLoader.cpp (only if it rejects a stale graph)
      accept: (1) no gate or world rule requires `layout.nav.json`'s waypoints, perches, beds or bowls to stay consistent with props, so placing furniture never requires moving a pet waypoint again; (2) the data files and loader stay (this task removes the coupling only); (3) the gates and unit tests are green
      verify: `run_checks.sh`; a probe prop placed across an old waypoint passes validation
      note: `HOUSE-01074` had to shift two sunroom pet waypoints to place two chairs. That cost now buys nothing
      note: (2026-09-23) Rule 5 in both offline and runtime validators now proves only player portal-graph connectivity; it no longer fails a valid walkthrough because retained pet nodes form disconnected components. The dedicated furniture-aware pet-navigation selftest is no longer a repository gate. A regression fixture places a chair directly over an isolated historical node and passes validation. `layout.nav.json`, its intrinsic reference checks, `build_nav.py`, loader support and `nav.bin` remain intact as frozen cleanup candidates; no pet system is revived

- [x] HOUSE-03302 — Room recipes and the kit catalogue
      dep: HOUSE-03201 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3, G3 · est: 1.5
      files: docs/furnishing-kit.md
      accept: (1) one recipe per room type in the house (bedroom by variant, bathroom, WC, closet and store, study, library, games and sitting room, mudroom, laundry, pantry, garage and loft, basement hall, mechanical, electrical, utility, cinema, gym, workshop, wine cellar, hobby room, attic room, attic store, shed, stair landing, balcony), each **tiered**: utility lists only the essential pieces, secondary enough to read as the room, main the complete set, hero the complete set plus secondary dressing; (2) each recipe names its pieces from the kit table above, its typical dimensions and clearances, and its **lighting preset** (fixture type, light group, schedule class) so that M5 lights rooms by type, not one by one; (3) a catalogue of the existing reusable ground-floor pieces (`HOUSE-01037`–`01076`) and the recipes they serve; (4) the reuse policy of rule R8; (5) the needed count per acquisition group, within its cap
      verify: every accessible cell in `layout.cells.json` maps to a recipe
      note: (2026-09-23) `docs/furnishing-kit.md` defines cumulative U/S/M/H recipes, measured placement envelopes and clearances, eight room-type lighting presets tied to the existing authored group roles, the reusable `HOUSE-01037`–`01076` catalogue, and an exhaustive map of all 90 intended-accessible cells. The recipes need 23 acquired models plus one ≤ 8-image art set: 3 appliances, 4 wet fixtures, 3 seating/table models, 2 beds, 2 lamps, 4 decorations and 5 clutter models; existing and generated families replace the nine unused catalogue slots. A data audit proves every `accessible: true` cell occurs exactly once and its tier matches `docs/zones.json`; the visible non-walkable garage loft separately receives its utility recipe

- [x] HOUSE-03303 — Placement validator for static props
      dep: HOUSE-03225 · sys: world · plat: TOOL · pri: MUST · zone: all · adv: D2, D3 · est: 2
      files: tools/world/validate_world.py (or a sibling checker run by it), tests
      accept: every row in `layout.props.json` (1) rests on a floor or support surface within 1 cm; (2) lies inside its cell; (3) does not intersect walls, openings or other props' collision proxies beyond a stated tolerance; (4) keeps door-swing arcs, window fronts and a ≥ 0.70 m circulation path clear; the check runs in `run_checks.sh`, and the grand tour remains the end-to-end proof
      verify: `--selftest` cases for each rejection; the current 133 props pass or are fixed
      note: (2026-09-23) `validate_props.py` now reuses the chunk reader, collision builder, cell geometry and terrain interpolation to check all 133 rows' measured LOD0 envelopes and `_COL` components. It enforces 1 cm support/containment/overlap, 0.60 m window fronts, clear apertures and a 0.70 m route; world rule 14 remains the single exact door-sweep implementation. Scoped 4 cm dining-chair/table and 12 cm counter-stool/island tolerances describe broad-box tuck beneath non-intersecting overhangs. Fifteen unsupported, outside-cell or intersecting authored placements were corrected, including terrain-grounded exterior fixtures whose linked light positions moved with them. Rejection selftests, all world rules, inside-geometry and the 90-cell controller grand tour pass

- [x] HOUSE-00971 — Per-instance tint and scale variation for placed props
      dep: HOUSE-00215, HOUSE-00891 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 1.5
      accept: (1) a prop row in `layout.props.json` may carry a tint and a uniform or per-axis scale, and a seeded jitter (yaw, small offset) where the recipe allows it; (2) the chunk builder applies them, the collision proxy follows the scale, and the placement validator (`HOUSE-03303`) checks the result; (3) the existing 133 props build unchanged when they carry none; (4) no runtime system is added: variation is baked at build time or uses existing material parameters
      verify: a probe row with tint, scale and jitter builds, validates and renders as expected; the existing chunk goldens are unchanged
      trace: was *Implement the placement pipeline: a prop row → a chunk entry or a dynamic instance, with per-instance jitter and tint*; the row → chunk path already exists, so only the variation remains (`HOUSE-03206`)
      note: (2026-09-23) `layout.props.json` now accepts positive uniform or xyz scale, RGB tint and deterministic bounded jitter (≤ 15° yaw, ≤ 0.25 m X/Z offset) resolved from SHA-256 of seed plus prop id. One shared offline transform feeds chunk vertices, inverse-transpose normals, collision proxies and the placement validator. Tint selects an already-authored, otherwise-identical canonical material variant, so no chunk-format or runtime rendering system was added. Synthetic probes prove deterministic baked geometry, material selection and matching collision/validation transforms; the C++ loader accepts xyz scale. The production 133-prop chunk file remains byte-identical at `a649fcfa6480a7f5fec3c4f796d4e926e27287675d872dbdae71cb48cbc7709c`

- [x] HOUSE-00973 — Fill-kit generator: four seeded fill kits for shelves and storage
      dep: HOUSE-00971 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 1.5
      accept: (1) four kits: books; folded textiles (towels, linen, clothes); crockery and jars; tools and paint tins; (2) seeded, varied count, spacing, lean and colour, so two invocations never look identical; (3) they dress open shelves, visible surfaces and the basement, attic and garage storage; container interiors are out of scope
      verify: two seeds side by side; the placement validator
      trace: was 12 kits (`FILL_CUTLERY` … `FILL_LINEN`), then 6; toys and shoes are cut, and lower clutter density is intended (`HOUSE-03206`)
      note: (2026-09-23) `fill_kit_gen.py` now emits four compact support-relative static families: varied upright/stacked books, folded textiles, plate stacks with jars, and hand tools with paint tins. Count, spacing, bounded lean/rotation and approved colour-role selection derive from an explicit seed; repeated equal seeds are byte-identical while a second seed for every family has different geometry/palette signatures. The four canonical GLBs total 2,164 triangles and 166 KiB, need no collision/runtime/container system, and have one group gate covering deterministic regeneration, UVs, support origin, measured shelf-scale envelopes, canonical material bridges and alternate-seed variation. A headless two-column seed comparison rendered all eight clusters on shelves with no floating, clipping or z-fighting; the production 133-prop placement validation remains clean

- [x] HOUSE-00985 — `prop_kit_gen.py`: generate storage furniture, cabinets, shelving, boxes, fixtures and service runs from data
      dep: HOUSE-00971 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 3
      accept: (1) cabinet carcasses with door and drawer-front variants that also serve as wardrobes, dressers, chests, nightstands, bookcases and kitchen/utility modules; (2) open shelving and worktops or workbenches; (3) boxes; (4) mirrors, towel rails, a static screen and a projector body; (5) ducts and pipe and cable runs from one run generator; radiators only where the house has them; (6) every piece is a static prop with a collision proxy, reproducible from data, covered by one group-level gate
      verify: the group gate; one generated example of each family placed and validated
      note: the main breadth enabler: it serves the basement, the attic, the garage and every storage room
      trace: absorbs `HOUSE-00980` (acquired storage furniture): storage furniture is generated only (`HOUSE-03206`)
      note: (2026-09-23) One compact JSON table now drives 16 deterministic static GLBs across six reusable families: six door/drawer/open carcasses, open shelving, a workbench, a lidded box, four wall fixtures, and duct/pipe/cable variants from one run author. Every asset carries one 12-triangle `_COL` box, UV0, a support- or wall-plane origin and an approved canonical material bridge; the single group gate regenerates each asset twice and pins hashes, envelopes, triangles, origins, collision and production examples. No radiator was generated because no authored house data contains one. Six family examples are placed in the master closet, attic store, basement workshop/WC/service room; all 139 production props retain support, opening and 0.70 m route clearance. All 16 assets compile through CNA, and a 4×4 rendered shape/material review found no floating, clipping or z-fighting

- [x] HOUSE-02681 — Static curtains and blinds: one generated curtain family and one blind family
      dep: HOUSE-00971, HOUSE-03302 · sys: content · plat: ALL · pri: MUST · zone: all · adv: D3 · est: 1
      accept: (1) one curtain family and one blind family, meshes and materials only, with size and tint variants; (2) placed by the room recipes in M4 and M6; (3) portals keep their glass opacity; there is no `blindFraction`, no interaction and no translucent-mode switching
      verify: one window of each type placed and captured by day and at night
      trace: absorbs `HOUSE-02682`; earlier wording (interactive blinds, the `translucent` portal mode) superseded
      note: (2026-09-23) The retained deterministic family-room curtains and a new 2,432-triangle generated slatted blind form the bounded static family. The blind has UV0, a wall-plane origin, no collision or interaction state, and uses the existing baked xyz-scale/canonical-tint recipe path; one muted-blue 0.92-width instance fits the east window of `L1_BED2`. The group gate regenerates and measures the blind, checks both family material bridges and placements, pins both referenced portals to `opacity: glass`, and rejects `blindFraction`, interaction or translucent-mode fields. All 140 props pass placement and route checks; the 790-chunk production world builds with one measured blind material batch. Fixed day/night captures of the bedroom blind and both family-room curtains show correct mounting, glass, illumination and silhouettes without clipping or z-fighting. The adjacent generated prop kit's furniture-only material roles were also corrected from UV2 shell paints to existing BasicEffect roles when the complete chunk build exposed that integration error

- [x] HOUSE-00975 — Acquire the kitchen and utility appliances (cap 3)
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 0.75
      accept: the models of the kit table's *Kitchen and utility* row, within the cap, as static props with collision proxies and manifest and licence rows
      verify: the group gate; the licence gate
      trace: was *the 24 kitchen appliance and fixture models*, then cap 4 (`HOUSE-03206`)
      note: (2026-09-23) The capped group contains exactly three CC0 static derivatives: one 600 mm front-load laundry body reused by the washer/dryer recipes, one 1.1 m chest freezer and one wall-hung combi boiler. Their official 3D Assets pack/CDN provenance and upstream SHA-256 values are pinned; the source pack declares the models AI-generated with Claude Fable 5.1 under CC0 1.0. The bounded preparation tool bakes the closed rest pose, removes every rigid open/close clip and hierarchy, adds UV0 and appends one enclosing 12-triangle collision box per asset. The shared five-role material bridge reuses existing canonical paint, steel, dark glass, cabinet glass and brass; no appliance behavior or runtime system was added. The group/licence/scale/origin/manifest gates pass, and CNA compiled the 6,254 visible triangles to three CNBs totalling 957,496 bytes. A 1200 × 700 headless review render with collision hidden confirmed recognizable closed silhouettes, grounded feet and clean lids/fronts/pipes without clipping, floating parts or z-fighting

- [x] HOUSE-00976 — Acquire the bathroom fixtures (cap 4)
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 1.25
      accept: the kit table's *Bathroom fixtures* row, within the cap, reused across every bathroom and WC, as static props with collision proxies and manifest and licence rows
      verify: the group gate; the licence gate
      trace: was *the 16 bathroom fixture models*, then cap 6 (`HOUSE-03206`)
      note: (2026-09-23) The capped family contains exactly four matching CC0 static derivatives from one source pack: a close-coupled WC, 600 mm vanity with inset basin, 1700 mm bath, and 900 mm shower whose one model includes both tray and glazed enclosure. The official 3D Assets pack/CDN provenance and upstream SHA-256 values are pinned; the pack declares the models AI-generated with Claude Fable 5.1 under CC0 1.0. The closed-static preparation path from `HOUSE-00975` bakes transforms, strips the WC seat/lid, vanity drawer and shower-door clips and hierarchy, adds UV0 and appends one enclosing 12-triangle collision box per asset. Canonical porcelain, steel, clear glass, paint and dark accent roles are reused; no plumbing or interaction behavior was introduced. The group/licence/scale/origin/manifest gates pass, CNA compiled the 5,056 visible triangles to four CNBs totalling 778,304 bytes, and a 1300 × 720 headless review render confirmed recognizable grounded fixtures with closed parts and clean silhouettes without clipping, floating parts or z-fighting

- [x] HOUSE-00977 — Acquire the seating and tables (cap 7)
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 2
      accept: the kit table's *Seating and tables* row, within the cap; the ground-floor sofas and chairs are reused (tinted) before anything is acquired; static props with collision proxies and manifest and licence rows
      verify: the group gate; the licence gate
      trace: absorbs `HOUSE-00978` (tables and desks); was *the 20 seating models* and *the 14 table and desk models*, then caps 6 and 5 (`HOUSE-03206`)
      note: (2026-09-23) R8 reuse eliminated four acquisitions: the existing ground-floor sofa, armchair, dining-chair, stool, bench, coffee/side-table and dining-table families remain the first choice and can take recipe tints. Only three CC0 static derivatives were needed within cap seven: a four-place seat-down cinema tier, a 1.4 m writing desk and a regulation nine-foot pool table. Their official 3D Assets pack/CDN provenance and upstream SHA-256 values are pinned; the cinema/pool packs declare Claude Opus 5 and the furniture pack Muse Spark via T3 Code as their AI generators. The shared bounded preparation path bakes the authored rest pose, removes tip-up-seat/drawer clips and hierarchy, adds UV0 and appends one enclosing 12-triangle proxy per asset. Existing canonical fabric, timber, paint, metal and accent materials cover every role; no furniture interaction was introduced. The group/licence/scale/origin/manifest gates pass, CNA compiled the 11,472 visible triangles to three CNBs totalling 1,303,272 bytes, and a 1400 × 720 headless review render confirmed grounded, recognizable silhouettes with open leg/seat spaces, six clear pool pockets and no clipping, floating parts or z-fighting

- [x] HOUSE-00979 — Acquire the bed frames (cap 2)
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 1
      accept: a double and a single frame; the child's bed is the single frame scaled or tinted; bedding varies by tint; static props with collision proxies and manifest and licence rows
      verify: the group gate; the licence gate
      trace: was *the 12 bed and bedding models*, then cap 3 (`HOUSE-03206`)
      note: (2026-09-23) The capped family contains exactly two CC0 static derivatives from the official 3D Assets Bedroom and Living Room Furniture pack: one 1.71 × 2.19 m upholstered double and one 0.98 × 2.00 m timber single. The pack/CDN provenance and upstream SHA-256 values are pinned, including its Muse Spark via T3 Code generator declaration. The existing bounded static preparation path bakes transforms and hierarchy, applies only vertical normalization to put both mattress tops at the canonical 0.60 m, adds UV0 and appends one enclosing 12-triangle collision proxy apiece. Existing canonical timber and textile roles allow bedding tint variation, and the same single frame serves child rooms through the already-baked scale/tint path; no third bed or runtime system was added. The group/licence/scale/origin/manifest gates pass, CNA compiled the 3,928 visible triangles to two CNBs totalling 472,032 bytes, and a 1400 × 720 headless review confirmed recognizable grounded frames, clean bedding/headboard silhouettes and no clipping, floating parts or z-fighting

- [x] HOUSE-00981 — Acquire the lamps (cap 4)
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3, D6 · est: 1
      accept: the kit table's *Lamps* row, within the cap, usable as the physical fixtures of M5's light groups; static props with collision proxies and manifest and licence rows
      verify: the group gate; the licence gate
      trace: was *the 26 small-appliance and lamp models*, then cap 6 (`HOUSE-03206`)
      note: (2026-09-23) R8 reuse retained six existing table, floor, ceiling, pendant and task-puck fixture families; only the two shapes still named by the kit were acquired within cap four. The 0.60 m articulated desk lamp comes from the official CC0 3D Assets Bedroom and Living Room Furniture pack (Muse Spark via T3 Code), and the unadorned 2.4 m twin-tube utility fitting from its Woodworking Workshop and Joinery pack (Claude Opus 5). Both official CDN bytes and upstream hashes are pinned. The shared bounded static preparation path bakes their hierarchy, adds UV0 and one enclosing 12-triangle proxy apiece; the desk lamp retains a support-plane origin while the utility fitting retains its ceiling fixing plane. Existing warm and neutral switched-emissive roles mark the physical diffuser meshes for M5 without any new runtime lighting system. The group/licence/scale/origin/manifest gates pass, CNA compiled the 944 visible triangles to two CNBs totalling 132,784 bytes, and a 1400 × 720 headless review showed both support contacts, recognizable silhouettes and no clipping, floating parts or z-fighting

- [x] HOUSE-00982 — Acquire the decoration set (cap 6 plus one wall-art set)
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 1.25
      accept: the kit table's *Decoration* row, within the cap, including one CC0 wall-art set of at most 8 images with frame variants; rugs are generated planes with CC0 textures; static props with collision proxies and manifest and licence rows
      verify: the group gate; the licence gate
      trace: absorbs `HOUSE-02688`; was *the 55 decoration models*, then cap 15 (`HOUSE-03206`)
      note: (2026-09-23) Rule R8 retained the existing ground-floor plant, frame and rug families, so only four CC0 models were acquired within cap six: a grouped second plant form, standing mirror, wall clock and narrow vase. Official 3D Assets CDN bytes and generator declarations are provenance/hash pinned. The shared bounded static path bakes their hierarchy, UV0 and one 12-triangle proxy apiece; the plant's unused required-WebP preview nodes are stripped because its named slots bind existing canonical materials, avoiding an unsupported CNA source extension without changing visible roles. The one wall-art set contains four selected CC0 OpenGameArt abstracts (two landscape, one centred portrait and one square crop) in three deterministic wall-plane frame variants, below the eight-image ceiling. Existing generated entry/hall rug planes continue to use ambientCG's recorded CC0 Fabric061 weave. One group gate pins the four acquisitions, four textures, four reproducible frames, manifests, licences, bounds, origins, materials and rug reuse. The group totals 6,994 visible and 96 collision triangles; CNA compiled the eight models to 758,912 bytes and the four art textures to 3,182,144 bytes. Individual headless frame renders and a decoration composition review found grounded support/fixing planes, distinct readable images and no clipping, floating parts or z-fighting

- [x] HOUSE-00984 — Acquire the clutter set (cap 6)
      dep: HOUSE-00296, HOUSE-03302 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: D3 · est: 1.25
      accept: the kit table's *Clutter* row, within the cap; boxes come from `HOUSE-00985` and pantry and shelf items from `HOUSE-00973`; static props with collision proxies and manifest and licence rows
      verify: the group gate; the licence gate
      trace: absorbs `HOUSE-00983`; was *the 45 box, crate, bin, tool and clutter models*, then cap 12 (`HOUSE-03206`)
      note: (2026-09-23) The capped group contains exactly the five recipe shapes, one below cap six: a static office bin, upright suitcase, closed-pose cantilever toolbox, grounded long-handled garden-tool set and one 5 litre paint tin. All are official 3D Assets CC0 releases with publisher bytes and upstream SHA-256 pins. The existing bounded static preparation path bakes hierarchy and UV0, removes the toolbox source clips after retaining its compact default pose, and appends one enclosing 12-triangle proxy per model. The completed generated storage box replaces a crate and the four fill-kit families remain the pantry/shelf source; no duplicate acquisitions or runtime behaviour were added. The group/licence/scale/origin/manifest gates pass, CNA compiled 6,210 visible triangles to five CNBs totalling 869,976 bytes, and a 1400 × 720 headless workbench review showed all five recognizable and grounded without clipping, floating parts or z-fighting

---

## M4 — Dressing everywhere (the furnishing half of C3) · checkpoint

Track A. Every accessible room gets its recipe's pieces **for its tier** (rule R8). For secondary
and utility rooms this is their final furnishing. Main cells get the main-tier set here and are
finished to C4 in M6a; hero areas get the main-tier set here and their C5 pass in M6b. Work the
zones **least complete first** (R2), at most three consecutive tasks per zone (R3). The expected
order today: `Z-L1` → `Z-L2` → `Z-B1` → `Z-L3` → `Z-L0S` → `Z-GAR` → `Z-STAIR` → `Z-EXR` → `Z-STR` →
`Z-EXF`. `Z-L0M` is already dressed and gets **no** M4 task (R4). Lighting is not judged here; it is
M5's, and C3 is complete only at G3. **Rooms of one type reuse one arrangement**, adjusted to the
room: two double bedrooms may share a layout, and every WC uses the same fixtures.

**Acceptance of every task below:** (1) the recipe's pieces for each named cell's tier
(`HOUSE-03302`) are present, so the room's purpose reads at a glance; (2) all props are static with
collision proxies; (3) the placement validator (`HOUSE-03303`) and the grand tour (`HOUSE-03226`)
stay green; (4) no S1/S2 defect in the zone's fixed views for these cells; (5) kit first and reuse
freely, with no bespoke authoring (rule R8). **Verify:** the zone's capture set by day; the
placement validator; the grand tour. Every task depends on gate G2 (`HOUSE-03280`), the variation
and validation tooling (`HOUSE-00971`, `HOUSE-03303`), the recipes (`HOUSE-03302`) and the kit
groups on its `dep:` line.

### `Z-L1` — first upper floor

- [x] HOUSE-00999 — Dress `L1_LANDING`, `L1_HALL` and `L1_HALL_W`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00982 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 0.75
      note: (2026-09-23) Round 113 places a reused upholstered bench on the landing and one scaled existing runner plus one reused framed-art instance in each hall. All five rows are static; the bench and frames keep their existing proxies while floor runners remain intentionally non-colliding. The exact door-sweep and 0.70 m route validators pass, the grand tour still reaches every accessible cell, and measured chunk totals are `L1_LANDING` 14/14, `L1_HALL` 10/10 and `L1_HALL_W` 10/10 with no split. Actual fixed-day and forced-light detail renders show grounded pieces, clear portals and readable art without clipping, floating geometry or z-fighting. Placing the acquired art exposed two existing Reach defects in its bounded offline preparation: non-power-of-two textures and reversed picture-plane winding. The same four-member family now uses 512-square runtime textures and a front-facing image plane; no runtime system or new asset was added

- [x] HOUSE-01000 — Furnish `L1_MASTER_BED` with its main-tier set
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00979, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 1
      note: a main cell; its C4 pass is `HOUSE-03444`
      note: (2026-09-23) Round 114 places the complete cumulative `R-BED-PRIMARY` main-tier set as ten kit-first rows: the acquired double bed, two generated nightstands, generated wardrobe and dresser, reused armchair, writing desk and dining chair, reused rug and the existing open-curtain family across the paired south windows. The eight solid pieces retain their asset proxies while the thin floor textile and open window treatment remain intentionally non-colliding; every row is static. Exact prop, door-sweep and 0.70 m route validation passes, the 90-cell grand tour passes, and the measured cell stays at its exact 19/19 chunk ceiling without a Reach split. Clear-day, forced-light and reciprocal detail renders show grounded furniture, open glazing, readable room purpose and no clipping, floating geometry, z-fighting or blocked portal. The known inside-geometry push-out failures in `L1_LANDING` and `L1_MASTER_CLOSET` predate this task; the two master-bedroom failures exposed during placement were cleared by moving the east wardrobe out of the wall-midpoint probe and retaining the south slider approach beside the bed

- [x] HOUSE-01001 — Furnish `L1_MASTER_BATH` and `L1_MASTER_CLOSET`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00976, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 1
      note: (2026-09-23) Round 115 completes both secondary recipes with seven net new static rows and no new asset: the master bath has the acquired bath, vanity-basin and WC plus generated mirror and towel rail; the walk-in closet repositions its stable-id wardrobe away from the north-wall push-out, adds a generated dresser as the second carcass and places one bounded folded-textile cluster on top. Six solid or wall-mounted pieces keep their existing proxies and the small elevated textile cluster is intentionally non-colliding. Exact placement, door-sweep and 0.70 m route validation passes; the 90-cell grand tour passes; the prior closet inside-geometry failure is gone. Measured chunk totals are exactly 12/12 in the bathroom and 8/8 in the closet with no Reach split. Clear-day, forced-light and reciprocal room-side renders show grounded fixtures and carcasses, a clear fixed camera and both closet portals, with no clipping, floating geometry, z-fighting or blocked route. The separate older `L1_LANDING west` push-out beside `HOUSE-00999`'s bench remains the only failure in the broader inside-geometry probe

- [x] HOUSE-01002 — Furnish the double bedrooms `L1_BED2` and `L1_BED5` (the guest room)
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00979, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 1.25
      trace: absorbs `HOUSE-01005`
      note: (2026-09-24) Round 118 completes the cumulative `R-BED-DOUBLE` secondary recipe in both rooms with the same four established pieces: acquired double bed plus generated nightstand, wardrobe and dresser. All eight rows are static and retain their existing proxies; the guest-room dresser uses a bounded 0.75 scale so the compact room keeps a clear east-side route between its stair and bathroom doors. Exact placement, support, door-sweep and 0.70 m route validation passes for all 180 props, and the 90-cell grand tour passes. The wider inside-geometry probe reports no bedroom regression; its sole failure remains the older `L1_LANDING west` point beside `HOUSE-00999`'s bench. Both measured cells build at their exact 15/15 chunk ceilings with no split. The clear-day zone set and reciprocal forced-light detail renders show grounded furniture, readable room purpose and no clipping, floating geometry, z-fighting or blocked portal; final normal-light readability remains M5 scope

- [x] HOUSE-01003 — Furnish the children's rooms `L1_BED3` and `L1_BED4`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00979, HOUSE-00984, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 1.25
      trace: absorbs `HOUSE-01004`; the "deliberately untidy teenager's room" is a tint and arrangement variant, not extra clutter
      note: (2026-09-24) Round 121 completes the cumulative `R-BED-SINGLE` secondary recipe in both rooms with ten static kit-first rows: each receives the acquired single bed, generated nightstand and wardrobe, acquired writing desk and a reused dining chair. The smaller child's frame uses the acquisition's supported 0.90 child scale; the teenager's room is distinguished by its existing muted-teal/grey palette and a different furniture arrangement, with no optional clutter added. All rows retain their existing proxies. Exact support, overlap, opening, door-sweep and 0.70 m route validation passes for all 215 props, the 90-cell grand tour and `world-content-current` pass, and iteration moved both beds away from the wider inside-geometry probe's cardinal starts; its sole remaining failure is again the older `L1_LANDING west` point beside `HOUSE-00999`'s bench. Both rooms build at exactly 14/14 chunks with no vertex or Reach-cap split. Clear-day zone controls and reciprocal forced-light room renders show grounded furniture, readable bed/work/storage groups and clear openings without furniture clipping, floating geometry or z-fighting; normal-light balance remains M5 scope

- [x] HOUSE-01006 — Furnish `L1_BATH2`, `L1_BATH3`, `L1_WC3` and `L1_WC4`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00976, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 1
      note: (2026-09-24) Round 123 completes the four secondary wet-room recipes with eighteen static kit-first rows. Each bathroom has an acquired bath or shower, generated vanity-basin and toilet, plus the existing generated mirror and towel rail; each compact WC reuses the same vanity, toilet, mirror and rail family without variants or plumbing behaviour. Solid and wall-mounted pieces retain their existing proxies. Exact support, overlap, wall-plane, opening, door-sweep and 0.70 m route validation passes for all 242 props, and the 90-cell grand tour plus `world-content-current` pass. Iteration moved fixtures away from three new cardinal push-out starts; the wider inside-geometry result is back to only the two older `L1_LANDING west` bodies beside `HOUSE-00999`'s bench. Measured chunks are exactly `L1_BATH2` 12/12, `L1_BATH3` 11/11, `L1_WC3` 9/9 and `L1_WC4` 9/9, with no vertex or Reach-cap split. Clear-day controls and direct forced-light room views show grounded bath/shower/vanity/WC layouts, supported wall pieces and clear required routes without clipping, floating geometry or z-fighting; final normal-light balance remains M5 scope

- [x] HOUSE-01007 — Dress `L1_LINEN`, `L1_STOR`, `L1_CLOSET_2`, `L1_CLOSET_3` and the two balconies
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00985, HOUSE-00771 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G3, D3 · est: 0.75
      trace: absorbs `HOUSE-01008` (`L1_BALCONY_REAR`, `L1_BALCONY_FRONT`)
      note: (2026-09-24) Round 128 completes the four utility recipes and both secondary balcony recipes with nine static rows from the existing capped kit. Each closet receives one physically scaled open shelf, `L1_STOR` adds the required storage box, and each balcony stops at one reused outdoor lounger plus one reused side table; no planter, optional clutter, asset or runtime system was added. Exact support, overlap, opening, door-sweep and 0.70 m route validation passes for all 303 props after moving four shelves off the wider probe's cardinal axes. The 90-cell grand tour, authored-world validator and `world-content-current` pass; the broader inside-geometry probe returns to only its pre-existing `L1_LANDING west` and `L2_LIBRARY north` locations (four bodies total). The build measures 983 chunks: `L1_LINEN`, `L1_CLOSET_2` and `L1_CLOSET_3` remain at 6/6, `L1_STOR` is 9/9, the front balcony 9/9 and the rear balcony 7/7, with no new vertex or Reach-cap split. Clear-day zone controls and reciprocal full-resolution balcony/closet views show grounded furniture, clear openings and no clipping, floating geometry or z-fighting. Door leaves and the 0.90 m depth occlude useful camera coverage in the linen/store pair, so those placements are claimed from exact geometry and traversal evidence rather than an overstated screenshot review; normal-light balance remains M5 work

### `Z-L2` — second upper floor

- [x] HOUSE-01009 — Dress `L2_LANDING`, `L2_HALL` and `L2_HALL_W`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00982 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G3, D3 · est: 0.75
      note: (2026-09-24) Round 116 reuses the upholstered piano bench on the landing and one scaled existing runner plus one acquired framed-art instance in each hall. All five rows are static; the bench and frames retain their solid proxies while the thin floor runners remain intentionally non-colliding. Exact placement, door-sweep and 0.70 m route validation passes, the 90-cell grand tour passes, and measured chunk totals are exactly `L2_LANDING` 14/14, `L2_HALL` 10/10 and `L2_HALL_W` 9/9 with no Reach split. Clear-day, forced-light and direct detail renders show the grounded bench, flat runners and correctly wall-mounted images without clipping, floating geometry, z-fighting or blocked portals. The wider inside-geometry probe reports no L2 failure; its sole failure remains the older `L1_LANDING west` point beside `HOUSE-00999`'s bench

- [x] HOUSE-01010 — Furnish `L2_LIBRARY` with its main-tier set: shelving walls with books, reading chairs, a desk
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00977, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G3, D3 · est: 1.5
      note: hero area H5; its C5 pass is `HOUSE-03454`
      note: (2026-09-24) Round 119 completes the library's main-tier composition with sixteen static rows from the capped kit: five bookcases and five shelf-height book clusters form south/east shelving walls, while paired reading chairs, a side table, desk, rug and open north-window treatment preserve the central route and both portals. The book clusters use the existing `MAT_FAMILY_BOOK_SPINES` BasicEffect material because their reusable source model has no UV1 and its unrelated paint mappings would otherwise select DualTexture; no asset, material or rendering system was added. Exact placement, support, door-sweep and 0.70 m route validation passes for all 196 props after moving the standing point into the clear east aisle. The 90-cell grand tour and world-content-current gate pass; the wider inside-geometry probe has no L2 regression and still reports only the older `L1_LANDING west` failure. The measured library builds at exactly 17/17 chunks with no vertex or Reach-cap split. Clear-day, forced-light and reciprocal full-resolution renders show grounded furniture, readable room purpose, clear portals and no clipping, floating geometry or z-fighting; lamps, plants, art and denser shelf dressing remain correctly deferred to the H5 C5 task rather than being revived here

- [x] HOUSE-01011 — Furnish `L2_GAMES` and `L2_SITTING`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00981 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G3, D3 · est: 1.25
      trace: absorbs `HOUSE-01012`; both are secondary cells since `HOUSE-03206` (the pool table reads the games room; the dartboard, arcade cabinet, jigsaw and record player are not required)
      note: (2026-09-24) Round 122 completes both bounded secondary recipes with nine static kit-first rows. `L2_GAMES` has the existing acquired pool table plus two reused dining chairs and a side table; `L2_SITTING` has two reused dining chairs, a side table, floor lamp and rug. Solid pieces retain their existing proxies while the lamp and thin rug remain intentionally non-colliding. Exact support, overlap, opening, door-sweep and 0.70 m route validation passes for all 224 props. The first wider inside-geometry run exposed one inaccessible wall/furniture pocket in the games room; moving its compact seating group into the clear west bay removed it without weakening a test, restoring the result to only the two older `L1_LANDING west` bodies beside `HOUSE-00999`'s bench. The 90-cell grand tour and `world-content-current` pass. Both rooms build at exactly 14/14 chunks with seven measured furniture finish groups and no vertex or Reach-cap split. Clear-day, forced-light and reciprocal full-resolution renders show grounded, separated pieces, readable room purposes and clear doors/windows without clipping, floating geometry or z-fighting. Arcade machines, dartboard, jigsaw, record player and extra clutter remain intentionally out of scope; normal-light balance remains M5 work

- [x] HOUSE-01013 — Furnish `L2_BED6` (sewing room) and `L2_BED7`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00979 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G3, D3 · est: 1
      note: (2026-09-24) Round 125 completes both secondary bedrooms with ten static kit-first rows. `L2_BED6` uses the acquired double frame at a physically plausible 0.80 small-double scale, a generated nightstand and wardrobe, plus the existing writing desk, reused dining chair and generated chest as its bounded sewing/guest composition; no dress-form model or bespoke sewing system was added. `L2_BED7` uses the established four-piece double-bedroom recipe: bed, nightstand, wardrobe and dresser. Every solid row retains its existing collision proxy. Exact support, overlap, opening, door-sweep and 0.70 m route validation passes for all 263 props, and the 90-cell grand tour plus `world-content-current` pass. Moving the BED6 frame off the north-wall midpoint removed its only new inside-geometry dead end; the wider probe returns to the two pre-existing `L1_LANDING west` and `L2_LIBRARY north` results. Measured chunks are exactly `L2_BED6` 17/17 and `L2_BED7` 14/14 with no vertex or Reach-cap split. Clear-day, forced-light and reciprocal full-resolution renders show grounded, separated furniture, a readable sewing group, open windows/doors and clear routes without clipping, floating geometry or z-fighting; normal-light balance remains M5 work

- [x] HOUSE-01014 — Furnish `L2_BATH4`, `L2_BATH5`, `L2_WC5` and `L2_WC6`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00976, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G3, D3 · est: 1
      note: (2026-09-24) Round 127 completes all four secondary wet-room recipes with eighteen static rows from the existing capped bathroom and generated prop kits. `L2_BATH4` uses the acquired shower while `L2_BATH5` uses the acquired bath; both bathrooms and both WCs reuse the established toilet, vanity, mirror and towel-rail families. All pieces retain their existing proxies, with no new asset, material or runtime system. Exact support, overlap, wall-plane, opening, door-sweep and 0.70 m route validation passes for all 294 props. An initial bath arrangement created a `L2_BATH5` cardinal push-out pocket; moving the fixtures off the affected axes removed it without weakening a test, returning the wider inside-geometry result to the two pre-existing `L1_LANDING west` and `L2_LIBRARY north` failures. The 90-cell grand tour and `world-content-current` pass. Measured chunks are exactly `L2_BATH4` 13/13, `L2_BATH5` 10/10 and both WCs 9/9, with no vertex or Reach-cap split. Clear-day zone controls and direct forced-light renders show grounded, separated fixtures, supported wall pieces and clear openings without clipping, floating geometry or z-fighting; final normal-light balance remains M5 work

- [x] HOUSE-01015 — Dress `L2_STOR2`, `L2_CLOSET_4`, `L2_LINEN2` and `L2_STAIR_ATTIC`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G3, D3 · est: 0.5
      note: (2026-09-24) Round 137 completes the three remaining `Z-L2` utility recipes with four static rows from the existing generated kit: each hall closet receives one physically scaled open shelf, and the long three-door store receives one low shelf and one box while retaining its validated 0.70 m through-route. `L2_STAIR_ATTIC` retains the existing slim bench and wall art from `HOUSE-03343`, which already satisfy its secondary landing recipe without narrowing the stair. No asset, material family or runtime system was added. All 387 production props pass exact support, overlap, opening, door-sweep and route validation. The authored-world validator and 90-cell grand tour pass; the wider inside-geometry probe remains exactly at its four pre-existing bodies, first `L1_LANDING west`. The build measures 1,083 chunks / 2,920,463 vertices / 76,860,136 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk; `L2_CLOSET_4` and `L2_LINEN2` remain 6/6 and `L2_STOR2` is exactly 9/9. Six fixed and four direct forced-light views show grounded shelves and box, clear doors and no clipping, floating geometry or z-fighting; final normal-light readability remains M5 work

### `Z-B1` — basement

- [x] HOUSE-01020 — Dress `B1_HALL`, `B1_STAIR` and `B1_UNDERSTAIR`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G3, D3 · est: 0.75
      note: (2026-09-24) Round 117 completes the three cumulative secondary/utility recipes with five static kit-first rows: `B1_HALL` has a reused narrow console and bench, `B1_STAIR` has a second slim bench outside the flight and doorway, and the low `B1_UNDERSTAIR` store has a scaled open shelf plus storage box beyond its crouched standing point. Every row retains its existing collision proxy. Placement, door-sweep and exact 0.70 m route validation passes, the 90-cell grand tour passes, and measured chunk totals are `B1_HALL` 9/9, `B1_STAIR` 11/11 and `B1_UNDERSTAIR` 9/9 with no Reach split. Furnishing the L-shaped store exposed that the placement validator required a circulation disc to fit one authored box rather than their geometric union; its bounded fix now tests the exact axis-aligned union, with focused seam, missing-corner and turning-route selftests. Clear-day, forced-light and direct detail renders show grounded pieces, a clear stair/door route and no clipping, floating geometry or z-fighting. The wider inside-geometry probe has no B1 failure; its sole failure remains the older `L1_LANDING west` point beside `HOUSE-00999`'s bench

- [x] HOUSE-01021 — Equip `B1_MECHANICAL`, `B1_ELECTRICAL` and `B1_UTILITY` with static plant, panels and runs
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00975, HOUSE-00985, HOUSE-00386, HOUSE-00387 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G3, D3 · est: 1.25
      accept: the furnace or boiler, water heater and water main placed from the plumbing/HVAC data (which only positions them), an electrical panel, and duct, pipe and cable runs from `HOUSE-00985`; everything static
      trace: absorbs `HOUSE-01022`
      note: (2026-09-24) Round 120 completes the three utility-tier recipes with nine new static rows plus the retained duct-kit example. Two bounded instances of the acquired boiler form the boiler/water-heater pair around the authored HVAC plant point; the water-main pipe follows the documented south-foundation entry and a second duct forms the ceiling trunk. A scaled generated utility module reads as the electrical panel beside two cable runs, while two utility-room pipes mark the STACK-A/STACK-D service landings. Thin wall/ceiling/floor runs are intentionally non-colliding; both plant bodies and the panel retain their proxies. Exact placement, support, door-sweep and 0.70 m route validation passes for all 205 props after moving the mechanical standing point into its clear west aisle and keeping the panel outside both door arcs. The 90-cell grand tour and world-content-current gate pass. A first panel position exposed and then eliminated a west-wall push-out pocket; the final wider inside-geometry result has no B1 failure and again reports only the older `L1_LANDING west` issue. Measured chunks are exactly `B1_MECHANICAL` 11/11, `B1_ELECTRICAL` 8/8 and `B1_UTILITY` 6/6, with no vertex or Reach-cap split. Clear-day zone controls and direct forced-light room renders show grounded plant/panel bodies, supported runs, clear doors and no clipping, floating geometry or z-fighting; final normal-light readability remains M5 scope

- [x] HOUSE-01023 — Furnish `B1_CINEMA` with its main-tier set, and `B1_WC7`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00976, HOUSE-00977, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G3, D3 · est: 1.5
      accept: a static screen and projector body (no playback), two rows of cinema seats, acoustic panels; `B1_WC7` from the bathroom set
      note: hero area H6; its C5 pass is `HOUSE-03455`
      note: (2026-09-24) Round 124 completes the cinema's bounded main-tier set with a static light-faced screen, generated projector, two acquired four-seat rows and four upholstered acoustic panels, and completes the compact WC with the established toilet, vanity, mirror and towel rail. All eleven new rows are static and reuse existing assets/materials; solid floor pieces and the projector retain their proxies, while the screen and thin wall panels rely on the wall collision already in front of them so they cannot create unreachable slivers. Exact support, overlap, wall-plane, opening, door-sweep and 0.70 m route validation passes for all 253 props, the 90-cell grand tour and `world-content-current` pass, and the wider inside-geometry probe returns to only the two older `L1_LANDING west` bodies beside `HOUSE-00999`'s bench. Measured chunks are exactly `B1_CINEMA` 13/13 and `B1_WC7` 10/10, with no vertex or Reach-cap split. Clear-day, forced-light, reciprocal cinema and hall-side WC renders expose both seat rows, the screen/projector, wall panels and grounded bathroom fixtures without clipping, floating geometry, z-fighting or a blocked route; the screen and panels were moved from the structural wall thickness onto the measured interior faces after the first render exposed them. Playback, posters, clutter and hero polish remain correctly deferred or out of scope; normal-light balance remains M5 work

- [x] HOUSE-01024 — Furnish `B1_GYM` and `B1_WORKSHOP`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G3, D3 · est: 1.25
      note: `B1_WORKSHOP` is a main cell; its C4 pass is `HOUSE-03441`
      note: (2026-09-24) Round 126 completes the secondary gym and main-tier workshop with thirteen static kit-first rows beyond the retained workbench example. The gym uses three repeated generated mirror panels, a reused upholstered bench and scaled shelf/box forms as its bounded bench-and-rack group while preserving a measured open exercise floor. The workshop adds a second bench, open storage, supported toolbox/tools/paint tin, one wall service run and a floor bin; source models with shell-only paint roles use existing BasicEffect steel/dark overrides rather than gaining UV1 or a new material system. Exact support, overlap, opening, door-sweep and 0.70 m route validation passes for all 276 props. The first mirror placement exposed a north-wall push-out pocket; removing collision only from the thin wall panels, whose backing wall already collides, restores the wider inside-geometry result to the two pre-existing `L1_LANDING west` and `L2_LIBRARY north` failures without weakening the test. The 90-cell grand tour and `world-content-current` pass. Measured chunks are exactly `B1_GYM` 14/14 and `B1_WORKSHOP` 11/11 with no vertex or Reach-cap split. Clear-day, forced-light, direct and reciprocal full-resolution renders show the open gym floor, grounded bench/rack group and complete two-bench workshop composition without clipping, floating geometry, z-fighting or blocked portals; normal-light balance remains M5 work

- [x] HOUSE-01025 — Dress `B1_STOR1`, `B1_STOR2`, `B1_CELLAR`, `B1_LAUNDRY2` and `B1_HOBBY`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00975, HOUSE-00984, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G3, D3 · est: 1.5
      note: (2026-09-24) Round 132 completes the five remaining basement recipes with fourteen static rows from the existing capped kit. Each utility store stops at one open shelf and one box; the secondary cellar uses two shelf runs, a supported crockery cluster and one crate; the utility laundry uses two material variants of the acquired static laundry body; and the secondary hobby room uses the acquired desk, one reused dining chair, an open shelf and supported tool fill. Solid floor pieces retain their proxies while the two small shelf fills remain intentionally non-colliding on collidable shelves; no asset, material family, behaviour or runtime system was added. Exact support, overlap, opening, door-sweep and 0.70 m route validation passes for all 336 props. The authored-world validator, 90-cell grand tour and `world-content-current` pass; the wider inside-geometry probe remains exactly at its four pre-existing bodies, first `L1_LANDING west`. The build measures 1,032 chunks / 2,796,371 vertices / 72,914,056 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk. Exact cell ceilings are `B1_STOR1` 10/10, `B1_STOR2` 10/10, `B1_CELLAR` 9/9, `B1_LAUNDRY2` 11/11 and `B1_HOBBY` 9/9. Fixed controls and direct forced-light views show grounded, separated compositions, supported fills, clear windows/openings and no clipping, floating geometry or z-fighting; the deliberately dark normal-light balance remains M5 work

### `Z-L3` — attic

- [x] HOUSE-01016 — Furnish `L3_ROOM`, the finished attic room, with its main-tier set
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00984, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L3 · adv: G3, D3 · est: 1
      accept: an old sofa, a desk, boxes and a few toys from the kit, read as a lived-in attic room
      note: a main cell since `HOUSE-03206` (it was hero area H7); its C4 pass is `HOUSE-03446`
      note: (2026-09-24) Round 133 completes the main-tier attic-room recipe with twelve static rows from the existing capped kit: the reused leather sofa and side table form the old sitting corner; the acquired desk, reused dining chair and generated bookcase form the work corner; and a rug, storage chest, full-size box, scaled toy-box pair, children's-book cluster and open double-dormer curtain complete the lived-in composition. ADR-0016 removed the separate toys fill kit, so the bounded children's cluster deliberately reuses the retained storage and books families instead of reviving that asset group; no new model, material family, behaviour or runtime system was added. All 348 production props pass exact support, overlap, window-front, opening, door-sweep and 0.70 m route validation. The authored-world validator, 90-cell grand tour and `world-content-current` pass; the wider inside-geometry probe remains exactly at its four pre-existing bodies, first `L1_LANDING west`. The build measures 1,044 chunks / 2,834,069 vertices / 74,120,392 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk; `L3_ROOM` is exactly 21/21 with no new split. Fixed and reciprocal forced-light views show grounded, separated furniture, an unobstructed store route and correctly mounted open dormer treatment without clipping, floating geometry or z-fighting; final normal-light readability remains M5 work

- [x] HOUSE-01017 — Dress the attic stores `L3_STORE_W`, `L3_STORE_N`, `L3_STORE_S` and `L3_STAIR_HEAD` as long-term storage
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L3 · adv: G3, D3 · est: 1
      accept: enough generated boxes, suitcases and an old wardrobe to read as decades of storage, clear of the walkway; the box count is not a target
      trace: absorbs `HOUSE-01019`; the legacy "40 boxes" and spider's web are not required
      note: (2026-09-24) Round 136 completes the bounded long-term-storage composition with twelve static rows from the existing kit. The west store adds an old wardrobe, a second low shelf, suitcase and box; the north and south stores add low shelves and small box groups; and the narrow strip beyond the attic-stair opening gains one scaled shelf and box while preserving the full stair route. No asset, material family, behaviour or runtime system was added. All 383 production props pass exact support, overlap, opening, door-sweep and 0.70 m route validation, and the authored-world validator plus 90-cell grand tour pass. The wider inside-geometry probe remains exactly at its four pre-existing bodies, first `L1_LANDING west`. The build measures 1,077 chunks / 2,913,983 vertices / 76,652,776 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk; exact ceilings are `L3_STORE_W` 14/14, `L3_STORE_N` 11/11, `L3_STORE_S` 10/10 and `L3_STAIR_HEAD` 12/12. Fixed and direct forced-light views show grounded storage clusters, the old wardrobe and clear boarded walkways/opening without clipping, floating geometry or z-fighting; final normal-light readability remains M5 work

- [x] HOUSE-01018 — Equip `L3_STORE_E` with its services: header tank, ducts, aerial mast and cable runs
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L3 · adv: G3, D3 · est: 0.75
      note: (2026-09-24) Round 141 completes the east attic service recipe with seven static rows from the existing capped kit: a low scaled utility module reads as the header tank under the east eaves, two joined duct sections run along the high west side, two joined cable sections follow the measured east inner wall face, and a slender scaled utility module plus reused rail form the bounded internal aerial mast. All pieces share the existing steel role; only the tank keeps a proxy, while the thin wall/overhead services rely on the enclosing shell collision and preserve the full central route plus the north-store turn. No asset, material family, interaction or runtime system was added. All 425 production props pass exact support, overlap, opening, door-sweep and 0.70 m route validation; all 15 authored-world rules, the 90-cell grand tour and `world-content-current` pass. The wider inside-geometry probe retains exactly the nine pre-existing bodies recorded by `HOUSE-00847`, first `B1_HOBBY south`, with none in `Z-L3`. The rebuilt world measures 1,113 chunks / 2,993,877 vertices / 79,209,384 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk; `L3_STORE_E` is exactly 6/6 with no exception. Direct normal-light and targeted diagnostic views show grounded, separated services under the roof without clipping, floating geometry or z-fighting; final normal-light readability remains M5 work

### `Z-L0S` — ground-floor service rooms

- [x] HOUSE-00991 — Stock `L0_PANTRY`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0S · adv: G3, D3 · est: 0.5
      accept: shelving with the crockery-and-jars fill kit; `L0_BUTLERS` (in `Z-L0M`, already dressed by `HOUSE-01075`) is not reworked
      note: (2026-09-24) Round 129 stocks the utility-tier pantry with two repeated open shelves and one supported crockery-and-jars fill instance, all from the existing generated kit; `L0_BUTLERS` is unchanged and no asset, material family, clutter variant or runtime system was added. All 306 production props pass exact support, overlap, opening, door-sweep and 0.70 m route validation. The 90-cell grand tour, authored-world validator and `world-content-current` pass; the broader inside-geometry probe still reports only its pre-existing `L1_LANDING west` and `L2_LIBRARY north` locations (four bodies total). The build measures 985 chunks / 2,705,195 vertices / 69,996,424 packed bytes; `L0_PANTRY` is exactly 10/10 chunks, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk. Direct reciprocal clear-day renders show grounded shelves, the supported fill, clear openings and no clipping, floating geometry or z-fighting; the room's current dark normal-light balance remains M5 work

- [x] HOUSE-00994 — Furnish `L0_OFFICE` and `L0_CLOSET_W`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00977, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0S · adv: G3, D3 · est: 0.75
      note: (2026-09-24) Round 130 completes the secondary study recipe with the acquired writing desk, one reused dining chair, a generated bookcase and the acquired desk-lamp form, and gives the utility-tier west closet one physically scaled open shelf. The lamp uses the established painted BasicEffect role because its source has no UV1; no material, asset, interaction or runtime system was added. All 311 production props pass exact support, overlap, window-front, opening, door-sweep and 0.70 m route validation. A first centred bookcase and larger closet shelf created two new cardinal push-out pockets; moving both into clear corners returns the wider inside-geometry probe to only its four pre-existing bodies at `L1_LANDING west` and `L2_LIBRARY north`. The authored-world validator, 90-cell grand tour and `world-content-current` pass. The build measures 989 chunks / 2,716,860 vertices / 70,369,704 packed bytes; `L0_OFFICE` is exactly 14/14 and `L0_CLOSET_W` remains 6/6, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk. Fixed and reciprocal clear-day renders show a grounded desk/chair/storage composition, supported lamp and closet shelf, clear windows/openings and no clipping, floating geometry or z-fighting; final Basic-lit furniture and closet readability remain M5 work

- [x] HOUSE-00995 — Furnish `L0_MUDROOM` and `L0_LAUNDRY`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00975, HOUSE-00984, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0S · adv: G3, D3 · est: 0.75
      note: (2026-09-24) Round 135 completes the secondary mudroom and utility laundry recipes with six static rows from the existing capped kit. The mudroom has a scaled utility-module bench, supported open cubbies, tall wardrobe and bin around the existing garage door; the laundry has two material variants of the acquired static laundry body. Existing painted, wood and visible steel roles provide all finishes, so no asset, material family, appliance behaviour, interaction or runtime system was added. All 371 production props pass exact support, overlap, opening, door-sweep and 0.70 m route validation, and the authored-world validator plus 90-cell grand tour pass. The wider inside-geometry probe remains exactly at its four pre-existing bodies, first `L1_LANDING west`. The build measures 1,064 chunks / 2,898,207 vertices / 76,147,944 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk; both changed cells are exactly 10/10. Fixed and direct forced-light views show grounded, separated storage and two recognizable laundry bodies, clear doors and no clipping, floating geometry or z-fighting; final normal-light readability remains M5 work

- [x] HOUSE-00996 — Furnish `L0_WC1` and `L0_WC2`, and dress `L0_STOR` and `L0_STAIR_MAIN`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00973, HOUSE-00976, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0S · adv: G3, D3 · est: 0.75
      trace: absorbs `HOUSE-00997`
      note: (2026-09-24) Round 138 completes both secondary compact-WC recipes with the established toilet, vanity, mirror and towel rail, and the utility-tier hall store with one open shelf and one box. `L0_STAIR_MAIN` retains the bench, runner and wall art already supplied by `HOUSE-03343`; those pieces satisfy its landing recipe, so no optional polish was added. All ten new rows reuse existing capped assets and proxies; no asset, material family, behaviour or runtime system was added. All 397 production props pass exact support, overlap, opening, door-sweep and 0.70 m route validation. The authored-world validator, 90-cell grand tour and `world-content-current` pass. The wider inside-geometry probe remains exactly at its four pre-existing bodies, first `L1_LANDING west`. The build measures 1,094 chunks / 2,945,831 vertices / 77,671,912 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk; `L0_WC1`, `L0_WC2` and `L0_STOR` are each exactly 9/9. Fixed zone controls and direct forced-light views show grounded, separated fixtures/storage, clear openings and no clipping, floating geometry or z-fighting; final normal-light readability remains M5 work

### `Z-GAR` — garage

- [x] HOUSE-00998 — Furnish `L0_GARAGE` and `L0_GARAGE_LOFT`
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303, HOUSE-00984, HOUSE-00985, HOUSE-00847 · sys: world · plat: TOOL · pri: MUST · zone: Z-GAR · adv: G3, D3 · est: 1.5
      accept: the household car from `HOUSE-00847`'s car family, a workbench, shelving, tools, bikes or garden tools and bins in the garage; long-term storage (boxes, seasonal items, spare lumber, a garden-furniture stack) in the loft, clear of its access and guard. No containers and no door mechanism
      trace: absorbs `HOUSE-03341` (the loft)
      note: (2026-09-24) Round 140 completes both bounded recipes with sixteen static rows from the existing capped kit. The main bay keeps the household estate and adds a workbench with supported tools, one open shelf, a toolbox, long-handled garden tools and two bins. The utility loft adds four varied boxes, a seasonal suitcase, a flattened open-shelf abstraction for spare lumber and a three-piece stack of reused garden loungers; every piece stays clear of the hatch, ladder and perimeter guard. No asset, material family, interaction, container behaviour, door mechanism or runtime system was added. All 418 production props pass exact support, overlap, opening, door-sweep and 0.70 m route validation; all 15 authored-world rules, the 90-cell grand tour and `world-content-current` pass. The wider inside-geometry probe retains exactly the nine pre-existing bodies recorded by `HOUSE-00847`, first `B1_HOBBY south`, with none in `Z-GAR`. The rebuilt world measures 1,112 chunks / 2,986,205 vertices / 78,963,880 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk; `L0_GARAGE` is 16/16 and `L0_GARAGE_LOFT` 6/6. Fixed clear-day/forced-light views and targeted diagnostic views show grounded, separated workshop and storage compositions, a clear vehicle bay and unobstructed loft access without clipping, floating geometry or z-fighting; final normal-light readability remains M5 work

### `Z-STAIR`, `Z-EXR`, `Z-STR`, `Z-EXF`

- [x] HOUSE-03343 — Dress the stair halls and landings
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03302, HOUSE-03303 · sys: world · plat: TOOL · pri: MUST · zone: Z-STAIR · adv: G3, D3 · est: 0.75
      accept: C3 for the six stair cells: stair-wall frames where the recipe places them, a runner where the finish schedule has one, and landing pieces; nothing narrows a flight below its walkable width
      verify: `Z-STAIR` capture; stair traversal tests; grand tour
      note: (2026-09-24) Round 131 completes all six stair-cell recipes with six reused wall-art rows, two reused benches and three scaled landing runners. The two main flights now use the finish schedule's `stair_carpet` surface: the existing shell generator lays one continuous 760 mm wool runner over every tread and riser while retaining 170 mm exposed oak at each edge; the unchanged collision ramps preserve the full authored 1.10 m walkable width. No new asset, material family, runtime system or collision shape was added. All 322 props pass exact placement/support/opening/door-sweep/0.70 m route validation; both stair-traversal tests, all three stairwell tests, the authored-world validator, 90-cell grand tour and `world-content-current` pass. The wider inside-geometry probe retains only its four pre-existing bodies, first at `L1_LANDING west`. The build measures 1,016 chunks / 2,760,492 vertices / 71,765,928 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk; all six stair cells meet their exact declared ceilings. Nine fixed clear-day and forced-light `Z-STAIR` views plus direct landing views were inspected: the landing pieces are grounded, openings and flights remain clear, and the runner geometry is present without clipping; final normal day/night readability remains M5 work

- [x] HOUSE-03342 — Dress the shed, the side yards, the orchard corner and the vegetable garden
      dep: HOUSE-03280, HOUSE-00971, HOUSE-03303, HOUSE-00984, HOUSE-00985 · sys: world · plat: TOOL · pri: MUST · zone: Z-EXR · adv: G3, D3 · est: 1.5
      accept: C3 for `EXT_SHED`, `EXT_SIDEYARD_W`, `EXT_SIDEYARD_E`, `EXT_ORCHARD` and `EXT_GARDEN`: the shed's tools, mower, pots and bench; service-side objects (the AC condenser and meters if `HOUSE-00770` does not place them, a firewood store, a wheelbarrow, garden tools, a compost area, a potting bench and a rain barrel), placed with purpose and collision
      verify: `Z-EXR` capture; placement validator; grand tour
      trace: absorbs `HOUSE-01026` (the shed)
      note: (2026-09-24) Round 134 completes the five remaining rear/side exterior recipes with seventeen rows from the existing capped kit. The shed receives a workbench, supported toolbox/tools, shelf, long-handled tools, bin, pots and a compact mower abstraction; the garden receives a potting bench, tools and pots; the west service strip receives a rack and visible crossed-log basket; the east strip receives a rain-barrel abstraction; and the orchard receives a bounded low-detail garden-cart/wheelbarrow silhouette. The existing compost area remains the task's compost element, while the condenser and meters stay with `HOUSE-00770` as the acceptance permits. No bespoke asset, material family or runtime system was added. All 365 props pass exact placement/support/opening/door-sweep/0.70 m route validation; rule 14 and the snow-shell builder now both resolve existing per-axis prop scale through the shared transform parser, with focused selftests. The authored-world validator, 90-cell grand tour and `world-content-current` pass; the wider inside-geometry probe retains exactly its four pre-existing bodies, first `L1_LANDING west`. The build measures 1,059 chunks / 2,874,783 vertices / 75,398,376 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk. Seven fixed clear-day and forced-shed-light `Z-EXR` views show a grounded, separated service composition and unchanged broad rear views without clipping, floating geometry or z-fighting; final normal day/night readability remains M5 work

- [x] HOUSE-00770 — Place the front exterior props: mailbox, three bins, hose reel, AC condenser, gas meter, water tap, downspout splash blocks
      dep: HOUSE-00764, HOUSE-03280, HOUSE-03303 · sys: world · plat: TOOL · pri: MUST · zone: Z-EXF · adv: G3, D3 · est: 0.75
      accept: all static props with collision proxies
      note: (2026-09-24) Round 142 completes the bounded exterior-service recipe with fifteen static rows from the existing capped kit: a two-piece post mailbox beside the pedestrian gate; three grouped full-height bins; a wall-mounted hose holder and tap abstraction; compact condenser and gas-meter cabinets; and one low splash block at each of the six authored downspouts. Every row has a collision proxy, and the established bronze/service-metal roles keep every affected exterior cell inside its existing measured chunk exception. No asset, material family, behaviour or runtime system was added. All 440 props pass exact placement/support/overlap/opening/door-sweep/0.70 m route validation; all 15 world rules, the 90-cell grand tour, material binding and `world-content-current` pass. The wider inside-geometry probe retains exactly its nine pre-existing bodies, first `B1_HOBBY south`. The rebuilt world measures 1,114 chunks / 3,010,601 vertices / 79,744,552 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk. Four fixed `Z-EXF` views and five close service views show grounded, separated pieces with clear routes and no clipping, floating geometry or z-fighting

- [x] HOUSE-00847 — One low-detail car family, placed: the household car, three parked neighbour cars and a delivery van
      dep: HOUSE-00293, HOUSE-03280, HOUSE-03303 · sys: world · plat: TOOL · pri: MUST · zone: Z-STR, Z-GAR · adv: G3, D3 · est: 1
      accept: one car family (an estate and a van body), cleanly licensed or generated, varied by tint; static props with collision proxies; readable at street distance, and in the garage at C3
      trace: absorbs the cancelled `HOUSE-01035`/`01036` (the household car)
      note: (2026-09-24) Round 139 adds one project-authored deterministic low-detail vehicle family: a 248-triangle estate body reused under blue, red and silver paint tints, and a 212-triangle boxy white delivery van sharing its wheels, glazing, lamps and construction. All four committed GLBs are byte-reproducible, clean under the glTF validator and carry one enclosing 12-triangle `_COL` proxy. One blue estate anchors the garage and the three street estates plus van retain the four permanent `NB_*` placement ids. The street placements were moved from the not-yet-rendered neighbourhood instance array into the existing static-prop/chunk path, so this MUST task has no hidden dependency on the optional neighbourhood renderer and adds no runtime system. All 402 props pass exact placement/support/opening/door-sweep/route validation; all 15 authored-world rules, the 90-cell grand tour, collision/neighbourhood round trips, row resolution and direct deployment comparison pass. The rebuilt world measures 1,110 chunks / 2,948,439 vertices / 77,755,368 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk. Fixed garage and street views plus direct runtime views of all four variants show grounded, separated, recognizable silhouettes without clipping, floating geometry or z-fighting. The wider inside-geometry probe currently reports nine unrelated previously placed room props (first `B1_HOBBY south`) after the full content refresh; none is in `Z-GAR` or `Z-STR`, and the grand tour remains clear

### Checkpoint

- [x] HOUSE-03380 — **Dressing checkpoint: every accessible room has its tier's essential props**
      dep: HOUSE-00991, HOUSE-00994, HOUSE-00995, HOUSE-00996, HOUSE-00998, HOUSE-00999, HOUSE-01000, HOUSE-01001, HOUSE-01002, HOUSE-01003, HOUSE-01006, HOUSE-01007, HOUSE-01009, HOUSE-01010, HOUSE-01011, HOUSE-01013, HOUSE-01014, HOUSE-01015, HOUSE-01016, HOUSE-01017, HOUSE-01018, HOUSE-01020, HOUSE-01021, HOUSE-01023, HOUSE-01024, HOUSE-01025, HOUSE-00770, HOUSE-00847, HOUSE-03342, HOUSE-03343 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G3 · est: 0.75
      accept: `zone_scoreboard.py` shows zero-prop accessible rooms = 0 (except named empty-by-design spaces); no S1/S2 dressing defect open; each zone's own views captured once by day. This is the point the house-wide lighting work of M5 waits for; it is **not** a gate
      verify: `zone_scoreboard.py`; one capture per zone
      note: (2026-09-24) Round 143 closes M4 after inspecting all 68 fixed clear-day views across every zone. The scoreboard assigns all 96 cells once and reports zero zero-prop accessible rooms in all eleven zones; no S1/S2 dressing defect remains. Dark secondary interiors are the already-scheduled M5 lighting work. The complete C3 content measures 1,114 chunks, 50 alpha-test batches, 131 opaque-state changes and 220 authored materials. Tests now pin that finished-house baseline rather than the earlier partial-dressing snapshots; deliberate boundary teleports into nine wall/furniture gaps are named separately while the 90-cell grand tour, 0.70 m authored routes, 1,420 unit tests and focused integration retry remain green. Captures remain local under `docs/visual-review/captures/house-03380-dressed-checkpoint-r143/`

---

## M5 — Baseline lighting everywhere (the lighting half of C3) · gate G3

Track A. Brings every accessible room's lighting up to the C3 floor and ends with **gate G3**: the
whole house is dressed and lit at its baseline. The dressing is in place (checkpoint
`HOUSE-03380`), so the object-versus-bake mismatch is fixed **once, for the whole house**
(`HOUSE-03402`), the house is re-baked with its furniture, and each zone is then lit **by room type
from the recipes' lighting presets** (`HOUSE-03302`), not hand-tuned room by room. Hand-tuning is
spent only where a preset leaves an S1/S2. Secondary and utility spaces need convincing functional
light, nothing more. No new lighting system is added for polish (rule R9).

- [x] HOUSE-03401 — Automatic interior lighting schedule ("an occupied house")
      dep: HOUSE-03201 · sys: lighting · plat: ALL · pri: MUST · zone: all · adv: D6 · est: 2
      files: src/lighting/LightingSystem.cpp|hpp (or a small `LightSchedule`), assets-src/world/layout.lights.json, tests
      accept: (1) a data-driven schedule per light group, keyed to the sun and clock: living spaces on from dusk until a late hour; bedroom lamps in the evening; circulation lights on at night; bathrooms, closets and storage off unless the schedule names them; (2) seeded, deterministic per-group offsets so windows do not switch in unison; (3) `--light-on`/`--light-off` still override; (4) the review capture at night uses the schedule, not hand-picked flags; (5) no switch plate, prompt or interaction code
      verify: unit LightScheduleTests.* (times, determinism, override, and one light group per level switching and changing its room's level)
      note: until now interior lights change only through `--light-on`. A night walkthrough is dark indoors except for dusk-sensor fixtures
      note: (2026-09-24) All 138 authored light groups now have exactly one data-driven class: 29 off, 18 living, 16 bedroom, 18 wet, 18 task, 26 circulation and 13 dusk. The existing `LightingSystem` evaluates those classes from the shared sun and civil clock with a stable group-id offset in [-8,+8] minutes; the existing per-fixture dusk staggering remains intact and CLI on/off flags become persistent overrides. Schema, C++ and Python validation reject missing, duplicate and unknown schedule groups. Four focused test cases cover the exact windows, determinism, B1/L0/L1/L2/L3 room output and override persistence. Round 146 captured all six fixed `Z-L1` views at 22:00 without a light flag and visibly shows the scheduled hall, landing, bedroom, bathroom and balcony groups; it is schedule evidence, not a premature C3 luminance claim. Full evidence is in `docs/visual-review/house-03401-light-schedule.md`

- [x] HOUSE-03402 — Light static props consistently with their baked receivers, house-wide
      dep: HOUSE-03380 · sys: rendering · plat: ALL · pri: MUST · zone: all · adv: D6, G3 · est: 4
      files: src/rendering/*, src/lighting/*, tools/blender/lightmap_bake.py (as chosen)
      accept: (1) choose, by matched captures, between props receiving per-cell irradiance from the bake (probe or sampled lightmap), baked prop lightmaps, or baked vertex occlusion, and record the decision with its numbers; (2) Basic-lit furniture no longer reads markedly darker or warmer than the floor and wall it stands against: measured luminance ratios within a stated band on ≥ 1 view per zone, day and night; (3) the rejected global-gain probes of `HOUSE-01076` stay rejected; outdoor props keep their calibration; (4) the cheapest option that meets (2) wins, and nothing beyond it is built
      verify: matched before/after captures of ≥ 1 view per zone; the ratio measurement; render goldens refreshed only where inspected
      note: generalises Round 102's top ground-floor defect (piano, sofas, sunroom chairs) to the whole house. With every zone dressed, this is a system fix, not a room fix (R1 exception b)
      note: (2026-09-24) Round 144 chooses the cheapest accepted option: each existing daylight/artificial bake now records its non-padding linear receiver mean (216 scalar bindings), and indoor Basic-lit detail composes the owning cell's daylight tint plus active owned fixture groups. The established heuristic remains the floor and a measured 1.40x component-wise cap prevents a cell average from overpowering local UV2 shade; no UV1, prop atlas, vertex stream, draw, manager or global gain was added. Eight indoor fixed views measured by day/night finish inside the stated 0.25–3.50 linear-luminance ratio band (0.283–3.369); three exterior-zone controls are pixel-identical before/after. The rejected uncapped candidate reached 10.834 in the master bedroom and 37.680 in the library and was discarded. Full numbers and ROIs are in `docs/visual-review/house-03402-prop-lighting.md`. Matched captures show no clipping, new warm cast or outdoor change; no render golden failed or required replacement. The 1,420 unit tests, 140 integration tests, schema validation, authored-world validation and focused static-pass tests pass

- [x] HOUSE-01030 — Rebuild the chunks and re-bake the lightmaps over the furnished house
      dep: HOUSE-03380, HOUSE-00473, HOUSE-00909 · sys: content · plat: TOOL · pri: MUST · zone: all · adv: G3, D6 · est: 4
      accept: (1) the chunks are rebuilt over the furnished house and the ≤ 6-chunks-per-cell target still holds, or each exception is measured and recorded; (2) the lightmaps are re-baked so furniture casts and receives baked light; (3) S1/S2 lightmap seams and gutter bleed found on inspection are fixed on this bake; smaller seams are S3/S4
      verify: the chunk budget check; one capture per zone by day; the bake log
      trace: absorbs `HOUSE-01029` (chunk rebuild) and `HOUSE-00913` (seams)
      note: (2026-09-24) Round 145 rebuilds the final furnished world at 1,114 chunks / 3,010,601 vertices / 79,744,552 packed vertex bytes. All 79 cells above the six-chunk target exactly match their measured existing exceptions; there are 8 16-bit-cap splits, 15 Reach-cap splits and no 32-bit chunk. The existing deterministic baker now imports only the 252 solid static `collision: proxy` props in the 78 receiver cells as caster-only geometry (2,585 visible mesh objects), excluding `_COL`, lower LOD and thin dressing nodes. It records a per-cell hash of the exact prop rows and GLB bytes so resume cannot reuse a stale furnished bake. The 256-sample full runs produced 78 daylight and 138 artificial atlases in 206.455 s and 477.534 s; every receiver mean is positive, both families bind the same 78 current shell/prop signatures, and a full resume reuses all 78 cells. The refreshed receiver means continue to light furniture through `HOUSE-03402` while the imported solid furniture casts into the shell atlases. Round 145 inspected one fixed clear-day view per zone against Round 144: interior changes are the expected furnishing/contact-shadow response, exterior controls are unchanged apart from HUD timing, and there is no black atlas, S1/S2 seam, gutter bleed, clipping or z-fighting. The unwrap report covers 78 cells/atlases with no problem or attention row. Full evidence is in `docs/visual-review/house-01030-furnished-rebake.md`; the build, schema/world/manifest/licence/budget/deployment gates, 1,420 unit tests and 49 runnable render tests pass. Integration has 140/141 passes under load; the sole weather-clock timing miss passes on its exact focused retry and is unrelated to the bake

- [x] HOUSE-03403 — Light the basement, the attic and the stairwells to the C3 baseline
      dep: HOUSE-03402, HOUSE-01030, HOUSE-03401 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-B1, Z-L3, Z-STAIR · adv: G3, D6 · est: 2
      accept: every room and flight in `Z-B1`, `Z-L3` and `Z-STAIR` has physical fixtures on its authored groups, set by its recipe's preset; windowless rooms are readable under their scheduled lights and never black; the attic reads by dormer daylight plus bare-bulb fixtures; the main stair foot's S2 darkness (Rounds 98, 102) is closed
      verify: zone captures day and night (utility rooms by day only); lighting tests
      note: (2026-09-24) Round 147 closes the first C3 lighting band with 44 physical fixture props backing all 46 authored sources across the exact 25 cells of `Z-B1`, `Z-L3` and `Z-STAIR`. Four existing fixture families, the existing schedule, bake path and prop path are reused; no runtime code, exposure constant, material family, asset or subsystem was added. The basement hall's three sources now span its length, scheduled working spaces are readable without a global lift, attic stores read by dormer daylight and retain their intentional `SC-OFF` night state, and the finished attic room plus all eight flights are readable under their scheduled groups. Night mean linear RGB rises 1.37–6.46x in the measured basement views, 5.11x in the attic room and 1.25–3.31x at the measured stair transitions; the main-stair-foot S2 is closed. The targeted 256-sample promotion refreshed 25 daylight and 37 artificial products. The 1,209-chunk / 3,032,941-vertex / 80,459,432-byte world retains 8 16-bit splits, 15 Reach splits and zero 32-bit chunks; every exception exactly matches its measured ceiling. Schema/world/scoreboard/chunk gates, 39 focused lighting tests and all 1,424 unit tests pass. Integration is 140/141 under load only because the known weather-clock threshold misses at 137.688 versus 138; its exact retry plus `world-content-current` passes 2/2. Strict XNA compiles all 325 translation units clean with six workers; the complete static gate exits nonzero only for the known user-owned root `.claude` layout entry. Full visual and numeric evidence is in `docs/visual-review/house-03403-basement-attic-stairs-lighting.md`

- [x] HOUSE-03404 — Light the two upper floors to the C3 baseline
      dep: HOUSE-03402, HOUSE-01030, HOUSE-03401 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-L1, Z-L2 · adv: G3, D6 · est: 2
      accept: every room in `Z-L1` and `Z-L2` has physical fixtures (ceiling lights, bedside and desk lamps from the kit) on its groups, set by its recipe's preset; daylight is readable at 10:30 in every room; the evening schedule is readable at night
      verify: zone captures day and night
      note: (2026-09-24) Round 148 brings all 35 accessible `Z-L1`/`Z-L2` cells to C3. One hundred ten new static fixture rows plus the two already-present linked practicals back all 112 authored sources, with every source naming its physical prop and exact switched emissive slot. Seven existing fixture/lamp families, the existing schedule, bake path and prop path are reused; no runtime system, asset acquisition, exposure constant or material family was added. Desk, bedside and reading lamps sit on or beside the existing recipe furniture; ceiling fittings remain separated and inside their cells. The targeted 256-sample work produced final current products for 33 receiver cells, 33 daylight atlases and 66 artificial group atlases; the two balcony cells use their existing dynamic exterior treatment. Fixed day/night captures show readable rooms, visible practicals and no clipping, floating fixture, z-fighting, missing texture or blocked route. Across six fixed views per zone, night changes cover 3,463,305 pixels on L1 and 4,148,483 on L2 while day mean linear RGB stays within 0.03% of its baseline. The rebuilt 594-prop world is 1,345 chunks / 3,110,604 vertices / 82,944,648 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk; all 86 exceptions exactly match their measured ceilings. All fifteen world rules, exact prop placement, fixture/manifest/licence checks, the focused fixture regression and all 1,424 unit tests pass; `world-content-current` plus the 90-cell grand tour pass 2/2. Strict XNA compiles all 325 translation units clean with six workers; the complete static gate exits nonzero only for the known user-owned root `.claude` layout entry. Full evidence is in `docs/visual-review/house-03404-upper-floor-lighting.md`

- [x] HOUSE-03405 — Light the ground-floor service rooms and the garage to the C3 baseline
      dep: HOUSE-03402, HOUSE-01030, HOUSE-03401 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-L0S, Z-GAR · adv: G3, D6 · est: 1.25
      accept: `Z-L0S` and `Z-GAR` lit by their recipes' presets, including the service rooms' night depth from Round 102 and the garage's fluorescent utility character
      verify: zone captures day and night
      note: (2026-09-24) Round 149 brings both zones to C3. Seventeen new static fixture rows plus the office's existing desk lamp back all 18 authored sources across the eight service cells, garage and visible loft; every source names its physical prop and exact switched emissive slot. The pass reuses the family ceiling, utility batten, task-puck and desk-lamp families, existing schedule, lightmap baker and prop path, with no runtime system, asset acquisition, exposure constant or material family. Targeted 256-sample products cover 10 receiver cells, 10 daylight atlases and 13 artificial group atlases; four artificial PNGs change where fixture position or physical desk-lamp material linkage changes. Fixed day/night captures show the office's scheduled task depth, deliberate day-readable `SC-OFF` utility rooms and the garage's scheduled fluorescent character. Night changes cover 876,638 pixels in `Z-L0S` and 581,985 in `Z-GAR`; full-resolution inspection found no clipping, floating fixture, z-fighting, missing texture or blocked route. The rebuilt 611-prop world is 1,380 chunks / 3,119,436 vertices / 83,227,272 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk; all 88 exceptions exactly match their measured ceilings. All fifteen world rules, exact prop placement, fixture/manifest/licence checks, the focused fixture regression and all 1,424 unit tests pass; `world-content-current` plus the 90-cell grand tour pass 2/2. Full evidence is in `docs/visual-review/house-03405-ground-service-garage-lighting.md`

- [x] HOUSE-03406 — Light the rear and side exterior at night to the C3 baseline
      dep: HOUSE-03380 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-EXR, Z-EXF · adv: G3, D6 · est: 1.25
      accept: the rear garden, side yards and shed read at night by lanterns, the dusk-sensor fixtures and window spill, without a lifted global exposure; the front is re-checked, not re-worked
      verify: `Z-EXR` and `Z-EXF` night captures
      note: (2026-09-24) Round 150 brings `Z-EXR` to C3 using only existing lighting mechanisms. The terrace lanterns, driveway-edge dusk fixtures and scheduled room windows already provide the broad rear/side cues; the sole bare source was the shed. One reused utility batten now backs `LIGHT_EXT_SHED_MAIN_1` with its exact neutral emitter, and its unchanged 3.30 m source explicitly spills only to `EXT_GARDEN` as the narrow east-window pool. No intensity, schedule, exposure, runtime path, material family or acquired asset changed. Seven fixed rear poses, four unchanged front controls and direct utility-side-yard views were inspected at 22:00 without overrides. `Z-EXR` mean linear RGB moves 0.007644→0.008158; the intended local changes cover 71,654 shed-interior and 122,464 shed-facade pixels. The front controls change only 2,433/5,760,000 HUD/timing pixels and were not re-worked. The rebuilt 612-prop world is 1,384 chunks / 3,119,926 vertices / 83,242,952 packed bytes, with 8 16-bit splits, 15 Reach splits and no 32-bit chunk; all 88 exceptions exactly match their ceilings. All fifteen world rules, exact prop placement, fixture/id/chunk gates, the focused physical-source regression and all 1,424 unit tests pass; `world-content-current` plus the 90-cell grand tour pass 2/2. All 325 strict-XNA translation units pass with four workers; the complete static suite exits nonzero only for the known user-owned root `.claude` layout entry. Full evidence is in `docs/visual-review/house-03406-rear-side-exterior-lighting.md`

- [x] HOUSE-03407 — Close the ground-floor route's lighting S2 list
      dep: HOUSE-03402, HOUSE-01030, HOUSE-03401, HOUSE-03403, HOUSE-03404, HOUSE-03405 · sys: lighting · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G3 · est: 1.25
      accept: Round 102's ranked S2 list is closed on the fixed route views by day and night under the schedule; no new ground-floor-only lighting constant is introduced unless the house-wide solution fails there, and then it is recorded
      verify: `Z-L0M` captures day and night
      note: deliberately **last** in M5. It is the only permitted `Z-L0M` work before G3 (rule R4)
      note: (2026-09-24) Round 151 closes all three ranked Round 102 findings without a ground-floor-only constant or new implementation. `HOUSE-03402`'s bounded per-cell receiver sample already brings the measured `Z-L0M` furniture/receiver ratios into its 0.25–3.50 acceptance band (family-media 0.334 day / 0.283 night), `HOUSE-03403` closes the main-stair-foot darkness with physical scheduled fixtures, and `HOUSE-03405` closes service-room night depth. Fresh 10:30 clear and 22:00 scheduled captures cover all 23 fixed `Z-L0M` poses without light overrides. Full-resolution inspection shows the piano body, keys, score and physical picture light; separates the living/family furniture from adjacent receivers; and keeps foyer, hall, kitchen, dining, butler's pantry and sunroom readable with no clipping, floating fixtures, z-fighting, missing texture or blocked route. Cropping the HUD, aggregate mean linear luminance is 0.043056 by day and 0.024087 at night; the darkest individual frames remain non-black at 0.013928 and 0.008296. No source, schedule, exposure, renderer, material, asset or lightmap changed. Full evidence is in `docs/visual-review/house-03407-ground-route-lighting.md`

- [x] HOUSE-01275 — Representative interior render set: one pose per zone and per hero area, by day and at night
      dep: HOUSE-03403, HOUSE-03404, HOUSE-03405, HOUSE-03406, HOUSE-03407 · sys: ci · plat: CI · pri: MUST · zone: all · adv: G3, D11 · est: 1
      accept: the existing review poses reused as render tests: one per zone plus one per hero area, at 10:30 clear and at 22:00 under the schedule (about 32 scenes), within the render-test tolerance; no lights-on/lights-off matrix
      verify: the render suite
      trace: was *20 rooms, lights on and off, at noon and at midnight*; absorbs `HOUSE-00917`, `HOUSE-01032`, `HOUSE-02590` and `HOUSE-02592`
      note: (2026-09-24) The versioned Tier-S/High Mesa set contains exactly 32 640x360 frames: one unchanged review pose for all eleven zones and all five retained hero areas at clear 10:30 and scheduled 22:00, with no light override. A coverage test pins those exact sets; a fresh software-renderer run passes all 32 comparisons with the existing per-channel tolerance 2 and <0.2% differing-pixel limit. Full-resolution inspection accepts both contact sheets and replaces the initially near-blank basement-stair candidate with the existing `main-stair-foot` pose. Capture frame two is measured: frame one retains CNA's initial sampler state, while frame three made the darkest cinema frame vary by 15.55–16.38% between processes; two independent frame-two cinema renders were bit-identical. No tolerance was widened and no runtime/content/asset changed. Full evidence is in `docs/visual-review/house-01275-representative-render-set.md`

- [x] HOUSE-03420 — **Gate G3 review: every accessible room at C3 (dressed and lit baseline)**
      dep: HOUSE-03402, HOUSE-03403, HOUSE-03404, HOUSE-03405, HOUSE-03406, HOUSE-03407, HOUSE-01030, HOUSE-01275 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G3, D3, D6 · est: 1
      accept: one all-zone round, day and night (utility rooms by day only), with no S1/S2 open in any accessible room; `zone_scoreboard.py` shows zero-prop accessible rooms = 0; the scoreboard shows every zone's C3 floor reached; the handoff records the gate
      verify: ledger round; scoreboard; `zone_scoreboard.py`
      note: (2026-09-24) Round 153 passes G3. Two fresh 68-view Tier-S/High sets cover all eleven zones at clear 10:30 and scheduled 22:00 without light overrides; every one of the 22 accessible utility cells was additionally inspected in four cardinal clear-day views, with eight targeted look-up/content views for the darkest service spaces. Full-resolution inspection finds no S1/S2, clipping, floating prop/fixture, z-fighting, missing texture, blocked route or impossible placement. The deliberately restrained `LP-SERVICE`/`SC-OFF` rooms remain non-black and expose their essential equipment under their required day-only utility review, so the approved schedule was not expanded. `zone_scoreboard.py --check` assigns all 96 cells exactly once with zero zero-prop accessible rooms, and the controller grand tour reaches all 90 accessible cells through 558 stops in 11.46 s. The local captures are `house-03420-g3-{day,night,utility-day}-r153`; full evidence is in `docs/visual-review/house-03420-g3-review.md`

---

## M6 — Main cells and hero areas (C4, C5) · gates G4 and G5

Track A, after G3. **M6a** raises the twelve main cells to C4 and ends with gate G4. **M6b** raises
the five hero areas to C5 and ends with gate G5. Nothing is raised above its tier's target. The
ground floor goes last at each stage (rule R4), and the dependencies encode it. A C4 task completes
its cells' main-tier recipe (textiles, wall items, shelf contents and curtains where the recipe has
them) and balances their light by day and at night; a C5 task adds secondary dressing and signs of
habitation, checks the overcast view, and closes S3 only where cheap. **Both are bounded by their
estimates (rule R7); neither is a campaign.**

### M6a — main cells to C4 · gate G4

**Acceptance of every C4 task below:** the named cells meet C4 (see [Completion levels](#completion-levels)):
the recipe's main-tier set complete, lighting balanced by day and at night, and their fixed views,
day and night, free of S1/S2. **Verify:** those views, day and night.

- [x] HOUSE-03441 — Bring the basement workshop to C4
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G4, D4 · est: 0.75
      accept: `B1_WORKSHOP`
      trace: `B1_HALL` became secondary (`HOUSE-03206`)
      note: (2026-09-25) Round 156 confirms C4 without adding optional clutter. `HOUSE-01024` already completed the bounded `R-WORKSHOP` main-tier set with two workbenches, open storage, supported toolbox/tools/paint tin, a service run and floor bin; `HOUSE-03403` already supplied two physical utility fittings and the task puck on the existing schedule. Fresh four-view `Z-B1` sets at clear 10:30 and scheduled 22:00 include the fixed workshop view without light overrides. Full-resolution inspection finds a recognizable, grounded composition with clear openings and no S1/S2, clipping, floating furnishing, z-fighting or missing texture. HUD-cropped mean linear luminance is 0.010331 by day and 0.073821 at night. All 612 props pass placement/route validation, the scoreboard assigns all 96 cells exactly once and the 90-cell controller grand tour passes with 558 stops, 90,469 steps and 27 detours in 10.49 s. No runtime, content, asset, material, light, schedule or lightmap changed. Captures remain local under `house-03441-workshop-{day,night}-r156`

- [x] HOUSE-03442 — Bring the garage to C4
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-GAR · adv: G4, D4 · est: 1
      accept: `L0_GARAGE`
      trace: absorbs the cancelled `HOUSE-03443`; `L0_OFFICE` became secondary (`HOUSE-03206`)
      note: (2026-09-25) Round 159 confirms C4 without adding optional garage clutter. `HOUSE-00998` already completed the bounded main-tier composition with the household estate, workbench and supported tools, open shelf, toolbox, garden tools and two bins while keeping the vehicle bay and loft ladder clear; the visible loft retains its long-term storage composition. `HOUSE-03405` already supplied four physical utility fittings and the opener puck on the normal schedule. Fresh two-view `Z-GAR` sets at clear 10:30 and scheduled 22:00 show the complete bay, open sectional leaf and loft access without light overrides. Full-resolution inspection finds no S1/S2, clipping, floating/intersecting furnishing, z-fighting, missing texture or blocked route. All 613 props pass placement/route validation, the scoreboard assigns all 96 cells exactly once and the 90-cell controller grand tour passes with 558 stops, 90,469 steps and 27 detours in 10.35 s. No runtime, content, asset, material, light, schedule, chunk or lightmap changed. Captures remain local under `house-03442-garage-{day,night}-r159`

- [x] HOUSE-03444 — Bring the master bedroom to C4
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-L1 · adv: G4, D4 · est: 1.5
      accept: `L1_MASTER_BED`, with bedding, curtains, a rug, wall art and bedside lamps
      trace: absorbs `HOUSE-03453` (its C5 pass: the master bedroom is no longer hero area H4); `L1_LANDING`, `L1_HALL` and `L1_MASTER_BATH` became secondary (`HOUSE-03206`)
      note: (2026-09-25) Round 158 completes the bounded C4 recipe with one missing wall item: a single reused `MODEL_DECOR_WALL_ART_14` from the capped family, centred above the existing double bed. The existing bed supplies its authored bedding, while the paired nightstands and physical bedside lamps, south curtain, rug, wardrobe, dresser, desk and sitting corner remain unchanged. Fresh six-view `Z-L1` sets at clear 10:30 and scheduled 22:00 show the grounded composition and balanced fixture hierarchy without light overrides; full-resolution inspection finds no S1/S2, clipping, floating furnishing, z-fighting, missing texture or blocked route. All 613 props pass exact placement/route validation, all fifteen world rules pass, the inside-geometry route passes and the 90-cell controller grand tour reaches 558 stops through 90,469 steps and 27 detours in 10.81 s. The rebuilt master-bedroom cell is exactly 29 chunks: the art's three truthful frame, paper and image roles add no vertex-cap or Reach split. Captures remain local under `house-03444-master-{before,after}-{day,night}-r158`

- [x] HOUSE-03446 — Bring the attic room to C4
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-L3 · adv: G4, D4 · est: 1.25
      accept: `L3_ROOM`, reading as a lived-in attic room under dormer daylight and its evening lights
      trace: was the C5 pass of hero area H7, retired by `HOUSE-03206`
      note: (2026-09-25) Round 157 confirms C4 without restoring the room's retired hero depth. `HOUSE-01016` already completed the bounded `R-ATTIC-ROOM` main-tier set: reused sofa/side table, desk/chair/bookcase, rug, storage chest and box, a small storage/books cluster and the open double-dormer curtain. `HOUSE-03403` already supplied two physical ceiling fixtures and the desk practical on the existing evening schedule. Fresh four-view `Z-L3` sets at clear 10:30 and scheduled 22:00 include the fixed room view without light overrides. Full-resolution inspection shows the lived-in work/sitting/storage composition under dormer daylight and warm evening light, with clear store openings and no S1/S2, clipping, floating furnishing, z-fighting or missing texture. HUD-cropped mean linear luminance is 0.010101 by day and 0.022133 at night. All 612 props pass placement/route validation, the scoreboard assigns all 96 cells exactly once and the 90-cell controller grand tour passes with 558 stops, 90,469 steps and 27 detours in 10.32 s. No runtime, content, asset, material, light, schedule or lightmap changed. Captures remain local under `house-03446-attic-room-{day,night}-r157`

- [x] HOUSE-03447 — Bring the main stair to C4
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-STAIR · adv: G4, D4 · est: 0.75
      accept: `L0_STAIR_MAIN`, `L1_STAIR_MAIN` and `L2_STAIR_MAIN`, seen from every foot and head
      note: (2026-09-24) Round 154 confirms C4 without adding optional density. `HOUSE-03343` already completed the bounded `R-STAIR-LANDING` main-tier treatment: a continuous 760 mm wool runner on both principal flights, scaled landing runners, wall art in every stair cell and a bench where the broad L0 landing permits one; narrower landings remain clear as the recipe requires. `HOUSE-03403` already supplied physical scheduled fixtures and readable transitions. Fresh fixed `Z-STAIR` sets inspect all nine foot/head controls at clear 10:30 and scheduled 22:00 with no light override. Full-resolution review finds the runner, joinery, landing pieces and wall art grounded and readable, with no S1/S2, clipping, z-fighting, missing texture or narrowed route. The prop validator, both stair-traversal tests, all three stairwell tests and the 90-cell controller grand tour pass; the latter records 558 stops, 90,469 steps and 27 collision detours in 12.60 s. No runtime, content, asset, material, light, schedule or lightmap changed. Captures remain local under `house-03447-main-stair-{day,night}-r154`

- [x] HOUSE-03448 — Bring the rear terrace and back garden to C4
      dep: HOUSE-03420 · sys: world · plat: TOOL · pri: MUST · zone: Z-EXR · adv: G4, D4 · est: 1.25
      accept: `EXT_TERRACE` and `EXT_BACKYARD`, including the planting along the fences seen from them
      note: (2026-09-24) Round 155 confirms C4 without adding optional variety. The existing bounded recipes already give `EXT_TERRACE` its four-seat dining group, two loungers and four planters, and `EXT_BACKYARD` its swing bench, cold fire pit and birdbath; the authored shrub bands layer both compositions against the fences. Fresh seven-view `Z-EXR` sets at clear 10:30 and scheduled 22:00 show the furniture, routes, planting and rear elevation without light overrides. Full-resolution inspection finds no S1/S2, floating/intersecting furnishing, z-fighting, missing texture or blocked route. Night remains deliberately exterior-dark rather than globally lifted, while the path, primary silhouettes, fence planting, terrace lanterns and scheduled windows remain readable; HUD-cropped mean linear luminance is 0.004025 from the terrace and 0.005336 toward the house. All 612 props pass placement/route validation, the scoreboard assigns all 96 cells exactly once and the 90-cell controller grand tour passes with 558 stops, 90,469 steps and 27 detours in 11.84 s. No runtime, content, asset, material, light, schedule or lightmap changed. Captures remain local under `house-03448-rear-main-{day,night}-r155`

- [x] HOUSE-00989 — Bring the family room, the dining room and the sunroom to C4
      dep: HOUSE-03441, HOUSE-03442, HOUSE-03444, HOUSE-03446, HOUSE-03447, HOUSE-03448 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G4, D4 · est: 1.5
      accept: `L0_FAMILY`, `L0_DINING` and `L0_SUNROOM`, which are already densely dressed (`HOUSE-01037`–`01076`): complete what their main-tier recipe still lacks and close their S1/S2; the television is a static screen and the dog bed a static prop
      trace: absorbs `HOUSE-00992` (family) and `HOUSE-00993` (sunroom); was *Furnish `L0_DINING`*, then C5, then C4 (`HOUSE-03206`). Rule R4: after every other zone's C4 task
      note: (2026-09-25) Round 160 confirms all three deliberately-last ground-floor main cells at C4 without adding optional decoration. Their retained `HOUSE-01037`–`01076` compositions exactly match the bounded recipes: family seating/media, bookcase/side table, rug/lamp/plant/art and paired curtains; dining table/eight chairs, sideboard/practical lamps, rug and eight-place dressing; sunroom breakfast, lounge and wet-bar groups, plants and physical task/ceiling fixtures. The television remains a static screen and the dog bed static dressing. Fresh 23-view `Z-L0M` sets at clear 10:30 and scheduled 22:00 cover all eighteen fixed views through the three cells plus their route thresholds without light overrides. Full-resolution inspection finds deliberate, readable day/night compositions and no S1/S2, clipping, floating/intersecting furnishing, z-fighting, missing texture or blocked route. All 613 props pass placement/route validation, the scoreboard assigns all 96 cells exactly once and the 90-cell controller grand tour passes with 558 stops, 90,469 steps and 27 detours in 10.43 s. No runtime, content, asset, material, light, schedule, chunk or lightmap changed. Captures remain local under `house-00989-ground-main-{day,night}-r160`

- [x] HOUSE-03452 — **Gate G4 review: every main cell at C4**
      dep: HOUSE-03441, HOUSE-03442, HOUSE-03444, HOUSE-03446, HOUSE-03447, HOUSE-03448, HOUSE-00989 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G4, D4 · est: 0.75
      accept: one round over the main-tier views, day and night, with no S1/S2 open in any main cell; the scoreboard's *Main at C4* column is complete in every zone; every hero area is at least C3; the handoff records the gate
      verify: ledger round; scoreboard
      note: (2026-09-25) Gate G4 passes. Round 161 captured all 68 fixed views at clear 10:30 and scheduled 22:00 without light overrides, then reviewed the seven zone sheets containing the twelve main cells and all twelve representative main-cell views individually at full resolution in both conditions. The complete C4 compositions remain readable with clear routes and no S1/S2, clipping, floating/intersecting furnishing, z-fighting or missing texture. The scoreboard assigns all 96 cells exactly once, its *Main at C4* column is complete in every zone, all eleven hero cells retain the C3 baseline proved at G3 and the fixed-view checker still covers all five hero areas. No runtime, content, asset, material, light, schedule, chunk or lightmap changed. Captures remain local under `house-03452-main-gate-{day,night}-r161`

### M6b — hero areas to C5 · gate G5

**Acceptance of every C5 task below:** the named cells meet C5 (see [Completion levels](#completion-levels)):
the full recipe with secondary dressing and signs of habitation; at most one bespoke piece, inside
the task's estimate and named in its `note:` when chosen (rule R8); views by day, at night and
overcast (where the area has windows) with no S1/S2; S3 closed only where cheap; the representative
view within the High budget. **Verify:** the hero area's views and the performance scenario that
covers it.

- [x] HOUSE-03450 — Check the front approach against C5 and close its S1/S2 (hero area H1)
      dep: HOUSE-03452 · sys: world · plat: TOOL · pri: MUST · zone: Z-EXF · adv: G5, D4 · est: 1
      accept: `EXT_WALK`, `EXT_FRONTYARD_W`, `EXT_FRONTYARD_E`, `EXT_DRIVEWAY` and the front elevation (the porch is `HOUSE-00986`). The area has had about 25 tasks: this is a check and S1/S2 fixes, not a new campaign
      note: (2026-09-25) Round 164 closes the intended check without another content campaign. Fresh fixed views cover all four `Z-EXF` cameras at clear 10:30 and scheduled 22:00 plus the two H1 representative cameras at overcast 10:30. Full-resolution inspection finds the complete facade, gate, walk, paired front gardens, driveway and garage elevation readable in all three conditions, with open routes and no S1/S2, clipping, floating/intersecting dressing, z-fighting or missing texture. No runtime, content, asset, material or lighting change was justified. On the §71.1 AMD Radeon 780M, the existing Release `StreetApproach` scenario at 1920×1080 Tier S / High reports 479 draw calls, 1,008,132 triangles, 2.968 ms CPU total and 4.878 ms GPU-completion median (5.592 ms p95), within the desktop High hard budgets

- [x] HOUSE-03454 — Bring the library to C5 (hero area H5)
      dep: HOUSE-03452 · sys: world · plat: TOOL · pri: MUST · zone: Z-L2 · adv: G5, D4 · est: 2
      accept: `L2_LIBRARY`: filled shelves (`HOUSE-00973`'s books), a ladder, reading chairs, a desk and lamps
      note: (2026-09-25) Round 162 retains the existing paired reading chairs/table, desk/lamp, rug, curtains and physical scheduled fixtures, fills all five bookcases to three varied book clusters apiece, and adds a plant and wall art from the capped reusable families. The one permitted bespoke piece is a deterministic 1,296-triangle leaning library ladder generated by the existing prop-kit path, with a 12-triangle narrow collision proxy. All 626 props pass placement, overlap and route validation. Clear-day, scheduled-night and overcast fixed views plus reciprocal detail views show the full composition, readable circulation and no S1/S2, clipping, floating/intersecting furnishing, z-fighting or missing texture. On the §71.1 AMD Radeon 780M, the Release library scenario at 1920×1080 Tier S / High reports 89 draw calls, 555,264 triangles, 1.256 ms CPU total and 3.034 ms GPU-completion median (3.539 ms p95), all within the High budgets

- [x] HOUSE-03455 — Bring the basement cinema to C5 (hero area H6)
      dep: HOUSE-03452 · sys: world · plat: TOOL · pri: MUST · zone: Z-B1 · adv: G5, D4 · est: 2
      accept: `B1_CINEMA`: the static screen and projector, seating rows, acoustic panels and low step lights, readable in the dark under its scheduled lights; day and night views only (it has no windows)
      note: (2026-09-25) Round 163 closes this as a review-only C5 pass: `HOUSE-01023` already supplied the static screen/projector, two four-seat rows and four acoustic panels, and `HOUSE-03403` already supplied four scheduled ceiling fixtures plus two low aisle pucks. Fresh day and scheduled-night fixed views and a reciprocal night view show the full screen, both rows, projector, panels, low lights and clear route with no S1/S2, clipping, floating/intersecting furnishing, z-fighting or missing texture. No extra content was justified. A task-local Release probe using `HOUSE-02402`'s unchanged measurement path on the §71.1 AMD Radeon 780M at 1920×1080 Tier S / High reports 27 draw calls, 28,200 triangles, 0.957 ms CPU total and 2.167 ms GPU-completion median (2.571 ms p95); the standard eight-scenario harness was restored unchanged immediately after the measurement

- [x] HOUSE-00986 — Bring the foyer, the porch and the central hall to C5 (hero areas H1 and H2)
      dep: HOUSE-03450, HOUSE-03454, HOUSE-03455 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G5, D4 · est: 2
      accept: `L0_FOYER`, `L0_PORCH` and `L0_HALL`, including the hall's gallery wall; already dressed, so this completes the hero recipe and closes S1/S2
      trace: absorbs `HOUSE-00987` (`L0_HALL`); was *Furnish `L0_FOYER` and `L0_PORCH`*. Rule R4: after every other hero task
      note: (2026-09-25) Round 165 closes the deliberately-last review without extra decoration. The retained recipes already provide the foyer console/armchair, entry rug/plant and surface dressing; the hall runner, portal art and paired gallery walls; and the porch's clear arrival plus paired lantern/ceiling-light composition. Fresh full `Z-L0M` fixed sets at clear 10:30 and scheduled 22:00 plus targeted overcast views inspect all seven foyer/hall cameras; Round 164's three-condition front-path views inspect the porch. Full-resolution review finds the compositions readable with open routes and no S1/S2, clipping, floating/intersecting furnishing, z-fighting or missing texture. No runtime, content, asset, material or lighting change was justified. On the §71.1 AMD Radeon 780M, the adjacent Release `MainStair` scenario at 1920×1080 Tier S / High reports 57 draw calls, 24,214 triangles, 1.166 ms CPU total and 3.044 ms GPU-completion median (3.524 ms p95); Round 164's `StreetApproach` measurement covers the porch/front arrival. Both are within the desktop High hard budgets

- [x] HOUSE-00988 — Bring the living room to C5 (hero area H2)
      dep: HOUSE-03450, HOUSE-03454, HOUSE-03455 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G5, D4 · est: 1.5
      accept: `L0_LIVING` (fireplace, piano, seating, bay window seat): already dressed, so this completes the hero recipe and closes S1/S2
      trace: was *Furnish `L0_LIVING`*. Rule R4: after every other hero task
      note: (2026-09-25) Round 166 closes the deliberately-last living-room review without extra decoration. The retained recipe already supplies the fireplace and focal art/light, piano/bench and folio, main sofa/chair/coffee-table/rug composition, floor lamp, plant, throw, table dressing, wall art and bay-window seating. Three fresh fixed views from Round 165 cover clear 10:30 and scheduled 22:00; a fresh H2 composition view covers overcast 10:30. Full-resolution inspection finds the complete room readable with an open route and no S1/S2, clipping, floating/intersecting furnishing, z-fighting or missing texture. No runtime, content, asset, material or lighting change was justified. A task-local Release probe through `HOUSE-02402`'s unchanged measurement path on the §71.1 AMD Radeon 780M at 1920×1080 Tier S / High reports 129 draw calls, 792,239 triangles, 1.547 ms CPU total and 3.604 ms GPU-completion median (4.051 ms p95), within the desktop High hard budgets; the standard eight-scenario harness was restored and rebuilt immediately afterwards

- [x] HOUSE-00990 — Bring the kitchen to C5 (hero area H3)
      dep: HOUSE-03450, HOUSE-03454, HOUSE-03455 · sys: world · plat: TOOL · pri: MUST · zone: Z-L0M · adv: G5, D4 · est: 1.5
      accept: `L0_KITCHEN`: already dressed with static closed joinery, the island and the appliances; this completes the hero recipe and closes S1/S2. There are no containers
      trace: was *Furnish `L0_KITCHEN` (the densest room: island, run, appliances, 62 containers)*; the containers were removed by `HOUSE-03201`. Rule R4: after every other hero task
      note: (2026-09-25) Round 167 closes the deliberately-last kitchen review without reviving containers or adding decoration. The retained recipe already supplies the static closed north run, island and range wall, closed refrigerator, stools, canisters, kettle, island dressing and physical pendant/task/ceiling fixtures. Three fresh fixed views from Round 165 cover clear 10:30 and scheduled 22:00; Round 166's fresh H3 view covers overcast 10:30. Full-resolution inspection finds the complete room readable with the work aisle and through-routes open and no S1/S2, clipping, floating/intersecting furnishing, z-fighting or missing texture. No runtime, content, asset, material or lighting change was justified. On the §71.1 AMD Radeon 780M, the existing Release `Kitchen` scenario at 1920×1080 Tier S / High reports 73 draw calls, 172,934 triangles, 1.173 ms CPU total and 2.670 ms GPU-completion median (3.224 ms p95), within the desktop High hard budgets

- [x] HOUSE-03480 — **Gate G5 review: every hero area at C5**
      dep: HOUSE-03450, HOUSE-03454, HOUSE-03455, HOUSE-00986, HOUSE-00988, HOUSE-00990 · sys: — · plat: ALL · pri: MUST · zone: all · adv: G5, D4 · est: 1
      accept: one round over the five hero areas (day, night, overcast where it applies) with no S1/S2 open anywhere; the scoreboard's *Hero cells at C5* column is complete; the S3/S4 backlog is handed to M11
      verify: ledger round; scoreboard
      note: (2026-09-25) Gate G5 passes. Round 168 consolidates six fixed views covering all five retained hero areas: exterior-front and front-path (H1), living-composition (H2), kitchen-facing-west (H3), library (H5) and basement-cinema (H6). All eighteen clear-day, scheduled-night and overcast images were inspected individually at full resolution. Every hero composition remains complete, readable and traversable with no S1/S2, clipping, floating/intersecting furnishing, z-fighting or missing texture. The scoreboard's hero column is complete in every applicable zone. Existing task-level AMD Radeon 780M measurements cover all five areas within the desktop High hard limits. No new S3/S4 was found; the existing zone backlog remains deferred to M11. No runtime, content, asset, material, light, schedule, chunk or lightmap changed. Captures remain local under `house-03480-g5-{day,night,overcast}-r168`

---

## M7 — A compact environment

Track B. **Active:** time of day and day/night, the sun, moon and stars (all existing), **clear,
overcast and rain**, rain kept out of covered areas, **wet surfaces** in rain, and **fog**. The
weather state and its archetypes, the seasons, the astronomy, the sky dome, the clouds, the sun, the
moon and the stars already exist (legacy phases 22–26); the fog path already exists in
`MaterialBinder`. What remains is the weather wired into the sky, light and fog, the rain particles
and the wet-material swap. The storm and hail archetypes stay in the weather data and render as heavy
rain; a snow state renders its overcast sky and fog with no particles. **Falling snow, snow cover,
hail, storm and lightning, glare and lens flare, vegetation sway, puddles, splashes and seasonal
vegetation are in the [optional backlog](#optional-and-conditional-backlog).** No piece may grow into
a simulation of its own.

- [x] HOUSE-01696 — Wire the weather into the sky (cloud cover), the light (cloud modulation) and the fog
      dep: HOUSE-01686, HOUSE-01648, HOUSE-01706 · sys: weather · plat: ALL · pri: MUST · zone: all · adv: D5 · est: 1.5
      accept: (1) cloud cover follows the weather state, so clear, overcast and rain read differently; (2) cloud cover dims and cools the sun and sky light; (3) the storm archetype darkens clouds and light only; there is no lightning
      verify: unit tests of the mapping; the environment captures (`HOUSE-03520`)
      note: (2026-09-25) The remaining wiring was already present through the retained systems and is now verified as one path rather than duplicated: `CnaHouseGame` publishes `WeatherSystem::State().cloudCover` to `LightingSystem`, and the same cover plus wind and thunder intensity to `SkySystem` every frame. Existing mapping tests prove continuous cloud-layer interpolation including the storm row, clear-to-overcast sky tint, direct/diffuse attenuation and non-finite rejection; the end-to-end fixed-weather test proves the command-line weather reaches the shared light state. Round 169 inspected matched 10:30 clear, overcast, rain and thunderstorm front views: clear is visibly blue/bright, all three covered states are cooler/dimmer, storm is the darkest, and no lightning path exists. HUD-cropped linear-grey means were 0.573992, 0.528032, 0.527462 and 0.526576 respectively. The later `HOUSE-03520` set remains the representative environment suite; no duplicate mapper, runtime system or visible retuning was justified. Captures remain local under `house-01696-weather-r169`

- [x] HOUSE-01650 — Fog from the weather: horizon colour in the view direction, distance from `fogDensity` and precipitation, exterior batches only
      dep: HOUSE-01649, HOUSE-00892 · sys: rendering · plat: ALL · pri: MUST · zone: all · adv: D5 · est: 1.5
      accept: (1) the existing `MaterialBinder` fog path receives its fog parameters from the weather each frame; (2) interior batches stay fog-free; (3) clear, overcast and rain show distinct, stable fog without banding
      verify: unit tests of the parameters; the environment captures
      note: (2026-09-25) The game now derives one live `FogParams` value each frame from the rendered sky's horizon colour in the horizontal camera direction, authored `fogDensity`, precipitation intensity and the camera far plane. The retained stock-XNA binder path consumes it only for `EXT_WORLD`, alpha-tested exterior vegetation and explicitly weather-facing shell/window/door batches; ordinary interior batches continue to pass null and therefore clear the shared effect's fog state. The mapping is continuous, bounded and non-degenerate: at the canonical 400 m far plane representative clear/overcast/rain samples progressively pull both ramp endpoints inward, with rain beginning inside the retained front-approach distance. Unit/integration tests cover monotonic parameters, invalid inputs, directional sky colour, overcast uniformity, every stock-effect fog setter/reset and the exterior-cell boundary. Full-resolution Round 173 front views and Round 174 100 m views show stable clear/overcast/rain separation without bands; the Round 174 centre-crop linear-grey standard deviation falls 0.052706 → 0.022241 → 0.020789 as distant contrast is progressively lost to the matching horizon. No second sky/weather system was added

- [x] HOUSE-01741 — Shared particle renderer: pooled camera-facing quads in a `DynamicVertexBuffer`, one draw per material
      dep: HOUSE-00092 · sys: weather · plat: ALL · pri: MUST · zone: all · adv: D5 · est: 2
      accept: (1) a fixed pool, no per-frame allocation; (2) one draw call per material, at most a few materials; (3) XNA 4.0 API only; works under every quality preset
      verify: unit tests of the pool; a draw-count check
      note: (2026-09-25) `ParticleRenderer` owns fixed arrays for 2,000 particle records and 8,000 `VertexPositionColorTexture` vertices, plus one retained `DynamicVertexBuffer`/index buffer pair created on first use. `BeginFrame` only resets counters and selects the existing preset ceiling (500 Low, 1,000 Medium/High, 2,000 Ultra); it never grows a container. Valid submissions are regrouped into at most six borrowed texture slots without allocation, uploaded once with `SetDataOptions::Discard`, and drawn once per used material through stock XNA `BasicEffect`, alpha blend and read-only depth. Pure tests cover all four presets, capacity rejection, stable pool storage, invalid records and the camera-facing dimensions; the real OPENGLES3 test interleaves five quads across two materials and proves one upload, exactly two draws and no empty-frame upload. Strict-XNA compiled all 330 translation units clean with four workers. The renderer deliberately has no visible weather consumer until dependent `HOUSE-01742`/`HOUSE-01743`; no placeholder texture or optional particle effect was added

- [x] HOUSE-01742 — Camera-relative precipitation volume with wrap-around and a wind offset
      dep: HOUSE-01741 · sys: weather · plat: ALL · pri: MUST · zone: all · adv: D5 · est: 1.5
      accept: the volume follows the camera with wrap-around, and its wind offset reads the weather state's smoothed wind vector; there is no gust model
      verify: unit tests of the wrap and the offset
      note: (2026-09-25) `PrecipitationVolume` is the allocation-free geometry boundary for dependent rain work: a 12 m radius, 14 m high cylinder follows the camera eye and shifts exactly 3 m downwind using the already rate-limited `WeatherState::windSpeed`/`windDirectionDeg`. Meteorological “from” direction is converted once to the project's +X-east/-Z-north travel vector; `gustFactor` is not read and no oscillator/RNG was introduced. `Follow` wraps a caller-owned fixed position span in place after every camera/wind update, using opposite-side periodic vertical and radial boundaries; non-finite input leaves both centre and positions untouched. Four unit tests cover cardinal wind, calm and gust independence, the exact offset, both cylinder boundaries, large camera relocation, stable in-place storage and transactional invalid-input rejection. Strict-XNA compiled all 332 translation units clean with four workers. No rain is emitted or drawn yet; that is the immediate dependent `HOUSE-01743`

- [x] HOUSE-01743 — Rain: particle motion and the rain streak texture and material
      dep: HOUSE-01742 · sys: weather · plat: ALL · pri: MUST · zone: all · adv: D5 · est: 1.75
      accept: (1) gravity, wind, an intensity-driven count and velocity-elongated quads; (2) a rain streak texture and material, manifested; (3) rain, sleet and the hail and storm archetypes draw rain streaks; a snow state draws nothing
      verify: unit tests of the motion; the environment captures
      trace: absorbs `HOUSE-01745` (the streak texture)
      note: (2026-09-25) `RainParticles` retains exactly 900 deterministic positions in the existing 12 m × 14 m camera-relative volume and advances them without allocation. The active prefix follows `round(900 · intensity^0.8 · qualityScale)` (495 Low, 720 Medium/High-preset, 900 Ultra-preset at full intensity); rain, sleet and hail submit it, while none and snow clear the shared renderer. Each step uses the already rate-limited wind at 0.55× plus §37.1's 6–9 m/s downward terminal velocity, wraps the whole fixed pool, and submits one 2 cm-class soft streak whose long edge is projected from that velocity into the camera plane. `TransparentPass` borrows the existing `ParticleRenderer`, draws its single rain material before additive fixture glows and adds no pass or effect. `tools/assets/rain_streak.py` deterministically authors the manifested 16 × 64 RGBA texture `TEX_WEATHER_RAIN_STREAK`; the content pipeline compiled and loaded it as `Textures/Weather/rain_streak`. Three rain-motion/phase tests plus the extended billboard test cover the formula, every retained liquid phase, snow clearing, gravity/wind displacement, wrapping, fixed storage, streak length and velocity alignment; the existing authored-weather test proves `W_THUNDERSTORM` is Rain, and the new Hail path consumes Hail directly. All 1,434 unit tests and eight focused integration tests pass; strict-XNA compiled all 334 translation units clean with four workers. Full-resolution OPENGLES3 Round 175 exterior rain was inspected at 1,600 × 900: the streak texture loaded, streaks are soft and velocity-slanted, and no missing-texture, clipping or blend defect was visible. The paired interior capture deliberately exposes the next task's unsheltered particles; roof/window coverage remains solely `HOUSE-01744`

- [x] HOUSE-01744 — Keep rain out of covered areas: the roof/coverage mask and the teleport-to-top of sheltered particles
      dep: HOUSE-01743, HOUSE-00777 · sys: weather · plat: ALL · pri: MUST · zone: all · adv: D5, D11 · est: 2
      accept: (1) standing under the porch, rain visibly stops at the porch edge, and likewise at the other covered outdoor areas; (2) windows are closed, so no rain particle enters through one; (3) a test proves no rain particle is ever drawn below a covered surface
      verify: the coverage test; a porch capture in rain
      trace: absorbs `HOUSE-01755` (the coverage test)
      note: (2026-09-25) `CoverageMask` is the strict runtime reader for the existing CCOV v1 bake (160 × 128 at 0.5 m; 1,635/20,480 finite samples) and the fixed rain update teleports every drop at or below finite cover to the top of its existing camera-relative volume before submission. The writer now correctly stores the highest slab underside—the first barrier encountered by rain falling from the sky—rather than the lowest floor in a multi-storey stack; the format documentation also explicitly treats IEEE +infinity as open sky before applying the finite-height comparison. The authored-field test measures the porch at 3.30 m 0.30 m inside its edge and +infinity 0.30 m outside, the shed at 2.35 m, observes sheltered teleports, and proves every submitted drop is exposed both under the porch and from an indoor camera. Outdoor rain remains visible through the closed family-room windows while no particle position under the house roof is submitted, preserving `HOUSE-03520`'s indoor-rain view without modelling open windows. Full-resolution OPENGLES3 captures at 1,600 × 900 were inspected: rain stops at the porch edge and the indoor frame shows streaks only through the window apertures, with none in the room. The coverage selftest and six focused rain/coverage tests pass; the complete 1,437-test unit label passes. The serial offscreen integration label passes 142/143; the sole failure is the pre-existing authored-content assertion expecting 50 alpha-cutout batches while `chunks.bin` contains 51, reproduced alone and unrelated to this task. Strict-XNA compiles all 336 translation units clean with four workers

- [x] HOUSE-01748 — Wet surfaces in rain: the wet-material swap driven by `surfaceWetness` (Tier S)
      dep: HOUSE-01690, HOUSE-00905 · sys: rendering · plat: ALL · pri: MUST · zone: all · adv: D5 · est: 1.25
      accept: the 14 existing wet variants (`HOUSE-00905`) replace their dry materials as `surfaceWetness` (`HOUSE-01690`) rises, and return as it falls; no puddles
      verify: unit test of the swap threshold; the wet-driveway capture
      note: (2026-09-25) `StaticGeometryPass` resolves each dry Tier-S material to one of the fourteen existing fully-wet endpoints once at construction and swaps at integrated `surfaceWetness >= 0.5`, returning to dry below the threshold. The generated unbaked outdoor rows now carry an explicit data-owned `wetVariant` reference to their canonical endpoint; `MaterialBinder::BindAs` applies that endpoint's tint and specular values through the Basic vertex layout those meshes actually pack, so the driveway does not silently remain dry and no material id is hard-coded in runtime code. Fixed command-line liquid-weather reviews start at their established wet endpoint while ordinary weather still rises and falls through the existing integrator. Three focused swap tests, the material-loader case, all eight serial software-offscreen `StaticGeometryPass` tests and the complete 1,440-test unit label pass. Strict-XNA compiles all 337 translation units clean with four workers. The complete pre-commit gate set passes except the known user-owned root `.claude` layout report. Matched full-resolution OPENGLES3 heavy-rain frames at 14:00 use the same camera, seed and frame 3: the driveway crop's linear mean falls from 0.362553 to 0.335560 (-7.45%) and the complete-frame MAE is 0.0234842; inspection finds the retained darkened surface and no puddles

- [x] HOUSE-03520 — Environment captures and the representative environment render set
      dep: HOUSE-01696, HOUSE-01650, HOUSE-01743, HOUSE-01744, HOUSE-01748 · sys: ci · plat: LNX · pri: MUST · zone: all · adv: D5, D11 · est: 1.75
      accept: (1) `capture_review.py` gains an environment scenario set: clear, overcast and rain at noon, dusk and night from the front approach and from an upper-floor window looking out; (2) every D5 item is visible in it; (3) the same scenes, plus heavy rain seen from indoors, rain under the porch and the wet driveway, become render tests (about 13 scenes); (4) sky and fog are tuned only where the set shows an S1/S2, bounded to 1 h; (5) S1/S2 environment defects are fixed or filed
      verify: the capture set; the render suite; a ledger round
      trace: absorbs `HOUSE-01653` (the environment render tests) and `HOUSE-01652` (sky and fog tuning); the snowfall scene moved to the optional backlog with `HOUSE-01791`
      note: (2026-09-25) Round 178 adds one bounded 13-scene Tier-S/High set to the existing capture tool and software-OPENGLES3 render harness. Clear noon, dusk and night are shown from the fixed front approach and through the master-bedroom's upper-floor windows; matched noon overcast/rain views plus heavy rain through those closed windows, rain stopping at the porch edge, the fully wet driveway, and the cold `W_SNOW` dawn state cover the remaining D5 weather boundaries. The first snow candidate at 12:00 correctly resolved to rain at 8.9 C, so the final deterministic row uses 06:00 at -0.4 C and visibly has the required overcast/fog state with no precipitation particle. Existing dedicated sun/moon/star render fixtures continue to prove their exact discs/catalogue; this integrated set shows the clear time-of-day progression and star field. All thirteen 1600 x 900 frames and their contact sheet were inspected at full resolution: rain stays outdoors, wet materials remain coherent, clear/overcast/rain/snow atmospheres separate, and no S1/S2, clipping, missing material or fog band is present. The one-hour tuning allowance was therefore unused. The committed 640 x 360 references pass both focused environment tests at the existing tolerance; no runtime, content, sky or fog value changed

---

## M8 — Atmospheric audio essentials

Track B. A supporting layer, not a subsystem: footsteps on **six broad surface categories**, one
**interior tone**, exterior **day and night** beds, **rain** and **wind** layers quieter indoors by
sky exposure, and volume settings. Everything comes from the imported NOX collection
(`content/Audio/ambience`, `footstep`, `footstep-exterior`); there is no new sourcing, no offline
derivative set, no positional source, no special zone loop and no room-aware routing.
`src/audio/AudioSystem.cpp` already exists and is extended, not replaced.

- [x] HOUSE-01911 — `AudioSystem`: master and three category volumes, mute, device-loss handling and the sound-bank loader
      dep: HOUSE-00154, HOUSE-00279 · sys: audio · plat: ALL · pri: MUST · zone: all · adv: D7 · est: 2
      accept: (1) categories master, footsteps, ambience and weather, applied live; (2) mute, and no crash or stall when no audio device exists or it is lost; (3) the sound banks load from `layout.audio.json` and the manifest; a missing bank is reported and silent, not fatal
      verify: unit tests of volumes and bank loading; the no-audio-device run
      trace: absorbs `HOUSE-01917` (the bank loader)
      note: (2026-09-25) The existing `AudioSystem` now owns the retained master/footsteps/ambience/weather mix, clamps live changes, preserves the configured master across mute and moves to supported silent state if opening or updating the XNA device throws. `layout.audio.json` authors thirteen reusable NOX banks: six broad footstep families, one interior bed, exterior day/night beds and calm/strong rain and wind. The loader resolves their sound ids through the deployed authoritative manifest without opening a device; an invalid or missing bank is logged once and remains silent while valid banks survive. The existing world loader/data path carries the rows, and the existing content deploy copies the normalized manifest beside the normalized world files; no player, routing, positional-source or generalized audio framework was added. Twelve focused `AudioSystem` unit tests, the loader case and five `AudioGateTests` pass, including all thirteen deployed banks against the deployed manifest and a complete `--no-audio` session. The complete 1,443-test unit label and serial 144-test integration label pass. An initial four-worker integration run exposed two unrelated test issues: the HOUSE-01748 wet split had made the stable alpha-test batch count 51 while the assertion still said 50, and the real-time weather bound exceeded two simulated minutes only while four software render sessions contended. The corrected count and unchanged weather behavior both pass in the final serial label. The full static gate compiles 338 strict-XNA translation units clean with four workers; after regenerating the content-stage document and world-id golden, only the user-owned root `.claude` layout entry remains outside this task

- [x] HOUSE-01919 — `FootstepDirector`: stride accumulator, surface lookup, round-robin sample selection and one step per stair riser
      dep: HOUSE-01911, HOUSE-00560, HOUSE-00027 · sys: audio · plat: ALL · pri: MUST · zone: all · adv: D7, D11 · est: 2
      accept: (1) cadence follows speed, and stairs give one step per riser; (2) per-surface round-robin selection with no repeat within 4, pitch ±4 % and volume ±10 %; (3) a test proves cadence matches speed within 5 % across speeds and surfaces
      verify: unit FootstepDirectorTests.*
      trace: absorbs `HOUSE-01918` (the `Bag<T>` selector) and `HOUSE-01938` (the cadence test)
      note: (2026-09-25) `FootstepDirector` now consumes the fixed-step distance already measured by the player controller, retains cadence across flat-ground steps, and changes between the documented 0.75 m walk and 0.95 m fast-walk strides without introducing another movement probe. The caller supplies the collision surface and, on stairs, the authored riser height plus actual vertical travel; the director resets the ordinary stride phase on the stair boundary and emits exactly one event per crossed riser. Surface spellings bind to the existing resolved banks, each bank advances deterministically through its samples, and the existing project RNG supplies bounded ±4 % pitch and ±10 % gain variation. Unknown surfaces and airborne/non-finite samples stay silent and reset cadence rather than leaking distance into a different material. Five focused tests prove two speeds across two surfaces within 5 %, correct surface-bank selection, ten selections without a repeat in the preceding four, exact variation bounds, one event for each of seventeen risers, and silent/reset behavior. The complete 1,448-test unit label passes with four workers, and strict-XNA compiles all 340 translation units clean; the full static gate reports only the known user-owned root `.claude` layout entry. Runtime binding and playback remain with the dependent `HOUSE-01920`; no mixer, collision query or generalized selector was added

- [ ] HOUSE-01920 — Wire six broad footstep surface categories
      dep: HOUSE-01919, HOUSE-00280 · sys: audio · plat: TOOL · pri: MUST · zone: all · adv: D7 · est: 1
      accept: wood (hardwood and wooden stairs, NOX `wood`), carpet (the softest NOX pack at a lower level), tile, stone and concrete (NOX `rock`), gravel, grass; every surface in the house maps to one of them; no new sourcing and no offline derivative
      verify: a mapping check over every surface in the world data; a listening check in one room of each category
      trace: was *Wire the 20 footstep surface sets (12 from NOX, 8 sourced)*, then six with an offline-derived carpet set (`HOUSE-03206`)
      blocked: (2026-09-25) Implementation and automated validation are ready locally: all 22 authored surface spellings map exactly once to the six retained direct-NOX banks, 20 focused unit tests, five audio-gate integration tests and the real-controller silent walk pass. The required subjective listening check cannot be performed in this execution environment: `/dev/snd` is absent, `aplay -l` reports no soundcards, PulseAudio refuses the connection, and the Codex runtime rejects audio input. A 48 kHz mono six-category review montage was produced under `/tmp` and measured at 4.628 s / -8.07 dBFS peak, but it was not claimed as listened to. The box remains open until a human-capable audio session listens in one representative location per category; no requirement is weakened.

- [ ] HOUSE-01922 — Ambience: one interior tone and the exterior day and night beds, cross-faded by listener cell and sun
      dep: HOUSE-01911, HOUSE-00559 · sys: audio · plat: ALL · pri: MUST · zone: all · adv: D7 · est: 1.75
      accept: (1) the listener cell selects the interior or the exterior bed, with a short cross-fade on an indoor ↔ outdoor change; (2) one interior room tone (from NOX, or generated offline as a single loop); (3) the exterior bed is NOX day (`forest-birds`) and night (`night`, `cicadas`), cross-faded by sun elevation; (4) no per-cell room tone and no special zone loop
      verify: unit tests of the selection and the cross-fade; a listening walk indoors and out
      trace: absorbs `HOUSE-01923` and `HOUSE-01924`; was *per-cell room tone with an 0.8 s cross-fade* plus two special zone loops (`HOUSE-03206`)
      blocked: (2026-09-25) The bounded implementation and automated evidence are ready locally: listener cell selection, the 0.8 s indoor/outdoor cross-fade and the -6°..+3° sun-elevation day/night blend pass four focused director tests; the real `--scene=walk` path opens exactly the four retained loop voices (interior, forest-birds, night and cicadas) under SDL dummy audio. All 1,453 unit tests pass; 137/145 serial integration tests pass, with the other eight isolated to the sandbox refusing SaveStore writes under `/home/robertvokac/.local/share`; strict-XNA compiles 342 translation units clean. The required indoor/outdoor listening walk cannot be performed here because `/dev/snd` is absent, ALSA reports no soundcards, PulseAudio refuses the connection and the Codex runtime rejects audio input. The task remains open until an audio-capable session performs that walk; no acceptance criterion is weakened.

- [ ] HOUSE-01925 — Weather layers: rain and wind over the exterior bed, quieter and duller indoors by sky exposure
      dep: HOUSE-01922, HOUSE-00779 · sys: audio · plat: ALL · pri: MUST · zone: all · adv: D7 · est: 1.5
      accept: (1) rain (NOX `rain-calm`/`rain-strong` by intensity) and wind (`wind-calm`/`wind-forest` by wind speed); (2) indoors both are attenuated and low-passed by the listener cell's sky exposure: `L3_STORE_W` hears strong rain on the roof, `B1_CINEMA` essentially nothing, the glazed sunroom clearly more than an interior room; (3) no seasonal layer, no per-window or per-roof layer
      verify: unit tests of the attenuation; a listening check in those three cells
      trace: absorbs `HOUSE-02000` (sky-exposure routing), `HOUSE-01750` and `HOUSE-01883`

- [ ] HOUSE-01939 — Listening walk of every zone; fix what is wrong, missing or too loud
      dep: HOUSE-01920, HOUSE-01925 · sys: audio · plat: LNX · pri: MUST · zone: all · adv: D7 · est: 0.75
      accept: one walk of each zone by day and at night, in clear weather and in rain; every S1/S2 audio defect fixed; no per-room or per-weather mixing matrix
      verify: the walk's findings in the ledger

---

## M9 — Application shell

Track B. Functional, not productised: *Start*, pause, **one settings screen**, credits, quit, a
controls hint, and layouts that work at desktop and phone aspects. It reuses the existing
`MenuStack`, `TextRenderer` and loading screen, and settings persistence (`HOUSE-00131`,
`HOUSE-00152`). The debug overlays (`F1`–`F5`, `F8`, `F9`) and the console stay as they are; none is
extended.

- [x] HOUSE-02516 — The settings screen: one page with Graphics, Audio, Controls and Environment sections
      dep: HOUSE-00156, HOUSE-00131, HOUSE-01911 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D7, D8 · est: 2
      accept: (1) one screen, no tabs and no second page, navigable by keyboard, mouse and touch, with every control reachable by keyboard; (2) the Audio section: master, footsteps, ambience and weather volumes, applied live; (3) the Controls section: look sensitivity, invert-Y and the walk-speed toggle behaviour; no key remapping; (4) the Graphics and Environment sections are `HOUSE-02518` and `HOUSE-02521`
      verify: unit tests of navigation; `HOUSE-02528`
      trace: absorbs `HOUSE-02519` (Audio), `HOUSE-02520` (Controls), `HOUSE-02529` and `HOUSE-01935`; was *the settings menu shell with the 5 tabs*
      note: (2026-09-25) `SettingsScreen` reuses `MenuStack`, `TextRenderer`, the existing settings file and the compact M8 mix. Keyboard, mouse and XNA touch feed one device-independent navigation path; all seven Audio/Controls rows are keyboard-reachable and apply live. Settings v9 migrates the old `effectsVolume` to footsteps and persists weather separately. Graphics and Environment remain headings only for their owning tasks. Focused tests pass 48/48 unit and 6/6 offscreen integration; the complete unit label passes 1458/1458. No key remapping or widget framework was added.

- [x] HOUSE-02518 — The Graphics section, filtered by the project-owned effective feature set
      dep: HOUSE-02516, HOUSE-00160 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D8 · est: 1.25
      accept: (1) the quality preset (High, Web, Android), resolution (or canvas size on the Web), fullscreen, v-sync and field of view; (2) options come from the effective feature set of `cna-house.md` §68 (`RenderTier` + build/platform profile + validated standard-XNA behaviour), so no meaningless toggle is shown; (3) `SupportsCapability()` or any other CNA extension query is called nowhere, and `check_xna_only.py` passes on the UI sources
      verify: unit tests of the filtering; `check_xna_only.py`
      trace: absorbs `HOUSE-02517`'s display rows
      note: (2026-09-25) The one page now exposes only quality (High/Web/Android), standard-XNA display resolution (`Canvas size` on Web), fullscreen, v-sync and FOV. `SettingsFeatures` resolves rows from `BuildTarget`, `RenderTier` and `GraphicsAdapter` display modes: Web retains canvas size/fullscreen for `HOUSE-03721` but omits browser-fixed v-sync, Android omits fixed device display controls, and Tier S labels the effective High profile honestly. Changes apply to the existing graphics manager, effective quality, camera, text and input viewports. Settings v10 migrates the removed persisted Ultra UI choice to High; the diagnostic command-line row remains internal. Focused tests pass 52/52, startup/tier integrations 9/9 and the complete unit label 1461/1461. `check_xna_only.py` is clean; strict-XNA caught and removed a first-pass CNAEXT collection iterator, then compiled all 342 translation units clean through the standard XNA format indexer.

- [x] HOUSE-02521 — The Environment section: time of day and weather
      dep: HOUSE-02516 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D5, D8 · est: 1
      accept: time of day (automatic, or a fixed hour), time speed, and weather (automatic, clear, overcast, rain); no location, season jump, archetype list, pets or save slots
      verify: unit tests; `HOUSE-02528`
      trace: was *the Simulation tab (day length, weather mode, moon speed, location, pets, pause-on-menu, slots)* (`HOUSE-03206`)
      note: (2026-09-25) The existing one-page settings screen now exposes only Automatic or 06:00/12:00/18:00/22:00 time, 60x/30x/15x time speed, and Automatic/Clear/Overcast/Rain weather. Fixed time updates and freezes the existing `SimClock`; returning to Automatic resumes it at the selected speed. Weather changes rebuild only the existing `WeatherSystem` state so fixed choices apply immediately, including wet rain, while Automatic resumes authored transitions. Settings v11 persists the fixed-hour choice and narrows removed legacy fixed weather to Clear. No location, season, full archetype list or simulation UI was added. Focused tests pass 28/28 and the complete unit label passes 1463/1463; XNA-only and strict-XNA are clean.

- [x] HOUSE-02523 — The main menu, the pause menu, the credits screen and the first-run controls hint
      dep: HOUSE-02516, HOUSE-00198 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D8 · est: 2
      accept: (1) main menu *Start* · *Settings* · *Credits* · *Quit*; (2) pause menu *Resume* · *Settings* · *Main menu* · *Quit*; (3) credits show `THIRD-PARTY-ASSETS.md`; (4) a first-run hint names the controls of the active input scheme (keyboard and mouse, or touch) and fades after about 12 s; (5) there is no *Continue* (session resume is optional)
      verify: unit tests of the menu flow; the UI render set (`HOUSE-02527`)
      trace: absorbs `HOUSE-02525` (the hint line)
      note: (2026-09-25) The loading/audio gate now hands off to a four-item main menu, whose Start action loads the existing blockout/walk presentation. Escape or the menu action opens the four-item pause menu; settings reuse the existing one-page screen, Main menu replaces the stack without reloading the house, and Credits presents the generated `licenses/THIRD-PARTY-ASSETS.md` through standard-XNA `TitleContainer`. The first successful Start shows one input-scheme-specific control line and fades it over the final 3 s of a 12 s lifetime; returning to the main menu cannot replay it. Five focused menu/hint tests pass, the complete unit label passes 1464/1464, and two offscreen integrations prove loading → main menu → Start → traversable house and the full generated credits document. Layout/safe-area renders remain owned by `HOUSE-02527`; no Continue, session persistence, widget framework or gameplay system was added. A same-day manual regression report found that the legacy scene-level Escape exit still ran after menu dispatch: Settings correctly popped and then the application exited in the same frame, while the walk's newly opened pause menu consumed the opening edge and closed immediately. The catch-all is now restricted to non-interactive diagnostic scenes, a newly opened pause screen does not receive its opening edge, and two offscreen application integrations prove changed Settings → Escape → main menu and walk → Escape → pause → Escape → walk both reach their frame limit without quitting.

- [x] HOUSE-02527 — UI layout in virtual units with safe-area insets, and the UI render set
      dep: HOUSE-02523, HOUSE-00145 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D8, D10b, D10c, D11 · est: 1.5
      accept: (1) the menus and the settings screen lay out correctly at 16:9, 4:3 and 20:9 (the phone aspect of M15) with safe-area insets; (2) render tests of the main menu, the pause menu and the settings screen at 16:9 and 20:9 (about 6 scenes; `HOUSE-02995` adds the forced-touch HUD)
      verify: the render suite
      trace: absorbs `HOUSE-02530` (the UI render tests) and `HOUSE-02956`; was proved at 4:3, 16:9, 21:9 and 18:9
      note: (2026-09-25) `TextRenderer` now uniformly fits its 1600x900 virtual canvas inside standard-XNA `Viewport.TitleSafeArea`, centres letterbox space at 4:3 and 20:9, clamps invalid rectangles, and preserves the authored SpriteFont pixel size for low-resolution legibility. Runtime load and graphics-setting changes both refresh the transform from the live viewport. Four focused unit cases cover 4:3, 16:9, 20:9 and invalid insets; all 1,468 unit tests pass. Six software-renderer references cover main, pause and settings at 16:9 and 20:9 with forced non-zero insets; all six pass exact geometry/coverage and pixel comparison after visual inspection. The inspection also found and fixed the pre-existing Settings/Graphics heading overlap. The older `hud-season-01` render's behavioural legibility/hide assertion passes, but its committed whole-world reference remains stale (3.8496%; over 9,000 exact differing pixels remain below the HUD band), independently of this UI-only change.

- [x] HOUSE-02528 — Test: every settings value round-trips, clamps and applies live
      dep: HOUSE-02516, HOUSE-02518, HOUSE-02521 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D8, D11 · est: 0.75
      verify: unit SettingsRoundTripTests.*
      note: (2026-09-25) `SettingsRoundTripTests` now prove all 22 persisted fields through the production JSON writer/reader, both endpoints of every bounded numeric setting plus the empty fixed-weather fallback, and all 15 retained settings-page rows. Each page row is mutated before its existing live-apply callback sees the committed value; no second application path or settings subsystem was introduced. The focused 3/3 tests and complete 1,471/1,471 unit suite pass.

---

## M10 — Performance (measurement-driven)

Track B. House Simulator is a CNA stress test, so performance work is kept, but it is **driven by
measurement**: optimise only a scenario that misses its documented target, stop as soon as it meets
it, and keep **no headroom margin**. A task whose measurement comes back green closes with the
numbers and no code. No speculative optimisation infrastructure is built (rule R9).

- [x] HOUSE-02402 — The eight representative performance scenarios as a runnable harness
      dep: HOUSE-00150, HOUSE-03202 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D9 · est: 1.5
      accept: eight scenarios: three interiors (the kitchen, the library, the main stair looking up), three exteriors (the street approach towards the house and the neighbourhood, the rear garden, an upper-floor window looking out), heavy rain outside, and night outside; each runs headless with a fixed camera and prints draw calls, triangles and CPU and GPU milliseconds
      verify: the `perf` preset runs all eight
      trace: was *the 10 performance scenarios*
      note: (2026-09-25) Added eight fixed 1920×1080 Tier-S/High scenarios with 120 warm-up and 600 measured frames each. The test-only XNA path renders once into a preserved full-size target, records CPU submission, forces GPU completion with the already-proved one-texel `GetData`, and reports average draw calls/triangles plus median/p95 timings. CTest supplies fresh-process isolation, current-world fixture, offscreen video, dummy audio and serial execution. Release CTest ran the fixture and all eight scenarios in 649.67 s with 9/9 passing. The available Mesa offscreen path is functional evidence only; its timing is not the reference-hardware baseline owned by `HOUSE-02403`

- [x] HOUSE-02403 — Measure the furnished house: the eight scenarios, memory and cold-start load, recorded as the baseline
      dep: HOUSE-02402, HOUSE-03380, HOUSE-01030, HOUSE-00203 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D9 · est: 2
      accept: (1) `docs/performance-log.md` records, per scenario, draw calls, triangles, CPU and GPU milliseconds; (2) texture and lightmap memory against their budgets (300 MB textures, 60 MB lightmaps), with texture sizes or lightmap texel density reduced only where over; (3) cold-start load time and total resident footprint (GPU and RSS) with everything loaded; if cold start ≤ 4.0 s, GPU ≤ 550 MB and RSS ≤ 1.6 GB, no residency or streaming work is planned; if a limit is exceeded, a Planning corrections entry plans the minimum residency the numbers demand, under R9, R14 and with new ids
      verify: the log entry
      trace: absorbs `HOUSE-00915` (texture memory), `HOUSE-02451` (the load/footprint gate), `HOUSE-00911`, `HOUSE-01031`, `HOUSE-00853`, `HOUSE-01276`, `HOUSE-01654`, `HOUSE-01753` and `HOUSE-02463`
      note: (2026-09-25) The then-current Release build was stale and its shared CNA SDL-cache lock could not be acquired under that session's sandbox; this was not treated as a measured baseline.
      note: (2026-09-26) Refreshed the existing Release `build-probe/` against current source/content and measured all eight 1920×1080 Tier-S/High scenarios on the reference Radeon 780M Wayland/radeonsi path. Worst measured scenario: 83 draws, 412,957 triangles, 2.154 ms CPU (average updates plus median submit) and 4.557 ms GPU median (these extrema occur in different scenes); all are below High hard limits. The 332 compiled textures total 117.334 MB base RGBA8, ≤156.445 MB including a conservative full mip chain; 216 lightmaps total 14.156/≤18.874 MB, below 300/60 MB. Three process-cold, warm-filesystem startup captures took 1.38/1.38/1.27 s including readback/PNG. All 1,391 chunks were resident; with no culling, live amdgpu memory was 368.4 MB and RSS 710.2 MB, while the isolated-scenario RSS peak was 761.7 MB, below the 550 MB and 1.6 GB limits. `docs/performance-log.md` records per-scene metrics and measurement qualifications. No residency/streaming system or texture-size reduction is warranted. The separate compiled pack-size warnings remain for packaging work. The user-owned `.claude` root entry still fails the full layout gate; targeted built-in pack targets refreshed Release assets without changing or bypassing that entry.

- [ ] HOUSE-02404 — Optimise the scenarios that miss their target, and only those
      dep: HOUSE-02403 · sys: rendering · plat: LNX · pri: MUST · zone: all · adv: D9 · est: 4
      accept: (1) every scenario meets its documented target at the *High* tier (`cna-house.md` §71.2), with no headroom margin; (2) the technique is chosen by the measured bottleneck, and each technique is a ≤ 4 h slice planned under R7 only when chosen: per-category LOD, detail-set culling, chunk sub-range culling, render-list and state work, instancing; (3) when every scenario meets its target, the task closes, even with budget left; (4) the result is recorded in `docs/performance-log.md`
      verify: the harness before and after
      note: impostors (`HOUSE-00851`/`02395`) stay in the optional backlog unless a measurement proves the neighbourhood is the bottleneck and nothing cheaper works

- [ ] HOUSE-02405 — The three quality presets, each measured against its own budget
      dep: HOUSE-02404, HOUSE-00157 · sys: rendering · plat: ALL · pri: MUST · zone: all · adv: D9, D10b, D10c · est: 2
      accept: High (desktop), Web and Android presets, selectable with `--quality` and in the Graphics section; each measured on desktop against its documented budget in the harness
      verify: the harness under each preset
      trace: absorbs `HOUSE-02406`, `HOUSE-02407`, `HOUSE-02852` and `HOUSE-02957`

---

## M11 — Final defect pass (bounded) · after G5 only

**No M11 task starts before `HOUSE-03480` (G5).** One whole-property review, one bounded fix pass
and one golden refresh. This is where rule R12's budget lives: **4.5 agent-hours of S2/S3 work** plus
whatever S1 fixes need (about 7 h for the milestone; never more than 12 h unless S1/S2 remain).
There is no zone rotation, no "polish everything" pass and no screenshot → tweak → screenshot loop.
**When the pass is spent and no S1/S2 remains, polish is done.**

- [ ] HOUSE-02714 — Final walkthrough review: everything that still reads as broken or fake, classified by severity
      dep: HOUSE-03480, HOUSE-01939, HOUSE-03520 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D4, D13, D14 · est: 1.5
      accept: the grand-tour route walked once by day and once at night in first person, plus each hero area's views; every finding classified S1–S4 by the severity table and filed under its zone
      verify: the ledger round
      trace: absorbs `HOUSE-01033` (the furnishing review); was *Phase-45 review: a full walkthrough with fresh eyes*

- [x] HOUSE-03632 — Restore reliable room-boundary tracking and the main-stair approach
      dep: HOUSE-03480 · sys: player, world-content · plat: ALL · pri: MUST · zone: Z-L0M, Z-L0S, Z-STAIR · adv: D2, D13 · est: 2
      accept: (1) a grounded player crosses the L0 powder-room doorway in both directions and the tracked cell follows the body instead of remaining on the old side; (2) the foyer-to-main-stair opening contains no stair support, furnishing or rail obstruction and a standing capsule crosses it; (3) the real controller walks from the foyer through that approach and reaches L1; (4) all eight flights remain walkable up and down
      verify: `HeadlessRunTests.PowderRoomDoorwayIsTraversableInBothDirections`, `OpeningReachTests.*`, `StairTraversalTests.*`, `PosedLeafCollisionTests.*`, and an inspected fixed-pose stair-entrance capture
      note: (2026-09-25, targeted R15 correction) Manual exploration found an S1 trap at `L0_WC1` and an inaccessible main-stair entrance after G5. Grounded soles settle a few ulps below FFL, so exact vertical cell containment left the old room active after the body crossed a doorway; tracking the capsule centre restores the invariant already used at spawn. The stair opening's southern half was authored into the solid return support, the lower run had no approach landing, and the foyer console occupied the only remaining path. The cased opening is now a clear 1.00 m passage, the L0 flight leaves its real approach strip while still opening the basement-well guard, and the existing console/dressing are mirrored away from the route. The final focused set passes 10/10 plus the two-direction offscreen runtime test: every door retains its open control, all eight flights traverse both ways, and the foyer route reaches L1. A trial shift of the L1→L2 flight was rejected before commit when the open-door control proved it obstructed `L1_BED5`; that flight remains unchanged. The inspected day capture shows the furniture removed from the opening; lighting remains too dark and is `HOUSE-03633`. The complete gate passes every task-owned check, including 341 strict-XNA translation units; only the pre-existing user-owned root `.claude` layout entry fails. This S1 work consumes 2 h of `HOUSE-03631`'s original 5 h allowance rather than expanding M11

- [x] HOUSE-03633 — Restore automatic light coverage in dark circulation and wet rooms
      dep: HOUSE-03632 · sys: lighting · plat: ALL · pri: MUST · zone: all · adv: D6, D13 · est: 1
      accept: (1) the retained automatic schedule makes the ground-floor hall, WC1 and the main-stair approach readable by day and night without player interaction; (2) the authored fixtures visibly contribute light in those fixed views; (3) the fix uses the existing schedule/lightmap path and does not globally raise exposure or add light interaction
      verify: focused lighting tests plus inspected fixed-pose before/after captures of WC1/hall and the stair approach
      note: (2026-09-25, targeted R15 correction) Manual exploration and fixed captures found an S2 D6 failure: Wet and Circulation schedules switched all fixtures off during bright clock hours even where baked daylight was insufficient. Those two existing schedule classes now remain automatic throughout the civil day; Living, Bedroom, Task, Dusk and Off keep their bounded behaviour. The existing WC1 and main-stair bakes were linearly recalibrated through their source-local `bakeLumensPerRadiantWatt` values, with matching scale and receiver-mean metadata; no texture, fixture count, exposure value, interaction or subsystem was added. Focused tests prove the three reported groups are on at noon and retain at least 0.10 mean baked receiver energy. Matched 1,600 × 900 OPENGLES3 day captures with the same groups forced off/on change 5.00% MAE in WC1 and 6.51% at the stair; HUD-cropped linear luminance rises 0.008336→0.022411 in WC1 and 0.024460→0.062018 over the stair route. Day and 22:00 views show visible warm fixtures and readable WC1, hall and stair approach. All 1,473 unit tests and every task-owned repository gate pass, including 341 strict-XNA translation units with four workers; only the pre-existing user-owned root `.claude` layout entry fails. This consumes 1 h of `HOUSE-03631`'s original allowance; no new lighting system or weather depth was added

- [x] HOUSE-03634 — Make every accessible authored room light automatic
      dep: HOUSE-03633 · sys: lighting, world-content · plat: ALL · pri: MUST · zone: all · adv: D6, D13 · est: 0.75
      accept: (1) no accessible cell with authored fixtures has only groups that can never turn on; (2) service rooms, closets and stores reuse the retained automatic schedule rather than gaining switches or a new controller; (3) a real fixed view shows the fixture and its baked contribution; (4) the timed Living, Bedroom and Task schedules remain unchanged
      verify: `LightScheduleTests.EveryAccessibleLitCellHasAnAutomaticGroup`, the complete unit suite and an inspected matched automatic/forced-off service-room capture
      note: (2026-09-25, targeted R15 correction) Manual play exposed the wider case behind the dark-room report: 28 accessible service, closet and store groups were `SC-OFF`, so their physical fixtures could never emit in the interaction-free showcase. Those groups now reuse the already-always-on `SC-CIRC` class; `SC-OFF` remains only on the nested refrigerator interior outside the retained walk. A regression checks every non-nested lit cell. The 10:30 Butler's-pantry automatic/forced-off pair changes 95.08% of pixels; linear luminance rises 0.020995→0.034080 over the fixture and 0.014411→0.024711 on the left wall. The fixture visibly emits and the floor receives the existing bake. All 1,474 unit tests pass. This consumes 0.75 h of `HOUSE-03631`'s remaining allowance and adds no light, interaction, exposure change or subsystem

- [ ] HOUSE-03631 — The bounded fix pass
      dep: HOUSE-02714, HOUSE-03634 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D2, D4, D13, D14 · est: 1.25
      accept: (1) every S1 and S2 from `HOUSE-02714` and the zone backlogs is fixed; (2) S3 only where the severity table allows it (cheap, repeatedly visible, in a hero area or in a release capture); (3) S4 is never scheduled; (4) **hard budget: 4.5 agent-hours of S2/S3 work**, plus whatever S1 fixes need; when it is spent, the remaining S3 items are waived with a reason; (5) no area gets more than two fix rounds (rule R6)
      verify: one ledger round over the fixed items
      note: split into ≤ 4 h slices when scheduled (R7). This replaces the zone polish rotation and the eleven legacy polish passes. `HOUSE-03632`, `HOUSE-03633` and `HOUSE-03634` consume 3.75 h of the original 5 h estimate after manual S1/S2 findings, leaving 1.25 h here; M11 does not expand

- [ ] HOUSE-02713 — Refresh the representative render sets whose images changed on purpose
      dep: HOUSE-03631 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D11 · est: 0.5
      accept: only the representative sets (`HOUSE-01275`, `HOUSE-03520`, `HOUSE-02527`) whose images changed on purpose; every refreshed golden inspected
      verify: the render suite

---

## M12 — Representative tests

Track B. The existing suites and gates stay. Tests for removed or finished systems are not gaps,
and new tests are representative, not exhaustive. Each feature task carries its own tests (rule
R16): the representative render sets live with their features (`HOUSE-01275`, `HOUSE-03520`,
`HOUSE-02527`) and the grand tour is `HOUSE-03226`. This milestone holds only what no feature owns.

- [x] HOUSE-00493 — Make intermittent gate failures diagnosable: output on failure, the failing pose and pixel count, and a difference image
      dep: HOUSE-00483 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D11 · est: 1.5
      accept: (1) `ctest` runs with `--output-on-failure` in the wrapper; (2) a render comparison names the failing pose, the differing pixel count and the mean absolute difference against its stated budget, and writes a difference image as an artefact; (3) the two known intermittent failures (`AudioGateTests.NoAudioRunsAFullSessionAndNeverOpensTheDevice`, `BlockoutPoseRenderTests.*`) are diagnosed and fixed if they reproduce, or their next sighting is guaranteed to be diagnosable
      verify: a deliberately perturbed golden fails with a named pose, a count and a difference image
      note: four sightings on 2026-09-09/10, all in full-suite runs on a loaded machine, none reproducible alone; neither failure's output was captured
      note: (2026-09-25) The shared `tools/ci/run_tests.sh` makes `--output-on-failure` unconditional and all CI ctest entries use it. A deliberately mismatched `ext-road`/`ext-east` comparison reported `ext-road`, 137551/230400 pixels, mean absolute channel difference 31.194 against the 0.2000%/tolerance-2 budget, and wrote a visually inspected 640x360 RGBA difference PNG. The audio gate passed 10/10 targeted repetitions and the historical transient blockout failure did not reproduce; the former now adds exit/frame/audio-state/reason context and every blockout mismatch writes the same counted-pixel diagnostic. A separate load-sensitive weather assertion reproduced at 137.650 minutes remaining, passed alone, and was fixed to assert the behaviour rather than host speed; the complete correctly isolated integration label then passed 147/147. Unit passed 1472/1472 and strict XNA passed 341 translation units with four workers. The only full-gate failure is the pre-existing user-owned root `.claude` entry.
      trace: absorbs `HOUSE-02594` (the tolerance policy and the difference image)

- [x] HOUSE-02598 — The 20-minute automated stability run
      dep: HOUSE-03226 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D11 · est: 2
      accept: the grand tour looped plus the seeded random walk, the clock at 60× and the weather cycling clear → overcast → rain; RSS growth below 20 MB/hour extrapolated; no crash; no audio starvation; ≥ 200 settings writes; runnable locally and in CI. The single 2-hour run is `HOUSE-02784`
      verify: the run's report
      trace: was *the 2-hour soak test at 60 FPS with 200 autosaves*
      note: (2026-09-25) `tools/ci/run_stability.py` reuses the production-collision grand tour and seed-618 random walk in two long-lived repeated GoogleTest processes, measures their Linux RSS from `/proc`, and writes a machine-readable report; the nightly headless CI job runs it and uploads the report and logs. The control slice advances the existing clock at 60x through clear → overcast → rain, performs and reads back 200 real atomic `DesktopSaveStore` settings writes, and runs the no-audio full-session gate. The required local run completed in 1,363.512 s: 54 grand tours in 639.985 s and seven six-start random walks in 630.642 s, with 0 crashes, 0 audio-starvation/underrun records and worst extrapolated RSS growth 4.991 MiB/hour against the <20 limit (grand tour 0.717, random walk 4.991). The focused control passed 3/3 and the complete integration label passed 149/149. The same runner accepts a longer duration for the one two-hour run in `HOUSE-02784`; no runtime subsystem was added.

---

## M13 — Linux desktop release

After M11. The DONE audit and the packaged desktop build.

- [ ] HOUSE-02781 — DONE audit: check D1–D14 item by item for the desktop, and open a task for every gap
      dep: HOUSE-02713 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D1–D14 · est: 1.25
      accept: (1) every desktop DONE item checked against its evidence; (2) the "zero TODO/TBD/FIXME in shipping paths" rule verified; (3) the day-length decision (Q-11) recorded; (4) every gap becomes a task in `HOUSE-02782` or, if not needed for DONE, a line in the optional backlog
      verify: the audit table in `docs/handoff.md`
      trace: absorbs `HOUSE-02786` (the TODO rule) and `HOUSE-02793` (Q-11); was *the §79 feature-complete checklist*

- [ ] HOUSE-02782 — Fix the gaps found by the DONE audit
      dep: HOUSE-02781 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D1–D14 · est: 1.5
      accept: every gap `HOUSE-02781` found is fixed; a gap larger than this estimate gets its own task under R7 and R14

- [ ] HOUSE-02783 — Run the full suite under ASAN and UBSAN; fix every report
      dep: HOUSE-02782, HOUSE-00137 · sys: — · plat: CI · pri: MUST · zone: all · adv: D11 · est: 1.5
      accept: clean under both, built in the existing `build-asan/` and `build-ubsan/` directories only (no TSAN)

- [ ] HOUSE-02784 — The one 2-hour run before DONE; fix every leak and drift
      dep: HOUSE-02598, HOUSE-02782 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D11 · est: 1.5
      accept: `HOUSE-02598`'s stability run for 2 hours with the same limits; it is not repeated at other milestones

- [ ] HOUSE-02788 — Verify every asset is manifested and licensed, and regenerate `THIRD-PARTY-ASSETS.md`
      dep: HOUSE-00299 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D12 · est: 0.75

- [ ] HOUSE-02789 — Package a distributable Linux build with its content, licences and a launch script
      dep: HOUSE-02788 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D10a · est: 1.75
      accept: a Release build with `CNAHOUSE_DEBUG_TOOLS=OFF`, which also proves the debug tools compile out
      trace: absorbs `HOUSE-02515`

- [ ] HOUSE-02790 — Run the packaged build on a clean profile, with and without an audio device
      dep: HOUSE-02789 · sys: — · plat: LNX · pri: MUST · zone: all · adv: D10a · est: 1.5
      accept: (1) no dev environment and no sibling checkouts; (2) the High preset at 1920×1080 and 2560×1080, and the Web and Android presets at 1920×1080; (3) a run with no audio device completes without error
      trace: absorbs `HOUSE-02791` (no audio device) and `HOUSE-02792`; the `CNA_ENABLE_VIDEO=OFF` variant is dropped because the showcase plays no video

- [ ] HOUSE-02797 — Tag the feature-complete desktop release
      dep: HOUSE-02783, HOUSE-02784, HOUSE-02790 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D10a · est: 0.5
      accept: the notes are the changelog since the last tag; the full release notes are `HOUSE-03077`
      trace: absorbs `HOUSE-02796`

---

## M14 — Web

**Web is a first-class DONE target** (D10b). The bring-up block may run as Track B any time after
G1: it de-risks the toolchain and exposes Web-only content limits early. The verification block
follows M13. Validation breadth is deliberately small: Chrome and Firefox, one representative
traversal, acceptable performance, and a headless smoke test in CI. Cosmetic Web luxuries are cut:
there is no progressive pack streaming, no persistence across a page reload, no context-loss
recovery and no threads (all in the optional backlog); losing the WebGL context is a known
limitation (reload the page), recorded by `HOUSE-02904`.

### Bring-up (Track B, any time after G1)

- [x] HOUSE-02843 — Audit and fix Web blockers in the desktop code: custom loops, `Game::Run` misuse, filesystem access outside `DesktopSaveStore`
      dep: HOUSE-00127, HOUSE-02841 · sys: app · plat: ALL · pri: MUST · zone: all · adv: D10b · est: 1
      trace: absorbs `HOUSE-02842` (filesystem access)
      done: the sole POSIX-file exemption was an unused `Log::SetFileSink`; the API and its
            `std::fopen` handle are removed. Runtime review finds exactly one `CnaHouseGame`
            construction and one `game.Run()` call, both in `Main.cpp`; all ordinary loops in the
            app/player/UI path are bounded parsers or fixed-step drains. The existing XNA gate now
            rejects direct browser-loop ownership (`emscripten_set_main_loop`, the Emscripten RAF
            loop and `requestAnimationFrame`) with a planted regression, and has no path exemption.
      verify: XNA-only self-test 17/17; XNA-only repository scan clean; debug build; unit 1473/1473

- [x] HOUSE-02891 — The Emscripten preset, proved on a minimal scene
      dep: HOUSE-02843 · sys: app · plat: WEB · pri: MUST · zone: all · adv: D10b · est: 2.5
      accept: (1) the `web` preset in `CMakePresets.json` builds with `WEBGL2`, the exception ABI, Asyncify and the pack preloads; (2) a minimal scene (one room, one prop, one sound) runs in Chrome, validating the toolchain and the Asyncify interaction before the full build
      done: the real `web` preset cross-builds a bounded `cna-house-web-spike` through the native
            content tool. Its generated link contract contains exact WebGL2 min/max, JS exception
            catching, Asyncify and a preloaded 27 KiB compiled sound pack. The XNA-only spike
            renders one open-front room and one central prop, loads and starts that sound, then
            exits through `Game::Run` after 90 updates; it introduces no browser-loop owner.
      verify: Chrome 152 reports `WEBGL2`, renders the room and prop, initializes audio and prints
              `HOUSE-02891 WEB SPIKE PASS room=1 prop=1 sound=1 frames=90`; inspected 800×457
              running-frame PNG SHA-256
              `688565ad1296748ab851350b78bbf8d53294113ec708362c6c82e6956c7af404`
      trace: absorbs `HOUSE-02855` (the spike)

- [x] HOUSE-02848 — Lint the material data: every runtime texture is `SurfaceFormat::Color` with pre-generated mips; no MRT, stencil, geometry or tessellation stage
      dep: HOUSE-00110 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D10b · est: 0.5
      done: the manifest-backed gate checks every one of the 332 packaged PNG textures, their
            content configuration and all world-material albedo/normal references. It rejects
            absent mip generation, non-Color format overrides and runtime MRT/stencil/geometry/
            tessellation APIs. The 218 previously unconfigured packaged textures plus two
            fallback textures now request complete offline mip chains; the 34 ORM authoring maps
            explicitly marked notPackaged remain outside the runtime set.
      verify: gate self-test and repository scan pass; native content build recompiles 252 and
              skips 114 textures with no failure; direct CNB-header audit confirms all 332
              packaged textures are RGBA8 with full mip chains (149.44 MiB compiled total);
              native `cna-house` builds and 1474/1474 unit tests pass. The complete CI gate has
              only the pre-existing root-layout complaint about another session's `.claude`
      trace: absorbs `HOUSE-02849`

- [x] HOUSE-03722 — Cross-build and link the full game for Emscripten
      dep: HOUSE-02891 · sys: app · plat: WEB · pri: MUST · zone: all · adv: D10b · est: 3.5
      accept: the full `cna-house` target builds and links with the Web preset and the required
              content preload; all compile and link errors are fixed without changing CNA
      done: the `web` build preset now targets the full game, using the proved WEBGL2/exception/
            Asyncify link contract and one upfront preload of the existing `content` and
            `content-fx` roots. Clang-only warning failures and a libc++ incomplete-type
            constructor issue were fixed locally without changing CNA or visible behaviour
      verify: four-core Emscripten build links `cna-house.html`, 10 MiB WASM and 392 MiB data;
              native game and unit-test targets build, 1474/1474 unit tests pass, strict-XNA
              checks 341 units clean. The full CI gate has only the pre-existing `.claude`
              root-layout complaint. The large uncompressed data warning is measured input
              to `HOUSE-02850`, not a claim that Web pack limits are met
      note: the compile/link slice of `HOUSE-02892`, scheduled under R7; no added scope or hours

- [x] HOUSE-02892 — Start the full Emscripten game in Chrome and fix startup to the main menu
      dep: HOUSE-03722 · sys: app · plat: WEB · pri: MUST · zone: all · adv: D10b · est: 1.5
      accept: the full game starts to the main menu in Chrome
      done: served the full HTML/WASM/data build to local Chrome 152. CNA reported WEBGL2,
            loaded Tier E and displayed the title/loading screen; the intended first
            click/Enter user gesture opened audio and advanced to the House Simulator main menu
            with Start selected. No unhandled browser exception appeared
      verify: CDP console trace and actual 1280×720 Chrome screenshot
              `/tmp/house-02892-gesture-5.png`, SHA-256
              `5d647de20b9ebf8139eeef266f37ba1e8b3c7d66cea01f81d46c2c027ea2d208`;
              the visible 1600×900 canvas extends below the viewport, owned by `HOUSE-02895`
      note: browser-start slice of the original five-hour task under R7; the two estimates sum to 5

- [x] HOUSE-03721 — Web desktop controls: pointer-lock mouse look, keyboard walk, focus handling
      dep: HOUSE-02892 · sys: player · plat: WEB · pri: MUST · zone: all · adv: D8, D10b · est: 1.5
      accept: (1) a click on the canvas acquires pointer lock and mouse look works without drift; (2) Esc and focus loss release the pointer and open the pause menu; (3) WASD/arrows and the walk-speed modifier work; (4) verified in Chrome and Firefox
      verify: manual browser check recorded in `docs/portability.md`; the headless smoke remains `HOUSE-02901`
      note: (2026-09-26, targeted R7/R15 split) Chrome and Firefox both reached walking, acquired pointer lock on canvas click, changed view without idle drift, and opened Pause with cursor release on Escape and tab focus loss. Firefox real-key W and Up moved the body; native `InputTests` covers WASD/arrow aliases and Shift's speed edge. XNA has no browser pointer-lock operation, so a tiny pre-JS bridge follows the existing `Game.IsMouseVisible` cursor state; Emscripten alone skips desktop mouse recentering. The touch-only condition was not claimed: M15's `TouchSource`/HUD is unfinished and is now `HOUSE-03723` after its real dependency. `docs/portability.md` records the browser evidence and remaining limitation. The native/Web builds passed; the isolated unit suite passed 1475/1475 and four-worker strict XNA passed 341 units. The full static gate passed except the pre-existing user-owned `.claude` root-layout entry

- [x] HOUSE-03723 — Select and verify the touch-only Web control scheme
      dep: HOUSE-03721, HOUSE-02995 · sys: player · plat: WEB · pri: MUST · zone: all · adv: D8, D10b · est: 0.75
      accept: on a touch-only browser use M15's `TouchSource` scheme when CNA `TouchPanel` reports touch under Emscripten; otherwise record the measured limitation
      verify: browser touch emulation or device check recorded in `docs/portability.md`
      trace: remaining conditional touch criterion split from `HOUSE-03721` under R7/R15, not optional scope
      note: (2026-09-26) Chrome mobile/touch emulation at 400×800 received real CNA `TouchPanel` frames: the first tap selected Web touch input, dismissed the gesture gate and displayed the touch-spaced main menu; a Start tap entered walking with MOVE/MENU/WALK HUD, and the Menu button opened Pause. CNA advances a new Pressed touch to Moved before the game read; `TouchSource` now recognises the XNA previous-Pressed location as a one-frame edge. Device polling stays inside that source (`check_input_boundary`: 282 files clean), and untouched Web stays keyboard/mouse until actual touch. Native/Web builds, 12 focused tests and the full 1489/1489 unit label pass; one unrelated SkySystem timing test failed at four-way parallelism, passed alone and in the final two-way full run. Strict XNA passed 344 translation units with four workers; the full static gate reports only the pre-existing user-owned `.claude` root-layout entry. Browser captures are in `docs/portability.md`

### Verification (after M13)

- [ ] HOUSE-02850 — Fit the Web packs: every pack ≤ 60 MB and the Web-tier total ≤ 180 MB compressed, reducing content only where over
      dep: HOUSE-00203, HOUSE-02405 · sys: content · plat: CI · pri: MUST · zone: all · adv: D10b · est: 1.5
      accept: (1) a check of both limits in CI; (2) where a limit is missed, the Web tier's texture sizes, audio bitrates or LOD-only packs are reduced until it fits
      trace: absorbs `HOUSE-02851` (the reduction)

- [x] HOUSE-02895 — The canvas as the display, and the audio gesture gate, in a real browser
      dep: HOUSE-02892 · sys: ui · plat: WEB · pri: MUST · zone: all · adv: D10b · est: 0.75
      accept: (1) canvas resize and the fullscreen toggle work from the Graphics section; (2) audio starts only after the first user gesture, in Chrome and Firefox, and the desktop build is unaffected
      trace: absorbs `HOUSE-02896` and `HOUSE-02846`
      verify: Chrome 152 CDP and Firefox 140 ESR/Xvfb showed audio waiting before the first click,
              then device opened/ready after it; Graphics changed the DOM canvas from 1280×720
              to 1600×900 and toggled document/browser fullscreen on and off in both browsers.
              Inspected menu/fullscreen screenshots and the native game/unit build; focused
              SettingsScreen tests passed 8/8, isolated full unit suite 1476/1476, strict XNA
              341 units. The full static gate had only the pre-existing user-owned `.claude`
              root-layout failure. Details in `docs/portability.md`
      note: (2026-09-26) The Web launch now starts at the documented 1280×720 tier instead
            of the desktop 1600×900 size. The Web Graphics list uses the two legible 16:9
            canvas sizes rather than the browser's 800×600 pseudo-monitor mode; desktop mode
            enumeration remains unchanged. The generated Emscripten launcher still adds a
            header above the canvas in windowed mode, so a short viewport may scroll; its
            bounded page/loading treatment remains `HOUSE-02899`

- [ ] HOUSE-02898 — Measure the Web build against its target; reduce content until it fits
      dep: HOUSE-02892, HOUSE-02405 · sys: — · plat: WEB · pri: MUST · zone: all · adv: D9, D10b · est: 2
      accept: the Web preset's representative scenes (the street approach, one interior, the rear garden) in Chrome on the reference machine meet the documented Web target

- [x] HOUSE-02899 — The browser loading screen with progress from the pack preload
      dep: HOUSE-02892, HOUSE-00156 · sys: ui · plat: WEB · pri: MUST · zone: all · adv: D10b · est: 1
      accept: progress shown from the preload until the main menu; no progressive fetch or streaming
      trace: absorbs `HOUSE-02854`; was *the browser loading screen and the progressive pack fetch*
      verify: the full Web preset rebuilt with its one upfront 391 MiB preload. Chrome CDP
              throttling showed visible progress advancing from 8,504,652 to 31,963,944 of
              410,635,649 bytes; after the preload, the existing in-game gesture/loading
              screen appeared, and its first click opened audio and reached the main menu.
              Firefox also reached the in-game prompt and main menu under the new shell.
              Native game build and strict XNA (341 units, four workers) passed; the full
              static gate failed only the pre-existing user-owned `.claude` root-layout entry.
              Inspected captures and DOM states are in `docs/portability.md`
      note: (2026-09-26) One Web-only Emscripten shell replaces the generic toolbar with a
            centred progress screen using Emscripten's existing `setStatus` and
            `monitorRunDependencies` callbacks. When the engine takes over, the existing
            `LoadingScreen` carries the startup/gesture handoff to the menu. No runtime
            fetch, streaming, new loader system or desktop behaviour changed

- [ ] HOUSE-02900 — The Web DONE checklist in Chrome and in Firefox
      dep: HOUSE-02898, HOUSE-02899, HOUSE-02895, HOUSE-03721, HOUSE-03723, HOUSE-02850, HOUSE-02797 · sys: — · plat: WEB · pri: MUST · zone: all · adv: D10b · est: 1.5
      accept: the build loads; the controls work; the representative traversal (street → foyer → main stair → an upper floor → the garden) completes; no major rendering corruption; acceptable performance; saving settings does not fail (persistence across a reload is optional). Differences between the browsers are recorded
      trace: absorbs `HOUSE-02903`; was *Verify the whole feature set in a browser: every phase's headline feature*

- [ ] HOUSE-02901 — A headless-Chrome smoke test in CI
      dep: HOUSE-02900 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D10b, D11 · est: 1.5
      accept: menu, load, 300 frames of the walk, one settings save; no interaction

- [ ] HOUSE-02904 — Package and document the Web build
      dep: HOUSE-02900, HOUSE-02901 · sys: — · plat: WEB · pri: MUST · zone: all · adv: D10b, D12 · est: 0.75
      accept: a deployable directory; `docs/portability.md` records the readiness matrix, the browser differences and the known limitations (context loss means a reload)
      trace: absorbs `HOUSE-02856` and `HOUSE-02905` (the phase review)

---

## M15 — Android

**Android is a first-class DONE target** (D10c). The desktop-side readiness block waits only for
G1. CNA's Android graphics gate passed on 2026-09-26 (`HOUSE-02951`); `cna-house` still does not
modify CNA. The touch scheme is a
floating movement stick, a look region, a walk-speed toggle and a menu button; there is no interact
button and no camera toggle. Validation is one representative device, or the best available
emulator, not a compatibility lab. Cosmetic platform luxuries are cut.

### Desktop-side readiness (Track B, any time after G1)

- [x] HOUSE-02991 — `TouchSource` over `TouchPanel`, and a complete `IInputSource` abstraction
      dep: HOUSE-00101, HOUSE-00140 · sys: player · plat: AND · pri: MUST · zone: all · adv: D8, D10c · est: 2.25
      accept: (1) multi-touch tracking with the gestures the scheme needs: the stick, the look drag and taps; (2) no direct `Keyboard`/`Mouse` read anywhere outside the input sources, checked by a lint
      trace: absorbs `HOUSE-02954` (the `IInputSource` check)
      verify: five new touch-sequence tests and 21 existing input tests pass; the isolated
              native suite passes 1481/1481. Input-boundary lint scans 280 other runtime
              files; native and Web builds pass. Strict XNA passed 343 translation units
              with four workers; the full static gate failed only on the pre-existing
              user-owned `.claude` root-layout entry. Details in `docs/handoff.md`
      note: (2026-09-26) `TouchSource` now implements the existing `IInputSource` and samples
            XNA `TouchPanel`, preserving stick/look finger IDs across collection reordering,
            independent drags, press/tap pointer edges and clean role release. Recorded
            sequences cover crossing, simultaneous third-finger taps, viewport/sensitivity
            scaling and re-acquisition without a look jump. The new input-boundary lint
            scans runtime code; only the source implementations may poll devices. Native
            and Web builds pass; the HUD, forced desktop selection and button regions remain
            `HOUSE-02992`/`HOUSE-02995`, not claimed here

- [x] HOUSE-02992 — The floating movement stick and the look region
      dep: HOUSE-02991 · sys: ui · plat: AND · pri: MUST · zone: all · adv: D8, D10c · est: 2
      accept: (1) a floating stick bottom-left (180 vu) with analogue direction and magnitude; (2) a look region (the right half minus buttons) with its own sensitivity setting; (3) both tuned on desktop with `--force-touch`
      trace: absorbs `HOUSE-02993` (the look region) and the desktop half of `HOUSE-03001`
      verify: native and full Web targets build with four jobs; 33 focused
              input/CLI/settings tests and the 1483-test native unit suite pass.
              Xvfb 1600×900 captured the held floating stick, and Xvfb 2560×1440
              captured a right-half drag rotating the actual camera at 1600×900 back buffer.
              The full static gate passes except the pre-existing user-owned `.claude`
              layout entry; strict XNA recompilation passes 343 units with four workers
      note: (2026-09-26) The source keeps independent roles, clips the analogue stick to
            a 180-vu radius and excludes the two future button corners from look starts.
            A 24-segment ring and movable thumb use the existing HUD SpriteBatch/white texel;
            no new graphics or input framework was added. `touchLookSensitivity` is a separate,
            backward-compatible persisted setting with 0.2–4× clamp. `--force-touch` selects
            the source and turns a desktop mouse into one test finger. The flag was moved here
            from the dependent `HOUSE-02995` because this task's desktop acceptance requires it;
            see the targeted correction below. Android device proof remains gated by BL-13

- [x] HOUSE-02995 — The walk-speed and menu buttons, the touch-HUD visibility rule and forced-touch integration
      dep: HOUSE-02992, HOUSE-00141, HOUSE-02527 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D10c · est: 1.5
      accept: (1) a walk-speed toggle and a menu button, no camera toggle; (2) the HUD shows only when `hasTouch && !hasKeyboard`, so desktop never shows it without the force flag; (3) the existing `--force-touch` makes the HUD testable on Linux and proves that the `Platform` struct drives it; (4) its 20:9 screenshot joins `HOUSE-02527`'s UI render set
      trace: absorbs `HOUSE-02996`, `HOUSE-02997`, `HOUSE-02955` and `HOUSE-03000`
      verify: 10 focused touch-source tests pass, 1486/1486 native unit tests pass,
              and all seven UI layout references (including the inspected 20:9 touch
              screenshot) pass in Xvfb software GL. Native/Web builds pass with four
              jobs. Xvfb game captures prove `WALK` → `FAST`, Menu → Pause without
              exit, and no touch HUD in ordinary desktop mode. Strict XNA passes
              344 translation units with four workers; the full static gate fails
              only on the pre-existing user-owned `.claude` root-layout entry
      note: (2026-09-26) Android's build profile supplies touch/no-keyboard; Linux and
            Web default to keyboard/no-touch. The force flag overrides only those
            two Platform input facts, and the app uses `hasTouch && !hasKeyboard`
            for source selection, HUD drawing and mouse-capture suppression.
            Touch hit boxes follow the existing safe virtual canvas; two 160-vu
            corners emit menu/speed edges independently of held stick/look fingers.
            The existing SpriteBatch draws the bounded MOVE/MENU/WALK or FAST HUD.
            No camera toggle, avatar, extra input framework or CNA API was added

- [x] HOUSE-02998 — Touch-friendly menu hit targets (≥ 88 vu) without changing the desktop layout
      dep: HOUSE-02995 · sys: ui · plat: ALL · pri: MUST · zone: all · adv: D10c · est: 0.5
      note: (2026-09-26) The four-item main/pause menus use touch-only rows centred 108 virtual units apart and an 88-vu vertical pointer target, preserving the desktop rows and bands. Touch pointers now normalise within the existing safe virtual canvas, so widescreen insets do not offset those targets. Edge/gap/desktop unit tests pass; an actual forced-touch Xvfb capture shows the four separated rows. Native and Web builds and the full native unit label pass. Strict XNA passed with four workers; the full static gate is otherwise blocked only by the pre-existing user-owned `.claude` root-layout entry

### Device path (BL-13 gate passed 2026-09-26)

- [x] HOUSE-02951 — **Gate: CNA can build and draw on Android** (BL-13)
      dep: — · sys: — · plat: AND · pri: MUST · zone: all · adv: D10c · est: 1.5
      accept: re-verified whenever this task is picked (the last recorded state is *blocked*, 2026-09-06): (1) CNA's Android cross-compile succeeds for `arm64-v8a` (the two `sharp-runtime` NDK-portability bugs fixed upstream, CNA Task 920); (2) `CNA_GRAPHICS_RENDERER=OPENGLES3` is selectable and buildable for Android; (3) a CNA graphics sample runs on a device or emulator. If any step fails, the evidence is recorded here and in `cna-house.md` §6, the device path stops, and the task stays open; the desktop-side readiness tasks do not wait for it. A recorded blocker never closes this task, M15 or D10c
      trace: absorbs `HOUSE-02952` and `HOUSE-02953` (the second and third gates)
      verify: current CNA `cefe6c83b` with sharp-runtime `d86adb65` configured for
              Android API 24/`arm64-v8a`/`OPENGLES3` using installed NDK 29.0.14206865;
              `cna_runtime` and `cna_renderer_easygl` cross-built successfully with
              four jobs and the shared ccache. A temporary copy of CNA's existing
              `demo_devices` graphics sample built as an arm64 APK, installed and ran
              on `Medium_Phone` (API 35, x86_64 emulator with arm64-v8a translation,
              GPU-accelerated OpenGL ES 3.1). The inspected `adb screencap` is
              `docs/android-cna-graphics-sample.png`; `pidof` still reported the app
              and logcat had no graphics crash after the draw
      note: (2026-09-26) The old two sharp-runtime NDK failures did not recur. The
            sample's old Gradle defaults required absent Build Tools 34/NDK 30 and
            CMake 3.22 (below EasyGL's 3.23 minimum); only the ignored probe copy
            selected installed Build Tools 36.1/NDK 29/CMake 4.1, target `main`,
            and SDL's documented `SDL_main.h` entry point. The first APK launched
            but returned home because its library exported `main`, not `SDL_main`;
            the corrected probe exported `SDL_main` and drew. CNA and sharp-runtime
            source trees were not changed. This closes only the CNA gate, not Android
            House Simulator D10c; `HOUSE-03031` is now unblocked

- [x] HOUSE-03031 — The Gradle/NDK project producing a shared library plus `SDLActivity`, following CNA's own devices-demo precedent
      dep: HOUSE-02951 · sys: app · plat: AND · pri: MUST · zone: all · adv: D10c · est: 2.5
      verify: `android/gradlew assembleDebug --offline --max-workers=4` packages
              `lib/arm64-v8a/libmain.so` with CNA's SDL3 Java activity and native
              libraries; APK installs on the `Medium_Phone` emulator. Gradle and
              CMake/Ninja products stay in the existing `build-probe/`; the native
              compile pool is four jobs and uses the shared ccache

- [x] HOUSE-03032 — Build the full game for `arm64-v8a` and fix every compile and link error
      dep: HOUSE-03031 · sys: — · plat: AND · pri: MUST · zone: all · adv: D10c · est: 4
      verify: the full `cnahouse_core` and `libmain.so` compile and link for API 24
              `arm64-v8a`/OPENGLES3; `llvm-nm -D` finds an unmangled `SDL_main`,
              SDLActivity loads it and the emulator keeps the process running.
              The first launch had `_Z8SDL_mainiPPc` and SDL's `Couldn't find
              function SDL_main`; C linkage in the project-owned entry point fixed it.
              A dark startup frame remains for content/runtime tasks, so this is
              **not** the Android DONE checklist or a claim of visible House content

- [ ] HOUSE-03033 — Content delivery (APK assets or OBB), the content root, and the package-size budget
      dep: HOUSE-03032, HOUSE-00203, HOUSE-02405 · sys: content · plat: AND · pri: MUST · zone: all · adv: D10c · est: 1.5
      accept: the Android-tier packs are delivered and found at run time, and fit the APK + OBB budget
      trace: absorbs `HOUSE-02958` (the budget check)

- [x] HOUSE-03034 — The settings store in Android's app-private storage
      dep: HOUSE-03032, HOUSE-00152 · sys: persistence · plat: AND · pri: MUST · zone: all · adv: D10c · est: 0.5
      verify: the existing XNA `StorageDevice`/`DesktopSaveStore` now loads
              settings at Android startup and writes each Settings-screen change
              and the walk-speed toggle;
              the emulator's `run-as` shows a 596-byte version-11 JSON file at
              `files/game/CnaHouse/AllPlayers/settings.json`, owner-only mode 0600,
              and it survives a force-stop/relaunch. `SettingsTests.*` and
              `SaveStoreTest.*` pass. CNA's Android StorageDevice already maps this
              standard-XNA container beneath the package-private files directory;
              no Android or SDL storage API was added to House runtime

- [x] HOUSE-03036 — The Android lifecycle: pause, resume, background and surface loss onto `Game`'s events, verified on a device
      dep: HOUSE-03032 · sys: app · plat: AND · pri: MUST · zone: all · adv: D10c · est: 2
      accept: (1) the mapping is designed and recorded; (2) the desktop focus-loss path runs the same lifecycle code; (3) pause, resume and background work on the device without a crash or a lost surface
      trace: absorbs `HOUSE-02959` (the design) and `HOUSE-02960`
      verify: `docs/portability.md` records the XNA event map. Real Xvfb focus
              transfer logged activated → deactivated → activated through House's
              shared handlers. On the API-35 `Medium_Phone` emulator, Home caused
              SDL `onPause()`/`surfaceDestroyed()`; returning caused
              `onResume()`/`surfaceCreated()`. The House PID stayed 6363,
              EGL resumed frame submission; after orientation settled, the
              1600×700 central rendered crop had zero differing pixels versus
              before backgrounding (clear-color pixel `srgba(18,20,24,1)`). No
              content was packaged, so this proves surface lifecycle, not the
              later scene traversal or Android performance checklist

- [ ] HOUSE-03037 — Run on the device; measure against the Android target; reduce until it fits
      dep: HOUSE-03036, HOUSE-03033 · sys: — · plat: AND · pri: MUST · zone: all · adv: D9, D10c · est: 2.5
      accept: one physical device; if none is available, the best available emulator with GPU acceleration, recorded as such; the Android preset's representative scenes meet the documented Android target
      note: (2026-09-26) A measured representative scene requires the Android
            content root and packs from `HOUSE-03033`; a clear-color-only APK
            cannot establish the preset's target. This is dependency ordering,
            not a new performance requirement

- [ ] HOUSE-03038 — The Android DONE checklist
      dep: HOUSE-03037, HOUSE-02797 · sys: — · plat: AND · pri: MUST · zone: all · adv: D10c · est: 1.5
      accept: install and run; touch controls sufficient for the walk; the representative traversal of `HOUSE-02900`; no major corruption; acceptable performance. One device is enough; more are recorded when available
      trace: absorbs `HOUSE-03040`; was *Verify the whole feature set on a device*

- [ ] HOUSE-03039 — Tune the touch controls on the device
      dep: HOUSE-03038, HOUSE-02992, HOUSE-02998 · sys: ui · plat: AND · pri: MUST · zone: all · adv: D8, D10c · est: 0.75

- [ ] HOUSE-03041 — Package and document the Android build
      dep: HOUSE-03039, HOUSE-03033, HOUSE-03034 · sys: — · plat: AND · pri: MUST · zone: all · adv: D10c, D12 · est: 0.75
      accept: an installable package; `docs/portability.md` records the readiness matrix and the device used
      trace: absorbs `HOUSE-02961` and `HOUSE-03042` (the phase review)

---

## M16 — Final release

Last. Measure each platform once more, fix only real misses, audit, document, tag, and enter
maintenance mode.

- [ ] HOUSE-03071 — Final measurement on all three platforms; fix only target misses and platform-specific catastrophic regressions
      dep: HOUSE-02797, HOUSE-02904, HOUSE-03041 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D9 · est: 2
      accept: (1) the representative scenarios of `HOUSE-02402` on Linux, the Web and Android presets' scenes in the browser and on the device; (2) only misses of a platform's documented target and catastrophic regressions are fixed; no headroom work; (3) the Android measurement needs the device path: while BL-13 blocks it, this task stays open (a recorded blocker satisfies neither `HOUSE-03041` nor D10c), and the Linux and Web halves may be done and recorded meanwhile
      verify: `docs/performance-log.md`
      trace: absorbs `HOUSE-03072` (*work the optimisation list until every platform meets its budget with 15 % headroom*; the headroom requirement is removed)

- [ ] HOUSE-03074 — Final XNA-only audit
      dep: HOUSE-03071 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D11 · est: 0.5
      accept: the XNA-only lint, the strict-XNA gate, the `nm -C` symbol check and `check_anim_assets.py` pass, and a manual review confirms that no runtime source calls a CNA-specific graphics or model API (no `SupportsCapability`, no `*EXT*` call, no `Model::Tag` read); `docs/xna-deviations.md` grants nothing and the lint has no allowlist
      trace: absorbs `HOUSE-02787`

- [ ] HOUSE-03075 — Final documentation pass
      dep: HOUSE-03074 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D12 · est: 1.75
      accept: `cna-house.md`, `README.md` and `docs/*` describe the shipped software; `docs/testing.md` states what each suite covers, how to run it, how to add a case and how to accept a render-test change; `plan.md` states that the project is in maintenance mode
      trace: absorbs `HOUSE-02795` and `HOUSE-02600` (`docs/testing.md`)

- [ ] HOUSE-03076 — Final test and content pass
      dep: HOUSE-03075 · sys: ci · plat: CI · pri: MUST · zone: all · adv: D11, D12 · est: 1.25
      accept: the suites and gates green; the Web smoke (`HOUSE-02901`) and the Android checklist's smoke (`HOUSE-03038`); every asset used, no orphans, every licence recorded and `THIRD-PARTY-ASSETS.md` regenerated. No Web soak and no second long run
      trace: absorbs `HOUSE-03073` (the final content audit)

- [ ] HOUSE-03077 — The final release notes, the known-limitations list and the credits
      dep: HOUSE-03076 · sys: — · plat: ALL · pri: MUST · zone: all · adv: D12 · est: 0.75

- [ ] HOUSE-03078 — Tag the release; House Simulator enters maintenance mode
      dep: HOUSE-03077 · sys: — · plat: ALL · pri: MUST · zone: all · adv: DONE · est: 0.25
      accept: D1–D14 hold; the tag is made; `docs/handoff.md` records that feature development has stopped (rule R17)

---

## Optional and conditional backlog

**Not part of DONE. `pri: OPT`.** These are real ideas with real ids, kept so that nobody re-invents
them. Rule R13 governs every one of them:

* never scheduled while a MUST task is open, and never implemented because it "looks cheap";
* never counted in the estimate, and never blocking a gate, a release or DONE;
* never moved into the active plan silently. **Activation rule:** a Planning corrections entry names
  the DONE item that cannot be met without it (or, after DONE, the real use that demonstrated the
  need), shows that it fits under rule R14's ceiling, and only then changes its `pri:`.

Each entry shows its title and dependencies only; the full text is in the legacy ledger (every id
here is a legacy id) and in `plan.md` at `5927073`. A dependency on a cancelled or merged task is
replanned when the optional task is activated.

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

### Falling snow

- [ ] HOUSE-01791 — Implement snow particle motion: slow fall, lateral drift, per-particle flutter, rotation
      dep: HOUSE-01742 · sys: weather · plat: ALL · pri: OPT · was: MUST, M7 (moved by `HOUSE-03206`)
- [ ] HOUSE-01792 — Author the snowflake texture and material (alpha-blended, occluding)
      dep: HOUSE-01791 · sys: content · plat: TOOL · pri: OPT · was: MUST, M7 (moved by `HOUSE-03206`)

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

Without ids (each would need a Planning corrections entry under R9 and R13 first): an attract mode
that auto-walks the grand-tour route for demonstrations; seasonal decorations; additional unique
furniture or acquired storage furniture beyond the kit caps; the toys and shoes fill kits; special
zone audio loops (a basement hum, attic wind) and offline-derived footstep sets; progressive Web pack
streaming; additional browsers and devices; C4 for the cells made secondary by `HOUSE-03206`
(`B1_HALL`, `L0_OFFICE`, `L1_LANDING`, `L1_HALL`, `L1_MASTER_BATH`, `L2_LANDING`, `L2_HALL`,
`L2_GAMES`, `L2_SITTING`); C5 for the master bedroom or the attic room (the retired hero areas H4 and
H7); additional hero-area polish; more hero areas; a drivable car (still Q-06, a different project).

---

## Cancelled or merged by the final scope reduction (`HOUSE-03206`)

54 ids left the active plan on 2026-09-22 under
[ADR-0016](docs/decisions/ADR-0016-final-scope-reduction.md). **Merged** means the named active task
now carries the old task's still-required purpose in its own `accept:` and names the old id in its
`trace:` line; the old id is complete when that task is. Nothing that was required for DONE was
dropped silently. The earlier text of every id is `plan.md` at `174f2ba`. Ids are never reused.
The first two reductions' cancellations are in
[`docs/history/scope-reductions-2026-09-21.md`](docs/history/scope-reductions-2026-09-21.md).

| Old id(s) | Now | Why |
|---|---|---|
| `00978` | merged into `00977` | one seating-and-tables group (cap 7) instead of two |
| `00980` | merged into `00985` | storage furniture is generated only; no acquired wardrobes, dressers or bookcases |
| `01005` | merged into `01002` | the two double bedrooms share one recipe and arrangement |
| `01004` | merged into `01003` | the child's and teenager's rooms share one recipe; "untidy" is an arrangement variant |
| `01008` | merged into `01007` | the balconies are dressed with the `Z-L1` stores in one small task |
| `01012` | merged into `01011` | `L2_GAMES` and `L2_SITTING` became secondary cells |
| `01022` | merged into `01021` | the three basement service rooms are one equipment pass |
| `01019` | merged into `01017` | the attic stores share one storage recipe |
| `00997` | merged into `00996` | `L0_STOR` and the main-stair foot are dressed with the WCs |
| `03341` | merged into `00998` | the garage and its loft are one task |
| `01026` | merged into `03342` | the shed is dressed with the rest of the rear exterior |
| `01029`, `00913` | merged into `01030` | the chunk rebuild, the furnished re-bake and its S1/S2 seam fixes are one deliverable |
| `03453` | merged into `03444` | hero area H4 retired: the master bedroom is a main cell at C4 |
| `00992`, `00993` | merged into `00989` | the already-dressed family room, dining room and sunroom reach C4 in one bounded pass |
| `00987` | merged into `00986` | the foyer, porch and hall are one hero pass |
| `03445` | **cancelled** | `L2_LANDING`, `L2_HALL`, `L2_GAMES` and `L2_SITTING` became secondary cells; the library is `Z-L2`'s showcase room. Their C4 is in the optional backlog |
| `01745` | merged into `01743` | the rain texture is part of the rain deliverable |
| `01755` | merged into `01744` | the coverage test is the coverage task's own proof (rule R16) |
| `01653` | merged into `03520` | one environment capture and render set instead of a capture set plus a render set |
| `01791`, `01792` | **optional backlog** | falling snow does not block DONE; a snow state shows its overcast sky and fog |
| `01917` | merged into `01911` | the bank loader is part of the audio system |
| `01918`, `01938` | merged into `01919` | the round-robin selector and the cadence test belong to the footstep director |
| `01924` | merged into `01922` | the day/night bed is part of the one ambience director |
| `02000` | merged into `01925` | the sky-exposure attenuation is part of the weather layers |
| `02519`, `02520` | merged into `02516` | the audio and controls sections are rows of the one settings screen |
| `02525` | merged into `02523` | the controls hint is part of the menu flow |
| `02530` | merged into `02527` | the UI render tests are the layout task's own proof (rule R16) |
| `00915`, `02451` | merged into `02403` | memory and load-time measurement are part of the one baseline measurement |
| `02594` | merged into `00493` | the tolerance report and difference image are what makes flaky gates diagnosable |
| `02600` | merged into `03075` | `docs/testing.md` is written in the final documentation pass |
| `02786` | merged into `02781` | the TODO rule is one line of the DONE audit |
| `02791` | merged into `02790` | the no-audio-device run is part of the clean-profile run |
| `02842` | merged into `02843` | one audit of desktop-only assumptions |
| `02855` | merged into `02891` | the spike is how the Emscripten preset is proved |
| `02851` | merged into `02850` | pack reduction happens only where the pack check fails |
| `02896` | merged into `02895` | the audio gesture gate is checked with the canvas display |
| `02905` | merged into `02904` | a separate "phase review and commit" task is process duplication (rule R16) |
| `02954` | merged into `02991` | the `IInputSource` check is part of the touch source |
| `02993` | merged into `02992` | the look region ships with the stick |
| `02996`, `02997` | merged into `02995` | the HUD visibility rule and `--force-touch` ship with the buttons |
| `02952`, `02953` | merged into `02951` | one BL-13 gate instead of three consecutive gates |
| `02958` | merged into `03033` | the package-size budget is checked by the content-delivery task |
| `02959` | merged into `03036` | the lifecycle design and its wiring are one task |
| `03042` | merged into `03041` | a separate "phase review and commit" task is process duplication (rule R16) |
| `03072` | merged into `03071` | measure and fix only real misses, in one task; the 15 % headroom was already removed |
| `03073` | merged into `03076` | the final content audit is a gate run in the final test pass |

**Canonicalised.** Every other open task was rewritten in place so that its title and `accept:`
state the current work: for example `HOUSE-01920` *Wire the 20 footstep surface sets* is now *Wire six
broad footstep surface categories*, `HOUSE-00976` *the 16 bathroom fixture models* is now *(cap 4)*,
`HOUSE-02521` *the Simulation tab (… pets, … slots)* is now *the Environment section*, `HOUSE-00990`
*(… 62 containers)* is now *Bring the kitchen to C5*, and `HOUSE-03446` *Bring the attic room to C5*
is now *to C4*.

---

## Scope-reduction cleanup candidates

Infrastructure that was built or imported for scope that is now removed. **Nothing here was deleted
by any of the three reductions, and removing a candidate is not part of the estimate:** it is done
only when the candidate starts to cost (as `HOUSE-03301` does for the pet graph), or after DONE as
maintenance (rule R17). Classification: **keep** (harmless or still useful), **deprecate** (do not extend;
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


## Remaining-work estimate

Agent-hours, recomputed by `HOUSE-03206` from the repository on 2026-09-22. **Realistic** is the sum
of the open tasks' `est:` values. **Optimistic** is 0.85 × realistic. **Pessimistic** is 1.10 ×
realistic for ordinary estimation error **plus** the named risk reserves below, each charged to its
milestone. Optional work is not counted. The *before* columns are the open MUST tasks at `174f2ba`.

| Milestone | Tasks before | Hours before | **Tasks now** | Optimistic | **Realistic** | Pessimistic | Reserve |
|---|---|---|---|---|---|---|---|
| M1 Whole-property traversal | 4 | 9 | **0** | 0 | **0** | 0 | — |
| M2 Architectural completion | 8 | 18 | **0** | 0 | **0** | 0 | — |
| M3 The reusable furnishing kit | 16 | 33 | **0** | 0 | **0** | 0 | — |
| M4 Dressing everywhere | 40 | 48.75 | **0** | 0 | **0** | 0 | R-D +3 |
| M5 Baseline lighting everywhere | 12 | 24.5 | **0** | 0 | **0** | 0 | — |
| M6 Main cells and hero areas | 20 | 32.5 | **0** | 0 | **0** | 0 | — |
| M7 A compact environment | 13 | 21 | **0** | 0 | **0** | 0 | — |
| M8 Atmospheric audio essentials | 11 | 14.5 | **4** | 4.25 | **5** | 5.5 | — |
| M9 Application shell | 10 | 12.5 | **0** | 0 | **0** | 0 | — |
| M10 Performance | 6 | 13 | **2** | 5.1 | **6** | 10.6 | R-C +4 |
| M11 Final defect pass | 3 | 11 | **3** | 2.75 | **3.25** | 3.58 | — |
| M12 Representative tests | 4 | 5 | **0** | 0 | **0** | 0 | — |
| M13 Linux desktop release | 10 | 13.25 | **8** | 8.75 | **10.25** | 11.28 | — |
| M14 Web | 17 | 24.75 | **5** | 6.2 | **7.25** | 13.98 | R-B +6 |
| M15 Android | 23 | 29 | **5** | 6.0 | **7** | 15.7 | R-A +8 |
| M16 Final release | 8 | 9 | **6** | 5.5 | **6.5** | 7.15 | — |
| **Total, all three platforms** | **205** | **318.75** | **33** | **38.5** | **45.25** | **67.78** | +21 |

The remaining forecast has fallen below the reduction's initial 210–250 h realistic target as work
completed, and the pessimistic remaining total is under the **280 h hard ceiling** (rule R14) by
212.22 h. Adding the 181.75 task-hours completed since `HOUSE-03206` gives a ceiling projection of
249.53 h, 30.47 h under the limit. That margin is
small on purpose: the ceiling is a limit, not a budget to fill.

| Reserve | Milestone | Hours | Risk |
|---|---|---|---|
| R-A | M15 | 8 | House Simulator's device path after the CNA graphics gate: build, traversal and performance on Android |
| R-B | M14 | 6 | Web memory or package size forces deeper content cuts; Emscripten workarounds |
| R-C | M10 | 4 | performance needs two of `HOUSE-02404`'s candidate techniques |
| R-D | M4 | 3 | a few rooms (the library shelving, the cinema, the workshop) need more than their recipe estimate |
| R-E | M1 | 0 (retired at G1) | the grand tour and sweep stayed within M1's budget plus gate review |

**By area,** against the owner's sanity reference for this pass: traversal 0 h (complete); architecture
4 h (≈ 18); furnishing kit 19 h (18–22); whole-house furnishing 31 h (30–38); lighting 19.75 h
(18–22); C4/C5 polish 19.75 h (18–24); environment 4.75 h; audio 9 h (8–10); shell 5.25 h
(8–10); performance 9.5 h (≈ 10); final polish 7 h (6–8); tests 3.5 h (≈ 5); Linux 10.25 h
(11–13); Web 10.5 h (18–22); Android 23.75 h (23–27); release 6.5 h (6–8).

**Where the 92.5 h came from** (realistic, before → now):

1. **Furnishing** (M3 + M4, −30.75 h). Kit caps from 62 to 32 acquired models plus one art set;
   storage furniture generated only; four fill kits instead of six; nine small room tasks merged
   into the neighbouring task of the same zone, and each room estimate set by its tier.
2. **Main cells and hero areas** (M6, −12.75 h). Seven hero areas → five; nineteen main cells →
   twelve; the ground floor's already-dressed rooms finished in two bounded passes instead of five.
3. **Platforms** (M14 + M15, −10.5 h). Gate triples, "review and commit" tasks and split budget
   checks merged; Web progressive streaming cut. Both platforms keep every step real release
   operation needs.
4. **Environment** (M7, −7.75 h). Falling snow to the optional backlog; tests and textures merged
   into the deliverables they prove; fog reuses the existing `MaterialBinder` path.
5. **Audio and shell** (M8 + M9, −9.5 h). One audio system, one footstep director, one ambience
   director; one settings screen whose sections are rows, not tasks; no offline sample derivation
   and no special zone loops.
6. **Lighting** (M5, −4.75 h). Lighting by room-type preset; the chunk rebuild, re-bake and seams as
   one deliverable.
7. **Architecture, performance, tests, polish, release** (M2, M10, M11, M12, M13, M16, −17.5 h).
   Utility construction plain; one baseline measurement instead of three; the M11 polish budget
   8 h → 4.5 h; test infrastructure folded into the feature tasks; duplicate audits merged.

M1 grew by 1 h: `HOUSE-03228` gives the two integration failures found by `HOUSE-03223` a task, so
G1 cannot pass while they are open.

**Critical path.** M1 (the sweep `03227`, **G1**) → M2 (seven
zone tasks, **G2**) → M4 (31 tasks on the M3 kit, the dressing checkpoint) → M5 (props versus bake,
the furnished re-bake, zone lighting, **G3**) → M6a (**G4**) → M6b (**G5**) → M11 → M13 → the Web and
Android verification blocks → M16. M3 must be ready before M4 starts; it may begin at any time under
R1 exception (b). M7–M10, M12, the Web bring-up and the Android readiness block interleave as Track B
under R5.

**What could still explode** (watch these at every R10 reassessment):

1. **House Android after the CNA graphics gate** (`HOUSE-03033`, `03037`, `03038`).
   CNA's sample and House's clear-color APK now draw on the emulator, but House's
   content, real traversal and performance are still unverified; the reserve is 8 h
   for the device path. If this path costs more,
   R15 cuts depth elsewhere, never the platform. House Simulator is **not DONE** until its
   own device path passes.
2. **The full Emscripten build** (`HOUSE-02892`, 5 h) and Web memory (`HOUSE-02898`, `02850`).
3. **Props versus the bake** (`HOUSE-03402`, 4 h) and the furnished re-bake (`HOUSE-01030`, 4 h):
   a second iteration of either is the likeliest lighting overrun.
4. **Performance** (`HOUSE-02404`): the furnished house has never been measured. Two techniques is
   the reserve; a streaming scheme (if `HOUSE-02403` finds the footprint over its limit) is not in
   the estimate and would be a Planning corrections entry under R14.
5. **Kit acquisition** (M3): if licensed pieces of acceptable quality are not found within the caps,
   more is generated, not acquired beyond the caps.

---

## Planning corrections

Corrections made to `cna-house.md` or to this file during implementation, with the evidence that
forced each one. Nothing is changed silently. The 2026-09-06 → 2026-09-20 corrections are in the
legacy ledger; the two of 2026-09-21 (`HOUSE-03201`, `HOUSE-03205`) are in
[`docs/history/scope-reductions-2026-09-21.md`](docs/history/scope-reductions-2026-09-21.md).

### PC-2026-09-26 — Android performance measurement needs delivered scenes (`HOUSE-03037`)

* **Evidence:** `HOUSE-03036` verified that the current Android APK survives a real
  `surfaceDestroyed()`/`surfaceCreated()` cycle, but it presently contains no House
  content. Its displayed pixel is only the game's clear color. `HOUSE-03037` asks
  for the Android preset's representative *scenes* to meet their target; that
  measurement is not possible before `HOUSE-03033` delivers the scene packs.
* **Smallest correction:** add `HOUSE-03033` to `03037`'s `dep:` line. No acceptance
  criterion, platform, scope or estimate changes; a blank-surface frame cannot
  substitute for the existing D9/D10c measurement.

### PC-2026-09-26 — Put the desktop touch-test flag before its dependent HUD task (`HOUSE-02992`)

* **Evidence:** `HOUSE-02992` requires both stick and look to be tuned on desktop with
  `--force-touch`, but the flag was assigned to the dependent `HOUSE-02995`. Completing
  the former without a real flag would falsely satisfy its acceptance.
* **Correction:** `HOUSE-02992` now introduces the flag and mouse-as-one-finger test path.
  `HOUSE-02995` still owns the Platform-driven visibility rule, both buttons and the
  20:9 HUD screenshot; it integrates and verifies the existing flag rather than
  introducing it. No feature, task or estimate was added, and Android remains required.
* **Budget:** realistic remaining 60.5 h; pessimistic 84.55 h. Completed task estimates
  166.5 h plus pessimistic remaining project 251.05 h, below the unchanged 280 h ceiling.

### PC-2026-09-26 — Web touch controls wait for the M15 input source (`HOUSE-03721`)

* **Evidence:** desktop Chrome and Firefox could prove pointer lock, keyboard walk and focus
  handling, but `HOUSE-03721`'s touch-only criterion explicitly needs M15's unfinished
  `TouchSource` and HUD. CNA's XNA `TouchPanel` can report a connected device after touch
  input; the keyboard/mouse source only maps a first touch to menu pointer input. Claiming
  touch-only walking now would be false.
* **Correction:** keep the desktop browser controls under permanent id `HOUSE-03721`, and put
  only its original conditional touch criterion into `HOUSE-03723`, dependent on `HOUSE-02995`.
  The Web DONE checklist now depends on both. The additional 0.75 h is a targeted R7/R15
  estimate for integration and browser verification, not a new feature or system.
* **Budget:** realistic remaining 66.5 h, pessimistic 91.1 h; 160.5 completed task-hours plus
  the remaining pessimistic forecast projects 251.6 h, below the unchanged 280 h ceiling.

### PC-2026-09-23 — Walk-only accessibility and traversal topology (`HOUSE-03226`)

* **Evidence:** the production controller cannot reach `L0_GARAGE_LOFT`: its floor is 2.75 m above
  the garage and the showcase deliberately has no ladder or jump traversal. The first complete
  controller tour then found four authored route obstructions: `STEPS_GARAGE` did not overlap the
  mudroom aperture; both basement service doors were behind the solid basement-stair wedge; the
  0.90 m-deep `L2_STOR2` connector's three leaves all swung inward and blocked the only route to
  `L2_BATH5`; and the attic flight ended in a 230 mm rise, above the controller's 220 mm step-up,
  while its store opening overlapped the 0.90 m landing by only 50 mm.
* **Correction:** classify the visible ladder-only garage loft as excluded from the walk-only
  accessibility manifest (91 → 90 accessible cells; five → six documented exclusions). Move the
  existing garage steps onto their aperture and the two existing basement doors to unobstructed
  positions. Widen `L2_STOR2` by 0.20 m at its shared boundary with `L2_SITTING` and swing its three
  existing leaves into their destination rooms. Set the attic going to 235 mm and align its
  existing store opening with the landing. Standing points were adjusted only where these facts
  changed the safe destination.
* **Result:** the deterministic tour derives 558 stops from the portal graph and manifest, drives
  the real controller through all eight flights and all 90 accessible cells, and returns to the
  road in 90,614 steps / 10.28 s with zero boundary, floor-loss or obstacle-penetration failures.
* **Preserved:** every floor, the garage and loft architecture, every room, the exterior and all
  three platforms remain. No room, door, feature or system was added, and the 280-hour ceiling was
  unchanged. This is the smallest evidence-driven R15 correction, not another scope reduction.

### PC-2026-09-22 — Final scope reduction: canonical tasks, five hero areas, a 280-hour ceiling (`HOUSE-03206`, ADR-0016)

* **Evidence:** after two reductions the plan held 205 open MUST tasks and 318.75 realistic hours
  (pessimistic ≈ 393 h), above the owner's target of about 220 h with a 280 h ceiling. Its active
  tasks still carried their legacy titles ("the 16 bathroom fixture models", "the 20 footstep
  surface sets", "62 containers", "the 5 tabs … pets … slots") with up to five `amended:` lines each,
  so an agent had to apply several generations of amendments to know what was required. Seven hero
  areas and nineteen main cells, a kit of 62 acquired models, falling snow, separate test, review and
  "phase review and commit" tasks for single deliverables, and three consecutive BL-13 gates
  remained. Two failing integration tests were recorded only in the handoff.
* **Correction:** every active task canonicalised; 51 ids merged into the task that now carries their
  purpose, one cancelled, two moved to the optional backlog, one added (`HOUSE-03228`); hero areas
  7 → 5 (H4 and H7 retired, their cells main) and main cells 19 → 12 (`docs/zones.json`,
  `zone_scoreboard.py`, `capture_review.py`); kit caps 62 → 32 models plus one art set; the M11
  budget 8 → 4.5 h of S2/S3 work; rules R8, R9, R12 and R13 tightened, and R14 (budget ceiling), R15
  (final scope, targeted corrections only), R16 (validation lives in the task) and R17 (maintenance
  mode; CNA stays CNA) added; the Definition of DONE gains D14 (bounded polish done) and splits D10
  per platform. The history sections of the first two reductions moved verbatim to
  `docs/history/scope-reductions-2026-09-21.md`.
* **Result:** 152 open tasks; ≈ 192 / 226 / 273 agent-hours (optimistic / realistic / pessimistic).
  No breadth was cut: every floor, the basement, the attic, the garage, the exterior, Linux, Web and
  Android stay.
* **Preserved:** completed work stays, and nothing in the repository was deleted. The retired hero
  ids and every merged id stay reserved.
* **Android correction (same day, owner review):** D10 used to accept "`HOUSE-02951` recording
  BL-13" as proof, so an upstream blocker could be read as satisfying Android. That contradicted
  Android being a first-class DONE target. D10c, the M15 exit, `HOUSE-02951`, `HOUSE-03071`,
  `HOUSE-03076` and the risk list now say it plainly: only a working device path (`HOUSE-03038`)
  satisfies D10c; a blocker keeps the device tasks open and House Simulator not DONE.
* **This is the final proactive scope reduction.** Later changes are targeted corrections under R15.

---

## Deferred and explicitly out of scope

Recorded so nobody has to re-derive the decision. The legacy rows are kept, and the 2026-09-21 and
2026-09-22 rows are added.

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
| Falling snow | **Optional** 2026-09-22 | ADR-0016: a snow state shows its overcast sky and fog |
| Hero areas H4 (master bedroom) and H7 (attic room) | **Retired** 2026-09-22 | ADR-0016: both are main cells at C4; five hero areas remain |
| Acquired storage furniture; kit models beyond the caps; the toys and shoes fill kits | **Removed** 2026-09-22 | ADR-0016: storage furniture is generated; 32 acquired models plus one art set |
| Special zone audio loops; offline-derived footstep sets | **Removed** 2026-09-22 | ADR-0016 |
| Progressive Web pack streaming | **Removed** 2026-09-22 | ADR-0016: one preload with a progress screen |
| Separate validate, review, cross-check and "phase review and commit" tasks for one deliverable | **Removed** 2026-09-22 | ADR-0016, rule R16 |
| Further broad replanning passes | **Not planned** 2026-09-22 | ADR-0016, rule R15: `HOUSE-03206` is the final proactive scope reduction |
