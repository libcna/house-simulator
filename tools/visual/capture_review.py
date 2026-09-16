#!/usr/bin/env python3
"""Capture fixed, human-reviewed views used by the visual-convergence sprint.

This is deliberately separate from pixel-golden render tests.  The camera, simulation inputs and
software renderer are fixed so two rounds are compositionally comparable; a person still decides
whether the newer house actually looks better.
"""

from __future__ import annotations

import argparse
import os
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SEED = 6840335469064670721
POSES = (
    ("exterior-front", "0.00,0.00,5.20,0.0,3.0"),
    ("garage-approach", "13.00,0.00,-4.00,0.0,0.0"),
    ("entrance-foyer", "0.00,0.60,-14.90,0.0,0.0"),
    ("foyer-facing-front", "0.00,0.60,-16.70,180.0,0.0"),
    ("central-hall", "0.00,0.60,-20.65,0.0,0.0"),
    ("kitchen-from-hall", "0.00,0.60,-23.55,285.0,0.0"),
    ("living-room", "-5.20,0.60,-17.25,90.0,0.0"),
    ("living-composition", "-3.10,0.60,-18.50,270.0,0.0"),
    # HOUSE-01040: -3.0,-25.05 was inside the newly authored island's proxy. The paired
    # east/west pose moves to the measured 1.04 m circulation lane beside its east end.
    ("kitchen", "-1.10,0.60,-25.05,90.0,0.0"),
    ("kitchen-facing-west", "-1.10,0.60,-25.05,270.0,0.0"),
    ("family-room", "5.45,0.60,-24.55,270.0,0.0"),
    ("family-composition", "3.20,0.60,-25.80,90.0,0.0"),
)
SCENARIOS = {
    "clear-day": (10.5, "W_CLEAR"),
    "clear-noon": (12.0, "W_CLEAR"),
    "overcast-day": (10.5, "W_OVERCAST"),
    "clear-night": (22.0, "W_CLEAR"),
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("label", help="capture directory label, normally <short-head>-<round>")
    parser.add_argument("--binary", type=Path, default=REPO / "build" / "cna-house")
    parser.add_argument("--scenario", choices=SCENARIOS, default="clear-day",
                        help="fixed time/weather review condition (default: clear-day)")
    args = parser.parse_args()

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
    time_of_day, weather = SCENARIOS[args.scenario]

    for name, pose in POSES:
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
            "--no-audio",
            "--screenshot-frame=3",
            f"--screenshot={output}",
        ]
        print(f"capture_review: {name} -> {output.relative_to(REPO)}", flush=True)
        completed = subprocess.run(command, cwd=REPO, env=environment, check=False)
        if completed.returncode != 0 or not output.is_file():
            return completed.returncode or 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
