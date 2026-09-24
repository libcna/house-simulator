# HOUSE-03405 ground-service and garage lighting evidence

Date: 2026-09-24

## Bounded implementation

The ground-service and garage lighting pass reuses four existing fixture families, the authored
light groups, the occupied-house schedule, the existing lightmap baker and the static-prop path.
Seventeen new fixture props plus the office's existing desk lamp now back all 18 authored sources
in the eight `Z-L0S` cells, the garage and its visible non-walkable loft. Every source names its
physical prop and exact independently switched emissive slot. No runtime code, exposure constant,
new material family, acquired asset or subsystem was added.

The service rooms follow their existing `LP-SERVICE`, `LP-WET` and `LP-TASK` recipes. Utility
battens remain `SC-OFF` and readable by day; the office's ceiling and physical desk lamp follow
the task schedule. Four reused neutral battens give the garage its fluorescent utility character,
the existing opener group gets a small physical practical, and the visible loft receives the same
bounded utility fitting.

## Bake and content measurements

The final targeted 256-sample products cover all 10 receiver cells: 10 daylight atlases and 13
artificial group atlases. Artificial baking took 42.5 s and daylight baking 35.4 s. Most source
positions were deliberately unchanged, so only four committed artificial PNGs differ: the office
main and desk products and the two WCs whose physical semi-flush fittings use the established
lower optical origin. The unchanged products remained byte-identical.

The rebuilt world is 1,380 chunks over 96 cells, 611 props, 3,119,436 vertices and 83,227,272
packed vertex bytes versus CNA's 149,732,928-byte raw layout (44% saved). The preceding
`HOUSE-03404` world was 1,345 chunks, 594 props, 3,110,604 vertices and 82,944,648 packed bytes.
There are still exactly 8 16-bit-cap splits, 15 Reach-cap splits and no 32-bit chunk. All 88 cells
above the six-chunk target exactly match their documented ceilings. Source and deployed
`chunks.bin` match at SHA-256
`f8f8115250c57cd2c0368a0104bc6d71dbc63e4adb1987f711b84e61e6c6d971`.

## Round 149 visual review

Two fixed views in each zone were captured before and after under clear 10:30 daylight and the
normal 22:00 schedule. The aggregate mean below converts sRGB samples to linear RGB before taking
the mean over each two-frame set:

| Zone and condition | Before | After | Changed pixels / 2,880,000 |
|---|---:|---:|---:|
| `Z-L0S` clear day | 0.038523 | 0.038372 | 75,462 |
| `Z-L0S` scheduled night | 0.011547 | 0.014054 | 876,638 |
| `Z-GAR` clear day | 0.031130 | 0.031262 | 27,212 |
| `Z-GAR` scheduled night | 0.007407 | 0.009211 | 581,985 |

Day exposure stays within 0.5% of its pre-task baseline while the physical fixture geometry is
visible. At night the office gains warm task depth and the garage's four neutral battens read as
the intended utility composition. Mudroom, laundry, closets and stores remain deliberately off
under `SC-OFF`; their clear-day captures and baked products establish the C3 baseline without
turning a service recipe into all-night decorative lighting. Full-resolution inspection found no
floating or overlapping fixture, z-fighting, missing texture, impossible support, blocked route
or newly black room.

Local, Git-ignored evidence:

- `captures/house-03405-before-l0s-day-r149/`
- `captures/house-03405-after-l0s-day-r149/`
- `captures/house-03405-before-l0s-night-r149/`
- `captures/house-03405-after-l0s-night-r149/`
- `captures/house-03405-before-gar-day-r149/`
- `captures/house-03405-after-gar-day-r149/`
- `captures/house-03405-before-gar-night-r149/`
- `captures/house-03405-after-gar-night-r149/`

## Automated evidence

- Schema and all fifteen authored-world rules pass; all 611 static props pass exact placement and
  route checks.
- The fixture regression covers 70 cells and 176 sources, including the exact ten-cell target,
  and proves resolvable physical props plus non-empty emissive slots.
- Fixture preparation, manifest/licence generation, world ids and the exact chunk-budget report
  pass.
- The focused authored-light regression passes 1/1, and the complete unit label passes
  1,424/1,424 at `-j6` in 121.51 s wall time.
- Strict XNA compiles all 325 translation units clean with six workers. The complete static gate
  leaves every project-owned check green and exits nonzero only for the known user-owned root
  `.claude` layout entry, which remains untouched.
- `world-content-current` and the 90-cell grand tour pass 2/2 in 16.04 s.
