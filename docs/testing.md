# Testing

What each suite covers, how to run it, how to add a case, and how to accept a render change. The
suites are GoogleTest executables in the build tree, registered with CTest by kind
(`tests/CMakeLists.txt`); they run from the **repository root**, so fixture paths are stable
wherever the build tree lives. Never run a windowed suite on the owner's desktop display: use the
offscreen driver, a private Xvfb or CNA's `tools/platform/run_gpu_tests_private.sh --exec`.

## The suites

| Suite | Binary | Covers | Runs where |
|---|---|---|---|
| Unit | `cnahouse_unit_tests` | Pure logic: world data and formats, collision, movement, visibility and culling, lighting, weather, time, audio decisions, settings, UI screens, input, command line, content paths | anywhere, no display (`env -u DISPLAY -u WAYLAND_DISPLAY`) |
| Integration | `cnahouse_integration_tests` | The real `CnaHouseGame` on the real content: loading, the walk, the grand tour of every cell, stairs and ground support, audio gates, lighting walks, the filming tour, packaging-relevant paths | GPU display (the private runner) with `SDL_AUDIO_DRIVER=dummy`; four tests need `SDL_VIDEODRIVER=offscreen` with no display at all and skip otherwise |
| Render | `cnahouse_render_tests` | Pixel comparisons against `tests/render/reference/*.png`: blockout poses, first-person poses, property poses, representative rooms by day and night, sky, sun, moon, stars, UI, fonts, culling sanity | a private Xvfb with software GL (below) |
| Performance | `cnahouse_perf_tests` | The eight representative scenarios × the High, Web and Android presets: draw calls, triangles, CPU and GPU time against the preset's budget; only High's counts are asserted | offscreen GPU, serial (CTest sets `SDL_VIDEODRIVER=offscreen`) |
| Gates | `tools/ci/run_checks.sh` | Layout, the XNA-only lint, the strict-XNA compile of every unit, content, manifests, licences, schemas, tables, budgets and the tools' own self-tests | anywhere |
| Web smoke | `tools/ci/web_smoke.py` | The built Web page in headless Chrome: title, Start, 300 walk frames, a settings change, no errors | after a Web build |
| Android smoke | `tools/ci/android_smoke.py` | The installed APK on a device or emulator, by touch: title, Start, 300 walk frames, MENU → Settings → quality change, Home and return | one device attached through adb |

## Running them

```bash
cmake --build build --target cnahouse_unit_tests cnahouse_integration_tests cnahouse_render_tests
env -u DISPLAY -u WAYLAND_DISPLAY build/cnahouse_unit_tests
SDL_AUDIO_DRIVER=dummy ../cna/tools/platform/run_gpu_tests_private.sh --exec build/cnahouse_integration_tests
env -u DISPLAY -u WAYLAND_DISPLAY SDL_VIDEODRIVER=offscreen SDL_AUDIO_DRIVER=dummy \
    build/cnahouse_integration_tests --gtest_filter='*CinemaStop*:GroundSupport*'
Xvfb :141 -nolisten tcp &   # a private display; stop it afterwards
DISPLAY=:141 SDL_VIDEODRIVER=x11 LIBGL_ALWAYS_SOFTWARE=1 build/cnahouse_render_tests
ctest --test-dir build -L perf --output-on-failure   # numbers only from a Release build
tools/ci/run_checks.sh
```

`ctest --test-dir build --output-on-failure` runs every registered test; the authoritative count
of a suite is its binary's own (`--gtest_list_tests`). Opt-in reviews -- the GPU cinema capture,
the doorway frames, the moving whole-house review -- skip unless their `HOUSE_*` variable is set;
the skip message names it. The sanitizer build is the `linux-asan` preset
(`LD_PRELOAD="libEGL_mesa.so.0 libGLX_mesa.so.0" ASAN_OPTIONS=verify_asan_link_order=0
LSAN_OPTIONS=suppressions=tools/ci/lsan.supp`).

## Adding a case

Put the test in the suite that matches what it needs: no GPU and no content means `tests/unit/`,
the real game or content means `tests/integration/`, a picture means `tests/render/`. A new
`.cpp` in those directories is picked up by the next configure. One behaviour per test, named as a
sentence (`AWebOrAndroidBuildStartsOnItsOwnPreset`); fixtures live beside the test or are generated
by the build (`CNAHOUSE_TEST_*` paths); anything a test writes goes to `build/test-output/`. A
regression test is written to fail without the fix -- run it once against the old code. A tool
under `tools/` gets a `--selftest` of planted failures and a line in `run_checks.sh`.

## Accepting a render change

A reference changes only when the change in the picture is intended; the whole procedure, the id
grammar and the history of why it is strict are in [`screenshot-scenes.md`](screenshot-scenes.md).
In short: a failing comparison writes `<id>-actual.png` and a difference image to
`build/test-output/`; look at the actual image, and only if it is right copy it over
`tests/render/reference/<id>.png` -- every scene the change affects, in the commit that made the
change, whose message says why. A region that genuinely cannot be reproduced is excluded by name,
never by widening the tolerance.
