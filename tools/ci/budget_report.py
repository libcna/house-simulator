#!/usr/bin/env python3
"""budget_report.py -- what each content pack costs, measured, with the unknowns named.

`HOUSE-00203`. `cna-house.md` §27.2 gives every pack a size budget and §72 gives the project a
memory budget; §80.1 and §80.2 both name this tool as the thing that proves them. A budget nobody
measures is a wish, and a budget measured by a tool that guesses is worse than no budget at all --
so the rule this file is built around is:

**Nothing is estimated. A number that cannot be measured is printed as `--` and counted.**

That matters most now, when almost every pack is empty. A report that quietly filled an unmeasured
row with 0 would say the project is comfortably inside every budget, which is true and useless; a
report that says "3 of 14 rows could not be measured, and here is why" stays honest as the tree
grows from forty kilobytes to a gigabyte.

## What is measured, and from what

| Column | Source | When it is unknown |
|---|---|---|
| source bytes | the file under `assets-src/` | never; a row whose source is missing is an error |
| compiled bytes | `<content-root>/<contentName>.cnb`, `.xnb` for effects, plus the streamed media file for a video | no build tree was given, or the asset has not been built |
| texture memory | the PNG's own `IHDR`, at RGBA8 -- CNB texture schema 1 is frozen to `Rgba8` (`HOUSE-00111`) | the source is not a PNG |
| triangles | the glTF's index accessors, `mode == TRIANGLES` | the source is not a glTF, or a primitive is non-indexed |
| audio seconds | the WAV's own `fmt ` and `data` chunks | the source is not a RIFF/WAVE file |
| video seconds | `ffprobe`, when it is installed | `ffprobe` is absent |

**Texture memory is the base level only.** A mip chain adds up to a third, and whether one is
generated is per-asset `.cna-content.json` configuration that this report does not read -- so the
number is labelled for what it is rather than inflated by a factor nobody chose.

## The committed report

`docs/budget-report.md` is generated **from the manifest alone**, with no build tree, and committed
— the same arrangement `licenses/THIRD-PARTY-ASSETS.md` uses, for the same reason: a generated
document nobody can see is a document nobody reads, and one nobody checks goes stale. `--check`
regenerates it in memory and fails if the committed copy disagrees, and it needs no build tree, so
it can be an ordinary gate in `run_checks.sh`.

The compiled column is therefore `--` in the committed copy. That is deliberate: the compiled size
depends on which build tree you point at, and a document whose contents depended on a directory
outside the repository could not be checked at all. Pass `--content`/`--effects` when you want the
numbers the pack budgets are actually written against.

    tools/ci/budget_report.py                                  the table, from the manifest alone
    tools/ci/budget_report.py --content build/content --effects build/content-fx
    tools/ci/budget_report.py --emit                           write docs/budget-report.md
    tools/ci/budget_report.py --check                          fail if that file is stale
    tools/ci/budget_report.py --enforce --content build/content   fail if a pack is over budget
    tools/ci/budget_report.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "assets"))
import gltf_io  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
REPORT = REPO / "docs" / "budget-report.md"

#: Bytes per texel of a sampled texture. CNB texture schema 1 is frozen to `Rgba8` and a
#: `textureFormat` asking for DXT is warned about and silently kept uncompressed (`HOUSE-00111`),
#: so 4 is the honest figure for the `.cnb` tree. A future `.xnb` DXT profile divides it by 4 or 8,
#: and this constant is where that will be read from the profile rather than assumed.
BYTES_PER_TEXEL = 4

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


class Unknown:
    """A value that could not be measured, and the reason.

    A class rather than `None` so a reason travels with the absence: "no build tree" and "the
    source is not a PNG" are different problems and a reader needs to know which one they have.
    """

    __slots__ = ("reason",)

    def __init__(self, reason: str):
        self.reason = reason

    def __repr__(self) -> str:  # pragma: no cover - diagnostics only
        return f"Unknown({self.reason!r})"


class NotApplicable(Unknown):
    """A measurement the question does not have, as opposed to one that failed.

    A font has no triangle count and a model has no duration. Printing those as `--` alongside "the
    build tree does not contain this asset" would bury the second in eighty instances of the first,
    which is exactly how a report stops being read. They print as a middle dot and are summarised
    in one line instead of listed.
    """


def png_dimensions(path: Path) -> tuple[int, int] | Unknown:
    """Width and height from the PNG's own `IHDR`, without decoding it."""
    try:
        with path.open("rb") as handle:
            header = handle.read(24)
    except OSError as error:
        return Unknown(f"could not be read: {error}")
    if len(header) < 24 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
        return Unknown("not a PNG")
    width, height = struct.unpack_from(">II", header, 16)
    return width, height


def wav_seconds(path: Path) -> float | Unknown:
    """Duration from the WAV's own chunks. Parsed rather than decoded: a multi-hundred-megabyte
    audio tree measured by decoding would take minutes to produce a number that is in the header."""
    try:
        data = path.read_bytes()
    except OSError as error:
        return Unknown(f"could not be read: {error}")
    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        return Unknown("not a RIFF/WAVE file")

    offset = 12
    rate = channels = bits = 0
    frames_bytes = 0
    while offset + 8 <= len(data):
        tag = data[offset:offset + 4]
        (size,) = struct.unpack_from("<I", data, offset + 4)
        body = offset + 8
        if tag == b"fmt " and size >= 16:
            _, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", data, body)
        elif tag == b"data":
            frames_bytes = min(size, len(data) - body)
        # Chunks are 2-byte aligned; a chunk of odd length is followed by a pad byte.
        offset = body + size + (size & 1)

    if rate <= 0 or channels <= 0 or bits <= 0:
        return Unknown("the WAV has no usable 'fmt ' chunk")
    return frames_bytes / (rate * channels * (bits // 8))


def gltf_triangles(path: Path) -> int | Unknown:
    """Triangles from the index accessors' counts. No buffer is read: glTF requires the count on
    the accessor, so the number is in the JSON."""
    try:
        document, _ = gltf_io.read_model(path)
    except (gltf_io.GltfError, OSError, ValueError) as error:
        return Unknown(f"not a readable glTF: {error}")

    accessors = document.get("accessors", [])
    total = 0
    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            if primitive.get("mode", 4) != 4:
                return Unknown("a primitive is not a triangle list")
            index = primitive.get("indices")
            if index is None:
                # Non-indexed geometry is legal glTF and this project does not produce it; refusing
                # to count it is better than dividing a vertex count by three and hoping.
                return Unknown("a primitive is not indexed")
            total += int(accessors[index]["count"]) // 3
    return total


def video_seconds(path: Path) -> float | Unknown:
    if shutil.which("ffprobe") is None:
        return Unknown("ffprobe is not installed")
    result = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of",
         "default=nw=1:nk=1", str(path)],
        capture_output=True, text=True, check=False)
    if result.returncode != 0 or not result.stdout.strip():
        return Unknown("ffprobe could not read a duration")
    try:
        return float(result.stdout.strip())
    except ValueError:
        return Unknown(f"ffprobe returned {result.stdout.strip()!r}")


def compiled_paths(row: dict, content: Path | None, effects: Path | None) -> list[Path]:
    """Where this asset's compiled form lives, or an empty list if no tree was given.

    An effect is `.xnb` under the effect root and everything else is `.cnb` under the content root
    -- two roots, because `HOUSE-00064` measured that `.xnb` wins the resolution order and a single
    tree lets a stale one shadow what the build just produced. A video is TWO files: the metadata
    `.cnb` and the streamed media beside it, and counting only the first would report a 25 MB pack
    as a few kilobytes.
    """
    name = row.get("contentName", "")
    if not name:
        return []
    if row.get("kind") == "effect":
        return [] if effects is None else [effects / f"{name}.xnb"]
    if content is None:
        return []
    paths = [content / f"{name}.cnb"]
    if row.get("kind") == "video":
        source_suffix = Path(row.get("sourceFile", "")).suffix
        if source_suffix:
            paths.append(content / f"{name}{source_suffix}")
    return paths


def measure(document: dict, content: Path | None, effects: Path | None) -> list[dict]:
    rows = []
    for row in document.get("assets", []):
        if "notPackaged" in row:
            continue
        source = REPO / row["sourceFile"]
        kind = row.get("kind", "")
        entry: dict = {
            "id": row["id"],
            "pack": row.get("residencyPack", ""),
            "kind": kind,
            "contentName": row.get("contentName", ""),
            "sourceBytes": source.stat().st_size if source.is_file()
            else Unknown("the source file is missing"),
            "compiledBytes": Unknown("no build tree was given"),
            "textureBytes": NotApplicable(f"{kind}s have no texture memory"),
            "triangles": NotApplicable(f"{kind}s have no triangles"),
            "audioSeconds": NotApplicable(f"{kind}s have no duration"),
        }

        targets = compiled_paths(row, content, effects)
        if targets:
            missing = [t for t in targets if not t.is_file()]
            if missing:
                entry["compiledBytes"] = Unknown(
                    f"not built: {', '.join(str(m.name) for m in missing)}")
            else:
                entry["compiledBytes"] = sum(t.stat().st_size for t in targets)

        if kind == "texture":
            size = png_dimensions(source)
            entry["textureBytes"] = (Unknown(size.reason) if isinstance(size, Unknown)
                                     else size[0] * size[1] * BYTES_PER_TEXEL)
            if not isinstance(size, Unknown):
                entry["dimensions"] = f"{size[0]}x{size[1]}"
        elif kind == "model":
            entry["triangles"] = gltf_triangles(source)
        elif kind == "sound":
            entry["audioSeconds"] = wav_seconds(source)
        elif kind == "video":
            entry["audioSeconds"] = video_seconds(source)
        elif kind == "font":
            # A `SpriteFont` IS a texture atlas and does occupy GPU memory, so this is a genuine
            # unknown rather than a question that does not apply: the atlas dimensions live inside
            # the compiled `.cnb`, which this report does not parse. Saying so is better than
            # calling it not applicable, which would quietly leave real texture memory out of §72.
            entry["textureBytes"] = Unknown(
                "a SpriteFont's atlas dimensions are inside the compiled .cnb, which this report "
                "does not parse")

        rows.append(entry)
    rows.sort(key=lambda entry: (entry["pack"], entry["id"]))
    return rows


def total(rows: list[dict], field: str) -> tuple[float, int]:
    """(sum of the measured values, count of the unmeasured ones)."""
    measured = 0.0
    unknown = 0
    for row in rows:
        value = row[field]
        if isinstance(value, Unknown):
            unknown += 1
        else:
            measured += value
    return measured, unknown


def cell(value, formatter=lambda v: f"{v:,.0f}") -> str:
    if isinstance(value, NotApplicable):
        return "·"
    if isinstance(value, Unknown):
        return "--"
    return formatter(value)


def megabytes(value) -> str:
    return "--" if isinstance(value, Unknown) else f"{value / 1e6:.2f}"


def render(document: dict, rows: list[dict], content: Path | None,
           effects: Path | None) -> tuple[str, list[str]]:
    """The Markdown report, and the list of packs that are over budget."""
    packs = document.get("packs", [])
    over: list[str] = []
    lines: list[str] = []

    lines.append("# Content budget report")
    lines.append("")
    lines.append("Generated by `tools/ci/budget_report.py` from `assets-src/assets.manifest.json`.")
    lines.append("**Every number here is measured. `--` means it could not be measured, never "
                 "zero** -- the reasons are listed at the end.")
    lines.append("")
    if content is None and effects is None:
        lines.append("> No build tree was given, so the compiled column is empty throughout. Run "
                     "with `--content build/content --effects build/content-fx` after a build for "
                     "the numbers the budgets are actually written against.")
        lines.append("")

    lines.append("## By pack")
    lines.append("")
    lines.append("| Pack | Ship | Assets | Source MB | Compiled MB | Budget MB | Used | "
                 "Texture MB | Triangles | Audio s |")
    lines.append("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|")

    for pack in packs:
        pack_id = pack.get("id", "")
        inside = [row for row in rows if row["pack"] == pack_id]
        source_bytes, source_unknown = total(inside, "sourceBytes")
        compiled_bytes, compiled_unknown = total(inside, "compiledBytes")
        texture_bytes, _ = total(inside, "textureBytes")
        triangles, _ = total(inside, "triangles")
        audio, _ = total(inside, "audioSeconds")
        budget = pack.get("budgetBytes", 0)

        # The budget is compared against the COMPILED size, because that is what ships. When any
        # row in the pack is unbuilt the comparison is suppressed rather than made against a
        # partial sum -- a pack reported at 12 % of budget because half of it was not built is the
        # kind of reassurance that gets a project into trouble.
        if compiled_unknown or not inside:
            used = "--"
        else:
            fraction = compiled_bytes / budget if budget else 0.0
            used = f"{fraction * 100:.1f}%"
            if fraction > 1.0:
                # Bytes below a megabyte, or a 100-byte budget breached by 4 000 bytes reads as
                # "0.0 MB against a 0 MB budget" and tells the reader nothing.
                def amount(value: float) -> str:
                    return f"{value / 1e6:.1f} MB" if value >= 1e6 else f"{value:,.0f} B"

                over.append(f"{pack_id}: {amount(compiled_bytes)} compiled against a "
                            f"{amount(budget)} budget ({fraction * 100:.0f}%)")

        lines.append(
            f"| `{pack_id}` | {'yes' if pack.get('shipped') else '**no**'} | {len(inside)} | "
            f"{source_bytes / 1e6:.2f}{'+' if source_unknown else ''} | "
            f"{(compiled_bytes / 1e6):.2f}{'+' if compiled_unknown else ''} | "
            f"{budget / 1e6:.0f} | {used} | "
            f"{texture_bytes / 1e6:.2f} | {triangles:,.0f} | {audio:.2f} |")

    shipped_ids = {p.get("id") for p in packs if p.get("shipped")}
    shipped_rows = [row for row in rows if row["pack"] in shipped_ids]
    shipped_source, _ = total(shipped_rows, "sourceBytes")
    all_source, _ = total(rows, "sourceBytes")
    lines.append("")
    lines.append(f"**Shipped total** {shipped_source / 1e6:.2f} MB of source across "
                 f"{len(shipped_rows)} asset(s); {(all_source - shipped_source) / 1e6:.2f} MB more "
                 f"in packs marked not shipped.")
    lines.append("")
    lines.append("A `+` after a total means at least one row in that pack could not be measured, "
                 "so the true figure is higher.")
    lines.append("")

    lines.append("## By asset")
    lines.append("")
    lines.append("| Pack | Asset | Kind | Content name | Source B | Compiled B | Texture B | "
                 "Triangles | Seconds |")
    lines.append("|---|---|---|---|---:|---:|---:|---:|---:|")
    for row in rows:
        lines.append(
            f"| `{row['pack']}` | `{row['id']}` | {row['kind']} | `{row['contentName']}` | "
            f"{cell(row['sourceBytes'])} | {cell(row['compiledBytes'])} | "
            f"{cell(row['textureBytes'])} | {cell(row['triangles'])} | "
            f"{cell(row['audioSeconds'], lambda v: f'{v:.3f}')} |")
    lines.append("")

    reasons: dict[str, int] = {}
    inapplicable = 0
    for row in rows:
        for field in ("sourceBytes", "compiledBytes", "textureBytes", "triangles", "audioSeconds"):
            value = row[field]
            if isinstance(value, NotApplicable):
                inapplicable += 1
            elif isinstance(value, Unknown):
                reasons[value.reason] = reasons.get(value.reason, 0) + 1
    lines.append("## What could not be measured")
    lines.append("")
    lines.append(f"`·` is a question that does not apply to that kind of asset — a font has no "
                 f"triangles — and there are {inapplicable} of them. `--` is a measurement that "
                 f"was attempted and failed, and every one is listed here:")
    lines.append("")
    if not reasons:
        lines.append("Nothing: every applicable cell above is a measurement.")
    else:
        lines.append("| Reason | Cells |")
        lines.append("|---|---:|")
        for reason in sorted(reasons):
            lines.append(f"| {reason} | {reasons[reason]} |")
    lines.append("")
    lines.append("## Notes")
    lines.append("")
    lines.append(f"* **Texture memory is the base level only**, at {BYTES_PER_TEXEL} bytes per "
                 f"texel. A mip chain adds up to a third; whether one is generated is per-asset "
                 f"`.cna-content.json` configuration this report does not read. CNB texture "
                 f"schema 1 is frozen to `Rgba8` (`HOUSE-00111`), so no compression is assumed.")
    lines.append("* **A video counts its streamed media file as well as its metadata `.cnb`.** "
                 "CNA deploys the media beside the metadata rather than embedding it, so counting "
                 "only the `.cnb` would report a television pack as a few kilobytes.")
    lines.append("* Budgets come from `cna-house.md` §27.2, declared in the manifest's `packs` "
                 "block so that this report and the residency system read one list.")
    return "\n".join(lines) + "\n", over


def build(content: Path | None, effects: Path | None) -> tuple[str, list[str]]:
    document = json.loads(MANIFEST.read_text(encoding="utf-8"))
    rows = measure(document, content, effects)
    return render(document, rows, content, effects)


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

    global MANIFEST, REPO
    original_manifest = MANIFEST
    original_repo = REPO
    workspace = Path(tempfile.mkdtemp(prefix="budget_report_selftest_"))
    try:
        sys.path.insert(0, str(REPO / "tools" / "assets"))
        import make_fallback_assets as fallback

        assets = workspace / "assets-src"
        (assets / "Textures").mkdir(parents=True)
        (assets / "Models").mkdir(parents=True)
        (assets / "Audio").mkdir(parents=True)

        # A 32x16 texture: 512 texels, 2048 bytes at RGBA8. Non-square on purpose -- w*h and h*w
        # are the same number for a square, so a square fixture cannot catch a transposed read.
        fallback.write_png(str(assets / "Textures" / "t.png"), 32, 16,
                           lambda x, y: (1, 2, 3, 255))
        # The fallback cube: 6 faces x 2 triangles = 12.
        fallback.write_glb(str(assets / "Models" / "m.glb"))
        # Exactly half a second at 44 100 Hz mono 16-bit.
        fallback.write_wav(str(assets / "Audio" / "s.wav"), seconds=0.5)
        # Not a PNG, not a glTF, not a WAV: the row whose measurements must come back Unknown.
        (assets / "Textures" / "bad.png").write_bytes(b"this is not a png")

        def row(asset_id, kind, name, source, pack):
            return {"id": asset_id, "category": "fixture", "sourceFile": source,
                    "sourceSha256": "0" * 64, "contentName": name, "kind": kind,
                    "residencyPack": pack,
                    "origin": {"kind": "generated", "name": asset_id, "licence": "Ms-PL",
                               "licenceFile": "LICENSE", "redistributeSource": True,
                               "redistributeDerived": True, "commercialUse": True,
                               "modification": True}}

        document = {
            "schema": "cna-house/assets/1",
            "packs": [
                {"id": "core", "budgetBytes": 1_000_000, "shipped": True, "contents": "x"},
                {"id": "tiny", "budgetBytes": 100, "shipped": True, "contents": "y"},
                # `partial` exists to make the partial-sum guard LOAD-BEARING. It holds one asset
                # that is built and one that is not, and its budget is smaller than the built one
                # alone -- so a report that compared a pack against the rows it happened to find
                # would call it over budget while half of it was still unbuilt, and a report that
                # suppresses the comparison says nothing. Without this pack the guard could be
                # deleted and every claim below would still pass.
                {"id": "partial", "budgetBytes": 1_000, "shipped": True, "contents": "w"},
                {"id": "empty", "budgetBytes": 500_000, "shipped": False, "contents": "z"},
            ],
            "assets": [
                row("TEX", "texture", "Textures/t", "Textures/t.png", "partial"),
                row("TEX2", "texture", "Textures/t2", "Textures/t.png", "partial"),
                row("MODEL", "model", "Models/m", "Models/m.glb", "core"),
                row("SND", "sound", "Audio/s", "Audio/s.wav", "core"),
                row("BAD", "texture", "Textures/bad", "Textures/bad.png", "tiny"),
                {"id": "TTF", "category": "font", "sourceFile": "Textures/t.png",
                 "sourceSha256": "0" * 64,
                 "notPackaged": "a build input", "origin": {}},
            ],
        }
        manifest_path = workspace / "manifest.json"
        manifest_path.write_text(json.dumps(document), encoding="utf-8")

        # The measurement functions resolve sources against REPO, so point both at the workspace.
        REPO = workspace / "assets-src"
        MANIFEST = manifest_path

        rows = measure(document, None, None)

        # 1. A row that is not packaged is not a budget row.
        require(len(rows) == 5 and all(r["id"] != "TTF" for r in rows),
                f"a notPackaged row is left out of the budget ({len(rows)} rows)")

        by_id = {r["id"]: r for r in rows}

        # 2/3/4. The three measurements, against values known by construction.
        require(by_id["TEX"]["textureBytes"] == 32 * 16 * 4,
                f"a 32x16 RGBA8 texture is {by_id['TEX']['textureBytes']} bytes of texture memory")
        require(by_id["TEX"].get("dimensions") == "32x16",
                f"...and its dimensions are read the right way round "
                f"({by_id['TEX'].get('dimensions')})")
        require(by_id["MODEL"]["triangles"] == 12,
                f"a six-faced cube is {by_id['MODEL']['triangles']} triangles")
        seconds = by_id["SND"]["audioSeconds"]
        require(not isinstance(seconds, Unknown) and abs(seconds - 0.5) < 1e-6,
                f"a half-second WAV measures {seconds}")

        # 5. The KEY claim: an unmeasurable value is Unknown, never 0.
        require(isinstance(by_id["BAD"]["textureBytes"], Unknown),
                "a file that is not a PNG yields Unknown texture memory, not 0")
        require("not a PNG" in by_id["BAD"]["textureBytes"].reason,
                f"...and the reason says why: {by_id['BAD']['textureBytes'].reason}")
        require(isinstance(by_id["TEX"]["triangles"], Unknown)
                and isinstance(by_id["MODEL"]["audioSeconds"], Unknown),
                "a kind that cannot have a measurement reports Unknown for it")

        # 6. Totals separate the measured sum from the count that could not be measured.
        measured, unknown = total(rows, "textureBytes")
        require(measured == 2 * 32 * 16 * 4 and unknown == 3,
                f"the texture total is {measured} over {len(rows)} rows with {unknown} "
                f"unmeasured or inapplicable")

        # 7. Without a build tree, EVERY compiled cell is Unknown -- not 0, which would report the
        #    whole project as free.
        require(all(isinstance(r["compiledBytes"], Unknown) for r in rows),
                "with no build tree, every compiled size is Unknown")
        text, over = render(document, rows, None, None)
        require(not over, "...and no pack is declared over budget on the strength of nothing")

        # 8. With a build tree, a MISSING compiled file is Unknown rather than 0.
        content = workspace / "content"
        (content / "Textures").mkdir(parents=True)
        (content / "Models").mkdir(parents=True)
        (content / "Audio").mkdir(parents=True)
        (content / "Textures" / "t.cnb").write_bytes(b"x" * 5000)
        (content / "Models" / "m.cnb").write_bytes(b"x" * 3000)
        rows = measure(document, content, None)
        by_id = {r["id"]: r for r in rows}
        require(by_id["TEX"]["compiledBytes"] == 5000 and by_id["MODEL"]["compiledBytes"] == 3000,
                "a built asset's compiled size is read from the build tree")
        require(isinstance(by_id["SND"]["compiledBytes"], Unknown)
                and "not built" in by_id["SND"]["compiledBytes"].reason,
                f"an UNBUILT asset is Unknown, not 0: {by_id['SND']['compiledBytes'].reason}")

        text, over = render(document, rows, content, None)
        # `partial` holds a 5 000-byte built asset against a 1 000-byte budget and one unbuilt row.
        # Comparing it now would report it at 500 % while half of it does not exist yet.
        require(not over,
                f"a pack with an unbuilt row is not compared against its budget at all, rather "
                f"than compared against a partial sum ({over})")

        # 9. ...and once everything in a pack is built, the comparison happens and FIRES.
        (content / "Audio" / "s.cnb").write_bytes(b"x" * 2000)
        (content / "Textures" / "bad.cnb").write_bytes(b"x" * 4000)
        rows = measure(document, content, None)
        text, over = render(document, rows, content, None)
        require(sorted(o.split(":")[0] for o in over) == ["tiny"],
                f"the 100-byte pack holding a 4 000-byte asset is reported over budget, and only "
                f"it ({over})")
        require("| `core` |" in text and "10000" not in text.replace(",", ""),
                "the core pack's row is present")
        require("`empty` | **no**" in text,
                "a pack marked not shipped is called out in the table")

        # 10. Determinism: the same inputs produce the same bytes.
        again, _ = render(document, measure(document, content, None), content, None)
        require(text == again, "two runs produce byte-identical Markdown")

        # 11. Every unmeasured cell is accounted for in the report's own reasons table.
        require("## What could not be measured" in text and "not a PNG" in text,
                "the report lists why each unmeasured cell is unmeasured")

    finally:
        MANIFEST = original_manifest
        REPO = original_repo
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("budget_report: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--content", type=Path, help="a built .cnb tree, e.g. build/content")
    parser.add_argument("--effects", type=Path, help="a built .xnb tree, e.g. build/content-fx")
    parser.add_argument("--out", type=Path, help="write the Markdown here instead of stdout")
    parser.add_argument("--emit", action="store_true",
                        help=f"write {REPORT.relative_to(REPO)}")
    parser.add_argument("--check", action="store_true",
                        help=f"fail if {REPORT.relative_to(REPO)} is stale or hand-edited")
    parser.add_argument("--enforce", action="store_true",
                        help="fail if a pack's compiled size exceeds its budget")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    if args.check:
        # No build tree, deliberately: the committed copy is the manifest-only one, so this gate
        # gives the same answer on a machine that has never run a build.
        expected, _ = build(None, None)
        if not REPORT.is_file():
            print(f"budget_report: {REPORT.relative_to(REPO)} does not exist; "
                  f"generate it with --emit", file=sys.stderr)
            return 1
        if REPORT.read_text(encoding="utf-8") != expected:
            print(f"budget_report: {REPORT.relative_to(REPO)} is stale or hand-edited.\n"
                  f"  Regenerate it with tools/ci/budget_report.py --emit and commit the result.",
                  file=sys.stderr)
            return 1
        print(f"budget_report: {REPORT.relative_to(REPO)} matches the manifest.")
        return 0

    text, over = build(args.content, args.effects)
    if args.emit:
        REPORT.write_text(text, encoding="utf-8")
        print(f"budget_report: wrote {REPORT.relative_to(REPO)}")
    elif args.out is not None:
        args.out.write_text(text, encoding="utf-8")
        print(f"budget_report: wrote {args.out}")
    else:
        print(text, end="")

    for problem in over:
        print(f"budget_report: OVER BUDGET -- {problem}", file=sys.stderr)
    return 1 if (args.enforce and over) else 0


if __name__ == "__main__":
    sys.exit(main())
