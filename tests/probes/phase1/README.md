# Phase-1 capability probes

These are the programs that produced `docs/cna-capability-report.md`. They are **not** the
`cna-house` application and they are not tests: each one answers a specific question about CNA that
`cna-house.md` §5 asserted but had not measured, and each is named after the `HOUSE-000xx` task that
asked it.

They are kept because a verdict without its probe is a claim, and because
`HOUSE-00115`'s "re-run these if you change renderer" list is only actionable if the probes still
exist. `HOUSE-00114` deleted every binary and object; these sources and their generators are all
that remain.

## How to build and run one

The probes are built out of `build-probe/`, which is a *binary* directory from the openeggbert
closed list and holds no sources of its own:

```bash
export CCACHE_DIR="$HOME/.cache/ccache" CCACHE_BASEDIR=/rv
cmake -S build-probe -B build-probe -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache
python3 tests/probes/phase1/p1-make-gltf.py build-probe/p1-fixtures   # generate the fixtures
cmake --build build-probe --target p1-static -j"$(nproc)"
./build-probe/p1-static
```

Targets are `EXCLUDE_FROM_ALL`, so a tranche builds only the probes it is measuring. Every probe
prints one `[ok]`/`[FAIL]` line per check and exits non-zero if any check failed.

A probe needing a different renderer or a different CNA option is built in `build-consumer/`,
configured from the *same* source directory — see `HOUSE-00117`:

```bash
cmake -S build-probe -B build-consumer -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCNA_GRAPHICS_RENDERER=HEADLESS
```

`CNA_CNAEXT=OFF` is `FORCE`d and cannot be varied from either directory.

## What each probe measures

| Probe | Tasks | Question |
|---|---|---|
| `p1-common.hpp` | — | Shared scaffolding: the `Report` check/verdict harness, a `Game` that measures once in `Draw` and exits, and a layout-agnostic vertex-buffer reader |
| `p1-make-gltf.py` | fixtures | Generates every `.glb` fixture **and the C++ literals the probes compare against**, so an expectation is derived from the generator rather than transcribed |
| `p1-split-skins.py` | `HOUSE-00076` | The offline multi-skin splitter: one `.glb` per skin plus a `.attach.json` attachment record |
| `p1-static.cpp` | `HOUSE-00070`, `00071`, `00073` | glTF → `.cnb` → `Model`: bit-exact positions/normals/UVs, bounds, orientation, winding, mesh bounding sphere |
| `p1-hier.cpp` | `HOUSE-00072` | Bone hierarchy, `ParentBone`, `Root`, `CopyAbsoluteBoneTransformsTo`, against offline-computed matrices |
| `p1-skin.cpp` | `HOUSE-00074` | **Do blend indices reference `Model::Bones` or the skin's own joint list?** (they are skin-local) |
| `p1-skinanim.cpp` | `HOUSE-00075`, `00077` | A project-owned clip evaluator driving `SkinnedEffect::SetBoneTransforms`; the 72-bone cap |
| `p1-skinsplit.cpp` | `HOUSE-00076` | The pipeline's per-skin split and the offline splitter, compared pixel for pixel |
| `p1-dualtex.cpp` | `HOUSE-00078` | `DualTextureEffect` against the analytic product, with the two UV channels deliberately different |
| `p1-multipass.cpp` | `HOUSE-00079` | Additive multipass lighting and depth-equal invariance, on a depth-tilted quad |
| `p1-alphatest.cpp` | `HOUSE-00080` | The alpha cutoff for all six compare functions, on a 256-column ramp; two-sided foliage |
| `p1-envmap.cpp` | `HOUSE-00081` | Which cube face `reflect(-E, N)` samples; the Fresnel term |
| `p1-basicfx.cpp` | `HOUSE-00082` | `BasicEffect`'s whole lighting model, implemented in C++ and asserted per pixel |
| `p1-rtsingle.cpp` | `HOUSE-00083` | `SurfaceFormat::Single` 2048² end to end — the probe that settled `BL-09`/`Q-01` |
| `p1-rtcaps.cpp` | `HOUSE-00084`, `00085`, `00086` | Mips and MSAA; the stencil test that disproved `BL-02`; stock-effect MRT |
| `p1-fxload.cpp` | `HOUSE-00087`–`00089`, `00086` | A compiled `.fx`: techniques by name, parameters, `SpriteBatch`, and two-output MRT |
| `p1-formats.cpp` | `HOUSE-00110`, `00111` | The `SurfaceFormat` survey at both graphics profiles; DXT through `.cnb` versus `.xnb` |
| `p1-perf.cpp` | `HOUSE-00090`–`00094`, `00106`, `00107` | `OcclusionQuery`; 32-bit indices; dynamic streaming; instancing; draw-call and upload cost |
| `p1-audio3d.cpp` | `HOUSE-00095`–`00097` | What `Apply3D` does and does not write; the Doppler guarantee; the voice ceiling |
| `p1-video.cpp` | `HOUSE-00098` | `VideoPlayer::GetTexture` frame advance, classified against a locally generated clip |
| `p1-videooff.cpp` | `HOUSE-00099` | `CNA_ENABLE_VIDEO=OFF`: the refusal, and that the rest still links |
| `p1-input.cpp` | `HOUSE-00100`, `00101` | The 10 000-frame mouse recentring loop; keyboard, gamepad and desktop `TouchPanel` |
| `p1-storage.cpp` | `HOUSE-00102`, `00103` | `StorageDevice`/`StorageContainer` and the resolved path; bit-exact JSON float round-trip |
| `p1-math.cpp` | `HOUSE-00104` | Frustum, box, sphere, ray and plane intersections against hand-computed answers |
| `p1-headless.cpp` | `HOUSE-00105` | 600 frames under `HEADLESS` with no display server, asserted from inside the process |
| `p1-devicereset.cpp` | `HOUSE-00108` | `GraphicsDevice::Reset()` and rebuilding resources from CPU state |
| `p1-aniso.cpp` | `HOUSE-00109` | Whether `AnisotropicClamp` measurably retains detail, not merely whether it is accepted |

The `.fx` fixture `p1-fxload` uses lives at `assets-src/Effects/P1Probe.fx`, because that is the only
place `tools/ci/check_xna_only.py` permits an effect source — and having it in the tree means the
Tier E pipeline stays exercised rather than becoming a one-off.

## The rule these were written under

**A probe never confirms what it hoped for.** Every expectation is computed independently — by the
generator, by hand in a frame chosen so the answer is exact, or from the formula in CNA's own source
clearly labelled as *read* rather than *measured*. Four probes in this phase produced confident,
wrong answers before their fixtures were corrected; `HOUSE-00119` in the capability report lists
them, because the fixture is the part most likely to be wrong and the part least likely to be
questioned.
