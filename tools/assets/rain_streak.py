#!/usr/bin/env python3
"""Generate HOUSE-01743's small, project-authored rain streak texture."""

from __future__ import annotations

import argparse
import binascii
import math
from pathlib import Path
import struct
import sys
import zlib


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "assets-src/Textures/Weather/rain_streak.png"
WIDTH = 16
HEIGHT = 64


def chunk(kind: bytes, payload: bytes) -> bytes:
    body = kind + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(">I", binascii.crc32(body) & 0xFFFFFFFF)


def texture_bytes() -> bytes:
    rows = bytearray()
    for y in range(HEIGHT):
        rows.append(0)  # PNG filter: none
        vertical = math.sin(math.pi * (y + 0.5) / HEIGHT) ** 0.45
        for x in range(WIDTH):
            centre_distance = abs((x + 0.5) - WIDTH * 0.5) / (WIDTH * 0.5)
            horizontal = math.exp(-9.0 * centre_distance * centre_distance)
            alpha = round(210.0 * horizontal * vertical)
            rows.extend((255, 255, 255, alpha))
    signature = b"\x89PNG\r\n\x1a\n"
    header = struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 6, 0, 0, 0)
    return signature + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b"")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if the committed texture differs")
    args = parser.parse_args()
    expected = texture_bytes()
    if args.check:
        if not OUTPUT.is_file() or OUTPUT.read_bytes() != expected:
            print(f"rain_streak.py: stale or missing {OUTPUT.relative_to(ROOT)}", file=sys.stderr)
            return 1
        print(f"rain_streak.py: {OUTPUT.relative_to(ROOT)} is current")
        return 0
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes(expected)
    print(f"rain_streak.py: wrote {OUTPUT.relative_to(ROOT)} ({WIDTH} x {HEIGHT})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
