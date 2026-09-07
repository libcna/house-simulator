#!/usr/bin/env python3
"""footstep_map.py -- which of the twenty surfaces the NOX packs actually cover, and which they do not.

`HOUSE-00280`. `cna-house.md` §62.4 names twenty footstep surfaces and requires **≥ 6 walk, ≥ 6 run
and ≥ 2 land** variants of each. `HOUSE-00278` imported twelve NOX packs. This measures the overlap
against the files that are actually in the tree, rather than against §63.4's prose estimate, and
writes `docs/asset-selection/footstep-surfaces.md` -- which is `HOUSE-00281`'s shopping list.

    tools/assets/footstep_map.py            # write the map
    tools/assets/footstep_map.py --check    # fail if it is stale
    tools/assets/footstep_map.py --selftest

## The counts come from the manifest, not from the packs

§63.4 says the collection covers "12 of the 20 surfaces the game needs", and that is a count of
*packs*. What §62.4 requires is a count of **variants per action per surface**, and the two are not
the same question: a pack can cover a surface and still be one land sample short of the rule. Every
number here is counted from `assets-src/Audio/footstep*` by way of the manifest, so a surface that
looks served and is not shows up as a shortfall rather than as a tick.

## What maps to what

`jump`, `land` and `jump-land` are all the **landing impact** -- the NOX packs name the same sound
three different ways, and the files under all three are `..._Jump_Land_NN.wav`. `jump-start` is the
take-off, which §62.4 does not ask for and which is recorded as a bonus. `foley` is cloth movement
and is not a footstep at all.

The `footstep-exterior/*` packs are **weather variants of surfaces that already exist**, not new
ones: frozen and wet gravel are gravel, and §38 is explicit that the three snow packs all feed the
one `snow` surface set. They add variants to their base surface and are counted there.

`Wood` is the one pack that maps to **nothing**. §63.4: "the Wood pack is exterior-flavoured", so
it is not interior `hardwood`, and §62.4 has no exterior timber surface. Recording that as a
non-mapping rather than quietly assigning it to `hardwood` is the point of this task -- a
deck-flavoured creak under a living-room floor is exactly the sort of thing nobody notices in a
spreadsheet and everybody notices in the room.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import manifest as manifest_tool  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
REPORT = REPO / "docs" / "asset-selection" / "footstep-surfaces.md"

#: `cna-house.md` §62.4, in its own order. Twenty.
SURFACES = [
    "hardwood", "carpet", "tile", "concrete",
    "stair_wood", "stair_carpet", "stair_wood_open",
    "grass", "gravel", "asphalt", "bluestone", "soil",
    "mud", "snow", "water", "metal", "leaves", "rock", "sand", "dirt",
]

#: §62.4's minimum per surface.
MINIMUM = {"walk": 6, "run": 6, "land": 2}

#: NOX category prefix -> the §62.4 surface it serves. A prefix absent from this table serves no
#: surface, and `Wood` is deliberately absent: §63.4 records it as exterior-flavoured.
NOX_SURFACE = {
    "footstep/dirtyground": ("dirt", "soil"),
    "footstep/grass": ("grass",),
    "footstep/gravel": ("gravel",),
    "footstep/leaves": ("leaves",),
    "footstep/metal": ("metal",),
    "footstep/mud": ("mud",),
    "footstep/rock": ("rock",),
    "footstep/sand": ("sand",),
    "footstep/snow": ("snow",),
    "footstep/tile": ("tile",),
    "footstep/water": ("water",),
    # Weather variants of surfaces that already exist (§38: the snow packs all feed `snow`).
    "footstep-exterior/grass": ("grass",),
    "footstep-exterior/gravel-frozen": ("gravel",),
    "footstep-exterior/gravel-wet": ("gravel",),
    "footstep-exterior/snow-grass": ("snow",),
    "footstep-exterior/snow-hard": ("snow",),
    "footstep-exterior/snow-short-grass": ("snow",),
}

#: The NOX action names, and which of §62.4's three each one is. `jump`, `land` and `jump-land` are
#: the same sound under three names; the files under all three are `..._Jump_Land_NN.wav`.
NOX_ACTION = {
    "walk": "walk", "run": "run",
    "jump": "land", "land": "land", "jump-land": "land",
    "jump-start": "takeoff",   # §62.4 does not ask for it; recorded as a bonus
    "foley": "foley",          # cloth movement, not a footstep
}


def tally(rows: list[dict]) -> dict[str, dict[str, int]]:
    """`surface -> action -> count`, from the manifest rows themselves."""
    counts: dict[str, dict[str, int]] = {surface: {} for surface in SURFACES}
    for row in rows:
        category = (row.get("audio") or {}).get("noxCategory", "")
        if not category.startswith("footstep"):
            continue
        parts = category.split("/")
        prefix, action = "/".join(parts[:-1]), parts[-1]
        surfaces = NOX_SURFACE.get(prefix)
        if not surfaces:
            continue
        mapped = NOX_ACTION.get(action)
        if mapped is None:
            continue
        for surface in surfaces:
            counts[surface][mapped] = counts[surface].get(mapped, 0) + 1
    return counts


def shortfalls(counts: dict[str, dict[str, int]]) -> dict[str, dict[str, int]]:
    """`surface -> action -> how many more are needed`, for every surface that is short."""
    out: dict[str, dict[str, int]] = {}
    for surface in SURFACES:
        missing = {action: need - counts[surface].get(action, 0)
                   for action, need in MINIMUM.items()
                   if counts[surface].get(action, 0) < need}
        if missing:
            out[surface] = missing
    return out


def unmapped_packs(rows: list[dict]) -> dict[str, int]:
    """NOX footstep prefixes that serve no §62.4 surface, with how many files each holds."""
    out: dict[str, int] = {}
    for row in rows:
        category = (row.get("audio") or {}).get("noxCategory", "")
        if not category.startswith("footstep"):
            continue
        prefix = "/".join(category.split("/")[:-1])
        if prefix not in NOX_SURFACE:
            out[prefix] = out.get(prefix, 0) + 1
    return out


def document(counts, missing, unmapped) -> str:
    served = [s for s in SURFACES if s not in missing]
    lines = [
        "# Footstep surfaces — what the NOX packs cover",
        "",
        "*Generated by `tools/assets/footstep_map.py` (`HOUSE-00280`). Every number is counted "
        "from the manifest, not estimated from the pack list. Regenerate rather than edit.*",
        "",
        f"`cna-house.md` §62.4 names **{len(SURFACES)} surfaces** and requires "
        f"**≥ {MINIMUM['walk']} walk, ≥ {MINIMUM['run']} run, ≥ {MINIMUM['land']} land** of each.",
        "",
        f"**{len(served)} of {len(SURFACES)} are served. {len(missing)} are not** — and that is "
        f"`HOUSE-00281`'s list.",
        "",
        "## Per surface",
        "",
        "| Surface | walk | run | land | takeoff | Verdict |",
        "|---|---:|---:|---:|---:|---|",
    ]
    for surface in SURFACES:
        row = counts[surface]
        need = missing.get(surface)
        if need is None:
            verdict = "served"
        else:
            verdict = "**unserved** — needs " + ", ".join(
                f"{count} more {action}" for action, count in sorted(need.items()))
        lines.append(
            f"| `{surface}` | {row.get('walk', 0)} | {row.get('run', 0)} | "
            f"{row.get('land', 0)} | {row.get('takeoff', 0)} | {verdict} |")

    lines += ["", "## What HOUSE-00281 must source", ""]
    if missing:
        lines.append("| Surface | walk | run | land |")
        lines.append("|---|---:|---:|---:|")
        for surface, need in missing.items():
            lines.append(f"| `{surface}` | {need.get('walk', 0)} | {need.get('run', 0)} | "
                         f"{need.get('land', 0)} |")
        total = sum(sum(need.values()) for need in missing.values())
        lines += ["", f"**{total} samples** across {len(missing)} surfaces, CC0."]
    else:
        lines.append("Nothing — every surface meets §62.4.")

    lines += ["", "## Packs that map to no surface", ""]
    if unmapped:
        for prefix, count in sorted(unmapped.items()):
            lines.append(f"* `{prefix}` — {count} file(s). §63.4 records the Wood pack as "
                         f"exterior-flavoured, so it is not interior `hardwood`, and §62.4 has no "
                         f"exterior timber surface. Assigning it to `hardwood` anyway would put a "
                         f"deck creak under a living-room floor.")
    else:
        lines.append("None.")
    lines.append("")
    return "\n".join(lines)


# ======================================================================================= selftest


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("footstep_map: selftest")

    require(len(SURFACES) == 20 and len(set(SURFACES)) == 20,
            f"§62.4's list is twenty surfaces and no duplicates ({len(SURFACES)})")
    require(MINIMUM == {"walk": 6, "run": 6, "land": 2},
            "and its minimum is 6 walk, 6 run, 2 land")

    def rows_for(category: str, count: int) -> list[dict]:
        return [{"id": f"X{i}", "audio": {"noxCategory": category}} for i in range(count)]

    # 1. `jump`, `land` and `jump-land` are the SAME sound under three names -- the files under all
    #    three are `..._Jump_Land_NN.wav`. Counting them as three actions would show every surface
    #    meeting a 2-land minimum it had never been tested against.
    counts = tally(rows_for("footstep/tile/jump", 2) + rows_for("footstep/dirtyground/land", 2))
    require(counts["tile"].get("land") == 2 and counts["dirt"].get("land") == 2,
            "both `jump` and `land` count as land")
    counts = tally(rows_for("footstep-exterior/grass/jump-land", 3))
    require(counts["grass"].get("land") == 3, "and so does `jump-land`")

    # 2. `jump-start` is the take-off, which §62.4 does not ask for, and `foley` is cloth movement
    #    and is not a footstep. Neither may count towards a minimum.
    counts = tally(rows_for("footstep-exterior/grass/jump-start", 9)
                   + rows_for("footstep/grass/foley", 9))
    require(counts["grass"].get("land", 0) == 0 and counts["grass"].get("walk", 0) == 0,
            "a take-off and a foley clip count towards no §62.4 minimum")
    require(counts["grass"].get("takeoff") == 9,
            "the take-offs are still counted, as a bonus the report shows")

    # 3. One pack can serve two surfaces, and the weather packs feed their base surface.
    counts = tally(rows_for("footstep/dirtyground/walk", 6))
    require(counts["dirt"].get("walk") == 6 and counts["soil"].get("walk") == 6,
            "the DirtyGround pack serves both `dirt` and `soil`")
    counts = tally(rows_for("footstep/gravel/walk", 3)
                   + rows_for("footstep-exterior/gravel-wet/walk", 2)
                   + rows_for("footstep-exterior/gravel-frozen/walk", 1))
    require(counts["gravel"].get("walk") == 6,
            "wet and frozen gravel are gravel -- weather variants, not new surfaces")
    counts = tally(rows_for("footstep-exterior/snow-hard/walk", 2)
                   + rows_for("footstep-exterior/snow-grass/walk", 2)
                   + rows_for("footstep-exterior/snow-short-grass/walk", 2))
    require(counts["snow"].get("walk") == 6,
            "and §38's three snow packs all feed the one `snow` surface set")

    # 4. Wood maps to NOTHING, deliberately. §63.4 records it as exterior-flavoured.
    counts = tally(rows_for("footstep/wood/walk", 20))
    require(counts["hardwood"].get("walk", 0) == 0,
            "the Wood pack does not serve interior `hardwood` -- a deck creak under a "
            "living-room floor is what assigning it anyway would produce")
    require(unmapped_packs(rows_for("footstep/wood/walk", 20)) == {"footstep/wood": 20},
            "and it is reported as unmapped rather than silently dropped")

    # 5. A surface that is one sample short is UNSERVED. §63.4 counts packs; §62.4 counts variants
    #    per action, and a pack can cover a surface and still be one land short.
    counts = tally(rows_for("footstep/tile/walk", 6) + rows_for("footstep/tile/run", 6)
                   + rows_for("footstep/tile/jump", 1))
    missing = shortfalls(counts)
    require("tile" in missing and missing["tile"] == {"land": 1},
            f"a surface with 6 walk, 6 run and ONE land is short by exactly one land "
            f"({missing.get('tile')})")
    counts = tally(rows_for("footstep/tile/walk", 6) + rows_for("footstep/tile/run", 6)
                   + rows_for("footstep/tile/jump", 2))
    require("tile" not in shortfalls(counts), "...and is served with two")

    # 6. The real tree. This is the measurement the task exists for.
    document_rows = manifest_tool.load()["assets"]
    real_counts = tally(document_rows)
    real_missing = shortfalls(real_counts)
    served = [s for s in SURFACES if s not in real_missing]
    # §63.4 says "12 of the 20 surfaces", and that is a count of PACKS. Counted per action, as
    # §62.4 actually requires, it is ELEVEN -- `water` has a pack and only five run variants.
    require(len(served) == 11,
            f"eleven of the twenty surfaces meet §62.4, not §63.4's twelve ({len(served)}: "
            f"{sorted(served)})")
    require("water" in real_missing and real_missing["water"] == {"run": 1},
            f"and `water` is the difference: it is short by exactly one run variant "
            f"({real_missing.get('water')}). NOX ships FIVE water run samples in the whole "
            f"collection -- nox_select.py took 5 of 5 against a cap of 6 -- so this is an upstream "
            f"shortfall, not something the selection could have chosen its way out of")
    require(set(real_missing) - {"water"} == {"hardwood", "carpet", "concrete", "stair_wood",
                                              "stair_carpet", "stair_wood_open", "asphalt",
                                              "bluestone"},
            f"the other eight are the ones §63.4 predicted ({sorted(real_missing)})")
    require(all(sum(need.values()) == 14 for surface, need in real_missing.items()
                if surface != "water"),
            "each of those eight needs the full 14 -- none is partly covered")
    total = sum(sum(need.values()) for need in real_missing.values())
    require(total == 113,
            f"so HOUSE-00281's row is {total} samples, not 8 x 14 = 112: the eight surfaces plus "
            f"the one water run variant NOX does not have")

    text = document(real_counts, real_missing, unmapped_packs(document_rows))
    require("`hardwood`" in text and "unserved" in text, "the document names the unserved surfaces")
    require("footstep/wood" in text, "and records the pack that maps to nothing")

    if failures:
        return 1
    print("footstep_map: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail if the document is stale")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    rows = manifest_tool.load()["assets"]
    counts = tally(rows)
    missing = shortfalls(counts)
    text = document(counts, missing, unmapped_packs(rows))

    if args.check:
        if not REPORT.is_file() or REPORT.read_text(encoding="utf-8") != text:
            print(f"footstep_map: {REPORT.relative_to(REPO)} is stale; run "
                  f"tools/assets/footstep_map.py", file=sys.stderr)
            return 1
        print(f"footstep_map: {REPORT.relative_to(REPO)} matches the manifest.")
        return 0

    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(text, encoding="utf-8")
    served = len(SURFACES) - len(missing)
    print(f"footstep_map: {served} of {len(SURFACES)} surfaces served, {len(missing)} not; "
          f"wrote {REPORT.relative_to(REPO)}")
    for surface, need in missing.items():
        print(f"  {surface:<18} needs " + ", ".join(
            f"{count} {action}" for action, count in sorted(need.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
