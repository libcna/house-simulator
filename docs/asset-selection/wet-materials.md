# Wet exterior material endpoints

`HOUSE-00905`, authored 2026-09-14. Tier S cannot continuously blend material parameters, so the
wet endpoint is a second material selected by `HOUSE-01748`; Tier E continues to use
`SurfaceBlend/Wet`. [The retained contact sheet](../asset-review/materials/wet/contact-sheet.png)
shows every dry source beside the derived fully-wet tint under identical pixels.

| Dry material | Wet material | Class | Darken | Specular boost | Power boost |
|---|---|---|---:|---:|---:|
| `MAT_ASPHALT_01` | `MAT_ASPHALT_01_WET` | `wet_asphalt` | 0.35 | 2.4 | 2.6 |
| `MAT_BLUESTONE_PAVER` | `MAT_BLUESTONE_PAVER_WET` | `wet_stone` | 0.28 | 2.2 | 2.5 |
| `MAT_BRICK_WATER_TABLE` | `MAT_BRICK_WATER_TABLE_WET` | `wet_stone` | 0.26 | 2.0 | 2.2 |
| `MAT_CONCRETE_BROOM` | `MAT_CONCRETE_BROOM_WET` | `wet_concrete` | 0.24 | 2.0 | 2.0 |
| `MAT_CONCRETE_KERB` | `MAT_CONCRETE_KERB_WET` | `wet_concrete` | 0.24 | 2.0 | 2.0 |
| `MAT_CONCRETE_SLAB` | `MAT_CONCRETE_SLAB_WET` | `wet_concrete` | 0.24 | 2.0 | 2.0 |
| `MAT_DECK_WOOD` | `MAT_DECK_WOOD_WET` | `wet_wood` | 0.28 | 2.4 | 3.0 |
| `MAT_GRAVEL_PATH` | `MAT_GRAVEL_PATH_WET` | `wet_gravel` | 0.30 | 1.8 | 1.8 |
| `MAT_GROUND_LAWN` | `MAT_GROUND_LAWN_WET` | `wet_grass` | 0.20 | 1.3 | 1.2 |
| `MAT_ROOF_SHINGLE` | `MAT_ROOF_SHINGLE_WET` | `wet_asphalt` | 0.35 | 2.4 | 2.6 |
| `MAT_SIDING_DUSTY_BLUE` | `MAT_SIDING_DUSTY_BLUE_WET` | `wet_wood` | 0.28 | 2.4 | 3.0 |
| `MAT_SIDING_SAGE` | `MAT_SIDING_SAGE_WET` | `wet_wood` | 0.28 | 2.4 | 3.0 |
| `MAT_SIDING_WARM_WHITE` | `MAT_SIDING_WARM_WHITE_WET` | `wet_wood` | 0.28 | 2.4 | 3.0 |
| `MAT_SOIL_GARDEN` | `MAT_SOIL_GARDEN_WET` | `wet_soil` | 0.34 | 1.8 | 1.6 |

## Derivation and scope

The complete authored exterior inventory has seventeen finishes. Fourteen are exposed,
non-metallic surfaces whose stock-XNA endpoint can represent rain by darkening the shared albedo
tint and tightening its specular highlight. The downward-facing soffit is sheltered. Balcony and
gutter metal already draw through the metal `BasicEffect` path, have black diffuse tints, and gain
nothing from a second darkened-albedo material; they remain outside this exact set.

`tools/assets/wet_materials.py` applies each dry row's `wetResponse` once, retains its texture,
lightmap, UV, snow and absorption fields, and writes the result with a `wet_<class>` spelling and
`SurfaceBlend/Wet` Tier-E technique. The derived row's own response is identity so a generic
effect path cannot darken an already-wet endpoint twice. Saturated garden soil changes its
footstep surface from `soil` to the already-authored `mud`; other surfaces retain their physical
footstep category.

No texture is duplicated: tint multiplication is the exact operation stock XNA performs, while
the same source normal remains available to Tier E. The generator proves exact membership,
field-for-field derivation, genuine darkening/specular increase and preview pixels.
