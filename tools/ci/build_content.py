#!/usr/bin/env python3
"""build_content.py -- one command that builds the content tree, in the right order, once.

`HOUSE-00216`. Thirty-five tools have accumulated under `tools/` and until now the order they run
in existed only in the order the tasks happened to be written. This is that order, declared once,
in one place, with the dependencies between stages stated rather than implied -- and it is a
**graph**, not a list: the order is derived from it by topological sort, so adding a stage means
naming what it needs and nothing else.

    tools/ci/build_content.py                 # build what is out of date
    tools/ci/build_content.py --dry-run       # print the plan and stop
    tools/ci/build_content.py --force         # rebuild everything
    tools/ci/build_content.py --only world    # one group
    tools/ci/build_content.py --selftest

    cmake --build build --target content

## Up-to-date is decided by CONTENT, never by mtime

This is the decision the whole tool turns on, and `AGENTS.md` is the reason. Its opening argument
is that every avoidable rebuild is irreversible flash wear on one SSD shared by ten agents, and
mtime produces avoidable rebuilds in both directions:

* **git does not preserve mtime.** A fresh clone, a branch switch, a `git stash pop` -- each gives
  every touched file the time it was written, so an mtime build rebuilds a content tree that has
  not changed at all. On this repository that is the whole of `assets-src/`.
* **mtime can also say "fresh" when it is stale.** Check out an older revision of an input and its
  mtime is *now*, which is newer than the output -- that direction rebuilds correctly. But restore
  an output from a backup, or let a tool write one without touching its input, and the output is
  newer than an input it does not match.

So a stage is up to date when the SHA-256 of every input file, plus the exact command line, plus
the list of outputs, matches what the stamp file recorded when those outputs were produced. Touch
an input all you like; change one byte of it and the stage runs.

## A stage with no inputs is SKIPPED, and said so

Most of the world and Blender tools read `assets-src/world/*.json`, which phase 5 has not authored
yet, and the house shell, which phase 6 has not generated. `make content` has to be runnable today,
so a stage whose inputs do not exist is reported as **skipped** -- distinct from *ran*, from *up to
date*, and above all from *failed*. A build that quietly succeeded by doing nothing would be the
worst of the four.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

#: Where the stamps live. Under `build/`, because they describe a build and `.gitignore` already
#: covers `/build*/`; never under `assets-src/`, which is source.
DEFAULT_STAMPS = REPO / "build" / "content-stamps.json"

#: `CMakeLists.txt`'s `CNAHOUSE_CNB_ASSET_DIRS` plus `CNAHOUSE_CNB_MEDIA_DIR`. `Media/` builds to
#: `content/` itself rather than to `content/Media/`, because a `Video` compiles to a metadata
#: `.cnb` beside a byte-identical copy the runtime resolves through `BuildAssetPath`
#: (`HOUSE-00201`); the rest build to `content/<dir>/`.
CONTENT_ROOTS = [("Models", "content/Models"), ("Textures", "content/Textures"),
                 ("Audio", "content/Audio"), ("Fonts", "content/Fonts"),
                 ("world", "content/world"), ("Media", "content")]

STAMP_VERSION = 1


class Stage:
    """One step of the content build: what it runs, what it reads, what it writes, what it needs.

    `inputs` and `outputs` are repository-relative glob patterns. A stage is identified by `name`,
    and `needs` names other stages -- never files -- because a stage's dependency on another is a
    fact about the pipeline, while which file carries it is an implementation detail that changes.
    """

    def __init__(self, name: str, group: str, command: list[str], inputs: list[str],
                 outputs: list[str], needs: list[str] | None = None,
                 description: str = "", tool: Path | None = None,
                 requires_tool: bool | None = None) -> None:
        self.name = name
        self.group = group
        self.command = command
        self.inputs = inputs
        self.outputs = outputs
        self.needs = list(needs or [])
        self.description = description
        #: An executable the stage cannot run without. Absent means SKIP, not fail: `cna-content`
        #: is built by CNA, and a checkout that has not configured a build yet can still run every
        #: validator. Distinct from a missing input, and reported as its own reason.
        #:
        #: `HOUSE-01279`: the rule used to be spelt "`tool` is None AND the group is `compile`",
        #: which silently did nothing for any other group. A stage that names a tool means it,
        #: whatever group it is in -- `shading` needs Blender, which a checkout may not have -- so
        #: the flag below records that a tool was ASKED for and the skip keys on that. Passing
        #: `tool=None` where none was requested is still not a skip.
        self.tool = tool
        self.requires_tool = requires_tool if requires_tool is not None else tool is not None
        #: What to call the missing tool in the skip reason. The command's own name, because a
        #: reader of the report wants to know what to install and not which stage wanted it.
        self.tool_name = command[0] if command else name


def find_cna_content() -> Path | None:
    """CNA's content compiler, wherever a configured build left it.

    The same three places `content_verify.py` looks, and for the same reason: it is built by CNA,
    not by this repository, so its path depends on how the sibling was configured.
    """
    # AM4-199: an explicit `CNA_CONTENT`, then the three configured-build locations, then PATH --
    # a build tree outside the repository (an out-of-source build root) is otherwise never found.
    explicit = os.environ.get("CNA_CONTENT")
    candidates = [Path(explicit)] if explicit else []
    candidates += [REPO / "build" / "CNA_BUILD" / "cna-content",
                   REPO / "build-consumer" / "CNA_BUILD" / "cna-content",
                   REPO.parent / "cnanext" / "build" / "cna-content"]
    on_path = shutil.which("cna-content")
    if on_path:
        candidates.append(Path(on_path))
    for candidate in candidates:
        if candidate.is_file() and candidate.stat().st_mode & 0o111:
            return candidate
    return None


def with_validator_gate(stages: list[Stage]) -> list[Stage]:
    """Give every generator an explicit dependency on every validator.

    "Validators first" is a real dependency, not a presentation preference: a gate that ran after
    the generators would be reporting on assets the pipeline had already consumed. Writing it as an
    edge rather than as a sort key is what makes it survive -- ordering the plan by group happens
    to work for today's graph and quietly stops working the moment a generator has no `needs` and
    lands in the same level as a validator, which is how this was first written and how the
    selftest caught it.
    """
    validators = [s.name for s in stages if s.group == "validate"]
    for stage in stages:
        if stage.group == "validate":
            continue
        for name in validators:
            if name not in stage.needs:
                stage.needs.append(name)
    return stages


def default_stages() -> list[Stage]:
    """The whole pipeline. `cna-house.md` §18.3 is the authority for the lighting chain's order.

    Validators come first and produce nothing: they are the gates that stop a bad asset entering
    the pipeline at all, and running them after the generators would mean generating from it.
    """
    stages = [
        Stage("layout", "validate", ["python3", "tools/ci/check_layout.py"],
              inputs=["tools/ci/check_layout.py"], outputs=[],
              description="the directory-set and file-placement gate"),
        Stage("manifest", "validate", ["python3", "tools/ci/check_manifest.py"],
              inputs=["assets-src/assets.manifest.json"], outputs=[],
              description="no unlisted file under assets-src/, no hash mismatch"),
        Stage("licences", "validate", ["python3", "tools/assets/verify_licences.py", "--check"],
              inputs=["assets-src/assets.manifest.json", "licenses/THIRD-PARTY-ASSETS.md"],
              outputs=[], needs=["manifest"],
              description="every row has a licence, a licence file and the booleans"),
        Stage("fonts", "validate", ["python3", "tools/ci/check_fonts.py"],
              inputs=["assets-src/Fonts/*.spritefont"], outputs=[],
              description="every descriptor resolves to a vendored face"),
        Stage("anim", "validate", ["python3", "tools/ci/check_anim_assets.py"],
              inputs=["assets-src/Models/**/*.glb"], outputs=[],
              description="single-skin models, and every sidecar binds"),
        Stage("sky-lut", "validate", ["python3", "tools/world/sky_lut.py", "--check"],
              inputs=["tools/world/sky_lut.py", "assets-src/world/layout.sky.json"], outputs=[],
              description="§31.2's generated 32 x 8 x 16 compact sky-colour contract"),
        Stage("cloud-textures", "validate",
              ["python3", "tools/world/cloud_textures.py", "--check"],
              inputs=["tools/world/cloud_textures.py", "assets-src/Textures/Sky/cloud_*.png"],
              outputs=[],
              description="§31.3's deterministic 1024-square tileable RGBA cloud sources"),
        Stage("moon-albedo", "validate",
              ["python3", "tools/assets/moon_albedo.py", "--check"],
              inputs=["tools/assets/moon_albedo.py", "assets-src/Textures/Sky/moon_albedo.png"],
              outputs=[],
              description="§33.3's pinned 1024-square NASA lunar albedo source"),
        Stage("star-catalogue", "validate",
              ["python3", "tools/world/build_stars.py", "--check"],
              inputs=["tools/world/build_stars.py", "assets-src/world/bsc5p.psv"],
              outputs=[],
              description="§34's pinned 1,500-row NASA HEASARC BSC5P subset"),
        # --- the world chain (`HOUSE-00210`…`HOUSE-00215`) --------------------------------------
        # `HOUSE-00363`. The layout gate comes first in this chain, because `build_collision.py`
        # and the five tools after it read the layout and believe it; §15.7 says a validation
        # failure fails the build, and the only moment that can be true is before the first
        # generator has read a portal.
        #
        # It is in the world group and not in `validate` deliberately. `with_validator_gate` wires
        # every stage in that group to every generator, which is right for the licence and
        # manifest gates -- they speak for the whole tree -- and wrong for this one: an unauthored
        # layout is no reason to stop compiling the textures.
        Stage("world-rules", "world",
              ["python3", "tools/world/validate_world.py", "assets-src/world"],
              inputs=["assets-src/world/*.json"], outputs=[], needs=["manifest"],
              description="the twelve rules of §15.7 over the authored layout"),
        # `HOUSE-00421`. The authored files are JSONC and the runtime's `System::Text::Json` is
        # not: this is the stripping step `world-format.md` describes. After the rules, because
        # deploying a layout that fails §15.7 would put a broken house where the game reads.
        Stage("world-deploy", "world",
              ["python3", "tools/world/deploy_world.py"],
              inputs=["assets-src/world/*.json", "assets-src/assets.manifest.json"],
              outputs=["content/world/*.json"],
              needs=["world-rules"],
              description="strip the comments, deploy as plain JSON, and hash what was written"),
        Stage("collision", "world",
              ["python3", "tools/world/build_collision.py"],
              inputs=["assets-src/world/*.json", "assets-src/assets.manifest.json",
                      "assets-src/Models/**/*.glb"],
              outputs=["content/world/collision.bin"], needs=["world-rules"],
              description="rooms become walls; the layout and the _COL proxies"),
        Stage("nav", "world", ["python3", "tools/world/build_nav.py"],
              inputs=["assets-src/world/*.json"], outputs=["content/world/nav.bin"],
              needs=["collision"], description="the pet waypoint graph"),
        Stage("coverage", "world", ["python3", "tools/world/build_coverage.py"],
              inputs=["assets-src/world/*.json"], outputs=["content/world/coverage.bin"],
              needs=["collision"], description="the rain/roof coverage height field"),
        Stage("sky-dome", "world",
              ["python3", "tools/world/build_skydome.py", "--out",
               "content/world/sky_dome.bin"],
              inputs=["tools/world/build_skydome.py"], outputs=["content/world/sky_dome.bin"],
              needs=["sky-lut"], description="§31.1's indexed hemisphere, skirt and ground disc"),
        Stage("stars", "world", ["python3", "tools/world/build_stars.py", "--emit"],
              inputs=["tools/world/build_stars.py", "assets-src/world/bsc5p.psv"],
              outputs=["content/world/stars.bin"], needs=["star-catalogue"],
              description="§34's brightest complete 1,500-star catalogue binary"),
        # --- the generated exterior tree (`HOUSE-00227`) -----------------------------------------
        # These four write `.glb` that `build_chunks.py` then reads, and until `HOUSE-00227` NO
        # stage ran them: `run_checks.sh` gates their selftests, which is a different question from
        # whether their output is current. It was not. On 2026-09-10 `build/terrain` held tiles
        # written at 12:49 on the 9th from a height field that was rewritten at 14:56 the same day,
        # `build/fence` held a tree from before `HOUSE-00785` fixed the generator, and
        # `chunks.bin` -- and every render reference of the outdoors -- was built from both.
        Stage("terrain-tiles", "world",
              ["python3", "tools/world/terrain_gen.py", "--tiles"],
              inputs=["tools/world/outdoor_materials.py", "assets-src/world/layout.exterior.json", "assets-src/world/terrain.png",
                      "assets-src/world/terrain_materials.png"],
              outputs=["build/terrain/TERRAIN_*.glb"], needs=["world-rules"],
              description="§11.5's twenty height-field tiles"),
        Stage("road", "world",
              ["python3", "tools/world/terrain_gen.py", "--road"],
              inputs=["tools/world/outdoor_materials.py", "assets-src/world/layout.exterior.json", "assets-src/world/terrain.png"],
              outputs=["build/terrain/ROAD_*.glb"], needs=["world-rules"],
              description="§11.4's road, kerbs, sidewalks, grates and markings"),
        Stage("fence", "world", ["python3", "tools/world/fence_gen.py"],
              inputs=["tools/world/outdoor_materials.py", "assets-src/world/layout.exterior.json", "assets-src/world/terrain.png"],
              outputs=["build/fence/*.glb"], needs=["world-rules"],
              description="§11.2's fences and gates and §11.1's garden structures"),
        Stage("neighbourhood", "world", ["python3", "tools/world/neighbourhood_gen.py"],
              inputs=["assets-src/world/layout.exterior.json"],
              outputs=["build/neighbourhood/*.glb"], needs=["world-rules"],
              description="§11.4's neighbour houses, impostor cards and street furniture"),
        # `HOUSE-00856`. The meshes the runtime opens. `neighbourhood` writes the `.glb`; this
        # turns them into one `content/world/neighbourhood.bin` keyed by asset id, because §11.4's
        # 118 instances of 34 assets cannot be chunked -- a chunk bakes its placement and an
        # instance has to keep its own so §26 can swap its LOD.
        Stage("neighbourhood-bin", "world",
              ["python3", "tools/world/build_neighbourhood.py"],
              inputs=["assets-src/world/layout.exterior.json", "build/neighbourhood/*.glb"],
              outputs=["content/world/neighbourhood.bin"],
              needs=["neighbourhood", "world-rules"],
              description="§11.4's meshes, keyed by the asset id a row names"),
        Stage("chunks", "world", ["python3", "tools/world/build_chunks.py"],
              # ...and `build/shell`, which `build_chunks.py` has read by default since
              # `HOUSE-00473` and which no stage declared until `HOUSE-00492` regenerated the
              # shell and looked. It is masked today only because `chunks` fails on every run
              # (`HOUSE-00487`) and therefore reruns anyway; the day that is fixed, a shell
              # regeneration would go unchunked. `build/shell-lm` is deliberately NOT here: it is
              # the PREFERRED tree when it exists and the build works without it, so requiring it
              # would skip the stage on a checkout that has never baked a lightmap.
              inputs=["assets-src/world/*.json", "assets-src/Models/**/*.glb",
                      "build/terrain/*.glb", "build/fence/*.glb", "build/shell/*.glb"],
              outputs=["content/world/chunks.bin"],
              needs=["collision", "terrain-tiles", "road", "fence"],
              description="per-cell static prop batches"),
        Stage("skyexposure", "world", ["python3", "tools/world/build_skyexposure.py"],
              inputs=["assets-src/world/*.json"], outputs=["content/world/skyexposure.bin"],
              needs=["coverage"], description="per-cell sky and facade exposure"),
        # `HOUSE-01279`. §22's per-window sun-shading grid, and the first stage in this build that
        # needs Blender -- which is why `requires_tool` exists. `build/shell` has no stage of its
        # own (see `chunks`), so a checkout that has never generated the shell skips this on its
        # missing input rather than baking a grid against nothing.
        Stage("shading", "world",
              ["python3", "tools/blender/shading_factor.py", "build/shell",
               "--world", "assets-src/world",
               "--neighbourhood", "build/neighbourhood",
               "--out", "content/world"],
              inputs=["assets-src/world/layout.openings.json",
                      "assets-src/world/layout.portals.json",
                      "assets-src/world/layout.cells.json",
                      "assets-src/world/layout.exterior.json",
                      "build/shell/*.glb", "build/neighbourhood/*.glb"],
              outputs=["content/world/shading.bin"],
              needs=["neighbourhood", "world-rules"],
              tool=shutil.which("blender"), requires_tool=True,
              description="§22's per-window sun-shading grid, 12 x 24 nodes a window"),
        Stage("snowshell", "world", ["python3", "tools/world/build_snowshell.py"],
              inputs=["assets-src/world/*.json", "assets-src/Models/**/*.glb"],
              outputs=["content/world/snowshell.bin"], needs=["coverage"],
              description="the snow shells over up-facing exterior surfaces"),
    ]

    # --- cna-content (`HOUSE-00182`) -------------------------------------------------------------
    # Last, and per root rather than as one call: a texture change should not recompile the audio.
    # `world/` is listed here because `CMakeLists.txt` lists it, and it comes after the generators
    # that write into it.
    tool = find_cna_content()
    for directory, output in CONTENT_ROOTS:
        needs = ["collision"] if directory == "world" else []
        stages.append(Stage(
            f"cnb-{directory.lower()}", "compile",
            [str(tool or "cna-content"), "build", f"assets-src/{directory}", "-o", output,
             "--quiet"],
            inputs=[f"assets-src/{directory}/**/*"], outputs=[], needs=needs, tool=tool,
            requires_tool=True,
            description=f"compile assets-src/{directory} to {output} with cna-content"))

    return with_validator_gate(stages)


# ==================================================================================== the ordering


class GraphError(Exception):
    """The declared graph does not describe a buildable pipeline."""


def resolve_order(stages: list[Stage]) -> list[Stage]:
    """Topological order, deterministic, with every failure named.

    Ordered by name within a level rather than by declaration order, so the plan does not change
    when someone reorders the list -- a build whose order depends on the order of a Python literal
    is a build whose order nobody has actually decided.
    """
    by_name = {}
    for stage in stages:
        if stage.name in by_name:
            raise GraphError(f"two stages are called {stage.name!r}")
        by_name[stage.name] = stage
    for stage in stages:
        for need in stage.needs:
            if need not in by_name:
                raise GraphError(
                    f"stage {stage.name!r} needs {need!r}, which is not a stage; the stages are "
                    f"{', '.join(sorted(by_name))}")

    done: set[str] = set()
    order: list[Stage] = []
    remaining = sorted(by_name)
    while remaining:
        ready = [name for name in remaining if all(n in done for n in by_name[name].needs)]
        if not ready:
            cycle = ", ".join(sorted(remaining))
            raise GraphError(f"the stage graph has a cycle among: {cycle}")
        for name in ready:
            order.append(by_name[name])
            done.add(name)
        remaining = [name for name in remaining if name not in done]
    return order


# ================================================================================ up-to-date checks


def expand(patterns: list[str], root: Path) -> tuple[list[Path], list[str]]:
    """Every existing file a stage's patterns name, and the patterns that named nothing.

    Both halves matter. "This stage has no inputs" is **not** the same as "no pattern matched":
    a stage that reads the layout *and* the asset manifest, in a repository that has the manifest
    and not the layout, has one of the two and cannot run. The first version of this asked only
    whether the whole set was empty, so `build_collision` was launched on the strength of the
    manifest existing and failed inside the tool -- which reads as a broken build rather than as a
    pipeline waiting for phase 5.
    """
    found: set[Path] = set()
    empty: list[str] = []
    for pattern in patterns:
        matched = [p for p in root.glob(pattern) if p.is_file()]
        if not matched:
            empty.append(pattern)
        found.update(matched)
    return sorted(found), empty


def _output_present(pattern: str, root: Path) -> bool:
    """Whether @p pattern names something that exists -- globbing it when it is a glob."""
    if any(character in pattern for character in "*?["):
        return any(root.glob(pattern))
    return (root / pattern).exists()


def fingerprint(stage: Stage, root: Path) -> tuple[str, list[Path]]:
    """The SHA-256 of everything the stage's result depends on, and the inputs it found.

    The command line is hashed with the files. A stage run with `--samples 4` and one run with
    `--samples 64` produce different content from identical inputs, and a fingerprint over the
    inputs alone would call the second one up to date.
    """  # noqa: D401
    inputs, missing = expand(stage.inputs, root)
    digest = hashlib.sha256()
    digest.update(json.dumps(stage.command, sort_keys=True).encode())
    digest.update(json.dumps(sorted(stage.outputs), sort_keys=True).encode())
    # ...and the TOOL, which is an input like any other (`HOUSE-00226`). The command line was
    # hashed as text and the scripts it names were not, so editing a generator left every stage
    # that runs it "up to date": `HOUSE-00777` changed what `build_collision.py` writes and the
    # graph rebuilt nothing, including the nav graph built against its output. Every argument that
    # names a file in the repository is hashed -- the script, and anything else a stage is handed.
    for token in stage.command:
        if any(token == pattern or fnmatch.fnmatch(token, pattern) for pattern in stage.outputs):
            # A stage that names its own output on the command line would otherwise be stale the
            # moment it ran: its fingerprint would contain what it had just written. Matched as a
            # PATTERN as well as compared, so a stage declaring `out/*.bin` and naming `out/a.bin`
            # is covered too -- `HOUSE-00226` wrote the equality and `HOUSE-00856` found the glob
            # form while giving the exterior generators glob outputs.
            continue
        candidate = root / token
        if candidate.is_file():
            digest.update(token.encode())
            digest.update(candidate.read_bytes())
    for path in inputs:
        digest.update(str(path.relative_to(root)).encode())
        digest.update(path.read_bytes())
    return digest.hexdigest(), inputs, missing


def load_stamps(path: Path) -> dict:
    if not path.is_file():
        return {"version": STAMP_VERSION, "stages": {}}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        # A corrupt stamp file means "rebuild", never "crash": it is a cache, and a cache that can
        # break the build is worse than no cache.
        return {"version": STAMP_VERSION, "stages": {}}
    if data.get("version") != STAMP_VERSION:
        return {"version": STAMP_VERSION, "stages": {}}
    return data


def save_stamps(path: Path, stamps: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(stamps, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def status_of(stage: Stage, root: Path, stamps: dict, force: bool) -> tuple[str, str]:
    """One of `skip`, `run`, `fresh`, with the reason."""
    digest, _inputs, missing = fingerprint(stage, root)
    if stage.requires_tool and stage.tool is None:
        return "skip", f"{stage.tool_name} is not available; the stage needs it to run"
    if missing:
        return "skip", f"no input matches {', '.join(missing)} yet"
    if force:
        return "run", "forced"
    recorded = stamps["stages"].get(stage.name)
    if recorded is None:
        return "run", "never built"
    if recorded.get("fingerprint") != digest:
        return "run", "inputs or command changed"
    # A GLOB output is satisfied by any match, not by a file literally called `*.glb`. Written as
    # `(root / p).exists()` this reported "output missing" on every run for `world-deploy` and for
    # `HOUSE-00227`'s four exterior generators, so five stages rebuilt every time however fresh
    # they were -- found by `HOUSE-00856`, whose own output is a single named file and was the
    # only one of the five that ever reported fresh.
    missing = [p for p in stage.outputs if not _output_present(p, root)]
    if missing:
        # The stamp says fresh and the file is not there. Trusting the stamp would leave the build
        # reporting success over an output nobody can load.
        return "run", f"output missing: {', '.join(missing)}"
    return "fresh", "up to date"


# ====================================================================================== the runner


def run_command(command: list[str], root: Path) -> tuple[int, str]:
    result = subprocess.run(command, cwd=root, capture_output=True, text=True)
    return result.returncode, (result.stdout + result.stderr)


def build(stages: list[Stage], root: Path, stamps_path: Path, *, force: bool = False,
          dry_run: bool = False, only: str | None = None, runner=run_command) -> dict:
    """Run the pipeline. Returns a report; raises nothing that is not a `GraphError`."""
    order = resolve_order(stages)
    if only:
        order = [s for s in order if s.group == only]
        if not order:
            raise GraphError(f"no stage is in group {only!r}")
    stamps = load_stamps(stamps_path)

    results = []
    blocked: set[str] = set()
    # A stage whose upstream RAN is stale, whatever its own fingerprint says. `needs` is a fact
    # about the graph and this is what makes it one: the pet graph is built against the collision
    # world, and until this a terrain change rebuilt `collision.bin` and left `nav.bin` sitting
    # over the ground it used to be. Found by `HOUSE-00768`, whose shed pad moved the lawn 0.29 m
    # and rebuilt collision alone.
    rebuilt: set[str] = set()
    failed = False
    for stage in order:
        if any(need in blocked for need in stage.needs):
            results.append({"stage": stage.name, "status": "blocked", "seconds": 0.0,
                            "reason": "an upstream stage failed or was skipped"})
            blocked.add(stage.name)
            continue
        status, reason = status_of(stage, root, stamps, force)
        upstream = sorted(need for need in stage.needs if need in rebuilt)
        if status == "fresh" and upstream:
            status, reason = "run", f"{', '.join(upstream)} rebuilt"
        if status == "skip":
            results.append({"stage": stage.name, "status": "skipped", "seconds": 0.0,
                            "reason": reason})
            # A skipped stage blocks its dependents: they would read outputs it never wrote.
            blocked.add(stage.name)
            continue
        if status == "fresh":
            results.append({"stage": stage.name, "status": "fresh", "seconds": 0.0,
                            "reason": reason})
            continue
        if dry_run:
            results.append({"stage": stage.name, "status": "would-run", "seconds": 0.0,
                            "reason": reason})
            continue

        started = time.monotonic()
        code, output = runner(stage.command, root)
        elapsed = time.monotonic() - started
        if code != 0:
            results.append({"stage": stage.name, "status": "failed", "seconds": elapsed,
                            "reason": reason, "output": output[-4000:]})
            blocked.add(stage.name)
            failed = True
            continue
        digest, _, _ = fingerprint(stage, root)
        stamps["stages"][stage.name] = {
            "fingerprint": digest, "outputs": list(stage.outputs), "seconds": round(elapsed, 3)}
        results.append({"stage": stage.name, "status": "built", "seconds": elapsed,
                        "reason": reason})
        rebuilt.add(stage.name)

    if not dry_run:
        save_stamps(stamps_path, stamps)
    return {"order": [s.name for s in order], "results": results, "failed": failed}


def report(outcome: dict) -> str:
    lines = []
    total = 0.0
    counts: dict[str, int] = {}
    for entry in outcome["results"]:
        counts[entry["status"]] = counts.get(entry["status"], 0) + 1
        total += entry["seconds"]
        mark = {"built": "built  ", "fresh": "fresh  ", "skipped": "skipped",
                "blocked": "blocked", "failed": "FAILED ", "would-run": "would  "}[entry["status"]]
        timing = f"{entry['seconds']:6.2f}s" if entry["seconds"] else "       "
        lines.append(f"  {mark} {entry['stage']:<14} {timing}  {entry['reason']}")
    lines.append("  " + ", ".join(f"{n} {k}" for k, n in sorted(counts.items()))
                 + f"; {total:.2f}s of work")
    return "\n".join(lines)


# ================================================================================== documentation


#: The generated block in `docs/content-build.md`. `HOUSE-00217` asks for "what each tool does, in
#: what order" -- which is the graph, written out. Writing it by hand would make it a second
#: description of the pipeline, and a second description drifts: `budget_report.py` solved the same
#: problem the same way, and `--check-docs` is what stops it drifting here.
DOC_BEGIN = "<!-- BEGIN GENERATED: build_content.py --docs -->"
DOC_END = "<!-- END GENERATED -->"

DOC_PATH = REPO / "docs" / "content-build.md"


def documentation(stages: list[Stage]) -> str:
    """The stage table, in build order, from the graph itself."""
    order = resolve_order(stages)
    lines = [DOC_BEGIN, "",
             "| # | Stage | Group | What it does | Needs | Reads | Writes |",
             "|---|---|---|---|---|---|---|"]
    for index, stage in enumerate(order, start=1):
        needs = ", ".join(f"`{n}`" for n in sorted(stage.needs)) or "—"
        reads = "<br>".join(f"`{p}`" for p in stage.inputs) or "—"
        writes = "<br>".join(f"`{p}`" for p in stage.outputs) or "— (a gate)"
        lines.append(f"| {index} | **`{stage.name}`** | {stage.group} | {stage.description} | "
                     f"{needs} | {reads} | {writes} |")
    lines += ["", "The command each stage runs:", "", "```"]
    for stage in order:
        lines.append(f"{stage.name:<14} {' '.join(stage.command)}")
    lines += ["```", "", DOC_END]
    return "\n".join(lines)


def splice_documentation(text: str, generated: str) -> str:
    if DOC_BEGIN not in text or DOC_END not in text:
        raise GraphError(
            f"{DOC_PATH.name} has no generated block; it needs the {DOC_BEGIN} / {DOC_END} "
            f"markers so the table can be regenerated in place")
    head = text[:text.index(DOC_BEGIN)]
    tail = text[text.index(DOC_END) + len(DOC_END):]
    return head + generated + tail


# ======================================================================================= selftest


def selftest() -> int:
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("build_content: selftest")
    workspace = Path(tempfile.mkdtemp(prefix="build_content_selftest_"))
    try:
        root = workspace / "repo"
        (root / "src").mkdir(parents=True)
        (root / "out").mkdir()
        (root / "src" / "a.txt").write_text("alpha\n", encoding="utf-8")
        (root / "src" / "b.txt").write_text("beta\n", encoding="utf-8")
        stamps = workspace / "stamps.json"

        ran: list[str] = []

        def fake(command, cwd):
            """Records the call and writes the stage's declared outputs."""
            ran.append(command[0])
            for name in command[1:]:
                (cwd / name).parent.mkdir(parents=True, exist_ok=True)
                (cwd / name).write_text("built\n", encoding="utf-8")
            return 0, ""

        def fixture():
            return [
                Stage("second", "g", ["second", "out/second.bin"], ["src/b.txt"],
                      ["out/second.bin"], needs=["first"]),
                Stage("first", "g", ["first", "out/first.bin"], ["src/a.txt"], ["out/first.bin"]),
                Stage("third", "g", ["third", "out/third.bin"], ["src/a.txt"],
                      ["out/third.bin"], needs=["second"]),
            ]

        # 1. The order comes from the GRAPH, not from the order the stages were written in.
        order = [s.name for s in resolve_order(fixture())]
        require(order == ["first", "second", "third"],
                f"the plan is a topological sort of `needs`, not the declaration order "
                f"({order}) -- the fixture declares them second, first, third")
        shuffled = list(reversed(fixture()))
        require([s.name for s in resolve_order(shuffled)] == order,
                "and reordering the declaration does not change it: a build whose order depends "
                "on the order of a Python literal is a build whose order nobody decided")

        # 2. A cycle and a dangling `needs` are refused, by name.
        cyclic = fixture() + [Stage("fourth", "g", ["x"], [], [], needs=["third"])]
        cyclic[1].needs = ["fourth"]
        try:
            resolve_order(cyclic)
            raised = ""
        except GraphError as exc:
            raised = str(exc)
        require("cycle" in raised and "first" in raised,
                f"a cycle is refused and its members named ({raised[:70]})")
        try:
            resolve_order([Stage("x", "g", ["x"], [], [], needs=["nope"])])
            raised = ""
        except GraphError as exc:
            raised = str(exc)
        require("nope" in raised and "not a stage" in raised,
                "a `needs` naming no stage is refused, naming it")

        # 3. A first build runs everything; a second runs nothing.
        ran.clear()
        first = build(fixture(), root, stamps, runner=fake)
        require([r["status"] for r in first["results"]] == ["built"] * 3,
                f"a first build runs every stage ({[r['status'] for r in first['results']]})")
        ran.clear()
        second = build(fixture(), root, stamps, runner=fake)
        require(ran == [],
                f"a second build with nothing changed runs nothing at all ({ran})")
        require(all(r["status"] == "fresh" for r in second["results"]),
                "and reports every stage fresh")

        # 3b. A stage whose UPSTREAM ran is stale, whatever its own inputs say (`HOUSE-00768`).
        #     `third` reads `src/a.txt` and `second` reads `src/b.txt`, so changing b rebuilds
        #     second -- and third, which is built against what second wrote. Until this, `nav.bin`
        #     sat over the ground the terrain used to be, because `collision.bin` was rebuilt
        #     under it and nav's own inputs had not changed.
        ran.clear()
        (root / "src" / "b.txt").write_text("beta again\n", encoding="utf-8")
        cascade = build(fixture(), root, stamps, runner=fake)
        statuses = {entry["stage"]: entry["status"] for entry in cascade["results"]}
        reasons = {entry["stage"]: entry["reason"] for entry in cascade["results"]}
        require(statuses == {"first": "fresh", "second": "built", "third": "built"}
                and reasons["third"] == "second rebuilt",
                f"a stage whose `needs` rebuilt is rebuilt too, and the report says whose "
                f"({statuses}, third because {reasons['third']!r})")
        ran.clear()
        again = build(fixture(), root, stamps, runner=fake)
        require(ran == [] and all(r["status"] == "fresh" for r in again["results"]),
                "and it settles: the run after that rebuilds nothing")

        # 4. THE CLAIM THIS TOOL EXISTS FOR: mtime is not what freshness means. `git` does not
        #    preserve mtime, so an mtime build rebuilds the whole tree after every clone or branch
        #    switch -- and `AGENTS.md`'s entire argument is that avoidable rebuilds are
        #    irreversible flash wear on one shared SSD.
        # `touch()` can leave the mtime where it was when the file was written in the same
        # instant, so the time is set explicitly -- otherwise this claim passes for the fixture
        # having done nothing.
        import os as _os

        before = (root / "src" / "a.txt").stat().st_mtime
        _os.utime(root / "src" / "a.txt", (before + 3600, before + 3600))
        after = (root / "src" / "a.txt").stat().st_mtime
        require(after > before + 1.0,
                f"the fixture really did move the input's mtime forward, by an hour "
                f"({before:.1f} -> {after:.1f})")
        ran.clear()
        build(fixture(), root, stamps, runner=fake)
        require(ran == [],
                f"touching an input rebuilds NOTHING, because the bytes did not change ({ran})")

        # 5. ...and one byte does.
        (root / "src" / "a.txt").write_text("alpha!\n", encoding="utf-8")
        ran.clear()
        build(fixture(), root, stamps, runner=fake)
        require(sorted(ran) == ["first", "second", "third"],
                f"changing a.txt rebuilds `first`, which reads it, and everything downstream of "
                f"`first` ({sorted(ran)}). Until `HOUSE-00768` this stopped at the stages whose "
                f"own inputs changed, which left every stage built against another's output "
                f"stale the moment that output moved")

        # 6. The COMMAND is part of the fingerprint. A stage run at 4 samples and one run at 64
        #    produce different content from identical inputs.
        changed = fixture()
        changed[1].command = ["first", "out/first.bin", "--samples", "64"]
        ran.clear()
        build(changed, root, stamps, runner=fake)
        require("first" in ran,
                f"changing a stage's command line rebuilds it, though no input moved ({ran})")

        # 6b. ...and so is the TOOL the command names (`HOUSE-00226`). A generator is an input:
        #     editing what `build_collision.py` writes and having the graph report every stage
        #     up to date is how a content tree ends up built by a program that no longer exists.
        build(fixture(), root, stamps, runner=fake)
        tool = root / "tools" / "first.py"
        tool.parent.mkdir(parents=True, exist_ok=True)
        tool.write_text("# a generator\n", encoding="utf-8")
        toolish = fixture()
        toolish[1].command = ["first", "tools/first.py", "out/first.bin"]
        build(toolish, root, stamps, runner=fake)
        ran.clear()
        build(toolish, root, stamps, runner=fake)
        require(ran == [], f"a stage with an unchanged tool is fresh ({ran})")
        tool.write_text("# a generator, changed\n", encoding="utf-8")
        ran.clear()
        build(toolish, root, stamps, runner=fake)
        require(sorted(ran) == ["first", "second", "third"],
                f"editing the tool a stage runs rebuilds it, and everything downstream "
                f"({sorted(ran)})")
        # ...and back to the plain fixture, settled, so what follows starts from a fresh graph.
        build(fixture(), root, stamps, runner=fake)

        # 7. A missing output rebuilds, whatever the stamp says. Trusting the stamp would leave
        #    the build reporting success over a file nobody can load.
        build(fixture(), root, stamps, runner=fake)
        (root / "out" / "second.bin").unlink()
        ran.clear()
        outcome = build(fixture(), root, stamps, runner=fake)
        require(ran == ["second", "third"],
                f"deleting an output rebuilds that stage, and `third` with it because it is built "
                f"against what `second` writes ({ran})")
        require(any("output missing" in r["reason"] for r in outcome["results"]),
                f"and says so, rather than reporting it fresh "
                f"({[(r['stage'], r['reason']) for r in outcome['results']]})")

        # 7b. `HOUSE-00856`: a GLOB output is satisfied by any match, not by a file literally
        #     called `*.bin`. Written the naive way this reported "output missing" for
        #     `world-deploy` and for `HOUSE-00227`'s four exterior generators on EVERY run, so five
        #     stages rebuilt however fresh they were, for months, saying so in the report each time.
        globbed = [Stage("globby", "g", ["globby", "out/globby-1.bin", "out/globby-2.bin"],
                         ["src/a.txt"], ["out/globby-*.bin"])]
        build(globbed, root, stamps, runner=fake)
        ran.clear()
        outcome = build(globbed, root, stamps, runner=fake)
        require(not ran,
                f"a stage whose output is a glob is fresh once it has run ({ran})")
        require(all(r["status"] == "fresh" for r in outcome["results"]),
                f"and says fresh rather than 'output missing' "
                f"({[(r['stage'], r['reason']) for r in outcome['results']]})")
        for path in sorted((root / "out").glob("globby-*.bin")):
            path.unlink()
        ran.clear()
        outcome = build(globbed, root, stamps, runner=fake)
        require(ran == ["globby"],
                f"...and deleting every match still rebuilds it ({ran})")
        require(any(r["reason"].startswith("output missing") for r in outcome["results"]),
                f"for THAT reason and not because its own output was in its fingerprint "
                f"({[(r['stage'], r['reason']) for r in outcome['results']]})")

        # 8. A stage with no inputs yet is SKIPPED -- distinct from fresh, and it blocks what
        #    depends on it. Most of the world tools are in this state until phase 5 authors the
        #    layout, and `make content` has to be runnable today.
        pending = fixture() + [
            Stage("future", "g", ["future", "out/future.bin"], ["src/nothing/*.json"],
                  ["out/future.bin"]),
            Stage("after", "g", ["after", "out/after.bin"], ["src/a.txt"], ["out/after.bin"],
                  needs=["future"])]
        ran.clear()
        outcome = build(pending, root, stamps, runner=fake)
        statuses = {r["stage"]: r["status"] for r in outcome["results"]}
        require(statuses["future"] == "skipped",
                f"a stage whose inputs do not exist is skipped ({statuses['future']})")
        require(statuses["after"] == "blocked",
                f"...and its dependents are blocked, not run against outputs it never wrote "
                f"({statuses['after']})")
        require("future" not in ran and "after" not in ran,
                "neither is executed")
        require(not outcome["failed"],
                "and none of that is a FAILURE -- a pipeline waiting on phase 5 is not a broken "
                "one, and the four states have to stay distinct")

        # 8b. "No inputs" is not "no pattern matched". A stage that reads two things, in a tree
        #     that has one of them, cannot run either -- and the first version launched
        #     `build_collision` on the strength of the manifest existing while the layout it
        #     actually reads was absent, which read as a broken build rather than as a wait.
        partial = fixture() + [
            Stage("half", "g", ["half", "out/half.bin"],
                  ["src/a.txt", "src/nothing/*.json"], ["out/half.bin"])]
        ran.clear()
        outcome = build(partial, root, workspace / "partial.json", runner=fake)
        entry = next(r for r in outcome["results"] if r["stage"] == "half")
        require(entry["status"] == "skipped",
                f"a stage with one input present and one absent is skipped, not run "
                f"({entry['status']})")
        require("src/nothing/*.json" in entry["reason"]
                and "src/a.txt" not in entry["reason"],
                f"...and the reason names the pattern that matched nothing, not the one that did "
                f"({entry['reason']})")
        require("half" not in ran, "and it is not executed")

        # 9. A failing stage blocks its dependents and is reported, and the build fails.
        def failing(command, cwd):
            ran.append(command[0])
            if command[0] == "first":
                return 3, "first: exploded"
            return fake(command, cwd)

        broken_stamps = workspace / "broken.json"
        ran.clear()
        outcome = build(fixture(), root, broken_stamps, runner=failing)
        statuses = {r["stage"]: r["status"] for r in outcome["results"]}
        require(statuses["first"] == "failed" and outcome["failed"],
                f"a failing stage is reported as failed and fails the build ({statuses})")
        require(statuses["second"] == "blocked" and statuses["third"] == "blocked",
                "and everything downstream is blocked rather than run on missing inputs")
        require(any("exploded" in r.get("output", "") for r in outcome["results"]),
                "the failing stage's output is kept, so the reason is in the report")
        require(json.loads(broken_stamps.read_text())["stages"] == {},
                "and a failed stage stamps nothing, so the next run retries it")

        # 10. `--dry-run` runs nothing and writes no stamp.
        (root / "src" / "a.txt").write_text("gamma\n", encoding="utf-8")
        dry_stamps = workspace / "dry.json"
        ran.clear()
        outcome = build(fixture(), root, dry_stamps, dry_run=True, runner=fake)
        require(ran == [] and not dry_stamps.exists(),
                "--dry-run executes nothing and writes no stamp file")
        require(all(r["status"] == "would-run" for r in outcome["results"]),
                f"and reports what it would do "
                f"({[r['status'] for r in outcome['results']]})")

        # 11. `--force` rebuilds even a fresh tree.
        build(fixture(), root, stamps, runner=fake)
        ran.clear()
        build(fixture(), root, stamps, force=True, runner=fake)
        require(sorted(ran) == ["first", "second", "third"],
                f"--force rebuilds everything ({sorted(ran)})")

        # 12. A corrupt stamp file means "rebuild", never "crash".
        stamps.write_text("{not json", encoding="utf-8")
        ran.clear()
        build(fixture(), root, stamps, runner=fake)
        require(sorted(ran) == ["first", "second", "third"],
                f"a corrupt stamp file rebuilds rather than failing ({sorted(ran)}) -- it is a "
                f"cache, and a cache that can break the build is worse than no cache")

        # 13. The real pipeline's graph resolves, and its declared order matches §18.3.
        real = resolve_order(default_stages())
        names = [s.name for s in real]
        require(names.index("collision") < names.index("nav")
                and names.index("collision") < names.index("chunks")
                and names.index("coverage") < names.index("skyexposure")
                and names.index("coverage") < names.index("snowshell"),
                f"the real world chain resolves in dependency order ({names})")
        require(all(s.group == "validate" for s in real[:len([
                    x for x in default_stages() if x.group == "validate"])]),
                "every validator runs before every generator -- a gate that ran afterwards would "
                "be reporting on assets the pipeline had already consumed")
        for stage in default_stages():
            require(bool(stage.description),
                    f"stage {stage.name!r} says what it does, for HOUSE-00217's documentation")
        collision = next(stage for stage in default_stages() if stage.name == "collision")
        require("assets-src/Models/**/*.glb" in collision.inputs,
                "collision must rebuild when an authored _COL proxy changes")

        # 13b. `HOUSE-00227`: the generated exterior tree is IN the pipeline. Four generators wrote
        #      the `.glb` that `build_chunks.py` reads and no stage ran any of them, which is how
        #      `build/terrain` came to hold tiles three hours older than the height field they are
        #      drawn from and `chunks.bin` to be built from both that and a fence tree
        #      `HOUSE-00785` had already replaced.
        by_name = {stage.name: stage for stage in default_stages()}
        exterior = ("terrain-tiles", "road", "fence", "neighbourhood")
        # `HOUSE-00856`: and the neighbourhood's `.glb` reach the runtime through their own stage,
        # which is what closes the gap the four above only halved. A generator whose output no
        # stage consumes is a generator nobody would notice failing.
        require("neighbourhood-bin" in by_name
                and "neighbourhood" in by_name["neighbourhood-bin"].needs,
                "the neighbourhood's meshes are turned into a file the runtime opens")
        require(any(pattern.startswith("build/neighbourhood/")
                    for pattern in by_name.get("neighbourhood-bin", Stage(
                        "x", "x", [], [], [])).inputs),
                "...and that stage hashes the `.glb` it reads")
        require(all(name in by_name for name in exterior),
                f"the four exterior generators are stages "
                f"({[name for name in exterior if name not in by_name]} missing)")
        require(all(by_name[name].outputs for name in exterior if name in by_name),
                "and each of them declares what it writes, so a stale output is a rebuild")
        chunks = by_name["chunks"]
        require(all(name in chunks.needs for name in exterior if name != "neighbourhood"),
                f"`chunks` needs the three whose output it reads ({chunks.needs})")
        # ...and their output is among its INPUTS, not only its `needs`. `needs` rebuilds it when
        # they run in this pipeline; the fingerprint is what notices a generator someone ran by
        # hand, which is every way the tree went stale in the first place.
        read_by_chunks = {"build/terrain", "build/fence"}
        require("build/shell/*.glb" in chunks.inputs,
                f"...and the SHELL it reads by default, which nothing declared until "
                f"`HOUSE-00492` regenerated it ({chunks.inputs})")
        written = {pattern.rsplit("/", 1)[0] for name in exterior if name in by_name
                   for pattern in by_name[name].outputs}
        seen = {pattern.rsplit("/", 1)[0] for pattern in chunks.inputs}
        require(read_by_chunks <= written and read_by_chunks <= seen,
                f"and it hashes what they wrote, so a hand-run generator is noticed too "
                f"({sorted(read_by_chunks - seen)} unhashed)")

        # 14. The documentation is GENERATED from the graph, so it cannot drift from it. A
        #     hand-written order is a second description of the pipeline, and `HOUSE-00217` asks
        #     for exactly the thing that would go stale first.
        generated = documentation(default_stages())
        require(all(f"**`{s.name}`**" in generated for s in default_stages()),
                "every stage appears in the generated table")
        require(all(s.description in generated for s in default_stages()),
                "with what it does")
        table_order = [line.split("**`")[1].split("`**")[0]
                       for line in generated.splitlines() if "**`" in line]
        require(table_order == [s.name for s in resolve_order(default_stages())],
                f"and the table's order IS the build order, not a copy of it ({table_order[:4]}…)")
        spliced = splice_documentation(
            f"intro\n{DOC_BEGIN}\nstale table\n{DOC_END}\noutro\n", generated)
        require(spliced.startswith("intro") and spliced.endswith("outro\n")
                and "stale table" not in spliced,
                "splicing replaces only the generated block and keeps the prose around it")
        try:
            splice_documentation("no markers here", generated)
            raised = ""
        except GraphError as exc:
            raised = str(exc)
        require("no generated block" in raised,
                "a document with no markers is refused, rather than silently overwritten")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("build_content: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=REPO)
    parser.add_argument("--stamps", type=Path, default=DEFAULT_STAMPS)
    parser.add_argument("--force", action="store_true", help="rebuild every stage")
    parser.add_argument("--dry-run", action="store_true", help="print the plan and stop")
    parser.add_argument("--only", help="build one group: validate, world")
    parser.add_argument("--json", action="store_true", help="emit the report as JSON")
    parser.add_argument("--docs", action="store_true",
                        help=f"regenerate the stage table in {DOC_PATH.name}")
    parser.add_argument("--check-docs", action="store_true",
                        help="fail if that table is stale")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    if args.docs or args.check_docs:
        try:
            generated = documentation(default_stages())
            current = DOC_PATH.read_text(encoding="utf-8")
            wanted = splice_documentation(current, generated)
        except GraphError as exc:
            print(f"build_content: {exc}", file=sys.stderr)
            return 2
        if args.check_docs:
            if current != wanted:
                print(f"build_content: {DOC_PATH.relative_to(REPO)} is stale; run "
                      f"tools/ci/build_content.py --docs", file=sys.stderr)
                return 1
            print(f"build_content: {DOC_PATH.relative_to(REPO)} matches the stage graph.")
            return 0
        DOC_PATH.write_text(wanted, encoding="utf-8")
        print(f"build_content: wrote the stage table into {DOC_PATH.relative_to(REPO)}")
        return 0

    started = time.monotonic()
    try:
        outcome = build(default_stages(), args.root, args.stamps,
                        force=args.force, dry_run=args.dry_run, only=args.only)
    except GraphError as exc:
        print(f"build_content: {exc}", file=sys.stderr)
        return 2

    outcome["wallSeconds"] = round(time.monotonic() - started, 3)
    if args.json:
        print(json.dumps(outcome, indent=2, sort_keys=True))
    else:
        print(report(outcome))
        print(f"  wall clock {outcome['wallSeconds']:.2f}s")
    for entry in outcome["results"]:
        if entry["status"] == "failed":
            print(f"\nbuild_content: {entry['stage']} failed:\n{entry.get('output', '')}",
                  file=sys.stderr)
    return 1 if outcome["failed"] else 0


if __name__ == "__main__":
    sys.exit(main())
