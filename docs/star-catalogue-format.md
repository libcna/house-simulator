# `stars.bin` — the naked-eye star catalogue

*`HOUSE-01609` writer; the runtime reader belongs to `HOUSE-01610`. Normative. The only writer is
`tools/world/build_stars.py`.*

## 1. Source and selection

The source is NASA HEASARC's BSC5P table, a revised distribution of the Bright Star Catalogue 5th
Edition. `assets-src/world/bsc5p.psv` is the complete 9,110-row export of the five fields this
project uses: HR identifier, J2000 right ascension, J2000 declination, visual magnitude and B−V
colour index. Its exact query, retrieval date, licence evidence and SHA-256 are recorded in
`assets-src/world/SOURCE.md` and `docs/licence-evidence/astronomical.md`.

The writer rejects a changed header, duplicate or missing HR identifiers, malformed coordinates,
non-finite values and a source with any row count other than 9,110. It removes the 14 HR rows that
HEASARC identifies as non-stellar, then removes records missing either visual magnitude or B−V.
The remaining 8,786 complete stars are sorted by `(visual magnitude, HR number)` and the first
1,500 are written. This is the brightest reproducible complete subset: nine brighter source rows
without B−V are deliberately not filled with invented colour. The selected range is Sirius at
V = −1.46 through the deterministic V = 4.94 tie boundary, within the authored 5.5 full-dark
magnitude limit.

## 2. Encoding

All integers are unsigned little-endian and all floats are IEEE-754 binary32. The file is packed
without padding and accepts no trailing bytes. Angles are J2000 degrees; right ascension is in
`[0, 360)` and declination in `[-90, 90]`.

| Field | Type | Value |
|---|---:|---|
| `magic` | 4 bytes | ASCII `CSTR` |
| `version` | `u32` | 1 |
| `flags` | `u32` | 0; unknown bits are rejected |
| `starCount` | `u32` | 1,500 |
| `stars[].raDeg` | `f32` | J2000 right ascension, degrees |
| `stars[].decDeg` | `f32` | J2000 declination, degrees |
| `stars[].visualMagnitude` | `f32` | Johnson V magnitude; rows are brightest-first |
| `stars[].bvColourIndex` | `f32` | Johnson B−V colour index |

The header is 16 bytes and each record is 16 bytes, so version 1 is exactly 24,016 bytes. HR
identifiers are build-time provenance and tie breakers, not runtime rendering data, and therefore
do not occupy the binary.

## 3. Build and verification

`tools/world/build_stars.py --emit` reads only the committed source and writes
`content/world/stars.bin`. The `stars` stage in `tools/ci/build_content.py` performs this generation;
the preceding `star-catalogue` validator pins both source and generated SHA-256. Normal builds and
CI do not use the network. `--fetch` is an explicit maintenance operation that queries only the
recorded HEASARC endpoint, refuses any response whose hash differs from the reviewed snapshot, and
also requires byte identity with that snapshot.

The self-test freezes the row and byte counts, magnitude bounds, Sirius/Canopus ordering, Polaris
presence, known coordinates and colours for four named stars, deterministic packing, exact binary
round-trip, and rejection of wrong magic, version, flags, count, truncation and trailing data.
