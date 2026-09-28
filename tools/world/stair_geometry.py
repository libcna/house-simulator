#!/usr/bin/env python3
"""stair_geometry.py -- where a flight's runs, landings and steps are, in world coordinates.

`HOUSE-00472`. Two tools need the same answer and each worked it out for itself:

* `tools/blender/house_shell_gen.py` builds the steps you walk on (`HOUSE-00459`);
* `tools/world/build_collision.py` builds the prism you collide with (`HOUSE-00210`).

They disagreed, and both were wrong about the main stair.

`build_collision.py` predates the authored placement entirely. Its comment said so -- *"without an
authored footprint the only defensible placement is the shared edge of the two cells' boxes"* --
and that was true until `HOUSE-00459` gave every flight a `footprint`, a `run` and a `shape`. It
ran every flight along +Z from a cell corner, in one lane, so the collision for `STAIR_MAIN_L0_L1`
was neither where the shell drew it nor, for a `u`, two lanes at all.

`house_shell_gen.py` read the footprint but turned the wrong way. It laid the second run of a `u`
BEYOND the landing, continuing in the run direction, climbing back towards it -- which puts the
TOP tread of the flight against the landing's far edge. You would have stepped off the half-landing
at +2.2147 straight onto L1's floor at +3.65. §12's table calls the flight `U, half-landing at
riser 9`: after the landing you turn through 180 degrees and climb back beside the way you came, on
the other side of the well, which is why the footprint is 2.70 m across for a flight 1.10 m wide.

So this module is the single source of truth for a flight's placement, and both tools import it.

    tools/world/stair_geometry.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import layout_io  # noqa: E402

RUNS = ("-X", "+X", "-Z", "+Z")


def turns(flight: dict):
    """`[(riser, depth), ...]`, sorted. A landing splits a run whatever the flight's shape is.

    A `u` turns on its landing; a straight flight with an intermediate landing keeps going in the
    same direction on the same side. Both are runs of steps with a flat pad between them, and
    `HOUSE-00347` found that the split must happen for both: it is where the walking surface stops
    sloping, and a sweep that treats a landing as part of the ramp walks up through it.
    """
    rows = flight.get("landings") or []
    risers = int(flight["risers"])
    seen = sorted({int(row["at"]) for row in rows if 0 < int(row["at"]) < risers})
    depths = {int(row["at"]): float(row.get("depth") or 0.0) for row in rows}
    return [(at, depths[at]) for at in seen]


def segments(flight: dict, bottom: float):
    """A flight as an ordered walk, before it is placed: what you meet going up.

    Each entry is a dict with `kind` (`"run"` or `"landing"`), `along0`/`along1` in metres in
    travel order from the foot, `y0`/`y1`, `lane` and `risers`. `along` is signed and DECREASES
    after a `u` turns, which is the whole point: the second run comes back over the ground the
    first one covered, one lane across.
    """
    risers = int(flight["risers"])
    rise, going = float(flight["rise"]), float(flight["going"])
    doubles_back = flight.get("shape") == "u"
    walk: list[dict] = []
    step, along, y, lane, direction = 0, 0.0, bottom, 0, 1.0
    for at, depth in turns(flight) + [(risers, 0.0)]:
        count = at - step
        if count > 0:
            walk.append({"kind": "run", "along0": along, "along1": along + direction * count * going,
                         "y0": y, "y1": y + count * rise, "lane": lane, "risers": count})
            along += direction * count * going
            y += count * rise
            step = at
        if depth > 0.0:
            walk.append({"kind": "landing", "along0": along, "along1": along + direction * depth,
                         "y0": y, "y1": y, "lane": -1, "risers": 0})
            if doubles_back:
                # A switchback crosses the full-width pad at its *near* edge. Starting the
                # returning treads at the pad's far edge puts them directly over the landing,
                # including a balustrade across the only walking route to the second run.
                # The pad extends beyond both flights to provide turning room; it does not
                # consume more longitudinal run before the direction reverses.
                direction = -direction
                lane = 1 - lane
            else:
                along += direction * depth
    if doubles_back and flight.get("topLandingToFoot"):
        # The upper run ends short of the south wall. A level strip beside the incoming
        # run joins its top tread to the landing/doorway at that wall, without filling the
        # full stairwell or blocking headroom over the other run.
        cross_depth = float(flight.get("topCrossLandingDepth") or 0.0)
        if abs(along - cross_depth) > 1.0e-6:
            walk.append({"kind": "exit_landing", "along0": along, "along1": cross_depth,
                         "y0": y, "y1": y, "lane": lane, "risers": 0})
        if cross_depth + float(flight.get("approachDepth") or 0.0) > 1.0e-6:
            walk.append({"kind": "cross_landing", "along0": cross_depth,
                         "along1": -float(flight.get("approachDepth") or 0.0),
                         "y0": y, "y1": y, "lane": -1, "risers": 0})
    return walk


def well_cross(flight: dict, portals=()):
    """The cross-axis range a flight must fit through, or None when nothing narrows it.

    `HOUSE-00480`. A flight's `footprint` is its cell's box, whose edges are WALL CENTRE LINES:
    placing a lane hard against one puts 0.20 m of the flight inside the wall and outside the hole
    the floor above has in it. The stairwell portal IS that hole -- `P_STAIR_L0_L1` is authored
    X 2.40-4.70 against the main stair's footprint of 2.20-4.90 -- and a flight has to fit through
    the hole it comes up. So the lanes are placed in the intersection of the two, which is authored
    data rather than a wall thickness guessed at from `construction`.

    Only the horizontal portal joining this flight's own from/to cells constrains it. A lower
    flight's hole must not squeeze a different flight above it. A flight with no such portal --
    the porch and terrace steps -- keeps its whole footprint.
    """
    fields = _fields_of(flight)
    if fields is None:
        return None
    axis, _sign, _start, cross_lo, cross_hi = fields
    lo, hi = cross_lo, cross_hi
    footprint = flight["footprint"]
    fx0, fx1 = (float(value) for value in footprint["x"])
    fz0, fz1 = (float(value) for value in footprint["z"])
    for portal in portals:
        plane = portal.get("plane") or {}
        rect = portal.get("rect") or {}
        if plane.get("axis") != "y" or not rect:
            continue
        # Only the slab between THIS flight's two cells can constrain its lanes. The main
        # stair and the basement stair share a plan footprint but pierce different slabs;
        # intersecting both holes would force the U flight into the basement's narrow lane.
        if {portal.get("cellA"), portal.get("cellB")} != {flight.get("fromCell"), flight.get("toCell")}:
            continue
        # `u` is x and `v` is z for a horizontal plane, which is what `world-format.md` says and
        # what `slab_holes` reads.
        u0, u1 = (float(value) for value in rect["u"])
        v0, v1 = (float(value) for value in rect["v"])
        if min(u1, fx1) - max(u0, fx0) <= 1e-6 or min(v1, fz1) - max(v0, fz0) <= 1e-6:
            continue
        if axis == "x":
            lo, hi = max(lo, v0), min(hi, v1)
        else:
            lo, hi = max(lo, u0), min(hi, u1)
    if hi - lo < float(flight["width"]) - 1e-6:
        # The hole is narrower than the flight. Narrowing further would put the flight through a
        # wall to no purpose, so the footprint stands and `verify_shell` says what it costs.
        return None
    return (lo, hi)


def _fields_of(flight: dict):
    """`(axis, sign, start, cross_lo, cross_hi)` straight from the footprint, before any narrowing."""
    footprint = flight.get("footprint") or {}
    if not footprint or flight.get("run") not in RUNS:
        return None
    run = flight["run"]
    x0, x1 = (float(value) for value in footprint["x"])
    z0, z1 = (float(value) for value in footprint["z"])
    axis = "x" if run in ("-X", "+X") else "z"
    sign = -1.0 if run in ("-X", "-Z") else 1.0
    if axis == "x":
        return ("x", sign, x1 if sign < 0 else x0, z0, z1)
    return ("z", sign, z1 if sign < 0 else z0, x0, x1)


def frame(flight: dict, portals=()):
    """`(axis, sign, start, cross_lo, cross_hi, width)`, or None when nothing is authored.

    `axis` is the world axis the flight travels along, `sign` the direction of travel, `start` the
    edge of the footprint the foot of the flight stands on. Lane 0 is `cross_lo` by default;
    `firstRunAt: cross_hi` mirrors a U flight. `approachDepth` leaves a level entry strip
    ahead of a flight without shortening its upper-floor exit bridge.
    Both shell and collision use this frame. The cross range is `well_cross`'s where the flight's
    own stairwell narrows it and the footprint's otherwise.
    """
    fields = _fields_of(flight)
    if fields is None:
        return None
    axis, sign, start, cross_lo, cross_hi = fields
    narrowed = well_cross(flight, portals)
    if narrowed is not None:
        cross_lo, cross_hi = narrowed
    start += sign * float(flight.get("approachDepth") or 0.0)
    return (axis, sign, start, cross_lo, cross_hi, float(flight["width"]))


def place(flight: dict, along0: float, along1: float, lane: int, portals=()):
    """One segment's `(x0, x1, z0, z1)`, or None when the flight has no authored placement.

    `lane < 0` spans the full width of the footprint across, which is what a landing is: you turn
    round on it, so it is as wide as both runs and the well between them.
    """
    fields = frame(flight, portals)
    if fields is None:
        return None
    axis, sign, start, cross_lo, cross_hi, width = fields
    low, high = sorted((start + sign * along0, start + sign * along1))
    if lane < 0:
        cross = (cross_lo, cross_hi)
    elif (lane == 0) != (flight.get("firstRunAt") == "cross_hi"):
        cross = (cross_lo, cross_lo + width)
    else:
        cross = (cross_hi - width, cross_hi)
    return (low, high, cross[0], cross[1]) if axis == "x" else (cross[0], cross[1], low, high)


def flight_runs(flight: dict, bottom: float, portals=()):
    """Every run and landing of @p flight as a placed box, or None if it has no placement.

    Each entry adds `box` -- `(x0, x1, z0, z1)` -- `axis`, and `up`: the direction along `axis` the
    run RISES in, `+1` towards the larger coordinate. A `u`'s two runs have opposite `up`, and a
    consumer that ignores it builds a staircase you descend to reach the floor above.
    """
    fields = frame(flight, portals)
    if fields is None:
        return None
    axis, sign, *_rest = fields
    walk = segments(flight, bottom)
    for entry in walk:
        entry["box"] = place(flight, entry["along0"], entry["along1"], entry["lane"], portals)
        entry["axis"] = axis
        travel = 1.0 if entry["along1"] >= entry["along0"] else -1.0
        entry["up"] = int(travel * sign)
    return walk


def flight_steps(flight: dict, bottom: float, portals=()):
    """Every tread of @p flight, in climbing order: `box`, `y0`, `y1`, `lane`, `axis`, `up`.

    `y1` is the top of the tread -- the surface you stand on -- so the n-th step of a flight is at
    `bottom + n * rise`, which is what §12.4 means by a landing being where a riser ends.
    """
    placed = flight_runs(flight, bottom, portals)
    if placed is None:
        return None
    going, rise = float(flight["going"]), float(flight["rise"])
    treads: list[dict] = []
    number = 0
    for entry in placed:
        if entry["kind"] != "run":
            continue
        direction = 1.0 if entry["along1"] >= entry["along0"] else -1.0
        number += 1
        for index in range(entry["risers"]):
            a0 = entry["along0"] + direction * index * going
            treads.append({
                "box": place(flight, a0, a0 + direction * going, entry["lane"], portals),
                "y0": bottom, "y1": entry["y0"] + (index + 1) * rise,
                "lane": entry["lane"], "axis": entry["axis"], "up": entry["up"],
                "run": number - 1, "along0": a0, "along1": a0 + direction * going})
    return treads


def foot_of(flight: dict, cells: dict, levels: dict) -> float:
    """The Y a flight starts at: what it declares, else the floor of the cell it stands in.

    The porch, terrace and garage steps join two cells on ONE level and are the only things that
    know what they climb (`HOUSE-00379`), so their own `fromY` wins.
    """
    declared = flight.get("fromY")
    if declared is not None:
        return float(declared)
    cell = cells.get(flight.get("fromCell"))
    if cell is None:
        return 0.0
    level = levels.get(cell.get("level"))
    if level is None:
        return 0.0
    return layout_io.cell_extent(cell, level)[0]


# ======================================================================================= selftest


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("stair_geometry: selftest")
    source = Path(__file__).resolve().parents[2] / "assets-src" / "world"
    if not (source / "layout.stairs.json").is_file():
        require(False, "the authored stairs are there")
        return 1

    layout = layout_io.load_layout(source, kinds=["levels", "cells", "stairs", "portals"])
    levels = {row["id"]: row for row in layout_io.rows(layout, "levels")}
    cells = {row["id"]: row for row in layout_io.rows(layout, "cells")}
    flights = {row["id"]: row for row in layout_io.rows(layout, "stairs")}
    portals = layout_io.rows(layout, "portals")

    main = flights["STAIR_MAIN_L0_L1"]
    foot = foot_of(main, cells, levels)
    require(abs(foot - 0.60) < 1e-9, f"the main stair starts on L0's floor ({foot})")

    walk = flight_runs(main, foot)
    kinds = [entry["kind"] for entry in walk]
    require(kinds == ["run", "landing", "run", "cross_landing"],
            f"the main U stair has two runs, a turn and a continuous upper exit ({kinds})")
    require([e["risers"] for e in walk if e["kind"] == "run"] == [9, 8],
            "the two main flights are balanced around the switchback landing")
    require(walk[0]["lane"] == 0 and walk[2]["lane"] == 1,
            "and the second run is on the other side of the well")
    actual = flight_runs(main, foot, portals)
    require(all(abs(a - b) < 1e-6 for a, b in zip(actual[0]["box"], (2.2, 3.3, -17.82, -15.3)))
            and all(abs(a - b) < 1e-6 for a, b in zip(actual[2]["box"], (3.8, 4.9, -17.82, -15.58)))
            and all(abs(a - b) < 1e-6 for a, b in zip(actual[-1]["box"],
                                                      (2.2, 4.9, -15.58, -14.3))),
            f"the foyer directly meets the west first run and the L1 exit spans back to its "
            f"west doorway ({actual[0]['box']}, {actual[2]['box']}, {actual[-1]['box']})")
    require(actual[1]["box"][3] <= actual[2]["box"][2] + 1e-6,
            "the returning treads begin beyond the north pad, never on top of its walking surface")
    basement = flights["STAIR_BASEMENT_L0_B1"]
    basement_walk = flight_runs(basement, foot_of(basement, cells, levels), portals)
    require(basement_walk[0]["box"][0:2] == (3.6, 4.6),
            f"the basement stair fits its own east-lane well ({basement_walk[0]['box']})")
    upper = flights["STAIR_MAIN_L1_L2"]
    upper_walk = flight_runs(upper, foot_of(upper, cells, levels), portals)
    require(upper_walk[-1]["kind"] == "cross_landing"
            and all(abs(a - b) < 1e-6 for a, b in zip(
                upper_walk[-1]["box"], (2.2, 4.9, -15.3, -14.3))),
            f"the L2 east door meets a full-width cross landing ({upper_walk[-1]['box']})")
    require(walk[0]["up"] == -walk[2]["up"],
            f"the two runs of a `u` climb in OPPOSITE directions "
            f"({walk[0]['up']}, {walk[2]['up']})")
    require(abs(walk[-1]["y1"] - float(levels["L1"]["ffl"])) < 1e-6,
            f"the top of the last run IS the floor above ({walk[-1]['y1']})")
    require(abs(walk[1]["y0"] - (foot + 9 * float(main["rise"]))) < 1e-9
            and walk[1]["lane"] == -1,
            f"the landing is at the top of the ninth riser and as wide as both runs "
            f"({walk[1]['y0']:.4f})")

    # The second run must MEET the landing at its own foot, not at its head. This is the defect
    # `house_shell_gen.py` had: it laid the run beyond the landing and climbed back towards it, so
    # you stepped off the half-landing onto the top tread and arrived a whole storey early.
    landing_box, second = walk[1]["box"], walk[2]
    axis_index = 0 if second["axis"] == "x" else 2
    low_end = second["box"][axis_index] if second["up"] > 0 else second["box"][axis_index + 1]
    require(abs(low_end - landing_box[axis_index + (1 if second["up"] > 0 else 0)]) < 1e-6,
            f"the second run's FOOT touches the landing, not its head ({low_end:.3f})")

    for identifier, flight in sorted(flights.items()):
        base = foot_of(flight, cells, levels)
        placed = flight_runs(flight, base)
        require(placed is not None, f"{identifier} has an authored placement")
        if placed is None:
            continue
        top = placed[-1]["y1"]
        climbed = int(flight["risers"]) * float(flight["rise"])
        require(abs((top - base) - climbed) < 1e-6,
                f"{identifier} climbs its own {flight['risers']} x {flight['rise']:.4f} m "
                f"({top - base:.4f})")
        box = flight["footprint"]
        inside = all(float(box["x"][0]) - 1e-6 <= entry["box"][0]
                     and entry["box"][1] <= float(box["x"][1]) + 1e-6
                     and float(box["z"][0]) - 1e-6 <= entry["box"][2]
                     and entry["box"][3] <= float(box["z"][1]) + 1e-6 for entry in placed)
        require(inside, f"{identifier} stays inside the footprint it declares")
        treads = flight_steps(flight, base)
        require(len(treads) == int(flight["risers"]),
                f"{identifier} has one tread per riser ({len(treads)})")
        rises = [treads[index]["y1"] - treads[index - 1]["y1"] for index in range(1, len(treads))]
        require(not rises or max(rises) - min(rises) < 1e-9,
                f"{identifier}'s treads rise by an equal amount every time (§12.4)")
        require(abs(treads[-1]["y1"] - top) < 1e-9 and abs(treads[0]["y1"] - base - float(
            flight["rise"])) < 1e-9,
            f"{identifier}'s first tread is one riser up and its last IS the floor it reaches "
            f"({treads[0]['y1']:.4f}, {treads[-1]['y1']:.4f})")
        require(all(entry["box"] is not None for entry in placed), f"{identifier} is placed")
        for tread in treads:
            span = tread["box"][0 if tread["axis"] == "x" else 2:][:2]
            require(abs((span[1] - span[0]) - float(flight["going"])) < 1e-6,
                    f"{identifier}'s treads are its own {flight['going']:.3f} m going deep")
            break

    lane0 = [e for e in flight_runs(main, foot) if e["lane"] == 0][0]["box"]
    lane1 = [e for e in flight_runs(main, foot) if e["lane"] == 1][0]["box"]
    require(lane0[1] <= lane1[0] + 1e-9 or lane1[1] <= lane0[0] + 1e-9,
            f"the main stair's two lanes are side by side, not through each other "
            f"({lane0[0]:.2f}..{lane0[1]:.2f}, {lane1[0]:.2f}..{lane1[1]:.2f})")

    straight = flights["STAIR_ATTIC_L2_L3"]
    walk = flight_runs(straight, 6.55)
    require(len(walk) == 1 and walk[0]["kind"] == "run",
            "a straight flight is one run and no landing")
    attic = flight_runs(straight, 6.55, portals)[0]["box"]
    head = cells["L3_STAIR_HEAD"]["boxes"][0]["z"]
    require(abs(attic[3] - (-14.30 - float(straight["approachDepth"]))) < 1e-6
            and float(straight["approachDepth"]) >= 0.9,
            f"the attic foot has a level entry before its first tread ({attic[3]:.3f})")
    require(head[0] < attic[2] - 0.9,
            f"the attic top has over 0.9 m of level landing before its north wall "
            f"({head[0]:.3f}..{attic[2]:.3f})")

    basement = flights["STAIR_BASEMENT_L0_B1"]
    basement_run = flight_runs(basement, -2.30, portals)[0]["box"]
    require(basement.get("approachDepth", 0.0) >= 1.0
            and abs(basement_run[2] - (-19.20)) < 1e-6
            and abs(basement_run[3] - (-15.20)) < 1e-6,
            "the basement has a level north approach and joins the clear south entry strip")

    garage = flights["STEPS_GARAGE"]
    gx0, gx1, gz0, gz1 = flight_runs(garage, 0.15)[0]["box"]
    require(abs((gx1 - gx0) - 3 * float(garage["going"])) < 1e-6
            and abs((gz1 - gz0) - float(garage["width"])) < 1e-6,
            f"the garage steps run along X and are 1.10 m across Z "
            f"({gx1 - gx0:.2f} by {gz1 - gz0:.2f})")

    dogleg = dict(flights["STAIR_ATTIC_L2_L3"], shape="straight", risers=10,
                  landings=[{"at": 5, "depth": 1.0}])
    grouped = flight_steps(dogleg, 0.0)
    require(sorted({tread["run"] for tread in grouped}) == [0, 1]
            and {tread["lane"] for tread in grouped} == {0}
            and len([t for t in grouped if t["run"] == 0]) == 5,
            f"treads are numbered by RUN, not by lane: a straight flight with a landing has two "
            f"runs in one lane ({sorted({t['run'] for t in grouped})}, "
            f"{sorted({t['lane'] for t in grouped})})")

    # A straight flight with an intermediate landing keeps its direction and its lane: only a `u`
    # turns. Nothing in this house is one, so the claim is made against a constructed flight.
    walk = flight_runs(dogleg, 0.0)
    require([e["kind"] for e in walk] == ["run", "landing", "run"]
            and walk[0]["lane"] == walk[2]["lane"] == 0
            and walk[0]["up"] == walk[2]["up"],
            "a straight flight with a landing carries straight on, same lane, same direction")

    # §12's porch, terrace and garage steps join two cells on ONE level: the cell's floor says
    # nothing about what they climb, so the flight's own `fromY` has to win.
    for identifier in ("STEPS_PORCH", "STEPS_TERRACE", "STEPS_TERRACE_LAWN", "STEPS_GARAGE"):
        declared = float(flights[identifier]["fromY"])
        require(abs(foot_of(flights[identifier], cells, levels) - declared) < 1e-9,
                f"{identifier} starts at the +{declared:.2f} m it declares, not at its cell's "
                f"floor")
    lawn = flights["STEPS_TERRACE_LAWN"]
    from_floor = layout_io.cell_extent(cells[lawn["fromCell"]],
                                       levels[cells[lawn["fromCell"]]["level"]])[0]
    require(abs(foot_of(lawn, cells, levels) - from_floor) > 0.5,
            f"and for the lawn steps that is {float(lawn['fromY']):.2f} m, not the "
            f"{from_floor:.2f} m their cell's floor is at -- so the claim above proves something")

    # A landing AT the top riser is the floor the flight arrives on, not a pad within it.
    topped = dict(flights["STAIR_ATTIC_L2_L3"], risers=8,
                  landings=[{"at": 8, "depth": 1.2}])
    walk = flight_runs(topped, 0.0)
    require([e["kind"] for e in walk] == ["run"] and walk[0]["risers"] == 8,
            f"a landing on the last riser is the floor above, not a segment "
            f"({[e['kind'] for e in walk]})")

    unplaced = {key: value for key, value in flights["STAIR_ATTIC_L2_L3"].items()
                if key not in ("footprint", "run")}
    require(flight_runs(unplaced, 0.0) is None and place(unplaced, 0.0, 1.0, 0) is None,
            "a flight with no authored footprint reports that it cannot be placed")
    require(len(segments(unplaced, 0.0)) == 1,
            "though its shape is still known, so a caller can fall back to its own guess")

    if failures:
        print(f"\nstair_geometry: {len(failures)} claim(s) FAILED")
        return 1
    print(f"stair_geometry: selftest passed ({len(flights)} flights).")
    return 0


if __name__ == "__main__":
    sys.exit(selftest())
