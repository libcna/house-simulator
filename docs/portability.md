# Platform portability evidence

This is a running validation record, not a claim that the Linux, Web or Android DONE checklists
have passed. `plan.md` remains the release authority.

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
committed assets or an image-quality baseline. The current 1600×900 canvas overflows the
1280×720 test viewport (`HOUSE-02895`). Firefox briefly showed its slow-page warning while
loading the 391 MiB preload; pack sizing remains `HOUSE-02850`.

Touch-only Web control is **not yet validated or claimed functional**. CNA's `TouchPanel`
can report a connected device after a touch event, but the required multi-touch `TouchSource`
and touch HUD are still open M15 work (`HOUSE-02991`–`HOUSE-02995`). The conditional Web
selection/browser check is tracked separately as `HOUSE-03723`; the Web DONE checklist depends
on it. Desktop Chrome/Firefox evidence does not substitute for that check.
