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
| 2026-09-07 | HOUSE-00218 | `make content` — cold, no stamp file, whole pipeline (5 validators + 5 `cna-content` roots run; 6 world stages and `cnb-world` skipped for want of a layout) | wall clock / — / — / this machine, shared with ~10 agents | — | **0.77 s** median of 5 (0.73–0.78) | — | baseline | 0.47 s of that is stage work — the fonts root is the largest single item at 0.12 s, being five `.spritefont` descriptors. The world stages are *skipped*, not fast: `assets-src/world/` is empty until phase 5, so this is a floor and not a full build. |
| 2026-09-07 | HOUSE-00218 | `make content` — warm, nothing changed | wall clock / — / — / as above | — | **0.09 s** median of 5 (0.09–0.09) | — | baseline | Almost entirely interpreter start-up plus hashing the inputs. The stamp check itself is not measurable at this size, and it is what keeps the number here instead of at 0.77 s. |
| 2026-09-07 | HOUSE-00218 | `lightmap_bake.py` — one Cycles bake of the two-room fixture, CPU, denoised | wall clock / Cycles CPU / — / as above | `lightmap_bake` selftest fixture | 32²/16 spp **0.039 s** · 64²/16 **0.055 s** · 128²/16 **0.200 s** · 64²/64 **0.191 s** · 128²/64 **0.617 s** | — | baseline | Above ~64² the cost is close to linear in texels × samples; below it, fixed overhead dominates. Measured to make the extrapolation below defensible rather than guessed. |
| 2026-09-07 | HOUSE-00218 | **Estimated** full lightmap bake, §18.3's settings (2048², 256 spp, denoised) | wall clock / Cycles CPU / — / as above | — | **≈ 10 min per atlas; ≈ 7–8 h for §18.3's 42 atlases** | — | baseline | Extrapolated from the 128²/64 row by ×256 texels and ×4 samples. A **lower bound**: the fixture is two rooms of 16 faces and two lamps, and a furnished cell has far more geometry per ray. The number to plan against is *overnight*, not *coffee break* — which is why `HOUSE-00216`'s content-hash stamps are load-bearing rather than a convenience. |

### Phase 9 — room/portal visibility (`HOUSE-00697`)

**The first two rows are COUNTS and not times**, which is why they carry a Debug config without the
usual warning: how many cells a pose sees and how many draws follow from them is a property of the
house and of §25's arithmetic, and an optimiser cannot change either. The time rows below them are
`HOUSE-00694`'s, taken in an optimised build; they are recorded here rather than re-measured
because re-measuring costs two full rebuilds of a tree shared with nine other agents for a number
whose inputs have not moved.

| date | task | what | config | scene | value | budget | verdict | notes |
|---|---|---|---|---|---|---|---|---|
| 2026-09-11 | HOUSE-01561 | `SunPositionAt` — §32.1's whole solar-position model, once | `-O2` / — / — / probe `p0-suncost` | 100 000 evaluations | **0.000129 ms** | called once a frame | **0.011 % of budget** | §32.1 says *"it is ~40 flops"* and *"evaluated once per frame"*; this is what that costs. Eight transcendental calls dominate it, not the arithmetic. Nothing about the sun's POSITION needs caching. |
| 2026-09-11 | HOUSE-01544 | `DaylightMinutesAt` — the seasonal day length, **before** the memo | `-O2` / — / — / probe `p0-suncost` | 1 000 calls | **0.1446 ms** | 1.20 ms worst-case frame | **12 % of budget** — refused | Two `SunDayFor` calls, each a 4-minute scan of a day plus two bisections: ~960 evaluations for a number that moves by 0.05 minutes a frame. Recorded because it is what the obvious implementation costs, and it is why the next row exists. |
| 2026-09-11 | HOUSE-01544 | The same, **after** the memo, at the real access pattern | `-O2` / — / — / probe `p0-suncost` | 60 000 calls, 60 a calendar day, walking forward | **0.001253 ms** | 1.20 ms worst-case frame | **0.10 % of budget** | Amortised, and the amortisation is honest: the figure includes the one miss per sixty calls. **115× faster** than the row above. The memo is keyed on every input, so a hit is by construction the value a miss would have computed. |
| 2026-09-11 | HOUSE-01544 | The same — the WORST single call, a calendar day rolling over | `-O2` / — / — / probe `p0-suncost` | 2 000 consecutive days | **0.0742 ms** | 1.20 ms worst-case frame | **6 % of budget**, on one frame in sixty | Halved from 0.1446 by reusing the previous day's *tomorrow* as the new *today*: time normally moves forward, and the memo already held the answer. A spike is felt where an average is not, which is why it is a row of its own. |
| 2026-09-11 | HOUSE-01544 | The same — a COLD miss, which is what `time set` costs | `-O2` / — / — / probe `p0-suncost` | 2 000 calls 37 days apart | **0.1510 ms** | not a per-frame path | **n/a** | A seek cannot reuse anything and pays both `SunDayFor` calls. It happens when a console command or a load moves the clock, never in a steady frame. |
| 2026-09-10 | HOUSE-00697 | Visible cells from §70.4's 12 budget poses, **every door in the house open** | Debug / — / — / a count, not a time | unit `VisibilityBudgetTests` | **worst 13** (`l0-sunroom`), mean 6.8 | 9 typical · 22 worst · 30 hard fail | **within** | The twelve see 82 cells between them with the doors open and 54 with them shut, out of §16's 96. §25's `cellsDropped` is zero at every one of them, so nothing was thrown away to reach the number. |
| 2026-09-10 | HOUSE-00697 | Draw calls and state changes that follow from the same twelve poses | Debug / — / — / a count, not a time | unit `VisibilityBudgetTests` | **69 draws**, 16 state changes | 620 · 90 | **11 % of budget** | 69 of the house's **469** chunks, which is phase 9 in one number. **16 of the 69 are §25.6's outdoors** (`HOUSE-00700`), which reached 10 of the 12 poses for 120 chunks in all; before that path existed the same twelve measured 53 draws and 7 state changes and were measuring half the frame. |
| 2026-09-09 | HOUSE-00694 | §25.1's steps 1–4 over §70.6's ten scenarios — the MEDIAN scenario | Release / OPENGLES3 / — / — | perf `VisibilityCostTests` | **0.084–0.097 ms** | 0.55 ms typical | **15–18 % of budget** | Median of 25 blocks, four runs on a machine with nine other agents on it; the range is the four runs. Step 5, the sort, is 0.0003 ms and is reported separately rather than added. |
| 2026-09-09 | HOUSE-00694 | The same, the **worst** scenario | Release / OPENGLES3 / — / — | perf `VisibilityCostTests` | **0.26–0.43 ms** | 1.20 ms worst case | **22–36 % of budget** | The portal walk is 0.0029 ms (`L3_STORE_W`, one crossing) to 0.0195 ms (`L0_KITCHEN`, thirteen) of it; §25.6's exterior hierarchy is **88–96 %** of every scenario that can see outdoors. The exterior cost tracks the number of CONES into the outdoors, not the geometry: the kitchen reaches it through seven and costs three times the road, which sees a quarter of the house's worth more geometry through one. |
| 2026-09-09 | HOUSE-00694 | The 90-second walk — worst FRAME | Release / OPENGLES3 / — / — | perf `VisibilityCostTests` | **0.43–0.68 ms** | 1.20 ms worst case | **36–57 % of budget** | Median 0.023–0.037 ms, p95 0.15–0.25 ms. A distribution rather than a pose: §25's answer is recomputed from the camera cell every frame, so a route costs what the poses along it cost. |

**What the time rows do not include.** They predate `HOUSE-00700`, which added §25.6's real
instances to the frame: a `GatherExteriorCones` pass over the visible set, one walk of a
49-instance hierarchy, and a `std::set_difference` against the walk's chunks. Measured as counts on
the twelve budget poses that is at most 120 chunks over twelve frames — three orders of magnitude
under the 4 100 synthetic instances the rows above are dominated by — so the rows bound the new
cost rather than describe it, and the next optimised measurement should retake them.

**The exterior instance set in those rows is synthetic** — 4 100 instances in §25.6's categories
over §10.3's extents — because the real vegetation (`HOUSE-00772`) and neighbourhood
(`HOUSE-00852`) are Phase 10 content. The 49 real ones are the ground, the road, the fences and
the garden structures. Retake these numbers when the content lands.

### Phase 24 — moon and stars

| date | task | what | config | scene | value | budget | verdict | notes |
|---|---|---|---|---|---|---|---|---|
| 2026-09-13 | HOUSE-01604 | 64 KiB `Texture2D::SetData` for the 128² RGBA moon mask, to completion | Debug / OPENGLES3 / Tier S+E / offscreen | perf `MoonMaskUploadTests` | **0.284 ms median** | 16.67 ms | **1.7 % of budget — negligible** | 3 warm-up uploads discarded, then the median of 21 samples. Each timed sample includes a one-texel render-target readback after `SetData`, forcing completion rather than measuring submission alone. Debug is stated because the active shared build is Debug; this path is driver/upload-bound and the number is used only to establish that the infrequent mask update is not a frame-budget concern. |

## Budgets this log is measured against

Recorded here for convenience; `cna-house.md` §71–72 is authoritative.

| Budget | Target |
|---|---|
| Desktop frame time | 16.67 ms at 1920×1080 (60 FPS) |
| Save write | < 8 ms |
| Portal-path audio solve | ≈ 40 µs for 64 emitters |
| Cold-start load | < 4 s, or a background loader is implemented (`HOUSE-02451`) |

### 2026-09-26 — furnished-house Release baseline (`HOUSE-02403`)

Current `build-probe/` Release build, 1920×1080, OPENGLES3, Tier S / High, vsync off,
fixed time/weather/seed, on the reference Radeon 780M with Wayland/radeonsi (not Mesa's
offscreen software path). Each scenario ran in a fresh process with 120 warm-up and 600
measured frames. CPU is average update plus median render submission; GPU is median completion
after a one-texel render-target readback, with its p95 shown. Draws and triangles are
per-frame averages. CPU and GPU are different clocks and must not be added.

| Scene | Draw calls | Triangles | CPU ms | GPU median / p95 ms |
|---|---:|---:|---:|---:|
| Kitchen | 15 | 29,679 | 1.006 | 2.276 / 3.463 |
| Library | 16 | 25,574 | 0.988 | 2.215 / 3.500 |
| Main stair looking up | 82 | 336,055 | 2.154 | 4.557 / 6.471 |
| Street approach | 71 | 412,957 | 1.145 | 2.404 / 4.814 |
| Rear garden | 12 | 11,847 | 1.282 | 2.459 / 4.094 |
| Upper window | 20 | 23,429 | 1.482 | 2.810 / 4.471 |
| Heavy rain outside | 83 | 380,032 | 2.150 | 3.686 / 5.924 |
| Night outside | 30 | 361,931 | 2.101 | 3.471 / 6.491 |

All eight pass the High hard limits (CPU 9.5 ms, GPU 14 ms, 1,800 draws,
3.4 million triangles). These are fixed-camera performance results, **not** proof
that the main stair is visually acceptable or walkable with live controls.

The current compiled content has 332 textures accounting for 117.334 MB of
base-level RGBA8 texels, including 216 lightmaps accounting for 14.156 MB.
Charging a conservative full 4/3 mip chain gives at most 156.445 MB total
textures and 18.874 MB lightmaps, below the respective 300 MB and 60 MB
budgets; no texture or lightmap downscaling is warranted. The report does not
enumerate font atlases, so these are content-texture estimates, not an exact
GPU allocation ledger. The separate compiled **pack-size** report still warns
about `core` (66.6 MB versus 55 MB) and `audio-core` (30.05 MB versus 30 MB);
those packaging budgets are not the texture-memory criterion here.

Three fresh-process starts of the normal walk scene to the first captured frame,
including screenshot readback/PNG encoding, took 1.38, 1.38 and 1.27 s wall
(median 1.38 s) with the filesystem cache warm. This is an upper bound for
load-to-first-frame under that cache condition, not a disk-cache-cold result.
The game reported all 1,391 chunks across 96 cells resident and 93.613 MB of
geometry uploaded. With `--no-cull` to draw the resident world and exercise its
materials, the live process had 693,580 KiB RSS (710.2 MB) and one amdgpu DRM
client with 353,696 KiB GTT plus 6,116 KiB VRAM (368.4 MB total); repeated
fdinfo entries for that client were counted once. The worst peak RSS of the
eight isolated scenarios was 743,828 KiB (761.7 MB). These are measured live
allocations, not a claim that every packaged but unused texture was uploaded;
the conservative texture/mip sum above bounds that remainder. Both are below
the 550 MB GPU and 1.6 GB RSS gates, so no residency system is justified.

`HOUSE-02404` decision: no High scenario misses a hard target, so no
optimization technique is selected and the M10 performance-technique risk
reserve is retired. The eight rows above are the task's before-and-after
evidence: no render code or content changed between them and this decision.
