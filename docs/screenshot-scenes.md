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
| `content` | The content pipeline itself: one of each asset type, loaded and shown | `content-smoke-01` |
| `fp` | First-person: twelve places a body can stand, seen through §44's camera at head height | `fp-l0-hall`, `fp-l3-room` |
| `sun-season` | Longest/shortest-day edges at matched local clock times | `sun-season-01`, `sun-season-04` |

A qualifier is only added where it distinguishes scenes within a family, and it always reads
left-to-right from coarse to fine: family, place, state, ordinal.

## Files

```
tests/render/scenes/<id>.json         the scene definition — camera, clock, weather, seed, states
tests/render/reference/<id>.png       the accepted image
```

`HOUSE-02511` owns moving the existing C++ scene definitions into the JSON harness above. Until
that task lands, the definition beside each render test is authoritative; the references already
use the final paths and ids. In particular, `sun-season-01..04` live in
`tests/render/SunSeasonRenderTests.cpp`. This is the same arrangement as the `fp`, `property` and
`blockout` families and is recorded here rather than pretending an unimplemented JSON reader ran.

A failing comparison writes three artefacts, named so they sort together:

```
<id>.reference.png   <id>.actual.png   <id>.diff.png
```

## The `sun-season` quartet

All four stand at `(0.00, 0.00, +5.20)` on `EXT_ROAD`, use the canonical world state and
`W_PARTLY` weather (`cloudCover 0.35`), seed `0x5EEDC0DEC0FFEE01`, Tier S/Low at 640×360, and
freeze the clock before the capture. The date/time pairs are exact positions the 24× compressed
calendar can reach, not arbitrary date/time combinations rounded by `SetStandard`.

| Scene | Comparison | Exact calendar position | Player yaw/pitch | Measured sun |
|---|---|---:|---:|---:|
| `sun-season-01` | longest-day morning, 06:00 DST | day 173: 2031-06-23 | 30° / +3° | +3.66°, azimuth 62.19° |
| `sun-season-02` | longest-day evening, 20:00 DST | day 187: 2031-07-07 | 330° / +3° | +4.70°, azimuth 295.64° |
| `sun-season-03` | shortest-day morning, 06:00 standard | day 366: 2032-01-02 | 30° / +3° | −15.43°, azimuth 107.22° |
| `sun-season-04` | shortest-day evening, 20:00 standard | day 356: 2031-12-23 | 330° / +3° | −37.29°, azimuth 269.28° |

The same road-and-house composition is intentional. The summer references visibly contain the
reddened sun disc; the matched winter references do not. Human inspection accepted all four under
software Mesa 25.0.7. Exact-pixel comparison finds 28 changed pixels in the morning pair and 26 in
the evening pair — the small disc itself, not a moved camera or changed house.

## The `fp` family is named for its PLACE, not numbered

`HOUSE-00633`'s twelve first-person frames are `fp-<level>-<room>` rather than `fp-NN`, the same
way `HOUSE-00483`'s blockout poses are: what makes one of them reproducible is
`--scene=walk --player=x,y,z,yaw,pitch`, which is written down in the test beside its name, and a
place reads better in a failure than an ordinal does. The ordinal rule above still governs every
family whose members are *states* of one place rather than different places.

**These are frames of the game and not of the model.** `--player` puts §49's capsule on the floor
with its feet at the given point, lets §49.3 settle it onto whatever it is standing on, and looks
through §44's camera — the eye 1.68 m over the soles, a 70° lens, §10.3's 0.10 m near plane. A
`--camera` pose can float through a wall to get a better angle; this one cannot, which is the
point: a ceiling 20 mm too low is invisible from outside and obvious from under it.

They are captured on the **first drawn frame**, like every other scene here, because the corner
line carries the frame time and only frame 1's is a fixed number on every machine. What the frame
loop does after that is asserted by `HeadlessRunTests`, which walks 400 frames and checks where the
body and the eye ended up.

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

## When a scene legitimately cannot be fully compared

`content-smoke-01` carries a **video panel**, and which decoded frame the player has reached depends
on wall-clock time. That rectangle is excluded from the comparison **by name**, which is
`ImageCompare.hpp`'s rule: a region that is genuinely not reproducible is named, never absorbed by
widening the per-channel tolerance until it passes — because a tolerance wide enough to cover a
changing video is wide enough to cover a missing object. What the excluded rectangle is still
checked for is that it is *not empty*, and that the video **advances** is asserted by
`ContentSmokeTests`, which watches two hundred frames instead of one.

## Updating a reference

A reference changes only when the change in output is **intended**. The commit that updates it says
why, and updates every scene the change affects in the same commit — never one at a time as tests
are noticed failing.

**That rule was broken once, and the cost is worth writing down** (`HOUSE-00201`, 2026-09-07).
`title-01.png` was captured by `HOUSE-00164` when the HUD font was `Fonts/Hud.spritefont`, which
named an *installed* DejaVu Sans. `HOUSE-00200` replaced it with the vendored Noto `Fonts/ui-16` —
an intended change, correctly made — and did not regenerate the reference. The fixture then
disagreed with reality by **1.17 % of the frame (16 491 pixels, max channel delta 248)** for two
commits, and nothing said so, because the render suite is nightly and no nightly ran in between.
The lesson is not "be careful": it is that **a change to a font, a shader or a clear colour is a
reference change**, and the commit that makes it has to run the render suite rather than assume the
nightly will.
