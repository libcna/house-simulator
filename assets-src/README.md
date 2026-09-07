# `assets-src/` — the authored truth

Every runtime asset starts here and is compiled into `content/` (`.cnb`) or `content-fx/` (`.xnb`)
by `cna-content`. Nothing under `content*/` is edited or committed; it is all derived.

## The two trees, and why they are two

```
assets-src/Models Textures Audio Fonts world   ──→  content/<dir>/   (.cnb)
assets-src/Media                                ──→  content/         (.cnb + streamed media)
assets-src/Effects                              ──→  content-fx/      (.xnb)
```

`cna_add_content` has **no format option** — it always produces `.cnb` — so effects, which must be
`.xnb`, cannot go through the same call. That is why `CMakeLists.txt` drives the effect tree with an
explicit `cna-content build --format xnb` custom command instead.

**The main tree is built one subdirectory at a time, not as one root, and that is measured rather
than stylistic.** `cna-house.md` §18.1 sketched a single `cna_add_content(SOURCE_DIR assets-src)`
with "`Effects/` and `world/` excluded by the config". The version-1 configuration format has **no
exclusion mechanism** — it maps per-asset overrides and named source roots, and nothing else — and a
single-root build was tried (`HOUSE-00181`): it discovers `Effects/P1Probe.fx`, tries to compile it
into the `.cnb` tree, and **fails the whole build** when no `fxc` is configured. Building
`Models/`, `Textures/`, `Audio/`, `Fonts/`, `Video/` and `world/` as separate roots is what the
format actually supports.

## `Media/` is built into the content ROOT, and that is measured (`HOUSE-00201`)

Every other tree is built one subdirectory at a time into `content/<dir>/`. `Media/` is not: it is
built with `content/` **itself** as its output directory, and carries the `Video/` component in its
own source layout instead — `assets-src/Media/Video/clip.ogv` produces `content/Video/clip.cnb`.

The reason is a real asymmetry in how CNA resolves a streamed asset. A `Video` (and a `Song`)
compiles to a small metadata `.cnb` **plus a byte-identical copy of the media file**, and the
runtime finds that copy through `ContentManager::BuildAssetPath` — that is, relative to the
**content root**, not to the `.cnb` sitting beside it. Everything else is resolved relative to the
directory it was built into. With `OUTPUT_DIR = content/Video` the two never meet:

| `streamReference` | Deployed to | Looked for at |
|---|---|---|
| `smoke_clip.ogv` | `content/Video/smoke_clip.ogv` | `content/smoke_clip.ogv` |
| `Video/smoke_clip.ogv` | `content/Video/Video/smoke_clip.ogv` | `content/Video/smoke_clip.ogv` |

Both were tried, and both fail with *"'Video/smoke_clip' streams '…', which was not found beside
it."* Building the media tree at the content root makes the deployed path and the resolved path the
same one, keeps the content name `Video/smoke_clip` that `cna-house.md` §58 uses, and copies
nothing twice. `ContentSmokeTests.TheStreamedVideoSitsWhereTheRuntimeResolvesIt` asserts the layout
so that "tidying" it back is a red test rather than a silent load failure.

### `assets-src/Media/.cna-content.json`

| Asset | Parameter | Value | Why |
|---|---|---|---|
| `Video/smoke_clip.ogv` | `width`, `height` | `64`, `64` | **`VideoProcessor` requires them**, because CNA does not decode the file at build time — it deploys the stream and trusts the metadata. That means the metadata *can* disagree with the file, silently, so `make_smoke_assets.py --selftest` re-probes the clip and asserts they agree. |
| | `framesPerSecond` | `10.0` | As above. |
| | `durationMs` | `2000` | Optional, and given: `Video::getDurationProperty` is what the TV backends of §58.2 will schedule against. |
| | `streamReference` | `Video/smoke_clip.ogv` | Content-root-relative, for the reason in the table above. |

Numeric values are written as **strings** (`"value": "64"`), which the configuration format
requires: *"numeric values must be strings so their exact persisted value is stable."*

## Where the `.cna-content.json` files live, and why not at the top

`cna-content` reads `.cna-content.json` **from the source root of the build that is running**, and
asset keys inside it are relative to that root. Because the main tree is built one subdirectory at a
time (above), a single file at `assets-src/` would never be read by anything. It was tried:
placing one there changed no output byte. The configs therefore sit in the roots that actually have
a choice to make — `assets-src/Textures/` and `assets-src/Effects/` — and a subdirectory with no
content-affecting choice has no file, because an empty config is a file someone has to read to
learn it says nothing.

**Both are verified read, not assumed read.** Flipping `generateMipmaps` to `true` in the textures
config moved `grey.cnb` from 448 to 564 bytes and `missing.cnb` from 1 408 to 1 940 bytes; putting
an invalid `profile` in the effects config failed the build with
`EffectSourceProcessor parameter 'profile' must be 'reach' or 'hidef'`. A configuration file that
has never been shown to change anything is a file that may not be being read at all.

## The `.cna-content.json` files

JSON carries no comments, so the reasons live here.

### `assets-src/Textures/.cna-content.json`

| Asset | Parameter | Value | Why |
|---|---|---|---|
| `Fallback/grey.png` | `generateMipmaps` | `false` | A single flat colour. A mip chain of one colour is the same colour at every level and costs a third more memory for nothing. |
| | `premultiplyAlpha` | `true` | Pinned, not defaulted. It **is** the pipeline default, and `HOUSE-00065` measured that `SpriteBatch::Begin()` selects `BlendState::AlphaBlend` — the premultiplied blend — so straight alpha would fringe. Writing it down means a change to CNA's default cannot silently change our pixels; the parameter enters the build fingerprint, so such a change rebuilds instead of being absorbed. |
| `Fallback/missing.png` | `generateMipmaps` | `false` | The magenta checker exists to be **unmissable**. Mipping a high-frequency checker averages it toward flat pink at distance, which is exactly the failure it is there to prevent. |
| | `premultiplyAlpha` | `true` | As above. |

### `assets-src/Effects/.cna-content.json`

| Asset | Parameter | Value | Why |
|---|---|---|---|
| `P1Probe.fx` | `profile` | `reach` | The source compiles at `vs_2_0`/`ps_2_0`, which is Reach. §18.1 names `hidef` for the effect set, and that is right for the phase-12 effects that will use shader model 3 — setting it here, on a 2.0 source, would be a value nobody had a reason for that changes the fingerprint anyway. |
| | `debug` | `false` | Pinned for the same reason as `premultiplyAlpha`: a compiler default that changed under us would otherwise ship a debug shader silently. |

## `SOURCE.md`: every downloaded asset says where it came from (`HOUSE-00261`)

Any directory here that holds a **downloaded** asset must carry a `SOURCE.md`, copied from
[`docs/asset-review/SOURCE-TEMPLATE.md`](../docs/asset-review/SOURCE-TEMPLATE.md).
`tools/assets/verify_licences.py` fails the build if one is missing — and, more usefully, if one
exists but does not mention every downloaded asset in that directory **by its manifest id**. A
record written for the first asset and never updated when a second arrived would otherwise pass
forever, which is how every unenforced "please document it" convention ends.

It is not a duplicate of the manifest. The manifest holds the facts a machine checks: hashes, the
four rights booleans, the licence file. `SOURCE.md` holds what a JSON field cannot — *how* the
licence was established, *what* was checked, and *what was rejected and why*. Negative findings
belong there too; a variant considered and dropped is worth more to the next reader than a bare
statement of what was chosen.

**"Free download" is not "redistributable".** Read the licence on the publisher's own page, never an
aggregator's label, and archive the text under `licenses/<slug>/` rather than linking to it. If the
terms are unclear the asset is unusable until they are resolved: say so, and mark the row
`PROVENANCE UNKNOWN — DO NOT SHIP`.


## The fonts are vendored, and that is load-bearing (`HOUSE-00200`)

`Fonts/` holds two committed `.ttf` files and the five `.spritefont` descriptors that rasterise
them:

```
Fonts/NotoSans-Regular.ttf       Noto Sans 2.015, hinted        →  ui-16, ui-22, ui-30
Fonts/NotoSansMono-Regular.ttf   Noto Sans Mono 2.014, hinted   →  mono-13, mono-16
```

Both are unmodified official Noto release artifacts under the SIL Open Font License 1.1. The full
evidence — release tags, both canonical URLs, the SHA-256 of each file, the licence checks, and why
the *hinted* build was chosen over `unhinted/` and `full/` — is
[`docs/font-provenance.md`](../docs/font-provenance.md).

**`<FontName>` names a FILE IN THIS DIRECTORY, not an installed family.** The importer tries
`<FontName>`, then `<FontName>.ttf`, beside the descriptor, and searches `/usr/share/fonts` only if
neither exists. That search is the hazard this section used to describe: it succeeds with a
**warning**, not an error.

It is not hypothetical. This machine has
`/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf` at version **2.004** and
`NotoSansMono-Regular.ttf` at **2.006** — the filename stems collide with ours exactly. Delete the
vendored files and the build still passes, silently rasterising the host's older faces, and all five
outputs change. `tools/ci/check_fonts.py` therefore fails a descriptor whose font file is missing,
because a warning in a build log is not a defence.

**Measured, and no longer a hazard:** building the `Fonts` root inside a mount namespace with every
system font directory replaced by an empty one produces byte-identical output. The five compiled
fonts do not depend on what is installed. `content_verify.py`'s `KNOWN_CROSS_MACHINE_HAZARDS` is now
empty, and `Fonts/Hud.cnb` — which used to be its only entry — no longer exists; `Fonts/ui-16`
replaced it, and `CnaHouseGame::LoadContent` loads that.

Two traps worth knowing before editing a descriptor:

* **`<Size>` is points at 96 dpi, not pixels.** `<Size>16</Size>` is a 21.3 px em and a 29 px line.
* **A character the face cannot draw fails the build**, and Noto Sans Mono has no U+00AD. The
  regions are ASCII plus Latin-1 *minus soft hyphen*: 190 characters. Widening them is how a green
  build becomes a red one.

Compiled sizes, for the budget: `ui-16` 272 kB, `ui-22` 534 kB, `ui-30` 1 058 kB, `mono-13` 272 kB,
`mono-16` 272 kB.


## Build cost, measured (`HOUSE-00182`)

`cna-content` alone, on the `Fonts` root — the largest asset in the tree at 136 kB of compiled
`SpriteFont`:

| | |
|---|---|
| Cold (output removed first) | **0.08 s** |
| Incremental no-op | **0.04–0.05 s** |

Through `cmake --build --target cnahouse_content`, both cold and no-op measure **0.25–0.29 s**, and
that number is Ninja's own overhead across six custom targets rather than the pipeline's: the whole
tree is five assets. The figure worth remembering is the pipeline's, and the reason a no-op is cheap
is visible with `--explain`:

```
[SKIP] Fallback/grey -> .../grey.cnb
  reason: fingerprint and published output digests unchanged
```

It re-checks fingerprints rather than re-reading sources, and it does not scan the output tree. When
`assets-src/` holds a house rather than five fallbacks, that is the property that matters, and it is
recorded now so a later regression has something to be compared against.


## Why `Effects/*.xnb` is committed (BL-04)

`.fx` needs Microsoft's legacy `fxc` at profile `fx_2_0`. CNA embeds no HLSL compiler and no
portable substitute exists — a modern standalone `d3dcompiler` cannot write a legacy effect at all.
On Linux that means Wine plus a DirectX SDK (June 2010) extraction, which is a great deal to ask of
someone who only wants to build the game.

So the compiled `.xnb` is committed **next to the `.fx` that produced it**, and CMake copies it into
`content-fx/` when this machine has no compiler. `cmake` says which route it took:

```
-- cna-house: Tier E effects come from the effect compiler
-- cna-house: Tier E effects come from the committed baseline
```

**Compiling wins when a compiler is available**, and that is the right way round: a source edited
without re-running `build_effects.sh` would otherwise be silently ignored in favour of a stale
committed file, which is the failure every baseline like this eventually produces. On a machine
without one, `build_effects.sh --check` catches the same staleness by hash, without needing to
compile anything.

`COMPILER.txt` is the record that makes the committed bytes accountable: the `cna-content` hash, the
`fxc` hash and size, the Wine version, and the hash of every source and every output. It carries **no
timestamp and no hostname** — a record that changed on every run would produce a diff on every run,
and a file that always has a diff is a file nobody reads.

### Changing an effect

```bash
tools/effects/build_effects.sh --fxc /path/to/fxc.exe   # rebuilds the .xnb and COMPILER.txt
git add assets-src/Effects                              # both, in the same commit
```
