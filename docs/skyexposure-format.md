# `skyexposure.bin` — how much sky each room can hear

*`HOUSE-00213`. Normative. The writer is `tools/world/build_skyexposure.py`; there is no other. The
runtime reader is `HOUSE-02000`'s ambience routing and does not exist yet — the writer's own
`read_back` is the round trip that keeps the format honest until it does.*

---

## 1. What this file is for

`cna-house.md` §64.6: each cell has a precomputed **sky exposure** — *"the solid angle of open sky
reachable from the cell's centre through its windows and doors, computed offline by ray casting"* —
and a **facade exposure** per orientation. The open-air rain and wind layers are gained by
`skyExposure(cell) + Σ aperture-weighted window contributions`.

So this file holds the part that does not change, and the runtime adds the part that does. The
baked figure is deliberately the **geometric** opening, aperture-independent: `HOUSE-00210` punched
every portal out of its wall, so a ray leaving through a doorway meets no geometry and escapes on
its own, and nothing here needs to know what a portal is. Whether the window is *open* is the
runtime's multiplier, not this file's.

§64.6's own worked examples are the acceptance test: `B1_CINEMA` gets "essentially nothing", the
sunroom with its slider open gets "almost the outdoor level", and `L3_STORE_W` sits under the roof.

## 2. Conventions

`docs/anim-format.md` §2's conventions apply unchanged.

## 3. Layout

| Field | Type | Value |
|---|---|---|
| `magic` | 4 bytes | ASCII `CSKY` |
| `version` | `u32` | **1** |
| `flags` | `u32` | 0. A reader must **reject** any unknown bit |
| `worldHash` | string | `world.manifest.json`'s `worldHash` |
| `rays` | `u32` | how many directions each figure was measured with |
| `orientationCount` | `u32` | 8 |
| `orientations` | 8 × string | `N NE E SE S SW W NW` |
| `cellCount` | `u32` | |
| per cell: `id` | string | |
| `origin` | 3 × `f32` | where the listener was placed |
| `sky` | `f32` | 0 … 1, the fraction of the hemisphere that reaches open sky |
| `facade` | 8 × `f32` | the same fraction restricted to each compass sector |

`rays` travels with the file because these are Monte Carlo estimates and a consumer is entitled to
know their precision. `origin` travels with it because "the cell's centre" is not always where the
centre is — see §5.

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
cell with no free point at ear height at all is a warning, never a silent zero. The point used is
written into the file so the number can be traced back to where it was measured.

## 6. What this is not

It is not `HOUSE-00207`'s `shading_factor.py`. That is per **window**, per (sun altitude, azimuth)
on a 12 × 24 grid, and it feeds §22's daylight calculation. This is per **cell**, direction-
independent, and feeds §64.6's ambience routing. They both ray-cast against the same house and
answer different questions.
