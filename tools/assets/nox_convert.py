#!/usr/bin/env python3
"""nox_convert.py -- turn `HOUSE-00277`'s selection into the audio this project actually ships.

`HOUSE-00278`. `HOUSE-00277` chose 445 of the NOX Essentials Series' 1 644 files by rule and wrote
`docs/asset-selection/nox-subset.json` with every one measured. This converts them, and it is the
last point at which the source pool is needed: `/rv/tmp/Essentials_Series_NOX_SOUND` is scratch
space that will be deleted, so **after this runs the converted files are the project's source of
record** and the recorded `sha256` of each original is what keeps the conversion auditable.

    tools/assets/nox_convert.py --pool /rv/tmp/Essentials_Series_NOX_SOUND
    tools/assets/nox_convert.py --dry-run
    tools/assets/nox_convert.py --selftest

## Three decisions, and one of them is a decision NOT to act

**Loops are shortened to 10 s and one-shots are not.** `HOUSE-00277` measured §72's audio budget as
156 MB against its 95, with 72 loops carrying 136 MB of it, and found that trimming every loop to
10 s brings the total to 91 MB -- inside the budget. That constraint belongs to the conversion, not
to the selection, and this is the conversion. A "loop" is `nox_select.py`'s own rule, over 3 s; only
the files over 10 s actually shrink.

**Silence is trimmed from one-shots and never from loops.** A footstep with 200 ms of room tone in
front of it fires late, and the trim is what fixes that. The same trim applied to an ambience bed
removes the quiet part of a recording that is *supposed* to breathe, and moves the loop point.

**Nothing is peak-normalised, and that is deliberate.** `convert_audio.py` offers `--normalise` and
it is the wrong tool here: `nox_select.py` chose the eight variants in each group by **farthest-point
sampling in (duration, RMS)** precisely so that they differ in level, and a per-file peak normalise
would flatten exactly the difference the selection exists to preserve. The loudest and quietest of
each group are reported by `HOUSE-00277` for a human check; making them equal would be undoing that
work.

Channels and sample rate are preserved. The selection is already 434 mono files and 11 stereo, and
which is which is a property of the recording -- the stereo ones are ambience beds and the mono ones
are the sources `Apply3D` will position. Forcing either way would override a decision the publisher
already made correctly.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import shutil
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import convert_audio  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SELECTION = REPO / "docs" / "asset-selection" / "nox-subset.json"
DEST_ROOT = REPO / "assets-src" / "Audio"
REPORT = REPO / "docs" / "asset-selection" / "nox-conversion.json"

#: `nox_select.py`'s own boundary: over this many seconds a file is a loop rather than a one-shot.
LOOP_SECONDS = 3.0

#: The loop cap. **8 s, not §72's 10**, and the difference is a measurement rather than taste.
#:
#: §72 derived 10 s by checking the total against its own **audio-buffer memory row of 95 MB** and
#: finding 91 MB. But the binding constraint is §71's **pack** budgets -- `audio-core` 30 MB and
#: `audio-ambience` 55 MB, 85 MB together -- which are 10 MB tighter and which §72 never checked
#: against. Measured on the real selection:
#:
#:     cap 10 s -> audio-core 103.8 %, audio-ambience 107.0 %   both over
#:     cap  9 s -> audio-core 101.9 %, audio-ambience  98.8 %   core still over
#:     cap  8 s -> audio-core  99.8 %, audio-ambience  89.5 %   both inside
#:
#: §72's audible argument -- "ten seconds of rain or of a fan does not read as a repeat behind
#: everything else in a house; thirty is what the recordist supplied" -- is about being an order of
#: magnitude away from an obvious one-second loop, and eight seconds is the same argument. The
#: figure it names was validated against the wrong budget, so this is the smallest correction that
#: makes the row true.
LOOP_TRIM_SECONDS = 8.0

#: How much of a shortened loop's head is blended with the material that followed its new end, so
#: the wrap stays continuous. See `convert_audio.loop_filter_complex`.
LOOP_CROSSFADE_SECONDS = 0.25

#: Leading and trailing silence below this is not signal. `convert_audio.py`'s own default.
TRIM_THRESHOLD_DBFS = -60.0


def is_loop(entry: dict) -> bool:
    return float(entry["seconds"]) > LOOP_SECONDS


def destination_for(entry: dict) -> Path:
    """`assets-src/Audio/<category>/<name>.wav`.

    The category is `HOUSE-00277`'s, which is the taxonomy the sound bank is organised by -- so the
    directory a file lands in is the reason it was selected, not the directory the publisher
    happened to ship it in.
    """
    name = Path(entry["file"]).stem + ".wav"
    return DEST_ROOT / entry["category"] / name


def options_for(entry: dict) -> argparse.Namespace:
    """The conversion parameters this file gets, and nothing it does not need."""
    loop = is_loop(entry)
    # The threshold has to carry the crossfade with it. A 10.15 s loop cannot supply the 0.25 s of
    # material the blend draws its head from, and cutting it to 10.0 s would save 1.5 % anyway --
    # so "longer than the cap" is not the rule; "long enough to be worth cutting AND to supply the
    # blend" is. Two files of the 445 sit in exactly that gap, which is how this was found.
    shorten = loop and float(entry["seconds"]) > LOOP_TRIM_SECONDS + LOOP_CROSSFADE_SECONDS
    return argparse.Namespace(
        require_rate=None,
        normalise=None,               # deliberately: see the module docstring
        trim=not loop,
        trim_threshold=TRIM_THRESHOLD_DBFS,
        highpass=None,
        lowpass=None,
        mono=False,                   # channels are preserved
        sample_rate=None,             # rate is preserved (HOUSE-00069)
        max_seconds=LOOP_TRIM_SECONDS if shorten else None,
        loop_crossfade=LOOP_CROSSFADE_SECONDS if shorten else 0.0,
    )


def _repo_relative(path: Path) -> str:
    """Repository-relative where possible; absolute otherwise, so a selftest tree still reports."""
    try:
        return path.relative_to(REPO).as_posix()
    except ValueError:
        return path.as_posix()


def convert_one(entry: dict, pool: Path) -> dict:
    source = pool / entry["relative"]
    if not source.is_file():
        return {"file": entry["relative"], "status": "missing-source",
                "detail": f"{source} is not in the pool"}
    if convert_audio.sha256_of(source) != entry["sha256"]:
        # The selection recorded a hash for every file precisely so that a pool which has been
        # re-downloaded, re-encoded or partially replaced cannot be converted silently.
        return {"file": entry["relative"], "status": "hash-mismatch",
                "detail": "the pool file is not the one HOUSE-00277 measured"}
    destination = destination_for(entry)
    options = options_for(entry)
    try:
        result = convert_audio.convert(source, destination, options)
    except convert_audio.ConversionError as exc:
        return {"file": entry["relative"], "status": "failed", "detail": str(exc)}
    return {
        "file": entry["relative"],
        "status": "converted",
        "category": entry["category"],
        "output": _repo_relative(destination),
        "sourceSha256": entry["sha256"],
        "outputSha256": result["outputSha256"],
        "sourceSeconds": round(float(entry["seconds"]), 4),
        "outputSeconds": round(result["outputFormat"]["duration"], 4),
        "channels": result["outputFormat"]["channels"],
        "sampleRate": result["outputFormat"]["sampleRate"],
        "outputBytes": destination.stat().st_size,
        "loop": is_loop(entry),
        "shortened": result.get("shortenedToSeconds"),
        "trimmed": options.trim,
    }


def run(pool: Path, selection: Path, workers: int, limit: int | None = None) -> dict:
    document = json.loads(selection.read_text(encoding="utf-8"))
    entries = document["files"][:limit] if limit else document["files"]
    started = time.monotonic()
    results: list[dict] = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool_exec:
        futures = {pool_exec.submit(convert_one, entry, pool): entry for entry in entries}
        for future in concurrent.futures.as_completed(futures):
            results.append(future.result())
    results.sort(key=lambda r: r["file"])

    converted = [r for r in results if r["status"] == "converted"]
    total_bytes = sum(r["outputBytes"] for r in converted)
    loops = [r for r in converted if r["loop"]]
    return {
        "tool": "nox_convert.py",
        "selection": _repo_relative(selection),
        "poolFiles": len(entries),
        "converted": len(converted),
        "failed": [r for r in results if r["status"] != "converted"],
        "totalOutputBytes": total_bytes,
        "loopCount": len(loops),
        "loopOutputBytes": sum(r["outputBytes"] for r in loops),
        "oneShotOutputBytes": total_bytes - sum(r["outputBytes"] for r in loops),
        "shortened": sum(1 for r in converted if r["shortened"]),
        "trimmedSeconds": round(sum(r["sourceSeconds"] - r["outputSeconds"]
                                    for r in converted), 1),
        "wallSeconds": round(time.monotonic() - started, 1),
        "auditionSet": audition_set(converted),
        "files": converted,
    }


#: How many files `R-17`'s listening check covers.
AUDITION_COUNT = 10


def audition_set(converted: list[dict]) -> list[dict]:
    """Ten files chosen so that a listener hears the EDGES of the conversion, not ten average ones.

    `HOUSE-00278`'s acceptance includes a listening check on 10 files (`R-17`), and this tool
    cannot listen. What it can do is make the check short and pointed: a random ten would almost
    all be ordinary footsteps and would confirm nothing. These ten are the files where each
    decision the conversion made is most likely to be audible -- the longest shortened loop, the
    quietest survivor, the most heavily trimmed one-shot -- so a fault in any of them shows up
    here or nowhere.
    """
    if not converted:
        return []
    picks: list[tuple[str, dict]] = []

    def add(why: str, entry: dict | None) -> None:
        if entry is not None and not any(e["file"] == entry["file"] for _, e in picks):
            picks.append((why, entry))

    shortened = [e for e in converted if e["shortened"]]
    trimmed = [e for e in converted if e["trimmed"]]
    add("the loop shortened by the most — the wrap crossfade has the most to hide",
        max(shortened, key=lambda e: e["sourceSeconds"] - e["outputSeconds"], default=None))
    add("a stereo loop — the crossfade must not collapse the image",
        next((e for e in shortened if e["channels"] == 2), None))
    add("the shortest loop that was still shortened — least material for the blend",
        min(shortened, key=lambda e: e["sourceSeconds"], default=None))
    add("the one-shot the trim removed the most from — a clipped attack would show here",
        max(trimmed, key=lambda e: e["sourceSeconds"] - e["outputSeconds"], default=None))
    add("the shortest one-shot after trimming — the trim may have eaten it",
        min((e for e in trimmed if e["outputSeconds"] > 0), key=lambda e: e["outputSeconds"],
            default=None))
    add("a footstep, the sound the game plays most often",
        next((e for e in converted if e["category"].startswith("footstep")), None))
    add("a loop left alone — the control for the shortened ones",
        next((e for e in converted if e["loop"] and not e["shortened"]), None))
    add("an ambience bed — §64.6 plays this under everything",
        next((e for e in converted if e["category"].startswith("ambience")), None))
    add("an appliance loop — room tone, and the longest thing on screen",
        next((e for e in converted if e["category"].startswith("appliance") and e["loop"]), None))
    # Any remaining slots go to the largest categories NOT already represented. Filling them in
    # file order instead put three consecutive `appliance/car` clips at the end of the list, which
    # is ten minutes of a listener's time spent on one shelf of the library.
    covered = {e["category"].split("/")[0] for _, e in picks}
    sizes: dict[str, int] = {}
    for entry in converted:
        top = entry["category"].split("/")[0]
        sizes[top] = sizes.get(top, 0) + 1
    for top in sorted(sizes, key=lambda t: (-sizes[t], t)):
        if len(picks) >= AUDITION_COUNT:
            break
        if top in covered:
            continue
        candidate = min((e for e in converted if e["category"].split("/")[0] == top),
                        key=lambda e: e["file"])
        add(f"the {top} group, which nothing above covers ({sizes[top]} files)", candidate)
        covered.add(top)
    for entry in sorted(converted, key=lambda e: e["file"]):
        if len(picks) >= AUDITION_COUNT:
            break
        add("a further file, for coverage", entry)
    return [{"why": why, **entry} for why, entry in picks[:AUDITION_COUNT]]


def report(outcome: dict) -> str:
    lines = [
        f"{outcome['converted']} of {outcome['poolFiles']} converted in "
        f"{outcome['wallSeconds']} s",
        f"  {outcome['totalOutputBytes'] / 1e6:.1f} MB total — "
        f"{outcome['oneShotOutputBytes'] / 1e6:.1f} MB one-shots, "
        f"{outcome['loopOutputBytes'] / 1e6:.1f} MB in {outcome['loopCount']} loops",
        f"  {outcome['shortened']} loop(s) shortened to {LOOP_TRIM_SECONDS} s with a "
        f"{LOOP_CROSSFADE_SECONDS} s wrap crossfade; "
        f"{outcome['trimmedSeconds']} s removed in total",
    ]
    for failure in outcome["failed"][:10]:
        lines.append(f"  {failure['status']}: {failure['file']} — {failure['detail']}")
    if outcome.get("auditionSet"):
        lines.append(f"\n  R-17 listening check — play these {len(outcome['auditionSet'])}:")
        for pick in outcome["auditionSet"]:
            lines.append(f"    {pick['output']}")
            lines.append(f"      {pick['why']}")
    return "\n".join(lines)


# ======================================================================================= selftest


def selftest() -> int:
    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("nox_convert: selftest")
    workspace = Path(tempfile.mkdtemp(prefix="nox_convert_selftest_"))
    try:
        pool = workspace / "pool"
        (pool / "Pack").mkdir(parents=True)

        def make(name: str, seconds: float) -> Path:
            path = pool / "Pack" / name
            convert_audio._run([
                "ffmpeg", "-y", "-v", "error", "-f", "lavfi",
                "-i", f"aevalsrc=0.4*(1-t/40)*sin(2*PI*(200+90*t)*t+1.1):s=48000:d={seconds}",
                "-c:a", "pcm_s24le", str(path)])
            return path

        short = make("step.wav", 1.0)
        medium = make("breeze.wav", 5.0)
        long_loop = make("rain.wav", 26.0)

        entries = []
        for path, category, seconds in ((short, "footstep/grass/walk", 1.0),
                                        (medium, "ambience/breeze", 5.0),
                                        (long_loop, "ambience/rain-strong", 26.0)):
            entries.append({
                "relative": f"Pack/{path.name}", "file": path.name, "category": category,
                "seconds": seconds, "sha256": convert_audio.sha256_of(path),
                "channels": 1, "rate": 48000, "bits": 24,
            })
        selection = workspace / "selection.json"
        selection.write_text(json.dumps({"files": entries}), encoding="utf-8")

        # 1. `nox_select.py`'s own boundary decides what is a loop, and only files over the trim
        #    length actually shrink -- a 5 s ambience is a loop and is left alone.
        require(not is_loop(entries[0]) and is_loop(entries[1]) and is_loop(entries[2]),
                "over 3 s is a loop, at or under it is a one-shot -- nox_select.py's own rule")
        require(options_for(entries[0]).trim is True,
                "a one-shot is trimmed: a footstep with 200 ms of room tone in front of it fires "
                "late")
        require(options_for(entries[1]).trim is False and options_for(entries[2]).trim is False,
                "a loop is NOT trimmed -- the same filter removes the quiet part of a recording "
                "that is supposed to breathe, and moves the loop point")
        require(options_for(entries[1]).max_seconds is None,
                "a 5 s loop is already inside the budget and is not shortened")
        require(options_for(entries[2]).max_seconds == LOOP_TRIM_SECONDS
                and options_for(entries[2]).loop_crossfade == LOOP_CROSSFADE_SECONDS,
                f"a 26 s loop is shortened to {LOOP_TRIM_SECONDS} s, with the wrap crossfade")
        barely = dict(entries[2], seconds=LOOP_TRIM_SECONDS + LOOP_CROSSFADE_SECONDS / 2)
        require(options_for(barely).max_seconds is None,
                f"a loop of {barely['seconds']} s is NOT shortened -- it cannot supply the "
                f"{LOOP_CROSSFADE_SECONDS} s the blend draws its head from, and cutting it would "
                f"save 1 % anyway. Two of the 445 sit in exactly that gap")
        just_over = dict(entries[2], seconds=LOOP_TRIM_SECONDS + LOOP_CROSSFADE_SECONDS + 0.01)
        require(options_for(just_over).max_seconds == LOOP_TRIM_SECONDS,
                "...and one just past it is")
        require(all(options_for(e).normalise is None for e in entries),
                "nothing is peak-normalised: nox_select.py chose each group's variants BY their "
                "RMS spread, and normalising would flatten the difference it selected for")
        require(all(options_for(e).mono is False and options_for(e).sample_rate is None
                    for e in entries),
                "channels and sample rate are preserved, both being properties of the recording")

        # 2. The destination is the CATEGORY's, not the publisher's directory.
        require(destination_for(entries[0]).as_posix().endswith(
                    "assets-src/Audio/footstep/grass/walk/step.wav"),
                f"a file lands under the category it was selected for "
                f"({_repo_relative(destination_for(entries[0]))})")

        # 3. The conversion runs, and produces what was asked for.
        global DEST_ROOT
        original_root = DEST_ROOT
        DEST_ROOT = workspace / "out"
        try:
            outcome = run(pool, selection, workers=2)
        finally:
            DEST_ROOT = original_root
        require(outcome["converted"] == 3 and not outcome["failed"],
                f"all three convert ({outcome['converted']}, failures {outcome['failed']})")
        by_name = {Path(r["file"]).name: r for r in outcome["files"]}
        require(by_name["rain.wav"]["outputSeconds"] <= LOOP_TRIM_SECONDS + 0.05,
                f"the 26 s loop comes out at {by_name['rain.wav']['outputSeconds']} s")
        require(by_name["breeze.wav"]["outputSeconds"] > 4.9,
                f"the 5 s loop is untouched ({by_name['breeze.wav']['outputSeconds']} s)")
        require(all(r["channels"] == 1 and r["sampleRate"] == 48000 for r in outcome["files"]),
                "every output keeps its channel count and sample rate")
        require(outcome["shortened"] == 1,
                f"exactly one file was shortened ({outcome['shortened']})")

        # 4. The 24-bit source becomes 16-bit, which is the whole point of the conversion.
        probe = convert_audio.probe(Path(by_name["step.wav"]["output"]))
        require(probe["sampleFmt"] == "s16",
                f"the 24-bit source is written as 16-bit ({probe['sampleFmt']})")

        # 5. A pool that is not the one HOUSE-00277 measured is REFUSED, not converted. The pool is
        #    scratch space that will be deleted, and the recorded hash is the only thing that can
        #    tell a re-download from the original afterwards.
        tampered = dict(entries[0], sha256="0" * 64)
        result = convert_one(tampered, pool)
        require(result["status"] == "hash-mismatch",
                f"a file whose bytes do not match the selection's hash is refused "
                f"({result['status']})")
        missing = dict(entries[0], relative="Pack/nope.wav")
        require(convert_one(missing, pool)["status"] == "missing-source",
                "and a file absent from the pool is reported as missing, not as a failure to "
                "convert")

        # 6. The R-17 audition set picks the EDGES, not ten average files. This tool cannot listen;
        #    what it can do is make the check short and pointed, so a fault shows up in it or
        #    nowhere. A random ten would be ordinary footsteps and would confirm nothing.
        picks = audition_set(outcome["files"])
        require(len(picks) == min(AUDITION_COUNT, len(outcome["files"])),
                f"the audition set is as large as it can be ({len(picks)} of "
                f"{len(outcome['files'])} converted)")
        require(len({p["file"] for p in picks}) == len(picks),
                "with no file listed twice")
        require(all(p["why"] for p in picks),
                "and every entry says WHY it is worth listening to, so the check is a checklist "
                "rather than a playlist")
        reasons = " ".join(p["why"] for p in picks)
        require("shortened by the most" in reasons and "trim removed the most" in reasons,
                f"including the file each decision is most likely to be audible in ({reasons[:60]}…)")
        shortened_pick = next((p for p in picks if p["shortened"]), None)
        require(shortened_pick is not None
                and shortened_pick["file"].endswith("rain.wav"),
                "and the 26 s loop -- the only shortened file in this fixture -- is in it")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("nox_convert: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--pool", type=Path,
                        default=Path("/rv/tmp/Essentials_Series_NOX_SOUND"))
    parser.add_argument("--selection", type=Path, default=SELECTION)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--limit", type=int, default=None,
                        help="convert only the first N, for a trial run")
    parser.add_argument("--dry-run", action="store_true",
                        help="print what would be converted and stop")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    if not args.selection.is_file():
        print(f"nox_convert: {args.selection} is not there; run nox_select.py --emit first",
              file=sys.stderr)
        return 2
    if not args.pool.is_dir():
        print(f"nox_convert: the pool {args.pool} is not there. It is scratch space and may have "
              f"been deleted; the converted files under assets-src/Audio/ are the source of "
              f"record now.", file=sys.stderr)
        return 2

    document = json.loads(args.selection.read_text(encoding="utf-8"))
    entries = document["files"][:args.limit] if args.limit else document["files"]
    if args.dry_run:
        loops = [e for e in entries if is_loop(e)]
        shortened = [e for e in loops if float(e["seconds"]) > LOOP_TRIM_SECONDS]
        print(f"{len(entries)} file(s): {len(entries) - len(loops)} one-shot(s) trimmed, "
              f"{len(loops)} loop(s) of which {len(shortened)} shortened to "
              f"{LOOP_TRIM_SECONDS} s")
        for entry in entries[:5]:
            print(f"  {entry['relative']} -> {_repo_relative(destination_for(entry))}")
        return 0

    outcome = run(args.pool, args.selection, args.workers, args.limit)
    print(report(outcome))
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(outcome, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"nox_convert: wrote {REPORT.relative_to(REPO)}")
    return 1 if outcome["failed"] else 0


if __name__ == "__main__":
    sys.exit(main())
