#!/usr/bin/env python3
"""audio_probe.py -- read a WAV and measure it, so an audio tool's claims can be checked.

`HOUSE-00220`/`HOUSE-00221`. The two derivative-audio tools make claims a listener cannot verify in
a build log — "a 4th-order low-pass at 900 Hz", "the tail is 0.4 s", "it does not clip" — and a
claim nobody measures is a claim that quietly stops being true. This module is how they are
measured: it decodes a 16-bit PCM WAV without ffmpeg, and reports level, per-band energy and decay.

It reads the file itself rather than shelling out to `ffprobe`/`volumedetect`, for two reasons.
`volumedetect` gives one number for the whole file and this needs a number **per frequency**; and a
measurement performed by the same program that applied the filter would be measuring the intention
rather than the bytes.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import math
import struct
import sys
from pathlib import Path


class WavError(Exception):
    """A refusal with a reason a human can act on."""


def read_wav(path: Path) -> dict:
    """A 16-bit PCM WAV as `{rate, channels, bits, samples}`, samples per channel as floats."""
    data = path.read_bytes()
    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise WavError(f"{path.name} is not a RIFF/WAVE file")

    offset = 12
    rate = channels = bits = 0
    frames = b""
    while offset + 8 <= len(data):
        tag = data[offset:offset + 4]
        (size,) = struct.unpack_from("<I", data, offset + 4)
        body = offset + 8
        if tag == b"fmt " and size >= 16:
            _, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", data, body)
        elif tag == b"data":
            frames = data[body:body + min(size, len(data) - body)]
        offset = body + size + (size & 1)

    # Checked in this order: a file with no `fmt ` chunk at all reports THAT, rather than
    # reporting "0-bit", which describes the symptom and not the problem.
    if rate <= 0 or channels <= 0:
        raise WavError(f"{path.name} has no usable 'fmt ' chunk")
    if bits != 16:
        raise WavError(f"{path.name} is {bits}-bit; this reader handles 16-bit PCM "
                       f"(the runtime format, HOUSE-00069)")

    count = len(frames) // 2
    flat = struct.unpack_from(f"<{count}h", frames, 0)
    per_channel = [[flat[i] / 32768.0 for i in range(channel, count, channels)]
                   for channel in range(channels)]
    return {"rate": rate, "channels": channels, "bits": bits, "samples": per_channel,
            "frames": count // channels}


def peak_dbfs(samples: list[float]) -> float:
    top = max((abs(v) for v in samples), default=0.0)
    return -math.inf if top <= 0 else 20.0 * math.log10(top)


def rms_dbfs(samples: list[float]) -> float:
    if not samples:
        return -math.inf
    energy = sum(v * v for v in samples) / len(samples)
    return -math.inf if energy <= 0 else 10.0 * math.log10(energy)


def tone_level_dbfs(samples: list[float], rate: int, frequency: float,
                    start: float = 0.0, seconds: float | None = None) -> float:
    """The level of ONE frequency, by correlating against it (a single-bin Goertzel).

    A whole-file RMS cannot answer "how much did 1 800 Hz drop", which is the only question that
    can tell a 2nd-order filter from a 4th-order one. Correlating against the exact tone needs no
    FFT, no window choice and no bin alignment -- and the fixture's tones are exact by construction,
    so there is nothing to leak.
    """
    begin = int(start * rate)
    end = len(samples) if seconds is None else min(len(samples), begin + int(seconds * rate))
    if end - begin < 2:
        return -math.inf
    window = samples[begin:end]
    real = imaginary = 0.0
    for index, value in enumerate(window):
        phase = 2.0 * math.pi * frequency * index / rate
        real += value * math.cos(phase)
        imaginary += value * math.sin(phase)
    amplitude = 2.0 * math.hypot(real, imaginary) / len(window)
    return -math.inf if amplitude <= 0 else 20.0 * math.log10(amplitude)


def decay_seconds(samples: list[float], rate: int, floor_db: float = -60.0) -> float:
    """How long until the signal's envelope stays below `floor_db` of its own peak.

    Measured from the LAST sample above the floor, not from the first below it: a decaying tail
    crosses the threshold many times on its way down, and taking the first crossing would report a
    fraction of the real tail.
    """
    top = max((abs(v) for v in samples), default=0.0)
    if top <= 0:
        return 0.0
    floor = top * (10.0 ** (floor_db / 20.0))
    last = -1
    for index in range(len(samples) - 1, -1, -1):
        if abs(samples[index]) > floor:
            last = index
            break
    return 0.0 if last < 0 else (last + 1) / rate


def measure(path: Path) -> dict:
    """Format, duration, peak and RMS for a PCM WAV of 16, 24 or 32 bits, via numpy.

    `read_wav` converts every sample to a Python float, which is right for a fixture of a few
    thousand samples and hopeless for a library: measuring 1 634 files that way, or by running
    `ffmpeg -af volumedetect` on each, takes eighteen minutes. This reads the `data` chunk once
    into a numpy array and is bounded by disk. 24-bit is assembled from its three bytes explicitly,
    because numpy has no 24-bit integer type and a naive `frombuffer` silently misreads it.
    """
    import numpy as np

    data = path.read_bytes()
    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise WavError(f"{path.name} is not a RIFF/WAVE file")

    offset = 12
    rate = channels = bits = 0
    body = size = 0
    while offset + 8 <= len(data):
        tag = data[offset:offset + 4]
        (chunk,) = struct.unpack_from("<I", data, offset + 4)
        start = offset + 8
        if tag == b"fmt " and chunk >= 16:
            _, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", data, start)
        elif tag == b"data":
            body, size = start, min(chunk, len(data) - start)
        offset = start + chunk + (chunk & 1)

    if rate <= 0 or channels <= 0:
        raise WavError(f"{path.name} has no usable 'fmt ' chunk")
    if bits not in (16, 24, 32):
        raise WavError(f"{path.name} is {bits}-bit; this reader handles 16, 24 and 32")

    width = bits // 8
    frames = size // (width * channels)
    raw = np.frombuffer(data, dtype=np.uint8, count=frames * channels * width, offset=body)
    if bits == 16:
        samples = raw.view(np.int16).astype(np.float64) / 32768.0
    elif bits == 32:
        samples = raw.view(np.int32).astype(np.float64) / 2147483648.0
    else:
        triples = raw.reshape(-1, 3).astype(np.int32)
        packed = triples[:, 0] | (triples[:, 1] << 8) | (triples[:, 2] << 16)
        # Sign-extend from 24 bits. Without this every negative sample reads as a large positive
        # one and the peak comes back at exactly 0 dBFS for every file in the library.
        packed = np.where(packed & 0x800000, packed - 0x1000000, packed)
        samples = packed.astype(np.float64) / 8388608.0

    peak = float(np.max(np.abs(samples))) if samples.size else 0.0
    energy = float(np.mean(samples * samples)) if samples.size else 0.0
    return {"rate": rate, "channels": channels, "bits": bits, "frames": frames,
            "seconds": frames / rate if rate else 0.0,
            "peakDbfs": -math.inf if peak <= 0 else 20.0 * math.log10(peak),
            "rmsDbfs": -math.inf if energy <= 0 else 10.0 * math.log10(energy),
            "bytes": len(data)}


def write_wav(path: Path, channels: list[list[float]], rate: int) -> None:
    """16-bit PCM, for the fixtures. Values outside [-1, 1) are an error, not a wrap."""
    count = len(channels[0])
    payload = bytearray()
    for index in range(count):
        for channel in channels:
            value = channel[index]
            if not -1.0 <= value < 1.0:
                raise WavError(f"sample {index} is {value}, outside [-1, 1); writing it would "
                               f"wrap rather than clip")
            payload += struct.pack("<h", int(round(value * 32767.0)))
    header = (b"RIFF" + struct.pack("<I", 36 + len(payload)) + b"WAVE"
              + b"fmt " + struct.pack("<IHHIIHH", 16, 1, len(channels), rate,
                                      rate * len(channels) * 2, len(channels) * 2, 16)
              + b"data" + struct.pack("<I", len(payload)))
    path.write_bytes(header + bytes(payload))


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: audio_probe.py <file.wav> ...", file=sys.stderr)
        return 2
    for argument in sys.argv[1:]:
        path = Path(argument)
        try:
            wav = read_wav(path)
        except WavError as error:
            print(f"audio_probe: {error}", file=sys.stderr)
            return 1
        mono = wav["samples"][0]
        print(f"{path.name}: {wav['rate']} Hz, {wav['channels']} ch, "
              f"{wav['frames'] / wav['rate']:.3f} s, "
              f"peak {peak_dbfs(mono):.2f} dBFS, rms {rms_dbfs(mono):.2f} dBFS, "
              f"tail to -60 dB {decay_seconds(mono, wav['rate']):.3f} s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
