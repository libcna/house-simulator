# Platform portability evidence

This is a running validation record, not a claim that the Linux, Web or Android DONE checklists
have passed. `plan.md` remains the release authority.

## House Android lifecycle — 2026-09-26

`HOUSE-03036` uses XNA `Game` events, not an Android/SDL runtime hook in House:

| Source event in current CNA | XNA event | House action |
|---|---|---|
| Desktop focus lost; Android will enter background | `Game::OnDeactivated` | Pause updates, discard held touch roles and pending run toggle, release mouse capture, mute the existing audio mix |
| Desktop focus gained; Android entered foreground | `Game::OnActivated` | Reset elapsed time, discard stale touches, resume updates and mix |
| Android surface destroyed/recreated | CNA's renderer surface invalidation at foreground/resizing | House keeps XNA graphics resources; CNA restores the output surface without a House-native graphics call |

CNA's current `Game.cpp` maps Android background/foreground to activation events,
suspends its loop while backgrounded, resets elapsed timing at return and invalidates
the renderer surface. The same House event handlers ran under real Linux/Xvfb focus
transfer: log sequence `activated → deactivated → activated`.

On the `Medium_Phone` API-35 GPU-backed emulator, the installed arm64 House APK kept
PID 6363 across `KEYCODE_HOME` and return. SDL logged `onPause()` then
`surfaceDestroyed()`, followed by `onResume()`, `surfaceCreated()` and
`surfaceChanged()`. EGL frame-submission logs resumed (about 60 frames per
one-second interval), with no fatal exception. Once the foreground orientation
settled at 2400×1080, a 1600×700 central crop compared pixel-identical before
and after backgrounding (AE 0); its center pixel was `srgba(18,20,24,1)`.
The clear-color frame proves the output surface was restored; without packaged House content it cannot
prove scene rendering, touch traversal or a performance target. Those remain open.

## Android settings store — 2026-09-26

`HOUSE-03034` reuses the existing XNA `StorageDevice`/`DesktopSaveStore`. Current
CNA maps that standard-XNA container beneath the Android package's private `files`
directory. On the `Medium_Phone` emulator, the built APK wrote
`files/game/CnaHouse/AllPlayers/settings.json` (596 bytes, schema version 11,
mode 0600), confirmed with `adb shell run-as com.libcna.house`. The file remained
after force-stop and relaunch. Startup reads the file if present; each settings-page
change and the in-walk speed toggle write through the same store. The existing
store and settings tests pass.
No SDL or Android storage API was introduced into House runtime. This is not yet
proof of a visible settings-page interaction on the device; that awaits content
delivery and the Android traversal checklist.

## House Android package and arm64 build — 2026-09-26

The new `android/` Gradle/NDK project follows CNA's existing `demo_devices` SDLActivity
precedent. It builds House's real `cnahouse_core` and `Main.cpp` into `libmain.so` for
API 24/`arm64-v8a` with OPENGLES3, alongside CNA's SDL3 and SDL3_mixer libraries.
Gradle 8.12/AGP 8.7.3, NDK 29.0.14206865, CMake 4.1.2 and Build Tools 36.1.0
produced a 47,879,471-byte debug APK before content packaging. Generated outputs stay in the existing
`build-probe/`; Ninja's compile pool is limited to four jobs, as is Gradle, with
CPU affinity 0–3 and the shared ccache.

The APK installed on `Medium_Phone` (API 35, x86_64 emulator translating the
arm64-v8a library). The first launch failed at SDL's entry lookup: `llvm-nm -D`
showed `_Z8SDL_mainiPPc`, and logcat said `Couldn't find function SDL_main`.
The House entry point now uses C linkage for Android; `llvm-nm -D` confirms the
literal `SDL_main` export, SDLActivity loads it, and the process remains running.
The observed screen is still dark because House content and its Android content
root have not been delivered or validated. This is build and launch evidence for
`HOUSE-03031`/`HOUSE-03032`, **not** the device traversal, performance or Android
DONE checklist. Content delivery is `HOUSE-03033`, after its performance-preset
dependency; the independent settings and lifecycle tasks can proceed first.

## CNA Android graphics gate — 2026-09-26

The Android environment is **available**, not an unavailable-device blocker. The installed
SDK has `adb` 37.0.0, emulator 36.5.11, platforms 34/35/36.1/37.0, NDK
23.1.7779620/25.2.9519653/29.0.14206865, CMake 3.22.1/4.1.2 and Build Tools
36.1.0. `adb devices -l` reported `emulator-5554 device`; `emulator -list-avds` reported
`CNA_Racing_API35` and `Medium_Phone`. The latter was chosen because its AVD enables
GPU acceleration. It booted with `-gpu host -no-window -no-audio -no-snapshot-save`,
`sys.boot_completed=1`, API 35, x86_64 with `ro.product.cpu.abilist=x86_64,arm64-v8a`.
SurfaceFlinger reported the Android Emulator OpenGL ES Translator on host AMD Radeon
780M, OpenGL ES 3.1. `/dev/kvm` was absent, but boot and graphics still worked.

Current CNA `cefe6c83b` and sharp-runtime `d86adb65` were configured using the NDK
29.0.14206865 Android toolchain for API 24, `arm64-v8a` and
`CNA_GRAPHICS_RENDERER=OPENGLES3` in CNA's existing `build-probe/`. With the shared
`/rv/cnaccache` and four build jobs/CPU affinity 0–3, both `cna_runtime` and
`cna_renderer_easygl` built successfully. The old sharp-runtime portability errors
did not recur. This proves the arm64 cross-compile and explicit renderer selection,
not a House Simulator Android build.

For runtime proof, a **temporary ignored copy** of CNA's existing `demo_devices` APK
template was built in House's existing `build-probe/`. Only that copy selected the
installed NDK 29, Build Tools 36.1 and CMake 4.1 (the template defaults required
unavailable Build Tools 34/NDK 30 and CMake 3.22, below EasyGL's 3.23 minimum),
restricted Gradle's native target list to `main` to avoid cross-building host content
tools, selected OPENGLES3, and included SDL's documented `SDL_main.h` in `Main.cpp`.
The first APK had no `SDL_main` export; `logcat` showed `Couldn't find function
SDL_main`, then the app returned home. The corrected library exported `SDL_main`
(`llvm-nm -D`), the 18-MB arm64 APK installed, the app remained running (`pidof`
reported 4764), and a real emulator screencap shows CNA `SpriteBatch` rendering
the demo's colored panels: [Android CNA sample capture](android-cna-graphics-sample.png).
No graphics crash appeared in the post-launch logcat. CNA/sharp-runtime source trees
were not edited. Thus the CNA gate is passed; House Simulator's Android packaging,
touch traversal and performance remain open. Older dated entries below are snapshots
from before this re-verification.

## Touch-only Web controls — HOUSE-03723 (2026-09-26)

The Web build still starts with keyboard/mouse controls. On the first actual XNA `TouchPanel`
frame it switches to the existing `TouchSource` and touch-only HUD before sampling that frame;
device capability presence alone does not switch a hybrid desktop. Chrome 152 in 400×800
mobile/touch emulation, serving the full `build-consumer/cna-house.html` over localhost,
reported the transition as `Web touch input selected`. The first touch dismissed the
first-gesture loading gate; a subsequent tap displayed the touch-spaced main menu, a
tap on Start entered the furnished walk with MOVE/MENU/WALK HUD, and the upper-right
Menu button opened Pause. Inspected CDP captures are `/tmp/house-03723-before.png`,
`/tmp/house-03723-menu.png`, `/tmp/house-03723-walk.png` and
`/tmp/house-03723-pause.png`. The canvas occupied a 400×225 letterboxed region within
the 400×800 portrait emulation; all taps were placed inside that measured rectangle.

The first browser probe selected touch but could not dismiss the loading gate. CNA's
event snapshot had already advanced the new finger from `Pressed` to `Moved` before
`TouchSource` read it. Recognising the standard-XNA previous `Pressed` location as
the new-finger edge fixed that without touching CNA or adding an input system; the
source emits the edge only once on the next `Moved` frame. Twelve focused native
touch tests include that case. The native and full Web builds pass with the shared
ccache and four-job cap. The Web link still warns about its 391-MB bundle, owned by
`HOUSE-02850`; this control validation does not close Web performance or pack sizing.

## Touch HUD desktop readiness — HOUSE-02995 (2026-09-26)

The game now selects and displays touch controls only from the project-owned
`Platform.hasTouch && !Platform.hasKeyboard` profile. Android defaults to touch-only;
Linux and Web default to keyboard/mouse. `--force-touch` overrides those input facts
for a Linux test without querying CNA capabilities. Menu and walk-speed hit boxes
follow the safe 1600×900 virtual canvas even on a 20:9 display. The inspected
`tests/render/reference/ui-touch-20x9.png` joins the six existing UI references;
all seven match under Xvfb software GL. Live Linux captures in `/tmp` show the
speed button switching WALK → FAST (`house-02995-fast2.png`), Menu opening Pause
without quitting (`house-02995-menu.png`), and no touch HUD in normal desktop mode
(`house-02995-desktop.png`). These desktop results do not claim Android device or
touch-only Web browser validation; BL-13 and `HOUSE-03723` remain open.

## Desktop touch-control preview — HOUSE-02992 (2026-09-26)

On Linux, `--force-touch` selects the XNA touch input source and emulates one finger from
the mouse for desktop tuning. The left-bottom floating stick uses a 180-virtual-unit
radius and analogue magnitude; the right half drives look except for the two reserved
160-vu button corners. Its look sensitivity is separate from mouse sensitivity in the
backward-compatible settings JSON. The HUD ring reuses the current SpriteBatch/texel.
Xvfb captures `/tmp/house-02992-active2.png` and `/tmp/house-02992-look5.png` prove the
active ring and a 200-pixel right-half drag changing the walk camera respectively.
`HOUSE-02995` still owns actual touch buttons, the Platform-driven visibility rule and
20:9 UI reference; `HOUSE-03723` still owns touch-only browser verification. This is not
Android device validation; BL-13 remains open.

## Android desktop-side touch input — HOUSE-02991 (2026-09-26)

The new XNA `TouchPanel`-backed `TouchSource` is unit-tested with recorded multi-finger
snapshots and implements the same `IInputSource` consumed by the game. It tracks floating
stick and look IDs independently, retains them across reordered/crossing fingers, reports
new presses as normalised UI tap edges, and drops released/missing roles without a look jump.
Its native and Web compilation passed. This is input-source readiness only: the visible HUD,
desktop `--force-touch` selection, touch-only browser check and real Android device path are
still open. An upstream Android graphics blocker remains a blocker, not proof of DONE.

## Web preload progress — HOUSE-02899 (2026-09-26)

The Web target now links a small project-owned Emscripten shell. It uses Emscripten's existing
`Module.setStatus` download reports and `monitorRunDependencies` preparation counts to show a
visible progress bar, then yields to the existing in-game loading/first-gesture screen. That
screen remains visible until the user enters the main menu. The default Emscripten toolbar is
gone; the canvas fits the window without its former header/scrollbar. The shell does not fetch
packs itself: CMake still preloads the existing `content` and `content-fx` roots up front. It
does not attempt progressive fetch or streaming.

Chrome 152 was reloaded with its local network deliberately throttled. At five seconds the
loading overlay was visible with 8,504,652 / 410,635,649 bytes; at fifteen seconds it showed
31,963,944 / 410,635,649 bytes. After normal speed was restored, the overlay was hidden,
the canvas showed the existing `Press any key to begin` screen, and one click opened audio
and displayed the complete main menu. Chrome could still start the house and acquire pointer
lock from the scaled canvas. Firefox 140 ESR independently reached the same in-game prompt
and main menu after its local test server was restarted (the initial connection error was
that stopped server, not a game error). Inspected local captures are
`/tmp/house-02899-download-5s.png`, `/tmp/house-02899-download-15s.png`,
`/tmp/house-02899-ready.png`, `/tmp/house-03721-click.png` (Chrome's post-gesture menu),
`/tmp/house-02899-firefox-loaded.png` and `/tmp/house-02899-firefox-menu.png`.
The measured 410.6 MB transfer and the 391 MiB pack remain overlarge; `HOUSE-02850` owns the
limit, while `HOUSE-02898` owns Web performance.

## Web canvas and first-gesture audio — HOUSE-02895 (2026-09-26)

The full 391 MiB-preload game was served from localhost to Chrome 152 (headless/WebGL2) and
Firefox 140 ESR (Xvfb/software WebGL2). The Web launch now requests a 1280×720 back buffer;
desktop still uses its existing 1600×900 default. The Graphics menu offers 1280×720 and
1600×900 canvas sizes. The browser's reported 800×600 pseudo-monitor size is excluded because
its row spacing makes the existing settings page unreadable. Native monitor-mode enumeration
is unchanged. `SettingsScreenTests.WebCanvasSizeIgnoresTheBrowserPseudoMonitorMode` covers
the list and cycling behavior.

Both browsers logged `audio waiting for a user gesture` on a fresh load before input, then
`audio device opened` and `audio ready` after a real canvas click. Chrome used CDP console
events and a canvas click; Firefox used WebDriver BiDi `log.entryAdded` with a held X11 click.
Neither opened the audio device during preload. Chrome's DOM canvas dimensions changed from
1280×720 to 1600×900 after Graphics → Canvas size. Firefox showed the same change in its
inspected settings capture. The Fullscreen row entered and left browser fullscreen in both:
Chrome's `document.fullscreenElement` toggled false → true → false, and Firefox displayed the
browser's fullscreen confirmation and the menu's On → Off state. Headless Chrome's virtual
fullscreen screen was only 800×544; that is a test-display size, not a production target.

Inspected local captures: `/tmp/house-02895-chrome-click.png`,
`/tmp/house-02895-chrome-key.png`, `/tmp/house-02895-firefox-settings.png`,
`/tmp/house-02895-firefox-resized.png`, `/tmp/house-02895-firefox-fullscreen.png` and
`/tmp/house-02895-firefox-windowed.png`. The generic Emscripten toolbar visible in these
earlier captures was subsequently removed by `HOUSE-02899`; those snapshots still accurately
record the in-game controls and DOM canvas transitions. Pack sizing remains `HOUSE-02850`.

## Web desktop controls — HOUSE-03721 (2026-09-26)

The full `build-consumer/cna-house.html` build was served over localhost. Chrome 152
(headless, WebGL2/SwiftShader) and Firefox 140 ESR (Xvfb, software WebGL2) both reached the
main menu and the furnished house. In both browsers the menu kept its visible cursor, a walking
canvas click captured the pointer, mouse movement changed the view, and the view remained still
after movement stopped. Escape opened Pause and released the pointer. Switching away from the
game tab and back also opened Pause in both browsers; Resume returned to walking. Firefox's
real-key test moved through the scene with W and Up; the source's WASD/arrow alias and Shift
speed-modifier edges are covered by `InputTests` on the native build. Chrome's CDP-synthesized
Escape was not delivered to SDL while pointer-locked, so its focus/Escape path was also tested
with DOM key events and an actual tab-activation change. Firefox used XTest keys and tabs.

The Web launch bridge reads CNA's existing XNA `Game.IsMouseVisible` canvas cursor state. It
requests DOM pointer lock only for walking, releases it for menus, and feeds browser-initiated
pointer-lock loss through the existing Escape binding. XNA 4.0 has no pointer-lock method;
the bridge adds no graphics API or CNAEXT runtime call. The browser owns locked motion, so the
desktop mouse-recentering path is disabled only for Emscripten.

Firefox pointer capture was additionally checked through X11: a canvas click at (600, 450)
recentered the system pointer to (642, 445), and a synthetic +120 X movement returned to
(642, 445) after one second while changing the camera. This is independent of the hidden
cursor CSS.

Representative inspected captures: `/tmp/house-03721-click.png`,
`/tmp/house-03721-move.png`, `/tmp/house-03721-state.png` (Chrome) and
`/tmp/house-03721-firefox-locked.png`, `/tmp/house-03721-firefox-arrow.png`,
`/tmp/house-03721-firefox-refocused.png`, and
`/tmp/house-03721-firefox-lockproof.png` (Firefox). These are local test evidence, not
committed assets or an image-quality baseline. The earlier 1600×900 launch canvas has since
become 1280×720 under `HOUSE-02895`. Firefox briefly showed its slow-page warning while
loading the 391 MiB preload; pack sizing remains `HOUSE-02850`.

The desktop Chrome/Firefox evidence above did not validate touch-only Web control;
the separate mobile-emulation check is recorded under `HOUSE-03723` above. It does
not substitute for the remaining Web performance, pack-sizing or Android device gates.
