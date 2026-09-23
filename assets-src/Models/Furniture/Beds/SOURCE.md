# Bed-frame sources

`HOUSE-00979` uses exactly two models from 3D Assets' CC0 [Bedroom and Living Room
Furniture](https://3dassets.dev/packs/bedroom-and-living-room-furniture) pack, retrieved
2026-09-23:

- asset 38770, *Double Bed Upholstered*, a 160 cm double bed;
- asset 38771, *Single Bed Timber*, a 90 cm single bed.

The official pack manifest declares Muse Spark via T3 Code as its AI generator and releases both
models under CC0 1.0. Exact CDN URLs and source SHA-256 values are pinned in
`tools/assets/bed_frames_prepare.py` and repeated in `assets-src/assets.manifest.json`. Source
downloads are not shipped. Run the preparation tool with `--download --cache PATH`, then `--write
--cache PATH`, to reproduce the committed derivatives.

Both models are static, include their frame and restrained bedding, and receive one enclosing
12-triangle collision proxy. A bounded vertical normalization brings their measured mattress plane
to 0.60 m while preserving authored width and depth. Room recipes reuse the single frame for the
child's bed and vary bedding through existing transform/tint data; there is no separate child model
and no interaction behavior. Licence: `licenses/cc0-1.0/LICENCE.txt`.
