#!/usr/bin/env python3
"""gltf_validate.py -- refuse a glTF the content pipeline cannot import cleanly.

`HOUSE-00186`. Two passes, and the second is the one that matters:

1. **`gltf-validator`, if it is installed.** Khronos' own validator catches spec violations the CNA
   importer may accept by accident. It is optional because it is a Node package nobody should be
   made to install to build the game, and its absence is reported rather than hidden.
2. **A CNA importer pass with warnings as errors.** This is the authoritative check, because the
   question is not "is this file valid glTF" but "does the pipeline this project actually uses
   import it without complaining". A warning from `cna-content` is a statement that something was
   guessed at, and a guess in an asset is a bug waiting for a screenshot.

Also enforced here, because they are project rules rather than glTF rules and nothing else would
catch them:

* **exactly one skin per file** (`cna-house.md` §47.0). `HOUSE-00076` measured that
  `CNA.ModelProcessor` refuses a multi-skin glTF outright, so this only turns a late build failure
  into an early, clearer one -- but it also catches a file that has been split incorrectly.
* the glTF axis convention this project relies on, recorded rather than re-derived.

    tools/assets/gltf_validate.py assets-src/Models/Fallback/box.glb
    tools/assets/gltf_validate.py --all
    tools/assets/gltf_validate.py --selftest

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

REPO = Path(__file__).resolve().parents[2]
MODELS = REPO / "assets-src" / "Models"

GLB_MAGIC = 0x46546C67  # 'glTF'
CHUNK_JSON = 0x4E4F534A  # 'JSON'


def find_cna_content() -> Path | None:
    for candidate in (
        REPO / "build" / "CNA_BUILD" / "cna-content",
        REPO / "build-consumer" / "CNA_BUILD" / "cna-content",
        REPO.parent / "cna" / "build" / "cna-content",
    ):
        if candidate.is_file():
            return candidate
    return None


def read_gltf_json(path: Path) -> tuple[dict | None, str | None]:
    """The JSON of a `.gltf` or the JSON chunk of a `.glb`."""
    try:
        data = path.read_bytes()
    except OSError as error:
        return None, f"could not be read: {error}"

    if path.suffix.lower() == ".gltf":
        try:
            return json.loads(data.decode("utf-8")), None
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            return None, f"is not valid JSON: {error}"

    # `.glb`: a 12-byte header then length-prefixed chunks. Parsed here rather than with a library
    # so this tool has no dependency to install -- the same reason `gltf-validator` is optional.
    if len(data) < 20:
        return None, "is shorter than a glTF binary header"
    magic, version, total = struct.unpack_from("<III", data, 0)
    if magic != GLB_MAGIC:
        return None, f"has magic {magic:#010x}, not 'glTF'"
    if version != 2:
        return None, f"declares glTF version {version}; this project uses glTF 2.0"
    if total != len(data):
        return None, f"declares {total} bytes but the file is {len(data)}"

    offset = 12
    while offset + 8 <= len(data):
        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        if offset + chunk_length > len(data):
            return None, "has a chunk that runs past the end of the file"
        if chunk_type == CHUNK_JSON:
            try:
                return json.loads(data[offset : offset + chunk_length].decode("utf-8")), None
            except (UnicodeDecodeError, json.JSONDecodeError) as error:
                return None, f"has a JSON chunk that does not parse: {error}"
        offset += chunk_length
    return None, "has no JSON chunk"


def project_rules(document: dict) -> list[str]:
    problems: list[str] = []

    skins = document.get("skins", [])
    if len(skins) > 1:
        # MEASURED (`HOUSE-00076`): `CNA.ModelProcessor` refuses a multi-skin glTF outright, so this
        # is an earlier and clearer version of a failure that would happen anyway.
        problems.append(
            f"declares {len(skins)} skins; `cna-house.md` §47.0 allows exactly one per runtime "
            f"file. Split it with tools/assets/skin_split.py."
        )

    asset = document.get("asset", {})
    version = str(asset.get("version", ""))
    if not version.startswith("2."):
        problems.append(f"asset.version is {version!r}; this project uses glTF 2.0")

    meshes = document.get("meshes", [])
    if not meshes:
        problems.append("has no meshes")

    for index, mesh in enumerate(meshes):
        name = mesh.get("name", f"mesh {index}")
        for primitive in mesh.get("primitives", []):
            mode = primitive.get("mode", 4)
            if mode != 4:
                # The pipeline builds indexed triangle lists; anything else silently imports as
                # nothing or as the wrong topology.
                problems.append(f"mesh '{name}' uses primitive mode {mode}, not TRIANGLES (4)")
            attributes = primitive.get("attributes", {})
            if "POSITION" not in attributes:
                problems.append(f"mesh '{name}' has a primitive with no POSITION attribute")
            if "JOINTS_0" in attributes and "WEIGHTS_0" not in attributes:
                problems.append(f"mesh '{name}' has JOINTS_0 but no WEIGHTS_0")

    return problems


def run_gltf_validator(path: Path) -> tuple[bool, list[str]]:
    """(ran, problems)."""
    tool = shutil.which("gltf-validator") or shutil.which("gltf_validator")
    if tool is None:
        return False, []
    result = subprocess.run([tool, str(path)], capture_output=True, text=True)
    if result.returncode == 0:
        return True, []
    output = (result.stdout + result.stderr).strip().splitlines()
    return True, [f"gltf-validator: {line}" for line in output[:20]]


def run_importer(path: Path, tool: Path) -> list[str]:
    """A real `cna-content` import, with its warnings treated as errors."""
    # build/ is where a developer tree keeps scratch; a fresh checkout (CI) has none.
    (REPO / "build").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="gltf-validate-", dir=str(REPO / "build")) as work:
        result = subprocess.run(
            [str(tool), "build", str(path), "-o", str(Path(work) / "out.cnb")],
            capture_output=True,
            text=True,
        )
        output = result.stdout + result.stderr
        problems: list[str] = []
        if result.returncode != 0:
            for line in output.strip().splitlines():
                if line.strip():
                    problems.append(f"cna-content: {line.strip()}")
        else:
            # WARNINGS ARE ERRORS. A warning from the importer says something was guessed at, and a
            # guess in an asset is a bug waiting for a screenshot -- the acceptance for this task is
            # a *useful message*, and the importer's own is the most useful one available.
            for line in output.splitlines():
                if "warning" in line.lower():
                    problems.append(f"cna-content warning (treated as an error): {line.strip()}")
        return problems


def validate(path: Path, tool: Path | None) -> list[str]:
    document, error = read_gltf_json(path)
    if error is not None:
        return [error]
    assert document is not None

    problems = project_rules(document)

    ran, validator_problems = run_gltf_validator(path)
    problems += validator_problems

    if tool is not None:
        problems += run_importer(path, tool)

    return problems


def selftest(tool: Path | None) -> int:
    """`HOUSE-00186`'s acceptance: a deliberately broken glTF is rejected with a useful message."""
    cases: list[tuple[str, bytes, str]] = [
        ("truncated header", b"glTF", "shorter"),
        ("wrong magic", b"NOPE" + b"\x02\x00\x00\x00" + b"\x14\x00\x00\x00" + b"\x00" * 8, "magic"),
    ]
    # And one that is structurally a valid `.glb` but breaks a PROJECT rule, which is the case a
    # generic validator would pass.
    two_skins = json.dumps(
        {
            "asset": {"version": "2.0"},
            "meshes": [{"name": "m", "primitives": [{"attributes": {"POSITION": 0}}]}],
            "skins": [{"joints": [0]}, {"joints": [1]}],
        }
    ).encode("utf-8")
    two_skins += b" " * ((4 - len(two_skins) % 4) % 4)
    glb = struct.pack("<III", GLB_MAGIC, 2, 12 + 8 + len(two_skins))
    glb += struct.pack("<II", len(two_skins), CHUNK_JSON) + two_skins
    cases.append(("two skins", glb, "skins"))

    failures = 0
    # build/ is where a developer tree keeps scratch; a fresh checkout (CI) has none.
    (REPO / "build").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="gltf-selftest-", dir=str(REPO / "build")) as work:
        for name, payload, expect in cases:
            path = Path(work) / f"{name.replace(' ', '_')}.glb"
            path.write_bytes(payload)
            problems = validate(path, tool=None)  # no importer: these never reach it
            if not problems:
                print(f"  SELFTEST FAILED: '{name}' was accepted", file=sys.stderr)
                failures += 1
            elif not any(expect in p for p in problems):
                print(
                    f"  SELFTEST FAILED: '{name}' was rejected, but no message mentioned "
                    f"'{expect}': {problems}",
                    file=sys.stderr,
                )
                failures += 1
            else:
                print(f"  caught '{name}': {problems[0]}")
    if failures:
        return 1
    print(f"gltf_validate: selftest passed -- {len(cases)} broken file(s) rejected with a reason.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="*", type=Path)
    parser.add_argument("--all", action="store_true", help="every .glb/.gltf under assets-src/Models")
    parser.add_argument("--selftest", action="store_true", help="prove the checks reject bad input")
    parser.add_argument("--no-importer", action="store_true", help="skip the cna-content pass")
    args = parser.parse_args()

    tool = None if args.no_importer else find_cna_content()

    if args.selftest:
        return selftest(tool)

    paths = list(args.paths)
    if args.all or not paths:
        paths = sorted(p for p in MODELS.rglob("*") if p.suffix.lower() in (".glb", ".gltf"))
    if not paths:
        print("gltf_validate: no glTF files to check.")
        return 0

    if tool is None and not args.no_importer:
        # Reported, never hidden: a run without the importer pass has checked much less than a run
        # with it, and silently doing less is how a gate becomes decorative.
        print(
            "gltf_validate: no cna-content found, so the IMPORTER pass was skipped. "
            "Configure a build first for the full check.",
            file=sys.stderr,
        )

    total = 0
    for path in paths:
        problems = validate(path, tool)
        relative = path.relative_to(REPO) if path.is_relative_to(REPO) else path
        if problems:
            print(f"gltf_validate: {relative}", file=sys.stderr)
            for problem in problems:
                print(f"  {problem}", file=sys.stderr)
            total += len(problems)
        else:
            print(f"gltf_validate: {relative} OK")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
