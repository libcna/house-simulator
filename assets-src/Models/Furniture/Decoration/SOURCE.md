# Interior decoration sources (`HOUSE-00982`)

Rule R8 leaves the existing ground-floor plant and frame families in service. This bounded group
adds only the four forms still named by `docs/furnishing-kit.md`, within the six-model cap:

* `floor_mirror.glb` derives from **Dressing Mirror Round**, asset 38808 in the official 3D Assets
  *Bedroom and Living Room Furniture* pack (Muse Spark via T3 Code).
* `wall_clock.glb` derives from **Wall clock**, asset 25552 in the official 3D Assets
  *Laundromat and Cleaning Services* pack (Claude Fable 5.1).
* `plant_group.glb` derives from **Potted Plant Group**, asset 36579 in the official 3D Assets
  *Indian Bazaar Street and Temple* pack (Claude Opus 5).
* `narrow_vase.glb` derives from **Narrow Neck Vase**, asset 29517 in the official 3D Assets
  *Ceramics Pottery Studio* pack (GPT-6).

3D Assets publishes each source under CC0 1.0. `tools/assets/decoration_prepare.py` pins the
official CDN bytes, bakes static geometry and hierarchy, adds UV0 and one enclosing collision box,
and validates scale and support/wall origins. The plant's unused WebP preview nodes are removed
because its named source slots bind the canonical project materials. The four `wall_art_*.glb`
files are deterministic
frame variants for the four-image CC0 set documented beside its textures. Existing generated entry
and hall rug planes reuse ambientCG's CC0 Fabric061 weave; no new rug model or texture is needed.
