# ADR-0009 — A 24-minute simulated day

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-06 |
| **Task** | `HOUSE-00015` |
| **Owns** | `cna-house.md` §35 |

## Context

The brief asked for an accelerated day/night cycle and suggested 20 real minutes per simulated
day. The choice sets `timeScale`, and through it the sun's angular rate, the pace of the weather
system, the rate at which the moon's phase advances, and — less obviously — the arithmetic that
every log line, test and settings dialogue in the project has to do.

## The analysis

| Real min / sim day | `timeScale` | 1 real second = | Sun motion | Verdict |
|---|---|---|---|---|
| 20 | 72× | 1.2 sim minutes | 0.30 °/s | The original suggestion. Fine, but 1.2 minutes per second is awkward to reason about, and a 15-minute in-game hour boundary lands at 12.5 real seconds. |
| **24** | **60×** | **exactly 1 sim minute** | **0.25 °/s** | **Chosen.** |
| 48 | 30× | 30 sim seconds | 0.125 °/s | A good "slow" preset for screenshots and atmosphere. |
| 96 | 15× | 15 sim seconds | 0.06 °/s | "Very slow" preset. |
| 1440 | 1× | 1 sim second | real time | Debug/realtime preset. |
| ∞ | 0× | frozen | none | Debug preset, **essential** for pixel-regression tests. |

## Decision

**24 real minutes per simulated day (`timeScale = 60.0`) is the default**, with 20, 48, 96, real
time, frozen and a free numeric entry offered in the settings. The brief's original 20 is retained
as a preset, so nothing is taken away.

The reason 24 wins **is not aesthetic**. At `timeScale = 60`:

* 1 real second = 1 simulated minute;
* 1 real minute = 1 simulated hour.

That removes an entire class of arithmetic mistakes from every test, log line and settings
dialogue in the project. A test that says "advance 3 seconds and assert the clock advanced 3
minutes" needs no conversion, and therefore cannot get the conversion wrong. A log line reading
`14:05` after 845 s of play is checkable in one's head.

The aesthetics are checked and acceptable: at 0.25 °/s the sun crosses its own diameter (about
0.53°) in 2.1 real seconds — smooth and perceptible without being distracting.

## Alternatives considered

**20 minutes (`timeScale = 72`), as the brief suggested.** Rejected as the *default*, retained as a
preset. The 20 % faster sky is not better or worse; 1.2 sim-minutes per real second is simply worse
to reason about, and the project will spend far more hours reading clocks in logs than watching the
sun.

**Real time (1×).** Rejected as a default — a player would never see night — but essential as a
debug preset, and offered.

**Frozen (0×).** Not a candidate as a default, and *required* as a preset: the render-regression
suite needs a fixed clock, fixed weather and fixed RNG to compare against stored PNGs.

**A day length that varies with season or weather.** Rejected as gratuitous: the *simulated* day
already varies in daylight hours through the astronomical model. Varying the real-time mapping as
well would make the clock unpredictable for no gain.

## Consequences

* `timeScale` is persisted in the save's `clock` block, so a player who chose 48 minutes keeps it
  across sessions, and a loaded save never silently re-times the world.
* Every automated test that touches time sets `timeScale` explicitly — usually to 0 for render
  tests and to 60 for simulation tests — rather than relying on the default.
* The moon's phase advance is expressed in *simulated* days and has its own 1×–8× speed setting, so
  choosing a longer day does not make the lunation invisible.
* Dusk-triggered exterior lights use per-fixture random offsets of ±8 simulated minutes, which at
  `timeScale = 60` is ±8 real seconds — visible as staggered switching rather than a single global
  event, at any of the offered day lengths.
