# CNA House Master Implementation Plan

`STATUS: APPROVED — IMPLEMENTATION IN PROGRESS (approved 2026-09-06)`

The project owner approved implementation on 2026-09-06. This file is now the **execution
ledger**: a task's checkbox moves to `[x]` only when its acceptance criteria are genuinely
satisfied and its `verify:` step has been run. Task ids are permanent and are never renumbered.

| | |
|---|---|
| Phase in progress | 1 — CNA capability verification |
| Completed | 44 of 1 308 tasks |
| Baseline commit | `96d21db` (the approved planning baseline) |
| Phase 0 | stage A closed by `HOUSE-00042`; seven stage-B tasks deferred to phase 2 by design |
| Phase 1 | `HOUSE-00061`–`HOUSE-00069` measured; `BL-06` settled and corrected |

Corrections made to the planning documents during implementation are recorded in
[Planning corrections](#planning-corrections) at the end of this file, never applied silently.

---

## Planning baseline

| Item | Value |
|---|---|
| Plan created | 2026-09-06 |
| Project path | `/rv/data/development/github.com/openeggbert/cna-house` |
| Repository state at planning time | git initialised, branch `main`, **no commits** |
| CNA checkout | `/rv/data/development/github.com/openeggbert/cnanext` |
| CNA revision | `d42203805057d43dc092bb713613bcc945ad8d5a` |
| CNA branch | `next` |
| CNA head commit date | 2026-09-06 15:07:20 +0200 |
| sharp-runtime checkout | `/rv/data/development/github.com/openeggbert/sharp-runtimenext` |
| sharp-runtime revision | `30ccdef30ba4d27534864b0777729b9e78ee8a21` (branch `next`) |
| CNA samples checkout (reference) | `/rv/data/development/github.com/openeggbert/cna-samples` @ `48f6fc9d2ec72d574732a4faa42fb2543fcc78bd` (branch `develop`) |
| Host platform | Debian 13, Linux 6.12.107, x86_64, 16 threads, 30 GB RAM, no swap |
| Compiler | g++ (Debian 14.2.0-19) 14.2.0, C++23 |
| CMake / Ninja | 3.31.6 / 1.12.1 |
| GPU | AMD Radeon 780M (radeonsi, phoenix), Mesa 25.0.7, GL 4.6 core / GLES 3.2, 1024 MB reported video memory |
| Primary renderer | `CNA_GRAPHICS_RENDERER=OPENGLES3` (EasyGL) |
| Secondary renderers | `HEADLESS` (CI/logic tests), `OPENGL33` (occlusion-count validation only) |
| Platform backend | `CNA_PLATFORM=SDL3`; audio `CNA_AUDIO_PLATFORM=SDL3` |
| Engine layer | `CNA_CNAEXT=OFF` — permanently, non-negotiable |
| Compiled effects | `CNA_EASYGL_COMPILED_EFFECTS=ON` — and this build-time fact, not any runtime query, is what selects Tier E (`cna-house.md` §7.3) |
| Video | `CNA_ENABLE_VIDEO=AUTO` (FFmpeg present on this host) |
| Offline toolchain | Blender 4.3.2 (`/usr/bin/blender`), ffmpeg 7.1.5, Python 3.11.9, `fxc.exe` from the DirectX SDK June 2010 at `/rv/tmp/samples/_tools/directx-sdk-june-2010/extract/DXSDK/Utilities/bin/x86/fxc.exe` run through Wine, official XNA 4.0 content pipeline under `WINEPREFIX=~/.wine-cna-xna40` |
| ccache | `CCACHE_DIR=/rv/cnaccache`, `CCACHE_BASEDIR=/rv`, launchers passed to CMake |
| Build directories | `build/`, `build-asan/`, `build-ubsan/`, `build-probe/`, `build-consumer/` — the closed list, in-repo |
| `/rv/tmp` audio collection | **FOUND** — `/rv/tmp/Essentials_Series_NOX_SOUND/`, 1 644 `.wav`, 1.1 GB, `pcm_s24le` @ 48 kHz, declared CC0 by its own bundled README. Audited in `cna-house.md` §63. |
| Known external asset sources (candidates, unverified) | Poly Haven, ambientCG, Khronos glTF-Sample-Assets, Quaternius, Kenney, Poly Pizza, BlenderKit free, Sketchfab CC0, Blend Swap CC0, MakeHuman/MPFB2, CMU Motion Capture Database, Freesound CC0, SIL OFL fonts |
| Network access during planning | **None used.** No licence in this plan is asserted as verified; phase 4 verifies every one. |
| Default simulated day length | **24 real minutes** (`timeScale = 60`, 1 real second = 1 simulated minute) |
| House size | 5 levels (`B1`/`L0`/`L1`/`L2`/`L3`), ≈ 935 m² above grade, ≈ 1 306 m² total enclosed, 95 cells, 186 portals |
| Blockers identified | 15 (`BL-01` … `BL-15`, `cna-house.md` §6) — 1 High for the Android phase only, 0 High for Linux |

---

## How to read this plan

Every task has the form:

```
- [ ] HOUSE-09999 — Objective, in one line
      dep: HOUSE-09991, HOUSE-09992 · sys: visibility · plat: LNX · pri: MUST
      files: src/visibility/PortalTraversal.cpp|hpp
      accept: (1) closed opaque door removes the target cell; (2) opening restores it;
              (3) both directions covered
      verify: unit PortalTraversalTests.ClosedDoorCulls; debug overlay F4; screenshot cull-01
```

| Field | Meaning |
|---|---|
| `dep` | Task IDs that must be complete first. `—` means none. |
| `sys` | Owning subsystem, matching the `src/` directory. |
| `plat` | `LNX` (Linux desktop), `WEB`, `AND`, `ALL`, `TOOL` (offline tooling), `CI`. |
| `pri` | `MUST`, `SHOULD`, `OPT` — per `cna-house.md` §81. |
| `files` | Expected files. `a|b` means `a.cpp` and `a.hpp`. Advisory, not binding. |
| `accept` | Acceptance criteria. Numbered when there is more than one. |
| `verify` | How it is proved: a named test, a debug overlay, a screenshot scene, a measurement. |

**Task ID stability.** IDs are permanent. A completed task is never renumbered. A cancelled task
is struck through and keeps its ID. New work takes the next free ID **in its phase's reserved
range**; each phase reserves more IDs than it currently uses precisely so insertions do not
disturb anything.

**Grouped tasks.** Where work is genuinely repetitive and data-driven — furnishing 78 rooms,
sourcing 40 sounds, authoring 62 door rows — one task covers the group and states the count and
the per-item acceptance criterion. This is deliberate: creating 214 tasks for 214 drawers would
hide the real structure of the work rather than reveal it. Architectural and systems work is
decomposed to single-commit granularity.

**Priorities.** Every feature the brief requested is `MUST` unless it is explicitly a later
platform (`SHOULD`, phases 47–51) or an optimisation gated on a measurement (`SHOULD`/`OPT`, with
the gate named). Nothing requested has been downgraded to make the plan shorter.

---

## Phase index and ID ranges

| Phase | Name | ID range | Tasks | Exit criterion |
|---|---|---|---|---|
| 0 | Repository, conventions, decisions | 00001–00060 | 42 | The repo builds an empty `Game` and CI is green |
| 1 | CNA capability verification | 00061–00120 | 60 | Every §5 claim re-proved; `BL-09` settled; probes deleted |
| 2 | Build skeleton and CI | 00121–00180 | 47 | `Game` clears the screen; HEADLESS tests run in CI |
| 3 | Content pipeline | 00181–00260 | 45 | glTF, PNG, WAV, SpriteFont and FX all compile and load |
| 4 | Asset provenance and licensing | 00261–00340 | 42 | Manifest tooling green; NOX imported; every source licence verified |
| 5 | World and floor-plan data | 00341–00450 | 80 | The full layout authored, validated and loaded |
| 6 | Blockout house geometry | 00451–00540 | 34 | The generated shell renders |
| 7 | Collision and player controller | 00541–00620 | 35 | You can walk the whole blockout |
| 8 | First-person camera | 00621–00660 | 14 | It feels right and is tested |
| 9 | Room/portal visibility | 00661–00760 | 37 | Culling correct, proved, and within budget |
| 10 | Exterior and property | 00761–00840 | 23 | Terrain, fences, gates, drive, garden |
| 11 | Neighbourhood background | 00841–00890 | 15 | The house is not floating in nothing |
| 12 | Materials and textures | 00891–00970 | 30 | The blockout reads as a building |
| 13 | Static furniture and dressing | 00971–01120 | 64 | Every room furnished to density |
| 14 | Interactable framework | 01121–01180 | 26 | The 12 behaviours and the data model |
| 15 | Doors and windows | 01181–01250 | 22 | Portals are dynamic |
| 16 | Lights and switches | 01251–01310 | 28 | The house can be lit — **first playable** |
| 17 | Containers | 01311–01360 | 13 | 214 things open with contents |
| 18 | Kitchen and refrigerator | 01361–01410 | 23 | |
| 19 | Plumbing and water | 01411–01460 | 19 | |
| 20 | Toilets | 01461–01490 | 14 | |
| 21 | Television and video | 01491–01530 | 14 | Both backends |
| 22 | Time | 01531–01560 | 17 | |
| 23 | Sun, glare, sun-clock | 01561–01600 | 18 | |
| 24 | Moon and stars | 01601–01640 | 19 | |
| 25 | Sky and clouds | 01641–01680 | 15 | |
| 26 | Weather core | 01681–01740 | 24 | |
| 27 | Rain | 01741–01790 | 16 | |
| 28 | Snow | 01791–01830 | 12 | |
| 29 | Storm, lightning, thunder | 01831–01870 | 14 | |
| 30 | Hail and wind | 01871–01910 | 20 | |
| 31 | Audio foundation | 01911–01990 | 31 | |
| 32 | Room-aware 3-D audio | 01991–02040 | 19 | |
| 33 | Dog | 02041–02090 | 21 | |
| 34 | Cat | 02091–02130 | 16 | |
| 35 | Third-person avatar | 02131–02170 | 16 | |
| 36 | Character customisation | 02171–02210 | 17 | |
| 37 | Character animation | 02211–02270 | 28 | |
| 38 | Stair animation and foot IK | 02271–02300 | 14 | |
| 39 | Persistence | 02301–02370 | 33 | |
| 40 | Reset House | 02371–02390 | 10 | |
| 41 | Culling and LOD optimisation | 02391–02450 | 19 | All budgets met |
| 42 | Streaming and loading | 02451–02500 | 13 | Gated on `HOUSE-02451` |
| 43 | Debug tools and settings | 02501–02570 | 31 | |
| 44 | Automated tests | 02571–02680 | 31 | Full suite green |
| 45 | Visual polish and habitation | 02681–02780 | 34 | |
| 46 | Linux desktop stabilisation | 02781–02840 | 17 | **Feature-complete desktop** |
| 47 | Web preparation | 02841–02890 | 17 | |
| 48 | Web implementation | 02891–02950 | 15 | |
| 49 | Android preparation | 02951–02990 | 12 | Gated on `BL-13` upstream |
| 50 | Android touch UI | 02991–03030 | 12 | |
| 51 | Android implementation | 03031–03070 | 12 | |
| 52 | Final optimisation and release | 03071–03120 | 9 | |

---

## Phase 0 — Repository, conventions and decisions

**Goal.** A repository that a later session can pick up without asking a single question: layout,
build, conventions, licence, decision records, and the enforcement gates that keep the XNA-only
rule true.

**Exit.** `cmake --build build` produces a binary that opens a window and clears it; CI runs the
lint gates and an empty test suite; every ADR listed below exists.

**Phase 0 closes in two stages, deliberately.** Its exit criterion names a build and a test suite,
and neither exists until phase 2 authors `CMakeLists.txt` (`HOUSE-00121`) and the GoogleTest
harness (`HOUSE-00125`). Phase 1 must not wait for them: capability probing needs only the sibling
CNA checkout, and its whole purpose is to settle the facts phase 2's build skeleton is designed
against. The two stages are therefore:

* **Stage A — the pre-probe foundation, gated by `HOUSE-00042`.** Everything that can be finished
  and proved *before* the application build exists: the directory skeleton, the twelve ADRs, the
  conventions, workflow, licensing, content-authoring and world-format documents, the enforcement
  gates and their self-test, and the header-only `Result<T>`. `HOUSE-00042` certifies exactly this
  set, and it is the gate `HOUSE-00061` opens phase 1 on.
* **Stage B — the deferred verifications.** `HOUSE-00002`, `HOUSE-00025`–`HOUSE-00029` and
  `HOUSE-00035` stay **open**. Each is blocked on infrastructure phase 2 creates, and each closes
  when its own `verify:` step becomes executable — not before, and never by relaxing its
  acceptance criteria. They are listed with their unblocking task in [Status](#status).

This is dependency staging, not ignored work. A stage-B task is a real open task with a real
acceptance criterion; it is not marked complete on the strength of the code having been written.

- [x] HOUSE-00001 — Create the repository skeleton: directories per `cna-house.md` §17.5, with a `.gitkeep` in each empty one
      dep: — · sys: app · plat: ALL · pri: MUST
      files: (directories only)
      accept: every directory in §17.5 exists; `git status` is clean after the first commit
      verify: `tools/ci/check_layout.py` asserts the directory set
- [x] HOUSE-00002 — Write `README.md`: what the project is, the XNA-only rule in three sentences, how to build, how to run, where the plan is
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
      accept: a new contributor can build from the README alone
      verify: manual, plus a CI check that the build commands in the README are the ones CI runs
      history: WRITTEN, NOT TICKED (2026-09-06 morning). The acceptance criterion is that a
              contributor can *build* from it, which could not be true before `HOUSE-00121`
              authored `CMakeLists.txt`. The README carried a visible note saying so.
      note: (2026-09-06) **Closed, and verified by executing it.** The build now exists, the note is
            removed, and every command in the README was run from a **deleted build directory**:
            `cmake --preset linux-debug`, `cmake --build build --parallel`,
            `ctest --test-dir build --output-on-failure` (170/170), `./build/cna-house
            --renderer-info` and `tools/ci/run_checks.sh`. The commands are the ones
            `CMakePresets.json` defines and the ones `.github/workflows/ci.yml` runs, which is the
            "CI check" half of the criterion — CI drives the same presets rather than a parallel
            copy of the commands, so they cannot drift.
      corrections: writing the build made three statements in it wrong, and all three are fixed.
            (1) The sibling-checkout requirement was implied rather than stated; it is now a diagram
            plus the note that `git submodule update --init` will not fetch them, because that is
            the first thing anyone tries. (2) `--renderer-info` was documented as printing "then
            continue"; it prints and **exits**. (3) The save location was given as
            `${XDG_DATA_HOME:-~/.local/share}/cna-house/`; `HOUSE-00102` measured it as
            `.../game/CnaHouse/`, and the README now says why the `game` component is literal.
            `CCACHE_DIR` also moves from `/rv/cnaccache` to `$HOME/.cache/ccache` here, in
            `cna-house.md` §8.1 and in the capability report — the same physical cache, but the home
            path is ccache's own default, so code that forgets to export it still lands in the one
            cache instead of starting a second.
- [x] HOUSE-00003 — Choose and add the project licence and `NOTICE.md`
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
      accept: licence file present; `NOTICE.md` explains that content assets carry their own licences and points at `licenses/`
      verify: `verify_licences.py` step 0
      note: (2026-09-06) accepted on the stated criteria, checked by hand — `LICENSE` carries
            the Ms-PL text, `NOTICE.md` states that content assets carry their own licences and
            points at `licenses/`. `verify_licences.py` itself is phase 4 (`HOUSE-00261`+); it
            re-checks this row automatically when it lands.
- [x] HOUSE-00004 — Add `.gitignore`: `build*/`, `content/` (except the committed baseline), `*.log`, editor dirs, `assets-src/**/.blend1`
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
      accept: a full build leaves `git status` clean
      verify: CI runs a build then `git diff --exit-code`
      note: (2026-09-06) verified by synthesising exactly what a full build and a run leave
            behind — `build/`, `build-asan/`, `build-ubsan/`, `build-probe/`,
            `build-consumer/`, `content/**`, `*.log`, `screenshots/`, `test-output/`,
            `*.blend1` — and asserting `git status --porcelain -uall` reported none of them,
            while `content/.gitkeep` stayed trackable. The CI build-then-diff form of the same
            check lands with `HOUSE-00121`.
- [x] HOUSE-00005 — Add `.editorconfig` and `.clang-format` matching CNA's own style (4 spaces, 110 columns, Allman for types)
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
      verify: `clang-format --dry-run -Werror` over the tree
- [x] HOUSE-00006 — Add `AGENTS.md`/`CLAUDE.md` for this repository: the XNA-only rule, the build-directory rules, ccache, the ID convention, the "commit after each task" rule
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
      accept: it restates the openeggbert build rules and adds only project-specific rules
- [x] HOUSE-00007 — ADR-0001: XNA-only interpretation and the three-tier A/P/C policy — pure XNA, project-owned `cnahouse::` code, forbidden CNA API, with no middle tier and no allowlist
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
      files: docs/decisions/ADR-0001-xna-only.md
      accept: reproduces `cna-house.md` §4 as a decision record with the alternatives considered
- [x] HOUSE-00008 — ADR-0002: renderer selection (`OPENGLES3`), with the rejected alternatives and their reasons
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00009 — ADR-0003: two rendering tiers, and why Tier S must be complete alone
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00010 — ADR-0004: portal visibility with frustum reduction; stencil, PVS and BSP rejected with reasons
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00011 — ADR-0005: data-driven world; JSON schemas; the closed expression vocabulary
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00012 — ADR-0006: composition over ECS, with the entity-count argument
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00013 — ADR-0007: project-owned kinematic collision; Bullet/Jolt rejected; the swap-in seam named
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00014 — ADR-0008: delta save format, versioning and migration policy
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00015 — ADR-0009: 24-minute simulated day, with the 20-vs-24-vs-48 analysis
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00016 — ADR-0010: room-aware audio computed over the portal graph rather than by `Apply3D`
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00017 — ADR-0011: no third-party runtime dependencies; the candidate table and each verdict
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00018 — ADR-0012: asset licensing policy and the "no row, no build" rule
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00019 — Create `docs/xna-deviations.md`: the Tier A/P/C policy of `cna-house.md` §4.3, the seven Tier-P project-owned subsystem rows (OWN-01…07), and an explicit statement that the file grants **no permission to call any CNA symbol**
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
      accept: (1) zero rows permit a CNA API call; (2) each Tier-P row names the subsystem, what XNA 4.0 lacks and the owning document section; (3) the file states that `check_xna_only.py` reads no allowlist from it
- [x] HOUSE-00020 — Write `tools/ci/check_xna_only.py` — the strict gate of `cna-house.md` §70.1, with **no allowlist**: reject in any runtime source a `CNA/` include, a `CNA::` reference, any `*EXT*` identifier (`getSkinsEXTProperty`, `setOwnedResources`, `SkinnedModelEXT`, …), `SupportsCapability`, `ShaderEffect`/`PbrEffect`/`SkinnedPbrEffect`/`AvatarRenderer`, a `Model::Tag`/`getTagProperty` read, a CNA `Graphics::SkinningData`/`AnimationClip`/`Keyframe`/`AnimationPlayer` type, a GL/GLES/EGL/Vulkan/WebGPU/D3D/Metal/SDL-rendering symbol, or `CNA_CNAEXT=ON` in the CMake cache
      dep: HOUSE-00019 · sys: ci · plat: CI · pri: MUST
      files: tools/ci/check_xna_only.py
      accept: (1) a deliberately-planted violation of each class is detected; (2) the clean tree passes; (3) the script consults no per-symbol exception list — a violation cannot be argued into the build, only rewritten as `cnahouse::` code
      verify: the script's own self-test with 14 planted-violation fixtures, one per rejected class
- [x] HOUSE-00021 — Extend `check_xna_only.py` with the "no `isRaining`-style boolean" lint, the "no `std::filesystem` outside `SaveStore`" lint, and the "custom shaders are XNA `Effect`s" lint (no GLSL/SPIR-V source anywhere in the tree; `.fx` only under `assets-src/Effects/`)
      dep: HOUSE-00020 · sys: ci · plat: CI · pri: MUST
      correction: (2026-09-06) the path is `assets-src/Effects/`, PascalCase. This task and
                  `cna-house.md` §70.1 said `assets-src/effects/` while §18.1's pipeline
                  diagram, §17.5 and the "directories are PascalCase" rule of §8.3 all said
                  `Effects/`. The lint needs one canonical spelling; PascalCase wins because
                  three statements support it and one did not. `cna-house.md` §70.1 and the
                  §18.1 CMake snippet were corrected in the same commit.
      note: the three lints are implemented in `tools/ci/check_xna_only.py` alongside
            `HOUSE-00020`'s and are covered by the same `--selftest` fixture set
            (`weather-boolean`, `std-filesystem`, `shader-source`, `fx-placement`).
- [x] HOUSE-00022 — Write `tools/ci/check_layout.py` — the directory-set and file-placement gate
      dep: HOUSE-00001 · sys: ci · plat: CI · pri: MUST
- [x] HOUSE-00023 — Establish the naming conventions document: ids, files, namespaces, content names, JSON keys
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
      files: docs/conventions.md
      accept: covers cell/portal/light/interactable/asset/material/sound id grammars with examples and the regex each must match
- [x] HOUSE-00024 — Define the C++ error-handling policy: `Result<T>` for recoverable, exceptions only at the `Game` boundary and for genuinely exceptional content failures, `assert` for invariants
      dep: HOUSE-00023 · sys: util · plat: ALL · pri: MUST
      files: docs/conventions.md, include/cnahouse/util/Result.hpp
      note: (2026-09-06) `Result<T>`, `Result<void>`, `Error` and `ErrorCode` are implemented
            and were verified by compiling a standalone behavioural check against the header
            with `-std=c++23 -Wall -Wextra -Wpedantic -Werror`, in both the assert-enabled and
            `-O2 -DNDEBUG` configurations, and running it: value and error paths, context
            accumulation, `ValueOr`, move-only payloads, `operator->`, `Result<void>` and error
            equality all pass. The header is self-contained and `clang-format`-clean. Its
            permanent GoogleTest unit test lands with the test harness in phase 2 — no test
            file was committed that this session could not run.
- [x] HOUSE-00025 — Define the logging policy and implement `util/Log`: levels, categories, rate limiting, ring buffer, file + stderr sinks
      dep: HOUSE-00024 · sys: util · plat: ALL · pri: MUST
      files: src/util/Log.cpp|hpp
      accept: (1) a message repeated 1 000× in a frame is logged once with a count; (2) categories can be filtered at runtime
      verify: unit LogTests.*
      note: (2026-09-06) `util/Log` with six levels, 18 categories, a 512-record ring buffer, a
            file sink and per-frame rate limiting. Both acceptance points verified by `LogTests.*`:
            a message repeated 1 000× in one frame produces **one** record carrying
            `repeats == 1000`, and categories are filtered at runtime by name.
      finding: **the limiter is per FRAME, not per session**, because suppressing across frames
            would hide a problem that is still happening — the opposite of what a rate limiter is
            for. And an unknown category name from `--log=` is **reported**, never ignored: a
            misspelled category is indistinguishable from a subsystem that is simply quiet, which
            is the worst possible failure for a diagnostic option.
      finding: the limiter holds indices into the ring, so evicting the oldest record shifts every
            one of them. `LogTests.TheLimiterSurvivesTheRingWrappingAround` is the test that would
            otherwise have found a repeat counted against the wrong record — silently, and only
            under load.
- [x] HOUSE-00026 — Implement `util/Ids`: interned string ids with a stable 32-bit hash, a debug-only reverse map, and a compile-time literal form
      dep: HOUSE-00024 · sys: util · plat: ALL · pri: MUST
      accept: (1) collisions are detected and fatal at load; (2) `Id("L0_KITCHEN")` is constexpr-comparable
      verify: unit IdsTests.*
      note: (2026-09-06) `util/Ids`: FNV-1a 32-bit, `constexpr`, with a registry that detects
            collisions and a reverse map for diagnostics. Both acceptance points verified by
            `IdsTests.*`: `Id::Of("L0_KITCHEN") == Id::Of("L0_KITCHEN")` is a `static_assert`, and a
            collision is detected with **both** colliding names reported.
      finding: the collision test uses a **real** FNV-1a pair — `"n512789"` and `"n749192"` both
            hash to `0xEB03B14B` — found by searching once, offline, and pinned. An earlier version
            searched at run time, took 15 seconds and then skipped when it found nothing, which is a
            test that reports success for having failed to look. The pinned pair exercises the
            genuine detection path in microseconds.
      finding: the reverse map is present in **every** build, not only debug ones. A log line naming
            `0x9a3f21c4` instead of `L0_KITCHEN` is a log line nobody can act on, and the map costs
            a few hundred kilobytes for a house with a few thousand ids.
- [x] HOUSE-00027 — Implement `util/Rng`: xoshiro256++ with explicit state save/restore, plus a `Bag<T>` shuffled-draw helper for round-robin sample selection
      dep: HOUSE-00024 · sys: util · plat: ALL · pri: MUST
      accept: (1) the same seed reproduces the same 10⁶ draws; (2) state round-trips through JSON
      verify: unit RngTests.*
      note: (2026-09-06) `util/Rng`: xoshiro256++ with SplitMix64 seeding, plus `Bag<T>`. Both
            acceptance points verified by `RngTests.*`: the same seed reproduces **10⁶** draws
            exactly, and the state round-trips through 64 hex characters.
      finding: **`Bag<T>` had a real bug and the test caught it.** `lastIndex_` was set inside
            `Refill` to the item about to be drawn *first*, so the anti-repeat check compared the
            new cycle against itself and let a boundary repeat through — on draw 16 of the test. It
            is now updated on the draw. That repeat is the entire reason the type exists: it is what
            a player hears as "the dog barked the same way twice".
      finding: `NextInt` uses Lemire's rejection method rather than a modulo, because a modulo over
            a range that does not divide 2^64 biases the low values — invisibly, and exactly where a
            designer would later wonder why the first row of a table comes up slightly too often.
            The wide multiply it needs is written in standard C++ rather than `unsigned __int128`,
            which `-Wpedantic` rejects and the Web target does not have.
- [x] HOUSE-00028 — Implement `util/Json`: a thin, typed wrapper over `System::Text::Json` giving `RequireString/Int/Float/Vector3/Box/Array/Object` with path-carrying error messages
      dep: HOUSE-00024 · sys: util · plat: ALL · pri: MUST
      accept: an error names the file, the JSON path and what was expected
      verify: unit JsonTests.* with 20 malformed fixtures
      note: (2026-09-06) `util/Json`, a typed path-carrying wrapper over `System::Text::Json`,
            with `Result<T>` and `util::Error` beneath it. The acceptance is verified by
            `JsonTests.*` with **twenty malformed fixtures**, each asserting the error's **code**,
            that the message names **what was expected**, and that the context names the **JSON
            path** — so a wrong value in `rooms[3].portals[1].width` says exactly that.
      finding: the wrapper exists because most failures this project will ever see are authoring
            mistakes in JSON, and the difference between a usable project and an infuriating one is
            whether the error says `rooms[3].portals[1].width: expected a number, found a string`
            or `std::bad_variant_access`. A `BoundingBox` is additionally validated as min ≤ max per
            axis: an inverted box passes every later type check and then silently contains nothing,
            which shows up as a room that is invisible for no reason.
- [ ] HOUSE-00029 — Implement `util/SmallVector` and `util/FixedString`, or decide against them after measuring; record the decision
      dep: HOUSE-00024 · sys: util · plat: ALL · pri: SHOULD
- [x] HOUSE-00030 — Set up the git hooks / CI pre-commit equivalent running clang-format and the lint gates
      dep: HOUSE-00020, HOUSE-00022 · sys: ci · plat: CI · pri: SHOULD
- [x] HOUSE-00031 — Write `docs/performance-log.md` with its row format and the first (empty) table
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00032 — Write `docs/content-authoring.md`: the authoring conventions of `cna-house.md` §18.2 in checklist form for whoever makes an asset
      dep: HOUSE-00023 · sys: — · plat: TOOL · pri: MUST
- [x] HOUSE-00033 — Write `docs/world-format.md`: the JSON schemas of §15 as a reference, with a worked example per file
      dep: HOUSE-00023 · sys: — · plat: TOOL · pri: MUST
- [x] HOUSE-00034 — Decide and record the versioning scheme (`MAJOR.MINOR.PATCH+gHASH`) and where the version string lives
      dep: HOUSE-00001 · sys: app · plat: ALL · pri: MUST
- [x] HOUSE-00035 — Add `CMakePresets.json` with the presets `linux-debug`, `linux-release`, `linux-asan`, `linux-ubsan`, `headless`, `gl33`, `web` (unbuilt for now)
      dep: HOUSE-00001 · sys: app · plat: ALL · pri: MUST
      accept: each preset sets `CCACHE_DIR`/`CCACHE_BASEDIR` launchers and the right build directory from the closed list
      note: (2026-09-06) `CMakePresets.json` with all seven presets plus a hidden `base` carrying
            the ccache launchers and `CCACHE_DIR`/`CCACHE_BASEDIR`. Each names a build directory
            **from the closed list**: `linux-debug` and `linux-release` share `build/`,
            `linux-asan` uses `build-asan/`, `linux-ubsan` uses `build-ubsan/`, and `headless`,
            `gl33` and `web` share `build-consumer/` — which is `HOUSE-00117`'s measured answer,
            one binary directory per renderer, reconfigured in turn rather than a new name each
            time. Four test presets match the four `ctest` labels.
      finding: `CCACHE_DIR` is `$env{HOME}/.cache/ccache`, not `/rv/cnaccache`. They are the same
            physical cache — the second is a symlink to the first — and the home path is the one
            ccache uses by default, so code that forgets to export it still lands in the one cache
            instead of starting a second.
- [x] HOUSE-00036 — Record the openeggbert build rules compliance checklist in `AGENTS.md`: one ccache, reuse build dirs, never build in `/tmp`, `~/deps` for third-party, watch RAM
      dep: HOUSE-00006 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00037 — Create `licenses/` with `THIRD-PARTY-ASSETS.md` as a generated stub and the generator's contract
      dep: HOUSE-00003 · sys: — · plat: TOOL · pri: MUST
- [x] HOUSE-00038 — Define the issue/task workflow: one task = one commit, commit message references the `HOUSE-` id, statuses updated in this file
      dep: HOUSE-00006 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00039 — Define and document the screenshot-scene naming scheme used by the render tests and the docs
      dep: HOUSE-00023 · sys: debug · plat: ALL · pri: MUST
- [x] HOUSE-00040 — Add `docs/asset-review/` with the hero-asset sign-off template (4 views, reference photo, verdict, reviewer, date)
      dep: HOUSE-00018 · sys: — · plat: TOOL · pri: MUST
- [x] HOUSE-00041 — Record the definition of done for a task: builds, tests pass, lint green, docs updated, plan checkbox ticked, one commit
      dep: HOUSE-00038 · sys: — · plat: ALL · pri: MUST
- [x] HOUSE-00042 — Pre-probe foundation checkpoint: the skeleton, the documents and the gates are established, and capability probing may begin
      dep: HOUSE-00001, HOUSE-00003…HOUSE-00024, HOUSE-00030…HOUSE-00034, HOUSE-00036…HOUSE-00041 ·
           sys: — · plat: ALL · pri: MUST
      accept: (1) the §17.5 directory skeleton exists and `check_layout.py` asserts it; (2) all
              twelve ADRs exist; (3) the conventions, workflow, versioning, screenshot-scene,
              content-authoring, world-format, xna-deviations, performance-log, asset-review and
              licensing documents exist; (4) `tools/ci/run_checks.sh` is green on the committed
              tree and `check_xna_only.py --selftest` detects all 14 planted violations;
              (5) `Result<T>` compiles and behaves under `-Wall -Wextra -Wpedantic -Werror` in both
              the assert-enabled and `-O2 -DNDEBUG` configurations; (6) the tree is clean and no
              build product is tracked
      verify: `tools/ci/check_xna_only.py --selftest`; `tools/ci/run_checks.sh`;
              `python3 -m py_compile tools/ci/*.py`; probe `p0-result-check`; `git diff --check`;
              `git status --porcelain`
      correction: (2026-09-06) this task was "First commit: the skeleton, the documents, the gates /
                  dep: HOUSE-00001…HOUSE-00041 / accept: CI green on a fresh clone". Both the
                  dependency list and the acceptance criterion were unsatisfiable, and they
                  deadlocked the plan — see [Planning corrections](#planning-corrections). It is now
                  the **stage-A gate**: it certifies the foundation that can exist before the
                  application build, and nothing else. The seven stage-B tasks it no longer depends
                  on stay open with their own criteria intact.
      note: (2026-09-06) all six criteria checked at `5631b95` + the planning correction.
            `run_checks.sh` green (layout, xna-only, clang-format); the self-test detected 14/14
            fixtures; `p0-result-check` passed in both configurations and was then deleted.
            "CI green on a fresh clone" is not lost — it is the acceptance criterion of
            `HOUSE-00133` (lint CI) and `HOUSE-00134` (build + `unit` CI), where it is executable.

---

## Phase 1 — CNA capability verification

**Goal.** Re-prove every claim in `cna-house.md` §5 against the actual checkout, settle the open
questions that block architecture, and record the evidence. Probes are built in
`/rv/data/development/github.com/openeggbert/cna-house/build-probe/` with a `p1-` file-name prefix
(never a per-ticket directory, never in `/tmp`, never in the scratchpad) and **deleted when their
finding is written down**.

**Exit.** `docs/cna-capability-report.md` exists with a row per claim, a verdict and the probe
that produced it; `BL-09` is settled; every probe binary is removed.

- [x] HOUSE-00061 — Create `docs/cna-capability-report.md` with the claim table skeleton (one row per §5 row)
      dep: HOUSE-00042 · sys: — · plat: LNX · pri: MUST
      note: (2026-09-06) 40 claim rows across §5.1–§5.8, plus the configuration under test, the
            verdict vocabulary, an XNA-only measured section and the blocker table. Rows carry
            `PENDING` until a probe actually runs; `NOT PROBED` is used only where a reason is
            given.
- [x] HOUSE-00062 — Probe: build a minimal `Game` against `../cnanext` with `OPENGLES3` and confirm it opens a window, clears and presents
      dep: HOUSE-00061 · sys: app · plat: LNX · pri: MUST
      accept: 300 frames at the target resolution with no GL error and no validation warning
      verify: probe `p1-hello` output + a screenshot
      note: (2026-09-06) PASS. 300 frames at exactly 1600×900, `Exit()` clean, process exit 0.
            EasyGL reported `OpenGL ES 3.2 Mesa 25.0.7`. No GL error and no validation warning
            attributable to the probe; the one stderr line is an unrelated GTK portal warning
            from SDL3's file-dialog init. **Finding:** the samples' `CNA/Platform/Entrypoint.hpp`
            is a forbidden `CNA/` include and is *not needed* — under SDL3 it expands to nothing
            off Android/iOS, and a plain `int main()` linked and ran. Sizes `HOUSE-00127`.
- [x] HOUSE-00063 — Probe: confirm `CNA_CNAEXT=OFF` genuinely removes the engine layer (no `CNA::Graphics` symbols in the binary)
      dep: HOUSE-00062 · sys: ci · plat: LNX · pri: MUST
      accept: `nm -C` on the linked binary finds zero `CNA::Graphics::` symbols
      verify: recorded in the capability report; becomes a permanent CI check (`HOUSE-00133`)
      note: (2026-09-06) PASS on the stated criterion: 0 symbols, against a **positive control** of
            6 277 in the `CNAEXT=ON` archive (37.4 MB → 69 kB with the option off), so the check
            demonstrably detects what it asserts absent.
      finding: `CNA_CNAEXT=OFF` removes the `CNA::Graphics::` engine layer but **not** the other
            forbidden identifiers. `SupportsCapability`, `GraphicsCapability`,
            `getSkinsEXTProperty`, `setOwnedResources`, `ShaderEffect`, `PbrEffect`,
            `SkinnedPbrEffect` and `SkinnedModelEXT` live in `Microsoft::Xna::Framework::Graphics`
            in the always-compiled core and are all present in the linked binary. For those,
            `check_xna_only.py` is the **only** gate, not a second line of defence — it must never
            be relaxed. `HOUSE-00136` must be scoped to `CNA::Graphics::` and `AvatarRenderer`
            (both genuinely 0) and state why the rest cannot be asserted at the symbol level.
            CNA does offer `-DCNA_STRICT_XNA_API`, which turns `CNAEXT`-tagged calls into compile
            errors; it was verified to catch `Model::getSkinsEXTProperty()`, but it also fires on
            genuine XNA 4.0 (`IVertexType`, `TouchPanel::MAX_TOUCHES`, `ContentTypeReaderBase`,
            8 errors in a clean TU), so it is not adoptable today. Recorded, not patched.
- [x] HOUSE-00064 — Probe: `ContentManager` resolution order — build one asset as `.cnb` and one as `.xnb` with the same name and confirm `.xnb` wins
      dep: HOUSE-00062 · sys: content · plat: LNX · pri: MUST
      note: (2026-09-06) **`.xnb` wins** — confirmed as §5.1 claims. Measured by compiling two
            *different* payloads to one content name (red → `.xnb`, blue → `.cnb`) and reading the
            texels back: red. Consequence for `HOUSE-00126`: a stale `.xnb` silently shadows the
            `.cnb` the build just produced, so the content output tree must never hold both. The
            literal-path middle tier of the claim was not exercised.
- [x] HOUSE-00065 — Probe: load a `Texture2D` from `.cnb`; verify dimensions, format and a byte-exact `GetData` round trip
      dep: HOUSE-00064 · sys: content · plat: LNX · pri: MUST
      note: (2026-09-06) PASS. 4×4, `SurfaceFormat::Color`, 16/16 texels byte-exact.
      finding: the round trip is exact **against the premultiplied model, not the source PNG**.
            The first run failed 15/16; the measured (1,0) texel `66,13,35,223` is the source
            `75,15,40,223` scaled by `223/255`. `TextureProcessor`'s `premultiplyAlpha` defaults
            to `true`, exactly as XNA 4.0's does, because `BlendState::AlphaBlend` — what
            `SpriteBatch::Begin()` selects by default — is the premultiplied blend. The probe was
            corrected to evaluate both models and report which fits (straight 1/16, premultiplied
            16/16). Consequences: golden-image tests must expect premultiplied values; the
            `DualTextureEffect` product of `HOUSE-00078` must not premultiply twice; nothing may be
            drawn with `BlendState::NonPremultiplied`.
- [x] HOUSE-00066 — Probe: load a `SpriteFont` built from a TTF through the `.spritefont` route; draw a string; verify glyph placement
      dep: HOUSE-00064 · sys: content · plat: LNX · pri: MUST
      note: (2026-09-06) PASS. `lineSpacing = 50`; `MeasureString("I")` = 13.0×51.0 and `×10` =
            130.0×51.0 (exactly linear at `<Spacing>0</Spacing>`). Placement was **measured**: the
            string was drawn into a `RenderTarget2D`, read back with `GetData`, and the ink bbox
            `x[14,49] y[15,47]` confirmed to start after the draw origin (10,8) and stay inside the
            measured 44×51 box. `MeasureString` is therefore a usable layout oracle for the HUD.
      licensing: the descriptor named a copy of the host's DejaVuSans inside `build-probe/`, which
            is git-ignored and was deleted with the probes. **No font entered the repository** and
            no redistribution claim is made. CNA resolves `<FontName>` to a file beside the
            descriptor before any system font, so `cna-house` always ships the `.ttf` next to the
            `.spritefont`.
- [x] HOUSE-00067 — Probe: load a `SoundEffect` from a 16-bit PCM WAV; play it; confirm audible duration
      dep: HOUSE-00064 · sys: audio · plat: LNX · pri: MUST
      note: (2026-09-06) PASS. A 1.0 s 440 Hz mono `pcm_s16le` @ 44.1 kHz compiled through
            `CNA.WavImporter -> CNA.SoundEffectProcessor -> CNA.SoundEffectContentWriter`, loaded,
            and reported `Duration` 1.000000 s. `SoundEffectInstance::getStateProperty()` was
            `Playing` immediately after `Play()`. The SDL3 mixer opened a **real** device
            (44 100 Hz stereo), so this is not a null-sink result.
- [x] HOUSE-00068 — Probe: confirm a **24-bit** PCM WAV is rejected (BL-06), and record the exact exception text
      dep: HOUSE-00067 · sys: audio · plat: LNX · pri: MUST
      accept: ~~the failure mode is documented so the pipeline can assert against it~~ →
              **the actual behaviour is measured and documented, and `BL-06` is corrected**
      correction: (2026-09-06) the task's premise was false, so its acceptance criterion could not
              be met as written. There is no failure mode to document. Per the phase-1 rule that a
              measurement is never massaged to match the architecture, the measured result is
              recorded and `cna-house.md` §6 `BL-06` is corrected instead. See
              [Planning corrections](#planning-corrections).
      note: (2026-09-06) **24-bit PCM is NOT rejected, by either path.** (1) `cna-content` converts
            it and warns: *"the source is 24-bit PCM and was converted to 16-bit PCM with
            round-to-nearest and saturation; this discards precision the source carried"* — exit
            code 0. (2) `SoundEffect::FromStream` on the untouched raw 24-bit WAV **accepted** it,
            no exception, duration 1.000000 s. The conversion is not merely silent but correct:
            compiled against the same signal authored as 16-bit, the two `.cnb` files differ in
            only 29 of 96 304 bytes, all header (asset name, lengths, fingerprint), and the PCM is
            byte-identical from byte 400 to EOF. CNA's 24→16 conversion equals ffmpeg's.
- [x] HOUSE-00069 — Probe: convert one NOX 24-bit file to 16-bit with ffmpeg and confirm it loads and sounds right
      dep: HOUSE-00068 · sys: audio · plat: TOOL · pri: MUST
      note: (2026-09-06) PASS. One representative source, chosen without inventorying the
            collection: `Electromagnetic_NOX_SOUND/Electromagnetic_Car_Dashboard_Loop_Mono_Elektrousi_01.wav`,
            `pcm_s24le` 48 kHz mono 7.964396 s, sha256 `55522170…0ac5ec`. Both a 44.1 kHz and a
            48 kHz 16-bit conversion compiled, loaded, played and preserved duration (7.964399 s
            and 7.964396 s).
      finding: **`BL-06`'s command is wrong and is corrected.** Measuring the two halves
            separately: 24→16 bit alone costs 0.0017 dB RMS, but adding `-ar 44100` costs
            0.889 dB RMS and 0.26 dB peak, because resampling 48 → 44.1 kHz lowpasses content this
            source carries. `-ar 44100` is dropped — the collection is uniformly 48 kHz, CNA played
            the 48 kHz asset correctly, and the mixer resamples at playback anyway. Given
            `HOUSE-00068`, the offline step is now kept for **provenance, not format**.
      licensing: this measured a format conversion using one file. **No redistribution clearance is
            claimed.** The collection's status remains exactly what `cna-house.md` §63 records and
            is settled only by its dedicated phase-4 licensing task.
- [x] HOUSE-00070 — Probe: build a simple static glTF through `cna-content` to `.cnb`, load as `Model`, draw with `BasicEffect`
      dep: HOUSE-00064 · sys: content · plat: LNX · pri: MUST
      accept: correct bounds, correct orientation, correct winding with `CullClockwise`
      note: (2026-09-06) PASS, 21/21 checks in probe `p1-static`. A self-authored `P1Static.glb`
            box with six distinct plane coordinates compiled through
            `CNA.GltfImporter -> CNA.ModelProcessor -> CNA.ModelContentWriter`. Bounds exact
            (`min(-1,-2,-3) max(4,5,6)`); positions, normals and UVs **bit-exact** against the
            authored values read back with the plain XNA `GetData`; indices in source order.
            Orientation confirmed twice — by the readback and by comparing the render's ink bbox
            `x[88,167] y[72,183]` against the analytic projection `x[88,168] y[72,184]`.
      finding: **the emitted vertex is 48 bytes, not `VertexPositionNormalTexture` (40).** The
            layout is Position@0/Normal@12/**Tangent(Vector4)@24**/TexCoord0@40 — the processor
            synthesises a `TANGENT` the source never authored. Reading a `Model` back with a
            built-in XNA vertex type silently misaligns every vertex after the first. Any
            `cna-house` geometry readback declares its own struct and validates it against
            `VertexDeclaration::GetVertexElements()`. Sizes `HOUSE-00126` and phase 44.
- [x] HOUSE-00071 — Probe: confirm the glTF winding conclusion — render the same model with `CullClockwise` and `CullCounterClockwise` and record which is correct
      dep: HOUSE-00070 · sys: rendering · plat: LNX · pri: MUST
      note: (2026-09-06) **`CullClockwise` is correct**, measured by pixel count into a 256²
            `RenderTarget2D`: `CullNone` 8 960 px, `CullClockwise` 8 960 px,
            `CullCounterClockwise` **0** px.
      finding: **a closed solid cannot measure winding.** Run against the box fixture, all three
            cull modes covered an identical 8 960 px — with the near faces culled you see the far
            faces through them and the silhouette does not change. The measurement needed a second
            fixture, `P1Quad.glb`, an open single-sided quad, where the wrong cull mode renders
            literally nothing. Recorded because it is an easy mistake to repeat in phase 6.
- [x] HOUSE-00072 — Probe: `Model` with a real multi-bone hierarchy from `.cnb` — verify `Bones`, `ParentBone`, `Root` and `CopyAbsoluteBoneTransformsTo`
      dep: HOUSE-00070 · sys: content · plat: LNX · pri: MUST
      note: (2026-09-06) PASS, 22/22 in probe `p1-hier`. `P1Hier.glb`: four nodes, depth three plus
            a sibling, a 90° Z rotation that does not commute with its translation, a non-uniform
            scale `(2, 0.5, 4)`, and one node authored as an explicit glTF `matrix`. The generator
            computes every expected local and absolute matrix in XNA's own convention and emits
            them as C++ literals, so the probe compares CNA against arithmetic rather than against
            another CNA call, and reports a *transposed* result distinctly from a merely wrong one.
      finding: three facts that change how `cna-house` is written.
            (1) **CNA inserts a synthetic `Root` bone** above the glTF scene root — bone count is
            *nodes + 1* and `Model::Root` is that synthetic bone, so every bone-index table must be
            built by **name lookup**, never by assuming node *i* is bone *i*. This is what
            `HOUSE-00074`'s joint mapping must be built on.
            (2) `CopyAbsoluteBoneTransformsTo`/`CopyBoneTransformsTo` **require a destination
            already sized to `Bones.Count`** and throw `destinationBoneTransforms` otherwise; they
            do not grow the vector.
            (3) The explicit-`matrix` node round-trips exactly, confirming `ConvertGltfMatrix`.
- [x] HOUSE-00073 — Probe: `Model::Meshes[i].BoundingSphere` is populated from `.cnb` (it is not from `.model.json` — BL-12)
      dep: HOUSE-00072 · sys: content · plat: LNX · pri: MUST
      accept: a non-degenerate sphere per mesh; if degenerate, record it as a new blocker and plan to compute bounds offline
      note: (2026-09-06) PASS on the stated criterion — the sphere is populated, non-degenerate and
            provably contains every vertex, so **`BL-12` is settled positively** and no offline
            bounds computation is needed for correctness.
      finding: it is **conservative, not minimal.** On the test box: centre `(1.726, -0.049, 2.114)`
            radius `7.686`, against the minimal `(1.5, 1.5, 1.5)` radius `6.225` — **23 % over** on
            radius and 1.55 off in Y, the signature of an incremental (Ritter-style) construction.
            Phase 9 and phase 41 must treat it as a cheap conservative reject only; wherever a tight
            bound is wanted, `cna-house` computes its own from the vertex data it already reads
            back. Sizes `HOUSE-00761` and the culling work.
- [x] HOUSE-00074 — **Probe: skinned glTF → `.cnb` → `Model`, and prove a project-owned sidecar can bind to it** (settles R-16). Read the skin's joint names and order from the source `.glb`, compile the model, compare against `Model::Bones`. `Model::Tag` is not read.
      dep: HOUSE-00072 · sys: content · plat: LNX · pri: MUST
      accept: (1) every glTF skin joint has a same-named `Model::Bones` entry; (2) the joint order the vertex blend indices reference is recoverable and stable across rebuilds; (3) `ParentBone`/`Transform` agree with the source hierarchy; (4) if any of these fails, the fallback — an explicit joint-name→bone-index map emitted into the sidecar — is recorded instead
      verify: probe `p1-skin`, output recorded in the capability report; the answer fixes the `.chanim` format of HOUSE-00166
      note: (2026-09-06) PASS, 20/20. Criterion (1) holds — all three skin joints resolve by name in
            `Model::Bones`. Criterion (3) holds — parents and local transforms match the source.
            Criterion (2) holds for stability: two builds of the same source produced a
            **byte-identical** `.cnb` (`dc702b15…f06379`).
      finding: **the answer is criterion (4), not (2): blend indices are SKIN-LOCAL.** They are
            `0..N-1` in `skin.joints` declaration order, **not** `Model::Bones` indices — measured
            0/10 vertices under the bone hypothesis and 10/10 under the skin-local one. So the
            `.chanim` sidecar of `HOUSE-00166` **must** carry the skin's joint names in blend-index
            order; nothing in the compiled `Model` reproduces that list. At load the runtime maps
            name → `Model::Bones` index (the collection has a by-name indexer), and the palette
            handed to `SkinnedEffect::SetBoneTransforms` is built in the **sidecar's** order.
            R-16 is settled without `Model::Tag` and without `getSkinsEXTProperty()`.
            The skinned vertex is 68 bytes: the 48-byte static layout plus `BlendWeight`
            (`Vector4`@48) and `BlendIndices` (`Byte4`@64) — `Byte4` caps a skin at 256 joints at
            the vertex level, well above `SkinnedEffect`'s 72.
- [x] HOUSE-00075 — Probe: animate that model through a hand-written clip evaluator and `SkinnedEffect::SetBoneTransforms`; confirm visible deformation
      dep: HOUSE-00074 · sys: animation · plat: LNX · pri: MUST
      note: (2026-09-06) PASS, 11/11 in probe `p1-skinanim`, which implements the evaluator
            `cnahouse::anim` will ship — per-joint TRS tracks, `Lerp`/`Slerp`, composition in XNA's
            order, a parent walk to absolute matrices, and a palette built from **project-owned**
            inverse bind matrices. Checked analytically before anything was drawn: the bind pose
            skins to the identity for every joint, and the bend carries the tip from `(0,4,0)` to
            exactly `(-2,2,0)`. Deformation then **measured**: bind pose 1 785 px, bbox
            `x[118,138] y[86,170]`; bent pose 1 569 px, bbox `x[86,143] y[118,170]`; 1 800 pixels
            differ. The silhouette moves 32 px left and 32 px down — what a +90° turn about `+Z` at
            the middle joint predicts, and what a scale or a translation would not produce.
      finding: **`SkinnedEffect` refuses `LightingEnabled = false`** — *"SkinnedEffect does not
            support setting LightingEnabled to false."* — exactly as XNA 4.0's does. There is no
            flat unlit skinned draw. Anything drawn unlit and skinned uses a full ambient term
            instead. Sizes the avatar and creature work of phases 33–37.
- [x] HOUSE-00076 — Probe: a two-skin glTF **split offline into one `.glb` per skin**; confirm each part compiles to a single-skin `Model` that binds to its own sidecar and that the parts reassemble on a shared skeleton. `getSkinsEXTProperty()` is not called and must not be needed.
      dep: HOUSE-00074 · sys: content · plat: LNX · pri: MUST
      accept: (1) the split parts render identically to the unsplit source; (2) the attachment-bone record round-trips; (3) the finding is written into `cna-house.md` §21.3 and sizes HOUSE-00224
      verify: probe `p1-skinsplit`
      correction: (2026-09-06) criterion (1) as written is unmeasurable, because **there is no
            loadable unsplit source.** `CNA.ModelProcessor` refuses a multi-skin glTF outright —
            *"glTF produced 2 Model documents; set ModelProcessor bool parameter
            `generateChildAssets` to true to publish the deterministic multi-Model output set"*,
            exit 1. That is stronger than the architecture assumed (the one-skin rule enforces
            itself at build time), but it means the comparison had to be reformulated: the probe
            measures that the **two independent split routes agree pixel for pixel**, driven from
            one pose of one shared skeleton evaluated once.
      note: (2026-09-06) PASS, 13/13. Route (a), the pipeline's own split via
            `"generateChildAssets": {"type":"bool","value":true}` in the asset config, publishes
            `P1TwoSkin.cnb` (the lexicographically first group) and `P1TwoSkin_P1SkinB.cnb`, both
            ordinary `Load<Model>()` names. Route (b), the project-owned `p1-split-skins.py`,
            publishes one `.glb` per skin plus a `.attach.json`. **0 differing pixels** between the
            two routes; both parts drew (1 287 px left, 1 289 px right). Criterion (2) holds: the
            attachment record round-trips through `System::Text::Json` — shared skeleton root,
            attachment bone, and the skin's joint names in blend-index order. Criterion (3) done:
            `cna-house.md` §21.3 rewritten.
      finding: **route (a) becomes the default** — one line of asset config replaces a project-owned
            tool, and the child assets are ordinary logical content names. `HOUSE-00224` implements
            the offline splitter as the fallback for sources whose generated child names are
            unacceptable, not as the primary route.
      finding: **`Text.Json` is not in CNA's default sharp-runtime component set**, and CNA does not
            link it, so its include directories do not reach a consumer through the `CNA` target.
            A consumer must add `Text.Json` to `SHARP_RUNTIME_COMPONENTS` **and** link
            `SharpRuntime::Text.Json` itself. `cna-house`'s world data, saves and sidecars are all
            `System::Text::Json`, so `HOUSE-00121`/`HOUSE-00122` must carry both lines.
      finding: a second `ContentManager` needs the `Game`'s service provider —
            `ContentManager(nullptr)` throws *"no GraphicsDevice is available from the service
            provider"* at the first `Load<Model>`. Sizes `HOUSE-00858`'s per-pack managers.
- [x] HOUSE-00077 — Probe: `SkinnedEffect` bone-count limit — confirm 72 accepted, 73 throws
      dep: HOUSE-00075 · sys: rendering · plat: LNX · pri: MUST
      note: (2026-09-06) PASS. `SkinnedEffect::MaxBones == 72`; `SetBoneTransforms` accepts exactly
            72 and throws `boneTransforms exceeds MaxBones.` at 73. The cap is real and enforced,
            so the avatar rig of phase 35 is budgeted against 72, not against a hoped-for larger
            number.
- [x] HOUSE-00078 — Probe: `DualTextureEffect` — albedo × lightmap on a quad with two UV channels; confirm the second channel reaches the effect
      dep: HOUSE-00070 · sys: rendering · plat: LNX · pri: MUST
      accept: a checker albedo × a gradient lightmap produces the analytic product within 2/255
      note: (2026-09-06) PASS. **64/64 texels within 1/255**, worst delta 1. Channel 1 carries a
            *mirrored* X coordinate, so a `TEXCOORD0`-for-both implementation would have failed on
            most texels; none did. XNA has no built-in two-channel vertex type — the probe declares
            its own 28-byte `VertexDeclaration`, which is what phase 12 does too.
      finding: the FNA `*2` doubling factor is present — 128 × 128 reads back as **128**, not 64.
            So a lightmap texel of 0.5 grey means *no change*, not half brightness, and the phase-12
            bake targets that midpoint. This is invisible to any test using saturated 0/1 values.
- [x] HOUSE-00079 — Probe: multi-pass additive lighting — draw the same chunk twice with `Opaque` then `Additive` + `DepthStencilState` depth-equal, and confirm no z-fighting and correct addition
      dep: HOUSE-00078 · sys: rendering · plat: LNX · pri: MUST
      accept: the sum is exact; a depth-equal second pass draws every pixel of the first
      note: (2026-09-06) PASS, both criteria. **0 pixels rejected** by the depth-equal test, and the
            sum is exact on all 3 249 covered pixels: 60 + 40 = 100, and 60 + 40 + 40 = 140 for the
            three-light case. Measured on a quad **tilted in depth** on purpose — a screen-parallel
            quad has constant interpolated depth and would pass even on hardware whose two passes
            disagree, so it would measure nothing. Tier S's three-light room is sound.
      finding: **a re-bound render target needs `RenderTargetUsage::PreserveContents`.** The default
            is `DiscardContents`, so the natural draw/unbind/read/rebind/draw sequence silently
            loses the first pass. Sizes `HOUSE-00631` and the phase-16 light passes.
- [x] HOUSE-00080 — Probe: `AlphaTestEffect` cutoff behaviour and two-sided rendering for foliage
      dep: HOUSE-00070 · sys: rendering · plat: LNX · pri: MUST
      note: (2026-09-06) PASS, 10/10. Measured on a 256×1 alpha ramp — one texel per alpha value —
            so the cutoff column *is* the threshold. At reference 128: `Greater` keeps 129…255 (127
            texels), `GreaterEqual` keeps 128…255 (128), `Less` keeps 0…127 (128), `Equal` keeps
            exactly one, `Always` all 256, `Never` none. Every one is XNA's semantics **to a single
            alpha value**. Two-sided: a back-facing card is invisible under `CullClockwise`, appears
            under `CullCounterClockwise`, and `CullNone` draws it from both sides with identical
            coverage — the foliage state works.
      finding: **procedurally authored geometry does not inherit the glTF winding convention.** The
            probe's first quad was wound top-left → top-right → bottom-right, which in normalised
            device coordinates (+Y is **up**) is *clockwise*, and the whole quad vanished under
            `CullClockwise` — every check failed for one fixture reason. `cna-house` generates
            geometry procedurally in phases 6, 10, 25 and 27; each generator must be wound
            counter-clockwise to match the imported assets, and each needs its own coverage
            assertion.
- [x] HOUSE-00081 — Probe: `EnvironmentMapEffect` with a baked `TextureCube`; confirm sampling and the Fresnel term
      dep: HOUSE-00070 · sys: rendering · plat: LNX · pri: SHOULD
      note: (2026-09-06) PASS, 10/10. Six distinctly coloured cube faces make "which face was
            sampled" a single pixel read, compared against `reflect(-E, N)` computed in C++: head-on
            → `+Z`, +45° about `+Y` → `+X`, −45° → `-X`, all exact. `EnvironmentMapAmount` is a
            linear blend weight (0 → `(0,0,0)`, 0.5 → `(100,5,100)`, 1 → `(200,10,200)`). Fresnel
            behaves as defined: at factors 1 and 4 the grazing angle is markedly more reflective
            than head-on; at factor 0 the weighting is uniform, which is the control.
      finding: `AmbientLightColor` **does** reach this effect (0.5 grey → `(128,128,128)`), although
            it appears in no uniform of EasyGL's environment-map fragment shader — it is folded into
            the emissive term before upload. Recorded because reading the shader alone would suggest
            the opposite.
- [x] HOUSE-00082 — Probe: `BasicEffect` three directional lights + ambient + specular + fog, all simultaneously
      dep: HOUSE-00070 · sys: rendering · plat: LNX · pri: MUST
      note: (2026-09-06) PASS, **byte-exact**. Tier S *is* `BasicEffect`, so the probe implements
            the lighting model in C++ and asserts the pixel, adding one term at a time so a
            disagreement would localise. All six terms together: measured `(92,90,86)`, computed
            `(92,90,86)`, delta **0**. Diffuse-only and specular-only stages also delta 0; the two
            ends of the fog ramp are within 1/255. The three lights were given different directions
            *and* different colours precisely so a model that dropped one could not still look
            plausible. `PreferPerPixelLighting` on and off agree exactly on a constant-normal
            surface, as they must.
      finding: the confirmed composition order is `litRGB = (ambient + Σ diffuse_i·NdotL_i)·
            DiffuseColor + Emissive`, then **specular added after the diffuse product and scaled by
            the final alpha**, then `mix(FogColor, colour, fogFactor)` with
            `fogFactor = 1 - (d - FogStart)/(FogEnd - FogStart)` on the **view-space** distance.
            Phase 12's material mapping and phase 16's light budget are computed against this, not
            against a remembered formula.
- [x] HOUSE-00083 — **Probe: `RenderTarget2D` with `SurfaceFormat::Single`, 2048², `DepthFormat::Depth24` — create, render depth, bind as an effect texture, read back** (settles `BL-09` / `Q-01`)
      dep: HOUSE-00062 · sys: rendering · plat: LNX · pri: MUST
      accept: either it works (record: Tier E uses a float shadow map) or it fails (record the exact error; Tier E packs depth into RGBA8)
      verify: probe `p1-rtsingle`; the answer is written into `docs/cna-capability-report.md` and `cna-house.md` §6 BL-09 is updated
      note: (2026-09-06) **IT WORKS — `BL-09`/`Q-01` settled positively, 10/10.** All four stages
            executed separately: create 2048² `Single`+`Depth24`; bind, clear and draw; read back
            with `GetData(float*)`; bind as an effect texture and sample. The readback is
            **bit-exact** — 3 396 649 drawn texels all exactly `0.625` (a value chosen because it is
            exactly representable, so any change would be real rather than rounding), 797 655
            cleared texels exactly `0`, nothing else. Sampled through `BasicEffect` it arrives as
            159/255.
      finding: **Tier E uses a real float shadow map.** The RGBA8 depth-packing fallback is not
            needed, and the pack/unpack in every shadow lookup goes away. The fallback path was
            exercised in the same probe anyway, so it is known good whichever way this went. Sizes
            `HOUSE-00633` and the phase-16 shadow work.
- [x] HOUSE-00084 — Probe: `RenderTarget2D` `Color` with mips and MSAA; bind, clear, draw, unbind, sample
      dep: HOUSE-00083 · sys: rendering · plat: LNX · pri: MUST
      note: (2026-09-06) PASS at MSAA 0 **and** MSAA 4. A 64×64 mipped `Color` target reports the
            full 7-level chain and, at MSAA 4, reports `MultiSampleCount == 4` — so the multisample
            allocation is real, not silently dropped. Level 0 holds exactly the drawn `(128,64,191)`
            in both cases, and after unbinding it samples back through `BasicEffect` as exactly
            `(128,64,191)` — the step an implicit-resolve bug would break.
- [x] HOUSE-00085 — Probe: confirm `Clear(ClearOptions::Stencil)` is ignored and `ReferenceStencil` has no effect (BL-02), and record it
      dep: HOUSE-00084 · sys: rendering · plat: LNX · pri: MUST
      correction: (2026-09-06) **the premise is FALSE — stencilling works**, so the task could not
            be closed as written. Per the phase-1 rule, the measurement is recorded and
            `cna-house.md` §6 `BL-02` is corrected rather than the measurement massaged. Same
            pattern as `HOUSE-00068`.
      note: (2026-09-06) The probe was built so a working stencil and an inert one give *different
            images*, and so neither verdict could be inferred from an absent exception: clear
            colour/depth/stencil to 0 on a `Depth24Stencil8` target; pass 1 stamps
            `ReferenceStencil = 1` with `Always`+`Replace` over the **left half only**; pass 2 draws
            the **whole** quad white under `Equal`. An inert `ReferenceStencil` would cover
            everything. Measured: **left half 2048/2048 lit, right half 0/2048.**
      finding: the stencil buffer is available to `cna-house` after all. Anything designed around an
            inert stencil — mirror and window-portal masking in particular — may use it. Sizes
            phases 9 and 15.
- [x] HOUSE-00086 — Probe: confirm EasyGL MRT attachment 1 stays black (BL-03), and record it
      dep: HOUSE-00084 · sys: rendering · plat: LNX · pri: MUST
      note: (2026-09-06) Confirmed **for the stock-effect case**. `SetRenderTargets` with two
            `Color` targets bound at once succeeded; after one `BasicEffect` draw, attachment 0 held
            `(255,128,64)` and attachment 1 held `(0,0,0)`.
      scope: this shows the renderer does not broadcast a single-output draw to every attachment,
            and that binding two targets is not itself an error. It does **not** show what a
            *compiled* `.fx` declaring two outputs would do — so the task was **reopened** once
            `HOUSE-00087` proved compiled effects work.
      correction: (2026-09-06) **`BL-03`'s premise is FALSE.** A technique declaring `COLOR0` and
            `COLOR1` with deliberately different values wrote `(255,128,64)` to attachment 0 and its
            **own** `(32,223,96)` to attachment 1 — matching the authored `(0.125, 0.875, 0.375)` to
            within a rounding step. MRT works on EasyGL; attachment 1 stays black only because a
            stock effect never writes to it. `cna-house.md` §6 `BL-03` is corrected and Tier E may
            use multiple render targets.
- [x] HOUSE-00087 — **Probe: compile a trivial `.fx` through `cna-content --format xnb --fx-compiler <fxc> --fx-compiler-launcher wine`, load it as `Effect`, and draw with it**
      dep: HOUSE-00062 · sys: content · plat: LNX · pri: MUST
      accept: (1) the build succeeds; (2) `CompiledEffects` is true; (3) a named technique is selectable; (4) a parameter set changes the output
      verify: probe `p1-fx`; if it fails, `BL-04` is escalated and Tier E is deferred without blocking anything
      note: (2026-09-06) **PASS on all four acceptance points — Tier E is viable**, 17/17 in probe
            `p1-fxload`. `fxc.exe` from the June 2010 DirectX SDK under Wine, through
            `CNA.EffectSourceImporter -> CNA.EffectSourceProcessor -> CNA.XnbEffectWriter`, to a
            3 424-byte `.xnb`. At runtime: 2 techniques and 3 parameters discovered by name;
            `TintColor = (1, 0.5, 0.25, 1)` arrives as exactly `(255,128,64)`; changing it to
            `(0.25, 0.75, 1, 1)` gives exactly `(64,191,255)`. `BL-04` is closed, not escalated.
      finding: **the documented `--fx-compiler-launcher wine` does not work on its own, and the fix
            is ours.** `cna-content` builds the `fxc` command line with Unix absolute paths, and
            `fxc` is a Windows tool that introduces *options* with `/`, so it reports
            *"Unknown or invalid option '/tmp/cna-fx-0-…/effect.fxb'"*. Neither side is wrong — two
            conventions collide, and `--fx-compiler-launcher` is exactly the seam for it.
            `tools/effects/fxc-wine.sh` is now that launcher: it translates through `winepath -w`
            any argument that names an **existing** path, plus any argument following a path-taking
            option. Offline tooling, so ADR-0001 does not reach it; a machine without Wine simply
            builds no Tier E, which ADR-0003 already requires. Sizes `HOUSE-00230`.
      finding: **`Load<Effect>` does not compile.** `Effect` is neither copyable nor
            default-constructible and its reader is registered for `std::shared_ptr<Effect>`, so the
            call is `Load<std::shared_ptr<Effect>>(name)` — whereas `Load<Model>` returns *by value*.
            The two are not consistent and neither is guessable; both are now recorded.
- [x] HOUSE-00088 — Probe: a two-technique `.fx` with techniques switched by name per draw, mirroring SAMPLE-038
      dep: HOUSE-00087 · sys: rendering · plat: LNX · pri: MUST
      note: (2026-09-06) PASS. `Tint` and `Textured` were selected by name and drawn alternately
            within one frame. The fixture makes the switch **numerically** decidable rather than
            visually: a uniform 0.5-grey texture means `Textured` must halve every channel, so the
            same tint gives `(255,128,64)` under `Tint` and `(128,64,32)` under `Textured`.
            Switching back restored `(255,128,64)` bit-identically — an implementation that had
            silently kept one technique bound could not produce that pattern.
- [x] HOUSE-00089 — Probe: `SpriteBatch::Begin(effect)` with a compiled effect
      dep: HOUSE-00087 · sys: rendering · plat: LNX · pri: SHOULD
      note: (2026-09-06) PASS. `Begin(SpriteSortMode::Immediate, &BlendState::Opaque,
            &SamplerState::PointClamp, &DepthStencilState::None, &RasterizerState::CullNone,
            effect)` accepted the compiled effect and drew the sprite through it — a 0.5-grey sprite
            arrived at `(128,128,128)`. SpriteBatch supplies its own vertices but **not** the
            effect's transform: the effect's `WorldViewProj` had to be set to the equivalent
            `CreateOrthographicOffCenter` projection by hand, exactly as an XNA game does. Sizes the
            phase-43 debug overlay and phase-45 post-processing.
- [x] HOUSE-00090 — Probe: `OcclusionQuery` — visible and occluded quads; record whether `PixelCount` is a real count or a boolean on this driver at `OPENGLES3`
      dep: HOUSE-00062 · sys: rendering · plat: LNX · pri: MUST
      accept: the boolean/count verdict is recorded and drives the N×N grid design
      note: (2026-09-06) **It is a BOOLEAN — `BL-07` confirmed.** A visible quad covering an
            *analytic* 16 384 pixels reported `PixelCount == 1`; the same quad fully occluded
            reported `0`. The analytic area is what makes this decisive: a real tally would have
            returned ~16 384, and no threshold guessing was needed.
      finding: a coverage ratio computed from this count is `1/area`, not a fraction, so the **N×N
            grid approximation stays in the design** for phase 9. `isPixelCountPreciseEXT()` exists
            but is a `CNAEXT` identifier and is therefore forbidden — which is exactly why this has
            to be measured once here and encoded into the platform profile.
- [x] HOUSE-00091 — Probe: the same under `OPENGL33` to confirm a real `GL_SAMPLES_PASSED` count, validating the grid approximation later
      dep: HOUSE-00090 · sys: rendering · plat: LNX · pri: SHOULD
      note: (2026-09-06) **Confirmed, decisively.** `build-consumer/` reconfigured from the same
            source directory with `-DCNA_GRAPHICS_RENDERER=OPENGL33` (`OpenGL 4.6 Core, Mesa
            25.0.7`) and the whole performance probe re-run. For a quad of analytic area 16 384:
            `OPENGLES3` reports **1**, `OPENGL33` reports **16 384** — exact. Same probe, same
            fixture, same driver, same GPU, so the boolean degradation is a property of the **ES
            profile's query target**, not of CNA and not of this hardware. The N×N grid
            approximation of phase 9 is validated against a renderer that returns a true count.
      finding: the cross-renderer comparison also shows **`OPENGLES3` submits draws about 1.5×
            more cheaply** than `OPENGL33` here — 8.15 µs against 12.23 µs of CPU per
            `DrawIndexedPrimitives` — which reinforces ADR-0002's renderer choice. Instancing
            (13.7× vs 12.8×) and texture upload (430 vs 438 MiB/s) are renderer-independent to
            within run-to-run noise, so those may be treated as properties of the machine; the
            occlusion verdict emphatically may not. Feeds `HOUSE-00115`.
- [x] HOUSE-00092 — Probe: `DynamicVertexBuffer` + `SetData(..., SetDataOptions::Discard)` at 2 000 quads per frame; measure the cost
      dep: HOUSE-00062 · sys: rendering · plat: LNX · pri: MUST
      accept: a number in ms, recorded in the performance log; it sizes the particle budget
      note: (2026-09-06) **CPU submit 0.055 ms, 0.171 ms to GPU completion**, median of 21 samples
            after 3 discarded warm-up rounds, Release, 512² target. Dynamic geometry is nearly free
            — a particle budget in the low thousands is not the constraint the design assumed.
- [x] HOUSE-00093 — Probe: `DrawInstancedPrimitives` — does it work on EasyGL, and is it faster than N draws for 200 identical props?
      dep: HOUSE-00092 · sys: rendering · plat: LNX · pri: SHOULD
      accept: a measurement and a verdict; if it works well it becomes the vegetation and neighbourhood path
      note: (2026-09-06) **It works, and it is 13.7× faster.** 200 instances: one
            `DrawInstancedPrimitives` **0.156 ms** against **2.144 ms** for 200 separate draws. It
            becomes the vegetation and neighbourhood path, as the task hoped.
      scope: `BasicEffect` has no per-instance input, so this measures the **draw path** — that
            `DrawInstancedPrimitives` works, consumes a second stream at instance frequency 1, and
            is dramatically cheaper. A stock effect cannot *read* the per-instance offset; consuming
            it needs a Tier E `.fx`, which `HOUSE-00087` has now shown to be available. Sizes
            `HOUSE-00541` and phase 11.
- [x] HOUSE-00094 — Probe: 32-bit index buffers on EasyGL with a > 65 535-vertex chunk
      dep: HOUSE-00062 · sys: rendering · plat: LNX · pri: MUST
      note: (2026-09-06) PASS. A 70 000-vertex buffer with `IndexElementSize::ThirtyTwoBits`, whose
            drawn triangle is addressed by the **last three** indices — values needing 17 bits —
            produced 106 261 lit pixels against an analytic ~106 000. Truncation to 16 bits would
            have drawn different geometry entirely, which a smaller fixture could not have
            detected.
- [x] HOUSE-00095 — Probe: `AudioListener`/`AudioEmitter`/`Apply3D` — pan and attenuation across a 20 m sweep; record the curve
      dep: HOUSE-00067 · sys: audio · plat: LNX · pri: MUST
      accept: the measured attenuation curve is recorded so our own gain model can be calibrated against it
      finding: **`Apply3D` does not touch the public `Volume`, `Pan` or `Pitch`.** The first version
            of this probe read them back across a 20 m sweep and an ±10 m pan sweep and saw a flat
            curve — a probe bug with a real finding inside it. `Apply3D` stores attenuation, pan and
            Doppler in private state and composes them with the caller's values only when writing
            the mixer track. **A game cannot read back what CNA applied**, so `cna-house` must model
            the same curve rather than query it. Sizes phase 32.
      note: (2026-09-06) 7/7. What is measured and what is read are kept apart. **Measured:** the
            public properties are unchanged at every distance and position; and `Apply3D` is *not*
            inert — an instance put into pan mode and then played **refuses** a later `Apply3D`
            (*"Apply3D cannot be called on a playing instance that is not using 3D audio."*) while
            one aimed in 3D before playing accepts it while playing. An inert implementation could
            not produce that distinction. **Read from
            `modules/audio/src/Xna/SoundEffectInstance.cpp`, and labelled as read:**
            `attenuation = normalized >= 1 ? clamp(1/normalized, 0, 1) : 1` where
            `normalized = distance / DistanceScale`; `pan = clamp(rightDisplacement / distance,
            -1, 1)`. At `DistanceScale = 1`: 1 m → 1.0000, 2 m → 0.5000, 5 m → 0.2000,
            10 m → 0.1000, 20 m → 0.0500. **Full volume inside `DistanceScale`, then inverse
            *distance* beyond it** — not inverse-square, and not a falloff from zero. No HRTF, no
            cone or orientation term; `rightDisplacement` is projected onto the listener's own
            `Forward × Up` axis, so a rotated listener is handled.
- [x] HOUSE-00096 — Probe: confirm `DopplerScale = 0` and zero velocities produce no pitch change (BL-11)
      dep: HOUSE-00095 · sys: audio · plat: LNX · pri: MUST
      note: (2026-09-06) **`BL-11` holds, for a stronger reason than the blocker assumed.** When
            `emitter.DopplerScale * SoundEffect::DopplerScale` is zero the factor is set to exactly
            `1.0f` **without evaluating the Doppler math at all**, so there is no rounding path by
            which a pitch change could appear; zero velocities alone would also give 1.0. The public
            `Pitch` property is untouched by `Apply3D` in either case, which the probe measured.
- [x] HOUSE-00097 — Probe: concurrent `SoundEffectInstance` count — find the practical ceiling on this host
      dep: HOUSE-00095 · sys: audio · plat: LNX · pri: MUST
      accept: the number, recorded; the voice budget is set from it (32 is the design target)
      note: (2026-09-06) **512 of 512** looping instances reported `Playing`; CNA refused nothing up
            to the probe's own ceiling.
      finding: **there is no hardware limit discoverable through the XNA surface**, so the number
            the acceptance criterion asked for does not exist as a CNA-reported value. The 32-voice
            budget is therefore a design decision `cna-house` must **enforce itself** — CNA will not
            tell us when the mixer has been overcommitted. Sizes `HOUSE-00795` and phase 31.
- [x] HOUSE-00098 — Probe: `Video` + `VideoPlayer::GetTexture()` on a transcoded test clip; confirm frame advance and audio
      dep: HOUSE-00062 · sys: rendering · plat: LNX · pri: MUST
      note: (2026-09-06) PASS, 9/9. The clip is generated locally by `ffmpeg` from `lavfi` sources —
            64×64, 2 s, 10 fps, colour changing every half second over a 440 Hz sine. **Nothing
            downloaded, no third party's media**, so there is no licensing question and the expected
            pixel at any timestamp is known by construction. Compiled through
            `CNA.VideoImporter -> CNA.VideoProcessor -> CNA.VideoContentWriter`; metadata matches
            exactly. **Frame advance measured, not assumed:** sixteen `GetTexture` readbacks across
            the clip classified as `R R R G G G G B B B B B W W W W` at play positions 0.12 s …
            1.93 s — exactly the authored sequence. A player handing back frame 0 forever would pass
            a test that only asked "did a texture come back"; it could not pass this one. The mixer
            opened a real 44.1 kHz stereo device, so the soundtrack is live.
      finding: **a third `Load<T>` shape.** `Load<Video>` returns **by value**, like `Model` and
            unlike `Effect` (registered for `shared_ptr<Effect>`). None of the three is guessable,
            but CNA says so precisely when asked wrongly: *"holds a Video asset, which is not the
            type requested"*.
- [x] HOUSE-00099 — Probe: `VideoPlayer` behaviour with `CNA_ENABLE_VIDEO=OFF` — confirm `NotSupportedException` and that the rest still links (BL-05)
      dep: HOUSE-00098 · sys: rendering · plat: LNX · pri: MUST
      note: (2026-09-06) **`BL-05` settled**, 6/6, built in `build-consumer/` with
            `CNA_ENABLE_VIDEO=OFF`. `Play()` throws *"Video playback is unavailable because CNA was
            built without the optional FFmpeg video backend. Configure with `-DCNA_ENABLE_VIDEO=ON`,
            or use AUTO with all required FFmpeg development packages installed."* — a refusal that
            names its own fix. The probe deliberately keeps using the media API afterwards, because
            the linking half is the half that matters: `VideoPlayer` still constructs, still reports
            `Stopped`, and `Stop()` is still callable.
      finding: **`Load<Video>` still SUCCEEDS with the backend absent** — content loading is not
            gated, only playback is. So phase 21 can load its television content unconditionally and
            fail only at `Play`, which is a much easier shape to write than a load-time branch.
- [ ] HOUSE-00100 — Probe: `Mouse::GetState` + `SetPosition` recentring loop; measure the delta accuracy and any drift over 10 000 frames
      dep: HOUSE-00062 · sys: player · plat: LNX · pri: MUST
      status: **OPEN — INCONCLUSIVE, not failed.** The full 10 000 frames were run (deliberately not
            shortened: a fractional per-frame error is invisible in 100 frames and ruins a camera in
            10 000), but the environment could not supply the input the criterion needs.
      note: (2026-09-06) **Measured:** `Mouse::SetPosition(400,300)` **is** reflected by a
            `GetState` in the same frame; every one of the 10 000 subsequent frames then read
            `(0,0)`. **Cause**, from `modules/input/src/Xna/Mouse.cpp:117`: `GetState` returns a
            *snapshot* the platform layer maintains from SDL mouse-motion events. On an unattended
            desktop the pointer never enters or moves over the probe window, no motion event
            arrives, and the snapshot never leaves its initial value — so **no delta measured here
            is a delta** and no drift figure from this run means anything. Recorded as inconclusive
            rather than rounded to a pass. Needs a session with the pointer actually over the
            window; `HOUSE-00115` lists it.
      finding: two facts ARE established, and both change how the camera is written.
            (1) **`Game::IsActive` is not a proxy for "the mouse is usable"** — it was `true` on all
            9 999 frames while the snapshot never advanced, so a camera gating only on `IsActive`
            would consume garbage.
            (2) **`GetState` is event-driven, not a live cursor query** — the camera must seed its
            previous position from a real motion event and must never assume the cursor starts
            centred. Sizes phase 8.
- [x] HOUSE-00101 — Probe: `Keyboard`, `GamePad`, and the presence/behaviour of `TouchPanel` on desktop
      dep: HOUSE-00100 · sys: player · plat: LNX · pri: MUST
      note: (2026-09-06) PASS. `Keyboard::GetState` answers and `IsKeyDown` agrees with
            `GetPressedKeys` on every reported key; a key that is not on the keyboard reads as up.
            All four `GamePad` slots report **not connected**, and `GetCapabilities` agrees with
            `GetState` on every one — the *agreement* is the point, since a game trusting only one
            of them would be wrong half the time. `TouchPanel::GetCapabilities` and `GetState` can
            be called on desktop **without throwing**, report `IsConnected = false` and **0
            touches**, so phase 50 may poll them unconditionally rather than branching on platform.
      note: this task's dependency on `HOUSE-00100` is satisfied in substance — the probe ran and
            its input findings are recorded — even though `HOUSE-00100` itself stays open for a
            criterion this environment cannot exercise.
- [x] HOUSE-00102 — Probe: `StorageDevice`/`StorageContainer` — write, read, list and delete a file; record the resolved path
      dep: HOUSE-00062 · sys: persistence · plat: LNX · pri: MUST
      note: (2026-09-06) PASS, 13/13. `BeginShowSelector`/`EndShowSelector` complete with no UI on
            desktop; the device reports connected with 243 GB free; a container opens; a 276-byte
            file is written, appears in `GetFileNames`, reads back **byte-identical**, and deletes.
      finding: **the resolved path is not what `cna-house.md` §5 records.** §5 says
            `$XDG_DATA_HOME/<app>` with `<app>` implied to be the game.
            `StorageDevice.cpp:75` is `appName_.empty() ? "game" : appName_`, and the **only**
            setter is `SetAppNameEXT` — a `CNAEXT` identifier ADR-0001 forbids. Measured root:
            **`~/.local/share/game/P1Probe`**. An XNA-only game's saves therefore land under a
            directory literally called `game`, shared with every other CNA application.
            **The fix needs no extension:** the *container* name is ours through plain XNA, so
            `BeginOpenContainer("CnaHouse")` gives `.../game/CnaHouse/`. `HOUSE-00113` corrects §5
            and ADR-0008; sizes `HOUSE-00700`.
- [x] HOUSE-00103 — Probe: `System::Text::Json` — serialise and deserialise a nested object with arrays and floats; check round-trip precision
      dep: HOUSE-00102 · sys: persistence · plat: LNX · pri: MUST
      note: (2026-09-06) PASS. **6/6 floats return bit-identical**, compared by **bit pattern rather
            than epsilon** — a save that drifts one ulp per cycle still corrupts a long-running
            house, only slowly. The values were chosen to break a naive serialiser: `1/3`, `0.1`
            (repeating in binary), `1.1754944e-38`, `3.4028235e+38`, `-0.0` and `2.0`. Nested
            objects resolve by name to depth 4, a string array round-trips in order, and an integer
            stays an integer. `Text.Json` had to be added to `SHARP_RUNTIME_COMPONENTS` and linked
            directly — see `HOUSE-00076`.
- [x] HOUSE-00104 — Probe: `BoundingFrustum::GetCorners`, `Intersects(BoundingBox)`, `BoundingBox::CreateFromPoints`, `Ray::Intersects` — verify against analytic answers
      dep: HOUSE-00062 · sys: visibility · plat: LNX · pri: MUST
      note: (2026-09-06) PASS, **24/24**, every expectation computed by hand. The frustum is
            **orthographic and axis-aligned** on purpose: its corners are then exactly the corners
            of a box with the projection's own extents, with no perspective divide to force a
            tolerance — measured `x[-2,2] y[-1,1] z[-11,-1]`, exact. Ray distances 4.0, 3.0 and 3.0
            against box, sphere and plane; a ray starting **inside** returns 0.0; a ray pointing
            **away** misses although its line would hit — the case a naive line-intersection
            implementation gets wrong. `CreateFromPoints` on an unordered list with duplicates gives
            `min(-2,-1,-4) max(3,5,9)`. Frustum vs box gives `Contains`/`Intersects`/`Disjoint`/
            `Disjoint` for inside/straddling/behind/beyond-far, and a sphere **tangent** to a side
            plane and a box **face-touching** another both count as intersecting — the two cases an
            epsilon error flips. Phase 9 can be built on these.
- [x] HOUSE-00105 — Probe: build and run under the `HEADLESS` renderer; confirm `Update` runs with no window and no GPU
      dep: HOUSE-00062 · sys: app · plat: CI · pri: MUST
      accept: a 600-frame headless run with no display server (`unset DISPLAY`)
      note: (2026-09-06) PASS, 5/5, exit 0. Built in `build-consumer/` with
            `-DCNA_GRAPHICS_RENDERER=HEADLESS` and run under
            `env -u DISPLAY -u WAYLAND_DISPLAY`. The probe **asserts from inside the process** that
            neither display variable is set, because a run that silently reached a live X server
            would prove nothing about CI and is the easiest possible thing to get wrong. 600
            `Update` calls, 599 `Draw` calls — so the whole frame loop runs, not just `Update` —
            and `GameTime` advances. Phase 44's automated tests and phase 2's CI have a
            foundation.
- [x] HOUSE-00106 — Probe: measure `EffectPass::Apply()` cost and the cost of 1 000 small `DrawIndexedPrimitives` calls, to calibrate the draw budget
      dep: HOUSE-00062 · sys: rendering · plat: LNX · pri: MUST
      note: (2026-09-06) `EffectPass::Apply()` **0.184 µs each** (0.184 ms for 1 000).
            `Apply` + `DrawIndexedPrimitives` **8.15 µs of CPU per draw** — 8.15 ms of CPU for
            1 000, 13.44 ms to GPU completion.
      finding: **the draw call is the budget and `Apply()` is not** — the draw is 44× the cost of
            the state application before it. So phase 9's job is to reduce **draw calls**, not state
            changes: batching by material is worth far less than not submitting the room at all.
            A practical ceiling of **300–400 draws per frame** leaves room for game logic. This
            replaces any assumed budget in `cna-house.md` §41 and sizes `HOUSE-00745`.
- [x] HOUSE-00107 — Probe: `Texture2D` upload bandwidth for a 4 MB residency budget; confirm the per-frame promotion budget is realistic
      dep: HOUSE-00065 · sys: content · plat: LNX · pri: SHOULD
      note: (2026-09-06) **It is NOT realistic as one atomic step.** 4 MiB (1024²) `SetData` costs
            **9.30 ms** to completion — more than half a 60 Hz frame — at **430 MiB/s**. A 1 MiB
            (512²) upload costs **2.47 ms** at **405 MiB/s**, so the cost is **linear and
            bandwidth-bound**, not a fixed per-call overhead.
      finding: the fix follows directly from the linearity: promote **≈1 MiB per frame** and spread
            a large texture over four frames. The streaming design of phase 42 must split
            promotions; it may not treat 4 MiB as one step. Sizes `HOUSE-00772`.
- [x] HOUSE-00108 — Probe: ~~EasyGL `DebugSimulateContextLoss` on desktop~~ → **the XNA-legal device-reset path**; confirm resources can be rebuilt from CPU state
      dep: HOUSE-00084 · sys: rendering · plat: LNX · pri: SHOULD
      accept: it is a supported path we can test against on Linux, de-risking the Web port
      correction: (2026-09-06) **the named call cannot be used at all.**
            `DebugSimulateContextLoss` is declared in
            `CNA/Internal/Renderers/Common/IGraphicsRenderer.hpp` — a `CNA/` include of a
            `CNA::Internal::` type — so ADR-0001 forbids it **twice over**, in a probe as much as in
            the runtime. Reaching for it "just to measure once" would be exactly the erosion the
            rule exists to prevent. The task is repurposed to the path `cna-house` would actually
            use, and the acceptance criterion is met by it.
      note: (2026-09-06) PASS, 5/5, via `GraphicsDevice::Reset()` and the
            `DeviceLost`/`DeviceResetting`/`DeviceReset` events. `Reset()` is callable; a texture,
            a vertex buffer and an index buffer were all rebuilt from the same CPU-side state
            afterwards; and the rebuilt scene sampled `(128,128,128)` — **identical** to before.
            So the property the Web port needs (resources reconstructible from CPU state, with an
            event to say when) holds on Linux and is testable there.
      finding: **`DeviceLost` does NOT fire** — only `DeviceResetting` and `DeviceReset` do. A game
            that hung its rebuild on `DeviceLost` alone would never rebuild. Sizes phase 47.
- [x] HOUSE-00109 — Probe: anisotropic filtering availability and its visual effect at grazing angles on the floor
      dep: HOUSE-00065 · sys: rendering · plat: LNX · pri: OPT
      note: (2026-09-06) **Available AND effective.** The same grazing floor rendered under
            `LinearClamp` and `AnisotropicClamp` differs in **3 056 of 65 536** pixels; mean
            per-row far-field contrast rises from **6.04 to 8.90 (1.47×)**, and one row goes from
            0.00 (fully blurred out) to 47.68 (fully resolved). "Accepted as a state" was
            deliberately not treated as the answer — a driver can accept `AnisotropicClamp` and
            silently do trilinear.
      finding: two fixture corrections were needed before the measurement meant anything, and both
            are the standard ways to fake this result. (1) A **4-texel checker repeated 60×** is
            minified so hard that *both* modes collapse to flat grey, and the probe reported a
            contrast of 0.00 for each — measuring the fixture, not the driver; a realistic 32-texel
            tile at 16 repeats is what discriminates. (2) Counting **rows improved versus worsened**
            oscillates with wherever a tile boundary falls, and swung 21 against 28 even while the
            image as a whole gained 47 % contrast; the aggregate over every lit row is the statistic
            that is not an artefact of tile phase.
      verify: the verdict is recorded in `docs/cna-capability-report.md` as a property of the build/platform profile, and is what HOUSE-00916 keys off — it is measured once here, never queried at runtime
- [x] HOUSE-00110 — Probe: `SurfaceFormat` support survey — which formats can be created as textures and as render targets on this driver
      dep: HOUSE-00083 · sys: rendering · plat: LNX · pri: MUST
      note: (2026-09-06) Complete, and every render-target candidate was **used** — bound, cleared
            and drawn into — not merely created, because a driver can accept a format at creation
            and fail on first use.
            **As `Texture2D`:** `Color`, `Bgr565`, `Bgra5551`, `Bgra4444`, `Dxt1`, `Dxt3`, `Dxt5`,
            `NormalizedByte2`, `NormalizedByte4`.
            **As `RenderTarget2D`:** `Color`, `Single`, `Vector2`, `Vector4`, `HalfSingle`,
            `HalfVector2`, `HalfVector4`.
      finding: **only `Color` is in both lists.** `SurfaceFormat::Single` is a perfectly good render
            target — `HOUSE-00083` rendered, read back and sampled one — but a `Texture2D` of that
            format **cannot be created by the application at all**. Any float data `cna-house` wants
            on the GPU must arrive as the output of a render pass, never as an uploaded texture.
      finding: surveyed at **both** graphics profiles, because `GraphicsDeviceManager` defaults to
            `GraphicsProfile::Reach` and XNA 4.0's Reach forbids float formats outright — a survey
            at the default alone would blame the driver for an XNA rule. The **lists are identical**
            at `Reach` and `HiDef` here; only the refusal message changes. So the limit is EasyGL's,
            and moving `cna-house` to `HiDef` would buy nothing on this platform. This is what
            `HOUSE-00916` keys off, measured once, never queried at runtime.
- [x] HOUSE-00111 — Probe: DXT compressed texture support and whether the pipeline's DXT output stays compressed on EasyGL
      dep: HOUSE-00110 · sys: content · plat: LNX · pri: SHOULD
      accept: a verdict that sizes the texture-memory budget
      note: (2026-09-06) **It works — but only through `.xnb`.** Same 64×64 source, same
            `textureFormat` parameter, two containers:
            `.cnb` Dxt1 → 16 752 B, runtime format `Color`; `.cnb` Dxt5 → 16 752 B, `Color`;
            `.xnb` Dxt1 → **2 235 B**, runtime format **`Dxt1`**; `.xnb` Dxt5 → **4 283 B**,
            **`Dxt5`**; uncompressed `.xnb` → 16 571 B, `Color`. So the blocks survive to the GPU:
            **8× for `Dxt1`, 4× for `Dxt5`.**
      finding: **CNB texture schema 1 is frozen to Rgba8.** The pipeline warns rather than fails —
            *"textureFormat Dxt1 has no representation in CNB texture schema 1, which stores Rgba8
            only; this .cnb keeps the uncompressed pixels. Build with --format xnb to get the
            compressed texture."* — so the asset is silently 8× larger and the warning is easy to
            lose in a large build. `cna-house` needs a build gate of its own for it.
      finding: consequence for the `linux` content profile (`cna-house.md` §27.2): **texture assets
            that want compression must be built as `.xnb`, not `.cnb`.** Combined with
            `HOUSE-00064`'s finding that `.xnb` wins the resolution order, a deliberately mixed tree
            (models and audio `.cnb`, textures `.xnb`) is coherent; an accidental one is a trap.
            Sizes `HOUSE-00126` and the phase-3 pipeline.
      verify: the verdict is written into the `linux` content profile (`cna-house.md` §27.2), which decides offline which representation is packaged; the uncompressed variant remains the packaged fallback, and no runtime renderer query is added
- [x] HOUSE-00112 — Write `docs/cna-capability-report.md` in full from the probe results, with a verdict per claim and a link to each probe's recorded output
      dep: HOUSE-00062…HOUSE-00111 · sys: — · plat: LNX · pri: MUST
      note: (2026-09-06, phase 2) **The file shipped with its second half duplicated** — 1 373 lines
            repeating `## Findings` onwards from an earlier tranche, superseded line for line by the
            copy above it. Found while reading the measured `BasicEffect`/`DualTextureEffect` facts
            for HOUSE-00162 and removed: 3 084 lines → 1 708. Nothing was lost; the 29 lines unique
            to the stale copy were older statements of the probe-hygiene table and the ccache
            paragraph that the kept copy states more completely, and every task `HOUSE-00062`
            through `HOUSE-00120` still has its section. The duplication came from appending to the
            report rather than editing it — worth naming, because a document with two `## Findings`
            headings is one a reader stops trusting.
      note: (2026-09-06) Written **as the probes ran**, not reconstructed at the end — every finding
            was recorded in the same commit as the probe that produced it, which is why the measured
            numbers in it are the ones the probes actually printed. Final tally across the claim
            tables: **44 `PASS`, 5 `DIFFERENT`, 6 `NOT PROBED`, 0 `FAIL`, 0 `PENDING`.** Every
            `NOT PROBED` states its reason. The five `DIFFERENT` rows are the ones that changed the
            architecture: the `StorageDevice` root, `Text.Json`'s component wiring,
            `Mouse::GetState`'s snapshot semantics, and the two `.fx` launcher/route corrections.
- [x] HOUSE-00113 — Update `cna-house.md` §5, §6 and §76 with anything the probes changed; in particular settle BL-09/Q-01
      dep: HOUSE-00112 · sys: — · plat: ALL · pri: MUST
      note: (2026-09-06) **§6 — seven blocker rows rewritten from measurement.** `BL-02` (stencil)
            and `BL-03` (MRT) had **false premises** and are downgraded to notes with the evidence;
            `BL-09` is **resolved positively** and Tier E gets a real float shadow map; `BL-04` is
            downgraded M → L now that the `.fx` route is verified end to end against a genuine
            `fxc`; `BL-05`, `BL-07`, `BL-11` and `BL-12` are confirmed with their measured detail.
            **§76 — `Q-01` closed** (the feature-matrix row was the stale one), **`R-16` resolved**
            (the answer was the fallback: blend indices are skin-local, so the sidecar must carry
            the joint-name list), and **`R-06` largely retired** (Wine `.fx` compilation works; the
            one fragility was ours and is fixed).
            **§5 — the storage root corrected** (`<app>` is the literal `game`; identify by
            container name instead), **`Text.Json`'s component wiring recorded**, and a new preamble
            to §5.3 stating the five CNA-vs-XNA shape facts that are invisible from CNA's own
            documentation.
            **§21.3 rewritten** with the measured multi-skin behaviour, and **§27.2 given a measured
            `linux` content profile** — DXT only through `.xnb`, anisotropy on, ~1 MiB per-frame
            promotions, and the two format lists.
- [x] HOUSE-00114 — Delete every probe binary and object from `build-probe/`; keep only the probe sources and a one-paragraph README per probe
      dep: HOUSE-00112 · sys: — · plat: LNX · pri: MUST
      accept: `du -sh build-probe/` is under 2 MB
      correction: (2026-09-06) **the sources are kept in `tests/probes/phase1/`, not in
            `build-probe/`.** Kept in `build-probe/` they would not survive a clone — `.gitignore`'s
            `/build*/` excludes the whole directory — which is not "kept" in any useful sense.
            `tests/` was chosen over `tools/` deliberately: it is the one tree where **both** gates
            already cover the code (`check_layout.py` permits C++ there and `check_xna_only.py`'s
            `TEST_ROOTS` scans it), so the probes are held to the same XNA-only rule as the runtime
            — and they pass it. `build-probe/CMakeLists.txt` globs them from there and adds both
            roots to the include path, since the generated fixtures stay in the binary tree.
      note: (2026-09-06) 24 probe sources + `p1-common.hpp` + the two generators (388 KB) and a
            README describing what every probe measures and how to build one. The `.fx` fixture went
            to `assets-src/Effects/P1Probe.fx`, the only place `check_xna_only.py` permits an effect
            source — which also keeps the Tier E pipeline exercised rather than a one-off. All 21
            runnable probes were rebuilt from the new location, `clang-format`ed and **re-run: 21/21
            still PASS**. `build-probe/` and `build-consumer/` are then deleted whole, so the
            acceptance figure is met by removal rather than by trimming.
      done: (2026-09-06) `build-probe/` (290 MB) and `build-consumer/` (116 MB) deleted — 406 MB —
            after checking `ps` and every `/proc/*/cwd` for a live process holding either, per
            openeggbert build rule 7. Nothing but the tracked sources, the generators, the README
            and `assets-src/Effects/P1Probe.fx` survives, and the whole set rebuilds in 15 s from a
            warm cache (`HOUSE-00116`).
- [x] HOUSE-00115 — Record in `docs/cna-capability-report.md` a "if you change renderer, re-run these" list
      dep: HOUSE-00112 · sys: — · plat: ALL · pri: MUST
      note: (2026-09-06) Ten rows to re-run, ordered by how badly a wrong assumption would hurt,
            each naming its probe — led by `OcclusionQuery`, which is **already known to differ**
            between `OPENGLES3` and `OPENGL33` on the same driver. The list also names what does
            **not** need re-running, and why: content-pipeline, math and host properties are
            renderer-independent, and `HOUSE-00107`'s upload bandwidth was measured within noise on
            both renderers. It closes with the one measurement still owed on any renderer —
            `HOUSE-00100`'s mouse drift.
- [x] HOUSE-00116 — Probe: measure a full clean CNA build time and an incremental one with ccache warm, to set expectations for later sessions
      dep: HOUSE-00062 · sys: — · plat: LNX · pri: SHOULD
      note: (2026-09-06) Measured on the probe tree (CNA + sharp-runtime + one probe, 724 targets,
            Release, `-j6`, 16-thread host shared with other agents):
            **cold — ccache disabled: 330.3 s wall, 1 802.7 s user CPU, peak RSS 892 MB.**
            **the same build from an empty directory with the shared ccache warm: 14.9 s wall,
            20.1 s user CPU, peak RSS 342 MB — a 22× wall-clock and 90× CPU saving.** That single
            pair is the whole justification for openeggbert build rule 1, measured rather than
            argued.
            **CMake configure alone: 26–49 s** depending on cache warmth of the configure-time audits.
            **Edit one probe source, rebuild and relink: 0.94 s** (0.77 s when the content hashes back
            to a cache hit).
            **Rebuild all ten stock-effect and model probes: 11.6 s.**
            **Switching renderer in `build-consumer/` (full CNA relink, warm ccache): 75 s (HEADLESS),
            138 s (OPENGL33).**
            Session ccache movement: +1 812 hits, +364 misses; the shared cache stands at 34.5 GB of
            100 GB with a 29.9 % lifetime hit rate.
      finding: the number that actually sets expectations is **~1 s to iterate on a probe**, not the
            cold figure — the openeggbert build rules exist precisely so the cold path is never
            taken. The cold measurement was run once, deliberately, in the throwaway
            `build-consumer/` tree that `HOUSE-00114` deletes anyway, so it cost nothing that was
            not already going to be discarded.
- [x] HOUSE-00117 — Probe: confirm the `HEADLESS` and `OPENGLES3` builds can share one `build/` tree via presets without full reconfigures thrashing
      dep: HOUSE-00105 · sys: — · plat: LNX · pri: SHOULD
      accept: separate build directories from the closed list are used per renderer; document which
      note: (2026-09-06) **They cannot share one tree, and they do not need to.** `CNA_GRAPHICS_RENDERER`
            selects which renderer *family target* is composed, so changing it changes the target set
            and forces a full reconfigure and relink — sharing one tree would mean thrashing exactly
            the way the task feared. **The answer is one binary directory per renderer, both from the
            closed list, both configured from the SAME source directory:**
            `cmake -S build-probe -B build-probe` → `OPENGLES3` (the shipping configuration), and
            `cmake -S build-probe -B build-consumer -DCNA_GRAPHICS_RENDERER=…` → whichever variant is
            being measured. This works because `build-probe/CMakeLists.txt` leaves
            `CNA_GRAPHICS_RENDERER` and `CNA_ENABLE_VIDEO` as ordinary cache entries rather than
            `FORCE`-ing them, while `CNA_CNAEXT=OFF` stays `FORCE`d and can never be varied.
      finding: `build-consumer/` served three variants in this session in turn — `HEADLESS`
            (`HOUSE-00105`), `CNA_ENABLE_VIDEO=OFF` (`HOUSE-00099`) and `OPENGL33` (`HOUSE-00091`) —
            each a deliberate reconfigure of the same directory, which is the "configuration
            genuinely changed" exception the openeggbert rules allow. No per-ticket directory was
            ever created. Phase 2's `CMakePresets.json` (`HOUSE-00035`) should encode exactly this
            pairing.
- [x] HOUSE-00118 — Probe: run the relevant subset of CNA's own ctests (graphics stock effects, content, audio) once, to confirm the checkout is healthy before depending on it
      dep: HOUSE-00062 · sys: — · plat: LNX · pri: SHOULD
      accept: pass/fail recorded; a failure here is an upstream issue, not ours
      note: (2026-09-06) **The checkout is healthy: 5 677 tests, 3 failures, all upstream.** The
            per-example `cna_test_easygl_*` render binaries are not built in this checkout, so the
            five module suites that are built were run instead — which is the same coverage in
            aggregate and is what the criterion asks for.
            `CnaMathTests` 846/846 · `CnaContentTests` 1 798 ran, 1 792 passed, 4 skipped, **2
            failed** · `CnaGraphicsTests` 2 364 ran, 2 311 passed, 53 skipped, **0 failed** ·
            `CnaRuntimeTests` 169 ran, 166 passed, 2 skipped, **1 failed** ·
            `CnaInputModuleTests` 500/500.
            The three failures: `XnbContentPipelineTest.SpriteFontRuntimeXnbAndTranscodedCnbHave
            EquivalentSemantics`, `ContentManagerVideoXnbTest.TheObjectReferencedFormLoadsToTheSame
            ValuesAsTheInlineOne`, `GameWindowPlatformTest.DelegatesStateAndGeometryToTheSelected
            PlatformWindow`. None touches a route `cna-house` uses: the first two are about
            `.xnb` ↔ `.cnb` *transcoding equivalence* and the `.xnb` object-reference form, and this
            project uses `.cnb` for fonts and video (`.xnb` only for compressed textures and
            compiled effects, per `HOUSE-00111` and `HOUSE-00087`). Recorded as upstream, not
            reported and not patched, per `CLAUDE.md` §3.
      finding: **the harness is a trap worth writing down.** A first run from `cnanext/build/`
            produced **64 failures** in `CnaContentTests`. Every one was a fixture path that did not
            resolve — the tests use paths relative to the **repository root**, which is where `ctest`
            would have set the working directory and where running the binary directly does not.
            Re-run from the root, the same 29 tests in those suites passed. A test harness that
            reports 64 red for one wrong `cd` is a harness that will be believed, so
            `tests/CMakeLists.txt` sets `WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}` on every discovered
            `cna-house` test and says why.
- [x] HOUSE-00119 — Write the "CNA facts that must never be assumed again" section of the capability report: the five things that most surprised us
      dep: HOUSE-00112 · sys: — · plat: ALL · pri: MUST
      note: (2026-09-06) The five, each of which was believed otherwise until measured:
            (1) blend indices are **skin-local**, so the compiled `Model` does not contain the
            binding and the sidecar's joint-name list *is* it;
            (2) **`Apply3D` writes nothing a game can read**, and its law is inverse *distance*
            beyond `DistanceScale` rather than the inverse-square a designer would assume;
            (3) **half the blockers were wrong, in both directions** — `BL-02`, `BL-03` and `BL-06`
            false, `BL-09` pessimistic, `BL-07` right and explained by `HOUSE-00091`;
            (4) **the XNA surface is not uniformly XNA-shaped in C++** — `CNAEXT` iterators, three
            different `Load<T>` shapes, a non-trivially-copyable `Color`, pre-sized bone
            destinations;
            (5) **the measurement is only as good as the fixture, and a bad fixture reports a clean
            pass** — four probes here produced confident wrong answers first, and the habit that
            caught every one was insisting on an *analytic* expectation.
- [x] HOUSE-00120 — Phase-1 review: confirm no design in `cna-house.md` now rests on an unproven CNA claim
      dep: HOUSE-00113 · sys: — · plat: ALL · pri: MUST
      note: (2026-09-06) **Review performed, not asserted.** Every `NOT PROBED` row was traced back
            into `cna-house.md` to see whether a design actually depends on it. Final tally: **44
            `PASS`, 5 `DIFFERENT`, 6 `NOT PROBED` (each with a stated reason), 0 `FAIL`, 0
            `PENDING`.** Of the six unprobed rows, four are referenced **only** by §5's availability
            table and by no design at all — `RenderTargetCube`, `Curve`, and `Song`/`MediaPlayer`,
            the last two moot because D-21 is "no music" — and the other two are deliberately owned
            by the phase that consumes them (`cna_add_content` → phase 2; WebGL context loss →
            phases 47–48, already gated by `BL-14`).
      finding: **one residual, named: `HOUSE-00100`.** Phase 8's first-person camera rests on the
            mouse recentring loop and its drift is unmeasured, because this environment could not
            put a pointer over the probe window. The design is **not blind** while it stays open:
            the probe did establish that `Game::IsActive` is not a proxy for "the mouse is usable"
            and that `GetState` is an event-driven snapshot rather than a live cursor query, and
            both constrain phase 8 more than a drift figure would have. The task stays unchecked,
            `HOUSE-00115` lists it as owed, and phase 8's first task is where it gets measured —
            under the ordinary condition for that work.
      verdict: **no material design in `cna-house.md` rests on an unverified CNA claim, with that
            one named exception.** The architecture now stands on 44 measured capabilities, five
            corrected ones, three disproved blockers and one resolved open question, rather than on
            a feature matrix.

---

## Phase 2 — Build skeleton and CI

**Goal.** The real application skeleton: CMake, the `Game` subclass, the service container, the
system update order, the settings file, the logging, and a CI that runs lints and headless tests.

**Exit.** `cna-house` opens a window, clears to a known colour, draws a version string with
`SpriteFont`, exits cleanly; CI runs lint + unit + headless integration on every push.

- [x] HOUSE-00121 — Root `CMakeLists.txt`: project, C++23, the CNA/sharp-runtime cache variables of `cna-house.md` §8.1, `add_subdirectory(../cnanext CNA_BUILD)`
      dep: HOUSE-00120 · sys: app · plat: ALL · pri: MUST
      accept: configures and builds against the sibling checkouts with no vendoring
      note: (2026-09-06) `CMakeLists.txt` configures and builds against `../cnanext` and
            `../sharp-runtimenext` by `add_subdirectory`, with no vendoring, no submodule and no
            fetch. A missing sibling is a `FATAL_ERROR` that says they are checkouts rather than
            submodules, because `git submodule update --init` is the first thing anyone tries.
      finding: **the siblings' include directories are marked `SYSTEM`.** `cna-house` compiles at
            `-Wall -Wextra -Wpedantic -Werror`; CNA and sharp-runtime do not, and are entitled not
            to — `-Wpedantic` fires on sharp-runtime's `unsigned __int128` and `-Wshadow` on
            several CNA headers, and neither is a defect. Promoting a sibling's warnings to *our*
            errors would make this project's policy break someone else's code. The whole
            subdirectory tree is walked rather than a few targets named: the include that actually
            broke the build first was `Decimal.hpp`, reached transitively through `Game.hpp`, which
            naming targets by hand would have missed.
- [x] HOUSE-00122 — CMake: derive `CNAHOUSE_TIER_E` from the CNA configuration this build has just set — the selected `CNA_GRAPHICS_RENDERER` and that renderer's compiled-effect option (`CNA_EASYGL_COMPILED_EFFECTS`, or its `SDL_GPU`/`VULKAN` sibling) — and add `CNAHOUSE_DEBUG_TOOLS`. This is the **only** place the Tier-E decision is made; no runtime code queries the device.
      dep: HOUSE-00121 · sys: app · plat: ALL · pri: MUST
      files: CMakeLists.txt, cmake/TierSelection.cmake
      accept: (1) with the renderer's compiled-effect option off, `CNAHOUSE_TIER_E` is off and the Tier-E sources and `.fx` tree are not compiled; (2) a user can force it off, never on; (3) the resolved value is printed at configure time and baked into the version string
      note: (2026-09-06) `cmake/TierSelection.cmake` derives `CNAHOUSE_TIER_E` from
            `CNAHOUSE_RENDERER` and that renderer family's compiled-effect option, and adds
            `CNAHOUSE_DEBUG_TOOLS`. All three acceptance points hold: (1) with
            `CNA_EASYGL_COMPILED_EFFECTS` off, or under `HEADLESS`, Tier E resolves off and the
            `.fx` tree is not compiled; (2) the rule is **asymmetric** — a user may force it off,
            never on, because a binary built without it has no compiled effects in its content and
            honouring `--tier=e` would fail at the first draw; (3) the resolved value is printed at
            configure time (`-- cna-house: renderer OPENGLES3, Tier E ON, …`) and baked in as
            `CNAHOUSE_TIER_E`, which `--renderer-info` reports.
- [x] HOUSE-00123 — CMake: the `cnahouse_core` library and the `cna-house` executable, with the `src/` subdirectory structure
      dep: HOUSE-00121 · sys: app · plat: ALL · pri: MUST
      note: (2026-09-06) `cnahouse_core` holds everything except `main`, so the game and every
            test target link the **same** library rather than a recompiled variant of it. `src/`
            groups its sources by subsystem in the order `cna-house.md` §7.5 updates them.
- [x] HOUSE-00124 — CMake: warnings-as-errors, `-Wall -Wextra -Wpedantic -Werror`, and the sanitizer presets
      dep: HOUSE-00123 · sys: app · plat: ALL · pri: MUST
      note: (2026-09-06) `-Wall -Wextra -Wpedantic -Werror` plus `-Wshadow`,
            `-Wold-style-cast`, `-Wconversion`, `-Wsign-conversion`, `-Wnon-virtual-dtor`,
            `-Wcast-align`, `-Woverloaded-virtual`, `-Wdouble-promotion` and `-Wnull-dereference`,
            applied through an interface library so they reach **this project's targets only**.
            The sanitizer presets are `linux-asan` and `linux-ubsan` in `CMakePresets.json`.
      finding: the strict set is not decorative — it caught two real defects in the first hour.
            `-Wsign-conversion` found `Rng::NextInt` computing `max - min` in `int32_t`, which
            overflows for a range spanning the type; `-Wpedantic` found the `unsigned __int128` in
            its wide multiply, which is a compiler extension the Web target does not have. Both are
            now written in standard C++.
- [x] HOUSE-00125 — CMake: GoogleTest integration and the four test targets (`unit`, `integration`, `render`, `perf`)
      dep: HOUSE-00123 · sys: app · plat: CI · pri: MUST
      note: (2026-09-06) GoogleTest comes from the sibling CNA checkout's vendored copy rather
            than being fetched — no network in the build, one copy on disk. The four targets are
            created from `tests/{unit,integration,render,perf}/` and labelled, so
            `ctest -L unit` is what CI runs.
      finding: **every test's working directory is the repository root, and that is deliberate.**
            `HOUSE-00118` watched CNA's own content suite report **64 failures** purely because the
            binary was run from `build/` and its fixture paths are relative to the repo root. A
            harness that goes 64 red for one wrong `cd` is a harness that will be believed, so the
            `WORKING_DIRECTORY` is pinned and the reason is written beside it. Content, which lives
            in the *build* tree, is passed separately as `CNAHOUSE_TEST_CONTENT_ROOT`.
- [x] HOUSE-00126 — CMake: `cna_add_content` wiring for the two content trees (cnb and effects), gated on `CNAHOUSE_TIER_E` for the second
      dep: HOUSE-00123 · sys: content · plat: ALL · pri: MUST
      note: (2026-09-06) Two trees in two containers, for measured reasons: `content/` is `.cnb`
            (models, fonts, audio, video, world data) and `content-fx/` is `.xnb` (compiled
            effects, and later the DXT textures — `HOUSE-00111` measured that CNB texture schema 1
            is frozen to `Rgba8`). The effect tree is gated on `CNAHOUSE_TIER_E` **and** on an
            `fxc` actually being found, because those are different questions: the build may want
            Tier E and the machine may not be able to produce it. Verified both ways — with no
            `CNAHOUSE_FXC` the configure prints why and Tier S is unaffected; with it set,
            `assets-src/Effects/P1Probe.fx` compiles to a 3 424-byte `.xnb` through
            `tools/effects/fxc-wine.sh`.
      finding: **`cna_add_content` has no format option** — it always produces `.cnb` — so the two
            trees cannot share a source root: a `.fx` under a `.cnb` build is attempted and fails.
            The `.cnb` build therefore walks the asset subdirectories **except `Effects/`**, which
            Tier E owns. That is the same split the containers already force, made visible.
            Combined with `HOUSE-00064` (`.xnb` wins the resolution order), separate output roots
            are what keep a mixed tree deliberate rather than accidental.
- [x] HOUSE-00127 — Implement `CnaHouseGame : Game` with `Initialize`, `LoadContent`, `UnloadContent`, `Update`, `Draw`, `Exit`
      dep: HOUSE-00123 · sys: app · plat: ALL · pri: MUST
      files: src/app/CnaHouseGame.cpp|hpp
      accept: opens a 1600×900 window titled "CNA House", clears to a known colour, exits on `Esc`
      note: (2026-09-06) `CnaHouseGame : Game` with all six overrides. Opens a 1600×900 window,
            clears to a **known** colour — `(18, 20, 24)`, deliberately not `CornflowerBlue`, since
            every XNA sample in existence is cornflower blue and a screenshot of one proves nothing
            about which program produced it — and exits on `Esc`. Verified by a headless
            integration test that runs 120 real frames and exits 0, and by running the window.
      finding: **`SpriteFont` has no default constructor**, so the HUD font is a
            `std::optional<SpriteFont>` rather than a member plus a `hasFont` flag. That is the
            honest shape anyway: the font is genuinely absent until it loads, `has_value()` *is*
            the "did it load" question, and there is no second flag to fall out of step with it.
            A build whose content tree has not been generated logs one warning and runs.
- [x] HOUSE-00128 — Implement `Services`: a typed container constructing every system in dependency order and destroying in reverse
      dep: HOUSE-00127 · sys: app · plat: ALL · pri: MUST
      accept: (1) construction order is explicit and asserted; (2) a system cannot resolve one constructed after it
      verify: unit ServicesTests.*
      note: (2026-09-06) `Services` with explicit sequenced construction, reverse destruction, and
            `ResolveFrom` — verified by `ServicesTests.*`. Both acceptance points hold: the
            construction order is recorded and asserted, and a service **cannot** resolve one
            registered after it (`InvalidData`, with both positions named and "move it earlier in
            Bootstrap" as the fix).
      finding: ADR-0006 chose composition over an ECS, and a plain struct of members is the obvious
            composition — but it gives no place to state the *order*, and order is the thing that
            goes wrong. Sixty members constructed in declaration order, one quietly reading another
            not yet built, is a startup null-dereference that reorders itself every time someone
            adds a field. Registering twice is refused rather than replacing, because replacing
            leaks the first instance and dangles every pointer to it; a *borrowed* registration
            exists for the objects XNA owns (`GraphicsDevice`, `ContentManager`) and is never
            destroyed by the container.
- [x] HOUSE-00129 — Define `ISystem` (`Update(const FrameContext&)`) and the fixed system order of `cna-house.md` §7.5
      dep: HOUSE-00128 · sys: app · plat: ALL · pri: MUST
      note: (2026-09-06) `ISystem` and `UpdateStage`, the twelve stages of §7.5 declared as an enum
            rather than as the sequence of calls in some `Update` function — so the order can be
            asserted, timed per stage, printed in the debug overlay, and so a new system has to be
            *placed* rather than appended.
      finding: the order is load-bearing, not conventional. Interaction opens a door, the door
            animates its aperture, the aperture changes which portals are open, visibility walks
            those portals. Reorder two and the frame is one frame stale in a way that presents as a
            door you can see through before it has opened.
- [x] HOUSE-00130 — Implement the event queue: typed events, drained once per frame in a fixed order, with a per-frame cap and an overflow diagnostic
      dep: HOUSE-00129 · sys: app · plat: ALL · pri: MUST
      verify: unit EventQueueTests.*
      note: (2026-09-06) `EventQueue`: typed, drained once per frame in publish order, with a
            4 096-event per-frame cap and an overflow diagnostic naming the event type. Verified by
            `EventQueueTests.*`, including a deliberate publish loop.
      finding: an event published *by a handler* is delivered in the **same** drain, so a switch
            that opens a door that changes a portal resolves this frame rather than arriving one
            frame late — which is exactly why the cap has to exist. On overflow the remainder is
            **dropped, not carried**: carrying it would let a publish loop survive the cap and
            overflow again forever, turning a loud failure into a quiet one. Without the cap a
            publish loop is a hang, and a hang is the least diagnosable failure a game can have.
- [x] HOUSE-00131 — Implement `Settings`: load/save `settings.json`, defaults, versioning, migration, and typed accessors
      dep: HOUSE-00028, HOUSE-00127 · sys: app · plat: ALL · pri: MUST
      verify: unit SettingsTests.* incl. a v1→v2 migration fixture
      note: (2026-09-06) `Settings` with load, save, defaults, versioning and a v1 → v2 migration,
            verified by `SettingsTests.*` including the fixture the acceptance names.
      finding: the migration story has **two** directions and only one of them is a version number.
            A v1 file must migrate up; a file written by a *newer* build must also still load, or a
            developer who switches branches loses their settings. Unknown fields are therefore
            ignored deliberately, while a wrong *type* on a known field is still an error — an
            optional field may be absent, not a string standing in for a number.
- [x] HOUSE-00132 — Implement the command-line parser: `--quality`, `--tier`, `--headless`, `--scene`, `--seed`, `--time`, `--weather`, `--no-audio`, `--screenshot`, and the optional diagnostic `--renderer-info`. **There is no `--renderer` option**: `CNA_GRAPHICS_RENDERER` is fixed at configure time (`cna-house.md` §7.3, §8.1) and standard XNA 4.0 cannot change it afterwards, so a separate build is produced per renderer. `--renderer-info` only prints what the application already knows about itself — configured renderer name, the `CNAHOUSE_TIER_E` build fact, the resolved `RenderTier` — and queries nothing.
      dep: HOUSE-00127 · sys: app · plat: ALL · pri: MUST
      accept: no option can change the renderer of a built binary; `--renderer-info` calls no CNA-specific API and `check_xna_only.py` passes on the parser sources
      note: (2026-09-06) Every documented option parses, and an unknown one is **refused** rather
            than ignored — a mistyped `--quailty=low` that ran anyway at the default would produce
            a bug report about a setting that was never applied. Both acceptance points hold: no
            option changes the renderer, and `--renderer-info` prints only compile-time constants
            this binary carries, so `check_xna_only.py` passes on the parser.
      finding: `--renderer` is handled **by name**, not left to the unknown-option path, so a user
            reaching for a reasonable-sounding option is told *why* it cannot exist and what to use
            instead. Verified end to end: `./build/cna-house --renderer=opengl33` refuses with that
            explanation and exits 2.
- [x] HOUSE-00133 — CI job: lint (`check_xna_only`, `check_layout`, clang-format) on every push
      dep: HOUSE-00020, HOUSE-00022 · sys: ci · plat: CI · pri: MUST
      note: (2026-09-06) `.github/workflows/ci.yml`, job `lint`. Runs `tools/ci/run_checks.sh` and
            then `check_xna_only.py --selftest`, because **a gate that has never been shown to fire
            is a gate nobody should trust** — the self-test plants a violation of each of the
            fourteen rejected classes and requires each to be found. Verified locally: 14/14
            detected, clean tree accepted.
- [x] HOUSE-00134 — CI job: build `linux-debug` and `linux-release`, run `unit`
      dep: HOUSE-00125 · sys: ci · plat: CI · pri: MUST
      note: (2026-09-06) Job `build-and-unit`, a matrix over `linux-debug` and `linux-release`
            driven by `CMakePresets.json`, running `ctest --preset unit`. All three repositories are
            checked out into the sibling shape `cna-house.md` §8.1 requires; ccache is cached
            between runs with `CCACHE_BASEDIR` set, which is what makes the cache hit at all.
- [x] HOUSE-00135 — CI job: build `headless`, run `integration`
      dep: HOUSE-00125, HOUSE-00105 · sys: ci · plat: CI · pri: MUST
      note: (2026-09-06) Job `headless-integration`, building the `headless` preset and running
            `ctest --preset integration` under `env -u DISPLAY -u WAYLAND_DISPLAY`. The unset is
            deliberate and is the same condition `HOUSE-00105` measured 600 frames under: a run
            that silently found an X server would prove nothing about CI.
- [x] HOUSE-00136 — CI job: the symbol check that no `CNA::Graphics::` symbol is linked into the binary (from HOUSE-00063)
      dep: HOUSE-00063, HOUSE-00134 · sys: ci · plat: CI · pri: MUST
      note: (2026-09-06) Job `symbol-check`, and it **passes on the real binary today**:
            `nm -C build/cna-house | grep -c 'CNA::Graphics::'` is **0** and `AvatarRenderer` is
            **0**.
      scope: deliberately narrowed to those two symbols, per `HOUSE-00063`'s measurement, and the
            reason is written into the job. `CNA_CNAEXT=OFF` removes the `CNA::Graphics::` engine
            layer — 0 against a control of 6 277 — but it does **not** remove
            `SupportsCapability`, `GraphicsCapability`, `getSkinsEXTProperty`, `setOwnedResources`,
            `ShaderEffect`, `PbrEffect`, `SkinnedPbrEffect` or `SkinnedModelEXT`, which live in
            `Microsoft::Xna::Framework::Graphics` in the always-compiled core and are linked
            regardless. For those, `check_xna_only.py` is the **only** gate rather than a second
            line of defence — which is why it must never be relaxed, and why this job says so.
- [x] HOUSE-00137 — CI job (nightly): `linux-asan` and `linux-ubsan` builds running `unit` + `integration`
      dep: HOUSE-00124 · sys: ci · plat: CI · pri: MUST
      note: (2026-09-06) Job `sanitizers`, nightly, a matrix over the `linux-asan` and
            `linux-ubsan` presets running the `unit` and `integration` labels. One variant per job:
            `HOUSE-00106`'s note and the openeggbert rules both record that sanitizer binaries in
            these projects reach 400–900 MB, so building both speculatively is exactly the waste
            those rules exist to stop.
- [x] HOUSE-00138 — CI job (nightly): `render` tests under `Xvfb` with `OPENGLES3`
      dep: HOUSE-00125 · sys: ci · plat: CI · pri: MUST
      note: (2026-09-06) Job `render-tests`, nightly, under `xvfb-run` at 1600×900 with
            `LIBGL_ALWAYS_SOFTWARE=1` so the job needs no GPU runner and its numbers are
            reproducible.
      scope: software rasterisation is **not** the driver phase 1 measured against, so a render
            test asserts geometry and coverage rather than exact shading. `HOUSE-00115`'s "re-run
            these if you change renderer" list is what says which verdicts are driver-specific.
- [x] HOUSE-00139 — Implement the frame timer and `FrameContext`: real dt, clamped dt, frame index, fixed-step accumulator
      dep: HOUSE-00129 · sys: app · plat: ALL · pri: MUST
      accept: a 250 ms hitch clamps to 4 physics substeps and does not spiral
      verify: unit FrameTimingTests.*
      note: (2026-09-06) `FrameTimer` and `FrameContext`, verified by `FrameTimingTests.*`. The
            acceptance case is asserted literally: a **250 ms hitch clamps to 4 substeps** and the
            following 30 frames never ask for more than 2, so it does not spiral. The delta is
            taken as a parameter rather than read from a clock, which is what makes the hitch
            behaviour testable without sleeping.
      finding: the residue past the step cap is **discarded, not carried**. Carrying it is exactly
            what compounds into the spiral of death, because the next frame would then ask for more
            steps than this one could not deliver. What is carried instead is a *count* of dropped
            steps, because a simulation running slower than real time is what a player reports as
            "sluggish" and is otherwise invisible.
- [x] HOUSE-00140 — Implement `IInputSource` and `KeyboardMouseSource`; no system may read `Keyboard`/`Mouse` directly
      dep: HOUSE-00132 · sys: player · plat: ALL · pri: MUST
      accept: the lint of HOUSE-00021 is extended to enforce it
      note: (2026-09-06) `IInputSource` and `KeyboardMouseSource`. Nothing in `InputState` mentions
            a key, a button or a pixel: `move` is a direction, `look` is an angular delta **in
            radians**, and presses are **edges computed once here** — two systems each tracking "was
            it down last frame" is two chances to disagree about the frame. That translation is why
            gamepad support, remapping and the phase-50 touch UI are changes to one class rather
            than to every system that reads input. Verified by `InputTests.*`, including that
            diagonal movement is not faster than cardinal.
      finding: **the mouse handling is written against what `HOUSE-00100` measured, not against
            what one would assume**, and these tests are where that is pinned down since the probe
            itself could not get real pointer motion. The previous position is seeded from the first
            *real* sample rather than an assumed window centre — assuming the centre makes the first
            frame one enormous bogus delta, the classic "camera snaps on startup". A capture change
            discards the history, or the first delta after closing a menu is the whole distance the
            pointer travelled. And an unchanged sample yields **no** look and `LookAvailable() ==
            false`, because "the player held still" and "no motion event arrived" are
            indistinguishable from here and neither should move the camera.
      finding: **`KeyboardState()` and `MouseState()` are `CNAEXT`-marked**; only the
            `initializer_list` constructor is plain XNA 4.0. So an input-state member cannot be
            default-constructed, and `check_xna_only.py` cannot see it — the identifier in the
            source is just the type name. `KeyboardMouseSource` keeps five booleans for the edges it
            reports instead, which is also all it needs. Recorded in `docs/conventions.md` §5a.
- [x] HOUSE-00141 — Implement the `Platform` capability struct and its desktop population
      dep: HOUSE-00140 · sys: app · plat: ALL · pri: MUST
      note: (2026-09-06) `app::Platform`, and every field says which of three kinds of fact it is:
            a **build constant** baked in by CMake, a **standard XNA query**
            (`GraphicsAdapter::CurrentDisplayMode` and the adapter description — plain XNA 4.0, not
            a capability query), or a **phase-1 measurement** encoded into the platform profile.
            A field that could only be filled by a forbidden query does not exist.
      finding: the measured fields are the phase-1 verdicts made actionable —
            `anisotropicFiltering` (`HOUSE-00109`), `floatRenderTargets` (`HOUSE-00083`),
            `occlusionQueryIsBoolean` (`HOUSE-00090`/`00091`), `blockCompressedTextures`
            (`HOUSE-00111`) and `drawCallMicroseconds = 8.15` (`HOUSE-00106`). The draw budget is
            computed from that last one rather than from a remembered rule of thumb, and it is a
            field rather than a constant because a different machine has a different number.
- [x] HOUSE-00142 — Implement `ContentRegistry`: content name ↔ asset id ↔ pack, loaded from `assets.manifest.json`
      dep: HOUSE-00028 · sys: content · plat: ALL · pri: MUST
      note: (2026-09-06) `content::ContentRegistry`, loading the runtime subset of
            `assets.manifest.json` (`cna-house.md` §20.3): id, kind, content name, residency pack.
            The manifest's source hashes, licence and review status are for the offline tooling and
            the phase-4 audit and are deliberately **not** modelled here — none of it is read at run
            time. Verified by `ContentRegistryTests.*`.
      finding: the indirection earns itself three times over, and none of them is optional.
            **Residency** promotes and evicts by *pack*, and nothing in a content name says which
            pack an asset is in. **Ids** let world data reference a prop without a path, so renaming
            a file is not a world-data migration. **Validation** catches a bad row at startup,
            naming it, rather than as a `ContentLoadException` when a player walks into the room.
      finding: **every bad row is reported, not the first** (`docs/conventions.md` §5.1) — fixing
            forty authoring mistakes one build at a time is intolerable, and a manifest is exactly
            the file that accumulates them. A failed load leaves the registry **empty** rather than
            half-populated, because a half-loaded manifest would start the game and then fail at the
            first asset that happened to be in the missing half.
- [x] HOUSE-00143 — Implement `TextureCache`, `ModelCache`, `SoundCache`, `EffectCache` over `ContentManager`, with fallback assets and a load-failure policy
      dep: HOUSE-00142 · sys: content · plat: ALL · pri: MUST
      accept: a missing asset yields the typed fallback, logs once, and does not throw
      verify: unit ContentCacheTests.*
      note: (2026-09-06) `content::AssetCache<T>` plus `Caches`. All three acceptance points hold,
            verified by `CachesTests.*` — an **integration** test, deliberately, because
            `Load<Model>` needs a real `GraphicsDevice` and a stubbed `ContentManager` would be
            testing the stub when the whole behaviour under test is what happens at the XNA content
            boundary.
      finding: the policy is the design. A missing asset **yields the fallback** — a house with one
            missing prop should still be walkable, and an exception here would turn a content
            mistake, the most common kind this project will have, into an unplayable build. It
            **logs once** — verified with 500 requests producing exactly one message — because a
            missing texture is requested every time the room is drawn, and `util::Log`'s *per-frame*
            limiter is not enough when the same asset fails every frame forever. And it **does not
            throw**, because the caller is a draw path (§5.2).
      finding: one template rather than four hand-written caches, so their policies cannot drift
            apart. The loaders differ only because `Load<T>` has three shapes (`HOUSE-00070`,
            `HOUSE-00087`, `HOUSE-00098`), and wrapping each once here keeps that inconsistency in
            one file instead of at every call site.
      finding: `Clear()` also clears the **failure** record, deliberately: after a content rebuild
            an asset that was missing may now be present, and a cache that remembered forever would
            keep showing the fallback.
- [x] HOUSE-00144 — Author the fallback assets: grey box model, mid-grey texture, magenta debug texture, silent sound
      dep: HOUSE-00143 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-06) `tools/assets/make_fallback_assets.py` generates all four, and all four
            compile through the pipeline and load at run time (`CachesTests.EveryFallbackAsset
            ActuallyLoads`): a 1 m grey box `.glb` (2 036 B), a 4×4 mid-grey PNG (75 B), a 16×16
            magenta/black checker (92 B) and a quarter-second silent WAV (22 kB).
      finding: **authoring them procedurally settles their provenance completely** (ADR-0012):
            there is no source to trace, no licence to read and no redistribution question. That
            matters more for these than for any other asset, because a fallback ships in every build
            and is the one asset guaranteed to reach a player's screen if anything goes wrong.
      finding: they are deliberately **ugly**, and the two texture fallbacks are deliberately
            **different**. A fallback that looks plausible hides a missing asset until someone
            notices the fridge is a grey box — possibly months. A missing *albedo* gets neutral grey
            so the lighting still reads correctly and a missing texture does not also look like a
            lighting bug; a missing *required* texture gets a magenta **checker**, checkered rather
            than flat because flat magenta can be mistaken for an authored colour and a checker
            cannot. The box is 1 m on a side so a missing prop does not also look like a scale bug,
            and the silent WAV is a real quarter second rather than an empty file, because a
            zero-length sound is a different failure from a successfully-loaded silent one.
- [x] HOUSE-00145 — Implement `SpriteFont` loading and a `TextRenderer` helper (measure, draw, drop shadow, virtual-unit scaling)
      dep: HOUSE-00143 · sys: ui · plat: ALL · pri: MUST
      note: (2026-09-06) `ui::TextRenderer` with measure, anchored draw, drop shadow and virtual-unit
            scaling. Verified by `TextRendererTests.*` for everything reachable without a
            `GraphicsDevice`; anchoring needs `MeasureString` and therefore a device, so it belongs
            to `HOUSE-00164`'s render harness rather than to a stub that would prove nothing.
      finding: **virtual units are the point.** The HUD is authored against 1600×900 and the window
            can be anything, so a position in pixels is right on one machine and wrong on every
            other. The scale is **uniform and takes the smaller of the two ratios**, so text never
            overflows the tighter axis and the glyphs are never stretched.
      finding: the drop shadow is not decoration. White text on a bright window frame or a sunlit
            wall is unreadable and this house has both; one offset copy at 60 % black makes every
            string legible against every background the game produces, for one extra `DrawString`.
- [x] HOUSE-00146 — Draw the version string and frame time in the corner; the first real thing on screen
      dep: HOUSE-00145 · sys: ui · plat: ALL · pri: MUST
      note: (2026-09-06) The version string is top-left and the frame time top-right, both through
            `TextRenderer::DrawShadowed`, in **one** `SpriteBatch` — `HOUSE-00106` measured a draw
            call at 8.15 µs of CPU, so a batch per string would spend more on submission than the
            rest of the frame does. Verified by running the game: the HUD font loads from the built
            content tree and the strings draw.
      finding: the frame-time line shows **both** milliseconds and frames per second, deliberately.
            Milliseconds is the number a budget is written in and the one that adds up across
            systems; fps is the number a person feels. Showing only fps hides that 60 → 50 is a
            bigger regression than 30 → 28. The value is a short exponential average (α = 0.1),
            because the instantaneous delta jitters by a millisecond or two every frame and makes a
            real regression invisible inside the noise.
- [x] HOUSE-00147 — Implement `DebugDraw`: lines, wire boxes, wire spheres, wire frusta, filled quads, all on `BasicEffect`
      dep: HOUSE-00127 · sys: debug · plat: ALL · pri: MUST
      accept: compiled out entirely when `CNAHOUSE_DEBUG_TOOLS=OFF`
      note: (2026-09-06) `debug::DebugDraw` with lines, wire boxes, wire spheres, wire frusta and
            filled quads, all on `BasicEffect`. Compiled always; the OVERLAY is what
            `CNAHOUSE_DEBUG_TOOLS` gates, because a counter that exists only in a debug build cannot
            be asserted by a perf test — which would make the perf tests measure a different
            program.
      finding: **everything is batched into one dynamic vertex buffer and drawn in one call per
            primitive type.** The obvious implementation — a `DrawUserPrimitives` per wire box —
            would cost more than the scene it annotates and would move the very timings the overlay
            reports: `HOUSE-00106` measured 8.15 µs of CPU per draw call, while `HOUSE-00092`
            measured 2 000 dynamic quads at 0.171 ms. A sphere is three orthogonal circles rather
            than a mesh, because a mesh reads as a solid at a glance and hides what is behind it.
            `DepthRead` rather than `DepthStencilState::None`, so a line genuinely behind a wall is
            hidden and the overlay stays legible in a house rather than becoming a thicket.
      finding: the vertex struct holds a **packed `std::uint32_t`** colour, not a `Color` — `Color`
            is not trivially copyable and so cannot go in a struct passed to `SetData<T>`, measured
            while writing the phase-1 probes.
- [x] HOUSE-00148 — Implement the counters system: named per-frame counters with min/avg/max over a 240-frame window
      dep: HOUSE-00147 · sys: debug · plat: ALL · pri: MUST
      note: (2026-09-06) `debug::Counters`: named counters with min, average and max over a rolling
            240-frame window, resolved to a handle once and incremented by index thereafter — lookup
            by string every frame would put a string hash on a per-frame path for nothing. Verified
            by `CountersTests.*`.
      finding: **a window rather than an instantaneous value**, because "what is it now" is the
            least useful question: draws spike when a room comes into view, visible cells spike at a
            doorway, and the number a person happens to see is whichever frame their eye landed on.
            240 frames is four seconds at 60 Hz — long enough that walking through a doorway is
            entirely inside it, short enough that the numbers still respond while someone moves
            around looking for the spike.
- [x] HOUSE-00149 — Implement the CPU timing scopes and the per-system timing breakdown
      dep: HOUSE-00148 · sys: debug · plat: ALL · pri: MUST
      note: (2026-09-06) `debug::Timing` with an RAII `Scope`, per-`UpdateStage`, over the same
            240-frame window so the overlay's columns agree. Verified by `CountersTests.Timing*`.
      finding: time **accumulates within a frame** rather than overwriting: `Physics` runs up to
            four times per frame (`FrameTimer::kMaxFixedSteps`) and a budget cares what the frame
            cost, not what the last substep cost. Per **stage** rather than per system, because
            `UpdateStage` is the unit a budget is written in and a per-system list is one nobody can
            hold in their head. **CPU time, not GPU**: `HOUSE-00106` measured that submission alone
            is 8.15 µs per draw, so the CPU is the budget that binds here — and a GPU number needs a
            sync, which distorts the thing being measured. The phase-1 probes paid that cost
            deliberately; a per-frame overlay must not.
- [x] HOUSE-00150 — Implement the `F1` performance overlay (FPS, frame graph, system times, counters)
      dep: HOUSE-00149 · sys: debug · plat: ALL · pri: MUST
      note: (2026-09-06) The `F1` overlay: build summary, CPU total against the 16.67 ms budget, a
            frame graph, the per-stage table and the counters. Verified by `OverlayTests.*` —
            possible **without a device** because the overlay is a *presenter* that owns no
            measurement and returns its content as text.
      finding: the graph's ceiling is **33.3 ms, not 16.6**. A graph whose ceiling is the target
            clips exactly when something goes wrong, which is the moment the shape matters most. And
            the graph exists at all because an average tells you the cost while the *shape* tells
            you whether something hitches every two seconds — which is the bug that actually gets
            reported and is invisible in an average. It is a **text** graph deliberately: no vertex
            buffer, no second effect, works identically under `HEADLESS`, and assertable in a test.
- [x] HOUSE-00151 — Implement the screenshot command and the `--screenshot` flag (writes PNG via `Texture2D::SaveAsPng`)
      dep: HOUSE-00146 · sys: debug · plat: ALL · pri: MUST
      note: (2026-09-06) `debug::Screenshot` plus the `--screenshot` flag and `F12`. **Verified end
            to end**: `./build/cna-house --screenshot=shot.png` wrote a 1600×900 8-bit RGBA PNG
            showing the clear colour, the version string and the frame time — which is also the
            Phase-2 exit criterion demonstrated as an image rather than asserted.
      finding: **the capture renders the frame again into a `RenderTarget2D` rather than reading the
            back buffer**, because XNA offers no way to read the presented buffer — and because
            drawing into a target makes the image independent of the compositor (no title bar, no
            cursor, nothing on top), which is the only form usable as a regression fixture
            (`HOUSE-00164`). `RenderFrame()` is factored out so the capture draws exactly what the
            player sees rather than a second path that could drift from it.
      finding: **`SaveAsPng` on a render target throws** *"no CPU-side pixel data available"* — its
            pixels are on the GPU and the const save path has no shadow copy. The fix is not a
            workaround but the two calls phase 1 already proved: `GetData` into a `Color` array
            (`HOUSE-00083` read a 2048² target back bit-exactly) and `SetData` into a staging
            texture (`HOUSE-00078`). Also: `SaveAsPng(const std::string&)` is `CNAEXT`; only the
            stream overload is plain XNA, so the file is opened with `System::IO::FileStream`.
- [x] HOUSE-00152 — Implement `ISaveStore` with `DesktopSaveStore` over `StorageDevice`; atomic write, backup, read
      dep: HOUSE-00102 · sys: persistence · plat: LNX · pri: MUST
      verify: unit SaveStoreTests.* incl. an interrupted-write simulation
      note: (2026-09-06) `ISaveStore` and `DesktopSaveStore` over `StorageDevice`/`StorageContainer`,
            with ADR-0008's atomic sequence — write `<name>.tmp`, promote the current save to
            `<name>.bak`, rename the temporary into place, remove the temporary. Verified by
            `SaveStoreTests.*` (integration, because the behaviour under test *is* the filesystem
            sequence and a stub would test the stub): the round trip is exact, the second write
            leaves the first as a readable backup, no `.tmp` survives, a missing save is `NotFound`
            rather than a throw, delete is idempotent, an empty payload is distinguishable from a
            missing file, and a 200 kB payload survives the chunked read.
      finding: **the container name is how `cna-house` identifies itself**, and that follows
            directly from `HOUSE-00102`: `StorageDevice`'s `<app>` component is the literal string
            `game`, and the only way to change it is `SetAppNameEXT`, which ADR-0001 forbids. The
            container name *is* ours through plain XNA, so `CnaHouse` is what separates these saves
            from every other CNA application's — giving `~/.local/share/game/CnaHouse/`.
      finding: the interface is **text in, text out**, not bytes. ADR-0008 requires saves to be
            human-readable JSON, and a byte-oriented interface would invite someone to put a struct
            through it — which is precisely the "never serialise raw memory" rule.
      finding: an empty payload is a reachable case, not a degenerate one: a house in exactly its
            canonical initial state saves an **empty delta**, and it must not be indistinguishable
            from a missing file. The chunked read is likewise the normal path, since ADR-0008 sizes
            a heavily explored house at ~90 kB against an 8 kB chunk.
- [x] HOUSE-00153 — Implement the crash boundary: catch at `Update`/`Draw`, log, emergency-save, offer restart
      dep: HOUSE-00152 · sys: app · plat: ALL · pri: MUST
      note: (2026-09-06) `Update` and `Draw` are each wrapped, and `HandleCrash` logs what was
            thrown **with the frame it happened in**, writes a `crash.json` report through the save
            store, and stops. Verified by running the game unchanged afterwards.
      finding: **this is not a `catch (...)` that swallows**, which `docs/conventions.md` §5.4
            forbids. The `catch (...)` arm exists to record that something *not* derived from
            `std::exception` escaped — which is itself the most useful fact available about it — and
            then stop. A game that keeps running after an unhandled exception produces a second,
            less comprehensible failure on top of the first.
      finding: the emergency save is attempted **first**, and its own failure is reported rather
            than allowed to mask the original crash. That is the classic way a crash report ends up
            describing the handler instead of the bug. `HandleCrash` is also re-entrant-safe: a
            second failure while handling the first returns immediately rather than recursing.
- [ ] HOUSE-00154 — Implement `AudioSystem` skeleton: device init, graceful `NoAudioHardwareException` handling, master volume
      dep: HOUSE-00097 · sys: audio · plat: ALL · pri: MUST
      accept: `--no-audio` and a missing device both leave the game fully playable
- [ ] HOUSE-00155 — Implement the user-gesture audio gate (title screen "press any key") on every platform
      dep: HOUSE-00154 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-00156 — Implement the loading screen and the `MenuStack` skeleton
      dep: HOUSE-00145 · sys: ui · plat: ALL · pri: MUST
- [x] HOUSE-00157 — Implement the quality-tier table and the auto-detect heuristic (from `GraphicsAdapter` — standard XNA — and the project-owned effective feature set of `cna-house.md` §68; no CNA-specific capability query)
      dep: HOUSE-00131 · sys: rendering · plat: ALL · pri: MUST
      note: (2026-09-06) `rendering/Quality.{hpp,cpp}`: `QualitySettings` (§68's Graphics tab
            resolved), `SettingsFor(preset)` as a pure table, `Restrict(settings, platform, tier)`
            as the effective feature set applied, and `AutoDetect(platform, tier)`. Fourteen tests,
            and they are **unit** tests — which is itself the claim: if any of this needed a
            `GraphicsDevice` it would be a capability query, and it compiles and runs without one
            because it is not.
      note: `QualityPreset` gained `Ultra`, so the table has §68's four fixed rows. **`Custom` was
            deliberately not added**: it means "whatever the user set in the Graphics tab", and
            there is no Graphics tab yet — a `Custom` resolving to a fixed row would be a
            placeholder pretending to be a feature. It arrives with the settings UI.
      note: `Options::quality` became `std::optional`, because §68 gives the preset the default
            *auto-detected* and a fixed default would have made `AutoDetect` unreachable for anyone
            who did not know to ask for it. Two existing tests failed on this and both were right to:
            one asserted the old fixed default, and one asserted that `--quality=ultra` was rejected.
      finding: **the heuristic is deliberately coarse, and the honest reason is that standard XNA
            4.0 has nothing to be precise with.** `GraphicsAdapter` gives a description string and
            the display modes; there is no VRAM figure, no GPU class, no feature level, and ADR-0001
            forbids asking CNA for more. So it acts confidently on the one reliable signal — a
            software rasteriser by name (`llvmpipe`, `softpipe`, `swrast`, Mesa offscreen), which no
            quality setting makes fast, and which CI runs on deliberately (`HOUSE-00138` sets
            `LIBGL_ALWAYS_SOFTWARE=1`) — steps down one row at 3 840 pixels wide because the same
            GPU must fill four times 1080p and this project has no dynamic resolution, and otherwise
            guesses. **It never returns `Ultra`.** Guessing a machine into the top row from a name
            string is exactly the confidence the available facts do not support.
      finding: `Restrict` is where "nothing meaningless is offered" becomes true rather than stated.
            Tier S keeps **blob** shadows and loses only the two Tier-E passes — which is what makes
            ADR-0003's "Tier S is complete" a fact: a Tier-S session still has shadows, cheaper ones.
            Without `HOUSE-00083`'s float render targets the shadow map drops to blobs rather than
            shipping an untested RGBA8 packing; without `HOUSE-00109`'s anisotropy every preset
            samples trilinear (`anisotropy == 1`, which is §68's named fallback, not "filtering off").
      finding: **MEASURED — CNA's `Game::Initialize()` calls `LoadContent()` at its end**, exactly as
            XNA 4.0 does (`modules/runtime/src/Game.cpp`). Everything this override assigned *after*
            the base call was therefore still empty while content loaded, which is where the tier and
            the quality are resolved. The first run auto-detected against a blank profile: it logged
            "adapter unknown" and forced anisotropy to 1 on a machine that has it. `Initialize` now
            sets the platform profile **before** `Game::Initialize()` and creates only the
            device-dependent members after it. This belongs in `docs/conventions.md` §5a, and is
            there.
      finding: the resolved knobs are logged every start, not just the preset name. "high" is
            ambiguous — the same preset draws differently on a Tier-S build and on a profile without
            float render targets — so the line names shadows, particles, view distance, LOD bias,
            anisotropy and post-processing, and a second line fires only when the feature set
            actually took something away.
- [x] HOUSE-00158 — Implement the `StateTracker`: skip redundant `BlendState`/`DepthStencilState`/`RasterizerState`/`SamplerState` sets, and count changes
      dep: HOUSE-00127 · sys: rendering · plat: ALL · pri: MUST
      verify: unit StateTrackerTests.* replaying a recorded command list
      note: (2026-09-06) `rendering/StateTracker.{hpp,cpp}`. Comparison is by **pointer identity**,
            not by value: every state this project sets is a shared singleton — `RenderStates.hpp`'s
            three, or XNA's own `BlendState::Opaque` and friends — so identity is the right
            question, and a member-by-member comparison would cost more than the set it avoids.
      note: the tests landed in `tests/integration/`, **not** `tests/unit/` as the `verify:` line
            assumed, and the plan line is left as written rather than rewritten to match. The class
            exists to talk to a `GraphicsDevice`; a unit test with a mock device in its place would
            verify only that the mock and the tracker agree. Under the `headless` preset it gets a
            real device with no window, so `setBlendStateProperty` is really called and a state the
            device rejects fails here rather than in a nightly render job. Six tests.
      finding: **the counters are the point of this class, not the skipping.** `HOUSE-00106`
            measured `EffectPass::Apply()` at 0.184 µs against a draw call at 8.15 µs — a ratio of
            44 — so the saving from a skipped state set is close to noise. What is *not* noise is
            the number: a renderer that changes blend state 900 times to issue 300 draws is sorting
            its render list wrongly, and nothing else in the frame makes that visible. The counters
            are therefore compiled always, like `debug::Counters`, and not behind
            `CNAHOUSE_DEBUG_TOOLS` — a perf test that measured a different program would be
            measuring nothing.
      finding: `Invalidate()` clears **everything**, not the one kind that changed. `SpriteBatch::End`
            restores several states at once and a render-target change resets others; tracking which
            would be a second model of XNA's behaviour to keep in sync, and a tracker that believes
            a stale binding skips the set that was actually needed — a frame drawn with someone
            else's blend state, which is far worse than a redundant set.
      finding: sampler slots are tracked independently and an out-of-range slot is ignored rather
            than counted. One remembered sampler would skip slot 1 because slot 0 already held that
            object, and the second texture would be sampled with the wrong filter.
- [x] HOUSE-00159 — Implement `Renderer` with the pass list of `cna-house.md` §7.5 as empty passes
      dep: HOUSE-00158 · sys: rendering · plat: ALL · pri: MUST
      note: (2026-09-06) `rendering/Renderer.{hpp,cpp}`: a `Pass` enum in §7.5 order, `IRenderPass`,
            a `PassContext`, and a `Renderer` that walks the enum. Eleven tests.
      note: **"empty passes" is taken as scaffolding, and this project's rules require scaffolding
            to be real infrastructure rather than a dead layer.** No placeholder pass is shipped:
            an uninstalled pass is skipped and counted, which is exactly what an empty frame should
            report. What IS real from the first commit is the part that is painful to retrofit —
            the order, the tier gate, the state invalidation between passes and the per-pass timing.
            The one pass that exists, `Pass::Hud`, is the game's actual HUD wired through the
            renderer, so the layer is exercised by every frame the application draws rather than
            waiting for phase 12 to find out whether it works.
      finding: **the enum order IS the frame order, and there is no way to reorder it.** Sky before
            opaque so the dome is overdrawn rather than overdrawing; alpha-test before transparent
            so cut-outs write the depth the sorted pass tests against. Those are architectural
            decisions, and a runtime-sortable list would let a caller discover them by getting one
            wrong.
      finding: the tier gate lives in `Renderer::WillRun` and nowhere else. `PassIsTierEOnly` is
            true for exactly two passes — shadow and composite — and a test asserts the count is
            two, because ADR-0003's promise that Tier S is *complete* holds only while the Tier-E
            passes are the ones whose absence changes how the frame LOOKS rather than what it
            contains.
      finding: `IRenderPass::DisturbsDeviceState()` is what makes HOUSE-00158's tracker safe in a
            multi-pass frame. `SpriteBatch::Begin`/`End` sets and restores several states together,
            so the renderer invalidates after any pass that says yes. Both directions are tested:
            with the flag, two passes setting the *same* `BlendState::Opaque` produce two real
            applies; without it, one apply and one skip. The second test is the control — without
            it, "two applies" would also be consistent with a renderer that invalidated after every
            pass, which would make the flag meaningless.
      finding: `IsActive()` is asked once per frame by the renderer rather than left to each pass to
            return early, so that *ran* and *had nothing to do* are different numbers. A pass that
            quietly did nothing for a hundred frames is a bug that looks exactly like a pass that is
            working.
- [x] HOUSE-00160 — Implement `render::RenderTier`: the build-time tier fact (`CNAHOUSE_TIER_E`, HOUSE-00122) plus the runtime-resolved active tier, published once and read by everything else. **No device capability query anywhere** — nothing in the type touches `GraphicsDevice` for this purpose.
      dep: HOUSE-00159, HOUSE-00122 · sys: rendering · plat: ALL · pri: MUST
      files: src/rendering/RenderTier.cpp|hpp
      accept: (1) with `CNAHOUSE_TIER_E=OFF` the Tier-E branch is absent from the binary; (2) `check_xna_only.py` proves no file calls `SupportsCapability`; (3) the active tier is logged once and shown in the debug overlay
      verify: unit RenderTierTests.*; one build of each configuration
      note: (2026-09-06) `rendering/RenderTier.{hpp,cpp}`. Built **before** its stated dependency
            HOUSE-00159 (`Renderer` pass list), and that ordering is deliberate rather than an
            oversight: `RenderTier` has no `Renderer` in it, the dependency was a sequencing guess
            made when the plan was written, and HOUSE-00161/00163 both needed the tier first.
            HOUSE-00159 is unblocked and unchanged.
      note: accept (1) — with `CNAHOUSE_TIER_E=OFF` the Tier-E branch is absent — is satisfied by
            `if constexpr`, not by an ordinary `if`. This is the difference between *absent* and
            *unreached*: the first version used a plain early return, which left the second
            `ContentManager`, the effect-set load and the asset name `Effects/P1Probe` in the image.
            Verified on the `headless` preset, where `TierSelection.cmake` turns Tier E off by
            itself, `strings -a build-consumer/cna-house | grep -c 'Effects/P1Probe'` returns **0**
            against **1** for `build/cna-house`, and a control string present in both rules out a
            stripped binary.
      note: accept (2) — `check_xna_only.py` reports clean, and nothing in the type touches
            `GraphicsDevice` at all. accept (3) — the active tier is logged once from `LoadContent`
            and drawn every frame in the corner line (see the `SessionLine` finding below).
      finding: **the type is asymmetric on purpose: `FallBackToS` narrows and there is no
            `PromoteToE`.** A binary without compiled effects in its content tree cannot acquire
            them at run time, and a method implying otherwise would eventually be called.
            `FallBackToS` returns whether it *changed* the tier, so a repeated failure produces one
            log line rather than one per frame, and the first reason is kept rather than the last —
            later failures are consequences of the first.
      finding: `TierEselectable()` goes false after a fallback. Offering a settings toggle that
            cannot work is worse than offering none, because the user will try it and conclude the
            game is broken.
      finding: the unit tests assert against `RenderTier::CompiledIn()` rather than against a
            literal `true`/`false`, so they are meaningful in **both** configurations instead of
            only in the one the author happened to build.
      finding: **building the second configuration is what made the tests worth having.** One
            assertion — that `FallBackToS` records its reason — passed on the Tier-E build and
            failed on `headless`, because the type's real invariant is narrower than it looked: a
            non-empty `FallbackReason()` means the tier was *moved at run time*, and on a Tier-S-only
            build nothing was moved. The assertion was wrong, not the code; reporting a fallback that
            did not occur would be a false diagnostic in a bug report. Both configurations now run
            190/190.
- [x] HOUSE-00161 — Implement Tier-E activation as a guarded content load: in `LoadContent`, load the Tier-E effect set inside one `try`/`catch (ContentLoadException | NotSupportedException)`; any failure selects Tier S, logs once with the failing asset, and disables the settings toggle. Add `--tier=s` and the settings entry, which force Tier S and can never force Tier E.
      dep: HOUSE-00160 · sys: rendering · plat: ALL · pri: MUST
      accept: (1) deleting one Tier-E `.xnb` yields a fully playable Tier-S run with one logged line and no exception escaping `LoadContent`; (2) `--tier=s` on a Tier-E build renders the Tier-S path; (3) no code path can turn Tier E on
      verify: integration TierFallbackTests.MissingEffectFallsBackToTierS; render test `tier-fallback-01`
      note: (2026-09-06) `CnaHouseGame::ActivateTierE`, called once from `LoadContent`, plus
            `tests/integration/TierFallbackTests.cpp` (3 tests, including the one this line names).
            The render test `tier-fallback-01` waits for HOUSE-00164's harness and is recorded there
            rather than claimed here.
      note: the whole effect set loads inside **one** `try`, so a partial load is impossible. Half a
            tier is a renderer that works until it reaches the pass whose effect is missing, which
            fails in the middle of a frame instead of at load. Only `Effects/P1Probe` is in the set
            so far; phase 12 adds the real one and the shape of the function does not change.
      note: accept (1) verified for real, and not by deleting a file: the test points `effectRoot`
            at a directory with no `Effects/` subtree, which is the exact shape of a build whose
            `content-fx` tree was never generated — and that is not hypothetical, it happened during
            this task's first run. 30 frames drawn, exit code 0, tier S, one logged line.
            accept (2) `--tier=s` on this Tier-E build logs `Tier S (requested, or narrowed by
            --tier=s)` and never touches the effect content. accept (3) there is no code path that
            turns Tier E on: `TierSelection.cmake` refuses to force it on at configure time and
            `RenderTier` has no widening method.
      finding: **Tier E needs a SECOND `ContentManager`, with `content-fx` as its root.**
            `HOUSE-00064` measured that `.xnb` wins the resolution order over `.cnb` within one
            root, so a single tree would let a Tier-E `.xnb` effect silently shadow a Tier-S `.cnb`
            asset of the same name. Two roots make that impossible rather than unlikely.
      finding: the drawn corner line had to be split in two. `VersionLine()` is static and names
            what the **build** contains (`Tier S+E`); `SessionLine()` names what the **session** is
            running and appends `· running S` when they disagree. A `--tier=s` screenshot of a
            Tier-E build previously read `Tier S+E`, which misattributes the frame it is a
            screenshot of — and a screenshot is the primary bug-report artefact.
      finding: `catch (const std::exception&)` is the right width here and not a swallow.
            `ContentLoadException` and `NotSupportedException` are the two documented failures and
            both mean one thing to the caller — this binary cannot draw Tier E — so distinguishing
            them would produce two branches that do the same thing. The message is preserved
            verbatim in `FallbackReason()` and logged.
- [x] HOUSE-00162 — Implement `MaterialBinder` skeleton: material id → effect instance + parameters
      dep: HOUSE-00161 · sys: rendering · plat: ALL · pri: MUST
      note: (2026-09-06) `rendering/MaterialBinder.{hpp,cpp}`: a `MaterialKind` of four, a
            device-independent `MaterialDesc` loaded from data, a `DrawParams` for what a material
            cannot know, `Register` / `Find` / `CullFor` / `Bind`. Twelve tests against a real
            device, so every parameter written is one the effect really accepted.
      note: `MaterialKind` has **four** members and the list is closed for Tier S. ADR-0003 promises
            Tier S is complete on stock effects only, and phase 1 measured all four end to end
            against analytic expectations — `HOUSE-00082` (`BasicEffect`, byte-exact),
            `HOUSE-00078` (`DualTextureEffect`), `HOUSE-00080` (`AlphaTestEffect`),
            `HOUSE-00075`/`HOUSE-00077` (`SkinnedEffect`). A fifth kind is a fifth thing to measure.
      finding: **one effect instance per KIND, not per material.** An XNA effect object holds
            whatever parameters were last written to it, so they are set per draw whatever happens;
            an instance per material would buy nothing and cost one shader object per material in
            the house. `HOUSE-00106` measured `EffectPass::Apply()` at 0.184 µs against 8.15 µs for
            the draw, so re-writing parameters is not where the frame goes. Instances are created on
            first use, not at registration — a test asserts `EffectsCreated() == 0` after two
            registrations.
      finding: **what the binder REFUSES is the point of it.** Two phase-1 measurements are enforced
            at registration or bind, where the error can name the material, rather than being left
            to throw two hundred draws later where it could only name the effect: an unlit skinned
            material (`HOUSE-00077` — `SkinnedEffect` throws *"does not support setting
            LightingEnabled to false"*, exactly as XNA 4.0 does), and a bone palette over 72
            (`HOUSE-00077` — 72 accepted, 73 throws). Both are returned as `util::Result` errors and
            not thrown: a skin one bone over the limit is a content problem, and the frame should
            say so and keep going. The 72 case is asserted against a real `SkinnedEffect`, so it is
            still a measurement and not a remembered number.
      finding: a duplicate material id is refused rather than replacing silently. Replacing is the
            worse failure — two rooms sharing a material name would render correctly for whichever
            loaded second, a bug that reproduces one room at a time.
      finding: a `DualTexture` material with no second texture is refused as `InvalidData`. Left
            alone it draws black, because `HOUSE-00078`'s lightmap product is with nothing; what the
            author meant was `Basic`. The `*2` doubling factor that probe found is documented at the
            bind site with an explicit instruction NOT to compensate for it a second time.
      finding: the constructor as well as the destructor had to move into the .cpp. The effect
            members are `unique_ptr`s to forward-declared XNA types, and an inline constructor needs
            their complete types for the exception path that unwinds a partly built object — which
            would drag `BasicEffect`, `SkinnedEffect` and the rest into every translation unit that
            merely names a material. The out-of-line destructor alone is not enough, which is the
            half of the pimpl idiom that is usually left out.
- [x] HOUSE-00163 — Implement the shared `RasterizerState` objects (`CullClockwise` default for glTF-derived geometry, `CullCounterClockwise` for mirrored, `CullNone` for foliage/sky)
      dep: HOUSE-00071 · sys: rendering · plat: ALL · pri: MUST
      note: (2026-09-06) `rendering/RenderStates.{hpp,cpp}`: a `CullPolicy` enum, `StateFor(policy)`
            returning a reference to the shared state, and `PolicyForDeterminant(det, twoSided)`.
            Seven unit tests in `tests/unit/RenderTierTests.cpp`.
      note: the enum has **four** members for three states. `ProceduralFront` is the same
            `CullClockwise` as `ImportedFront` under its own name, because
            **procedural geometry does not inherit the glTF winding convention** and forgetting that
            is silent. `HOUSE-00080`'s first fixture was wound top-left → top-right → bottom-right,
            which in NDC (+Y up) is *clockwise*, and the entire quad vanished: ten checks failed for
            one fixture reason. The separate name is where the next generator author is told.
      finding: `PolicyForDeterminant` exists so that the mirroring rule is applied in one place. A
            negative world determinant means an odd number of axes were flipped, so the winding seen
            by the rasteriser reverses and the cull state must reverse with it; a mirrored prop drawn
            with `ImportedFront` is inside-out, which reads as a *hole* rather than as a backwards
            object. Two-sided wins over mirrored: a foliage card is meant to be seen from behind
            whether or not its placement mirrors.
      finding: the states are shared objects compared by address, which is what makes
            `StateTracker`'s pointer-identity skip correct. XNA state objects are immutable after
            first use anyway, and `HOUSE-00106` measured a draw call at 8.15 µs of CPU — there is no
            room in that for a per-draw allocation.
- [ ] HOUSE-00164 — Add the first render regression test harness: fixed pose, fixed clock, render, compare PNG with tolerance
      dep: HOUSE-00151, HOUSE-00138 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00166 — Define `docs/anim-format.md`: the project-owned `.chanim` binary sidecar — magic, version, joint list by name in skin-joint order, parent indices, bind and inverse-bind poses, clips as per-bone TRS keyframe tracks, stride length and foot-plant markers
      dep: HOUSE-00074, HOUSE-00028 · sys: animation · plat: ALL · pri: MUST
      files: docs/anim-format.md
      accept: (1) fully specified with byte offsets and a worked example; (2) a version field and a rejection rule for unknown versions; (3) nothing in it could only have come from a CNA type
- [ ] HOUSE-00167 — Implement `anim::Skeleton`, `anim::Clip`, `anim::ClipLibrary`, the `.chanim` reader over `TitleContainer::OpenStream` + `System::IO::BinaryReader`, `ClipLibrary::BindTo(const Model&)`, and an `AnimationCache` alongside the other content caches
      dep: HOUSE-00166, HOUSE-00143 · sys: animation · plat: ALL · pri: MUST
      files: src/animation/Skeleton.cpp|hpp, src/animation/ClipLibrary.cpp|hpp, src/animation/ChanimReader.cpp|hpp
      accept: (1) a hand-written fixture round-trips; (2) `BindTo` fills `modelBoneIndex` for a matching model; (3) a joint absent from `Model::Bones` is a **fatal** load error naming the joint, never a silent deformation; (4) a truncated or wrong-version file is rejected with a precise message; (5) no CNA symbol appears in any of these files
      verify: unit ChanimReaderTests.*, ClipLibraryTests.BindMismatchIsFatal
- [ ] HOUSE-00165 — Phase-2 review and commit; update the performance log with the empty-scene frame time
      dep: HOUSE-00121…HOUSE-00167 · sys: — · plat: ALL · pri: MUST

---

## Phase 3 — Content pipeline

**Goal.** Every source format in the project compiles and loads, reproducibly, with the tooling in
`tools/`.

**Exit.** `make content` builds `assets-src/` into `content/`; `make content-verify` proves
determinism; a smoke scene loads a model, a texture, a font, a sound, an effect and a video.

- [ ] HOUSE-00181 — Create `assets-src/` with its subdirectories and a `.cna-content.json` per tree
      dep: HOUSE-00126 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00182 — Wire `cna_add_content` for the main tree; confirm an incremental no-op build is fast
      dep: HOUSE-00181 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00183 — Wire the effects tree with `--format xnb --fx-compiler --fx-compiler-launcher`, driven by CMake cache variables so a machine without Wine can skip it
      dep: HOUSE-00087, HOUSE-00182 · sys: content · plat: TOOL · pri: MUST
      accept: with `CNAHOUSE_TIER_E=OFF` the tree is skipped and the committed `.xnb` files are used instead
- [ ] HOUSE-00184 — `tools/effects/build_effects.sh`: the reproducible wrapper around the wine+fxc invocation, with the compiler hash recorded
      dep: HOUSE-00183 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00185 — Commit the baseline compiled `Effects/*.xnb` alongside their `.fx` sources, with a README explaining why (BL-04)
      dep: HOUSE-00184 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00186 — `tools/assets/gltf_validate.py`: run `gltf-validator` if present plus a CNA importer pass with warnings-as-errors
      dep: HOUSE-00181 · sys: content · plat: TOOL · pri: MUST
      accept: rejects a deliberately broken glTF with a useful message
- [ ] HOUSE-00187 — `tools/assets/scale_check.py`: assert an asset's bounds against its category table (`cna-house.md` §70.5)
      dep: HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00188 — `tools/assets/origin_check.py`: assert the origin is at the support point (or the wall plane for wall-mounted) within 2 cm
      dep: HOUSE-00187 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00189 — `tools/blender/lod_gen.py`: decimate to LOD1/LOD2 with normal transfer, UV preservation and a triangle-budget target
      dep: HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
      accept: LOD1 ≈ 35 %, LOD2 ≈ 12 % of LOD0 triangles; silhouette error under a stated threshold
- [ ] HOUSE-00190 — `tools/blender/collision_proxy.py`: generate `<name>_COL` as a box or convex decomposition, ≤ 64 triangles
      dep: HOUSE-00189 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00191 — `tools/assets/pbr_to_stock.py`: metallic-roughness → `DiffuseColor`/`SpecularColor`/`SpecularPower` with a fixed documented mapping
      dep: HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
      accept: the mapping is written down in `docs/content-authoring.md` and is reversible enough to review
- [ ] HOUSE-00192 — `tools/assets/atlas_pack.py`: pack small-prop textures into shared atlases and rewrite the models' UVs
      dep: HOUSE-00191 · sys: content · plat: TOOL · pri: SHOULD
- [ ] HOUSE-00193 — `tools/assets/convert_audio.py`: 24→16-bit, 48→44.1 kHz, trim, normalise, loop points, and the dull-variant filter
      dep: HOUSE-00069 · sys: content · plat: TOOL · pri: MUST
      accept: deterministic output; both hashes recorded in the manifest
- [ ] HOUSE-00194 — `tools/assets/measure_stride.py`: measure a locomotion clip's stride length and duration for rate matching
      dep: HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00195 — `tools/assets/manifest.py`: add/update a manifest row, compute hashes, validate the schema
      dep: HOUSE-00181 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00196 — `tools/ci/check_manifest.py`: no unlisted file under `assets-src/`, no hash mismatch
      dep: HOUSE-00195 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00197 — `tools/assets/verify_licences.py`: every row has a licence, a licence file and the redistribution booleans; packaging refuses `PROVENANCE UNKNOWN`
      dep: HOUSE-00195 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00198 — `licenses/THIRD-PARTY-ASSETS.md` generator from the manifest
      dep: HOUSE-00197 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00199 — `make content-verify`: rebuild everything and assert byte-identical output
      dep: HOUSE-00182 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00200 — Author the two UI fonts as `.spritefont` (UI face at 16/22/30, mono at 13/16) and verify glyph coverage for the languages we ship (English only, but with the full Latin-1 set)
      dep: HOUSE-00066 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00201 — Content smoke scene: load one model, one texture, one font, one sound, one effect, one video and display/play them
      dep: HOUSE-00182…HOUSE-00200 · sys: content · plat: LNX · pri: MUST
      verify: render test `content-smoke-01`
- [ ] HOUSE-00202 — Define the pack partition in `assets.manifest.json` and enforce that every asset belongs to exactly one pack
      dep: HOUSE-00195 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00203 — `tools/ci/budget_report.py`: per-pack size, texture memory, triangle totals, audio duration; writes a Markdown table
      dep: HOUSE-00202 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00204 — `tools/blender/impostor_render.py`: render 8-yaw impostor atlases with EEVEE, pack, and emit the metadata
      dep: HOUSE-00189 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00205 — `tools/blender/lightmap_unwrap.py`: second-UV atlas packing at a configurable texel density with a 4-texel gutter
      dep: HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00206 — `tools/blender/lightmap_bake.py`: per-cell, per-light-group diffuse+indirect bake with denoising, plus the daylight bake
      dep: HOUSE-00205 · sys: content · plat: TOOL · pri: MUST
      accept: deterministic given a seed; a shell-geometry hash is embedded so a stale bake is detected
- [ ] HOUSE-00207 — `tools/blender/shading_factor.py`: precompute per-window sun shading on a 12×24 (altitude, azimuth) grid by ray casting
      dep: HOUSE-00206 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00208 — `tools/blender/sun_patch.py`: precompute the sun-patch polygons cast through each window onto floors and walls, same grid
      dep: HOUSE-00207 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00209 — `tools/blender/cubemap_bake.py`: bake the 4 mirror cube maps
      dep: HOUSE-00206 · sys: content · plat: TOOL · pri: SHOULD
- [ ] HOUSE-00210 — `tools/world/build_collision.py`: layout + `_COL` proxies → `content/world/collision.bin`
      dep: HOUSE-00190 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00211 — `tools/world/build_nav.py`: layout → the pet waypoint graph with perches
      dep: HOUSE-00210 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00212 — `tools/world/build_coverage.py`: the rain/roof coverage height field on a 0.5 m grid
      dep: HOUSE-00210 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00213 — `tools/world/build_skyexposure.py`: per-cell sky exposure and per-orientation facade exposure by ray casting
      dep: HOUSE-00212 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00214 — `tools/world/build_snowshell.py`: generate the snow-shell meshes from up-facing exterior surfaces, respecting per-material slope limits
      dep: HOUSE-00212 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00215 — `tools/world/build_chunks.py`: batch per-cell static props into ≤ 6 chunks by (effect, material, light groups, alpha mode), pre-transformed to world space, with per-sub-range bounds
      dep: HOUSE-00210 · sys: content · plat: TOOL · pri: MUST
      accept: ≤ 6 chunks per cell; ≤ 65 535 vertices per chunk where 16-bit indices are used
- [ ] HOUSE-00216 — Content build orchestration: one `make content` target running validators, generators and `cna-content` in the right order with dependency tracking
      dep: HOUSE-00205…HOUSE-00215 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00217 — Add the content-build documentation: what each tool does, in what order, and how to rebuild one asset
      dep: HOUSE-00216 · sys: — · plat: TOOL · pri: MUST
- [ ] HOUSE-00218 — Measure and record a full content build time; set the expectation for later sessions
      dep: HOUSE-00216 · sys: — · plat: TOOL · pri: SHOULD
- [ ] HOUSE-00219 — `tools/assets/video_transcode.py`: transcode source footage to the runtime video format and to the frame-strip atlases
      dep: HOUSE-00098 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00220 — `tools/assets/reverb_variants.py`: produce the dry/small/large pre-reverberated variants for the 30 transient sounds
      dep: HOUSE-00193 · sys: content · plat: TOOL · pri: SHOULD
- [ ] HOUSE-00221 — `tools/assets/dull_variants.py`: produce the low-passed "dull" variants for the 22 muffle-critical sounds
      dep: HOUSE-00193 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00223 — `tools/assets/anim_extract.py`: read a source `.glb` and write its `.chanim` sidecar — skeleton in skin-joint order, bind and inverse-bind poses, every clip as per-bone TRS keyframe tracks
      dep: HOUSE-00166, HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
      files: tools/assets/anim_extract.py
      accept: (1) deterministic output, hash recorded in the manifest; (2) keyframes decimated only within a stated tolerance; (3) stride length (`measure_stride.py`) and foot-plant markers written into the same file
- [ ] HOUSE-00224 — `tools/assets/skin_split.py`: split a multi-skin source `.glb` into one file per skin, record each part's attachment bone in the manifest, and extend `gltf_validate.py` to **reject** any `.glb` entering the build that declares more than one skin
      dep: HOUSE-00076, HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
      accept: (1) the split parts render identically to the source; (2) a two-skin file fails validation with a message naming both skins and pointing at the splitter; (3) the rule is documented in `docs/content-authoring.md`
- [ ] HOUSE-00225 — `tools/ci/check_anim_assets.py`: for every character, assert the `.chanim` joint list matches the compiled `.cnb` `Model::Bones` name-for-name and in order, and that no source `.glb` declares more than one skin
      dep: HOUSE-00223, HOUSE-00224 · sys: ci · plat: CI · pri: MUST
      accept: a deliberately renamed joint and a deliberately reintroduced second skin both fail the content build
- [ ] HOUSE-00222 — Phase-3 review and commit; run `budget_report.py` for the first time
      dep: HOUSE-00181…HOUSE-00225 · sys: — · plat: ALL · pri: MUST

---

## Phase 4 — Asset provenance and licensing

**Goal.** Before a single downloaded asset enters the repository, the machinery that keeps it
legal exists and works; and the licences of every candidate source are actually read.

**Exit.** Every source in `cna-house.md` §19.2 has a verified licence record; the NOX collection
is imported and manifested; the hero-asset research tasks have concrete answers.

- [ ] HOUSE-00261 — Establish `assets-src/` provenance discipline: a `SOURCE.md` per downloaded asset directory recording where it came from
      dep: HOUSE-00195 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00262 — Verify the **Poly Haven** licence; archive the licence page and terms; record redistribution and modification rights
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00263 — Verify the **ambientCG** licence, same treatment
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00264 — Verify the **Khronos glTF-Sample-Assets** per-asset licences and record which are usable
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00265 — Verify **Quaternius**, **Kenney**, **Poly Pizza** licences
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00266 — Verify **BlenderKit free tier** licensing model and whether per-asset licences are machine-readable
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: SHOULD
- [ ] HOUSE-00267 — Verify **Sketchfab CC0** filter semantics: does the filter guarantee CC0, and what evidence do we archive per asset?
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00268 — Verify **Blend Swap CC0** the same way
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: SHOULD
- [ ] HOUSE-00269 — **Verify the MakeHuman / MPFB2 asset licence** (Q-03): may generated meshes be redistributed, and under what terms?
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
      accept: a written answer with the licence text archived; if negative, R-04's fallback is triggered here, not later
- [ ] HOUSE-00270 — **Verify the CMU Motion Capture Database terms** (Q-04): may retargeted, baked derivatives be redistributed?
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00271 — Verify **Mixamo** terms and record the verdict; expected outcome is "not used for shipped files"
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: SHOULD
- [ ] HOUSE-00272 — Verify **Freesound CC0** filter semantics and the per-sound evidence we must archive
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00273 — Verify the **font** licences (OFL) and archive them
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00274 — Verify the **lunar albedo map** and **star catalogue** provenance (public domain expected)
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00275 — Verify the **video footage** sources for the television channels
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00276 — **Q-02: re-verify the NOX_SOUND Essentials Series CC0 declaration** against the publisher's live page; archive the evidence
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
      accept: either confirmed (mark `CC0-1.0`, verified, with the archived page) or not (mark `PROVENANCE DECLARED — UNVERIFIED` and do not ship until resolved)
- [ ] HOUSE-00277 — Select the NOX subset to ship: ~430 files, listed explicitly with the category each serves
      dep: HOUSE-00276 · sys: audio · plat: TOOL · pri: MUST
      accept: the exclusion of the Azores flows, the combat voices and the truck pack is explicit
- [ ] HOUSE-00278 — Convert the selected NOX subset with `convert_audio.py`; verify a listening check on 10 files (R-17)
      dep: HOUSE-00277, HOUSE-00193 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-00279 — Manifest every converted NOX file with both hashes, duration, channels and its target category
      dep: HOUSE-00278 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-00280 — Map the NOX footstep packs onto the 20 game surfaces; record which 8 surfaces are unserved
      dep: HOUSE-00279 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-00281 — Grouped: source the 8 missing footstep surfaces (carpet, concrete, interior hardwood, 3 stair variants, asphalt, bluestone) — ≥ 6 walk + 6 run + 2 land each, CC0
      dep: HOUSE-00272, HOUSE-00280 · sys: audio · plat: TOOL · pri: MUST
      accept: 8 surfaces × ≥ 14 samples, manifested, converted, auditioned
- [ ] HOUSE-00282 — Grouped: source door sounds — 4 door classes × {open, close, latch, creak ×3, slam}, CC0
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-00283 — Grouped: source switch, cabinet and drawer sounds — ≥ 24 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-00284 — Grouped: source water sounds — tap ×3 flow rates, shower, drain gurgle, water hammer, pipe hiss, bath fill, toilet flush ×2, cistern refill — ≥ 16 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-00285 — Grouped: source appliance sounds — fridge compressor start/run/stop, freezer, washer ×3 stages, dryer, dishwasher ×3 stages, oven fan, extractor ×3, microwave, kettle, toaster — ≥ 22 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-00286 — Grouped: source weather sounds NOX lacks — thunder ×3 distances (≥ 9 samples), hail on roof/window/car, rain on roof/window, gutter trickle — ≥ 18 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-00287 — Grouped: source dog sounds — bark ×4, whine, growl, pant loop, sigh, eat, drink, collar jingle, nail clicks — ≥ 14 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-00288 — Grouped: source cat sounds — meow ×5, chirrup, purr loop, hiss, paw falls, scratch — ≥ 12 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-00289 — Grouped: source house sounds — HVAC burner/blower/duct tick, clock tick, clock chime, doorbell, garage motor, gate motor, creaks ×8 — ≥ 18 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-00290 — Grouped: source outdoor ambience NOX lacks — distant road, suburban day, suburban night, lawnmower, aircraft, neighbourhood dog — ≥ 10 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-00291 — **Hero asset research: the dog.** Three sourcing attempts with recorded results; if none passes the bar, schedule the build-it-ourselves fallback (R-01)
      dep: HOUSE-00267, HOUSE-00268 · sys: content · plat: TOOL · pri: MUST
      accept: a decision with evidence: chosen asset + licence, or "build it" with a task list
- [ ] HOUSE-00292 — **Hero asset research: the cat.** Same (R-02)
      dep: HOUSE-00291 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00293 — **Hero asset research: the car.** Same (R-03)
      dep: HOUSE-00291 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00294 — **Hero asset research: the human base meshes.** Produce two MakeHuman bodies, evaluate topology and silhouette, and sign off (R-04)
      dep: HOUSE-00269 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00295 — **Hero asset research: locomotion clips.** Retarget 6 CMU clips onto the base rig and evaluate quality (R-05)
      dep: HOUSE-00270, HOUSE-00294 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00296 — Grouped: acquire the base material set from ambientCG/Poly Haven — 34 PBR materials (paint ×6, wood ×6, tile ×4, carpet ×3, stone ×3, brick, plaster, concrete, asphalt, gravel, grass, soil, fabric ×3, metal ×2)
      dep: HOUSE-00262, HOUSE-00263 · sys: content · plat: TOOL · pri: MUST
      accept: each manifested, resized to budget, converted, and previewed on a test sphere and a test floor
- [ ] HOUSE-00297 — Grouped: acquire the vegetation set — 6 tree species × 3 ages, 9 shrubs, 5 flowers, 2 grass card sets
      dep: HOUSE-00262 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00298 — Set up `docs/asset-review/` sign-offs for the four hero assets
      dep: HOUSE-00040, HOUSE-00291…HOUSE-00295 · sys: — · plat: TOOL · pri: MUST
- [ ] HOUSE-00299 — Run `verify_licences.py` over everything acquired so far; fix every gap
      dep: HOUSE-00296, HOUSE-00297 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00300 — Generate and review `licenses/THIRD-PARTY-ASSETS.md`
      dep: HOUSE-00299 · sys: — · plat: TOOL · pri: MUST
- [ ] HOUSE-00301 — Establish the asset-acquisition runbook so later sessions add assets consistently
      dep: HOUSE-00300 · sys: — · plat: TOOL · pri: MUST
- [ ] HOUSE-00302 — Phase-4 review and commit; update `cna-house.md` §19/§20 with what was actually found
      dep: HOUSE-00261…HOUSE-00301 · sys: — · plat: ALL · pri: MUST

---

## Phase 5 — World and floor-plan data

**Goal.** The house exists as data: every level, cell, portal, opening, stair, light, material,
prop placement, nav node, audio zone and interactable, authored, validated and loadable.

**Exit.** `validate_world.py` and the C++ validator both pass on the complete layout; the game
loads it in under 250 ms; `report_graph.py` produces the adjacency tables of `cna-house.md` §16.

### 5.1 Schema and loader

- [ ] HOUSE-00341 — Define and document the JSON schema files for all 16 world files (`docs/world-format.md` + machine-checkable JSON Schema)
      dep: HOUSE-00033 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00342 — Implement `WorldData`: the immutable in-memory model (levels, cells, portals, openings, stairs, lights, materials, props, nav, audio, exterior)
      dep: HOUSE-00341, HOUSE-00028 · sys: world · plat: ALL · pri: MUST
      files: src/world/WorldData.cpp|hpp
- [ ] HOUSE-00343 — Implement `WorldLoader` for `world.manifest.json` + `layout.levels.json`
      dep: HOUSE-00342 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00344 — `WorldLoader`: cells, including multi-box cells and the `yOverride` case
      dep: HOUSE-00343 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00345 — `WorldLoader`: portals, including plane/rect validation against both cells
      dep: HOUSE-00344 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00346 — `WorldLoader`: openings (doors, windows) with hinge/swing/travel metadata
      dep: HOUSE-00345 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00347 — `WorldLoader`: stairs, with the derived collision ramp parameters
      dep: HOUSE-00344 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00348 — `WorldLoader`: lights and light groups
      dep: HOUSE-00344 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00349 — `WorldLoader`: materials
      dep: HOUSE-00343 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00350 — `WorldLoader`: prop placements with LOD group, collision reference and interactable reference
      dep: HOUSE-00349 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00351 — `WorldLoader`: nav graph, perches, beds, forbidden zones
      dep: HOUSE-00344 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00352 — `WorldLoader`: audio zones, ambience beds, emitter placements, portal transmission losses
      dep: HOUSE-00345 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00353 — `WorldLoader`: exterior (terrain reference, road, fences, neighbourhood, vegetation instances)
      dep: HOUSE-00344 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00354 — `WorldLoader`: interactables, with the closed-vocabulary predicate/effect parser
      dep: HOUSE-00350 · sys: world · plat: ALL · pri: MUST
      accept: an unknown token is a load-time error naming the file, the id and the token
      verify: unit InteractableExprTests.* with 20 valid and 20 invalid expressions
- [ ] HOUSE-00355 — `WorldLoader`: `initialstate.json` into the canonical state table
      dep: HOUSE-00354 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00356 — Implement `SpatialIndex`: the 2 m × 2 m × level grid over cells, built at load
      dep: HOUSE-00344 · sys: world · plat: ALL · pri: MUST
      verify: unit SpatialIndexTests.* against brute force on 10⁵ random points
- [ ] HOUSE-00357 — Implement `WorldValidator` in C++ mirroring `validate_world.py`'s 11 rules, run at load in debug builds
      dep: HOUSE-00355 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00358 — Implement `tools/world/validate_world.py` with all 11 rules and clear diagnostics
      dep: HOUSE-00341 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00359 — Implement `tools/world/report_graph.py`: adjacency tables, degree stats, diameter, component count with all doors closed
      dep: HOUSE-00358 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00360 — Implement the realism checks of `cna-house.md` §70.5 inside `validate_world.py`
      dep: HOUSE-00358 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00361 — Implement the reachability proof (rule 11): every interactable's focus point reachable by a 2.5 m ray from a standing eye on its room's floor
      dep: HOUSE-00360 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00362 — Implement the capsule-clearance proof: the player capsule fits through all 186 portals, or the portal is marked `crouch`
      dep: HOUSE-00360 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00363 — Wire `validate_world.py` into the content build and CI
      dep: HOUSE-00358 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00364 — Implement `world.manifest.json` hashing and the `worldHash` used by the save system
      dep: HOUSE-00343 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00365 — Measure world load time; assert < 250 ms
      dep: HOUSE-00357 · sys: world · plat: LNX · pri: MUST

### 5.2 Authoring the layout

- [ ] HOUSE-00366 — Author `layout.levels.json`: 5 levels, elevations and the construction constants of `cna-house.md` §12.2/§12.3
      dep: HOUSE-00343 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00367 — Author `layout.cells.json` for `B1`: 14 cells per §13.2, with materials, footstep surfaces, acoustics, light groups and residency
      dep: HOUSE-00366 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00368 — Author `layout.cells.json` for `L0`: 19 cells per §13.3
      dep: HOUSE-00366 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00369 — Author `layout.cells.json` for `L1`: 21 cells per §13.4
      dep: HOUSE-00366 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00370 — Author `layout.cells.json` for `L2`: 18 cells per §13.5
      dep: HOUSE-00366 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00371 — Author `layout.cells.json` for `L3`: 6 cells per §13.6, with the rafter-envelope description
      dep: HOUSE-00366 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00372 — Author the 17 exterior cells per §13.7, including `EXT_WORLD` and the 4 attached cells
      dep: HOUSE-00366 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00373 — Author the 2 container sub-cells for the refrigerator and freezer interiors, and the pattern for future ones
      dep: HOUSE-00368 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00374 — Author `layout.portals.json` — the 41 always-open portals (cased openings, stair wells)
      dep: HOUSE-00367…HOUSE-00372 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00375 — Author the 62 door portals with hinge, swing and opacity
      dep: HOUSE-00374 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00376 — Author the 81 window portals (62 openable + 19 fixed) with `opacity: glass` and `maxDepth`
      dep: HOUSE-00374 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00377 — Author the garage-door portal and the 1 hatch portal
      dep: HOUSE-00374 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00378 — Author `layout.openings.json`: the 7 door types and 10 window types with leaf sizes, frames and hardware references
      dep: HOUSE-00375, HOUSE-00376 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00379 — Author `layout.stairs.json`: the 7 flights of §12.4 with risers, goings, landings and surfaces
      dep: HOUSE-00366 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00380 — Author the window schedule per façade per level (§12.6), and cross-check counts against the portals file
      dep: HOUSE-00376 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00381 — Author `layout.lights.json` for `B1` and `L0`: fixtures, groups, colour temperatures, ranges, defaults
      dep: HOUSE-00368 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00382 — Author `layout.lights.json` for `L1`, `L2`, `L3`
      dep: HOUSE-00381 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00383 — Author the exterior lights: porch, garage flood, terrace, path, shed, plus the street and neighbour lights
      dep: HOUSE-00381 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00384 — Author the switch plates: position, gang count, group mapping, including the two three-way pairs
      dep: HOUSE-00382 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00385 — Author `layout.materials.json`: the material class table of §22.2 with all fields
      dep: HOUSE-00296 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00386 — Author the plumbing stack description (STACK-A…F) as data, so the validator can check fixture placement
      dep: HOUSE-00368 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00387 — Author the HVAC duct/register description as data, for audio placement
      dep: HOUSE-00386 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00388 — Author `layout.audio.json`: per-cell room tone, absorption, reverb hint; per-portal transmission losses (§64.3)
      dep: HOUSE-00367…HOUSE-00372 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00389 — Author `layout.nav.json`: ~300 nodes, edges, 22 perches, 2 beds, 2 bowls, forbidden zones
      dep: HOUSE-00367…HOUSE-00372 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00390 — Author `layout.exterior.json`: terrain reference, road segments, sidewalks, kerbs, driveway, paths, fences, gates, shed
      dep: HOUSE-00372 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00391 — Author the neighbourhood instance list: N1–N60 with positions, LOD class and material palette
      dep: HOUSE-00390 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00392 — Author the vegetation instance list: 34 street trees, property trees, shrubs, hedges, flower beds
      dep: HOUSE-00390 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00393 — Author `layout.weather.json`: the 14 archetypes, 4 seasonal transition matrices, dwell and transition distributions, rate limits
      dep: HOUSE-00341 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00394 — Author `layout.sky.json`: the zenith and horizon LUTs, cloud layer definitions, the sun/moon colour LUTs, the star catalogue reference
      dep: HOUSE-00341 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00395 — Author `initialstate.json` exactly as `cna-house.md` §65.6 specifies
      dep: HOUSE-00355 · sys: world · plat: TOOL · pri: MUST
      accept: every field in §65.6 is present; the three named open doors and the one open window are explicit
- [ ] HOUSE-00396 — Run `report_graph.py` and reconcile its output with `cna-house.md` §16.3; fix whichever is wrong
      dep: HOUSE-00374…HOUSE-00377 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00397 — Run the full validator; fix every violation; record the first clean run
      dep: HOUSE-00396, HOUSE-00360…HOUSE-00362 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00398 — Generate the printable floor plans (SVG per level) from the layout for review and for the docs
      dep: HOUSE-00397 · sys: world · plat: TOOL · pri: SHOULD
      files: tools/world/plan_svg.py, docs/plans/*.svg
- [ ] HOUSE-00399 — Author the ID golden list (every cell, portal, light group, interactable, material, asset) for the stability test
      dep: HOUSE-00397 · sys: world · plat: CI · pri: MUST
- [ ] HOUSE-00400 — Phase-5 review: walk the plan on paper against the room schedule and the plumbing diagram; correct `cna-house.md` if the data disagrees
      dep: HOUSE-00397 · sys: — · plat: ALL · pri: MUST

### 5.3 Interactable data authoring (placement only; behaviour comes in phase 14)

- [ ] HOUSE-00401 — Author `interactables.json` rows for the 62 doors
      dep: HOUSE-00375 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00402 — Author rows for the 62 openable windows
      dep: HOUSE-00376 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00403 — Author rows for the 84 light groups and their switch plates
      dep: HOUSE-00384 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00404 — Author rows for the 62 kitchen containers
      dep: HOUSE-00368 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00405 — Author rows for the 18 butler's/pantry containers
      dep: HOUSE-00404 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00406 — Author rows for the 34 bathroom and WC containers
      dep: HOUSE-00404 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00407 — Author rows for the 22 bedroom containers
      dep: HOUSE-00404 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00408 — Author rows for the 16 laundry/mudroom containers
      dep: HOUSE-00404 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00409 — Author rows for the 28 garage/workshop containers
      dep: HOUSE-00404 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00410 — Author rows for the 20 living/dining/office containers
      dep: HOUSE-00404 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00411 — Author rows for the 14 basement/attic containers
      dep: HOUSE-00404 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00412 — Author rows for the 17 water outlets, with their basins, plugs and stacks
      dep: HOUSE-00386 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00413 — Author rows for the 13 toilets
      dep: HOUSE-00386 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00414 — Author rows for the 3 televisions
      dep: HOUSE-00368 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00415 — Author rows for the 11 appliances
      dep: HOUSE-00368 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00416 — Author rows for the garage door, the 3 gates and their motors
      dep: HOUSE-00377 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00417 — Author rows for the 148 pickup items, including the 32 fridge/freezer items, with their home surfaces
      dep: HOUSE-00404 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00418 — Author rows for the 22 seats
      dep: HOUSE-00368 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00419 — Author the `placeable` surface list (tables, counters, shelves, floors) used when putting an item down
      dep: HOUSE-00417 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00420 — Validate the complete interactable set: 640 ± 20 rows, all reachable, all with sounds and persistence fields
      dep: HOUSE-00401…HOUSE-00419 · sys: world · plat: TOOL · pri: MUST

---

## Phase 6 — Blockout house geometry

**Goal.** Turn the layout data into real geometry: the generated architectural shell, correctly
scaled, correctly wound, chunked per cell, rendered with placeholder materials.

**Exit.** You can look at the house from the road and recognise it; every room's shell exists;
the chunk builder produces ≤ 6 chunks per cell.

- [ ] HOUSE-00451 — `house_shell_gen.py` skeleton: read the layout, open Blender headless, emit `.glb` per cell
      dep: HOUSE-00397, HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00452 — Generate floors and ceilings per cell, with the partition-centre-line inset
      dep: HOUSE-00451 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00453 — Generate interior partitions between adjacent cells, deduplicated so a shared wall is generated once
      dep: HOUSE-00452 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00454 — Generate exterior walls with the correct thickness, and the foundation walls
      dep: HOUSE-00453 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00455 — Cut door and window openings from walls, with reveals and sills
      dep: HOUSE-00454, HOUSE-00378 · sys: content · plat: TOOL · pri: MUST
      accept: every opening in `layout.openings.json` produces a hole of the right size in the right wall
- [ ] HOUSE-00456 — Generate door frames, architraves and thresholds
      dep: HOUSE-00455 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00457 — Generate window frames, sashes, sills, glazing bars and the glass quad
      dep: HOUSE-00455 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00458 — Generate skirtings and cornices per cell, mitred at corners and interrupted at openings
      dep: HOUSE-00456 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00459 — Generate stair carriages, treads, risers, nosings and landings for the 7 flights
      dep: HOUSE-00379 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00460 — Generate balustrades, newels, handrails and the stairwell openings in the floors above
      dep: HOUSE-00459 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00461 — Generate the roof: hipped-and-gabled planes at 7:12, ridge, hips, valleys, eaves, soffits, fascias
      dep: HOUSE-00454 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00462 — Generate the 5 dormers with their own roofs, cheeks and windows
      dep: HOUSE-00461 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00463 — Generate the attic knee walls, collar-tie ceiling, rafters, purlins and the walkway boarding
      dep: HOUSE-00461 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00464 — Generate the front porch: deck, columns, beam, roof, steps, balustrade
      dep: HOUSE-00454 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00465 — Generate the rear extension (sunroom) shell and its flat roof / balcony deck with parapet and railing
      dep: HOUSE-00454 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00466 — Generate the front balcony over the porch and the juliet balcony
      dep: HOUSE-00464 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00467 — Generate the garage wing shell, its slab, its loft platform and the sectional-door opening
      dep: HOUSE-00454 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00468 — Generate the chimney, the gutters, the downspouts and the roof vents
      dep: HOUSE-00461 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00469 — Generate the basement window wells
      dep: HOUSE-00454 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00470 — Assign placeholder materials per surface class so the blockout is readable
      dep: HOUSE-00452…HOUSE-00469 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00471 — Generate second-UV lightmap coordinates for the whole shell (`lightmap_unwrap.py`)
      dep: HOUSE-00470, HOUSE-00205 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00472 — Generate `_COL` collision proxies for the shell: wall/floor/ceiling OBBs and the stair ramps
      dep: HOUSE-00470, HOUSE-00190 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00473 — Run `build_chunks.py` over the shell; verify ≤ 6 chunks per cell and the vertex limits
      dep: HOUSE-00472, HOUSE-00215 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00474 — Implement `CellRuntime` and load the per-cell chunk buffers into `VertexBuffer`/`IndexBuffer`
      dep: HOUSE-00473, HOUSE-00344 · sys: world · plat: ALL · pri: MUST
- [ ] HOUSE-00475 — Implement the opaque static pass drawing all cells with `BasicEffect` and a fixed camera; no culling yet
      dep: HOUSE-00474, HOUSE-00159 · sys: rendering · plat: ALL · pri: MUST
      verify: render test `blockout-01` — the house from the road
- [ ] HOUSE-00476 — Implement a free-fly debug camera to inspect the blockout
      dep: HOUSE-00475 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-00477 — Verify the shell against the realism checks: door heights, ceiling heights, stair geometry, headroom
      dep: HOUSE-00475, HOUSE-00360 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00478 — Verify winding and normals across the whole shell (no black facets, no inside-out rooms)
      dep: HOUSE-00475 · sys: rendering · plat: LNX · pri: MUST
      verify: a render test that draws the shell with a normal-visualisation material
- [ ] HOUSE-00479 — Measure the shell's triangle count per cell and per level against the budget
      dep: HOUSE-00475 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-00480 — Fix the inevitable geometry issues found by HOUSE-00477/78/79; iterate the generator, not the output
      dep: HOUSE-00477…HOUSE-00479 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00481 — Determinism check: two runs of the generator produce byte-identical `.glb`
      dep: HOUSE-00480 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00482 — Generate and commit the shell asset manifest rows (`origin.kind = generated`, generator version, seed)
      dep: HOUSE-00481, HOUSE-00195 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00483 — Render test suite for the blockout: 8 exterior and 12 interior poses
      dep: HOUSE-00475 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00484 — Phase-6 review: does it look like the house in `cna-house.md` §12? Correct whichever is wrong.
      dep: HOUSE-00483 · sys: — · plat: ALL · pri: MUST

---

## Phase 7 — Collision and player controller

**Goal.** Walk the whole blockout: every room, every flight, in and out of the house, without
falling through, sticking, or passing through anything.

**Exit.** The collision guarantee suite (HOUSE-00612…00618) is green; a 20-minute random-walk bot
never escapes and never penetrates.

- [ ] HOUSE-00541 — Implement the collision data format and `CollisionLoader` for `collision.bin`
      dep: HOUSE-00472, HOUSE-00210 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00542 — Implement the per-cell loose 1 m collision grid
      dep: HOUSE-00541 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00543 — Implement `Capsule` and the capsule-vs-OBB sweep
      dep: HOUSE-00541 · sys: physics · plat: ALL · pri: MUST
      verify: unit SweepTests.CapsuleObb against 40 analytic cases
- [ ] HOUSE-00544 — Implement capsule-vs-triangle sweep (for the stair ramps and terrain)
      dep: HOUSE-00543 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00545 — Implement sphere sweep (for the third-person camera)
      dep: HOUSE-00543 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00546 — Implement `RayCast` against OBBs, triangles and the terrain height field
      dep: HOUSE-00544 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00547 — Implement `Overlap` and depenetration (4 iterations, 0.02 m push-out)
      dep: HOUSE-00543 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00548 — Implement `GroundProbe`: downward sweep returning height, normal, surface kind, cell id
      dep: HOUSE-00546 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00549 — Implement the fixed-step accumulator at 1/120 s with a 4-step clamp
      dep: HOUSE-00139 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00550 — Implement collide-and-slide (3 iterations) with the slope limit
      dep: HOUSE-00547, HOUSE-00549 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00551 — Implement step-up (≤ 0.22 m) and step-down (≤ 0.45 m) assist
      dep: HOUSE-00550 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00552 — Implement gravity and landing detection with soft/hard thresholds
      dep: HOUSE-00550 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00553 — Implement the terrain height-field collider with bilinear sampling and a triangle test above 20°
      dep: HOUSE-00544 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00554 — Implement the dynamic-obstacle list per cell (doors, garage door, pets) and its per-frame refresh
      dep: HOUSE-00542 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00555 — Implement `PlayerController`: input → desired velocity → sweep → position, with the acceleration model
      dep: HOUSE-00551, HOUSE-00140 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-00556 — Implement the two walk speeds and the `Shift` toggle, with the settings persistence of D-09
      dep: HOUSE-00555, HOUSE-00131 · sys: player · plat: ALL · pri: MUST
      accept: 1.35 / 2.05 m/s measured over a 20 m run within 1 %
      verify: unit WalkSpeedTests.*
- [ ] HOUSE-00557 — Implement directional speed modifiers (backwards, strafe, stairs, crouch, snow, carrying)
      dep: HOUSE-00556 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-00558 — Implement the automatic attic crouch: headroom probe, capsule swap, eye height, speed
      dep: HOUSE-00557 · sys: player · plat: ALL · pri: MUST
      accept: entering `L3_STORE_W` under the knee wall crouches; standing up is automatic and never clips
- [ ] HOUSE-00559 — Implement cell tracking with 5 cm hysteresis and the `CellEntered` event
      dep: HOUSE-00555, HOUSE-00356 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-00560 — Implement the stair ramp surface handling: slope detection, `SurfaceKind::Stairs`, speed reduction
      dep: HOUSE-00553, HOUSE-00379 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00561 — Implement the eye-height critically-damped spring, stiffened on stairs
      dep: HOUSE-00560 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-00562 — Implement the debug physics overlay (`F9`): shapes, capsule, probes, sweeps
      dep: HOUSE-00147, HOUSE-00550 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-00563 — Implement `teleport <cellId>` and `noclip` console commands
      dep: HOUSE-00555 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-00564 — Implement the invisible playable-volume boundary and its escape counter
      dep: HOUSE-00555 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00565 — Implement the nudgeable-prop mini-physics (gravity, support plane, damping, push impulse) for the 12 named props
      dep: HOUSE-00554 · sys: physics · plat: ALL · pri: SHOULD
- [ ] HOUSE-00566 — Determinism: fixed-step replay test at 30/60/144 FPS producing an identical final position
      dep: HOUSE-00555 · sys: physics · plat: CI · pri: MUST
      verify: unit PhysicsDeterminismTests.*
- [ ] HOUSE-00612 — Guarantee test: the player cannot pass any closed door (all 62, both sides)
      dep: HOUSE-00554 · sys: physics · plat: CI · pri: MUST
- [ ] HOUSE-00613 — Guarantee test: the player cannot pass any wall, floor or ceiling (2 000 randomised pushes)
      dep: HOUSE-00550 · sys: physics · plat: CI · pri: MUST
- [ ] HOUSE-00614 — Guarantee test: the player cannot fall through any floor (2 000 randomised drops from 3 m)
      dep: HOUSE-00552 · sys: physics · plat: CI · pri: MUST
- [ ] HOUSE-00615 — Guarantee test: every flight is traversable in both directions, and every landing is reachable
      dep: HOUSE-00560 · sys: physics · plat: CI · pri: MUST
- [ ] HOUSE-00616 — Guarantee test: the player capsule fits through every open portal
      dep: HOUSE-00362 · sys: physics · plat: CI · pri: MUST
- [ ] HOUSE-00617 — Guarantee test: the player never ends a frame inside static geometry (checked every step of a scripted tour)
      dep: HOUSE-00547 · sys: physics · plat: CI · pri: MUST
- [ ] HOUSE-00618 — Guarantee test: a 20-minute seeded random walk never trips the boundary counter and never leaves the named cells
      dep: HOUSE-00564 · sys: physics · plat: CI · pri: MUST
- [ ] HOUSE-00619 — Measure physics cost against the budget (0.35 ms typical, 0.80 worst)
      dep: HOUSE-00555 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-00620 — Phase-7 review and commit
      dep: HOUSE-00541…HOUSE-00619 · sys: — · plat: ALL · pri: MUST

---

## Phase 8 — First-person camera

**Goal.** The camera the player will spend the whole game inside. It must feel right, which means
it must be tuned, not just implemented.

- [ ] HOUSE-00621 — Implement `FirstPersonCamera`: eye from the controller, view matrix, projection, near/far
      dep: HOUSE-00561 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-00622 — Implement mouse look: `Mouse::GetState` delta from the window centre, sensitivity, `Mouse::SetPosition` recentre
      dep: HOUSE-00621, HOUSE-00100 · sys: player · plat: ALL · pri: MUST
      accept: no drift over 10 000 frames; consistent at every frame rate
- [ ] HOUSE-00623 — Implement pitch clamping (±85°) and zero roll
      dep: HOUSE-00622 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-00624 — Implement cursor hiding, mouse capture, and the `Alt` release plus automatic release on focus loss and menus
      dep: HOUSE-00622 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-00625 — Implement sensitivity, invert-Y and optional 2-frame smoothing as settings
      dep: HOUSE-00622, HOUSE-00131 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-00626 — Implement the FOV setting and its aspect handling
      dep: HOUSE-00621 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-00627 — Implement head bob (vertical + lateral) tied to the footstep cadence, with the three settings levels
      dep: HOUSE-00621, HOUSE-00556 · sys: player · plat: ALL · pri: MUST
      accept: at "Subtle" the amplitude is ≤ 0.012 m and no tester reports nausea
- [ ] HOUSE-00628 — Implement the near-surface eye pull-back to prevent near-plane clipping
      dep: HOUSE-00621, HOUSE-00546 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-00629 — Implement the landing camera dip and its recovery
      dep: HOUSE-00552, HOUSE-00621 · sys: player · plat: ALL · pri: SHOULD
- [ ] HOUSE-00630 — Implement the `BoundingFrustum` construction from the camera each frame
      dep: HOUSE-00621, HOUSE-00104 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00631 — Implement the `F2` world overlay (cell, position, yaw/pitch, surface, target)
      dep: HOUSE-00621, HOUSE-00559 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-00632 — Tune pass: walk every room and every flight and adjust bob, spring, FOV and step assist until it feels right; record the final numbers
      dep: HOUSE-00627 · sys: player · plat: LNX · pri: MUST
- [ ] HOUSE-00633 — Render tests: 12 first-person poses across the house
      dep: HOUSE-00632 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00634 — Phase-8 review and commit
      dep: HOUSE-00621…HOUSE-00633 · sys: — · plat: ALL · pri: MUST

---

## Phase 9 — Room/portal visibility

**Goal.** The core architectural system. Everything about the project's viability rests here.

**Exit.** All the culling tests are green; the visible-cell counts meet budget in all 10
performance scenarios; the `F4`/`F5` overlays exist and are useful.

### 9.1 The traversal

- [ ] HOUSE-00661 — Implement `ClipFrustum`: an N-plane frustum (N ≤ 10) over `Plane`, with `Intersects(BoundingBox)` and `Intersects(BoundingSphere)`
      dep: HOUSE-00630 · sys: visibility · plat: ALL · pri: MUST
      verify: unit ClipFrustumTests.* against `BoundingFrustum` for the 6-plane case
- [ ] HOUSE-00662 — Implement `ClipRectToFrustum`: 2-D Sutherland–Hodgman of an axis-aligned portal rectangle against a frustum's side planes
      dep: HOUSE-00661 · sys: visibility · plat: ALL · pri: MUST
      verify: unit ClipTests.* incl. fully inside, fully outside, straddling every plane, and degenerate cases
- [ ] HOUSE-00663 — Implement `ReduceFrustum`: build side planes from the camera and the clipped polygon's edges
      dep: HOUSE-00662 · sys: visibility · plat: ALL · pri: MUST
      accept: **the reduced frustum contains every point the portal can see and no point outside the parent** — proved by 10⁵ random samples
      verify: unit ReduceFrustumTests.ContainmentProperty
- [ ] HOUSE-00664 — Implement the NDC-area computation of a clipped polygon and the `kMinPortalNdcArea` cutoff
      dep: HOUSE-00663 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00665 — Implement `PortalRuntime`: per-portal aperture, cached world rect, opacity, and the closed/open hysteresis
      dep: HOUSE-00345 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00666 — Implement the portal back-face test (portal plane vs. camera side)
      dep: HOUSE-00665 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00667 — Implement `maxDepthFor(portal)` with the interior/exterior asymmetry table
      dep: HOUSE-00665 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00668 — Implement `PortalTraversal`: the BFS with per-cell frustum lists, the `kMaxFrustaPerCell` cap and the containment skip
      dep: HOUSE-00663…HOUSE-00667 · sys: visibility · plat: ALL · pri: MUST
      files: src/visibility/PortalTraversal.cpp|hpp
- [ ] HOUSE-00669 — Implement the translucent-portal `DIFFUSE` flag and its effect on the target cell's detail set
      dep: HOUSE-00668 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00670 — Implement `VisibilitySystem`: camera cell → traversal → visible set, published once per frame
      dep: HOUSE-00668, HOUSE-00559 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00671 — Implement the `maxVisibleCells` hard stop with graceful degradation (drop the smallest-frustum cells first) and a counter
      dep: HOUSE-00670 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00672 — Implement per-cell chunk culling against the cell's frusta
      dep: HOUSE-00670, HOUSE-00474 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00673 — Implement per-cell dynamic-instance culling
      dep: HOUSE-00672 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00674 — Implement distance culling per prop category
      dep: HOUSE-00673 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00675 — Implement `RenderList`: the sorted draw list (pass → effect → material → chunk)
      dep: HOUSE-00674 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00676 — Wire `RenderList` into the opaque static pass, replacing the draw-everything path
      dep: HOUSE-00675, HOUSE-00475 · sys: rendering · plat: ALL · pri: MUST

### 9.2 Exterior visibility

- [ ] HOUSE-00677 — Implement the exterior loose BVH over `EXT_WORLD` instances, built at load
      dep: HOUSE-00674 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00678 — Implement BVH frustum traversal with the per-category distance cutoffs
      dep: HOUSE-00677 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00679 — Implement the indoor→outdoor portal traversal into `EXT_WORLD` with the reduced frustum
      dep: HOUSE-00678, HOUSE-00668 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00680 — Implement the outdoor→indoor traversal at depth 1 through windows
      dep: HOUSE-00679 · sys: visibility · plat: ALL · pri: MUST
      accept: standing in the garden, exactly one room is visible through each window, not the whole house

### 9.3 Instrumentation and proof

- [ ] HOUSE-00681 — Implement the `F3` visibility overlay (counts, traversals, portals tested, depth, the visible list)
      dep: HOUSE-00670 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-00682 — Implement the `F4` visibility geometry overlay (cell wireframes, portal quads, reduced frusta)
      dep: HOUSE-00681, HOUSE-00147 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-00683 — Implement `F5` freeze-visibility and the detached inspection camera
      dep: HOUSE-00682 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-00684 — Implement the `cull off|on` console command
      dep: HOUSE-00670 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-00685 — Author the 24 named visibility poses with their expected visible-cell sets
      dep: HOUSE-00681 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00686 — Test: exact visible-set assertion for all 24 poses
      dep: HOUSE-00685 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00687 — Test: the door-state matrix — for each of the 62 doors, open and close it and assert the visible set changes in the expected direction, from both sides
      dep: HOUSE-00686 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00688 — Test: **no over-culling** — render each of the 24 poses normally and with culling disabled and assert the images match within tolerance
      dep: HOUSE-00684, HOUSE-00164 · sys: ci · plat: CI · pri: MUST
      accept: this is the single most important test in the project; a failure means something visible was culled
- [ ] HOUSE-00689 — Test: with all doors closed, the graph fragments as `report_graph.py` predicts and the visible set from `L0_FOYER` is the golden list
      dep: HOUSE-00686 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00690 — Test: with all doors open, the visible-cell count from the 12 budget poses stays within budget
      dep: HOUSE-00686 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00691 — Test: the partially-open-door rule — a door at 0.02 culls, at 0.10 does not, and the leaf occludes inside the target cell
      dep: HOUSE-00687 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00692 — Test: window visibility from outside is capped at depth 1
      dep: HOUSE-00680 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00693 — Test: the stair well makes three floors visible from the foyer, and the basement/attic doors cut their levels off entirely
      dep: HOUSE-00686 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00694 — Measure visibility cost in all 10 performance scenarios against the 0.55/1.20 ms budget
      dep: HOUSE-00690 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-00695 — Optimise the traversal against the measurement: the portal-plane test, the frusta containment check, and the work-queue allocation
      dep: HOUSE-00694 · sys: visibility · plat: ALL · pri: MUST
      accept: no allocation in the steady state; the work queue is a fixed-capacity ring
- [ ] HOUSE-00696 — Write `docs/visibility.md`: the algorithm, its parameters, its guarantees, and how to debug it
      dep: HOUSE-00695 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00697 — Phase-9 review and commit; record the visible-cell and draw-call numbers in the performance log
      dep: HOUSE-00661…HOUSE-00696 · sys: — · plat: ALL · pri: MUST

---

## Phase 10 — Exterior and property

- [ ] HOUSE-00761 — Author the terrain height field (81 × 65 samples) with the slope described in §10.2, plus its material index map
      dep: HOUSE-00390 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00762 — `terrain_gen.py`: generate the 25 terrain tiles with skirts, per-tile bounds and lightmap UVs
      dep: HOUSE-00761 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00763 — Generate the road, kerbs, sidewalks, drain grates and the road markings
      dep: HOUSE-00762 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00764 — Generate the driveway, the apron and the connecting path
      dep: HOUSE-00762 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00765 — Generate the front walk, the terrace paving and the garden paths
      dep: HOUSE-00762 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00766 — `fence_gen.py`: the 1.85 m board fence (W/N/E), the 1.35 m ornamental front fence, posts, caps and rails
      dep: HOUSE-00762 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00767 — Generate the pedestrian gate, the sliding vehicle gate and the rear service gate with their hardware
      dep: HOUSE-00766 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00768 — Generate the garden shed (walls, roof, door, window, floor) as an enterable cell
      dep: HOUSE-00766 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00769 — Generate the raised vegetable beds, the trellis and the compost bin
      dep: HOUSE-00762 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00770 — Place the exterior props: mailbox, bins ×3, hose reel, AC condenser, gas meter, water tap, downspout splash blocks
      dep: HOUSE-00764 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00771 — Place the garden furniture: terrace table and chairs, two loungers, swing bench, fire pit, birdbath, planters
      dep: HOUSE-00765 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00772 — Place the vegetation: 34 street trees, 9 property trees, 4 fruit trees, 40 shrubs, hedges, flower beds, grass patches
      dep: HOUSE-00297, HOUSE-00392 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00773 — Implement grass-card rendering with `AlphaTestEffect` and per-instance jitter
      dep: HOUSE-00772, HOUSE-00080 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00774 — Build the exterior collision: terrain, fences, kerbs, walls, shed, tree trunks, vehicles
      dep: HOUSE-00553, HOUSE-00766 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-00775 — Implement the road-end barriers: hedge, stone wall, parked van, street trees, and the sign
      dep: HOUSE-00763 · sys: world · plat: TOOL · pri: MUST
      accept: the player is stopped by visible objects at x = ±35, never by an invisible wall
- [ ] HOUSE-00776 — Place the four downspouts, the gutters and their splash points (used by the rain audio)
      dep: HOUSE-00468 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00777 — Build the coverage height field (`build_coverage.py`) from the house, garage, porch, balconies, sunroom and shed
      dep: HOUSE-00212, HOUSE-00768 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00778 — Build the snow shells (`build_snowshell.py`) for terrain, roofs, decks, rails, furniture and the car
      dep: HOUSE-00214, HOUSE-00777 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00779 — Build the sky-exposure data (`build_skyexposure.py`) per cell and per facade
      dep: HOUSE-00213, HOUSE-00777 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00780 — Implement exterior chunking and residency for the `exterior` pack
      dep: HOUSE-00215, HOUSE-00762 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00781 — Render tests: 8 exterior poses covering the road, drive, front, side yards, terrace, garden and orchard
      dep: HOUSE-00780 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00782 — Test: the property is fully walkable and fully bounded (bot walk, boundary counter 0)
      dep: HOUSE-00774, HOUSE-00775 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00783 — Phase-10 review and commit
      dep: HOUSE-00761…HOUSE-00782 · sys: — · plat: ALL · pri: MUST

---

## Phase 11 — Neighbourhood background

- [ ] HOUSE-00841 — `neighbourhood_gen.py`: the parametric house grammar (footprint, storeys, roof type, garage, porch) with 8 material palettes
      dep: HOUSE-00391 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00842 — Generate the 2 adjacent houses (N1, N2) at LOD0 detail, with real windows, doors, drives and fences
      dep: HOUSE-00841 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00843 — Generate the 6 across-the-street houses (N3–N8) at LOD1
      dep: HOUSE-00841 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00844 — Generate the 16 further houses (N9–N24) at LOD2
      dep: HOUSE-00841 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00845 — Generate the 36 distant silhouettes (N25–N60) as impostor cards on the horizon ring
      dep: HOUSE-00204, HOUSE-00844 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00846 — Generate the street furniture: 9 street lights, 5 utility poles with catenary wires, 4 signs, 12 mailboxes, 2 bin clusters, a basketball hoop
      dep: HOUSE-00763 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00847 — Place the 3 parked neighbour cars and the delivery van
      dep: HOUSE-00293 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00848 — Generate the distant tree line, the ridge and the water tower on the horizon ring
      dep: HOUSE-00845 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00849 — Implement the neighbourhood window-glow cards for night (emissive quads behind the window openings)
      dep: HOUSE-00842 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00850 — Implement the neighbour porch lights and street lights on the dusk sensor with per-fixture offsets
      dep: HOUSE-00849 · sys: lighting · plat: ALL · pri: MUST
- [ ] HOUSE-00851 — Implement impostor rendering: yaw slice selection, sky tinting, vertical-axis billboarding for trees
      dep: HOUSE-00845, HOUSE-00204 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00852 — Wire the neighbourhood into the exterior BVH with the correct LOD distances
      dep: HOUSE-00851, HOUSE-00678 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00853 — Measure the neighbourhood's draw-call and triangle cost from the road pose against budget
      dep: HOUSE-00852 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-00854 — Render tests: 6 poses covering the neighbourhood at 3 LOD distances, day and night
      dep: HOUSE-00852 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00855 — Phase-11 review: does the property look like it belongs to a street? Correct if not.
      dep: HOUSE-00854 · sys: — · plat: ALL · pri: MUST

---

## Phase 12 — Materials and textures

- [ ] HOUSE-00891 — Implement `MaterialDef` loading and the material registry
      dep: HOUSE-00349, HOUSE-00385 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00892 — Implement `MaterialBinder` for `BasicEffect`: diffuse, specular, texture, vertex colour, alpha, fog
      dep: HOUSE-00891, HOUSE-00162 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00893 — Implement `MaterialBinder` for `DualTextureEffect`: albedo + lightmap + diffuse tint
      dep: HOUSE-00892, HOUSE-00078 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00894 — Implement `MaterialBinder` for `AlphaTestEffect`: cutoff, compare function, two-sided
      dep: HOUSE-00892, HOUSE-00080 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00895 — Implement `MaterialBinder` for `SkinnedEffect`
      dep: HOUSE-00892, HOUSE-00077 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00896 — Implement `MaterialBinder` for `EnvironmentMapEffect` with the baked cube maps
      dep: HOUSE-00892, HOUSE-00081 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-00897 — Implement the effect-instance pool: one instance per (effect class, material variant), cloned as needed, never allocated per draw
      dep: HOUSE-00895 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00898 — Implement the transparency pass with back-to-front sorting by cell then by object
      dep: HOUSE-00676 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00899 — Implement the alpha-test pass before transparency with full depth writes
      dep: HOUSE-00894 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00900 — Grouped: author the 34 base materials from the acquired PBR set, mapped through `pbr_to_stock.py`, previewed and tuned
      dep: HOUSE-00296, HOUSE-00891 · sys: content · plat: TOOL · pri: MUST
      accept: each material previewed on a sphere, a floor plane and a wall at three light levels
- [ ] HOUSE-00901 — Grouped: author the 18 interior paint variants (walls and ceilings per room palette)
      dep: HOUSE-00900 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00902 — Grouped: author the 12 floor materials (oak, walnut, 3 carpets, 4 tiles, concrete, stone, vinyl)
      dep: HOUSE-00900 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00903 — Grouped: author the 9 exterior materials (siding ×3, brick, roof shingle, soffit, concrete, asphalt, gravel)
      dep: HOUSE-00900 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00904 — Grouped: author the 8 glass and water materials
      dep: HOUSE-00900 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00905 — Grouped: author the 14 wet-variant materials for the outdoor surfaces
      dep: HOUSE-00903 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00906 — Grouped: author the 6 snow materials and the snow-shell material
      dep: HOUSE-00903 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00907 — Apply the real materials to the generated shell, replacing the placeholders, room by room per the palette table
      dep: HOUSE-00901, HOUSE-00902 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00908 — Author the room palette table (wall colour, ceiling colour, floor material, trim colour) for all 78 interior cells
      dep: HOUSE-00901 · sys: world · plat: TOOL · pri: MUST
      accept: the palette reads as one house decorated by one family, not 78 unrelated rooms
- [ ] HOUSE-00909 — Run the first lightmap bake over the whole shell (`lightmap_bake.py`) — artificial groups only
      dep: HOUSE-00206, HOUSE-00907 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00910 — Run the daylight bake per cell
      dep: HOUSE-00909 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00911 — Measure the lightmap atlas count, size and bake time; adjust texel density to fit the 60 MB budget
      dep: HOUSE-00910 · sys: — · plat: TOOL · pri: MUST
- [ ] HOUSE-00912 — Implement lightmap loading and binding, and the shell's `DualTextureEffect` draw path
      dep: HOUSE-00893, HOUSE-00910 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00913 — Fix lightmap seams and gutter bleed found on inspection
      dep: HOUSE-00912 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00914 — Implement the texture streaming-free residency for the `house-*` packs (load-at-start for now)
      dep: HOUSE-00142 · sys: content · plat: ALL · pri: MUST
- [ ] HOUSE-00915 — Measure texture memory against the 300 MB budget; adjust sizes where over
      dep: HOUSE-00914, HOUSE-00203 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-00916 — Implement anisotropic filtering selection from the project-owned effective feature set (`cna-house.md` §68): request `SamplerState::MaxAnisotropy` only on a build/platform profile validated to permit it (HOUSE-00109), and sample **trilinear** (`TextureFilter::Linear` with mips) everywhere else. No CNA-specific runtime capability query — the setting row is simply not offered on a profile that does not permit it.
      dep: HOUSE-00109 · sys: rendering · plat: ALL · pri: SHOULD
      accept: on a profile without anisotropy the renderer samples trilinear and the settings row is absent; `check_xna_only.py` proves no file calls `SupportsCapability`
- [ ] HOUSE-00917 — Render tests: a materials sheet scene plus 12 room poses at the new materials
      dep: HOUSE-00912 · sys: ci · plat: CI · pri: MUST
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
- [ ] HOUSE-00918 — Phase-12 review: does the house read as a real building yet?
      dep: HOUSE-00917 · sys: — · plat: ALL · pri: MUST

---

## Phase 13 — Static furniture and dressing

**Goal.** Furnish 78 interior cells and the exterior to the density of `cna-house.md` §59.1, from
≈ 420 unique models, without visible repetition.

**Method.** One task per cell or small group of cells. Each task places `essential`, `dressing`
and `micro` sets, respects the anti-repetition rules, and ends with a render-test pose.

- [ ] HOUSE-00971 — Implement the placement pipeline: a prop row in `layout.props.json` → a chunk entry or a dynamic instance, with per-instance jitter and tint
      dep: HOUSE-00215, HOUSE-00891 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00972 — Implement the anti-repetition validator (≤ 14 uses per model overall, ≤ 6 per cell, whitelist)
      dep: HOUSE-00971 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00973 — Implement the fill-kit generator: `FILL_CUTLERY`, `FILL_TOWELS`, `FILL_TOOLS`, `FILL_TSHIRTS`, `FILL_PAPERS`, `FILL_BOOKS`, `FILL_CROCKERY`, `FILL_TOYS`, `FILL_PAINT`, `FILL_JARS`, `FILL_SHOES`, `FILL_LINEN`
      dep: HOUSE-00971 · sys: content · plat: TOOL · pri: MUST
      accept: seeded, varied count/spacing/lean/colour; two invocations never look identical
- [ ] HOUSE-00974 — Implement the detail-set tagging (`essential`/`dressing`/`micro`) and its culling
      dep: HOUSE-00971, HOUSE-00674 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-00975 — Grouped: acquire and prepare the 24 kitchen appliance and fixture models
      dep: HOUSE-00296 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00976 — Grouped: acquire and prepare the 16 bathroom fixture models
      dep: HOUSE-00296 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00977 — Grouped: acquire and prepare the 20 seating models
      dep: HOUSE-00296 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00978 — Grouped: acquire and prepare the 14 table and desk models
      dep: HOUSE-00296 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00979 — Grouped: acquire and prepare the 12 bed and bedding models
      dep: HOUSE-00296 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00980 — Grouped: acquire and prepare the 22 storage-furniture models, plus the generated cabinet carcasses and shelving
      dep: HOUSE-00296 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00981 — Grouped: acquire and prepare the 26 small-appliance and lamp models
      dep: HOUSE-00296 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00982 — Grouped: acquire and prepare the 55 decoration models (rugs, curtains, mirrors, clocks, vases, plants, frames)
      dep: HOUSE-00296 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00983 — Grouped: acquire and prepare the 40 kitchenware and food prop models
      dep: HOUSE-00296 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00984 — Grouped: acquire and prepare the 45 box, crate, bin, tool and clutter models
      dep: HOUSE-00296 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00985 — `prop_kit_gen.py`: generate cabinet carcasses, shelving, boxes, radiators, ducts, pipe runs and wire runs from data
      dep: HOUSE-00971 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00986 — Furnish `L0_FOYER` and `L0_PORCH`
      dep: HOUSE-00973…HOUSE-00985 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00987 — Furnish `L0_HALL` (including the gallery wall placement)
      dep: HOUSE-00986 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00988 — Furnish `L0_LIVING` (fireplace, piano, seating, bay window seat)
      dep: HOUSE-00986 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00989 — Furnish `L0_DINING`
      dep: HOUSE-00986 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00990 — Furnish `L0_KITCHEN` (the densest room: island, run, appliances, 62 containers)
      dep: HOUSE-00975 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00991 — Furnish `L0_PANTRY` and `L0_BUTLERS`
      dep: HOUSE-00990 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00992 — Furnish `L0_FAMILY` (television, sectional, dog bed, media unit)
      dep: HOUSE-00986 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00993 — Furnish `L0_SUNROOM` (breakfast table, wicker, plants, wet bar)
      dep: HOUSE-00986 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00994 — Furnish `L0_OFFICE`, `L0_CLOSET_W`
      dep: HOUSE-00986 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00995 — Furnish `L0_MUDROOM`, `L0_LAUNDRY`
      dep: HOUSE-00986 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00996 — Furnish `L0_WC1`, `L0_WC2`
      dep: HOUSE-00976 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00997 — Furnish `L0_STOR`, `L0_STAIR_MAIN`
      dep: HOUSE-00986 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00998 — Furnish `L0_GARAGE` (car, workbench, shelving, tools, bikes, bins, 28 containers, the door mechanism)
      dep: HOUSE-00293, HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00999 — Furnish `L1_LANDING` and `L1_HALL`, `L1_HALL_W`
      dep: HOUSE-00986 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01000 — Furnish `L1_MASTER_BED`
      dep: HOUSE-00979 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01001 — Furnish `L1_MASTER_BATH` and `L1_MASTER_CLOSET`
      dep: HOUSE-00976 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01002 — Furnish `L1_BED2` (double, desk, wardrobe)
      dep: HOUSE-00979 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01003 — Furnish `L1_BED3` (child's room)
      dep: HOUSE-00979 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01004 — Furnish `L1_BED4` (teenager's room, deliberately untidy)
      dep: HOUSE-00979 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01005 — Furnish `L1_BED5` (guest, made up, unused)
      dep: HOUSE-00979 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01006 — Furnish `L1_BATH2`, `L1_BATH3`, `L1_WC3`, `L1_WC4`
      dep: HOUSE-00976 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01007 — Furnish `L1_LINEN`, `L1_STOR`, `L1_CLOSET_2`, `L1_CLOSET_3`
      dep: HOUSE-00973 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01008 — Furnish `L1_BALCONY_REAR` and `L1_BALCONY_FRONT`
      dep: HOUSE-00771 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01009 — Furnish `L2_LANDING`, `L2_HALL`, `L2_HALL_W`
      dep: HOUSE-00999 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01010 — Furnish `L2_LIBRARY` (floor-to-ceiling shelves, ladder, reading chairs)
      dep: HOUSE-00980 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01011 — Furnish `L2_GAMES` (pool table, dartboard, arcade cabinet, jigsaw)
      dep: HOUSE-00977 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01012 — Furnish `L2_SITTING` (record player, plants)
      dep: HOUSE-00977 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01013 — Furnish `L2_BED6` (sewing room) and `L2_BED7`
      dep: HOUSE-00979 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01014 — Furnish `L2_BATH4`, `L2_BATH5`, `L2_WC5`, `L2_WC6`
      dep: HOUSE-00976 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01015 — Furnish `L2_STOR2`, `L2_CLOSET_4`, `L2_LINEN2`, `L2_STAIR_ATTIC`
      dep: HOUSE-00973 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01016 — Furnish `L3_ROOM` (the finished attic: old sofa, desk, boxes, rocking horse, train set)
      dep: HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01017 — Furnish `L3_STORE_W` (40 boxes, wardrobe, suitcases, insulation, walkway, the spider's web)
      dep: HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01018 — Furnish `L3_STORE_E` (header tank, ducts, aerial mast, cable runs)
      dep: HOUSE-00985 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01019 — Furnish `L3_STORE_N`, `L3_STORE_S`, `L3_STAIR_HEAD`
      dep: HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01020 — Furnish `B1_HALL`, `B1_STAIR`, `B1_UNDERSTAIR`
      dep: HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01021 — Furnish `B1_MECHANICAL` (furnace, air handler, water heater, expansion tank, water main) from the plumbing/HVAC data
      dep: HOUSE-00386, HOUSE-00387 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01022 — Furnish `B1_ELECTRICAL` and `B1_UTILITY`
      dep: HOUSE-01021 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01023 — Furnish `B1_CINEMA` (projector, screen, 6 seats, acoustic panels) and `B1_WC7`
      dep: HOUSE-00977 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01024 — Furnish `B1_GYM` and `B1_WORKSHOP`
      dep: HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01025 — Furnish `B1_STOR1`, `B1_STOR2`, `B1_CELLAR`, `B1_LAUNDRY2`, `B1_HOBBY`
      dep: HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01026 — Furnish `EXT_SHED` (tools, mower, pots, bags, bench)
      dep: HOUSE-00984 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01027 — Run the anti-repetition validator over the whole house and fix every violation
      dep: HOUSE-00986…HOUSE-01026 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01028 — Run the density check: every cell within its §59.1 band
      dep: HOUSE-01027 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01029 — Rebuild chunks over the furnished house; verify the ≤ 6-chunks-per-cell target still holds
      dep: HOUSE-01028, HOUSE-00473 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-01030 — Re-bake lightmaps over the furnished house (furniture casts and receives baked light)
      dep: HOUSE-01029, HOUSE-00909 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-01031 — Measure draw calls, triangles and texture memory over the furnished house against budget
      dep: HOUSE-01030 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-01032 — Render tests: one pose per interior cell (78 scenes) at a fixed time and lighting
      dep: HOUSE-01030 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01033 — Furnishing review: walk the whole house and list what reads as fake; fix it
      dep: HOUSE-01032 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-01034 — Phase-13 review and commit
      dep: HOUSE-01033 · sys: — · plat: ALL · pri: MUST

---

## Phase 14 — Interactable framework

- [ ] HOUSE-01121 — Implement `Interactable`: the record, its typed state variant, and the registry keyed by stable id
      dep: HOUSE-00354 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01122 — Implement the behaviour factory keyed by `kind`, and the `IBehaviour` interface (`Update`, `CanPerform`, `Perform`, `Serialise`, `Deserialise`, `Reset`)
      dep: HOUSE-01121 · sys: interaction · plat: ALL · pri: MUST
      accept: adding a behaviour is one class and one registry line; no `if (name == …)` anywhere
- [ ] HOUSE-01123 — Implement the predicate/effect evaluator over the closed vocabulary, with load-time compilation
      dep: HOUSE-01122, HOUSE-00354 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01124 — Implement `Targeting`: the candidate set from the camera cell plus one open portal hop plus the proximity sphere
      dep: HOUSE-01121, HOUSE-00670 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01125 — Implement ray/bounds intersection scoring with the distance + angular term
      dep: HOUSE-01124, HOUSE-00546 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01126 — Implement the occlusion check (one short raycast between the eye and the hit)
      dep: HOUSE-01125 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01127 — Implement the precise/generous acceptance modes and the per-interactable `maxDistance`
      dep: HOUSE-01125 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01128 — Implement target stickiness (0.15 s, 12° tolerance)
      dep: HOUSE-01127 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01129 — Implement the third-person rule: the ray originates at the avatar's eye, not the camera
      dep: HOUSE-01124 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01130 — Implement the prompt view: verb + name, primary and secondary, fade in/out, virtual-unit layout
      dep: HOUSE-01128, HOUSE-00145 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-01131 — Implement the crosshair and its target-acquired state
      dep: HOUSE-01130 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-01132 — Implement action dispatch: predicate → effect → animation → sound → event → dirty-for-save
      dep: HOUSE-01123 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01133 — Implement busy/blocked reporting and the toast for "Locked", "Blocked", "Busy"
      dep: HOUSE-01132 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01134 — Implement action cancellation for non-atomic actions on moving away
      dep: HOUSE-01132 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01135 — Implement the rigid-animation driver: the per-kind closed-form transform functions of `cna-house.md` §24.2
      dep: HOUSE-01132 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01136 — Implement `PickupBehaviour` and the single-item carrying model, with `E` place and `G` drop
      dep: HOUSE-01132, HOUSE-00419 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01137 — Implement the held-item first-person prop rendering and the HUD thumbnail
      dep: HOUSE-01136 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-01138 — Implement `SeatBehaviour` (sit down, stand up, occupancy)
      dep: HOUSE-01132 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01139 — Implement the interaction sound hookup (per-action sound at the interactable's emitter)
      dep: HOUSE-01132, HOUSE-00154 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01140 — Implement the `F2` overlay's target section (id, kind, state, actions, predicates)
      dep: HOUSE-01132, HOUSE-00631 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-01141 — Implement the console `interact <id> <verb>` command for scripted tests
      dep: HOUSE-01132 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-01142 — Test: targeting picks the nearest valid candidate; stickiness prevents flicker between adjacent switches
      dep: HOUSE-01128 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01143 — Test: every one of the 640 interactables is targetable from a reachable standing position
      dep: HOUSE-00361, HOUSE-01127 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01144 — Test: an unknown predicate token fails at load, not at runtime
      dep: HOUSE-01123 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01145 — Measure interaction cost against the budget
      dep: HOUSE-01132 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-01146 — Phase-14 review and commit
      dep: HOUSE-01121…HOUSE-01145 · sys: — · plat: ALL · pri: MUST

---

## Phase 15 — Doors and windows

- [ ] HOUSE-01181 — Implement `DoorBehaviour`: `openFraction`, `latched`, `locked`, swing sign, open/close durations and easing
      dep: HOUSE-01122, HOUSE-00401 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01182 — Wire the door leaf's rigid transform and the double-door offset
      dep: HOUSE-01181, HOUSE-01135 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01183 — Wire the door's OBB into the dynamic collision list; implement the blocked-by-player stop
      dep: HOUSE-01182, HOUSE-00554 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-01184 — Wire `openFraction` → `PortalRuntime::aperture` with the 0.05/0.08 hysteresis
      dep: HOUSE-01182, HOUSE-00665 · sys: visibility · plat: ALL · pri: MUST
      verify: the door-state matrix test of HOUSE-00687 now runs against real doors
- [ ] HOUSE-01185 — Implement locking: the front door, rear slider, garage side door and rear gate; the player's key; the "Locked" report and rattle
      dep: HOUSE-01181 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01186 — Implement the self-closing garage↔mudroom door
      dep: HOUSE-01181 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01187 — Implement wind-driven door motion and slamming above 9 m/s
      dep: HOUSE-01181 · sys: interaction · plat: ALL · pri: SHOULD
      dep-note: needs the wind system; scheduled after phase 30, tracked here
- [ ] HOUSE-01188 — Implement door audio: latch, hinge creak scaled by speed, thud, slam, per door class
      dep: HOUSE-00282, HOUSE-01139 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01189 — Implement `GarageDoorBehaviour`: 5 panel segments on a spline, motor sound, rail rumble, end clunk, 6 s travel
      dep: HOUSE-01181, HOUSE-00416 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01190 — Implement `GateBehaviour`: the hinged pedestrian gate, the sliding vehicle gate with its motor, the bolted rear gate
      dep: HOUSE-01181, HOUSE-00416 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01191 — Implement `WindowBehaviour`: the four mechanisms (double-hung, casement, slider, hopper) with their travels
      dep: HOUSE-01122, HOUSE-00402 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01192 — Wire window sash transforms and the sill collision
      dep: HOUSE-01191, HOUSE-01135 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01193 — Wire windows to their `glass` portals: vision always passes, the player never does
      dep: HOUSE-01192, HOUSE-00665 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-01194 — Implement window audio: sash rumble by mass class, latch, and the exterior-ambience gain change on opening
      dep: HOUSE-01191 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01195 — Implement the open-window rain spray decal and the drying wet patch on the sill
      dep: HOUSE-01191 · sys: weather · plat: ALL · pri: SHOULD
      dep-note: needs the rain system; scheduled after phase 27
- [ ] HOUSE-01196 — Implement the `blindFraction` data path and the `translucent` portal mode (blinds themselves are phase 45)
      dep: HOUSE-01193 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-01197 — Author and place the door and window hardware props (handles, latches, hinges, sash locks)
      dep: HOUSE-00378 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01198 — Test: every door opens, closes, blocks the player when closed, and changes its portal
      dep: HOUSE-01184 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01199 — Test: every window opens, closes, never admits the player, and always passes vision
      dep: HOUSE-01193 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01200 — Test: the garage door's five segments animate correctly and block the player at every intermediate position
      dep: HOUSE-01189 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01201 — Render tests: 10 door poses at 0.0 / 0.35 / 1.0 open
      dep: HOUSE-01184 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01202 — Phase-15 review and commit
      dep: HOUSE-01181…HOUSE-01201 · sys: — · plat: ALL · pri: MUST

---

## Phase 16 — Lights and switches  ·  **First playable milestone**

- [ ] HOUSE-01251 — Implement `RoomLightState` and `LightingSystem` skeleton: per-cell artificial and daylight levels
      dep: HOUSE-00348 · sys: lighting · plat: ALL · pri: MUST
- [ ] HOUSE-01252 — Implement `LightBehaviour` and the switch-group model, with the three-way pairs
      dep: HOUSE-01122, HOUSE-00403 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01253 — Implement the switch-plate interactables with per-gang actions
      dep: HOUSE-01252, HOUSE-00384 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01254 — Implement the lamp-as-switch case (table and floor lamps switched by interacting with the lamp)
      dep: HOUSE-01252 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01255 — Implement the Planckian colour-temperature → RGB lookup
      dep: HOUSE-01251 · sys: lighting · plat: ALL · pri: MUST
- [ ] HOUSE-01256 — Implement the multi-pass additive static lighting draw (pass 1 opaque + up to 3 additive light-group passes)
      dep: HOUSE-00912, HOUSE-00079 · sys: rendering · plat: ALL · pri: MUST
      accept: depth-equal on the additive passes; no z-fighting; exact addition
- [ ] HOUSE-01257 — Implement the `ambientFloor` so an unlit windowless room is very dark but not black
      dep: HOUSE-01256 · sys: lighting · plat: ALL · pri: MUST
- [ ] HOUSE-01258 — Implement the switch-on transition per bulb class (filament ramp, instant LED, fluorescent flicker start)
      dep: HOUSE-01256 · sys: lighting · plat: ALL · pri: MUST
- [ ] HOUSE-01259 — Implement fixture emissive materials and their on/off state
      dep: HOUSE-01258 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01260 — Implement the additive glow quads for bulbs, with camera-exposure-dependent size and alpha
      dep: HOUSE-01259 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01261 — Implement the per-object three-directional-light assignment (key/fill/bounce) for dynamic objects
      dep: HOUSE-01251, HOUSE-00892 · sys: lighting · plat: ALL · pri: MUST
- [ ] HOUSE-01262 — Implement the point-light-as-directional approximation with distance attenuation
      dep: HOUSE-01261 · sys: lighting · plat: ALL · pri: MUST
- [ ] HOUSE-01263 — Implement the daylight model: per-window transmission, open boost, sky exposure, shading factor
      dep: HOUSE-00779, HOUSE-00207 · sys: lighting · plat: ALL · pri: MUST
      dep-note: uses a fixed noon sun until phase 23 lands the real sun
- [ ] HOUSE-01264 — Implement the `LM_DAY` additive pass driven by `daylightLevel`
      dep: HOUSE-01263, HOUSE-00910 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01265 — Implement the 2-hop light flood through open portals, capped at 0.35 of the source
      dep: HOUSE-01263, HOUSE-00665 · sys: lighting · plat: ALL · pri: MUST
      accept: opening the kitchen door visibly brightens the hall
- [ ] HOUSE-01266 — Implement the exposure model: per-cell target, asymmetric adaptation, Tier S implementation by effect scaling plus a tint quad
      dep: HOUSE-01264 · sys: lighting · plat: ALL · pri: MUST
- [ ] HOUSE-01267 — Implement blob shadows: projected ellipse, direction from the dominant light, sun-elongation outdoors
      dep: HOUSE-01261 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01268 — Implement the sun-patch decals from the precomputed polygons, interpolated between grid entries
      dep: HOUSE-00208, HOUSE-01263 · sys: rendering · plat: ALL · pri: MUST
      dep-note: static until phase 23; then it moves with the sun
- [ ] HOUSE-01269 — Implement the exterior dusk-sensor lights with per-fixture random offsets
      dep: HOUSE-01252 · sys: lighting · plat: ALL · pri: MUST
- [ ] HOUSE-01270 — Implement light-switch audio (plate click at the plate's position)
      dep: HOUSE-00283, HOUSE-01252 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01271 — Implement the `F6` lighting overlay
      dep: HOUSE-01251, HOUSE-00147 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-01272 — Implement the console `light <group> <on|off>` command
      dep: HOUSE-01252 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-01273 — Test: each of the 84 groups switches, changes its room's level, and changes the neighbour's borrowed light
      dep: HOUSE-01265 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01274 — Test: the 12 observable-behaviour rows of `cna-house.md` §30, each asserted on a measurable proxy
      dep: HOUSE-01266 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01275 — Render tests: 20 rooms, lights on and off, at noon and at midnight
      dep: HOUSE-01266 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01276 — Measure the lighting system and the extra additive passes against budget
      dep: HOUSE-01256 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-01277 — **First-playable review**: run the §78 exit criteria in full — 60 FPS at High in all 10 scenarios, visibility suite green, collision suite green, 20-minute bot walk clean
      dep: HOUSE-01276 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-01278 — Tag and record the first-playable build; capture a screenshot set for the documentation
      dep: HOUSE-01277 · sys: — · plat: ALL · pri: MUST

---

## Phase 17 — Containers

- [ ] HOUSE-01311 — Implement `ContainerBehaviour`: `openFraction`, contents list, open/close durations per kind
      dep: HOUSE-01122, HOUSE-00404…HOUSE-00411 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01312 — Implement drawer slide, cabinet swing and wardrobe double-door transforms
      dep: HOUSE-01311, HOUSE-01135 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01313 — Implement container sub-cells and their portals, so contents are culled when closed
      dep: HOUSE-01311, HOUSE-00373 · sys: visibility · plat: ALL · pri: MUST
      accept: a closed drawer submits zero contents draws — verified on the `F3` counter
- [ ] HOUSE-01314 — Implement the contents-visible threshold (`openFraction > 0.15`) and its hysteresis
      dep: HOUSE-01313 · sys: visibility · plat: ALL · pri: MUST
- [ ] HOUSE-01315 — Apply the fill kits to all 214 containers, with per-container seeds
      dep: HOUSE-00973, HOUSE-01311 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01316 — Implement the "visibly not openable" treatment for decorative case goods (no handle or a lock plate)
      dep: HOUSE-01311 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01317 — Implement container audio: 6 open/close sets by kind, with a slide/rattle for drawers
      dep: HOUSE-00283, HOUSE-01311 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01318 — Implement container collision (an open drawer is an obstacle)
      dep: HOUSE-01312, HOUSE-00554 · sys: physics · plat: ALL · pri: MUST
- [ ] HOUSE-01319 — Implement the takeable-item-in-container path (`PickupBehaviour` children)
      dep: HOUSE-01136, HOUSE-01311 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01320 — Test: every one of the 214 containers opens, shows contents, closes, and hides them
      dep: HOUSE-01314 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01321 — Test: no container's contents are ever submitted while it is closed
      dep: HOUSE-01313 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01322 — Measure the cost of a room with every container open (`L0_KITCHEN`, 62 open) against budget
      dep: HOUSE-01320 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-01323 — Phase-17 review and commit
      dep: HOUSE-01311…HOUSE-01322 · sys: — · plat: ALL · pri: MUST

---

## Phase 18 — Kitchen and refrigerator

- [ ] HOUSE-01361 — Implement the refrigerator as a two-door container with two interior cells and two portals
      dep: HOUSE-01313 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01362 — Implement the interior light group driven by `openFraction`
      dep: HOUSE-01361, HOUSE-01252 · sys: lighting · plat: ALL · pri: MUST
- [ ] HOUSE-01363 — Implement the cold light wedge on the kitchen floor (a sun-patch-style decal)
      dep: HOUSE-01362, HOUSE-01268 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01364 — Author and place the 24 fridge items and 8 freezer items with their shelves
      dep: HOUSE-00417, HOUSE-00983 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-01365 — Implement taking, carrying, placing and returning fridge items
      dep: HOUSE-01364, HOUSE-01136 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01366 — Implement the compressor cycle: 6–9 sim min on every 20–30, with start clunk, run loop and stop sigh, louder with the door open
      dep: HOUSE-00285, HOUSE-01361 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01367 — Implement the door-left-open alarm (40 real seconds, soft repeating beep)
      dep: HOUSE-01366 · sys: audio · plat: ALL · pri: SHOULD
- [ ] HOUSE-01368 — Implement the freezer frost overlay material and its colder interior light
      dep: HOUSE-01361 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-01369 — Implement the chest freezer in `L0_PANTRY` (lid, contents, its own slower cycle)
      dep: HOUSE-01361 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01370 — Implement `ApplianceBehaviour` and the range: 4 burners, flame/glow, hob-on hum, knob interaction
      dep: HOUSE-01122, HOUSE-00415 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01371 — Implement the double ovens: door, interior light, warming glow, timer, preheat sound
      dep: HOUSE-01370 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01372 — Implement the dishwasher: door, rack pull-out, a 3-stage programme with real audio and a completion chime
      dep: HOUSE-01370 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01373 — Implement the microwave: door, start, turntable rotation, hum, ding
      dep: HOUSE-01370 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01374 — Implement the kettle: fill from the tap, switch, a 45 s boil that builds, steam particles, click off
      dep: HOUSE-01370 · sys: interaction · plat: ALL · pri: MUST
      dep-note: the fill step needs phase 19
- [ ] HOUSE-01375 — Implement the coffee machine and the toaster
      dep: HOUSE-01370 · sys: interaction · plat: ALL · pri: SHOULD
- [ ] HOUSE-01376 — Implement the extractor hood: 3 speeds, light, fan sound
      dep: HOUSE-01370 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01377 — Implement the pedal bin: lid, contents
      dep: HOUSE-01311 · sys: interaction · plat: ALL · pri: SHOULD
- [ ] HOUSE-01378 — Implement the washing machine and dryer in `L0_LAUNDRY` with their 3-stage programmes
      dep: HOUSE-01370 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01379 — Implement the wine fridge in `L0_BUTLERS`
      dep: HOUSE-01361 · sys: interaction · plat: ALL · pri: OPT
- [ ] HOUSE-01380 — Test: the fridge interior is never submitted while both doors are closed
      dep: HOUSE-01361 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01381 — Test: take an item, place it, save, load, and find it where it was left
      dep: HOUSE-01365 · sys: ci · plat: CI · pri: MUST
      dep-note: needs phase 39
- [ ] HOUSE-01382 — Render tests: the kitchen at 4 states (dark, lit, fridge open in the dark, everything open)
      dep: HOUSE-01363 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01383 — Phase-18 review and commit
      dep: HOUSE-01361…HOUSE-01382 · sys: — · plat: ALL · pri: MUST

---

## Phase 19 — Plumbing and water

- [ ] HOUSE-01411 — Implement `FaucetBehaviour`: `flow`, `hotFraction`, 4 flow steps, on/off, mixer cycling
      dep: HOUSE-01122, HOUSE-00412 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01412 — Implement the tap handle rigid animation per tap type
      dep: HOUSE-01411, HOUSE-01135 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01413 — Implement the water-flow mesh: tapered cylinder, scrolling stretched texture, length clamped to the basin
      dep: HOUSE-01411, HOUSE-00904 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01414 — Implement the impact splash particles and the basin wet decal
      dep: HOUSE-01413 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01415 — Implement the plug state on the 3 baths and the kitchen sink, and the basin water plane
      dep: HOUSE-01411 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01416 — Implement bath filling: `fillLevel` integration, the rising water plane, the fill cap and the overflow outlet
      dep: HOUSE-01415 · sys: interaction · plat: ALL · pri: MUST
      accept: full from empty in ≈ 85 s at full flow; at 1.0 it stops and the overflow runs
- [ ] HOUSE-01417 — Implement the rising-pitch fill audio as the bath fills
      dep: HOUSE-01416, HOUSE-00284 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01418 — Implement the drain: level fall with the plug out, and the drain gurgle when flow stops
      dep: HOUSE-01415 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01419 — Implement the 3 showers: falling-particle cone, the steam build-up, the fogging glass screen
      dep: HOUSE-01413 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01420 — Implement the outdoor tap and hose reel
      dep: HOUSE-01411 · sys: interaction · plat: ALL · pri: SHOULD
- [ ] HOUSE-01421 — Implement per-fixture flow audio with 3 rate variants cross-faded
      dep: HOUSE-00284, HOUSE-01411 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01422 — Implement water hammer on a fast tap close, placed on the correct stack
      dep: HOUSE-01421, HOUSE-00386 · sys: audio · plat: ALL · pri: SHOULD
- [ ] HOUSE-01423 — Implement the whole-house pipe hiss in `B1` while any outlet runs, routed through the stacks
      dep: HOUSE-01422 · sys: audio · plat: ALL · pri: SHOULD
- [ ] HOUSE-01424 — Implement the water-heater response: it fires after hot water is drawn
      dep: HOUSE-01421 · sys: audio · plat: ALL · pri: SHOULD
- [ ] HOUSE-01425 — Test: every one of the 17 outlets turns on and off, makes the right sound, and shows water
      dep: HOUSE-01421 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01426 — Test: a running tap survives save and load with the same flow (D-15)
      dep: HOUSE-01411 · sys: ci · plat: CI · pri: MUST
      dep-note: needs phase 39
- [ ] HOUSE-01427 — Test: a bath fills to 1.0, stops, and never floods the room
      dep: HOUSE-01416 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01428 — Render tests: 6 water scenes (tap running, bath filling, shower with steam, fogged screen)
      dep: HOUSE-01419 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01429 — Phase-19 review and commit
      dep: HOUSE-01411…HOUSE-01428 · sys: — · plat: ALL · pri: MUST

---

## Phase 20 — Toilets

- [ ] HOUSE-01461 — Implement `ToiletBehaviour` and its state (`urine`, `solids`, `lidOpen`, `seatUp`, `flushPhase`, `paperLeft`)
      dep: HOUSE-01122, HOUSE-00413 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01462 — Implement lid and seat rigid rotations with their clacks
      dep: HOUSE-01461, HOUSE-01135 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01463 — Implement the `Urinate` action: 6 s, first-person tilt and vignette, no body animation, `urine += 0.35`
      dep: HOUSE-01461 · sys: interaction · plat: ALL · pri: MUST
      accept: prompt text is plain (`[E] Urinate`); nothing anatomical is drawn
- [ ] HOUSE-01464 — Implement the `Defecate` action: 12 s, third-person sit/stand clips, `solids += 1`, `urine += 0.2`
      dep: HOUSE-01463 · sys: interaction · plat: ALL · pri: MUST
      dep-note: the third-person clips arrive in phase 37; first person is complete without them
- [ ] HOUSE-01465 — Author the 3 solid meshes and their in-bowl placement with jitter
      dep: HOUSE-01464 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-01466 — Implement the bowl water plane and its urine tint
      dep: HOUSE-01461, HOUSE-00904 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01467 — Implement the flush: 2.2 s swirl (rotating scrolling texture + level dip), contents cleared, 6 s cistern refill
      dep: HOUSE-01466 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01468 — Implement flush and refill audio, routed through the room-aware model
      dep: HOUSE-00284, HOUSE-01467 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01469 — Implement `paperLeft` and the `Use paper` cosmetic action
      dep: HOUSE-01461 · sys: interaction · plat: ALL · pri: OPT
- [ ] HOUSE-01470 — Implement the closed-lid occlusion (no special case — the lid is opaque geometry over the bowl)
      dep: HOUSE-01462 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01471 — Test: use, save, load, contents present; flush, save, load, contents gone (all 13 toilets)
      dep: HOUSE-01467 · sys: ci · plat: CI · pri: MUST
      dep-note: needs phase 39
- [ ] HOUSE-01472 — Test: *Reset House* clears every toilet
      dep: HOUSE-01471 · sys: ci · plat: CI · pri: MUST
      dep-note: needs phase 40
- [ ] HOUSE-01473 — Render tests: clean, used-with-lid-open, used-with-lid-closed, mid-flush
      dep: HOUSE-01470 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01474 — Phase-20 review: confirm the system stayed matter-of-fact and proportionate (≈ 180 lines, 2 meshes, 4 sounds)
      dep: HOUSE-01473 · sys: — · plat: ALL · pri: MUST

---

## Phase 21 — Television and video

- [ ] HOUSE-01491 — Implement `ITvSource` and `TelevisionBehaviour` (`on`, `channel`, `volume`, `playhead`)
      dep: HOUSE-01122, HOUSE-00414 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01492 — Implement `VideoTvSource` over `Video`/`VideoPlayer` with `GetTexture()`
      dep: HOUSE-01491, HOUSE-00098 · sys: rendering · plat: LNX · pri: MUST
- [ ] HOUSE-01493 — Implement `SequenceTvSource`: frame-strip atlas, 12 fps timer, separate looping audio track
      dep: HOUSE-01491 · sys: rendering · plat: ALL · pri: MUST
      accept: selectable on Linux by a setting so it is exercised continuously (BL-05, D-17)
- [ ] HOUSE-01494 — Implement the screen quad material: unlit emissive plus a scanline/vignette overlay
      dep: HOUSE-01492 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01495 — Implement the screen-average light: a 4×4 downsample every 6 frames driving an emissive light group
      dep: HOUSE-01494, HOUSE-01252 · sys: lighting · plat: ALL · pri: MUST
      accept: a dark family room flickers with the picture
- [ ] HOUSE-01496 — Implement TV audio routing: the room-aware gain applied to `VideoPlayer::Volume` / the sequence track
      dep: HOUSE-01492 · sys: audio · plat: ALL · pri: MUST
      dep-note: full routing lands in phase 32
- [ ] HOUSE-01497 — Implement channel switching and the off state (a black screen with a fading white line)
      dep: HOUSE-01491 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01498 — Author the 4 channels: a generated weather-map channel, a generated clock channel, a CC0 nature channel, a no-signal channel
      dep: HOUSE-00275, HOUSE-00219 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-01499 — Implement the `B1_CINEMA` projector variant (a projected quad plus a light cone volume)
      dep: HOUSE-01494 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-01500 — Implement the `L1_MASTER_BED` set
      dep: HOUSE-01494 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-01501 — Test: the capability-absent path (`CNA_ENABLE_VIDEO=OFF`) falls back to `SequenceTvSource` with no error shown
      dep: HOUSE-01493, HOUSE-00099 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01502 — Test: TV state and playhead survive save and load
      dep: HOUSE-01491 · sys: ci · plat: CI · pri: MUST
      dep-note: needs phase 39
- [ ] HOUSE-01503 — Render tests: TV on and off, day and night, from inside the room and through a doorway
      dep: HOUSE-01495 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01504 — Phase-21 review and commit
      dep: HOUSE-01491…HOUSE-01503 · sys: — · plat: ALL · pri: MUST

---

## Phase 22 — Time

- [ ] HOUSE-01531 — Implement `SimClock`: epoch seconds, time scale, location, UTC offset, US DST rules
      dep: HOUSE-00139 · sys: environment · plat: ALL · pri: MUST
- [ ] HOUSE-01532 — Implement calendar conversion (epoch ↔ Y/M/D h:m:s, day-of-year, weekday) and its tests
      dep: HOUSE-01531 · sys: environment · plat: ALL · pri: MUST
      verify: unit ClockTests.* against 500 known conversions incl. DST boundaries and leap years
- [ ] HOUSE-01533 — Implement the day-length presets and the custom value, wired to settings
      dep: HOUSE-01531, HOUSE-00131 · sys: environment · plat: ALL · pri: MUST
- [ ] HOUSE-01534 — Implement season derivation and its exposure to the weather system
      dep: HOUSE-01532 · sys: environment · plat: ALL · pri: MUST
- [ ] HOUSE-01535 — Implement the temperature curve (annual + diurnal) for the configured location
      dep: HOUSE-01534 · sys: environment · plat: ALL · pri: MUST
- [ ] HOUSE-01536 — Implement the console commands `time set`, `time scale`, `time advance`
      dep: HOUSE-01531 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-01537 — Implement the `F8` environment overlay's time section
      dep: HOUSE-01531, HOUSE-00147 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-01538 — Implement clock persistence in the save model
      dep: HOUSE-01531 · sys: persistence · plat: ALL · pri: MUST
      dep-note: schema lands in phase 39
- [ ] HOUSE-01539 — Test: at `timeScale = 60`, 3 real seconds advance the clock by exactly 3 simulated minutes
      dep: HOUSE-01533 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01540 — Test: a frame hitch of 250 ms advances the clock by the real elapsed time, and the physics accumulator clamps
      dep: HOUSE-01531, HOUSE-00549 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01542 — Implement the compressed year of `cna-house.md` §35.2b: `SimClock::calendarDaysPerSimDay` (default 24.0), so one simulated day advances the calendar by 24 days and a year takes 365 real minutes
      dep: HOUSE-01532 · sys: environment · plat: ALL · pri: MUST
      files: src/environment/SimClock.cpp|hpp
      accept: (1) at `timeScale = 60` and the default compression, 365 real minutes advance the
              calendar by exactly one year; (2) the solar hour angle still completes one turn per
              24 real minutes while the declination completes one turn per 365 real minutes;
              (3) `calendarDaysPerSimDay = 1.0` restores a realistic calendar for debugging
      verify: unit ClockTests.CompressedYear
- [ ] HOUSE-01543 — Implement `SeasonPhase` (`yearFraction`, `primary`, `secondary`, `blend`) as a **continuous** value per `cna-house.md` §36.3; a new game starts at the vernal equinox
      dep: HOUSE-01542, HOUSE-01534 · sys: environment · plat: ALL · pri: MUST
      files: src/environment/SeasonPhase.cpp|hpp
      accept: (1) a fresh save starts in spring at `yearFraction == 0`; (2) `blend` is 0 through the
              middle of a season and ramps over the outer 20 % at each end; (3) `yearFraction` is
              continuous and monotonic across the year wrap; (4) **no consumer reads an integer
              season without a blend weight** — asserted by a lint over `src/`
      verify: unit SeasonPhaseTests.*
- [ ] HOUSE-01544 — Implement seasonal day-length variation: sunrise and sunset drift with the declination, so the shortest and longest days are visibly different within one 6-hour year
      dep: HOUSE-01542, HOUSE-01561 · sys: environment · plat: ALL · pri: MUST
      accept: (1) at the configured latitude, midsummer and midwinter daylight lengths differ by the
              analytic amount within 2 simulated minutes; (2) the drift is smooth frame to frame
      verify: unit SunTests.SeasonalDayLength; screenshot scene sun-season-01..04
- [ ] HOUSE-01545 — Implement the outdoor temperature model: seasonal base curve + diurnal curve + weather `Δtemp`, driven by `SeasonPhase` and blended, never switched
      dep: HOUSE-01543, HOUSE-01535 · sys: environment · plat: ALL · pri: MUST
      accept: (1) the annual minimum and maximum land in winter and summer respectively; (2) the
              diurnal minimum is near dawn; (3) the curve is continuous across every season
              boundary; (4) it is the single source `W_SNOW` gating and storm probability read
      verify: unit TemperatureTests.* sampling a full year
- [ ] HOUSE-01546 — Implement the in-game environment readout: time of day, season, progress through the year, and the outdoor temperature
      dep: HOUSE-01545, HOUSE-01543 · sys: ui · plat: ALL · pri: MUST
      accept: (1) it is a **player-facing** readout, not the `F8` debug overlay; (2) it shows the
              simulated clock time, the current season, year progress, and the outdoor temperature
              in °C; (3) units and the exact presentation are settled during the task — the
              requirement is that all four values are legible, not a particular widget;
              (4) it can be hidden from settings
      verify: render test hud-season-01; manual read-through of one full simulated year
- [ ] HOUSE-01547 — Test: one uninterrupted 365-real-minute run passes through all four seasons exactly once, starting and ending in spring
      dep: HOUSE-01546 · sys: ci · plat: CI · pri: MUST
      accept: run headless with the clock driven at a large `timeScale`; assert the season sequence,
              that `blend` never jumps, and that the temperature curve is continuous throughout
      verify: integration SeasonCycleTests.FullYear
- [ ] HOUSE-01541 — Phase-22 review and commit
      dep: HOUSE-01531…HOUSE-01540, HOUSE-01542…HOUSE-01547 · sys: — · plat: ALL · pri: MUST

---

## Phase 23 — Sun, glare and the sun-clock

- [ ] HOUSE-01561 — Implement `SunModel`: the simplified NOAA algorithm producing declination, hour angle, altitude and azimuth
      dep: HOUSE-01532 · sys: environment · plat: ALL · pri: MUST
      verify: unit SunModelTests.* against 200 published sunrise/sunset times, ± 3 minutes
- [ ] HOUSE-01562 — Implement the world-space sun direction with the north = `−Z` convention
      dep: HOUSE-01561 · sys: environment · plat: ALL · pri: MUST
- [ ] HOUSE-01563 — Implement the sun colour and intensity LUT over altitude, with the cloud modulation
      dep: HOUSE-01562 · sys: environment · plat: ALL · pri: MUST
- [ ] HOUSE-01564 — Wire the sun into `LightingSystem`: it becomes `DirectionalLight0` for outdoor objects and drives `daylight`
      dep: HOUSE-01563, HOUSE-01263 · sys: lighting · plat: ALL · pri: MUST
- [ ] HOUSE-01565 — Make the sun-patch decals follow the real sun by interpolating the 12 × 24 grid
      dep: HOUSE-01564, HOUSE-01268 · sys: rendering · plat: ALL · pri: MUST
      accept: a patch visibly crosses a bedroom floor over a simulated afternoon
- [ ] HOUSE-01566 — Implement the sun disc quad with horizon scaling and reddening
      dep: HOUSE-01563 · sys: rendering · plat: ALL · pri: MUST
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
- [ ] HOUSE-01572 — **Implement the sun-clock overlay**: the 22°/30° hysteresis cone, the altitude and coverage conditions, the fades, the position and the two text lines
      dep: HOUSE-01569, HOUSE-00145 · sys: ui · plat: ALL · pri: MUST
      accept: (1) looking near the sun shows the time within 0.35 s; (2) looking away hides it within 0.6 s; (3) it never flickers at the boundary; (4) it does not require centre-pixel aim
      verify: unit SunClockTests.* on the hysteresis state machine; render tests `sunclock-on`/`sunclock-off`
- [ ] HOUSE-01573 — Implement the moon variant of the overlay at a 28° cone
      dep: HOUSE-01572 · sys: ui · plat: ALL · pri: MUST
      dep-note: needs phase 24
- [ ] HOUSE-01574 — Implement civil/nautical/astronomical twilight thresholds and their effect on ambient and stars
      dep: HOUSE-01563 · sys: environment · plat: ALL · pri: MUST
- [ ] HOUSE-01575 — Validate the OPENGLES3 boolean-grid coverage against the OPENGL33 true count on the same scene
      dep: HOUSE-01569, HOUSE-00091 · sys: ci · plat: LNX · pri: MUST
      accept: the two agree within 0.15 coverage across 20 sampled camera angles
- [ ] HOUSE-01576 — Render tests: sunrise, morning, noon, afternoon, sunset, and the glare at 6 angles
      dep: HOUSE-01572 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01577 — Measure the glare queries' cost
      dep: HOUSE-01569 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-01578 — Phase-23 review and commit
      dep: HOUSE-01561…HOUSE-01577 · sys: — · plat: ALL · pri: MUST

---

## Phase 24 — Moon and stars

- [ ] HOUSE-01601 — Implement `MoonModel`: position from the truncated ELP terms
      dep: HOUSE-01561 · sys: environment · plat: ALL · pri: MUST
      verify: unit MoonModelTests.* against 60 published moonrise times, ± 8 minutes
- [ ] HOUSE-01602 — Implement the continuous phase computation (elongation → illuminated fraction → waxing/waning → `phase` ∈ [0,1))
      dep: HOUSE-01601 · sys: environment · plat: ALL · pri: MUST
      verify: unit MoonPhaseTests.* against 60 published phase dates, ± 0.02
- [ ] HOUSE-01603 — Implement the phase-name mapping for the overlay and the almanac line
      dep: HOUSE-01602 · sys: environment · plat: ALL · pri: MUST
- [ ] HOUSE-01604 — Implement the CPU-generated 128² phase mask with the exact elliptical terminator, earthshine and libration rotation
      dep: HOUSE-01602 · sys: rendering · plat: ALL · pri: MUST
      accept: regenerated only when `phase` moves by > 1/128; upload cost measured and negligible
      verify: unit MoonMaskTests.* comparing generated masks against analytic references at 16 phases
- [ ] HOUSE-01605 — Acquire and prepare the lunar albedo texture (1024², public domain, provenance recorded)
      dep: HOUSE-00274 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-01606 — Implement the moon disc quad with the mask, horizon scaling and reddening
      dep: HOUSE-01604, HOUSE-01605 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01607 — Implement moonlight: intensity from the non-linear phase curve, the blue colour, and the cloud modulation
      dep: HOUSE-01602, HOUSE-01564 · sys: lighting · plat: ALL · pri: MUST
      accept: a new-moon overcast night with the lights off is genuinely dark; a full-moon clear night is navigable
- [ ] HOUSE-01608 — Implement `moonPhaseSpeedMultiplier` and the `time advance <days>` command's effect on the phase
      dep: HOUSE-01602, HOUSE-00131 · sys: environment · plat: ALL · pri: MUST
- [ ] HOUSE-01609 — Generate the 1 500-star catalogue binary from a public-domain source (RA, Dec, magnitude, B−V)
      dep: HOUSE-00274 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-01610 — Implement `StarField`: the dynamic vertex buffer of camera-facing quads with magnitude-driven size and B−V colour
      dep: HOUSE-01609 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01611 — Implement sidereal rotation about the celestial pole at the configured latitude
      dep: HOUSE-01610, HOUSE-01532 · sys: rendering · plat: ALL · pri: MUST
      accept: Polaris sits at altitude ≈ latitude; constellations change with the season
- [ ] HOUSE-01612 — Implement star visibility: the twilight ramp, the magnitude cutoff that tightens with twilight, cloud and moon suppression
      dep: HOUSE-01611, HOUSE-01574 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01613 — Implement twinkle with the altitude-dependent amplitude, updated at 20 Hz
      dep: HOUSE-01612 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-01614 — Implement the light-pollution dome glow toward the town
      dep: HOUSE-01612 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-01615 — Implement the two satellites and the occasional meteor
      dep: HOUSE-01612 · sys: rendering · plat: ALL · pri: OPT
- [ ] HOUSE-01616 — Implement the `F8` overlay's sun/moon/star section
      dep: HOUSE-01612, HOUSE-01537 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-01617 — Render tests: the 8 named phases, a clear night, an overcast night, moonrise, and the star field at three twilight stages
      dep: HOUSE-01612 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01618 — Test: 30 simulated days advance the phase through exactly one lunation ± 0.03
      dep: HOUSE-01608 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01619 — Phase-24 review and commit
      dep: HOUSE-01601…HOUSE-01618 · sys: — · plat: ALL · pri: MUST

---

## Phase 25 — Sky and clouds

- [ ] HOUSE-01641 — Generate the sky LUTs offline (zenith and horizon, over sun altitude, cloud cover and azimuth offset) and tune the sunrise/sunset warmth
      dep: HOUSE-00394 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-01642 — Generate the sky dome mesh (32 × 18 hemisphere + horizon skirt)
      dep: HOUSE-01641 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-01643 — Implement `SkySystem`: the dome draw with `DepthStencilState::None`, camera-following translation
      dep: HOUSE-01642 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01644 — Implement CPU vertex-colour recomputation from the LUTs, triggered by a material change in the sky state
      dep: HOUSE-01643 · sys: rendering · plat: ALL · pri: MUST
      accept: recomputed a few times per simulated minute, not per frame; measured cost negligible
- [ ] HOUSE-01645 — Implement the night sky blend and the sun-glow term
      dep: HOUSE-01644 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01646 — Author the three cloud textures (cirrus, cumulus, stratus) with alpha
      dep: HOUSE-01641 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-01647 — Implement the three cloud dome rings with wind-aligned UV scrolling
      dep: HOUSE-01646, HOUSE-01643 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01648 — Implement the cloud-state → layer-alpha mapping, interpolated continuously from `cloudCover` and `thunderIntensity`
      dep: HOUSE-01647 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01649 — Implement cloud tinting by the sky colour so clouds pick up sunset light
      dep: HOUSE-01648 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01650 — Implement fog: colour from the horizon in the view direction, start/end from `fogDensity` and precipitation, enabled only for exterior batches
      dep: HOUSE-01649, HOUSE-00892 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01651 — Implement the sky's contribution to `LightingSystem`'s ambient and to the `LM_DAY` tint
      dep: HOUSE-01645, HOUSE-01263 · sys: lighting · plat: ALL · pri: MUST
- [ ] HOUSE-01652 — Tune the six sky states against reference photographs; record the final LUTs
      dep: HOUSE-01650 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-01653 — Render tests: the six sky states at four times of day (24 scenes)
      dep: HOUSE-01652 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01654 — Measure the sky and cloud cost
      dep: HOUSE-01650 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-01655 — Phase-25 review and commit
      dep: HOUSE-01641…HOUSE-01654 · sys: — · plat: ALL · pri: MUST

---

## Phase 26 — Weather core

- [ ] HOUSE-01681 — Implement `WeatherState` with every field of `cna-house.md` §36.1, and its JSON round trip
      dep: HOUSE-00393 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01682 — Implement archetype loading and validation from `layout.weather.json`
      dep: HOUSE-01681 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01683 — Implement the seasonal transition matrices and archetype sampling from the seeded RNG
      dep: HOUSE-01682, HOUSE-00027 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01684 — Implement dwell and transition-time distributions
      dep: HOUSE-01683 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01685 — Implement the per-channel rate limiter with the table of §42.2
      dep: HOUSE-01684 · sys: weather · plat: ALL · pri: MUST
      accept: **no channel can ever exceed its rate**; asserted over 10 000 simulated minutes × 200 seeds
      verify: unit WeatherRateTests.NoChannelEverExceedsItsRate
- [ ] HOUSE-01686 — Implement the smoothstep blend from the snapshot to the target
      dep: HOUSE-01685 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01687 — Implement the `W_WINDY` modifier as an independent wind draw combinable with any precipitation archetype
      dep: HOUSE-01686 · sys: weather · plat: ALL · pri: MUST
      accept: "windy heavy rain" and "blizzard" arise without a combinatorial state list
- [ ] HOUSE-01688 — Implement temperature-derived `precipType` (rain / sleet / snow) overriding the archetype's nominal type
      dep: HOUSE-01686, HOUSE-01535 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01689 — Implement the "precip type cannot change while intensity > 0.05" rule with its ramp-down/ramp-up
      dep: HOUSE-01688 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01690 — Implement `surfaceWetness` integration (accumulation and drying)
      dep: HOUSE-01686 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01691 — Implement `snowDepth` integration (accumulation and melt)
      dep: HOUSE-01690 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01692 — Implement RNG-state persistence so a reload reproduces the same weather future
      dep: HOUSE-01683 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01693 — Implement the settings: weather on / fixed / off, and the fixed-archetype selector
      dep: HOUSE-01686, HOUSE-00131 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01694 — Implement the console `weather set <archetype>` and `weather freeze`
      dep: HOUSE-01693 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-01695 — Implement the `F8` overlay's weather section (full vector, archetype, time to transition, RNG state)
      dep: HOUSE-01686, HOUSE-01537 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-01696 — Wire the weather into the sky (cloud cover, thunder), lighting (cloud modulation) and fog
      dep: HOUSE-01686, HOUSE-01648 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01697 — Test: determinism — the same seed produces an identical 10 000-simulated-minute history
      dep: HOUSE-01692 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01698 — Test: over 30 simulated days the system visits ≥ 8 archetypes and never produces an impossible combination
      dep: HOUSE-01697 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01699 — Test: it snows in January and rains in July at the default location, with no special-casing
      dep: HOUSE-01688 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01700 — Test: no `bool isRaining`-style field exists anywhere (lint)
      dep: HOUSE-00021 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01701 — Tune the archetype table and the transition matrices against a subjective "does a week of weather feel right?" review over 7 simulated days
      dep: HOUSE-01698 · sys: weather · plat: LNX · pri: MUST
- [ ] HOUSE-01703 — Blend the four seasonal transition matrices continuously from `SeasonPhase::blend` instead of selecting one by day-of-year
      dep: HOUSE-01543, HOUSE-01683 · sys: weather · plat: ALL · pri: MUST
      accept: (1) the effective archetype probabilities are the blend-weighted mix of the two
              neighbouring seasons; (2) crossing a season boundary changes no probability
              discontinuously; (3) determinism is preserved — the same seed still reproduces the
              same weather sequence
      verify: unit WeatherSeasonTests.BlendedMatrices
- [ ] HOUSE-01704 — Gate the archetypes on the measured outdoor temperature: `W_SNOW` impossible in summer, `W_THUNDERSTORM` probability rising with the summer temperature excess
      dep: HOUSE-01703, HOUSE-01545 · sys: weather · plat: ALL · pri: MUST
      accept: (1) over a full simulated year, `W_SNOW` is never selected while the outdoor
              temperature is above its threshold — **zero occurrences in summer**; (2) storm
              frequency correlates positively with summer temperature; (3) the gate is expressed
              through the temperature curve, not as a hardcoded month test
      verify: integration WeatherSeasonTests.NoSummerSnow over a 10-year headless run
- [ ] HOUSE-01702 — Phase-26 review and commit
      dep: HOUSE-01681…HOUSE-01701 · sys: — · plat: ALL · pri: MUST

---

## Phase 27 — Rain

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
- [ ] HOUSE-01747 — Implement water-surface ripples instead of splashes on water materials
      dep: HOUSE-01746 · sys: weather · plat: ALL · pri: SHOULD
- [ ] HOUSE-01748 — Implement the wet-material swap driven by `surfaceWetness` (Tier S)
      dep: HOUSE-01690, HOUSE-00905 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-01749 — Implement puddle decals at the height field's local minima above `wetness > 0.55`
      dep: HOUSE-01748 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-01750 — Implement the four rain audio layers (open air, roof, windows, downspouts) with their gain drivers
      dep: HOUSE-00286, HOUSE-00779 · sys: audio · plat: ALL · pri: MUST
      dep-note: full routing lands in phase 32; the gains are defined here
- [ ] HOUSE-01751 — Implement the open-window rain audio response (immediate and audible)
      dep: HOUSE-01750, HOUSE-01194 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01752 — Implement the open-window spray decal and the drying sill patch (completes HOUSE-01195)
      dep: HOUSE-01744, HOUSE-01195 · sys: weather · plat: ALL · pri: SHOULD
- [ ] HOUSE-01753 — Measure the rain system at intensity 1.0 against the particle and CPU budgets
      dep: HOUSE-01746 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-01754 — Render tests: light rain, heavy rain, rain seen from indoors through a window, rain under the porch, wet driveway
      dep: HOUSE-01749 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01755 — Test: no rain particle is ever drawn below a covered surface
      dep: HOUSE-01744 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01756 — Phase-27 review and commit
      dep: HOUSE-01741…HOUSE-01755 · sys: — · plat: ALL · pri: MUST

---

## Phase 28 — Snow

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
- [ ] HOUSE-01796 — Implement the snow ambience change (quieter, absorbed) and the exterior reverb hint change
      dep: HOUSE-01691 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01797 — Implement melt: `snowDepth` decay from temperature and direct sun, faster on dark materials
      dep: HOUSE-01691 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01798 — Implement snow on the car, the bins, the fence rails and the garden furniture via their shell entries
      dep: HOUSE-01793 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-01799 — Measure the snow system at intensity 1.0 against budget
      dep: HOUSE-01794 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-01800 — Render tests: light snow, blizzard, 5 cm accumulation, 30 cm accumulation, melt in progress
      dep: HOUSE-01797 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01802 — Implement accumulating seasonal snow cover on the garden, the roof, the fence and the cars, building and melting with the outdoor temperature rather than with the season index
      dep: HOUSE-01704, HOUSE-01793 · sys: weather · plat: ALL · pri: MUST
      accept: (1) cover builds over simulated hours of snowfall and melts over simulated hours
              above freezing; (2) a warm spell mid-winter visibly clears the drive; (3) the depth
              is saved; (4) the four surfaces accumulate independently
      verify: render test snow-cover-01..04; integration SnowCoverTests.MeltCycle
- [ ] HOUSE-01801 — Phase-28 review and commit
      dep: HOUSE-01791…HOUSE-01800 · sys: — · plat: ALL · pri: MUST

---

## Phase 29 — Storm, lightning and thunder

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
- [ ] HOUSE-01844 — Phase-29 review and commit
      dep: HOUSE-01831…HOUSE-01843 · sys: — · plat: ALL · pri: MUST

---

## Phase 30 — Hail and wind

- [ ] HOUSE-01871 — Implement hail particle motion: fast fall, small bright quads, motion-blur streak
      dep: HOUSE-01742 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01872 — Implement the hail bounce (0.35 restitution, randomised tangential, 0.5 s extra life)
      dep: HOUSE-01871, HOUSE-00777 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01873 — Implement the short-lived hail-stone ground scatter above intensity 0.5
      dep: HOUSE-01872 · sys: weather · plat: ALL · pri: SHOULD
- [ ] HOUSE-01874 — Implement hail audio: the density-driven impact layer plus roof, window and car layers
      dep: HOUSE-00286, HOUSE-01872 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01875 — Implement the hail-with-thunder-and-temperature-drop coupling in the archetype table
      dep: HOUSE-01688 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01876 — Implement the wind model: base + 3-octave gust noise + direction wander
      dep: HOUSE-01686, HOUSE-00027 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01877 — Implement vegetation sway: the 15 Hz CPU vertex pass for the ≤ 40 nearest instances, static beyond
      dep: HOUSE-01876, HOUSE-00772 · sys: weather · plat: ALL · pri: MUST
- [ ] HOUSE-01878 — Implement the gust-synchronised whole-canopy lean
      dep: HOUSE-01877 · sys: weather · plat: ALL · pri: SHOULD
- [ ] HOUSE-01879 — Implement grass and hedge sway (faster, smaller)
      dep: HOUSE-01877 · sys: weather · plat: ALL · pri: SHOULD
- [ ] HOUSE-01880 — Implement curtain sway at an open window (a 3-bone rigid sway)
      dep: HOUSE-01876 · sys: weather · plat: ALL · pri: OPT
      dep-note: needs the curtains of phase 45
- [ ] HOUSE-01881 — Implement wind-driven door motion and slamming (completes HOUSE-01187)
      dep: HOUSE-01876, HOUSE-01187 · sys: interaction · plat: ALL · pri: MUST
      accept: a door left open in a gale eventually slams, closing its portal — and the visibility system just follows
- [ ] HOUSE-01882 — Implement the gate rattle
      dep: HOUSE-01876 · sys: audio · plat: ALL · pri: SHOULD
- [ ] HOUSE-01883 — Implement the wind ambience gain and low-pass cross-fade by speed
      dep: HOUSE-01876, HOUSE-00290 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01884 — Implement the wind-through-an-open-window whistle scaled by aperture
      dep: HOUSE-01883, HOUSE-01194 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01885 — Implement the roof and eaves whistle in the attic cells
      dep: HOUSE-01883 · sys: audio · plat: ALL · pri: SHOULD
- [ ] HOUSE-01886 — Implement the weather vane on the shed pointing into the wind
      dep: HOUSE-01876 · sys: rendering · plat: ALL · pri: OPT
- [ ] HOUSE-01887 — Implement the swing bench and wind chimes ambient motion
      dep: HOUSE-01876 · sys: rendering · plat: ALL · pri: OPT
- [ ] HOUSE-01888 — Measure hail at intensity 1.0 and wind at 20 m/s against budget
      dep: HOUSE-01877 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-01889 — Render tests: a hail squall, trees at 4 wind speeds, a door mid-slam
      dep: HOUSE-01881 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01890 — Phase-30 review and commit
      dep: HOUSE-01871…HOUSE-01889 · sys: — · plat: ALL · pri: MUST

---

## Phase 31 — Audio foundation

- [ ] HOUSE-01911 — Implement `AudioSystem` proper: category volumes, master, mute, device-loss handling
      dep: HOUSE-00154 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01912 — Implement `EmitterPool`: up to 64 logical emitters with lifetime, position, category and priority
      dep: HOUSE-01911 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01913 — Implement `VoiceManager`: 32 concurrent instances, per-category limits, priority × audible-gain sorting, virtualisation
      dep: HOUSE-01912, HOUSE-00097 · sys: audio · plat: ALL · pri: MUST
      accept: a hail storm never starves the door the player just opened
      verify: unit VoiceManagerTests.* with a 200-emitter stress
- [ ] HOUSE-01914 — Implement the `AudioListener` update from the camera, with velocity pinned to zero
      dep: HOUSE-01911 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01915 — Implement `Apply3D` invocation per positional emitter, with `DopplerScale = 0` by default
      dep: HOUSE-01914, HOUSE-00096 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01916 — Implement the Doppler settings toggle and its warning text
      dep: HOUSE-01915, HOUSE-00131 · sys: audio · plat: ALL · pri: SHOULD
- [ ] HOUSE-01917 — Implement the sound bank loader from `layout.audio.json` and the manifest
      dep: HOUSE-01911, HOUSE-00279 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01918 — Implement `Bag<T>` round-robin sample selection with a no-repeat-within-4 rule, pitch ±4 % and volume ±10 %
      dep: HOUSE-01917, HOUSE-00027 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01919 — Implement `FootstepDirector`: the stride accumulator, surface lookup, per-surface bag, and the stair riser-crossing trigger
      dep: HOUSE-01918, HOUSE-00560 · sys: audio · plat: ALL · pri: MUST
      accept: cadence matches speed; stairs give one step per riser
- [ ] HOUSE-01920 — Wire the 20 footstep surface sets (12 from NOX, 8 sourced)
      dep: HOUSE-01919, HOUSE-00280, HOUSE-00281 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-01921 — Implement the third-person footstep path driven by the same accumulator, with a development-build cross-check against the clip's foot-plant markers
      dep: HOUSE-01919 · sys: audio · plat: ALL · pri: MUST
      dep-note: markers arrive in phase 37; the cross-check test is HOUSE-02230 plus HOUSE-01938
- [ ] HOUSE-01922 — Implement `AmbienceDirector`: per-cell room tone with an 0.8 s cross-fade on cell change
      dep: HOUSE-01917, HOUSE-00559 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01923 — Author and wire the 6 room-tone beds
      dep: HOUSE-01922 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-01924 — Implement the exterior ambience bed with time-of-day variation (dawn chorus, day traffic, evening crickets, night quiet)
      dep: HOUSE-01922, HOUSE-00290 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01925 — Implement the seasonal and weather layers over the exterior bed
      dep: HOUSE-01924 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01926 — Wire the interaction sound table: 62 doors × 4 classes, 84 switches, 214 containers, 17 taps, 13 toilets, 11 appliances
      dep: HOUSE-01917, HOUSE-01139 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-01927 — Implement the house's own sound emitters from the plumbing and HVAC data (furnace, blower, duct ticks, water heater, pipe hiss)
      dep: HOUSE-00387, HOUSE-01912 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01928 — Implement the thermostat cycle driving the furnace from the outdoor temperature
      dep: HOUSE-01927, HOUSE-01535 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01929 — Implement the house-creak system: 14 placed points, rate weighted by the rate of change of outdoor temperature
      dep: HOUSE-01927, HOUSE-00289 · sys: audio · plat: ALL · pri: SHOULD
- [ ] HOUSE-01930 — Implement the clocks: a tick per simulated minute in 3 rooms, and the hall clock's hourly chime
      dep: HOUSE-01927, HOUSE-01531 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01931 — Implement the appliance loops (fridge compressor, freezer, router, console, extractor, fans) from the NOX electromagnetic pack
      dep: HOUSE-01927, HOUSE-00279 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01932 — Implement the doorbell (debug command and one scripted event only)
      dep: HOUSE-01926, HOUSE-00289 · sys: audio · plat: ALL · pri: OPT
- [ ] HOUSE-01933 — Implement the garage-door motor, rail rumble and end clunk
      dep: HOUSE-01926, HOUSE-01189 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01934 — Implement the `F7` audio overlay (voices by category, emitters, gains)
      dep: HOUSE-01913, HOUSE-00147 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-01935 — Implement the audio settings tab and its live application
      dep: HOUSE-01911, HOUSE-00131 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01936 — Measure the audio update cost against the 0.25/0.60 ms budget
      dep: HOUSE-01913 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-01937 — Test: 32 concurrent voices are never exceeded; the 33rd is virtualised, not dropped
      dep: HOUSE-01913 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01938 — Test: footstep cadence matches speed within 5 % across all speeds and surfaces
      dep: HOUSE-01919 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-01939 — Listening review: walk the house and list every sound that is wrong, missing or too loud; fix
      dep: HOUSE-01931 · sys: audio · plat: LNX · pri: MUST
- [ ] HOUSE-01941 — Make the exterior ambience bed seasonal: spring birdsong and dawn chorus, summer insects, autumn wind in bare branches, winter muffled stillness — cross-faded on `SeasonPhase`
      dep: HOUSE-01543, HOUSE-01925 · sys: audio · plat: ALL · pri: MUST
      accept: (1) the bed is the blend-weighted mix of the two neighbouring seasons, cross-faded
              rather than switched; (2) birdsong is absent in winter and densest in spring;
              (3) it respects the room-aware attenuation of §64 — the bed is quieter indoors
      verify: manual listen across one simulated year; unit AmbienceTests.SeasonalWeights
- [ ] HOUSE-01940 — Phase-31 review and commit
      dep: HOUSE-01911…HOUSE-01939 · sys: — · plat: ALL · pri: MUST

---

## Phase 32 — Room-aware 3-D audio

- [ ] HOUSE-01991 — Implement `PortalPathSolver`: Dijkstra from the listener cell over the portal graph with air, transmission and floor costs
      dep: HOUSE-01912, HOUSE-00665 · sys: audio · plat: ALL · pri: MUST
      files: src/audio/PortalPath.cpp|hpp
      verify: unit PortalPathTests.* on hand-computed 3-cell and 6-cell cases
- [ ] HOUSE-01992 — Implement the transmission-loss table of `cna-house.md` §64.3 as data in `layout.audio.json`
      dep: HOUSE-00388 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-01993 — Implement the aperture-interpolated transmission loss with the smoothstep
      dep: HOUSE-01992, HOUSE-01991 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01994 — Implement the 60 dB cost ceiling and emitter virtualisation beyond it
      dep: HOUSE-01991, HOUSE-01913 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01995 — **Implement apparent-position repositioning**: an emitter outside the listener's cell is placed at the first portal on the cheapest path
      dep: HOUSE-01991, HOUSE-01915 · sys: audio · plat: ALL · pri: MUST
      accept: a television in the family room, heard from the hall, is heard *at the doorway*; walking past the doorway sweeps it across the stereo field
- [ ] HOUSE-01996 — Implement the through-the-wall fallback: direct distance plus 40 dB per crossed boundary, up to 3
      dep: HOUSE-01991 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01997 — Implement the re-solve trigger policy (listener cell change, aperture change > 0.05, emitter cell change) and the intra-cell distance-only update
      dep: HOUSE-01991 · sys: audio · plat: ALL · pri: MUST
      accept: measured solver cost ≤ 60 µs per frame in the worst case
- [ ] HOUSE-01998 — Implement the bright/dull two-instance cross-fade for the 22 muffle-critical sounds
      dep: HOUSE-00221, HOUSE-01991 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-01999 — Implement the −4 dB-per-unit muffle stand-in for everything else
      dep: HOUSE-01998 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-02000 — Implement sky-exposure-driven ambience routing for the weather layers
      dep: HOUSE-00779, HOUSE-01925 · sys: audio · plat: ALL · pri: MUST
      accept: `L3_STORE_W` gets full rain-on-roof; `B1_CINEMA` gets essentially nothing; an open slider in the sunroom nearly matches outdoors
- [ ] HOUSE-02001 — Implement the pre-reverberated variant selection from the cell's `reverbHint` for the 30 transient sounds
      dep: HOUSE-00220, HOUSE-01991 · sys: audio · plat: ALL · pri: SHOULD
- [ ] HOUSE-02002 — Wire the TV, thunder, rain, water and pet audio through the room-aware model (completes HOUSE-01496, 01839, 01750)
      dep: HOUSE-01995 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-02003 — Implement the `F7` overlay's path visualisation (the solved path drawn as a line, with per-portal losses)
      dep: HOUSE-01991, HOUSE-01934 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02004 — Test: closing a door between the listener and an emitter reduces its gain by the tabled amount
      dep: HOUSE-01993 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02005 — Test: apparent position is the portal centre when the emitter is not in the listener's cell
      dep: HOUSE-01995 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02006 — Test: with every door closed, an emitter in the basement is audible only through the wall fallback
      dep: HOUSE-01996 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02007 — Measure the solver against its budget in the worst case (64 emitters, 95 cells)
      dep: HOUSE-01997 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-02008 — Listening review: the 12 signature audio situations of §64 and §62.6, each auditioned and tuned
      dep: HOUSE-02002 · sys: audio · plat: LNX · pri: MUST
- [ ] HOUSE-02009 — Phase-32 review and commit
      dep: HOUSE-01991…HOUSE-02008 · sys: — · plat: ALL · pri: MUST

---

## Phase 33 — Dog

- [ ] HOUSE-02041 — Acquire or build the dog model to the acceptance bar; complete the visual sign-off
      dep: HOUSE-00291, HOUSE-00298 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02042 — Rig the dog (38 bones) and verify the bone list, weights (≤ 4 influences) and scale
      dep: HOUSE-02041 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02043 — Author or retarget the 8 dog clips (idle, sit, lie, walk, trot, trot_stairs, bark, eat)
      dep: HOUSE-02042 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02044 — Build the dog through the pipeline to `.cnb`, extract its `.chanim` sidecar, and verify the skeleton and clip list bind to the model (`check_anim_assets.py`)
      dep: HOUSE-02043, HOUSE-00223, HOUSE-00225 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02045 — Implement `Pet` base: transform, capsule, cell tracking, model, `ClipPlayer`
      dep: HOUSE-02044 · sys: animals · plat: ALL · pri: MUST
      dep-note: `ClipPlayer` arrives in phase 37; a single-clip player suffices until then
- [ ] HOUSE-02046 — Implement `NavGraph` loading and A* with the portal-aperture check
      dep: HOUSE-00389, HOUSE-02045 · sys: animals · plat: ALL · pri: MUST
      accept: a closed door blocks the path; the dog goes to the door and waits
- [ ] HOUSE-02047 — Implement steering: seek with arrival, obstacle avoidance against the cell's OBBs, capsule collision
      dep: HOUSE-02046, HOUSE-00543 · sys: animals · plat: ALL · pri: MUST
- [ ] HOUSE-02048 — Implement stair traversal on the ramps with the `trot_stairs` clip and reduced speed
      dep: HOUSE-02047, HOUSE-00560 · sys: animals · plat: ALL · pri: MUST
- [ ] HOUSE-02049 — Implement `DogBrain`: the utility scorer, the three drives (arousal, affection, boredom) with decay, minimum dwell and hysteresis
      dep: HOUSE-02047 · sys: animals · plat: ALL · pri: MUST
- [ ] HOUSE-02050 — Implement the 12 dog states of `cna-house.md` §60.2 with their scoring
      dep: HOUSE-02049 · sys: animals · plat: ALL · pri: MUST
- [ ] HOUSE-02051 — Implement the alert triggers (door opening, doorbell, thunder, the cat)
      dep: HOUSE-02050 · sys: animals · plat: ALL · pri: MUST
- [ ] HOUSE-02052 — Implement dog audio: bark ×4, whine, growl, pant loop, sigh, eat, drink, all through the room-aware model
      dep: HOUSE-00287, HOUSE-01995 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-02053 — Implement the collar-tag jingle and the surface-aware nail clicks synchronised to the gait
      dep: HOUSE-02052, HOUSE-01919 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-02054 — Implement the dog's blob shadow and its LOD
      dep: HOUSE-02045, HOUSE-01267 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-02055 — Implement dog persistence (cell, position, yaw, state, timer, drives, bed)
      dep: HOUSE-02050 · sys: persistence · plat: ALL · pri: MUST
      dep-note: schema in phase 39
- [ ] HOUSE-02056 — Implement the console `pet dog state <state>` and `pet dog goto <cell>` commands
      dep: HOUSE-02050 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02057 — Test: the dog never walks through a wall, furniture or a closed door (a 30-minute simulated soak)
      dep: HOUSE-02047 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02058 — Test: the dog paths from `L0_FAMILY` to `L1_BED2` only when the doors on the way are open
      dep: HOUSE-02046 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02059 — Render tests: the dog in 4 states, 2 lighting conditions
      dep: HOUSE-02054 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02060 — Behaviour review: does it read as a dog rather than a state machine? Tune the drives.
      dep: HOUSE-02059 · sys: animals · plat: LNX · pri: MUST
- [ ] HOUSE-02061 — Phase-33 review and commit
      dep: HOUSE-02041…HOUSE-02060 · sys: — · plat: ALL · pri: MUST

---

## Phase 34 — Cat

- [ ] HOUSE-02091 — Acquire or build the cat model to the acceptance bar; complete the visual sign-off
      dep: HOUSE-00292, HOUSE-00298 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02092 — Rig the cat (34 bones) and verify
      dep: HOUSE-02091 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02093 — Author or retarget the 11 cat clips
      dep: HOUSE-02092 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02094 — Build the cat through the pipeline and verify
      dep: HOUSE-02093 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02095 — Implement `CatBrain` over the shared `Pet` base, with its 13 states
      dep: HOUSE-02094, HOUSE-02049 · sys: animals · plat: ALL · pri: MUST
- [ ] HOUSE-02096 — Implement the perch system: 22 perch points, `jump_up`/`jump_down`, occupancy
      dep: HOUSE-02095, HOUSE-00389 · sys: animals · plat: ALL · pri: MUST
      accept: the cat is *on* things, not beside them — the single most important cat behaviour
- [ ] HOUSE-02097 — Implement the `Avoid` drive (keeps 1.5 m unless affection is high; moves away from a fast approach)
      dep: HOUSE-02095 · sys: animals · plat: ALL · pri: MUST
- [ ] HOUSE-02098 — Implement `Groom`, `Knead` and `Stretch` idles
      dep: HOUSE-02095 · sys: animals · plat: ALL · pri: SHOULD
- [ ] HOUSE-02099 — Implement `Hunt`: a fixed prey point, stalk, crouch, tail twitch, lose interest
      dep: HOUSE-02095 · sys: animals · plat: ALL · pri: OPT
- [ ] HOUSE-02100 — Implement the dog↔cat awareness (3 m): the cat leaves a room the dog enters unless perched; the dog occasionally follows
      dep: HOUSE-02097, HOUSE-02050 · sys: animals · plat: ALL · pri: MUST
- [ ] HOUSE-02101 — Implement cat audio: meow ×5, chirrup, purr loop near the player, hiss at the dog, soft paw falls on hard surfaces only, the door-frame scratch
      dep: HOUSE-00288, HOUSE-01995 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-02102 — Implement cat persistence including the current perch
      dep: HOUSE-02095 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02103 — Test: the cat never walks through geometry; perch transitions never leave it inside a prop
      dep: HOUSE-02096 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02104 — Render tests: the cat on 4 perches, 2 lighting conditions
      dep: HOUSE-02096 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02105 — Behaviour review and tuning
      dep: HOUSE-02104 · sys: animals · plat: LNX · pri: MUST
- [ ] HOUSE-02106 — Phase-34 review and commit
      dep: HOUSE-02091…HOUSE-02105 · sys: — · plat: ALL · pri: MUST

---

## Phase 35 — Third-person avatar

- [ ] HOUSE-02131 — Build the two base bodies through the pipeline, extract their `.chanim` sidecars, and verify the skeletons and bone lists bind to the models (`check_anim_assets.py`)
      dep: HOUSE-00294, HOUSE-00223, HOUSE-00225 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02132 — Implement `PlayerAvatar`: the model set, the shared bone palette, and the multi-mesh draw
      dep: HOUSE-02131 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-02133 — Implement the load-time bone-list compatibility check across all customisation meshes
      dep: HOUSE-02132 · sys: player · plat: ALL · pri: MUST
      accept: a mismatched mesh is rejected with a clear diagnostic, never silently deformed
- [ ] HOUSE-02134 — Implement the avatar's world transform from the controller, with the 0.18 s turn blend toward the movement direction
      dep: HOUSE-02132 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-02135 — Implement `ThirdPersonCamera`: the spring arm, desired distance and pitch, sphere-swept collision
      dep: HOUSE-02134, HOUSE-00545 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-02136 — Implement the narrow-space over-the-shoulder mode and the sub-0.7 m first-person fallback with blends
      dep: HOUSE-02135 · sys: player · plat: ALL · pri: MUST
      accept: corridors and closets never trap the camera
- [ ] HOUSE-02137 — Implement the avatar alpha fade below 0.9 m and cull below 0.5 m
      dep: HOUSE-02136 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-02138 — Implement the shoulder-side switch and its setting
      dep: HOUSE-02135, HOUSE-00131 · sys: player · plat: ALL · pri: SHOULD
- [ ] HOUSE-02139 — Implement the stair-specific camera damping
      dep: HOUSE-02135, HOUSE-00560 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-02140 — Implement the `V` camera-mode toggle with a 0.3 s blend between the two cameras
      dep: HOUSE-02136 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-02141 — Implement the avatar's blob shadow and its LOD
      dep: HOUSE-02132, HOUSE-01267 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-02142 — Implement the third-person interaction ray from the avatar's eye (completes HOUSE-01129)
      dep: HOUSE-02134, HOUSE-01129 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-02143 — Test: the camera never enters static geometry across a scripted tour of every cell
      dep: HOUSE-02136 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02144 — Test: what is interactable does not change between camera modes
      dep: HOUSE-02142 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02145 — Render tests: third person in 8 locations including two corridors and a closet
      dep: HOUSE-02141 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02146 — Phase-35 review and commit
      dep: HOUSE-02131…HOUSE-02145 · sys: — · plat: ALL · pri: MUST

---

## Phase 36 — Character customisation

- [ ] HOUSE-02171 — Author the 6 skin-tone textures per sex (12 total) sharing UVs
      dep: HOUSE-02131 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02172 — Author the 3 face-variant head meshes per sex (6 total) sharing the skeleton
      dep: HOUSE-02131 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02173 — Author the 6 hair meshes with alpha-tested cards and a scalp cap
      dep: HOUSE-02131 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02174 — Implement hair rendering: the scalp with `SkinnedEffect`, the cards rigid-transformed by the head bone with `AlphaTestEffect`
      dep: HOUSE-02173, HOUSE-00894 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-02175 — Author the 8 hair colour tints
      dep: HOUSE-02174 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02176 — Author the 5 top meshes per sex (10) and their 8 tints
      dep: HOUSE-02131 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02177 — Author the 4 bottom meshes per sex (8) and their tints
      dep: HOUSE-02131 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02178 — Author the 3 shoe meshes per sex (6) and their tints
      dep: HOUSE-02131 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02179 — Implement `AvatarConfig` and its application to the draw set
      dep: HOUSE-02176…HOUSE-02178 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-02180 — Implement the customisation screen: a rotating avatar on a pedestal under the house's own lighting, slot arrows, colour strip
      dep: HOUSE-02179, HOUSE-00156 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-02181 — Implement mid-game customisation (the pause menu does not freeze the world)
      dep: HOUSE-02180 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-02182 — Implement avatar persistence in settings and mirrored in the save
      dep: HOUSE-02179 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02183 — Implement the default configuration of §46.3 for deterministic tests and screenshots
      dep: HOUSE-02179 · sys: player · plat: ALL · pri: MUST
- [ ] HOUSE-02184 — Test: all 38 meshes share the body's bone list name-for-name
      dep: HOUSE-02133 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02185 — Test: 20 random configurations render without a missing mesh, a z-fighting overlap or a clipping garment
      dep: HOUSE-02179 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02186 — Render tests: 6 customisations in 4 poses
      dep: HOUSE-02185 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02187 — Phase-36 review: does the character look like a person? Sign off.
      dep: HOUSE-02186 · sys: — · plat: LNX · pri: MUST

---

## Phase 37 — Character animation

- [ ] HOUSE-02211 — Implement TRS handling of the loaded clip keyframes at load time (the `.chanim` sidecar already stores TRS; this is the validation and the fallback decomposition for any matrix-valued track)
      dep: HOUSE-00167 · sys: animation · plat: ALL · pri: MUST
      accept: decomposition round-trips to the original matrices within 1e-5
- [ ] HOUSE-02212 — Implement `ClipPlayer`: single-track evaluation producing local transforms
      dep: HOUSE-02211 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02213 — Implement world-transform composition and skin-transform production (inverse bind pose × world)
      dep: HOUSE-02212 · sys: animation · plat: ALL · pri: MUST
      verify: unit ClipPlayerTests.IdentityPaletteIsNoOp, matching the SAMPLE-054 precedent
- [ ] HOUSE-02214 — Implement two-track cross-fading with `Vector3::Lerp` + `Quaternion::Slerp`
      dep: HOUSE-02213 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02215 — Implement the upper-body mask layer by bone index
      dep: HOUSE-02214 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02216 — Implement clip rate scaling (`SetRate`) and looping/one-shot handling with end events
      dep: HOUSE-02212 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02217 — Implement `AnimationSet`: the named clip table per actor, built from the actor's `anim::ClipLibrary` (never from `Model::Tag`)
      dep: HOUSE-02212, HOUSE-00167 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02218 — Wire `ClipPlayer` into `SkinnedEffect::SetBoneTransforms` for the avatar and both pets
      dep: HOUSE-02213, HOUSE-00895 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02219 — Author or retarget the 18 player clips (both sexes where the gait differs)
      dep: HOUSE-00295, HOUSE-02131 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02220 — Measure and store each locomotion clip's stride length (`measure_stride.py`)
      dep: HOUSE-02219, HOUSE-00194 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02221 — Implement the locomotion state machine (idle ↔ locomotion ↔ turn ↔ stairs ↔ crouch ↔ landing)
      dep: HOUSE-02217, HOUSE-02134 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02222 — Implement the 2-D velocity blend across walk forward/back/left/right and fast walk
      dep: HOUSE-02221, HOUSE-02214 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02223 — Implement stride matching: `rate = speed / (strideLength / duration)`, clamped, with a clip switch outside the clamp
      dep: HOUSE-02222, HOUSE-02220 · sys: animation · plat: ALL · pri: MUST
      accept: no visible foot sliding at any speed
      verify: a test measuring foot-contact-point drift over 40 frames
- [ ] HOUSE-02224 — Implement in-place turn clips when the avatar rotates without translating
      dep: HOUSE-02221 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02225 — Implement the crouch state for the attic
      dep: HOUSE-02221, HOUSE-00558 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02226 — Implement landing clips with soft/hard selection
      dep: HOUSE-02221, HOUSE-00552 · sys: animation · plat: ALL · pri: SHOULD
- [ ] HOUSE-02227 — Implement the `idle_look` occasional idle after 8 s stationary
      dep: HOUSE-02221 · sys: animation · plat: ALL · pri: SHOULD
- [ ] HOUSE-02228 — Implement the masked interaction clips (`reach_low/mid/high`, `open_door`) triggered by the interaction system
      dep: HOUSE-02215, HOUSE-01132 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02229 — Implement the sit/stand clips for seats and the toilet (completes HOUSE-01464)
      dep: HOUSE-02221, HOUSE-01138 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02230 — Implement foot-plant markers per clip and their export
      dep: HOUSE-02219 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02231 — Implement the pets' clip players over the same `ClipPlayer` (replacing the phase-33 single-clip stand-in)
      dep: HOUSE-02214, HOUSE-02045 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02232 — Implement LOD for animated actors (mesh decimation only; the bone count is unchanged)
      dep: HOUSE-02218 · sys: animation · plat: ALL · pri: SHOULD
- [ ] HOUSE-02233 — Measure the animation cost against the 0.45/1.00 ms budget with 3 actors
      dep: HOUSE-02231 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-02234 — Test: blending between any two clips never produces a NaN, a flipped quaternion or a scale collapse
      dep: HOUSE-02214 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02235 — Test: the mask layer affects exactly the declared bones and no others
      dep: HOUSE-02215 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02236 — Render tests: 10 animation states, both sexes
      dep: HOUSE-02229 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02237 — Animation review: walk every room and watch the avatar; list and fix what looks wrong
      dep: HOUSE-02236 · sys: animation · plat: LNX · pri: MUST
- [ ] HOUSE-02238 — Phase-37 review and commit
      dep: HOUSE-02211…HOUSE-02237 · sys: — · plat: ALL · pri: MUST

---

## Phase 38 — Stair animation and foot IK

- [ ] HOUSE-02271 — Implement stair detection from the surface kind and slope, with the 15° threshold and 0.2 s blend
      dep: HOUSE-02221, HOUSE-00560 · sys: animation · plat: ALL · pri: MUST
      accept: **the stair clips are used, never a diagonally-translated walk cycle**
- [ ] HOUSE-02272 — Implement along-slope rate matching for the stair clips
      dep: HOUSE-02271, HOUSE-02223 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02273 — Implement the two-bone analytic IK (hip, knee, ankle) with swing-plane preservation
      dep: HOUSE-02213 · sys: animation · plat: ALL · pri: MUST
      verify: unit FootIkTests.* against 200 analytic targets, incl. fully extended and fully folded
- [ ] HOUSE-02274 — Implement the per-foot downward ray, the ±0.12 m clamp and the 0.08 s smoothing
      dep: HOUSE-02273, HOUSE-00546 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02275 — Implement foot rotation to the surface normal, clamped to 25°
      dep: HOUSE-02274 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02276 — Enable IK on stairs and slopes; disable in mid-air and during in-place turns
      dep: HOUSE-02275 · sys: animation · plat: ALL · pri: MUST
- [ ] HOUSE-02277 — Implement the same IK for the dog and cat on stairs
      dep: HOUSE-02276, HOUSE-02231 · sys: animation · plat: ALL · pri: SHOULD
- [ ] HOUSE-02278 — Implement the stair footstep cadence by riser crossing (completes HOUSE-01919)
      dep: HOUSE-02272, HOUSE-01919 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-02279 — Wire the stair-specific footstep surface sets (wood, carpeted, open-riser, stone, concrete)
      dep: HOUSE-02278, HOUSE-00281 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-02280 — Test: on every flight, each foot's contact point lies on a tread within 3 cm
      dep: HOUSE-02276 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02281 — Test: no foot ever penetrates a tread or floats above one by more than 2 cm
      dep: HOUSE-02280 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02282 — Render tests: ascending and descending each of the 4 interior flights, third person
      dep: HOUSE-02276 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02283 — Stair review: watch all 7 flights in both directions in both camera modes; tune
      dep: HOUSE-02282 · sys: animation · plat: LNX · pri: MUST
- [ ] HOUSE-02284 — Phase-38 review and commit
      dep: HOUSE-02271…HOUSE-02283 · sys: — · plat: ALL · pri: MUST

---

## Phase 39 — Persistence

- [ ] HOUSE-02301 — Define the save schema v1 formally (`docs/save-format.md`) with every field and its type and range
      dep: HOUSE-00014 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02302 — Implement `SaveModel`: the in-memory representation matching the schema
      dep: HOUSE-02301 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02303 — Implement canonical serialisation (stable key order, fixed float formatting) so the checksum is reproducible
      dep: HOUSE-02302, HOUSE-00028 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02304 — Implement the envelope: format, version, timestamps, `gameVersion`, `worldHash`, `checksum`
      dep: HOUSE-02303, HOUSE-00364 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02305 — Implement the delta computation against the canonical initial state
      dep: HOUSE-02302, HOUSE-00395 · sys: persistence · plat: ALL · pri: MUST
      accept: a fresh house saves ≤ 6 KB; adding a new interactable does not invalidate old saves
- [ ] HOUSE-02306 — Implement player-block serialisation (cell, position, orientation, modes, held item, avatar)
      dep: HOUSE-02305 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02307 — Implement clock serialisation (completes HOUSE-01538)
      dep: HOUSE-02305, HOUSE-01538 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02308 — Implement weather serialisation including the RNG state and the transition bookkeeping
      dep: HOUSE-02305, HOUSE-01692 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02309 — Implement interactable-delta serialisation via each behaviour's `Serialise`/`Deserialise`
      dep: HOUSE-02305, HOUSE-01122 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02310 — Implement the quantisation rules (openFraction 1/64, container 1/32, timers 0.1 s, playhead 1 s)
      dep: HOUSE-02309 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02311 — Implement pet serialisation (completes HOUSE-02055, HOUSE-02102)
      dep: HOUSE-02305, HOUSE-02055, HOUSE-02102 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02312 — Implement stats serialisation
      dep: HOUSE-02305 · sys: persistence · plat: ALL · pri: SHOULD
- [ ] HOUSE-02313 — Implement `RebuildDerivedState()`: the single code path from a state snapshot to a running world
      dep: HOUSE-02309 · sys: persistence · plat: ALL · pri: MUST
      accept: load and *Reset House* both go through it, so they can never drift apart
- [ ] HOUSE-02314 — Implement atomic write (temp, flush, rename) with the `.bak` rotation
      dep: HOUSE-02304, HOUSE-00152 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02315 — Implement load with checksum, schema and range validation
      dep: HOUSE-02314 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02316 — Implement the corruption policy of §65.5 (bak fallback, clear errors, per-field clamping, unknown-id skip)
      dep: HOUSE-02315 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02317 — Implement the `worldHash`-mismatch policy (load anyway, drop missing ids, canonical for new ids, respawn if the cell is gone)
      dep: HOUSE-02316 · sys: persistence · plat: ALL · pri: MUST
      accept: this is the *normal* development case and must be graceful
- [ ] HOUSE-02318 — Implement the migration framework and the version policy (older migrates, newer refuses)
      dep: HOUSE-02315 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02319 — Write the first migration (v1→v2) as a worked example plus its fixture, even though v1 is current
      dep: HOUSE-02318 · sys: persistence · plat: ALL · pri: MUST
      accept: the mechanism is proved before anything depends on it (R-15)
- [ ] HOUSE-02320 — Implement autosave triggers (quit, pause menu, every 5 real minutes, cell change after 60 s)
      dep: HOUSE-02314 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02321 — Implement the save toast and the `F9` quick save (development builds)
      dep: HOUSE-02320 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-02322 — Implement the 3-slot option and the slot-selection UI
      dep: HOUSE-02320, HOUSE-00131 · sys: ui · plat: ALL · pri: SHOULD
- [ ] HOUSE-02323 — Implement the emergency `crash-save.json` path (completes HOUSE-00153)
      dep: HOUSE-02314, HOUSE-00153 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02324 — Implement the `F11` save overlay (delta, entity count, size, last save, version)
      dep: HOUSE-02305, HOUSE-00147 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02325 — Implement the console `save` and `load` commands
      dep: HOUSE-02320 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02326 — Test: every persistent field of every behaviour round-trips exactly
      dep: HOUSE-02309 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02327 — Test: the 12 named save/load scenarios of `cna-house.md` §70.3 (tap, bath, toilet, fridge item, TV, doors, lights, windows, pets, clock, weather, player)
      dep: HOUSE-02313 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02328 — Test: a save made before an interactable existed loads correctly and gives the new entity its canonical state
      dep: HOUSE-02317 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02329 — Test: **fuzz** — 5 000 mutations of a real save; the game always loads or reports a clean error, never crashes or hangs
      dep: HOUSE-02316 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02330 — Test: a save cycle takes under 8 ms
      dep: HOUSE-02314 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02331 — Test: 200 consecutive autosave cycles leave no leak and no corruption
      dep: HOUSE-02320 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02332 — Write the test-fixture regeneration script so schema changes do not invalidate the suite by hand
      dep: HOUSE-02319 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02333 — Phase-39 review and commit
      dep: HOUSE-02301…HOUSE-02332 · sys: — · plat: ALL · pri: MUST

---

## Phase 40 — Reset House

- [ ] HOUSE-02371 — Implement `ResetHouse(scope)` over `RebuildDerivedState()`
      dep: HOUSE-02313 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02372 — Implement the two scopes: *Reset Everything* and *Reset Objects Only*
      dep: HOUSE-02371 · sys: persistence · plat: ALL · pri: MUST
- [ ] HOUSE-02373 — Implement the confirmation dialogue naming exactly what will be lost, with the default on *Reset Everything*
      dep: HOUSE-02372, HOUSE-00156 · sys: ui · plat: ALL · pri: MUST
      accept: it is never bound to a key and never happens automatically
- [ ] HOUSE-02374 — Implement the post-reset toast and the immediate autosave
      dep: HOUSE-02373 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-02375 — Implement the console `reset house [everything|objects]` command
      dep: HOUSE-02372 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02376 — Test: 300 randomised interactions across every behaviour, then reset, produce a state byte-identical to a fresh game
      dep: HOUSE-02372 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02377 — Test: reset → save → reload is identical
      dep: HOUSE-02376 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02378 — Test: *Reset Objects Only* leaves the clock and the RNG state untouched to the last bit
      dep: HOUSE-02372 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02379 — Test: settings are never reset by either scope
      dep: HOUSE-02372 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02380 — Phase-40 review and commit
      dep: HOUSE-02371…HOUSE-02379 · sys: — · plat: ALL · pri: MUST

---

## Phase 41 — Culling and LOD optimisation

- [ ] HOUSE-02391 — Implement the LOD selector with the projected-height metric and hysteresis
      dep: HOUSE-00674 · sys: visibility · plat: ALL · pri: MUST
      verify: unit LodTests.* proving no oscillation at a boundary over a 600-frame approach
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
- [ ] HOUSE-02399 — Optimise the state tracker and measure the state-change reduction
      dep: HOUSE-00158, HOUSE-02398 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-02400 — Evaluate `DrawInstancedPrimitives` for vegetation and the neighbourhood; adopt if it measures better (from HOUSE-00093)
      dep: HOUSE-00093, HOUSE-02395 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-02401 — **Q-08 experiment: `OcclusionQuery` for exterior occlusion.** Queries around the 24 neighbourhood LOD groups, a 60-second recorded camera path, A/B GPU timing
      dep: HOUSE-01567, HOUSE-00852 · sys: rendering · plat: LNX · pri: OPT
      accept: adopt only if it saves ≥ 0.6 ms GPU at no visual cost; otherwise record the measurement and drop the idea
- [ ] HOUSE-02402 — Implement the 10 performance scenarios as a runnable harness
      dep: HOUSE-00150 · sys: ci · plat: CI · pri: MUST
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
- [ ] HOUSE-02409 — Phase-41 review and commit
      dep: HOUSE-02391…HOUSE-02408 · sys: — · plat: ALL · pri: MUST

---

## Phase 42 — Streaming and loading

**Gated.** `HOUSE-02451` decides whether this phase runs at all.

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
- [ ] HOUSE-02460 — Test: no frame during a scripted whole-house tour exceeds the frame budget because of a promotion
      dep: HOUSE-02454 · sys: ci · plat: CI · pri: SHOULD
- [ ] HOUSE-02461 — Test: walking back and forth through a door 100 times causes no thrash (the grace period holds)
      dep: HOUSE-02452 · sys: ci · plat: CI · pri: SHOULD
- [ ] HOUSE-02462 — **Q-07 decision: is a background loading thread needed?** Measure with residency in place; if promotions still hitch, plan the thread; otherwise record the decision not to
      dep: HOUSE-02460 · sys: — · plat: LNX · pri: SHOULD
- [ ] HOUSE-02463 — Phase-42 review and commit (or record the skip)
      dep: HOUSE-02451 · sys: — · plat: ALL · pri: MUST

---

## Phase 43 — Debug tools and settings

- [ ] HOUSE-02501 — Complete the `F1` performance overlay (GPU timing where available, per-pass draw counts)
      dep: HOUSE-00150 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02502 — Complete the `F2` world overlay
      dep: HOUSE-00631, HOUSE-01140 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02503 — Complete the `F3`/`F4`/`F5` visibility overlays
      dep: HOUSE-00683 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02504 — Complete the `F6` lighting overlay
      dep: HOUSE-01271 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02505 — Complete the `F7` audio overlay with the path visualisation
      dep: HOUSE-02003 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02506 — Complete the `F8` environment overlay
      dep: HOUSE-01616, HOUSE-01695 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02507 — Complete the `F9` physics overlay with the pets' capsules and paths
      dep: HOUSE-00562, HOUSE-02047 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02508 — Complete the `F10` content overlay
      dep: HOUSE-02459 · sys: debug · plat: ALL · pri: SHOULD
- [ ] HOUSE-02509 — Complete the `F11` save overlay
      dep: HOUSE-02324 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02510 — Implement the console with history, completion and the full command set of `cna-house.md` §69
      dep: HOUSE-00145 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02511 — Implement the screenshot harness driven by a named-pose JSON, with fixed time and weather
      dep: HOUSE-00151, HOUSE-01694 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02512 — Implement `nav draw` and the pet-path visualisation
      dep: HOUSE-02046 · sys: debug · plat: ALL · pri: SHOULD
- [ ] HOUSE-02513 — Implement `validate world` as a runtime command
      dep: HOUSE-00357 · sys: debug · plat: ALL · pri: SHOULD
- [ ] HOUSE-02514 — Implement `budget report` writing the current frame's numbers against the budget table
      dep: HOUSE-02403 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-02515 — Verify every debug tool compiles out with `CNAHOUSE_DEBUG_TOOLS=OFF` and costs nothing in a release build
      dep: HOUSE-02501…HOUSE-02514 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02516 — Implement the settings menu shell with the 5 tabs and keyboard-only navigation
      dep: HOUSE-00156, HOUSE-00131 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-02517 — Implement the Display tab (resolution, window mode, v-sync, frame cap, FOV, UI scale)
      dep: HOUSE-02516 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-02518 — Implement the Graphics tab, with options filtered by the project-owned **effective feature set** (`cna-house.md` §68) — `RenderTier` + build/platform profile + validated standard-XNA behaviour, all of it known to the application already
      dep: HOUSE-02516, HOUSE-00160 · sys: ui · plat: ALL · pri: MUST
      accept: no meaningless toggle is ever shown; a build whose profile cannot draw shadow maps does not offer them; the tab calls `SupportsCapability()` or another CNA extension query nowhere, and `check_xna_only.py` passes on the UI sources
- [ ] HOUSE-02519 — Implement the Audio tab with live application
      dep: HOUSE-02516, HOUSE-01935 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-02520 — Implement the Controls tab including full key remapping through `IInputSource`
      dep: HOUSE-02516, HOUSE-00140 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-02521 — Implement the Simulation tab (day length, weather mode, moon speed, location, pets, pause-on-menu, slots)
      dep: HOUSE-02516 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-02522 — Implement settings validation, clamping and migration
      dep: HOUSE-02521, HOUSE-00131 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-02523 — Implement the main menu, the pause menu and the credits screen (showing `THIRD-PARTY-ASSETS.md`)
      dep: HOUSE-02516, HOUSE-00198 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-02524 — Implement the pause-on-menu setting and its default (D-29)
      dep: HOUSE-02523 · sys: ui · plat: ALL · pri: MUST
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
- [ ] HOUSE-02531 — Phase-43 review and commit
      dep: HOUSE-02501…HOUSE-02530 · sys: — · plat: ALL · pri: MUST

---

## Phase 44 — Automated tests

**Goal.** Bring the suite to the shape of `cna-house.md` §70: ~900 unit, ~220 integration,
~120 render, 10 performance, plus the static gates. Many of these tests were written alongside
their features; this phase closes the gaps and makes the suite a first-class artefact.

- [ ] HOUSE-02571 — Audit the existing suite against §70 and produce a gap list
      dep: HOUSE-02531 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02572 — Grouped: complete the world-data unit tests (schemas, loader, ids, adjacency, plane membership, stair maths)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02573 — Grouped: complete the visibility unit tests (clip, reduce, containment property, area cutoff, depth caps, hysteresis)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02574 — Grouped: complete the collision unit tests (every primitive against analytic answers, 200 cases each)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02575 — Grouped: complete the save unit tests (round trip, delta, migration, corruption, fuzz, quantisation)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02576 — Grouped: complete the weather unit tests (rate limits, determinism, seasons, type derivation, integration channels)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02577 — Grouped: complete the astronomy unit tests (sun, moon, phase, sidereal, twilight)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02578 — Grouped: complete the interaction unit tests (predicates, effects, targeting, stickiness, all 12 behaviours)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02579 — Grouped: complete the animation unit tests (blend, mask, stride, IK, TRS round trip)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02580 — Grouped: complete the audio unit tests (portal path, voice manager, footstep cadence, bags)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02581 — Grouped: complete the lighting unit tests (daylight model, flood, exposure, colour temperature)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02582 — Implement the ID-stability test against the golden list (adding is fine, renaming fails)
      dep: HOUSE-00399 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02583 — Grouped: complete the integration tests for doors and visibility (all 62, both directions, both sides)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02584 — Grouped: complete the integration tests for lights and illumination (all 84 groups, plus the 12 §30 rows)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02585 — Grouped: complete the integration tests for water, toilets, fridge, TV, containers and pickups
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02586 — Grouped: complete the integration tests for save/load/reset across every subsystem
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02587 — Grouped: complete the integration tests for pets (pathing, doors, perches, dog↔cat)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02588 — Grouped: complete the long-run integration tests (30 simulated days: lunation, weather variety, no leak, no drift)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02589 — Implement the scripted-bot harness (seeded random walk with interaction) and its assertions
      dep: HOUSE-00618 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02590 — Grouped: complete the render regression scenes for time of day (64 scenes)
      dep: HOUSE-02571 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02591 — Grouped: complete the render regression scenes for weather (48 scenes)
      dep: HOUSE-02590 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02592 — Grouped: complete the render regression scenes for room lighting (80 scenes across 20 rooms)
      dep: HOUSE-02590 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02593 — Grouped: complete the render regression scenes for doors, culling sanity, tiers, characters, exterior and UI
      dep: HOUSE-02590 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02594 — Implement the render-test tolerance policy (per-pixel and mean-absolute-difference budgets) and the difference-image artefact
      dep: HOUSE-02590 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02595 — Implement the Tier S vs Tier E content-identity test (same scene content, different pixels)
      dep: HOUSE-02593 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02596 — Complete the realism-validation suite over the layout and every asset
      dep: HOUSE-00360, HOUSE-00187 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02597 — Complete the performance test suite (10 scenarios, all budgets, nightly, logged)
      dep: HOUSE-02408 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02598 — Implement the soak test: 2 hours unattended at 60 FPS, RSS growth < 20 MB/hour, no crash, no audio starvation, 200 autosaves
      dep: HOUSE-02589 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02599 — Measure and record test-suite runtime; keep the `unit` target under 60 s
      dep: HOUSE-02597 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02600 — Write `docs/testing.md`: what each suite covers, how to run it, how to add a case, how to accept a render-test change
      dep: HOUSE-02599 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-02601 — Phase-44 review and commit
      dep: HOUSE-02571…HOUSE-02600 · sys: — · plat: ALL · pri: MUST

---

## Phase 45 — Visual polish and habitation

- [ ] HOUSE-02681 — Implement curtains and blinds: meshes, `blindFraction` interaction, the `translucent` portal mode, the transmission cut
      dep: HOUSE-01196 · sys: interaction · plat: ALL · pri: MUST
- [ ] HOUSE-02682 — Author curtains and blinds for the 34 windows that have them
      dep: HOUSE-02681 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02683 — Implement curtain wind sway at an open window (completes HOUSE-01880)
      dep: HOUSE-02682, HOUSE-01880 · sys: weather · plat: ALL · pri: OPT
- [ ] HOUSE-02684 — Implement the mirror cube maps and the `EnvironmentMapEffect` mirror material
      dep: HOUSE-00209, HOUSE-00896 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-02685 — **Q-05 decision: measure a planar reflection pass for one mirror; adopt or record the rejection**
      dep: HOUSE-02684 · sys: rendering · plat: LNX · pri: OPT
- [ ] HOUSE-02686 — Grouped: the habitation pass — place the ~25 signs-of-habitation items of `cna-house.md` §59.3
      dep: HOUSE-01033 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-02687 — Render the fictional family photographs in Blender using the game's own characters and rooms (9 gallery frames + 6 stair frames)
      dep: HOUSE-02183 · sys: content · plat: TOOL · pri: MUST
      accept: no real person is depicted; the images are ours outright (D-26)
- [ ] HOUSE-02688 — Acquire and place the remaining wall art (landscapes, abstracts, botanicals, posters), all CC0, all manifested
      dep: HOUSE-00262 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02689 — Author the corkboard, whiteboard, timetable, height chart and child's drawing as small texture props
      dep: HOUSE-02686 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-02690 — Implement the dust-motes-in-sunbeam particle effect where a sun patch is strong
      dep: HOUSE-01565, HOUSE-01741 · sys: weather · plat: ALL · pri: OPT
- [ ] HOUSE-02691 — Implement fireplace embers, flame and the fire loop audio in `L0_LIVING`, and the garden fire pit
      dep: HOUSE-01741, HOUSE-00279 · sys: weather · plat: ALL · pri: SHOULD
- [ ] HOUSE-02692 — Implement the kettle steam and the shower steam volumes (completes HOUSE-01374, HOUSE-01419)
      dep: HOUSE-01741 · sys: weather · plat: ALL · pri: MUST
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
      dep: HOUSE-02008 · sys: audio · plat: LNX · pri: MUST
- [ ] HOUSE-02700 — Polish pass: animation smoothing, transition times and the interaction reach clips
      dep: HOUSE-02237 · sys: animation · plat: LNX · pri: MUST
- [ ] HOUSE-02701 — Polish pass: UI typography, spacing, colour and the prompt's readability against bright and dark backgrounds
      dep: HOUSE-02530 · sys: ui · plat: LNX · pri: MUST
- [ ] HOUSE-02702 — Implement the Tier E `RoomLit.fx` and its four techniques
      dep: HOUSE-00087, HOUSE-01256 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-02703 — Implement the Tier E `ShadowDepth.fx` and the shadow map with the frustum-fitted light projection and texel snapping
      dep: HOUSE-02702, HOUSE-00083 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-02704 — Implement 3×3 PCF and the shadow's indoor disable rule
      dep: HOUSE-02703 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-02705 — Implement the Tier E `SkinLit.fx` for shadow-receiving characters
      dep: HOUSE-02703 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-02706 — Implement the Tier E `SkyDome.fx` with the per-pixel gradient, three cloud layers, sun disc and dithering
      dep: HOUSE-02702, HOUSE-01650 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-02707 — Implement the Tier E `Precip.fx` with soft-edge depth fade and wind shear
      dep: HOUSE-02702, HOUSE-01741 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-02708 — Implement the Tier E `SurfaceBlend.fx` for continuous wetness and snow
      dep: HOUSE-02702, HOUSE-01748 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-02709 — Implement the Tier E `PostComposite.fx` with exposure adaptation and the glare composite
      dep: HOUSE-02702, HOUSE-01266 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-02710 — Implement the Tier E `WaterFlow.fx`
      dep: HOUSE-02702, HOUSE-01413 · sys: rendering · plat: ALL · pri: OPT
- [ ] HOUSE-02711 — Verify every Tier E path has a working Tier S fallback and that forcing Tier S loses no scene content
      dep: HOUSE-02702…HOUSE-02710, HOUSE-02595 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02712 — Measure Tier E against budget and against Tier S
      dep: HOUSE-02711 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-02713 — Grouped: re-render the whole render-test corpus after polish and accept the new references
      dep: HOUSE-02701 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02714 — Phase-45 review: a full walkthrough with fresh eyes; list everything that still reads as fake; fix or schedule
      dep: HOUSE-02713 · sys: — · plat: LNX · pri: MUST

---

## Phase 46 — Linux desktop stabilisation  ·  **Feature-complete desktop**

- [ ] HOUSE-02781 — Work through the §79 feature-complete checklist item by item; open a task for every gap
      dep: HOUSE-02714 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-02782 — Fix every gap found by HOUSE-02781
      dep: HOUSE-02781 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-02783 — Run the full suite under ASAN and UBSAN; fix every report
      dep: HOUSE-02782, HOUSE-00137 · sys: — · plat: CI · pri: MUST
- [ ] HOUSE-02784 — Run the 2-hour soak test; fix every leak and every drift
      dep: HOUSE-02598 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-02785 — Verify the failure-handling table of `cna-house.md` §73 row by row by inducing each failure
      dep: HOUSE-02782 · sys: ci · plat: CI · pri: MUST
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

## Phase 47 — Web preparation

**Nothing here builds for the browser yet.** The goal is to satisfy every readiness criterion of
`cna-house.md` §80.1 *on Linux*, so the Web port is a build problem rather than a rewrite.

- [ ] HOUSE-02841 — Extend the lint to reject `std::filesystem`, `fopen` and `std::thread` outside their permitted homes
      dep: HOUSE-00021 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-02842 — Audit and fix every filesystem access outside `DesktopSaveStore`
      dep: HOUSE-02841 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-02843 — Audit and fix any custom loop or `Game::Run` misuse
      dep: HOUSE-00127 · sys: app · plat: ALL · pri: MUST
- [ ] HOUSE-02844 — Implement the desktop context-loss test using EasyGL's `DebugSimulateContextLoss`, destroying and rebuilding every GPU resource and comparing a rendered frame
      dep: HOUSE-00108 · sys: ci · plat: CI · pri: MUST
      accept: **every GPU resource is reconstructible from CPU-side data** — the single hardest Web requirement, proved on Linux
- [ ] HOUSE-02845 — Fix every resource that fails HOUSE-02844
      dep: HOUSE-02844 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-02846 — Verify the user-gesture audio gate is on the desktop path too
      dep: HOUSE-00155 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-02847 — Verify `SequenceTvSource` is exercised on Linux by a setting and by a test
      dep: HOUSE-01501 · sys: ci · plat: CI · pri: MUST
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
- [ ] HOUSE-02855 — Spike: an Emscripten build of a minimal scene (one room, one prop, one sound) to validate the toolchain and the Asyncify interaction
      dep: HOUSE-02843 · sys: app · plat: WEB · pri: MUST
      accept: it runs in Chrome; this is the gate for phase 48
- [ ] HOUSE-02856 — Record the phase-47 readiness matrix in `docs/portability.md`
      dep: HOUSE-02855 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-02857 — Phase-47 review and commit
      dep: HOUSE-02841…HOUSE-02856 · sys: — · plat: ALL · pri: MUST

---

## Phase 48 — Web implementation

- [ ] HOUSE-02891 — CMake: the Emscripten preset with `WEBGL2`, the exception ABI, Asyncify and the pack preloads
      dep: HOUSE-02855 · sys: app · plat: WEB · pri: SHOULD
- [ ] HOUSE-02892 — Build the full game for Emscripten and fix every compile and link error
      dep: HOUSE-02891 · sys: — · plat: WEB · pri: SHOULD
- [ ] HOUSE-02893 — Wire `WebSaveStore` and verify persistence across a page reload
      dep: HOUSE-02892, HOUSE-02853 · sys: persistence · plat: WEB · pri: SHOULD
- [ ] HOUSE-02894 — Wire `SequenceTvSource` as the only television backend on Web
      dep: HOUSE-02892 · sys: rendering · plat: WEB · pri: SHOULD
- [ ] HOUSE-02895 — Wire the canvas-as-display model into the Display settings
      dep: HOUSE-02892 · sys: ui · plat: WEB · pri: SHOULD
- [ ] HOUSE-02896 — Verify the audio gesture gate in a real browser
      dep: HOUSE-02892 · sys: audio · plat: WEB · pri: SHOULD
- [ ] HOUSE-02897 — Verify context loss and restore in a real browser using `WEBGL_lose_context`
      dep: HOUSE-02892, HOUSE-02844 · sys: rendering · plat: WEB · pri: SHOULD
- [ ] HOUSE-02898 — Measure the Web build against its budget; reduce content until it fits
      dep: HOUSE-02892 · sys: — · plat: WEB · pri: SHOULD
- [ ] HOUSE-02899 — Implement the browser loading screen and the progressive pack fetch
      dep: HOUSE-02854, HOUSE-02892 · sys: ui · plat: WEB · pri: SHOULD
- [ ] HOUSE-02900 — Verify the whole feature set in a browser: every phase's headline feature, tested by hand against a checklist
      dep: HOUSE-02898 · sys: — · plat: WEB · pri: SHOULD
- [ ] HOUSE-02901 — Implement a headless-Chrome smoke test in CI (menu, load, 300 frames, one interaction, one save)
      dep: HOUSE-02900 · sys: ci · plat: CI · pri: SHOULD
- [ ] HOUSE-02902 — Evaluate whether threads are needed (COOP/COEP cost vs. load time); decide and record
      dep: HOUSE-02898 · sys: — · plat: WEB · pri: OPT
- [ ] HOUSE-02903 — Multi-browser check: Chrome, Firefox; record what differs
      dep: HOUSE-02900 · sys: — · plat: WEB · pri: SHOULD
- [ ] HOUSE-02904 — Package and document the Web build
      dep: HOUSE-02903 · sys: — · plat: WEB · pri: SHOULD
- [ ] HOUSE-02905 — Phase-48 review and commit
      dep: HOUSE-02891…HOUSE-02904 · sys: — · plat: ALL · pri: SHOULD

---

## Phase 49 — Android preparation

**Blocked on CNA (BL-13).** `HOUSE-02951` is an upstream gate that `cna-house` does not fix.

- [ ] HOUSE-02951 — **Gate: confirm CNA's Android cross-compile succeeds** — `sharp-runtime`'s two NDK-portability bugs fixed upstream (CNA Task 920)
      dep: HOUSE-02905 · sys: — · plat: AND · pri: SHOULD
      accept: `cmake --build` completes for `arm64-v8a`; if not, this phase stops here and is revisited later
- [ ] HOUSE-02952 — **Gate: confirm `CNA_GRAPHICS_RENDERER=OPENGLES3` is selectable and buildable for Android**
      dep: HOUSE-02951 · sys: — · plat: AND · pri: SHOULD
- [ ] HOUSE-02953 — **Gate: confirm a CNA graphics sample runs on a device or emulator**
      dep: HOUSE-02952 · sys: — · plat: AND · pri: SHOULD
- [ ] HOUSE-02954 — Verify the `IInputSource` abstraction is complete (no direct `Keyboard`/`Mouse` reads anywhere)
      dep: HOUSE-00140 · sys: ci · plat: CI · pri: SHOULD
- [ ] HOUSE-02955 — Verify the `Platform` capability struct drives HUD and defaults, using a forced-touch desktop mode
      dep: HOUSE-00141 · sys: ci · plat: CI · pri: SHOULD
- [ ] HOUSE-02956 — Verify the UI at 18:9 and 20:9 with safe-area insets on desktop
      dep: HOUSE-02527 · sys: ci · plat: CI · pri: SHOULD
- [ ] HOUSE-02957 — Implement the Android quality tier and verify it on desktop via `--quality android`
      dep: HOUSE-02407 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-02958 — Verify the content packs fit the APK + OBB budget
      dep: HOUSE-00203, HOUSE-02957 · sys: ci · plat: CI · pri: SHOULD
- [ ] HOUSE-02959 — Design the Android lifecycle mapping (pause, resume, background, surface loss) onto `Game`'s events
      dep: HOUSE-02953 · sys: app · plat: AND · pri: SHOULD
- [ ] HOUSE-02960 — Verify the desktop focus-loss path exercises the same lifecycle code
      dep: HOUSE-02959 · sys: ci · plat: CI · pri: SHOULD
- [ ] HOUSE-02961 — Record the phase-49 readiness matrix in `docs/portability.md`
      dep: HOUSE-02960 · sys: — · plat: ALL · pri: SHOULD
- [ ] HOUSE-02962 — Phase-49 review
      dep: HOUSE-02951…HOUSE-02961 · sys: — · plat: ALL · pri: SHOULD

---

## Phase 50 — Android touch UI

- [ ] HOUSE-02991 — Implement `TouchSource` over `TouchPanel`, with multi-touch tracking and gesture recognition
      dep: HOUSE-00101, HOUSE-00140 · sys: player · plat: AND · pri: SHOULD
- [ ] HOUSE-02992 — Implement the floating movement stick (bottom-left, 180 vu, analogue direction and magnitude)
      dep: HOUSE-02991 · sys: ui · plat: AND · pri: SHOULD
- [ ] HOUSE-02993 — Implement the look region (right half minus buttons) with its own sensitivity setting
      dep: HOUSE-02991 · sys: ui · plat: AND · pri: SHOULD
- [ ] HOUSE-02994 — Implement the interact button showing the same verb text as the desktop prompt
      dep: HOUSE-02992, HOUSE-01130 · sys: ui · plat: AND · pri: SHOULD
- [ ] HOUSE-02995 — Implement the walk-mode, camera and menu buttons
      dep: HOUSE-02994 · sys: ui · plat: AND · pri: SHOULD
- [ ] HOUSE-02996 — Implement the touch-HUD visibility rule (`hasTouch && !hasKeyboard`) so desktop never shows it
      dep: HOUSE-02995, HOUSE-00141 · sys: ui · plat: ALL · pri: SHOULD
- [ ] HOUSE-02997 — Implement the `--force-touch` desktop flag so the touch HUD is testable and screenshot-able on Linux
      dep: HOUSE-02996 · sys: debug · plat: LNX · pri: SHOULD
- [ ] HOUSE-02998 — Implement touch-friendly menu hit targets (≥ 88 vu) without changing the desktop layout
      dep: HOUSE-02996 · sys: ui · plat: ALL · pri: SHOULD
- [ ] HOUSE-02999 — Implement touch targeting assistance (a slightly larger interaction cone on touch)
      dep: HOUSE-02994, HOUSE-01127 · sys: interaction · plat: AND · pri: SHOULD
- [ ] HOUSE-03000 — Render tests: the touch HUD at 3 aspect ratios via `--force-touch`
      dep: HOUSE-02997 · sys: ci · plat: CI · pri: SHOULD
- [ ] HOUSE-03001 — Playtest the touch controls on desktop with a touchscreen or a simulator; tune
      dep: HOUSE-03000 · sys: ui · plat: LNX · pri: SHOULD
- [ ] HOUSE-03002 — Phase-50 review
      dep: HOUSE-02991…HOUSE-03001 · sys: — · plat: ALL · pri: SHOULD

---

## Phase 51 — Android implementation

- [ ] HOUSE-03031 — Create the Gradle/NDK project producing a shared library plus `SDLActivity`, following CNA's own devices-demo precedent
      dep: HOUSE-02962 · sys: app · plat: AND · pri: SHOULD
- [ ] HOUSE-03032 — Build the full game for `arm64-v8a` and fix every compile and link error
      dep: HOUSE-03031 · sys: — · plat: AND · pri: SHOULD
- [ ] HOUSE-03033 — Wire content delivery (APK assets or OBB) and the content root
      dep: HOUSE-03032, HOUSE-02958 · sys: content · plat: AND · pri: SHOULD
- [ ] HOUSE-03034 — Wire the save store to Android's app-private storage
      dep: HOUSE-03032, HOUSE-00152 · sys: persistence · plat: AND · pri: SHOULD
- [ ] HOUSE-03035 — Wire `SequenceTvSource` as the television backend
      dep: HOUSE-03032 · sys: rendering · plat: AND · pri: SHOULD
- [ ] HOUSE-03036 — Wire the lifecycle mapping and verify pause/resume/background on a device
      dep: HOUSE-03032, HOUSE-02959 · sys: app · plat: AND · pri: SHOULD
- [ ] HOUSE-03037 — Run on a real device; measure against the Android budget; reduce until it fits
      dep: HOUSE-03036 · sys: — · plat: AND · pri: SHOULD
- [ ] HOUSE-03038 — Verify the whole feature set on a device against the checklist
      dep: HOUSE-03037 · sys: — · plat: AND · pri: SHOULD
- [ ] HOUSE-03039 — Verify the touch controls on a device and tune
      dep: HOUSE-03038, HOUSE-03001 · sys: ui · plat: AND · pri: SHOULD
- [ ] HOUSE-03040 — Test on at least 3 devices spanning GPU vendors; record what differs
      dep: HOUSE-03039 · sys: — · plat: AND · pri: SHOULD
- [ ] HOUSE-03041 — Package and document the Android build
      dep: HOUSE-03040 · sys: — · plat: AND · pri: SHOULD
- [ ] HOUSE-03042 — Phase-51 review and commit
      dep: HOUSE-03031…HOUSE-03041 · sys: — · plat: ALL · pri: SHOULD

---

## Phase 52 — Final optimisation and release

- [ ] HOUSE-03071 — Profile the release build on all three platforms and produce a prioritised optimisation list
      dep: HOUSE-03042 · sys: — · plat: ALL · pri: MUST
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

## Deferred and explicitly out of scope

Recorded so nobody has to re-derive the decision.

| Item | Status | Reason |
|---|---|---|
| Drivable car | **Deferred** (Q-06; no implementation task exists) | A different project; the brief permits the car to be a prop |
| Player reflection in mirrors | **Deferred** (Q-05, `HOUSE-02685` measures it) | Budget; D-19 |
| Swimming pool | **Rejected** | D-22 |
| Music score | **Rejected** | D-21 |
| Rigid-body physics engine | **Rejected** | D-07 |
| ECS | **Rejected** | D-06 |
| Deferred rendering / G-buffer | **Impossible** | BL-03, EasyGL MRT |
| Stencil-based techniques | **Impossible** | BL-02 |
| Real-time global illumination | **Out of scope** | §3 |
| Multiplayer | **Out of scope** | §3 |
| Modding API / level editor GUI | **Out of scope** | §3; the JSON is hand-editable |
| Cooking, hunger, spoilage, inventory grid | **Rejected** | §55.3 — this is not a survival game |
| Air-flow / temperature / humidity simulation | **Rejected** | §52 — an open window changes light, sound and a splash decal, nothing more |
| Water damage / flooding | **Rejected** | §56.3 — a bath overflows into its overflow outlet |
| HRTF / convolution reverb | **Impossible** | BL-11; approximations designed instead (§64) |
| iOS, macOS, Windows, console targets | **Out of scope** | Not requested |

---

## Task count

**1 297 numbered tasks across 53 phases.**

| Phase group | Phases | Tasks |
|---|---|---|
| Foundations, capability proof, build, pipeline, assets | 0–4 | 236 |
| World data, blockout, collision, camera, visibility | 5–9 | 200 |
| Exterior, neighbourhood, materials, furnishing | 10–13 | 130 |
| Interaction framework and the systems built on it | 14–21 | 159 |
| Time, sun, moon, stars, sky, weather | 22–30 | 146 |
| Audio, room-aware audio, animals, avatar, animation | 31–38 | 161 |
| Persistence, reset, optimisation, streaming, debug, tests, polish, stabilisation | 39–46 | 188 |
| Web, Android, release | 47–52 | 77 |
| **Total** | **0–52** | **1 297** |

The **ID ranges reserved** in the phase index are larger than the tasks written, deliberately:
every phase has headroom so that inserted work takes a fresh ID inside its own phase and never
disturbs an existing one.

**Grouped tasks and the real size of the work.** Tasks whose objective begins `Grouped:` each
cover a stated number of items and name that number in the task text — 214 containers, 62 doors,
78 furnished cells, 20 footstep surfaces, 34 base materials, 430 audio files, 78 render-test
scenes, 62 window rows, 84 light groups, 60 neighbourhood buildings. Expanding every one of those
into its own numbered row would produce roughly
**4 100 rows** without adding a single piece of information: the counts, the acceptance criteria
and the verification method are already stated. The plan is decomposed to single-commit
granularity everywhere the work is *architectural* — visibility, collision, animation, audio
routing, persistence, the content pipeline — and grouped everywhere it is *data*, which is
exactly the split `cna-house.md` §17.3 argues for in the code itself. Nothing is hidden: the real
scale of this project is somewhere between 4 000 and 5 000 discrete pieces of work, and the
document says so here rather than burying it in a task list nobody could read.

---

## Requirements traceability

The full mapping from every numbered requirement of the brief to a document section, a phase and
an ID range is `cna-house.md` §81. It is maintained there rather than duplicated here so the two
documents cannot disagree.

---

## Planning corrections

Corrections made to `cna-house.md` or to this file *during* implementation, with the evidence that
forced each one. Nothing is changed silently, and an accepted decision is not redesigned without
evidence that it fails.

| Date | Task | Correction | Why |
|---|---|---|---|
| 2026-09-06 | `HOUSE-00021` | `assets-src/effects/` → `assets-src/Effects/` in `cna-house.md` §70.1, in the §18.1 CMake snippet and in this task's text | Four statements in the two documents disagreed on the case of one path. §18.1's pipeline diagram, §17.5 and the "directories are PascalCase" rule of §8.3 said `Effects/`; §70.1 and the §18.1 CMake snippet said `effects/`. `check_xna_only.py` enforces where a `.fx` may live and needs exactly one spelling. |
| 2026-09-06 | `HOUSE-00021` | `SOURCE_DIR assets-src/content` → `SOURCE_DIR assets-src` in the §18.1 CMake snippet, with the config file moved to `assets-src/.cna-content.json` | The same snippet placed the ContentManager-bound trees under `assets-src/content/`, while §18.1's own diagram, §17.5 and §15.1 place `Models/`, `Textures/`, `Audio/`, `Fonts/`, `Video/`, `Effects/` and `world/` directly under `assets-src/`. The directory skeleton created by `HOUSE-00001` follows the majority, and `check_layout.py` asserts it. |
| 2026-09-06 | — | `cna-house.md` header and this file's header now record implementation as in progress rather than forbidden | The project owner approved implementation on 2026-09-06. |
| 2026-09-06 | `HOUSE-00042` | `dep: HOUSE-00001…HOUSE-00041` → the completed pre-build foundation tasks only (`HOUSE-00001`, `HOUSE-00003`–`HOUSE-00024`, `HOUSE-00030`–`HOUSE-00034`, `HOUSE-00036`–`HOUSE-00041`); `accept: CI green on a fresh clone` → the six stage-A criteria now listed on the task; the task is retitled the pre-probe foundation checkpoint | **The plan deadlocked.** `HOUSE-00042` depended on all of `HOUSE-00001…00041`, which includes seven tasks (`HOUSE-00002`, `HOUSE-00025`–`HOUSE-00029`, `HOUSE-00035`) whose acceptance genuinely needs the `CMakeLists.txt` of `HOUSE-00121` and the GoogleTest harness of `HOUSE-00125`. `HOUSE-00061` depends on `HOUSE-00042`, phase 1 ends at `HOUSE-00120`, and `HOUSE-00121` depends on `HOUSE-00120` — a closed cycle in which no phase could start. Its own criterion, "CI green on a fresh clone", was unsatisfiable for the same reason and is not lost: it is what `HOUSE-00133` and `HOUSE-00134` accept on. The seven tasks stay **open** with their criteria unchanged; only the gate moved. No id was renumbered, no task was struck, no phase was reordered. |
| 2026-09-06 | `HOUSE-01542`…`HOUSE-01547`, `HOUSE-01703`, `HOUSE-01704`, `HOUSE-01802`, `HOUSE-00919`, `HOUSE-00920`, `HOUSE-01941` | **New work, requested by the project owner on 2026-09-06:** a compressed seasonal year with visible seasons. `cna-house.md` gains §35.2b (the compressed year) and rewrites §36.3 (continuous season phase, seasonal gating, the four seasonal looks). Eleven tasks added, each in its own phase's reserved free range. | The plan had a 24-real-minute day but a realistic calendar, so one year took 146 real hours and no player would ever see autumn. The owner set the calendar to advance 24 days per simulated day, making a year 365 real minutes (~6 h): all four seasons in one long session, with the sun still rising once per 24 real minutes. The owner additionally required seasonal weather gating (no snow in summer, storms likelier in summer heat), seasonal day/night length, seasonal vegetation and snow cover, gradual rather than stepwise transitions, and a player-facing readout of time of day, year progress and outdoor temperature. No existing id was renumbered or struck; `HOUSE-01534`/`HOUSE-01535` keep their scope and are now dependencies of the new tasks. |
| 2026-09-06 | `HOUSE-00068` | `accept: the failure mode is documented so the pipeline can assert against it` → the actual behaviour is measured and documented, and `BL-06` is corrected. `cna-house.md` §6 `BL-06` rewritten and downgraded from severity `L` to a pipeline note. | **The task's premise was measured false.** `BL-06` predicted 24-bit PCM is rejected. It is not, by either path: `cna-content`'s `SoundEffectProcessor` converts it to 16-bit with a warning and exit code 0, and `SoundEffect::FromStream` accepts a raw 24-bit WAV without throwing. The conversion is byte-identical to `ffmpeg -c:a pcm_s16le` (29 of 96 304 bytes differ, all header; PCM identical from byte 400 to EOF). There is no failure mode to document, so the criterion as written was unsatisfiable. Phase 1 exists to measure, and a measurement is never massaged to fit the architecture. |
| 2026-09-06 | `HOUSE-00069` | `BL-06`'s workaround command `ffmpeg -i in.wav -c:a pcm_s16le -ar 44100 out.wav` → `ffmpeg -i in.wav -c:a pcm_s16le out.wav`; the offline step is re-scoped from format conversion to provenance capture | Measuring the two halves separately showed the resample, not the bit-depth reduction, is what damages the signal: 24→16 bit costs 0.0017 dB RMS, while adding `-ar 44100` costs 0.889 dB RMS and 0.26 dB peak on a high-frequency NOX source. The collection is uniformly 48 kHz, CNA loaded and played a 48 kHz asset correctly, and the mixer resamples at playback anyway, so the offline resample bought nothing and cost signal. |
| 2026-09-06 | `HOUSE-00063` | No text change to the task; a finding recorded against `HOUSE-00136` and `HOUSE-00124` | `CNA_CNAEXT=OFF` removes the `CNA::Graphics::` engine layer (6 277 symbols → 0; archive 37.4 MB → 69 kB) but **not** the other forbidden identifiers, which live in `Microsoft::Xna::Framework::Graphics` in CNA's always-compiled core and are present in the linked binary. `check_xna_only.py` is therefore the *only* gate for those, not a redundant one. `HOUSE-00136` must be scoped to what can actually be asserted at the symbol level. |

---

## Status

`STATUS: APPROVED — IMPLEMENTATION IN PROGRESS`

**Phase 0 — 35 of 42 tasks complete. Stage A is closed; stage B is deferred by design.**

Done: `HOUSE-00001`, `HOUSE-00003`–`HOUSE-00024`, `HOUSE-00030`–`HOUSE-00034`,
`HOUSE-00036`–`HOUSE-00042`.

`HOUSE-00042` — the pre-probe foundation checkpoint — closed on 2026-09-06. **Phase 1 is
unblocked.**

Stage B: still open, each blocked on infrastructure a later phase creates. These are real open
tasks. None of them is complete, and none will be ticked until its own `verify:` step can actually
be run:

| Task | Why it is still open | Closes after |
|---|---|---|
| `HOUSE-00002` `README.md` | The README is written and carries a visible note saying so. Its criterion is that a contributor can *build* from it. | `HOUSE-00121` |
| `HOUSE-00025` `util/Log` | `verify: unit LogTests.*` needs the GoogleTest harness. | `HOUSE-00125` |
| `HOUSE-00026` `util/Ids` | Same — `verify: unit IdsTests.*`. | `HOUSE-00125` |
| `HOUSE-00027` `util/Rng` | Same — `verify: unit RngTests.*`. | `HOUSE-00125` |
| `HOUSE-00028` `util/Json` | Same, and it additionally needs `System::Text::Json` linked, i.e. the CMake project. | `HOUSE-00121`, `HOUSE-00125` |
| `HOUSE-00029` `util/SmallVector` | Explicitly gated on a measurement, which needs a build. | `HOUSE-00125` |
| `HOUSE-00035` `CMakePresets.json` | Its criterion is that each preset configures; that needs `CMakeLists.txt`. | `HOUSE-00121` |

`HOUSE-00024`'s `Result<T>` is ticked but shares stage B's shape: the header is verified, its
permanent GoogleTest test lands with `HOUSE-00125`.

**Phase 1 — in progress. 9 of 60 tasks complete: `HOUSE-00061`–`HOUSE-00069`.**

Measured against cnanext `d422038` (branch `next`, 11 modified files, all Markdown, **zero under
`modules/`**) and sharp-runtimenext `30ccdef`, at `CNA_GRAPHICS_RENDERER=OPENGLES3`,
`CNA_PLATFORM=SDL3`, `CNA_CNAEXT=OFF`, `Release`, on Mesa 25.0.7 / GLES 3.2.

| Task | Result |
|---|---|
| `HOUSE-00061` | Capability report created, 40 claim rows |
| `HOUSE-00062` | PASS — 300 frames at 1600×900, clean exit, no GL error |
| `HOUSE-00063` | PASS — 0 `CNA::Graphics::` symbols against a 6 277-symbol control; **caveat recorded** |
| `HOUSE-00064` | PASS — **`.xnb` wins** over `.cnb` |
| `HOUSE-00065` | PASS — 16/16 texels exact, **premultiplied**; expectation corrected |
| `HOUSE-00066` | PASS — glyph placement measured from rendered ink |
| `HOUSE-00067` | PASS — 16-bit WAV loads and plays on a real device |
| `HOUSE-00068` | **`BL-06` disproved** — 24-bit PCM is accepted, not rejected |
| `HOUSE-00069` | PASS — NOX conversion verified; **`BL-06`'s command corrected** |

Nothing in this tranche failed or is blocked. Every probe binary and source was deleted;
`build-probe/` is empty and untracked.

The next tranche is `HOUSE-00070` onwards — the glTF, model and rendering probes — deliberately
not started in the same session. `HOUSE-00083` (`SurfaceFormat::Single` render target) still owns
`BL-09`: EasyGL's startup line advertises render-target `RGBA16F`/`RGBA32F` but names no
single-channel float, so that row stays open until a target is actually created.

Phase 0's stage-B tasks close after phase 2 delivers the build and the test harness.
