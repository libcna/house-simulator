# `.chanim` — the project-owned animation sidecar

*`HOUSE-00166`. Normative. The reader is `HOUSE-00167`; the writer is
`tools/assets/anim_extract.py`; the CI cross-check is `tools/ci/check_anim_assets.py`
(`HOUSE-00225`).*

---

## 1. Why this file exists at all

XNA's own answer to "the framework has no place for my skeleton" was a custom content processor
that wrote a custom type into `Model.Tag`. `cna-house` cannot add a processor to CNA's pipeline
without modifying CNA, which `CLAUDE.md` §3 forbids, so the same custom data travels **beside** the
model instead of inside it. The runtime shape is identical; only the transport differs.

Three consequences, all of them good:

* `BL-01` — the XNB writer emits null `Model::Tag`s — cannot affect data that was never put there.
* ADR-0001 holds without an exception: the runtime reads this file with
  `TitleContainer::OpenStream` and `System::IO::BinaryReader`, both plain XNA 4.0 / .NET API and
  neither of them graphics. CNA's `CNAEXT` `Graphics::SkinningData`, `AnimationClip` and `Keyframe`
  are never named.
* The format is ours, so it can carry the one thing the compiled model does not.

**That one thing is the joint-name list in blend-index order.** `HOUSE-00074` measured it and the
answer was the fallback rather than the hoped-for case:

> Vertex blend indices are **skin-local**, not `Model::Bones` indices. Slot *i* of the skinning
> palette is joint *i* **of this skin**, and nothing in the compiled `Model` reproduces which bone
> that is. The sidecar's `boneNames` list *is* the binding.

Two supporting measurements from the same probe: a compiled `.cnb` is byte-identical across
rebuilds (two builds of one source both hashed `dc702b15…f06379`), so the order is stable; and the
skinned vertex is 68 bytes — the 48-byte static layout plus `BlendWeight` (`Vector4`, offset 48)
and `BlendIndices` (`Byte4`, offset 64), so `Byte4` caps a skin at 256 joints at the vertex level,
well above `SkinnedEffect`'s own 72 (`HOUSE-00077`: 72 accepted, 73 throws).

---

## 2. Conventions that apply to the whole file

| | |
|---|---|
| Byte order | **Little-endian**, always, on every platform. `BinaryReader` reads little-endian, and a byte-order mark would be a second thing to get wrong. |
| Floats | IEEE-754 binary32, little-endian. |
| Integers | Fixed width and explicit — `u32`, `i32`, `u16`. Never `BinaryReader`'s 7-bit-encoded length prefix, which is a .NET-specific encoding and hostile to any other writer. |
| Strings | `u16` byte length, then that many **UTF-8** bytes, no terminator. |
| Matrices | 16 floats, **row-major**, in `Microsoft::Xna::Framework::Matrix`'s own member order (`M11 M12 M13 M14 M21 …`), so a read is a straight copy into the struct. |
| Vectors | `Vector3` is 3 floats *x y z*. `Quaternion` is 4 floats *x y z **w***, in that order — **`w` last**, matching XNA's constructor and glTF, and not the `w`-first order some maths libraries use. |
| Units | Metres and seconds. Times are relative to the start of their own clip. |
| Handedness | Right-handed, +Y up, −Z forward — the glTF convention the source assets use (`HOUSE-00070`). |
| Alignment | None. The format is read sequentially by a `BinaryReader`, never memory-mapped or cast over, so no padding exists and none may be assumed. |

**No section offsets, no index table, no seeking.** The whole file is read forward, once, in the
order below. An offset table is a second description of the layout that can disagree with the
first, and this file is small enough (a full 18-clip character is a few hundred kilobytes) that
random access buys nothing.

---

## 3. Layout

### 3.1 Header

| Field | Type | Value |
|---|---|---|
| `magic` | 4 bytes | `0x43 0x48 0x41 0x4E` — ASCII `CHAN` |
| `version` | `u32` | **1** |
| `flags` | `u32` | 0. Reserved; a reader must **reject** a file with any unknown bit set rather than ignore it |
| `boneCount` | `u32` | 1 … 256 |
| `clipCount` | `u32` | 0 … 4096 |

`version` is a **whole-file** version, not a per-section one. A reader accepts exactly the versions
it knows and reports the number it found in the error; there is no forward compatibility and none is
promised, because both the writer and the reader live in this repository and a version bump is one
commit that touches both.

`boneCount` is capped at 256 because `Byte4` blend indices cannot address more (`HOUSE-00074`).
It is **not** capped at 72: `SkinnedEffect::MaxBones` limits what one draw call can *use*, not what
a skeleton may *contain*, and a character split across several models (§47.0) may exceed it in
total. The 72-bone check belongs at the draw, and `MaterialBinder` makes it there.

### 3.2 Skeleton

Exactly `boneCount` repetitions of:

| Field | Type | Notes |
|---|---|---|
| `name` | string | The glTF joint name. **Position in this list is the blend index.** |
| `parent` | `i32` | Index into this same list; `-1` for a root. Must be `< ` the child's own index |
| `bindPose` | `Matrix` | The joint's **local** bind transform, relative to its parent |
| `inverseBindPose` | `Matrix` | World → joint at bind time. Taken from the source asset offline, **never** from a CNA query |

Then, immediately after the bone records:

| Field | Type | Notes |
|---|---|---|
| `rootBone` | `i32` | Index of the joint locomotion is measured against, or `-1`. Usually the hips |

**Parents strictly precede children.** That is a constraint on the file, not a hint: it makes the
absolute-transform walk a single forward pass with no recursion and no visited set, and it makes a
cycle unrepresentable rather than merely unlikely. A reader rejects `parent >= index`.

**Why store both bind and inverse bind** when one is derivable from the other? Because deriving it
needs a matrix inverse per joint at load, and because a mismatch between them is the single most
useful thing a validator can check: `HOUSE-00075`'s analytic test is that the **bind pose must skin
to the identity for every joint**, and that test only exists if both are present.

### 3.3 Clips

Exactly `clipCount` repetitions of:

| Field | Type | Notes |
|---|---|---|
| `name` | string | Unique within the file. `idle`, `walk_fwd`, … (§47.1) |
| `duration` | `f32` | Seconds, `> 0` |
| `flags` | `u32` | bit 0 = **loops**. Other bits reserved and rejected if set |
| `strideLength` | `f32` | Metres of ground travel per cycle; **0** for non-locomotion clips (§47.4) |
| `footPlantCount` | `u32` | 0 … 64 |
| `footPlants` | `f32` × `footPlantCount` | Contact times in `[0, duration]`, ascending (§48) |
| `keyCount` | `u32` | 1 … 1 048 576 |
| `boneFirstKey` | `u32` × (`boneCount` + 1) | Index of each bone's first key; the last entry is `keyCount` |
| `keys` | key × `keyCount` | See below |

One key:

| Field | Type |
|---|---|
| `time` | `f32` |
| `translation` | `Vector3` |
| `rotation` | `Quaternion` (`x y z w`) |
| `scale` | `Vector3` |

**A key carries no bone index.** `Keyframe::bone` in the runtime struct is filled from
`boneFirstKey`, which is the same information without repeating it 100 000 times — a key is 40 bytes
and a bone index would add 10 % to the largest section of the file for something already implied by
position. Keys are stored **sorted by bone, then by time**, which is what makes `boneFirstKey` a
valid partition and what lets the sampler binary-search one bone's track without touching another's.

`boneFirstKey` has `boneCount + 1` entries so that bone *b*'s track is exactly
`[boneFirstKey[b], boneFirstKey[b + 1])` with no special case for the last bone. A bone with no keys
has an empty half-open range, which is legal: it holds its bind pose.

**Scale is stored even though this project never scales a joint.** Three floats per key against the
alternative — a format that cannot represent a retargeted asset that does scale, discovered halfway
through phase 37. The reader does not special-case a unit scale.

---

## 4. What a reader must reject

A `.chanim` is content, and `docs/conventions.md` §5.4 makes malformed content a **recoverable**
failure that names the file, never an assertion and never a silent default. Every one of these is a
distinct error message:

| Condition | Why it matters |
|---|---|
| `magic != "CHAN"` | The wrong file entirely; say so rather than reading garbage lengths |
| `version` not known | Name the version found and the versions accepted |
| any reserved flag bit set | A newer writer put something here that this reader would silently drop |
| `boneCount == 0` or `> 256` | Zero is not a skeleton; 256 is the `Byte4` ceiling (`HOUSE-00074`) |
| `parent < -1` or `parent >= index` | Forward reference or cycle |
| a duplicate bone name | The name list *is* the binding; a duplicate makes it ambiguous |
| `duration <= 0` | A clip that cannot be sampled |
| `boneFirstKey` not non-decreasing, or `back() != keyCount` | The partition is broken; every later index is wrong |
| a key time outside `[0, duration]` | Unsamplable, and usually a units error |
| key times within one bone not ascending | The sampler binary-searches; unsorted input silently returns the wrong pose |
| a foot-plant time outside `[0, duration]` | Silently breaks the stride-length correction of §48 |
| the stream ending early | Truncation must be an error, not a short skeleton |

**And one that is not the reader's job.** Whether the bone names match the model's is checked by
`ClipLibrary::BindTo(const Model&)`, not by the reader, because the reader has no model. A mismatch
there is a **fatal content error naming the offending joint** (`HOUSE-00167`) — never a silent
deformation, which is what the same mistake produces in every engine that resolves joints
positionally.

---

## 5. Worked size

A body character: 62 joints, 18 clips, keys at 30 Hz.

| | |
|---|---|
| Header | 20 B |
| Skeleton | 62 × (≈ 20 B name + 4 + 64 + 64) ≈ **9.4 kB** |
| One 1.06 s clip (`walk_fwd`) | 62 bones × 32 keys × 40 B + 252 B index ≈ **79 kB** |
| 18 clips, averaging 2 s | ≈ **2.7 MB** |

Larger than it needs to be, and deliberately not compressed in version 1. Compression is a decision
with a measurement behind it, and there is no measurement yet; §27.2's budget has room, and a format
that is trivially readable in a hex dump is worth a great deal while the writer is new.

---

## 6. Relationship to the runtime types

The file is a direct serialisation of `cnahouse::anim::Skeleton` and `cnahouse::anim::Clip`
(`cna-house.md` §47.0) with two deliberate differences:

* `Skeleton::modelBoneIndex` is **not** in the file. It is filled by `BindTo` at load, from the
  model actually being drawn, and storing it would be storing a claim about a file this one has
  never seen.
* `Keyframe::bone` is **not** in the file, for the reason in §3.3.

Everything else is one field, one write, one read, in this order.
