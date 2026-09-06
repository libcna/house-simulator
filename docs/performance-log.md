# Performance log

Every performance number this project acts on is recorded here, with the configuration that
produced it. The rule behind the file is simple: **no optimisation without a measurement, and no
performance claim without a row.** A task that says "this is faster" and has no row here is not
finished.

The log is append-only. A superseded measurement keeps its row and gains a note; deleting it would
destroy the trend, which is usually the interesting part.

## Reference hardware

| | |
|---|---|
| CPU | 16 threads, x86_64 |
| RAM | 30 GB, no swap, shared with other build jobs |
| GPU | AMD Radeon 780M (radeonsi, phoenix), Mesa 25.0.7, GL 4.6 core / GLES 3.2, 1 024 MB reported video memory |
| OS | Debian 13, kernel 6.12 |
| Compiler | g++ 14.2, C++23 |

A measurement taken on other hardware is still welcome; it records the hardware in the `notes`
column and is never compared directly against a reference-hardware row.

## Row format

| Field | Meaning |
|---|---|
| `date` | ISO date of the measurement |
| `task` | the `HOUSE-` id that produced it |
| `what` | the quantity, precisely enough that someone else could measure the same thing |
| `config` | build type, renderer, tier, resolution — e.g. `Release / OPENGLES3 / Tier S / 1920x1080` |
| `scene` | the screenshot-scene id or the named camera path ([`screenshot-scenes.md`](screenshot-scenes.md)) |
| `value` | the number, with its unit |
| `budget` | the budget from `cna-house.md` §71–72, or `—` |
| `verdict` | `within`, `over`, or `baseline` for a first measurement with no budget yet |
| `notes` | anything a reader needs to interpret the number, including the hardware if not the reference |

Measurement discipline, so that two rows are comparable:

* Release build unless the row says otherwise. Debug timings are not comparable and are marked so.
* A fixed clock (`timeScale = 0`), a fixed weather archetype and a fixed RNG seed.
* Discard the first 120 frames; report the **median** and the **95th percentile** over the next
  600. A single number without a percentile is a mean, and means hide the frames that hurt.
* State whether the number is CPU frame time, GPU frame time or wall-clock.

## Measurements

| date | task | what | config | scene | value | budget | verdict | notes |
|---|---|---|---|---|---|---|---|---|
| | | | | | | | | *(no measurements yet — phase 1 fills the first rows)* |

## Budgets this log is measured against

Recorded here for convenience; `cna-house.md` §71–72 is authoritative.

| Budget | Target |
|---|---|
| Desktop frame time | 16.67 ms at 1920×1080 (60 FPS) |
| Save write | < 8 ms |
| Portal-path audio solve | ≈ 40 µs for 64 emitters |
| Cold-start load | < 4 s, or a background loader is implemented (`HOUSE-02451`) |
