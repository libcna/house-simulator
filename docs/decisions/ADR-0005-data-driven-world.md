# ADR-0005 — A data-driven world: JSON schemas and a closed expression vocabulary

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-06 |
| **Task** | `HOUSE-00011` |
| **Depends on** | [ADR-0001](ADR-0001-xna-only.md) |
| **Owns** | `cna-house.md` §15, §16 |

## Context

The building has 5 levels, 78 interior cells, 17 exterior cells, ~186 portals, ~640 interactables,
~2 400 static prop placements, a light per fixture and a material per surface. All of it needs to
be consumed by at least six systems — visibility, rendering, collision, audio, interaction and pet
navigation — and by at least four offline tools (validation, collision build, navigation build,
lightmap bake).

Two things could hold this: C++ or data. C++ would put 95 room definitions into translation units,
recompile the world for a moved doorway, and make the offline tools parse C++ or duplicate the
truth.

## Decision

**The house is data, not code.** A machine-readable floor plan under `assets-src/world/` is the
single source of truth for geometry, collision, portals, lighting zones, audio zones, interaction
placement and asset residency. C++ contains *systems*; the building contains no hard-coded rooms.

* Seventeen JSON files, each with a `"schema": "cna-house/<kind>/<version>"` header:
  `world.manifest.json`, `layout.levels.json`, `layout.cells.json`, `layout.portals.json`,
  `layout.openings.json`, `layout.stairs.json`, `layout.lights.json`, `layout.props.json`,
  `layout.materials.json`, `layout.nav.json`, `layout.audio.json`, `layout.exterior.json`,
  `layout.weather.json`, `layout.sky.json`, `interactables.json`, `initialstate.json`,
  `assets.manifest.json`. The full reference is [`docs/world-format.md`](../world-format.md).
* They are deployed as **raw `.json`** alongside the compiled content and read with
  `System::IO::File` + `System::Text::Json`. They are data the *game* owns, not framework content,
  so no `ContentTypeReader` registration is needed — and no CNA pipeline extension, which we could
  not add without modifying CNA.
* `world.manifest.json` lists every member file with its SHA-256 and a combined `worldHash`. The
  save format stores that hash, which is how a save survives world edits gracefully
  ([ADR-0008](ADR-0008-save-format.md)).
* Everything is addressed by **stable string id** matching `^[A-Z][A-Z0-9_]*$` —
  `L0_KITCHEN`, `P_L0_HALL__L0_KITCHEN`, `LG_L0_KITCHEN_MAIN`, `FRIDGE_L0_KITCHEN`. Ids are the
  contract between the data, the code, the save file and the tools.
* **Validation is a build step, not a hope.** `tools/world/validate_world.py`, mirrored by a C++
  validator used by the unit tests, enforces the eleven rules of `cna-house.md` §15.7 — including
  that no two cells on a level overlap by more than 1 cm², that every portal rectangle lies in both
  cells' boundary planes within 1 cm, that the graph is connected from `L0_FOYER`, that stair rises
  sum to the level difference within 1 mm, and that every interactable's focus point is provably
  reachable by a 2.5 m ray from a standing eye position. A failure fails the build.

**The closed expression vocabulary.** Interactable actions carry `when` and `do` strings:

```jsonc
{ "verb": "Open", "when": "state.doorOpen == false", "do": "setDoor(true)", ... }
```

These are **not a scripting language**. They are parsed *at load time* into a small fixed
expression tree over the interactable's own typed state fields, with a **closed vocabulary of
operations per interactable kind** — a refrigerator understands `setDoor`, a tap understands
`setFlow`, and neither understands the other's verbs. An unknown token is a **load-time error**
naming the file, the id and the token; it is never a runtime surprise, and there is no way to
express a loop, a call, an allocation or an arbitrary side effect.

## Alternatives considered

**Hard-coded C++ world definition.** Rejected: recompilation per doorway, no tool access to the
truth, and 95 rooms of initialiser lists that no one can validate.

**A binary world format.** Rejected for the authored layer. JSON is diffable, reviewable, editable
by hand and inspectable when a tester reports a problem, and the parse cost is irrelevant against
a load that also decompresses textures. Derived data that is genuinely large *is* binary —
`content/world/collision.bin` is built offline from the same JSON.

**A general embedded scripting language (Lua, a bytecode VM, an expression JIT).** Rejected. It
would be a second, unvalidated source of behaviour with its own failure modes, its own security
surface, its own portability question on Emscripten, and no way to prove at build time that a
world file is correct. The closed vocabulary gives the declarative benefit — behaviour lives with
the data — without inventing a VM.

**Booleans in the data for continuous quantities.** Rejected project-wide and linted:
`check_xna_only.py` rejects any member matching `is(Raining|Snowing|Windy|Stormy)`. Weather,
apertures, wetness and flow are continuous; a boolean would quietly re-introduce the state
machine the design removed.

**A GUI level editor.** Explicitly a non-goal (`cna-house.md` §3). The JSON is editable by hand and
generated by tools; a GUI would be a second project.

## Consequences

* Tooling comes first: nothing can be authored until `validate_world.py` exists, which is why
  phase 5 opens with the schema and the validator rather than with rooms.
* The C++ loader must produce error messages that name the file, the JSON path and what was
  expected — a requirement inherited by `util/Json` (`HOUSE-00028`).
* The world data is versioned by schema string and hashed as a whole, so both the loader and the
  save system can detect drift and respond deliberately.
* Systems consume, they do not own: `VisibilitySystem`, `AudioSystem`, `PhysicsSystem`,
  `PetSystem`, `InteractionSystem` and `LightingSystem` all read the same immutable `WorldData`
  loaded once.
