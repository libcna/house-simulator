# `chunks.bin` — the pre-batched static geometry

*`HOUSE-00215`. Normative. The writer is `tools/world/build_chunks.py`; there is no other. The
runtime reader is `HOUSE-00474`'s `CellRuntime` and does not exist yet — the writer's own
`read_back` is the round trip that keeps the format honest until it does.*

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
| `worldHash` | string | `world.manifest.json`'s `worldHash` |
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

## 5. Grouping: the four-part key has one free part

§17.4 groups by `(effectClass, material, lightGroupSet, alphaMode)`. Measured against the data
model, three of those four are not independent:

* `alphaMode` is a **field of the material**;
* `class`, which decides `effectClass`, is a **field of the material**;
* `lightGroups` lives on the **cell**, and a chunk never spans two cells.

So the key collapses, exactly and without loss, to **the material id**, and *"≤ 6 chunks per cell"*
means *"≤ 6 distinct materials among a cell's static props"* — which is a far more useful sentence
to give an author, because it is the thing they control.

The writer still builds the key from all four parts, so that a schema which later gives a prop its
own light groups needs no change, and `--report` prints how many chunks the other three parts
actually separated. Today that number is zero, and printing it is better than a comment asserting
it.

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
