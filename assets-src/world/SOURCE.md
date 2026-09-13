# World-source provenance

## `DATA_SKY_BSC5P` — `bsc5p.psv`

| | |
|---|---|
| Publisher | NASA High Energy Astrophysics Science Archive Research Center (HEASARC) |
| Catalogue | BSC5P — Bright Star Catalog, 5th Edition, preliminary |
| Table page | <https://heasarc.gsfc.nasa.gov/W3Browse/catalog/bsc5p.html> |
| Dataset record | <https://catalog.data.gov/dataset/bright-star-catalog> |
| Retrieved | 2026-09-13 |
| SHA-256 | `31464f3928a834a44c1a7b1c960081550357b6e4134c2ff568223d6b03e865b7` |

The source snapshot is the complete 9,110-row response from HEASARC's public Xamin interface,
sorted by its `name` field and restricted to the five columns used by this project:

```text
https://heasarc.gsfc.nasa.gov/xamin/cli?table=bsc5p&fields=name%2Cra%2Cdec%2Cvmag%2Cbv_color&format=stream&sortvar=name&resultmax=10000
```

Data.gov identifies this exact HEASARC dataset as `ivo://nasa.heasarc/bsc5p`, lists HEASARC as its
publisher, and records its licence as `https://www.usa.gov/government-works`. The detailed decision,
including the federal-work and international-use caveats, is retained in
`docs/licence-evidence/astronomical.md`; the statutory US text is archived in
`licenses/us-gov-pd/LICENCE.txt`.

`tools/world/build_stars.py` is the sole consumer and output writer. It validates the entire source,
removes HEASARC's 14 named non-stellar HR records and rows missing either V magnitude or B−V, then
writes the 1,500 brightest complete stellar records to generated `content/world/stars.bin`. The
source snapshot itself is not packaged.
