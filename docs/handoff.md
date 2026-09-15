# Visual-sprint handoff — 2026-09-15 (`HOUSE-01280` checkpoint)

Branch `develop`; verify HEAD and `git status` before acting. This session began at
`238d6aed27c1e8662f0d118013ef08bb801ae018` (`HOUSE-00922`) and committed the first real
living/family static kit as `86a80f705c10f146b71d70c63f36813860914053` (`HOUSE-01037`). The
second one-task checkpoint is `HOUSE-01280`: stock-XNA Tier-S receiver ambient and static Basic
detail now follow the live cell rather than a mostly black lamp bake or XNA's constructor-white
directional key. Check `git log -3` for the exact HOUSE-01280 commit. No ordinary gameplay route
uses the hashed blockout palette; `--scene=blockout` and `--debug-blockout-materials` remain
explicit diagnostics. `VISUAL-GATE-1` is **not passed**.

Before: [`house-01037-furniture-r7`](visual-review/captures/house-01037-furniture-r7).
After: [`house-01280-detail-r2`](visual-review/captures/house-01280-detail-r2). Both are the same
eight normal-game fixed cameras, clear 10:30, Tier S/High and software Mesa. R2 makes the walls,
floors and sofas appreciably more readable but is still visibly incomplete. Largest defects, in
review order: **missing production outdoor ground/road/fence/gate/roof geometry**; almost
pure-white windows; dark, empty foyer and hall; empty kitchen; peeling boundary paint, bright
leaves and sofa style/lighting mismatch. See [`visual-review/README.md`](visual-review/README.md),
Round 8. The first high-value follow-up is the outdoor production material assignment: at the
same `EXT_ROAD` player pose looking down 10°, explicit `--debug-blockout-materials` visibly draws
road, fence, gate, landscaping and roof while normal production does not. The generated chunk
string table still contains role names (`TERRAIN_asphalt`, `FENCE_board`, `ROAD_paint`) absent from
canonical `layout.materials.json`. Outdoor generators omit `materialId` and `build_chunks.py` still
accepts those legacy roles, while unbaked outdoor receivers are deliberately `Basic` layout.
The stock-XNA production pass correctly skips a chunk whose material cannot be resolved. Fix the
generator/material-table parity through canonical data and existing content stages, not a
diagnostic colour fallback. Then diagnose **window/background brightness** with actual
inside/outside captures; do not assume the clear-glass alpha is wrong. CNA's local BasicEffect
forwarding already multiplies diffuse RGB by `Alpha`, matching XNA premultiplied
`BlendState::AlphaBlend`, so blindly premultiplying it again would darken the pane. Primary
`L0_FOYER`/`L0_HALL`/`L0_KITCHEN` furnishings remain a priority; the existing 15 static props only
furnish `L0_LIVING`/`L0_FAMILY`.

`HOUSE-01280` renderer integration proves two live lamp groups at noon use four receiver passes,
the same room with both off uses two, and canonical four-group `L0_KITCHEN` at noon uses six
(neutral opaque floor + four authored artificial maps + daylight). The extra pass is real;
GPU frame-time budget has **not** been proven by the fixed-step screenshot HUD. Do not claim that
32.1-fps overlay is a measured renderer result. Static Basic detail gets one cell-derived key and
coarse sky/fixture ambient; full per-dynamic-object key/fill/bounce (`HOUSE-01261`) remains open.

Critical test correction: a green hardware CTest render registration may be **capture-only** if
`RenderingInSoftware()` is false. Under `LIBGL_ALWAYS_SOFTWARE=1`, the first strict comparison
found three stale refs after final HOUSE-01037 furniture shifts. The actual silhouettes were
inspected and only `blockout-l0-kitchen`, `blockout-l0-living` and `fp-l0-kitchen` were corrected.
The new ambient/key composition then changed all 12 production first-person views and four
season/sun outer-skin detail views; all 16 before/after pairs were inspected and their refs
deliberately updated. A subsequent **actual software** test passed production, explicit debug,
property, season/sun and 18-pair culling suites. Run these software pixel checks in the default
sandbox with `LIBGL_ALWAYS_SOFTWARE=1`, offscreen video and dummy audio. The complete broader
CTest suite should use the escalated four-core environment below **without** forcing LIBGL
software, which otherwise makes this machine's hardware-EGL cases fail for environmental reasons.
The full 1,583-registration CTest run passed in that broader configuration; `git diff --check`
was clean. `tools/ci/run_checks.sh` passed, including 323 strict-XNA translation units, before the
HOUSE-01280 commit. Sibling CNA had an unrelated deleted video fixture at the final read-only
status check; neither CNA nor sharp-runtime was changed by this task.

## Archived HOUSE-01037 checkpoint — 2026-09-15

The 2026-09-14 handoff below is an **archive**, not the present repository state. Verify the
branch, HEAD and worktree before acting. Current starting HEAD is
`238d6aed27c1e8662f0d118013ef08bb801ae018` (`HOUSE-00922`) on `develop`.

`HOUSE-01037` completes this first static-furniture checkpoint: nine licensed/attributed furniture
GLBs and eight sRGB albedos, 15 static placements in `L0_LIVING` and `L0_FAMILY`, five heavy-model
LOD/collision preparations, source-material-preserving chunks, Reach primitive splitting and
power-of-two static textures. Current reviewed normal-game captures are
[`house-01037-furniture-r7`](visual-review/captures/house-01037-furniture-r7), with the earlier
empty route at [`house-00922-outdoor-sky-r1`](visual-review/captures/house-00922-outdoor-sky-r1).
`VISUAL-GATE-1` is **not passed**: furnishings are now visible but white upholstery clips against
almost-black walls/floors, kitchen and foyer remain empty, and the front façade lacks convincing
ground/vegetation context. The concise ranked review is in
[`docs/visual-review/README.md`](visual-review/README.md), Rounds 6–7.

The source glTF importer, scale/origin, manifest, packaging licence and exact credit-generation
checks pass. One important credit-generator correction now includes CC BY `derived` furniture in
the in-game credits. The initial furniture-aware `nav.bin` build reached roughly 50 minutes before
signal 143; an **exact conservative mesh-AABB lower bound** then reduced a full build to 19 minutes
without changing the clearance answer. Its selftest passed, including near/far equivalence. After
the first full unit suite found authored furniture crowding at two room midpoints and wedges at
three wall poses, the source placements were adjusted, **not the collision limits**. The final
world build completed in 1,282.09 s (nav 1,267.51 s), the full content graph is fresh, the full
CMake build passes and the final eight views have been inspected. Strict interior refs, the
18-pose culled/unculled comparison and the complete 1,583-registration CTest run return green
(existing intentional skips/disabled cases preserved). Two old integration
tests needed corrected pass-slice/material-count expectations for the real plant leaves and eight
new furniture materials; their stronger invariants also pass. `tools/ci/run_checks.sh` passed again
after those two test edits, including 323 strict-XNA translation units. The task checkbox is
complete in the one `HOUSE-01037` commit.
Build/test CPU usage stays pinned to cores 4,5,7,9 with the env block below. Never bypass the
Python 3.11 `jsonschema` commit hook. Do not modify CNA or sharp-runtime. CNA acquired concurrent,
unrelated X11/platform edits during this session; they are outside this repository/task and were
not touched here. `sharp-runtime` and `living-room-simulator` remain clean.

Once this checkpoint is committed, the next highest-visible-value work is to reconcile the
stock `BasicEffect`'s default white downward furniture key with the live room lighting and to
restore a plausible ambient/daylight base to L0 receiver surfaces. The artificial main atlas for
`L0_LIVING` has a 10.55 peak against 0.011 mean irradiance; using its mostly dark spatial pattern
as the sole opaque ambient-floor carrier is a measured source of black walls. Keep the Tier-S/XNA
architecture and validate any correction against the fixed eight review cameras.

## Archived handoff — 2026-09-14

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
