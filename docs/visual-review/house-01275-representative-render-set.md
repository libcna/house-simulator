# HOUSE-01275 representative day/night render set

Date: 2026-09-24

## Result

The committed render-regression set contains exactly 32 Tier-S/High software-renderer frames:
one existing fixed review pose for each of the eleven planning zones and one for each of the five
retained hero areas, each at clear 10:30 and scheduled 22:00. No light group is forced on or off.

| Coverage | Existing review pose |
|---|---|
| `Z-B1` | `basement-workshop` |
| `Z-L0M` | `butlers-from-kitchen` |
| `Z-L0S` | `ground-office` |
| `Z-GAR` | `garage-interior` |
| `Z-L1` | `bedroom-2` |
| `Z-L2` | `games-room` |
| `Z-L3` | `attic-store-west` |
| `Z-STAIR` | `main-stair-foot` |
| `Z-EXF` | `garage-approach` |
| `Z-EXR` | `garden` |
| `Z-STR` | `neighbourhood-street` |
| `H1` | `exterior-front` |
| `H2` | `living-composition` |
| `H3` | `kitchen-facing-west` |
| `H5` | `library` |
| `H6` | `basement-cinema` |

The test fixes 640x360 output, Tier S, High quality, seed `6840335469064670721`, clear weather,
the production walk scene, frozen simulation time and the exact coordinates already owned by
`tools/visual/capture_review.py`. A coverage test requires the precise eleven-zone/five-hero sets.
The software-renderer test compares all pixels outside the named frame-time HUD region with a
per-channel tolerance of two and requires fewer than 0.2% differing pixels. A hardware run still
captures every frame but does not compare hardware pixels with Mesa references.

## Deterministic capture boundary

The initial implementation copied the review tool's third-frame capture. Thirty-one scenes passed,
but the final dark `night:basement-cinema` frame varied by 15.55–16.38% of compared pixels between
processes (mean channel delta 0.547–0.577). Raising the tolerance or masking the room was rejected.

Two independent first-frame cinema captures were bit-identical, but full-set inspection showed
CNA's known initial sampler state on some wooden surfaces. Two independent second-frame cinema
captures were also bit-identical (SHA-256
`037c3c497cb974117c5a647947a064994e7d3f027617b05f395c681b8aba8149e` at 1600x900) after the
sampler warm-up. Frame two is therefore the bounded deterministic capture point: it preserves the
correct material sampling without accumulating the variable wall time seen by frame three.

## Inspection and validation

Both final 4x4 contact sheets were inspected at full resolution. The day set has readable room
purpose and exterior context; the scheduled-night set shows the authored occupied-house hierarchy.
The attic store remains deliberately dark at night with a lit adjoining doorway. The selected
main-stair view shows the complete flight rather than the near-blank wall visible in the initially
considered basement-stair pose. No clipping, floating prop, z-fighting, missing texture, broken
shadow, blocked route or impossible placement is visible.

The disabled regeneration test wrote all 32 references through Mesa 25.0.7 in 140.934 s. A fresh
profile then ran the two active tests in 145.105 s: exact coverage passed and all 32 day/night
frames stayed within the declared render tolerance. All 1,424 unit tests pass with four workers.
The complete static gate passes every project-owned check, including all 326 strict-XNA translation
units with four workers; it exits nonzero only for the known user-owned root `.claude` layout entry.
