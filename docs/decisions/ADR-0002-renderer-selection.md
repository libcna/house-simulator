# ADR-0002 — Renderer selection: `OPENGLES3` (EasyGL)

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-06 |
| **Task** | `HOUSE-00008` |
| **Depends on** | [ADR-0001](ADR-0001-xna-only.md) |
| **Owns** | `cna-house.md` §7.1, §8.1 |

## Context

CNA implements XNA's `GraphicsDevice` over a choice of renderer backends, selected at CMake
configure time by `CNA_GRAPHICS_RENDERER`. The choice is made once per binary: standard XNA 4.0
exposes no way to change the backend afterwards, so it is a build decision, not a runtime one.

The candidates that could plausibly host this project on Linux are `OPENGLES3` (the EasyGL
implementation over an OpenGL ES 3.0 context), `OPENGL33` (desktop GL 3.3), `VULKAN`, `BGFX` and
`SDL_GPU`, plus `HEADLESS` for tests. The project must also stay portable to Web (`WEBGL2`) and
eventually Android (`OPENGLES3`) without a rewrite (`cna-house.md` §9), and must not lose access to
`OcclusionQuery`, on which the sun-glare model depends (§32.4).

## Decision

**`CNA_GRAPHICS_RENDERER=OPENGLES3` on Linux.** Two secondary configurations exist for development
only, never for shipping:

| Config | Renderer | Purpose |
|---|---|---|
| `desktop-gl33` | `OPENGL33` | Gives a real `GL_SAMPLES_PASSED`, so `OcclusionQuery::PixelCount` is a true fragment count — used to *validate* that the N×N grid approximation of BL-07 agrees with a real coverage measurement |
| `headless` | `HEADLESS` | CI: `Update()` and the whole simulation with no window, no GPU and no display server |

The reasons, in the order they mattered:

1. **It is the configuration 80 ported XNA samples are verified on** (`cna-samples/CMakeLists.txt:23`),
   including the shadow-mapping and skinning samples this project depends on. Every other choice
   trades verified ground for unverified ground.
2. **It is the portability baseline.** GLES 3.0 and WebGL 2 are closely related API families, and
   CNA's `WEBGL2` is the *same EasyGL implementation* over the second of them: the same renderer
   code path, the same shaders, the same feature floor. It is also the correct Android target once
   BL-13 is lifted upstream.
3. **`OcclusionQuery` is wired and pixel-verified in both directions only on EasyGL**
   (`docs/occlusionquery-support.md` support matrix).

**A baseline is not a guarantee.** Running under Linux `OPENGLES3` does not by itself establish
that the same scene runs under WebGL 2 / Emscripten, which adds Asyncify, JS exceptions, no
threads, real context loss, and browser texture-format and canvas rules (BL-14). Those are settled
by the dedicated Web phases 47–48 and their own validation, never by extrapolation.

**There is no `--renderer` option.** The binary cannot change its backend, so offering the switch
would be a lie about what the binary can do. One build per renderer. The optional `--renderer-info`
prints only what the application already knows about itself — the configured renderer name, the
`CNAHOUSE_TIER_E` build fact and the resolved render tier — and queries nothing.

## Alternatives considered

| Candidate | Verdict |
|---|---|
| `OPENGL33` | Rejected as the primary. Desktop-only: it is not the Web path and not the Android path, so shipping on it would mean discovering every GLES-3.0 restriction late, in a port. Retained as a *development* configuration for exactly one job — validating the occlusion-query grid against a real pixel count. |
| `VULKAN` | Rejected. No Emscripten wiring at all; CNA's own web/android documentation states that no renderer other than EasyGL has any Emscripten-specific wiring. Choosing it would strand the Web phase. It also carries BL-02 (stencil) and BL-08 (mip `SetData`) without compensating benefits at this project's draw-call scale. |
| `BGFX` | Rejected. Same portability objection, plus BL-08. |
| `SDL_GPU` | Rejected. Newer and less exercised in CNA than EasyGL; nothing in this project needs it. |
| `HEADLESS` as primary | Not a candidate for shipping — it renders nothing. Adopted for CI, where it is exactly right. |

## Consequences

* The feature floor is OpenGL ES 3.0: no stencil (BL-02), MRT attachment 1 unusable (BL-03), no
  geometry or tessellation stages, `OcclusionQuery::PixelCount` degraded to a boolean (BL-07).
  Every one of these is designed around explicitly rather than discovered later.
* Shader model for Tier E is `vs_3_0`/`ps_3_0` (`profile: hidef`), which MojoShader translates to
  GLSL ES 3.00. See [ADR-0003](ADR-0003-render-tiers.md).
* Mip chains are shipped pre-generated in content, because mip-level `SetData` is correct on
  EasyGL but a silent no-op elsewhere (BL-08) — recorded so that a future renderer change is a
  conscious decision rather than a silent visual regression.
* Two extra CI configurations must keep building: `headless` for the logic suite and `gl33` for the
  occlusion-count validation. Both are cheap; neither ships.
