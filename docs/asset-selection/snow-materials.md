# Snow-shell materials

`HOUSE-00906`, authored 2026-09-14. [The retained contact
sheet](../asset-review/materials/snow/contact-sheet.png) shows each substrate at shallow coverage
beside its nearly opaque deep-snow endpoint. The visual source is one shared project-authored,
tileable 256-square albedo/normal pair; the six rows are the exact `snow_<class>` mapping required
by `docs/snowshell-format.md`.

| Material id | Shell source class | Tint | Specular | Power |
|---|---|---|---|---:|
| `MAT_SNOW_ASPHALT` | `asphalt` | `0.88, 0.91, 0.94` | `0.10, 0.11, 0.12` | 8 |
| `MAT_SNOW_GRASS` | `grass` | `0.92, 0.95, 0.96` | `0.08, 0.09, 0.10` | 6 |
| `MAT_SNOW_METAL` | `metal` | `0.94, 0.96, 1.00` | `0.14, 0.15, 0.16` | 12 |
| `MAT_SNOW_SOIL` | `soil` | `0.89, 0.91, 0.92` | `0.08, 0.09, 0.09` | 6 |
| `MAT_SNOW_STONE` | `stone` | `0.91, 0.93, 0.95` | `0.11, 0.12, 0.13` | 10 |
| `MAT_SNOW_WOOD` | `wood` | `0.90, 0.93, 0.94` | `0.10, 0.11, 0.12` | 8 |

## Why exactly six

The offline shell source graph currently emits 17 groups over six base classes: asphalt, grass,
metal, soil, stone and wood. Those are also the classes of §38's planned terrain, roofs, open decks,
fence/furniture tops and car surfaces. `snow_materials.py` derives that set from the real snow-shell
build and compares it with this fixed inventory. A future seventh class therefore fails the content
gate instead of drawing with an arbitrary fallback.

The task's singular snow-shell material is the shared visual surface, not a seventh fake class.
The §22.2 contract requires a `snow_<class>` spelling so each of its six material rows preserves the
base-class identity. All six use the same `snow_shell` texture pair, `BasicEffect` and the runtime
`smoothstep(0.002, 0.03, snowDepth)` alpha. The shell format has only `TEXCOORD_0`, so these dynamic
overlays deliberately set `lightmapChannel: 0`; Tier E instead names `SurfaceBlend/Snowy` and can
consume the shared normal map.

The rows are opaque at their authored endpoint and use `alphaMode: blend`; `HOUSE-01793` multiplies
that alpha by the smoothstep coverage. They cannot recursively collect snow, every footstep becomes
`snow`, and absorption 0.85 supplies §38's markedly quieter ambience. The subtle class tint records
the substrate influence visible through shallow accumulation without manufacturing six duplicate
textures.

The generated source hashes are `8f90ea1961bf…` (albedo) and `011731ba30cd…` (normal).
`tools/assets/snow_materials.py` proves byte-deterministic maps, exact class membership,
field-for-field rows, the live snow-shell source-class census and retained preview pixels.
