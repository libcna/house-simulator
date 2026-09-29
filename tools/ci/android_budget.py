#!/usr/bin/env python3
"""android_budget.py -- the Android package against its budget (`HOUSE-03033`).

    tools/ci/android_budget.py --apk-dir build-probe/android-gradle/app/outputs/apk \
        --stage build-probe/android-stage
    tools/ci/android_budget.py --selftest

`cna-house.md` §71.5: the APK plus any OBB at most 400 MB. House ships no OBB, so the limit applies
to each APK as downloaded. Every APK must also carry the staged content tree (`stage_content.py`)
exactly -- the same files at the same sizes under `assets/` -- because on Android the content root
is the APK's assets and a file the stage has but the APK lacks is only found missing at run time.
Exit status 1 when either fails. The Gradle build runs this after every assemble.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import io
import sys
import zipfile
from pathlib import Path

LIMIT = 400_000_000


def staged_files(stage: Path) -> dict[str, int]:
    return {"assets/" + p.relative_to(stage).as_posix(): p.stat().st_size
            for p in sorted(stage.rglob("*")) if p.is_file()}


def check(apk_name: str, apk_size: int, archive: zipfile.ZipFile, staged: dict[str, int]) -> list[str]:
    problems = []
    entries = {info.filename: info.file_size for info in archive.infolist()}
    content = sum(info.compress_size for info in archive.infolist() if info.filename.startswith("assets/"))
    native = sum(info.compress_size for info in archive.infolist() if info.filename.startswith("lib/"))
    print(f"android_budget: {apk_name}: {apk_size / 1e6:.1f} MB (content {content / 1e6:.1f} MB, "
          f"native {native / 1e6:.1f} MB) of {LIMIT / 1e6:.0f} MB")
    if apk_size > LIMIT:
        problems.append(f"{apk_name} is {apk_size / 1e6:.1f} MB, over the {LIMIT / 1e6:.0f} MB budget")
    missing = [name for name in staged if name not in entries]
    wrong = [name for name, size in staged.items() if name in entries and entries[name] != size]
    extra = [name for name in entries if name.startswith("assets/") and name not in staged]
    for label, names in (("missing", missing), ("different size", wrong), ("not staged", extra)):
        if names:
            problems.append(f"{apk_name}: {len(names)} content file(s) {label}, first {names[0]}")
    return problems


def selftest() -> int:
    staged = {"assets/content/world/chunks.bin": 3}

    def apk(files: dict[str, bytes]) -> zipfile.ZipFile:
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, "w") as archive:
            for name, data in files.items():
                archive.writestr(name, data)
        return zipfile.ZipFile(io.BytesIO(buffer.getvalue()))

    claims = [
        ("a complete APK under the limit passes", not check("ok", 10, apk({"assets/content/world/chunks.bin": b"abc"}), staged)),
        ("an APK over the limit fails", bool(check("big", LIMIT + 1, apk({"assets/content/world/chunks.bin": b"abc"}), staged))),
        ("a missing content file fails", bool(check("short", 10, apk({"lib/x.so": b""}), staged))),
        ("a content file of another size fails", bool(check("stale", 10, apk({"assets/content/world/chunks.bin": b"ab"}), staged))),
        ("an unstaged asset fails", bool(check("extra", 10, apk({"assets/content/world/chunks.bin": b"abc",
                                                                  "assets/content/dev/x": b""}), staged))),
    ]
    for claim, held in claims:
        print(("PASS " if held else "FAIL ") + claim)
    return 0 if all(held for _, held in claims) else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--apk-dir", type=Path, help="checked recursively for *.apk")
    parser.add_argument("--stage", type=Path, help="the staged tree the APK's assets come from")
    parser.add_argument("--selftest", action="store_true")
    arguments = parser.parse_args()
    if arguments.selftest:
        return selftest()
    if arguments.apk_dir is None or arguments.stage is None:
        parser.error("--apk-dir and --stage are required")
    apks = sorted(arguments.apk_dir.rglob("*.apk"))
    if not apks:
        print(f"android_budget: no APK under {arguments.apk_dir}", file=sys.stderr)
        return 1
    staged = staged_files(arguments.stage)
    problems = []
    for path in apks:
        with zipfile.ZipFile(path) as archive:
            problems += check(path.name, path.stat().st_size, archive, staged)
    for problem in problems:
        print("android_budget: " + problem, file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
