#!/usr/bin/env python3
"""build_stars.py -- build §34's 1,500 brightest complete BSC5P star records.

`HOUSE-01609`.  The committed source is a pipe-delimited export of the public NASA HEASARC
`bsc5p` table.  Generation is offline and deterministic; `--fetch` is the explicit network path
that verifies the upstream response before using it.

    tools/world/build_stars.py --emit
    tools/world/build_stars.py --check
    tools/world/build_stars.py --report
    tools/world/build_stars.py --selftest
    tools/world/build_stars.py --fetch

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import math
import re
import struct
import sys
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "assets-src" / "world" / "bsc5p.psv"
DEFAULT_OUT = REPO / "content" / "world" / "stars.bin"

SOURCE_ENDPOINT = "https://heasarc.gsfc.nasa.gov/xamin/cli"
SOURCE_PARAMETERS = (
    ("table", "bsc5p"),
    ("fields", "name,ra,dec,vmag,bv_color"),
    ("format", "stream"),
    ("sortvar", "name"),
    ("resultmax", "10000"),
)
SOURCE_URL = SOURCE_ENDPOINT + "?" + urllib.parse.urlencode(SOURCE_PARAMETERS)
SOURCE_SHA256 = "31464f3928a834a44c1a7b1c960081550357b6e4134c2ff568223d6b03e865b7"
SOURCE_ROW_COUNT = 9110
SOURCE_COLUMNS = ("name", "ra", "dec", "vmag", "bv_color")

MAGIC = b"CSTR"
VERSION = 1
FLAGS = 0
STAR_COUNT = 1500
HEADER = struct.Struct("<4sIII")
RECORD = struct.Struct("<4f")

# Populated after the first reviewed generation.  `--check` pins the derived bytes as well as the
# downloaded source, so a selection or packing change is an explicit format change.
EXPECTED_CATALOGUE_SHA256 = "cf1145ec49855acced63cbb509f044b02977cd3a7233c3d0921145d5a30d4a20"

# HEASARC identifies these HR rows as non-stellar objects on the BSC5P table page.  None happens
# to cross the 1,500-row cutoff, but filtering before sorting makes the contract true independent
# of their magnitudes and guards future upstream revisions.
NON_STELLAR_HR = frozenset(
    {92, 95, 182, 1057, 1841, 2472, 2496, 3515, 3671, 6309, 6515, 7189, 7539, 8296}
)

HR_RE = re.compile(r"^HR ([1-9][0-9]{0,3})$")


class CatalogueError(ValueError):
    """The source or generated catalogue violates its documented contract."""


@dataclass(frozen=True)
class SourceStar:
    hr: int
    ra_deg: float
    dec_deg: float
    magnitude: float | None
    bv: float | None


@dataclass(frozen=True)
class Star:
    ra_deg: float
    dec_deg: float
    magnitude: float
    bv: float


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_ra(value: str, where: str) -> float:
    try:
        hour_text, minute_text, second_text = value.split()
        hour, minute, second = int(hour_text), int(minute_text), float(second_text)
    except (TypeError, ValueError) as error:
        raise CatalogueError(f"{where}: invalid right ascension {value!r}") from error
    if not (0 <= hour < 24 and 0 <= minute < 60 and 0.0 <= second < 60.0):
        raise CatalogueError(f"{where}: right ascension is outside 00:00:00..23:59:59")
    return 15.0 * (hour + minute / 60.0 + second / 3600.0)


def parse_dec(value: str, where: str) -> float:
    try:
        degree_text, minute_text, second_text = value.split()
        sign = -1.0 if degree_text.startswith("-") else 1.0
        degree, minute, second = abs(int(degree_text)), int(minute_text), float(second_text)
    except (TypeError, ValueError) as error:
        raise CatalogueError(f"{where}: invalid declination {value!r}") from error
    if (not 0 <= degree <= 90 or not 0 <= minute < 60 or not 0.0 <= second < 60.0
            or (degree == 90 and (minute != 0 or second != 0.0))):
        raise CatalogueError(f"{where}: declination is outside -90..+90 degrees")
    return sign * (degree + minute / 60.0 + second / 3600.0)


def optional_float(value: str, field: str, where: str) -> float | None:
    if not value.strip():
        return None
    try:
        result = float(value)
    except ValueError as error:
        raise CatalogueError(f"{where}: {field} is not numeric: {value!r}") from error
    if not math.isfinite(result):
        raise CatalogueError(f"{where}: {field} is not finite")
    return result


def parse_source(data: bytes) -> list[SourceStar]:
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as error:
        raise CatalogueError("BSC5P source is not UTF-8") from error
    reader = csv.DictReader(io.StringIO(text, newline=""), delimiter="|")
    if tuple(reader.fieldnames or ()) != SOURCE_COLUMNS:
        raise CatalogueError(
            f"BSC5P columns are {tuple(reader.fieldnames or ())}, expected {SOURCE_COLUMNS}"
        )

    stars: list[SourceStar] = []
    seen: set[int] = set()
    for line, row in enumerate(reader, start=2):
        if None in row:
            raise CatalogueError(f"line {line}: too many pipe-delimited fields")
        if any(row[column] is None for column in SOURCE_COLUMNS):
            raise CatalogueError(f"line {line}: too few pipe-delimited fields")
        match = HR_RE.fullmatch(row["name"])
        if match is None:
            raise CatalogueError(f"line {line}: invalid BSC identifier {row['name']!r}")
        hr = int(match.group(1))
        if hr in seen:
            raise CatalogueError(f"line {line}: duplicate HR {hr}")
        seen.add(hr)
        where = f"line {line} / HR {hr}"
        stars.append(
            SourceStar(
                hr,
                parse_ra(row["ra"], where),
                parse_dec(row["dec"], where),
                optional_float(row["vmag"], "V magnitude", where),
                optional_float(row["bv_color"], "B-V colour", where),
            )
        )

    if len(stars) != SOURCE_ROW_COUNT:
        raise CatalogueError(f"BSC5P has {len(stars)} rows, expected {SOURCE_ROW_COUNT}")
    expected_ids = set(range(1, SOURCE_ROW_COUNT + 1))
    if seen != expected_ids:
        missing = sorted(expected_ids - seen)[:5]
        extra = sorted(seen - expected_ids)[:5]
        raise CatalogueError(f"BSC5P HR sequence is incomplete (missing {missing}, extra {extra})")
    return stars


def select(source: list[SourceStar]) -> tuple[list[SourceStar], list[Star]]:
    eligible = [
        star for star in source
        if star.hr not in NON_STELLAR_HR and star.magnitude is not None and star.bv is not None
    ]
    eligible.sort(key=lambda star: (star.magnitude, star.hr))
    if len(eligible) < STAR_COUNT:
        raise CatalogueError(f"only {len(eligible)} complete stellar rows; need {STAR_COUNT}")
    selected = eligible[:STAR_COUNT]
    records = [Star(star.ra_deg, star.dec_deg, star.magnitude, star.bv) for star in selected]
    return selected, records


def encode(stars: list[Star]) -> bytes:
    if len(stars) != STAR_COUNT:
        raise CatalogueError(f"catalogue has {len(stars)} stars, expected {STAR_COUNT}")
    output = bytearray(HEADER.pack(MAGIC, VERSION, FLAGS, len(stars)))
    for star in stars:
        output += RECORD.pack(star.ra_deg, star.dec_deg, star.magnitude, star.bv)
    return bytes(output)


def decode(data: bytes) -> list[Star]:
    if len(data) < HEADER.size:
        raise CatalogueError("stars.bin is truncated before its header")
    magic, version, flags, count = HEADER.unpack_from(data)
    if magic != MAGIC:
        raise CatalogueError("not a stars.bin: bad magic")
    if version != VERSION:
        raise CatalogueError(f"stars.bin is version {version}; this reader knows {VERSION}")
    if flags != FLAGS:
        raise CatalogueError(f"stars.bin sets unknown flag bits {flags:#x}")
    if count != STAR_COUNT:
        raise CatalogueError(f"stars.bin declares {count} stars, expected {STAR_COUNT}")
    wanted = HEADER.size + count * RECORD.size
    if len(data) != wanted:
        detail = "truncated" if len(data) < wanted else "has trailing bytes"
        raise CatalogueError(f"stars.bin {detail}: {len(data)} bytes, expected {wanted}")

    stars = []
    offset = HEADER.size
    for _ in range(count):
        stars.append(Star(*RECORD.unpack_from(data, offset)))
        offset += RECORD.size
    return stars


def validation_problems(source: list[SourceStar], selected: list[SourceStar],
                        records: list[Star], encoded: bytes) -> list[str]:
    problems: list[str] = []
    if len(selected) != STAR_COUNT or len(records) != STAR_COUNT:
        problems.append(f"selection has {len(selected)} source rows / {len(records)} records")
    if any(star.hr in NON_STELLAR_HR for star in selected):
        problems.append("selection contains one of HEASARC's 14 non-stellar HR objects")
    if any(star.magnitude is None or star.bv is None for star in selected):
        problems.append("selection contains a missing magnitude or B-V colour")
    if any(selected[index - 1].magnitude > selected[index].magnitude
           for index in range(1, len(selected))):
        problems.append("selection is not ordered brightest-first")
    if selected and abs(selected[-1].magnitude - 4.94) > 1.0e-6:
        problems.append(f"faintest selected magnitude is {selected[-1].magnitude}, expected 4.94")
    for index, star in enumerate(records):
        values = (star.ra_deg, star.dec_deg, star.magnitude, star.bv)
        if not all(math.isfinite(value) for value in values):
            problems.append(f"star {index} contains a non-finite value")
            break
        if not (0.0 <= star.ra_deg < 360.0 and -90.0 <= star.dec_deg <= 90.0):
            problems.append(f"star {index} has invalid equatorial coordinates")
            break
        if not (-2.0 <= star.magnitude <= 5.5 and -0.5 <= star.bv <= 3.0):
            problems.append(f"star {index} has implausible magnitude or B-V colour")
            break

    by_hr = {star.hr: star for star in selected}
    known = {
        2491: (101.287083, -16.716111, -1.46, 0.00),  # Sirius
        2326: (95.987917, -52.695806, -0.72, 0.15),   # Canopus
        7001: (279.234583, 38.783611, 0.03, 0.00),    # Vega
        424: (37.952917, 89.264194, 2.02, 0.60),      # Polaris
    }
    for hr, expected in known.items():
        star = by_hr.get(hr)
        actual = None if star is None else (star.ra_deg, star.dec_deg, star.magnitude, star.bv)
        if actual is None or any(abs(left - right) > 1.0e-5
                                 for left, right in zip(actual, expected)):
            problems.append(f"HR {hr} fixture is {actual}, expected {expected}")

    try:
        decoded = decode(encoded)
    except CatalogueError as error:
        problems.append(str(error))
    else:
        if encode(decoded) != encoded:
            problems.append("binary does not round-trip byte-for-byte through binary32")
    return problems


def canonical(source_data: bytes) -> tuple[list[SourceStar], list[SourceStar], list[Star], bytes]:
    source = parse_source(source_data)
    selected, records = select(source)
    encoded = encode(records)
    problems = validation_problems(source, selected, records, encoded)
    if problems:
        raise CatalogueError("; ".join(problems))
    return source, selected, records, encoded


def print_report(source: list[SourceStar], selected: list[SourceStar], encoded: bytes) -> None:
    missing_magnitude = sum(star.magnitude is None for star in source)
    missing_bv = sum(star.bv is None for star in source)
    print(
        f"stars.bin: {len(selected)} stars, {len(encoded):,} bytes, "
        f"V {selected[0].magnitude:.2f}..{selected[-1].magnitude:.2f}, "
        f"source {len(source)} rows ({missing_magnitude} missing V, {missing_bv} missing B-V), "
        f"sha256 {sha256(encoded)}"
    )


def read_canonical_source(path: Path, *, require_pin: bool) -> bytes:
    try:
        data = path.read_bytes()
    except OSError as error:
        raise CatalogueError(f"cannot read {path}: {error}") from error
    if require_pin and sha256(data) != SOURCE_SHA256:
        raise CatalogueError(
            f"source SHA-256 is {sha256(data)}, expected reviewed {SOURCE_SHA256}"
        )
    return data


def emit(source_path: Path, out: Path, *, require_pin: bool) -> int:
    try:
        source, selected, _records, encoded = canonical(
            read_canonical_source(source_path, require_pin=require_pin)
        )
    except CatalogueError as error:
        print(f"build_stars: {error}", file=sys.stderr)
        return 1
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(encoded)
    print_report(source, selected, encoded)
    return 0


def check() -> int:
    try:
        source, selected, _records, encoded = canonical(
            read_canonical_source(SOURCE, require_pin=True)
        )
    except CatalogueError as error:
        print(f"build_stars: {error}", file=sys.stderr)
        return 1
    actual_hash = sha256(encoded)
    problems = []
    if actual_hash != EXPECTED_CATALOGUE_SHA256:
        problems.append(
            f"generated SHA-256 is {actual_hash}, expected {EXPECTED_CATALOGUE_SHA256}"
        )
    if DEFAULT_OUT.is_file() and DEFAULT_OUT.read_bytes() != encoded:
        problems.append(f"{DEFAULT_OUT.relative_to(REPO)} is stale; run build_stars.py --emit")
    if problems:
        print("build_stars: catalogue is stale or invalid:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1
    print_report(source, selected, encoded)
    return 0


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("build_stars: selftest")
    try:
        source, selected, records, encoded = canonical(
            read_canonical_source(SOURCE, require_pin=True)
        )
    except CatalogueError as error:
        print(f"  FAIL  canonical source: {error}")
        return 1

    require(len(source) == 9110 and len(selected) == len(records) == 1500,
            "9,110 BSC5P rows deterministically select exactly 1,500 complete stars")
    require(selected[0].hr == 2491 and selected[1].hr == 2326,
            "Sirius and Canopus are the two brightest catalogue rows")
    require(424 in {star.hr for star in selected},
            "Polaris remains present for the sidereal-rotation acceptance test")
    require(len(encoded) == HEADER.size + STAR_COUNT * RECORD.size == 24016,
            "the packed file is one 16-byte header plus 1,500 four-float records")
    require(encode(decode(encoded)) == encoded,
            "the binary round-trips byte-for-byte")
    require(encode(records) == encode(records), "generation is byte-deterministic")

    for label, damaged in (
        ("bad magic", b"NOPE" + encoded[4:]),
        ("unknown version", encoded[:4] + struct.pack("<I", 2) + encoded[8:]),
        ("unknown flags", encoded[:8] + struct.pack("<I", 1) + encoded[12:]),
        ("wrong count", encoded[:12] + struct.pack("<I", 1499) + encoded[16:]),
        ("truncation", encoded[:-1]),
        ("trailing bytes", encoded + b"x"),
    ):
        try:
            decode(damaged)
            refused = False
        except CatalogueError:
            refused = True
        require(refused, f"the reader refuses {label}")

    altered_header = b"wrong|header\n" + SOURCE.read_bytes().split(b"\n", 1)[1]
    try:
        parse_source(altered_header)
        refused = False
    except CatalogueError:
        refused = True
    require(refused, "the source parser refuses a changed HEASARC column contract")

    print("build_stars: selftest passed." if not failures
          else f"build_stars: {len(failures)} claim(s) FAILED")
    return 1 if failures else 0


def fetch(out: Path) -> int:
    request = urllib.request.Request(SOURCE_URL, headers={"User-Agent": "cna-house/1"})
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            data = response.read()
    except OSError as error:
        print(f"build_stars: download failed: {error}", file=sys.stderr)
        return 1
    if sha256(data) != SOURCE_SHA256:
        print(
            f"build_stars: downloaded SHA-256 is {sha256(data)}, expected {SOURCE_SHA256}; "
            "review the upstream revision before changing the pin",
            file=sys.stderr,
        )
        return 1
    if not SOURCE.is_file() or SOURCE.read_bytes() != data:
        print("build_stars: reviewed download differs from the committed source snapshot",
              file=sys.stderr)
        return 1
    try:
        source, selected, _records, encoded = canonical(data)
    except CatalogueError as error:
        print(f"build_stars: {error}", file=sys.stderr)
        return 1
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(encoded)
    print_report(source, selected, encoded)
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--emit", action="store_true")
    modes.add_argument("--check", action="store_true")
    modes.add_argument("--report", action="store_true")
    modes.add_argument("--selftest", action="store_true")
    modes.add_argument("--fetch", action="store_true")
    parser.add_argument("--source", type=Path, default=SOURCE)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args(argv)

    if args.source != SOURCE and not args.emit:
        parser.error("--source is used only with --emit")
    if args.out != DEFAULT_OUT and not (args.emit or args.fetch or args.report):
        parser.error("--out is used only with --emit, --fetch or --report")
    if args.emit:
        return emit(args.source, args.out, require_pin=args.source == SOURCE)
    if args.check:
        return check()
    if args.selftest:
        return selftest()
    if args.fetch:
        return fetch(args.out)
    try:
        data = args.out.read_bytes()
        records = decode(data)
    except (OSError, CatalogueError) as error:
        print(f"build_stars: {error}", file=sys.stderr)
        return 1
    print(
        f"stars.bin: {len(records)} stars, {len(data):,} bytes, "
        f"V {records[0].magnitude:.2f}..{records[-1].magnitude:.2f}, sha256 {sha256(data)}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
