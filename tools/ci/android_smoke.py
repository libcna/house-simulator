#!/usr/bin/env python3
"""android_smoke.py -- the Android build's smoke test on a device or emulator (`HOUSE-03038`).

    tools/ci/android_smoke.py                      # the installed package, one attached device
    tools/ci/android_smoke.py --apk <path.apk>     # install this APK first

Drives the installed game through adb the way a player would, by touch only: the title's tap (the
audio gesture), Start from the main menu, 300 presented frames of the walk, the MENU button,
Settings, the quality row taken round its three presets back to where it was and the phone's Back
button out of the page, then Home and back, which must deactivate and
reactivate the game. Every step waits for the log line that proves it happened (the game logs to
logcat under the tag `cna-house`); an error or fatal log line, the process dying or a missed step
fails the run with the step named.

Touches are held for 0.6 s and the Back key is sent as a long press: XNA reads touch and keyboard
STATE once a frame, and a press shorter than a frame is never seen. Positions are the game's normalised UI coordinates inside its 16:9 view, which the
device letterboxes. Needs `adb` (`$ANDROID_HOME/platform-tools` or PATH). Offline tooling: not
runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

PACKAGE = "com.libcna.house"
FRAMES = 300
# Normalised positions in the game's 16:9 view (src/ui/MenuStack.cpp, src/ui/TouchHud.cpp).
TITLE = (0.50, 0.50)
START = (0.50, 0.35)
MENU_BUTTON = (0.95, 0.09)
PAUSE_SETTINGS = (0.50, 0.47)
QUALITY_ROW = (0.65, 0.111)


def adb_path() -> str:
    home = os.environ.get("ANDROID_HOME") or os.path.expanduser("~/Android/Sdk")
    candidate = Path(home) / "platform-tools" / "adb"
    return str(candidate) if candidate.is_file() else (shutil.which("adb") or "adb")


class Device:
    def __init__(self) -> None:
        self.adb = adb_path()
        size = re.search(r"(\d+)x(\d+)", self.run("shell", "wm size"))
        if size is None:
            raise RuntimeError("no device answers adb")
        width, height = sorted((int(size.group(1)), int(size.group(2))), reverse=True)  # landscape
        view_width = min(width, height * 16 / 9)
        view_height = view_width * 9 / 16
        self.origin = ((width - view_width) / 2, (height - view_height) / 2)
        self.view = (view_width, view_height)

    def run(self, *args: str) -> str:
        return subprocess.run([self.adb, *args], capture_output=True, text=True).stdout

    def touch(self, point: tuple[float, float]) -> None:
        x = round(self.origin[0] + point[0] * self.view[0])
        y = round(self.origin[1] + point[1] * self.view[1])
        self.run("shell", f"input swipe {x} {y} {x} {y} 600")

    def log(self) -> list[str]:
        return self.run("logcat", "-d", "-s", "cna-house").splitlines()

    def alive(self) -> bool:
        return bool(self.run("shell", f"pidof {PACKAGE}").strip())

    def until(self, needle: str, seconds: float, step: str, count: int = 1) -> None:
        end = time.time() + seconds
        seen = False
        while time.time() < end:
            if sum(needle in line for line in self.log()) >= count:
                return
            alive = self.alive()
            if seen and not alive:
                raise RuntimeError(f"{step}: the process is gone")
            seen = seen or alive
            time.sleep(1.0)
        raise RuntimeError(f"{step}: no '{needle}' within {seconds:.0f} s")

    def presented_frames(self, seconds: float) -> int:
        layer = None
        for line in self.run("shell", "dumpsys SurfaceFlinger --list").splitlines():
            found = re.search(r"(SurfaceView\[" + re.escape(PACKAGE) + r"/[^\]]*\]\(BLAST\)#\d+)", line)
            if found:
                layer = found.group(1)
        if layer is None:
            raise RuntimeError("walk: no game surface in SurfaceFlinger")
        presents: set[int] = set()
        end = time.time() + seconds
        while time.time() < end:
            values = self.run("shell", f"dumpsys SurfaceFlinger --latency '{layer}'").split()[1:]
            presents.update(int(values[i]) for i in range(1, len(values), 3) if 0 < int(values[i]) < 2**62)
            time.sleep(1.0)
        return len(presents)


def run(apk: Path | None) -> int:
    device = Device()
    if apk is not None and "Success" not in device.run("install", "-r", str(apk)):
        print(f"android_smoke: {apk} did not install", file=sys.stderr)
        return 2
    step = "launch"
    try:
        device.run("shell", f"am force-stop {PACKAGE}")
        device.run("logcat", "-c")
        device.run("shell", f"monkey -p {PACKAGE} -c android.intent.category.LAUNCHER 1")
        device.until("audio waiting for a user gesture", 300, "the game loads to the title")
        step = "title"
        time.sleep(2)
        device.touch(TITLE)
        device.until("audio ready", 60, "the title's tap opens audio")
        time.sleep(2)
        step = "start"
        device.touch(START)
        device.until("walking in", 300, "Start loads the house")
        step = "walk"
        frames = device.presented_frames(20)
        if frames < FRAMES:
            raise RuntimeError(f"the walk presented {frames} frames in 20 s, fewer than {FRAMES}")
        step = "settings"
        device.touch(MENU_BUTTON)
        time.sleep(2)
        device.touch(PAUSE_SETTINGS)
        time.sleep(2)
        device.touch(QUALITY_ROW)
        device.until("settings quality", 60, "the quality row changes the preset")
        # Settings persist on the device: two more taps take the row round its three presets and
        # back to where the player had it.
        for count in (2, 3):
            time.sleep(1)
            device.touch(QUALITY_ROW)
            device.until("settings quality", 60, "the quality row cycles back", count=count)
        device.run("shell", "input keyevent --longpress KEYCODE_BACK")
        time.sleep(1)
        step = "lifecycle"
        # SDL holds the game thread while the activity is paused, so the deactivation is delivered
        # together with the reactivation when the game returns.
        device.run("shell", "input keyevent KEYCODE_HOME")
        time.sleep(5)
        if not device.alive():
            raise RuntimeError("the process did not survive Home")
        device.run("shell", f"monkey -p {PACKAGE} -c android.intent.category.LAUNCHER 1")
        device.until("deactivated", 30, "Home deactivates the game")
        device.until("activated: simulation resumed", 30, "returning reactivates the game", count=2)
        problems = [line for line in device.log() if "[E]" in line or "[F]" in line]
        if problems:
            raise RuntimeError("the log reported: " + " | ".join(problems[:5]))
        print(f"android_smoke: passed -- title, Start, {frames} walk frames in 20 s, the quality row round "
              "its presets, Home/return, no errors")
        return 0
    except Exception as error:
        print(f"android_smoke: FAILED at {step}: {error}", file=sys.stderr)
        return 1
    finally:
        device.run("shell", f"am force-stop {PACKAGE}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--apk", type=Path, help="install this APK before the run")
    arguments = parser.parse_args()
    return run(arguments.apk.resolve() if arguments.apk else None)


if __name__ == "__main__":
    sys.exit(main())
