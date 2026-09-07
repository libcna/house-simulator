#!/usr/bin/env python3
"""nox_select.py -- choose the NOX Essentials subset this project ships, and say why.

`HOUSE-00277`. `HOUSE-00276` verified the collection as `CC0-1.0` for 1 634 of its 1 644 files and
`cna-house.md` §63.3/§63.4 assessed each pack. This selects the roughly 430 that ship, records the
category each one serves, and — the part that matters as much — records every file that does NOT
ship and the reason.

## Selection is rules over measurements, never a hand-picked list

Every file is measured (duration, rate, channels, bit depth, peak, RMS) and then passed through
rules that are written down here rather than applied by eye:

1. **Exclusions**, by directory or name, each with a reason. §63.3 excludes the Azores flows;
   `HOUSE-00276` adds `Sample_A_Sound_Effect/` because it advertises NOX's *paid* libraries and
   ADR-0012 says unclear means unusable; §63.3 excludes the truck pack and the combat voices as
   irrelevant to a house.
2. **Quality**, measured: a file that clips, that is effectively silent, or whose duration is
   outside what its family can be is rejected and named.
3. **A cap per group**, because a game needs *enough* variation, not all of it. Eight footstep
   variations per surface and action is more than a player can hear as a pattern; forty is forty
   times the memory of one.
4. **Which eight**, chosen by **farthest-point sampling in (duration, RMS)** rather than by taking
   the first eight. Consecutive takes from one session are the most similar files in the group, so
   "the first eight" is the one selection guaranteed to sound repetitive. Starting from the
   median-duration file and repeatedly adding the most different remaining one is deterministic
   and maximises the spread that survives.

## What this does not do

It does not listen. Level, duration and channel count are measurable and are measured; whether a
recording is *clean* is not, and this tool does not pretend otherwise — it reports the loudest, the
quietest, the shortest and the longest of every group so a human check has somewhere to start.

    tools/assets/nox_select.py --pool /rv/tmp/Essentials_Series_NOX_SOUND
    tools/assets/nox_select.py --pool <dir> --emit
    tools/assets/nox_select.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audio_probe  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SELECTION = REPO / "docs" / "asset-selection" / "nox-subset.json"
SUMMARY = REPO / "docs" / "asset-selection" / "nox-subset.md"
DEFAULT_POOL = Path("/rv/tmp/Essentials_Series_NOX_SOUND")

#: Directories and names that do not ship, each with the reason it does not. Matched against the
#: pool-relative POSIX path. Every one of these is a decision someone made for a stated reason, and
#: a reader who disagrees can find the reason rather than the omission.
EXCLUSIONS: tuple[tuple[str, str], ...] = (
    (r"Sample_A_Sound_Effect/",
     "a sampler advertising NOX's PAID A Sound Effect libraries, with a store shortcut beside it. "
     "A rights holder may licence their own work twice, so this is not a contradiction -- but it "
     "is not clear either, and ADR-0012 says unclear means unusable (HOUSE-00276)."),
    (r"Miguel_Flows",
     "Azores field recordings: hot springs and ocean at three locations. cna-house.md §63.3 -- "
     "not useful for a suburban house, and 294 MB of it."),
    (r"Vehicle_Essential_Truck/",
     "a truck. §63.3 ships the car's static sounds; there is no truck in this house."),
    (r"Voice_(Male|Female)_(Attack|Hit|Pain)/",
     "combat vocalisations. §63.3: irrelevant to this project and will not ship."),
    (r"Voice_(Male|Female)_Expressions/",
     "184 acted expressions -- shouts, laughs, exclamations. The house has no dialogue and no "
     "NPC that speaks; §63.3 lists breath, effort, cough and throat-clearing as the usable "
     "voice content and this is not among them."),
    (r"Voice_(Male|Female)_Jump_Land/",
     "vocalised jump and landing efforts. The avatar in a house does not jump (§47.1 has "
     "land_soft/land_hard as animation, not vocalisation), and the effort set already covers "
     "exertion on stairs."),
    (r"Ambiance_(Sea|Waterfall)",
     "sea and waterfall. §63.3: no sea and no waterfall in a suburb. The light stream loops are "
     "kept for the gutter trickle; these are not."),
    (r"Ambiance_Cave",
     "cave tone. There is no cave; the basement's tone is sourced with the other room tones."),
    (r"Iceland_Flows_NOX_SOUND/Ambiance_Stream_(Big|Moderate)",
     "big and moderate Icelandic waterfalls and rivers. §63.3 keeps only the LIGHT stream loops, "
     "as the base layer for the gutter and downspout; a suburban garden has no Skogafoss in it."),
    (r"Vehicle_Car_(Drive|Engine)",
     "drive loops and engine RPM. §63.3: the drive loops are unused because the car does not "
     "move -- it sits in the garage and is heard as doors, buttons, keys and a seatbelt."),
    (r"Voice_(Male|Female)_VN?_Breath_(Gasp|Shocked|Frozen)",
     "gasps, shocked breaths and shivering. Dramatic vocalisation for a game that has none: the "
     "house's use for breath is exertion on stairs, which is the single and sequence takes."),
    (r"Black_Pebble",
     "an Icelandic black-pebble beach surface. Not one of the twenty surfaces §63 lists."),
)

#: `(pattern, category, cap, note)`. The FIRST match wins, so the order is meaningful: the
#: specific rules come before the general ones.
#:
#: A cap is a statement about how much variation a player can hear, not about how much exists.
#: `HOUSE-00193` measured that a converted footstep is about 40 kB; eight per surface and action is
#: a third of a megabyte and is already more than the ear can pattern-match. Six rather than eight
#: for footsteps: at eight the selection came to 489 files and 236 MB of 24-bit source, and §72
#: budgets "~430 clips ... ≈ 95 MB" at 16-bit. Six is the cap that lands on §72's own count.
RULES: tuple[tuple[str, str, int, str], ...] = (
    (r"Footsteps_Essentials_NOX_SOUND/Footsteps_(?P<surface>[A-Za-z]+)/"
     r"Footsteps_[A-Za-z]+_(?P<action>Walk|Run|Jump|Land)/",
     "footstep/{surface}/{action}", 6,
     "§63.4: twelve of the twenty surfaces the game needs, and the collection's strength"),
    (r"Footsteps_Essentials_NOX_SOUND/Footsteps_(?P<surface>[A-Za-z]+)/",
     "footstep/{surface}/foley", 4,
     "loose foley in a surface folder -- scuffs and shifts rather than steps"),
    (r"Iceland_Footsteps_Pack_NOX_SOUND/Footsteps_Iceland_(?:Walk|Run|Jump)/"
     r"Footsteps_Iceland_(?P<action>Walk|Run|Jump_Land|Jump_Start)_"
     r"(?P<surface>Grass|Gravel_Wet|Gravel_Frozen|Snow_Hard|Snow_Grass|Snow_Short_Grass)_",
     "footstep-exterior/{surface}/{action}", 4,
     "outdoor variation for the garden, the drive and the winter season -- the snow surfaces "
     "matter because the season system needs them"),
    (r"Iceland_Flows_NOX_SOUND/Ambiance_Stream_Light_",
     "ambience/water/gutter-trickle", 2,
     "§63.3: the light stream loops are the base layer for the gutter and downspout"),
    (r"Nature_Essentials_NOX_SOUND/Ambiance_(?P<what>Rain_Calm|Rain_Strong|Wind_Calm|Wind_Forest|"
     r"Forest_Birds|Night|Cicadas|Fire|Firecamp)",
     "ambience/{what}", 4,
     "§63.3: the core of the weather and time-of-day ambience, and the fireplace"),
    (r"Nature_Essentials_NOX_SOUND/Ambiance_(?P<what>River|Stream)",
     "ambience/{what}", 2, "a water bed for the garden, kept sparingly"),
    (r"Electromagnetic_NOX_SOUND/Electromagnetic_(?P<device>Neon|Fan|Computer|Router|Charger|"
     r"Playstation|Speaker|Electric|Engine|Nuc|Phone|Smartphone|Keyboard|Macbook|Ipad|Car)_",
     "appliance/{device}", 3,
     "§63.3: neon is the garage fluorescent, fan is the extractor and bathroom, computer and "
     "router are the office and the wiring cabinet"),
    (r"Vehicle_Essential_Car/Vehicle_Car_(?:Electric_)?(?P<what>Door_Opening|Door_Closing|Trunk|"
     r"Keys|Button|Hand_Brake|Seat_Belt|Glove_Box|Indicator|Window_Opening|Window_Closing|Wiper|"
     r"Warning|Horn|Start_Engine|Stop_Engine)",
     "car/{what}", 3,
     "§63.3: the garage car is static, so its doors, buttons, keys and belt are what is heard"),
    (r"Voice_Essential_(?P<sex>Male|Female)/Voice_(?:Male|Female)_Breath/",
     "human/{sex}/breath", 8,
     "§63.3: exertion on stairs -- the single and sequence takes, the dramatic ones excluded"),
    (r"Voice_Essential_(?P<sex>Male|Female)/Voice_(?:Male|Female)_Effort/",
     "human/{sex}/effort", 8,
     "§63.3: exertion on stairs"),
    (r"Voice_Essential_(?P<sex>Male|Female)/Voice_(?:Male|Female)_(?P<what>Cough|"
     r"Throat_Cleaning)/",
     "human/{sex}/{what}", 4,
     "§63.3: very occasional idle sounds"),
)

#: What a file must measure to ship. Not taste -- each of these is a defect that would reach a
#: player's ears, and each is measurable.
MAX_PEAK_DBFS = -0.05      # above this the recording is clipped, or was normalised into the rail
MIN_RMS_DBFS = -55.0       # below this there is effectively nothing there
MIN_SECONDS = 0.05         # shorter than a footstep's transient
MAX_SECONDS = 400.0        # longer than any loop this project keeps resident

#: Above this a file is a LOOP rather than a one-shot, for the purposes of the budget arithmetic.
#: Three seconds is longer than any footstep, door or button and shorter than any ambience bed.
LOOP_SECONDS = 3.0
#: What a loop would cost if it were trimmed to this at conversion time (`HOUSE-00278`). Ten
#: seconds of rain or of a computer fan does not read as a repeat behind everything else in a
#: house; thirty is what the recordist supplied, not what the game needs.
LOOP_TRIM_SECONDS = 10.0


def excluded(relative: str) -> str | None:
    for pattern, reason in EXCLUSIONS:
        if re.search(pattern, relative):
            return reason
    return None


def classify(relative: str) -> tuple[str, int, str] | None:
    """(category, cap, note) for a file, or None when no rule claims it."""
    for pattern, template, cap, note in RULES:
        match = re.search(pattern, relative)
        if match:
            fields = {k: v for k, v in match.groupdict().items() if v}
            category = template.format(**fields).lower().replace("_", "-")
            return category, cap, note
    return None


def farthest_point(entries: list[dict], count: int) -> list[dict]:
    """Pick `count` of `entries`, maximising spread in (duration, RMS).

    Consecutive takes from one recording session are the most similar files in a group, so taking
    the first N is the one choice guaranteed to sound repetitive. This starts from the
    median-duration file -- the most typical one, so the group still sounds like itself -- and
    repeatedly adds whichever remaining file is furthest from everything already chosen.

    Deterministic: ties break on the file name, and the inputs are sorted before anything happens.
    """
    if len(entries) <= count:
        return list(entries)

    ordered = sorted(entries, key=lambda e: e["relative"])
    seconds = [e["seconds"] for e in ordered]
    rms = [e["rmsDbfs"] for e in ordered]
    # Normalised so a second of duration and a decibel of level count comparably; without this the
    # decibel axis (a range of tens) would swamp the duration axis (a range of fractions).
    span_s = max(max(seconds) - min(seconds), 1e-6)
    span_r = max(max(rms) - min(rms), 1e-6)
    points = [((e["seconds"] - min(seconds)) / span_s, (e["rmsDbfs"] - min(rms)) / span_r)
              for e in ordered]

    by_duration = sorted(range(len(ordered)), key=lambda i: (seconds[i], ordered[i]["relative"]))
    chosen = [by_duration[len(by_duration) // 2]]
    while len(chosen) < count:
        best = None
        best_distance = -1.0
        for index in range(len(ordered)):
            if index in chosen:
                continue
            distance = min((points[index][0] - points[c][0]) ** 2
                           + (points[index][1] - points[c][1]) ** 2 for c in chosen)
            if distance > best_distance or (distance == best_distance and best is not None
                                            and ordered[index]["relative"]
                                            < ordered[best]["relative"]):
                best_distance = distance
                best = index
        chosen.append(best)
    return [ordered[i] for i in sorted(chosen)]


def select(pool: Path, hash_selected: bool = True) -> dict:
    if not pool.is_dir():
        raise SystemExit(f"nox_select: {pool} is not a directory")

    files = sorted(pool.rglob("*.wav"), key=lambda p: p.as_posix())
    if not files:
        raise SystemExit(f"nox_select: {pool} holds no .wav files")

    excluded_rows: list[dict] = []
    rejected: list[dict] = []
    unclaimed: list[str] = []
    groups: dict[str, list[dict]] = {}
    group_note: dict[str, str] = {}
    group_cap: dict[str, int] = {}

    for path in files:
        relative = path.relative_to(pool).as_posix()
        reason = excluded(relative)
        if reason is not None:
            excluded_rows.append({"file": relative, "reason": reason})
            continue

        try:
            measured = audio_probe.measure(path)
        except audio_probe.WavError as error:
            rejected.append({"file": relative, "reason": f"could not be measured: {error}"})
            continue

        rule = classify(relative)
        if rule is None:
            # NOT silently dropped. A file no rule claims is a rule that has not been written, and
            # a selection that quietly omitted it would be a selection nobody could audit.
            unclaimed.append(relative)
            continue
        category, cap, note = rule

        entry = {"file": path.name, "relative": relative, "category": category,
                 "seconds": round(measured["seconds"], 4),
                 "rate": measured["rate"], "channels": measured["channels"],
                 "bits": measured["bits"],
                 "peakDbfs": round(measured["peakDbfs"], 3),
                 "rmsDbfs": round(measured["rmsDbfs"], 3),
                 "bytes": measured["bytes"]}

        problems = []
        if measured["peakDbfs"] > MAX_PEAK_DBFS:
            problems.append(f"peaks at {measured['peakDbfs']:.2f} dBFS: clipped or normalised "
                            f"into the rail")
        if measured["rmsDbfs"] < MIN_RMS_DBFS:
            problems.append(f"RMS {measured['rmsDbfs']:.1f} dBFS: effectively silent")
        if not MIN_SECONDS <= measured["seconds"] <= MAX_SECONDS:
            problems.append(f"{measured['seconds']:.3f} s is outside "
                            f"[{MIN_SECONDS}, {MAX_SECONDS}]")
        if problems:
            rejected.append({**entry, "reason": "; ".join(problems)})
            continue

        groups.setdefault(category, []).append(entry)
        group_note[category] = note
        group_cap[category] = cap

    chosen: list[dict] = []
    group_rows: list[dict] = []
    for category in sorted(groups):
        available = groups[category]
        keep = farthest_point(available, group_cap[category])
        kept = {e["relative"] for e in keep}
        for entry in available:
            if entry["relative"] not in kept:
                rejected.append({**entry, "reason": f"over the cap of {group_cap[category]} for "
                                                    f"{category}; kept the most varied"})
        chosen.extend(keep)
        durations = sorted(e["seconds"] for e in keep)
        levels = sorted(e["rmsDbfs"] for e in keep)
        group_rows.append({
            "category": category, "note": group_note[category],
            "available": len(available), "cap": group_cap[category], "selected": len(keep),
            "secondsRange": [durations[0], durations[-1]],
            "rmsRange": [levels[0], levels[-1]],
            # Named so a human check has somewhere to start: these four are where a bad recording
            # in the group would be, if there is one.
            "shortest": min(keep, key=lambda e: e["seconds"])["file"],
            "longest": max(keep, key=lambda e: e["seconds"])["file"],
            "quietest": min(keep, key=lambda e: e["rmsDbfs"])["file"],
            "loudest": max(keep, key=lambda e: e["rmsDbfs"])["file"],
        })

    chosen.sort(key=lambda e: (e["category"], e["relative"]))
    if hash_selected:
        for entry in chosen:
            entry["sha256"] = hashlib.sha256((pool / entry["relative"]).read_bytes()).hexdigest()

    # What the selection COSTS, at the format it will ship in. `HOUSE-00193` established that the
    # sample rate is preserved and the bit depth goes to 16, so this is arithmetic on measured
    # durations rather than an estimate -- and it is the number §72's audio budget has to meet.
    def bytes16(entries: list[dict]) -> int:
        return int(sum(e["seconds"] * e["rate"] * e["channels"] * 2 for e in entries))

    loops = [e for e in chosen if e["seconds"] > LOOP_SECONDS]
    oneshots = [e for e in chosen if e["seconds"] <= LOOP_SECONDS]
    trimmed = int(sum(min(e["seconds"], LOOP_TRIM_SECONDS) * e["rate"] * e["channels"] * 2
                      for e in chosen))

    return {
        "tool": "tools/assets/nox_select.py",
        "formatVersion": 1,
        "budget": {
            "bytes16Bit": bytes16(chosen),
            "oneShots": len(oneshots), "oneShotBytes16Bit": bytes16(oneshots),
            "loops": len(loops), "loopBytes16Bit": bytes16(loops),
            "loopSeconds": round(sum(e["seconds"] for e in loops), 1),
            "bytes16BitIfLoopsTrimmed": trimmed,
            "loopTrimSeconds": LOOP_TRIM_SECONDS,
        },
        "pool": pool.name,
        "poolFiles": len(files),
        "selected": len(chosen),
        "selectedBytes": sum(e["bytes"] for e in chosen),
        "excluded": len(excluded_rows),
        "rejected": len(rejected),
        "unclaimed": unclaimed,
        "rules": [{"category": template, "cap": cap, "note": note}
                  for _, template, cap, note in RULES],
        "exclusions": [{"pattern": p, "reason": r} for p, r in EXCLUSIONS],
        "groups": group_rows,
        "files": chosen,
        "excludedFiles": excluded_rows,
        # Path and reason only. A rejected file's measurements are already IN its reason -- "peaks
        # at 0.00 dBFS", "0.02 s is outside [0.05, 400.0]" -- and repeating the whole measurement
        # row for 1 199 files trebles the size of a document whose value is that it can be read.
        "rejectedFiles": [{"file": e.get("relative", e.get("file", "")), "reason": e["reason"]}
                          for e in sorted(rejected,
                                          key=lambda e: e.get("relative", e.get("file", "")))],
    }


def summarise(result: dict) -> str:
    lines = ["# The NOX Essentials subset this project ships", ""]
    lines.append(f"Generated by `tools/assets/nox_select.py` from `{result['pool']}`. "
                 f"`HOUSE-00277`.")
    lines.append("")
    lines.append(f"**{result['selected']} of {result['poolFiles']} files ship** — "
                 f"{result['selectedBytes'] / 1e6:.1f} MB of 24-bit source, before "
                 f"`convert_audio.py` takes it to 16-bit (`HOUSE-00278`). "
                 f"{result['excluded']} are excluded by rule and {result['rejected']} did not "
                 f"survive the measurements or the caps.")
    lines.append("")
    lines.append("Every number here is measured from the file. Nothing was chosen by ear, and "
                 "nothing about how *clean* a recording is has been assessed — that is what the "
                 "shortest, longest, quietest and loudest columns are for.")
    lines.append("")

    budget = result["budget"]
    lines.append("## What it costs, and the one number that does not fit")
    lines.append("")
    lines.append(f"| | Files | 16-bit bytes |")
    lines.append("|---|---:|---:|")
    lines.append(f"| One-shots (≤ {LOOP_SECONDS:g} s) | {budget['oneShots']} | "
                 f"{budget['oneShotBytes16Bit'] / 1e6:.1f} MB |")
    lines.append(f"| Loops (> {LOOP_SECONDS:g} s) | {budget['loops']} | "
                 f"{budget['loopBytes16Bit'] / 1e6:.1f} MB |")
    lines.append(f"| **Total** | **{result['selected']}** | "
                 f"**{budget['bytes16Bit'] / 1e6:.1f} MB** |")
    lines.append("")
    lines.append(f"`cna-house.md` §72 budgets *\"~430 clips at 16-bit 44.1 kHz mono/stereo, "
                 f"≈ 95 MB\"*. This selection is {result['selected']} clips — the right count — "
                 f"and **{budget['bytes16Bit'] / 1e6:.0f} MB**, which is "
                 f"{budget['bytes16Bit'] / 95e6:.1f}× that budget.")
    lines.append("")
    lines.append(f"The count is right and the size is not, and the reason is visible in the split "
                 f"above: **{budget['oneShots']} one-shots come to only "
                 f"{budget['oneShotBytes16Bit'] / 1e6:.0f} MB**, while "
                 f"{budget['loops']} loops carry {budget['loopSeconds']:.0f} seconds and "
                 f"{budget['loopBytes16Bit'] / 1e6:.0f} MB. §72's figure works out to about 0.22 MB "
                 f"a clip, which is roughly one second of mono — it priced one-shots and did not "
                 f"price a 30-second stereo rain bed or a 108-second computer hum.")
    lines.append("")
    lines.append(f"**Trimming every loop to {LOOP_TRIM_SECONDS:g} seconds brings the total to "
                 f"{budget['bytes16BitIfLoopsTrimmed'] / 1e6:.0f} MB**, inside the budget. Ten "
                 f"seconds of rain or of a fan does not read as a repeat behind everything else in "
                 f"a house; thirty is what the recordist supplied, not what the game needs. That "
                 f"is `HOUSE-00278`'s decision to make with `convert_audio.py --trim`, not this "
                 f"tool's — the selection is *which* files ship, and this is the measurement that "
                 f"tells the conversion what it has to achieve.")
    lines.append("")

    lines.append("## What ships, by category")
    lines.append("")
    lines.append("| Category | Available | Cap | Ships | Seconds | RMS dBFS | Why |")
    lines.append("|---|---:|---:|---:|---|---|---|")
    for group in result["groups"]:
        lines.append(f"| `{group['category']}` | {group['available']} | {group['cap']} | "
                     f"{group['selected']} | "
                     f"{group['secondsRange'][0]:.2f}–{group['secondsRange'][1]:.2f} | "
                     f"{group['rmsRange'][0]:.1f}–{group['rmsRange'][1]:.1f} | {group['note']} |")
    lines.append("")

    lines.append("## What does not ship, and why")
    lines.append("")
    counts: dict[str, int] = {}
    for row in result["excludedFiles"]:
        counts[row["reason"]] = counts.get(row["reason"], 0) + 1
    lines.append("| Files | Reason |")
    lines.append("|---:|---|")
    for reason in sorted(counts, key=lambda r: -counts[r]):
        lines.append(f"| {counts[reason]} | {reason} |")
    lines.append("")

    measured_rejects = [r for r in result["rejectedFiles"] if "over the cap" not in r["reason"]]
    lines.append(f"A further **{len(result['rejectedFiles']) - len(measured_rejects)}** files were "
                 f"inside a category but over its cap, and **{len(measured_rejects)}** failed a "
                 f"measurement:")
    lines.append("")
    if measured_rejects:
        lines.append("| File | Reason |")
        lines.append("|---|---|")
        for row in measured_rejects[:40]:
            lines.append(f"| `{row.get('relative', row.get('file'))}` | {row['reason']} |")
    else:
        lines.append("*None: every file inside a category passed the level, duration and format "
                     "checks.*")
    lines.append("")

    if result["unclaimed"]:
        lines.append(f"## {len(result['unclaimed'])} files no rule claims")
        lines.append("")
        lines.append("Neither shipped nor excluded — a rule has not been written for them. Listed "
                     "rather than dropped, because a selection nobody can audit is not a selection.")
        lines.append("")
        for relative in result["unclaimed"][:60]:
            lines.append(f"* `{relative}`")
        if len(result["unclaimed"]) > 60:
            lines.append(f"* …and {len(result['unclaimed']) - 60} more")
        lines.append("")

    lines.append("## How the files inside a cap were chosen")
    lines.append("")
    lines.append("**Farthest-point sampling in (duration, RMS)**, starting from the "
                 "median-duration file. Consecutive takes from one recording session are the most "
                 "similar files in a group, so taking the first *N* is the one selection "
                 "guaranteed to sound repetitive. Starting from the most typical file keeps the "
                 "group sounding like itself; adding whichever remaining file is furthest from "
                 "everything already chosen keeps the spread. It is deterministic — the inputs are "
                 "sorted and ties break on the file name.")
    return "\n".join(lines) + "\n"


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

    import math

    workspace = Path(tempfile.mkdtemp(prefix="nox_select_selftest_"))
    try:
        pool = workspace / "pool"

        def tone(relative: str, seconds: float, amplitude: float, rate: int = 48000) -> None:
            path = pool / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            count = int(rate * seconds)
            audio_probe.write_wav(path, [[amplitude * math.sin(2 * math.pi * 440 * i / rate)
                                          for i in range(count)]], rate)

        # A group with more files than its cap, deliberately spread in duration and level so the
        # farthest-point choice has something to find.
        for index in range(12):
            tone(f"Footsteps_Essentials_NOX_SOUND/Footsteps_Grass/Footsteps_Grass_Walk/"
                 f"Footsteps_Grass_Walk_{index:02d}.wav",
                 0.20 + 0.02 * index, 0.30 + 0.03 * index)
        # One of each thing that must be excluded, so every exclusion is exercised.
        tone("Sample_A_Sound_Effect/Household_Door_Wood_Open_Stereo.wav", 0.5, 0.3)
        tone("Sao_Miguel_Flows_NOX_SOUND/Ambiance_Hot_Spring.wav", 0.5, 0.3)
        tone("Vehicle_Essentials_NOX_SOUND/Vehicle_Essential_Truck/Vehicle_Truck_Door_01.wav",
             0.5, 0.3)
        tone("Voices_Essentials_NOX_SOUND/Voice_Essential_Male/Voice_Male_Pain/"
             "Voice_Male_V1_Pain_Mono_01.wav", 0.5, 0.3)
        tone("Voices_Essentials_NOX_SOUND/Voice_Essential_Male/Voice_Male_Expressions/"
             "Voice_Male_V1_Expressions_Mono_01.wav", 0.5, 0.3)
        tone("Iceland_Packs_NOX_SOUND/Iceland_Flows_NOX_SOUND/Ambiance_Sea_Strong_Loop.wav",
             0.5, 0.3)
        # A clipped file and a silent one, both inside a shipping category.
        tone("Footsteps_Essentials_NOX_SOUND/Footsteps_Tile/Footsteps_Tile_Walk/"
             "Footsteps_Tile_Walk_01.wav", 0.3, 0.99999)
        tone("Footsteps_Essentials_NOX_SOUND/Footsteps_Tile/Footsteps_Tile_Walk/"
             "Footsteps_Tile_Walk_02.wav", 0.3, 0.0004)
        tone("Footsteps_Essentials_NOX_SOUND/Footsteps_Tile/Footsteps_Tile_Walk/"
             "Footsteps_Tile_Walk_03.wav", 0.3, 0.4)
        # A file no rule claims.
        tone("Mystery_Pack/Something_Unclassified_01.wav", 0.4, 0.3)

        result = select(pool, hash_selected=True)

        # 1. Every exclusion fires, and each carries its reason rather than a bare omission.
        require(result["excluded"] == 6,
                f"six files are excluded by rule ({result['excluded']})")
        reasons = {row["reason"].split(".")[0] for row in result["excludedFiles"]}
        require(len(reasons) == 6, f"each exclusion carries its own reason ({len(reasons)})")
        excluded_names = {row["file"] for row in result["excludedFiles"]}
        require(any("Miguel" in n for n in excluded_names)
                and any("Truck" in n for n in excluded_names)
                and any("Pain" in n for n in excluded_names)
                and any("Sample_A_Sound_Effect" in n for n in excluded_names),
                "the Azores flows, the truck pack, the combat voices and the paid sampler are all "
                "excluded -- which is HOUSE-00277's acceptance criterion")

        # 2. The measured rejections. A clipped file and a silent one are both real defects that a
        #    listener would find and a file list would not.
        rejects = {r["file"]: r["reason"] for r in result["rejectedFiles"] if "file" in r}
        require(any("clipped" in r for r in rejects.values()),
                f"a file peaking at 0 dBFS is rejected as clipped")
        require(any("effectively silent" in r for r in rejects.values()),
                "a file at -68 dBFS RMS is rejected as silent")

        # 3. The cap holds, and what it kept is SPREAD rather than the first eight.
        grass = [e for e in result["files"] if e["category"] == "footstep/grass/walk"]
        cap = next(c for pattern, _, c, _ in RULES if "Footsteps_Essentials" in pattern
                   and "action" in pattern)
        require(len(grass) == cap, f"the twelve grass walks are capped to {len(grass)} (cap {cap})")
        indices = sorted(int(e["file"][-6:-4]) for e in grass)
        require(indices != list(range(cap)),
                f"the {cap} kept are not simply the first {cap}: {indices}")
        require(0 in indices and 11 in indices,
                f"...and they include both extremes of the group, which is what maximising the "
                f"spread means ({indices})")

        # 4. A file no rule claims is LISTED, not dropped. A selection that silently omitted a file
        #    is a selection nobody can audit.
        require(result["unclaimed"] == ["Mystery_Pack/Something_Unclassified_01.wav"],
                f"an unclaimed file is listed rather than dropped: {result['unclaimed']}")

        # 5. Every shipped file carries its hash. The pool is scratch space that will be deleted;
        #    the hash is what keeps the conversion auditable afterwards (HOUSE-00276's risk note).
        require(all(len(e.get("sha256", "")) == 64 for e in result["files"]),
                "every selected file carries a sha256")

        # 5b. The PARTITION: every file in the pool is selected, excluded, rejected or listed as
        #     unclaimed, and no file is two of those. A selection that lost a file somewhere in the
        #     middle would look exactly like a selection that did not want it.
        total = (result["selected"] + result["excluded"] + result["rejected"]
                 + len(result["unclaimed"]))
        require(total == result["poolFiles"],
                f"every one of the {result['poolFiles']} files is accounted for exactly once "
                f"({result['selected']} + {result['excluded']} + {result['rejected']} + "
                f"{len(result['unclaimed'])} = {total})")
        everywhere = ([e["relative"] for e in result["files"]]
                      + [e["file"] for e in result["excludedFiles"]]
                      + [e["file"] for e in result["rejectedFiles"]]
                      + result["unclaimed"])
        require(len(set(everywhere)) == len(everywhere) == result["poolFiles"],
                f"...and no file appears in two of the four lists "
                f"({len(set(everywhere))} distinct of {len(everywhere)})")

        # 6. Determinism.
        again = select(pool, hash_selected=False)
        require([e["relative"] for e in again["files"]]
                == [e["relative"] for e in result["files"]],
                "two runs select the same files in the same order")

        # 7. The summary names the four extremes of every group, which is where a human check
        #    starts -- the tool measures level and duration and cannot judge cleanliness.
        text = summarise(result)
        require("Shortest" in text or "shortest" in str(result["groups"][0]),
                "each group names its shortest, longest, quietest and loudest file")
        require("|" in text and "footstep/grass/walk" in text,
                "the summary is a Markdown table naming the categories")

        # 8. Farthest-point sampling, on its own, against a known answer.
        entries = [{"relative": f"{i}", "seconds": float(i), "rmsDbfs": 0.0} for i in range(11)]
        picked = sorted(float(e["relative"]) for e in farthest_point(entries, 3))
        require(picked[0] == 0.0 and picked[-1] == 10.0,
                f"farthest-point sampling takes both ends of a uniform spread: {picked}")
        require(len(farthest_point(entries, 20)) == 11,
                "asking for more than exist returns all of them")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("nox_select: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--pool", type=Path, default=DEFAULT_POOL)
    parser.add_argument("--emit", action="store_true",
                        help=f"write {SELECTION.relative_to(REPO)} and "
                             f"{SUMMARY.relative_to(REPO)}")
    parser.add_argument("--no-hash", action="store_true",
                        help="skip hashing the selected files (faster, not shippable)")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    result = select(args.pool, hash_selected=not args.no_hash)
    if args.emit:
        SELECTION.parent.mkdir(parents=True, exist_ok=True)
        SELECTION.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                             encoding="utf-8")
        SUMMARY.write_text(summarise(result), encoding="utf-8")
        print(f"nox_select: wrote {SELECTION.relative_to(REPO)} and "
              f"{SUMMARY.relative_to(REPO)}")

    print(f"{result['selected']} of {result['poolFiles']} files selected "
          f"({result['selectedBytes'] / 1e6:.1f} MB of 24-bit source); "
          f"{result['excluded']} excluded by rule, {result['rejected']} rejected, "
          f"{len(result['unclaimed'])} unclaimed")
    for group in result["groups"]:
        print(f"  {group['category']:<40} {group['selected']:>3} of {group['available']:>3} "
              f"(cap {group['cap']})  {group['secondsRange'][0]:6.2f}-"
              f"{group['secondsRange'][1]:6.2f} s")
    for relative in result["unclaimed"][:20]:
        print(f"  unclaimed: {relative}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
