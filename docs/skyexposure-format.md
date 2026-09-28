# `skyexposure.bin` — how much sky each room can hear

*`HOUSE-00213`. Normative. The writer is `tools/world/build_skyexposure.py`; there is no other. The
runtime reader is `cnahouse::audio::SkyExposure` (`HOUSE-01925`, which absorbs `HOUSE-02000`).
It checks v2, flags, bounded strings/counts, finite fractions/origins, orientation order,
duplicates, trailing bytes and agreement with the loaded chunk world's hash. The writer's
`read_back` and current runtime tests retain the cross-language contract.*

---

## 1. What this file is for

`cna-house.md` §64.6: each cell has a precomputed **sky exposure** — *"the solid angle of open sky
reachable from the cell's centre through its windows and doors, computed offline by ray casting"* —
and a **facade exposure** per orientation. Reduced M8 uses only geometric sky exposure for
four retained rain/wind loops; facade fields are validated but not routed. Historical
per-window contributions and open-window gameplay are not active requirements.

So this file holds the geometric part that does not change. The
baked figure is deliberately the **geometric** opening, aperture-independent: `HOUSE-00210` punched
every portal out of its wall, so a ray leaving through a doorway meets no geometry and escapes on
its own, and nothing here needs to know what a portal is. Current windows remain closed/static;
the compact audio mix is not a portal-path or per-window acoustic simulation.

Current M8's listening cases are `B1_CINEMA` (essentially nothing), `L0_SUNROOM` (clearly more
than an interior hall) and `L3_STORE_W` (strong rain transmitted through its nearby roof).
The historic open-slider/full-outdoor example is not window gameplay or an extra layer.

## 2. Conventions

`docs/anim-format.md` §2's conventions apply unchanged.

## 3. Layout

| Field | Type | Value |
|---|---|---|
| `magic` | 4 bytes | ASCII `CSKY` |
| `version` | `u32` | **2** |
| `flags` | `u32` | 0. A reader must **reject** any unknown bit |
| `worldHash` | string | `world.manifest.json`'s `worldHash` |
| `rays` | `u32` | how many directions each figure was measured with |
| `orientationCount` | `u32` | 8 |
| `orientations` | 8 × string | `N NE E SE S SW W NW` |
| `cellCount` | `u32` | |
| per cell: `id` | string | |
| `origin` | 3 × `f32` | the cell's centre — the first of its listening points |
| `samples` | `u32` | how many listening points the figures below are the mean of |
| `sky` | `f32` | 0 … 1, the fraction of the hemisphere that reaches open sky |
| `facade` | 8 × `f32` | the same fraction restricted to each compass sector |

`rays` travels with the file because these are Monte Carlo estimates and a consumer is entitled to
know their precision, and `samples` for the same reason: the error falls with the square root of
`rays × samples`, not of `rays`. `origin` travels with it because "the cell's centre" is not always
where the centre is — see §5.

**Version 2** added `samples` (`HOUSE-00779`). Version 1 measured each cell from one point and the
field would have been 1 throughout; the change is described in §5.

§14's axes: **−Z is north**, +Z is the road, +X is east.

## 4. How it is measured

Directions are a **Fibonacci hemisphere**: deterministic, uniform in solid angle, and the same
every run with no random number generator anywhere. Uniformity matters and the obvious alternative
does not have it — a latitude/longitude grid puts as many samples in the last degree below the
zenith as in the first degree above the horizon, so a skylight would be weighted like a wall of
glass. The selftest checks the distribution against `1 − cos 45° = 0.293`.

A fraction over *n* uniform directions has a standard error of `√(p(1−p)/n)`. A single interior
door subtends roughly 2 % of the hemisphere, so 512 rays put the error near 0.6 % absolute —
under the 1 % an 8-bit audio gain can represent, which is the only consumer. `--report` prints the
error actually incurred rather than this argument, and the estimator is checked against three
geometries whose answer is known exactly: open sky (1), an unbounded lid (0), and a half-plane
(0.5).

Rays are cast against the whole static world through a 2 m uniform grid marched by a 3-D DDA.
The broad phase exists for speed only, and the selftest asserts it agrees with brute force on every
ray — a broad phase that also changes answers is not an optimisation.

## 5. Where the listener stands

§64.6 says "the cell's centre", and the area-weighted centroid of the cell's boxes at 1.60 m is
that. Two things can go wrong with it, and both are handled rather than assumed away:

* an **L-shaped room's centroid can fall outside the room**;
* a **sofa can be standing on it**, and a listener inside a solid box hears nothing at all — the
  cell would report zero exposure and the rain would go silent in a room with a window.

So the centroid is tested against the geometry, and when it fails the cell's boxes are scanned on a
coarse grid for the nearest free point. Every fallback is counted and named in the report, and a
cell with no free point at ear height at all is a warning, never a silent zero.

**A third thing goes wrong, and it is the one that needed the format changed** (`HOUSE-00779`): a
centroid can be free, inside the room, and still unable to see any of the room's own windows.
`L3_ROOM` is a T — a body 7 m deep with two dormer bays reaching the front wall — and every
straight line from its centroid to any of its three dormers leaves the room through `L3_STORE_S` on
the way. A room §13.6 calls *"lit by three dormers"* measured **0.000**, and no amount of testing
the point for solidity would have found it, because the point was fine. One point in a room can
only ever be one point in a room.

So the figure is the **mean over the cell's floor**. The listening points are a grid at
`SAMPLE_STEP` = 1.0 m over the cell's boxes at ear height, with the centre first; a point standing
inside geometry is dropped rather than measured as silence. The step grows with the cell,
using `MAX_SAMPLES` = 16 as a spacing target — `EXT_WORLD` is 200 m across and a metre grid over
it would be 40 000 points. This is not a wire cap: centre plus per-box grids can exceed 16 in thin
or multi-box cells (the current rear yard has 22). Runtime accepts 1–4096 samples; this is confidence
metadata, not an allocation or runtime ray-casting count. The historical bake came to
**997 points over 96 cells**. `origin` is still the
centre and `samples` says how many points the figure averages, so a number can still be reproduced;
what it can no longer be is traced to a single place, because it no longer comes from one.

## 6. What this is not

It is not `HOUSE-00207`'s `shading_factor.py`. That is per **window**, per (sun altitude, azimuth)
on a 12 × 24 grid, and it feeds §22's daylight calculation. This is per **cell**, direction-
independent, and feeds §64.6's ambience routing. They both ray-cast against the same house and
answer different questions.
