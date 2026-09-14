# Base material acquisition and review

`HOUSE-00296`, acquired and reviewed 2026-09-13. The task requires 34 PBR inputs: paint ×6,
wood ×6, tile ×4, carpet ×3, stone ×3, one each of brick, plaster, concrete, asphalt, gravel,
grass and soil, fabric ×3, and metal ×2.

## Source and selection

All 34 sets come from ambientCG's official per-asset download endpoint. ambientCG was preferred
over mixing sources because its asset files and preview renders have an explicit CC0 grant, its
API permits a fixed named selection, and every selected 1K-JPG archive provides colour,
OpenGL-normal and roughness maps. The two metal sets additionally provide metalness; missing AO
and metalness on dielectric sets are filled with physically neutral white and black respectively.

Candidates were selected from the official API's popular results for each requested class, then
reviewed in the local PBR scene. One initial metal candidate, `Metal063`, was rejected after that
render: it was a dark contaminated finish and did not match the intended brushed appliance steel.
`Metal009` replaces it. `Metal049A` is retained as chrome because §22.2 explicitly calls for a
chrome path for taps and handles. Exact asset URLs, archive SHA-256 hashes and all 102 manifest ids
are recorded in [`SOURCE.md`](../../assets-src/Textures/Materials/SOURCE.md).

| Class | Retained local materials |
|---|---|
| paint ×6 | [white fine](../asset-review/materials/paint_white_fine.png), [warm fine](../asset-review/materials/paint_warm_fine.png), [grey fine](../asset-review/materials/paint_grey_fine.png), [cream rough](../asset-review/materials/paint_cream_rough.png), [cool rough](../asset-review/materials/paint_cool_rough.png), [aged](../asset-review/materials/paint_aged.png) |
| wood ×6 | [oak floor](../asset-review/materials/wood_oak_floor.png), [light floor](../asset-review/materials/wood_light_floor.png), [dark floor](../asset-review/materials/wood_dark_floor.png), [parquet floor](../asset-review/materials/wood_parquet_floor.png), [worn floor](../asset-review/materials/wood_worn_floor.png), [board](../asset-review/materials/wood_board.png) |
| tile ×4 | [warm](../asset-review/materials/tile_warm_square.png), [grey](../asset-review/materials/tile_grey_square.png), [light](../asset-review/materials/tile_light_square.png), [dark](../asset-review/materials/tile_dark_square.png) |
| carpet ×3 | [beige](../asset-review/materials/carpet_beige.png), [grey](../asset-review/materials/carpet_grey.png), [brown](../asset-review/materials/carpet_brown.png) |
| stone ×3 | [marble](../asset-review/materials/stone_marble.png), [onyx](../asset-review/materials/stone_onyx.png), [rough](../asset-review/materials/stone_rough.png) |
| fabric ×3 | [plain](../asset-review/materials/fabric_plain.png), [weave](../asset-review/materials/fabric_weave.png), [coarse](../asset-review/materials/fabric_coarse.png) |
| metal ×2 | [brushed steel](../asset-review/materials/metal_brushed.png), [chrome](../asset-review/materials/metal_chrome.png) |
| single classes | [brick](../asset-review/materials/brick_red.png), [plaster](../asset-review/materials/plaster_natural.png), [concrete](../asset-review/materials/concrete_smooth.png), [asphalt](../asset-review/materials/asphalt_road.png), [gravel](../asset-review/materials/gravel_mixed.png), [grass](../asset-review/materials/grass_lawn.png), [soil](../asset-review/materials/soil_garden.png) |

## Conversion and measured budget

`tools/assets/ambientcg_materials.py` downloads only the fixed 34-id list and refuses an archive
whose SHA-256 differs from the reviewed bytes. It preserves source aspect ratio and writes three
opaque, equal-sized, power-of-two PNGs per material, with maximum edge 256:

- `*_albedo.png`: sRGB colour;
- `*_normal.png`: linear OpenGL/glTF (+Y) tangent normal;
- `*_orm.png`: linear `R=ambient occlusion`, `G=roughness`, `B=metalness`.

The retained result is **102 textures: 90 at 256×256 and 12 at 256×128**. The PNG sources occupy
9,489,783 bytes. Their measured RGBA8 base levels occupy 25,165,824 bytes; all three channels with
complete mip chains would occupy exactly 33,554,432 bytes (32 MiB).

The first real CNB build measured `core` at 58.4 MB when all three channels were packaged, above
its 55 MB limit. Section 22.1's application-owned runtime record has albedo and normal texture
slots but deliberately stores metallic/roughness as the `specularColor` and `specularPower`
scalars derived by `pbr_to_stock.py`. The 34 ORM maps therefore remain hash-manifested offline
inputs for the reproducible `HOUSE-00900` mapping, not packaged runtime textures. The 68 packaged maps occupy 22,422,896
compiled bytes including mip chains; the complete `core` pack then measures 47.2 MB, leaving
7.8 MB headroom. That is also why 512 was not used: the packaged pair would require four times
this memory. The content configuration explicitly enables mips and disables alpha
premultiplication for every packaged map.

## Stock-mapped authoring review

[The labelled contact sheet](../asset-review/materials/contact-sheet.jpg) summarizes the 34
retained 960×480 renders. Each linked image above is the full-resolution evidence. Every image
shows the authored stock mapping on a UV sphere, tiled floor and vertical wall, repeated at low,
medium and high direct-light levels from left to right. The preview reconstructs the fixed
Blinn-Phong lobe from the row's `specularColor` and `specularPower`; ORM is not sampled by the
preview or runtime and remains an offline reproducibility input.

All 34 sets tile across the floor without an edge discontinuity, keep their tangent-space relief
continuous around the sphere, remain readable against the wall, and preserve their identity at all
three light levels. Surface classes remain distinct at 256 pixels: carpet/fabric stay matte, floor
woods retain board or parquet structure, masonry joints remain readable, and brushed steel
resolves its directional grain. Chrome is intentionally dark away from the direct highlights
because the stock mapping has no diffuse metal lobe or image-based lighting.

`layout.materials.json` now contains one complete, neutral `MAT_BASE_*` §22.1 row for every set.
`pbr_to_stock.py` measures mean roughness, maps it to the clamped 4–256 Blinn-Phong power, gives
dielectrics F0 0.04, and uses mean albedo as the tint of a metal's specular lobe. JPEG endpoint noise
is snapped to the reviewed class: dielectrics are metallic 0 and the two metals metallic 1. Brick
uses §22.2's `stone` path and plaster its `paint` path instead of inventing runtime classes. The
base rows deliberately keep neutral tint and UV scale; room- and object-specific variants tune
those fields in `HOUSE-00901` onward.
