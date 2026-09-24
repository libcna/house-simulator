#!/usr/bin/env python3
"""scale_check.py -- assert an asset's real-world size against its category.

`HOUSE-00187`, enforcing the table in `cna-house.md` §70.5. The house is the point of this project,
and a house whose door is 1.7 m tall reads as wrong before anyone can say why. Scale errors are also
the single most common defect in downloaded assets: a model authored in centimetres imports a
hundred times too large, and a model authored in inches imports 2.54 times too small -- both of
which look plausible in isolation and impossible next to a chair.

The category comes from the manifest row (`category`), so the check is driven by the same record
that carries the licence, and an asset without a row is caught by `check_manifest.py` first.

    tools/assets/scale_check.py                      # every model with a manifest row
    tools/assets/scale_check.py PATH ...
    tools/assets/scale_check.py --list-categories
    tools/assets/scale_check.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gltf_validate  # noqa: E402  (shared .glb reader; one parser, not two)
import manifest as manifest_tool  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

#: `cna-house.md` §70.5, transcribed. Each entry is (axis, low, high, what the number means).
#:
#: The AXIS matters and is not always height: a door leaf is checked on both its height and its
#: width, and checking a bounding box's largest dimension instead would pass a door lying on its
#: side. Axes are glTF's: +Y up, -Z forward (`HOUSE-00070`).
CATEGORIES: dict[str, list[tuple[str, float, float, str]]] = {
    "door-interior": [
        ("y", 1.98, 2.10, "interior door leaf height"),
        ("x", 0.76, 0.95, "interior door leaf width"),
    ],
    "counter-kitchen": [("y", 0.88, 0.95, "kitchen counter height")],
    # A combined base run plus faucet exceeds counter height by design. Keep the established
    # counter band on a measured manifest property, and constrain the assembly's full AABB too.
    "kitchen-sink-run": [("x", 2.70, 2.95, "kitchen sink run width"),
                         ("z", 0.72, 0.95, "kitchen sink run depth including tap"),
                         ("y", 0.88, 0.95, "kitchen counter height under faucet")],
    # The butler's west window starts only 0.90 m above its floor. Its fitted
    # three-bay run is narrower and shallower than the main kitchen sink wall;
    # the separately measured stone worktop must clear that low sill.
    "kitchen-service-run": [("x", 1.50, 1.70, "butler service run width"),
                            ("z", 0.60, 0.72, "butler service run depth including pulls"),
                            ("y", 0.88, 0.90, "under-window service counter height")],
    # The fitted cooking wall includes 2.65 m upper cabinets and hood. Its working top is a
    # separately measured manifest property, exactly as for the sink/faucet assembly.
    "kitchen-cooking-wall": [("x", 0.95, 1.05, "fitted cooking bay width"),
                             ("z", 0.65, 0.80, "cooking wall depth including pulls"),
                             ("y", 0.88, 0.95, "cooking wall counter height")],
    # A large refrigerator plus an overhead bridge reaches the L0 ceiling. The appliance
    # body height is separately measured, as a combined AABB cannot identify its top.
    "appliance-refrigerator": [("x", 1.60, 1.90, "large refrigerator width"),
                               ("z", 0.70, 0.90, "depth including door pulls"),
                               ("y", 1.80, 2.10, "appliance body height under bridge")],
    # HOUSE-00975 keeps three appliance bodies rather than a broad white-goods catalogue. The
    # bands follow their standard domestic nominal sizes and still catch centimetre/inch imports.
    "appliance-laundry": [("x", 0.58, 0.70, "600 mm laundry appliance width"),
                          ("y", 0.80, 0.92, "undercounter laundry appliance height"),
                          ("z", 0.62, 0.78, "laundry appliance depth including door")],
    "appliance-chest-freezer": [("x", 1.00, 1.25, "domestic chest-freezer width"),
                                ("y", 0.78, 0.92, "domestic chest-freezer height"),
                                ("z", 0.72, 0.90, "domestic chest-freezer depth")],
    "appliance-boiler": [("x", 0.36, 0.55, "wall boiler width"),
                         ("y", 0.90, 1.25, "wall boiler body height"),
                         ("z", 0.25, 0.45, "wall boiler projection")],
    "upright-piano": [("x", 1.35, 1.60, "domestic upright piano width"),
                      ("y", 1.10, 1.35, "domestic upright piano height"),
                      ("z", 0.50, 0.76, "domestic upright piano depth including pedals")],
    "cabinet-upper": [("y", 1.40, 1.55, "upper cabinet underside")],
    "table-dining": [("y", 0.72, 0.78, "dining table top")],
    "table-setting": [("x", 2.10, 2.30, "eight-place setting span"),
                      ("z", 0.75, 0.92, "setting depth inside the dining top"),
                      ("y", 0.06, 0.12, "low serving vessel height")],
    "dining-sideboard": [("x", 1.50, 1.70, "formal sideboard width"),
                          ("y", 0.85, 1.05, "cabinet and restrained table dressing height"),
                          ("z", 0.44, 0.52, "shallow dining sideboard depth")],
    "table-lamp": [("x", 0.32, 0.44, "table lamp shade diameter"),
                    ("y", 0.60, 0.72, "domestic table lamp height"),
                    ("z", 0.32, 0.44, "table lamp shade diameter")],
    # HOUSE-00981 fills only the two fixture shapes the reusable ground-floor catalogue lacks.
    # These broad but physical bands reject unit mistakes without forcing one decorative design.
    "desk-lamp": [("x", 0.15, 0.35, "desk lamp footprint width"),
                  ("y", 0.35, 0.75, "desk lamp height"),
                  ("z", 0.15, 0.45, "desk lamp footprint depth")],
    "ceiling-light": [("x", 0.15, 2.50, "utility ceiling fitting length"),
                      ("y", 0.05, 0.60, "utility ceiling fitting drop"),
                      ("z", 0.10, 0.80, "utility ceiling fitting width")],
    "wall-clock": [("x", 0.20, 0.60, "domestic wall-clock width"),
                   ("y", 0.20, 0.60, "domestic wall-clock height"),
                   ("z", 0.03, 0.20, "wall-clock projection")],
    # HOUSE-00984's five bounded clutter shapes. These broad physical bands reject unit mistakes
    # while allowing reuse in utility rooms, closets, garage and shed.
    "waste-bin": [("x", 0.20, 0.60, "domestic waste-bin width"),
                  ("y", 0.25, 1.00, "domestic waste-bin height"),
                  ("z", 0.20, 0.60, "domestic waste-bin depth")],
    "suitcase": [("x", 0.40, 0.85, "standing suitcase width"),
                 ("y", 0.45, 0.95, "standing suitcase height"),
                 ("z", 0.18, 0.40, "standing suitcase depth")],
    "toolbox": [("x", 0.35, 0.75, "portable toolbox width"),
                ("y", 0.20, 0.60, "portable toolbox height"),
                ("z", 0.20, 0.45, "portable toolbox depth")],
    "garden-tool-set": [("x", 0.40, 1.20, "garden-tool set width"),
                        ("y", 1.00, 2.00, "long-handled garden-tool height"),
                        ("z", 0.25, 0.80, "garden-tool stand depth")],
    "paint-tin": [("x", 0.15, 0.35, "domestic paint-tin width"),
                  ("y", 0.20, 0.50, "domestic paint-tin height"),
                  ("z", 0.15, 0.35, "domestic paint-tin depth")],
    "desk": [("x", 1.00, 1.80, "domestic desk width"),
             ("y", 0.72, 0.78, "desk top"),
             ("z", 0.55, 0.90, "domestic desk depth")],
    "table-pool": [("x", 2.20, 3.00, "domestic pool-table length"),
                   ("y", 0.75, 0.85, "pool-table cushion top"),
                   ("z", 1.20, 1.70, "domestic pool-table width")],
    "cinema-seat-row": [("x", 2.80, 3.30, "four-seat cinema-row width"),
                        ("y", 0.95, 1.15, "tiered cinema-row height"),
                        ("z", 0.95, 1.25, "tiered cinema-row depth")],
    "chair": [("y", 0.42, 0.48, "chair seat")],
    "stool-counter": [("x", 0.38, 0.58, "counter stool width"),
                      ("y", 0.62, 0.70, "counter stool seat"),
                      ("z", 0.38, 0.58, "counter stool depth")],
    "sofa": [("y", 0.38, 0.45, "sofa seat")],
    "bed": [("y", 0.48, 0.62, "bed mattress top")],
    "wc": [("x", 0.35, 0.55, "domestic WC width"),
           ("y", 0.38, 0.45, "WC seat"),
           ("z", 0.52, 0.80, "domestic WC depth")],
    "basin": [("y", 0.80, 0.90, "basin rim")],
    "bath": [("x", 1.50, 1.90, "domestic bath length"),
             ("y", 0.50, 0.60, "bath rim"),
             ("z", 0.65, 0.95, "domestic bath width")],
    "shower-enclosure": [("x", 0.80, 1.25, "shower tray width"),
                         ("y", 1.80, 2.15, "shower enclosure height"),
                         ("z", 0.75, 1.10, "shower tray depth")],
    "handrail": [("y", 0.85, 0.95, "handrail height")],
    "avatar": [("y", 1.55, 1.90, "human avatar height")],
    "dog": [("y", 0.50, 0.70, "dog withers height")],
    "cat": [("y", 0.20, 0.32, "cat shoulder height")],
    # The one row of §70.5 this table did not transcribe (`HOUSE-00360`). A car is the only entry
    # in the section with THREE bounds, and it needs all three: a hatchback and an estate differ
    # by half a metre of length and by nothing else, so a height-only check would pass a model
    # scaled to a van. Length is -Z (glTF forward), which is why the depth bound is the long one.
    "car": [("z", 4.2, 5.2, "car length"),
            ("x", 1.7, 2.0, "car width"),
            ("y", 1.4, 1.9, "car height")],
    "delivery-van": [("z", 4.8, 7.5, "delivery van length"),
                     ("x", 1.8, 2.6, "delivery van width"),
                     ("y", 1.9, 3.0, "delivery van height")],
    # HOUSE-00297's age bands are deliberately disjoint. A mature tree accidentally registered
    # as a sapling is a layout collision bug as well as an art bug, and the three rows make that
    # mistake measurable instead of relying on the filename.
    "tree-sapling": [("y", 3.40, 3.60, "tree sapling height")],
    "tree-young": [("y", 6.85, 7.15, "young tree height")],
    "tree-mature": [("y", 11.75, 12.25, "mature tree height")],
    "shrub": [("y", 0.45, 1.85, "shrub / bush height")],
    "flower": [("y", 0.20, 0.50, "flower clump height")],
    "grass-card": [("y", 0.20, 0.60, "grass-card height")],
    # Countertop groupings span a kettle, canister trio and composed board/bowl/produce set.
    # The deliberately broad three-axis band still catches centimetre/inch imports while the
    # deterministic generator and manifest retain each exact measured AABB.
    "kitchen-counter-decor": [("x", 0.10, 1.00, "countertop grouping width"),
                              ("y", 0.10, 0.55, "countertop grouping height"),
                              ("z", 0.10, 0.60, "countertop grouping depth")],
    # A pipeline fixture is not a real-world object, but it is still geometry with a size, and the
    # bug this table exists to catch -- a model authored in centimetres -- is exactly as possible
    # here as anywhere. The band is wide because the size is chosen to be legible in a screenshot;
    # it is a band rather than nothing because 80 would be as wrong as 0.008.
    "fixture": [("x", 0.05, 3.0, "content-pipeline fixture, legible at a few metres"),
                ("y", 0.05, 3.0, "content-pipeline fixture, legible at a few metres"),
                ("z", 0.05, 3.0, "content-pipeline fixture, legible at a few metres")],
    # HOUSE-01284's flush task puck is deliberately much thinner than a pendant or wall pack.
    # Keep all three axes bounded so a centimetre/inch import still fails without forcing a
    # physically implausible 50 mm body merely to reuse the broad fixture band.
    "fixture-small": [("x", 0.04, 0.25, "small integrated fixture width"),
                      ("y", 0.005, 0.08, "small integrated fixture thickness"),
                      ("z", 0.04, 0.25, "small integrated fixture depth")],
    # Wall-mounted fixtures pivot on the wall plane rather than a floor support point. The band
    # remains deliberately broad across sconces and picture lights while still rejecting a
    # centimetre/inch import or a fixture large enough to dominate a domestic wall bay.
    "wall-light": [("x", 0.10, 1.50, "domestic wall-light width"),
                   ("y", 0.05, 1.50, "domestic wall-light height"),
                   ("z", 0.05, 0.75, "domestic wall-light projection")],
    # A complete floor-supported surround includes the over-mantel composition. All three axes
    # are constrained because a plausible isolated fireplace is especially sensitive to an
    # inch/centimetre import even though §70.5 did not originally name this furniture category.
    "fireplace-surround": [("x", 1.40, 2.20, "domestic fireplace surround width"),
                           ("y", 1.80, 2.80, "surround plus over-mantel composition height"),
                           ("z", 0.30, 0.80, "hearth projection")],
    "throw-blanket": [("x", 0.30, 1.20, "folded throw width"),
                      ("y", 0.30, 1.20, "draped throw height"),
                      ("z", 0.30, 1.50, "draped throw depth")],
    # Full-height paired dressing around a domestic picture window. All three axes matter: a
    # centimetre import can otherwise remain centred on the correct wall and merely look absent.
    "window-treatment": [("x", 2.50, 3.50, "picture-window rod and curtain width"),
                         ("y", 2.20, 2.80, "full-length curtain height"),
                         ("z", 0.05, 0.30, "rod and gathered-fabric wall projection")],
    "window-blind": [("x", 0.55, 2.50, "domestic blind width"),
                     ("y", 0.55, 2.50, "domestic blind drop"),
                     ("z", 0.03, 0.25, "slat and head-rail wall projection")],
    # HOUSE-00771's grouped exterior pieces are checked on their full placed silhouettes.  The
    # dining row is a table plus four chairs, while every other row is one reusable object.
    "garden-dining-set": [("x", 2.40, 3.20, "four-place outdoor dining-group width"),
                          ("y", 0.70, 1.10, "outdoor dining-group height"),
                          ("z", 2.40, 3.20, "four-place outdoor dining-group depth")],
    # HOUSE-01068's breakfast composition is validated as one reusable group because all four
    # chairs have measured clearances to one fixed table. The fitted bar includes open shelves;
    # its full height, not merely the 0.94 m worktop, catches centimetre-scale imports.
    "breakfast-dining-group": [("x", 2.15, 2.80, "four-place breakfast-group width"),
                               ("y", 0.80, 1.10, "breakfast-group chair/back height"),
                               ("z", 2.15, 2.80, "four-place breakfast-group depth")],
    "sunroom-lounge-group": [("x", 1.25, 1.80, "two cane chairs and tea table width"),
                             ("y", 0.85, 1.10, "cane chair back height"),
                             ("z", 1.90, 2.50, "two-chair reading group depth")],
    "wet-bar": [("x", 2.10, 2.70, "fitted wet-bar width"),
                ("y", 2.10, 2.70, "wet bar plus dressed open shelves height"),
                ("z", 0.50, 0.85, "fitted wet-bar working depth")],
    "garden-lounger": [("x", 0.55, 0.90, "garden lounger width"),
                       ("y", 0.55, 1.10, "raised garden lounger height"),
                       ("z", 1.60, 2.30, "garden lounger length")],
    "garden-swing": [("x", 2.10, 2.80, "two-place garden swing width"),
                     ("y", 1.80, 2.40, "garden swing frame height"),
                     ("z", 0.90, 1.50, "garden swing frame depth")],
    "garden-firepit": [("x", 0.80, 1.50, "domestic garden fire-pit width"),
                       ("y", 0.25, 0.60, "domestic garden fire-pit height"),
                       ("z", 0.80, 1.50, "domestic garden fire-pit depth")],
    "birdbath": [("x", 0.45, 0.80, "garden birdbath basin width"),
                 ("y", 0.70, 1.20, "garden birdbath height"),
                 ("z", 0.45, 0.80, "garden birdbath basin depth")],
    "planter": [("x", 0.35, 0.80, "planted terrace pot width"),
                ("y", 0.50, 1.20, "planted terrace pot height"),
                ("z", 0.35, 0.80, "planted terrace pot depth")],
    "tabletop-decor": [("x", 0.40, 0.90, "composed tabletop vignette width"),
                       ("y", 0.08, 0.35, "tabletop vignette height"),
                       ("z", 0.20, 0.55, "composed tabletop vignette depth")],
    "console-decor": [("x", 0.60, 1.30, "composed console vignette width"),
                      ("y", 0.40, 0.95, "console vignette height"),
                      ("z", 0.15, 0.45, "composed console vignette depth")],
}

#: Categories that exist in the manifest but carry no size expectation. Listed explicitly so that a
#: NEW category is an error rather than a silent skip -- the failure this whole check exists to
#: prevent is a model nobody looked at, and "unknown category, so no check" is exactly that.
UNSIZED_CATEGORIES = {
    "fallback": "a deliberately ugly placeholder; its size is meaningless",
    "effect": "not geometry",
    "font": "not geometry",
    "audio": "not geometry",
    "video": "not geometry",
    "floor-lamp": "§70.5 states no lamp-height band; manifest bounds and room review own it",
    "floor-mirror": "§70.5 states no floor-mirror band; manifest bounds and room review own it",
    "houseplant": "§70.5 states no indoor-plant band; manifest bounds and room review own it",
    "vase": "§70.5 states no decorative-vase band; manifest bounds and room review own it",
    "media-console": "§70.5 states no media-furniture band; manifest bounds and room review own it",
    "occasional-table": "§70.5 states no coffee/side-table band; manifest bounds and room review own it",
    "bookcase": "§70.5 states no bookcase band; manifest bounds and room review own it",
    "pantry-shelf": "§70.5 states no pantry-shelf band; measured room clearance and manifest bounds own it",
    "fill-kit": "seeded shelf and surface clusters; the HOUSE-00973 group gate owns their measured envelopes",
    "storage-furniture": "the HOUSE-00985 data and group gate own the reusable carcass envelopes",
    "open-shelving": "the HOUSE-00985 data and group gate own the shelving envelope",
    "workbench": "the HOUSE-00985 data and group gate own the workbench envelope",
    "storage-box": "the HOUSE-00985 data and group gate own the reusable box envelope",
    "mirror": "the HOUSE-00985 data and wall-plane check own the mirror envelope",
    "towel-rail": "the HOUSE-00985 data and wall-plane check own the rail envelope",
    "static-screen": "the HOUSE-00985 data and wall-plane check own the cinema screen envelope",
    "projector": "the HOUSE-00985 data and wall-plane check own the static projector envelope",
    "service-run": "the HOUSE-00985 common run generator owns duct, pipe and cable envelopes",
    "pet-bed": "§70.5 states no pet-bed band; manifest bounds and linked nav bed own it",
    "picture": "§70.5 states no wall-art band; manifest bounds and wall-plane origin own it",
    "rug": "§70.5 states no rug-size band; manifest bounds and room review own it",
    "television": "§70.5 states no television-size band; manifest bounds and room review own it",
}


def accessor_bounds(document: dict) -> tuple[list[float], list[float]] | None:
    """The model's bounds, from the glTF accessors' own declared `min`/`max`.

    **Read from the accessors rather than from the vertex data.** glTF requires `min` and `max` on a
    POSITION accessor, so they are authoritative, already in metres, and available without decoding
    a buffer -- which is what keeps this tool free of a glTF library. A file that omits them is
    rejected rather than guessed at.
    """
    accessors = document.get("accessors", [])
    low = [float("inf")] * 3
    high = [float("-inf")] * 3
    found = False
    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            index = primitive.get("attributes", {}).get("POSITION")
            if index is None or index >= len(accessors):
                continue
            accessor = accessors[index]
            a_min, a_max = accessor.get("min"), accessor.get("max")
            if not (isinstance(a_min, list) and isinstance(a_max, list)):
                continue
            if len(a_min) < 3 or len(a_max) < 3:
                continue
            found = True
            for axis in range(3):
                low[axis] = min(low[axis], float(a_min[axis]))
                high[axis] = max(high[axis], float(a_max[axis]))
    return (low, high) if found else None


AXIS_INDEX = {"x": 0, "y": 1, "z": 2}


def check(path: Path, category: str, geometry: dict | None = None) -> list[str]:
    if category in UNSIZED_CATEGORIES:
        return []
    rules = CATEGORIES.get(category)
    if rules is None:
        return [
            f"category '{category}' has no entry in §70.5's table and is not listed as unsized. "
            f"Add it to CATEGORIES or to UNSIZED_CATEGORIES with a reason -- an unknown category "
            f"must not mean 'no check'."
        ]

    document, error = gltf_validate.read_gltf_json(path)
    if error is not None:
        return [error]
    assert document is not None

    bounds = accessor_bounds(document)
    if bounds is None:
        return [
            "no POSITION accessor declares min/max, which glTF requires. The size cannot be "
            "checked without decoding the buffers, and a guess is worse than a refusal."
        ]
    low, high = bounds
    size = [high[i] - low[i] for i in range(3)]

    problems: list[str] = []
    for axis, minimum, maximum, what in rules:
        value = size[AXIS_INDEX[axis]]
        # §70.5 constrains the SEAT of a chair/sofa, not the top of its back. The original checker
        # compared the whole Y bound and would reject every correctly proportioned armchair around
        # 0.84 m tall. Seat height cannot be recovered from an AABB, so it is an explicit measured
        # asset property in the same manifest geometry block that records the overall bounds.
        if category in {"chair", "stool-counter", "sofa", "wc"} and axis == "y":
            seat = (geometry or {}).get("seatHeightMetres")
            if not isinstance(seat, (int, float)):
                problems.append(
                    f"{what}: manifest geometry.seatHeightMetres is required; the model's "
                    "{:.3f} m overall height is not a seat measurement".format(size[1]))
                continue
            value = float(seat)
            if not math.isfinite(value) or value <= 0.0 or value > size[1]:
                problems.append(
                    f"{what}: measured seat {value:.3f} m must lie within the model's "
                    f"{size[1]:.3f} m height")
                continue
        # A bed's headboard and pillows likewise stand above the mattress. HOUSE-00979 records the
        # reviewed mattress plane explicitly instead of pretending the model AABB is that plane.
        if category == "bed" and axis == "y":
            mattress = (geometry or {}).get("mattressTopMetres")
            if not isinstance(mattress, (int, float)):
                problems.append(
                    f"{what}: manifest geometry.mattressTopMetres is required; the model's "
                    f"{size[1]:.3f} m overall height includes its headboard and pillows")
                continue
            value = float(mattress)
            if not math.isfinite(value) or value <= 0.0 or value > size[1]:
                problems.append(
                    f"{what}: measured mattress top {value:.3f} m must lie within the model's "
                    f"{size[1]:.3f} m height")
                continue
        if category in {"kitchen-sink-run", "kitchen-service-run", "kitchen-cooking-wall"} and axis == "y":
            counter = (geometry or {}).get("counterHeightMetres")
            if not isinstance(counter, (int, float)):
                problems.append(
                    f"{what}: manifest geometry.counterHeightMetres is required; the model's "
                    f"{size[1]:.3f} m overall height includes the tap")
                continue
            value = float(counter)
            if not math.isfinite(value) or value <= 0.0 or value > size[1]:
                problems.append(
                    f"{what}: measured counter {value:.3f} m must lie within the model's "
                    f"{size[1]:.3f} m overall height")
                continue
        if category == "appliance-refrigerator" and axis == "y":
            appliance = (geometry or {}).get("applianceHeightMetres")
            if not isinstance(appliance, (int, float)):
                problems.append(
                    f"{what}: manifest geometry.applianceHeightMetres is required; "
                    f"the {size[1]:.3f} m assembled AABB includes the bridge cabinet")
                continue
            value = float(appliance)
            if not math.isfinite(value) or value <= 0.0 or value > size[1]:
                problems.append(
                    f"{what}: measured appliance height {value:.3f} m must lie within "
                    f"the assembly's {size[1]:.3f} m height")
                continue
        if not (minimum <= value <= maximum):
            # The message names the RATIO, because that is what identifies the mistake: 100x is
            # centimetres, 2.54x is inches, 0.01x is a model authored in a scene scaled down.
            ratio = value / maximum if value > maximum else value / minimum
            problems.append(
                f"{what}: {value:.3f} m is outside {minimum:.2f}-{maximum:.2f} m "
                f"({ratio:.2f}x the nearest bound -- 100x is centimetres, 2.54x is inches)"
            )
    if category == "appliance-refrigerator" and not (2.60 <= size[1] <= 2.80):
        problems.append(
            f"integrated fridge/bridge bay height: {size[1]:.3f} m is outside "
            "2.60-2.80 m for L0's 2.70 m floor-to-ceiling clearance")
    return problems


def selftest() -> int:
    """A synthetic glTF at the right size, and the same one in centimetres."""
    import tempfile

    def make(width: float, height: float, depth: float) -> bytes:
        """A door-shaped box. NOT a cube: the first version of this fixture was a cube, and the
        width rule correctly rejected a 2.04 m one -- the checker was right and the fixture was
        wrong, which is exactly the confusion a selftest is supposed to remove rather than create.
        """
        payload = json.dumps(
            {
                "asset": {"version": "2.0"},
                "accessors": [
                    {
                        "type": "VEC3",
                        "componentType": 5126,
                        "count": 8,
                        "min": [-width / 2.0, 0.0, -depth / 2.0],
                        "max": [width / 2.0, height, depth / 2.0],
                    }
                ],
                "meshes": [{"name": "m", "primitives": [{"attributes": {"POSITION": 0}}]}],
            }
        ).encode("utf-8")
        payload += b" " * ((4 - len(payload) % 4) % 4)
        header = struct.pack("<III", gltf_validate.GLB_MAGIC, 2, 12 + 8 + len(payload))
        return header + struct.pack("<II", len(payload), gltf_validate.CHUNK_JSON) + payload

    failures = 0
    with tempfile.TemporaryDirectory(prefix="scale-selftest-", dir=str(REPO / "build")) as work:
        good = Path(work) / "door_ok.glb"
        good.write_bytes(make(0.85, 2.04, 0.05))
        if check(good, "door-interior"):
            print(f"  SELFTEST FAILED: a 2.04 m door was rejected: {check(good, 'door-interior')}",
                  file=sys.stderr)
            failures += 1
        else:
            print("  a 2.04 m door passes")

        # The classic: authored in centimetres, so a hundred times too large.
        centimetres = Path(work) / "door_cm.glb"
        centimetres.write_bytes(make(85.0, 204.0, 5.0))
        problems = check(centimetres, "door-interior")
        # Asserting on the MEASURED value, not on the hint text. The first version looked for
        # "100" and matched the constant advice string "100x is centimetres" rather than anything
        # the checker had computed -- a test that passes on its own error message.
        if not problems or not any("204.000 m" in p for p in problems):
            print(f"  SELFTEST FAILED: a 204 m door was not caught as centimetres: {problems}",
                  file=sys.stderr)
            failures += 1
        else:
            print(f"  caught centimetres: {problems[0]}")

        # And one just outside the tolerance, which is the case a loose check would miss.
        near = Path(work) / "door_near.glb"
        near.write_bytes(make(0.85, 1.90, 0.05))
        if not check(near, "door-interior"):
            print("  SELFTEST FAILED: a 1.90 m door was accepted", file=sys.stderr)
            failures += 1
        else:
            print("  a 1.90 m door is rejected, so the tolerance is a tolerance")

        # HOUSE-01040: a real tap raises the whole sink-run AABB to 1.25 m. The separately
        # measured working top stays in the original counter-height band, not a relaxed one.
        sink_run = Path(work) / "sink_run.glb"
        sink_run.write_bytes(make(2.86, 1.252, 0.738))
        if check(sink_run, "kitchen-sink-run", {"counterHeightMetres": 0.94}):
            print("  SELFTEST FAILED: measured 0.94 m sink counter was rejected",
                  file=sys.stderr)
            failures += 1
        if not check(sink_run, "kitchen-sink-run"):
            print("  SELFTEST FAILED: sink run without measured counter was accepted",
                  file=sys.stderr)
            failures += 1
        if not check(sink_run, "kitchen-sink-run", {"counterHeightMetres": 0.84}):
            print("  SELFTEST FAILED: 0.84 m sink counter was accepted", file=sys.stderr)
            failures += 1
        print("  sink run requires a measured in-band counter despite its 1.25 m tap")

        # HOUSE-01072: the window limits the smaller service run to an 0.90 m top.
        service = Path(work) / "service_run.glb"
        service.write_bytes(make(1.62, 1.036, 0.676))
        if check(service, "kitchen-service-run", {"counterHeightMetres": 0.893}):
            print("  SELFTEST FAILED: measured under-window service run was rejected",
                  file=sys.stderr)
            failures += 1
        if not check(service, "kitchen-service-run"):
            print("  SELFTEST FAILED: service run without measured counter was accepted",
                  file=sys.stderr)
            failures += 1
        if not check(service, "kitchen-service-run", {"counterHeightMetres": 0.94}):
            print("  SELFTEST FAILED: service counter above the window sill was accepted",
                  file=sys.stderr)
            failures += 1
        print("  service run requires a measured counter below its window")

        setting = Path(work) / "dining_setting.glb"
        setting.write_bytes(make(2.204, 0.0885, 0.834))
        if check(setting, "table-setting"):
            print("  SELFTEST FAILED: measured eight-place dressing was rejected",
                  file=sys.stderr)
            failures += 1
        oversized_setting = Path(work) / "dining_setting_oversized.glb"
        oversized_setting.write_bytes(make(2.50, 0.0885, 1.10))
        if not check(oversized_setting, "table-setting"):
            print("  SELFTEST FAILED: dressing beyond the tabletop was accepted",
                  file=sys.stderr)
            failures += 1
        print("  dining setting is measured inside its physical tabletop")

        # HOUSE-01044: the hood raises the compact assembly to 2.65 m, while the two stone
        # scribe tops remain at the same human-scale 0.94 m datum as the sink run.
        cooking = Path(work) / "cooking_wall.glb"
        cooking.write_bytes(make(1.00, 2.65, 0.738))
        if check(cooking, "kitchen-cooking-wall", {"counterHeightMetres": 0.94}):
            print("  SELFTEST FAILED: measured 1.00 m cooking bay was rejected",
                  file=sys.stderr)
            failures += 1
        if not check(cooking, "kitchen-cooking-wall"):
            print("  SELFTEST FAILED: cooking wall without measured counter was accepted",
                  file=sys.stderr)
            failures += 1
        if not check(cooking, "kitchen-cooking-wall", {"counterHeightMetres": 0.98}):
            print("  SELFTEST FAILED: 0.98 m cooking-wall counter was accepted",
                  file=sys.stderr)
            failures += 1
        print("  cooking-bay width, depth and separate counter height are measured")

        # HOUSE-01042: the bridge cupboard must not let the appliance body pass an
        # arbitrary 2.70 m height check, or leave its own ceiling band unverified.
        fridge = Path(work) / "fridge_bridge.glb"
        fridge.write_bytes(make(1.80, 2.70, 0.833))
        if check(fridge, "appliance-refrigerator", {"applianceHeightMetres": 1.95}):
            print("  SELFTEST FAILED: measured large fridge/bridge was rejected",
                  file=sys.stderr)
            failures += 1
        if not check(fridge, "appliance-refrigerator"):
            print("  SELFTEST FAILED: fridge without measured body height was accepted",
                  file=sys.stderr)
            failures += 1
        if not check(fridge, "appliance-refrigerator", {"applianceHeightMetres": 2.30}):
            print("  SELFTEST FAILED: oversized fridge body was accepted", file=sys.stderr)
            failures += 1
        too_tall = Path(work) / "fridge_tall_bridge.glb"
        too_tall.write_bytes(make(1.80, 3.10, 0.833))
        if not check(too_tall, "appliance-refrigerator", {"applianceHeightMetres": 1.95}):
            print("  SELFTEST FAILED: bridge above L0 ceiling was accepted", file=sys.stderr)
            failures += 1
        print("  fridge body and integrated bay heights are independently measured")

        # A car is checked on all three axes, and the axis is what makes the check mean anything:
        # a model 1.8 m long and 4.6 m wide is a car turned sideways, which a "largest dimension"
        # check would wave through (`HOUSE-00360`).
        car = Path(work) / "car_ok.glb"
        car.write_bytes(make(1.85, 1.55, 4.60))
        if check(car, "car"):
            print(f"  SELFTEST FAILED: a 4.60 x 1.85 x 1.55 m car was rejected: "
                  f"{check(car, 'car')}", file=sys.stderr)
            failures += 1
        else:
            print("  a 4.60 x 1.85 x 1.55 m car passes")

        sideways = Path(work) / "car_sideways.glb"
        sideways.write_bytes(make(4.60, 1.55, 1.85))
        problems = check(sideways, "car")
        if not any("car length" in p for p in problems) or not any("car width" in p
                                                                   for p in problems):
            print(f"  SELFTEST FAILED: a car lying across its own axes passed: {problems}",
                  file=sys.stderr)
            failures += 1
        else:
            print("  and the same car rotated 90 degrees fails on length AND width")

        chair = Path(work) / "chair_ok.glb"
        chair.write_bytes(make(0.80, 0.84, 0.85))
        if check(chair, "chair", {"seatHeightMetres": 0.45}):
            print("  SELFTEST FAILED: a 0.45 m seat in a 0.84 m chair was rejected",
                  file=sys.stderr)
            failures += 1
        else:
            print("  a chair checks its 0.45 m seat, not its 0.84 m back")
        problems = check(chair, "chair")
        if not any("seatHeightMetres" in problem for problem in problems):
            print("  SELFTEST FAILED: a chair without a measured seat height passed",
                  file=sys.stderr)
            failures += 1
        else:
            print("  and a chair without a measured seat height is refused")
        if not check(chair, "chair", {"seatHeightMetres": 1.0}):
            print("  SELFTEST FAILED: a seat above its chair back passed", file=sys.stderr)
            failures += 1
        else:
            print("  and a seat above its chair back is refused")

        # An unknown category must be an ERROR, never a silent skip.
        if not check(good, "spaceship"):
            print("  SELFTEST FAILED: an unknown category was silently skipped", file=sys.stderr)
            failures += 1
        else:
            print("  an unknown category is an error, not a skip")

    if failures:
        return 1
    print("scale_check: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="*", type=Path)
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--list-categories", action="store_true")
    args = parser.parse_args()

    if args.list_categories:
        for name, rules in sorted(CATEGORIES.items()):
            for axis, low, high, what in rules:
                print(f"  {name:20s} {axis}  {low:.2f}-{high:.2f} m   {what}")
        for name, reason in sorted(UNSIZED_CATEGORIES.items()):
            print(f"  {name:20s} -  unsized -- {reason}")
        return 0

    if args.selftest:
        return selftest()

    document = manifest_tool.load()
    by_file = {row.get("sourceFile", ""): row for row in document.get("assets", [])}

    if args.paths:
        targets = [(p, by_file.get(p.relative_to(REPO).as_posix() if p.is_relative_to(REPO)
                                   else str(p))) for p in args.paths]
    else:
        targets = [
            (REPO / relative, row)
            for relative, row in sorted(by_file.items())
            if relative.lower().endswith((".glb", ".gltf"))
        ]

    if not targets:
        print("scale_check: no models with a manifest row.")
        return 0

    total = 0
    for path, row in targets:
        relative = path.relative_to(REPO) if path.is_relative_to(REPO) else path
        if row is None:
            print(f"scale_check: {relative} has no manifest row, so its category is unknown",
                  file=sys.stderr)
            total += 1
            continue
        category = row.get("category", "")
        problems = check(path, category, row.get("geometry"))
        if problems:
            print(f"scale_check: {relative} (category '{category}')", file=sys.stderr)
            for problem in problems:
                print(f"  {problem}", file=sys.stderr)
            total += len(problems)
        else:
            note = " -- unsized" if category in UNSIZED_CATEGORIES else ""
            print(f"scale_check: {relative} OK (category '{category}'){note}")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
