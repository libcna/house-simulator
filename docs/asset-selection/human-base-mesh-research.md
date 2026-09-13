# Human base-mesh research and sign-off

`HOUSE-00294`, retrieved and generated 2026-09-13. R-04 asks whether MakeHuman/MPFB can provide
two legally clean, convincing human bases. The fixed bar is `cna-house.md` §19.3, §21.5, §70.5
and §72: a female body near 1.66 m, a male body near 1.78 m, plausible adult silhouettes, topology
suitable for later skinning, no more than 22,000 LOD0 triangles, and provenance that permits the
source and derived files to remain in this public repository.

## Reproducible source

The selected source is the official Blender Extensions build of **MPFB 2.0.17**, build
`20260722`, downloaded from <https://extensions.blender.org/add-ons/mpfb/>. Its ZIP matched the
publisher's SHA-256
`4f0a879d64a39bf646fbf5f53601ac678855da329d650617dca5737548239a87`. The current extension page
requires Blender 4.2 or newer; generation used Blender 4.3.2.

Both bodies use only MPFB's bundled basemesh and bundled macro targets. No community asset, skin,
eye, hair, clothing, pose, rig or proxy was loaded. The phenotype is the fully female or fully male
25-year-old endpoint with average muscle, weight and proportions and an equal three-way ethnicity
blend. The height macro is solved to the target; no object-scale correction is applied. The exact
recipe and version refusal are executable in `tools/assets/generate_human_bases.py`, and the
machine-readable result is in
[`measurements.json`](../asset-review/human-bases/measurements.json).

The legal result from `HOUSE-00269` therefore applies without a community-asset caveat: core
assets and their derivatives are CC0-1.0. Full provenance is beside the models in
`assets-src/Models/Characters/SOURCE.md`.

## Topology candidates

The raw MPFB surface is visually good and all-quad after helper removal, but it has 13,378 quads,
or **26,756 render triangles**, so it fails the 22,000-triangle budget. The official 267 MB
`makehuman_system_assets_cc0.zip` pack was inspected rather than assumed to solve that:

| Candidate | Vertices | Render triangles | Finding |
|---|---:|---:|---|
| MPFB body surface, helpers removed | 13,380 | 26,756 | Clean source edge flow, but 21.6% over budget |
| `female_generic` / `male_generic` core proxies | 13,868 / 13,652 | 27,732 / 27,300 | Clean quads, but both are farther over budget than the basemesh |
| `female_muscle_13442` / `male_muscle_13290` | 13,514 / 13,290 | 27,024 / 26,576 | Over budget and the muscular silhouette is not the requested average base |
| `female1605` / `male1591` | 1,605 / 1,591 | 3,168 / 3,140 | Under budget, but visibly angular at the face, shoulders, breasts, hands and feet; unsuitable at third-person conversation distance |
| `proxy741` | 741 | 1,460 | Distant-crowd topology, not a hero body |
| **Selected: basemesh + one unsubdivide pass** | **6,772** | **13,540** | Under budget with 8,460 triangles of headroom for later rigging adjustments |

The selected result is not a ratio-driven collapse. Blender's `UNSUBDIV` mode removes one level
from the regular quad structure, retaining the existing loops around the shoulders, elbows, hips,
knees, face, hands and feet. Each body has 6,516 quads and 508 triangles, one UV layer, zero open
boundary edges and zero non-manifold edges. The later rig task must still test deformation and
weights; this sign-off establishes that the neutral topology is a sound input, not that an
uncreated rig has already passed animation.

## Measured result

| Body | Height | A-pose bounds (Blender X/Y/Z) | Vertices | Triangles | Manifold |
|---|---:|---:|---:|---:|---|
| Female | 1.660067 m | 1.013159 × 0.400624 × 1.660067 m | 6,772 | 13,540 | yes; 0 boundary/non-manifold edges |
| Male | 1.779991 m | 1.161882 × 0.489377 × 1.779991 m | 6,772 | 13,540 | yes; 0 boundary/non-manifold edges |

Both GLBs pass `gltf_validate.py`, including the real CNA content importer with warnings treated as
errors. They are intentionally unrigged, untextured research inputs and are marked `notPackaged`
in the asset manifest. `HOUSE-02131` owns the 32-bone rig, four-influence weights, `.chanim`
sidecar and runtime build; hair, face variants, skin textures and clothes remain the bounded work
in phase 36.

## Four-view review

### Female

[Solid silhouette](../asset-review/human-bases/human_female_silhouette.png) ·
[topology overlay](../asset-review/human-bases/human_female_topology.png)

The front/back outline reads as an average adult rather than a stylised fashion figure. The side
view has a plausible head, spine, pelvis, knee and foot profile. The single unsubdivide pass does
not introduce a visible corner or flat spot at this review distance.

### Male

[Solid silhouette](../asset-review/human-bases/human_male_silhouette.png) ·
[topology overlay](../asset-review/human-bases/human_male_topology.png)

The shoulder, torso, pelvis and limb proportions read as an average adult male without bodybuilder
exaggeration. Head, hands and feet retain enough density for the later third-person camera and
clothing silhouettes.

## Decision

**R-04 is retired; its hand-modelled fallback is not triggered.** The two generated GLBs satisfy
the provenance, height, neutral silhouette, manifold/UV and triangle-budget parts of the bar and
are approved as the avatar base inputs. Rigging and deformation remain explicit downstream
acceptance work rather than being silently claimed here.
