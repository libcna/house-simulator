# HOUSE-03407 ground-floor route lighting evidence

Date: 2026-09-24

## Decision

Close Round 102's ranked `Z-L0M` S2 list by verifying the required dependency work; add no new
ground-floor tuning. The list's three items now resolve as follows:

1. `HOUSE-03402` applies the accepted bounded receiver sample to Basic-lit props house-wide. Its
   measured `Z-L0M` family-media prop/receiver ratios are 0.334 by day and 0.283 at night, both
   inside the declared 0.25–3.50 band. The fresh living views show the formerly under-readable
   piano and dark primary furniture separated from their floor and wall receivers.
2. `HOUSE-03403` closes the main-stair-foot darkness with physical fixtures on the existing
   scheduled groups. Round 147's day/night stair evidence remains current.
3. `HOUSE-03405` closes the service-room night-depth item. Round 149's scheduled-night service
   views remain current.

The old Round 102 day/night set was an incremental lighting-development capture and visibly kept
the selected room groups on in both conditions. It was not evidence of the final automatic
schedule. Round 151 therefore validates the current production conditions directly rather than
trying to preserve that intentionally brighter all-on image.

No source, intensity, range, schedule class, exposure constant, renderer path, lightmap, material,
prop or asset changes in this task. In particular, the acceptance criterion's fallback for a new
ground-floor-only lighting constant was not activated: the existing house-wide solution passes.

## Current scheduled captures

- Day: `captures/house-03407-current-day-r151/`
- Night: `captures/house-03407-current-night-r151/`
- Condition: clear 10:30 and clear 22:00, respectively
- Coverage: all 23 fixed `Z-L0M` poses in each set
- Overrides: none (`--light-on` and `--light-off` were not used)

Full-resolution inspection confirms:

- the piano body, keys, score, bench, wall art and physical picture light read in both conditions;
- living and family sofas, chairs, tables and media furniture remain distinct from adjacent baked
  floors and walls without the rejected global gain;
- foyer, hall, kitchen, dining, butler's pantry and sunroom remain readable under their scheduled
  fixtures;
- the schedule preserves useful dark-to-light room hierarchy rather than flattening the connected
  ground floor;
- there is no clipping, floating fixture, z-fighting, missing texture or blocked route in the fixed
  views.

## Image measurements

Measurements use mean linear-sRGB luminance after cropping the top 100 pixels containing the HUD.
They are evidence that the scheduled set remains non-black, not a claim of performance change.

| Set | Frames | Aggregate mean | Darkest frame | Darkest mean |
|---|---:|---:|---|---:|
| clear day | 23 | 0.043056 | `hall-gallery-west` | 0.013928 |
| clear scheduled night | 23 | 0.024087 | `butlers-from-kitchen` | 0.008296 |

The specific piano-detail means are 0.020461 by day and 0.015603 at night. The current images are
deliberately dimmer than the old all-on development set while retaining normal-eye-height object
separation and legibility.

## Existing dependency evidence reused

- `docs/visual-review/house-03402-prop-lighting.md`: chosen house-wide receiver solution and the
  measured 0.25–3.50 prop/receiver band.
- `docs/visual-review/house-03403-basement-attic-stairs-lighting.md`: Round 147 physical stair
  fixtures and scheduled day/night closure.
- `docs/visual-review/house-03405-ground-service-garage-lighting.md`: Round 149 ground-service
  scheduled-night closure.

This bounded integration review is the only additional work needed for `HOUSE-03407`; further
ground-floor presentation depth belongs to the existing C4/C5 tasks after gates G3/G4.
