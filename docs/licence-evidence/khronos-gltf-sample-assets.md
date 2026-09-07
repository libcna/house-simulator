# Khronos glTF-Sample-Assets — licence evidence

`HOUSE-00264`. The reference corpus this project's glTF tooling is tested against, and a possible
source of a few real props.

| | |
|---|---|
| Repository | <https://github.com/KhronosGroup/glTF-Sample-Assets> |
| Licence index read | `Models/Models.md` and `Models/Models-issues.md` (raw, `main`) |
| Retrieved | 2026-09-07 |
| **Verdict** | **APPROVED per model**, with a named exclusion list. There is **no repository-wide licence**. |

---

## There is no single licence, and the README says so

> A summary of the model license is shown in each display, but see the `README.md` in each model's
> directory for detailed license information.

Each model carries its own copyright and licence, contributed by a different party — Wayfair,
Cesium, Microsoft, individual artists. Treating "the Khronos samples" as one licensed body would be
the aggregator error ADR-0012 exists to prevent.

## Measured distribution

Counted from the licence links in `Models/Models.md`: **148 models, 163 licence links** (some models
have several, one per contributor).

| Licence | Links | Usable here? |
|---|---|---|
| Creative Commons Zero v1.0 Universal (CC0) | **93** | yes, preferred |
| Creative Commons Attribution 4.0 International (CC BY) | **70** | yes — attribution is generated from the manifest |
| anything NC, ND, or proprietary | **0** | — none found |

That is a genuinely clean corpus: no non-commercial and no no-derivatives terms anywhere in the
list, so nothing here is barred by ADR-0012's two standing restrictions. LOD generation and format
conversion are permitted throughout.

## The exclusion that matters: the `#issues` list

The repository maintains `Models/Models-issues.md` — "Models with one or more issues **with respect
to ownership, license, or markings**". Seven models are on it:

```
AntiqueCamera  BoxTextured  BoxTexturedNonPowerOfTwo  CesiumMan
CesiumMilkTruck  PrimitiveModeNormalsTest  RecursiveSkeletons
```

**These must not be shipped**, and the list is not a formality: `CesiumMan` and `RecursiveSkeletons`
are precisely the skinned, animated models a character pipeline reaches for first, and
`BoxTextured` is the obvious choice for a texture smoke test. A future session picking a
"convenient standard model" would land on one of them without this note.

They remain usable as **local test fixtures** — nothing is redistributed by validating against a
file — but no `#issues` model may enter `assets-src/`, because that directory is published.

## Conditions on use

1. The licence is read from the **individual model's `README.md`**, not from the repository, and
   archived in that directory's `SOURCE.md` with the model name, the commit or tag, and the date.
2. CC BY models require attribution; this project generates it from the manifest, so they are usable
   without ceremony. The attribution string must be the model's own credit line verbatim — several
   name multiple contributors.
3. No `#issues` model ships.
4. These are *sample* assets, authored to exercise glTF features rather than to furnish a house. The
   quality bar applies: most are not suitable as furniture, and none is suitable as a hero asset.
