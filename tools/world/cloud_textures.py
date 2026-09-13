#!/usr/bin/env python3
"""cloud_textures.py -- generate §31.3's three tileable RGBA cloud textures.

`HOUSE-01646`.  The source PNGs are deterministic project-owned content rather than downloaded
photographs: periodic value-noise fields make the opposite edges continuous, and three different
density profiles produce high cirrus, mid-level cumulus and low stratus.  RGB stays white so
`HOUSE-01649` can tint the layers from the sky; opacity alone carries the cloud structure.

    tools/world/cloud_textures.py --emit
    tools/world/cloud_textures.py --check
    tools/world/cloud_textures.py --report
    tools/world/cloud_textures.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import struct
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OUTPUT_DIR = REPO / "assets-src" / "Textures" / "Sky"
SIZE = 1024
WORK_SIZE = 256
UPSCALE = SIZE // WORK_SIZE


@dataclass(frozen=True)
class TextureSpec:
    name: str
    seed: int
    cells: tuple[tuple[int, int], ...]
    weights: tuple[float, ...]
    transparent_at: float
    opaque_at: float


SPECS = (
    TextureSpec(
        "cloud_cirrus.png",
        0xC1A255,
        ((3, 12), (6, 24), (12, 48), (24, 96)),
        (0.48, 0.27, 0.16, 0.09),
        0.48,
        0.72,
    ),
    TextureSpec(
        "cloud_cumulus.png",
        0xC0A117,
        ((4, 4), (8, 8), (16, 16), (32, 32), (64, 64)),
        (0.44, 0.25, 0.15, 0.10, 0.06),
        0.43,
        0.62,
    ),
    TextureSpec(
        "cloud_stratus.png",
        0x57A7A5,
        ((3, 6), (6, 12), (12, 24), (24, 48), (48, 96)),
        (0.48, 0.25, 0.14, 0.08, 0.05),
        0.28,
        0.55,
    ),
)


def hash_value(x: int, y: int, seed: int) -> float:
    """A stable integer hash mapped to [0, 1]; no interpreter PRNG state is involved."""
    value = (seed ^ (x * 0x9E3779B1) ^ (y * 0x85EBCA77)) & 0xFFFFFFFF
    value ^= value >> 16
    value = (value * 0x7FEB352D) & 0xFFFFFFFF
    value ^= value >> 15
    value = (value * 0x846CA68B) & 0xFFFFFFFF
    value ^= value >> 16
    return value / 0xFFFFFFFF


def axis_samples(cells: int) -> list[tuple[int, int, float]]:
    result = []
    for coordinate in range(WORK_SIZE):
        scaled = coordinate * cells / WORK_SIZE
        low = math.floor(scaled)
        fraction = scaled - low
        smooth = fraction * fraction * (3.0 - 2.0 * fraction)
        result.append((low % cells, (low + 1) % cells, smooth))
    return result


def value_noise(cells_x: int, cells_y: int, seed: int) -> list[float]:
    """One smooth periodic field; the lattice wraps before interpolation."""
    lattice = [
        hash_value(x, y, seed)
        for y in range(cells_y)
        for x in range(cells_x)
    ]
    xs = axis_samples(cells_x)
    ys = axis_samples(cells_y)
    field = [0.0] * (WORK_SIZE * WORK_SIZE)
    for y, (y0, y1, y_mix) in enumerate(ys):
        row0 = y0 * cells_x
        row1 = y1 * cells_x
        out = y * WORK_SIZE
        for x, (x0, x1, x_mix) in enumerate(xs):
            top = lattice[row0 + x0] + (lattice[row0 + x1] - lattice[row0 + x0]) * x_mix
            bottom = lattice[row1 + x0] + (lattice[row1 + x1] - lattice[row1 + x0]) * x_mix
            field[out + x] = top + (bottom - top) * y_mix
    return field


def smoothstep(low: float, high: float, value: float) -> float:
    amount = min(max((value - low) / (high - low), 0.0), 1.0)
    return amount * amount * (3.0 - 2.0 * amount)


def low_resolution_alpha(spec: TextureSpec) -> bytearray:
    weights = spec.weights
    if len(spec.cells) != len(weights) or abs(sum(weights) - 1.0) > 1.0e-9:
        raise ValueError(f"{spec.name}: octave weights must match and sum to one")
    density = [0.0] * (WORK_SIZE * WORK_SIZE)
    for octave, ((cells_x, cells_y), weight) in enumerate(zip(spec.cells, weights)):
        noise = value_noise(cells_x, cells_y, spec.seed + octave * 0x1F123BB5)
        for index, sample in enumerate(noise):
            density[index] += sample * weight

    alpha = bytearray(WORK_SIZE * WORK_SIZE)
    for index, sample in enumerate(density):
        alpha[index] = round(255.0 * smoothstep(spec.transparent_at, spec.opaque_at, sample))
    return alpha


def upscale_periodic(source: bytearray) -> bytearray:
    """Bilinear 4x expansion whose final texels interpolate toward the first wrapped texels."""
    if len(source) != WORK_SIZE * WORK_SIZE:
        raise ValueError("the low-resolution field has the wrong size")
    output = bytearray(SIZE * SIZE)
    for y in range(SIZE):
        y0, fy = divmod(y, UPSCALE)
        y1 = (y0 + 1) % WORK_SIZE
        row0 = y0 * WORK_SIZE
        row1 = y1 * WORK_SIZE
        out = y * SIZE
        for x in range(SIZE):
            x0, fx = divmod(x, UPSCALE)
            x1 = (x0 + 1) % WORK_SIZE
            top = source[row0 + x0] * (UPSCALE - fx) + source[row0 + x1] * fx
            bottom = source[row1 + x0] * (UPSCALE - fx) + source[row1 + x1] * fx
            output[out + x] = (
                top * (UPSCALE - fy) + bottom * fy + UPSCALE * UPSCALE // 2
            ) // (UPSCALE * UPSCALE)
    return output


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))


def encode_png(alpha: bytearray) -> bytes:
    if len(alpha) != SIZE * SIZE:
        raise ValueError("the alpha plane has the wrong size")
    raw = bytearray()
    for y in range(SIZE):
        raw.append(0)  # PNG filter None: stable and enough for these smooth source textures.
        start = y * SIZE
        for opacity in alpha[start:start + SIZE]:
            raw.extend((255, 255, 255, opacity))
    header = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(raw, 9))
        + png_chunk(b"IEND", b"")
    )


def build() -> dict[str, tuple[bytearray, bytes]]:
    result = {}
    for spec in SPECS:
        alpha = upscale_periodic(low_resolution_alpha(spec))
        result[spec.name] = (alpha, encode_png(alpha))
    return result


def mean(values) -> float:
    total = 0.0
    count = 0
    for value in values:
        total += value
        count += 1
    if count == 0:
        raise ValueError("cannot take the mean of no values")
    return total / count


def metrics(alpha: bytearray) -> dict[str, float | int]:
    horizontal_seam = mean(abs(alpha[y * SIZE] - alpha[y * SIZE + SIZE - 1]) for y in range(SIZE))
    vertical_seam = mean(abs(alpha[x] - alpha[(SIZE - 1) * SIZE + x]) for x in range(SIZE))
    horizontal_step = mean(
        abs(alpha[y * SIZE + x] - alpha[y * SIZE + x - 1])
        for y in range(0, SIZE, 8)
        for x in range(1, SIZE, 8)
    )
    vertical_step = mean(
        abs(alpha[y * SIZE + x] - alpha[(y - 1) * SIZE + x])
        for y in range(1, SIZE, 8)
        for x in range(0, SIZE, 8)
    )
    return {
        "minimum": min(alpha),
        "maximum": max(alpha),
        "mean": mean(alpha),
        "transparentPercent": 100.0 * sum(value <= 8 for value in alpha) / len(alpha),
        "opaquePercent": 100.0 * sum(value >= 247 for value in alpha) / len(alpha),
        "distinct": len(set(alpha)),
        "horizontalSeam": horizontal_seam,
        "verticalSeam": vertical_seam,
        "horizontalStep": horizontal_step,
        "verticalStep": vertical_step,
    }


def report(textures: dict[str, tuple[bytearray, bytes]]) -> None:
    for name, (alpha, encoded) in textures.items():
        values = metrics(alpha)
        digest = hashlib.sha256(encoded).hexdigest()
        print(
            f"{name}: {SIZE}x{SIZE} RGBA, {len(encoded):,} bytes, alpha "
            f"{values['minimum']}..{values['maximum']} mean {values['mean']:.1f}, "
            f"clear {values['transparentPercent']:.1f}%, opaque {values['opaquePercent']:.1f}%, "
            f"seam {values['horizontalSeam']:.2f}/{values['verticalSeam']:.2f}, sha256 {digest}"
        )


def selftest(textures: dict[str, tuple[bytearray, bytes]]) -> int:
    print("cloud_textures: selftest")
    failures = 0

    def require(condition: bool, message: str) -> None:
        nonlocal failures
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        failures += 0 if condition else 1

    require(SIZE == 1024 and WORK_SIZE == 256 and SIZE % WORK_SIZE == 0,
            "all source textures are exactly 1024 squared with an integral periodic upscale")
    require(tuple(textures) == tuple(spec.name for spec in SPECS),
            "the output set is exactly cirrus, cumulus and stratus")
    summaries = {name: metrics(alpha) for name, (alpha, _) in textures.items()}
    require(all(data.startswith(b"\x89PNG\r\n\x1a\n") and data[24] == 8 and data[25] == 6
                for _, data in textures.values()),
            "every file is an 8-bit RGBA PNG, not an opaque RGB image")
    require(all(summary["minimum"] == 0 and summary["maximum"] == 255
                and summary["distinct"] >= 240 for summary in summaries.values()),
            "every alpha plane spans transparent to opaque with at least 240 density levels")
    require(summaries["cloud_cirrus.png"]["mean"]
            < summaries["cloud_cumulus.png"]["mean"]
            < summaries["cloud_stratus.png"]["mean"],
            "coverage increases from sparse cirrus through cumulus to stratus")
    require(summaries["cloud_cirrus.png"]["transparentPercent"] >= 25.0,
            "cirrus keeps at least one quarter of the dome ring genuinely clear")
    require(summaries["cloud_cumulus.png"]["transparentPercent"] >= 10.0
            and summaries["cloud_cumulus.png"]["opaquePercent"] >= 10.0,
            "cumulus contains both open sky and dense cloud bodies")
    require(summaries["cloud_stratus.png"]["transparentPercent"] <= 10.0
            and summaries["cloud_stratus.png"]["opaquePercent"] >= 25.0,
            "stratus is a substantially closed, dense layer")
    require(all(summary["horizontalSeam"] <= summary["horizontalStep"] * 2.0 + 0.5
                and summary["verticalSeam"] <= summary["verticalStep"] * 2.0 + 0.5
                for summary in summaries.values()),
            "opposite-edge jumps are no larger than ordinary neighbouring-texel changes")
    require(build() == textures, "generation is byte-deterministic within one process")
    print("cloud_textures: selftest passed." if not failures
          else f"cloud_textures: {failures} claim(s) FAILED")
    return 1 if failures else 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--emit", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)
    if sum((args.emit, args.check, args.report, args.selftest)) != 1:
        parser.error("choose exactly one of --emit, --check, --report or --selftest")

    textures = build()
    if args.report:
        report(textures)
        return 0
    if args.selftest:
        report(textures)
        return selftest(textures)
    if args.check:
        stale = []
        for name, (_, encoded) in textures.items():
            path = OUTPUT_DIR / name
            if not path.is_file() or path.read_bytes() != encoded:
                stale.append(path.relative_to(REPO).as_posix())
        if stale:
            print("cloud_textures: missing or stale: " + ", ".join(stale))
            return 1
        report(textures)
        return 0

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, (_, encoded) in textures.items():
        (OUTPUT_DIR / name).write_bytes(encoded)
    report(textures)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
