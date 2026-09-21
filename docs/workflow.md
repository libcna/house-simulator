# Workflow and definition of done

## One task, one commit

`plan.md` is the execution ledger. Work is picked from it, not invented beside it.

1. **Pick the next unfinished task whose dependencies are all complete, by `plan.md`'s scheduling
   rules (R1–R13).** Dependency order is real: a task's `dep:` line names what must exist first.
   Among eligible tasks, the rules decide: breadth before depth, the least-complete zone first,
   no cell above its quality tier's target, the ground floor last at every stage, at most three
   consecutive tasks per zone, and never an `OPT` task while a MUST task is open. Not the lowest
   id, and not the largest defect in the area you worked on last. Nothing on the plan's
   *Non-goals* list belongs in a task.
2. **Do the whole task**, including its `verify:` step.
3. **Tick the checkbox** `- [ ]` → `- [x]` in `plan.md`, in the same commit as the work.
4. **Commit once**, with the id in the message.

A batch of small, tightly related tasks may share a commit when splitting them would produce
commits that do not build or do not stand alone. The message then names every id.

## Commit messages

```
<type>: <what changed, imperative, one line>

<why, and anything a reviewer needs>

HOUSE-00020, HOUSE-00021
```

`<type>` is one of `feat`, `fix`, `docs`, `test`, `build`, `tools`, `content`, `refactor`,
`perf`, `chore`, `release`. The id line is last and lists every task the commit completes.

Never mention a task id in a commit that does not complete it; a search for an id must find the
commit that finished it and nothing else.

## Definition of done

A task is done when **all** of these are true. Not most.

- [ ] **It builds.** `cmake --build build` succeeds with `-Wall -Wextra -Wpedantic -Werror`.
- [ ] **Its own verification passes.** Whatever the task's `verify:` field names — a unit test, an
      integration test, a configure/build test, a runtime smoke test, a content validation, a
      static check, a screenshot scene — has been run, and the output is real.
- [ ] **The relevant broader suite passes.** At minimum the unit tests; the integration suite when
      the change touches a system's behaviour.
- [ ] **The gates are green.** `tools/ci/run_checks.sh`: layout, XNA-only, clang-format.
- [ ] **Acceptance criteria are satisfied, not approximately satisfied.** If one criterion is
      knowingly unmet, the box does not move — see *Blocked tasks* below.
- [ ] **Documentation is updated** where the task changed something a document states. Not
      "documentation was written"; *the documents that were made wrong by this change are right
      again.*
- [ ] **`plan.md` is updated** — the checkbox, plus any dependency, blocker or corrected criterion
      the work revealed, and the zone scoreboard when the task changed a zone's level (rule R10).
- [ ] **The diff is clean.** `git diff --check` passes; no build products, no probe binaries, no
      stray files; no sibling repository touched.

## Blocked tasks

A task that cannot be completed is not quietly skipped and not fraudulently ticked.

1. **Verify the blocker.** Reproduce it and capture the exact evidence — the error text, the
   measurement, the file and line.
2. **Record it** in `plan.md` under the task as a `blocked:` line naming the evidence, and, if it
   is a CNA limitation, add or update a row in `cna-house.md` §6.
3. **Leave the checkbox unticked.**
4. **Move to independent work** in the same milestone. One blocked task does not stop a session.

Never invent a workaround that violates the XNA-only rule to close a task. That is the one thing
this project cannot trade away.

## Correcting the plan

`plan.md` and `cna-house.md` are authoritative, and implementation is allowed to prove them wrong.
When it does:

* investigate until the contradiction is certain, not suspected;
* make the **smallest** correction that resolves it;
* record *why*, in the same commit;
* never deviate silently, and never redesign an accepted decision without evidence that it fails.

Task ids are permanent: never renumbered, never reused, never deleted. A cancelled task is struck
through and keeps its id. New work takes the next free id **in its milestone's reserved range**
(`plan.md`'s milestone index). Tasks completed or cancelled before 2026-09-21 are recorded in the
frozen legacy ledger, [`history/plan-legacy-2026-09-21.md`](history/plan-legacy-2026-09-21.md).

An architectural change additionally needs an ADR — a new one superseding the old, never an edit
that makes an accepted record say something different ([`decisions/`](decisions/)).

## Before you finish a session

```bash
tools/ci/run_checks.sh
git diff --check
git status
```

Then confirm, explicitly: no CNA or sibling repository was modified; `CNA_CNAEXT=OFF` was not
weakened; no forbidden symbol entered a runtime source; every ticked box is genuinely earned.
