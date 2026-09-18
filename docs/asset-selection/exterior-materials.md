# Exterior materials

`HOUSE-00903`, authored 2026-09-14 and refined by `HOUSE-00941` and `HOUSE-00943` on
2026-09-18. The fixed library has three siding colours, brick water table,
roof shingle, soffit, broomed concrete, asphalt and gravel. [The retained contact
sheet](../asset-review/materials/exterior/contact-sheet.png) applies the exact XNA tint multiply to
all nine source albedos before shell assignment.

| Material id | Role | Source base | Tint | UV scale | Snow limit |
|---|---|---|---|---|---|
| `MAT_SIDING_WARM_WHITE` | siding | `siding_clapboard` | `1.00, 0.96, 0.88` | `1.00, 1.00` | 40° |
| `MAT_SIDING_SAGE` | siding | `siding_clapboard` | `0.68, 0.78, 0.62` | `1.00, 1.00` | 40° |
| `MAT_SIDING_DUSTY_BLUE` | siding | `siding_clapboard` | `0.64, 0.74, 0.90` | `1.00, 1.00` | 40° |
| `MAT_BRICK_WATER_TABLE` | brick | `brick_red` | `1.00, 0.92, 0.86` | `0.75, 0.75` | 40° |
| `MAT_ROOF_SHINGLE` | roof shingle | `roof_asphalt_shingle` | `0.68, 0.66, 0.64` | `1.00, 1.00` | 55° |
| `MAT_SOFFIT_WHITE` | soffit | `paint_white_fine` | `1.00, 0.98, 0.93` | `1.00, 1.00` | none |
| `MAT_CONCRETE_BROOM` | concrete | `concrete_smooth` | `0.88, 0.86, 0.82` | `0.75, 0.75` | 15° |
| `MAT_ASPHALT_01` | asphalt | `asphalt_road` | `1.00, 1.00, 1.00` | `0.50, 0.50` | 12° |
| `MAT_GRAVEL_PATH` | gravel | `gravel_mixed` | `0.92, 0.90, 0.86` | `1.00, 1.00` | 30° |

## Mapping decisions

The three existing production ids for asphalt, concrete and gravel are adopted into the generated
set unchanged, rather than duplicated or renamed. This keeps all existing exterior references
stable while making the complete nine-material library reproducible from measured bases.

Round 71 exposed that the original three siding rows inherited ambientCG `Wood095`: useful bare
board for joinery, but visibly wrong as painted Colonial Revival clapboard. `HOUSE-00941` replaces
only those rows and their generated wet/unbaked derivatives with the project-authored
`siding_clapboard` pair. Six courses per one-metre world UV tile give 167 mm exposure; the neutral
albedo retains all three tints and the narrow overlap shading stays legible in stock
`DualTextureEffect`. The matching +Y normal preserves the same profile for the approved
normal-aware path. Fence pickets deliberately retain `wood_board`, so the material correction does
not reclassify unrelated bare boards.

The 34-source acquisition has no dedicated roofing map. `HOUSE-00903` therefore recorded
`tile_light_square` as a visible limitation rather than falsely naming it shingles. Fixed exterior
review later proved the charcoal square ceramic grid still reads exactly as floor tile across the
main and garage roofs. `HOUSE-00943` replaces only that visible source with a deterministic
project-authored asphalt pair: seven 143 mm courses and four staggered tabs per true surface metre,
restrained granular variation, short exposed slots and a matching +Y tangent normal. Its restrained
warm-neutral tint reads charcoal under the cool clear-day sky; asphalt class, wet response,
footstep, absorption and 55° snow limit remain stable. Roof UV0 follows
the horizontal contour and measures V on the actual slope, so the one-metre claim holds on all hip
planes and dormers. The downward-facing soffit remains the sole non-coverable member.

`tools/assets/exterior_materials.py` derives every complete §22.1 row from its measured source,
deterministically regenerates and pixel-checks the clapboard and asphalt-shingle pairs,
verifies the exact `3 + 1 + 1 + 1 + 1 + 1 + 1` role inventory, checks the roof and soffit semantic
exceptions, and compares the committed preview pixel for pixel. Shell assignment remains
`HOUSE-00907`'s work; wet and snow overlay variants remain `HOUSE-00905` and `HOUSE-00906`.
