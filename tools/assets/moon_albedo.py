#!/usr/bin/env python3
"""moon_albedo.py -- prepare §33.3's 1024-square lunar albedo texture.

`HOUSE-01605`.  NASA SVS publishes the LRO colour mosaic as a 2:1 equirectangular map.  The Tier-S
moon is a camera-facing quad rather than a sphere, so this tool makes the required orthographic
view of the near side, converts the sRGB colour mosaic to one greyscale albedo channel, and writes
a deterministic RGBA8 PNG.  Pixels outside the geometric disc extend the nearest limb texel; the
separate `MoonMask` supplies the disc alpha, and the extension prevents a black bilinear fringe.

    tools/assets/moon_albedo.py --emit --source /path/to/lroc_color_poles_2k.tif
    tools/assets/moon_albedo.py --fetch
    tools/assets/moon_albedo.py --check
    tools/assets/moon_albedo.py --report
    tools/assets/moon_albedo.py --selftest

Only `--fetch` uses the network.  Normal builds and CI verify the committed output entirely
offline.  Offline tooling is not runtime code and is not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import math
import struct
import sys
import tempfile
import urllib.request
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OUTPUT = REPO / "assets-src" / "Textures" / "Sky" / "moon_albedo.png"
SOURCE_URL = (
    "https://svs.gsfc.nasa.gov/vis/a000000/a004700/a004720/"
    "lroc_color_poles_2k.tif"
)
SOURCE_SHA256 = "13b797422e8c4b8607ff2b2623ac3a046a6da0132d567c2d272d92fad7052c4a"
EXPECTED_OUTPUT_SHA256 = "d7fa53d450a40b605ab29dd28a51b9be4b43cae1b10481d5f9a91f428b851e75"
SOURCE_WIDTH = 2048
SOURCE_HEIGHT = 1024
SIZE = 1024


class AlbedoError(RuntimeError):
    pass


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    body = kind + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def encode_png(pixels: bytes) -> bytes:
    """Encode fixed-size RGBA8 using stable filter-none rows."""
    if len(pixels) != SIZE * SIZE * 4:
        raise AlbedoError("the output pixel buffer is not 1024-square RGBA8")
    stride = SIZE * 4
    scanlines = bytearray()
    for y in range(SIZE):
        scanlines.append(0)
        scanlines.extend(pixels[y * stride:(y + 1) * stride])
    header = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(scanlines, 9))
        + png_chunk(b"IEND", b"")
    )


def load_image(data: bytes, description: str) -> tuple[int, int, bytes]:
    try:
        from PIL import Image
    except ImportError as error:  # pragma: no cover - an actionable host-tool failure
        raise AlbedoError(f"Pillow is required to read {description}: {error}") from error
    try:
        with Image.open(io.BytesIO(data)) as image:
            rgba = image.convert("RGBA")
            return rgba.width, rgba.height, rgba.tobytes()
    except Exception as error:  # pragma: no cover - Pillow supplies format-specific detail
        raise AlbedoError(f"could not read {description}: {error}") from error


def source_coordinates(nx: float, ny: float, width: int, height: int) -> tuple[float, float]:
    """Map an orthographic near-side point to equirectangular source pixel coordinates."""
    radius = math.hypot(nx, ny)
    if radius >= 1.0:
        scale = (1.0 - 1.0e-12) / radius
        nx *= scale
        ny *= scale
    front = math.sqrt(max(0.0, 1.0 - nx * nx - ny * ny))
    longitude = math.atan2(nx, front)
    latitude = math.asin(max(-1.0, min(1.0, ny)))
    return (
        (longitude / (2.0 * math.pi) + 0.5) * width - 0.5,
        (0.5 - latitude / math.pi) * height - 0.5,
    )


def greyscale_plane(rgba: bytes) -> bytearray:
    """Rec.709 sRGB luma, with integer weights summing exactly to 256."""
    result = bytearray(len(rgba) // 4)
    for index in range(len(result)):
        offset = index * 4
        result[index] = (
            54 * rgba[offset] + 183 * rgba[offset + 1] + 19 * rgba[offset + 2] + 128
        ) >> 8
    return result


def bilinear(source: bytearray, width: int, height: int, x: float, y: float) -> int:
    x0 = math.floor(x)
    y0 = math.floor(y)
    tx = x - x0
    ty = y - y0
    x0 %= width
    x1 = (x0 + 1) % width
    y0 = max(0, min(height - 1, y0))
    y1 = max(0, min(height - 1, y0 + 1))
    top = source[y0 * width + x0] * (1.0 - tx) + source[y0 * width + x1] * tx
    bottom = source[y1 * width + x0] * (1.0 - tx) + source[y1 * width + x1] * tx
    return max(0, min(255, round(top * (1.0 - ty) + bottom * ty)))


def prepare(source_data: bytes) -> tuple[bytes, bytearray]:
    if sha256(source_data) != SOURCE_SHA256:
        raise AlbedoError(
            "source SHA-256 differs from the reviewed NASA TIFF; refusing an unreviewed asset"
        )
    width, height, rgba = load_image(source_data, "NASA lunar colour TIFF")
    if (width, height) != (SOURCE_WIDTH, SOURCE_HEIGHT):
        raise AlbedoError(
            f"source is {width}x{height}, expected {SOURCE_WIDTH}x{SOURCE_HEIGHT}"
        )
    grey = greyscale_plane(rgba)
    output = bytearray(SIZE * SIZE * 4)
    for y in range(SIZE):
        ny = 1.0 - (2.0 * (y + 0.5) / SIZE)
        for x in range(SIZE):
            nx = 2.0 * (x + 0.5) / SIZE - 1.0
            sx, sy = source_coordinates(nx, ny, width, height)
            value = bilinear(grey, width, height, sx, sy)
            offset = (y * SIZE + x) * 4
            output[offset:offset + 4] = bytes((value, value, value, 255))
    return encode_png(output), output


def decoded_output() -> tuple[bytes, bytearray]:
    if not OUTPUT.is_file():
        raise AlbedoError(f"missing {OUTPUT.relative_to(REPO)}")
    encoded = OUTPUT.read_bytes()
    width, height, rgba = load_image(encoded, "committed lunar albedo PNG")
    if (width, height) != (SIZE, SIZE):
        raise AlbedoError(f"output is {width}x{height}, expected {SIZE}x{SIZE}")
    return encoded, bytearray(rgba)


def metrics(pixels: bytearray) -> dict[str, float | int]:
    values = pixels[0::4]
    return {
        "minimum": min(values),
        "maximum": max(values),
        "mean": sum(values) / len(values),
        "distinct": len(set(values)),
    }


def validate_pixels(pixels: bytearray) -> list[str]:
    problems: list[str] = []
    if any(pixels[index] != pixels[index + 1] or pixels[index] != pixels[index + 2]
           for index in range(0, len(pixels), 4)):
        problems.append("RGB channels are not identical greyscale values")
    if any(pixels[index] != 255 for index in range(3, len(pixels), 4)):
        problems.append("albedo alpha is not fully opaque; MoonMask owns disc opacity")
    summary = metrics(pixels)
    if summary["minimum"] > 60 or summary["maximum"] < 220 or summary["distinct"] < 180:
        problems.append(f"implausibly flat lunar contrast: {summary}")
    return problems


def print_report(encoded: bytes, pixels: bytearray) -> None:
    summary = metrics(pixels)
    print(
        f"moon_albedo.png: {SIZE}x{SIZE} RGBA greyscale, {len(encoded):,} bytes, "
        f"value {summary['minimum']}..{summary['maximum']} mean {summary['mean']:.2f}, "
        f"{summary['distinct']} levels, sha256 {sha256(encoded)}"
    )


def check() -> int:
    try:
        encoded, pixels = decoded_output()
    except AlbedoError as error:
        print(f"moon_albedo: {error}", file=sys.stderr)
        return 1
    problems = validate_pixels(pixels)
    actual = sha256(encoded)
    if actual != EXPECTED_OUTPUT_SHA256:
        problems.append(
            f"output SHA-256 is {actual}, expected {EXPECTED_OUTPUT_SHA256 or '<not pinned>'}"
        )
    if problems:
        print("moon_albedo: output is stale or invalid:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1
    print_report(encoded, pixels)
    return 0


def selftest() -> int:
    failures = 0

    def require(condition: bool, message: str) -> None:
        nonlocal failures
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        failures += 0 if condition else 1

    print("moon_albedo: selftest")
    width, height = 2048, 1024
    centre = source_coordinates(0.0, 0.0, width, height)
    left = source_coordinates(-1.0, 0.0, width, height)
    right = source_coordinates(1.0, 0.0, width, height)
    north = source_coordinates(0.0, 1.0, width, height)
    require(abs(centre[0] - 1023.5) < 1.0e-9 and abs(centre[1] - 511.5) < 1.0e-9,
            "disc centre samples 0 degrees longitude and latitude")
    require(abs(left[0] - 511.5) < 1.0e-3 and abs(right[0] - 1535.5) < 1.0e-3,
            "left and right limbs sample -90 and +90 degrees longitude")
    require(abs(north[0] - 1023.5) < 1.0e-9 and abs(north[1] + 0.5) < 1.0e-3,
            "top limb samples the north pole without flipping latitude")
    sample = bytearray((0, 100, 200, 255))
    require(bilinear(sample, 4, 1, 1.5, 0.0) == 150,
            "bilinear sampling interpolates neighbouring albedo texels")
    test_pixels = bytes((17, 17, 17, 255)) * (SIZE * SIZE)
    encoded = encode_png(test_pixels)
    require(encoded == encode_png(test_pixels), "PNG encoding is byte-deterministic")
    decoded_width, decoded_height, decoded = load_image(encoded, "selftest PNG")
    require((decoded_width, decoded_height) == (SIZE, SIZE) and decoded == test_pixels,
            "the deterministic RGBA8 PNG round-trips exactly")
    print("moon_albedo: selftest passed." if not failures
          else f"moon_albedo: {failures} claim(s) FAILED")
    return 1 if failures else 0


def emit_from(source_path: Path) -> int:
    try:
        encoded, pixels = prepare(source_path.read_bytes())
    except (OSError, AlbedoError) as error:
        print(f"moon_albedo: {error}", file=sys.stderr)
        return 1
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes(encoded)
    print_report(encoded, pixels)
    return 0


def fetch_and_emit() -> int:
    try:
        with urllib.request.urlopen(SOURCE_URL, timeout=60) as response:
            source = response.read()
    except OSError as error:
        print(f"moon_albedo: download failed: {error}", file=sys.stderr)
        return 1
    with tempfile.NamedTemporaryFile(suffix=".tif") as temporary:
        temporary.write(source)
        temporary.flush()
        return emit_from(Path(temporary.name))


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--emit", action="store_true")
    parser.add_argument("--source", type=Path)
    parser.add_argument("--fetch", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)
    if sum((args.emit, args.fetch, args.check, args.report, args.selftest)) != 1:
        parser.error("choose exactly one of --emit, --fetch, --check, --report or --selftest")
    if args.source is not None and not args.emit:
        parser.error("--source is used only with --emit")
    if args.emit:
        if args.source is None:
            parser.error("--emit requires --source; use --fetch to retrieve the reviewed source")
        return emit_from(args.source)
    if args.fetch:
        return fetch_and_emit()
    if args.check:
        return check()
    if args.report:
        try:
            encoded, pixels = decoded_output()
        except AlbedoError as error:
            print(f"moon_albedo: {error}", file=sys.stderr)
            return 1
        print_report(encoded, pixels)
        return 0
    return selftest()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
