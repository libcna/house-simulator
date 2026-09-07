#!/usr/bin/env python3
"""layout_io.py -- reading `assets-src/world/*.json`, which is JSONC, not JSON.

Extracted for `HOUSE-00210`, because `build_collision.py` is the first of six world tools
(`HOUSE-00210`…`HOUSE-00215`) that all read the same seventeen files and would otherwise each grow
their own comment stripper and their own idea of what a cell's vertical extent is.

`docs/world-format.md` says the authored files permit comments and that the build strips them. A
stripper is four lines to write and one line to get wrong, and the line to get wrong is the string
literal: `"Textures/tile"` contains `//` and is not a comment. This one tracks string state, and
`selftest` asserts it against exactly that case.

Nothing here interprets a field. It parses, checks the `schema` header, indexes rows by id, and
answers the two geometric questions every consumer asks -- a cell's vertical extent and a cell's
footprint -- because three tools deriving "ffl to ceiling, unless `yOverride`" separately is three
chances to derive it differently.

    tools/world/layout_io.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

#: The seventeen authored files, by the `kind` in their `"schema": "cna-house/<kind>/<n>"` header.
#: The value is the file name and the key its rows live under; `None` means the file's payload is
#: an object rather than a list of rows (`layout.levels.json`'s `construction`, for instance, is
#: read alongside its `levels`).
FILES = {
    "manifest": ("world.manifest.json", "members"),
    "levels": ("layout.levels.json", "levels"),
    "cells": ("layout.cells.json", "cells"),
    "portals": ("layout.portals.json", "portals"),
    "openings": ("layout.openings.json", "openings"),
    "stairs": ("layout.stairs.json", "flights"),
    "lights": ("layout.lights.json", "lights"),
    "props": ("layout.props.json", "props"),
    "materials": ("layout.materials.json", "materials"),
    "nav": ("layout.nav.json", "nodes"),
    "audio": ("layout.audio.json", "zones"),
    "exterior": ("layout.exterior.json", None),
    "weather": ("layout.weather.json", None),
    "sky": ("layout.sky.json", None),
    "interactables": ("interactables.json", "interactables"),
    "initialstate": ("initialstate.json", None),
    "assets": ("assets.manifest.json", "assets"),
}


class LayoutError(Exception):
    """A layout file is missing, malformed, or not the schema this tool was written against."""


# ------------------------------------------------------------------------------------ parsing ----


def strip_jsonc(text: str) -> str:
    """Replace `//` and `/* */` comments with spaces, leaving everything else byte-identical.

    Comments become spaces rather than vanishing so that a `json` parse error still reports the
    line and column of the authored file. String literals are tracked, because a path like
    `"Textures/Architecture//tile"` is data and a `\\"` inside a string does not end it.
    """
    out = []
    i, n = 0, len(text)
    in_string = False
    while i < n:
        ch = text[i]
        if in_string:
            out.append(ch)
            if ch == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if ch == '"':
                in_string = False
            i += 1
            continue
        if ch == '"':
            in_string = True
            out.append(ch)
            i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            out.append("  ")
            i += 2
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def load_file(path: Path, kind: str | None = None) -> dict:
    """Parse one JSONC file and check its `schema` header names `kind`."""
    if not path.is_file():
        raise LayoutError(f"{path}: no such file")
    try:
        document = json.loads(strip_jsonc(path.read_text(encoding="utf-8")))
    except json.JSONDecodeError as exc:
        raise LayoutError(f"{path}:{exc.lineno}:{exc.colno}: {exc.msg}") from exc
    if not isinstance(document, dict):
        raise LayoutError(f"{path}: the top level is {type(document).__name__}, expected an object")
    schema = document.get("schema")
    if not isinstance(schema, str):
        raise LayoutError(f"{path}: no \"schema\" header; every world file carries one")
    parts = schema.split("/")
    if len(parts) != 3 or parts[0] != "cna-house":
        raise LayoutError(f"{path}: schema {schema!r} is not \"cna-house/<kind>/<version>\"")
    if kind is not None and parts[1] != kind:
        raise LayoutError(f"{path}: schema says {parts[1]!r}, this file must be {kind!r}")
    return document


def load_layout(directory: Path, kinds: list[str] | None = None) -> dict[str, dict]:
    """Load the named kinds (default: every file that is present) from a world directory.

    A kind that is asked for by name and is absent is an error; a kind that is merely part of the
    default sweep and absent is skipped, so a tool that needs three of the seventeen files can run
    against a partial layout while a tool that needs `cells` still fails loudly without it.
    """
    wanted = list(FILES) if kinds is None else kinds
    out: dict[str, dict] = {}
    for kind in wanted:
        if kind not in FILES:
            raise LayoutError(f"{kind!r} is not one of the world files: {', '.join(FILES)}")
        name, _ = FILES[kind]
        path = directory / name
        if not path.is_file():
            if kinds is None:
                continue
            raise LayoutError(f"{path}: required by this tool and not present")
        out[kind] = load_file(path, kind)
    return out


def rows(layout: dict[str, dict], kind: str) -> list[dict]:
    """The row list of one kind, or `[]` if the file was not loaded."""
    name, key = FILES[kind]
    if key is None:
        raise LayoutError(f"{name} has no row list; read its object fields directly")
    return list(layout.get(kind, {}).get(key, []))


def by_id(row_list: list[dict], what: str) -> dict[str, dict]:
    """Index rows by `id`, refusing a duplicate rather than letting the last one win."""
    out: dict[str, dict] = {}
    for row in row_list:
        identifier = row.get("id")
        if not isinstance(identifier, str):
            raise LayoutError(f"a {what} row has no string id: {row!r}")
        if identifier in out:
            raise LayoutError(f"{what} id {identifier!r} appears twice")
        out[identifier] = row
    return out


# ---------------------------------------------------------------------------------- geometry ----


def cell_extent(cell: dict, level: dict) -> tuple[float, float]:
    """The cell's floor and ceiling Y, in metres.

    `yOverride` wins when present; otherwise the level's `ffl` to its `ceiling`. `L3`'s `ceiling`
    is `null` because the attic is bounded by rafters rather than a plane (`world-format.md`), so a
    cell on such a level must carry its own `yOverride` -- there is no sane default to invent, and
    inventing one would put a ceiling slab through the roof.
    """
    override = cell.get("yOverride")
    if override is not None:
        low, high = float(override[0]), float(override[1])
        if not low < high:
            raise LayoutError(f"cell {cell.get('id')!r}: yOverride {override} is not min < max")
        return low, high
    ffl = level.get("ffl")
    ceiling = level.get("ceiling")
    if ffl is None:
        raise LayoutError(f"level {level.get('id')!r} has no ffl")
    if ceiling is None:
        raise LayoutError(
            f"cell {cell.get('id')!r} is on level {level.get('id')!r}, whose ceiling is null "
            f"(rafter-bounded), so the cell must declare its own yOverride"
        )
    return float(ffl), float(ceiling)


def cell_boxes(cell: dict) -> list[tuple[float, float, float, float]]:
    """The cell's footprint boxes as `(x0, x1, z0, z1)`, checked for min < max."""
    out = []
    for index, box in enumerate(cell.get("boxes", [])):
        x0, x1 = float(box["x"][0]), float(box["x"][1])
        z0, z1 = float(box["z"][0]), float(box["z"][1])
        if not (x0 < x1 and z0 < z1):
            raise LayoutError(
                f"cell {cell.get('id')!r} box {index} is degenerate: x {x0}..{x1}, z {z0}..{z1}"
            )
        out.append((x0, x1, z0, z1))
    if not out:
        raise LayoutError(f"cell {cell.get('id')!r} has no boxes")
    return out


# ----------------------------------------------------------------------------------- selftest ----


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("layout_io: selftest")

    # 1. The failure this stripper exists to avoid: `//` inside a string literal is data.
    text = '{"albedo": "Textures/Architecture//tile", "n": 1} // trailing\n'
    require(json.loads(strip_jsonc(text))["albedo"] == "Textures/Architecture//tile",
            "a `//` inside a string literal survives; only the trailing comment is stripped")

    # 2. An escaped quote does not end the string, so the comment scanner does not resume early.
    text = '{"note": "he said \\"a // b\\" once", "n": 2}'
    require(json.loads(strip_jsonc(text))["note"] == 'he said "a // b" once',
            "an escaped quote inside a string does not end it")

    # 3. Comments become spaces, so a parse error still names the authored line. A stripper that
    #    deleted them would report line 1 for a mistake on line 4.
    text = '{\n  // one\n  /* two\n     three */\n  "bad": ,\n}\n'
    stripped = strip_jsonc(text)
    require(stripped.count("\n") == text.count("\n"),
            f"stripping preserves the line count ({stripped.count(chr(10))} of "
            f"{text.count(chr(10))}), so error positions still point at the source")
    try:
        json.loads(stripped)
        line = -1
    except json.JSONDecodeError as exc:
        line = exc.lineno
    require(line == 5, f"...and the parse error is reported on line 5, where it is (got {line})")

    # 4. Block comments.
    require(json.loads(strip_jsonc('{"a": 1, /* b */ "c": 2}')) == {"a": 1, "c": 2},
            "a block comment between two members is stripped")

    # 5. `cell_extent`: the override wins, the level is the default, and a rafter-bounded level
    #    with no override is an error rather than a guess.
    level_l0 = {"id": "L0", "ffl": 0.60, "ceiling": 3.30}
    level_l3 = {"id": "L3", "ffl": 9.30, "ceiling": None}
    require(cell_extent({"id": "C", "yOverride": None}, level_l0) == (0.60, 3.30),
            "a null yOverride means the level's ffl..ceiling")
    require(cell_extent({"id": "C", "yOverride": [0.60, 2.10]}, level_l0) == (0.60, 2.10),
            "a yOverride wins over the level (a closet under a stair)")
    try:
        cell_extent({"id": "L3_ATTIC"}, level_l3)
        raised = ""
    except LayoutError as exc:
        raised = str(exc)
    require("yOverride" in raised and "rafter" in raised,
            "a cell on a rafter-bounded level with no yOverride is refused, naming why")

    # 6. `by_id` refuses a duplicate. Ids are globally unique (`world-format.md`); silently letting
    #    the last row win is how two cells become one.
    try:
        by_id([{"id": "A"}, {"id": "A"}], "cell")
        raised = ""
    except LayoutError as exc:
        raised = str(exc)
    require("twice" in raised, "a duplicate id is refused, not overwritten")

    # 7. A degenerate box is refused. A zero-area cell would generate zero-thickness collision.
    try:
        cell_boxes({"id": "C", "boxes": [{"x": [1.0, 1.0], "z": [0.0, 2.0]}]})
        raised = ""
    except LayoutError as exc:
        raised = str(exc)
    require("degenerate" in raised, "a zero-width cell box is refused")

    if failures:
        return 1
    print("layout_io: selftest passed.")
    return 0


if __name__ == "__main__":
    if "--selftest" in sys.argv[1:]:
        sys.exit(selftest())
    print(__doc__.splitlines()[0], file=sys.stderr)
    sys.exit(2)
