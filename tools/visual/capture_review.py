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
    ("entrance-foyer", "0.00,0.60,-14.90,0.0,0.0"),
    ("central-hall", "0.00,0.60,-20.65,0.0,0.0"),
    ("living-room", "-5.20,0.60,-17.25,90.0,0.0"),
    ("living-composition", "-3.10,0.60,-18.50,270.0,0.0"),
    ("kitchen", "-3.00,0.60,-25.05,90.0,0.0"),
    ("family-room", "5.45,0.60,-24.55,270.0,0.0"),
    ("family-composition", "3.20,0.60,-25.80,90.0,0.0"),
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("label", help="capture directory label, normally <short-head>-<round>")
    parser.add_argument("--binary", type=Path, default=REPO / "build" / "cna-house")
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

    for name, pose in POSES:
        output = destination / f"{name}.png"
        command = [
            str(binary),
            "--scene=walk",
            f"--player={pose}",
            "--tier=s",
            "--quality=high",
            f"--seed={SEED}",
            "--time=10.5",
            "--freeze-time",
            "--weather=W_CLEAR",
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
