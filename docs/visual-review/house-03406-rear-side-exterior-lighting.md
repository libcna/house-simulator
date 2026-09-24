# HOUSE-03406 rear and side exterior lighting evidence

Date: 2026-09-24

## Bounded implementation

The rear and side exterior baseline reuses the existing occupied-house schedule, physical terrace
lanterns, driveway-edge bollards, interior window light and fixed-detail spill path. The only
missing physical source was `EXT_SHED`'s bare point. One existing utility batten now backs that
source with its exact neutral emissive slot, and the source explicitly names only `EXT_GARDEN` as
an additional receiver. Its unchanged 3.30 m range represents the narrow pool visible through the
shed's east window without adding an exterior light, raising global exposure or creating a new
renderer path.

No light intensity, schedule class, exposure constant, runtime code, material family or acquired
asset changed. The front elevation and approach were re-captured as controls and were not
re-worked.

## Round 150 visual review

The established seven `Z-EXR` and four `Z-EXF` fixed poses were captured before and after at 22:00
in clear weather under the normal schedule, with no light override. Both utility-tier side yards
were also inspected from the existing property-pose coordinates; they remain zone-walk evidence
rather than being promoted to fixed polish views.

| Set | Before mean linear RGB | After mean linear RGB | Changed pixels |
|---|---:|---:|---:|
| `Z-EXR`, seven fixed poses | 0.007644 | 0.008158 | 217,377 / 10,080,000 |
| `Z-EXF`, four control poses | 0.009780 | 0.009789 | 2,433 / 5,760,000 |
| west/east side-yard walk evidence | 0.007072 | 0.007133 | 54,856 / 2,880,000 |

The intended local changes are 71,654 pixels in `shed-interior`, where the switched tubes are now
visible, and 122,464 pixels in `garden-shed-facade`, where the short window pool reveals the shed
wall and doorway. `garden-potting` changes 17,337 pixels; the other four rear controls change only
419--3,853 pixels. The front set's 2,433 pixels are confined to HUD/frame timing, demonstrating
that its already-finished lighting was re-checked rather than altered. The east side-yard numeric
delta is similarly a subtle resident/draw-order change after the exterior chunk rebuild; full-size
before/after inspection shows the same garage-window composition and no new source there.

At full resolution, the terrace lanterns retain their bounded pools, the rear house reads through
scheduled windows, the two utility side yards remain navigable silhouettes beside lit windows,
and the shed now has a visible supported source plus local window spill. Inspection found no
clipping, floating fixture, z-fighting, missing texture, blocked route, lifted background or
exposure change.

Local, Git-ignored evidence:

- `captures/house-03406-before-zexr-r150/`
- `captures/house-03406-after-zexr-r150/`
- `captures/house-03406-before-zexf-r150/`
- `captures/house-03406-after-zexf-r150/`
- `captures/house-03406-before-sideyards-r150/`
- `captures/house-03406-after-sideyards-r150/`

## Content and automated evidence

The rebuilt world is 1,384 chunks over 96 cells, 612 props, 3,119,926 vertices and 83,242,952
packed vertex bytes versus CNA's 149,756,448-byte raw model layout (44% saved). The source and
deployed chunk files match at SHA-256
`911145c173f7081b969bcd83319296031a35ad0358d4eca3625042340a785ef6`. There are still 8
16-bit-cap splits, 15 Reach-cap splits and no 32-bit chunks. All 88 cells above the six-chunk
target match their exact documented ceilings; `EXT_SHED` rises from 11 to 15 solely for the reused
batten's four existing material roles.

- All fifteen authored-world rules, exact placement for all 612 props, the fixture preparation
  gate and the 3,579-id append-only golden pass.
- The fixture regression now covers 71 cells and 177 physical sources; its focused test passes
  1/1 and the complete unit label passes 1,424/1,424 in 96.85 s wall time at `-j6`.
- `world-content-current` and the 90-cell grand tour pass 2/2 in 12.38 s.
- The complete static gate and its strict-XNA result are recorded in the task note and handoff.
