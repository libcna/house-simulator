# CNA capability report

`STATUS: COMPLETE for phase 1 — 49 of 50 probe tasks measured; `HOUSE-00100` is recorded as
inconclusive and stays open. Last updated 2026-09-06.`

**What this file is.** `cna-house.md` §5 audits CNA by reading its source, its documentation and
its sample ports. That is an audit of *claims*. This file records what CNA was **measured** to do,
by running code against the actual checkout. Phase 1 exists because a design that rests on an
unread assumption is a design that fails late.

**The rule this file is written under.** A row is only ever moved off `PENDING` by a probe that
was actually run and whose output was actually read. Source expectation and measured result are
recorded separately and are never conflated. Where they disagree, the measurement wins and the
planning documents are corrected — never the other way round.

---

## Configuration under test

| Item | Value |
|---|---|
| CNA checkout | `/rv/data/development/github.com/openeggbert/cnanext` |
| CNA revision | `d42203805057d43dc092bb713613bcc945ad8d5a`, branch `next`, committed 2026-09-06 15:07:20 +0200 |
| CNA working tree | **dirty** — documentation and integration notes modified, no `modules/` source change; see [Checkout hygiene](#checkout-hygiene) |
| sharp-runtime checkout | `/rv/data/development/github.com/openeggbert/sharp-runtimenext` |
| sharp-runtime revision | `30ccdef30ba4d27534864b0777729b9e78ee8a21`, branch `next` |
| `CNA_GRAPHICS_RENDERER` | `OPENGLES3` (EasyGL) |
| `CNA_PLATFORM` / `CNA_AUDIO_PLATFORM` | `SDL3` / `SDL3` |
| `CNA_CNAEXT` | **`OFF`** — the non-negotiable rule of ADR-0001 |
| `CNA_EASYGL_COMPILED_EFFECTS` | `ON` |
| `CNA_ENABLE_VIDEO` | `AUTO` (FFmpeg backend `cna_video_ffmpeg` selected at configure time) |
| `CNA_ENABLE_NET` | `OFF` |
| `CMAKE_BUILD_TYPE` | `Release` |
| Compiler | g++ (Debian 14.2.0-19) 14.2.0, C++23 |
| Host | Debian 13, Linux 6.12.107, x86_64, 16 threads, 30 GB RAM, no swap |
| GPU / driver | AMD Radeon 780M (radeonsi, phoenix), Mesa 25.0.7 |
| Probe build directory | `build-probe/`, in this repository, `p1-` file-name prefix per probe |
| Second build directory | `build-consumer/`, configured **from the same source directory** with a different renderer or video setting — see `HOUSE-00117` |
| `fxc` | Microsoft Direct3D Shader Compiler **9.29.952.3111** (DXSDK June 2010), under Wine, through `tools/effects/fxc-wine.sh` |

**This is one configuration.** Every verdict below is a verdict about *this* renderer on *this*
driver. `HOUSE-00115` records which rows must be re-run when the renderer changes.

---

## Verdict vocabulary

| Verdict | Meaning |
|---|---|
| `PASS` | Measured, and it behaves as `cna-house.md` §5 claims. |
| `FAIL` | Measured, and it does not work. A blocker row is opened or updated. |
| `DIFFERENT` | Measured, it works, but not the way §5 describes. The planning documents are corrected. |
| `PENDING` | Not yet measured. **Not** a synonym for "probably fine". |
| `NOT PROBED` | Deliberately out of scope for phase 1, with the reason stated. |

---

## §5.1 Core framework

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| C-01 | `Game`, `GraphicsDeviceManager`, `GameTime`, `GameComponent` are available | `modules/runtime/` | **`PASS`** | `HOUSE-00062` | A `Game` subclass with a `GraphicsDeviceManager` ran 300 frames at 1600×900 and exited cleanly. |
| C-02 | `Game::Run()` is a blocking lifetime on desktop and on Emscripten | `docs/emscripten-mainloop-game-lifetime.md` | **`PASS`** (desktop half) | `HOUSE-00062` | `Run()` blocked until `Exit()`, then returned; `main` printed its summary and exited 0. The Emscripten half is phase 48. |
| C-03 | `ContentManager::Load<T>()`, `RootDirectory`, `Unload()` exist, and resolution order is `.xnb` first, then a literal path, then `.cnj`/`.cnb` | `docs/xnb-content-pipeline-support.md` §Scope | **`PASS`** | `HOUSE-00064` | Same content name as both `.xnb` (red) and `.cnb` (blue): the red texels came back — **`.xnb` wins**. The literal-path middle tier was not exercised. |
| C-04 | `System::*` from sharp-runtime — `Text.Json`, `IO`, `Xml.Serialization`, `IO.IsolatedStorage` — are available as CMake components | `sharp-runtimenext/modules/text-json/CMakeLists.txt` | **`PASS`** (`Text.Json`, `IO`) | `HOUSE-00076`, `HOUSE-00103` | Both used to read a sidecar from disk. **`Text.Json` is not in CNA's default component set** and CNA does not link it, so a consumer must add it to `SHARP_RUNTIME_COMPONENTS` *and* link `SharpRuntime::Text.Json` itself. Sizes `HOUSE-00121`/`HOUSE-00122`. |
| C-05 | `StorageDevice`/`StorageContainer` work; Linux root is `$XDG_DATA_HOME/<app>` else `~/.local/share/<app>` | `modules/storage/src/StorageDevice.cpp:88-109` | **`DIFFERENT`** | `HOUSE-00102` | Write, read, list and delete all work. But `<app>` is **the literal string `game`**, not the title: the only setter is `SetAppNameEXT`, which ADR-0001 forbids. Measured root: `~/.local/share/game/P1Probe`. The container name is ours, so a distinctive container is the XNA-only fix. |

## §5.2 Math and volumes

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| M-01 | The XNA math surface is complete: `Vector2/3/4`, `Matrix`, `Quaternion`, `Plane`, `Ray`, `BoundingBox`, `BoundingSphere`, `BoundingFrustum`, `ContainmentType`, `Curve`, `MathHelper`, `Point`, `Rectangle`, `Color` | `modules/math/include/Microsoft/Xna/Framework/` | **`PASS`** (the phase-9 subset) | `HOUSE-00104` | Every type the room/portal system uses was constructed and exercised. `Curve` was not probed. **`Color` is not trivially copyable** — it cannot go in a vertex struct passed to `SetData<T>`. |
| M-02 | `BoundingFrustum::GetCorners`, `BoundingBox::CreateFromPoints`, `Ray::Intersects` and `BoundingFrustum::Intersects(BoundingBox)` agree with analytic answers | SAMPLE-038 | **`PASS`** | `HOUSE-00104` | 24/24 against hand-computed answers, in an orthographic frame chosen so every expectation is exact. Frustum corners `x[-2,2] y[-1,1] z[-11,-1]`; ray distances 4.0, 3.0, 3.0 and 0.0 exactly. |

## §5.3 Graphics

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| G-01 | `BasicEffect` with 3 directional lights, specular, fog and per-pixel lighting | `docs/basiceffect-support.md` | **`PASS`** | `HOUSE-00082` | All six terms at once agree with the analytic model **to the byte**: `(92,90,86)` measured, `(92,90,86)` computed. Per-vertex and per-pixel agree on a constant-normal surface. |
| G-02 | `SkinnedEffect`, `MaxBones = 72`, `WeightsPerVertex` 1/2/4 | `docs/skinnedeffect-support.md` | **`PASS`** | `HOUSE-00075`, `HOUSE-00077` | `MaxBones == 72`; 72 accepted, 73 throws `boneTransforms exceeds MaxBones.`. Visible deformation confirmed by pixel comparison, not by absence of an exception. **`LightingEnabled = false` is refused** — as XNA 4.0 refuses it. |
| G-03 | `DualTextureEffect` (albedo × lightmap, two UV channels) | `docs/dualtextureeffect-support.md` | **`PASS`** | `HOUSE-00078` | 64/64 texels within **1/255** of the analytic product, with the two channels carrying *different* coordinates so a TEXCOORD0-for-both implementation would fail. The FNA `*2` doubling factor is present (128×128 → 128). |
| G-04 | `AlphaTestEffect` | `docs/alphatesteffect-support.md` | **`PASS`** | `HOUSE-00080` | The cutoff is exact to one alpha value for all six comparison functions, measured on a 256-column alpha ramp. Two-sided foliage works under `CullNone`; a back-facing card is correctly invisible under the single-sided state. |
| G-05 | `EnvironmentMapEffect` including the Fresnel term, `TextureCube` sampling | `docs/environmentmapeffect-support.md` | **`PASS`** | `HOUSE-00081` | The sampled cube face equals `reflect(-E, N)` at three tilts, checked against six distinctly coloured faces. `EnvironmentMapAmount` blends linearly (0 → 0, 0.5 → half, 1 → the environment). Fresnel raises grazing reflectivity for factors 1 and 4. `AmbientLightColor` **does** reach this effect. |
| G-06 | `Effect` from compiled Effect-Framework bytecode, behind `CNA_EASYGL_COMPILED_EFFECTS=ON` (MojoShader) | `docs/fx-compiled-effects.md` §10 | **`PASS`** | `HOUSE-00087` | `.fx` → `fxc` under Wine → `.xnb` → MojoShader → a draw, end to end. Requires the project-side path-translating launcher `tools/effects/fxc-wine.sh`; the bare `--fx-compiler-launcher wine` fails. |
| G-07 | A compiled `Effect` works end-to-end in a real scene: two techniques switched by name, `Single` 2048² render target with `Depth24`, that target rebound as an effect texture | `cna-samples/plan.md:785` | **`PASS`** | `HOUSE-00083`, `HOUSE-00088` | All three halves measured: techniques `Tint`/`Textured` switched by name and back within one frame with numerically different results; the 2048² `Single`+`Depth24` target rendered, read back bit-exactly and sampled. |
| G-08 | `Model`/`ModelMesh`/`ModelMeshPart`/`ModelBone` and `CopyAbsoluteBoneTransformsTo` | `docs/model-content-pipeline-support.md` | **`PASS`** | `HOUSE-00072` | Depth-3 hierarchy with a sibling branch: every local and absolute transform equals the matrix computed offline, to 2e-5. `Copy*BoneTransformsTo` require a **pre-sized** destination and throw `destinationBoneTransforms` otherwise. |
| G-09 | `Model` from compiled content carries a real bone hierarchy via `.cnb` | `docs/xnb-content-pipeline-support.md` | **`PASS`** | `HOUSE-00072` | 4 authored nodes → 5 bones: CNA inserts a **synthetic `Root`** above the scene root. Parent links, `Index`, `Children` and mesh `ParentBone` all as authored. |
| G-10 | A skinned glTF compiles to `.cnb` and its joints are recoverable **without reading `Model::Tag`** | `docs/content-pipeline.md:456-458` | **`PASS`** | `HOUSE-00074`, `HOUSE-00076` | Recoverable by **name lookup into `Model::Bones`**, which is all a sidecar carries. Blend indices are **skin-local**, not bone indices, so the sidecar *must* carry the joint-name list — acceptance path (4), not (2). `Model::Tag` and `getSkinsEXTProperty()` are never read. |
| G-11 | `VertexBuffer`, `IndexBuffer`, `DynamicVertexBuffer`; EasyGL has a real 32-bit index factory | feature matrix | **`PASS`** | `HOUSE-00092`, `HOUSE-00094` | 70 000-vertex buffer with 32-bit indices draws the triangle addressed by 17-bit indices (106 261 lit px against an analytic ~106 000). 2 000 dynamic quads/frame with `Discard` cost 0.171 ms to GPU completion. |
| G-12 | `RenderTarget2D`, `RenderTargetCube`, mip chains, MSAA on EasyGL | feature matrix | **`PASS`** (`RenderTarget2D`) | `HOUSE-00083`, `HOUSE-00084` | 2048² `Single`+`Depth24` and 64² `Color` with a 7-level mip chain, at MSAA 0 and 4, all create, render, unbind and sample. `RenderTargetCube` not probed. |
| G-13 | `BlendState`, `DepthStencilState` compare functions, `RasterizerState`, 16 per-slot `SamplerState`s | feature matrix | **`PASS`** (the Tier S subset) | `HOUSE-00079`, `HOUSE-00080` | `BlendState::Opaque`/`Additive`, `CompareFunction::Equal` depth with writes off, all three `CullMode`s and per-slot `PointClamp` on slots 0 and 1 all behave. Additive sums are **exact** and a depth-equal second pass reaches every pixel of the first. |
| G-14 | `SpriteBatch` (all overloads, sort modes, custom `Effect`) and `SpriteFont` | feature matrix | **`PASS`** (SpriteFont + `Begin`/`DrawString`/`End`) | `HOUSE-00066`, `HOUSE-00089` | Text drawn into a `RenderTarget2D` and read back: 440 lit pixels, ink inside the `MeasureString` box. Sort modes and custom-`Effect` overloads remain for `HOUSE-00089`. |
| G-15 | `OcclusionQuery` exists; `PixelCount` is a real count **only** where the driver exposes `GL_SAMPLES_PASSED`, which the ES 3.2 profile does not — so it degrades to 0/1 (`BL-07`) | `docs/occlusionquery-support.md` | **`PASS`** (the claim is correct) | `HOUSE-00090` | A quad covering an analytic **16 384** pixels reported `PixelCount == 1`; fully occluded reported `0`. It is a boolean here, exactly as §5 says. |
| G-16 | `DrawInstancedPrimitives` is present in the API | `GraphicsDevice.hpp:424` | **`PASS`, and it is worth using** | `HOUSE-00093` | 200 instances in one instanced draw: **0.156 ms** against **2.144 ms** for 200 separate draws — **13.7× faster**. |
| G-19 | Multiple render targets: a compiled two-output effect reaches attachment 1 | `HOUSE-00086` scope note | **`PASS`** | `HOUSE-00086`, `HOUSE-00087` | A `COLOR0`/`COLOR1` technique wrote `(255,128,64)` to attachment 0 and its own distinct `(32,223,96)` to attachment 1. `BL-03` does not hold for compiled effects. |
| G-17 | `Texture2D::SetData`/`GetData`/`FromStream`/`SaveAsPng`, NPOT sizes | feature matrix | **`PASS`** (`GetData`, `SetData`) | `HOUSE-00065`, `HOUSE-00078`, `HOUSE-00110` | 4×4 `Color` texture, 16/16 texels byte-exact — **against the premultiplied model**; see the finding. `SetData`/`FromStream`/`SaveAsPng` and NPOT are not yet probed. |
| G-18 | WebGL context-loss handling is implemented and browser-qualified | `docs/web-emscripten-graphics-limitations.md` | `NOT PROBED` | — | Deferred to the Web phases (47–48). `HOUSE-00108` could **not** use `DebugSimulateContextLoss` — it is a `CNA::Internal` call behind a `CNA/` include and ADR-0001 forbids it twice over — so the XNA-legal `GraphicsDevice::Reset()` path was probed instead. |

## §5.4 Audio

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| A-01 | `SoundEffect`, `SoundEffectInstance`, looping, volume/pitch/pan on the SDL3 mixer | `modules/audio/` | **`PASS`** (load, `CreateInstance`, volume, `Play`, `Stop`) | `HOUSE-00067` | Real device opened at 44 100 Hz stereo; duration exact, state `Playing` after `Play()`. Looping and pitch/pan are not yet probed. |
| A-02 | `AudioListener`, `AudioEmitter`, `SoundEffectInstance::Apply3D` | `SoundEffectInstance.hpp:362,383` | **`PASS`** | `HOUSE-00095` | All three types construct and `Apply3D` runs. It writes **none of its result to the public `Volume`/`Pan`/`Pitch`** — the spatial state is private and reaches only the mixer track. |
| A-03 | The 3D model is **simplified**: pan, distance attenuation and Doppler only — no speaker matrix, cones, curves, LFE, HRTF or reverb sends | `cna_audio_deep_audit_2026-07-17.md:161` | **`PASS`** (the claim is correct) | `HOUSE-00095` | Confirmed from the implementation and by behaviour: **full volume inside `DistanceScale`, then inverse *distance*** (1 m → 1.000, 2 m → 0.500, 20 m → 0.050), pan is the listener-relative rightward displacement over distance clamped to [-1,1]. No HRTF, no cone, no curve. |
| A-04 | Doppler is present and carries an up-to-4× pitch-inflation risk on wrong velocity units (`BL-11`) | same doc, A-09 | **`PASS`** (the guarantee holds) | `HOUSE-00096` | At `emitter.DopplerScale × SoundEffect::DopplerScale == 0` the factor is set to exactly `1.0f` **without evaluating the Doppler math**, so no rounding path to a pitch change exists. The velocity-units risk is real but is ours to avoid, not CNA's to fix. |
| A-05 | Accepted wave formats are PCM8, PCM16, IEEE float, MS-ADPCM, IMA-ADPCM — **24-bit PCM is not listed** (`BL-06`) | `docs/xnb-content-pipeline-support.md` audio matrix | **`DIFFERENT`** | `HOUSE-00067`, `HOUSE-00068` | **24-bit PCM is not rejected.** The pipeline converts it to 16-bit with a warning, bit-identically to ffmpeg; `SoundEffect::FromStream` accepts raw 24-bit too. `BL-06` corrected. |
| A-06 | `MediaPlayer`/`Song` exist; `Song` is an external stream reference | same doc | `NOT PROBED` | — | `cna-house` uses `SoundEffect` throughout (§62); the TV audio track decision is `HOUSE-01361`+. |

## §5.5 Media

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| V-01 | `Video`/`VideoPlayer` with `GetTexture()`, `Play`, `Pause`, `Stop`, `IsLooped`, `Volume`, `State` | `VideoPlayer.hpp:70-148` | **`PASS`** (`GetTexture`, `Play`, `Stop`, `State`, `PlayPosition`) | `HOUSE-00098` | Sixteen `GetTexture` readbacks across a 2 s clip classified as `R R R G G G G B B B B B W W W W` — the authored sequence, at the authored timestamps. `Pause`, `IsLooped` and `Volume` were not exercised. |
| V-02 | Decoding is the optional `cna_video_ffmpeg` backend; Linux native works with FFmpeg dev packages present | `docs/video-backend.md` | **`PASS`** | `HOUSE-00098` | Configure printed `CNA: video enabled (AUTO; FFmpeg backend cna_video_ffmpeg)`, and a clip then decoded and advanced frames at runtime with a live 44.1 kHz audio device. |
| V-03 | With the backend absent, `Play()` throws `System::NotSupportedException` (`BL-05`) | `docs/video-backend.md` | **`PASS`** | `HOUSE-00099` | Built with `CNA_ENABLE_VIDEO=OFF`: `Play()` throws *"Video playback is unavailable because CNA was built without the optional FFmpeg video backend…"*. **`Load<Video>` still succeeds** — only playback is gated. |

## §5.6 Input

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| I-01 | `Keyboard`, `Mouse`, `GamePad*` and the whole `Touch/` namespace are present | `modules/input/include/…` | **`PASS`** | `HOUSE-00101` | `Keyboard`, `Mouse`, all four `GamePad` slots and `Touch::TouchPanel` all queried without throwing. `GetCapabilities` and `GetState` agree on connection for every gamepad slot; desktop `TouchPanel` reports `IsConnected=false` and 0 touches. |
| I-02 | `Mouse::SetPosition(int,int)` is available, which is all strict-XNA first-person mouse-look needs | `Mouse.hpp:49` | **`DIFFERENT`** | `HOUSE-00100` | `SetPosition` exists and is reflected by a `GetState` in the **same frame**. But `GetState` returns an **event-driven snapshot**, not a live cursor query: with no motion event over the window it never advances, and `Game::IsActive` was `true` throughout while it did not. Mouse-look needs more than `SetPosition`. |
| I-03 | CNA's `CNAEXT` relative-mouse mode exists and is **not used** | `Mouse.hpp:67,74` | **`PASS`** (not used) | `HOUSE-00063` | The probe calls it nowhere and the source lint forbids it. Note it is **not** unlinkable: CNAEXT members of core XNA classes are always compiled — see the `HOUSE-00063` finding. |

## §5.7 Content pipeline (`cna-content`)

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| P-01 | `.gltf`/`.glb` → `CNA.GltfImporter/2` → `CNA.ModelContentWriter/3` → `Model` | `docs/content-pipeline.md:370-382` | **`PASS`** | `HOUSE-00070`, `HOUSE-00071` | `CNA.GltfImporter -> CNA.ModelProcessor -> CNA.ModelContentWriter`. Positions, normals and UVs bit-exact against the authored `.glb`; indices unreversed; bounds exact. **`CullClockwise` is the correct draw state.** The emitted vertex is **48 bytes, not `VertexPositionNormalTexture`** — see the finding. |
| P-02 | `.png`/`.jpg`/`.dds` → `Texture2D` | same | **`PASS`** (`.png`) | `HOUSE-00065` | `CNA.ImageImporter -> CNA.TextureProcessor -> CNA.Texture2DContentWriter`. `premultiplyAlpha` defaults true. `.jpg`/`.dds` not probed. |
| P-03 | `.wav` → `SoundEffect` | same | **`PASS`** | `HOUSE-00067`, `HOUSE-00068`, `HOUSE-00069` | `CNA.WavImporter -> CNA.SoundEffectProcessor -> CNA.SoundEffectContentWriter`, for 16-bit and (by conversion) 24-bit sources. |
| P-04 | `.spritefont` (+ TTF via FreeType) → `SpriteFont` | same | **`PASS`** | `HOUSE-00066` | `CNA.FontDescriptionImporter -> CNA.FontDescriptionProcessor -> CNA.SpriteFontContentWriter`; FreeType 2.13.3. `<FontName>` resolves a file beside the descriptor before any system font. |
| P-05 | `.fx` → `Effect`, **`--format xnb` only** | same | **`PASS`** | `HOUSE-00087` | `CNA.EffectSourceImporter -> CNA.EffectSourceProcessor -> CNA.XnbEffectWriter`, `--format xnb`, through a genuine Microsoft `fxc` 9.29.952.3111 under Wine. |
| P-06 | `.fxb` (already-compiled bytecode) → `Effect` | same | `NOT PROBED` | `HOUSE-00087` | The `.fx` source route was probed end to end; a pre-compiled `.fxb` was not, because `cna-house` has no source of one and the `.fx` route is what it will use. |
| P-07 | video → `CnbVideoData` → `Video` (deployed, not decoded, at build time) | same | **`PASS`** | `HOUSE-00098` | `CNA.VideoImporter -> CNA.VideoProcessor -> CNA.VideoContentWriter`, one `.cnb` plus the deployed `.mp4`. Metadata (64×64, 10 fps, 2.000 s) matches the authored clip exactly. |
| P-08 | `cna_add_content(TARGET … SOURCE_DIR … OUTPUT_DIR … CONFIG_FILE … WORKERS …)` is the CMake integration | `cmake/ToolContentPipeline.cmake:48` | `NOT PROBED` | `HOUSE-00126` | **Deliberately deferred to phase 2**, which is what consumes it. Phase 1 drove `cna-content` directly, which measured the *pipeline*; the CMake wrapper is build integration and is tested by being used. |
| P-09 | `.fx` compilation is driven through an external compiler with `--fx-compiler <fxc> --fx-compiler-launcher wine` | `docs/content-pipeline.md:409-424` | **`DIFFERENT`** | `HOUSE-00087` | The mechanism works, but **`--fx-compiler-launcher wine` alone does not**: `cna-content` passes Unix absolute paths and `fxc` reads a leading `/` as an option. `tools/effects/fxc-wine.sh` translates them and is the launcher to use. |

## §5.8 Proven by sample

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| S-01 | 80 of 157 ported XNA samples are complete, each with a native `OPENGLES3` build, and they cover every subsystem `cna-house` needs | `cna-samples/plan.md:192` | `NOT PROBED` | — | Sample *ports* are evidence about CNA, not about `cna-house`. Phase 1 re-proves the capabilities directly rather than inheriting them. `HOUSE-00118` runs CNA's own ctest subset as a checkout-health check. |

---

## The XNA-only rule, measured

The rule of ADR-0001 is a property of the built artifact, not only of the source. Both halves are
recorded here.

| # | Property | Verdict | Probe | Measured |
|---|---|---|---|---|
| X-01 | No runtime source references a forbidden symbol | `PASS` | `tools/ci/check_xna_only.py` | Clean at every commit; the gate's own `--selftest` detects all 14 planted-violation classes. |
| X-02 | `CNA_CNAEXT=OFF` genuinely removes the engine layer — zero `CNA::Graphics::` symbols in the linked binary | **`PASS`** | `HOUSE-00063` | 0 in `p1-hello`, against 6 277 in the `CNAEXT=ON` control; the archive shrinks 37.4 MB → 69 kB. **Caveat:** other forbidden identifiers live in `Microsoft::Xna::Framework::Graphics` and remain linked — the source lint is their only gate. |

---

## Blockers this phase settles

| Blocker | Question | Settled by | Status |
|---|---|---|---|
| `BL-06` | Is 24-bit PCM really rejected, and with what exact error? | `HOUSE-00068` | **SETTLED — the premise was false.** It is not rejected by either path; the pipeline converts it bit-identically to ffmpeg. `cna-house.md` §6 updated. |
| `BL-07` | Is `OcclusionQuery::PixelCount` a count or a boolean here? | `HOUSE-00090` | **SETTLED — it is a BOOLEAN.** A quad of analytic area 16 384 reported `1`. The N×N grid approximation is required, and a coverage ratio computed from this count would be `1/area`, not a fraction. |
| `BL-09` / `Q-01` | Can a `SurfaceFormat::Single` 2048² `RenderTarget2D` be created, rendered to and read back? | `HOUSE-00083` | **SETTLED — YES, all four stages.** Created with `Depth24`, rendered into, read back with `GetData(float*)` **bit-exactly** (0.625 in, 0.625 out over 3 396 649 texels), and bound as an effect texture and sampled (159/255). **Tier E uses a real float shadow map**; the RGBA8 packing fallback is not needed. |
| `BL-02` | Is `ClearOptions::Stencil` ignored and `ReferenceStencil` inert? | `HOUSE-00085` | **SETTLED — the premise is FALSE. Stencilling works.** A two-pass mask stamped `ReferenceStencil = 1` over the left half and then drew the full quad under `CompareFunction::Equal`: left half **2048/2048** lit, right half **0/2048**. `cna-house.md` §6 `BL-02` is corrected. |
| `BL-03` | Does EasyGL MRT attachment 1 stay black? | `HOUSE-00086`, `HOUSE-00087` | **SETTLED — the premise is FALSE for compiled effects.** With a *stock* effect, which declares one output, attachment 1 stays `(0,0,0)` — the renderer does not broadcast. With a **compiled two-output technique**, attachment 1 receives its own `COLOR1` exactly: `(32,223,96)` for an authored `(0.125, 0.875, 0.375)`. MRT works; `cna-house.md` §6 `BL-03` is corrected. |
| `BL-05` | Does `VideoPlayer::Play()` throw `NotSupportedException` without the backend? | `HOUSE-00099` | **SETTLED — yes, and the rest still links.** Built with `CNA_ENABLE_VIDEO=OFF`: `Play()` throws *"Video playback is unavailable because CNA was built without the optional FFmpeg video backend…"*, naming the fix. `Load<Video>` still **succeeds** — only playback is gated — and the player stays a usable object afterwards. |
| `BL-11` | Does `DopplerScale = 0` with zero velocities produce no pitch change? | `HOUSE-00096` | **SETTLED — yes, and unconditionally.** When `emitter.DopplerScale × SoundEffect::DopplerScale` is zero the factor is set to exactly `1.0f` **without evaluating the Doppler math at all**, so there is no rounding path by which a pitch change could appear. Zero velocities alone would also give 1.0. |
| `BL-12` | Is `Model::Meshes[i].BoundingSphere` populated from `.cnb`? | `HOUSE-00073` | **SETTLED — yes, and it is conservative rather than minimal.** Non-degenerate and it contains every vertex, but on the test box its radius is 7.686 against a minimal 6.225 (**+23 %**) and its centre is 1.55 off in Y. Usable for a cheap reject; not usable as a tight bound. |

---

## Checkout hygiene

The `cnanext` working tree carries local modifications at the time of probing. They must be
recorded, because a measurement against an unrecorded tree is not reproducible.

`cna-house` **does not modify CNA or sharp-runtime** (`CLAUDE.md` §3). The modifications below
were present before this session and were not made by it; they are listed so a later session can
tell whether a differing result comes from a different tree.

Measured 2026-09-06, `git status --porcelain` in `cnanext`: **11 modified files** — `.gitignore`
plus ten Markdown files (`NEXT.md`, `NEXT_gltf.md`, three `integration/BATCH_*_STABILIZATION.md`,
two `integration/lanes/*.md`, two `modularization/**/*.md` and `plans/plan_binding.md`). An earlier
revision of this paragraph called all eleven Markdown; `.gitignore` is not, and the count and the
file list are otherwise unchanged.

**Re-checked at the end of the session: still exactly those 11 files, still zero under `modules/`,
and `sharp-runtimenext` is completely clean.** Neither sibling was modified by this work.

**Zero files under `modules/` are modified.** Every measurement below is therefore against
pristine CNA source at `d422038`; only planning prose differs from the committed tree.

`sharp-runtimenext` at `30ccdef` was not modified by this session either.

---

## Findings

### `HOUSE-00062` — minimal `Game`, `OPENGLES3`, `CNA_CNAEXT=OFF` · **PASS**

Probe `build-probe/p1-hello.cpp`: a `Game` subclass in strictly XNA-shaped API — no `CNA/`
include, no `CNA::` reference, no native GL call. It sets the back buffer to 1600×900, clears to
`CornflowerBlue`, counts frames in `Draw`, and calls `Exit()` at 300.

```
$ cmake -S build-probe -B build-probe -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache
$ cmake --build build-probe --target p1-hello -j16
$ ./build-probe/p1-hello
p1-hello: Initialize
p1-hello: frames=300 backbuffer=1600x900 expected=1600x900
p1-hello: PASS                                            # exit code 0
```

Runtime log:

```
[INFO][RENDER] EasyGLRenderer initialized with OpenGL OpenGL ES 3.2 Mesa 25.0.7-2+deb13u1
[INFO][RENDER] CNA: graphics renderer: OPENGLES3
```

* The window opened at exactly the planned 1600×900; `PresentationParameters` reported the
  requested size back, so the request was honoured rather than silently clamped.
* 300 frames drawn, clean `Exit()`, process exit code 0.
* **No GL error and no validation warning attributable to the probe.** One unrelated line appears
  on stderr — `Gtk-WARNING: gtk_disable_setlocale() must be called before gtk_init()` — emitted by
  the GTK file-dialog portal SDL3 initialises on this desktop. It is not produced by CNA or by the
  probe and does not affect the result.

**Entrypoint finding.** The samples include `CNA/Platform/Entrypoint.hpp` in the translation unit
that defines `main()`. That header is a **`CNA/` include and therefore forbidden in `cna-house`
runtime code.** Reading it settles the question: under `CNA_PLATFORM_SDL3` it expands to nothing
unless the target is Android or iOS, where SDL's entry-point rename is load-bearing. This probe
omitted it entirely and linked, ran and exited correctly. So a plain `int main()` is sufficient on
Linux, and the Android entry point becomes a phase-49 problem to solve in the build system rather
than a reason to put a `CNA/` include in `src/app/`. Recorded for `HOUSE-00127`.

**Capability line captured for later probes** (verbatim, from EasyGL at startup — it is *reported*
configuration, not a measurement, and does not close any row):

```
MSAA up to 8x; MRT up to 4 targets (GL draw buffers=8, color attachments=8, CNA/FNA cap=4);
indexed color masks: supported; anisotropic filtering: supported (Task 918, up to 16x);
texture SurfaceFormat: Color + NormalizedByte4 (RGBA8_SNORM) + NormalizedByte2 (RG8_SNORM)
  + Bgr565 (RGB565) + Bgra5551 (RGB5_A1) + Bgra4444 (RGBA4) + Dxt1/Dxt3/Dxt5 (S3TC blocks);
render-target SurfaceFormat: Color + half-float (RGBA16F) + float (RGBA32F)
```

Two things in that line matter later and are **not** settled here. The render-target list names
`RGBA32F`, not a single-channel `R32F` — `HOUSE-00083` must still create an actual
`SurfaceFormat::Single` target before `BL-09` can be closed. And `Dxt1/Dxt3/Dxt5` appear as
*texture* formats, which `HOUSE-00111` must confirm survive the pipeline still compressed.

---

### `HOUSE-00063` — does `CNA_CNAEXT=OFF` remove the engine layer? · **PASS, with a material caveat**

The stated criterion is met exactly:

```
$ nm -C build-probe/p1-hello | grep -c 'CNA::Graphics::'
0
```

**A zero is only evidence if the check could have found something.** The positive control is the
CNAEXT-enabled build already present in CNA's own tree (inspected read-only, not modified):

```
$ nm -C ../cnanext/build/modules/graphics-ext/libcna_graphics_ext.a | grep -c 'CNA::Graphics::'
6277
```

| Archive | `CNA_CNAEXT` | Size | `CNA::Graphics::` symbols |
|---|---|---|---|
| `../cnanext/build/.../libcna_graphics_ext.a` | `ON` | 37 445 582 B | **6 277** |
| `build-probe/CNA_BUILD/.../libcna_graphics_ext.a` | `OFF` | 68 996 B | **0** |

So `nm -C` demonstrably detects these symbols when they exist, the option genuinely collapses the
engine layer by a factor of 543 in size, and none of it reaches the linked binary. `X-02` is
`PASS`.

#### The caveat: `CNA_CNAEXT=OFF` does **not** remove every forbidden symbol

The forbidden list in `CLAUDE.md` §1 is wider than `CNA::Graphics::`. Running the whole list
against the linked probe binary:

| Forbidden identifier | Occurrences in `p1-hello` |
|---|---|
| `CNA::Graphics::` | **0** |
| `AvatarRenderer` | **0** |
| `ShaderEffect` | 98 |
| `PbrEffect` | 389 |
| `SkinnedPbrEffect` | 199 |
| `SkinnedModelEXT` | 50 |
| `GraphicsCapability` | 5 |
| `SupportsCapability` | 3 |
| `getSkinsEXTProperty` | 1 |
| `setOwnedResources` | 1 |

These are **not** in the `CNA::` engine namespace. They live inside `Microsoft::Xna::Framework::Graphics`,
in the always-compiled `modules/graphics` core:

```
Microsoft::Xna::Framework::Graphics::GraphicsDevice::SupportsCapability(CNA::GraphicsCapability) const
Microsoft::Xna::Framework::Graphics::GraphicsDevice::BuildRendererCapabilityProfileEXT() const
Microsoft::Xna::Framework::Graphics::Model::getSkinsEXTProperty() const
Microsoft::Xna::Framework::Graphics::Model::setOwnedResources(std::shared_ptr<void>)
Microsoft::Xna::Framework::Graphics::SkinnedModelEXT::AddPartEXT(...)
Microsoft::Xna::Framework::Graphics::PbrEffect / SkinnedPbrEffect / ShaderEffect
```

CNA states the design reason itself, in `AreaLightEXT.hpp`: *"an always-compiled XNA header must
not include one that exists only under `CNA_CNAEXT`, and an effect's public surface must not
depend on a build flag."* That is a defensible decision for CNA. Its consequence for `cna-house`
is precise and must not be misread:

> **`CNA_CNAEXT=OFF` removes the `CNA::Graphics::` engine layer. It does not make the other
> forbidden calls unlinkable. For those, `tools/ci/check_xna_only.py` is not a second line of
> defence — it is the only one.**

This does not weaken ADR-0001; it identifies where the rule is actually enforced. Two consequences:

* `HOUSE-00136` (the CI symbol check) can meaningfully assert `CNA::Graphics:: == 0` and
  `AvatarRenderer == 0`. It **cannot** assert the rest, because CNA's own core defines them. The
  task should be scoped to the two that are real, and say why the others are excluded.
* The source lint's coverage of `SupportsCapability`, `*EXT*`, `PbrEffect`, `ShaderEffect`,
  `SkinnedModelEXT` and `setOwnedResources` is load-bearing and must never be relaxed.

#### A compiler-level enforcement mechanism exists — and is not yet usable

`modules/core/include/CNA/CNAHelper.hpp` defines the `CNAEXT` marker macro:

```cpp
#ifdef CNA_STRICT_XNA_API
#define CNAEXT [[deprecated("CNAEXT: not part of the XNA 4.0 API surface")]]
#else
#define CNAEXT
#endif
```

With `-DCNA_STRICT_XNA_API -Werror=deprecated-declarations`, calling a CNAEXT-tagged member becomes
a **compile error**. That is exactly what ADR-0001 wants, enforced by the compiler rather than by a
Python lint, and it is a build flag rather than a runtime query, so adopting it would not breach
the XNA-only rule. It was tested in both directions.

Negative control — `build-probe/p1-strict-negative.cpp` calls `Model::getSkinsEXTProperty()`:

```
without the flag                                  -> compiles (exit 0)
with -DCNA_STRICT_XNA_API -Werror=deprecated-...  -> error: '...Model::getSkinsEXTProperty() const'
                                                     is deprecated: CNAEXT: not part of the
                                                     XNA 4.0 API surface           (exit 1)
```

**It works. It is also, today, unusable**, because it fires on genuine XNA 4.0 API. Compiling the
*clean* `p1-hello.cpp` — which calls nothing forbidden — under the same flag produces 8 distinct
errors:

```
Microsoft::Xna::Framework::Graphics::IVertexType::~IVertexType()
Microsoft::Xna::Framework::Graphics::PackedVector::IPackedVector::~IPackedVector()
Microsoft::Xna::Framework::Input::Touch::TouchPanel::MAX_TOUCHES
Microsoft::Xna::Framework::Media::VisualizationData::Size
Microsoft::Xna::Framework::Content::LooseFileContentTypeReader<T>
ContentManifestEntry · ContentManifestReaderUsage · ContentTypeReaderBase
```

`IVertexType`, `TouchPanel` and the content-reader base are real XNA 4.0 types; their destructors
and members carry the marker, so merely including `Game.hpp` trips the flag.

**Verdict: a promising second line of defence, blocked upstream on CNAEXT tagging precision.**
`cna-house` does not modify CNA (`CLAUDE.md` §3), so this is recorded, not patched. If CNA ever
tightens the tagging, `CNAHOUSE_STRICT_XNA` becomes a cheap and very strong addition to
`HOUSE-00124`. Until then the Python lint stands alone.

---

### `HOUSE-00064` — `ContentManager` resolution order · **PASS**

Two *different* payloads compiled to the same content name, so the answer is read off the pixels
rather than inferred from a file listing: a solid **red** 2×2 PNG to `.xnb`, a solid **blue** one
to `.cnb`.

```
$ cna-content build build-probe/p1-red.png  -o .../P1Resolution.xnb --format xnb
$ cna-content build build-probe/p1-blue.png -o .../P1Resolution.cnb --format cnb
$ ls .../P1Resolution.*        # both present: .cnb 384 B, .xnb 203 B
```

```
[HOUSE-00064] same name, .xnb vs .cnb   PASS  first texel R=255 G=0 B=0 -> .xnb WON
```

**`.xnb` wins over `.cnb`.** Row `C-03` confirmed as `cna-house.md` §5.1 claims. `cna-house`
compiles to `.cnb`, so the practical rule is: **a stale `.xnb` of the same name silently shadows
the `.cnb` the build just produced.** `cna_add_content` wiring (`HOUSE-00126`) must not leave both
in the output tree, and `HOUSE-00114`-style cleanup applies to content directories too.

The middle tier of the claim — a *literal path* between `.xnb` and `.cnb` — was not exercised;
this probe only ordered the two compiled containers.

---

### `HOUSE-00065` — `Texture2D` from `.cnb` · **PASS, with a correction that matters**

Source: a purpose-built 4×4 PNG in which **every texel is distinct** in all four channels, so a
round trip that reordered, flipped or dropped a channel could not pass by luck.

```
[HOUSE-00065] dimensions                  PASS 4x4 (expected 4x4)
[HOUSE-00065] surface format              PASS Color (expected Color)
[HOUSE-00065] alpha model: straight 1/16 exact, premultiplied 16/16 exact
[HOUSE-00065] GetData round trip          PASS 16/16 exact against the premultiplied model
```

The first run of this probe **failed**, reporting 1/16 texels exact. The one that matched was the
only fully opaque texel. The measured value at (1,0) was `66,13,35,223` where the source PNG holds
`75,15,40,223` — and `75 × 223/255 = 65.6`, `15 × 223/255 = 13.1`, `40 × 223/255 = 35.0`.

**The pipeline premultiplies alpha.** The probe was then changed to evaluate *both* models and
report which the data fits, rather than to assume either: straight alpha matches 1/16, premultiplied
matches **16/16 exactly**. CNA documents this (`docs/content-pipeline.md`): `TextureProcessor`'s
`premultiplyAlpha` parameter **defaults to `true`, exactly as XNA 4.0's `TextureProcessor.PremultiplyAlpha`
does**, because `BlendState::AlphaBlend` — what `SpriteBatch::Begin()` selects when given no blend
state — is the premultiplied blend.

The defect was in the probe's expectation, not in CNA. The finding is still worth its own row,
because three later systems would each have produced a subtly wrong result from the naive
assumption:

* any texture round-trip or golden-image test must compare against premultiplied values;
* the `DualTextureEffect` lightmap product of `HOUSE-00078` must not premultiply a second time;
* content authored as straight alpha and drawn with `BlendState::NonPremultiplied` will show dark
  fringes. `cna-house` uses the default, i.e. premultiplied, everywhere.

Recorded against `HOUSE-00078`, `HOUSE-00107` and the content-authoring conventions.

---

### `HOUSE-00066` — `SpriteFont` through the `.spritefont` route · **PASS**

```
$ cna-content build build-probe/p1-content-src -o build-probe/p1-content
[BUILD] Fonts/P1Font -> .../Fonts/P1Font.cnb (529488 bytes;
        CNA.FontDescriptionImporter -> CNA.FontDescriptionProcessor -> CNA.SpriteFontContentWriter)
```

```
[HOUSE-00066] loaded, line spacing sane          PASS lineSpacing=50
[HOUSE-00066] MeasureString scales with length   PASS "I"=13.000000x51.000000 "I"x10=130.000000x51.000000
[HOUSE-00066] glyphs rendered (ink present)      PASS 440 lit pixels of 16384
[HOUSE-00066] ink inside measured box            PASS ink bbox x[14,49] y[15,47] draw origin (10,8)
                                                      measured 44.000000x51.000000
```

Placement was **measured, not assumed**: the string was drawn into an offscreen `RenderTarget2D`,
read back with `GetData`, and the ink's bounding box compared against `MeasureString` and the draw
origin. Ink starts after the origin `(10,8)` and stays inside the measured box — glyphs are placed
where the advance widths say they are, and `MeasureString` is a usable layout oracle for the HUD.

`MeasureString("I")` = 13 px advance and `×10` = 130 px is exactly linear, as expected for a
kerning-free repeated glyph at `<Spacing>0</Spacing>`.

**Licensing.** The descriptor names `P1TestFont.ttf`, a copy of the host's
`/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf`, placed beside the `.spritefont` inside
`build-probe/` — which is `.gitignore`d and is deleted with the probes. **No font was added to the
repository**, and no redistribution claim is made or needed. The shipping font is chosen under the
phase-4 licensing tasks.

A pipeline note worth keeping: CNA resolves `<FontName>` to a **file beside the descriptor** first
and only then to an installed system font, warning when it falls back because that would make the
build depend on the machine. `cna-house` must always ship the `.ttf` next to the `.spritefont`.

---

### `HOUSE-00067` — 16-bit PCM `SoundEffect` · **PASS**

Fixture: `ffmpeg -f lavfi -i sine=frequency=440:duration=1.0:sample_rate=44100 -c:a pcm_s16le -ac 1`.

```
[BUILD] Audio/P1Tone16 -> .../Audio/P1Tone16.cnb (88504 bytes;
        CNA.WavImporter -> CNA.SoundEffectProcessor -> CNA.SoundEffectContentWriter)

[HOUSE-00067] duration                        PASS 1.000000 s (expected ~1.000000 s)
[HOUSE-00067] state is Playing after Play()   PASS state=0
```

The SDL3 mixer initialised on this host — `[AudioMixer] Requested format=0x0 channels=2 freq=44100;
application format=0x8010 channels=2 freq=44100` — so playback was exercised against a real device,
not a null sink. Duration is exact to the microsecond and `SoundEffectInstance::getStateProperty()`
reports `Playing` immediately after `Play()`.

---

### `HOUSE-00068` — the 24-bit PCM question · **`BL-06` IS WRONG**

`BL-06` predicted that 24-bit PCM is rejected, and this task's acceptance criterion was to record
the exact exception so the pipeline could assert against it. **There is no exception. Both paths
accept 24-bit PCM.**

**Path 1 — the content pipeline.** It converts, and says so:

```
$ cna-content build build-probe/p1-tone24.wav -o .../P1Tone24.cnb --format cnb
[BUILD] p1-tone24 -> .../P1Tone24.cnb (96304 bytes;
        CNA.WavImporter -> CNA.SoundEffectProcessor -> CNA.SoundEffectContentWriter)
  warning (CNA.SoundEffectProcessor): the source is 24-bit PCM and was converted to 16-bit PCM
  with round-to-nearest and saturation; this discards precision the source carried.
Built: 1  Skipped: 0  Failed: 0                          # exit code 0
```

**Path 2 — the runtime**, bypassing the pipeline entirely with `SoundEffect::FromStream` on the
untouched 24-bit file:

```
[HOUSE-00068] raw 24-bit FromStream ACCEPTED, duration 1.0000 s
[HOUSE-00068] raw 24-bit via FromStream   PASS  ACCEPTED (no exception), duration 1.000000 s
```

**Is the conversion correct, or merely silent?** The same 48 kHz signal was authored as 16-bit and
compiled the same way, and the two `.cnb` files compared byte for byte:

```
$ cmp -l .../P1Tone24.cnb build-probe/p1-tone24-as16.cnb | wc -l
29                       # of 96 304 bytes
$ cmp -i 400 .../P1Tone24.cnb build-probe/p1-tone24-as16.cnb
                         # no output: identical from byte 400 to EOF
```

All 29 differing bytes are in the header — the asset-name string (`p1-tone24` vs
`p1-tone24-as16`), its length fields and the build fingerprint. **The PCM payload is byte-identical.**
CNA's 24→16 conversion equals `ffmpeg -c:a pcm_s16le` sample for sample.

`BL-06` must be corrected. Its severity was already `L`; it is now closer to a pipeline note than a
blocker. The offline conversion step it mandates is **optional**, not required — see `HOUSE-00069`
for the one part of it that does matter.

---

### `HOUSE-00069` — NOX conversion · **PASS, and it corrects the `BL-06` command**

One representative file, selected without inventorying the collection:

| | |
|---|---|
| Source | `/rv/tmp/Essentials_Series_NOX_SOUND/Electromagnetic_NOX_SOUND/Electromagnetic_Car_Dashboard_Loop_Mono_Elektrousi_01.wav` |
| Format | `pcm_s24le`, 48 000 Hz, mono, 7.964396 s |
| SHA-256 | `55522170764eec9f6463d6056023aaf08ca08d63534ed50bf2dabe970c0ac5ec` |

```
[HOUSE-00069] duration  PASS 7.964399 s (16-bit @ 44.1 kHz, expected ~7.9644)
[HOUSE-00069] duration  PASS 7.964396 s (16-bit @ 48 kHz,   expected ~7.9644)
[HOUSE-00069] state is Playing after Play()   PASS   (both)
```

Both convert, compile, load, play, and preserve duration — the 48 kHz variant to the microsecond.

**The correction.** `cna-house.md` `BL-06` gives the command as
`ffmpeg -i in.wav -c:a pcm_s16le -ar 44100 out.wav`. Measuring the two halves separately shows the
resample, not the bit depth, is what damages the signal:

| Variant | Peak dB | RMS dB | Δ RMS vs source |
|---|---|---|---|
| Source (24-bit, 48 kHz) | −15.209455 | −32.406843 | — |
| 24→16 bit, **48 kHz kept** | −15.209542 | −32.405142 | **0.0017 dB** |
| 24→16 bit **and `-ar 44100`** | −15.469952 | −33.296462 | **0.889 dB** |

Bit-depth reduction is effectively lossless. `-ar 44100` costs 0.89 dB RMS and 0.26 dB peak on this
material, because resampling 48 → 44.1 kHz lowpasses content this high-frequency source actually
carries.

**Recommendation: drop `-ar 44100`.** The whole NOX collection is 48 kHz, CNA loaded and played the
48 kHz asset correctly, and the mixer opened at 44 100 Hz and resamples at playback anyway — so the
offline resample buys nothing and costs signal. `BL-06`'s command becomes
`ffmpeg -i in.wav -c:a pcm_s16le out.wav`.

Given `HOUSE-00068`, even that is optional: the pipeline already converts 24-bit sources itself,
bit-identically. The reason to keep an explicit offline step is **provenance, not format** — the
asset manifest of ADR-0012 wants both hashes recorded, and an explicit step is where that happens.

**No redistribution claim is made here.** This probe used one file to measure a format conversion.
The collection's licence status remains exactly what `cna-house.md` §63 says it is, and is settled
only by its dedicated phase-4 licensing task.

### `HOUSE-00070` / `HOUSE-00071` — static glTF → `.cnb` → `Model`, and the winding verdict · **PASS**

Fixture: `p1-make-gltf.py` writes `P1Static.glb`, an axis-aligned box whose six plane coordinates
are all distinct (`min(-1,-2,-3) max(4,5,6)`), with per-vertex-unique asymmetric UVs. Self-authored,
so every number the probe compares against is derivable from the generator rather than from another
CNA call. `p1-static` loads the compiled `.cnb`, reads the vertex and index buffers back with the
plain XNA `GetData` overloads, and compares element by element.

```
$ cna-content build build-probe/p1-fixtures/P1Static.glb \
      -o build-probe/p1-content/P1Static.cnb --format cnb
[BUILD] P1Static -> ... (2440 bytes; CNA.GltfImporter -> CNA.ModelProcessor -> CNA.ModelContentWriter)
$ ./build-probe/p1-static
  [ok] positions pass through bit-exact (no axis remap or flip)  0 of 24 differ
  [ok] normals pass through unchanged                            0 differ
  [ok] UVs pass through bit-exact (no V flip)                    0 differ
  [ok] indices are the source order, unreversed                  0 differ
  [ok] vertex-buffer bounds equal the authored bounds            min(-1,-2,-3) max(4,5,6)
  [ok] screen-space extents match the analytic projection (+Y is up)
       measured x[88,167] y[72,183] vs analytic x[88.0,168.0] y[72.0,184.0]
p1-static: 21/21 checks passed
```

`docs/gltf-conventions.md`'s claim of *no axis remap, no handedness negation and no V flip* is
therefore confirmed by measurement and not only by reading the importer.

**Winding (`HOUSE-00071`), measured numerically:**

| Cull mode | Pixels covered |
|---|---|
| `CullNone` | 8 960 |
| `CullClockwise` | **8 960** |
| `CullCounterClockwise` | **0** |

**`CullClockwise` is the correct state for glTF-authored geometry**, exactly as §5 claims.

This measurement needed a *second* fixture, and the reason is worth recording because it is an easy
mistake to repeat: **a closed solid cannot measure winding.** Run against the box, all three modes
covered an identical 8 960 pixels — with front faces culled you simply see the far faces through the
near ones, and the silhouette is unchanged. `P1Quad.glb`, an open single-sided quad, is what makes
the wrong cull mode render literally nothing.

**Finding — the vertex layout is not a built-in XNA vertex type.** `CNA.ModelProcessor` emits a
**48-byte** vertex for a primitive carrying `POSITION`/`NORMAL`/`TEXCOORD_0`:

| Offset | Usage | Format |
|---|---|---|
| 0 | `Position` | `Vector3` |
| 12 | `Normal` | `Vector3` |
| 24 | **`Tangent`** | `Vector4` |
| 40 | `TextureCoordinate` (index 0) | `Vector2` |

`VertexPositionNormalTexture` is 40 bytes, and the source `.glb` authored **no** `TANGENT`
attribute — the processor synthesises one. A first version of this probe read the buffer as
`VertexPositionNormalTexture` and got vertex 0 right and every later vertex wrong, because the
stride mismatch walked it off the data. Consequences: any `cna-house` code that reads model
geometry back (collision meshes, offline bake verification, the phase-44 asset tests) must declare
its own 48-byte struct and check it against `VertexDeclaration::GetVertexElements()` at load, never
name a built-in type.

### `HOUSE-00072` — bone hierarchy, `ParentBone`, `Root`, `CopyAbsoluteBoneTransformsTo` · **PASS**

Fixture `P1Hier.glb`: four nodes, depth three plus a sibling branch, a 90° rotation about Z that
does not commute with its translation, a non-uniform scale `(2, 0.5, 4)`, and one node authored as
an explicit glTF `matrix` rather than TRS — the only place CNA's importer converts a layout at all.
The generator computes every expected local and absolute matrix **in XNA's own convention**
(row-major storage, row-vector transform, `local = S·R·T`, `absolute = local · absolute(parent)`)
and emits them as C++ literals, so the probe compares CNA against arithmetic, not against CNA.

`p1-hier` reports 22/22, including a check that distinguishes a *transposed* result from a merely
wrong one — the failure mode that a transform-convention mistake actually produces.

```
  [--] bone count   5              [--] mesh count   4
  [--] bone[0] Root    index=0 parent=<none>  children=1
  [--] bone[1] P1Base  index=1 parent=Root    children=2  [1 0 0 0 | 0 1 0 0 | 0 0 1 0 | 10 0 0 1]
  [--] bone[2] P1Mid   index=2 parent=P1Base  children=1
  [--] bone[3] P1Tip   index=3 parent=P1Mid   children=0  [2 0 0 0 | 0 0.5 0 0 | 0 0 4 0 | 0 0 3 1]
  [--] bone[4] P1Wing  index=4 parent=P1Base  children=0
  [ok] P1Tip: local Transform            # authored as an explicit glTF matrix
  [ok] P1Tip: absolute transform
  [ok] an undersized destination is rejected, not silently grown   threw: destinationBoneTransforms
p1-hier: 22/22 checks passed
```

Four facts fall out of it:

* **CNA inserts a synthetic `Root` bone** above the glTF scene root. Bone count is *nodes + 1*, and
  `Model::Root` is that synthetic bone, not the first authored node. Any `cna-house` bone-index
  table must be built by **name lookup**, never by assuming node *i* is bone *i*.
* `Bones[i]->Index == i` holds, and every bone reaches `Root` by following `Parent`.
* The explicit-`matrix` node round-trips correctly, so `ConvertGltfMatrix` does what
  `docs/gltf-conventions.md` says.
* `CopyAbsoluteBoneTransformsTo` / `CopyBoneTransformsTo` **require a destination already sized to
  `Bones.Count`** and throw `std::invalid_argument("destinationBoneTransforms")` otherwise. They do
  not grow the vector. This is XNA 4.0's array contract preserved literally in C++.

### `HOUSE-00073` — `ModelMesh::BoundingSphere` from `.cnb` · **PASS, with a caveat that matters**

Populated and non-degenerate; it contains every vertex. It is **not minimal**:

| | Measured | Minimal for this box |
|---|---|---|
| Centre | `(1.726, -0.049, 2.114)` | `(1.5, 1.5, 1.5)` |
| Radius | `7.686` | `6.225` |

23 % over on radius, and the centre is 1.55 off in Y — the signature of an incremental
(Ritter-style) construction rather than a minimal enclosing sphere. `BL-12` is settled *positively*:
no offline bounds computation is needed for correctness. But the phase-9 and phase-41 visibility
work must treat it as a **cheap conservative reject only**; anywhere a tight bound is wanted,
`cna-house` computes its own from the vertex data it already reads back.

### Three CNA-versus-XNA shape differences these probes exposed

None of them is a defect; all three change how `cna-house` must be written, and none is visible
from reading `cna-house.md` §5.

1. **`ContentManager::Load<T>` returns `T` by value.** `Model` is a value type in CNA where XNA
   4.0's is a reference type. `Model* m = content.Load<Model>(...)` does not compile.
2. **Collection iterators are `CNAEXT`.** `ModelMeshCollection`, `ModelBoneCollection`,
   `ModelMeshPartCollection` and `EffectPassCollection` all mark `begin()`/`end()` `CNAEXT`, so a
   range-`for` over them is forbidden under ADR-0001 even though `foreach` over the same collection
   is ordinary XNA 4.0 in C#. **Every loop over an XNA collection in `cna-house` is an index loop**
   over `getCountProperty()`. This is pervasive, cheap to comply with, and expensive to discover
   late; `HOUSE-00136` should gate it.
3. **`Matrix::Identity` is `Matrix::getIdentityProperty()`** and `Vector3::Up` is a plain static —
   the property-name mapping is not uniform, so it is read per type rather than guessed.


### `HOUSE-00074` — the skinned binding, and the one fact that fixes the `.chanim` format · **PASS**

Fixture `P1Skin.glb`: a two-column, five-row ribbon bound to a three-joint chain, with rows 1 and 3
authored as exact 50/50 blends so a *dropped* weight is distinguishable from a *swapped* one. The
probe matches each compiled vertex to its authored counterpart **by position**, never by array
index, and then tests the blend index against both hypotheses at once.

```
  [--] bone count  6                    # five authored nodes + the synthetic Root
  [--] vertex declaration  stride=68 Position@0:Vector3 Normal@12:Vector3 Tangent@24:Vector4
                           TexCoord@40:Vector2 BlendWeight@48:Vector4 BlendIndices@64:Byte4
  [ok] blend weights equal the authored weights                       10/10
  [--] blend-index meaning  indexes Model::Bones on 0/10 vertices;
                            indexes the skin joint list on 10/10
  [--] name -> bone-index map a sidecar would carry   P1J0->2 P1J1->3 P1J2->4
p1-skin: 20/20 checks passed
```

**The verdict: blend indices are SKIN-LOCAL.** They are `0..N-1` in the order the glTF `skin.joints`
array declares, **not** indices into `Model::Bones`. On this fixture the two differ by two, because
of the synthetic `Root` and the skin root node.

This is acceptance path **(4)** of `HOUSE-00074`, not path (2), and it settles `R-16` and the
`.chanim` format of `HOUSE-00166`:

* the sidecar **must** carry the skin's joint names **in blend-index order**; that list *is* the
  binding, and nothing in the compiled `Model` reproduces it;
* at load the runtime resolves each name to a `Model::Bones` index (`bones[name]` — the collection
  has a by-name indexer, so this needs no search of our own);
* the skinning palette handed to `SkinnedEffect::SetBoneTransforms` is indexed by **skin-local**
  joint index, so it is built in the sidecar's order, not the bone order.

Two supporting measurements:

* **The compiled `.cnb` is byte-identical across rebuilds** — two builds of the same source both
  hashed `dc702b15…f06379` — so the joint order is stable, which is criterion (2)'s real content.
* The skinned vertex is **68 bytes**: the 48-byte static layout plus `BlendWeight` (`Vector4`, @48)
  and `BlendIndices` (`Byte4`, @64). `Byte4` caps a skin at 256 joints at the vertex level, well
  above `SkinnedEffect`'s own 72.

### `HOUSE-00075` / `HOUSE-00077` — project-owned animation and the bone cap · **PASS**

`p1-skinanim` implements the evaluator `cnahouse::anim` will ship: per-joint TRS keyframe tracks,
`Lerp`/`Slerp` sampling, composition in XNA's order, a parent walk to absolute matrices, and a
palette built with **inverse bind matrices the project owns** — taken from the source asset offline,
never from a CNA query.

It is checked analytically *before* anything is drawn: the bind pose must skin to the identity for
every joint (or the inverse bind matrices are wrong), and the evaluated bend must carry the tip from
`(0,4,0)` to exactly `(-2,2,0)`. Both hold.

Deformation is then measured, not assumed:

| | Covered pixels | Ink bounding box |
|---|---|---|
| Bind pose | 1 785 | `x[118,138] y[86,170]` |
| Bent 90° about +Z at the middle joint | 1 569 | `x[86,143] y[118,170]` |

1 800 pixels differ against a bind-pose coverage of 1 785; the silhouette extends 32 px further
**left** and starts 32 px **lower**, which is what a +90° turn about `+Z` at the middle joint
predicts and what a merely-scaled or merely-translated result would not produce.

`HOUSE-00077`: `SkinnedEffect::MaxBones == 72`; `SetBoneTransforms` accepts exactly 72 and throws
`boneTransforms exceeds MaxBones.` at 73. The cap is real and enforced.

**Finding — `SkinnedEffect` refuses `LightingEnabled = false`**, throwing *"SkinnedEffect does not
support setting LightingEnabled to false."*, exactly as XNA 4.0's does. There is no flat unlit
skinned draw. Anything `cna-house` wants to draw unlit and skinned must use a full ambient term
(`AmbientLightColor = (1,1,1)` with the directional lights off), which is what the probe does.

### `HOUSE-00076` — one skin per runtime asset · **PASS, with the task's premise corrected**

The task says to confirm *"the split parts render identically to the unsplit source"*. The first run
established that **there is no loadable unsplit source**: `CNA.ModelProcessor` refuses a multi-skin
glTF outright.

```
$ cna-content build build-probe/p1-fixtures/P1TwoSkin.glb -o .../P1TwoSkin.cnb --format cnb
  Process (CNA.ModelProcessor): glTF produced 2 Model documents; set ModelProcessor bool
  parameter 'generateChildAssets' to true to publish the deterministic multi-Model output set.
Built: 0  Skipped: 0  Failed: 1
```

That is *better* than the architecture assumed — a multi-skin source cannot silently reach the
runtime, so the rule enforces itself at build time — but it means the comparison had to be
reformulated. What the probe measures instead is that the **two independent split routes agree
pixel for pixel**, driven from one pose of one shared skeleton evaluated once:

| Route | How | Assets produced |
|---|---|---|
| **Pipeline** | `"generateChildAssets": {"type":"bool","value":true}` in the asset config | `P1TwoSkin.cnb` (primary — the lexicographically first group) and `P1TwoSkin_P1SkinB.cnb` (child), both ordinary `Load<Model>()` names |
| **Offline** | `p1-split-skins.py`, the project-owned splitter | `P1PartA.glb` / `P1PartB.glb`, each single-skin, plus a `.attach.json` record |

```
  [--] offline-split route coverage   2576 px (1287 left, 1289 right)
  [ok] both parts actually drew       1287 / 1289
  [ok] the pipeline split and the offline split are pixel-identical   0 differing pixels
p1-skinsplit: 13/13 checks passed
```

**The pipeline route becomes the default** (`cna-house.md` §21.3 updated): one line of asset config
replaces a project-owned tool, and the child names are ordinary logical content names. The offline
splitter stays as the fallback for sources whose generated child names are unacceptable, and it is
what `HOUSE-00224` implements.

The **attachment record** is what makes the parts reassemble, and it round-trips through
`System::Text::Json` in the probe exactly as the runtime will read it: the shared skeleton root, the
part's attachment bone, and the skin's joint names in blend-index order.

**Finding — `Text.Json` is not in CNA's default sharp-runtime component set.** Including
`System/Text/Json/JsonDocument.hpp` does not compile until the consumer both adds `Text.Json` to
`SHARP_RUNTIME_COMPONENTS` *and* links `SharpRuntime::Text.Json` directly, because CNA does not link
it and so does not propagate its include directories. `cna-house`'s world data, save files and
sidecars are all `System::Text::Json`, so phase 2's CMake (`HOUSE-00121`, `HOUSE-00122`) must carry
both lines.

**Finding — a second `ContentManager` needs the `Game`'s service provider.** `ContentManager(nullptr)`
throws *"ContentManager: no GraphicsDevice is available from the service provider."* at the first
`Load<Model>`; `ContentManager(&game.getServicesProperty())` works. Relevant to `HOUSE-00858`'s
per-pack managers.


### `HOUSE-00078` — `DualTextureEffect` · **PASS**

The albedo is an 8×8 checker of 240/80 and the lightmap an 8×8 gradient `16x + 2y`, both opaque, both
built with `SetData` so no premultiplication policy enters the arithmetic. Every texel is compared
against the formula FNA defines and CNA implements — `color = tex0; color.rgb *= 2; color *= tex1`.

**64/64 texels within 1/255. Worst absolute delta: 1.**

The fixture makes the *second-channel* half of the claim falsifiable: channel 1 carries a
**mirrored** X coordinate, so an implementation that fed `TEXCOORD0` to both samplers would produce
a symmetric image, and the checker guarantees most texels would then be wrong. None were.

The probe also keeps a permanent witness for the `*2` doubling factor — the bug CNA's own Task 383
found and fixed — because it is invisible to any test using saturated 0/1 values: 128 × 128 must
read back as 128, not 64. It reads back as **128**.

**Consequence for `cna-house`:** a lightmap authored at 0.5 grey means "no change", not "half
brightness". The bake in phase 12 targets that midpoint.

### `HOUSE-00079` — multi-pass additive lighting · **PASS**

This is the Tier S lighting mechanism, so both of its assumptions were measured. The fixture is a
quad **tilted in depth**, deliberately: a screen-parallel quad has constant interpolated depth and
would pass a depth-equal test even on hardware whose passes disagree.

| | Result |
|---|---|
| First pass (`Opaque`, depth write) | 3 249 px at exactly 60 |
| Second pass (`Additive`, `CompareFunction::Equal`, no depth write) | 3 249 px at exactly **100** |
| Pixels rejected by the depth-equal test | **0** |
| Three passes, 60 + 40 + 40 | 3 249 px at exactly **140** |

Additive accumulation is exact and depth-equal invariance holds. Tier S's three-light room is sound.

**Finding — a re-bound render target needs `RenderTargetUsage::PreserveContents`.** The default is
`DiscardContents`, so the natural "draw pass 1, unbind, read back, rebind, draw pass 2" sequence
loses the first pass. The probe uses the explicit usage; phase 16 must too.

### `HOUSE-00080` — `AlphaTestEffect` · **PASS**

Measured on a 256×1 alpha ramp — one texel per alpha value — so the cutoff column *is* the
threshold, read off rather than estimated.

| `AlphaFunction` (reference 128) | First surviving alpha | Count |
|---|---|---|
| `Greater` | 129 | 127 |
| `GreaterEqual` | 128 | 128 |
| `Less` | 0 | 128 |
| `Equal` | 128 | 1 |
| `Always` | 0 | 256 |
| `Never` | — | 0 |

Every one is exactly XNA's semantics, to a single alpha value. Two-sided rendering: a back-facing
card is invisible under `CullClockwise`, appears under `CullCounterClockwise`, and `CullNone` draws
it from both sides with identical coverage — the foliage state works.

**Finding — procedurally authored geometry does not inherit the glTF winding convention.** The
first version of this probe wound its quad top-left → top-right → bottom-right, which in normalised
device coordinates (where **+Y is up**) is *clockwise*, and the entire quad vanished under
`CullClockwise`. Every check failed for one fixture reason. `cna-house` generates geometry
procedurally in phases 6, 10, 25 and 27; each generator must be wound counter-clockwise to match
what the imported assets use, and each needs a coverage assertion of its own.

### `HOUSE-00081` — `EnvironmentMapEffect` · **PASS**

The `TextureCube` carries six distinctly coloured faces, so "which face was sampled" is one pixel
read, compared against `reflect(-E, N)` computed in C++.

| Quad orientation | `reflect(-E, N)` | Expected face | Measured |
|---|---|---|---|
| head-on | `(0, 0, 1)` | `+Z` magenta | `(200,10,200)` ✓ |
| +45° about `+Y` | `(1, 0, 0)` | `+X` red | `(200,10,10)` ✓ |
| −45° about `+Y` | `(-1, 0, 0)` | `-X` green | `(10,200,10)` ✓ |

`EnvironmentMapAmount` is a linear blend weight: 0 → `(0,0,0)`, 0.5 → `(100,5,100)`, 1 →
`(200,10,200)`. Fresnel behaves as its definition requires — at factors 1 and 4 the grazing angle is
markedly more reflective than head-on, and at factor 0 (`pow(x,0) == 1`) the weighting is uniform.

`AmbientLightColor` **does** reach this effect (0.5 grey ambient → `(128,128,128)`), even though it
appears in no uniform of EasyGL's environment-map fragment shader — it is folded into the emissive
term before upload. Recorded because the shader source alone would suggest otherwise.

### `HOUSE-00082` — `BasicEffect`, everything at once · **PASS, byte-exact**

Tier S *is* `BasicEffect`, so the probe implements the lighting model in C++ and asserts the pixel,
adding one contribution at a time so a disagreement would localise to a single term.

| Configuration | Measured | Analytic | Δ |
|---|---|---|---|
| ambient + emissive | `(25,25,25)` | `(26,26,26)` | 1 |
| + three directional diffuse terms | `(107,107,87)` | `(107,107,87)` | **0** |
| + three specular terms | `(158,129,95)` | `(158,129,95)` | **0** |
| + fog — **all six terms together** | `(92,90,86)` | `(92,90,86)` | **0** |
| at `FogStart` (no fog yet) | `(158,129,95)` | `(158,129,95)` | **0** |
| past `FogEnd` (pure fog colour) | `(25,51,76)` | `(26,51,77)` | 1 |

The three lights were given different directions *and* different colours precisely so that a model
dropping one of them could not still look plausible. `PreferPerPixelLighting` on and off agree
exactly on a constant-normal surface, as they must.

The model confirmed, in CNA's own terms:

```
lightSum    = ambient + Σ lightDiffuse_i · max(dot(N, -dir_i), 0)
litRGB      = lightSum · DiffuseColor + EmissiveColor
spec_i      = pow(max(dot(normalize(E - dir_i), N), 0) · step(0, dot(N,-dir_i)), SpecularPower)
FragColor   = litRGB;  FragColor.rgb += Σ spec_i · lightSpecular_i · SpecularColor · alpha
FragColor.rgb = mix(FogColor, FragColor.rgb, fogFactor),  fogFactor = 1 - (d - FogStart)/(FogEnd - FogStart)
```

Specular is added **after** the diffuse product and **before** fog, and is scaled by the final
alpha. Phase 12's material mapping and phase 16's light budget can be computed against this.


### `HOUSE-00083` — `SurfaceFormat::Single` 2048² render target · **`BL-09` / `Q-01` SETTLED POSITIVELY**

Four stages, executed separately because each failure would lead somewhere different, and all four
succeeded:

| Stage | Result |
|---|---|
| Create 2048×2048 `Single` + `Depth24` | ✓ — reports its own format and depth format back correctly |
| Bind, clear, draw depth-varying geometry | ✓ |
| `GetData(float*)` over all 4 194 304 texels | ✓ — 3 396 649 drawn texels, **every one exactly `0.625`**, 797 655 cleared texels exactly `0`, **0 anything else** |
| Bind it as an effect texture and sample it | ✓ — `0.625` arrives in the shader as `159/255` |

**Tier E gets a real float shadow map.** The RGBA8 depth-packing fallback the architecture kept in
reserve is not needed, and the extra pack/unpack in every shadow lookup goes away. The fallback path
was nonetheless exercised in the same probe, so it is known to work whichever way this went.

The value `0.625` was chosen because it is exactly representable in binary: a round trip that
changed it at all would be a real difference, not a rounding artefact.

### `HOUSE-00084` — mip chains and MSAA · **PASS**

A 64×64 `Color` target with `mipMap = true`, at MSAA 0 and MSAA 4:

| | MSAA 0 | MSAA 4 |
|---|---|---|
| `LevelCount` | 7 (64 → 1, the full chain) | 7 |
| `MultiSampleCount` | 0 | **4** |
| Level 0 after drawing `(128,64,191)` | `(128,64,191)` | `(128,64,191)` |
| Sampled back through `BasicEffect` after unbinding | `(128,64,191)` | `(128,64,191)` |

MSAA 4 is genuinely allocated (the target reports it) and resolves to the exact drawn colour, which
is the step an implicit-resolve bug would break.

### `HOUSE-00085` — `BL-02` · **the premise is FALSE; stencilling works**

The probe was built so that a working stencil and an inert one produce *different images*, and so
that neither verdict could be inferred from an absent exception:

1. clear colour, depth and stencil to 0 on a `Depth24Stencil8` target;
2. pass 1 — `CompareFunction::Always` + `StencilOperation::Replace`, `ReferenceStencil = 1`, drawn
   over the **left half only**, writing black;
3. pass 2 — `CompareFunction::Equal`, `ReferenceStencil = 1`, drawn over the **whole** quad in white.

If `ReferenceStencil` were inert, pass 2 would cover everything.

```
  [--] stencil mask result   left half 2048/2048 lit, right half 0/2048 lit
  [--] BL-02 verdict         STENCIL WORKS -- the mask confined the second pass
```

`BL-02` is corrected. Anything in `cna-house` that was designed around an inert stencil — mirror
and window-portal masking in particular — may use the stencil buffer after all.

### `HOUSE-00086` — `BL-03`, MRT attachment 1 · **confirmed, with its scope stated**

`SetRenderTargets` with two `Color` targets bound at once succeeded. After one `BasicEffect` draw:
attachment 0 held `(255,128,64)`, attachment 1 held `(0,0,0)`.

**What this does and does not show.** It shows the renderer does not broadcast a single-output draw
to every attachment, and that binding two targets is not itself an error. It does **not** show what
a *compiled* `.fx` declaring two outputs would do — that is a Tier E question, and `BL-03` as it
affects Tier E is only answerable once `HOUSE-00087` establishes whether compiled effects work at
all. Recorded as such rather than overclaimed. Tier S writes one attachment and is unaffected.

### `HOUSE-00110` — the `SurfaceFormat` survey · **complete, and the two lists barely overlap**

Every format was *used*, not merely created: each render-target candidate was bound, cleared and
drawn into, because a driver can accept a format at creation and fail on first use.

| | Supported |
|---|---|
| **As a `Texture2D`** | `Color`, `Bgr565`, `Bgra5551`, `Bgra4444`, **`Dxt1`, `Dxt3`, `Dxt5`**, `NormalizedByte2`, `NormalizedByte4` |
| **As a `RenderTarget2D`** | `Color`, **`Single`, `Vector2`, `Vector4`, `HalfSingle`, `HalfVector2`, `HalfVector4`** |

**Only `Color` is in both lists.** In particular `SurfaceFormat::Single` is a perfectly good render
target — `HOUSE-00083` rendered, read back and sampled one — but a `Texture2D` of that format
**cannot be created by the application at all**. Any float data `cna-house` wants on the GPU has to
arrive as the output of a render pass, never as an uploaded texture.

Surveyed at **both** graphics profiles, because `GraphicsDeviceManager` defaults to
`GraphicsProfile::Reach` and XNA 4.0's Reach profile forbids float formats outright — a survey run
only at the default would blame the driver for an XNA rule. The **lists are identical** at `Reach`
and `HiDef` on this renderer; only the refusal *message* changes (`"not available on
GraphicsProfile"` versus `"not implemented by the selected renderer"`). So the limit here is
EasyGL's, and switching `cna-house` to `HiDef` would buy nothing on this platform.

### `HOUSE-00111` — DXT · **it works, but only through `.xnb`**

| Source | Container | File size | Runtime `SurfaceFormat` |
|---|---|---|---|
| 64×64 RGBA, `textureFormat: Dxt1` | `.cnb` | 16 752 B | **`Color`** |
| 64×64 RGBA, `textureFormat: Dxt5` | `.cnb` | 16 752 B | **`Color`** |
| 64×64 RGBA, `textureFormat: Dxt1` | `.xnb` | **2 235 B** | **`Dxt1`** |
| 64×64 RGBA, `textureFormat: Dxt5` | `.xnb` | **4 283 B** | **`Dxt5`** |
| 64×64 RGBA, no parameter | `.xnb` | 16 571 B | `Color` |

**CNB texture schema 1 is frozen to Rgba8.** The pipeline says so itself rather than failing:

```
warning (CNA.TextureProcessor): textureFormat Dxt1 has no representation in CNB texture schema 1,
which stores Rgba8 only; this .cnb keeps the uncompressed pixels. Build with --format xnb to get
the compressed texture.
```

That warning is easy to miss in a large build and the resulting asset is silently 8× larger, so it
is worth a build-gate of our own.

**Consequence for the Linux content profile (`cna-house.md` §27.2):** block compression is real —
8× for `Dxt1`, 4× for `Dxt5`, and the blocks stay compressed all the way to the GPU — but **texture
assets that want it must be built as `.xnb`, not `.cnb`.** Combined with `HOUSE-00064`'s finding
that `.xnb` wins the resolution order, a deliberately mixed tree (models and audio as `.cnb`,
textures as `.xnb`) is coherent; an accidental one is a trap.


### `HOUSE-00087` / `HOUSE-00088` / `HOUSE-00089` — compiled effects · **PASS. Tier E is viable.**

All four of `HOUSE-00087`'s acceptance points hold, and the two dependent tasks with them: 17/17
checks in `p1-fxload`.

**The build.** `fxc.exe` from the June 2010 DirectX SDK, run under Wine, via `cna-content`:

```
$ cna-content build build-probe/p1-fx/P1Effect.fx -o .../P1Effect.xnb --format xnb \
      --fx-compiler <fxc.exe> --fx-compiler-launcher tools/effects/fxc-wine.sh
[BUILD] P1Effect -> .../P1Effect.xnb (3424 bytes;
        CNA.EffectSourceImporter -> CNA.EffectSourceProcessor -> CNA.XnbEffectWriter)
```

**Finding — the documented `--fx-compiler-launcher wine` does not work on its own, and the fix is
ours.** `cna-content` builds the `fxc` command line with ordinary Unix absolute paths, and `fxc` is
a Windows tool that introduces *options* with `/`:

```
error: Unknown or invalid option '/tmp/cna-fx-0-140733683835328/effect.fxb', use /? for help
```

Neither side is wrong; two conventions collide, and `--fx-compiler-launcher` is precisely the seam
for it. `tools/effects/fxc-wine.sh` is that launcher: it translates any argument naming a path that
**exists**, and any argument following a path-taking option, through `winepath -w`, and passes
everything else through. (Testing the *parent* directory instead of the argument would translate
`/T` as well, because `dirname /T` is `/` — a mistake the script's first version made and its
comment now prevents.) It is offline tooling, so the XNA-only rule does not reach it; a machine with
no Wine simply builds no Tier E, which is what ADR-0003 already requires.

**The runtime.**

| Measurement | Result |
|---|---|
| Techniques discovered | 2 — `Tint`, `Textured` (plus `MultiTarget`, added later) |
| Parameters discovered | 3 — `WorldViewProj`, `TintColor`, `BaseTexture` |
| `technique["Tint"]` selected by name, `TintColor = (1, 0.5, 0.25, 1)` | `(255,128,64)` — the parameter reaches the shader **exactly** |
| Same technique, `TintColor = (0.25, 0.75, 1, 1)` | `(64,191,255)` — a parameter change changes the output |
| `technique["Textured"]`, same tint, 0.5-grey texture | `(128,64,32)` — each channel halved, so the technique really switched |
| Switched back to `Tint` within the same frame | `(255,128,64)` — bit-identical to the first draw |
| `SpriteBatch::Begin(..., effect)` (`HOUSE-00089`) | `(128,128,128)` — the sprite is drawn through the custom effect |

**Finding — `Load<Effect>` does not compile.** `Effect` is neither copyable nor
default-constructible and its reader is registered for `std::shared_ptr<Effect>`, so the call is
`content.Load<std::shared_ptr<Effect>>(name)`. Worth stating because `Load<Model>` *does* return by
value, so the two are not consistent and neither is guessable.

**Finding — `BL-03` is false for compiled effects** (`HOUSE-00086` reopened and closed). With a
stock effect, which declares one output, attachment 1 stays black — the renderer does not broadcast.
With a technique declaring `COLOR0` and `COLOR1` carrying deliberately different values, attachment 0
received `(255,128,64)` and attachment 1 received its **own** `(32,223,96)`, matching the authored
`(0.125, 0.875, 0.375)` to within a rounding step. MRT works. Tier E may use it.


### `HOUSE-00090` / `HOUSE-00092` / `HOUSE-00093` / `HOUSE-00094` / `HOUSE-00106` / `HOUSE-00107` — the performance tranche · **PASS**

**Method, stated once because every number below depends on it.** Release (`-O3 -DNDEBUG`),
`OPENGLES3`/EasyGL, AMD Radeon 780M (radeonsi, phoenix), Mesa 25.0.7, into a 512×512
`RenderTarget2D`. **3 warm-up rounds are discarded**, then **21 samples** are taken and the
**median** reported — an odd count, so the median is an actual sample rather than an average of two.

CPU submission and GPU completion are reported **separately**, because a loop that only fills a
command buffer says nothing about a frame. Every "to completion" figure ends with a **one-texel**
`GetData` on the render target, which forces the GPU to finish. One texel and not the whole surface:
a full 512² readback would cost more than the work being measured. (The whole-surface `GetData`
overload correctly refuses a short buffer — *"elementCount is less than the number of pixels in the
requested region"* — so the rect overload is the right tool, not a workaround.)

| # | Measurement | CPU | To GPU completion |
|---|---|---|---|
| `HOUSE-00092` | 2 000 dynamic quads/frame, `SetDataOptions::Discard` | **0.055 ms** | **0.171 ms** |
| `HOUSE-00106` | 1 000 × `EffectPass::Apply()` alone | **0.184 ms** (**0.184 µs each**) | — |
| `HOUSE-00106` | 1 000 × (`Apply` + `DrawIndexedPrimitives`) | **8.15 ms** (**8.15 µs per draw**) | **13.44 ms** |
| `HOUSE-00093` | 200 instances, one `DrawInstancedPrimitives` | — | **0.156 ms** |
| `HOUSE-00093` | the same 200 as separate draws | — | **2.144 ms** |
| `HOUSE-00107` | 4 MiB `Texture2D::SetData` (1024²) | 8.57 ms | **9.30 ms** → **430 MiB/s** |
| `HOUSE-00107` | 1 MiB `Texture2D::SetData` (512²) | — | **2.47 ms** → **405 MiB/s** |

**What these numbers mean for the design, in order of how much they change it.**

1. **The draw call is the budget, and `Apply()` is not.** `EffectPass::Apply()` costs 0.184 µs —
   effectively free, and 44× cheaper than the draw that follows it. A `DrawIndexedPrimitives` costs
   **8.15 µs of CPU**, so **1 000 draw calls is 8.15 ms of CPU submission alone** — half a 60 Hz
   frame before any game logic runs. The room/portal design's job is therefore to reduce *draw
   calls*, not to reduce state changes: batching by material is worth far less than not submitting
   the room at all. A practical ceiling of **300–400 draws per frame** leaves room for everything
   else.
2. **Instancing is worth 13.7×** and should be the vegetation and neighbourhood path, as
   `HOUSE-00093` hoped. **Scope:** `BasicEffect` has no per-instance input, so this measures the
   *draw path* — that `DrawInstancedPrimitives` works, consumes a second stream at instance
   frequency 1, and is dramatically cheaper. A stock effect cannot actually *read* the per-instance
   offset; consuming it needs a Tier E `.fx`, which `HOUSE-00087` has now shown to be available.
3. **A 4 MiB per-frame texture promotion does not fit.** At 9.3 ms it is more than half a 60 Hz
   frame. The 1 MiB measurement shows the cost is **linear and bandwidth-bound** (430 vs 405 MiB/s),
   not a fixed per-call overhead — so the fix is simply to promote **≈1 MiB per frame** (2.47 ms) and
   spread a large texture over four frames. The streaming design must split promotions; it cannot
   treat 4 MiB as one atomic step.
4. **Dynamic geometry is nearly free.** 2 000 quads streamed with `Discard` cost 0.171 ms to
   completion, so a particle budget in the low thousands is not the constraint anyone expected it to
   be.
5. **`BL-07` is real.** `OcclusionQuery::PixelCount` returned **1** for a quad covering an analytic
   **16 384** pixels, and **0** when fully occluded. It is a boolean on this driver, exactly as §5
   claims. Any coverage ratio computed from it would be `1/area`. The N×N grid approximation stays
   in the design.
6. **32-bit indices are real.** A 70 000-vertex buffer with a triangle addressed by indices needing
   17 bits drew 106 261 lit pixels against an analytic ~106 000 — so the indices were not truncated
   to 16 bits, which is the failure a smaller fixture could not have detected.

**Finding — `Color` is not trivially copyable**, so a custom vertex struct containing one cannot go
through the `SetData<T>`/`GetData<T>` templates; the `static_assert` fires. Custom vertex layouts
store the packed `std::uint32_t` that `VertexElementFormat::Color` reads anyway.


### `HOUSE-00095` / `HOUSE-00096` / `HOUSE-00097` — 3-D audio · **PASS, after the observable was corrected**

The first version of this probe read `Volume`, `Pan` and `Pitch` back after `Apply3D` and reported a
flat curve. That was a **probe bug with a real finding inside it**:

> **`Apply3D` does not touch the public `Volume`, `Pan` or `Pitch` properties.** It stores
> attenuation, pan and Doppler in private state (`attenuation_`, `spatialPan_`, `dopplerFactor_`)
> and composes them with the caller's values only when writing the mixer track. The properties keep
> returning whatever the caller last set — measured across a 20 m sweep and an ±10 m pan sweep, all
> readings unchanged.

So a game **cannot read back what CNA applied**, and `cna-house` — which owns room/portal occlusion
(§32) — must model the same curve itself rather than query it.

What the probe measures, and what it reads, are kept strictly apart:

**Measured.** The public properties are unchanged (above). `Apply3D` is *not* inert: an instance put
into pan mode and then played **refuses** a later `Apply3D` — *"Apply3D cannot be called on a playing
instance that is not using 3D audio."* — while an instance aimed in 3D before playing accepts it
while playing. An implementation where `Apply3D` did nothing could not produce that distinction.

**Read from `modules/audio/src/Xna/SoundEffectInstance.cpp`, and labelled as read.** The curve
`cna-house` must calibrate against:

```
normalized  = distance / SoundEffect::DistanceScale
attenuation = normalized >= 1 ? clamp(1 / normalized, 0, 1) : 1
pan         = distance > 0 ? clamp(rightDisplacement / distance, -1, 1) : 0
```

At the default `DistanceScale = 1`: `0 m → 1.0000, 1 m → 1.0000, 2 m → 0.5000, 3 m → 0.3333,
5 m → 0.2000, 8 m → 0.1250, 10 m → 0.1000, 15 m → 0.0667, 20 m → 0.0500`.

In words: **full volume inside `DistanceScale`, then inverse *distance* beyond it** — not
inverse-square, and not a continuous falloff from zero. Pan is the listener-relative rightward
displacement over distance, clamped — a linear approximation with **no HRTF and no cone or
orientation term**. `rightDisplacement` is projected onto the listener's own `Forward × Up` axis, so
a rotated listener is handled correctly.

**`BL-11` holds, for a stronger reason than the blocker assumed** (`HOUSE-00096`). When
`emitter.DopplerScale * SoundEffect::DopplerScale` is zero, the factor is set to exactly `1.0f`
*without evaluating the Doppler math at all* — there is no rounding path by which a pitch change
could appear. Zero velocities alone would also give 1.0.

**`HOUSE-00097` — the voice ceiling is ours to impose.** 512 of 512 looping instances reported
`Playing`; **CNA refused nothing** up to the probe's own ceiling. There is no hardware limit
discoverable through the XNA surface, so the 32-voice budget is a design decision `cna-house` must
enforce itself — CNA will not tell us when the mixer has been overcommitted.

### `HOUSE-00102` / `HOUSE-00103` — storage and JSON · **PASS, with §5 corrected**

Write, read, list and delete all work through `StorageDevice`/`StorageContainer`, and a 276-byte
save read back byte-identical.

**Finding — the save path is not what §5 says.** `cna-house.md` §5 records the Linux root as
`$XDG_DATA_HOME/<app>` or `~/.local/share/<app>`, with `<app>` implied to be the game. It is not:
`StorageDevice.cpp:75` is `appName_.empty() ? "game" : appName_`, and the **only** setter is
`SetAppNameEXT` — a `CNAEXT` identifier ADR-0001 forbids. The measured root is:

```
/home/<user>/.local/share/game/P1Probe
```

So an XNA-only game's saves land under a directory literally called `game`, shared with every other
CNA application on the machine. **The fix needs no extension:** the *container* name is ours through
plain XNA, so `BeginOpenContainer("CnaHouse")` gives `.../game/CnaHouse/`, which is unambiguous. The
probe verified the container name really is a directory level by finding its own. `HOUSE-00113`
corrects §5 and ADR-0008.

**`HOUSE-00103` — float round-trip is bit-exact.** The awkward values were chosen to break a naive
serialiser — `1/3`, `0.1` (repeating in binary), `1.1754944e-38`, `3.4028235e+38`, `-0.0`, `2.0` —
and compared by **bit pattern**, not epsilon, because a save that drifts one ulp per cycle still
corrupts a long-running house, only slowly. **6/6 returned bit-identical.** Nested objects, string
arrays in order, and integers-staying-integers all hold.

### `HOUSE-00104` — the math the room/portal system rests on · **PASS, 24/24**

Every expectation is a number computed by hand, in a frame chosen so it is exact: the frustum is
**orthographic and axis-aligned**, so its eight corners are exactly the corners of a box with the
projection's own extents and there is no perspective divide to force a tolerance.

| Call | Expected | Measured |
|---|---|---|
| `BoundingFrustum::GetCorners` extents | `x[-2,2] y[-1,1] z[-11,-1]` | exact |
| `Ray::Intersects(BoundingBox)`, aimed from 5 units at a face at z = 1 | `4.0` | `4.000000` |
| `Ray::Intersects(BoundingSphere)`, r = 2 | `3.0` | `3.000000` |
| `Ray::Intersects(Plane)`, from y = 3 to y = 0 | `3.0` | `3.000000` |
| `Ray` starting **inside** the box | `0.0` | `0.000000` |
| `Ray` pointing **away** (its line would hit) | miss | miss |
| `BoundingBox::CreateFromPoints` on an unordered list with duplicates | `min(-2,-1,-4) max(3,5,9)` | exact |
| Frustum vs box: inside / straddling / behind / beyond far | `Contains` / `Intersects` / `Disjoint` / `Disjoint` | all four |
| A sphere **tangent** to a side plane | intersects | intersects |
| A box **face-touching** another | intersects | intersects |

The last two are the cases an epsilon error flips, and the "ray pointing away" case is the one a
naive line-intersection implementation gets wrong. Phase 9 can be built on these.

### `HOUSE-00100` / `HOUSE-00101` — input · **`HOUSE-00101` PASS; `HOUSE-00100` INCONCLUSIVE, and left open**

`HOUSE-00101` is settled. `Keyboard::GetState` answers and `IsKeyDown` agrees with
`GetPressedKeys`. All four `GamePad` slots report **not connected**, and `GetCapabilities` agrees
with `GetState` on every one — the consistency is the point, since a game that trusted only one of
them would be wrong half the time. `TouchPanel::GetCapabilities` and `GetState` can be called on
desktop **without throwing**, report `IsConnected = false` and **0 touches**, so a game may poll
them unconditionally.

`HOUSE-00100` ran its full **10 000 frames** — deliberately not shortened, since a fractional
per-frame error is invisible in 100 frames and ruins a camera in 10 000 — and the result is
**inconclusive for the drift criterion**, which is recorded rather than rounded to a pass:

* **Measured:** `Mouse::SetPosition(400, 300)` **is** reflected by a `GetState` in the same frame.
* **Measured:** every one of the 10 000 subsequent frames read `(0, 0)`.
* **Cause, from `modules/input/src/Xna/Mouse.cpp:117`:** `GetState` returns a *snapshot* the platform
  layer maintains from SDL mouse-motion events. On an unattended desktop the pointer never enters or
  moves over the probe window, no motion event arrives, and the snapshot never leaves its initial
  value. **No delta measured here is a delta**, so no drift figure from this run means anything.

Two facts are established regardless, and both change how the camera is written:

1. **`Game::IsActive` is not a proxy for "the mouse is usable".** It was `true` on all 9 999 frames
   while the snapshot never advanced. A camera gating only on `IsActive` would consume garbage.
2. **`GetState` is event-driven, not a live cursor query.** The camera must seed its previous
   position from a real motion event and must never assume the cursor starts centred.

`HOUSE-00100` stays **unchecked** in `plan.md`. It needs a session with the pointer actually over the
window, and `HOUSE-00115` lists it as such.


### `HOUSE-00098` / `HOUSE-00099` — video · **PASS**

The clip is generated locally by `ffmpeg` from `lavfi` sources: 64×64, 2 seconds, 10 fps, colour
changing every half second — red, green, blue, white — over a 440 Hz sine. **Nothing is downloaded
and no third party's media is involved**, so there is no licensing question and the expected pixel
at any timestamp is known by construction.

Frame advance is *measured*, not assumed. The texture is read back sixteen times across the clip and
each centre texel classified against the four authored colours:

```
  [--] GetTexture calls    16, of which 0 returned null
  [--] sampled frame colours   R R R  G G G G  B B B B B  W W W W
  [--] PlayPosition at each sample
       0.12 0.24 0.36 0.48 0.60 0.72 0.84 0.96 1.08 1.21 1.33 1.45 1.57 1.69 1.81 1.93
```

Exactly the authored sequence, at the authored timestamps. A player handing back the first frame
forever would pass a test that only asked "did a texture come back"; this one it could not pass. The
audio mixer opened a real device for the clip (`44100 Hz stereo`), so the soundtrack is live.

**`HOUSE-00099` / `BL-05` — settled, with one detail the blocker did not anticipate.** Built in
`build-consumer/` with `CNA_ENABLE_VIDEO=OFF`:

* `VideoPlayer` still **constructs** and reports `Stopped`;
* **`Load<Video>` still succeeds** — content loading is not gated, only playback is;
* `Play()` throws *"Video playback is unavailable because CNA was built without the optional FFmpeg
  video backend. Configure with `-DCNA_ENABLE_VIDEO=ON`, or use AUTO with all required FFmpeg
  development packages installed."* — a refusal that names its own fix;
* the player is still a usable object afterwards and `Stop()` is still callable.

The linking half is the half that matters: phase 21's television can be a feature that is absent on a
platform without the rest of the house failing to build.

**Finding — a third `Load<T>` shape.** `Load<Video>` returns **by value**, like `Model` and unlike
`Effect` (whose reader is registered for `shared_ptr<Effect>`). The three are not consistent and none
is guessable, but CNA says so precisely when asked wrongly: *"'P1Clip.cnb' holds a Video asset, which
is not the type requested for 'P1Clip'."*

### `HOUSE-00105` — the `HEADLESS` renderer · **PASS**

The acceptance criterion is taken literally: **600 frames with no display server**. The probe asserts
from *inside the process* that `DISPLAY` and `WAYLAND_DISPLAY` are both unset, because a test that
silently ran against a live X server would prove nothing about CI and is the easiest possible thing
to get wrong.

```
$ env -u DISPLAY -u WAYLAND_DISPLAY ./build-consumer/p1-headless
[INFO][RENDER] CNA: graphics renderer: HEADLESS
  [ok] no display server is reachable from this process
  [ok] Initialize ran
  [ok] Update ran for 600 frames        600
  [--] Draw calls                       599
  [ok] GameTime advances                0.0116 s across 600 updates
p1-headless: 5/5 checks passed        # exit 0
```

`Draw` runs too, so the whole frame loop is exercised rather than just `Update`. Phase 44's automated
tests and phase 2's CI have a foundation.

### `HOUSE-00108` — resource rebuild after a device reset · **PASS, through a different door**

The task named EasyGL's `DebugSimulateContextLoss`. That call is declared in
`CNA/Internal/Renderers/Common/IGraphicsRenderer.hpp` — a `CNA/` include of a `CNA::Internal::`
type — so **ADR-0001 forbids it twice over** and `cna-house` can never call it, in a probe or
anywhere else. Reaching for it to "just measure once" would have been exactly the erosion the rule
exists to prevent.

The question the task was really for was asked through the XNA 4.0 surface instead —
`GraphicsDevice::Reset()` with the `DeviceLost` / `DeviceResetting` / `DeviceReset` events:

| | Result |
|---|---|
| `GraphicsDevice::Reset()` | callable, no exception |
| Events raised | `DeviceResetting` ×1, `DeviceReset` ×1 (**`DeviceLost` ×0**) |
| Texture, vertex and index buffers rebuilt from the same CPU state | all three |
| The rebuilt scene, sampled | `(128,128,128)` — **identical** to before the reset |

So the property the Web port needs — resources reconstructible from CPU-side state, with an event to
say when — holds on Linux and is testable there. Note that **`DeviceLost` does not fire**: a game
that hung its rebuild on that event alone would never rebuild. `DeviceReset` is the one to use.

### `HOUSE-00109` — anisotropic filtering · **available and effective**

| | Linear (trilinear) | Anisotropic |
|---|---|---|
| Pixels differing between the two images | — | **3 056** of 65 536 |
| Mean per-row far-field contrast | 6.04 | **8.90** (**1.47×**) |
| Best single row | 0.00 (fully blurred out) | **47.68** (fully resolved) |

**Verdict: available and effective; the `linux` profile enables it.** This is measured once here and
written into the profile — it is what `HOUSE-00916` keys off, and it is never a runtime query.

Two fixture corrections were needed before the measurement meant anything, and both are worth
recording because they are the standard ways to fake this result:

1. The first fixture used a **4-texel checker repeated 60×**. That is minified so hard that *both*
   modes collapse to flat grey across the whole floor, and the probe dutifully reported a contrast
   of 0.00 for each — a measurement of the fixture, not the driver. A realistic 32-texel tile at 16
   repeats is what discriminates.
2. The first summary statistic counted **rows improved versus rows worsened**. With a checkerboard
   that oscillates with wherever a tile boundary happens to fall, and it swung 21 against 28 even
   while the image as a whole gained 47 % more contrast. The aggregate over every lit row is the
   statistic that is not an artefact of tile phase.


### `HOUSE-00091` — the same measurements under `OPENGL33` · **the grid approximation is validated**

`build-consumer/` was reconfigured from the same source directory with
`-DCNA_GRAPHICS_RENDERER=OPENGL33` and the whole performance probe re-run against
`OpenGL 4.6 (Core Profile) Mesa 25.0.7`. (The probe's own "configuration" line is a compile-time
constant and still says `OPENGLES3`; the renderer actually in use is on CNA's `[RENDER]` log line
immediately above it. Recorded rather than trusted.)

**The decisive result, and the reason this task existed:**

| `OcclusionQuery::PixelCount` for a quad of analytic area 16 384 | |
|---|---|
| `OPENGLES3` | **1** |
| `OPENGL33` | **16 384** — exact |

Same probe, same fixture, same driver, same GPU. So `BL-07`'s boolean degradation is a property of
the **ES profile's query target**, not of CNA and not of this hardware — exactly as `cna-house.md`
§5 claims — and the N×N grid approximation phase 9 is designed around is validated against a
renderer that returns a true count. That is what the task asked for and it is now answered.

**The rest of the tranche, for comparison. These differences matter to `HOUSE-00115`:**

| Measurement | `OPENGLES3` | `OPENGL33` |
|---|---|---|
| `OcclusionQuery` on a 16 384-pixel quad | 1 (boolean) | **16 384** (real tally) |
| `EffectPass::Apply()` | 0.184 µs | 0.202 µs |
| CPU per `DrawIndexedPrimitives` | **8.15 µs** | **12.23 µs** |
| 1 000 small draws, to GPU completion | 13.44 ms | 12.6–23.8 ms (run to run) |
| 2 000 dynamic quads, to completion | **0.171 ms** | 0.316–0.570 ms |
| 200 instances vs 200 draws | 13.7× | 12.8× |
| 4 MiB `Texture2D::SetData` | 9.30 ms, 430 MiB/s | 9.14 ms, 438 MiB/s |

Two things follow. **`OPENGLES3` submits draws about 1.5× more cheaply** than `OPENGL33` on this
driver, which is the opposite of what one might expect and reinforces ADR-0002's renderer choice.
And **the draw-call budget, the instancing win and the upload bandwidth are all renderer-independent
to within run-to-run noise** — so those three numbers can be treated as properties of the machine
rather than of the renderer, while the occlusion verdict emphatically cannot.


---

## `HOUSE-00115` — if you change renderer, re-run these

Every verdict in this file is a verdict about **`OPENGLES3` / EasyGL on Mesa 25.0.7 / Radeon 780M**.
Most would survive a renderer change; some emphatically would not. This is the list, ordered by how
badly a wrong assumption would hurt.

| Re-run | Why | Probe |
|---|---|---|
| **`OcclusionQuery` `PixelCount`** | **Known already to differ**: boolean under `OPENGLES3`, an exact 16 384 under `OPENGL33`, same driver and GPU. Phase 9's grid approximation exists *because of* this row. | `p1-perf` |
| **The `SurfaceFormat` survey, both lists** | Which formats are textures and which are render targets is a renderer property. `Single` being a render target but **not** a `Texture2D` is what forces float data to arrive from a render pass. | `p1-formats` |
| **`SurfaceFormat::Single` 2048² end to end** | Tier E's shadow map depends on it. The RGBA8 packing fallback exists for the case where it fails. | `p1-rtsingle` |
| **MRT with a compiled two-output effect** | `BL-03` was true for stock effects and false for compiled ones on this renderer; another renderer may differ in either direction. | `p1-fxload` |
| **Stencil** | `BL-02` claimed it was inert on three renderers and it is not inert here. Do not assume the correction generalises. | `p1-rtcaps` |
| **Compiled effects at all** | `CNA_EASYGL_COMPILED_EFFECTS` is an EasyGL option. A renderer without it makes Tier E unbuildable, which ADR-0003 already allows for. | `p1-fxload` |
| **MSAA and mip chains on a render target** | Resolve behaviour is renderer-specific. | `p1-rtcaps` |
| **Draw-call cost** | Already measured to differ by 1.5× between `OPENGLES3` (8.15 µs) and `OPENGL33` (12.23 µs). The 300–400-draw budget is a per-renderer number. | `p1-perf` |
| **Anisotropic filtering** | A driver/profile property; the `linux` content profile keys off it. | `p1-aniso` |
| **DXT staying compressed** | Block-format support is per renderer, and the `.xnb`-only route is a *container* fact that would survive, while the GPU support is not. | `p1-formats` |

**These do not need re-running on a renderer change**, because they are properties of the content
pipeline, the math library, or the host rather than of the renderer:

`HOUSE-00064`–`HOUSE-00069` (content resolution, textures, fonts, audio formats),
`HOUSE-00070`–`HOUSE-00074`, `HOUSE-00076` (glTF import, hierarchy, skin binding — the *import* is
renderer-independent even though the *draw* is not), `HOUSE-00102`–`HOUSE-00104` (storage, JSON,
math), `HOUSE-00107` (upload bandwidth — measured within noise on both renderers).

**Still owed, on any renderer:** `HOUSE-00100`'s mouse-drift measurement, which needs a session with
the pointer actually over the window.

---

## `HOUSE-00119` — CNA facts that must never be assumed again

The five that most changed what this project will do. Each was believed otherwise — by
`cna-house.md`, by CNA's own documentation, or by the probe's first draft — until it was measured.

**1. Blend indices are skin-local, not bone indices.** The compiled `Model` does not contain the
mapping from a vertex's blend index to a bone. It contains a *number*, `0..N-1` in the order the
glTF `skin.joints` array declared, and CNA inserts a synthetic `Root` bone that shifts every bone
index away from it. The joint-name list in the `.chanim` sidecar **is** the binding; without it the
skeleton cannot be driven at all. Measured 0/10 vertices under the bone hypothesis and 10/10 under
the skin-local one — the fixture was built so that exactly one of the two could be true.

**2. `Apply3D` writes nothing a game can read.** Attenuation, pan and Doppler go into private state
and reach only the mixer track; `Volume`, `Pan` and `Pitch` keep returning what the caller last set.
Every room-audio design that expected to *read back* CNA's spatial gain and then modify it has to be
rewritten to *model* the same curve instead. And the curve is **inverse distance beyond
`DistanceScale`, full volume inside it** — not the inverse-square that a physics-minded designer
would assume, and not a falloff that starts at zero distance.

**3. Half the blockers were wrong, in both directions.** `BL-02` (stencil inert), `BL-03` (MRT
attachment 1 black) and `BL-06` (24-bit PCM rejected) were all **false**. `BL-09` was *pessimistic*
— the float render target works perfectly and Tier E needs no packing fallback. `BL-07` was
**right**, and `HOUSE-00091` showed exactly *why* by reproducing it on one renderer and not the
other. A blocker inherited from a feature matrix is a hypothesis, not a fact, and the cost of
believing one is a design that routes around a limitation that does not exist.

**4. The XNA surface is not uniformly XNA-shaped in C++.** Collection iterators are `CNAEXT`, so
every `foreach` becomes an index loop. `Load<T>` returns by value for `Model` and `Video` but a
`shared_ptr` for `Effect`. `Matrix::getIdentityProperty()` is a property while `Vector3::Up` is a
plain static. `Color` is not trivially copyable, so it cannot go in a vertex struct. `SpriteFont`
has no default constructor. **`KeyboardState()` and `MouseState()` are themselves `CNAEXT`**, so an
input-state member cannot be default-constructed — and that one is invisible to
`check_xna_only.py`, because the identifier in the source is just the type name. `Copy*Bone
TransformsTo` requires a pre-sized destination. None of these is guessable from the C# API, and each
one is a compile error or a silent misread waiting in code that assumed otherwise — the
`VertexPositionNormalTexture` assumption read vertex 0 correctly and every later vertex from the
wrong offset. `docs/conventions.md` §5a carries the working rules.

**5. The measurement is only as good as the fixture, and a bad fixture reports a clean pass.** Four
probes in this phase produced confident, wrong answers before their fixtures were corrected: a
**closed box** cannot measure winding (both cull modes cover the same silhouette); a **4-texel
checker at 60× repeat** collapses under trilinear *and* anisotropic filtering, so both measure 0.00;
a **screen-parallel quad** passes a depth-equal test even where the two passes disagree; and reading
**`Volume` after `Apply3D`** measures a property that call never touches. In every case the probe
"passed" or produced a plausible number first. The habit that caught them was insisting on an
*analytic* expectation — a number computed independently, in a frame chosen so it is exact — rather
than a plausible-looking one.


---

## `HOUSE-00120` — phase-1 review

**The question.** Does any material design in `cna-house.md` still rest on a CNA behaviour that
phase 1 was meant to settle and did not?

**The method.** Every `NOT PROBED` row was traced back into `cna-house.md` to see whether a design
actually depends on it, rather than being waved through as "not needed". Every blocker row and every
open question was re-read against its measurement.

### The tally

| | Count |
|---|---|
| `PASS` | 44 |
| `DIFFERENT` — works, but not as §5 described; the documents are corrected | 5 |
| `NOT PROBED` — with a stated reason, each traced below | 6 |
| `FAIL` | **0** |
| `PENDING` | **0** |

Blockers: **`BL-02`, `BL-03` and `BL-06` had false premises** and are downgraded to notes;
**`BL-09` is resolved positively**; `BL-04` is downgraded M → L; `BL-05`, `BL-07`, `BL-11` and
`BL-12` are confirmed with their measured detail; `BL-01`, `BL-08`, `BL-10` and `BL-15` were already
design-avoided and remain so; `BL-13` (Android) and `BL-14` (Web) are gated to their own phases by
design. Open questions: **`Q-01` closed**, **`R-16` resolved**, **`R-06` largely retired**.

### Every `NOT PROBED` row, traced

| Row | Why it was not probed | Does a design rest on it? |
|---|---|---|
| `S-01` — 80 sample ports cover every subsystem | Sample ports are evidence about **CNA**, not about `cna-house`. Phase 1 re-proved the capabilities directly instead of inheriting them. | No — nothing inherits from it |
| `G-18` — WebGL context-loss handling | A browser property. `HOUSE-00108` probed the desktop equivalent (`GraphicsDevice::Reset`) and found resources rebuild from CPU state. | Deferred to phases 47–48 **by design**, and `BL-14` already gates them |
| `P-06` — `.fxb` pre-compiled bytecode | `cna-house` has no source of a `.fxb`; the `.fx` route is what it will use, and that route is `PASS`. | No |
| `P-08` — `cna_add_content` CMake integration | Build integration, consumed by phase 2 and tested by being used. Phase 1 drove `cna-content` directly, which measured the *pipeline*. | Phase 2's own exit criteria cover it |
| `RenderTargetCube` | Appears only in §5's availability table. | **No** — no design references it |
| `Curve`, `Song` / `MediaPlayer` | Appear only in §5's availability table. `Curve` is the XNA type; §24's `phaseIlluminationCurve` is our own function, not it. **D-21 is "no music"**, so `Song`/`MediaPlayer` are never used. | **No** |

### The one residual

**`HOUSE-00100` — the mouse recentring drift is unmeasured, and phase 8's first-person camera
depends on it.**

The probe ran its full 10 000 frames, but `Mouse::GetState` returns an event-driven snapshot and on
an unattended desktop no motion event ever reached the probe window, so every frame read `(0,0)` and
no delta measured was a delta. This is recorded as inconclusive rather than rounded to a pass.

**The design is not blind while it stays open.** Two facts *were* established and both constrain
phase 8 more than a drift figure would have:

* `Game::IsActive` is **not** a proxy for "the mouse is usable" — it was `true` on all 9 999 frames
  while the snapshot never advanced, so a camera gating only on `IsActive` would consume garbage;
* `GetState` is **event-driven, not a live cursor query**, so the camera must seed its previous
  position from a real motion event and must never assume the cursor starts centred.

`HOUSE-00100` stays unchecked in `plan.md`, `HOUSE-00115` lists it as still owed, and phase 8's
first task is where it gets measured — under a session with the pointer actually over the window,
which is the ordinary condition for that work anyway.

### Verdict

**No material design in `cna-house.md` rests on an unverified CNA claim, with one named exception**
whose owner task is open, whose impact is bounded to phase 8, and whose two established facts
already constrain the design. Phase 1 has done what it was for: the architecture now rests on 44
measured capabilities, five corrected ones, three disproved blockers and one resolved open question,
rather than on a feature matrix.


### `HOUSE-00118` — CNA's own test suites, as a checkout-health check · **healthy**

The per-example `cna_test_easygl_*` render binaries are not built in this checkout, so the five
module suites that are built were run instead — the same coverage in aggregate.

| Suite | Ran | Passed | Skipped | **Failed** |
|---|---|---|---|---|
| `CnaMathTests` | 846 | 846 | 0 | **0** |
| `CnaContentTests` | 1 798 | 1 792 | 4 | **2** |
| `CnaGraphicsTests` | 2 364 | 2 311 | 53 | **0** |
| `CnaRuntimeTests` | 169 | 166 | 2 | **1** |
| `CnaInputModuleTests` | 500 | 500 | 0 | **0** |
| **Total** | **5 677** | **5 615** | **59** | **3** |

The three failures are `XnbContentPipelineTest.SpriteFontRuntimeXnbAndTranscodedCnbHaveEquivalent
Semantics`, `ContentManagerVideoXnbTest.TheObjectReferencedFormLoadsToTheSameValuesAsTheInlineOne`
and `GameWindowPlatformTest.DelegatesStateAndGeometryToTheSelectedPlatformWindow`. **None touches a
route `cna-house` uses.** The first two concern `.xnb` ↔ `.cnb` transcoding equivalence and the
`.xnb` object-reference form; this project loads fonts and video from `.cnb` and uses `.xnb` only
for compressed textures (`HOUSE-00111`) and compiled effects (`HOUSE-00087`). They are recorded as
upstream, not reported and not patched (`CLAUDE.md` §3).

**Finding — a harness trap worth writing down.** The first run, from `cnanext/build/`, produced
**64 failures** in `CnaContentTests`. Every one was an unresolved fixture path: the tests address
their assets relative to the **repository root**, which is where `ctest` sets the working directory
and where running the binary directly does not. Re-run from the root, the same 29 tests passed.

A harness that reports 64 red for one wrong `cd` is a harness that will be believed — and the same
mistake is one line away in this project. `tests/CMakeLists.txt` therefore sets
`WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}` on every discovered test, with that reason written beside
it.


---

## Probe hygiene

Every probe in phase 1 was built in `build-probe/` or `build-consumer/` — both from the openeggbert
closed list, both in this repository, never in `/tmp`, never in the session scratchpad, never a
per-ticket directory — with `CCACHE_DIR="$HOME/.cache/ccache"` and `CCACHE_BASEDIR=/rv`, and every
source carries a `p1-` (or `p0-`) file-name prefix.

**`HOUSE-00114`, with a correction.** The task said to delete the binaries and *keep the probe
sources in `build-probe/`*. Kept there they would not survive a clone, because `.gitignore`'s
`/build*/` excludes the whole directory — which is not "kept" in any useful sense. The sources
therefore live in **`tests/probes/phase1/`**, which is tracked, and `build-probe/CMakeLists.txt`
globs them from there. `tests/` was chosen over `tools/` deliberately: it is the one tree where
**both** gates already cover the code — `check_layout.py` permits C++ there, and
`check_xna_only.py`'s `TEST_ROOTS` scans it, so the probes are held to the same XNA-only rule as the
runtime. They pass it.

`build-probe/` now holds only build products and generated fixtures, and is deleted whole.

| Probe | Tasks | Fate |
|---|---|---|
| `p0-result-check.cpp` | `HOUSE-00042` | Deleted (previous session) |
| `p1-hello.cpp`, `p1-strict-negative.cpp`, `p1-content.cpp` | `HOUSE-00062`–`HOUSE-00069` | Deleted (previous session) |
| 24 `p1-*.cpp` + `p1-common.hpp` | `HOUSE-00070`–`HOUSE-00111` | **Sources kept in `tests/probes/phase1/`**; binaries deleted |
| `p1-make-gltf.py`, `p1-split-skins.py` | fixtures, `HOUSE-00076` | **Kept** — the generators are what make the expectations reproducible |
| `assets-src/Effects/P1Probe.fx` | `HOUSE-00087`–`HOUSE-00089` | **Kept** — the only place `check_xna_only.py` permits an effect source, and keeping it means the Tier E pipeline stays exercised |
| `build-probe/p1-fixtures/`, `p1-content/`, `p1-src/`, `p1-content-xnb/`, `p1-content-fx/`, `p1-content-child/` | all | Deleted — regenerated in seconds by the tracked generators |

`tests/probes/phase1/README.md` records what each probe measures and how to build one.

**`cna-content` was not rebuilt.** The host tool already present in CNA's own `build/` was reused,
which is correct under openeggbert build rule 2 and legitimate under `CLAUDE.md` §1, since offline
tooling is not runtime and is unconstrained.

**Build cost, measured (`HOUSE-00116`).** CNA + sharp-runtime + one probe, 724 targets, Release,
`-j6`, on a 16-thread host shared with other agents:

| | Wall | User CPU | Peak RSS |
|---|---|---|---|
| Cold, `CCACHE_DISABLE=1` | **330.3 s** | 1 802.7 s | 892 MB |
| The same, empty directory, **shared ccache warm** | **14.9 s** | 20.1 s | 342 MB |
| CMake configure alone | 26–49 s | — | — |
| Edit one probe source, rebuild and relink | **0.94 s** | — | — |
| Rebuild ten stock-effect and model probes | 11.6 s | — | — |
| Switch renderer in `build-consumer/` (full CNA relink, warm cache) | 75 s (`HEADLESS`), 138 s (`OPENGL33`) | — | — |

**A 22× wall-clock and 90× CPU saving from one warm cache.** That pair is the whole justification
for openeggbert build rule 1, measured rather than argued. The cold run was taken once, deliberately,
in the throwaway `build-consumer/` tree that this section deletes anyway, so it cost nothing that was
not already going to be discarded. Session ccache movement: **+1 812 hits, +364 misses**; the shared
cache stands at 34.5 GB of 100 GB.

---

## Checkout hygiene

The `cnanext` working tree carries local modifications at the time of probing. They must be
recorded, because a measurement against an unrecorded tree is not reproducible.

`cna-house` **does not modify CNA or sharp-runtime** (`CLAUDE.md` §3). The modifications below
were present before this session and were not made by it; they are listed so a later session can
tell whether a differing result comes from a different tree.

Measured 2026-09-06, `git status --porcelain` in `cnanext`: **11 modified files, all Markdown**
— `.gitignore`, `NEXT.md`, `NEXT_gltf.md`, three `integration/BATCH_*_STABILIZATION.md`, two
`integration/lanes/*.md`, two `modularization/**/*.md` and `plans/plan_binding.md`.

**Zero files under `modules/` are modified.** Every measurement below is therefore against
pristine CNA source at `d422038`; only planning prose differs from the committed tree.

`sharp-runtimenext` at `30ccdef` was not modified by this session either.

---

## Findings

### `HOUSE-00062` — minimal `Game`, `OPENGLES3`, `CNA_CNAEXT=OFF` · **PASS**

Probe `build-probe/p1-hello.cpp`: a `Game` subclass in strictly XNA-shaped API — no `CNA/`
include, no `CNA::` reference, no native GL call. It sets the back buffer to 1600×900, clears to
`CornflowerBlue`, counts frames in `Draw`, and calls `Exit()` at 300.

```
$ cmake -S build-probe -B build-probe -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache
$ cmake --build build-probe --target p1-hello -j16
$ ./build-probe/p1-hello
p1-hello: Initialize
p1-hello: frames=300 backbuffer=1600x900 expected=1600x900
p1-hello: PASS                                            # exit code 0
```

Runtime log:

```
[INFO][RENDER] EasyGLRenderer initialized with OpenGL OpenGL ES 3.2 Mesa 25.0.7-2+deb13u1
[INFO][RENDER] CNA: graphics renderer: OPENGLES3
```

* The window opened at exactly the planned 1600×900; `PresentationParameters` reported the
  requested size back, so the request was honoured rather than silently clamped.
* 300 frames drawn, clean `Exit()`, process exit code 0.
* **No GL error and no validation warning attributable to the probe.** One unrelated line appears
  on stderr — `Gtk-WARNING: gtk_disable_setlocale() must be called before gtk_init()` — emitted by
  the GTK file-dialog portal SDL3 initialises on this desktop. It is not produced by CNA or by the
  probe and does not affect the result.

**Entrypoint finding.** The samples include `CNA/Platform/Entrypoint.hpp` in the translation unit
that defines `main()`. That header is a **`CNA/` include and therefore forbidden in `cna-house`
runtime code.** Reading it settles the question: under `CNA_PLATFORM_SDL3` it expands to nothing
unless the target is Android or iOS, where SDL's entry-point rename is load-bearing. This probe
omitted it entirely and linked, ran and exited correctly. So a plain `int main()` is sufficient on
Linux, and the Android entry point becomes a phase-49 problem to solve in the build system rather
than a reason to put a `CNA/` include in `src/app/`. Recorded for `HOUSE-00127`.

**Capability line captured for later probes** (verbatim, from EasyGL at startup — it is *reported*
configuration, not a measurement, and does not close any row):

```
MSAA up to 8x; MRT up to 4 targets (GL draw buffers=8, color attachments=8, CNA/FNA cap=4);
indexed color masks: supported; anisotropic filtering: supported (Task 918, up to 16x);
texture SurfaceFormat: Color + NormalizedByte4 (RGBA8_SNORM) + NormalizedByte2 (RG8_SNORM)
  + Bgr565 (RGB565) + Bgra5551 (RGB5_A1) + Bgra4444 (RGBA4) + Dxt1/Dxt3/Dxt5 (S3TC blocks);
render-target SurfaceFormat: Color + half-float (RGBA16F) + float (RGBA32F)
```

Two things in that line matter later and are **not** settled here. The render-target list names
`RGBA32F`, not a single-channel `R32F` — `HOUSE-00083` must still create an actual
`SurfaceFormat::Single` target before `BL-09` can be closed. And `Dxt1/Dxt3/Dxt5` appear as
*texture* formats, which `HOUSE-00111` must confirm survive the pipeline still compressed.

---

### `HOUSE-00063` — does `CNA_CNAEXT=OFF` remove the engine layer? · **PASS, with a material caveat**

The stated criterion is met exactly:

```
$ nm -C build-probe/p1-hello | grep -c 'CNA::Graphics::'
0
```

**A zero is only evidence if the check could have found something.** The positive control is the
CNAEXT-enabled build already present in CNA's own tree (inspected read-only, not modified):

```
$ nm -C ../cnanext/build/modules/graphics-ext/libcna_graphics_ext.a | grep -c 'CNA::Graphics::'
6277
```

| Archive | `CNA_CNAEXT` | Size | `CNA::Graphics::` symbols |
|---|---|---|---|
| `../cnanext/build/.../libcna_graphics_ext.a` | `ON` | 37 445 582 B | **6 277** |
| `build-probe/CNA_BUILD/.../libcna_graphics_ext.a` | `OFF` | 68 996 B | **0** |

So `nm -C` demonstrably detects these symbols when they exist, the option genuinely collapses the
engine layer by a factor of 543 in size, and none of it reaches the linked binary. `X-02` is
`PASS`.

#### The caveat: `CNA_CNAEXT=OFF` does **not** remove every forbidden symbol

The forbidden list in `CLAUDE.md` §1 is wider than `CNA::Graphics::`. Running the whole list
against the linked probe binary:

| Forbidden identifier | Occurrences in `p1-hello` |
|---|---|
| `CNA::Graphics::` | **0** |
| `AvatarRenderer` | **0** |
| `ShaderEffect` | 98 |
| `PbrEffect` | 389 |
| `SkinnedPbrEffect` | 199 |
| `SkinnedModelEXT` | 50 |
| `GraphicsCapability` | 5 |
| `SupportsCapability` | 3 |
| `getSkinsEXTProperty` | 1 |
| `setOwnedResources` | 1 |

These are **not** in the `CNA::` engine namespace. They live inside `Microsoft::Xna::Framework::Graphics`,
in the always-compiled `modules/graphics` core:

```
Microsoft::Xna::Framework::Graphics::GraphicsDevice::SupportsCapability(CNA::GraphicsCapability) const
Microsoft::Xna::Framework::Graphics::GraphicsDevice::BuildRendererCapabilityProfileEXT() const
Microsoft::Xna::Framework::Graphics::Model::getSkinsEXTProperty() const
Microsoft::Xna::Framework::Graphics::Model::setOwnedResources(std::shared_ptr<void>)
Microsoft::Xna::Framework::Graphics::SkinnedModelEXT::AddPartEXT(...)
Microsoft::Xna::Framework::Graphics::PbrEffect / SkinnedPbrEffect / ShaderEffect
```

CNA states the design reason itself, in `AreaLightEXT.hpp`: *"an always-compiled XNA header must
not include one that exists only under `CNA_CNAEXT`, and an effect's public surface must not
depend on a build flag."* That is a defensible decision for CNA. Its consequence for `cna-house`
is precise and must not be misread:

> **`CNA_CNAEXT=OFF` removes the `CNA::Graphics::` engine layer. It does not make the other
> forbidden calls unlinkable. For those, `tools/ci/check_xna_only.py` is not a second line of
> defence — it is the only one.**

This does not weaken ADR-0001; it identifies where the rule is actually enforced. Two consequences:

* `HOUSE-00136` (the CI symbol check) can meaningfully assert `CNA::Graphics:: == 0` and
  `AvatarRenderer == 0`. It **cannot** assert the rest, because CNA's own core defines them. The
  task should be scoped to the two that are real, and say why the others are excluded.
* The source lint's coverage of `SupportsCapability`, `*EXT*`, `PbrEffect`, `ShaderEffect`,
  `SkinnedModelEXT` and `setOwnedResources` is load-bearing and must never be relaxed.

#### A compiler-level enforcement mechanism exists — and is not yet usable

`modules/core/include/CNA/CNAHelper.hpp` defines the `CNAEXT` marker macro:

```cpp
#ifdef CNA_STRICT_XNA_API
#define CNAEXT [[deprecated("CNAEXT: not part of the XNA 4.0 API surface")]]
#else
#define CNAEXT
#endif
```

With `-DCNA_STRICT_XNA_API -Werror=deprecated-declarations`, calling a CNAEXT-tagged member becomes
a **compile error**. That is exactly what ADR-0001 wants, enforced by the compiler rather than by a
Python lint, and it is a build flag rather than a runtime query, so adopting it would not breach
the XNA-only rule. It was tested in both directions.

Negative control — `build-probe/p1-strict-negative.cpp` calls `Model::getSkinsEXTProperty()`:

```
without the flag                                  -> compiles (exit 0)
with -DCNA_STRICT_XNA_API -Werror=deprecated-...  -> error: '...Model::getSkinsEXTProperty() const'
                                                     is deprecated: CNAEXT: not part of the
                                                     XNA 4.0 API surface           (exit 1)
```

**It works. It is also, today, unusable**, because it fires on genuine XNA 4.0 API. Compiling the
*clean* `p1-hello.cpp` — which calls nothing forbidden — under the same flag produces 8 distinct
errors:

```
Microsoft::Xna::Framework::Graphics::IVertexType::~IVertexType()
Microsoft::Xna::Framework::Graphics::PackedVector::IPackedVector::~IPackedVector()
Microsoft::Xna::Framework::Input::Touch::TouchPanel::MAX_TOUCHES
Microsoft::Xna::Framework::Media::VisualizationData::Size
Microsoft::Xna::Framework::Content::LooseFileContentTypeReader<T>
ContentManifestEntry · ContentManifestReaderUsage · ContentTypeReaderBase
```

`IVertexType`, `TouchPanel` and the content-reader base are real XNA 4.0 types; their destructors
and members carry the marker, so merely including `Game.hpp` trips the flag.

**Verdict: a promising second line of defence, blocked upstream on CNAEXT tagging precision.**
`cna-house` does not modify CNA (`CLAUDE.md` §3), so this is recorded, not patched. If CNA ever
tightens the tagging, `CNAHOUSE_STRICT_XNA` becomes a cheap and very strong addition to
`HOUSE-00124`. Until then the Python lint stands alone.

---

### `HOUSE-00064` — `ContentManager` resolution order · **PASS**

Two *different* payloads compiled to the same content name, so the answer is read off the pixels
rather than inferred from a file listing: a solid **red** 2×2 PNG to `.xnb`, a solid **blue** one
to `.cnb`.

```
$ cna-content build build-probe/p1-red.png  -o .../P1Resolution.xnb --format xnb
$ cna-content build build-probe/p1-blue.png -o .../P1Resolution.cnb --format cnb
$ ls .../P1Resolution.*        # both present: .cnb 384 B, .xnb 203 B
```

```
[HOUSE-00064] same name, .xnb vs .cnb   PASS  first texel R=255 G=0 B=0 -> .xnb WON
```

**`.xnb` wins over `.cnb`.** Row `C-03` confirmed as `cna-house.md` §5.1 claims. `cna-house`
compiles to `.cnb`, so the practical rule is: **a stale `.xnb` of the same name silently shadows
the `.cnb` the build just produced.** `cna_add_content` wiring (`HOUSE-00126`) must not leave both
in the output tree, and `HOUSE-00114`-style cleanup applies to content directories too.

The middle tier of the claim — a *literal path* between `.xnb` and `.cnb` — was not exercised;
this probe only ordered the two compiled containers.

---

### `HOUSE-00065` — `Texture2D` from `.cnb` · **PASS, with a correction that matters**

Source: a purpose-built 4×4 PNG in which **every texel is distinct** in all four channels, so a
round trip that reordered, flipped or dropped a channel could not pass by luck.

```
[HOUSE-00065] dimensions                  PASS 4x4 (expected 4x4)
[HOUSE-00065] surface format              PASS Color (expected Color)
[HOUSE-00065] alpha model: straight 1/16 exact, premultiplied 16/16 exact
[HOUSE-00065] GetData round trip          PASS 16/16 exact against the premultiplied model
```

The first run of this probe **failed**, reporting 1/16 texels exact. The one that matched was the
only fully opaque texel. The measured value at (1,0) was `66,13,35,223` where the source PNG holds
`75,15,40,223` — and `75 × 223/255 = 65.6`, `15 × 223/255 = 13.1`, `40 × 223/255 = 35.0`.

**The pipeline premultiplies alpha.** The probe was then changed to evaluate *both* models and
report which the data fits, rather than to assume either: straight alpha matches 1/16, premultiplied
matches **16/16 exactly**. CNA documents this (`docs/content-pipeline.md`): `TextureProcessor`'s
`premultiplyAlpha` parameter **defaults to `true`, exactly as XNA 4.0's `TextureProcessor.PremultiplyAlpha`
does**, because `BlendState::AlphaBlend` — what `SpriteBatch::Begin()` selects when given no blend
state — is the premultiplied blend.

The defect was in the probe's expectation, not in CNA. The finding is still worth its own row,
because three later systems would each have produced a subtly wrong result from the naive
assumption:

* any texture round-trip or golden-image test must compare against premultiplied values;
* the `DualTextureEffect` lightmap product of `HOUSE-00078` must not premultiply a second time;
* content authored as straight alpha and drawn with `BlendState::NonPremultiplied` will show dark
  fringes. `cna-house` uses the default, i.e. premultiplied, everywhere.

Recorded against `HOUSE-00078`, `HOUSE-00107` and the content-authoring conventions.

---

### `HOUSE-00066` — `SpriteFont` through the `.spritefont` route · **PASS**

```
$ cna-content build build-probe/p1-content-src -o build-probe/p1-content
[BUILD] Fonts/P1Font -> .../Fonts/P1Font.cnb (529488 bytes;
        CNA.FontDescriptionImporter -> CNA.FontDescriptionProcessor -> CNA.SpriteFontContentWriter)
```

```
[HOUSE-00066] loaded, line spacing sane          PASS lineSpacing=50
[HOUSE-00066] MeasureString scales with length   PASS "I"=13.000000x51.000000 "I"x10=130.000000x51.000000
[HOUSE-00066] glyphs rendered (ink present)      PASS 440 lit pixels of 16384
[HOUSE-00066] ink inside measured box            PASS ink bbox x[14,49] y[15,47] draw origin (10,8)
                                                      measured 44.000000x51.000000
```

Placement was **measured, not assumed**: the string was drawn into an offscreen `RenderTarget2D`,
read back with `GetData`, and the ink's bounding box compared against `MeasureString` and the draw
origin. Ink starts after the origin `(10,8)` and stays inside the measured box — glyphs are placed
where the advance widths say they are, and `MeasureString` is a usable layout oracle for the HUD.

`MeasureString("I")` = 13 px advance and `×10` = 130 px is exactly linear, as expected for a
kerning-free repeated glyph at `<Spacing>0</Spacing>`.

**Licensing.** The descriptor names `P1TestFont.ttf`, a copy of the host's
`/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf`, placed beside the `.spritefont` inside
`build-probe/` — which is `.gitignore`d and is deleted with the probes. **No font was added to the
repository**, and no redistribution claim is made or needed. The shipping font is chosen under the
phase-4 licensing tasks.

A pipeline note worth keeping: CNA resolves `<FontName>` to a **file beside the descriptor** first
and only then to an installed system font, warning when it falls back because that would make the
build depend on the machine. `cna-house` must always ship the `.ttf` next to the `.spritefont`.

---

### `HOUSE-00067` — 16-bit PCM `SoundEffect` · **PASS**

Fixture: `ffmpeg -f lavfi -i sine=frequency=440:duration=1.0:sample_rate=44100 -c:a pcm_s16le -ac 1`.

```
[BUILD] Audio/P1Tone16 -> .../Audio/P1Tone16.cnb (88504 bytes;
        CNA.WavImporter -> CNA.SoundEffectProcessor -> CNA.SoundEffectContentWriter)

[HOUSE-00067] duration                        PASS 1.000000 s (expected ~1.000000 s)
[HOUSE-00067] state is Playing after Play()   PASS state=0
```

The SDL3 mixer initialised on this host — `[AudioMixer] Requested format=0x0 channels=2 freq=44100;
application format=0x8010 channels=2 freq=44100` — so playback was exercised against a real device,
not a null sink. Duration is exact to the microsecond and `SoundEffectInstance::getStateProperty()`
reports `Playing` immediately after `Play()`.

---

### `HOUSE-00068` — the 24-bit PCM question · **`BL-06` IS WRONG**

`BL-06` predicted that 24-bit PCM is rejected, and this task's acceptance criterion was to record
the exact exception so the pipeline could assert against it. **There is no exception. Both paths
accept 24-bit PCM.**

**Path 1 — the content pipeline.** It converts, and says so:

```
$ cna-content build build-probe/p1-tone24.wav -o .../P1Tone24.cnb --format cnb
[BUILD] p1-tone24 -> .../P1Tone24.cnb (96304 bytes;
        CNA.WavImporter -> CNA.SoundEffectProcessor -> CNA.SoundEffectContentWriter)
  warning (CNA.SoundEffectProcessor): the source is 24-bit PCM and was converted to 16-bit PCM
  with round-to-nearest and saturation; this discards precision the source carried.
Built: 1  Skipped: 0  Failed: 0                          # exit code 0
```

**Path 2 — the runtime**, bypassing the pipeline entirely with `SoundEffect::FromStream` on the
untouched 24-bit file:

```
[HOUSE-00068] raw 24-bit FromStream ACCEPTED, duration 1.0000 s
[HOUSE-00068] raw 24-bit via FromStream   PASS  ACCEPTED (no exception), duration 1.000000 s
```

**Is the conversion correct, or merely silent?** The same 48 kHz signal was authored as 16-bit and
compiled the same way, and the two `.cnb` files compared byte for byte:

```
$ cmp -l .../P1Tone24.cnb build-probe/p1-tone24-as16.cnb | wc -l
29                       # of 96 304 bytes
$ cmp -i 400 .../P1Tone24.cnb build-probe/p1-tone24-as16.cnb
                         # no output: identical from byte 400 to EOF
```

All 29 differing bytes are in the header — the asset-name string (`p1-tone24` vs
`p1-tone24-as16`), its length fields and the build fingerprint. **The PCM payload is byte-identical.**
CNA's 24→16 conversion equals `ffmpeg -c:a pcm_s16le` sample for sample.

`BL-06` must be corrected. Its severity was already `L`; it is now closer to a pipeline note than a
blocker. The offline conversion step it mandates is **optional**, not required — see `HOUSE-00069`
for the one part of it that does matter.

---

### `HOUSE-00069` — NOX conversion · **PASS, and it corrects the `BL-06` command**

One representative file, selected without inventorying the collection:

| | |
|---|---|
| Source | `/rv/tmp/Essentials_Series_NOX_SOUND/Electromagnetic_NOX_SOUND/Electromagnetic_Car_Dashboard_Loop_Mono_Elektrousi_01.wav` |
| Format | `pcm_s24le`, 48 000 Hz, mono, 7.964396 s |
| SHA-256 | `55522170764eec9f6463d6056023aaf08ca08d63534ed50bf2dabe970c0ac5ec` |

```
[HOUSE-00069] duration  PASS 7.964399 s (16-bit @ 44.1 kHz, expected ~7.9644)
[HOUSE-00069] duration  PASS 7.964396 s (16-bit @ 48 kHz,   expected ~7.9644)
[HOUSE-00069] state is Playing after Play()   PASS   (both)
```

Both convert, compile, load, play, and preserve duration — the 48 kHz variant to the microsecond.

**The correction.** `cna-house.md` `BL-06` gives the command as
`ffmpeg -i in.wav -c:a pcm_s16le -ar 44100 out.wav`. Measuring the two halves separately shows the
resample, not the bit depth, is what damages the signal:

| Variant | Peak dB | RMS dB | Δ RMS vs source |
|---|---|---|---|
| Source (24-bit, 48 kHz) | −15.209455 | −32.406843 | — |
| 24→16 bit, **48 kHz kept** | −15.209542 | −32.405142 | **0.0017 dB** |
| 24→16 bit **and `-ar 44100`** | −15.469952 | −33.296462 | **0.889 dB** |

Bit-depth reduction is effectively lossless. `-ar 44100` costs 0.89 dB RMS and 0.26 dB peak on this
material, because resampling 48 → 44.1 kHz lowpasses content this high-frequency source actually
carries.

**Recommendation: drop `-ar 44100`.** The whole NOX collection is 48 kHz, CNA loaded and played the
48 kHz asset correctly, and the mixer opened at 44 100 Hz and resamples at playback anyway — so the
offline resample buys nothing and costs signal. `BL-06`'s command becomes
`ffmpeg -i in.wav -c:a pcm_s16le out.wav`.

Given `HOUSE-00068`, even that is optional: the pipeline already converts 24-bit sources itself,
bit-identically. The reason to keep an explicit offline step is **provenance, not format** — the
asset manifest of ADR-0012 wants both hashes recorded, and an explicit step is where that happens.

**No redistribution claim is made here.** This probe used one file to measure a format conversion.
The collection's licence status remains exactly what `cna-house.md` §63 says it is, and is settled
only by its dedicated phase-4 licensing task.

### `HOUSE-00070` / `HOUSE-00071` — static glTF → `.cnb` → `Model`, and the winding verdict · **PASS**

Fixture: `p1-make-gltf.py` writes `P1Static.glb`, an axis-aligned box whose six plane coordinates
are all distinct (`min(-1,-2,-3) max(4,5,6)`), with per-vertex-unique asymmetric UVs. Self-authored,
so every number the probe compares against is derivable from the generator rather than from another
CNA call. `p1-static` loads the compiled `.cnb`, reads the vertex and index buffers back with the
plain XNA `GetData` overloads, and compares element by element.

```
$ cna-content build build-probe/p1-fixtures/P1Static.glb \
      -o build-probe/p1-content/P1Static.cnb --format cnb
[BUILD] P1Static -> ... (2440 bytes; CNA.GltfImporter -> CNA.ModelProcessor -> CNA.ModelContentWriter)
$ ./build-probe/p1-static
  [ok] positions pass through bit-exact (no axis remap or flip)  0 of 24 differ
  [ok] normals pass through unchanged                            0 differ
  [ok] UVs pass through bit-exact (no V flip)                    0 differ
  [ok] indices are the source order, unreversed                  0 differ
  [ok] vertex-buffer bounds equal the authored bounds            min(-1,-2,-3) max(4,5,6)
  [ok] screen-space extents match the analytic projection (+Y is up)
       measured x[88,167] y[72,183] vs analytic x[88.0,168.0] y[72.0,184.0]
p1-static: 21/21 checks passed
```

`docs/gltf-conventions.md`'s claim of *no axis remap, no handedness negation and no V flip* is
therefore confirmed by measurement and not only by reading the importer.

**Winding (`HOUSE-00071`), measured numerically:**

| Cull mode | Pixels covered |
|---|---|
| `CullNone` | 8 960 |
| `CullClockwise` | **8 960** |
| `CullCounterClockwise` | **0** |

**`CullClockwise` is the correct state for glTF-authored geometry**, exactly as §5 claims.

This measurement needed a *second* fixture, and the reason is worth recording because it is an easy
mistake to repeat: **a closed solid cannot measure winding.** Run against the box, all three modes
covered an identical 8 960 pixels — with front faces culled you simply see the far faces through the
near ones, and the silhouette is unchanged. `P1Quad.glb`, an open single-sided quad, is what makes
the wrong cull mode render literally nothing.

**Finding — the vertex layout is not a built-in XNA vertex type.** `CNA.ModelProcessor` emits a
**48-byte** vertex for a primitive carrying `POSITION`/`NORMAL`/`TEXCOORD_0`:

| Offset | Usage | Format |
|---|---|---|
| 0 | `Position` | `Vector3` |
| 12 | `Normal` | `Vector3` |
| 24 | **`Tangent`** | `Vector4` |
| 40 | `TextureCoordinate` (index 0) | `Vector2` |

`VertexPositionNormalTexture` is 40 bytes, and the source `.glb` authored **no** `TANGENT`
attribute — the processor synthesises one. A first version of this probe read the buffer as
`VertexPositionNormalTexture` and got vertex 0 right and every later vertex wrong, because the
stride mismatch walked it off the data. Consequences: any `cna-house` code that reads model
geometry back (collision meshes, offline bake verification, the phase-44 asset tests) must declare
its own 48-byte struct and check it against `VertexDeclaration::GetVertexElements()` at load, never
name a built-in type.

### `HOUSE-00072` — bone hierarchy, `ParentBone`, `Root`, `CopyAbsoluteBoneTransformsTo` · **PASS**

Fixture `P1Hier.glb`: four nodes, depth three plus a sibling branch, a 90° rotation about Z that
does not commute with its translation, a non-uniform scale `(2, 0.5, 4)`, and one node authored as
an explicit glTF `matrix` rather than TRS — the only place CNA's importer converts a layout at all.
The generator computes every expected local and absolute matrix **in XNA's own convention**
(row-major storage, row-vector transform, `local = S·R·T`, `absolute = local · absolute(parent)`)
and emits them as C++ literals, so the probe compares CNA against arithmetic, not against CNA.

`p1-hier` reports 22/22, including a check that distinguishes a *transposed* result from a merely
wrong one — the failure mode that a transform-convention mistake actually produces.

```
  [--] bone count   5              [--] mesh count   4
  [--] bone[0] Root    index=0 parent=<none>  children=1
  [--] bone[1] P1Base  index=1 parent=Root    children=2  [1 0 0 0 | 0 1 0 0 | 0 0 1 0 | 10 0 0 1]
  [--] bone[2] P1Mid   index=2 parent=P1Base  children=1
  [--] bone[3] P1Tip   index=3 parent=P1Mid   children=0  [2 0 0 0 | 0 0.5 0 0 | 0 0 4 0 | 0 0 3 1]
  [--] bone[4] P1Wing  index=4 parent=P1Base  children=0
  [ok] P1Tip: local Transform            # authored as an explicit glTF matrix
  [ok] P1Tip: absolute transform
  [ok] an undersized destination is rejected, not silently grown   threw: destinationBoneTransforms
p1-hier: 22/22 checks passed
```

Four facts fall out of it:

* **CNA inserts a synthetic `Root` bone** above the glTF scene root. Bone count is *nodes + 1*, and
  `Model::Root` is that synthetic bone, not the first authored node. Any `cna-house` bone-index
  table must be built by **name lookup**, never by assuming node *i* is bone *i*.
* `Bones[i]->Index == i` holds, and every bone reaches `Root` by following `Parent`.
* The explicit-`matrix` node round-trips correctly, so `ConvertGltfMatrix` does what
  `docs/gltf-conventions.md` says.
* `CopyAbsoluteBoneTransformsTo` / `CopyBoneTransformsTo` **require a destination already sized to
  `Bones.Count`** and throw `std::invalid_argument("destinationBoneTransforms")` otherwise. They do
  not grow the vector. This is XNA 4.0's array contract preserved literally in C++.

### `HOUSE-00073` — `ModelMesh::BoundingSphere` from `.cnb` · **PASS, with a caveat that matters**

Populated and non-degenerate; it contains every vertex. It is **not minimal**:

| | Measured | Minimal for this box |
|---|---|---|
| Centre | `(1.726, -0.049, 2.114)` | `(1.5, 1.5, 1.5)` |
| Radius | `7.686` | `6.225` |

23 % over on radius, and the centre is 1.55 off in Y — the signature of an incremental
(Ritter-style) construction rather than a minimal enclosing sphere. `BL-12` is settled *positively*:
no offline bounds computation is needed for correctness. But the phase-9 and phase-41 visibility
work must treat it as a **cheap conservative reject only**; anywhere a tight bound is wanted,
`cna-house` computes its own from the vertex data it already reads back.

### Three CNA-versus-XNA shape differences these probes exposed

None of them is a defect; all three change how `cna-house` must be written, and none is visible
from reading `cna-house.md` §5.

1. **`ContentManager::Load<T>` returns `T` by value.** `Model` is a value type in CNA where XNA
   4.0's is a reference type. `Model* m = content.Load<Model>(...)` does not compile.
2. **Collection iterators are `CNAEXT`.** `ModelMeshCollection`, `ModelBoneCollection`,
   `ModelMeshPartCollection` and `EffectPassCollection` all mark `begin()`/`end()` `CNAEXT`, so a
   range-`for` over them is forbidden under ADR-0001 even though `foreach` over the same collection
   is ordinary XNA 4.0 in C#. **Every loop over an XNA collection in `cna-house` is an index loop**
   over `getCountProperty()`. This is pervasive, cheap to comply with, and expensive to discover
   late; `HOUSE-00136` should gate it.
3. **`Matrix::Identity` is `Matrix::getIdentityProperty()`** and `Vector3::Up` is a plain static —
   the property-name mapping is not uniform, so it is read per type rather than guessed.


### `HOUSE-00074` — the skinned binding, and the one fact that fixes the `.chanim` format · **PASS**

Fixture `P1Skin.glb`: a two-column, five-row ribbon bound to a three-joint chain, with rows 1 and 3
authored as exact 50/50 blends so a *dropped* weight is distinguishable from a *swapped* one. The
probe matches each compiled vertex to its authored counterpart **by position**, never by array
index, and then tests the blend index against both hypotheses at once.

```
  [--] bone count  6                    # five authored nodes + the synthetic Root
  [--] vertex declaration  stride=68 Position@0:Vector3 Normal@12:Vector3 Tangent@24:Vector4
                           TexCoord@40:Vector2 BlendWeight@48:Vector4 BlendIndices@64:Byte4
  [ok] blend weights equal the authored weights                       10/10
  [--] blend-index meaning  indexes Model::Bones on 0/10 vertices;
                            indexes the skin joint list on 10/10
  [--] name -> bone-index map a sidecar would carry   P1J0->2 P1J1->3 P1J2->4
p1-skin: 20/20 checks passed
```

**The verdict: blend indices are SKIN-LOCAL.** They are `0..N-1` in the order the glTF `skin.joints`
array declares, **not** indices into `Model::Bones`. On this fixture the two differ by two, because
of the synthetic `Root` and the skin root node.

This is acceptance path **(4)** of `HOUSE-00074`, not path (2), and it settles `R-16` and the
`.chanim` format of `HOUSE-00166`:

* the sidecar **must** carry the skin's joint names **in blend-index order**; that list *is* the
  binding, and nothing in the compiled `Model` reproduces it;
* at load the runtime resolves each name to a `Model::Bones` index (`bones[name]` — the collection
  has a by-name indexer, so this needs no search of our own);
* the skinning palette handed to `SkinnedEffect::SetBoneTransforms` is indexed by **skin-local**
  joint index, so it is built in the sidecar's order, not the bone order.

Two supporting measurements:

* **The compiled `.cnb` is byte-identical across rebuilds** — two builds of the same source both
  hashed `dc702b15…f06379` — so the joint order is stable, which is criterion (2)'s real content.
* The skinned vertex is **68 bytes**: the 48-byte static layout plus `BlendWeight` (`Vector4`, @48)
  and `BlendIndices` (`Byte4`, @64). `Byte4` caps a skin at 256 joints at the vertex level, well
  above `SkinnedEffect`'s own 72.

### `HOUSE-00075` / `HOUSE-00077` — project-owned animation and the bone cap · **PASS**

`p1-skinanim` implements the evaluator `cnahouse::anim` will ship: per-joint TRS keyframe tracks,
`Lerp`/`Slerp` sampling, composition in XNA's order, a parent walk to absolute matrices, and a
palette built with **inverse bind matrices the project owns** — taken from the source asset offline,
never from a CNA query.

It is checked analytically *before* anything is drawn: the bind pose must skin to the identity for
every joint (or the inverse bind matrices are wrong), and the evaluated bend must carry the tip from
`(0,4,0)` to exactly `(-2,2,0)`. Both hold.

Deformation is then measured, not assumed:

| | Covered pixels | Ink bounding box |
|---|---|---|
| Bind pose | 1 785 | `x[118,138] y[86,170]` |
| Bent 90° about +Z at the middle joint | 1 569 | `x[86,143] y[118,170]` |

1 800 pixels differ against a bind-pose coverage of 1 785; the silhouette extends 32 px further
**left** and starts 32 px **lower**, which is what a +90° turn about `+Z` at the middle joint
predicts and what a merely-scaled or merely-translated result would not produce.

`HOUSE-00077`: `SkinnedEffect::MaxBones == 72`; `SetBoneTransforms` accepts exactly 72 and throws
`boneTransforms exceeds MaxBones.` at 73. The cap is real and enforced.

**Finding — `SkinnedEffect` refuses `LightingEnabled = false`**, throwing *"SkinnedEffect does not
support setting LightingEnabled to false."*, exactly as XNA 4.0's does. There is no flat unlit
skinned draw. Anything `cna-house` wants to draw unlit and skinned must use a full ambient term
(`AmbientLightColor = (1,1,1)` with the directional lights off), which is what the probe does.

### `HOUSE-00076` — one skin per runtime asset · **PASS, with the task's premise corrected**

The task says to confirm *"the split parts render identically to the unsplit source"*. The first run
established that **there is no loadable unsplit source**: `CNA.ModelProcessor` refuses a multi-skin
glTF outright.

```
$ cna-content build build-probe/p1-fixtures/P1TwoSkin.glb -o .../P1TwoSkin.cnb --format cnb
  Process (CNA.ModelProcessor): glTF produced 2 Model documents; set ModelProcessor bool
  parameter 'generateChildAssets' to true to publish the deterministic multi-Model output set.
Built: 0  Skipped: 0  Failed: 1
```

That is *better* than the architecture assumed — a multi-skin source cannot silently reach the
runtime, so the rule enforces itself at build time — but it means the comparison had to be
reformulated. What the probe measures instead is that the **two independent split routes agree
pixel for pixel**, driven from one pose of one shared skeleton evaluated once:

| Route | How | Assets produced |
|---|---|---|
| **Pipeline** | `"generateChildAssets": {"type":"bool","value":true}` in the asset config | `P1TwoSkin.cnb` (primary — the lexicographically first group) and `P1TwoSkin_P1SkinB.cnb` (child), both ordinary `Load<Model>()` names |
| **Offline** | `p1-split-skins.py`, the project-owned splitter | `P1PartA.glb` / `P1PartB.glb`, each single-skin, plus a `.attach.json` record |

```
  [--] offline-split route coverage   2576 px (1287 left, 1289 right)
  [ok] both parts actually drew       1287 / 1289
  [ok] the pipeline split and the offline split are pixel-identical   0 differing pixels
p1-skinsplit: 13/13 checks passed
```

**The pipeline route becomes the default** (`cna-house.md` §21.3 updated): one line of asset config
replaces a project-owned tool, and the child names are ordinary logical content names. The offline
splitter stays as the fallback for sources whose generated child names are unacceptable, and it is
what `HOUSE-00224` implements.

The **attachment record** is what makes the parts reassemble, and it round-trips through
`System::Text::Json` in the probe exactly as the runtime will read it: the shared skeleton root, the
part's attachment bone, and the skin's joint names in blend-index order.

**Finding — `Text.Json` is not in CNA's default sharp-runtime component set.** Including
`System/Text/Json/JsonDocument.hpp` does not compile until the consumer both adds `Text.Json` to
`SHARP_RUNTIME_COMPONENTS` *and* links `SharpRuntime::Text.Json` directly, because CNA does not link
it and so does not propagate its include directories. `cna-house`'s world data, save files and
sidecars are all `System::Text::Json`, so phase 2's CMake (`HOUSE-00121`, `HOUSE-00122`) must carry
both lines.

**Finding — a second `ContentManager` needs the `Game`'s service provider.** `ContentManager(nullptr)`
throws *"ContentManager: no GraphicsDevice is available from the service provider."* at the first
`Load<Model>`; `ContentManager(&game.getServicesProperty())` works. Relevant to `HOUSE-00858`'s
per-pack managers.


### `HOUSE-00078` — `DualTextureEffect` · **PASS**

The albedo is an 8×8 checker of 240/80 and the lightmap an 8×8 gradient `16x + 2y`, both opaque, both
built with `SetData` so no premultiplication policy enters the arithmetic. Every texel is compared
against the formula FNA defines and CNA implements — `color = tex0; color.rgb *= 2; color *= tex1`.

**64/64 texels within 1/255. Worst absolute delta: 1.**

The fixture makes the *second-channel* half of the claim falsifiable: channel 1 carries a
**mirrored** X coordinate, so an implementation that fed `TEXCOORD0` to both samplers would produce
a symmetric image, and the checker guarantees most texels would then be wrong. None were.

The probe also keeps a permanent witness for the `*2` doubling factor — the bug CNA's own Task 383
found and fixed — because it is invisible to any test using saturated 0/1 values: 128 × 128 must
read back as 128, not 64. It reads back as **128**.

**Consequence for `cna-house`:** a lightmap authored at 0.5 grey means "no change", not "half
brightness". The bake in phase 12 targets that midpoint.

### `HOUSE-00079` — multi-pass additive lighting · **PASS**

This is the Tier S lighting mechanism, so both of its assumptions were measured. The fixture is a
quad **tilted in depth**, deliberately: a screen-parallel quad has constant interpolated depth and
would pass a depth-equal test even on hardware whose passes disagree.

| | Result |
|---|---|
| First pass (`Opaque`, depth write) | 3 249 px at exactly 60 |
| Second pass (`Additive`, `CompareFunction::Equal`, no depth write) | 3 249 px at exactly **100** |
| Pixels rejected by the depth-equal test | **0** |
| Three passes, 60 + 40 + 40 | 3 249 px at exactly **140** |

Additive accumulation is exact and depth-equal invariance holds. Tier S's three-light room is sound.

**Finding — a re-bound render target needs `RenderTargetUsage::PreserveContents`.** The default is
`DiscardContents`, so the natural "draw pass 1, unbind, read back, rebind, draw pass 2" sequence
loses the first pass. The probe uses the explicit usage; phase 16 must too.

### `HOUSE-00080` — `AlphaTestEffect` · **PASS**

Measured on a 256×1 alpha ramp — one texel per alpha value — so the cutoff column *is* the
threshold, read off rather than estimated.

| `AlphaFunction` (reference 128) | First surviving alpha | Count |
|---|---|---|
| `Greater` | 129 | 127 |
| `GreaterEqual` | 128 | 128 |
| `Less` | 0 | 128 |
| `Equal` | 128 | 1 |
| `Always` | 0 | 256 |
| `Never` | — | 0 |

Every one is exactly XNA's semantics, to a single alpha value. Two-sided rendering: a back-facing
card is invisible under `CullClockwise`, appears under `CullCounterClockwise`, and `CullNone` draws
it from both sides with identical coverage — the foliage state works.

**Finding — procedurally authored geometry does not inherit the glTF winding convention.** The
first version of this probe wound its quad top-left → top-right → bottom-right, which in normalised
device coordinates (where **+Y is up**) is *clockwise*, and the entire quad vanished under
`CullClockwise`. Every check failed for one fixture reason. `cna-house` generates geometry
procedurally in phases 6, 10, 25 and 27; each generator must be wound counter-clockwise to match
what the imported assets use, and each needs a coverage assertion of its own.

### `HOUSE-00081` — `EnvironmentMapEffect` · **PASS**

The `TextureCube` carries six distinctly coloured faces, so "which face was sampled" is one pixel
read, compared against `reflect(-E, N)` computed in C++.

| Quad orientation | `reflect(-E, N)` | Expected face | Measured |
|---|---|---|---|
| head-on | `(0, 0, 1)` | `+Z` magenta | `(200,10,200)` ✓ |
| +45° about `+Y` | `(1, 0, 0)` | `+X` red | `(200,10,10)` ✓ |
| −45° about `+Y` | `(-1, 0, 0)` | `-X` green | `(10,200,10)` ✓ |

`EnvironmentMapAmount` is a linear blend weight: 0 → `(0,0,0)`, 0.5 → `(100,5,100)`, 1 →
`(200,10,200)`. Fresnel behaves as its definition requires — at factors 1 and 4 the grazing angle is
markedly more reflective than head-on, and at factor 0 (`pow(x,0) == 1`) the weighting is uniform.

`AmbientLightColor` **does** reach this effect (0.5 grey ambient → `(128,128,128)`), even though it
appears in no uniform of EasyGL's environment-map fragment shader — it is folded into the emissive
term before upload. Recorded because the shader source alone would suggest otherwise.

### `HOUSE-00082` — `BasicEffect`, everything at once · **PASS, byte-exact**

Tier S *is* `BasicEffect`, so the probe implements the lighting model in C++ and asserts the pixel,
adding one contribution at a time so a disagreement would localise to a single term.

| Configuration | Measured | Analytic | Δ |
|---|---|---|---|
| ambient + emissive | `(25,25,25)` | `(26,26,26)` | 1 |
| + three directional diffuse terms | `(107,107,87)` | `(107,107,87)` | **0** |
| + three specular terms | `(158,129,95)` | `(158,129,95)` | **0** |
| + fog — **all six terms together** | `(92,90,86)` | `(92,90,86)` | **0** |
| at `FogStart` (no fog yet) | `(158,129,95)` | `(158,129,95)` | **0** |
| past `FogEnd` (pure fog colour) | `(25,51,76)` | `(26,51,77)` | 1 |

The three lights were given different directions *and* different colours precisely so that a model
dropping one of them could not still look plausible. `PreferPerPixelLighting` on and off agree
exactly on a constant-normal surface, as they must.

The model confirmed, in CNA's own terms:

```
lightSum    = ambient + Σ lightDiffuse_i · max(dot(N, -dir_i), 0)
litRGB      = lightSum · DiffuseColor + EmissiveColor
spec_i      = pow(max(dot(normalize(E - dir_i), N), 0) · step(0, dot(N,-dir_i)), SpecularPower)
FragColor   = litRGB;  FragColor.rgb += Σ spec_i · lightSpecular_i · SpecularColor · alpha
FragColor.rgb = mix(FogColor, FragColor.rgb, fogFactor),  fogFactor = 1 - (d - FogStart)/(FogEnd - FogStart)
```

Specular is added **after** the diffuse product and **before** fog, and is scaled by the final
alpha. Phase 12's material mapping and phase 16's light budget can be computed against this.


### `HOUSE-00083` — `SurfaceFormat::Single` 2048² render target · **`BL-09` / `Q-01` SETTLED POSITIVELY**

Four stages, executed separately because each failure would lead somewhere different, and all four
succeeded:

| Stage | Result |
|---|---|
| Create 2048×2048 `Single` + `Depth24` | ✓ — reports its own format and depth format back correctly |
| Bind, clear, draw depth-varying geometry | ✓ |
| `GetData(float*)` over all 4 194 304 texels | ✓ — 3 396 649 drawn texels, **every one exactly `0.625`**, 797 655 cleared texels exactly `0`, **0 anything else** |
| Bind it as an effect texture and sample it | ✓ — `0.625` arrives in the shader as `159/255` |

**Tier E gets a real float shadow map.** The RGBA8 depth-packing fallback the architecture kept in
reserve is not needed, and the extra pack/unpack in every shadow lookup goes away. The fallback path
was nonetheless exercised in the same probe, so it is known to work whichever way this went.

The value `0.625` was chosen because it is exactly representable in binary: a round trip that
changed it at all would be a real difference, not a rounding artefact.

### `HOUSE-00084` — mip chains and MSAA · **PASS**

A 64×64 `Color` target with `mipMap = true`, at MSAA 0 and MSAA 4:

| | MSAA 0 | MSAA 4 |
|---|---|---|
| `LevelCount` | 7 (64 → 1, the full chain) | 7 |
| `MultiSampleCount` | 0 | **4** |
| Level 0 after drawing `(128,64,191)` | `(128,64,191)` | `(128,64,191)` |
| Sampled back through `BasicEffect` after unbinding | `(128,64,191)` | `(128,64,191)` |

MSAA 4 is genuinely allocated (the target reports it) and resolves to the exact drawn colour, which
is the step an implicit-resolve bug would break.

### `HOUSE-00085` — `BL-02` · **the premise is FALSE; stencilling works**

The probe was built so that a working stencil and an inert one produce *different images*, and so
that neither verdict could be inferred from an absent exception:

1. clear colour, depth and stencil to 0 on a `Depth24Stencil8` target;
2. pass 1 — `CompareFunction::Always` + `StencilOperation::Replace`, `ReferenceStencil = 1`, drawn
   over the **left half only**, writing black;
3. pass 2 — `CompareFunction::Equal`, `ReferenceStencil = 1`, drawn over the **whole** quad in white.

If `ReferenceStencil` were inert, pass 2 would cover everything.

```
  [--] stencil mask result   left half 2048/2048 lit, right half 0/2048 lit
  [--] BL-02 verdict         STENCIL WORKS -- the mask confined the second pass
```

`BL-02` is corrected. Anything in `cna-house` that was designed around an inert stencil — mirror
and window-portal masking in particular — may use the stencil buffer after all.

### `HOUSE-00086` — `BL-03`, MRT attachment 1 · **confirmed, with its scope stated**

`SetRenderTargets` with two `Color` targets bound at once succeeded. After one `BasicEffect` draw:
attachment 0 held `(255,128,64)`, attachment 1 held `(0,0,0)`.

**What this does and does not show.** It shows the renderer does not broadcast a single-output draw
to every attachment, and that binding two targets is not itself an error. It does **not** show what
a *compiled* `.fx` declaring two outputs would do — that is a Tier E question, and `BL-03` as it
affects Tier E is only answerable once `HOUSE-00087` establishes whether compiled effects work at
all. Recorded as such rather than overclaimed. Tier S writes one attachment and is unaffected.

### `HOUSE-00110` — the `SurfaceFormat` survey · **complete, and the two lists barely overlap**

Every format was *used*, not merely created: each render-target candidate was bound, cleared and
drawn into, because a driver can accept a format at creation and fail on first use.

| | Supported |
|---|---|
| **As a `Texture2D`** | `Color`, `Bgr565`, `Bgra5551`, `Bgra4444`, **`Dxt1`, `Dxt3`, `Dxt5`**, `NormalizedByte2`, `NormalizedByte4` |
| **As a `RenderTarget2D`** | `Color`, **`Single`, `Vector2`, `Vector4`, `HalfSingle`, `HalfVector2`, `HalfVector4`** |

**Only `Color` is in both lists.** In particular `SurfaceFormat::Single` is a perfectly good render
target — `HOUSE-00083` rendered, read back and sampled one — but a `Texture2D` of that format
**cannot be created by the application at all**. Any float data `cna-house` wants on the GPU has to
arrive as the output of a render pass, never as an uploaded texture.

Surveyed at **both** graphics profiles, because `GraphicsDeviceManager` defaults to
`GraphicsProfile::Reach` and XNA 4.0's Reach profile forbids float formats outright — a survey run
only at the default would blame the driver for an XNA rule. The **lists are identical** at `Reach`
and `HiDef` on this renderer; only the refusal *message* changes (`"not available on
GraphicsProfile"` versus `"not implemented by the selected renderer"`). So the limit here is
EasyGL's, and switching `cna-house` to `HiDef` would buy nothing on this platform.

### `HOUSE-00111` — DXT · **it works, but only through `.xnb`**

| Source | Container | File size | Runtime `SurfaceFormat` |
|---|---|---|---|
| 64×64 RGBA, `textureFormat: Dxt1` | `.cnb` | 16 752 B | **`Color`** |
| 64×64 RGBA, `textureFormat: Dxt5` | `.cnb` | 16 752 B | **`Color`** |
| 64×64 RGBA, `textureFormat: Dxt1` | `.xnb` | **2 235 B** | **`Dxt1`** |
| 64×64 RGBA, `textureFormat: Dxt5` | `.xnb` | **4 283 B** | **`Dxt5`** |
| 64×64 RGBA, no parameter | `.xnb` | 16 571 B | `Color` |

**CNB texture schema 1 is frozen to Rgba8.** The pipeline says so itself rather than failing:

```
warning (CNA.TextureProcessor): textureFormat Dxt1 has no representation in CNB texture schema 1,
which stores Rgba8 only; this .cnb keeps the uncompressed pixels. Build with --format xnb to get
the compressed texture.
```

That warning is easy to miss in a large build and the resulting asset is silently 8× larger, so it
is worth a build-gate of our own.

**Consequence for the Linux content profile (`cna-house.md` §27.2):** block compression is real —
8× for `Dxt1`, 4× for `Dxt5`, and the blocks stay compressed all the way to the GPU — but **texture
assets that want it must be built as `.xnb`, not `.cnb`.** Combined with `HOUSE-00064`'s finding
that `.xnb` wins the resolution order, a deliberately mixed tree (models and audio as `.cnb`,
textures as `.xnb`) is coherent; an accidental one is a trap.


### `HOUSE-00087` / `HOUSE-00088` / `HOUSE-00089` — compiled effects · **PASS. Tier E is viable.**

All four of `HOUSE-00087`'s acceptance points hold, and the two dependent tasks with them: 17/17
checks in `p1-fxload`.

**The build.** `fxc.exe` from the June 2010 DirectX SDK, run under Wine, via `cna-content`:

```
$ cna-content build build-probe/p1-fx/P1Effect.fx -o .../P1Effect.xnb --format xnb \
      --fx-compiler <fxc.exe> --fx-compiler-launcher tools/effects/fxc-wine.sh
[BUILD] P1Effect -> .../P1Effect.xnb (3424 bytes;
        CNA.EffectSourceImporter -> CNA.EffectSourceProcessor -> CNA.XnbEffectWriter)
```

**Finding — the documented `--fx-compiler-launcher wine` does not work on its own, and the fix is
ours.** `cna-content` builds the `fxc` command line with ordinary Unix absolute paths, and `fxc` is
a Windows tool that introduces *options* with `/`:

```
error: Unknown or invalid option '/tmp/cna-fx-0-140733683835328/effect.fxb', use /? for help
```

Neither side is wrong; two conventions collide, and `--fx-compiler-launcher` is precisely the seam
for it. `tools/effects/fxc-wine.sh` is that launcher: it translates any argument naming a path that
**exists**, and any argument following a path-taking option, through `winepath -w`, and passes
everything else through. (Testing the *parent* directory instead of the argument would translate
`/T` as well, because `dirname /T` is `/` — a mistake the script's first version made and its
comment now prevents.) It is offline tooling, so the XNA-only rule does not reach it; a machine with
no Wine simply builds no Tier E, which is what ADR-0003 already requires.

**The runtime.**

| Measurement | Result |
|---|---|
| Techniques discovered | 2 — `Tint`, `Textured` (plus `MultiTarget`, added later) |
| Parameters discovered | 3 — `WorldViewProj`, `TintColor`, `BaseTexture` |
| `technique["Tint"]` selected by name, `TintColor = (1, 0.5, 0.25, 1)` | `(255,128,64)` — the parameter reaches the shader **exactly** |
| Same technique, `TintColor = (0.25, 0.75, 1, 1)` | `(64,191,255)` — a parameter change changes the output |
| `technique["Textured"]`, same tint, 0.5-grey texture | `(128,64,32)` — each channel halved, so the technique really switched |
| Switched back to `Tint` within the same frame | `(255,128,64)` — bit-identical to the first draw |
| `SpriteBatch::Begin(..., effect)` (`HOUSE-00089`) | `(128,128,128)` — the sprite is drawn through the custom effect |

**Finding — `Load<Effect>` does not compile.** `Effect` is neither copyable nor
default-constructible and its reader is registered for `std::shared_ptr<Effect>`, so the call is
`content.Load<std::shared_ptr<Effect>>(name)`. Worth stating because `Load<Model>` *does* return by
value, so the two are not consistent and neither is guessable.

**Finding — `BL-03` is false for compiled effects** (`HOUSE-00086` reopened and closed). With a
stock effect, which declares one output, attachment 1 stays black — the renderer does not broadcast.
With a technique declaring `COLOR0` and `COLOR1` carrying deliberately different values, attachment 0
received `(255,128,64)` and attachment 1 received its **own** `(32,223,96)`, matching the authored
`(0.125, 0.875, 0.375)` to within a rounding step. MRT works. Tier E may use it.


### `HOUSE-00090` / `HOUSE-00092` / `HOUSE-00093` / `HOUSE-00094` / `HOUSE-00106` / `HOUSE-00107` — the performance tranche · **PASS**

**Method, stated once because every number below depends on it.** Release (`-O3 -DNDEBUG`),
`OPENGLES3`/EasyGL, AMD Radeon 780M (radeonsi, phoenix), Mesa 25.0.7, into a 512×512
`RenderTarget2D`. **3 warm-up rounds are discarded**, then **21 samples** are taken and the
**median** reported — an odd count, so the median is an actual sample rather than an average of two.

CPU submission and GPU completion are reported **separately**, because a loop that only fills a
command buffer says nothing about a frame. Every "to completion" figure ends with a **one-texel**
`GetData` on the render target, which forces the GPU to finish. One texel and not the whole surface:
a full 512² readback would cost more than the work being measured. (The whole-surface `GetData`
overload correctly refuses a short buffer — *"elementCount is less than the number of pixels in the
requested region"* — so the rect overload is the right tool, not a workaround.)

| # | Measurement | CPU | To GPU completion |
|---|---|---|---|
| `HOUSE-00092` | 2 000 dynamic quads/frame, `SetDataOptions::Discard` | **0.055 ms** | **0.171 ms** |
| `HOUSE-00106` | 1 000 × `EffectPass::Apply()` alone | **0.184 ms** (**0.184 µs each**) | — |
| `HOUSE-00106` | 1 000 × (`Apply` + `DrawIndexedPrimitives`) | **8.15 ms** (**8.15 µs per draw**) | **13.44 ms** |
| `HOUSE-00093` | 200 instances, one `DrawInstancedPrimitives` | — | **0.156 ms** |
| `HOUSE-00093` | the same 200 as separate draws | — | **2.144 ms** |
| `HOUSE-00107` | 4 MiB `Texture2D::SetData` (1024²) | 8.57 ms | **9.30 ms** → **430 MiB/s** |
| `HOUSE-00107` | 1 MiB `Texture2D::SetData` (512²) | — | **2.47 ms** → **405 MiB/s** |

**What these numbers mean for the design, in order of how much they change it.**

1. **The draw call is the budget, and `Apply()` is not.** `EffectPass::Apply()` costs 0.184 µs —
   effectively free, and 44× cheaper than the draw that follows it. A `DrawIndexedPrimitives` costs
   **8.15 µs of CPU**, so **1 000 draw calls is 8.15 ms of CPU submission alone** — half a 60 Hz
   frame before any game logic runs. The room/portal design's job is therefore to reduce *draw
   calls*, not to reduce state changes: batching by material is worth far less than not submitting
   the room at all. A practical ceiling of **300–400 draws per frame** leaves room for everything
   else.
2. **Instancing is worth 13.7×** and should be the vegetation and neighbourhood path, as
   `HOUSE-00093` hoped. **Scope:** `BasicEffect` has no per-instance input, so this measures the
   *draw path* — that `DrawInstancedPrimitives` works, consumes a second stream at instance
   frequency 1, and is dramatically cheaper. A stock effect cannot actually *read* the per-instance
   offset; consuming it needs a Tier E `.fx`, which `HOUSE-00087` has now shown to be available.
3. **A 4 MiB per-frame texture promotion does not fit.** At 9.3 ms it is more than half a 60 Hz
   frame. The 1 MiB measurement shows the cost is **linear and bandwidth-bound** (430 vs 405 MiB/s),
   not a fixed per-call overhead — so the fix is simply to promote **≈1 MiB per frame** (2.47 ms) and
   spread a large texture over four frames. The streaming design must split promotions; it cannot
   treat 4 MiB as one atomic step.
4. **Dynamic geometry is nearly free.** 2 000 quads streamed with `Discard` cost 0.171 ms to
   completion, so a particle budget in the low thousands is not the constraint anyone expected it to
   be.
5. **`BL-07` is real.** `OcclusionQuery::PixelCount` returned **1** for a quad covering an analytic
   **16 384** pixels, and **0** when fully occluded. It is a boolean on this driver, exactly as §5
   claims. Any coverage ratio computed from it would be `1/area`. The N×N grid approximation stays
   in the design.
6. **32-bit indices are real.** A 70 000-vertex buffer with a triangle addressed by indices needing
   17 bits drew 106 261 lit pixels against an analytic ~106 000 — so the indices were not truncated
   to 16 bits, which is the failure a smaller fixture could not have detected.

**Finding — `Color` is not trivially copyable**, so a custom vertex struct containing one cannot go
through the `SetData<T>`/`GetData<T>` templates; the `static_assert` fires. Custom vertex layouts
store the packed `std::uint32_t` that `VertexElementFormat::Color` reads anyway.


### `HOUSE-00095` / `HOUSE-00096` / `HOUSE-00097` — 3-D audio · **PASS, after the observable was corrected**

The first version of this probe read `Volume`, `Pan` and `Pitch` back after `Apply3D` and reported a
flat curve. That was a **probe bug with a real finding inside it**:

> **`Apply3D` does not touch the public `Volume`, `Pan` or `Pitch` properties.** It stores
> attenuation, pan and Doppler in private state (`attenuation_`, `spatialPan_`, `dopplerFactor_`)
> and composes them with the caller's values only when writing the mixer track. The properties keep
> returning whatever the caller last set — measured across a 20 m sweep and an ±10 m pan sweep, all
> readings unchanged.

So a game **cannot read back what CNA applied**, and `cna-house` — which owns room/portal occlusion
(§32) — must model the same curve itself rather than query it.

What the probe measures, and what it reads, are kept strictly apart:

**Measured.** The public properties are unchanged (above). `Apply3D` is *not* inert: an instance put
into pan mode and then played **refuses** a later `Apply3D` — *"Apply3D cannot be called on a playing
instance that is not using 3D audio."* — while an instance aimed in 3D before playing accepts it
while playing. An implementation where `Apply3D` did nothing could not produce that distinction.

**Read from `modules/audio/src/Xna/SoundEffectInstance.cpp`, and labelled as read.** The curve
`cna-house` must calibrate against:

```
normalized  = distance / SoundEffect::DistanceScale
attenuation = normalized >= 1 ? clamp(1 / normalized, 0, 1) : 1
pan         = distance > 0 ? clamp(rightDisplacement / distance, -1, 1) : 0
```

At the default `DistanceScale = 1`: `0 m → 1.0000, 1 m → 1.0000, 2 m → 0.5000, 3 m → 0.3333,
5 m → 0.2000, 8 m → 0.1250, 10 m → 0.1000, 15 m → 0.0667, 20 m → 0.0500`.

In words: **full volume inside `DistanceScale`, then inverse *distance* beyond it** — not
inverse-square, and not a continuous falloff from zero. Pan is the listener-relative rightward
displacement over distance, clamped — a linear approximation with **no HRTF and no cone or
orientation term**. `rightDisplacement` is projected onto the listener's own `Forward × Up` axis, so
a rotated listener is handled correctly.

**`BL-11` holds, for a stronger reason than the blocker assumed** (`HOUSE-00096`). When
`emitter.DopplerScale * SoundEffect::DopplerScale` is zero, the factor is set to exactly `1.0f`
*without evaluating the Doppler math at all* — there is no rounding path by which a pitch change
could appear. Zero velocities alone would also give 1.0.

**`HOUSE-00097` — the voice ceiling is ours to impose.** 512 of 512 looping instances reported
`Playing`; **CNA refused nothing** up to the probe's own ceiling. There is no hardware limit
discoverable through the XNA surface, so the 32-voice budget is a design decision `cna-house` must
enforce itself — CNA will not tell us when the mixer has been overcommitted.

### `HOUSE-00102` / `HOUSE-00103` — storage and JSON · **PASS, with §5 corrected**

Write, read, list and delete all work through `StorageDevice`/`StorageContainer`, and a 276-byte
save read back byte-identical.

**Finding — the save path is not what §5 says.** `cna-house.md` §5 records the Linux root as
`$XDG_DATA_HOME/<app>` or `~/.local/share/<app>`, with `<app>` implied to be the game. It is not:
`StorageDevice.cpp:75` is `appName_.empty() ? "game" : appName_`, and the **only** setter is
`SetAppNameEXT` — a `CNAEXT` identifier ADR-0001 forbids. The measured root is:

```
/home/<user>/.local/share/game/P1Probe
```

So an XNA-only game's saves land under a directory literally called `game`, shared with every other
CNA application on the machine. **The fix needs no extension:** the *container* name is ours through
plain XNA, so `BeginOpenContainer("CnaHouse")` gives `.../game/CnaHouse/`, which is unambiguous. The
probe verified the container name really is a directory level by finding its own. `HOUSE-00113`
corrects §5 and ADR-0008.

**`HOUSE-00103` — float round-trip is bit-exact.** The awkward values were chosen to break a naive
serialiser — `1/3`, `0.1` (repeating in binary), `1.1754944e-38`, `3.4028235e+38`, `-0.0`, `2.0` —
and compared by **bit pattern**, not epsilon, because a save that drifts one ulp per cycle still
corrupts a long-running house, only slowly. **6/6 returned bit-identical.** Nested objects, string
arrays in order, and integers-staying-integers all hold.

### `HOUSE-00104` — the math the room/portal system rests on · **PASS, 24/24**

Every expectation is a number computed by hand, in a frame chosen so it is exact: the frustum is
**orthographic and axis-aligned**, so its eight corners are exactly the corners of a box with the
projection's own extents and there is no perspective divide to force a tolerance.

| Call | Expected | Measured |
|---|---|---|
| `BoundingFrustum::GetCorners` extents | `x[-2,2] y[-1,1] z[-11,-1]` | exact |
| `Ray::Intersects(BoundingBox)`, aimed from 5 units at a face at z = 1 | `4.0` | `4.000000` |
| `Ray::Intersects(BoundingSphere)`, r = 2 | `3.0` | `3.000000` |
| `Ray::Intersects(Plane)`, from y = 3 to y = 0 | `3.0` | `3.000000` |
| `Ray` starting **inside** the box | `0.0` | `0.000000` |
| `Ray` pointing **away** (its line would hit) | miss | miss |
| `BoundingBox::CreateFromPoints` on an unordered list with duplicates | `min(-2,-1,-4) max(3,5,9)` | exact |
| Frustum vs box: inside / straddling / behind / beyond far | `Contains` / `Intersects` / `Disjoint` / `Disjoint` | all four |
| A sphere **tangent** to a side plane | intersects | intersects |
| A box **face-touching** another | intersects | intersects |

The last two are the cases an epsilon error flips, and the "ray pointing away" case is the one a
naive line-intersection implementation gets wrong. Phase 9 can be built on these.

### `HOUSE-00100` / `HOUSE-00101` — input · **`HOUSE-00101` PASS; `HOUSE-00100` INCONCLUSIVE, and left open**

`HOUSE-00101` is settled. `Keyboard::GetState` answers and `IsKeyDown` agrees with
`GetPressedKeys`. All four `GamePad` slots report **not connected**, and `GetCapabilities` agrees
with `GetState` on every one — the consistency is the point, since a game that trusted only one of
them would be wrong half the time. `TouchPanel::GetCapabilities` and `GetState` can be called on
desktop **without throwing**, report `IsConnected = false` and **0 touches**, so a game may poll
them unconditionally.

`HOUSE-00100` ran its full **10 000 frames** — deliberately not shortened, since a fractional
per-frame error is invisible in 100 frames and ruins a camera in 10 000 — and the result is
**inconclusive for the drift criterion**, which is recorded rather than rounded to a pass:

* **Measured:** `Mouse::SetPosition(400, 300)` **is** reflected by a `GetState` in the same frame.
* **Measured:** every one of the 10 000 subsequent frames read `(0, 0)`.
* **Cause, from `modules/input/src/Xna/Mouse.cpp:117`:** `GetState` returns a *snapshot* the platform
  layer maintains from SDL mouse-motion events. On an unattended desktop the pointer never enters or
  moves over the probe window, no motion event arrives, and the snapshot never leaves its initial
  value. **No delta measured here is a delta**, so no drift figure from this run means anything.

Two facts are established regardless, and both change how the camera is written:

1. **`Game::IsActive` is not a proxy for "the mouse is usable".** It was `true` on all 9 999 frames
   while the snapshot never advanced. A camera gating only on `IsActive` would consume garbage.
2. **`GetState` is event-driven, not a live cursor query.** The camera must seed its previous
   position from a real motion event and must never assume the cursor starts centred.

`HOUSE-00100` stays **unchecked** in `plan.md`. It needs a session with the pointer actually over the
window, and `HOUSE-00115` lists it as such.


### `HOUSE-00098` / `HOUSE-00099` — video · **PASS**

The clip is generated locally by `ffmpeg` from `lavfi` sources: 64×64, 2 seconds, 10 fps, colour
changing every half second — red, green, blue, white — over a 440 Hz sine. **Nothing is downloaded
and no third party's media is involved**, so there is no licensing question and the expected pixel
at any timestamp is known by construction.

Frame advance is *measured*, not assumed. The texture is read back sixteen times across the clip and
each centre texel classified against the four authored colours:

```
  [--] GetTexture calls    16, of which 0 returned null
  [--] sampled frame colours   R R R  G G G G  B B B B B  W W W W
  [--] PlayPosition at each sample
       0.12 0.24 0.36 0.48 0.60 0.72 0.84 0.96 1.08 1.21 1.33 1.45 1.57 1.69 1.81 1.93
```

Exactly the authored sequence, at the authored timestamps. A player handing back the first frame
forever would pass a test that only asked "did a texture come back"; this one it could not pass. The
audio mixer opened a real device for the clip (`44100 Hz stereo`), so the soundtrack is live.

**`HOUSE-00099` / `BL-05` — settled, with one detail the blocker did not anticipate.** Built in
`build-consumer/` with `CNA_ENABLE_VIDEO=OFF`:

* `VideoPlayer` still **constructs** and reports `Stopped`;
* **`Load<Video>` still succeeds** — content loading is not gated, only playback is;
* `Play()` throws *"Video playback is unavailable because CNA was built without the optional FFmpeg
  video backend. Configure with `-DCNA_ENABLE_VIDEO=ON`, or use AUTO with all required FFmpeg
  development packages installed."* — a refusal that names its own fix;
* the player is still a usable object afterwards and `Stop()` is still callable.

The linking half is the half that matters: phase 21's television can be a feature that is absent on a
platform without the rest of the house failing to build.

**Finding — a third `Load<T>` shape.** `Load<Video>` returns **by value**, like `Model` and unlike
`Effect` (whose reader is registered for `shared_ptr<Effect>`). The three are not consistent and none
is guessable, but CNA says so precisely when asked wrongly: *"'P1Clip.cnb' holds a Video asset, which
is not the type requested for 'P1Clip'."*

### `HOUSE-00105` — the `HEADLESS` renderer · **PASS**

The acceptance criterion is taken literally: **600 frames with no display server**. The probe asserts
from *inside the process* that `DISPLAY` and `WAYLAND_DISPLAY` are both unset, because a test that
silently ran against a live X server would prove nothing about CI and is the easiest possible thing
to get wrong.

```
$ env -u DISPLAY -u WAYLAND_DISPLAY ./build-consumer/p1-headless
[INFO][RENDER] CNA: graphics renderer: HEADLESS
  [ok] no display server is reachable from this process
  [ok] Initialize ran
  [ok] Update ran for 600 frames        600
  [--] Draw calls                       599
  [ok] GameTime advances                0.0116 s across 600 updates
p1-headless: 5/5 checks passed        # exit 0
```

`Draw` runs too, so the whole frame loop is exercised rather than just `Update`. Phase 44's automated
tests and phase 2's CI have a foundation.

### `HOUSE-00108` — resource rebuild after a device reset · **PASS, through a different door**

The task named EasyGL's `DebugSimulateContextLoss`. That call is declared in
`CNA/Internal/Renderers/Common/IGraphicsRenderer.hpp` — a `CNA/` include of a `CNA::Internal::`
type — so **ADR-0001 forbids it twice over** and `cna-house` can never call it, in a probe or
anywhere else. Reaching for it to "just measure once" would have been exactly the erosion the rule
exists to prevent.

The question the task was really for was asked through the XNA 4.0 surface instead —
`GraphicsDevice::Reset()` with the `DeviceLost` / `DeviceResetting` / `DeviceReset` events:

| | Result |
|---|---|
| `GraphicsDevice::Reset()` | callable, no exception |
| Events raised | `DeviceResetting` ×1, `DeviceReset` ×1 (**`DeviceLost` ×0**) |
| Texture, vertex and index buffers rebuilt from the same CPU state | all three |
| The rebuilt scene, sampled | `(128,128,128)` — **identical** to before the reset |

So the property the Web port needs — resources reconstructible from CPU-side state, with an event to
say when — holds on Linux and is testable there. Note that **`DeviceLost` does not fire**: a game
that hung its rebuild on that event alone would never rebuild. `DeviceReset` is the one to use.

### `HOUSE-00109` — anisotropic filtering · **available and effective**

| | Linear (trilinear) | Anisotropic |
|---|---|---|
| Pixels differing between the two images | — | **3 056** of 65 536 |
| Mean per-row far-field contrast | 6.04 | **8.90** (**1.47×**) |
| Best single row | 0.00 (fully blurred out) | **47.68** (fully resolved) |

**Verdict: available and effective; the `linux` profile enables it.** This is measured once here and
written into the profile — it is what `HOUSE-00916` keys off, and it is never a runtime query.

Two fixture corrections were needed before the measurement meant anything, and both are worth
recording because they are the standard ways to fake this result:

1. The first fixture used a **4-texel checker repeated 60×**. That is minified so hard that *both*
   modes collapse to flat grey across the whole floor, and the probe dutifully reported a contrast
   of 0.00 for each — a measurement of the fixture, not the driver. A realistic 32-texel tile at 16
   repeats is what discriminates.
2. The first summary statistic counted **rows improved versus rows worsened**. With a checkerboard
   that oscillates with wherever a tile boundary happens to fall, and it swung 21 against 28 even
   while the image as a whole gained 47 % more contrast. The aggregate over every lit row is the
   statistic that is not an artefact of tile phase.


### `HOUSE-00091` — the same measurements under `OPENGL33` · **the grid approximation is validated**

`build-consumer/` was reconfigured from the same source directory with
`-DCNA_GRAPHICS_RENDERER=OPENGL33` and the whole performance probe re-run against
`OpenGL 4.6 (Core Profile) Mesa 25.0.7`. (The probe's own "configuration" line is a compile-time
constant and still says `OPENGLES3`; the renderer actually in use is on CNA's `[RENDER]` log line
immediately above it. Recorded rather than trusted.)

**The decisive result, and the reason this task existed:**

| `OcclusionQuery::PixelCount` for a quad of analytic area 16 384 | |
|---|---|
| `OPENGLES3` | **1** |
| `OPENGL33` | **16 384** — exact |

Same probe, same fixture, same driver, same GPU. So `BL-07`'s boolean degradation is a property of
the **ES profile's query target**, not of CNA and not of this hardware — exactly as `cna-house.md`
§5 claims — and the N×N grid approximation phase 9 is designed around is validated against a
renderer that returns a true count. That is what the task asked for and it is now answered.

**The rest of the tranche, for comparison. These differences matter to `HOUSE-00115`:**

| Measurement | `OPENGLES3` | `OPENGL33` |
|---|---|---|
| `OcclusionQuery` on a 16 384-pixel quad | 1 (boolean) | **16 384** (real tally) |
| `EffectPass::Apply()` | 0.184 µs | 0.202 µs |
| CPU per `DrawIndexedPrimitives` | **8.15 µs** | **12.23 µs** |
| 1 000 small draws, to GPU completion | 13.44 ms | 12.6–23.8 ms (run to run) |
| 2 000 dynamic quads, to completion | **0.171 ms** | 0.316–0.570 ms |
| 200 instances vs 200 draws | 13.7× | 12.8× |
| 4 MiB `Texture2D::SetData` | 9.30 ms, 430 MiB/s | 9.14 ms, 438 MiB/s |

Two things follow. **`OPENGLES3` submits draws about 1.5× more cheaply** than `OPENGL33` on this
driver, which is the opposite of what one might expect and reinforces ADR-0002's renderer choice.
And **the draw-call budget, the instancing win and the upload bandwidth are all renderer-independent
to within run-to-run noise** — so those three numbers can be treated as properties of the machine
rather than of the renderer, while the occlusion verdict emphatically cannot.


---

## `HOUSE-00115` — if you change renderer, re-run these

Every verdict in this file is a verdict about **`OPENGLES3` / EasyGL on Mesa 25.0.7 / Radeon 780M**.
Most would survive a renderer change; some emphatically would not. This is the list, ordered by how
badly a wrong assumption would hurt.

| Re-run | Why | Probe |
|---|---|---|
| **`OcclusionQuery` `PixelCount`** | **Known already to differ**: boolean under `OPENGLES3`, an exact 16 384 under `OPENGL33`, same driver and GPU. Phase 9's grid approximation exists *because of* this row. | `p1-perf` |
| **The `SurfaceFormat` survey, both lists** | Which formats are textures and which are render targets is a renderer property. `Single` being a render target but **not** a `Texture2D` is what forces float data to arrive from a render pass. | `p1-formats` |
| **`SurfaceFormat::Single` 2048² end to end** | Tier E's shadow map depends on it. The RGBA8 packing fallback exists for the case where it fails. | `p1-rtsingle` |
| **MRT with a compiled two-output effect** | `BL-03` was true for stock effects and false for compiled ones on this renderer; another renderer may differ in either direction. | `p1-fxload` |
| **Stencil** | `BL-02` claimed it was inert on three renderers and it is not inert here. Do not assume the correction generalises. | `p1-rtcaps` |
| **Compiled effects at all** | `CNA_EASYGL_COMPILED_EFFECTS` is an EasyGL option. A renderer without it makes Tier E unbuildable, which ADR-0003 already allows for. | `p1-fxload` |
| **MSAA and mip chains on a render target** | Resolve behaviour is renderer-specific. | `p1-rtcaps` |
| **Draw-call cost** | Already measured to differ by 1.5× between `OPENGLES3` (8.15 µs) and `OPENGL33` (12.23 µs). The 300–400-draw budget is a per-renderer number. | `p1-perf` |
| **Anisotropic filtering** | A driver/profile property; the `linux` content profile keys off it. | `p1-aniso` |
| **DXT staying compressed** | Block-format support is per renderer, and the `.xnb`-only route is a *container* fact that would survive, while the GPU support is not. | `p1-formats` |

**These do not need re-running on a renderer change**, because they are properties of the content
pipeline, the math library, or the host rather than of the renderer:

`HOUSE-00064`–`HOUSE-00069` (content resolution, textures, fonts, audio formats),
`HOUSE-00070`–`HOUSE-00074`, `HOUSE-00076` (glTF import, hierarchy, skin binding — the *import* is
renderer-independent even though the *draw* is not), `HOUSE-00102`–`HOUSE-00104` (storage, JSON,
math), `HOUSE-00107` (upload bandwidth — measured within noise on both renderers).

**Still owed, on any renderer:** `HOUSE-00100`'s mouse-drift measurement, which needs a session with
the pointer actually over the window.

---

## `HOUSE-00119` — CNA facts that must never be assumed again

The five that most changed what this project will do. Each was believed otherwise — by
`cna-house.md`, by CNA's own documentation, or by the probe's first draft — until it was measured.

**1. Blend indices are skin-local, not bone indices.** The compiled `Model` does not contain the
mapping from a vertex's blend index to a bone. It contains a *number*, `0..N-1` in the order the
glTF `skin.joints` array declared, and CNA inserts a synthetic `Root` bone that shifts every bone
index away from it. The joint-name list in the `.chanim` sidecar **is** the binding; without it the
skeleton cannot be driven at all. Measured 0/10 vertices under the bone hypothesis and 10/10 under
the skin-local one — the fixture was built so that exactly one of the two could be true.

**2. `Apply3D` writes nothing a game can read.** Attenuation, pan and Doppler go into private state
and reach only the mixer track; `Volume`, `Pan` and `Pitch` keep returning what the caller last set.
Every room-audio design that expected to *read back* CNA's spatial gain and then modify it has to be
rewritten to *model* the same curve instead. And the curve is **inverse distance beyond
`DistanceScale`, full volume inside it** — not the inverse-square that a physics-minded designer
would assume, and not a falloff that starts at zero distance.

**3. Half the blockers were wrong, in both directions.** `BL-02` (stencil inert), `BL-03` (MRT
attachment 1 black) and `BL-06` (24-bit PCM rejected) were all **false**. `BL-09` was *pessimistic*
— the float render target works perfectly and Tier E needs no packing fallback. `BL-07` was
**right**, and `HOUSE-00091` showed exactly *why* by reproducing it on one renderer and not the
other. A blocker inherited from a feature matrix is a hypothesis, not a fact, and the cost of
believing one is a design that routes around a limitation that does not exist.

**4. The XNA surface is not uniformly XNA-shaped in C++.** Collection iterators are `CNAEXT`, so
every `foreach` becomes an index loop. `Load<T>` returns by value for `Model` and `Video` but a
`shared_ptr` for `Effect`. `Matrix::getIdentityProperty()` is a property while `Vector3::Up` is a
plain static. `Color` is not trivially copyable, so it cannot go in a vertex struct. `Copy*Bone
TransformsTo` requires a pre-sized destination. None of these is guessable from the C# API, and each
one is a compile error or a silent misread waiting in code that assumed otherwise — the
`VertexPositionNormalTexture` assumption read vertex 0 correctly and every later vertex from the
wrong offset.

**5. The measurement is only as good as the fixture, and a bad fixture reports a clean pass.** Four
probes in this phase produced confident, wrong answers before their fixtures were corrected: a
**closed box** cannot measure winding (both cull modes cover the same silhouette); a **4-texel
checker at 60× repeat** collapses under trilinear *and* anisotropic filtering, so both measure 0.00;
a **screen-parallel quad** passes a depth-equal test even where the two passes disagree; and reading
**`Volume` after `Apply3D`** measures a property that call never touches. In every case the probe
"passed" or produced a plausible number first. The habit that caught them was insisting on an
*analytic* expectation — a number computed independently, in a frame chosen so it is exact — rather
than a plausible-looking one.


---

## Probe hygiene

Every probe in this tranche was built in `build-probe/` — in this repository, never `/tmp`, never
the session scratchpad, never a per-ticket directory — with `CCACHE_DIR="$HOME/.cache/ccache"` and
`CCACHE_BASEDIR=/rv`, and each source carried a `p1-` (or `p0-`) file-name prefix:

| Probe | Task | Fate |
|---|---|---|
| `p0-result-check.cpp` | `HOUSE-00042` | Deleted |
| `p1-hello.cpp` | `HOUSE-00062`, `HOUSE-00063` | Deleted |
| `p1-strict-negative.cpp` | `HOUSE-00063` | Deleted |
| `p1-content.cpp` (target `p1-contentprobe`) | `HOUSE-00064`–`HOUSE-00069` | Deleted |
| `p1-content-src/` fixtures + compiled `.cnb`/`.xnb` | `HOUSE-00064`–`HOUSE-00069` | Deleted |

`build-probe/` was removed entirely once the findings above were written down — 135 MB, including
the `CNA_BUILD` tree. Nothing from it is tracked; `.gitignore`'s `/build*/` covers the directory.
No probe binary, object or fixture was ever staged.

**The next tranche re-configures and rebuilds CNA at `CNA_CNAEXT=OFF`.** That is deliberate and
cheap: every translation unit from this session is now in the shared ccache, so the rebuild is
served from cache rather than recompiled. The CMake configuration to recreate is recorded in
[Configuration under test](#configuration-under-test); the probe `CMakeLists.txt` set
`CNA_GRAPHICS_RENDERER`, `CNA_PLATFORM`, `CNA_AUDIO_PLATFORM`, `CNA_CNAEXT`,
`CNA_EASYGL_COMPILED_EFFECTS`, `CNA_ENABLE_VIDEO`, `CNA_ENABLE_NET`, the three `*_BUILD_TESTS`
options, `CNA_SHARP_RUNTIME_ROOT` and `SHARP_RUNTIME_COMPONENTS`, then
`add_subdirectory(../../cnanext CNA_BUILD)` and linked each probe against the `CNA` target.

`cna-content` was **not** rebuilt: the host tool already present in CNA's own `build/` was reused,
which is correct under openeggbert build rule 2 and legitimate under `CLAUDE.md` §1, since offline
tooling is not runtime and is unconstrained.

**No CNA or sharp-runtime file was modified by this session.**

---

## If you change renderer, re-run these

*(`HOUSE-00115`.)*
