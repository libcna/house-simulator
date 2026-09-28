# HOUSE-03572 — the bounded whole-property filming walk

**Accepted after the complete fresh GPU walk and image review.** Current Debug game,
Radeon 780M / stock OPENGLES3, 2026-09-28. SDL offscreen/EGL surfaceless with
DISPLAY and WAYLAND_DISPLAY unset: no window on the owner's physical monitor.

## Behaviour and scope

C in ordinary Linux play joins a capsule-clear point in the current cell and
walks one forward circuit. C stops in place; Esc or Tab stops and opens Pause.
Ordinary controls return without a saved-pose teleport. Only the active tour
suppresses head bob, HUD and debug overlays. Environment settings remain active;
choose fixed time/weather for a consistent recording with the usual recorder.

The immutable optional `filmingTour.route` in initialstate.json contains 2422
points, including one eight-second panorama for each of the 90 accessible cells.
The existing world deployment hashes that file. Old worlds without the optional
object remain loadable. No additional camera, navigation engine, cutscene editor,
collision bypass, household persistence or gameplay interaction was introduced.

The route derives from an actual collision-resolved walk (10919 samples, reduced
with a 5 mm chord-error bound), then runs again through the ordinary 120 Hz
player controller. The policy emits movement and look intent only. Walking speed
is at most 0.70 m/s; turns are bounded to 45 degrees/s. Each cell rotates once
through 360 degrees. Street/front/rear grounds precede the main interior,
basement and garage, then L1, L2 and attic, then remaining ground-floor/service
and garden spaces. Starting elsewhere rotates the same circuit, rather than
walking an unsafe reversed prefix. In the normal application's hall spawn the
nearest route join first approaches the pantry; the whole street-to-attic
circuit is still included. A twelve-second lack of progress stops safely;
there is no skip-to-next-room or teleport fallback.

## Rejected evidence and camera regression

The first GPU panorama review was visibly wrong despite pure route coverage:
the camera looked at ceilings/sky. `ApplyLook` subtracts screen-Y, whereas the
new policy emitted the opposite pitch error. It diverged to the +85-degree
clamp. That run was intentionally interrupted after 24 views, not accepted as
a tour. Its 3445 original captures remain local under
`build/test-output/cinema-review-rejected-pitch`; log:
`/tmp/house-03572-rejected-pitch-gpu.log`.

The policy now emits `look.pitch - wantedPitch`. A before-failing regression
uses the real ApplyLook function from three starting pitches and converges to
the intended five-degree downward panorama. The fresh complete GPU review also
checks actual camera pitch on every rendered frame; old ceiling images are
not presented as current visual evidence. Earlier reversed-route and old-grid
attempts are likewise rejected/local, not counted as acceptance.

## Current validation

The complete unit suite passes **1516/1516**. Focused pitch/input/menu and
solid-join/no-progress regressions pass. The unchanged fast GrandTour and the
slow filming route both visit all 90 accessible cells. These pure
collision/controller results supplement, rather than replace, visual acceptance.

The completed GPU walk logs to `/tmp/house-03572-final-gpu.log` and captures
to `build/test-output/cinema-review`: **73534 frames, 294140 controller steps,
4903 original PNGs, 90 unique room panoramas**, elapsed 1731623 ms. It renders every 30 fps review frame with
the same four 120 Hz controller steps, retaining two original PNGs per simulated
second. Frame metadata records the input-sampling pose; the screenshot follows
that frame's update (at most one normal frame later), not an independently
fabricated viewpoint. Actual game-time normal play is unchanged; deterministic
review time is enabled only with an explicitly supplied synthetic input source.

All ten final contact pages covering every room panorama were visually reviewed.
Retained evidence: [90 four-angle panorama strips and source hashes](house-03572/index.json),
plus [movement sequences](house-03572/movement/) containing 209 full-resolution
lossless WebP captures across seven passages: cinema entry, basement descent,
both main stair ascents, BED3 entry, attic ascent and attic room/store entrances.
These sequences were also reviewed, including the final basement descent. The
cinema shows its seats/screen, ordinary rooms their furnishings, and the corrected
stairs have continuous readable flights, landings and exits. No new black room
or blocked route was identified. Moving captures are two per simulated second,
not a claim to exhaustively detect single-frame artefacts.

Early live previews mistakenly indexed PNG names extracted from mixed stdout/stderr:
23 names were lost through interleaved asset logs. Their later room/angle labels
were wrong and **are not acceptance evidence**. Final evidence uses all 4903
actual files ordered by capture timestamp, checked against the 4903-row TSV.
Do not infer a new dark-room defect from a mislabelled live preview.

Three actual-game C/Esc/Tab cases pass: stop in place, resume Pause with Escape,
then ordinary W input moves without a saved-pose teleport or application exit.
Full integration initially used SDL dummy video, which cannot create this GPU
context; that invocation is rejected, not an application defect. Correct isolated
GPU invocation passes 162 tests, skips three opt-in reviews (the complete cinema
review ran separately), and exposes one independent pre-existing counter-handle
ownership failure tracked as HOUSE-03644. Static gates pass except the inherited
owner-owned `.claude` layout entry; 349 strict-XNA translation units are clean.

Commands and local logs:

```sh
taskset -c 0-3 env -u DISPLAY -u WAYLAND_DISPLAY SDL_VIDEODRIVER=offscreen EGL_PLATFORM=surfaceless LP_NUM_THREADS=4 GALLIUM_NUM_THREADS=4 HOUSE_CINEMA_GPU_REVIEW=1 build/cnahouse_integration_tests --gtest_filter=CinemaGameTests.CompleteActualGpuTourReview
```

`/tmp/house-03572-{accepted-units,final-control-focused,final-tours,
final-integration-gpu,final-static}.log` retain the other results.
The source initialstate SHA256 is
`e822d25df096f1a1f02a84f831f4b9d5855d825986a35bbb0ac366e48b5462c0`;
production executable SHA256
`074e78fd36e0f12cf5dee7b3b0d8060727d10925b38d579b25c8f6e37fb7990a`.
At ordinary pace the tested circuit is approximately 41 minutes; there is no
speed-up or recording/export subsystem. Use an external recorder.

No uncontended FPS, physical-keyboard manual walkthrough or performance
improvement is claimed by this capture.
