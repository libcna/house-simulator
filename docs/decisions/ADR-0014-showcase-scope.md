# ADR-0014 — An architectural showcase, not a life simulator

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-21 |
| **Task** | `HOUSE-03201` |
| **Depends on** | [ADR-0001](ADR-0001-xna-only.md), [ADR-0004](ADR-0004-portal-visibility.md), [ADR-0005](ADR-0005-data-driven-world.md) |
| **Supersedes in part** | [ADR-0008](ADR-0008-save-format.md) (the household delta save), [ADR-0010](ADR-0010-room-aware-audio.md) (the portal-path audio solver) |
| **Owns** | `plan.md` (Direction, Definition of DONE, Scheduling rules); `cna-house.md` scope amendment |

## Context

The approved 2026-09-06 design described `cna-house` as an inhabited-house *simulation*: 640
interactables, 214 openable containers, working plumbing, toilets with a waste-state model,
appliance programmes, three televisions, a dog and a cat with navigation and behaviour, a
customisable third-person avatar with 18 animation clips and foot IK, and a delta save of the
whole household. That was 1 349 tasks across 53 phases.

After two weeks, 674 of those tasks were complete, and the evidence of what the project had
become was clear:

* Every placed prop, 133 in all, stood on the ground floor (`L0`) or outside. The basement
  (15 cells), both upper floors (39 cells) and the attic (6 cells) held **zero** furniture.
  Only one prop stood on `L1`: a balcony lantern.
* The Visual Convergence Sprint (2026-09-14 → 2026-09-20) ran **102 review rounds**, and every one
  of them inspected the same ground-floor route. No view had ever been captured inside the
  basement, the `L1` or `L2` rooms, the garage or the attic.
* Not one of the 26 interaction-framework tasks, 22 door/window-behaviour tasks, 83 container,
  kitchen, plumbing, toilet and television tasks, 37 pet tasks or 75 avatar and animation tasks
  had started. Together with persistence and Reset House (43 tasks) they made up 286 of the 764
  open tasks. None of them makes the house more complete to walk through.
* The data already existed for a complete, walkable five-level house: shell, materials and
  daylight and artificial lightmaps for all 78 interior cells, eight flights proved walkable both
  ways, a bounded property, and a 60-building neighbourhood. The missing work is breadth:
  furnishing, dressing, lighting and review of the areas nobody had looked at.

The project owner redirected the project on 2026-09-21.

## Decision

**`cna-house` is a polished, atmospheric, multiplatform architectural and graphics showcase for
CNA. The player walks through a large, detailed house (basement, ground floor, both upper floors
and attic), its garage, garden, street and neighbourhood, and looks at it.** Its value is a
complete, convincing environment and what that environment demonstrates about CNA. It no longer
depends on how many objects have gameplay code behind them.

1. **Removed from scope:** the dog and the cat; the visible player avatar, its customisation and
   every character animation; the interaction framework (targeting, prompts, `IBehaviour`),
   pick-up/carry/place, seats, containers and their contents, the appliance, kitchen, plumbing,
   water, toilet and television simulations; door, window and switch *gameplay*; household-state
   persistence and Reset House; the portal-path audio solver; audio that exists only to serve
   those systems.
2. **Objects are static dressing.** A toilet looks like a toilet, a refrigerator looks like a
   refrigerator, and cupboards stay closed. Static furniture, fixtures and dressing are *raised* in
   priority, because they are what the player sees.
3. **Doors and gates have static poses.** Most interior doors rest open. Every leaf collides where
   it is drawn, and every portal's visibility state follows the pose. Nothing is made
   inaccessible by the removal of interaction.
4. **Lights follow an automatic schedule** plus the debug console, not wall switches. The dusk
   sensor already drives exterior lights.
5. **Persistence is settings** (already implemented) plus an optional small session file (player
   pose, clock, weather) behind the existing `ISaveStore`.
6. **Retained in full:** first-person exploration and its controller; portal visibility;
   collision; materials and baked lighting; sky, sun, moon, stars, time of day, seasons, weather
   (rain, snow, storm, wind, hail) and their visuals; lightweight atmospheric audio (footsteps,
   room tone, exterior ambience, weather); LOD, impostors and measured optimisation; tests; and
   **Linux desktop, Web and Android** as platforms. Web and Android are not dropped.
7. **Breadth before depth.** Completion is measured per zone on a ladder (traversable →
   architecture → dressed → lit → showcase baseline → polished). A zone may not be worked more
   than one level ahead of the least-complete zone. Minor polish is deferred until every zone
   reaches the showcase baseline. `plan.md` states the exact rules.

## Alternatives rejected

* **Keep the simulator scope and reprioritise.** Rejected. Even at the sprint's pace, the removed
  systems were about half the remaining plan, and several of them (avatar, pets, 640
  interactables) are each a large project. They also tax every other task: each furnishing change
  had to keep the pet waypoint graph valid.
* **Keep the interaction framework "for doors only".** Rejected. Static poses give the same
  accessibility without targeting, prompts, a behaviour registry or persistence, and a leaf that
  never moves cannot desynchronise from its collision or its portal.
* **Drop Web and Android to finish sooner.** Rejected. CNA is a multiplatform framework, and the
  showcase exists to demonstrate that. Platform limits are handled with quality tiers, not by
  deleting the platform.
* **Delete the now-unused code and data in the same change.** Rejected. That would be a large
  refactor with its own risk. `plan.md` lists the cleanup candidates and classifies each as keep,
  deprecate or remove later.

## Consequences

* 674 legacy tasks stay complete. Of the 764 legacy tasks still open, 385 are cancelled with a
  recorded reason, 373 are carried into the new plan with their ids, and 6 are deferred until
  after DONE (optional). 51 new tasks join them. Task ids remain permanent. The legacy ledger is
  frozen verbatim in [`docs/history/plan-legacy-2026-09-21.md`](../history/plan-legacy-2026-09-21.md).
* `cna-house.md` §§ 45–47, 50–58, 60–61, 64–66 and the 640-interactable, pet and avatar rows of
  its other tables describe removed scope. They remain as design history under a scope banner and
  are not requirements.
* ADR-0008's delta save of household state is not built. Its versioning and migration policy still
  governs `settings.json` and the session file. ADR-0010's portal-path solver is not built.
  Positional ambient loops are gated by cell and one open-portal hop, and weather ambience is
  routed by sky exposure.
* ADR-0005 stands. Its closed interaction-expression vocabulary is frozen: it is not extended.
* Existing infrastructure for removed features stays in the repository for now and is listed in
  `plan.md` under *Scope-reduction cleanup candidates*. Examples are `src/animation/`, the
  `.chanim` sidecar, the human base meshes, the pet navigation graph, and the faucet, toilet,
  appliance and television rows of `interactables.json`.
* The Definition of DONE in `plan.md` requires no animal, avatar, household interaction or
  appliance behaviour.
