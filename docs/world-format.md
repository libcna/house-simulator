# The world format

The house is data, not code ([ADR-0005](decisions/ADR-0005-data-driven-world.md)). Seventeen JSON
files under `assets-src/world/` are the single source of truth for geometry, collision, portals,
lighting zones, audio zones, interaction placement and asset residency. This document is the
reference; `cna-house.md` §15 is the authority behind it.

Each file also has a machine-checkable [JSON Schema](world-schema/) — see
[The machine-checkable half](#the-machine-checkable-half) below for what it does and does not
decide.

The authored files are JSONC (comments permitted); the build strips comments and deploys plain
JSON beside the compiled content, where the runtime reads them with `System::IO::File` +
`System::Text::Json`. That stripping step is
[`tools/world/deploy_world.py`](../tools/world/deploy_world.py): `assets-src/world/*.json` →
`content/world/*.json`, a `build_content.py` stage and a CI gate (`HOUSE-00421`). The runtime never
reads `assets-src/`, and `System::Text::Json` refuses the first comment, so a change that is not
deployed is a change the game does not have. They are data the *game* owns, not framework content, so no
`ContentTypeReader` is registered and no CNA pipeline extension is needed.

---

## Conventions that apply to every file

| Rule | Detail |
|---|---|
| Header | Every file starts with `"schema": "cna-house/<kind>/<version>"`. The version is an integer, incremented on any incompatible change. |
| Keys | `camelCase`. |
| Ids | `^[A-Z][A-Z0-9_]*$`, with a per-kind shape — see [`conventions.md`](conventions.md) §1. Globally unique across every kind. |
| Units | Metres, seconds, kelvin (`colorK`), lumens (`intensityLm`). Degrees in data, radians in code; a key carrying degrees says so (`yawDeg`). |
| Axes | Right-handed, **Y up**, **−Z is north**, +Z is the road (`cna-house.md` §14). Vectors are `[x, y, z]`. |
| Ranges | `{"x": [min, max], "z": [min, max]}`, min < max. |
| `null` | "Not specified, use the documented default". **Never** a synonym for zero. |
| Booleans | Only for genuinely two-state things. Continuous quantities are floats — a boolean weather or aperture field fails the lint. |

---

## `world.manifest.json`

The index. It is what `worldHash` in a save is taken over, and what tells the loader that world
data has changed since the save was written.

```jsonc
{
  "schema": "cna-house/manifest/1",
  "worldHash": "sha256:0e2f…",          // over the member list, in the order given
  "members": [
    { "file": "layout.levels.json",  "sha256": "sha256:a91c…" },
    { "file": "layout.cells.json",   "sha256": "sha256:77bd…" }
  ]
}
```

Rule: a member listed here that does not exist, or whose hash does not match, is a **load-time
error**. A file present in the directory but absent from the manifest is also an error — silent
extra data is how two sources of truth begin.

The hashes are defined exactly, because two implementations have to agree —
[`world_manifest.py`](../tools/world/world_manifest.py) writes them and
[`WorldLoader`](../include/cnahouse/world/WorldLoader.hpp) verifies them:

* a **member** hash is `sha256:` plus the lower-case hex SHA-256 of the file's **bytes**. Not of
  its parsed JSON: a reformatted file is a different file, the deployed copy is what the loader
  reads, and "the bytes on disk" is the only definition both sides can implement without first
  agreeing on a JSON canonicalisation;
* **`worldHash`** is `sha256:` plus the SHA-256 of `<file>\n<sha256>\n` for each member, in the
  order the manifest lists them, encoded UTF-8. Over the list rather than over the contents,
  because it must change whenever any member changes and that costs one hash of a few hundred
  bytes instead of a rehash of the world.

Members are written in this page's order — never the filesystem's — because the order is part of
the hash, and a list that reshuffled between two machines would make a save written on one look
stale on the other.

The manifest is **generated, not authored**, and it describes the **deployed** copy in
`content/world/` rather than the authored JSONC in `assets-src/world/`. `WorldLoader` rehashes the
files it is about to read, and those are the deployed ones; a manifest over the authored bytes
would fail at load on every file that had a comment in it, which is every file.
[`deploy_world.py`](../tools/world/deploy_world.py) writes it as its last step (`HOUSE-00421`), and
`--check` is a CI gate.

## `layout.levels.json`

The five levels and the construction constants everything else derives from.

```jsonc
{
  "schema": "cna-house/levels/1",
  "units": "metres",
  "north": "-Z",
  "levels": [
    { "id": "B1", "name": "Basement",      "ffl": -2.30, "ceiling":  0.25, "structureDepth": 0.35 },
    { "id": "L0", "name": "Main Floor",    "ffl":  0.60, "ceiling":  3.30, "structureDepth": 0.35 },
    { "id": "L1", "name": "Upper Floor 1", "ffl":  3.65, "ceiling":  6.20, "structureDepth": 0.35 },
    { "id": "L2", "name": "Upper Floor 2", "ffl":  6.55, "ceiling":  9.00, "structureDepth": 0.35 },
    { "id": "L3", "name": "Attic",         "ffl":  9.30, "ceiling": null,  "roof": "ROOF_MAIN" }
  ],
  "construction": {
    "wallExterior": 0.30, "wallPartition": 0.15, "wallPlumbing": 0.20, "wallGarage": 0.25,
    "foundationWall": 0.30, "kneeWallHeight": 1.20, "ridgeY": 14.30, "roofPitch": 0.594,
    "skirting": 0.14, "cornice": 0.11, "balustrade": 0.95, "railing": 1.10
  }
}
```

The optional `hvac` block carries §12.5's HVAC row: the plant, the vertical chase beside the
stair, and the branches with their registers.

```jsonc
"hvac": {
  "plant": { "cell": "B1_MECHANICAL", "position": [6.80, -1.40, -16.30],
             "chase": { "x": [2.20, 2.60], "z": [-20.20, -19.40] } },
  "branches": [
    { "id": "DUCT_L0_WEST", "trunk": "joist space below",
      "cells": ["L0_LIVING", "L0_DINING"],
      "registers": [ { "cell": "L0_LIVING", "position": [-4.60, 0.62, -17.85],
                       "kind": "floor" } ] }
  ]
}
```

A register is in the **floor** upstairs, where the branch runs in the joist space below, and in the
**ceiling** in the basement, where the trunk is exposed under the `L0` slab — which is also why
§62.6's duct rumble is loudest in `B1`. The registers live here rather than as audio emitters
because §62.6 places the rumble "at each register", and an emitter per register would be the same
fact written twice.

Branch membership is not repeated: a cell names its branch in `thermal.ductBranch` and the branch
lists its cells, and rule 6 checks the two against each other — a branch that has lost a room is a
room the furnace is silent in, and neither half can see that alone (`HOUSE-00387`).

The optional `plumbing` block carries §12.5's stacks, because they are house-wide structural
constants like `construction` and because validator rule 9 needs them:

```jsonc
"plumbing": {
  "stacks": [
    { "id": "STACK_A", "cells": ["L0_WC1", "L1_WC4", "L2_WC6"],
      "chase": { "x": [2.20, 4.90], "z": [-22.00, -20.20] },
      "dropTo": "B1_UTILITY" }
  ]
}
```

A prop that is a plumbing fixture names the stack it drains to in its own `plumbing` field; every
other prop leaves it `null`. Rule 9 checks both directions — that each fixture is in a cell its
stack lists, and that each stack's cells exist, are listed once each, overlap the chase and land on
a cell.

`chase` is the footprint the stack rises within — §12.5's "vertical alignment" — and not the pipe:
rule 9 asks whether a cell is above the drop, and a box the width of a 100 mm pipe answers a
question nobody is asking. Several cells may sit on **one** level: a kitchen sink and a wet bar on
the same floor branch into the same stack, which is what §12.5's STACK-E is, and requiring one cell
per level was a rule that §12.5's own data broke (`HOUSE-00386`). `dropTo` may be `null` for a
stack already at the bottom — STACK-F drains into an ejector pit, which is machinery and not a
cell.

`ffl` is finished floor level; `ceiling` is the underside of the ceiling above it. `L3`'s
`ceiling` is `null` because the attic is bounded by rafters, named by `roof`, not by a plane.

## `layout.cells.json`

A cell is the unit of visibility, audio, lighting and residency. One row per room, corridor,
stair, closet, garage bay and exterior region.

```jsonc
{
  "schema": "cna-house/cells/1",
  "cells": [
    {
      "id": "L0_KITCHEN",
      "level": "L0",
      "name": "Kitchen",
      "kind": "room",                    // room | corridor | stair | closet | garage | exterior | void
      "boxes": [ { "x": [-8.20, 2.20], "z": [-27.10, -23.00] } ],
      "yOverride": null,                 // null = the level's ffl..ceiling
      "floorMaterial":   "MAT_TILE_PORCELAIN_GREY",
      "wallMaterial":    "MAT_PAINT_WARM_WHITE",
      "ceilingMaterial": "MAT_PAINT_FLAT_WHITE",
      "footstepSurface": "tile",
      "acoustic": { "roomTone": "AMB_KITCHEN", "absorption": 0.28, "reverbHint": "small_hard" },
      "thermal":  { "heated": true, "ductBranch": "DUCT_L0_W" },
      "lightGroups": ["LG_L0_KITCHEN_MAIN", "LG_L0_KITCHEN_UNDERCAB"],
      "daylight": { "windowIds": ["W_L0_KITCHEN_N1"], "orientation": "N", "exposure": 0.55 },
      "residencyPack": "house-l0",
      "lodBias": 0,
      "visibilityHint": "opaque",        // opaque | open  (open = no walls, e.g. exterior)
      "navMeshRegion": "NAV_L0_KITCHEN"
    }
  ]
}
```

An L-shaped room is **several boxes**, not one bounding box: the boxes are what the portal
traversal and the cell-membership lookup use, so an over-large box would put the player in the
wrong room. `footstepSurface` and `absorption` live here rather than on the material because they
are properties of the *room* as experienced, and a room with three floor materials still has one
dominant footstep sound.

### Container sub-cells

`cna-house.md` §54: "a container is a tiny sub-cell with its own portal, so this falls out of the
visibility system rather than being a special case". A sub-cell is an ordinary cell with a
`parent`:

```jsonc
{
  "id": "CELL_FRIDGE_INTERIOR", "level": "L0", "kind": "closet", "parent": "L0_KITCHEN",
  "boxes": [{ "x": [0.40, 2.00], "z": [-26.95, -26.45] }],
  "yOverride": [0.70, 2.45]
}
```

The parent is **declared, not inferred**, because a room accidentally drawn inside another looks
identical to a validator otherwise — and declaring it does not switch rule 3 off: a sub-cell is
then checked to lie inside its parent in all three axes, and to be only one level deep. Without
the field, each of §54's 214 containers would have to be carved out of the room around it, which
would make `L0_KITCHEN` a sixty-three-box polygon whose area changed every time a drawer moved.

## `layout.portals.json`

A portal is always an **axis-aligned rectangle on an axis-aligned plane**. That restriction makes
the visibility clip exact and cheap, and every door and window in this house is in a rectilinear
wall.

```jsonc
{
  "schema": "cna-house/portals/1",
  "portals": [
    {
      "id": "P_L0_HALL__L0_KITCHEN",
      "cellA": "L0_HALL", "cellB": "L0_KITCHEN",
      "plane": { "axis": "z", "value": -23.00 },
      "rect":  { "u": [-1.00, 1.00], "v": [0.60, 2.80] },  // u = the other horizontal axis, v = world Y
      "kind": "cased_opening",   // cased_opening | door | double_door | slider | window
                                 // | garage_door | stair_well | exterior_door | hatch
      "aperture": null,          // null = always fully open; otherwise a door/window entity id
      "opacity": "open",         // open | opaque_when_closed | translucent | glass
      "maxDepth": null,          // optional per-portal traversal cap
      "soundLoss": { "open": 0.0, "closed": 0.0 }
    }
  ]
}
```

`plane.axis` is `x` or `z` for a wall, and `y` for a **horizontal** opening. A `stair_well` and a
`hatch` are both in the vocabulary above and neither is a hole in a wall, so the third case is
needed: on a `y` plane `u` is world X and `v` is world Z, the rectangle must lie inside *both*
cells' footprints, and the plane must be the boundary the two cells share — one's ceiling is the
other's floor. A cell a stair climbs through therefore carries a `yOverride` that reaches the
landing above, which is why `stair` and `void` cells are exempt from the slab-underside bound of
validator rule 2.

`opacity` semantics:

| Value | Vision when closed | Player passage when closed | Sound when closed |
|---|---|---|---|
| `open` | passes | passes | passes |
| `opaque_when_closed` | **blocked** | blocked | attenuated by `soundLoss.closed` |
| `glass` | passes, subject to `maxDepth` | blocked | attenuated |
| `translucent` | passes, but the reduced frustum is marked *diffuse*: the target cell renders at LOD+1 with no small props | blocked | attenuated |

`soundLoss` is in normalised loss, 0 = transparent, 1 = inaudible; the audio system maps it to the
decibel figures of `cna-house.md` §64.3. The validator requires the rectangle to lie in **both**
cells' boundary planes within 1 cm and its `v` range to lie inside both cells' vertical extent.

## `layout.openings.json`

Doors and windows as geometry plus entity: leaf size, hinge side, swing direction, frame. Each row
pairs with exactly one portal in `layout.portals.json` (validator rule 7), and with one
interactable.

```jsonc
{
  "schema": "cna-house/openings/1",
  "openings": [
    {
      "id": "DOOR_L0_WC1", "kind": "door",
      "portal": "P_L0_HALL__L0_WC1",
      "leaf": { "width": 0.86, "height": 2.04, "thickness": 0.040 },
      "hinge": "left", "swing": "into_L0_WC1", "maxAngleDeg": 95.0,
      "frame": { "asset": "MODEL_DOOR_FRAME_INT_01", "casing": 0.070 },
      "asset": "MODEL_DOOR_LEAF_PANEL_01",
      "material": "MAT_PAINT_TRIM_WHITE",
      "solid": false,                      // hollow-core: 16 dB closed, vs 24 dB for solid
      "lockable": false
    }
  ]
}
```

## `layout.stairs.json`

```jsonc
{
  "schema": "cna-house/stairs/1",
  "flights": [
    {
      "id": "STAIR_L0_L1_MAIN",
      "fromCell": "L0_FOYER", "toCell": "L1_LANDING",
      "risers": 17, "rise": 0.179, "going": 0.280, "width": 1.20,
      "landings": [ { "at": 9, "depth": 1.20 } ],
      "collisionRamp": true,
      "surface": "wood"
    },
    {
      "id": "STEPS_PORCH",
      "fromCell": "EXT_WALK", "toCell": "L0_PORCH",
      "risers": 3, "rise": 0.190, "going": 0.300, "width": 3.00,
      "fromY": 0.00, "toY": 0.57,        // required: one level has no level difference
      "landings": [], "collisionRamp": true, "surface": "bluestone"
    }
  ]
}
```

The validator checks `risers × rise` equals the level difference to within 1 mm, and that
`2·rise + going` lands in the comfortable range — the realism rule that catches a stair nobody
could climb. Exterior steps may exceed the upper bound and the porch does, at 680 mm: shallower
and deeper is the right thing outdoors. They may not go under it — a steep step is a steep step in
the rain as much as on the landing.

`rise` is a magnitude and direction lives in `fromCell`/`toCell`, so §12.4's "L0 +0.60 → B1 −2.30"
is the same flight whichever end it is authored from. `fromY` and `toY` are for the flights that
join two cells on one level; a flight like that with neither is reported, because the alternative
is a rule that quietly says nothing about half the stairs in the house.

## `layout.lights.json`

```jsonc
{
  "schema": "cna-house/lights/1",
  "lights": [
    {
      "id": "LIGHT_L0_KITCHEN_ISLAND_1",
      "cell": "L0_KITCHEN", "group": "LG_L0_KITCHEN_ISLAND",
      "type": "point",                     // point | spot | directional | area_proxy | emissive_only
      "position": [-3.00, 2.85, -25.10], "direction": [0, -1, 0],
      "colorK": 2700,                      // kelvin, converted through a Planckian LUT
      "intensityLm": 800, "range": 6.0,
      "coneInnerDeg": 30, "coneOuterDeg": 55,
      "fixtureProp": "PROP_L0_KITCHEN_PENDANT_1",
      "emissiveMaterialSlot": "shade",
      "castsBlobShadow": true,
      "bakedIntoLightmap": true,
      "defaultOn": false
    }
  ]
}
```

A light belongs to exactly one **group**, and a group is what a switch toggles and what a lightmap
is baked per. `bakedIntoLightmap` and `castsBlobShadow` are independent: a baked light still needs
a blob shadow for the dynamic objects the bake never saw.

A cell's `lightGroups` is the **index** of the groups its own lights belong to, and §28.1 walks it
once per frame. Rule 6 checks it both ways: a group in the list with no light of that group in the
cell, and a light in the cell whose group the list omits, are both errors. An index that has
drifted is worse than none — one way round it is a switch the room does not respond to, the other
a lightmap pass over a group with nothing in the room to light (`HOUSE-00381`).

## `layout.props.json`

Static and dynamic placements. A prop that never moves is batched offline into its cell's chunks
and has no runtime object; one that moves becomes a `DynamicInstance`.

```jsonc
{
  "schema": "cna-house/props/1",
  "props": [
    {
      "id": "PROP_L0_KITCHEN_FRIDGE",
      "asset": "MODEL_PROP_KITCHEN_FRIDGE_01",
      "cell": "L0_KITCHEN",
      "position": [1.20, 0.60, -26.70], "yawDeg": 180.0, "scale": 1.0,
      "static": false,                    // false -> a dynamic instance, excluded from batching
      "lodGroup": "LODG_APPLIANCE",
      "collision": "proxy",               // proxy | none | box
      "material": null,                   // null = the asset's own materials
      "interactable": "FRIDGE_L0_KITCHEN"
    }
  ]
}
```

## `layout.materials.json`

The application-owned material. **`cna-house.md` §22.1 is the authority for this record and §22.2
for the class vocabulary**; §15.1 defines this file as "material definitions (§22)", so where this
page and §22 differ, §22 wins. It carries the non-visual properties the audio, physics and weather
systems read as well as the visual ones.

```jsonc
{
  "schema": "cna-house/materials/1",
  "materials": [
    {
      "id": "MAT_TILE_PORCELAIN_GREY",
      "class": "tile",                   // §22.2's vocabulary: paint · wood · carpet · tile ·
                                         // stone · concrete · metal · plastic · glass · fabric ·
                                         // skin · hair · fur · foliage · asphalt · gravel ·
                                         // grass · soil · water · emissive, and the derived
                                         // `wet_<class>` / `snow_<class>` forms
      "albedo": "Textures/Architecture/tile-porcelain-grey",
      "normal": null,                    // Tier E only
      "lightmapChannel": 1,
      "tint": [1.00, 1.00, 1.00],
      "specularPower": 48.0, "specularColor": [0.30, 0.30, 0.30],
      "alphaMode": "opaque",             // opaque | mask | blend
      "alpha": 1.0,                       // stock-effect opacity, 0..1
      "alphaCutoff": null,
      "twoSided": false,
      "uvScale": [4.0, 4.0],
      "wetResponse":  { "albedoDarken": 0.22, "specularBoost": 2.1, "powerBoost": 2.5 },
      "snowResponse": { "coverable": true, "slopeLimitDeg": 40 },
      "footstepSurface": "tile",
      "audioAbsorption": 0.06,
      "effectTierS": "DualTexture",      // Basic | DualTexture | AlphaTest | Skinned
      "effectTierE": "RoomLit"
    }
  ]
}
```

`effectTierS` is what the content build reads to choose a chunk's vertex layout
([`chunk-format.md`](chunk-format.md) §3), because it is stated rather than inferred. §22.2's
class table is the documented fallback when a material omits it — `wood` is `DualTextureEffect`
static and `BasicEffect` dynamic, and only static props are batched, so the static column is the
one the batcher uses.

`snowResponse.slopeLimitDeg` is read by [`snowshell-format.md`](snowshell-format.md): a face
steeper than this never joins the snow shell, which is what stops snow clinging to walls (§38).

## `layout.nav.json`

The pet navigation graph: a waypoint graph, not a navmesh ([ADR-0011](decisions/ADR-0011-no-runtime-dependencies.md)).

```jsonc
{
  "schema": "cna-house/nav/1",
  "nodes": [ { "id": "NAV_L0_KITCHEN_C", "cell": "L0_KITCHEN", "position": [-3.0, 0.60, -25.0],
               "kind": "floor" } ],
  "edges": [ { "a": "NAV_L0_KITCHEN_C", "b": "NAV_L0_HALL_S", "portal": "P_L0_HALL__L0_KITCHEN",
               "cost": 4.2, "species": ["dog", "cat"] } ],
  "perches":  [ { "id": "PERCH_L1_WINDOWSEAT", "cell": "L1_LANDING",
                  "position": [0.4, 4.10, -15.1], "species": ["cat"] } ],
  "beds":     [ { "id": "BED_DOG_FAMILY", "prop": "PROP_DOG_BED", "species": ["dog"] } ],
  "forbidden": [ { "cell": "L0_GARAGE", "species": ["cat"] } ]
}
```

An edge crossing a portal names it, so a closed door closes the route for the pets exactly as it
does for vision and sound. One authored graph, four consumers.

Rule 6 checks that an edge's `a` and `b` are nodes, that its `portal` exists, and that the portal
it names joins **the two cells the edge joins** — a route through a door goes through that door.
Rule 5 checks that the whole graph is one component. Windows, hatches and the sectional door carry
no edge at all: a person can climb through a hatch and a pet cannot.

## `layout.audio.json`

```jsonc
{
  "schema": "cna-house/audio/1",
  "zones":    [ { "id": "AZ_L0_KITCHEN", "cell": "L0_KITCHEN", "bed": "AMB_KITCHEN",
                  "gain": 0.55 } ],
  "emitters": [ { "id": "EM_FRIDGE_HUM", "cell": "L0_KITCHEN", "position": [1.20, 0.90, -26.70],
                  "loop": "AMB_FRIDGE_HUM", "gain": 0.35, "radius": 4.0,
                  "interactable": "FRIDGE_L0_KITCHEN" } ],
  "transmission": { "door_hollow": { "open": 0.05, "closed": 0.55 },
                    "door_solid":  { "open": 0.05, "closed": 0.78 } }
}
```

Every emitter carries a cell id, because the portal-path solver
([ADR-0010](decisions/ADR-0010-room-aware-audio.md)) starts from cells, not from positions.

`transmission` is §64.3's table in the form a portal carries: `loss = 1 − 10^(−dB/20)`, so a 16 dB
door loses 0.842 of the amplitude. It is the **source**; a portal's `soundLoss` is a cache of the
row its leaf's type selects, and rule 6 checks each portal against it. Both exist because the
runtime wants the number per portal and the design states it per class, and two copies of one fact
drift otherwise (`HOUSE-00388`).

## `layout.exterior.json`

Terrain reference, road, fences, gates, driveway, neighbourhood instances and vegetation
instances. Neighbourhood buildings are placed as LOD groups with an impostor distance; vegetation
is placed as instance arrays so it can be drawn instanced where that measures faster.

```jsonc
{
  "schema": "cna-house/exterior/1",
  "terrain": { "heightfield": "world/terrain.png", "materialIndex": "world/terrain_materials.png",
               "size": [45.0, 48.0], "samples": [46, 49], "step": 1.0,
               "origin": [-22.5, 0.0, -24.0], "yScale": 6.0, "material": "MAT_GROUND_LAWN",
               "materials": ["grass", "lawn_worn", "soil", "gravel", "concrete", "asphalt",
                             "bluestone", "mulch"] },
  "road":    { "centreline": [[-40.0, 0.0, 22.0], [40.0, 0.0, 22.0]], "width": 7.0,
               "material": "MAT_ASPHALT_01" },
  "fences":  [ { "id": "FENCE_N", "asset": "MODEL_FENCE_PICKET_01", "path": [[-22.5,0.0,-24.0],[22.5,0.0,-24.0]],
                 "height": 1.80, "gate": "GATE_N1" } ],
  "neighbourhood": [ { "id": "NB_HOUSE_01", "asset": "MODEL_NB_HOUSE_01", "position": [-38.0, 0.0, 30.0],
                       "yawDeg": 180.0, "lodGroup": "LODG_NB_HOUSE", "impostorFrom": 70.0 } ],
  "vegetation":    [ { "id": "VEG_OAK_01", "asset": "MODEL_TREE_OAK_01",
                       "instances": [ { "position": [-14.0, 0.0, -6.0], "yawDeg": 31.0, "scale": 1.12 } ] } ]
}
```

`HOUSE-00390` added four arrays the file needed and did not have: `gates` (§11.2's three),
`kerbs` (§11.4's two), `paths` (the walk, the driveway and its apron, the two sidewalks, the verge
and the garden path) and `structures` (the shed). Rule 6 checks that a gate hangs in a declared
fence and that a structure's `cell` exists; rule 10 checks that an enterable structure's footprint
**contains** that cell's, because a shell that does not hold its own interior is a building drawn
beside its inside.

A `neighbourhood` row's `asset` carries its **palette** since `HOUSE-00843`
([ADR-0013](decisions/ADR-0013-neighbourhood-asset-variants.md)): `MODEL_NB_HOUSE_A_CREAM` is
§11.4 shape A in the cream palette at full detail and `MODEL_NB_HOUSE_C_RENDER_LOW` is shape C in
render at LOD1. `neighbourhood_gen.py` parses that name — an unknown shape or palette is refused,
never painted with a default — and builds only the combinations the rows actually name. There is no
per-instance material override in this file, and adding one is the alternative that record rejects.

The same array carries §11.4's **street furniture**, and `HOUSE-00846` is what finally put geometry
under all of it. Two of its rows are worth reading twice. `MODEL_UTILITY_POLE_LAMP` is a pole that
also carries a street lantern: all five of §11.4's poles stand at an X that also wants one of its
nine street lights, and two posts 0.70 m apart read as one doubled pole, so the pole carries the
lamp the way a suburban street does. `MODEL_UTILITY_SPAN` is the **catenary between two poles** —
a run-spanning asset like a fence rather than a per-instance one, anchored at its west pole and
running 50 m to the next, so four spans reach from x -100 to x +100 with no wire ending in mid-air.

The nine lanterns are the geometry under `layout.lights.json`'s `LIGHT_EXT_STREET_1`…`_9`, and
`neighbourhood_gen.py --selftest` asserts that every one of them is **at** its light rather than
near it: the lamp columns stand on the pole line at Z +0.80 and reach the lights' Z +1.50 on a
0.70 m outreach arm. A light and its lamp 1.6 m apart is the kind of thing only a screenshot ever
finds.

`HOUSE-00848` adds §11.4's **far background** to the same array: `MODEL_HORIZON_RIDGE`, `_SPUR` and
`_WOODS` on a ring of their own at 360 m — outside `HOUSE-00391`'s 114–266 m impostors and inside
§10.3's 420 m far plane — plus one `MODEL_WATER_TOWER`. Twenty-four cards of 15° each, and two
numbers make the skyline continuous rather than scalloped: a card is 96.00 m wide against a chord
of 93.98 m, so neighbours overlap, and every kind's profile starts and ends at +6.00, so one card
meets the next without a step. The kinds are three landscapes and not one shape drawn three times —
the ridge carries §11.4's tree line on its skyline, the spur is bare, the wood is trees on flat
ground — and they are walked round the ring by `(i + i/4) mod 3`, which puts eight of each on it
with no two of the same land side by side, the wrap from the last card to the first included.

`HOUSE-00776` added a fifth: `downspouts`, one row per pipe with `position` (the head, at the
gutter) and `splash` (where the water lands, on §11.5's height field under it). §37.3's splash
particles and §37.4's `Apply3D` trickle emitter both need that point and neither can read a Blender
mesh, which is where the pipes had lived since `HOUSE-00468`. The rows are authored and rule 13
checks every one of them against `roof_geometry.house_downspouts` — the same function the shell
draws from — so the writing and the drawing cannot drift. There are **six**, not eight: the two
corners where the garage wing meets the house are each inside the other roof's footprint.

A `neighbourhood` row's `asset` resolves to a mesh in `content/world/neighbourhood.bin`
(`HOUSE-00856`, [`docs/neighbourhood-format.md`](neighbourhood-format.md)), which holds one mesh
per asset **in its own space** — the row supplies the transform. It is not `chunks.bin`, and the
reason is stated there: a chunk bakes its placement, and an instance that has to pick its own LOD
cannot have one baked in. `NeighbourhoodResolutionTests` checks both directions against the world
the build produced — every row resolves to a mesh, and every mesh is placed by a row.

§11.4's *"real windows with interior-glow cards at night"* are geometry on the LOD0 houses
(`HOUSE-00849`): one `NB_WINDOW_GLOW` quad per window, inset in the trim frame and 5 mm proud of
the wall, in its own material and therefore its own primitive so a day frame can leave it out.
**Which** of them are lit is not in the mesh — the mesh is the same every night and the lit windows
are not — and comes from `rendering::WindowGlow`, keyed on the ROW's id rather than its asset,
because the two LOD0 houses share a mesh where they share a shape.

`terrain.heightfield` and `terrain.materialIndex` are paths into the **deployed world directory**,
not asset ids: the two images are copied there verbatim by `deploy_world.py` and hashed into
`world.manifest.json` beside the JSON, so `WorldLoader::VerifyManifest` covers the ground the
player stands on. They are not content-pipeline assets and belong to no residency pack.

The height field is a **16-bit greyscale PNG** (§11.5) and the index an 8-bit one over the same
grid, both generated by `tools/world/terrain_gen.py` (`HOUSE-00761`) from §10.2's slope and this
file's own `paths`, `structures` and outdoor cells. Sample 0 is `origin.y` and 65535 is
`origin.y + yScale`; a material sample is a position in `terrain.materials`.

`samples`, `step` and `materials` live **here** and not in a sidecar beside the images. A
`terrain.json` would be a second statement of the grid, would not be deployed (it is not one of
the sixteen) and would not be hashed -- so the runtime would read its geometry from the one file
nothing checks. `terrain_gen.py --check` fails if this block and the generator disagree.

## `layout.weather.json`

The 14 archetypes, the transition matrix and the seasonal tables. Every quantity is continuous;
there is no `isRaining`.

```jsonc
{
  "schema": "cna-house/weather/1",
  "archetypes": [
    { "id": "W_PARTLY", "cloudCover": [0.25, 0.55], "cloudCumuliform": [0.4, 0.8],
      "precipType": "None", "precipIntensity": [0.0, 0.0],
      "windSpeed": [1.0, 5.0], "gustFactor": [0.1, 0.35], "fogDensity": [0.0, 0.05],
      "temperatureOffsetC": [-1.0, 2.0], "humidity": [0.4, 0.7], "weight": 1.0 }
  ],
  "transitions": { "W_PARTLY": { "W_CLEAR": 0.35, "W_OVERCAST": 0.40, "W_RAIN": 0.25 } },
  "timing": {
    "W_PARTLY": { "dwellMinutes": [60, 180], "transitionMinutes": [10, 25] }
  },
  "rates": { "cloudCoverPerMin": 0.045, "cloudCumuliformPerMin": 0.08,
    "precipIntensityPerMin": 0.07, "windSpeedPerMin": 0.9,
    "windDirectionDegPerMin": 4.5, "gustFactorPerMin": 0.15,
    "fogDensityPerMin": 0.03, "thunderIntensityPerMin": 0.055,
    "temperatureCPerMin": 0.25, "humidityPerMin": 0.08 },
  "seasons": [ { "id": "WINTER", "months": [12, 1, 2],
    "weights": { "W_SNOW": 2.5, "W_CLEAR": 0.6 },
    "dwellScale": { "W_SNOW": 1.2, "W_CLEAR": 0.8 } } ]
}
```

`rates` is the anti-absurdity guarantee (`cna-house.md` §42.2): it is what stops a clear sky
becoming a thunderstorm in four seconds. Its ten required rows cover every archetype-driven
continuous channel. `surfaceWetness` and `snowDepth` are integrated by their own equations rather
than moved toward archetype targets, and `precipType` is discrete.

`timing` provides one uniform dwell and characteristic transition-time distribution per state.
The effective transition range is the component-wise mean of the source and destination ranges,
so an abrupt state and a slow state both influence the change without duplicating timing data in a
13 × 13 table. A season's sparse `dwellScale` multiplies the destination dwell range; omitted
states keep scale 1, and the two neighbouring seasonal scales are blended continuously. The
effective bounds must remain inside §42.1's 25–380 minute dwell and 5–45 minute transition limits.

Every quantity is a **range**, not a value: §36.2's table gives the archetype's centre and the
target is drawn from the band, which is why two thunderstorms are not identical. `thunderProbability`
and `modifier` were added by `HOUSE-00393` — §36.2's table has a thunder column with nowhere to go,
and its `W_WINDY` row is a modifier that combines with any precipitation archetype rather than a
state you transition into. The file defines its wind and gust bands but no activation
distribution; the independent continuous modifier amount is therefore a caller input, not a
hidden runtime constant.

The authored `precipType` is nominal. For rain, snow and sleet targets the live outdoor
temperature overrides it: below 0 °C is snow, 0 through 2.5 °C inclusive is sleet, and above
2.5 °C is rain. `None` remains dry and `Hail` remains hail.

§36.3 asks for four seasonal transition matrices; this is one base matrix and four seasonal weight
vectors, which is the same thing in 13 numbers a season instead of 169 — the matrix says what
follows what and the season says what the year is fond of. Rule 6 checks that a transition names
states and that every state has a row to leave by; rule 10 checks that each row sums to 1, because
a row summing to 0.8 does not fail loudly — the sky just favours whatever the sampler reaches
first and the weather is subtly wrong forever.

## `layout.sky.json`

Sky gradient lookup tables by solar elevation, cloud layer definitions and the star catalogue
reference.

```jsonc
{
  "schema": "cna-house/sky/1",
  "gradient": [ { "sunElevationDeg": -18.0, "zenith": [0.01,0.01,0.03], "horizon": [0.02,0.02,0.05] },
                { "sunElevationDeg":   0.0, "zenith": [0.16,0.24,0.45], "horizon": [0.95,0.55,0.28] },
                { "sunElevationDeg":  60.0, "zenith": [0.16,0.35,0.78], "horizon": [0.62,0.74,0.90] } ],
  "cloudLayers": [
    { "id": "CL_HIGH", "texture": "Textures/Sky/cloud_cirrus", "altitude": 880.0,
      "scrollScale": 0.15, "opacity": 0.5 },
    { "id": "CL_MID", "texture": "Textures/Sky/cloud_cumulus", "altitude": 860.0,
      "scrollScale": 0.60, "opacity": 0.7 },
    { "id": "CL_LOW", "texture": "Textures/Sky/cloud_stratus", "altitude": 830.0,
      "scrollScale": 1.00, "opacity": 0.85 }
  ],
  "cloudAlpha": [
    { "cloudCover": [0.00, 0.10], "high": 0.15, "mid": 0.00, "low": 0.00,
      "midTint": null },
    // Three further contiguous bands omitted here.
    { "cloudCover": [0.85, 1.00], "high": 0.00, "mid": 0.60, "low": 0.95,
      "midTint": "storm_grey" }
  ],
  "stormCloudAlpha": { "high": 0.00, "mid": 0.70, "low": 1.00 },
  "stars": { "catalogue": "world/stars.bin", "count": 1500, "magnitudeLimit": 5.5 }
}
```

§31.2 describes the colouring as a 32 × 8 zenith LUT and a 32 × 8 × 16 horizon LUT — sun altitude
by cloud cover, and by angle from the sun's azimuth. This file carries the **clear-sky** curve over
sun altitude alone, because §31.2's own pseudocode applies the other two axes analytically:
`mix(c, overcastGrey, cloudCover^1.5)` is the cloud axis and `sunGlowTerm(angleToSun)` the azimuth
one. One 32-entry table plus two terms the CPU applies when it colours the dome represents the same
32 × 8 × 16 grid without committing 4096 redundant RGB triples (`HOUSE-00394`).

`colourModel` records those two terms rather than burying them in C++: `overcastGrey` is approached
as `cloudCover^1.5`, while `sunGlowColor`, `sunGlowStrength` and `sunGlowExponent` define the
azimuth lobe, attenuated by `(1-cloudCover)^2` and the sun table's intensity. The two sample counts
are fixed at 8 and 16 so tooling and runtime agree about the conceptual grid. The gradient and this
model are generated by `tools/world/sky_lut.py --emit` from eleven named altitude anchors;
`--check` rejects hand edits and `--selftest` expands all 4096 horizon samples. The warm dawn/dusk
band is deliberately darker away from the sun and restored by a local warm glow near it, instead of
painting the entire horizon orange (`HOUSE-01641`).

`HOUSE-01614` adds the second directional gradient recorded by `colourModel`: the fixed town
azimuth, warm RGB and strength, horizontal and vertical lobe exponents, and maximum star-magnitude
loss. It peaks due south at the horizon, falls cubically toward the zenith and is absent over the
northern half of the sky. The dome and star field read the same six values, so the visible glow and
the loss of faint southern stars cannot drift apart. Daylight blending removes the dome term, while
the existing star twilight factor already removes the stellar term by day.

`cloudAlpha`, `sun` and `moon` were added with it: §31.3's per-layer alphas by sky state and
§32/§33's disc colours by elevation had nowhere to live. Rule 10 checks that the three elevation
tables are in ascending order — one that is not interpolates backwards, and a sun table out of
order reddens at noon — and that the cloud-alpha bands are contiguous from 0 to 1, because a cover
in a gap is a sky with no clouds drawn at all. At runtime each `cloudAlpha` row is an anchor at its
band centre. Cover interpolates linearly between adjacent anchors and clamps outside the first and
last centre; `thunderIntensity` then linearly blends all three values toward `stormCloudAlpha`.
This makes both weather inputs continuous (`HOUSE-01648`).

`HOUSE-01645` also reuses the seven authored `sun` intensities for the directional dome glow.
`SkyColourModelReader` therefore requires those rows, validates their ascending elevations and
unit-range intensities, and interpolates the same values that `sky_lut.py` uses when expanding the
conceptual azimuth axis. The committed compact model and its 4,096-cell expansion oracle therefore
cannot silently diverge at twilight.

The cloud RGB is not another authored table. `HOUSE-01649` takes the live horizon colour produced
by `gradient` plus `colourModel.overcastGrey` and writes that common tint into all three rings'
vertex colours. Thus the white procedural textures take on dusk light and converge on the same
flat grey as the sky under complete cover, without a second colour curve that could drift.

## `interactables.json`

The 640 rows. `kind` selects one of the 12 behaviours; `actions` are declarative and use the
**closed expression vocabulary** — parsed at load time into a fixed expression tree over this
interactable's own typed state fields, with an unknown token a load-time error.

```jsonc
{
  "schema": "cna-house/interactables/1",
  "interactables": [
    {
      "id": "FRIDGE_L0_KITCHEN",
      "kind": "refrigerator",
      "cell": "L0_KITCHEN",
      "prop": "PROP_L0_KITCHEN_FRIDGE",
      "focus":  { "point": [1.20, 1.40, -26.40], "normal": [0, 0, 1], "radius": 0.85 },
      "bounds": { "min": [0.30, 0.60, -27.05], "max": [2.10, 2.55, -26.35] },
      "actions": [
        { "verb": "Open",  "when": "state.doorOpen == false", "do": "state.doorOpen = true",
          "sound": "SFX_FRIDGE_OPEN",  "anim": "door", "duration": 0.9 },
        { "verb": "Close", "when": "state.doorOpen == true",  "do": "state.doorOpen = false",
          "sound": "SFX_FRIDGE_CLOSE", "anim": "door", "duration": 0.7 }
      ],
      "childInteractables": ["FRIDGE_ITEM_MILK_1"],
      "state":  { "doorOpen": false, "interiorLightOn": false, "compressorRunning": true },
      "persist": ["doorOpen", "removedItems"],
      "audio":  { "loop": "AMB_FRIDGE_HUM", "emitter": [1.20, 0.90, -26.70] },
      "portal": "P_FRIDGE_INTERIOR"
    }
  ]
}
```

### Light switches

A switch plate is one row with one action per gang, and its **state field is named after the light
group** the gang controls: `state.LG_L0_HALL_MAIN`. That is what makes §53's three-way pair
expressible — "a group with two switch props" is two plates whose gangs name the same group — and
what §53's "one bit per group" persists. Rule 6 checks every state field of a `light_switch` row
against the declared groups, because `state` is free-form by design and a typo in a gang is a
switch that toggles nothing (`HOUSE-00384`).

### The expression vocabulary

`when` and `do` are parsed at load into a fixed tree over **this row's own** `state` fields
(`HOUSE-00354`, [`InteractableExpr.hpp`](../include/cnahouse/world/InteractableExpr.hpp)):

```
when := or
or   := and ( "||" and )*
and  := cmp ( "&&" cmp )*
cmp  := "!" cmp | "(" or ")" | term [ ("=="|"!="|"<"|"<="|">"|">=") term ]
term := "state." IDENT | NUMBER | "true" | "false" | "'" TEXT "'"

do   := stmt ( ";" stmt )*
stmt := "state." IDENT ("="|"+="|"-=") term | "toggle" "(" "state." IDENT ")"
```

`&&` binds tighter than `||`, and `!` tighter than both. An absent `when` is always true; an
absent `do` changes nothing, which is right for an action whose whole effect is a sound. Types are
checked at **parse**: `state.doorOpen == 0.5` is a load error when `doorOpen` is declared `false`,
and `<` orders numbers only.

The *operations* are closed and the *field names* are not a list. §50.4 has twelve behaviour
classes with about forty distinct state fields between them, so a closed list of setter verbs —
`setDoor`, `setFlow`, `setChannel` — would be a forty-entry table that has to be kept in step with
every behaviour class, and adding the 641st interactable would stop being "a JSON row". Checking
each field against the row's own `state` is closed by construction, catches a typo in exactly the
same way, and lets the message name the fields that do exist.

An unknown token is a load-time error naming the file, the interactable, the action and the token:

```
interactables.json/FRIDGE_L0_KITCHEN Open when [offset 6 of "state.doorAjar == true"]:
  this interactable has no state field "doorAjar"; it declares doorOpen, temperatureC, programme
```

`persist` names exactly the fields the save carries. A field not listed is derived or transient and
is recomputed on load — that is what keeps a save at ~90 KB and what makes adding a transient field
free.

## `initialstate.json`

The canonical initial state (`cna-house.md` §65.6): the state of the house on a fresh start. It is
the reference every delta save is taken against and every *Reset House* returns to, so it is
validated as strictly as the layout.

```jsonc
{
  "schema": "cna-house/initialstate/1",
  "player":  { "cell": "L0_FOYER", "position": [0.0, 0.60, -18.4], "yawDeg": 0.0 },
  "clock":   { "epochSeconds": 21600.0, "timeScale": 60.0,
               "latitudeDeg": 40.05, "longitudeDeg": -75.30, "utcOffsetMinutes": -300 },
  "weather": { "target": "W_PARTLY", "cloudCover": 0.35, "cloudCumuliform": 0.9,
               "precipType": "None", "precipIntensity": 0.0,
               "windSpeed": 3.2, "windDirectionDeg": 225.0, "gustFactor": 0.28,
               "fogDensity": 0.03, "thunderIntensity": 0.0,
               "temperatureC": 21.5, "humidity": 0.55,
               "surfaceWetness": 0.0, "snowDepth": 0.0,
               "targetExpiryMinutes": 140.0, "rngState": "0x5EEDC0DEC0FFEE01" },
  "interactables": { "DOOR_L0_FRONT": { "openFraction": 0.0, "latched": true, "locked": true } },
  "pets":    { "PET_DOG": { "cell": "L0_FAMILY", "state": "Lie" } }
}
```

The fresh-world `rngState` is a single 64-bit seed (16 hexadecimal digits), expanded through
SplitMix64 into xoshiro256++'s four-word state. Saves carry the expanded 64-digit state instead;
conflating the two would make the first save/load change the future sequence.

## `assets.manifest.json`

The asset manifest ([ADR-0012](decisions/ADR-0012-asset-licensing.md)). One row per file under
`assets-src/`; **no row, no build**. The full schema is `cna-house.md` §20.3.

---

## The machine-checkable half

This document is the reference a person reads. Beside it, [`world-schema/`](world-schema/) carries
the same sixteen files as **JSON Schema draft 2020-12** — one `<kind>.schema.json` each — which an
editor can attach for completion and diagnostics as you type, and which
[`validate_world.py`](../tools/world/validate_world.py) runs before any of its own rules.

They are generated from a single source, [`tools/world/world_schema.py`](../tools/world/world_schema.py),
so that the id pattern, the `[x, y, z]` vector, the `{"x": [min, max], "z": [min, max]}` range and
the `schema` header rule are one definition rather than sixteen that can quietly disagree.
`world_schema.py --check` is a CI gate; regenerate with `--emit` after changing a shape here.

The schemas check **shape**: presence, type, range, and that an id looks like an id, each problem
reported against the field that carries it. They deliberately do **not** check the thirteen rules
below. Rules 4, 5, 6, 7, 9 and 11 span two files or the whole layout, and JSON Schema cannot see
across a file boundary; rules 2, 3, 8 and 10 compare two numbers to each other, which it also
cannot do. A schema that attempted them would be a second, weaker validator — so a portal naming a
cell that does not exist, and a box whose `min` exceeds its `max`, both pass the schema and fail
the validator.

## Validation

`tools/world/validate_world.py`, mirrored by a C++ validator the unit tests use, enforces twelve
rules (`cna-house.md` §15.7) on top of the schemas above. It runs in CI and as a pre-build step, and a failure **fails the
build**:

1. every id is unique and matches `^[A-Z][A-Z0-9_]*$`;
2. every cell box is non-degenerate and inside its level's envelope;
3. no two cells on a level overlap by more than 1 cm²;
4. every portal rectangle lies in both cells' boundary planes within 1 cm — or in the **wall**
   between them when they do not abut: two rooms either side of a partition share a coordinate
   (§13.1), but a room and the yard outside it do not, because there are `wallExterior` metres of
   wall in between and the window is in them. The two facing planes must straddle the portal, each
   must span the opening, and they must be no further apart than the thickest wall `construction`
   declares — a portal in the middle of a room, on the wrong wall, or between two cells that do not
   face each other still fails. The `v` range lies inside both cells' vertical extent;
5. the portal graph is connected — every cell that is not `void` is reachable from `L0_FOYER`
   through always-open or door portals. Not "every interior cell": `EXT_SHED` is an `exterior`
   cell that is indoors, and under the narrower wording it was authored with no door
   (`HOUSE-00374`). Nothing is exempt, `EXT_WORLD` included — it is where the road runs off the
   map, so it has a portal like everything else;
6. every referenced material, asset, light group, sound, nav region and animation exists;
7. every door has exactly one portal, and every window has exactly one portal — in both
   directions: the leaf names the portal and the portal names the leaf in `aperture`. Every kind
   that has something that opens is covered, `garage_door` and `hatch` included: a sectional door
   is five hinged segments and a chest lid lifts (widened by `HOUSE-00378`);
8. stair flights connect the declared cells and their total rise equals the level difference to
   within 1 mm. Two extras `HOUSE-00379` added: a portal must join the two cells, or the flight is
   a staircase into a wall; and a flight between two cells on **one** level is checked against the
   `fromY`/`toY` it declares, because one level has no level difference to check against. `rise`
   is a magnitude, so a flight authored downward is the same flight;
9. every plumbing fixture's cell appears in a declared stack;
10. realism: door heights, ceiling heights, counter heights, stair `2R + G`, human/pet/car scale,
    capsule clearance through every portal, and every light's position inside the footprint and
    vertical extent of the cell it names (`HOUSE-00383`);
11. every interactable's `focus.point` is inside its cell and reachable by a 2.5 m ray from a
    standing eye position on the room's floor — a reachability **proof**, not an assumption;
12. nothing outdoors stands in something else: no two `structures` footprints overlap, no `paths`
    box runs into one, and no `vegetation` instance is inside one (`HOUSE-00769`). Rules 2 and 3
    say this about the house's cells; the lot has three kinds of rectangle of its own and nothing
    compared them, which is how a garden path came to run three metres through the shed. A
    centimetre of overlap is the tolerance, so a lean-to against a wall and a path that stops at a
    threshold are both still legal.

Error messages name the file, the JSON path and what was expected, and the validator reports
**every** failing row rather than the first: fixing 40 authoring mistakes one build at a time is
intolerable ([`conventions.md`](conventions.md) §5.1).

Three of the rules read further than their one-line summary suggests, and it is worth knowing
which way:

* **Rule 3** compares two cells only when they also share vertical space. Two cells on one level
  legitimately share a footprint when one carries a `yOverride` — a stair void open to the floor
  below sits over the room it looks into — and without the guard the rule would forbid the one
  arrangement the format has a field for.
* **Rule 10** checks the four §70.5 rows the layout decides: interior door leaf size, habitable
  clear height, stair `2·rise + going`, and player capsule clearance. Counter, seat, sill, switch
  and socket heights, and human, pet and car scale, are properties of an *asset* and belong to
  `scale_check.py`; §70.5's "rise consistency within a flight" is not checkable at all, because
  one `rise` per flight makes every riser equal by construction.
* **Rule 11** searches the interactable's cell **and its portal-neighbours**, requires the eye
  position to be somewhere the player capsule can stand, and requires a segment that leaves the
  cell to cross the shared plane inside the portal rectangle. A shallow closet, a cabinet and a
  meter cupboard are all reached from the room next door, and a switch in a 0.4 m slot has floor
  beneath it and nobody who can reach it.
