# HOUSE-03402 static-prop lighting evidence

Date: 2026-09-24

## Decision

Use one precomputed irradiance sample per cell/lightmap product for Basic-lit indoor static detail.
The bake records the linear-luminance mean over non-black receiver texels before PNG
normalisation. Runtime composes the daylight sample with the room daylight tint and only the
cell's active, owned fixture-group samples. It then uses the result as a floor for the existing
BasicEffect bounce, capped to a 1.40x component-wise lift because a cell-wide value cannot
reproduce the receiver's local UV2 gradient.

This is the cheapest of the three accepted options. It adds 216 scalar values to products that
already exist (78 daylight and 138 artificial bindings), no texture lookup, UV1, vertex stream,
draw or runtime manager. Baked prop lightmaps would add UV1/content/batches to the complete prop
kit. Baked vertex occlusion would need a second geometry-authoring path and would not follow the
existing scheduled fixture groups. The rejected global-gain probes from `HOUSE-01076` remain
rejected: the existing global constants are unchanged and the new floor is indoor-only.

The uncapped candidate was rejected after matched captures: it raised the selected master-bedroom
prop/receiver ratio from 2.020 to 10.834 and the selected library chair/wall ratio from 5.201 to
37.680. The bounded final candidate keeps the pale master-bedroom prop at 3.369 and a matched wood
shelf/wall sample in the library at 1.960 without clipping either surface.

## Measurement

All figures below are mean linear-sRGB luminance ratios (`prop / adjacent baked receiver`) from
fixed 1600x900 captures. Rectangles are half-open `(left, top, right, bottom)` pixel coordinates.
The declared indoor acceptance band is **0.25–3.50**. It permits the authored albedo difference
between dark wood and pale fabric while rejecting a lighting-driven black silhouette or clipped
prop. All 16 final indoor day/night ratios are inside the band; the final range is 0.283–3.369.

| Zone | Fixed view | Prop rectangle | Receiver rectangle | Day before → after | Night before → after |
|---|---|---:|---:|---:|---:|
| Z-B1 | basement-workshop | 150,620,560,735 | 150,420,560,540 | 0.929 → 1.273 | 0.797 → 0.797 |
| Z-L0M | family-media | 850,650,1120,755 | 600,780,1100,860 | 0.258 → 0.334 | 0.250 → 0.283 |
| Z-L0S | ground-mudroom | 370,390,650,840 | 700,300,960,700 | 1.717 → 1.757 | 1.371 → 1.371 |
| Z-GAR | garage-interior | 850,500,1320,660 | 850,720,1350,850 | 0.995 → 1.447 | 1.903 → 1.903 |
| Z-L1 | master-bedroom | 690,515,775,595 | 650,400,850,500 | 2.020 → 3.369 | 0.955 → 0.955 |
| Z-L2 | library | 130,535,285,570 | 880,390,1080,540 | 1.407 → 1.960 | 0.647 → 0.647 |
| Z-L3 | attic-room | 650,500,780,610 | 620,690,900,820 | 1.255 → 1.255 | 1.254 → 1.254 |
| Z-STAIR | main-stair-l1-exit | 940,600,1090,700 | 1030,720,1280,850 | 0.717 → 0.892 | 0.961 → 0.961 |

At the current authored new-game state, several upper/service fixture groups are off at 22:00;
their unchanged night rows are therefore expected. `HOUSE-03401` owns the automatic schedule.
Where a fixture group is active (`Z-L0M`), the sample follows it. Full-resolution inspection found
no new orange cast: daylight uses the receiver's room tint and fixture samples use the same group
colour as the adjacent baked shell.

Outdoor controls remain outside the indoor ratio band because a black lantern, timber bench or
partly occluded car is deliberately a different albedo from pavement, lawn or road. The relevant
test is preservation of their existing calibration. Every selected outdoor ROI is pixel-identical
before/after apart from the HUD timing text outside the rectangles:

| Zone | Fixed view | Prop rectangle | Receiver rectangle | Day before = after | Night before = after |
|---|---|---:|---:|---:|---:|
| Z-EXF | garage-approach | 1190,535,1245,615 | 1130,650,1320,790 | 0.768 | 6.086 |
| Z-EXR | garden-potting | 580,635,1120,720 | 600,520,1100,620 | 1.086 | 0.278 |
| Z-STR | neighbourhood-street | 1180,405,1310,455 | 900,525,1320,650 | 0.283 | 52.782 |

## Captures reviewed

- Before day: `captures/house-03380-dressed-checkpoint-r143/`
- Before night: `captures/house-03402-before-night-r144/`
- Final day: `captures/house-03402-final-day-r144/`
- Final night: `captures/house-03402-final-night-r144/`

The capture directories are the repository's established local, Git-ignored visual evidence.
