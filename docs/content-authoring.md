# Content authoring checklist

For whoever makes an asset — modelled, downloaded, derived or generated. Every rule here exists
because breaking it costs someone else an afternoon. The authoring conventions are
`cna-house.md` §18.2; this is the same set in the order you actually need them.

The pipeline is two stages and you only touch the first:

```
assets-src/  --(cna-content build)-->  content/  --(ContentManager::Load<T>)-->  the game
```

`content/` is generated and git-ignored. Never edit anything in it, and never commit anything to it
outside the tiny committed baseline (`cna-house.md` §18.4).

---

## 1. Before you make or fetch anything

- [ ] **Check it does not already exist.** `assets-src/assets.manifest.json` is the inventory.
- [ ] **Check the licence first, not last.** No provable provenance means the asset cannot ship —
      see [ADR-0012](decisions/ADR-0012-asset-licensing.md). `redistributeDerived = false` (ND
      terms) means it cannot even be decimated for LODs, so it is unusable here whatever it looks
      like.
- [ ] **Check the budget.** `cna-house.md` §72 gives the memory and triangle budgets by category.
      A hero prop and a doorstop are not allowed the same texture.

---

## 2. Models

- [ ] **Format: `.glb`** — binary glTF, one logical object per file. Single-file provenance,
      hashable, no missing sidecar textures.
- [ ] **One skin and one skeleton per file** for skinned characters. The pipeline groups mesh
      placements by skin and one group becomes one `Model`; a second skin in the same file is a
      build error (`HOUSE-00225`). Split it offline instead.
- [ ] **Units: metres. Up: +Y. Front: +Z.** This matches the world (`cna-house.md` §14) and glTF,
      so nothing is converted at import.
- [ ] **Origin at the support point** — where the object touches the floor — so that placement in
      the layout is a plain position plus a yaw. Wall-mounted objects put the origin **at the wall
      plane** instead.
- [ ] **Front faces wound counter-clockwise**, the glTF convention. The game binds
      `CullClockwise` for every model draw; procedurally generated geometry follows the same
      convention so one state serves everything.
- [ ] **Scale is real.** A door leaf is 0.86 m wide, a counter is 0.92 m high, a mug is 95 mm. The
      realism validator (`cna-house.md` §70.5) checks the ones it can; a reviewer checks the rest
      against a reference photograph.
- [ ] **LODs**: every model carries `LOD0`. If `LOD0` exceeds 4 000 triangles, it also carries
      `<name>_LOD1` and `<name>_LOD2` as extra glTF meshes. `tools/blender/lod_gen.py` generates
      them; check the result rather than trusting it.
- [ ] **Collision proxy**: a separate mesh named `<name>_COL`, convex or box-decomposed, at most 64
      triangles. The build extracts it into the collision file and strips it from the runtime
      model. No proxy means the object is not solid.
- [ ] **Animated props** keep their animations as glTF animations in the same file; skinned
      characters get their clips as a project-owned `.chanim` sidecar
      (`docs/anim-format.md`, authored in phase 37).
- [ ] **Names are `snake_case`**: `fridge_01.glb`, `oak_chair_02.glb`.

## 3. Textures

- [ ] **Format: PNG.** Base colour is **sRGB**; normal, roughness, metallic, occlusion and mask
      maps are **linear**. Getting this backwards is the single most common asset defect and it is
      invisible until lighting looks wrong everywhere.
- [ ] **Powers of two**, and no larger than the budget allows: ≤ 2048 for hero props, ≤ 1024 for
      common props, ≤ 512 for small props, ≤ 256 for tiny ones.
- [ ] **Embedded in the `.glb`** for props. Shared architectural materials are referenced
      externally so the atlas of shared surfaces stays deduplicated.
- [ ] **Mip chains ship pre-generated** in content. Do not rely on runtime mip generation: it is
      correct on EasyGL and a silent no-op on other renderers (BL-08).
- [ ] **Alpha**: cut-out foliage and lattice use a hard alpha mask for `AlphaTestEffect`; only
      genuinely transparent surfaces (glass, water, curtains) carry graded alpha, and they cost a
      back-to-front sort.

## 4. Audio

- [ ] **16-bit PCM WAV.** 24-bit PCM is rejected by the content pipeline (BL-06) — every file in
      the NOX collection is `pcm_s24le` and must be converted:
      `ffmpeg -i in.wav -c:a pcm_s16le -ar 44100 out.wav`. Record **both** hashes in the manifest.
- [ ] **44.1 kHz** unless there is a reason; mono for anything positioned in the world, stereo only
      for ambience beds and music.
- [ ] **Trim the silence** at both ends, normalise to a consistent peak, and remove DC offset. A
      sound that starts 80 ms late feels like input lag.
- [ ] **Loops are seamless** and marked as loops in the manifest.
- [ ] **Name by what it is, not where it came from**: `door-close-heavy`, not `Door_Slam_03_NOX`.

## 5. Fonts

- [ ] **TTF**, through a `.spritefont` descriptor, compiled to a `SpriteFont`.
- [ ] The licence must permit embedding. SIL OFL does; many free-for-personal-use fonts do not.
- [ ] Include every glyph the UI uses, including the degree sign, the times sign and the arrows
      the HUD prompts need.

## 6. Effects (`.fx`)

- [ ] **`assets-src/Effects/` and nowhere else.** `check_xna_only.py` rejects a `.fx` outside it.
- [ ] Target `vs_3_0`/`ps_3_0` (`profile: hidef`). No geometry or tessellation stages: GLES 3.0 and
      WebGL 2 have neither.
- [ ] **No GLSL, no SPIR-V, no WGSL anywhere in the tree.** That would mean `ShaderEffect`, which
      is Tier C ([ADR-0001](decisions/ADR-0001-xna-only.md)).
- [ ] Every effect has a **named Tier-S fallback** ([ADR-0003](decisions/ADR-0003-render-tiers.md))
      and a render test that draws the same scene both ways.
- [ ] **Commit the compiled `.xnb`** beside the `.fx`, so a contributor without Wine can build
      (BL-04).

## 7. World data

- [ ] JSON under `assets-src/world/`, keys `camelCase`, ids matching the grammars in
      [`conventions.md`](conventions.md), schema header present.
- [ ] `tools/world/validate_world.py` passes. It is not advisory: a failure fails the build.
- [ ] See [`world-format.md`](world-format.md) for the per-file reference.

## 8. Every asset, before you commit

- [ ] A **manifest row** in `assets-src/assets.manifest.json` — id, category, source file,
      SHA-256, origin block with licence and URL and retrieval date, geometry block, `usedIn`,
      `residencyPack`. **No row, no build.**
- [ ] The **licence text** copied into `licenses/<slug>/LICENCE.txt` if the slug is new, with
      `source.url.txt` and `retrieved.txt` beside it.
- [ ] `tools/assets/verify_licences.py` and `tools/ci/check_manifest.py` pass.
- [ ] **Hero assets** additionally need a sign-off in `docs/asset-review/` — four views, a
      reference photograph, a verdict, a reviewer and a date. The template is
      [`asset-review/TEMPLATE.md`](asset-review/TEMPLATE.md).
- [ ] The content build is **reproducible**: `make content-verify` rebuilds everything and asserts
      the hashes match. Anything non-deterministic in your tool is a bug in your tool.

---

## Rejection criteria

An asset is rejected, however good it looks, if it: has no provable provenance; carries ND or NC
terms; is at the wrong scale; has non-manifold or inverted geometry; has a normal map in the wrong
colour space; exceeds its category budget; has no collision proxy where it needs one; carries a
second skin; or depicts an identifiable real private person (`cna-house.md` §59.4).
