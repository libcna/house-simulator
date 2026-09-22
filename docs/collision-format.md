# `collision.bin` — the static collision world

*`HOUSE-00210`. Normative. The writer is `tools/world/build_collision.py`; there is no other. The
runtime reader is `src/physics/CollisionLoader.cpp` (`HOUSE-00541`). Three checks keep the two
honest, because `HOUSE-00225` is the standing lesson — a reader and a writer each tested against
their own hand-written fixtures both passed while producing and expecting different bytes: the
writer's own `read_back`, `CollisionLoaderTests` over bytes built by hand, and
`CollisionRoundTripTests` over a fixture `build_collision.py --fixture` writes at build time.*

**The reader checks what this document PROMISES, not only what it can parse.** A bucket entry
outside its cell's own shape list, a triangle outside its mesh's vertices, a shape index outside
the world's shapes, a surface index outside the table, a kind this build does not know, a negative
half-extent, an inverted bounding box, a duplicate cell id: each is a read past the end of an array
in the sweep or a shape silently behaving as something it is not, and each is one comparison at
load. A cell grid above 1 048 576 buckets is refused too — a cell's grid is sized from the union of
its own shapes, so one shape that escaped its cell sizes it, and the writer refuses to produce such
a file for the same reason.

---

## 1. What this file is for

`cna-house.md` §49.1 rejects a third-party physics engine, so §49.2's representation is the whole
of static collision: per cell a list of **OBBs** and a short list of **triangle meshes**, indexed
by a **1 m loose grid** so that a capsule sweep tests about six shapes rather than every shape in
the room.

The file is built offline from `assets-src/world/*.json` and the per-asset `_COL` proxies. Nothing
at runtime derives collision from render geometry, and nothing authored names an OBB: the layout
describes rooms, and §3 below is where rooms become walls.

## 2. Conventions that apply to the whole file

These are `docs/anim-format.md` §2's conventions, deliberately unchanged — two project-owned binary
formats that disagreed about endianness or string encoding would be two chances to get a reader
wrong.

| | |
|---|---|
| Byte order | **Little-endian**, always, on every platform. |
| Floats | IEEE-754 binary32, little-endian. |
| Integers | Fixed width and explicit — `u32`, `u16`, `u8`. Never a 7-bit-encoded length prefix. |
| Strings | `u16` byte length, then that many **UTF-8** bytes, no terminator. |
| Units | Metres. |
| Angles | **Radians**, unlike the authored JSON, which says `yawDeg` and means degrees. The file is read straight into code and code works in radians (`world-format.md`). |
| Axes | Right-handed, **Y up**, **−Z is north**, +Z is the road (`cna-house.md` §14). Vectors are *x y z*. |
| Alignment | None. The file is read sequentially and never memory-mapped or cast over, so no padding exists and none may be assumed. |

**No section offsets, no index table, no seeking.** The file is read forward, once. An offset table
is a second description of the layout that can disagree with the first.

## 3. Layout

### 3.1 Header

| Field | Type | Value |
|---|---|---|
| `magic` | 4 bytes | `0x43 0x43 0x4F 0x4C` — ASCII `CCOL` |
| `version` | `u32` | **3** — version 1 had no §3.5, version 2 no `outdoors` (§3.4) |
| `flags` | `u32` | 0. Reserved; a reader must **reject** a file with any unknown bit set rather than ignore it |
| `worldHash` | string | `world.manifest.json`'s `worldHash`, or empty when the layout has no manifest |
| `gridCell` | `f32` | The loose grid's cell size in metres, **1.0** |

`worldHash` is what makes a stale collision file detectable: the layout changed, the hash changed,
and a runtime that loads a `collision.bin` whose hash does not match the world it is loading is
loading walls that are no longer where the rooms are.

`gridCell` is written rather than assumed so the reader does not carry a second copy of the
constant that could disagree with the writer's.

### 3.2 Surface table

| Field | Type | Value |
|---|---|---|
| `surfaceCount` | `u32` | |
| `surfaces` | `surfaceCount` × string | e.g. `tile`, `wood`, `concrete` |

Every shape names its surface by index into this table. Footstep surfaces repeat across thousands
of shapes and are a handful of distinct strings; the table is the difference between a few dozen
bytes and a few hundred kilobytes.

### 3.3 The shape pool

Shapes are **global and shared**, not per cell. A partition wall bounds the room on each side of
it and must be collided with from both, but it is one wall; cells reference it by index (§3.4).
Without the pool every shared surface — every partition, and every floor slab that is also the
ceiling slab below — is written twice, and §49.2's estimate of "~4 300 OBBs" stops describing the
file it is an estimate for.

**OBBs.**

| Field | Type | |
|---|---|---|
| `obbCount` | `u32` | |
| then per OBB, **31 bytes**: | | |
| `centre` | 3 × `f32` | world space |
| `halfExtents` | 3 × `f32` | |
| `yaw` | `f32` | **radians**, about +Y |
| `surface` | `u16` | index into §3.2 |
| `kind` | `u8` | 0 floor · 1 ceiling · 2 wall · 3 stair · 4 prop · 5 exterior |

A single `yaw` rather than a 3×3 basis: the house is rectilinear and `layout.props.json` gives a
prop one `yawDeg` and no other rotation, so a full basis would store 8 floats that are always
derivable from one. This is a **format** restriction, and a future asset that needs pitch or roll
needs a version 2, not a workaround.

**Triangle meshes**, for the curved minority §49.2 keeps them for — stair ramps, the terrain patch,
the roof underside, and any `_COL` proxy that is genuinely not a box.

| Field | Type | |
|---|---|---|
| `meshCount` | `u32` | |
| then per mesh: | | |
| `surface` | `u16` | |
| `kind` | `u8` | |
| `bounds` | 6 × `f32` | min *xyz*, max *xyz*; written so the reader does not recompute it |
| `vertexCount` | `u32` | ≤ 65 535 — `u16` indices address no more, and the writer refuses a proxy that exceeds it rather than truncating |
| `vertices` | `vertexCount` × 3 × `f32` | world space |
| `triangleCount` | `u32` | |
| `triangles` | `triangleCount` × 3 × `u16` | |

Mesh vertices are **world space**, like OBB centres: the prop's placement is baked in at build
time, because a static prop's transform never changes and a runtime that had to apply one would be
doing it per sweep.

### 3.4 Cells

| Field | Type | |
|---|---|---|
| `cellCount` | `u32` | |
| then per cell: | | |
| `id` | string | the `layout.cells.json` id |
| `outdoors` | `u8` | 0 or 1: **is §3.5's ground part of this cell's collision?** |
| `bounds` | 6 × `f32` | min *xyz*, max *xyz* over every shape the cell references, **as that cell indexes it** — §4.1's borrowed shapes count only for the part of them within reach |
| `shapeCount` | `u32` | ≤ 65 535 |
| `shapes` | `shapeCount` × `u32` | **global** shape indices: `0 … obbCount−1` are OBBs, `obbCount …` are meshes at `index − obbCount` |
| `nx`, `nz` | `u32`, `u32` | grid dimensions |
| `origin` | 2 × `f32` | the grid's minimum corner, *x* and *z* |
| `buckets` | `nx × nz` of (`u16` count, count × `u16`) | **local** indices into this cell's own `shapes` list |

Bucket entries are local, not global: a cell holds far fewer than 65 535 shapes while the world
holds more, so local indices halve the largest part of the file.

The grid is **x/z only**. A cell is one storey tall, so a third axis would multiply buckets without
dividing shapes — the floor slab alone spans every bucket of every vertical layer — and the
vertical reject is one comparison against the shape's own AABB, which the sweep does anyway.

`outdoors` is a property of the CELL and not of the world (`HOUSE-00774`). §3.5's height field is
one surface over the whole lot and the house stands on it, so the ground runs through the basement
and a tenth of a metre under `L0`'s floor: a body on the basement stair is beside it and must not
be pushed by it, and a body on the lawn must. The flag is what tells §49.3's step 5 and the ground
probe which of the two they are looking at. It is 1 for §15's open exterior cells and for
`EXT_WORLD`, and 0 for every room — including `EXT_SHED`, which is an `exterior` cell that is a
BUILDING (§15.7 rule 5 draws the same line).

The flag stays, and so does the field it guards. `HOUSE-00786` cut the ground the basement stands
in out of the drawn MESH — rendering has no equivalent gate, because a tile is 16 m square and the
one seen through a basement window is the one that continues under the house — but the collider
still reads every sample, because the field is also what says how high the ground is against a wall
and `height_at` has callers that are nowhere near a room.

A shape is listed in **every bucket its AABB overlaps**, not the one its minimum corner falls in.
A shape a cell borrows through a hole (§4.1) is listed by the part of it within reach of that hole
rather than by all of it: the shape is whole — the narrow phase gets its real geometry — and the
clip decides only which buckets have to find it.

### 3.5 The terrain height field

*Version 2 (`HOUSE-00553`). §11.5's ground, and §49.2's "exterior collision uses the terrain height
field plus OBBs".*

| Field | Type | |
|---|---|---|
| `hasTerrain` | `u8` | 0 or 1. **0 ends the file** — nothing below is written |
| `samplesX`, `samplesZ` | `u32`, `u32` | 81 × 65 for this house |
| `originX`, `originZ` | `f32`, `f32` | the sample grid's minimum corner |
| `step` | `f32` | metres between samples, 1.0 |
| `heights` | `samplesX × samplesZ` × `f32` | **metres**, row-major, *z* outer and *x* inner |
| `materialCount` | `u32` | |
| `materials` | `materialCount` × `u16` | indices into §3.2's surface table |
| `materialIndex` | `samplesX × samplesZ` × `u8` | index into `materials` |

**Why the ground is in this file at all.** It is authored as `terrain.png`, a 16-bit greyscale
image, and the runtime has no way to read one: `Texture2D::FromStream` is XNA 4.0 but needs a
`GraphicsDevice` — so it could not run in a headless physics test — and hands back 8-bit colour
regardless. A PNG decoder in `cnahouse::` would be a second implementation of an encoding
`terrain_gen.py` already owns. 5 265 samples cost 26 KB in a 700 KB file, which is the cheapest
of the three answers.

**Heights are metres, not the PNG's `u16`.** The quantisation is the authoring format's business.
`build_collision.py` reads the committed image and decodes it, so the collider and the renderer
agree sample for sample, and the reader needs no copy of `originY` or `yScale` to make sense of a
number.

**Materials resolve through §3.2's surface table**, not a table of their own. A footstep on grass
and a footstep on a tiled floor are the same question, and two tables would be two answers.

`hasTerrain` is 0 for a world with no exterior — the round-trip fixture is one — and a reader must
treat that as "no ground", not as an error.

## 4. How rooms become walls

The layout has no walls in it. `layout.cells.json` gives each room's *interior* boxes; the wall
between two rooms is implied by the two rooms abutting. Four rules make the implication concrete,
and each is a decision:

1. **A wall is centred on the boundary plane.** `world-format.md`'s validator rule 4 requires a
   portal rectangle to lie in *both* cells' boundary planes, so two abutting cells share one plane
   exactly with no gap authored for a wall to sit in. A wall of thickness *t* takes *t*/2 from each
   room. Growing it outward from one room instead would put it inside the neighbour, and which room
   got the intrusion would depend on the order the cells happen to appear in the file.

2. **A side is segmented by what is on the other side of it, and each segment gets its own
   thickness.** `wallGarage` when either cell is a garage, `wallPartition` between two interior
   cells, `wallExterior` facing outdoors, `foundationWall` instead when the level's `ffl` is below
   grade. A side that is partition for four metres and exterior for two is two OBBs, not one
   averaged one.

3. **A wall shared by two boxes of the *same* cell does not exist.** An L-shaped room is authored
   as several boxes and the boundary between them is the middle of the room. This is the failure
   that hides: the room renders correctly, the lightmap bakes, and the player is stopped by nothing
   in the middle of the lounge.

4. **A portal is a hole, and a hole in a rectangle is up to four rectangles.** A doorway reaching
   the floor leaves two jambs and a header — three pieces. A window leaves four, adding a sill.
   Two openings stacked with no gap between them leave three, not four, because the decomposition
   merges vertically; an unmerged seam across a jamb is a seam the lightmap finds.

### 4.1 A hole belongs to both rooms

*`HOUSE-00568`.* The four rules above make a **wall** shared, and that is what makes the per-cell
partition safe: a body swept against one cell's list cannot reach anything behind a wall, because
the wall is in that list and stops it first. At a **hole** it is not safe. §16.4's lookup hands the
body to a cell and hands it over 0.05 m past the boundary at the earliest, so a body standing in a
doorway is 0.35 m into the room it has not been given yet — and whatever stands there was, until
this rule, in the other cell's list alone.

`L0_FOYER`'s cased opening is 0.20 m west of the main stair's first run. `HOUSE-00618`'s
twenty-minute bot walked east through it and was **0.151 m inside the staircase** before anything
stopped it; the shove back out arrived a frame later, when the cell tracker changed its mind. The
same is true of every hole with something behind it.

So, through every hole in a boundary, each side's list gains the other side's shapes that a body
standing in the hole can touch:

* **0.40 m** past the plane — §43.1's 0.30 m capsule radius plus §16.4's 0.05 m hysteresis, and
  50 mm so the rule does not sit exactly on the number it is derived from;
* over the hole's own width, plus that same reach at each jamb;
* over the **body's** vertical band and not the hole's — from the lower of the two floors to
  1.80 m above the higher — when the hole is one a body can walk through (a sill no more than
  §43.1's 0.22 m step-up above the floor). The fridge sub-cell is where the difference showed: its
  ceiling slab starts exactly at the top of its own opening, and a body standing on its +0.70 floor
  has 50 mm of head inside that slab. A hole with a higher sill is a **window**: a body cannot
  stand in one, so only what is level with the hole itself is carried.

A `y` portal is not a hole for this purpose. A hole in a slab is a way *down*, the body that goes
through one is falling, and the cell it lands in answers for it; the flights that reach through a
stair well are in the lists of both cells they connect already, because `build_stairs` puts them
there.

The house's census: **376 shape references**, over 107 holes a body can stand in.
`OpeningReachTests` is the guarantee — within the hysteresis band either side of every such hole,
the deepest overlap does not depend on which of the two lists you ask — and it holds to 0.000000 m
over 5 739 poses.

**And a dormer is a hole in the roof, not a lump on it** (`HOUSE-00490`). §12.1's five dormers sit
in the front and rear walls and rise through the slope; the main plane was left whole underneath
them, so from inside the attic the roof ran across the window, and the dormer's own front gable put
a second board over the top two thirds of it. `L3_ROOM` is a room §13.6 calls *"lit by three
dormers"* and §64.6 measured its sky exposure at **0.000**. Each dormer's footprint is now cut out
of the plane in plan — from its front wall back to where its ridge dies into the slope, and no
further, because the eaves oversail the wall and that strip is in front of the dormer rather than
under it — and its own eight faces stand in the hole: a gable, two jambs and a header round the
open window, two cheeks and two roof planes. The roof's plan area is unchanged by the operation,
which is the check that says the dormer covers exactly what it removed. `roof_geometry.py` owns
both halves, so the shell you see and the shell you collide with are the same shape.

### 4.2 The property outdoors

*`HOUSE-00774`.* §49.2: *"Exterior collision uses the terrain height field plus OBBs for fences,
walls, kerbs, the shed, vehicles and tree trunks."* The height field is §3.5's and the shed is a
cell with walls like any room; this is the rest of that sentence, and until it existed the property
was divided by its **cell boundaries** instead — 86 wall pieces between one open yard and another,
5 219 m² of invisible wall, 29 of them over five metres tall, none of it drawn by anything. You
could not walk from the front lawn to the side yard.

Two open exterior cells abut on grass, so §4's rule 1 does not apply between them and no wall is
built (107 boundaries in this house). **Nor does one get a floor** (`HOUSE-00782`): §3.5's height
field is the ground it stands on, and a slab as well would be a second answer to "how high is the
ground here" -- one that wins wherever the lot has dropped away from the cell's declared floor.
`EXT_ORCHARD` says +0.00 and the lawn under it is 0.30 m lower, which is a plinth with a step round
it that §43.1's 0.22 m step-up cannot climb. A BALCONY keeps its slab: it is an exterior cell whose
floor is 3.65 m over the lawn, and no height field carries that; so does every cell in a world that
has no height field at all, which is every fixture.

**§11.4's street furniture is solid** (`HOUSE-00857`). §49.2's sentence names *vehicles*, and
this file took exactly those from the `neighbourhood` array while `HOUSE-00846` drew nine lamps,
five poles, five signs, twelve mailboxes, two bin clusters and a hoop that nobody could touch —
`NB_POLE_03` stands 0.80 m outside §11.2's pedestrian gate, which is the first thing a player walks
at on leaving the property, and they walked through it. Each solid piece is now an OBB of its own,
surface `furniture`, sized to the SOLID part rather than to the asset's box: a lamp's box reaches
1.76 m out to a lantern 8 m up, and a body colliding with that would stop 1.5 m short of a post it
can see. The selftest checks each proxy fits inside what `neighbourhood_gen.py` draws — containment
and not equality, because collision may be smaller than the thing and never larger.

The catenary spans, the horizon cards and the water tower are named as **deliberately intangible**:
a wire 8.7 m up and a card 360 m away are not things a body walks into, and a proxy for one is a
shape nothing will ever sweep against. Naming them is what makes a NEW asset that is in neither
list a selftest failure rather than a ghost. Fifteen pieces survive `place`'s reach test; the rest
of the street is 100 m down the road, where no open cell can touch them.

It has no **lid** either — but only if it is open. `EXT_SHED` is an `exterior` cell that is a
BUILDING, and a building has a ceiling: §15.7 rule 5 draws that line and `HOUSE-00777` found the
shed roofless in §37.2's coverage field, which is to say it rained in it. The five interior cells
marked `visibilityHint: open` are open to the stair WELL, which is a hole in the slab (§3.4's
`_slab_holes`), not the absence of one.

**And a wall of an open cell stops at the roof of what it is a wall of** (`HOUSE-00784`). An open
cell is a volume of *outdoors*, so its extent is §10.3's ceiling — `EXT_GARDEN` runs to +20.00 and
`EXT_WORLD` to +60.00 — and §4's rule 1 built the shared wall over the CELL's extent rather than
over the neighbour's. That put **1 053 m² of collision into open air in 12 segments**: over the
sunroom's roof (`EXT_TERRACE` and `EXT_BACKYARD`, +3.30 to +20.00), over the garage's
(`EXT_SIDEYARD_E` and `EXT_DRIVEWAY`, +4.30 to +20.00) and over the shed's (`EXT_GARDEN` on all
four sides, +2.35 to +20.00 — a 17.65 m overhang). Nothing drew it and nothing could walk into it,
because a body is 1.8 m tall and every piece began at 2.35 m or higher; what found it was
`HOUSE-00779` casting a ray at the sky from the terrace and hitting a wall standing on the
sunroom's roof.

The top is found across **every level**, not the cell's own: the other side of a balcony's wall at
+6.55 is the storey above, which is a cell on another level, and clipping to the same-level
neighbour alone would take 21.8 m² of real wall off the rear balcony. An **open** occupant does not
count either — the rear balcony stands at +3.65 over the sunroom — so only solid cells raise the
roof line, which is what makes this one rule rather than two. `EXT_SHED` is the case that shows the
rule is about the OPEN cell and not about exterior cells: its own walls are its own height and are
untouched; it is the garden's copy of that wall, running to +20.00, that was wrong.

What is built instead comes from `layout.exterior.json`:

| From | What | This house |
|---|---|---|
| `fences` | one OBB per 2 m of run, `FENCE_THICKNESS` thick, sitting on the ground under it, with each gate's opening **cut out of the run before it is cut into pieces** | 91 |
| `kerbs` | the same, 0.15 m high — a body steps over a kerb, because §43.1's step-up is 0.22 m | 48 |
| `structures` without a cell | one OBB each: §11.1's raised beds, its trellis and the compost bin | 8 |
| `vegetation` whose asset is a tree | one OBB per instance, sized as a **trunk** — a body walks under a maple, not into it | 17 |
| `neighbourhood` whose asset is a parked car | one OBB per car, carrying the row's yaw | 2 |

A gate remains a hole in its fence run, then `HOUSE-03223` adds one separately named OBB at the
authored fixed pose. Doors are static for the walkthrough too: hinged and sliding leaves are OBBs,
while the pitched overhead garage leaf is a triangle-mesh box because the format's OBB rotation is
yaw-only. Each proxy's surface name is `door_leaf:<opening>:<leaf>` or
`gate_leaf:<gate>:<leaf>` so the production test can account for every leaf across the binary
writer/reader boundary. No runtime door interaction or per-frame obstacle update is involved.

Each piece is put in **every** open cell within `OPENING_REACH` of it, for §4.1's reason one step
further out: two yards abut with no wall AND no portal, so §16.4 hands a body from one to the other
in the middle of an open lawn and nothing else would share the fence post on the boundary.

Nothing is built where no open cell can reach it. The kerbs run 440 m down the road and 34 street
trees stand along it; the property's own cells cover 45 m of that, and a kerb 180 m away is scenery
that §10.3's playable volume puts out of reach for ever.

## 5. Proxies: most of them are boxes

`collision_proxy.py` writes three shapes: `box` (12 triangles), `hull` (a 14-DOP) and `boxes` (up
to five boxes joined into one mesh). Two of the three are boxes as far as collision is concerned,
and a swept capsule against an OBB is a handful of operations against a mesh's per-triangle loop.

So a `_COL` mesh is split into connected components **by welded vertex position, not by index** —
the exporter splits vertices per face, so index adjacency finds nothing — and each component that
is an axis-aligned box in the asset's own space becomes an OBB carrying the prop's yaw. Only what
is left becomes a triangle mesh.

"Is a box" is 8 distinct positions, 12 triangles, **and** every one of those positions a corner of
the component's own bounds. The third clause is the one that matters: 8 corners and 12 triangles
also describes a box with a dent in it, and the dent is exactly where a player would walk in.

## 6. What this file does not contain

* **Dynamic obstacles** — doors, the garage door, pets, nudgeable props. §49.4 builds those per
  frame from the animated transform; a door's OBB is not static and does not belong in a static
  file.
* **The terrain height field** — `layout.exterior.json` names `world/terrain.r16` and the runtime
  reads it directly. Duplicating it here would be a second copy to keep in step.
* **Nav, coverage, sky exposure and chunk data** — `HOUSE-00211`…`HOUSE-00215` write their own
  files. One tool, one output.
