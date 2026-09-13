# Dog hero asset research

`HOUSE-00291`, retrieved 2026-09-13. The fixed target is `cna-house.md` §60.1: a realistically
proportioned medium Labrador-retriever type, 0.60 m at the withers and 1.05 m nose to tail base,
ultimately carrying a 38-bone rig and the eight clips named by `HOUSE-02043`. As a hero asset it
also needs provenance stronger than an uploader's unchecked declaration (§19.3), must survive a
close-up, and must fit the 22 MB `pets` pack together with the cat, clips and sounds.

This task is research, not acquisition. No candidate below cleared both provenance and quality, so
no third-party bytes were downloaded and no manifest row was created.

## Attempt 1 — Sketchfab, CC0-filtered search

Targeted searches for a downloadable, animated CC0 Labrador produced no candidate that met the
fixed bar. The closest concrete results show why broadening “CC0” to “free” would be a bad escape:

* [Labrador Dog by kenchoo](https://sketchfab.com/3d-models/labrador-dog-1f56cfbab07e4fe49b5d9e521c82073a)
  is the right breed and visually plausible, but its page says **CC BY 4.0**, 52,800 triangles and
  only a simple idle. It also says it is a rebuild of another uploader's model. It therefore
  misses seven of eight clips and has the aggregator chain-of-title weakness that
  [`sketchfab.md`](../licence-evidence/sketchfab.md) already rules out for hero assets.
* [Animated Labrador by AnimalMesh 3D](https://sketchfab.com/3d-models/animated-labrador-3d-animal-model-4d3216da334e4c73a7f55857ff8b1593)
  is rigged and detailed, but the model page says this version is **personal use only** and sends
  commercial users to a paid bundle. Non-commercial content is forbidden by ADR-0012.
* [Labrador Retriever Dog (Game Ready)](https://sketchfab.com/3d-models/labrador-retriever-dog-game-ready-e35d5d53df97405093195b871051104b)
  advertises 22 animations and 35,608 triangles, but is a store listing rather than a CC download.
  Its terms do not permit placing the source model in this public repository.

**Result: rejected.** No CC0 Labrador with the required quality and animation coverage was found;
the plausible candidates fail licence/provenance or clip coverage before a download is justified.

## Attempt 2 — Blend Swap, CC0-filtered search

The strongest per-asset match was
[Dog Low Poly (Rigged)](https://blendswap.com/blend/19009): its page says CC0 and 1,428 vertices,
but the author describes it as a low-poly **Jack Russell**. It lists a rig but no animation clips.
It is a useful distant or prototype animal, not a Labrador-type hero seen at conversational range.

The more complete-looking
[Dog model + textured + rigged + idle animation](https://blendswap.com/blend/25344) is CC BY,
contains only an idle, and is itself derived from another Blend Swap upload. Even a technically
clean download would not resolve the chain of title. Blend Swap explicitly warrants neither
third-party rights nor fitness, as recorded in
[`blendswap.md`](../licence-evidence/blendswap.md), so this source is not accepted for a hero asset.

**Result: rejected.** The CC0 candidate fails breed, realism and animation coverage; the closer
alternative fails provenance and still supplies only one of eight clips.

## Attempt 3 — Quaternius

The first-party [Farm Animal Pack](https://quaternius.com/) contains an animated low-poly pug/dog;
the archived listing describes run, idle, jump and walk. That silhouette and art style are plainly
not §60.1's realistic Labrador, and four generic locomotion clips do not cover the required eight.

There is an independent legal failure. The pack page and current FAQ label models CC0, while the
current [Quaternius Asset License v1.0](https://quaternius.com/license.html) says that neither the
original nor a modified asset may be redistributed as an asset. This repository publishes
`assets-src/`, including modified LODs and the source model, so those statements cannot both govern
the same download. The contradiction and exact clauses are already archived in
[`quaternius-kenney-polypizza.md`](../licence-evidence/quaternius-kenney-polypizza.md).

**Result: rejected.** The available dog is the wrong breed and style, lacks the behaviour-specific
clips, and the source's contradictory current terms do not allow this public source repository to
rely on the CC0 badge.

## Decision — build it ourselves

No sourced model passes the fixed bar. The fallback in risk R-01 is therefore selected, not merely
kept as an option. The already-planned phase-33 chain is the implementation schedule:

1. `HOUSE-02041`: hand-model and texture the Labrador-type dog in Blender, then complete the four
   visual sign-off views at final scale;
2. `HOUSE-02042`: author and validate the 38-bone rig and weights;
3. `HOUSE-02043`: author the eight named clips — idle, sit, lie, walk, trot, `trot_stairs`, bark
   and eat — without importing third-party motion;
4. `HOUSE-02044`: export through the real glTF/`.chanim`/`.cnb` pipeline and verify binding.

The five-day Blender allowance from R-01 covers `HOUSE-02041`–`HOUSE-02043`; the pipeline task is
separate engineering work. The model, textures, rig and animation will be project-authored, so the
published source/derived redistribution question has an unambiguous answer.
