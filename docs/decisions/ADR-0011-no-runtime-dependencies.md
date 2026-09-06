# ADR-0011 — No third-party runtime dependencies

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-06 |
| **Task** | `HOUSE-00017` |
| **Depends on** | [ADR-0001](ADR-0001-xna-only.md) |
| **Owns** | `cna-house.md` §82.3 |

## Context

`cna-house` links CNA and, through it, sharp-runtime; CNA in turn pulls SDL3 always, and FFmpeg and
MojoShader when its own options select them. The question this record settles is whether
`cna-house` adds anything of its own on top.

Three forces push toward "no". First, the project's point is a demonstration of the XNA 4.0 surface
(ADR-0001), and a library that does the interesting part undermines it. Second, every runtime
dependency must survive three targets — Linux, Emscripten/WebGL 2 and eventually Android — and each
one is a separate build integration, a separate licence question and a separate portability risk.
Third, the world is small (ADR-0006): most candidate libraries are sized for problems this project
does not have.

## Decision

**`cna-house` adds no third-party runtime dependency of its own.** Its entire runtime dependency
set is CNA, sharp-runtime, and whatever CNA itself pulls in.

The candidates, each with its verdict:

| Candidate | Purpose | Verdict |
|---|---|---|
| Bullet / Jolt / PhysX | Physics | **Rejected** — the world needs a swept capsule and a support plane, not a constraint solver. [ADR-0007](ADR-0007-kinematic-collision.md) names Jolt as the swap-in seam if that ever changes. |
| nlohmann/json, RapidJSON | JSON | **Rejected** — `System::Text::Json` from sharp-runtime is already in the build, is the project-idiomatic choice, and is the same parser the save system must use anyway. |
| EnTT | ECS | **Rejected** — [ADR-0006](ADR-0006-composition-over-ecs.md): the largest per-frame homogeneous population is three. |
| Dear ImGui | Debug UI | **Rejected** — `SpriteBatch`/`SpriteFont` overlays are sufficient for the counters and graphs we need, and keep the XNA-only rule clean. ImGui would also need its own renderer backend, which is exactly the native-graphics access ADR-0001 forbids. |
| Recast / Detour | Navigation | **Rejected** — the pets walk a ~300-node authored waypoint graph; A* over it is ~150 lines. Navmesh generation would be solving a harder problem than the one we have. |
| FMOD / Wwise | Audio | **Rejected** — XNA audio is the requirement, and the room-aware layer is ours by design ([ADR-0010](ADR-0010-room-aware-audio.md)). |
| stb_image, tinygltf, cgltf | Asset loading | **Rejected at runtime** — everything is compiled offline by `cna-content`; the runtime sees `.cnb`/`.xnb` only. |
| spdlog / fmt | Logging | **Rejected** — `util/Log` is ~200 lines over `std::format`, and needs project-specific behaviour (categories, rate limiting, a ring buffer for the debug overlay) that a general logger would not give for free. |
| Catch2 | Testing | **Rejected** — GoogleTest instead, matching CNA's own convention. |
| **GoogleTest** | Testing | **Accepted** — test-only, never linked into the game, matches CNA, already in the build environment. |
| **Blender, ffmpeg, `fxc`, Python, `cna-content`** | Offline tooling | **Accepted** — build-time only, never linked. |

The distinction that makes the list coherent: **build-time and test-only tools are unconstrained;
the shipped binary is not.** Offline tooling is where third-party power is welcome, because none of
it exists at runtime.

## Alternatives considered

**Allow header-only libraries.** Rejected as a category. "Header-only" is a packaging detail, not a
licence, portability or scope argument; it would admit EnTT and nlohmann/json through a loophole
that has nothing to do with why they were rejected.

**Allow a dependency behind a build flag, defaulted off.** Rejected. A path that is off by default
is a path that is never tested; it would rot, and the first person to turn it on would find it
broken.

**Vendor rather than link.** Rejected for the same reasons — vendoring changes who ships the code,
not whether the project depends on it.

## Consequences

* We write more code, and it is budgeted: collision (~1 200 lines), navigation A* (~150), the
  logger (~200), the clip player, the portal solver, the `.chanim` reader.
* The Emscripten and Android ports have exactly one integration to worry about — CNA's — which is
  the point.
* Licence accounting for the *binary* is short: Ms-PL for `cna-house`, CNA and sharp-runtime, plus
  whatever CNA's own options bring in. `NOTICE.md` states it in full.
* Reversing this decision for any single candidate requires a new ADR that supersedes the relevant
  row, with the feature that needs it named.
