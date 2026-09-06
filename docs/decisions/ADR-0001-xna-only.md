# ADR-0001 — The XNA-only rule, and the three-tier A/P/C policy

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-06 |
| **Task** | `HOUSE-00007` |
| **Owns** | `cna-house.md` §4 |
| **Supersedes** | — |

## Context

`cna-house` exists to demonstrate that a large, atmospheric, modern-feeling 3-D simulation can be
delivered through the XNA 4.0 API surface alone, as CNA implements it. That is the point of the
project, not a constraint imposed on it. If the rule is soft, the demonstration is worthless: a
reader can always suspect the interesting parts were done with something else.

CNA makes the rule easy to break by accident. It ships, in the same headers and on the same types
the game legitimately uses:

* a whole engine layer, `CNA::Graphics::*` in `modules/graphics-ext/` — HDR, a post-process stack,
  shadow helpers, a sky engine, image-based lighting, a material system, instancing/LOD/culling
  helpers, compute;
* `CNA::Graphics::ShaderEffect`, a source-based custom-shader API taking GLSL or SPIR-V;
* `CNAEXT`-marked convenience methods hanging off otherwise-XNA types — `Model::getSkinsEXTProperty()`,
  `Model::setOwnedResources()`, `GraphicsDevice::SupportsCapability()`;
* convenience copies of the classes from Microsoft's XNA Skinned Model Sample —
  `Graphics::SkinningData`, `AnimationClip`, `Keyframe`, `AnimationPlayer` — which were *sample*
  code, on the game side, never XNA Framework API.

Every one of those is a single identifier away from code that is otherwise pure XNA. A policy of
"prefer XNA, deviate when justified" would be dead within a month, because each individual
deviation reads as reasonable at the moment someone writes it.

## Decision

Everything `cna-house` compiles falls into exactly **three** classes. There is deliberately **no
middle tier**: nothing in the runtime is allowed to be "CNA-specific, but justified".

| Tier | Definition | Policy |
|---|---|---|
| **A** | Pure XNA 4.0 as implemented by CNA, plus `System::*` from sharp-runtime and the C++ standard library | Default. No justification needed. |
| **P** | **Project-owned** code in `cnahouse::` implementing something XNA 4.0 does not provide, written over Tier-A types and over data produced by our own offline tooling | Allowed and expected. This is *game code*, not a deviation. Every real XNA title had a great deal of it. |
| **C** | `CNA::` in any form — the engine layer, renderer contracts, native graphics APIs — **and** every `CNAEXT`-marked convenience call on an otherwise-XNA type | **Forbidden. No exceptions, no register rows, no allowlist.** |

Three consequences follow, and they are the whole decision:

1. **`docs/xna-deviations.md` contains zero permissive rows.** It is a *register of Tier-P
   subsystems* — for each, what XNA 4.0 lacks and what we wrote instead (OWN-01 … OWN-07). It
   grants permission to call nothing.
2. **`tools/ci/check_xna_only.py` has no symbol allowlist to consult.** There is no file a
   contributor can add a line to. A violation cannot be argued into the build; it can only be
   rewritten as `cnahouse::` code — which is what an XNA developer would have had to do anyway.
3. **`CNA_CNAEXT=OFF` is forced by our CMake.** With it off, every file in `modules/graphics-ext/`
   is compiled out and a CNA ctest (`CNAEXT_GuardDiscipline`) enforces it. The forbidden layer
   does not merely go uncalled — it is not in the binary. That is the strongest mechanical
   guarantee available, and CI re-checks it with `nm -C` (`HOUSE-00136`).

**Offline tooling is not runtime.** The rule constrains the running program. `cna-content`,
Blender, `fxc` under Wine, ffmpeg and Python are unconstrained; none of them exists at runtime, and
the runtime sees only `.cnb`/`.xnb` and `ContentManager::Load<T>()`.

**When XNA cannot do something,** the procedure is fixed: prove the limitation from CNA source,
documentation or a measured probe; record it in the blocker table (`cna-house.md` §6) with
evidence; design a Tier-A workaround, a Tier-P subsystem, a Tier-E (compiled `Effect`) workaround,
or a reduced but coherent behaviour. Never reach for a CNA symbol.

## Alternatives considered

**"Prefer XNA, allow justified deviations, record them in a register."** Rejected. A register with
permissive rows is an allowlist with better manners. Each row looks defensible in isolation; the
sum is a project that no longer demonstrates anything. It also gives the lint an exception file to
read, which is precisely the mechanism that erodes such rules in practice.

**Use `CNA::Graphics::ShaderEffect` for custom shaders.** Rejected — it is Tier C, it takes
caller-authored GLSL/SPIR-V (one source per backend), and XNA 4.0's own answer to "I want a custom
shader" is `Effect` with compiled Effect-Framework bytecode, which CNA implements. See ADR-0003.

**Use CNA's `Graphics::AnimationPlayer` and friends.** Rejected. They reproduce sample-side
classes that were never framework API, and `AnimationPlayer` plays exactly one clip with no
blending, layering or masking (BL-10) — less than the project needs anyway. We own
`cnahouse::anim::Skeleton`/`Clip`/`ClipPlayer` (OWN-01, OWN-02), exactly as a title built on that
sample would have.

**Use `GraphicsDevice::SupportsCapability()` to pick a rendering tier.** Rejected. XNA 4.0 has no
capability query, so neither do we. Tier selection is a build-time fact plus an ordinary guarded
content load (ADR-0003), and the settings UI filters itself through a project-owned effective
feature set (`cna-house.md` §68) computed from what the project already knows.

**Vendor a permissive subset of CNAEXT into `cnahouse::`.** Rejected as licence-clean but
pointless: copying the engine layer under our own namespace would satisfy the letter of the lint
while abandoning the demonstration. Tier-P code must be *ours*, written against XNA primitives.

## Consequences

* We write more code: portal visibility, kinematic collision, an animation player, a room-aware
  audio solver, a `.chanim` sidecar format and its reader. Roughly seven subsystems, each recorded
  in `docs/xna-deviations.md`. This is the expected cost and is budgeted in `plan.md`.
* Some techniques are permanently unavailable: stencil portals and stencil shadow volumes
  (BL-02), deferred shading and G-buffers (BL-03), geometry and tessellation stages. The design
  routes around each one explicitly rather than treating it as a gap to be patched later.
* Contributors need one rule, not a policy document: *if it is not `Microsoft::Xna::Framework`,
  `System::`, the standard library or `cnahouse::`, it does not go in.*
* The gate is mechanical, so the rule survives contributors who have never read this file.
