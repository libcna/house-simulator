#!/usr/bin/env python3
"""check_xna_strict.py -- catch CNAEXT calls that reach the compiler but not the source text.

`HOUSE-00168`. `check_xna_only.py` reads source text, and that is genuinely all it can do: it looks
for forbidden identifiers. **Overload resolution is invisible to it.** These three lines contain no
forbidden identifier and every one of them selects a `CNAEXT` member:

    instance.setIsLoopedProperty(true);                  // prvalue -> bool&& -> the CNAEXT overload
    Color(uint8_t{0}, uint8_t{0}, uint8_t{0}, alpha);    // XNA's Color takes int or float, not byte
    engine.setMasterVolumeProperty(0.5f);                // prvalue -> float&&

ADR-0001 forbids all three. No amount of work on a text-matching lint would find them, because
which overload a call selects is a question only the compiler can answer.

**So this gate asks the compiler.** CNA tags every non-XNA declaration with the `CNAEXT` macro,
which expands to nothing in a normal build and to `[[deprecated]]` when `CNA_STRICT_XNA_API` is
defined -- a mechanism CNA provides and this project was not using. Recompiling each translation
unit with that macro and `-Wdeprecated-declarations` makes the compiler name every call that
actually resolves to a CNAEXT declaration, with the file, the line, and the exact signature chosen.

    tools/ci/check_xna_strict.py                # check the runtime sources
    tools/ci/check_xna_strict.py --all          # include the test sources
    tools/ci/check_xna_strict.py --list         # print every hit, exempt ones included

It needs `build/compile_commands.json`, which `CMAKE_EXPORT_COMPILE_COMMANDS` already produces, and
a compiler. That makes it a *build-time* gate rather than one of the fast static checks --
`run_checks.sh` runs it only when the compile database is present, and skips with a message when it
is not, so a fresh clone is not blocked by a check it cannot yet run.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import re
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
COMPILE_DB = REPO / "build" / "compile_commands.json"

#: Mirrors `check_xna_only.py`. Runtime code is what ADR-0001 governs; test code is checked only
#: when asked, because the phase-1 probes exist precisely to exercise CNA and are deleted at the end
#: of phase 1.
RUNTIME_ROOTS = ("src", "include")
TEST_ROOTS = ("tests",)

#: A `CNAEXT` hit that is NOT a violation, with the reason. Kept deliberately tiny: every entry is
#: an argument, not a convenience, and a rule with a long exemption list is not a rule.
#:
#: **Destructors.** CNA tags `~Texture2D()`, `~SpriteBatch()`, `~RenderTarget2D()` and friends as
#: CNAEXT, which is accurate documentation -- XNA is C# and has no destructors -- but it is not a
#: prohibition anyone can obey. Every C++ object with automatic storage runs its destructor, so a
#: gate that failed on them would fail on `SpriteBatch batch(device);` and would have to be turned
#: off. They are excluded by shape, not by name, so a newly tagged destructor is exempt too.
DESTRUCTOR = re.compile(r"::~[A-Za-z_][A-Za-z0-9_]*\(\)")

HIT = re.compile(
    r"^(?P<file>[^:\n]+):(?P<line>\d+):(?P<col>\d+):\s+"
    r"(?:warning|error):\s+[‘'](?P<what>.+?)[’']\s+is deprecated:\s+CNAEXT",
    re.MULTILINE,
)


class Hit:
    __slots__ = ("file", "line", "col", "what")

    def __init__(self, file: str, line: int, col: int, what: str) -> None:
        self.file = file
        self.line = line
        self.col = col
        self.what = what

    @property
    def exempt(self) -> bool:
        return bool(DESTRUCTOR.search(self.what))

    def __str__(self) -> str:
        return f"{self.file}:{self.line}:{self.col}: {self.what}"


def load_entries(roots: tuple[str, ...]) -> list[dict]:
    if not COMPILE_DB.is_file():
        return []
    with COMPILE_DB.open(encoding="utf-8") as handle:
        database = json.load(handle)

    wanted: dict[str, dict] = {}
    for entry in database:
        source = Path(entry["file"])
        try:
            relative = source.resolve().relative_to(REPO)
        except ValueError:
            continue  # a CNA or sharp-runtime translation unit; not ours to police
        if relative.parts[0] not in roots:
            continue
        # One entry per file. The database holds a row per target, and the same source compiled
        # into two targets asks the compiler the same question twice.
        wanted.setdefault(str(relative), entry)
    return list(wanted.values())


def check_one(entry: dict) -> tuple[str, list[Hit], str]:
    """Recompiles one translation unit under `CNA_STRICT_XNA_API`."""
    command = shlex.split(entry["command"]) if "command" in entry else list(entry["arguments"])

    # Rebuild the command line: syntax-only, strict macro on, deprecation as a WARNING so every hit
    # in the file is reported rather than only the first, and the project's own -Werror removed so
    # an unrelated warning cannot masquerade as a violation.
    rebuilt: list[str] = []
    skip_next = False
    for argument in command:
        if skip_next:
            skip_next = False
            continue
        if argument in ("-o", "-MT", "-MF"):
            skip_next = True
            continue
        if argument in ("-c", "-Werror") or argument.startswith("-Werror=") or argument == "-MD":
            continue
        rebuilt.append(argument)
    rebuilt += [
        "-fsyntax-only",
        "-DCNA_STRICT_XNA_API",
        "-Wdeprecated-declarations",
        "-fdiagnostics-plain-output",
    ]

    result = subprocess.run(
        rebuilt, cwd=entry.get("directory", str(REPO)), capture_output=True, text=True
    )
    hits: list[Hit] = []
    for match in HIT.finditer(result.stderr):
        path = match.group("file")
        try:
            path = str(Path(path).resolve().relative_to(REPO))
        except (ValueError, OSError):
            pass
        hits.append(Hit(path, int(match.group("line")), int(match.group("col")), match.group("what")))
    return entry["file"], hits, result.stderr


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--all", action="store_true", help="check the test sources too")
    parser.add_argument("--list", action="store_true", help="print exempt hits as well")
    parser.add_argument("--jobs", type=int, default=0, help="parallel compiles (default: cpu count)")
    args = parser.parse_args()

    roots = RUNTIME_ROOTS + TEST_ROOTS if args.all else RUNTIME_ROOTS
    entries = load_entries(roots)
    if not entries:
        if not COMPILE_DB.is_file():
            print(
                f"check_xna_strict: no {COMPILE_DB.relative_to(REPO)}; configure a build first "
                f"(CMAKE_EXPORT_COMPILE_COMMANDS is already on). SKIPPED, not failed."
            )
            return 0
        print("check_xna_strict: the compile database names no source under " + ", ".join(roots))
        return 0

    import os

    jobs = args.jobs or (os.cpu_count() or 4)
    violations: list[Hit] = []
    exempted = 0
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        for _source, hits, _stderr in pool.map(check_one, entries):
            for hit in hits:
                if hit.exempt:
                    exempted += 1
                    if args.list:
                        print(f"  exempt   {hit}")
                else:
                    violations.append(hit)

    # Deduplicated: a header included by twenty translation units reports its hit twenty times, and
    # twenty copies of one line is a report nobody reads to the end.
    unique = sorted({(h.file, h.line, h.col, h.what) for h in violations})

    if unique:
        print(
            f"check_xna_strict: {len(unique)} CNAEXT call(s) selected by overload resolution "
            f"(ADR-0001):",
            file=sys.stderr,
        )
        for file, line, col, what in unique:
            print(f"  {file}:{line}:{col}", file=sys.stderr)
            print(f"      selects {what}", file=sys.stderr)
        print(
            "\n  These name no forbidden identifier, so check_xna_only.py cannot see them.\n"
            "  Fix by choosing the XNA overload explicitly -- e.g. Color(int,int,int,int) rather\n"
            "  than the byte constructor, or an lvalue rather than a prvalue for a setter that has\n"
            "  a CNAEXT rvalue overload.",
            file=sys.stderr,
        )
        return 1

    print(
        f"check_xna_strict: {len(entries)} translation unit(s) clean "
        f"({exempted} destructor hit(s) exempt)."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
