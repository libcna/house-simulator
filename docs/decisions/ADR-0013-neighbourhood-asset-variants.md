# ADR-0013 — The neighbourhood's variety is generated asset variants, not per-instance material overrides

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-10 |
| **Task** | `HOUSE-00843` |
| **Owns** | `cna-house.md` §11.4's palettes, `layout.exterior.json`'s `neighbourhood.asset` |

## Context

§11.4 puts sixty buildings round the plot and asks for eight material palettes, so that a street of
three repeated shapes does not read as a copy-pasted housing estate. `HOUSE-00841`'s grammar builds
the shapes; the palettes have to attach to individual houses somehow, and there are two ways to do
it.

`layout.exterior.json`'s `neighbourhood` rows already say what each instance IS: an `id`, an
`asset`, a position, a yaw, an LOD group and an impostor distance. Nothing in that record, or in
the runtime that reads it, can say "this instance, but painted differently".

## Decision

**A neighbourhood instance identifies its visual representation through `asset`, and the palette is
resolved offline by `neighbourhood_gen.py`.** The generated ids are semantic —
`MODEL_NB_HOUSE_A_CREAM`, `MODEL_NB_HOUSE_B_SAGE`, `MODEL_NB_HOUSE_C_RENDER_LOW` — and they name a
shape from the grammar, one of §11.4's eight palettes, and the LOD band.

Three rules go with it:

1. **Only the combinations the layout actually names are generated.** Three shapes × eight palettes
   × two LOD bands is forty-eight assets; this street needs **nineteen**, and nineteen is what the
   tool writes. The generator reads the layout and builds what it finds, so the set cannot drift
   from what is placed.
2. **The assignment is deterministic and neighbourly.** Houses are walked in street order — by
   depth band, then along x — and each takes the first palette not already used by a house within
   30 m across and 10 m deep. Nothing is random, two runs produce the same street, and no two
   houses you can see together are painted alike.
3. **The variants are generated, never duplicated by hand.** A variant is the same parametric
   grammar with a different palette, so a change to the grammar reaches all nineteen.

## Alternatives rejected

**Per-instance material overrides in the layout** — a `materials: {...}` block on a `neighbourhood`
row, remapped at load. Rejected: it is a runtime feature (a material-remapping path through
`CellRuntime` and the chunk builder, plus a schema for the override) built for one decorative
purpose, and §17.4 chunks by material, so an override would either break batching or have to be
resolved before batching — which is this decision, arrived at the long way round.

**Opaque numbering (`MODEL_NB_HOUSE_A1`…`A8`)** — rejected because an id is the durable name of a
thing (`HOUSE-00399`'s golden list makes that explicit), and `A7` tells a reader nothing that
`A_STONE` does not tell them better. The cost of the semantic name is that renaming a palette
renames assets; palettes are §11.4's and are not expected to churn.

**All forty-eight combinations, generated speculatively** — rejected as a Cartesian product nobody
asked for: twenty-nine files that no row names, in a build tree, shipped in a pack budget.

## Consequences

* `neighbourhood_gen.py` parses an asset id rather than being told a palette, and refuses one whose
  shape or palette it does not know — the same "no silent default" rule the rest of the toolchain
  follows.
* The palette of a given house is visible in the layout, which is where a person looks to see what
  is on the street, and diffs as a one-line change.
* Adding a house means adding a row; if its palette clashes with a neighbour's, the generator's own
  selftest says so by name.
* An LOD variant carries its palette (`..._CREAM_LOW`), so §26's bands and §11.4's variety are
  independent of each other.
