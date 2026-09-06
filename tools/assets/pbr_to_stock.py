#!/usr/bin/env python3
"""pbr_to_stock.py -- metallic-roughness to the stock XNA effects' Blinn-Phong parameters.

`HOUSE-00191`. Source assets are authored metallic-roughness. `BasicEffect`, `SkinnedEffect` and
`DualTextureEffect` are not: they are Blinn-Phong with `DiffuseColor`, `SpecularColor` and
`SpecularPower`. Something has to convert, and `cna-house.md` §82.3 requires the conversion be
**documented and fixed** rather than eyeballed per asset -- because a mapping that differs per
material is a mapping nobody can reason about when two props sit side by side and one looks wrong.

## The mapping

Three lines, each with a reason:

    diffuse       = baseColour x (1 - metallic)
    specular      = lerp(0.04, baseColour, metallic)
    specularPower = clamp(2 / alpha^2 - 2, 4, 256),  alpha = roughness^2

**Diffuse.** A metal has no diffuse lobe -- all of its reflection is specular. `1 - metallic` is the
standard metallic-workflow split and is exact at both ends: a pure dielectric keeps its whole base
colour, a pure metal has none.

**Specular.** A dielectric reflects about **4 %** of incident light at normal incidence, achromatically;
that is the F0 = 0.04 every metallic-roughness renderer uses. A metal's reflection is **tinted by its
base colour** -- which is why gold looks gold in its highlight and plastic does not. Interpolating
between the two on `metallic` is the whole of the metallic workflow, restated for an effect that has
no metallic parameter.

**Specular power.** `alpha = roughness^2` is glTF's own remapping (perceptual roughness to the
GGX alpha), and `2/alpha^2 - 2` is the standard lobe match from GGX to a Blinn-Phong exponent. The
clamp is not cosmetic: at roughness 0 the formula diverges, and an exponent above ~256 produces a
highlight smaller than a pixel that aliases into a crawling sparkle. 4 at the rough end keeps a
matte surface from turning into a uniform sheen.

**What this mapping does NOT do**, said out loud so nobody looks for it: no energy conservation, no
Fresnel falloff with angle, no image-based lighting. `BasicEffect` has none of those inputs. The
goal is that a metallic-roughness source lands somewhere *defensible* and *consistent*, not that
Tier S becomes a PBR renderer. Tier E's `RoomLit.fx` consumes the same three parameters, so both
tiers agree with each other rather than each inventing a look.

    tools/assets/pbr_to_stock.py --value 0.9 0.7 0.3 --metallic 1.0 --roughness 0.25
    tools/assets/pbr_to_stock.py assets-src/Models/Fallback/box.glb
    tools/assets/pbr_to_stock.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gltf_validate  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

#: Normal-incidence reflectance of a dielectric. 0.04 is the value every metallic-roughness
#: renderer uses; it corresponds to an index of refraction of about 1.5, which is glass, most
#: plastics and most painted surfaces.
DIELECTRIC_F0 = 0.04

#: Below 4 a highlight is a uniform sheen over the whole surface, which reads as fog rather than as
#: a rough material. Above 256 the highlight is smaller than a pixel and aliases into a crawling
#: sparkle as the camera moves -- worse than no highlight at all.
MIN_SPECULAR_POWER = 4.0
MAX_SPECULAR_POWER = 256.0


def convert(base_colour: tuple[float, float, float], metallic: float, roughness: float) -> dict:
    metallic = min(max(metallic, 0.0), 1.0)
    roughness = min(max(roughness, 0.0), 1.0)

    diffuse = tuple(c * (1.0 - metallic) for c in base_colour)
    specular = tuple(DIELECTRIC_F0 * (1.0 - metallic) + c * metallic for c in base_colour)

    alpha = roughness * roughness
    if alpha <= 1e-6:
        power = MAX_SPECULAR_POWER
    else:
        power = 2.0 / (alpha * alpha) - 2.0
    power = min(max(power, MIN_SPECULAR_POWER), MAX_SPECULAR_POWER)

    return {
        "diffuseColour": [round(c, 6) for c in diffuse],
        "specularColour": [round(c, 6) for c in specular],
        "specularPower": round(power, 3),
        "from": {
            "baseColour": [round(c, 6) for c in base_colour],
            "metallic": round(metallic, 6),
            "roughness": round(roughness, 6),
        },
    }


def convert_gltf(path: Path) -> list[dict]:
    document, error = gltf_validate.read_gltf_json(path)
    if error is not None:
        raise SystemExit(f"pbr_to_stock: {path}: {error}")
    assert document is not None

    results = []
    for index, material in enumerate(document.get("materials", [])):
        pbr = material.get("pbrMetallicRoughness", {})
        base = pbr.get("baseColorFactor", [1.0, 1.0, 1.0, 1.0])
        # glTF's own defaults, which are NOT (0, 0): an unspecified material is a fully rough metal.
        # Using (0, 1) instead would silently make every untagged material plastic.
        metallic = float(pbr.get("metallicFactor", 1.0))
        roughness = float(pbr.get("roughnessFactor", 1.0))
        row = convert((float(base[0]), float(base[1]), float(base[2])), metallic, roughness)
        row["name"] = material.get("name", f"material{index}")
        row["alpha"] = round(float(base[3]) if len(base) > 3 else 1.0, 6)
        results.append(row)
    return results


def selftest() -> int:
    failures = 0

    def require(condition: bool, message: str) -> None:
        nonlocal failures
        if condition:
            print(f"  {message}")
        else:
            print(f"  SELFTEST FAILED: {message}", file=sys.stderr)
            failures += 1

    white = (1.0, 1.0, 1.0)
    gold = (1.0, 0.766, 0.336)

    # A pure dielectric keeps ALL of its base colour and reflects 4%, achromatically.
    plastic = convert(white, metallic=0.0, roughness=0.5)
    require(
        plastic["diffuseColour"] == [1.0, 1.0, 1.0],
        "a dielectric keeps its whole base colour as diffuse",
    )
    require(
        all(abs(c - DIELECTRIC_F0) < 1e-6 for c in plastic["specularColour"]),
        f"a dielectric reflects {DIELECTRIC_F0} achromatically",
    )

    # A pure metal has NO diffuse and takes its base colour into the specular -- which is why gold
    # has a gold highlight and plastic has a white one.
    metal = convert(gold, metallic=1.0, roughness=0.25)
    require(metal["diffuseColour"] == [0.0, 0.0, 0.0], "a metal has no diffuse lobe")
    require(
        [round(c, 3) for c in metal["specularColour"]] == [1.0, 0.766, 0.336],
        "a metal's specular is tinted by its base colour",
    )

    # Monotonic in roughness, in the right direction: rougher means a broader, lower-exponent lobe.
    powers = [convert(white, 0.0, r)["specularPower"] for r in (0.1, 0.3, 0.6, 0.9)]
    require(
        all(a >= b for a, b in zip(powers, powers[1:])),
        f"specular power falls as roughness rises: {powers}",
    )
    require(
        powers[0] == MAX_SPECULAR_POWER,
        f"a near-mirror clamps at {MAX_SPECULAR_POWER} rather than diverging",
    )
    require(
        convert(white, 0.0, 1.0)["specularPower"] >= MIN_SPECULAR_POWER,
        f"a fully rough surface floors at {MIN_SPECULAR_POWER} rather than vanishing",
    )

    # Roughness 0 is the case the formula divides by, and it must not raise.
    mirror = convert(white, 0.0, 0.0)
    require(
        mirror["specularPower"] == MAX_SPECULAR_POWER,
        "roughness 0 is clamped rather than dividing by zero",
    )

    # Half-metal: the interpolation must actually interpolate, not snap to an end.
    half = convert(white, metallic=0.5, roughness=0.5)
    require(
        all(abs(c - 0.5) < 1e-6 for c in half["diffuseColour"]),
        "diffuse at metallic 0.5 is half the base colour",
    )
    require(
        all(abs(c - 0.52) < 1e-6 for c in half["specularColour"]),
        "specular at metallic 0.5 is halfway between 0.04 and the base colour",
    )

    # Out-of-range input is clamped, not trusted: these come from exporters.
    require(
        convert(white, metallic=5.0, roughness=-1.0)["from"]["metallic"] == 1.0,
        "an out-of-range metallic is clamped",
    )

    if failures:
        return 1
    print("pbr_to_stock: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="*", type=Path)
    parser.add_argument("--value", nargs=3, type=float, metavar=("R", "G", "B"))
    parser.add_argument("--metallic", type=float, default=0.0)
    parser.add_argument("--roughness", type=float, default=0.5)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    if args.value:
        result = convert(tuple(args.value), args.metallic, args.roughness)
        print(json.dumps(result, indent=2))
        return 0

    if not args.paths:
        parser.print_help()
        return 2

    for path in args.paths:
        rows = convert_gltf(path)
        relative = path.relative_to(REPO) if path.is_relative_to(REPO) else path
        print(f"# {relative}")
        print(json.dumps(rows, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
