# The content build

*`HOUSE-00216` builds it, `HOUSE-00217` is this page. The stage table in §3 is **generated** by
`tools/ci/build_content.py --docs` and checked by `--check-docs` in CI — do not edit it by hand,
and if it looks wrong, the graph in `default_stages()` is what is wrong.*

---

## 1. One command

```bash
cmake --build build --target content        # build what is out of date
cmake --build build --target content-plan   # print the plan, run nothing

tools/ci/build_content.py                   # the same, directly
tools/ci/build_content.py --dry-run
tools/ci/build_content.py --force           # rebuild everything
tools/ci/build_content.py --only world      # one group
tools/ci/build_content.py --json            # for a script
```

Neither target is part of `all`. A content build reads and writes `assets-src/` and `content/`, and
a code build should not.

## 2. Up to date means the same bytes, never a newer mtime

A stage is up to date when the SHA-256 of every input file, plus its exact command line, plus its
list of outputs, matches what the stamp recorded when those outputs were produced. `AGENTS.md` is
the reason, and it is not a stylistic one — its opening argument is that every avoidable rebuild is
irreversible flash wear on one SSD shared by ten agents, and mtime produces avoidable rebuilds in
both directions:

* **git does not preserve mtime.** A clone, a branch switch, a `git stash pop` — each stamps every
  touched file with the time it was written, so an mtime build rebuilds a content tree that has not
  changed at all. Here that is the whole of `assets-src/`.
* **mtime can also claim fresh when it is stale.** Restore an output from a backup, or let a tool
  write one without touching its input, and the output is newer than an input it does not match.

Stamps live in the build directory (`build/content-stamps.json`), never under `assets-src/`. A
corrupt stamp file means *rebuild*, never *crash*: it is a cache, and a cache that can break the
build is worse than no cache.

Three other things force a rebuild, and each has caught a real mistake:

* **the command line changed** — a stage run at `--samples 4` and one at `--samples 64` produce
  different content from identical inputs;
* **an output is missing** — trusting the stamp would leave the build reporting success over a file
  nobody can load;
* **`--force`**.

## 3. The stages, in build order

Every stage names *what it needs*, and the order is a topological sort of that — never the order
the stages happen to be written in. Adding a stage means naming its dependencies and nothing else.

Validators gate the generators by an explicit edge rather than by sorting: a gate that ran after
the generators would be reporting on assets the pipeline had already consumed.

<!-- BEGIN GENERATED: build_content.py --docs -->

| # | Stage | Group | What it does | Needs | Reads | Writes |
|---|---|---|---|---|---|---|
| 1 | **`anim`** | validate | single-skin models, and every sidecar binds | — | `assets-src/Models/**/*.glb` | — (a gate) |
| 2 | **`fonts`** | validate | every descriptor resolves to a vendored face | — | `assets-src/Fonts/*.spritefont` | — (a gate) |
| 3 | **`layout`** | validate | the directory-set and file-placement gate | — | `tools/ci/check_layout.py` | — (a gate) |
| 4 | **`manifest`** | validate | no unlisted file under assets-src/, no hash mismatch | — | `assets-src/assets.manifest.json` | — (a gate) |
| 5 | **`licences`** | validate | every row has a licence, a licence file and the booleans | `manifest` | `assets-src/assets.manifest.json`<br>`licenses/THIRD-PARTY-ASSETS.md` | — (a gate) |
| 6 | **`collision`** | world | rooms become walls; the layout and the _COL proxies | `anim`, `fonts`, `layout`, `licences`, `manifest` | `assets-src/world/*.json`<br>`assets-src/assets.manifest.json` | `content/world/collision.bin` |
| 7 | **`chunks`** | world | per-cell static prop batches | `anim`, `collision`, `fonts`, `layout`, `licences`, `manifest` | `assets-src/world/*.json`<br>`assets-src/Models/**/*.glb` | `content/world/chunks.bin` |
| 8 | **`coverage`** | world | the rain/roof coverage height field | `anim`, `collision`, `fonts`, `layout`, `licences`, `manifest` | `assets-src/world/*.json` | `content/world/coverage.bin` |
| 9 | **`nav`** | world | the pet waypoint graph | `anim`, `collision`, `fonts`, `layout`, `licences`, `manifest` | `assets-src/world/*.json` | `content/world/nav.bin` |
| 10 | **`skyexposure`** | world | per-cell sky and facade exposure | `anim`, `coverage`, `fonts`, `layout`, `licences`, `manifest` | `assets-src/world/*.json` | `content/world/skyexposure.bin` |
| 11 | **`snowshell`** | world | the snow shells over up-facing exterior surfaces | `anim`, `coverage`, `fonts`, `layout`, `licences`, `manifest` | `assets-src/world/*.json`<br>`assets-src/Models/**/*.glb` | `content/world/snowshell.bin` |

The command each stage runs:

```
anim           python3 tools/ci/check_anim_assets.py
fonts          python3 tools/ci/check_fonts.py
layout         python3 tools/ci/check_layout.py
manifest       python3 tools/ci/check_manifest.py
licences       python3 tools/assets/verify_licences.py --check
collision      python3 tools/world/build_collision.py
chunks         python3 tools/world/build_chunks.py
coverage       python3 tools/world/build_coverage.py
nav            python3 tools/world/build_nav.py
skyexposure    python3 tools/world/build_skyexposure.py
snowshell      python3 tools/world/build_snowshell.py
```

<!-- END GENERATED -->

## 4. The five outcomes, and why *skipped* is not *failed*

| Outcome | Means |
|---|---|
| **built** | it ran |
| **fresh** | its inputs, command and outputs are unchanged |
| **skipped** | one of its input patterns matches nothing yet |
| **blocked** | something it needs was skipped or failed |
| **FAILED** | the tool exited non-zero; the build fails |

Most of the world and Blender stages are **skipped** today: they read `assets-src/world/*.json`,
which phase 5 has not authored, and the house shell, which phase 6 has not generated. `make content`
has to be runnable now, so waiting for phase 5 is reported as waiting — not as success, and not as
failure. A build that quietly succeeded by doing nothing would be the worst of the five.

"No inputs" is **not** "no pattern matched". A stage that reads the layout *and* the asset manifest,
in a repository that has the manifest and not the layout, has one of the two and cannot run; the
first version of the runner launched `build_collision` on the strength of the manifest existing, and
it failed inside the tool — which reads as a broken build rather than as a wait. Every input pattern
must match at least one file, and the skip reason names the pattern that did not.

A failed stage **stamps nothing**, so the next run retries it, and its output is kept in the report
so the reason is in front of you.

## 5. Rebuilding one asset

**One stage:** run its command from the table above, directly. It is an ordinary tool with ordinary
arguments and no hidden state; the runner adds nothing but ordering.

**One stage and everything downstream of it:** delete its outputs and run `content`. A missing
output rebuilds that stage, and its dependents follow because their inputs changed.

**One asset inside a stage:** most tools take the asset as an argument —
`tools/blender/lod_gen.py IN.glb OUT.glb`. Do that, then run `content`, which will notice the
changed bytes and rebuild whatever reads them.

**Everything:** `--force`. Please have a reason; see §2.

## 6. Adding a stage

1. Add a `Stage(...)` to `default_stages()` in `tools/ci/build_content.py`: its name, group,
   command, input patterns, output paths, what it `needs`, and one line saying what it does.
2. Run `tools/ci/build_content.py --docs` to regenerate §3.
3. Run `tools/ci/build_content.py --selftest`.

The description is not decoration — §3 is generated from it, and the selftest requires every stage
to have one.
