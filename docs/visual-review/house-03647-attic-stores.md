# HOUSE-03647 — attic storage walls

The released [north-store day view](house-03636/after-day/L3_STORE_N.webp) showed large dark
patches across the low wall and roof slope. The [released west-store view](house-03636/after-day/L3_STORE_W.webp)
showed the same aged-plaster pattern on its end wall. All four attic stores used the strongly
chipped `paint_grey_fine` source on walls and ceiling. A 1,024-sample bake with that finish barely
changed the north wall. A bake without its three solid props differed by only 0.30 of 255 RGB
levels on average; a 256-square probe made the rafter shadows sharper. The two point emitters had
the baker's 5 mm default radius, which cast the hard pattern onto the long north and south walls.

The four stores now reuse the existing warm-white fine plaster and cream ceiling finish. Their
shell receivers and both lightmap modes were regenerated at 1,024 samples, with only those four
cells promoted. North and south utility fixtures additionally use a 0.25 m radius **in the
offline bake**. Their authored position, 800 lm intensity, runtime group and lighting schedule
remain the same. The two bake reports, cell shell hashes and manifest source hashes were promoted
together; `lightmap_bake.py --selftest` covers the radius override.

Inspected current OPENGLES3, Tier S, High captures at clear 10:30 and scheduled 22:00:

| Store | Day | Night |
|---|---|---|
| North | [after](house-03647/after-north-day.webp) | [after](house-03647/after-north-night.webp) |
| South | [after](house-03647/after-south-day.webp) | [after](house-03647/after-south-night.webp) |
| East | [after](house-03647/after-east-day.webp) | inspected in the same fixed pose |
| West | [after](house-03647/after-west-day.webp) | inspected in the same fixed review pose |

The large mottled patches are gone; the rooms retain their intentionally warm, low attic light.
The rest of the attic and every room outside these four cells keeps its previous finish and bake.
