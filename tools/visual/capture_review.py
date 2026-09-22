#!/usr/bin/env python3
"""Capture fixed, human-reviewed views used by the visual-convergence sprint.

This is deliberately separate from pixel-golden render tests.  The camera, simulation inputs and
software renderer are fixed so two rounds are compositionally comparable; a person still decides
whether the newer house actually looks better.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SEED = 6840335469064670721
LEGACY_POSES = (
    ("exterior-front", "0.00,0.00,5.20,0.0,3.0"),
    ("garage-approach", "13.00,0.00,-4.00,0.0,0.0"),
    # HOUSE-00937: the road camera proves the whole arrival composition but the fence hides its
    # ground-level joinery. This path-height view keeps both foundation beds, porch and front door
    # in one deterministic frame so planting scale and accidental porch/driveway overlap remain
    # human-reviewable.
    ("front-path", "0.00,0.60,-8.50,0.0,0.0"),
    # HOUSE-01288: this low, downward approach sees the alternating path fixtures at human scale
    # and keeps their clear walking lane visible; the road and porch cameras retain the context.
    ("front-walk-bollards", "0.00,0.00,-5.00,0.0,-15.0"),
    # HOUSE-01287: stand on the canonical front balcony and look directly at its over-door
    # lantern. The road view proves composition; this one proves physical scale, mounting height,
    # daytime appearance and the bounded pool on the door rather than a floating debug source.
    ("front-balcony-light", "0.00,3.65,-12.10,0.0,12.0"),
    ("entrance-foyer", "0.00,0.60,-14.90,0.0,0.0"),
    # HOUSE-01071: the horizontal door camera sees only the leading edge of the entry rug.
    # A fixed, gently downward eye-height view checks its whole footprint and door clearance.
    ("foyer-entry-floor", "0.00,0.60,-15.00,0.0,-15.0"),
    # HOUSE-00940: the route camera sees the foyer/living pair only at the extreme left edge and
    # the living-room camera sees its unlit face. This measured foyer-side view keeps both raised
    # panels, the centre meeting stile and the matched passage/dummy hardware reviewable.
    ("foyer-living-doors", "0.40,0.60,-16.30,270.0,0.0"),
    ("foyer-facing-front", "0.00,0.60,-16.70,180.0,0.0"),
    # HOUSE-00489: the foyer sees only the side of the U flight. Keep its actual foot and the
    # first-floor exit in the same fixed review set so a passable stair is not judged by one angle.
    ("main-stair-foot", "4.10,0.60,-14.70,0.0,0.0"),
    ("main-stair-l1-exit", "2.95,3.65,-14.65,270.0,0.0"),
    ("central-hall", "0.00,0.60,-20.65,0.0,0.0"),
    # HOUSE-01065: the route view looks north through the hall and therefore only catches these
    # side-wall gallery clusters obliquely. Paired cross-hall views keep all nine frame positions,
    # their measured hanging band and the adjacent clear doorways directly reviewable.
    ("hall-gallery-west", "0.60,0.60,-19.47,270.0,0.0"),
    ("hall-gallery-east", "-0.60,0.60,-19.47,90.0,0.0"),
    ("kitchen-from-hall", "0.00,0.60,-23.55,285.0,0.0"),
    ("living-room", "-5.20,0.60,-17.25,90.0,0.0"),
    # HOUSE-01066: the straight room view proves wall composition but is too distant to judge the
    # 88-key instrument's joinery, folio, hardware and floor contact. This clear standing position
    # stays west of the bench proxy and looks squarely at the piano without changing the room.
    ("living-piano-detail", "-4.45,0.60,-18.65,90.0,0.0"),
    ("living-composition", "-3.10,0.60,-18.50,270.0,0.0"),
    # HOUSE-01048: look east along the formal dining room's long axis so table scale, all eight
    # chair positions, the hall door and the living/kitchen circulation edges remain reviewable.
    ("dining-room", "-7.70,0.60,-21.60,90.0,0.0"),
    # HOUSE-01069: the long-axis camera proves the table but hides the north-wall sideboard.
    # This reciprocal standing pose keeps the cabinet, both linked lamps and the seating
    # clearance in one unchanged-time room-scale comparison.
    ("dining-sideboard", "-6.10,0.60,-22.95,210.0,0.0"),
    # HOUSE-01040: -3.0,-25.05 was inside the newly authored island's proxy. The paired
    # east/west pose moves to the measured 1.04 m circulation lane beside its east end.
    ("kitchen", "-1.10,0.60,-25.05,90.0,0.0"),
    ("kitchen-facing-west", "-1.10,0.60,-25.05,270.0,0.0"),
    # The kitchen's open service passage exposes this room on the L0 route. The long kitchen
    # camera sees only a narrow recess, so review its actual envelope from the threshold too.
    ("butlers-from-kitchen", "-8.55,0.60,-24.00,270.0,0.0"),
    ("family-room", "5.45,0.60,-24.55,270.0,0.0"),
    ("family-composition", "3.20,0.60,-25.80,90.0,0.0"),
    # HOUSE-01050: the paired family-room views look across the seating and kitchen opening but
    # barely exercise the television/media focal wall. This measured head-height view makes the
    # source screen, surround finish and their alignment a permanent visual-review boundary.
    ("family-media", "5.10,0.60,-26.25,180.0,0.0"),
    # HOUSE-00771: the interior route views see the rear landscape only through picture-window
    # glass.  This oblique terrace-level camera keeps furniture scale, circulation and the
    # terrace-to-lawn transition directly reviewable without replacing those interior views.
    ("rear-terrace", "0.00,0.45,-32.80,0.0,-8.0"),
    # The reciprocal lawn view proves that the terrace composition and the freestanding rear-lawn
    # pieces belong to the house rather than reading as isolated catalogue props against a fence.
    ("backyard-to-house", "-8.00,-0.28,-42.00,135.0,0.0"),
    # A room-side reciprocal view keeps the exact defect that selected HOUSE-00771 in the fixed
    # set: the rear composition must contribute domestic depth through the family picture window.
    ("family-garden-view", "5.70,0.60,-23.05,0.0,0.0"),
    # HOUSE-01068: the kitchen threshold proves the breakfast group and the uninterrupted route
    # to the slider; the reciprocal view proves fitted bar depth, shelf dressing and floor contact.
    ("sunroom-breakfast", "-2.00,0.60,-27.35,340.0,0.0"),
    ("sunroom-wet-bar", "-2.00,0.60,-31.45,180.0,0.0"),
    # HOUSE-01074: the route/breakfast/bar cameras mostly look along the glazing and miss the
    # east-side reading bay. Review the whole group and its slider clearance at eye height.
    ("sunroom-lounge", "-1.50,0.60,-29.80,90.0,0.0"),
)
NEW_POSES = (
    ("basement-hall", "0.00,-2.30,-15.20,0.0,0.0"),
    ("basement-cinema", "3.00,-2.30,-25.00,90.0,0.0"),
    ("basement-workshop", "-11.90,-2.30,-16.30,90.0,0.0"),
    ("basement-gym", "-7.40,-2.30,-16.30,90.0,0.0"),
    ("ground-office", "-11.90,0.60,-16.30,90.0,0.0"),
    ("ground-mudroom", "5.50,0.60,-16.30,90.0,0.0"),
    ("garage-interior", "9.70,0.15,-17.50,90.0,0.0"),
    ("first-landing", "0.00,3.65,-15.00,0.0,0.0"),
    ("first-hall", "0.00,3.65,-18.90,0.0,0.0"),
    ("master-bedroom", "-6.20,3.65,-24.80,90.0,0.0"),
    ("master-bath", "-10.00,3.65,-26.20,225.0,0.0"),
    ("bedroom-2", "3.00,3.65,-25.00,90.0,0.0"),
    ("second-landing", "0.00,6.55,-15.00,0.0,0.0"),
    ("second-hall", "0.00,6.55,-18.90,0.0,0.0"),
    ("library", "-7.40,6.55,-16.30,90.0,0.0"),
    ("games-room", "-6.20,6.55,-24.80,90.0,0.0"),
    ("sitting-room", "3.00,6.55,-25.00,90.0,0.0"),
    ("bedroom-6", "-11.90,6.55,-16.30,90.0,0.0"),
    ("attic-room", "-5.00,9.30,-20.50,90.0,0.0"),
    ("attic-store-west", "-11.90,9.30,-20.50,90.0,0.0"),
    ("main-stair-l2-exit", "3.55,6.55,-15.50,0.0,0.0"),
    ("basement-stair", "4.30,-2.30,-19.50,180.0,0.0"),
    ("garden", "-15.00,-0.35,-42.80,180.0,0.0"),
    ("neighbourhood-street", "0.00,0.00,20.00,0.0,0.0"),
)
POSES = LEGACY_POSES + NEW_POSES

# The camera's owning cell is explicit: a pose can stand on a shared stair boundary, and a view of
# a hero composition can stand in a secondary approach cell.  The optional third field marks only
# the compact representative views captured in overcast, not every day/night view in a hero cell.
POSE_DETAILS = {
    "exterior-front": ("Z-EXF", "EXT_ROAD", "H1"),
    "garage-approach": ("Z-EXF", "EXT_DRIVEWAY", None),
    "front-path": ("Z-EXF", "EXT_WALK", "H1"),
    "front-walk-bollards": ("Z-EXF", "EXT_WALK", None),
    "front-balcony-light": ("Z-L1", "L1_BALCONY_FRONT", None),
    "entrance-foyer": ("Z-L0M", "L0_FOYER", None),
    "foyer-entry-floor": ("Z-L0M", "L0_FOYER", None),
    "foyer-living-doors": ("Z-L0M", "L0_FOYER", None),
    "foyer-facing-front": ("Z-L0M", "L0_FOYER", None),
    "main-stair-foot": ("Z-STAIR", "L0_STAIR_MAIN", None),
    "main-stair-l1-exit": ("Z-STAIR", "L1_STAIR_MAIN", None),
    "central-hall": ("Z-L0M", "L0_HALL", None),
    "hall-gallery-west": ("Z-L0M", "L0_HALL", None),
    "hall-gallery-east": ("Z-L0M", "L0_HALL", None),
    "kitchen-from-hall": ("Z-L0M", "L0_KITCHEN", None),
    "living-room": ("Z-L0M", "L0_LIVING", None),
    "living-piano-detail": ("Z-L0M", "L0_LIVING", None),
    "living-composition": ("Z-L0M", "L0_LIVING", "H2"),
    "dining-room": ("Z-L0M", "L0_DINING", None),
    "dining-sideboard": ("Z-L0M", "L0_DINING", None),
    "kitchen": ("Z-L0M", "L0_KITCHEN", None),
    "kitchen-facing-west": ("Z-L0M", "L0_KITCHEN", "H3"),
    "butlers-from-kitchen": ("Z-L0M", "L0_BUTLERS", None),
    "family-room": ("Z-L0M", "L0_FAMILY", None),
    "family-composition": ("Z-L0M", "L0_FAMILY", None),
    "family-media": ("Z-L0M", "L0_FAMILY", None),
    "rear-terrace": ("Z-EXR", "EXT_TERRACE", None),
    "backyard-to-house": ("Z-EXR", "EXT_BACKYARD", None),
    "family-garden-view": ("Z-L0M", "L0_FAMILY", None),
    "sunroom-breakfast": ("Z-L0M", "L0_SUNROOM", None),
    "sunroom-wet-bar": ("Z-L0M", "L0_SUNROOM", None),
    "sunroom-lounge": ("Z-L0M", "L0_SUNROOM", None),
    "basement-hall": ("Z-B1", "B1_HALL", None),
    "basement-cinema": ("Z-B1", "B1_CINEMA", "H6"),
    "basement-workshop": ("Z-B1", "B1_WORKSHOP", None),
    "basement-gym": ("Z-B1", "B1_GYM", None),
    "ground-office": ("Z-L0S", "L0_OFFICE", None),
    "ground-mudroom": ("Z-L0S", "L0_MUDROOM", None),
    "garage-interior": ("Z-GAR", "L0_GARAGE", None),
    "first-landing": ("Z-L1", "L1_LANDING", None),
    "first-hall": ("Z-L1", "L1_HALL", None),
    "master-bedroom": ("Z-L1", "L1_MASTER_BED", None),
    "master-bath": ("Z-L1", "L1_MASTER_BATH", None),
    "bedroom-2": ("Z-L1", "L1_BED2", None),
    "second-landing": ("Z-L2", "L2_LANDING", None),
    "second-hall": ("Z-L2", "L2_HALL", None),
    "library": ("Z-L2", "L2_LIBRARY", "H5"),
    "games-room": ("Z-L2", "L2_GAMES", None),
    "sitting-room": ("Z-L2", "L2_SITTING", None),
    "bedroom-6": ("Z-L2", "L2_BED6", None),
    "attic-room": ("Z-L3", "L3_ROOM", None),
    "attic-store-west": ("Z-L3", "L3_STORE_W", None),
    "main-stair-l2-exit": ("Z-STAIR", "L2_STAIR_MAIN", None),
    "basement-stair": ("Z-STAIR", "B1_STAIR", None),
    "garden": ("Z-EXR", "EXT_GARDEN", None),
    "neighbourhood-street": ("Z-STR", "EXT_WORLD", None),
}
SCENARIOS = {
    "clear-day": (10.5, "W_CLEAR"),
    "clear-noon": (12.0, "W_CLEAR"),
    "overcast-day": (10.5, "W_OVERCAST"),
    "clear-overcast": (10.5, "W_OVERCAST"),
    "clear-night": (22.0, "W_CLEAR"),
}


def load_zones() -> dict:
    try:
        return json.loads((REPO / "docs" / "zones.json").read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RuntimeError(f"cannot read docs/zones.json: {error}") from error


def coverage_problems(document: dict) -> list[str]:
    """Check the reduced-plan coverage and keep the zone manifest in sync with this script."""
    found: list[str] = []
    pose_names = [name for name, _ in POSES]
    if len(LEGACY_POSES) != 32:
        found.append(f"the retained legacy set has {len(LEGACY_POSES)} poses, expected 32")
    if len(pose_names) != len(set(pose_names)):
        found.append("pose names are not unique")
    if set(pose_names) != set(POSE_DETAILS):
        found.append("POSES and POSE_DETAILS name different pose sets")

    zones = {zone.get("id"): zone for zone in document.get("zones", [])
             if isinstance(zone, dict)}
    cells: dict[str, tuple[str, str | None]] = {}
    manifest_poses: dict[str, str] = {}
    for zone_id, zone in zones.items():
        for row in zone.get("cells", []):
            if isinstance(row, dict) and isinstance(row.get("id"), str):
                cells[row["id"]] = (zone_id, row.get("tier"))
        for name in zone.get("reviewPoses", []):
            if name in manifest_poses:
                found.append(f"{name}: assigned to both {manifest_poses[name]} and {zone_id}")
            manifest_poses[name] = zone_id

    for name, _ in POSES:
        zone_id, cell_id, _ = POSE_DETAILS[name]
        if zone_id not in zones:
            found.append(f"{name}: unknown zone {zone_id}")
        if cell_id not in cells:
            found.append(f"{name}: unknown cell {cell_id}")
        elif cells[cell_id][0] != zone_id:
            found.append(f"{name}: {cell_id} belongs to {cells[cell_id][0]}, not {zone_id}")
        elif cells[cell_id][1] == "U":
            found.append(f"{name}: utility cell {cell_id} must be judged from the zone walk")
        if manifest_poses.get(name) != zone_id:
            found.append(f"{name}: docs/zones.json does not assign it to {zone_id}")
    for name in sorted(set(manifest_poses) - set(pose_names)):
        found.append(f"docs/zones.json assigns unknown review pose {name}")

    covered_cells = {cell_id for zone_id, cell_id, hero in POSE_DETAILS.values()}
    for cell_id, (zone_id, tier) in sorted(cells.items()):
        if tier == "M" and cell_id not in covered_cells:
            found.append(f"{zone_id}: main cell {cell_id} has no fixed pose")
    for zone_id, zone in zones.items():
        secondary = {row["id"] for row in zone.get("cells", [])
                     if isinstance(row, dict) and row.get("tier") == "S"}
        if secondary and not (secondary & covered_cells):
            found.append(f"{zone_id}: no fixed pose stands in a secondary cell")
    # HOUSE-03206 retired H4 and H7; the five remaining hero ids keep their numbers.
    hero_counts = {hero: 0 for hero in ("H1", "H2", "H3", "H5", "H6")}
    for _, _, hero in POSE_DETAILS.values():
        if hero in hero_counts:
            hero_counts[hero] += 1
    for hero, count in hero_counts.items():
        minimum = 2 if hero == "H1" else 1
        if count < minimum:
            found.append(f"{hero}: {count} representative pose(s), needs at least {minimum}")
    return found


def make_contact_sheet(destination: Path, names: list[str], title: str, output: Path) -> None:
    """Write one labelled sheet from the screenshots already captured in ``destination``."""
    from PIL import Image, ImageDraw

    tile_width, tile_height, label_height = 384, 216, 28
    columns = min(4, max(1, len(names)))
    rows = (len(names) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * tile_width, 36 + rows * (tile_height + label_height)),
                      (18, 18, 20))
    draw = ImageDraw.Draw(sheet)
    draw.text((10, 10), title, fill=(245, 245, 245))
    resampling = getattr(Image, "Resampling", Image).LANCZOS
    for index, name in enumerate(names):
        with Image.open(destination / f"{name}.png") as source:
            frame = source.convert("RGB")
            frame.thumbnail((tile_width, tile_height), resampling)
        left = (index % columns) * tile_width
        top = 36 + (index // columns) * (tile_height + label_height)
        sheet.paste(frame, (left + (tile_width - frame.width) // 2, top))
        draw.text((left + 8, top + tile_height + 6), name, fill=(245, 245, 245))
    sheet.save(output)


def selected_poses(zone: str | None, scenario: str) -> list[tuple[str, str]]:
    poses = list(POSES)
    if zone is not None:
        poses = [pose for pose in poses if POSE_DETAILS[pose[0]][0] == zone]
    if scenario in {"clear-overcast", "overcast-day"}:
        poses = [pose for pose in poses if POSE_DETAILS[pose[0]][2] is not None]
    return poses


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("label", nargs="?",
                        help="capture directory label, normally <short-head>-<round>")
    parser.add_argument("--binary", type=Path, default=REPO / "build" / "cna-house")
    parser.add_argument("--scenario", choices=SCENARIOS, default="clear-day",
                        help="fixed time/weather review condition (default: clear-day)")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--zone", choices=sorted({value[0] for value in POSE_DETAILS.values()}),
                           help="capture only one planning zone")
    selection.add_argument("--all-zones", action="store_true",
                           help="capture every planning zone (the default)")
    parser.add_argument("--check", action="store_true",
                        help="validate fixed-view coverage and exit without capturing")
    parser.add_argument("--light-on", action="append", default=[], metavar="GROUP",
                        help="turn on an authored manual group for this review set (repeatable)")
    parser.add_argument("--light-off", action="append", default=[], metavar="GROUP",
                        help="turn off an authored group for this review set (repeatable)")
    args = parser.parse_args()

    try:
        found = coverage_problems(load_zones())
    except RuntimeError as error:
        print(f"capture_review: {error}", file=sys.stderr)
        return 1
    if found:
        for problem in found:
            print(f"capture_review: {problem}", file=sys.stderr)
        return 1
    if args.check:
        print(f"capture_review: {len(POSES)} fixed poses cover 11 zones, 12 main cells and "
              "5 hero areas")
        return 0
    if args.label is None:
        parser.error("label is required unless --check is used")

    binary = args.binary.resolve()
    if not binary.is_file():
        parser.error(f"game binary does not exist: {binary}")
    destination = REPO / "docs" / "visual-review" / "captures" / args.label
    destination.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ)
    environment.update({
        "SDL_VIDEODRIVER": "offscreen",
        "SDL_AUDIODRIVER": "dummy",
        "LIBGL_ALWAYS_SOFTWARE": "1",
    })
    # A review set must show the authored new-game state, never whichever switches a developer's
    # interactive save happened to leave on or off. Keep one clean profile alive for the complete
    # set so every view shares the same deterministic starting state without touching user data.
    time_of_day, weather = SCENARIOS[args.scenario]
    poses = selected_poses(args.zone, args.scenario)
    if not poses:
        parser.error(f"{args.zone or 'the selected set'} has no {args.scenario} hero view")

    with tempfile.TemporaryDirectory(prefix="cnahouse-visual-review-") as review_data_home:
        environment["XDG_DATA_HOME"] = review_data_home
        for name, pose in poses:
            output = destination / f"{name}.png"
            command = [
                str(binary),
                "--scene=walk",
                f"--player={pose}",
                "--tier=s",
                "--quality=high",
                f"--seed={SEED}",
                f"--time={time_of_day}",
                "--freeze-time",
                f"--weather={weather}",
                *(f"--light-on={group}" for group in args.light_on),
                *(f"--light-off={group}" for group in args.light_off),
                "--no-audio",
                "--screenshot-frame=3",
                f"--screenshot={output}",
            ]
            print(f"capture_review: {name} -> {output.relative_to(REPO)}", flush=True)
            completed = subprocess.run(command, cwd=REPO, env=environment, check=False,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            if completed.returncode != 0 or not output.is_file():
                print(completed.stdout, file=sys.stderr)
                return completed.returncode or 1

    by_zone: dict[str, list[str]] = {}
    for name, _ in poses:
        by_zone.setdefault(POSE_DETAILS[name][0], []).append(name)
    for zone_id, names in by_zone.items():
        make_contact_sheet(destination, names, f"{zone_id} · {args.scenario}",
                           destination / f"contact-{zone_id}.png")
    if len(by_zone) > 1:
        make_contact_sheet(destination, [name for name, _ in poses],
                           f"All zones · {args.scenario}",
                           destination / "contact-all-zones.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
