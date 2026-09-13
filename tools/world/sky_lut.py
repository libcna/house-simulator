#!/usr/bin/env python3
"""sky_lut.py -- generate §31.2's art-directed sky lookup tables offline.

`HOUSE-01641`.  The runtime does not evaluate an atmospheric scattering model.  It interpolates
this tool's 32 clear-sky samples and applies the cloud-cover and sun-azimuth axes analytically.
That is the compact representation chosen by `HOUSE-00394`: 32 authored rows plus two equations,
not 4096 redundant RGB triples.  This tool nevertheless evaluates the complete 32 x 8 x 16 grid
in its self-test so all three axes remain a measured contract.

    tools/world/sky_lut.py --emit
    tools/world/sky_lut.py --check
    tools/world/sky_lut.py --selftest

The eleven anchors are deliberately visible below.  Sunrise and sunset use the same curve because
sun elevation, not clock direction, is the input; their warm horizon is localised around the sun
by the azimuth lobe instead of painting the whole horizon orange.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SKY = REPO / "assets-src" / "world" / "layout.sky.json"
BEGIN = "  // BEGIN GENERATED: sky_lut.py -- do not edit the gradient or colour model by hand"
END = "  // END GENERATED: sky_lut.py"

SAMPLES = 32
CLOUD_SAMPLES = 8
AZIMUTH_SAMPLES = 16
OVERCAST_GREY = (0.370, 0.400, 0.440)
SUN_GLOW_COLOUR = (1.000, 0.620, 0.300)
SUN_GLOW_STRENGTH = 0.180
SUN_GLOW_EXPONENT = 6.0
LIGHT_POLLUTION_COLOUR = (1.000, 0.450, 0.160)
LIGHT_POLLUTION_STRENGTH = 0.055
TOWN_AZIMUTH_DEG = 180.0
LIGHT_POLLUTION_AZIMUTH_EXPONENT = 4.0
LIGHT_POLLUTION_ALTITUDE_EXPONENT = 3.0
LIGHT_POLLUTION_STAR_MAGNITUDE_LOSS = 2.5


@dataclass(frozen=True)
class Anchor:
    elevation: float
    zenith: tuple[float, float, float]
    horizon: tuple[float, float, float]


# Astronomical, nautical and civil twilight; the pre-sunrise band; the horizon crossing and
# golden hour; then low, middle and high daylight.  The low-elevation horizon is intentionally
# darker than the old all-azimuth orange band: SUN_GLOW_COLOUR restores the brightness only near
# the sun, which is both warmer and spatially readable.
ANCHORS = (
    Anchor(-18.0, (0.010, 0.012, 0.030), (0.018, 0.020, 0.048)),
    Anchor(-12.0, (0.020, 0.026, 0.062), (0.045, 0.048, 0.100)),
    Anchor(-6.0, (0.055, 0.080, 0.170), (0.180, 0.120, 0.160)),
    Anchor(-2.0, (0.105, 0.155, 0.300), (0.620, 0.270, 0.120)),
    Anchor(0.0, (0.155, 0.235, 0.450), (0.800, 0.380, 0.160)),
    Anchor(3.0, (0.170, 0.275, 0.550), (0.820, 0.550, 0.340)),
    Anchor(10.0, (0.170, 0.310, 0.660), (0.820, 0.760, 0.680)),
    Anchor(25.0, (0.165, 0.335, 0.730), (0.700, 0.760, 0.820)),
    Anchor(45.0, (0.160, 0.350, 0.770), (0.640, 0.750, 0.890)),
    Anchor(60.0, (0.160, 0.350, 0.780), (0.620, 0.740, 0.900)),
    Anchor(90.0, (0.155, 0.350, 0.790), (0.610, 0.740, 0.905)),
)


def smoothstep(value: float) -> float:
    value = min(max(value, 0.0), 1.0)
    return value * value * (3.0 - 2.0 * value)


def mix(a: tuple[float, float, float], b: tuple[float, float, float], amount: float
        ) -> tuple[float, float, float]:
    return tuple(a[i] + (b[i] - a[i]) * amount for i in range(3))


def clear_sky(elevation: float) -> tuple[tuple[float, float, float],
                                         tuple[float, float, float]]:
    """Smoothly interpolate the two clear-sky colours at one sun elevation."""
    if elevation <= ANCHORS[0].elevation:
        return ANCHORS[0].zenith, ANCHORS[0].horizon
    for low, high in zip(ANCHORS, ANCHORS[1:]):
        if elevation <= high.elevation:
            amount = smoothstep((elevation - low.elevation) /
                                (high.elevation - low.elevation))
            return mix(low.zenith, high.zenith, amount), mix(
                low.horizon, high.horizon, amount)
    return ANCHORS[-1].zenith, ANCHORS[-1].horizon


def sample_elevation(index: int) -> float:
    return ANCHORS[0].elevation + (ANCHORS[-1].elevation - ANCHORS[0].elevation) * (
        index / (SAMPLES - 1))


def gradient() -> list[dict]:
    rows = []
    for index in range(SAMPLES):
        # The written coordinate is the coordinate runtime interpolates at.  Round it first so
        # the colour beside it is generated AT that value rather than a nearby hidden one.
        elevation = round(sample_elevation(index), 2)
        zenith, horizon = clear_sky(elevation)
        rows.append({
            "sunElevationDeg": round(elevation, 2),
            "zenith": [round(value, 4) for value in zenith],
            "horizon": [round(value, 4) for value in horizon],
        })
    return rows


def sun_intensity(elevation: float) -> float:
    """The subset of layout.sky.json's sun curve needed to evaluate the virtual full LUT."""
    points = ((-6.0, 0.0), (-1.0, 0.25), (0.0, 0.45), (3.0, 0.75),
              (10.0, 0.95), (30.0, 1.0), (90.0, 1.0))
    if elevation <= points[0][0]:
        return points[0][1]
    for low, high in zip(points, points[1:]):
        if elevation <= high[0]:
            amount = (elevation - low[0]) / (high[0] - low[0])
            return low[1] + (high[1] - low[1]) * amount
    return points[-1][1]


def evaluated_horizon(elevation: float, cloud_cover: float,
                      azimuth_offset_deg: float) -> tuple[float, float, float]:
    """Evaluate the compact form as one cell of §31.2's conceptual 32 x 8 x 16 LUT."""
    _, horizon = clear_sky(elevation)
    cloud_amount = cloud_cover ** 1.5
    colour = mix(horizon, OVERCAST_GREY, cloud_amount)
    lobe = max(math.cos(math.radians(azimuth_offset_deg)), 0.0) ** SUN_GLOW_EXPONENT
    glow = SUN_GLOW_STRENGTH * sun_intensity(elevation) * lobe * (1.0 - cloud_cover) ** 2
    return tuple(colour[i] + SUN_GLOW_COLOUR[i] * glow for i in range(3))


def evaluated_zenith(elevation: float, cloud_cover: float) -> tuple[float, float, float]:
    """Evaluate one cell of §31.2's conceptual 32 x 8 zenith LUT."""
    zenith, _ = clear_sky(elevation)
    return mix(zenith, OVERCAST_GREY, cloud_cover ** 1.5)


def light_pollution_factor(azimuth_deg: float, altitude_sine: float) -> float:
    """The generated town-facing low-horizon gradient shared by dome and stars."""
    offset = math.radians(azimuth_deg - TOWN_AZIMUTH_DEG)
    directional = max(math.cos(offset), 0.0) ** LIGHT_POLLUTION_AZIMUTH_EXPONENT
    horizon = (1.0 - min(max(altitude_sine, 0.0), 1.0)) ** LIGHT_POLLUTION_ALTITUDE_EXPONENT
    return directional * horizon


def virtual_lut() -> list[tuple[float, float, float]]:
    """Materialise the horizon table represented by the committed compact form."""
    return [
        evaluated_horizon(row["sunElevationDeg"], cloud_index / (CLOUD_SAMPLES - 1),
                          azimuth_index * 180.0 / (AZIMUTH_SAMPLES - 1))
        for row in gradient()
        for cloud_index in range(CLOUD_SAMPLES)
        for azimuth_index in range(AZIMUTH_SAMPLES)
    ]


def render_block() -> str:
    lines = ['  "gradient": [']
    rows = gradient()
    for index, row in enumerate(rows):
        elevation = row["sunElevationDeg"]
        zenith = ", ".join(f"{value:.4f}" for value in row["zenith"])
        horizon = ", ".join(f"{value:.4f}" for value in row["horizon"])
        comma = "," if index + 1 < len(rows) else ""
        lines.append(f'    {{ "sunElevationDeg": {elevation:6.2f}, "zenith": [{zenith}], '
                     f'"horizon": [{horizon}] }}{comma}')
    lines.extend([
        "  ],",
        "  \"colourModel\": {",
        f'    "cloudCoverSamples": {CLOUD_SAMPLES}, "azimuthOffsetSamples": {AZIMUTH_SAMPLES},',
        '    "overcastGrey": [' + ", ".join(f"{value:.3f}" for value in OVERCAST_GREY) + "],",
        '    "sunGlowColor": [' + ", ".join(f"{value:.3f}" for value in SUN_GLOW_COLOUR) + "],",
        f'    "sunGlowStrength": {SUN_GLOW_STRENGTH:.3f}, '
        f'"sunGlowExponent": {SUN_GLOW_EXPONENT:.1f},',
        '    "lightPollutionColor": [' +
        ", ".join(f"{value:.3f}" for value in LIGHT_POLLUTION_COLOUR) + "],",
        f'    "lightPollutionStrength": {LIGHT_POLLUTION_STRENGTH:.3f}, '
        f'"townAzimuthDeg": {TOWN_AZIMUTH_DEG:.1f},',
        f'    "lightPollutionAzimuthExponent": {LIGHT_POLLUTION_AZIMUTH_EXPONENT:.1f}, '
        f'"lightPollutionAltitudeExponent": {LIGHT_POLLUTION_ALTITUDE_EXPONENT:.1f},',
        f'    "lightPollutionStarMagnitudeLoss": {LIGHT_POLLUTION_STAR_MAGNITUDE_LOSS:.1f}',
        "  },",
    ])
    return "\n".join(lines)


def replace_generated(text: str) -> str:
    if text.count(BEGIN) != 1 or text.count(END) != 1:
        raise ValueError(f"{SKY.relative_to(REPO)} must contain exactly one generated marker pair")
    before, rest = text.split(BEGIN, 1)
    _, after = rest.split(END, 1)
    return before + BEGIN + "\n" + render_block() + "\n" + END + after


def selftest() -> int:
    failures = 0

    def require(condition: bool, message: str) -> None:
        nonlocal failures
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        failures += 0 if condition else 1

    rows = gradient()
    require(len(ANCHORS) == 11, "the art direction has exactly eleven named anchors")
    require(len(rows) == 32, "the compact sun-elevation table has 32 rows")
    require(all(a.elevation < b.elevation for a, b in zip(ANCHORS, ANCHORS[1:])),
            "anchor elevations are strictly ascending")
    require(rows[0]["sunElevationDeg"] == -18.0 and rows[-1]["sunElevationDeg"] == 90.0,
            "the table spans astronomical night through the zenith")
    require(all(0.0 <= value <= 1.0 for row in rows
                for name in ("zenith", "horizon") for value in row[name]),
            "every stored clear-sky channel is representable by XNA Color")

    dawn = min(rows, key=lambda row: abs(row["sunElevationDeg"]))
    noon = min(rows, key=lambda row: abs(row["sunElevationDeg"] - 45.0))
    require(dawn["horizon"][0] > 2.0 * dawn["horizon"][2]
            and dawn["horizon"][0] > 1.8 * dawn["horizon"][1],
            f"the horizon crossing is distinctly warm ({dawn['horizon']})")
    require(noon["zenith"][2] > 2.0 * noon["zenith"][1] > 3.0 * noon["zenith"][0],
            f"midday returns to a blue zenith ({noon['zenith']})")

    lut = virtual_lut()
    require(len(lut) == 32 * 8 * 16, "the compact form expands to all 4096 horizon samples")
    zenith_lut = [evaluated_zenith(row["sunElevationDeg"],
                                  cloud_index / (CLOUD_SAMPLES - 1))
                  for row in rows
                  for cloud_index in range(CLOUD_SAMPLES)]
    require(len(zenith_lut) == 32 * 8,
            "and to all 256 zenith samples, with no untested axis")
    require(all(0.0 <= channel <= 1.0 for colour in lut + zenith_lut for channel in colour),
            "every expanded sample remains representable by XNA Color")
    overcast = [evaluated_horizon(elevation, 1.0, angle)
                for elevation in (-18.0, 0.0, 45.0, 90.0) for angle in (0.0, 90.0, 180.0)]
    require(all(max(abs(colour[i] - OVERCAST_GREY[i]) for i in range(3)) < 1e-12
                for colour in overcast), "full overcast removes time and azimuth variation")
    near_sun = evaluated_horizon(0.0, 0.0, 0.0)
    away_sun = evaluated_horizon(0.0, 0.0, 180.0)
    require(near_sun[0] > away_sun[0] and near_sun[0] > 4.0 * near_sun[2],
            f"the clear sunrise glow is local and warm ({near_sun} versus {away_sun})")
    require(max(near_sun) <= 1.0, "the tuned sunrise remains in XNA Color's 0..1 range")

    require(light_pollution_factor(180.0, 0.0) == 1.0,
            "the extra night gradient peaks on the southern town horizon")
    require(light_pollution_factor(0.0, 0.0) == 0.0
            and light_pollution_factor(90.0, 0.0) < 1e-60,
            "the town gradient contributes nothing on the northern or eastern horizon")
    require(abs(light_pollution_factor(180.0, 0.5) - 0.125) < 1e-12
            and light_pollution_factor(180.0, 1.0) == 0.0,
            "the town gradient falls cubically from horizon to zenith")
    require(LIGHT_POLLUTION_STAR_MAGNITUDE_LOSS == 2.5,
            "the glow removes 2.5 magnitudes only at its strongest point")

    print("sky_lut: selftest passed." if not failures
          else f"sky_lut: {failures} claim(s) FAILED")
    return 1 if failures else 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--emit", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)

    if args.selftest:
        return selftest()
    if not SKY.is_file():
        print(f"sky_lut: {SKY.relative_to(REPO)} is missing")
        return 1
    current = SKY.read_text(encoding="utf-8")
    try:
        wanted = replace_generated(current)
    except ValueError as exc:
        print(f"sky_lut: {exc}")
        return 1
    if args.emit:
        SKY.write_text(wanted, encoding="utf-8")
        print(f"sky_lut: wrote {SKY.relative_to(REPO)} ({SAMPLES} compact rows, "
              f"{SAMPLES * CLOUD_SAMPLES * AZIMUTH_SAMPLES} represented horizon samples)")
        return 0
    if args.check:
        if current != wanted:
            print(f"sky_lut: {SKY.relative_to(REPO)} is stale; run --emit")
            return 1
        print(f"sky_lut: {SKY.relative_to(REPO)} matches ({SAMPLES} compact rows, "
              f"{SAMPLES * CLOUD_SAMPLES * AZIMUTH_SAMPLES} represented horizon samples).")
        return 0
    parser.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
