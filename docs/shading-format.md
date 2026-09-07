# `shading.bin` — how much of each window the sun reaches

*`HOUSE-00207`. Normative. The writer is `tools/blender/shading_factor.py`; there is no other. The
runtime reader is `HOUSE-01263`'s daylight model and does not exist yet — the writer's own
`read_back` is the round trip that keeps the format honest until it does.*

---

## 1. What this file is for

`cna-house.md` §22's daylight sum multiplies, per window:

```
daylight(cell) = Σ_w window_w.area · transmission(w) · openBoost(w)
                     · skyExposure(w.orientation, sunAlt, sunAz, cloudCover)
                     · shadingFactor(w)            ← this file
                 / cell.floorArea
```

and §22 says what `shadingFactor` is: *"precomputed offline per window per (sun altitude, azimuth)
on a 12 × 24 grid by ray-casting against the house and neighbour geometry in Blender — so the porch
roof genuinely keeps the sun out of the foyer in the afternoon, and the west neighbour's gable
shades the study at sunset. 81 windows × 288 samples × 1 byte = 23 KB. Cheap, and it is the single
detail that makes interior daylight feel real."*

This is **occlusion only**. Whether the sun is above the horizon, and whether it is within ±75° of
the window's outward normal, are §22's `skyExposure` and are not baked here — so the grid stays a
pure property of the geometry and does not have to be rebuilt when that analytic function changes.

## 2. Conventions

`docs/anim-format.md` §2's conventions apply. Angles in this file are **degrees**, because they
index a table rather than enter a calculation.

§14's axes decide the azimuth: right-handed, Y up, **−Z is north**, +Z is the road. Azimuth 0 is
north, 90 is east (+X), 180 is south (+Z). Getting this wrong produces no error — it produces a
house whose windows are shaded at the wrong time of day, and it looks plausible all afternoon.

## 3. Layout

| Field | Type | Value |
|---|---|---|
| `magic` | 4 bytes | ASCII `CSHF` |
| `version` | `u32` | **1** |
| `flags` | `u32` | 0. A reader must **reject** any unknown bit |
| `altitudeSteps` | `u32` | **12** |
| `azimuthSteps` | `u32` | **24** |
| `samples` | `u32` | rays per axis across the window, the grid was measured with |
| `windowCount` | `u32` | |
| per window: `id` | string | the `layout.openings.json` id |
| `cell` | string | the interior cell it lights |
| `normal` | 3 × `f32` | the outward normal, for §22's ±75° test |
| `grid` | `altitudeSteps × azimuthSteps` bytes | altitude-major |

`grid[a * azimuthSteps + z]` is `round(fraction × 255)` for altitude node `a` and azimuth node `z`.

## 4. The grid is NODES, not cells

Altitude is sampled **at** `90·a/11` for `a` in 0…11 — so 0° and 90° are measured, not extrapolated
— and azimuth **at** `15·z` for `z` in 0…23, wrapping from 345 back to 0 with no seam. The runtime
interpolates bilinearly between four samples, and node sampling makes that exact at the ends.

Cell-centre sampling would leave the horizon and the zenith to be guessed from the nearest interior
sample, and the horizon is where a low sun and a long shadow make the difference most visible.

## 5. The window is sampled, not probed

A single ray from the window's centre answers a yes/no question, and the answer this feeds is not
yes/no: §22 *multiplies* by it, so a window half-covered by an eave must read about a half. One ray
reads 1 until the shadow crosses the centre and then 0, so the foyer's daylight would step rather
than slide as the sun moved — once per window per afternoon.

`samples × samples` points spread over the window's own rectangle give the fraction directly, at
`samples²` rays per node. The default 4 × 4 puts §22's 81 windows over 288 nodes at 373 000 rays,
which a BVH answers in seconds.

Each ray starts 2 mm off the glass along the outward normal. Starting exactly on the surface lets a
ray hit the face it left, and the window would shade itself completely; 2 mm is an order of
magnitude below the thinnest wall the layout can build, so it cannot push a sample through the
geometry it is measuring.

## 6. Two shortcuts and one trap

**A sun behind the window's own wall is stored as 0 without casting.** The geometry would report the
same thing — the wall is in the way — but only after `samples²` rays, and this is half the grid.

**Outward is decided by the geometry.** The interior cell is the one that is not `exterior`, and
outward is away from its boxes — not from whether the portal happens to name it `cellA` or `cellB`.
A window authored the other way round would otherwise cast every ray into the room it is lighting.

**The trap** is placing a test obstruction where no ray can reach it. A south-facing window's rays
only ever travel towards +Z, so nothing due *west* of it, at the same Z, can ever be occluded — the
first version of this tool's fixture put the neighbour's gable there and the claim silently measured
the eave instead. The fixture now compares south-west against south-east, and proves the gable is
responsible by deleting it and re-baking.

## 7. One byte

§22 budgets one byte per sample, so a factor is `round(fraction × 255)` — **rounded, not
truncated**; with 16 samples the fractions are sixteenths, and 3/16 rounds to 48 and truncates to
47. The quantum is 0.4 %, well under what an 8-bit daylight response can show.

81 windows × 288 bytes is 23 KB of grid, which is §22's figure.
