# `chunks.bin` — the pre-batched static geometry

*`HOUSE-00215`. Normative. The writer is `tools/world/build_chunks.py`; there is no other. The
runtime reader is `src/world/ChunkReader.cpp` and `CellRuntime` uploads what it returns
(`HOUSE-00474`). Three checks keep the two honest: the writer's own `read_back`, `ChunkReaderTests`
over bytes built by hand, and `ChunkRoundTripTests` over a fixture this tool writes at build time —
the last is the only one that can see the writer and the reader disagreeing about field order.*

---

## 1. What this file is for

`cna-house.md` §17.4: every cell's static props are grouped and merged into one vertex/index buffer
pair per group, with a bounding box per group and one per **sub-range** so a big group can still be
partially culled. *"Drawing a fully visible kitchen with 140 props costs 5 draw calls, not 140."*

§17.3's draw loop takes it from here: `SetVertexBuffer(chunk.vb)`, `setIndicesProperty(chunk.ib)`,
`setWorldProperty(Matrix::Identity)`. That last call is why every placement in this file is baked
into the vertices — a chunk has no transform, so a prop's position and yaw are in the geometry or
they are nowhere.

## 2. Conventions

`docs/anim-format.md` §2's conventions apply unchanged: little-endian, IEEE-754 binary32, explicit
fixed-width integers, `u16`-length UTF-8 strings, metres, right-handed Y-up with −Z north, no
alignment, no seeking.

## 3. The vertex layouts

**A chunk does not use CNA's model vertex.** §349 measured that at 48 bytes — `Position@0`,
`Normal@12`, `Tangent@24`, `TexCoord0@40` — and it is wrong here in both directions at once:

* it has **no second UV**, and §17.4's chunks are drawn with `DualTextureEffect`, whose entire
  purpose is albedo × lightmap on two channels (the lightmap UV comes from `HOUSE-00205`);
* it has a **normal and a tangent `DualTextureEffect` never reads** — that effect is unlit, the
  lightmap *is* the lighting — so 20 of its 48 bytes would be uploaded, stored in VRAM, and never
  sampled.

A chunk is built by us and uploaded into our own `VertexBuffer`, so it declares what its own effect
reads and nothing more. Chunks are grouped by effect class, so a chunk always has exactly one:

| Id | Name | Attributes | Bytes | Effect |
|---|---|---|---|---|
| 0 | `basic` | `Position` `Normal` `TexCoord0` | 32 | `BasicEffect` |
| 1 | `dual` | `Position` `TexCoord0` `TexCoord1` | 28 | `DualTextureEffect` |
| 2 | `alphatest` | `Position` `TexCoord0` | 20 | `AlphaTestEffect` |

`SkinnedEffect` has no layout here: a skinned prop is an animated one, and animated props are not
batched (§17.4).

The layout comes from the material's **`effectTierS`**, which `cna-house.md` §22.1 puts in the
material record precisely so the effect is stated rather than inferred — inferring it when the data
already says it is a second opinion about the same thing, and two opinions disagree eventually.
When a material omits it, §22.2's class table is the documented fallback (`wood` is
`DualTextureEffect` static and `BasicEffect` dynamic, and only static props are batched, so the
static column is the one that applies). `wet_<class>` and `snow_<class>` resolve to their base
class.

A material that resolves to neither is an **error naming both**, never a silent fallback to
`BasicEffect`: a material quietly drawn with the wrong effect is a rendering bug that presents as
an art bug and gets looked for in the wrong place. A material whose class is `skin` or `fur`, or
whose stated effect is `SkinnedEffect`, is refused outright — a skinned prop is an animated prop,
and §17.4 excludes animated props from batching.

## 4. Layout

| Field | Type | Value |
|---|---|---|
| `magic` | 4 bytes | ASCII `CCHK` |
| `version` | `u32` | **1** |
| `flags` | `u32` | 0. A reader must **reject** any unknown bit |
| `worldHash` | string | `world.manifest.json`'s `worldHash`, or **empty** before `deploy_world.py` has written one. The only string in this format that may be empty: it means the staleness check cannot run, which is different from the file being corrupt |
| `cellCount` | `u32` | |
| `cells` | `cellCount` × string | sorted cell ids |
| `materialCount` | `u32` | |
| `materials` | `materialCount` × string | sorted material ids |
| `chunkCount` | `u32` | |

Then per chunk:

| Field | Type | |
|---|---|---|
| `cell`, `material` | 2 × `u16` | indices into the tables above |
| `layout` | `u8` | §3 |
| `wideIndices` | `u8` | 0 = `u16` indices, 1 = `u32` |
| `bounds` | 6 × `f32` | min *xyz*, max *xyz* |
| `vertexCount` | `u32` | |
| `vertices` | `vertexCount` × the layout's attributes | world space |
| `indexCount` | `u32` | |
| `indices` | `indexCount` × (`u16` or `u32`) | per `wideIndices` |
| `subRangeCount` | `u32` | |
| per sub-range: `prop` | string | the `layout.props.json` id it came from |
| `indexStart`, `indexCount` | 2 × `u32` | |
| `bounds` | 6 × `f32` | **this prop's own box**, not the chunk's |

The sub-range boxes tile the chunk's index buffer exactly — no gaps, no overlaps — and each is its
own prop's. A sub-range carrying the group's box culls nothing and costs 24 bytes to do it.

Textures are not named here. A chunk names its material, and `layout.materials.json` names the
material's albedo; duplicating the path would be a second place for it to be wrong.

## 4a. The shell is chunked too

`HOUSE-00473`. §17.4 was written about "every cell's static props", and a cell's own floor,
ceiling, walls, trim and glass are drawn as well. `build_chunks.py --shell <dir>...` reads the
`.glb` per cell that `tools/blender/house_shell_gen.py` writes and chunks it the same way, with
three differences worth stating:

* **One primitive per surface class, not one mesh.** The exporter splits a cell by material, which
  is exactly what chunking groups by, so the shell is read by a separate function that keeps the
  primitives apart. `read_geometry`, which welds them, stays as it is because that is right for a
  prop.
* **The material is the authored `materialId` in the `.glb`** (`MAT_PAINT_WARM_WHITE`,
  `MAT_GLASS_CLEAR`, …), written by `HOUSE-00907` from the cell palette, opening schedule or fixed
  architectural role. The diagnostic glTF slot name also contains its `surfaceClass`; both values
  live in material `extras`, because the same finish can be a receiver on a floor and detail on a
  step. What it is drawn with comes from the `lightmapReceiver` `HOUSE-00471` writes beside them: a
  baked receiver is `DualTextureEffect`, everything else `BasicEffect`. The material's
  `alphaMode`, however, comes from `layout.materials.json`; glass is no longer guessed from a
  surface-class name.
* **Baked and not baked are different questions.** §18.3 bakes per cell, which is a description of
  an interior; `shell_unwrap.py` skips the yards, decks, roofs and chimney, so their floors and
  walls — receiver classes both — arrive with no `TEXCOORD_1` and §22 lights them directly every
  frame. A receiver class outside a baked cell is therefore `BasicEffect`; a receiver **inside**
  one with no second UV is an error naming `shell_unwrap.py`.

A sub-range's `prop` is the `layout.props.json` id for a prop and `<file>:<surface-class>` for a
surface class of the shell. A shell file whose name is not a cell — `ROOF_MAIN`, `ROOF_GARAGE`, `CHIMNEY` —
draws with **the property's own outdoors**: the largest exterior cell in §27.2's `exterior` pack,
derived rather than named (`HOUSE-00494`). It was the largest exterior cell full stop until then,
which is `EXT_WORLD`, whose pack is `neighbourhood` — so the player's own roof loaded after the
distant houses and was demoted with them, and the house is visible from the road the player starts
on.

**A preferred shell directory older than the one it was derived from is refused.** `build/shell-lm`
is read before `build/shell` and is written by a second Blender tool that nothing runs
automatically; on 2026-09-10 it held cells unwrapped the previous evening, so three shell
regenerations in a row were silently ignored for 78 of the 99 cells and the chunk tree was a
mixture of two days' geometry. The file was there, it parsed, and it had the right name. mtime is
the right signal for this and only this: `build/` is never in git, so inside a build tree a derived
file older than its source is exactly what it looks like.

## 4c. The outdoors is chunked too, and its cell is a residency key

*`HOUSE-00780`.* The exterior generators write one `.glb` per thing — 25 terrain tiles, 22 road
segments, 8 fence runs, 3 gates, 9 garden structures — and none of those names is a cell id, so
until this task every one of them landed in `EXT_WORLD` by `outdoor_cell`'s fallback: 64 files and
20 chunks in the one cell whose residency pack is `neighbourhood`, when §27.2 puts terrain, road,
fences, garden and shed in **`exterior`**. A chunk's pack is its cell's, so where a tile is filed
decides when it is in memory.

Each is now placed in the exterior cell it **covers most of**:

* not the cell containing its centre. A terrain tile is 16 m across and the front porch is 2.7 m;
  the tile over the front of the house has its centre inside the porch and one 46th of its area
  there, and filing it there put the ground the player walks in on into the ground-floor pack.
* not a cell at the wrong **height**. `L1_BALCONY_REAR` is an exterior cell whose plan box sits
  over the back lawn: a ground tile from −0.72 to +0.45 covers it exactly and is 3.65 m below its
  floor. Filed there, the lawn would load with `house-l1` and unload when the player left the
  first floor.
* and never `EXT_WORLD` while any cell of the property overlaps it. The world is the ring outside
  the boundary and the west fence stands on the line, half its posts each side; whichever way that
  arithmetic came out, this property's fence belongs to this property's pack. A road segment 100 m
  away overlaps nothing of the property and lands in the world, which is where it is.

**The cell is a residency key here and not a visibility one.** §25.6 culls the outdoors with a
bounding-volume hierarchy over instances and their own boxes, precisely because `EXT_WORLD` is one
enormous cell that portal traversal cannot help inside — so a terrain tile straddling two yards is
not hidden by the one it is filed under. This house: **61 exterior files, 30 chunks in `exterior`
and 9 in `neighbourhood`** — the latter being the 21 road segments and 8 terrain tiles that are
genuinely beyond the property.

## 4b. Uploading: the declaration is the meaning, the built-in type is a carrier

`HOUSE-00474`. XNA 4.0's `VertexBuffer.SetData<T>` is generic over any struct, so real XNA declares
a `VertexPositionDualTexture : IVertexType` and uploads it. **CNA has no generic**: four concrete
overloads — `VertexPositionColor`, `VertexPositionColorTexture`, `VertexPositionNormalTexture`,
`VertexPositionTexture` — plus a `CNAEXT SetDataRaw` this project may not call (ADR-0001). None of
the four carries two texture coordinates, which is exactly what `DualTextureEffect` reads.

This is not a blocker, because CNA supports the way round it deliberately.
`VertexBuffer::ValidateSetDataRange` says: *"a built-in type's own declaration already describes
exactly that stream, but this buffer may carry any declaration the caller chose, so every declared
element still has to fit in the bytes actually uploaded."* So:

| Layout | Uploaded through | GPU stride | Declaration reads |
|---|---|---|---|
| `basic` | `VertexPositionNormalTexture` | 32 | `Position@0` `Normal@12` `TexCoord0@24` |
| `dual` | `VertexPositionNormalTexture` | 32 | `Position@0` `TexCoord0@12` `TexCoord1@20` |
| `alphatest` | `VertexPositionTexture` | 20 | `Position@0` `TexCoord0@12` |

The carrier's stream is `x y z nx ny nz u v`; for `dual` the declaration reads `nx ny` as TEXCOORD0
and `nz u` as TEXCOORD1, and `v` goes unread. The struct's field names are not what reaches the
GPU. **The cost is four bytes a vertex** — 32 uploaded where this file stores 28 —
and `CellRuntime::ResidentBytes()` counts what was uploaded so the difference is measured rather
than assumed. A generic `SetData<T>` in CNA would remove it.

## 5. Grouping: the architectural key plus switched emitters

§17.4 groups by `(effectClass, material, lightGroupSet, alphaMode)`. Measured against the data
model, three of those four are not independent:

* `alphaMode` is a **field of the material**;
* `class`, which decides `effectClass`, is a **field of the material**;
* `lightGroups` lives on the **cell**, and a chunk never spans two cells.

So for ordinary geometry the key collapses, exactly and without loss, to **the material id**, and
*"≤ 6 chunks per cell"* normally means *"≤ 6 distinct materials among a cell's static props"* —
which is a far more useful sentence to give an author, because it is the thing they control.

`HOUSE-01259` adds one internal fifth discriminator: the switch group of a linked fixture's exact
emissive source slot. It is empty for shell, vegetation and ordinary props. It is deliberately not
serialized as a new chunk-file field: sub-range source prop ids and canonical light links let the
runtime derive the same group, while the build-time discriminator ensures two shades that share a
material but have independent switches can never merge into one draw. The porcelain/wood/etc.
case still batches exactly as before. `--report` prints how many chunks the non-material parts
actually separate rather than assuming that number is zero.

## 6. Where the two limits pull against each other

A group over 65 535 vertices must be split to keep 16-bit indices, and splitting makes more chunks —
which is the other limit. The rules:

* **Split on prop boundaries.** A sub-range is a prop already, and a sub-range straddling two
  buffers could not have one bounding box.
* **A single prop over the cap cannot be split at all**, so that chunk takes 32-bit indices —
  §17.4's *"32-bit is available on EasyGL if needed"*.
* **Both are reported, never resolved silently.** A cell needing eight chunks is an authoring
  problem, and the tool's job is to say which cell and how many materials are in it. `--report`
  and the writer exit non-zero when a cell exceeds the six.
