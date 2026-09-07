# `nav.bin` — the pet waypoint graph

*`HOUSE-00211`. Normative. The writer is `tools/world/build_nav.py`; there is no other. The runtime
reader is `HOUSE-02046`'s `NavGraph` and does not exist yet — the writer's own `read_back` is
the round trip that keeps the format honest until it does.*

---

## 1. What this file is for

ADR-0011 rejects Recast/Detour: "a 300-node waypoint graph with A\* is ~150 lines". `cna-house.md`
§60.3 specifies the graph — nodes at every doorway centre, every room centre, and 2–6 more per room
chosen to avoid furniture; edges between mutually visible nodes within a cell and across portals —
and §61 adds the cat's 22 perches. This file is that graph, resolved at build time so the runtime
loads it rather than deriving it.

It contains **no adjacency list**. Adjacency is derivable from the edges, and a second description
of the same relationship is a second thing that can disagree with the first (`anim-format.md` §2).
The loader builds it once.

## 2. Conventions

`docs/anim-format.md` §2's conventions apply unchanged: little-endian, IEEE-754 binary32, explicit
fixed-width integers, `u16`-length UTF-8 strings, metres, right-handed Y-up with −Z north, no
alignment, and no seeking — the file is read forward, once.

## 3. Layout

### 3.1 Header

| Field | Type | Value |
|---|---|---|
| `magic` | 4 bytes | ASCII `CNAV` |
| `version` | `u32` | **1** |
| `flags` | `u32` | 0. A reader must **reject** any unknown bit rather than ignore it |
| `worldHash` | string | `world.manifest.json`'s `worldHash` |

### 3.2 Species table

| Field | Type | |
|---|---|---|
| `speciesCount` | `u32` | 2 — `dog`, `cat`, in bit order |
| then per species: `name` | string | |
| `radius`, `height` | 2 × `f32` | the capsule the graph was generated for |

The capsule travels with the graph because the graph is only valid for it. A runtime that used a
different radius than the one the edges were tested against would be planning routes for an animal
that does not exist.

The dog's capsule is §60.3's, verbatim: **r 0.22, h 0.60**. The cat has no capsule anywhere in the
architecture, only a size — 0.25 m at the shoulder, 0.46 m body (§61) — so **r 0.11, h 0.25** is
derived from that, once, here, rather than guessed separately by every subsystem that needs it.

Masks are `u8`, so a third animal costs no version bump.

### 3.3 Cell table

| Field | Type | |
|---|---|---|
| `cellCount` | `u32` | |
| `cells` | `cellCount` × string | `layout.cells.json` ids, sorted |

### 3.4 Nodes

| Field | Type | |
|---|---|---|
| `nodeCount` | `u32` | |
| then per node: `id` | string | |
| `cell` | `u16` | index into §3.3 |
| `position` | 3 × `f32` | world space, at the animal's **feet** |
| `kind` | `u8` | 0 room · 1 doorway · 2 open · 3 perch · 4 bed · 5 bowl |
| `species` | `u8` | bit per species — who may stand here |

Edges reference nodes by their **index in this list**, so the order is part of the format.

### 3.5 Edges

| Field | Type | |
|---|---|---|
| `edgeCount` | `u32` | |
| then per edge: `a`, `b` | 2 × `u32` | node indices |
| `cost` | `f32` | metres |
| `species` | `u8` | who may traverse it |
| `portal` | string | the portal id it crosses, or empty for an edge inside one cell |

An edge that names a portal is closed when that portal's aperture is closed — §60.3's "a dog cannot
path through a closed door; it goes to the door and waits, which is exactly right". An edge with no
portal is always open.

## 4. How the graph is generated

### 4.1 The edge test is a clearance test, not a ray

This is the decision the tool turns on. A ray between two nodes passes through a 10 cm gap between
the sofa and the wall, and the graph then holds an edge the dog physically cannot take: §60.3 says
it "is a capsule (r 0.22, h 0.60) and is genuinely collided", so the steering jams while the
planner insists the route is fine. What is tested is whether the animal's radius **fits** the whole
way — the distance from its capsule axis to the nearest blocking shape must exceed its radius, at
samples 0.05 m apart along the walk.

Two species, two radii, two graphs over one node set. Gaps the dog cannot use, the cat can.

### 4.2 The query is a capsule axis, never a point

A capsule of radius *r* and height *h* is the points within *r* of a segment from *r* to *h* − *r*;
the caps are the rest. Three consequences, each of which was a bug before it was a rule:

* Testing a **point** at some chosen height has no right answer. At the animal's feet it misses a
  wall shelf; at its chest it sails over the 0.25 m bench a dog plainly cannot cross.
* Testing the segment **0 … h** and comparing to *r* inflates the animal by its own radius at each
  end — it would be refused a doorway it walks under and stopped by a rug.
* Floors and ceilings are **excluded** from the blocking set. A floor is not an obstacle to
  something standing on it; counting it gives every node in the house zero clearance.

Against an OBB the answer is exact and closed-form rather than sampled: `build_collision`'s OBBs are
yawed about Y only, so undoing the yaw leaves the query segment vertical and the distance separates
into a 2-D clamp in x/z and a 1-D interval gap in y. Triangle meshes, the small minority (§49.2),
are sampled up the axis.

### 4.3 Not every portal is a route

A portal is a route only if its rectangle's bottom edge is at the floor, it is taller than the
animal, and it is wider than twice the animal's radius. **Geometry, not `kind`**: the same three
numbers decide it for a cased opening, a door, a slider and a garage door, and a list of kinds is a
list someone has to remember to extend.

A window fails the first clause, and getting this wrong is not a missing edge but a broken graph: a
node placed at floor level under a 0.9 m sill is *inside* the sill, which is a wall piece, so it
fails its own clearance test and takes the two cells' subgraphs apart. The tool reports every
portal it rejected, by name.

### 4.4 Generated and authored

Generated, because 300 nodes is not a thing to type: the room-centre node — the *clearance-tested
candidate nearest* the cell's centroid, since an L-shaped room's centroid can be outside the room
and a centroid inside the sofa is not somewhere an animal can stand — the doorway node at each
passable portal, and up to six more per cell by **farthest-point sampling**, which maximises the
minimum separation where taking the first few of a scan leaves neighbours one grid step apart.

Authored, because no rule produces them: the 22 perches, the beds and bowls, the forbidden zones,
and any hand-placed node. Authored rows are read first and never overwritten; a generated node
landing within 0.60 m of one is dropped.

A forbidden zone removes one species from every node in a cell — a cat that may not enter the
garage must not have a node there for A\* to aim at.
