# ADR-0010 — Room-aware audio over the portal graph, not `Apply3D`

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-09-06 |
| **Task** | `HOUSE-00016` |
| **Depends on** | [ADR-0001](ADR-0001-xna-only.md), [ADR-0004](ADR-0004-portal-visibility.md) |
| **Owns** | `cna-house.md` §62, §64 |

## Context

XNA's spatial audio is `AudioListener`, `AudioEmitter` and `SoundEffectInstance::Apply3D`. As CNA
implements it, that is a simplified stereo pan plus distance attenuation plus Doppler: no cones, no
custom curves, no filters, no reverb, no HRTF (BL-11). The Doppler implementation additionally
carries a documented up-to-4× pitch risk when velocity units are wrong.

In a house that is not enough, and the failure is obvious to any listener. A television playing in
the family room, heard from the hall, should be quieter and duller than the same television across
an open room — and it should seem to come from **the doorway**, not through the wall. `Apply3D`
positions it through the wall, at full brightness, because free-space attenuation is all it knows.

The project already maintains an exact, dynamic room graph for visibility (ADR-0004), whose portal
apertures are the door and window states.

## Decision

**Compute room-aware gain, muffling and apparent position ourselves, over the portal graph, and
hand `Apply3D` only the result.**

Once per frame, for each active emitter (≤ 64), a Dijkstra search over the portal graph from the
**listener's cell**:

```
cost(cell → cell') through portal p =
      airLoss(distance through p)          // 6 dB per doubling, as free space
    + transmissionLoss(p)                  // per portal kind, lerped by aperture
    + floorPenalty(levelDelta)             // 6 dB per floor

transmissionLoss(p) = lerp(p.soundLoss.closed, p.soundLoss.open, smoothstep(0, 0.35, aperture))
```

The search terminates at the emitter's cell or at a 60 dB cost ceiling, beyond which the sound is
inaudible and is virtualised. It yields three outputs:

| Output | Use |
|---|---|
| `pathGain` | multiplies the emitter's `Volume` |
| `muffle` ∈ [0,1] | drives a bright/dull cross-fade between two instances |
| `apparentPosition` | **the centre of the first portal on the path**, when the emitter is not in the listener's cell |

`apparentPosition` is the highest-value part of the whole system. The television heard through the
open doorway is positioned *at the doorway*; walk past it and the sound sweeps across the stereo
field exactly as it should; close the door and it dims, dulls, and stays at the door.

**Doppler is off by default** (`SoundEffect::DopplerScale = 0`, zero listener and emitter
velocities), exposed as a setting, because of BL-11's pitch risk and because nothing in a house
moves fast enough to earn it.

**Muffling without a filter.** CNA has no low-pass filter on a voice, so the ~20 sounds that need
it are authored as a bright and a dull variant and cross-faded by `muffle`. Sounds that do not need
it do not pay for it.

## Cost

≤ 64 searches over ≤ 95 nodes with a small binary heap ≈ **40 µs**, and it is not recomputed every
frame: only when the listener changes cell, a door or window aperture changes by more than 0.05, or
an emitter changes cell. Otherwise the previous solution is reused and only the intra-cell distance
is updated.

## Alternatives considered

**Use `Apply3D` alone.** Rejected. It is the difference between "a house" and "sounds in a box",
and it is audible in the first minute of play.

**Ask CNA for a filter, a reverb or a cone.** Not available, and asking would mean modifying CNA
(forbidden from this repository) or reaching into a non-XNA API (Tier C, ADR-0001).

**Raytraced or geometric acoustics.** Rejected — explicitly a non-goal. It would cost far more than
40 µs, need a second geometric representation, and produce a result no more convincing than a
correct portal path in a rectilinear house.

**A third-party audio middleware (FMOD, Wwise).** Rejected — the brief requires XNA audio, and
ADR-0011 forbids a runtime dependency.

**Per-room reverb by swapping pre-reverbed samples.** Kept as a *limited* technique for room tone
and the handful of sounds where it matters, not as a general mechanism: 78 rooms × N sounds is a
content explosion, and the absorption/`reverbHint` fields in the cell data drive the cheap
approximation instead.

## Consequences

* Audio depends on the same authored graph as visibility, and on two additional authored fields per
  portal: `soundLoss.open` and `soundLoss.closed`. The world validator checks they exist and are in
  range.
* The voice budget (design target 32 concurrent `SoundEffectInstance`s, confirmed against the host
  by `HOUSE-00097`) is managed by a `VoiceManager` that virtualises anything beyond the ceiling —
  and the portal solver's 60 dB cutoff is what makes that cheap to decide.
* Every emitter needs a cell id, so emitter placement is part of the world data
  (`layout.audio.json`), not an ad-hoc position in code.
