# `sunpatch.bin` — the shape the sun makes on the floor

*`HOUSE-00208`. Normative. The writer is `tools/blender/sun_patch.py`; there is no other. The
runtime reader is `HOUSE-01268`'s sun-patch decals and does not exist yet — the writer's own
`read_back` is the round trip that keeps the format honest until it does.*

---

## 1. What this file is for

`cna-house.md` §29.1: blob shadows *"cannot cast a chair's shape onto a wall, or the window frame's
shape onto the floor — and the second of those is the one that matters most in a house. So Tier S
adds one more thing: baked window 'sun patch' decals. For each window, offline, we precompute the
polygon the sun casts through it onto the room's floor and walls, for the same 12 × 24 (altitude,
azimuth) grid used by `shadingFactor`. At runtime the two nearest grid entries are interpolated and
the patch is drawn as an additive, softly-edged quad-strip tinted by the sun colour and scaled by
`(1 − cloudCover)³`."*

The grid is [`shading-format.md`](shading-format.md)'s, node for node, because §29.1 says so and
because a patch and its shading factor are read together.

## 2. "The two nearest grid entries are interpolated" decides the whole format

Two polygons of different shapes cannot be blended vertex by vertex unless they have the same
vertices in the same order. So a patch is **not** the outline of wherever the light happens to
land. It is a fixed **(n+1) × (n+1) lattice**, one point per corner of an n × n subdivision of the
window, in the same order at every one of the 288 nodes. Node 5 and node 6 then interpolate by a
lerp per point, with nothing to match up and no vertex appearing or disappearing as the sun moves.

Two consequences follow directly, and neither is optional:

* **A ray that hits nothing still produces a point**, clamped inside the room. Dropping it would
  change that node's vertex count and break the node it is interpolated with.
* **The lattice is row-major along the window's width.** §29.1 draws a quad-*strip*, and a strip's
  indices assume consecutive points run along a row.

## 3. Subdividing is what lets the patch bend

§29.1 asks for a quad-strip rather than a quad, and the reason appears the moment the sun is low:
the patch runs across the floor and part-way up the far wall. Projecting only the window's four
corners gives a flat quad that cuts through the wall. Projecting a lattice lets each little quad
land on whatever surface its own corners hit, so the patch folds along the floor/wall junction by
construction rather than by a special case.

## 4. Layout

| Field | Type | Value |
|---|---|---|
| `magic` | 4 bytes | ASCII `CSUN` |
| `version` | `u32` | **1** |
| `flags` | `u32` | 0. A reader must **reject** any unknown bit |
| `altitudeSteps` | `u32` | **12** |
| `azimuthSteps` | `u32` | **24** |
| `subdivisions` | `u32` | *n*; the lattice is (n+1)² points |
| `windowCount` | `u32` | |
| per window: `id` | string | |
| `cell` | string | |
| `bounds` | 6 × `f32` | the frame the `u16` coordinates are relative to |
| per node (`altitudeSteps × azimuthSteps`, altitude-major): | | |
| `present` | `u8` | 0 = the sun cannot shine in; **no further bytes for this node** |
| `points` | (n+1)² × 3 × `u16` | only when `present` |

A coordinate decodes as `bounds.min + code / 65535 × (bounds.max − bounds.min)`. Over a 10 m room
that is 0.15 mm — far finer than a soft-edged decal can show, and measured rather than assumed.

## 5. What it costs

At *n* = 3 a present node is 96 bytes plus its flag. Half the grid is empty, because the sun is
behind the window's own wall and that is stored as one byte, so a window is about **14 KB** and
§22's 81 windows about **1.1 MB**.

`cna-house.md` §72 has no line for this yet, and that is the number it needs. It is an order of
magnitude above `shading.bin`'s 23 KB, which is the price of storing geometry rather than a scalar,
and `--subdivisions` is the dial if it has to come down.

## 6. Two things the fixture had to be built to show

**The light travels opposite to the sun.** `sun_direction` points *from* a surface *towards* the
sun, so inside the room the ray runs along its negation. Backwards, every patch lands on the lawn,
where nothing sees it and nothing complains.

**"A low sun throws a longer patch" is false in a real room.** It was the first claim written here
and it does not hold: four metres from the window the back wall truncates the patch, so a low sun's
extent (2.33 m on the fixture) is no larger than a high sun's (2.07 m). What is true in any room —
and what §29.1 actually asks to see — is that the patch **moves**, monotonically, sweeping about
4 m of floor as the sun climbs, with no reversal for the eye to catch.
