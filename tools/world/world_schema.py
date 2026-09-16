#!/usr/bin/env python3
"""world_schema.py -- the 16 world files, as JSON Schema a machine can check.

`HOUSE-00341`. `HOUSE-00033` wrote `docs/world-format.md`, which is the reference a person reads.
This is the half a person cannot check by reading: the same 16 files as **JSON Schema draft
2020-12**, emitted to `docs/world-schema/`, so that `layout.cells.json` can be told it is wrong
before `WorldLoader` (`HOUSE-00343`…) or any of `HOUSE-00210`…`HOUSE-00215`'s tools trip over it.

    tools/world/world_schema.py --emit          # write docs/world-schema/*.schema.json
    tools/world/world_schema.py --check         # fail if those files are stale
    tools/world/world_schema.py --validate DIR  # validate a world directory against them
    tools/world/world_schema.py --selftest

`--emit` and `--check` need nothing but the standard library, which is why `--check` can sit in
`run_checks.sh` beside the gates that run on every machine. `--validate` and `--selftest` need
`jsonschema`; CI installs it for them alone.

## Generated from one source, not sixteen hand-written files

Every one of the sixteen needs the same `schema` header rule, the same id pattern, the same
`[x, y, z]` vector and the same `{"x": [min, max], "z": [min, max]}` range. Written out sixteen
times those drift, and the drift is invisible: a schema that accepts a lower-case id in one file
and not in another looks like sixteen correct files. They are built here from shared `$defs` and
emitted, and `--check` is a gate, which is the same arrangement `budget_report.py` and
`build_content.py` already use.

## What these schemas do and do not decide

They check **shape**: that a field is present, is the right type, is in range, and that an id looks
like an id. They deliberately do **not** check the eleven cross-file rules of §15.7 — that a portal
lies in both its cells' planes, that the graph is connected, that every material referenced exists.
Those need the whole layout at once and belong to `validate_world.py` (`HOUSE-00358`), and a schema
that tried would be a second, weaker copy of it.

The division matters for a practical reason: a shape error should be reported against the **file,
line and field** that carries it, which a schema does well, while a reference error can only be
reported against the pair of files it spans.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_io  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SCHEMA_DIR = REPO / "docs" / "world-schema"

DRAFT = "https://json-schema.org/draft/2020-12/schema"

#: `docs/world-format.md`: ids are `^[A-Z][A-Z0-9_]*$` and globally unique across every kind.
ID_PATTERN = "^[A-Z][A-Z0-9_]*$"

#: The sixteen WORLD files. `assets.manifest.json` is in `layout_io.FILES` and is not one of them:
#: it is the asset manifest, whose schema is `cna-house.md` §20.3 and whose gate is
#: `check_manifest.py`.
WORLD_KINDS = [k for k in layout_io.FILES if k != "assets"]


def defs() -> dict:
    """The shared vocabulary. Written once so sixteen files cannot disagree about it."""
    return {
        "id": {"type": "string", "pattern": ID_PATTERN},
        "idOrNull": {"anyOf": [{"$ref": "#/$defs/id"}, {"type": "null"}]},
        "vector3": {"type": "array", "items": {"type": "number"},
                    "minItems": 3, "maxItems": 3},
        "rgb": {"type": "array", "items": {"type": "number", "minimum": 0},
                "minItems": 3, "maxItems": 3},
        "interval": {"type": "array", "items": {"type": "number"},
                     "minItems": 2, "maxItems": 2},
        "unit": {"type": "number", "minimum": 0.0, "maximum": 1.0},
        "unitInterval": {"type": "array",
                         "items": {"type": "number", "minimum": 0.0, "maximum": 1.0},
                         "minItems": 2, "maxItems": 2},
        # A footprint box. `docs/world-format.md`: `{"x": [min, max], "z": [min, max]}`, min < max.
        # The min < max part is not expressible in JSON Schema and is `validate_world.py` rule 2.
        "box": {
            "type": "object", "additionalProperties": False,
            "required": ["x", "z"],
            "properties": {"x": {"$ref": "#/$defs/interval"},
                           "z": {"$ref": "#/$defs/interval"}},
        },
        "sha256": {"type": "string", "pattern": "^sha256:[0-9a-f]{64}$"},
    }


def envelope(kind: str, title: str, payload: dict) -> dict:
    """A whole file: the `schema` header every world file carries, plus its own body."""
    properties = {
        "schema": {"type": "string", "pattern": rf"^cna-house/{kind}/\d+$"},
    }
    properties.update(payload.get("properties", {}))
    return {
        "$schema": DRAFT,
        "$id": f"https://openeggbert.org/cna-house/world-schema/{kind}.schema.json",
        # JSON has no comments, and these files are the one place a reader meets them without
        # meeting the generator. Say so here or somebody edits one and loses the edit.
        "$comment": ("Generated by tools/world/world_schema.py (HOUSE-00341). Do not edit: "
                     "change the generator and run --emit. docs/world-format.md is the prose."),
        "title": title,
        "type": "object",
        "required": ["schema"] + payload.get("required", []),
        "properties": properties,
        # Closed by default. `docs/world-format.md`: "A file present in the directory but absent
        # from the manifest is also an error -- silent extra data is how two sources of truth
        # begin", and the same argument applies inside a file.
        "additionalProperties": False,
        "$defs": defs(),
    }


def rows(name: str, item: dict, extra_required: list[str] | None = None) -> dict:
    return {"required": [name] + (extra_required or []),
            "properties": {name: {"type": "array", "items": item}}}


def obj(required: list[str], properties: dict, *, closed: bool = True) -> dict:
    return {"type": "object", "required": required, "properties": properties,
            "additionalProperties": not closed}


def nullable(schema: dict) -> dict:
    return {"anyOf": [schema, {"type": "null"}]}


ID = {"$ref": "#/$defs/id"}
ID_OR_NULL = {"$ref": "#/$defs/idOrNull"}
VEC3 = {"$ref": "#/$defs/vector3"}
RGB = {"$ref": "#/$defs/rgb"}
INTERVAL = {"$ref": "#/$defs/interval"}
UNIT = {"$ref": "#/$defs/unit"}
BOX = {"$ref": "#/$defs/box"}
NUM = {"type": "number"}
STR = {"type": "string"}
BOOL = {"type": "boolean"}


def build() -> dict[str, dict]:
    """Every world file's schema, keyed by `layout_io` kind."""
    schemas: dict[str, dict] = {}

    schemas["manifest"] = envelope("manifest", "world.manifest.json", {
        "required": ["worldHash", "members"],
        "properties": {
            "worldHash": {"$ref": "#/$defs/sha256"},
            "members": {"type": "array", "items": obj(
                ["file", "sha256"],
                {"file": STR, "sha256": {"$ref": "#/$defs/sha256"}})}}})

    schemas["levels"] = envelope("levels", "layout.levels.json", {
        "required": ["levels", "construction"],
        "properties": {
            "units": {"const": "metres"},
            "north": {"const": "-Z"},
            "levels": {"type": "array", "items": obj(
                ["id", "name", "ffl"],
                {"id": ID, "name": STR, "ffl": NUM, "ceiling": nullable(NUM),
                 "structureDepth": NUM, "roof": ID_OR_NULL})},
            "construction": obj([], {
                "wallExterior": NUM, "wallPartition": NUM, "wallPlumbing": NUM,
                "wallGarage": NUM, "foundationWall": NUM, "kneeWallHeight": NUM,
                "ridgeY": NUM, "roofPitch": NUM, "skirting": NUM, "cornice": NUM,
                "balustrade": NUM, "railing": NUM}, closed=False),
            # §12.5's STACK-A..F. They live here rather than in a file of their own because they
            # are house-wide structural constants, which is what this file already holds, and
            # because `validate_world.py` rule 9 needs them to exist before `HOUSE-00386`
            # authors the rows.
            # §12.5's HVAC row, as data (`HOUSE-00387`). Beside `plumbing` and for the same
            # reasons: it is a house-wide structural constant, and §62.6 places the duct rumble
            # and tick "at each register", which needs the registers to exist somewhere.
            "hvac": obj(["plant", "branches"], {
                "plant": obj(["cell"], {"cell": ID, "position": VEC3, "chase": BOX}),
                "branches": {"type": "array", "items": obj(
                    ["id", "cells"],
                    {"id": ID, "cells": {"type": "array", "items": ID, "minItems": 1},
                     "trunk": {"anyOf": [STR, {"type": "null"}]},
                     "registers": {"type": "array", "items": obj(
                         ["cell", "position"], {"cell": ID, "position": VEC3,
                                                "kind": {"enum": ["floor", "ceiling", "wall"]}})}})}}),
            "plumbing": obj(["stacks"], {"stacks": {"type": "array", "items": obj(
                ["id", "cells", "chase"],
                {"id": ID, "cells": {"type": "array", "items": ID, "minItems": 1},
                 "chase": BOX, "dropTo": ID_OR_NULL})}})}})

    schemas["cells"] = envelope("cells", "layout.cells.json", rows("cells", obj(
        ["id", "level", "kind", "boxes"],
        {"id": ID, "level": ID, "name": STR,
         "kind": {"enum": ["room", "corridor", "stair", "closet", "garage", "exterior", "void"]},
         "boxes": {"type": "array", "items": BOX, "minItems": 1},
         "yOverride": nullable(INTERVAL),
         "floorMaterial": ID_OR_NULL, "wallMaterial": ID_OR_NULL,
         "ceilingMaterial": ID_OR_NULL, "trimMaterial": ID_OR_NULL,
         "footstepSurface": {"anyOf": [STR, {"type": "null"}]},
         "acoustic": obj([], {"roomTone": ID_OR_NULL, "absorption": UNIT,
                              "reverbHint": STR}),
         "thermal": obj([], {"heated": BOOL, "ductBranch": ID_OR_NULL}),
         "lightGroups": {"type": "array", "items": ID},
         "daylight": obj([], {"windowIds": {"type": "array", "items": ID},
                              "orientation": {"enum": ["N", "NE", "E", "SE",
                                                       "S", "SW", "W", "NW"]},
                              "exposure": UNIT}),
         # Generated by the full-house bake driver. The scale restores the HDR range removed
         # before each linear PNG was written; contentName is what XNA's ContentManager loads.
         "lightmaps": obj(["shellHash", "daylight", "artificial"], {
             "shellHash": {"$ref": "#/$defs/sha256"},
             "daylight": nullable(obj(["contentName", "scale"], {
                 "contentName": STR, "scale": {"type": "number", "exclusiveMinimum": 0}})),
             "artificial": {"type": "array", "items": obj(
                 ["group", "contentName", "scale"],
                 {"group": ID, "contentName": STR,
                  "scale": {"type": "number", "exclusiveMinimum": 0}})}}),
         "residencyPack": {"anyOf": [STR, {"type": "null"}]},
         "lodBias": {"type": "integer"},
         "visibilityHint": {"enum": ["opaque", "open"]},
         "navMeshRegion": ID_OR_NULL,
         # §54: "a container is a tiny sub-cell with its own portal, so this falls out of the
         # visibility system rather than being a special case". A sub-cell names the cell it is
         # nested in; without that, rule 3 has no way to tell a fridge interior from two rooms
         # drawn on top of each other, and 214 containers would each have to be carved out of the
         # room around them.
         "parent": ID_OR_NULL})))

    schemas["portals"] = envelope("portals", "layout.portals.json", rows("portals", obj(
        ["id", "cellA", "cellB", "plane", "rect", "kind"],
        {"id": ID, "cellA": ID, "cellB": ID,
         # `x` and `z` are the walls; `y` is a horizontal plane, which `stair_well` and `hatch`
         # need and which the vocabulary listed without providing. On `y`, `u` is world X and `v`
         # is world Z. See docs/world-format.md, layout.portals.json.
         "plane": obj(["axis", "value"], {"axis": {"enum": ["x", "y", "z"]}, "value": NUM}),
         "rect": obj(["u", "v"], {"u": INTERVAL, "v": INTERVAL}),
         "kind": {"enum": ["cased_opening", "door", "double_door", "slider", "window",
                           "garage_door", "stair_well", "exterior_door", "hatch"]},
         "aperture": ID_OR_NULL,
         "opacity": {"enum": ["open", "opaque_when_closed", "translucent", "glass"]},
         # §70.5 exempts a deliberately low portal from the 1.95 m capsule clearance. Two-state,
         # so a boolean is right here where a continuous quantity would not be.
         "crouch": BOOL,
         "maxDepth": nullable({"type": "integer", "minimum": 0}),
         "soundLoss": obj([], {"open": UNIT, "closed": UNIT})})))

    schemas["openings"] = envelope("openings", "layout.openings.json", rows("openings", obj(
        ["id", "kind", "portal", "leaf"],
        {"id": ID, "kind": {"enum": ["door", "window"]}, "portal": ID,
         # The opening's TYPE, §12.3's and §12.6's schedules: `D_INT_PASSAGE`, `W_DH_STD`. Added
         # by `HOUSE-00378`, because both documents describe openings by type and 133 rows that
         # carried only their measurements could not be asked "which of these are bathroom
         # windows". Id-shaped but not an id: it names a kind, so many rows share one and rule 1
         # never sees it.
         "type": {"$ref": "#/$defs/id"},
         "leaf": obj(["width", "height"], {"width": NUM, "height": NUM, "thickness": NUM}),
         "hinge": {"enum": ["left", "right", None]},
         "swing": {"anyOf": [STR, {"type": "null"}]},
         "maxAngleDeg": NUM,
         "frame": obj([], {"asset": ID_OR_NULL, "casing": NUM}),
         "asset": ID_OR_NULL, "material": ID_OR_NULL,
         "solid": BOOL, "lockable": BOOL})))

    schemas["stairs"] = envelope("stairs", "layout.stairs.json", rows("flights", obj(
        ["id", "fromCell", "toCell", "risers", "rise", "going", "width"],
        {"id": ID, "fromCell": ID, "toCell": ID,
         "risers": {"type": "integer", "minimum": 1},
         "rise": {"type": "number", "exclusiveMinimum": 0},
         "going": {"type": "number", "exclusiveMinimum": 0},
         "width": {"type": "number", "exclusiveMinimum": 0},
         # The heights a flight starts and ends at, for the flights that connect two cells on ONE
         # level -- the porch, terrace and garage steps. §15.7 rule 8 checks the climb against the
         # two levels' FFLs, and between two cells on the same level that difference is zero: the
         # flight is the only thing that knows what it climbs. Added by `HOUSE-00379`.
         "fromY": NUM, "toY": NUM,
         # Where the flight IS, and which way you go while climbing it (`HOUSE-00459`). §12.4 has
         # given every flight a footprint and a shape since it was written, in prose; the geometry
         # generator needs them as data, and a generator that read the prose would be reading a
         # table the runtime cannot. `run` is the direction of travel while going UP.
         "footprint": BOX,
         "run": {"enum": ["-X", "+X", "-Z", "+Z"]},
         "shape": {"enum": ["straight", "u"]},
         "landings": {"type": "array", "items": obj(
             ["at", "depth"], {"at": {"type": "integer", "minimum": 0}, "depth": NUM})},
         "collisionRamp": BOOL,
         "surface": {"anyOf": [STR, {"type": "null"}]}})))

    schemas["lights"] = envelope("lights", "layout.lights.json", rows("lights", obj(
        ["id", "cell", "group", "type", "position"],
        {"id": ID, "cell": ID, "group": ID,
         "type": {"enum": ["point", "spot", "directional", "area_proxy", "emissive_only"]},
         "bulbClass": {"enum": ["filament", "led", "fluorescent"]},
         "position": VEC3, "direction": VEC3,
         "colorK": {"type": "number", "minimum": 1000, "maximum": 12000},
         "intensityLm": {"type": "number", "minimum": 0},
         "range": {"type": "number", "minimum": 0},
         # Optional extra receiver cells for a baked spill. The light still belongs to `cell`
         # for switching, exposure and dynamic-object assignment; this list only tells the
         # offline baker that a fixed neighbouring shell can see it.
         "bakeCells": {"type": "array", "items": ID, "uniqueItems": True},
         # Optional adjacent unbaked static-detail receivers. Runtime uses the fixed source only
         # for their stock-BasicEffect chunks; room state and dynamic objects remain cell-local.
         "spillCells": {"type": "array", "items": ID, "uniqueItems": True},
         "coneInnerDeg": {"type": "number", "minimum": 0, "maximum": 180},
         "coneOuterDeg": {"type": "number", "minimum": 0, "maximum": 180},
         "fixtureProp": ID_OR_NULL, "emissiveMaterialSlot": {"anyOf": [STR, {"type": "null"}]},
         "castsBlobShadow": BOOL, "bakedIntoLightmap": BOOL, "defaultOn": BOOL,
         "duskSensor": BOOL})))

    schemas["props"] = envelope("props", "layout.props.json", rows("props", obj(
        ["id", "asset", "cell", "position"],
        {"id": ID, "asset": ID, "cell": ID, "position": VEC3,
         "yawDeg": NUM, "scale": {"type": "number", "exclusiveMinimum": 0},
         "static": BOOL, "lodGroup": ID_OR_NULL,
         "collision": {"enum": ["proxy", "none", "box"]},
         "material": ID_OR_NULL, "interactable": ID_OR_NULL,
         # A plumbing fixture names the §12.5 stack it drains to; everything else is null. This
         # is what makes rule 9 checkable: without it "every fixture's cell appears in a declared
         # stack" has no way to say which props are fixtures.
         "plumbing": ID_OR_NULL})))

    schemas["materials"] = envelope("materials", "layout.materials.json", rows(
        "materials", obj(
            ["id", "class"],
            {"id": ID, "class": STR, "albedo": {"anyOf": [STR, {"type": "null"}]},
             "normal": {"anyOf": [STR, {"type": "null"}]},
             "lightmapChannel": {"type": "integer", "minimum": 0, "maximum": 1},
             "tint": RGB, "specularColor": RGB, "specularPower": NUM,
             "alphaMode": {"enum": ["opaque", "mask", "blend"]},
             "alpha": UNIT, "alphaCutoff": nullable(UNIT), "twoSided": BOOL,
             "uvScale": {"type": "array", "items": NUM, "minItems": 2, "maxItems": 2},
             "wetResponse": obj([], {"albedoDarken": UNIT, "specularBoost": NUM,
                                     "powerBoost": NUM}),
             "snowResponse": obj([], {"coverable": BOOL,
                                      "slopeLimitDeg": {"type": "number", "minimum": 0,
                                                        "maximum": 90}}),
             "footstepSurface": {"anyOf": [STR, {"type": "null"}]},
             "audioAbsorption": UNIT,
             "effectTierS": {"enum": ["Basic", "DualTexture", "AlphaTest", "Skinned"]},
             "effectTierE": {"anyOf": [STR, {"type": "null"}]}})))

    species = {"type": "array", "items": {"enum": ["dog", "cat"]}}
    schemas["nav"] = envelope("nav", "layout.nav.json", {
        "required": ["nodes", "edges"],
        "properties": {
            "nodes": {"type": "array", "items": obj(
                ["id", "cell", "position"],
                {"id": ID, "cell": ID, "position": VEC3, "kind": STR})},
            "edges": {"type": "array", "items": obj(
                ["a", "b"], {"a": ID, "b": ID, "portal": ID_OR_NULL, "cost": NUM,
                             "species": species})},
            "perches": {"type": "array", "items": obj(
                ["id", "cell"], {"id": ID, "cell": ID, "position": VEC3, "species": species})},
            "beds": {"type": "array", "items": obj(
                ["id"], {"id": ID, "cell": ID, "prop": ID_OR_NULL, "position": VEC3,
                         "species": species})},
            "bowls": {"type": "array", "items": obj(
                ["id"], {"id": ID, "cell": ID, "prop": ID_OR_NULL, "position": VEC3,
                         "species": species})},
            "forbidden": {"type": "array", "items": obj(
                ["cell", "species"], {"cell": ID, "species": species})}}})

    schemas["audio"] = envelope("audio", "layout.audio.json", {
        "required": ["zones"],
        "properties": {
            "zones": {"type": "array", "items": obj(
                ["id", "cell"], {"id": ID, "cell": ID, "bed": ID_OR_NULL, "gain": UNIT})},
            "emitters": {"type": "array", "items": obj(
                ["id", "cell", "position"],
                {"id": ID, "cell": ID, "position": VEC3, "loop": ID_OR_NULL,
                 "gain": UNIT, "radius": {"type": "number", "minimum": 0},
                 "interactable": ID_OR_NULL})},
            "transmission": {"type": "object", "additionalProperties": obj(
                [], {"open": UNIT, "closed": UNIT})}}})

    schemas["exterior"] = envelope("exterior", "layout.exterior.json", {
        "required": ["terrain"],
        "properties": {
            "terrain": obj(["heightfield", "size", "origin"], {
                "heightfield": STR,
                "size": {"type": "array", "items": NUM, "minItems": 2, "maxItems": 2},
                "origin": VEC3, "yScale": NUM, "material": ID_OR_NULL,
                # How to read the height field, in the file that is deployed and hashed rather
                # than in a sidecar beside it that neither happens to (`HOUSE-00761`).
                "samples": {"type": "array", "items": {"type": "integer", "minimum": 2},
                            "minItems": 2, "maxItems": 2},
                "step": NUM,
                "materialIndex": {"anyOf": [STR, {"type": "null"}]},
                "materials": {"type": "array", "items": STR}}),
            "road": obj([], {"centreline": {"type": "array", "items": VEC3, "minItems": 2},
                             "width": NUM, "material": ID_OR_NULL}),
            "fences": {"type": "array", "items": obj(
                ["id", "asset", "path"],
                {"id": ID, "asset": ID,
                 "path": {"type": "array", "items": VEC3, "minItems": 2},
                 "height": NUM, "gate": ID_OR_NULL})},
            # §11.2's gates, §11.4's kerbs and the paved zones of §11.4 and §11.6. Added by
            # `HOUSE-00390`: the file had terrain, a road and fences, and the property has a
            # driveway, a front walk, two sidewalks, a garden path, three gates and a shed.
            # `hinge` is which side of its opening a swinging leaf is hung on (`HOUSE-00767`).
            # §11.2 says the pedestrian gate is "single hinge east" and says nothing about the
            # other two, and a hinge side is not derivable from anything else in the file: a gate
            # without it is hung at the low end of its own opening, which is a default and not a
            # fact, so the one §11.2 states is authored.
            "gates": {"type": "array", "items": obj(
                ["id", "fence", "kind", "opening"],
                {"id": ID, "fence": ID, "kind": {"enum": ["hinged", "sliding", "bolted"]},
                 "opening": BOX, "height": NUM, "asset": ID_OR_NULL,
                 "hinge": {"enum": ["north", "south", "east", "west"]},
                 "interactable": ID_OR_NULL})},
            "kerbs": {"type": "array", "items": obj(
                ["id", "path"],
                {"id": ID, "path": {"type": "array", "items": VEC3, "minItems": 2},
                 "height": NUM, "material": ID_OR_NULL})},
            "paths": {"type": "array", "items": obj(
                ["id", "kind", "boxes"],
                {"id": ID,
                 "kind": {"enum": ["walk", "driveway", "sidewalk", "garden", "apron", "verge"]},
                 "boxes": {"type": "array", "items": BOX, "minItems": 1},
                 "y": NUM, "material": ID_OR_NULL})},
            # A structure with a `cell` is a BUILDING -- you stand inside it, and its heights
            # come from the cell and from `eavesY`/`ridgeY`. One without a cell is garden
            # furniture: §11.1's raised beds, its trellis and the compost bin, which have no
            # inside and one height each (`HOUSE-00769`). `height` is measured from the GROUND
            # under them, because that is what they stand on.
            "structures": {"type": "array", "items": obj(
                ["id", "footprint"],
                {"id": ID, "cell": ID_OR_NULL, "footprint": BOX, "asset": ID_OR_NULL,
                 "eavesY": NUM, "ridgeY": NUM,
                 "height": {"type": "number", "exclusiveMinimum": 0}})},
            # `HOUSE-00776`: §37.3's splash points and §37.4's trickle emitters. The pipes have
            # been drawn since `HOUSE-00468` and nothing outside the shell knew they existed;
            # neither a particle system nor an `Apply3D` emitter can read a Blender mesh.
            # `position` is the head, at the gutter; `splash` is where the water lands, on the
            # GROUND under it rather than at +0.00, which is 0.13 m out at the north corners.
            "downspouts": {"type": "array", "items": obj(
                ["id", "roof", "position", "splash"],
                {"id": ID, "roof": ID, "position": VEC3, "splash": VEC3,
                 "material": ID_OR_NULL})},
            "neighbourhood": {"type": "array", "items": obj(
                ["id", "asset", "position"],
                {"id": ID, "asset": ID, "position": VEC3, "yawDeg": NUM,
                 "lodGroup": ID_OR_NULL, "impostorFrom": NUM})},
            "vegetation": {"type": "array", "items": obj(
                ["id", "asset", "instances"],
                {"id": ID, "asset": ID,
                 # `HOUSE-00772`: a source model can be the right species and proportion but a
                 # different authored role. The hedge's one measured height belongs on its group,
                 # not copied into 169 deterministic jittered placement scales. The renderer
                 # multiplies both positive factors; collision remains the role's authored proxy.
                 "scale": {"type": "number", "exclusiveMinimum": 0},
                 "instances": {"type": "array", "items": obj(
                    ["position"], {"position": VEC3, "yawDeg": NUM,
                                   "scale": {"type": "number", "exclusiveMinimum": 0}})}})}}})

    schemas["weather"] = envelope("weather", "layout.weather.json", {
        "required": ["archetypes", "transitions", "timing", "rates", "seasons"],
        "properties": {
            "archetypes": {"type": "array", "items": obj(
                ["id", "cloudCover", "precipType", "precipIntensity", "windSpeed"],
                {"id": ID,
                 "cloudCover": {"$ref": "#/$defs/unitInterval"},
                 "cloudCumuliform": {"$ref": "#/$defs/unitInterval"},
                 "precipType": {"enum": ["None", "Rain", "Snow", "Hail", "Sleet"]},
                 "precipIntensity": {"$ref": "#/$defs/unitInterval"},
                 "windSpeed": INTERVAL, "gustFactor": {"$ref": "#/$defs/unitInterval"},
                 "fogDensity": {"$ref": "#/$defs/unitInterval"},
                 "temperatureOffsetC": INTERVAL,
                 "humidity": {"$ref": "#/$defs/unitInterval"},
                 # §36.2's table has a `thunder` column and this file had nowhere to put it, and a
                 # `W_WINDY` row that is a MODIFIER rather than a state -- it combines with any
                 # precipitation archetype, which is how "windy heavy rain" arises without a
                 # combinatorial state list. Both added by `HOUSE-00393`.
                 "thunderProbability": {"$ref": "#/$defs/unitInterval"},
                 "modifier": BOOL,
                 "weight": {"type": "number", "minimum": 0}})},
            "transitions": {"type": "object", "additionalProperties": {
                "type": "object", "additionalProperties": UNIT}},
            "timing": {"type": "object", "additionalProperties": obj(
                ["dwellMinutes", "transitionMinutes"],
                {"dwellMinutes": {"type": "array", "minItems": 2, "maxItems": 2,
                                  "items": {"type": "number", "minimum": 25,
                                            "maximum": 380}},
                 "transitionMinutes": {"type": "array", "minItems": 2, "maxItems": 2,
                                       "items": {"type": "number", "minimum": 5,
                                                 "maximum": 45}}})},
            "rates": obj(
                ["cloudCoverPerMin", "cloudCumuliformPerMin", "precipIntensityPerMin",
                 "windSpeedPerMin", "windDirectionDegPerMin", "gustFactorPerMin",
                 "fogDensityPerMin", "thunderIntensityPerMin", "temperatureCPerMin",
                 "humidityPerMin"],
                {name: {"type": "number", "exclusiveMinimum": 0} for name in (
                    "cloudCoverPerMin", "cloudCumuliformPerMin", "precipIntensityPerMin",
                    "windSpeedPerMin", "windDirectionDegPerMin", "gustFactorPerMin",
                    "fogDensityPerMin", "thunderIntensityPerMin", "temperatureCPerMin",
                    "humidityPerMin")}),
            "seasons": {"type": "array", "items": obj(
                ["id", "months", "weights", "dwellScale"],
                {"id": ID,
                 "months": {"type": "array", "items": {"type": "integer",
                                                       "minimum": 1, "maximum": 12}},
                 "weights": {"type": "object",
                             "additionalProperties": {"type": "number", "minimum": 0}},
                 "dwellScale": {"type": "object",
                                "additionalProperties": {"type": "number",
                                                         "exclusiveMinimum": 0}}})}}})

    schemas["sky"] = envelope("sky", "layout.sky.json", {
        "required": ["gradient", "colourModel", "cloudLayers", "cloudAlpha",
                     "stormCloudAlpha"],
        "properties": {
            "gradient": {"type": "array", "minItems": 2, "items": obj(
                ["sunElevationDeg", "zenith", "horizon"],
                {"sunElevationDeg": {"type": "number", "minimum": -90, "maximum": 90},
                 "zenith": RGB, "horizon": RGB})},
            # `HOUSE-01641`: the two axes deliberately not expanded into the gradient.  These
            # parameters are data because `SkySystem` will consume them; leaving them as constants
            # in the offline generator would let its 4096-cell proof disagree with the frame.
            "colourModel": obj(
                ["cloudCoverSamples", "azimuthOffsetSamples", "overcastGrey", "sunGlowColor",
                 "sunGlowStrength", "sunGlowExponent", "lightPollutionColor",
                 "lightPollutionStrength", "townAzimuthDeg",
                 "lightPollutionAzimuthExponent", "lightPollutionAltitudeExponent",
                 "lightPollutionStarMagnitudeLoss"],
                {"cloudCoverSamples": {"const": 8},
                 "azimuthOffsetSamples": {"const": 16},
                 "overcastGrey": RGB,
                 "sunGlowColor": RGB,
                 "sunGlowStrength": UNIT,
                 "sunGlowExponent": {"type": "number", "exclusiveMinimum": 0},
                 # `HOUSE-01614`: one generated low-horizon lobe is shared by the warm dome
                 # gradient and the directional loss of faint stars. Keep its bounds identical
                 # to SkyColourModelReader so authored data cannot pass here and fail at runtime.
                 "lightPollutionColor": RGB,
                 "lightPollutionStrength": UNIT,
                 "townAzimuthDeg": {"type": "number", "minimum": 0,
                                     "exclusiveMaximum": 360},
                 "lightPollutionAzimuthExponent": {"type": "number",
                                                     "exclusiveMinimum": 0},
                 "lightPollutionAltitudeExponent": {"type": "number",
                                                      "exclusiveMinimum": 0},
                 "lightPollutionStarMagnitudeLoss": {"type": "number", "minimum": 0,
                                                       "maximum": 5.5}}),
            "cloudLayers": {"type": "array", "items": obj(
                ["id", "texture", "altitude"],
                {"id": ID, "texture": STR, "altitude": NUM,
                 "scrollScale": NUM, "opacity": UNIT})},
            # §31.3's per-layer alphas by sky state, and §32/§33's sun and moon disc colours by
            # elevation. The file had the dome gradient and the layers' textures and nowhere to put
            # either (`HOUSE-00394`).
            "cloudAlpha": {"type": "array", "items": obj(
                ["cloudCover", "high", "mid", "low"],
                {"cloudCover": INTERVAL, "high": UNIT, "mid": UNIT, "low": UNIT,
                 "midTint": {"anyOf": [STR, {"type": "null"}]}})},
            "stormCloudAlpha": obj(["high", "mid", "low"],
                                   {"high": UNIT, "mid": UNIT, "low": UNIT}),
            "sun": {"type": "array", "minItems": 2, "items": obj(
                ["elevationDeg", "color"],
                {"elevationDeg": {"type": "number", "minimum": -90, "maximum": 90},
                 "color": RGB, "intensity": NUM})},
            "moon": {"type": "array", "minItems": 2, "items": obj(
                ["elevationDeg", "color"],
                {"elevationDeg": {"type": "number", "minimum": -90, "maximum": 90},
                 "color": RGB, "intensity": NUM})},
            "stars": obj(["catalogue"], {
                "catalogue": STR, "count": {"type": "integer", "minimum": 0},
                "magnitudeLimit": NUM})}})

    schemas["interactables"] = envelope("interactables", "interactables.json", rows(
        "interactables", obj(
            ["id", "kind", "cell", "actions"],
            {"id": ID, "kind": STR, "cell": ID, "prop": ID_OR_NULL,
             "focus": obj(["point"], {"point": VEC3, "normal": VEC3,
                                      "radius": {"type": "number", "minimum": 0}}),
             "bounds": obj(["min", "max"], {"min": VEC3, "max": VEC3}),
             "actions": {"type": "array", "items": obj(
                 ["verb", "do"],
                 {"verb": STR, "when": {"anyOf": [STR, {"type": "null"}]}, "do": STR,
                  "sound": ID_OR_NULL, "anim": {"anyOf": [STR, {"type": "null"}]},
                  "duration": {"type": "number", "minimum": 0}})},
             "childInteractables": {"type": "array", "items": ID},
             "state": {"type": "object"},
             "persist": {"type": "array", "items": STR},
             "audio": obj([], {"loop": ID_OR_NULL, "emitter": VEC3}),
             "portal": ID_OR_NULL})))

    schemas["initialstate"] = envelope("initialstate", "initialstate.json", {
        "required": ["player", "clock"],
        "properties": {
            "player": obj(["cell", "position"], {"cell": ID, "position": VEC3, "yawDeg": NUM}),
            "clock": obj(["epochSeconds", "timeScale"], {
                "epochSeconds": NUM, "timeScale": {"type": "number", "minimum": 0},
                "latitudeDeg": {"type": "number", "minimum": -90, "maximum": 90},
                "longitudeDeg": {"type": "number", "minimum": -180, "maximum": 180},
                "utcOffsetMinutes": {"type": "integer"}}),
            "weather": obj([], {
                "target": ID,
                "cloudCover": UNIT,
                "cloudCumuliform": UNIT,
                "precipType": {"enum": ["None", "Rain", "Snow", "Sleet", "Hail"]},
                "precipIntensity": UNIT,
                "windSpeed": {"type": "number", "minimum": 0, "maximum": 30},
                "windDirectionDeg": {"type": "number", "minimum": 0, "maximum": 360},
                "gustFactor": UNIT,
                "fogDensity": UNIT,
                "thunderIntensity": UNIT,
                "temperatureC": {"type": "number", "minimum": -18, "maximum": 38},
                "humidity": UNIT,
                "surfaceWetness": UNIT,
                "snowDepth": {"type": "number", "minimum": 0, "maximum": 0.35},
                "targetExpiryMinutes": {"type": "number", "minimum": 0, "maximum": 380},
                "rngState": {"type": "string", "pattern": r"^0x[0-9A-Fa-f]{16}$"}}),
            "interactables": {"type": "object", "additionalProperties": {"type": "object"}},
            "pets": {"type": "object", "additionalProperties": {"type": "object"}}}})

    missing = [k for k in WORLD_KINDS if k not in schemas]
    if missing:
        raise SystemExit(f"world_schema: no schema built for {', '.join(missing)}")
    return schemas


def path_for(kind: str) -> Path:
    return SCHEMA_DIR / f"{kind}.schema.json"


def rendered(schema: dict) -> str:
    return json.dumps(schema, indent=2, sort_keys=True) + "\n"


def emit(*, dry_run: bool = False) -> list[str]:
    """Write every schema. Returns the ones that were stale."""
    stale = []
    for kind, schema in build().items():
        path = path_for(kind)
        text = rendered(schema)
        if path.is_file() and path.read_text(encoding="utf-8") == text:
            continue
        stale.append(path.relative_to(REPO).as_posix())
        if not dry_run:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")
    return stale


# ===================================================================================== validation


def validate_document(kind: str, document: dict) -> list[str]:
    """Every shape problem in one parsed file, each naming the field that carries it.

    Every problem, not the first: `conventions.md` §5.1 -- fixing forty authoring mistakes one
    build at a time is intolerable.
    """
    import jsonschema

    validator = jsonschema.Draft202012Validator(build()[kind])
    problems = []
    for error in sorted(validator.iter_errors(document), key=lambda e: list(e.absolute_path)):
        where = "/".join(str(part) for part in error.absolute_path) or "(root)"
        problems.append(f"{where}: {error.message}")
    return problems


def validate_directory(directory: Path) -> dict[str, list[str]]:
    """`file -> problems`, for every world file present. A file that is absent is not a problem
    here: which files must exist is `world.manifest.json`'s business and rule 6's."""
    out: dict[str, list[str]] = {}
    for kind in WORLD_KINDS:
        name, _ = layout_io.FILES[kind]
        path = directory / name
        if not path.is_file():
            continue
        try:
            document = layout_io.load_file(path, kind)
        except layout_io.LayoutError as exc:
            out[name] = [str(exc)]
            continue
        problems = validate_document(kind, document)
        if problems:
            out[name] = problems
    return out


# ======================================================================================= selftest


def selftest() -> int:
    import jsonschema

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("world_schema: selftest")
    schemas = build()

    # 1. Sixteen files, and the sixteen are the world's -- not seventeen.
    require(len(WORLD_KINDS) == 16,
            f"there are sixteen world files ({len(WORLD_KINDS)}: {WORLD_KINDS})")
    require("assets" not in WORLD_KINDS,
            "and assets.manifest.json is not one of them -- it is the ASSET manifest, whose "
            "schema is §20.3 and whose gate is check_manifest.py")
    require(set(schemas) == set(WORLD_KINDS), "every one of them has a schema")

    # 2. Every schema is itself a valid JSON Schema. A schema with a typo in a keyword silently
    #    accepts everything, which is worse than having no schema at all.
    for kind, schema in schemas.items():
        try:
            jsonschema.Draft202012Validator.check_schema(schema)
            ok = True
            detail = ""
        except jsonschema.SchemaError as exc:
            ok, detail = False, str(exc)[:80]
        require(ok, f"{kind}.schema.json is a valid draft 2020-12 schema{' — ' + detail if detail else ''}")

    # 3. The shared vocabulary is shared. Written out sixteen times it drifts, and a schema that
    #    accepts a lower-case id in one file and not another looks like sixteen correct files.
    require(all(s["$defs"] == defs() for s in schemas.values()),
            "all sixteen carry the identical $defs block")
    require(all(s["$defs"]["id"]["pattern"] == ID_PATTERN for s in schemas.values()),
            f"including one id pattern, {ID_PATTERN}, which is world-format.md's")

    # 4. The header rule. Every file carries `cna-house/<kind>/<version>` and a file claiming to be
    #    another kind must be refused -- `layout_io.load_file` checks it too, and this is the
    #    machine-checkable half of the same rule.
    for kind in WORLD_KINDS:
        good = {"schema": f"cna-house/{kind}/1"}
        wrong = {"schema": "cna-house/somethingelse/1"}
        require(not any("schema" in p for p in validate_document(kind, good)),
                f"{kind} accepts its own header")
        require(any("schema" in p for p in validate_document(kind, wrong)),
                f"{kind} refuses another kind's header")
        require(any("schema" in p for p in validate_document(kind, {})),
                f"{kind} refuses a file with no header at all")

    # 5. Shape errors are caught, and each names the field. A cells file is the worked example
    #    because it is the one every tool reads.
    cells = {"schema": "cna-house/cells/1", "cells": [{
        "id": "L0_KITCHEN", "level": "L0", "kind": "room",
        "boxes": [{"x": [-8.2, 2.2], "z": [-27.1, -23.0]}]}]}
    require(validate_document("cells", cells) == [],
            f"a minimal valid cell passes ({validate_document('cells', cells)})")

    bad_id = json.loads(json.dumps(cells))
    bad_id["cells"][0]["id"] = "l0_kitchen"
    problems = validate_document("cells", bad_id)
    require(problems and "cells/0/id" in problems[0],
            f"a lower-case id is refused, and the message names the field ({problems[:1]})")

    bad_kind = json.loads(json.dumps(cells))
    bad_kind["cells"][0]["kind"] = "conservatory"
    require(any("kind" in p for p in validate_document("cells", bad_kind)),
            "a cell kind outside the closed list is refused")

    no_boxes = json.loads(json.dumps(cells))
    no_boxes["cells"][0]["boxes"] = []
    require(validate_document("cells", no_boxes),
            "a cell with no boxes is refused -- every cell has a footprint")

    extra = json.loads(json.dumps(cells))
    extra["cells"][0]["colour"] = "blue"
    require(validate_document("cells", extra),
            "an unknown field is refused: silent extra data is how two sources of truth begin")

    # 6. EVERY problem is reported, not the first. conventions.md §5.1.
    many = {"schema": "cna-house/cells/1", "cells": [
        {"id": "bad1", "level": "L0", "kind": "room", "boxes": [{"x": [0, 1], "z": [0, 1]}]},
        {"id": "bad2", "level": "L0", "kind": "room", "boxes": [{"x": [0, 1], "z": [0, 1]}]},
        {"id": "bad3", "level": "L0", "kind": "nope", "boxes": [{"x": [0, 1], "z": [0, 1]}]}]}
    problems = validate_document("cells", many)
    require(len(problems) >= 3,
            f"three broken rows produce at least three problems ({len(problems)}) -- fixing forty "
            f"authoring mistakes one build at a time is intolerable")

    # 7. What these schemas deliberately do NOT check. A schema cannot see across files, and one
    #    that tried would be a second, weaker copy of validate_world.py.
    dangling = {"schema": "cna-house/portals/1", "portals": [{
        "id": "P_A__B", "cellA": "NO_SUCH_CELL", "cellB": "ALSO_MISSING",
        "plane": {"axis": "x", "value": 0.0}, "rect": {"u": [0, 1], "v": [0, 2]},
        "kind": "door"}]}
    require(validate_document("portals", dangling) == [],
            "a portal naming two cells that do not exist passes the SHAPE check -- rule 6 is "
            "validate_world.py's (HOUSE-00358), and it needs the whole layout at once")
    inverted = {"schema": "cna-house/cells/1", "cells": [{
        "id": "C", "level": "L0", "kind": "room",
        "boxes": [{"x": [5.0, 1.0], "z": [0.0, 1.0]}]}]}
    require(validate_document("cells", inverted) == [],
            "and a box with min > max passes it too -- JSON Schema cannot compare two members of "
            "an array, so that is rule 2's")

    # 8. Ranges are checked where they exist, because a number out of range is a shape error.
    hot = {"schema": "cna-house/lights/1", "lights": [{
        "id": "L", "cell": "C", "group": "G", "type": "point", "position": [0, 0, 0],
        "colorK": 250000}]}
    require(validate_document("lights", hot),
            "a colour temperature of 250 000 K is refused")
    steep = {"schema": "cna-house/materials/1", "materials": [{
        "id": "M", "class": "tile", "snowResponse": {"slopeLimitDeg": 400}}]}
    require(validate_document("materials", steep),
            "and a snow slope limit of 400 degrees is too")
    loud = {"schema": "cna-house/materials/1", "materials": [{
        "id": "M", "class": "tile", "audioAbsorption": 1.5}]}
    require(validate_document("materials", loud),
            "a 0..1 fraction given as 1.5 is refused")
    flat = {"schema": "cna-house/lights/1", "lights": [{
        "id": "L", "cell": "C", "group": "G", "type": "point", "position": [0.0, 0.0]}]}
    require(validate_document("lights", flat),
            "and a position of two numbers is refused -- vectors are [x, y, z]")

    sky = layout_io.load_file(REPO / "assets-src" / "world" / "layout.sky.json", "sky")
    require(validate_document("sky", sky) == [],
            "the generated town-glow model crosses the authored sky/schema boundary")
    bad_pollution = json.loads(json.dumps(sky))
    bad_pollution["colourModel"]["townAzimuthDeg"] = 360.0
    require(any("townAzimuthDeg" in problem
                for problem in validate_document("sky", bad_pollution)),
            "a town azimuth outside the runtime's half-open compass range is refused")
    bad_pollution["colourModel"]["townAzimuthDeg"] = 180.0
    bad_pollution["colourModel"]["lightPollutionStarMagnitudeLoss"] = 6.0
    require(any("lightPollutionStarMagnitudeLoss" in problem
                for problem in validate_document("sky", bad_pollution)),
            "the light-pollution star loss cannot exceed the catalogue's magnitude span")

    # 9. The emitted files are current, and round trip.
    stale = emit(dry_run=True)
    require(not stale,
            f"the emitted docs/world-schema/ files are current -- they are what a person and an "
            f"editor read, and a stale one is worse than none ({stale[:2]})")
    require(all(rendered(schemas[k]) == rendered(build()[k]) for k in WORLD_KINDS),
            "two builds render byte-identically -- --check is a gate, and a gate that depends on "
            "dict ordering fails on somebody else's machine and not on mine")
    for kind in WORLD_KINDS:
        require(rendered(schemas[kind]) == rendered(json.loads(rendered(schemas[kind]))),
                f"{kind}'s schema is stable through a JSON round trip")

    # 10. A whole directory validates, and `layout_io`'s JSONC comments survive the trip.
    import shutil
    import tempfile

    workspace = Path(tempfile.mkdtemp(prefix="world_schema_selftest_"))
    try:
        (workspace / "layout.cells.json").write_text(
            '{\n  // a comment, which world-format.md permits\n'
            '  "schema": "cna-house/cells/1",\n'
            '  "cells": [{"id": "L0_A", "level": "L0", "kind": "room",\n'
            '             "boxes": [{"x": [0, 1], "z": [0, 1]}]}]\n}\n', encoding="utf-8")
        require(validate_directory(workspace) == {},
                "a directory of one valid JSONC file reports nothing")
        (workspace / "layout.portals.json").write_text(
            '{"schema": "cna-house/portals/1", "portals": [{"id": "bad"}]}\n', encoding="utf-8")
        found = validate_directory(workspace)
        require(list(found) == ["layout.portals.json"],
                f"and one broken file is reported by NAME, not the whole directory ({list(found)})")
    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("world_schema: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--emit", action="store_true", help="write docs/world-schema/")
    parser.add_argument("--check", action="store_true", help="fail if those files are stale")
    parser.add_argument("--validate", type=Path, metavar="DIR",
                        help="validate a world directory against the schemas")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    if args.validate:
        found = validate_directory(args.validate)
        for name, problems in sorted(found.items()):
            print(f"{name}: {len(problems)} problem(s)", file=sys.stderr)
            for problem in problems[:20]:
                print(f"  {problem}", file=sys.stderr)
        if found:
            return 1
        print(f"world_schema: every world file in {args.validate} matches its schema.")
        return 0

    if args.check:
        stale = emit(dry_run=True)
        if stale:
            print(f"world_schema: {len(stale)} schema(s) stale; run "
                  f"tools/world/world_schema.py --emit", file=sys.stderr)
            for path in stale[:5]:
                print(f"  {path}", file=sys.stderr)
            return 1
        print(f"world_schema: all {len(WORLD_KINDS)} schemas are current.")
        return 0

    written = emit()
    print(f"world_schema: {len(WORLD_KINDS)} schema(s) in "
          f"{SCHEMA_DIR.relative_to(REPO)}, {len(written)} written.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
