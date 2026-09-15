# First L0 living/family furniture kit

Source: [gkjohnson/3d-demo-data Bitterli room resources](https://github.com/gkjohnson/3d-demo-data/tree/main/models/bitterli-rendering-resources), retrieved 2026-09-15. The White Room by Jay-Artist and The Grey & White Room by Wig42 are CC BY 3.0 as stated in the upstream README. Per-object geometry came from the proven `libcna/living-room-simulator` extraction; exact source hashes, authors, material maps and licence terms live in `assets-src/assets.manifest.json`.

The White Room (Jay-Artist): MODEL_FURNITURE_COFFEE_TABLE, MODEL_FURNITURE_FLOOR_LAMP, MODEL_FURNITURE_LEATHER_ARMCHAIR_A, MODEL_FURNITURE_LEATHER_ARMCHAIR_B, MODEL_FURNITURE_LEATHER_SOFA, MODEL_FURNITURE_RUG, MODEL_FURNITURE_TV, MODEL_FURNITURE_TV_UNIT.

The Grey & White Room (Wig42): MODEL_FURNITURE_POTTED_PLANT_A.

Five heavier meshes received LOD1/LOD2 and enclosed `_COL` proxies through the repository's Blender tools. The source material names and UVs remain intact. See `docs/asset-selection/visual-slice-furniture.md` and `docs/licence-evidence/bitterli-rendering-resources.md` for quality and licence decisions.

`tools/assets/gltf_sampler_compat.py` deterministically changed embedded-image samplers from
mipmapped minification to the same nearest/linear choice without mipmaps, matching CNA's
single-level glTF import. It also removed stale transmission/volume declarations. The TV and
media-unit decorative glass slots declared physical transmission that this strict XNA static path
cannot reproduce; their original textured base colour is intentionally used as an opaque fallback
instead of CNA's almost-invisible alpha approximation. The individual source hashes above are for
these import-compatible, geometry-preserving derived GLBs.
