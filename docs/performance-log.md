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

All rows below are **medians of 21 samples after 3 discarded warm-up rounds**, taken by the phase-1
probes (`tests/probes/phase1/`) rather than by the game, because phase 1 exists to size the budgets
before there is a game to measure. Rows marked *CPU submit* are the cost of building the command
stream; rows marked *to completion* end with a one-texel `GetData` that forces the GPU to finish.
The two are reported separately because a loop that only fills a command buffer says nothing about
a frame.

| date | task | what | config | scene | value | budget | verdict | notes |
|---|---|---|---|---|---|---|---|---|
| 2026-09-06 | HOUSE-00106 | `EffectPass::Apply()`, per call | Release / OPENGLES3 / — / 512² RT | probe `p1-perf` | **0.184 µs** | — | baseline | 0.184 ms for 1 000. Effectively free, and 44× cheaper than the draw that follows it. |
| 2026-09-06 | HOUSE-00106 | `DrawIndexedPrimitives`, CPU submit per call | Release / OPENGLES3 / — / 512² RT | probe `p1-perf` | **8.15 µs** | — | baseline | **The number that sets the draw budget.** 1 000 draws is 8.15 ms of CPU alone — half a 60 Hz frame before any game logic. A practical ceiling of 300–400 draws per frame follows from it. |
| 2026-09-06 | HOUSE-00106 | 1 000 × (`Apply` + `DrawIndexedPrimitives`), to completion | Release / OPENGLES3 / — / 512² RT | probe `p1-perf` | 13.44 ms | 16.67 ms | within | Only just, and with nothing else in the frame. |
| 2026-09-06 | HOUSE-00092 | 2 000 dynamic quads/frame, `SetDataOptions::Discard`, to completion | Release / OPENGLES3 / — / 512² RT | probe `p1-perf` | **0.171 ms** | — | baseline | CPU submit 0.055 ms. Dynamic geometry is nearly free; the particle budget is not the constraint anyone expected. |
| 2026-09-06 | HOUSE-00093 | 200 instances, one `DrawInstancedPrimitives`, to completion | Release / OPENGLES3 / — / 512² RT | probe `p1-perf` | **0.156 ms** | — | baseline | Against **2.144 ms** for the same 200 as separate draws — a **13.7×** saving. Becomes the vegetation and neighbourhood path. |
| 2026-09-06 | HOUSE-00107 | 4 MiB `Texture2D::SetData` (1024²), to completion | Release / OPENGLES3 / — / — | probe `p1-perf` | **9.30 ms** (430 MiB/s) | 16.67 ms | **over** | More than half a frame. The per-frame residency promotion must be **split**. |
| 2026-09-06 | HOUSE-00107 | 1 MiB `Texture2D::SetData` (512²), to completion | Release / OPENGLES3 / — / — | probe `p1-perf` | 2.47 ms (405 MiB/s) | — | baseline | Linear and bandwidth-bound, not a fixed per-call cost — so ≈1 MiB per frame is the promotion size, and a large texture spreads over four frames. |
| 2026-09-06 | HOUSE-00091 | `DrawIndexedPrimitives`, CPU submit per call | Release / **OPENGL33** / — / 512² RT | probe `p1-perf` | 12.23 µs | — | baseline | **1.5× more expensive than `OPENGLES3`** on the same driver and GPU, which reinforces ADR-0002's renderer choice. |
| 2026-09-06 | HOUSE-00091 | 200 instances vs 200 draws | Release / **OPENGL33** / — / 512² RT | probe `p1-perf` | 12.8× | — | baseline | Within noise of `OPENGLES3`'s 13.7×, so the instancing win is a property of the machine rather than of the renderer. |
| 2026-09-06 | HOUSE-00116 | Cold build of CNA + sharp-runtime + one probe, `CCACHE_DISABLE=1` | Release / OPENGLES3 / — / `-j6` | — | **330.3 s** wall | — | baseline | 1 802.7 s user CPU, peak RSS 892 MB, 724 targets. |
| 2026-09-06 | HOUSE-00116 | The same build from an empty directory, **shared ccache warm** | Release / OPENGLES3 / — / `-j6` | — | **14.9 s** wall | — | baseline | 20.1 s user CPU. A **22× wall-clock and 90× CPU** saving — the whole justification for openeggbert build rule 1, measured rather than argued. |
| 2026-09-06 | HOUSE-00116 | Edit one probe source, rebuild and relink | Release / OPENGLES3 / — / `-j6` | — | 0.94 s | — | baseline | The number that actually sets expectations for a working session. |

**Not yet measured:** the empty-scene frame time of the real application. `HOUSE-00165` fills that
row at the phase-2 review; the application currently clears and draws two HUD strings, which is not
a scene and would produce a number nobody should quote.

## Budgets this log is measured against

Recorded here for convenience; `cna-house.md` §71–72 is authoritative.

| Budget | Target |
|---|---|
| Desktop frame time | 16.67 ms at 1920×1080 (60 FPS) |
| Save write | < 8 ms |
| Portal-path audio solve | ≈ 40 µs for 64 emitters |
| Cold-start load | < 4 s, or a background loader is implemented (`HOUSE-02451`) |
