# `sky_dome.bin` — the dynamic-colour sky geometry

*`HOUSE-01642` writer and `HOUSE-01643` runtime reader. Normative. The only writer is
`tools/world/build_skydome.py`; `rendering::SkyDomeReader` is the only runtime reader.*

## 1. Geometry

The hemisphere has 32 longitude segments and 18 latitude segments at §31.1's 900 m radius. It is
an indexed mesh with one pole and 18 rings of 32 vertices: there is no duplicated pole and the
longitude seam wraps by index. The horizon ring continues down to a second ring 90 m below it and
that ring closes as a triangle fan. This sloping skirt plus ground disc prevents a crack beneath
the horizon; terrain and buildings overwrite it because §31.1 draws the sky first without depth
testing or writes.

The resulting mesh is 610 vertices and 1,216 triangles: 577 hemisphere vertices, 32 skirt-ring
vertices and one disc centre. All 1,216 triangles have positive area. The earlier §31.2 estimate
of 594 vertices was `33 × 18`: it duplicated every seam vertex, still omitted the skirt, and a
naive pole row would add 32 zero-area triangles. Measured generated topology replaces that stale
arithmetic.

Only positions are stored. `SkySystem` creates XNA `VertexPositionColor` vertices, initially in a
visible bootstrap blue; `HOUSE-01644` computes their changing colours from the LUT. Baking a colour
into this file would duplicate the LUT and be stale as soon as the sun moves.

## 2. Encoding

All integers are unsigned little-endian; all floats are IEEE-754 binary32. The file is packed with
no padding and accepts no trailing bytes.

| Field | Type | Value |
|---|---:|---|
| `magic` | 4 bytes | ASCII `CSKY` |
| `version` | `u32` | 1 |
| `flags` | `u32` | 0; unknown bits are rejected |
| `longitudeSegments` | `u32` | 32 |
| `radius` | `f32` | 900 m |
| `skirtDepth` | `f32` | 90 m |
| `latitudeSegments` | `u32` | 18 |
| `domeVertexCount` | `u32` | 577; vertices before this index are on the hemisphere |
| `vertexCount` | `u32` | 610 |
| `indexCount` | `u32` | 3,648 |
| `vertices` | `vertexCount × 3 × f32` | position XYZ, metres, right-handed Y-up |
| `indices` | `indexCount × u16` | triangle list; every index is below `vertexCount` |

The radius and skirt depth travel in the header so a reader never has to repeat generator
constants to decide which vertices belong to the dome and how altitude fractions clamp on the
skirt.

## 3. Build and verification

`sky-dome` is a world stage in `tools/ci/build_content.py`, after the `sky-lut` validator, and
writes `content/world/sky_dome.bin`. `tools/world/build_skydome.py --selftest` asserts the exact
topology, bounds, sphere radius, index range, absence of duplicate or degenerate vertices and
triangles, byte determinism, round trip, and rejection of damaged headers and lengths.

At runtime `SkyDomeReader` rejects any other length, magic, version, flags, dimensions, radius,
skirt depth or counts before allocating the arrays, then rejects non-finite positions and indices
outside the 610-vertex table. `SkySystem` uploads the resulting 610 vertices and 3,648 `u16`
indices once, draws 1,216 triangles with opaque blending, `DepthStencilState::None` and
`RasterizerState::CullNone`, and translates the object by the current camera eye every draw. It is
also the single `Pass::Sky` owner and draws the existing additive sun disc after the dome; the
renderer intentionally permits only one implementation per pass.
