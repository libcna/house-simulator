#!/usr/bin/env python3
"""package_android.py -- the installable Android package (`HOUSE-03041`).

    (cd android && ./gradlew assembleRelease)
    tools/ci/package_android.py

Writes `build-probe/package/cna-house-<version>-android/`: the release APK (arm64-v8a, signed with
the local debug key, content staged at the Android preset's LOD level and checked against its
budget by the Gradle build), with LICENSE, NOTICE.md, licenses/ and a README on installing it.
Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import shutil
import sys
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
APK = REPO / "build-probe" / "android-gradle" / "app" / "outputs" / "apk" / "release" / "app-release.apk"

README = """CNA House {version} for Android

Install on a phone or tablet running Android 7.0 (API 24) or later with a 64-bit ARM processor and
OpenGL ES 3.0, either from a computer with USB debugging enabled:

    adb install cna-house-{version}-arm64.apk

or by opening the APK on the device and allowing the install. The package is signed with a
development key, not a store key; a store release is signed with its own.

Controls: the left stick moves, a drag anywhere else looks, WALK switches walking and running,
MENU pauses. Settings are kept in the app's private storage. The Android quality preset is chosen
automatically (shadows off, reduced view distance and vegetation detail).

Measured on the Android emulator only (API 35, GPU-accelerated), not yet on a physical phone.

The programme is Ms-PL (LICENSE); runtime notices are in NOTICE.md and every content asset's
author and licence is in licenses/THIRD-PARTY-ASSETS.md.
"""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--apk", type=Path, default=APK, help="the release APK (default: Gradle's output)")
    arguments = parser.parse_args()
    apk = arguments.apk.resolve()
    if not apk.is_file():
        print(f"package_android: {apk} does not exist; run ./gradlew assembleRelease in android/",
              file=sys.stderr)
        return 1
    with zipfile.ZipFile(apk) as archive:
        abis = sorted({name.split("/")[1] for name in archive.namelist() if name.startswith("lib/")})
    if abis != ["arm64-v8a"]:
        print(f"package_android: {apk.name} is built for {abis}, not the shipped arm64-v8a", file=sys.stderr)
        return 1

    version = (REPO / "VERSION").read_text().strip()
    out = REPO / "build-probe" / "package" / f"cna-house-{version}-android"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    shutil.copy2(apk, out / f"cna-house-{version}-arm64.apk")
    for document in ("LICENSE", "NOTICE.md"):
        shutil.copy2(REPO / document, out / document)
    shutil.copytree(REPO / "licenses", out / "licenses")
    (out / "README.txt").write_text(README.format(version=version))
    size = sum(p.stat().st_size for p in out.rglob("*") if p.is_file())
    print(f"package_android: wrote {out} ({size / 1e6:.1f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
