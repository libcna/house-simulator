#!/usr/bin/env python3
"""package_linux.py -- the distributable Linux build (`HOUSE-02789`).

    tools/ci/package_linux.py --build-dir build-probe            # stage and archive
    tools/ci/package_linux.py --build-dir build-probe --no-archive

The build directory must be a Release configuration with `CNAHOUSE_DEBUG_TOOLS=OFF`, and the
package proves that the debug tools compiled out: the binary reports them absent and links no
overlay or debug-draw code. The staged tree is

    cna-house-<version>-linux-x86_64/
        cna-house            the game, RUNPATH $ORIGIN/lib
        cna-house.sh         the launch script
        lib/                 CNA and SDL, the libraries the build made rather than the system has
        content/ content-fx/ the shipped content (`stage_content.py`)
        LICENSE NOTICE.md licenses/ README.txt

It is written to `<build-dir>/package/`, beside the build it came from. Content is hard-linked
where the filesystem allows, so staging 400 MB of it writes almost nothing; the archive is the one
real copy. Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "ci"))

import stage_content  # noqa: E402

# Linked into a release binary, any of these means a debug tool did not compile out.
DEBUG_SYMBOL = re.compile(
    r"cnahouse::debug::(DebugDraw|Overlay|WorldOverlay|VisibilityOverlay|"
    r"VisibilityGeometryOverlay|PhysicsOverlay)::(Draw|Begin|Flush|DebugDraw)\b")

# Where a library the system provides lives; anything else resolved by `ldd` was built here.
SYSTEM_PREFIXES = ("/lib/", "/lib64/", "/usr/lib/", "/usr/lib64/", "/usr/local/lib/")

README = """CNA House {version} for Linux x86_64

Run ./cna-house.sh (or ./cna-house) from anywhere; the game finds its content beside itself.

  --quality=android|web|high|ultra   quality preset (default: auto-detected)
  --no-audio                         start without audio
  --help                             every option

Settings are kept in ${{XDG_DATA_HOME:-~/.local/share}}/game/CnaHouse/.

Requires an OpenGL ES 3 capable driver and the FFmpeg runtime libraries
(libavcodec, libavformat, libavutil, libswresample) of your distribution.
The programme is Ms-PL (LICENSE); runtime notices are in NOTICE.md and every
content asset's author and licence is in licenses/THIRD-PARTY-ASSETS.md.
"""

LAUNCHER = """#!/bin/sh
# CNA House launcher: the game resolves its content beside the executable, so the only job here is
# to run the copy this script sits next to, wherever it was started from.
here=$(dirname "$(readlink -f "$0")")
exec "$here/cna-house" "$@"
"""


def fail(message: str) -> None:
    print(f"package_linux: {message}", file=sys.stderr)
    sys.exit(1)


def cache_value(build: Path, key: str) -> str:
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if line.startswith(f"{key}:"):
            return line.split("=", 1)[1]
    return ""


def check_release(build: Path) -> None:
    if cache_value(build, "CMAKE_BUILD_TYPE") != "Release":
        fail(f"{build} is not a Release build")
    if cache_value(build, "CNAHOUSE_DEBUG_TOOLS").upper() not in {"OFF", "0", "FALSE", "NO"}:
        fail(f"{build} has CNAHOUSE_DEBUG_TOOLS on; a release ships without them")


def check_debug_tools_absent(binary: Path) -> None:
    environment = {k: v for k, v in os.environ.items() if k not in {"DISPLAY", "WAYLAND_DISPLAY"}}
    environment.update(SDL_VIDEODRIVER="offscreen", SDL_AUDIODRIVER="dummy")
    info = subprocess.run([str(binary), "--renderer-info"], capture_output=True, text=True,
                          env=environment, check=False)
    if not re.search(r"debug tools compiled in:\s+no", info.stdout):
        fail(f"{binary} --renderer-info does not report the debug tools absent:\n{info.stdout}")
    symbols = subprocess.run(["nm", "-C", str(binary)], capture_output=True, text=True, check=True)
    linked = sorted({m.group(0) for m in DEBUG_SYMBOL.finditer(symbols.stdout)})
    if linked:
        fail("debug-tool code is linked into the release binary: " + ", ".join(linked))


def built_libraries(binary: Path) -> list[Path]:
    output = subprocess.run(["ldd", str(binary)], capture_output=True, text=True, check=True).stdout
    found: list[Path] = []
    for line in output.splitlines():
        match = re.match(r"\s*(\S+) => (\S+)", line)
        if not match:
            continue
        if match.group(2) == "not":
            fail(f"{binary} cannot resolve {match.group(1)}")
        path = Path(match.group(2))
        if not str(path).startswith(SYSTEM_PREFIXES):
            found.append(path)
    return found


def set_runpath(path: Path, runpath: str) -> None:
    # CMake edits ELF dynamic sections itself, so no patchelf is needed. The new value must fit in
    # the old one's space, which a build tree's absolute path always leaves. A library with no
    # search path of its own (SDL3_mixer) names nothing outside the package and is left alone:
    # its libSDL3 is the copy libcna.so already loaded.
    dynamic = subprocess.run(["readelf", "-d", str(path)], capture_output=True, text=True, check=True)
    if "(RUNPATH)" not in dynamic.stdout and "(RPATH)" not in dynamic.stdout:
        return
    script = path.parent / ".set-runpath.cmake"
    script.write_text(f'file(RPATH_SET FILE "{path}" NEW_RPATH "{runpath}")\n')
    try:
        subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True, check=True)
    finally:
        script.unlink()


def check_closure(stage: Path) -> None:
    binary = stage / "cna-house"
    for library in built_libraries(binary):
        if stage.resolve() not in library.resolve().parents:
            fail(f"the staged binary still resolves {library} outside the package")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--no-archive", action="store_true", help="stage only")
    arguments = parser.parse_args()

    build = arguments.build_dir.resolve()
    binary = build / "cna-house"
    if not binary.is_file():
        fail(f"{binary} does not exist; build the cna-house target first")
    check_release(build)
    check_debug_tools_absent(binary)

    version = (REPO / "VERSION").read_text().strip()
    name = f"cna-house-{version}-linux-x86_64"
    stage = build / "package" / name
    if stage.exists():
        shutil.rmtree(stage)
    (stage / "lib").mkdir(parents=True)

    shutil.copy2(binary, stage / "cna-house")
    set_runpath(stage / "cna-house", "$ORIGIN/lib")
    for library in built_libraries(binary):
        copied = stage / "lib" / library.name
        shutil.copy2(library.resolve(), copied)
        set_runpath(copied, "$ORIGIN")

    # Every LOD level: a desktop player can pick any preset. What the manifest does not ship and
    # the content build's bookkeeping stay out (`HOUSE-02850`).
    files = stage_content.stage(build / "content", build / "content-fx", stage, None)

    for document in ("LICENSE", "NOTICE.md"):
        shutil.copy2(REPO / document, stage / document)
    shutil.copytree(REPO / "licenses", stage / "licenses")
    (stage / "README.txt").write_text(README.format(version=version))
    launcher = stage / "cna-house.sh"
    launcher.write_text(LAUNCHER)
    launcher.chmod(0o755)

    check_closure(stage)
    print(f"package_linux: staged {stage} ({files} content files, "
          f"{len(list((stage / 'lib').iterdir()))} bundled libraries)")

    if not arguments.no_archive:
        archive = stage.parent / f"{name}.tar.gz"
        with tarfile.open(archive, "w:gz") as tar:
            tar.add(stage, arcname=name)
        print(f"package_linux: wrote {archive} ({archive.stat().st_size / 2**20:.1f} MiB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
