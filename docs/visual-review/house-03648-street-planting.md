# HOUSE-03648 — street planting

The release [street view](house-03648/before-street-day.webp) had a nearly level band of repeated
leaves across the foreground. The source was 121 overlapping `VEG_HEDGE_FAR` instances behind the
far sidewalk. The 34 street trees were also all the same sapling geometry despite jittered poses.

All 121 one-metre hedge sections and their collision remain at the same positions. A small number
of existing sections now make full-height crowns every 5–7 m; the intervening sections provide a
continuous lower layer. Four trees in the central arrival use the already licensed young
jacaranda geometry at the same centres; 30 retain the sapling. The two growth stages share the
existing three material roles. No new runtime asset or renderer path was added. The young geometry
adds one measured Reach-cap chunk in `EXT_FRONTYARD_E` (15 → 16), recorded in its explicit budget
exception; no other exterior chunk exception rises.

The matched [before](house-03648/before-street-day.webp) and
[after](house-03648/after-street-day.webp) fixed views show varied crown height and distinguishable
hedge groups. The [larger after view](house-03648/after-street-detail.webp) confirms the lower
planting layer still covers the road-edge boundary. The road foreground remains a broad paved
surface; this task corrects the released S3 planting item, not the separate road-surface backlog.
