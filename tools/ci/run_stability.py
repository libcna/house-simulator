#!/usr/bin/env python3
"""Run HOUSE-02598's bounded Linux/CI stability workload and write its report."""

from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
import tempfile
import time
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
RSS_LIMIT_MIB_PER_HOUR = 20.0
SETTINGS_WRITES = 200
POLL_SECONDS = 0.25
STARVATION = re.compile(r"\b(?:audio[ -])?(?:starv(?:e|ed|ation)|underrun|xrun)\b", re.IGNORECASE)


def resident_kib(pid: int) -> int | None:
    """Read the Linux process' current resident set without another process."""
    try:
        for line in Path(f"/proc/{pid}/status").read_text(encoding="utf-8").splitlines():
            if line.startswith("VmRSS:"):
                return int(line.split()[1])
    except (FileNotFoundError, ProcessLookupError, PermissionError, ValueError):
        return None
    return None


def rss_growth_mib_per_hour(samples: list[tuple[float, int]]) -> float:
    """Least-squares RSS slope after discarding process warm-up."""
    if len(samples) < 30:
        raise RuntimeError(f"only {len(samples)} RSS samples; at least 30 are required")
    duration = samples[-1][0] - samples[0][0]
    warmup = min(duration * 0.25, max(10.0, duration * 0.10))
    kept = [(seconds, kib) for seconds, kib in samples if seconds >= samples[0][0] + warmup]
    if len(kept) < 20:
        raise RuntimeError(f"only {len(kept)} RSS samples remain after warm-up")
    mean_x = statistics.fmean(point[0] for point in kept)
    mean_y = statistics.fmean(point[1] for point in kept)
    denominator = sum((point[0] - mean_x) ** 2 for point in kept)
    if denominator == 0.0:
        return 0.0
    kib_per_second = sum((point[0] - mean_x) * (point[1] - mean_y) for point in kept) / denominator
    return max(0.0, kib_per_second * 3600.0 / 1024.0)


def run_process(
    name: str,
    command: list[str],
    output_dir: Path,
    environment: dict[str, str],
) -> dict[str, Any]:
    log_path = output_dir / f"stability-{name}.log"
    print(f"[stability] starting {name}: {' '.join(command)}", flush=True)
    started = time.monotonic()
    samples: list[tuple[float, int]] = []
    with log_path.open("w", encoding="utf-8") as log:
        log.write("command: " + " ".join(command) + "\n")
        log.flush()
        process = subprocess.Popen(
            command,
            cwd=ROOT,
            env=environment,
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True,
        )
        while process.poll() is None:
            rss = resident_kib(process.pid)
            if rss is not None:
                samples.append((time.monotonic() - started, rss))
            time.sleep(POLL_SECONDS)
        rss = resident_kib(process.pid)
        if rss is not None:
            samples.append((time.monotonic() - started, rss))
        return_code = process.returncode

    duration = time.monotonic() - started
    text = log_path.read_text(encoding="utf-8", errors="replace")
    result: dict[str, Any] = {
        "name": name,
        "command": command,
        "duration_seconds": round(duration, 3),
        "exit_code": return_code,
        "rss_samples": len(samples),
        "rss_min_mib": round(min((point[1] for point in samples), default=0) / 1024.0, 3),
        "rss_max_mib": round(max((point[1] for point in samples), default=0) / 1024.0, 3),
        "starvation_events": len(STARVATION.findall(text)),
        "log": str(log_path),
    }
    if len(samples) >= 30:
        result["rss_growth_mib_per_hour"] = round(rss_growth_mib_per_hour(samples), 3)
    print(
        f"[stability] {name}: exit {return_code}, {duration:.1f} s, "
        f"RSS {result['rss_min_mib']:.1f}..{result['rss_max_mib']:.1f} MiB",
        flush=True,
    )
    return result


def require_success(result: dict[str, Any]) -> None:
    if result["exit_code"] == 0:
        return
    log_path = Path(result["log"])
    tail = log_path.read_text(encoding="utf-8", errors="replace").splitlines()[-80:]
    raise RuntimeError(f"{result['name']} exited {result['exit_code']}\n" + "\n".join(tail))


def repetitions_for(target_seconds: float, calibration_seconds: float) -> int:
    remaining = max(0.0, target_seconds - calibration_seconds)
    return max(2, math.ceil(remaining / max(calibration_seconds, 0.001)))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--duration-seconds", type=float, default=20.0 * 60.0)
    parser.add_argument("--report", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    build_dir = args.build_dir.resolve()
    unit = build_dir / "cnahouse_unit_tests"
    integration = build_dir / "cnahouse_integration_tests"
    for binary in (unit, integration):
        if not binary.is_file():
            raise RuntimeError(f"missing {binary}; build the existing test targets first")
    if args.duration_seconds <= 0.0:
        raise RuntimeError("--duration-seconds must be positive")

    output_dir = build_dir / "test-output"
    output_dir.mkdir(parents=True, exist_ok=True)
    report_path = (args.report or output_dir / "stability-report.json").resolve()
    environment = os.environ.copy()
    environment.setdefault("SDL_VIDEODRIVER", "offscreen")
    environment.setdefault("SDL_AUDIODRIVER", "dummy")
    environment["OMP_NUM_THREADS"] = "1"

    run_started = time.monotonic()
    results: list[dict[str, Any]] = []
    with tempfile.TemporaryDirectory(prefix="cnahouse-stability-", dir=output_dir) as data_home:
        environment["XDG_DATA_HOME"] = data_home
        control_filter = ":".join(
            [
                "SaveStoreTest.TwoHundredSettingsWritesRemainReadable",
                "WeatherSeasonTests.StabilityCycleUsesSixtyTimesClockAndClearOvercastRain",
                "AudioGateTests.NoAudioRunsAFullSessionAndNeverOpensTheDevice",
            ]
        )
        control = run_process(
            "control",
            [str(integration), f"--gtest_filter={control_filter}", "--gtest_color=no"],
            output_dir,
            environment,
        )
        results.append(control)
        require_success(control)
        control_text = Path(control["log"]).read_text(encoding="utf-8", errors="replace")
        settings_match = re.search(r"stability settings writes: (\d+)", control_text)
        clock_weather_ok = "stability clock: 60x; weather: clear -> overcast -> rain" in control_text
        if settings_match is None or int(settings_match.group(1)) < SETTINGS_WRITES:
            raise RuntimeError("the control run did not prove 200 settings writes")
        if not clock_weather_ok:
            raise RuntimeError("the control run did not prove the 60x clear -> overcast -> rain cycle")

        workloads = [
            (
                "grand-tour",
                integration,
                "GrandTourTests.EveryAccessibleCellIsReachedOnFoot",
            ),
            (
                "seeded-random-walk",
                unit,
                "RandomWalkTests.TwentyMinutesOfWanderingStaysInTheHouse",
            ),
        ]
        target_per_workload = args.duration_seconds / 2.0
        for name, binary, test_filter in workloads:
            calibration = run_process(
                f"{name}-calibration",
                [str(binary), f"--gtest_filter={test_filter}", "--gtest_color=no"],
                output_dir,
                environment,
            )
            results.append(calibration)
            require_success(calibration)
            repeats = repetitions_for(target_per_workload, float(calibration["duration_seconds"]))
            repeated = run_process(
                name,
                [
                    str(binary),
                    f"--gtest_filter={test_filter}",
                    f"--gtest_repeat={repeats}",
                    "--gtest_fail_fast",
                    "--gtest_color=no",
                ],
                output_dir,
                environment,
            )
            repeated["repetitions"] = repeats
            results.append(repeated)
            require_success(repeated)

    measured = [result for result in results if result["name"] in {"grand-tour", "seeded-random-walk"}]
    if any("rss_growth_mib_per_hour" not in result for result in measured):
        raise RuntimeError("a repeated workload was too short to measure RSS growth")
    worst_growth = max(float(result["rss_growth_mib_per_hour"]) for result in measured)
    starvation_events = sum(int(result["starvation_events"]) for result in results)
    passed = worst_growth < RSS_LIMIT_MIB_PER_HOUR and starvation_events == 0
    report = {
        "schema": "cna-house/stability-report/1",
        "house_task": "HOUSE-02598",
        "requested_workload_seconds": args.duration_seconds,
        "elapsed_seconds": round(time.monotonic() - run_started, 3),
        "settings_writes": int(settings_match.group(1)),
        "clock_scale": 60,
        "weather_cycle": ["clear", "overcast", "rain"],
        "rss_limit_mib_per_hour": RSS_LIMIT_MIB_PER_HOUR,
        "worst_rss_growth_mib_per_hour": round(worst_growth, 3),
        "audio_starvation_events": starvation_events,
        "crashes": sum(1 for result in results if result["exit_code"] != 0),
        "passed": passed,
        "processes": results,
    }
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    print(f"report: {report_path}")
    if not passed:
        raise RuntimeError(
            f"stability limits failed: RSS {worst_growth:.3f} MiB/hour, "
            f"audio starvation events {starvation_events}"
        )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
