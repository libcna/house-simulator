# Utility appliance source

`HOUSE-00975` uses exactly three CC0 models from 3D Assets' [Home Appliances and Utility
Room](https://3dassets.dev/packs/home-appliances-and-utility) pack, retrieved 2026-09-23:

- asset 14050, Front load washing machine 600;
- asset 14043, Chest freezer 1.1 m;
- asset 14137, Wall hung combi boiler.

The upstream pack manifest identifies all three as AI-generated with Claude Fable 5.1 and releases
them under CC0 1.0. The exact CDN URLs and source SHA-256 values are pinned in
`tools/assets/utility_appliances_prepare.py` and repeated in `assets-src/assets.manifest.json`.
The source downloads are not shipped. Run the prepare tool with `--download --cache PATH`, then
`--write --cache PATH`, to reproduce the committed derivatives.

House Simulator keeps only each model's closed rest pose. The preparation step removes every
rigid open/close clip and hierarchy, bakes transforms, creates UV0 for canonical XNA materials and
adds one twelve-triangle enclosing `_COL` box. These are static dressing assets, not interactive
appliances. Licence: `licenses/cc0-1.0/LICENCE.txt`.
