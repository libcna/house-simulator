# Glass and water materials

`HOUSE-00904`, authored 2026-09-14. The fixed transparent library covers the four glass uses named
by §22.2 and the four water uses that later plumbing, toilet and rain tasks consume. [The retained
contact sheet](../asset-review/materials/glass-water/contact-sheet.png) composites every tint and
opacity over the same high-contrast background; this makes lost transparency or indistinguishable
variants visible before geometry exists.

| Material id | Role | Texture | Alpha | UV scale | Tier E |
|---|---|---|---|---|---|
| `MAT_GLASS_CLEAR` | windows | untextured | 0.12 | `1.00, 1.00` | `RoomLit/Glass` |
| `MAT_GLASS_OBSCURED` | bathroom windows | untextured | 0.32 | `1.00, 1.00` | `RoomLit/Glass` |
| `MAT_GLASS_CABINET` | cabinet fronts | untextured | 0.18 | `1.00, 1.00` | `RoomLit/Glass` |
| `MAT_GLASS_SHOWER` | foggable shower screen | untextured | 0.16 | `1.00, 1.00` | `RoomLit/Glass` |
| `MAT_WATER_FLOW` | tap and shower stream | `water_flow` | 0.52 | `0.50, 3.00` | `WaterFlow` |
| `MAT_WATER_BASIN` | bath and plugged basin | `water_flow` | 0.38 | `1.00, 1.00` | `WaterFlow` |
| `MAT_WATER_TOILET` | toilet bowl | `water_flow` | 0.46 | `1.50, 1.50` | `WaterFlow` |
| `MAT_WATER_PUDDLE` | rain puddle | `water_flow` | 0.28 | `0.75, 0.75` | `WaterFlow` |

## Texture and stock-XNA decisions

The acquired PBR library has no glass or water map. Glass therefore stays honestly untextured on
the architecture's tinted `BasicEffect` fallback; inventing opaque surface detail would make clear
panes worse. The two existing production ids and their established 0.12/0.32 opacity values are
preserved unchanged.

Water cannot meet §56.2's scrolling-flow design with tint alone. The authoring tool generates one
256×256 periodic height field, converts it to a tileable blue-white RGBA albedo and a linear +Y
tangent normal, and assigns both to all four water roles. The content pipeline premultiplies only
the RGBA albedo; the normal remains linear. Role-specific UV scale makes the flow vertically
stretched, the toilet swirl denser and broad puddles calmer without duplicating textures.

The source hashes are `b62d32dae4a0…` (albedo) and `b849a3b4bd85…` (normal).
`tools/assets/glass_water_materials.py` proves byte-deterministic generation, exact 4+4 membership,
complete blend/Basic/two-sided records, non-snow response, texture pixels and preview pixels. The
later motion/fog/refraction work remains with `HOUSE-01413`, `HOUSE-01419`, `HOUSE-01466`,
`HOUSE-01747`, `HOUSE-01749` and `HOUSE-02710`.
