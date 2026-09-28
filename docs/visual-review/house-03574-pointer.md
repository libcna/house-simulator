# Windowed menu pointer correction — 2026-09-28

Owner report: occasional displaced mouse click targets in windowed mode;
fullscreen appears correct. ACTIVE/MUST HOUSE-03574, D8/D11. No monitor windows.

## Reproduction and cause

The deployed pre-fix application is run with ordinary KeyboardMouseSource on a
private Xvfb display, windowed 800x600, private settings, audio disabled. The menu
fits its 1600x900 reference into an 800x450 canvas at y=75. Mouse hit coordinates
instead divide by 800x600. Clicking the actual Settings ink at (400,314) leaves
the main menu unchanged. Open Settings with Down/Enter, then click the displayed
Field of view text at (590,215): **Master changes to 75%, FOV remains 70°**.

Evidence: `/tmp/house-03574-before-{main,settings,keyboard-settings,fov,back}.png`
and `/tmp/house-03574-before-pointer.log`. The first instant-click draft is not
acceptance: down/up could both occur between device snapshots. The final probe
holds each mouse button for 200 ms, then releases it; the wrong audio control is
actually selected and changed. This is real XNA pointer delivery on an isolated
software X surface, not a hardware-GPU claim or a human desktop session.

Source investigation also identifies physical-font versus scaled-row coordinates:
DrawString keeps the authored font size while row anchors scale with the canvas.
Normalizing the whole back buffer cannot account for either letterbox space or
the rendered line's half-height. CNA's current Mouse implementation already
maps window pixels to logical back-buffer coordinates; scaling by ClientBounds
again would be incorrect. No upstream modification is required.

## Correction

Reuse the existing TextRenderer canvas, KeyboardMouseSource and TouchSource.
Both device sources invert that safe canvas and compensate the measured physical
font half-height for top-anchored row centres. Reject clicks/taps in padding,
without manufacturing an edge when a held pointer moves inside. Refresh the
live layout before sampling; keep recenter/camera/touch dimensions aligned with
the live standard-XNA viewport. Do not change mouse-look deltas, menu drawings,
serialized settings, owner preferences, audio mix or quality profiles.

## Current validation

The corrected normal windowed input opens Settings and the same (590,215)
held click changes **FOV to 85°**, leaving Master unchanged. Reviewed originals:
`/tmp/house-03574-after-{main,settings,fov}.png`; device-run log:
`/tmp/house-03574-after-pointer.log`. Owner settings were never used or modified.

* Current build passes with warnings as errors. The first test draft needed
  explicit int-to-float casts; the corrected build passes.
* 65 focused input/touch/layout/menu tests pass; **1540/1540 unit tests pass**,
  172 suites, 124635 ms (`/tmp/house-03574-unit.log`).
* **172 hardware-GPU integration tests pass / 3 explicit review opt-ins skip /
  0 fail**, 175 total, 343022 ms (`/tmp/house-03574-integration.log`).
* The new actual-game pointer regression starts at 800x600, 1600x900 and
  2000x900, clicks the drawn Settings/FOV/resolution rows, then verifies the
  new live surface and unchanged four audio volumes. A final extension also
  clicks fullscreen on/off and FOV after each ApplyChanges. **3/3 focused GPU
  tests pass**, including that extension and the existing fullscreen tests
  (`/tmp/house-03574-fullscreen-gpu.log`). Runtime code is identical to the full
  suite snapshot; only this regression's frame sequence was extended afterwards.
  Inspected captures: `build/test-output/house-03574/{800,1600,2000}-settings.png`.
  These final images are 1024x768 after the tested resolution change, not images
  claiming to show the three starting aspects.

The native fullscreen experiment on private Xvfb **is not a successful native
fullscreen verification**. The server has no window manager. Both the old and
corrected binaries log `Time out elapsed after mode switch on display 3 with no
window becoming fullscreen; reverting`; both retain an 800x600 window. Current
and old logs, geometry, screenshots and owned-process backtraces are
`/tmp/house-03574-{current,old}-fullscreen*`. This pre-existing SDL/window-manager
fixture limitation is separate from the actual-GPU GraphicsDeviceManager and
pointer-after-ApplyChanges checks. No upstream code or window-manager installation
was changed to hide it. The earlier instantaneous keyboard draft dropped an edge
and is discarded; the final comparison held/released each key for 200 ms.

No visual-quality or performance improvement is claimed. No Web/Android build
or emulator runtime validation is inferred from these desktop regressions.
## Strict validation correction — task remains open

`tools/ci/run_checks.sh` printed all gates green, **356 strict translation units /
109 destructor exemptions** (`/tmp/house-03574-static.log`). **That strict result
is invalidated**, not accepted: the final one-TU compiler probe returns real
syntax errors in current CNA `3d5742e84`. `CNAEXT using` aliases become invalid
`[[deprecated(...)]] using` declarations under `CNA_STRICT_XNA_API`:
`ContentManager.hpp:313/395`, `GameComponentCollection.hpp:25/27/29`,
`GameWindow.hpp:52/54`. Exact compiler log:
`/tmp/house-03574-final-test-strict.log`.

The existing checker discarded subprocess exit status and scanned only
deprecation matches; a failed compiler therefore looked clean. HOUSE-03683
corrects that separate House-owned CI defect with planted fail-closed tests.
BL-18 records the remaining upstream syntax defect. No macro/policy relaxation,
upstream edit or claimed strict acceptance. **HOUSE-03574 remains open despite
the reproduced and corrected pointer behaviour** until current strict validation
can actually compile. The current full corrected wrapper reports **38 compiler
invocations failed**, with **only `xna-strict` failed**; all other gates, including
seven new checker selftests, pass (`/tmp/house-03683-static.log`). The same
one-TU failure is reverified after CNA advances to `a62c40b09`
(`/tmp/house-03683-upstream-recheck.log`). Minimal C++ leading-attribute alias
fails; the correctly placed alias-attribute control compiles
(`/tmp/house-03683-alias-{minimal,control}.log`). `git diff --check` passes.
The temporary pre-fix executable `build/house-03574-before` was removed after
recording evidence; screenshots and logs remain.
