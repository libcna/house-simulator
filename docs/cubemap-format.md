# The mirror cube maps

*`HOUSE-00209`. Normative. The writer is `tools/blender/cubemap_bake.py`; there is no other. The
runtime consumer is `HOUSE-00896`'s `MaterialBinder` for `EnvironmentMapEffect`, and
`HOUSE-02684`'s mirror material.*

---

## 1. What these are for

`cna-house.md` §59: *"Mirrors: 4 of them. Implementation is a static cube map per mirror baked
offline (`EnvironmentMapEffect`), which is correct for a fixed mirror in a fixed room and costs
nothing. The player's reflection is not rendered — a deliberate, documented limitation (D-19, §77)
— because a real planar reflection would need a second render pass per mirror per frame and the
budget is better spent elsewhere."*

## 2. The files

Six PNGs per mirror plus one shared sidecar:

```
<MIRROR_ID>_CUBE_px.png   <MIRROR_ID>_CUBE_nx.png
<MIRROR_ID>_CUBE_py.png   <MIRROR_ID>_CUBE_ny.png
<MIRROR_ID>_CUBE_pz.png   <MIRROR_ID>_CUBE_nz.png
cubemaps.json
```

The sidecar records, per mirror: the id, the cell, the point it was baked from, the face size, the
file names, the **face order** (`px nx py ny pz nz` — Direct3D's, which is what `TextureCube`
indexes by, so a reader never has to infer it from a filename), and which objects were hidden.

Faces are sRGB colour, not `Non-Color`. This is the opposite decision from
[`lightmap_bake.py`](../tools/blender/lightmap_bake.py) and it is right for the opposite reason: a
lightmap is data and a reflection is a picture. Both share the `Standard` view transform, because
Blender 4.x's AgX default would tone-map the result out of step with the room XNA draws beside it.

Four mirrors at 256² are **4.5 MB** uncompressed, 1.1 MB as DXT1.

## 3. The face convention: half measured, half asserted

**Measured, against CNA.** `HOUSE-00081` (`docs/cna-capability-report.md`) put six distinctly
coloured faces in a `TextureCube` and read back which one a known reflection vector sampled:
`(0,0,1)` → `+Z`, `(1,0,0)` → `+X`, `(-1,0,0)` → `−X`. So **which face a direction lands on** is
known, and this tool renders to that mapping.

**Asserted, not measured.** The orientation *within* a face — which way is up, which way is right —
follows Direct3D's convention, which XNA inherits. Nothing here has read it back out of CNA.

**Why that matters.** A face rendered upside down or mirrored still reflects the right room and
looks entirely plausible; it fails only when someone reads a sign or a clock in it.

**What is checked instead.** Adjacent faces are rendered from the same point and share an edge, so
the rays along that edge are the same rays. The selftest verifies that all twelve adjacent pairs
have an edge whose pixels look in identical directions. **Seam continuity catches every per-face
error** — one face rotated, one flipped, one up-vector wrong. What it cannot catch is a single
*global* handedness flip applied consistently to all six.

> **Open, for the first mirror placed:** a `HOUSE-00081`-style probe of the in-face orientation.
> Put an asymmetric marker in a baked cube map, sample it through `EnvironmentMapEffect`, and
> confirm the marker appears on the side this tool puts it. Until then §4's flip is reasoning, not
> measurement.

## 4. The handedness flip

Direct3D's cube faces are described in a **left-handed** space; `cna-house.md` §14's world is
right-handed; and a Blender camera basis can only be right-handed. The consequence is exact and the
selftest asserts it face by face: **the camera's right vector is the negation of D3D's, on all six
faces.**

So every rendered face is **mirrored horizontally on write**. Without it, every reflection in the
house is inside out.

The same write also makes the output **byte-identical between runs**, which matters for a second
reason: `bpy.ops.render.render(write_still=True)` stamps metadata into the PNG, so two
pixel-identical bakes produce different files. `HOUSE-00199` rebuilds everything and asserts
byte-identical output, and a stamped file would fail that gate for a render that never changed.
Re-saving through `image.save()` writes no such chunk.

## 5. Field of view

**90°, and no other value works.** A face's half-extent at unit distance is `tan(fov/2)`, so
`tan 45° = 1` *is* the condition that six faces close a cube. Narrower leaves a wedge of the room in
no face at all; wider puts the same wedge in two and the seam shows as a doubled reflection.

## 6. The mirror hides itself

A camera at the mirror's own position that can see the mirror bakes the mirror's back into its own
reflection. Every object named in the mirror's `hide` list is hidden for its bake, and the mirror's
own prop and id are added to that list automatically — then **unhidden afterwards**, so a second
mirror's bake is not made in a room the first one emptied.
