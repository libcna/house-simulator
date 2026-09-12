# Session handoff — 2026-09-12

This is the authoritative session handoff for the next agent. Read it first, then read only the
parts of `plan.md`, `cna-house.md`, `AGENTS.md`/`CLAUDE.md`, and source files needed for the next
dependency-valid task. Do not restart the project or repeat completed audits.

## 1. Exact repository state

| Item | State |
|---|---|
| Repository | `/rv/data/development/github.com/libcna/house-simulator` |
| Branch | `develop` |
| Code baseline | `94b160e` — `feat: update sky dome colours from LUT (HOUSE-01644)` |
| Remote | `origin/develop`; `94b160e` was pushed before this handoff commit |
| Handoff commit | The commit containing this file; it is a docs-only child of `94b160e` and is also intended to be pushed |
| Working tree at handoff | Clean after the handoff commit |
| Ledger count before the handoff commit | 509 completed / 833 open task rows |

Verify these claims with `git branch --show-current`, `git rev-parse HEAD`, `git status --short`,
and `git log --oneline -5` before changing anything.

The previous build-directory migration is complete. The active `build/` cache uses the sibling
repositories the owner requested:

| Dependency | Branch / HEAD | Build use |
|---|---|---|
| `../cna` | `next` / `0a3a1460169c9256897626f5b9c7bc308735949d` | `CNA_SOURCE_DIR`; clean |
| `../sharp-runtime` | `next` / `0c82d9b888bdf5f7d5663c77942f339bcb2a7445` | `CNAHOUSE_SHARP_RUNTIME_ROOT`; clean |
| SDL3 prebuilt | `../cna/.sdl-prebuilt-Linux-x86_64-wayland` | present and used by the build |

There are no standalone `../cna/build` or `../sharp-runtime/build` directories. This is not a
problem: house-simulator builds both dependencies as CMake subprojects under `build/CNA_BUILD/`
and `build/CNA_BUILD/SHARP_RUNTIME/`. The current tree contains the CNA libraries, 14
sharp-runtime archives, and `build/CNA_BUILD/modules/renderers/easygl/libcna_renderer_easygl.a`.
The `cna-house`, unit-test, and integration-test binaries all build successfully from this cache.
Do not reconfigure merely to create standalone sibling build directories.

The cache also confirms:

```text
CNA_SOURCE_DIR=/rv/data/development/github.com/libcna/cna
CNAHOUSE_SHARP_RUNTIME_ROOT=/rv/data/development/github.com/libcna/sharp-runtime
CNA_CNAEXT=OFF
CMAKE_C_COMPILER_LAUNCHER=ccache
CMAKE_CXX_COMPILER_LAUNCHER=ccache
```

Neither sibling repository was modified by this session.

## 2. Mandatory display and build rules

**Never use `DISPLAY=:99` here. It is a real, visible display on this machine.** An earlier test
invocation incorrectly assumed otherwise and opened test windows on the owner's screen. There
were no test/game processes left running at handoff.

Every graphical or integration invocation must explicitly use:

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy <command>
```

Pure static checks do not create windows. Do not run broad graphical suites when a focused test is
sufficient. A real-screen game launch requires an explicit owner request.

For every build use the established environment and the existing `build/` directory:

```bash
PATH=/home/robertvokac/.pyenv/versions/3.11.9/bin:/usr/local/bin:/usr/bin:/bin \
CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv \
cmake --build build --target <targets> -j$(nproc)
```

Keep that Python first in `PATH` when committing too. The pre-commit hook runs world validation,
and `/usr/bin/python3` does not have `jsonschema`; the Python 3.11.9 environment does. Do not bypass
the hook with `--no-verify`.

All normal project constraints remain absolute: strict XNA 4.0-shaped runtime API,
`CNA_CNAEXT=OFF`, no CNAEXT/model convenience API, no sibling-repository edits, and one completed
task plus its `plan.md` checkbox per commit.

## 3. Work completed in the sky chain

The dependency chain `HOUSE-01641` through `HOUSE-01644` is complete. Do not redo it.

| Commit | Task | Result |
|---|---|---|
| `969f735` | `HOUSE-01641` | Generated the compact 32-row sky colour curve and analytic cloud/azimuth model in `layout.sky.json`. |
| `5837243` | `HOUSE-01642` | Generated the non-degenerate 610-vertex / 1,216-triangle `CSKY` v1 dome. |
| `461ac3e` | `HOUSE-01643` | Added the camera-following XNA `SkySystem` dome and composed the existing sun disc in the one `Pass::Sky` slot. |
| `94b160e` | `HOUSE-01644` | Added strict colour-model loading, material-change CPU recolouring, and live XNA vertex-buffer updates. |

`HOUSE-01644` specifically added:

* `SkyColourModelReader`, which validates schema `cna-house/sky/1`, exactly 32 strictly ascending
  elevation rows, the 8 × 16 conceptual sample axes, unit RGB values, and finite bounded scalars;
* retained `VertexPositionColor` data for all 610 vertices;
* interpolation between altitude rows, smoothstep from horizon to zenith, and the generated
  `cloudCover^1.5` overcast mix;
* updates only after a greater-than 0.25-degree altitude change or greater-than 0.01 cloud-cover
  change, with no per-update vector allocation;
* re-upload to the existing XNA vertex buffer and counters `sky.colour.updates` and
  `sky.colour.micros`;
* unit coverage for the parser, values, exact thresholds, 600-frame non-per-frame behaviour, full
  overcast, and timing; and an offscreen device test that performs a second GPU upload.

Measured across 64 forced debug-build updates: mean **0.105 ms**, maximum **0.121 ms**. A
600-frame half-degree transition caused only two updates including construction. These figures and
the completed checkbox are recorded in `plan.md`; architecture and format details are in
`cna-house.md` §31.2 and `docs/sky-dome-format.md`.

Important input detail: `assets-src/world/layout.sky.json` is JSONC with comments and is consumed by
offline tooling. Runtime and C++ tests must read the deployed, comment-free
`content/world/layout.sky.json`. Do not switch the tests back to the source JSONC file.

## 4. Verification paid at `94b160e`

All commands below completed successfully. Graphical commands used the offscreen/dummy drivers.

```text
Build: cna-house, cnahouse_unit_tests, cnahouse_integration_tests
Unit:  SkySystemTests.* — 6 / 6 passed
Integration: SkySystemPassTests.* plus
             HeadlessRunTests.TheWalkSceneLoadsTheSunBakeAndPublishesDaylight — 2 / 2 passed
Static: tools/ci/run_checks.sh — all gates green
Strict API: check_xna_strict — 297 translation units clean, 27 destructor exemptions
Diff: git diff --check — clean
```

The first commit attempt was correctly rejected because the hook inherited `/usr/bin/python3` and
could not import `jsonschema`. No commit was created by that attempt. Re-running with the PATH in
§2 made every staged gate green and produced `94b160e` normally.

## 5. Next dependency-valid work

Start by reassessing the DAG in `plan.md`. The immediate Phase-25 successor is:

* **`HOUSE-01645` — implement the night-sky blend and sun-glow term.**

Useful existing seams:

* `SkyColourModel` already loads and retains `sunGlowColor`, `sunGlowStrength`, and
  `sunGlowExponent`.
* `SkySystem::SetSun` receives the full `environment::SunPosition`; its `azimuthDeg` is available,
  although `HOUSE-01644` intentionally forwards only altitude and cloud cover to `SetSky`.
* The generated model and its independent expansion oracle are in `tools/world/sky_lut.py`.
  Preserve that oracle rather than duplicating expected values from the runtime.
* The full target formula is in `cna-house.md` §31.2. `HOUSE-01644` implemented the altitude,
  vertex-altitude, and overcast portions only. Directional glow and night blending are intentionally
  still open.

There is a dependency question to resolve honestly before marking `HOUSE-01645` complete:
§31.2 writes the final night mix in terms of moon altitude and phase, but `MoonModel` and phase are
still open as `HOUSE-01601` and `HOUSE-01602`, while `HOUSE-01645` currently depends only on
`HOUSE-01644`. Do not invent moon data or fake completion. Inspect the architecture and ledger for
the smallest justified resolution; if the moon inputs genuinely block the whole task, record the
evidence in `plan.md` and continue independent dependency-valid work.

The nearest independent task is:

* **`HOUSE-01646` — author the three alpha cloud textures** (depends only on completed
  `HOUSE-01641`). It unblocks `HOUSE-01647` through `HOUSE-01650`.

Do not proceed past this handoff until the owner starts a new session or gives new instructions.
