#!/usr/bin/env python3
"""check_fonts.py -- every `.spritefont` rasterises a font FROM THIS REPOSITORY, and covers Latin-1.

`HOUSE-00200`. Two failures this gate exists to prevent, both of which are silent without it:

* **Silent system-font fallback.** `FontDescriptionImporter` resolves `<FontName>` to a file beside
  the descriptor first and falls back to `/usr/share/fonts` only if there is none -- and that
  fallback is a *warning*, not an error. Measured on this machine: delete the vendored
  `NotoSans-Regular.ttf` and the build still succeeds, quietly rasterising the host's Noto 2.004
  instead of the vendored 2.015, and all five compiled fonts change. A warning in a build log that
  nobody reads is not a defence, so the gate is here.
* **A repertoire the face cannot draw.** `FontDescriptionProcessor` fails the build when a
  `<CharacterRegion>` asks for a character the face has no glyph for. That is the right behaviour,
  but it is discovered only when the content pipeline runs; this gate finds it from the sources, in
  a second, without a content build.

    tools/ci/check_fonts.py             # check every .spritefont under assets-src/
    tools/ci/check_fonts.py --list      # also print each descriptor's resolved face and repertoire

**Standard library only, and the TrueType reader is ours.** Every other tool here parses its own
formats (`gltf_validate.py` reads `.glb` with `struct`); adding fontTools for one cmap table would
put a third-party package between a contributor and a green gate. Only what is needed is
implemented: the table directory, and `cmap` subtable formats 4 and 12.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import struct
import sys
import xml.etree.ElementTree as ElementTree
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ASSETS_SRC = REPO / "assets-src"

#: The repertoire every shipped face must cover (`HOUSE-00200`, `cna-house.md` §67.1: "English
#: only, but with the full Latin-1 set").
#:
#: U+00AD SOFT HYPHEN is excluded, and that exclusion is measured rather than chosen: Noto Sans Mono
#: 2.014 ships no glyph for it, and the processor treats a requested-but-absent character as a hard
#: build failure. It is an invisible line-break hint, there is no line-breaking engine here to
#: honour one, and a SpriteFont has nothing to draw for it -- so nothing is lost by leaving it out,
#: and both faces keep the same repertoire and stay diff-comparable.
SOFT_HYPHEN = 0xAD
REQUIRED = frozenset(range(0x20, 0x7F)) | (frozenset(range(0xA0, 0x100)) - {SOFT_HYPHEN})

#: Mirrors `FontDescriptionImporter`: the extensions it will try when `<FontName>` has none. The
#: importer also accepts upper-case spellings for the SYSTEM search only; a project-relative
#: candidate is tried in exactly this order.
FONT_EXTENSIONS = (".ttf", ".otf", ".ttc")

#: Where the importer looks when no file sits beside the descriptor. Named so the failure message
#: can say which host file would have been used instead, which is what makes the failure actionable.
SYSTEM_FONT_DIRECTORIES = (
    Path("/usr/share/fonts"),
    Path("/usr/local/share/fonts"),
    Path("/Library/Fonts"),
    Path("/System/Library/Fonts"),
)


# ------------------------------------------------------------------------------------------------
# A very small TrueType/OpenType reader: the table directory and the character map.
# ------------------------------------------------------------------------------------------------


class FontError(Exception):
    """The file is not a font this tool can read. Always fatal -- never downgraded to a warning."""


def _tables(data: bytes) -> dict[str, tuple[int, int]]:
    """Returns ``{tag: (offset, length)}`` from the table directory."""
    if len(data) < 12:
        raise FontError("file is too short to hold a table directory")
    tag = data[:4]
    if tag == b"ttcf":
        # A collection: use the first face. No .ttc is vendored today, but failing with "unsupported"
        # on one would be a worse message than simply reading its first font.
        (offset,) = struct.unpack(">I", data[12:16])
        return _tables(data[offset:] + b"")
    if tag not in (b"\x00\x01\x00\x00", b"OTTO", b"true", b"typ1"):
        raise FontError(f"unrecognised sfnt version {tag!r}")
    (count,) = struct.unpack(">H", data[4:6])
    out: dict[str, tuple[int, int]] = {}
    for index in range(count):
        base = 12 + index * 16
        if base + 16 > len(data):
            raise FontError("the table directory runs past the end of the file")
        name, _checksum, offset, length = struct.unpack(">4sIII", data[base : base + 16])
        out[name.decode("latin-1")] = (offset, length)
    return out


def _cmap_format_4(data: bytes, base: int) -> set[int]:
    (seg_x2,) = struct.unpack(">H", data[base + 6 : base + 8])
    segments = seg_x2 // 2
    ends = base + 14
    starts = ends + seg_x2 + 2
    deltas = starts + seg_x2
    ranges = deltas + seg_x2
    covered: set[int] = set()
    for index in range(segments):
        (end,) = struct.unpack(">H", data[ends + index * 2 : ends + index * 2 + 2])
        (start,) = struct.unpack(">H", data[starts + index * 2 : starts + index * 2 + 2])
        (delta,) = struct.unpack(">h", data[deltas + index * 2 : deltas + index * 2 + 2])
        offset_at = ranges + index * 2
        (range_offset,) = struct.unpack(">H", data[offset_at : offset_at + 2])
        if start == 0xFFFF:
            continue
        for code in range(start, end + 1):
            if range_offset == 0:
                glyph = (code + delta) & 0xFFFF
            else:
                at = offset_at + range_offset + (code - start) * 2
                if at + 2 > len(data):
                    continue
                (glyph,) = struct.unpack(">H", data[at : at + 2])
                if glyph != 0:
                    glyph = (glyph + delta) & 0xFFFF
            # Glyph 0 is `.notdef`. A cmap entry pointing at it is not coverage -- it is the
            # absence of coverage spelled out, and treating it as a hit is exactly how a missing
            # glyph gets past a checker.
            if glyph != 0:
                covered.add(code)
    return covered


def _cmap_format_12(data: bytes, base: int) -> set[int]:
    (groups,) = struct.unpack(">I", data[base + 12 : base + 16])
    covered: set[int] = set()
    for index in range(groups):
        at = base + 16 + index * 12
        start, end, glyph = struct.unpack(">III", data[at : at + 12])
        if glyph == 0:
            continue
        # Bounded: a format-12 table legitimately spans the whole of Unicode, and this tool only
        # ever asks about Latin-1.
        covered.update(range(start, min(end, 0x2FFFF) + 1))
    return covered


def coverage(path: Path) -> set[int]:
    """The set of Unicode code points @p path has a real (non-`.notdef`) glyph for."""
    data = path.read_bytes()
    tables = _tables(data)
    if "cmap" not in tables:
        raise FontError("no 'cmap' table, so the font maps no characters at all")
    cmap_at, _length = tables["cmap"]
    (count,) = struct.unpack(">H", data[cmap_at + 2 : cmap_at + 4])

    # Preference order: a Unicode full-repertoire table, then a Unicode BMP one, then the Windows
    # BMP table every practical font carries. Picking the first subtable instead would sometimes
    # land on a Macintosh Roman table and answer a different question.
    best: tuple[int, int] | None = None
    for index in range(count):
        at = cmap_at + 4 + index * 8
        platform, encoding, offset = struct.unpack(">HHI", data[at : at + 8])
        rank = {
            (3, 10): 4,
            (0, 4): 4,
            (0, 6): 4,
            (3, 1): 3,
            (0, 3): 3,
            (0, 2): 2,
            (0, 1): 2,
            (0, 0): 2,
        }.get((platform, encoding), 0)
        if rank and (best is None or rank > best[0]):
            best = (rank, cmap_at + offset)
    if best is None:
        raise FontError("no Unicode 'cmap' subtable")

    subtable = best[1]
    (fmt,) = struct.unpack(">H", data[subtable : subtable + 2])
    if fmt == 4:
        return _cmap_format_4(data, subtable)
    if fmt == 12:
        return _cmap_format_12(data, subtable)
    raise FontError(f"unsupported 'cmap' subtable format {fmt}")


# ------------------------------------------------------------------------------------------------
# The descriptors
# ------------------------------------------------------------------------------------------------


def requested_characters(descriptor: Path) -> tuple[str, set[int]]:
    """Returns ``(font_name, requested_code_points)`` for a `.spritefont`."""
    root = ElementTree.parse(descriptor).getroot()
    asset = root.find("Asset")
    if asset is None:
        raise FontError("no <Asset> element")
    name_element = asset.find("FontName")
    if name_element is None or not (name_element.text or "").strip():
        raise FontError("no <FontName>")
    font_name = (name_element.text or "").strip()

    requested: set[int] = set()
    for region in asset.iter("CharacterRegion"):
        start_element = region.find("Start")
        end_element = region.find("End")
        if start_element is None or end_element is None:
            raise FontError("a <CharacterRegion> is missing <Start> or <End>")
        # Empty text is reported, never allowed to reach `ord()`. A malformed descriptor is exactly
        # what this tool is for, and answering it with a stack trace would tell a contributor that
        # the tool is broken rather than that their file is.
        start_text = start_element.text or ""
        end_text = end_element.text or ""
        if not start_text or not end_text:
            raise FontError(
                "a <CharacterRegion> has an empty <Start> or <End>; each must hold exactly one "
                "character, normally as a numeric entity such as <Start>&#32;</Start>"
            )
        start = ord(start_text[:1])
        end = ord(end_text[:1])
        if end < start:
            raise FontError(f"a <CharacterRegion> runs backwards: U+{start:04X}..U+{end:04X}")
        requested.update(range(start, end + 1))
    if not requested:
        raise FontError("no <CharacterRegion>, so the font would contain no glyphs")
    return font_name, requested


def resolve_beside(descriptor: Path, font_name: str) -> Path | None:
    """The file the importer would use from beside @p descriptor, or None if there is none."""
    for candidate in (font_name, *(font_name + ext for ext in FONT_EXTENSIONS)):
        probe = descriptor.parent / candidate
        if probe.is_file():
            return probe
    return None


def find_system_font(font_name: str) -> Path | None:
    """What `FindSystemFont` would return -- used only to make a failure message concrete."""
    wanted = "".join(c.lower() for c in font_name if c not in " -_")
    candidates: list[Path] = []
    for directory in SYSTEM_FONT_DIRECTORIES:
        if not directory.is_dir():
            continue
        for path in directory.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in FONT_EXTENSIONS:
                continue
            if "".join(c.lower() for c in path.stem if c not in " -_") == wanted:
                candidates.append(path)
    return min(candidates) if candidates else None


def check() -> list[str]:
    problems: list[str] = []
    descriptors = sorted(ASSETS_SRC.rglob("*.spritefont"))
    if not descriptors:
        return ["no .spritefont under assets-src/; HOUSE-00200 authored five"]

    for descriptor in descriptors:
        where = descriptor.relative_to(REPO)
        try:
            font_name, requested = requested_characters(descriptor)
        except (FontError, ElementTree.ParseError) as error:
            problems.append(f"{where}: {error}")
            continue

        face = resolve_beside(descriptor, font_name)
        if face is None:
            system = find_system_font(font_name)
            detail = (
                f"the build would SILENTLY use the host's '{system}' instead, with only a warning"
                if system
                else "the content build would fail outright"
            )
            problems.append(
                f"{where}: <FontName> '{font_name}' names no font file beside the descriptor, so "
                f"{detail}. Vendor the .ttf next to the .spritefont (HOUSE-00200)."
            )
            continue

        missing_required = REQUIRED - requested
        if missing_required:
            problems.append(
                f"{where}: the <CharacterRegions> omit "
                f"{_describe(missing_required)} from the required Latin-1 repertoire"
            )

        try:
            covered = coverage(face)
        except FontError as error:
            problems.append(f"{face.relative_to(REPO)}: {error}")
            continue

        uncoverable = requested - covered
        if uncoverable:
            problems.append(
                f"{where}: '{face.name}' has no glyph for {_describe(uncoverable)}, which the "
                f"<CharacterRegions> ask for. The content build fails on this."
            )
    return problems


def _describe(codes: set[int]) -> str:
    shown = ", ".join(f"U+{c:04X}" for c in sorted(codes)[:8])
    return shown + (f" (+{len(codes) - 8} more)" if len(codes) > 8 else "")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--list", action="store_true", help="print each descriptor's face and repertoire"
    )
    args = parser.parse_args()

    if args.list:
        for descriptor in sorted(ASSETS_SRC.rglob("*.spritefont")):
            try:
                font_name, requested = requested_characters(descriptor)
            except (FontError, ElementTree.ParseError) as error:
                print(f"{descriptor.relative_to(REPO)}: {error}")
                continue
            face = resolve_beside(descriptor, font_name)
            resolved = face.relative_to(REPO) if face else "** NOT VENDORED **"
            print(f"{descriptor.relative_to(REPO)}")
            print(f"    face       {resolved}")
            print(f"    characters {len(requested)}")

    problems = check()
    if problems:
        print(f"check_fonts: {len(problems)} problem(s):", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1
    count = len(list(ASSETS_SRC.rglob("*.spritefont")))
    print(
        f"check_fonts: {count} descriptor(s) resolve to a vendored face and cover the "
        f"{len(REQUIRED)}-character Latin-1 repertoire."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
