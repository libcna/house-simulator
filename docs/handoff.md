# Session handoff — 2026-09-11

*Written for the next agent picking this repository up on a fresh context. It records what this
session did, what it found, what is owed, and exactly where to start. It is a snapshot and will go
stale: `plan.md` is the authority, always.*

---

## 1. Where the repository is

| | |
|---|---|
| Branch | `develop`, pushed to `origin/develop` |
| HEAD at session start | `731c5be` |
| HEAD at session end | `6fb8e86` |
| Commits this session | 9 |
| Working tree | clean |
| Tasks done / open | **472 / 867** (was 464 / 874) |
| Dependency-valid unfinished MUST tasks | 74 |

Sibling repositories, recorded and **not modified by this session**:

| Repository | HEAD | Note |
|---|---|---|
| `cnanext` | `c1c017cd79d3f3ffe06d0abff4dd0b5285b61438` (`next`) | **A vulkan→next merge is staged uncommitted in its working tree.** See §5. |
| `sharp-runtimenext` | `0c82d9b888bdf5f7d5663c77942f339bcb2a7445` (`next`) | untouched |

### Test and gate state

* **1 290** unit + integration tests pass (`ctest --test-dir build -L 'unit|integration' -j3`).
* **35 / 35** render tests passed earlier in the session, at `731c5be` against the pre-merge
  `cnanext`. **They have not been re-run since** — see §5, this is the one thing owed.
* `tools/ci/run_checks.sh` — all gates green, including the two new ones this session added.

**Count tests from the binary, not from `ctest`.** `gtest_discover_tests(... DISCOVERY_MODE
PRE_TEST)` makes `ctest -N` report whatever the last build left behind; two calls minutes apart
legitimately answered 142 and 1141 here. The authoritative count is
`./build/cnahouse_unit_tests --gtest_list_tests | grep -c '^  '`. This is written into
`tests/CMakeLists.txt` beside the call.

---

## 2. What this session completed

Nine commits, oldest first.

| Commit | Task(s) | What |
|---|---|---|
| `e34cea8` | — | What *"the culled and unculled frames match"* actually measures |
| `0826491` | `HOUSE-01561` | §32.1's `SunModel`, against 390 USNO-published rise/set times |
| `834ebfe` | `HOUSE-01562`, `HOUSE-01563` | §32.2's sun direction, the colour LUT, and a continuous day length |
| `91ce2ac` | `HOUSE-01251` | §28.1's per-room `LightingSystem` over the house's 243 fixtures |
| `f84e564` | — | `shading_factor.py` could not be run against the real house, twice over |
| `ee3c47b` | `HOUSE-01279` | Baked §22's sun-shading grid; a third defect in the tool |
| `902e572` | `HOUSE-01700` | The `isRaining` lint caught four names and missed fourteen |
| `5afbcda` | `HOUSE-02841` | `fopen`, the POSIX doors, and the single-thread rule |
| `6fb8e86` | `HOUSE-01263` | §28.4's daylight model — and the section's `shadingFactor` is wrong |

Tasks closed: **`HOUSE-01251`, `01263`, `01279`, `01561`, `01562`, `01563`, `01700`, `02841`**.
One new task was created (`HOUSE-01279`) and one was created and then **withdrawn** — see §4.

### The subsystems that now exist

* **`cnahouse::environment` — the sun.** `SunModel` (position, rise/set/transit by bisecting the
  model's own altitude), `SunLight` (world direction, §32.2's 64-entry colour LUT, cloud factors),
  and a continuous seasonal day length on `SimClock`.
* **`cnahouse::lighting` — the house's light.** `RoomLightState` / `LightingSystem` (per-cell
  artificial level, running in the frame at `UpdateStage::Lighting`), `ShadingGrid` (the `CSHF`
  reader plus a sky-view factor), and `DaylightModel` (§28.4's five-factor sum).
* **`content/world/shading.bin`** is now produced by a `shading` stage in the content build. It is
  the first stage that needs Blender, and the build's skip rule was generalised for it.

---

## 3. Start here

**`HOUSE-01564` — wire the sun into `LightingSystem`.** Everything it needs now exists and nothing
else is closer to being a visible, integrated change.

* `SunPositionFor(clock)` gives altitude and azimuth; `SunDirection` gives the vector an XNA
  `DirectionalLight` takes; `SunShadingFor` gives colour and intensity.
* `DaylightModel::Evaluate(alt, az, cloud, out)` fills a per-cell array in the world's cell order,
  which is the same order `LightingSystem::Cells()` is in — they were built to line up.
* `RoomLightState::daylight` is **deliberately left at 0** and
  `LightingSystemTests.TheDaylightAndBorrowedFieldsAreZeroAndThatIsDeliberate` asserts it is.
  **That test is written to be deleted by whoever lands this.** It exists so that "the daylight
  model is not wired yet" cannot be mistaken for "the daylight model is broken".
* `CnaHouseGame` already loads `layout.lights.json` and constructs `lighting_`. It does **not** yet
  load openings, interactables or `shading.bin`; wiring the daylight means adding those.
  `DaylightModel` cannot honour `initialstate.json`'s open window without `LoadInteractables` —
  see the portal finding in §4.

Then, in rough order of value:

| Task | Why it is next |
|---|---|
| `HOUSE-01566` | The sun disc. With `01564` this makes the sun *visible*, which unblocks `HOUSE-01544`'s owed screenshots. |
| `HOUSE-01544` | **Code complete, box unticked.** Both acceptance criteria are measured and met; its `verify` line also names screenshot scenes that need `01564` and `01566`. It closes the day those scenes can be taken. |
| `HOUSE-01255` | The Planckian LUT. `LightingSystem` has no colour yet, and §28.1's `ambientColor` needs it. |
| `HOUSE-01265` | The 2-hop light flood. `RoomLightState::borrowed` is the other field left at 0. |
| `HOUSE-01574` | Twilight thresholds. `SunModel` already exposes the three constants and `SunDayFor` solves for any of them. |
| `HOUSE-01545` | The outdoor temperature model — small, and the season and clock foundation is all there. |

---

## 4. Findings a future agent must not re-discover

These cost real time to establish. They are all recorded in `plan.md` against their tasks; this is
the index.

### About the architecture document

* **§28.4's `shadingFactor` multiplies the DIRECT term only.** The section writes it multiplying
  the whole of `skyExposure`, and taken literally *every north-facing room in the house is pitch
  dark from dawn to dusk*. The baked grid is a sun-direction occlusion mask — 0 for every direction
  behind the window's own wall — so a north window's factor is 0 whenever the sun is in the south,
  which at 40° N is always. Measured: `L0_FAMILY` read exactly `0.000` at a 45° sun due south. The
  diffuse term takes `ShadingGrid::SkyViewFactor` instead, derived from the same baked grid.
* **§28.4's `max(0, sinh(sunAltitude))` is `sin h`**, the sine of the altitude — standard solar
  notation, and what irradiance on a horizontal surface scales with. The hyperbolic sine is
  unbounded and reaches 1.65 at this latitude.
* **§22's *"the west neighbour's gable shades the study at sunset"* is not literally true.** The
  neighbours that shade `L0_OFFICE` are across the road to the south; nothing stands due west of it.
  The substance holds and is measured (−14.5 % of its sky); the compass point does not.
* **§22's *"porch roof"* over the foyer is `L1_BALCONY_FRONT`'s slab.** Deleting `L0_PORCH` alone
  moves the foyer 22 → 25 lit nodes; deleting the balcony moves it 22 → 65.

### About the world data

* **`CellKind::Exterior` is not "outdoors".** `EXT_SHED` is an exterior cell *and* a shed — walls,
  roof, one window, `visibilityHint: opaque`. Outdoors is `Exterior` **and** `visibilityHint::Open`
  together, which picks out exactly the 17 cells a person would call outside.
* **An interactable and its opening are joined by PORTAL, never by id.** `interactables.json` calls
  a window `WIN_L1_MASTER_N2`; `layout.openings.json` calls the same window `WIN_L1_MASTER_BED_2`.
  **All 54 window interactables differ from their opening's id this way.** Matching on the id finds
  nothing and fails silently.
* `MODEL_DELIVERY_VAN` and `MODEL_PARKED_CAR` are referenced by four `layout.exterior.json` rows and
  do not exist in the neighbourhood model library. This is known and tracked: they are
  `HOUSE-00847`'s, and `build_neighbourhood.py` asserts it.
* The `snowshell` content stage **fails on every run** for want of `assets-src/world/layout.materials.json`.
  That is `HOUSE-00778`, still open, and predates this session.

### About the tooling

* `tools/blender/shading_factor.py` had **three defects**, all invisible to its 22 original claims,
  and the common cause is worth remembering: *every one of those claims built its geometry directly
  with `bpy.data.meshes.new` and not one ever imported a `.glb`*, in a tool whose only job is to
  ray-cast against imported geometry. The defects were the glTF importer's Y-up→Z-up conversion, a
  ray epsilon smaller than the window sash the shell draws, and sampling the portal rectangle
  instead of the glazed aperture. It has **53 claims** now and each new one was proved by injecting
  the bug back.
* `src/util/Log.cpp` uses `std::fopen` for the debug log sink. §8.3 forbids POSIX file access
  outside `SaveStore`'s desktop implementation. It is recorded as a **path exemption** in
  `check_xna_only.py` naming `HOUSE-02842`, and the exemption is checked for staleness — it will
  fail the day `HOUSE-02842` lands and nobody deletes it.

### About C++ in this codebase

* **`DaylightModel` keeps a pointer to its `ShadingGrid`.** Passing a temporary dangles, and the
  symptom is not a crash — every window reads a shading factor of 1.0, so the baked grid appears to
  make no difference and the test that checks it appears to have found a content bug. The rvalue
  overload is deleted, so the same mistake is now a compile error. Other systems in this repository
  hold references the same way; the trap generalises.

---

## 5. What is owed, and the constraint that put it there

> **RENDER QUALIFICATION OWED.** The render suite has not been run since `731c5be`.

The whole of `cnanext` is mid-merge. A peer session (`cnanextmerge`) is landing **vulkan → next**:
331 commits, 20 conflicting files, and at the time of writing the merge is **resolved, compiling,
and staged uncommitted** in that repository's working tree. That session asked this one to:

1. **not build against `cnanext` until it reported green** — it since has, and this session
   rebuilt and re-ran 1 290 unit + integration tests against the merged tree, all passing;
2. **hold the render suite until it releases `:99`** — its own full `ctest` was running there.
   *This is still outstanding.* Do not take `:99` without checking.

### What to do when `:99` is released

Run `DISPLAY=:99 ctest --test-dir build -L render -j1 --output-on-failure`, and **ask which
`cnanext` commit it is against before recording the result** — a number measured against an
uncommitted working tree has no address.

The comparison baseline, measured this session at `f84e564` against pre-merge `c1c017cd7`:

> `CullingSanityRenderTests`, worst pose `l0-sunroom`: **349 of 225 792 compared pixels differ
> (0.1546 %)**, max channel delta **104**, mean **0.072**. Tolerance 2 per channel, threshold
> **0.2 %** of compared pixels.

The merge touched EasyGL's sprite sampler path — both branches independently added a `SamplerState`
W-address hook and the merged renderer now also applies mip state. The peer verified that the
`-1` "not supplied" sentinel is guarded in both flush paths and that `WrapR` is written
unconditionally, so there is no frame-order-dependent path. The 18 culling poses are `BasicEffect`
blockout geometry and *should* be untouched; `TitleScreenRenderTests` and the five
`FontRenderTests` are the sensitive ones, because they do nothing but draw text.

**If `CullingSanityRenderTests` moves materially, that is a regression and Phase 9 is closed
around it.** Do not update a baseline to accommodate it.

---

## 6. Standing rules this session worked under

Beyond `CLAUDE.md` and `AGENTS.md`, which are authoritative:

* **Independent oracles, always.** `HOUSE-01561` was blocked for a whole previous session rather
  than close it on a NOAA implementation checking a NOAA implementation. It closed against the US
  Naval Observatory's published times. The calendar is checked against Python's `datetime`. The
  seasonal day length is checked against the closed-form sunrise equation *and* the USNO. Keep this.
* **A number in a document is a claim to be tested, not a fact to be quoted.** Three of this
  session's most useful findings are places where `cna-house.md` was wrong and the code proved it.
  Make the smallest justified correction, record why, never deviate silently.
* **Do not tick a box on half a `verify` line.** `HOUSE-01544` is code-complete with both
  acceptance criteria measured and stays open because its screenshots cannot be taken yet.
* **Prove a new test claim by injecting the bug it is meant to catch.** Every claim added to
  `shading_factor.py` and `check_xna_only.py` this session was proved that way.
* **Never delete a build directory another session may be using**, and keep to the closed list of
  build directory names. This machine runs about ten agents.

---

## 7. Things that are fine and may look alarming

* `content/` is **gitignored and generated**. `shading.bin` living there is correct.
* The `world` content group regenerates `road`, `terrain-tiles`, `chunks` and `world-deploy` on a
  run, reporting *"inputs or command changed"*. Verified this session: **every regenerated file is
  byte-identical** to what the build tree already held. It is stamp bookkeeping, not change.
* `HOUSE-00497` appears in commit `f84e564`'s message as a shell defect and **does not exist in
  `plan.md`**. It was created on a wrong diagnosis and withdrawn in the very next commit when the
  defect turned out to be in the tool's sampler rather than in the shell. Its id is free.
* `HOUSE-01538` (clock persistence) is still blocked on there being no save model. That is
  expected and is not a stop condition.
