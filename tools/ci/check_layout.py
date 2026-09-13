#!/usr/bin/env python3
"""check_layout.py -- the directory-set and file-placement gate for cna-house.

Asserts that the repository still has the shape `cna-house.md` section 17.5 specifies, and that
files are where that shape says they belong. It is deliberately strict about *structure* and says
nothing about content -- `check_xna_only.py` owns content.

Usage:
    check_layout.py [--root DIR] [--format text|json]
    check_layout.py --selftest

Exit status: 0 clean, 1 problems found, 2 usage error, 3 self-test failure.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import re
import shutil
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

# --------------------------------------------------------------------------------------------
# The shape (cna-house.md section 17.5)
# --------------------------------------------------------------------------------------------

SUBSYSTEMS = [
    "app", "world", "visibility", "rendering", "content", "physics", "player", "animation",
    "animals", "interaction", "environment", "weather", "lighting", "audio", "persistence",
    "ui", "debug", "util",
]

# `src/` and `include/cnahouse/` mirror each other, one directory per subsystem.
REQUIRED_DIRS = (
    ["docs", "docs/decisions", "docs/asset-review", "licenses", "content"]
    + [f"src/{s}" for s in SUBSYSTEMS]
    + ["src/interaction/behaviours"]
    + [f"include/cnahouse/{s}" for s in SUBSYSTEMS]
    + ["include/cnahouse/interaction/behaviours"]
    + ["tools/world", "tools/assets", "tools/blender", "tools/effects", "tools/ci"]
    + ["assets-src/Models", "assets-src/Textures", "assets-src/Audio", "assets-src/Fonts",
       "assets-src/Media", "assets-src/Effects", "assets-src/world"]
    + ["tests/unit", "tests/integration", "tests/render", "tests/perf"]
)

REQUIRED_FILES = [
    "README.md", "LICENSE", "NOTICE.md", "AGENTS.md", "CLAUDE.md", "VERSION",
    ".gitignore", ".clang-format", ".editorconfig",
    "cna-house.md", "plan.md",
    "docs/xna-deviations.md", "docs/conventions.md", "docs/world-format.md",
    "docs/content-authoring.md", "docs/performance-log.md", "docs/workflow.md",
    "docs/versioning.md", "docs/screenshot-scenes.md",
    "docs/decisions/README.md",
    "docs/asset-review/TEMPLATE.md",
    "licenses/README.md", "licenses/THIRD-PARTY-ASSETS.md",
    "tools/ci/check_xna_only.py", "tools/ci/check_layout.py", "tools/ci/run_checks.sh",
]

# Authored by a later phase. Reported as a reminder, never as a failure, until the task that
# creates them lands -- at which point they move into REQUIRED_FILES.
#   CMakeLists.txt    -- HOUSE-00121 (phase 2)
#   CMakePresets.json -- HOUSE-00035
PENDING_FILES = {
    "CMakeLists.txt": "HOUSE-00121",
    "CMakePresets.json": "HOUSE-00035",
}

# The closed list of build-directory names (AGENTS.md rule 2). Any other directory that looks
# like a build tree is a rule violation, not a stylistic matter.
ALLOWED_BUILD_DIRS = {
    "build", "build-asan", "build-ubsan", "build-tsan", "build-probe", "build-consumer",
}
BUILD_LIKE_RE = re.compile(r"^(build|cmake-build)")

ALLOWED_ROOT_ENTRIES = {
    # files
    "README.md", "LICENSE", "NOTICE.md", "AGENTS.md", "CLAUDE.md", "VERSION",
    "cna-house.md", "plan.md", "CMakeLists.txt", "CMakePresets.json", "CMakeUserPresets.json",
    ".gitignore", ".gitattributes", ".clang-format", ".editorconfig",
    # directories
    "docs", "include", "src", "tools", "tests", "assets-src", "content", "licenses",
    # CMake modules. `cmake/TierSelection.cmake` is named by HOUSE-00122 as the one place the
    # Tier-E decision is made, and a `cmake/` directory is the standard home for it; §17.5 simply
    # predated there being a build.
    "cmake",
    ".git", ".github", ".githooks",
}

CXX_SUFFIXES = {".cpp", ".cc", ".cxx", ".hpp", ".h", ".hh", ".hxx", ".inl", ".ipp"}
HEADER_SUFFIXES = {".hpp", ".h", ".hh", ".hxx", ".inl", ".ipp"}
IMPL_SUFFIXES = {".cpp", ".cc", ".cxx"}

ADR_RE = re.compile(r"^ADR-\d{4}-[a-z0-9]+(?:-[a-z0-9]+)*\.md$")

SKIP_DIRS_ANYWHERE = {".git", "__pycache__", ".idea", ".vscode", ".vs", ".cache", "node_modules"}

# Codex creates these empty control directories at repository root. Inside its filesystem sandbox
# they are read-only mounts; the commit hook runs outside that namespace and sees ordinary empty
# directories. They are not repository content and git cannot record an empty directory.
INJECTED_EMPTY_ROOT_DIRS = {".agents", ".codex"}


@dataclass(frozen=True)
class Problem:
    kind: str
    path: str
    detail: str

    def render(self) -> str:
        return f"[{self.kind}] {self.path}: {self.detail}"


# --------------------------------------------------------------------------------------------
# Checks
# --------------------------------------------------------------------------------------------

def check_required(root: Path, out: list[Problem]) -> None:
    for rel in REQUIRED_DIRS:
        if not (root / rel).is_dir():
            out.append(Problem("missing-directory", rel,
                               "required by cna-house.md section 17.5"))
    for rel in REQUIRED_FILES:
        if not (root / rel).is_file():
            out.append(Problem("missing-file", rel, "required by the repository layout"))


def check_root_entries(root: Path, out: list[Problem]) -> None:
    for entry in sorted(os.listdir(root)):
        path = root / entry
        if _is_injected_empty_root_dir(root, path):
            continue
        if entry in ALLOWED_ROOT_ENTRIES:
            continue
        if entry in ALLOWED_BUILD_DIRS:
            continue
        if BUILD_LIKE_RE.match(entry):
            out.append(Problem("build-directory", entry,
                               "not in the closed list of build-directory names "
                               f"({', '.join(sorted(ALLOWED_BUILD_DIRS))}) -- AGENTS.md rule 2"))
            continue
        out.append(Problem("stray-root-entry", entry,
                           "the repository root holds only the entries of "
                           "cna-house.md section 17.5"))


def check_mirror(root: Path, out: list[Problem]) -> None:
    """src/ and include/cnahouse/ must carry the same subsystem directories, and no others."""
    for parent, label in ((root / "src", "src"),
                          (root / "include" / "cnahouse", "include/cnahouse")):
        if not parent.is_dir():
            continue
        for child in sorted(parent.iterdir()):
            if not child.is_dir() or child.name in SKIP_DIRS_ANYWHERE:
                continue
            if child.name not in SUBSYSTEMS:
                out.append(Problem("unknown-subsystem", f"{label}/{child.name}",
                                   "not one of the 18 subsystems of cna-house.md section 17.5"))


def check_placement(root: Path, out: list[Problem]) -> None:
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS_ANYWHERE)
        here = Path(dirpath)
        if here == root:
            dirnames[:] = [d for d in dirnames
                           if not _is_injected_empty_root_dir(root, root / d)]
            dirnames[:] = [d for d in dirnames
                           if d not in ALLOWED_BUILD_DIRS and not BUILD_LIKE_RE.match(d)]
            dirnames[:] = [d for d in dirnames if d != "content"]
        for name in sorted(filenames):
            rel = (here / name).relative_to(root).as_posix()
            top = rel.split("/", 1)[0]
            suffix = Path(name).suffix

            if suffix in CXX_SUFFIXES and top not in {"src", "include", "tests"}:
                out.append(Problem("misplaced-source", rel,
                                   "C++ sources live in src/, include/cnahouse/ or tests/"))
            if suffix in IMPL_SUFFIXES and top == "include":
                out.append(Problem("misplaced-source", rel,
                                   "include/cnahouse/ holds headers; implementations live in src/"))
            if top == "include" and not rel.startswith("include/cnahouse/"):
                out.append(Problem("misplaced-header", rel,
                                   "public headers live under include/cnahouse/"))
            if suffix in HEADER_SUFFIXES and top == "src":
                # Private headers beside their implementation are fine; public ones are not.
                pass
            if rel.startswith("docs/decisions/") and name != "README.md":
                if not ADR_RE.match(name):
                    out.append(Problem("bad-adr-name", rel,
                                       "decision records are named ADR-NNNN-lower-slug.md"))
            if suffix == ".py" and top not in {"tools", "tests"}:
                out.append(Problem("misplaced-tool", rel,
                                   "Python tooling lives under tools/"))


def check_gitkeep(root: Path, out: list[Problem]) -> None:
    """A .gitkeep is how an empty directory is tracked; it must not survive the first real file.

    "Empty" means **empty as far as git is concerned**, not empty on disk. `content/` is generated
    and git-ignored, and its `.gitkeep` is what keeps the directory in the repository at all -- so
    the moment a content build runs, an on-disk test calls that `.gitkeep` stale and the layout
    gate fails. Which it did: `HOUSE-00216` made `make content` actually populate `content/`, and
    the first stage of the *next* build was this check, failing on the output of the previous one.

    An ignored file therefore does not make a `.gitkeep` stale. The one that does is a file git
    would track, because that is exactly when the directory no longer needs a placeholder.
    """
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS_ANYWHERE)
        here = Path(dirpath)
        if here == root:
            dirnames[:] = [d for d in dirnames
                           if not _is_injected_empty_root_dir(root, root / d)]
            dirnames[:] = [d for d in dirnames
                           if d not in ALLOWED_BUILD_DIRS and not BUILD_LIKE_RE.match(d)]
        if ".gitkeep" not in filenames:
            continue
        siblings = [here / f for f in filenames if f != ".gitkeep"]
        siblings += [here / d for d in dirnames]
        visible = [p for p in siblings if not _git_ignores(root, p)]
        if visible:
            rel = (here / ".gitkeep").relative_to(root).as_posix()
            names = ", ".join(sorted(p.name for p in visible)[:3])
            out.append(Problem("stale-gitkeep", rel,
                               f"the directory now holds tracked content ({names}); delete the "
                               f".gitkeep"))


def _git_ignores(root: Path, path: Path) -> bool:
    """Would git ignore this path? Answered by git, because `.gitignore` is git's to interpret.

    A repository without git, or a path outside one, answers "no": the check then behaves exactly
    as it did before, which is the safe direction -- it can report a problem that is not one, and
    never miss one that is.
    """
    try:
        result = subprocess.run(
            ["git", "-C", str(root), "check-ignore", "--quiet", str(path)],
            capture_output=True, check=False)
    except (OSError, ValueError):
        return False
    return result.returncode == 0


def _git_tracks(root: Path, path: Path) -> bool:
    """Does the repository index contain this path, even if a mount hides it on disk?"""
    try:
        rel = path.relative_to(root).as_posix()
        result = subprocess.run(
            ["git", "-C", str(root), "ls-files", "--error-unmatch", "--", rel],
            capture_output=True, check=False)
    except (OSError, ValueError):
        return False
    return result.returncode == 0


def _injected_root_policy(*, name: str, directory: bool, empty: bool, tracked: bool) -> bool:
    """Recognise only the two empty, untracked control directories Codex injects."""
    return name in INJECTED_EMPTY_ROOT_DIRS and directory and empty and not tracked


def _is_injected_empty_root_dir(root: Path, path: Path) -> bool:
    """Exclude environment control points without admitting repository content or debris."""
    try:
        directory = path.is_dir()
        empty = directory and next(path.iterdir(), None) is None
    except OSError:
        return False
    return _injected_root_policy(
        name=path.name, directory=directory, empty=empty, tracked=_git_tracks(root, path))


def scan(root: Path) -> tuple[list[Problem], list[str]]:
    root = root.resolve()
    problems: list[Problem] = []
    check_required(root, problems)
    check_root_entries(root, problems)
    check_mirror(root, problems)
    check_placement(root, problems)
    check_gitkeep(root, problems)
    problems.sort(key=lambda p: (p.kind, p.path))

    notes = [f"{rel} is still to be authored by {task}"
             for rel, task in sorted(PENDING_FILES.items())
             if not (root / rel).is_file()]
    return problems, notes


# --------------------------------------------------------------------------------------------
# Self-test
# --------------------------------------------------------------------------------------------

def _build_clean_tree(root: Path) -> None:
    for rel in REQUIRED_DIRS:
        (root / rel).mkdir(parents=True, exist_ok=True)
    for rel in REQUIRED_FILES:
        path = root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("placeholder\n", encoding="utf-8")
    for dirpath, dirnames, filenames in os.walk(root):
        if not dirnames and not filenames:
            (Path(dirpath) / ".gitkeep").touch()


PLANTED = {
    "missing-directory": lambda r: _rmtree(r / "src" / "weather"),
    "missing-file": lambda r: (r / "docs" / "conventions.md").unlink(),
    "stray-root-entry": lambda r: (r / "scratch.txt").write_text("x\n"),
    "build-directory": lambda r: (r / "build-probe-1794").mkdir(),
    "unknown-subsystem": lambda r: (r / "src" / "particles").mkdir(),
    "misplaced-source": lambda r: (r / "tools" / "ci" / "Helper.cpp").write_text("int f();\n"),
    "misplaced-header": lambda r: _mk(r / "include" / "stray" / "Thing.hpp", "#pragma once\n"),
    "bad-adr-name": lambda r: (r / "docs" / "decisions" / "adr-13-thing.md").write_text("x\n"),
    "misplaced-tool": lambda r: (r / "src" / "util" / "gen.py").write_text("print(1)\n"),
    "stale-gitkeep": lambda r: _stale_gitkeep(r),
}


def _rmtree(path: Path) -> None:
    shutil.rmtree(path)


def _mk(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def _stale_gitkeep(root: Path) -> None:
    target = root / "src" / "audio"
    (target / ".gitkeep").touch()
    (target / "AudioSystem.cpp").write_text("int f();\n", encoding="utf-8")


def selftest() -> int:
    failures: list[str] = []

    injected_cases = {
        "empty untracked .agents": (".agents", True, True, False, True),
        "empty untracked .codex": (".codex", True, True, False, True),
        "ordinary empty directory": ("scratch", True, True, False, False),
        "non-empty .agents": (".agents", True, False, False, False),
        "tracked .codex": (".codex", True, True, True, False),
    }
    for label, (name, directory, empty, tracked, expected) in injected_cases.items():
        actual = _injected_root_policy(
            name=name, directory=directory, empty=empty, tracked=tracked)
        if actual != expected:
            failures.append(
                f"injected-directory policy classified {label!r} as {actual}, expected {expected}")

    with tempfile.TemporaryDirectory(prefix="cnahouse-layout-") as tmp:
        root = Path(tmp)
        _build_clean_tree(root)
        problems, _ = scan(root)
        if problems:
            failures.append("the clean tree was rejected:\n  " +
                            "\n  ".join(p.render() for p in problems))

    for kind, plant in PLANTED.items():
        with tempfile.TemporaryDirectory(prefix="cnahouse-layout-") as tmp:
            root = Path(tmp)
            _build_clean_tree(root)
            plant(root)
            problems, _ = scan(root)
            if not any(p.kind == kind for p in problems):
                failures.append(f"planted [{kind}] was NOT detected; reported "
                                f"{sorted({p.kind for p in problems}) or 'nothing'}")

    # A `.gitkeep` beside a GIT-IGNORED file is not stale, and the planted faults above cannot
    # show that: the temporary tree is not a git repository, so `_git_ignores` answers "no" for
    # everything and the check behaves as it did before this rule existed. `content/` is the real
    # case -- generated, ignored, and kept in the repository only by that `.gitkeep` -- and
    # `HOUSE-00216` made a content build populate it, so the previous rule failed the layout gate
    # on the output of the previous build.
    with tempfile.TemporaryDirectory(prefix="cnahouse-layout-git-") as tmp:
        root = Path(tmp)
        _build_clean_tree(root)
        created = subprocess.run(["git", "-C", str(root), "init", "-q"],
                                 capture_output=True, check=False).returncode == 0
        if not created:
            failures.append("could not create a git repository for the gitkeep/ignore claim")
        else:
            (root / ".gitignore").write_text("/generated/\n", encoding="utf-8")
            generated = root / "generated"
            generated.mkdir()
            (generated / ".gitkeep").touch()
            (generated / "output.bin").write_bytes(b"built\n")
            if not _git_ignores(root, generated / "output.bin"):
                failures.append("git did not report the ignored file as ignored")
            problems, _ = scan(root)
            if any(p.kind == "stale-gitkeep" for p in problems):
                failures.append(
                    "a .gitkeep beside a git-IGNORED file was called stale; that is the rule "
                    "that failed the layout gate on content/ after HOUSE-00216 built into it")
            (generated / "tracked.txt").write_text("x\n", encoding="utf-8")
            (root / ".gitignore").write_text("/generated/output.bin\n", encoding="utf-8")
            problems, _ = scan(root)
            if not any(p.kind == "stale-gitkeep" for p in problems):
                failures.append(
                    "a .gitkeep beside a TRACKED file was not called stale; the rule still has "
                    "to fire when the directory really has content git keeps")

    if failures:
        print("check_layout self-test FAILED", file=sys.stderr)
        for failure in failures:
            print("  - " + failure, file=sys.stderr)
        return 3

    print(f"check_layout self-test passed: {len(PLANTED)} planted layout faults detected, "
          f"{len(injected_cases)} injected-directory cases classified, "
          f"a .gitkeep beside ignored output accepted and beside tracked content rejected, "
          f"clean tree accepted.")
    return 0


# --------------------------------------------------------------------------------------------

def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=None)
    parser.add_argument("--format", choices=("text", "json"), default="text")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)

    if args.selftest:
        return selftest()

    root = Path(args.root) if args.root else Path(__file__).resolve().parents[2]
    if not root.is_dir():
        print(f"check_layout: not a directory: {root}", file=sys.stderr)
        return 2

    problems, notes = scan(root)

    if args.format == "json":
        print(json.dumps({"problems": [p.__dict__ for p in problems], "notes": notes}, indent=2))
    else:
        for problem in problems:
            print(problem.render())
        for note in notes:
            print(f"[note] {note}")
        if problems:
            print(f"\ncheck_layout: {len(problems)} problem(s) in {root}", file=sys.stderr)
        else:
            print(f"check_layout: clean ({root})")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
