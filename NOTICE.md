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

### Fonts — SIL Open Font License 1.1

Two font files are redistributed with this project, in source form under `assets-src/Fonts/` and in
rasterised form inside the compiled `Fonts/*.cnb`. Both are **unmodified** official Noto release
artifacts:

> Copyright 2022 The Noto Project Authors
> (<https://github.com/notofonts/latin-greek-cyrillic>)
>
> This Font Software is licensed under the SIL Open Font License, Version 1.1.
> This license is copied at [`licenses/ofl-1.1/LICENCE.txt`](licenses/ofl-1.1/LICENCE.txt) and is
> also available with a FAQ at <https://openfontlicense.org>.

| File | Family | Upstream version | SHA-256 |
|---|---|---|---|
| `assets-src/Fonts/NotoSans-Regular.ttf` | Noto Sans | `NotoSans-v2.015` | `478c558ea716033cd60c03438f628dfa75694dcf6b5f6d505a2f05fd2b4f3823` |
| `assets-src/Fonts/NotoSansMono-Regular.ttf` | Noto Sans Mono | `NotoSansMono-v2.014` | `65b5e2b2c4a1fba9ae8be1f026cb35b03dcb8886d9b2a4147054fde12f7e767d` |

**The OFL applies to the font software, and to nothing else in this repository.** OFL 1.1 says so
itself — "The requirement for fonts to remain under this license does not apply to any document
created using the Font Software" — and SIL's own FAQ 1.3 and 1.13 confirm that bundling an OFL font
with a program neither makes the program open source nor changes the licence of anything drawn with
it. `cna-house` remains Ms-PL. FAQ 1.4 permits selling a software package containing these fonts and
names "games and entertainment software" among its examples; the one prohibition that matters is
OFL clause 1, which forbids selling the fonts **by themselves**, and this project does not.

No Reserved Font Name is declared for either family, so clause 5 places no restriction on naming.
"Noto" is a trademark of Google LLC; it is used here only as the factual name of the unmodified
upstream files and is not part of this product's branding.

The evidence behind every one of these statements — the URLs, the retrieval date, the hashes, and
which authoritative page each claim came from — is in
[`docs/font-provenance.md`](docs/font-provenance.md).

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

SDL3 and SDL3_mixer (zlib licence) always; FFmpeg (LGPL-2.1-or-later, as configured by CNA) when
`CNA_ENABLE_VIDEO` resolves to on; MojoShader (zlib licence) when compiled effects are enabled.
These are selected by CNA's own CMake options, not by `cna-house`.

The Linux package (`tools/ci/package_linux.py`) bundles the `libcna.so`, SDL3 and SDL3_mixer
shared libraries the game was built with. FFmpeg and the graphics driver are the system's, loaded
dynamically and not redistributed.

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

### Test reference data

Two test fixtures hold numbers this project did not compute, because a model checked against its
own arithmetic proves only that the arithmetic is self-consistent.

| Fixture | Source | Licence |
|---|---|---|
| `tests/unit/reference/calendar.golden.txt` | Python's `datetime` and `zoneinfo`, over the IANA time-zone database | PSF-2.0 / IANA database public domain; the file is generated output, not a copy |
| `tests/unit/reference/suntimes.usno.txt` and its `.raw.json` cache | [US Naval Observatory, Astronomical Applications Department](https://aa.usno.navy.mil/api/rstt/oneday), retrieved 2026-09-11 | a work of the United States Government, **not subject to copyright in the United States** (17 U.S.C. §105) |
| `tests/unit/reference/moontimes.usno.txt`, `moonphases.usno.txt` and their `.raw.json` caches | [US Naval Observatory, Astronomical Applications Department](https://aa.usno.navy.mil/api/rstt/oneday), retrieved 2026-09-13 | a work of the United States Government, **not subject to copyright in the United States** (17 U.S.C. §105) |

The USNO extracts contain 195 days of sunrise, sunset and transit times, 60 moonrise times, and 60
local-noon lunar illumination/phase observations at one location — §33's 40.05° N, 75.30° W —
kept so that CI never needs the network. `tools/ci/suntimes_table.py --fetch`,
`tools/ci/moontimes_table.py --fetch`, and `tools/ci/moonphases_table.py --fetch` reproduce them.

---

## XNA Game Studio

"XNA" and "XNA Game Studio" are Microsoft trademarks. `cna-house` is an independent project. It is
not affiliated with, endorsed by, or sponsored by Microsoft Corporation. No Microsoft source code
or redistributable is included in this repository.
