# The content build

*`HOUSE-00216` builds it, `HOUSE-00217` is this page. The stage table in §3 is **generated** by
`tools/ci/build_content.py --docs` and checked by `--check-docs` in CI — do not edit it by hand,
and if it looks wrong, the graph in `default_stages()` is what is wrong.*

---

## 1. One command

```bash
cmake --build build --target content        # build what is out of date
cmake --build build --target content-plan   # print the plan, run nothing

tools/ci/build_content.py                   # the same, directly
tools/ci/build_content.py --dry-run
tools/ci/build_content.py --force           # rebuild everything
tools/ci/build_content.py --only world      # one group
tools/ci/build_content.py --json            # for a script
```

Neither target is part of `all`. A content build reads and writes `assets-src/` and `content/`, and
a code build should not.

## 2. Up to date means the same bytes, never a newer mtime

A stage is up to date when the SHA-256 of every input file, plus its exact command line, plus its
list of outputs, matches what the stamp recorded when those outputs were produced. `AGENTS.md` is
the reason, and it is not a stylistic one — its opening argument is that every avoidable rebuild is
irreversible flash wear on one SSD shared by ten agents, and mtime produces avoidable rebuilds in
both directions:

* **git does not preserve mtime.** A clone, a branch switch, a `git stash pop` — each stamps every
  touched file with the time it was written, so an mtime build rebuilds a content tree that has not
  changed at all. Here that is the whole of `assets-src/`.
* **mtime can also claim fresh when it is stale.** Restore an output from a backup, or let a tool
  write one without touching its input, and the output is newer than an input it does not match.

Stamps live in the build directory (`build/content-stamps.json`), never under `assets-src/`. A
corrupt stamp file means *rebuild*, never *crash*: it is a cache, and a cache that can break the
build is worse than no cache.

Three other things force a rebuild, and each has caught a real mistake:

* **the command line changed** — a stage run at `--samples 4` and one at `--samples 64` produce
  different content from identical inputs;
* **an output is missing** — trusting the stamp would leave the build reporting success over a file
  nobody can load;
* **`--force`**.

## 3. The stages, in build order

Every stage names *what it needs*, and the order is a topological sort of that — never the order
the stages happen to be written in. Adding a stage means naming its dependencies and nothing else.

Validators gate the generators by an explicit edge rather than by sorting: a gate that ran after
the generators would be reporting on assets the pipeline had already consumed.

**`needs` also means stale.** A stage whose upstream ran is rebuilt, whatever its own inputs say,
and the report names which upstream did it. Until `HOUSE-00768` this was ordering alone, and the
consequence was a `nav.bin` built against a `collision.bin` that had since been rebuilt under it:
the pet graph sat over the ground the terrain used to be, and every stage reported success.

**The generated exterior tree is part of the pipeline** since `HOUSE-00227`. `terrain-tiles`,
`road`, `fence` and `neighbourhood` write `.glb` into `build/terrain`, `build/fence` and
`build/neighbourhood`, and `chunks` reads the first two — so they are its inputs as well as its
`needs`. Before that no stage ran them at all: `tools/ci/run_checks.sh` gates their **selftests**,
which is a different question from whether their **output** is current, and on 2026-09-10 it was
not. `build/terrain` held tiles written at 12:49 on the 9th from a height field rewritten at 14:56
the same day; `build/fence` held a tree from before `HOUSE-00785` repaired the generator; and
`chunks.bin`, plus fourteen committed render references of the outdoors, were built from both.

**A glob output is satisfied by any match** (`HOUSE-00856`). The "is the output still there" check
asked `exists()` of the pattern itself, and `content/world/*.json` is not a file — so `world-deploy`
and the four stages above reported *output missing* on every run however fresh they were, and
rebuilt. `fingerprint`'s "never hash your own output" rule matches a command token against the
pattern as well as comparing it, so a stage declaring `out/*.bin` and naming `out/a.bin` is covered
by the same fix.

`chunks` also reads **`build/shell`**, which it has done by default since `HOUSE-00473` and which
no stage declared until `HOUSE-00492` regenerated the shell and looked. It is an input now. The
omission was masked because `chunks` fails on every run (`HOUSE-00487`) and therefore reruns
anyway; the day that is fixed, a shell regeneration would have gone unchunked. `build/shell-lm` is
deliberately not an input: it is the *preferred* tree when it exists and the build works without
it, so requiring it would skip the stage on a checkout that has never baked a lightmap.

<!-- BEGIN GENERATED: build_content.py --docs -->

| # | Stage | Group | What it does | Needs | Reads | Writes |
|---|---|---|---|---|---|---|
| 1 | **`anim`** | validate | single-skin models, and every sidecar binds | — | `assets-src/Models/**/*.glb` | — (a gate) |
| 2 | **`fonts`** | validate | every descriptor resolves to a vendored face | — | `assets-src/Fonts/*.spritefont` | — (a gate) |
| 3 | **`layout`** | validate | the directory-set and file-placement gate | — | `tools/ci/check_layout.py` | — (a gate) |
| 4 | **`manifest`** | validate | no unlisted file under assets-src/, no hash mismatch | — | `assets-src/assets.manifest.json` | — (a gate) |
| 5 | **`sky-lut`** | validate | §31.2's generated 32 x 8 x 16 compact sky-colour contract | — | `tools/world/sky_lut.py`<br>`assets-src/world/layout.sky.json` | — (a gate) |
| 6 | **`licences`** | validate | every row has a licence, a licence file and the booleans | `manifest` | `assets-src/assets.manifest.json`<br>`licenses/THIRD-PARTY-ASSETS.md` | — (a gate) |
| 7 | **`cnb-audio`** | compile | compile assets-src/Audio to content/Audio with cna-content | `anim`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut` | `assets-src/Audio/**/*` | — (a gate) |
| 8 | **`cnb-fonts`** | compile | compile assets-src/Fonts to content/Fonts with cna-content | `anim`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut` | `assets-src/Fonts/**/*` | — (a gate) |
| 9 | **`cnb-media`** | compile | compile assets-src/Media to content with cna-content | `anim`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut` | `assets-src/Media/**/*` | — (a gate) |
| 10 | **`cnb-models`** | compile | compile assets-src/Models to content/Models with cna-content | `anim`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut` | `assets-src/Models/**/*` | — (a gate) |
| 11 | **`cnb-textures`** | compile | compile assets-src/Textures to content/Textures with cna-content | `anim`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut` | `assets-src/Textures/**/*` | — (a gate) |
| 12 | **`world-rules`** | world | the twelve rules of §15.7 over the authored layout | `anim`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut` | `assets-src/world/*.json` | — (a gate) |
| 13 | **`collision`** | world | rooms become walls; the layout and the _COL proxies | `anim`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut`, `world-rules` | `assets-src/world/*.json`<br>`assets-src/assets.manifest.json` | `content/world/collision.bin` |
| 14 | **`fence`** | world | §11.2's fences and gates and §11.1's garden structures | `anim`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut`, `world-rules` | `assets-src/world/layout.exterior.json`<br>`assets-src/world/terrain.png` | `build/fence/*.glb` |
| 15 | **`neighbourhood`** | world | §11.4's neighbour houses, impostor cards and street furniture | `anim`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut`, `world-rules` | `assets-src/world/layout.exterior.json` | `build/neighbourhood/*.glb` |
| 16 | **`road`** | world | §11.4's road, kerbs, sidewalks, grates and markings | `anim`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut`, `world-rules` | `assets-src/world/layout.exterior.json`<br>`assets-src/world/terrain.png` | `build/terrain/ROAD_*.glb` |
| 17 | **`terrain-tiles`** | world | §11.5's twenty height-field tiles | `anim`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut`, `world-rules` | `assets-src/world/layout.exterior.json`<br>`assets-src/world/terrain.png`<br>`assets-src/world/terrain_materials.png` | `build/terrain/TERRAIN_*.glb` |
| 18 | **`world-deploy`** | world | strip the comments, deploy as plain JSON, and hash what was written | `anim`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut`, `world-rules` | `assets-src/world/*.json` | `content/world/*.json` |
| 19 | **`chunks`** | world | per-cell static prop batches | `anim`, `collision`, `fence`, `fonts`, `layout`, `licences`, `manifest`, `road`, `sky-lut`, `terrain-tiles` | `assets-src/world/*.json`<br>`assets-src/Models/**/*.glb`<br>`build/terrain/*.glb`<br>`build/fence/*.glb`<br>`build/shell/*.glb` | `content/world/chunks.bin` |
| 20 | **`cnb-world`** | compile | compile assets-src/world to content/world with cna-content | `anim`, `collision`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut` | `assets-src/world/**/*` | — (a gate) |
| 21 | **`coverage`** | world | the rain/roof coverage height field | `anim`, `collision`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut` | `assets-src/world/*.json` | `content/world/coverage.bin` |
| 22 | **`nav`** | world | the pet waypoint graph | `anim`, `collision`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut` | `assets-src/world/*.json` | `content/world/nav.bin` |
| 23 | **`neighbourhood-bin`** | world | §11.4's meshes, keyed by the asset id a row names | `anim`, `fonts`, `layout`, `licences`, `manifest`, `neighbourhood`, `sky-lut`, `world-rules` | `assets-src/world/layout.exterior.json`<br>`build/neighbourhood/*.glb` | `content/world/neighbourhood.bin` |
| 24 | **`shading`** | world | §22's per-window sun-shading grid, 12 x 24 nodes a window | `anim`, `fonts`, `layout`, `licences`, `manifest`, `neighbourhood`, `sky-lut`, `world-rules` | `assets-src/world/layout.openings.json`<br>`assets-src/world/layout.portals.json`<br>`assets-src/world/layout.cells.json`<br>`assets-src/world/layout.exterior.json`<br>`build/shell/*.glb`<br>`build/neighbourhood/*.glb` | `content/world/shading.bin` |
| 25 | **`skyexposure`** | world | per-cell sky and facade exposure | `anim`, `coverage`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut` | `assets-src/world/*.json` | `content/world/skyexposure.bin` |
| 26 | **`snowshell`** | world | the snow shells over up-facing exterior surfaces | `anim`, `coverage`, `fonts`, `layout`, `licences`, `manifest`, `sky-lut` | `assets-src/world/*.json`<br>`assets-src/Models/**/*.glb` | `content/world/snowshell.bin` |

The command each stage runs:

```
anim           python3 tools/ci/check_anim_assets.py
fonts          python3 tools/ci/check_fonts.py
layout         python3 tools/ci/check_layout.py
manifest       python3 tools/ci/check_manifest.py
sky-lut        python3 tools/world/sky_lut.py --check
licences       python3 tools/assets/verify_licences.py --check
cnb-audio      /rv/data/development/github.com/libcna/house-simulator/build/CNA_BUILD/cna-content build assets-src/Audio -o content/Audio --quiet
cnb-fonts      /rv/data/development/github.com/libcna/house-simulator/build/CNA_BUILD/cna-content build assets-src/Fonts -o content/Fonts --quiet
cnb-media      /rv/data/development/github.com/libcna/house-simulator/build/CNA_BUILD/cna-content build assets-src/Media -o content --quiet
cnb-models     /rv/data/development/github.com/libcna/house-simulator/build/CNA_BUILD/cna-content build assets-src/Models -o content/Models --quiet
cnb-textures   /rv/data/development/github.com/libcna/house-simulator/build/CNA_BUILD/cna-content build assets-src/Textures -o content/Textures --quiet
world-rules    python3 tools/world/validate_world.py assets-src/world
collision      python3 tools/world/build_collision.py
fence          python3 tools/world/fence_gen.py
neighbourhood  python3 tools/world/neighbourhood_gen.py
road           python3 tools/world/terrain_gen.py --road
terrain-tiles  python3 tools/world/terrain_gen.py --tiles
world-deploy   python3 tools/world/deploy_world.py
chunks         python3 tools/world/build_chunks.py
cnb-world      /rv/data/development/github.com/libcna/house-simulator/build/CNA_BUILD/cna-content build assets-src/world -o content/world --quiet
coverage       python3 tools/world/build_coverage.py
nav            python3 tools/world/build_nav.py
neighbourhood-bin python3 tools/world/build_neighbourhood.py
shading        python3 tools/blender/shading_factor.py build/shell --world assets-src/world --neighbourhood build/neighbourhood --out content/world
skyexposure    python3 tools/world/build_skyexposure.py
snowshell      python3 tools/world/build_snowshell.py
```

<!-- END GENERATED -->

## 4. The five outcomes, and why *skipped* is not *failed*

| Outcome | Means |
|---|---|
| **built** | it ran |
| **fresh** | its inputs, command and outputs are unchanged — **and nothing it needs rebuilt** |
| **skipped** | one of its input patterns matches nothing yet |
| **blocked** | something it needs was skipped or failed |
| **FAILED** | the tool exited non-zero; the build fails |

Most of the world and Blender stages are **skipped** today: they read `assets-src/world/*.json`,
which phase 5 has not authored, and the house shell, which phase 6 has not generated. `make content`
has to be runnable now, so waiting for phase 5 is reported as waiting — not as success, and not as
failure. A build that quietly succeeded by doing nothing would be the worst of the five.

"No inputs" is **not** "no pattern matched". A stage that reads the layout *and* the asset manifest,
in a repository that has the manifest and not the layout, has one of the two and cannot run; the
first version of the runner launched `build_collision` on the strength of the manifest existing, and
it failed inside the tool — which reads as a broken build rather than as a wait. Every input pattern
must match at least one file, and the skip reason names the pattern that did not.

A failed stage **stamps nothing**, so the next run retries it, and its output is kept in the report
so the reason is in front of you.

## 5. Rebuilding one asset

**One stage:** run its command from the table above, directly. It is an ordinary tool with ordinary
arguments and no hidden state; the runner adds nothing but ordering.

**One stage and everything downstream of it:** delete its outputs and run `content`. A missing
output rebuilds that stage, and its dependents follow because their inputs changed.

**One asset inside a stage:** most tools take the asset as an argument —
`tools/blender/lod_gen.py IN.glb OUT.glb`. Do that, then run `content`, which will notice the
changed bytes and rebuild whatever reads them.

**Everything:** `--force`. Please have a reason; see §2.

## 6. Adding a stage

1. Add a `Stage(...)` to `default_stages()` in `tools/ci/build_content.py`: its name, group,
   command, input patterns, output paths, what it `needs`, and one line saying what it does.
2. Run `tools/ci/build_content.py --docs` to regenerate §3.
3. Run `tools/ci/build_content.py --selftest`.

The description is not decoration — §3 is generated from it, and the selftest requires every stage
to have one.
