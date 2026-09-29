# Final walkthrough evidence — HOUSE-02714

The day/night walkthrough and classification have been performed on the actual
GPU application. **Accepted 2026-09-29**, once CNA `2c70eaf0f` fixed the
strict-XNA blocker BL-18 and the mandatory gate compiled clean. The defects found
here are fixed under HOUSE-03631 ([Round 180](house-03631-fixes.md)); this
record itself describes the review as performed.

## Method and current evidence

* Fresh normal-controller GrandTour trace: all 90 intended-accessible arrivals,
  11729 samples, PASS, `/tmp/house-02714-trace.log`.
* Existing moving review follows that trace through the normal game controller,
  without teleporting between rooms. The existing deterministic test clock runs
  four ordinary 120 Hz physics steps per 30 Hz review frame. Production GameTime
  is unchanged. This is automated first-person transport, **not** a claim of a
  new human WASD/mouse staircase-usability test.
* OPENGLES3, Tier S+E, High, clear weather, 960x540, frozen 10.5 h and 22 h.
  Surfaceless EGL, no DISPLAY/WAYLAND_DISPLAY, no software override, dummy audio.
  Actual amdgpu client 1012887, PCI 0000:c3:00.0, is recorded in
  `/tmp/house-02714-hardware-proof.log`. These timings are review wall time,
  **not** performance-budget measurements.
* Complete day: PASS, 90 arrivals, 811464 ms. Complete night retry: PASS,
  90 arrivals, 692254 ms. Logs: `/tmp/house-02714-day.log` and
  `/tmp/house-02714-night-retry.log`. Both capture sets are under
  `build/test-output/lighting-walk-{day,night}-to11729/`.
* Each run records 1076 frames: four views per room (360), plus four consecutive
  frames at each of 179 cell changes (716). All 27 day and all 27 night zone
  contact-sheet pages were actually inspected. Full-size day frames 550
  (L1_BED3) and 89 (EXT_SHED) were inspected to confirm the concrete defects.
* Six representative entry-frame bursts were inspected for each time of day:
  L0_HALL, L0_STAIR_MAIN, L1_HALL, L2_HALL, B1_HALL and L0_GARAGE. No threshold
  flash appears in these sampled consecutive frames. This is **not** a claim
  that all 716 entry frames were individually reviewed, or that a static image
  can prove a movement defect fixed. Sheets:
  `build/test-output/house-02714-review-sheets/{day,night}-threshold-sample.png`.
* Level pans miss low fixtures in small rooms. Another 72 GPU first-person
  downward views at the same arrival positions supplement 18 bathroom/service
  and cinema/library cells. All four supplement sheets were inspected. These
  first-frame static captures establish furnishing placement, **not** settled
  lighting or movement acceptance. Evidence:
  `build/test-output/house-02714-supplement/`,
  `/tmp/house-02714-supplement.log`.
* No house geometry, material, lightmap or asset changed for this review. The
  only test-source change selects the existing bounded review-clock seam.
* All five hero areas also have current GPU day/night captures at the six
  established G5 poses: exterior-front, front-path, living-composition,
  kitchen-facing-west, library and basement-cinema. All twelve were inspected
  on the two hero sheets. Evidence: `build/test-output/house-02714-heroes/`,
  `/tmp/house-02714-heroes.log` and `house-02714-review-sheets/{day,night}-heroes.png`.
  These static hero views supplement the completed continuous controller walk,
  not replace movement acceptance or prove a new performance result.
* The deliberately interrupted five-cell day pilot is not acceptance. Two
  earlier night runs received SIGTERM, for an unestablished reason; neither is
  a pass or proof of an external product blocker. Their partial images remain
  in `build/test-output/house-02714-interrupted-night{,-first}/`; logs are
  `/tmp/house-02714-night{,-first}.log`. The completed retry used a persistent
  PTY outside the sandbox and still opened no monitor window.

## Findings by zone

| Zone / tier | Severity | Finding and evidence | Disposition |
|---|---|---|---|
| Z-EXR / C3, EXT_SHED | S1 | From inside, roof/gable enclosure disappears and sky is visible above the walls, by both day and night. Day `89-EXT_SHED-view1.png` confirms it at full size. `tools/world/fence_gen.py` emits only outward roof/gable faces; normal back-face culling has no inward enclosure to draw. | Existing MUST HOUSE-03631: correct the enclosure, not global culling. S1 is fix-now under R1. |
| Z-L1 / C3, L1_BED3 | S2 | Leaves/branches visibly intersect the bedroom corner wall above the desk, by day and night. Day `550-L1_BED3-view2.png` confirms it at full size. A nearby mature-tree instance is at (-13.50, 0, -12.00); its transformed canopy clearance still needs measurement. | HOUSE-03631: measure and correct placement, not remove the tree family or change the renderer to hide it. |
| Z-EXR / C3, EXT_SHED | S2 | Daytime enclosed-side surfaces/tools are difficult to read. The bright doorway permits navigation, but does not establish C3 material/purpose readability. Night lighting is substantially more readable. Actual daytime-light selection/bake/material cause remains to be investigated after correcting the roof. | HOUSE-03631: room-local or underlying correction, never a blind global ambient increase. |
| Z-EXF, Z-EXR, Z-STR / C3 | S2 | At 22 h, large foliage bands and tree crowns remain conspicuously bright against the dark ground and facade. Night EXT_ROAD, EXT_SIDEYARD_W and EXT_FRONTYARD views show the mismatch. Its actual shader/material/lighting root cause is not yet proven. | HOUSE-03631: investigate the existing foliage lighting path; no new rendering subsystem. |
| Z-L3 / C3, L3_STORE_N/S | S3 | Strong mottled illumination/material pattern on sloped storage walls; shelves, floor edges and navigation remain readable. Day/night views 892–895 and 904–907. | Existing bounded pass only; no bespoke attic polish. |
| Z-STR / C3 | S3, retained | Plain road foreground and repetitive vegetation band already in the zone backlog; current day road views reconfirm it. | Existing bounded-pass severity rules, not a new asset quota. |
| Z-B1, Z-L0M, Z-L0S, Z-L2, Z-GAR, Z-STAIR | — | No additional S1/S2 identified in the inspected room views. Supplements confirm low bathroom/laundry fixtures missed by level pans; utility pipe runs are intentionally high. Circulation and doors remain visible at scheduled night. | Not a substitute for final platform validation or owner usability acceptance. |

No S4 work is scheduled. No scoreboard tier is silently raised, lowered or
waived. Finding a defect does not invalidate the transport evidence; transport
PASS does not dismiss the defect. The fixed-image older G1–G5 evidence is not
used to waive these current findings.

## Next work

The existing MUST bounded fix pass HOUSE-03631 owns these findings. The S1 shed
roof is fix-now under R1, even while final strict acceptance is blocked. Complete
the correction and its geometry/winding regression plus actual interior/exterior
GPU evidence, then the S2 findings, respecting R6/R12. Do not change goldens
until HOUSE-02713 authorizes the representative intentional changes.
