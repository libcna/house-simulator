# ADR-0008 — Delta save format, versioning and migration policy

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-06 |
| **Task** | `HOUSE-00014` |
| **Depends on** | [ADR-0005](ADR-0005-data-driven-world.md) |
| **Owns** | `cna-house.md` §65, §66 |

## Context

Goal G5 is that the house remembers its state: doors, windows, lights, drawers, the fridge, the
taps, the toilets, the television, the pets, the clock and the weather. That is ~640 interactables
plus the player, the clock, the weather vector, the pets and some statistics.

The world data is under active development for the whole life of the project. Interactables will be
added, renamed and removed *after* saves exist — including saves belonging to whoever is testing
the build. A format that breaks on every world edit would be abandoned within a week, and one that
silently guesses would produce bugs no one can reproduce.

## Decision

**A JSON delta against the canonical initial state, versioned, checksummed and migrated.**

* **Never serialise raw memory.** No `memcpy` of structs, no pointer values, no
  implementation-defined layout. The format is written by hand and readable by a human.
* **Stable string ids everywhere** — `L0_KITCHEN`, `DOOR_L1_MASTER`, `LG_L0_KITCHEN_MAIN`. A save
  written before a prop moved still finds its entities.
* **Delta against `initialstate.json`.** Only entities differing from the canonical initial state
  are written. A fresh house saves ~4 KB; a heavily explored one ~90 KB. This is not primarily a
  size optimisation: it makes adding an interactable **backward compatible by construction**,
  because an entity the save has never heard of simply uses its canonical state.
* **JSON via `System::Text::Json`** (sharp-runtime) — human-readable and diffable, which is what
  makes "the fridge was open when I quit" a five-second investigation rather than a hex dump.
* **Atomic writes.** Write `save.tmp`, flush, rename over `save.json`, keep `save.bak`. A crash
  mid-save can lose the newest state; it can never produce a half-written save.
* **Validated on read:** checksum over the canonically serialised payload, schema version,
  `worldHash`, and per-field range checks.
* **Saving goes through `StorageDevice`/`StorageContainer`** behind an `ISaveStore` interface, so
  the Web port swaps in `System::IO::IsolatedStorage` without touching a caller. `std::filesystem`
  is forbidden everywhere except `SaveStore`'s desktop implementation, and the lint enforces it.

**Versioning and migration:**

```cpp
struct Migration { int from, to; void (*apply)(JsonObject&); const char* note; };
```

* Version **lower** than current → apply migrations in order, log each, and write at the new
  version on the next save. The chain is data, and each step carries a note explaining the change.
* Version **higher** than current → refuse with a clear message and offer to start fresh.
  **Never guess.**
* **`worldHash` mismatch** → load anyway. Drop any interactable id that no longer exists, logging
  each; give any new id its canonical state; if the player's cell is gone, respawn at `L0_FOYER`.
  This is the *normal* case during development and must be graceful, not fatal.

**Reset House** is defined against the same canonical state: it is not "delete the save", it is
"return the named scope to `initialstate.json`", with two scopes (whole house, or the current
room) so that a player can undo a mess without losing everything else.

## Alternatives considered

**A full-state save.** Rejected. It would be ~10× larger, and every new interactable would silently
be *missing* from every existing save, requiring per-field defaulting logic that is exactly the
migration burden the delta removes.

**A binary save.** Rejected. At 90 KB, size is not a concern, and binary would cost the two
properties that matter most during development: diffability and inspectability. A binary format
also makes migration code harder to review, which is where save bugs live.

**Version-free "just be tolerant".** Rejected. Tolerance without a version number means the loader
can never tell "this field is new" from "this field is corrupt", and every fix becomes a guess.

**Automatic downgrade for newer saves.** Rejected — refusing is the only honest answer. A newer
version may have written state this build has no concept of; loading it would produce a plausible
wrong house.

**`std::filesystem` directly.** Rejected. It does not exist usefully on the Emscripten target, and
routing through `ISaveStore` is what keeps the Web port a swap rather than a rewrite.

## Consequences

* `initialstate.json` becomes load-bearing: it is the reference every delta is taken against and
  every reset returns to, and it must be validated as strictly as the layout.
* Migrations accumulate and are never deleted, so a save from any shipped version loads. Each has a
  test with a stored fixture save.
* Autosave is frequent (on quit, on entering the pause menu, every 5 real minutes, and on a cell
  change after 60 s) because it is cheap. A save that ever exceeds 8 ms fails a performance test.
* Corruption is a designed path, not an exception: checksum failure falls back to `save.bak`, and
  a failure of both is reported plainly with the option to start fresh.
