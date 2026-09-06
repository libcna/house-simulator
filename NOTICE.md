# Third-party notices

`cna-house` itself is licensed under the Microsoft Public License (Ms-PL); see
[`LICENSE`](LICENSE). **That licence covers the source code in this repository only.**

---

## Content assets carry their own licences

Every file under `assets-src/` — models, textures, audio, fonts, video — is governed by the
licence of the party that produced it, **not** by this repository's Ms-PL. Those licences are
recorded per asset and are reproduced verbatim in [`licenses/`](licenses/):

* `licenses/<slug>/LICENCE.txt` — the upstream licence text, copied verbatim;
* `licenses/<slug>/source.url.txt` — where it came from;
* `licenses/<slug>/retrieved.txt` — when it was retrieved;
* [`licenses/THIRD-PARTY-ASSETS.md`](licenses/THIRD-PARTY-ASSETS.md) — **generated** from
  `assets-src/assets.manifest.json`, one row per asset, and displayed in-game on the credits
  screen so attribution cannot be forgotten.

The rules that keep this true are in
[`docs/decisions/ADR-0012-asset-licensing.md`](docs/decisions/ADR-0012-asset-licensing.md) and
`cna-house.md` §20. The short version: **no manifest row, no build.**

---

## Runtime dependencies

`cna-house` adds no third-party runtime dependency of its own (`cna-house.md` §82.3). It is built
against two sibling source checkouts, consumed by `add_subdirectory`, never vendored:

### CNA

A C++ reimplementation of the Microsoft XNA Game Studio 4.0 programming model, developed by the
OpenEggbert project. Licensed under the Microsoft Public License (Ms-PL).

<https://github.com/openeggbert/cna>

`cna-house` uses only the XNA 4.0 API surface CNA implements. The CNAEXT engine layer
(`modules/graphics-ext/`) is compiled out by `CNA_CNAEXT=OFF` and must never be enabled; see
[`docs/decisions/ADR-0001-xna-only.md`](docs/decisions/ADR-0001-xna-only.md).

### sharp-runtime

A C++ port of selected .NET base class library types, developed by the OpenEggbert project.
Licensed under the Microsoft Public License (Ms-PL).

<https://github.com/openeggbert/sharp-runtime>

### Transitively, through CNA

SDL3 (zlib licence) always; FFmpeg (LGPL-2.1-or-later, as configured by CNA) when
`CNA_ENABLE_VIDEO` resolves to on; MojoShader (zlib licence) when compiled effects are enabled.
These are selected by CNA's own CMake options, not by `cna-house`.

---

## Test-only and build-only dependencies

Never linked into a shipped binary:

| Component | Licence | Used for |
|---|---|---|
| GoogleTest | BSD-3-Clause | the unit, integration, render and perf test binaries |
| Blender 4.3.2 | GPL-3.0-or-later | offline LOD, lightmap and impostor generation |
| ffmpeg | LGPL-2.1-or-later / GPL | offline audio and video conversion |
| `fxc.exe` (DirectX SDK, June 2010) | Microsoft DirectX SDK EULA | offline `.fx` → Effect bytecode compilation, run through Wine |
| Python 3.11 | PSF-2.0 | manifest, validation and generation tooling |
| `cna-content` | Ms-PL (part of CNA) | the offline content pipeline |

None of them exists at runtime. The runtime sees only `.cnb`/`.xnb` files and
`ContentManager::Load<T>()`.

---

## XNA Game Studio

"XNA" and "XNA Game Studio" are Microsoft trademarks. `cna-house` is an independent project. It is
not affiliated with, endorsed by, or sponsored by Microsoft Corporation. No Microsoft source code
or redistributable is included in this repository.
