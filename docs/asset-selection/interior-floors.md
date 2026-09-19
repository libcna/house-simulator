# Interior floor materials

`HOUSE-00902`, authored 2026-09-14. The fixed library has the requested oak and walnut, three
carpets, four tiles, concrete, stone and vinyl. [The retained contact
sheet](../asset-review/materials/interior-floors/contact-sheet.png) applies the exact XNA tint
multiply to all 12 source albedos before any room assignment.

| Material id | Role | Source base | Tint | UV scale |
|---|---|---|---|---|
| `MAT_WOOD_OAK_FLOOR` | oak | `wood_oak_floor` | `1.00, 0.97, 0.93` | `1.00, 1.00` |
| `MAT_WOOD_WALNUT_FLOOR` | walnut | `wood_dark_floor` | `0.78, 0.60, 0.48` | `1.00, 1.00` |
| `MAT_CARPET_BEIGE` | carpet | `carpet_beige` | `1.00, 0.96, 0.88` | `1.00, 1.00` |
| `MAT_CARPET_GREY` | carpet | `carpet_grey` | `0.82, 0.86, 0.90` | `1.00, 1.00` |
| `MAT_CARPET_BROWN` | carpet | `carpet_brown` | `0.88, 0.78, 0.66` | `1.00, 1.00` |
| `MAT_TILE_PORCELAIN_GREY` | grey porcelain | `tile_warm_square` | `0.92, 0.96, 1.00` | `1.00, 1.00` |
| `MAT_TILE_CERAMIC_WARM` | warm ceramic | `tile_grey_square` | `1.00, 0.94, 0.86` | `1.00, 1.00` |
| `MAT_TILE_CERAMIC_LIGHT` | light ceramic | `tile_dark_square` | `1.00, 1.00, 1.00` | `1.00, 1.00` |
| `MAT_TILE_CERAMIC_DARK` | dark ceramic | `tile_light_square` | `0.86, 0.88, 0.92` | `1.00, 1.00` |
| `MAT_FLOOR_CONCRETE` | concrete | `concrete_smooth` | `0.88, 0.86, 0.82` | `0.75, 0.75` |
| `MAT_FLOOR_STONE` | marble stone | `stone_marble` | `1.00, 0.98, 0.95` | `1.00, 1.00` |
| `MAT_FLOOR_VINYL` | wood-look vinyl | `wood_light_floor` | `0.90, 0.88, 0.82` | `1.00, 1.00` |

## Mapping decisions

The first contact sheet exposed that the historical tile source slugs describe their selection
slots rather than their visible values: `tile_light_square` is charcoal, while
`tile_dark_square` is pale. Those immutable slugs remain honest provenance, but the four gameplay
material ids are mapped by the reviewed appearance. The result now reads grey, warm, light and
dark in that order.

Section 22.2 has no `vinyl` runtime class. Treating it as `plastic` would select `BasicEffect` and
lose the second UV/lightmap slot required by every static floor. `MAT_FLOOR_VINYL` therefore uses
the `tile` class and `DualTexture` tier, but explicitly carries `footstepSurface: vinyl` and its own
absorption. This is an application-owned semantic mapping, not a new rendering path.

`tools/assets/floor_materials.py` derives every full §22.1 row from the measured `MAT_BASE_*`
source, verifies the exact requested role counts, refuses non-positive UV scales, checks that all
12 remain lightmap receivers and non-snow-coverable, and compares the committed preview pixel for
pixel. Room selection remains `HOUSE-00908`'s work.

The visible sunroom is a later room-specific correction (`HOUSE-00950`), not a thirteenth
member of the original fixed library. `MAT_SUNROOM_LIMESTONE_TILE` reuses approved CC0 ambientCG
Tiles139 (`tile_grey_square` albedo and normal) with a near-neutral warm tint. Four source tiles
per repeat at 0.45 repeats per world metre yield 0.556 m modules; the separate DualTexture row
keeps UV2, the tile footstep and the rest of the house's original marble unchanged.
