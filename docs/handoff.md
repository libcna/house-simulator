# Session handoff — 2026-09-14

This is the authoritative handoff for the next agent. The owner explicitly wants a **fresh Sol
High context with a new optimisation target: make House Simulator visually convincing**. Do not
resume this session's habit of mechanically following the dependency DAG. Read the visual brief in
§5 first, inspect the current game on the real screen, and then select the highest-leverage
dependency-valid work.

## 1. Exact repository state

| Item | State |
|---|---|
| Repository | `/rv/data/development/github.com/libcna/house-simulator` |
| Branch | `develop` |
| Parent baseline | `8c40790` — `world: author all room palettes (HOUSE-00908)` |
| Handoff commit | The commit containing this file completes `HOUSE-00907` |
| Working tree | Clean after that commit |
| Active unfinished task | None |

Verify with `git branch --show-current`, `git rev-parse HEAD`, `git status --short`, and
`git log --oneline -5` before changing anything.

The active `build/` cache is usable and configured with:

```text
CNA_SOURCE_DIR=/rv/data/development/github.com/libcna/cna
CNAHOUSE_SHARP_RUNTIME_ROOT=/rv/data/development/github.com/libcna/sharp-runtime
CNA_CNAEXT=OFF
CMAKE_C_COMPILER_LAUNCHER=ccache
CMAKE_CXX_COMPILER_LAUNCHER=ccache
```

`../sharp-runtime` is on `next` at `0c82d9b888bd` and clean. At the last check, `../cna` was on
`native-platforms-integration` at `5f583a2bece0` with an unrelated removed video fixture from
another session. That sibling changed underneath this session, so verify it again but do not touch,
restore, stage or commit its state. This session did not modify either sibling.

## 2. Mandatory display and build rules

The owner requires **at most four CPU cores** for compilation and tests. Use the existing `build/`
directory and pin every expensive command to four cores:

```bash
PATH=/home/robertvokac/.pyenv/versions/3.11.9/bin:/usr/local/bin:/usr/bin:/bin \
CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv \
CMAKE_BUILD_PARALLEL_LEVEL=4 CTEST_PARALLEL_LEVEL=4 MAKEFLAGS=-j4 \
OMP_NUM_THREADS=1 PYTHON_CPU_COUNT=4 \
taskset -c 4,5,7,9 <command>
```

Keep Python 3.11.9 first in `PATH` when committing because the hook needs `jsonschema`. Never
bypass the hook. Graphical tests must add:

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
```

Do not use `DISPLAY=:99`; it is a real visible display here. Conversely, the next session should
launch `cna-house` normally on the real screen for the owner because that visual audit is explicitly
requested. Stop the game cleanly after inspection.

All project rules remain absolute: XNA-only runtime, `CNA_CNAEXT=OFF`, no sibling-repository edits,
the house is data rather than C++, and one completed task plus its `plan.md` checkbox per commit.

## 3. Work completed by HOUSE-00907

`HOUSE-00907` replaced the generated house shell's diagnostic placeholder materials with real
authored ids:

* each cell's floor, wall, ceiling and trim come directly from the `HOUSE-00908` palette fields;
* window glass comes from `layout.openings.json`, including obscured bathroom glazing;
* stair finish comes from `layout.stairs.json`;
* basement outer skin and chimney use brick water table, upper outer skin uses warm-white siding,
  roofs use shingles, eaves use white soffit, and metal uses balcony/gutter finishes;
* all 99 generated GLBs use 43 real material ids, with no surviving `BLOCKOUT_*` material;
* each glTF slot retains structured `materialId`, `surfaceClass`, and `lightmapReceiver` extras;
* `build_chunks.py` validates shell ids against `layout.materials.json` and takes `alphaMode` from
  the real row rather than guessing that glass is transparent from a class name;
* one real finish can be both a receiver and detail, so keeping `surfaceClass` separately prevents
  bluestone floor/step and plywood trim/structure from being incorrectly merged;
* real grouping reduced chunk-budget exceptions from seven cells to five.

The final generated measurements are:

```text
Material gate: 96 cell maps, 99 GLBs, 115 authored material rows
Unwrap:        78 cells, 5,286 receiver faces, 35,306 detail faces, 78 atlases
Chunks:        459 chunks / 93 cells, 108,023 vertices
Packed bytes:  3,428,956 (raw 5,185,104), five documented exceptions, no problems
```

The generated shell is under `build/shell`, its unwrapped form is under `build/shell-lm`, and the
deployed chunk file is `content/world/chunks.bin`. `docs/shell-manifest.json` matches the final
generator and shell bytes. `tools/ci/run_checks.sh` now includes a `shell-materials` gate.

## 4. Verification paid by this commit

All builds and tests were limited to four cores. Graphical tests used offscreen video and dummy
audio.

```text
Generator self-test:                         passed
Shell material assignment gate:             96 maps / 99 GLBs passed
Chunk builder self-test and production run: passed
Build:                                       complete, warning-clean
Complete ctest suite:                        1,555 non-failures; 9 initial failures
Focused retry, headless weather:             passed
Focused retry, SaveStoreTest.*:              10 / 10 passed outside sandbox
Static gates:                                tools/ci/run_checks.sh all green
Whitespace:                                  git diff --check clean
```

The first eight SaveStore failures were sandbox permission failures when writing the measured user
save directory. All SaveStore tests passed outside the sandbox. The remaining headless weather
case passed immediately in isolation. No HOUSE-00907 failure remained.

## 5. The next session's visual objective

Open a **new clean Sol High context**. Its objective is not “continue the dependency DAG”; it is:

> Make House Simulator finally look visually convincing.

Begin by launching the current `cna-house` binary on the owner's real screen and walking a useful
exterior/interior route. Record the largest visible blockers with screenshots or precise poses.
Then map those blockers to the smallest dependency-valid tasks and execute the highest-impact one.
Architecture and the ledger still constrain implementation, but task ordering should serve the
visual result.

The strongest known blocker before that audit is that real material ids now reach `chunks.bin`,
but `StaticGeometryPass` still computes a hashed diagnostic `BlockoutColour` instead of binding the
authored albedo/tint. The canonical DAG offers `HOUSE-00909`–`HOUSE-00911` for lightmap baking and
`HOUSE-00912` for lightmap loading plus the shell `DualTextureEffect` draw path. A visually driven
agent should inspect the real frame first, then determine whether accelerating the real material
binding/render path (while preserving honest dependencies and task acceptance criteria) is the
highest-leverage route. Do not claim visual completion merely because offline material ids exist.

The previous real-screen inspection, before HOUSE-00907, showed a functional 60 fps walk/HUD but
mostly diagnostic magenta/green blockout rendering. HOUSE-00907 fixes the offline identities, not
that runtime presentation. The new context should use this as the baseline to beat.
