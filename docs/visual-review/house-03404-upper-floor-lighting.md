# HOUSE-03404 upper-floor lighting evidence

Date: 2026-09-24

## Bounded implementation

The upper-floor lighting pass reuses seven existing fixture and lamp families, the authored light
groups, the occupied-house schedule, the existing lightmap baker and the static-prop path. One
hundred ten new fixture props plus the two already-present linked practicals now back all 112 light
sources in the 35 accessible cells of `Z-L1` and `Z-L2`. Every source names its physical prop and
the model's exact independently switched emissive slot. No runtime code, exposure constant, new
material family, acquired asset or subsystem was added.

Ceiling and utility fittings follow the room-type recipes. Existing desks, nightstands, dressers,
side tables and reading corners carry the bounded desk, bedside and floor lamps. The existing
second-floor sitting-room lamp was switched to its already-authored lit variant. The rear balcony
uses the existing porch-lantern and puck families; the front balcony retains its established
linked lantern. A desk-lamp paint slot that cannot use the `DualTextureEffect` UV1 contract reuses
an existing Basic painted finish rather than adding a UV pipeline or material family.

## Bake and content measurements

The final targeted 256-sample products cover the 33 receiver cells (the two balconies use the
existing dynamic exterior treatment): 33 daylight atlases and 66 artificial group atlases. The
initial upper-floor runs took 96.0 s for daylight and 256.0 s for artificial light. A bounded
placement correction re-ran only five affected cells in 18.6 s daylight and 52.8 s artificial,
then promoted both modes together. Seventeen artificial PNGs differ from the pre-task tree; the
unchanged products remained byte-identical.

The rebuilt world is 1,345 chunks over 96 cells, 594 props, 3,110,604 vertices and 82,944,648
packed vertex bytes versus CNA's 149,308,992-byte raw layout (44% saved). The preceding
`HOUSE-03403` world was 1,209 chunks, 484 props, 3,032,941 vertices and 80,459,432 packed bytes.
There are still exactly 8 16-bit-cap splits, 15 Reach-cap splits and no 32-bit chunk. All 86 cells
above the six-chunk target exactly match their documented ceilings. The deployed `chunks.bin`
SHA-256 is `3e35aa0d8f41d54e94b0f12b5ec7e0478c0360a1b9d18d6bebc6de1e0d38fc84`.

## Round 148 visual review

Six fixed views in each zone were captured before and after under clear 10:30 daylight and the
normal 22:00 schedule. The aggregate mean is measured in linear RGB over each six-frame set:

| Zone and condition | Before | After | Changed pixels / 8,640,000 |
|---|---:|---:|---:|
| `Z-L1` clear day | 0.277012 | 0.276942 | 57,854 |
| `Z-L1` scheduled night | 0.256023 | 0.256978 | 3,463,305 |
| `Z-L2` clear day | 0.270673 | 0.270716 | 85,158 |
| `Z-L2` scheduled night | 0.256875 | 0.258544 | 4,148,483 |

Day exposure stays within 0.03% of its pre-task baseline while the added physical fixture geometry
is visible. At night the existing schedule activates the ceiling, bedside, desk, reading, wet-room
and circulation groups without review-only flags. The broad changed-pixel coverage is the expected
fixture/light response rather than a global lift. Full-resolution inspection found no floating or
overlapping fixture, z-fighting, missing texture, impossible support, blocked route or newly black
room. Secondary spaces stop at functional C3 light; no C4/C5 polish was pulled forward.

Local, Git-ignored evidence:

- `captures/house-03404-before-l1-day-r148/`
- `captures/house-03404-after-l1-day-r148/`
- `captures/house-03404-before-l1-night-r148/`
- `captures/house-03404-after-l1-night-r148/`
- `captures/house-03404-before-l2-day-r148/`
- `captures/house-03404-after-l2-day-r148/`
- `captures/house-03404-before-l2-night-r148/`
- `captures/house-03404-after-l2-night-r148/`

## Automated evidence

- Schema and all fifteen authored-world rules pass; all 594 static props pass exact placement and
  route checks.
- The fixture regression covers 60 cells and 158 sources, including the exact 35-cell upper-floor
  target, and proves resolvable physical props plus non-empty emissive slots.
- Fixture preparation, manifest/licence generation, world ids and the exact chunk-budget report
  pass.
- The focused authored-light regression passes 1/1, and the complete unit label passes
  1,424/1,424 at `-j6` in 158.29 s.
- Strict XNA compiles all 325 translation units clean with six workers. The complete static gate
  leaves every project-owned check green and exits nonzero only for the known user-owned root
  `.claude` layout entry, which remains untouched. `world-content-current` and the 90-cell grand
  tour pass 2/2.
