#!/usr/bin/env python3
"""blender_env.py -- run a Blender tool headlessly, with glTF actually working.

`HOUSE-00189`. Every `tools/blender/*.py` script is written to run two ways: as a plain script,
which re-executes itself inside Blender, and inside Blender, which does the work. This module is the
first half, factored out because seven more Blender tools follow it (`HOUSE-00190`, `00204`–`00209`)
and none of them should have to rediscover the two facts below.

**Debian's Blender uses the SYSTEM Python, and glTF needs numpy.** `blender 4.3.2+dfsg-2` embeds
`/usr/bin/python3.13` rather than shipping its own interpreter, and `io_scene_gltf2` imports `numpy`
the moment an import or export actually runs — not when the operator is looked up. So
`hasattr(bpy.ops.export_scene, "gltf")` is `True` on a machine where every glTF operation fails, and
the failure arrives as a traceback out of the addon rather than as a missing-dependency message.
`python3-numpy` is not installed for 3.13 here, and the user's pyenv numpy is built for 3.11.

**The fix is a shared numpy, not a system change.** `AGENTS.md` rule 4 puts shared dependencies in
`~/deps/<name>`, one copy for every session; this uses `~/deps/blender-python`. Nothing outside that
directory is touched, no `apt` is involved, and deleting the directory is a complete undo. Create it
with:

    /usr/bin/python3.13 -m pip install --target ~/deps/blender-python numpy

**Blender ignores `PYTHONPATH` unless you tell it not to.** `--python-use-system-env` is required;
without it the environment variable is silently dropped and the numpy above is invisible. That flag
plus `--factory-startup` (so a user's saved preferences cannot change a content build) is the whole
invocation.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

#: Where a numpy built for Blender's interpreter is kept. Overridable for a machine that puts it
#: elsewhere; the default is the location `AGENTS.md` rule 4 prescribes.
DEFAULT_PYTHON_DEPS = Path(os.environ.get("CNAHOUSE_BLENDER_PYTHONPATH", "")) or (
    Path.home() / "deps" / "blender-python"
)

#: Lines a tool prints that are its report rather than Blender's noise. Blender writes a good deal
#: to stdout even in background mode, and a caller wants the tool's answer, not the splash.
REPORT_PREFIXES = ("  ", "PASS", "FAIL")


def find_blender() -> str | None:
    return os.environ.get("CNAHOUSE_BLENDER") or shutil.which("blender")


def build_command(script: Path, args: list[str]) -> list[str] | None:
    blender = find_blender()
    if blender is None:
        return None
    return [
        blender,
        "--background",
        # A content build must not depend on anyone's saved preferences or enabled add-ons.
        "--factory-startup",
        # Without this, PYTHONPATH is dropped and the shared numpy is invisible. See the docstring.
        "--python-use-system-env",
        "--python",
        str(script),
        "--",
        *args,
    ]


def environment() -> dict:
    """The child's environment: the shared numpy on `PYTHONPATH`, and **no display**.

    **`--background` is not enough to keep Blender off the screen.** Cycles on the CPU never
    touches a window, but EEVEE needs a GL context, and on a desktop session Blender takes one
    from the running X/Wayland server -- which puts a window on the user's actual screen, in the
    middle of whatever they were doing, every time a content tool or a CI selftest runs.
    `impostor_render.py` does exactly that, and a batch of selftests does it repeatedly.

    So `DISPLAY` and `WAYLAND_DISPLAY` are removed from the child's environment. Blender 4.3 falls
    back to headless GL and EEVEE renders normally -- measured, not assumed -- so nothing is lost
    and there is no window for anything to appear in.

    **`xvfb-run` was the first fix and was worse.** It works, but its server outlives the command
    on this machine: a run leaves an `Xvfb` process and a `/tmp/xvfb-run.XXXX` directory behind,
    which is precisely the leak `AGENTS.md` rule 3 names. Denying Blender a display needs no
    second process, leaks nothing, and cannot be defeated by a wrapper that fails to reap.

    **A VIRTUAL display is the third option, and the best one when there is a server to point at.**
    `CNAHOUSE_BLENDER_DISPLAY=:99` sends Blender to that display instead of taking its display
    away: it gets a real GL context, nothing appears on the user's screen, and the objection to
    `xvfb-run` does not apply because the session starts ONE `Xvfb` and every run shares it --
    nothing is spawned, and so nothing is leaked, per invocation. Start one with:

        Xvfb :99 -screen 0 1920x1080x24 -nolisten tcp &

    The variable is honoured only if something is actually answering on that display, so a stale
    export cannot silently send a batch of renders into a socket that is not there.

    `CNAHOUSE_BLENDER_KEEP_DISPLAY=1` opts out of all of it and keeps the session's own display,
    for somebody who genuinely wants to watch Blender work.
    """
    env = dict(os.environ)
    deps = DEFAULT_PYTHON_DEPS
    if deps.is_dir():
        existing = env.get("PYTHONPATH", "")
        env["PYTHONPATH"] = f"{deps}{os.pathsep}{existing}" if existing else str(deps)
    if not env.get("CNAHOUSE_BLENDER_KEEP_DISPLAY"):
        virtual = env.get("CNAHOUSE_BLENDER_DISPLAY", "").strip()
        if virtual and _display_answers(virtual):
            env["DISPLAY"] = virtual
        else:
            env.pop("DISPLAY", None)
        env.pop("WAYLAND_DISPLAY", None)
    return env


def _display_answers(display: str) -> bool:
    """Is an X server actually listening on @p display?

    Checked rather than trusted: an exported `CNAHOUSE_BLENDER_DISPLAY` that names a server which
    has since died would send every render at a socket that is not there, and Blender's failure
    then looks like a GL problem rather than a missing server. The socket is enough to ask -- it
    needs no `xdpyinfo` on the machine and costs nothing.
    """
    name = display.split(".", 1)[0]
    if not name.startswith(":"):
        return False
    number = name[1:]
    if not number.isdigit():
        return False
    return Path(f"/tmp/.X11-unix/X{number}").exists()


def relaunch(script: Path, args: list[str], *, tool: str) -> int:
    """Re-executes @p script inside Blender and relays its report. Returns an exit status."""
    command = build_command(script, args)
    if command is None:
        print(
            f"{tool}: blender is not on PATH. Install it, or set CNAHOUSE_BLENDER to its path.",
            file=sys.stderr,
        )
        return 2

    result = subprocess.run(command, capture_output=True, text=True, env=environment())

    # **Blender's exit code is not the tool's.** Measured on 4.3.2: a script that calls `sys.exit(1)`
    # or raises an uncaught exception still leaves `blender` returning 0. Every Blender gate in this
    # project would silently pass. So each tool prints `"<tool>: EXIT <n>"` as its last line and that
    # is the authority; a missing sentinel means the script died before reaching it, which is also a
    # failure.
    status = None
    sentinel = f"{tool}: EXIT "
    for line in result.stdout.splitlines():
        if line.startswith(sentinel):
            status = int(line[len(sentinel):].strip())
            continue
        if line.startswith(f"{tool}:") or line.startswith(REPORT_PREFIXES) or "_JSON " in line:
            print(line)
    if status is None:
        print(
            f"{tool}: the tool did not report an exit status, so it died before finishing.",
            file=sys.stderr,
        )
        status = 1
    result = subprocess.CompletedProcess(
        result.args, status, result.stdout, result.stderr
    )

    if result.returncode != 0:
        if "No module named 'numpy'" in result.stdout + result.stderr:
            # The one failure worth translating, because its traceback names io_scene_gltf2 and
            # sends the reader to Blender's addon rather than to the missing dependency.
            print(
                f"{tool}: Blender's Python ({sys.platform}) has no numpy, so glTF import/export\n"
                f"  cannot run. Create the shared copy once — it is not a system change:\n"
                f"      /usr/bin/python3.13 -m pip install --target {DEFAULT_PYTHON_DEPS} numpy\n"
                f"  See tools/blender/blender_env.py for why.",
                file=sys.stderr,
            )
        else:
            sys.stderr.write(result.stdout[-4000:])
            sys.stderr.write(result.stderr[-4000:])
    return result.returncode


def argv_after_ddash() -> list[str]:
    """The arguments Blender passed through, i.e. everything after `--`."""
    return sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
