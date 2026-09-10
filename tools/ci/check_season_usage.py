#!/usr/bin/env python3
"""check_season_usage.py -- nobody reads an integer season without a blend weight.

`HOUSE-01543`, acceptance criterion 4. §36.3 is unambiguous: *"Season is a continuous phase, never
an enum... Every seasonal quantity -- the transition matrix, the temperature curve, vegetation
colour, foliage density, snow-cover probability, the ambience bed -- is the `blend`-weighted mix of
the two neighbouring seasons' values, never a switch."*

`SeasonPhase::primary` exists because a caller has to start somewhere, and the moment somebody
writes `switch (phase.primary)` the design is gone -- silently, and in a way no test of the season
code itself can see, because the season code is still right. So the rule is enforced where it can
be: at the point of USE, over `src/` and `include/`.

    tools/ci/check_season_usage.py            # the gate
    tools/ci/check_season_usage.py --selftest

## The rule, and how coarse it is

**A file that reads `primary` or names a `Season::` constant must also mention `blend` or call
`MixBySeason`.** That is FILE granularity, not statement granularity, and it is stated here rather
than implied: a file that legitimately blends in one function and switches in another passes. A C++
parser would do better and is not worth its weight for a rule whose job is to make the wrong thing
conspicuous rather than impossible -- the same bargain `check_xna_only.py` makes with identifiers.

Exemptions are by path, with a reason, and there are two: the file that COMPUTES the phase, and the
debug overlay that NAMES it for a person to read. Both are producers rather than consumers of a
seasonal quantity.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ROOTS = ("src", "include")

#: What reading an integer season looks like.
READS = (
    re.compile(r"\b(?:phase|season)\s*(?:\.|->)\s*(?:primary|secondary)\b"),
    re.compile(r"\bSeason::(?:Spring|Summer|Autumn|Winter)\b"),
)

#: What doing it PROPERLY looks like anywhere in the same file.
BLENDS = (re.compile(r"\bblend\b"), re.compile(r"\bMixBySeason\b"))

#: Path -> why it may read a season without blending.
#:
#: **Empty, and that is a finding rather than an omission.** The two files that legitimately read a
#: season -- `Season.cpp`, which computes the phase, and `EnvironmentOverlay.cpp`, which names it
#: for a person -- both mention `blend` in their own right and pass the rule without help. They
#: were exempted in the first draft and the stale-exemption check below threw all four entries out
#: on its first run, which is the check earning its place before the gate had run once.
#:
#: The machinery stays because the day somebody needs an exemption they should have to write the
#: reason down beside it, and because an exemption that stops being needed has to fail.
EXEMPT: dict[str, str] = {}


def offenders(root: Path) -> list[str]:
    """Every file that reads a season without a blend anywhere in it."""
    out = []
    for directory in ROOTS:
        for path in sorted((root / directory).rglob("*")):
            if path.suffix not in (".cpp", ".hpp"):
                continue
            relative = path.relative_to(root).as_posix()
            if relative in EXEMPT:
                continue
            text = path.read_text(encoding="utf-8")
            if not any(pattern.search(text) for pattern in READS):
                continue
            if any(pattern.search(text) for pattern in BLENDS):
                continue
            out.append(relative)
    return out


def stale_exemptions(root: Path) -> list[str]:
    """Exemptions that are not doing anything: gone, not reading a season, or passing anyway.

    The third case is the one worth having. An exemption whose file would pass the rule WITHOUT it
    is a licence nobody is using, and it will still be there on the day that file starts switching
    on `primary` -- at which point it silently permits exactly what the gate exists to stop. This
    is `build_chunks.py`'s third chunk-budget rule in another subsystem: an exception its file no
    longer needs fails as loudly as one it has exceeded.
    """
    out = []
    for relative in sorted(EXEMPT):
        path = root / relative
        if not path.is_file():
            out.append(f"{relative}: exempt and no longer exists")
            continue
        text = path.read_text(encoding="utf-8")
        if not any(pattern.search(text) for pattern in READS):
            out.append(f"{relative}: exempt and no longer reads a season")
        elif any(pattern.search(text) for pattern in BLENDS):
            out.append(f"{relative}: exempt and passes the rule anyway; delete the exemption")
    return out


def selftest() -> int:
    import tempfile

    failures = 0

    def require(condition: bool, message: str) -> None:
        nonlocal failures
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        failures += 0 if condition else 1

    with tempfile.TemporaryDirectory() as raw:
        fake = Path(raw)
        (fake / "src" / "weather").mkdir(parents=True)
        (fake / "include").mkdir(parents=True)

        good = fake / "src" / "weather" / "Matrix.cpp"
        good.write_text("float p = MixBySeason(phase, kPerSeason);\n", encoding="utf-8")
        require(offenders(fake) == [], f"a file that mixes is clean ({offenders(fake)})")

        bad = fake / "src" / "weather" / "Switcher.cpp"
        bad.write_text("switch (phase.primary) { case 0: break; }\n", encoding="utf-8")
        require(offenders(fake) == ["src/weather/Switcher.cpp"],
                f"a switch on `primary` with no blend is caught ({offenders(fake)})")

        # A named season with NO `primary` in sight, which is the only thing that isolates the
        # second pattern: the obvious fixture -- `season.primary == static_cast<int>(...)` --
        # matches the first one as well and would pass with the second deleted.
        bad.write_text("if (CurrentSeason() == Season::Winter) { snow(); }\n", encoding="utf-8")
        require("src/weather/Switcher.cpp" in offenders(fake),
                f"comparing against a named season is caught on its own ({offenders(fake)})")

        bad.write_text("const float value = perSeason[phase.primary] * (1.0F - phase.blend);\n",
                       encoding="utf-8")
        require(offenders(fake) == [],
                f"a hand-written mix that uses `blend` is clean ({offenders(fake)})")

        bad.unlink()
        (fake / "src" / "weather" / "Notes.txt").write_text("switch (phase.primary)", encoding="utf-8")
        require(offenders(fake) == [], "a non-C++ file is not scanned")

    # The stale-exemption rule, over a fixture rather than over the repository -- which has no
    # exemptions to be stale.
    with tempfile.TemporaryDirectory() as raw:
        fake = Path(raw)
        (fake / "src").mkdir(parents=True)
        (fake / "src" / "Gone.cpp").write_text("switch (phase.primary) {}\n", encoding="utf-8")
        (fake / "src" / "Passes.cpp").write_text("perSeason[phase.primary] * phase.blend;\n",
                                                 encoding="utf-8")
        original = dict(EXEMPT)
        try:
            EXEMPT.clear()
            EXEMPT["src/Missing.cpp"] = "does not exist"
            require(stale_exemptions(fake) == ["src/Missing.cpp: exempt and no longer exists"],
                    f"an exemption for a file that is gone is reported ({stale_exemptions(fake)})")
            EXEMPT.clear()
            EXEMPT["src/Passes.cpp"] = "would pass anyway"
            require(len(stale_exemptions(fake)) == 1 and "passes the rule anyway" in
                    stale_exemptions(fake)[0],
                    f"an exemption that is not doing anything is reported ({stale_exemptions(fake)})")
            EXEMPT.clear()
            EXEMPT["src/Gone.cpp"] = "really needs it"
            require(stale_exemptions(fake) == [],
                    f"an exemption that IS load-bearing is left alone ({stale_exemptions(fake)})")
        finally:
            EXEMPT.clear()
            EXEMPT.update(original)

    require(stale_exemptions(REPO) == [] and offenders(REPO) == [],
            f"the repository itself is clean ({offenders(REPO)}, {stale_exemptions(REPO)})")
    print("check_season_usage: selftest passed." if not failures
          else f"check_season_usage: {failures} claim(s) FAILED")
    return 1 if failures else 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)
    if args.selftest:
        return selftest()

    problems = [f"{path}: reads an integer season with no blend weight (§36.3)"
                for path in offenders(REPO)]
    problems += stale_exemptions(REPO)
    if problems:
        print(f"check_season_usage: {len(problems)} problem(s) in {REPO}")
        for problem in problems:
            print(f"[season-as-enum] {problem}")
        return 1
    print(f"check_season_usage: clean ({len(EXEMPT)} exemption(s), each with a reason).")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
