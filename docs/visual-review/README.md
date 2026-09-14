# Visual review ledger

These captures answer “does the playable house look good?”, not “did a pixel change?”. Run
`python3 tools/visual/capture_review.py <short-head>-<round>` from the repository root. The tool
fixes the six player poses, Tier S/High, software Mesa, clear weather, 10:30, the session seed and
capture frame. Strict golden render tests remain separate.

## Round 0 — visual-convergence baseline

Commit: `ff788720` (`2026-09-14`)

Capture: [`captures/ff788720-before`](captures/ff788720-before)

Ranked defects:

1. Normal gameplay hashes material ids into saturated flat colours; architecture is cyan/green,
   brown, pink and blue instead of using the authored material textures.
2. No baked lighting reaches the shell, so rooms have no daylight gradient, fixture pools, bounce
   or useful depth.
3. Foyer, hall, living room, family room and kitchen are visibly empty.
4. Exterior shell, drive and ground read as test geometry rather than a coherent front approach.
5. Trim, frames and glass are geometry, but their debug colours keep them from reading as finishes.

Fixed in this round: baseline only.

Remaining: all five defects. `VISUAL-GATE-1` has not passed.
