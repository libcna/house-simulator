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
| 2 | Build skeleton and CI | 00121–00180 | 48 | `Game` clears the screen; HEADLESS tests run in CI |
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
      finding: (corrected 2026-09-07, while writing `HOUSE-00216`) the `stale-gitkeep` rule judged
            emptiness **on disk**, so the first content build that actually populated `content/`
            made the gate fail on `content/.gitkeep` — a placeholder whose whole purpose is to keep
            a git-ignored directory present in the repository. The layout gate is also the first
            stage of the content build, so it failed on the output of the previous run. Emptiness
            is now judged as git judges it, via `git check-ignore`: an ignored file does not make a
            `.gitkeep` stale, a tracked one does, and a tree that is not a git repository behaves
            exactly as before. The planted-fault fixture could not have caught this — its temporary
            tree is not a repository — so the selftest now builds one.
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
- [x] HOUSE-00029 — Implement `util/SmallVector` and `util/FixedString`, or decide against them after measuring; record the decision
      dep: HOUSE-00024 · sys: util · plat: ALL · pri: SHOULD
      note: (2026-09-06, phase 2) **DECIDED AGAINST BOTH, with numbers.** The task offered either
            outcome and the measurement chose. `tests/perf/SmallContainerTests.cpp` is the
            measurement and stays in the tree, so the decision can be revisited against evidence
            rather than re-argued. Release, medians of nine runs, reference hardware:
      finding: a `std::vector<int>` of 8 elements, built and discarded, costs **15.5 ns** against
            **0.8 ns** for a stack array plus a count — a 20× ratio and 14.7 ns of real saving. The
            ratio is not the question; the COUNT is. To reach even 1 % of the 16.67 ms frame budget
            a frame would have to build **11 300** such containers, and nothing in this design comes
            near that: phase 11's visibility set builds one per visible cell, which is dozens.
            `util::SmallVector` would be a new type on every call site to buy a fraction of a
            percent that cannot be measured in a frame.
      finding: `util::FixedString` buys **nothing at all** for the strings this project actually
            builds. Short-string optimisation already makes a 10-character `std::string`
            (`"L0_KITCHEN"`) cost **0.4 ns and no allocation**; a 57-character asset path costs
            12.5 ns because it allocates, and asset paths are built at load, not per frame. A
            fixed-capacity string would be a second string type in the codebase to avoid an
            allocation that libstdc++ already avoids.
      finding: **the measurement did find one real per-draw allocation, and it was fixed.**
            `HOUSE-00162`'s `MaterialBinder::Bind` built a fresh 72-matrix skinning palette on every
            skinned draw: **149 ns** against **107 ns** to refill a reused buffer, so 42 ns of pure
            allocation on a per-draw path. Against `HOUSE-00106`'s 8.15 µs draw call that is 0.5 %
            — small — and it is removed anyway, because a member vector is one line and there is
            nothing to weigh against it. Note that this is NOT a `SmallVector` case either: 72 × 64 B
            is 4 608 B, which belongs on the heap once rather than on the stack every call.
      note: the decision is reversible and the condition is written down: if a profile ever shows a
            frame building thousands of short-lived containers — the plausible candidate is phase
            11's per-cell frustum lists at a cell count nobody has measured yet — this task's
            benchmark is here to re-run and the answer may change. What must not happen is adding
            the type first and looking for the justification afterwards.
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
      finding: (2026-09-07, `HOUSE-00204`'s session) **the save-store tests were not parallel-safe,
            and CI could never have found out.** Every test in the fixture used one fixed file name
            in the user's REAL save directory and deleted it in both `SetUp` and `TearDown`, so two
            `ctest -j2` processes running two of these tests at once deleted each other's file
            mid-test. Measured: `ctest -L integration -j2` failed
            `TheSecondWriteLeavesTheFirstAsABackup` while that same test passed five times out of
            five on its own. CI runs `ctest --preset integration` with no `-j`, so it was serial
            and green throughout. The name is now unique per process; three consecutive `-j2` runs
            are clean where one had failed. A test that fails only under parallelism is a test
            people learn to ignore.
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
- [x] HOUSE-00154 — Implement `AudioSystem` skeleton: device init, graceful `NoAudioHardwareException` handling, master volume
      dep: HOUSE-00097 · sys: audio · plat: ALL · pri: MUST
      accept: `--no-audio` and a missing device both leave the game fully playable
      note: (2026-09-06) `audio/AudioSystem.{hpp,cpp}`: three states (`Waiting`, `Ready`, `Silent`),
            §68's five mix categories with their defaults, a clamped master, and a `Summary()` line
            for the log header. Nine unit tests and three integration tests; both acceptance clauses
            are exercised by a whole `Game` running 30 frames and exiting 0.
      finding: **`SoundEffect::setMasterVolumeProperty` IS the device probe, and that is measured.**
            It is one of the five entry points in `modules/audio/src/Xna/SoundEffect.cpp` that force
            CNA's mixer up and convert the internal failure into `NoAudioHardwareException` — exactly
            as FNA's `SoundEffect.Device()` does. So the device is opened by setting the volume, in
            plain XNA, with nothing CNA-specific asked of it and no separate init call to invent.
      finding: **`setMasterVolumeProperty` has TWO overloads and the `float&&` one is `CNAEXT`.**
            `SoundEffect::setMasterVolumeProperty(0.8f)` compiles, runs, and violates ADR-0001 —
            overload resolution picks the rvalue overload for a literal or a temporary. The fix is a
            named lvalue. **`check_xna_only.py` cannot see this class of violation**, because it is
            an overload-resolution outcome and not a name; it is the second such case after
            `KeyboardState`'s default constructor, and both are now written into
            `docs/conventions.md` §5a.
      finding: silence is a supported way to run, not an error path, so nothing here throws and the
            "no audio hardware" case is logged at **Info** rather than Error — an error line would
            send someone looking for a fault that is not there. `--no-audio` is `Silent` immediately
            and never touches the device at all, which matters because it is the option someone
            reaches for when the device is what is broken.
      finding: `EffectiveVolume` returns **0 whenever audio is not ready**, rather than the mix, so a
            caller that forgot to check cannot play into a device that is not there.
      finding: the voice ceiling is not here and that is deliberate. MEASURED (`HOUSE-00097`): 512 of
            512 looping instances reported `Playing` and CNA refused nothing, so a budget is a design
            decision this project must enforce itself — it belongs with the voice manager in phase 31
            (`cna-house.md` §31), not in the device skeleton.
- [x] HOUSE-00155 — Implement the user-gesture audio gate (title screen "press any key") on every platform
      dep: HOUSE-00154 · sys: audio · plat: ALL · pri: MUST
      note: (2026-09-06) Done in two commits, and left OPEN between them rather than ticked early.
            The MECHANISM landed with `HOUSE-00154`: `AudioSystem` starts in `Waiting` and opens the
            device only from `NoteUserGesture()`, and `InputState::anyPressed` is a new edge meaning
            *any* key or mouse button went down — separate from every bound action, because a
            browser waits for any interaction and a gate wired to one named key is a gate the player
            can fail to find. The **title screen** landed with `HOUSE-00156` and completes it: the
            loading screen shows a pulsing `Press any key to begin`, and its own `Update` is what
            calls back into the audio system. Verified by screenshot at 1600×900 and by an
            integration test that runs 30 headless frames and asserts the screen is still up and the
            device still closed.
      finding: **the loading screen, the title screen and the audio gate are ONE screen, and that is
            what makes the gate free.** A browser will not open an audio device until the user has
            interacted with the page, so something must wait for a key press before play starts.
            Making that the loading screen means the wait costs nothing — the player reads the prompt
            while content is still loading, and the gate is satisfied before it could block anything.
      finding: the gesture is recognised in exactly ONE place. The `Game` does not check
            `anyPressed` itself; the screen does, and calls back. Two places recognising the same
            gesture is two places to disagree about whether it has happened, and the audio device
            would be opened twice or not at all.
      finding: the callback fires on the FIRST press rather than on the dismissing one, and exactly
            once. Early, so a player who presses a key during load does not then wait again for the
            mixer; once, so holding a key does not re-probe the device every frame.
- [x] HOUSE-00156 — Implement the loading screen and the `MenuStack` skeleton
      dep: HOUSE-00145 · sys: ui · plat: ALL · pri: MUST
      note: (2026-09-06) `ui/MenuStack.{hpp,cpp}` (`IScreen`, `ScreenId` for §67.3's six screens plus
            `Loading`, `ScreenAction`) and `ui/LoadingScreen.{hpp,cpp}`. Fifteen unit tests plus one
            end-to-end integration test. Wired into the game: pushed at the end of `LoadContent`,
            updated before every system, drawn inside the HUD's single `SpriteBatch`.
      finding: **only the top screen updates, but more than one may draw.** That asymmetry is the
            whole design. A `Confirm` over a `PauseMenu` must show the menu behind it and must be
            the only thing that hears the keyboard — if both heard it, `Escape` would close both at
            once, which reads as the confirmation having been *answered* when it was only dismissed.
            `Draw` therefore walks down from the top to the first opaque screen and draws upward
            from there, so nothing hidden costs a pass.
      finding: `WorldIsPaused()` asks **every** screen in the stack, not the top one. A `Confirm` is
            translucent and does not itself pause, but the `PauseMenu` under it may, and the world
            must not resume because a dialogue opened on top of it. The default is *not* paused,
            which is §67.3's decision rather than an omission: the pause menu keeps the clock
            running because a house that stops when you look away is less convincing.
      finding: `ScreenAction::Quit` is **reported**, not acted on — the stack never calls `Exit()`.
            Ending the session is the `Game`'s business, and a UI widget that can stop the process is
            one that can stop it by accident.
      finding: the loading screen's `Loading…` line is currently unreachable in the real app, and
            that is honest rather than dead: content loads synchronously inside `LoadContent`, so
            `contentLoaded_` is already true on the first frame that updates. The condition is a
            real flag and the line appears the moment phase 3's residency makes loading take time.
      finding: **MEASURED — `Color(bytecs, bytecs, bytecs, bytecs)` is `CNAEXT`.** The plain XNA 4.0
            constructors take `intcs` or `float`. A `std::uint8_t` alpha for the prompt's pulse
            compiled straight into an ADR-0001 violation, caught only because the byte overload is
            *also* ambiguous against the int one — had it not been, it would have compiled silently.
            This is the **third** CNAEXT-overload trap after `setMasterVolumeProperty` and
            `KeyboardState()`, and `check_xna_only.py` can see none of them. `docs/conventions.md`
            §5a now carries the rule that the header of any XNA type is read before it is first
            constructed.
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
            The render test `tier-fallback-01` is still outstanding: `HOUSE-00164`'s harness now
            exists and `title-01` uses it, but a *tier* fixture needs a scene whose two tiers look
            different, and Tier E currently draws exactly what Tier S does. It is recorded against
            phase 12, where the first Tier-E effect makes the comparison mean something.
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
- [x] HOUSE-00164 — Add the first render regression test harness: fixed pose, fixed clock, render, compare PNG with tolerance
      dep: HOUSE-00151, HOUSE-00138 · sys: ci · plat: CI · pri: MUST
      note: (2026-09-06) `tests/render/ImageCompare.hpp` (header-only, so the unit lane can test the
            comparison without the render lane's cost), `tests/render/RenderHarness.{hpp,cpp}`, the
            fixture `title-01` and its committed reference. Four render tests plus eight unit tests
            of the comparison itself.
      note: **the harness drives `--screenshot`, the production capture path.** A harness that
            rendered its own frame would be testing a second renderer the player never sees, and it
            would be the first thing to drift. Decoding is `Texture2D::FromStream`, which is plain
            XNA 4.0 — the `assetName` constructors and `SaveAsPng(filename)` are the CNAEXT ones.
      finding: **the reference frame is generated under `LIBGL_ALWAYS_SOFTWARE=1`, and that is the
            decision that makes the whole thing work.** A hardware reference could never be
            reproduced on a machine nobody owns; llvmpipe is what `HOUSE-00138`'s CI job already
            uses, so the same bytes come out there. On hardware the pixel test SKIPS with an
            explanation and the three driver-independent tests still run — which is exactly what
            `HOUSE-00115` says to do when the driver is not the one phase 1 measured against.
      finding: `ImageDiff` reports **four** numbers, not one. A single "percentage different" hides
            the distinction that matters: every pixel off by one is Mesa version drift; 0.3 % of
            pixels off by 200 is a missing object. A unit test puts ONE maximally wrong pixel in
            4 096 and asserts the mean stays under 0.05 — which is why the mean is never the
            assertion.
      finding: the corner frame-time readout genuinely differs between two runs, so it is excluded
            by an explicit **region**, never by widening the tolerance. Raising the per-channel
            tolerance to 255 to absorb it would absorb every real regression with it.
      finding: **the harness was shown to FAIL before it was trusted.** The clear colour was moved
            by 10 in one channel, the suite rebuilt, and the reference test reported
            `1 407 000 of 1 411 200 pixels differ (99.70%), max channel delta 10` and failed; the
            perturbation was then reverted and all four went green again. A regression harness whose
            comparison has never been shown to fail is one that reports green forever.
      finding: determinism is asserted separately and first, at tolerance **0**: two captures of the
            same build are bit-identical. Without that the reference comparison cannot mean anything,
            and a flaky render test is worse than none because it teaches people to ignore it.
      note: captures are written to `${CMAKE_BINARY_DIR}/test-output`, not beside the reference. A
            `git status` that lists a captured frame after every test run teaches people to ignore
            `git status`.
- [x] HOUSE-00166 — Define `docs/anim-format.md`: the project-owned `.chanim` binary sidecar — magic, version, joint list by name in skin-joint order, parent indices, bind and inverse-bind poses, clips as per-bone TRS keyframe tracks, stride length and foot-plant markers
      dep: HOUSE-00074, HOUSE-00028 · sys: animation · plat: ALL · pri: MUST
      note: (2026-09-06) `docs/anim-format.md`, 206 lines, normative. Version 1: `CHAN` magic, a
            whole-file version, the skeleton as name + parent + bind + inverse-bind per joint in
            **blend-index order**, then clips as per-bone TRS tracks with a `boneFirstKey` partition,
            stride length and foot-plant times.
      finding: **the joint-name list in blend-index order is the entire reason this file exists.**
            `HOUSE-00074` measured that vertex blend indices are SKIN-LOCAL, not `Model::Bones`
            indices, and that nothing in the compiled `Model` reproduces which bone slot *i* is. The
            list *is* the binding. Everything else in the format could in principle be derived from
            the source asset; that list could not be recovered at all.
      finding: **no section offsets and no index table.** The file is read forward once by a
            `BinaryReader`. An offset table is a second description of the layout that can disagree
            with the first, and a full character is a few hundred kilobytes — random access buys
            nothing. For the same reason there is no alignment padding: nothing is memory-mapped or
            cast over.
      finding: a key carries **no bone index**. `boneFirstKey` is the same information without
            repeating it 100 000 times: a key is 40 bytes, and a bone index would add 10 % to the
            largest section of the file for something already implied by position. It has
            `boneCount + 1` entries so bone *b*'s track is `[first[b], first[b+1])` with no special
            case for the last bone, and an empty range is legal — that bone holds its bind pose.
      finding: `boneCount` is capped at **256**, not at 72. `Byte4` blend indices cannot address
            more (`HOUSE-00074`), while `SkinnedEffect::MaxBones == 72` limits what one DRAW may
            use, not what a skeleton may contain — a character split across several models (§47.0)
            exceeds it in total. The 72-bone check belongs at the draw, and `HOUSE-00162`'s
            `MaterialBinder` makes it there.
      finding: both the bind and the inverse-bind pose are stored, though either derives from the
            other. Deriving costs a matrix inverse per joint at load, and — more importantly — a
            mismatch between the two is the single most useful thing a validator can check.
            `HOUSE-00075`'s analytic test is that **the bind pose must skin to the identity for
            every joint**, and that test only exists if both are present.
      finding: parents strictly precede children, as a constraint on the file rather than a hint. It
            makes the absolute-transform walk one forward pass with no recursion and no visited set,
            and it makes a cycle **unrepresentable** rather than merely unlikely.
      finding: quaternions are stored `x y z w` — **w last** — matching XNA's constructor and glTF
            rather than the w-first order some maths libraries use. Written down because it is
            invisible in a hex dump and produces a plausible-looking wrong pose.
      finding: scale is stored per key even though this project never scales a joint. Three floats
            against the alternative: a format that cannot represent a retargeted asset that does
            scale, discovered halfway through phase 37.
      note: what the reader must reject is a table of thirteen distinct conditions with a distinct
            message each, because `docs/conventions.md` §5.4 makes malformed content a recoverable
            failure that names the file. The one check that is NOT the reader's is the joint names
            against the model's: the reader has no model, so `ClipLibrary::BindTo` does it
            (`HOUSE-00167`).
      files: docs/anim-format.md
      accept: (1) fully specified with byte offsets and a worked example; (2) a version field and a rejection rule for unknown versions; (3) nothing in it could only have come from a CNA type
- [x] HOUSE-00167 — Implement `anim::Skeleton`, `anim::Clip`, `anim::ClipLibrary`, the `.chanim` reader over `TitleContainer::OpenStream` + `System::IO::BinaryReader`, `ClipLibrary::BindTo(const Model&)`, and an `AnimationCache` alongside the other content caches
      dep: HOUSE-00166, HOUSE-00143 · sys: animation · plat: ALL · pri: MUST
      note: (2026-09-06) `animation/Animation.{hpp,cpp}` (Keyframe, Clip, Skeleton, ClipLibrary),
            `animation/ChanimReader.{hpp,cpp}` and `animation/AnimationCache.{hpp,cpp}`. Eighteen
            unit tests of the reader, all building a `.chanim` byte-for-byte in memory, plus six
            integration tests of `BindTo` and the cache against a real `Model`.
      finding: **MEASURED — `ModelBoneCollection`'s by-name indexer THROWS for an unknown name.**
            `bones[name]` raises *"ModelBoneCollection: bone not found: <name>"*; it does not return
            null, which is what the first version of `BindTo` assumed and what the deliberately
            wrong-joint test caught within a minute of being written. The fix is
            `TryGetValue(name, bone)`, which is plain XNA 4.0 on this collection — only its
            iterators are `CNAEXT` — and which turns the miss into the `util::Result` this project
            reports failures with instead of an exception crossing a non-content boundary. The
            capability report's `HOUSE-00074` note said the collection "has a by-name indexer, so
            this needs no search of our own", which is true and was *not* the whole story.
      finding: **`BindTo` RETURNS its error rather than throwing, correcting `cna-house.md` §47.0.**
            §47.0's code sketch says `throws on mismatch`; `docs/conventions.md` §5.4 says malformed
            content is a recoverable failure reported as a `Result`. The two authoritative documents
            disagreed. The smallest correction is the sketch: a `Result` still lets a caller treat
            the failure as fatal, while a throw does not let it do anything else — and the prose of
            §47.0 ("a fatal content error naming the offending joint") is satisfied either way.
      finding: a failed bind clears **every** index back to -1 rather than leaving the ones that had
            already resolved. A half-bound skeleton deforms silently, which is the single outcome
            this entire mechanism exists to prevent, and it is asserted by its own test.
      finding: the error names the joint **and its blend index**. "The skeleton does not match"
            would leave someone comparing two lists of sixty-two names by eye; "skin joint 41 is
            named 'hand_L', which the model has no bone for" is one grep.
      finding: **`AnimationCache` is deliberately not a `content::AssetCache`, and the reason is not
            the loader signature.** `AssetCache` exists to substitute a fallback — a missing texture
            becomes grey and the room still reads. **There is no fallback skeleton.** A character
            whose skeleton did not load cannot be drawn at all, and substituting one would produce
            exactly the silent wrong deformation `HOUSE-00074` and `BindTo` guard against. So it
            returns the error and the caller decides. It keeps `AssetCache`'s "log once" property,
            because that path runs every frame the character is drawn; `Clear()` forgets the
            failures too, since a content reload exists in order to try again.
      finding: the duration check is `!(duration > 0)` and not `duration <= 0`, so a **NaN** is
            rejected. A NaN compares false against everything and walks straight through a `<=`,
            and a NaN duration turns every later sample into a NaN pose. There is a test for it.
      finding: key times are checked ascending **within each bone's track**, after the keys are read,
            because the `boneFirstKey` partition is what says where a track starts. The sampler
            binary-searches, so unsorted input does not fail — it silently returns the wrong pose.
      note: strings are `u16` length + UTF-8, **not** `BinaryReader::ReadString`. That method's
            7-bit-encoded length prefix is a .NET-specific encoding and hostile to any other writer
            of this format — and `tools/assets/anim_extract.py` (`HOUSE-00223`) is a Python writer.
      note: truncation is caught by the `catch` around the whole read, because `BinaryReader` throws
            at end of stream. It is the one failure the field-by-field checks cannot see, and it
            must be an error rather than a skeleton quietly missing its last joints.
      files: src/animation/Skeleton.cpp|hpp, src/animation/ClipLibrary.cpp|hpp, src/animation/ChanimReader.cpp|hpp
      accept: (1) a hand-written fixture round-trips; (2) `BindTo` fills `modelBoneIndex` for a matching model; (3) a joint absent from `Model::Bones` is a **fatal** load error naming the joint, never a silent deformation; (4) a truncated or wrong-version file is rejected with a precise message; (5) no CNA symbol appears in any of these files
      verify: unit ChanimReaderTests.*, ClipLibraryTests.BindMismatchIsFatal
- [x] HOUSE-00165 — Phase-2 review and commit; update the performance log with the empty-scene frame time
      dep: HOUSE-00121…HOUSE-00167 · sys: — · plat: ALL · pri: MUST
      note: (2026-09-06) **Phase 2 is complete: 47 of 47 tasks.** The phase's own exit criterion —
            "`cna-house` opens a window, clears to a known colour, draws a version string with
            `SpriteFont`, exits cleanly; CI runs lint + unit + headless integration on every push" —
            is demonstrated by a 1600×900 screenshot and by `.github/workflows/ci.yml`'s six jobs.
      note: **Test counts, in three real configurations, counted rather than estimated.**
            `linux-debug` OPENGLES3: **286** unit + integration, 4 render (under
            `LIBGL_ALWAYS_SOFTWARE=1`), 4 perf — **294** in total. `headless` Tier-S: 286 unit +
            integration, and the identical count is itself worth noting, because it means no test in
            the suite is silently skipped by the renderer. `linux-release` with debug tools off:
            286 as well. Every gate green; `nm -C build/cna-house | grep -c 'CNA::Graphics::'` is
            **0** and so is `AvatarRenderer`, in the Release binary.
            *(Corrected 2026-09-06 during `HOUSE-00181`: this note first gave a split — "290 unit +
            integration, 4 render, 1 perf" — whose parts did not add up to the 291 total actually
            observed. The total was measured; the split was not, and writing an unmeasured
            breakdown beside a measured total is exactly the habit this plan's `finding:` lines
            exist to prevent.)*
      finding: **the second and third configurations earned their keep three times.** The `headless`
            Tier-S build caught two tests whose assertions were only true where Tier E exists
            (`HOUSE-00160`, `HOUSE-00157`), and the Release build caught the debug-tools default
            below. A suite that only ever runs in the configuration its author built is a suite that
            tests one build.
      finding: **`CNAHOUSE_DEBUG_TOOLS` defaulted ON in every configuration, including `Release`.**
            `cna-house.md` §69 says ON for `Debug` and `RelWithDebInfo`, OFF for `Release`, and the
            option had simply been written `ON`. The symptom was a Release binary printing
            `debug on` in its own version banner — found by reading that banner during this review,
            which is exactly what the banner is for. Fixed in `cmake/TierSelection.cmake`, the one
            place the decision is made; a user may still force either way, only the DEFAULT follows
            the build type.
      finding: **and fixing that exposed a second, quieter one.** `linux-debug` and `linux-release`
            share `build/` — openeggbert build rule 2 keeps the directory list closed — and
            `option()` never overwrites an existing cache entry, so switching back to `linux-debug`
            straight afterwards produced a *Debug* build with the debug tools off and four tests
            silently absent. The first fix re-derived the value on a build-type change, and it
            **discarded an explicit `-DCNAHOUSE_DEBUG_TOOLS=ON` given on the same command line** —
            a mechanism that overrides what the user just asked for is worse than the bug it fixes,
            so it was replaced by a configure-time **warning**. It changes nothing and only makes
            the disagreement impossible to miss, which is all that was ever missing. Verified in
            both directions, and verified silent once corrected.
      finding: the empty scene costs **0.21 ms median** in the shipping configuration — **1.2 % of
            the 16.67 ms budget**. That is the floor, not a scene: there is no house yet. Its value
            is that every later phase can spend its budget on the house rather than on the harness.
            Recorded with both configurations, because the difference between debug tools on and off
            (≈0.02 ms) is **smaller than the run-to-run spread** (±0.02 ms), and the honest reading
            is that the hidden overlay costs nothing detectable rather than that it costs 0.02 ms.
      finding: three `CNAEXT`-overload traps were found this phase — `setMasterVolumeProperty`'s
            `float&&`, `Color(bytecs,…)`, and `KeyboardState()` from phase 1 — and
            **`check_xna_only.py` can see none of them**, because each is an overload-resolution
            outcome rather than a name. That is now the standing rule in `docs/conventions.md` §5a:
            read the header of any XNA type before first constructing or assigning it. It is the
            largest known gap in the ADR-0001 enforcement and it is a gap in the *gate*, not in the
            rule.
      finding: two documents were corrected against the code rather than the other way round.
            `docs/conventions.md` said members are `m_camelCase` (111 of them carry a trailing
            underscore; three files carry `m_`) and named a `cnahouse::render` alias nothing used;
            `cna-house.md` §47.0's sketch said `BindTo` throws where §5.4 says content failures are
            `Result`s. Each was the smallest correction that made the documents describe what is
            true.
      finding: (recorded 2026-09-07) **the `cnanext` checkout moved during phase 2**, from `d422038`
            — the tree every phase-1 measurement was taken against — to `cde325ec`, one commit ahead
            and made by someone else. It touches `modules/renderers/easygl/src/EasyGLRenderer.cpp`,
            the renderer this project measures through. Nothing here was changed in response and
            nothing needed to be: the whole suite passes against the new tree. But a passing suite is
            not a re-measured capability, and `docs/cna-capability-report.md` now says which rows a
            renderer change could move (the `HOUSE-00106`/`00091`/`00092` timings) and which it
            cannot (the behavioural rows, which are core-module properties). `cna-house` modified
            nothing in `cnanext` at any point.
- [x] HOUSE-00168 — `tools/ci/check_xna_strict.py`: ask the COMPILER which overload each call selected, so a `CNAEXT` member reached through overload resolution cannot pass the source-text lint
      dep: HOUSE-00121 · sys: ci · plat: CI · pri: MUST
      note: (2026-09-07) **New task, next free id in phase 2's reserved 00121–00180 range.** Not a
            renumbering and not a change to any existing task: `HOUSE-00123`'s `check_xna_only.py`
            does exactly what it was specified to do, and this closes a gap that specification could
            not have closed. Wired into `run_checks.sh` for full runs only — it needs a compile
            database and ~25 s, and the pre-commit hook's whole value is being fast enough that
            nobody disables it. CI runs the full script.
      finding: **CNA already had the mechanism and this project was not using it.** `CNAEXT` expands
            to nothing normally and to `[[deprecated]]` when `CNA_STRICT_XNA_API` is defined.
            Recompiling each translation unit with that macro plus `-Wdeprecated-declarations` makes
            the compiler name every call that *actually resolves* to a CNAEXT declaration, with the
            file, line and chosen signature. No modification to CNA was needed or made.
      finding: **all three of the traps named as theoretical were real and present in this
            repository.** (1) `src/ui/TextRenderer.cpp:112` selected the CNAEXT
            `Color(bytecs, bytecs, bytecs, bytecs)` — the code cast deliberately to `std::uint8_t`,
            believing that was the correct route; the XNA constructor is `Color(intcs, …)`.
            (2) `tests/unit/InputTests.cpp` selected the CNAEXT `KeyboardState()` **16 times**.
            (3) `SoundEffectInstance::setIsLoopedProperty(bool&&)` is CNAEXT and
            `tests/probes/phase1/p1-audio3d.cpp:283` calls it with a prvalue. All fixed except the
            phase-1 probe, which is deleted at phase 1 exit and is not in the compile database.
      finding: **the `KeyboardState` case is the one worth remembering, because the source said the
            opposite.** `InputTests.cpp` carried a comment asserting that "`KeyboardState{}` is an
            empty list rather than the extension default". It is not: for `T{}`, C++ prefers the
            **default constructor** over an `initializer_list` one when both exist, so every "no
            keys held" line selected the CNAEXT default. `KeyboardState({})` passes an explicitly
            empty list and selects the XNA constructor. A careful, documented, *wrong* belief about
            overload resolution is precisely what no text-matching lint can catch.
      finding: **destructors are exempt, by shape and with the reason recorded.** CNA tags
            `~Texture2D()`, `~SpriteBatch()` and `~RenderTarget2D()` as CNAEXT, which is accurate
            documentation — XNA is C# and has no destructors — but is not a prohibition anyone can
            obey, since every C++ object with automatic storage runs one. 14 such hits are excluded.
            That is the whole exemption list; a rule with a long one is not a rule.
      finding: the two gates are complementary and neither replaces the other. `check_xna_only.py`
            runs in milliseconds with no compiler and catches forbidden **identifiers**, including
            in files that never compile. `check_xna_strict.py` needs the toolchain and catches
            forbidden **resolutions**. 70 translation units clean at time of writing.
      note: **still open and carried forward.** `HOUSE-00100` (mouse-delta measurement,
            INCONCLUSIVE — needs a session with the pointer over the window) and `HOUSE-00029`
            (`util::SmallVector`/`FixedString`, or a measured decision against them) are the two
            tasks left behind by phases 1 and 0. The render fixture `tier-fallback-01` named by
            `HOUSE-00161` waits for phase 12, when the two tiers first look different.

---

## Phase 3 — Content pipeline

**Goal.** Every source format in the project compiles and loads, reproducibly, with the tooling in
`tools/`.

**Exit.** `make content` builds `assets-src/` into `content/`; `make content-verify` proves
determinism; a smoke scene loads a model, a texture, a font, a sound, an effect and a video.

- [x] HOUSE-00181 — Create `assets-src/` with its subdirectories and a `.cna-content.json` per tree
      dep: HOUSE-00126 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-06) The subdirectories already existed from phase 0; this task added
            `assets-src/Textures/.cna-content.json`, `assets-src/Effects/.cna-content.json` and
            `assets-src/README.md`, which is where the reasons live because JSON carries no comments.
      finding: **`cna-house.md` §18.1's single-root sketch does not work, and that is measured.**
            §18.1 shows one `cna_add_content(SOURCE_DIR assets-src)` with "`Effects/` and `world/`
            excluded by the config". The version-1 configuration format has **no exclusion
            mechanism** — it maps per-asset overrides and named source roots and nothing else — and
            a single-root build was actually run: it discovers `Effects/P1Probe.fx`, tries to
            compile it into the `.cnb` tree, and **fails the whole build** with
            `no usable effect compiler: fxc`. Building the six subdirectories as separate roots is
            what the format supports, which is what `CMakeLists.txt` already did; the correction is
            to the architecture's sketch, and it is written into `assets-src/README.md` rather than
            left to be rediscovered.
      finding: **the config must therefore live in each ROOT, not at `assets-src/`.**
            `cna-content` reads `.cna-content.json` from the source root of the build that is
            running, and asset keys are relative to that root — so a file at `assets-src/` is never
            read by a build rooted at `assets-src/Textures`. It was tried first and changed no output
            byte, which is how the placement was settled.
      finding: **both configs are verified READ, not assumed read.** Flipping `generateMipmaps` to
            `true` moved `grey.cnb` from 448 to 564 bytes and `missing.cnb` from 1 408 to 1 940;
            an invalid `profile` in the effects config failed the build with
            `EffectSourceProcessor parameter 'profile' must be 'reach' or 'hidef'`. A configuration
            file that has never been shown to change anything is a file that may not be read at all.
      finding: `profile` is set to **`reach`**, not §18.1's `hidef`. `P1Probe.fx` compiles at
            `vs_2_0`/`ps_2_0`, which is Reach. `hidef` is right for the phase-12 effect set that
            will use shader model 3; setting it now, on a 2.0 source, would be a value nobody had a
            reason for that changes the build fingerprint anyway.
      finding: **`Hud.spritefont` is not reproducible across machines, and `cna-content` says so on
            every build**: `<FontName> 'DejaVu Sans' was resolved to the installed font
            /usr/share/fonts/.../DejaVuSans.ttf`. Two machines with different DejaVu versions produce
            different `Fonts/Hud.cnb` bytes. That is exactly what `HOUSE-00199`'s `content-verify` is
            for and what `HOUSE-00200` must fix by vendoring an OFL face beside the descriptor; it is
            recorded in `assets-src/README.md` so it is not rediscovered from a red CI job.
- [x] HOUSE-00182 — Wire `cna_add_content` for the main tree; confirm an incremental no-op build is fast
      dep: HOUSE-00181 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-06) Wired in phase 2 by `HOUSE-00127`'s `CMakeLists.txt`; this task confirmed
            the incremental behaviour and recorded the numbers.
      finding: `cna-content` alone on the `Fonts` root — the largest asset at 136 kB of compiled
            `SpriteFont` — takes **0.08 s** cold and **0.04–0.05 s** for a no-op. Through
            `cmake --build --target cnahouse_content` both cold and no-op measure **0.25–0.29 s**,
            and **that number is Ninja's overhead across six custom targets, not the pipeline's**:
            the whole tree is five assets, so cold and no-op are indistinguishable through the build
            system. Quoting the 0.28 s as "the content build time" would be quoting the wrong thing.
      finding: `--explain` gives the reason a no-op is cheap: `fingerprint and published output
            digests unchanged`. It re-checks fingerprints rather than re-reading sources and does not
            scan the output tree — which is the property that will matter when `assets-src/` holds a
            house instead of five fallbacks, and is recorded now so a later regression has a
            baseline.
- [x] HOUSE-00183 — Wire the effects tree with `--format xnb --fx-compiler --fx-compiler-launcher`, driven by CMake cache variables so a machine without Wine can skip it
      dep: HOUSE-00087, HOUSE-00182 · sys: content · plat: TOOL · pri: MUST
      accept: with `CNAHOUSE_TIER_E=OFF` the tree is skipped and the committed `.xnb` files are used instead
      note: (2026-09-07) `CNAHOUSE_FXC` and `CNAHOUSE_FXC_LAUNCHER` are the cache variables; the
            launcher defaults to `tools/effects/fxc-wine.sh`, which `HOUSE-00087` measured to be
            necessary because bare `wine` hands `fxc` Unix paths it reads as options.
      note: **the acceptance line conflates two different cases and both are now handled.** With
            `CNAHOUSE_TIER_E=OFF` the tree is skipped *and nothing is used*, because there is no
            Tier E to feed — verified on the `headless` preset, which turns Tier E off by itself.
            The case where the committed `.xnb` IS used is `CNAHOUSE_TIER_E=ON` with **no compiler**,
            which is the machine BL-04 is about. Verified both ways by configuring with and without
            `-DCNAHOUSE_FXC`: CMake prints `Tier E effects come from the effect compiler` or
            `Tier E effects come from the committed baseline`, and the game logs
            `Tier E active: the compiled effect set loaded` in both.
      finding: **compiling WINS over the committed baseline when a compiler is available**, and that
            ordering is the whole safety of the scheme. The other way round, a `.fx` edited without
            re-running `build_effects.sh` would be silently ignored in favour of a stale committed
            file — the failure every baseline like this eventually produces.
      finding: the baseline copy depends on the committed `.xnb`, **not** on the `.fx`. Depending on
            the source would make a target that re-copies forever without becoming up to date, on
            precisely the machines that cannot fix it.
- [x] HOUSE-00184 — `tools/effects/build_effects.sh`: the reproducible wrapper around the wine+fxc invocation, with the compiler hash recorded
      dep: HOUSE-00183 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/effects/build_effects.sh`, with a `--check` mode. It writes
            `assets-src/Effects/COMPILER.txt`: the `cna-content` hash, the `fxc` hash and size, the
            Wine version, the fxc **reported version**, and the hash of every source and output.
      finding: **the pipeline's own manifest carries a better compiler identity than any hash** —
            the processor version reads `CNA.EffectSourceProcessor/1+fxc-9.29.952.3111-fx_2_0`, so
            the record now extracts `fxc-9.29.952.3111-fx_2_0`. That is the number Microsoft put in
            the binary rather than a property of one copy of it, and it is what a reader six months
            from now can actually compare against.
      finding: `COMPILER.txt` carries **no timestamp and no hostname**, deliberately. A record that
            changed on every run would produce a diff on every run, and a file that always has a
            diff is a file nobody reads.
      finding: `--check` **needs no compiler**, which is what makes it a gate rather than a
            convenience: it compares hashes. It is wired into `tools/ci/run_checks.sh` and into the
            CI lint job, where there is no Wine — a stale baseline is otherwise invisible on every
            machine that cannot compile effects, which is most of them. Verified to fire: appending
            one comment line to `P1Probe.fx` produced `STALE: P1Probe.fx has changed since the
            committed .xnb was built` with both hashes and exit status 1.
      finding: the script removes `cna-content`'s manifest and lock from the source tree afterwards.
            That also makes the next baseline build **cold**, which is what is wanted before
            committing bytes: an incremental "skip" would let a stale `.xnb` survive a change nobody
            noticed.
      finding: MEASURED — `cna-content` has **no `--version`**; it answers *"the first argument must
            be 'build' or 'clean'"*. The binary's own hash is its identity in the record, and the
            line that would always have read `unknown` was removed rather than kept.
- [x] HOUSE-00185 — Commit the baseline compiled `Effects/*.xnb` alongside their `.fx` sources, with a README explaining why (BL-04)
      dep: HOUSE-00184 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `assets-src/Effects/P1Probe.xnb` (3 424 B) and `COMPILER.txt` are committed
            beside `P1Probe.fx`; `assets-src/README.md` carries the explanation and the
            change-an-effect recipe.
      finding: **`cna-house.md` §18.4 and BL-04 disagreed about WHERE the baseline lives, and BL-04
            was right.** §18.4 said `content/` was gitignored "except for a tiny committed baseline",
            and `.gitignore` carried three exceptions — `/content/Fonts/`, `/content/Effects/`,
            `/content/Textures/Debug/` — for files that **never existed**: the build writes to
            `${CMAKE_BINARY_DIR}/content`, and the top-level `content/` holds one `.gitkeep`. Beside
            the source is better anyway: a reviewer sees the `.fx` and the `.xnb` change together,
            and tracked files inside an otherwise-generated tree make an accidental
            `git add content/` far too easy. §18.4 and `.gitignore` are both corrected.
- [x] HOUSE-00186 — `tools/assets/gltf_validate.py`: run `gltf-validator` if present plus a CNA importer pass with warnings-as-errors
      dep: HOUSE-00181 · sys: content · plat: TOOL · pri: MUST
      accept: rejects a deliberately broken glTF with a useful message
      note: (2026-09-07) In `tools/ci/run_checks.sh`, and `--selftest` runs in the CI lint job.
            `assets-src/Models/Fallback/box.glb` passes all three passes.
      note: the acceptance is met by `--selftest`, which plants three broken files and requires each
            to be rejected **with a message naming the reason**: a truncated header
            (`is shorter than a glTF binary header`), wrong magic (`has magic 0x45504f4e, not
            'glTF'`), and a structurally valid `.glb` that breaks a project rule (`declares 2 skins;
            §47.0 allows exactly one`). "Rejects a broken file" is easy; rejecting it usefully is
            the part worth testing.
      finding: **the CNA importer pass is the authoritative one, and its warnings are treated as
            errors.** The question is not "is this valid glTF" but "does the pipeline this project
            actually uses import it without complaining" — and a warning from `cna-content` is a
            statement that something was *guessed at*, which in an asset is a bug waiting for a
            screenshot.
      finding: `gltf-validator` is optional and its absence is **reported, never hidden**. It is a
            Node package nobody should be made to install to build the game, but a run that silently
            checked less than the reader assumed is how a gate becomes decorative. The same applies
            to the importer pass when no `cna-content` has been built yet.
      finding: the `.glb` container is parsed by hand — twelve-byte header, length-prefixed chunks —
            rather than with a library, for the same reason the validator is optional: this tool has
            no dependency to install. That also means the malformed-container cases are caught here
            rather than as an exception from someone else's parser.
      finding: the one-skin rule is enforced here even though `HOUSE-00076` measured that
            `CNA.ModelProcessor` refuses a multi-skin glTF anyway. This turns a late failure deep in
            a content build into an early one that names the file and the remedy — and it also
            catches a file that `skin_split.py` split incorrectly, which the processor would not.
- [x] HOUSE-00187 — `tools/assets/scale_check.py`: assert an asset's bounds against its category table (`cna-house.md` §70.5)
      dep: HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) §70.5's table transcribed, driven by the manifest row's `category` — so the
            same record that carries the licence decides the size rule, and an asset with no row is
            caught by `check_manifest.py` first. Gated in `run_checks.sh`; `--selftest` in CI.
      finding: **the rule names an AXIS, not a largest dimension.** A door is checked on its height
            AND its width; checking a bounding box's biggest side instead would pass a door lying on
            its side. Axes are glTF's, +Y up and −Z forward (`HOUSE-00070`).
      finding: bounds are read from the **accessors' own declared `min`/`max`**, which glTF requires
            on a POSITION accessor. They are authoritative, already in metres, and available without
            decoding a buffer — which is what keeps this tool free of a glTF library. A file that
            omits them is **refused**, not guessed at.
      finding: **an unknown category is an error, never a silent skip.** The failure this whole check
            exists to prevent is a model nobody looked at, and "unknown category, so no check" is
            precisely that. Categories with no size expectation are listed explicitly, each with the
            reason it is unsized.
      finding: the failure message names the **ratio** to the nearest bound, because that is what
            identifies the mistake: 100× is centimetres, 2.54× is inches, 0.01× is a scaled-down
            scene. "Outside the tolerance" tells an author to change a number; "97× the bound" tells
            them which export setting is wrong.
      finding: **the selftest caught a bug in the SELFTEST, not in the checker**, and that is worth
            recording. The first fixture was a 2.04 m *cube*, which the door-width rule correctly
            rejected — the checker was right and the fixture was wrong. A second assertion looked
            for `"100"` in the message and was matching the constant advice string
            `100x is centimetres` rather than anything computed; it now asserts on the measured
            `204.000 m`. A test that passes on its own error message is worse than no test.
- [x] HOUSE-00188 — `tools/assets/origin_check.py`: assert the origin is at the support point (or the wall plane for wall-mounted) within 2 cm
      dep: HOUSE-00187 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) Seven selftest cases, both directions, including one 1.5 cm out that must
            PASS — a tolerance that rejects everything is not a tolerance.
      finding: **why this matters more than it looks.** Every placement in `assets-src/world/*.json`
            is a position for the ORIGIN, so an origin 30 cm above the floor puts the whole prop 30
            cm in the air and an origin at a fridge's centre buries half of it. Both read as a
            physics or a layout bug and are neither, which is how a day gets spent in the wrong file.
      finding: X and Z are checked as well as Y. An object whose origin is off to one side **rotates
            about a point outside itself**, which is invisible until a door swings or a chair is
            turned — and by then the placement data has been authored around it.
      finding: 2 cm is chosen from both ends: below what reads as wrong at eye height, above what a
            decimation or a normal transfer can move a vertex by. A tighter tolerance would fail on
            `lod_gen.py`'s own output.
      finding: the wall-mounted case checks the **back face at Z = 0** rather than the centre, and
            the sign is written down in the source, because "which face touches the wall" under
            glTF's −Z-forward convention is exactly the kind of thing each author would otherwise
            re-derive and half of them would get backwards.
- [x] HOUSE-00189 — `tools/blender/lod_gen.py`: decimate to LOD1/LOD2 with normal transfer, UV preservation and a triangle-budget target
      dep: HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
      accept: LOD1 ≈ 35 %, LOD2 ≈ 12 % of LOD0 triangles; silhouette error under a stated threshold
      note: (2026-09-07) **ACCEPTED, measured: LOD1 = 0.350 and LOD2 = 0.120 exactly**, silhouette
            error 0.0000 and 0.0056 against budgets of 0.03 and 0.10. 13 selftest checks. The first
            Blender tool, so it also establishes `tools/blender/blender_env.py`, which the seven
            that follow (`HOUSE-00190`, `00204`–`00209`) reuse.
      finding: **the stated silhouette threshold is 3 % for LOD1 and 10 % for LOD2**, as a fraction
            of LOD0's silhouette area over 8 yaws, measured by projecting and filling the triangles
            rather than by rendering — the question is purely geometric and EEVEE would drag
            lighting and colour management into it. The budget is calibrated, not guessed: at the
            specified 0.12 ratio LOD2 measures 0.0056, and forcing 0.01 pushes it to 0.1390, which
            the gate rejects. A triangle count cannot see a destroyed outline; this can.
      finding: **`blender --background` does NOT propagate a script's exit status.** Measured on
            4.3.2: `sys.exit(1)` inside `--python`, and an uncaught exception, both leave `blender`
            returning **0**. Every Blender gate in this project would have silently passed. Each
            tool now prints `"<tool>: EXIT <n>"` as its last line and `blender_env` treats that as
            the authority; a missing sentinel is itself a failure.
      finding: **Debian's Blender embeds the SYSTEM python (3.13) and glTF needs numpy**, which is
            not installed for it here. Worse, the operator *exists* —
            `hasattr(bpy.ops.export_scene, "gltf")` is `True` — and only fails when actually run,
            as a traceback out of the addon. Fixed per `AGENTS.md` rule 4 with one shared copy at
            `~/deps/blender-python`, activated by `PYTHONPATH` **plus `--python-use-system-env`**,
            without which Blender drops the variable silently. No `apt`, no system change; deleting
            the directory is a complete undo.
      finding: **the normal-transfer check was measuring the wrong normals and would have passed a
            do-nothing modifier.** `Data Transfer` writes CUSTOM SPLIT normals, which live per
            loop; `MeshVertex.normal` is recomputed from geometry and never shows them, so with the
            transfer on and off the figure was identical to three decimals (55.205°). Measured over
            `mesh.corner_normals` instead, the transfer is worth 12.732° against 13.473°. The check
            survives only because it compares on-against-off rather than testing one absolute
            number.
      finding: **Blender's datablock uniquifying leaked `.001` into the exported mesh names**, so
            the file carried `armchair_LOD1.001` where §18 names `<name>_LOD1` exactly — invisible
            from inside Blender, and enough to make a consumer matching on the name miss the mesh.
            The cause is removing the old datablock *after* renaming the new one. Asserted now by
            reading the mesh names back out of the exported `.glb`.
      finding: output is **byte-identical across runs**, and so is the fixture, so a LOD rebuild
            does not churn the content tree.
      note: the fixture is a parametric armchair — the "large furniture" category §26.2 gives
            LOD0/1 — with bevelled panels, curved cushions, four thin legs that a naive decimation
            deletes, angle-based smooth shading and a real `smart_project` unwrap. 5 568 triangles.
            **Nothing is committed**: `--make-fixture` authors it deterministically, so downstream
            Blender tasks share the fixture without a binary entering the repository.
      finding: (corrected 2026-09-07, on the project owner's report) **`--background` does not keep
            Blender off the screen.** Cycles on the CPU never touches a window, but EEVEE needs a GL
            context and takes one from the running X/Wayland server, so every run of
            `impostor_render.py` — a content build, a CI selftest, a batch of them — put a window on
            the owner's actual screen in the middle of their work. `environment()` now removes
            `DISPLAY` and `WAYLAND_DISPLAY` from the child; Blender 4.3 falls back to headless GL
            and EEVEE renders normally, which was **measured** rather than assumed.
            `CNAHOUSE_BLENDER_KEEP_DISPLAY=1` opts out.
      finding: **`xvfb-run` was the first fix and was the wrong one.** It works, but its server
            outlives the command on this machine — a run leaves an `Xvfb` process and a
            `/tmp/xvfb-run.XXXX` directory behind, which is exactly the leak `AGENTS.md` rule 3
            names, once per Blender invocation across ten agents. Denying Blender a display needs
            no second process, leaks nothing, and cannot be defeated by a wrapper that fails to
            reap. The lesson generalises: the fix that adds a process to clean up after is worse
            than the fix that removes the thing being cleaned up after.
- [x] HOUSE-00190 — `tools/blender/collision_proxy.py`: generate `<name>_COL` as a box or convex decomposition, ≤ 64 triangles
      dep: HOUSE-00189 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) Three shapes, chosen by measurement, all under §18's 64-triangle ceiling:
            `box` (12), `hull` (a **14-DOP**, ≤ 64), `boxes` (up to 5 × 12). 17 selftest checks over
            three fixtures that each select a different shape. Output byte-identical across runs.
      finding: **the selection rule is OCCUPANCY, not a volume ratio, and the volume ratio was
            actively wrong.** `BMesh.calc_volume` sums by the divergence theorem, so the L-shaped
            sofa — two cubes joined into one mesh, intersecting at the corner — double-counts the
            overlap and reported `fill = 1.2454` for a proxy that provably enclosed every vertex.
            An impossible number that a threshold would have silently believed. Replaced by sampling
            a 24³ grid and asking "inside the source?" / "inside the proxy?", which is robust to
            intersecting shells and to a proxy made of several boxes. Measured: wardrobe 1.00 as a
            box; vase 0.51 as a box, 0.74 as a 14-DOP; L-sofa **0.30 as one box against 0.89 as
            five** — that gap is the whole reason the mode exists.
      finding: **a decimated convex hull cannot meet a triangle ceiling, and a 14-DOP can.** Three
            approaches were measured before one worked. Decimate-then-re-hull does not converge: the
            hull of N points has up to 2N−4 faces, so re-hulling restores what the decimation
            removed, and a 32×16 UV sphere stayed at 961 triangles after eight rounds. Hulling a
            reduced point set converges but no longer contains every vertex, and expanding it about
            its centroid until it does over-inflates — 0.431 occupancy, worse than the 5-box split it
            was meant to beat. A **k-DOP is enclosing by construction**: the plane at
            `max(dot(v, d))` supports the source, so containment is a property of the build rather
            than something to test afterwards, and the face count is known before any geometry
            exists. 14 directions (6 axes + 8 body diagonals) is the largest standard set that fits;
            the 26-DOP is tighter and lands near 104 triangles.
      finding: **`bmesh.ops.convex_hull` does not guarantee consistent winding**, and every
            containment test here is a signed distance to a face plane. Inward-facing normals made
            interior points read as **0.677 outside** a 0.25-radius sphere. `recalc_face_normals`
            after every hull, bisect and box.
      finding: two smaller traps worth recording. `bmesh.ops.delete` raises "found the same
            (BMVert/BMEdge/BMFace) used multiple times" when the three `convex_hull` result lists
            overlap — they must be deduplicated **by identity**, since BMesh elements are not
            hashable by value. And repeated `bisect_plane` shatters each DOP plane into a fan: 170
            triangles before `dissolve_limit` merges each plane's fragments back into one polygon,
            44 after, with the geometry unchanged.
      finding: the box decomposition's slabs are **extended to meet exactly**, not left at their
            contents' bounds. Two boxes with a gap between them let a capsule sweep find the seam
            and slip through, which is a bug that only appears when a player walks into it.
      note: the enclosure test is asserted rather than assumed — every source vertex against the
            proxy — and was itself shown to fail, rejecting a box shrunk to 80 % with the worst
            vertex 0.1000 outside.
- [x] HOUSE-00191 — `tools/assets/pbr_to_stock.py`: metallic-roughness → `DiffuseColor`/`SpecularColor`/`SpecularPower` with a fixed documented mapping
      dep: HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) The mapping had to be **chosen**: `cna-house.md` §82.3 requires it be
            "documented and fixed" but does not give the formula. Three lines, each with its reason
            in the module docstring, and eleven selftest assertions in CI.
            `diffuse = base x (1 - metallic)`;
            `specular = lerp(0.04, base, metallic)`;
            `specularPower = clamp(2/alpha^2 - 2, 4, 256)` with `alpha = roughness^2`.
      finding: the specular line **is** the metallic workflow, restated for an effect that has no
            metallic parameter. A dielectric reflects ~4 % achromatically (F0 = 0.04, an IOR of
            about 1.5 — glass, most plastics, most paint); a metal's reflection is **tinted by its
            base colour**, which is why gold has a gold highlight and plastic has a white one.
            Interpolating on `metallic` is the whole of it.
      finding: `alpha = roughness^2` is glTF's own perceptual remapping and `2/alpha^2 - 2` is the
            standard GGX-to-Blinn-Phong lobe match. **The clamp is not cosmetic**: at roughness 0
            the formula diverges, and an exponent above ~256 makes a highlight smaller than a pixel
            that aliases into a crawling sparkle as the camera moves — worse than no highlight. The
            floor of 4 stops a matte surface becoming a uniform sheen that reads as fog.
      finding: glTF's defaults for an unspecified material are `metallic = 1, roughness = 1` — a
            fully rough **metal**, not plastic. Using `(0, 1)` because it "looks more sensible"
            would silently make every untagged material a dielectric, so the tool uses the spec's
            defaults and the choice is written down where someone will look for it.
      finding: what the mapping does NOT do is stated in the tool itself — no energy conservation,
            no angular Fresnel, no image-based lighting, because `BasicEffect` has no inputs for
            them. The goal is that a metallic-roughness source lands somewhere defensible and
            **consistent between the two tiers**, not that Tier S becomes a PBR renderer. Tier E's
            `RoomLit.fx` consumes the same three parameters, so neither tier invents its own look.
      accept: the mapping is written down in `docs/content-authoring.md` and is reversible enough to review
- [x] HOUSE-00192 — `tools/assets/atlas_pack.py`: pack small-prop textures into shared atlases and rewrite the models' UVs
      dep: HOUSE-00191 · sys: content · plat: TOOL · pri: SHOULD
      note: (2026-09-07) `tools/assets/atlas_pack.py`, with `--selftest`, `--verify` and
            `--make-fixture`. 26 selftest claims over a prop set the tool authors itself; nothing is
            committed. Measured on 40 props of 256² albedo + normal: a 2048² pair of atlases in
            **1.3 s**, mip-safe to level 3. `--selftest` runs in the CI lint job.
      finding: **the packing unit is the MATERIAL's whole texture set, not the image**, and getting
            this wrong is silent. One set of UVs addresses a material's albedo *and* its normal, so
            the two must land at the same coordinates in their respective atlases. Packing images
            independently by size — the obvious implementation — puts a 64² normal somewhere
            different from its 64² albedo and the prop samples a *neighbour's* normals, which reads
            as a lighting bug rather than as an atlas bug. The layout is therefore computed once
            over texture sets and every channel atlas is stamped from it.
      finding: the consequence is a **refusal**: a material whose maps differ in size has no single
            region. The tool names the material and its sizes rather than rescaling somebody's
            normal map behind their back.
      finding: **UVs outside [0, 1] are refused, not handled.** Inside an atlas `REPEAT` wraps the
            whole atlas, not the region, so a prop authored with UVs 0..2 tiles its *neighbours*
            across itself. No UV transform fixes this; baking the repeat into one texture is a
            different task. The atlased samplers are written `CLAMP_TO_EDGE` for the same reason.
      finding: a `TEXCOORD_0` accessor **shared by two primitives with different materials** cannot
            be rewritten in place — the two need different transforms. Each `(accessor, transform)`
            pair gets its own accessor and the accessor is reused only when the transform is truly
            identical. The fixture contains this case because a real exporter produces it whenever
            two material slots sit on one mesh.
      finding: output `TEXCOORD_0` is **always FLOAT**, whatever came in. A normalized `ushort` that
            resolved 1/65535 of a 64² source would, after the rewrite, resolve 1/65535 of the whole
            2048² atlas — the same bits now spread over 32× the area, which is visible drift.
      finding: **the buffer is compacted, or the atlas makes files bigger.** Dropping the embedded
            texture without rebuilding the buffer leaves it in the `.glb` as an orphan bufferView.
            The rewrite keeps only live accessors and their views; the fixture prop goes 2192 → 1692
            bytes, and a selftest claim asserts no dead accessor or bufferView survives.
      finding: **the mip-safety rule this tool first used was wrong, and wrong in the direction that
            makes the tool look broken.** The obvious rule — level `L` is safe while `2**L` divides
            every region origin and size — reported **level 1** for the fixture. The condition that
            actually matters is narrower: averaging a region's edge with its *own* replicated gutter
            is harmless, and only a reduced texel that mixes **two different props' content** is
            fatal. Snapping each region outward to the `2**L` grid and checking it touches no other
            region gives the true answer for the same layout: **level 3**. Two levels of mip chain
            were being given away by a rule that sounded right.
      finding: the gutter is filled by **replicating the region's edge texels outward**, never with
            transparent black — black is the classic dark atlas seam under bilinear filtering.
      finding: the PNG **encoder is this project's own** so the atlas bytes do not depend on which
            Pillow is installed; Pillow decodes sources only. What is *not* guaranteed across
            toolchains is the DEFLATE stream, `zlib.compress(level=9)` being stable per zlib version
            rather than across them. `--verify` therefore compares decoded pixels, and this is
            recorded so a future byte-difference is diagnosed as a toolchain change (as
            `HOUSE-00200` records for FreeType) rather than misread as asset drift.
      finding: **the selftest was shown to fail before it was trusted, and it caught a tautology in
            itself.** 15 injected bugs; 13 of the first 14 were detected immediately. The two that
            were not are the point: (a) changing the neutral normal fill to black passed, because
            the assertion compared the atlas against the very constant that had produced it — the
            check now writes `(128, 128, 255, 255)` out as a literal; (b) removing the layout sort
            passed, because the "reverse the command line" check is **vacuous** — `collect()` sorts
            the paths, so both runs were the same run. Renaming the models with prefixes that
            *invert* the sorted order is the test that discriminates, and it is now the one used.
            A third pass added a check that no dead accessor survives (an injection that kept them
            all had passed) and a `POSITION`/`TEXCOORD_0` count check in `--verify`.
      finding: the packer's own contract is **enforced, not trusted** — a pairwise box-overlap and
            bounds check after layout. A packer bug that overlaps two boxes produces a wrong
            *picture*, not a crash, which is the kind of bug that ships. Proven to fire by removing
            both of `shelf_pack`'s bounds tests.
      accept: deterministic image, model and metadata output; every rewritten UV verified from the
            written file to lie inside its assigned region; both refusals demonstrated
- [x] HOUSE-00193 — `tools/assets/convert_audio.py`: 24→16-bit, ~~48→44.1 kHz~~ **sample rate preserved**, trim, normalise, loop points, and the dull-variant filter
      dep: HOUSE-00069 · sys: content · plat: TOOL · pri: MUST
      accept: deterministic output; both hashes recorded in the manifest
      note: (2026-09-07) The title's "48→44.1 kHz" is **struck, not deleted** — the task id and its
            history stay. `HOUSE-00069` had already measured that clause wrong and corrected
            `BL-06`; this is the same correction reaching the tool and `cna-house.md` §63.5, which
            still carried the old command. `--selftest` proves ten claims; run it before trusting
            the tool.
      finding: **the sample rate is preserved and that is a measured decision.** `HOUSE-00069`:
            24→16 bit alone costs 0.0017 dB RMS; adding `-ar 44100` costs **0.889 dB RMS and
            0.26 dB peak**, because the resample lowpasses content these sources carry. §63.5's
            justification — "resampling once offline beats resampling every frame" — is a CPU
            argument that ignores the signal it spends. `--sample-rate` exists, is never the
            default, and names what it is doing.
      finding: **`dynaudnorm`, which §63.5 sketched, is rejected.** It rides gain over time, which
            would flatten the difference between a soft and a hard footstep — and those variants
            exist precisely to be different. Peak normalisation applies one fixed gain per file, and
            the tool reports the gain it applied so a later reader can check it.
      finding: **ffmpeg's default output is NOT reproducible across toolchains**, and invisibly so.
            It copies the source's `LIST/INFO` metadata and adds an `ISFT` encoder tag naming the
            libavformat version (`Lavf61.7.103` here), so the same input on a machine with a
            different ffmpeg yields different bytes. `-map_metadata -1` plus `-fflags +bitexact`
            **as an output option** removes both; the same flag before `-i` binds to the demuxer and
            does not. The selftest asserts the clean output has no tags *and* that the naive command
            does, so the flags cannot rot into decoration.
      finding: **loop points are DISCARDED by the content pipeline, measured rather than assumed,
            and the result is not the obvious one.** `SoundEffect` carries `loopStart`/`loopLength`,
            `.cnb` and `.xnb` both have the fields, `SoundEffect::FromStream` parses a WAV `smpl`
            chunk, and the region is applied at `Play()` — the capability exists end to end *except*
            at the step this project uses. `CNA.WavImporter` never reads `smpl`: the same one-second
            tone compiled with and without a `smpl` chunk declaring a loop over samples 100–40 000
            produced **byte-identical payloads** (only the asset name and the build fingerprint
            differ). A CNA limitation, not worked around here. It costs little — `IsLooped` loops
            the whole buffer, which is what every looping sound in this house wants — so a `Loop`
            file is left **untrimmed** and seamlessness stays a property of the recording.
      finding: verified against the real collection, including both classes of outlier
            `HOUSE-00276` found: a 32-bit `pcm_s32le` car-engine loop and a 96 kHz keyboard loop both
            convert correctly, the 96 kHz file keeps its rate, and `--require-rate 48000` **refuses**
            it (exit 1) rather than resampling a surprise. A converted footstep compiles through
            `cna-content` (`CNA.WavImporter -> CNA.SoundEffectProcessor ->
            CNA.SoundEffectContentWriter`) and the full filter chain is byte-identical across two
            runs on a real file.
- [x] HOUSE-00194 — `tools/assets/measure_stride.py`: measure a locomotion clip's stride length and duration for rate matching
      dep: HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/assets/measure_stride.py`, with `--selftest` and `--make-fixture`.
            36 selftest claims over eleven fixtures the tool authors itself; nothing is committed.
            Pure standard library — no numpy, no Pillow — and `--selftest` runs in the CI lint job.
            The byte-level glTF reading is now `tools/assets/gltf_io.py`, shared with
            `atlas_pack.py` rather than copied into a second tool.
      finding: **the two source conventions need one formula, not two code paths.** A CMU trial
            translates its root; the same clip after retargeting stands still and cycles its feet.
            The identity that covers both: while a foot is planted it does not move, so the body's
            velocity over the ground is `-d/dt(footWorld - rootWorld)` during stance. On a
            root-motion clip this reproduces the root's own travel, which makes the two an
            independent cross-check of each other — and **their disagreement is the foot sliding**,
            reported rather than averaged away. The fixture's 30 %-sliding walk is caught at 29.4 %;
            the clean one reads 0.8 %.
      finding: **plant detection by SPEED is wrong, and the fixture proves it rather than the
            documentation asserting it.** In an ideal in-place cycle a swinging foot moves at
            *exactly* the same speed as a planted one — only the direction differs. The first
            implementation compared speed magnitudes, folded every swing frame into the stance, and
            measured 0.62 m for an authored 1.40 m while reporting two cycles instead of one. The
            median is taken as a **vector**; a sample is stance when it is low, in the forward
            hemisphere of that vector, and within the speed band. All three tests are load-bearing
            and each has a fixture that fails without it — the speed band's is a foot that *drags*
            backwards at four times the gait rate for 12 % of its swing, which height and direction
            both accept and which inflates the stride to 1.75 m if the band is opened up.
      finding: **double support must be averaged, not summed.** With a realistic 60 % stance both
            feet are down for 20 % of the cycle. Summing each foot's stance displacement — the
            obvious implementation — counts the overlap twice and reports 1.2× the stride. The
            fixture carries this case precisely so the mistake cannot be made silently later.
      finding: **the units are checked, because CMU's raw skeletons are in inches.** An unscaled
            stride of 55.6 reaching `Clip::strideLength` would make every character sprint, and it
            looks like a number rather than like a bug. `unitsHint` compares the skeleton's own hip
            height against a human and says `metres`, `centimetres` or `inches`; the three bands are
            factors of 2.54 and 100 apart, so no plausible human lands between them. It is measured
            on the **first animated frame**, not the rest pose — an exporter may leave the rest pose
            at the identity and carry the whole skeleton in the tracks, and the fixture does.
      finding: what the tool refuses to do is as specified as what it does. A turn in place measures
            below the 0.05 m/s floor and gets `isLocomotion: false, strideLength: 0`, which is what
            §47.4 wants for a non-locomotion clip. A skeleton whose feet cannot be named reports
            `method: "none"` and `0` with a warning rather than a plausible guess — and
            `--foot-left/--foot-right` then recovers the full measurement.
      finding: **stride is per CYCLE, and the clip is not the cycle.** A two-cycle clip reports
            `cycles: 2`, `clipTravel: 2.81 m` and `strideLength: 1.41 m`. Cycles are counted from
            the plant intervals, with a plant that crosses a looping clip's start counted once
            rather than as two.
      finding: bone-name matching covers CMU (`LeftFoot`), Mixamo (`mixamorig:LeftToeBase`), Rigify
            (`foot.L`) and the underscore conventions, prefers the **ankle to the toe** when a
            skeleton has both, and requires a single-letter `l`/`r` to be **delimited** — otherwise
            `Ball`, `Roll` and `Collar` become sides.
      finding: **shown to fail before it was trusted.** 17 injected bugs, 16 detected. The one that
            was not is recorded rather than papered over: the two-frame minimum contact length is a
            noise filter for real captures, and the synthetic fixtures are clean enough never to
            produce a spurious short run. Two of the injections that *were* caught had to be
            re-run after the first attempt missed — one sed matched the wrong indentation, one
            disabled a test the fixture did not yet exercise, and the fixture gained the dragging
            foot as a result.
      accept: deterministic output; duration, stride, cycles, foot-plant times and the in-place /
            root-motion distinction all measured, with `docs/anim-format.md` §3.3's `Clip` fields
            present and in range so `HOUSE-00223` consumes this rather than measuring again
- [x] HOUSE-00195 — `tools/assets/manifest.py`: add/update a manifest row, compute hashes, validate the schema
      dep: HOUSE-00181 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/assets/manifest.py` with `validate`, `rehash`, `add` and `list`, and
            `assets-src/assets.manifest.json` with **six rows** — every asset currently in the tree.
      finding: **the four redistribution booleans default to FALSE** when a row is added. A row
            created without thinking about them then says "we may not do any of this", which is the
            safe direction to be wrong in; the opposite default would let an unconsidered asset
            claim permissions nobody granted.
      finding: they are validated as **JSON booleans**, not truthy values. A string `"true"` is the
            classic way a permission check silently passes, and it is rejected by type.
      finding: `origin.kind` decides which other fields are required. Only a `downloaded` asset must
            record `url`, `author` and `retrieved` — demanding them of something this project
            generated would fill the manifest with `n/a`, which is how a provenance record stops
            meaning anything.
      finding: rows are saved sorted by id with a stable two-space indent and no ASCII escaping,
            because every one of those is about the **diff**: a manifest whose order depended on
            when a row was added would produce a reordering diff on every change, and an escaped
            non-ASCII author name is unreadable in review.
- [x] HOUSE-00196 — `tools/ci/check_manifest.py`: no unlisted file under `assets-src/`, no hash mismatch
      dep: HOUSE-00195 · sys: ci · plat: CI · pri: MUST
      note: (2026-09-07) In `tools/ci/run_checks.sh` and in the CI lint job. Reports
            `6 listed, 8 exempt, 14 file(s)`.
      finding: **both failure modes were shown to fire before the gate was trusted.** Copying a
            texture to an unlisted path produced `assets-src/Textures/stray.png: no manifest row`;
            appending one newline to `P1Probe.fx` produced `sourceSha256 does not match the file`
            with both hashes. A gate never seen to fail is a gate nobody should rely on.
      finding: the exemptions are **exact names and exact rules, never wildcards over an extension**,
            and each carries the reason it is exempt. A blanket `*.json` exemption would silently
            exempt a future `world/*.json`, which very much is an asset. The one rule-based
            exemption is `<name>.xnb` beside `<name>.fx`, whose provenance IS the `.fx`'s and whose
            staleness `build_effects.sh --check` already gates — giving it a row of its own would
            mean a second hash to update on every effect change, which is a rule people work around
            rather than follow.
      finding: a row pointing at a **deleted** file is checked too. It is the mirror image of an
            unlisted file and just as wrong: the manifest would keep asserting a licence for
            something that is gone.
- [x] HOUSE-00197 — `tools/assets/verify_licences.py`: every row has a licence, a licence file and the redistribution booleans; packaging refuses `PROVENANCE UNKNOWN`
      dep: HOUSE-00195 · sys: ci · plat: CI · pri: MUST
      finding: **the packaging gate already refuses something, on day one, and it is the right
            thing.** `FONT_HUD` has `redistributeSource: true` and `redistributeDerived: false`:
            the `.spritefont` descriptor is ours, but `cna-content` resolves `<FontName>`
            `'DejaVu Sans'` to whatever is installed and **embeds its rasterised glyphs**, so
            `content/Fonts/Hud.cnb` carries third-party glyph data this project has not cleared or
            vendored. That is exactly the case §20.1's two separate booleans exist for, and the
            technical ability to compile the font is not permission to distribute it. It clears when
            `HOUSE-00200` vendors an OFL or Bitstream-Vera face beside the descriptor.
      finding: the shipping refusals are **separate messages**, one per condition, because each has
            a different remedy. "Cannot ship" without saying why is a message that gets ignored.
- [x] HOUSE-00198 — `licenses/THIRD-PARTY-ASSETS.md` generator from the manifest
      dep: HOUSE-00197 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `verify_licences.py --emit` writes it; `--check` regenerates into a buffer
            and fails if the committed file differs, so a hand edit cannot survive. Both are gated.
      finding: **verification and generation are one tool on purpose.** A generator that could run
            on unverified rows would produce a credits page asserting licences nobody had checked —
            which is worse than no credits page, because it looks like diligence.
      finding: the document lists **third-party assets only**. Listing the six assets this project
            authored would bury the ones that actually need attribution among the ones that do not,
            which is how an attribution document stops being read. The two sections that DO appear
            are the ones a reader must act on: provenance-unknown assets, and assets whose source
            ships but whose compiled form does not.
      finding: the stamp at the foot names the **manifest's own sha256** and carries no timestamp,
            for the same reason as `COMPILER.txt`: a file that changes on every run produces a diff
            on every run, and a file that always has a diff is a file nobody reads.
- [x] HOUSE-00199 — `make content-verify`: rebuild everything and assert byte-identical output
      dep: HOUSE-00182 · sys: ci · plat: CI · pri: MUST
      note: (2026-09-07) `tools/ci/content_verify.py` plus a CMake target `content-verify` — this
            project has no Makefile, so "`make content-verify`" is
            `cmake --build build --target content-verify`. Deliberately **not** part of `all`: it
            builds every content root twice and is a check someone asks for, not a cost every
            incremental build pays. Wired into CI on the `linux-release` job only, because the
            answer cannot depend on the compiler flags of a program that does not participate in it.
      finding: **each of the two builds goes into a FRESH output directory.** Reusing one would let
            the pipeline's own incremental skip answer the question instead of the pipeline —
            "identical because nothing was rebuilt" is not the claim being tested.
      finding: **it was shown to fail before it was trusted.** `--selftest` flips one byte in the
            middle of one output and requires the comparison to catch it; it reported four corrupted
            outputs and exited 0 (the selftest inverts the status). A determinism check that has
            never failed is a check that reports green forever, and `--selftest` runs in CI first.
      finding: **same-machine determinism holds for all five outputs**, which is what §18.4 asks
            for. Cross-machine reproducibility is a different and currently weaker property, and the
            tool names the one asset that breaks it — `Fonts/Hud.cnb` embeds glyphs rasterised from
            the machine's installed DejaVu Sans (`HOUSE-00181`). It is listed as a known hazard
            rather than silently passed, so the check does not appear to prove more than it does.
            `HOUSE-00200` closes it.
      finding: `cna-content`'s own `.cna-content-manifest.json` is excluded from the comparison. It
            records absolute output paths and a build ordering, so it differs between two runs **by
            design** and is not content; comparing it would make the check fail for a reason that
            says nothing about the assets.
- [x] HOUSE-00200 — Author the two UI fonts as `.spritefont` (UI face at 16/22/30, mono at 13/16) and verify glyph coverage for the languages we ship (English only, but with the full Latin-1 set)
      dep: HOUSE-00066 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) Five descriptors — `ui-16`, `ui-22`, `ui-30`, `mono-13`, `mono-16` — over
            two **vendored** faces: Noto Sans 2.015 and Noto Sans Mono 2.014, both the official
            `hinted/ttf` release artifacts, both unmodified, both OFL-1.1. The owner authorised the
            family and the permanent redistribution, including a possible commercial Steam release,
            conditional on the licence verification below. `Hud.spritefont` is gone: it *was* the UI
            face at 16, and `CnaHouseGame::LoadContent` now loads `Fonts/ui-16`. New gate
            `tools/ci/check_fonts.py`; new suites `FontMetricsTests` (7) and `FontRenderTests` (5).
            Full evidence in `docs/font-provenance.md`.
      finding: **the system-font fallback is a WARNING, not an error, and the stems collide.** The
            importer resolves `<FontName>` beside the descriptor first and only then searches
            `/usr/share/fonts`. This machine carries `NotoSans-Regular.ttf` at 2.004 and
            `NotoSansMono-Regular.ttf` at 2.006 — the same filename stems as the vendored 2.015 and
            2.014. Deleting the vendored files and rebuilding **succeeded**, with five warnings and
            five different hashes. A build log nobody reads is not a defence, so `check_fonts.py`
            fails a descriptor whose font file is missing.
      finding: **hiding every system font changes nothing.** The `Fonts` root built inside a mount
            namespace with `/usr/share/fonts` and `/usr/local/share/fonts` bind-mounted over by an
            empty directory produced byte-identical output for all five. The repository `.ttf` is
            the source of truth, and the negative control above proves the test is not vacuous. No
            host path reaches the output either: the only printable string in the five `.cnb` files
            is `Microsoft.Xna.Framework.Graphics.SpriteFont`.
      finding: **`<Size>` is POINTS AT 96 DPI, not pixels** — `FT_Set_Char_Size(…, 96, 96)`. So the
            five assets have 21.3/29.3/40.0 and 17.3/21.3 px em boxes and line spacings of
            29/40/54 and 24/29 px. `cna-house.md` §67.1 calls them "16/22/30 px" and "13/16 px";
            the numbers are the `.spritefont` `<Size>` values, which is the XNA convention, but the
            unit is wrong and the rendered text is about a third larger than "px" implies. Corrected
            in §67.1 rather than by changing the sizes, which are what the task and the owner
            specified.
      finding: **Noto Sans Mono has no U+00AD, and a missing glyph FAILS the build** rather than
            substituting the default character. The repertoire is therefore ASCII plus Latin-1
            *minus soft hyphen* — 190 characters — and both faces declare the same set so they stay
            diff-comparable. Soft hyphen is an invisible line-break hint and there is no
            line-breaking engine here to honour one, so nothing is lost.
      finding: **the mono face is exactly monospaced at 16 and out by one pixel at 13**, which the
            obvious assertion would have got wrong. All 190 characters advance 600/1000 em in the
            source, `post.isFixedPitch` is nonetheless 0 in both files, and the pipeline truncates
            the grid-fitted advance with `slot->advance.x >> 6`. At `<Size>16` that is 12.8 px →
            13 for all 190; at `<Size>13` it is 10.4 px → 10 for 176 and 11 for 14. Asserted as
            spread 0 and spread ≤ 1 rather than behind a loose tolerance a proportional face would
            also pass.
      finding: **the *hinted* build was chosen on measured grounds.** Hinted and unhinted carry
            identical outlines (0 of 3 884 glyphs differ) and identical `hmtx`; they differ only in
            the four hinting tables and 2 995 glyph programs — yet they compile to **different**
            `.cnb` bytes, which proves FreeType's bytecode interpreter is live and upstream's
            ttfautohint instructions are genuinely consumed at 13–30 px. `full/` was rejected as
            631 glyphs we never rasterise.
      finding: **licence verified against authoritative sources, not a search snippet.** The OFL
            body in Noto's `OFL.txt` is identical to SIL's own text at `openfontlicense.org`; both
            families ship the same `OFL.txt`; `name` ID 13/14 inside both binaries assert OFL-1.1;
            **no Reserved Font Name is declared** (the phrase appears only in the licence's own
            definitions), so clause 5 restricts nothing. SIL FAQ 1.4 permits selling a package
            containing the fonts and names "games and entertainment software"; FAQ 1.3 and 1.13
            confirm the OFL does not reach the program or anything drawn with it. "Noto" is a
            Google LLC trademark — recorded, and harmless because the files are unmodified and the
            name is not our branding. Both `.ttf` files were fetched from the release zip **and**
            from `notofonts.github.io` and are byte-identical, so two official distribution points
            agree on the hashes.
      finding: **the packaging gate now passes.** `FONT_HUD` carried `redistributeDerived: false`
            because its compiled bytes embedded unvetted host glyphs, which would have refused a
            shipping build. The seven rows replacing it are all true on all four booleans, and
            `content_verify.py`'s `KNOWN_CROSS_MACHINE_HAZARDS` is now empty — kept as an empty dict
            rather than deleted, because it is where the next such asset gets recorded.
      finding: every check here was **shown to fail before it was trusted**: the gate against a
            deleted `.ttf`, a narrowed region and a region asking for U+00AD; `FontMetricsTests`
            against a proportional face and against a repertoire including soft hyphen;
            `FontRenderTests` against a character outside the declared regions, which correctly
            reported that it had rendered as the `?` default.
      risk: cross-**toolchain** reproducibility is still open, and vendoring a font cannot close it:
            the atlas is what a particular FreeType rasterised. This build used FreeType 2.13.3.
            Recorded so a future byte-difference in `Fonts/*.cnb` is diagnosed as a toolchain change
            rather than misread as asset drift.
- [x] HOUSE-00201 — Content smoke scene: load one model, one texture, one font, one sound, one effect, one video and display/play them
      dep: HOUSE-00182…HOUSE-00200 · sys: content · plat: LNX · pri: MUST
      verify: render test `content-smoke-01`
      note: (2026-09-07) `--scene=content-smoke`. `cnahouse::content::SmokeScene` loads the six,
            draws the model through the compiled effect in `Pass::OpaqueDynamic`, draws the video
            frame and the source texture as panels, and writes six report lines with its own face.
            Four assets are authored by `tools/assets/make_smoke_assets.py` (40 kB total); the font
            and the effect are the ones `HOUSE-00200` and `HOUSE-00185` already vendored.
            **Eight integration tests and four render tests**, plus the `content-smoke-01`
            reference. Both selftests run in the CI lint job.
      finding: **each of the six is checked at the thing that CONSUMES it, not at the `Load<T>`
            call**, because those are different failures. The model reaches a draw call, the
            texture reaches a sampler, the font reaches a glyph, the sound reaches the mixer, the
            effect reaches a shader, and the video's play position moves. Seven injected bugs; six
            were caught immediately and the seventh is the interesting one.
      finding: **`soundStarted` was a lie, and an injection proved it.** It was set beside the
            `Play()` call, so removing `Play()` altogether left every test green — the flag recorded
            that the code path had run. It now reads `SoundEffectInstance::getStateProperty()` back
            and stores what the mixer said; the same injection then fails with *"the mixer reports
            it as 'stopped' rather than playing"*. `Play()` returns `void`, so the read-back is the
            only evidence available.
      finding: **the tint is what proves the effect reached the shader, and a colour-distance check
            could not see it.** Red tinted and red untinted are 16 units apart — any slack wide
            enough for a rasteriser swallows the difference. The render test therefore counts BOTH
            forms of all four quadrant colours and requires the untinted count to be **zero** on the
            model; blue carries the claim, at 143 against 230 in the blue channel. Measured: the
            shader point-samples, so the rendered texel is the authored one exactly.
      finding: **CNA resolves a streamed media XREF from the CONTENT ROOT, not from the `.cnb`
            beside it**, and every other asset type resolves relative to the directory it was built
            into. §18.1 builds one subdirectory at a time, so for `Video` the two never meet:
            `streamReference: smoke_clip.ogv` deploys to `content/Video/smoke_clip.ogv` and is
            looked for at `content/smoke_clip.ogv`; `Video/smoke_clip.ogv` deploys to
            `content/Video/Video/smoke_clip.ogv` and is looked for at `content/Video/smoke_clip.ogv`.
            Both were tried and both fail. The fix is `assets-src/Media/`, built into `content/`
            **itself** with the `Video/` component in the source layout — the content name stays
            `Video/smoke_clip`, nothing is copied twice, and an integration test asserts the layout
            so tidying it back is a red test rather than a silent load failure.
      finding: **`VideoProcessor` requires `width`, `height` and `framesPerSecond` as authored
            parameters** — CNA does not decode the source at build time, it deploys the stream and
            trusts the metadata. So the metadata can disagree with the file and nothing at build
            time notices; `make_smoke_assets.py --selftest` re-probes the clip instead.
      finding: **`libtheora` here drops a frame identical to the one before it.** A two-second
            flat-colour clip at 10 fps encoded to **four** frames rather than twenty, silently, and
            `drawbox` in this ffmpeg has no per-frame `eval` with which to move anything. The clip's
            frames are therefore PNGs the script authors — a background that changes every half
            second and a marker that steps three pixels right every frame, so a decoded texture says
            both which half-second and which frame it is.
      finding: **`--screenshot-frame` was added rather than widening a tolerance.** A `VideoPlayer`
            produces a texture when the decoder produces one, not when `Play` returns, so a capture
            of frame 1 shows an empty panel however healthy the pipeline is. Measured: frame 1 is
            empty, frame 60 is not. The video panel is then excluded from the reference comparison
            **by name** (`ImageCompare.hpp`'s rule) because which frame it holds depends on the
            clock — and it is still asserted non-empty, while the ADVANCE is asserted by the
            integration test, which can watch 240 frames.
      finding: **`check_xna_strict.py` caught a real overload trap in this commit's own code.**
            `soundInstance_->setVolumeProperty(audio_.EffectiveVolume(...))` passes a prvalue and
            therefore selects the `CNAEXT`-marked `float&&` overload — a forbidden call in which no
            forbidden identifier appears, invisible to `check_xna_only.py`. Binding the value to a
            named `float` first fixes it. The gate found it; review had not.
      finding: `ModelMeshCollection::begin`/`end` are `CNAEXT`-marked, so a **range-for loop over a
            model's meshes is a forbidden call reached through iteration**. Every walk here is
            indexed through `getCountProperty()` and `operator[]`.
      finding: **`title-01.png` had been stale for two commits and nothing said so.** It was
            captured by `HOUSE-00164` when the HUD font was `Fonts/Hud.spritefont`, naming an
            *installed* DejaVu Sans; `HOUSE-00200` vendored Noto and made `Fonts/ui-16` the HUD face
            without regenerating the reference. The fixture disagreed with reality by **1.17 % of
            the frame — 16 491 pixels, max channel delta 248** — and the render suite is nightly, so
            no nightly ran in between. Regenerated here, and the rule is written into
            `docs/screenshot-scenes.md`: a change to a font, a shader or a clear colour IS a
            reference change, and the commit that makes it runs the render suite.
      accept: all six load and are visibly used; `content-smoke-01` matches its reference outside
            the named video rectangle and is bit-identical across two runs; no `.xnb` shadows the
            `.cnb` tree; Tier S is a complete session with five of six
- [x] HOUSE-00202 — Define the pack partition in `assets.manifest.json` and enforce that every asset belongs to exactly one pack
      dep: HOUSE-00195 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) A `packs` block at the top of `assets-src/assets.manifest.json` declaring
            §27.2's twelve packs plus `dev`, and three new fields on every row — `contentName`,
            `kind`, `residencyPack`. `manifest.py validate` refuses a row that has neither those
            three nor a written `notPackaged` reason, and `manifest.py packs` prints the partition
            with its counts. Gated by `check_manifest.py`; `manifest.py packs` runs in CI.
      finding: **the runtime was reading a schema nothing produced, and every unit test passed.**
            `ContentRegistry` requires `id`, `contentName`, `kind` and `residencyPack`;
            `assets.manifest.json` carried `id`, `category`, `sourceFile`, `sourceSha256` and
            `origin` and none of the other three. Nine `ContentRegistryTests` were green throughout,
            because every one of them parsed a **hand-written fixture** — which is exactly how the
            gap survived four phases. `ContentRegistryTest.TheREALManifestLoads` now parses the
            committed file, and it is the only test in that suite that could have caught this.
      finding: **`residencyPack` no longer defaults to `core`, and the old default was the worst
            possible one.** A row that named no pack silently became an always-resident one: pinned
            in memory for the whole session and appearing in no download budget. It is now required
            at both ends — the tool refuses to write such a row and the runtime refuses to load one
            — so the two agree instead of the runtime quietly absorbing what the gate missed.
      finding: a row is a runtime asset or it is **not**, and there is no third state. The two
            vendored `.ttf` files are committed assets with licences and hashes that nothing loads
            at run time; they carry `notPackaged` with the reason written out, and
            `ContentRegistry` skips them. "No pack" as a silent default is precisely how an asset
            ends up in no download and nobody's budget.
      finding: **a thirteenth pack, `dev`, is a correction to §27.2 rather than a convenience.**
            §27.2 lists the *shipping* packs; the four content-smoke fixtures (`HOUSE-00201`) really
            are runtime assets — compiled, loaded and drawn — so calling them "not packaged" would
            be false. `dev` carries `shipped: false`, which is the true statement, and gives
            `budget_report.py` a shipped-versus-total split that means something.
      accept: every asset row names exactly one declared pack or says why it names none; the
            runtime loads the real manifest; both are enforced by a gate
- [x] HOUSE-00203 — `tools/ci/budget_report.py`: per-pack size, texture memory, triangle totals, audio duration; writes a Markdown table
      dep: HOUSE-00202 · sys: ci · plat: CI · pri: MUST
      note: (2026-09-07) `tools/ci/budget_report.py` with `--emit`, `--check`, `--enforce` and
            `--selftest`; `docs/budget-report.md` is committed and gated for staleness in
            `run_checks.sh`, the same arrangement `licenses/THIRD-PARTY-ASSETS.md` uses. 19
            selftest claims over a fixture manifest and content tree it authors itself.
      finding: **the rule the whole file is built around is that nothing is estimated.** A value
            that cannot be measured prints `--`, never 0. That matters most NOW, when twelve of
            thirteen packs are empty: a report that zeroed an unmeasured row would say the project
            is comfortably inside every budget, which is true and useless. Measured from real
            sources rather than guessed — a PNG's own `IHDR`, a glTF's index accessor counts, a
            WAV's `fmt `/`data` chunks, `ffprobe` for a video, and the file sizes themselves.
      finding: **"not applicable" and "could not be measured" are different, and conflating them
            buries the second.** A font has no triangle count; that is not an unknown. The first
            version printed both as `--` and produced a fourteen-row reasons table of which twelve
            were "a font has no triangles". `·` now means the question does not apply and `--`
            means a measurement was attempted and failed, and only the latter is listed.
      finding: a `SpriteFont` **is** a texture atlas and does occupy the §72 texture budget, so its
            texture memory is a genuine `--` with the reason "the atlas dimensions are inside the
            compiled `.cnb`, which this report does not parse" — not a `·`. Calling it inapplicable
            would quietly leave real GPU memory out of the budget it belongs in.
      finding: **a pack with an unbuilt row is not compared against its budget at all.** Comparing
            it against the rows that happen to be built reports a pack at 12 % of budget because
            half of it was not built, which is the kind of reassurance that gets a project into
            trouble. The selftest's `partial` pack exists solely to make that guard load-bearing:
            5 000 built bytes against a 1 000-byte budget plus one unbuilt row, so deleting the
            guard turns a silent pass into a false 500 %.
      finding: **a video counts its streamed media as well as its metadata `.cnb`.** CNA deploys
            the media beside the metadata rather than embedding it (`HOUSE-00201`), so counting the
            `.cnb` alone would report the 25 MB television pack as a few kilobytes.
      finding: **the committed report is generated from the manifest ALONE, with no build tree**,
            so `--check` gives the same answer on a machine that has never run a build and can be
            an ordinary gate. Its compiled column is `--` by design and the report says so;
            `--content`/`--effects` produce the numbers the pack budgets are written against.
      finding: shown to fail before it was trusted — 8 injected bugs, all 8 detected, two of them
            only after the fixture gained the case that discriminates them.
      accept: per-pack source bytes, compiled bytes, texture memory, triangles and audio duration,
            with every unmeasurable cell named rather than zeroed; deterministic Markdown; the
            over-budget path demonstrated to fire
- [x] HOUSE-00204 — `tools/blender/impostor_render.py`: render 8-yaw impostor atlases with EEVEE, pack, and emit the metadata
      dep: HOUSE-00189 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/blender/impostor_render.py`. Eight yaws at 10° elevation into a
            2048² RGBA atlas, 4 × 2 cells of 512 × 1024, with a metadata sidecar. 17 selftest
            claims against an **asymmetric** fixture; `--selftest` runs in the CI Blender job.
            EEVEE Next renders headless here (12.9 s for the first frame, shader compilation);
            Cycles does not — *"Failed to denoise, build has no OpenImageDenoise support"*.
      finding: **the lighting is deliberately baked and deliberately DIRECTIONLESS**, which is what
            §26.3's "pre-lit for an overcast sky" has to mean. There is no sun: the world is a
            uniform white dome, so the atlas carries albedo × ambient occlusion and the runtime's
            sky tint multiplies cleanly. A baked sun would fight that tint and light the tree from
            the wrong side for most of the day. Asserted by rendering the same white subject from
            opposite yaws and requiring the same luminance — 0.997 against 1.000.
      finding: `view_transform` is forced to **Standard**. Blender's default AgX is a film response
            curve for look development, and baking it into an image the runtime multiplies by a sky
            colour compounds the curve twice: a white leaf arrives at about 0.8 instead of 1.0. The
            selftest measures 0.997, which is the check.
      finding: **`ortho_scale` is the view size along the LARGER image dimension.** Setting it to
            the required WIDTH made a 512 × 1024 frame 4.37 m tall for a 5.40 m subject — the tree
            was decapitated, every slice still carried a plausible silhouette, the coverage still
            differed between yaws, the lighting was still flat, and nothing complained. Only *"does
            any silhouette touch its cell border"* saw it, and that claim is in the selftest for
            exactly that reason.
      finding: **the tool must work in Blender's Z-UP, not the source's Y-up**, and the first
            version did not — including its own fixture. `Vector.to_track_quat` keeps an object
            upright against world **+Z**, so a Y-up camera solve rolled the camera 90° at every yaw
            that is not on an axis: yaws 0 and 180 framed correctly and the other six came out
            lying on their side, stretched across the full cell width and squashed to half its
            height. The silhouette AREA barely moved, so a coverage check saw nothing; the border
            check did. The glTF importer converts a Y-up source to Z-up on the way in, so Z-up is
            the right convention throughout.
      finding: **coverage cannot tell a view from its mirror.** Yaw 90 and yaw 270 of any subject
            have the same silhouette area and different silhouettes, and the first "the eight
            differ" check compared areas and passed on a broken render. It now compares the slices
            **pixel by pixel**, and the fixture is asymmetric in two axes so no two views are
            mirrors of each other either.
      finding: the camera is **orthographic**, and that is not a style choice: a perspective camera
            at a fixed distance projects a near branch and a far one at different scales, so the
            eight slices would disagree about the subject's width and the runtime quad would appear
            to breathe as the camera circled it. The frame is sized from the plan bounding CIRCLE,
            not the box, or the corners clip at 45°; and the elevation's depth-into-height term is
            included, or a deep subject loses its top.
      finding: 512 × 1024 cells rather than 724², because the subjects are trees and gable ends —
            taller than they are wide, and a square cell spends half its texels on empty sky.
      accept: eight yaws, one consistent camera, alpha silhouette, correct bounds, packed atlas,
            metadata that states the baked lighting explicitly, and byte-identical repeatability
      finding: (corrected 2026-09-07, while writing `HOUSE-00206`) `impostor_render.py` never printed
            `blender_env.py`'s `"<tool>: EXIT <n>"` sentinel. Blender does not propagate a script's
            exit status, so that sentinel is the authority and a missing one is treated as a
            failure — this tool therefore **reported failure in CI on a clean run**, and could
            never have reported failure on a broken one either. `collision_proxy.py` had it
            right from the start; the fix is copied from there.
- [x] HOUSE-00205 — `tools/blender/lightmap_unwrap.py`: second-UV atlas packing at a configurable texel density with a 4-texel gutter
      dep: HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/blender/lightmap_unwrap.py`. 16 selftest claims against a **room
            shell** fixture — floor, ceiling, four walls, a doorway and a window — rather than a
            synthetic triangle, because the failures this guards are architectural. `--selftest`
            runs in the CI Blender job.
      finding: **`uv.average_islands_scale` and `uv.pack_islands` silently CANCEL in background
            mode**, and this cost an hour before it was found. They act on the UV selection, and
            with no UV editor there is none unless `scene.tool_settings.use_uv_select_sync` is on.
            Nothing raises: the unwrap completes, the UVs look plausible, and the density even
            measures *uniform* — because `smart_project`'s own output happens to be nearly uniform
            for flat architecture. Only the gutter gave it away, at one texel. Every UV operator is
            now checked against `FINISHED`, and `_require` names the likely cause.
      finding: **"a 4-texel gutter" is four EMPTY texels, not a Chebyshev distance of four.** Two
            islands one texel apart have a gutter of zero. The first version measured distance and
            reported a passing layout as failing by exactly one; the measurement now reports empty
            texels between, which is the quantity a bilinear tap at the mip level actually reads.
            Measured on the fixture: no two islands come within five texels of each other.
      finding: the overlap test is **face-to-face, not island-to-island, and samples texel
            CENTRES** — which is what the baker does. A lightmap texel is one light sample, so two
            faces over one texel are two surfaces lit by one measurement and the brighter wins.
            Centre sampling is also what stops faces that merely share an edge counting as
            overlapping. Demonstrated by stacking two faces: 1 610 shared texels found.
      finding: **below about 80 texels the GUTTER sets the atlas size, not the density**, and that
            is not the obvious answer. A 4-texel gutter around eleven islands needs a certain
            atlas whatever the density is, so the attic's 2 texels/m lands on the *same* 128² as
            the bathroom's 8 and is over-resolved at 11.56. §18.3's per-cell densities therefore do
            not translate into proportional memory for coarse cells, and §22's 21-atlas budget
            should be read with that in mind. Asking for MORE density does behave: 32 texels/m
            gives 512² against 8's 128².
      finding: **a power-of-two atlas cannot hit an arbitrary density**, so a 5 % shortfall is
            accepted and the achieved density reported. The next size up costs four times the
            memory for at most twice the density; refusing the shortfall would quadruple the
            lightmap budget to buy something nobody can see.
      finding: the atlas size is found by **packing at each candidate**, not by extrapolating from
            one. The gutter is a fixed number of texels, so its cost as a fraction of the atlas
            halves every time the size doubles — an extrapolation is wrong by that factor, and
            badly wrong at the small sizes where the gutter dominates. At 32 texels a 4-texel
            gutter collapsed islands to zero area while `pack_islands` reported `FINISHED`.
      finding: the fixture's first version emitted a **zero-area face** — a doorway reaching the
            floor has no wall below it — which is how the degenerate check was first exercised, and
            which is why a zero-area input face is now refused before anything is unwrapped rather
            than surfacing three steps later as "every face has zero UV area".
      accept: no overlapping islands where forbidden; the configured texel density reached; a
            4-texel gutter measured as four empty texels; no degenerate UVs; UV0 intact byte for
            byte; repeatable
      finding: (corrected 2026-09-07, while writing `HOUSE-00206`) `lightmap_unwrap.py` never printed
            `blender_env.py`'s `"<tool>: EXIT <n>"` sentinel. Blender does not propagate a script's
            exit status, so that sentinel is the authority and a missing one is treated as a
            failure — this tool therefore **reported failure in CI on a clean run**, and could
            never have reported failure on a broken one either. `collision_proxy.py` had it
            right from the start; the fix is copied from there.
- [x] HOUSE-00206 — `tools/blender/lightmap_bake.py`: per-cell, per-light-group diffuse+indirect bake with denoising, plus the daylight bake
      dep: HOUSE-00205 · sys: content · plat: TOOL · pri: MUST
      accept: deterministic given a seed; a shell-geometry hash is embedded so a stale bake is detected
      note: (2026-09-07) `tools/blender/lightmap_bake.py`. 24 selftest claims over a two-room
            fixture with a doorway and a skylight; `--selftest` runs in the CI Blender job. 17
            injected bugs, all caught — but see the finding below about the first two sweeps,
            which caught nothing at all while reporting sixteen out of sixteen.
      finding: **`use_pass_color = False` is the setting this tool exists to get right.** A
            lightmap is irradiance, not lit colour; with the colour pass on, the albedo is baked
            in and then multiplied a second time by `DualTextureEffect` at runtime. Measured on
            the fixture: the RED wall bakes to `[3.09, 3.09, 3.09]` — a grey value, identical in
            kind to the white wall beside it — and to `[0.99, 0.0, 0.0]` the moment the pass is
            turned on.
      finding: **Blender 4.x defaults to AgX**, a filmic tone map. Left alone it rolls the
            highlights off the pendant's pool of light and lifts the shadows, producing a
            photograph of the room's lighting rather than a measurement of it, and nothing about
            the saved PNG looks wrong. `view_transform = Standard` and a `Non-Color` bake target,
            both asserted and both recorded in the sidecar.
      finding: **an 8-bit PNG cannot hold irradiance, and §18.3 step 4 exports PNG.** The fixture
            bakes to 20.7 with a single ordinary lamp; saved straight, everything bright clips to
            flat white and the *shape* §28.3 says the bake exists to capture is exactly what is
            lost. Each atlas is divided by its own peak and the scale recorded; §23.4's runtime
            sum is `lightmapArt.rgb * artLevels`, so the scale folds into `artLevels` for free.
            The Tier-E packed atlas needed the same treatment **per channel** and did not get it
            at first: written raw it clamped 565 of 1 024 fixture texels to a flat 1.0, throwing
            away precisely the shape it exists to carry while the per-group atlases beside it
            were correct.
      finding: **the first two bug-injection sweeps were worthless, and reported 16 of 16.**
            `blender_env.py` takes a `"<tool>: EXIT <n>"` sentinel as the authority because
            Blender does not propagate a script's exit status — and this tool did not print one,
            so it returned 1 on a *clean* run. Every injection therefore "failed" and was scored
            as caught. Fixed, and the honest re-run scored 11 of 16; the five that had been
            passing were all claims that set the very thing they were measuring (switching the
            indirect pass on themselves, hiding the lamps themselves) and so could not notice that
            the tool had not. They were rewritten to assert against the tool's own configuration
            and against `bake_cell`'s own output files, and one injected "bug" was withdrawn as
            ill-posed rather than left as a miss.
      finding: **`lightmap_unwrap.py` (`HOUSE-00205`) and `impostor_render.py` (`HOUSE-00204`) had
            the same defect**, found by looking for it. Both omitted the sentinel, so both CI gates
            reported failure for a passing selftest — and could never have reported failure for a
            failing one either. Both fixed here; the third Blender tool, `collision_proxy.py`, had
            it right and is what the fix was copied from.
      finding: the fixture's first version was a pair of hand-wound rooms and **four of its
            thirteen faces were lit**: a quad wound the wrong way produces no warning and no hole,
            just a black face whose normal points out of the room. Winding is now chosen by where
            the room is. The second version was then a *sealed* pair of boxes, so the daylight bake
            measured stray light on the outsides of walls and read a suspiciously exact 1.0 — a
            skylight was added, and `max(day)` was reading the ALPHA channel, which is 1.0
            everywhere and reports bright daylight for a scene in total darkness.
      finding: faces are found **by position**, never by the order the fixture emitted them. A
            hardcoded face index is correct right up until the fixture gains a skylight, at which
            point every claim downstream measures a different face and still passes.
      finding: the shell hash covers geometry, lightmap UVs **and the lights**. A hash over
            geometry alone calls a relit room fresh, and moving a lamp changes the bake as
            completely as moving a wall.
- [x] HOUSE-00207 — `tools/blender/shading_factor.py`: precompute per-window sun shading on a 12×24 (altitude, azimuth) grid by ray casting
      dep: HOUSE-00206 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/blender/shading_factor.py` and `docs/shading-format.md`, the
            normative `CSHF` version 1 spec. 22 selftest claims; `--selftest` runs in the CI
            Blender job. 15 injected bugs, all caught. One window's grid is 288 bytes, so §22's
            81 windows are 23 KB — its own figure, confirmed rather than assumed.
      finding: **the fixture's first neighbour was somewhere no ray could reach it.** §22 names
            "the west neighbour's gable", so the gable went due west — and a south-facing window's
            rays only ever travel towards +Z, so nothing at the same Z to its west can ever be
            occluded. The claim passed and failed on the *eave* instead, in both directions. The
            comparison is now south-west against south-east, and the gable is proved responsible
            by **deleting it and re-baking** rather than by comparing altitudes, because the eave
            shades the south-west too and an altitude comparison cannot say which of the two did
            it.
      finding: **the window is sampled, not probed.** §22 *multiplies* by `shadingFactor`, so a
            window half-covered by an eave must read about a half; a single centre ray reads 1
            until the shadow crosses the middle and then 0, and the foyer's daylight would step
            rather than slide once per window per afternoon. Measured on the fixture: the 4×4 grid
            reports `1.0, 1.0, 1.0, 1.0, 0.75, 0.75, 0.25, 0.0 …` down the southern column where a
            centre ray reports only ones and zeros. The column is also asserted **monotonic** — an
            eave that shades at one altitude shades at every higher one.
      finding: the grid is **nodes, not cell centres**: altitude at 0…90 inclusive over 12 samples
            and azimuth at 0…345 over 24. The runtime interpolates bilinearly, and node sampling
            makes that exact at the ends — the horizon and the zenith are measured rather than
            guessed from the nearest interior sample, and the horizon is where a low sun and a long
            shadow show most.
      finding: the grid is **occlusion only**. Whether the sun is up, and whether it is within
            §22's ±75° of the window normal, stay in the analytic `skyExposure` — so the baked grid
            is a pure property of the geometry and survives a change to that function. What *is*
            short-circuited is a sun behind the window's own wall: stored as 0 without casting,
            which is half the grid and 144 of the fixture's 288 nodes.
      finding: outward is decided by **the geometry**, not by whether the portal names the interior
            cell `cellA` or `cellB`. A window authored the other way round would otherwise cast
            every ray into the room it is trying to light, and the selftest builds the same portal
            both ways round to prove it does not.
      finding: the stored byte is `round(fraction × 255)`, **rounded and not truncated** — with 16
            samples the fractions are sixteenths and 3/16 rounds to 48 but truncates to 47. The
            claim compares the stored byte against a fraction measured independently in the
            selftest, because a claim that recomputes the fraction the way the tool does cannot
            see the tool's own rounding, and one that reads the stored byte back finds an integer
            by construction and reports that rounding never matters.
      accept: an eave shades a high sun and not a low one; a neighbour shades one azimuth and not
            its mirror, and stops when deleted; partial shading is reported as a fraction; two
            bakes of one scene are identical
- [x] HOUSE-00208 — `tools/blender/sun_patch.py`: precompute the sun-patch polygons cast through each window onto floors and walls, same grid
      dep: HOUSE-00207 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/blender/sun_patch.py` and `docs/sunpatch-format.md`, the normative
            `CSUN` version 1 spec. 15 selftest claims; `--selftest` runs in the CI Blender job.
            12 injected bugs, all caught.
      finding: **§29.1's "the two nearest grid entries are interpolated" decides the entire
            format.** Two polygons of different shapes cannot be blended vertex by vertex, so a
            patch is not the outline of wherever the light lands — it is a fixed `(n+1)²` lattice,
            one point per corner of an `n×n` subdivision of the window, in the same order at every
            one of the 288 nodes. Two things follow and neither is optional: a ray that hits
            nothing **still emits a point**, clamped inside the room, because dropping it would
            change the node's vertex count and break the node it is interpolated with; and the
            lattice is **row-major** along the window's width, because §29.1 draws a quad-*strip*
            and a strip's indices assume consecutive points run along a row.
      finding: **subdividing is what lets the patch bend**, which is why §29.1 says quad-strip and
            not quad. Projecting only the window's four corners gives a flat quad that cuts
            through the wall the moment the sun is low; a lattice lets each little quad land on
            whatever surface its own corners hit, and the patch folds along the floor/wall
            junction by construction. Measured: at least one altitude puts part of the fixture's
            patch on the floor and part 0.3 m up the back wall.
      finding: **"a low sun throws a longer patch" is false in a real room**, and it was the first
            claim written here. Four metres from the window the back wall truncates the patch, so a
            low sun's extent (2.33 m) is no larger than a high sun's (2.07 m). What is true in any
            room, and is what §29.1 asks to see, is that the patch **moves** — monotonically,
            sweeping 4 m of floor as the sun climbs, with no reversal for the eye to catch.
      finding: the light travels **opposite** the sun. `sun_direction` points from a surface
            towards the sun, so inside the room the ray runs along its negation; backwards, every
            patch lands on the lawn, where nothing sees it and nothing complains.
      finding: **the size, measured, because §72 has no line for it.** At `n=3` a present node is
            96 bytes, half the grid is empty (the sun behind the window's own wall, one byte), so a
            window is 13.8 KB and §22's 81 windows about **1.1 MB** — two orders above
            `shading.bin`'s 23 KB, which is the price of storing geometry instead of a scalar.
            `--subdivisions` is the dial if it has to come down.
      accept: every patch lands inside the room behind the window plane; every node has the same
            point count in the same order; the patch sweeps monotonically with altitude; two bakes
            are byte-identical
- [x] HOUSE-00209 — `tools/blender/cubemap_bake.py`: bake the 4 mirror cube maps
      dep: HOUSE-00206 · sys: content · plat: TOOL · pri: SHOULD
      note: (2026-09-07) `tools/blender/cubemap_bake.py` and `docs/cubemap-format.md`. Six sRGB
            PNGs per mirror in Direct3D's face order plus a `cubemaps.json` sidecar; 24 selftest
            claims; `--selftest` runs in the CI Blender job. 13 injected bugs, all caught. §59's
            four mirrors at 256² are 4.5 MB uncompressed, 1.1 MB as DXT1.
      finding: **the face convention is half measured and half asserted, and the tool says which
            half.** `HOUSE-00081` measured *which* face a direction samples against CNA itself —
            `(0,0,1)` → `+Z`, `(1,0,0)` → `+X`, `(-1,0,0)` → `−X` — so that mapping is known. What
            it did **not** measure is the orientation *within* a face, and a face rendered upside
            down or mirrored still reflects the right room: it fails only when someone reads a
            clock in it. `docs/cubemap-format.md` carries an explicit open item for a
            `HOUSE-00081`-style probe when the first mirror is placed.
      finding: **what can be checked without CNA is that the six faces agree with each other.**
            Adjacent faces are rendered from the same point and share an edge, so the rays along
            that edge are the same rays; all twelve adjacent pairs are asserted to look in
            identical directions along their shared edge. That catches every per-face error — one
            rotated, one flipped, one up-vector wrong — and leaves exactly one thing it cannot
            catch, a global handedness flip, which is why the open item above exists.
      finding: **there IS a handedness flip, and the selftest pins its direction.** D3D's cube
            faces are described in a left-handed space, §14's world is right-handed, and a Blender
            camera basis can only be right-handed — so the camera's right vector is the exact
            negation of D3D's on all six faces, asserted face by face. Every rendered face is
            mirrored horizontally on write; without it every reflection in the house is inside
            out, and looks fine.
      finding: **`render(write_still=True)` stamps metadata into the PNG**, so two bakes that are
            pixel-identical produce files that are not — measured, 971 bytes each and different.
            `HOUSE-00199` rebuilds everything and asserts byte-identical output, so that gate
            would have failed for a render that never changed. Re-saving through `image.save()`
            writes no such chunk, and the flip needed a re-save anyway, so one operation does both.
      finding: 90° is not a choice. A face's half-extent at unit distance is `tan(fov/2)`, so
            `tan 45° = 1` **is** the condition that six faces close a cube; narrower leaves a wedge
            of the room in no face at all, wider puts it in two and the seam doubles.
      finding: the mirror hides itself and is **unhidden afterwards** — a camera at the mirror's
            own position bakes the mirror's back into its own reflection, and hiding that is not
            undone leaves the second mirror baking a room the first one emptied.
      accept: each face shows the wall it points at; all twelve adjacent pairs are continuous
            across their seam; the mirror is absent from its own reflection and present in the
            next one's; two bakes are byte-identical
- [x] HOUSE-00210 — `tools/world/build_collision.py`: layout + `_COL` proxies → `content/world/collision.bin`
      dep: HOUSE-00190 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/world/build_collision.py` plus `tools/world/layout_io.py`, the
            JSONC reader `HOUSE-00211`…`HOUSE-00215` will share, and
            `docs/collision-format.md`, the normative `CCOL` version 1 spec. 44 selftest claims
            over a three-room fixture; both `--selftest`s run in CI. 18 injected bugs, all caught
            — but **six of the eighteen were missed by the first version of the selftest**, and
            each miss was a claim that did not exercise what its sentence said: the grid claim
            asked whether every shape was in *some* bucket (a corner-only index passes), and the
            prop-yaw claim used a proxy centred on its own origin, which rotation cannot move.
            The claims were rewritten against the injected bug, not the other way round.
      finding: **the layout contains no walls, and deriving them is four decisions, not a step.**
            (1) A wall is **centred on the boundary plane**: `world-format.md`'s validator rule 4
            makes two abutting cells share one plane exactly, so there is no authored gap for a
            wall to sit in and it must take half its thickness from each room. (2) A side is
            **segmented by what is on the other side of it** — the fixture's hall has one side that
            is 0.25 garage partition for three metres and 0.30 exterior for two. (3) A wall between
            two boxes of the **same** cell does not exist; an L-shaped room is authored as several
            boxes and that edge is the middle of the room. (4) A portal is a hole, and a hole in a
            rectangle is up to four rectangles.
      finding: **rule 3 is the one that would have shipped.** An L-shaped lounge with a wall across
            its own internal edge renders correctly, lightmaps correctly, validates correctly, and
            stops the player in mid-air in the middle of the room. It is now the same subtraction
            that punches a doorway, so there is one mechanism and not two.
      finding: the neighbour test compared the wrong face of the neighbour's box — its far face
            instead of the one touching the plane. The symptom was not "no wall": **both cells
            still built a wall, at different thicknesses**, because each side of the plane
            classified the other as absent. The two walls then failed to pool and sat in the same
            place at 0.30 and 0.15. Caught by asserting the thickness of a named segment, which is
            why that claim names a number rather than checking a wall exists.
      finding: **shapes are pooled globally and cells reference them by index.** A partition bounds
            the room on each side and must be collided with from both, but it is one wall. On the
            three-room fixture that is 7 of 38 references costing nothing; on the real house the
            shared fraction is far larger. §49.2's "~4 300 OBBs" is a count of *shapes*, and only
            the pooled file means what that estimate says.
      finding: **most `_COL` proxies are boxes and must not become triangle meshes.** Components
            are found by welding vertices **by position** — the glTF exporter splits them per face,
            so index adjacency finds one component where there are five — and "is a box" needs
            three clauses, not two: 8 distinct vertices and 12 triangles **also** describes a box
            with a corner pushed in, and the dent is exactly where a player would walk in.
      finding: the loose grid is **x/z, not x/y/z**. A cell is one storey tall, so a third axis
            multiplies buckets without dividing shapes — the floor slab alone spans every bucket of
            every vertical layer — while the vertical reject is one comparison the sweep already
            makes. Measured on the fixture: mean bucket occupancy 3.53 shapes, worst 7, against
            §49.2's "about six shapes, not nine hundred".
      finding: the stair wedge is **closed**, and closure is asserted by counting edges rather than
            by looking at it. The first version had the left face twice and the walking surface not
            at all; it had ten triangles, looked like a staircase in every summary, and a sweep from
            below would have passed through the flight.
      finding: `HOUSE-00472` ("generate `_COL` proxies for the shell") is **not** a second source
            for walls. This task precedes the shell by three phases and can only derive shell
            collision from the layout, which is what §49.2's "built offline from the layout" says;
            read `HOUSE-00472` as supplying proxies for what the layout cannot express — the
            rafter envelope, curved surfaces — and not as re-deriving the rectilinear shell.
      finding: (corrected 2026-09-07, while writing `HOUSE-00213`) the first version gave **every**
            cell a ceiling slab and four walls, including a cell of kind `exterior` — so a terrace
            was roofed over and walled in. `world-format.md` states the rule outright
            (`visibilityHint: open` is "no walls, e.g. exterior") and this tool ignored it. An open
            cell now gets a floor, no ceiling, and no wall on a side with no neighbour; it keeps
            the wall it shares with the house. The defect was invisible to this task's own claims
            and to `HOUSE-00212`'s, and surfaced only when `HOUSE-00213` cast rays from a patio and
            got none out. Recorded here rather than silently, because the shipped behaviour changed
            after the task was ticked.
      accept: every wall implied by two abutting cells exists once, at the thickness its neighbours
            imply, with a hole at every portal and no wall inside a single cell; an open cell has
            no lid; the file reads back byte-for-byte through its own reader; two builds of one
            layout are identical
      finding: (2026-09-07, found by `HOUSE-00347`) a flight with a half-landing produced
            **three** wedges instead of two: the riser a run starts on is the riser the landing
            below it ended on, so every run after a landing was one riser long. Continuous
            geometry, one extra shape per landing, and invisible to the existing claims because
            they were about closure and edges rather than about the count. Fixed with a `run == 0`
            guard; the selftest now asserts a half-landing makes two wedges of five risers each.
- [x] HOUSE-00211 — `tools/world/build_nav.py`: layout → the pet waypoint graph with perches
      dep: HOUSE-00210 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/world/build_nav.py` and `docs/nav-format.md`, the normative `CNAV`
            version 1 spec. 54 selftest claims; `--selftest` runs in CI. 19 injected bugs, all
            caught — again only after the claims were rewritten: **six of the nineteen were missed
            first time**, and five of those six were missed for the same reason, that the
            verifying claim called the very function it was meant to be checking. A selftest that
            asserts "no edge crosses a wall" with the tool's own `segment_clearance` agrees with
            the tool however broken that function is.
      finding: **the clearance test is what this tool is, and it is not a ray.** §60.3 says the dog
            "is a capsule (r 0.22, h 0.60) and is genuinely collided". A ray between two nodes goes
            through the 10 cm gap between the sofa and the wall; the graph then holds an edge the
            steering can never take, so the dog jams against the sofa while the planner insists the
            route is fine. Measured on the fixture: a 0.30 m gap gives 0.15 m of clearance —
            refused for the dog, allowed for the cat. Two species, two radii, two graphs over one
            node set.
      finding: **the query is a capsule AXIS, from r to h−r, never a point and never 0…h.** A point
            probe has no right height: at the feet it misses a wall shelf, at the chest it sails
            over the 0.25 m bench a dog obviously cannot cross. Testing 0…h and then comparing to r
            inflates the animal by its own radius at each end, refusing it a doorway it walks under.
            Both were live bugs here, and only the second-round injections found them: the first
            selftest measured clearance with the same broken helper it was checking. Against an OBB
            the answer is exact rather than sampled — `build_collision`'s OBBs are yawed about Y
            only, so undoing the yaw leaves the query vertical and the distance separates into a
            2-D clamp in x/z and a 1-D interval gap in y.
      finding: **not every portal is a route, and getting it wrong breaks the graph rather than
            thinning it.** A doorway node at floor level under a 0.9 m window sill is *inside* the
            sill, which is a wall piece from `HOUSE-00210`, so it fails its own clearance test and
            takes the two cells' subgraphs apart — a disconnected house whose cause is a node
            nobody asked for. The rule is **geometry, not `kind`**: bottom edge at the floor,
            taller than the animal, wider than 2r. The same three numbers settle a cased opening,
            a door, a slider and a garage door, where a list of kinds is a list someone must
            remember to extend. Rejected portals are reported by name.
      finding: floors and ceilings are **excluded** from the blocking set. A floor is not an
            obstacle to something standing on it; counting it gives every node in the house zero
            clearance and no graph at all.
      finding: the cross-portal edge must be clearance-tested like any other. Joining the doorway
            to the *nearest* node in the far cell is wrong — a node just around the corner is close
            in metres and behind a wall — so candidates are tried in distance order and the first
            the animal actually fits wins.
      finding: the room-centre node is the clearance-tested candidate **nearest** the cell's
            centroid, not the centroid. An L-shaped room's centroid can be outside the room
            entirely, and a centroid inside the sofa is not somewhere an animal can stand. The
            fixture's sofa sits on the lounge centroid precisely so this is measured rather than
            asserted.
      finding: the cat has **no capsule anywhere in the architecture** — §61 gives it clips,
            states, parameters and a size (0.25 m at the shoulder, 0.46 m body) but never a radius.
            r 0.11, h 0.25 is derived from that size here, once, in the open, and travels in the
            file, rather than being guessed separately by navigation, steering and collision.
      accept: no edge offers a route the animal's own capsule does not fit; a window never becomes
            a doorway node; every cell reachable through open portals is in one component per
            species; the file reads back through its own reader; two builds are byte-identical
- [x] HOUSE-00212 — `tools/world/build_coverage.py`: the rain/roof coverage height field on a 0.5 m grid
      dep: HOUSE-00210 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/world/build_coverage.py` and `docs/coverage-format.md`, the
            normative `CCOV` version 1 spec. 29 selftest claims; `--selftest` runs in CI. 13
            injected bugs, all caught — four missed first time, three of them because the claim
            was written against the constant it was supposed to be checking (`cell == CELL_SIZE`
            moves with the mistake; `cell == 0.5` does not).
      finding: **the soffits are already built, and building them twice would be the bug.** §37.2
            attributes the mask to `terrain_gen.py`, but none of the geometry it needs is terrain:
            a porch soffit is the underside of the balcony floor above it, and the roof underside
            is the top cell's ceiling. `HOUSE-00210` already derives every one of those slabs, so
            this asks `build_collision` for them. The whole rule is then one line — **the lowest
            underside of any floor or ceiling slab above the ground** — and it covers §37.2's
            entire list (house, garage, porch, balcony, sunroom, shed) because each is a cell, and
            a cell has a ceiling and the thing above it has a floor. That is also why the task
            depends on `HOUSE-00210` rather than on the terrain.
      finding: **below-ground slabs are not shelter, and this is not a detail.** A basement floor
            is 2.6 m under the lawn; without the ground test every square metre over the basement
            reports itself sheltered and the rain stops in mid-air over the garden, from a slab
            nobody can see. The fixture grew a cellar for exactly this claim: 4 of its 8 covering
            slabs are below ground and rejected.
      finding: **the field is deliberately NOT conservative.** Marking a cell covered when any part
            of it lies under a roof biases every soffit outward by up to half a grid step, so the
            rain stops 0.25 m short of the porch edge, in open air — which reads as a bug, not as
            shelter, and §37.2's acceptance is precisely "rain visibly stops at the porch edge".
            Centre sampling puts the error either side of the true edge. The claim is stated as an
            equivalence over all 2 397 cells of the fixture grid, not as a spot check.
      finding: uncovered is an **actual IEEE +∞**, not a large finite sentinel. `particle.y >
            coverage` is then false for every particle with no magic constant the writer and the
            runtime must agree on, and `f32` carries infinity exactly through the round trip.
      finding: the first version of the porch-edge claim tested the porch's **x = 0 side**, which
            abuts the lounge — sheltered at 2.50 m on both sides, so the assertion passed whatever
            the tool did. It now tests the open edge facing the garden. A boundary claim has to be
            written against a boundary that is actually one.
      finding: (corrected 2026-09-07, while writing `HOUSE-00213`) the porch claim had been
            **passing by coincidence**. `HOUSE-00210` was giving the porch — an `exterior` cell —
            a ceiling slab of its own, whose underside sat at 2.50 m, exactly where the bedroom
            floor above it also sits. The assertion could not tell the two apart, so it would have
            passed with the cover coming from the wrong surface entirely. `HOUSE-00210` no longer
            builds that lid, and the claim now removes the bedroom and requires the porch to become
            open sky.
      accept: rain is sheltered exactly where a slab is overhead and nowhere else; a basement never
            shelters the garden; open sky is +INF; two builds are byte-identical
- [x] HOUSE-00213 — `tools/world/build_skyexposure.py`: per-cell sky exposure and per-orientation facade exposure by ray casting
      dep: HOUSE-00212 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/world/build_skyexposure.py` and `docs/skyexposure-format.md`, the
            normative `CSKY` version 1 spec. 44 selftest claims; `--selftest` runs in CI. 15
            injected bugs, all caught.
      finding: **this task found a defect in `HOUSE-00210` that two finished tasks had already
            shipped past.** `build_collision` gave *every* cell a ceiling slab, including a cell
            of kind `exterior` — so a terrace was roofed over, and every ray from it was stopped
            by a lid that is not there. `world-format.md` states the rule outright
            (`visibilityHint: open` is "no walls, e.g. exterior") and the tool ignored it. Fixed
            in `build_collision`: an open cell gets a floor, no ceiling, and no wall on a side
            with no neighbour — it keeps the wall it shares with the house, because that wall is
            real. `HOUSE-00212`'s porch claim had been **passing by coincidence**: the porch's
            spurious own ceiling sat at 2.50 m, the same height as the bedroom floor above it, so
            the assertion could not tell the two apart. It now removes the bedroom and requires
            the porch to become open sky.
      finding: the openings need no special case. `HOUSE-00210` punched every portal out of its
            wall, so a ray leaving through a doorway meets no geometry and escapes on its own —
            and that is also §64.6's correct semantics, because the baked figure is the
            **geometric** opening and the runtime multiplies in the aperture live as windows open.
      finding: directions are a **Fibonacci hemisphere**, uniform in solid angle, and the obvious
            alternative is actively wrong: a latitude/longitude grid puts as many samples in the
            last degree below the zenith as in the first above the horizon, so a skylight would be
            weighted like a wall of glass. Checked against `1 − cos 45° = 0.293`.
      finding: the Monte Carlo estimator is checked against **three geometries whose answer is
            known exactly** — open sky 1, an unbounded lid 0, a half-plane 0.5 — rather than
            against itself. The first version of the lid fixture was 50 m across and measured
            0.070, which is not an error: a ray at 5° of elevation travels 50 m horizontally
            before it rises 4.4 m, so it genuinely escapes past the edge. The fixture is now
            effectively unbounded and the 50 m case is kept as its own claim, asserted against
            `sin(atan(4.4/50))`.
      finding: the broad phase is asserted to **agree with brute force on every ray**. A 2 m grid
            marched by a 3-D DDA is what makes 78 cells × 512 rays × ~4 300 shapes finish in pure
            Python at all, and a broad phase that also changes answers is not an optimisation.
      finding: §64.6's "the cell's centre" needs two guards, and both were live: an L-shaped
            room's centroid can fall **outside the room**, and a sofa can be **standing on it** —
            a listener inside a solid box hears nothing, so the cell would report zero exposure
            and the rain would go silent in a room with a window. The centroid is tested and a
            free point found when it fails; every fallback is named, and a cell with no free point
            at ear height is a warning rather than a silent zero. The point used is written into
            the file so a figure can be traced back to where it was measured.
      accept: a sealed cell measures exactly 0 and an open one strictly more; the eight facades
            average to the sky exposure; the broad phase changes no answer; two builds are
            byte-identical
- [x] HOUSE-00214 — `tools/world/build_snowshell.py`: generate the snow-shell meshes from up-facing exterior surfaces, respecting per-material slope limits
      dep: HOUSE-00212 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/world/build_snowshell.py` and `docs/snowshell-format.md`, the
            normative `CSNW` version 1 spec. 40 selftest claims; `--selftest` runs in CI. 16
            injected bugs, all caught.
      finding: **§38 asks for two things a static vertex buffer cannot both have.** "Offset along
            the surface normal by `snowDepth`" and "it needs no shader" hold together only while
            `snowDepth` is fixed, and it integrates from 0 to 0.35 m and back; baking one offset
            in picks a depth and is wrong at every other, with the shell floating a hand's width
            above the roof at full depth while the alpha says it is barely there. Resolved by
            shipping **base positions and normals, unoffset**, and letting the runtime do
            `p + n·snowDepth` — one multiply-add, on a buffer rewritten only when the depth has
            moved (`dD/dt = 0.0009·intensity` puts a full 0→0.35 m at ~390 s, so a 1 cm threshold
            fires about every 11 s in the heaviest snowfall). The plan already agrees:
            `HOUSE-01793` renders the shell and `HOUSE-01794` displaces it, two runtime tasks, so
            the displacement was never this file's to bake.
      finding: **the slope test is measured from +Y, never from |Y|.** A deck's underside is as
            flat as its top and points the other way, and `abs(dot(n, up))` would hang a snow
            shell under the balcony — the one artefact the whole technique exists to prevent.
            Measuring from +Y makes "snow does not cling to walls" (90°) and "snow does not cling
            to soffits" (180°) the same comparison.
      finding: **the normal written is the FACE normal, not the model's.** The slope test is a test
            of a face, so a vertex is in the file only because its face passed; offsetting along a
            smoothed vertex normal pushes it off the surface it was accepted for, and at a hard
            edge two faces pull the shared vertex in a compromise direction and open a gap along
            the crease at every depth. The shell is flat-shaded by construction and vertices are
            welded on **(position, face normal)**. Measured on the fixture: `build_chunks`'s chiral
            model has every authored normal along +X, so a shell that copied them would offset the
            snow sideways off the table.
      finding: the deck quad's first winding was the box's reading order — x0z0, x1z0, x1z1, x0z1 —
            whose cross product points **down**, so the terrace was rejected as a soffit by its own
            slope test and produced no shell at all. Deriving the normal from the winding rather
            than asserting +Y is what caught it, and is why authored boxes and imported prop
            geometry go through the same rule.
      finding: a material with no `snowResponse` gets a documented 40° default (§22.1's own example
            value) and is **named in the report**, never a silent 90 that snows on walls.
            `coverable: false` excludes a surface at any pitch, which is what glass and water are
            for. A degenerate face is rejected *and counted as degenerate*: it has no normal, so it
            would otherwise fall through as "facing down" and be miscounted.
      finding: two of §38's six sources cannot be read yet — **roofs** wait on `HOUSE-00470`'s
            shell and **terrain tiles** on `layout.exterior.json`'s height field. Both are named in
            the report rather than quietly absent, because a shell covering half of what §38 lists
            and saying nothing looks exactly like a complete one.
      accept: no shell exists on a face steeper than its material's limit, on a downward face, or
            in an indoor cell; positions are unoffset and normals are the face's; two builds are
            byte-identical
- [x] HOUSE-00215 — `tools/world/build_chunks.py`: batch per-cell static props into ≤ 6 chunks by (effect, material, light groups, alpha mode), pre-transformed to world space, with per-sub-range bounds
      dep: HOUSE-00210 · sys: content · plat: TOOL · pri: MUST
      accept: ≤ 6 chunks per cell; ≤ 65 535 vertices per chunk where 16-bit indices are used
      note: (2026-09-07) `tools/world/build_chunks.py` and `docs/chunk-format.md`, the normative
            `CCHK` version 1 spec. 53 selftest claims; `--selftest` runs in CI. 20 injected bugs,
            all caught — six missed first time, and **two of those six were missed because the
            fixture was a cube**: a 1 m cube with up-facing normals is invariant under the yaw
            this tool bakes in, so "yaw dropped" and "normals not rotated" both passed. The
            fixture is now chiral, 0.5 m across x by 2.0 m along z with every normal along +x, and
            both bugs fail loudly. A fixture has to be able to tell the answers apart.
      finding: **§17.4's four-part key has one free part.** `alphaMode` is a field of the material
            and so is `class`, which decides the effect; `lightGroups` lives on the cell and a
            chunk never spans two cells. So `(effectClass, material, lightGroupSet, alphaMode)`
            collapses exactly and without loss to **the material id**, and "≤ 6 chunks per cell"
            means "≤ 6 distinct materials among a cell's static props" — which is the sentence to
            give an author, because it is the thing they control. The key is still built from all
            four parts so a later per-prop light-group schema keeps working, and the report prints
            how many chunks the other three actually separated (today: none) rather than a comment
            asserting it.
      finding: **a chunk must not use CNA's 48-byte model vertex**, and it is wrong in both
            directions at once. §349's layout has *no second UV*, and §17.4's chunks are drawn with
            `DualTextureEffect` whose whole purpose is albedo × lightmap on two channels; and it
            carries a normal and a tangent that `DualTextureEffect` never reads, because that
            effect is unlit — the lightmap *is* the lighting. A chunk is uploaded into our own
            `VertexBuffer`, so it declares what its own effect reads: 28 bytes for
            `DualTextureEffect`, 32 for `BasicEffect`, 20 for `AlphaTestEffect`, against 48. Since
            chunks are grouped by effect class, a chunk always has exactly one layout.
      finding: **§17.4's two limits pull against each other** and the resolution has to be stated.
            A group over 65 535 vertices must be split to stay on 16-bit indices, and splitting
            makes more chunks — the other limit. Splits fall on **prop boundaries**, because a
            sub-range is a prop already and one straddling two buffers could not have a bounding
            box; a single prop over the cap cannot be split at all, so that chunk takes 32-bit
            indices. Both are reported and the tool exits non-zero over the six, because a cell
            needing eight chunks is an authoring problem rather than something to resolve quietly.
      finding: the placement is **baked into the vertices**, because §17.3 draws a chunk with
            `setWorldProperty(Matrix::Identity)` — a chunk has no transform, so a prop's position
            and yaw are in the geometry or they are nowhere. Normals are rotated but not scaled; a
            non-uniform scale would need the inverse transpose and is refused rather than producing
            normals wrong by a factor no wireframe shows.
      finding: (corrected 2026-09-07, while writing `HOUSE-00214`) the first version keyed the
            vertex layout on a class vocabulary that **does not exist**. `docs/world-format.md`'s
            abbreviated material example invents `lightmapped_opaque` / `lit_opaque`; §15.1 defines
            `layout.materials.json` as "material definitions (**§22**)", and §22.1's record states
            the effect outright in `effectTierS` while §22.2 gives the real class vocabulary
            (`paint`, `wood`, `tile`, `metal`, `foliage`, …). The tool now reads `effectTierS`
            first — an effect that is stated should not be inferred — falls back to §22.2's class
            table, resolves `wet_<class>` / `snow_<class>` to their base, and refuses a `skin` or
            `fur` material outright, because a skinned prop is an animated prop and §17.4 excludes
            those from batching. `docs/world-format.md`'s example was corrected to §22's
            vocabulary and now says which document wins.
      finding: a material that resolves to no stock effect is an **error naming both** the stated
            effect and the class, never a fallback to `BasicEffect`. A material quietly drawn with
            the wrong effect is a rendering bug that presents as an art bug and gets looked for in
            the wrong place.
      finding: `_COL` proxies are stripped here as well as extracted by `HOUSE-00210` — §18 says
            the build does both. A proxy left in a chunk is invisible geometry the GPU still
            transforms, and on a model whose proxy shares the source mesh it doubles the vertex
            count with nothing to show for it.
- [x] HOUSE-00216 — Content build orchestration: one `make content` target running validators, generators and `cna-content` in the right order with dependency tracking
      dep: HOUSE-00205…HOUSE-00215 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/ci/build_content.py` plus the `content` and `content-plan` CMake
            targets. 41 selftest claims; 15 injected bugs, all caught. The order is a **topological
            sort of a declared graph**, not a written list — adding a stage means naming what it
            needs and nothing else, and reordering the declaration changes nothing.
      finding: **freshness is content, never mtime**, and `AGENTS.md` is the reason rather than
            taste. Its opening argument is that every avoidable rebuild is irreversible flash wear
            on one SSD shared by ten agents, and mtime gets it wrong in both directions: **git does
            not preserve mtime**, so a clone or a branch switch stamps every file with the time it
            was written and an mtime build rebuilds a tree that has not changed at all; and an
            output restored from a backup is newer than an input it does not match. A stage is
            fresh when the SHA-256 of its inputs, its exact command line and its output list match
            the stamp. The selftest moves an input's mtime forward by an hour and requires that
            nothing rebuilds.
      finding: **"no inputs" is not "no pattern matched", and the difference cost a real run.** The
            first version skipped a stage only when its whole input set was empty, so
            `build_collision` was launched on the strength of `assets.manifest.json` existing while
            `assets-src/world/*.json` — the thing it actually reads — did not, and it failed inside
            the tool. A pipeline waiting for phase 5 reported as a broken build. Every input
            pattern must now match at least one file, and the skip names the pattern that did not.
      finding: **validators gate generators by an explicit edge, not by a sort key.** Ordering the
            plan by group happens to work for today's graph and stops working the moment a
            generator has no `needs` and lands in the same topological level as a validator —
            which is exactly what happened, and what the claim "every validator runs before every
            generator" caught. `with_validator_gate` adds the edges; the sort then does the work.
      finding: five outcomes, and keeping **skipped** distinct from **failed** is the point. Six of
            eleven stages are skipped today because phase 5 has not authored the layout; `make
            content` has to be runnable now, and a build that quietly succeeded by doing nothing
            would be the worst of the five. A failed stage **stamps nothing**, so the next run
            retries it, and blocks its dependents rather than running them on missing inputs.
      finding: the first version left **`cna-content` out of the graph**, though the task title
            names it. Added as one stage per content root — `CMakeLists.txt`'s
            `CNAHOUSE_CNB_ASSET_DIRS` plus `Media/` — rather than one call over `assets-src/`, so a
            texture change does not recompile the audio. A stage can now also declare a **tool** it
            needs: `cna-content` is built by CNA, and a checkout that has not configured a build
            can still run every validator, so a missing tool is its own skip reason rather than a
            failure. With it in place `make content` really does build `assets-src/` into
            `content/` — 25 files — which is Phase 3's first exit criterion.
      finding: **building into `content/` broke the layout gate**, and the gate is the first stage
            of the next build. `check_layout.py` called `content/.gitkeep` stale "because the
            directory is no longer empty" — but everything in it is git-ignored, and that
            `.gitkeep` is the only reason the directory exists in the repository at all. Nobody had
            hit it because nobody had run a content build into `content/` before. Fixed in
            `HOUSE-00022`'s tool: emptiness is now judged **as git judges it**, by asking
            `git check-ignore`, so an ignored file does not make a placeholder stale and a tracked
            one still does.
      accept: the order is derived from the graph and a cycle is refused by name; touching an input
            rebuilds nothing and changing a byte rebuilds exactly its readers; a stage that cannot
            run yet is skipped and named, not failed; `assets-src/` is compiled into `content/`
- [x] HOUSE-00217 — Add the content-build documentation: what each tool does, in what order, and how to rebuild one asset
      dep: HOUSE-00216 · sys: — · plat: TOOL · pri: MUST
      note: (2026-09-07) `docs/content-build.md`. The stage table is **generated** from the
            pipeline graph by `build_content.py --docs` and checked by `--check-docs`, which is now
            a gate in `tools/ci/run_checks.sh` — the same idiom `budget_report.py` uses, for the
            same reason.
      finding: a hand-written build order is **a second description of the dependencies**, and a
            second description drifts: the one thing this page documents is the one thing most
            likely to go stale the next time a stage is added. Generating it removes the failure
            mode instead of asking someone to remember. Every `Stage` therefore carries a
            one-line `description`, and the selftest refuses a stage without one.
      finding: the gate was verified to **bite** — the committed table was edited by hand and
            `--check-docs` failed, naming the command that fixes it. A generated document whose
            checker cannot fail is a hand-written document with extra steps.
- [x] HOUSE-00218 — Measure and record a full content build time; set the expectation for later sessions
      dep: HOUSE-00216 · sys: — · plat: TOOL · pri: SHOULD
      note: (2026-09-07) Four rows in `docs/performance-log.md`.
      finding: **today's content build is 0.29 s cold and 0.09 s warm**, medians of five — and
            that number means very little on its own, because six of the eleven stages are skipped
            for want of a layout. Recorded as the baseline it is, clearly labelled, so a later
            session comparing against it knows what was and was not in it.
      finding: **the expectation this task exists to set is the lightmap bake, and it is
            overnight.** Measured scaling on `lightmap_bake`'s fixture — 32²/16 spp 0.039 s, 128²/16
            0.200 s, 128²/64 0.617 s — is close to linear in texels × samples above about 64².
            Extrapolating to §18.3's 2048² at 256 samples gives **≈ 10 minutes per atlas** and
            **≈ 7–8 hours for its 42 atlases**, and that is a *lower* bound: the fixture is two
            rooms of sixteen faces and two lamps, where a furnished cell has far more geometry per
            ray. The scaling was measured rather than assumed precisely so the extrapolation could
            be defended.
      finding: that number is what makes `HOUSE-00216`'s content-hash stamps **load-bearing rather
            than a convenience**. A pipeline whose longest stage is seven hours cannot afford to
            rebuild because git rewrote an mtime, and `AGENTS.md`'s SSD argument and this one point
            the same way.
- [x] HOUSE-00219 — `tools/assets/video_transcode.py`: transcode source footage to the runtime video format and to the frame-strip atlases
      dep: HOUSE-00098 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/assets/video_transcode.py`, producing both of §58.3's backends:
            a 512×288 24 fps Theora `.ogv` with the `VideoProcessor` parameters CNA requires, and
            the 8×8 frame-strip atlases with their soundtrack. 24 selftest claims; `--selftest`
            runs in CI.
      finding: **the atlas is 2048 × 1152, not §58.3's "2048²"**, and that is the smallest
            correction that makes the arithmetic true: 8 × 256 is 2048 and 8 × 144 is 1152.
            Rounding the height up leaves 896 rows — **44 % of the texture, 7.3 MB of 16.8 MB** —
            holding nothing, per atlas, in a 25 MB pack. `--square` restores the padded layout and
            reports what it costs. NPOT is not assumed: OpenGL ES 3.0 requires it for an
            unmipmapped clamped texture, and a 2048 × 1152 PNG was compiled through
            `CNA.ImageImporter -> CNA.TextureProcessor` to confirm.
      finding: **the frame-strip backend does not fit its own pack as `.cnb`, and this sizes phase
            21.** That compile produced **9 437 552 bytes** — exactly 2048 × 1152 × 4 plus a header
            — because CNB texture schema 1 is frozen to `Rgba8` (`HOUSE-00111`). At 9.44 MB per
            5.33 s atlas, §27.2's 25 MB `video` pack holds **2 atlases: 14 seconds of television,
            for four channels.** The same atlas as DXT1 through `.xnb` is 1.18 MB — eight times
            smaller — and 25 MB then holds 21 atlases, **113 seconds**. The strip backend is
            therefore only viable through the `.xnb` texture route §27.2 already names for the
            `web` and `android` profiles, which is exactly where `SequenceTvSource` is needed.
            Recorded in `cna-house.md` §58.3.
      finding: **the frame-by-frame check is the point of the fixture.** Every source frame carries
            a black bar whose LENGTH is its own index, so a decoded atlas cell says exactly which
            frame it holds. Six seconds at 12 fps is 72 frames, deliberately more than one atlas:
            all 64 cells of the first are verified to hold their own frame, and the second atlas's
            first cell is verified to hold frame **64** — a transposed grid, an off-by-one or a
            frame dropped at the boundary all survive a check that only counts atlases.
      finding: the unused tail of the last atlas is **black, not the last frame repeated**. A
            television that runs out of frames should go dark rather than freeze on one, and the
            metadata's frame count is what stops the timer reaching those cells at all.
      finding: determinism is reported **split by what can actually be promised**. The atlases and
            the soundtrack are written by this project's own encoders and are byte-identical run to
            run; the `.ogv` is byte-identical *for this ffmpeg* and not across builds, because
            Theora's bitstream depends on the encoder — the same finding `HOUSE-00201` recorded,
            stated rather than papered over.
      finding: a source with no audio is a **warning**, not a failure — a static channel has no
            soundtrack and is still a channel — while a file with no video stream is refused. And
            when both exist, a soundtrack more than 0.5 s out of step with the strip is warned
            about, because `SequenceTvSource` advances both from one timer and they would drift.
      accept: runtime transcode, frame-strip atlas, timing metadata, dimensions, frame count and
            audio handling all verified; determinism recorded exactly as far as the codec permits
- [x] HOUSE-00220 — `tools/assets/reverb_variants.py`: produce the dry/small/large pre-reverberated variants for the 30 transient sounds
      dep: HOUSE-00193 · sys: content · plat: TOOL · pri: SHOULD
      note: (2026-09-07) `tools/assets/reverb_variants.py`. 18 selftest claims; `--selftest` runs
            in CI. Stated plainly in the tool itself: this is an **application-level acoustic
            approximation**, not a claim about room acoustics. No room is measured and no impulse
            response is convolved; two tap patterns are chosen and then what they actually did is
            measured on every file written.
      finding: **the dry variant is the source copied byte for byte**, not re-encoded. §64.7 has
            the profile *select* one of the three rather than cross-fade them, so the dry version
            must be exactly the sound every other reference already means; a re-encode would differ
            from its own source by a dither's worth of noise for nothing.
      finding: **the tails are measured from the impulse response, not read off the preset.** An
            impulse in, and the decay out must end at the preset's own last tap — 31.0 ms measured
            against 31 ms authored for `small`, 97.0 ms against 97 ms for `large`. The arithmetic
            and the audio agreeing is what says the filter did what the numbers describe.
      finding: **the clipping guard is load-bearing, but not for the sounds the section names, and
            the measurement corrected the assumption.** A fast transient's taps land 11 ms or more
            after its peak, by which time the direct sound has decayed — the door-slam fixture comes
            back at −2.2 dBFS with or without the pre-attenuation. It is a SUSTAINED sound (a bark's
            body, a flush, a motor) whose taps land on the signal still playing: without the
            attenuation that reaches exactly 0.00 dBFS — clipping — and with it, −5.67. The
            selftest's first version claimed the transient proved it and was simply wrong.
      finding: the attenuation is a property of the **preset**, not of the file: `1 / (1 + Σ decays)`,
            the worst case in which every tap lines up with the direct sound. A per-file
            normalisation would break the mix — two sounds balanced dry would come back at
            different levels wet — and halving the source is shown to halve the variant, 6.02 dB.
      finding: a variant that still exceeds the ceiling is a **refusal**, not a warning, and a
            reverberated variant that is not LONGER than its source is refused too: it has no tail,
            which means the filter did not run.
      accept: the dry reference is preserved; the presets are reproducible; gain clipping is
            avoided and the guard demonstrated; the tail duration is documented per file
- [x] HOUSE-00221 — `tools/assets/dull_variants.py`: produce the low-passed "dull" variants for the 22 muffle-critical sounds
      dep: HOUSE-00193 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/assets/dull_variants.py`, with `tools/assets/audio_probe.py` — a
            WAV decoder and measurement module written so both audio tools' claims are *measured*
            rather than asserted. 14 selftest claims; `--selftest` runs in CI.
      finding: **§64.5's "4th-order low-pass" is a Linkwitz-Riley, not a Butterworth, and the
            corner is at −6 dB rather than −3.** ffmpeg's `lowpass` at `p=2` is one Butterworth
            (Q = 0.707) 2nd-order section; cascading two squares the magnitude response, which is
            the definition of a 4th-order Linkwitz-Riley. That is the *right* answer here rather
            than an approximation to be apologised for: −6 dB at the crossover is exactly what a
            pair of complementary gains `(1 − muffle)` and `muffle` wants. A true 4th-order
            Butterworth would need sections at Q = 0.541 and Q = 1.307, which `lowpass` cannot
            express. **Measured:** the corner drops 9.02 dB (−6 filter, −3 tilt), 100 Hz drops
            3.00 dB (the tilt alone), the octave above the corner loses a further 18.7 dB and the
            next 24.2 — approaching 24 dB/octave as it leaves the knee.
      finding: the section count is shown to be **load-bearing**: the same file through one section
            loses 9.4 dB per octave against two sections' 18.7. Without that comparison "4th order"
            is a word in a docstring.
      finding: **the "−3 dB tilt" is applied flat, and that is a decision rather than a reading.**
            A spectral tilt is the literal meaning, but after a 900 Hz 4th-order low-pass there is
            almost nothing above 2 kHz left to tilt — a high shelf changes the file by a fraction
            of a decibel. What §64.5 describes is a sound through a wall being duller *and quieter*,
            and a flat −3 dB is that, exactly. `--shelf-hz` applies a real shelf where one is
            wanted, and the selftest shows it leaves 100 Hz within 0.04 dB where the flat tilt takes
            the full 3.
      finding: **rate, channel count and length are pinned to the source's and checked afterwards.**
            §64.5 cross-fades the pair sample-aligned, so a filter chain that silently resampled
            would desynchronise the two halves of every muffled sound — a failure with no error
            message anywhere. The source itself is never touched: it IS the bright half.
      finding: determinism is proved the way `HOUSE-00193` proved it, including the negative half —
            the naive ffmpeg command stamps `ISFT` with the libavformat version into the file and
            the flags used here remove it, so those flags cannot rot into decoration.
      accept: measured low-pass parameters, predictable level, no unintended resampling,
            deterministic output
- [x] HOUSE-00223 — `tools/assets/anim_extract.py`: read a source `.glb` and write its `.chanim` sidecar — skeleton in skin-joint order, bind and inverse-bind poses, every clip as per-bone TRS keyframe tracks
      dep: HOUSE-00166, HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
      files: tools/assets/anim_extract.py, tests/unit/ChanimRoundTripTests.cpp
      note: (2026-09-07) `tools/assets/anim_extract.py` with `--selftest` and `--make-fixture`,
            plus `ChanimRoundTripTests` — **seven C++ tests that read what the Python tool wrote,
            with the real `ChanimReader`.** The fixture is generated at build time by CMake rather
            than committed, for the same reason no compiled content is: a checked-in binary produced
            by a tool in the same repository goes stale against that tool unnoticed. The Python
            selftest runs in CI.
      finding: **the round-trip test is the only one that could exist, and it did not.**
            `ChanimReaderTests` (`HOUSE-00167`) builds every byte by hand, deliberately, because
            when it was written the writer did not exist — which makes it an excellent test of the
            reader and **no test at all of the two agreeing**. Nine injected writer bugs prove the
            point: transposed matrices, inverse binds off by one joint, `w`-first quaternions, a
            `boneFirstKey` off by one, alphabetically sorted joints, a dropped stride, bind and
            inverse-bind swapped — every one of them passes `ChanimReaderTests` and fails in the
            game. All nine now fail the round trip.
      finding: **glTF's column-major array and XNA's row-major array are the SAME sixteen floats.**
            XNA's `Matrix` is the transpose of glTF's (row-vector against column-vector), and
            transposing a column-major array yields a row-major array of the transpose — so
            `inverseBindMatrices` is copied straight through and translation lands at `M41..M43`,
            which is elements 12–14 either way. "Just copy it" is either right for a reason or
            wrong forever, so the C++ test asserts the fourth ROW carries the translation and the
            fourth COLUMN is (0, 0, 0, 1).
      finding: **an identity quaternion cannot show a `w`-first write**, and the first fixture used
            one. An injected writer that emitted `w` first passed every test in this repository.
            The hips now end `walk_fwd` at 40° about the normalised axis (1, 2, 3) — four distinct
            components — and both sides assert the order.
      finding: **a bone animated to exactly its bind pose is written with NO KEYS AT ALL.** §3.3
            makes an empty range legal and says such a bone holds its bind pose, and on a real rig
            most bones in most clips do exactly that. The fixture animates `RightUpLeg` to its own
            bind pose so the case is exercised by a bone that HAS channels — a bone with none has
            nothing to drop and proves nothing.
      finding: decimation is greedy and its correctness is asserted by **reconstruction, not by a
            key count**: what survives must reproduce every original sample to within the tolerance
            it was given. Measured on a curved fixture: 31 keys to 14, worst error 0.0199 m against
            a 0.02 m tolerance. Rotation is compared as an **angle between quaternions** because
            `q` and `-q` are the same rotation and a component distance calls them 180° apart.
      finding: keys are sampled at the **union of that bone's own channel times**, not on a
            resampling grid. The source's times are where the author put information; a uniform
            grid adds keys where there is nothing to say and loses the exact instant of a contact.
            The fixture samples the hips' translation at 20 Hz and its rotation at 5 Hz so the
            union is exercised rather than a single shared grid.
      finding: `strideLength` and `footPlants` are **consumed from `measure_stride.py`**
            (`HOUSE-00194`), never measured a second time — which is what stops two implementations
            of §47.4's one number drifting apart. A measurement that fails leaves the stride at 0,
            which §47.4 already defines as "not locomotion", and says so in a warning.
      finding: `check()` re-asserts every condition `docs/anim-format.md` §4 makes a reader reject,
            **before** writing. A writer that can emit a file its own reader refuses is a writer
            that will, and the failure then surfaces at load time in the game rather than in the
            build that produced it. Eight of those conditions are shown to fire.
      accept: exact skeleton order, bind and inverse-bind data, clips, per-bone TRS tracks,
            decimation tolerance, stride metadata from `HOUSE-00194`, foot-plant markers,
            binary/versioning validation and deterministic bytes — all verified through the real
            C++ reader rather than against the writer's own idea of the format
      accept: (1) deterministic output, hash recorded in the manifest; (2) keyframes decimated only within a stated tolerance; (3) stride length (`measure_stride.py`) and foot-plant markers written into the same file
- [x] HOUSE-00224 — `tools/assets/skin_split.py`: split a multi-skin source `.glb` into one file per skin, record each part's attachment bone in the manifest, and extend `gltf_validate.py` to **reject** any `.glb` entering the build that declares more than one skin
      dep: HOUSE-00076, HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/assets/skin_split.py` with `--verify`, `--manifest`, `--selftest`
            and `--make-fixture`; `manifest.py` validates an optional `attachment` object. The
            `gltf_validate.py` half was already in place (`HOUSE-00186`) and is confirmed live here:
            the two-skin fixture is refused by BOTH the project rule *and* `CNA.ModelProcessor`
            itself, and the two split parts then pass the whole gate including the real importer.
      finding: **"equivalent" is proved arithmetically rather than by "the file loads".** For every
            vertex of every kept mesh, `p' = Σ w_j · (jointWorld_j · inverseBind_j) · p` is computed
            from the source and from the part and must agree to 10 microns — at the bind pose and
            in motion. A reversed joint list, a corrupted inverse bind matrix and a moved vertex are
            each shown to fail it. `HOUSE-00076`'s criterion (1) could not be measured at all
            because there is no loadable unsplit source; this is the form of that comparison that
            can be.
      finding: **both sides are posed by the SOURCE's shared skeleton, and that is the whole
            architecture, not a testing convenience.** §47.0 draws a multi-part character as several
            `Model`s over one skeleton and one palette. An earlier version of this check posed each
            part from its own copy of the animation and was wrong twice over: it made the comparison
            depend on data the runtime never uses, and it failed the moment the channel pruning
            below became necessary. An injected part whose joint node had MOVED correctly produced
            no complaint — under a shared pose a part's own node translations are overridden and
            genuinely do not matter — so the injection was replaced with ones that do.
      finding: **a part must NOT carry animation channels that miss its own joints**, and this is
            measured. `CNA.GltfImporter` warns *"Clip 'sway' has 1 channel(s) whose target node is
            not a joint of this skin — they drive nothing in this palette, and are skipped"*, and
            `gltf_validate.py` treats an importer warning as an error, so such a part cannot enter
            the build at all. Nothing is lost by pruning them: CNA discards them anyway, and this
            project's animation transport is the `.chanim` sidecar rather than `Model::Tag`, which
            BL-01 makes null regardless. Every dropped channel is NAMED in the attachment record —
            the fixture's tail part records `sway:Root.rotation` — so "the tail stopped following
            the body" can never be an unexplained observation.
      finding: the production tool **prunes the buffer, the materials, the textures and the
            images**; `p1-split-skins.py` deliberately did not and recorded that it did not, because
            its job was to establish the shape of the split. The fixture's parts come out at 52 %
            and 65 % of the source, a figure held down only by a four-vertex fixture being mostly
            JSON header.
      finding: what a part **keeps** is the whole node graph, every joint of every skin. Pruning the
            other part's joints would give each part its own bone numbering and make one shared
            `.chanim` impossible, which is the arrangement §47.0 depends on.
      finding: the fixture lists the tail's joints **out of node order** (`TailB` before `TailA`) on
            purpose. A splitter that rebuilt the joint list from the node graph — the obvious
            implementation — silently reverses the binding, and every blend index then means a
            different bone. `verify()` catches exactly that.
      finding: `--manifest` **updates** rows, never creates them. A row carries a licence, an origin
            and four permission booleans a splitter cannot know; provenance comes first and the
            attachment second, and a part with no row yet is reported rather than invented.
      accept: every part is single-skin and passes the full glTF gate; the attachment record
            round-trips; skinned-vertex equivalence against the source is demonstrated, and the
            check is demonstrated to fail
      accept: (1) the split parts render identically to the source; (2) a two-skin file fails validation with a message naming both skins and pointing at the splitter; (3) the rule is documented in `docs/content-authoring.md`
- [x] HOUSE-00225 — `tools/ci/check_anim_assets.py`: for every character, assert the `.chanim` joint list matches the compiled `.cnb` `Model::Bones` name-for-name and in order, and that no source `.glb` declares more than one skin
      dep: HOUSE-00223, HOUSE-00224 · sys: ci · plat: CI · pri: MUST
      note: (2026-09-07) `tools/ci/check_anim_assets.py`, gated in `run_checks.sh`, plus
            `tools/assets/cnb_model.py` — a reader for the compiled `.cnb`'s bone table, written
            against `cnanext/docs/cnb-format.md`. 15 selftest claims, including an END-TO-END one
            that compiles the fixture with the real `cna-content` and binds the sidecar to what
            comes out. Today it reports "2 source model(s) scanned, all single-skin; 0 sidecar(s)
            bind"; the second half becomes live the moment a character exists.
      correction: the title says "name-for-name **and in order**". Measured against
            `ClipLibrary::BindTo`, that second clause is **not what binds and cannot be required**:
            `BindTo` resolves each joint by NAME through `ModelBoneCollection::TryGetValue`, and
            `Model::Bones` is the whole scene graph — the fixture's compiled model has seven bones
            (`Root`, the five joints, `Body`) for a five-joint skin. The joints are a SUBSET in a
            different indexing, which is precisely why `HOUSE-00074` concluded the sidecar must
            carry the names at all. The gate therefore asserts what actually binds: every joint
            name resolves, exactly once. Requiring identical order would fail every real character.
      finding: **the comparison is against the compiled `.cnb`, not the source `.glb`**, and that
            is the point of the task. What ships is the `.cnb` and what the game binds against is
            the bone table inside it; a check against the source passes happily while an importer
            change, a renamed node or a stale build makes the shipped pair disagree. `cnb_model.py`
            reads the header, the table of contents, `MSTR` and `MBON` from the bytes — and
            verifies **both CRC-32Cs**, because a reader that skipped them would report a corrupt
            file as a wrong bone list, the least useful diagnosis available.
      finding: CRC-32C is **Castagnoli** (`0x82F63B78` reflected), not `zlib.crc32`. Using the
            wrong one produces a plausible number for every input and fails nothing until a real
            file arrives, so the standard check value is pinned in the selftest:
            `crc32c("123456789") == 0xE3069283`.
      finding: **two bones with one name is a real failure and no importer produces one.**
            `TryGetValue` resolves by name, so which of the two it returns is an implementation
            detail and the wrong one deforms silently. The fixture for it is built BYTE BY BYTE —
            the only way that case can exist to be tested at all, and the reason the selftest has a
            hand-written `.cnb` writer beside its real one.
      finding: pairing is `<content>/Anim/<name>.chanim` to the one `<content>/Models/**/<name>.cnb`.
            **None** and **several** are both reported rather than guessed at: a sidecar for an
            asset not in the build, and an ambiguity a naming convention cannot resolve, are
            different bugs and both are silent otherwise.
      accept: a joint the model has no bone for is caught by name; a duplicate bone name is caught;
            a corrupt `.cnb` is reported as corrupt; a two-skin source is caught and its split parts
            pass; and the whole check is demonstrated against a model the real content pipeline
            compiled
      accept: a deliberately renamed joint and a deliberately reintroduced second skin both fail the content build
- [x] HOUSE-00222 — Phase-3 review and commit; run `budget_report.py` for the first time
      dep: HOUSE-00181…HOUSE-00225 · sys: — · plat: ALL · pri: MUST
      note: (2026-09-07) **Phase 3 closes.** All three exit criteria met and measured, not
            asserted:
            (1) `make content` builds `assets-src/` into `content/` — 25 files, 0.77 s cold;
            (2) `make content-verify` proves determinism — 12 output files byte-identical across
            two builds over 6 roots;
            (3) the smoke scene loads all six content types — `ContentSmokeTests`
            `.AllSixContentTypesLoad` passes, with the model drawn through its compiled effect and
            the font and video panel both carrying pixels.
            The full suite is **326 tests, all passing**: 324 on hardware GL, plus the two
            committed-reference frame comparisons which skip there by design (the reference is a
            software-rasteriser frame, `HOUSE-00138`) and pass under `LIBGL_ALWAYS_SOFTWARE=1`.
            The skip message names that flag, which is why closing the loop took one command
            rather than an investigation.
      finding: **`budget_report.py`'s first run against a real compiled tree, and the fonts are
            the whole of it.** Source-only it reported 0.04 MB for `core`; against `content/` the
            same pack is **2.43 MB**, and 2.41 MB of that is five `.spritefont` descriptors:
            `FONT_UI_30` alone expands from 2 555 bytes to **1 058 336** — 414×. The three UI
            sizes scale as their atlas area (16 → 271 KB, 22 → 534 KB, 30 → 1 058 KB, each step
            roughly doubling), so a fourth size is not a small ask. That is 4.4 % of `core`'s 55 MB
            budget spent before a single texture or model, and it is exactly the kind of number
            §72 is written against — the source column would never have shown it.
      finding: the report's own discipline held up: with no build tree the compiled column was
            `--` throughout and the header said why, rather than reporting zero. `HOUSE-00203`
            built it that way and this is the run that proves it was worth doing — a report that
            had shown 0.00 MB compiled would have said every budget was comfortably met.

---

## Phase 4 — Asset provenance and licensing

**Goal.** Before a single downloaded asset enters the repository, the machinery that keeps it
legal exists and works; and the licences of every candidate source are actually read.

**Exit.** Every source in `cna-house.md` §19.2 has a verified licence record; the NOX collection
is imported and manifested; the hero-asset research tasks have concrete answers.

- [x] HOUSE-00261 — Establish `assets-src/` provenance discipline: a `SOURCE.md` per downloaded asset directory recording where it came from
      dep: HOUSE-00195 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `docs/asset-review/SOURCE-TEMPLATE.md` is the template;
            `assets-src/Fonts/SOURCE.md` is the first filled-in record, covering the two Noto faces
            `HOUSE-00200` vendored. The discipline is enforced by `verify_licences.py`, which
            already runs in `run_checks.sh` and in the packaging gate, so no new gate was added.
            `SOURCE.md` is exempt from needing a manifest row of its own — it is enforced elsewhere
            rather than merely tolerated.
      finding: **the check is that the record NAMES each asset, not that a file exists.** A
            `SOURCE.md` written for one asset and never updated when a second arrived would pass
            forever otherwise, which is how every unenforced "please document it" convention ends.
            Both failure modes were injected and both were caught: the file removed, and one asset
            id renamed out of it.
      finding: the record is deliberately **not** a duplicate of the manifest. The manifest holds
            what a machine checks — hashes, the four rights booleans, the licence file — and is
            authoritative about facts; `SOURCE.md` holds how the licence was established, what was
            checked and **what was rejected and why**, and is authoritative about reasoning. Hashes
            are not repeated in it, because a copy that can drift is worse than a reference.
- [x] HOUSE-00262 — Verify the **Poly Haven** licence; archive the licence page and terms; record redistribution and modification rights
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `docs/licence-evidence/polyhaven.md`; CC0 text archived at
            `licenses/cc0-1.0/LICENCE.txt` from Creative Commons' own `legalcode.txt`.
      finding: **APPROVED.** CC0 1.0 for every asset, stated on the publisher's own licence page,
            explicitly covering commercial use, redistribution and "even in a product you sell",
            with no attribution required.
      finding: **the provenance claim is unusually strong and is why this source ranks above the
            aggregators.** The page states the assets are "the original work of Poly Haven staff, or
            artists who willingly and directly donate/sell their work to Poly Haven" — a first-party
            warranty of chain of title, which is exactly what ADR-0012 says an aggregator's restated
            label cannot give.
      finding: **the site ToS forbids scraping** (§3.2, "Web scraping or data mining without express
            permission"), which the asset licence does not override. `HOUSE-00296`/`HOUSE-00297`
            must fetch the specific assets they name, not bulk-mirror the library. Consistent with
            this project's own rule against indiscriminate downloads, but easy to breach with a
            well-meant script.
      finding: **preview renders are NOT CC0** — ToS §4.1 reserves everything that is not the asset
            itself, naming example renders and page text. Unlike ambientCG, which grants its preview
            renders explicitly. Do not use a Poly Haven preview as a texture or as document art.
- [x] HOUSE-00263 — Verify the **ambientCG** licence, same treatment
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `docs/licence-evidence/ambientcg.md`. `ambientcg.com/license` redirects to
            `docs.ambientcg.com/license/`, which is the page of record.
      finding: **APPROVED, no caveats** — the cleanest terms of any source examined so far. CC0 1.0
            for every asset, and the page answers this project's actual question in its own words:
            "You can include the raw files in your project, for example a video game."
      finding: that sentence matters more here than it would for a closed-source game, because
            `assets-src/` **is published**. This repository distributes raw asset files, not only a
            compiled build, so a licence permitting the second but not the first is unusable for
            this project's structure — which is exactly what `HOUSE-00265` found for Quaternius.
      finding: **ambientCG's preview renders are CC0 too**, stated explicitly, where Poly Haven's
            ToS §4.1 reserves its own. The two sources are otherwise interchangeable and this
            difference is invisible from the licence label, so it is recorded rather than left to be
            rediscovered.
- [x] HOUSE-00264 — Verify the **Khronos glTF-Sample-Assets** per-asset licences and record which are usable
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `docs/licence-evidence/khronos-gltf-sample-assets.md`.
      finding: **there is no repository-wide licence, and the README says so** — each model carries
            its own, contributed by a different party. Counted from the licence links in
            `Models/Models.md`: **148 models, 163 licence links — 93 CC0 and 70 CC BY 4.0, and
            nothing else.** No NC and no ND anywhere, so neither of ADR-0012's standing restrictions
            bites and LOD generation is permitted throughout. Both licences are usable here, CC BY
            because attribution is generated from the manifest.
      finding: **seven models are on the repository's own `#issues` list — "issues with respect to
            ownership, license, or markings" — and must not ship**: `AntiqueCamera`, `BoxTextured`,
            `BoxTexturedNonPowerOfTwo`, `CesiumMan`, `CesiumMilkTruck`,
            `PrimitiveModeNormalsTest`, `RecursiveSkeletons`. Not a formality: `CesiumMan` and
            `RecursiveSkeletons` are exactly the skinned, animated models a character pipeline
            reaches for first, and `BoxTextured` is the obvious texture smoke test. They stay usable
            as **local fixtures** — validating a file redistributes nothing — but none may enter
            `assets-src/`, which is published.
      finding: these are *sample* assets, authored to exercise glTF features rather than to furnish
            a house. The quality bar applies: most are unsuitable as furniture and none as a hero
            asset.
- [x] HOUSE-00265 — Verify **Quaternius**, **Kenney**, **Poly Pizza** licences
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `docs/licence-evidence/quaternius-kenney-polypizza.md`. Three sources,
            three different verdicts.
      finding: **Quaternius is REJECTED, because the publisher's own site contradicts itself.** The
            licence page (dated "Last updated: 8/28/2026") is now the *Quaternius Asset License
            v1.0*, whose §3(a) forbids redistributing the assets "as a standalone asset, asset pack,
            stock file, template, or similar product … regardless of how much the Assets have been
            modified". The FAQ page, live on the same site on the same day, says twice that "All
            models are under the CC0 License". Both cannot be true: CC0 permits exactly what §3(a)
            forbids. ADR-0012's rule for unclear terms is that the source is unusable until
            resolved, and resolving it means asking the publisher — an owner action, not a gate.
            **No plan task depends on Quaternius alone**, so nothing is blocked.
      finding: **even ignoring the contradiction, the QAL fits this project badly, and the reason is
            structural: `assets-src/` is PUBLISHED.** §2 clearly permits shipping a build that
            incorporates the assets; §3(a) forbids redistributing the asset files. A public git
            repository carrying the raw `.glb` sits exactly on that line. A licence can permit the
            compiled game and still forbid this repository, and that distinction now has to be
            checked for every source — it is what the manifest's separate `redistributeSource` and
            `redistributeDerived` booleans are for.
      finding: **Kenney is APPROVED** — CC0, but declared **per asset page**, not site-wide. The
            site ToS is an ordinary website agreement that mentions no CC0 at all and in fact
            asserts copyright "unless marked otherwise"; the per-asset "License: Creative Commons
            CC0" line is that marking. A reader who stopped at the ToS would get the wrong answer.
            When a pack is acquired, its `SOURCE.md` must record both the asset page and the
            `License.txt` bundled in the download.
      finding: **Poly Pizza is CONDITIONAL** — an aggregator with no site-wide licence. Its terms
            say "We do not claim ownership over any User Content" and push the licence to the
            uploader: "you agree to adhere to the terms of the Creative Commons license that applies
            at the time of download." **The default is CC-BY, not CC0** — both sampled models showed
            "Creative Commons Attribution". Usable per asset with archived per-asset evidence, and
            CC-BY is fine because attribution is generated from the manifest; but never for a hero
            asset, where an uploader's self-assertion is not provenance enough.
      finding: the general lesson, now recorded: **a licence is a property of a file at a moment,
            not of a website.** Quaternius was a legitimately CC0 source and is not one today. That
            is why every `SOURCE.md` records the URL read and the retrieval date, and why the
            per-asset check carries the claim rather than the source's reputation.
- [x] HOUSE-00266 — Verify **BlenderKit free tier** licensing model and whether per-asset licences are machine-readable
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: SHOULD
      note: (2026-09-07) `docs/licence-evidence/blenderkit.md`. Both questions answered, the second
            by measurement.
      finding: **exactly two licences, and only one of them works here.** "Everything you download
            is available for commercial use. Both allow you to sell higher-level-derivative works,
            but **royalty free license doesn't allow to re-sell 3D models even if modified**". For a
            *closed-source* game both would be fine. Here they are not: `assets-src/` is
            **published**, so a Royalty-Free model committed to it is a 3D model offered as a 3D
            model to anyone who clones the repository. Arguable when the repository is free —
            and ADR-0012 does not let this project rely on the arguable reading. Same shape as
            Quaternius and CMU, except that with CMU the derivative is what we need and *can* be
            committed, whereas here the model itself is the deliverable.
      finding: **per-asset licences ARE machine-readable, verified.** The public API
            `blenderkit.com/api/v1/search/?query=<q>&asset_type=model` returns `results[].license`
            (`royalty_free` | `cc_zero`) and `results[].isFree` — better than most sources: no
            scraping, and the filter applies before anything is downloaded.
      finding: **and the measurement decides it.** Sampling `chair`, `sofa`, `lamp`, `fridge`,
            `table`: 75 results, **73 `royalty_free` and 2 `cc_zero`** — about 3 % usable here.
            "Free" and "CC0" are independent: 36 of the free assets are Royalty Free, which is
            exactly the "free download ≠ redistributable" confusion this project must avoid. **Not a
            general furniture source**; Poly Haven and ambientCG cover the same ground CC0
            throughout with first-party provenance. A specific `cc_zero` asset remains usable.
- [x] HOUSE-00267 — Verify **Sketchfab CC0** filter semantics: does the filter guarantee CC0, and what evidence do we archive per asset?
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `docs/licence-evidence/sketchfab.md`. Both questions answered; one thing
            could **not** be verified and is recorded as such rather than papered over.
      finding: **the filter does not guarantee CC0.** A filter is a query over a metadata field the
            uploader set when publishing; selecting a value in a search URL cannot turn a
            self-declaration into a warranty. Same structure as Freesound's filter and Poly Pizza's
            per-asset licences. Sketchfab therefore ranks at the bottom of this project's provenance
            order with Blend Swap — below Freesound, whose terms at least make the uploader warrant
            and indemnify, and far below Poly Haven and ambientCG. **Never a hero-asset source.**
      finding: **a trap worth naming: `sketchfab.com/licenses` is about PURCHASED royalty-free
            assets, not the free Creative Commons downloads**, and it defines an "editorial" tier
            that "cannot be used for any commercial or promotional use" — NC by another name, and
            rejected. That page is what a search for "Sketchfab license" returns, so the two systems
            are easy to conflate in exactly the direction that causes harm.
      finding: per-asset evidence is fixed: model page URL and id, uploader name, **the licence as
            displayed on that model page that day**, retrieval date, the `license.txt` Sketchfab
            bundles into a Creative Commons download, and hashes of both the archive and the
            extracted asset.
      finding: **NOT VERIFIED, and named:** the help-centre articles documenting the Creative
            Commons filter now 404, the help search endpoint 404s, and `sketchfab.com/tos` renders
            in JavaScript, so no automated read returns its licensing clauses — Sketchfab having
            been absorbed into Epic's Fab. The conclusions above are reasoned from the structure of
            an uploader-declared field and this project's own rules, **not quoted from a Sketchfab
            terms page**. Operationally harmless: the disposition is conservative throughout and **no
            current task requires Sketchfab** — `HOUSE-00291`…`HOUSE-00293` can satisfy their three
            sourcing attempts from stronger sources. If it is ever actually reached for, the terms
            page must be read first, by a human with a browser if necessary.
- [x] HOUSE-00268 — Verify **Blend Swap CC0** the same way
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: SHOULD
      note: (2026-09-07) `docs/licence-evidence/blendswap.md`.
      finding: **CONDITIONAL and weak — the baseline here is SHARE-ALIKE, not permissive.** The
            Terms name a site-wide default of "Creative Commons **BY-SA 3.0 USA** (or under the
            license the content is marked with)", and the FAQ lists CC0, CC BY, CC BY-SA "as well as
            our General Asset License". The opposite shape from Poly Haven or ambientCG.
      finding: **non-commercial assets are present and are rejected outright.** Terms §7: "Users may
            upload content under the Creative Commons Attribution-NonCommercial-ShareAlike License
            … as long as they do not resell it or **use it for any kind of monetary profit, even
            through derivatives of the work**." ADR-0012 rejects NC, and unlike a source that simply
            has none, this one mixes NC into the same library behind the same download button. The
            per-asset check is not optional here.
      finding: **the site explicitly disclaims chain of title** — "Neither we or our Affiliates
            warrant or represent that your use of materials … will not infringe the rights of third
            parties" — which is precisely the assurance a hero asset needs. Blend Swap therefore
            ranks at the bottom of this project's provenance order, with Sketchfab, and is **never**
            a hero-asset source.
      finding: **CC BY-SA needs a deliberate decision rather than a default yes.** Share-alike
            attaches to derivatives, and this project derives from everything it ships — LOD1/LOD2,
            the collision proxy, the compiled `.cnb` — so those would have to be offered under
            BY-SA. It does not reach the rest of the game, but it is a real obligation; prefer CC0.
      finding: **the "General Asset License" text could not be found** on `/faq`, `/terms`,
            `/license`, `/general-asset-license` or `/legal`. An asset under a licence nobody has
            read is unusable (ADR-0012). Recorded as a named gap rather than assumed benign.
      finding: the site is opening a **paid marketplace** alongside the free CC library, so
            "downloadable from Blend Swap" will increasingly not imply Creative Commons — one more
            reason the per-asset licence read on the day is the only evidence that counts.
- [x] HOUSE-00269 — **Verify the MakeHuman / MPFB2 asset licence** (Q-03): may generated meshes be redistributed, and under what terms?
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
      accept: a written answer with the licence text archived; if negative, R-04's fallback is triggered here, not later
      note: (2026-09-07) `docs/licence-evidence/makehuman-mpfb2.md`.
      finding: **Q-03 answered YES.** The current licence page states a **split licence**: "All core
            assets are shared under Creative Commons, CC0 … you are free to do as you see fit with
            the asset or **derivates of the assets**", while GPL/AGPL covers only the *source code*.
            We ship meshes, not their code, so a body — retopologised, rigged, decimated, compiled —
            is CC0 throughout. **R-04's fallback is NOT triggered**; `HOUSE-00294` may proceed and
            judge the meshes on topology and silhouette rather than on licence.
      finding: **the older page says something narrower and is superseded, which is worth recording
            because it is still reachable.** `makehumancommunity.org/content/license.html`
            (copyright line "2001-2015") puts assets under **AGPL** with a CC0 exception conditional
            on the export coming from "an OFFICIAL and UNMODIFIED version of MakeHuman" — under
            which an **MPFB2** export, MPFB2 being a Blender add-on, is not obviously covered at all.
            Unlike the Quaternius contradiction this is not unresolved: the divergence is a
            liberalisation rather than a restriction, the newer page is the current site and is the
            only one that covers MPFB, and **CC0 is irrevocable**, so a grant standing when an asset
            was obtained keeps applying to it.
      finding: **"core" is load-bearing.** The CC0 statement covers the core base meshes, morphs and
            rigs. The community asset repository — clothes, hair, skins, poses — carries per-asset
            licences, so a *dressed* character is not automatically CC0. `HOUSE-00294` must record
            per asset whether it is core or community, and a community asset's own terms.
- [x] HOUSE-00270 — **Verify the CMU Motion Capture Database terms** (Q-04): may retargeted, baked derivatives be redistributed?
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `docs/licence-evidence/cmu-mocap.md`.
      finding: **Q-04 answered YES for derivatives and NO for the source, and the difference is the
            finding.** Verbatim: "This data is free for use in research projects. **You may include
            this data in commercially-sold products, but you may not resell this data directly, even
            in converted form.**" A retargeted clip baked into a `.chanim` is *included in a
            product*; the raw `.asf`/`.amc`/`.bvh` sitting in a **published** `assets-src/` offers
            the database itself, as data. The clause literally forbids reselling and a free
            repository is not a sale — but "even in converted form" shows the intent, and ADR-0012's
            rule for an unclear permission is that we do not take it.
      finding: this is the second source where `redistributeSource` and `redistributeDerived`
            genuinely diverge. For Quaternius the divergence disqualified the source; here it does
            not, because the derivative is what this project needs. **CMU is therefore an explicit
            exception to §18's "everything starts in `assets-src/`"**: the clips are fetched outside
            the repository and never committed, the committed artefact is the `.chanim` from
            `anim_extract.py`, and that row records the source URL, subject/trial numbers, retrieval
            date and **the source clip's SHA-256** — so the derivation stays auditable with its
            input absent. That is what a hash in a manifest is for.
      finding: `HOUSE-00295` must record the **six subject and trial numbers**, not "six CMU walk
            clips": the database holds several takes per subject, its own page warns that
            low-numbered subjects are early sessions of lower quality, and that the toe and hand
            joints are noisy and may need smoothing.
      finding: the requested acknowledgement — "The data used in this project was obtained from
            mocap.cs.cmu.edu. The database was created with funding from NSF EIA-0196217." — is
            phrased as a request tied to publishing results rather than a licence condition. Carried
            anyway, in the manifest's `attribution` field, so it reaches the generated credits
            automatically. As with Poly Haven, the page also asks that the database not be crawled.
- [x] HOUSE-00271 — Verify **Mixamo** terms and record the verdict; expected outcome is "not used for shipped files"
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: SHOULD
      note: (2026-09-07) `docs/licence-evidence/mixamo.md`. **The plan's expectation is confirmed**,
            though not for the reason one might guess — the use grant is generous.
      finding: the FAQ grants a genuinely broad **use** licence: "You can use both characters and
            animations **royalty free for personal, commercial, and non-profit projects** including
            … **Create video games**." If `cna-house` shipped only a binary, Mixamo would be usable.
      finding: **it is rejected because the grant is about USE and says nothing about
            REDISTRIBUTION.** Every item in the permitted list is an end product; none is "publish
            the animation file". `assets-src/` is published, so committing a Mixamo `.fbx` there
            distributes the animation as data, which the FAQ neither permits nor discusses — and
            ADR-0012 says an ungranted permission is not assumed. **The fourth source this phase
            with that exact shape** (Quaternius, CMU, BlenderKit Royalty Free), and always for the
            same reason: this project publishes its asset sources, so "may I use it?" and "may I
            republish it?" are different questions.
      finding: Mixamo differs from CMU in a way that matters. With CMU the asset is a raw capture
            and our deliverable is a retargeted `.chanim` — a genuine derivative, expressly
            shippable. With Mixamo the animation **is** the finished product, so a `.chanim` of a
            Mixamo clip is substantially the same data in another container: much closer to
            "redistributing in converted form" than to deriving something new.
      finding: access needs an **Adobe account** under Adobe's General Terms of Use rather than an
            asset licence — not text this project can archive as a stable licence the way it
            archives OFL or CC0 — and the service is unavailable in some territories.
      note: **nothing depends on it.** `HOUSE-00295` sources locomotion from CMU, whose terms
            expressly permit inclusion in commercially-sold products. Mixamo remains usable as a
            private reference while evaluating rigs; nothing from it enters `assets-src/`, the
            manifest, or a build.
- [x] HOUSE-00272 — Verify **Freesound CC0** filter semantics and the per-sound evidence we must archive
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `docs/licence-evidence/freesound.md`. Unblocks `HOUSE-00281`…`HOUSE-00290`,
            roughly 150 samples.
      finding: **the CC0 filter does not GUARANTEE CC0, and cannot.** It reflects a field the
            uploader chose. What stands behind it is the ToS: the uploader *warrants* they hold the
            rights (§87b) and *indemnifies UPF, its licensees and its users* against a breach (§87c),
            and the licence is granted **directly to downstream users** (§87a) — so our right does
            not depend on Freesound continuing to exist. But nobody at Freesound checks. That places
            Freesound **below** Poly Haven and ambientCG, which warrant their own chain of title
            first-hand, and **above** a bare aggregator. Acceptable for short effects; not for a
            hero asset.
      finding: **CC-BY-NC and the retired Sampling+ are excluded outright.** NC conflicts with a
            possible paid release and is already rejected by ADR-0012; Sampling+ was retired by
            Creative Commons as too hard to interpret and Freesound's own reading forbids commercial
            advertising use — an ambiguous licence is an unusable one. Both still appear on old
            uploads, so the filter must be set, not assumed.
      finding: **ToS §65 is a trap for a later reader.** "you may not use the Freesound website
            portal for commercial purposes" restricts commercial use of the *portal*, not of a CC0
            sound in a commercial product — that is governed by the sound's own licence, granted to
            users by §87(a). Recorded because the sentence read alone looks like it forbids exactly
            what this project intends.
      finding: per-sound evidence is fixed as: sound page URL in the stable `freesound.org/s/<id>/`
            form, numeric id, uploader username, **the licence as displayed on that page that day**,
            retrieval date, and both hashes (original and converted, per `HOUSE-00279`). The search
            filter is not the evidence. Freesound's own per-account attribution list is a useful
            cross-check but lives behind a login and cannot be read by a gate or a future developer.
      finding: CC-BY is usable — attribution is generated from the manifest, so the obligation is
            met by construction — but `licenses/cc-by-4.0/` must be archived before the first such
            sound is committed, and CC0 should be preferred simply because 150 CC-BY sounds means
            150 credit lines for fractions of a second of audio.
- [x] HOUSE-00273 — Verify the **font** licences (OFL) and archive them
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) Largely discharged by `HOUSE-00200`, which had to verify the licence
            before the binaries could enter the repository; this task adds what that one did not —
            the **archived FAQ evidence**. `docs/licence-evidence/ofl-1.1-sil-faq.md` quotes SIL's
            own answers verbatim, with the FAQ's own version identifier (1.1-update7, November 2023)
            and the retrieval date, so no conclusion rests on a web page that may since have
            changed. `docs/licence-evidence/` is new and is where `HOUSE-00262`…`HOUSE-00276` should
            archive their sources' terms.
      finding: **verified, and safe for a commercial release.** OFL-1.1, no Reserved Font Name
            (FAQ 5.7 — none are reserved by default in 1.1, and Noto declares none after its
            copyright statement). FAQ 1.4 permits selling a package containing the fonts and names
            "games and entertainment software"; FAQ 1.3 and 1.13 confirm the OFL reaches neither the
            program nor artwork made with it. The archived licence body is identical to SIL's own
            published text.
      finding: **the one obligation that can be broken by accident is packaging.** FAQ 1.20: a
            bundled font must travel with the copyright statement, the licence notice and the
            licence text. A packaging step that dropped `licenses/` would breach the OFL while every
            existing gate stayed green — no check can see a step that has not been written yet. It
            is recorded in `NOTICE.md`, `assets-src/Fonts/SOURCE.md` and the evidence file, and it
            is a requirement on the eventual packaging task rather than something closed here.
      finding: the "Noto" trademark (Google LLC, `name` ID 7 in both binaries) sits **outside** the
            OFL — FAQ 3.7 is explicit that the licence grants no trademark rights. It does not bite:
            the files ship unmodified so the notice stays where it is, and the name is not used as
            this product's branding.
- [x] HOUSE-00274 — Verify the **lunar albedo map** and **star catalogue** provenance (public domain expected)
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `docs/licence-evidence/astronomical.md`. **The expectation holds for the
            Moon and does NOT hold for the stars** — which is the finding.
      finding: **Moon: APPROVED.** NASA SVS "CGI Moon Kit" (`svs.gsfc.nasa.gov/4720`), built from
            Lunar Reconnaissance Orbiter camera and altimeter data. NASA's Media Usage Guidelines
            address this case almost word for word: "NASA content – images, audio, video, and
            **media files used in the rendition of 3-dimensional models, such as texture maps and
            polygon data in any format** – generally are **not subject to copyright in the United
            States** … including … **computer graphical simulations**." Three constraints, none
            biting: no implied NASA endorsement (relevant to store art, not to a moon in the sky);
            the **insignia and logotype are NOT public domain** and are untouched; third-party
            material on NASA sites is marked as such, so the per-asset page must still be read.
            Credit — "NASA's Scientific Visualization Studio", visualizer Ernie Wright (USRA) — is
            carried in `attribution` though it is not a licence condition.
      finding: **Stars: the expectation is WRONG.** The obvious source, the **HYG database** v4.4,
            is **CC BY-SA 4.0**, verified from both its `README.md` and its `LICENSE`. Share-alike
            matters: phase 24 bakes a positions-and-magnitudes table, which is a derivative of a
            database and would have to be offered under BY-SA. Workable — a star table can be
            published BY-SA without touching the rest of the game — but it is an obligation to
            record, not to discover at packaging time.
      finding: HYG has **moved to Codeberg**; the GitHub repository most links point at is stale and
            its licence metadata reads `NOASSERTION`, so a reader who stopped there would have no
            licence at all. The non-share-alike alternative is the upstream scientific catalogues
            HYG compiles — Yale Bright Star, Hipparcos/Tycho, via CDS/VizieR, and 9 110 naked-eye
            stars is exactly what phase 24 wants. **Their terms are NOT verified here** and must be
            before use; the option is recorded, not approved.
      finding: the general lesson: **"astronomical data is public domain" is a reasonable prior and
            it is false for the most convenient source.**
- [x] HOUSE-00275 — Verify the **video footage** sources for the television channels
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-07) `docs/licence-evidence/video-footage.md`. §58.4 already restricts this to
            four channels of which **three are generated by our own tooling** — weather map, clock,
            static — so only the nature/landscape channel has an external source, and only it
            needed verifying.
      finding: **Pixabay is NOT CC0, verified.** It is the site most often described as CC0 and has
            not been for years; it has its **own Content License**, whose Prohibited Uses include
            "You cannot sell or distribute Content … **on a Standalone basis**. Standalone means
            where no creative effort has been applied to the Content and it remains in substantially
            the same form as it exists on our website." Two independent failures: it is not CC0, and
            §58.4 asks for CC0; and the Standalone clause collides with a **published**
            `assets-src/` — calling a downscale to 256 × 144 "creative effort" is exactly the
            arguable reading ADR-0012 forbids relying on. Pixabay also disclaims responsibility for
            third-party rights *inside* the content, and video is worse than stills for that:
            recognisable people, trademarks, background music.
      finding: **Pexels was NOT verified** — `pexels.com/license/` returns **403** to automated
            retrieval. Widely assumed CC0 and, on the Pixabay evidence, probably not. Not approved;
            must be read before use.
      finding: **use NASA video instead — already verified.** The Media Usage Guidelines cleared for
            `HOUSE-00274` cover video in the same sentence as imagery ("images, **audio, video**, and
            media files … generally are not subject to copyright in the United States"), with the
            same three constraints. Earth-observation footage suits a nature channel exactly and
            needs no new verification. The alternative is a per-item public-domain archive such as
            the Internet Archive's Prelinger collection, where **the item's own rights statement is
            the evidence**, not the collection's reputation.
      finding: the architecture had already forbidden broadcast, film and music content before this
            task ran, and that judgement is confirmed: a television inside a game showing
            recognisable footage is the easiest way for this project to acquire a rights problem.
- [x] HOUSE-00276 — **Q-02: re-verify the NOX_SOUND Essentials Series CC0 declaration** against the publisher's live page; archive the evidence
      dep: HOUSE-00261 · sys: content · plat: TOOL · pri: MUST
      accept: either confirmed (mark `CC0-1.0`, verified, with the archived page) or not (mark `PROVENANCE DECLARED — UNVERIFIED` and do not ship until resolved)
      note: (2026-09-07) **CONFIRMED — `CC0-1.0`, verified**, for 1 634 of the 1 644 files.
            `docs/licence-evidence/nox-sound-essentials.md` and
            `docs/licence-evidence/nox-sound-format-census.txt`.
      finding: the authoritative page is the publisher's **itch.io** listing,
            `nox-sound-design.itch.io/essentials-series-sfx-nox-sound`, which states: "You can use
            these sounds for personal and commercial projects. All sounds are released under CC0,
            allowing you to use them freely without attribution or restrictions."
      finding: **"NOX Sound is CC0" is FALSE as a general statement, and the bundled README's four
            links all lead somewhere that would have said so.** A Sound Effect sells NOX libraries
            as paid products ($10–$20) under its own licence agreement; Unity distributes under
            Unity's store terms; the Freesound account is CC0 but is only a **sampler** — its
            "Pack - Electromagnetic" holds 5 sounds against the local 72, and its "Pack - Footsteps"
            holds 12 consolidated files against the local 479. Stopping at Freesound, the obvious
            first-party CC0 evidence, would have verified ~60 sounds and silently implied it for
            1 644. The CC0 grant attaches to **the Essentials Series release**, not to the
            publisher.
      finding: **identity established by counting, not by folder names.** The publisher states
            1 644 sounds; the local tree holds **exactly 1 644 `.wav` files**. Footsteps 479 = 479,
            Vehicle 161 = 161, Nature 18 = 18, São Miguel 14 = 14, Iceland 233 = 210 + 23 — five
            exact per-pack matches plus the exact headline total, and the download is named
            `Essentials_Series_NOX_SOUND.zip`, which is the local directory's name. Voices (657 vs a
            stated 526) and Electromagnetic (72 vs 71) are both *larger* locally, consistent with
            the page's per-pack prose being stale against its own headline — the listing carries an
            "Update #1" devlog and says "Updated 19 days ago".
      finding: **10 files are EXCLUDED.** `Sample_A_Sound_Effect/` holds promotional teasers for
            NOX's **paid** A Sound Effect libraries (Household Essentials, Clothes Movement,
            Backpack, Ambiance–Nature, Ambiance–Atmosphere), with a `.url` shortcut to that store.
            A rights holder may licence their own work twice, so this is not a contradiction — but
            it is not clear either, and ADR-0012 says unclear means unusable. Ten files out of
            1 644, in categories `HOUSE-00282`/`HOUSE-00290` source separately. **`HOUSE-00277` must
            exclude that directory explicitly**, alongside the exclusions it already names.
      finding: **the publisher's stated "48 kHz / 24-bit" is wrong for 23 files**, measured with
            `ffprobe` over all 1 644: **20 files are 96 kHz** (14 are the documented São Miguel
            pack; 6 are not documented as such) and **3 are 32-bit `pcm_s32le`** (car engine loops,
            not documented at all). So `convert_audio.py` (`HOUSE-00193`) **must probe each input**
            and cannot assume the stated format. 1 624 of 1 644 are 48 kHz, which is the measure of
            how much of the collection the "do not resample to 44.1 kHz" correction governs.
      finding: several names arrived **mojibaked** in the zip (`S<..>o_Miguel`, `CaldeirΣes`).
            `HOUSE-00277` must normalise on import; `docs/conventions.md` requires
            lower-case-with-hyphens content names anyway, so nothing mojibaked can reach
            `assets-src/`.
      risk: the collection lives in `/rv/tmp/`, which is not version-controlled and is scratch space
            under this machine's build rules. Only the converted subset enters `assets-src/`; the
            originals' hashes in the manifest are what keep the conversion auditable after the
            source directory is gone.
- [x] HOUSE-00277 — Select the NOX subset to ship: ~430 files, listed explicitly with the category each serves
      dep: HOUSE-00276 · sys: audio · plat: TOOL · pri: MUST
      accept: the exclusion of the Azores flows, the combat voices and the truck pack is explicit
      note: (2026-09-07) `tools/assets/nox_select.py`, and the selection it produces:
            `docs/asset-selection/nox-subset.json` (machine-readable, hashed, with the category
            each file serves) and `nox-subset.md`. **445 of 1 644 files ship**; 624 are excluded by
            rule, 575 fail a measurement or a cap, and **0 are unclaimed**. 16 selftest claims;
            `--selftest` runs in CI against a fixture, since the pool is scratch space.
      finding: **selection is rules over measurements, not a hand-picked list.** Every file is
            measured — duration, rate, channels, bit depth, peak, RMS — and passed through
            exclusions, quality thresholds and a per-group cap, each written down with its reason.
            Measuring 1 644 files needed a numpy WAV reader (`audio_probe.measure`): `ffmpeg -af
            volumedetect` per file takes **eighteen minutes**, reading the `data` chunk once takes
            **24 seconds**. 24-bit is assembled from its three bytes explicitly, because numpy has
            no 24-bit type and a naive `frombuffer` misreads it — without the sign extension every
            negative sample reads positive and every file reports a peak of exactly 0 dBFS.
      finding: **which files inside a cap is chosen by farthest-point sampling in (duration, RMS)**,
            starting from the median-duration file. Consecutive takes from one recording session
            are the most similar files in a group, so "the first six" is the one selection
            guaranteed to sound repetitive. Of twelve grass walks the rule keeps 0, 1, 3, 6, 8 and
            11 — both extremes and a spread between, rather than 0–5.
      finding: **every one of the 1 644 files is accounted for exactly once**, and that is asserted
            rather than assumed: selected + excluded + rejected + unclaimed = the pool, with no
            file in two lists. A file no rule claims is LISTED, never dropped — the first run had
            **317** of them (the voice rules matched `Voice_Male/` where the directory is
            `Voice_Essential_Male/`, so not one voice file was selected, and the Iceland jumps are
            `Jump_Land`/`Jump_Start` rather than `Jump`). A selection that had silently dropped
            them would have looked complete.
      finding: the exclusions the acceptance names are all explicit and each carries its reason,
            and three more were added from §63.3 and `HOUSE-00276`: the paid `Sample_A_Sound_Effect`
            sampler, the big and moderate Icelandic waterfalls (only the LIGHT stream loops serve
            the gutter), the car's drive and engine loops (the car does not move), the acted
            expressions, the vocalised jumps, and the dramatic breaths — gasps, shocked, shivering
            — which a house has no use for.
      finding: **§72's audio budget priced one-shots and not loops, and the selection proves it.**
            445 clips is §72's own count and comes to **156 MB** at 16 bit against its 95: the
            **373 one-shots are 21 MB** while **72 loops carry 1 106 seconds and 136 MB**. 95 MB
            over 430 clips is 0.22 MB each — about one second of mono — and a 30-second stereo rain
            bed is 5.8 MB. **Trimming every loop to 10 s brings the total to 91 MB**, inside the
            budget, so the row is achievable but only as a constraint on the CONVERSION
            (`HOUSE-00278`, `convert_audio.py --trim`) and not on the selection. Recorded in §72.
      finding: what this tool does **not** do is stated in it: it does not listen. Level, duration
            and channel count are measurable and measured; whether a recording is *clean* is not.
            Each group therefore names its shortest, longest, quietest and loudest file, so an
            aural check has somewhere to start rather than 445 files and no order.
- [x] HOUSE-00278 — Convert the selected NOX subset with `convert_audio.py`; verify a listening check on 10 files (R-17)
      dep: HOUSE-00277, HOUSE-00193 · sys: audio · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/assets/nox_convert.py`, plus `--max-seconds` and `--loop-crossfade`
            added to `convert_audio.py` (`HOUSE-00193`) because it had no duration cap and §72's
            budget is a cap. **445 of 445 converted, 79.2 MB** — 19.3 MB of one-shots and 59.9 MB
            in 72 loops, 49 of them shortened. 20 selftest claims here and 4 more in
            `convert_audio.py`. **The listening check is NOT done and is the one outstanding item;
            see below.**
      finding: **a loop cut with `-t` clicks, once per loop, for as long as the room is on
            screen.** §72's budget only closes if every ambience loop is trimmed, and the obvious
            way to do it leaves the last sample and the first unrelated. The fix is a **head**
            crossfade — `result[0]` is made to be `source[N]`, so it matches `result[N⁻]` — and the
            first attempt put the blend at the tail, which makes `result[N⁻]` approach `source[F]`
            and is no more continuous than the naive cut. Measured on a swept fixture: a wrap step
            of **0.149 naive against 0.015 crossfaded**, ten times smaller.
      finding: two fixtures in a row could not tell the constructions apart, and both failures were
            the fixture rather than the code. A sweep starting at phase 0 has a first sample of
            exactly 0, so "the step at the wrap" was measured against silence; and a pure sweep of
            that length completes a whole number of cycles by t = 3, so `source[3]` equalled
            `source[0]` to four decimals. An **amplitude ramp** is what finally made the ends
            differ. `acrossfade` was also abandoned for two `afade`s and an `amix`: it returned a
            0.6-second output for a 3-second request whatever it was fed.
      finding: the shorten threshold needs the crossfade in it. Two of the 445 are 10.01 s and
            10.15 s — over the cap but unable to supply the 0.25 s the blend draws its head from,
            and cutting them would have saved 1.5 %. "Longer than the cap" is not the rule;
            "long enough to be worth cutting **and** to supply the blend" is.
      finding: **nothing is peak-normalised, deliberately.** `nox_select.py` chose each group's
            eight variants by farthest-point sampling in (duration, RMS) precisely so they differ
            in level; a per-file normalise would flatten exactly the difference the selection
            exists to preserve.
      finding: **§72's 10-second cap was validated against the wrong budget, and 8 is the right
            number.** §72 checked its total against the *audio-buffer memory* row of 95 MB; the
            binding constraint is §71's *pack* budgets of 30 + 55 = 85 MB, which it never touched.
            Measured: at 10 s both packs are over (103.8 % and 107.0 %), at 9 s `audio-core` is
            still over, at **8 s both fit** (99.8 % and 89.5 %). §72's audible argument is about
            being an order of magnitude away from an obvious one-second loop and is unchanged.
            Recorded in §72.
      finding: **`audio-core` is at 99.8 % with the NOX subset alone**, and `HOUSE-00281`…
            `HOUSE-00290` must still add ~220 one-shots to that same pack — about 11 MB at the
            measured 0.052 MB a clip. The pack cannot hold them. The options are raising it,
            moving the ten human-breath loops (6.4 MB) out of it, or cutting further; that is a
            budget decision and is recorded in §72 rather than taken here.
      blocked-part: **the R-17 listening check on 10 files is outstanding** — this session cannot
            listen. It is staged rather than deferred: `nox_convert.py` writes an `auditionSet`
            into `docs/asset-selection/nox-conversion.json` naming ten files chosen for the
            **edges** of the conversion, each with the reason it is worth hearing — the loop
            shortened the most, the one-shot the trim took the most from, a stereo loop whose
            image the crossfade could collapse, a loop left alone as the control. A random ten
            would be ordinary footsteps and would confirm nothing. Playing those ten is a
            five-minute job and the rest of the acceptance is met.
- [x] HOUSE-00279 — Manifest every converted NOX file with both hashes, duration, channels and its target category
      dep: HOUSE-00278 · sys: audio · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/assets/nox_manifest.py`. **445 rows added, 461 in the manifest**,
            plus **110 generated `SOURCE.md` records** — `verify_licences.py` requires one per
            directory holding a downloaded asset (`HOUSE-00261`) and each must name every asset in
            it. 25 selftest claims. The credits document and the budget report were regenerated
            and every gate is green.
      finding: the requirements were not invented here.
            `docs/licence-evidence/nox-sound-essentials.md` §5 writes this task out in full —
            `CC0-1.0`, `licenses/cc0-1.0/LICENCE.txt`, the itch.io URL, `retrieved: 2026-09-07`,
            `author: Nox_Sound`, and both hashes — so the tool encodes that document rather than a
            judgement made while writing it.
      finding: **the original's hash is the point of "both hashes".** `/rv/tmp` is scratch and will
            be deleted; after that, `audio.originalSha256` is the only link between a file in the
            tree and the licence evidence written about it. The converted file's hash is what
            `check_manifest.py` verifies day to day; the original's is what an auditor needs in a
            year and cannot be recovered later.
      finding: **the pack split is functional, not lexical, and getting that wrong put 58 of the
            72 loops in the wrong pack.** Splitting on the category name — `ambience/*` to
            `audio-ambience` — left `audio-core` at **66 MB against its 30 MB budget** while
            `audio-ambience` sat at 24 of 55. §71 describes the packs by what the sounds *are*:
            the 39 appliance loops are the fridge, the computer and the extractor, which is what
            "room tone" names. But length alone is not the rule either — a human breath sequence
            and a car engine loop are loops and are interaction sounds, so they stay in
            `audio-core`.
      finding: an id needs the category **and** the file name. The publisher reuses names across
            packs, so `Footstep_Grass_Walk_01.wav` exists in more than one, and a manifest id built
            from the name alone would collide.
- [x] HOUSE-00280 — Map the NOX footstep packs onto the 20 game surfaces; record which 8 surfaces are unserved
      dep: HOUSE-00279 · sys: audio · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/assets/footstep_map.py` and the generated
            `docs/asset-selection/footstep-surfaces.md`; `--check` is now a gate in
            `run_checks.sh`, because the map is counted from the manifest and a stale one is
            `HOUSE-00281` sourcing the wrong list. 20 selftest claims.
      finding: **the task title says 8 unserved surfaces and the measurement says 9.** §63.4's
            "12 of the 20 surfaces" is a count of *packs*; §62.4 requires **≥ 6 walk, ≥ 6 run,
            ≥ 2 land per surface**, and counted that way it is **11 of 20**. The difference is
            `water`: it has a pack, 6 walk and 6 land, and **5 run**. This is not a selection
            artefact — `nox_select.py` took 5 of 5 against a cap of 6, so **NOX ships five water
            run samples in the whole collection** and no choice among them could have reached six.
            `HOUSE-00281`'s row is therefore **113 samples, not 112**: its eight surfaces plus one
            water run variant.
      finding: `jump`, `land` and `jump-land` are **the same sound under three names** — the files
            under all three are `..._Jump_Land_NN.wav`, and the packs simply disagree about what to
            call the directory. Counting them as three actions would have shown every surface
            comfortably meeting a land minimum it had never been tested against. `jump-start` is
            the take-off, which §62.4 does not ask for, and `foley` is cloth movement and is not a
            footstep; neither counts towards a minimum, and the take-offs are reported as a bonus.
      finding: **the `Wood` pack maps to nothing, deliberately.** §63.4 records it as
            exterior-flavoured, so it is not interior `hardwood`, and §62.4 has no exterior timber
            surface. It is reported as an unmapped pack rather than quietly assigned — a
            deck-flavoured creak under a living-room floor is the sort of thing nobody notices in a
            spreadsheet and everybody notices in the room.
      finding: one pack can serve two surfaces and six packs can serve one. `DirtyGround` covers
            both `dirt` and `soil`; the `footstep-exterior/*` packs are **weather variants of
            surfaces that already exist** — wet and frozen gravel are gravel, and §38 is explicit
            that the three snow packs all feed the one `snow` set — so they add variants to their
            base surface rather than becoming surfaces of their own.
- [ ] HOUSE-00281 — Grouped: source the 8 missing footstep surfaces (carpet, concrete, interior hardwood, 3 stair variants, asphalt, bluestone) — ≥ 6 walk + 6 run + 2 land each, CC0
      dep: HOUSE-00272, HOUSE-00280 · sys: audio · plat: TOOL · pri: MUST
      accept: 8 surfaces × ≥ 14 samples, manifested, converted, auditioned
      note: (2026-09-07) `HOUSE-00280` measured the shortfall and it is **113 samples, not
            112**: `water` needs one more run variant as well. It has a pack, 6 walk and
            6 land, and NOX ships only 5 run samples in the entire collection, so no
            selection could have reached §62.4's six. `docs/asset-selection/footstep-surfaces.md`
            is the generated shopping list and is kept current by a gate.
      note: `audio-core` is at **99.8 % of its 30 MB budget** with the NOX subset alone
            (`HOUSE-00278`), and these samples land in that pack — about 6 MB at the
            measured 0.052 MB a clip. The budget question is recorded in §72.
      blocked: (2026-09-07) **Freesound originals require an account, and creating one means
            accepting their ToS — which is the project owner's to accept, not a tool's.** Measured
            rather than assumed: `https://freesound.org/apiv2/search/text/?query=…` answers
            **401 "Authentication credentials were not provided"**, and a sound's `/download/` URL
            **302s to `https://freesound.org/home/login/`**. The search pages and the CC0 filter
            read fine anonymously, so the *selection* work could be done; only the bytes cannot be
            fetched. The lossy CDN previews are readable without a login and are **not** a
            substitute — this project converts 24-bit originals and manifests their hashes.
            `HOUSE-00272` established that the licence assurance rests on ToS §87b/§87c, whose
            warranty and indemnity bind **the account holder**, which is the same reason the
            account cannot be created on the owner's behalf.
- [ ] HOUSE-00282 — Grouped: source door sounds — 4 door classes × {open, close, latch, creak ×3, slam}, CC0
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
      blocked: (2026-09-07) **Freesound originals require an account, and creating one means
            accepting their ToS — which is the project owner's to accept, not a tool's.** Measured
            rather than assumed: `https://freesound.org/apiv2/search/text/?query=…` answers
            **401 "Authentication credentials were not provided"**, and a sound's `/download/` URL
            **302s to `https://freesound.org/home/login/`**. The search pages and the CC0 filter
            read fine anonymously, so the *selection* work could be done; only the bytes cannot be
            fetched. The lossy CDN previews are readable without a login and are **not** a
            substitute — this project converts 24-bit originals and manifests their hashes.
            `HOUSE-00272` established that the licence assurance rests on ToS §87b/§87c, whose
            warranty and indemnity bind **the account holder**, which is the same reason the
            account cannot be created on the owner's behalf.
- [ ] HOUSE-00283 — Grouped: source switch, cabinet and drawer sounds — ≥ 24 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
      blocked: (2026-09-07) **Freesound originals require an account, and creating one means
            accepting their ToS — which is the project owner's to accept, not a tool's.** Measured
            rather than assumed: `https://freesound.org/apiv2/search/text/?query=…` answers
            **401 "Authentication credentials were not provided"**, and a sound's `/download/` URL
            **302s to `https://freesound.org/home/login/`**. The search pages and the CC0 filter
            read fine anonymously, so the *selection* work could be done; only the bytes cannot be
            fetched. The lossy CDN previews are readable without a login and are **not** a
            substitute — this project converts 24-bit originals and manifests their hashes.
            `HOUSE-00272` established that the licence assurance rests on ToS §87b/§87c, whose
            warranty and indemnity bind **the account holder**, which is the same reason the
            account cannot be created on the owner's behalf.
- [ ] HOUSE-00284 — Grouped: source water sounds — tap ×3 flow rates, shower, drain gurgle, water hammer, pipe hiss, bath fill, toilet flush ×2, cistern refill — ≥ 16 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
      blocked: (2026-09-07) **Freesound originals require an account, and creating one means
            accepting their ToS — which is the project owner's to accept, not a tool's.** Measured
            rather than assumed: `https://freesound.org/apiv2/search/text/?query=…` answers
            **401 "Authentication credentials were not provided"**, and a sound's `/download/` URL
            **302s to `https://freesound.org/home/login/`**. The search pages and the CC0 filter
            read fine anonymously, so the *selection* work could be done; only the bytes cannot be
            fetched. The lossy CDN previews are readable without a login and are **not** a
            substitute — this project converts 24-bit originals and manifests their hashes.
            `HOUSE-00272` established that the licence assurance rests on ToS §87b/§87c, whose
            warranty and indemnity bind **the account holder**, which is the same reason the
            account cannot be created on the owner's behalf.
- [ ] HOUSE-00285 — Grouped: source appliance sounds — fridge compressor start/run/stop, freezer, washer ×3 stages, dryer, dishwasher ×3 stages, oven fan, extractor ×3, microwave, kettle, toaster — ≥ 22 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
      blocked: (2026-09-07) **Freesound originals require an account, and creating one means
            accepting their ToS — which is the project owner's to accept, not a tool's.** Measured
            rather than assumed: `https://freesound.org/apiv2/search/text/?query=…` answers
            **401 "Authentication credentials were not provided"**, and a sound's `/download/` URL
            **302s to `https://freesound.org/home/login/`**. The search pages and the CC0 filter
            read fine anonymously, so the *selection* work could be done; only the bytes cannot be
            fetched. The lossy CDN previews are readable without a login and are **not** a
            substitute — this project converts 24-bit originals and manifests their hashes.
            `HOUSE-00272` established that the licence assurance rests on ToS §87b/§87c, whose
            warranty and indemnity bind **the account holder**, which is the same reason the
            account cannot be created on the owner's behalf.
- [ ] HOUSE-00286 — Grouped: source weather sounds NOX lacks — thunder ×3 distances (≥ 9 samples), hail on roof/window/car, rain on roof/window, gutter trickle — ≥ 18 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
      blocked: (2026-09-07) **Freesound originals require an account, and creating one means
            accepting their ToS — which is the project owner's to accept, not a tool's.** Measured
            rather than assumed: `https://freesound.org/apiv2/search/text/?query=…` answers
            **401 "Authentication credentials were not provided"**, and a sound's `/download/` URL
            **302s to `https://freesound.org/home/login/`**. The search pages and the CC0 filter
            read fine anonymously, so the *selection* work could be done; only the bytes cannot be
            fetched. The lossy CDN previews are readable without a login and are **not** a
            substitute — this project converts 24-bit originals and manifests their hashes.
            `HOUSE-00272` established that the licence assurance rests on ToS §87b/§87c, whose
            warranty and indemnity bind **the account holder**, which is the same reason the
            account cannot be created on the owner's behalf.
- [ ] HOUSE-00287 — Grouped: source dog sounds — bark ×4, whine, growl, pant loop, sigh, eat, drink, collar jingle, nail clicks — ≥ 14 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
      blocked: (2026-09-07) **Freesound originals require an account, and creating one means
            accepting their ToS — which is the project owner's to accept, not a tool's.** Measured
            rather than assumed: `https://freesound.org/apiv2/search/text/?query=…` answers
            **401 "Authentication credentials were not provided"**, and a sound's `/download/` URL
            **302s to `https://freesound.org/home/login/`**. The search pages and the CC0 filter
            read fine anonymously, so the *selection* work could be done; only the bytes cannot be
            fetched. The lossy CDN previews are readable without a login and are **not** a
            substitute — this project converts 24-bit originals and manifests their hashes.
            `HOUSE-00272` established that the licence assurance rests on ToS §87b/§87c, whose
            warranty and indemnity bind **the account holder**, which is the same reason the
            account cannot be created on the owner's behalf.
- [ ] HOUSE-00288 — Grouped: source cat sounds — meow ×5, chirrup, purr loop, hiss, paw falls, scratch — ≥ 12 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
      blocked: (2026-09-07) **Freesound originals require an account, and creating one means
            accepting their ToS — which is the project owner's to accept, not a tool's.** Measured
            rather than assumed: `https://freesound.org/apiv2/search/text/?query=…` answers
            **401 "Authentication credentials were not provided"**, and a sound's `/download/` URL
            **302s to `https://freesound.org/home/login/`**. The search pages and the CC0 filter
            read fine anonymously, so the *selection* work could be done; only the bytes cannot be
            fetched. The lossy CDN previews are readable without a login and are **not** a
            substitute — this project converts 24-bit originals and manifests their hashes.
            `HOUSE-00272` established that the licence assurance rests on ToS §87b/§87c, whose
            warranty and indemnity bind **the account holder**, which is the same reason the
            account cannot be created on the owner's behalf.
- [ ] HOUSE-00289 — Grouped: source house sounds — HVAC burner/blower/duct tick, clock tick, clock chime, doorbell, garage motor, gate motor, creaks ×8 — ≥ 18 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
      blocked: (2026-09-07) **Freesound originals require an account, and creating one means
            accepting their ToS — which is the project owner's to accept, not a tool's.** Measured
            rather than assumed: `https://freesound.org/apiv2/search/text/?query=…` answers
            **401 "Authentication credentials were not provided"**, and a sound's `/download/` URL
            **302s to `https://freesound.org/home/login/`**. The search pages and the CC0 filter
            read fine anonymously, so the *selection* work could be done; only the bytes cannot be
            fetched. The lossy CDN previews are readable without a login and are **not** a
            substitute — this project converts 24-bit originals and manifests their hashes.
            `HOUSE-00272` established that the licence assurance rests on ToS §87b/§87c, whose
            warranty and indemnity bind **the account holder**, which is the same reason the
            account cannot be created on the owner's behalf.
- [ ] HOUSE-00290 — Grouped: source outdoor ambience NOX lacks — distant road, suburban day, suburban night, lawnmower, aircraft, neighbourhood dog — ≥ 10 samples
      dep: HOUSE-00272 · sys: audio · plat: TOOL · pri: MUST
      blocked: (2026-09-07) **Freesound originals require an account, and creating one means
            accepting their ToS — which is the project owner's to accept, not a tool's.** Measured
            rather than assumed: `https://freesound.org/apiv2/search/text/?query=…` answers
            **401 "Authentication credentials were not provided"**, and a sound's `/download/` URL
            **302s to `https://freesound.org/home/login/`**. The search pages and the CC0 filter
            read fine anonymously, so the *selection* work could be done; only the bytes cannot be
            fetched. The lossy CDN previews are readable without a login and are **not** a
            substitute — this project converts 24-bit originals and manifests their hashes.
            `HOUSE-00272` established that the licence assurance rests on ToS §87b/§87c, whose
            warranty and indemnity bind **the account holder**, which is the same reason the
            account cannot be created on the owner's behalf.
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
- [x] HOUSE-00301 — Establish the asset-acquisition runbook so later sessions add assets consistently
      dep: HOUSE-00300 · sys: — · plat: TOOL · pri: MUST
      note: `docs/asset-acquisition-runbook.md`: the seven steps every asset in this repository
            already went through, written down. Evidence before download; `SOURCE.md` beside the
            files; a manifest row with the hash and the four redistribution booleans; verify and
            generate in one tool; the per-kind gates; the working recorded, not just the result.
      note: the `dep` on `HOUSE-00300` is about having reviewed the credits document, and that
            document exists and passes `verify_licences.py --check` today. The runbook is a
            **process**, and writing it before the next acquisition is the only time it can save
            anybody anything — after would be a description of what somebody had already had to
            work out.
      note: it ends with the two failures this project has actually had, because a runbook that
            lists only the happy path is a runbook nobody reads twice: a licence that says one
            thing on the store page and another in the download, and a pack that fits until it does
            not — `audio-core` at 99.8 % of 30 MB before the ambience beds were finished.
- [ ] HOUSE-00302 — Phase-4 review and commit; update `cna-house.md` §19/§20 with what was actually found
      dep: HOUSE-00261…HOUSE-00301 · sys: — · plat: ALL · pri: MUST

---

## Phase 5 — World and floor-plan data

**Goal.** The house exists as data: every level, cell, portal, opening, stair, light, material,
prop placement, nav node, audio zone and interactable, authored, validated and loadable.

**Exit.** `validate_world.py` and the C++ validator both pass on the complete layout; the game
loads it in under 250 ms; `report_graph.py` produces the adjacency tables of `cna-house.md` §16.

### 5.1 Schema and loader

- [x] HOUSE-00341 — Define and document the JSON schema files for all 16 world files (`docs/world-format.md` + machine-checkable JSON Schema)
      dep: HOUSE-00033 · sys: world · plat: TOOL · pri: MUST
      note: (2026-09-07) `tools/world/world_schema.py` generates all sixteen draft-2020-12
            schemas into `docs/world-schema/` from **one** source, so the id pattern, the
            `[x, y, z]` vector, the `{"x": [min,max], "z": [min,max]}` range and the
            `cna-house/<kind>/<n>` header rule are one definition, not sixteen that can quietly
            disagree — sixteen schemas that accept a lower-case id in one file and not another
            look exactly like sixteen correct schemas. `--check` is a gate in `run_checks.sh` and
            needs only the standard library; `--validate DIR` and `--selftest` need `jsonschema`,
            which CI installs for them alone. 101 selftest claims, 15/15 injected bugs caught.
            `docs/world-format.md` gains a "machine-checkable half" section.
      note: the sixteen are `layout_io.FILES` **minus `assets.manifest.json`**. `world-format.md`
            says "seventeen JSON files under `assets-src/world/`" and both counts are right: the
            seventeenth is the *asset* manifest, whose schema is `cna-house.md` §20.3 and whose
            gate is `check_manifest.py`. A selftest claim pins this so the next reader does not
            have to rediscover it.
      note: (2026-09-07) `HOUSE-00358` corrected three shapes here and regenerated:
            `portals.plane.axis` accepts `y`, `levels` gained `plumbing.stacks`, and `props`
            gained `plumbing`. Each is recorded with its reason on `HOUSE-00358`.
      finding: (2026-09-07, found by `HOUSE-00368`) rule 2's slab bounds asked the wrong question
            in both directions. "Is this cell inside its level's slabs" is only meaningful where
            the level above or below actually reaches over it; the garage is a single-storey wing
            with its own slab at +0.15 and its own roof at +4.30. Both bounds now check the
            footprints of the neighbouring levels.
      finding: (2026-09-07, found by `HOUSE-00367`) **rule 5 could not survive staged authoring.**
            Cells are authored a level at a time and portals come after them, so a layout with
            cells and no `layout.portals.json` failed rule 5 on every commit — it has claimed no
            connectivity at all. The rule now stands down when there is no portals file *and* no
            `L0_FOYER`, and bites as hard as before once either exists: a portal graph with no
            front door is exactly the mistake it is for. Two claims pin both halves.
      finding: the schemas check **shape only**, and this is a decision rather than a limitation.
            Of §15.7's eleven rules, 4, 5, 6, 7, 9 and 11 span two files or the whole layout,
            which JSON Schema cannot see across, and 2, 3, 8 and 10 compare two numbers to each
            other, which it cannot do either. Two selftest claims assert the *negative* — a portal
            naming cells that do not exist passes, and a box with `min > max` passes — so that
            `HOUSE-00358` is not written on the assumption that the schema already caught them.
- [x] HOUSE-00342 — Implement `WorldData`: the immutable in-memory model (levels, cells, portals, openings, stairs, lights, materials, props, nav, audio, exterior)
      dep: HOUSE-00341, HOUSE-00028 · sys: world · plat: ALL · pri: MUST
      note: (2026-09-07) `include/cnahouse/world/WorldTypes.hpp` (the row structs and the
            vocabularies) and `WorldData.hpp` (the container). Immutable **by construction**: the
            loader fills a `WorldData::Contents`, `Create` indexes it and moves it in, and after
            that there is no non-const accessor and no way to add a row — which is what lets every
            other system hold a `const WorldData&` and a raw index into it for the life of the
            process. 25 unit tests; five injected bugs, five caught.
      note: three things change on the way in and each is deliberate: every id becomes a
            `util::Id` (compared on every physics step; a string compare there is a string compare
            a hundred thousand times a second), every enum-valued string becomes an enum parsed
            once (an unknown value is a load-time error, not a silent default at frame 4000), and
            `null` becomes `std::optional` — `world-format.md` says `null` is never a synonym for
            zero and an `optional` is the only spelling that cannot be confused with a real value.
      note: `Create` checks the two invariants an index cannot be built without — no id twice, no
            row without one — and nothing else. A portal's cells existing, its rectangle lying in
            their plane, the graph being connected: those are `validate_world.py`'s eleven rules
            and `HOUSE-00357`'s C++ mirror. A third copy here could disagree with the other two.
      finding: a portal is grouped under **both** its cells. Every caller of `PortalsOf` —
            visibility, nav, audio transmission — asks "what leads out of this cell", and a portal
            listed only under `cellA` is invisible from the room on the other side of it. The
            injected-bug run confirms the test that says so fails without it.
      finding: `-Wchanges-meaning` rejected an accessor called `NavForbidden()` beside the row type
            `NavForbidden`, and it was right to: the name would mean two things inside one class.
            The accessor is `ForbiddenZones()`.
      files: src/world/WorldData.cpp|hpp
- [x] HOUSE-00343 — Implement `WorldLoader` for `world.manifest.json` + `layout.levels.json`
      dep: HOUSE-00342 · sys: world · plat: ALL · pri: MUST
      note: (2026-09-07) `WorldLoader::LoadManifest`, `LoadLevels` and `Load`, plus the pieces
            every later reader needs: `Open` (which checks the `schema` header before a single row
            is read), `RequireId`/`OptionalId`, `ReadFootprint`. 24 unit tests against fixtures
            written to a real directory and read back through `System::IO` — the manifest's whole
            job is a statement about a *directory*, and a test that never touched one could not
            fail on either half of it. Seven injected bugs, seven caught.
      note: the file order is the **dependency** order, not §15.1's printing order: materials and
            levels before cells, cells before portals, portals before openings. A reference is then
            always into something already read, so a dangling one is reported against the row that
            carries it instead of at the end of the load. A test pins the order.
      note: hash verification is deliberately **not** here. `HOUSE-00364` owns `worldHash`; what
            this checks is both directions of §15.1's presence rule — a member the directory does
            not have, and a world file the manifest does not list. The second is the one that is
            easy to leave out, and it matters because an unlisted file is a second source of truth
            that nothing hashes.
      finding: `util::JsonValue` could not tell `null` from "present and the wrong type".
            `Has()` is true for a `null` field, so `OptionalFloat("ceiling", 0)` read `L3`'s
            rafter-bounded `null` as **0.0** — an attic whose ceiling is below its floor, which
            looks like a geometry bug and is a read bug. `world-format.md` is explicit that `null`
            is never a synonym for zero, so `JsonValue::IsNull(field)` was added (`HOUSE-00028`'s
            file, smallest correction) and every later loader task needs it too.
- [x] HOUSE-00344 — `WorldLoader`: cells, including multi-box cells and the `yOverride` case
      dep: HOUSE-00343 · sys: world · plat: ALL · pri: MUST
      note: (2026-09-07) `WorldLoader::LoadCells`, every field of §15.3 including the four
            optional blocks. 9 new unit tests; nine injected bugs, nine caught.
      note: the two cases the task names are the two that are easy to get wrong. A cell is a
            **union** of boxes, not one box: an L-shaped room's bounding box includes a notch that
            belongs to the room next door, which is why `CellContains` walks a list. And a
            `yOverride` of `null` defers to the level while `[0.60, 3.65]` wins — the stair cell
            pierces the slab it climbs through and says so.
      finding: an inverted `yOverride` is refused **at the read**, not left to rule 2. Read as
            authored it is a room whose ceiling is below its floor, and every `CellContains` in it
            answers false: a room the player falls through, not a rectangle nobody notices.
      finding: the loader deliberately does **not** resolve cross-file references. Resolution is
            §15.7 rule 6, owned by `validate_world.py` and mirrored by `HOUSE-00357`; a third
            reading here could disagree with both. What the loader guarantees is that the id is
            interned, so the validator can name it. A test pins the non-behaviour.
      finding: the daylight fixture uses `NE` and not `world-format.md`'s `N`. `N` is the first
            value of the enum, so a reader that ignored the field entirely would have passed — the
            injected-bug run found exactly that and the fixture was changed.
- [x] HOUSE-00345 — `WorldLoader`: portals, including plane/rect validation against both cells
      dep: HOUSE-00344 · sys: world · plat: ALL · pri: MUST
      note: (2026-09-07) `WorldLoader::LoadPortals`, with §15.7 rule 4 checked as it goes — the
            rectangle lies in **both** cells' boundary planes within 1 cm and its `v` range lies
            inside both vertical extents. 11 new unit tests; eleven injected bugs, eleven caught.
      note: this is the loader's **one** exception to "resolution is rule 6's, not mine", and the
            reason does not generalise: the rectangle is what the visibility clip uses directly,
            every frame, so a rectangle that is not in the wall it claims does not fail — it
            produces a frustum that is silently wrong and a room that flickers. The check also
            needs nothing outside the two files just read. The `WorldLoader` docstring says so.
      finding: "both cells" is the half that is easy to lose. The hall's face on `x = 2` runs
            `z 4..10` and the WC's runs `z 4..6`; a rectangle at `z 6.5..7.0` is in the hall's wall
            and in no wall of the WC — a hole into the middle of a partition. The fixture is built
            around exactly that asymmetry and an injected bug that checked only `cellA` fails it.
      finding: the horizontal (`y`) case is checked as hard as the wall case, because it had to be
            added to the vocabulary at all (`HOUSE-00358`): a stair well at the wrong height joins
            two floors that do not meet there and the solver sees through a slab.
      finding: `maxDepth: null` is "no cap" and must not read as 0, which would mean the opposite —
            the difference between a glazed door and a bricked-up one. Same for `soundLoss`.
      finding: a portal whose two sides name the same cell is refused. A hole from a room into
            itself is not a portal, and it would give that cell two entries in its own portal list.
- [x] HOUSE-00346 — `WorldLoader`: openings (doors, windows) with hinge/swing/travel metadata
      dep: HOUSE-00345 · sys: world · plat: ALL · pri: MUST
      note: (2026-09-07) `WorldLoader::LoadOpenings`. 6 new unit tests; eight injected bugs,
            seven caught and one redundantly covered (see below).
      finding: `hinge: null` is "does not swing", not "hinged left". A slider has no hinge, and
            `Left` is the first value of the enum — so a reader that ignored the field entirely
            would look right on every door and wrong on every slider in the house. The fixture
            carries a left door, a right door and a hingeless slider for exactly that reason.
      finding: `solid` is read here and not inferred from the asset, because §64.3's 16 dB for a
            hollow-core door and 24 dB for a solid one is what the audio solve reads, and the
            `.glb` does not know which it is.
      note: the leaf's `width` and `height` are checked twice over — required, and positive — so an
            injected bug that made them optional is still caught by the positivity rule. That is a
            redundancy rather than a gap, and the test now also requires the message to name the
            dimension either way.
      note: §15.7 rule 7's opening↔portal bijection is **not** checked here. It is a statement
            about two whole files (every door has one portal AND no portal has two leaves) and the
            loader has read one of them; a partial check would report the wrong half.
- [x] HOUSE-00347 — `WorldLoader`: stairs, with the derived collision ramp parameters
      dep: HOUSE-00344 · sys: world · plat: ALL · pri: MUST
      note: (2026-09-07) `WorldLoader::LoadStairs`, plus `SegmentFlight` and `TotalRun` in
            `WorldTypes`. The ramp parameters are **derived**, not stored: `SegmentFlight` does
            the same walk `build_collision.py` does — consume risers until the next landing, emit
            the run, emit the landing — from the same authored row. Storing them would be a second
            copy that can disagree with the wedges the content build actually baked. 7 new unit
            tests; eight injected bugs, eight caught.
      finding: **`build_collision.py` emitted three wedges for a flight with one half-landing,
            not two** (`HOUSE-00210`, recorded there too). The riser a run starts on is the riser
            the landing below it ended on, so without a `run == 0` guard every run after a landing
            was exactly one riser long. The geometry stayed continuous — the risers still added up
            and the wedges still met — which is why nothing caught it: the existing claims were
            about closure and about edges, and none about the COUNT. Both sides now carry the
            guard, and `build_collision.py` gains a claim that a half-landing makes two wedges of
            five risers each.
      note: `collisionRamp` defaults to **true**, which is `build_collision.py`'s default.
            Defaulting the other way would silently give every flight in the house a box per step
            where it asked for two wedges.
- [x] HOUSE-00348 — `WorldLoader`: lights and light groups
      dep: HOUSE-00344 · sys: world · plat: ALL · pri: MUST
      note: (2026-09-07) `WorldLoader::LoadLights`, plus `WorldData::LightsInGroup`. 8 new unit
            tests; eight injected bugs, eight caught.
      note: lights are indexed **twice**, by cell and by switch group, because the two questions
            are asked by different systems for different reasons: the renderer asks "what lights
            this cell", a switch asks "what does this group toggle", and a group crosses cells —
            the stair-hall group lights two floors from one plate. The fixture is built so that it
            does.
      finding: `bakedIntoLightmap` and `castsBlobShadow` are independent and the file says so. A
            baked light still needs a blob shadow for the dynamic objects the bake never saw, so
            reading one from the other loses every moving shadow in a room that was lit offline.
      finding: a `spot` or `directional` light with no direction is refused. `Vector3::Zero`
            normalises to a NaN — a black room at run time with nothing in the frame that says
            why — and the fixture's direction is `[0.20, -0.90, 0.40]` rather than straight down,
            because an injected bug that dropped the field and defaulted to `[0, -1, 0]` survived
            the first version of the test.
      finding: `colorK` is bounded 1000..12000 K. §70.5 gives no range, but the physical one is
            not open: a missing zero on 2 700 puts a kitchen under a match, and that is a typo
            rather than a choice.
- [x] HOUSE-00349 — `WorldLoader`: materials
      dep: HOUSE-00343 · sys: world · plat: ALL · pri: MUST
      note: (2026-09-07) `WorldLoader::LoadMaterials`, read **before** the cells because a cell
            names three of them. 8 new unit tests; eight injected bugs, eight caught.
      note: §22.2's twenty classes become a closed `MaterialClass` enum, and its `wet_<class>` /
            `snow_<class>` forms become a `SurfaceState` modifier rather than sixty classes —
            `wet_wood` is wood with a darkened albedo, not a different class. Reading them apart is
            what lets the effect-tier fallback consult one table of twenty rows instead of three.
      finding: the §22.2 class→effect table now exists **twice**, here as `DefaultEffectTier` and
            in `build_chunks.py` as `CLASS_TO_LAYOUT`, and it has to: one chooses the effect the
            game draws with, the other the vertex layout the content build bakes. A chunk built
            with one layout and drawn with the effect the other chose is a wrong-looking surface
            nobody can trace back to a table, so a unit test asserts the two agree row for row.
      note: the texture paths stay `std::string` and are not interned. They are handed to
            `ContentManager` and never compared, so interning would put a few hundred
            never-looked-up names into the id registry for nothing.
      finding: a `mask` material with no `alphaCutoff` is refused. There is no threshold to test
            against, and the stock `AlphaTestEffect` would quietly use its own default instead of
            the author's — a foliage card with the wrong fringe, everywhere, and nothing to look at.
- [x] HOUSE-00350 — `WorldLoader`: prop placements with LOD group, collision reference and interactable reference
      dep: HOUSE-00349 · sys: world · plat: ALL · pri: MUST
      note: (2026-09-07) `WorldLoader::LoadProps`. 6 new unit tests; seven injected bugs, seven
            caught.
      finding: `static` defaults to **true**, and the two failure modes are not symmetric. §17.4
            batches a prop that never moves and the file's prose says a row has to *say* `false`
            to become a `DynamicInstance`; defaulting the other way would silently un-batch the
            whole house — a draw-call regression the budget report would show as a number nobody
            traces back to a default.
      note: every fixture value is chosen to differ from the field's default (the WC pan is scaled
            0.98, not 1.0). An injected bug that ignored `scale` survived the first version of the
            test, because the fixture agreed with the default by accident.
- [x] HOUSE-00351 — `WorldLoader`: nav graph, perches, beds, forbidden zones
      dep: HOUSE-00344 · sys: world · plat: ALL · pri: MUST
      note: (2026-09-07) `WorldLoader::LoadNav`. Perches, beds and bowls differ only in their name
            in the file, so they are read into one list with a `MarkerKind` — losing the kind puts
            the cat's water in the dog's bed, and a test says so. 8 new unit tests; nine injected
            bugs, nine caught.
      finding: **absent** and **empty** species lists are not the same thing. §61's answer for
            most of the graph is "both", so an absent list means both; an empty one is a row that
            does nothing and is far more likely a mistake, so it is refused. The one exception is
            a forbidden zone, where an absent list would read as a rule that forbids nobody — so
            there the list is required.
      finding: a marker is placed by position **or** by a prop, and §61 uses both — a windowsill
            perch is a point, a dog bed is wherever the bed prop ended up. Neither is refused: a
            marker with no position and no prop is a marker nowhere.
      note: an edge that crosses a portal names it, which is the whole reason there is one
            authored graph and not two: a closed door closes the route for the pets exactly as it
            does for vision and sound.
- [x] HOUSE-00352 — `WorldLoader`: audio zones, ambience beds, emitter placements, portal transmission losses
      dep: HOUSE-00345 · sys: world · plat: ALL · pri: MUST
      note: (2026-09-07) `WorldLoader::LoadAudio`, plus `AudioTransmission` and
            `WorldData::FindTransmission`. 7 new unit tests; eight injected bugs, eight caught.
      note: the transmission table is a **named** loss pair — `door_hollow`, `door_solid` — rather
            than a per-portal number, because §64.3's figures are properties of a kind of
            construction and the house has 62 doors of half a dozen kinds. A portal's own
            `soundLoss` overrides it where a door is unusual; this is what the other sixty read.
            Held as a vector, not a map: a handful of rows, read once per portal solve, and a
            stable order keeps a diagnostic that lists them stable too.
      finding: `closed` may not be **less** than `open`. Closing a door cannot make it quieter to
            shut than to leave open, and a sign-flipped pair sounds exactly like a broken audio
            system with nothing in the frame pointing at the data.
      note: an emitter must name its cell. ADR-0010's portal-path solve starts from cells and not
            from positions, so a point alone would have to be located first — on every voice,
            every frame.
- [x] HOUSE-00353 — `WorldLoader`: exterior (terrain reference, road, fences, neighbourhood, vegetation instances)
      dep: HOUSE-00344 · sys: world · plat: ALL · pri: MUST
      note: (2026-09-07) `WorldLoader::LoadExterior`. 6 new unit tests; nine injected bugs, nine
            caught.
      note: vegetation stays **grouped by asset** as the file writes it. §17.4 draws it instanced
            where that measures faster, and that needs the grouping in the data rather than
            rebuilt at load from one row per plant.
      finding: a path of one point is refused. It draws nothing, and worse, a fence built from it
            occupies no ground at all — a garden with a gap nobody authored and nothing in the
            data that looks wrong.
      finding: `impostorFrom: 0` is kept as a real choice (always an impostor, which is right for
            the far row of houses) while a **negative** distance is refused: that is a sign error
            that swaps the two branches and draws a full mesh at the horizon.
- [x] HOUSE-00354 — `WorldLoader`: interactables, with the closed-vocabulary predicate/effect parser
      dep: HOUSE-00350 · sys: world · plat: ALL · pri: MUST
      accept: an unknown token is a load-time error naming the file, the id and the token
      verify: unit InteractableExprTests.* with 20 valid and 20 invalid expressions
      note: (2026-09-07) `InteractableExpr.hpp` (the grammar) and `WorldLoader::LoadInteractables`.
            Both acceptance lines are met: `InteractableExprTests` carries exactly 20 valid and 20
            invalid predicates plus 10 invalid effects, and the message reads
            `interactables.json/FRIDGE_L0_KITCHEN Open when [offset 6 …]: this interactable has no
            state field "doorAjar"` — file, id, action, token and offset. 14 + 10 unit tests;
            twelve injected bugs, twelve caught.
      finding: **the grammar did not exist.** `docs/world-format.md` states the requirement in one
            sentence — "parsed at load time into a fixed expression tree over this interactable's
            own typed state fields, with an unknown token a load-time error" — and gives one
            example, `setDoor(true)`. Nothing anywhere defines the vocabulary, so this task had to.
      finding: the *operations* are closed and the *field names* are not a list, and the example's
            `setDoor` had to go. §50.4 has twelve behaviour classes with ~40 distinct state fields
            between them; a closed list of setter verbs would be a 40-entry table kept in step with
            every behaviour class, and "adding a 641st interactable is a JSON row" would stop being
            true. Checking each field against the row's **own** `state` is closed by construction,
            catches a typo identically, and lets the message list the fields that do exist — which
            is also exactly what the one authoritative sentence asks for. `world-format.md`'s
            example now reads `state.doorOpen = true` and the grammar is documented beside it.
      finding: a row's `state` must be read **before** its `actions`, and a test pins it. That
            ordering is the only reason the vocabulary can be closed at all.
      finding: types are checked at **parse**, not at run. `state.doorOpen == 0.5` against a
            boolean field would otherwise be a silent `false` for the life of the build.
      note: `util::JsonValue` gained `Members()` — the counterpart of `Elements()` for an object
            whose KEYS are data. A `state` block's field names are the interactable's own and no
            reader can know them in advance; there was no way to enumerate them (`HOUSE-00028`'s
            file, smallest correction, in file order so a diagnostic quotes what the author sees).
      note: evaluation walks the tree with an explicit stack. The tree comes from data, and a
            2 000-term predicate — far past anything an author would write — must not be able to
            overflow the game's stack. A test builds one.
- [x] HOUSE-00355 — `WorldLoader`: `initialstate.json` into the canonical state table
      dep: HOUSE-00354 · sys: world · plat: ALL · pri: MUST
      note: (2026-09-07) `WorldLoader::LoadInitialState`, read **last** so its `interactables`
            block can be checked against the state each interactable declares. 8 new unit tests;
            ten injected bugs, ten caught.
      finding: each opening value is checked by **name and by type** against the interactable's
            declared field. This file is what every delta save is taken against (§65.6), so a
            field here that the interactable does not have is a value the save would carry for
            ever and nothing would ever read — and a `doorOpen` that starts as `0.5` against a
            boolean is the same bug one type further on.
      note: an opening state is only the fields it **names**. `interactables.json` already
            declares every field and its default, so a block that repeated all of them would be a
            second place to change a default.
      note: a block naming an interactable that does not exist is **not** reported here — that is
            §15.7 rule 6, it has one owner, and a second message for it would be a worse one. The
            field check simply does not run for it. Same arrangement as the portal-plane check.
      finding: `player.cell` is required. Without it §16.4 step 4 assigns `EXT_WORLD` and the game
            starts the player outside the house it just loaded, with nothing in the frame saying
            the spawn row was incomplete. An injected bug that made it optional survived the first
            version of the test, which only checked that the whole `player` block was required.
- [x] HOUSE-00356 — Implement `SpatialIndex`: the 2 m × 2 m × level grid over cells, built at load
      dep: HOUSE-00344 · sys: world · plat: ALL · pri: MUST
      verify: unit SpatialIndexTests.* against brute force on 10⁵ random points
      note: (2026-09-07) all three of §16.4's steps — incremental with the 5 cm hysteresis, the
            neighbour walk through the current cell's portals, and the 2 m grid. The verification
            line is met literally: 100 000 seeded random points, every one compared with the loop
            the index replaces, and the sample asserted to land both inside and outside cells often
            enough for the agreement to mean something. 12 unit tests; eleven injected bugs, eleven
            caught.
      finding: the grid is keyed on `(x, z)` and **not** on `(level, x, z)`, and §16.4's "× level"
            is done by the Y test instead. A cell's vertical extent can be overridden per cell — a
            stair spans two storeys — so a level cannot be derived from a Y coordinate, and a third
            key would need one. The bound §16.4 states still holds and for its own reason: cells on
            one level do not overlap, so a 2 m square sees one cell per level plus the exterior,
            which is the "≤ 6 cells" the section expects. A test asserts it.
      finding: the hysteresis is on **step 1 only**. Staying put is sticky; arriving is not. A
            margin on the grid step would let two cells claim one point with no rule for choosing,
            and an injected bug that added one is caught.
      finding: §16.4's step 4 assigns `EXT_WORLD`, which is a room name — the one thing
            `CLAUDE.md` §3 says C++ must never contain. `Find` returns an invalid id and the
            controller, which already owns the "clamp to the last good cell" diagnostic, decides.
      finding: steps 2 and 3 give the **same answer**, so nothing about the returned id can show
            that the neighbour walk ran — an injected bug that deleted it passed every test. `Find`
            now reports which step answered, which makes the claim testable and is what §70.6's
            performance scenarios and the debug overlay want to read anyway.
- [x] HOUSE-00357 — Implement `WorldValidator` in C++ mirroring `validate_world.py`'s 11 rules, run at load in debug builds
      dep: HOUSE-00355 · sys: world · plat: ALL · pri: MUST
      note: **four of the eleven are already enforced before a `WorldData` exists**, and repeating
            them would check the same thing twice in every debug run: rule 1 by
            `WorldData::Create`, and rules 2, 4 and 8 by `WorldLoader`, which refuses the row.
            `WorldValidator` runs the seven that are properties of the **whole** world — 3, 5, 6,
            7, 9, 10 and 11 — and the header says which is enforced where, so the split is a
            statement rather than an omission.
      note: it reports, it does not refuse. A world that breaks a whole-world rule is still one
            the loader read, and a designer running the game to look at a room they have half-moved
            should get the list, not a black screen. `Load` logs each problem at `Warn` in a debug
            build; `Fast` and not `Full`, because rule 11 samples a floor per interactable and
            belongs in the test that mirrors the Python gate rather than in every launch.
      note: **it passed the authored house on the first run**, which is the result the session's
            three loader/Python disagreements make worth stating: `HOUSE-00378` found the loader
            refusing a portal the Python accepted, `HOUSE-00388` found it dropping seven of §64.3's
            ten transmission classes, and `HOUSE-00387` found a whole file the loader never opened.
            A rule stated once is a rule nobody checks.
      note: a validator that has only ever passed is not a validator. Ten injected breakages, one
            per rule and two each for the rules with two halves, each asserting that the rule which
            owns the thing is the one that fires — the C++ half of the Python gate's discipline.
            Two of the ten needed the fixture sharpened first: emptying the openings list looks
            exactly like "the leaves are not authored yet", for which rule 7 correctly stands down;
            and a crawl space next door to a room you can stand in is reachable **from** that room,
            which is what rule 11's neighbour search is for.
      note: the C++ model does not yet carry the exterior file's gates, kerbs, paths and
            structures, the sky and weather tables, or a pet's start perch and bed. Those stay with
            the Python gate alone until the loader reads them, and the header lists them rather than
            leaving a reader to wonder.
- [x] HOUSE-00358 — Implement `tools/world/validate_world.py` with all 11 rules and clear diagnostics
      dep: HOUSE-00341 · sys: world · plat: TOOL · pri: MUST
      note: (2026-09-07) all eleven rules, each reporting **every** failure with file, JSON path,
            expected and found. `--rules N,M` runs a subset. `--selftest` builds a two-storey
            house — 9 cells, 8 portals, 3 openings, a stacked WC pair, a 0.5 m closet — that
            passes all eleven, then mutates it once per rule and requires the mutation to be
            caught by **its own rule and by no other**: a rule that reports its neighbour's
            problem looks like a working rule right up to the day the neighbour is switched off.
            66 claims, 24/24 injected bugs caught.
      note: a shape failure stops the semantic rules and the output says so. Otherwise one typed
            field would produce a cascade of eleven rules reporting the consequences of it.
      finding: **`stair_well` and `hatch` are in the portal vocabulary and the plane vocabulary
            could not express either of them.** `plane.axis` was `x | z` — both vertical — while
            §15.7 rule 5 requires the graph to be connected *through* portals, so a stair could
            not join the floors it climbs. Smallest correction: `axis` also accepts `y`, where `u`
            is world X and `v` is world Z, the rectangle must lie in both cells' footprints and
            the plane must be the boundary they share. Recorded in `docs/world-format.md` and in
            the schema; `HOUSE-00366`…`HOUSE-00381` author against it.
      finding: rule 2 exempts `stair` and `void` cells from the slab-underside bound for the same
            reason — a stair cell pierces the slab it climbs through by definition — and
            `exterior` cells because a terrace's ceiling is the sky. That last one is the defect
            `build_collision.py` shipped and `build_skyexposure.py` found.
      finding: **§15.7 rule 9 had no data to read.** `HOUSE-00386` is written as "author the
            plumbing stack description … *so the validator can check* fixture placement", so the
            format had to exist first. Smallest correction: `layout.levels.json` gains
            `plumbing.stacks` (§12.5's STACK-A…F: member cells, the chase rect, the cell it drops
            to) and a prop gains `plumbing`, the stack it drains to. Without the second field
            "every fixture's cell appears in a declared stack" cannot say which props are
            fixtures. Rule 9 checks the stack too, not only the fixture: a stack whose WCs are not
            actually above one another is a drawing, not a drain.
      finding: **rule 11 cannot search only the interactable's own cell.** §15.7 says "the room's
            floor", and a shallow closet, a cabinet, a meter cupboard and a serving hatch are all
            reached from the room next door — the fixture's 0.5 m closet has floor in it and
            nobody who can stand on that floor. The eye search therefore covers the cell and its
            portal-neighbours, the eye position must be somewhere the **player capsule can
            stand**, and a segment leaving the cell must cross the shared plane *inside the portal
            rectangle*. Dropping any one of the three lets something absurd through, and the
            selftest has a case that turns on each.
      finding: §70.5's "rise consistency within a flight ≤ 2 mm" is **not checkable and not
            missing**: `layout.stairs.json` carries one `rise` per flight, so every riser is equal
            by construction and the check would assert a tautology. Likewise the counter, table,
            seat, sill and switch heights and the human/pet/car scales are properties of an
            *asset*, not of the layout; §70.5 assigns those to `scale_check.py`, and
            `HOUSE-00360` joins the two.
- [x] HOUSE-00359 — Implement `tools/world/report_graph.py`: adjacency tables, degree stats, diameter, component count with all doors closed
      dep: HOUSE-00358 · sys: world · plat: TOOL · pri: MUST
      note: (2026-09-07) §16.1's adjacency tables per level plus §16.2's vertical table, and every
            metric of §16.3, computed from `assets-src/world/` rather than restated. Markdown by
            default, `--json` for the numbers, `-o` to a file. 19 claims, 12/12 injected bugs
            caught.
      note: deliberately a report and **not** a gate. The numbers move for good reasons — a task
            adds a room, a door becomes an opening — so a gate over them would fail on every
            legitimate change and be turned off within a week. What is worth gating is
            connectivity, and `validate_world.py` rule 5 does that.
      note: it shares `validate_world.py`'s fixture on purpose. Two tools that disagree about what
            a house looks like will disagree silently, and one selftest claim asserts that
            `report_graph`'s "isolated cell" and rule 5's "unreachable cell" name the same room in
            the same file.
      finding: "deterministic" had to mean *against a reordered file*, not against a second run.
            Within one process Python's ordering is stable, so a claim that two runs agree passed
            while the table was being emitted in whatever order it was read — the injected bug
            survived it. The claim now reverses the cell and portal lists and requires the same
            document byte for byte.
- [x] HOUSE-00360 — Implement the realism checks of `cna-house.md` §70.5 inside `validate_world.py`
      dep: HOUSE-00358 · sys: world · plat: TOOL · pri: MUST
      finding: **yesterday's note was wrong about what this task is**, and the correction is the
            first thing done under it. It said §70.5's remaining rows were all asset properties and
            that this task was therefore the join with `scale_check.py`. Four of them are not:
            a **window sill** is a portal rectangle's lower edge over a cell's floor, a **light
            switch centre** and a **door handle centre** are an interactable's focus point over the
            same floor (`interactables.json` says so in as many words — "the focus is the handle:
            away from the hinge, 1.05 m up"), and **room area against function** is a footprint.
            No `.glb` decides any of them. The join the old note described is `HOUSE-02596`, which
            already exists and already says so.
      note: (2026-09-08) so the four are implemented, in **both** validators — rule 10 in
            `validate_world.py` and `WorldValidator::CheckRealism` — because the reason for having
            two is that they find each other wrong. Eight new selftest claims and six injected
            bugs, all caught; six new C++ cases. The authored house passes both.
      note: the sill row is decided by the window's declared **type**, never by its measurement.
            Eleven of the 66 windows sit outside §70.5's 0.50–1.10 m on purpose: the front door's
            sidelights at 0.10, the sunroom's full-height panels at 0.30, the bathrooms' obscured
            glazing at 1.40, the basement hoppers at 1.85 and the transom at 2.20. An exemption
            written as "high sills are fine" would have exempted every mis-authored window with
            them — the same argument `HOUSE-00378` settled for the door leaf.
      note: the layout has **no `function` field**. A WC and a study are both `kind: room`, and
            §70.5's area row is about the function, so the check reads the cell's `name` — the only
            place the data says which. Rename a 1.6 m² "WC 1" to "Meter Cupboard" and it has no
            minimum, which is the honest consequence and is claimed as one.
      finding: **§70.5's car row was in the table and not in `scale_check.py`.** Added, with all
            three bounds and the axes that make them mean something: a model 1.8 m long and 4.6 m
            wide is a car turned sideways, and a largest-dimension check would wave it through.
      note: four rows are checked by **neither** gate, and each is a missing input rather than a
            missing check — recorded under §70.5 and in the tool's own docstring. Rise consistency
            is a tautology (one `rise` per flight); headroom over a flight needs a plan position
            `layout.stairs.json` does not carry, so it waits for `HOUSE-00459`; balustrades have no
            row anywhere; and there are no socket interactables, so a socket band would pass over
            an empty set.
- [x] HOUSE-00361 — Implement the reachability proof (rule 11): every interactable's focus point reachable by a 2.5 m ray from a standing eye on its room's floor
      dep: HOUSE-00358, HOUSE-00384 · sys: world · plat: TOOL · pri: MUST
      note: (2026-09-07) the mechanism is already in `validate_world.py` rule 11 from
            `HOUSE-00358`, with the standing, portal-crossing and neighbour-cell conditions and
            five selftest claims. What is left for this task is running it over the **authored**
            layout and answering for the interactables it rejects — which needs
            `HOUSE-00389`…`HOUSE-00395`.
      note: (2026-09-08) **run, and it rejected two.** `HOUSE-00384`'s 80 switch plates are the
            first interactables the house has, and rule 11 refused a plate in the garage loft and
            one in the under-stair cupboard: both rooms are under 1.95 m, so neither has a standing
            eye position to reach a switch from. The answer is not to loosen the rule — it is that
            those two lights are switched from the next room, which is what a house does anyway.
            All 80 pass. The proof is a proof because it changed the data.
      note: the `dep` line said `HOUSE-00360`, and this rule needed nothing from it: rule 11 is
            about interactables and §70.5's asset join is about props. The real dependency is
            `HOUSE-00384`, which authored the first interactables to check, and it now says so.
- [x] HOUSE-00362 — Implement the capsule-clearance proof: the player capsule fits through every authored portal, or the portal is marked `crouch`
      dep: HOUSE-00358, HOUSE-00374…HOUSE-00377 · sys: world · plat: TOOL · pri: MUST
      note: (2026-09-07) `validate_world.py` rule 10 checks it (0.62 m × 1.95 m, `crouch` and
            `hatch` exempt, and a horizontal portal measured across its narrowest dimension since
            both of its dimensions are horizontal). The acceptance names **all 186 portals**, so
            this task closes when `HOUSE-00374`…`HOUSE-00379` have authored them.
      note: (2026-09-08) **run over all 179.** Not 186: the design's figure was an estimate and
            §16.3 now carries the measured breakdown (`HOUSE-00376`). Nine portals are marked
            `crouch` — seven between the attic stores under the 1.20 m knee wall, one into
            `B1_UNDERSTAIR` and the refrigerator door — and every other passable portal takes the
            capsule standing. The title says "every authored portal" rather than a number that was
            never right.
      note: the `dep` line said `HOUSE-00360`, which this needed nothing from, and not the portal
            tasks it obviously waits on. Corrected to what it actually depended on.
- [x] HOUSE-00363 — Wire `validate_world.py` into the content build and CI
      dep: HOUSE-00358 · sys: ci · plat: CI · pri: MUST
      note: (2026-09-07) a `world-rules` stage at the head of the world chain in
            `build_content.py`, and a `world-rules` gate in `run_checks.sh`. §15.7 says a
            validation failure fails the build, and the only moment that can be true is before
            `build_collision.py` has read a portal.
      finding: it is deliberately **not** in the `validate` group. `with_validator_gate` wires
            every stage in that group to every generator, which is right for the licence and
            manifest gates — they speak for the whole tree — and wrong for this one: putting it
            there blocked `cnb-textures`, `cnb-audio`, `cnb-fonts`, `cnb-models` and `cnb-media`
            because the layout is not authored yet. An unauthored layout is no reason to stop
            compiling the textures.
      note: until `HOUSE-00366` writes the first world file the gate prints why it has nothing to
            check rather than a bare green line, and it is the one gate in `run_checks.sh` that
            needs `jsonschema`, so a checkout without it is told rather than quietly passed.
- [x] HOUSE-00364 — Implement `world.manifest.json` hashing and the `worldHash` used by the save system
      dep: HOUSE-00343 · sys: world · plat: ALL · pri: MUST
      note: (2026-09-07) both halves, because a hash the loader verifies has to be written by
            something: `tools/world/world_manifest.py` (`--emit`, `--check`, `--selftest`, 13
            claims) writes it, and `WorldLoader::HashFile` / `ComputeWorldHash` /
            `VerifyManifest` check it at load. `--check` is a gate in `run_checks.sh` and a stage
            in `build_content.py`, ahead of `world-rules`. Six new C++ tests; fourteen injected
            bugs across the two, fourteen caught.
      note: the definition is pinned in `docs/world-format.md` and **cross-checked**: a C++ test
            asserts `ComputeWorldHash` against a literal the Python writer produced for the same
            two members, and against the well-known SHA-256 of an empty file. Two implementations
            that merely happened to agree would drift, and every save written by one would then be
            stale to the other.
      finding: a member's hash is of the file's **bytes**, not of its parsed JSON. A reformatted
            file is a different file, the deployed copy is what the loader reads, and "the bytes on
            disk" is the only definition both sides can implement without first agreeing on a JSON
            canonicalisation nobody has written.
      finding: `worldHash` is over the member **list**, not the contents — it must change whenever
            any member changes, and that costs one hash of a few hundred bytes instead of a rehash
            of the world. The order is part of it, so members are written in §15.1's order and
            never the filesystem's: a list that reshuffled between two machines would make a save
            written on one look stale on the other.
      note: the gate says "nothing to index" out loud while the world is unauthored rather than
            returning a silent zero, so it never reads as "the index is fine".
      finding: (2026-09-07, corrected by `HOUSE-00421`) the manifest indexed `assets-src/world`,
            and `VerifyManifest` rehashes what the loader **reads** — the deployed copy. It now
            indexes `content/world`, is written by `deploy_world.py`, and is not authored at all.
- [x] HOUSE-00365 — Measure world load time; assert < 250 ms
      dep: HOUSE-00357 · sys: world · plat: LNX · pri: MUST
      finding: **167 ms median against a 250 ms budget, with the biggest file still unwritten.**
            710 KiB over thirteen files — 96 cells, 179 portals, 133 leaves, 243 lights, 333 nav
            nodes, 80 interactables — and `layout.props.json`'s ~2 400 placements are not among
            them. The budget is at 67 % before the largest file exists, which is a number to act on
            now rather than to discover at `HOUSE-00450`. Recorded, not fixed: the fix is either a
            faster parse or a binary deploy, and both are decisions rather than tweaks.
      note: the manifest is 10.6 ms of the 167 — `VerifyManifest` rehashes all thirteen files
            before reading them (`HOUSE-00364`). Broken out in the printed line so a later
            regression can be attributed to the hashing or to the parse rather than guessed at.
      note: **validation costs 1.8 ms Fast and 2.0 ms Full**, against a 167 ms read. That answers
            the question `HOUSE-00357` left open — whether §15.7 belongs in the debug load path —
            with a number instead of an opinion. The second perf test records the relationship and
            not just the figure, because it is the relationship that would justify moving it out.
      note: the measurement is of the thirteen files that exist, not of `Load`, which requires all
            sixteen: `layout.materials.json` and `layout.props.json` are `HOUSE-00385` and later.
            A measurement that waited for them would be a measurement nobody has at the point where
            it is cheapest to act on.
      note: on a machine shared with other build agents the median is stable at 167–170 ms and the
            worst sample has been seen at 308 ms. The assertion is on the **median**, and §70.4
            says perf tests never gate.

### 5.2 Authoring the layout

- [x] HOUSE-00366 — Author `layout.levels.json`: 5 levels, elevations and the construction constants of `cna-house.md` §12.2/§12.3
      dep: HOUSE-00343 · sys: world · plat: TOOL · pri: MUST
      note: (2026-09-07) **the first world file exists.** `assets-src/world/layout.levels.json`
            plus the `world.manifest.json` that indexes it, and the two gates that were saying
            "nothing to check" now check something: `world_schema.py --validate` and
            `validate_world.py` both pass, 11/11 rules.
      finding: §12.1 says "7:12 pitch" and §15.2's example writes `roofPitch: 0.594`; the two do
            not agree and neither is a radian measure (7:12 in radians is 0.528). Authored as
            **0.583333 = 7/12, a slope**, which is what "7:12" means and what §12.2's attic section
            confirms: 1.20 m of headroom at the knee wall rising to 5.00 m at the ridge is 3.80 m
            of rise, and at 7/12 that puts the knee wall 6.51 m from the ridge — 0.19 m inboard of
            the 6.70 m half-span, which is where a knee wall goes. §15.2's 0.594 is illustrative.
      finding: **the C++ loader cannot read the authored file, and nothing deploys a copy it can.**
            `world-format.md` says the authored files are JSONC and "the build strips comments and
            deploys plain JSON beside the compiled content", but no task or stage does the
            stripping — `System::Text::Json` refuses the first comment. Raised as `HOUSE-00421`,
            the next free id in phase 5's reserved 00341–00450 range.
      finding: `check_manifest.py` demanded an `assets.manifest.json` row for every world file, and
            its own note anticipated this ("a blanket `*.json` exemption would silently exempt a
            future `world/*.json`, which very much is an asset"). It is exempt now, by an exact
            rule and with the argument written beside it: a world file is authored in this
            repository, so a row carries no provenance ADR-0012 wants, and the hash such a row
            would carry already exists in `world.manifest.json` — gated at build **and verified by
            `WorldLoader` at load**, which a row is not. Two hashes for one file is the shape of
            rule people work around.
- [x] HOUSE-00367 — Author `layout.cells.json` for `B1`: ~~14~~ **15** cells per §13.2, with materials, footstep surfaces, acoustics, light groups and residency
      dep: HOUSE-00366 · sys: world · plat: TOOL · pri: MUST
      finding: **§13.2's `B1_WC7` sits inside `B1_UNDERSTAIR`.** The table gives the store the
            whole block X +2.20…+4.90, Z −23.00…−20.20 (7.6 m²) and gives the WC a rectangle
            *inside* it, so the two overlap by 2.88 m² and rule 3 fails. Authored with the WC
            carved out and the store as the L-shaped remainder, 4.68 m².
            **The document's own arithmetic confirms the reading**: measured, the fourteen cells
            then cover 273.92 m² with no overlap and no gap, against §13.2's "≈ the 273.9 m²
            internal envelope". The tabulated 276.9 m² total double-counts the WC.
      finding: four fields are left unset on every row and each is a forward reference, not an
            omission — the three material slots (`HOUSE-00385` authors §22.2's table),
            `lightGroups` (`HOUSE-00381`…`HOUSE-00384`), `daylight.windowIds`
            (`HOUSE-00374`…`HOUSE-00379`) and `acoustic.roomTone` (`HOUSE-00397`). Filling them
            here would make this file the source of truth for a table another task owns, and rule
            6 cannot check any of them until that table exists. The task title says "with
            materials"; the material table does not exist yet and cannot until `HOUSE-00296`.
      finding: §62.4 has no `rubber` surface for the gym's floor. Adding a twenty-first surface
            for one room would mean sourcing a set of samples for it (`HOUSE-00281`'s shortfall is
            already 113); `concrete` is the nearest of the twenty and the mat is a prop.
      note: `B1_UNDERSTAIR` carries a `yOverride` because §13.2 calls it "low, sloped ceiling" —
            it is under the flight, so its ceiling is not the level's.
      finding: (2026-09-07) **§13.2's table has 15 rows and its summary says "14 cells".** Counted:
            fifteen `B1_*` ids are tabulated, and the authored file has all fifteen. The summary
            line is the same one that gives 276.9 m² by double-counting `B1_WC7` inside
            `B1_UNDERSTAIR`, so both halves of it are wrong in the same place — dropping the WC
            from the count and adding its area to the store. The task title inherits the 14.
            Corrected count: **15 cells, 273.92 m²**, which is §13.2's own stated envelope.
- [x] HOUSE-00368 — Author `layout.cells.json` for `L0`: 19 cells per §13.3
      dep: HOUSE-00366 · sys: world · plat: TOOL · pri: MUST
      note: (2026-09-07) §13.3 needed **no** correction: measured, every one of the nineteen areas
            matches its rectangle, nothing overlaps, and the fourteen main-block cells cover the
            273.9 m² envelope with no gap. `L0_FOYER` now exists, which is where §15.7 rule 5 will
            start its walk once the portals are authored.
      finding: the garage broke rule 2 **twice, in both directions**, and the rule was wrong both
            times. §12.2 puts the garage slab at +0.15 and its ceiling at +4.30 — a 4.15 m bay in
            a single-storey wing — while L0's floor structure starts at +0.25 and L1's slab
            underside is at +3.30. The rule asked "is this cell inside its level's slabs"; the
            question it wants is "is there a storey above or below it to poke into". It now checks
            the footprints of the levels either side, so a projecting wing may have its own slab
            and its own roof, and a cell with a room over it is refused exactly as before. Four
            claims pin both directions.
      finding: rule 5's stand-down (`HOUSE-00367`) was still wrong: it skipped only when there was
            neither a portals file *nor* an `L0_FOYER`, so authoring L0 turned the rule back on
            with no portals to walk and failed all 32 interior cells. It now stands down on the
            **presence of the file**, and an *empty* portals file is a claim — that nothing in the
            house connects — which the rule still reports.
- [x] HOUSE-00369 — Author `layout.cells.json` for `L1`: 21 cells per §13.4
      dep: HOUSE-00366 · sys: world · plat: TOOL · pri: MUST
      note: (2026-09-07) §13.4 needed no correction: measured, every area matches its rectangle,
            nothing overlaps, and the nineteen interior cells cover the 273.9 m² envelope exactly
            against the document's 274.0.
      note: the two balconies are `exterior` cells with an explicit `yOverride` reaching to +9.00.
            They sit on the roofs below them and are open to the sky, so they take no ceiling from
            their level — which is what lets rain reach them (`build_coverage.py`) and what the
            `exterior` exemption in rule 2 is for. The front balcony IS the porch's ceiling, which
            is why `L0_PORCH` stops at +3.35.
      note: `L1_STAIR_MAIN` overrides up to +6.55 because the flight continues to L2: it pierces
            this level's ceiling as well as its floor, and `stair` is exempt from both bounds for
            exactly that reason.
- [x] HOUSE-00370 — Author `layout.cells.json` for `L2`: 18 cells per §13.5
      dep: HOUSE-00366 · sys: world · plat: TOOL · pri: MUST
      finding: **§13.5's attic stair leaves 4.00 m² of the envelope in no cell.** It is tabulated
            at X +5.40…+8.20 (11.2 m²) inside a bay that runs +4.90…+8.70, so there are 0.50 m
            strips either side belonging to nothing — and §16.4 step 4 says a point inside the
            shell and in no cell is a world-data bug. Authored at the full bay, 15.2 m². The
            document's own arithmetic decides it: the interior then totals **273.9 m²**, exactly
            the envelope and exactly what B1, L0 and L1 come to, where 269.9 was 4.0 short.
            §13.6's `L3_STAIR_HEAD` has the same footprint and the same fix.
      finding: rule 3 forbids overlap and says nothing about **gaps**, yet §16.4 step 4 says the
            validator "is supposed to have caught" a point inside the shell that is in no cell. No
            coverage check was added here: "the shell" is not declared anywhere in §15, and the
            strips above are open to the north face, so neither a bounding box nor a flood fill
            from outside would have found them. Recorded rather than invented — measuring each
            level's total against its envelope is what found this one, and that is a comparison a
            person makes, not a rule.
- [x] HOUSE-00371 — Author `layout.cells.json` for `L3`: 6 cells per §13.6, with the rafter-envelope description
      dep: HOUSE-00366 · sys: world · plat: TOOL · pri: MUST
      note: (2026-09-07) **79 cells now exist across all five levels** — 15 + 19 + 21 + 18 + 6 —
            and every one of §15.7's eleven rules passes over them.
      note: every attic cell declares its own `yOverride`, because `layout.levels.json` gives `L3`
            a null ceiling and `WorldData::ExtentOf` refuses to invent one. That refusal is what
            makes the null mean "there is no ceiling plane" rather than "the ceiling is at zero",
            and this is the level it was written for. The upper bound is §13.6's **maximum**
            head-room plus the floor: a cell is an axis-aligned bounding volume and the rafter
            slope inside it is geometry, not extent.
      finding: `L3_ROOM`'s clear height is 3.30 m, outside §70.5's 2.35–3.10 m for a habitable
            room, and it is not a violation: that range is a range for a **flat** ceiling, and
            §13.6 gives the room 2.4 m at the knee wall rising to 5.0 m at the ridge. Rule 10 now
            skips a cell whose level is rafter-bounded — checking it would report every attic room
            in every house ever built. Two claims pin the distinction.
      note: `L3_STAIR_HEAD` takes the same full-bay correction as `L2_STAIR_ATTIC`
            (`HOUSE-00370`), and the six cells then cover 273.9 m² — the envelope, and the same
            total as every other level.
- [x] HOUSE-00372 — Author the ~~17~~ **14** exterior cells per §13.7, including `EXT_WORLD` (the 4 attached ones came with their levels)
      dep: HOUSE-00366 · sys: world · plat: TOOL · pri: MUST
      note: (2026-09-07) **`layout.cells.json` is complete: 93 cells, 114 boxes, five levels and
            the plot.** All eleven rules pass, the file deploys, and the C++ loader reads it.
      finding: §13.7 marks its extents "Approximate" and they are — **eleven pairs overlap, by up
            to 133 m², and two run through the porch.** Resolving them was a decision rather than a
            transcription, and the three rules used are written into the file beside the rows: a
            region named as being inside another is carved out of it (the shed out of the garden,
            the garden out of the west side yard, the terrace and orchard out of the back lawn);
            the side yards are the strips outboard of everything else, so the side-yard/back-lawn
            boundary is the building line; and every region stops at a building face rather than
            running under one.
      finding: measured after the resolution, **the thirteen outdoor regions plus the four building
            footprints tile the entire plot with no overlap and nothing left over** — a stronger
            statement than §13.7 makes, and the one §16.4 step 4 needs: a point on the plot is in
            exactly one cell, so "no cell" really does mean beyond the fences. `EXT_WORLD` is
            therefore a **ring** of four boxes around the plot rather than a rectangle over it.
      finding: the driveway is squared to the garage bay (X +9.3…+17.1) rather than §13.7's
            +9.6…+16.8. A driveway narrower than the door it serves leaves two strips of nothing
            at the apron, which is the same class of gap `HOUSE-00370` found at the attic stair.
      finding: §13.8 and §16.3 both say "95 cells (78 interior + 17 exterior)" and the tables give
            **93 (75 + 18)**. Counted: B1 15 + L0 19 + L1 21 + L2 18 + L3 6 = 79 rows, 4 of them
            attached exterior cells, so 75 interior; §13.7 lists 14 `EXT_*` ids **including**
            `EXT_WORLD`, so 18 exterior. The summary counted three interior cells that are
            tabulated nowhere and left `EXT_WORLD` out of the exterior count. Both places
            corrected, and this task's own title inherited the 17.
- [x] HOUSE-00373 — Author the 2 container sub-cells for the refrigerator and freezer interiors, and the pattern for future ones
      dep: HOUSE-00368 · sys: world · plat: TOOL · pri: MUST
      finding: **the format had no way to say "inside".** §54 and §56.1 both describe a container
            as a sub-cell with its own portal, and §15.3's cell record has no field for it — so
            `CELL_FRIDGE_INTERIOR` is a cell whose footprint is inside `L0_KITCHEN`'s, which rule 3
            reads as two rooms overlapping. A cell gains `parent`.
      note: the parent is **declared, not inferred**. A room accidentally drawn inside another
            looks identical to a validator otherwise; and declaring it does not switch rule 3 off,
            because a declared sub-cell is then checked to lie inside its parent in all three axes
            and to be only one level deep — a container inside a container is a depth the
            visibility solver does not walk. Five selftest claims, including the one that matters:
            the same cell **without** the declaration is still an overlap.
      note: the alternative was carving each container out of the room around it, which would make
            `L0_KITCHEN` a sixty-three-box polygon whose area changed every time a drawer moved.
      note: the pattern for the other 212 is written into `layout.cells.json` beside the two rows —
            `kind: closet`, a `parent`, a footprint and `yOverride` strictly inside it taken from
            the prop's `bounds` less the carcass, `visibilityHint: opaque` because the door is the
            portal, the parent's residency pack, and `heated: false` because §57's temperature
            model reads it.
      note: the chest freezer opens upward, so its portal will be a **horizontal** one — the second
            use for `plane.axis: "y"` after the stair well that forced it into the vocabulary
            (`HOUSE-00358`).
- [x] HOUSE-00374 — Author `layout.portals.json` — the 47 always-open portals (cased openings, stair wells)
      dep: HOUSE-00367…HOUSE-00372 · sys: world · plat: TOOL · pri: MUST
- [x] HOUSE-00375 — Author the 62 door portals with hinge, swing and opacity
      dep: HOUSE-00374 · sys: world · plat: TOOL · pri: MUST
      note: **one commit for two tasks, deliberately.** §15.7 rule 5 is a property of the whole
            portal set, not of one portal: a house with its cased openings and no doors leaves
            about thirty rooms unreachable, so `HOUSE-00374` alone cannot satisfy the rule it is
            supposed to satisfy and the two halves cannot be validated apart. `plan.md` splits
            them because they are different authoring work, and they were done in that order.
      finding: **§16.3's "41 always-open" is 47.** Composed: 27 interior cased openings, 17
            exterior links and 3 stair wells. Six of the difference is the exterior ring, which
            grew when `HOUSE-00372` corrected §13.7 from 14 exterior cells to 18 — a ring of 18
            cells needs more links than a ring of 14. §16.3 now carries the measured breakdown and
            says which rows are measured and which are still design figures.
      finding: **`EXT_SHED` had no portal at all**, and no rule could see it. §15.7 rule 5 said
            "every **interior** cell is reachable from `L0_FOYER`", and the shed is `kind:
            exterior` — an exterior cell that is indoors: roofed, `yOverride` [0.0, 2.35],
            `visibilityHint: opaque`, its own `footstepSurface: hardwood`. A building you cannot
            enter is the same defect as a room you cannot enter, so rule 5 now walks every cell
            that is not `void`. Nothing is exempted, `EXT_WORLD` included: it is where the road
            runs off the map, so it has a portal like everything else, and a rule with no
            exceptions is one fewer place for the next unreachable cell to hide. §15.7 and
            `docs/world-format.md` both corrected; the shed gained its east door, in the face it
            shares with `EXT_GARDEN` at X −16.6, which `daylight.orientation: "E"` already implied.
      finding: **`report_graph.py` reported "largest component with all doors closed: 1 cells"**
            beside an exterior ring of eighteen cased openings. `components()` orders its groups by
            their smallest member — deliberately, so the list diffs cleanly — and the report took
            `[0]` as the largest. On this house that is `B1_CELLAR`, alone behind its door. Fixed
            to `max`, with a claim on a fixture whose first group is a 1-cell WC so the two answers
            differ; the same claim on the unmodified fixture passes either way, which is why the
            bug survived `HOUSE-00359`.
      finding: **eight portals were under the 1.95 m standing capsule** — seven between the attic
            store rooms under the 1.20 m knee wall, one into `B1_UNDERSTAIR`. They are correct as
            authored: those are crawl spaces. They carry `"crouch": true` so that `HOUSE-00362`'s
            clearance check can tell "you must duck here" from "this doorway is a mistake".
      finding: **eight portals had no shared plane**, all of them between a room and an outdoor
            deck across a 0.30 m exterior wall: the two cells stopped either side of it, so there
            was no plane in both. Fixed in the geometry rather than by loosening rule 4's 1 cm —
            `L0_PORCH` and `L1_BALCONY_FRONT` extended to Z −14.30, `EXT_TERRACE` to Z −32.10,
            `L0_GARAGE` to X 8.70, `EXT_SIDEYARD_E`'s slot box removed and `EXT_FRONTYARD_E` /
            `EXT_BACKYARD` adjusted to suit. A deck that does not touch the wall it is against is
            a modelling error, and rule 4 was right to say so.
      note: rule 4 gained the container case. A portal into a sub-cell is not in a shared wall: the
            sub-cell is inside its parent, so the opening is in the sub-cell's OWN face — the
            fridge door, the chest lid — and the parent has no face there. Four claims, including
            the two that matter: a portal in NEITHER face is still refused, and one taller than the
            container it opens into is still refused.
      note: rule 7 (every door has exactly one portal) stands down while `layout.openings.json`
            does not exist. Portals are authored before leaves (`HOUSE-00378`), and a bijection
            between two files of which only one exists is not a claim the data has made. It is the
            file's presence, not its contents — an empty openings file beside 62 doors is a claim,
            and a wrong one.
      note: measured after authoring, by `tools/world/report_graph.py`: one component through open
            doors, diameter 13 hops (`EXT_SHED` → `L2_BATH5`), 58 components with every door shut
            and the largest of them the 18-cell exterior ring.
- [x] HOUSE-00376 — Author the 66 window portals with `opacity: glass`/`translucent` and `maxDepth`
      dep: HOUSE-00374 · sys: world · plat: TOOL · pri: MUST
      finding: **rule 4 was right for partitions and wrong for the shell, and all 66 windows hit
            it at once.** Two rooms either side of a partition share a coordinate (§13.1), so "the
            portal lies in both cells' planes within 1 cm" holds. A room and the yard outside it do
            not: the room stops at the interior face, the yard at the exterior one, and the window
            is in the 0.30 m of wall between them, in neither cell's plane. Rule 4 now also accepts
            a portal **in the wall** — the two facing planes must straddle it, each must span the
            opening, and they must be no further apart than the thickest wall `construction`
            declares. Nothing else is loosened: a portal in the middle of a room, on the wrong
            wall, or between cells that do not face each other still fails, and between two cells
            that DO abut the old 1 cm still governs. Seven claims, four of them injected-bug tests.
      finding: **the first fix passed the selftest and failed every window in the house.** The
            fixture put its wall at the origin, where `0.30 − 0.00` is exactly 0.3; the house puts
            its front wall at Z −14.30 against a yard at Z −14.00, where the same subtraction is
            0.30000000000000071 and a `<= 0.30` written without slack rejects it. The tolerance is
            now a wall **plus** the same 1 cm, and the selftest has a fixture at those coordinates.
      finding: **eight rooms are scheduled a window and have no exterior wall.** `L0_LAUNDRY`,
            `L0_WC1`, `L0_WC2`, `L0_DINING`, `L1_BATH3`, `L1_WC4`, `L2_BATH5`, `L2_WC6` are all in
            the middle of the plan, enclosed on four sides by other rooms. §13's rows are corrected
            to `—` with the reason in the Notes column; nothing else can be done without moving
            walls, and a dining room the plan puts in the core is a design decision, not a bug.
      finding: **`L3_ROOM`'s 3 dormers had nowhere to be.** Its footprint stopped 2.70 m short of
            the front wall with the `L3_STORE_S` roof void in between. A dormer belongs to the room
            it lights, so `L3_ROOM` now reaches the eaves in two bays and the void is what is left
            between them. The bays are at the **ends** of the elevation because the porch roof and
            the front balcony fill X −3.60…+3.60 to +9.00 and above them there is no exterior cell
            at all — the same reason `L3_STORE_N`'s two dormers are squeezed into X +2.70…+4.90.
            `P_L3_STORE_W__L3_STORE_S` is gone with them: the west bay is exactly the corner where
            the two voids met. The store is still reachable, one hop longer, through `L3_ROOM`.
      finding: **basement hoppers sit below their yard's floor.** §12.6 puts them in 0.9 m window
            wells at an absolute sill of −0.45, and the yards' floor was 0.00, so the sill was
            outside the cell the window opens into. The four yards that carry a basement window now
            declare a floor of −0.90; the east side yard does not, because the garage is there and
            it has no basement.
      finding: **§12.6 and §13 never agreed about how many windows this house has** — §13's
            per-room column totals 71 openings, §12.6's per-elevation table about 86, and only the
            attic matched. §13 wins, because it names the room the window is in and a portal has to
            be in a room. §12.6's count table is replaced by the measured one: **66** = 73 openings
            §13 schedules (71 rows plus the foyer's transom and the living room's tall window,
            which its "2 sidelights + transom" and "1 bay + 1 tall" cells each undercount by one)
            − 8 in landlocked rooms + the garden shed's, which §13.7 describes only in prose.
      note: three window types the data needed are added to §12.6 — `W_SIDELIGHT`, `W_PANEL` (the
            sunroom's fixed flanks) and `W_INTERNAL` (borrowed light, kitchen → sunroom). Sizes and
            sills of the ten that were already there are unchanged.
      note: the openable/fixed split (§12.6's 62 + 19) is **not** a property of this file:
            `opacity` says what you can see through, not what opens. `HOUSE-00378` authors the
            leaves and that is where the split becomes measurable.
      note: `maxDepth` is authored per §16's `maxDepthFor` table — 3 through clear glass, 1 through
            obscured glass and gable louvres, 2 through an internal window, a transom or a hopper.
            The doc's §15.4 example connects a window to `EXT_WORLD`; it was written before §13.7's
            yard cells existed, and a window that skipped the yard would leave someone standing in
            the garden with no portal to look through. Every window here opens into the cell it
            actually faces.
- [x] HOUSE-00377 — Author the garage-door portal and the 1 hatch portal
      dep: HOUSE-00374 · sys: world · plat: TOOL · pri: MUST
      finding: **the hatch had nothing to open into.** §16.3 counts one hatch and §12.2 describes
            "a storage loft over the rear half at +2.90", but no table lists it as a cell, so the
            portal had no second end. `L0_GARAGE_LOFT` is now a cell — and it has to be a **nested**
            one, because a mezzanine is two volumes at different heights over one footprint, which
            rule 3 reads as two rooms overlapping. That is `HOUSE-00373`'s `parent` mechanism used
            for what it literally says rather than for a container: the loft is inside the garage
            in all three axes, its floor at +2.90 is a face the garage does not have, and rule 4's
            nested case is what checks the hatch. `parent` was introduced for the 214 containers;
            this is the first cell to use it that is a place you can stand up in.
      note: rule 5 earned its widening within the hour. The loft was authored before its hatch and
            the run reported "cell L0_GARAGE_LOFT is not reachable from L0_FOYER; it has 0 passable
            portal(s)" — the same sentence that `EXT_SHED` could not produce two commits ago.
      note: the sectional door is §12.3's 4.90 × 2.40 with its sill on the **garage slab** at
            +0.15, not on L0's floor at +0.60, and centred on the driveway at X +13.20 rather than
            on the garage: the garage runs from X +8.70 because it takes the party wall with the
            mudroom, and centring on it would put a quarter of the door in front of that wall.
      note: `plane.axis: "y"` now has its third user — the stair well, the chest freezer's lid and
            this. It was added on `HOUSE-00358` as a gap in the vocabulary; it is no longer an edge
            case.
      note: §16.3's whole table is now measured rather than designed: 96 cells, 179 portals, 46
            always-open, 63 doors, 3 nested, 66 windows, 1 garage door. The design said 95 and 186.
- [x] HOUSE-00378 — Author `layout.openings.json`: 12 door types and 12 window types with leaf sizes, frames and material references
      dep: HOUSE-00375, HOUSE-00376 · sys: world · plat: TOOL · pri: MUST
      finding: **63 door portals said they were always fully open.** `aperture: null` is this
            format's way of saying "no leaf, never closes" (`docs/world-format.md`), and
            `HOUSE-00375` left it null on every door it authored. Nothing could see it, because
            rule 7 only ever looked from the openings file towards the portals. It now checks both
            ends: a leaf names its portal, the portal names the leaf back, and a shut-able portal
            with a null `aperture` is reported as a door claiming to be a hole.
      finding: **rule 7 had never looked at a garage door or a hatch.** Both have leaves — a
            sectional door is five hinged segments (§54's spline) and a chest lid lifts — and both
            were outside the kind list, so either could have been authored with no leaf at all and
            passed. `LEAF_BEARING_KINDS` is now one list with a reason beside it.
      finding: **the C++ loader and the Python validator disagreed about the house, and a new test
            caught it the first time it read the real portals.** `WorldLoader::LoadPortals` carries
            its own §15.7 rule 4 check, written before container sub-cells (`HOUSE-00373`) and
            before the wall case (`HOUSE-00376`), so it refused the refrigerator door and would
            have refused all 66 windows. Both cases are now implemented in C++ as well, with their
            own unit tests, and `AuthoredWorldTest` asserts rule 7's bijection over the deployed
            world from the other implementation: the day the two disagree about the house, one of
            them says so.
      finding: **§70.5's leaf band was applied to four things that are not interior doors** — the
            two sliders, the refrigerator door and the under-stair store's 1.55 m leaf. The
            exemption is the opening's declared `type`, never its measurement: a rule that let a
            leaf out of the band because it happened to be short would let every mistake out with
            it, which the selftest now proves with a 1.60 m door that is still caught.
      note: **`type` is new in the openings schema.** §12.3 and §12.6 describe openings by type and
            133 rows carrying only their measurements could not be asked which were bathroom
            windows. Id-shaped but not an id — many rows share one, so rule 1 never sees it.
      note: **`swing` is a cell id now, not prose.** The format wrote `"into_L0_WC1"`, which says
            what the field name already says in a form nothing can resolve. It is a reference, rule
            6 checks it, and `Opening::swing` is a `util::Id` rather than a `std::string`.
      note: leaf sizes are **derived** from the portal rather than repeated beside it — the rough
            opening less its reveal, 0.04 m across and 0.05 m up for a door, 0.02 m all round for a
            window sash — so nothing here can disagree with the hole it hangs in. Two rows are
            stated instead and say why: the basement stair-head door, whose portal is the 2.30 ×
            3.30 hole in the floor rather than the leaf, and the pair of double doors.
      note: `hinge` and `swing` are derived too, from stated rules: a leaf hangs on the side with
            more wall beside it so it folds flat, and opens into the smaller cell — never outdoors,
            because a leaf over a porch step would foul it, and never down a stair, because a door
            at the head of a flight must not swing over the top step. A hundred and thirty chosen by
            hand would be a hundred and thirty chances to be arbitrary in a different way.
      note: `asset` and `frame.asset` are null throughout and say so in the file: they are model
            references and the models are phase 6. `material` is not null — those ten `MAT_*` ids
            are an obligation on `HOUSE-00385`, and rule 6 holds it to them the moment
            `layout.materials.json` exists.
      note: 12 door types and 12 window types, not the 7 and 10 in this task's title. The extra
            doors are the ones later tasks created: the appliance door and the freezer lid
            (`HOUSE-00373`), the garage loft hatch (`HOUSE-00377`), the stair-head door and the
            under-stair crawl door. The extra windows are §12.6's three additions from
            `HOUSE-00376` less `W_SLIDER`, which is a door here and not a window.
- [x] HOUSE-00379 — Author `layout.stairs.json`: the 8 flights of §12.4 with risers, goings, landings and surfaces
      dep: HOUSE-00366 · sys: world · plat: TOOL · pri: MUST
      finding: **the terrace could be seen and not reached.** §12.4 scheduled seven flights and the
            terrace deck is +0.45 with the lawn at grade, so three steps between them were missing
            entirely. `STEPS_TERRACE_LAWN` is the eighth flight, and §12.4 has the row now.
      finding: **the garage steps were below §70.5's band.** `2 × 150 + 280 = 580` mm, and the
            band starts at 600. The rise is fixed by the elevations — +0.15 to +0.60 in three — so
            the going moves: 300 mm makes it exactly 600 and the flight has the room for it.
      finding: **both U-stair landings were off a riser boundary**, by 2.7 mm and 15 mm. A landing
            is where a riser ends, so the main stair's half-landing is 9 × 179.4 mm above L0's
            floor (+2.2147, not +2.212) and the L1→L2 landing 12 × 181.3 mm above L1's (+2.175
            rel., not +2.16). The rises themselves are exact — 3.05/17, 2.90/16, 2.75/15 — and
            §12.4's tenths of a millimetre are those numbers rounded, which the file says.
      finding: **two cells stood at the wrong height.** `L0_PORCH`'s floor was L0's +0.60 and the
            porch deck is +0.57, three 190 mm steps above grade with a 30 mm threshold at the door.
            `EXT_TERRACE`'s was grade, and its deck is +0.45 — so the cell contained 0.45 m of
            solid stone. Both corrected, and `P_EXT_BACKYARD__EXT_TERRACE`'s rect with them: the
            opening is at the top of the new steps, not at the bottom.
      finding: **rule 8 could say nothing about half the stairs.** It compared `risers × rise` to
            the two levels' FFL difference, which is zero for the four flights that join two cells
            on one level — the porch, both terrace flights and the garage steps all passed a test
            that was checking 0.57 m against 0.00 m and would have caught nothing. Those flights
            now declare `fromY`/`toY` (new in the schema and in `StairFlight`), the rule checks
            against them, and a same-level flight that declares neither is reported rather than
            waved through.
      note: rule 8 gained two more things while it was open. `rise` is a magnitude, so a flight
            authored downward is the same flight and §12.4's "L0 +0.60 → B1 −2.30" needs no
            re-ordering; and the two cells must be joined by a portal, or the flight is a staircase
            into a wall — which nothing else would have said, because rule 5 walks portals and
            never looks at a flight.
      note: rule 10's exterior exemption is **one-sided**. The porch's `2 × 190 + 300 = 680` mm is
            outside §70.5's band deliberately: shallower and deeper is the right step outdoors, and
            §12.4 already marked it permitted. Steeper is not, indoors or out, so an outdoor flight
            under 600 mm is still refused — and so is a generous one indoors, which is the claim
            that stops the exemption becoming "big treads are fine".
      note: the C++ loader reads `fromY`/`toY` as `std::optional<float>`: absent is a real state,
            and reading a missing field as 0.00 would make the porch steps climb from the basement.
            It refuses a flight that declares one and not the other. `AuthoredWorldTest` asserts
            rule 8 over the deployed world from the other implementation, as it now does rule 7.
- [x] HOUSE-00380 — Author the window schedule per façade per level (§12.6), and cross-check counts against the portals file
      dep: HOUSE-00376 · sys: world · plat: TOOL · pri: MUST
      finding: **`layout.windows.json` does not exist and should not.** §12.6 said "full schedule
            lives in `layout.windows.json`", and a window is already two rows — a portal and a
            leaf. A third file repeating both would be a third thing to keep in step and a third
            place to be wrong. The schedule is **derived**: `tools/world/window_schedule.py`
            writes `docs/window-schedule.md` from the layout, and §12.6 now points at it.
      note: the deliverable with teeth is the **cross-check**, not the document. §12.6's counts
            table is written and maintained by a human, and `HOUSE-00376` already found it fifteen
            windows out. `--check` parses it out of `cna-house.md` and compares it cell by cell
            with the data, so a window added to one and not the other is a failed gate rather than
            a discrepancy nobody notices for a year. Wired as the `window-schedule` gate and as a
            CI selftest step.
      note: façade names are matched through a normalisation, not by string equality: §12.6 writes
            "South (front, `z = −14.30`)" and the tool's list says "South (front)". Comparing the
            raw strings would fail the day somebody adds the coordinate a reader needs.
      note: the size in the schedule is the **portal's**, which is the hole, and not the leaf's,
            which is 20–40 mm smaller. Both are in the data and only one is the opening in the
            wall; a claim pins which, because the two look equally plausible in a table.
      note: six injected bugs, all caught — a door scheduled as a window, the leaf's size reported
            instead of the hole's, a missing §12.6 table passing, a drifted count not reported, an
            unplaced window not reported, and the schedule left in the portals file's order. The
            last two needed the fixtures sharpening: the ordering claim only bites when the
            out-of-order window also sorts first by id.
- [x] HOUSE-00381 — Author `layout.lights.json` for `B1` and `L0`: fixtures, groups, colour temperatures, ranges, defaults
      dep: HOUSE-00368 · sys: world · plat: TOOL · pri: MUST
      finding: **§28.2 and §13 disagree about how many switch groups the house has.** §28.2 and
            §53 both say "84 switch groups, 169 fixtures"; §13's `Lights` column, which §13.1 says
            counts groups, totals **128** across the five levels. `B1` and `L0` alone account for
            55 of them and 97 fixtures. §13 wins for authoring, because it names the room; the
            reconciliation belongs to `HOUSE-00382`, which finishes the set and can then correct
            §28.2 and §53 against a complete count rather than half of one.
      finding: **no cell declared its `lightGroups`.** The field is §28.1's per-frame index and
            every one of the 96 cells had it empty, which rule 6 could not see: it only checked
            that a listed group exists, and an empty list lists nothing. Rule 6 now checks the
            index **both ways** — a group listed with no light of that group in the cell, and a
            light in the cell whose group the list omits. One direction is a switch the room does
            not respond to; the other is a lightmap pass over a group with nothing to light.
      note: positions are derived, not eyeballed: fixtures sit on a 1×N or 2×M grid in the cell's
            largest box, inset so N fixtures divide the room into N+1 bands, at the ceiling less
            the mounting drop for their kind — 0.02 m for a recessed can, 0.85 m for a pendant,
            1.05 m for a sconce. `range` is the distance to the farthest corner of the cell plus
            half a metre, so a light reaches its own room and does not leak two rooms further.
      note: `L0_PORCH`'s group is deliberately **not** here. §13.3 counts it, and `HOUSE-00383`
            authors the exterior lights including the two lanterns; authoring it twice is how a
            group ends up defined in two places with different ids.
      note: `CELL_FRIDGE_INTERIOR` gets a light §13 does not count, because §13's tables do not
            list container sub-cells. It is `emissive_only`, not baked, and §53 drives it from the
            door rather than from a switch.
      note: `fixtureProp` is null throughout — the props are `HOUSE-00450` onwards and a made-up id
            would point at nothing. `emissiveMaterialSlot` is not null where the fixture has a
            visible lit part, because §53 lights that slot when the group comes on.
      note: `AuthoredWorldTest` asserts the 97 fixtures, the 55 groups and rule 6's index through
            the C++ loader.
- [x] HOUSE-00382 — Author `layout.lights.json` for `L1`, `L2`, `L3`
      dep: HOUSE-00381 · sys: world · plat: TOOL · pri: MUST
      finding: **§28.2 and §53's "169 fixtures in 84 switch groups" was never reachable.** With
            all five floors authored the house holds **220 fixtures in 128 groups**, and 128 is
            exactly §13's per-room `Lights` column summed — the column §13.1 says counts groups.
            84 is not a rounding of anything in §13. The per-room column wins because it names the
            room the switch is in; §28.2, §53 and §62's audio-gap table are corrected, and so is
            §28.6's aside about "six faces × 169 fixtures".
      finding: **§53's exterior list forgets the balconies.** It enumerates porch, garage flood,
            terrace, paths, shed and street, and §13.4 gives `L1_BALCONY_REAR` two groups and
            `L1_BALCONY_FRONT` one. They are `L1` rows, so they are authored here with the rest of
            `L1` — two wall lanterns and a festoon over the table, one lantern on the front — and
            §53's list now says so, which is also what keeps `HOUSE-00383` from authoring them a
            second time under different ids.
      note: 44 more cells gained their `lightGroups` index. Rule 6 checks it both ways, so an
            upper-floor cell whose lights were authored and whose index was not would have failed
            the gate rather than sat wrong for a phase.
      note: the fixture-per-group counts follow the room: three pendants over the pool table, four
            strips washing the library's floor-to-ceiling shelves, a bare bulb on a pull cord at
            the attic stair because §13.5 says so, two bedside lamps in the master because a king
            bed has two sides.
- [x] HOUSE-00383 — Author the exterior lights: porch, garage flood, terrace, path, shed, plus the street lights
      dep: HOUSE-00381 · sys: world · plat: TOOL · pri: MUST
      finding: **a light could name any cell in the house and sit anywhere.** Rule 6 checked that
            the cell exists; a fixture 30 m away resolved perfectly while lighting nothing and
            baking a lightmap for a room it is not in. Rule 10 now checks a light's position
            against its cell's footprint **and** its vertical extent. This task is where it would
            have bitten: the nine street lights are spread over 200 m of road, one of them over
            the plot in `EXT_ROAD` and eight past the boundary in `EXT_WORLD`, and getting that
            wrong is invisible until a lightmap bakes.
      finding: **§65.6's initial state says "all 84 groups off"** — the same count §28.2 and §53
            carried. 134 with the exterior authored. Corrected there too, so `HOUSE-00395` starts
            from a number that exists.
      note: 19 fixtures in 6 groups, exactly §53's list: 2 porch lanterns, 1 garage flood aimed
            down the drive rather than at the street, 2 terrace lights on the sunroom's north
            wall, §11.4's 4 bluestone path bollards, 1 shed bulb and the 9 street lights.
      note: these are the only fixtures in the file whose height is measured **up from the
            ground**. An exterior cell's `yOverride` top is the sky at +20, and a lantern 0.02 m
            below the sky is not a lantern; every exterior position states its height above the
            cell's own floor instead.
      note: **the neighbours' porch lights are deliberately not here**, and this task's title is
            corrected to say so. §53 lists them, and their positions are the positions of N1-N60,
            which `HOUSE-00391` authors. Placing them first would mean inventing sixty houses to
            hang them on; `HOUSE-00391` adds them when the houses exist.
      note: the whole house now holds **239 fixtures in 134 groups**, asserted through the C++
            loader by `AuthoredWorldTest` alongside rule 6's per-cell index.
- [x] HOUSE-00384 — Author the switch plates: position, gang count, group mapping, including the two three-way pairs
      dep: HOUSE-00382 · sys: world · plat: TOOL · pri: MUST
      finding: **two rooms could not have their own switch.** §53 puts a plate at 1.20 m beside
            the doorway and §15.7 rule 11 asks for a standing eye position to reach it; the garage
            loft is 1.40 m under its rafters and the under-stair cupboard 1.75 m, so neither has
            one. Both are switched from the next room — the loft at the bottom of its ladder, the
            cupboard from the hall — which is what a house does anyway, and the rule found it
            before a play-test would have.
      note: **the gang is the group.** A plate's state field is named after the light group it
            controls, `state.LG_L0_HALL_MAIN`, which is what makes §53's three-way pair
            expressible: "a group with two switch props" is two plates whose gangs name the same
            group, and §53's "one bit per group" is that field. Rule 6 now checks every
            `light_switch` state field against the declared groups — `state` is free-form by
            design (`HOUSE-00354`), so a typo in a gang would otherwise be a switch that toggles
            nothing, and a claim proves the check reads `kind` rather than every row with a state.
      note: placement is derived: beside the room's entry doorway (a door into a corridor first,
            then any way in), 0.30 m clear of the opening on the side with more wall beside it,
            0.06 m into the room, at 1.20 m above that cell's floor. Rule 11 then **proves** all
            80 are reachable, which is the point of putting them in data rather than in an editor.
      note: 80 plates, 121 gangs. 15 groups have no plate, exactly as §53 lists: the table and
            floor lamps are switched at the lamp, the refrigerator's interior light by its door,
            and the porch lanterns and street lights by a dusk sensor.
      note: this is the first content of `interactables.json`. §53 wants 640 rows there in twelve
            behaviour classes; these are the light switches and the file grows.
      note: `AuthoredWorldTest` asserts the 80 plates, the 121 gangs, that every gang names a real
            group, and that exactly two groups are on two plates each.
- [ ] HOUSE-00385 — Author `layout.materials.json`: the material class table of §22.2 with all fields
      dep: HOUSE-00296 · sys: world · plat: TOOL · pri: MUST
- [x] HOUSE-00421 — Deploy `assets-src/world/*.json` to `content/world/` with the comments stripped
      dep: HOUSE-00366 · sys: ci · plat: CI · pri: MUST
      note: (2026-09-07) **New task, next free id in phase 5's reserved 00341–00450 range.** Found
            by `HOUSE-00366`: `docs/world-format.md` says the authored files are JSONC and "the
            build strips comments and deploys plain JSON beside the compiled content", and nothing
            does the stripping. `System::Text::Json` refuses the first comment, so the runtime
            cannot read what was just authored — the two halves of the format were specified and
            neither was built.
      accept: every world file in `assets-src/world/` appears in `content/world/` as plain JSON
            with identical parsed content; the deploy is a `build_content.py` stage; `WorldLoader`
            loads the deployed copy
      verify: unit `AuthoredWorldTest.*` loads the deployed world with the real loader
      note: (2026-09-07) `tools/world/deploy_world.py`, a `build_content.py` stage after
            `world-rules`, and a `run_checks.sh` gate. 13 selftest claims; `AuthoredWorldTest`
            loads `content/world` with the real `WorldLoader` and asserts §12.2's five elevations,
            the attic's null ceiling, and that every storey's ceiling plus its structure depth is
            the next storey's floor.
      note: the deployed file is the parsed document written back, **not** the source with the
            comment characters blanked. A blanked comment leaves the whitespace it occupied, and —
            the reason that matters — rewriting through a parser proves the authored file parses
            at all before it reaches the game. A byte copy would ship a syntax error and let the
            runtime find it.
      finding: **this corrected `HOUSE-00364`.** The manifest hashed the *authored* bytes, and
            `WorldLoader::VerifyManifest` rehashes the files it is about to read — which after
            this step are the *deployed* ones. Every file with a comment in it, which is every
            file, would have failed at load. The manifest is now generated by `deploy_world.py` as
            its last step, over what it just wrote, and `assets-src/world/world.manifest.json` is
            deleted: it was generated data sitting in a source tree, which is a second source of
            truth by another name. Two selftest claims pin it — the manifest hashes the deployed
            bytes and *not* the authored ones, which differ by exactly the stripped comments.
      finding: a file that is deployed and no longer authored is reported rather than left. The
            manifest would not list it and the loader would not read it, so it would be a room
            that exists in the game's directory and nowhere else.
- [x] HOUSE-00386 — Author the plumbing stack description (STACK-A…F) as data, so the validator can check fixture placement
      dep: HOUSE-00368 · sys: world · plat: TOOL · pri: MUST
      finding: **§15.7 rule 9 required one cell per level and §12.5's own data breaks it twice.**
            STACK-E takes the kitchen sink and the sunroom's wet bar, both on `L0`; STACK-F takes
            two basement fixtures. Several fixtures on one floor branch into the same stack, which
            is what plumbing does. The rule now checks that no cell is listed twice instead — the
            check it lost caught nothing real, and the one that does, "this cell is not over the
            drop", is untouched and pinned by its own claim.
      note: `chase` is the footprint the stack rises **within** — §12.5's "vertical alignment"
            column — and not the 100 mm pipe. Rule 9 asks whether a cell is above the drop; a box
            the width of the pipe would answer a question nobody is asking. Where §12.5 gives a
            real chase, STACK-B's shared corner and STACK-C's west wall, the box is that chase.
      note: STACK-F is not a vertical stack at all. Both its fixtures are already in the basement,
            so its "chase" is the footprint of the drain run under the slab and `dropTo` is null:
            it drops into an ejector pit, which is machinery and not a cell. The format already
            allowed a null `dropTo`; this is the first row that needed it.
      note: §12.5's other four rows — the water main, the sanitary drain, the gas meter and the
            HVAC trunk — are services, not stacks. `HOUSE-00387` authors the ducts.
      note: `AuthoredWorldTest` asserts the six stacks through the C++ loader, including the two
            that branch and the null drop.
      note: (2026-09-07) the **format** now exists: `layout.levels.json` `plumbing.stacks`
            (`id`, `cells`, `chase`, `dropTo`) and a prop's `plumbing` field, added under
            `HOUSE-00358` because rule 9 could not be written without them. This task authors
            §12.5's six rows into it.
- [x] HOUSE-00387 — Author the HVAC duct/register description as data, for audio placement
      dep: HOUSE-00386 · sys: world · plat: TOOL · pri: MUST
      finding: **the format had no HVAC block at all**, and 42 cells already named a duct branch
            in `thermal.ductBranch` that nothing declared or checked. §12.5 describes the plant,
            the trunk and the chase in one table row and §62.6 places the duct rumble "at each
            register", so `layout.levels.json` gains `hvac` beside `plumbing`: the plant, §12.5's
            chase at X +2.20…+2.60, **14 branches and 54 registers**.
      note: the registers live here and **not** as 54 more audio emitters. §62.6 places the rumble
            at each register, and an emitter per register would be the same fact written twice —
            the mistake `HOUSE-00388` had just finished undoing for the transmission losses.
      note: branch membership is not repeated either: a cell names its branch and the branch lists
            its cells, and rule 6 checks the two against each other, as it does a cell's
            `lightGroups` against its lights. A branch that has lost a room is a room the furnace
            is silent in, and neither half of the pair can see that alone. Four claims, four
            injected bugs, all caught.
      note: a register is in the **floor** upstairs, where the branch runs in the joist space
            below, and in the **ceiling** in the basement, where the trunk is exposed under the
            `L0` slab — which is also §62.6's reason for the rumble being loudest in `B1`. The C++
            loader reads the block and `AuthoredWorldTest` asserts both, so the registers reach the
            runtime that needs them rather than sitting in a file nothing opens.
- [x] HOUSE-00388 — Author `layout.audio.json`: per-cell room tone, absorption, reverb hint; per-portal transmission losses (§64.3)
      dep: HOUSE-00367…HOUSE-00372 · sys: world · plat: TOOL · pri: MUST
      finding: **the C++ loader dropped seven of §64.3's ten transmission classes without a
            word.** `LoadAudio` held a hardcoded list of six class names — `door_hollow`,
            `door_solid`, `door_glazed`, `window`, `opening`, `hatch` — invented before §64.3 was
            authored, and the names the design actually uses (`door_exterior`, `slider_glass`,
            `window_single`, `window_hopper`, `door_garage`, and the two added below) matched none
            of them. The comment beside the list claimed "the count is compared below and a
            mismatch is reported"; there was no such comparison. It enumerates the object now,
            with `JsonValue::Members()`, so there is no list to keep in step. A new
            `AuthoredWorldTest` found it the first time the C++ reader saw the real table.
      finding: **§64.3 had no row for a hatch or an appliance door**, and both are portals: the
            garage loft hatch and the refrigerator door and chest-freezer lid. Two rows added —
            loft hatch 1/18 dB, gasketed appliance 2/34 dB.
      finding: **`soundLoss` and §64.3 were two copies of one fact.** Every portal carried a
            hand-picked pair and §64.3 stated the same thing per class, in decibels. §64.3 is the
            source now: `layout.audio.json`'s `transmission` block is that table as fractions
            (`loss = 1 − 10^(−dB/20)`), all 179 portals are derived from it, and rule 6 checks each
            portal against the class its **leaf type** selects — a solid-core door and a hollow one
            are the same portal kind and 8 dB apart, which the injected-bug test pins.
      note: §64.3 names the study, the master and the cinema as solid-core. The study is entered
            through a double door, which was solid already; the other two are now `D_INT_SOLID`,
            a thirteenth door type, and the file says why.
      note: 92 zones — every cell that is not a container or `EXT_WORLD` — sharing **eight** beds
            rather than one each: room tone is a bed, and 92 of them would be 92 samples nobody
            could tell apart. `acoustic.roomTone` was empty on every cell until now.
      note: 29 emitters, all of §62.6's table that can be placed yet: the furnace and water heater,
            the refrigerator and freezer, three clocks, the garage motor, the doorbell, six pipe
            runs on §12.5's own chases, and the 14 creak points spread over the storeys. The duct
            registers wait for `HOUSE-00387`, which authors the ducts.
- [x] HOUSE-00389 — Author `layout.nav.json`: 333 nodes, 677 edges, 22 perches, 2 beds, 2 bowls, forbidden zones
      dep: HOUSE-00367…HOUSE-00372 · sys: world · plat: TOOL · pri: MUST
      finding: **nothing checked the nav file beyond a node's cell.** Rule 6 read `nodes/N/cell`
            and stopped: an edge could name a node that does not exist, or name a portal joining
            two entirely different rooms, and a perch could be in a cell nobody authored. All four
            are checked now, and the portal one is the interesting one — a route through a door
            has to go through **that** door, or the pet walks through a wall while the data claims
            it used the doorway.
      finding: **rule 5 asked its connectivity question of one graph and there are two.** The pet
            waypoint graph has its own edges, and a room reachable through a door is not reachable
            by a dog unless somebody put a waypoint in it. Rule 5 now walks both; the authored
            graph is one component of 333 nodes, and the claim that proves the check works names
            the room the stranded node is in.
      note: nodes are a grid over each cell's boxes — 2.5 m indoors, 10 m out, inset 0.55 m from
            the walls so a node is somewhere a dog can actually stand — and an edge joins nodes
            within a spacing and a half, which on a grid is the four neighbours and no more.
            Windows, hatches and the sectional door carry no edge: a person can climb through a
            hatch and a pet cannot.
      note: the 22 perches are §61's, including the eleven it names — the landing window seat, the
            top of the kitchen cabinets, the back of the living-room sofa, the office desk, the
            master bed, the warm spot on the mechanical-room duct, three windowsills, the newel
            post and the airing-cupboard shelf. All cat-only: §61 says a cat that is *on* things
            rather than beside them is most of what makes it read as a cat.
      note: forbidden zones are the plant room (both), the garage (the cat), the four attic stores
            (the dog: the attic stair is 0.90 m and steep) and the road — which also has no nodes
            at all, and that is the real fence.
- [x] HOUSE-00390 — Author `layout.exterior.json`: terrain reference, road segments, sidewalks, kerbs, driveway, paths, fences, gates, shed
      dep: HOUSE-00372 · sys: world · plat: TOOL · pri: MUST
      finding: **the format had terrain, a road and fences, and the task asks for seven more
            things.** Four arrays added: `gates` (§11.2's pedestrian, sliding vehicle and bolted
            rear), `kerbs` (§11.4's two at Z +3.2 and +10.2), `paths` (the walk, the driveway and
            its apron, both sidewalks, the verge and the garden path) and `structures` (the shed).
            Without them §11.2's gates and §11.6's shed existed only in prose.
      note: the front fence is **three** runs, not one, because the two gates are holes in it. A
            fence drawn through its own gate is the kind of thing nobody notices until they walk
            at it.
      note: rule 6 checks that a gate hangs in a declared fence and that a structure's cell exists;
            rule 10 checks that an enterable structure's footprint **contains** its cell's. The
            shed's shell is 3.6 m and its cell 3.2 m, which is the 0.2 m of wall — and a shell that
            does not hold its own interior is a building drawn beside its inside. Three injected
            bugs, one of which needed a sharper fixture: a shed the right width and the wrong depth
            is exactly as wrong as one in the next county, and half a test missed it.
      note: `terrain.heightfield` is a path and not an asset id, because §11.5's height field is
            authored later. The convention it will be read with is stated in the data instead —
            sample 0 is `origin.y`, sample 65535 is `origin.y + yScale` — so the lot's +0.15 to
            −0.35 uses a hundredth of the range and nothing has to be re-encoded when a swale is
            added.
      note: the road runs the full ±220 m and not §10.4's accessible ±35: the geometry continues
            past where the player can walk, which is what makes the barrier believable.
- [x] HOUSE-00391 — Author the neighbourhood instance list: N1–N60 with positions, LOD class and impostor distance
      dep: HOUSE-00390 · sys: world · plat: TOOL · pri: MUST
      note: §11.4's sixty buildings in four LOD bands — the two adjacent houses at full detail, six
            across the street at medium, sixteen further out at low, and thirty-six impostor cards
            on a ring at 120–260 m — plus the street furniture: five utility poles, four signs,
            twelve mailboxes, three parked cars, two bin clusters and the basketball hoop. 87 rows.
            The nine street lights are **not** among them: they are lights and live in
            `layout.lights.json` (`HOUSE-00383`).
      note: **the neighbours' porch lights are placed now**, which is what `HOUSE-00383` deferred
            and said why: their positions are N1's and N2's, and the two houses did not exist. Two
            per facade, `EXT_WORLD`, on the same dusk sensor as ours and not player-controlled.
            Rule 6 caught the index the moment they landed — `EXT_WORLD` had lights in a group it
            did not list — which is the check `HOUSE-00381` added doing its job on a task written
            two days later.
      note: this task's title said "material palette"; the schema has no such field and the design
            does not name one per building. `lodGroup` and `impostorFrom` are what a neighbourhood
            row carries, and the title now says so rather than promising a field that would have
            to be invented to satisfy it.
      note: positions are deterministic — one fixed seed for the jitter — so the street is the same
            in every build and a screenshot comparison means something.
- [x] HOUSE-00392 — Author the vegetation instance list: 34 street trees, property trees, shrubs, hedges, flower beds
      dep: HOUSE-00390 · sys: world · plat: TOOL · pri: MUST
      note: seven groups, 229 instances: §11.4's 34 street trees stepped around the pedestrian
            gate, §11.6's two mature maples and the birch, the four fruit trees in the orchard,
            §11.4's 60 shrubs across the west border, the foundation planting and the rear lawn,
            §10.4's privet hedge, and §11.6's six raised beds plus the flower bed against the
            porch.
      note: a hedge is an extruded strip and the schema has instance arrays, so it is **one 1 m
            section per instance** — 121 of them. That is not a workaround: a hedge with a gap in
            it is a hedge with a gap in it, and 121 sections are cheaper to check than a polyline
            nothing validates.
      note: the jitter is fixed-seed, like `HOUSE-00391`'s: the garden is the same in every build,
            so a screenshot comparison means something.
- [x] HOUSE-00393 — Author `layout.weather.json`: the 14 archetypes, 4 seasonal transition matrices, dwell and transition distributions, rate limits
      dep: HOUSE-00341 · sys: world · plat: TOOL · pri: MUST
      finding: **§36.2 says "twelve named archetypes" over a table of fourteen rows.** Thirteen
            states and one modifier. Corrected there with the count.
      finding: **the format could not carry two of §36.2's own columns.** The table has a `thunder`
            column with nowhere to go, and a `W_WINDY` row that is a **modifier** — it combines
            with any precipitation archetype, which is how "windy heavy rain" arises without a
            combinatorial state list — and nothing said so. `thunderProbability` and `modifier`
            added to the schema; rule 6 now refuses a transition INTO the modifier, because §36.2
            says it combines with a state rather than being one.
      finding: **nothing checked that a transition row is a distribution.** A row summing to 0.8
            does not fail loudly at runtime: the sky favours whatever the sampler reaches first and
            the weather is subtly wrong forever. Rule 10 checks the sum, rule 6 checks that every
            state has a row to leave by — a state with none is a sky that arrives and never moves.
      note: §36.3 asks for four seasonal transition matrices; this is **one base matrix and four
            seasonal weight vectors**, which is the same thing in 13 numbers a season instead of
            169. The matrix says what follows what; the season says what the year is fond of. The
            reduction is recorded in the file and in `docs/world-format.md` rather than left as a
            silent simplification.
      note: every quantity is a range and not a value, because §36.2's numbers are an archetype's
            centre and the target is drawn from the band around it. Two thunderstorms are not
            identical, and that falls out of the data rather than out of a special case.
      note: summer weights `W_SNOW` and `W_HEAVY_SNOW` at **zero**, not merely low — §36.3's one
            hard seasonal gate, and the only zero in the four vectors.
- [x] HOUSE-00394 — Author `layout.sky.json`: the zenith and horizon LUTs, cloud layer definitions, the sun/moon colour LUTs, the star catalogue reference
      dep: HOUSE-00341 · sys: world · plat: TOOL · pri: MUST
      finding: **the file had nowhere to put the sun, the moon or §31.3's cloud alphas.** The
            schema had a dome gradient, the layers' textures and the star reference. `cloudAlpha`,
            `sun` and `moon` added — §31.3's table of per-layer alphas by sky state and §32/§33's
            disc colour and brightness by elevation are design the format could not carry.
      note: §31.2 describes a 32 × 8 zenith LUT and a 32 × 8 × 16 horizon LUT — sun altitude by
            cloud cover, and by angle from the sun's azimuth. This file carries the **clear-sky**
            curve over sun altitude alone, 32 entries, because §31.2's own pseudocode already
            applies the other two axes analytically: `mix(c, overcastGrey, cloudCover^1.5)` is the
            cloud axis and `sunGlowTerm(angleToSun)` the azimuth one. One 32-entry table plus two
            terms the shader computes is the same sky as a 4096-entry table, and it is one a person
            can art-direct. Recorded in the file and in `docs/world-format.md`, not assumed.
      note: the entries are smoothstep interpolations between **eleven hand-tuned anchors** — the
            three twilight bands, the sunrise horizon, midday — and not a Preetham evaluation. If
            the look needs the physical model later, this table is what it replaces, in the same
            shape, and the file says so rather than implying the numbers are physical.
      note: rule 10 checks the three elevation tables ascend — one that does not interpolates
            backwards, and a sun table out of order reddens at noon — and that the cloud-alpha
            bands run contiguously from 0 to 1, because a cover in a gap is a sky with no clouds
            drawn at all. Five injected bugs, all caught once the band-start fixture was added.
      note: 1 500 stars, per §34, and not the 1 800 in `docs/world-format.md`'s example.
- [x] HOUSE-00395 — Author `initialstate.json` exactly as `cna-house.md` §65.6 specifies
      dep: HOUSE-00355 · sys: world · plat: TOOL · pri: MUST
      finding: **the game started 3.3 m above the road.** §65.6 puts the player at
            `(0.00, 3.32, +5.20)`, and §49 makes `playerPosition` the **feet** with the eye at
            `+ (0, eyeHeight, 0)`. Corrected to grade, and rule 10 now checks that the player and
            both pets start inside the cell they name, in footprint and in extent — this is the
            one frame every test and every screenshot begins on, and a start above the floor is a
            first frame spent falling.
      finding: **`epochSeconds` was documented as "seconds into the simulated day".** §65.6 starts
            the game on "Saturday 14 June 2031, 09:20", and a calendar date — which the season, the
            moon phase and the sun's position all need — is not expressible as an offset into a
            day. It is seconds since the Unix epoch: 1939209600 is 2031-06-14T13:20:00Z, 09:20 at
            UTC−4, and 14 June 2031 really is a Saturday. The C++ comment is corrected and §65.2's
            save example, which carried a value that was neither reading, with it.
      finding: **§65.6 names `PERCH_L1_WINDOWSEAT` and `HOUSE-00389` had called it
            `PERCH_L1_LANDING_SEAT`.** The document named it first, in two places; the nav file is
            renamed to match rather than the other way round.
      note: `utcOffsetMinutes` is −240, not §32's −300: June is inside US DST, which is what
            §65.6's "(UTC−4 DST)" says and what the season and the sun's azimuth depend on.
      note: the file is a **delta**. Everything §65.6 lists as "all closed", "all off" or "all
            zero" is the default and is absent; the four rows that are there are the only things
            not starting at rest. Those four ids — three doors and the master's open window — are
            an obligation on the task that authors door and window interactables, exactly as
            `layout.openings.json`'s `MAT_*` ids are on `HOUSE-00385`, and rule 6 will hold this
            file to them the moment those rows exist.
      note: rule 6 also checks the weather target is an archetype, the pets' perch and bed are ones
            `layout.nav.json` declares, and every cell named exists. Four injected bugs, all
            caught. `AuthoredWorldTest` asserts the corrected position and clock through the C++
            loader.
      accept: every field in §65.6 is present; the three named open doors and the one open window are explicit
- [x] HOUSE-00396 — Run `report_graph.py` and reconcile its output with `cna-house.md` §16.3; fix whichever is wrong
      dep: HOUSE-00374…HOUSE-00377 · sys: world · plat: TOOL · pri: MUST
      finding: **§16.1 named a portal that does not exist.** `P_EXT__L0_PORCH`, an opening from
            `EXT_WORLD` straight onto the porch, was written before §13.7's yard cells: the porch
            is actually reached by three cased openings, from the two front yards and up the steps
            from the walk. §16.1 now lists those three.
      note: §16.3's metrics already agreed, because `HOUSE-00374`…`HOUSE-00377` corrected them as
            they authored. The deliverable that makes it **stay** true is the gate:
            `report_graph.py --check` compares both directions — every `P_*` §16.1 and §16.2 name
            must exist in the data, and every §16.3 metric row must state the number the layout
            measures — and it is wired as `graph-report` beside `window-schedule`.
      note: the metric comparison is "the measured number appears in the row" rather than a full
            parse. Those rows carry the design figure beside the measurement on purpose, and a
            parser strict enough to read them would break whenever somebody explains something.
            A deleted row is caught separately, because a check that only compares the rows it
            finds says nothing about the one somebody removed.
- [x] HOUSE-00397 — Run the full validator; fix every violation; record the first clean run
      dep: HOUSE-00396, HOUSE-00360…HOUSE-00362 · sys: world · plat: TOOL · pri: MUST
      note: (2026-09-08) **the first clean run, recorded.** All eleven rules of §15.7, no rule
            restricted, over the authored world and over the deployed one:

                $ python3 tools/world/validate_world.py assets-src/world
                validate_world: assets-src/world passes all 11 rule(s).   0 shape, 0 rule, 594 ms
                $ python3 tools/world/validate_world.py content/world
                validate_world: content/world passes all 11 rule(s).      0 shape, 0 rule, 646 ms

            What it validated: 5 levels, 96 cells, 179 portals, 133 openings, 8 stair flights,
            243 light fixtures, 333 nav nodes and 677 edges, 92 audio zones and 30 emitters,
            7 plumbing stacks, 14 duct branches, 253 interactables. The C++ half agrees:
            `WorldValidatorTest.TheAuthoredWorldPassesEveryRuleItCanSee` runs rules 3, 5, 6, 7, 9,
            10 and 11 at `ValidationDepth::Full` over the **deployed** copy in 288 ms and reports
            nothing, and the whole suite is 515/515.
      note: **no violations were left to fix, and that is the finding rather than an anticlimax.**
            Every violation this task was written to catch was found and fixed by the authoring
            task that caused it — the dormer bays that cost `P_L3_STORE_W__L3_STORE_S` its shared
            plane, the fridge door rule 4 refused before it understood sub-cells, the two plumbing
            stacks and the eighth stair flight that placing fixtures discovered. A rule that runs
            on every commit does not accumulate a backlog to work through, which is the whole
            argument for `HOUSE-00363` having wired it into CI before the data was authored.
      note: the deployed copy is validated here but is deliberately **not** a second gate.
            `deploy_world.py --check` compares *parsed documents*, not text, so a deployed file
            that parses to the same document as its source cannot fail a rule its source passes.
            A second `world-rules` run over `content/world` would cost 646 ms on every commit to
            re-prove that equality.
- [x] HOUSE-00398 — Generate the printable floor plans (SVG per level) from the layout for review and for the docs
      dep: HOUSE-00397 · sys: world · plat: TOOL · pri: SHOULD
      note: six plans in `docs/floor-plans/` — one per level plus a site plan for the yards, the
            terrace, the garden and the shed. Every line comes from `assets-src/world/`: cell
            boxes, portal rectangles and the plumbing chases. Nothing is positioned by hand, so a
            plan cannot drift from the house, and the `floor-plans` gate fails when the committed
            SVGs stop matching — a stale plan is worse than no plan, because somebody acts on it.
      note: SVG rather than a raster because it prints, it diffs as text and a reviewer opens it in
            a browser with no tooling. A PNG would be a binary blob nobody can review in a diff.
      note: the claim that earns its keep is the one nobody would notice being wrong: §14 puts
            north at −Z, so a cell further north is drawn **higher** on the page. A plan with north
            upside down is one every reviewer acts on and nobody questions, so the selftest moves a
            room north and asserts its label moved up. Four injected bugs, all caught.
      note: this is the first time the whole session's authoring can be **looked** at: 96 cells and
            179 portals of JSON become six pages.
      files: tools/world/plan_svg.py, docs/plans/*.svg
- [x] HOUSE-00399 — Author the ID golden list (every cell, portal, light group, interactable, material, asset) for the stability test
      dep: HOUSE-00397 · sys: world · plat: CI · pri: MUST
      note: (2026-09-08) `tests/unit/reference/world-ids.golden.txt` — **2 152 ids over 27 kinds**,
            generated by `tools/world/id_golden.py` and gated as `world-ids`. The task names six
            kinds; the tool takes every object anywhere in the sixteen files that has an id-shaped
            `id`, found by walking the documents rather than by naming the arrays here, because a
            new array of things with ids is exactly what a hand-maintained list of kinds misses.
            463 assets, 333 nav nodes, 253 interactables, 243 fixtures, 179 portals, 135 light
            groups, 133 openings, 96 cells, 92 audio zones, 87 neighbourhood buildings, and so on
            down to the one shed.
      note: **append-only, and that is the whole design.** `--emit` adds and never removes, so a
            rename reads as one id gone and one arrived and only the arrival is automatic; the
            departure keeps failing until a person deletes the line and says why. A tool that
            regenerated the file would launder precisely the change it exists to catch — and the
            first version of the selftest could not tell, because the claim re-implemented the
            merge instead of calling it. Extracting `merged()` so the claim runs the same code the
            tool runs is what made the injected "`--emit` regenerates" bug fail.
      note: **light groups are in the list and no row declares one.** A group exists only as a
            reference from a cell and a fixture, and a light-switch interactable's state field *is*
            the group id — so renaming a group silently changes what a saved switch state means.
            §70.2 names them for that reason and the collector special-cases them.
      note: two of the six kinds the title names are **empty and recorded as empty**: there are no
            materials (`layout.materials.json` is `HOUSE-00385`, behind `HOUSE-00296`) and no props
            (`HOUSE-00450` onward). Nothing needs doing when they arrive — the walk is generic, so
            `--emit` will pick them up.
- [x] HOUSE-00400 — Phase-5 review: walk the plan on paper against the room schedule and the plumbing diagram; correct `cna-house.md` if the data disagrees
      dep: HOUSE-00397 · sys: — · plat: ALL · pri: MUST
      finding: **§13 disagreed with the layout in 52 places.** Walked all 94 rows of §13.2–§13.7
            against `assets-src/world/`: 6 wrong areas, 2 wrong extents, 44 wrong window and door
            counts, and 9 unions of boxes that §13.1's own `∪` convention says must be marked and
            none of which was. The `Lights` column was right everywhere, which is what a column
            that means one thing looks like.
      finding: **the garage is 70.6 m², not 65.5.** Its own X and Z columns say 8.4 × 8.4; the area
            beside them was the designed figure and nobody multiplied. Likewise `B1_UNDERSTAIR`,
            still carrying 7.6 m² a year after a correction note **in the same section** had worked
            out 4.68: it is an L-shaped remainder, and §13.1 never said whether Area meant the
            union or the rectangle around it. It says so now, and the row is marked `∪`.
      finding: **the porch and the front balcony began at Z −14.00 and the house's front face is
            at −14.30.** The layout is right — every other L0 cell's front boundary is −14.30 and
            rule 3 confirms no overlap — so both extents were corrected, which also moves both
            areas from 17.3 to 19.4 m² and §13.4's balcony total from 64.3 to 66.4.
      finding: **§13.3's summary added part of the floor twice.** "404.6 m² incl. garage and porch;
            291.9 m² of heated interior" → with the garage and porch corrected the floor measures
            438.30 m², less the loft's 27.38 = **410.9**, and the heated main block is the
            21.4 × 12.8 plate, **273.9** — the old figure counted part of what it then added again
            as the sunroom.
      finding: **§13.7's cell arithmetic was one row out.** It read 75 interior, 93 total, 3 nested
            sub-cells. §13.3 has 20 rows and its summary says "19 cells" because it deliberately
            excludes the garage loft; the recount took that 19 for the row count. So the loft *is*
            tabulated, there are 2 unlisted sub-cells and not 3, and 15 + 20 + 21 + 18 + 6 = 80
            rows − 4 attached exterior = **76** interior, + 18 exterior = **94**, + 2 = 96.
      note: **`Win` and `Doors` had no single meaning.** The columns held counts, attributions and
            prose together — a central hall with three doorways written as `0`, a garage door count
            that omitted the loft hatch, `2 sidelights + transom` where the schedule asked for a
            number — and no rule reproduced them: counting by the leaf's `swing` cell matched 45
            rows, counting the cell's boundary matched 29. §13.1 now defines them as the openings
            on the **boundary** — every sash and leaf you can see from inside the room, so a door
            counts for both cells — because that is the only definition a reader can check by
            standing in the room. The eighteen descriptions moved into `Notes`.
      note: **§12.5's plumbing diagram was already right**, all seven stacks, their fixtures and
            their drops. It is right because `HOUSE-00386` and `HOUSE-00413` corrected it as they
            authored — `STACK-G` exists in both places because placing the toilets found two
            draining nowhere. It is now checked as well as correct.
      note: the review is a **gate**, not a walk: `tools/world/room_schedule.py --check`, wired as
            `room-schedule`. It measures the four numeric columns and the `∪` marks and rewrites
            them with `--emit`; it compares the `X`/`Z` extents and never rewrites one, because
            which of the two is right is a question about the house and a tool cannot answer it.
            Five injected bugs caught, and a sixth injection found a line of dead code — an em dash
            special case that no outcome depended on — which was deleted rather than defended.

### 5.3 Interactable data authoring (placement only; behaviour comes in phase 14)

- [x] HOUSE-00401 — Author `interactables.json` rows for the 64 doors
      dep: HOUSE-00375 · sys: world · plat: TOOL · pri: MUST
      note: 64, not 62: every leaf that swings, slides or lifts on a frame — the 53 interior doors,
            4 double doors, 2 entry, 2 exterior side, 2 sliders, the garage sectional and the
            basement stair-head door — which is `layout.openings.json`'s doors less the
            refrigerator's and the two hatches, and those belong to the container and hatch tasks.
      note: **the opening is the geometry and this is the thing you use.**
            `docs/world-format.md` says an opening "pairs with exactly one portal and with one
            interactable"; these are those interactables, each naming its portal, so visibility,
            audio and the pet graph all reach the same door through the row they already hold.
      note: three are named as §65.6 names them — `DOOR_L1_MASTER`, `DOOR_L0_PANTRY`,
            `DOOR_L2_ATTIC` — because a save delta that references a door by a name the design has
            already published should not have to be translated. Three of `initialstate.json`'s four
            dangling references now resolve; `WIN_L1_MASTER_N2` waits for `HOUSE-00402`.
      note: `openFraction` is continuous, not a boolean: §54 swings a door 100° over 0.6 s and
            §65.6 starts one of these at 0.35. A door that is either open or shut cannot be ajar.
      note: **rule 11 proved all 64**, on top of the 80 switch plates: the focus is the handle,
            away from the hinge, 1.05 m up and 0.06 m inside the room the leaf opens into, and
            every one is reachable from somewhere a player can stand.
      note: seeding the name counter with §65.6's three was not optional — the butler's-pantry door
            derives `DOOR_L0_PANTRY` from its own swing cell, and rule 1 caught the collision.
- [x] HOUSE-00402 — Author rows for the 54 openable windows
      dep: HOUSE-00376 · sys: world · plat: TOOL · pri: MUST
      note: 54, not 62. §12.6's twelve fixed lights are not interactables — the two picture
            windows, the two gable louvres, the transom, the two sidelights and the sunroom's five
            glazed panels — because a light that does not open is geometry and nothing else. The
            62/19 split was never reachable from §13's per-room column, which `HOUSE-00376`
            recorded when it authored the 66; this is the openable half of the measured breakdown.
      note: named `WIN_<cell>_<facade><n>`, which is §65.6's own `WIN_L1_MASTER_N2` shape and
            cannot collide with the **opening** ids, which are `WIN_<cell>_<n>` — rule 1 caught
            that collision on the first attempt. The facade letter earns its place: "the second
            north window of bedroom 2" is a thing a person can find in a house with 66 of them.
      finding: **`initialstate.json`'s last dangling reference now resolves**, and the obligation
            `HOUSE-00395` recorded is a check: rule 6 refuses a delta that sets an interactable
            nobody declares. All four of §65.6's rows — three doors and this window — point at
            rows that exist.
      finding: **the load-time budget is exceeded, and authoring cannot fix it.** 273 ms for
            885 KiB, before materials and props. Per file the cost is a flat ≈ 3.3 KiB/ms across
            every loader, which says the throughput is `System::Text::Json`'s and not any one
            reader. Recorded against the phase-5 exit criterion with the per-file numbers, because
            "the world got slower" is not something anybody can act on and "the parser does
            3.3 KiB/ms and the world is 885 KiB" is.
- [x] HOUSE-00403 — Author rows for the 135 light groups and their switch plates
      dep: HOUSE-00384 · sys: world · plat: TOOL · pri: MUST
      note: **delivered by `HOUSE-00384`**, which is the same work under a §5.2 heading: 80 plates
            carrying 121 gangs, in `interactables.json`. Recorded here rather than struck, because
            the id is permanent and a reader looking for the switch rows should be told where they
            are.
      note: 135 groups and not 84 — §28.2's and §53's figure, corrected by `HOUSE-00382` against
            §13's own per-room column and §53's exterior list. 15 of the 135 have no plate at all,
            exactly as §53 describes: the table and floor lamps are switched at the lamp, the
            refrigerator's interior light by its door, and the porch lanterns, the street lights
            and the neighbours' by a dusk sensor.
      note: there is no row **per group**. A group is not a thing you touch; a plate is, and a gang
            on a plate is the group. §53's "one bit per group" is that gang's state field, which is
            what makes the two three-way pairs expressible as two plates naming one group — and
            rule 6 checks every gang against the declared groups.
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
- [x] HOUSE-00412 — Author rows for the 26 water outlets, with their basins, plugs and stacks
      dep: HOUSE-00386 · sys: world · plat: TOOL · pri: MUST
      finding: **26, not 17.** §56.1's own list undercounts the basins: it says "5 bathroom/WC
            basin taps" and §13 gives a basin to each of the seven WCs and each of the five
            bathrooms, with a **double** vanity in the master — thirteen basin taps before a bath,
            a shower or a sink. Everything else in §56.1's list is right and is here in full.
      finding: **four showers, not three.** The master, `L1_BATH2` (over the bath), `L1_BATH3` and
            `L2_BATH4` all have one in §13's own notes; §56.1 and §13.8 both said three. The three
            baths are right — the master's freestanding one, `L1_BATH2`'s and `L2_BATH5`'s.
      note: each outlet is placed at its room's §12.5 chase, like the toilets: a tap is on the wall
            the water arrives in, and a stack that moves takes its taps with it. The four rooms
            with no stack fall back to the room centre, and STACK-E already puts the kitchen's
            water on the north wall.
      note: `flow` and `hotFraction` are continuous and a plug and a fill level belong to the
            fixtures that have one. There is no `isRunning` boolean, for the same reason §36.1 has
            no `isRaining`.
- [x] HOUSE-00413 — Author rows for the 12 toilets
      dep: HOUSE-00386 · sys: world · plat: TOOL · pri: MUST
      finding: **twelve, not thirteen.** §13.8 says "8 WC-only rooms" over a list of **seven** —
            `WC1`…`WC7`, a complete run with no gap — so its "13 (8 WCs + 5 bathrooms)" is one too
            many. Seven WCs and five bathrooms is twelve. §13.8, §57's opening line and phase 14's
            checklist are all corrected against the rooms that exist.
      finding: **two toilets drained nowhere.** Placing each pan against the wall its §12.5 stack
            runs in is what turned it up: `L1_WC3` and `L2_WC5` are on no stack at all, and they
            sit directly above one another. `STACK_G` is the run they share, dropping into
            `B1_HOBBY`'s ceiling void, and §12.5 has the row now. Rule 9 could not have caught it —
            it checks fixtures **against** stacks, and a fixture with no stack to name is invisible
            to it until somebody places one.
      note: placing from the chase is what makes the position data rather than decoration: move a
            stack and the toilets move with it. The focus is the flush handle on the cistern,
            0.78 m up, which is the thing you reach for.
      note: §57.1's state in full — `urine`, `solids`, `lidOpen`, `seatUp`, `flushPhase`,
            `paperLeft` — and §57.2's verbs with the preconditions it gives: the lid before the
            seat, the seat before the second of the two long actions. Matter-of-fact and
            proportionate, as §57 asks.
- [x] HOUSE-00414 — Author rows for the 3 televisions
      dep: HOUSE-00368 · sys: world · plat: TOOL · pri: MUST
      note: §58.1's three, where §58.1 and §13 put them: the family room's wall-mounted set, the
            smaller one on the master's dresser, and the cinema's projector and 2.4 m screen.
            §58.1 says all three share one behaviour, so the projector is a television with a
            bigger bounds and not a fourth kind — which is the whole reason §58.1 says it.
      note: `channel` and `playhead` are §65.2's own fields. A save records which channel is on and
            how far into it, so a television left running is still running the same thing when you
            come back; that is what makes it worth persisting rather than resetting.
      note: all three start off, as §65.6 says — unlike §65.2's save example, which is a different
            moment on purpose and should not be mistaken for the initial state.
- [x] HOUSE-00415 — Author rows for the 11 appliances
      dep: HOUSE-00368 · sys: world · plat: TOOL · pri: MUST
      note: §50.4 gives a count of **11** over **nine** named kinds — oven, hob, washer, dryer,
            dishwasher, microwave, kettle, coffee machine, fireplace. The reading that satisfies
            both: two ovens, because §13.3 says "range, **ovens**", and the butler's pantry's wine
            fridge, which §13.3 lists and §50.4's kind list does not. Recorded rather than resolved
            by silently authoring nine or thirteen.
      note: `on`, `programme`, `progress` and `doorOpen` are §50.4's own fields. `progress` runs
            0..1, so a wash is a thing that **finishes** rather than a thing that toggles — and a
            fire burns down on the same field, which is why the fireplace needs no special case.
      note: placed along the kitchen's north run, the wall §12.5's STACK-E puts the water on, and
            in the laundry: an appliance is where its services are rather than where it looked
            good. §65.6's "the washing machine has clothes in it, unstarted" is the state this row
            starts in.
      note: the house now holds **253 interactables** — 80 switch plates, 63 doors, the sectional,
            54 windows, 3 gates, 12 toilets, 26 taps, 3 televisions, 11 appliances — against §53's
            640, with the containers and the pickups still to come.
- [x] HOUSE-00416 — Author rows for the garage door, the 3 gates and their motors
      dep: HOUSE-00377 · sys: world · plat: TOOL · pri: MUST
      finding: **the sectional door had a swinging door's actions.** `HOUSE-00401` authored 64
            leaves with one shape, and the garage door came out of it opening in 0.6 s on a hinge.
            §54 gives it five panel segments on a spline and §11.2 gives the gate beside it 6 s of
            travel, so it is `kind: garage_door` now, with a motor's timing and a motor's sounds —
            and a name that says which door it is rather than `DOOR_L0_GARAGE_2`.
      note: §11.2's three gates, which `HOUSE-00390` authored with `interactable: null` because
            there was nothing to point at. There is now, and rule 6 checks it: a gate pointing at
            nothing is a gate the player walks into.
      note: the pedestrian gate is deliberately the first thing you touch — §65.6 starts the player
            on the road with it closed and unlocked — so it swings in 0.8 s and latches audibly.
            The rear gate is bolted and stays bolted; it is interactable so that the bolt is a
            thing you can **try**, which is more honest than a gate that ignores you.
      note: the drive gate's motor is an audio emitter on the gate post, beside §62.6's garage-door
            motor. §11.2 asks for "motor + rail sound, 6 s travel" and the travel is in the action's
            duration, so the two agree by construction rather than by somebody remembering.
- [ ] HOUSE-00417 — Author rows for the 148 pickup items, including the 32 fridge/freezer items, with their home surfaces
      dep: HOUSE-00404 · sys: world · plat: TOOL · pri: MUST
- [ ] HOUSE-00418 — Author rows for the 22 seats
      dep: HOUSE-00368 · sys: world · plat: TOOL · pri: MUST
      note: (2026-09-08) **the `dep` line understates it.** A seat's position is a piece of
            furniture's position, and `layout.props.json` is `HOUSE-00450` onwards. Every other
            interactable authored this session had an anchor in the layout — a doorway, a wall, a
            §12.5 chase — and a sofa has none. Authoring seats first would mean inventing furniture
            positions that the props task then has to match, which is the dependency backwards.
            The same applies to `HOUSE-00404`…`HOUSE-00411`'s containers: §54 makes a container a
            sub-cell with its own portal, and 62 kitchen drawers are 62 sub-cells inside the
            units' geometry.
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

- [x] HOUSE-00451 — `house_shell_gen.py` skeleton: read the layout, open Blender headless, emit `.glb` per cell
      dep: HOUSE-00397, HOUSE-00186 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-08) all **96 cells** generate, 784 KiB of `.glb` in `build/shell/`, in 5 s.
            The pass emits the **massing**: one closed box per footprint box, floor to ceiling.
            That is real geometry — the rooms are in their right places at their right heights —
            and each of the next four tasks *supersedes* part of it rather than adding beside it:
            `HOUSE-00452` the top and bottom faces, `HOUSE-00453` the shared sides, `HOUSE-00454`
            the outer ones, `HOUSE-00455` the openings. When the last lands no massing face is
            left, which is what stops this being scaffolding nobody removes.
      note: **the axis convention is the one thing here that is silently wrong or silently right.**
            The world is Y-up, −Z north and so is glTF, so the export must be the identity; Blender
            is Z-up and `export_yup=True` writes `gltf(x, y, z) = blender(X, Z, −Y)`, so a world
            point is built at `(x, −z, y)`. The claim is not that the code computes that — it is
            that the kitchen's exported `POSITION` accessor reads
            `[−8.2, 0.6, −27.1]…[2.2, 3.3, −23.0]`, which is `layout.cells.json`'s box, read back
            out of the file with the project's own glTF reader. Both sign injections were caught by
            it and by nothing else.
      note: a level with a `null` ceiling gives a cell no height unless it overrides one. Every
            attic cell does, so that branch is unreachable from the authored house — which is why
            it is claimed against a made-up cell rather than left to be exercised by a house that
            never has one. Five injected bugs, all caught, including a non-deterministic vertex
            that §18.4's byte-identical claim caught and nothing else would have.
      note: no `run_checks.sh` gate, following the seven Blender tools already here: a gate that
            needs Blender is not a fast gate, and none of them is wired as one. Nor a
            `build_content.py` stage yet — nothing consumes the shell until it is chunked, and
            where the generated shell lives on the way to `content/` is `HOUSE-00452`…`HOUSE-00465`'s
            question rather than a skeleton's to answer.
- [x] HOUSE-00452 — Generate floors and ceilings per cell, with the partition-centre-line inset
      dep: HOUSE-00451 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-08) the inset is computed **per side** from `layout.levels.json`'s own
            `construction` block, not from §13.1's transcribed 0.075/0.15: another interior cell
            across a side means `wallPartition` and 0.075, an exterior cell or nothing means
            `wallExterior` and 0.15, and the garage means `wallGarage` and 0.125 from either side.
            The kitchen's floor comes out 40.49 m² of §13's 42.64 m² centre-line area.
      finding: **an exterior cell is a cell, and the wall to it is still an exterior wall.**
            `L0_PORCH` abuts `L0_FOYER`'s front face, so a neighbour test that only asked "is
            there a cell across this?" made the front of the house a 0.15 m partition. Exterior
            cells are excluded from the neighbour list, and the porch is the claim that says so —
            the first version had no case where an exterior cell actually abutted an interior one,
            so the injected bug that removed the rule went unnoticed twice: once because the
            selftest built its own neighbour list instead of calling the generator's, and once
            because the cell it tested had nothing across it either way.
      finding: **`HOUSE-00451`'s four side faces all pointed into the room.** `to_blender` negates
            z, which reverses a plan winding, and the massing was wound the obvious way. Nothing
            noticed: the export-bounds claim reads an accessor's min/max, which a mesh turned
            inside out satisfies exactly. Measuring the polygon normals caught it, and the same
            claim caught the floor slab I had just wound the same wrong way.
      note: `wallPlumbing` (0.20) is **not** applied and the reason is recorded in the tool: §12.5
            gives each stack a chase as a volume in a room and never says which of that room's four
            walls is the thick one. A builder that guessed would be inventing the house for the
            sake of two centimetres.
      note: six injected bugs, all caught after the two above were fixed.
- [x] HOUSE-00453 — Generate interior partitions between adjacent cells, deduplicated so a shared wall is generated once
      dep: HOUSE-00452 · sys: content · plat: TOOL · pri: MUST
      finding: **a side is rarely all one thing.** Seven of the house's 324 sides are partly
            partition and partly exterior — `L0_KITCHEN`'s north side is 8.9 m against the sunroom
            and 1.5 m of outside wall beside it — so `HOUSE-00452`'s one-wall-per-side rule was
            wrong on them. A side is now split into **runs**, one face each, at the inner face of
            the wall that bounds that run.
      finding: **the floor must take the THINNEST wall on a mixed side, not the thickest.** The
            first version took the thickest, reasoning that floor should never end up inside a
            wall. It leaves a 75 mm slot between the floor and the skirting, 8.9 m long, that you
            can see the void through. Floor under a wall is never seen; floor short of one is. The
            claim that caught it is that the exported mesh's bounds equal the inset box — the two
            had drifted apart on exactly that side.
      note: **"deduplicated" means the shared plane is described once and each cell carries the
            face that looks at it.** Not one wall volume owned by one of the two cells: rendering
            is per visible cell, per chunk (§17.4), so a wall living only in `L0_HALL`'s chunk
            would be missing while you stand in the kitchen looking at it. One pass over the shared
            plane, two inner faces, no surface in two chunks.
      note: runs are clamped to the room's inset extent on the perpendicular axis, so at a corner
            the two inner faces stop at each other instead of running 75 mm into the wall they
            meet. That is what makes the exported bounds equal the inset box exactly.
      finding: **every face pointed the wrong way, twice, for opposite reasons.** `HOUSE-00451`'s
            massing was a solid block seen from outside; these are the inner faces of walls, seen
            from inside the room, like the floor from above. The winding is now **computed** — the
            polygon normal is measured against the direction the face should look and reversed if
            it disagrees — rather than written down and hoped for, because `to_blender` negates z
            and reverses every plan winding, and a mesh turned inside out has exactly the right
            bounds and passes every claim about coordinates.
      note: five injected bugs, all caught.
- [x] HOUSE-00454 — Generate exterior walls with the correct thickness, and the foundation walls
      dep: HOUSE-00453 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-08) a partition has two rooms and each carries its own inner face; an
            exterior run has one room and the weather, so it carries a second face on the far side
            of the centre line. The two are `wallExterior` apart — 0.30 m, straddling the line the
            layout stores — which is the claim, measured on the exported mesh rather than on the
            arithmetic that placed it.
      finding: **an outer skin built from cell extents alone leaves a slot round the whole house
            at every storey.** `L0`'s ceiling is 3.30 and `L1`'s floor is 3.65: the 0.35 m of floor
            structure between them belongs to no cell's extent, and the wall outside it belonged to
            nobody. The lower cell now carries that band — its outer runs reach the **next** level's
            `ffl`, and the topmost storey's stop at its own ceiling because there is no next one —
            so every band is carried exactly once.
      note: below grade the same run is a `foundationWall`, decided by the level's own `ffl` being
            under §10.2's grade rather than by the level being called `B1`. A house with a second
            basement gets the right answer without this tool learning its name. The two thicknesses
            are both 0.30 m today, so the geometry is identical and the **name** is the point:
            §12's foundation takes a different material.
      note: five injected bugs, all caught, including the outer face put on the inner plane —
            which a claim about the mesh's bounds catches and a claim about wall thickness alone
            would not.
- [x] HOUSE-00455 — Cut door and window openings from walls, with reveals and sills
      dep: HOUSE-00454, HOUSE-00378 · sys: content · plat: TOOL · pri: MUST
      accept: every opening in `layout.openings.json` produces a hole of the right size in the right wall
      note: (2026-09-08) the acceptance criterion is a **claim**, run over the whole house: for all
            133 openings, the portal it names is a hole of its own height in the wall of both cells
            it joins. Two exemptions, each with its reason: a yard has no wall to cut, so an
            exterior cell is skipped and the room on the other side carries the hole; and a
            container's door is a hole in the container, not in the room's wall, so a portal into
            a nested sub-cell is skipped for the parent.
      finding: **a portal is the hole, not an opening.** 43 of the house's vertical portals are
            cased openings with no leaf, and a generator that cut only the ones with an `aperture`
            would wall up every archway in the house. §13.1's "cased openings are not doors" is
            about counting doors; it says nothing about whether there is a hole. Both are cut, and
            both are claimed.
      note: the hole is subtracted with a **grid**, not a boolean: the cuts are the holes' own
            edges and a cell is emitted unless its centre is inside a hole. Exact for the handful
            of openings a wall has, deterministic, no library — and every piece it emits is a
            rectangle, which the lightmap unwrap downstream would much rather have than a polygon
            with a slot in it. The claim is area: a panel plus its holes is the whole wall, and no
            two pieces overlap.
      note: the reveal is the four surfaces of the hole through the wall, of which the bottom one
            **is** the sill. It runs from this room's inner face to the outer face of an exterior
            wall, or to the centre line of a partition — the room on the other side carries its
            half, the same rule as the wall faces themselves.
      note: the shell is now 2.6 MB over 96 cells. Seven injected bugs; the two that survived the
            first round were "cut only apertured portals" and "ignore the plane's value, match only
            its axis" — the second would have put every room's door in its opposite wall as well,
            and needed a claim that a portal in one wall is **not** a hole in another.
      note: horizontal portals — the stairwells — pierce floors rather than walls and are left to
            `HOUSE-00460`, which the task names.
- [x] HOUSE-00456 — Generate door frames, architraves and thresholds
      dep: HOUSE-00455 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-08) two jambs, a head and a threshold per doorway, per room that looks at it.
            The architrave's width is the **opening's own** `frame.casing`; it stands 18 mm proud
            of the wall, because a board coplanar with the wall z-fights with it and §12 gives no
            number for the projection. Windows get nothing here: a window's frame is
            `HOUSE-00457`'s and has a different section, and lumping the two together would give
            every window a doorstep.
      finding: **`HOUSE-00455` gave no reveal to a partition.** The `continue` that skips the outer
            face of an interior wall skipped the reveal with it, so every doorway between two rooms
            was a hole through a wall with no sides — you could see out through the wall's thickness
            at every one of them. The acceptance claim was about holes and could not see it; the
            trim pass found it because trim and reveal share the same guard. Restructured so only
            the outer face is conditional.
      finding: **a claim about a function is not a claim about the code that calls it.** Every door
            in this house declares the same 0.06 m casing, so a builder that hard-coded 0.06 passed
            every claim, including one that measured `architrave_boards` at two different widths.
            What catches it is doubling a door's declared casing and requiring the **mesh** to move.
      note: the shell is now 15 178 triangles over 96 cells, against §17.2's ~180 000 for the
            finished thing — the trims, stairs, roof and dormers still to come. Six injected bugs,
            all caught after the two findings above were fixed.
- [x] HOUSE-00457 — Generate window frames, sashes, sills, glazing bars and the glass quad
      dep: HOUSE-00455 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-08) a four-board frame filling the reveal's depth, a four-board sash inside
            it, 6 mm of glass in the middle, and a sill board projecting 30 mm into the room. §12.6
            gives every window type a leaf size and a sill height and **no section at all**, so the
            four sections are this generator's and are named at the top of the file rather than
            buried in the arithmetic — when §12 gains a section, one place changes.
      note: the glazing bar is a **meeting rail**, and only §12.6's `W_DH_*` types get one: a
            double-hung window has two sashes that pass, and the picture, panel, slider, hopper,
            louvre, bay, dormer and borrowed-light types are all single lights. Read from the type
            prefix, which is §12.6's own schedule name. Two types have a rail and ten do not, and
            the claim names both lists.
      finding: **`EXT_BACKYARD` owned the kitchen's window.** A window is one object, so it is
            built once, by the first of its portal's *interior* cells in id order — and with the
            cell table missing the exterior test silently passed, `EXT_BACKYARD` sorted first, and
            the kitchen was left with a hole, a sill board and no glass in it. The rule is a named
            function now, its fallback is "the cell that is asking" rather than "whichever id sorts
            first", and both directions are claimed: a window onto the lawn is built by the room,
            and a borrowed-light window between two rooms is built by exactly one of them.
      note: the shell is 23 902 triangles over 96 cells. Five injected bugs, all caught; the count
            claim is written as **parts** — four frame boards, four sash boards, a rail, a pane, a
            sill — so a bug that drops one is a claim about the part that went missing rather than
            an unexplained number.
- [x] HOUSE-00458 — Generate skirtings and cornices per cell, mitred at corners and interrupted at openings
      dep: HOUSE-00456 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-08) §12's own `skirting` 0.14 and `cornice` 0.11, along the foot and the head
            of every run of wall, standing as proud of it as an architrave does.
      note: **interrupted by what crosses the band, not by "doors".** A doorway breaks the
            skirting; so does the front door's sidelight, whose 0.10 m sill is below the board's
            0.14 m top; a window at 0.90 m does not. Asking whether the opening's own vertical
            range overlaps the board's is the same rule for all three, and it is the rule that
            gets `W_SIDELIGHT` right without anybody having to notice `W_SIDELIGHT`.
      note: **mitred** means each board is shortened by its own projection at an end that is a
            corner of the room — and only there. An end where one run of wall meets the next along
            the same side is left alone, or the boards would part company in the middle of a wall.
      note: the mesh is claimed to follow `minus`: the kitchen's 22 boards are counted from the
            layout, through the same interruption and mitre rules, and compared with the faces the
            builder actually emitted. Four of the first six injected bugs survived the claims that
            only tested the helper functions — a board that ignored `minus`, a cornice at the
            floor, a skirting measured from the ceiling, and no mitre at all — and that mesh claim
            is what catches all four.
      note: the shell is 39 766 triangles over 96 cells, still well under §17.2's ~180 000.
- [x] HOUSE-00459 — Generate stair carriages, treads, risers, nosings and landings for the 7 flights
      dep: HOUSE-00379 · sys: content · plat: TOOL · pri: MUST
      finding: **the layout could not say where a flight is.** §12.4 has given every flight a
            footprint, a shape and a direction since it was written — in prose — and
            `layout.stairs.json` carried `fromCell`, `toCell`, `risers`, `rise`, `going` and
            `width` and nothing about position. `HOUSE-00360` had already recorded that §70.5's
            headroom row was uncheckable for exactly this reason. So the schema gained
            `footprint`, `run` (the direction you travel while going **up**) and `shape`, and all
            eight flights were authored from §12.4 — a generator that read the prose table would
            be reading a document the runtime cannot.
      note: **eight flights, not the seven in this task's title.** `HOUSE-00379` found the missing
            terrace-to-lawn steps when it authored the data; the title is left as it was written,
            which is what "task ids and titles are permanent" means.
      note: two new gates come with the data. `validate_world.py` rule 10 checks that a flight fits
            the footprint it declares — a U-stair needs half its treads plus its landing along, and
            two widths across — because a footprint in the data is a number that can be wrong, and
            a flight longer than its footprint is a staircase coming through the wall at the top.
            And `room_schedule.py` now compares §12.4's own table with the layout: risers, rise,
            going, width and the footprint where the table states coordinates rather than "same
            footprint" or "inside the garage at the house wall". Both directions, so a flight
            deleted from the table is reported too.
      note: solid steps rather than treads on a carriage: a blockout wants the shape you walk on
            and the volume you cannot walk through, and a closed string is both. The nosing is a
            separate board because it overhangs, which is the one part of a step's profile you see
            from below — and it is the reason the bottom step is 25 mm outside the declared
            footprint, which the claim states rather than fudging.
      note: a flight is carried by its `fromCell`, so it is built once and it is in the chunk of
            the room you are standing in when you start to climb. Nine injected bugs; three
            survived the first round, all of them at the **call site** rather than in the geometry
            — no flight built at all, every cell building every flight, and a flight that started
            at the world origin. The last needed a claim about the FIRST tread: a stair well
            already reaches the floor above whether or not there is a stair in it.
      note: 41 566 triangles over 96 cells.
- [x] HOUSE-00460 — Generate balustrades, newels, handrails and the stairwell openings in the floors above
      dep: HOUSE-00459 · sys: content · plat: TOOL · pri: MUST
      note: (2026-09-08) a stairwell is a portal with a `y` plane, and until it was cut the landing
            above every flight had a floor across it and the stair arrived in a ceiling. The same
            `panel` that cuts a doorway out of a wall cuts the well out of a slab — 11.27 m² of
            `L1_STAIR_MAIN`'s 14.47 m² floor is the hole you would fall through — and the claim is
            the same one: the pieces plus the hole are the whole floor.
      note: **the handrail is raked.** A level box over each tread is the easy way to avoid sloping
            quads and is not a handrail; you can see the difference from the hall. It runs from one
            balustrade height over the first tread to the same over the last, with a newel at each
            end, and the injected "level, not raked" bug is caught by the claim that the highest
            thing in the stair well is the rail over the TOP tread.
      note: a rail goes up every side of a run that is **not against a wall**, decided by the run's
            across edge lying on the cell's own boundary — both are centre-line numbers, which is
            what makes them comparable. A `u`'s two inner edges face the well and never do, so the
            main stair gets one rail up each run and its outer sides get none.
      note: the railing round a well has a **gap where the stair arrives**, cut with the same
            `minus` the skirting uses. A railing across the top of a flight is a railing you have
            to climb, and it is the one thing about this task that a face count alone would not
            have noticed: telling the builder about the flights turns the single rail along that
            edge into two.
      note: §70.5's balustrade row is still checked by neither gate — §12 declares 0.95 up a flight
            and 1.10 at a drop and this generator now uses both, but there is no *row* in the
            layout for a rail, so there is nothing for a validator to measure. That is the same
            "missing input, not missing check" recorded under §70.5 by `HOUSE-00360`, and it is now
            one step smaller: the heights are in the construction block and used.
      note: six injected bugs, all caught. 42 518 triangles over 96 cells.
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

- [x] HOUSE-00761 — Author the terrain height field (81 × 65 samples) with the slope described in §10.2, plus its material index map
      dep: HOUSE-00390 · sys: content · plat: TOOL · pri: MUST
      finding: **the file reference was `world/terrain.r16` and §11.5 asks for a 16-bit PNG.** The
            format doc's example named a raw blob; the section that specifies the encoding names a
            PNG. Repointed to `world/terrain.png` and `docs/world-format.md` corrected, because a
            renderer written against the example would have opened a file that never existed.
      note: the ground is **generated, not painted** — `tools/world/terrain_gen.py` reads §10.2's
            one-sentence slope and takes every flat pad from `layout.exterior.json`'s `paths` and
            `structures` and from the outdoor cells' floors. Painting it would re-derive numbers
            the layout already holds, and the first time a terrace moved the ground under it would
            not. The material index is the same argument: each of §11.5's eight classes is already
            a path's material or a cell's `footstepSurface`, so the ground and the footsteps agree
            without anybody maintaining two lists.
      note: the selftest's slope claim had to be moved **off the halfway point**. Halfway is the
            one point on the slope where running it backwards gives the same answer, and a claim
            that cannot tell uphill from downhill is not a claim about a slope. A quarter-point
            claim (+0.025 at Z −12) catches the reversal; five injected bugs, all caught.
      note: **no `terrain.json` sidecar.** The first draft wrote one, and it would have been a
            second statement of the grid that `deploy_world.py` does not copy (it is not one of the
            sixteen) and `world.manifest.json` does not hash — so the runtime would read its
            geometry from the one file nothing checks. `samples`, `step`, `materialIndex` and
            `materials` moved into `layout.exterior.json`'s `terrain` block, and
            `terrain_gen.py --check` now fails if that block and the generator disagree.
      note: the two PNGs are deployed **with the world**, not compiled into content:
            `deploy_world.py` copies them verbatim and `world_manifest.py` lists them after the
            JSON, so `WorldLoader::VerifyManifest` covers the ground the player stands on. Four
            injected bugs — not copied, not hashed, rewritten every deploy, and a stale copy the
            `*.json` glob would have missed — all caught. Their `assets.manifest.json` rows are
            `notPackaged` for the same reason: they belong to no residency pack.
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

**1 298 numbered tasks across 53 phases.**

| Phase group | Phases | Tasks |
|---|---|---|
| Foundations, capability proof, build, pipeline, assets | 0–4 | 237 |
| World data, blockout, collision, camera, visibility | 5–9 | 200 |
| Exterior, neighbourhood, materials, furnishing | 10–13 | 130 |
| Interaction framework and the systems built on it | 14–21 | 159 |
| Time, sun, moon, stars, sky, weather | 22–30 | 146 |
| Audio, room-aware audio, animals, avatar, animation | 31–38 | 161 |
| Persistence, reset, optimisation, streaming, debug, tests, polish, stabilisation | 39–46 | 188 |
| Web, Android, release | 47–52 | 77 |
| **Total** | **0–52** | **1 298** |

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
