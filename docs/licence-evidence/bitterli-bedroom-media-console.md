# Bedroom low cabinet licence evidence

- Asset: `MODEL_FAMILY_MEDIA_CONSOLE` (`HOUSE-01053`)
- Original scene/model: **Bedroom** / **Bedroom With Cycles**
- Author: SlykDrako
- Licence: CC0 1.0
- Original catalogue: <https://blendswap.com/blend/3391>
- Rendering-resources catalogue: <https://benedikt-bitterli.me/resources/>
- glTF distribution: <https://github.com/gkjohnson/3d-demo-data/tree/main/models/bitterli-rendering-resources>
- Pinned GLB: <https://raw.githubusercontent.com/gkjohnson/3d-demo-data/main/models/bitterli-rendering-resources/bedroom.glb>
- Retrieved/verified: 2026-09-17

The BlendSwap catalogue identifies SlykDrako as the uploader and explicitly labels the Bedroom
model CC0. The Bitterli catalogue independently identifies the Bedroom scene and author; the
3d-demo-data distribution points users back to the original source for licence terms. The full
pinned upstream GLB is 4,892,652 bytes with SHA-256
`30e26c0fe67da6b73612284598952011f7dc7f7319d5d9a802112147226ad2ba`.

The exact decoded intermediate reused from `libcna/living-room-simulator` is 3,252,936 bytes with
SHA-256 `51bb4602e858dc8155ed6dca1ddab80ff459d5ff710a41e4c1a15340782054cc`.
Its documented glTF-Transform recipe selects only `WoodFurniture_0004` and
`StainlessSmooth_0003`, decodes Draco, converts the source images and recentres the object.
`tools/assets/family_media_console_prepare.py` independently pins both hashes, keeps the two
geometry/UV0 streams, strips every source image and material extension, scales/grounds/recentres
the cabinet, substitutes canonical cna-house material roles and appends the collision proxy.

No source texture is distributed or relied on by cna-house. The committed derived GLB contains
only the CC0 cabinet geometry, normals, UV0, two role placeholders and project-authored two-box
proxy.
The archived CC0 legal text is `licenses/cc0-1.0/LICENCE.txt`.
