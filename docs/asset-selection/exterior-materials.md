# Exterior materials

`HOUSE-00903`, authored 2026-09-14. The fixed library has three siding colours, brick water table,
roof shingle, soffit, broomed concrete, asphalt and gravel. [The retained contact
sheet](../asset-review/materials/exterior/contact-sheet.png) applies the exact XNA tint multiply to
all nine source albedos before shell assignment.

| Material id | Role | Source base | Tint | UV scale | Snow limit |
|---|---|---|---|---|---|
| `MAT_SIDING_WARM_WHITE` | siding | `wood_board` | `1.00, 0.96, 0.88` | `1.00, 1.00` | 40° |
| `MAT_SIDING_SAGE` | siding | `wood_board` | `0.68, 0.78, 0.62` | `1.00, 1.00` | 40° |
| `MAT_SIDING_DUSTY_BLUE` | siding | `wood_board` | `0.64, 0.74, 0.90` | `1.00, 1.00` | 40° |
| `MAT_BRICK_WATER_TABLE` | brick | `brick_red` | `1.00, 0.92, 0.86` | `0.75, 0.75` | 40° |
| `MAT_ROOF_SHINGLE` | roof shingle | `tile_light_square` | `0.66, 0.69, 0.74` | `1.50, 1.50` | 55° |
| `MAT_SOFFIT_WHITE` | soffit | `paint_white_fine` | `1.00, 0.98, 0.93` | `1.00, 1.00` | none |
| `MAT_CONCRETE_BROOM` | concrete | `concrete_smooth` | `0.88, 0.86, 0.82` | `0.75, 0.75` | 15° |
| `MAT_ASPHALT_01` | asphalt | `asphalt_road` | `1.00, 1.00, 1.00` | `0.50, 0.50` | 12° |
| `MAT_GRAVEL_PATH` | gravel | `gravel_mixed` | `0.92, 0.90, 0.86` | `1.00, 1.00` | 30° |

## Mapping decisions

The three existing production ids for asphalt, concrete and gravel are adopted into the generated
set unchanged, rather than duplicated or renamed. This keeps all existing exterior references
stable while making the complete nine-material library reproducible from measured bases.

The 34-source acquisition has no dedicated roofing map. Its `tile_light_square` source is visibly
charcoal despite the immutable source slug and is the only retained texture with discrete,
weather-shedding units. The review therefore uses it for shingle courses and overrides its runtime
class, wet response, footstep and absorption with the existing asphalt semantics. This records the
limitation instead of claiming that a road texture contains shingles. Its 55° snow limit clears the
house's approximately 34° roof pitch; the downward-facing soffit is the sole non-coverable member.

`tools/assets/exterior_materials.py` derives every complete §22.1 row from its measured source,
verifies the exact `3 + 1 + 1 + 1 + 1 + 1 + 1` role inventory, checks the roof and soffit semantic
exceptions, and compares the committed preview pixel for pixel. Shell assignment remains
`HOUSE-00907`'s work; wet and snow overlay variants remain `HOUSE-00905` and `HOUSE-00906`.
