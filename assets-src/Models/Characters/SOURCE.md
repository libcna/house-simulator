# `assets-src/Models/Characters/` — provenance

## Assets

| Manifest id | File | Upstream name / version |
|---|---|---|
| `MODEL_HUMAN_FEMALE_BASE_RESEARCH` | `human_female_base.glb` | MPFB 2.0.17 core basemesh and macro targets, female phenotype |
| `MODEL_HUMAN_MALE_BASE_RESEARCH` | `human_male_base.glb` | MPFB 2.0.17 core basemesh and macro targets, male phenotype |

## Source

| | |
|---|---|
| Publisher / author | MakeHuman Community / Joel Palmius and MakeHuman contributors |
| Canonical URL | <https://extensions.blender.org/add-ons/mpfb/> |
| Retrieved | 2026-09-13 |
| Upstream release / tag / version | MPFB 2.0.17, build 20260722; official extension ZIP SHA-256 `4f0a879d64a39bf646fbf5f53601ac678855da329d650617dca5737548239a87` |
| Second source cross-checked? | <https://github.com/makehumancommunity/mpfb2/releases/tag/v2.0.17> identifies the same 2.0.17 release and build date |

## Licence

| | |
|---|---|
| Licence | CC0-1.0 for the bundled core assets and derivatives |
| Archived at | `licenses/cc0-1.0/LICENCE.txt` |
| How it was established | The current MakeHuman licence page explicitly places all core assets and their derivatives under CC0; the narrower superseded wording and why it does not govern MPFB are recorded in `docs/licence-evidence/makehuman-mpfb2.md` |
| Redistribute source | yes |
| Redistribute derived / compiled | yes |
| Commercial use | yes |
| Modification | yes |
| Attribution required | none |
| Other obligations | none for the mesh assets; MPFB's GPL applies to the tool code, which is not shipped here |

## Modification

| | |
|---|---|
| Modified from upstream? | **yes** |
| If yes, what and why | The selected macro targets are baked, helper and joint geometry is removed, and one Blender `UNSUBDIV` pass reduces the body surface from 26,756 to 13,540 triangles while retaining the silhouette and deformation-oriented edge flow. Heights are solved through the MPFB height macro rather than object scaling. |
| Conversion history | MPFB 2.0.17 core basemesh → average 25-year-old phenotype in neutral A-pose → helpers removed → macro shape keys baked → one unsubdivide iteration → Blender glTF 2.0 exporter, GLB, metres and Y-up |

## Core/community boundary

The two files use only the basemesh and macro targets bundled in the official MPFB extension.
No community asset, skin, eye, hair, clothing, pose, rig or proxy was loaded. The official
`makehuman_system_assets_cc0.zip` pack (SHA-256
`b542127a8e25547c7c29c19f2d1d2adb9a664c80396ecd694095dbc8028a0107`) was downloaded only to
evaluate its six core topology alternatives. None contributes bytes to either GLB.

## Reproduction and review

`tools/assets/generate_human_bases.py` pins MPFB 2.0.17/build 20260722 and reproduces both GLBs,
the measurement report and the solid/wireframe review sheets. The sign-off and rejected topology
alternatives are in `docs/asset-selection/human-base-mesh-research.md`. These unrigged research
meshes are intentionally `notPackaged`; `HOUSE-02131` owns the 32-bone game rig, weights,
animation sidecar and runtime content build.
