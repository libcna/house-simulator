# Vegetation — provenance

`HOUSE-00297`. All outputs in this directory are derived from the specifically named
Poly Haven assets below. The source 1K glTFs are cache inputs and are not committed.

| Output manifest ids | Upstream asset | Canonical URL |
|---|---|---|
| `MODEL_VEGETATION_TREE_FIR_SAPLING`, `MODEL_VEGETATION_TREE_FIR_YOUNG`, `MODEL_VEGETATION_TREE_FIR_MATURE` | Fir (`fir_sapling`) | <https://polyhaven.com/a/fir_sapling> |
| `MODEL_VEGETATION_TREE_PINE_SAPLING`, `MODEL_VEGETATION_TREE_PINE_YOUNG`, `MODEL_VEGETATION_TREE_PINE_MATURE` | Pine (`pine_sapling_small`) | <https://polyhaven.com/a/pine_sapling_small> |
| `MODEL_VEGETATION_TREE_TREE_SMALL_02_SAPLING`, `MODEL_VEGETATION_TREE_TREE_SMALL_02_YOUNG`, `MODEL_VEGETATION_TREE_TREE_SMALL_02_MATURE` | Burkea africana / wild syringa (`tree_small_02`) | <https://polyhaven.com/a/tree_small_02> |
| `MODEL_VEGETATION_TREE_JACARANDA_SAPLING`, `MODEL_VEGETATION_TREE_JACARANDA_YOUNG`, `MODEL_VEGETATION_TREE_JACARANDA_MATURE` | Jacaranda (`jacaranda_tree`) | <https://polyhaven.com/a/jacaranda_tree> |
| `MODEL_VEGETATION_TREE_SEARSIA_BURCHELLII_SAPLING`, `MODEL_VEGETATION_TREE_SEARSIA_BURCHELLII_YOUNG`, `MODEL_VEGETATION_TREE_SEARSIA_BURCHELLII_MATURE` | Searsia burchellii (`searsia_burchellii`) | <https://polyhaven.com/a/searsia_burchellii> |
| `MODEL_VEGETATION_TREE_SEARSIA_LUCIDA_SAPLING`, `MODEL_VEGETATION_TREE_SEARSIA_LUCIDA_YOUNG`, `MODEL_VEGETATION_TREE_SEARSIA_LUCIDA_MATURE` | Searsia lucida (`searsia_lucida`) | <https://polyhaven.com/a/searsia_lucida> |
| `MODEL_VEGETATION_SHRUB_01` | Shrub 01 (`shrub_01`) | <https://polyhaven.com/a/shrub_01> |
| `MODEL_VEGETATION_SHRUB_02` | Shrub 02 (`shrub_02`) | <https://polyhaven.com/a/shrub_02> |
| `MODEL_VEGETATION_SHRUB_03` | Shrub 03 (`shrub_03`) | <https://polyhaven.com/a/shrub_03> |
| `MODEL_VEGETATION_SHRUB_04` | Shrub 04 (`shrub_04`) | <https://polyhaven.com/a/shrub_04> |
| `MODEL_VEGETATION_FERN_02` | Fern 02 (`fern_02`) | <https://polyhaven.com/a/fern_02> |
| `MODEL_VEGETATION_NETTLE_PLANT` | Nettle plant (`nettle_plant`) | <https://polyhaven.com/a/nettle_plant> |
| `MODEL_VEGETATION_WEED_PLANT_02` | Weed plant 02 (`weed_plant_02`) | <https://polyhaven.com/a/weed_plant_02> |
| `MODEL_VEGETATION_SHRUB_SORREL_01` | Shrub sorrel 01 (`shrub_sorrel_01`) | <https://polyhaven.com/a/shrub_sorrel_01> |
| `MODEL_VEGETATION_WILD_ROOIBOS_BUSH` | Wild rooibos bush (`wild_rooibos_bush`) | <https://polyhaven.com/a/wild_rooibos_bush> |
| `MODEL_VEGETATION_CELANDINE_01` | Celandine 01 (`celandine_01`) | <https://polyhaven.com/a/celandine_01> |
| `MODEL_VEGETATION_DANDELION_01` | Dandelion 01 (`dandelion_01`) | <https://polyhaven.com/a/dandelion_01> |
| `MODEL_VEGETATION_PERIWINKLE_PLANT` | Periwinkle plant (`periwinkle_plant`) | <https://polyhaven.com/a/periwinkle_plant> |
| `MODEL_VEGETATION_FLOWER_GAZANIA` | Gazania (`flower_gazania`) | <https://polyhaven.com/a/flower_gazania> |
| `MODEL_VEGETATION_FLOWER_STINKKRUID` | Stinkkruid (`flower_stinkkruid`) | <https://polyhaven.com/a/flower_stinkkruid> |
| `MODEL_VEGETATION_GRASS_BERMUDA_01` | Bermuda grass 01 (`grass_bermuda_01`) | <https://polyhaven.com/a/grass_bermuda_01> |
| `MODEL_VEGETATION_GRASS_MEDIUM_02` | Grass medium 02 (`grass_medium_02`) | <https://polyhaven.com/a/grass_medium_02> |

## Licence

Poly Haven declares every asset CC0 1.0 and warrants that assets are original work of its
staff or artists who directly donated/sold the work. The project verified those terms in
`docs/licence-evidence/polyhaven.md`; the full licence is
`licenses/cc0-1.0/LICENCE.txt`. Source and derivatives may be redistributed, modified and
used commercially; attribution is not required. Retrieved 2026-09-13.

## Modification

`tools/assets/polyhaven_vegetation.py` pins every API metadata response, verifies every
downloaded byte against its declared MD5, keeps only base-colour/alpha data needed by the
stock XNA path, reduces textures to 256 px, and invokes Blender to centre, ground, scale
and decimate each model. Tree ages are separate derived meshes; LOD1 and LOD2 use §26.1's
0.35 and 0.12 triangle ratios. The exact measurements are in
`docs/asset-selection/vegetation-set.json`.

Poly Haven preview renders are deliberately absent: its ToS reserves them. Review images
are rendered locally from these CC0-derived GLBs.
