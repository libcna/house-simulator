# ADR-0006 — Composition over ECS

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-06 |
| **Task** | `HOUSE-00012` |
| **Depends on** | [ADR-0001](ADR-0001-xna-only.md) |
| **Owns** | `cna-house.md` §17.1, §17.2, §17.3 |

## Context

An entity-component-system architecture pays for itself when there are many thousands of
homogeneous entities whose component sets change at runtime, and when iteration order over packed
component arrays is the dominant cost. It costs an archetype/storage layer, a query language, a
scheduling story, and an indirection between "the thing" and "its parts" that every debugging
session has to traverse.

## The entity-count argument

This is the whole world, counted:

| Kind | Representation | Count | Per-frame update cost |
|---|---|---|---|
| Cells | `WorldData` rows + `CellRuntime` | 95 | flags only |
| Portals | `WorldData` rows + `PortalRuntime` | 186 | aperture, when it changes |
| **Static renderables** | an index into the cell's pre-batched chunk list — **no per-frame object at all** | ~2 400 placements → ~380 chunks | **zero** |
| **Dynamic renderables** | `struct DynamicInstance { AssetId model; Matrix world; BoundingSphere bounds; MaterialOverrideId mat; uint16 cell; uint8 lodBias; uint8 flags; }` — 96 bytes, in a per-cell `std::vector` | ~900 | only when their state changes |
| **Interactables** | a data row referencing a dynamic instance (or a static sub-range) by id, dispatched to one of **12 behaviours** | ~640 | only the targeted one |
| **Characters** | `PlayerAvatar`, `Pet` — model + `ClipPlayer` + capsule + bone palette | **3** | every frame |
| Systems | one object each, fixed update order | 14 | every frame |

The largest homogeneous population that updates every frame is **three**. The largest population of
any kind is 2 400 static placements, which are batched offline into ~380 chunks and have no
runtime object at all. There is no iteration cost for an ECS to optimise, and no volatility of
component sets for it to manage.

## Decision

**Plain composition, with clear ownership and a fixed update order.**

```
CnaHouseGame : Microsoft::Xna::Framework::Game
 ├── Services      a small typed service locator, constructed in a fixed order
 ├── World         WorldData (immutable) + CellRuntime[] + PortalRuntime[] + SpatialIndex
 ├── Systems       14, fixed update order, each owns its own state, none owns another
 ├── Renderer      RenderList, EffectCache, the passes, DebugDraw
 └── Content       ContentRegistry, the caches, SaveStore
```

The rules that make this hold together:

* `Game` owns `Services`; `Services` owns every system by `std::unique_ptr`, constructed in
  dependency order and destroyed in reverse.
* Systems hold **non-owning references** to the systems they read, resolved once at construction.
  There are no cyclic dependencies; where two systems would need each other, an explicit **event
  queue** (`std::vector<Event>`, drained once per frame in a fixed order) breaks the cycle. The
  events are enumerated: `DoorStateChanged`, `LightSwitched`, `WeatherChanged`, `CellEntered`,
  `InteractionPerformed`, `FixtureUsed`, `SaveRequested`.
* GPU resources are owned by the caches and released in `UnloadContent()`.
* **No global mutable state.** The only globals are `constexpr` tables.
* **No raw owning pointers.** `T*` in a signature always means "borrowed, non-null, outlives the
  call", and is documented as such.

**640 interactables do not become 640 classes.** An `Interactable` is *not* a renderable: it is a
separate record referencing a dynamic instance or a static chunk sub-range by id. There are 12
behaviour types and 640 data rows. That separation — not an ECS — is what keeps the count from
becoming a class hierarchy.

## Alternatives considered

**EnTT (or any third-party ECS).** Rejected — `cna-house.md` §82.3. It would add a runtime
dependency (ADR-0011 forbids one), an Emscripten build integration, and machinery sized for a
problem this world does not have.

**A hand-rolled ECS.** Rejected for the same reason with worse ergonomics: we would write and test
an archetype store to manage a maximum of three per-frame entities.

**A deep inheritance hierarchy of game objects** (`Entity` → `Renderable` → `Interactable` →
`Door`). Rejected — this is the failure mode composition exists to avoid, and it forces every
interactable to carry a renderable's vtable and lifetime whether it needs one or not.

**A singleton per system.** Rejected. Singletons are global mutable state with a nicer name; they
destroy construction order, make the dependency graph invisible, and make tests share state. The
`Services` locator gives the same convenience with explicit ownership and explicit lifetime.

## Consequences

* Update order is a written, fixed list (`cna-house.md` §7.5) rather than an emergent property of a
  scheduler. It is reviewable and it is deterministic — which the save/replay tests depend on.
* Adding a system means adding it to `Services` in the right position and to the frame list. That
  friction is intentional: 14 systems is the design, not a starting point.
* If a future feature genuinely produced thousands of homogeneous per-frame entities, this decision
  would deserve revisiting. Nothing in the brief does, and the burden of evidence sits with that
  future feature.
