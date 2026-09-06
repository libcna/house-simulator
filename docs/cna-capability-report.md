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
| C-04 | `System::*` from sharp-runtime — `Text.Json`, `IO`, `Xml.Serialization`, `IO.IsolatedStorage` — are available as CMake components | `sharp-runtimenext/modules/text-json/CMakeLists.txt` | `PENDING` | `HOUSE-00103` | — |
| C-05 | `StorageDevice`/`StorageContainer` work; Linux root is `$XDG_DATA_HOME/<app>` else `~/.local/share/<app>` | `modules/storage/src/StorageDevice.cpp:88-109` | `PENDING` | `HOUSE-00102` | — |

## §5.2 Math and volumes

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| M-01 | The XNA math surface is complete: `Vector2/3/4`, `Matrix`, `Quaternion`, `Plane`, `Ray`, `BoundingBox`, `BoundingSphere`, `BoundingFrustum`, `ContainmentType`, `Curve`, `MathHelper`, `Point`, `Rectangle`, `Color` | `modules/math/include/Microsoft/Xna/Framework/` | `PENDING` | `HOUSE-00104` | — |
| M-02 | `BoundingFrustum::GetCorners`, `BoundingBox::CreateFromPoints`, `Ray::Intersects` and `BoundingFrustum::Intersects(BoundingBox)` agree with analytic answers | SAMPLE-038 | `PENDING` | `HOUSE-00104` | — |

## §5.3 Graphics

| # | Claim | §5 evidence | Verdict | Probe | Measured |
|---|---|---|---|---|---|
| G-01 | `BasicEffect` with 3 directional lights, specular, fog and per-pixel lighting | `docs/basiceffect-support.md` | `PENDING` | `HOUSE-00082` | — |
| G-02 | `SkinnedEffect`, `MaxBones = 72`, `WeightsPerVertex` 1/2/4 | `docs/skinnedeffect-support.md` | `PENDING` | `HOUSE-00075`, `HOUSE-00077` | — |
| G-03 | `DualTextureEffect` (albedo × lightmap, two UV channels) | `docs/dualtextureeffect-support.md` | `PENDING` | `HOUSE-00078` | — |
| G-04 | `AlphaTestEffect` | `docs/alphatesteffect-support.md` | `PENDING` | `HOUSE-00080` | — |
| G-05 | `EnvironmentMapEffect` including the Fresnel term, `TextureCube` sampling | `docs/environmentmapeffect-support.md` | `PENDING` | `HOUSE-00081` | — |
| G-06 | `Effect` from compiled Effect-Framework bytecode, behind `CNA_EASYGL_COMPILED_EFFECTS=ON` (MojoShader) | `docs/fx-compiled-effects.md` §10 | `PENDING` | `HOUSE-00087` | — |
| G-07 | A compiled `Effect` works end-to-end in a real scene: two techniques switched by name, `Single` 2048² render target with `Depth24`, that target rebound as an effect texture | `cna-samples/plan.md:785` | `PENDING` | `HOUSE-00083`, `HOUSE-00088` | — |
| G-08 | `Model`/`ModelMesh`/`ModelMeshPart`/`ModelBone` and `CopyAbsoluteBoneTransformsTo` | `docs/model-content-pipeline-support.md` | **`PASS`** | `HOUSE-00072` | Depth-3 hierarchy with a sibling branch: every local and absolute transform equals the matrix computed offline, to 2e-5. `Copy*BoneTransformsTo` require a **pre-sized** destination and throw `destinationBoneTransforms` otherwise. |
| G-09 | `Model` from compiled content carries a real bone hierarchy via `.cnb` | `docs/xnb-content-pipeline-support.md` | **`PASS`** | `HOUSE-00072` | 4 authored nodes → 5 bones: CNA inserts a **synthetic `Root`** above the scene root. Parent links, `Index`, `Children` and mesh `ParentBone` all as authored. |
| G-10 | A skinned glTF compiles to `.cnb` and its joints are recoverable **without reading `Model::Tag`** | `docs/content-pipeline.md:456-458` | `PENDING` | `HOUSE-00074` | — |
| G-11 | `VertexBuffer`, `IndexBuffer`, `DynamicVertexBuffer`; EasyGL has a real 32-bit index factory | feature matrix | `PENDING` | `HOUSE-00092`, `HOUSE-00094` | — |
| G-12 | `RenderTarget2D`, `RenderTargetCube`, mip chains, MSAA on EasyGL | feature matrix | `PENDING` | `HOUSE-00083`, `HOUSE-00084` | — |
| G-13 | `BlendState`, `DepthStencilState` compare functions, `RasterizerState`, 16 per-slot `SamplerState`s | feature matrix | `PENDING` | `HOUSE-00079` | — |
| G-14 | `SpriteBatch` (all overloads, sort modes, custom `Effect`) and `SpriteFont` | feature matrix | **`PASS`** (SpriteFont + `Begin`/`DrawString`/`End`) | `HOUSE-00066`, `HOUSE-00089` | Text drawn into a `RenderTarget2D` and read back: 440 lit pixels, ink inside the `MeasureString` box. Sort modes and custom-`Effect` overloads remain for `HOUSE-00089`. |
| G-15 | `OcclusionQuery` exists; `PixelCount` is a real count **only** where the driver exposes `GL_SAMPLES_PASSED`, which the ES 3.2 profile does not — so it degrades to 0/1 (`BL-07`) | `docs/occlusionquery-support.md` | `PENDING` | `HOUSE-00090`, `HOUSE-00091` | — |
| G-16 | `DrawInstancedPrimitives` is present in the API | `GraphicsDevice.hpp:424` | `PENDING` | `HOUSE-00093` | — |
| G-17 | `Texture2D::SetData`/`GetData`/`FromStream`/`SaveAsPng`, NPOT sizes | feature matrix | **`PASS`** (`GetData`) | `HOUSE-00065` | 4×4 `Color` texture, 16/16 texels byte-exact — **against the premultiplied model**; see the finding. `SetData`/`FromStream`/`SaveAsPng` and NPOT are not yet probed. |
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
| `BL-09` / `Q-01` | Can a `SurfaceFormat::Single` 2048² `RenderTarget2D` be created, rendered to and read back? | `HOUSE-00083` | `PENDING` |
| `BL-02` | Is `ClearOptions::Stencil` ignored and `ReferenceStencil` inert? | `HOUSE-00085` | `PENDING` |
| `BL-03` | Does EasyGL MRT attachment 1 stay black? | `HOUSE-00086` | `PENDING` |
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
