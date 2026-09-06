# CNA capability report

`STATUS: IN PROGRESS — phase 1`

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
| C-05 | `StorageDevice`/`StorageContainer` work; Linux root is `$XDG_DATA_HOME/<app>` else `~/.local/share/<app>` | `modules/storage/src/StorageDevice.cpp:88-109` | `PENDING` | `HOUSE-00102` | — |

## §5.2 Math and volumes

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| M-01 | The XNA math surface is complete: `Vector2/3/4`, `Matrix`, `Quaternion`, `Plane`, `Ray`, `BoundingBox`, `BoundingSphere`, `BoundingFrustum`, `ContainmentType`, `Curve`, `MathHelper`, `Point`, `Rectangle`, `Color` | `modules/math/include/Microsoft/Xna/Framework/` | `PENDING` | `HOUSE-00104` | — |
| M-02 | `BoundingFrustum::GetCorners`, `BoundingBox::CreateFromPoints`, `Ray::Intersects` and `BoundingFrustum::Intersects(BoundingBox)` agree with analytic answers | SAMPLE-038 | `PENDING` | `HOUSE-00104` | — |

## §5.3 Graphics

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| G-01 | `BasicEffect` with 3 directional lights, specular, fog and per-pixel lighting | `docs/basiceffect-support.md` | **`PASS`** | `HOUSE-00082` | All six terms at once agree with the analytic model **to the byte**: `(92,90,86)` measured, `(92,90,86)` computed. Per-vertex and per-pixel agree on a constant-normal surface. |
| G-02 | `SkinnedEffect`, `MaxBones = 72`, `WeightsPerVertex` 1/2/4 | `docs/skinnedeffect-support.md` | **`PASS`** | `HOUSE-00075`, `HOUSE-00077` | `MaxBones == 72`; 72 accepted, 73 throws `boneTransforms exceeds MaxBones.`. Visible deformation confirmed by pixel comparison, not by absence of an exception. **`LightingEnabled = false` is refused** — as XNA 4.0 refuses it. |
| G-03 | `DualTextureEffect` (albedo × lightmap, two UV channels) | `docs/dualtextureeffect-support.md` | **`PASS`** | `HOUSE-00078` | 64/64 texels within **1/255** of the analytic product, with the two channels carrying *different* coordinates so a TEXCOORD0-for-both implementation would fail. The FNA `*2` doubling factor is present (128×128 → 128). |
| G-04 | `AlphaTestEffect` | `docs/alphatesteffect-support.md` | **`PASS`** | `HOUSE-00080` | The cutoff is exact to one alpha value for all six comparison functions, measured on a 256-column alpha ramp. Two-sided foliage works under `CullNone`; a back-facing card is correctly invisible under the single-sided state. |
| G-05 | `EnvironmentMapEffect` including the Fresnel term, `TextureCube` sampling | `docs/environmentmapeffect-support.md` | **`PASS`** | `HOUSE-00081` | The sampled cube face equals `reflect(-E, N)` at three tilts, checked against six distinctly coloured faces. `EnvironmentMapAmount` blends linearly (0 → 0, 0.5 → half, 1 → the environment). Fresnel raises grazing reflectivity for factors 1 and 4. `AmbientLightColor` **does** reach this effect. |
| G-06 | `Effect` from compiled Effect-Framework bytecode, behind `CNA_EASYGL_COMPILED_EFFECTS=ON` (MojoShader) | `docs/fx-compiled-effects.md` §10 | `PENDING` | `HOUSE-00087` | — |
| G-07 | A compiled `Effect` works end-to-end in a real scene: two techniques switched by name, `Single` 2048² render target with `Depth24`, that target rebound as an effect texture | `cna-samples/plan.md:785` | `PENDING` | `HOUSE-00083`, `HOUSE-00088` | — |
| G-08 | `Model`/`ModelMesh`/`ModelMeshPart`/`ModelBone` and `CopyAbsoluteBoneTransformsTo` | `docs/model-content-pipeline-support.md` | **`PASS`** | `HOUSE-00072` | Depth-3 hierarchy with a sibling branch: every local and absolute transform equals the matrix computed offline, to 2e-5. `Copy*BoneTransformsTo` require a **pre-sized** destination and throw `destinationBoneTransforms` otherwise. |
| G-09 | `Model` from compiled content carries a real bone hierarchy via `.cnb` | `docs/xnb-content-pipeline-support.md` | **`PASS`** | `HOUSE-00072` | 4 authored nodes → 5 bones: CNA inserts a **synthetic `Root`** above the scene root. Parent links, `Index`, `Children` and mesh `ParentBone` all as authored. |
| G-10 | A skinned glTF compiles to `.cnb` and its joints are recoverable **without reading `Model::Tag`** | `docs/content-pipeline.md:456-458` | **`PASS`** | `HOUSE-00074`, `HOUSE-00076` | Recoverable by **name lookup into `Model::Bones`**, which is all a sidecar carries. Blend indices are **skin-local**, not bone indices, so the sidecar *must* carry the joint-name list — acceptance path (4), not (2). `Model::Tag` and `getSkinsEXTProperty()` are never read. |
| G-11 | `VertexBuffer`, `IndexBuffer`, `DynamicVertexBuffer`; EasyGL has a real 32-bit index factory | feature matrix | `PENDING` | `HOUSE-00092`, `HOUSE-00094` | — |
| G-12 | `RenderTarget2D`, `RenderTargetCube`, mip chains, MSAA on EasyGL | feature matrix | **`PASS`** (`RenderTarget2D`) | `HOUSE-00083`, `HOUSE-00084` | 2048² `Single`+`Depth24` and 64² `Color` with a 7-level mip chain, at MSAA 0 and 4, all create, render, unbind and sample. `RenderTargetCube` not probed. |
| G-13 | `BlendState`, `DepthStencilState` compare functions, `RasterizerState`, 16 per-slot `SamplerState`s | feature matrix | **`PASS`** (the Tier S subset) | `HOUSE-00079`, `HOUSE-00080` | `BlendState::Opaque`/`Additive`, `CompareFunction::Equal` depth with writes off, all three `CullMode`s and per-slot `PointClamp` on slots 0 and 1 all behave. Additive sums are **exact** and a depth-equal second pass reaches every pixel of the first. |
| G-14 | `SpriteBatch` (all overloads, sort modes, custom `Effect`) and `SpriteFont` | feature matrix | **`PASS`** (SpriteFont + `Begin`/`DrawString`/`End`) | `HOUSE-00066`, `HOUSE-00089` | Text drawn into a `RenderTarget2D` and read back: 440 lit pixels, ink inside the `MeasureString` box. Sort modes and custom-`Effect` overloads remain for `HOUSE-00089`. |
| G-15 | `OcclusionQuery` exists; `PixelCount` is a real count **only** where the driver exposes `GL_SAMPLES_PASSED`, which the ES 3.2 profile does not — so it degrades to 0/1 (`BL-07`) | `docs/occlusionquery-support.md` | `PENDING` | `HOUSE-00090`, `HOUSE-00091` | — |
| G-16 | `DrawInstancedPrimitives` is present in the API | `GraphicsDevice.hpp:424` | `PENDING` | `HOUSE-00093` | — |
| G-17 | `Texture2D::SetData`/`GetData`/`FromStream`/`SaveAsPng`, NPOT sizes | feature matrix | **`PASS`** (`GetData`, `SetData`) | `HOUSE-00065`, `HOUSE-00078`, `HOUSE-00110` | 4×4 `Color` texture, 16/16 texels byte-exact — **against the premultiplied model**; see the finding. `SetData`/`FromStream`/`SaveAsPng` and NPOT are not yet probed. |
| G-18 | WebGL context-loss handling is implemented and browser-qualified | `docs/web-emscripten-graphics-limitations.md` | `NOT PROBED` | — | Deferred to the Web phases (47–48). `HOUSE-00108` probes the desktop `DebugSimulateContextLoss` path only. |

## §5.4 Audio

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| A-01 | `SoundEffect`, `SoundEffectInstance`, looping, volume/pitch/pan on the SDL3 mixer | `modules/audio/` | **`PASS`** (load, `CreateInstance`, volume, `Play`, `Stop`) | `HOUSE-00067` | Real device opened at 44 100 Hz stereo; duration exact, state `Playing` after `Play()`. Looping and pitch/pan are not yet probed. |
| A-02 | `AudioListener`, `AudioEmitter`, `SoundEffectInstance::Apply3D` | `SoundEffectInstance.hpp:362,383` | `PENDING` | `HOUSE-00095` | — |
| A-03 | The 3D model is **simplified**: pan, distance attenuation and Doppler only — no speaker matrix, cones, curves, LFE, HRTF or reverb sends | `cna_audio_deep_audit_2026-07-17.md:161` | `PENDING` | `HOUSE-00095` | — |
| A-04 | Doppler is present and carries an up-to-4× pitch-inflation risk on wrong velocity units (`BL-11`) | same doc, A-09 | `PENDING` | `HOUSE-00096` | — |
| A-05 | Accepted wave formats are PCM8, PCM16, IEEE float, MS-ADPCM, IMA-ADPCM — **24-bit PCM is not listed** (`BL-06`) | `docs/xnb-content-pipeline-support.md` audio matrix | **`DIFFERENT`** | `HOUSE-00067`, `HOUSE-00068` | **24-bit PCM is not rejected.** The pipeline converts it to 16-bit with a warning, bit-identically to ffmpeg; `SoundEffect::FromStream` accepts raw 24-bit too. `BL-06` corrected. |
| A-06 | `MediaPlayer`/`Song` exist; `Song` is an external stream reference | same doc | `NOT PROBED` | — | `cna-house` uses `SoundEffect` throughout (§62); the TV audio track decision is `HOUSE-01361`+. |

## §5.5 Media

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| V-01 | `Video`/`VideoPlayer` with `GetTexture()`, `Play`, `Pause`, `Stop`, `IsLooped`, `Volume`, `State` | `VideoPlayer.hpp:70-148` | `PENDING` | `HOUSE-00098` | — |
| V-02 | Decoding is the optional `cna_video_ffmpeg` backend; Linux native works with FFmpeg dev packages present | `docs/video-backend.md` | `PENDING` | `HOUSE-00098` | Configure-time observation only: this build printed `CNA: video enabled (AUTO; FFmpeg backend cna_video_ffmpeg)` and linked `libcna_video_ffmpeg.a`. Runtime behaviour is unmeasured. |
| V-03 | With the backend absent, `Play()` throws `System::NotSupportedException` (`BL-05`) | `docs/video-backend.md` | `PENDING` | `HOUSE-00099` | — |

## §5.6 Input

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| I-01 | `Keyboard`, `Mouse`, `GamePad*` and the whole `Touch/` namespace are present | `modules/input/include/…` | `PENDING` | `HOUSE-00101` | — |
| I-02 | `Mouse::SetPosition(int,int)` is available, which is all strict-XNA first-person mouse-look needs | `Mouse.hpp:49` | `PENDING` | `HOUSE-00100` | — |
| I-03 | CNA's `CNAEXT` relative-mouse mode exists and is **not used** | `Mouse.hpp:67,74` | **`PASS`** (not used) | `HOUSE-00063` | The probe calls it nowhere and the source lint forbids it. Note it is **not** unlinkable: CNAEXT members of core XNA classes are always compiled — see the `HOUSE-00063` finding. |

## §5.7 Content pipeline (`cna-content`)

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| P-01 | `.gltf`/`.glb` → `CNA.GltfImporter/2` → `CNA.ModelContentWriter/3` → `Model` | `docs/content-pipeline.md:370-382` | **`PASS`** | `HOUSE-00070`, `HOUSE-00071` | `CNA.GltfImporter -> CNA.ModelProcessor -> CNA.ModelContentWriter`. Positions, normals and UVs bit-exact against the authored `.glb`; indices unreversed; bounds exact. **`CullClockwise` is the correct draw state.** The emitted vertex is **48 bytes, not `VertexPositionNormalTexture`** — see the finding. |
| P-02 | `.png`/`.jpg`/`.dds` → `Texture2D` | same | **`PASS`** (`.png`) | `HOUSE-00065` | `CNA.ImageImporter -> CNA.TextureProcessor -> CNA.Texture2DContentWriter`. `premultiplyAlpha` defaults true. `.jpg`/`.dds` not probed. |
| P-03 | `.wav` → `SoundEffect` | same | **`PASS`** | `HOUSE-00067`, `HOUSE-00068`, `HOUSE-00069` | `CNA.WavImporter -> CNA.SoundEffectProcessor -> CNA.SoundEffectContentWriter`, for 16-bit and (by conversion) 24-bit sources. |
| P-04 | `.spritefont` (+ TTF via FreeType) → `SpriteFont` | same | **`PASS`** | `HOUSE-00066` | `CNA.FontDescriptionImporter -> CNA.FontDescriptionProcessor -> CNA.SpriteFontContentWriter`; FreeType 2.13.3. `<FontName>` resolves a file beside the descriptor before any system font. |
| P-05 | `.fx` → `Effect`, **`--format xnb` only** | same | `PENDING` | `HOUSE-00087` | — |
| P-06 | `.fxb` (already-compiled bytecode) → `Effect` | same | `PENDING` | `HOUSE-00087` | — |
| P-07 | video → `CnbVideoData` → `Video` (deployed, not decoded, at build time) | same | `PENDING` | `HOUSE-00098` | — |
| P-08 | `cna_add_content(TARGET … SOURCE_DIR … OUTPUT_DIR … CONFIG_FILE … WORKERS …)` is the CMake integration | `cmake/ToolContentPipeline.cmake:48` | `PENDING` | `HOUSE-00126` | Phase 2 consumes it; phase 1 drives `cna-content` directly. |
| P-09 | `.fx` compilation is driven through an external compiler with `--fx-compiler <fxc> --fx-compiler-launcher wine` | `docs/content-pipeline.md:409-424` | `PENDING` | `HOUSE-00087` | — |

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
| `BL-07` | Is `OcclusionQuery::PixelCount` a count or a boolean here? | `HOUSE-00090`, `HOUSE-00091` | `PENDING` |
| `BL-09` / `Q-01` | Can a `SurfaceFormat::Single` 2048² `RenderTarget2D` be created, rendered to and read back? | `HOUSE-00083` | **SETTLED — YES, all four stages.** Created with `Depth24`, rendered into, read back with `GetData(float*)` **bit-exactly** (0.625 in, 0.625 out over 3 396 649 texels), and bound as an effect texture and sampled (159/255). **Tier E uses a real float shadow map**; the RGBA8 packing fallback is not needed. |
| `BL-02` | Is `ClearOptions::Stencil` ignored and `ReferenceStencil` inert? | `HOUSE-00085` | **SETTLED — the premise is FALSE. Stencilling works.** A two-pass mask stamped `ReferenceStencil = 1` over the left half and then drew the full quad under `CompareFunction::Equal`: left half **2048/2048** lit, right half **0/2048**. `cna-house.md` §6 `BL-02` is corrected. |
| `BL-03` | Does EasyGL MRT attachment 1 stay black? | `HOUSE-00086` | **SETTLED for the stock-effect case — yes.** Two targets bound with `SetRenderTargets`: attachment 0 received `(255,128,64)`, attachment 1 stayed `(0,0,0)`. **Scope:** a stock effect declares one output, so this shows the renderer does not broadcast; whether a *compiled* two-output `.fx` reaches attachment 1 is a Tier E question and is not answered here. |
| `BL-05` | Does `VideoPlayer::Play()` throw `NotSupportedException` without the backend? | `HOUSE-00099` | `PENDING` |
| `BL-11` | Does `DopplerScale = 0` with zero velocities produce no pitch change? | `HOUSE-00096` | `PENDING` |
| `BL-12` | Is `Model::Meshes[i].BoundingSphere` populated from `.cnb`? | `HOUSE-00073` | **SETTLED — yes, and it is conservative rather than minimal.** Non-degenerate and it contains every vertex, but on the test box its radius is 7.686 against a minimal 6.225 (**+23 %**) and its centre is 1.55 off in Y. Usable for a cheap reject; not usable as a tight bound. |

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


---

## Probe hygiene

Every probe in this tranche was built in `build-probe/` — in this repository, never `/tmp`, never
the session scratchpad, never a per-ticket directory — with `CCACHE_DIR=/rv/cnaccache` and
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
