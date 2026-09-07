# `snowshell.bin` — the surfaces snow settles on

*`HOUSE-00214`. Normative. The writer is `tools/world/build_snowshell.py`; there is no other. The
runtime reader is `HOUSE-01793`'s snow accumulation and does not exist yet — the writer's own
`read_back` is the round trip that keeps the format honest until it does.*

---

## 1. What this file is for

`cna-house.md` §38: accumulation is drawn as a **snow shell** — *"an offline-generated duplicate of
every up-facing exterior surface (terrain tiles, roofs, the porch and balcony decks, fence rails,
garden furniture tops, the car's roof and bonnet), offset along the surface normal by `snowDepth`
and drawn with a white snow material at `alpha = smoothstep(0.002, 0.03, snowDepth)`. Faces steeper
than each material's `snowResponse.slopeLimitDeg` are excluded from the shell at generation time,
so snow does not cling to walls."*

The shell's material is §22.2's `snow_<class>` for the base material's class.

## 2. Conventions

`docs/anim-format.md` §2's conventions apply unchanged.

## 3. §38 asks for two things a static buffer cannot both have

*"Offset along the surface normal by `snowDepth`"* and *"it needs no shader"* are compatible only
while `snowDepth` is fixed, and it is not — it integrates from 0 to 0.35 m and back. Baking one
offset in picks a depth and is wrong at every other: at 0.35 m the shell floats a hand's width
above the roof while the alpha says it is barely there.

So the file carries **base positions and normals, unoffset**, and the runtime computes
`p + n · snowDepth`. That is one multiply-add per vertex, on a buffer it rewrites only when the
depth has actually moved: `dD/dt = 0.0009 · intensity` puts a full 0 → 0.35 m at about 390 s, so a
1 cm rebuild threshold fires roughly every 11 s in the heaviest snowfall and never otherwise. Tier E
does the same sum in its vertex shader from the same two attributes and rebuilds nothing.

The plan agrees: `HOUSE-01793` is "implement the snow-shell **rendering** driven by `snowDepth`"
and `HOUSE-01794` is "implement the snow **displacement** of the shell along the surface normal by
`snowDepth`" — two runtime tasks, so the displacement was never this file's to bake in.

`maxDepth` (0.35, §38's cap) travels in the header so generation and the runtime agree on the range
the shell was built for.

## 4. Layout

| Field | Type | Value |
|---|---|---|
| `magic` | 4 bytes | ASCII `CSNW` |
| `version` | `u32` | **1** |
| `flags` | `u32` | 0. A reader must **reject** any unknown bit |
| `worldHash` | string | `world.manifest.json`'s `worldHash` |
| `maxDepth` | `f32` | 0.35 |
| `cellCount` | `u32` | |
| `cells` | `cellCount` × string | |
| `materialCount` | `u32` | |
| `materials` | `materialCount` × string | the **base** material; the shell draws with `snow_<its class>` |
| `shellCount` | `u32` | |

Then per shell:

| Field | Type | |
|---|---|---|
| `cell`, `material` | 2 × `u16` | indices into the tables above |
| `wideIndices` | `u8` | 0 = `u16` indices, 1 = `u32` |
| `bounds` | 6 × `f32` | |
| `area` | `f32` | m², for the budget report |
| `vertexCount` | `u32` | |
| `vertices` | `vertexCount` × (`position` 3×`f32`, `normal` 3×`f32`, `uv` 2×`f32`) | 32 bytes |
| `indexCount` | `u32` | |
| `indices` | `indexCount` × (`u16` or `u32`) | |

## 5. The rules

**The slope test is against +Y, not |Y|.** A deck's underside is as flat as its top and points the
other way; testing `abs(dot(n, up))` would hang a snow shell under the balcony, which is the one
artefact this technique exists to make impossible. Measuring the angle from +Y makes "snow does not
cling to walls" (90°) and "snow does not cling to soffits" (180°) the same comparison.

**The limit is the material's.** `snowResponse.slopeLimitDeg`, from §22.1's record. A material with
no `snowResponse` gets a documented 40° default — §22.1's own example value — and is **named in the
report**, never a silent 90 that snows on walls. `snowResponse.coverable: false` excludes the
surface at any pitch, which is what glass and water are for.

**The normal written is the FACE normal.** The slope test is a test of a face, and a vertex is in
this file only because the face it belongs to passed. Offsetting along a *smoothed* vertex normal
would push it off the surface it was accepted for, and at a hard edge two faces would pull the
shared vertex in a compromise direction and open a gap along the crease at every depth. The shell
is flat-shaded by construction, and vertices are welded on **(position, face normal)** — one vertex
per smooth join, split at every hard edge. It also means a source model's authored normals, which
may be smoothed or simply wrong, cannot move snow off a roof.

**Degenerate faces are rejected and counted as such.** A zero-area triangle has no normal, so it
would otherwise fall through the slope test as "facing down" and be miscounted — and it is a
triangle the GPU still fetches and rasterises to nothing.

## 6. What is not in it yet

Two of §38's sources do not exist to read, and the report **names them** rather than quietly
producing a shell that covers half of what §38 lists:

* **roofs** — the shell geometry arrives with `HOUSE-00470`;
* **terrain tiles** — `layout.exterior.json`'s height field is not authored yet.

What is covered today is every open cell's deck (`kind: exterior` or `visibilityHint: open` — the
porch, the terrace, the balconies) and the up-facing faces of the static props standing in them.
