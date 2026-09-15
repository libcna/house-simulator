# Base-colour images for the first L0 furniture kit

These images were extracted from the selected Bitterli scene GLBs; no unrelated texture was substituted. Source/licence: [upstream scene README](https://github.com/gkjohnson/3d-demo-data/blob/main/models/bitterli-rendering-resources/README.md), retrieved 2026-09-15. The White Room by Jay-Artist and The Grey & White Room by Wig42 are CC BY 3.0.

The White Room (Jay-Artist): TEXTURE_FURNITURE_CUSHION_GREEN, TEXTURE_FURNITURE_CUSHION_PURPLE, TEXTURE_FURNITURE_CUSHION_PURPLE_YELLOW, TEXTURE_FURNITURE_LAMP_SHADE, TEXTURE_FURNITURE_RUG_CHARCOAL, TEXTURE_FURNITURE_WHITE_ROOM_PALETTE.

The Grey & White Room (Wig42): TEXTURE_FURNITURE_GREY_ROOM_PALETTE, TEXTURE_FURNITURE_PLANT_LEAF.

Each file has its own SHA-256 and author/licence row in `assets-src/assets.manifest.json`; the canonical furniture materials use these sRGB base-colour images through ordinary stock XNA effects. Six non-power-of-two embedded images were Lanczos-resampled to 1024² (cushions, shade and rug) or 512² (leaf) without changing normalized UV correspondence. The leaf remains RGBA. This is required by the stock XNA Reach sampler when wrapping; the first real gameplay capture proved that leaving the 800² image untouched crashes the frame.
