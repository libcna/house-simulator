# HOUSE-03403 basement, attic and stair lighting evidence

Date: 2026-09-24

## Bounded implementation

The first zone-lighting band reuses four existing fixture families and the existing authored light
groups, schedule, lightmap baker and static-prop path. Forty-four physical fixture props now back all 46
sources in the 25 cells of `Z-B1`, `Z-L3` and `Z-STAIR`. Every source names its physical prop and
that model's exact independently switched emissive material slot. No runtime code, exposure
constant, material family, asset or subsystem was added.

The basement keeps a neutral utility character, with warm ceiling fittings in the cinema and WC
and compact task fittings at the gym mirror, workbench and hobby table. The three basement-hall
sources are distributed along the full hall rather than occupying one point. The attic stores and
attic flights use the utility fitting; the finished attic room uses the existing family ceiling
fitting and compact side-lamp family. The main stair uses the same family fitting plus one existing
puck at its foot. Only scheduled/dark cells needed bounded flux changes. `L3_ROOM` alone uses a
100 lm/W bake conversion for its two main fittings and desk lamp; the default 683 lm/W conversion
left that roof-bounded room visibly under the C3 floor. Global exposure is unchanged.

## Bake and content measurements

Targeted 256-sample promotion rebuilt 25 daylight products in 74.6 s and 37 artificial products
in 125.5 s, followed by bounded `B1_HOBBY`/`L3_ROOM` fixture corrections and a final `L3_ROOM`
calibration bake. The final attic-room artificial peaks are 0.9893 for its main group and 0.0370
for its desk group. The generated cell bindings, bake manifests, fifteen changed artificial PNGs
and asset hashes are current.

The rebuilt world is 1,209 chunks over 96 cells, 484 props, 3,032,941 vertices and 80,459,432
packed vertex bytes versus CNA's 145,581,168-byte raw layout (45% saved). The pre-task furnished
world was 1,114 chunks, 440 props, 3,010,601 vertices and 79,744,552 packed bytes. The increase is
the measured physical-fixture material roles: there are still exactly 8 16-bit-cap splits, 15 Reach
splits and no 32-bit chunk. All 81 cells above the six-chunk target exactly match their documented
ceilings. The deployed `chunks.bin` SHA-256 is
`1d6fb660962fc9be2906cc955d3a22d0c708dc03b8de0be45eae188f3bc579f8`.

## Round 147 visual review

Night before/after frames use the fixed zone poses. Mean linear RGB was measured over the view
after excluding the HUD:

| View | Before | After | Ratio |
|---|---:|---:|---:|
| basement cinema | 0.006567 | 0.008967 | 1.37x |
| basement gym | 0.011142 | 0.072012 | 6.46x |
| basement hall | 0.014523 | 0.050916 | 3.51x |
| basement workshop | 0.011647 | 0.064119 | 5.51x |
| attic room | 0.004511 | 0.023030 | 5.11x |
| attic stair foot | — | — | 3.31x |
| attic stair head | — | — | 2.48x |
| basement stair foot | — | — | 1.97x |
| basement stair head | — | — | 1.51x |
| main stair foot | — | — | 1.25x |
| main stair L1 transition | — | — | 1.64x |
| main stair L2 exit | — | — | 2.56x |

The cinema remains deliberately dim but readable. The scheduled attic stores remain off at night,
as required by `SC-OFF`; their day frames remain readable from the dormer/daylight bake. The final
day and night sets show visible physical fixtures, a readable attic room and basement working
spaces, and a closed main-stair-foot S2. Inspection found no fixture clipping, floating geometry,
z-fighting, missing texture or blocked flight.

Local, Git-ignored evidence:

- `captures/house-03403-before-b1-night-r147/`
- `captures/house-03403-after-b1-night-r147/`
- `captures/house-03403-final-b1-day-r147/`
- `captures/house-03403-before-l3-night-r147/`
- `captures/house-03403-final-l3-night-r147/`
- `captures/house-03403-final-l3-day-r147/`
- `captures/house-03403-before-stair-night-r147/`
- `captures/house-03403-after-stair-night-r147/`
- `captures/house-03403-final-stair-day-r147/`

## Automated evidence

- Schema generation check, schema validation, all fifteen authored-world rules, the zone
  scoreboard and exact chunk-budget check pass.
- The fixture regression proves the exact 25-cell target set, 46 sources, resolvable physical props
  and non-empty emissive slots; the focused lighting set passes 39/39.
- The complete unit label passes 1,424/1,424.
- Integration passes 140/141 under six-way load; the sole known weather-clock threshold miss
  (`137.688 > 138`) passes in the exact focused retry together with `world-content-current` (2/2).
- Strict XNA compiles all 325 translation units clean with six workers. The complete static gate
  reaches that result with every project-owned check green and exits nonzero only for the known
  user-owned root `.claude` layout entry, which remains untouched.
