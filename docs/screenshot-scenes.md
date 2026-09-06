# Screenshot scenes

A **screenshot scene** is a fully determined frame: a fixed camera pose, a fixed simulated clock, a
fixed weather state, a fixed RNG seed and a fixed set of world states. Given a scene id, any build
on any machine renders the same image. That is what makes the render-regression suite
(`cna-house.md` §70.4) meaningful, and it is why documentation, bug reports and the performance log
all refer to frames by scene id rather than by description.

## The id grammar

```
^[a-z][a-z0-9]*(-[a-z0-9]+)*-\d{2}$

<family>[-<qualifier>...]-<NN>
```

Lower case, hyphen-separated, ending in a two-digit ordinal. The ordinal is permanent: `cull-01`
means one specific frame for the life of the project, and a scene that is retired keeps its number
rather than letting a later scene inherit it.

| Family | Covers | Examples |
|---|---|---|
| `tod` | Time of day: 8 poses × 8 clock settings | `tod-0600-03`, `tod-1930-07` |
| `wx` | Weather: 6 poses × the archetype set | `wx-rain-heavy-02`, `wx-snow-04` |
| `room` | Per-room lighting, lights on and off, noon and midnight | `room-l0-kitchen-on-noon-01` |
| `door` | A door at 0.0 / 0.35 / 1.0 open | `door-l0-kitchen-035-02` |
| `cull` | Culling sanity: rendered normally and with culling disabled; the images must match | `cull-01` |
| `tier` | Tier S vs Tier E, same content, both rendered | `tier-l0-family-03` |
| `char` | Avatar customisations and poses; dog and cat states | `char-avatar-f3-walk-02`, `char-dog-lie-01` |
| `ext` | Exterior: road, drive, garden, terrace, neighbourhood at 3 LOD distances | `ext-drive-lod1-05` |
| `ui` | Prompt, held item, sun clock, menus | `ui-prompt-door-01` |

A qualifier is only added where it distinguishes scenes within a family, and it always reads
left-to-right from coarse to fine: family, place, state, ordinal.

## Files

```
tests/render/scenes/<id>.json         the scene definition — camera, clock, weather, seed, states
tests/render/reference/<id>.png       the accepted image
```

A failing comparison writes three artefacts, named so they sort together:

```
<id>.reference.png   <id>.actual.png   <id>.diff.png
```

## Referring to a scene

* In `plan.md`: `verify: ... screenshot cull-01`.
* In [`performance-log.md`](performance-log.md): the `scene` column.
* In a document or a bug report: the bare id. Never "the kitchen shot at night" — that is not a
  scene, and nobody can reproduce it.

## Adding a scene

1. Take the next free ordinal **within the family**; never reuse a retired one.
2. Write `tests/render/scenes/<id>.json`. Every field that affects the image is explicit: camera
   position and orientation, `timeScale = 0` with the exact simulated time, the weather archetype
   and its state vector, the RNG seed, the render tier, and any non-default door, window or light
   state.
3. Render, **look at the image**, and accept it only if it is right. A reference image is an
   assertion about correct output; accepting a wrong one poisons every later comparison.
4. Commit the scene and its reference together, in the commit that needed them.

## Updating a reference

A reference changes only when the change in output is **intended**. The commit that updates it says
why, and updates every scene the change affects in the same commit — never one at a time as tests
are noticed failing.
