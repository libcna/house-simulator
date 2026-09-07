#!/usr/bin/env python3
"""video_transcode.py -- source footage to the runtime video, and to the frame-strip atlases.

`HOUSE-00219`. `cna-house.md` §58.3 gives television two backends behind one `ITvSource`, and this
tool produces the content for both:

* **`VideoTvSource`** — a `Video` the FFmpeg backend decodes. Theora in Ogg, which is what
  `HOUSE-00098` measured travelling through `VideoImporter` → `.cnb` → `Load<Video>` →
  `VideoPlayer::GetTexture`, and what `HOUSE-00201` shipped for the smoke scene.
* **`SequenceTvSource`** — Emscripten, Android, and any build without `CNA_VIDEO_AVAILABLE`
  (`BL-05`). A pre-baked frame-strip atlas: 8 × 8 frames of 256 × 144 at 12 fps, 5.33 s per atlas,
  N atlases per channel, advanced by a timer, with the audio as a separate looping `SoundEffect`.

§58.3 calls that atlas "2048²" and it is **2048 × 1152**, which is the smallest correction that
makes the arithmetic true: 8 × 256 is 2048 and 8 × 144 is 1152. Rounding the height up to 2048
would leave 896 rows — **44 % of the texture, 7.3 MB of the 16.8 MB** — holding nothing, per atlas,
in a 25 MB pack. OpenGL ES 3.0 requires non-power-of-two support for a texture with no mip chain
and clamped addressing, which is exactly what a frame strip is; `--square` restores the padded
layout for a profile that turns out to need it, and reports what it costs.

## What is and is not deterministic, measured rather than hoped

* The **atlases are byte-identical** run to run. They are assembled and written by this project's
  own PNG encoder (`atlas_pack.png_encode`), so the bytes do not depend on which Pillow is
  installed — only the frame extraction goes through ffmpeg, and it is pixel-deterministic.
* The **audio track is byte-identical**, with `HOUSE-00193`'s flags.
* The **`.ogv` is not**, across ffmpeg builds: Theora's bitstream depends on the encoder. It is
  deterministic for a given ffmpeg, which the selftest checks, and the tool says so rather than
  claiming more. That is the same finding `make_smoke_assets.py` recorded.

    tools/assets/video_transcode.py source.mp4 --name broadcast_01 --out assets-src
    tools/assets/video_transcode.py source.mp4 --name x --out <dir> --no-strip
    tools/assets/video_transcode.py --make-fixture <dir>
    tools/assets/video_transcode.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import atlas_pack  # noqa: E402
import audio_probe  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

#: §58.3's frame-strip geometry.
FRAME_WIDTH = 256
FRAME_HEIGHT = 144
GRID_COLUMNS = 8
GRID_ROWS = 8
FRAMES_PER_ATLAS = GRID_COLUMNS * GRID_ROWS
STRIP_FPS = 12

#: The runtime video. 512 × 288 is 16:9 and is what a 55" screen seen from 3 m in a game needs;
#: `--video-width` changes it. Theora because that is the route `HOUSE-00098` measured end to end.
VIDEO_WIDTH = 512
VIDEO_HEIGHT = 288
VIDEO_FPS = 24
VIDEO_QUALITY = 6
AUDIO_QUALITY = 3

#: `HOUSE-00193`'s determinism flags, and `-flags +bitexact` for the encoder as well as the muxer.
BITEXACT = ("-fflags", "+bitexact", "-flags", "+bitexact", "-map_metadata", "-1")


class TranscodeError(RuntimeError):
    pass


def _run(command: list[str]) -> subprocess.CompletedProcess:
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise TranscodeError(f"{command[0]} failed:\n  {' '.join(command)}\n"
                             f"{result.stderr.strip()}")
    return result


def probe(path: Path) -> dict:
    if shutil.which("ffprobe") is None:
        raise TranscodeError("ffprobe is required to read the source's metadata")
    result = _run(["ffprobe", "-v", "error", "-show_entries",
                   "format=duration:stream=codec_type,codec_name,width,height,r_frame_rate",
                   "-of", "json", str(path)])
    parsed = json.loads(result.stdout)
    video = next((s for s in parsed.get("streams", []) if s.get("codec_type") == "video"), None)
    audio = next((s for s in parsed.get("streams", []) if s.get("codec_type") == "audio"), None)
    if video is None:
        raise TranscodeError(f"{path.name} has no video stream")
    numerator, _, denominator = video.get("r_frame_rate", "0/1").partition("/")
    rate = float(numerator) / float(denominator) if float(denominator or 0) else 0.0
    return {"seconds": float(parsed.get("format", {}).get("duration", 0.0)),
            "width": int(video["width"]), "height": int(video["height"]),
            "fps": rate, "codec": video.get("codec_name", ""),
            "hasAudio": audio is not None,
            "audioCodec": audio.get("codec_name", "") if audio else ""}


# ------------------------------------------------------------------------------ runtime video ----

def transcode(source: Path, destination: Path, width: int = VIDEO_WIDTH,
              height: int = VIDEO_HEIGHT, fps: int = VIDEO_FPS) -> dict:
    destination.parent.mkdir(parents=True, exist_ok=True)
    command = ["ffmpeg", "-y", "-v", "error", "-i", str(source),
               "-vf", f"fps={fps},scale={width}:{height}:flags=bicubic",
               "-c:v", "libtheora", "-q:v", str(VIDEO_QUALITY), "-pix_fmt", "yuv420p"]
    if probe(source)["hasAudio"]:
        command += ["-c:a", "libvorbis", "-q:a", str(AUDIO_QUALITY)]
    else:
        command += ["-an"]
    command += [*BITEXACT, str(destination)]
    _run(command)

    measured = probe(destination)
    problems = []
    if (measured["width"], measured["height"]) != (width, height):
        problems.append(f"it is {measured['width']}x{measured['height']}, not {width}x{height}")
    if abs(measured["fps"] - fps) > 0.01:
        problems.append(f"its frame rate is {measured['fps']}, not {fps}")
    if problems:
        raise TranscodeError(f"{destination.name}: " + "; ".join(problems))

    return {
        "file": destination.name,
        "codec": measured["codec"],
        "width": width, "height": height, "fps": fps,
        "seconds": round(measured["seconds"], 3),
        "bytes": destination.stat().st_size,
        "sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
        # `VideoProcessor` REQUIRES these as authored parameters -- CNA does not decode the source
        # at build time, it deploys the stream and trusts the metadata (`HOUSE-00201`). Emitting
        # them here is what stops the two disagreeing.
        "contentConfig": {
            "width": {"type": "u64", "value": str(width)},
            "height": {"type": "u64", "value": str(height)},
            "framesPerSecond": {"type": "f64", "value": f"{float(fps)}"},
            "durationMs": {"type": "u64", "value": str(int(round(measured["seconds"] * 1000)))},
        },
    }


# ------------------------------------------------------------------------------- frame strips ----

def strip_size(square: bool) -> tuple[int, int]:
    width = FRAME_WIDTH * GRID_COLUMNS
    height = FRAME_HEIGHT * GRID_ROWS
    return (width, max(width, height)) if square else (width, height)


def extract_frames(source: Path, workspace: Path, fps: int = STRIP_FPS) -> list[Path]:
    workspace.mkdir(parents=True, exist_ok=True)
    _run(["ffmpeg", "-y", "-v", "error", "-i", str(source),
          "-vf", f"fps={fps},scale={FRAME_WIDTH}:{FRAME_HEIGHT}:flags=bicubic",
          "-fps_mode", "cfr", *BITEXACT, str(workspace / "frame%05d.png")])
    frames = sorted(workspace.glob("frame*.png"))
    if not frames:
        raise TranscodeError(f"{source.name}: no frames were extracted")
    return frames


def build_strips(source: Path, out: Path, name: str, square: bool = False,
                 fps: int = STRIP_FPS) -> dict:
    width, height = strip_size(square)
    out.mkdir(parents=True, exist_ok=True)
    workspace = Path(tempfile.mkdtemp(prefix="video_strip_"))
    try:
        frames = extract_frames(source, workspace, fps)
        atlases = []
        for index in range(0, len(frames), FRAMES_PER_ATLAS):
            block = frames[index:index + FRAMES_PER_ATLAS]
            # The unused tail of the last atlas is BLACK, not the previous frame repeated: a
            # television that runs out of frames should go dark rather than freeze on one, and the
            # metadata's frame count is what stops the timer reaching them at all.
            pixels = bytearray(bytes((0, 0, 0, 255)) * (width * height))
            for slot, frame in enumerate(block):
                fw, fh, texels = atlas_pack.png_decode(frame.read_bytes())
                if (fw, fh) != (FRAME_WIDTH, FRAME_HEIGHT):
                    raise TranscodeError(f"{frame.name} is {fw}x{fh}, not "
                                         f"{FRAME_WIDTH}x{FRAME_HEIGHT}")
                x0 = (slot % GRID_COLUMNS) * FRAME_WIDTH
                y0 = (slot // GRID_COLUMNS) * FRAME_HEIGHT
                for row in range(FRAME_HEIGHT):
                    target = ((y0 + row) * width + x0) * 4
                    source_row = row * FRAME_WIDTH * 4
                    pixels[target:target + FRAME_WIDTH * 4] = \
                        texels[source_row:source_row + FRAME_WIDTH * 4]

            data = atlas_pack.png_encode(width, height, bytes(pixels))
            target = out / f"{name}_{len(atlases):02d}.png"
            target.write_bytes(data)
            atlases.append({"file": target.name, "frames": len(block),
                            "firstFrame": index,
                            "sha256": hashlib.sha256(data).hexdigest(),
                            "bytes": len(data)})
        return {"atlases": atlases, "frameCount": len(frames),
                "width": width, "height": height}
    finally:
        shutil.rmtree(workspace, ignore_errors=True)


def extract_audio(source: Path, destination: Path) -> dict | None:
    """The strip backend's separately-streamed track, as a looping `SoundEffect`.

    16-bit PCM, at the source's own rate: `HOUSE-00069` measured that resampling costs 0.889 dB RMS
    for a CPU argument this offline step does not have to make.
    """
    if not probe(source)["hasAudio"]:
        return None
    destination.parent.mkdir(parents=True, exist_ok=True)
    _run(["ffmpeg", "-y", "-v", "error", "-i", str(source), "-vn",
          "-c:a", "pcm_s16le", *BITEXACT, str(destination)])
    wav = audio_probe.read_wav(destination)
    return {"file": destination.name, "rate": wav["rate"], "channels": wav["channels"],
            "seconds": round(wav["frames"] / wav["rate"], 6),
            "peakDbfs": round(audio_probe.peak_dbfs(wav["samples"][0]), 3),
            "bytes": destination.stat().st_size,
            "sha256": hashlib.sha256(destination.read_bytes()).hexdigest()}


def convert(source: Path, out: Path, name: str, *, strips: bool = True, video: bool = True,
            square: bool = False) -> dict:
    source_info = probe(source)
    report: dict = {
        "tool": "tools/assets/video_transcode.py",
        "formatVersion": 1,
        "name": name,
        "source": {"file": source.name, "codec": source_info["codec"],
                   "width": source_info["width"], "height": source_info["height"],
                   "fps": round(source_info["fps"], 4),
                   "seconds": round(source_info["seconds"], 3),
                   "hasAudio": source_info["hasAudio"],
                   "sha256": hashlib.sha256(source.read_bytes()).hexdigest()},
        "warnings": [],
    }

    if video:
        report["video"] = transcode(source, out / "Media" / "Video" / f"{name}.ogv")
        report["video"]["contentName"] = f"Video/{name}"

    if strips:
        built = build_strips(source, out / "Textures" / "TV", name, square)
        audio = extract_audio(source, out / "Audio" / "TV" / f"{name}.wav")
        seconds_per_atlas = FRAMES_PER_ATLAS / STRIP_FPS
        report["strip"] = {
            **built,
            "fps": STRIP_FPS,
            "frameWidth": FRAME_WIDTH, "frameHeight": FRAME_HEIGHT,
            "columns": GRID_COLUMNS, "rows": GRID_ROWS,
            "framesPerAtlas": FRAMES_PER_ATLAS,
            "secondsPerAtlas": round(seconds_per_atlas, 6),
            "totalSeconds": round(built["frameCount"] / STRIP_FPS, 6),
            "square": square,
            "audio": audio,
            "contentNames": [f"Textures/TV/{a['file'][:-4]}" for a in built["atlases"]],
        }
        if audio is None:
            report["warnings"].append("the source has no audio track, so the strip backend has "
                                      "no soundtrack to keep in sync")
        elif abs(audio["seconds"] - report["strip"]["totalSeconds"]) > 0.5:
            # `SequenceTvSource` keeps the two in step with ONE timer, so a track that is half a
            # second out drifts visibly against the picture within a minute.
            report["warnings"].append(
                f"the audio is {audio['seconds']:.3f} s against the strip's "
                f"{report['strip']['totalSeconds']:.3f} s; one timer advances both and they will "
                f"drift")
    return report


# ------------------------------------------------------------------------------------ fixture ----

def make_fixture(path: Path, seconds: int = 6) -> Path:
    """A source clip whose every frame is identifiable, so a strip can be checked frame by frame.

    Each frame carries a black bar whose LENGTH is its own index, in a band at the top, and a
    background that steps through four colours. Between them, a decoded atlas cell says exactly
    which source frame it holds -- which is the only way to check that frame 64 begins the second
    atlas rather than being dropped or duplicated.

    Authored as PNGs rather than an `lavfi` graph for the reason `HOUSE-00201` measured: `libtheora`
    drops a frame identical to the one before it, and `drawbox` in this ffmpeg has no per-frame
    `eval` with which to move anything.
    """
    path.parent.mkdir(parents=True, exist_ok=True)
    workspace = Path(tempfile.mkdtemp(prefix="video_fixture_"))
    try:
        colours = ((190, 40, 40), (40, 180, 60), (50, 90, 210), (220, 200, 60))
        total = seconds * STRIP_FPS
        for index in range(total):
            background = colours[(index // STRIP_FPS) % len(colours)] + (255,)

            def pixel(x: int, y: int, index=index, background=background):
                # The index bar: `index + 1` pixels wide, in the top eight rows. Readable from a
                # decoded atlas cell by counting, with no OCR and no ambiguity.
                if y < 8 and x <= index:
                    return (0, 0, 0, 255)
                return background

            atlas_pack_png = atlas_pack.png_encode(FRAME_WIDTH, FRAME_HEIGHT,
                                                   b"".join(bytes(pixel(x, y))
                                                            for y in range(FRAME_HEIGHT)
                                                            for x in range(FRAME_WIDTH)))
            (workspace / f"f{index:05d}.png").write_bytes(atlas_pack_png)

        tone = workspace / "tone.wav"
        rate = 44100
        count = rate * seconds
        audio_probe.write_wav(tone, [[0.3 * math.sin(2.0 * math.pi * 330.0 * i / rate)
                                      for i in range(count)]], rate)
        _run(["ffmpeg", "-y", "-v", "error", "-framerate", str(STRIP_FPS),
              "-i", str(workspace / "f%05d.png"), "-i", str(tone),
              "-c:v", "libtheora", "-q:v", "8", "-c:a", "libvorbis", "-q:a", "4",
              "-pix_fmt", "yuv420p", *BITEXACT, str(path)])
        return path
    finally:
        shutil.rmtree(workspace, ignore_errors=True)


# ----------------------------------------------------------------------------------- selftest ----

def _cell_index(pixels: bytes, width: int, column: int, row: int) -> int:
    """Read a cell's frame index back by counting the black bar's length."""
    x0 = column * FRAME_WIDTH
    y0 = row * FRAME_HEIGHT
    # Row 3 of the cell, comfortably inside the 8-row band.
    base = ((y0 + 3) * width + x0) * 4
    length = 0
    for x in range(FRAME_WIDTH):
        r, g, b = pixels[base + x * 4], pixels[base + x * 4 + 1], pixels[base + x * 4 + 2]
        if r < 90 and g < 90 and b < 90:
            length += 1
        else:
            break
    return length - 1


def selftest() -> int:
    failures = 0

    def require(condition: bool, message: str) -> None:
        nonlocal failures
        if condition:
            print(f"  {message}")
        else:
            print(f"  SELFTEST FAILED: {message}", file=sys.stderr)
            failures += 1

    if shutil.which("ffmpeg") is None or shutil.which("ffprobe") is None:
        print("  ffmpeg/ffprobe are not installed, so nothing can be measured", file=sys.stderr)
        return 1

    workspace = Path(tempfile.mkdtemp(prefix="video_transcode_selftest_"))
    try:
        # 6 s at 12 fps is 72 frames: more than one atlas, so the SECOND atlas exists and the
        # boundary between them can be checked. 64 exactly would prove nothing about it.
        source = make_fixture(workspace / "src" / "channel.ogv", seconds=6)
        report = convert(source, workspace / "out", "channel")

        # 1. The runtime video is the size, rate and codec §58.2 asks for.
        video = report["video"]
        require(video["codec"] == "theora" and video["width"] == VIDEO_WIDTH
                and video["height"] == VIDEO_HEIGHT and video["fps"] == VIDEO_FPS,
                f"the runtime video is {video['codec']} {video['width']}x{video['height']} at "
                f"{video['fps']} fps")
        require(abs(video["seconds"] - 6.0) < 0.2,
                f"...and {video['seconds']:.2f} s long")

        # 2. The `VideoProcessor` parameters are emitted, because CNA does not decode the source at
        #    build time and a metadata block that disagrees with the file is silent (`HOUSE-00201`).
        config = video["contentConfig"]
        require(config["width"]["value"] == str(VIDEO_WIDTH)
                and config["framesPerSecond"]["value"] == f"{float(VIDEO_FPS)}"
                and all(isinstance(v["value"], str) for v in config.values()),
                f"the content config carries width, height, fps and duration as STRINGS, which "
                f"the config format requires: {config}")

        # 3. The strip geometry is §58.3's, and the atlas is the exact fit rather than padded.
        strip = report["strip"]
        require((strip["width"], strip["height"]) == (2048, 1152),
                f"the atlas is {strip['width']}x{strip['height']}: 8 x 256 by 8 x 144, exactly")
        require(strip["framesPerAtlas"] == 64 and abs(strip["secondsPerAtlas"] - 5.3333) < 0.001,
                f"64 frames per atlas at {strip['fps']} fps is "
                f"{strip['secondsPerAtlas']:.4f} s, which is §58.3's 5.3")

        # 4. Two atlases for 72 frames, and the SECOND one holds the remainder -- the case a
        #    one-atlas clip cannot exercise.
        require(strip["frameCount"] == 72,
                f"6 s at 12 fps extracts {strip['frameCount']} frames")
        require(len(strip["atlases"]) == 2,
                f"72 frames fill {len(strip['atlases'])} atlases")
        require(strip["atlases"][0]["frames"] == 64 and strip["atlases"][1]["frames"] == 8,
                f"the split is {strip['atlases'][0]['frames']} + "
                f"{strip['atlases'][1]['frames']}")
        require(strip["atlases"][1]["firstFrame"] == 64,
                f"the second atlas starts at source frame {strip['atlases'][1]['firstFrame']}")

        # 5. THE claim: every cell holds the frame it should, in the right cell, in the right
        #    atlas. Read back from the decoded pixels by counting the index bar, so a transposed
        #    grid, an off-by-one or a dropped frame is caught rather than assumed away.
        atlases = workspace / "out" / "Textures" / "TV"
        width, _, pixels = atlas_pack.png_decode(
            (atlases / strip["atlases"][0]["file"]).read_bytes())
        wrong = []
        for slot in range(64):
            found = _cell_index(pixels, width, slot % GRID_COLUMNS, slot // GRID_COLUMNS)
            if found != slot:
                wrong.append((slot, found))
        require(not wrong, f"every one of the first atlas's 64 cells holds its own frame "
                           f"(wrong: {wrong[:5]})")

        width, _, pixels = atlas_pack.png_decode(
            (atlases / strip["atlases"][1]["file"]).read_bytes())
        found = _cell_index(pixels, width, 0, 0)
        require(found == 64,
                f"the second atlas's first cell holds source frame {found}, not 0 or 63")

        # 6. The unused tail of the last atlas is BLACK, not the last frame repeated: a television
        #    that runs out of frames goes dark rather than freezing on one.
        base = ((7 * FRAME_HEIGHT + 70) * width + 7 * FRAME_WIDTH + 100) * 4
        require(tuple(pixels[base:base + 3]) == (0, 0, 0),
                f"the unused cells are black ({tuple(pixels[base:base + 3])})")

        # 7. Audio: extracted, at the source's own rate, and in step with the picture.
        audio = strip["audio"]
        require(audio is not None and audio["rate"] == 44100 and audio["channels"] == 1,
                f"the soundtrack is extracted at {audio['rate']} Hz, {audio['channels']} ch")
        require(abs(audio["seconds"] - strip["totalSeconds"]) < 0.5,
                f"it is {audio['seconds']:.3f} s against the strip's "
                f"{strip['totalSeconds']:.3f} s, so one timer can advance both")
        require(not report["warnings"], f"no warnings: {report['warnings']}")

        # 8. Determinism, split by what can actually be promised. The atlases and the audio are
        #    written by this project's own encoders and are byte-identical; the `.ogv` is
        #    deterministic for THIS ffmpeg and not across builds, and the tool says so.
        again = convert(source, workspace / "out2", "channel")
        for index, atlas in enumerate(strip["atlases"]):
            require(atlas["sha256"] == again["strip"]["atlases"][index]["sha256"],
                    f"atlas {index} is byte-identical on a second run")
        require(strip["audio"]["sha256"] == again["strip"]["audio"]["sha256"],
                "the soundtrack is byte-identical on a second run")
        require(report["video"]["sha256"] == again["video"]["sha256"],
                "the .ogv is byte-identical on a second run WITH THIS FFMPEG -- Theora's "
                "bitstream depends on the encoder build, so that is the whole of the promise")

        # 9. `--square` pads to 2048x2048 and the cost of doing so is reported rather than hidden.
        padded = convert(source, workspace / "out3", "channel", video=False, square=True)
        require((padded["strip"]["width"], padded["strip"]["height"]) == (2048, 2048),
                f"--square gives {padded['strip']['width']}x{padded['strip']['height']}")
        exact_memory = strip["width"] * strip["height"] * 4
        padded_memory = 2048 * 2048 * 4
        require(padded_memory - exact_memory == 7_340_032,
                f"padding costs {(padded_memory - exact_memory) / 1e6:.1f} MB of texture memory "
                f"per atlas, {100 * (padded_memory - exact_memory) / padded_memory:.0f} % of it")

        # 10. A source with no video stream is refused, and one with no audio is a WARNING rather
        #     than a failure -- a static channel has no soundtrack and is still a channel.
        silent = workspace / "silent.ogv"
        _run(["ffmpeg", "-y", "-v", "error", "-i", str(source), "-an", "-c:v", "copy",
              *BITEXACT, str(silent)])
        quiet_report = convert(silent, workspace / "out4", "silent", video=False)
        require(quiet_report["strip"]["audio"] is None
                and any("no audio track" in w for w in quiet_report["warnings"]),
                f"a source with no audio warns rather than fails: {quiet_report['warnings']}")

        not_video = workspace / "notvideo.wav"
        audio_probe.write_wav(not_video, [[0.0] * 100], 44100)
        try:
            probe(not_video)
            require(False, "a file with no video stream is refused")
        except TranscodeError as error:
            require("no video stream" in str(error),
                    f"a file with no video stream is refused: {error}")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("video_transcode: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("source", nargs="?", type=Path)
    parser.add_argument("--name", help="the channel's asset name, e.g. broadcast_01")
    parser.add_argument("--out", type=Path, help="an assets-src-shaped directory to write into")
    parser.add_argument("--no-strip", action="store_true", help="skip the frame-strip atlases")
    parser.add_argument("--no-video", action="store_true", help="skip the runtime .ogv")
    parser.add_argument("--square", action="store_true",
                        help="pad the atlas to 2048x2048; costs 7.3 MB of texture memory each")
    parser.add_argument("--json", type=Path)
    parser.add_argument("--make-fixture", type=Path, metavar="OGV")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if args.make_fixture is not None:
        path = make_fixture(args.make_fixture)
        print(f"{path}  {path.stat().st_size} bytes")
        return 0
    if args.source is None or args.name is None or args.out is None:
        parser.print_help()
        return 2

    try:
        report = convert(args.source, args.out, args.name, strips=not args.no_strip,
                         video=not args.no_video, square=args.square)
    except (TranscodeError, audio_probe.WavError) as error:
        print(f"video_transcode: {error}", file=sys.stderr)
        return 1

    if args.json is not None:
        args.json.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    source = report["source"]
    print(f"{source['file']}: {source['width']}x{source['height']} {source['codec']} "
          f"at {source['fps']:.2f} fps, {source['seconds']:.2f} s")
    if "video" in report:
        video = report["video"]
        print(f"  video   {video['file']:<28} {video['width']}x{video['height']} @ "
              f"{video['fps']} fps  {video['bytes']:,} bytes -> {video['contentName']}")
    if "strip" in report:
        strip = report["strip"]
        print(f"  strip   {len(strip['atlases'])} atlas(es) of "
              f"{strip['width']}x{strip['height']}, {strip['frameCount']} frames at "
              f"{strip['fps']} fps ({strip['totalSeconds']:.2f} s)")
        for atlas in strip["atlases"]:
            print(f"          {atlas['file']:<28} frames {atlas['firstFrame']}.."
                  f"{atlas['firstFrame'] + atlas['frames'] - 1}  {atlas['bytes']:,} bytes")
        if strip["audio"]:
            print(f"  audio   {strip['audio']['file']:<28} {strip['audio']['rate']} Hz, "
                  f"{strip['audio']['channels']} ch, {strip['audio']['seconds']:.3f} s")
    for warning in report["warnings"]:
        print(f"  warning: {warning}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
