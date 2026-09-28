# Normal-game audio wiring — 2026-09-28

The ordinary game previously resolved bank metadata but never played footsteps
or started ambience loops. Earlier implementations were only in local stashes.
The tracked application now measures ordinary controller travel, selects the six
existing surface banks and calls standard-XNA SoundEffect.Play. Stairs use the
authored riser height. Four retained standard-XNA loop instances provide one
interior tone, forest birds by day, and night/cicadas by night. Indoor/outdoor
weights cross-fade over 0.8 s; sun altitude blends day/night over -6°..+3°.
Master volume is applied once globally, category/bank gains once per voice.
Unload releases loop instances before cached SoundEffects. Web/Android still
require a real gesture; explicit desktop walk scenes can open the device.

## Actual backend output, distinct from subjective listening

The owner's PulseAudio-compatible **PipeWire 1.4.2** server is available at
`/run/user/1000/pulse/native`, with a real speaker sink. Missing `/dev/snd` in an
agent sandbox is not evidence that the application lacks working audio.

Temporary null-sink modules isolate captures. SDL selects the server's explicit
default playback device and **ignores PULSE_SINK for that choice**. The first
capture attempt therefore recorded an unrelated silent monitor: reject its
digital-zero result, including the old baseline claim. The corrected capture
matches the child application's PID, moves only its own sink-input to its own
private sink, records PCM and unloads only its own module. No default device or
owner stream is changed; graphics uses the offscreen/surfaceless GPU path.

| Actual application path | Captured PCM peak / RMS, dBFS |
|---|---|
| Ordinary controller walking B1_HALL, footsteps | -19.659 / -46.252 |
| Standing indoors, B1_HALL at noon | -51.224 / -67.416 |
| Standing outdoors at noon | -31.052 / -54.370 |
| Standing outdoors at 23:00 | -32.258 / -52.042 |

Each production ambience run starts exactly four retained loop voices. The
interior tone is deliberately quiet; these measurements **do not claim human
audibility, subjective balance or a completed listening walk**. Those acceptance
criteria remain open in HOUSE-01920/01922. Weather loops are not implemented by
this checkpoint; they remain HOUSE-01925, not permission for optional audio.

Evidence: `/tmp/house-01920-routed-audio.{log,pcm}` and
`/tmp/house-01922-{indoor-day,outdoor-day,outdoor-night}.{log,pcm,png}`.
All private routes report established=true. Captures use the current Debug
Radeon/OPENGLES3 application, not software rendering or dummy audio.

## Checks and rejected drafts

* Current full unit suite: **1523/1523 PASS**, 184518 ms.
* Current serial actual-GPU integration: **165 PASS / 3 explicit opt-in SKIP /
  0 FAIL**, 285964 ms. Automated suite audio uses SDL dummy, unlike PCM probes.
* Direct bank map: six categories, 22 aliases, all authored surfaces covered;
  metadata/schema/generator checks pass. No new production audio assets.
* First focused runtime draft: 8 PASS / 2 FAIL because copied deployed metadata
  was stale. Rebuilt the existing world pipeline (14 built / 2 fresh), deployed
  it, and reran the full current integration suite. Do not count the stale draft
  as accepted evidence.
* Initial compile caught a local shadowed variable; renamed it and rebuilt all
  application/unit/integration targets successfully using shared ccache, -j2.
* The full static script passes every completed content/identifier/format gate
  except the pre-existing owner-owned root `.claude` layout failure. Its final
  two-worker strict compile was terminated (exit 143, no compiler error); the
  strict check is rerun separately with four workers, within the six-core limit.
  That rerun passes **351 translation units / 109 destructor exemptions**:
  `/tmp/house-01922-strict.log`. The audio checkpoint remains open only for
  subjective listening, not an unexplained compiler or device failure.

Logs: `/tmp/house-01922-{current-build,units,integration,static}.log` and
`/tmp/house-01920-{build-fixed,world,focused-gpu}.log`. No sibling changes,
native runtime audio calls, monitor windows or performance improvement claim.
