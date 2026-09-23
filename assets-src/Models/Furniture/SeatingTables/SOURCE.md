# Seating and table sources

`HOUSE-00977` reuses the project's existing ground-floor sofas, armchairs, dining chairs, stools,
coffee tables and side tables before acquiring anything. Only the three recipe gaps below remain,
within the authoritative seven-model cap. Each was retrieved from the official 3DAssets.dev CDN
on 2026-09-23 under CC0 1.0:

- asset 34650, *Tiered Seating Row Module*, from [Cinema Multiplex and
  Foyer](https://3dassets.dev/packs/cinema-multiplex-and-foyer);
- asset 38795, *Desk Writing*, from [Bedroom and Living Room
  Furniture](https://3dassets.dev/packs/bedroom-and-living-room-furniture);
- asset 33854, *Pool Table, Nine Foot*, from [Bowling Alley and Pool
  Hall](https://3dassets.dev/packs/bowling-alley-and-pool-hall).

The first and third packs declare Claude Opus 5 as their AI generator; the furniture pack declares
Muse Spark via T3 Code. Exact CDN URLs and source SHA-256 values are pinned in
`tools/assets/seating_tables_prepare.py` and repeated in `assets-src/assets.manifest.json`. Source
downloads are not shipped. Run the preparation tool with `--download --cache PATH`, then `--write
--cache PATH`, to reproduce the committed derivatives.

House Simulator uses authored static rest poses only. The existing bounded static-preparation path
bakes transforms, removes drawer and tip-up-seat clips and hierarchy, adds UV0 for canonical XNA
materials, and appends one twelve-triangle enclosing `_COL` box per model. The cinema row stays in
its useful seat-down pose. No furniture interaction or gameplay is introduced. Licence:
`licenses/cc0-1.0/LICENCE.txt`.
