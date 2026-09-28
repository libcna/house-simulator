# Project-owned subsystems (the "deviation" register)

**This file grants permission to call nothing.**

It is named after the register a project like this usually keeps — a list of approved deviations
from its own rule — and it is deliberately *not* one. `cna-house` has no approved deviations,
because the policy it implements ([ADR-0001](decisions/ADR-0001-xna-only.md), `cna-house.md` §4.3)
has no middle tier. What follows is the opposite kind of document: a record of the places where
XNA 4.0 does not provide something, and of the code **we wrote ourselves** to fill the gap.

---

## The three tiers

| Tier | Definition | Policy |
|---|---|---|
| **A** | Pure XNA 4.0 as implemented by CNA, plus `System::*` from sharp-runtime and the C++ standard library | Default. No justification needed. |
| **P** | **Project-owned** code in the `cnahouse::` namespace implementing something XNA 4.0 does not provide, written over Tier-A types and over data produced by our own offline tooling | Allowed and expected — this is *game code*, not a deviation. Every real XNA title had a great deal of it. |
| **C** | `CNA::` in any form — the CNAEXT engine layer, renderer contracts, native graphics APIs — **and** every `CNAEXT`-marked convenience call on an otherwise-XNA graphics or model type, including `Model::getSkinsEXTProperty()`, `Model::setOwnedResources()` and `GraphicsDevice::SupportsCapability()` | **Forbidden. No exceptions, no register rows, no allowlist.** |

There is no Tier between P and C. A symbol is either something XNA 4.0 gives us, something we wrote
in `cnahouse::`, or something that does not go in the runtime.

---

## `check_xna_only.py` reads no allowlist from this file

`tools/ci/check_xna_only.py` does not parse this document, does not load any exception list, and
has no per-symbol allowlist anywhere in its source or configuration. There is no file a contributor
can add a line to in order to admit a forbidden symbol.

That is the mechanical point of the whole policy: a violation cannot be *argued* into the build. It
can only be rewritten as `cnahouse::` code — which is what an XNA developer would have had to do
anyway. If you believe a forbidden symbol is genuinely necessary, the procedure is to write a new
ADR superseding ADR-0001 and to get it accepted; it is not to edit this file.

---

## Tier-P subsystems

Each row names the subsystem, what XNA 4.0 does not provide, and the section of `cna-house.md` that
owns its design.

| ID | Project-owned subsystem | What XNA 4.0 does not provide | Owning section |
|---|---|---|---|
| **OWN-01** | `cnahouse::anim::Skeleton`, `Clip`, `Keyframe`, `ClipLibrary` | XNA's `Model` has no skeleton and no clip container. Microsoft's own Skinned Model Sample solved this **in the sample**, on the game side, not in the framework. CNA ships a convenience copy of those sample classes (`CNA::Graphics::SkinningData`, `AnimationClip`, `Keyframe`); we deliberately do not use it, because it was never XNA Framework API. | §47.0 |
| **OWN-02** | `cnahouse::anim::ClipPlayer` | No blending, layering, masking or rate control anywhere in XNA. CNA's sample-derived `AnimationPlayer` plays exactly one clip and overwrites every bone on `Update()` (BL-10), which is less than the project needs in any case. | §47.3 |
| **OWN-03** | `cnahouse::visibility` — portal traversal with frustum reduction | XNA has `BoundingFrustum` and nothing above it: no cells, no portals, no scene graph, no occlusion structure. | §25 |
| **OWN-04** | `cnahouse::physics` — kinematic capsule collision | XNA 4.0 has no collision and no physics at all; it stops at `BoundingBox`, `BoundingSphere`, `Ray` and `Plane`. | §49 |
| **OWN-05** | `cnahouse::audio` — compact ambience/weather mix and in-memory muffling | No public XNA low-pass or loaded PCM getter. House decodes/filters the existing four weather loops once, then uses standard raw-PCM `SoundEffect` instances. No portal-path solver, extra assets or internal filter calls. | §64 |
| **OWN-06** | `cnahouse::render::RenderTier` | XNA 4.0 has no capability query, and CNA's `SupportsCapability` is Tier C. Tier selection is a build-time fact (`CNAHOUSE_TIER_E`) plus an ordinary guarded content load, and is one term of the project-owned effective feature set. | §7.3, §68 |
| **OWN-07** | `cnahouse::content` — the `.chanim` sidecar format and its reader | XNA's own answer was a custom content processor writing a custom type into `Model.Tag`. We cannot add a processor to CNA's pipeline without modifying CNA, and the runtime never reads `Model::Tag` (BL-01), so the same custom data travels **beside** the model as a project-owned sidecar. | §47.0 |

---

## What each row is *not*

* Not permission to call a CNA symbol. Every one of these subsystems is written in `cnahouse::`
  over XNA types and over data our own offline tooling produced.
* Not a place to add a row when a symbol is inconvenient to avoid. Adding a row here changes
  nothing about what the lint accepts.
* Not a list of XNA's shortcomings in general — only of the gaps this project had to fill. A
  limitation we route around rather than fill (no stencil, no MRT, no geometry stage) belongs in
  the blocker table, `cna-house.md` §6.

---

## Adding a Tier-P subsystem

1. Confirm XNA 4.0 genuinely does not provide it. Prove it from the CNA source, CNA documentation
   or a measured probe — not from memory.
2. Confirm the gap is not already covered by an existing row.
3. Write it in `cnahouse::`, over Tier-A types.
4. Add a row here: id (`OWN-NN`, next free, never reused), subsystem, what XNA lacks, owning
   section.
5. Add the same row to `cna-house.md` §4.3, so the architecture document and this file agree.
