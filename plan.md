# CNA House Master Implementation Plan

`STATUS: AWAITING USER APPROVAL — IMPLEMENTATION FORBIDDEN`

No implementation task in this file may be started. This document and
[`cna-house.md`](cna-house.md) are the complete output of the planning pass. Work begins only
after the project owner replies with `APPROVED: START IMPLEMENTATION`.

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
| Compiled effects | `CNA_EASYGL_COMPILED_EFFECTS=ON` |
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
| 2 | Build skeleton and CI | 00121–00180 | 45 | `Game` clears the screen; HEADLESS tests run in CI |
| 3 | Content pipeline | 00181–00260 | 42 | glTF, PNG, WAV, SpriteFont and FX all compile and load |
| 4 | Asset provenance and licensing | 00261–00340 | 42 | Manifest tooling green; NOX imported; every source licence verified |
| 5 | World and floor-plan data | 00341–00450 | 80 | The full layout authored, validated and loaded |
| 6 | Blockout house geometry | 00451–00540 | 34 | The generated shell renders |
| 7 | Collision and player controller | 00541–00620 | 35 | You can walk the whole blockout |
| 8 | First-person camera | 00621–00660 | 14 | It feels right and is tested |
| 9 | Room/portal visibility | 00661–00760 | 37 | Culling correct, proved, and within budget |
| 10 | Exterior and property | 00761–00840 | 23 | Terrain, fences, gates, drive, garden |
| 11 | Neighbourhood background | 00841–00890 | 15 | The house is not floating in nothing |
| 12 | Materials and textures | 00891–00970 | 28 | The blockout reads as a building |
| 13 | Static furniture and dressing | 00971–01120 | 64 | Every room furnished to density |
| 14 | Interactable framework | 01121–01180 | 26 | The 12 behaviours and the data model |
| 15 | Doors and windows | 01181–01250 | 22 | Portals are dynamic |
| 16 | Lights and switches | 01251–01310 | 28 | The house can be lit — **first playable** |
| 17 | Containers | 01311–01360 | 13 | 214 things open with contents |
| 18 | Kitchen and refrigerator | 01361–01410 | 23 | |
| 19 | Plumbing and water | 01411–01460 | 19 | |
| 20 | Toilets | 01461–01490 | 14 | |
| 21 | Television and video | 01491–01530 | 14 | Both backends |
| 22 | Time | 01531–01560 | 11 | |
| 23 | Sun, glare, sun-clock | 01561–01600 | 18 | |
| 24 | Moon and stars | 01601–01640 | 19 | |
| 25 | Sky and clouds | 01641–01680 | 15 | |
| 26 | Weather core | 01681–01740 | 22 | |
| 27 | Rain | 01741–01790 | 16 | |
| 28 | Snow | 01791–01830 | 11 | |
| 29 | Storm, lightning, thunder | 01831–01870 | 14 | |
| 30 | Hail and wind | 01871–01910 | 20 | |
| 31 | Audio foundation | 01911–01990 | 30 | |
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

- [ ] HOUSE-00001 — Create the repository skeleton: directories per `cna-house.md` §17.5, with a `.gitkeep` in each empty one
      dep: — · sys: app · plat: ALL · pri: MUST
      files: (directories only)
      accept: every directory in §17.5 exists; `git status` is clean after the first commit
      verify: `tools/ci/check_layout.py` asserts the directory set
- [ ] HOUSE-00002 — Write `README.md`: what the project is, the XNA-only rule in three sentences, how to build, how to run, where the plan is
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
      accept: a new contributor can build from the README alone
      verify: manual, plus a CI check that the build commands in the README are the ones CI runs
- [ ] HOUSE-00003 — Choose and add the project licence and `NOTICE.md`
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
      accept: licence file present; `NOTICE.md` explains that content assets carry their own licences and points at `licenses/`
      verify: `verify_licences.py` step 0
- [ ] HOUSE-00004 — Add `.gitignore`: `build*/`, `content/` (except the committed baseline), `*.log`, editor dirs, `assets-src/**/.blend1`
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
      accept: a full build leaves `git status` clean
      verify: CI runs a build then `git diff --exit-code`
- [ ] HOUSE-00005 — Add `.editorconfig` and `.clang-format` matching CNA's own style (4 spaces, 110 columns, Allman for types)
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
      verify: `clang-format --dry-run -Werror` over the tree
- [ ] HOUSE-00006 — Add `AGENTS.md`/`CLAUDE.md` for this repository: the XNA-only rule, the build-directory rules, ccache, the ID convention, the "commit after each task" rule
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
      accept: it restates the openeggbert build rules and adds only project-specific rules
- [ ] HOUSE-00007 — ADR-0001: XNA-only interpretation and the four-tier deviation policy
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
      files: docs/decisions/ADR-0001-xna-only.md
      accept: reproduces `cna-house.md` §4 as a decision record with the alternatives considered
- [ ] HOUSE-00008 — ADR-0002: renderer selection (`OPENGLES3`), with the rejected alternatives and their reasons
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00009 — ADR-0003: two rendering tiers, and why Tier S must be complete alone
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00010 — ADR-0004: portal visibility with frustum reduction; stencil, PVS and BSP rejected with reasons
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00011 — ADR-0005: data-driven world; JSON schemas; the closed expression vocabulary
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00012 — ADR-0006: composition over ECS, with the entity-count argument
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00013 — ADR-0007: project-owned kinematic collision; Bullet/Jolt rejected; the swap-in seam named
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00014 — ADR-0008: delta save format, versioning and migration policy
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00015 — ADR-0009: 24-minute simulated day, with the 20-vs-24-vs-48 analysis
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00016 — ADR-0010: room-aware audio computed over the portal graph rather than by `Apply3D`
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00017 — ADR-0011: no third-party runtime dependencies; the candidate table and each verdict
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00018 — ADR-0012: asset licensing policy and the "no row, no build" rule
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00019 — Create `docs/xna-deviations.md` with the three seed rows (DEV-001…003) and its own format description
      dep: HOUSE-00007 · sys: — · plat: ALL · pri: MUST
      accept: each row names the symbol, the tier, the XNA-side reason and the call sites
- [ ] HOUSE-00020 — Write `tools/ci/check_xna_only.py`: reject `CNA/Graphics/*` engine includes, `CNA::Graphics::` references, unregistered `*EXT*` symbols, `CNA_CNAEXT=ON` in the CMake cache, and direct GL/Vulkan/SDL/WebGPU symbols
      dep: HOUSE-00019 · sys: ci · plat: CI · pri: MUST
      files: tools/ci/check_xna_only.py
      accept: (1) a deliberately-planted violation of each class is detected; (2) clean tree passes
      verify: the script's own self-test with 8 planted-violation fixtures
- [ ] HOUSE-00021 — Extend `check_xna_only.py` with the "no `isRaining`-style boolean" lint and the "no `std::filesystem` outside `SaveStore`" lint
      dep: HOUSE-00020 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00022 — Write `tools/ci/check_layout.py` — the directory-set and file-placement gate
      dep: HOUSE-00001 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00023 — Establish the naming conventions document: ids, files, namespaces, content names, JSON keys
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
      files: docs/conventions.md
      accept: covers cell/portal/light/interactable/asset/material/sound id grammars with examples and the regex each must match
- [ ] HOUSE-00024 — Define the C++ error-handling policy: `Result<T>` for recoverable, exceptions only at the `Game` boundary and for genuinely exceptional content failures, `assert` for invariants
      dep: HOUSE-00023 · sys: util · plat: ALL · pri: MUST
      files: docs/conventions.md, include/cnahouse/util/Result.hpp
- [ ] HOUSE-00025 — Define the logging policy and implement `util/Log`: levels, categories, rate limiting, ring buffer, file + stderr sinks
      dep: HOUSE-00024 · sys: util · plat: ALL · pri: MUST
      files: src/util/Log.cpp|hpp
      accept: (1) a message repeated 1 000× in a frame is logged once with a count; (2) categories can be filtered at runtime
      verify: unit LogTests.*
- [ ] HOUSE-00026 — Implement `util/Ids`: interned string ids with a stable 32-bit hash, a debug-only reverse map, and a compile-time literal form
      dep: HOUSE-00024 · sys: util · plat: ALL · pri: MUST
      accept: (1) collisions are detected and fatal at load; (2) `Id("L0_KITCHEN")` is constexpr-comparable
      verify: unit IdsTests.*
- [ ] HOUSE-00027 — Implement `util/Rng`: xoshiro256++ with explicit state save/restore, plus a `Bag<T>` shuffled-draw helper for round-robin sample selection
      dep: HOUSE-00024 · sys: util · plat: ALL · pri: MUST
      accept: (1) the same seed reproduces the same 10⁶ draws; (2) state round-trips through JSON
      verify: unit RngTests.*
- [ ] HOUSE-00028 — Implement `util/Json`: a thin, typed wrapper over `System::Text::Json` giving `RequireString/Int/Float/Vector3/Box/Array/Object` with path-carrying error messages
      dep: HOUSE-00024 · sys: util · plat: ALL · pri: MUST
      accept: an error names the file, the JSON path and what was expected
      verify: unit JsonTests.* with 20 malformed fixtures
- [ ] HOUSE-00029 — Implement `util/SmallVector` and `util/FixedString`, or decide against them after measuring; record the decision
      dep: HOUSE-00024 · sys: util · plat: ALL · pri: SHOULD
- [ ] HOUSE-00030 — Set up the git hooks / CI pre-commit equivalent running clang-format and the lint gates
      dep: HOUSE-00020, HOUSE-00022 · sys: ci · plat: CI · pri: SHOULD
- [ ] HOUSE-00031 — Write `docs/performance-log.md` with its row format and the first (empty) table
      dep: HOUSE-00001 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00032 — Write `docs/content-authoring.md`: the authoring conventions of `cna-house.md` §18.2 in checklist form for whoever makes an asset
      dep: HOUSE-00023 · sys: — · plat: TOOL · pri: MUST
- [ ] HOUSE-00033 — Write `docs/world-format.md`: the JSON schemas of §15 as a reference, with a worked example per file
      dep: HOUSE-00023 · sys: — · plat: TOOL · pri: MUST
- [ ] HOUSE-00034 — Decide and record the versioning scheme (`MAJOR.MINOR.PATCH+gHASH`) and where the version string lives
      dep: HOUSE-00001 · sys: app · plat: ALL · pri: MUST
- [ ] HOUSE-00035 — Add `CMakePresets.json` with the presets `linux-debug`, `linux-release`, `linux-asan`, `linux-ubsan`, `headless`, `gl33`, `web` (unbuilt for now)
      dep: HOUSE-00001 · sys: app · plat: ALL · pri: MUST
      accept: each preset sets `CCACHE_DIR`/`CCACHE_BASEDIR` launchers and the right build directory from the closed list
- [ ] HOUSE-00036 — Record the openeggbert build rules compliance checklist in `AGENTS.md`: one ccache, reuse build dirs, never build in `/tmp`, `~/deps` for third-party, watch RAM
      dep: HOUSE-00006 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00037 — Create `licenses/` with `THIRD-PARTY-ASSETS.md` as a generated stub and the generator's contract
      dep: HOUSE-00003 · sys: — · plat: TOOL · pri: MUST
- [ ] HOUSE-00038 — Define the issue/task workflow: one task = one commit, commit message references the `HOUSE-` id, statuses updated in this file
      dep: HOUSE-00006 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00039 — Define and document the screenshot-scene naming scheme used by the render tests and the docs
      dep: HOUSE-00023 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-00040 — Add `docs/asset-review/` with the hero-asset sign-off template (4 views, reference photo, verdict, reviewer, date)
      dep: HOUSE-00018 · sys: — · plat: TOOL · pri: MUST
- [ ] HOUSE-00041 — Record the definition of done for a task: builds, tests pass, lint green, docs updated, plan checkbox ticked, one commit
      dep: HOUSE-00038 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00042 — First commit: the skeleton, the documents, the gates
      dep: HOUSE-00001…HOUSE-00041 · sys: — · plat: ALL · pri: MUST
      accept: CI green on a fresh clone

---

## Phase 1 — CNA capability verification

**Goal.** Re-prove every claim in `cna-house.md` §5 against the actual checkout, settle the open
questions that block architecture, and record the evidence. Probes are built in
`/rv/data/development/github.com/openeggbert/cna-house/build-probe/` with a `p1-` file-name prefix
(never a per-ticket directory, never in `/tmp`, never in the scratchpad) and **deleted when their
finding is written down**.

**Exit.** `docs/cna-capability-report.md` exists with a row per claim, a verdict and the probe
that produced it; `BL-09` is settled; every probe binary is removed.

- [ ] HOUSE-00061 — Create `docs/cna-capability-report.md` with the claim table skeleton (one row per §5 row)
      dep: HOUSE-00042 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-00062 — Probe: build a minimal `Game` against `../cnanext` with `OPENGLES3` and confirm it opens a window, clears and presents
      dep: HOUSE-00061 · sys: app · plat: LNX · pri: MUST
      accept: 300 frames at the target resolution with no GL error and no validation warning
      verify: probe `p1-hello` output + a screenshot
- [ ] HOUSE-00063 — Probe: confirm `CNA_CNAEXT=OFF` genuinely removes the engine layer (no `CNA::Graphics` symbols in the binary)
      dep: HOUSE-00062 · sys: ci · plat: LNX · pri: MUST
      accept: `nm -C` on the linked binary finds zero `CNA::Graphics::` symbols
      verify: recorded in the capability report; becomes a permanent CI check (`HOUSE-00133`)
- [ ] HOUSE-00064 — Probe: `ContentManager` resolution order — build one asset as `.cnb` and one as `.xnb` with the same name and confirm `.xnb` wins
      dep: HOUSE-00062 · sys: content · plat: LNX · pri: MUST
- [ ] HOUSE-00065 — Probe: load a `Texture2D` from `.cnb`; verify dimensions, format and a byte-exact `GetData` round trip
      dep: HOUSE-00064 · sys: content · plat: LNX · pri: MUST
- [ ] HOUSE-00066 — Probe: load a `SpriteFont` built from a TTF through the `.spritefont` route; draw a string; verify glyph placement
      dep: HOUSE-00064 · sys: content · plat: LNX · pri: MUST
- [ ] HOUSE-00067 — Probe: load a `SoundEffect` from a 16-bit PCM WAV; play it; confirm audible duration
      dep: HOUSE-00064 · sys: audio · plat: LNX · pri: MUST
- [ ] HOUSE-00068 — Probe: confirm a **24-bit** PCM WAV is rejected (BL-06), and record the exact exception text
      dep: HOUSE-00067 · sys: audio · plat: LNX · pri: MUST
      accept: the failure mode is documented so the pipeline can assert against it
- [ ] HOUSE-00069 — Probe: convert one NOX 24-bit file to 16-bit with ffmpeg and confirm it loads and sounds right
      dep: HOUSE-00068 · sys: audio · plat: TOOL · pri: MUST
- [ ] HOUSE-00070 — Probe: build a simple static glTF through `cna-content` to `.cnb`, load as `Model`, draw with `BasicEffect`
      dep: HOUSE-00064 · sys: content · plat: LNX · pri: MUST
      accept: correct bounds, correct orientation, correct winding with `CullClockwise`
- [ ] HOUSE-00071 — Probe: confirm the glTF winding conclusion — render the same model with `CullClockwise` and `CullCounterClockwise` and record which is correct
      dep: HOUSE-00070 · sys: rendering · plat: LNX · pri: MUST
- [ ] HOUSE-00072 — Probe: `Model` with a real multi-bone hierarchy from `.cnb` — verify `Bones`, `ParentBone`, `Root` and `CopyAbsoluteBoneTransformsTo`
      dep: HOUSE-00070 · sys: content · plat: LNX · pri: MUST
- [ ] HOUSE-00073 — Probe: `Model::Meshes[i].BoundingSphere` is populated from `.cnb` (it is not from `.model.json` — BL-12)
      dep: HOUSE-00072 · sys: content · plat: LNX · pri: MUST
      accept: a non-degenerate sphere per mesh; if degenerate, record it as a new blocker and plan to compute bounds offline
- [ ] HOUSE-00074 — **Probe: skinned glTF → `.cnb` → `Model` with `SkinningData` on `Model::Tag`** (settles R-16)
      dep: HOUSE-00072 · sys: content · plat: LNX · pri: MUST
      accept: (1) `Tag` is non-null and castable to `SkinningData`; (2) `BindPose`/`InverseBindPose`/`SkeletonHierarchy` sizes agree; (3) the clip list is present and named
      verify: probe `p1-skin`, output recorded in the capability report
- [ ] HOUSE-00075 — Probe: animate that model through a hand-written clip evaluator and `SkinnedEffect::SetBoneTransforms`; confirm visible deformation
      dep: HOUSE-00074 · sys: animation · plat: LNX · pri: MUST
- [ ] HOUSE-00076 — Probe: `Model::getSkinsEXTProperty()` on a two-skin glTF; confirm the multi-skin path and record whether we need it
      dep: HOUSE-00074 · sys: content · plat: LNX · pri: SHOULD
- [ ] HOUSE-00077 — Probe: `SkinnedEffect` bone-count limit — confirm 72 accepted, 73 throws
      dep: HOUSE-00075 · sys: rendering · plat: LNX · pri: MUST
- [ ] HOUSE-00078 — Probe: `DualTextureEffect` — albedo × lightmap on a quad with two UV channels; confirm the second channel reaches the effect
      dep: HOUSE-00070 · sys: rendering · plat: LNX · pri: MUST
      accept: a checker albedo × a gradient lightmap produces the analytic product within 2/255
- [ ] HOUSE-00079 — Probe: multi-pass additive lighting — draw the same chunk twice with `Opaque` then `Additive` + `DepthStencilState` depth-equal, and confirm no z-fighting and correct addition
      dep: HOUSE-00078 · sys: rendering · plat: LNX · pri: MUST
      accept: the sum is exact; a depth-equal second pass draws every pixel of the first
- [ ] HOUSE-00080 — Probe: `AlphaTestEffect` cutoff behaviour and two-sided rendering for foliage
      dep: HOUSE-00070 · sys: rendering · plat: LNX · pri: MUST
- [ ] HOUSE-00081 — Probe: `EnvironmentMapEffect` with a baked `TextureCube`; confirm sampling and the Fresnel term
      dep: HOUSE-00070 · sys: rendering · plat: LNX · pri: SHOULD
- [ ] HOUSE-00082 — Probe: `BasicEffect` three directional lights + ambient + specular + fog, all simultaneously
      dep: HOUSE-00070 · sys: rendering · plat: LNX · pri: MUST
- [ ] HOUSE-00083 — **Probe: `RenderTarget2D` with `SurfaceFormat::Single`, 2048², `DepthFormat::Depth24` — create, render depth, bind as an effect texture, read back** (settles `BL-09` / `Q-01`)
      dep: HOUSE-00062 · sys: rendering · plat: LNX · pri: MUST
      accept: either it works (record: Tier E uses a float shadow map) or it fails (record the exact error; Tier E packs depth into RGBA8)
      verify: probe `p1-rtsingle`; the answer is written into `docs/cna-capability-report.md` and `cna-house.md` §6 BL-09 is updated
- [ ] HOUSE-00084 — Probe: `RenderTarget2D` `Color` with mips and MSAA; bind, clear, draw, unbind, sample
      dep: HOUSE-00083 · sys: rendering · plat: LNX · pri: MUST
- [ ] HOUSE-00085 — Probe: confirm `Clear(ClearOptions::Stencil)` is ignored and `ReferenceStencil` has no effect (BL-02), and record it
      dep: HOUSE-00084 · sys: rendering · plat: LNX · pri: MUST
- [ ] HOUSE-00086 — Probe: confirm EasyGL MRT attachment 1 stays black (BL-03), and record it
      dep: HOUSE-00084 · sys: rendering · plat: LNX · pri: MUST
- [ ] HOUSE-00087 — **Probe: compile a trivial `.fx` through `cna-content --format xnb --fx-compiler <fxc> --fx-compiler-launcher wine`, load it as `Effect`, and draw with it**
      dep: HOUSE-00062 · sys: content · plat: LNX · pri: MUST
      accept: (1) the build succeeds; (2) `CompiledEffects` is true; (3) a named technique is selectable; (4) a parameter set changes the output
      verify: probe `p1-fx`; if it fails, `BL-04` is escalated and Tier E is deferred without blocking anything
- [ ] HOUSE-00088 — Probe: a two-technique `.fx` with techniques switched by name per draw, mirroring SAMPLE-038
      dep: HOUSE-00087 · sys: rendering · plat: LNX · pri: MUST
- [ ] HOUSE-00089 — Probe: `SpriteBatch::Begin(effect)` with a compiled effect
      dep: HOUSE-00087 · sys: rendering · plat: LNX · pri: SHOULD
- [ ] HOUSE-00090 — Probe: `OcclusionQuery` — visible and occluded quads; record whether `PixelCount` is a real count or a boolean on this driver at `OPENGLES3`
      dep: HOUSE-00062 · sys: rendering · plat: LNX · pri: MUST
      accept: the boolean/count verdict is recorded and drives the N×N grid design
- [ ] HOUSE-00091 — Probe: the same under `OPENGL33` to confirm a real `GL_SAMPLES_PASSED` count, validating the grid approximation later
      dep: HOUSE-00090 · sys: rendering · plat: LNX · pri: SHOULD
- [ ] HOUSE-00092 — Probe: `DynamicVertexBuffer` + `SetData(..., SetDataOptions::Discard)` at 2 000 quads per frame; measure the cost
      dep: HOUSE-00062 · sys: rendering · plat: LNX · pri: MUST
      accept: a number in ms, recorded in the performance log; it sizes the particle budget
- [ ] HOUSE-00093 — Probe: `DrawInstancedPrimitives` — does it work on EasyGL, and is it faster than N draws for 200 identical props?
      dep: HOUSE-00092 · sys: rendering · plat: LNX · pri: SHOULD
      accept: a measurement and a verdict; if it works well it becomes the vegetation and neighbourhood path
- [ ] HOUSE-00094 — Probe: 32-bit index buffers on EasyGL with a > 65 535-vertex chunk
      dep: HOUSE-00062 · sys: rendering · plat: LNX · pri: MUST
- [ ] HOUSE-00095 — Probe: `AudioListener`/`AudioEmitter`/`Apply3D` — pan and attenuation across a 20 m sweep; record the curve
      dep: HOUSE-00067 · sys: audio · plat: LNX · pri: MUST
      accept: the measured attenuation curve is recorded so our own gain model can be calibrated against it
- [ ] HOUSE-00096 — Probe: confirm `DopplerScale = 0` and zero velocities produce no pitch change (BL-11)
      dep: HOUSE-00095 · sys: audio · plat: LNX · pri: MUST
- [ ] HOUSE-00097 — Probe: concurrent `SoundEffectInstance` count — find the practical ceiling on this host
      dep: HOUSE-00095 · sys: audio · plat: LNX · pri: MUST
      accept: the number, recorded; the voice budget is set from it (32 is the design target)
- [ ] HOUSE-00098 — Probe: `Video` + `VideoPlayer::GetTexture()` on a transcoded test clip; confirm frame advance and audio
      dep: HOUSE-00062 · sys: rendering · plat: LNX · pri: MUST
- [ ] HOUSE-00099 — Probe: `VideoPlayer` behaviour with `CNA_ENABLE_VIDEO=OFF` — confirm `NotSupportedException` and that the rest still links (BL-05)
      dep: HOUSE-00098 · sys: rendering · plat: LNX · pri: MUST
- [ ] HOUSE-00100 — Probe: `Mouse::GetState` + `SetPosition` recentring loop; measure the delta accuracy and any drift over 10 000 frames
      dep: HOUSE-00062 · sys: player · plat: LNX · pri: MUST
- [ ] HOUSE-00101 — Probe: `Keyboard`, `GamePad`, and the presence/behaviour of `TouchPanel` on desktop
      dep: HOUSE-00100 · sys: player · plat: LNX · pri: MUST
- [ ] HOUSE-00102 — Probe: `StorageDevice`/`StorageContainer` — write, read, list and delete a file; record the resolved path
      dep: HOUSE-00062 · sys: persistence · plat: LNX · pri: MUST
- [ ] HOUSE-00103 — Probe: `System::Text::Json` — serialise and deserialise a nested object with arrays and floats; check round-trip precision
      dep: HOUSE-00102 · sys: persistence · plat: LNX · pri: MUST
- [ ] HOUSE-00104 — Probe: `BoundingFrustum::GetCorners`, `Intersects(BoundingBox)`, `BoundingBox::CreateFromPoints`, `Ray::Intersects` — verify against analytic answers
      dep: HOUSE-00062 · sys: visibility · plat: LNX · pri: MUST
- [ ] HOUSE-00105 — Probe: build and run under the `HEADLESS` renderer; confirm `Update` runs with no window and no GPU
      dep: HOUSE-00062 · sys: app · plat: CI · pri: MUST
      accept: a 600-frame headless run with no display server (`unset DISPLAY`)
- [ ] HOUSE-00106 — Probe: measure `EffectPass::Apply()` cost and the cost of 1 000 small `DrawIndexedPrimitives` calls, to calibrate the draw budget
      dep: HOUSE-00062 · sys: rendering · plat: LNX · pri: MUST
- [ ] HOUSE-00107 — Probe: `Texture2D` upload bandwidth for a 4 MB residency budget; confirm the per-frame promotion budget is realistic
      dep: HOUSE-00065 · sys: content · plat: LNX · pri: SHOULD
- [ ] HOUSE-00108 — Probe: EasyGL `DebugSimulateContextLoss` on desktop; confirm resources can be rebuilt from CPU state
      dep: HOUSE-00084 · sys: rendering · plat: LNX · pri: SHOULD
      accept: it is a supported path we can test against on Linux, de-risking the Web port
- [ ] HOUSE-00109 — Probe: anisotropic filtering availability and its visual effect at grazing angles on the floor
      dep: HOUSE-00065 · sys: rendering · plat: LNX · pri: OPT
- [ ] HOUSE-00110 — Probe: `SurfaceFormat` support survey — which formats can be created as textures and as render targets on this driver
      dep: HOUSE-00083 · sys: rendering · plat: LNX · pri: MUST
- [ ] HOUSE-00111 — Probe: DXT compressed texture support and whether the pipeline's DXT output stays compressed on EasyGL
      dep: HOUSE-00110 · sys: content · plat: LNX · pri: SHOULD
      accept: a verdict that sizes the texture-memory budget
- [ ] HOUSE-00112 — Write `docs/cna-capability-report.md` in full from the probe results, with a verdict per claim and a link to each probe's recorded output
      dep: HOUSE-00062…HOUSE-00111 · sys: — · plat: LNX · pri: MUST
- [ ] HOUSE-00113 — Update `cna-house.md` §5, §6 and §76 with anything the probes changed; in particular settle BL-09/Q-01
      dep: HOUSE-00112 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00114 — Delete every probe binary and object from `build-probe/`; keep only the probe sources and a one-paragraph README per probe
      dep: HOUSE-00112 · sys: — · plat: LNX · pri: MUST
      accept: `du -sh build-probe/` is under 2 MB
- [ ] HOUSE-00115 — Record in `docs/cna-capability-report.md` a "if you change renderer, re-run these" list
      dep: HOUSE-00112 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00116 — Probe: measure a full clean CNA build time and an incremental one with ccache warm, to set expectations for later sessions
      dep: HOUSE-00062 · sys: — · plat: LNX · pri: SHOULD
- [ ] HOUSE-00117 — Probe: confirm the `HEADLESS` and `OPENGLES3` builds can share one `build/` tree via presets without full reconfigures thrashing
      dep: HOUSE-00105 · sys: — · plat: LNX · pri: SHOULD
      accept: separate build directories from the closed list are used per renderer; document which
- [ ] HOUSE-00118 — Probe: run the relevant subset of CNA's own ctests (graphics stock effects, content, audio) once, to confirm the checkout is healthy before depending on it
      dep: HOUSE-00062 · sys: — · plat: LNX · pri: SHOULD
      accept: pass/fail recorded; a failure here is an upstream issue, not ours
- [ ] HOUSE-00119 — Write the "CNA facts that must never be assumed again" section of the capability report: the five things that most surprised us
      dep: HOUSE-00112 · sys: — · plat: ALL · pri: MUST
- [ ] HOUSE-00120 — Phase-1 review: confirm no design in `cna-house.md` now rests on an unproven CNA claim
      dep: HOUSE-00113 · sys: — · plat: ALL · pri: MUST

---

## Phase 2 — Build skeleton and CI

**Goal.** The real application skeleton: CMake, the `Game` subclass, the service container, the
system update order, the settings file, the logging, and a CI that runs lints and headless tests.

**Exit.** `cna-house` opens a window, clears to a known colour, draws a version string with
`SpriteFont`, exits cleanly; CI runs lint + unit + headless integration on every push.

- [ ] HOUSE-00121 — Root `CMakeLists.txt`: project, C++23, the CNA/sharp-runtime cache variables of `cna-house.md` §8.1, `add_subdirectory(../cnanext CNA_BUILD)`
      dep: HOUSE-00120 · sys: app · plat: ALL · pri: MUST
      accept: configures and builds against the sibling checkouts with no vendoring
- [ ] HOUSE-00122 — CMake option `CNAHOUSE_TIER_E` (default ON where `CNA_EASYGL_COMPILED_EFFECTS` is on) and `CNAHOUSE_DEBUG_TOOLS`
      dep: HOUSE-00121 · sys: app · plat: ALL · pri: MUST
- [ ] HOUSE-00123 — CMake: the `cnahouse_core` library and the `cna-house` executable, with the `src/` subdirectory structure
      dep: HOUSE-00121 · sys: app · plat: ALL · pri: MUST
- [ ] HOUSE-00124 — CMake: warnings-as-errors, `-Wall -Wextra -Wpedantic -Werror`, and the sanitizer presets
      dep: HOUSE-00123 · sys: app · plat: ALL · pri: MUST
- [ ] HOUSE-00125 — CMake: GoogleTest integration and the four test targets (`unit`, `integration`, `render`, `perf`)
      dep: HOUSE-00123 · sys: app · plat: CI · pri: MUST
- [ ] HOUSE-00126 — CMake: `cna_add_content` wiring for the two content trees (cnb and effects), gated on `CNAHOUSE_TIER_E` for the second
      dep: HOUSE-00123 · sys: content · plat: ALL · pri: MUST
- [ ] HOUSE-00127 — Implement `CnaHouseGame : Game` with `Initialize`, `LoadContent`, `UnloadContent`, `Update`, `Draw`, `Exit`
      dep: HOUSE-00123 · sys: app · plat: ALL · pri: MUST
      files: src/app/CnaHouseGame.cpp|hpp
      accept: opens a 1600×900 window titled "CNA House", clears to a known colour, exits on `Esc`
- [ ] HOUSE-00128 — Implement `Services`: a typed container constructing every system in dependency order and destroying in reverse
      dep: HOUSE-00127 · sys: app · plat: ALL · pri: MUST
      accept: (1) construction order is explicit and asserted; (2) a system cannot resolve one constructed after it
      verify: unit ServicesTests.*
- [ ] HOUSE-00129 — Define `ISystem` (`Update(const FrameContext&)`) and the fixed system order of `cna-house.md` §7.5
      dep: HOUSE-00128 · sys: app · plat: ALL · pri: MUST
- [ ] HOUSE-00130 — Implement the event queue: typed events, drained once per frame in a fixed order, with a per-frame cap and an overflow diagnostic
      dep: HOUSE-00129 · sys: app · plat: ALL · pri: MUST
      verify: unit EventQueueTests.*
- [ ] HOUSE-00131 — Implement `Settings`: load/save `settings.json`, defaults, versioning, migration, and typed accessors
      dep: HOUSE-00028, HOUSE-00127 · sys: app · plat: ALL · pri: MUST
      verify: unit SettingsTests.* incl. a v1→v2 migration fixture
- [ ] HOUSE-00132 — Implement the command-line parser: `--quality`, `--renderer`, `--headless`, `--scene`, `--seed`, `--time`, `--weather`, `--no-audio`, `--screenshot`
      dep: HOUSE-00127 · sys: app · plat: ALL · pri: MUST
- [ ] HOUSE-00133 — CI job: lint (`check_xna_only`, `check_layout`, clang-format) on every push
      dep: HOUSE-00020, HOUSE-00022 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00134 — CI job: build `linux-debug` and `linux-release`, run `unit`
      dep: HOUSE-00125 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00135 — CI job: build `headless`, run `integration`
      dep: HOUSE-00125, HOUSE-00105 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00136 — CI job: the symbol check that no `CNA::Graphics::` symbol is linked into the binary (from HOUSE-00063)
      dep: HOUSE-00063, HOUSE-00134 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00137 — CI job (nightly): `linux-asan` and `linux-ubsan` builds running `unit` + `integration`
      dep: HOUSE-00124 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00138 — CI job (nightly): `render` tests under `Xvfb` with `OPENGLES3`
      dep: HOUSE-00125 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00139 — Implement the frame timer and `FrameContext`: real dt, clamped dt, frame index, fixed-step accumulator
      dep: HOUSE-00129 · sys: app · plat: ALL · pri: MUST
      accept: a 250 ms hitch clamps to 4 physics substeps and does not spiral
      verify: unit FrameTimingTests.*
- [ ] HOUSE-00140 — Implement `IInputSource` and `KeyboardMouseSource`; no system may read `Keyboard`/`Mouse` directly
      dep: HOUSE-00132 · sys: player · plat: ALL · pri: MUST
      accept: the lint of HOUSE-00021 is extended to enforce it
- [ ] HOUSE-00141 — Implement the `Platform` capability struct and its desktop population
      dep: HOUSE-00140 · sys: app · plat: ALL · pri: MUST
- [ ] HOUSE-00142 — Implement `ContentRegistry`: content name ↔ asset id ↔ pack, loaded from `assets.manifest.json`
      dep: HOUSE-00028 · sys: content · plat: ALL · pri: MUST
- [ ] HOUSE-00143 — Implement `TextureCache`, `ModelCache`, `SoundCache`, `EffectCache` over `ContentManager`, with fallback assets and a load-failure policy
      dep: HOUSE-00142 · sys: content · plat: ALL · pri: MUST
      accept: a missing asset yields the typed fallback, logs once, and does not throw
      verify: unit ContentCacheTests.*
- [ ] HOUSE-00144 — Author the fallback assets: grey box model, mid-grey texture, magenta debug texture, silent sound
      dep: HOUSE-00143 · sys: content · plat: TOOL · pri: MUST
- [ ] HOUSE-00145 — Implement `SpriteFont` loading and a `TextRenderer` helper (measure, draw, drop shadow, virtual-unit scaling)
      dep: HOUSE-00143 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-00146 — Draw the version string and frame time in the corner; the first real thing on screen
      dep: HOUSE-00145 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-00147 — Implement `DebugDraw`: lines, wire boxes, wire spheres, wire frusta, filled quads, all on `BasicEffect`
      dep: HOUSE-00127 · sys: debug · plat: ALL · pri: MUST
      accept: compiled out entirely when `CNAHOUSE_DEBUG_TOOLS=OFF`
- [ ] HOUSE-00148 — Implement the counters system: named per-frame counters with min/avg/max over a 240-frame window
      dep: HOUSE-00147 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-00149 — Implement the CPU timing scopes and the per-system timing breakdown
      dep: HOUSE-00148 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-00150 — Implement the `F1` performance overlay (FPS, frame graph, system times, counters)
      dep: HOUSE-00149 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-00151 — Implement the screenshot command and the `--screenshot` flag (writes PNG via `Texture2D::SaveAsPng`)
      dep: HOUSE-00146 · sys: debug · plat: ALL · pri: MUST
- [ ] HOUSE-00152 — Implement `ISaveStore` with `DesktopSaveStore` over `StorageDevice`; atomic write, backup, read
      dep: HOUSE-00102 · sys: persistence · plat: LNX · pri: MUST
      verify: unit SaveStoreTests.* incl. an interrupted-write simulation
- [ ] HOUSE-00153 — Implement the crash boundary: catch at `Update`/`Draw`, log, emergency-save, offer restart
      dep: HOUSE-00152 · sys: app · plat: ALL · pri: MUST
- [ ] HOUSE-00154 — Implement `AudioSystem` skeleton: device init, graceful `NoAudioHardwareException` handling, master volume
      dep: HOUSE-00097 · sys: audio · plat: ALL · pri: MUST
      accept: `--no-audio` and a missing device both leave the game fully playable
- [ ] HOUSE-00155 — Implement the user-gesture audio gate (title screen "press any key") on every platform
      dep: HOUSE-00154 · sys: audio · plat: ALL · pri: MUST
- [ ] HOUSE-00156 — Implement the loading screen and the `MenuStack` skeleton
      dep: HOUSE-00145 · sys: ui · plat: ALL · pri: MUST
- [ ] HOUSE-00157 — Implement the quality-tier table and the auto-detect heuristic (from `GraphicsAdapter` and the capability profile)
      dep: HOUSE-00131 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00158 — Implement the `StateTracker`: skip redundant `BlendState`/`DepthStencilState`/`RasterizerState`/`SamplerState` sets, and count changes
      dep: HOUSE-00127 · sys: rendering · plat: ALL · pri: MUST
      verify: unit StateTrackerTests.* replaying a recorded command list
- [ ] HOUSE-00159 — Implement `Renderer` with the pass list of `cna-house.md` §7.5 as empty passes
      dep: HOUSE-00158 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00160 — Implement the capability query layer: one place that asks `SupportsCapability` and publishes a `RenderCaps` struct
      dep: HOUSE-00159 · sys: rendering · plat: ALL · pri: MUST
      accept: no other file calls `SupportsCapability`
- [ ] HOUSE-00161 — Implement the Tier S / Tier E selection from `RenderCaps` + settings, with a forced-Tier-S command-line switch
      dep: HOUSE-00160 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00162 — Implement `MaterialBinder` skeleton: material id → effect instance + parameters
      dep: HOUSE-00161 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00163 — Implement the shared `RasterizerState` objects (`CullClockwise` default for glTF-derived geometry, `CullCounterClockwise` for mirrored, `CullNone` for foliage/sky)
      dep: HOUSE-00071 · sys: rendering · plat: ALL · pri: MUST
- [ ] HOUSE-00164 — Add the first render regression test harness: fixed pose, fixed clock, render, compare PNG with tolerance
      dep: HOUSE-00151, HOUSE-00138 · sys: ci · plat: CI · pri: MUST
- [ ] HOUSE-00165 — Phase-2 review and commit; update the performance log with the empty-scene frame time
      dep: HOUSE-00121…HOUSE-00164 · sys: — · plat: ALL · pri: MUST

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
- [ ] HOUSE-00222 — Phase-3 review and commit; run `budget_report.py` for the first time
      dep: HOUSE-00181…HOUSE-00221 · sys: — · plat: ALL · pri: MUST

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
- [ ] HOUSE-00916 — Implement anisotropic filtering selection with the capability gate and the trilinear fallback
      dep: HOUSE-00109 · sys: rendering · plat: ALL · pri: SHOULD
- [ ] HOUSE-00917 — Render tests: a materials sheet scene plus 12 room poses at the new materials
      dep: HOUSE-00912 · sys: ci · plat: CI · pri: MUST
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
- [ ] HOUSE-01541 — Phase-22 review and commit
      dep: HOUSE-01531…HOUSE-01540 · sys: — · plat: ALL · pri: MUST

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
- [ ] HOUSE-02044 — Build the dog through the pipeline to `.cnb` and verify `SkinningData` and the clip list
      dep: HOUSE-02043, HOUSE-00074 · sys: content · plat: TOOL · pri: MUST
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

- [ ] HOUSE-02131 — Build the two base bodies through the pipeline and verify their `SkinningData` and bone lists
      dep: HOUSE-00294, HOUSE-00074 · sys: content · plat: TOOL · pri: MUST
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

- [ ] HOUSE-02211 — Implement TRS decomposition of the loaded clip keyframes at load time
      dep: HOUSE-00074 · sys: animation · plat: ALL · pri: MUST
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
- [ ] HOUSE-02217 — Implement `AnimationSet`: the named clip table per actor, loaded from the model
      dep: HOUSE-02212 · sys: animation · plat: ALL · pri: MUST
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
- [ ] HOUSE-02518 — Implement the Graphics tab, with options filtered by the live capability query
      dep: HOUSE-02516, HOUSE-00160 · sys: ui · plat: ALL · pri: MUST
      accept: no meaningless toggle is ever shown; a tier that cannot draw shadow maps does not offer them
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
- [ ] HOUSE-02787 — Verify `docs/xna-deviations.md` is complete and the lint enforces it
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
- [ ] HOUSE-03074 — Final XNA-only audit: the lint, the symbol check, and a manual review of `docs/xna-deviations.md`
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

**1 292 numbered tasks across 53 phases.**

| Phase group | Phases | Tasks |
|---|---|---|
| Foundations, capability proof, build, pipeline, assets | 0–4 | 231 |
| World data, blockout, collision, camera, visibility | 5–9 | 200 |
| Exterior, neighbourhood, materials, furnishing | 10–13 | 130 |
| Interaction framework and the systems built on it | 14–21 | 159 |
| Time, sun, moon, stars, sky, weather | 22–30 | 146 |
| Audio, room-aware audio, animals, avatar, animation | 31–38 | 161 |
| Persistence, reset, optimisation, streaming, debug, tests, polish, stabilisation | 39–46 | 188 |
| Web, Android, release | 47–52 | 77 |
| **Total** | **0–52** | **1 292** |

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

## Status

`STATUS: AWAITING USER APPROVAL — IMPLEMENTATION FORBIDDEN`

No task above may be started. `HOUSE-00001` begins only after
`APPROVED: START IMPLEMENTATION`.
