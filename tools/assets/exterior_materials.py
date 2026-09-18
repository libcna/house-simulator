#!/usr/bin/env python3
"""Author and verify HOUSE-00903's nine exterior materials."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ambientcg_materials  # noqa: E402
import pbr_to_stock  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
MATERIALS_FILE = REPO / "assets-src" / "world" / "layout.materials.json"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
CONTENT_CONFIG = REPO / "assets-src" / "Textures" / ".cna-content.json"
TEXTURES = REPO / "assets-src" / "Textures" / "Materials"
SIDING_ALBEDO = TEXTURES / "siding_clapboard_albedo.png"
SIDING_NORMAL = TEXTURES / "siding_clapboard_normal.png"
ROOF_ALBEDO = TEXTURES / "roof_asphalt_shingle_albedo.png"
ROOF_NORMAL = TEXTURES / "roof_asphalt_shingle_normal.png"
PREVIEW = REPO / "docs" / "asset-review" / "materials" / "exterior" / "contact-sheet.png"
BEGIN = "    // BEGIN GENERATED EXTERIOR MATERIALS (exterior_materials.py)"
END = "    // END GENERATED EXTERIOR MATERIALS"
SIDING_SIZE = 512
SIDING_COURSES_PER_METRE = 6
SIDING_ROUGHNESS = 0.72
ROOF_SIZE = 512
ROOF_COURSES_PER_METRE = 7
ROOF_TABS_PER_METRE = 4
ROOF_ROUGHNESS = 0.86


@dataclass(frozen=True)
class Exterior:
    material_id: str
    source: str
    tint: tuple[float, float, float]
    role: str
    uv_scale: tuple[float, float] = (1.0, 1.0)
    snow: tuple[bool, float] = (True, 40.0)
    class_override: str | None = None
    footstep_override: str | None = None
    absorption_override: float | None = None
    wet_override: tuple[float, float, float] | None = None


EXTERIORS = (
    Exterior("MAT_SIDING_WARM_WHITE", "wood_board", (1.00, 0.96, 0.88), "siding"),
    Exterior("MAT_SIDING_SAGE", "wood_board", (0.68, 0.78, 0.62), "siding"),
    Exterior("MAT_SIDING_DUSTY_BLUE", "wood_board", (0.64, 0.74, 0.90), "siding"),
    Exterior("MAT_BRICK_WATER_TABLE", "brick_red", (1.00, 0.92, 0.86), "brick",
             (0.75, 0.75)),
    # `tile_light_square` remains the deterministic semantic seed, but HOUSE-00943 replaces its
    # visibly ceramic square grid with a project-authored seven-course asphalt-shingle pair.
    Exterior("MAT_ROOF_SHINGLE", "tile_light_square", (0.68, 0.66, 0.64), "roof shingle",
             (1.0, 1.0), (True, 55.0), "asphalt", "asphalt", 0.12,
             (0.35, 2.4, 2.6)),
    Exterior("MAT_SOFFIT_WHITE", "paint_white_fine", (1.00, 0.98, 0.93), "soffit",
             snow=(False, 0.0), class_override="paint",
             footstep_override="painted_wood", absorption_override=0.12),
    Exterior("MAT_CONCRETE_BROOM", "concrete_smooth", (0.88, 0.86, 0.82), "concrete",
             (0.75, 0.75), (True, 15.0)),
    Exterior("MAT_ASPHALT_01", "asphalt_road", (1.00, 1.00, 1.00), "asphalt",
             (0.5, 0.5), (True, 12.0)),
    Exterior("MAT_GRAVEL_PATH", "gravel_mixed", (0.92, 0.90, 0.86), "gravel",
             snow=(True, 30.0)),
)


def source_material(slug: str):
    return next(material for material in ambientcg_materials.MATERIALS if material.slug == slug)


def material_row(exterior: Exterior) -> dict:
    row = pbr_to_stock.base_material_row(source_material(exterior.source))
    row["id"] = exterior.material_id
    row["tint"] = list(exterior.tint)
    row["uvScale"] = list(exterior.uv_scale)
    row["snowResponse"] = {
        "coverable": exterior.snow[0],
        "slopeLimitDeg": exterior.snow[1],
    }
    if exterior.role == "siding":
        # Wood095 remains the approved generic bare-board source for joinery and furniture. The
        # house itself is painted clapboard: six 167 mm exposed courses in the generator's
        # one-world-metre UV tile, with a rough painted dielectric response.
        mapped = pbr_to_stock.convert((1.0, 1.0, 1.0), 0.0, SIDING_ROUGHNESS)
        row["albedo"] = "Textures/Materials/siding_clapboard_albedo"
        row["normal"] = "Textures/Materials/siding_clapboard_normal"
        row["specularColor"] = mapped["specularColour"]
        row["specularPower"] = mapped["specularPower"]
    if exterior.role == "roof shingle":
        # A square ceramic floor tile was always only the least-wrong source in the first fixed
        # library. The stable material id now resolves to a real one-metre asphalt layout: seven
        # 143 mm exposures with four staggered tabs, while the established wet/snow/audio contract
        # remains on this row and its generated derivatives.
        mapped = pbr_to_stock.convert((1.0, 1.0, 1.0), 0.0, ROOF_ROUGHNESS)
        row["albedo"] = "Textures/Materials/roof_asphalt_shingle_albedo"
        row["normal"] = "Textures/Materials/roof_asphalt_shingle_normal"
        row["specularColor"] = mapped["specularColour"]
        row["specularPower"] = mapped["specularPower"]
    if exterior.class_override is not None:
        row["class"] = exterior.class_override
    if exterior.footstep_override is not None:
        row["footstepSurface"] = exterior.footstep_override
    if exterior.absorption_override is not None:
        row["audioAbsorption"] = exterior.absorption_override
    if exterior.wet_override is not None:
        row["wetResponse"] = {
            "albedoDarken": exterior.wet_override[0],
            "specularBoost": exterior.wet_override[1],
            "powerBoost": exterior.wet_override[2],
        }
    return row


def siding_height(x: int, y: int) -> float:
    """Periodic painted-board relief; the sawtooth reset is the physical overlapping lap."""
    u = (x + 0.5) / SIDING_SIZE
    v = (y + 0.5) / SIDING_SIZE
    phase = (v * SIDING_COURSES_PER_METRE) % 1.0
    grain = 0.012 * math.sin(2.0 * math.pi * (u * 3.0 + v))
    grain += 0.006 * math.sin(2.0 * math.pi * (u * 11.0 - v * 2.0))
    return 0.035 * phase + grain


def siding_texture_images():
    """Return one neutral painted albedo and matching +Y tangent normal, both tileable."""
    from PIL import Image

    albedo = Image.new("RGBA", (SIDING_SIZE, SIDING_SIZE))
    normal = Image.new("RGB", (SIDING_SIZE, SIDING_SIZE))
    albedo_pixels = albedo.load()
    normal_pixels = normal.load()
    heights = [[siding_height(x, y) for x in range(SIDING_SIZE)]
               for y in range(SIDING_SIZE)]
    for y in range(SIDING_SIZE):
        v = (y + 0.5) / SIDING_SIZE
        phase = (v * SIDING_COURSES_PER_METRE) % 1.0
        # A narrow cool shadow identifies the overlap. Everything else stays neutral and subtle,
        # so the three canonical paint tints, live sky and baked occlusion remain the colour source.
        lap_shadow = 34.0 * math.exp(-0.5 * (phase / 0.045) ** 2)
        lip_highlight = 7.0 * math.exp(-0.5 * ((phase - 0.105) / 0.045) ** 2)
        face_gradient = 5.0 * (0.5 - phase)
        for x in range(SIDING_SIZE):
            u = (x + 0.5) / SIDING_SIZE
            paint = 2.2 * math.sin(2.0 * math.pi * (u * 2.0 + v))
            paint += 1.3 * math.sin(2.0 * math.pi * (u * 13.0 - v * 2.0))
            value = max(0, min(255, round(210.0 - lap_shadow + lip_highlight + face_gradient + paint)))
            albedo_pixels[x, y] = (value, value, max(0, value - 1), 255)

            dx = heights[y][(x + 1) % SIDING_SIZE] - heights[y][(x - 1) % SIDING_SIZE]
            dy = heights[(y + 1) % SIDING_SIZE][x] - heights[(y - 1) % SIDING_SIZE][x]
            nx = -16.0 * dx
            ny = -16.0 * dy
            nz = 1.0
            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            normal_pixels[x, y] = (
                round((nx / length * 0.5 + 0.5) * 255.0),
                round((ny / length * 0.5 + 0.5) * 255.0),
                round((nz / length * 0.5 + 0.5) * 255.0),
            )
    return albedo, normal


def roof_noise(x: int, y: int, seed: int = 0) -> float:
    """Stable periodic -1..1 aggregate noise without a PRNG or platform state."""
    x %= ROOF_SIZE
    y %= ROOF_SIZE
    value = ((x * 73856093) ^ (y * 19349663) ^ (seed * 83492791)) & 0xffffffff
    value ^= value >> 13
    value = (value * 1274126177) & 0xffffffff
    return ((value & 0xffff) / 32767.5) - 1.0


def roof_height(x: int, y: int) -> float:
    """Periodic laminated-shingle relief with an overlap and short exposed tab slots."""
    u = ((x % ROOF_SIZE) + 0.5) / ROOF_SIZE
    v = ((y % ROOF_SIZE) + 0.5) / ROOF_SIZE
    course_value = v * ROOF_COURSES_PER_METRE
    course = math.floor(course_value)
    phase = course_value - course
    tab_value = u * ROOF_TABS_PER_METRE + (0.5 if course % 2 else 0.0)
    tab_phase = tab_value - math.floor(tab_value)
    tab_distance = min(tab_phase, 1.0 - tab_phase)
    overlap = 0.022 * phase
    # Slots stop below the covered head of each shingle instead of scoring the whole roof grid.
    slot = -0.010 * math.exp(-0.5 * (tab_distance / 0.025) ** 2) \
        * max(0.0, min(1.0, (phase - 0.08) / 0.10)) \
        * max(0.0, min(1.0, (0.78 - phase) / 0.10))
    granule = 0.0018 * roof_noise(x, y, course)
    return overlap + slot + granule


def roof_texture_images():
    """Return tileable charcoal asphalt-shingle albedo and matching +Y tangent normal."""
    from PIL import Image

    albedo = Image.new("RGBA", (ROOF_SIZE, ROOF_SIZE))
    normal = Image.new("RGB", (ROOF_SIZE, ROOF_SIZE))
    albedo_pixels = albedo.load()
    normal_pixels = normal.load()
    heights = [[roof_height(x, y) for x in range(ROOF_SIZE)] for y in range(ROOF_SIZE)]
    for y in range(ROOF_SIZE):
        v = (y + 0.5) / ROOF_SIZE
        course_value = v * ROOF_COURSES_PER_METRE
        course = math.floor(course_value)
        phase = course_value - course
        lap_shadow = 24.0 * math.exp(-0.5 * (phase / 0.035) ** 2)
        edge_highlight = 5.0 * math.exp(-0.5 * ((phase - 0.075) / 0.035) ** 2)
        for x in range(ROOF_SIZE):
            u = (x + 0.5) / ROOF_SIZE
            tab_value = u * ROOF_TABS_PER_METRE + (0.5 if course % 2 else 0.0)
            tab_phase = tab_value - math.floor(tab_value)
            tab_distance = min(tab_phase, 1.0 - tab_phase)
            slot = 20.0 * math.exp(-0.5 * (tab_distance / 0.018) ** 2) \
                * max(0.0, min(1.0, (phase - 0.10) / 0.08)) \
                * max(0.0, min(1.0, (0.76 - phase) / 0.08))
            aggregate = 8.0 * roof_noise(x, y, 17) + 3.0 * roof_noise(x // 4, y // 4, 29)
            course_tone = 3.0 * math.sin(2.0 * math.pi * (course / ROOF_COURSES_PER_METRE))
            base = max(0, min(255, round(
                112.0 + course_tone + aggregate - lap_shadow - slot + edge_highlight)))
            albedo_pixels[x, y] = (
                max(0, base - 5), max(0, base - 3), base, 255)

            dx = heights[y][(x + 1) % ROOF_SIZE] - heights[y][(x - 1) % ROOF_SIZE]
            dy = heights[(y + 1) % ROOF_SIZE][x] - heights[(y - 1) % ROOF_SIZE][x]
            nx = -20.0 * dx
            ny = -20.0 * dy
            nz = 1.0
            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            normal_pixels[x, y] = (
                round((nx / length * 0.5 + 0.5) * 255.0),
                round((ny / length * 0.5 + 0.5) * 255.0),
                round((nz / length * 0.5 + 0.5) * 255.0),
            )
    return albedo, normal


def png_bytes(image) -> bytes:
    output = io.BytesIO()
    image.save(output, format="PNG", optimize=True)
    return output.getvalue()


def compare_image(path: Path, expected, problems: list[str]) -> None:
    from PIL import Image, ImageChops

    try:
        with Image.open(path) as source:
            actual = source.convert(expected.mode)
            actual.load()
        if actual.size != expected.size:
            problems.append(f"{path}: size {actual.size}, expected {expected.size}")
        elif ImageChops.difference(actual, expected).getbbox() is not None:
            problems.append(f"{path}: pixels are stale; regenerate with --write")
    except OSError as error:
        problems.append(f"{path}: {error}")


def validate_definitions() -> list[str]:
    problems: list[str] = []
    ids = [exterior.material_id for exterior in EXTERIORS]
    roles = [exterior.role for exterior in EXTERIORS]
    source_slugs = {material.slug for material in ambientcg_materials.MATERIALS}
    if len(EXTERIORS) != 9 or len(set(ids)) != 9:
        problems.append("expected exactly 9 unique exterior ids")
    expected_roles = {
        "siding": 3, "brick": 1, "roof shingle": 1, "soffit": 1,
        "concrete": 1, "asphalt": 1, "gravel": 1,
    }
    actual_roles = {role: roles.count(role) for role in set(roles)}
    if actual_roles != expected_roles:
        problems.append(f"roles {actual_roles}, expected {expected_roles}")
    for exterior in EXTERIORS:
        if exterior.source not in source_slugs:
            problems.append(f"{exterior.material_id} uses unknown source {exterior.source}")
        if any(channel < 0.0 or channel > 1.0 for channel in exterior.tint):
            problems.append(f"{exterior.material_id} tint is outside 0..1")
        if any(scale <= 0.0 for scale in exterior.uv_scale):
            problems.append(f"{exterior.material_id} UV scale is not positive")
        if exterior.snow[0] and not 0.0 < exterior.snow[1] <= 90.0:
            problems.append(f"{exterior.material_id} has an invalid snow slope")
    roof = next(exterior for exterior in EXTERIORS if exterior.role == "roof shingle")
    if (roof.class_override != "asphalt" or roof.snow[1] < 40.0 or
            roof.wet_override is None or roof.uv_scale != (1.0, 1.0)):
        problems.append("roof must retain asphalt response and accept snow on the house pitch")
    soffit = next(exterior for exterior in EXTERIORS if exterior.role == "soffit")
    if soffit.snow[0]:
        problems.append("downward-facing soffit must not be snow-coverable")
    return problems


def load_rows() -> list[dict]:
    sys.path.insert(0, str(REPO / "tools" / "world"))
    import layout_io

    document = json.loads(layout_io.strip_jsonc(MATERIALS_FILE.read_text(encoding="utf-8")))
    return document["materials"]


def preview_image():
    from PIL import Image, ImageChops, ImageDraw

    tile_width = 320
    image_height = 190
    label_height = 30
    columns = 3
    rows = 3
    sheet = Image.new("RGB", (columns * tile_width, rows * (image_height + label_height)),
                      (31, 34, 40))
    draw = ImageDraw.Draw(sheet)
    siding_albedo, _ = siding_texture_images()
    roof_albedo, _ = roof_texture_images()
    for index, exterior in enumerate(EXTERIORS):
        if exterior.role == "siding":
            albedo = siding_albedo.convert("RGB").resize(
                (tile_width, image_height), Image.Resampling.LANCZOS)
        elif exterior.role == "roof shingle":
            albedo = roof_albedo.convert("RGB").resize(
                (tile_width, image_height), Image.Resampling.LANCZOS)
        else:
            with Image.open(pbr_to_stock.TEXTURES / f"{exterior.source}_albedo.png") as source:
                albedo = source.convert("RGB").resize(
                    (tile_width, image_height), Image.Resampling.LANCZOS
                )
        tint = tuple(round(channel * 255.0) for channel in exterior.tint)
        pixels = ImageChops.multiply(albedo, Image.new("RGB", albedo.size, tint))
        x = (index % columns) * tile_width
        y = (index // columns) * (image_height + label_height)
        sheet.paste(pixels, (x, y))
        label = exterior.material_id.removeprefix("MAT_").lower()
        draw.rectangle((x, y + image_height, x + tile_width, y + image_height + label_height),
                       fill=(31, 34, 40))
        draw.text((x + 7, y + image_height + 7), label, fill=(235, 235, 235))
    return sheet


def check() -> int:
    from PIL import Image, ImageChops

    problems = validate_definitions()
    rows = load_rows()
    by_id = {row["id"]: row for row in rows}
    if len(by_id) != len(rows):
        problems.append("material table contains duplicate ids")
    for exterior in EXTERIORS:
        actual = by_id.get(exterior.material_id)
        expected = material_row(exterior)
        if actual is None:
            problems.append(f"missing {exterior.material_id} in {MATERIALS_FILE}")
            continue
        for field in sorted(set(actual) | set(expected)):
            if actual.get(field) != expected.get(field):
                problems.append(
                    f"{exterior.material_id}/{field}: {actual.get(field)!r}, "
                    f"expected {expected.get(field)!r}"
                )
    expected_albedo, expected_normal = siding_texture_images()
    compare_image(SIDING_ALBEDO, expected_albedo, problems)
    compare_image(SIDING_NORMAL, expected_normal, problems)
    expected_roof_albedo, expected_roof_normal = roof_texture_images()
    compare_image(ROOF_ALBEDO, expected_roof_albedo, problems)
    compare_image(ROOF_NORMAL, expected_roof_normal, problems)
    try:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        manifest_rows = {row.get("id"): row for row in manifest.get("assets", [])}
        expected_manifest = {
            "TEXTURE_MATERIAL_SIDING_CLAPBOARD_ALBEDO": SIDING_ALBEDO,
            "TEXTURE_MATERIAL_SIDING_CLAPBOARD_NORMAL": SIDING_NORMAL,
            "TEXTURE_MATERIAL_ROOF_ASPHALT_SHINGLE_ALBEDO": ROOF_ALBEDO,
            "TEXTURE_MATERIAL_ROOF_ASPHALT_SHINGLE_NORMAL": ROOF_NORMAL,
        }
        for asset_id, path in expected_manifest.items():
            row = manifest_rows.get(asset_id)
            if row is None:
                problems.append(f"{MANIFEST}: missing {asset_id}")
                continue
            digest = hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else "missing"
            if row.get("sourceSha256") != digest:
                problems.append(f"{asset_id}: manifest hash is stale")
            if row.get("origin", {}).get("kind") != "generated":
                problems.append(f"{asset_id}: architectural source must be project-generated")
        config = json.loads(CONTENT_CONFIG.read_text(encoding="utf-8"))
        configured = config.get("assets", {})
        expected_parameters = {
            "generateMipmaps": {"type": "bool", "value": True},
            "premultiplyAlpha": {"type": "bool", "value": False},
        }
        for name in ("Materials/siding_clapboard_albedo.png",
                     "Materials/siding_clapboard_normal.png",
                     "Materials/roof_asphalt_shingle_albedo.png",
                     "Materials/roof_asphalt_shingle_normal.png"):
            if configured.get(name, {}).get("parameters") != expected_parameters:
                problems.append(f"{CONTENT_CONFIG}: {name} must pin mips=true, premultiply=false")
    except (OSError, json.JSONDecodeError) as error:
        problems.append(f"could not verify generated architectural registration: {error}")
    try:
        with Image.open(PREVIEW) as source:
            actual_preview = source.convert("RGB")
            actual_preview.load()
        expected_preview = preview_image()
        if actual_preview.size != expected_preview.size:
            problems.append(
                f"{PREVIEW}: size {actual_preview.size}, expected {expected_preview.size}"
            )
        elif ImageChops.difference(actual_preview, expected_preview).getbbox() is not None:
            problems.append(f"{PREVIEW}: pixels are stale; regenerate with --preview")
    except OSError as error:
        problems.append(f"{PREVIEW}: {error}")
    if problems:
        for problem in problems:
            print(f"exterior_materials: {problem}", file=sys.stderr)
        return 1
    print("exterior_materials: 9 derived exterior materials clean")
    return 0


def render_row(row: dict) -> str:
    def value(field: str) -> str:
        return json.dumps(row[field])

    return "\n".join((
        "    {",
        f'      "id": {value("id")}, "class": {value("class")},',
        f'      "albedo": {value("albedo")},',
        f'      "normal": {value("normal")}, "lightmapChannel": {value("lightmapChannel")},',
        f'      "tint": {value("tint")}, "specularColor": {value("specularColor")},',
        f'      "specularPower": {value("specularPower")}, "alphaMode": {value("alphaMode")},',
        f'      "alpha": {value("alpha")}, "alphaCutoff": {value("alphaCutoff")},',
        f'      "twoSided": {value("twoSided")}, "uvScale": {value("uvScale")},',
        f'      "wetResponse": {value("wetResponse")},',
        f'      "snowResponse": {value("snowResponse")},',
        f'      "footstepSurface": {value("footstepSurface")},',
        f'      "audioAbsorption": {value("audioAbsorption")},',
        f'      "effectTierS": {value("effectTierS")}, "effectTierE": {value("effectTierE")}',
        "    },",
    ))


def write() -> int:
    problems = validate_definitions()
    if problems:
        for problem in problems:
            print(f"exterior_materials: {problem}", file=sys.stderr)
        return 1
    text = MATERIALS_FILE.read_text(encoding="utf-8")
    if text.count(BEGIN) != 1 or text.count(END) != 1:
        print("exterior_materials: generated markers are missing or duplicated", file=sys.stderr)
        return 1
    rendered = "\n".join(render_row(material_row(exterior))
                           for exterior in sorted(EXTERIORS, key=lambda item: item.material_id))
    before, remainder = text.split(BEGIN, 1)
    _, after = remainder.split(END, 1)
    MATERIALS_FILE.write_text(before + BEGIN + "\n" + rendered + "\n" + END + after,
                              encoding="utf-8")
    siding_albedo, siding_normal = siding_texture_images()
    roof_albedo, roof_normal = roof_texture_images()
    SIDING_ALBEDO.write_bytes(png_bytes(siding_albedo))
    SIDING_NORMAL.write_bytes(png_bytes(siding_normal))
    ROOF_ALBEDO.write_bytes(png_bytes(roof_albedo))
    ROOF_NORMAL.write_bytes(png_bytes(roof_normal))
    print(f"exterior_materials: wrote {len(EXTERIORS)} materials, two "
          f"{SIDING_SIZE}x{SIDING_SIZE} painted-clapboard maps and two "
          f"{ROOF_SIZE}x{ROOF_SIZE} asphalt-shingle maps")
    return 0


def write_preview() -> int:
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    preview_image().save(PREVIEW, optimize=True)
    print(f"exterior_materials: wrote exterior review to {PREVIEW}")
    return 0


def selftest() -> int:
    problems = validate_definitions()
    if problems:
        for problem in problems:
            print(f"exterior_materials: {problem}", file=sys.stderr)
        return 1
    rows = [material_row(exterior) for exterior in EXTERIORS]
    if any(row["lightmapChannel"] != 1 or row["effectTierS"] != "DualTexture"
           for row in rows):
        print("exterior_materials: every exterior must remain a static lightmap receiver",
              file=sys.stderr)
        return 1
    if sum(row["snowResponse"]["coverable"] for row in rows) != 8:
        print("exterior_materials: exactly the downward-facing soffit must reject snow",
              file=sys.stderr)
        return 1
    siding_rows = [row for row in rows if row["id"].startswith("MAT_SIDING_")]
    if (len(siding_rows) != 3 or
            any(row["albedo"] != "Textures/Materials/siding_clapboard_albedo"
                or row["normal"] != "Textures/Materials/siding_clapboard_normal"
                for row in siding_rows)):
        print("exterior_materials: the three siding colours do not share the clapboard maps",
              file=sys.stderr)
        return 1
    albedo, normal = siding_texture_images()
    if png_bytes(albedo) != png_bytes(siding_texture_images()[0]) or \
            png_bytes(normal) != png_bytes(siding_texture_images()[1]):
        print("exterior_materials: clapboard map generation is not byte-deterministic",
              file=sys.stderr)
        return 1
    for coordinate in range(0, SIDING_SIZE, 37):
        if (abs(siding_height(coordinate, coordinate) -
                siding_height(coordinate + SIDING_SIZE, coordinate)) > 1e-12 or
                abs(siding_height(coordinate, coordinate) -
                    siding_height(coordinate, coordinate + SIDING_SIZE)) > 1e-12):
            print("exterior_materials: clapboard height field is not exactly periodic",
                  file=sys.stderr)
            return 1
    centre_x = SIDING_SIZE // 2
    values = [albedo.getpixel((centre_x, y))[0] for y in range(SIDING_SIZE)]
    minima = sum(values[y] < values[(y - 1) % SIDING_SIZE]
                 and values[y] <= values[(y + 1) % SIDING_SIZE] and values[y] < 190
                 for y in range(SIDING_SIZE))
    if minima != SIDING_COURSES_PER_METRE:
        print(f"exterior_materials: detected {minima} lap shadows, expected "
              f"{SIDING_COURSES_PER_METRE}", file=sys.stderr)
        return 1
    roof_row = next(row for row in rows if row["id"] == "MAT_ROOF_SHINGLE")
    if (roof_row["albedo"] != "Textures/Materials/roof_asphalt_shingle_albedo" or
            roof_row["normal"] != "Textures/Materials/roof_asphalt_shingle_normal" or
            roof_row["uvScale"] != [1.0, 1.0]):
        print("exterior_materials: roof does not use the one-metre asphalt-shingle pair",
              file=sys.stderr)
        return 1
    roof_albedo, roof_normal = roof_texture_images()
    if (png_bytes(roof_albedo) != png_bytes(roof_texture_images()[0]) or
            png_bytes(roof_normal) != png_bytes(roof_texture_images()[1])):
        print("exterior_materials: asphalt-shingle generation is not byte-deterministic",
              file=sys.stderr)
        return 1
    roof_values = [roof_albedo.getpixel((ROOF_SIZE // 3, y))[0]
                   for y in range(ROOF_SIZE)]
    roof_minima = sum(roof_values[y] < roof_values[(y - 1) % ROOF_SIZE]
                      and roof_values[y] <= roof_values[(y + 1) % ROOF_SIZE]
                      and roof_values[y] < 95 for y in range(ROOF_SIZE))
    if roof_minima < ROOF_COURSES_PER_METRE:
        print(f"exterior_materials: detected only {roof_minima} shingle laps, expected at least "
              f"{ROOF_COURSES_PER_METRE}", file=sys.stderr)
        return 1
    # The exposed slot is a short shingle-tab joint, not a ceramic grout line running through the
    # covered head. Sample one staggered boundary in the exposed and covered parts of course zero.
    boundary_x = 0
    exposed_y = round((0.45 / ROOF_COURSES_PER_METRE) * ROOF_SIZE)
    covered_y = round((0.90 / ROOF_COURSES_PER_METRE) * ROOF_SIZE)
    if not (roof_albedo.getpixel((boundary_x, exposed_y))[0] + 8 <
            roof_albedo.getpixel((ROOF_SIZE // 8, exposed_y))[0] and
            abs(roof_albedo.getpixel((boundary_x, covered_y))[0] -
                roof_albedo.getpixel((ROOF_SIZE // 8, covered_y))[0]) < 18):
        print("exterior_materials: shingle tabs do not stop below the covered head",
              file=sys.stderr)
        return 1
    print("exterior_materials: selftest passed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--check", action="store_true")
    group.add_argument("--write", action="store_true")
    group.add_argument("--preview", action="store_true")
    group.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.write:
        return write()
    if args.preview:
        return write_preview()
    if args.selftest:
        return selftest()
    return check()


if __name__ == "__main__":
    sys.exit(main())
