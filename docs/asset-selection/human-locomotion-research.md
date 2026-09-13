# Human locomotion research — CMU retarget

`HOUSE-00295` tested six locomotion captures on the female base selected by `HOUSE-00294`.
The result is a **technical rejection**: the sources and licence are usable, but the tested
31-bone CMU-to-MPFB retarget tears the skinned surface and cannot become shipped animation.
`HOUSE-02219` therefore owns the already-budgeted hand-authored fallback on the final 32-bone
game rig.

## Fixed inputs

The source files were fetched individually to `/home/robertvokac/deps/cmu-mocap/91`; none is
committed. This follows the redistribution decision in
[`cmu-mocap.md`](../licence-evidence/cmu-mocap.md). The common subject skeleton is
`91.asf`, SHA-256
`de06a1ee5d917e4bd23461e22e49b43591ed8d9bd84a92d6b6927f423a972b30`.

| Trial | CMU description | AMC frames | AMC SHA-256 | MotionBuilder BVH SHA-256 |
|---:|---|---:|---|---|
| 91_02 | WalkStraight | 1,748 | `e9fb90488a44dcd2d2480022e320bd75e5cc71edcb7dc48cf316aec10e6c9074` | `c1efc11d7112d411c46e56280272062c6ecf7278652e13c553254c1ec49a8e2b` |
| 91_10 | SlowWalk | 3,175 | `1067d0edf7e662c1d497251b6b2319484f1c652cd057e5e183d68f316d1d70ce` | `ed74f3531857f24e9b8e4010d08e975d6e476ed87459c021d1ab82cf839f738a` |
| 91_17 | QuickWalk | 1,520 | `d596f0bc01df894601299a88986da05e4db7edd15887eef79e51787bf60b646f` | `12ac9a8f25c99c8d38b97523770428fdf22d3e365d95e739e2f29f07b94910b1` |
| 91_22 | CasualQuickWalk | 2,106 | `5de00b5265c232496cec489869d8b0097e1fb6e32c4be377363174cf971b950c` | `d61d2e9a2845441778935dfbfd67a488515a8e828f25f050eee694ab453e2a5f` |
| 91_29 | NormalWalk | 2,181 | `a1dd0136f60022c7f6189fc04c3591fb251b691c5edaf7337f4daddea6ae53c2` | `378688b1619a87fcdd04d7cf8f5c04273c5057917f0c5f8211c788eec447b89d` |
| 91_31 | NormalWalk | 1,992 | `b7878cddc6ac028a07c1c97f0fb2df8c160736cc62a89f1e4e14bb20ce0d2482` | `cf3a60d33345b22efa1008e6cda1614724598fa9bf9a5f7ab88674e6d6d7027d` |

The original files are at `http://mocap.cs.cmu.edu/subjects/91/`. The retarget inputs are Bruce
Hahn's MotionBuilder-friendly BVH conversion, mirrored by
<https://github.com/una-dinosauria/cmu-mocap> at revision
`09a07f54f3bbb58797325f009282d0b2048a2871`. Each BVH has one added calibration T-pose frame,
then the original 120 Hz capture. Both forms are independently hash-checked before Blender starts.

The required acknowledgement remains:

> The data used in this project was obtained from mocap.cs.cmu.edu. The database was created with
> funding from NSF EIA-0196217.

## Reproducible experiment

`tools/assets/retarget_cmu_locomotion.py` requires Blender 4.3 and MPFB 2.0.17 build 20260722.
It generates the selected female body, adds MPFB's 31-bone `cmu_mb` research rig, limits and
normalises the skin to four weights per vertex, and keeps the selected 13,540-triangle topology.
For every trial it samples a centred 4-second window from 120 Hz to 30 Hz, removes horizontal root
travel, and retains vertical motion.

The BVHs are Y-up. The tool transforms the import object's axis conversion into target-armature
space, calibrates every bone against the added T-pose, and transfers global orientations onto the
MPFB bind pose. This is deliberately stricter than copying Euler channels by name: direct channel
copy and source-roll substitution were also tested and failed more severely.

Run from the repository root:

```bash
BLENDER_USER_CONFIG=/tmp/house-blender-config \
BLENDER_USER_RESOURCES=/tmp/house-blender-resources \
taskset -c 0-3 blender --background \
  --python tools/assets/retarget_cmu_locomotion.py -- \
  --source-dir /home/robertvokac/deps/cmu-mocap/91 \
  --bvh-dir /home/robertvokac/deps/cmu-mocap/91-bvh \
  --output /tmp/house-00295-cmu-retarget-research.glb \
  --review-dir docs/asset-review/human-locomotion \
  --report docs/asset-review/human-locomotion/measurements.json
```

The temporary GLB proves that six named actions were baked and exportable, but it is deliberately
not committed and no `.chanim` is extracted from it. Structurally valid animation that visibly
tears a character is not a usable asset.

## Quality result

The clean [`rest_pose.png`](../asset-review/human-locomotion/rest_pose.png) proves that the selected
mesh, four-weight reduction, topology reduction and MPFB bind pose are sound before animation.
Each trial's four-frame sheet then shows the same release-blocking failure:

| Trial | Review | Verdict |
|---:|---|---|
| 91_02 | [`walk_straight.png`](../asset-review/human-locomotion/walk_straight.png) | Reject — shoulder, pelvis and extremity tearing |
| 91_10 | [`walk_slow.png`](../asset-review/human-locomotion/walk_slow.png) | Reject — same systemic deformation |
| 91_17 | [`walk_quick.png`](../asset-review/human-locomotion/walk_quick.png) | Reject — same systemic deformation |
| 91_22 | [`walk_casual_quick.png`](../asset-review/human-locomotion/walk_casual_quick.png) | Reject — same systemic deformation |
| 91_29 | [`walk_normal_a.png`](../asset-review/human-locomotion/walk_normal_a.png) | Reject — same systemic deformation |
| 91_31 | [`walk_normal_b.png`](../asset-review/human-locomotion/walk_normal_b.png) | Reject — same systemic deformation |

This is not a clip-selection defect: six higher-numbered takes covering slow, normal, quick and
casual walking fail at the same bind boundary. Loop trimming, toe smoothing and stride measurement
would only polish motion after that boundary and cannot repair it.

## Decision

R-05's licence concern is retired by `HOUSE-00270`: CMU derivatives may be included in a product,
although raw data stays outside the repository. The separate quality experiment failed. The
project therefore does not ship these retargets and triggers the existing fallback: author the 18
player clips directly on the final 32-bone game rig in `HOUSE-02219`. That avoids carrying a
temporary 31-bone skeleton into `HOUSE-02131` and makes the final bind, clothing compatibility and
deformation review happen on the skeleton actually used at runtime.
