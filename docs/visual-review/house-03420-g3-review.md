# HOUSE-03420 G3 dressed-and-lit baseline review

Date: 2026-09-24

## Result

Gate G3 passes. Every accessible room remains at C3: it is traversable, dressed to its assigned
tier, lit at its required review times and has no open S1/S2. `Z-STR` remains the documented
scenery-only C2 zone and contains no accessible cell.

## Round 153 visual evidence

The standard review tool captured all 68 fixed Tier-S/High views twice, under clear 10:30 daylight
and the unmodified 22:00 schedule. Each set produced eleven zone sheets and one all-zone sheet.
No light group was forced. All 22 zone sheets were inspected at full resolution. They show
recognizable furnished rooms, intact materials and the intended occupied-house night hierarchy,
without clipping, floating props or fixtures, z-fighting, missing textures, broken shadows,
blocked routes or impossible placement.

Utility-tier cells are reviewed by day under the approved `LP-SERVICE`/`SC-OFF` rule. All 22
accessible utility cells received four cardinal views from their manifest standing point. Eight
additional views look directly toward the overhead service runs and storage pieces in the four
darkest compact spaces. These restrained rooms remain non-black and their essential equipment,
fixtures or storage silhouette is visible; no new schedule class, exposure lift or room polish was
introduced. The visible but ladder-only garage loft is not one of the 90 walk-accessible cells and
is already covered by the garage fixed view.

The Git-ignored evidence is in:

- `captures/house-03420-g3-day-r153/` — 68 frames, 11 zone sheets, one all-zone sheet;
- `captures/house-03420-g3-night-r153/` — the same coverage at scheduled night;
- `captures/house-03420-g3-utility-day-r153/` — 88 cardinal and 8 targeted utility frames.

## Objective evidence

- `capture_review.py --check`: 68 poses cover 11 zones, 12 main cells and 5 hero areas.
- `zone_scoreboard.py --check`: all 96 cells assigned exactly once across 11 zones and two explicit
  non-zone entries; every accessible zone remains C3 and zero-prop accessible rooms remain zero.
- `GrandTourTests.EveryAccessibleCellIsReachedOnFoot`: 90 cells, 558 route stops, 90,469
  controller steps, 27 collision detours, 11.46 s; pass.
- The CTest wrapper could not regenerate Ninja metadata inside the restricted environment because
  CNA's sibling SDL prebuilt lock was read-only. Running the already-built integration binary from
  the repository root exercised and passed the exact test without modifying CNA.

Rule R2 selects `HOUSE-03447` next: all M6a zones have zero completed main cells, and
`Z-STAIR` has the largest remaining main-tier target (three cells). Rule R4 keeps the ground-floor
C4 task last.
