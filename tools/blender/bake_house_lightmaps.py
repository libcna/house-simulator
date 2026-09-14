#!/usr/bin/env python3
"""Bake the unwrapped canonical shell cell by cell and record one auditable result.

`lightmap_bake.py` owns the Blender bake of one GLB.  This driver owns the house-sized operation:
the exact 78 receiver cells, their per-cell atlas resolutions, the source PNG destination, the
project-owned provenance rows, and a compact report that survives the ignored build tree.

    python3 tools/blender/bake_house_lightmaps.py --artificial
    python3 tools/blender/bake_house_lightmaps.py --daylight
    python3 tools/blender/bake_house_lightmaps.py --artificial --cells L0_FOYER,L0_HALL

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SHELL = REPO / "build" / "shell-lm"
LIGHTS = REPO / "assets-src" / "world" / "layout.lights.json"
CELLS = REPO / "assets-src" / "world" / "layout.cells.json"
OUTPUT = REPO / "assets-src" / "Textures" / "Lightmaps"
META = REPO / "build" / "visual-lightmap-meta"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
BAKER = Path(__file__).with_name("lightmap_bake.py")
SEED = 20260907
SAMPLES = 256

sys.path.insert(0, str(REPO / "tools" / "world"))
import layout_io  # noqa: E402


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def json_sha256(value: object) -> str:
    """Hash semantic JSON, so editing another cell's lamps does not stale this cell's bake."""
    encoded = json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def pack_for(cell: str) -> str:
    if cell.startswith("B1_"):
        return "house-b1"
    if cell.startswith("L1_"):
        return "house-l1"
    if cell.startswith("L2_"):
        return "house-l2"
    if cell.startswith("L3_"):
        return "house-l3"
    return "house-l0"


def update_manifest(images: list[Path], mode: str, size_by_cell: dict[str, int]) -> None:
    """Replace only this generated lightmap family's rows, leaving every other asset untouched."""
    document = json.loads(MANIFEST.read_text(encoding="utf-8"))
    category = f"lightmap-{mode}"
    document["assets"] = [row for row in document["assets"] if row.get("category") != category]
    marker = "_LM_DAY" if mode == "daylight" else "_LM_LG_"
    for path in sorted(images):
        stem = path.stem
        cell = stem.split(marker, 1)[0]
        relative = path.relative_to(REPO).as_posix()
        content_name = path.relative_to(REPO / "assets-src").with_suffix("").as_posix()
        document["assets"].append({
            "id": f"TEXTURE_{stem}",
            "category": category,
            "sourceFile": relative,
            "sourceSha256": sha256(path),
            "texture": {
                "width": size_by_cell[cell],
                "height": size_by_cell[cell],
                "colourSpace": "linear",
                "channelLayout": "RGB normalised irradiance; scale is in the bake report",
            },
            "contentName": content_name,
            "kind": "texture",
            "residencyPack": pack_for(cell),
            "origin": {
                "kind": "generated",
                "name": f"Canonical {mode} lightmap for {cell}",
                "licence": "Ms-PL",
                "licenceFile": "LICENSE",
                "attribution": "",
                "redistributeSource": True,
                "redistributeDerived": True,
                "commercialUse": True,
                "modification": True,
                "note": ("Generated deterministically by tools/blender/bake_house_lightmaps.py "
                         f"with seed {SEED} from the project-owned canonical shell and light rows."),
            },
        })
    MANIFEST.write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def object_end(text: str, start: int) -> int:
    """Return one past the object at *start*, ignoring braces in strings and JSONC comments."""
    depth = 0
    quote = False
    escaped = False
    line_comment = False
    block_comment = False
    index = start
    while index < len(text):
        char = text[index]
        following = text[index + 1] if index + 1 < len(text) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
        elif block_comment:
            if char == "*" and following == "/":
                block_comment = False
                index += 1
        elif quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quote = False
        elif char == '"':
            quote = True
        elif char == "/" and following == "/":
            line_comment = True
            index += 1
        elif char == "/" and following == "*":
            block_comment = True
            index += 1
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return index + 1
        index += 1
    raise ValueError("unterminated JSON object")


def set_lightmap_field(text: str, cell: str, binding: dict) -> str:
    """Insert or replace one generated cell field without discarding authored JSONC comments."""
    needle = f'"id": "{cell}"'
    identifier = text.find(needle)
    if identifier < 0:
        raise ValueError(f"layout.cells.json has no {cell} row")
    row_start = text.rfind("{", 0, identifier)
    row_end = object_end(text, row_start)
    row = text[row_start:row_end]
    formatted = json.dumps(binding, indent=2, ensure_ascii=False).replace("\n", "\n      ")
    field = f'      "lightmaps": {formatted},\n'

    existing = row.find('"lightmaps"')
    if existing >= 0:
        line_start = row.rfind("\n", 0, existing) + 1
        value_start = row.find("{", existing)
        value_end = object_end(row, value_start)
        comma_end = value_end + (1 if row[value_end:value_end + 1] == "," else 0)
        if row[comma_end:comma_end + 1] == "\n":
            comma_end += 1
        row = row[:line_start] + field + row[comma_end:]
    else:
        residency = row.find('"residencyPack"')
        if residency < 0:
            raise ValueError(f"{cell} has no residencyPack insertion point")
        line_start = row.rfind("\n", 0, residency) + 1
        row = row[:line_start] + field + row[line_start:]
    return text[:row_start] + row + text[row_end:]


def update_cell_bindings() -> None:
    """Merge the two durable bake reports into the canonical cell-owned runtime bindings."""
    reports = {}
    for mode in ("daylight", "artificial"):
        path = REPO / "docs" / "lightmaps" / f"{mode}-bake.json"
        if not path.is_file():
            return
        reports[mode] = json.loads(path.read_text(encoding="utf-8"))
        if reports[mode].get("cells") != 78:
            return
    daylight = {row["cell"]: row for row in reports["daylight"]["products"]}
    artificial = {row["cell"]: row for row in reports["artificial"]["products"]}
    if set(daylight) != set(artificial):
        raise ValueError("daylight and artificial reports name different receiver cells")

    text = CELLS.read_text(encoding="utf-8")
    for cell in sorted(daylight):
        day = daylight[cell]
        art = artificial[cell]
        if day["shellHash"] != art["shellHash"]:
            raise ValueError(f"{cell} daylight/artificial shell hashes disagree")
        day_product = day.get("daylight")
        binding = {
            "shellHash": day["shellHash"],
            "daylight": ({
                "contentName": f"Textures/Lightmaps/Daylight/{Path(day_product['image']).stem}",
                "scale": day_product["scale"],
            } if day_product else None),
            "artificial": [{
                "group": group["group"],
                "contentName": f"Textures/Lightmaps/Artificial/{Path(group['image']).stem}",
                "scale": group["scale"],
            } for group in art.get("groups", [])],
        }
        text = set_lightmap_field(text, cell, binding)
    CELLS.write_text(text, encoding="utf-8")


def unlit_products(sidecar: dict, mode: str) -> list[str]:
    """Name any baked product whose measured irradiance is effectively black."""
    if mode == "artificial":
        return [entry["group"] for entry in sidecar.get("groups", [])
                if float(entry.get("peak", 0.0)) <= 1e-6]
    daylight = sidecar.get("daylight")
    if not isinstance(daylight, dict) or float(daylight.get("scale", 0.0)) <= 1e-6:
        return ["LM_DAY"]
    return []


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--artificial", action="store_true")
    modes.add_argument("--daylight", action="store_true")
    parser.add_argument("--cells", help="comma-separated subset for an inspection bake")
    parser.add_argument("--samples", type=int, default=SAMPLES)
    parser.add_argument("--seed", type=int, default=SEED)
    parser.add_argument("--resume", action="store_true",
                        help="reuse matching completed sidecars after an interrupted house bake")
    args = parser.parse_args()

    report_path = SHELL / "report.json"
    if not report_path.is_file():
        print("bake_house_lightmaps: build/shell-lm/report.json is missing; run shell_unwrap.py",
              file=sys.stderr)
        return 1
    unwrap = json.loads(report_path.read_text(encoding="utf-8"))
    per_cell = {row["cell"]: int(row["atlas"]) for row in unwrap.get("perCell", [])}
    if len(per_cell) != 78:
        print(f"bake_house_lightmaps: expected 78 receiver cells, found {len(per_cell)}", file=sys.stderr)
        return 1

    light_document = json.loads(layout_io.strip_jsonc(LIGHTS.read_text(encoding="utf-8")))
    lights_by_cell = {
        cell: [row for row in light_document.get("lights", []) if row.get("cell") == cell]
        for cell in per_cell
    }

    wanted = set(per_cell)
    if args.cells:
        requested = {name.strip() for name in args.cells.split(",") if name.strip()}
        unknown = sorted(requested - wanted)
        if unknown:
            print(f"bake_house_lightmaps: unknown receiver cell(s): {', '.join(unknown)}", file=sys.stderr)
            return 2
        wanted = requested
    full_house = wanted == set(per_cell)

    mode = "daylight" if args.daylight else "artificial"
    destination = OUTPUT / ("Daylight" if args.daylight else "Artificial")
    destination.mkdir(parents=True, exist_ok=True)
    META.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    sidecars = []
    reused_cells = 0
    for ordinal, cell in enumerate(sorted(wanted), 1):
        print(f"bake_house_lightmaps: [{ordinal:02d}/{len(wanted):02d}] {cell} {mode}", flush=True)
        shell = SHELL / f"{cell}.glb"
        if not shell.is_file():
            print(f"bake_house_lightmaps: missing {shell.relative_to(REPO)}", file=sys.stderr)
            return 1
        source_hash = sha256(shell)
        lights_hash = json_sha256(lights_by_cell[cell])
        baker_hash = sha256(BAKER)
        meta_path = META / f"{cell}_{mode}.json"
        if args.resume and meta_path.is_file():
            previous = json.loads(meta_path.read_text(encoding="utf-8"))
            products = ([entry["image"] for entry in previous.get("groups", [])]
                        if args.artificial else
                        ([previous["daylight"]["image"]] if previous.get("daylight") else []))
            if (previous.get("seed") == args.seed and previous.get("samples") == args.samples
                    and previous.get("size") == per_cell[cell]
                    and previous.get("sourceGlbSha256") == source_hash
                    and previous.get("cellLightsSha256") == lights_hash
                    and previous.get("bakerSha256") == baker_hash
                    and all((destination / name).is_file() for name in products)):
                dark = unlit_products(previous, mode)
                if dark:
                    print(f"bake_house_lightmaps: {cell} has unlit cached product(s): "
                          f"{', '.join(dark)}", file=sys.stderr)
                    return 1
                print(f"bake_house_lightmaps: reused {len(products)} completed product(s)")
                sidecars.append(previous)
                reused_cells += 1
                continue
        command = [sys.executable, str(BAKER), str(shell), "--lights", str(LIGHTS),
                   "--cell", cell, "--out", str(destination), "--size", str(per_cell[cell]),
                   "--samples", str(args.samples), "--seed", str(args.seed),
                   "--daylight-only" if args.daylight else "--artificial-only"]
        completed = subprocess.run(command, cwd=REPO, check=False)
        if completed.returncode != 0:
            return completed.returncode
        generated_meta = destination / f"{cell}_LM.json"
        generated_meta.replace(meta_path)
        sidecar = json.loads(meta_path.read_text(encoding="utf-8"))
        sidecar["sourceGlbSha256"] = source_hash
        sidecar["cellLightsSha256"] = lights_hash
        sidecar["bakerSha256"] = baker_hash
        meta_path.write_text(json.dumps(sidecar, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        dark = unlit_products(sidecar, mode)
        if dark:
            print(f"bake_house_lightmaps: {cell} produced unlit product(s): "
                  f"{', '.join(dark)}", file=sys.stderr)
            return 1
        sidecars.append(sidecar)

    marker = "*_LM_DAY.png" if args.daylight else "*_LM_LG_*.png"
    images = sorted(destination.glob(marker))
    expected = len(wanted) if args.daylight else sum(len(row["groups"]) for row in sidecars)
    if len(images) < expected:
        print(f"bake_house_lightmaps: expected at least {expected} {mode} images, found {len(images)}",
              file=sys.stderr)
        return 1

    selected_images = [path for path in images
                       if path.stem.split("_LM_", 1)[0] in wanted]
    elapsed = time.monotonic() - started
    docs = REPO / "docs" / "lightmaps"
    durable = docs / f"{mode}-bake.json"
    previous_full_seconds = None
    if durable.is_file():
        previous_full_seconds = json.loads(durable.read_text(encoding="utf-8")).get(
            "fullBakeSeconds")
    report = {
        "tool": "bake_house_lightmaps.py",
        "mode": mode,
        "seed": args.seed,
        "samples": args.samples,
        "cells": len(wanted),
        "atlases": len(selected_images),
        "groups": sum(len(row["groups"]) for row in sidecars),
        "atlasSizes": sorted({per_cell[cell] for cell in wanted}),
        "sourceBytes": sum(path.stat().st_size for path in selected_images),
        "rgba8Bytes": sum(per_cell[cell] ** 2 * 4 for cell in wanted)
        if args.daylight else sum(per_cell[row["cell"]] ** 2 * len(row["groups"])
                                   * 4 for row in sidecars),
        "invocationSeconds": round(elapsed, 3),
        "reusedCells": reused_cells,
        "products": [{
            "cell": row["cell"],
            "shellHash": row["shellHash"],
            "groups": row["groups"],
            "daylight": row["daylight"],
        } for row in sidecars],
    }
    if full_house and reused_cells == 0:
        report["fullBakeSeconds"] = round(elapsed, 3)
    elif previous_full_seconds is not None:
        report["fullBakeSeconds"] = previous_full_seconds
    if args.artificial:
        report["emptyArtificialCells"] = sum(not row["groups"] for row in sidecars)
    if full_house:
        docs.mkdir(parents=True, exist_ok=True)
        durable.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        update_manifest(selected_images, mode, per_cell)
        update_cell_bindings()
    else:
        subset_report = META / f"{mode}-subset-report.json"
        subset_report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                                 encoding="utf-8")
    print(f"bake_house_lightmaps: {len(wanted)} cells, {len(selected_images)} atlas(es), "
          f"{report['sourceBytes'] / 1e6:.2f} MB PNG in {elapsed:.1f} s")
    if full_house:
        print(f"bake_house_lightmaps: wrote {durable.relative_to(REPO)} and updated asset provenance")
    else:
        print("bake_house_lightmaps: inspection subset left the durable report and manifest alone")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
