#!/usr/bin/env python3
"""reverb_variants.py -- the dry / small-room / large-hard variants for a transient sound.

`HOUSE-00220`. `cna-house.md` §64.7: **CNA provides no convolution and no algorithmic reverb**, so
each acoustic profile instead selects a pre-reverberated variant of the thirty loudest transient
sounds -- door slams, the flush, dropped objects, the dog's bark -- and the cell's `reverbHint`
picks one. It is an old technique, it is cheap, and in a house, where the interesting rooms are
small, it is convincing.

## What this is and is not

It is an **application-level acoustic approximation**, not a claim about room acoustics. No room is
measured, no impulse response is convolved, and the two presets are tap patterns chosen to sound
like a small hard room and a large hard room. What IS measured, on every file the tool writes, is
what those presets did to it: the tail length, the peak, the level change and the ratio between
early and late energy. A preset nobody measures drifts; a preset whose numbers are in the manifest
does not.

## The dry variant is the source, byte for byte

`dry` is a **copy**, not a re-encode. §64.7 cross-fades nothing here -- the profile selects one of
the three -- so the dry version must be exactly the sound everything else already refers to. A
tool that re-encoded it would produce a file that differs from its own source by a dither's worth
of noise, for nothing.

## Clipping

Reverb adds energy, so a sound already peaking near full scale will clip once tails are summed onto
it. Rather than let that happen or silently ride a limiter over it, the tool applies **one fixed
attenuation before the taps and reports it** -- computed from the preset's own total tap gain, so
it is a property of the preset and not of the file. The result is measured afterwards and a variant
that still clips is a refusal, not a warning.

    tools/assets/reverb_variants.py Audio/**/door_slam.wav --out assets-src/Audio/Reverb
    tools/assets/reverb_variants.py --selftest

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

#: `HOUSE-00193`'s flags, for the reason it measured: ffmpeg's default output carries a `LIST`
#: chunk and an `ISFT` tag naming the libavformat version.
DETERMINISM_FLAGS = ("-map_metadata", "-1", "-fflags", "+bitexact")

#: The three variants §64.7 names, and the tap pattern each one is.
#:
#: `aecho=in_gain:out_gain:delays:decays` is the only reverberator CNA's toolchain has to hand, and
#: a handful of taps is genuinely what a small hard room sounds like: a few strong early
#: reflections and nothing else. The large preset adds later, quieter taps at prime-ish spacings so
#: they do not reinforce each other into a flutter.
#:
#: Delays are milliseconds. A first reflection at 17 ms is a surface about 3 m away, which is a
#: bathroom; at 53 ms it is about 9 m, which is the cinema.
PRESETS = {
    "dry": None,
    "small": {"delays": (11, 17, 23, 31), "decays": (0.38, 0.28, 0.18, 0.11),
              "description": "a small hard room -- tiled bathroom, hall, stairwell"},
    "large": {"delays": (23, 37, 53, 71, 97), "decays": (0.42, 0.32, 0.24, 0.16, 0.10),
              "description": "a large hard room -- the cinema, the garage, the empty basement"},
}

#: A variant may not peak above this. -0.5 dBFS rather than 0: a `SoundEffect` played at a volume
#: of 1.0 through a mixer that sums voices needs somewhere to go.
PEAK_CEILING_DBFS = -0.5


class ReverbError(RuntimeError):
    pass


def _run(command: list[str]) -> subprocess.CompletedProcess:
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise ReverbError(f"{command[0]} failed:\n  {' '.join(command)}\n{result.stderr.strip()}")
    return result


def preset_gain(preset: dict) -> float:
    """The attenuation to apply before the taps, from the preset's own arithmetic.

    Worst case, every tap lines up with the direct sound, so the summed amplitude is
    `1 + Σ decays`. Attenuating by its reciprocal makes that worst case reach exactly full scale
    rather than past it -- and, being a property of the PRESET, gives every sound the same
    relationship between its dry and reverberated versions. A per-file normalisation would not:
    two sounds mixed at the same level dry would come back at different levels wet.
    """
    return 1.0 / (1.0 + sum(preset["decays"]))


def filter_chain(preset: dict) -> str:
    gain = preset_gain(preset)
    delays = "|".join(str(d) for d in preset["delays"])
    decays = "|".join(f"{d:g}" for d in preset["decays"])
    # `in_gain` 1 and `out_gain` 1: the headroom is taken once, in front, where it is one number
    # that can be reported. Folding it into `out_gain` would attenuate the tails twice.
    return f"volume={gain:.6f},aecho=1:1:{delays}:{decays}"


def variant_path(source: Path, name: str, out: Path | None) -> Path:
    directory = out if out is not None else source.parent
    return directory / f"{source.stem}_{name}{source.suffix}"


def make_variant(source: Path, name: str, destination: Path) -> dict:
    preset = PRESETS[name]
    original = audio_probe.read_wav(source)
    destination.parent.mkdir(parents=True, exist_ok=True)

    if preset is None:
        # A COPY. See the module docstring: the dry variant must be exactly the sound everything
        # else refers to, and a re-encode would differ from its own source for nothing.
        shutil.copyfile(source, destination)
        gain = 1.0
    else:
        gain = preset_gain(preset)
        _run(["ffmpeg", "-y", "-v", "error", "-i", str(source),
              "-af", filter_chain(preset),
              "-ar", str(original["rate"]), "-ac", str(original["channels"]), "-c:a", "pcm_s16le",
              *DETERMINISM_FLAGS, str(destination)])

    result = audio_probe.read_wav(destination)
    mono = result["samples"][0]
    peak = audio_probe.peak_dbfs(mono)
    problems = []
    if result["rate"] != original["rate"] or result["channels"] != original["channels"]:
        problems.append(f"the format changed from {original['rate']} Hz/{original['channels']} ch "
                        f"to {result['rate']} Hz/{result['channels']} ch")
    if peak > PEAK_CEILING_DBFS:
        problems.append(f"it peaks at {peak:.2f} dBFS, above the {PEAK_CEILING_DBFS} dBFS ceiling")
    if preset is not None and result["frames"] <= original["frames"]:
        problems.append(f"it is {result['frames']} frames against the source's "
                        f"{original['frames']}: a reverberated variant must be LONGER, and one "
                        f"that is not has no tail")
    if problems:
        raise ReverbError(f"{destination.name}: " + "; ".join(problems))

    dry_tail = audio_probe.decay_seconds(original["samples"][0], original["rate"])
    return {
        "source": source.name,
        "variant": name,
        "file": destination.name,
        "description": "the source, copied byte for byte" if preset is None
        else preset["description"],
        "preAttenuationDb": round(20.0 * math.log10(gain), 3) if gain > 0 else None,
        "delaysMs": list(preset["delays"]) if preset else [],
        "decays": list(preset["decays"]) if preset else [],
        "seconds": round(result["frames"] / result["rate"], 6),
        "addedSeconds": round((result["frames"] - original["frames"]) / result["rate"], 6),
        # The tail this file actually has, measured to -60 dB of its own peak. This is the number
        # §64.7's presets are chosen by and the number a later reader will want.
        "tailSeconds": round(audio_probe.decay_seconds(mono, result["rate"]), 6),
        "sourceTailSeconds": round(dry_tail, 6),
        "peakDbfs": round(peak, 3),
        "rmsDbfs": round(audio_probe.rms_dbfs(mono), 3),
        "sourcePeakDbfs": round(audio_probe.peak_dbfs(original["samples"][0]), 3),
        "sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
    }


def make_all(source: Path, out: Path | None) -> list[dict]:
    return [make_variant(source, name, variant_path(source, name, out)) for name in PRESETS]


# ----------------------------------------------------------------------------------- selftest ----

def _impulse(path: Path, rate: int = 44100, seconds: float = 0.5) -> None:
    """One click, then silence. The tail of an impulse IS the preset's impulse response, so this
    is how a tap pattern's real decay is measured rather than inferred from its numbers."""
    samples = [0.0] * int(rate * seconds)
    samples[0] = 0.9
    audio_probe.write_wav(path, [samples], rate)


def _burst(path: Path, rate: int = 44100) -> None:
    """A short loud transient, which is the kind of sound §64.7 applies to: a door slam."""
    count = int(rate * 0.08)
    samples = []
    for index in range(count):
        envelope = math.exp(-index / (rate * 0.012))
        samples.append(0.85 * envelope * math.sin(2.0 * math.pi * 220.0 * index / rate))
    samples += [0.0] * int(rate * 0.4)
    audio_probe.write_wav(path, [samples], rate)


def selftest() -> int:
    global PEAK_CEILING_DBFS
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

    workspace = Path(tempfile.mkdtemp(prefix="reverb_variants_selftest_"))
    try:
        source = workspace / "slam.wav"
        _burst(source)
        records = {record["variant"]: record for record in make_all(source, workspace / "out")}

        # 1. Three variants, exactly the three §64.7 names.
        require(sorted(records) == ["dry", "large", "small"],
                f"the three variants are {sorted(records)}")

        # 2. THE claim about the dry one: it is the source, byte for byte. Not "very close".
        dry = workspace / "out" / "slam_dry.wav"
        require(dry.read_bytes() == source.read_bytes(),
                "the dry variant is the source copied byte for byte, not re-encoded")

        # 3. Each preset adds a tail, and large adds more than small. Measured from the file, not
        #    read off the preset's own delay numbers.
        require(records["small"]["tailSeconds"] > records["dry"]["tailSeconds"],
                f"small adds a tail: {records['dry']['tailSeconds']:.3f} s dry -> "
                f"{records['small']['tailSeconds']:.3f} s")
        require(records["large"]["tailSeconds"] > records["small"]["tailSeconds"],
                f"large's tail is longer than small's "
                f"({records['large']['tailSeconds']:.3f} s against "
                f"{records['small']['tailSeconds']:.3f} s)")

        # 4. The IMPULSE response, which is what the tail really is. An impulse in, and the
        #    measured decay must match the preset's own last tap -- the arithmetic and the audio
        #    agreeing is what says the filter did what the numbers describe.
        impulse = workspace / "impulse.wav"
        _impulse(impulse)
        for name in ("small", "large"):
            response = workspace / f"ir_{name}.wav"
            make_variant(impulse, name, response)
            wav = audio_probe.read_wav(response)
            measured = audio_probe.decay_seconds(wav["samples"][0], wav["rate"])
            expected = max(PRESETS[name]["delays"]) / 1000.0
            require(abs(measured - expected) < 0.005,
                    f"{name}'s impulse response ends at {measured * 1000:.1f} ms, its last tap at "
                    f"{expected * 1000:.0f} ms")

        # 5. Nothing clips.
        for name, record in records.items():
            require(record["peakDbfs"] <= PEAK_CEILING_DBFS,
                    f"{name} peaks at {record['peakDbfs']:.2f} dBFS, inside the "
                    f"{PEAK_CEILING_DBFS} dBFS ceiling")

        # 5b. The pre-attenuation is load-bearing -- but MEASURED, and the measurement corrects the
        #     obvious assumption. A fast transient's taps land 11 ms or more after its peak, by
        #     which time the direct sound has decayed, so they never sum with it: the door slam
        #     above comes back at -2.2 dBFS with or without the attenuation. It is a SUSTAINED
        #     sound -- a bark's body, a flush, a motor -- whose taps land on top of the signal
        #     still playing, and that is the case this claim has to use.
        sustained = workspace / "sustained.wav"
        rate = 44100
        count = int(rate * 0.3)
        audio_probe.write_wav(sustained, [[0.89 * math.sin(2.0 * math.pi * 180.0 * i / rate)
                                           for i in range(count)]], rate)
        preset = PRESETS["large"]
        unattenuated = workspace / "unattenuated.wav"
        _run(["ffmpeg", "-y", "-v", "error", "-i", str(sustained), "-af",
              f"aecho=1:1:{'|'.join(str(d) for d in preset['delays'])}:"
              f"{'|'.join(f'{d:g}' for d in preset['decays'])}",
              "-c:a", "pcm_s16le", str(unattenuated)])
        raw_peak = audio_probe.peak_dbfs(audio_probe.read_wav(unattenuated)["samples"][0])
        require(raw_peak > -0.2,
                f"a SUSTAINED source without the pre-attenuation reaches {raw_peak:.2f} dBFS -- "
                f"it is clipping")
        attenuated = make_variant(sustained, "large", workspace / "attenuated.wav")
        require(attenuated["peakDbfs"] <= PEAK_CEILING_DBFS,
                f"...and with it, {attenuated['peakDbfs']:.2f} dBFS, so the attenuation is "
                f"load-bearing for exactly the sounds that need it")

        # 6. The attenuation is a property of the PRESET, not of the file. Two sources at different
        #    levels must keep their relative levels, or a mix balanced dry comes back unbalanced.
        quiet = workspace / "quiet.wav"
        wav = audio_probe.read_wav(source)
        audio_probe.write_wav(quiet, [[v * 0.5 for v in wav["samples"][0]]], wav["rate"])
        quiet_record = make_variant(quiet, "large", workspace / "out" / "quiet_large.wav")
        difference = records["large"]["peakDbfs"] - quiet_record["peakDbfs"]
        require(abs(difference - 6.02) < 0.3,
                f"halving the source halves the variant ({difference:.2f} dB apart), so the gain "
                f"is the preset's and not a per-file normalisation")

        # 7. Determinism, and the naive command's version stamp.
        again = workspace / "again.wav"
        make_variant(source, "small", again)
        require(again.read_bytes() == (workspace / "out" / "slam_small.wav").read_bytes(),
                "two runs produce byte-identical output")
        naive = workspace / "naive.wav"
        _run(["ffmpeg", "-y", "-v", "error", "-i", str(source), "-af",
              filter_chain(PRESETS["small"]), "-c:a", "pcm_s16le", str(naive)])
        require(b"ISFT" in naive.read_bytes()
                and b"ISFT" not in (workspace / "out" / "slam_small.wav").read_bytes(),
                "the naive command stamps the libavformat version in and the flags used here "
                "remove it")

        # 8. Format is preserved. A variant at a different rate would be a different sound.
        for name, record in records.items():
            variant = audio_probe.read_wav(workspace / "out" / record["file"])
            require(variant["rate"] == wav["rate"] and variant["channels"] == wav["channels"],
                    f"{name} keeps {variant['rate']} Hz / {variant['channels']} ch")

        # 9. A variant that clips is a REFUSAL, not a warning. Forced by lowering the ceiling
        #    below what any real file can meet.
        ceiling = PEAK_CEILING_DBFS
        PEAK_CEILING_DBFS = -60.0
        try:
            make_variant(source, "small", workspace / "refused.wav")
            require(False, "a variant above the ceiling is refused")
        except ReverbError as error:
            require("ceiling" in str(error), f"a variant above the ceiling is refused: {error}")
        finally:
            PEAK_CEILING_DBFS = ceiling

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("reverb_variants: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("sources", nargs="*", type=Path)
    parser.add_argument("--out", type=Path, help="where the variants are written")
    parser.add_argument("--variant", choices=sorted(PRESETS), action="append", default=[],
                        help="only these variants; all three by default")
    parser.add_argument("--json", type=Path)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if not args.sources:
        parser.print_help()
        return 2

    wanted = args.variant or list(PRESETS)
    records = []
    for source in args.sources:
        for name in wanted:
            try:
                record = make_variant(source, name, variant_path(source, name, args.out))
            except (ReverbError, audio_probe.WavError) as error:
                print(f"reverb_variants: {error}", file=sys.stderr)
                return 1
            records.append(record)
            print(f"{record['file']:<36} {record['seconds']:6.3f} s "
                  f"(+{record['addedSeconds']:.3f})  tail {record['tailSeconds']:5.3f} s  "
                  f"peak {record['peakDbfs']:6.2f} dBFS  {record['description']}")
    if args.json is not None:
        args.json.write_text(json.dumps(records, indent=2, sort_keys=True) + "\n",
                             encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
