# Normal-game audio wiring — 2026-09-28

**Current acceptance:** `HOUSE-01920` and `HOUSE-01922` are complete, with the
owner's actual six-category and interior/exterior day/night listening below.
`HOUSE-01925` is complete with actual owner acceptance of the corrected indoor
rain and final automated gates green. `HOUSE-01939` whole-zone weather listening remains open. Earlier blocked
notes are historical, not missing-device claims.

## Retained weather loops and the owner's indoor-rain correction

Four existing NOX rain/wind banks are now in the ordinary Start/walk path, using
eight retained standard-XNA loop voices: original and in-memory 900 Hz low-pass
per bank. No new recording, derivative asset set, per-window/per-roof layer,
internal CNA API or general DSP subsystem. Master/mute and live Weather category
volume still apply once. Unload releases instances before their filtered sounds.
CSKY v2 is bound to the loaded chunk hash and all 96 cell IDs; existing coverage
and collision terrain supply shelter distances. Invalid data reports weather
unavailable without disabling the accepted footsteps/ambience.

The first full implementation passed 1536 unit / 171 GPU integration tests
(3 opt-in skips), but **failed actual listening**. The owner heard rain outside,
almost none inside or on the attic. Do not treat those old green tests as
audibility acceptance. Strong-rain source RMS is -32.050566 dBFS and its raw
900 Hz filtered RMS is -35.741752: an unintended additional **3.69 dB** loss.
The old far-roof/sky-only gain also silenced windowless above-ground rooms.

The correction separates muffling from shelter gain. Filtered loop RMS matches
its original loop, bounded to 8× gain and 90% PCM headroom; actual gains are
1.17409 / 1.52830 / 1.02263 / 1.22909 for calm/strong rain and calm/forest wind.
Independent stereo filter state, loop warm-up and the true frequency-response
test remain. Above-ground rooms retain a small transmitted rain bed; roof
proximity raises it strongly in the attic. Underground depth uses the listener
cell's **authored floor**, not eye height or hard-coded room names. Using eye
height in an intermediate draft leaked rain into B1_CINEMA: its focused GPU
test failed (0.106600806 against ≤0.001) and PCM was -59.667955 dBFS. That draft
is rejected; the corrected floor-based capture is completely silent.

The owner subsequently confirms **“nyni je vse ok”** in response to the explicit
Heavy-rain check: strong attic rain, audible muffled rain in ordinary above-ground
rooms, silent basement and sunroom stronger than hall. This is subjective
acceptance of those listening cases, **not** a four-state walk of every zone.

### Isolated actual-backend measurements

Private profile, master 0.8 / Weather 1.0 / ambience and footsteps 0, fixed noon,
seed 6840335469064670721, stationary normal controller, 900 actual hardware-GPU
frames per run. No visible monitor window. Own child stream routed to an owned
temporary PipeWire sink and recorded at 48 kHz signed-16 stereo. Owner settings,
speaker/default sink and unrelated streams are not changed; modules are removed.
Whole-recording RMS includes startup, so these are matched-level evidence, not
calibrated perceived loudness or a performance benchmark.

| Listening position | Before RMS dBFS | Corrected RMS dBFS | Evidence |
|---|---:|---:|---|
| L3_STORE_W | -50.559543 | -40.990206 | `/tmp/house-01925-{pcm,after-pcm}-attic.{log,pcm}` |
| L0_SUNROOM | -46.200800 | -41.342621 | `/tmp/house-01925-{pcm,after-pcm}-sunroom.{log,pcm}` |
| L0_HALL | not captured | -47.087331 | `/tmp/house-01925-after-pcm-hall.{log,pcm}` |
| B1_CINEMA | -inf (silence) | -inf (silence) | `/tmp/house-01925-final-pcm-cinema.{log,pcm}` |
| Outside, heavy rain | -38.978439 | -38.843998 | `/tmp/house-01925-{pcm,after-pcm}-outdoor-rain.{log,pcm}` |

Attic rain rises about **9.57 dB**, sunroom **4.86 dB**; sunroom is about
**5.74 dB** above hall. Outside is unchanged apart from run-duration/startup
variation; basement remains silent. Above-ground captures precede the final
floor-based basement correction; their above-ground gains are unchanged by it.
Final B1_CINEMA capture independently verifies the correction. Screenshots:
`build/test-output/audio-review/house-01925-after-{attic,sunroom,hall,outdoor-rain}.png`
and `house-01925-final-cinema.png`. They identify real GPU/listener positions;
static pictures do not prove audible sound or renewed visual acceptance.

Clear-weather outside wind also produces real PCM (-75.350189 dBFS RMS at that
calm state). An additional `--weather=W_WINDY` probe was invalid: W_WINDY is a
modifier, not a standalone authored state. Its zero output is rejected probe
evidence, not a silent-wind defect or permission to expand the weather model.

Current validation: **1537/1537 unit PASS**, 172 suites, 202353 ms;
**171 hardware-GPU integration PASS / 3 explicit opt-in SKIP / 0 FAIL**, 174 total,
545592 ms; **25/25 focused unit PASS**, including seven new weather tests and
existing mixer/ambience tests. The current actual-GPU four-room regression passes.
Only the full filming, threshold-frame capture and whole-house moving-lighting
reviews are skipped; no audio test is skipped. Evidence:
`/tmp/house-01925-floor-focused-unit.log`,
`/tmp/house-01925-floor-unit.log`,
`/tmp/house-01925-floor-integration.log`,
`/tmp/house-01925-final-pcm-cinema.log`. An earlier contended full unit run passed
1536 and failed the unchanged SkySystem 5 ms wall-time assertion at 9.182943 ms;
the final full suite passes without changing the assertion or sky code.
Final `tools/ci/run_checks.sh`: **all gates green**, including **356 strict-XNA
translation units / 109 existing destructor exemptions**. Log:
`/tmp/house-01925-audibility-static.log`. Builds reuse `build/` and the shared
ccache; two compile workers pinned to CPUs 0–5, strict four workers pinned to 0–3.
No sanitizer, soak, fresh performance benchmark or Web/Android build was selected
for this audio change; those platform/release tasks remain open, not unavailable.

## Current normal Start path, not only the CLI scene

`AudioGateTests.NormalTitleAndStartOpenAudioAndPlayTheProductionWalk` now
exercises the normal application with no `--scene`: audio is Waiting before
the title gesture, Ready in the main menu, then Start loads the production
world and four ambience voices. Standing emits no footsteps; ordinary controller
travel then plays them. The Radeon offscreen/surfaceless test passes, and a
separate real-PipeWire run through the same title/Start/input path passes with
zero bank problems and nonzero PCM: peak **-18.788821 dBFS**, whole-recording
RMS **-46.884028 dBFS** (including startup and the standing interval).
Logs/capture: `/tmp/house-01920-normal-start-{gpu,routed}.log` and
`/tmp/house-01920-normal-start-routed.pcm`. Only the child PID's own stream
was moved to its temporary sink; that sink was unloaded afterwards.

Read-only inspection found the owner's current saved master 0.80, footsteps
0.85, ambience **0.10500002** and weather 0.75. The speaker sink was unmuted
at 57% (-14.51 dB). This can explain very quiet ambience, not missing footsteps;
it does not prove what the owner hears. No preferences, defaults or unrelated
streams were changed. Digital output and successful voice state do not substitute
for subjective acceptance.

The owner subsequently confirmed **“Kroky slyším”** after launching the current
`./build/cna-house` through Start. This is real normal-play listening evidence,
and resolves the report that ordinary gameplay has no audible footsteps. It is
not by itself claimed as six separate surface-category checks or an ambience listening walk.

## Current owner listening and the silent garden contact

The owner then verified audible wood, carpet, tile, stone/concrete and grass
locations, but reported the garden bed was silent. The owner separately confirmed
the interior/exterior day/night ambience and doorway cross-fades with
**“Ověřeno, ambience i přechody jsou v pořádku”**.
This supplies the previously missing ambience listening evidence; it does not
close the still-failing sixth footstep category by implication.

The actual GPU/controller reproduction confirmed the raised vegetable bed's
contact was **`structure`**, not its cell's `soil`, and played **zero** footsteps.
The bank table itself loaded without problems, so the old bank-only check missed
it. The physical terrain palette also contained unmapped `mulch` and `lawn_worn`.
Before evidence: `/tmp/house-01920-garden-before.log`, two failing focused tests.
The retained gravel source is nonzero (-5.217772 dBFS peak / -34.270745 dBFS RMS);
this was classification, not a silent recording or a global volume problem.

All six raised beds now author `footstepSurface: soil`, consumed by the existing
collision writer without changing geometry, height, ownership or traversal.
`mulch` reuses the existing gravel/soil bank and `lawn_worn` the grass bank.
The six banks and their samples/gains are unchanged. The existing map now checks
24 spellings including the eight physical terrain materials and declared
structure contacts; unknown contacts are rejected. The schema/prose are updated.

Fresh canonical content and deployed collision/audio JSON were compared. Five
repetitions of the two physical audio regressions and the corrected threshold
case pass (15/15): the real controller stands/walks on `soil` and emits a oneshot;
all physical terrain materials select a bank. Evidence:
`/tmp/house-current-garden-threshold-focused.log`; all six authored contacts are
also checked by `/tmp/house-01920-garden-collision-final-selftest.log`.
After restarting the current `./build/cna-house`, the owner confirms
**“Ano, v záhonu jsou nyní kroky slyšet”**. All six category listening checks
are now satisfied; the five other confirmed categories were not changed or
needlessly re-reviewed. The ambience confirmation above independently satisfies
its indoor/outdoor listening requirement.

Current complete validation: **1530/1530 unit PASS** and **170 GPU integration
PASS / 3 explicit opt-in SKIP / 0 FAIL**, 173 total, 325181 ms. Logs:
`/tmp/house-current-final-{units,integration}.log`. The skipped full filming,
threshold-capture and moving whole-house visual reviews are separate opt-in
fixtures, not missing audio tests or a substitute for the owner's listening.

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
interior tone is deliberately quiet; these measurements alone **do not claim human
audibility, subjective balance or a completed listening walk**. The later owner
confirmation above supplies the ambience listening criterion; the garden's
post-fix listening is now confirmed too. Weather loops are not implemented
by this checkpoint; they remain HOUSE-01925, not permission for optional audio.

Evidence: `/tmp/house-01920-routed-audio.{log,pcm}` and
`/tmp/house-01922-{indoor-day,outdoor-day,outdoor-night}.{log,pcm,png}`.
All private routes report established=true. Captures use the current Debug
Radeon/OPENGLES3 application, not software rendering or dummy audio.

## Earlier checkpoint checks and rejected drafts

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
