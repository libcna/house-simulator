# Architecture decision records

One file per decision, named `ADR-NNNN-slug.md`, numbered permanently and never renumbered. A
record is written when a decision constrains later work; it states the context, the decision, the
alternatives that were rejected **and why**, and the consequences we accept.

A record is never edited to say something different. If a decision changes, a new record supersedes
it and both are updated to point at each other.

| # | Decision | Status | Task |
|---|---|---|---|
| [0001](ADR-0001-xna-only.md) | The XNA-only rule, and the three-tier A/P/C policy | Accepted | `HOUSE-00007` |
| [0002](ADR-0002-renderer-selection.md) | Renderer selection: `OPENGLES3` (EasyGL) | Accepted | `HOUSE-00008` |
| [0003](ADR-0003-render-tiers.md) | Two rendering tiers, and why Tier S must be complete alone | Accepted | `HOUSE-00009` |
| [0004](ADR-0004-portal-visibility.md) | Portal visibility with frustum reduction | Accepted | `HOUSE-00010` |
| [0005](ADR-0005-data-driven-world.md) | A data-driven world: JSON schemas and a closed expression vocabulary | Accepted | `HOUSE-00011` |
| [0006](ADR-0006-composition-over-ecs.md) | Composition over ECS | Accepted | `HOUSE-00012` |
| [0007](ADR-0007-kinematic-collision.md) | Project-owned kinematic collision, not a physics engine | Accepted | `HOUSE-00013` |
| [0008](ADR-0008-save-format.md) | Delta save format, versioning and migration policy | Accepted; superseded in part by 0014 | `HOUSE-00014` |
| [0009](ADR-0009-day-length.md) | A 24-minute simulated day | Accepted | `HOUSE-00015` |
| [0010](ADR-0010-room-aware-audio.md) | Room-aware audio over the portal graph, not `Apply3D` | Accepted; superseded in part by 0014 and 0015 | `HOUSE-00016` |
| [0011](ADR-0011-no-runtime-dependencies.md) | No third-party runtime dependencies | Accepted | `HOUSE-00017` |
| [0012](ADR-0012-asset-licensing.md) | Asset licensing policy, and the "no row, no build" rule | Accepted | `HOUSE-00018` |
| [0013](ADR-0013-neighbourhood-asset-variants.md) | The neighbourhood's variety is generated asset variants, not per-instance material overrides | Accepted | `HOUSE-00843` |
| [0014](ADR-0014-showcase-scope.md) | An architectural showcase, not a life simulator: scope reduction, breadth-first completion | Accepted; supersedes 0008 and 0010 in part; superseded in part by 0015 | `HOUSE-03201` |
| [0015](ADR-0015-quality-tiers-and-compact-scope.md) | Quality tiers, a reusable kit and a compact feature set: second scope reduction | Accepted; supersedes 0014 in part | `HOUSE-03205` |
