# ADR-0003 — Two rendering tiers, and why Tier S must be complete alone

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-06 |
| **Task** | `HOUSE-00009` |
| **Depends on** | [ADR-0001](ADR-0001-xna-only.md), [ADR-0002](ADR-0002-renderer-selection.md) |
| **Owns** | `cna-house.md` §7.2, §7.3, §7.4 |

## Context

XNA 4.0 offers two ways to shade a triangle: the five stock effects (`BasicEffect`,
`DualTextureEffect`, `SkinnedEffect`, `AlphaTestEffect`, `EnvironmentMapEffect`), and `Effect` —
compiled Direct3D 9 Effect-Framework bytecode loaded through `ContentManager`.

Custom effects are not free here. CNA embeds no HLSL compiler, so `.fx` must be compiled by an
external `fxc.exe` from the DirectX SDK (June 2010) run through Wine, and CNA's own documentation
records that this route is *"not verified against a genuine Microsoft `fxc`"* (BL-04). Compiled
effects are also a CNA build option (`CNA_EASYGL_COMPILED_EFFECTS`), so a given CNA build may not
support them at all — and `SpriteBatch::Begin(effect)` throws on a renderer without them (BL-15).

The project must therefore be shippable whether or not custom effects work, without maintaining
two games.

## Decision

Two tiers, **both complete**, differing only in shading:

**Tier S — stock effects only.** The guaranteed baseline and the project's default configuration.
It may use `BasicEffect`, `DualTextureEffect`, `SkinnedEffect`, `AlphaTestEffect`,
`EnvironmentMapEffect`, `SpriteBatch` and `SpriteFont`, and nothing else. Tier S is a **complete,
shippable game**: the house is lit (baked lightmaps + additive daylight + per-object directional
lights), the sky is a vertex-coloured dome with scrolling cloud layers, shadows are blob shadows,
and weather is particles plus material swaps. Every feature in `cna-house.md` is specified against
Tier S first.

**Tier E — compiled XNA `Effect`s.** Purely additive: eight `.fx` files (`RoomLit`, `ShadowDepth`,
`SkinLit`, `SkyDome`, `Precip`, `SurfaceBlend`, `PostComposite`, `WaterFlow`) targeting
`vs_3_0`/`ps_3_0`. Every Tier-E effect has a **named Tier-S fallback** and a test that renders the
same scene both ways: the visual difference may be large, but the *scene content* must be
identical.

**How Tier E is selected — without asking CNA anything at runtime:**

1. **Build time, the primary mechanism.** `cna-house`'s CMake reads the CNA configuration it has
   itself just set — the selected `CNA_GRAPHICS_RENDERER` and that renderer's compiled-effect
   option — and sets `CNAHOUSE_TIER_E` from it. When it is off, the Tier-E sources and the `.fx`
   content tree are not compiled at all and the binary contains only Tier S.
2. **Load time, the safety net.** When Tier E is compiled in, `LoadContent` loads the effect set
   inside one `try`/`catch`. A `ContentLoadException` or `NotSupportedException` — a missing
   `.xnb`, a driver that will not accept the bytecode — selects Tier S, logs once, and the game
   continues. That is ordinary XNA content loading, not a capability query, and it also covers the
   content failure a capability query would have missed.
3. **User choice.** `--tier=s` and the graphics settings force Tier S at any time. There is no
   switch that forces Tier E into a build that does not have it.

The compiled `Effects/*.xnb` are **committed** next to their `.fx` sources, precisely so a
contributor without Wine can still build a Tier-E binary (BL-04).

## Alternatives considered

**One tier, custom effects required.** Rejected. It makes the whole project hostage to BL-04 and to
a Wine-hosted 2010 compiler, and it would leave `cna-house` unable to run on any CNA build without
compiled effects — including, plausibly, the Web and Android builds.

**One tier, stock effects only.** Tempting, and it would still be a complete game. Rejected because
shadow mapping, single-pass room lighting and a real sky are the difference between "convincing"
and "a demo", and because demonstrating that XNA's `Effect` path works end to end through CNA is
part of the project's point.

**Runtime capability query to choose the tier.** Rejected — Tier C and forbidden (ADR-0001). It is
also the *wrong* mechanism: a capability query answers "can this device compile effects", when the
question that actually matters is "did my eight effects load". The guarded load answers the real
question and needs no CNA extension.

**Tier E as a runtime-switchable quality setting.** Rejected. Effects that are not compiled into
the binary cannot be switched on; a setting that pretends otherwise is a lie. The setting offers
only what the build actually contains, filtered through the project-owned effective feature set
(`cna-house.md` §68).

**`CNA::Graphics::ShaderEffect` (GLSL/SPIR-V source).** Rejected — Tier C (ADR-0001), and one
shader source per backend instead of one authored `.fx` for all of them.

## Consequences

* Every visual feature is designed twice, and the Tier-S design is the one that must be coherent
  on its own. This is a real cost, paid deliberately.
* The render-regression suite carries a "Tier S vs Tier E" family: 10 poses rendered both ways,
  asserting content identity rather than pixel identity (`cna-house.md` §70.4).
* Tier E cannot be blamed for a broken Tier S. If a Tier-S path rots, the tests catch it, because
  Tier S is the default configuration and the one CI runs first.
* `SpriteBatch::Begin(effect)` is compiled only into a Tier-E build and reached only after that
  build's effect set has loaded (BL-15), so the throw is unreachable by construction.
