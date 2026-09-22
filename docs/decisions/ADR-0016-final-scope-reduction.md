# ADR-0016 — The final scope reduction: canonical tasks, five hero areas, a 280-hour ceiling

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-22 |
| **Task** | `HOUSE-03206` |
| **Depends on** | [ADR-0014](ADR-0014-showcase-scope.md), [ADR-0015](ADR-0015-quality-tiers-and-compact-scope.md) |
| **Supersedes in part** | [ADR-0015](ADR-0015-quality-tiers-and-compact-scope.md): decision 1's seven hero areas and 19 main cells, decision 3's "simple falling snow" as a DONE requirement, decision 7's polish budget, and decision 8's reference to an upstream-blocker provision for Android |
| **Owns** | `plan.md` (Definition of DONE D1–D14, rules R14–R17, the kit caps, the optional and conditional backlog); `cna-house.md`'s third scope amendment |

## Context

After ADR-0014 and ADR-0015 the plan held 205 open MUST tasks and 318.75 realistic agent-hours
(about 393 pessimistic). The project owner asked on 2026-09-22 for a third and final pass: keep
the whole house and all three platforms, reduce depth until the remaining work is about 220 realistic
hours, and never more than 280 hours pessimistic.

The audit found:

* **Uncanonical tasks.** Active tasks still carried their legacy titles ("the 16 bathroom fixture
  models", "the 20 footstep surface sets", "62 containers", "the 5 tabs … pets … slots") followed by
  up to five `amended:` lines. An agent had to apply several generations of amendments to know what
  was required, and could implement a superseded requirement by reading the title.
* **Depth that the showcase does not need.** Seven hero areas and 19 main cells; an acquired kit of
  62 models plus 6 fill kits; falling snow as a DONE item; special zone audio loops and an
  offline-derived footstep set; a progressive Web pack fetch.
* **Process inflation.** Separate test, texture, review and "phase review and commit" tasks for
  single deliverables; three consecutive BL-13 gates; two content audits; the same measurement split
  across three tasks.
* **An untracked defect.** Two failing integration tests recorded only in the handoff.

## Decision

1. **This is the final proactive scope reduction.** No further broad replanning pass is scheduled.
   Later reductions happen only when a task proves unexpectedly expensive, a platform constraint
   makes a feature disproportionately costly, or the ceiling is threatened, each as one targeted
   Planning corrections entry (rule R15). Expansion is never automatic.
2. **Every active task is canonical.** Its title and `accept:` state the currently authorised work;
   a `trace:` line records absorbed ids and says where earlier wording lives. Earlier wording is
   superseded and is not implemented.
3. **Breadth is fixed.** The basement, the ground floor, both upper floors, the attic, the garage,
   the exterior and approach, and Linux, Web and Android all stay and are all required for DONE.
4. **Depth is reduced.** Five hero areas (front approach and porch, entry and living room, kitchen,
   library, basement cinema); `H4` and `H7` are retired and the master bedroom and attic room become
   main cells. Twelve main cells. An acquired kit of at most 32 models plus one wall-art set; storage
   furniture generated only; four fill kits. Falling snow, special audio loops and progressive Web
   streaming move to the optional backlog or are removed. The bounded polish pass drops from 8 to
   4.5 hours of S2/S3 work.
5. **Validation lives in the task** (rule R16). Standalone reviews are only the gates, the dressing
   checkpoint, the final walkthrough, the DONE audit and the per-platform checklists.
6. **A budget ceiling** (rule R14). Realistic 226 h, pessimistic 273 h; the hard ceiling is 280 h,
   measured as hours spent since 2026-09-22 plus the remaining pessimistic estimate. An overrun is
   resolved by cutting depth, never by raising the ceiling or re-estimating without cutting.
7. **Android is required, and an upstream blocker does not satisfy it.** Earlier plans let a
   re-verified BL-13 record stand in for the Android device path (D10). That contradicted Android
   being a first-class DONE target. Now only a working device path (`HOUSE-03038`) satisfies D10c.
   `HOUSE-02951` may establish that CNA blocks it; the device tasks then stay open, and House
   Simulator is not DONE until CNA lands its fix.
8. **Maintenance mode after DONE** (rule R17). Once the release is tagged, feature development
   stops. CNA defects are recorded and worked around where reasonable, never turned into a CNA
   roadmap inside this project.

## Alternatives rejected

* **Cut a floor, the attic, the basement, the garage, the exterior, Web or Android.** Rejected by the
  owner: the showcase's value is a complete property on every platform CNA targets.
* **Keep seven hero areas and shave estimates.** Rejected: estimates lowered without removing work
  are not a plan. Two hero areas were retired instead, and the upper floors still have one (the
  library) and the basement one (the cinema).
* **Keep the amendment chains and add a fourth.** Rejected: that is the problem this decision
  solves.
* **Archive the whole previous plan as a second document.** Rejected: two full plans invite reading
  the wrong one. The previous `plan.md` is available at commit `174f2ba`; only its history sections
  (cancellation lists, legacy mapping, earlier estimates and corrections) moved verbatim to
  `docs/history/scope-reductions-2026-09-21.md`.
* **Delete finished infrastructure for removed scope.** Rejected, as in ADR-0015: finished work
  costs nothing to keep; cleanup happens only when a candidate starts to cost.

## Consequences

* 205 open MUST tasks become 152: 51 ids are merged into the active task that now carries their
  purpose, one is cancelled (`HOUSE-03445`), two move to the optional backlog (`HOUSE-01791`,
  `HOUSE-01792`), and one is added (`HOUSE-03228`, the two integration failures). The estimate falls
  from 318.75 to 226.25 realistic hours (≈ 192 optimistic, ≈ 273 pessimistic).
* `docs/zones.json` records the new tiers; `tools/world/zone_scoreboard.py --check` enforces 11 hero
  cells, 12 main cells and the hero ids `H1`, `H2`, `H3`, `H5`, `H6`; `tools/visual/capture_review.py
  --check` enforces their review coverage.
* The Definition of DONE gains D14 (the bounded polish pass is complete) and splits D10 into D10a
  (Linux), D10b (Web) and D10c (Android). D5 no longer requires falling snow: a snow weather state
  shows its overcast sky and fog.
* `cna-house.md` gains a third scope amendment; where its sections require more than `plan.md`,
  `plan.md` governs.
