# Conventions

Everything in this file is enforced by a tool, by a review, or by both. Where a grammar is given,
it is the grammar the validator uses; a name that does not match is a load-time or build-time
error, never a runtime surprise.

---

## 1. Identifiers in world data

All world-data ids share one outer grammar:

```
^[A-Z][A-Z0-9_]*$
```

Uppercase, digits and underscores, starting with a letter. Ids are the contract between the JSON,
the C++, the save file and the offline tools, so they are **permanent**: renaming one is a
migration ([ADR-0008](decisions/ADR-0008-save-format.md)), not an edit.

Within that grammar each kind has its own shape. `<LVL>` is one of `B1`, `L0`, `L1`, `L2`, `L3`;
`<ROOM>` is a room slug in `[A-Z][A-Z0-9]*(_[A-Z0-9]+)*`.

| Kind | Grammar | Example |
|---|---|---|
| Level | `^(B1|L0|L1|L2|L3)$` | `L1` |
| Interior cell | `^(B1|L0|L1|L2|L3)_[A-Z][A-Z0-9_]*$` | `L0_KITCHEN` |
| Exterior cell | `^EXT_[A-Z][A-Z0-9_]*$` | `EXT_GARDEN_N` |
| Portal | `^P_[A-Z0-9_]+__[A-Z0-9_]+(__[A-Z0-9]+)?$` — the two cell ids separated by a **double** underscore, plus an optional discriminator when a pair has several portals | `P_L0_HALL__L0_KITCHEN`, `P_L1_MASTER_BED__EXT_WORLD__W1` |
| Door entity | `^DOOR_[A-Z0-9_]+$` | `DOOR_L1_MASTER` |
| Window entity | `^WIN_[A-Z0-9_]+$` | `WIN_L1_MASTER_S1` |
| Light | `^LIGHT_[A-Z0-9_]+$` | `LIGHT_L0_KITCHEN_ISLAND_1` |
| Light group (switch group) | `^LG_[A-Z0-9_]+$` | `LG_L0_KITCHEN_MAIN` |
| Material | `^MAT_[A-Z0-9_]+$` | `MAT_TILE_PORCELAIN_GREY` |
| Prop placement | `^PROP_[A-Z0-9_]+$` | `PROP_L0_KITCHEN_FRIDGE` |
| Asset (manifest row) | `^(MODEL|TEX|SFX|AMB|MUSIC|FONT|VIDEO|FX)_[A-Z0-9_]+$` | `MODEL_PROP_KITCHEN_FRIDGE_01` |
| Sound cue | `^(SFX|AMB)_[A-Z0-9_]+$` | `SFX_FRIDGE_OPEN`, `AMB_KITCHEN` |
| Interactable | `^[A-Z][A-Z0-9_]*$`, conventionally `<KIND>_<CELL>[_<N>]` | `FRIDGE_L0_KITCHEN`, `DRAWER_L0_KITCHEN_07` |
| Item | `^ITEM_[A-Z0-9_]+$` | `ITEM_MUG_03` |
| Nav region / perch | `^NAV_[A-Z0-9_]+$` / `^PERCH_[A-Z0-9_]+$` | `NAV_L0_KITCHEN` |
| Weather archetype | `^W_[A-Z0-9_]+$` | `W_PARTLY`, `W_RAIN` |
| Residency pack | `^[a-z][a-z0-9-]*$` — **lower-case**, because a pack is a content path component | `house-l0` |
| Animation clip | `^[a-z][a-z0-9_]*$` — lower-case, because clips are keys inside a `.chanim` sidecar | `walk_forward` |

Two rules that catch real mistakes:

* **Uniqueness is global, not per-kind.** One id space, one uniqueness check (`validate_world.py`
  rule 1). Two different things may never share an id even if their prefixes differ in intent.
* **The cell prefix must be true.** An id beginning `L0_` must belong to level `L0`. The validator
  checks it, because a copy-pasted room row that keeps the old prefix is the most common authoring
  error there is.

---

## 2. Content names

Content names are the strings passed to `ContentManager::Load<T>()`. They are **never** OS paths
(`cna-house.md` section 8.3).

```
Category/Sub/name          e.g. Props/Kitchen/fridge-01
```

* Directory components are `PascalCase`: `Models`, `Textures`, `Audio`, `Fonts`, `Video`,
  `Effects`, `Props`, `Kitchen`.
* The leaf name is `lower-case-with-hyphens`: `fridge-01`, `oak-floor-01`, `door-close-heavy`.
* No extension. `ContentManager` appends it.
* A build check rejects two content names differing only by case, because Linux would accept them
  and Windows and macOS would not.

Source files under `assets-src/` use `snake_case` (`fridge_01.glb`); the content build maps them to
hyphenated content names. That asymmetry is deliberate: `snake_case` matches Blender and glTF
authoring habits, hyphens match the content-name grammar.

---

## 3. C++ files, types and namespaces

| Thing | Convention | Example |
|---|---|---|
| Namespace | all lower case, one per subsystem, under `cnahouse` | `cnahouse::visibility`, `cnahouse::anim` |
| File name | `PascalCase`, one primary type per file | `PortalTraversal.hpp` |
| Header location | `include/cnahouse/<subsystem>/<Type>.hpp` for anything another subsystem uses | `include/cnahouse/world/WorldData.hpp` |
| Implementation | `src/<subsystem>/<Type>.cpp`, mirroring the header path | `src/world/WorldData.cpp` |
| Private header | beside the `.cpp` in `src/<subsystem>/`, never in `include/` | `src/world/CellGrid.hpp` |
| Type | `PascalCase` | `PortalRuntime` |
| Function / method | `PascalCase` for anything XNA-facing or public, matching CNA and XNA | `Update`, `SweepCapsule` |
| Local variable, parameter | `camelCase` | `visibleCells` |
| Member variable | `m_camelCase` | `m_apertureFraction` |
| Constant, `constexpr` | `kPascalCase` | `kMaxBonesPerModel` |
| Enum | `enum class`, `PascalCase` type, `PascalCase` enumerators | `PortalOpacity::OpaqueWhenClosed` |
| Macro | avoid; where unavoidable, `CNAHOUSE_SCREAMING_CASE` | `CNAHOUSE_TIER_E` |
| Include guard | `#pragma once` | |

Namespaces map one-to-one onto the 18 subsystem directories, with two aliases the architecture
already uses in prose: `cnahouse::anim` for `src/animation/` and `cnahouse::render` for
`src/rendering/`. `check_layout.py` asserts the directory set; reviewers assert the namespace.

**Ordering of includes** (clang-format sorts within each block; the blocks are ours):

1. the header this `.cpp` implements;
2. `Microsoft/Xna/Framework/...`;
3. `System/...`;
4. `cnahouse/...`;
5. the C++ standard library.

**Formatting is not a matter of opinion.** `.clang-format` decides: 4 spaces, 110 columns, Allman
braces, pointers and references bound to the type (`Model* model`, `const Matrix& world`).
`clang-format --dry-run -Werror` runs in CI.

---

## 4. JSON keys

* Keys are `camelCase`: `floorMaterial`, `soundLoss`, `coneOuterDeg`.
* Every file begins with `"schema": "cna-house/<kind>/<version>"`, where `<version>` is an integer
  that increments on any incompatible change.
* Units are named in the key when they are not the project default: the default is **metres**,
  **seconds**, **radians in code / degrees in data**, **kelvin** for colour temperature and
  **lumens** for luminous flux. So: `yawDeg`, `coneInnerDeg`, `colorK`, `intensityLm`,
  `epochSeconds`, `targetExpiry` (seconds). A bare `position`, `range` or `width` is metres.
* Vectors are `[x, y, z]` arrays in world axes (`cna-house.md` section 14: right-handed, Y-up,
  −Z north). Axis-aligned ranges are `{"x": [min, max], "z": [min, max]}`.
* Booleans are for genuinely two-state things (`locked`, `plug`, `defaultOn`). **Continuous
  quantities are floats**, and `check_xna_only.py` rejects `isRaining`-style members outright:
  weather, apertures, wetness and flow are all continuous.
* `null` means "not specified, use the default", and the default is documented in
  [`world-format.md`](world-format.md). It never means zero.
* Comments are permitted in the authored files (JSONC) and stripped by the build; the deployed
  copies are plain JSON.

---

## 5. Error-handling policy

Three mechanisms, with a clear boundary between them. The rule that decides which: **can the
program sensibly continue?**

### 5.1 `Result<T>` — recoverable failures

`cnahouse::util::Result<T>` (`include/cnahouse/util/Result.hpp`) is the default for anything a
caller can reasonably handle: a world file that does not parse, a save whose checksum is wrong, an
expression with an unknown token, a portal rectangle that is not in its plane.

```cpp
util::Result<WorldData> LoadWorld(std::string_view root);

auto world = LoadWorld(root);
if (!world)
{
    Log::Error(LogCat::World, "{}", world.Error().Message());
    return false;
}
UseWorld(*world);
```

* `Result<T>` is `[[nodiscard]]`. Ignoring a failure is a compile warning, and warnings are errors.
* The error payload is `util::Error`: a stable `ErrorCode`, a human message, and an optional
  context string naming the file and the JSON path. Messages name **what was expected**, not only
  what was found.
* `Result<void>` exists for operations with no value.
* Errors are **accumulated at boundaries** where a batch is validated — the world loader reports
  every bad row, not the first — because fixing 40 authoring mistakes one build at a time is
  intolerable.

### 5.2 Exceptions — the `Game` boundary, and genuinely exceptional content failures

* XNA itself throws: `ContentLoadException`, `NotSupportedException`,
  `InvalidOperationException`. We catch them where the design says to — in particular the Tier-E
  effect load ([ADR-0003](decisions/ADR-0003-render-tiers.md)) — and we do not pretend they do not
  exist.
* `cnahouse` throws only from `Bootstrap`/`LoadContent` when the program genuinely cannot start:
  the world data is absent or invalid, or a required content root is missing. `CnaHouseGame`
  catches at the top, logs, and shows the failure screen (`cna-house.md` section 73).
* Exceptions are **never** used for control flow, never for a condition the caller was expected to
  test, and never across a per-frame path.

### 5.3 `assert` — invariants

`assert` states something the code believes must be true because an earlier stage guaranteed it: a
cell index in range after the loader validated it, a normalised quaternion after the clip player
normalised it. An `assert` is not input validation. If a condition can be caused by data, it is a
`Result`, not an assert.

Release builds keep asserts **on** in `RelWithDebInfo` and off in `Release`; the invariants they
guard are also covered by unit tests, so a disabled assert never hides an untested claim.

### 5.4 What never happens

* Silently returning a default on failure.
* `catch (...)` that swallows.
* Logging an error and continuing as though it had not happened.
* An error message that does not say which file, which id and what was expected.

---

## 6. Task ids and commits

Task ids are `HOUSE-` plus five digits, permanent, never renumbered, and each belongs to its
phase's reserved range (`plan.md`). One task is one commit; the message names the id; the checkbox
moves in the same commit. See [`workflow.md`](workflow.md).
