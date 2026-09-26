# CNA House — Architecture and Design Document

**Project:** `cna-house`
**Repository:** `/rv/data/development/github.com/openeggbert/cna-house`
**Document status:** design baseline for the planning pass completed 2026-09-06.
**Implementation status:** IN PROGRESS since 2026-09-06. **Scope reduced 2026-09-21** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md)), **again the same day** ([ADR-0015](docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md)), **and finally on 2026-09-22** ([ADR-0016](docs/decisions/ADR-0016-final-scope-reduction.md)). See `plan.md` for
the ledger.

> Implementation was approved by the project owner on 2026-09-06. This document remains the
> architecture baseline: it is corrected when implementation establishes a durable fact that
> differs from it, and is not rewritten after every source change.

> **Scope amendment — 2026-09-21 (`HOUSE-03201`, [ADR-0014](docs/decisions/ADR-0014-showcase-scope.md)).** `cna-house` is now a
> polished, atmospheric, multiplatform **architectural and graphics showcase for CNA**, not a life
> simulator. The player explores the whole house (basement to attic), the garage, the garden, the
> street and the neighbourhood in first person, and looks at them. **Removed from scope:** the dog
> and the cat (§60, §61); the visible avatar, third person, customisation and character animation
> (§45–§47, the animation parts of §48); the interaction framework and every interactive household
> system (§50, §54–§58), including door, window and switch gameplay (§51–§53); household-state
> persistence and Reset House (§65, §66); and the portal-path audio solver (§64). Objects are
> **static dressing**. Doors and gates have **static poses**, so everything stays reachable.
> Lights follow an **automatic schedule**. Persistence is `settings.json` plus an optional small
> session file. **Retained:** architecture, collision, the first-person controller, portal
> visibility, materials and lighting, sky, sun, moon, stars, time, seasons, weather and their
> visuals, lightweight atmospheric audio, LOD and optimisation, tests, and **Linux, Web and
> Android**. Sections describing removed systems carry a banner and remain as design history.
> They are not requirements. Where any other section mentions a removed feature (the interact
> button and camera toggle of §9.3, the prompts of §67, the pets and save slots of §68, the
> interaction, pet and save tests of §70, the animation and pet budgets of §71), that mention is
> void. The goal, the Definition of DONE, the scheduling rules and the milestones are in
> [`plan.md`](plan.md).

> **Second scope amendment — 2026-09-21 (`HOUSE-03205`, [ADR-0015](docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md)).** *Cut depth,
> preserve breadth.* Every floor, the basement, the attic, the garage, the garden, the exterior, Web
> and Android stay. What this document designs beyond the following is **not required for DONE**; it
> remains design for work that is optional after DONE (`plan.md`, *Optional after DONE*).
> **Quality:** tiers, not a showcase level everywhere: every accessible room reaches a dressed and
> lit baseline, the main rooms a presentation level, and seven hero areas a showcase level
> (`plan.md`, *Quality tiers*). **Furnishing:** a reusable modular kit, reused freely and varied by
> tint, scale and arrangement; the density bands and the anti-repetition limits of §59.1–§59.2 do
> not apply, and §59.3's signs of habitation are required in hero areas only. **Environment:** time
> of day, day and night, the sun, moon and stars, clear, overcast and rain (with the roof mask of
> §37.2 and the wet surfaces of §37.4), fog, and falling snow without accumulation. Snow cover and
> melt (§38), the storm's lightning and thunder (§39), hail (§40), vegetation sway (§41), the glare
> and lens flare of §32.4, puddles and splashes (§37.3–§37.4) and seasonal vegetation (§36.3.1) are
> optional; the storm and hail weather states still exist and render as heavy rain. **Audio:**
> footsteps on six broad surface categories, one interior tone, exterior day and night beds, rain
> and wind (§62); no positional loops, clocks, creaks, appliance hums or seasonal beds, and of §64
> only the weather beds' sky-exposure gain. **Shell:** one settings screen (§68 reduced to graphics,
> audio, controls and environment), no key remapping, and the existing debug tools of §69 as they
> are. **Tests and budgets:** the gates and suites of §70 that exist, plus representative sets; not
> the full matrices of §70.2–§70.4 and §70.6. The budgets of §71 are targets on representative
> scenarios, with no headroom margin. **Platforms:** Web in Chrome and Firefox; Android on one
> representative device. Where a section below requires more than this, `plan.md` governs.

> **Third and final scope amendment — 2026-09-22 (`HOUSE-03206`, [ADR-0016](docs/decisions/ADR-0016-final-scope-reduction.md)).**
> Breadth is fixed: the basement, every floor, the attic, the garage, the exterior, Linux, Web and
> Android all remain required. Depth is reduced further. **Quality:** five hero areas (the front
> approach and porch, the entry and living room, the kitchen, the library and the basement cinema);
> the master bedroom and the attic room are main rooms. **Furnishing:** at most 32 acquired kit models
> plus one wall-art set; storage furniture is generated; §59's catalogue sizes do not apply.
> **Environment:** falling snow (§38) is optional too; a snow weather state shows its overcast sky and
> fog. **Audio:** no special zone loops and no offline-derived footstep sets. **Web:** one preload
> with a progress screen, no progressive pack streaming. The remaining work is bounded by a 280-hour
> ceiling, and after the release the project is in maintenance mode. **Android is required for
> DONE:** an upstream BL-13 blocker does not satisfy it, so the "complete and shippable without it"
> of R-12 and the Android ordering of D-30 no longer make DONE possible without Android. **`plan.md`'s active task text
> is the requirement;** where any section of this document, or an earlier amendment, asks for more,
> `plan.md` governs.

---

## Table of contents

| # | Section |
|---|---|
| 1 | [Executive summary](#1-executive-summary) |
| 2 | [Project goals](#2-project-goals) |
| 3 | [Explicit non-goals](#3-explicit-non-goals) |
| 4 | [Interpretation of the XNA-only requirement](#4-interpretation-of-the-xna-only-requirement) |
| 5 | [Audited CNA capabilities](#5-audited-cna-capabilities) |
| 6 | [Known CNA blockers and risks](#6-known-cna-blockers-and-risks) |
| 7 | [Renderer strategy](#7-renderer-strategy) |
| 8 | [Linux-first platform strategy](#8-linux-first-platform-strategy) |
| 9 | [Future Web and Android strategy](#9-future-web-and-android-strategy) |
| 10 | [World dimensions](#10-world-dimensions) |
| 11 | [Property layout](#11-property-layout) |
| 12 | [Detailed house architecture](#12-detailed-house-architecture) |
| 13 | [Floor-by-floor room schedule](#13-floor-by-floor-room-schedule) |
| 14 | [Coordinate system](#14-coordinate-system) |
| 15 | [Machine-readable floor-plan schema](#15-machine-readable-floor-plan-schema) |
| 16 | [Room / portal graph](#16-room--portal-graph) |
| 17 | [Scene architecture](#17-scene-architecture) |
| 18 | [Content pipeline](#18-content-pipeline) |
| 19 | [External asset strategy](#19-external-asset-strategy) |
| 20 | [Asset licensing strategy](#20-asset-licensing-strategy) |
| 21 | [glTF conversion strategy](#21-gltf-conversion-strategy) |
| 22 | [Materials](#22-materials) |
| 23 | [Static rendering](#23-static-rendering) |
| 24 | [Animated rendering](#24-animated-rendering) |
| 25 | [Culling and visibility](#25-culling-and-visibility) |
| 26 | [Level of detail](#26-level-of-detail) |
| 27 | [Streaming and resource residency](#27-streaming-and-resource-residency) |
| 28 | [Lighting](#28-lighting) |
| 29 | [Shadows](#29-shadows) |
| 30 | [Interior illumination](#30-interior-illumination) |
| 31 | [Sky](#31-sky) |
| 32 | [Sun](#32-sun) |
| 33 | [Moon](#33-moon) |
| 34 | [Stars](#34-stars) |
| 35 | [Day/night cycle](#35-daynight-cycle) |
| 36 | [Weather model](#36-weather-model) |
| 37 | [Rain](#37-rain) |
| 38 | [Snow](#38-snow) |
| 39 | [Storm](#39-storm) |
| 40 | [Hail](#40-hail) |
| 41 | [Wind](#41-wind) |
| 42 | [Weather transitions](#42-weather-transitions) |
| 43 | [Player controller](#43-player-controller) |
| 44 | [First-person camera](#44-first-person-camera) |
| 45 | [Third-person camera](#45-third-person-camera) |
| 46 | [Character customisation](#46-character-customisation) |
| 47 | [Character animation](#47-character-animation) |
| 48 | [Stairs](#48-stairs) |
| 49 | [Physics and collision](#49-physics-and-collision) |
| 50 | [Interaction architecture](#50-interaction-architecture) |
| 51 | [Doors](#51-doors) |
| 52 | [Windows](#52-windows) |
| 53 | [Lights and switches](#53-lights-and-switches) |
| 54 | [Cabinets and storage](#54-cabinets-and-storage) |
| 55 | [Kitchen and refrigerator](#55-kitchen-and-refrigerator) |
| 56 | [Water and plumbing](#56-water-and-plumbing) |
| 57 | [Toilets and waste-state simulation](#57-toilets-and-waste-state-simulation) |
| 58 | [Television and video](#58-television-and-video) |
| 59 | [Furniture and decoration](#59-furniture-and-decoration) |
| 60 | [Dog](#60-dog) |
| 61 | [Cat](#61-cat) |
| 62 | [Audio](#62-audio) |
| 63 | [`/rv/tmp` audio-collection audit](#63-rvtmp-audio-collection-audit) |
| 64 | [Room-aware spatial audio](#64-room-aware-spatial-audio) |
| 65 | [Persistence and save format](#65-persistence-and-save-format) |
| 66 | [Reset-house semantics](#66-reset-house-semantics) |
| 67 | [UI and HUD](#67-ui-and-hud) |
| 68 | [Settings](#68-settings) |
| 69 | [Debug tools](#69-debug-tools) |
| 70 | [Testing](#70-testing) |
| 71 | [Performance budgets](#71-performance-budgets) |
| 72 | [Memory and asset budgets](#72-memory-and-asset-budgets) |
| 73 | [Failure handling](#73-failure-handling) |
| 74 | [Development phases](#74-development-phases) |
| 75 | [Risk register](#75-risk-register) |
| 76 | [Open questions](#76-open-questions) |
| 77 | [Explicit decisions and recommendations](#77-explicit-decisions-and-recommendations) |
| 78 | [First playable milestone](#78-first-playable-milestone) |
| 79 | [Feature-complete desktop version](#79-feature-complete-desktop-version) |
| 80 | [Portability readiness criteria](#80-portability-readiness-criteria) |
| 81 | [Requirements traceability](#81-requirements-traceability) |
| 82 | [Source evidence index](#82-source-evidence-index) |

---

## 1. Executive summary

`cna-house` is a first-person architectural showcase of a very large American-style detached
house, its garage, its fenced property, and a believable surrounding neighbourhood. There is no
win condition. The activity is *exploring* the place: walking through all five levels and the
grounds, watching the weather change through the windows, watching the sun cross the sky and the
moon change phase, and listening to the house and the street. *(Amended 2026-09-21, ADR-0014. The
original text described an interactive simulation with third person, opening and closing things,
switching lights, and a house "exactly as you left it".)*

It is built **exclusively on the XNA 4.0 API surface that CNA implements**. That is the point of
the project: to demonstrate that a modern-feeling, large, atmospheric 3D simulation can be
delivered through `Game`, `GraphicsDevice`, `ContentManager`, `Model`, `Effect`, `BasicEffect`,
`SkinnedEffect`, `DualTextureEffect`, `AlphaTestEffect`, `RenderTarget2D`, `SpriteBatch`,
`SpriteFont`, `BoundingFrustum`, `OcclusionQuery`, `SoundEffectInstance::Apply3D` and
`VideoPlayer` — and nothing below or beside them.

The four decisions that shape everything else:

1. **The house is data, not code.** A machine-readable floor plan (`content/world/*.json`) is the
   single source of truth for geometry, collision, portals, lighting zones, audio zones,
   interaction placement and asset residency. C++ contains *systems*; the building contains no
   hard-coded rooms.
2. **Visibility is a portal system, not a bounding-box sweep.** The house has ~95 cells and
   ~180 portals. A frustum-clipping portal traversal starting from the camera's cell is what makes
   a 935 m² three-storey house plus basement, attic, garage and neighbourhood affordable. Doors
   and windows are portals whose aperture state is the door/window state; closing the kitchen door
   genuinely removes the kitchen from the frame.
3. **Two rendering tiers, both complete.** *Tier S* uses only the stock XNA effects and is the
   baseline the game must be fully playable and visually coherent in, on every renderer. *Tier E*
   adds compiled XNA `Effect`s (real `.fx` compiled by `fxc` through the CNA content pipeline) for
   shadow mapping, single-pass room lighting, sky, wet/snow surfaces and glare. Tier E is
   additive and is a **build configuration**, not a runtime question: it is compiled only when the
   CNA build this binary links against enables compiled effects, and it is activated only if its
   compiled effect set actually loads. Nothing at runtime asks CNA what it can do.
4. **Offline conversion, XNA-shaped runtime.** Source assets are glTF 2.0 / GLB, PNG, WAV, TTF and
   `.fx`. `cna-content` (CNA's own content pipeline) compiles them to `.cnb` / `.xnb`. The runtime
   loads them exclusively through `ContentManager::Load<T>()`. No runtime glTF, no CNAEXT engine
   layer, no renderer-private API.

Headline numbers:

| Quantity | Value |
|---|---|
| Levels | 5 — `B1` basement, `L0` main, `L1` upper 1, `L2` upper 2, `L3` attic |
| Above-grade interior floor area | ~935 m² (≈ 10 060 sq ft) |
| Total enclosed area incl. basement, attic and garage | ~1 306 m² |
| Named interior cells | 78 |
| Named exterior cells | 17 |
| Portals (doors, openings, windows, stair wells) | ~186 |
| Interactable objects (target) | ~~~640~~, removed 2026-09-21 (ADR-0014); objects are static dressing |
| Lot | 45.0 m × 48.0 m = 2 160 m² |
| Ridge height above grade | 14.30 m |
| Simulated day length (default) | **24 real minutes** — 1 real second = 1 simulated minute |
| Initial platform | Linux desktop, `CNA_GRAPHICS_RENDERER=OPENGLES3` (EasyGL) |
| Later platforms | Web (`WEBGL2`), Android (`OPENGLES3`; CNA graphics gate passed 2026-09-26, House APK still open) |

---

## 2. Project goals

**G1 — A believable house.** Every dimension is derived from architectural practice: 2.70 m
ground-floor ceilings, 0.86 m interior door leaves, 179 mm stair risers with 280 mm goings,
0.92 m kitchen counters, a real plumbing stack diagram. A visitor with building experience should
not find anything absurd.

**G2 — A genuinely inhabited-looking place.** Objects are worn and placed the way people place
them: there are dishes in the drainer, boxes in the attic, a dog bed in the family room, a car in
the garage and coats in the mudroom. *(Amended 2026-09-21, ADR-0014: this is achieved by static
dressing throughout the whole house. Objects are not used.)*

**G3 — A strict XNA 4.0 demonstration.** Every graphics, audio, input and content call the game
makes is an XNA 4.0 call CNA implements. Deviations are enumerated, justified and small
(§4.3).

**G4 — Render only what is needed.** Prove it: a debug overlay reports visible cells, culled
cells, draw calls and portal traversals every frame, and automated tests assert culling
correctness and budgets.

**G5 — ~~Persist the whole house.~~** *Removed 2026-09-21 (ADR-0014).* Only settings persist, plus
an optional session file holding the player pose, the clock and the weather.

**G6 — A living sky.** An accelerated but astronomically-derived day/night cycle with a moving
sun, real sunrise/sunset colouring, stars, a moon whose phase advances over simulated days, and a
continuous weather system with rain, snow, hail, storms and wind. *(Amended 2026-09-21, ADR-0015:
the weather model keeps all of these states, but DONE shows clear, overcast, rain and falling snow;
storm and hail render as heavy rain, and snow cover, lightning and hail particles are optional.)*

**G7 — Portability without compromise today.** Linux desktop is the only target being built, but
no decision may be taken that gratuitously blocks WebGL 2 or OpenGL ES 3.0. *(2026-09-21: Web and
Android remain target platforms and are part of `plan.md`'s Definition of DONE.)*

**G8 — Breadth before depth** *(added 2026-09-21, ADR-0014).* The whole property reaches a common
showcase baseline before any area is polished further. `plan.md` enforces this with zones,
completion levels and gates.

---

## 3. Explicit non-goals

| Non-goal | Why |
|---|---|
| A game with objectives, score, quests, combat or an economy | The brief is a simulation of living in a house |
| A survival / hunger / inventory RPG | Item taking exists only as far as "take the milk out of the fridge" needs |
| Driving the car | The car is a prop in v1. A drivable car is a separate project. |
| Full global illumination, path tracing, real-time GI | Not reachable through XNA 4.0 stock effects or SM 3.0 |
| Photoreal PBR | XNA's stock effects are Blinn-Phong. Tier E stays SM 3.0 forward. |
| Fluid simulation | Water is animated geometry and particles, never simulated |
| Physically-modelled acoustics, HRTF, convolution reverb | CNA's mixer does not provide it (§62.2). Approximations are designed instead. |
| Rigid-body physics with stacking, ragdolls, constraints | Nothing in the brief needs it (§49) |
| Multiplayer | Out of scope |
| Modding API, level editor UI | The JSON world format is editable by hand; no GUI editor is planned |
| Explicit anatomy or sexualised content in the toilet system | Deliberately matter-of-fact (§57) |
| Identifiable real people in photographs or artwork | §59.4 |
| An Android or Web build during the initial phases | Planned, phased, and explicitly later |
| Animals (dog, cat) | *Added 2026-09-21*: removed from scope (ADR-0014) |
| A visible player avatar, third person, customisation, character animation | *Added 2026-09-21*: first person only (ADR-0014) |
| Interaction gameplay: pick-up, carry, containers, seats, appliances, plumbing, toilets, television, switches, openable doors and windows | *Added 2026-09-21*: objects are static dressing; doors have static poses (ADR-0014) |
| Persisting household state; Reset House | *Added 2026-09-21*: settings plus an optional session file only (ADR-0014) |

---

## 4. Interpretation of the XNA-only requirement

### 4.1 The rule

Runtime code in `cna-house` may call:

* anything in `Microsoft::Xna::Framework` and its sub-namespaces that is part of the XNA 4.0
  API as shipped by Microsoft, as implemented by CNA;
* `System::*` from `sharp-runtime` (the project's .NET runtime reimplementation), because XNA
  itself is written against .NET and `Stream`, `TimeSpan`, `Exception`, JSON and file I/O are
  .NET, not graphics;
* the C++ standard library;
* **its own code**, in the `cnahouse::` namespace, for everything XNA 4.0 does not provide —
  including the animation skeleton and clip types that Microsoft's own Skinned Model Sample kept
  on the game side rather than in the framework (§47.0).

Runtime code in `cna-house` may **not** call:

* `CNA::Graphics::*` — the CNAEXT engine layer in `modules/graphics-ext/`
  (`docs/cnaext-engine-layer.md`): HDR pipeline, post-process stack, shadow helpers, sky engine,
  image-based lighting, material system, instancing/LOD/culling helpers, compute. It is gated
  behind `-DCNA_CNAEXT=ON`, which `cna-house` **must never set**;
* `CNA::Internal::*` — renderer contracts and implementations;
* `CNA::Graphics::ShaderEffect` — the CNAEXT source-based custom-shader API. `cna-house` uses
  XNA's `Effect` with compiled Effect-Framework bytecode instead (§7.4);
* OpenGL, OpenGL ES, Vulkan, WebGPU, Direct3D, Metal, SDL rendering, or any renderer handle;
* `AvatarRenderer`, `SkinnedModelEXT`, `SkinnedPbrEffect`, `PbrEffect` and the rest of the
  Avatar/PBR extension surface;
* any `CNAEXT`-marked convenience call on an otherwise-XNA graphics or model type — in particular
  `Model::getSkinsEXTProperty()` and `Model::setOwnedResources()`. `cna-house` never constructs a
  `Model` itself and never asks CNA for a multi-skin view of one (§21.3, §47.0);
* `GraphicsDevice::SupportsCapability` and every other CNA-specific runtime capability query. XNA
  4.0 has no such call, so neither do we; tier selection is a build configuration (§7.3);
* CNA's `Graphics::SkinningData`, `Graphics::AnimationClip`, `Graphics::Keyframe` and
  `Graphics::AnimationPlayer`. These reproduce **sample-side** classes from Microsoft's XNA Skinned
  Model Sample; they were never XNA Framework API. `cna-house` owns its equivalents (§47.0),
  exactly as an original XNA title built on that sample would have done.

A CMake-time and a source-lint gate enforce this (§70.1, tasks `HOUSE-00019`–`HOUSE-00022` and `HOUSE-00136`).

### 4.2 Offline tooling is not runtime

The rule constrains the **running program**. It does not constrain the build. `cna-house` uses:

* `cna-content` — CNA's own content pipeline CLI (`docs/content-pipeline.md`), to compile glTF,
  PNG, WAV, `.spritefont` and `.fx` into `.cnb`/`.xnb`;
* Blender 4.3.2 (present at `/usr/bin/blender`) — for decimation, LOD generation, lightmap UV
  unwrapping, lightmap baking, impostor rendering and collision-proxy authoring;
* `fxc.exe` (DirectX SDK June 2010, present at
  `/rv/tmp/samples/_tools/directx-sdk-june-2010/extract/DXSDK/Utilities/bin/x86/fxc.exe`) run
  through Wine — for compiling `.fx` to Direct3D 9 Effect Framework bytecode;
* `ffmpeg` — for audio format conversion and video transcoding;
* Python 3.11 — for manifest, validation and generation tooling.

None of them exist at runtime. The runtime sees only `.cnb`/`.xnb` files and
`ContentManager::Load<T>()`.

### 4.3 Classification, and why there is no deviation allowance

Everything `cna-house` compiles falls into exactly three classes. There is deliberately **no
middle tier**: nothing in the runtime is allowed to be "CNA-specific, but justified".

| Tier | Definition | Policy |
|---|---|---|
| **A** | Pure XNA 4.0 API as implemented by CNA, plus `System::*` from `sharp-runtime` and the C++ standard library | Default. No justification needed. |
| **P** | **Project-owned** code in the `cnahouse::` namespace implementing something XNA 4.0 does not provide, written over Tier-A types and over data produced by our own offline tooling | Allowed and expected — this is *game code*, not a deviation. Every real XNA title had a great deal of it. Each subsystem is listed in `docs/xna-deviations.md` with what XNA lacks. |
| **C** | `CNA::` in any form — the CNAEXT engine layer, renderer contracts, native graphics APIs — **and** every `CNAEXT`-marked convenience call on an otherwise-XNA graphics or model type, including `Model::getSkinsEXTProperty()`, `Model::setOwnedResources()` and `GraphicsDevice::SupportsCapability` | **Forbidden. No exceptions, no register rows, no allowlist.** |

`docs/xna-deviations.md` therefore contains **zero permissive rows**. It is a record of the
Tier-P subsystems and, for each, what XNA 4.0 lacks and what we wrote instead:

| ID | Project-owned subsystem | What XNA 4.0 does not provide | Where |
|---|---|---|---|
| OWN-01 | `cnahouse::anim::Skeleton` / `Clip` / `Keyframe` / `ClipLibrary` | XNA's `Model` has no skeleton or clip container. Microsoft's Skinned Model Sample solved this **in the sample**, on the game side; CNA ships a convenience copy of those sample classes and we deliberately do not use it. | §47.0 |
| OWN-02 | `cnahouse::anim::ClipPlayer` | No blending, layering, masking or rate control anywhere in XNA (BL-10) | §47.3 |
| OWN-03 | `cnahouse::visibility` portal traversal with frustum reduction | XNA has `BoundingFrustum` and nothing above it | §25 |
| OWN-04 | `cnahouse::physics` kinematic capsule collision | XNA 4.0 has no collision or physics | §49 |
| OWN-05 | `cnahouse::audio` portal-path gain, occlusion and muffling | `Apply3D` is pan + attenuation only (BL-11) | §64 |
| OWN-06 | `cnahouse::render::RenderTier` | Tier selection is a build configuration plus a guarded content load; XNA has no capability query and CNA's is forbidden. It is one term of the project-owned effective feature set (§68) | §7.3, §68 |
| OWN-07 | `cnahouse::content` `.chanim` sidecar format and reader | XNA's answer was a custom content processor writing a custom type into `Model.Tag`; we cannot add a processor to CNA's pipeline without changing CNA, so the same custom data travels beside the model | §47.0 |

The mechanical consequence is the point of the whole section: `tools/ci/check_xna_only.py` has
**no symbol allowlist to consult**. Any `CNA::` reference, any `CNAEXT`-marked call, any `*EXT*`
identifier, any `Model::Tag` read in a runtime source fails the build outright (§70.1). A future
contributor cannot argue a symbol into the runtime; they can only write the missing behaviour in
`cnahouse::`, which is what an XNA developer would have had to do anyway.

### 4.4 What happens when XNA cannot do something

The procedure, applied to every feature in this document:

1. Prove the limitation from CNA source, CNA documentation or a measured probe.
2. Record it in the blocker table (§6) with evidence.
3. Design a Tier-A workaround, a Tier-P subsystem of our own, a Tier-E (compiled `Effect`)
   workaround, or a reduced but coherent behaviour.
4. Never reach for a CNA symbol — not the engine layer, not a `CNAEXT` convenience call, not a
   capability query. If XNA cannot do it, we write it (Tier P) or we do without it.

Worked examples in this document: no stencil buffer → no stencil portals or stencil mirrors, use
render-to-texture and frustum-clipped portals (§25.4, BL-02); no MRT on EasyGL → forward rendering
only, no deferred/G-buffer (BL-09); boolean `OcclusionQuery::PixelCount` on GLES 3 → an N×N grid
of independent queries yields N²+1 coverage levels (§32.4, BL-07); no video decoder on
Web/Android → a pre-baked frame-sequence television fallback (§58.3, BL-05).

---

## 5. Audited CNA capabilities

Audited against **cnanext `d42203805057d43dc092bb713613bcc945ad8d5a`, branch `next`,
2026-09-06 15:07:20 +0200** and **sharp-runtimenext `30ccdef30ba4d27534864b0777729b9e78ee8a21`,
branch `next`**. Every row cites the file, document or sample that produced the conclusion.

### 5.1 Core framework

| Capability | State | Evidence |
|---|---|---|
| `Game`, `GraphicsDeviceManager`, `GameTime`, `GameComponent` | Available | `modules/runtime/`, every sample in `/rv/tmp/samples` |
| `Game::Run()` blocking lifetime on desktop **and** Emscripten | Available; Emscripten uses Asyncify + `EM_ASYNC_JS` `requestAnimationFrame`, `Run()` still returns | `docs/emscripten-mainloop-game-lifetime.md` |
| `ContentManager::Load<T>()`, `RootDirectory`, `Unload()` | Available; resolution order is `.xnb` first, then a literal path, then `.cnj`/`.cnb` | `docs/xnb-content-pipeline-support.md` §Scope |
| `System::*` from sharp-runtime incl. `Text.Json`, `IO`, `Xml.Serialization`, `IO.IsolatedStorage` | Available as CMake components. **Measured (`HOUSE-00076`, `HOUSE-00103`):** `Text.Json` is **not** in CNA's default component set and CNA does not link it, so its include directories do not reach a consumer through the `CNA` target — a consumer must add `Text.Json` to `SHARP_RUNTIME_COMPONENTS` **and** link `SharpRuntime::Text.Json` itself. Float round-trip is **bit-exact** (6/6, compared by bit pattern) | `sharp-runtimenext/modules/text-json/CMakeLists.txt` (`NAME Text.Json`); measured by `HOUSE-00076`, `HOUSE-00103` |
| `StorageDevice` / `StorageContainer` | Available. **Corrected by measurement (`HOUSE-00102`, 2026-09-06):** the Linux root is `$XDG_DATA_HOME/game/<container>` else `~/.local/share/game/<container>`. The `<app>` component is **the literal string `game`** — `StorageDevice.cpp:75` is `appName_.empty() ? "game" : appName_` and the only setter is `SetAppNameEXT`, which ADR-0001 forbids. Measured root: `~/.local/share/game/P1Probe`. **`cna-house` therefore identifies itself by the *container* name**, which is plain XNA: `BeginOpenContainer("CnaHouse")` → `.../game/CnaHouse/` | `modules/storage/src/StorageDevice.cpp:75,88-109`; measured by `HOUSE-00102` |

### 5.2 Math and volumes

`Vector2/3/4`, `Matrix`, `Quaternion`, `Plane`, `Ray`, `BoundingBox`, `BoundingSphere`,
`BoundingFrustum`, `ContainmentType`, `Curve`, `MathHelper`, `Point`, `Rectangle`, `Color` — all
present in `modules/math/include/Microsoft/Xna/Framework/`. `BoundingFrustum::GetCorners` +
`BoundingBox::CreateFromPoints` are proven in production use by SAMPLE-038, which fits a light's
orthographic projection to the view frustum every frame
(`cna-samples/plan.md:785`).

### 5.3 Graphics

**Phase 1 measured every row below that `cna-house` depends on; `docs/cna-capability-report.md`
carries the numbers.** Five facts came out of it that are not visible from reading CNA's own
documentation, and that change how this project is written:

* **`CNA.ModelProcessor` emits a 48-byte vertex**, `Position@0` / `Normal@12` / **`Tangent`
  (`Vector4`) @24** / `TexCoord0@40` — *not* `VertexPositionNormalTexture` (40 bytes), and it
  synthesises a `TANGENT` the source `.glb` never authored. Any code reading model geometry back
  declares its own struct and validates it against `VertexDeclaration::GetVertexElements()`
  (`HOUSE-00070`).
* **CNA inserts a synthetic `Root` bone** above the glTF scene root, so bone count is *nodes + 1*
  and `Model::Root` is that synthetic bone. Bone tables are built by **name lookup**, never by node
  index (`HOUSE-00072`).
* **Vertex blend indices are skin-local**, `0..N-1` in `skin.joints` order — *not* `Model::Bones`
  indices. The `.chanim` sidecar must carry the joint names in blend-index order, because nothing in
  the compiled `Model` reproduces that list (`HOUSE-00074`).
* **XNA collection iterators are `CNAEXT`**, so every loop over `ModelMeshCollection`,
  `ModelBoneCollection`, `ModelMeshPartCollection` or `EffectPassCollection` is an index loop over
  `getCountProperty()` (`docs/conventions.md` §5a).
* **`ContentManager::Load<T>` has three different shapes**: `Model` and `Video` return **by value**,
  `Effect` is registered for `std::shared_ptr<Effect>`. None is guessable; CNA reports the mismatch
  precisely when asked wrongly.

| Capability | State | Evidence |
|---|---|---|
| `BasicEffect` incl. 3 directional lights, specular, fog, per-pixel lighting | Available and pixel-verified | `docs/basiceffect-support.md`; Tasks 885/886 fixed `DirectionalLight1/2` and specular |
| `SkinnedEffect`, `MaxBones = 72`, `WeightsPerVertex` 1/2/4, specular, fog | Available, pixel-verified on EasyGL/Vulkan/Bgfx | `docs/skinnedeffect-support.md` support matrix |
| `DualTextureEffect` | Available (the XNA lightmapping effect) | `docs/dualtextureeffect-support.md`; matrix row "Per-slot `SamplerState`" |
| `AlphaTestEffect` | Available | `docs/alphatesteffect-support.md` |
| `EnvironmentMapEffect` incl. Fresnel | Available; `TextureCube` sampling proven | `docs/environmentmapeffect-support.md`, feature matrix |
| `Effect` from compiled Effect-Framework bytecode | Available on EasyGL **behind `-DCNA_EASYGL_COMPILED_EFFECTS=ON`** (MojoShader) | `docs/fx-compiled-effects.md` §10; `cna-samples/CMakeLists.txt:40` sets it ON |
| Compiled `Effect` proven end-to-end in a real game scene | **Yes** — SAMPLE-038 ShadowMapping: two techniques switched by name per draw, `SurfaceFormat.Single` 2048×2048 render target with `DepthFormat.Depth24`, that target rebound as an effect texture parameter, on native OPENGLES3 **and** in real Chrome WEBGL2 | `cna-samples/plan.md:785` |
| `Model` / `ModelMesh` / `ModelMeshPart` / `ModelBone`, `CopyAbsoluteBoneTransformsTo` | Fully audited against FNA | `docs/model-content-pipeline-support.md` §"Model's runtime API" |
| `Model` from compiled content with real bone hierarchy | Yes, via `.xnb` `ModelReader` and via `.cnb` | `docs/xnb-content-pipeline-support.md` `ModelReader` row |
| Skinned/animated `Model` from glTF, with clips embedded and `SkinningData` on `Model::Tag` | Yes, via `cna-content` glTF → CNB — `cna-house` uses the `Model` and ignores the `Tag` (§47.0) | `docs/content-pipeline.md:456-458`; `CnbModelData.hpp` `animations`, `Model.hpp:159-171` |
| `VertexBuffer`, `IndexBuffer`, `DynamicVertexBuffer`, 16- and 32-bit indices | Available; EasyGL has a real 32-bit index factory | feature matrix "All-renderer 32-bit index audit" |
| `RenderTarget2D`, `RenderTargetCube`, mip chains, MSAA | Available on EasyGL | feature matrix "RenderTarget / MSAA / mip / depth" |
| `BlendState`, `DepthStencilState` (compare func), `RasterizerState`, per-slot `SamplerState` (16) | Available on EasyGL | feature matrix "GraphicsDevice state objects" |
| `SpriteBatch` (all overloads, sort modes, custom `Effect`), `SpriteFont` | Available and pixel-verified on EasyGL | feature matrix "2D SpriteBatch / SpriteFont" |
| `OcclusionQuery` | Available on EasyGL; `PixelCount` is a **real count only when the driver exposes `GL_SAMPLES_PASSED`** — the ES 3.2 profile does not, so it degrades to 0/1 | `docs/occlusionquery-support.md` §"The count is a count only where…" |
| `DrawInstancedPrimitives` | Present in the API | `GraphicsDevice.hpp:424` |
| `Texture2D::SetData/GetData/FromStream/SaveAsPng`, NPOT | Available | feature matrix |
| WebGL context loss handling | Implemented and browser-qualified | `docs/web-emscripten-graphics-limitations.md` |

### 5.4 Audio

| Capability | State | Evidence |
|---|---|---|
| `SoundEffect`, `SoundEffectInstance`, looping, volume/pitch/pan | Available (SDL3 mixer engine) | `modules/audio/`, `docs/cna_audio_deep_audit_2026-07-17.md` |
| `AudioListener`, `AudioEmitter`, `SoundEffectInstance::Apply3D` (single and multi-listener) | Available | `SoundEffectInstance.hpp:362,383` |
| 3D model fidelity | **Simplified**: pan, distance attenuation, Doppler on a stereo mixer. No speaker matrix, no cones, no curves, no LFE, no HRTF, no reverb sends | `cna_audio_deep_audit_2026-07-17.md:161` |
| Doppler | Present but carries a documented pitch-inflation risk when velocity units are wrong (up to 4×) | same doc, A-09 (line 131-135) |
| `SoundEffect` wave formats accepted from content | PCM8, PCM16, IEEE float, MS-ADPCM, IMA-ADPCM. **24-bit PCM is not listed** | `docs/xnb-content-pipeline-support.md` audio matrix |
| `MediaPlayer` / `Song` | Available; `Song` is an external stream reference | same doc |

### 5.5 Media

`Video`, `VideoPlayer` with `GetTexture()`, `Play`, `Pause`, `Stop`, `IsLooped`, `Volume`,
`State` exist in `modules/media/include/Microsoft/Xna/Framework/Media/Video/VideoPlayer.hpp`
(lines 70–148). Decoding is the optional `cna_video_ffmpeg` backend selected by
`CNA_ENABLE_VIDEO` (`AUTO` by default). **Linux native: works when FFmpeg dev packages are
installed. Emscripten and Android: fallback only — `Play()` throws
`System::NotSupportedException`** (`docs/video-backend.md` platform table).

### 5.6 Input

`Keyboard`, `KeyboardState`, `Keys`, `Mouse`, `MouseState`, `MouseCursor`, `GamePad*` and the
whole `Touch/` namespace (`TouchPanel`, `TouchCollection`, `TouchLocation`, `GestureSample`,
`GestureType`, `TouchPanelCapabilities`) are present in
`modules/input/include/Microsoft/Xna/Framework/Input/`. `Mouse::SetPosition(int,int)` is
available (`Mouse.hpp:49`), which is all that first-person mouse-look needs in strict XNA 4.0:
read the state, subtract the window centre, recentre. CNA additionally offers a `CNAEXT`
relative-mouse mode (`Mouse.hpp:67,74`) — **not used**, Tier C-adjacent and unnecessary.

### 5.7 Content pipeline (`cna-content`)

`cna-content build <src> -o <out>` compiles a source tree to `.cnb` (default) or `.xnb`
(`--format xnb`). Route table (`docs/content-pipeline.md:370-382`):

| Source | Importer | Processed | Writer | Runtime type |
|---|---|---|---|---|
| `.gltf`, `.glb` | `CNA.GltfImporter/2` | `ImportedModelDocument` | `CNA.ModelContentWriter/3` | `Model` (+ embedded `AnimationClip`s and `SkinningData` on `Tag`, both unused by `cna-house` — §47.0) |
| `.png`, `.jpg`, `.dds` | image front end | `CnbTexture2DData` | texture writer | `Texture2D` |
| `.wav` | wav front end | `CnbSoundEffectData` | audio writer | `SoundEffect` |
| `.spritefont` (+ TTF, FreeType) | `SpriteFontContentPipeline` | — | — | `SpriteFont` |
| `.fx` | `CNA.EffectSourceImporter/1` | `ImportedEffectSource` | `CNA.XnbEffectWriter/1` (**`--format xnb` only**) | `Effect` |
| `.fxb` | `CNA.CompiledEffectImporter/1` | — | `CNA.XnbEffectWriter/1` | `Effect` |
| video | `VideoImporter` (no decode; deploys the stream) | `CnbVideoData` | video writer | `Video` |

CMake integration: `cna_add_content(TARGET … SOURCE_DIR … OUTPUT_DIR … CONFIG_FILE … WORKERS …)`
(`cmake/ToolContentPipeline.cmake:48`).

`.fx` compilation is driven through an external compiler:
`--fx-compiler <path-to-fxc.exe> --fx-compiler-launcher wine`
(`docs/content-pipeline.md:409-424`).

### 5.8 Proven-by-sample capability set

157 XNA 4.0 samples have been ported to CNA in `cna-samples` and archived under `/rv/tmp/samples`.
`cna-samples/plan.md:192` records 80 complete, covering `SAMPLE-001`–`SAMPLE-063` and
`SAMPLE-065` (except the decision-blocked 014 and cancelled 015), `067`–`069`, `072`–`074`,
`076`–`084`, `092`, `098`, `099`, `102`. Each complete sample has a native `OPENGLES3` build and
a real-Chrome `WEBGL2` build. The ones that directly de-risk `cna-house`:

| Sample | What it proves for `cna-house` |
|---|---|
| SAMPLE-038 ShadowMapping | Compiled custom `Effect` with two techniques, float render target, render target as effect texture, frustum-fitted light projection — **all of Tier E** |
| SAMPLE-054 Skinning | Full skinned character content path, `SkinnedEffect` + `SkinningData` + clips, native and browser |
| SAMPLE-055 SkinnedModelExtensions | Skinned-model extension idioms |
| SAMPLE-041 LensFlare | The `OcclusionQuery` sun-glare idiom, and the boolean-`PixelCount` discovery |
| SAMPLE-043 Particles3D, SAMPLE-029 Particle | Particle systems for rain/snow/hail |
| SAMPLE-039 Billboard | Billboards/impostors for vegetation |
| SAMPLE-059 Audio3D | `AudioListener`/`AudioEmitter`/`Apply3D` in a real scene |
| SAMPLE-047/048 Picking, TrianglePicking | `Ray` interaction targeting |
| SAMPLE-049 HeightmapCollision, SAMPLE-074 TankOnAHeightMap | Terrain/ground collision |
| SAMPLE-061 MarbleMaze | Threaded background content loading in a real browser |
| SAMPLE-152 Racing Game | Large-content XNA title running on CNA, incl. WebGL context-loss recovery |
| SAMPLE-031 Bloom, SAMPLE-034 NormalMapping, SAMPLE-035 PerPixelLighting, SAMPLE-134 MultipassLighting | Effect-based lighting and post-processing idioms |

`cna-house` is therefore **not** breaking new ground in CNA. Every subsystem it needs has a
working precedent in this checkout.

---

## 6. Known CNA blockers and risks

Severity: **H** blocks a required feature on the initial target; **M** forces a design change;
**L** is a pipeline or ergonomics cost. "CNA change needed?" answers whether CNA itself must
eventually be modified — `cna-house` never modifies CNA.

| ID | Subsystem | Exact limitation | Source evidence | Sev | Workaround in `cna-house` | CNA change needed? | Can we proceed? |
|---|---|---|---|---|---|---|---|
| **BL-01** | Content / animation | The **XNB** writer emits `null tags only` for `Model`, so an XNB-compiled model cannot carry `SkinningData` on `Model::Tag` | `docs/content-pipeline.md:1068` | L | Irrelevant to `cna-house` either way: the runtime never reads `Model::Tag`. Models are compiled to **`.cnb`** for the bone hierarchy, bounds and materials; skeleton and clip data come from a project-owned `.chanim` sidecar (§47.0) | No | Yes |
| **BL-02** | Graphics / stencil | ~~`Clear` ignores `ClearOptions::Stencil` and `ReferenceStencil` has no renderer connection~~ — **measured false, `HOUSE-00085`, 2026-09-06. Stencilling works.** A `Depth24Stencil8` render target was cleared to stencil 0; a first pass stamped `ReferenceStencil = 1` with `Always`+`Replace` over the **left half only**; a second pass drew the **whole** quad white under `CompareFunction::Equal`. Result: **left half 2048/2048 lit, right half 0/2048.** An inert `ReferenceStencil` would have covered everything. | measured by `HOUSE-00085`, `docs/cna-capability-report.md` | **Downgraded M → note** | **The restriction is lifted.** Stencil-based masking is available if a design wants it — window portals and mirrors in particular. Nothing currently depends on it, so no design changes; but a future one may, and no longer has to route around a limitation that does not exist. | No | Yes |
| **BL-03** | Graphics / MRT | ~~attachment 1 stays black~~ — **measured false for compiled effects, `HOUSE-00086`/`HOUSE-00087`, 2026-09-06.** With a *stock* effect, which declares one output, attachment 1 does stay `(0,0,0)` — the renderer simply does not broadcast. With a **compiled `.fx` technique declaring `COLOR0` and `COLOR1`**, attachment 0 received `(255,128,64)` and attachment 1 received its **own** `(32,223,96)`, matching the authored `(0.125, 0.875, 0.375)` to a rounding step. **MRT works.** | measured by `HOUSE-00086`, `HOUSE-00087` | **Downgraded M → note** | Tier S stays forward-rendered because it uses stock effects, which is a property of Tier S and not a limitation. Tier E **may** use multiple render targets; a depth+normal prepass is now on the table for phase 45 rather than ruled out. | No | Yes |
| **BL-04** | Graphics / effects | CNA embeds no HLSL compiler. `.fx` needs an external `fxc`. **Verified end to end against a genuine Microsoft `fxc` (DXSDK June 2010) under Wine, `HOUSE-00087`, 2026-09-06** — `.fx` → `.xnb` → MojoShader → a draw, with techniques selected by name and parameters reaching the shader exactly. | measured by `HOUSE-00087`–`HOUSE-00089` | **Downgraded M → L** | Tier E uses `--fx-compiler <DXSDK June 2010 fxc.exe> --fx-compiler-launcher tools/effects/fxc-wine.sh`. **The bare `--fx-compiler-launcher wine` does not work**: `cna-content` builds the command line with Unix absolute paths and `fxc` reads a leading `/` as an option (*"Unknown or invalid option '/tmp/cna-fx-0-…/effect.fxb'"*). `tools/effects/fxc-wine.sh` translates existing paths through `winepath -w` and is the launcher to use. The compiled `.xnb` **is committed** next to the `.fx` so contributors without Wine can still build. Tier S must be complete without any custom effect. | No | Yes |
| **BL-05** | Media / video | FFmpeg decoding is Linux/macOS only. On **Emscripten and Android** `VideoPlayer::Play()` throws. **Measured with `CNA_ENABLE_VIDEO=OFF`, `HOUSE-00099`, 2026-09-06:** `Play()` throws *"Video playback is unavailable because CNA was built without the optional FFmpeg video backend…"*, the message names its own fix, and **`Load<Video>` still succeeds** — only playback is gated, not content loading. `VideoPlayer` still constructs, still reports `Stopped`, and `Stop()` is still callable, so the rest of the program links and runs. | `docs/video-backend.md` platform table; measured by `HOUSE-00099` | H *(for Web/Android only)* | TV has two backends behind one interface: `VideoTvSource` (`Video`+`VideoPlayer`) and `SequenceTvSource` (a pre-baked strip of frames in a `Texture2D` atlas, advanced on a timer, with a separately-streamed `SoundEffect`/`Song` track). Web/Android select the sequence backend. | Would be nice | Yes |
| **BL-06** | Audio / formats | ~~24-bit PCM is rejected~~ — **measured false, `HOUSE-00068`, 2026-09-06.** 24-bit PCM is accepted by both paths: `cna-content`'s `SoundEffectProcessor` converts it to 16-bit and warns (*"converted to 16-bit PCM with round-to-nearest and saturation"*), and `SoundEffect::FromStream` accepts a raw 24-bit WAV without throwing. The conversion is **byte-identical** to `ffmpeg -c:a pcm_s16le` (96 304-byte `.cnb`, only 29 header bytes differing, PCM identical from byte 400 to EOF). Every file in the `/rv/tmp` NOX collection is `pcm_s24le`, 48 kHz | `docs/xnb-content-pipeline-support.md` audio matrix; `ffprobe`; measured by `HOUSE-00068`/`HOUSE-00069`, `docs/cna-capability-report.md` | **Downgraded L → note** | No format work is required. The offline step is kept for **provenance, not format** — ADR-0012's manifest records the source and converted hashes. Where it is run the command is `ffmpeg -i in.wav -c:a pcm_s16le out.wav`: **`-ar 44100` is dropped**, because measuring the halves separately showed the resample, not the bit depth, is what costs signal (bit depth Δ 0.0017 dB RMS; adding `-ar 44100` Δ 0.889 dB RMS and 0.26 dB peak). The collection is uniformly 48 kHz, CNA loads and plays 48 kHz correctly, and the mixer resamples at playback anyway | No | Yes |
| **BL-07** | Graphics / queries | **Confirmed, `HOUSE-00090`/`HOUSE-00091`, 2026-09-06, and proved to be a profile property rather than a CNA or hardware one.** Same probe, fixture, driver and GPU, two renderers: a quad of analytic area **16 384** answered **`1`** under `OPENGLES3` and **`16 384`** exactly under `OPENGL33`. | measured by `HOUSE-00090`, `HOUSE-00091` | L | Sun/moon glare uses an **N×N grid of independent point queries** (default 3×3), giving 10 quantised coverage levels on any driver; on a driver with a real count the same code refines to per-query pixel counts | No | Yes |
| **BL-08** | Graphics / textures | `Texture2D` mip-level `SetData` (level > 0) is a silent no-op on Vulkan and Bgfx; correct on EasyGL | feature matrix "Texture2D mip-level SetData" | L | We ship pre-generated mip chains in content and only target EasyGL. Recorded so a renderer change is a conscious decision. | Yes, eventually | Yes |
| **BL-09** | Graphics / formats | **SETTLED POSITIVELY, `HOUSE-00083`, 2026-09-06.** A 2048×2048 `SurfaceFormat::Single` + `DepthFormat::Depth24` render target was created, rendered into, read back **bit-exactly** (3 396 649 drawn texels all exactly `0.625`, 797 655 cleared texels exactly `0`, nothing else) and **sampled as an effect texture** (arriving as 159/255). The matrix row was indeed stale for render targets. `HOUSE-00110` also mapped the boundary: **only `Color` is usable both as a `Texture2D` and as a `RenderTarget2D`**; `Single`, `Vector2`, `Vector4`, `HalfSingle`, `HalfVector2` and `HalfVector4` are render targets **only** — a `Texture2D` of those formats cannot be created at all, at either graphics profile. | measured by `HOUSE-00083`, `HOUSE-00110` | **Resolved** | **Tier E uses a real float shadow map.** The RGBA8 depth-packing fallback is not needed and the pack/unpack in every shadow lookup goes away; it was exercised anyway and is known good. Any other float data must arrive as the output of a render pass, never as an uploaded texture. | Row needs a refresh | Yes |
| **BL-10** | Animation | `AnimationPlayer` plays exactly one clip; `Update()` overwrites every bone. No blending, no layering, no additive tracks | `AnimationPlayer.hpp:107-169` | L | `cna-house` implements `cnahouse::anim::ClipPlayer` — the same evaluation with 2-clip cross-fade, an upper-body mask and rate control — over its **own** `Skeleton`/`Clip` types (§47.0), not CNA's sample copies. Pure XNA `Matrix`/`Quaternion` math. | No | Yes |
| **BL-11** | Audio / 3D | `Apply3D` is a simplified stereo pan + attenuation + Doppler; no cones, curves, filters, reverb or HRTF. **Measured, `HOUSE-00095`/`HOUSE-00096`, 2026-09-06:** the attenuation is **full volume inside `DistanceScale`, then inverse *distance* beyond it** (1 m → 1.000, 2 m → 0.500, 10 m → 0.100, 20 m → 0.050 at the default scale) — not inverse-square — and pan is the listener-relative rightward displacement over distance, clamped. **`Apply3D` writes none of this to the public `Volume`/`Pan`/`Pitch`**, so a game cannot read back what CNA applied. The Doppler guarantee holds absolutely: at scale 0 the factor is set to exactly `1.0f` **without evaluating the Doppler math**, so no rounding path exists. | `cna_audio_deep_audit_2026-07-17.md:131,161`; measured by `HOUSE-00095`, `HOUSE-00096` | M | `SoundEffect::DopplerScale = 0` and zero listener/emitter velocities by default (settings-exposed). Room-aware attenuation, occlusion and "sound through the doorway" repositioning are implemented **in `cna-house`** over the portal graph (§64); muffling is a two-instance bright/dull cross-fade on the ~20 sounds that need it | Would be nice | Yes |
| **BL-12** | Content | The legacy `.model.json` `ModelTypeReader` synthesises exactly one bone and never sets `ParentBone`, `BoundingSphere` or `Tag`. **The `.cnb` route was measured instead, `HOUSE-00072`/`HOUSE-00073`, 2026-09-06:** a depth-3 hierarchy round-trips with every local and absolute transform matching an offline computation, and `ModelMesh::BoundingSphere` **is** populated and contains every vertex. | `docs/model-content-pipeline-support.md` summary table; measured by `HOUSE-00072`, `HOUSE-00073` | — | Not used. `cna-house` uses `.cnb`. **Caveat measured:** the `.cnb` bounding sphere is **conservative, not minimal** — radius 7.686 against a minimal 6.225 on the test box (+23 %), centre 1.55 off in Y. Usable as a cheap conservative reject; anywhere a tight bound matters, `cna-house` computes its own from the vertex data it already reads back. | No | Yes |
| **BL-13** | Android | **Gate resolved by current-source measurement, 2026-09-26.** Android still defaults to `SDL_RENDERER`, but explicit `CNA_GRAPHICS_RENDERER=OPENGLES3` configured and `cna_runtime` plus `cna_renderer_easygl` cross-built for API 24/`arm64-v8a` with CNA `cefe6c83b`, sharp-runtime `d86adb65` and NDK 29.0.14206865. The former sharp-runtime NDK errors did not recur. CNA's `demo_devices` graphics sample then built as an arm64 APK and drew SpriteBatch panels on the GPU-accelerated `Medium_Phone` API-35 emulator (arm64 translation); the inspected capture is `docs/android-cna-graphics-sample.png`. Its old Gradle template needed installed Build Tools 36.1/CMake 4.1/NDK 29, a `main`-only build and an explicit `SDL_main.h` include in the ignored probe copy; without the include SDL logged `Couldn't find function SDL_main` and returned home. The corrected APK remained running after the draw. | `HOUSE-02951` verification in `plan.md`; `docs/portability.md`; emulator screenshot | **Resolved as a gate; Android House path still open** | Build House's Android package from the current CNA path; do not mistake this sample for House D10c. | No source change for the gate; CNA's sample template can be corrected upstream separately | Yes, proceed to Android House work |
| **BL-14** | Web | `WEBGL2` has no implicit WebGL 1 fallback; the Web build needs Asyncify + JS exceptions; SharedArrayBuffer needs COOP/COEP headers for the threaded variant | `docs/web-emscripten-graphics-limitations.md` | M | Single-threaded WebGL 2 build in phase 48; threads only if measurement demands them | No | Yes (later) |
| **BL-15** | Graphics / effects | `SpriteBatch::Begin(effect)` on a renderer without `CompiledEffects` throws | `docs/fx-compiled-effects.md` §3 | L | Tier E is a build configuration, never a runtime query (§7.3): `SpriteBatch::Begin(effect)` is compiled only into a Tier-E build and is reached only after that build's effect set has loaded successfully. Every Tier-E path has a named Tier-S fallback | No | Yes |
| **BL-16** | Graphics / samplers | CNA's public XNA-shaped `SamplerStateCollection::operator[]` returns a `SamplerState&`, but assigning an XNA singleton to it selects `SamplerState::operator=`, which CNA marks `CNAEXT`; there is no separate strict-XNA setter. Measured after CNA merge `fcd43e995` by `HOUSE-01620`, 2026-09-13. | `SamplerStateCollection.hpp`; strict-XNA gate | L | Runtime does not assign sampler slots. CNA initializes every slot to XNA's default `LinearWrap`. `HOUSE-00927` found that the HUD's default `SpriteBatch::Begin()` instead left `LinearClamp` in slot 0 after `End()`, flattening repeat-UV house materials in the next frame. The existing XNA-shaped `SpriteBatch::Begin(..., &SamplerState::LinearWrap, ...)` keeps the HUD premultiplied and leaves repeat sampling correct without calling the forbidden assignment. Fonts/UI and real-device material/golden tests protect the boundary. Sky sun/moon texture borders remain wrap/clamp-equivalent. | **Yes — an XNA-shaped indexed-property setter or equivalent is still required for arbitrary per-pass sampler control** | Yes |

| **BL-17** | Shared upstream build configuration | **Temporary, 2026-09-16:** `HOUSE-01039` CMake regeneration first stopped on sibling CNA's unclassified Wayland SDL test. By `HOUSE-01040` that file was no longer in sibling status, but regeneration now stops earlier when CNA's required MojoShader series writer cannot write `/home/robertvokac/deps/FNA3D/cna-mojoshader-patch-series.patch` under this workspace sandbox. CNA's patcher also contains a `git checkout -- .` fallback for a changed series in a presently modified shared MojoShader submodule: do not run it unsandboxed without proving the current series/stamp state. Neither failure is a house-data or XNA exception. | `/tmp/house01039-build-r1.log`, `/tmp/house01040-build-r1.log`; read-only sibling/dependency status and `CNA/cmake/patches/apply-fna3d-mojoshader-patch.cmake` | L *(current build session)* | Build canonical content normally, copy only its verified products into existing `build/content`, and use exact generated house test compile/link commands with the mandated shared ccache. Direct render, culling, unit and strict-XNA checks remain runnable. Do not claim a fresh full CMake/CTest pass until safe upstream/shared-dependency regeneration is available. | **Yes — safe shared-dependency build access or upstream stabilization for the full rebuild** | Yes for visual-data work; full rebuild is blocked |

**Manufactured blockers are not welcome.** Things that are merely *work* — writing a portal
system, baking lightmaps, authoring 640 interactables — are not blockers and do not appear here.

---

## 7. Renderer strategy

### 7.1 Renderer selection

`cna-house` builds with **`CNA_GRAPHICS_RENDERER=OPENGLES3`** on Linux — the EasyGL implementation
targeting an OpenGL ES 3.0 context. Rationale:

* It is the **exact configuration 80 ported XNA samples are verified on**
  (`cna-samples/CMakeLists.txt:23`), including the shadow-mapping and skinning samples this
  project depends on.
* GLES 3.0 and WebGL 2 are closely related API families, and CNA's `WEBGL2` is the same EasyGL
  implementation over the second of them. That makes Linux `OPENGLES3` an **excellent portability
  baseline** for the later Web target: the same renderer code path, the same shaders, the same
  feature floor. It is a baseline, not a guarantee — running successfully under Linux `OPENGLES3`
  does **not** by itself establish that the same scene will run under WebGL 2 / Emscripten, which
  adds constraints the desktop build never exercises (Asyncify, JS exceptions, no threads, real
  context loss, browser texture-format and canvas rules — §9.1, BL-14). Those are settled by the
  dedicated Web phases (47–48) and their own validation, never by extrapolation from Linux.
* It is also the Android target verified by the 2026-09-26 CNA graphics gate; House Simulator's
  own Android package and traversal remain to be proved.
* EasyGL is the only renderer where `OcclusionQuery` is both wired **and** pixel-verified in both
  directions (`docs/occlusionquery-support.md` support matrix).

Two secondary configurations exist for development, never for shipping:

| Config | Renderer | Purpose |
|---|---|---|
| `desktop-gl33` | `OPENGL33` | Desktop OpenGL 3.3. Gives `GL_SAMPLES_PASSED`, so `OcclusionQuery::PixelCount` is a real fragment count — used to *validate* that the N×N grid approximation agrees with a true coverage measurement |
| `headless` | `HEADLESS` | CI: runs `Update()` and the whole simulation with no window, no GPU and no display server. Used by every logic, save, weather, visibility-graph and interaction test |

`VULKAN` is explicitly **not** chosen: it has no Emscripten wiring, and CNA's own web/android docs
say "no other renderer has any Emscripten-specific wiring at all"
(`docs/web-emscripten-graphics-limitations.md`).

### 7.2 Tier S — stock effects only

Tier S is the guaranteed baseline. It may use only:

| Effect | Used for |
|---|---|
| `BasicEffect` | dynamic props, furniture, doors, pets' static parts, sky dome, sun/moon quads, stars, particles, water surfaces, blob shadows, debug geometry |
| `DualTextureEffect` | all baked static architecture: albedo × lightmap, one additive pass per light group (§28.3) |
| `SkinnedEffect` | player avatar, dog, cat |
| `AlphaTestEffect` | foliage cards, fence lattice, curtains, chain-link, grilles |
| `EnvironmentMapEffect` | mirrors (static cube map), chrome fixtures, the car's paint and glass |
| `SpriteBatch` + `SpriteFont` | HUD, prompts, glare/flare sprites, lightning flash, letterbox, debug overlays |

Tier S is a **complete, shippable game**. Everything in the feature list works in Tier S: the
house is lit (baked + additive daylight + per-object directional lights), the sky is a
vertex-coloured dome with scrolling cloud layers, shadows are blob shadows, weather is particles
and material swaps.

### 7.3 Tier E — compiled XNA `Effect`s

Tier E adds real `.fx` shaders, compiled to Direct3D 9 Effect-Framework bytecode by `fxc` and
loaded through `ContentManager::Load<std::shared_ptr<Effect>>()`.

**How Tier E is selected — without asking CNA anything at runtime.** Whether compiled effects
exist at all is a property of the CNA build this binary links against, and `cna-house` chooses
that build itself (§8.1). The decision is therefore made where the knowledge already lives:

1. **Build time — the primary mechanism.** `cna-house`'s own CMake reads the CNA configuration it
   has just set: the selected `CNA_GRAPHICS_RENDERER` and that renderer's compiled-effect option
   (`CNA_EASYGL_COMPILED_EFFECTS` for `OPENGLES3`, its `SDL_GPU`/`VULKAN` siblings otherwise). From
   that it sets `CNAHOUSE_TIER_E`. When it is off, the Tier-E sources and the `.fx` content tree
   are **not compiled at all** and the binary contains only Tier S.
2. **Load time — the safety net.** When Tier E is compiled in, `LoadContent` loads the effect set
   inside one `try`/`catch`. A `ContentLoadException` or `NotSupportedException` — a missing
   `.xnb`, a driver that will not accept the bytecode — selects Tier S, logs once, and the game
   continues. That is ordinary XNA content loading, not a capability query, and it also covers the
   content failure a capability query would have missed.
3. **User choice.** `--tier=s` and the graphics settings force Tier S at any time. There is no
   switch that forces Tier E into a build that does not have it.

**The renderer itself is not switchable at runtime.** `CNA_GRAPHICS_RENDERER` is fixed when the
binary is configured (§8.1), and standard XNA 4.0 exposes no way to change it afterwards, so
`cna-house` ships **no `--renderer` option** — offering one would be a lie about what the binary
can do. A separate build is produced per renderer. The optional diagnostic `--renderer-info`
prints only what the application already knows about itself — the configured renderer name, the
`CNAHOUSE_TIER_E` build fact and the resolved `RenderTier` — and then continues; it queries
nothing.

Tier S is never a degraded mode arrived at by accident: it is the project's default configuration
and the one every feature in this document is specified against. The shader set:

| Effect | Techniques | Replaces (Tier S) |
|---|---|---|
| `RoomLit.fx` | `Lit`, `LitShadowed`, `LitLightmap`, `LitLightmapShadowed` | the 2–4 additive `DualTextureEffect` passes → one pass with 1 directional + up to 4 point lights + lightmap + shadow lookup |
| `ShadowDepth.fx` | `Depth`, `DepthSkinned` | (no Tier-S equivalent; Tier S has no shadow map) |
| `SkinLit.fx` | `Skin`, `SkinShadowed` | `SkinnedEffect` (adds shadow receive + rim) |
| `SkyDome.fx` | `Sky`, `SkyClouds` | vertex-coloured dome + 2 cloud layers → analytic gradient + 3 blended cloud layers |
| `Precip.fx` | `Rain`, `Snow`, `Hail` | `BasicEffect` particles → soft-edged, depth-faded, wind-sheared particles |
| `SurfaceBlend.fx` | `Wet`, `Snowy`, `WetSnowy` | material swaps → continuous wetness/snow blend |
| `PostComposite.fx` | `Tonemap`, `TonemapGlare`, `Passthrough` | none → exposure adaptation + glare composite |
| `WaterFlow.fx` | `Faucet`, `Pool` | scrolling-UV `BasicEffect` → refracted, animated water |

Shader model target: **`vs_3_0` / `ps_3_0`** (`profile: hidef`). Rationale: SM 3.0 gives 224
constant registers and dynamic branching, which is enough for 4 point lights + a shadow lookup,
and MojoShader translates it to GLSL ES 3.00 for EasyGL. Nothing uses geometry or tessellation
stages (GLES 3.0/WebGL 2 have neither —
`docs/web-emscripten-graphics-limitations.md`).

**Every Tier-E effect has a named Tier-S fallback and a test that renders the same scene both
ways.** The visual difference is allowed to be large; the *scene content* must be identical.

### 7.4 Why `Effect` and not `ShaderEffect`

`CNA::Graphics::ShaderEffect` takes caller-authored GLSL/SPIR-V and lives in the CNAEXT engine
layer (`docs/shader-effect-vs-fx-bytecode.md`). It is Tier C: forbidden. XNA 4.0's own answer to
"I want a custom shader" is `Effect`, and CNA implements it for exactly that
(`Effect(GraphicsDevice&, byte[])` + `EffectReader`). Using `Effect` also means one authored
`.fx` runs on every renderer whose CNA build enables compiled effects, instead of one source per
backend.

### 7.5 Frame structure

```
Update(gt):
  1  input           → PlayerInput, InteractionIntent
  2  clock           → simulated time, sun/moon vectors, sky state
  3  weather         → weather state, transition, precip spawn budget
  4  interaction     → apply queued actions, mutate interactable states
  5  doors/windows   → animate apertures, publish portal aperture changes
  6  lighting        → per-room artificial + daylight levels, dominant lights
  7  physics (fixed) → player, pets, camera collision, at 1/120 s
  8  animation       → ClipPlayer evaluation, bone palettes
  9  pets            → behaviour, navigation
 10  audio           → listener, emitters, portal-path gains, voice manager
 11  visibility      → camera cell, portal traversal, visible cell set + frusta
 12  residency       → asset residency requests (measured, then gated)
Draw(gt):
  A  shadow pass     (Tier E)  → light frustum fit, ShadowDepth.fx into RT
  B  sky pass                  → dome, stars, moon, sun, clouds — depth write off
  C  opaque static             → per visible cell, per chunk, DualTexture/RoomLit
  D  opaque dynamic            → props, pets, avatar, doors
  E  alpha-test                → foliage, fences, grilles
  F  transparent (back-to-front)→ glass, water, curtains, particles
  G  glare queries             → OcclusionQuery grid at the sun/moon
  H  composite     (Tier E)    → tonemap + glare, full-screen quad
  I  HUD                       → SpriteBatch prompt, clock, debug overlays
```

---

## 8. Linux-first platform strategy

### 8.1 Build

`cna-house` consumes `../cnanext` and `../sharp-runtimenext` as sibling source checkouts by
`add_subdirectory`, exactly as `cna-samples` does
(`cna-samples/CMakeLists.txt:50`, `:30`). No vendoring, no submodules, no fetch.

```cmake
set(CNA_GRAPHICS_RENDERER            "OPENGLES3" CACHE STRING "" FORCE)   # WEBGL2 on Emscripten
set(CNA_SHARP_RUNTIME_ROOT           "${CMAKE_CURRENT_SOURCE_DIR}/../sharp-runtimenext" CACHE PATH "" FORCE)
set(CNA_EASYGL_COMPILED_EFFECTS      ON  CACHE BOOL "" FORCE)   # Tier E
set(CNA_ENABLE_VIDEO                 AUTO CACHE STRING "" FORCE) # television
set(CNA_CNAEXT                       OFF CACHE BOOL "" FORCE)   # the engine layer must not exist
set(CNA_BUILD_TESTS                  OFF CACHE BOOL "" FORCE)
set(CNA_BUILD_EXAMPLES               OFF CACHE BOOL "" FORCE)
set(SHARP_RUNTIME_COMPONENTS "Core.Base;Console;IO;IO.IsolatedStorage;Collections.Core;Collections.ObjectModel;Runtime;Threading;Text;Text.Json;Globalization;Diagnostics;Storage;Security.Cryptography" CACHE STRING "" FORCE)
add_subdirectory(../cnanext CNA_BUILD)
```

`CNA_CNAEXT OFF` is not cosmetic: with it off every file in `modules/graphics-ext/` is compiled
out and a CNA ctest (`CNAEXT_GuardDiscipline`) enforces it, so the forbidden layer *does not
exist in the binary*. That is the strongest possible mechanical guarantee of the XNA-only rule.

Build directories follow the openeggbert build rules: `build/`, `build-asan/`, `build-ubsan/`,
`build-probe/`, `build-consumer/` — the closed list, in-repo, never in `/tmp` or the scratchpad.
`CCACHE_DIR="$HOME/.cache/ccache"`, `CCACHE_BASEDIR=/rv`, launchers passed to CMake.
`/rv/cnaccache` is a symlink to the same physical cache; the home path is ccache's own
default, so code that forgets to export it still lands in the one cache rather than starting
a second one. `CMakePresets.json` sets both.

### 8.2 Runtime environment

| Item | Value |
|---|---|
| Distribution | Debian 13, kernel 6.12 |
| Compiler | g++ 14.2 (C++23) |
| GPU (dev machine) | AMD Radeon 780M (radeonsi), Mesa 25.0.7, GL 4.6 / GLES 3.2, 1 GB reported video memory, unified 30 GB system RAM |
| Window/platform | `CNA_PLATFORM=SDL3` (default) |
| Audio | `CNA_AUDIO_PLATFORM=SDL3` (the only one that defines `SOUND_ENABLED`) |
| Video | FFmpeg dev packages (`libavcodec/format/util/swresample`) |
| Save location | `${XDG_DATA_HOME:-~/.local/share}/cna-house/` |

### 8.3 Filesystem assumptions (kept portable from day one)

* All content is addressed by **content name**, never by an OS path:
  `content.Load<Model>("Props/Kitchen/fridge")`. `ContentManager::RootDirectory` is set once.
* Path separators are never hard-coded; `System::IO::Path` is used for the few places a real path
  is needed.
* Case sensitivity: content names are lower-case-with-hyphens, directories `PascalCase`, and a
  build check rejects any content name differing only by case.
* Saves go through `StorageDevice`/`StorageContainer`. The Web port swaps the backing store for
  `System::IO::IsolatedStorage` (IndexedDB-backed) behind the same `ISaveStore` interface.
* **No `std::filesystem` in game code** except inside `SaveStore`'s desktop implementation.

### 8.4 Threads

v1 is **single-threaded**. Everything runs on the game thread. Rationale: Emscripten threading
changes the ABI of the whole module and requires COOP/COEP headers
(`docs/web-emscripten-graphics-limitations.md`); the loading cost must be measured before paying
that price. `HOUSE-02451` measures cold-start load time; a background loader is only implemented
if it exceeds 4 s.

---

## 9. Future Web and Android strategy

### 9.1 Web (Emscripten → WebGL 2)

Everything below is *planned now, built later* (phases 47–48).

| Concern | Finding | Consequence for the architecture today |
|---|---|---|
| Renderer | `WEBGL2` is EasyGL over WebGL 2, the same API family as GLES 3.0; **no implicit WebGL 1 fallback** | Already the desktop renderer family, so the port starts from a working baseline rather than a rewrite — but it still has to be proved in a browser (phases 47–48). Desktop success is not evidence of browser success. |
| Main loop | `Game::Run()` blocks and returns, via Asyncify + `EM_ASYNC_JS` `requestAnimationFrame` | `Game` must own the loop; **never** write our own `while(true)` or `emscripten_set_main_loop` |
| Exceptions | JS-lowered exceptions, `-fexceptions -sDISABLE_EXCEPTION_CATCHING=0`, cannot combine native Wasm EH with Asyncify | Use exceptions sparingly and only for genuinely exceptional paths; never in the frame loop |
| Threads | Opt-in `-pthread`; changes the module ABI; needs `crossOriginIsolated` | Stay single-threaded (§8.4) |
| Video | `VideoPlayer::Play()` throws `NotSupportedException` | TV abstraction with a frame-sequence backend (BL-05, §58.3) |
| Audio | Browsers require a user gesture before audio starts | A "Click to enter" title screen that also serves as the loading screen; audio engine start is deferred until the first input event on every platform, so the desktop path exercises the same code |
| Filesystem / saves | No POSIX filesystem; IndexedDB via `IO.IsolatedStorage` | `ISaveStore` abstraction from day one (§8.3) |
| Download size | Whole content tree must be preloaded or fetched | Content is split into **packs** (`core`, `house-l0`, `house-l1`, `house-l2`, `house-b1`, `house-l3`, `exterior`, `neighbourhood`, `audio-core`, `audio-ambience`, `video`) from the start, even though the desktop build loads all of them |
| Texture memory | Narrower guaranteed formats; compressed-texture extensions vary | All runtime textures are `SurfaceFormat::Color` with pre-generated mips. Compressed (DXT) variants are produced **offline** and are selected by the target's content profile (§27.2), never by a runtime renderer query; an uncompressed variant is always packaged as the fallback |
| Anisotropic filtering | Needs `EXT_texture_filter_anisotropic`; falls back to trilinear | Never depend on it visually. The Web content profile simply does not permit it, so the Web build samples trilinear (§68) |
| Context loss | Real and handled by CNA (`webglcontextlost`/`restored`), qualified in Chrome | All GPU resources must be reconstructible from CPU-side state; no resource may be the only copy of its data |
| Canvas as display | No `DisplayMode` list | Resolution settings must degrade to "use the canvas size" |

Web budget tier: 1280×720, 30 FPS floor, ≤ 160 MB textures, ≤ 500 draw calls,
≤ 180 MB compressed download, neighbourhood at impostor LOD only.

### 9.2 Android

**CNA graphics gate passed on 2026-09-26 (BL-13).** The older upstream requirements were
re-tested against current CNA and sharp-runtime, rather than inferred from their 2026-09-06 state:

1. The two formerly reported `sharp-runtime` NDK-portability bugs (`FileStream.cpp`
   unused-private-field under `-Werror`; `FileSystemInfo.cpp` using `std::chrono::clock_cast`,
   absent from NDK libc++) did not recur in the current API-24/arm64 cross-build. The historical
   CNA Task 920 record is not a current blocker.
2. `CNA_GRAPHICS_RENDERER=OPENGLES3` must be selectable and buildable for `arm64-v8a` (CNA
   defaults Android to `SDL_RENDERER`, but the explicit OPENGLES3 cross-build succeeded).
3. CNA's `demo_devices` graphics sample ran and drew on the GPU-accelerated `Medium_Phone` emulator
   (API 35, arm64-v8a translated on x86_64). See `docs/portability.md` for toolchain and proof.

This unlocks House Simulator's Android APK work; the sample does **not** satisfy D10c.
The platform preparation already in the project includes:

* GLES 3.0 feature floor is already the desktop target.
* Input goes through an `IInputSource` abstraction with `KeyboardMouseSource` and (later)
  `TouchSource`; no game system reads `Keyboard`/`Mouse` directly.
* A `Platform` capability struct (`hasKeyboard`, `hasMouse`, `hasTouch`, `hasGamePad`,
  `hasVideo`, `preferredQualityTier`) drives HUD layout and quality defaults.
* All UI is laid out in *virtual units* scaled by a `uiScale` derived from the back-buffer size
  and DPI, so a touch HUD can be added without moving anything.
* Android budget tier: 1280×720, 30 FPS, ≤ 120 MB textures, ≤ 400 draw calls, LOD bias +1,
  weather particles ×0.4, no Tier-E post-composite.

### 9.3 Touch controls (planned, phase 50)

A translucent HUD, only shown when `Platform::hasTouch && !hasKeyboard`:

| Control | Placement | Behaviour |
|---|---|---|
| Movement stick | bottom-left, 180 vu radius, floating origin | analogue direction, magnitude → walk speed |
| Look region | whole right half minus buttons | drag = look, sensitivity settings-controlled |
| Interact button | bottom-right, 110 vu | shows the same verb text as the desktop prompt |
| Walk-mode toggle | above interact | normal / fast |
| Camera toggle | top-right | first / third person |
| Menu | top-left | pause menu |

Desktop never renders any of it.

---

## 10. World dimensions

### 10.1 Units and conventions

| Property | Value |
|---|---|
| Length unit | **1 world unit = 1 metre.** glTF 2.0 declares metres (§3.4), so `unitScale = 1.0` and no conversion happens at import |
| Handedness | Right-handed. glTF and XNA agree; **the importer performs no axis remap and no handedness swap** (`docs/gltf-conventions.md:15-25`) |
| Up | `+Y` |
| Forward (camera default) | `−Z` |
| **North** | `−Z` |
| **East** | `+X` |
| **South** | `+Z` |
| **West** | `−X` |
| Angles | Radians internally; degrees in JSON, converted on load |
| Yaw | Rotation about `+Y`; yaw 0 = facing north (`−Z`); positive yaw turns east (clockwise seen from above) |
| Time | Simulated seconds since the epoch, `double` |
| Winding | glTF front faces are CCW; XNA's default `CullCounterClockwise` removes CCW faces, so **every glTF-derived draw uses `RasterizerState::CullClockwise`** (`docs/gltf-conventions.md:22`). One shared `RasterizerState` object; mirrored placements flip it |

### 10.2 Origin

**The world origin `(0, 0, 0)` is the centre of the pedestrian gate opening in the front fence, at
finished grade.** The pedestrian gate, the front walk and the front door all share `x = 0`.

Grade at the house perimeter is `y = 0.00`. The lot slopes very gently: `y = +0.15` at the front
property line falling to `y = −0.35` at the rear fence, expressed as a coarse height field
(§11.5) so drainage, the terrace step and the shed pad read correctly.

### 10.3 World extents

| Extent | X | Y | Z |
|---|---|---|---|
| Playable volume | −40.0 … +40.0 | −3.5 … +20.0 | −52.0 … +12.0 |
| Property (fenced) | −22.5 … +22.5 | — | −48.0 … 0.0 |
| House main block (exterior) | −13.0 … +9.0 | −2.60 … +14.30 | −27.4 … −14.0 |
| Rear extension | −7.0 … +3.0 | −0.10 … +3.95 | −32.4 … −27.4 |
| Garage wing | +9.0 … +17.4 | −0.10 … +6.60 | −22.0 … −13.0 |
| Road corridor (accessible) | −35.0 … +35.0 | — | 0.0 … +11.5 |
| Neighbourhood shell (visual only) | −220 … +220 | −5 … +40 | −260 … +180 |
| Sky dome radius | 900 (drawn depth-write-off, so it never conflicts with the far plane) |

The far plane is **420 m**; the near plane **0.10 m**. Depth precision at 24-bit with a 0.10 m
near plane is adequate for interiors; a second, tighter projection (near 0.05, far 12) is used for
first-person hand-scale objects if any Z-fighting is measured (`HOUSE-00628`).

### 10.4 Bounding the playable world believably

The player is contained by real objects, in this priority order:

1. **The property fence** — 1.85 m painted timber board fence on the west, north and east
   boundaries; a 1.35 m ornamental fence with the two gates along the front.
2. **The neighbours' fences and hedges** — the road corridor is bounded on the far (south) side by
   the neighbours' front fences and a continuous 2.1 m privet hedge.
3. **Road termination** — the accessible road ends at `x = ±35` where it bends behind
   dense street trees and a parked delivery van (east) / a hedge-lined corner and a low stone wall
   with a "PRIVATE ROAD" sign (west). The road *geometry* continues visually far beyond. Built by
   `HOUSE-00775`: the barrier itself is a privet hedge across the whole corridor at each end --
   the corner the road bends behind -- and the wall, the sign, the van and the trees stand in
   front of it. A body walking the road is stopped at ±34.7 and at +11.2, all three of them by
   something drawn.
4. **Terrain** — the ground beyond the accessible area rises gently and is planted, so there is no
   visible edge.
5. **A final invisible boundary** at the playable-volume box, 6 m beyond every believable barrier,
   as a safety net. Crossing it is impossible in normal play; if it is ever touched, a debug
   counter increments so tests can detect a gap in the real barriers.

Automated test `HOUSE-00618`: a bot performs a 20-minute pseudo-random walk with a fixed seed and
asserts the invisible-boundary counter stays at 0 and the player never leaves the union of the
named exterior cells.

---

## 11. Property layout

### 11.1 The lot

45.0 m wide (X: −22.5 … +22.5) by 48.0 m deep (Z: −48.0 … 0.0) = **2 160 m²** (0.53 acre). Large
but ordinary for an American suburban lot carrying a house of this size.

| Zone | X | Z | Notes |
|---|---|---|---|
| Front lawn (west of walk) | −22.5 … −1.0 | −14.0 … 0.0 | mown grass, two mature maples, a flower bed against the porch |
| Front lawn (east of walk) | +1.0 … +9.5 | −13.0 … 0.0 | grass, a birch, the mailbox at the pedestrian gate |
| Front walk | −1.0 … +1.0 | −11.6 … 0.0 | 2.0 m wide bluestone pavers, 4 low path lights |
| Driveway | +9.6 … +16.8 | −13.0 … 0.0 | broom-finished concrete, 7.2 m wide at the gate narrowing to the garage apron |
| Porch | −3.6 … +3.6 | −14.0 … −11.6 | timber deck at `y = +0.57`, 4 columns, roof at `y = +3.65` (= the front balcony floor) |
| East side yard | +17.4 … +22.5 | −48.0 … 0.0 | 5.1 m strip: gravel service path, hose reel, AC condenser, two bins |
| West side yard | −22.5 … −13.0 | −48.0 … 0.0 | 9.5 m strip: lawn, a shrub border, the gas meter, a compost bin |
| Rear terrace | −6.7 … +6.7 | −36.0 … −32.4 | 48 m² flagged patio at `y = +0.45`, three steps down from the sunroom |
| Rear lawn | −22.5 … +22.5 | −44.0 … −36.0 | mown grass, a swing bench, a fire pit, a birdbath |
| Vegetable/flower garden | −20.0 … −13.0 | −44.0 … −36.5 | 6 raised beds, a trellis, a watering can |
| Garden shed | −20.0 … −16.4 | −44.0 … −40.4 | 3.6 × 3.6 m, ridge at `y = +2.85`, one door, one window, **enterable** |
| Orchard corner | +12.0 … +21.0 | −46.0 … −38.0 | 4 fruit trees, a stack of firewood under a lean-to |
| Rear fence line | −22.5 … +22.5 | −48.0 | 1.85 m board fence, a 0.9 m gate at `x = +19.0` to the alley (locked, decorative) |

`HOUSE-00771` realizes the two rear leisure zones as canonical static props rather than generator
decoration. `EXT_TERRACE` carries one four-seat dining group, two separately angled loungers and
four planted pots while preserving the central slider-to-lawn route. `EXT_BACKYARD` carries the
swing bench, stone fire pit and birdbath with clear circulation around the three terrace steps.
Six deterministic project-authored GLBs reuse the approved outdoor wood, metal, textile,
bluestone, soil and foliage finishes and carry named collision proxies where their solid mass
requires one. Their exact positions remain authoritative in `layout.props.json`; furniture scale
or composition is never hard-coded in runtime code.

**No swimming pool.** Analysis: a pool would add a large animated water surface, a fence-within-a-
fence, a pump house, seasonal covers, drowning-avoidance collision and winter behaviour, for a
feature the brief marks as optional and "spectacle". The same effort spent on the vegetable
garden, the terrace and the orchard produces a more inhabited, more believable property. Rejected,
with reasons, per the brief's own instruction.

### 11.2 Fencing and gates

| ID | Element | Position | Interactable |
|---|---|---|---|
| `EXT_FENCE_FRONT` | ornamental fence, 1.35 m, painted white | Z = 0, X = −22.5 … +22.5 minus gate openings | no |
| `EXT_GATE_PED` | pedestrian gate, 1.2 m leaf, single hinge east | X = −0.6 … +0.6, Z = 0 | **yes** — open/close, latch sound |
| `EXT_GATE_DRIVE` | sliding vehicle gate, 6.0 m | X = +10.2 … +16.2, Z = 0 | **yes** — slide open/close, motor + rail sound, 6 s travel |
| `EXT_FENCE_W` / `_E` / `_N` | 1.85 m board fence | boundaries | no |
| `EXT_GATE_REAR` | rear service gate, 0.9 m | X = +18.55 … +19.45, Z = −48 | **yes** — but bolted; opening reveals only a hedge |

### 11.3 The driveway and garage relationship

The garage is a **front-loaded, projecting east wing**. Its doors face south (`+Z`) at
`z = −13.0`, 1.0 m forward of the main façade — a very common American arrangement that also gives
the front elevation depth. The driveway runs straight from the vehicle gate to the apron, 13 m of
concrete, wide enough to park a second car beside the door. A 1.0 m concrete path links the
driveway to the front walk at `z = −6.0` so the player never has to cross grass.

`HOUSE-00946` gives that broad two-car slab buildable visual scale: six 30 mm control-joint runs
divide it into roughly 2.8 m panels, while a 150 mm bluestone edge and two 300 mm transverse
bluestone bands tie the drive to the front-walk palette. These are path-bound render details at
4–5 mm anti-z-fight lift, not a second path: the canonical concrete height, collision, navigation
and footstep class remain authoritative.

`HOUSE-00947` finishes the canonical 4.86 m sectional leaf with four framed glass lites in its top
band and a measured 300 mm centre pull on two mounts. The treatment is selected by the opening
row, reuses the existing exterior-window glass and bronze hardware finishes, and remains shallow
finish geometry around the same five authored sections. The aperture, leaf/collision envelope,
portal, future spline animation and navigation stay authoritative.

`HOUSE-00948` makes the weather-side opening part of the Colonial Revival facade rather than a
bare cut through siding. The same opening row selects two 240 mm painted pilasters on grounded
300 mm plinths, restrained 320 mm capitals, a 300 mm frieze and stepped 120 mm crown. Their backs
embed 6 mm behind the wall plane and their visible faces project 55–95 mm, so they neither float
nor compete with the door's aperture. The pieces reuse the approved white window-frame finish and
batch; the simulated leaf, wall opening, collision, navigation and receiver lightmaps do not move.

### 11.4 The road and the neighbourhood

| Feature | Z range | Notes |
|---|---|---|
| Verge (our side) | 0.0 … +1.6 | grass strip, 3 street trees, a fire hydrant at `x = +6.0` |
| Sidewalk (our side) | +1.6 … +3.2 | 1.6 m concrete, scored slabs |
| Kerb | +3.2 | 0.15 m |
| Carriageway | +3.2 … +10.2 | 7.0 m asphalt, two lanes, a faded centre line, drain grates at `x = ±14` |
| Kerb, sidewalk, verge (far side) | +10.2 … +13.4 | mirrored |
| Neighbours' fences / hedge (far side) | +11.5 … +12.0 | continuous barrier, at §10.3's own edge of the accessible corridor (corrected 2026-09-09 by `HOUSE-00775`: at +13.4 it stood beyond §11.5's ground and outside §10.3's playable volume, so it could never stop anybody) |

Neighbourhood composition (all non-enterable):

> Decided 2026-09-10 by `HOUSE-00843` ([ADR-0013](docs/decisions/ADR-0013-neighbourhood-asset-variants.md)).
> **The eight palettes are in the asset ids.** A `neighbourhood` row names its house as
> `MODEL_NB_HOUSE_A_CREAM` or `MODEL_NB_HOUSE_C_RENDER_LOW`, and `neighbourhood_gen.py` resolves the
> shape, the palette and the LOD band from that name offline; there is no per-instance material
> override in the layout and none at runtime. Only the combinations the street names are generated
> — **nineteen of a possible forty-eight** — and the assignment is deterministic and neighbourly:
> houses are walked in street order and each takes the first palette unused within 30 m across and
> 10 m deep, so no two houses you can see together are painted alike.

* **N1 / N2** — the two immediately adjacent houses at `x ≈ −34` and `x ≈ +36`, same street,
  full-detail facades and roofs, real windows with interior-glow cards at night, driveways, cars,
  mailboxes, lawns, fences. LOD0.
* **N3–N8** — six houses across the street, `z ≈ +22 … +30`, medium detail. LOD1.
* **N9–N24** — sixteen further houses along the street and behind, low detail. LOD2.
* **N25–N60** — distant roof/gable silhouettes and tree lines rendered as impostor cards on a ring
  at 120–260 m.
* Street furniture: 9 street lights (on a dusk sensor), 5 utility poles with catenary wires,
  4 stop/street signs, 12 mailboxes, 3 parked cars, 2 rubbish-bin clusters (only on the
  in-fiction collection day), a basketball hoop on a neighbour's drive.
* Vegetation: 34 street trees, 60 shrubs, hedges as extruded strips.
* Far background: a low tree-line ridge and a distant water tower on the horizon ring.

### 11.5 Ground

The terrain is a **height field** on a 1.0 m grid over the whole playable area (81 × 65 samples =
5 265 vertices), authored as a 16-bit PNG plus a JSON metadata sidecar, with a per-cell material
index (grass / lawn-worn / flower-bed soil / gravel / concrete / asphalt / bluestone / mulch).
Rendered as one static chunk per 16 × 16 m tile (**20 tiles**, 5 × 4 — corrected from 25 by
`HOUSE-00762`: 81 × 65 samples on a 1 m grid is 80 × 64 m, and 25 would need a 5 × 5 field), each
with its own `BoundingBox`, so distance culling works. Collision uses the same height field, as the **two triangles each square
is drawn as** — one surface for the collider and for anything that asks how high the ground is.

**The drawn ground stops at the wall of a room it would otherwise run through** (`HOUSE-00786`).
The field is one continuous surface over the whole lot, which means it passes straight through the
basement: over `B1_GYM`'s footprint the ground runs y −0.537…−0.006 and B1's ceiling is at +0.25,
so half a metre of lawn hangs inside the room. Collision already worked around this by asking the
ground only in outdoor cells (`HOUSE-00774`), and rendering cannot: a tile is 16 m square, so the
tile seen through a basement window is the same tile that continues under the house. So
`terrain_gen.py` cuts the mesh — and only the mesh — to the plan of every non-exterior cell whose
interior volume the surface enters, which on this house is the basement's fourteen boxes and
**269.24 m²** of the lot's 5 120. `terrain.png` keeps every sample, because the field is also what
says how high the ground is against a wall; the collider and every "how high is the ground here"
query read the same numbers they always did.

> **Corrected by `HOUSE-00553`.** This said *"bilinear sample + a triangle test for slopes > 20°"*,
> and a bilinear sample cannot be the collision surface. Over the same four samples the bilinear
> patch and the two triangles differ by a quarter of the square's twist, and on this lot that
> reaches **74 mm on a square that is not steep by the 20° rule** — the rule tests each triangle's
> own slope, which says nothing about how far the smooth patch strays between them. A body told
> the ground is at the bilinear height, and placed a millimetre over it, stands 73 mm inside the
> ground it is drawn on, and §49.3's depenetration shoves it out again every tick. The 20° figure
> is kept for what it is genuinely for: `TerrainSample::steep` tells a caller the ground here is a
> slope rather than a lawn, which is what §60 asks.

---

## 12. Detailed house architecture

### 12.1 Massing and style

A large, symmetrical-fronted **American Colonial Revival** with a projecting garage wing, a full-
width front porch on four square columns, a hipped-and-gabled roof at a 7:12 pitch with five
dormers, painted horizontal siding over a brick water table, white trim, black shutters, and
double-hung windows with 6-over-6 muntins on the front elevation. The rear elevation is plainer:
a single-storey sunroom extension with a flat roof used as the master balcony, and a
flagged terrace below.

The covered-porch finish is derived rather than a second authored facade (`HOUSE-00929`). A cell
whose full footprint is covered by the next stacked cell receives a downward-facing soffit at its
own head height, with fascia and cornice closing the measured gap to the covering floor. On the
front porch those canonical heights are +3.35 and +3.65: the 300 mm structure zone sits on four
five-part painted square columns, each with a 260 mm shaft and 400 mm base/capital. The same rule
reads the footprint, open sides and covering height from world data; it does not name `L0_PORCH`
or invent a second shell. Existing trim/soffit materials, portal ownership and collision stay the
single source of truth.

The main and garage eaves use the same finish discipline (`HOUSE-00934`) without changing their
settled roof planes: a 280 mm painted frieze and a 100 mm projecting crown sit below the existing
200 mm fascia/soffit edge. The five wall dormers keep the holes, cheeks and gable roofs derived by
`roof_geometry`; only their finish becomes architectural—approved upper siding on vertical faces,
with 65–75 mm painted corner, header and rake trim. These finish layers are non-lightmapped detail
and do not alter attic collision, drainage, ridge, pitch or eaves coordinates.

The covering's exposed seams and rainwater goods receive the same derived finish discipline
(`HOUSE-00944`). Each canonical hip has a 220 mm shingle-over cap lifted only 35 mm at its centre;
the five dormers use 180 mm ridge caps and 55 mm dark-metal cheek flashing. The continuous ridge
vent remains the one `roof_geometry.ridge_vent` defines but uses the shingle finish its construction
already specifies, not gutter metal. Four six-fold K-profile gutters per roof use the settled
120 mm nominal drainage section with a 12 mm rolled lip, and all six data-derived downspouts are
closed four-sided tubes to their existing terrain splash points. These are non-lightmapped finish
faces. Roof planes, dormer
holes, collision, eaves, drainage heads, splash/audio positions and the lightmap receiver set do
not move. The brushed gutter finish is charcoal rather than zero-diffuse black so its folded form
can respond to the existing stock-XNA lighting path.

The closed garage frontage (`HOUSE-00935`) is the existing canonical 4.86 × 2.35 m steel leaf,
not a second facade prop. Five equal horizontal sections and four restrained raised panels per
section give it spatial relief; the finish is a warm charcoal-grey exterior paint using the approved
fine-paint maps. A slightly lighter sibling finish keeps the spatial panels readable in the strict
Tier-S path rather than flattening them into a single coloured slab. The shell representation
remains non-lightmapped and room-owned, while only those two narrow weather-facing material roles
enter the exterior hierarchy. The opening, portal, threshold, collision and §54 five-segment
animation contract do not move.

The garage's authored +4.30 m head is also its roof eave (`HOUSE-01046`). It lies between ordinary
storey ceilings and therefore carries no 350 mm inter-storey floor band. Exterior skin extends to
the next FFL only when its cell head coincides with an actual level ceiling; a custom roof-bound
head stops at its own value. This keeps garage siding below the separate `ROOF_GARAGE` planes
instead of projecting a false wall through the front hip to L2 at +6.55.

Three above-grade storeys plus a habitable attic. This is what makes "several floors" believable
rather than a stack of arbitrary levels: three-storey Colonials of this size were genuinely built,
and the attic under a 7:12 roof over a 13.4 m span has real usable volume.

**The attic is ventilated the way a hip roof is** (`HOUSE-00491`, owner decision 2026-09-10):
**soffit intake** at the eaves, which the 0.15 m oversail already provides and `build_roof` has
drawn since `HOUSE-00461`, exhausting through a **continuous ridge vent** — a 0.28 m shingle-over
cap straddling the 8.60 m ridge, stopping 0.45 m short of each hip where the three planes meet and
there is no cavity under it to exhaust. It reads on the silhouette as a thickened ridge line, which
is what a ridge vent looks like.

`HOUSE-00468` had already drawn *"a vent along the ridge"* — a flat lid floating 0.08 m above the
apex, from constants of its own. That was two answers to one question, and the flat one could not
cap a slot it was supposed to straddle. There is one now, `roof_geometry.ridge_vent`, and it is the
one this section is written against.

This replaces the two `W_GABLE` louvres §12.6 used to list. They were in *"attic gable ends"*, and
this roof has none: `roof_geometry.roof_planes` builds four planes meeting at a ridge, so both
louvres stood **1.44 m inside solid roof**. The decision was to remove them rather than grow a
gablet to justify a detail an earlier plan had got wrong. The projecting garage wing is square —
9.0 × 9.0 outside — so its roof is a pyramid with no ridge at all, and it gets no ridge vent; the
generator answers that from the geometry rather than being told which roof it is drawing.

### 12.2 Level elevations

All values are **finished floor level** (FFL) unless stated. Structural depth between the top of
one ceiling and the next FFL is 0.35 m (engineered joists + subfloor + finish).

| Level | ID | FFL (y) | Ceiling (y) | Clear height | Notes |
|---|---|---|---|---|---|
| Basement | `B1` | **−2.30** | +0.25 | 2.55 m | Full basement under the main block only |
| Main / ground | `L0` | **+0.60** | +3.30 | 2.70 m | 0.60 m above grade; 3 porch steps |
| Upper floor 1 | `L1` | **+3.65** | +6.20 | 2.55 m | |
| Upper floor 2 | `L2` | **+6.55** | +9.00 | 2.45 m | |
| Attic | `L3` | **+9.30** | rafter line | 1.20 m at the knee wall → 5.00 m at the ridge | Collar-tie flat ceiling at +12.60 over the finished part |
| Roof ridge | — | **+14.30** | — | — | 14.30 m above grade |
| Rear-extension roof / rear balcony | — | **+3.65** | open | — | Same level as `L1` FFL — a level threshold |
| Porch roof / front balcony | — | **+3.65** | open | — | Same |
| Garage floor | — | **+0.15** | +4.30 | 4.15 m | Slab; a storage loft over the rear half at +2.90 |

Naming discipline: the document and the data use **`B1` / `L0` / `L1` / `L2` / `L3`** only.
"Ground floor" = `L0` (American "first floor"); "first upper floor" = `L1` (American "second
floor"). The words "first floor" and "second floor" are never used alone anywhere in the code,
data or UI.

### 12.3 Construction dimensions

| Element | Thickness / size |
|---|---|
| Exterior wall (L0–L2) | 0.30 m |
| Foundation wall (B1) | 0.30 m concrete, exposed 0.25 m above grade |
| Interior partition | 0.15 m |
| Plumbing wall | 0.20 m |
| Party wall garage↔house | 0.25 m (fire-rated) |
| Floor structure | 0.35 m |
| Roof rafter plane thickness | 0.28 m |
| Knee wall height (attic) | 1.20 m |
| Interior door leaf | 0.86 × 2.05 m; rough opening 0.90 × 2.10 m |
| Front entry door | 1.00 × 2.15 m + two 0.30 m sidelights |
| Rear/patio slider | 2.40 × 2.15 m |
| Garage door | 4.90 × 2.40 m five-band sectional with four top lites and centre pull, sill at the slab +0.15, centred on the driveway at X +13.20 |
| Cased opening (typical) | 1.60 × 2.20 m |
| Stair balustrade height | 0.95 m; balcony/terrace railing 1.10 m |
| Handrail | 0.90 m above the pitch line |
| Ceiling cornice | 0.11 m |
| Skirting/baseboard | 0.14 m |

### 12.4 Stairs

Every flight satisfies `2·rise + going ∈ [600, 650] mm` and a consistent rise within a flight.

| Stair | From → To | Total rise | Risers × rise | Going | Width | Shape | Footprint |
|---|---|---|---|---|---|---|---|
| `STAIR_MAIN_L0_L1` | L0 +0.60 → L1 +3.65 | 3.050 m | 17 × 179.4 mm | 280 mm | 1.10 m | U, half-landing at riser 9, **+2.2147** | X +2.20…+4.90, Z −20.20…−15.30; a 1.00 m clear foyer approach remains to the north wall |
| `STAIR_MAIN_L1_L2` | L1 +3.65 → L2 +6.55 | 2.900 m | 16 × 181.3 mm | 280 mm | 1.10 m | U, landing at riser 12, **+2.175 rel.** | X +2.20…+4.90, Z −20.20…−14.30 |
| `STAIR_ATTIC_L2_L3` | L2 +6.55 → L3 +9.30 | 2.750 m | 15 × 183.3 mm | 235 mm | 0.90 m | straight, north-running | X +5.40…+8.20, Z −18.30…−14.30 |
| `STAIR_BASEMENT_L0_B1` | L0 +0.60 → B1 −2.30 | 2.900 m | 16 × 181.3 mm | 275 mm | 1.00 m | straight, north-running, **directly beneath the main stair** | X +2.20…+4.90, Z −20.20…−14.30 |
| `STEPS_PORCH` | grade 0.00 → porch +0.57 | 0.570 m | 3 × 190 mm | 300 mm | 3.00 m | straight | Z −11.60…−10.70 |
| `STEPS_TERRACE_LAWN` | lawn 0.00 → terrace +0.45 | 0.450 m | 3 × 150 mm | 350 mm | 3.00 m | straight | Z −36.00, the terrace's south edge |
| `STEPS_TERRACE` | terrace +0.45 → sunroom +0.60 | 0.150 m | 1 × 150 mm | 350 mm | 3.60 m | single step | Z −32.40 |
| `STEPS_GARAGE` | garage +0.15 → mudroom +0.60 | 0.450 m | 3 × 150 mm | **300 mm** | 1.10 m | straight | inside the garage at the house wall |

`HOUSE-00489` corrects the main-stair circulation within this unchanged footprint: both U
flights ascend first in the east lane and return in the west. The basement flight uses an east
1.10 m well instead of a 2.30 m hole, leaving a real west-side approach from the foyer. Thin
upper-level exit strips connect the returning treads to the hall openings; L2 also has a
full-width south cross landing for its attic-stair door. The shell, collision and railing gaps
derive from the same flight placement, rather than moving a decorative stair mesh alone.

`2·179.4 + 280 = 638.8` ✔ · `2·181.3 + 280 = 642.6` ✔ · `2·183.3 + 235 = 601.6` ✔ ·
`2·181.3 + 275 = 637.6` ✔ · `2·150 + 350 = 650` ✔ (both terrace flights) ·
`2·150 + 300 = 600` ✔ (garage) · `2·190 + 300 = 680` (exterior, permitted) ✔

> Corrected 2026-09-07 by `HOUSE-00379`, which authored these as data and checked the arithmetic.
> **Eight flights, not seven:** the terrace deck is +0.45 and the lawn is grade, so there are three
> steps between them that nothing scheduled — without them the terrace is a place you can see and
> not reach. **The garage steps' going was 280 mm**, making `2·150 + 280 = 580`, below §70.5's
> band; 300 mm makes it exactly 600 and the flight has the room. **The two landing heights** were
> 2.7 mm and 15 mm off a riser boundary: a landing is where a riser ends, so it is 9 × 179.4 mm
> above L0's floor and 12 × 181.3 mm above L1's, which is +2.2147 and +2.175 rel. The rises
> themselves are exact — 3.05/17, 2.90/16, 2.75/15 — and this table's tenths of a millimetre are
> those numbers rounded.

> Corrected 2026-09-23 by `HOUSE-03226`. The attic flight's 265 mm going left a 230 mm terminal
> rise after the ramp, above the controller's 220 mm step-up limit. The 235 mm going reaches the L3
> floor before capsule contact and remains inside the required Blondel band. The existing attic
> store opening was shifted onto the resulting 0.90 m landing; no room or new route was added.

Headroom under every flight and at every landing nosing is ≥ 2.00 m, verified by
`HOUSE-00360`'s automated check against the level heights.

### 12.5 Plumbing relationships

Wet rooms are stacked, and every stack lands on a basement drain run. This constrains the room
schedule and is a first-class reason several rooms are where they are.

| Stack | Fixtures | Vertical alignment | Drop to |
|---|---|---|---|
| **STACK-A** (centre-east) | `L0_WC1`, `L1_WC4`, `L2_WC6` | X +2.20…+4.90, Z −22.00…−20.20 — perfectly aligned on all three floors | `B1_UTILITY` |
| **STACK-B** (west) | `L0_WC2`, `L1_BATH2`, `L2_BATH4` | shared corner X −10.40…−9.70, Z −20.20…−18.30 | `B1_STOR1` chase → main drain |
| **STACK-C** (master) | `L1_MASTER_BATH` | over `L0_PANTRY`/`L0_BUTLERS` (X −12.70…−8.20), chase in the west wall | `B1_STOR2` |
| **STACK-D** (guest east) | `L0_LAUNDRY`, `L1_BATH3`, `L2_BATH5` | X +4.90…+8.70, Z −22.00…−18.30 — perfectly aligned | `B1_UTILITY` |
| **STACK-E** (kitchen) | `L0_KITCHEN` sink + dishwasher, `L0_SUNROOM` wet bar | north wall of the kitchen, `x ≈ −4.0` | `B1_STOR2` |
| **STACK-F** (basement) | `B1_WC7`, laundry tub in `B1_LAUNDRY2` | at the drain | ejector pit |
| **STACK-G** (centre-west) | `L1_WC3`, `L2_WC5` | X −7.00…−6.40, Z −22.00…−21.30 — the two are directly above one another | `B1_HOBBY` ceiling void → main run |
| Water main | enters `B1_MECHANICAL` through the **south** foundation wall (from the street) | | |
| Sanitary drain | leaves `B1_UTILITY` through the **north** foundation wall | | |
| Gas | meter on the west elevation → `B1_MECHANICAL` | | |
| HVAC | air handler + furnace in `B1_MECHANICAL`; trunk ducts in the basement ceiling; a vertical chase X +2.20…+2.60 beside the stair serves L1/L2/L3 | | |

> STACK-G was added 2026-09-08 by `HOUSE-00413`, which placed the toilets against the walls their
> stacks run in and found two — `L1_WC3` and `L2_WC5`, one directly above the other — draining
> nowhere at all.

These are not decoration: the mechanical room's boiler hum, the water-hammer thump when a tap
closes, the pipe-run creak in the basement ceiling and the duct rumble when the furnace starts are
all placed from this diagram (§62.6).

### 12.6 Elevations — windows and doors by façade

Window types (leaf size; sill height above the room's FFL):

| Type | Size | Sill | Where |
|---|---|---|---|
| `W_DH_STD` | 1.20 × 1.50 | 0.90 | standard double-hung, most rooms |
| `W_DH_TALL` | 1.20 × 1.80 | 0.60 | L0 formal rooms |
| `W_BAY` | 2.80 × 1.70 (3-facet) | 0.55 | `L0_LIVING` south |
| `W_PICTURE` | 2.40 × 1.60 | 0.85 | `L0_FAMILY`, `L0_SUNROOM` |
| `W_SLIDER` | 2.40 × 2.15 (door) | 0.00 | sunroom → terrace, master → balcony |
| `W_BATH` | 0.60 × 0.90 obscured | 1.40 | bathrooms and WCs |
| `W_BASEMENT` | 0.90 × 0.60 hopper | −0.45 abs. | basement, in 0.9 m window wells |
| `W_DORMER` | 1.00 × 1.10 | 0.75 | attic dormers |
| ~~`W_GABLE`~~ | ~~0.80 × 0.80 louvre~~ | — | **removed by `HOUSE-00491`**: it named "attic gable ends", and this roof is a hip |
| `W_TRANSOM` | 1.00 × 0.40 | above doors | front door, garage side door |

Three more types the data needed and this table did not list, added 2026-09-07 by `HOUSE-00376`:

| Type | Size | Sill | Where |
|---|---|---|---|
| `W_SIDELIGHT` | 0.30 × 2.05 | 0.10 | either side of the front door |
| `W_PANEL` | 1.80 × 2.10 fixed | 0.30 | the sunroom's glazed flanks |
| `W_INTERNAL` | 1.20 × 1.10 | 0.95 | borrowed light, kitchen → sunroom |

Counts per elevation. **Measured** from `assets-src/world/layout.portals.json` after `HOUSE-00376`
authored it, replacing the designed counts that stood here before; the planes are the rooms' own
faces, which is why the front reads `z = −14.30` and not the outer face `z = −14.0`. The table
below is a **gate**: `tools/world/window_schedule.py --check` compares it with the layout cell by
cell and fails when the two drift (`HOUSE-00380`). The per-window schedule is
[`docs/window-schedule.md`](docs/window-schedule.md), generated from the same data:

| Façade | B1 | L0 | L1 | L2 | L3 | Total |
|---|---|---|---|---|---|---|
| South (front, `z = −14.30`) | 4 | 8 | 7 | 6 | 3 | **28** |
| North (rear, `z = −27.10`) | 1 | 4 | 4 | 4 | 2 | **15** |
| West (`x = −12.70`) | 3 | 3 | 3 | 3 | — | **12** |
| East (`x = +8.70`) | — | 1 | 1 | 1 | — | **3** |
| Sunroom flanks (`x = ∓6.70/+2.70`) | — | 4 | — | — | — | **4** |
| Garage wing (`x = +17.10`) | — | 1 | — | — | — | **1** |
| Garden shed (`x = −16.60`) | — | 1 | — | — | — | **1** |
| **Total** | **8** | **22** | **15** | **14** | **5** | **64** |

**66 → 64, and the two that went were impossible** (`HOUSE-00491`, owner decision 2026-09-10). The
`W_GABLE` row below described *"attic gable ends"*, and §12.1's main roof is a **hip**:
`roof_geometry.roof_planes` builds four planes meeting at a ridge, and a hip roof has no gable end
to put a louvre in. Measured, both stood **1.44 m inside solid roof** — a 0.80 m opening at
sill +11.30 in a wall the hip crosses at +10.57. The roof stays a hip; the attic is ventilated the
way a hip roof really is, and §12.1 says how.

Total windows: **64**, not the 81 designed here. The 17 are accounted for exactly — the 15 below,
plus `HOUSE-00491`'s two impossible louvres. Eight are
windows §13 scheduled for rooms in the middle of the plan that have no exterior wall at all —
`L0_LAUNDRY`, `L0_WC1`, `L0_WC2`, `L0_DINING`, `L1_BATH3`, `L1_WC4`, `L2_BATH5`, `L2_WC6` — and
those rows are corrected in §13. The other seven are the difference between this table's designed
per-elevation counts and §13's per-room ones, which never agreed: §13 totalled 71 openings against
this table's ~86, and only the attic agreed on both. §13's counts win, because they name the room
the window is in and a portal has to be in a room. One window is added that neither table had: the
garden shed's, which §13.7 describes in prose ("one window and one door").

The **openable / fixed** split is not a property of this file: `opacity` says what you can see
through, not what opens. It becomes measurable when `HOUSE-00378` authors the leaves in
`layout.openings.json`, and the 62/19 designed here is checked then.

---

## 13. Floor-by-floor room schedule

### 13.1 How to read these tables

* `X` and `Z` are the **partition centre-line box** of the cell. Visible floor area is the box
  inset by half the bounding partition thickness (0.075 m interior, 0.15 m exterior). The data
  file stores the centre-line box; the geometry builder applies the insets.
* A cell may be a **union of boxes** (marked `∪`). Portal traversal uses the union's AABB as the
  conservative bound and the individual boxes for the point-in-cell test. The `X` and `Z` columns
  of a marked row are that AABB, which is why the mark matters: the area beside it is smaller.
* Area is the **sum of the cell's boxes**, rounded to 0.1 m² — the L, not the rectangle around it.
* `Lights` counts controllable fixture *groups*, not bulbs.
* `Win` and `Doors` count the openings on the cell's **boundary**: every sash and every leaf you
  can see from inside the room, so a door between the hall and the WC counts for both of them.
  Cased openings are not doors. An em dash is none.

> The four numeric columns and the `∪` marks are measured from `assets-src/world/` by
> `tools/world/room_schedule.py` and gated as `room-schedule` (`HOUSE-00400`); the `X`/`Z` extents
> are compared and never rewritten, because which of the two is right is a question about the
> house. Before that gate existed the columns held a mixture of counts, attributions and prose —
> a central hall with three doorways written as `0`, a garage door count that omitted the loft
> hatch, and `2 sidelights + transom` where the schedule asked for a number — and no single rule
> reproduced them: counting by the leaf's `swing` cell matched 45 rows and counting the boundary
> matched 29. The descriptions moved into `Notes`, where prose belongs.

### 13.2 `B1` — Basement · FFL −2.30, ceiling +0.25, clear 2.55 m

Footprint: the main block only, interior X −12.70 … +8.70, Z −27.10 … −14.30.

| ID | Name | X | Z | Area | Lights | Win | Doors | Notes |
|---|---|---|---|---|---|---|---|---|
| `B1_STAIR` | Basement stair foot | +2.20 … +4.90 | −20.20 … −14.30 | 15.9 | 1 | — | 1 | Straight flight, under `STAIR_MAIN`; its only door is at the head, on L0 |
| `B1_HALL` | Basement hallway | −2.20 … +2.20 | −27.10 … −14.30 | 56.3 | 3 | — | 7 | Spine; exposed joists, duct trunk overhead |
| `B1_MECHANICAL` | Mechanical / HVAC | +4.90 … +8.70 | −18.30 … −14.30 | 15.2 | 1 | 1 | 1 | Furnace, air handler, water heater, water main, expansion tank |
| `B1_ELECTRICAL` | Electrical / service | +4.90 … +8.70 | −20.60 … −18.30 | 8.7 | 1 | — | 2 | Panel board, meter tails, structured-wiring cabinet |
| `B1_UTILITY` | Utility / drainage | +4.90 … +8.70 | −23.00 … −20.60 | 9.1 | 1 | — | 2 | Sump, ejector pit, main drain, softener, STACK-A/D landing |
| `B1_CINEMA` | Home cinema | +2.20 … +8.70 | −27.10 … −23.00 | 26.7 | 2 | — | 2 | Projector, screen, 6 seats, acoustic panels, no windows |
| `B1_WC7` | Basement WC | +2.20 … +3.80 | −23.00 … −21.20 | 2.9 | 1 | — | 1 | Off `B1_HALL`, on STACK-F |
| `B1_GYM` | Home gym | −8.20 … −2.20 | −18.30 … −14.30 | 24.0 | 2 | 2 | 2 | Rubber floor, mirror wall, treadmill, rack, bench |
| `B1_WORKSHOP` | Workshop | −12.70 … −8.20 | −18.30 … −14.30 | 18.0 | 2 | 2 | 1 | Bench, vice, pegboard, tool chest, dust extractor |
| `B1_STOR1` | Storage 1 | −12.70 … −8.20 | −22.00 … −18.30 | 16.6 | 1 | 1 | 1 | Steel shelving, labelled crates, STACK-B chase |
| `B1_STOR2` | Storage 2 | −12.70 … −8.20 | −27.10 … −22.00 | 23.0 | 1 | 2 | 1 | Seasonal goods, paint, luggage, STACK-C/E chase |
| `B1_HOBBY` | Hobby room | −8.20 … −2.20 | −24.00 … −18.30 | 34.2 | 2 | — | 4 | Model railway board, craft table, shelving |
| `B1_CELLAR` | Wine / root cellar | −8.20 … −4.60 | −27.10 … −24.00 | 11.2 | 1 | — | 1 | Racks, stone floor, cool, no heating duct |
| `B1_LAUNDRY2` | Secondary laundry / drying | −4.60 … −2.20 | −27.10 … −24.00 | 7.4 | 1 | — | 1 | Deep sink, drying rack, STACK-F |
| `B1_UNDERSTAIR` ∪ | Under-stair store | +2.20 … +4.90 | −23.00 … −20.20 | 4.7 | 1 | — | 1 | Low, sloped ceiling; the classic junk cupboard |

**15 cells. 273.9 m²**, which is the internal envelope exactly.

> Corrected 2026-09-07 by `HOUSE-00367`, which authored this table and measured it. It read
> "14 cells. 276.9 m² (≈ the 273.9 m² internal envelope; the difference is partition-centre-line
> accounting)". Both numbers were wrong in the same place: `B1_WC7` is tabulated **inside**
> `B1_UNDERSTAIR`, so the count dropped it and the area counted it twice. The store is the
> L-shaped remainder, 4.68 m² rather than 7.6, and the fifteen cells then cover the envelope with
> no overlap and no gap.

### 13.3 `L0` — Main floor · FFL +0.60, ceiling +3.30, clear 2.70 m

| ID | Name | X | Z | Area | Lights | Win | Doors | Notes |
|---|---|---|---|---|---|---|---|---|
| `L0_FOYER` | Entrance foyer | −2.20 … +2.20 | −18.30 … −14.30 | 17.6 | 2 | 3 | 2 | Console table, mirror, umbrella stand, coat hooks; the front door with two sidelights and a transom over |
| `L0_HALL` | Central hall | −2.20 … +2.20 | −23.00 … −18.30 | 20.7 | 2 | — | 3 | **Family photo gallery wall** (§59.4) |
| `L0_STAIR_MAIN` | Main staircase | +2.20 … +4.90 | −20.20 … −14.30 | 15.9 | 2 | 1 | — | Open to the foyer; the window is at the half-landing and the door is the basement flight's |
| `L0_MUDROOM` | Mudroom | +4.90 … +8.70 | −18.30 … −14.30 | 15.2 | 1 | 1 | 2 | Bench, cubbies, boots, leashes, dog towel; doors to the garage and the laundry |
| `L0_LAUNDRY` | Laundry room | +4.90 … +8.70 | −22.00 … −18.30 | 14.1 | 1 | — | 2 | Washer, dryer, folding counter, sink, STACK-D; no exterior wall (`HOUSE-00376`) |
| `L0_WC1` | Powder room | +2.20 … +4.90 | −22.00 … −20.20 | 4.9 | 1 | — | 1 | WC + basin, STACK-A; no exterior wall (`HOUSE-00376`) |
| `L0_FAMILY` | Family room | +2.20 … +8.70 | −27.10 … −22.00 | 33.2 | 3 | 2 | 2 | **Television**, sectional sofa, dog bed, bookshelves, media unit; two picture windows with measured static-open curtains (`HOUSE-01067`) |
| `L0_LIVING` | Formal living room | −8.20 … −2.20 | −20.20 … −14.30 | 35.4 | 3 | 2 | 3 | Fireplace on the west wall, piano, two armchairs; a bay window and a tall one |
| `L0_OFFICE` | Study / office | −12.70 … −8.20 | −18.30 … −14.30 | 18.0 | 2 | 2 | 2 | Desk, office chair, bookcases, filing cabinet, globe; two tall windows |
| `L0_WC2` | Guest powder room | −10.40 … −8.20 | −20.20 … −18.30 | 4.2 | 1 | — | 1 | STACK-B; no exterior wall (`HOUSE-00376`) |
| `L0_CLOSET_W` | Study closet | −12.70 … −10.40 | −20.20 … −18.30 | 4.4 | 1 | — | 1 | Archive boxes, stationery |
| `L0_DINING` | Formal dining room | −8.20 … −2.20 | −23.00 … −20.20 | 16.8 | 2 | — | 2 | Table for 8, sideboard, chandelier, china cabinet; no exterior wall (`HOUSE-00376`) |
| `L0_STOR` | Hall storage | −12.70 … −8.20 | −22.60 … −20.20 | 10.8 | 1 | — | 2 | Cleaning cupboard, vacuum, ironing board |
| `L0_BUTLERS` | Butler's pantry | −12.70 … −8.20 | −25.00 … −22.60 | 10.8 | 1 | 1 | 2 | Glass-front cabinets, counter, second sink, wine fridge |
| `L0_PANTRY` | Walk-in pantry | −12.70 … −8.20 | −27.10 … −25.00 | 9.5 | 1 | 1 | 3 | Shelved dry goods, **chest freezer**, step stool |
| `L0_KITCHEN` | Kitchen | −8.20 … +2.20 | −27.10 … −23.00 | 42.6 | 4 | 2 | 2 | Island, **large refrigerator**, range, ovens, dishwasher, sink |
| `L0_SUNROOM` | Sunroom / breakfast room (rear extension) | −6.70 … +2.70 | −32.10 … −27.10 | 47.0 | 2 | 6 | 1 | Breakfast table, wicker chairs, plants and wet bar; the visible suite and physical main/task fixtures are established by `HOUSE-01068`; five fixed panels and a slider to the terrace; roof = rear balcony |
| `L0_GARAGE` | Garage | +8.70 … +17.10 | −21.70 … −13.30 | 70.6 | 2 | 1 | 4 | **One car**, workbench, shelving, bins, bikes; sectional, side and house doors, plus the loft hatch |
| `L0_GARAGE_LOFT` | Garage storage loft | +9.20 … +16.60 | −21.20 … −17.50 | 27.4 | 1 | — | 1 | Nested in `L0_GARAGE` at +2.90; the ladder and 0.90 m hatch remain visible architecture, but the loft is not an intended-accessible area because the walk-only showcase has no ladder traversal (`HOUSE-00377`, PC-2026-09-23) |
| `L0_PORCH` | Front porch (exterior cell) | −3.60 … +3.60 | −14.30 … −11.60 | 19.4 | 1 | 3 | 1 | Two rockers, a doormat, a wall lantern each side of the door |

**19 cells. 410.9 m² incl. garage and porch; 273.9 m² of heated interior in the main block plus
47.0 m² sunroom.** The garage loft is a twentieth row in `layout.cells.json` and is deliberately
not in either figure: a mezzanine 2.90 m up is not floor area of `L0`, and its footprint is the
garage's, already counted (`HOUSE-00377`).

> Corrected 2026-09-08 by `HOUSE-00400`, walking §13 against `assets-src/world/`. It read
> "404.6 m² … 291.9 m² of heated interior". The garage is 8.4 × 8.4 = 70.56 m² and the table said
> 65.5, and the porch reaches the house's own front face at Z −14.30 rather than the wall's outer
> −14.00, so it is 7.2 × 2.7 = 19.44 m² and not 17.3. With those two corrected the floor measures
> 438.30 m², less the loft's 27.38 → **410.92**; and the heated main block is the 21.4 × 12.8
> plate, **273.92 m²**, not 291.9 — the old figure counted part of what it then added again as
> the sunroom.

### 13.4 `L1` — Upper floor 1 · FFL +3.65, ceiling +6.20, clear 2.55 m

| ID | Name | X | Z | Area | Lights | Win | Doors | Notes |
|---|---|---|---|---|---|---|---|---|
| `L1_LANDING` | Stair landing | −2.20 … +2.20 | −18.30 … −14.30 | 17.6 | 2 | 2 | 1 | Window seat; the **cat's favourite perch**; a door to the front balcony |
| `L1_STAIR_MAIN` | Staircase L1 | +2.20 … +4.90 | −20.20 … −14.30 | 15.9 | 1 | 1 | 1 | Continues to L2 |
| `L1_HALL` | Upper hall | −2.20 … +2.20 | −22.00 … −18.30 | 16.3 | 2 | — | 2 | Artwork, a console, a laundry hamper |
| `L1_HALL_W` | West corridor | −9.70 … −2.20 | −20.60 … −18.30 | 17.3 | 2 | — | 6 | Serves BED3, BED4, BATH2, WC3 |
| `L1_MASTER_BED` | Master bedroom | −7.00 … +2.20 | −27.10 … −22.00 | 46.9 | 4 | 3 | 3 | King bed, seating, dresser, TV (off), three windows and a slider to the rear balcony |
| `L1_MASTER_BATH` | Master bathroom | −12.70 … −9.40 | −27.10 … −22.00 | 16.8 | 3 | 1 | 1 | Double vanity, **freestanding bath**, **shower**, WC, one obscured window, STACK-C |
| `L1_MASTER_CLOSET` | Walk-in closet | −9.40 … −7.00 | −27.10 … −22.00 | 12.2 | 2 | — | 2 | Hanging rails, island drawers, shoe racks, mirror |
| `L1_BED2` | Bedroom 2 | +2.20 … +8.70 | −27.10 … −22.90 | 27.3 | 2 | 2 | 2 | Double bed, desk, wardrobe, bookshelf |
| `L1_LINEN` | Linen closet | +2.20 … +4.90 | −22.90 … −22.00 | 2.4 | 1 | — | 1 | Folded towels and bedding |
| `L1_STOR` | Upper store | +4.90 … +8.70 | −22.90 … −22.00 | 3.4 | 1 | — | 2 | Suitcases, spare bedding |
| `L1_BED3` | Bedroom 3 | −12.70 … −8.20 | −18.30 … −14.30 | 18.0 | 2 | 2 | 1 | Child's room: single bed, toy chest, wall posters |
| `L1_BED4` | Bedroom 4 | −8.20 … −2.20 | −18.30 … −14.30 | 24.0 | 2 | 2 | 1 | Teen's room: bed, desk, guitar, beanbag |
| `L1_BATH2` | Family bathroom | −12.70 … −9.70 | −22.00 … −18.30 | 11.1 | 2 | 1 | 1 | Bath + shower over, vanity, WC, one obscured window, STACK-B |
| `L1_WC3` | WC | −7.00 … −4.40 | −22.00 … −20.60 | 3.6 | 1 | — | 1 | WC + basin |
| `L1_CLOSET_2` | Hall closet | −9.70 … −7.00 | −22.00 … −20.60 | 3.8 | 1 | — | 1 | Coats, vacuum |
| `L1_CLOSET_3` | Hall closet | −4.40 … −2.20 | −22.00 … −20.60 | 3.1 | 1 | — | 1 | Cleaning supplies |
| `L1_BED5` | Guest bedroom | +4.90 … +8.70 | −18.30 … −14.30 | 15.2 | 2 | 1 | 2 | Double bed, chair, wardrobe, made up, unused |
| `L1_BATH3` | Guest bathroom | +4.90 … +8.70 | −22.00 … −18.30 | 14.1 | 2 | — | 2 | Shower, vanity, WC, STACK-D; no exterior wall (`HOUSE-00376`) |
| `L1_WC4` | WC | +2.20 … +4.90 | −22.00 … −20.20 | 4.9 | 1 | — | 1 | STACK-A; no exterior wall (`HOUSE-00376`) |
| `L1_BALCONY_REAR` | Rear balcony (exterior) | −6.70 … +2.70 | −32.10 … −27.10 | 47.0 | 2 | 5 | 1 | Over the sunroom; table, two loungers, planters, 1.10 m railing |
| `L1_BALCONY_FRONT` | Front balcony (exterior) | −3.60 … +3.60 | −14.30 … −11.60 | 19.4 | 1 | 4 | 1 | Over the porch; two chairs, a small table |

**21 cells. 273.9 m² interior + 66.4 m² of balcony.**

### 13.5 `L2` — Upper floor 2 · FFL +6.55, ceiling +9.00, clear 2.45 m

| ID | Name | X | Z | Area | Lights | Win | Doors | Notes |
|---|---|---|---|---|---|---|---|---|
| `L2_LANDING` | Stair landing | −2.20 … +2.20 | −18.30 … −14.30 | 17.6 | 2 | 2 | 2 | Juliet balcony over the front |
| `L2_STAIR_MAIN` | Staircase L2 head | +2.20 … +4.90 | −20.20 … −14.30 | 15.9 | 1 | 1 | 1 |  |
| `L2_STAIR_ATTIC` | Attic stair | +4.90 … +8.70 | −18.30 … −14.30 | 15.2 | 1 | — | 1 | Straight, narrow, painted timber, a bare bulb on a pull cord |
| `L2_HALL` | Upper hall | −2.20 … +2.20 | −22.00 … −18.30 | 16.3 | 2 | — | 2 |  |
| `L2_HALL_W` | West corridor | −9.70 … −2.20 | −20.60 … −18.30 | 17.3 | 2 | — | 4 |  |
| `L2_LIBRARY` | Library | −8.20 … −2.20 | −18.30 … −14.30 | 24.0 | 3 | 2 | 2 | Floor-to-ceiling shelves, reading chairs, a globe, a ladder |
| `L2_BED6` | Bedroom 6 | −12.70 … −8.20 | −18.30 … −14.30 | 18.0 | 2 | 2 | 1 | Guest / sewing room: bed, sewing table, dress form |
| `L2_BATH4` | Bathroom 4 | −12.70 … −9.70 | −22.00 … −18.30 | 11.1 | 2 | 1 | 2 | Shower, vanity, WC, one obscured window, STACK-B |
| `L2_CLOSET_4` | Hall closet | −9.70 … −7.00 | −22.00 … −20.60 | 3.8 | 1 | — | 1 | |
| `L2_WC5` | WC | −7.00 … −4.40 | −22.00 … −20.60 | 3.6 | 1 | — | 1 | |
| `L2_LINEN2` | Linen closet | −4.40 … −2.20 | −22.00 … −20.60 | 3.1 | 1 | — | 1 | |
| `L2_BED7` | Bedroom 7 | −12.70 … −7.00 | −27.10 … −22.00 | 29.1 | 2 | 2 | 1 | Big north bedroom, currently a store-room-in-waiting |
| `L2_GAMES` | Games / hobby room | −7.00 … +2.20 | −27.10 … −22.00 | 46.9 | 3 | 2 | 2 | Pool table, dartboard, sofa, arcade cabinet, model shelves |
| `L2_SITTING` | Secondary sitting room | +2.20 … +8.70 | −27.10 … −23.10 | 26.0 | 2 | 2 | 1 | Sofa, coffee table, record player, plants |
| `L2_STOR2` | Store | +2.20 … +8.70 | −23.10 … −22.00 | 7.2 | 1 | — | 3 | 1.10 m connector; all three leaves swing into their destination rooms |
| `L2_BATH5` | Bathroom 5 | +4.90 … +8.70 | −22.00 … −18.30 | 14.1 | 2 | — | 1 | Bath, vanity, WC, STACK-D; no exterior wall (`HOUSE-00376`) |
| `L2_WC6` | WC | +2.20 … +4.90 | −22.00 … −20.20 | 4.9 | 1 | — | 1 | STACK-A; no exterior wall (`HOUSE-00376`) |
| `L2_BALCONY_JULIET` | Juliet balcony (exterior) | −1.00 … +1.00 | −14.30 … −13.85 | 0.9 | — | — | 1 | Doors open onto a railing; standing room only |

**18 cells. 273.9 m² interior + 0.9 m² juliet balcony.**

> Corrected 2026-09-07 by `HOUSE-00370`. `L2_STAIR_ATTIC` read "+5.40 … +8.20 | 11.2", which
> leaves 0.50 m strips either side of it — 4.00 m² of the envelope in no cell at all, which §16.4
> step 4 calls a world-data bug. At the full bay the interior totals 273.9 m², exactly the
> envelope and exactly what `B1`, `L0` and `L1` come to. `L3_STAIR_HEAD` had the same footprint
> and the same fix.
>
> Corrected 2026-09-23 by `HOUSE-03226`. The store connector was 0.90 m deep and all three door
> leaves swung into it, blocking the only controller route to `L2_BATH5`. Moving its shared
> boundary with `L2_SITTING` by 0.20 m and swinging each existing leaf into the adjacent room gives
> the route usable clearance without changing the envelope, cell set or total area.

### 13.6 `L3` — Attic · floor +9.30, rafter ceiling to +14.30

Knee walls 1.20 m tall stand 1.60 m in from the eaves on the north and south. The finished part
has a collar-tie ceiling at +12.60; the rest is open to the rafters with visible trusses,
insulation batts between the joists in the unfinished bays, and rough boarding on the walkways.

| ID | Name | X | Z | Area | Head-room | Lights | Win | Notes |
|---|---|---|---|---|---|---|---|---|
| `L3_STAIR_HEAD` | Attic stair head | +4.90 … +8.70 | −18.30 … −14.30 | 15.2 | 2.2 → 3.4 m | 1 | — | Dormer above the head gives standing room |
| `L3_ROOM` ∪ | Finished attic room | −6.00 … +4.90 | −24.00 … −17.00, plus two dormer bays to −14.30 | 86.3 | 2.4 → 5.0 m | 2 | 3 | Boarded floor, plastered collar ceiling, lit by three dormers; an old sofa, a desk, boxes, a rocking horse, a train set |
| `L3_STORE_W` | West attic store | −12.70 … −6.00 | −27.10 … −14.30 | 85.8 | 1.2 → 4.6 m | 1 | — | Unfinished: rafters, insulation, walkway boards, 40 boxes, a wardrobe, suitcases. Ventilated by the eaves and the ridge, not by a window (`HOUSE-00491`) |
| `L3_STORE_E` | East attic / services | +4.90 … +8.70 | −27.10 … −18.30 | 33.4 | 1.2 → 4.2 m | 1 | — | Header tank, HVAC branch ducts, an aerial mast, cable runs. Ventilated by the eaves and the ridge, not by a window (`HOUSE-00491`) |
| `L3_STORE_N` | North attic store | −6.00 … +4.90 | −27.10 … −24.00 | 33.8 | 1.2 → 3.2 m | 1 | 2 | Christmas decorations, a cot, framed pictures; two dormers |
| `L3_STORE_S` | South attic store | −3.60 … +3.60 | −17.00 … −14.30 | 19.4 | 1.2 → 3.0 m | 1 | — | Roof-space void behind the front knee wall |

> Corrected 2026-09-07 by `HOUSE-00376`. `L3_ROOM` was scheduled 3 dormers and its footprint
> stopped 2.70 m short of the front wall, so there was no face for any of them to be in — the
> `L3_STORE_S` roof void was in the way. A dormer belongs to the room it lights, so the room now
> reaches the eaves in two bays, X −6.00…−3.60 and +3.60…+4.90, and the void is what is left
> between them. The bays are at the ends of the elevation and not spread across it because the
> porch roof and the front balcony fill X −3.60…+3.60 up to +9.00 and there is no exterior cell at
> all above them. The two areas still sum to 105.7 m², and `P_L3_STORE_W__L3_STORE_S` is gone: the
> west bay is exactly the corner where the two voids used to meet.

**6 cells, 273.9 m² gross, ≈ 163 m² with ≥ 1.90 m headroom.** (Corrected 2026-09-07 with
`L2_STAIR_ATTIC`; see §13.5.) Player movement in the attic uses a
**headroom-aware capsule**: below 1.85 m of clearance the controller automatically crouches
(capsule 1.25 m, eye 1.15 m, speed ×0.55) and the avatar plays a stooped walk. This is the only
place crouching exists, and it exists because the roof geometry demands it.

### 13.7 Exterior cells

| ID | Name | Approximate extent | Notes |
|---|---|---|---|
| `EXT_ROAD` | Carriageway + sidewalks | X −35…+35, Z 0…+13.4 | Player spawn |
| `EXT_FRONTYARD_W` ∪ | Front lawn, west | X −22.5…−1, Z −14…0 |  |
| `EXT_FRONTYARD_E` ∪ | Front lawn, east | X +1…+9.5, Z −13…0 |  |
| `EXT_WALK` | Front walk | X −1…+1, Z −11.6…0 | |
| `EXT_DRIVEWAY` | Driveway + apron | X +9.6…+16.8, Z −13…0 | |
| `EXT_SIDEYARD_W` ∪ | West side yard | X −22.5…−13, Z −48…0 |  |
| `EXT_SIDEYARD_E` ∪ | East side yard | X +17.4…+22.5, Z −48…0 |  |
| `EXT_TERRACE` | Rear terrace | X −6.7…+6.7, Z −36…−32.4 | |
| `EXT_BACKYARD` ∪ | Rear lawn | X −22.5…+22.5, Z −44…−32.4 |  |
| `EXT_GARDEN` ∪ | Vegetable garden | X −20…−13, Z −44…−36.5 |  |
| `EXT_ORCHARD` | Orchard corner | X +12…+21, Z −46…−38 | |
| `EXT_SHED` | Garden shed interior | X −19.8…−16.6, Z −43.8…−40.6 | Enterable, one window, one door |
| `EXT_NORTHSTRIP` | Behind the rear fence | X −22.5…+22.5, Z −52…−48 | Visual only, out of bounds |
| `L0_PORCH`, `L1_BALCONY_REAR`, `L1_BALCONY_FRONT`, `L2_BALCONY_JULIET` | see above | | Exterior cells attached to the house |
| `EXT_WORLD` ∪ | Everything beyond the fences | the rest | One huge cell holding terrain, road, neighbourhood, sky; internally culled by frustum + distance + LOD |

**18 exterior cells** (counting the 4 attached ones), **76 interior cells**, **94 total** —
plus the 2 nested sub-cells the data needs and this schedule does not list as rooms, the container
interiors of `HOUSE-00373`. So `layout.cells.json` holds **96**.

> Corrected 2026-09-08 by `HOUSE-00400`, counting the rows again. It read "75 interior cells, 93
> total — plus the 3 nested sub-cells … the two container interiors and the garage storage loft".
> §13.3 has **20** rows, not the 19 its summary states: the summary deliberately excludes the
> garage loft, and the recount below took that figure for the row count. The loft therefore *is*
> tabulated as a room, so there are two nested sub-cells this schedule omits and not three:
> 15 + 20 + 21 + 18 + 6 = **80** rows in §13.2–§13.6, of which 4 are the attached exterior cells,
> leaving **76** interior; §13.7 lists 14 `EXT_*` ids, so **18** exterior and **94** tabulated;
> 94 + 2 = 96.

> Corrected 2026-09-07 by `HOUSE-00372`, which authored these and counted the tables. It read
> "17 exterior cells (counting the 4 attached ones), 78 interior cells, 95 total". Counted from
> §13.2–§13.7 themselves: B1 15 + L0 19 + L1 21 + L2 18 + L3 6 = 79 rows, of which 4 are the
> attached exterior cells, so 75 interior; §13.7 lists 14 `EXT_*` ids including `EXT_WORLD`, so
> 18 exterior. The summary counted three interior cells that are not tabulated anywhere and left
> `EXT_WORLD` out of the exterior count.

### 13.8 Fixture count check against the brief

| Required | Delivered |
|---|---|
| ≥ 3 toilets / WCs | **7 WC-only rooms**: `B1_WC7`, `L0_WC1`, `L0_WC2`, `L1_WC3`, `L1_WC4`, `L2_WC5`, `L2_WC6`, plus a WC in each bathroom (said "8" over a list of seven until 2026-09-08, `HOUSE-00413`; the ids run `WC1`…`WC7` with no gap) |
| ≥ 3 bathrooms | **5 bathrooms**: `L1_MASTER_BATH`, `L1_BATH2`, `L1_BATH3`, `L2_BATH4`, `L2_BATH5` |
| Toilet fixtures total | **12** (7 WCs + 5 bathrooms) |
| Showers / baths | **4 showers** (master, `L1_BATH2` over the bath, `L1_BATH3`, `L2_BATH4`), 3 baths (master has both) — "3 showers" corrected 2026-09-08, `HOUSE-00412` |
| Bedrooms | 7 (`L1_MASTER_BED`, `L1_BED2..5`, `L2_BED6`, `L2_BED7`) |
| Basement spaces | 14 |
| Attic spaces | 6 |
| Balconies | 3 (rear, front, juliet) |
| Garage | 1, with one car |

---

## 14. Coordinate system

Restating §10.1 as the normative reference, because everything depends on it.

```
        −Z  NORTH
            ▲
            │
 WEST  −X ──┼── +X  EAST                 +Y is UP
            │
            ▼
        +Z  SOUTH   (the road is here)
```

* Right-handed, Y-up, camera forward `−Z`. Identical to glTF; **no conversion at import**.
* `Matrix::CreateLookAt(eye, target, Vector3::Up)` and
  `Matrix::CreatePerspectiveFieldOfView(fovY, aspect, near, far)` are used unchanged.
* Yaw `θ` gives forward `(sin θ, 0, −cos θ)`. θ = 0 → north; θ = π/2 → east.
* Pitch `φ` ∈ [−1.484, +1.484] rad (±85°).
* Object placement in the world data is `{position:[x,y,z], yawDeg, pitchDeg?, rollDeg?, scale?}`
  composed as `S · R(pitch) · R(yaw) · R(roll) · T` in XNA's row-vector convention, i.e.
  `Matrix::CreateScale * Matrix::CreateFromYawPitchRoll * Matrix::CreateTranslation`.
* glTF matrices are column-major/column-vector; CNA's importer already performs the layout
  conversion (`docs/gltf-conventions.md:70-90`). Nothing in `cna-house` re-does it.
* **Front-face winding**: glTF front faces are CCW; the game therefore binds
  `RasterizerState::CullClockwise` for all model draws and flips to `CullCounterClockwise` only
  for negatively-scaled (mirrored) placements. Procedurally generated geometry authored by
  `cna-house` tools follows the same glTF CCW convention so one state serves everything.

---

## 15. Machine-readable floor-plan schema

### 15.1 Files

All under `assets-src/world/` (authored) → compiled and copied to `content/world/` (runtime).
The runtime reads them as `.cnb`-wrapped JSON blobs through a custom `ContentTypeReader`, or —
simpler and chosen — as **deployed raw `.json` files** listed as deployment-support files by the
content pipeline and read through `System::IO::File` + `System::Text::Json`. They are data the
game owns, not framework content, so no reader registration is needed.

| File | Contents |
|---|---|
| `world.manifest.json` | schema version, list of member files with SHA-256, `worldHash` |
| `layout.levels.json` | the 5 levels + elevations + construction constants |
| `layout.cells.json` | every cell: id, level, name, box(es), kind, surface, acoustic, thermal, default lighting |
| `layout.portals.json` | every portal: id, cells, rectangle, kind, aperture entity, opacity |
| `layout.openings.json` | doors and windows as *geometry + entity*: leaf size, hinge side, swing direction, frame |
| `layout.stairs.json` | flights: from/to cell, risers, rise, going, width, landings, collision ramp, surface |
| `layout.lights.json` | every light: id, cell, type, transform, colour, intensity, range, switch group, default state |
| `layout.props.json` | static and dynamic prop placements: asset id, transform, cell, LOD group, collision, interactable ref |
| `layout.materials.json` | material definitions (§22) |
| `layout.nav.json` | pet navigation graph: nodes, edges, perches, beds, forbidden zones |
| `layout.audio.json` | audio zones, ambience beds, emitter placements, portal transmission losses |
| `layout.exterior.json` | terrain reference, road, fences, neighbourhood instances, vegetation instances |
| `layout.weather.json` | archetypes, transition matrices, seasonal tables |
| `layout.sky.json` | sky gradient LUTs, cloud layer definitions, star catalogue reference |
| `interactables.json` | every interactable: id, kind, cell, bounds, actions, initial state, sounds, persistence |
| `initialstate.json` | the canonical initial state (§40 of the brief; §65.6 here) |
| `assets.manifest.json` | the asset manifest (§20.3) |

### 15.2 Schema — levels

```jsonc
{
  "schema": "cna-house/levels/1",
  "units": "metres",
  "north": "-Z",
  "levels": [
    { "id": "B1", "name": "Basement",      "ffl": -2.30, "ceiling":  0.25, "structureDepth": 0.35 },
    { "id": "L0", "name": "Main Floor",    "ffl":  0.60, "ceiling":  3.30, "structureDepth": 0.35 },
    { "id": "L1", "name": "Upper Floor 1", "ffl":  3.65, "ceiling":  6.20, "structureDepth": 0.35 },
    { "id": "L2", "name": "Upper Floor 2", "ffl":  6.55, "ceiling":  9.00, "structureDepth": 0.35 },
    { "id": "L3", "name": "Attic",         "ffl":  9.30, "ceiling": null,  "roof": "ROOF_MAIN" }
  ],
  "construction": {
    "wallExterior": 0.30, "wallPartition": 0.15, "wallPlumbing": 0.20, "wallGarage": 0.25,
    "foundationWall": 0.30, "kneeWallHeight": 1.20, "ridgeY": 14.30, "roofPitch": 0.594,
    "skirting": 0.14, "cornice": 0.11, "balustrade": 0.95, "railing": 1.10
  }
}
```

### 15.3 Schema — cells

```jsonc
{
  "schema": "cna-house/cells/1",
  "cells": [
    {
      "id": "L0_KITCHEN",
      "level": "L0",
      "name": "Kitchen",
      "kind": "room",                    // room | corridor | stair | closet | garage | exterior | void
      "boxes": [ { "x": [-8.20, 2.20], "z": [-27.10, -23.00] } ],
      "yOverride": null,                 // null = level ffl..ceiling
      "floorMaterial": "MAT_TILE_PORCELAIN_GREY",
      "wallMaterial":  "MAT_PAINT_WARM_WHITE",
      "ceilingMaterial": "MAT_PAINT_FLAT_WHITE",
      "trimMaterial": "MAT_DOOR_PAINTED",       // unlightmapped joinery finish
      "footstepSurface": "tile",
      "acoustic": { "roomTone": "AMB_KITCHEN", "absorption": 0.28, "reverbHint": "small_hard" },
      "thermal": { "heated": true, "ductBranch": "DUCT_L0_W" },
      "lightGroups": ["LG_L0_KITCHEN_MAIN", "LG_L0_KITCHEN_UNDERCAB", "LG_L0_KITCHEN_ISLAND", "LG_L0_KITCHEN_SINK"],
      "daylight": { "windowIds": ["W_L0_KITCHEN_N1","W_L0_KITCHEN_N2"], "orientation": "N", "exposure": 0.55 },
      "residencyPack": "house-l0",
      "lodBias": 0,
      "visibilityHint": "opaque",        // opaque | open (cells with no walls, e.g. exterior)
      "navMeshRegion": "NAV_L0_KITCHEN"
    }
  ]
}
```

### 15.4 Schema — portals

A portal is always an **axis-aligned rectangle on an axis-aligned plane**. That restriction is
deliberate: it makes the clipping test exact and cheap, and every door and window in this house is
in a rectilinear wall.

```jsonc
{
  "schema": "cna-house/portals/1",
  "portals": [
    {
      "id": "P_L0_HALL__L0_KITCHEN",
      "cellA": "L0_HALL",  "cellB": "L0_KITCHEN",
      "plane": { "axis": "z", "value": -23.00 },
      "rect":  { "u": [-1.00, 1.00], "v": [0.60, 2.80] },  // u = the other horizontal axis, v = world Y
      "kind": "cased_opening",     // cased_opening | door | double_door | slider | window | garage_door | stair_well | exterior_door | hatch
      "aperture": null,            // null = always fully open
      "opacity": "open",           // open | opaque_when_closed | translucent | glass
      "maxDepth": null,            // optional per-portal traversal cap
      "soundLoss": { "open": 0.0, "closed": 0.0 }
    },
    {
      "id": "P_L0_HALL__L0_WC1",
      "cellA": "L0_HALL", "cellB": "L0_WC1",
      "plane": { "axis": "x", "value": 2.20 },
      "rect": { "u": [-21.35, -20.45], "v": [0.60, 2.70] },
      "kind": "door",
      "aperture": "DOOR_L0_WC1",
      "opacity": "opaque_when_closed",
      "soundLoss": { "open": 0.05, "closed": 0.55 }
    },
    {
      "id": "P_L1_MASTER_BED__EXT_WORLD__W1",
      "cellA": "L1_MASTER_BED", "cellB": "EXT_WORLD",
      "plane": { "axis": "z", "value": -27.40 },
      "rect": { "u": [-2.40, 0.00], "v": [3.65, 5.80] },
      "kind": "slider",
      "aperture": "WIN_L1_MASTER_S1",
      "opacity": "glass",          // always visually passable; blocks the player when closed
      "maxDepth": 2,
      "soundLoss": { "open": 0.02, "closed": 0.42 }
    }
  ]
}
```

`opacity` semantics:

| Value | Vision when closed | Player passage when closed | Sound when closed |
|---|---|---|---|
| `open` | passes | passes | passes |
| `opaque_when_closed` | **blocked** | blocked | attenuated by `soundLoss.closed` |
| `glass` | passes (with `maxDepth` cap) | blocked | attenuated |
| `translucent` | passes, but the reduced frustum is marked *diffuse* so the target cell renders at LOD+1 and no small props | blocked | attenuated |

### 15.5 Schema — lights

```jsonc
{
  "id": "LIGHT_L0_KITCHEN_ISLAND_1",
  "cell": "L0_KITCHEN",
  "group": "LG_L0_KITCHEN_ISLAND",
  "type": "point",                       // point | spot | directional | area_proxy | emissive_only
  "position": [-3.00, 2.85, -25.10],
  "direction": [0, -1, 0],
  "colorK": 2700,                        // colour temperature in kelvin → RGB via a Planckian LUT
  "intensityLm": 800,
  "range": 6.0,
  "coneInnerDeg": 30, "coneOuterDeg": 55,
  "fixtureProp": "PROP_L0_KITCHEN_PENDANT_1",
  "emissiveMaterialSlot": "shade",
  "castsBlobShadow": true,
  "bakedIntoLightmap": true,
  "defaultOn": false
}
```

`fixtureProp` is an optional link to the visible source of the light. When it is present, the prop
must exist in the same cell and `emissiveMaterialSlot` names the exact source material slot in its
model. That slot's manifest bridge resolves to an `emissive` canonical material. The content build
keeps differently switched emitters in separate static chunks, and the production Tier-S pass
tints the physical diffuser from the light group's live Kelvin colour and switch-on envelope.
This is a stock `BasicEffect` material path, not a custom shader or a second light-state system.

### 15.6 Schema — interactables

```jsonc
{
  "id": "FRIDGE_L0_KITCHEN",
  "kind": "refrigerator",
  "cell": "L0_KITCHEN",
  "prop": "PROP_L0_KITCHEN_FRIDGE",
  "focus": { "point": [1.20, 1.40, -26.40], "normal": [0, 0, 1], "radius": 0.85 },
  "bounds": { "min": [0.30, 0.60, -27.05], "max": [2.10, 2.55, -26.35] },
  "actions": [
    { "verb": "Open",  "when": "state.doorOpen == false", "do": "setDoor(true)",  "sound": "SFX_FRIDGE_OPEN",  "anim": "door", "duration": 0.9 },
    { "verb": "Close", "when": "state.doorOpen == true",  "do": "setDoor(false)", "sound": "SFX_FRIDGE_CLOSE", "anim": "door", "duration": 0.7 }
  ],
  "childInteractables": ["FRIDGE_ITEM_MILK_1", "FRIDGE_ITEM_EGGS_1", "..."],
  "state": { "doorOpen": false, "interiorLightOn": false, "compressorRunning": true },
  "persist": ["doorOpen", "removedItems"],
  "audio": { "loop": "AMB_FRIDGE_HUM", "emitter": [1.20, 0.90, -26.70] },
  "portal": "P_FRIDGE_INTERIOR"
}
```

The `when`/`do` strings are **not** a general scripting language. They are parsed at load time into
a tiny fixed expression tree over the interactable's own typed state fields, with a closed
vocabulary of operations per `kind`. An unknown token is a **load-time error**, not a runtime
surprise. This keeps the data declarative without inventing a VM.

### 15.7 Validation

`tools/world/validate_world.py` (and a mirrored C++ validator used by the unit tests) checks:

1. every id is unique and matches `^[A-Z][A-Z0-9_]*$`;
2. every cell box is non-degenerate and lies within its level's envelope;
3. no two cells on the same level overlap by more than 1 cm²;
4. every portal's rectangle lies **in** both cells' boundary planes within 1 cm — or, when the two
   cells do not abut, **in the wall between them**: their facing planes must straddle the portal,
   each must span the opening, and they must be no further apart than the thickest wall
   `construction` declares. Widened 2026-09-07 by `HOUSE-00376`, because the building's shell is
   not a partition: a room stops at the interior face and the yard outside stops at the exterior
   one, so every one of the 73 windows is in a plane that belongs to neither cell. The `v` range
   still lies inside both cells' vertical extent;
5. the portal graph is connected: every cell that is not `void` is reachable from `L0_FOYER`
   through *always-open or door* portals — corrected 2026-09-07 from "every interior cell", which
   is what let `EXT_SHED` be authored with no door at all (`HOUSE-00374`): an `exterior` cell that
   is roofed and `visibilityHint: opaque` is a building, and a building you cannot enter is the
   same defect as a room you cannot enter;
6. every referenced material, asset, light group, sound, nav region and animation exists — **and
   every value the data caches matches what it is derived from**: a cell's `lightGroups` is exactly
   the groups its own lights belong to, which §28.1 walks once per frame (`HOUSE-00381`); every
   state field of a `light_switch` names a real group, because a gang is a group and a typo in one
   is a switch that toggles nothing (`HOUSE-00384`); every portal's `soundLoss` is §64.3's figure
   for its transmission class, because two copies of one fact drift (`HOUSE-00388`); and a cell's
   `thermal.ductBranch` and that branch's `cells` list agree, because a branch that has lost a room
   is a room the furnace is silent in (`HOUSE-00387`); and a nav edge that names a portal joins
   **that** portal's two cells, because a route through a door goes through that door
   (`HOUSE-00389`);
7. every door has exactly one portal and every window has exactly one portal, **and the portal
   names the leaf back** in `aperture` — `aperture: null` means "always fully open", so a door that
   leaves it null claims to be a hole. Every kind with something that opens is covered,
   `garage_door` and `hatch` included (widened 2026-09-07 by `HOUSE-00378`, which found 63 doors
   with a null `aperture` and two kinds the rule had never looked at);
8. stair flights connect the declared cells — a portal has to join them, or the flight is a
   staircase into a wall — and their total rise equals the level difference to within 1 mm, or,
   for a flight between two cells on **one** level, the `fromY`/`toY` it declares, since one level
   has no level difference to check against (widened 2026-09-07 by `HOUSE-00379`);
9. plumbing: every fixture's cell appears in a declared stack;
10. realism checks (§66 of the brief; §70.5 here): door heights, ceiling heights, counter heights,
    stair `2R + G`, human/pet/car scale, capsule clearance through every portal, and every light
    inside the cell it names — a fixture 30 m from its room resolves perfectly and lights nothing
    (added 2026-09-08 by `HOUSE-00383`);
11. every interactable's `focus.point` is inside its cell and reachable by a 2.5 m ray from a
    standing eye position on the room's floor — a **reachability proof**, not an assumption;
12. **nothing outdoors stands in something else**: no two of §11's `structures` overlap, no path
    box runs into one, and no vegetation instance is inside one (added 2026-09-09 by
    `HOUSE-00769`). Rules 2 and 3 do this for the house and nothing did it for the lot, which has
    three kinds of rectangle that can be authored on top of each other — and two of them were: the
    garden path ran three metres through the shed and reached no door, and three of §11.1's six
    raised beds were points inside the shed's walls. Both were invisible, because a path and a
    shed pad are both gravel at the same height and a bed authored as an instance has no size for
    anything to overlap.
13. **every downspout is at a roof corner, on the ground under it**: the ids are exactly the ones
    `roof_geometry.house_downspouts` derives, each row's head is that corner at the gutter to the
    millimetre, and its splash point is directly below on §11.5's height field (added 2026-09-09 by
    `HOUSE-00776`). The pipes have been drawn since `HOUSE-00468` and nothing outside the shell
    knew they existed; §37.3's splashes and §37.4's trickle emitter both need to know where the
    water lands, and neither can read a Blender mesh.
14. **every movable leaf has a safe authored static pose**: leafed portals and gates declare an
    open fraction, intended-accessible routes retain at least 0.70 m of clearance, and a hinged
    leaf's complete swept arc remains inside its swing cell without meeting a wall or placed prop
    (added 2026-09-22 by `HOUSE-03224`). The showcase does not animate interaction gameplay, so
    the authored pose is both what collision exposes and what the shell draws.
15. **every intended-accessible cell has a physically valid arrival point**: `docs/zones.json`
    assigns all source cells exactly once, excludes exactly the two scenery cells, the Juliet
    balcony and the two appliance interiors with reasons, and gives every other cell a feet
    position on walkable floor, stair or terrain collision. The 0.31 m-radius capsule remains in
    the cell footprint and has headroom for the row's standing or explicitly crouched posture
    (added 2026-09-23 by `HOUSE-03225`).

Validation runs in CI and as a pre-build step. A failure fails the build.

---

## 16. Room / portal graph

### 16.1 Adjacency table — `L0`

`○` = cased opening (always open) · `▣` = hinged door · `◫` = double door · `▤` = slider ·
`▦` = garage door · `▥` = window · `↕` = stair well · `·` = shares a wall, no portal.

| From → To | Portal | Kind | Notes |
|---|---|---|---|
| `EXT_FRONTYARD_W` → `L0_PORCH` | `P_EXT_FRONT_W__L0_PORCH` | ○ | Open-sided porch, west |
| `EXT_FRONTYARD_E` → `L0_PORCH` | `P_EXT_FRONT_E__L0_PORCH` | ○ | Open-sided porch, east |
| `EXT_WALK` → `L0_PORCH` | `P_EXT_WALK__L0_PORCH` | ○ | Up the steps from the front walk |
| `L0_PORCH` → `L0_FOYER` | `P_L0_PORCH__L0_FOYER` | ▣ | **Front door**, 1.00 × 2.15, opens inward, hinge west |
| `L0_FOYER` → `L0_HALL` | `P_L0_FOYER__L0_HALL` | ○ | 3.00 m wide |
| `L0_FOYER` → `L0_LIVING` | `P_L0_FOYER__L0_LIVING` | ◫ | Double doors, hinge both jambs |
| `L0_FOYER` → `L0_STAIR_MAIN` | `P_L0_FOYER__L0_STAIR` | ○ | Open stair hall |
| `L0_STAIR_MAIN` → `L1_STAIR_MAIN` | `P_STAIR_L0_L1` | ↕ | Stair well, always open |
| `L0_STAIR_MAIN` → `B1_STAIR` | `P_L0_STAIR__B1_STAIR` | ▣ | Basement door, opens into the stair |
| `L0_STAIR_MAIN` → `L0_MUDROOM` | `P_L0_STAIR__L0_MUDROOM` | ○ | |
| `L0_MUDROOM` → `L0_GARAGE` | `P_L0_MUDROOM__L0_GARAGE` | ▣ | House-to-garage door, self-closing, 3 steps down |
| `L0_MUDROOM` → `L0_LAUNDRY` | `P_L0_MUDROOM__L0_LAUNDRY` | ▣ | |
| `L0_HALL` → `L0_WC1` | `P_L0_HALL__L0_WC1` | ▣ | |
| `L0_HALL` → `L0_DINING` | `P_L0_HALL__L0_DINING` | ◫ | |
| `L0_HALL` → `L0_KITCHEN` | `P_L0_HALL__L0_KITCHEN` | ○ | 2.00 m wide |
| `L0_HALL` → `L0_FAMILY` | `P_L0_HALL__L0_FAMILY` | ▣ | 1.00 m of shared wall at Z −23.00…−22.00 |
| `L0_LIVING` → `L0_DINING` | `P_L0_LIVING__L0_DINING` | ○ | 2.60 m cased opening |
| `L0_LIVING` → `L0_OFFICE` | `P_L0_LIVING__L0_OFFICE` | ◫ | Glazed double doors — **translucent portal** |
| `L0_LIVING` → `L0_WC2` | `P_L0_LIVING__L0_WC2` | ▣ | |
| `L0_OFFICE` → `L0_CLOSET_W` | `P_L0_OFFICE__L0_CLOSET_W` | ▣ | |
| `L0_DINING` → `L0_KITCHEN` | `P_L0_DINING__L0_KITCHEN` | ○ | |
| `L0_DINING` → `L0_STOR` | `P_L0_DINING__L0_STOR` | ▣ | |
| `L0_KITCHEN` → `L0_BUTLERS` | `P_L0_KITCHEN__L0_BUTLERS` | ○ | |
| `L0_KITCHEN` → `L0_PANTRY` | `P_L0_KITCHEN__L0_PANTRY` | ▣ | |
| `L0_KITCHEN` → `L0_FAMILY` | `P_L0_KITCHEN__L0_FAMILY` | ○ | 3.10 m — the main open-plan link |
| `L0_KITCHEN` → `L0_SUNROOM` | `P_L0_KITCHEN__L0_SUNROOM` | ○ | 4.00 m — through the old exterior wall |
| `L0_FAMILY` → `L0_LAUNDRY` | `P_L0_FAMILY__L0_LAUNDRY` | ▣ | |
| `L0_SUNROOM` → `EXT_TERRACE` | `P_L0_SUNROOM__EXT_TERRACE` | ▤ | 2.40 m slider, one step down |
| `L0_GARAGE` → `EXT_DRIVEWAY` | `P_L0_GARAGE__EXT_DRIVEWAY` | ▦ | Sectional door, 4.90 × 2.40 |
| `L0_GARAGE` → `EXT_SIDEYARD_E` | `P_L0_GARAGE__EXT_SIDEYARD_E` | ▣ | Side door |
| every window | `P_<cell>__EXT_WORLD__W<n>` | ▥ | 62 openable + 19 fixed |

### 16.2 Adjacency table — vertical

| Portal | From | To | Kind |
|---|---|---|---|
| `P_STAIR_L0_L1` | `L0_STAIR_MAIN` | `L1_STAIR_MAIN` | stair well |
| `P_STAIR_L1_L2` | `L1_STAIR_MAIN` | `L2_STAIR_MAIN` | stair well |
| `P_STAIR_L2_L3` | `L2_STAIR_ATTIC` | `L3_STAIR_HEAD` | stair well, with a **door at the foot** (`DOOR_L2_ATTIC`) |
| `P_L0_STAIR__B1_STAIR` | `L0_STAIR_MAIN` | `B1_STAIR` | stair well; basement door is `P_B1_STAIR__B1_HALL` |
| `P_L1_STAIR__L1_LANDING` | `L1_STAIR_MAIN` | `L1_LANDING` | open |
| `P_L2_STAIR__L2_LANDING` | `L2_STAIR_MAIN` | `L2_LANDING` | open |

The **stair well** is what lets the visibility system see three floors at once when you stand in
the foyer and look up — which is correct and desirable — while the attic door and the basement
door cut those two levels off completely when closed. This is exactly the behaviour the brief
asks for, and it falls out of the data, not out of special cases.

### 16.3 Graph shape

The design figures below were written before the layout existed. `HOUSE-00374` and `HOUSE-00375`
authored 112 of the portals, `HOUSE-00376` the 66 windows and `HOUSE-00377` the last two, so every
row below is now `tools/world/report_graph.py assets-src/world`'s output rather than an estimate.
Where a design figure differed it is kept beside the measurement, because the difference is the
interesting part.

| Metric | Value |
|---|---|
| Cells | 96 — 75 interior rooms + 3 nested sub-cells (§54) + 18 exterior; the design said 95 |
| Portals total | **177** — 46 + 63 + 3 + 64 + 1; the design said 186. 179 until `HOUSE-00491` retired the two `W_GABLE` louvres, which were 1.44 m inside solid roof |
| — always-open (cased/stair) | **46** — 26 interior cased openings, 17 exterior, 3 stair wells |
| — hinged/double/slider/exterior doors | **63** |
| — nested portals (fridge door, freezer lid, garage loft hatch) | **3** |
| — windows | **64** — see §12.6; the design said 81 and §12.6 accounts for the 17 |
| — garage door | **1** |
| Mean interior cell degree | **2.27** — through doors and openings only, because a window is not a way through; the visibility graph, which windows do join, is a different number |
| Max interior cell degree | `B1_HALL` = **7** — **measured**; the design said `L0_KITCHEN` = 8 |
| Graph diameter (through open doors) | **13 hops** (`EXT_SHED` → `L2_BATH5`) — **measured**; the design said 11 (`EXT_ROAD` → `L3_STORE_N`), which is what the graph measured before the garden shed had a door |
| Diameter with all doors closed | ∞ — **59 components**, which is the whole point; the design said 34 |
| Largest component with all doors closed | **19 cells** — the exterior ring and what cased openings join to it, and since `HOUSE-00484` also `B1_STAIR`: the basement's floor opening is the stairwell it always was rather than a door, so it no longer closes. The basement's door moved to the bottom landing, where shutting it severs `B1_HALL` instead |

### 16.4 Cell-membership lookup

A point→cell lookup runs on every physics step and every camera move. Implementation:

1. **Incremental**: the controller keeps `currentCell`. Each step, test the point against
   `currentCell`'s boxes (expanded by 5 cm hysteresis). Hit → done, O(1), the case 99.9 % of the
   time.
2. **Neighbour walk**: else test the cells reachable through `currentCell`'s portals. Hit → done.
3. **Grid fallback**: else look up a **uniform 2 m × 2 m × level grid** built at load time mapping
   each bucket to the ≤ 6 cells overlapping it, and test those. This is also the entry point for
   spawning, teleporting and save-loading.
4. **Failure**: no cell → the player is outside the house shell; assign `EXT_WORLD`. If the point
   is inside the shell but in no cell, that is a **world-data bug** and the validator (§15.7 rule
   3) is supposed to have caught it; at runtime it raises a diagnostic and clamps to the last good
   cell.

---

## 17. Scene architecture

### 17.1 Shape of the code

Not an ECS. An ECS pays for itself when there are many thousands of homogeneous entities with
volatile component sets. This world has ~95 cells, ~2 400 static prop placements, ~640
interactables, 3 characters and a handful of systems. What it needs is **clear ownership,
deterministic lifetime and data-driven placement** — which plain composition gives with far less
machinery.

```
CnaHouseGame : Microsoft::Xna::Framework::Game
 ├── Services                (a small typed service locator, constructed in a fixed order)
 ├── World
 │    ├── WorldData          (immutable, loaded once: levels, cells, portals, lights, materials)
 │    ├── CellRuntime[]      (per-cell mutable: visibility flags, residency, dynamic lists)
 │    ├── PortalRuntime[]    (per-portal mutable: apertureFraction, cached rect)
 │    └── SpatialIndex       (the 2 m grid of §16.4)
 ├── Systems (fixed update order, each owns its own state, none owns another)
 │    ├── TimeSystem         simulated clock, sun/moon vectors, season
 │    ├── WeatherSystem      state vector, transitions, RNG
 │    ├── SkySystem          dome colours, clouds, stars, moon phase texture
 │    ├── LightingSystem     per-room artificial/daylight levels, dominant lights
 │    ├── PhysicsSystem      static colliders, capsule sweeps, ground probes
 │    ├── PlayerSystem       controller, cameras, walk mode, avatar state
 │    ├── AnimationSystem    ClipPlayer instances, bone palettes
 │    ├── PetSystem          dog + cat behaviour and navigation
 │    ├── InteractionSystem  targeting, prompts, action dispatch
 │    ├── AudioSystem        listener, emitters, portal-path gains, voice manager
 │    ├── VisibilitySystem   camera cell, portal traversal, visible set
 │    ├── ResidencySystem    asset residency and (later) streaming
 │    ├── PersistenceSystem  save/load/reset
 │    └── UiSystem           HUD, prompts, menus, debug overlays
 ├── Renderer
 │    ├── RenderList         built from VisibilitySystem, sorted by pass/effect/material
 │    ├── EffectCache        stock effect instances + Tier-E Effect clones
 │    ├── ShadowPass, SkyPass, OpaquePass, AlphaTestPass, TransparentPass, GlarePass,
 │    │   CompositePass, HudPass
 │    └── DebugDraw
 └── Content
      ├── ContentRegistry    content name ↔ asset id ↔ residency pack
      ├── ModelCache, TextureCache, SoundCache, EffectCache
      └── SaveStore          ISaveStore: desktop (StorageDevice) / web (IsolatedStorage)
```

For Tier S, `MaterialBinder` is the effect pool: one lazily constructed instance of each of the
four primary stock-effect classes plus the supplemental `EnvironmentMapEffect`. It rewrites that
instance immediately before each draw and never clones by material. A clone per material variant
would retain the same values already written per draw while multiplying effect objects by the
material count; `HOUSE-00162` measured and rejected that ownership model.

### 17.2 Ownership and lifetime

* `Game` owns `Services`; `Services` owns every system by `std::unique_ptr`, constructed in
  dependency order and destroyed in reverse.
* Systems hold **non-owning references** to the systems they read, resolved once at construction.
  There are no cyclic dependencies; where two systems would need each other, an explicit
  **event queue** (`std::vector<Event>` drained once per frame, in a fixed order) breaks the
  cycle. Events: `DoorStateChanged`, `LightSwitched`, `WeatherChanged`, `CellEntered`,
  `InteractionPerformed`, `FixtureUsed`, `SaveRequested`.
* GPU resources (`Model`, `Texture2D`, `Effect`, `VertexBuffer`, `RenderTarget2D`) are owned by
  the caches as `std::shared_ptr`/`std::unique_ptr` and released in `UnloadContent()`.
* **No global mutable state.** The only globals are `constexpr` tables.
* **No raw owning pointers.** `T*` in a signature always means "borrowed, non-null, outlives the
  call" and is documented as such.

### 17.3 Entity model

Three kinds of thing live in a cell:

| Kind | Representation | Count | Update cost |
|---|---|---|---|
| **Static renderable** | An index into the cell's pre-batched chunk list. No per-frame object at all. | ~2 400 placements → ~380 chunks | zero |
| **Dynamic renderable** | `struct DynamicInstance { AssetId model; Matrix world; BoundingSphere bounds; MaterialOverrideId mat; uint16 cell; uint8 lodBias; uint8 flags; }` — 96 bytes, stored in a per-cell `std::vector` | ~900 (doors, drawers, taps, the car, movable props) | only when their state changes |
| **Character** | `PlayerAvatar`, `Pet` — model + `ClipPlayer` + capsule + bone palette | 3 | every frame |

An `Interactable` is *not* a renderable. It is a separate record referencing a dynamic instance
(or a static chunk sub-range) by id. That separation is what lets 640 interactables exist without
640 classes: there are **12 behaviour types** (§50.4) and 640 data rows.

### 17.4 Static chunk batching

At content-build time, every cell's static props are grouped by
`(effectClass, material, lightGroupSet, alphaMode)` and merged into one `VertexBuffer` +
`IndexBuffer` pair per group, with a per-group `BoundingBox`, plus a per-*sub-range* bounding box
so a big group can still be partially culled. Target: **≤ 6 chunks per cell**, ≤ 65 535 vertices
per chunk (16-bit indices where possible; 32-bit is available on EasyGL if needed).

Result: drawing a fully visible kitchen with 140 props costs 5 draw calls, not 140.

Props that must move (a drawer, a door, a chair the player can nudge) are excluded from batching
and become dynamic instances.

**The shell is chunked the same way, and it is not props.** The paragraph above was written about
furniture; a cell's own floor, ceiling, walls and trim are drawn too, and `HOUSE-00473` measured
them. The blockout, with no prop placed yet: **418 chunks over 86 cells — 3 to 7 per cell — 63 623
vertices, 2.25 MB.** The largest single chunk is under 2 000 vertices, ~3 % of the 16-bit index
cap, so the second criterion has an enormous margin and the first has none:

* Several cells are at **7 or more**. Each carries the four receiver classes (floor, ceiling, wall,
  exterior), plus glass, plus trim, plus a stair, a roof underside or a structure class. None of
  those is an artefact of the blockout's one-material-per-class placeholders: a real room's floor,
  ceiling and walls are genuinely different materials, and glass must be its own chunk because it
  is blended and drawn after the opaque pass.

**≤ 6 chunks per cell is a TARGET, and exceeding it is not automatically an architectural error**
(`HOUSE-00487`, owner decision 2026-09-10). The two ways to force every cell under it were both
refused: merging semantically useful blockout classes, and redefining a garage as something other
than a room. **Room semantics and render-chunk partitioning are separate concepts**, and changing
the architecture to satisfy a rendering number gets the dependency backwards.

Instead a cell may declare an **explicit exception**, and the model is what keeps that from
becoming unlimited fragmentation:

| Rule | |
|---|---|
| **≤ 6 is still the target** | a cell that can meet it should |
| **An exception states a reason and a ceiling** | not "this cell is exempt" — *this cell may have 7, because…* |
| **Over the target and undeclared fails the build** | which is what catches an accident |
| **Over its own ceiling fails** | an exception is a ceiling, not a licence |
| **An exception its cell no longer needs fails** | `L3_STORE_E` left the list the day `HOUSE-00491` retired its impossible louvre and took its `glass` chunk with it |
| **The report prints every exception in force** | with its count, its ceiling and its reason, so they are visible rather than merely tolerated |

The exceptions are declared in `tools/world/build_chunks.py`'s `CHUNK_BUDGET_EXCEPTIONS`, which is
where the partitioning is done and the only place that can measure them. Measured 2026-09-10:

| Cell | Chunks | Why |
|---|---|---|
| `EXT_ROAD` | 13 | not a room: the residency key for the property's outdoors — the carriageway's six ground and marking materials, the fence and gate on it, and `HOUSE-00494`'s two roofs and chimney |
| `L0_GARAGE` | 7 | four receiver classes, the stair to the loft, glazing, trim |
| `L0_STAIR_MAIN`, `L1_STAIR_MAIN` | 7 | a stair hall: the four, a `stair` class, glazing, trim |
| `L3_ROOM` | 8 | a rafter-bounded attic room draws the **roof** it looks up at as well as its collar ceiling (`HOUSE-00496`), and the **rafters** under that roof (`HOUSE-00488`) — two classes no room below it has, and both of them things you are looking at when you stand in it |
| `L3_STORE_N`, `L3_STORE_W` | 7 | attic stores: roof and structure instead of a ceiling, plus dormer glass |

`HOUSE-00926` (2026-09-15) separates weather-facing window frames from each room's indoor
skirting, so those frames can enter §25.6 without revealing ordinary trim through a closed
portal. The canonical source chunk report now measures 36 exception cells: 28 additional rooms
at exactly seven chunks, `L0_KITCHEN` at eight because its indoor borrowed-light glass also
remains, the six pre-existing interior exceptions each grow by exactly one, and the outdoor
`EXT_ROAD` exception is unchanged. These are **explicit cell-specific ceilings** in
`CHUNK_BUDGET_EXCEPTIONS`, not a new unlimited exception or a changed six-chunk target; the
existing over-ceiling and stale-entry gates still apply.

`HOUSE-01038` (2026-09-15) measures the first furnished foyer at **nine** static material
chunks: its previous six shell-finish groups and one weather-facing window-frame group,
plus two source-specific CC0 furniture albedos. Its cell-specific exception ceiling moves
from seven to exactly nine; the six-chunk target, Reach per-draw limit and culling roles
do not change. The two extra groups are visible close-range carved wood and upholstered
fabric, not permission to fragment other rooms.

`HOUSE-01045` (2026-09-16) gives `L0_LIVING` its specified upright piano on the short east-wall
run beside the foyer doors. The 1.485 × 1.240 × 0.733 m project-authored model has separately
modelled 52/36 keys, casework, music desk, legs and pedals; its four stock-`BasicEffect` finish
roles move that cell's base material ceiling from sixteen to twenty chunks. The existing measured
front-shutter group raises the final generated exception to **21 chunks / 20 materials**. The
collision proxy and placed bounds remain clear of the double-door portal and the room's main
circulation route. The deliberately unculled whole-house diagnostic is now 603 draws / 93 state
changes, inside §71.2's 1,400 / 210 worst-case envelope; named visible poses continue to protect
the 620 / 90 typical row.

`HOUSE-01066` (2026-09-18) refines that same fixed placement without widening its 1.485 × 1.240 ×
0.733 m bounds, support origin or collision/circulation envelope. The 18,852-triangle visible
model retains all 52/36 independent keys and adds a charcoal lacquer role distinct from the warm
walnut inset panels, finer bevelled joinery, physical hinges and maker plaque, front casters and
an open project-authored abstract practice folio. The folio copies no score, brand or likeness.
Its five approved stock-`BasicEffect` roles make the measured `L0_LIVING` base exception **34**;
the already measured shutter and double-door roles make the final cell **38 chunks**. The complete
unculled house is 638 opaque + 44 alpha-tested submissions and 107 opaque state changes, still
well inside §71.2's 1,400 / 210 envelope.

`HOUSE-00945` (2026-09-18) measures the east driveway-border landscape cell at **nine chunks**:
its existing concrete, grass, fence and street-tree roles plus one shared shrub atlas, mulch and
the shared bronze/emissive roles of three low bollards. Five shrubs and three fixtures remain
separately bounded exterior-hierarchy sub-ranges, not eight new draws. The exact cell-specific
ceiling records those visible roles while retaining the ordinary six-chunk target and both the
over-ceiling and stale-exception failures. The front 16 m terrain tile owns the part of the same
mulch strip nearest the road, so its one shared mulch chunk is correctly resident in `EXT_ROAD`
and moves that exterior aggregation's exact ceiling from seventeen to eighteen.

`HOUSE-00946` (2026-09-18) measures `EXT_SIDEYARD_E` at **eleven chunks** after the driveway
adds one shared asphalt-finish control-joint role and one shared bluestone-inlay role. Twelve
authored strips compile into those two batches across both terrain tiles; they do not become
twelve draws. Both canonical ids already existed, so the unculled house moves from 647 to 649
opaque submissions while staying at 107 opaque state changes.

`HOUSE-00947` (2026-09-18) measures `L0_GARAGE` at exactly **eleven chunks**. Four top-row lites
join its existing exterior-window glass batch, while the physical centre pull adds one separately
exterior-resident bronze-hardware role. The complete world is 695 chunks / 273 exterior hierarchy
instances / 57.695654 MB: 650 opaque submissions plus 45 cutouts at 107 opaque state changes. The
garage aperture, collision, portal, navigation and receiver-lightmap geometry are unchanged.

`HOUSE-00948` (2026-09-18) keeps `L0_GARAGE` at exactly **eleven chunks**. Its eight closed
surround pieces add 96 detail triangles to the existing white exterior-frame batch rather than a
new role; the complete world remains 695 chunks / 273 exterior hierarchy instances and grows only
to 57.702063 MB. The exterior hierarchy bounds include the new 55–95 mm projections, while the
garage aperture, collision, portal, navigation and receiver-lightmap geometry remain unchanged.

`HOUSE-01067` (2026-09-19) measures `L0_FAMILY` at exactly **26 chunks**. Its two picture-window
treatments share one new woven-fabric role and the room's existing steel role, so 12,168 visible
triangles add only one opaque draw/state. The complete world is 696 chunks / 94 props / 273
exterior hierarchy instances / 58.178427 MB: 651 opaque submissions plus 45 cutouts at 108 opaque
state changes. Both props are collision-free static dressing; no aperture, portal, glass,
lightmap, light, collision or navigation geometry changes.

`HOUSE-01068` (2026-09-19) measures `L0_SUNROOM` at exactly **20 chunks / 19 materials**: seven
shell/window roles, five breakfast-group roles, two additional wet-bar roles, two plant roles and
three shared physical-fixture roles, with one exact Reach primitive-cap split. The complete world
is 720 chunks / 116 props / 284 exterior hierarchy instances / 59.826849 MB. Both furniture GLBs
retain separately bounded named collision proxies, while the plants and fixture bodies remain
collision-free; collision is 1,654 shapes and the rebuilt pet graph is 893 nodes / 3,954 edges.

`HOUSE-00949` (2026-09-19) measures `L0_SUNROOM` at exactly **21 chunks / 20 materials** and
`L1_MASTER_BED` at exactly **eight chunks**. Each now has one additional aluminium-frame role;
the two broad clear panes join the existing exterior-glass batch in their respective cells. The
complete world is 722 chunks / 116 props / 286 exterior hierarchy instances / 59.850882 MB.
The selective UV2 unwrap retains all 350 receiver faces across these two cells; slider details
have no lightmap-receiver surfaces. Collision, nav, apertures and portal graph retain their
canonical authored inputs rather than acquiring a transparent collision surrogate.

`HOUSE-00950` (2026-09-19) gives only `L0_SUNROOM` a measured limestone-look ceramic floor.
The approved ambientCG Tiles139 (`tile_grey_square`) albedo/normal is reused by a distinct
lightmapped `MAT_SUNROOM_LIMESTONE_TILE` row, so the 4 × 4 source tile repeat at 0.45 UV repeats
per metre makes each module 0.556 m wide in the world-metre shell. The cold continuous marble
remains in the foyer, cellar and master bath, where it is already authored. Receiver UV2,
daylight/fixture atlases, static geometry, portals, collision and the stock-XNA effect contract
are unchanged; the canonical material census becomes 206, with `L0_SUNROOM` still 21 chunks /
20 used materials. This is a surface/material correction, not a claim that the room's daylight
depth or the near-black night garden is finished.

`HOUSE-00930` (2026-09-16) isolates each weather-facing `D_ENTRY` leaf from ordinary hardwood
joinery so the exterior hierarchy can retain it when its owning interior room is portal-culled.
`HOUSE-00932` gives that generated leaf four-panel millwork on both faces, paired bronze
handle/lock sets and a capped threshold. A darker sibling of the approved hardwood finish lets
the relief read beneath the covered porch without painting it; that role and the stable
weather-facing hardware role each add one Basic-effect chunk. The measured result is `L0_FOYER`
at exactly **twelve** and `L1_LANDING` exactly **ten**. The ordinary six-chunk target
remains; both cell-specific ceilings name these roles and retain the over-ceiling/stale-entry
gates.

§71's frame budget is what may tighten or restructure these later; a measurement is what should
move them, not an assertion in either direction.
* **The outdoors is chunked too, since `HOUSE-00780`.** The terrain tiles, road segments, fences,
  gates and garden structures are generated one file per thing and named for the thing, so each is
  filed in the exterior cell it covers most of — the cell being a **residency** key (§27.2's pack)
  and not a visibility one, because §25.6 culls the outdoors with a BVH over instances. Measured:
  61 files, 466 chunks over 93 cells, 30 chunks in the `exterior` pack and 9 in `neighbourhood`.
  `EXT_ROAD` and `EXT_WORLD` join the four cells over the six-chunk target, at 8 and 9.
* Ten cells and one non-cell draw **nothing**: a yard is ground and sky. `HOUSE-00475` found every
  exterior cell building walls at its own `yOverride` height — 20 m round each yard, 65 m round
  `EXT_WORLD`'s 400 m square — so the first frame the blockout drew was the inside of that box.
  The wall a yard abuts belongs to the house, which draws its own outer skin.
  That skin keeps the adjacent room as its residency key, but its siding and brick water-table
  chunks also enter §25.6's exterior hierarchy: a closed portal must not make the façade disappear
  from the yard (`HOUSE-00921`). `HOUSE-00926` gives weather-facing window frames/sashes and
  glazing distinct material/chunk roles in the same hierarchy. `HOUSE-00930` does the same only
  for explicitly authored `MAT_EXTERIOR_DOOR_*` entry leaves: an exterior cell intentionally
  generates no wall/leaf copy, while the remaining room-owned copy would otherwise disappear
  behind its closed portal. Ordinary indoor skirting, door trim, borrowed-light glass and interior
  walls remain exclusively on the room/portal path; adding those whole chunks to the outdoors
  would leak interior geometry through closed rooms.
  `HOUSE-00931` replaces the elevated balcony's old 550 mm solid visual parapet with a measured
  open balustrade: 80 mm lower rails, 45 mm painted balusters with no clear gap over 95 mm,
  120 mm end newels and the existing narrow brushed-metal top rail centred at the authored 1.10 m
  height. All remain owned by their exterior balcony cell and batch into the same painted-trim and
  metal chunks. The independent 200 mm-wide, full-height collision guard stays deliberately
  continuous so a swept capsule cannot find a fall-through gap between visual members; this is an
  offline conservative proxy, not visible geometry or a portal/BVH exception. The source-to-chunk
  material check and deterministic shell manifest record the split.
  `HOUSE-00932` keeps each `D_ENTRY` leaf's canonical box and adds generated millwork/hardware on
  both faces: four raised panel outlines, an opposite-hinge lock stile with a 0.98 m lever, a
  separate deadbolt and a thin capped threshold. The relief uses the leaf's approved broad-board
  map with a darker hardwood tint; it and `MAT_EXTERIOR_DOOR_HARDWARE_BRONZE` join §25.6 through
  the existing `MAT_EXTERIOR_DOOR_*` contract, so ordinary room wood and metal remain portal-owned.
  `HOUSE-00940` interprets a `D_DOUBLE` row's 860 mm width as the one leaf the schedule says it is,
  and builds two leaves inside the unchanged 1.80 m portal with an 8 mm meeting clearance and a
  shallow astragal. Opaque pairs use restrained two-panel millwork; the one portal already marked
  `translucent` uses two framed panes per leaf instead of an opaque body. Both forms use physical
  lever hardware and the existing room trim/glass/metal roles; opaque moulding uses
  `MAT_DOOR_PANEL_HARDWOOD`, a BasicEffect reuse of the approved broad-board maps whose darker
  stain keeps the real 55 mm relief readable at room scale. This is the static closed-shell
  representation only: portal ownership, collision and the later two-leaf animation contract
  stay data-driven and unchanged.
  `HOUSE-00942` extends that static-shell grammar only where an opening row explicitly pairs
  `joineryStyle: four_panel` with a `hardwareMaterial`. Five close L0 route leaves currently opt
  in. Each keeps its unchanged painted slab and receives four two-column/two-row shallow moulding
  outlines on both faces plus a physical backplate and lever on the lock stile opposite its
  authored hinge. The moulding reuses the cell's painted door role; the hardware uses the authored
  approved steel role. Unselected leaves remain plain, no room id selects geometry, and portal,
  aperture, collision, lightmap-receiver and later animation semantics remain unchanged.
  `HOUSE-00949` corrects the static closed form of both `D_SLIDER` rows. Each opening explicitly
  names approved clear glazing and a non-solid aluminium leaf with no hinge or swing. The 2.36 ×
  2.10 m leaf envelope contains a 55 mm full-depth perimeter, two shallow 45 mm sash rings with
  a 45 mm meeting overlap and 24 mm centre-to-centre track separation, two 8 mm glass panes and
  320 mm pulls on both faces of the moving meeting stile. The generated shell never emits the
  generic opaque leaf for this type. The aluminium frame is a weather-facing fenestration role
  in §25.6's exterior hierarchy, while surrounding walls and room trim remain portal-owned.
  This is still a static closed-shell representation: the portal's glass opacity, collision/nav
  opening contract and phase-15 motion/state remain separately data-driven and unchanged.

### 17.5 Directory layout

```
cna-house/
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/
│   └── TierSelection.cmake          ← the ONLY place the Tier-E decision is made (§7.3)
├── cna-house.md                     ← this document
├── plan.md                          ← the master task list
├── README.md
├── LICENSE
├── NOTICE.md
├── docs/
│   ├── xna-deviations.md            ← the Tier-P project-owned register (§4.3)
│   ├── anim-format.md               ← the project-owned `.chanim` format (§47.0)
│   ├── collision-format.md          ← the project-owned `collision.bin` format (§49.2)
│   ├── nav-format.md                ← the project-owned `nav.bin` pet graph (§60.3)
│   ├── coverage-format.md           ← the project-owned `coverage.bin` roof mask (§37.2)
│   ├── chunk-format.md              ← the project-owned `chunks.bin` static batches (§17.4)
│   ├── skyexposure-format.md        ← the project-owned `skyexposure.bin` (§64.6)
│   ├── snowshell-format.md          ← the project-owned `snowshell.bin` (§38)
│   ├── shading-format.md            ← the project-owned `shading.bin` sun shading (§22)
│   ├── sunpatch-format.md           ← the project-owned `sunpatch.bin` decals (§29.1)
│   ├── cubemap-format.md            ← the baked mirror cube maps (§59)
│   ├── content-authoring.md
│   ├── world-format.md
│   ├── performance-log.md
│   └── decisions/ADR-0001..NNNN.md
├── include/cnahouse/                ← public headers, mirroring src/
├── src/
│   ├── app/          CnaHouseGame, Services, Settings, Bootstrap, CommandLine
│   ├── world/        WorldData, WorldLoader, CellRuntime, PortalRuntime, SpatialIndex, Validator
│   ├── visibility/   VisibilitySystem, PortalTraversal, FrustumClip, RenderList
│   ├── rendering/    Renderer, passes, EffectCache, MaterialBinder, DebugDraw, Impostors
│   ├── content/      ContentRegistry, caches, ResidencySystem, packs
│   ├── physics/      PhysicsSystem, Capsule, SweepTests, GroundProbe, CollisionBuild
│   ├── player/       PlayerController, FirstPersonCamera, ThirdPersonCamera, WalkModes, Avatar
│   ├── animation/    Skeleton, Clip, ClipLibrary, ChanimReader, ClipPlayer, BonePalette,
│   │                  FootIk, AnimationSet, StairBlend
│   ├── animals/      Pet, DogBrain, CatBrain, NavGraph, PetAnimation
│   ├── interaction/  InteractionSystem, Targeting, Prompt, behaviours/{Door,Window,Light,
│   │                 Container,Faucet,Toilet,Television,GarageDoor,Appliance,Pickup,Seat,Gate}
│   ├── environment/  TimeSystem, SunModel, MoonModel, StarField, SkySystem, CloudLayers
│   ├── weather/      WeatherSystem, Archetypes, Transitions, Precipitation, Lightning, Wind,
│   │                 Wetness, SnowCover
│   ├── lighting/     LightingSystem, RoomLightState, DaylightModel, LightGroups, BlobShadows
│   ├── audio/        AudioSystem, EmitterPool, VoiceManager, PortalPath, Footsteps, Ambience
│   ├── persistence/  SaveStore, SaveModel, Serializer, Migrations, ResetHouse
│   ├── ui/           Hud, PromptView, MenuStack, SettingsView, Confirm, TouchHud (later)
│   ├── debug/        Overlays, Counters, FrameGraph, Cheats, Screenshot
│   └── util/         Ids, Json, Log, Result, FixedString, SmallVector, Rng
├── tools/
│   ├── world/        validate_world.py, build_collision.py, build_nav.py, report_graph.py
│   ├── assets/       fetch_asset.py, manifest.py, verify_licences.py, convert_audio.py,
│   │                 gltf_validate.py, scale_check.py
│   ├── blender/      lod_gen.py, lightmap_unwrap.py, lightmap_bake.py, impostor_render.py,
│   │                 collision_proxy.py, house_shell_gen.py, prop_kit_gen.py
│   ├── effects/      build_effects.sh (wine + fxc through cna-content)
│   └── ci/           check_xna_only.py, check_manifest.py, budget_report.py
├── assets-src/       source glTF, PNG, WAV, TTF, FX, world JSON — the authored truth
├── content/          generated. gitignored except for a small committed baseline
├── licenses/         upstream licence texts + THIRD-PARTY-ASSETS.md (generated)
└── tests/
    ├── unit/         gtest, headless, no GPU
    ├── integration/  gtest + HEADLESS renderer, full Game update loop
    ├── render/       gtest + OPENGLES3 under Xvfb, pixel/screenshot regression
    └── perf/         budget assertions and the measurement harness
```

---

## 18. Content pipeline

### 18.1 The two-stage build

```
assets-src/                                        content/
  Models/**.glb        ─┐                     ┌─→  Models/**.cnb        → Model
  Textures/**.png      ─┤                     ├─→  Textures/**.cnb      → Texture2D
  Audio/**.wav (16-bit)─┼→ cna-content build ─┼─→  Audio/**.cnb         → SoundEffect
  Fonts/*.spritefont   ─┘   (--format cnb)    └─→  Fonts/*.cnb          → SpriteFont

  Media/Video/*.ogv    ──→ cna-content build ───→  Video/*.cnb + stream → Video
                           (into content/ ITSELF, not content/Media -- see below)

  Effects/*.fx         ──→ cna-content build ───→  Effects/*.xnb        → Effect
                           --format xnb
                           --fx-compiler <fxc.exe>
                           --fx-compiler-launcher wine

  world/*.json         ──→ validate + copy    ───→ world/*.json         → read directly

  Models/**.glb        ──→ anim_extract.py    ───→ Anim/*.chanim        → cnahouse::anim::ClipLibrary
   (the skinned ones)                                                     (§47.0, TitleContainer)
```

`ContentManager::Load<T>()` tries `.xnb` first, then a literal path, then `.cnb`
(`docs/xnb-content-pipeline-support.md`), so the two output formats coexist in one `content/` tree
with no ambiguity: only effects are `.xnb`. `Anim/*.chanim` and `world/*.json` are not
`ContentManager` assets at all: they are project-owned files opened with `TitleContainer::OpenStream`
and parsed by our own reader, which is why neither depends on a CNA content type.

The shell resolves both `ContentManager` roots from XNA's `TitleLocation` before starting normal
gameplay. `TitleContainer` already resolves `world/` from that executable-relative base, whereas
a *relative* `ContentManager::RootDirectory` follows the process working directory in CNA. Without
the shared title base, launching `build/cna-house` from the repository root silently paired fresh
`build/content/world` with stale root-level `content/Textures`/`content/Models`; the first furnished
foyer and hall lightmaps then appeared nearly black despite passing isolated GPU/content tests.
Tests may inject fixture roots through `Options`, but the shipping shell always uses the title base.

CMake wiring:

```cmake
cna_add_content(TARGET cnahouse_content
                SOURCE_DIR assets-src            # Models/ Textures/ Audio/ Fonts/ Video/;
                                                 # Effects/ and world/ are excluded by the config
                OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/content
                CONFIG_FILE assets-src/.cna-content.json
                WORKERS 8)
cna_add_content(TARGET cnahouse_effects   # only when CNAHOUSE_TIER_E
                SOURCE_DIR assets-src/Effects
                OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/content/Effects
                CONFIG_FILE assets-src/Effects/.cna-content.json)
```

`.cna-content.json` sets per-asset processor parameters: `generateChildAssets` for multi-group
glTF, `profile: hidef` for effects, texture format and mip options.

**`Media/` is the one tree built into the content ROOT, and the reason is measured**
(`HOUSE-00201`, 2026-09-07). A `Video` or a `Song` compiles to a metadata `.cnb` *plus a
byte-identical copy of the media file*, and the runtime resolves that copy through
`ContentManager::BuildAssetPath` — relative to the **content root**, not to the `.cnb` beside it.
Everything else resolves relative to the directory it was built into. Building the media tree into
`content/Video` therefore deploys the stream where the runtime does not look, whichever
`streamReference` is configured; building it into `content/` and putting the `Video/` component in
the *source* layout makes the two the same path, keeps the content name `Video/<name>` that §58
uses, and copies nothing twice. `assets-src/README.md` has the two-row table of what each
alternative does. `VideoProcessor` also **requires** `width`, `height` and `framesPerSecond` as
parameters: CNA does not decode the source at build time, so the metadata can disagree with the
file and only an offline check can catch it.

### 18.2 Authoring conventions

| Rule | Reason |
|---|---|
| Source models are **`.glb`** (binary glTF), one logical object per file | Single-file provenance, hashable, no missing sidecar textures |
| Y-up, metres, +Z is the object's "front"; the origin is at the object's **support point** (floor contact) unless the asset is wall-mounted, where it is at the wall plane | Placement in the layout is then a plain position + yaw |
| Textures embedded in the `.glb` for props; shared architectural materials referenced externally | Keeps the atlas of shared surfaces deduplicated |
| Base colour textures are sRGB PNG; normal/roughness are linear PNG | Matches the pipeline's colour-space contract |
| Texture sizes are powers of two, ≤ 2048 for hero props, ≤ 1024 for common props, ≤ 512 for small props, ≤ 256 for tiny props | Budget (§72) |
| Every model carries `LOD0` and, if its LOD0 triangle count > 4 000, `LOD1` and `LOD2` as extra glTF meshes named `<name>_LOD1`, `<name>_LOD2` | Handled by the LOD tool |
| Collision proxies are separate meshes named `<name>_COL`, convex or box-decomposed, ≤ 64 triangles | The build extracts them into the collision file and strips them from the runtime model |
| Skinned characters keep one skin and one skeleton per file; clips are separate glTF animations in the same file | The pipeline groups mesh placements by skin, one group = one `Model` (`docs/content-pipeline.md:453`) |
| Names use `snake_case`; content names use `Category/Sub/name` | |

### 18.3 The lightmap pipeline

1. `tools/blender/house_shell_gen.py` reads `layout.cells.json` and generates the architectural
   shell — floors, ceilings, walls, openings, stairs, roof — as a Blender scene, deterministically.
2. `tools/blender/shell_unwrap.py` gives every shell **lightmap receiver** a second UV channel,
   packed per cell into a texel-density-uniform atlas (**4 texels/metre** for rooms, 8 for small
   rooms, 2 for the attic and basement) by `lightmap_unwrap.py`.

   **A lightmap receiver is a surface class, not a triangle size.** The receivers are the major
   static room-scale surfaces — interior floors, interior ceilings, interior wall surfaces and the
   major exterior wall/outer-skin surfaces — where baked light carries meaningful low-frequency
   information. Architectural **detail** is deliberately not lightmapped: skirtings, cornices,
   architraves, thresholds, window frames and sashes, glass, stair nosings, handrails, balusters,
   rafters and attic framing, gutters, downspouts, and the thin trim of the roof, porch and
   balconies. It is lit by the room's or the exterior's dynamic term instead (§22.2), which is a
   quality and performance choice rather than a missing feature: a 55 mm board carries no
   low-frequency lighting information worth a texel, and giving it one costs atlas area that a
   wall would use better.

   Movable and stateful geometry — door and window leaves, cabinet fronts, anything an interactable
   turns — is never baked either: a bake is a photograph of one state, and it is wrong the moment
   the thing moves.

   Because the rule is the class and not the size, **triangulating a receiver differently must not
   change whether it is lit by a bake.** `house_shell_gen.py` welds its receiver faces so a wall
   broken into strips round a doorway is one connected surface, and the unwrapper packs one island
   for the wall rather than one per strip. A small triangle that belongs to a large receiver is
   carried by that receiver's island; it is never dropped on its own account.

   The classification is **generated data, not a naming convention**: every material
   `house_shell_gen.py` creates carries `surfaceClass` and `lightmapReceiver`, and the glTF
   exporter writes both into the material's `extras`, so the unwrap and the bake read the
   generator's own decision out of the file.

   > **Decided 2026-09-09 (`HOUSE-00471`), replacing "every shell face".** That wording was not
   > achievable at 4 texels/metre and the measurement is in §72. Measured after the change: 78
   > cells, 5 306 receiver faces over 6 935 m², 29 608 detail faces over 1 877 m² left to the
   > dynamic term, 967 islands in 78 atlases of 128² — **1.28 M texels, under a third of one** of
   > §72's 21. `tools/blender/shell_preview.py` renders the distinction into `docs/blockout/`.
   >
   > Re-measured after `HOUSE-00475` stopped the generator giving open cells walls: **4 940
   > receiver faces over 6 742 m², 27 480 detail faces over 1 755 m², 936 islands, the same 78
   > atlases and the same 1.28 M texels.** The conclusion is unchanged; the numbers moved because
   > the 78 baked cells lost 2 494 faces of yard wall that were never part of any room.
3. `tools/blender/lightmap_bake.py` bakes, per cell, one lightmap per light group plus one
   "daylight" lightmap lit only by a uniform sky dome through that cell's window openings.
   Bakes are diffuse-only, indirect included, Cycles, 256 samples, denoised.
4. The atlases are exported as PNG (`_LM_<group>.png`) and compiled to `Texture2D` content.
5. `layout.cells.json` gains the generated lightmap names; a hash of the shell geometry is stored
   so a stale bake is detected and the build fails loudly rather than shipping wrong light.

Atlas budget: 21 lightmap atlases of 2048² (RGB, no alpha) ≈ 84 MB uncompressed / 21 MB DXT1.
Daylight atlases add the same again. See §72.

### 18.4 Determinism and reproducibility

* Every tool takes a `--seed` and defaults to a fixed one.
* Every generated file's header records the tool name, version, input hashes and seed.
* `content/` is **gitignored entirely**; nothing generated is committed. The one baseline that *is*
  committed lives **beside its source** — `assets-src/Effects/*.xnb`, next to the `.fx` that
  produced it, which is what BL-04 says and what `tools/effects/build_effects.sh` writes. *(Corrected
  2026-09-07 by `HOUSE-00185`: this line previously described the baseline as living inside
  `content/`, and the `.gitignore` carried three exceptions for files that never existed there.
  Beside the source is better anyway — a reviewer sees the `.fx` and the `.xnb` change together, and
  tracked files inside an otherwise-generated tree make an accidental `git add content/` far too
  easy.)*
* `assets-src/Effects/COMPILER.txt` records what produced those bytes: the `cna-content` hash, the
  `fxc` hash and size, the Wine version, and the source and output hashes.
  `tools/effects/build_effects.sh --check` verifies them and **needs no compiler**, which is the
  point — it is the check a contributor without Wine, and a CI job without Wine, can both run.
* A `make content-verify` target rebuilds everything and asserts the hashes match; drift is a
  build failure.

---

## 19. External asset strategy

### 19.1 Principles

1. **Legal certainty beats convenience.** An asset with an unclear licence is not used, however
   good it looks. `PROVENANCE UNKNOWN — DO NOT SHIP` is a real state in the manifest and the
   packaging step refuses to include such an asset.
2. **CC0 first.** Then CC-BY (with attribution generated automatically). Then a named
   permissive licence with a recorded redistribution clause. Never "free download".
3. **Reuse aggressively.** A house of 640 interactables and 2 400 placements must not need 2 400
   unique models. Target: **≈ 420 unique prop models**, each placed 1–14 times with per-instance
   material tints, scale jitter (±3 %) and yaw variation, so repetition does not read.
4. **Generate the boring things.** Walls, floors, ceilings, stairs, skirtings, trims, doors,
   window frames, shelving, plain boxes, fences, kerbs, paths, the terrain and the collision
   proxies are **procedurally generated from the layout** by our own Blender tools. This is where
   glTF generation is appropriate and safe.
5. **Buy quality where it shows.** Human, dog, cat, car, hero furniture, kitchen appliances and
   the bathroom fixtures must be genuinely good. These get individual research tasks and an
   explicit acceptance bar.

### 19.2 Candidate sources (to be verified at task time, not assumed)

| Source | Expected licence | Suitability |
|---|---|---|
| **Poly Haven** | CC0 | Textures/HDRIs (excellent), models (furniture, props, plants — very good). First stop. |
| **ambientCG** | CC0 | PBR material scans: wood, plaster, tile, brick, asphalt, gravel, fabric, roof shingle. First stop for materials. |
| **Khronos glTF-Sample-Assets** | mixed (mostly CC-BY / CC0 / public domain) | Reference/test assets and a few usable props; the pipeline conformance corpus |
| **Quaternius** | CC0 | Stylised — good for filler vegetation and neighbourhood LOD2 |
| **Kenney** | CC0 | Stylised — UI, small props, prototype kit |
| **Poly Pizza** | CC0 / CC-BY per asset | Broad prop coverage; per-asset licence check mandatory |
| **BlenderKit** (free tier) | per-asset; many CC0 | Furniture and appliances; per-asset check mandatory |
| **Sketchfab**, filtered to CC0 | CC0 | The most likely source for a realistic dog, cat and car; per-asset check mandatory |
| **Blend Swap**, filtered to CC0 | CC0 | Same |
| **MakeHuman / MPFB2** | tool AGPL; **generated meshes are CC0** by the project's asset licence | The human base meshes. Verified at task time. |
| **CMU Graphics Lab Motion Capture Database** | free for all uses | Locomotion clips, retargeted in Blender |
| **Mixamo** | free with account; **redistribution of source files restricted** | Usable for *derived* baked animation only if the licence permits; **flagged, not assumed** |
| **Freesound**, filtered to CC0 | CC0 | The audio gaps NOX does not cover (§63.4) |
| **Google Fonts / SIL OFL fonts** | OFL | HUD fonts |
| **Blender's own bundled assets** | CC0 | Occasional |

`HOUSE-00296` verified ambientCG and retained its fixed 34-material base set. Each upstream
1K-JPG archive is SHA-256 pinned; the prepared sources are albedo, OpenGL normal and packed ORM at
a power-of-two size with a maximum 256-pixel edge. Albedo and normal are runtime `core` textures.
ORM remains manifested offline input for §22.1's `pbr_to_stock.py` scalar mapping because the
application-owned runtime record has no ORM texture slot. The real uncompressed-CNB build measures
the complete `core` pack at **47.23 MB / 55 MB**; packaging ORM as an unused runtime texture would
instead make it 58.4 MB and is rejected. `HOUSE-00900` mapped all 34 sets to complete §22.1
`MAT_BASE_*` rows and reviewed each on a sphere, floor and wall at three light levels. Selection,
hashes, measurements, mapping decisions and renders are in
`docs/asset-selection/base-material-set.md`.

`HOUSE-00297` retained 22 specifically pinned Poly Haven CC0 vegetation sources and derived the
34-model set required below: six botanically distinct trees at three real geometry ages, nine
shrubs, five flowers and two grass-card sets. Prepared GLBs contain only stock-XNA-consumable base
colour/alpha at no more than 256 px; every tree age is grounded and scaled to its §70.5 band, LOD meshes
hit §26.1's ratios, and the locally rendered alpha-aware review is in
`docs/asset-selection/vegetation-set.md`. The source catalogue did not offer identified maple,
birch or fruit trees under the required terms, so no unrelated species was deceptively renamed;
`HOUSE-00772` owns visual role assignment and any focused exact-species gap it reveals.

`HOUSE-00772` places that set in the canonical exterior: **309 deterministic placements in 14
groups** (34 street trees, 3 property trees, 4 young orchard trees, 60 shrubs, 169 hedge shrubs,
18 flowers and 21 grass patches). Stable legacy role ids do not pretend their substitutes are exact
species: source and material records name Jacaranda, Searsia lucida and `tree_small_02`. Ten selected
visible roles use source-exact runtime atlases; the remainder use explicit category fallbacks from
the same pinned sources. Bark/branch groups use `BasicEffect`, cutout foliage/flowers/grass use
`AlphaTestEffect`, and all enter the normal static chunk/exterior-BVH path with per-placement
sub-ranges. Trunk and hedge proxies enter the canonical collision build. `HOUSE-00773` remains the
owner of grass-card jitter/wind, while later LOD work remains the owner of distance selection; the
placement task does not collapse those scopes into an unmaintainable special renderer.

### 19.3 Quality bar and rejection criteria

An asset is rejected if any of these is true:

* it fails glTF validation (`gltf-validator`, or CNA's own importer with warnings-as-errors);
* its bounding box, once scaled, is outside the realism tolerance for its category (§70.5);
* its normals are inverted, its winding is inconsistent, or it has non-manifold shells that show
  as black facets;
* its topology is obviously machine-generated slop: floating fragments, intersecting duplicated
  shells, a "melted" silhouette, a face with impossible anatomy;
* its texture is a photograph with baked-in lighting that fights the scene's own lighting;
* its triangle count exceeds the category budget by more than 3× and cannot be decimated cleanly;
* its licence is unclear.

Hero assets (human, dog, cat, car) additionally require a **visual sign-off task**: four
screenshots (front, side, three-quarter, close-up) at final scale in the actual scene lighting,
compared against a reference photograph, recorded in `docs/asset-review/`.

`HOUSE-00293` exhausted R-03's three sources and fixed the player car as a project-authored,
unbranded contemporary five-door family estate: **4.65 × 1.84 × 1.48 m**, with a simplified
visible cabin, LOD1/LOD2 and collision proxy. `HOUSE-01035` owns the source model and visual
sign-off, `HOUSE-01036` owns the pipeline proof, `HOUSE-00778` derives its snow shell, and
`HOUSE-00998` places it in the garage. It remains a static prop; the deferred drivable-car scope is
unchanged.

### 19.4 Category plan

| Category | Unique models | Approach |
|---|---|---|
| Architecture shell (walls, floors, ceilings, roof, stairs, trims) | generated | `house_shell_gen.py` from the layout |
| Doors and windows (7 door types, 10 window types) | generated + 4 sourced handles/hardware | Parametric, driven by `layout.openings.json` |
| Kitchen: fridge, range, ovens, dishwasher, hood, sink, taps, island, cabinets | 24 sourced + generated carcasses | Cabinet boxes generated, doors/fronts/handles sourced |
| Bathroom fixtures: WC ×3 styles, basin ×3, bath ×2, shower ×2, taps ×4, radiator ×2 | 16 sourced | Realism-critical; visible up close |
| Seating: sofas ×4, armchairs ×5, dining chairs ×4, office chairs ×2, stools ×3, benches ×2 | 20 sourced | |
| Tables and desks | 14 sourced | |
| Beds, mattresses, bedding, pillows | 12 sourced | |
| Storage: wardrobes, dressers, bookcases, shelving, cabinets | 22 sourced + generated shelving | |
| Appliances (small): kettle, toaster, microwave, coffee machine, washer, dryer, TV ×2, speakers, lamps ×9 | 26 sourced | |
| Decoration: rugs ×8, curtains ×5, mirrors ×4, clocks ×4, vases ×6, plants ×12, books (kit), frames ×6 | 55 sourced + kit | |
| Kitchenware and food props | 40 sourced/generated | Reused heavily in cupboards and the fridge |
| Boxes, crates, bins, tools, garage clutter, attic clutter | 45 sourced + generated | The generated ones are boxes, which is honest |
| Vegetation: 6 tree species × 3 ages, 9 shrubs, 5 flowers, 2 grass-card sets | 34 derived from 22 sourced | Plus impostors |
| Exterior: fence kit, gates, mailbox, bins, hose reel, AC unit, shed, garden furniture, fire pit, swing | 24 sourced + generated fence | |
| Neighbourhood houses | 8 sourced/generated bodies × material variants | Plus 3 LOD levels and impostors |
| Vehicles: the player's car, 3 parked neighbour cars, 1 delivery van | 1 project-authored + 4 sourced | The player's car is a hero asset |
| Characters: 2 base bodies, 6 hair, 5 clothing sets | 13 built from MakeHuman + Blender | Hero |
| Animals: dog, cat | 2 project-authored | Hero |
| **Total unique models** | **≈ 420** | |

---

## 20. Asset licensing strategy

### 20.1 Rules

* Every file under `assets-src/` has a manifest row. **No row, no build** —
  `tools/ci/check_manifest.py` fails the build on an unlisted file.
* Every manifest row carries the SHA-256 of the source file. Any change to a source file without a
  manifest update fails the build.
* Licence texts are copied verbatim into `licenses/<slug>/LICENCE.txt` alongside a
  `source.url.txt` and a `retrieved.txt` timestamp.
* `licenses/THIRD-PARTY-ASSETS.md` is **generated** from the manifest and shown in-game from the
  credits screen. Attribution is therefore impossible to forget.
* Redistribution is decided per asset, not per source. `redistributeSource` and
  `redistributeDerived` are separate booleans. A CC-BY-ND asset, for instance, would have
  `redistributeDerived = false` and could not be decimated for LODs — so it would not be used.
* Anything without provable provenance is marked `PROVENANCE UNKNOWN — DO NOT SHIP` and the
  packaging target refuses to include it. It may still sit in `assets-src/` as a placeholder
  during development, and a build with any such asset stamps a visible watermark on every frame.
* **A generated asset has no `assets-src/` row and is not exempt from the question.** The house
  shell is 99 `.glb` regenerated from the layout into `build/shell/`, so the row's two purposes —
  provenance and a licence — are served by `docs/shell-manifest.json` instead (`HOUSE-00482`),
  which records the form the question actually takes for something a tool makes: **which generator,
  at which version, over which layout, producing which bytes.** The version is a SHA-256 over the
  sources that decide the geometry rather than a number somebody has to remember to raise; the
  layout is `world.manifest.json`'s hash; the bytes are a full SHA-256 each.
  `house_shell_gen.py --check-manifest` is a gate, and it says so loudly on a checkout with no
  shell rather than passing green over nothing.

### 20.2 Network availability

Network access was **not** used during this planning pass, and no licence in §19.2 is asserted as
verified. Every source in that table is a *candidate* carrying an *expectation*. `plan.md`
phase 4 contains one research task per source and one per hero asset, each of which must record
the actual licence text, URL, author and retrieval date before any asset is used.

### 20.3 Manifest schema

```jsonc
{
  "schema": "cna-house/assets/1",
  "assets": [
    {
      "id": "MODEL_PROP_KITCHEN_FRIDGE_01",
      "category": "appliance",
      "sourceFile": "assets-src/Models/Kitchen/fridge_01.glb",
      "sourceSha256": "…",
      "processedFile": "content/Models/Kitchen/fridge_01.cnb",
      "processedSha256": "…",
      "origin": {
        "kind": "downloaded",              // downloaded | generated | authored | derived
        "name": "Refrigerator 01",
        "url": "https://…",
        "author": "…",
        "retrieved": "2026-09-20",
        "licence": "CC0-1.0",
        "licenceFile": "licenses/cc0-1.0/LICENCE.txt",
        "attribution": "…",                // empty for CC0
        "redistributeSource": true,
        "redistributeDerived": true,
        "commercialUse": true,
        "modification": true
      },
      "geometry": {
        "unitScale": 1.0, "upAxis": "Y", "frontAxis": "+Z",
        "boundsMetres": [0.90, 1.85, 0.72],
        "triangles": { "LOD0": 9840, "LOD1": 3100, "LOD2": 940 },
        "collision": "fridge_01_COL",
        "animations": ["door_open"],
        "materials": ["MAT_APPLIANCE_STEEL", "MAT_PLASTIC_WHITE", "MAT_GLASS_CLEAR"]
      },
      "usedIn": ["L0_KITCHEN"],
      "residencyPack": "house-l0",
      "review": { "status": "approved", "screenshots": ["docs/asset-review/fridge_01/*.png"] }
    }
  ]
}
```

### 20.4 Family photographs and artwork

The house has a photo gallery wall in `L0_HALL`, framed pictures in most bedrooms and artwork
throughout. Using identifiable real private people is not acceptable. Three permitted routes,
in order of preference:

1. **Non-identifiable photography** — landscapes, architecture, still life, abstract art, from
   CC0 sources. Fine for most frames.
2. **Deliberately abstracted "family" imagery** — silhouettes, back-of-head shots, distant figures,
   heavily painterly filters. Reads as family photography without depicting anyone.
3. **Rendered fictional family** — a small set of images rendered in Blender from the same
   MakeHuman-derived characters the game itself uses, posed in the house's own rooms. This is the
   most coherent option: the photos on the wall are of the people who live here, and we own them
   outright.

Route 3 is the recommendation for the ~9 gallery-wall frames; routes 1–2 fill the rest. **No
AI-generated photographic likenesses of people are used.**

---

## 21. glTF conversion strategy

### 21.1 The route, end to end

```
  .glb (glTF 2.0, metres, Y-up, CCW)
      │  gltf_validate.py  → gltf-validator + CNA importer with warnings-as-errors
      │  scale_check.py    → category bounds, origin at support point, front axis
      │  lod_gen.py        → LOD1/LOD2 by Blender decimate + normal transfer
      │  collision_proxy.py→ <name>_COL convex/box decomposition
      ▼
  cna-content build --format cnb
      │  CNA.GltfImporter/2 → ImportedModelDocument
      │  CNA.ModelProcessor/3 (generateChildAssets where needed)
      │  CNA.ModelContentWriter/3 → CnbModelData
      ▼
  .cnb  →  ContentManager::Load<Model>("…")  →  Microsoft::Xna::Framework::Graphics::Model
```

The runtime never sees glTF. It sees `Model`, `ModelMesh`, `ModelMeshPart`, `ModelBone`,
`VertexBuffer`, `IndexBuffer`, `Effect` — the XNA types. This is the "offline conversion, XNA-shaped
runtime" rule the brief asks for, and it is CNA's own documented pipeline
(`docs/content-pipeline.md:370`).

### 21.2 What the pipeline gives us

* **Bone hierarchy, `ParentBone`, `BoundingSphere`, materials, textures** — real, from CNB.
* **Skinned models**: mesh placements are grouped by skin; one group = one `Model`; a
  single-group animated glTF keeps every clip embedded in Model schema 1
  (`docs/content-pipeline.md:453-457`).
* **`SkinningData` on `Model::Tag`** for the first skin (`Model.hpp:159-171`). `cna-house` does
  **not** read it and never calls the multi-skin accessor: skeleton and clip data come from a
  project-owned `.chanim` sidecar (§47.0), and every runtime `.glb` carries exactly one skin
  (§21.3).
* **`AnimationClip`s** with keyframes, embedded in the model (`CnbModelData.hpp` `animations`).
* **Multi-group files** (a character plus separate static props in one glTF) need
  `generateChildAssets: true` and produce named child Models/Textures/Clips. We avoid needing it
  by authoring one logical object per `.glb`; it stays available for a few multi-part assets.

### 21.3 What we must handle ourselves

| Issue | Handling |
|---|---|
| **Winding**: glTF front faces are CCW; XNA's default state culls CCW | Bind `RasterizerState::CullClockwise` for all model draws; one shared state object; flip for mirrored placements (`docs/gltf-conventions.md:22`) |
| **PBR → Blinn-Phong**: source assets are metallic-roughness; the stock effects are not | `tools/assets/pbr_to_stock.py` derives `DiffuseColor`, `SpecularColor` and `SpecularPower` from base colour, metallic and roughness with a documented, fixed mapping, and bakes the metallic term into the specular colour. Recorded per material in `layout.materials.json`. Tier E's `RoomLit.fx` uses the same parameters, so both tiers agree. |
| **Normal maps** are unused by the stock effects | Tier S ignores them. Tier E's `RoomLit.fx` samples them. Assets keep them; the manifest records that Tier S drops them. |
| **Emissive** is not a `BasicEffect` texture | Emissive surfaces are split into their own material and drawn unlit with `BasicEffect { LightingEnabled=false; DiffuseColor = emissive; }` |
| **Vertex colours** | Preserved; `VertexColorEnabled` where the material declares it |
| **Multiple UV sets** | Channel 0 = albedo, channel 1 = lightmap. `DualTextureEffect` uses exactly two, which is what it is for |
| **Several skins in one glTF** | Not allowed into the build, and **CNA enforces this for us** (measured, `HOUSE-00076`). `CNA.ModelProcessor` *refuses* a multi-skin glTF outright — *"glTF produced 2 Model documents; set ModelProcessor bool parameter `generateChildAssets` to true to publish the deterministic multi-Model output set"* — so a multi-skin source cannot silently reach the runtime. Two routes then exist and were measured to render **pixel-identically**: (a) the pipeline's own per-skin split, `generateChildAssets: true` in the asset config, which publishes the lexicographically first group as the primary asset and the rest as `Model` children under ordinary logical names (`P1TwoSkin`, `P1TwoSkin_P1SkinB`) loadable with plain `ContentManager::Load<Model>()`; (b) `tools/assets/skin_split.py`, one `.glb` per skin, for sources the pipeline route cannot name acceptably. **Route (a) is the default** — it is one config line instead of a project-owned tool. Either way every runtime `Model` is single-skin, and `getSkinsEXTProperty()` is never needed (§47.0). The parts reassemble because each carries an **attachment record** naming the shared skeleton root, its attachment bone and its skin's joint names **in blend-index order**; `HOUSE-00074` measured that those indices are skin-local, so that name list *is* the binding. |
| **Draco compression** | Supported by CNA only with `libdraco-dev` present; we do **not** use it — sources stay uncompressed for reproducibility, and size is handled by the pack system |
| **Scale drift** | `scale_check.py` asserts every asset's bounds against its category (§70.5) *before* it enters the build |

### 21.4 Procedurally generated glTF

Generated by our own Blender scripts, from the layout data, deterministically:

| Generator | Output | Notes |
|---|---|---|
| `house_shell_gen.py` | floors, ceilings, walls with openings, skirtings, cornices, stair carriages/treads/risers/balustrades, roof planes, dormers, knee walls, foundation walls, the garage shell, the sunroom, the porch, balconies | The single biggest generator. **Measured 2026-09-08 by `HOUSE-00479`: 33 486 triangles**, not the ~180 000 estimated here — see §72 |
| `prop_kit_gen.py` | cabinet carcasses, shelving, plain boxes, crates, radiators, skirting-board runs, ducts, pipe runs, wire runs | Simple, structurally verifiable shapes |
| `fence_gen.py` | fences, gates, trellis, the shed, §10.4's road-end stone wall | From the exterior layout |
| `terrain_gen.py` | ground mesh tiles, road, sidewalks, kerbs, driveway, paths, terrace | From the height field |
| `neighbourhood_gen.py` | every neighbour house body from a small parametric grammar (footprint, storeys, roof type, garage, porch) with 8 material palettes, plus the impostor card | Deliberately simple, deliberately distant. `HOUSE-00841`: the roof is `roof_geometry`'s planes, so a neighbour's ridge is where our own ridge would be, and LOD1 is the massing WITHOUT the window and door reveals -- §26.2's 0.35 from one decision rather than from a decimation pass |
| `collision_proxy.py` | `_COL` meshes for generated geometry | |
| `impostor_render.py` | 8-yaw billboard atlases for trees and distant houses | Renders with Blender EEVEE, packs into 2048² atlases |

Every generated `.glb` goes through the **same** validation gate as a downloaded one, and gets a
manifest row with `origin.kind = "generated"` recording the generator, its version and its seed.

### 21.5 Characters specifically

1. MakeHuman / MPFB2 produces two base meshes (male ~1.78 m, female ~1.66 m) with clean topology.
   **Measured and fixed by `HOUSE-00294`:** MPFB 2.0.17's helper-free basemesh is 26,756
   triangles and exceeds §72's 22,000-triangle bar; its official generic proxies are larger and
   its 1,600-vertex proxies are visibly too angular. One topology-preserving Blender unsubdivide
   pass produces the selected 6,772-vertex / 13,540-triangle closed mesh for each sex, retaining
   the UV and the neutral silhouette. The exact core-only recipe and four-view review live in
   `tools/assets/generate_human_bases.py` and
   `docs/asset-selection/human-base-mesh-research.md`; no community asset is part of either body.
2. Blender: Rigify → a **32-bone game rig** (well within `SkinnedEffect::MaxBones = 72`), skin
   weights limited to **4 influences per vertex** (`WeightsPerVertex = 4`, and CNA now enforces
   the count on the GPU — `docs/skinnedeffect-support.md` Task 895).
3. Clothing and hair are separate meshes bound to the same rig, exported as separate `.glb`
   files sharing the skeleton, so customisation is a matter of which meshes are drawn.
4. **Updated by `HOUSE-00295`:** six fixed CMU subject-91 walk trials were retargeted onto the
   31-bone MPFB research rig and all failed the deformation bar at the rig/bind boundary. The raw
   data and rejected GLB are not shipped. `HOUSE-02219` authors the 18 clips directly on the final
   32-bone game rig instead; its cleanup, loop and gait bars are unchanged.
5. One `.glb` per (body, clip-set); a second per clothing piece; a third per hair piece.
6. `cna-content` produces a `Model` per file; `tools/assets/anim_extract.py` produces one
   `.chanim` sidecar carrying the body's skeleton and clip set (§47.0). The game verifies at load
   that the sidecar's joint list and every customisation mesh's bone list are identical
   name-for-name, and refuses otherwise — a real content bug caught at load, not a silent
   deformation.

The pets follow the same path with their own rigs (dog 38 bones, cat 34).

---

## 22. Materials

### 22.1 The application-owned material

`cna-house` defines its own material record and maps it onto XNA effects. It deliberately does not
resemble a modern engine material system, because that would be a lie about what the stock effects
can do.

```jsonc
{
  "id": "MAT_WOOD_OAK_FLOOR",
  "class": "wood",
  "albedo": "Textures/Arch/oak_floor_albedo",
  "normal": "Textures/Arch/oak_floor_normal",      // Tier E only
  "lightmapChannel": 1,
  "tint": [1.00, 0.97, 0.93],
  "alphaMode": "opaque",                            // opaque | mask | blend
  "alphaCutoff": 0.5,
  "twoSided": false,
  "specularColor": [0.18, 0.16, 0.13],
  "specularPower": 24,
  "alpha": 1.0,
  "uvScale": [1.0, 1.0],
  "wetResponse":  { "albedoDarken": 0.28, "specularBoost": 2.4, "powerBoost": 3.0 },
  "snowResponse": { "coverable": true, "slopeLimitDeg": 40 },
  "footstepSurface": "hardwood",
  "audioAbsorption": 0.16,
  "effectTierS": "DualTexture",
  "effectTierE": "RoomLit"
}
```

At runtime these rows load as `world::MaterialDef` records. `MaterialBinder::RegisterAll` converts
the complete table into an ID-keyed registry before the first frame and refuses the table
atomically if any row is invalid or duplicated. A lightmap is not part of `MaterialDef`: §23 chooses
it from the visible room/light group for each draw.

`HOUSE-00900` adds a deterministic `MAT_BASE_*` reference row for each of the 34 acquired PBR
sets. `tools/assets/pbr_to_stock.py` measures the prepared albedo and ORM images, fixes the reviewed
dielectric/metal endpoint, and writes the mapping above; its CI check compares every field against
the generated row. These neutral base rows are authoring sources, while `HOUSE-00901` onward names
placement-specific variants with explicit tint, UV scale and environmental response. Each base row
is retained in a sphere/floor/wall preview at low, medium and high light levels.

`HOUSE-00901` defines the first placement library as 12 reusable wall colours and 6 ceiling
finishes. Occupied rooms share the clean fine-plaster source and a restrained warm/cool family;
chipped maps are reserved for the named dusty-blue, aged-plaster, smoky-ochre and attic finishes
after contact-sheet review showed they read as deliberate wear rather than ordinary fresh paint.
The exact ids, source mapping, tints and review are in
`docs/asset-selection/interior-paints.md`. Cell assignments remain solely `HOUSE-00908`'s data.

`HOUSE-00902` similarly fixes 12 interior floor records: oak, walnut, three carpets, four tiles,
concrete, stone and vinyl. Vinyl maps to the `tile` class because §22.2 deliberately has no vinyl
class and a static floor needs `DualTextureEffect`'s lightmap slot; its explicit `vinyl` footstep
and absorption retain the distinct gameplay semantics that the class cannot express. Review also
maps the misleadingly named acquired tile slugs by their visible finish rather than propagating
their source-selection labels. Exact rows and the review sheet are in
`docs/asset-selection/interior-floors.md`; cell assignments still belong to `HOUSE-00908`.

`HOUSE-00903` fixes the nine exterior records: three siding colours, brick water table, roof
shingle, white soffit, broomed concrete, asphalt and gravel. The acquired set contains no named
roofing map; retained visual review selected its charcoal square-unit texture as the closest honest
shingle-course source, then gives it explicit asphalt wet, snow and audio semantics. The soffit is
the only member that rejects snow because it faces downward. Exact rows and the review sheet are
in `docs/asset-selection/exterior-materials.md`; shell assignment remains `HOUSE-00907`'s work.

`HOUSE-00941` corrects the siding source after fixed-camera review proved the original ambientCG
`Wood095` base reads as broad orange bare-board grain across the largest facade surface. The three
stable siding ids now share a deterministic neutral painted-clapboard albedo and matching linear
normal: one world-metre UV tile contains six 167 mm exposed horizontal courses, with a narrow lap
shadow and restrained paint variation. Their tint, wet/snow/audio semantics, shell/lightmap
ownership and stock-XNA effect paths are unchanged. The approved bare-board source remains in the
library for joinery, furniture and fence pickets; changing the facade does not silently turn those
unrelated objects into siding.

`HOUSE-00943` makes the corresponding roof correction. The stable dry, wet and unbaked roof ids
retain their asphalt class, weather response, audio semantics and stock-XNA effect mappings, but
their visible source is a deterministic project-authored asphalt-shingle albedo/linear-normal pair
instead of the acknowledged `Tiles140` square ceramic surrogate. One metre along the actual roof
slope carries seven approximately 143 mm exposed courses with four staggered tabs. Roof UV0 uses
an orthonormal contour/slope basis: U stays horizontal and V measures true surface distance, so hip
and dormer courses retain physical scale without changing the world-space roof surface or
silhouette, collision or lightmaps. Five pre-existing twisted dormer transition quads are split
explicitly on the same export diagonal before UV generation so each rendered triangle has one
metric basis instead of relying on an exporter-selected triangulation after UV assignment.

`HOUSE-00904` fixes four glass and four water records. Clear, obscured, cabinet and shower glass
retain the untextured tinted `BasicEffect` path; tap/shower flow, bath/basin level, toilet bowl and
rain puddle share a deterministic tileable RGBA/normal pair with role-specific tint, opacity and UV
scale. The source alpha is premultiplied by the content pipeline, matching §23.6; normals stay
linear. Exact rows, texture hashes and the transparency review are in
`docs/asset-selection/glass-water-materials.md`.

`HOUSE-00905` fixes fourteen fully-wet Tier-S endpoints for every exposed non-metallic exterior
finish. Each reuses its dry texture and bakes that row's `wetResponse` into a darker tint, stronger
specular colour and tighter highlight; its `wet_<class>` state and `SurfaceBlend/Wet` technique
retain the continuous Tier-E path. The sheltered soffit and the two already-specular black-tint
metal finishes are the three deliberate exclusions from the seventeen-finish outdoor inventory.
Saturated garden soil changes its footstep surface to `mud`. Exact pairs and dry/wet review are in
`docs/asset-selection/wet-materials.md`.

`HOUSE-00906` fixes the snow shell's one shared procedural powder surface and its six
`snow_<class>` rows: asphalt, grass, metal, soil, stone and wood, exactly matching the offline
shell source graph. Tier S draws the unlightmapped `TEXCOORD_0` shell through `BasicEffect` and
multiplies the row's full-opacity endpoint by §38's depth smoothstep; Tier E consumes the same
albedo/normal maps through `SurfaceBlend/Snowy`. Slight class tints retain the substrate influence
at shallow coverage without duplicating texture bytes. Snow rows reject recursive cover, select
the `snow` footstep surface and use 0.85 absorption. Exact rows, hashes and shallow/deep review are
in `docs/asset-selection/snow-materials.md`.

`HOUSE-00908` assigns a complete four-finish palette to all 78 non-exterior cells: the 76
tabulated interiors plus the refrigerator and freezer sub-cells. The ground floor and main
circulation share warm whites and oak, formal rooms repeat walnut and hardwood joinery, bedrooms
vary within the same muted paint family, the upper floor shifts cooler, and chipped finishes are
confined to basement service rooms and roof-space. `trimMaterial` is the one cell datum §15.3 was
missing: it selects a non-lightmapped `BasicEffect` joinery finish independently from the
lightmapped wall paint. The complete reviewable table is
`docs/asset-selection/room-palettes.md`; runtime and offline shell tools read the same cell fields.

`HOUSE-00907` applies those palettes to the generated shell. Each glTF material records both its
authored `materialId` and its semantic `surfaceClass`: keeping both lets a bluestone terrace floor
remain a lightmap receiver while a step in the same finish remains dynamically lit detail. Window
glass comes from `layout.openings.json` (including the three obscured bathroom sets), stair finish
comes from `layout.stairs.json`, basement outer skin and the chimney use the brick water-table
finish, upper outer skin uses warm-white siding, and roof/eaves/metalwork use the authored shingle,
white-soffit and gutter finishes. The chunk builder validates every shell id against
`layout.materials.json` and reads alpha mode from that row rather than guessing from a class name.

### 22.2 Material classes and their effect mapping

| Class | Tier S effect | Tier E technique | Notes |
|---|---|---|---|
| `paint` (walls, ceilings) | `DualTextureEffect` | `RoomLit/LitLightmap` | The bulk of interior surface area |
| `wood` (floors, trim, doors, furniture) | `DualTextureEffect` (static) / `BasicEffect` (dynamic) | `RoomLit` | |
| `carpet` | `DualTextureEffect` | `RoomLit` | High absorption, matte |
| `tile`, `stone`, `concrete` | `DualTextureEffect` | `RoomLit` | |
| `metal` | `BasicEffect` with high specular, or `EnvironmentMapEffect` for chrome | `RoomLit` | Taps, appliance fronts, handles |
| `plastic` | `BasicEffect` | `RoomLit` | |
| `glass` | `BasicEffect`, `BlendState::AlphaBlend`, `DepthStencilState::DepthRead`, `Alpha ≈ 0.12`, plus an `EnvironmentMapEffect` pass for reflection on the big panes | `RoomLit/Glass` | Windows, cabinet fronts, the shower screen |
| `fabric` | `BasicEffect`, low specular | `RoomLit` | Upholstery, curtains, bedding |
| `skin` | `SkinnedEffect`, specular power 12 | `SkinLit` | |
| `hair` | `AlphaTestEffect` cards + `SkinnedEffect` for the scalp | `SkinLit/Hair` | |
| `fur` | `SkinnedEffect` with a fur-textured albedo, plus 1 alpha-tested shell card layer | `SkinLit/Fur` | Dog and cat |
| `foliage` | `AlphaTestEffect`, two-sided, cutoff 0.5 | `RoomLit/Foliage` with wind | |
| `asphalt`, `gravel`, `grass`, `soil` | `DualTextureEffect` (terrain lightmap = AO) | `RoomLit` | |
| `water` | `BasicEffect` with scrolling UVs, alpha blend, additive specular highlight | `WaterFlow` | Taps, bath, toilet bowl, puddles |
| `wet_<class>` | a second material with darkened albedo | `SurfaceBlend/Wet` | Tier S swaps the material; Tier E blends continuously |
| `snow_<class>` | a snow shell mesh drawn over the base | `SurfaceBlend/Snowy` | |
| `emissive` | `BasicEffect` `LightingEnabled=false` | `RoomLit/Emissive` | Lamp shades, TV, fridge interior, appliance LEDs |

**Static shell surfaces: which of these paths, and when** (`HOUSE-00471`). A material's row above
says what it is; §18.3 says whether it is lit by a bake:

| Shell surface | Lit by | Tier S | Tier E |
|---|---|---|---|
| Floors, ceilings, wall surfaces — the **interior lightmap receivers** | baked lightmap + dynamic room term | `DualTextureEffect`, lightmap in texture 2 | `RoomLit/LitLightmap` |
| House outer skin — room-resident but outdoor-lit | its baked uniform-sky shape × live, unattenuated outdoor hemisphere irradiance; never the adjacent room's lamps or window attenuation | `DualTextureEffect`, `LM_DAY` in texture 2 | `RoomLit/LitLightmap` |
| Skirtings, cornices, architraves, thresholds, frames, sashes, nosings, handrails, balusters, rafters, gutters, downspouts — **architectural detail** | the room's or the exterior's dynamic term only | `BasicEffect` (the stock path its class already names above) | `RoomLit`, no lightmap sample |
| Glass | its own transparent/reflection path | `BasicEffect` + `EnvironmentMapEffect` as §22.2's `glass` row says | `RoomLit/Glass` |
| Door and window leaves, cabinet fronts, anything an interactable moves | dynamic only | as the class's row | as the class's row |

Detail geometry needs no second UV channel and no artificially high lightmap density; it is small,
it is close to the receiver behind it, and the room term it shares with that receiver is what keeps
the two consistent. Tier S is complete on its own here — nothing about this arrangement depends on
Tier E, which improves the result and is not required for correctness.

The outer skin is the deliberate exception to residency implying lighting ownership
(`HOUSE-00922`). Its siding and brick islands already live in the adjacent cell's `LM_DAY` atlas,
where the uniform world sky bakes eave, reveal and self-occlusion into them. At runtime that atlas
is the opaque base draw, tinted by `LightingSystem`'s global outdoor hemisphere irradiance. This
is distinct from the intentionally saturated sky-display gradient: `HOUSE-00927` mixes the
existing solar colour anchors with normalized sky chroma and scales them by `SunShadingFor`'s
cloud-aware diffuse energy, with dim display-gradient fallback through twilight and night. The
approved albedo's physical repeat UV0 then remains visible instead of becoming a dark blue-grey
slab. In Tier S the outdoor
receiver remains scene-referred against the unexposed sky even when an indoor camera has adapted
to a dark room (`HOUSE-00925`); the room's receiver still uses that camera's effect exposure. The
room's artificial atlases and its window-transmission-scaled daylight pass are not submitted for
the outside face. Thus closing a room or turning on its lamp cannot darken or illuminate the
façade, while no second bake format or runtime shader is introduced.

### 22.3 Why `footstepSurface` and `audioAbsorption` live here

They are the same physical fact as the visual material. Putting them in one record means a
designer who changes the kitchen floor from tile to wood automatically changes the footstep
sound and the room's reverberation hint, and cannot forget. It is also what the acoustic model
(§64) reads.

### 22.4 Material instancing

A material is bound once per draw batch. To avoid a texture change per prop, common props share
**atlas materials**: `MAT_ATLAS_KITCHEN_SMALL` (2048², 64 small kitchen objects),
`MAT_ATLAS_GARAGE_CLUTTER`, `MAT_ATLAS_ATTIC_CLUTTER`, `MAT_ATLAS_BOOKS`,
`MAT_ATLAS_GARDEN_SMALL`. Atlas packing is done by `tools/assets/atlas_pack.py` and recorded in
the manifest.

Two consequences of atlasing, both **measured** by `HOUSE-00192` rather than assumed:

* **An atlased prop may not tile its texture.** Inside an atlas `REPEAT` wraps the whole atlas, not
  the region, so a prop with UVs outside [0, 1] samples its neighbours. `atlas_pack.py` refuses such
  a prop; the fix is to bake the repeat into one texture before atlasing, not to atlas it anyway.
  Atlas samplers are `CLAMP_TO_EDGE`.
* **The mip chain is finite.** With the default 4-texel edge-replicated gutter, a representative
  40-prop 2048² atlas is mip-safe to **level 3** — the level above that averages two props' texels
  together, and no gutter fixes it. Content that needs a full chain needs its own texture. The tool
  computes and records the level per atlas (`mipSafeLevels`) rather than the project asserting one.

A material's *whole* texture set — albedo, normal, metallic-roughness, occlusion, emissive — is
packed as one unit at one region, because one set of UVs addresses all of them. That is why a
material whose maps differ in size cannot be atlased until they are made to match.

---

## 23. Static rendering

### 23.1 What "static" means

A surface is static if it never moves and never changes material. That is the architectural shell,
the terrain, the fences, the road, and every prop the player cannot open, move or switch. About
**82 %** of the world's triangles.

### 23.2 The static draw path

Per visible cell, per chunk (§17.4):

```cpp
device.SetVertexBuffer(chunk.vb);
device.setIndicesProperty(chunk.ib);
effect.setTextureProperty(chunk.albedo);          // DualTextureEffect
effect.setTexture2Property(pass.lightmap);     // neutral base, LM_ART group or LM_DAY
effect.setDiffuseColorProperty(pass.tint);      // floor or one live source's intensity/colour
effect.setWorldProperty(Matrix::Identity);        // baked into the chunk
effect.setViewProperty(view);
effect.setProjectionProperty(projection);
for (auto& pass : effect.getCurrentTechniqueProperty()->getPassesProperty()) {
    pass.Apply();
    device.DrawIndexedPrimitives(PrimitiveType::TriangleList, 0, 0,
                                 chunk.vertexCount, 0, chunk.primitiveCount);
}
```

The `DualTexture` binder contract therefore requires both `DrawParams::diffuse` and
`DrawParams::lightmap`; omitting either is a draw error reported before an effect is allocated.
The same per-draw fog boundary as §31.5 is applied here, because exterior static surfaces are the
largest fog receivers.

Chunks are pre-transformed into **world space** at build time, so `World` is identity and no
per-prop matrix upload happens. The cost is that a chunk cannot be instanced — which is the right
trade for a building.

### 23.3 Multi-pass room lighting (Tier S)

For each static chunk in a lit room:

| Pass | Effect | Texture2 | Diffuse | Blend | Depth |
|---|---|---|---|---|---|
| 1 | `DualTextureEffect` | neutral half-grey | `ambientFloor` | `Opaque` | write, `LessEqual` |
| 2..k+1 (active only) | `DualTextureEffect` | `LM_ART_<groupN>` | `artColor(groupN) · level(groupN)` | `Additive` | no write, `Equal` |
| k+2 (daylit only) | `DualTextureEffect` | `LM_DAY` | `skyColor · daylightLevel` | `Additive` | no write, `Equal` |

`k` ≤ 4 light groups per room (the validator enforces this). A dark room with no daylight draws
one pass; the four-group kitchen at noon with all switches on draws six. The
`ambientFloor` (≈ 0.025 × the room's paint
colour) means an unlit windowless room is *very* dark but not pure black — silhouettes remain
legible, which is what a real dark room looks like once your eyes adjust.

Correction (2026-09-15, `HOUSE-01280`): the original first row multiplied the 0.025 floor by the
primary lamp's spatial bake. `L0_LIVING`'s primary bake has peak 10.55 but mean 0.011 irradiance;
with that switch off, most painted walls were effectively black even at 10:30 in clear weather.
The advertised floor could not survive that product. Separating a neutral opaque floor from the
active authored lamp passes is the smallest stock-XNA correction that retains UV2, switchable
baked fixture shape and depth-equal additive composition. Budget and render equivalence must be
re-measured with the extra pass when groups are on: the canonical four-group kitchen's pass count
is six (two with its groups off and daylight present), asserted by `HOUSE-01280`'s integration
test; the 18 paired culled/unculled render views remain pixel-equivalent. Full GPU frame-time
measurement still belongs to `HOUSE-01276`, not the screenshot HUD's fixed-step FPS readout.

Depth-`Equal` on the additive passes requires an exact depth match, which is guaranteed because
the passes draw identical geometry with an identical transform.

### 23.4 Single-pass room lighting (Tier E)

`RoomLit.fx` technique `LitLightmapShadowed` collapses all of the above into one pass:

```
float3 lit = albedo.rgb * ( lightmapArt.rgb * artLevels          // up to 3 groups packed in RGB
                          + lightmapDay.rgb * dayLevel * skyColor
                          + Blinn(sunDir, N, V) * sunColor * shadow
                          + sum_{i<4} PointLight(i, P, N, V) );
```

Three light groups pack into the RGB channels of a single lightmap texture — one texture fetch
instead of three. That is a 3–5× reduction in static draw calls in lit rooms and is the main
reason Tier E exists.

### 23.5 Sorting and state

Draw order within the opaque pass: **effect → material → chunk**, so `EffectPass::Apply()` and
texture binds are minimised. Render state is set by *pass group*, not per draw. A per-frame
`StateTracker` skips redundant `BlendState`/`DepthStencilState`/`RasterizerState` assignments and is
asserted correct by a real-device test that replays a recorded command list. The repeat-UV world
needs `LinearWrap` in sampler 0. CNA initializes it that way, but its HUD `SpriteBatch::End()` leaves
the batch sampler bound for the next frame; `HOUSE-00927` therefore explicitly selects XNA
`LinearWrap` in the HUD's ordinary Begin overload. An indexed assignment would select a forbidden
`CNAEXT` copy assignment (BL-16). Lightmap UV1 islands remain inside the atlas and carry baked
gutters, so slot 1's default wrap never intentionally tiles a lightmap; no runtime bypass of the
strict-XNA API is used.

### 23.6 Transparency

Glass, water, curtains and particles are drawn last, back-to-front by cell distance then by
per-object distance, with `DepthStencilState::DepthRead` (test, no write). Windows are the
common case: a window pane is one quad per opening, drawn after the room behind it. Interior
glass (cabinet doors, the shower screen) sorts within its cell.

`HOUSE-00898` makes those two distances separate `RenderItem` keys. For static chunks, the cell
distance is measured to the centre of the union of all chunk bounds in that cell and the object
distance to the individual chunk centre; reusable scratch keeps that derivation allocation-free
after warm-up. `RenderList` resolves `MaterialDef::alphaMode` through the loaded `WorldData` table:
only `blend` enters `Transparent`, while a missing table or unknown material remains on the
layout-derived pass rather than being guessed from its name. `TransparentPass` consumes the sorted
slice without material regrouping under `CullNone`, premultiplied `AlphaBlend` and `DepthRead`.

Alpha-tested foliage and fences are drawn **before** transparency with full depth writes, because
`AlphaTestEffect` gives correct depth and sorting is then unnecessary.

`HOUSE-00899` implements that rule as `AlphaTestPass`. It consumes the effect/material-grouped
alpha-test slice, resolves each run's albedo through the shared content cache, and binds the run
once through the game's single lazy `MaterialBinder` pool. The binder supplies the material's exact
cutoff, tint and two-sided cull policy; the pass supplies `BlendState::Opaque` and
`DepthStencilState::Default`, so rejected texels write nothing and accepted texels write full
opaque depth. Both caches and the binder are owned ahead of `Renderer`, making their lifetime
longer than every renderer-owned pass that borrows them. The enum-defined frame order keeps this
pass immediately before `Transparent`.

---

## 24. Animated rendering

### 24.1 Skinned characters

Three skinned actors: the player avatar (when in third person or seen in a mirror), the dog, the
cat. Each is drawn as:

```cpp
skinnedEffect.SetBoneTransforms(clipPlayer.GetSkinTransforms());   // ≤ 72 matrices
skinnedEffect.setWorldProperty(actorWorld);
skinnedEffect.setDiffuseColorProperty(tint);
skinnedEffect.setTextureProperty(skinTexture);
// three directional lights configured from the actor's room (§28.5)
model.Draw(actorWorld, view, projection);        // or manual mesh-part draws for LOD control
```

Bone budget: player 32, dog 38, cat 34 — all well under `MaxBones = 72`.
`WeightsPerVertex = 4` throughout.
`rendering::MaterialBinder` requires a non-empty skin-local palette and albedo before allocating its
shared `SkinnedEffect`, fixes `WeightsPerVertex` at 4, pads a short palette to 72 identities, and
applies the material tint/specular values plus the per-draw fog boundary from §31.5.

### 24.2 Animated props

Doors, drawers, cabinet doors, the garage door, the gate, taps, the fridge door, the washing
machine drum and the toilet flush handle are **rigid animations**, not skinned. Each is a dynamic
instance whose `World` matrix is computed from its interactable's `openFraction` by a small
closed-form function per kind:

| Kind | Transform |
|---|---|
| hinged door | `Translate(-hinge) · RotateY(±maxAngle · f) · Translate(hinge)` |
| drawer | `Translate(outAxis · depth · f)` |
| sliding gate / sliding door | `Translate(slideAxis · travel · f)` |
| sectional garage door | 5 panel segments, each following a spline sampled at `f` |
| tap handle | `RotateZ(maxAngle · f)` |
| toilet lid / seat | `RotateX(maxAngle · f)` |
| flush handle | one-shot 0.4 s ease |

No animation content, no skeletons, no keyframes — just an ease curve
(`smoothstep`, or an ease-out-back for the garage door's final settle) and a matrix. This keeps
640 interactables affordable.

### 24.3 Vegetation

Trees and shrubs sway. Tier S: the foliage card meshes are stored in a `DynamicVertexBuffer` per
species-instance-group, and a CPU pass offsets each vertex by
`amplitude(vertexHeight) · sin(t · freq + phase(instance))` scaled by wind speed, updated at
**15 Hz** (not per frame) for the ≤ 40 instances within 25 m; further instances are static. Tier E:
the same maths in the vertex shader for every instance.

### 24.4 Particles

Rain, snow, hail, splashes, dust motes in sunbeams, fireplace embers, and the kitchen kettle's
steam. One shared particle renderer:

* CPU simulation into a `DynamicVertexBuffer` of camera-facing quads
  (`VertexPositionColorTexture`), rebuilt each frame with `SetData(..., SetDataOptions::Discard)`.
* One draw call per *particle material*, ≤ 6 materials, ≤ 2 000 quads total.
* `BasicEffect` with `TextureEnabled`, `VertexColorEnabled`, `LightingEnabled = false`, alpha
  blend (or additive for splashes and embers).
* Tier E adds soft-edge depth fade and wind shear in `Precip.fx`.

2 000 quads = 8 000 vertices = ~200 KB per frame of vertex upload. Measured against the budget in
§71; the fallback if it is too expensive is a 30 Hz particle update with interpolation.

---

## 25. Culling and visibility

This is the system the brief cares most about, so it is specified in full.

### 25.1 The pipeline

```
                     camera position, orientation, projection
                                     │
                     ┌───────────────▼────────────────┐
                     │ 1. locate camera cell (§16.4)  │
                     └───────────────┬────────────────┘
                                     ▼
                     ┌────────────────────────────────┐
                     │ 2. portal traversal with        │
                     │    frustum reduction            │  → visible cells + per-cell frusta
                     └───────────────┬────────────────┘
                                     ▼
                     ┌────────────────────────────────┐
                     │ 3. per-cell chunk/instance      │  → draw list
                     │    AABB/sphere vs. frusta       │
                     └───────────────┬────────────────┘
                                     ▼
                     ┌────────────────────────────────┐
                     │ 4. distance cull + LOD select   │
                     └───────────────┬────────────────┘
                                     ▼
                     ┌────────────────────────────────┐
                     │ 5. sort by pass/effect/material │
                     └────────────────────────────────┘
```

### 25.2 Step 2 in detail — portal traversal

```
struct Work { CellId cell; ClipFrustum frustum; uint8 depth; uint8 flags; };

visit(cameraCell, cameraFrustum, depth = 0)

pop (cell, frustum, depth):
    mark cell visible; append frustum to cell.frusta (cap kMaxFrustaPerCell = 4)
    for each portal p of cell:
        if p.opacity == opaque_when_closed and p.apertureFraction <= 0.05: continue
        if p.plane faces away from the camera:                            continue
        if depth >= maxDepthFor(p):                                       continue
        clipped = ClipRectToFrustum(p.rect, frustum)
        if clipped is empty:                                              continue
        if NdcArea(clipped) < kMinPortalNdcArea:                          continue
        next = ReduceFrustum(camera, clipped, frustum)
        if p.opacity == translucent: next.flags |= DIFFUSE
        if already visited p.other with a frustum that contains next:     continue
        push (p.other, next, depth + 1)
```

**`ClipRectToFrustum`** — because portals are axis-aligned rectangles on axis-aligned planes, the
clip is a 2-D Sutherland–Hodgman of the rectangle against the frustum's 4 side planes, performed
in the frustum's own space. It produces a convex polygon of ≤ 8 vertices.

**`ReduceFrustum`** — build a new frustum whose side planes each contain the camera position and
one edge of the clipped polygon, keeping the original near and far planes. Implemented over
`Microsoft::Xna::Framework::Plane` and evaluated with an explicit
`ContainmentType Intersects(const BoundingBox&)` written for the ≤ 8-plane case (XNA's
`BoundingFrustum` is fixed at 6 planes, so the reduced frustum is our own small type that reuses
`Plane` — a Tier-A construction, no CNAEXT).

**`maxDepthFor(p)`**:

| Portal kind | Max depth from an interior camera | From an exterior camera |
|---|---|---|
| cased opening, door, stair well | 6 | **6** |
| window / glass (interior → `EXT_WORLD`) | 3 | — |
| window / glass (`EXT_WORLD` → interior) | — | **1** |
| garage door | 4 | 2 |

The glazing asymmetry matters: standing in the garden you should see *one* room through a window,
not that room plus everything behind its open door. Depth 1 from outside delivers exactly that.
An authored-open entrance door can expose a long sightline through the house, so doors use six
from either camera side; the former exterior limit of two assumed the superseded all-doors-shut
start and over-culled the porch view once `HOUSE-03221` gave the entrance its static-open pose.

**`kMinPortalNdcArea = 1.2e-5`** — about 2 × 2 pixels at 1280 × 720. Below that the target cell
contributes nothing and the chain stops. This single cutoff is what keeps a corridor of eight open
doors from exploding.

**Frustum containment test** (`already visited with a containing frustum`): a cheap conservative
approximation — compare the clipped polygon's NDC bounding rectangle; if the new one is inside a
previously recorded one for that cell, skip. Exact containment is not needed for correctness, only
for efficiency, and a false "not contained" merely costs one more traversal.

### 25.3 Door apertures and partially-open doors

The brief asks specifically how a partially open door affects visibility. The honest, correct
answer:

> **A door leaf occludes geometry inside the target cell; it does not shrink the portal.**

The portal is the *doorway* — a hole in the wall. A sight line crossing the doorway plane is never
blocked by the leaf, because the leaf is not in the doorway plane; it has swung out of it into one
of the two cells. So the moment `apertureFraction > 0.05` the portal's aperture is the **full
doorway rectangle**, and the leaf becomes an ordinary opaque object standing inside the target
cell, hiding whatever is behind it exactly as a wardrobe would.

This is conservative (never culls something visible), simple, and produces the right picture: a
door open 10° reveals a thin slice of the room *because the leaf blocks the rest*, not because the
portal was narrow.

Two refinements, both `SHOULD`, both optional:

* **Leaf silhouette clipping** — subtract the leaf's projected quad from the reduced frustum. Only
  worth it if profiling shows barely-open doors are a real cost.
* **Latch hysteresis** — the aperture is treated as closed below 0.05 and as open above 0.08, so a
  door settling shut does not flicker the portal on and off.

A **closed glass door** (`opacity: glass`) never closes its portal for vision, only for movement
and (partially) for sound. A **closed opaque door** closes it completely — this is where the huge
win is: with the kitchen door shut, the kitchen's 5 chunks, 140 props, 4 lights and 6 further
portals all vanish from consideration in one test.

### 25.4 Why not stencil portals, and why not a BSP

* **Stencil portals** are impossible: `Clear` ignores `ClearOptions::Stencil` and
  `ReferenceStencil` is unconnected on EasyGL, and EasyGL render targets allocate no stencil at
  all (BL-02). Frustum reduction needs no stencil and is cheaper anyway.
* **BSP / PVS precomputation** would give a perfect potentially-visible set, but the visibility
  here is *dynamic* — it depends on which doors are open. A precomputed PVS would have to be
  computed per door-state combination (2⁶² of them). Portal traversal computes the answer exactly,
  at runtime, for the actual door states, in well under a millisecond.

### 25.5 Occlusion queries — where they help and where they do not

`OcclusionQuery` is used for exactly one thing in the main path: **sun and moon glare visibility**
(§32.4). It is *not* used for general object culling, and the reasoning is worth recording:

1. Portal visibility already answers the indoor question exactly and for free. An occlusion query
   would re-derive a worse answer at a higher price.
2. A query result is available one frame late at best; using it to cull produces popping.
3. On the ES 3.x profile `PixelCount` is a boolean (BL-07), so "how much is visible" is
   unavailable — only "is anything visible".
4. Per-object queries cost a draw call each and a GPU→CPU sync.

There is one place it might genuinely pay: **the house body occluding the neighbourhood** when the
player stands in the back garden. `HOUSE-02401` runs a measured experiment (queries around the 24
neighbourhood LOD groups, 60-second recorded camera path, A/B frame time) with a hard go/no-go
gate: adopt only if it saves ≥ 0.6 ms of GPU time at no visual cost. If it does not, the task
records the measurement and the feature is dropped.

### 25.6 Exterior visibility

`EXT_WORLD` is one enormous cell, so portal traversal cannot help inside it. Within it:

1. **Frustum culling** of a **loose 3-level bounding-volume hierarchy** built at load time over
   the ~4 100 exterior instances (terrain tiles, road segments, fences, trees, neighbourhood
   groups, garden props). Static, so the BVH is built once.
2. **Distance culling** per category: small props 45 m, garden furniture 70 m, fences 120 m,
   trees 180 m, neighbourhood LOD0 90 m, LOD1 160 m, LOD2 300 m, impostors 420 m — and **the
   ground 420 m**, which is §10.3's far plane and therefore no distance test at all
   (`HOUSE-00700`). Step 1 names *"terrain tiles, road segments"* among the instances and this
   list did not give them a distance; that was not an omission but an unstated answer, and it is
   stated now. A lawn you cannot see because it is 200 m away is a lawn outside the frustum or
   behind the far plane, and both of those are already answered.
3. **LOD** (§26).
4. **The house shell itself is an occluder** only in the optional experiment of §25.5.

House siding/water-table, weather-facing window detail and explicitly authored exterior entry
leaves participate as bounded instances even though their canonical residency owner remains an
interior cell (`HOUSE-00921`, `HOUSE-00926`, `HOUSE-00930`). This is a visibility bridge, not a
second copy of the mesh; the render list de-duplicates a chunk reached by both routes.

### 25.7 Indoor ⇄ outdoor transition

Crossing an exterior door is just another portal traversal, so there is no special case. What does
need care:

* **Exposure**: `LightingSystem` keeps a target exposure per cell (bright outdoors, dim indoors).
  The camera's exposure follows it with an asymmetric time constant (0.9 s brightening, 2.2 s
  darkening — matching human adaptation being slower in the dark). Tier E applies it in
  `PostComposite.fx`; Tier S applies it by scaling indoor receivers and their lit detail through the
  stock effect's `DiffuseColor`/`AmbientLightColor` and drawing a full-screen `SpriteBatch` tint
  quad for the residual. Clear glass and opaque outdoor receivers remain scene-referred against
  the unexposed sky when visible through a window (`HOUSE-00924`/`HOUSE-00925`): otherwise an
  indoor effect lift clips the approved red-brick chimney, fence and grass while the same frame's
  sky stays blue. The outdoor camera has a sub-unity target, so its residual tint remains global.
  This is what makes stepping
  from the sunlit terrace into the basement stair feel right.
* **Residency**: entering the house from outside promotes the interior packs and demotes the
  neighbourhood detail (§27).
* **Audio**: the listener's cell changes, so every emitter's portal path is recomputed (§64).

### 25.8 Instrumentation — proving it works

`F3` cycles a debug overlay showing, per frame:

```
cells: visible 9 / 95   traversals 23   portals tested 61   frusta 12   maxdepth 4
draws: 412  (static 231, dynamic 118, alpha 34, transparent 21, particles 8)
tris:  1 142 k    verts 786 k    state changes 96
cpu:   vis 0.51  anim 0.63  phys 0.44  audio 0.29  weather 0.41  draw 1.72  total 5.9 ms
gpu:   9.8 ms (estimated from timing queries where available)
cell:  L0_KITCHEN   pos (-3.42, 1.28, -25.06)   yaw 118°
```

`F4` draws the visible cell set as coloured wireframe boxes, the active portals as filled quads,
and the reduced frusta as wire pyramids. `F5` freezes the visibility computation so the camera can
fly out and inspect what was culled — the single most useful debugging tool for a portal system.

Automated proofs (§70.3): a set of 24 named camera poses with expected visible-cell sets, asserted
exactly; a door-state matrix test that opens and closes each of 62 doors and asserts the visible
set changes in the expected direction; a "no over-culling" test that, for each pose, renders once
normally and once with all culling disabled and compares the images (they must match within a
tolerance, proving nothing visible was culled).

---

## 26. Level of detail

### 26.1 Selection metric

Projected screen height in pixels of the object's bounding sphere:

```
h = 2 · r · viewportHeight / (2 · d · tan(fovY / 2))
```

with `d` clamped to `> r`. Selection uses hysteresis (`switch up` at the threshold, `switch down`
at 0.85 × the threshold) so an object at a boundary does not oscillate.

| Level | Threshold | Typical triangle ratio |
|---|---|---|
| LOD0 | h ≥ 220 px | 1.00 |
| LOD1 | h ≥ 90 px | 0.35 |
| LOD2 | h ≥ 30 px | 0.12 |
| Impostor | h ≥ 8 px | 2 triangles |
| Culled | h < 8 px | 0 |

The offline geometric silhouette gate treats every triangle as opaque. Alpha-tested vegetation is
the one exception because this would measure the transparent corners of leaf cards as foliage.
Its ratios, UVs and names remain hard gates, while silhouette quality is reviewed by rendering the
actual alpha material at the transition levels; the evidence is retained with the acquisition.

Global `lodBias` from the quality settings shifts every threshold; the Web and Android tiers ship
with `+1`.

### 26.2 Which objects get LODs

| Category | LODs |
|---|---|
| Architectural shell | none — it is already minimal, and it is always near |
| Large furniture (sofas, beds, wardrobes, appliances) | LOD0/1 |
| Small props | LOD0 only, distance-culled instead |
| Trees | LOD0/1/2 + 8-yaw impostor |
| Shrubs | LOD0/1 + impostor |
| Neighbourhood houses | LOD0/1/2 + impostor |
| Vehicles | LOD0/1/2 |
| Characters and pets | LOD0/1 (bone count is unchanged; only the mesh is decimated) |
| Terrain tiles | LOD0/1 (skirted, so no cracks) |

### 26.3 Impostors

`impostor_render.py` renders each impostor subject from 8 yaw angles at a fixed elevation into a
2048² RGBA atlas, with the alpha carrying the silhouette and the colour pre-lit for an overcast
sky. At runtime an impostor is one camera-facing quad choosing the nearest yaw slice, tinted by
the current sky colour so it does not look pasted-on at sunset. Trees additionally get a small
vertical-axis-only billboard so they do not spin when the camera passes overhead.

### 26.4 Detail sets

Beyond LOD, each cell has three **detail sets** driven by the quality tier and by how the cell was
reached:

| Set | Contents | Dropped when |
|---|---|---|
| `essential` | shell, doors, windows, lights, interactables, large furniture | never |
| `dressing` | books, cushions, ornaments, kitchenware, papers, cables | cell reached through a `translucent` portal, or at quality `low`, or beyond 18 m |
| `micro` | crumbs, small stains, tiny labels, individual pens | quality below `high`, or beyond 8 m |

This is a large, cheap win: a room seen through a frosted door renders its shell and furniture but
none of its 60 small dressing props.

---

## 27. Streaming and resource residency

### 27.1 Position

The honest engineering position is: **measure before building a streaming system.**
`HOUSE-02451` measures the cold-start load time and total resident footprint with everything
loaded. The design below is complete and ready, but its *implementation* tasks are gated on that
measurement exceeding 4 s cold start or 550 MB GPU / 1.6 GB RSS.

### 27.2 Packs

Content is partitioned into packs from day one, because the Web build needs them regardless:

| Pack | Contents | Est. size |
|---|---|---|
| `core` | fonts, HUD, shared materials, player avatar, sky, weather, effects | 55 MB |
| `exterior` | terrain, road, fences, garden, shed, vehicles, vegetation LOD0/1, **the house's own roofs and chimney** | 90 MB |
| `neighbourhood` | LOD1/LOD2 houses, impostors, distant vegetation, **the neighbours' roofs and chimneys, which are part of their house assets** | 45 MB |
| `house-l0` | L0 shell, lightmaps, props, sunroom, garage, porch | 110 MB |
| `house-l1` | L1 shell, lightmaps, props, balconies | 75 MB |
| `house-l2` | L2 shell, lightmaps, props | 70 MB |
| `house-b1` | B1 shell, lightmaps, props | 55 MB |
| `house-l3` | L3 shell, lightmaps, props | 40 MB |
| `pets` | dog, cat, their clips and sounds | 22 MB |
| `audio-core` | footsteps, interaction sounds, UI | 30 MB |
| `audio-ambience` | room tones, weather, exterior ambience | 55 MB |
| `video` | television media | 25 MB |

**The player's own roof and chimney are `exterior`, not `neighbourhood`** (`HOUSE-00494`, owner
decision 2026-09-10). `ROOF_MAIN`, `ROOF_GARAGE` and `CHIMNEY` are generated shell files that name
no cell, and until this decision `build_chunks.py` filed them under the largest outdoor cell —
`EXT_WORLD`, whose pack is `neighbourhood`. §27.3's T3 loads `exterior` and *then*
`neighbourhood`, so the house's own roof arrived after the distant houses did and was demoted with
them. **The house is visible from the road the player starts on, so its complete exterior
silhouette cannot depend on whether neighbourhood content has loaded.**

Placing them by where they *stand* does not answer it either, and the measurement is why: a roof's
plan box is the whole house and its overlap with the yards is the 0.15 m eaves oversail — 0.8 m²
against `EXT_SIDEYARD_W` and 0.8 m² against `EXT_BACKYARD`, a tie decided by rounding — while the
chimney overlaps no yard at all. So the answer is categorical rather than geometric: the property's
own outdoors, which is the largest exterior cell in the `exterior` pack. Which of them is
residency-neutral, because every exterior cell of this property is in that one pack. The
neighbours' roofs are part of their house assets and stay in `neighbourhood`; nothing is drawn
twice, because there is exactly one `ROOF_MAIN` and it is filed once.

**Platform content profiles.** A pack is built once per **content profile** — `linux`, `web`,
`android` — and the profile, not the running renderer, decides which representation of an asset is
packaged. Compressed (DXT/DXT1) variants are produced **offline** by `cna-content` alongside the
uncompressed ones; the profile's manifest names exactly one representation per asset, and the
uncompressed variant stays available as the documented fallback wherever a profile does not permit
compression. Texture sizes, LOD counts and audio bit depth are chosen the same way. The runtime
loads whatever the manifest names through ordinary `ContentManager::Load<T>()` and **never asks
the renderer what it supports** — a profile is validated once, offline, by the Phase-1 probes and
recorded in `docs/cna-capability-report.md`. Linux, Web and Android differences are absorbed here,
which is why no runtime detection is needed for them.

**The `linux` profile, as measured by phase 1 (2026-09-06).** Every value below came from a probe,
not from a feature matrix:

| Decision | Measured value | Probe |
|---|---|---|
| Block compression | **`Dxt1` and `Dxt5` work and stay compressed to the GPU** — 8× and 4× — but **only through `.xnb`** | `HOUSE-00111` |
| Compressed texture container | **`.xnb`**, not `.cnb`. CNB texture schema 1 is frozen to `Rgba8`; a `textureFormat` asking for DXT is **warned about and silently kept uncompressed** | `HOUSE-00111` |
| Anisotropic filtering | **enabled** — 1.47× more far-field contrast than trilinear on a grazing floor | `HOUSE-00109` |
| Texture formats available | `Color`, `Bgr565`, `Bgra5551`, `Bgra4444`, `Dxt1`, `Dxt3`, `Dxt5`, `NormalizedByte2`, `NormalizedByte4` — identical at both graphics profiles | `HOUSE-00110` |
| Render-target formats available | `Color`, `Single`, `Vector2`, `Vector4`, `HalfSingle`, `HalfVector2`, `HalfVector4` — **only `Color` is in both lists** | `HOUSE-00110` |
| Per-frame texture promotion | **≈1 MiB (2.47 ms)**, not 4 MiB (9.30 ms). Upload is linear and bandwidth-bound at ~430 MiB/s, so a large texture is spread over frames | `HOUSE-00107` |
| Audio source format | 16-bit PCM; 24-bit sources are converted by the pipeline **bit-identically to ffmpeg**, so the offline step is provenance, not format | `HOUSE-00068`, `HOUSE-00069` |

The `.xnb`-for-textures decision interacts with a second measured fact: **`.xnb` wins the content
resolution order** (`HOUSE-00064`). A deliberately mixed output tree — models and audio as `.cnb`,
textures as `.xnb` — is therefore coherent, but an *accidental* one silently shadows whatever the
build just produced. The pipeline task (`HOUSE-00126`) gates on it.

### 27.3 Residency tiers

| Tier | Rule | Contents |
|---|---|---|
| T0 — pinned | always | `core`, `audio-core` |
| T1 — level | the player's level ± the levels reachable through an open stair-well portal | the matching `house-*` pack |
| T2 — proximity | cells within 2 portal hops of the camera cell | those cells' `dressing`/`micro` sets |
| T3 — exterior | while the camera cell is exterior, or any exterior portal is visible | `exterior`, then `neighbourhood` |
| T4 — on demand | first use | `video`, `pets` (pinned once loaded — they are small and always present) |

### 27.4 Mechanism

* A `ResidencyRequest` set is recomputed whenever the camera changes cell.
* Promotions happen at **zone-transition boundaries** — the frame the player crosses a portal —
  with a per-frame budget of **4 MB of GPU uploads** so a transition never stalls more than one
  frame's worth.
* Demotions are LRU with a 20-second grace period, so walking back and forth through a door does
  not thrash.
* On Emscripten, packs are `--preload-file` bundles fetched by the loader screen; T2/T3
  promotions inside a preloaded pack are free (they are already in memory) and only the GPU upload
  is budgeted.
* **No background thread in v1** (§8.4).

### 27.5 Failure behaviour

If a promotion cannot complete (out of memory, missing file), the cell renders with its
`essential` set and a diagnostic is logged once per asset. The game never crashes and never shows
a missing-asset hole: every material has a fallback (a flat mid-grey with the material's tint) and
every model has a fallback (a correctly-sized grey box). In a development build the fallback is
magenta and the frame counter turns red.

---

## 28. Lighting

### 28.1 The model

Lighting is a **per-room state** plus a **per-object approximation**. There is no global
illumination solver at runtime; there is a bake, a small analytic daylight model, and three
directional lights per dynamic object. That is what XNA 4.0's stock effects offer, and used
carefully it is enough for a convincing house.

```
LightingSystem, once per frame:
  for each cell:
     artificial[g] = switchState(g) · fixtureDimmer(g)            for g in cell.lightGroups
     daylight      = DaylightModel(cell, sun, moon, sky, weather) ∈ [0,1]
     ambientColor  = mix(paintTint · artificialColor, skyColor, daylightWeight) · exposureScale
     dominantDir   = argmax over (sun through windows, brightest fixture)
     exposureTarget= f(daylight, artificial, cell.kind)
```

### 28.2 Light definitions

Every light is a row in `layout.lights.json` (§15.5) with a stable ID, cell, type, transform,
colour temperature, intensity, range, cone, fixture prop, switch group and default state. **220
fixtures in 128 switch groups inside the house**, measured after `HOUSE-00381` and `HOUSE-00382`
authored them, plus the exterior lights `HOUSE-00383` adds. This paragraph and §53 both said "169
fixtures in 84 switch groups"; §13's per-room `Lights` column, which §13.1 says counts groups,
totals 128 on its own and 84 was never reachable from it. The per-room column wins, because it
names the room the switch is in. Colour temperature is converted to RGB through a Planckian
lookup table, so a 2700 K bedroom lamp and a 4000 K garage fluorescent genuinely differ. The
runtime table covers the validated 1000–12000 K range in 100 K steps and linearly interpolates;
the result is normalised display RGB, while lumens remain the separate intensity. Mixed fixtures
within a group, and mixed active groups within a cell, are therefore colour-weighted by lumens
rather than by fixture count. This is `HOUSE-01255`'s boundary: paint tint, sky colour and exposure
assemble `ambientColor` later, but they consume this one artificial-light colour rather than
converting `colorK` again.

`HOUSE-03401` adds one data row per existing light group to the same file; it selects one of the
seven bounded schedule classes documented in `docs/furnishing-kit.md`. `LightingSystem` evaluates
that row from the shared sun and civil clock with a stable group-id offset, then feeds the existing
switch-group state and baked-atlas path. The command-line light flags set a persistent override.
No switch interaction, prompt, second clock or schedule manager is introduced.

### 28.3 Baked lighting (static)

Per cell, per light group, a diffuse+indirect lightmap; plus one `LM_DAY` lightmap lit by a
uniform sky through that cell's windows. Combined at runtime by additive passes (§23.3) or a
single Tier-E pass (§23.4).

Why bake at all, given the lights switch? Because the *shape* of a room's lighting — where the
pendant's pool of light falls, how the light spills into the corner, how the wall behind a lamp
glows — is fixed even though its *intensity* is not. Baking captures the shape; the runtime scales
it. This is the technique lightmapped games used for twenty years and it is a perfect fit for
`DualTextureEffect`, which XNA shipped *specifically* for lightmapping.

> Implemented cross-cell receiver rule (2026-09-16, `HOUSE-01281`): a fixed light may optionally
> name additional fixed `bakeCells` without changing its owning `cell`. The offline baker includes
> it in those receiver atlases, while runtime room state and control ownership remain with the
> source cell. On an outside-facing skin, Tier S rejects the owning room's artificial groups and
> accepts only such explicitly baked foreign groups, scaled by their global live envelope. This
> lets the porch lanterns illuminate the foyer-owned facade without making the foyer switch or
> exposure state claim those lamps, duplicating geometry, or introducing a custom shader.
> `HOUSE-00932` applies the same authored boundary to non-baked weather-facing door detail. The
> runtime derives a separate stock-`BasicEffect` candidate set only from foreign groups already
> bound to that receiver cell, so the panelled foyer entry follows the two porch lanterns at night
> while ordinary foyer objects and merely adjacent rooms cannot acquire them. During daylight,
> weather-facing door and window detail instead uses the outdoor sky/celestial term, matching the
> facade rather than the owning room's indirect level.
>
> `HOUSE-01282` adds the corresponding explicit `spillCells` boundary for fixed unbaked detail.
> Only the static `BasicEffect` path admits those range-bounded sources; cell light state,
> exposure, switches and ordinary dynamic-object assignment remain local. The first use is the
> `EXT_WALK` front stair receiving the adjacent porch lanterns without automatically enabling its
> independently switched path-light group.
>
> `HOUSE-01286` replaces the formal living room's four bare near-ceiling points with linked
> physical semi-flush practicals. Each stable source now originates at the lower opal diffuser,
> uses a 1,200 lm / 3,000 K broad 72/140-degree downward spot and starts on in normal play. The
> selected-cell rebake removes the old 10.5507 near-ceiling singularity: the main atlas peak is
> 0.1995 while mean useful irradiance rises from 0.0110 to 0.0323. The stock-XNA runtime,
> exposure and room-state model are unchanged. Repeatable review-only `--light-on` and
> `--light-off` overrides provide matched controls without rewriting canonical switch defaults.
>
> `HOUSE-01287` replaces the displaced bare front-balcony point with the approved physical
> bronze/opal wall lantern above its real door. The stable manual group starts on for the selected
> playable arrival state and drives a 600 lm / 2,700 K, 60/100-degree downward spot from the
> fixture's optical centre. Only `L1_LANDING` is named as a foreign fixed receiver: its selected
> atlas peaks at 2.9429 with mean 0.000702, while the global 683 lm/W calibration preserves the
> pre-existing landing groups. No L2 receiver, global exposure change or renderer path is added.
>
> `HOUSE-01288` gives the four stable front-walk sources real 200 x 544 x 200 mm low-voltage
> bollards at their unchanged alternating positions. Each linked `BollardShade` is the optical
> centre of a 180 lm / 2,700 K downward 70/120-degree spot with 3.2 m range. The selected normal
> arrival starts their existing group on, but `SWITCH_EXT_WALK` remains the independent manual
> control and persistence owner; the group is not converted to dusk automation. The fixtures are
> deterministic project-authored content, preserve the clear circulation lane and add no fake
> terrain illumination, exposure change, cross-cell receiver or renderer path.
>
> `HOUSE-01289` adds four deterministic 200 x 259 x 200 mm bronze/lens landscape uplights inside
> the two existing front foundation beds. Their linked optical centres drive a separate
> dusk-owned group of 2,400 lm / 3,000 K upward 24/44-degree spots with 9 m range. Each source
> names exactly one front L1 and one front L2 shell as a fixed foreign receiver, for eight selected
> 256-sample products total; L1 peaks at 0.3271–0.3465 and L2 at 0.0640–0.1562 under the explicit
> per-source 5 lm-per-radiant-watt offline calibration. This calibration is an artistic conversion
> inside the bake tool, not fixture efficacy, and leaves the global 683 value untouched. No L0
> receiver, global exposure change, renderer path or manual-circuit ownership is added.
>
> `HOUSE-01290` physicalises the one foyer and two hall main sources with the same approved
> semi-flush practical at the existing source positions. Their optical centres now sit in the
> opal diffuser plane at 3.12 m below the 3.30 m ceiling; the former points are broad downward
> 72/140-degree spots linked to the fixture's `FamilyCeilingDiffuser` slot. The 1,100 lm foyer
> source uses a source-local 13.5 lm-per-radiant-watt bake calibration and the two 900 lm hall
> sources use 18.75, preserving their established peaks at 0.4852 and 0.5133 while distributing
> useful mean irradiance to 0.0686 and 0.1155. These are offline artistic conversions only: the
> 2,700 K groups, ranges, switches, default-on state, stock-XNA runtime and exposure are unchanged.
>
> `HOUSE-01291` physicalises the two rear-terrace sources with approved 232-triangle wall
> lanterns at x = ±2.50 m on the sunroom's rear wall. Each stable point originates from its linked
> `LanternShade` at y = 2.75 m / z = -32.23 m and emits 1,600 lm at 2,700 K over 8.5 m. The group
> follows its one dusk-sensor owner; its obsolete wall plate is removed, and both validators reject
> any future group that mixes dusk automation with a wall switch. `L0_SUNROOM` is the only named
> fixed receiver and `EXT_BACKYARD` the only unbaked-detail spill cell. The selected atlas uses a
> restrained 0.03 source calibration and measures 0.778957 peak / 0.000364078 mean, retaining a
> local facade/terrace cue rather than lifting exposure across the rear elevation.
>
> `HOUSE-01068` replaces `L0_SUNROOM`'s four bare 700 lm ceiling points with linked approved
> semi-flush fixtures and broad 72/140-degree 1,200 lm / 3,000 K spots. Two linked 350 lm /
> 3,000 K pucks add a separately switched 50/100-degree bar-task group. Both manual groups start
> on in the selected normal-play state and are owned by the room's existing two-gang plate. Their
> selected 256-sample artificial atlases use the explicit 100 lm-per-radiant-watt offline
> calibration and peak at 0.1715 (main) and 0.1988 (bar). Exterior dusk ownership, daylight,
> exposure and runtime renderer paths do not change.
>
> `HOUSE-01292` measures a clear-day anomaly: switching both sunroom groups off makes the sage
> wall brighter because the full-on group fraction drives camera exposure to 1 despite their
> original artificial atlas mean of only 0.02946. Rather than change adaptation for every room,
> the same six physical fixtures retain their 5,500 lm and use a source-local 25 instead of
> 100 lm-per-radiant-watt offline calibration. The selected main/bar means rise to
> 0.11499/0.011037 and peaks to 0.67001/0.79517, with coherent warm daytime and night room
> views. The 0.778957 rear-terrace product and all other cells, switches, daylight scale and
> XNA-only runtime remain unchanged. This does not claim to solve the wider exposure-bake
> mismatch or garden ground lighting.
>
> `HOUSE-01293` makes the four existing 1,000 lm / 3,000 K kitchen-main spots physical:
> approved 636-triangle semi-flush fixtures put the optical centres at y = 3.12 m against the
> 3.30 m ceiling and link each source to its own switched `FamilyCeilingDiffuser`. The 256-sample
> `L0_KITCHEN` main atlas uses a source-local 40 lm-per-radiant-watt offline conversion, increasing
> mean/peak 0.02838/0.16987 to 0.07021/0.41957; the independent island, sink and under-cab
> groups retain their established 100 conversion, source lumens and switch state. The measured
> static batch grows from 20 to 22 material roles, not four separate roles per fixture.
> Camera-facing fixture-glow billboards are now suppressed when the eye lies above the owning
> storey's ceiling plus structural deck or below its floor minus structural depth: the actual
> emissive mesh remains depth-tested and the kitchen lightmap remains cell-local. This prevents
> a visible false halo on the master-bedroom floor above, without changing gameplay light or
> portal/collision ownership. All of this remains in the strict stock-XNA rendering path.

### 28.4 Daylight through windows

For each cell with windows, once per second (and immediately on a weather or door/window event):

```
daylight(cell) = Σ_w  window_w.area
                     · transmission(w)                      // glass 0.86, curtain 0.25, blind 0.05
                     · openBoost(w)                          // 1.15 when open
                     · skyExposure(w.orientation, sunAlt, sunAz, cloudCover)
                     · shadingFactor(w)                      // baked: eaves, porch roof, neighbours
                 / cell.floorArea
```

`skyExposure` is a small analytic function: a diffuse sky term proportional to
`max(0, sinh(sunAltitude))` modulated by cloud cover, plus a direct-sun term that is non-zero only
when the sun's azimuth is within ±75° of the window's outward normal and the sun is above the
horizon, multiplied by `(1 − cloudCover)³`.

`shadingFactor` is precomputed offline per window per (sun altitude, azimuth) on a 12 × 24 grid by
ray-casting against the house and neighbour geometry in Blender — so the porch roof genuinely
keeps the sun out of the foyer in the afternoon, and the west neighbour's gable shades the study at
sunset. 81 windows × 288 samples × 1 byte = 23 KB. Cheap, and it is the single detail that makes
interior daylight feel real.

**Light through open doors**: a cell with no windows still receives light from a neighbouring lit
cell. Rather than an expensive propagation, `LightingSystem` runs a **2-hop flood** over open
portals: `borrowed(cell) += Σ_p aperture(p) · portalArea(p)/cellArea · 0.30 · brightness(other)`,
capped at 0.35 of the source. This makes a dark neighbour genuinely brighten when a door opens. In
the authored house, switching the kitchen light brightens the hall through their permanently open
cased opening, while opening the kitchen's actual opaque door brightens the pantry. Both effects
use the portal graph we already have.

### 28.5 Dynamic object lighting

`BasicEffect`/`SkinnedEffect` give exactly three directional lights plus ambient. Assignment,
recomputed per object per frame (it is three normalises and a few dot products):

| Slot | Source |
|---|---|
| `DirectionalLight0` | **Key** — the sun if the object's cell has daylight ≥ 0.15, the moon for a sky-open exterior at night, else the brightest fixture in range, as a directional approximation `normalize(object.centre − light.position)` attenuated by `1/(1 + (d/range)²)` and, for a spot, its authored feathered cone |
| `DirectionalLight1` | **Fill** — the second-brightest fixture, or the daylight direction from the strongest window |
| `DirectionalLight2` | **Bounce** — `−(key + fill)` normalised, at 0.18 intensity, tinted by the room's dominant surface colour |
| `AmbientLightColor` | the cell's `ambientColor` |

Fixtures are approximated as directional because that is what the effect offers. The
approximation is good when the object is small relative to the light's distance, which is almost
always true. Tier E's `RoomLit.fx` uses genuine point lights with attenuation for up to four
fixtures and drops the approximation.

> Implemented through the stock-effect boundary by `HOUSE-01261`. `LightingSystem` publishes a
> stable key/fill/bounce assignment without allocating during a draw: celestial key plus the
> strongest-window fill by day, or the two brightest live fixtures by night, followed by the
> opposite 0.18 surface-tinted bounce. `MaterialBinder` writes all three slots to both
> `BasicEffect` and `SkinnedEffect` and explicitly disables absent slots on every shared-effect
> bind. `HOUSE-01262` completes the spatial half: positional fixtures aim from their canonical
> source to the submitted world-space bounds centre, rank after the authored range-bounded
> `1/(1+(d/range)^2)` falloff and disappear outside range. Static Basic runs submit the union
> centre of their resident chunks, so this contract is exercised by today's house as well as later
> dynamic actors; genuinely directional sources retain their authored direction. Candidate lists,
> positions and ranges are built once and assignment remains allocation-free during drawing.
> `HOUSE-01047` completes the existing spot contract at this same stock-effect boundary. The
> canonical inner/outer values are full cone angles, so their half-angle cosines bound a smooth
> feather before distance-ranked assignment. A spot therefore cannot illuminate fixed detail
> behind or beside its reflector as if it were a point light.

### 28.6 Emissive surfaces

A lit lamp's shade, the TV screen, the fridge interior, appliance LEDs and the fireplace embers
are drawn with `BasicEffect { LightingEnabled = false; DiffuseColor = emissiveColour; }` plus, for
the strongest ones, an additive camera-facing **glow quad** whose size and alpha follow the
emitter's intensity and the camera's exposure. The glow quad is what sells a bulb at night; it is
one quad, alpha-blended additively, and it is also what a lens flare is made of, so the code is
shared with §32.4.

> Implemented 2026-09-16 by `HOUSE-01260`. Tier S appends linked physical-fixture glows to the
> existing transparent pass with stock `BasicEffect`, `BlendState::Additive` and read-only depth.
> A generated radial texture and static quad are allocated lazily once; luminous flux, the live
> bulb envelope and adapted camera exposure determine radius/alpha. An authored light without both
> `fixtureProp` and `emissiveMaterialSlot` is deliberately not drawn, so incomplete canonical light
> data cannot appear as floating debug orbs. The presentation quad clears its physical shade toward
> the eye while the canonical light point and baked receiver stay fixed. The pure radial profile is
> the shared boundary that §32.4's later flare sprites consume; no CNAEXT or custom shader path was
> introduced.

### 28.7 Lightning

A lightning strike raises `daylight` for every cell with an exterior window to 1.0 for 90–160 ms,
sets `DirectionalLight0` on every dynamic object to a strong white light from the strike's
azimuth, and draws a full-screen additive white `SpriteBatch` quad at an alpha that follows the
flash envelope (a 3-lobed decay). Because the daylight term feeds the additive `LM_DAY` pass,
**rooms genuinely light up through their windows** — including rooms the player is not in, whose
light then spills through open doors by §28.4's flood. That is the effect worth having.

---

## 29. Shadows

### 29.1 Tier S — blob and baked

| Shadow | Technique |
|---|---|
| Static objects | **baked into the lightmap.** Free, correct, soft. |
| Dynamic objects (player, pets, doors, movable props) | **Blob shadow**: a soft dark ellipse projected onto the supporting surface along the dominant light direction, drawn as one alpha-blended quad per object with `BasicEffect { LightingEnabled=false }` and `DepthStencilState::DepthRead`. Radius and opacity follow the object's footprint, its height above the surface, and the dominant light's intensity. |
| Outdoors in direct sun | The blob is elongated along the sun's ground-projected direction and sheared by the sun's altitude, so at low sun the shadow stretches — a cheap trick that reads surprisingly well. |
| Under overcast | Blobs shrink and soften to a near-uniform ambient occlusion contact patch. |

Blob shadows are not a placeholder; used carefully they are the correct choice for a stock-effect
tier. What they cannot do is cast a chair's shape onto a wall, or the window frame's shape onto
the floor — and the second of those is the one that matters most in a house.

So Tier S adds one more thing: **baked window "sun patch" decals**. For each window, offline, we
precompute the polygon the sun casts through it onto the room's floor and walls, for the same
12 × 24 (altitude, azimuth) grid used by `shadingFactor`. At runtime the two nearest grid entries
are interpolated and the patch is drawn as an additive, softly-edged quad-strip tinted by the sun
colour and scaled by `(1 − cloudCover)³`. Moving sun patches crossing a bedroom floor through the
afternoon are one of the strongest "this place is real" cues available, and this gets them within
Tier S.

### 29.2 Tier E — a real shadow map

One directional shadow map for the sun (or moon at night), 2048², `SurfaceFormat::Single` if the
BL-09 probe confirms it, else RGBA8-packed depth.

* The light's orthographic frustum is fitted every frame to the **visible** portion of the view
  frustum: take `BoundingFrustum::GetCorners()` of a shortened view frustum (near 0.1, far 45 m),
  transform into light space, `BoundingBox::CreateFromPoints`. This is exactly SAMPLE-038's proven
  technique.
* Texel snapping (round the light-space origin to whole texels) removes shimmer.
* Casters: dynamic objects, plus static objects within 45 m that are marked `castsShadow`.
  The architectural shell and solid static furniture (`collision: proxy`) cast into the offline
  lightmaps; small dressing props do not. The runtime shadow-map path still excludes small static
  dressing.
* Receivers: everything drawn with `RoomLit.fx`/`SkinLit.fx`.
* Filtering: 3 × 3 PCF, 4 taps at ps_3_0 cost.
* **Indoors the shadow map is disabled** unless the cell's `daylight ≥ 0.25`, because indoor
  shadows come from fixtures, which a single directional map cannot represent. Indoor Tier E uses
  the same blob shadows as Tier S for fixture-cast shadows, plus per-fixture contact darkening.

One map, one extra pass, ≤ 500 casters. Measured in `HOUSE-02712`.

### 29.3 What is explicitly not attempted

Cascaded shadow maps, shadow-mapped point lights (six faces × 220 fixtures is absurd), soft-shadow
penumbra estimation, contact-hardening, screen-space shadows, and any technique requiring a stencil
buffer (BL-02) or MRT (BL-03).

---

## 30. Interior illumination

This section states the *behaviour* the player should observe, because that is what the brief
asks for and it is testable.

| Situation | Required observable behaviour | Mechanism |
|---|---|---|
| A windowless room (`B1_CINEMA`, `L0_PANTRY`, closets) with its light off, at any time of day | Very dark. Silhouettes only. Opening the door admits a visible wedge of light from the corridor. | `ambientFloor` 0.025; 2-hop light flood through the open portal (§28.4) |
| The same room, light on | Fully lit, with the fixture's pool of light visibly centred under it and the shade glowing | Baked `LM_ART` × switch level; emissive shade |
| A south-facing bedroom at 13:00, clear sky, light off | Bright; a sharp sun patch on the floor moving over the afternoon; the ceiling lit by bounce | `LM_DAY` × daylight; sun-patch decals |
| The same room at 13:00, overcast | Evenly lit, cool, flat, no sun patch | `daylight` reduced; direct term × (1−cover)³ = ~0 |
| The same room at 02:00, full moon, light off | Just navigable; blue-grey; a faint moon patch | Moon drives the same path at 1/400 the intensity, with a strong blue shift |
| The same room at 02:00, new moon, overcast | Effectively black except for the alarm-clock LED | ambientFloor only |
| Turning on a light in a room the player is not in, seen through an open door | The far room brightens and light spills into the corridor | Portal-visible cell renders with its new level; flood updates the corridor |
| Standing outside at night looking at a lit window | The window glows; the room behind it is dimly visible through the glass | Emissive window-glow card + depth-1 portal traversal into the room |
| A thunderstorm at night | Rooms with exterior windows flash white; the flash spills through open doors; thunder follows | §28.7 |
| The fridge door opens in a dark kitchen | The interior light turns on, the fridge interior becomes visible (its portal opens) and a cold wedge of light falls on the floor | Fridge interior is a cell behind a portal; interior light is a light group; a sun-patch-style decal quad on the floor |
| The TV is on in a dark family room | The room flickers with the video's average colour | Per-frame average of the video texture (a 4 × 4 downsample every 6 frames) drives a light group's colour and intensity |

Each row above has a corresponding integration test (§70.2) that asserts a measurable proxy —
mean luminance of a screenshot region, or the computed `RoomLightState` — rather than a human
judgement.

---

## 31. Sky

### 31.1 Geometry

A **sky dome**: a 32 × 18 lat-long hemisphere of radius 900, plus a ground-plane skirt disc so the
horizon is never a hard edge. Drawn first, `DepthStencilState::None` (no test, no write),
`RasterizerState::CullNone`, translated to the camera's position each frame so it can never be
reached.

`HOUSE-01643` makes `SkySystem` the one owner of `Pass::Sky`: it draws the generated dome first and
then the already-existing celestial overlays (currently the sun disc). This composition matters
because `Renderer::Install` deliberately has one slot per pass; installing the dome and sun as two
unrelated passes would replace one with the other. The `CSKY` reader validates the fixed v1 header,
file length, finite positions and every index before any graphics resource is created.

### 31.2 Tier S colouring

The generated mesh has 610 vertices: 577 on a seam-wrapped, single-pole hemisphere, 32 on the
bottom skirt ring and one disc centre. They become `VertexPositionColor` at runtime. Their colours
are recomputed on the CPU
whenever the sky state changes materially (sun altitude by > 0.25°, cloud cover by > 0.01, i.e.
a few times per simulated minute) and uploaded with `VertexBuffer::SetData`. The colour of a dome
vertex is:

```
zenithColour  = lut_zenith [sunAltitude, cloudCover]
horizonColour = lut_horizon[sunAltitude, cloudCover, angleFromSunAzimuth]
c = lerp(horizonColour, zenithColour, smoothstep(0, 1, vertexAltitudeFraction))
c = mix(c, overcastGrey, cloudCover^1.5)
c += sunGlowTerm(angleToSun) · (1 − cloudCover)²
c = mix(c, nightColour(moonAltitude, moonPhase), nightWeight(sunAltitude))
```

The conceptual LUTs are 32 × 8 (and 32 × 8 × 16) tables of RGB. `layout.sky.json` stores their
compact, exactly reproducible form: 32 clear-sky zenith/horizon rows generated offline from eleven
art-directed anchors, plus the overcast mix and sun-glow parameters that supply the cloud-cover and
azimuth axes. `tools/world/sky_lut.py --selftest` expands and checks all 4096 horizon samples. This
is the deliberate `HOUSE-00394` representation: evaluating the two simple extra axes while the
vertices are already being recomputed avoids 4064 redundant authored rows without adding an
analytic atmospheric model to runtime. Sunrise/sunset warmth is concentrated around the sun rather
than tinting every azimuth orange, and the same elevation curve naturally treats dawn and dusk
identically. The later reference-photo pass (`HOUSE-01652`) art-directs these same anchors.

`HOUSE-01644` implements the altitude interpolation, altitude-fraction gradient and overcast mix
above. It retains the 610 expanded vertices and calls `SetData` on the existing buffer only after
one of the material-change thresholds is crossed. A 64-update debug-build measurement averaged
0.105 ms and reached 0.121 ms maximum.

`HOUSE-01645` completes the two formerly symbolic terms. For each vertex,
`sunGlowTerm = sunGlowColor · sunGlowStrength · sunIntensity · max(0, dot(vertexDirection,
directionToSun))^sunGlowExponent`; `sunIntensity` is interpolated from `layout.sky.json`'s authored
sun curve, and the existing `(1 − cloudCover)²` factor then extinguishes it under overcast. The
night weight is `1 − TwilightAmbientFactor(sunAltitude)`: zero through civil twilight at −6°,
smoothly rising, and one at astronomical night −18°. Its target is the −18° gradient at the same
vertex altitude plus `MoonShadingFor(moon, phase, cloudCover)`, so the already-shared phase curve,
geometric-horizon rule and cloud attenuation cannot disagree with moonlight. The sky refreshes for
more than 1° of solar azimuth or lunar altitude and more than 1/128 phase illumination as well as
the existing altitude/cover thresholds. `CnaHouseGame` supplies all four celestial values from the
single per-frame `LightingSystem` evaluation. With all terms active, 64 forced debug-build updates
averaged 0.168 ms and reached 0.232 ms maximum.

`HOUSE-01651` also makes the non-directional part of this colour model the one source for room
lighting. A uniform hemispherical sky is the solid-angle mean of the altitude gradient: solid
angle is uniform in `sin(altitude)`, which is the dome's altitude fraction, and the mean of the
gradient's smoothstep is exactly 0.5. The resulting live colour includes the same altitude rows,
overcast mix and night/moon blend as the dome. It deliberately excludes the localized sun-glow
lobe and town light pollution; neither is uniform illumination in the baked `LM_DAY` map. For an
interior cell, `LightingSystem` publishes both the sky ambient contribution and §23.3's complete
`LM_DAY` diffuse tint as `skyColor · daylightLevel`; a sky-open exterior receives the unscaled sky
ambient. Paint tint, the ambient floor's draw and exposure scaling remain the separately owned
`HOUSE-01257`, `HOUSE-01264` and `HOUSE-01266` work.

610 vertices × 16 bytes = 9.8 KB per update. Negligible. The earlier 594-vertex estimate was
`33 × 18`: it duplicated each seam vertex, omitted the skirt and, if used for the pole, produced
32 zero-area triangles. `HOUSE-01642` records the generated non-degenerate topology in
`docs/sky-dome-format.md`.

### 31.3 Clouds

Three concentric dome rings at radii 880, 860 and 830, each a 48 × 8 strip with a scrolling cloud
texture, alpha-blended, `LightingEnabled = false`, `VertexColorEnabled = true` (the vertex colour
carries the same sky tint so clouds pick up sunset colour correctly).

| Layer | Texture | Scroll rate | Purpose |
|---|---|---|---|
| High | `cloud_cirrus` (1024², alpha) | 0.15 × wind | Wisps in clear/mostly-clear weather |
| Mid | `cloud_cumulus` (1024², alpha) | 0.60 × wind | The main cloud body |
| Low | `cloud_stratus` (1024², alpha) | 1.00 × wind | Overcast and storm |

`HOUSE-01646` makes these project-owned, deterministic RGBA source textures with
`tools/world/cloud_textures.py`: periodic multi-octave value noise gives every texture continuous
wrap-around edges, while separate anisotropy and density thresholds make cirrus sparse and
wind-stretched, cumulus broken into cloud bodies, and stratus substantially closed. RGB is white,
not pre-lit, because `HOUSE-01649` supplies the live sky tint; the graded alpha is the cloud
density. The content compiler premultiplies alpha and generates the mip chain offline.

`HOUSE-01647` draws the three authored radii as 48 × 8 hemispherical shells. Each uses 440
`VertexPositionColorTexture` vertices and 720 non-degenerate triangles: the top is a 48-triangle
fan with one pole carrier per wedge, followed by seven quad strips down to the horizon. Planar XZ
UVs repeat every 240 world metres and use `SamplerState::LinearWrap`; this both avoids a longitude
seam and makes the scroll a real horizontal direction. The meteorological direction names where
the wind comes *from*, so the sampling offset runs opposite the air's travel and the visible
texture feature moves with the wind. Offsets stay bounded to one repeat while their rates retain
the authored 0.15/0.60/1.00 ratios. The stock XNA `BasicEffect` draws the outer shell first under
`AlphaBlend`, no depth and two-sided rasterisation, after the sun overlay, for three draws and 2,160
triangles total; only the three small UV vertex buffers change while the textures, indices and
effects are retained.

Cloud state maps to layer alphas by a table:

| Sky state | `cloudCover` | High α | Mid α | Low α | Mid tint |
|---|---|---|---|---|---|
| clear | 0.00–0.10 | 0.15 | 0.00 | 0.00 | — |
| mostly clear | 0.10–0.30 | 0.35 | 0.20 | 0.00 | white |
| partly cloudy | 0.30–0.60 | 0.30 | 0.60 | 0.05 | white/grey |
| cloudy | 0.60–0.85 | 0.10 | 0.85 | 0.35 | grey |
| overcast | 0.85–1.00 | 0.00 | 0.60 | 0.95 | flat grey |
| storm | 0.90–1.00 | 0.00 | 0.70 | 1.00 | dark blue-grey |

Alphas are interpolated continuously from `cloudCover` and `thunderIntensity`, so a sky filling in
over ten simulated minutes reads as a gradual thickening rather than a state swap. Cloud UV scroll
is `windDirection`-aligned, so the sky moves the way the trees bend.

`HOUSE-01648` treats each ordinary row as an anchor at the centre of its contiguous cover band,
linearly interpolates between adjacent anchors and clamps beyond the first and last centres. It
then linearly pulls that result toward the separate storm row by `thunderIntensity`; there is no
weather-state branch and both inputs are clamped to 0..1. The resulting three live alphas feed the
retained cloud effects directly, while `sky.cloud.alpha_updates` shows material state changes.

`HOUSE-01649` uses the current horizon result from the same gradient and overcast computation as
the dome as the common cloud tint. It is pale blue by day, warm at sunrise and sunset, dark blue at
night, and converges on the authored overcast grey as cover reaches one. The tint is stored in
every cloud vertex, as the Tier-S design above requires. A revision makes all three retained vertex
buffers accept a colour-only change even when the wind is still; otherwise tint uploads piggyback
on the UV upload already needed for scrolling, and they retain `HOUSE-01644`'s material-change
cadence rather than becoming per-frame work.

### 31.4 Tier E

`SkyDome.fx` replaces the vertex-colour dome with a per-pixel analytic gradient (the same LUTs
sampled as a 2-D texture), does the three cloud layers in one pass with a per-pixel blend, adds a
soft sun disc with limb darkening and an aureole, and applies dithering to kill the banding a
vertex-interpolated gradient always shows on a large flat sky.

### 31.5 Fog

`BasicEffect`/`DualTextureEffect`/`AlphaTestEffect`/`SkinnedEffect` all support fog. Fog colour = the
horizon colour in the view direction; fog start/end are driven by `fogDensity` and precipitation
intensity. Fog is disabled for interior cells (a room does not have fog) and enabled for
`EXT_WORLD` — the switch happens per draw batch, which is free because batches are already grouped
by cell.
`rendering::FogParams` is that per-draw boundary: a null `DrawParams::fog` disables stock-effect
fog, while a value carries the already-derived colour and start/end distances. The binder owns no
weather state.

---

## 32. Sun

### 32.1 Position model

A simplified NOAA solar-position algorithm, evaluated once per frame (it is ~40 flops):

```
n          = days since 2000-01-01 12:00 UT (from the simulated clock)
L          = 280.460° + 0.9856474° · n                    (mean longitude)
g          = 357.528° + 0.9856003° · n                    (mean anomaly)
λ          = L + 1.915° sin g + 0.020° sin 2g             (ecliptic longitude)
ε          = 23.439° − 0.0000004° · n                     (obliquity)
δ          = asin(sin ε · sin λ)                          (declination)
α          = atan2(cos ε · sin λ, cos λ)                  (right ascension)
GMST       = 18.697374558h + 24.06570982441908h · n
H          = GMST + longitude/15 − α                      (hour angle)
altitude   = asin(sin φ sin δ + cos φ cos δ cos H)
azimuth    = atan2(−sin H, cos φ tan δ − sin φ cos H)      (from north, clockwise)
```

Default location: **latitude 40.05° N, longitude −75.30°, UTC−5 with US DST rules** — suburban
Pennsylvania, consistent with the house's architecture. Configurable in settings; a "Reykjavík"
and an "Equator" preset exist purely to make the seasonal and diurnal extremes easy to test.

Consequences that make the world feel right: the sun rises in the north-east in June and the
south-east in December; day length runs from 9h17m to 15h03m; the noon altitude runs from 26.4° to
73.4°; sunrise and sunset colours last a realistic 20–35 simulated minutes.

### 32.2 Sunlight

`sunDirection = −(cos(alt)·sin(az), sin(alt), −cos(alt)·cos(az))` in world space (recall north is
`−Z`).

`sunColor` and `sunIntensity` come from a 64-entry LUT over altitude:

| Altitude | Colour | Relative intensity |
|---|---|---|
| −6° (civil twilight) | (0.22, 0.24, 0.38) | 0.02 |
| −0.83° (geometric sunset) | (1.00, 0.42, 0.16) | 0.10 |
| +2° | (1.00, 0.62, 0.34) | 0.35 |
| +10° | (1.00, 0.86, 0.68) | 0.72 |
| +30° | (1.00, 0.96, 0.90) | 0.94 |
| +60° | (1.00, 1.00, 0.99) | 1.00 |

Multiplied by `(1 − 0.85·cloudCover)` for the direct term and `(1 − 0.35·cloudCover)` for the
sky-diffuse term. Below civil twilight the solar sky-diffuse term smoothsteps from full at −6° to
zero at astronomical night, −18°; the separate room ambient floor remains.

### 32.3 The sun disc

A camera-facing quad at 890 units along `−sunDirection`, angular diameter 0.53°, drawn additively
with a soft radial texture, sized up by ×2.6 near the horizon (atmospheric extinction makes the
disc *look* bigger and much dimmer) and tinted by the same LUT. The apparent-size multiplier is
×2.6 at and below 0° altitude, smoothsteps to ×1 over the 0–10° warm-LUT band, and stays ×1 above
it; this makes both endpoints continuous without inventing a second atmospheric threshold.

### 32.4 Glare, and the sun-clock overlay

> **Optional after DONE since 2026-09-21: occlusion-query glare and the lens flare are not required; the sun disc of §32.3 is** ([ADR-0015](docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md))

**Visibility measurement.** A 3 × 3 grid of sample points spanning 1.2° around the sun's screen
position. Each frame, for each point that is on screen:

```cpp
device.setBlendStateProperty(BlendState::Opaque);       // colour writes off via ColorWriteChannels::None
device.setDepthStencilStateProperty(DepthStencilState::DepthRead);
query[i].Begin();
DrawTinyQuad(samplePoint[i], 2px);
query[i].End();
```

Next frame, `coverage = (#queries reporting PixelCount > 0) / 9`. On a driver with a real
`GL_SAMPLES_PASSED` the same code refines to `Σ PixelCount / (9 · quadArea)`. This is the whole
answer to BL-07: **10 quantised coverage levels on any driver, continuous coverage on a good one**,
with identical game code. The measured coverage is smoothed with a 0.25 s time constant.

**Flare.** `SpriteBatch` with `BlendState::Additive`, drawing 7 flare sprites along the line from
the sun's screen position through the screen centre at parametric offsets
`{−0.5, −0.3, 0.0, 0.3, 0.6, 0.9, 1.3}` with per-sprite colour, scale and rotation. Total alpha =
`coverage · glareStrength(sunAltitude) · (1 − 0.9·cloudCover)`. Exactly XNA's LensFlare sample
idiom, which is already proven in CNA (SAMPLE-041).

**The sun-clock overlay** — the brief's distinctive requirement:

```
cosAngle = dot(cameraForward, −sunDirection)
show  when cosAngle > cos(22°)  AND sunAltitude > −2°  AND coverage > 0.15
hide  when cosAngle < cos(30°)  OR  sunAltitude < −4°  OR  coverage < 0.05
```

The 8° hysteresis band plus a 0.35 s fade in / 0.6 s fade out means the clock never flickers, and
the 22° acceptance cone means the player does not have to aim at a pixel — the sun merely has to
be *in view, near the middle*. The overlay is a small `SpriteFont` label drawn 90 virtual units
below the sun's screen position (clamped to stay on screen), showing:

```
        14:37
   Sat 14 June 2031
```

with a subtle drop shadow so it reads against a bright sky. At night the identical mechanism
applies to the **moon** (§33.4) with a 28° cone, because "look at the moon to learn the time" is
the natural counterpart and costs nothing extra.

---

## 33. Moon

### 33.1 Position

The same simplified ephemeris family: mean longitude, mean anomaly, mean elongation, argument of
latitude, then the six largest periodic terms of the ELP truncation — enough for the moon to rise
and set in the right places, to be up at the right times relative to its phase, and to move
against the stars over simulated days. About 60 flops, evaluated once per frame.

`HOUSE-01601` implements that truncation from the J2000 fundamental arguments. It retains the six
largest longitude terms and the six largest latitude terms, rotates the resulting geocentric
ecliptic direction through the date's mean obliquity, then uses the same sidereal-time and
east-positive observer conventions as `SunModel`. The returned altitude is geometric and
geocentric. Rise/set uses +0.125° for the centre: mean lunar horizontal parallax minus mean
refraction and semidiameter, with the opposite sign from the sun's −0.8333° threshold.

The compact series was checked independently against 60 moonrise times published by the United
States Naval Observatory for §33's location and fixed UTC−5 standard time. The rows span more than
a year and more than twelve lunations; the mean absolute error is 0.31 minutes and the worst is
1.01 minutes, comfortably inside the specified ±8 minutes. The committed extract and response
cache keep normal CI offline, and `tools/ci/moontimes_table.py --fetch` reproduces them.

### 33.2 Phase

```
elongation = angle between the geocentric sun and moon directions
illuminated = (1 − cos elongation) / 2                     ∈ [0,1]
waxing = (moonEclipticLongitude − sunEclipticLongitude) mod 360° < 180°
phase = waxing ? illuminated/2 : 1 − illuminated/2         ∈ [0,1)  — 0 = new, 0.5 = full
```

`HOUSE-01602` implements the angle as the dot product of the geocentric ecliptic directions,
including the Moon's latitude rather than treating longitude separation as the whole angle. The
longitude difference still selects waxing or waning exactly as above. `MoonPhaseAt` evaluates the
sun and moon at one J2000 instant; `MoonPhaseFor` reads the compressed civil instant, so position,
illumination and the saved calendar cannot drift onto different dates.

Sixty local-noon illuminated fractions and phase directions published independently by the USNO
were sampled every six days through 2031. Against their full ephemeris, the continuous phase's mean
circular error is 0.0014 of a lunation and its worst is 0.0030; illuminated fraction differs by at
most 0.006. At exactly published 0% or 100%, waxing/waning may flip hours apart between the compact
and full ephemerides while the circular phase still agrees, so direction is asserted only away
from those physically singular endpoints.

`phase` is a **continuous 0..1 value**, not one of eight buckets. The eight named phases are a
presentation detail used only by the debug overlay and the almanac line:

| `phase` | Name |
|---|---|
| 0.000–0.035, 0.965–1.000 | New moon |
| 0.035–0.215 | Waxing crescent |
| 0.215–0.285 | First quarter |
| 0.285–0.465 | Waxing gibbous |
| 0.465–0.535 | Full moon |
| 0.535–0.715 | Waning gibbous |
| 0.715–0.785 | Last quarter |
| 0.785–0.965 | Waning crescent |

`HOUSE-01603` keeps these half-open intervals in the single allocation-free `MoonPhaseName`
function used by both presentation sites. Phase values wrap because a lunation is circular;
non-finite input fails closed to New moon instead of leaking an empty or unstable label.

### 33.3 Rendering the phase

Four implementations were considered:

| Option | Verdict |
|---|---|
| 8 discrete phase textures | Rejected — visibly steps; the brief explicitly asks for continuous |
| A lit disc plus an offset dark disc | Rejected — geometrically wrong for gibbous phases and awkward to blend |
| **CPU-generated 128 × 128 RGBA mask, uploaded when `phase` changes by > 1/128** | **Chosen for Tier S.** 64 KB, regenerated a handful of times per simulated day. The terminator is an exact ellipse: for texel `(u,v)` on the unit disc, lit iff `u ≥ k·√(1−v²)` (waxing) where `k = cos(π·2·phase)`. Adds earthshine as a 4 % blue-grey fill on the unlit part, and libration by rotating the mask by the position angle of the bright limb. |
| A shader terminator in `MoonDisc.fx` | Tier E. Identical maths per pixel, no upload, plus limb darkening. |

`HOUSE-01604` implements the chosen Tier-S path in `MoonMask`. Texel centres are classified
directly against the ellipse; the waning half mirrors the inequality, pixels outside the unit disc
are transparent, and the unlit disc is opaque RGB `(8,9,10)` earthshine so it can still mask a
square albedo texture. Rotation is applied by inverse-transforming each texel into the canonical
mask, avoiding a second resampling pass. The retained mask compares circular phase distance to the
last value it actually generated: equality at `1/128` does not update, the first value beyond it
does, and a wrap through new moon cannot look like a whole-lunation jump. Its position angle is
sampled on that phase-driven update, so rotation never creates a second upload cadence.

The moon texture itself is a real 1024 × 1024 greyscale lunar albedo map, modulated by the mask.
`HOUSE-01605` prepares it from NASA SVS CGI Moon Kit's 2019 LRO colour mosaic. The reviewed
2048 × 1024 TIFF is hash-pinned, orthographically projected onto the visible near-side disc and
converted to Rec.709 greyscale by `tools/assets/moon_albedo.py`. The nearest limb texel is extended
outside the geometric disc so bilinear filtering cannot introduce a black fringe; `MoonMask`, not
the albedo, owns disc opacity. Normal builds remain offline and check the exact committed output,
while `--fetch` reproduces it from NASA. The NASA public-domain terms, source credit and both hashes
are carried by the manifest, generated credits and `assets-src/Textures/Sky/SOURCE.md`.

Disc drawn at 890 units along `−moonDirection`, angular diameter 0.52°, `BlendState::Additive`,
with the same horizon-scaling and reddening treatment as the sun.

`HOUSE-01606` implements that disc as the `MoonDiscPass` component of `SkySystem`. It projects the
sun direction into a stable world-up billboard basis to obtain the bright-limb position angle,
places a two-UV quad at the stated distance and size, and combines the compiled albedo with
`MoonMask` through stock XNA `DualTextureEffect`. Because that effect's light-map convention
multiplies by two, the pass supplies half the atmospheric tint and therefore leaves the intended
`albedo × mask × tint` product. The dynamic mask texture is uploaded only when `MoonMask` advances
past its phase threshold. Moon and sun are submitted after the opaque dome but before the three
cloud rings, so the ordinary alpha-blended cloud layer obscures both without a second cloud term.

### 33.4 Moonlight

When the sun is below −4°, `DirectionalLight0` for outdoor objects becomes the moon:

```
moonIntensity = 0.0022 · phaseIlluminationCurve(phase) · max(0, sin(moonAltitude))^0.6
                       · (1 − 0.95·cloudCover)
moonColor     = (0.62, 0.70, 1.00)
```

`phaseIlluminationCurve` is deliberately non-linear (`illum^1.8`): a half moon gives far less than
half a full moon's light, which is both physically true (opposition surge) and dramatically
useful — a new-moon night with the lights off is genuinely, usefully dark.

`HOUSE-01607` evaluates this once in `LightingSystem` beside the sun. `MoonShadingFor` clamps the
phase and cloud inputs, removes the beam at and below the geometric horizon, and returns the
normalised blue hue separately from intensity so cloud attenuation cannot be applied twice.
`CelestialKeyForCell` preserves the daytime sun, then selects the moon only when the sun is
strictly below −4°, the lunar intensity is non-zero and the cell is a sky-open exterior. Indoor
rooms keep their fixture key at night; this resolves §28.5's former shorthand against this
section's more specific outdoor-only rule. On 2031-01-06 at 19:00 the model supplies 97.2%
illumination at 39.4° altitude and a clear-sky blue-channel intensity of 0.001591; exact new moon
is zero even before the 95% overcast attenuation.

### 33.5 Lunation speed

A synodic month is 29.530 588 days. At the default 24 real minutes per simulated day, a full
lunation takes 11.8 real hours — so within one session the phase barely moves, but **across
sessions it visibly does**, because the date is saved. This is the correct default and it is
recorded as a deliberate decision (D-14, §77). Two escape hatches exist: a
`moonPhaseSpeedMultiplier` setting (1.0 default, up to 8.0) and the debug `time advance <days>`
command used by the tests.

`HOUSE-01608` implements the multiplier as a **phase-only** rate. The physical lunar position,
moonrise, sun, season and civil clock remain on the saved calendar; only the continuous phase gains
`(multiplier − 1) · elapsedCalendarDays / 29.530588` turns. Elapsed time is anchored at the new-game
vernal equinox, so changing the setting before a new game cannot change its initial moon. Both
normal frame advance and `time advance <days>` feed `SimClock::CalendarDays`, making the setting
deterministic and keeping the debug command exact even while the diurnal clock is frozen.

---

## 34. Stars

* **Catalogue**: the 1 500 brightest stars, generated offline into a compact binary
  (RA, Dec, magnitude, B−V colour index) from a public-domain catalogue (Yale BSC / Hipparcos
  subset; provenance recorded). Real constellations are recognisable, which is worth more than it
  costs.
* **Rendering**: one `DynamicVertexBuffer` of camera-facing quads, size and alpha from magnitude,
  colour from B−V through a small LUT, `BlendState::Additive`, `LightingEnabled = false`. The
  whole field is one draw call.
* **Rotation**: the field rotates with local sidereal time about the celestial pole
  (`altitude = latitude`, `azimuth = 0`, i.e. due north), so the stars wheel correctly and Polaris
  sits at the right height. Over a simulated year the visible constellations change with the
  season.
* **Visibility**: overall alpha = `starVisibility(sunAltitude) · (1 − cloudCover)^1.6 ·
  (1 − 0.55 · moonBrightness)`. `starVisibility` ramps from 0 at −4° to 1 at −14° (two degrees
  below nautical twilight; standard astronomical twilight is −18°), so stars appear gradually,
  brightest first — a Magnitude cutoff that tightens with twilight rather than a global fade, so
  faint stars vanish first exactly as they should.
* **Twinkle**: per-star `sin(t · f_i + φ_i)` amplitude scaled by `1/sin(altitude)` so stars near
  the horizon twinkle more. Computed in the same CPU pass that fills the vertex buffer, at 20 Hz.
* **Light pollution**: a faint warm dome glow toward the town, which also hides the faintest stars
  near the southern horizon. One extra gradient in the sky LUT.
* Two visible **satellites** and an occasional **meteor** (one every ~4 simulated minutes on clear
  nights) — three lines of code each, and they make the night sky feel alive. `HOUSE-01615` makes
  both effects deterministic functions of the one simulation clock. The two satellites repeat
  distinct 420 s and 660 s horizon-to-horizon tracks, fading at each reset; the meteor schedule is
  one 8 s streak per 240 simulated seconds when the sun is at or below −14° and cloud cover is at
  most 0.25. Its event number hashes to direction and slope, so save/load and frame rate cannot
  change which streak is due. All three are vertex-colour geometry in one additive XNA draw.

---

## 35. Day/night cycle

### 35.1 Clock

```cpp
struct SimClock {
    double  epochSeconds;      // seconds since 2031-01-01T00:00:00 local
    double  timeScale;         // simulated seconds per real second
    double  latitudeDeg, longitudeDeg;
    int     utcOffsetMinutes;
    bool    dstRulesUS;
};
```

`Update` accumulates `gameTime.ElapsedGameTime · timeScale`. Everything else — sun, moon, stars,
lights on a dusk sensor, weather seasonality, the pets' routine — reads the clock. The clock is
saved.

### 35.2 Day length: the analysis the brief asked for

| Real minutes / sim day | `timeScale` | 1 real second = | Sun motion | Verdict |
|---|---|---|---|---|
| 20 | 72× | 1.2 sim minutes | 0.30°/s | The original suggestion. Fine, but 1.2 minutes per second is awkward to reason about, and a 15-minute in-game hour boundary lands at 12.5 real seconds. |
| **24** | **60×** | **exactly 1 sim minute** | **0.25°/s** | **Chosen.** 1 real second = 1 simulated minute; 1 real minute = 1 simulated hour. Every debugging statement, every log line, every test that says "wait 3 seconds and check the clock advanced 3 minutes" becomes trivially readable. The sun's 0.25°/s is smooth and perceptible without being distracting: it crosses its own diameter in 2.1 real seconds. |
| 48 | 30× | 30 sim seconds | 0.125°/s | A good "slow" preset for screenshots and atmosphere. |
| 96 | 15× | 15 sim seconds | 0.06°/s | "Very slow" preset. |
| 1440 | 1× | 1 sim second | real time | Debug/realtime preset. |
| ∞ | 0× | frozen | none | Debug preset, essential for pixel-regression tests. |

**Decision: 24 real minutes per simulated day (`timeScale = 60.0`) is the default**, configurable
from the settings menu with the presets above and a free numeric entry. The brief's original 20
is retained as a preset. The reason 24 wins is not aesthetic — it is that `1 s = 1 min` removes an
entire class of arithmetic mistakes from every test, log and settings dialogue in the project.

### 35.2b The compressed year: 15.208333 simulated days

The 24-real-minute day of §35.2 settles how fast the *sun* moves. It leaves the calendar running
at one calendar day per simulated day, and that is far too slow to ever be seen: four seasons
would need 365 simulated days, which at 24 real minutes each is **146 real hours** of play. A
player would never witness autumn.

**Decision (project owner, 2026-09-06): the calendar is compressed so that one simulated day
advances the calendar by 24 calendar days.**

| Quantity | Value |
|---|---|
| Real minutes per simulated day | 24 (unchanged — `timeScale = 60`) |
| Calendar days advanced per simulated day | **24** |
| Simulated days per year | 365 / 24 = **15.208333…** |
| **Real minutes per year** | 15.208333… × 24 = **365** |
| **Real hours per year** | **6.0833…** (6 h 5 min) |
| Real minutes per season | 91.25 (1 h 31 min) |
| Starting season | **Spring** — a new game begins at the vernal equinox |

The number that matters is the last-but-two: **one full year takes 365 real minutes**, so a single
long session shows the whole house in all four seasons, while the sun still rises and sets once
every 24 real minutes rather than once a minute. The two rates are deliberately decoupled: the
*diurnal* rate is chosen for how the sun should look, the *annual* rate for how long a player
should have to wait to see winter.

The mnemonic falls out cleanly: **1 real second = 1 simulated minute; 1 real minute = 1 simulated
hour; 1 real hour ≈ 2.5 simulated days ≈ 2 simulated months.**

Two consequences are load-bearing and are why this is recorded as a decision rather than a
constant:

* **The solar declination must advance 24× faster than the hour angle.** Sunrise and sunset drift
  measurably from one simulated day to the next — which is the point: §35.3's day-length variation
  becomes visible within a session instead of being theoretical.
* **Nothing may key off an integer season.** A season boundary crossed every 91 real minutes is
  frequent enough that a stepwise change would read as a glitch. Season is therefore a
  **continuous phase**, not an enum — see §36.3.

`SimClock` gains one field, `calendarDaysPerSimDay` (default 24.0), so the compression is
configurable and can be set to 1.0 for a realistic-calendar debug run.

**A date and a time of day stop being independent** (`HOUSE-01542`). `epochSeconds` is the diurnal
clock and the date is derived from it, so a given time of day only ever falls on one date in 24:
most (date, time) pairs are instants the clock never passes through. `SimClock::SetCalendar` takes
a single continuous calendar position and is therefore exact — it is what a season, a solar
declination or a `time set` asking for "midwinter" wants — while `SetStandard`, which takes a civil
date and time, lands on the nearest instant the clock does pass through and lets `Standard()` say
where that is. Measured over 672 requests at the default rate, the worst landed **12 calendar days**
from the date asked for, which is half of `calendarDaysPerSimDay` and the most the nearest of two
candidates a rate apart can ever be. At 1.0 every request is exact.

The calendar advances **continuously** and not in a jump at simulated midnight, because §36.3
requires it: a season boundary crossed every 91 real minutes would read as a glitch if the phase
stepped. One simulated day of play therefore passes through all 24 of its calendar days in turn.

### 35.3 What the cycle drives

Sun and moon position and colour; sky dome colours and cloud tint; star visibility; outdoor
ambient and exposure; per-room daylight and hence indoor illumination; the sun-patch decals;
exterior lights (porch, street lights, neighbours' windows) on a dusk sensor with per-fixture
random offsets of ±8 simulated minutes so they do not all switch at once; the ambience bed
(dawn chorus, daytime distant traffic, evening crickets, night quiet); the pets' activity level;
weather seasonality; and the temperature curve.

The fixture owns this control as authored `duskSensor` data; a group may not mix automatic and
wall-switched fixtures. The common crossing is solar altitude -4°, and each stable fixture id maps
deterministically to one of the seventeen whole-minute offsets from -8 to +8. Tier S still owns
one combined artificial-light state/atlas per group, so the short stagger is represented by the
lumen-weighted active-fixture fraction. Full night is exactly one and daylight exactly zero; this
does not add a second per-fixture dynamic-light renderer (`HOUSE-01269`).

Interior groups use the same sun model through their authored schedule class. Living, bedroom,
wet/task and circulation classes add the small civil-time windows in `docs/furnishing-kit.md`;
closets, stores and utility rooms use `SC-OFF`. Exterior groups without per-fixture `duskSensor`
rows use `SC-DUSK`'s deterministic group offset, while the existing sensor groups retain their
finer lumen-weighted fixture stagger.

---

## 36. Weather model

### 36.1 State

```cpp
struct WeatherState {
    float cloudCover;         // 0..1
    float cloudCumuliform;    // 0 = stratiform sheet, 1 = puffy cumulus
    PrecipType precipType;    // None | Rain | Snow | Sleet | Hail
    float precipIntensity;    // 0..1
    float windSpeed;          // m/s, 0..30
    float windDirectionDeg;   // meteorological (from)
    float gustFactor;         // 0..1, multiplies short-term wind variance
    float fogDensity;         // 0..1
    float thunderIntensity;   // 0..1 — also sets the lightning rate
    float temperatureC;       // -18..38
    float humidity;           // 0..1
    float surfaceWetness;     // 0..1, integrated
    float snowDepth;          // 0..0.35 m, integrated
    uint64 rngState[4];       // xoshiro256++ — all four words are saved
};
```

Everything is continuous. There is no `isRaining` boolean anywhere in the codebase; a lint check
in CI rejects one.

### 36.2 Archetypes

**Fourteen** rows below: thirteen named archetypes and one modifier. They are *targets*, not
states; the live state always interpolates toward the current target under rate limits. (This
paragraph said "twelve" until 2026-09-08, when `HOUSE-00393` authored the table as data and
counted it.)

| ID | Name | cover | cumuli | precip | intensity | wind | gust | fog | thunder | Δtemp |
|---|---|---|---|---|---|---|---|---|---|---|
| `W_CLEAR` | Clear / calm | 0.05 | 0.6 | None | 0.00 | 1.5 | 0.10 | 0.02 | 0.00 | +2 |
| `W_MOSTLY_CLEAR` | Mostly clear | 0.22 | 0.8 | None | 0.00 | 2.5 | 0.20 | 0.03 | 0.00 | +1 |
| `W_PARTLY` | Partly cloudy | 0.45 | 0.9 | None | 0.00 | 3.5 | 0.30 | 0.04 | 0.00 | 0 |
| `W_CLOUDY` | Cloudy | 0.72 | 0.5 | None | 0.00 | 4.5 | 0.35 | 0.06 | 0.00 | −1 |
| `W_OVERCAST` | Overcast | 0.95 | 0.1 | None | 0.00 | 3.0 | 0.20 | 0.12 | 0.00 | −2 |
| `W_DRIZZLE` | Light rain | 0.92 | 0.2 | Rain | 0.22 | 4.0 | 0.30 | 0.22 | 0.00 | −2 |
| `W_RAIN` | Rain | 0.97 | 0.3 | Rain | 0.55 | 6.5 | 0.45 | 0.28 | 0.05 | −3 |
| `W_HEAVY_RAIN` | Heavy rain | 1.00 | 0.4 | Rain | 0.90 | 9.0 | 0.60 | 0.35 | 0.15 | −4 |
| `W_THUNDERSTORM` | Thunderstorm | 1.00 | 1.0 | Rain | 0.85 | 12.0 | 0.85 | 0.30 | 0.85 | −5 |
| `W_HAIL` | Hail squall | 1.00 | 1.0 | Hail | 0.70 | 11.0 | 0.80 | 0.25 | 0.55 | −7 |
| `W_SNOW` | Snow | 0.96 | 0.3 | Snow | 0.45 | 3.5 | 0.30 | 0.40 | 0.00 | −4 |
| `W_HEAVY_SNOW` | Heavy snow / blizzard | 1.00 | 0.5 | Snow | 0.95 | 14.0 | 0.75 | 0.70 | 0.00 | −6 |
| `W_WINDY` | Strong wind (modifier) | — | — | — | — | 16.0 | 0.90 | — | — | — |
| `W_FOG` | Fog | 0.85 | 0.0 | None | 0.00 | 1.0 | 0.05 | 0.92 | 0.00 | −1 |

`W_WINDY` is a **modifier**, not a state: it can be combined with any precipitation archetype,
which is how "windy heavy rain" and "blizzard" arise. Wind is drawn independently of the
precipitation archetype from a per-archetype distribution, then optionally boosted by the windy
modifier. That is what gives the required overlapping combinations without a combinatorial state
list. The data specifies the modifier's speed and gust bands but no activation distribution, so
the wind sampler accepts an independent continuous `0..1` amount from its caller rather than
hiding a frequency constant in code. A full-strength modifier draw takes the maximum of base and
modifier values: a "boost" can never make an already-strong blizzard calmer; intermediate amounts
blend between those endpoints.

`Δtemp` is applied to a **seasonal base temperature curve**
`base(dayOfYear, hourOfDay) = annualMean + annualAmp·cos(2π(doy−201)/365) + diurnalAmp·cos(2π(h−15)/24)`
with `annualMean = 11 °C`, `annualAmp = 12 °C`, `diurnalAmp = 6 °C` for the default location.
`precipType` is then **derived**: `temperatureC > 2.5 → Rain`, `< 0.0 → Snow`, between → `Sleet`,
overriding the archetype's nominal type. So it snows in January and rains in July without any
special-casing, and a spring storm can produce sleet. This override applies to the three water
phases: a non-precipitating archetype remains `None`, while `W_HAIL` remains `Hail` for the separate
hail coupling of §40.

The authored target bands preserve those meanings: every `None` archetype targets exactly zero
precipitation intensity, so dry weather cannot wet a surface through a hidden continuous value.
Only rain, heavy rain, thunderstorm and hail targets may request non-zero thunder; drizzle, snow
and every dry archetype target exactly zero. A protected phase transition may still expose the
small residuals specified by §42.3 while it settles.

### 36.3 Seasons

Four transition matrices (winter / spring / summer / autumn), differing in archetype probabilities
and dwell times. Summer favours `W_CLEAR`/`W_PARTLY` with occasional sharp `W_THUNDERSTORM`;
winter favours `W_OVERCAST`/`W_SNOW`; autumn favours `W_RAIN` and `W_FOG`; spring is the most
variable.

**Season is a continuous phase, never an enum.** Under the compressed year of §35.2b a boundary is
crossed every 91 real minutes, so a matrix that switched at an instant would be visible as a
glitch. The clock exposes:

```cpp
struct SeasonPhase {
    float  yearFraction;   // 0 .. 1, 0 = vernal equinox (a new game starts here)
    int    primary;        // 0 spring, 1 summer, 2 autumn, 3 winter
    int    secondary;      // the season being blended towards
    float  blend;          // 0 .. 1, how far towards `secondary`
};
```

`blend` is 0 through the middle of a season and ramps over the outer 20 % at each end, so the last
fifth of autumn is already partly winter. Every **piecewise seasonal table** — the transition
matrix, vegetation colour, foliage density, snow-cover probability and ambience bed — is the
`blend`-weighted mix of the two neighbouring seasons' values, never a switch. Analytic annual
curves, such as §36.2's exact temperature cosine and the sun's declination, read the continuous
calendar position directly; replacing either with four seasonal constants would approximate an
already-continuous curve and make two sources for the same cycle.

**Hard seasonal gates are expressed as probability, not as prohibition, with one exception.**
`W_SNOW` has probability zero in summer: it is gated on the outdoor temperature the §36.2
curve produces, so snow in July is impossible by construction rather than by a special case.
Conversely `W_THUNDERSTORM`'s probability rises with the summer temperature excess, so the hottest
part of the year is the stormiest — which is both what the brief asked for and what real
continental summers do.

The executable gate applies to both frozen-water targets (`W_SNOW` and `W_HEAVY_SNOW`): their
effective transition weight is zero at `temperatureC >= 0`. A target forced by the developer
console still derives sleet or rain through §36.2, but the autonomous sampler cannot choose a snow
event above freezing. Thunderstorm weight keeps its authored and continuously blended seasonal
weight, multiplied by `1 + clamp((temperatureC − 18) / 12, 0, 1)`; it therefore rises continuously
from 1× at 18 °C to 2× at 30 °C without inspecting a month or season enum.

### 36.3.1 What each season looks like

The four looks below are the acceptance target for the seasonal art and material work. Each is
reached by blending, never by switching.

| Season | Vegetation | Ground | Weather bias | Ambience |
|---|---|---|---|---|
| **Spring** | Light, fresh green; blossom on the fruit trees; flower beds in bloom | Damp, vivid grass | The most variable; frequent light rain | Dawn chorus, birdsong through the day |
| **Summer** | Dense, dark green canopy | Grass dries and yellows during heat waves | Mostly clear, with sharp afternoon thunderstorms in the heat | Insects, distant traffic, quiet nights |
| **Autumn** | Yellow → orange → red, then leaf fall over the last fifth of the season | Fallen leaves accumulate, then thin | More wind and sustained rain; fog | Wind in bare branches, fewer birds |
| **Winter** | Bare branches | Snow cover on the garden, the roof, the fence and the cars; frost and ice | Overcast and snow; the coldest temperatures | Muffled, still; snow damps the ambience |

Snow cover, leaf litter and grass dryness are **accumulating state**, not instantaneous functions
of the season: they build and melt over simulated hours so a warm spell mid-winter visibly clears
the drive.

### 36.4 Determinism

A single `xoshiro256++` stream, seeded from the save (or from `initialstate.json` on a fresh
start), drives every weather decision. The RNG state is part of the save, so reloading reproduces
the same weather future. The runtime sampling overloads reconstruct the stream from
`WeatherState::rngState` and write it back after every successful archetype, timing or wind draw;
a rejected decision consumes nothing. Tests set the seed and assert exact sequences, including
continuing on both sides of a JSON save/reload boundary.

---

## 37. Rain

### 37.1 Simulation

A camera-relative **precipitation volume**: a vertical cylinder of radius 12 m and height 14 m,
centred on the camera and offset 3 m along the horizontal wind vector. Particles are stored in a
fixed pool and wrap through the volume, so there is no spawning or dying cost.

```
count      = round(kRainMax · precipIntensity^0.8 · qualityScale)   // kRainMax = 900
velocity   = (windVector · 0.55) + (0, −(6.0 + 3.0·intensity), 0)
streakLen  = clamp(|velocity| · 0.045, 0.10, 0.55) metres
```

Each particle is one quad, camera-facing but **elongated along its velocity** rather than
screen-vertical, which is what makes rain read as rain rather than as falling dots.

### 37.2 The roof mask

Rain must not fall inside the house or under the porch. A **coverage height field** on a 0.5 m
grid over the property stores, per cell, the height of the highest roof/soffit above it (or +∞).
A particle is drawn if the sample is +∞ (open sky), or if
`particle.y > coverage(x,z)` — i.e. it is above whatever is over that spot. Particles below a roof
are teleported to the top of the volume. The mask is generated
offline by `build_coverage.py` from the house, garage, porch, balcony, sunroom and shed geometry.

Consequence: standing under the porch in a downpour, the rain visibly stops at the porch edge.
Standing at an open upstairs window, rain streaks past the opening but does not enter.

### 37.3 Splashes

Within 6 m of the camera, on surfaces the mask says are exposed, splash particles spawn at a rate
proportional to `intensity`, capped at 60 alive. A splash is a 2-frame additive ring quad lying on
the surface, 0.25 s life. On water surfaces they are ripples instead.

### 37.4 Wetness

`surfaceWetness` integrates: `dW/dt = 0.045·intensity − 0.006·dryingRate(temperature, humidity,
windSpeed, isSheltered)`, clamped to 0…1. The dimensionless drying rate is
`clamp((temperatureC + 18) / 56, 0, 1) · (1 − humidity) · (1 + windSpeed / 30) · shelter`, where
`shelter` is 0.25 under cover and 1.0 when exposed. This makes the result continuous in every
weather channel; `isSheltered` describes the sampled surface, not a duplicate weather state.
It drives the material wet response (§22.1): albedo darkening,
specular boost, and a subtle normal flattening on Tier E. Puddles appear on the driveway, the
terrace and the road above `wetness > 0.55` as flat alpha-blended decals whose alpha follows
wetness, placed offline at the height field's local minima.

### 37.5 Audio

Four simultaneous layers, each an `AudioEmitter`-less 2-D `SoundEffectInstance` whose volume is
computed by the room-aware model (§64) rather than by `Apply3D`, because rain is not a point
source:

| Layer | Source | Gain driver |
|---|---|---|
| Open-air rain | `Ambiance_Rain_Calm/Strong_Loop_Stereo` (NOX) | exposure of the listener's cell to the sky |
| Rain on the roof | needs sourcing (§63.4) | `1 − distanceToRoofAbove/6`, strongest in `L3_*` |
| Rain on windows | needs sourcing | Σ over the cell's windows of `area · (1 − soundLoss)` |
| Gutter / downspout trickle | needs sourcing | positional, at the **six** downspouts, `Apply3D` |

> Corrected 2026-09-09 by `HOUSE-00776`. **Six, not four.** Each roof has a downspout at each of
> its four corners, less the two corners where the roofs meet: the garage wing projects from the
> house's east wall, so `ROOF_GARAGE`'s north-west corner is 0.60 m inside `ROOF_MAIN`'s footprint
> and `ROOF_MAIN`'s south-east corner is inside the garage's. A pipe at either is a pipe indoors.
> `layout.exterior.json`'s `downspouts` rows carry the head and the splash point, §15.7's rule 13
> checks them against the geometry, and `layout.audio.json` has an emitter at each.

Indoors the open-air layer is heavily attenuated and cross-faded toward its dull variant; opening a
window raises it audibly and immediately, which is one of the most satisfying interactions in the
game.

---

## 38. Snow

> **Reduced 2026-09-21: DONE needs falling snow only. Accumulation, the snow shells, melt and snow footsteps are optional after DONE; `snowDepth` is still integrated but not drawn** ([ADR-0015](docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md))

Same particle machinery, different parameters, plus accumulation.

| Parameter | Value |
|---|---|
| Count | `kSnowMax(1300) · intensity^0.7` |
| Fall speed | 0.7 – 1.4 m/s |
| Lateral drift | wind × 1.15, plus a per-particle flutter `sin(t·f + φ)` of ±0.35 m/s |
| Quad | square, 0.02–0.06 m, soft round alpha texture, slowly rotating |
| Blend | alpha, not additive (snow occludes) |

**Accumulation.** `snowDepth` integrates `dD/dt = 0.0009·intensity − melt(temperature, sunlight)`,
capped at 0.35 m. The accumulation term is present only while `precipType == Snow`; rain, sleet
and hail do not create ground snow. `sunlight` is §32.2's cloud-attenuated direct intensity in
0…1, and `melt = 0.00006·max(temperatureC, 0)·(1 + sunlight)` metres per simulated minute. Thus
snow never melts below freezing, while full direct sunlight doubles above-freezing melt without a
binary daytime switch. Rendering uses a **snow shell**: an offline-generated duplicate of every
up-facing exterior surface (terrain tiles, roofs, the porch and balcony decks, fence rails, garden
furniture tops, the car's roof and bonnet), offset along the surface normal by `snowDepth` and
drawn with a white snow material at `alpha = smoothstep(0.002, 0.03, snowDepth)`. Faces steeper
than each material's `snowResponse.slopeLimitDeg` are excluded from the shell at generation time,
so snow does not cling to walls.

This is a Tier-S technique with one extra draw call per snow-shell chunk (≈ 18 chunks), it fades
in and out continuously, and it needs no shader. Tier E blends the snow material into the base
surface per pixel using the same depth value, which removes the shell entirely.

Footsteps switch to the `snow` surface set (NOX has 22 jump / 21 run / 24 walk snow samples), the
ambience becomes markedly quieter (snow absorbs), and the exterior reverb hint changes.

---

## 39. Storm

> **Optional after DONE since 2026-09-21: the thunderstorm weather state renders as heavy rain under dark cloud, with no lightning or thunder** ([ADR-0015](docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md))

`thunderIntensity` drives a Poisson lightning process:
`rate = 0.02 + 0.30 · thunderIntensity` strikes per second.

Per strike:

1. Draw a distance `d` from a distribution biased by `thunderIntensity`
   (0.4 – 12 km; a storm directly overhead gives 0.4–2 km).
2. Draw an azimuth, biased toward the wind's upwind direction.
3. **Flash**: a 3-lobed envelope over 90–160 ms (bright, dark, brighter, decay) —
   real lightning strobes. During the flash:
   * every cell with an exterior window gets `daylight = 1.0`;
   * every dynamic object's `DirectionalLight0` becomes a strong white light from the strike
     azimuth at ~15° altitude;
   * a full-screen additive white `SpriteBatch` quad is drawn at the envelope's alpha, scaled by
     how much sky the camera can see (0.25 indoors with the curtains open, 1.0 outdoors);
   * for near strikes (`d < 2 km`) a **bolt** is drawn: a recursive midpoint-displaced polyline of
     3 levels with 2 forks, as additive camera-facing quads along each segment.
4. **Thunder**: scheduled at `t + d/343` seconds. Sample selection by distance
   (`< 1.5 km` crack, `1.5–5 km` rumble, `> 5 km` distant roll), volume `∝ 1/(1 + d/2)`, and the
   room-aware attenuation of §64 applied on top so thunder in the basement is a muffled thump.
   The delay is the detail that sells it: counting the seconds between flash and thunder works.
5. Wind receives a **gust front**: `windSpeed` jumps by `+3..8 m/s` for 4–9 seconds shortly before
   a near strike, which bends the trees and slants the rain first — the classic pre-storm cue.

---

## 40. Hail

> **Optional after DONE since 2026-09-21: the hail weather state renders as heavy rain** ([ADR-0015](docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md))

| Parameter | Value |
|---|---|
| Count | `kHailMax(500) · intensity^0.9` |
| Fall speed | 9–16 m/s |
| Quad | 0.008–0.022 m, bright white-blue, additive rim, slight motion blur streak |
| Bounce | a particle reaching the coverage surface (roof mask or ground) reflects with 0.35 restitution and a randomised tangential component, lives 0.5 s more, then wraps |
| Ground accumulation | none (hail melts fast); instead a short-lived scatter of static hail-stone decals at `intensity > 0.5`, fading over 40 simulated seconds |
| Audio | a dense impact layer whose density follows intensity, plus distinct **hail-on-roof**, **hail-on-window** and **hail-on-car** layers routed by the room-aware model. Hail on the car in the driveway heard from inside the garage is a deliberate, achievable detail. |

Hail always co-occurs with `thunderIntensity > 0.3` and a temperature drop, because that is how
hail happens.

---

## 41. Wind

> **Reduced 2026-09-21: DONE uses the weather state's wind for the slant of rain and snow and for the wind audio layer. The gust model and vegetation sway are optional after DONE** ([ADR-0015](docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md))

Wind is a first-class quantity, not a rain modifier.

```
instantaneous = base + gust
gust(t)       = gustFactor · base · (0.6·noise1(t·0.11) + 0.3·noise1(t·0.43) + 0.1·noise1(t·1.7))
direction(t)  = windDirectionDeg + 12° · gustFactor · noise1(t·0.07)
```

`noise1` is a cheap 1-D value-noise with a fixed seed. What wind drives:

| Consumer | Effect |
|---|---|
| Rain / snow / hail | lateral velocity, streak angle, volume offset |
| Trees and shrubs | sway amplitude ∝ `windSpeed^1.3`, phase per instance, plus a gust-synchronised whole-canopy lean |
| Grass and hedges | a smaller, faster version of the same |
| Curtains at an open window | a 3-bone rigid sway on the curtain prop |
| Doors | a door left open in `windSpeed > 9 m/s` slowly swings and eventually slams (with a sound, and it *closes the portal* — the visibility system just follows) |
| The gate | rattles |
| Audio | wind ambience layer gain and low-pass cross-fade ∝ speed; a separate **wind-through-an-open-window** whistle whose gain follows the window's aperture; roof and eaves whistle in the attic |
| Chimes / the swing bench | small ambient motion and sound |
| The weather vane on the shed | points into the wind — a free, readable indicator |

---

## 42. Weather transitions

### 42.1 Mechanism

```
every simulated minute:
    if now >= targetExpiry:
        next = sampleArchetype(season, current, rng)
        targetExpiry = now + dwell(next, season, rng)          // 25 .. 380 simulated minutes
        transitionEnd = now + transitionTime(current, next, rng)  // 5 .. 45 simulated minutes
        blendStartTime = now
        blendStartState = snapshot(state)
    t = smoothstep(0, 1, (now − blendStartTime) / (transitionEnd − blendStartTime))
    for each channel c:
        desired = lerp(blendStartState[c], target[c], t)
        state[c] = moveTowards(state[c], desired, maxRate[c] · dt)
```

Both timing draws are uniform over authored ranges. `dwell(next, season, rng)` multiplies the
destination's base range by the continuously blended seasonal dwell scale.
`transitionTime(current, next, rng)` uses the component-wise mean of the current and next
archetypes' characteristic transition ranges: both sides of the change influence its tempo, but
the data does not repeat a timing range for all 169 source/destination pairs. Authored and
seasonally scaled ranges must keep the §42.1 bounds shown above.
The snapshot is immutable for the life of a transition: the desired curve is evaluated from that
captured vector, never recursively from the rate-limited live state. Scalar channels use the cubic
`t²(3−2t)` smoothstep; `windDirectionDeg` follows the shortest wrapped arc. Precipitation phase,
integrated surface state and RNG state are not interpolation channels.

`WeatherSystem` is the sole runtime owner of the live vector, selected target, expiry, persisted
RNG and immutable snapshot. `Weather: Fixed` disables automatic selection after converging to its
chosen target but continues the continuous and surface integration; `Weather: Off` freezes that
same owner. The console's `weather set` and `weather freeze` address those controls directly rather
than maintaining a debug copy of weather state.

### 42.2 Rate limits — the anti-absurdity guarantee

| Channel | Max rate (per simulated minute) | Time for a full 0→1 swing |
|---|---|---|
| `cloudCover` | 0.045 | ≥ 22 sim min |
| `cloudCumuliform` | 0.080 | ≥ 12.5 sim min |
| `precipIntensity` | 0.070 | ≥ 14 sim min |
| `windSpeed` | 0.9 m/s | ≥ 33 sim min for 0→30 |
| `windDirectionDeg` | 4.5°, shortest way | ≥ 40 sim min for a reversal |
| `gustFactor` | 0.150 | ≥ 6.7 sim min |
| `thunderIntensity` | 0.055 | ≥ 18 sim min |
| `fogDensity` | 0.030 | ≥ 33 sim min |
| `temperatureC` | 0.25 °C | ≥ 224 sim min for −18→38 °C |
| `humidity` | 0.080 | ≥ 12.5 sim min |
| `snowDepth` | integrated, never set directly | |
| `surfaceWetness` | integrated, never set directly | |

The first ten rows are driven toward archetype targets. `surfaceWetness` and `snowDepth` use their
accumulation/melt equations instead, and the discrete `precipType` uses §36.2's temperature rule
plus §42.1's protected phase change.

At the default 60× time scale, 22 simulated minutes is 22 real seconds. So the sky *can* fill in
noticeably fast — which is right for a summer storm — but "clear → blizzard → clear in seconds" is
arithmetically impossible. A unit test asserts exactly that by driving 10 000 simulated minutes
with 200 random seeds and checking no channel ever exceeds its rate.

### 42.3 Precipitation type changes

`precipType` cannot change while `precipIntensity > 0.05`. When the temperature crosses a
threshold mid-event, intensity is first ramped down to 0.05, the type flips, then intensity ramps
back under §42.2's normal precipitation rate. The first observable state carrying the new type is
therefore exactly at or below 0.05, even when one update spans the whole ramp-down. So rain turns
to sleet turns to snow through a lull, as it does in life.

The water phase is derived from the **next rate-limited live `temperatureC`**, not from the
archetype's eventual target temperature; after the protected lull, the two observable fields
therefore cannot contradict each other. The same boundary caps residual intensity after the type
has become `None`. Entering `Hail` additionally waits until `thunderIntensity > 0.3`, and leaving it
holds thunder just above that boundary until the protected phase switch has completed, preserving
§40's always-co-occurs rule on every observable state.

---

## 43. Player controller

### 43.1 Body and motion

| Property | Value |
|---|---|
| Capsule radius | 0.30 m |
| Standing height | 1.80 m (eye at 1.68 m) |
| Crouched height | 1.25 m (eye at 1.15 m) — **attic only**, automatic |
| Mass model | none; kinematic |
| Gravity | 9.81 m/s² (used only for stepping down and the short fall onto the terrace) |
| Terminal fall | 12 m/s; a fall > 2.4 m plays a heavy landing sound; there is no damage |
| Step-up | ≤ 0.22 m automatically |
| Step-down | ≤ 0.45 m automatically, with a small camera-height smoothing |
| Slope limit | 46° |
| Push-out | 0.02 m depenetration per iteration, 4 iterations |

### 43.2 Speeds

| Mode | Speed | Notes |
|---|---|---|
| **Normal walk** | **1.35 m/s** | The measured average human walking speed. |
| **Fast walk** | **2.05 m/s** | A brisk walk, not a run. Above ~2.2 m/s a human transitions to a jog, and the brief is explicit that this is still walking. |
| Backwards | ×0.72 | |
| Strafe | ×0.85 | |
| On stairs | ×0.72 | |
| Crouched (attic) | ×0.55 | |
| Carrying an item | ×0.94 | |
| Deep snow (`snowDepth > 0.12`) | ×0.80 | |
| Acceleration / deceleration | 9.0 / 13.0 m/s² | Reaches full speed in ~0.15 s — responsive, not floaty |

**Shift toggles** between normal and fast walk. It is not hold-to-sprint. Pressing it plays a
subtle UI tick and the HUD briefly shows a walk-mode glyph. The mode is stored in **settings**
(a preference, not world state) and therefore survives both a save/load and a *Reset House*;
the player block in the save also records it so a save carries a self-consistent snapshot, and
settings wins on conflict. That resolves the brief's open question explicitly (D-09, §77).

### 43.3 Input map (Linux desktop)

| Action | Binding |
|---|---|
| Move | `W` `A` `S` `D` / arrows |
| Look | mouse |
| Toggle walk speed | `Shift` |
| Interact | `E` |
| Secondary interact (e.g. "take" vs "open") | `F` |
| Toggle first / third person | `V` |
| Menu / back | `Esc` |
| Quick save | `F9` (development builds only; normal play autosaves) |
| Screenshot | `F12` |
| Debug overlays | `F1`–`F8` (development builds only) |
| Toggle mouse capture | `Alt` (also released automatically by the menu) |

All bindings are data (`settings.json`) and go through `IInputSource`, so remapping and, later,
touch and gamepad are additions rather than rewrites.

### 43.4 Interaction with the visibility system

The controller publishes `CellEntered` whenever `currentCell` changes. That single event drives
the visibility root, the audio listener's cell, the residency request set, the ambience bed
cross-fade and the debug overlay. Cell membership uses 5 cm of hysteresis so standing exactly in a
doorway does not thrash.

---

## 44. First-person camera

* Eye at `playerPosition + (0, eyeHeight, 0)`, with `eyeHeight` smoothed by a critically-damped
  spring (ω = 18 rad/s) so step-ups and stair climbing do not jolt the view. The spring tracks the
  eye's **world height** and not its height above the feet — above the feet it is the constant
  1.68 m, and a spring chasing a constant does nothing. A step up moves the *feet*; the eye is left
  behind in world space and catches up.
* The eye's height is three terms in a fixed order (`player::FirstPersonView`): the spring, then
  §44's bob, then §43.1's landing dip, and §44's near-surface pull-back last of all because it
  needs the view direction the others produce. The bob and the dip are added **after** the spring —
  through it they would be a 1.8 Hz wobble and a 62 ms impulse fed into a filter built to remove
  exactly those.
* **Mouse look**, strict XNA 4.0: each frame read `Mouse::GetState()`, compute the delta from the
  window centre, apply sensitivity, then `Mouse::SetPosition(centreX, centreY)`. The cursor is
  hidden (`Game::setIsMouseVisibleProperty(false)`). This is the canonical XNA first-person idiom
  and needs no CNAEXT relative-mouse mode.
* Sensitivity: 0.0022 rad/pixel by default, settings-controlled 0.2× – 4×; optional raw-ish
  smoothing over 2 frames, default off; invert-Y option.
* Pitch clamped to ±85°. **Roll is always zero** during ordinary walking.
* FOV: 70° vertical by default (102.4° horizontal at 16:9), settings-controlled 55°–95°. The
  setting is the **vertical** angle on every window 4:3 or wider — a wider display shows more
  of the room rather than the same room larger. Below 4:3 the vertical instead opens so the
  horizontal angle never falls under what 4:3 gives (a portrait window at 70° vertical would
  see 43° of the room), capped at 120° so the projection stays a projection.
* **Head motion**: a very small vertical bob, amplitude `0.012 m · (speed/1.35)`, at the footstep
  cadence, plus a 0.35° lateral sway. Default **on** but low; a settings toggle turns it off. The
  rule adopted here: any bob large enough to *notice* is too large. Those numbers are §68's
  `Subtle`, which is the default; `Normal` doubles them and `Off` is still. The cadence is §62.4's
  own accumulator — one step per 0.75 m walking, 0.95 m at the fast walk — and it is the *same*
  accumulator the footsteps come from, so the sound and the view cannot disagree about when a foot
  lands. The rise is measured **above** §43.1's eye height, which is the bottom of a walking cycle
  rather than its middle, so every foot plant is at the standing height and stopping, landing and
  teleporting all leave the eye where it was. The sway turns about the **vertical** axis: the view
  drifts left and right and the horizon stays level, because roll is zero.
* **Camera collision**: the eye is inside the player capsule, so walls are already handled. The
  only special case is a near-plane clip against a surface the capsule is touching — solved by
  pulling the near plane to 0.05 m and pushing the eye 0.06 m back along the view direction when
  a 0.10 m forward probe hits. Both are **graded by the probe's own distance** rather than
  switched on at the hit: full at the surface, nothing at 0.10 m, and stateless in between, so the
  response arrives as the player does and leaves as they step off. The probe follows the VIEW —
  pitch and the bob's sway included — because a player looking down at the floor they are standing
  on is the commonest surface at arm's length, and 0.06 m against §43.1's 0.30 m radius is what
  keeps the pulled-back eye inside the capsule that the walls are already handled by.
* **Stairs**: the eye height spring plus the step-up logic means climbing is smooth; the camera
  never intersects the flight above because the stair collision is a ramp (§48).
* **Landing dip**: when a fall ends (§43.1), the eye is given a downward *impulse* — not a
  displacement, so nothing jumps on the landing frame — and a critically damped ω = 16 spring
  turns it into a dip and a recovery. The dip's lowest point is `0.035 m · drop`, capped at
  0.09 m, reached 62 ms after the landing, nine tenths recovered by a third of a second. Past
  §43.1's 2.4 m the cap means a fall from the roof and one from the first floor land the same:
  the knees have already done everything they can.
* **Exposure adaptation** (§25.7) is a first-person feature as much as a lighting one.

---

## 45. Third-person camera

> **Removed from scope 2026-09-21. Design history, not a requirement: the game is first person only.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

* A spring arm from a pivot at `playerPosition + (0, 1.55, 0)`.
* Desired distance 3.2 m; desired pitch follows the look input; yaw follows the look input and the
  avatar turns to face the movement direction with a 0.18 s turn blend.
* **Collision**: sweep a sphere of radius 0.22 from the pivot to the desired camera position. The
  camera sits at the first hit minus 0.06 m. Recovery to the desired distance is eased at 2.5 m/s
  (fast enough not to feel stuck, slow enough not to snap).
* **Narrow spaces**: if the swept distance is below 1.4 m for more than 0.3 s, the camera switches
  to an over-the-shoulder offset (0.35 m right, 0.15 m up, distance 1.1 m). Below 0.7 m it
  temporarily returns to first person with a 0.25 s blend, and returns to third person when space
  allows — corridors and closets in this house genuinely require it.
* **Avatar fade**: when the camera is within 0.9 m the avatar fades to 40 % alpha
  (`BlendState::AlphaBlend`, `SkinnedEffect::Alpha`), and below 0.5 m it is not drawn.
* **Stairs**: the pivot uses the *smoothed* eye height, and the camera's vertical follow is
  damped more heavily than its horizontal follow, which removes the characteristic third-person
  stair bounce.
* Shoulder side is switchable (`Q`/`E` in third person, settings default right).
* In third person the interaction ray still originates at the **avatar's eye**, not the camera,
  so what you can interact with does not change with camera mode.

---

## 46. Character customisation

> **Removed from scope 2026-09-21. Design history, not a requirement: there is no visible avatar.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

### 46.1 Modular, not bespoke

```
Avatar = Body(sex, skinTone) + Head(sex, skinTone, faceVariant) + Hair(style, colour)
       + Top(set, colour) + Bottom(set, colour) + Shoes(set, colour)
```

| Slot | Options | Assets |
|---|---|---|
| Sex | male, female | 2 body meshes, 2 head meshes |
| Skin tone | 6 | 1 albedo texture per (sex, tone) = 12, sharing UVs |
| Face variant | 3 per sex | blend-shape-free: 3 head meshes per sex sharing the skeleton |
| Hair style | 6 (incl. "none"/short) | 6 meshes |
| Hair colour | 8 | 1 texture per colour, or 1 texture + a tint — tint chosen |
| Top | 5 sets (t-shirt, shirt, jumper, hoodie, blouse) | 5 meshes × 2 sexes = 10 |
| Top colour | 8 tints | tint only |
| Bottom | 4 sets (jeans, chinos, skirt, shorts) | 4 × 2 = 8 |
| Shoes | 3 sets | 3 × 2 = 6 |

**Total unique meshes: 2 + 6 + 6 + 10 + 8 + 6 = 38**, plus 12 skin textures and ~10 clothing
textures. Combinations: 2 × 6 × 3 × 6 × 8 × 5 × 8 × 4 × 3 = **414 720**, from 38 meshes. That is
the argument for modularity, made concretely.

### 46.2 Rendering a modular avatar

All meshes share one 32-bone skeleton and one bone palette, so the avatar is 4–6
`SkinnedEffect` draws (body, head, hair, top, bottom, shoes), each with its own texture and tint,
all using the same `SetBoneTransforms` array. Verified at load: every customisation mesh's bone
list must match the body's name-for-name, or the asset is rejected with a clear diagnostic.

Hair uses `AlphaTestEffect` for its card layers and `SkinnedEffect` for the scalp cap — but
`AlphaTestEffect` is not skinned. Resolution: hair cards are skinned to a **single** head bone, so
they are drawn as a rigid mesh transformed by that bone's world matrix with `AlphaTestEffect`.
This is exactly how XNA-era games did hair, and it works because hair does not deform.

### 46.3 The customisation screen

Reached from the main menu and the pause menu. A rotating avatar on a neutral pedestal under the
house's own lighting, with slot arrows and a colour strip. Choices are stored in **settings** and
mirrored into the save's player block. Changing the avatar mid-game is allowed and takes effect
immediately (the pause menu is not a modal freeze; the world keeps its state).

Default: female, skin tone 3, face 1, hair 2 in colour 4, blouse in colour 2, jeans, trainers.
Deterministic, so tests and screenshots are stable.

---

## 47. Character animation

> **Removed from scope 2026-09-21. Design history, not a requirement: nothing is animated by skeleton. `src/animation/` is a cleanup candidate.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

### 47.0 Project-owned animation data

`SkinningData`, `AnimationClip` and `Keyframe` come from Microsoft's **XNA Skinned Model Sample**.
They were *sample* classes — code Microsoft shipped for game developers to copy into their own
projects — not part of the XNA 4.0 Framework. CNA provides `CNAEXT`-marked copies as a convenience
for ported samples. `cna-house` does not use them, for the same reason it uses no other
CNA-specific type: they are not XNA 4.0 API. It owns its equivalents instead, which is precisely
what an original XNA title built on that sample did.

```cpp
namespace cnahouse::anim {

struct Keyframe { int bone; float time; Vector3 translation; Quaternion rotation; Vector3 scale; };

struct Clip {
    std::string           name;
    float                 duration;        // seconds
    std::vector<Keyframe> keys;            // sorted by bone, then by time
    std::vector<int>      boneFirstKey;    // index of each bone's first key, size = boneCount + 1
    float                 strideLength;    // 0 for non-locomotion clips (§47.4)
    std::vector<float>    footPlants;      // contact times, per foot (§48)
};

struct Skeleton {
    std::vector<std::string> boneNames;        // in glTF skin-joint order — the blend-index order
    std::vector<int>         parent;           // -1 for the root
    std::vector<Matrix>      bindPose;         // local, per bone
    std::vector<Matrix>      inverseBindPose;  // world -> bone
    std::vector<int>         modelBoneIndex;   // filled at bind time: -> Model::Bones
};

struct ClipLibrary {
    Skeleton                             skeleton;
    std::vector<Clip>                    clips;
    std::unordered_map<std::string, int> byName;
    // Validates and fills modelBoneIndex. RETURNS the failure (corrected 2026-09-06, HOUSE-00167):
    // docs/conventions.md §5.4 makes malformed content a recoverable failure, and a Result still
    // lets a caller treat a mismatch as fatal while a throw does not let it do anything else.
    util::Result<void> BindTo(const Model& model);
};

}   // namespace cnahouse::anim
```

Every member is a `Microsoft::Xna::Framework` math type or a standard-library container. Tier P
(§4.3, OWN-01).

**Where the data comes from.** Not from `Model::Tag`, which the runtime never reads. Offline,
`tools/assets/anim_extract.py` reads the same source `.glb` that `cna-content` compiles and writes
a sidecar `Content/Anim/<actor>.chanim`: a small, versioned binary file in our own format
(`docs/anim-format.md`, `HOUSE-00166`). At runtime it is opened with `TitleContainer::OpenStream`
and read with `System::IO::BinaryReader` — both XNA 4.0 / .NET API, neither of them graphics. The
mesh itself still arrives as a plain `Model` from `ContentManager::Load<Model>`, and the bone
palette still goes to `SkinnedEffect::SetBoneTransforms`. Nothing about the rendering path changes.

**Why a sidecar rather than the `Tag`.** XNA's own answer to "the framework has no place for my
skeleton" was a custom content processor writing a custom type into `Model.Tag`. We cannot add a
processor to CNA's pipeline without modifying CNA, so the same custom data travels *beside* the
model instead of inside it. The runtime shape is identical; only the transport differs. It also
removes BL-01 entirely — the XNB writer's null-`Tag` limitation cannot affect data we never put
there.

**The one thing that must agree.** The vertex blend indices baked into the `.cnb` reference the
glTF skin's joint order. The sidecar stores that same order *and* the joint names. `BindTo` checks
every joint name against `Model::Bones` and fills `modelBoneIndex`; a mismatch is a **fatal content
error** naming the offending joint, never a silent deformation (`HOUSE-00167`). `HOUSE-00074`
proves the ordering assumption against a real skinned asset in phase 1, before anything depends on
it, and `tools/ci/check_anim_assets.py` re-checks it for every character on every content build
(`HOUSE-00225`).

**One skin per runtime model.** Externally sourced characters sometimes carry several glTF skins in
one file. Rather than ask CNA for a multi-skin view of a `Model`, the rule is offline and absolute:
a `.glb` entering the build has **exactly one skin**. `tools/assets/skin_split.py` splits a
multi-skin source into one file per skin, recording each part's attachment bone in the asset
manifest, and `gltf_validate.py` rejects any remaining multi-skin file (`HOUSE-00224`). A
multi-part character is then drawn the way the customisation system already draws one (§46):
several `Model`s sharing a single skeleton and a single bone palette. `getSkinsEXTProperty()` is
never called, and the CI gate makes calling it impossible (§70.1).

### 47.1 Clip set

| Clip | Length | Loop | Notes |
|---|---|---|---|
| `idle` | 4.0 s | yes | breathing, weight shift |
| `idle_look` | 3.2 s | no | occasional, blended in when standing still > 8 s |
| `walk_fwd` | 1.06 s | yes | stride 1.43 m → cadence matched to 1.35 m/s |
| `walk_back` | 1.20 s | yes | |
| `walk_left`, `walk_right` | 1.10 s | yes | strafes |
| `fastwalk_fwd` | 0.86 s | yes | stride 1.76 m → 2.05 m/s |
| `turn_left_90`, `turn_right_90` | 0.7 s | no | in-place turns when the avatar rotates without translating |
| `stair_up` | 1.30 s | yes | one full step cycle covering 2 risers |
| `stair_down` | 1.20 s | yes | |
| `crouch_idle`, `crouch_walk` | 3.0 / 1.30 s | yes | attic |
| `reach_low`, `reach_mid`, `reach_high` | 0.9 s | no | upper-body only, masked |
| `sit_down`, `sit_idle`, `stand_up` | 1.4 / 4.0 / 1.3 s | no/yes/no | chairs, the toilet |
| `open_door` | 1.0 s | no | upper-body masked |
| `land_soft`, `land_hard` | 0.4 / 0.8 s | no | |

**18 clips per sex** (the two sexes share clips where the skeleton allows; separate clips only for
`idle`, `walk_fwd` and `fastwalk_fwd`, where gait differences are visible). `HOUSE-00295` rejected
the CMU-to-MPFB research retarget after all six trials tore the skin at the bind boundary, so these
are authored and cleaned directly on the final 32-bone game rig in Blender by `HOUSE-02219`, then
exported in the body `.glb`.

### 47.2 State machine and blending

```
        ┌──────── idle ◄──────────┐
        │          ▲              │
   speed>0.1    speed<0.1      turn only
        ▼          │              │
     locomotion ───┘         turn_l/r_90
        │  (walk_fwd/back/left/right/fastwalk blended by
        │   the 2-D velocity in local space)
        ├── onStairs & slope>+15° → stair_up
        ├── onStairs & slope<−15° → stair_down
        ├── crouched              → crouch_*
        └── landing               → land_soft/hard (one-shot, blended out)

  upper-body mask layer: reach_* / open_door, weight ramped 0→1→0 over the action
```

### 47.3 `ClipPlayer` — why we write our own

CNA's `AnimationPlayer` is a copy of the Skinned Model Sample's player: it plays exactly one clip
and overwrites every bone (BL-10). It is also sample code rather than XNA API, so `cna-house` does
not use it at all. `ClipPlayer` is written over the project-owned data of §47.0:

```cpp
class ClipPlayer {                              // cnahouse::anim
    const Skeleton* skeleton_;
    struct Track { const Clip* clip; double time; float weight; bool loop; };
    Track base_, blend_;            // cross-fade pair
    Track upper_;                   // masked layer
    std::vector<Matrix> local_, world_, skin_;
public:
    void Play(const Clip& clip, float blendSeconds, bool loop);
    void PlayUpper(const Clip& clip, float blendSeconds);
    void SetRate(float rate);       // stride matching
    void Update(TimeSpan dt);
    const std::vector<Matrix>& GetSkinTransforms() const;   // → SkinnedEffect::SetBoneTransforms
};
```

`Update` evaluates each track's keyframes into local **TRS** (decomposing the clip's stored
matrices once at load into position/rotation/scale so blending is a `Vector3::Lerp` +
`Quaternion::Slerp`, not a matrix lerp), blends by weight, applies the upper-body mask by bone
index, composes parent→child into world transforms, and multiplies by the inverse bind pose to
produce skin transforms. Every operation is `Microsoft::Xna::Framework::Matrix`,
`Quaternion` and `Vector3` over `cnahouse::anim` data — Tier A math, Tier P code, no CNA symbol of
any kind, ~250 lines.

### 47.4 Stride matching (no foot sliding)

```
rate = horizontalSpeed / (clipStrideLength / clipDuration)
```

clamped to [0.6, 1.6]; outside that range the state machine switches clip instead of stretching
one. `clipStrideLength` is measured offline per clip by `tools/assets/measure_stride.py` and
stored in the `.chanim` sidecar's `Clip::strideLength` (§47.0). This single number is what removes foot sliding, and it costs
nothing at runtime.

**How it is measured, since the source clips arrive in two incompatible shapes** (`HOUSE-00194`).
A CMU trial translates its root across the capture volume; the same clip after retargeting stands
still and cycles its feet underneath. One identity covers both: while a foot is planted it does not
move, so the body's velocity over the ground is the negative of that foot's velocity *relative to
the root*. Integrating that over the clip and dividing by the cycle count gives the stride, and for
a root-motion clip it reproduces the root's own travel — so the two measurements cross-check each
other, and **their disagreement is the foot sliding itself** (`footSlideRms`, and a warning above
10 %). Measured on an exact synthetic walk: the root-motion path returns the authored figure
exactly, the foot-contact path lands 0.8 % high from the finite sampling of the stance boundaries.

Three things the tool refuses to assume, each because assuming it would put a plausible wrong number
into `Clip::strideLength`: the **units** (a CMU skeleton is in inches, and an unscaled stride of 56
would make every character sprint), whether the clip is **locomotion at all** (a turn in place gets
0, which is what this section wants), and whether the **feet can be found** (an unrecognisable
skeleton reports 0 and a reason, never a guess).

### 47.5 Foot placement (IK-lite)

Per foot, per frame, when `onGround`:

1. Cast a `Ray` down from the ankle bone's animated world position, 0.35 m.
2. Compute `Δ = hitY + footOffset − ankleY`, clamped to ±0.12 m, smoothed over 0.08 s.
3. Apply `Δ` to the ankle via a **two-bone analytic IK** on (hip, knee, ankle): with the target
   ankle position known, the knee angle follows from the law of cosines and the knee's swing plane
   is preserved from the animated pose. ~30 lines of `Vector3`/`Quaternion` maths.
4. Rotate the foot to the surface normal, clamped to 25°.

Enabled on stairs and slopes; disabled in mid-air and while turning in place. This is the
difference between a character that walks *on* the stairs and one that walks *through* them.

---

## 48. Stairs

Stairs get their own section because the brief is explicit that a diagonally-translated walk cycle
is not acceptable, and because they are the hardest part of a multi-storey house.

### 48.1 Collision

Each flight is stored as (a) the real stepped visual geometry and (b) a **collision ramp**: a
single sloped box from the bottom nosing to the top nosing, plus a vertical box at each end.
Movement uses the ramp. This eliminates the classic staircase problems in one stroke: no
step-by-step jitter, no getting caught on a nosing, no capsule ping-pong, and a stable, continuous
ground height that the camera spring can follow.

The ramp carries `SurfaceKind::Stairs` and the flight's `slopeDeg`, `riserHeight`, `going` and
`direction`, so the animation and audio systems know exactly what the player is on.

### 48.2 Locomotion

* On a `Stairs` surface with `|slope| > 15°`, the state machine blends to `stair_up` or
  `stair_down` over 0.2 s.
* Speed is reduced to ×0.72 and the clip rate is matched to the **along-slope** speed, not the
  horizontal component, so the feet keep up with the actual travel.
* Foot IK (§47.5) is mandatory here: it is what puts each foot on a tread rather than in the air
  above one or inside the one below.
* The eye-height spring is stiffened (ω = 24) on stairs so the view rises steadily rather than
  bobbing per step.
* Turning on a half-landing uses the in-place turn clips.
* The third-person camera damps its vertical follow (§45).

### 48.3 Audio

`SurfaceKind::Stairs` selects a stair-specific footstep set (wood stair, carpeted stair, concrete
basement stair) rather than the room's floor surface, and the step cadence follows the *riser*
count rather than the stride distance: one footstep per riser, triggered by crossing each riser's
height. That is what makes stairs sound like stairs.

### 48.4 Which stairs, and their surfaces

| Flight | Visual | Footstep surface |
|---|---|---|
| `STAIR_MAIN_L0_L1`, `_L1_L2` | Painted timber carriage, oak treads, runner carpet, turned balusters | `stair_carpet` (runner) with `stair_wood` at the exposed edges |
| `STAIR_ATTIC_L2_L3` | Bare painted timber, closed string, no carpet | `stair_wood` |
| `STAIR_BASEMENT_L0_B1` | Timber treads, open risers, steel handrail | `stair_wood_open` (a hollower sound) |
| `STEPS_PORCH`, `STEPS_TERRACE` | Bluestone | `stone` |
| `STEPS_GARAGE` | Concrete | `concrete` |

---

## 49. Physics and collision

### 49.1 Decision: no third-party physics engine

**A project-owned deterministic kinematic collision system is used. Bullet, Jolt, PhysX and
friends are rejected.** The reasoning, since the brief asks for it explicitly:

| What the world needs | Does it need a rigid-body engine? |
|---|---|
| Player capsule vs. static world, slide, step, slope | No — a swept capsule and 4 depenetration iterations |
| Camera sphere sweep | No |
| Door swing volumes affecting the player | No — an animated OBB, tested per frame |
| Pets on a navigation graph | No |
| The car sitting in the garage | No — it never moves |
| Nudgeable small props (a chair, a ball, a box) | A *very* small amount: gravity, a support plane, friction, and a push impulse. ~200 lines. |
| Stacking, jointed bodies, ragdolls, vehicles, cloth, destruction | **Nothing in the brief needs any of it** |

Costs avoided: 2–8 MB of binary, an Emscripten build integration, a second memory allocator, a
determinism story that has to be re-established for saves and tests, and a large API surface that
invites over-use. Costs incurred: ~1 200 lines of well-tested collision code that we fully
understand and can make deterministic by construction.

If a future feature genuinely needs rigid bodies, the `PhysicsSystem` interface is narrow enough
(`SweepCapsule`, `SweepSphere`, `RayCast`, `Overlap`, `GroundProbe`) that swapping in Jolt would
be a contained change. That is recorded as ADR-0007.

### 49.2 Static collision representation

Built offline from the layout and the per-asset `_COL` proxies into `content/world/collision.bin`:

* Per cell: a list of **OBBs** (walls, floors, ceilings, furniture bounds) and a small list of
  **triangle meshes** (stairs ramps, the terrain patch, the roof underside in the attic, curved
  props).
* A per-cell **loose grid** (1 m) indexes them, so a capsule sweep tests ~6 shapes, not 900.
  **Measured by `HOUSE-00542`, and the "not 900" needs splitting in two.** A step-sized query — a
  0.62 m capsule swept 0.5 m, at the middle of every cell that has geometry — hands the narrow
  phase **4.84 shapes on average, worst 14**. What it beats is not 900 but **15.2**, which is what
  one cell holds; the other factor of seventy is the per-cell partition, not the grid. So the two
  earn their keep at different scales and both are needed: the cell turns 1 064 shapes into 15, and
  the grid turns 15 into 5.
* Exterior collision uses the terrain height field plus OBBs for fences, walls, kerbs, the shed,
  vehicles and tree trunks.

Total: ~4 300 OBBs, 18 triangle meshes (~9 000 triangles), the height field. Under 3 MB.

Measured over the authored layout when `HOUSE-00472` built it, with no prop carrying a proxy yet:
**1 068 shapes — 1 046 OBBs and 22 triangle meshes.** Phase 7 then cut the stair wells out of the
floors, put a rail round them and gave the house back the front wall it was missing, and the
census reads **1 132 shapes — 1 110 OBBs and 22 triangle meshes**: 886 wall pieces, 140 floors, 94
ceilings, 12 stair shapes (`HOUSE-00210`'s defect, `HOUSE-00567`). Three of those groups exist
because the layout **cannot** state them, which is what "built offline from the layout" leaves to
this stage:

* **The rafter envelope.** 13 roof planes, each clipped in plan to the cell under it, covering the
  attic's whole 273.9 m². A cell is an axis-aligned volume, so `L3_STORE_W` declares
  `yOverride: [9.30, 13.90]` — §13.6's *maximum* head-room, "1.2 → 4.6 m, so most of it is
  crouch-only". Read as a box it is a flat lid at +13.90 over the whole store. The rafters put the
  ceiling back where the roof is: over the west store the lowest of them is +10.57, 3.33 m under
  that lid. They come from the same `roof_geometry` module the shell is drawn from, so the roof you
  hit is the roof you see.
* **Drop guards.** 13 of them, at the only four places in the house you could fall more than
  §70.5's metre with nothing in the way: the two balconies, the juliet, and the garage's storage
  loft. The shell draws an open balustrade at each balcony edge; collision uses one continuous OBB
  behind its physical outline so the player capsule cannot pass through the necessarily open visual
  gaps. An open side of an exterior cell otherwise has no wall by construction. The porch at +0.57
  and the terrace at +0.45 get none, which is the same metre deciding it.
* **Stair rails.** 24 pieces, §12.3's 0.95 m balustrade round every well, open where a flight
  climbs from that floor and closed where one passes *under* it — a rail with a gap over a flight
  is a gap you fall through. `HOUSE-00567` built them, `HOUSE-00618`'s twenty-minute bot is what
  asked for them, and the same task found two things behind them: a `y` portal's rect is the
  *visibility* opening and over-runs the flight, so cutting it whole out of the slab left 0.88 m of
  nothing between the main stair's last tread and the floor; and `visibilityHint: open` was read as
  "no walls", which took the house's front wall away at every storey of the main stair. An outer
  wall is now also in the list of whichever yard faces it (52 pieces), because a body walking the
  lawn is swept against the lawn's shapes and met nothing until the cell tracker changed its mind.
  **`house_shell_gen.py` still has the same `open_cell` line and the same hole in the drawn
  elevation**: what you walk into is fixed, what you see is not.
* **The stair ramps.** 9 closed wedges, 2 landing boxes and 1 stepped flight, placed from
  `layout.stairs.json`'s authored `footprint`/`run`/`shape` through the same `stair_geometry`
  module `house_shell_gen.py` builds the treads from.

**Outdoors, the ground is the collision** (`HOUSE-00774`, `HOUSE-00782`). The height field plus the
OBBs above, and *nothing else* -- an open exterior cell on the ground storey has no floor slab of
its own either, because a slab and a height field are two answers to how high the ground is and
they disagree by up to 0.30 m of invisible plinth: two open exterior cells abut on grass, so the boundary between them is not a
wall. It was one until this task — 86 pieces, 5 219 m² of invisible wall, 29 of them over five
metres tall — and the front lawn could not be walked to the side yard. §11.2's fences (91 pieces),
§11.4's kerbs (48) and cars (2), §11.1's garden structures (8) and the tree trunks (17) are what
stands on the lot instead, and `collision.bin` says per cell whether the ground belongs to it,
because the height field runs under the house as well as over the lawn.

**A hole belongs to both rooms** (`HOUSE-00568`). Per-cell lists are safe wherever a wall separates
two rooms, because the wall is in both lists and stops a body before it can reach anything behind
one. At a hole they are not: §16.4's lookup keeps answering with the cell a body came from until it
is 0.05 m past the boundary, so a body standing in a doorway is 0.35 m into a room nothing has
looked at. Before `HOUSE-00489` the main stair's first run was also 0.20 m past `L0_FOYER`'s
cased opening; its corrected east-lane position leaves a west-side floor approach. Each side of
every hole still carries the other side's shapes within **0.40 m** of the plane — the 0.30 m
capsule plus that hysteresis plus 50 mm — over the hole's width and the body's own height, indexed
by the part of them within reach so a borrowed slab does not size the borrower's grid.
`docs/collision-format.md` §4.1 is normative and `OpeningReachTests` is the guarantee.

**A rafter-bounded cell is not a box** (`HOUSE-00496`). §13.6's `yOverride` on an attic cell is
its MAXIMUM head-room -- `L3_STORE_W` declares +13.90 -- and the drawn shell built its skin to
that height while §12.1's roof runs +10.39 to +14.30. From the road this house was a flat-topped
box with its roof inside it: dropping every roof chunk moved a full front elevation by 170 pixels.
`HOUSE-00472` fixed the same misreading for collision and left the picture alone. The walls and the
outer skin are clipped to the roof now -- the surface is the lower envelope of its planes, so
"under it" is an intersection of half-spaces and a panel stays one polygon -- and each attic cell
draws the roof's UNDERSIDE over its own boxes, because the roof itself belongs to the outdoors and
a room cannot see it. `verify_shell` measures it: nothing a rafter-bounded cell draws stands over
the roof above it.

**A dormer is a hole in the roof** (`HOUSE-00490`). §12.1's five come through the slope, and the
plane was left whole under them: the roof ran across the inside of every dormer window, and the
dormer's own front gable covered the rest of it. §64.6 measured `L3_ROOM` -- "lit by three dormers"
-- at a sky exposure of 0.000. The footprint is cut out of the plane and the dormer's eight faces
stand in the hole, with the window open between them; `roof_geometry.py` owns both, so what is
drawn and what is collided are one shape. The roof's plan area is the same afterwards, which is
what says the dormer covers exactly what it removed.

**A wall of an open cell stops at the roof of what it is a wall of** (`HOUSE-00784`). The same
rule as the boundary between two yards, one axis over: an open cell's extent is §10.3's +20.00
ceiling because that is how much sky it holds, and building the shared wall over the CELL's extent
rather than the neighbour's left 1 053 m² of collision standing in open air — over the sunroom's
roof, the garage's and the shed's, the last of them a 17.65 m overhang. A body is 1.8 m tall, so
nothing could walk into any of it and nothing had found it; `HOUSE-00779` did, casting a ray at the
sky from the terrace. The roof line is the tallest SOLID cell against that stretch of the face, at
any level: the storey above a balcony is on a different level and still holds its wall up, and the
balcony itself is outdoors and does not.

The census: **2 081 cell references over 1 335 shapes** — 1 262 OBBs and 73 meshes, of which 334
are what stands outdoors and 106 are floors, down from 140 when the yards stopped having them.
The wall count fell from 888 to 735 when the yards stopped being walled and to 726 at the roof
line; the ceiling count rose from 94 to 106 when `HOUSE-00777` gave the shed and the five stair
cells their lids back, and the mesh count from 22 to 73 when `HOUSE-00490` put the five dormers
in — 24 clipped rafter pieces where there were 13, and 40 dormer faces.

### 49.3 The player sweep

```
fixed step dt = 1/120 s, accumulated from GameTime, max 4 steps per frame
1. integrate desired velocity (input, gravity, wind for open doors)
2. for iteration in 0..2:
       hit = SweepCapsule(position, velocity·dt)
       if none: position += velocity·dt; break
       position += velocity·dt·hit.t·0.999
       velocity  = velocity − normal·dot(velocity, normal)      // slide
       remaining time reduced by hit.t
3. GroundProbe: a short downward sweep; sets onGround, groundNormal, surfaceKind, cellId
4. StepUp: if blocked horizontally and a 0.22 m raised sweep is clear, lift and retry once
5. Depenetrate: 4 iterations of 0.02 m push-out along the deepest overlap normal
```

Determinism: fixed step, no floating-point accumulation across frames beyond the accumulator, and
no dependence on frame rate. A unit test replays a recorded 60-second input sequence and asserts
the final position matches a stored value bit-for-bit at 30, 60 and 144 FPS.

### 49.4 Dynamic obstacles

* **Doors**: an OBB following the leaf's animated transform, updated per frame, present in the
  cell's dynamic collision list. A door swinging into the player pushes them (the door's motion is
  authoritative; the player is depenetrated). A door blocked by the player stops at the contact
  angle and the interaction reports "blocked".
* **The garage door**: 5 segment OBBs.
* **Pets**: capsules; the player can push them gently aside, and they yield with a step-away
  behaviour rather than being shoved through a wall.
* **Nudgeable props** (12 of them — a football, a stool, a waste bin, a few boxes): a sphere or
  box with gravity, a support-plane resolve, linear+angular damping, and a push impulse from the
  player capsule. They never stack and never sleep-fail because they are always resolved against
  static geometry only.

### 49.5 Guarantees tested

`HOUSE-00612`–`HOUSE-00618` assert: the player cannot pass any closed door, window, wall, floor or
ceiling; cannot leave the property except through the gates; cannot fall through any floor in
2 000 randomised drops; can traverse every flight in both directions; can pass through every
doorway (a capsule-clearance proof over all 186 portals); and never ends a frame inside static
geometry.

---

## 50. Interaction architecture

> **Removed from scope 2026-09-21. Design history, not a requirement: objects are static dressing, with no targeting, prompts or behaviours.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

### 50.1 Targeting

```
ray = Ray(eyePosition, viewForward)                       // XNA Ray
candidates = interactables in cameraCell
           ∪ interactables in cells one open portal away
           ∪ interactables whose bounds intersect a 2.6 m sphere at the eye
for each candidate:
    t = Ray.Intersects(candidate.bounds)                  // XNA BoundingBox/BoundingSphere
    if !t or t > candidate.maxDistance: continue
    if occluded by static geometry between eye and hit:   continue     // one short raycast
    score = t + 0.9 · angularDeviation(ray, candidate.focus)
best = min score
```

Two acceptance modes per interactable:

* **Precise** (small objects: a light switch, a mug, a tap) — the ray must hit the bounds.
* **Generous** (large objects: doors, the fridge, the garage door) — the ray must merely pass
  within a cone, and the player must be within `focus.radius` and roughly facing it.

A 0.15 s **stickiness** keeps the current target while it remains valid and within 12° of the
ray, so a prompt does not flicker between two adjacent switches.

### 50.2 Prompt

A single line at the lower centre: `[E]  Open refrigerator`. When a second action exists:
`[E] Open   ·   [F] Take milk`. Fades in over 0.12 s and out over 0.2 s. The verb text comes from
the action's `verb` plus the interactable's display name, both data.

### 50.3 Action dispatch

```
InteractionSystem::Perform(target, actionIndex):
    action = target.actions[actionIndex]
    if !EvaluatePredicate(action.when, target.state): report "blocked"; return
    if target.busy: report "busy"; return
    ApplyEffect(action.do, target.state)          // mutates typed state
    StartAnimation(target, action.anim, action.duration)
    PlaySound(action.sound, target.audio.emitter)
    RaiseEvent(InteractionPerformed{target.id, action.verb})
    MarkDirtyForSave(target.id)
```

Cancellation: an action with a `duration` can be interrupted by moving away beyond
`focus.radius · 1.5`; the animation eases back and the state is not committed for actions marked
`atomic: false`. Door swings are not cancellable once started (they are physical).

### 50.4 The twelve behaviours

There are **640 interactables and 12 C++ classes**. Everything else is data.

| Behaviour | Count | State |
|---|---|---|
| `DoorBehaviour` | 62 | `openFraction`, `latched`, `locked`, `swingSign` |
| `WindowBehaviour` | 62 | `openFraction`, `blindFraction` |
| `LightBehaviour` | 84 groups | `on`, `dimmer` |
| `ContainerBehaviour` (drawers, cabinets, wardrobes, the fridge, the freezer, the oven, the bin) | 214 | `openFraction`, `contents[]` |
| `FaucetBehaviour` (taps, showers, the hose, the outdoor tap) | 17 | `flow`, `hotFraction`, `fillLevel` |
| `ToiletBehaviour` | 13 | `urine`, `solids`, `lidOpen`, `seatUp`, `flushing` |
| `TelevisionBehaviour` | 3 | `on`, `channel`, `volume`, `playhead` |
| `GarageDoorBehaviour` | 1 | `openFraction`, `motorRunning` |
| `GateBehaviour` | 3 | `openFraction`, `bolted` |
| `ApplianceBehaviour` (oven, hob, washer, dryer, dishwasher, microwave, kettle, coffee machine, fireplace) | 11 | `on`, `programme`, `progress`, `doorOpen` |
| `PickupBehaviour` (takeable items) | 148 | `taken`, `heldBy`, `worldTransform` |
| `SeatBehaviour` (chairs, sofas, the bed, the toilet seat) | 22 | `occupied` |

Adding a 641st interactable is a JSON row.

### 50.5 Carrying items

Deliberately minimal, to avoid the "accidental survival RPG" the brief warns against:

* The player can hold **one** item at a time, shown in the lower right of the view (a small
  first-person prop, not an inventory grid).
* `E` on a held item's original surface (or any surface with a `placeable` tag) puts it down there.
* `G` drops it where you stand.
* There is no stacking, no count, no crafting, no use-on-target beyond the handful of
  interactables that accept one (put the milk in the fridge; put the mug on the table; put the
  ball in the dog's bed).
* Held items and their positions are persisted.

---

## 51. Doors

> **Superseded 2026-09-21. Every door, gate and the garage door has an authored static pose (`plan.md` M1, `HOUSE-03221`–`HOUSE-03224`); its collision and portal state follow the pose. Nothing opens at runtime.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

| Property | Value |
|---|---|
| Types | interior hinged (46), interior double (6), exterior hinged (5), slider (3), garage sectional (1), gate hinged (2), gate sliding (1) |
| Swing | 95° for interior, 100° for exterior; hinge side and swing direction are data |
| Duration | 0.85 s open, 0.75 s close, `smoothstep` easing; doubles open together with a 0.08 s offset |
| Collision | an OBB following the leaf; a blocked door stops at the contact angle |
| Portal | `apertureFraction = openFraction`; closed below 0.05, open above 0.08 (hysteresis) |
| Sound | latch click on release, hinge creak scaled by speed (three creak variants, one silent for the newly-oiled doors), a soft thud on close, a hard slam if wind-driven |
| Persistence | `openFraction` quantised to 1/64, `latched`, `locked` |
| Locking | The front door, the rear slider, the garage side door and the rear gate can be locked from inside. The player starts with the house key; a locked door reports "Locked" and rattles. No lockpicking, no lost keys. |
| Wind | A door open > 0.2 in `windSpeed > 9 m/s` accelerates toward closed and can slam |
| Self-closing | The garage↔mudroom door has a closer: it returns to closed over 3.5 s |

Interior doors that have **no** door in reality (cased openings between the foyer, hall, living,
dining, kitchen and family rooms) are portals with `aperture: null` — always open. That is the
architectural truth and it also gives the visibility system its permanent through-lines.

---

## 52. Windows

> **Superseded 2026-09-21. Windows stay closed: glass portals pass vision and never the player.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

| Property | Value |
|---|---|
| Openable | 62 of 81 |
| Mechanism | double-hung (lower sash rises, 0–0.55 m), casement (swings out 0–75°), slider (0–1.2 m), hopper (basement, 0–35°) |
| Duration | 1.1 s, with a sash rumble sound scaled by the sash's mass class |
| Collision | The opening is never passable by the player, open or closed — the sash opening is too small for a body in every case, which is both true and convenient. The *sill* is collidable. |
| Portal | `opacity: glass` — vision always passes (depth-capped), the player never does |
| Sound | opening a window raises the exterior ambience for that cell by `aperture · windowArea / cellArea`, capped, and cross-fades the dull variant toward the bright one. Rain, wind and thunder all follow. |
| Weather | An open window in rain with `windSpeed > 6 m/s` and a favourable orientation admits a small spray decal on the sill and a wet patch that dries over time. No water simulation. |
| Blinds and curtains | **Planned as a later feature** (phase 45). Each openable window has an optional `blindFraction`; the data and the portal's `translucent` mode already support it. When implemented, a closed blind sets the portal to `translucent`, cuts `transmission` to 0.05 and darkens the room. Explicit tasks exist; nothing else has to change. |
| Persistence | `openFraction` quantised to 1/64, `blindFraction` |
| Fixed windows | The 19 fixed/obscured/louvre windows are portals but not interactables |

No air-flow, temperature or humidity simulation. An open window changes light, sound and a splash
decal. That is the right scope.

---

## 53. Lights and switches

> **Partly superseded 2026-09-21. Light groups, fixtures and the illumination model stand. Switch plates and player switching are removed; lights follow the automatic schedule (`HOUSE-03401`) and the console.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

* **128 switch groups** control **220 fixtures** inside the house; **138 groups and 254 fixtures**
  counting the exterior. A group is what a real wall switch controls — the four kitchen
  down-lights, the two porch lanterns, the single closet bulb. Corrected 2026-09-08 from "84
  groups, 169 fixtures", which §13's own per-room column never added up to (`HOUSE-00382`).
* Switch plates are interactables placed at 1.20 m on the correct side of each doorway
  (data-driven, validated for reachability by §15.7 rule 11). Multi-gang plates offer one action
  per gang: `[E] Kitchen lights   ·   [F] Under-cabinet`.
* Three-way switching: `L0_STAIR_MAIN`'s lights and the `L1_HALL` lights each have two plates; the
  group toggles from either. Data expresses this as a group with two switch props.
* Some groups have no wall switch: table and floor lamps are switched by interacting with the lamp
  itself; the fridge interior light is driven by the door; the porch lantern, rear-terrace lanterns
  and street lights have a dusk sensor with a per-fixture random offset. A group may not combine
  an automatic dusk owner with a wall plate (`HOUSE-01291`).
* Turning a group on: the baked `LM_ART_<group>` pass fades in over 0.12 s (a filament ramp for the
  warm bulbs, instant for the LEDs, a 0.4 s flicker-start for the garage fluorescent), the
  fixture's emissive material lights, its glow quad appears, dynamic objects in the room pick it up
  as a directional light, and the 2-hop flood brightens the neighbours. A switch click plays at the
  plate's position.
  `layout.lights.json` authors this as `bulbClass: filament | led | fluorescent`; omission is the
  warm-filament default and a validator forbids mixed classes inside one baked group. One
  deterministic transition output drives the atlas, room state and borrowed light. Initial/load
  state snaps to its authored value; only a live off→on edge plays the start-up, so loading a night
  exterior does not replay every street lamp.
* The formal living room's narrow `LG_L0_LIVING_PIANO` accent starts on for the visual-slice
  arrival state (`HOUSE-01045`); its main ceiling group remains off. This is an authored switch
  state, not an exposure override, and saved state can still turn either group on or off.
* Persistence: one bit per group plus a dimmer byte for the 6 dimmable groups.
* Exterior lights: 4 porch sources (2 wall lanterns and 2 semi-flush ceiling fixtures), 4 facade
  uplights, 2 garage carriage lanterns, 3 driveway-border bollards, 1 independent manual garage
  flood, 2 rear terrace lights, 4 front-walk lights and 1 shed light, plus the 9 street lights and
  the neighbours' 4 porch lights (not player-controlled). The driveway-border fittings sit in a
  narrow mulched strip inside `EXT_SIDEYARD_E`; their 2,700 K / 160 lm downward spots share
  `LG_EXT_DRIVEWAY_EDGE`, follow the dusk sensor and explicitly spill only to `EXT_DRIVEWAY`.
  They do not change or automatically enable `LG_EXT_DRIVEWAY_FLOOD` (`HOUSE-00945`).
  The two rear-terrace sources are physical bronze/opal wall lanterns on the sunroom at x =
  ±2.50 m. Their 2,700 K / 1,600 lm / 8.5 m points follow the dusk sensor, bake only onto the
  selected adjacent `L0_SUNROOM` outer skin and spill only to `EXT_BACKYARD`; they have no wall
  switch (`HOUSE-01291`).
  The neighbours' porch lights are two per facade on N1 and N2, placed by `HOUSE-00391` with the
  houses they hang on. The **balconies** are missing from that list and §13.4 gives them three
  groups — two wall
  lanterns and a festoon on the rear balcony, one lantern on the front. They are `L1` rows, so
  `HOUSE-00382` authored them with the rest of `L1`.

---

## 54. Cabinets, drawers and closets

> **Removed from scope 2026-09-21. Design history, not a requirement: cupboards, drawers and wardrobes are static and stay closed.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

**Policy** — stated once so it is consistent, because "which drawers open?" is exactly the kind of
question that produces inconsistency:

1. **Every drawer and door in the kitchen, the butler's pantry, the pantry, the bathrooms, the
   mudroom, the laundry, the workshop bench and the garage shelving opens.** These are the rooms
   where a player expects to poke around, and where the contents are interesting.
2. **Every wardrobe and closet door opens**, including the walk-ins (which are cells with portals,
   not containers).
3. **Bedside tables, desks, sideboards, dressers, the media unit and the bathroom cabinets open.**
4. **Purely decorative case goods do not open**: display cabinets with glass fronts, the piano
   lid, the china cabinet's lower doors, built-in bookcases. They are visibly not-openable (no
   handle, or a lock plate) so the player never tries and fails.
5. If a container opens, it **has contents** — never an empty grey box. Contents come from a small
   set of "fill kits" (`FILL_CUTLERY`, `FILL_TOWELS`, `FILL_TOOLS`, `FILL_TSHIRTS`,
   `FILL_PAPERS`, …) placed procedurally with per-instance jitter, so 214 containers do not need
   214 hand-authored interiors.

Mechanics: drawers slide `depth × openFraction` over 0.55 s; cabinet doors swing 100° over 0.6 s;
the fridge and freezer doors swing 105° over 0.9 s with a gasket sound. Contents are drawn only
when `openFraction > 0.15` (a container is a tiny sub-cell with its own portal, so this falls out
of the visibility system rather than being a special case). Persistence: `openFraction` quantised
to 1/32.

Counts: 214 containers — 62 kitchen, 18 butler's/pantry, 34 bathroom/WC, 22 bedroom, 16 laundry/
mudroom, 28 garage/workshop, 20 living/dining/office, 14 basement/attic.

---

## 55. Kitchen and refrigerator

> **Removed from scope 2026-09-21. Design history, not a requirement: the kitchen and its appliances are static dressing.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

### 55.1 The kitchen

42.6 m², an island, a range with a hood, double wall ovens, a large refrigerator-freezer, a
dishwasher, a farmhouse sink with a mixer tap and a filter tap, a microwave, a kettle, a coffee
machine, a toaster, 62 cabinet doors and drawers, a walk-in pantry with a chest freezer, and a
butler's pantry with a second sink and a wine fridge.

Interactables: the range (4 burners with visible flame/glow and a hob-on hum), the ovens (door,
light, timer, a warming interior glow), the dishwasher (door, rack pull-out, a 3-stage wash
programme with real audio), the microwave (door, start, turntable, a ding), the kettle (fill from
the tap, switch on, a boil that builds over 45 s and produces steam particles, a click when it
finishes), the coffee machine, the toaster, the extractor hood (3 speeds), the bin (pedal, lid),
the taps (§56), and all 62 containers.

### 55.2 The refrigerator

| Aspect | Design |
|---|---|
| Structure | A prop with two doors (fridge, freezer), each an animated rigid part; the **interior is a cell** (`CELL_FRIDGE_INTERIOR`, `CELL_FREEZER_INTERIOR`) connected by a portal whose aperture is the door's `openFraction` |
| Not rendering the interior when closed | Falls directly out of the portal system — a closed opaque door closes the portal, so the 24 interior items, the shelves and the interior light are not submitted at all. No special case. |
| Interior light | A light group switched by `openFraction > 0.08`, casting a cold wedge onto the kitchen floor (a sun-patch-style decal) and lighting the interior |
| Contents | 24 items: milk, juice, eggs, butter, cheese, yoghurt ×3, leftovers ×2, vegetables ×5, fruit ×3, condiments ×4, a cake, a bottle of wine, a jar. Each is a `PickupBehaviour` |
| Taking | `[F] Take milk` while the door is open. The item leaves the fridge, is held, and can be placed on any `placeable` surface or put back |
| Freezer | 8 items; a visible frost overlay material; a colder light |
| Compressor | A looping hum emitter that cycles on for 6–9 simulated minutes every 20–30, louder with the door open, with a start clunk and a stop sigh. It is one of the house's signature sounds. |
| Door alarm | Left open for 40 real seconds, a soft repeating beep |
| Persistence | `doorOpen` per door, plus the set of removed item ids and where they now are |

### 55.3 Scope discipline

There is no hunger, no nutrition, no cooking simulation, no spoilage, no recipes. Food can be
taken out, carried, put down and put back. The oven can be turned on and gets warm and glows. That
is the whole model, and it is deliberate.

---

## 56. Water and plumbing

> **Removed from scope 2026-09-21. Design history, not a requirement: fixtures are static.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

### 56.1 Fixtures

**26** water outlets: kitchen mixer + filter tap, butler's-pantry tap, **13 bathroom/WC basin
taps** (each hot+cold or a mixer), 3 bath taps, **4 showers**, the laundry sink, the basement
laundry tub, the outdoor tap on the east elevation (with a hose reel).

> Corrected 2026-09-08 by `HOUSE-00412`, which authored them. This said "17 … 5 basin taps … 3
> showers", and §13 gives a basin to each of the seven WCs and each of the five bathrooms with a
> **double** vanity in the master — thirteen basin taps before a bath, a shower or a sink — and a
> shower to the master, `L1_BATH2` (over the bath), `L1_BATH3` and `L2_BATH4`, which is four. The
> three baths are right: the master's freestanding one, `L1_BATH2`'s and `L2_BATH5`'s.

### 56.2 Behaviour

| Aspect | Design |
|---|---|
| Interaction | `[E] Turn on water` / `[E] Turn off water`; on mixers, `[F]` cycles hot/cold and a scroll-equivalent adjusts flow in 4 steps |
| Flow visual | A tapered, slightly twisted cylinder mesh with a scrolling, vertically-stretched water texture, alpha-blended, plus a small additive highlight. Length is clamped to the basin depth. Tier E's `WaterFlow.fx` adds refraction and animated normals. |
| Impact | A ring of splash particles and a wet decal in the basin that fades when the flow stops |
| Basin water | The basin's own water plane rises only if the plug is in (`ContainerBehaviour`-like plug state on the 3 baths and the kitchen sink); otherwise it drains |
| Bath filling | **Implemented** for the 3 baths: `fillLevel` integrates at `0.012/s · flow`, the water plane rises with it, the sound changes as the bath fills (a rising pitch on the fill layer — a real acoustic effect and a very cheap one), and it drains at `0.030/s` with the plug out. Fill from empty ≈ 85 s. Worth the complexity: it is one of the most satisfying "the house works" moments and it is 60 lines. |
| Showers | A cone of falling particles plus a steam volume that builds over 20 s; the glass fogs (a material alpha ramp on the shower screen) |
| Audio | Per-fixture loop, `Apply3D`, with three flow-rate variants cross-faded; a distinct drain gurgle when the flow stops; water hammer (a single thump in the pipe run) when a tap closes fast; a faint whole-house pipe hiss in `B1` while any outlet runs |
| Persistence | **Yes** — `flow`, `hotFraction`, `plug`, `fillLevel` are all saved |

### 56.3 The "left the tap running" question

The brief asks for this to be analysed explicitly.

**Decision: a running tap keeps running across save and load, and the bath keeps filling — up to a
cap.** Rationale: the entire premise is "the house remembers how you left it". A tap that silently
turns itself off on load would be the single most conspicuous violation of that premise. It is
also the more interesting behaviour: coming back to find the bathroom tap still running and the
bath full is exactly the kind of consequence that makes a simulated house feel real.

The cap prevents absurdity: a bath fills to `fillLevel = 1.0` and then **overflows into the
overflow outlet** (a real fixture) rather than flooding the house. A basin with the plug in does
the same. There is no water damage, no floor flooding, no repair mechanic — the house is
plumbed correctly, and the consequence is a full bath, a running sound and a slowly rising water
bill nobody models.

Elapsed-time handling on load: the save records the simulated time. On load the fixture states are
restored as-is and simulation resumes; the game does **not** fast-forward the water. A bath left
half full is half full.

---

## 57. Toilets and waste-state simulation

> **Removed from scope 2026-09-21. Design history, not a requirement: toilets are static fixtures.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

Matter-of-fact, small, and proportionate. 12 toilets — seven WC-only rooms and the five
bathrooms, counted from §13's own room schedule (`HOUSE-00413`).

### 57.1 State

```cpp
struct ToiletState {
    float urine;        // 0..1  — tints the bowl water
    uint8 solids;       // 0..3  — count of solid meshes in the bowl
    bool  lidOpen;
    bool  seatUp;
    float flushPhase;   // 0 = idle, >0 = flushing
    float paperLeft;    // 0..1  — the roll depletes; cosmetic
};
```

### 57.2 Interactions

| Verb | Precondition | Effect |
|---|---|---|
| `Open lid` / `Close lid` | — | 0.5 s rotation, a plastic clack |
| `Raise seat` / `Lower seat` | lid open | 0.4 s rotation |
| `Urinate` | lid open | 6 s action; first person applies a slight downward camera tilt and a subtle vignette, no body animation; third person plays `sit_idle` or a standing pose. `urine += 0.35` |
| `Defecate` | lid open, seat down | 12 s action; third person plays `sit_down`/`sit_idle`/`stand_up`. `solids = min(3, solids+1)`, `urine += 0.2` |
| `Flush` | not already flushing | 2.2 s: the bowl water level dips and swirls (a rotating, scrolling texture on the water plane plus a level animation), solids and urine are cleared, then a 6 s cistern refill; audio: flush + refill |
| `Use paper` | after either | cosmetic; `paperLeft -= 0.05` |

### 57.3 Presentation rules

* **No anatomy is modelled or shown.** The first-person camera looks at the toilet; nothing else
  happens visually. The third-person avatar uses ordinary sit/stand clips.
* Solids are three small brown meshes with slight scale/rotation jitter, placed in the bowl.
* Urine tints the bowl water's `DiffuseColor` toward a pale yellow, proportional to `urine`, and
  adds a faint smell-free haze — no particles.
* The prompt text is plain: `[E] Urinate`, `[E] Defecate`, `[E] Flush`. No jokes, no euphemism,
  no reaction.
* An unflushed toilet is visible when the lid is open and **persists across save/load and across
  sessions**, exactly as the brief requires. It is not visible with the lid closed (the lid is
  opaque geometry; the bowl is inside its bounds and simply occluded — no special case).
* *Reset House* clears every toilet.

### 57.4 Scope

Total: one behaviour class, ~180 lines; two small meshes; one water-plane material; four sounds;
six state fields. Proportionate — roughly the same weight as the refrigerator, and far less than
the weather system, which is the correct balance.

---

## 58. Television and video

> **Removed from scope 2026-09-21. Design history, not a requirement: screens are static.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

### 58.1 Placement

Three televisions: `L0_FAMILY` (the main 55″ set, wall-mounted), `L1_MASTER_BED` (a smaller set on
the dresser), `B1_CINEMA` (a projector and a 2.4 m screen). All three share one behaviour.

### 58.2 The video backend (Linux)

```cpp
Video*        video  = content.Load<Video>("Video/broadcast_01");
VideoPlayer   player;
player.setIsLoopedProperty(true);
player.Play(video);
...
Texture2D* frame = player.GetTexture();          // per Draw
tvEffect.setTextureProperty(frame);              // BasicEffect, LightingEnabled = false
```

The frame texture is drawn on the screen quad as an unlit, emissive surface, with a slight
scanline/vignette overlay material to stop it looking like a pasted photograph. Audio is the
`VideoPlayer`'s own, routed through a spatial `AudioEmitter` at the TV's position by playing the
video muted and running a parallel `SoundEffectInstance` of the same track — **or**, simpler and
chosen, by letting `VideoPlayer` handle audio and applying a manual distance/portal gain to
`VideoPlayer::Volume` computed by the same room-aware model as everything else (§64). The latter
loses stereo panning but gains correct occlusion, which matters far more for a TV in another room.

The room's light group picks up the screen: a 4 × 4 downsample of the frame texture every 6 frames
gives an average colour that drives an emissive light group, so a dark family room genuinely
flickers with the picture.

### 58.3 The Web / Android fallback (BL-05)

`ITvSource` has two implementations:

| Implementation | Platform | Mechanism |
|---|---|---|
| `VideoTvSource` | Linux/macOS with FFmpeg | `Video` + `VideoPlayer` as above |
| `SequenceTvSource` | Emscripten, Android, or any build where `CNA_VIDEO_AVAILABLE` is absent | A pre-baked **frame-strip atlas**: 8 × 8 frames of 256 × 144 per **2048 × 1152** texture, 12 fps, 5.33 s per atlas, N atlases per "channel", advanced by a timer; audio is a separate looping `SoundEffect` kept in sync by the same timer |

**Two corrections to that row, both measured by `HOUSE-00219`.**

The atlas is **2048 × 1152, not 2048²**: 8 × 256 is 2048 and 8 × 144 is 1152, and rounding the
height up would leave 896 rows — 44 % of the texture, 7.3 MB of 16.8 MB — holding nothing, per
atlas, in a 25 MB pack. Non-power-of-two is safe here: OpenGL ES 3.0 requires it for a texture with
no mip chain and clamped addressing, and a 2048 × 1152 PNG was compiled through
`CNA.ImageImporter -> CNA.TextureProcessor` to confirm it.

**And the strip backend does not fit its pack as `.cnb`.** That compile produced **9 437 552
bytes** — exactly 2048 × 1152 × 4 plus a header — because CNB texture schema 1 is frozen to `Rgba8`
and a `textureFormat` asking for DXT is warned about and silently kept uncompressed
(`HOUSE-00111`). At 9.44 MB per 5.33 s atlas, §27.2's 25 MB `video` pack holds **2 atlases — 14
seconds of television, for four channels**. The same atlas as **DXT1 through `.xnb`** is 1.18 MB,
eight times smaller, and the same 25 MB then holds 21 atlases — **113 seconds**. So the frame-strip
backend is only viable through the `.xnb` texture route §27.2 already names for the `web` and
`android` content profiles, which is exactly the profile that needs `SequenceTvSource` in the first
place. Phase 21 must package these atlases as `.xnb` DXT1 or reduce the frame size; it cannot ship
them as `.cnb`.

`SequenceTvSource` is not a placeholder — it is a real, deliberately low-bandwidth television that
looks correct on a 55″ screen seen from 3 m in a game. It is also built and testable on Linux
(forced by a setting), so the fallback is exercised continuously rather than discovered broken on
the day of the Web port.

### 58.4 Content

Four "channels" of legally safe material: a weather-map channel and a clock channel generated by
our own tooling; a nature/landscape channel from CC0 stock footage; and a static/no-signal
channel. Everything is transcoded by `ffmpeg` to a CNA-supported container and recorded in the
manifest. **No broadcast, film or music content is used.**

### 58.5 State and persistence

`on`, `channel`, `volume`, and `playhead` (quantised to 1 s) are saved. Turning the TV on resumes
where it was, which is a small detail that reads as "the house remembers".

---

## 59. Furniture and decoration

> **Retained and raised in priority 2026-09-21. Everything here is static dressing; any mention of interactable, openable or usable furniture is void.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

> **Reduced 2026-09-21 (second amendment): furnishing follows `plan.md`'s tiered room recipes and its reusable kit. The density bands of §59.1 and the anti-repetition limits of §59.2 do not apply; reuse is allowed. §59.3's signs of habitation are required in hero areas only.** ([ADR-0015](docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md))

### 59.1 Density targets

| Cell class | Static props | Interactables | Dressing props |
|---|---|---|---|
| Major living rooms (living, family, kitchen, dining, master, games) | 45–90 | 12–70 | 30–60 |
| Bedrooms, library, office, sitting | 30–55 | 8–22 | 20–40 |
| Bathrooms, WCs | 18–30 | 6–14 | 10–20 |
| Corridors, landings, foyer | 10–22 | 2–8 | 8–18 |
| Closets, stores, pantry | 12–35 | 3–10 | 10–25 |
| Basement rooms | 20–60 | 4–14 | 15–40 |
| Attic | 25–70 | 2–6 | 20–50 |
| Garage | 55 | 12 | 40 |

Totals: **≈ 2 400 static placements, ≈ 640 interactables, ≈ 1 900 dressing props**, from ≈ 420
unique models.

### 59.2 Anti-repetition rules

* No model may appear more than **14** times in the whole house, and no more than **6** times in
  one cell — enforced by the validator, with a whitelist for genuinely repeated things (dining
  chairs, cabinet handles, books, fence pickets, floorboards).
* Every placement gets a deterministic per-instance jitter: yaw ±4°, uniform scale ±3 %, and a
  tint from the material's palette (±6 % value, ±4° hue).
* Six colourways exist for each of the eight most-repeated props.
* Books, papers, cushions and kitchenware are placed by **kit** generators that vary count,
  spacing, lean and colour from a seeded RNG, so two shelves never look identical.

### 59.3 Signs of habitation

The list that turns a showroom into a home, all cheap:

a coat on a hook and a scarf over a chair; muddy boots by the mudroom door and a dog towel
beside them; a half-read book face-down on the arm of the sofa; a mug and a coaster on the
coffee table; the newspaper on the kitchen island; a bowl of fruit, two of which are past their
best; a child's drawing on the fridge under a magnet; a chart of heights pencilled on the pantry
door frame; a laundry basket with clothes in it on the L1 landing; a phone charger cable trailing
from a bedside outlet; a made bed in the guest room and an unmade one in the teenager's; a games
console on the family-room floor with its controller on the sofa; a half-finished jigsaw in the
games room; a bicycle with one pedal off in the garage; a paint tin with a stirred lid in the
workshop; a broken lamp waiting to be fixed on the workbench; a school timetable pinned in the
kitchen; two toothbrushes in the master bathroom and one in each other; a cat bowl in the
mudroom and a dog bowl in the kitchen; a scratched patch on the door frame at cat height;
a stack of unopened post on the hall console; a Christmas box in the attic labelled in marker;
a spider's web in the far corner of `L3_STORE_W`.

Each of these is a placement row, and together they do more for believability than another
hundred generic props would.

### 59.4 Wall decoration

Every room has wall art appropriate to it: landscapes and abstracts in the formal rooms, framed
photographs on the hall gallery wall and the stairs, posters in the teenager's room, botanical
prints in the bathrooms, a corkboard in the office, a whiteboard in the games room, a mirror in
the foyer/bathrooms/master closet, clocks in the kitchen, hall and games room, floating shelves
with ornaments in the sitting room.

**The gallery wall** (`L0_HALL`, 9 frames, plus 6 on the stair wall): rendered fictional family
photographs of *the characters the game itself uses* (§20.4 route 3), posed in this house's own
rooms and rendered in Blender. We own them, they are consistent with the avatar the player
chooses at the level of "this is the family who live here", and no real person is depicted.

`HOUSE-01065` establishes the nine-frame `L0_HALL` composition before the avatar-dependent route-3
image set exists. Five physically layered frames occupy the west doorway bay and four occupy the
east stair-side bay; their original geometric silhouette reliefs are the explicitly permitted
§20.4 route 2 and depict no identifiable person. The frame dimensions, wall registration and
clusters are production geometry. The later route-3 pass may replace only the image-plane content
without moving the accepted composition. The six stair-wall frames remain part of the broader
`HOUSE-00987` scope and are not claimed by this bounded checkpoint.

Mirrors: 4 of them. Implementation is a **static cube map** per mirror baked offline
(`EnvironmentMapEffect`), which is correct for a fixed mirror in a fixed room and costs nothing.
The player's reflection is **not** rendered — a deliberate, documented limitation (D-19, §77),
because a real planar reflection would need a second render pass per mirror per frame and the
budget is better spent elsewhere. The mirrors are positioned so this is not conspicuous.
`MaterialBinder::BindEnvironmentMap` supplies that supplemental pass: the base material contributes
its tint, alpha and specular colour, while the draw supplies both its albedo and the placement-owned
baked cube plus blend amount, Fresnel factor and §31.5 fog. It is deliberately not a fifth primary
`effectTierS`: §22.2's glass and chrome retain their Basic pass and reflect in an additional pass.

### 59.5 Architectural furniture at shell interfaces

`HOUSE-01060` establishes the boundary for a static furniture suite that finishes existing shell
structure. The formal-living fireplace remains a prop owned by `L0_LIVING`; it does not replace,
rename or special-case the canonical chimney, room or appliance. Its local wall plane registers to
the chimney face while its support plane registers to the finished floor, so the asset-origin gate
checks both conditions. The visible 1.780 x 2.325 x 0.570 m composition supplies a projecting
hearth, stone surround, walnut mantel, recessed iron firebox/grate, unlit logs and original framed
relief through five existing stock-`BasicEffect` roles. A 12-triangle proxy covers only the low
projecting hearth: the existing shell already closes the tall body, and a second wall-height proxy
would create an artificial recovery pinch. Live embers, smoke, audio and interaction remain the
separate `HOUSE-02691` concern.

`HOUSE-01061` applies the same data-owned boundary to room-specific soft furnishing. The approved
rug mesh remains shared by living, family and dining; only `PROP_LIVING_RUG` uses a canonical
material override, `MAT_LIVING_RUG_WOOL`, which reuses the approved fabric weave/normal at 7x UV
scale and a low-specular oatmeal tint. The family and dining placements retain charcoal. A palette
choice therefore neither duplicates geometry nor becomes a room-name branch in runtime code.

`HOUSE-01062` adds a second bounded soft-furnishing pattern: a project-authored throw can be
tailored to a licensed hero sofa without modifying or copying that sofa. Its curved surface,
12 mm solidified thickness, folds and nine physical fringe cords remain one collision-free static
prop; the placement owns contact with the camera-near arm. The dedicated pale-blue wool role reuses
approved weave maps but does not make every use of that shared texture the same colour. The
deterministic preparation gate pins the 1,460-triangle geometry, component names, source hash,
material role and placement, keeping this close-range dressing out of runtime special cases.

`HOUSE-01063` treats a wall bay and its nearby coffee table as one restrained visual composition
while keeping their data ownership separate. The 3,448-triangle paired botanical relief registers
to the real south-wall plane; the 2,016-triangle books/tray/bowl vignette registers to the measured
1.019303 m table top. Both are collision-free static props and neither changes circulation or
runtime logic. Existing walnut, brass, canvas, book-binding and stone roles are reused. The relief's
cream paper is deliberately a new stock-`BasicEffect` role: close prop geometry has UV0 but no UV2,
so borrowing a shell-paint `DualTextureEffect` role would falsely require a lightmap channel. The
preparation gate regenerates both GLBs byte-for-byte and pins their hashes, bounds, components,
material slots and exact supporting planes.

`HOUSE-01064` extends that bounded surface-dressing contract across the foyer-to-hall arrival.
One 1.140 x 0.0205 x 3.960 m woven runner stays collision-free and leaves at least 1.20 m clear on
each side of the long hall; one paired 3.920 x 1.050 x 0.10876 m relief occupies only the two end-
wall flanks around the unchanged kitchen portal; and one 0.940995 x 0.840 x 0.3242 m vase,
photograph and key-tray group rests on the existing foyer console's exact 1.207883 m top. These
remain three data-owned props rather than one runtime room branch. Close geometry uses UV0 with
three stock-`BasicEffect` ceramic/wool roles; existing walnut, brass, canvas and paper roles are
reused. The preparation gate builds explicit smooth ellipsoids instead of Blender operator-
generated custom-normal data, so all three GLBs regenerate byte-for-byte while preserving scale,
component, material, placement, wall-contact and circulation checks.

`HOUSE-01067` applies the same data-owned soft-furnishing contract to the two dominant family-room
picture windows. One 3.040 x 2.463002 x 0.153991 m / 6,084-triangle project-authored treatment is
reused on the exact north and east wall planes. Each instance supplies two solidified gathered
panels, ten header tabs, a steel rod, finials, brackets and restrained fabric-covered holdback
rosettes while leaving a 2.06 m clear centre. The oatmeal wool-linen role reuses approved stock-XNA
weave maps at measured scale; hardware reuses canonical steel. Both placements are collision-free
and preserve glass, daylight, portals, views and circulation. They deliberately represent only the
authored new-game open state: phase 45 still owns `blindFraction`, movement, light transmission and
interaction. The preparation gate regenerates the GLB byte-for-byte and pins its hash, bounds,
triangles, UV0, named components, material roles, support/wall origin and both world transforms.

`HOUSE-01068` extends the same bounded data-owned approach into the visible sunroom without
claiming completion of the dependency-blocked bulk furnishing task. One deterministic
2.387063 x 0.945 x 2.387063 m / 4,324-triangle model composes a round oak table, four woven rattan
chairs, cushions and table setting. A separate 2.43 x 2.64 x 0.675 m / 4,208-triangle fitted wet
bar supplies shaker fronts, stone worktop, tiled upstand, sink/faucet, two dressed shelves and
restrained drinkware. Two approved plant instances finish the corners. The breakfast and bar
placements preserve the kitchen-to-terrace circulation line and the authored cat perch; their
named proxies enter ordinary collision and navigation generation. Two new stock-`BasicEffect`
rattan/cushion roles reuse approved project textures, while all remaining wood, stone, tile,
metal, ceramic, glass and foliage roles are canonical. The preparation gate pins byte
regeneration, hashes, dimensions, triangle counts, UV0, components, roles, proxies, origin/scale,
support planes and all canonical placements.

---

## 60. Dog

> **Removed from scope 2026-09-21. Design history, not a requirement.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

### 60.1 The asset

A medium-sized, realistically proportioned dog — a Labrador-retriever type, 0.60 m at the withers,
1.05 m nose to tail base. Rigged, 38 bones, 8 clips. This is a **hero asset** with an explicit
research task, an acceptance bar and a visual sign-off (§19.3). A malformed generated dog is not
acceptable at any point, including as a temporary placeholder — the placeholder is a plain grey
box labelled `DOG`, which is honest, rather than a bad dog, which is not.

`HOUSE-00291` exhausted R-01's three named sources. Sketchfab's plausible Labradors did not have
CC0 hero-grade provenance or the clip set; Blend Swap's CC0 match was a low-poly Jack Russell; and
Quaternius's animated pug was both the wrong style and covered by contradictory redistribution
terms. The decision is therefore to author the model, textures, 38-bone rig and eight clips in
Blender through `HOUSE-02041`–`HOUSE-02043`, with a five-day art allowance, then prove the real
pipeline in `HOUSE-02044`. No downloaded animal model is used as a base.

### 60.2 Behaviour

A utility-scored state machine, updated at 10 Hz:

| State | Score drivers |
|---|---|
| `Sleep` | night, low activity, on its bed |
| `Lie` | rested, warm room, near the player |
| `Sit` | moderate, near the player, after being looked at |
| `Idle` | default |
| `Wander` | boredom accumulates when nothing happens |
| `LookAtPlayer` | the player is within 6 m and moving |
| `ApproachPlayer` | the player entered the room; affection accumulates |
| `FollowPlayer` | the player has moved through 2+ cells recently |
| `Alert` | doorbell, a door opening, thunder, the cat running |
| `Bark` | alert, high arousal; 1–3 barks then decays |
| `DrinkEat` | at the bowl, on a slow timer |
| `Scratch` / `Shake` / `Yawn` | idle fidgets, low probability |

Arousal, affection and boredom are three floats with decay; the state with the highest score wins,
with a minimum dwell of 1.5 s and a small hysteresis so it does not twitch between states.

### 60.3 Navigation

A **waypoint graph** (`layout.nav.json`): nodes at every doorway centre, every room centre, and
2–6 additional nodes per room chosen to avoid furniture; edges between mutually visible nodes
within a cell and across portals. A* with a portal-aperture check, so a dog cannot path through a
closed door — it goes to the door and waits, or whines, which is exactly right. Steering is
seek-with-arrival plus obstacle avoidance against the cell's collision OBBs; the dog is a capsule
(r 0.22, h 0.60) and is genuinely collided, so it never walks through walls or furniture.

Stairs: the dog uses the stair ramps with a dedicated `trot_stairs` clip and a slower speed.
Doors: a closed door blocks it; an open door it walks through; a door that closes while it is in
the doorway makes it back out.

### 60.4 Audio

`AudioEmitter` at the dog's head. Bark (4 variants), whine, growl (rare, at thunder), pant (a loop
whose gain follows recent movement), a collar-tag jingle synchronised to its footsteps, nail
clicks on hard floors (surface-aware, like the player's), a sigh when it lies down, and eating and
drinking sounds. All `Apply3D`, all subject to the room-aware model — a dog barking in the
basement with the door shut is a muffled distant sound, which is a genuinely lovely detail.

### 60.5 Persistence

Cell, position, yaw, current state, state timer (quantised to 0.1 s), the three utility floats,
and its bed location. Not animation frames — on load it starts its saved state's clip from the
beginning, which is imperceptible.

---

## 61. Cat

> **Removed from scope 2026-09-21. Design history, not a requirement.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

The same machinery, different parameters and clips, plus one addition.

`HOUSE-00292` exhausted R-02's three named sources. Sketchfab and Blend Swap did not provide a
realistic, hero-provenance cat with the required clips, while Quaternius's low-poly cat also carries
the source's contradictory redistribution terms. The model, textures, 34-bone rig and eleven clips
will therefore be authored in Blender through `HOUSE-02091`–`HOUSE-02093`, then proved in the real
pipeline by `HOUSE-02094`. No downloaded animal model is used as a base.

| Aspect | Cat |
|---|---|
| Size | 0.25 m at the shoulder, 0.46 m body, 4.2 kg |
| Bones | 34 |
| Clips | `idle`, `sit`, `lie`, `groom`, `walk`, `trot`, `jump_up`, `jump_down`, `stretch`, `knead`, `meow` |
| States | `Sleep`, `Lie`, `Sit`, `Groom`, `Idle`, `Wander`, `WatchPlayer`, `Approach`, `Avoid`, `Perch`, `Knead`, `Meow`, `Hunt` |
| **Perches** | The addition: `layout.nav.json` declares 22 **perch points** — the L1 landing window seat, the top of the kitchen cabinets, the back of the living-room sofa, the office desk, the master bed, a warm spot on the mechanical-room duct, three windowsills, the stair newel post, a shelf in the airing cupboard. Reaching a perch plays `jump_up`, leaving plays `jump_down`. A cat that is *on* things rather than beside them is most of what makes it read as a cat. |
| `Avoid` | Unlike the dog, the cat has an avoidance drive: if the player approaches quickly it moves away, and it prefers to keep 1.5 m unless affection is high |
| `Hunt` | Very occasional: stalks a fixed "prey point" (a spider in the basement, a bird outside a window), crouches, twitches its tail, then loses interest |
| Audio | Meow (5 variants), chirrup, purr (a loop that starts when the player is within 1 m and affection is high), a hiss at the dog, soft paw-falls on hard surfaces only, a claw-scratch on the marked door frame |
| Dog interaction | A shared 3 m awareness: the cat leaves a room the dog enters unless it is on a perch, in which case it stays and looks down. The dog occasionally follows the cat. Two lines of code, and it makes both animals look like they know each other. |
| Persistence | Same fields as the dog, plus the current perch id |

---

## 62. Audio

> **Partly superseded 2026-09-21. Footsteps, room tone, exterior ambience, weather and a few static positional loops are retained. Interaction, appliance, plumbing, pet and television sounds are removed.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

> **Reduced again 2026-09-21: footsteps on six broad surface categories (not the 20 of §62.4), one interior tone, exterior day and night beds, rain and wind. The static positional loops and the house's own sounds of §62.6 are cut.** ([ADR-0015](docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md))

### 62.1 Architecture

```
AudioSystem
 ├── ListenerState        AudioListener at the camera; forward/up from the view; velocity ZERO
 ├── EmitterPool          up to 64 logical emitters, 32 concurrent SoundEffectInstances
 ├── VoiceManager         priority + virtualisation + per-category limits
 ├── PortalPathSolver     per-emitter gain and muffle from the portal graph (§64)
 ├── AmbienceDirector     room tone, weather layers, exterior beds, cross-fades
 ├── FootstepDirector     surface-aware, cadence-driven, round-robin
 └── MusicDirector        none — the house has no score. Deliberate.
```

There is **no music**. The soundtrack of this house is the house: the fridge, the furnace, the
rain, the clocks, the pets, the creaks. Adding a score would work against everything else.

### 62.2 What CNA gives, and what it does not

CNA's mixer implements a simplified 3-D model: pan, distance attenuation and Doppler on a stereo
mixer. It does **not** implement the speaker matrix, cones, custom curves, LFE, HRTF or reverb
sends (`cna_audio_deep_audit_2026-07-17.md:161`). Doppler carries a documented up-to-4× pitch
inflation risk if velocity units are wrong (same doc, A-09).

Consequences, all decided here:

* `SoundEffect::DopplerScale = 0` and both listener and emitter velocities are left at zero by
  default. Nothing in a house needs Doppler; a passing car on the road is the only candidate and
  it is not worth the risk. A settings toggle enables it for the curious.
* `Apply3D` is used for **positional** sources (point-like: a tap, a door, the dog, the TV) and
  provides pan + distance. Everything above that — occlusion, muffling, room transitions,
  through-the-wall attenuation — is computed by `cna-house` and applied to `Volume` and to a
  bright/dull cross-fade (§64).
* No claim of physical acoustic simulation is made anywhere in the game, its documentation or its
  marketing.

### 62.3 Categories and mixing

| Category | Voice limit | Default volume | Notes |
|---|---|---|---|
| Ambience (room tone, weather, exterior) | 8 | 0.75 | Always 2-D or wide-stereo |
| World SFX (doors, switches, containers, water, appliances) | 14 | 1.00 | `Apply3D` |
| Footsteps | 3 | 0.85 | Player + 2 pets |
| Animals | 4 | 0.90 | `Apply3D` |
| Media (television, radio) | 2 | 0.70 | |
| UI | 2 | 0.60 | 2-D |
| **Total concurrent** | **32** | | |

`VoiceManager` sorts candidate sounds by `priority · audibleGain` and virtualises the rest (they
keep their playhead but do not occupy a voice), so a burst of hail impacts never starves the
door you just opened.

### 62.4 Footsteps

* Surfaces: `hardwood`, `carpet`, `tile`, `concrete`, `stair_wood`, `stair_carpet`,
  `stair_wood_open`, `grass`, `gravel`, `asphalt`, `bluestone`, `soil`, `mud`, `snow`, `water`,
  `metal`, `leaves`, `rock`, `sand`, `dirt`.
* Each surface has ≥ 6 walk, ≥ 6 run and ≥ 2 land variants, chosen round-robin from a shuffled bag
  so the same sample never repeats within 4 steps; pitch ±4 %, volume ±10 %.
* Cadence: a distance accumulator with a stride of 0.75 m (normal) / 0.95 m (fast); on stairs, one
  step per riser crossed.
* In third person the same accumulator drives the sound, so audio and animation stay locked
  regardless of camera mode; the clip's own foot-plant markers are used as a cross-check in
  development builds and a mismatch > 60 ms fails a test.
* The pets have their own lighter sets: nail clicks on hard surfaces only, soft pads on carpet.

### 62.5 Ambience

Per-cell **room tone** (a very quiet, looped bed) selected by the cell's acoustic profile:
`AMB_ROOM_LARGE`, `AMB_ROOM_SMALL`, `AMB_ROOM_HARD`, `AMB_BASEMENT`, `AMB_ATTIC`, `AMB_GARAGE`.
Cross-faded over 0.8 s on a cell change. Above that sit the weather layers, the exterior bed
(distant road, birds by time of day, neighbourhood dogs, a lawnmower on summer afternoons, an
occasional aircraft) and the mechanical layers.

### 62.6 The house's own sounds

Placed from the plumbing and HVAC diagram (§12.5), because that is what makes them coherent:

| Sound | Placement | Trigger |
|---|---|---|
| Furnace burner + blower | `B1_MECHANICAL` | On a thermostat cycle driven by outdoor temperature; audible through the ducts everywhere, loudest in `B1` and at the registers |
| Duct rumble and tick | at each register | 3 s after the blower starts and stops |
| Water heater | `B1_MECHANICAL` | On a slow cycle, and after hot water is used |
| Pipe hiss | the stack runs | While any outlet flows |
| Water hammer | the stack runs | A fast tap close |
| Refrigerator compressor | `L0_KITCHEN` | §55.2 |
| Chest freezer | `L0_PANTRY` | Slower cycle |
| Clocks | `L0_HALL`, `L0_KITCHEN`, `L2_GAMES` | A tick per simulated minute; the hall clock chimes the hour |
| House creaks | 14 placed points in the shell | Random, weighted by the rate of change of outdoor temperature — so the house ticks as it cools at dusk. A tiny detail that no one will consciously notice and everyone will feel. |
| Wind in the eaves | `L3_*` | ∝ `windSpeed` |
| Rain on the roof | `L3_*`, then attenuated downward | §37.5 |
| Doorbell | `L0_PORCH` | Never rings on its own; a debug command and one scripted event |
| Garage door motor | `L0_GARAGE` | With rail rumble and an end-of-travel clunk |

---

## 63. `/rv/tmp` audio-collection audit

### 63.1 What was found

**Location: `/rv/tmp/Essentials_Series_NOX_SOUND/`** (1.1 GB, 1 644 `.wav` files, plus the
original `Essentials_Series_NOX_SOUND.zip`).

This is the **NOX_SOUND "Essentials Series"** free library, distributed through
asoundeffect.com (the bundled `A_Sound_Effect_NOX_SOUND.url` points at
`https://www.asoundeffect.com/sounddesigner/nox-sound/`).

### 63.2 Provenance and licence

`Essentials_Series_README.pdf`, extracted verbatim:

```
Instagram – AsoundEffect – Freesound – Unity
All these sounds are under CC0 license.
```

**Assessment: the collection declares itself CC0 in the publisher's own README shipped inside the
distribution.** That is a strong, first-party licence statement and it is what the manifest will
record. It is **not** yet verified against the publisher's live licence page, because no network
access was used in this planning pass. `plan.md` `HOUSE-00276` re-verifies it against the
publisher's page and archives the evidence before any file ships; until that task closes, the
collection is marked `PROVENANCE DECLARED CC0 — VERIFY BEFORE SHIP` rather than
`PROVENANCE UNKNOWN`, which would be an unfair reading of a document that says CC0 on its face.

### 63.3 Inventory

Uniform technical format across the whole collection, spot-checked with `ffprobe`:
**`pcm_s24le`, 48 000 Hz**, mono or stereo as the filename says.
**This is 24-bit PCM, which CNA's `SoundEffectReader` does not list as an accepted format
(BL-06)** — every file must be converted to 16-bit PCM offline.

| Pack | Files | Size | Contents | Use in `cna-house` |
|---|---|---|---|---|
| `Footsteps_Essentials_NOX_SOUND/` | 479 | 37 MB | 12 surfaces × {Walk, Run, Jump/Land} + Foley: `DirtyGround` (26), `Grass` (94 incl. 9 foley), `Gravel` (26), `Leaves` (16), `Metal` (74), `Mud` (22), `Rock` (31), `Sand` (50), `Snow` (67), `Tile` (20), `Water` (29), `Wood` (24) | **Directly usable and excellent.** Covers grass, gravel, tile, wood, metal, snow, water, mud, rock, sand, leaves, dirt — 12 of the 20 surfaces the game needs |
| `Iceland_Packs_NOX_SOUND/Iceland_Footsteps_Pack/` | 210 | — | 70 walk / 70 run / 70 jump, outdoor Icelandic surfaces | Extra variation for the exterior surfaces; useful for avoiding repetition |
| `Iceland_Packs_NOX_SOUND/Iceland_Flows_NOX_SOUND/` | 23 | — | Sea, stream, waterfall loops (Seljalandsfoss, Skógafoss, Skaftafell, Vík, Reynisdrangar) | Not directly useful — no sea or waterfall in a suburb. Two of the light-stream loops may serve as a base layer for the **gutter/downspout** trickle |
| `Nature_Essentials_NOX_SOUND/` | 18 | 122 MB | `Ambiance_Rain_Calm_Loop_Stereo`, `Ambiance_Rain_Strong_Loop_Stereo`, `Ambiance_Wind_Calm/Forest_Loop_Stereo`, `Ambiance_Forest_Birds`, `Ambiance_Night`, `Ambiance_Cicadas`, `Ambiance_River/Stream/Waterfall/Sea`, `Ambiance_Cave_*`, `Ambiance_Fire/Firecamp_*` | **The single most valuable pack.** Rain calm + strong, wind calm + forest, birds, night, cicadas → the core of the weather and time-of-day ambience. Fire loops → the living-room fireplace and the garden fire pit. |
| `Electromagnetic_NOX_SOUND/` | 72 | 156 MB | Appliance and electronics hums and sequences: Computer (21), Macbook (7), Car dashboard/fan/front (11), Ipad (4), Keyboard (3), Neon (5), Nuc (1), Phone (4), Playstation (3), Router (3), Smartphone (6), Speaker (1), Electric (3), Engine (1), Fan (3) | **Very useful and unexpected.** The `Neon` loops are exactly the garage fluorescent; `Computer_Idle`/`Charger`/`Router` loops serve the office, the structured-wiring cabinet and the media unit; `Fan` loops serve the extractor hood and the bathroom fans; `Playstation` serves the games console |
| `Vehicle_Essentials_NOX_SOUND/` | 161 | 89 MB | Car (63): doors open/close interior+exterior, buttons, drive loops, handbrake, indicator, seatbelt, wipers, windows; Truck (98): similar | **Directly usable for the garage car**: door open/close, button clicks, the seatbelt, the indicator relay. The drive loops are unused (the car does not move) |
| `Voices_Essentials_NOX_SOUND/` | 657 | 120 MB | Male and female: Breath (101), Expressions (184), Effort (74), Jump/Land (77), Hit (75), Attack (55), Pain (39), Cough (27), Throat clearing (25) | **Partially usable.** Breath and effort for the player's exertion on stairs; cough and throat-clearing as very occasional idle sounds. Attack, hit and pain are irrelevant to this project and will not ship. |
| `Sample_A_Sound_Effect/` | 10 | 73 MB | A sampler: `Household_Door_Wood_Open_Stereo`, `Household_Closet_Key_Insertion_Stereo`, `Cloth_Coat_PickUp`, `Backpack_*`, `Atmosphere_*`, `Ambiance_Nature_Meadow_Birds`, `Ambiance_Nature_Rain_Calm_Leaves` | Small but well-aimed: **a wooden door open** and **a key in a lock** are both directly usable, and the meadow/rain-on-leaves ambiences are good outdoor beds |
| `S~o_Miguel_Flows_NOX_SOUND/` | 14 | 294 MB | Azores field recordings: hot springs, ocean at three locations | Not useful for a suburban house. **Excluded from the shipped set.** |

### 63.4 Coverage assessment — the gaps

| Category | NOX coverage | Gap |
|---|---|---|
| Footsteps (12 of 20 surfaces) | **Excellent** | **Missing: carpet, concrete, hardwood-interior (the Wood pack is exterior-flavoured), stair variants, asphalt, bluestone.** Carpet and concrete are the two that matter most. |
| Rain, wind, birds, night, insects | **Excellent** | Missing: rain on a roof, rain on windows, gutter trickle |
| Appliance hums | **Very good** | Missing: refrigerator compressor start/stop, washing machine, dishwasher, oven fan, extractor hood |
| Car | Good (static sounds) | — |
| Human breath/effort | Good | — |
| Fire | Good | — |
| **Doors** | **1 sample** | **Major gap**: 62 doors need open/close/latch/creak/slam across 4 door classes |
| **Light switches** | none | Major gap: 128 switch groups |
| **Cabinets and drawers** | none | Major gap: 214 containers |
| **Water: taps, shower, drain, flush, cistern** | none (the stream loops are outdoor water) | Major gap |
| **Toilet flush** | none | Major gap |
| **Thunder** | none | Major gap: 3 distance classes needed |
| **Hail** | none | Major gap |
| **Dog and cat** | none | Major gap |
| **HVAC, furnace, ducts** | partial (fans) | Gap |
| **Clocks, doorbell, garage motor, gate** | none | Gap |
| **House creaks** | none | Gap |
| Television programme audio | none | Covered by the video/sequence content instead |

**Conclusion: the NOX collection supplies roughly 45 % of what the game needs — and the part it
supplies (footsteps and outdoor ambience) is the part that is hardest to make and easiest to get
wrong.** The remaining 55 % is largely *interaction* sounds, which are short, plentiful on
CC0 sources (Freesound's CC0 filter in particular), and easy to audition. `plan.md` phase 31
contains one sourcing task per gap category with an explicit target count.

### 63.5 Processing pipeline for the collection

This section sketched a command; `tools/assets/convert_audio.py` (`HOUSE-00193`) is the real one,
and three parts of the sketch were corrected against measurement:

```
tools/assets/convert_audio.py <src> <dst> [--mono] [--trim] [--normalise -3] [--highpass 28]
```

**The sample rate is preserved; `-ar 44100` is gone.** The sketch justified it as "the SDL3 mixer's
device rate is 44.1 kHz by default and resampling once offline beats resampling every frame". That
is a CPU argument, and `HOUSE-00069` measured the quality it costs: 24→16 bit alone costs 0.0017 dB
RMS, while adding `-ar 44100` costs **0.889 dB RMS and 0.26 dB peak**, because the resample
lowpasses content these sources carry. CNA loaded and played a 48 kHz asset correctly and the mixer
resamples at playback anyway, so the offline resample bought nothing. Bit depth and sample rate are
independent decisions.

**`dynaudnorm` is gone too.** It rides the gain over time, which would flatten the difference
between a soft and a hard footstep — and those variants exist precisely to be different. A single
fixed peak-normalisation gain per file preserves the dynamics within a file and between files.

**Nothing is assumed about the input.** `HOUSE-00276` measured all 1 644 files: the publisher
documents "48 kHz / 24-bit" and 23 files are neither — 20 at 96 kHz, 3 at 32-bit `pcm_s32le`. The
tool probes every input, and `--require-rate` fails loudly rather than resampling a surprise.

**Loop points are not written, because the content pipeline discards them.** This was measured, not
assumed, and the result is not the obvious one. `SoundEffect` carries `loopStart`/`loopLength`, the
`.cnb` and `.xnb` formats both have fields for them, `SoundEffect::FromStream` parses a WAV `smpl`
chunk, and the loop region is applied at `Play()` — so the capability exists end to end *except* at
the step this project uses. `CNA.WavImporter` never looks at `smpl`: compiling the same one-second
tone twice, once with a `smpl` chunk declaring a loop over samples 100–40 000 and once without,
produced compiled payloads that are **byte-identical** (the only differences are the asset name and
the build fingerprint in the header). Loop metadata put into a source WAV would be silently dropped.

That is a CNA limitation, not something to work around here — `cna-house` does not modify CNA. It
costs little: `SoundEffectInstance::IsLooped` loops the whole buffer, which is what every looping
sound in this house actually wants. A file whose name says `Loop` is therefore left **untrimmed** —
trimming is what would break the loop — and seamlessness is a property of the source recording
rather than something this step can add.

Per file, the tool reports what a manifest row needs: source path, **source sha256, output
sha256**, duration, channels and the applied gain (`HOUSE-00279`).

Mono is preferred for anything played through `Apply3D` (a stereo source cannot be panned
meaningfully); stereo is kept for 2-D ambience beds.

Selected subset estimate: ~430 files of the 1 634 shippable (`HOUSE-00276` excludes ten), ≈ 100 MB
after conversion to 16-bit at the source rate — comfortably inside the audio budget (§72).

---

## 64. Room-aware spatial audio

> **Superseded in part 2026-09-21: there is no portal-path solver. Positional loops are gated by cell and one open-portal hop (`HOUSE-03541`), and weather beds follow sky exposure (`HOUSE-02000`).** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

> **Reduced again 2026-09-21: the positional loops (`HOUSE-03541`) are cut too. Only §64.6's ambience routing remains, as the weather beds' sky-exposure gain (`HOUSE-02000`).** ([ADR-0015](docs/decisions/ADR-0015-quality-tiers-and-compact-scope.md))

### 64.1 The problem

`Apply3D` gives distance attenuation and stereo pan in free space. In a house, a sound 4 m away
through two closed doors and a floor should be far quieter and duller than a sound 4 m away across
an open room — and it should seem to come from the doorway, not through the wall. CNA cannot do
this (§62.2). `cna-house` does it, using the portal graph it already has.

### 64.2 The portal-path solver

Once per frame, for each active emitter (≤ 64), a Dijkstra search over the portal graph from the
**listener's cell**:

```
cost(cell → cell') through portal p =
      airLoss(distance through p)                       // 6 dB per doubling, as free space
    + transmissionLoss(p)                               // see below
    + floorPenalty(levelDelta)                          // 6 dB per floor
```

```
transmissionLoss(p) = lerp(p.soundLoss.closed, p.soundLoss.open, smoothstep(0, 0.35, aperture))
```

The search terminates at the emitter's cell or at a cost ceiling (60 dB, beyond which the sound is
inaudible and can be virtualised). The result is:

| Output | Use |
|---|---|
| `pathGain` | multiplies the emitter's `Volume` |
| `muffle` ∈ [0,1] | drives the bright/dull cross-fade |
| `apparentPosition` | **the centre of the first portal on the path**, if the emitter is not in the listener's cell |

`apparentPosition` is the highest-value trick in this whole system. A television playing in the
family room, heard from the hall through the open doorway, is positioned **at the doorway**. Walk
past the doorway and the sound sweeps across the stereo field exactly as it should. Close the door
and it dims, dulls and stays at the door. This is one Dijkstra over ≤ 95 nodes and it is the
difference between "a house" and "sounds in a box".

Cost: ≤ 64 searches × ~95 nodes with a small binary heap ≈ 40 µs. Recomputed only when the
listener changes cell, a door/window aperture changes by > 0.05, or an emitter changes cell —
otherwise the previous solution is reused and only the intra-cell distance is updated.

### 64.3 Transmission losses

| Portal kind | Open | Closed |
|---|---|---|
| Cased opening / stair well | 0 dB | — |
| Interior hollow-core door | 1 dB | 16 dB |
| Interior solid door (study, master, cinema) | 1 dB | 24 dB |
| Exterior door | 1 dB | 30 dB |
| Glass slider | 1 dB | 24 dB |
| Window (single) | 1 dB | 22 dB |
| Window (basement hopper) | 1 dB | 26 dB |
| Garage sectional door | 2 dB | 26 dB |
| Wall (no portal) | — | 40 dB, used only for the "through the wall" fallback below |
| Floor / ceiling | — | 32 dB |

### 64.4 The through-the-wall fallback

If no portal path exists (every door on the way is shut), the sound is not silent — it comes
through the wall. The fallback is a direct-distance model with a **wall count** penalty: cast a
ray from the listener to the emitter, count the cell boundaries it crosses (using the same spatial
index), and apply 40 dB per boundary up to 3. The apparent position is the true emitter position.
So the washing machine spinning in the laundry with the door shut is a dull thump *from that
direction*, which is right.

### 64.5 Muffling without a filter

CNA exposes no per-instance filter. The approximation: for the **22 sounds where it matters** —
rain, wind, thunder, the television, the washing machine, the dishwasher, the shower, the toilet
flush, the furnace, the dog's bark, the vacuum, the music from a radio, the garage motor, the
compressor, the doorbell, the hall clock chime, the range hood, the kettle, hail, the gate motor,
a car passing, the lawnmower — two variants of the sample are shipped: the original and a
pre-filtered "dull" version (a 4th-order low-pass at 900 Hz plus a −3 dB tilt, produced offline by
`ffmpeg`). Two `SoundEffectInstance`s play in sync with complementary gains
`(1 − muffle)` and `muffle`. Cost: one extra voice for those sounds only, and a doubling of their
content size (they are 22 sounds).

For everything else, `muffle` is applied as an additional −4 dB per unit, which is a crude but
perceptually reasonable stand-in.

### 64.6 Ambience routing

Weather and exterior ambience are not point sources. Each cell has a precomputed
**sky exposure** (the solid angle of open sky reachable from the cell's centre through its
windows and doors, computed offline by ray casting) and a **facade exposure** per orientation.
The open-air rain and wind layers are gained by `skyExposure(cell) + Σ aperture-weighted window
contributions`, which updates live as windows open. `L3_STORE_W` under the roof gets the
rain-on-roof layer at full strength; `B1_CINEMA` gets essentially nothing; the sunroom with its
slider open gets almost the outdoor level.

> Corrected 2026-09-09 by `HOUSE-00779`. **"The cell's centre" is the mean over the cell's floor**,
> not one point in it. A centre can be free, inside the room, and still see none of the room's own
> windows: `L3_ROOM` is a T whose three dormers are at the ends of its arms, and every straight
> line from its centroid to any of them leaves through `L3_STORE_S` — a room §13.6 calls "lit by
> three dormers" measured 0.000. The figure is now the mean over a 1 m grid at ear height, capped
> at 16 points a cell, with the centre first and still the point the file carries;
> `docs/skyexposure-format.md` §5 is normative. Measured over this house: 96 cells, 997 listening
> points, `EXT_WORLD` 0.970 down to nine sealed basement rooms at exactly 0.000.

### 64.7 Reverberation

No convolution and no algorithmic reverb (CNA provides neither). Instead, each acoustic profile
selects a **pre-reverberated variant** for the 30 loudest transient sounds (door slams, the flush,
dropped objects, the dog's bark) — a dry version, a small-room version and a large-hard version,
baked offline. The cell's `reverbHint` picks one. This is an old technique, it is cheap, and in a
house — where the interesting rooms are small — it is convincing.

---

## 65. Persistence and save format

> **Superseded in part 2026-09-21. Only `settings.json` and an optional session file (player pose, clock, weather; `HOUSE-03571`) persist. The atomic-write, versioning and migration policy still applies to them.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

### 65.1 Principles

* **Never serialise raw memory.** No `memcpy` of structs, no pointer values, no
  implementation-defined layout.
* **Stable string IDs everywhere.** `L0_KITCHEN`, `DOOR_L1_MASTER`, `LG_L0_KITCHEN_MAIN`. A save
  written before a prop was moved still finds its entities.
* **Versioned, with a real migration chain.**
* **Delta against the canonical initial state.** Only entities differing from
  `initialstate.json` are written. A fresh house saves ~4 KB; a heavily explored one ~90 KB.
  This keeps saves small, diffable, and — crucially — makes adding new interactables backward
  compatible by construction: an unknown-to-the-save entity simply uses its canonical state.
* **Atomic writes.** Write `save.tmp`, flush, then rename over `save.json`, keeping `save.bak`.
* **Validated on read.** Checksum, schema, world hash, and per-field range checks.

### 65.2 Format

JSON via `System::Text::Json` (sharp-runtime). Human-readable, diffable, easy to inspect when a
tester says "the fridge was open". Size is not a concern at 90 KB.

```jsonc
{
  "format": "cna-house-save",
  "version": 3,
  "createdUtc": "2026-11-02T19:44:03Z",
  "gameVersion": "0.7.2+g1a2b3c4",
  "worldHash": "sha256:…",            // of world.manifest.json
  "checksum": "sha256:…",             // of the payload object, canonically serialised
  "payload": {
    "player": {
      "cell": "L1_MASTER_BED", "position": [-2.11, 3.65, -25.02],
      "yawDeg": 214.5, "pitchDeg": -6.2,
      "viewMode": "first", "walkMode": "normal", "crouched": false,
      "held": { "item": "ITEM_MUG_03", "sourceSurface": "SURF_L0_KITCHEN_ISLAND" },
      "avatar": { "sex": "female", "skin": 3, "face": 1, "hair": 2, "hairColour": 4,
                  "top": 1, "topColour": 2, "bottom": 0, "shoes": 2 }
    },
    "clock": { "epochSeconds": 1939473011.5, "timeScale": 60.0,
               "latitudeDeg": 40.05, "longitudeDeg": -75.30, "utcOffsetMinutes": -300 },
    "weather": { "cloudCover": 0.81, "cloudCumuliform": 0.34, "precipType": "Rain",
                 "precipIntensity": 0.42, "windSpeed": 6.1, "windDirectionDeg": 231.0,
                 "gustFactor": 0.38, "fogDensity": 0.19, "thunderIntensity": 0.02,
                 "temperatureC": 9.4, "humidity": 0.88,
                 "surfaceWetness": 0.71, "snowDepth": 0.0,
                 "target": "W_RAIN", "targetExpiry": 1939478600.0,
                 "transitionEnd": 1939473500.0,
                 "rngState": "0xA62B82F58DB8A985B4B6BEDE1E6EF7F29AC2F6B53B3F23C6D7B2A9D19E4A74B2" },
    "interactables": {                 // DELTA only
      "DOOR_L0_FRONT":        { "openFraction": 0.0, "latched": true, "locked": true },
      "DOOR_L1_MASTER":       { "openFraction": 0.53 },
      "WIN_L1_MASTER_N2":     { "openFraction": 0.32 },
      "LG_L0_KITCHEN_MAIN":   { "on": true },
      "FRIDGE_L0_KITCHEN":    { "doorOpen": false,
                                "removedItems": ["FRIDGE_ITEM_MILK_1"] },
      "TAP_L1_BATH2_BASIN":   { "flow": 0.6, "hotFraction": 0.4 },
      "BATH_L1_MASTER":       { "flow": 0.0, "plug": true, "fillLevel": 0.83 },
      "WC_L2_WC5":            { "urine": 0.35, "solids": 1, "lidOpen": true },
      "TV_L0_FAMILY":         { "on": true, "channel": 2, "playhead": 412 },
      "DRAWER_L0_KITCHEN_07": { "openFraction": 0.75 },
      "ITEM_MUG_03":          { "taken": true, "heldBy": "player" }
    },
    "pets": {
      "PET_DOG": { "cell": "L0_FAMILY", "position": [5.2, 0.6, -24.8], "yawDeg": 92.0,
                   "state": "Lie", "stateTimer": 12.4,
                   "arousal": 0.12, "affection": 0.61, "boredom": 0.30, "bed": "PROP_DOG_BED" },
      "PET_CAT": { "cell": "L1_LANDING", "position": [0.4, 3.65, -15.1], "yawDeg": 180.0,
                   "state": "Perch", "stateTimer": 88.0, "perch": "PERCH_L1_WINDOWSEAT",
                   "arousal": 0.05, "affection": 0.22, "boredom": 0.44 }
    },
    "stats": { "playSeconds": 4831.0, "simDaysElapsed": 3.2, "cellsVisited": 71,
               "interactionsPerformed": 412, "flushes": 6, "distanceWalkedM": 5820.0 }
  }
}
```

### 65.3 Versioning and migration

```cpp
struct Migration { int from, to; void (*apply)(JsonObject&); const char* note; };
static const Migration kMigrations[] = {
  {1, 2, MigrateV1ToV2, "weather gains cloudCumuliform; default 0.5"},
  {2, 3, MigrateV2ToV3, "toilet urine becomes float; bool true -> 0.5"},
};
```

* Version **lower** than current → apply migrations in order; log each; save at the new version on
  the next write.
* Version **higher** than current → refuse with a clear message ("This save was made by a newer
  version of CNA House") and offer to start fresh. Never guess.
* `worldHash` mismatch → the world data changed. Load anyway, but drop any interactable id that no
  longer exists (logging each), and give any new id its canonical state. If the player's cell no
  longer exists, respawn at `L0_FOYER`. This is the normal case during development and must be
  graceful, not fatal.

### 65.4 Autosave and manual save

* Autosave on: quitting, entering the pause menu, every 5 real minutes, and on a cell change if
  more than 60 s have passed since the last save.
* Saving is **fast** (a delta of ~90 KB) and happens on the game thread with no stall worth
  measuring; a save that ever exceeds 8 ms fails a performance test.
* One save slot by default (the house is *the* house). A settings option enables 3 named slots for
  testing and for players who want to keep a snowy December alongside a June afternoon.

### 65.5 Corruption handling

| Failure | Behaviour |
|---|---|
| File missing | Start from the canonical initial state, silently |
| JSON parse error | Try `save.bak`; if that fails, report clearly and offer "start fresh" — never silently discard |
| Checksum mismatch | Same as parse error, with a distinct message |
| Unknown enum value / out-of-range number | Clamp or fall back to canonical for that field only; log; continue |
| Unknown interactable id | Ignore; log once |
| Version too new | Refuse; offer fresh start |

A fuzz test (`HOUSE-02329`) mutates a real save 5 000 times and asserts the game always either
loads successfully or reports a clean error — never crashes, never hangs, never loads a
half-applied state.

### 65.6 The canonical initial state

Deterministic, so every test and screenshot starts identically.

| Aspect | Value |
|---|---|
| Simulated date/time | **Saturday 14 June 2031, 09:20 local (UTC−4 DST)** — a summer morning, sun at altitude 42°, azimuth 96° (ESE). Long day, good light, plenty of time before dark. |
| Moon | Phase 0.62 (waning gibbous), below the horizon at start, rises at 23:41 — so the first night has a bright moon |
| Weather | `W_PARTLY` reached: `cloudCover 0.35`, `cumuliform 0.9`, no precipitation, `windSpeed 3.2 m/s` from 225° (SW), `fogDensity 0.03`, `temperatureC 21.5`, `humidity 0.55`, `surfaceWetness 0.0`, `snowDepth 0.0`. Target `W_PARTLY`, expiry +140 sim min. RNG seed `0x5EEDC0DEC0FFEE01`. |
| Player | On the road at `(0.00, 0.00, +5.20)` facing **north** (yaw 0°, pitch −3°), first person, normal walk, holding nothing. Avatar: the default of §46.3. (Y corrected from +3.32 on 2026-09-08 by `HOUSE-00395`: §49 makes `playerPosition` the **feet** and puts the eye at `+ (0, eyeHeight, 0)`, so +3.32 started the game 3.3 m above the road.) |
| Pedestrian gate | **Closed, unlocked** — the first interaction is opening your own gate, which teaches the interaction system with zero instruction |
| Vehicle gate | Closed |
| Front door | Closed, **unlocked** |
| Garage door | Closed. Garage side door closed. Garage↔mudroom door closed. |
| Interior doors | All closed, **except three**, listed explicitly so the initial state is reproducible: `DOOR_L1_MASTER` at 0.35, `DOOR_L0_PANTRY` at 1.00, `DOOR_L2_ATTIC` at 0.00 (closed — the attic is a discovery) |
| Windows | All closed **except** `WIN_L1_MASTER_N2` at 0.50 — so weather audio through an open window is testable from the first minute |
| Lights | **All 135 groups off.** The porch lanterns and the street lights are on their dusk sensor and are therefore off at 09:20. (84 → 134 corrected 2026-09-08; see §28.2.) |
| Refrigerator | Closed, full (24 + 8 items), compressor idle, next cycle in 7 sim minutes |
| Taps, showers | All off; all plugs out; baths empty |
| Toilets | All clean, lids **down**, seats down, paper full |
| Televisions | All off |
| Containers | All closed |
| Appliances | All off; the washing machine has clothes in it, unstarted |
| Dog | `PET_DOG` in `L0_FAMILY`, lying on its bed at `(5.2, 0.6, −24.8)`, state `Lie`, affection 0.4 |
| Cat | `PET_CAT` on `PERCH_L1_WINDOWSEAT` in `L1_LANDING`, state `Perch`, affection 0.15 |
| Car | Parked in the garage, doors closed, not drivable |
| Held item | None |
| Stats | All zero |

---

## 66. Reset-house semantics

> **Removed from scope 2026-09-21. There is no household state to reset; *Start on the street* begins fresh.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

### 66.1 The command

**Menu → House → Reset House…**, with a confirmation dialogue that names exactly what will be
lost. It is never bound to a key and never happens automatically.

### 66.2 Two scopes

The confirmation dialogue offers two buttons plus cancel, because the brief asks for a coherent
policy and there are genuinely two reasonable meanings:

| Option | Resets | Keeps |
|---|---|---|
| **Reset Everything** *(default, highlighted)* | Every interactable, every container's contents, taken items, toilets, taps, baths, televisions, lights, doors, windows, pets, the player's position and orientation, the clock (back to 14 June 2031 09:20), the weather (back to the canonical state and RNG seed), and the statistics | Settings, avatar choice, key bindings, saved screenshots |
| **Reset Objects Only** | Every interactable, containers, taken items, toilets, taps, televisions, lights, doors, windows, pets, the player's position | The clock, the weather and its RNG state, and the statistics — so a player can tidy the house without losing a snowy January evening they were enjoying |

Cancel does nothing at all.

### 66.3 Mechanism

Reset is trivial because of the delta save (§65.1): it **discards the delta**. Concretely:

```
ResetHouse(scope):
    interactables.Clear()                      // every entity falls back to canonical
    pets.RestoreCanonical()
    player.RestoreCanonical()
    if scope == Everything:
        clock.RestoreCanonical()
        weather.RestoreCanonical()             // including the RNG seed
        stats.Clear()
    RebuildDerivedState()                      // lighting, portals, audio, residency, visibility
    WriteSave()
    ShowToast("House reset")
```

`RebuildDerivedState()` is the same function the loader calls, so there is exactly one code path
that turns a state snapshot into a running world — which is why reset cannot drift from load.

### 66.4 Guarantees tested

`HOUSE-02376`: perform 300 randomised interactions across every behaviour type, reset, and assert
the resulting state is byte-identical to a freshly started game's state (after canonicalising the
clock). `HOUSE-02377`: reset, save, reload, and assert the same. `HOUSE-02378`: assert
*Reset Objects Only* leaves the clock and RNG state untouched to the last bit.

---

## 67. UI and HUD

### 67.1 Philosophy

The HUD is nearly empty. There is no health, no stamina, no minimap, no objective marker, no
compass. What is on screen:

| Element | When |
|---|---|
| Interaction prompt | When something is targeted |
| Held-item thumbnail (small, lower right) | When holding something |
| Crosshair — a 3 px dot at 25 % alpha | Always in first person; it grows to a 12 px ring when a target is acquired |
| Walk-mode glyph | For 1.2 s after `Shift` |
| Sun/moon clock | §32.4 |
| Environment readout — wall time · season · year progress · outdoor °C | In play; hideable in Settings |
| Toast (bottom centre, 2.5 s) | Saved, house reset, locked, blocked |
| Loading indicator | Only at start-up |
| Vignette | A very slight permanent one; deepens during a toilet action and while adapting to darkness |

Everything is `SpriteBatch` + `SpriteFont`. Two fonts: a UI face at `<Size>` 16/22/30 and a
monospace face at `<Size>` 13/16 for the debug overlays, both from an OFL-licensed family, compiled
through the `.spritefont` route. `HOUSE-00200` vendored those faces — Noto Sans and Noto Sans Mono,
OFL-1.1, committed under `assets-src/Fonts/` (`docs/font-provenance.md`).

**Those numbers are points at 96 dpi, not pixels**, which is the XNA `.spritefont` convention and
was measured rather than assumed: the pipeline calls `FT_Set_Char_Size(…, 96, 96)`, so the em box is
`<Size> × 4/3` and the rendered line heights are 29/40/54 px for the UI face and 24/29 px for the
mono face. This paragraph said "px" until `HOUSE-00200` measured otherwise. Size a HUD element
against those line heights, not against the `<Size>` number.

### 67.2 Layout and scaling

All UI is laid out in **virtual units** on a 1600 × 900 design canvas and scaled by
`min(w/1600, h/900) × uiScale`. Safe-area insets are respected (a legacy of the XNA
`SafeAreaSample` idiom and genuinely needed on Android later).

### 67.3 Menus

A simple stack: `MainMenu`, `PauseMenu`, one-page `SettingsMenu`, `Confirm` and `Credits`. Keyboard,
mouse and touch navigate the settings page through the same input boundary, and every control is
reachable by keyboard alone. The active requirements for the remaining main/pause flow are in M9
of `plan.md`; historical avatar and pause-on-menu behavior are not part of the final scope.

### 67.4 First run

No tutorial and no text wall. The player starts on the road facing their own closed gate. The
prompt says `[E] Open gate`. That is the entire tutorial, and it teaches interaction, targeting
and movement in one gesture. A single unobtrusive hint line appears once, at the bottom:
`WASD move · mouse look · E interact · V camera · Esc menu`, and fades after 12 s.

---

## 68. Settings

Stored in `settings.json` beside the save, versioned and migrated the same way. Never reset by
*Reset House*.

> **Final scope (ADR-0016 / `plan.md` M9):** one page with Graphics, Audio, Controls and
> Environment sections. Only the rows named by `HOUSE-02516`, `HOUSE-02518` and `HOUSE-02521` are
> active requirements. The larger historical table below records the original design; it does not
> authorise extra pages, key remapping, gameplay simulation or other removed scope.

The retained Graphics section is exactly five rows: **High / Web / Android** quality profiles,
resolution (**Canvas size** on Web), fullscreen, v-sync and field of view. The project-owned
effective feature set removes display rows fixed by the platform profile: Web owns canvas size and
fullscreen but not browser scheduling, while Android owns no desktop display switch. The aggregate
quality row is resolved through `RenderTier` and `QualitySettings`; individual shadow,
post-processing, texture, LOD and anisotropy toggles from the historical table are not exposed.

| Tab | Setting | Values | Default |
|---|---|---|---|
| **Display** | Resolution | detected modes; "Canvas size" on Web | native |
| | Window mode | Windowed / Borderless / Fullscreen | Borderless |
| | V-sync | On / Off | On |
| | Frame-rate cap | 30 / 60 / 120 / 144 / Unlimited | 60 |
| | Field of view | 55–95° | 70° |
| | UI scale | 0.75–1.5 | 1.0 |
| **Graphics** | Quality preset | Low / Medium / High / Ultra / Custom | auto-detected |
| | Render tier | Auto / Stock only (Tier S) | Auto |
| | Shadow quality | Off / Blob / Map 1024 / Map 2048 | auto |
| | Particle quality | Low / Medium / High | Medium |
| | View distance | 0.6× – 1.4× | 1.0× |
| | LOD bias | −1 / 0 / +1 / +2 | 0 |
| | Anisotropic filtering | Off / 4× / 8× / 16× | 8× |
| | Texture detail | Half / Full | Full |
| | Post-processing (Tier E) | Off / On | On |
| **Audio** | Master | 0–100 % | 80 |
| | Ambience | 0–100 % | 75 |
| | World effects | 0–100 % | 100 |
| | Animals | 0–100 % | 90 |
| | Media (TV) | 0–100 % | 70 |
| | UI | 0–100 % | 60 |
| | Doppler | Off / On | **Off** (BL-11) |
| **Controls** | Mouse sensitivity | 0.2× – 4× | 1.0× |
| | Invert Y | Off / On | Off |
| | Head bob | Off / Subtle / Normal | Subtle |
| | Walk-mode default | Normal / Fast | Normal |
| | Camera default | First / Third | First |
| | Key bindings | full remap | — |
| **Simulation** | Day length | 20 / 24 / 48 / 96 min, Real time, Frozen, Custom | **24 min** |
| | Environment readout | On / Off | On |
| | Weather | On / Fixed / Off | On |
| | Fixed weather archetype | the 13 state archetypes | `W_PARTLY` |
| | Moon phase speed | 1× – 8× | 1× |
| | Location | Default / Reykjavík / Equator / Custom lat-long | Default |
| | Pets | On / Off | On |
| | Pause on menu | Off / On | Off |
| | Save slots | 1 / 3 | 1 |

Nothing meaningless is exposed. There is no "enable shadows" toggle that does nothing on a tier
that cannot draw them. The filter is a **project-owned effective feature set**, computed by
`cna-house` from facts `cna-house` already holds:

```
effective feature set = RenderTier + build/platform profile + validated standard-XNA behaviour
```

| Term | Where it comes from |
|---|---|
| `RenderTier` | The build-time `CNAHOUSE_TIER_E` fact plus the load-time Tier-S/Tier-E resolution (§7.3, OWN-06) |
| Build/platform profile | The CNA configuration this binary was built against (`CNA_GRAPHICS_RENDERER`, `CNA_EASYGL_COMPILED_EFFECTS`, `CNA_ENABLE_VIDEO`; §8.1), the target platform (Linux / Web / Android; §9) and its content profile (§27.2) |
| Validated standard-XNA behaviour | What the Phase-1 probes measured for that profile and recorded in `docs/cna-capability-report.md` — established once, offline |

The result is a plain `cnahouse::` structure, seeded at build time and finalised once after
`LoadContent`. The Graphics tab reads it and offers only the rows it marks available: a build
whose profile has no shadow maps shows no shadow-map options, and a Tier-S-only build shows no
post-processing row.

**No CNA-specific runtime capability API is involved anywhere in this.** The UI never calls
`GraphicsDevice::SupportsCapability()` or any other CNA extension query — those are Tier C and
forbidden (§4.3), and `check_xna_only.py` rejects them outright (§70.1). Where a feature is
genuinely optional on the hardware rather than on the build, the profile decides in advance and
the fallback is a documented standard-XNA path: **anisotropic filtering** is requested through
`SamplerState::MaxAnisotropy` only on profiles validated to permit it, and every other profile
samples **trilinear** (`TextureFilter::Linear` with mips) — the setting row is simply not offered
there.

---

## 69. Debug tools

All compiled out of a release build by `CNAHOUSE_DEBUG_TOOLS` (default ON for `Debug` and
`RelWithDebInfo`, OFF for `Release`), and all drawn with `SpriteBatch`/`SpriteFont` and a small
line renderer built on `BasicEffect` — **no CNAEXT debug UI**.

| Key | Overlay |
|---|---|
| `F1` | Performance: FPS, frame time, a 240-sample frame-time graph, per-system CPU times, draw calls, triangles, state changes, GPU timing where available |
| `F2` | World: player cell, position, yaw/pitch, ground surface, current level, held item, target interactable and its full state |
| `F3` | Visibility: visible/culled cell counts, traversals, portals tested, frusta per cell, max depth reached, the visible cell list |
| `F4` | Visibility geometry: cell wireframes (green visible, red culled), portal quads (blue open, grey closed, cyan glass), reduced-frustum pyramids |
| `F5` | **Freeze visibility** — keep the current visible set and fly the camera out to inspect it. The single most valuable portal-debugging tool. |
| `F6` | Lighting: per-cell artificial/daylight levels, ambient colour swatch, dominant light direction, exposure, light-group states |
| `F7` | Audio: active voices by category, each emitter's cell, path gain, muffle, apparent position, the solved portal path drawn as a line |
| `F8` | Environment: simulated date/time, sun/moon altitude and azimuth, moon phase and name, the full weather state vector, the current archetype and time to the next transition, RNG state |
| `F9` | Physics: collision shapes in the visible cells, the player capsule, ground probe, sweep results, the pets' capsules and their current path |
| `F10` | Content: residency tiers, loaded packs, texture and model memory, cache hit rates, the last 20 loads |
| `F11` | Save: the current delta, entity count, last save time, save size, schema version |
| `` ` `` | Console |

Console commands (development builds only): `time set <hh:mm>`, `time scale <x>`,
`time advance <days>`, `weather set <archetype>`, `weather freeze`, `teleport <cellId>`,
`door <id> <0..1>`, `light <group> <on|off>`, `spawn <assetId>`, `reset house`,
`save`, `load`, `screenshot [path]`, `cull off|on`, `tier s|e`, `budget report`,
`nav draw`, `pet <dog|cat> state <state>`, `validate world`.

`weather set` accepts only one of §36.2's thirteen complete state archetypes; `W_WINDY` is a
modifier and is refused as a stand-alone target. `weather freeze` is a toggle: the first call
pauses the weather at its current continuous state and the next resumes transitions. A bare
`weather` reports both the selected target and whether transitions are running, so neither command
depends on invisible console state.

The **free-fly camera** (`cnahouse::debug::FreeFlyCamera`, `HOUSE-00476`) is what F5 flies out
with, and it is what `--scene=blockout` is steered by. It ignores collision, gravity, head height
and the portal graph on purpose: what it is for is standing inside a wall to see which side of it is
inside out. 4 m/s, ×6 while running; movement follows the view, strafe stays level, up and down are
world up and down, and the pitch stops 1° short of the pole where a yaw-then-pitch camera loses its
horizon. It reads `player::InputState` rather than keys, so it needs no window to be tested and is
remapped wherever every other control is (§68). Taking it over adopts the pose of whatever camera
was there, so the view does not jump.

A **screenshot harness** drives a named camera pose list from JSON, sets a fixed time and weather,
renders and writes PNGs — used by the render regression tests and for producing documentation
images.

---

## 70. Testing

### 70.1 Static and build-time gates

| Gate | Tool | Fails the build when |
|---|---|---|
| XNA-only | `tools/ci/check_xna_only.py` | A runtime source does **any** of: includes a header under `CNA/`; names `CNA::` in any form; contains an identifier matching `*EXT*` (`getSkinsEXTProperty`, `setOwnedResources`, `SkinnedModelEXT`, …); calls `SupportsCapability`; names `ShaderEffect`, `PbrEffect`, `SkinnedPbrEffect` or `AvatarRenderer`; reads `Model::Tag` or `Model::getTagProperty` (§47.0); names a `Graphics::SkinningData`/`AnimationClip`/`Keyframe`/`AnimationPlayer` type from CNA; touches GL/GLES/EGL/Vulkan/WebGPU/D3D/Metal/SDL-rendering symbols. Or the CMake cache has `CNA_CNAEXT=ON`. **There is no allowlist and no deviation register to consult** (§4.3) |
| Custom shaders are XNA `Effect`s | the same script | A `.fx` is authored outside `assets-src/Effects/`, or any GLSL/SPIR-V source appears in the tree |
| No CNAEXT in the linked binary | `nm -C` in CI (`HOUSE-00136`) | Any `CNA::Graphics::` symbol is present in `cna-house` |
| Animation assets bind | `tools/ci/check_anim_assets.py` | A `.chanim` sidecar's joint list disagrees with its model's `Model::Bones`, or a source `.glb` declares more than one skin (`HOUSE-00225`) |
| No booleans for continuous weather | the same script | A member matching `is(Raining|Snowing|Windy|Stormy)` appears |
| Manifest completeness | `check_manifest.py` | A file under `assets-src/` has no manifest row, or a hash mismatches |
| Licence completeness | `verify_licences.py` | A manifest row lacks a licence, or references a licence file that does not exist, or is marked `PROVENANCE UNKNOWN` in a packaging build |
| World validity | `validate_world.py` + the C++ validator | Any of the 15 rules in §15.7 |
| Content freshness | `make content-verify` | A rebuild produces different bytes |
| Compiler | `-Wall -Wextra -Wpedantic -Werror` | Any warning |
| Sanitizers | `build-asan`, `build-ubsan` in CI | Any report |
| Format | `clang-format --dry-run -Werror` | Any deviation |

### 70.2 Unit tests (headless, no GPU)

Roughly 900 cases across: the world loader and every schema; the portal graph (connectivity,
symmetry, plane membership); cell lookup including hysteresis and the grid fallback; frustum
reduction (a reduced frustum must contain every point the portal rectangle can see and no point
outside the parent frustum — proven with 10⁵ random samples); clipping degenerate cases; the save
model, round trip, delta, migration chain (each migration gets a fixture from the previous
version), corruption and fuzz; the weather state machine, rate limits, seasonal tables, seeded
determinism (the same seed must produce an identical 10 000-minute history); the clock, the solar
model against 200 published sunrise/sunset values (± 3 minutes), the lunar model against 60
published phase dates (± 0.02 phase), sidereal rotation; the interaction predicate/effect parser
(including that every unknown token is rejected at load); each of the 12 behaviours' state
machines; collision primitives (capsule/OBB, capsule/triangle, sphere sweep, ray/AABB) against
analytic answers; `ClipPlayer` blending and stride matching; the two-bone IK; the portal-path
audio solver; the LOD selector's hysteresis; ID stability (a golden list of every id in the world
data — adding is fine, renaming fails).

The golden list is [`tests/unit/reference/world-ids.golden.txt`](tests/unit/reference/world-ids.golden.txt),
2 152 ids over 27 kinds, maintained by `tools/world/id_golden.py` and gated as `world-ids`
(`HOUSE-00399`). It is **append-only**: `--emit` records ids the world has gained and never deletes
one, so a rename shows up as an id that left the world and keeps failing until a person removes the
line and says why. Nothing in §15.7's fifteen rules can see a rename — the layout is internally
consistent under either name — and a save file is a list of ids (§68), so this is the only gate
that stands between a tidy-up and every save ever written.

### 70.3 Integration tests (HEADLESS renderer, full `Game` loop)

Roughly 220 cases. Each constructs the real `CnaHouseGame` against
`CNA_GRAPHICS_RENDERER=HEADLESS`, runs `Update` for a scripted number of fixed steps, and asserts
on state. Highlights:

* Open the kitchen door from the hall → `L0_KITCHEN` enters the visible set; close it → it leaves.
  Repeated for all 62 doors, in both directions, from both sides.
* With every door closed, the visible set from `L0_FOYER` is exactly `{L0_FOYER, L0_HALL,
  L0_STAIR_MAIN, L1_STAIR_MAIN, L1_LANDING, L0_LIVING, L0_PORCH, EXT_WORLD, …}` — a golden list.
* With every door open, the visible cell count from 12 named poses stays within budget.
* Switch a light → the room's `artificialLevel` changes, the neighbouring cell's borrowed light
  changes, the fixture's emissive changes.
* Turn a tap on, save, load → still running, same flow, same `fillLevel`.
* Fill a bath to 1.0 → it stops at 1.0 and does not overflow the room.
* Use a toilet, save, load → contents present; flush → contents gone; save, load → still gone.
* Take milk from the fridge, put it on the island, save, load → it is on the island.
* Turn the TV on, save, load → on, same channel, playhead within 1 s.
* Reset House → byte-identical to a fresh game (§66.4).
* Walk a scripted 20-minute path → never leaves the playable cells, never enters static geometry,
  the invisible-boundary counter stays 0.
* The dog paths from `L0_FAMILY` to `L1_BED2` only when the doors on the way are open.
* Advance 30 simulated days → the moon completes one lunation ± 0.03 and the weather visits
  ≥ 8 archetypes.
* A save from schema v1 and one from v2 both load and produce the expected v3 state.

### 70.4 Render regression tests (`OPENGLES3` under `Xvfb`)

Roughly 120 scenes. Fixed camera pose, fixed clock, fixed weather, fixed RNG; render one frame;
compare against a stored PNG with a per-pixel tolerance and a mean-absolute-difference budget
(the same methodology `cna-samples` uses for its XNA-oracle comparisons). Scene families:

| Family | Scenes |
|---|---|
| Time of day | 8 poses × {06:00, 09:00, 13:00, 17:00, 19:30, 21:00, 00:00, 04:00} |
| Weather | 6 poses × {clear, overcast, rain, heavy rain, snow, thunderstorm (flash-frozen), fog, hail} |
| Room lighting | every room, lights on and off, at noon and at midnight (sampled: 20 rooms) |
| Doors | 10 poses with a door at 0.0, 0.35, 1.0 |
| Culling sanity | the 12 budget poses, rendered normally and with culling disabled — the images must match |
| Tier S vs Tier E | 10 poses rendered in both tiers — content identity, not pixel identity |
| Characters | avatar in 6 customisations, 4 poses; dog and cat in 4 states |
| Exterior | 8 poses covering the road, drive, garden, terrace and the neighbourhood at 3 LOD distances |
| UI | prompt, held item, sun clock, menus |

A failing scene writes the reference, the actual and a difference image into the CI artefacts.

### 70.5 Realism validation (automated)

Run by `validate_world.py` over the layout and by `scale_check.py` over every asset.

| Check | Tolerance |
|---|---|
| Interior door leaf height | 1.98–2.10 m |
| Interior door leaf width | 0.76–0.95 m |
| Ceiling clear height, habitable rooms | 2.35–3.10 m |
| Stair `2·rise + going` | 600–650 mm |
| Rise consistency within a flight | ≤ 2 mm |
| Headroom over every flight and landing | ≥ 2.00 m |
| Handrail height | 0.85–0.95 m |
| Balustrade / railing height | ≥ 0.90 m interior, ≥ 1.05 m at a drop > 1 m |
| Kitchen counter height | 0.88–0.95 m |
| First authored kitchen sink run | 2.70–2.95 m width, 0.72–0.95 m full depth including tap; its separate measured counter top stays 0.88–0.95 m |
| Fitted kitchen cooking bay | 0.95–1.05 m width, 0.65–0.80 m full depth including pulls; its separate measured counter top stays 0.88–0.95 m |
| Large kitchen refrigerator appliance | 1.60–1.90 m width, 0.70–0.90 m depth including pulls, 1.80–2.10 m body height; a bridge cabinet's combined AABB must not stand in for body height |
| Integrated refrigerator/bridge bay height | 2.60–2.80 m overall from L0 finished floor to ceiling |
| Domestic upright piano | 1.35–1.60 m width, 1.10–1.35 m height, 0.50–0.76 m full depth including pedals |
| Upper cabinet underside | 1.40–1.55 m |
| Dining table top | 0.72–0.78 m |
| Desk top | 0.72–0.78 m |
| Chair seat | 0.42–0.48 m |
| Sofa seat | 0.38–0.45 m |
| Bed mattress top | 0.48–0.62 m |
| WC seat | 0.38–0.45 m |
| Basin rim | 0.80–0.90 m |
| Bath rim | 0.50–0.60 m |
| Light switch centre | 1.10–1.30 m |
| Socket centre | 0.25–0.45 m |
| Door handle centre | 0.95–1.10 m |
| Window sill (habitable) | 0.50–1.10 m |
| Human avatar height | 1.55–1.90 m |
| Dog withers height | 0.50–0.70 m |
| Cat shoulder height | 0.20–0.32 m |
| Car length / width / height | 4.2–5.2 / 1.7–2.0 / 1.4–1.9 m |
| Delivery van length / width / height | 4.8–7.5 / 1.8–2.6 / 1.9–3.0 m |
| Tree height, sapling / young / mature | 3.40–3.60 / 6.85–7.15 / 11.75–12.25 m |
| Shrub / flower / grass-card height | 0.45–1.85 / 0.20–0.50 / 0.20–0.60 m |
| Player capsule clearance through every portal | ≥ 0.62 m width, ≥ 1.95 m height (or the portal is marked `crouch`) |

The combined `HOUSE-01040` sink run includes its gooseneck tap, so its whole-model Y bound
cannot stand in for counter height. Its manifest records `geometry.counterHeightMetres`, which
`scale_check.py` verifies inside the same 0.88–0.95 m band; the tap does not relax that rule.
The `HOUSE-01044` cooking bay similarly reaches 2.65 m at its hood; its 1.00 m width, full depth
and separately measured 0.94 m stone scribes are all checked instead of
mistaking the assembly height for a counter height.
| Every interactable reachable from a standing eye position | a 2.5 m ray must reach `focus.point` |
| Room area vs. its function | a bedroom ≥ 9 m², a bathroom ≥ 3.5 m², a WC ≥ 1.8 m², a corridor ≥ 0.9 m wide |

**Who checks what** (recorded 2026-09-08 by `HOUSE-00360`, which implemented the layout half).
`validate_world.py` rule 10 and `WorldValidator::CheckRealism` — the two say the same thing, and
both run — own every row the layout decides: interior door leaf height and width, habitable clear
height, `2·rise + going`, capsule clearance, window sill over the room's own floor, light switch
and door handle centres, room area against what the room's `name` says it is for, and the corridor
width. `scale_check.py` owns every row that is a property of an *asset*: counter, upper cabinet,
dining table, desk, chair and sofa seat, mattress, WC seat, basin and bath rim, handrail, human,
dog, cat and car, plus the vegetation age/category bands acquired by `HOUSE-00297`.

Four rows are checked by neither, and each is a missing **input**, not a missing check:

* *rise consistency within a flight* cannot fail. `layout.stairs.json` carries one `rise` per
  flight, so every riser is equal by construction and the check would assert a tautology.
* *headroom over every flight and landing* needs the flight's position in plan, and a flight row
  has `fromCell`, `toCell`, `risers`, `rise`, `going` and `width` — no origin, no direction. It
  becomes checkable when `HOUSE-00459` generates the carriages.
* *balustrade / railing height* has no row anywhere: railings are not in the layout and no asset
  is categorised as one. `handrail` is, and is checked.
* *socket centre* has no socket interactables to measure. A band over an empty set is a check that
  passes for the wrong reason, so it is absent rather than green.

The window sill row is decided by the window's declared **type**, never by its measurement: §12.6
gives a sill per type, and eleven of the house's 66 windows sit outside 0.50–1.10 m on purpose —
the sidelights beside the front door, the sunroom's full-height panels, the bathrooms' obscured
privacy glazing at 1.40 m, the basement hoppers at 1.85 m and the front door's transom at 2.20 m.
An exemption written as "high sills are fine" would have exempted every mis-authored window along
with them.

### 70.6 Performance tests

Run nightly on the dev machine against the budgets of §71, on the reduced plan's fixed set of
representative scenarios:

| Scenario | What it stresses |
|---|---|
| `L0_KITCHEN` | Densest retained interior composition |
| `L2_LIBRARY` | Hero-area shelving, dressing and practical lights |
| `L0_STAIR_MAIN` looking up | Three floors visible at once |
| Street approach towards the house | Neighbourhood, vegetation, terrain and facade |
| Rear garden towards the house | Garden, planting, fencing and rear elevation |
| An upper-floor window looking out | Interior/exterior overlap through glazing |
| Heavy rain outside | Maximum retained precipitation and wet exterior state |
| Night outside | Exterior lights, window glow and night sky |

`HOUSE-02402` runs each at 1920×1080, Tier S / High, with fixed camera, clock, weather and seed. It
prints average draw calls and triangles plus CPU submission and one-texel-readback GPU-completion
milliseconds. The harness reports rather than gates: `HOUSE-02403` records the reference-hardware
baseline in `docs/performance-log.md`, and `HOUSE-02404` compares it to §71 and optimises only rows
that miss.

---

## 71. Performance budgets

### 71.1 Reference hardware

The dev machine: AMD Radeon 780M integrated GPU (radeonsi, Mesa 25.0.7, GL 4.6 / GLES 3.2,
1 024 MB reported video memory, unified with 30 GB system RAM), 16 CPU threads, Debian 13.
This is deliberately *modest* hardware. A house that runs at 60 FPS on a 780M will run
comfortably on anything with a discrete GPU.

### 71.2 Desktop target — 1920 × 1080, 60 FPS (16.67 ms)

| Budget | Typical | Worst case | Hard fail |
|---|---|---|---|
| **Frame time** | 12.0 ms | 16.0 ms | > 16.67 ms sustained |
| **CPU total** | 5.5 ms | 8.0 ms | 9.5 ms |
| — visibility | 0.55 ms | 1.20 ms | 1.6 ms |
| — animation | 0.45 ms | 1.00 ms | 1.3 ms |
| — physics | 0.35 ms | 0.80 ms | 1.0 ms |
| — audio update | 0.25 ms | 0.60 ms | 0.8 ms |
| — weather + particles | 0.40 ms | 1.00 ms | 1.3 ms |
| — lighting | 0.20 ms | 0.45 ms | 0.6 ms |
| — pets + interaction + logic | 0.30 ms | 0.60 ms | 0.8 ms |
| — draw submission | 1.60 ms | 2.20 ms | 2.8 ms |
| — residency | 0.10 ms | 0.60 ms | 1.0 ms |
| **GPU** | 8.0 ms | 12.0 ms | 14.0 ms |
| **Draw calls** | 620 | 1 400 | 1 800 |
| **Visible cells** | 9 | 22 | 30 |
| **Portal traversals** | 24 | 70 | 110 |
| **Triangles submitted** | 950 k | 2 600 k | 3 400 k |
| **Vertices** | 640 k | 1 700 k | 2 200 k |
| **State changes** | 90 | 210 | 300 |
| **Audio voices** | 14 | 32 | 32 (hard) |
| **Weather particle quads** | 500 | 2 000 | 2 600 |
| **Dynamic instances submitted** | 180 | 520 | 700 |

### 71.3 Quality tiers

| Tier | Resolution | Shadows | Particles | LOD bias | View distance | Post | Target |
|---|---|---|---|---|---|---|---|
| Ultra | native | Map 2048 + patches | High | −1 | 1.3× | On | 60 FPS on discrete |
| High *(default on the dev machine)* | native | Map 2048 | Medium | 0 | 1.0× | On | 60 FPS |
| Medium | native | Map 1024 | Medium | 0 | 0.85× | Off | 60 FPS on weaker iGPU |
| Low | 0.75× scale | Blob only | Low | +1 | 0.7× | Off | 60 FPS on very weak |
| Web | 1280 × 720 | Blob only | Low | +1 | 0.7× | Off | 30 FPS |
| Android | 1280 × 720 | Blob only | Low | +2 | 0.6× | Off | 30 FPS |

### 71.4 Web tier budget

| Budget | Value |
|---|---|
| Frame time | 33 ms (30 FPS) |
| Draw calls | 500 |
| Triangles | 900 k |
| Texture memory | 160 MB |
| Total heap | 900 MB (Wasm) |
| Compressed download | 180 MB |
| Audio voices | 20 |
| Particles | 700 |

### 71.5 Android tier budget

| Budget | Value |
|---|---|
| Frame time | 33 ms |
| Draw calls | 400 |
| Triangles | 700 k |
| Texture memory | 120 MB |
| RSS | 700 MB |
| APK + OBB | 400 MB |
| Audio voices | 16 |
| Particles | 500 |

### 71.6 Enforcement

Every budget above is asserted by a nightly performance test (§70.6). A regression writes a row
into `docs/performance-log.md` with the commit hash, so it is always possible to say which change
cost what. Exceeding a **hard fail** number fails CI.

---

## 72. Memory and asset budgets

| Category | Budget | Notes |
|---|---|---|
| **GPU total** | 550 MB | On a 1 GB iGPU sharing system memory |
| — Textures (albedo, normal) | 300 MB | ~1 400 textures after atlasing, mostly DXT where the content profile packages it (§27.2) |
| — Lightmaps | 60 MB | 21 art atlases + 21 daylight atlases, 2048² DXT1, budgeted for §18.3's **lightmap receivers** and not for every triangle the shell generator makes |
| — Vertex/index buffers | 120 MB | ~6.2 M vertices resident across all packs |
| — Render targets | 40 MB | shadow map 2048² + 2 composite targets at native |
| — Dynamic buffers | 10 MB | particles, sky, stars, foliage |
| — Slack | 20 MB | |
| **CPU RSS** | 1 600 MB | |
| — Content (CPU copies, needed for WebGL context-loss recovery) | 480 MB | |
| — World data, collision, nav | 40 MB | |
| — Audio buffers | 140 MB | ~430 clips at 16-bit, ≈ 95 MB **only if every loop is trimmed** — see below — plus 22 dull variants |
| — Runtime structures | 90 MB | |
| — CNA + SDL + FFmpeg + allocator overhead | 450 MB | |
| — Slack | 400 MB | |
| **Disk (installed)** | 1.4 GB | |
| — `content/` | 1.1 GB | |
| — binaries | 90 MB | |
| — licences, docs | 10 MB | |
| **Source repository** | ≤ 3.5 GB | `assets-src/` dominates; large binaries are hash-pinned and fetched, not committed, except the small committed baseline |

**Why the lightmap budget is for receivers only** (measured 2026-09-08, decided 2026-09-09,
`HOUSE-00471`). Over the generated architectural shell as it then stood: **43 528 faces**, of which
**26 704 are below one texel** at §18.3's nominal 4 texels/metre. (`HOUSE-00475` later removed the
yard walls the generator should never have built; the shell is 33 486 triangles now and the 78
baked cells lost 2 494 faces. The ratio and the conclusion are unchanged, and the measurement above
is kept as it was taken.) A representative 55 mm handrail face is about
**0.2 texel** across at that density. Lighting thin detail from a bake would need roughly
**20 texels/metre** for a 55 mm board — a 25× increase in *texel area for the surfaces so treated*,
spent on the surfaces carrying the least lighting information. (25× is the density-area scaling for
those surfaces, not a measured multiple of the whole atlas requirement.) The budget above therefore
stands unchanged and covers the receivers; the detail is lit dynamically (§18.3, §22.2).

**Corrected again by `HOUSE-00278`, which performed the conversion: the loop cap is 8 s, not 10,
and 10 was checked against the wrong budget.** §72's arithmetic below validated 10 s against this
table's own **audio-buffer memory row of 95 MB** and found 91 MB — but the binding constraint is
§71's **pack** budgets, `audio-core` 30 MB and `audio-ambience` 55 MB, which total 85 MB and which
that arithmetic never touched. Measured on the real 445-file selection:

| Loop cap | `audio-core` | `audio-ambience` |
|---|---:|---:|
| 10 s | 31.14 MB — **103.8 %** | 58.87 MB — **107.0 %** |
| 9 s | 30.57 MB — **101.9 %** | 54.32 MB — 98.8 % |
| **8 s** | **29.93 MB — 99.8 %** | **49.24 MB — 89.5 %** |

Eight seconds is the largest cap that fits both. The audible argument below is unchanged by it —
it is about being an order of magnitude away from an obvious one-second loop, and eight seconds is
as far from that as ten. The achieved total is **79.2 MB**.

**Two things a later phase must know.** First, `audio-core` is at **99.8 %** with the NOX subset
alone, and `HOUSE-00281`…`HOUSE-00290` still have to add roughly 220 one-shots to that same pack —
about 11 MB at the measured average of 0.052 MB a clip. **The pack cannot hold them**, and the
choice between raising it, moving the ten human-breath loops (6.4 MB) out of it, and cutting
further is a budget decision this task deliberately did not make alone. Second, a shortened loop is
cut with a **head crossfade**, not with `-t`: cut plainly it steps once per loop for as long as the
room is on screen, measured at a 0.149 discontinuity against 0.015 with the blend.

**The audio row's "≈ 95 MB" priced one-shots and not loops**, measured by `HOUSE-00277` against the
real NOX selection. 445 clips — the row's own count — come to **156 MB** at 16 bit, and the split
says why: **373 one-shots are 21 MB** while **72 loops carry 1 106 seconds and 136 MB**. 95 MB over
430 clips is 0.22 MB each, about one second of mono; a 30-second stereo rain bed is 5.8 MB and a
108-second computer hum is 10 MB. Trimming every loop to **10 seconds** brings the total to
**91 MB**, inside the row — so the budget is achievable, but it is a constraint on the *conversion*
(`HOUSE-00278`, `convert_audio.py --trim`) and not something the selection can meet by choosing
fewer files. Ten seconds of rain or of a fan does not read as a repeat behind everything else in a
house; thirty is what the recordist supplied.
| **Cold start** | ≤ 4.0 s | to a playable frame on the dev machine; measured by `HOUSE-02451` |
| **Save file** | ≤ 150 KB | typical 90 KB |

Per-asset triangle budgets:

| Category | LOD0 triangles |
|---|---|
| Architectural shell, per cell | ≤ 3 500 |
| Hero furniture (sofa, bed, range) | ≤ 12 000 |
| Ordinary furniture | ≤ 5 000 |
| Small prop | ≤ 1 200 |
| Tiny prop | ≤ 300 |
| Character (body + head) | ≤ 22 000 |
| Clothing item | ≤ 4 000 |
| Hair | ≤ 6 000 |
| Dog | ≤ 18 000 |
| Cat | ≤ 14 000 |
| Car | ≤ 45 000 |

**Measured against the generated shell** (2026-09-08, `HOUSE-00479`, `tools/world/verify_shell.py
--report`). The whole shell is **33 486 triangles over 88 cells**, and the worst single cell,
`L0_SUNROOM`, is **1 080 — 30 % of the 3 500 above.** By level: B1 6 338, L0 10 468, L1 7 332,
L2 6 370, L3 2 654, and 324 in the roofs and chimney, which belong to no cell.

Two things worth reading off that. The generator's own row in §20.2 estimated **~180 000
triangles**, five times what it makes; the estimate was written before the generator existed and is
corrected there. And **74 % of the shell is `trim`** — 24 832 triangles of skirtings, cornices,
architraves, nosings and handrails, against 3 044 of wall and 458 of floor and ceiling. That is the
same geometry §18.3 decided not to lightmap, and it is worth knowing that the class carrying almost
no lighting information carries three quarters of the triangles.
| Tree LOD0 | ≤ 9 000 |
| Neighbourhood house LOD0 | ≤ 6 000 |

---

## 73. Failure handling

| Failure | Behaviour |
|---|---|
| Missing content asset | Load a typed fallback (grey box of the right size / mid-grey texture / silence), log once with the asset name, continue. In a development build the fallback is magenta and the frame counter turns red; a packaging build fails instead of shipping. |
| World data fails validation at load | **Fatal, with a precise message** naming the file, the rule and the offending id. A broken house is not playable and pretending otherwise wastes everyone's time. |
| Save corrupt | §65.5 |
| Built without Tier E (`CNAHOUSE_TIER_E=OFF`) | Tier S is the whole binary; the settings screen shows "Stock effects (this build has no compiled effects)" and the tier toggle is absent |
| The Tier-E effect set fails to load (missing `.xnb`, or the driver rejects the bytecode) | Caught at `LoadContent`; fall back to Tier S silently, log once with the failing asset name, disable the tier toggle (§7.3) |
| Renderer lacks `OcclusionQuery` | Sun glare uses a pure depth-buffer heuristic (sample the depth buffer at the sun's position via a 1×1 `RenderTarget2D` readback every 4th frame); the clock overlay drops the coverage condition |
| `SurfaceFormat::Single` render target unsupported (BL-09) | Shadow map packs depth into RGBA8; a settings note explains the small precision loss |
| FFmpeg absent / video unsupported (BL-05) | `SequenceTvSource`; no error shown to the player |
| Audio device absent | The game runs silently; `NoAudioHardwareException` is caught at start-up and every audio call becomes a no-op |
| WebGL context lost | CNA raises the loss; the game skips Draw and Present, keeps updating, rebuilds GPU resources from the CPU copies on restore, and shows a brief "Restoring graphics…" overlay |
| Out of GPU memory during a residency promotion | The promotion is abandoned, the cell renders its `essential` set, the residency system lowers its high-water mark permanently for that session, and a toast appears once |
| A frame takes > 250 ms | The fixed-step physics accumulator clamps to 4 steps so the simulation cannot spiral; the clock advances by the real elapsed time regardless, so a hitch does not desynchronise the world |
| An interaction predicate references an unknown field | **Load-time error**, not runtime — §15.6 |
| A pet cannot path anywhere | It reverts to `Idle` in place and logs once; the nav graph's connectivity is validated at load so this indicates a runtime door state, not a data bug |
| Unhandled exception in `Update`/`Draw` | Caught at the `Game` boundary, logged with a stack trace where available, the current state is emergency-saved to `crash-save.json`, and a message offers to restart. The game never dies silently. |

Logging: a small ring-buffer logger writing to `stderr` and to `cna-house.log` beside the save,
with levels and per-category rate limiting (an error that would repeat 60 times a second is
logged once with a count).

---

## 74. Development phases

> **Superseded 2026-09-21. The 53-phase list below is archived with the legacy ledger (`docs/history/plan-legacy-2026-09-21.md`). `plan.md`'s milestones M0–M16 govern.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

The full phase list with task IDs is `plan.md`. Summarised:

| # | Phase | Outcome |
|---|---|---|
| 0 | Repository, conventions, ADRs | The repo builds an empty `Game` |
| 1 | CNA capability verification | Every assumption in §5 re-proved by a probe; BL-09 settled |
| 2 | Build skeleton, CMake, CI | `Game` runs, clears the screen, HEADLESS tests run in CI |
| 3 | Content pipeline | glTF/PNG/WAV/SpriteFont/FX all compile and load |
| 4 | Asset provenance infrastructure | Manifest, licence tooling, the `/rv/tmp` audio import |
| 5 | World data format | The full layout JSON authored, validated, loaded |
| 6 | House blockout | The generated shell renders; you can look at the house |
| 7 | Collision and player controller | You can walk through the blockout |
| 8 | First-person camera | It feels right |
| 9 | Room/portal visibility | **The core system.** Culling works and is proved |
| 10 | Exterior and property | Terrain, fences, gates, drive, garden |
| 11 | Neighbourhood background | The house is no longer floating in nothing |
| 12 | Materials and textures | The blockout becomes a building |
| 13 | Static furniture | The building becomes rooms |
| 14 | Interactable framework | The 12 behaviours and the data model |
| 15 | Doors and windows | Portals become dynamic |
| 16 | Lights and switches | The house can be lit |
| 17 | Containers | 214 things open |
| 18 | Kitchen and refrigerator | |
| 19 | Plumbing | |
| 20 | Toilets | |
| 21 | Television and video | |
| 22 | Time | The clock runs |
| 23 | Sun | The sun moves, the glare works, the clock overlay works |
| 24 | Moon and stars | |
| 25 | Sky and clouds | |
| 26 | Weather core | |
| 27 | Rain | |
| 28 | Snow | |
| 29 | Storm, lightning, thunder | |
| 30 | Hail and wind | |
| 31 | Audio foundation | |
| 32 | Room-aware 3-D audio | |
| 33 | Dog | |
| 34 | Cat | |
| 35 | Third-person avatar | |
| 36 | Character customisation | |
| 37 | Character animation | |
| 38 | Stair animation and foot IK | |
| 39 | Persistence | |
| 40 | Reset House | |
| 41 | Culling and LOD optimisation | Budgets met |
| 42 | Streaming and loading | Only if §27.1's measurement demands it |
| 43 | Debug tools | |
| 44 | Automated tests | The full suite |
| 45 | Visual polish (incl. curtains/blinds, decoration pass, habitation pass) | |
| 46 | Linux desktop stabilisation | **Feature-complete desktop** |
| 47 | Web preparation | |
| 48 | Web implementation | |
| 49 | Android preparation (gated on BL-13 upstream) | |
| 50 | Android touch UI | |
| 51 | Android implementation | |
| 52 | Final optimisation and polish | |

Phases are not strictly sequential; `plan.md` records dependencies per task. In particular
phases 22–30 (environment) and 31–32 (audio) can proceed in parallel with 13–21 (interiors),
and phase 9 must precede 13.

---

## 75. Risk register

| ID | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R-01 | ~~No high-quality CC0 rigged **dog** exists~~ — **realised by `HOUSE-00291`** | Realised | High — it is a headline feature | All three named attempts failed the fixed quality/provenance bar. Build it ourselves in `HOUSE-02041`–`HOUSE-02043`, budgeted at 5 days of Blender work, then verify the pipeline in `HOUSE-02044`. |
| R-02 | ~~No high-quality CC0 rigged **cat** exists~~ — **realised by `HOUSE-00292`** | Realised | High | All three sourcing attempts failed the fixed quality/provenance bar. Build it ourselves in `HOUSE-02091`–`HOUSE-02093`, then verify the pipeline in `HOUSE-02094`. |
| R-03 | ~~No suitable cleanly licensed realistic **car** exists~~ — **realised by `HOUSE-00293`** | Realised | Medium | All three sources failed provenance, ordinary realism or the 45,000-triangle budget. Author the fixed unbranded estate in `HOUSE-01035`, prove it in `HOUSE-01036`, then place it in `HOUSE-00998`; it is seen in a dim garage at ≤ 6 m, so a good but not spectacular model suffices. |
| R-04 | ~~MakeHuman output quality or licence is not what we expect~~ — **retired by `HOUSE-00269` and `HOUSE-00294`** | Retired | High | Core output is CC0 and the two measured, reviewed MPFB 2.0.17 bodies pass silhouette, manifold, UV, height and triangle-budget checks after one unsubdivide pass. The hand-modelled fallback is not triggered; rig deformation remains separately owned by `HOUSE-00295`/`HOUSE-02131`. |
| R-05 | ~~Mocap licence (CMU) turns out to be restrictive~~ — **retired by `HOUSE-00270`; technical fallback triggered by `HOUSE-00295`** | Retired | Medium | CMU permits derivatives in products while raw capture data remains external. Six pinned walk trials nevertheless failed the MPFB rig/bind deformation bar, so `HOUSE-02219` uses the already-budgeted fallback: author 18 clips directly on the final game rig, ~8 days. |
| R-06 | ~~`.fx` compilation through Wine is fragile or fxc rejects our HLSL~~ — **largely retired 2026-09-06.** `HOUSE-00087` compiled a two-technique `.fx` through a genuine Microsoft `fxc` (DXSDK June 2010) under Wine and drew with it, with parameters arriving exactly. The one fragility found was real but ours to fix and now fixed: `cna-content` passes Unix paths to a Windows tool, so the launcher must be `tools/effects/fxc-wine.sh`, not bare `wine`. | Low | Medium | Tier S is complete without it (by design). Compiled `.xnb` is committed once it works. |
| R-07 | Lightmap baking for 78 cells is slow or produces seams | Medium | Medium | Per-cell bakes are independent and parallel; seams are handled by the packer's 4-texel gutter; a fallback to per-room vertex lighting exists and is 3 lines |
| R-08 | Portal traversal is slower than budgeted in the worst case | Low | High | The screen-area cutoff and depth caps bound it; the design has a `maxVisibleCells` hard stop that degrades gracefully (drop the smallest-frustum cells first) |
| R-09 | The 780M cannot hold 550 MB of GPU resources alongside the compositor | Medium | Medium | The residency system and the quality tiers exist for exactly this; the Medium tier halves texture memory |
| R-10 | Audio voice starvation in a storm | Low | Medium | The voice manager's category limits and virtualisation |
| R-11 | Emscripten Asyncify + our frame structure interact badly | Medium | Medium | Phase 47 starts with a minimal-scene Web build before any porting work |
| R-12 | Android House package or runtime fails after the CNA gate | **High** | Medium | Investigate on the available emulator and record exact ownership. Android remains required for DONE; an upstream blocker never makes the project complete or shippable as DONE. |
| R-13 | Scope creep in furnishing (2 400 placements is a lot of authoring) | High | Medium | Kit-based placement, per-room placement tasks with fixed counts, and a "good enough, move on" rule enforced by the phase's exit criteria |
| R-14 | The weather system becomes a research project | Medium | Medium | The archetype table and the rate-limit table are fixed *now*; tuning is a bounded task, not an open-ended one |
| R-15 | Save-format churn during development invalidates test saves | High | Low | The migration chain is built in phase 39 before any content depends on it; test fixtures are regenerated by a script |
| R-16 | ~~The `.cnb`'s skin-joint order or bone naming differs from the source `.glb`~~ — **RESOLVED 2026-09-06 by `HOUSE-00074`, and the answer was the fallback, not the hoped-for case.** Every skin joint resolves by name in `Model::Bones`, and the compiled `.cnb` is byte-identical across rebuilds, so the order is stable. But **vertex blend indices are skin-local, not bone indices**, so the sidecar **must** carry the joint names in blend-index order — nothing in the compiled `Model` reproduces that list. | — (resolved) | High | The `.chanim` format of `HOUSE-00166` carries the joint-name list; `ClipLibrary::BindTo` makes any disagreement a fatal load error (`HOUSE-00167`) and CI re-checks every character (`HOUSE-00225`) |
| R-17 | The 24-bit → 16-bit audio conversion degrades the NOX material audibly | Low | Low | 16-bit at 44.1 kHz is CD quality; a listening check on 10 files in phase 4 |
| R-18 | The house is so large that authoring it is the whole project | Medium | High | This is the real risk. Mitigations: procedural shell generation, kit-based furnishing, per-room task granularity, and an explicit "playable but sparsely furnished" milestone (§78) reached long before the furnishing is done |

---

## 76. Open questions

Genuine unresolved decisions, each with an owner-decision task in `plan.md`. Nothing here is a
placeholder for work not yet thought about.

| ID | Question | Why it is open | Decision task |
|---|---|---|---|
| ~~Q-01~~ | ~~Does `RenderTarget2D` with `SurfaceFormat::Single` work on EasyGL/OPENGLES3?~~ | **CLOSED 2026-09-06 by `HOUSE-00083`: yes, at all four stages.** A 2048² `Single`+`Depth24` target was created, rendered into, read back bit-exactly and sampled as an effect texture. The feature matrix row was the stale one. **Tier E uses a real float shadow map and needs no RGBA8 packing.** `HOUSE-00110` adds the boundary: `Single` is a render target **only** — a `Texture2D` of that format cannot be created at either graphics profile. | — (closed) |
| Q-02 | Is the NOX collection's CC0 declaration confirmed by the publisher's live page? | No network access during planning; the README says CC0 on its face | `HOUSE-00276` |
| Q-03 | Does MakeHuman's asset licence permit CC0 redistribution of generated meshes in this configuration? | Believed yes; must be read, not assumed | `HOUSE-00269` |
| Q-04 | Is CMU mocap redistributable as derived baked animation? | Believed yes ("free for all uses"); must be read | `HOUSE-00270` |
| Q-05 | Should the player's reflection appear in mirrors? | A planar reflection pass per mirror is affordable only if the mirror's room is small and the pass is cheap | `HOUSE-02685` — measure, then decide; default is no (D-19) |
| Q-06 | Should the car eventually be drivable? | Out of scope for v1 by decision; revisit only after §79 | deferred; see `plan.md` §Deferred — a research note, not an implementation |
| Q-07 | Is a background loading thread needed? | Depends on the cold-start measurement | `HOUSE-02451` gates `HOUSE-02452`–`02463`; `HOUSE-02462` decides |
| Q-08 | Do we need `OcclusionQuery` for exterior occlusion? | Depends on a measurement | `HOUSE-02401` with a go/no-go gate |
| Q-09 | Which of the two remaining rain-on-roof sourcing options sounds right? | An audition question, not a technical one | `HOUSE-00286` |
| Q-10 | Should the pause menu pause the world by default? | Taste; both are implemented and it is a setting | `HOUSE-02794` — pick a default after playtesting |
| Q-11 | Is 24 minutes the right day length after playtesting? | The analysis favours it; play may disagree | `HOUSE-02793` — re-evaluate at the feature-complete milestone; the setting exists either way |

---

## 77. Explicit decisions and recommendations

| ID | Decision | Rationale |
|---|---|---|
| D-01 | **`CNA_GRAPHICS_RENDERER=OPENGLES3` (EasyGL) on Linux** | The verified reference configuration for 80 ported samples; identical feature family to `WEBGL2` and the eventual Android target; the only renderer with pixel-verified `OcclusionQuery` |
| D-02 | **`CNA_CNAEXT=OFF`, always** | The forbidden engine layer then does not exist in the binary — the strongest possible enforcement of the XNA-only rule |
| D-03 | **Two rendering tiers; Tier S is complete on its own** | Compiled effects work (SAMPLE-038 proves it) but depend on Wine + fxc; the game must never be hostage to that |
| D-04 | **Portal visibility with frustum reduction; no stencil, no PVS, no BSP** | Stencil is unavailable (BL-02); PVS cannot express dynamic door states; frustum reduction computes the exact answer at runtime |
| D-05 | **`OcclusionQuery` only for sun/moon glare** | Portal culling already answers the indoor question; queries are latent, per-object-costly and boolean-only on ES3 |
| D-06 | **A partially open door does not shrink its portal; the leaf occludes inside the target cell** | Geometrically correct, conservative, and produces exactly the right picture |
| D-07 | **Project-owned kinematic collision; no Bullet/Jolt** | Nothing in the brief needs rigid-body dynamics; the dependency's cost is real and its benefit is zero |
| D-08 | **`DualTextureEffect` multi-pass lightmapping for static interiors** | It is XNA's lightmapping effect; baking captures the *shape* of light while the runtime scales its *intensity* |
| D-09 | **Walk mode toggles with `Shift` and is persisted in settings** (mirrored in the save) | It is a preference, not world state; the brief left this open and it is now closed |
| D-10 | **24 real minutes per simulated day (`timeScale = 60`), configurable** | 1 real second = 1 simulated minute removes an entire class of arithmetic error from tests, logs and settings; the sun's 0.25°/s is smooth and perceptible |
| D-11 | **A real solar-position model at 40.05° N** | Seasonal day length, correct sunrise azimuths and realistic twilight for free |
| D-12 | **Continuous lunar phase, rendered through a CPU-generated 128² mask (Tier S) or a shader terminator (Tier E)** | The brief asks for continuity; 8 textures cannot provide it |
| D-13 | **Continuous weather state with rate limits; no `isRaining` boolean anywhere** | The rate table makes absurd transitions arithmetically impossible; a CI lint enforces the absence of the boolean |
| D-14 | **The lunation runs at real astronomical speed by default** | Correctness; the phase visibly changes across sessions because the date is saved; a speed multiplier exists for those who want more |
| D-15 | **A running tap keeps running across save/load; baths fill and then overflow harmlessly** | The premise is that the house remembers; the alternative would be the most conspicuous possible violation of it |
| D-16 | **Toilet contents persist; flushing clears them; no anatomy, no jokes** | Exactly what the brief asks, at a proportionate size (~180 lines, 2 meshes, 4 sounds) |
| D-17 | **The television has two backends behind one interface, and the fallback is built and tested on Linux from day one** | Web and Android cannot decode video (BL-05); a fallback discovered late is a fallback that does not work |
| D-18 | **Room-aware audio is computed by `cna-house` over the portal graph, not by `Apply3D`** | CNA's 3-D model is a simplified stereo pan with no occlusion (BL-11); the portal graph gives us something better for ~40 µs |
| D-19 | **Mirrors use baked static cube maps; the player is not reflected** | A planar reflection pass per mirror per frame is not worth the budget; the mirrors are placed so it is not conspicuous. Revisit in Q-05. |
| D-20 | **Delta saves against a canonical initial state, in JSON, versioned, atomic, migrated** | Small, diffable, forward-compatible with new interactables by construction, and it makes *Reset House* a one-line operation |
| D-21 | **No music** | The house is the soundtrack |
| D-22 | **No swimming pool** | Analysed and rejected: high cost, marked optional, and the garden budget buys more believability elsewhere |
| D-23 | **The car is a prop in v1** | Driving is a different project; the brief permits this |
| D-24 | **≈ 420 unique models, kit-based placement, ≤ 14 uses per model** | The only way 2 400 placements is affordable and does not read as repetition |
| D-25 | **Every asset has a manifest row with a hash and a licence, enforced in CI** | A large asset collection without this becomes unshippable within months |
| D-26 | **Family photographs are rendered fictional images of the game's own characters** | Legally clean, thematically coherent, and we own them outright |
| D-27 | **Single-threaded in v1** | Emscripten threading changes the module ABI and needs COOP/COEP; the cost must be justified by a measurement |
| D-28 | **Crouching exists only in the attic, automatically** | The roof geometry demands it; a general crouch would be a feature nobody asked for |
| D-29 | **The pause menu does not pause the world by default** | A house that stops when you look away is less convincing; it is a setting |
| D-30 | **Android remains a required DONE platform; the CNA graphics gate passed on 2026-09-26** | House Simulator still must build, run and complete its device checklist; `cna-house` does not modify CNA |

---

## 78. First playable milestone

> **Superseded 2026-09-21 by `plan.md`'s gates G1–G5. Kept as history.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

**Definition.** A build in which a player can start on the road, open their gate, walk to the
front door, enter the house, walk through every level including the basement and the attic, open
and close doors, switch lights on and off, and watch the sun move — with correct culling, correct
collision, and no placeholder magenta.

**Contents:**

| Included | Not yet |
|---|---|
| The generated architectural shell, fully materialled | Furniture beyond ~15 hero pieces per major room |
| All 186 portals, all 62 doors, all 62 windows working | Containers, kitchen appliances, plumbing, toilets, TV |
| Portal visibility with the debug overlays and its test suite | LOD and impostors (the neighbourhood is LOD1 only) |
| Collision, the player controller, first-person camera, both walk speeds | Third person, the avatar, animation |
| 84 light groups with baked lightmaps and daylight | Tier E, shadow maps, sun patches |
| The clock, the sun, the sky dome, day/night | Moon, stars, weather |
| Footsteps, room tone, door and switch sounds | The full audio system, room-aware propagation |
| Save/load of doors, lights, windows, the player and the clock | The rest of the save schema (it is a delta — adding is trivial) |
| The exterior: terrain, fences, gates, drive, garden shell | Vegetation detail, garden props, the shed interior |
| The debug overlays `F1`–`F5` | The rest |

**Exit criteria:** 60 FPS at 1920 × 1080 on the dev machine at the *High* tier in all 10
performance scenarios; the visibility test suite green; the collision guarantee suite green;
a 20-minute unattended random walk with no boundary escapes, no geometry penetration and no
crash.

Estimated position in the plan: end of **phase 16**.

---

## 79. Feature-complete desktop version

> **Superseded 2026-09-21 by `plan.md`'s Definition of DONE. The checklist below is history.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

Every requirement in the brief, on Linux, at the quality bar this document sets.

**Checklist:**

* [ ] All 95 cells furnished to the §59.1 density, with the §59.3 habitation pass done
* [ ] All 640 interactables implemented, data-driven, persisted
* [ ] Kitchen complete, refrigerator with 32 items and its interior portal
* [ ] 17 water outlets, 3 fillable baths, 3 showers
* [ ] 12 toilets with the full waste-state model
* [ ] 3 televisions with both backends
* [ ] 214 containers with fill kits
* [ ] 84 light groups, baked + dynamic, with the daylight and flood model
* [ ] Day/night with a real solar model, sun glare, the sun/moon clock overlay
* [ ] Moon with a continuous phase, stars with a real catalogue, satellites and meteors
* [ ] The full weather system: 14 archetypes, rate-limited transitions, seasons, determinism
* [ ] Rain, snow with accumulation, hail with bounce, storms with lightning and delayed thunder,
      wind driving everything
* [ ] The dog and the cat, with navigation, behaviour, perches and spatial audio
* [ ] First and third person, the modular avatar with 414 720 combinations, 18 clips, blending,
      stride matching, foot IK, stair-specific locomotion
* [ ] Room-aware spatial audio over the portal graph, with the muffle cross-fade and apparent
      positioning
* [ ] Persistence of everything in §65.2, versioned and migrated, with a fuzz-tested loader
* [ ] Reset House with both scopes
* [ ] The neighbourhood, LOD, impostors, and a believable bounded world
* [ ] Both rendering tiers, with Tier E shadows, sky, precipitation, wetness and composite
* [ ] All budgets in §71 met in all 10 scenarios
* [ ] The full test suite green: ~900 unit, ~220 integration, ~120 render, 10 performance
* [ ] Every asset manifested and licensed; `THIRD-PARTY-ASSETS.md` generated and shown in-game
* [ ] Zero `TODO`, `TBD` or `FIXME` in shipping code paths
* [ ] `docs/xna-deviations.md` complete and enforced; no runtime source names a CNA symbol

**Exit criteria:** the checklist, plus a 2-hour unattended soak test at 60 FPS with no leak
(RSS growth < 20 MB/hour), no crash, no audio starvation, and no save corruption across 200
autosave cycles.

---

## 80. Portability readiness criteria

### 80.1 Web readiness (entering phase 48)

Meeting every criterion below on Linux makes the Web port *likely* to succeed; it does not make it
certain. The desktop `OPENGLES3` build is a strong baseline (§7.1), and the browser adds
constraints no desktop run exercises. Phase 47's Emscripten spike is what actually settles it.

| Criterion | How it is proved |
|---|---|
| No POSIX filesystem use outside `SaveStore`'s desktop implementation | `check_xna_only.py` extended to lint `std::filesystem` and `fopen` |
| No thread creation | Same lint |
| `Game::Run()` owns the loop; no custom loop, no `emscripten_set_main_loop` | Lint + review |
| All GPU resources reconstructible from CPU-side data | A context-loss simulation test on desktop (EasyGL's `DebugSimulateContextLoss`) that destroys and restores every resource and compares a rendered frame |
| Audio starts only after a user gesture | The title screen gate is on every platform, so the desktop build exercises it |
| Video abstracted behind `ITvSource` with the sequence backend tested on Linux | A test forces `SequenceTvSource` on desktop |
| Content split into packs, each ≤ 60 MB | `budget_report.py` |
| Total compressed download ≤ 180 MB at the Web tier | Same |
| Web quality tier defined and reachable on desktop | A `--quality web` command line renders at the Web tier and meets its budget |
| No `SurfaceFormat` other than `Color` for sampled textures | Lint over the material data |
| No MRT, no stencil, no geometry/tessellation stages | Already true by BL-02/BL-03 |
| The Emscripten toolchain builds a minimal scene | A phase-47 spike, not the whole game |

### 80.2 Android readiness (entering phase 49)

| Criterion | How it is proved |
|---|---|
| **Upstream:** CNA's Android cross-compile succeeds (`sharp-runtime` Task 920 fixed) | A build |
| **Upstream:** `CNA_GRAPHICS_RENDERER=OPENGLES3` selectable and buildable for `arm64-v8a` | A build |
| **Upstream:** a CNA graphics sample runs on a device or emulator | A screenshot |
| All input through `IInputSource`; no direct `Keyboard`/`Mouse` reads in systems | Lint |
| `Platform` capability struct drives HUD and defaults | Review + a forced-touch mode on desktop |
| UI laid out in virtual units with safe-area insets | A forced 18:9 with insets renders correctly on desktop |
| Android quality tier defined and reachable on desktop | `--quality android` meets its budget |
| Content packs fit the APK+OBB budget | `budget_report.py` |
| Touch HUD implemented and testable on desktop | A `--force-touch` flag |
| Lifecycle: pause / resume / background handling routed through `Game`'s events | Desktop focus-loss handling exercises the same path |

---

## 81. Requirements traceability

> **Amended 2026-09-21. Rows for removed requirement areas (interaction, appliances, plumbing, toilets, television, pets, avatar, animation, household persistence, reset) are void. `plan.md`'s Definition of DONE is the completion reference.** ([ADR-0014](docs/decisions/ADR-0014-showcase-scope.md))

Every numbered requirement area of the brief, mapped to this document and to `plan.md`.

| Brief § | Requirement | Doc § | Plan phase | Task IDs | MUST/SHOULD/OPT |
|---|---|---|---|---|---|
| 1 | Two documents only; approval gate | — | — | — | MUST |
| 2 | XNA 4.0 only; no CNAEXT graphics | 4, 7, 47.0 | 0, 1, 2, 3 | HOUSE-00007, 00019–00021, 00063, 00076, 00121–00122, 00136, 00160–00161, 00166–00167, 00223–00225, 03074 | MUST |
| 3 | Linux first; Web/Android later | 8, 9, 80 | 0, 2, 47–51 | HOUSE-00035, 00121, 02841–03042 | MUST |
| 4 | Project concept, coherence, no asset flip | 1, 2, 19, 59 | 12, 13, 45 | HOUSE-00891–01034, 02681–02714 | MUST |
| 5 | World layout, believable bounds | 10, 11 | 5, 10 | HOUSE-00366–00372, 00761–00783 | MUST |
| 6 | Very large plausible house, B1/L0/L1/L2/L3 | 12, 13 | 5, 6 | HOUSE-00366–00400, 00451–00484 | MUST |
| 7 | Required spaces incl. basement, attic, garage | 13 | 5, 6, 13 | HOUSE-00367–00372, 00998, 01016–01026 | MUST |
| 8 | Garden and property | 11 | 10 | HOUSE-00761–00783 | MUST |
| 9 | Machine-readable floor plan, adjacency, IDs | 14, 15, 16 | 5 | HOUSE-00341–00420 | MUST |
| 10 | Render only what is needed | 25 | 9, 41 | HOUSE-00661–00697, 02391–02409 | MUST |
| 11 | Graphics quality within XNA | 7, 22, 23, 28, 29 | 12, 16, 45 | HOUSE-00891–00918, 01251–01278, 02702–02712 | MUST |
| 12 | Stateful indoor lighting | 28, 30, 53 | 16 | HOUSE-01251–01278 | MUST |
| 13 | Day/night cycle, configurable | 35 | 22 | HOUSE-01531–01541 | MUST |
| 14 | Sun, glare, sun-clock overlay | 32 | 23 | HOUSE-01561–01578 | MUST |
| 15 | Night sky, stars, moon phases | 33, 34 | 24 | HOUSE-01601–01619 | MUST |
| 16 | Structured weather model | 36, 42 | 26 | HOUSE-01681–01707 | MUST |
| 17 | Weather visuals: rain/snow/hail/storm/wind | 37–41 | 27–30 | HOUSE-01741–01890 | MUST |
| 18 | Sky/cloud system | 31 | 25 | HOUSE-01641–01655 | MUST |
| 19 | First-person camera | 44 | 8 | HOUSE-00621–00634 | MUST |
| 20 | Two walk speeds, Shift toggles | 43 | 7 | HOUSE-00555–00558 | MUST |
| 21 | Third-person realistic character | 45, 46 | 35, 36 | HOUSE-02131–02187 | MUST |
| 22 | Character animation incl. stairs | 47, 48 | 2, 3, 37, 38 | HOUSE-00166–00167, 00223, 02211–02284 | MUST |
| 23 | Physics/collision | 49 | 7 | HOUSE-00541–00620 | MUST |
| 24 | Contextual interaction system | 50 | 14 | HOUSE-01121–01146 | MUST |
| 25 | Doors | 51 | 15 | HOUSE-01181–01190, 01198, 01200–01202 | MUST |
| 26 | Windows | 52 | 15, 45 | HOUSE-01191–01199, 02681–02683 | MUST |
| 27 | Light switches | 53 | 16 | HOUSE-01252–01254, 01270, 01273 | MUST |
| 28 | Cabinets/drawers/closets | 54 | 17 | HOUSE-01311–01323 | MUST |
| 29 | Kitchen and refrigerator | 55 | 18 | HOUSE-01361–01383 | MUST |
| 30 | Plumbing and running water | 56 | 19 | HOUSE-01411–01429 | MUST |
| 31 | Toilets and waste state | 57 | 20 | HOUSE-01461–01474 | MUST |
| 32 | Television with video and sound | 58 | 21 | HOUSE-01491–01504 | MUST |
| 33 | Furniture and interior detail | 59 | 13, 45 | HOUSE-00971–01034, 02686–02689 | MUST |
| 34 | Wall decoration, family photographs | 59.4, 20.4 | 45 | HOUSE-02687–02689 | MUST |
| 35 | Dog and cat | 60, 61 | 33, 34 | HOUSE-02041–02106 | MUST |
| 36 | Audio and spatial sound | 62, 64 | 31, 32 | HOUSE-01911–02009 | MUST |
| 37 | Footstep audio by surface | 62.4 | 31, 38 | HOUSE-01919–01921, 02278–02279 | MUST |
| 38 | House state persistence | 65 | 39 | HOUSE-02301–02333 | MUST |
| 39 | Reset House | 66 | 40 | HOUSE-02371–02380 | MUST |
| 40 | Canonical initial state | 65.6 | 5, 39 | HOUSE-00395, 02305 | MUST |
| 41 | Asset strategy | 19 | 4 | HOUSE-00261–00302 | MUST |
| 42 | Generated glTF assets, validated | 21.4 | 3, 6, 10, 11 | HOUSE-00186–00190, 00451–00482, 00762–00769, 00841–00848 | MUST |
| 43 | Content pipeline | 18 | 3 | HOUSE-00181–00222 | MUST |
| 44 | Geometry partitioned for visibility | 17.4, 23 | 3, 6, 13 | HOUSE-00215, 00473, 01029 | MUST |
| 45 | Scene architecture | 17 | 2 | HOUSE-00121–00165 | MUST |
| 46 | Background neighbourhood | 11.4, 26 | 11, 41 | HOUSE-00841–00855, 02391–02395 | MUST |
| 47 | Weather ↔ house interaction | 30, 37.5, 39, 64.6 | 27–32 | HOUSE-01744, 01750–01752, 01834, 01884, 02000 | MUST |
| 48 | Performance targets | 71 | 41, 44 | HOUSE-02391–02409, 02597 | MUST |
| 49 | Loading/streaming | 27 | 42 | HOUSE-02451–02463 | SHOULD (gated on HOUSE-02451) |
| 50 | Material system | 22 | 12 | HOUSE-00891–00908 | MUST |
| 51 | Testing strategy | 70 | 44 | HOUSE-02571–02601 | MUST |
| 52 | Debug tools | 69 | 43 | HOUSE-02501–02515 | MUST |
| 53 | Settings | 68 | 43 | HOUSE-02516–02531 | MUST |
| 54 | Future Android touch controls | 9.3 | 50 | HOUSE-02991–03002 | SHOULD (later) |
| 55 | Future Web target | 9.1, 80.1 | 47, 48 | HOUSE-02841–02905 | SHOULD (later) |
| 56 | Source structure | 17.5 | 0 | HOUSE-00001, 00022 | MUST |
| 57 | C++ quality | 17.2 | 0, 2 | HOUSE-00023–00029, 00124, 00128–00130 | MUST |
| 58 | Dependency policy | 49.1, 82.3 | 0 | HOUSE-00013, 00017 | MUST |
| 59 | `cna-house.md` structure | this document | — | — | MUST |
| 60 | `plan.md` requirements | `plan.md` | — | — | MUST |
| 61 | Plan phases | 74 | — | — | MUST |
| 62 | Do not hide the size | `plan.md` §Task count | — | — | MUST |
| 63 | MUST/SHOULD/OPTIONAL | this table + every task's `pri` field | — | — | MUST |
| 64 | Blocker table | 6 | 1 | HOUSE-00061–00120 | MUST |
| 65 | Asset manifest design | 20.3 | 3, 4 | HOUSE-00195–00198, 00261 | MUST |
| 66 | Realism validation | 70.5 | 5, 44 | HOUSE-00360–00362, 02596 | MUST |
| 67 | Source evidence for CNA claims | 5, 6, 82 | 1 | HOUSE-00112–00119 | MUST |
| 68 | Planning pass may run probes | — | 1 | HOUSE-00062–00114 | — |
| 69 | Overturn assumptions where warranted | 77 (D-06, D-07, D-10, D-19, D-22, D-23) | — | — | — |
| 70 | Finish: validate, traceability, no vague placeholders, commit | 76, 81 | — | — | MUST |

**Vagueness sweep.** `cna-house.md` and `plan.md` were searched for `TODO`, `TBD`, `FIXME`,
`implement later`, `etc.`, `and so on`, and `???`. The only surviving unresolved items are the
eleven explicitly-owned questions in §76, each with a named decision task. Every other statement
is a decision, a measurement, or a cited fact.

---

## 82. Source evidence index

### 82.1 Repository revisions audited

| Repository | Path | Revision | Branch | Date |
|---|---|---|---|---|
| CNA | `/rv/data/development/github.com/openeggbert/cnanext` | `d42203805057d43dc092bb713613bcc945ad8d5a` | `next` | 2026-09-06 15:07:20 +0200 |
| sharp-runtime | `/rv/data/development/github.com/openeggbert/sharp-runtimenext` | `30ccdef30ba4d27534864b0777729b9e78ee8a21` | `next` | 2026-09-06 10:52:16 +0200 |
| CNA samples | `/rv/data/development/github.com/openeggbert/cna-samples` | `48f6fc9d2ec72d574732a4faa42fb2543fcc78bd` | `develop` | 2026-09-06 15:08:11 +0200 |

### 82.2 Files and documents cited

| Claim | Source |
|---|---|
| CNA is a C++ reimplementation of XNA 4.0; FNA is the reference; XNA wins ties | `cnanext/CLAUDE.md` §Project Overview |
| The CNAEXT engine layer lives in `modules/graphics-ext/` behind `CNA_CNAEXT` (OFF by default), guarded by a ctest | `cnanext/CLAUDE.md` §"The CNAEXT Engine Layer" |
| 49 renderer identities; Linux default `OPENGLES3`; Emscripten `WEBGL2`; Android `SDL_RENDERER` | `cnanext/CLAUDE.md` §Platform Boundary; `docs/android-graphics-limitations.md` |
| `Model` runtime API fully audited; `.model.json` loader has real gaps | `docs/model-content-pipeline-support.md` |
| Real binary `.xnb` `ModelReader` with full bone hierarchy | `docs/xnb-content-pipeline-support.md` |
| `SkinnedEffect` `MaxBones=72`, `WeightsPerVertex` GPU-enforced, pixel-verified on 3 renderers | `docs/skinnedeffect-support.md` |
| `OcclusionQuery` on EasyGL; boolean `PixelCount` on ES3; measured 9 788 px → 1 | `docs/occlusionquery-support.md` |
| Video is an optional FFmpeg backend; Emscripten/Android fall back | `docs/video-backend.md` |
| `VideoPlayer::GetTexture()` exists | `modules/media/include/Microsoft/Xna/Framework/Media/Video/VideoPlayer.hpp:137` |
| Compiled `Effect` from D3D9 bytecode; no embedded HLSL compiler; EasyGL behind a build option | `docs/fx-compiled-effects.md`, `docs/shader-effect-vs-fx-bytecode.md` |
| `.fx` route needs `--fx-compiler` + `--fx-compiler-launcher wine` | `docs/content-pipeline.md:389-432` |
| glTF → CNB Model with embedded clips; `generateChildAssets`; skin grouping | `docs/content-pipeline.md:370-475` |
| `cna_add_content()` CMake function | `cnanext/cmake/ToolContentPipeline.cmake:48` |
| `SkinningData`/`AnimationClip`/`AnimationPlayer` mirror Microsoft's XNA Skinned Model Sample — i.e. they are *sample* code, not XNA Framework API, which is why `cna-house` owns its own (§47.0) | `modules/graphics/include/Microsoft/Xna/Framework/Graphics/AnimationPlayer.hpp:18-48,100-107` |
| `Model::Tag` carries the first skin's `SkinningData`; `getSkinsEXTProperty` is a `CNAEXT` multi-skin accessor — neither is used by `cna-house` | `modules/graphics/include/Microsoft/Xna/Framework/Graphics/Model.hpp:152-171,277-285` |
| glTF and XNA agree on handedness/up/forward; no conversion; winding differs | `docs/gltf-conventions.md:15-25` |
| EasyGL ignores `ClearOptions::Stencil`; `ReferenceStencil` unconnected; RT depth is always `DepthComponent24`, no stencil | `docs/graphics-renderer-feature-matrix.md` state-object table; `docs/rendertarget-support.md:193` |
| EasyGL MRT attachment 1 stays black | `docs/rendertarget-support.md:198` |
| `Texture2D` mip-level `SetData` no-op on Vulkan/Bgfx | feature matrix |
| Non-`Color` `SurfaceFormat` marked blocked (Task 732) | feature matrix |
| SAMPLE-038 used a `SurfaceFormat.Single` 2048² RT with `Depth24`, two effect techniques by name, on native OPENGLES3 and real Chrome WEBGL2 | `cna-samples/plan.md:785` |
| 80 samples complete, native + browser | `cna-samples/plan.md:192` |
| `cna-samples` build shape: sibling `cnanext`/`sharp-runtimenext`, `OPENGLES3`/`WEBGL2`, `CNA_EASYGL_COMPILED_EFFECTS=ON` | `cna-samples/CMakeLists.txt:15-50` |
| Emscripten `Game::Run()` uses Asyncify and returns; no custom main loop | `docs/emscripten-mainloop-game-lifetime.md` |
| WebGL 2 has no WebGL 1 fallback; threads are opt-in and need COOP/COEP; context loss is handled | `docs/web-emscripten-graphics-limitations.md` |
| Android build is currently broken by two `sharp-runtime` NDK bugs; renderer defaults to 2D-only | `docs/android-graphics-limitations.md` |
| `Apply3D` is a simplified stereo model; no cones/curves/HRTF/reverb; Doppler pitch risk | `docs/cna_audio_deep_audit_2026-07-17.md:131,161` |
| `SoundEffectReader` accepts PCM8/16, float, MS-ADPCM, IMA-ADPCM (no 24-bit) | `docs/xnb-content-pipeline-support.md` audio matrix |
| `Mouse::SetPosition` exists | `modules/input/include/Microsoft/Xna/Framework/Input/Mouse.hpp:49` |
| `TouchPanel` and gestures exist | `modules/input/include/Microsoft/Xna/Framework/Input/Touch/` |
| ~~`StorageDevice` Linux root is `$XDG_DATA_HOME/<app>`~~ → the `<app>` component is the literal `game` unless the forbidden `SetAppNameEXT` is called; identify by container name (`HOUSE-00102`) | `modules/storage/src/StorageDevice.cpp:75,88-109` |
| `Text.Json` is a sharp-runtime component | `sharp-runtimenext/modules/text-json/CMakeLists.txt:5` |
| `DrawInstancedPrimitives` exists | `modules/graphics/include/Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp:424` |
| NOX collection contents, formats and CC0 declaration | `/rv/tmp/Essentials_Series_NOX_SOUND/` — directory census, `ffprobe`, `Essentials_Series_README.pdf` |
| `fxc.exe` available | `/rv/tmp/samples/_tools/directx-sdk-june-2010/extract/DXSDK/Utilities/bin/x86/fxc.exe` |
| The official XNA 4.0 content pipeline runs here under Wine | `/rv/tmp/samples/SAMPLE-038-ShadowMappingSample_4_0/scripts/build-original.sh`, `/rv/tmp/samples/_tools/xna-game-studio-4-refresh/` |
| Blender 4.3.2 available | `/usr/bin/blender` |
| Dev machine GPU and memory | `glxinfo -B`, `free -g`, `nproc` |

### 82.3 Third-party runtime dependencies

`cna-house` adds **no third-party runtime dependency of its own.** Its entire runtime dependency
set is CNA and sharp-runtime, and whatever CNA itself pulls in (SDL3, and optionally FFmpeg and
MojoShader, both selected by CNA's own CMake options).

| Candidate | Purpose | Verdict |
|---|---|---|
| Bullet / Jolt / PhysX | Physics | **Rejected** — §49.1 |
| nlohmann/json, RapidJSON | JSON | **Rejected** — `System::Text::Json` from sharp-runtime is already in the build and is the project-idiomatic choice |
| EnTT | ECS | **Rejected** — §17.1 |
| Dear ImGui | Debug UI | **Rejected** — `SpriteBatch`/`SpriteFont` overlays are sufficient and keep the XNA-only rule clean |
| Recast/Detour | Navigation | **Rejected** — a 300-node waypoint graph with A* is ~150 lines |
| FMOD / Wwise | Audio | **Rejected** — XNA audio is the requirement |
| stb_image, tinygltf, cgltf | Asset loading | **Rejected at runtime** — everything is compiled offline by `cna-content` |
| Catch2 | Testing | **Rejected** — GoogleTest, matching CNA's own convention |
| **GoogleTest** | Testing | **Accepted** — test-only, matches CNA, already in the build environment |
| **Blender, ffmpeg, fxc, Python** | Offline tooling | **Accepted** — build-time only, never linked |

---

*End of `cna-house.md`.*
