# ADR-0015 — Quality tiers, a reusable kit and a compact feature set

| | |
|---|---|
| **Status** | Accepted; superseded in part by [ADR-0016](ADR-0016-final-scope-reduction.md) (five hero areas instead of seven, twelve main cells, falling snow optional, the polish budget) |
| **Date** | 2026-09-21 |
| **Task** | `HOUSE-03205` |
| **Depends on** | [ADR-0014](ADR-0014-showcase-scope.md) |
| **Supersedes in part** | [ADR-0014](ADR-0014-showcase-scope.md): decision 6's "retained in full" environment and audio lists, and decision 7's "showcase baseline everywhere" ladder |
| **Owns** | `plan.md` (Non-goals, Definition of DONE, Quality tiers, Scheduling rules R1–R13, Optional after DONE); `cna-house.md`'s second scope amendment |

## Context

ADR-0014 turned `cna-house` from a life simulator into an architectural showcase and cut 385
tasks. The plan it produced (`HOUSE-03201`, `plan.md` at `5927073`) still held 424 open tasks and
estimated ≈ 635–986 agent-hours to DONE. Most of that was depth, not breadth:

* **C5 everywhere.** Every one of the eleven zones had to reach the showcase baseline, with
  secondary dressing and day, night and overcast views, and then get a C6 polish rotation. Every
  closet, WC, storage room, plant room and side yard carried the living room's quality bar.
* **Unique content.** An anti-repetition validator allowed at most 14 uses of any model in the whole
  house and 6 in a cell, and density bands per cell. Both forced bespoke or additional models for no
  visible gain in a house this large.
* **A weather simulator.** 66 environment tasks: snow accumulation and melt driven by temperature and
  sun, hail with bounce, a Poisson lightning process with thunder timed to the strike distance,
  occlusion-query sun glare and a seven-sprite lens flare, seasonal vegetation with leaf fall,
  vegetation sway, and six tuned sky states.
* **An audio simulation.** 28 tasks: per-cell room tone, positional hums gated through portals,
  clocks, creaks driven by temperature change, seasonal beds, and four sourcing groups blocked on a
  Freesound account.
* **Exhaustive verification.** Render-test matrices of 64, 48, 80 and 76 scenes, every light group
  switched, 30 simulated days, a 2-hour soak as a recurring requirement, a 15 % performance headroom
  on every platform, and three Android devices across GPU vendors.

The project owner asked on 2026-09-21 for a second reduction to about 275–415 agent-hours, with
about 350 expected, **without** cutting any floor, the basement, the attic, the garage, the garden,
the exterior, Web or Android.

## Decision

**Cut depth, preserve breadth. Complete the whole house to a common baseline, then beautify a few
hero areas.**

1. **Quality tiers replace "C5 everywhere".** Every accessible cell is classified hero, main,
   secondary or utility (`plan.md`, *Quality tiers*). Every accessible cell reaches **C3**, which now
   means dressed *and* lit to a baseline. Main rooms and main areas (19 cells) reach **C4**. Seven
   hero areas spread across the levels reach **C5**: the front approach and porch, the entry and
   living room, the kitchen, the master bedroom, the library, the basement cinema and the attic
   room. C6 is retired as a target. The gates follow the ladder: G3 baseline complete, G4 main
   rooms, G5 hero areas.
2. **A reusable modular kit.** Furniture is generated or acquired in capped groups and reused freely,
   varied by tint, scale, dimensions, arrangement and small accessory swaps. There is no uniqueness
   or density quota. Bespoke per-object authoring is allowed only in hero areas, at most one piece
   each. Clutter density follows the tier.
3. **A compact environment.** Required: time of day (automatic and controllable), day and night, the
   sun, moon and stars, clear, overcast and rain (with rain kept out of covered areas, wet surfaces
   and fog), and simple falling snow. Snow cover, hail, lightning and thunder, glare and lens flare,
   vegetation sway, grass cards, seasonal vegetation, puddles and splashes move to *Optional after
   DONE*. The storm and hail weather states stay in the data and render as heavy rain.
4. **Audio essentials.** Footsteps on six broad surface categories, one interior tone, exterior day
   and night beds, rain and wind, at most two special zone loops, and volume settings, all from the
   imported NOX collection and offline derivatives. There are no positional sources, clocks, creaks,
   appliance hums or seasonal beds, and no new sourcing.
5. **A demo's shell.** *Start*, pause, one settings screen (graphics, audio, controls, environment),
   credits, quit, a controls hint, Web and touch controls. No key remapping, toasts, vignettes or new
   debug overlays. Session resume is optional.
6. **Representative verification.** The existing suites and gates stay. New tests are representative
   sets (interiors by zone and hero area, the environment, the UI), the grand tour, a 20-minute
   stability run and one 2-hour run before DONE. Performance is measured on eight representative
   scenarios and optimised only where a documented target is missed, with no headroom margin.
7. **A bounded final defect pass.** Polish outside the hero C5 tasks is one review and one fix pass
   with a hard budget, driven by severity: S1 and S2 must be fixed, S3 only when cheap, repeated,
   in a hero area or in a release capture, and S4 never blocks DONE.
8. **Web and Android stay first-class DONE requirements with limited validation breadth.** Web:
   Chrome and Firefox, a representative traversal, acceptable performance and a headless smoke test.
   Android: one representative device, or the best available emulator, touch controls, the
   representative traversal and acceptable performance. The upstream BL-13 provision of ADR-0014 is
   unchanged.
9. **Non-goals are written down.** `plan.md` lists what is not required for DONE and states that these
   are not postponed prerequisites. Optional work has its own section, is never scheduled while a
   MUST task is open, and never counts towards the estimate (rule R13).

## Alternatives rejected

* **Cut floors, the attic, the basement, the exterior, Web or Android.** Rejected by the owner and on
  the merits: the showcase's value is a complete property on every platform CNA targets. Depth is
  cheaper to cut and hurts less.
* **Keep C5 everywhere and shave estimates.** Rejected: an estimate that fits only because it was
  lowered is not a plan. Work was removed until the estimate became credible.
* **Keep the anti-repetition rule with a higher limit.** Rejected: any quota converts "the room reads
  right" into a counting exercise and still forces additional models. Tint, scale and arrangement
  vary reused pieces well enough at walking distance.
* **Drop snow entirely.** Rejected: falling snow reuses the rain particle system for about two hours
  of work, and the weather model already produces snow. Accumulation and melt are what was
  expensive, and they are optional.
* **Delete the systems that are no longer extended** (snow depth, the storm and hail archetypes, the
  debug overlays, the cube-map tooling). Rejected: they work, cost nothing to keep, and the optional
  tasks would use them. `plan.md` lists them under *Scope-reduction cleanup candidates*.

## Consequences

* Of the 424 tasks open at `5927073`, 208 are kept (amended where the scope changed), 146 are
  cancelled with a recorded reason, and 70 move to *Optional after DONE* together with the 6 tasks
  ADR-0014 had deferred. 4 new tasks are added: the G4 gate and three hero passes. 212 tasks remain
  open. Every open task carries `est:`, and the estimate is their sum: ≈ 272 / 336 / 411
  agent-hours (optimistic / expected / conservative), all three platforms included.
* Completion levels changed meaning. `HOUSE-03201`'s C3 (dressed) and C4 (lit) are merged into the
  new C3; its C5 is split into C4 (main) and C5 (hero); C6 is retired. The gate *tasks* keep their
  ids, but `HOUSE-03380` is now the dressing checkpoint, `HOUSE-03420` is G3, `HOUSE-03452` (new) is
  G4 and `HOUSE-03480` is G5. Notes written before this decision use the old numbering.
* The ground floor still goes last. Rule R4 now holds at every stage: nothing on `Z-L0M` before G3
  except S1 fixes and one lighting task, and its C4 and C5 tasks depend on every other zone's.
* `cna-house.md` §32.4 (glare), §38–§41 (snow, storm, hail, wind), §59.1–§59.3 (density,
  anti-repetition, habitation), §62 and §64 (audio) describe more than DONE requires. They remain as
  design for the optional work under a second scope banner, and are not requirements.
* ADR-0010's cell-gated positional loops, which ADR-0014 retained, are cut as well. Only the
  sky-exposure gain of the weather layers remains of room-aware audio.
