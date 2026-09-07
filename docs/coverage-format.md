# `coverage.bin` — the rain coverage height field

*`HOUSE-00212`. Normative. The writer is `tools/world/build_coverage.py`; there is no other. The
runtime reader is `HOUSE-01744`'s roof-mask test and does not exist yet — the writer's own `read_back`
is the round trip that keeps the format honest until it does.*

---

## 1. What this file is for

`cna-house.md` §37.2: a 0.5 m grid over the property storing, per cell, the height of the lowest
roof or soffit above it, or +∞. A rain particle is drawn only if `particle.y > coverage(x, z)`;
particles below a roof are teleported to the top of the volume.

The consequence §37.2 asks for is specific and visible, and is what the tool is judged against:
*"standing under the porch in a downpour, the rain visibly stops at the porch edge."*

## 2. Conventions

`docs/anim-format.md` §2's conventions apply unchanged: little-endian, IEEE-754 binary32, explicit
fixed-width integers, `u16`-length UTF-8 strings, metres, right-handed Y-up with −Z north, no
alignment, no seeking.

## 3. Layout

| Field | Type | Value |
|---|---|---|
| `magic` | 4 bytes | ASCII `CCOV` |
| `version` | `u32` | **1** |
| `flags` | `u32` | 0. A reader must **reject** any unknown bit |
| `worldHash` | string | `world.manifest.json`'s `worldHash` |
| `originX`, `originZ` | 2 × `f32` | the grid's minimum corner |
| `cell` | `f32` | **0.5** |
| `ground` | `f32` | the height rain lands at; below it a slab is foundation, not shelter |
| `nx`, `nz` | 2 × `u32` | |
| `field` | `nx × nz` × `f32` | row-major, `z` outer, `x` inner |

Uncovered cells hold an actual **IEEE +∞**, not a large finite sentinel. `particle.y > coverage`
is then false for every particle with no constant that the writer and the runtime have to agree on,
and `f32` carries infinity exactly.

The lookup is `i = floor((x − originX) / cell)`, `j = floor((z − originZ) / cell)`, out of range
being uncovered. There is no interpolation: a field holding +∞ cannot be interpolated, and a soffit
is a step, not a ramp.

## 4. How it is generated

**The soffits are already built.** §37.2 attributes the mask to `terrain_gen.py`, but the geometry
it needs is not terrain: a porch soffit is the underside of the balcony floor above it, and the
roof underside is the top cell's ceiling. `HOUSE-00210` derives every one of those slabs from the
layout, so this tool asks `build_collision` for them rather than re-deriving them from the same
JSON with a second set of rules that can drift from the first.

So the whole rule is: **coverage(x, z) is the lowest underside of any floor or ceiling slab above
the ground at (x, z)**, +∞ where there is none. That one rule covers §37.2's entire list — house,
garage, porch, balcony, sunroom, shed — because each of those is a cell, and a cell has a ceiling
and the thing above it has a floor.

Three decisions inside it:

* **Walls do not shelter.** Rain does not fall on a vertical surface from above, and a wall counted
  as cover would shelter the 0.15 m strip it stands in and nothing else.
* **Below-ground slabs do not shelter.** A basement floor is 2.6 m under the lawn; without the
  ground test every square metre over the basement reports itself sheltered, and the rain stops in
  mid-air over the garden, from a slab nobody can see.
* **A sample is its cell's centre, and the field is not conservative.** Marking a cell covered when
  *any part of it* is under a roof biases every soffit outward by up to half a grid step, so the
  rain would stop 0.25 m short of the porch edge, in open air — which reads as a bug rather than as
  shelter. Centre sampling puts the error on either side of the true edge instead.

The field extends past the building's footprint, because rain is drawn around the camera and the
camera stands in the garden. Its extent is `layout.exterior.json`'s terrain if declared, otherwise
the built footprint plus 8 m.

## 5. Reviewing one

`--ascii` prints a plan view: `.` for open sky, a digit for the height band of whatever is
overhead. A coverage field is a grid of numbers in which a porch accidentally extending three
metres into the garden looks exactly like a porch, and the plan view is where that shows up.
