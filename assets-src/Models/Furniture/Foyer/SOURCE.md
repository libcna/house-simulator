# L0 foyer furniture (HOUSE-01038)

Two 2K glTF sources from Poly Haven, both by Kirill Sannikov, retrieved 2026-09-15:

| Derived file | Source | Original model scale | Licence |
|---|---|---|---|
| `console_table.glb` | https://polyhaven.com/a/chinese_console_table | 1.720 × 0.661 × 0.339 m | CC0-1.0 |
| `upholstered_armchair.glb` | https://polyhaven.com/a/ArmChair_01 | 0.848 × 1.065 × 0.766 m; seat baseline measured near 0.47 m | CC0-1.0 |

`tools/assets/foyer_furniture_prepare.py` pins Poly Haven's file-list metadata SHA-256, checks every included 2K glTF/bin/JPEG size and MD5, packs the source maps, normalizes CNA's unsupported mip sampler, and adds a measured enclosing `_COL` proxy (24 and 48 triangles). `--check --cache CACHE` reproduces the committed bytes. The source cache and official preview images are not shipped. Licence: `licenses/cc0-1.0/LICENCE.txt`.
