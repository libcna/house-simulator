#!/usr/bin/env python3
"""convert_audio.py -- source WAV to the runtime's audio profile, reproducibly.

`HOUSE-00193`. One tool, one output format, every parameter written down, and the same input always
producing the same bytes.

    tools/assets/convert_audio.py IN.wav OUT.wav              # 16-bit, rate preserved
    tools/assets/convert_audio.py IN.wav OUT.wav --mono       # downmix for Apply3D
    tools/assets/convert_audio.py IN.wav OUT.wav --trim --normalise -3
    tools/assets/convert_audio.py IN.wav OUT.wav --lowpass 900   # the "dull" variant filter
    tools/assets/convert_audio.py --probe IN.wav              # what is this file?
    tools/assets/convert_audio.py --selftest                  # prove the claims below

**The sample rate is PRESERVED, and the task title saying "48→44.1 kHz" is stale.** `HOUSE-00069`
measured the two halves of that conversion separately on a NOX source: 24→16 bit costs 0.0017 dB
RMS, while adding `-ar 44100` costs **0.889 dB RMS and 0.26 dB peak**, because resampling 48 → 44.1
lowpasses content the source carries. CNA loaded and played a 48 kHz asset correctly and the mixer
resamples at playback anyway, so the offline resample bought nothing and cost signal. Bit depth and
sample rate are independent decisions and this tool treats them as such: `--sample-rate` exists, is
never the default, and says what it is doing.

**Nothing is assumed about the input.** `HOUSE-00276` measured all 1 644 files of the NOX Essentials
Series: the publisher documents "48 kHz / 24-bit" and **23 files are neither** — 20 at 96 kHz and 3
at 32-bit `pcm_s32le`. Every input is probed, and `--require-rate` fails loudly rather than silently
resampling something unexpected.

**Determinism is a measured property, not a hope.** `ffmpeg` writes a `LIST/INFO` chunk carrying the
source's metadata and, worse, an `ISFT` encoder tag naming the libavformat version — so the naive
command produces different bytes on a machine with a different ffmpeg. `-map_metadata -1` plus
`-fflags +bitexact` **as an output option** removes both; the same flag before `-i` applies to the
demuxer and does not. `--selftest` proves it.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

#: The runtime profile. 16-bit signed PCM, little-endian: what `SoundEffect` wants, and what
#: `HOUSE-00068` measured CNA's own 24→16 conversion to produce bit-for-bit.
RUNTIME_CODEC = "pcm_s16le"

#: Flags that make the output byte-identical across runs AND across ffmpeg builds. Order matters:
#: `-fflags +bitexact` must come after the input to bind to the muxer.
DETERMINISM_FLAGS = ("-map_metadata", "-1", "-fflags", "+bitexact")


class ConversionError(RuntimeError):
    pass


def _run(command: list[str]) -> subprocess.CompletedProcess:
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise ConversionError(
            f"{command[0]} failed:\n  {' '.join(command)}\n{result.stderr.strip()}"
        )
    return result


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def probe(path: Path) -> dict:
    """Everything a conversion decision needs, read from the file rather than assumed."""
    result = _run(
        [
            "ffprobe", "-v", "error",
            "-select_streams", "a:0",
            "-show_entries",
            "stream=codec_name,sample_rate,channels,bits_per_raw_sample,duration,sample_fmt",
            "-of", "json",
            str(path),
        ]
    )
    streams = json.loads(result.stdout).get("streams") or []
    if not streams:
        raise ConversionError(f"{path}: no audio stream")
    stream = streams[0]
    return {
        "codec": stream.get("codec_name"),
        "sampleRate": int(stream.get("sample_rate", 0)),
        "channels": int(stream.get("channels", 0)),
        # `bits_per_raw_sample` is absent for some formats; the sample_fmt is the fallback.
        "bits": int(stream.get("bits_per_raw_sample") or 0) or None,
        "sampleFmt": stream.get("sample_fmt"),
        "duration": float(stream.get("duration", 0.0)),
    }


def build_filters(args: argparse.Namespace) -> list[str]:
    """The `-af` chain, in the order the operations must happen."""
    filters: list[str] = []

    # Highpass first: subsonic rumble skews every measurement that follows, so removing it before
    # trimming and normalising means the trim threshold and the peak both describe audible signal.
    if args.highpass:
        filters.append(f"highpass=f={args.highpass}")

    if args.trim:
        # Both ends. `silenceremove` only handles the leading edge per pass, so the idiom is
        # trim-reverse-trim-reverse. `detection=peak` because RMS detection lags a sharp transient
        # and clips the attack off a footstep, which is the one thing a footstep cannot lose.
        one = (
            f"silenceremove=start_periods=1:start_silence=0:"
            f"start_threshold={args.trim_threshold}dB:detection=peak"
        )
        filters += [one, "areverse", one, "areverse"]

    if args.lowpass:
        # The "dull" filter (`HOUSE-00221`): what a sound through a closed door loses.
        filters.append(f"lowpass=f={args.lowpass}")

    if args.normalise is not None:
        # PEAK normalisation, not `dynaudnorm`. `cna-house.md` §63.5 sketched `dynaudnorm`, which
        # rides the gain over time -- it would flatten the difference between a soft and a hard
        # footstep, and those variants exist precisely to be different. A single fixed gain per file
        # preserves the dynamics between files and within them.
        filters.append(f"volume={args.normalise}dB:precision=fixed")
    return filters


def loop_filter_complex(seconds: float, crossfade: float) -> str:
    """Shorten a LOOP to `seconds` and keep it seamless, as one `-filter_complex` graph.

    **A loop cut with `-t` clicks.** `cna-house.md` §72's budget only closes if every ambience loop
    is trimmed to 10 s (`HOUSE-00277` measured 72 loops carrying 136 MB of the 156), and the
    obvious way to do that leaves the last sample and the first unrelated -- so the file plays a
    step discontinuity once every ten seconds, for as long as the room is on screen. A rain bed
    that ticks is worse than a rain bed that is thirty seconds long.

    **The blend goes at the HEAD, not the tail**, and getting that backwards was the first attempt
    here. What has to be true is `result[N-] == result[0]`, so the head is where the material has
    to be replaced:

        result[t] = source[t]                                    for t in [F, N)
        result[t] = source[t]·w(t) + source[t + N]·(1 - w(t))     for t in [0, F), w: 0 -> 1

    At `t -> N-` the output is `source[N-]`; at `t = 0` it is `source[N]`; and those two are the
    same sample. Fading the *tail* into the *head* instead makes `result[N-]` approach `source[F]`,
    which is a different point in the recording and no more continuous than the naive cut.

    Built from two `afade`s and an `amix` with `normalize=0` rather than from `acrossfade`, which
    produced a 0.6-second output from a 3-second request here whatever it was fed.
    """
    if crossfade <= 0 or crossfade >= seconds:
        raise ConversionError(
            f"a loop crossfade of {crossfade} s does not fit inside {seconds} s")
    return (
        f"[0:a]atrim=0:{seconds},asetpts=PTS-STARTPTS,"
        f"afade=t=in:st=0:d={crossfade}:curve=tri[main];"
        f"[1:a]atrim={seconds}:{seconds + crossfade},asetpts=PTS-STARTPTS,"
        f"afade=t=out:st=0:d={crossfade}:curve=tri[head];"
        f"[main][head]amix=inputs=2:duration=first:normalize=0[out]"
    )


def measure_peak(path: Path) -> float:
    """Peak level in dBFS, via `volumedetect`."""
    result = subprocess.run(
        ["ffmpeg", "-v", "info", "-i", str(path), "-af", "volumedetect", "-f", "null", "-"],
        capture_output=True,
        text=True,
    )
    for line in result.stderr.splitlines():
        if "max_volume:" in line:
            return float(line.split("max_volume:")[1].strip().split()[0])
    raise ConversionError(f"{path}: volumedetect reported no max_volume")


def convert(source: Path, destination: Path, args: argparse.Namespace) -> dict:
    info = probe(source)

    if args.require_rate and info["sampleRate"] != args.require_rate:
        raise ConversionError(
            f"{source}: sample rate is {info['sampleRate']} Hz, not the required "
            f"{args.require_rate} Hz. Convert deliberately with --sample-rate or fix the source; "
            f"this tool does not resample by accident."
        )

    command = ["ffmpeg", "-y", "-v", "error", "-i", str(source)]

    # Peak normalisation needs a measurement pass first, because a fixed gain cannot be chosen
    # without knowing the peak. Done here rather than inside the filter graph so the gain that was
    # applied is a number this tool can report and a later reader can check.
    applied_gain = None
    if args.normalise is not None:
        peak = measure_peak(source)
        applied_gain = round(args.normalise - peak, 4)
        args = argparse.Namespace(**{**vars(args), "normalise": applied_gain})

    filters = build_filters(args)
    max_seconds = getattr(args, "max_seconds", None)
    loop_crossfade = getattr(args, "loop_crossfade", 0.0) or 0.0
    shortened = None
    if max_seconds and info["duration"] > max_seconds:
        shortened = round(max_seconds, 4)
        if loop_crossfade > 0:
            if info["duration"] < max_seconds + loop_crossfade:
                raise ConversionError(
                    f"{source}: {info['duration']:.2f} s is too short to shorten to "
                    f"{max_seconds} s with a {loop_crossfade} s crossfade -- the blend needs "
                    f"{max_seconds + loop_crossfade:.2f} s of material to draw the head from")
            command += ["-i", str(source)]
            # A filter_complex and an -af chain cannot both feed the same output, so the ordinary
            # filters run first into an intermediate and the loop cut is a second pass. Two passes
            # rather than one graph because the trim and the normalise gain are measured from the
            # WHOLE clip, and measuring them from a shortened one would change what they mean.
            command += ["-filter_complex", loop_filter_complex(max_seconds, loop_crossfade),
                        "-map", "[out]"]
        else:
            command += ["-t", str(max_seconds)]
        if filters:
            raise ConversionError(
                "--max-seconds cannot be combined with a filter chain in one pass; run the "
                "filters first and shorten the result")
    elif filters:
        command += ["-af", ",".join(filters)]

    command += ["-c:a", RUNTIME_CODEC]
    if args.mono:
        command += ["-ac", "1"]
    if args.sample_rate:
        command += ["-ar", str(args.sample_rate)]
    command += list(DETERMINISM_FLAGS)
    command.append(str(destination))

    destination.parent.mkdir(parents=True, exist_ok=True)
    _run(command)
    after = probe(destination)

    return {
        "source": str(source),
        "sourceSha256": sha256_of(source),
        "sourceFormat": info,
        "output": str(destination),
        "outputSha256": sha256_of(destination),
        "outputFormat": after,
        "appliedGainDb": applied_gain,
        "shortenedToSeconds": shortened,
        "loopCrossfadeSeconds": loop_crossfade if shortened else None,
        "command": " ".join(command),
    }


# ------------------------------------------------------------------------------------------------
# Selftest -- the claims in the module docstring, each proved or the tool fails.
# ------------------------------------------------------------------------------------------------


def make_probe_wav(path: Path, *, rate: int, bits: int, channels: int, seconds: float) -> None:
    """A deterministic test signal: a sine sweep, so a lowpass has something to remove."""
    codec = {16: "pcm_s16le", 24: "pcm_s24le", 32: "pcm_s32le"}[bits]
    _run(
        [
            "ffmpeg", "-y", "-v", "error",
            "-f", "lavfi",
            "-i", f"sine=frequency=440:sample_rate={rate}:duration={seconds}",
            "-af", f"aeval='val(0)*0.5':c=same,pan={'mono|c0=c0' if channels == 1 else 'stereo|c0=c0|c1=c0'}",
            "-c:a", codec,
            *DETERMINISM_FLAGS,
            str(path),
        ]
    )


def _raises(call) -> bool:
    try:
        call()
    except ConversionError:
        return True
    return False


def selftest() -> int:
    failures: list[str] = []

    def check(name: str, condition: bool, detail: str = "") -> None:
        print(f"  {'PASS' if condition else 'FAIL'}  {name}{(' -- ' + detail) if detail else ''}")
        if not condition:
            failures.append(name)

    print("convert_audio selftest")
    with tempfile.TemporaryDirectory(prefix="cnahouse-audio-") as workdir:
        work = Path(workdir)
        source = work / "src24.wav"
        make_probe_wav(source, rate=48000, bits=24, channels=1, seconds=1.0)

        base = argparse.Namespace(
            mono=False, sample_rate=None, require_rate=None, trim=False,
            trim_threshold=-60, normalise=None, highpass=None, lowpass=None,
        )

        # 1. The bit depth is reduced and the sample rate is NOT touched.
        first = convert(source, work / "a.wav", base)
        check(
            "24-bit in, 16-bit out",
            first["outputFormat"]["sampleFmt"] == "s16",
            f"sample_fmt={first['outputFormat']['sampleFmt']}",
        )
        check(
            "sample rate preserved by default",
            first["outputFormat"]["sampleRate"] == 48000,
            f"{first['sourceFormat']['sampleRate']} -> {first['outputFormat']['sampleRate']} Hz",
        )

        # 2. Deterministic across runs.
        second = convert(source, work / "b.wav", base)
        check(
            "same input, same bytes",
            first["outputSha256"] == second["outputSha256"],
            first["outputSha256"][:16],
        )

        # 3. No toolchain fingerprint in the output. This is the one that fails without
        #    `-fflags +bitexact` on the OUTPUT side, and it fails invisibly.
        tags = json.loads(
            _run(["ffprobe", "-v", "error", "-show_entries", "format_tags",
                  "-of", "json", str(work / "a.wav")]).stdout
        ).get("format", {}).get("tags", {})
        check("no metadata or encoder tag in the output", tags == {}, f"tags={tags}")

        # 3b. ...and prove the naive command WOULD embed one, so the flags are shown to matter.
        _run(["ffmpeg", "-y", "-v", "error", "-i", str(source), "-c:a", RUNTIME_CODEC,
              str(work / "naive.wav")])
        naive_tags = json.loads(
            _run(["ffprobe", "-v", "error", "-show_entries", "format_tags",
                  "-of", "json", str(work / "naive.wav")]).stdout
        ).get("format", {}).get("tags", {})
        check(
            "the naive command DOES embed one (so the flags are load-bearing)",
            "encoder" in naive_tags,
            f"encoder={naive_tags.get('encoder')}",
        )

        # 4. An explicit resample is honoured, and only when asked.
        resampled = convert(
            source, work / "c.wav", argparse.Namespace(**{**vars(base), "sample_rate": 44100})
        )
        check(
            "--sample-rate resamples when asked",
            resampled["outputFormat"]["sampleRate"] == 44100,
        )

        # 5. --require-rate refuses rather than silently resampling.
        try:
            convert(source, work / "d.wav",
                    argparse.Namespace(**{**vars(base), "require_rate": 44100}))
            check("--require-rate refuses a mismatch", False, "it did not raise")
        except ConversionError:
            check("--require-rate refuses a mismatch", True)

        # 6. The lowpass actually removes high content: a 12 kHz tone through a 900 Hz lowpass must
        #    lose level. Asserting the FILTER RAN is not enough -- a misspelled filter name would
        #    make ffmpeg fail, but a filter applied to the wrong graph position would not.
        tone = work / "tone12k.wav"
        _run(["ffmpeg", "-y", "-v", "error", "-f", "lavfi",
              "-i", "sine=frequency=12000:sample_rate=48000:duration=1.0",
              "-c:a", "pcm_s24le", *DETERMINISM_FLAGS, str(tone)])
        dull = convert(tone, work / "dull.wav",
                       argparse.Namespace(**{**vars(base), "lowpass": 900}))
        before_peak = measure_peak(tone)
        after_peak = measure_peak(Path(dull["output"]))
        check(
            "--lowpass attenuates content above the corner",
            after_peak < before_peak - 20.0,
            f"{before_peak:.1f} dB -> {after_peak:.1f} dB",
        )

        # 7. Peak normalisation hits its target.
        quiet = work / "quiet.wav"
        _run(["ffmpeg", "-y", "-v", "error", "-i", str(source), "-af", "volume=-20dB",
              "-c:a", "pcm_s24le", *DETERMINISM_FLAGS, str(quiet)])
        loud = convert(quiet, work / "loud.wav",
                       argparse.Namespace(**{**vars(base), "normalise": -3.0}))
        reached = measure_peak(Path(loud["output"]))
        check(
            "--normalise reaches its target peak",
            abs(reached - (-3.0)) < 0.5,
            f"target -3.0 dB, reached {reached:.2f} dB",
        )

        # 8. Trimming removes leading silence and keeps the signal.
        padded = work / "padded.wav"
        _run(["ffmpeg", "-y", "-v", "error", "-i", str(source),
              "-af", "adelay=500|500,apad=pad_dur=0.5", "-c:a", "pcm_s24le",
              *DETERMINISM_FLAGS, str(padded)])
        trimmed = convert(padded, work / "trimmed.wav",
                          argparse.Namespace(**{**vars(base), "trim": True}))
        check(
            "--trim removes padded silence",
            trimmed["outputFormat"]["duration"] < probe(padded)["duration"] - 0.4,
            f"{probe(padded)['duration']:.3f} s -> {trimmed['outputFormat']['duration']:.3f} s",
        )

    print()
    # --- --max-seconds, and whether the shortened loop still wraps -------------------------------
    # §72's audio budget only closes if every ambience loop is cut to 10 s (`HOUSE-00277`: 72 loops
    # carry 136 MB of the 156). Cut with `-t`, the file plays a step discontinuity once per loop,
    # for as long as the room is on screen. The claim is that the crossfade removes it, measured as
    # the distance between the last sample and the first -- which is what the speaker hears at the
    # wrap.
    with tempfile.TemporaryDirectory(prefix="convert_audio_loop_") as tmp:
        work = Path(tmp)
        source = work / "sweep.wav"
        # A SWEEP, so the head and the tail are genuinely different: a steady tone wraps seamlessly
        # however it is cut and would prove nothing. The phase offset matters too -- a sweep
        # starting at phase 0 has a first sample of exactly 0, so "the step at the wrap" would be
        # measured against silence and come out small whatever the tail did. The AMPLITUDE RAMP
        # matters as much: a pure sweep of this length happens to complete a whole number of cycles
        # by t = 3, so source[3] equalled source[0] to four decimals and neither construction could
        # be told from the other.
        _run(["ffmpeg", "-y", "-v", "error", "-f", "lavfi",
              "-i", "aevalsrc=0.4*(1-t/8)*sin(2*PI*(200+120*t)*t+1.1):s=48000:d=6",
              "-c:a", "pcm_s16le", str(source)])

        def edges(path: Path):
            import struct as _struct
            import wave

            with wave.open(str(path), "rb") as handle:
                channels = handle.getnchannels()
                count = handle.getnframes()
                rate = handle.getframerate()
                frames = handle.readframes(count)
            values = _struct.unpack(f"<{len(frames) // 2}h", frames)
            return values[0] / 32768.0, values[-channels] / 32768.0, count / rate

        base = argparse.Namespace(
            require_rate=None, normalise=None, trim=False, trim_threshold=-60.0,
            highpass=None, lowpass=None, mono=False, sample_rate=None,
            max_seconds=None, loop_crossfade=0.0)

        naive = work / "naive.wav"
        convert(source, naive, argparse.Namespace(**{**vars(base), "max_seconds": 3.0}))
        faded = work / "faded.wav"
        convert(source, faded,
                argparse.Namespace(**{**vars(base), "max_seconds": 3.0, "loop_crossfade": 0.25}))

        naive_first, naive_last, naive_seconds = edges(naive)
        faded_first, faded_last, faded_seconds = edges(faded)
        check("--max-seconds shortens the file",
              abs(naive_seconds - 3.0) < 0.02 and abs(faded_seconds - 3.0) < 0.02,
              f"naive {naive_seconds:.3f} s, crossfaded {faded_seconds:.3f} s, from 6.000 s")
        naive_step = abs(naive_last - naive_first)
        faded_step = abs(faded_last - faded_first)
        check("a naive cut leaves a step at the wrap", naive_step > 0.05,
              f"last {naive_last:+.4f} against first {naive_first:+.4f} -- a jump of "
              f"{naive_step:.4f}, which is the click")
        check("the crossfade removes it", faded_step < naive_step / 4,
              f"last {faded_last:+.4f} against first {faded_first:+.4f} -- {faded_step:.4f}, "
              f"{naive_step / max(faded_step, 1e-9):.0f}x smaller")
        check("a crossfade that does not fit is refused",
              _raises(lambda: loop_filter_complex(3.0, 3.0)),
              "a crossfade as long as the clip has nothing to blend with")

    if failures:
        print(f"convert_audio selftest: {len(failures)} FAILED: {', '.join(failures)}",
              file=sys.stderr)
        return 1
    print("convert_audio selftest: all checks passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("source", nargs="?", type=Path)
    parser.add_argument("destination", nargs="?", type=Path)
    parser.add_argument("--probe", type=Path, help="print what a file is and exit")
    parser.add_argument("--selftest", action="store_true", help="prove the tool's own claims")
    parser.add_argument("--mono", action="store_true", help="downmix (Apply3D sources must be mono)")
    parser.add_argument(
        "--sample-rate", type=int,
        help="resample. NOT the default and rarely right: HOUSE-00069 measured 48->44.1 kHz "
             "costing 0.889 dB RMS for no benefit.",
    )
    parser.add_argument(
        "--require-rate", type=int,
        help="fail unless the source is already this rate, instead of resampling it",
    )
    parser.add_argument("--trim", action="store_true", help="remove leading and trailing silence")
    parser.add_argument("--trim-threshold", type=float, default=-60.0, help="dBFS (default -60)")
    parser.add_argument("--normalise", type=float, metavar="DBFS", help="peak-normalise to DBFS")
    parser.add_argument("--highpass", type=float, metavar="HZ", help="remove subsonic rumble")
    parser.add_argument("--lowpass", type=float, metavar="HZ", help='the "dull" variant filter')
    parser.add_argument("--max-seconds", type=float, metavar="S",
                        help="shorten a longer file to S seconds (§72's loop budget)")
    parser.add_argument("--loop-crossfade", type=float, default=0.0, metavar="S",
                        help="with --max-seconds, blend the tail into the head over S seconds so "
                             "the shortened loop still wraps without a click")
    parser.add_argument("--json", action="store_true", help="print the manifest fields as JSON")
    args = parser.parse_args()

    for tool in ("ffmpeg", "ffprobe"):
        if shutil.which(tool) is None:
            print(f"convert_audio: {tool} is not on PATH", file=sys.stderr)
            return 2

    if args.selftest:
        return selftest()

    try:
        if args.probe:
            print(json.dumps(probe(args.probe), indent=2))
            return 0
        if not args.source or not args.destination:
            parser.error("give SOURCE and DESTINATION, or --probe FILE, or --selftest")
        report = convert(args.source, args.destination, args)
    except ConversionError as error:
        print(f"convert_audio: {error}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(report, indent=2))
    else:
        source_format = report["sourceFormat"]
        output_format = report["outputFormat"]
        print(f"convert_audio: {report['source']} -> {report['output']}")
        print(
            f"  {source_format['codec']} {source_format['sampleRate']} Hz "
            f"{source_format['channels']}ch {source_format['sampleFmt']}"
            f"  ->  {output_format['codec']} {output_format['sampleRate']} Hz "
            f"{output_format['channels']}ch {output_format['sampleFmt']}"
        )
        if report["appliedGainDb"] is not None:
            print(f"  gain {report['appliedGainDb']:+.2f} dB")
        # Both hashes, because the manifest row needs both (`cna-house.md` §20.3, `HOUSE-00279`).
        print(f"  sourceSha256 {report['sourceSha256']}")
        print(f"  outputSha256 {report['outputSha256']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
