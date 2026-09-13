# Cat hero asset research

`HOUSE-00292`, retrieved 2026-09-13. The fixed target is `cna-house.md` §61: a realistically
proportioned domestic cat, 0.25 m at the shoulder, 0.46 m through the body and 4.2 kg, ultimately
carrying a 34-bone rig and the eleven clips named by `HOUSE-02093`. As a hero asset it also needs
provenance stronger than an uploader's unchecked declaration (§19.3), must survive close-up views
on furniture and perches, remain within 14,000 LOD0 triangles, and share the 22 MB `pets` pack with
the dog, clips and sounds.

This task is research, not acquisition. No candidate below cleared provenance, appearance and
animation coverage together, so no third-party bytes were downloaded and no manifest row was
created.

## Attempt 1 — Sketchfab, CC0-filtered search

Targeted searches for a downloadable, animated CC0 domestic cat produced no candidate that met the
fixed bar. The strongest concrete results establish the gap:

* [Animalia — Domestic Cat](https://sketchfab.com/3d-models/animalia-domestic-cat-2c5f3efa01d54ba29ba5f2a951a4f2db)
  is the closest technical match: 8,300 triangles, four colour variants and 110 animations. Its
  page is a non-downloadable showcase, however; it displays neither a Creative Commons licence nor
  source files that this repository may redistribute. A preview is not an acquirable asset.
* [Animated Cat by AnimalMesh 3D](https://sketchfab.com/3d-models/animated-cat-3d-animal-model-c86d90e98e8e467e92c908206e1ee667)
  is downloadable and visually plausible, but its description restricts the free version to
  **personal use only** and requires a paid Fab or Patreon version for commercial use. That direct
  restriction also conflicts with the page's generic CC-BY label; either reading fails ADR-0012's
  unambiguous-commercial-use requirement.
* [Lowpoly Cat Rig with Run](https://sketchfab.com/3d-models/lowpoly-cat-rig-run-animation-c36df576c9ae4ed28e89069b1a2f427a)
  is CC-BY but only 794 triangles and supplies one run cycle. It misses the close-up appearance and
  ten of the eleven required clips.

**Result: rejected.** The capable model is not offered under downloadable terms; the available
models are personal-use or visibly low-poly with only one clip. Sketchfab is also an uploader-led
aggregator that [`sketchfab.md`](../licence-evidence/sketchfab.md) excludes from hero assets.

## Attempt 2 — Blend Swap, CC0-filtered search

[Cat by squibblejack](https://blendswap.com/blend/23047) is a concrete CC0 result and its page says
the mesh is rigged, but declares no animation at all. The 6.98 MB Blender file relies on Cycles and
fluffy hair; even before a technical conversion, it cannot satisfy an eleven-clip task.

The same author's [Toon Cat](https://blendswap.com/blend/22931) is CC0 and has NLA action strips,
but is explicitly toon-styled and does not name a complete clip set. The most popular
[Rigged and animated Cat](https://blendswap.com/blend/18519) is CC-BY, uses dynamic hair and names
only walk and run cycles. None demonstrates `groom`, perch jumps, `knead`, `stretch` or `meow`.

**Result: rejected.** The CC0 candidates fail the realistic style or animation bar, and the closer
animated candidate supplies only two generic locomotion clips. Blend Swap expressly provides no
third-party-rights warranty, as archived in [`blendswap.md`](../licence-evidence/blendswap.md), and
this project's existing verdict excludes it from hero assets regardless.

## Attempt 3 — Quaternius

The first-party
[Ultimate Animated Animal Pack](https://quaternius.com/packs/ultimateanimatedanimals.html)
contains a cat among twelve low-poly animals and says each has more than twelve animations. The
examples are generic attack, death, kick, gallop, walk and jump actions: the page does not establish
the cat-specific `groom`, `knead`, `stretch` and `meow` coverage. More fundamentally, the deliberately
faceted low-poly style is incompatible with a cat repeatedly seen at close range on twenty-two
perches.

There is also an independent legal failure. The pack page and current FAQ label models CC0, while
the current [Quaternius Asset License v1.0](https://quaternius.com/license.html) says that neither
the original nor a modified asset may be redistributed as an asset. This public repository ships
`assets-src/`, including modified LODs and the source model. The contradictory clauses are archived
in [`quaternius-kenney-polypizza.md`](../licence-evidence/quaternius-kenney-polypizza.md); the project
does not possess a copy proven to have been obtained under the pack's earlier CC0 terms.

**Result: rejected.** The cat is the wrong visual style, its named animation coverage is
insufficient, and the source's contradictory current terms cannot support publishing source and
derived files here.

## Decision — build it ourselves

No sourced model passes the fixed bar. Risk R-02's build-it-ourselves fallback is selected and the
already-planned phase-34 chain is its implementation schedule:

1. `HOUSE-02091`: hand-model and texture the domestic cat in Blender, then complete the four visual
   sign-off views at final scale;
2. `HOUSE-02092`: author and validate the 34-bone rig and weights;
3. `HOUSE-02093`: author the eleven named clips — `idle`, `sit`, `lie`, `groom`, `walk`, `trot`,
   `jump_up`, `jump_down`, `stretch`, `knead` and `meow` — without third-party motion;
4. `HOUSE-02094`: export through the real glTF/`.chanim`/`.cnb` pipeline and verify binding.

The model, textures, rig and animation will be project-authored. This resolves both the hero
provenance requirement and redistribution of the source and derived assets without adding a
duplicate task to the permanent ledger.
