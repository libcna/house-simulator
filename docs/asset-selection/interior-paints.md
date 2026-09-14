# Interior paint variants

`HOUSE-00901`, authored 2026-09-14. The library contains 12 wall colours and 6 ceiling finishes;
it does not assign them to cells, which remains `HOUSE-00908`'s room-palette work.

[The retained contact sheet](../asset-review/materials/interior-paints/contact-sheet.png) shows the
exact albedo × tint operation used by stock XNA. The first two rows form a restrained family palette
for occupied rooms. The final row includes deliberately distressed service and attic finishes.

| Role | Material id | Source base | Tint |
|---|---|---|---|
| wall | `MAT_PAINT_WARM_WHITE` | `paint_white_fine` | `1.00, 0.96, 0.90` |
| wall | `MAT_PAINT_SOFT_WHITE` | `paint_white_fine` | `0.96, 0.98, 1.00` |
| ceiling | `MAT_PAINT_FLAT_WHITE` | `paint_white_fine` | `1.00, 1.00, 1.00` |
| wall | `MAT_PAINT_IVORY` | `paint_white_fine` | `1.00, 0.91, 0.74` |
| wall | `MAT_PAINT_LINEN` | `paint_white_fine` | `0.88, 0.80, 0.69` |
| ceiling | `MAT_PAINT_CEILING_WARM` | `paint_white_fine` | `1.00, 0.98, 0.93` |
| wall | `MAT_PAINT_SOFT_GREY` | `paint_white_fine` | `0.78, 0.82, 0.85` |
| wall | `MAT_PAINT_SAGE` | `paint_white_fine` | `0.68, 0.79, 0.64` |
| ceiling | `MAT_PAINT_CEILING_COOL` | `paint_white_fine` | `0.94, 0.98, 1.00` |
| wall | `MAT_PAINT_PALE_SAND` | `paint_white_fine` | `0.94, 0.80, 0.60` |
| wall | `MAT_PAINT_PALE_ROSE` | `paint_white_fine` | `1.00, 0.75, 0.78` |
| ceiling | `MAT_PAINT_CEILING_CREAM` | `paint_white_fine` | `1.00, 0.95, 0.86` |
| wall, distressed | `MAT_PAINT_DUSTY_BLUE` | `paint_cool_rough` | `0.78, 0.86, 1.00` |
| wall | `MAT_PAINT_MUTED_TEAL` | `paint_white_fine` | `0.56, 0.80, 0.75` |
| ceiling, moisture resistant | `MAT_PAINT_CEILING_MOISTURE` | `paint_white_fine` | `0.91, 0.98, 1.00` |
| wall, distressed | `MAT_PAINT_AGED_PLASTER` | `paint_grey_fine` | `0.90, 0.90, 0.86` |
| wall, distressed | `MAT_PAINT_SMOKY_OCHRE` | `paint_aged` | `0.72, 0.55, 0.40` |
| ceiling, distressed | `MAT_PAINT_CEILING_ATTIC` | `paint_grey_fine` | `1.00, 0.96, 0.87` |

## Review and derivation

The initial six-source-by-three-role draft was rejected on visual inspection. Five acquired paint
maps contain obvious peeling, staining or exposed masonry: useful detail for a basement, service
space or old attic, but incoherent across ordinary bedrooms and living rooms. The final library
therefore uses `paint_white_fine` for every clean colour and limits chipped sources to the four
explicitly distressed rows above. This lets `HOUSE-00908` decorate one maintained family house
without discarding the worn finishes needed by its older utility spaces.

`tools/assets/paint_variants.py` derives every complete §22.1 row from its measured `MAT_BASE_*`
record, changes only the permanent id, tint and indoor snow response, and regenerates the contact
sheet. Its CI check compares every field and every preview pixel. All tint components are at most
one because XNA's diffuse multiplier can attenuate a sampled texture but cannot recover detail by
amplifying clipped colour.
