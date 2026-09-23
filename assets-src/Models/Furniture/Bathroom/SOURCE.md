# Bathroom fixture source

`HOUSE-00976` uses exactly four CC0 models from 3D Assets' [Fitted Kitchen and Bathroom
Builder](https://3dassets.dev/packs/fitted-kitchen-and-bathroom-builder) pack, retrieved
2026-09-23:

- asset 17870, Close coupled WC;
- asset 17883, Bathroom vanity unit 600;
- asset 17865, Single ended bath 1700;
- asset 17894, Square shower enclosure 900.

The last asset includes both its tray and glazed screen/enclosure. The upstream pack manifest
identifies all four as AI-generated with Claude Fable 5.1 and releases them under CC0 1.0. The
exact CDN URLs and source SHA-256 values are pinned in
`tools/assets/bathroom_fixtures_prepare.py` and repeated in `assets-src/assets.manifest.json`. The
source downloads are not shipped. Run the prepare tool with `--download --cache PATH`, then
`--write --cache PATH`, to reproduce the committed derivatives.

House Simulator keeps only each model's closed rest pose. The preparation step reuses the bounded
static-furnishing conversion introduced by `HOUSE-00975`: it removes rigid open/close clips and
hierarchy, bakes transforms, creates UV0 for canonical XNA materials and adds one twelve-triangle
enclosing `_COL` box. These fixtures are static dressing, not interactive plumbing. Licence:
`licenses/cc0-1.0/LICENCE.txt`.
