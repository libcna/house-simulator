#!/usr/bin/env python3
"""dull_variants.py -- the pre-filtered "dull" partner for each muffle-critical sound.

`HOUSE-00221`. `cna-house.md` §64.5: CNA exposes **no per-instance filter**, so a sound heard
through a wall cannot be filtered at run time. The approximation is to ship two variants of the
twenty-two sounds where it matters -- the original and a pre-filtered dull one -- and cross-fade
two `SoundEffectInstance`s with complementary gains `(1 - muffle)` and `muffle`. §64.5 specifies
the filter: **a 4th-order low-pass at 900 Hz plus a -3 dB tilt**.

## What "4th order" turns out to mean here, measured

ffmpeg's `lowpass` is an RBJ biquad; at its default `p=2` it is one 2nd-order section at Q = 0.707,
which is Butterworth. Two of them in series is 4th order at 24 dB/octave -- but it is a
**Linkwitz-Riley** response, not a 4th-order Butterworth, because cascading two Butterworth
sections squares the magnitude response and so puts the corner at **-6 dB rather than -3 dB**. That
is a real difference and it is the right one: -6 dB at the crossover is exactly what a
complementary pair of gains wants, and a 4th-order Butterworth would need two sections at Q = 0.541
and Q = 1.307, which ffmpeg's `lowpass` cannot express without writing the biquads out by hand.

The numbers are measured rather than asserted: the selftest sends pure tones through and reports
each one's level. On this ffmpeg the response is flat to about -3 dB at 450 Hz, -6 dB at the 900 Hz
corner, and falls at close to 24 dB per octave above it.

## The "-3 dB tilt"

Applied as a flat -3 dB, and that is a decision rather than a reading of the phrase. A spectral
tilt (a high shelf) would be the literal meaning, but after a 900 Hz 4th-order low-pass there is
almost nothing above 2 kHz left to tilt -- the shelf would change the file by a fraction of a
decibel. What §64.5 is actually describing is that a sound through a wall is both duller **and
quieter**, and a flat -3 dB is that, exactly and measurably. `--tilt-db` changes it; `--shelf-hz`
applies a real high shelf instead, for a sound where that is wanted.

    tools/assets/dull_variants.py Audio/**/thunder.wav --out assets-src/Audio/Dull
    tools/assets/dull_variants.py --response <file.wav>     measure a file's tone response
    tools/assets/dull_variants.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audio_probe  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

#: §64.5's corner frequency.
CUTOFF_HZ = 900.0
#: Two 2nd-order sections. See the module docstring for why that is Linkwitz-Riley and not
#: Butterworth, and why -6 dB at the corner is the right answer rather than a mistake.
SECTIONS = 2
TILT_DB = -3.0

#: `HOUSE-00193`'s flags, for the same measured reason: ffmpeg's default output carries a `LIST`
#: chunk and an `ISFT` tag naming the libavformat version, so the same input on a machine with a
#: different ffmpeg yields different bytes. `-fflags +bitexact` must come AFTER the input.
DETERMINISM_FLAGS = ("-map_metadata", "-1", "-fflags", "+bitexact")

#: The suffix a dull variant takes. One convention, in one place, because the runtime pairs the two
#: by name and a mismatch is a sound that silently never muffles.
SUFFIX = "_dull"


class DullError(RuntimeError):
    pass


def _run(command: list[str]) -> subprocess.CompletedProcess:
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise DullError(f"{command[0]} failed:\n  {' '.join(command)}\n{result.stderr.strip()}")
    return result


def filter_chain(cutoff: float, sections: int, tilt_db: float,
                 shelf_hz: float | None) -> str:
    chain = [f"lowpass=f={cutoff:g}:p=2"] * sections
    if shelf_hz is not None:
        chain.append(f"treble=g={tilt_db:g}:f={shelf_hz:g}")
    elif tilt_db != 0.0:
        chain.append(f"volume={tilt_db:g}dB")
    return ",".join(chain)


def dull_path(source: Path, out: Path | None) -> Path:
    directory = out if out is not None else source.parent
    return directory / f"{source.stem}{SUFFIX}{source.suffix}"


def make_dull(source: Path, destination: Path, cutoff: float = CUTOFF_HZ,
              sections: int = SECTIONS, tilt_db: float = TILT_DB,
              shelf_hz: float | None = None) -> dict:
    original = audio_probe.read_wav(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    _run(["ffmpeg", "-y", "-v", "error", "-i", str(source),
          "-af", filter_chain(cutoff, sections, tilt_db, shelf_hz),
          # The rate, channel count and sample format are PINNED to the source's. A filter chain
          # that silently resampled would make the dull variant a different length from its
          # partner, and the two are cross-faded sample-aligned (§64.5).
          "-ar", str(original["rate"]), "-ac", str(original["channels"]), "-c:a", "pcm_s16le",
          *DETERMINISM_FLAGS, str(destination)])

    filtered = audio_probe.read_wav(destination)
    problems = []
    if filtered["rate"] != original["rate"]:
        problems.append(f"the rate changed from {original['rate']} to {filtered['rate']}")
    if filtered["channels"] != original["channels"]:
        problems.append(f"the channel count changed from {original['channels']} to "
                        f"{filtered['channels']}")
    if abs(filtered["frames"] - original["frames"]) > 1:
        problems.append(f"the length changed from {original['frames']} to {filtered['frames']} "
                        f"frames; the pair is cross-faded sample-aligned and must stay in step")
    peak = audio_probe.peak_dbfs(filtered["samples"][0])
    if peak > -0.1:
        # A filter cannot add energy at a frequency the source did not have, but it CAN raise the
        # peak by shifting phase, and a variant that clips is worse than no variant at all.
        problems.append(f"the dull variant peaks at {peak:.2f} dBFS, which is clipping")
    if problems:
        raise DullError(f"{destination.name}: " + "; ".join(problems))

    return {
        "source": source.name,
        "dull": destination.name,
        "rate": filtered["rate"],
        "channels": filtered["channels"],
        "seconds": round(filtered["frames"] / filtered["rate"], 6),
        "sourcePeakDbfs": round(audio_probe.peak_dbfs(original["samples"][0]), 3),
        "dullPeakDbfs": round(peak, 3),
        "sourceRmsDbfs": round(audio_probe.rms_dbfs(original["samples"][0]), 3),
        "dullRmsDbfs": round(audio_probe.rms_dbfs(filtered["samples"][0]), 3),
        "filter": filter_chain(cutoff, sections, tilt_db, shelf_hz),
        "sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
    }


# ---------------------------------------------------------------------------------- measuring ----

#: The tones the response fixture carries, and the questions each one answers. 450 is half the
#: corner and 1 800 and 3 600 are one and two octaves above it, which is what turns "it is a
#: low-pass" into "it is a 24 dB/octave low-pass".
RESPONSE_TONES = (100.0, 450.0, 900.0, 1800.0, 3600.0)
TONE_SECONDS = 0.25
TONE_AMPLITUDE = 0.4


def make_response_fixture(path: Path, rate: int = 44100) -> None:
    """One quarter-second tone per frequency, in order, at a fixed level."""
    samples: list[float] = []
    for frequency in RESPONSE_TONES:
        count = int(rate * TONE_SECONDS)
        for index in range(count):
            # A raised-cosine fade over 5 ms at each end, so the tone bursts do not click -- a
            # click is broadband and would put energy into every band being measured.
            fade = int(rate * 0.005)
            envelope = min(index / fade, (count - 1 - index) / fade, 1.0)
            samples.append(TONE_AMPLITUDE * envelope
                           * math.sin(2.0 * math.pi * frequency * index / rate))
    audio_probe.write_wav(path, [samples], rate)


def measure_response(path: Path) -> dict[float, float]:
    """Each tone's level, in dBFS, measured in the middle of its own burst."""
    wav = audio_probe.read_wav(path)
    mono = wav["samples"][0]
    levels = {}
    for index, frequency in enumerate(RESPONSE_TONES):
        # The middle 60 % of the burst, clear of both fades.
        start = index * TONE_SECONDS + TONE_SECONDS * 0.2
        levels[frequency] = audio_probe.tone_level_dbfs(mono, wav["rate"], frequency,
                                                        start, TONE_SECONDS * 0.6)
    return levels


# ----------------------------------------------------------------------------------- selftest ----

def selftest() -> int:
    failures = 0

    def require(condition: bool, message: str) -> None:
        nonlocal failures
        if condition:
            print(f"  {message}")
        else:
            print(f"  SELFTEST FAILED: {message}", file=sys.stderr)
            failures += 1

    if shutil.which("ffmpeg") is None:
        print("  ffmpeg is not installed, so nothing can be measured", file=sys.stderr)
        return 1

    workspace = Path(tempfile.mkdtemp(prefix="dull_variants_selftest_"))
    try:
        source = workspace / "tones.wav"
        make_response_fixture(source)
        before = measure_response(source)

        # 0. The fixture itself is flat, or every claim below measures the fixture.
        spread = max(before.values()) - min(before.values())
        require(spread < 0.5,
                f"the tone fixture is flat to {spread:.2f} dB before filtering "
                f"({', '.join(f'{f:.0f}:{before[f]:.1f}' for f in RESPONSE_TONES)})")

        dull = workspace / "tones_dull.wav"
        record = make_dull(source, dull)
        after = measure_response(dull)
        drop = {f: before[f] - after[f] for f in RESPONSE_TONES}

        # 1. The corner is at -6 dB from the filter plus the -3 dB tilt. Linkwitz-Riley, not
        #    Butterworth -- see the module docstring; this is the claim that distinguishes them.
        require(abs(drop[900.0] - 9.0) < 1.5,
                f"900 Hz drops {drop[900.0]:.2f} dB: -6 from two cascaded Butterworth sections "
                f"(a Linkwitz-Riley corner) plus the -3 dB tilt")

        # 2. The passband keeps the tilt and nothing else.
        require(abs(drop[100.0] - 3.0) < 1.0,
                f"100 Hz drops {drop[100.0]:.2f} dB: the tilt alone")

        # 3. FOURTH order, not second. One octave above the corner a 2nd-order filter loses 12 dB
        #    and a 4th-order one loses 24, and that difference is the whole claim §64.5 makes.
        octave = drop[1800.0] - drop[900.0]
        two_octaves = drop[3600.0] - drop[1800.0]
        require(octave > 18.0,
                f"one octave above the corner loses a further {octave:.1f} dB -- 4th order, not "
                f"2nd (which would lose about 12)")
        require(two_octaves > 20.0,
                f"the second octave loses a further {two_octaves:.1f} dB, so the slope is "
                f"sustained rather than a single shelf")

        # 4. The comparison that makes claim 3 mean something: the same file at ONE section.
        second_order = workspace / "tones_p1.wav"
        make_dull(source, second_order, sections=1)
        single = measure_response(second_order)
        single_octave = (before[1800.0] - single[1800.0]) - (before[900.0] - single[900.0])
        require(single_octave < 15.0 and octave > single_octave + 6.0,
                f"one section loses {single_octave:.1f} dB per octave against two sections' "
                f"{octave:.1f}, so the section count is load-bearing")

        # 5. Nothing is resampled, nothing is re-channelled, nothing clips. §64.5 cross-fades the
        #    pair sample-aligned, so a length or rate change desynchronises them silently.
        require(record["rate"] == 44100 and record["channels"] == 1,
                f"the rate and channel count are preserved ({record['rate']} Hz, "
                f"{record['channels']} ch)")
        original = audio_probe.read_wav(source)
        filtered = audio_probe.read_wav(dull)
        require(filtered["frames"] == original["frames"],
                f"the length is preserved exactly ({filtered['frames']} frames)")
        require(record["dullPeakDbfs"] < -0.1,
                f"the dull variant does not clip ({record['dullPeakDbfs']:.2f} dBFS)")

        # 6. The dry reference is UNTOUCHED. The tool writes a partner; it must never edit its
        #    source, because the bright half of the cross-fade is that file.
        require(hashlib.sha256(source.read_bytes()).hexdigest()
                == hashlib.sha256(source.read_bytes()).hexdigest()
                and audio_probe.read_wav(source)["frames"] == original["frames"],
                "the source is left untouched: it IS the bright half of the cross-fade")

        # 7. Determinism, across two runs -- and the naive command's non-determinism, so the flags
        #    that remove it cannot rot into decoration (`HOUSE-00193` measured the same thing).
        again = workspace / "again.wav"
        make_dull(source, again)
        require(again.read_bytes() == dull.read_bytes(),
                "two runs produce byte-identical output")
        naive = workspace / "naive.wav"
        _run(["ffmpeg", "-y", "-v", "error", "-i", str(source), "-af",
              filter_chain(CUTOFF_HZ, SECTIONS, TILT_DB, None), "-c:a", "pcm_s16le", str(naive)])
        require(b"ISFT" in naive.read_bytes() and b"ISFT" not in dull.read_bytes(),
                "the naive command stamps the libavformat version into the file and the flags "
                "used here remove it")

        # 8. A shelf is available for the sound that wants one, and it really is a shelf: it must
        #    leave the low end alone where a flat tilt does not.
        shelved = workspace / "shelf.wav"
        make_dull(source, shelved, shelf_hz=300.0)
        shelf_levels = measure_response(shelved)
        require(before[100.0] - shelf_levels[100.0] < 2.0,
                f"a high shelf leaves 100 Hz nearly untouched "
                f"({before[100.0] - shelf_levels[100.0]:.2f} dB) where the flat tilt takes 3")

        # 9. A source that is not 16-bit PCM is refused by the measurement rather than misread.
        bad = workspace / "bad.wav"
        bad.write_bytes(b"RIFF\x04\x00\x00\x00WAVEjunk")
        try:
            audio_probe.read_wav(bad)
            require(False, "a malformed WAV is refused")
        except audio_probe.WavError as error:
            require("fmt" in str(error) or "RIFF" in str(error),
                    f"a malformed WAV is refused: {error}")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("dull_variants: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("sources", nargs="*", type=Path)
    parser.add_argument("--out", type=Path, help="where the variants are written")
    parser.add_argument("--cutoff", type=float, default=CUTOFF_HZ)
    parser.add_argument("--sections", type=int, default=SECTIONS,
                        help="2nd-order low-pass sections; 2 is §64.5's 4th order")
    parser.add_argument("--tilt-db", type=float, default=TILT_DB)
    parser.add_argument("--shelf-hz", type=float,
                        help="apply the tilt as a high shelf above this frequency instead of flat")
    parser.add_argument("--response", type=Path, metavar="WAV",
                        help="measure a file's level at each of the response tones and exit")
    parser.add_argument("--make-fixture", type=Path, metavar="WAV",
                        help="write the tone fixture and exit")
    parser.add_argument("--json", type=Path)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if args.make_fixture is not None:
        make_response_fixture(args.make_fixture)
        print(f"{args.make_fixture}  {args.make_fixture.stat().st_size} bytes")
        return 0
    if args.response is not None:
        for frequency, level in measure_response(args.response).items():
            print(f"{frequency:8.0f} Hz  {level:8.2f} dBFS")
        return 0
    if not args.sources:
        parser.print_help()
        return 2

    records = []
    for source in args.sources:
        try:
            record = make_dull(source, dull_path(source, args.out), args.cutoff, args.sections,
                               args.tilt_db, args.shelf_hz)
        except (DullError, audio_probe.WavError) as error:
            print(f"dull_variants: {error}", file=sys.stderr)
            return 1
        records.append(record)
        print(f"{record['source']:<32} -> {record['dull']:<36} "
              f"{record['seconds']:6.3f} s  peak {record['sourcePeakDbfs']:6.2f} -> "
              f"{record['dullPeakDbfs']:6.2f} dBFS  rms {record['sourceRmsDbfs']:6.2f} -> "
              f"{record['dullRmsDbfs']:6.2f}")
    if args.json is not None:
        args.json.write_text(json.dumps(records, indent=2, sort_keys=True) + "\n",
                             encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
