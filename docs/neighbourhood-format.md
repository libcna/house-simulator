# `neighbourhood.bin` — §11.4's meshes, keyed by asset id

*`HOUSE-00856`. Normative. The writer is `tools/world/build_neighbourhood.py`; there is no other.
The runtime reader is `src/world/NeighbourhoodReader.cpp`. Three checks keep the two honest: the
writer's own `read_back`, `NeighbourhoodReaderTests` over bytes built by hand, and
`NeighbourhoodRoundTripTests` over the file this tool writes at build time — the last is the only
one that can see the writer and the reader disagreeing about field order.*

---

## 1. What this file is for

`neighbourhood_gen.py` has written `build/neighbourhood/<ASSET>.glb` since `HOUSE-00841` and
**nothing read that directory** — `grep` found the path twice, both times inside the generator that
writes it. This file is what reads it: one mesh per asset the layout names, in a form
`TitleContainer::OpenStream` can open and a `VertexBuffer` can take.

## 2. Why the neighbourhood is not chunked

`docs/chunk-format.md` §1 states the chunk format's own limit: *"a chunk has no transform, so a
prop's position and yaw are in the geometry or they are nowhere"*. That is right for a prop, which
stands in one place for ever. It is wrong for a neighbour, three times over:

* §11.4 places **122 instances of 34 assets**. Baking the placement writes the same house out once
  per instance — twenty-four houses where there are nineteen meshes, thirty-six impostor cards
  where there are four.
* §26.1 selects an instance's LOD **by its projected height** and §26.2's impostor takes over at
  `impostorFrom`. A baked instance cannot change what it draws, because what it draws is the only
  copy of those vertices in the file.
* §25.4 asks the culler for a **per-category instance list**, not a per-cell chunk list. The whole
  neighbourhood is one category and every member of it is at a different distance.

So the meshes here are in **their own space**, and the transform is applied at draw time:
`Matrix::CreateRotationY(yaw) * Matrix::CreateTranslation(position)`, from the row.

## 3. What is deliberately NOT here

**The instances.** `layout.exterior.json`'s `neighbourhood` rows carry the id, the asset, the
position, the yaw, the LOD group and the impostor distance, and `WorldLoader` already reads them
into `ExteriorContents::neighbourhood`. Writing them here as well would be a second answer to where
a house stands, and two answers disagree eventually. A reader resolves a row by its `asset` string
against this file's asset table; `NeighbourhoodLibrary::UnresolvedAssets` is that lookup done for
every row at once, so a row naming a mesh nobody drew is a named error and not an invisible gap.

**§11.4's vehicles.** `MODEL_PARKED_CAR` and `MODEL_DELIVERY_VAN` are in the same array and are
`HOUSE-00847`'s to deliver. `neighbourhood_gen.is_ours` is the test, and the writer's selftest
asserts those two are the only rows it does not hold — a statement about who owes what, which is
different from "the file is complete".

**Textures.** An asset names its materials and `layout.materials.json` names a material's albedo.
Duplicating the path would be a second place for it to be wrong.

## 4. Conventions

`docs/anim-format.md` §2's conventions apply unchanged: little-endian, IEEE-754 binary32, explicit
fixed-width integers, `u16`-length UTF-8 strings, metres, right-handed Y-up with −Z north, no
alignment, no seeking.

## 5. The vertex layout

One layout, and it is **`docs/chunk-format.md` §3's layout 0** — `Position` `Normal` `TexCoord0`,
32 bytes, `BasicEffect` — so the runtime declares the vertex once and both files use it.

The layout is *proved*, not assumed. §18.3 bakes **rooms**, and a neighbour has no inside to bake:
`shell_unwrap.py` never sees these meshes, so they carry no `TEXCOORD_1` and there is nothing for
`DualTextureEffect`'s second channel to read. `neighbourhood_gen.py` writes
`extras.lightmapReceiver: false` on every material it emits, and the writer **fails** on a `true`
rather than quietly emitting a layout with a UV channel full of zeros.

## 6. Layout

| Field | Type | Value |
|---|---|---|
| `magic` | 4 bytes | ASCII `CNBH` |
| `version` | `u32` | **1** |
| `flags` | `u32` | 0. A reader must **reject** any unknown bit |
| `worldHash` | string | `world.manifest.json`'s `worldHash`, or **empty** before `deploy_world.py` has written one. The only string here that may be empty: it means the staleness check cannot run, which is different from the file being corrupt |
| `materialCount` | `u32` | |
| `materials` | `materialCount` × string | sorted material ids |
| `assetCount` | `u32` | |

Then per asset, **in ascending id order** — the order `wanted_assets` returns, so a reader may
binary-search it:

| Field | Type | |
|---|---|---|
| `asset` | string | `MODEL_NB_HOUSE_A_CREAM`, `MODEL_UTILITY_SPAN`, `MODEL_HORIZON_RIDGE`, … |
| `bounds` | 6 × `f32` | min *xyz*, max *xyz*, in the ASSET's own space |
| `primitiveCount` | `u32` | one per material — what `§17.4` would batch by |

Then per primitive:

| Field | Type | |
|---|---|---|
| `material` | `u16` | an index into the table above |
| `layout` | `u8` | §5. Always 0 today; stated so a later layout is a version bump and not a guess |
| `wideIndices` | `u8` | 0 = `u16` indices, 1 = `u32` |
| `bounds` | 6 × `f32` | **this primitive's** box, not the asset's |
| `vertexCount` | `u32` | |
| `vertices` | `vertexCount` × 32 bytes | `Position` (3 × `f32`), `Normal` (3 × `f32`), `TexCoord0` (2 × `f32`), in that order |
| `indexCount` | `u32` | a multiple of 3 |
| `indices` | `indexCount` × (`u16` or `u32`) | per `wideIndices` |

## 7. What the file holds today

(2026-09-10) **34 assets, 228 834 bytes.** Nineteen house variants at LOD0/LOD1/LOD2, four impostor
cards, seven pieces of street furniture, three horizon cards and the water tower. The largest is a
LOD0 house at 6 primitives and 596 vertices — the sixth is `HOUSE-00849`'s `NB_WINDOW_GLOW`, one
quad per window, which is its own primitive precisely so that whatever draws the neighbourhood can
leave it out by day without touching the rest of the house; the smallest is
`MODEL_NB_IMPOSTOR_GABLE` at one primitive and 7.

## 8. Staleness

`worldHash` is the same field `collision.bin`, `chunks.bin` and `nav.bin` carry, and it means the
same thing: the hash of the deployed layout this file was built from. A runtime that finds it
different from `world.manifest.json`'s has a binary built from a world that no longer exists, and
should say so rather than drawing it.
