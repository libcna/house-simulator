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

| 2026-09-06 | HOUSE-00165 | Empty-scene frame time — clear + title screen + HUD, whole `Game` loop | Release / OPENGLES3 / Tier S+E, quality low, **debug tools ON** / 1600×900, **vsync off** | perf `EmptySceneFrameTests` | **0.229 ms** median | 16.67 ms | **1.4 % of budget** | 600 samples after 120 warm-up frames; p95 0.351 ms, p99 0.802 ms, worst 3.150 ms. Four repeats gave medians of 0.225, 0.225, 0.229 and 0.232 ms. Superseded as the shipping number by the row below, which is the same build with debug tools off — this row is kept because it is what a developer build costs. |
| 2026-09-06 | HOUSE-00165 | The same, **debug tools OFF** — the shipping configuration | Release / OPENGLES3 / Tier S+E, quality low / 1600×900, **vsync off** | perf `EmptySceneFrameTests` | **0.21 ms** median | 16.67 ms | **1.2 % of budget** | Three repeats: 0.183, 0.209, 0.228 ms. Quoted to two digits, because the spread between repeats (±0.02 ms) is larger than the third digit and quoting it would be inventing precision. |

**What that number is and is not.** It is the whole `Game` loop — `Update`, the render pass list,
one `SpriteBatch` of two strings, and the present — with vsync off, because with vsync on the
measurement would be the display's refresh rate and nothing about this program. It is **not** a
scene: there is no house yet. Its value is as a **floor**: 1.4 % of the frame is what the
application costs before anything is drawn, so every later phase's budget can be spent on the house
rather than on the harness. The p99 of 0.802 ms against a 0.229 ms median is a machine shared with
other build jobs, not a hitch in the program.

`worst` is recorded and deliberately not asserted on. On a machine running ten agents, the largest
of 600 samples measures the scheduler.

**The two rows differ by the debug tools, and the difference is smaller than the run-to-run spread**
— roughly 0.02 ms against a ±0.02 ms spread. So the honest reading is that the overlay's *measuring*
costs nothing detectable while it is hidden, not that it costs 0.02 ms. The rows are kept separate
anyway, because a later phase comparing against "the empty scene" needs to know which build the
number came from.

| 2026-09-06 | HOUSE-00029 | `std::vector<int>` of 8, built and discarded, vs `std::array` + count | Release / — / — / medians of 9 runs × 200 000 | perf `SmallContainerTests` | 15.5 ns vs **0.8 ns** | — | baseline | A 20× ratio, and it settles nothing on its own: 11 300 such containers per frame would be needed to reach 1 % of the budget. `util::SmallVector` **rejected** on this number. |
| 2026-09-06 | HOUSE-00029 | `std::string` construction, 10 chars (SSO) vs 57 chars (allocates) | Release / — / — / medians of 9 runs × 200 000 | perf `SmallContainerTests` | **0.4 ns** vs 12.5 ns | — | baseline | Short-string optimisation already removes the allocation `util::FixedString` would have removed. **Rejected.** |
| 2026-09-06 | HOUSE-00029 | 72-matrix bone palette: fresh `std::vector` per draw vs a reused buffer | Release / — / — / medians of 9 runs × 200 000 | perf `SmallContainerTests` | 149.1 ns vs **106.8 ns** | 8.15 µs (one draw) | 0.5 % of a draw | The one real per-draw allocation the measurement found. Fixed in `MaterialBinder` by reusing a member buffer — not because 42 ns mattered, but because removing it cost one line. |
| 2026-09-07 | HOUSE-00218 | `make content` — cold, no stamp file, current pipeline (5 validators run, 6 world stages skipped for want of a layout) | wall clock / — / — / this machine, shared with ~10 agents | — | **0.29 s** median of 5 (0.29–0.31) | — | baseline | 0.19 s of that is the five validators; the rest is Python start-up. The six world stages are *skipped*, not fast: `assets-src/world/` is empty until phase 5. |
| 2026-09-07 | HOUSE-00218 | `make content` — warm, nothing changed | wall clock / — / — / as above | — | **0.09 s** median of 5 (0.09–0.09) | — | baseline | Almost entirely interpreter start-up plus hashing 7 input files. The stamp check itself is not measurable at this size. |
| 2026-09-07 | HOUSE-00218 | `lightmap_bake.py` — one Cycles bake of the two-room fixture, CPU, denoised | wall clock / Cycles CPU / — / as above | `lightmap_bake` selftest fixture | 32²/16 spp **0.039 s** · 64²/16 **0.055 s** · 128²/16 **0.200 s** · 64²/64 **0.191 s** · 128²/64 **0.617 s** | — | baseline | Above ~64² the cost is close to linear in texels × samples; below it, fixed overhead dominates. Measured to make the extrapolation below defensible rather than guessed. |
| 2026-09-07 | HOUSE-00218 | **Estimated** full lightmap bake, §18.3's settings (2048², 256 spp, denoised) | wall clock / Cycles CPU / — / as above | — | **≈ 10 min per atlas; ≈ 7–8 h for §18.3's 42 atlases** | — | baseline | Extrapolated from the 128²/64 row by ×256 texels and ×4 samples. A **lower bound**: the fixture is two rooms of 16 faces and two lamps, and a furnished cell has far more geometry per ray. The number to plan against is *overnight*, not *coffee break* — which is why `HOUSE-00216`'s content-hash stamps are load-bearing rather than a convenience. |

## Budgets this log is measured against

Recorded here for convenience; `cna-house.md` §71–72 is authoritative.

| Budget | Target |
|---|---|
| Desktop frame time | 16.67 ms at 1920×1080 (60 FPS) |
| Save write | < 8 ms |
| Portal-path audio solve | ≈ 40 µs for 64 emitters |
| Cold-start load | < 4 s, or a background loader is implemented (`HOUSE-02451`) |
