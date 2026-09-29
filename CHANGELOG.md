# Changelog

Versions follow [`docs/versioning.md`](docs/versioning.md).

## 1.1.0 — 2026-09-30 — the multiplatform release

The same house on the Linux desktop, in a browser and on Android. With this release every MUST task
of `plan.md` is done and the project is in maintenance mode.

**Web.** The whole game as a WebGL 2 page: `tools/ci/package_web.py` writes a directory any static
server can host. The browser starts on the Web preset and holds 60 FPS in Chrome on the reference
machine; the download is 96.8 MB compressed. `?arg=` passes a command line.

**Android.** An arm64-v8a APK of 99.6 MB (`tools/ci/package_android.py`) with the content inside
it. Touch controls: a floating stick to walk, a drag to look, WALK/RUN, MENU, and the phone's Back
button; the Android preset is chosen automatically, settings are kept in the app's storage, and
the game pauses in the background. The launching intent's `args` extra passes a command line.

**Also.** `--filming-tour` starts the whole-property walk from the command line on any platform.
Touch-only devices can leave the settings page and set their own look sensitivity. The Android
build simulates exactly what the desktop does (no fused multiply-add on arm64). The packages carry
only the assets the game loads. Three CNA defects met on the way were fixed in CNA: `.cnb` content
inside an APK, application lifecycle events, and activation after an Android launch.

**Tested.** Unit, GPU integration and render suites; the Web DONE checklist in Chrome and Firefox
and the Android DONE checklist on the emulator, each with the whole filming tour (90/90 views);
the Web and Android smoke tests (`tools/ci/web_smoke.py`, `tools/ci/android_smoke.py`). How to run
them: `docs/testing.md`.

**Known limitations.**

* Android was measured on the emulator (API 35, GPU-accelerated), not on a phone. Through the
  emulator's arm64 translator the two street views run at 30 FPS; they draw 509 calls, over the
  Android preset's 400 and the Web preset's 500, with the frame targets met.
* On Android the game learns it went to the background only when it returns: SDL holds the game
  thread while the app is paused.
* The APK is signed with a development key.
* On the Web a lost WebGL context needs a reload, and settings last a session.
* Two logged S3 visual items: mottled light on the attic storage wall and the plain street planting.

**Credits.** Built on CNA -- XNA 4.0 in C++ -- and sharp-runtime, with SDL3 and SDL3_mixer
(`NOTICE.md`). The interface font is Noto Sans (OFL 1.1). Every third-party model,
texture, sound and data set, with its author and licence, is in
`licenses/THIRD-PARTY-ASSETS.md` and on the in-game credits screen.

## 1.0.0 — 2026-09-29 — the feature-complete Linux desktop release

The first tagged version: everything since the repository began on 2026-09-06, as the desktop
meets `plan.md`'s Definition of DONE (D1–D14 for the desktop; Web and Android follow in their own
milestones).

**The property.** A furnished house to walk through on foot: basement, ground floor, garage, two
upper floors and the attic, joined by real stairs, with the front and rear gardens, a shed and the
street. 96 cells, 90 of them accessible. Every accessible room is dressed to its purpose and lit
by day and at night; the twelve main rooms and five hero areas carry the most detail.

**Environment.** Time of day runs automatically (a 24-minute day by default) or is set; sun, moon
and stars follow the calendar; the sky shows clear, overcast and rain, with a snow state; rain
stays out of covered places, surfaces get wet and fog follows the weather. Interior lights follow
an automatic schedule.

**Audio.** Footsteps on six surface categories, an interior room tone, exterior day and night
ambience, and rain and wind layers that are quieter indoors, each with its own volume.

**Application.** Title, pause, one settings screen (graphics, audio, controls, environment),
credits with every third-party asset, quit and a controls hint. `C` starts or stops a bounded
filming tour of the whole property. Quality presets High, Web and Android (and Ultra), selectable
with `--quality` and in the settings; lower presets draw cooked vegetation LOD and upload only
their own level.

**Rendering.** Stock XNA 4.0 effects only (Tier S), with an optional compiled-effect tier (Tier E),
on CNA's OPENGLES3 renderer. No CNA extension is used; the XNA-only and strict-XNA gates enforce
it.

**Performance.** The eight fixed scenarios meet the High target on the reference desktop (CPU
≤ 9.5 ms, GPU ≤ 14 ms, ≤ 1,800 draws, ≤ 3.4 M triangles); the Web and Android presets are measured
against their own budgets.

**Quality.** Unit, GPU integration and software render suites pass, also under ASAN and UBSAN;
a 20-minute and one 2-hour stability run pass; the static gates guard the content and code.

**Packaging.** `tools/ci/package_linux.py` builds a self-contained Linux archive (game, CNA and SDL
libraries, content, licences, launcher) that runs on a clean profile with or without an audio
device.

**Known limitations.** Web and Android are not yet released. Two logged S3 items remain: mottled
light on the attic storage wall and the plain street planting.
