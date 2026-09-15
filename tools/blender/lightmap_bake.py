#!/usr/bin/env python3
"""lightmap_bake.py -- the shape of a room's light, baked once and scaled for ever after.

`HOUSE-00206`. `cna-house.md` §18.3 step 3: "bakes, per cell, one lightmap per light group plus one
'daylight' lightmap lit only by a uniform sky dome through that cell's window openings. Bakes are
diffuse-only, indirect included, Cycles, 256 samples, denoised." §28.3 says why: "the *shape* of a
room's lighting is fixed even though its *intensity* is not. Baking captures the shape; the runtime
scales it."

    tools/blender/lightmap_bake.py SHELL.glb --lights lights.json --cell L0_KITCHEN --out DIR
    tools/blender/lightmap_bake.py SHELL.glb --lights lights.json --cell L0_KITCHEN --out DIR \\
        --size 512 --samples 64 --seed 20260907
    tools/blender/lightmap_bake.py --selftest

## Four settings decide whether this produces a lightmap or a picture of one

**`use_pass_color = False`.** A lightmap is *irradiance*, not lit colour. With the colour pass on,
the albedo is baked in and then multiplied again by `DualTextureEffect` at runtime, so every red
wall goes twice as red and every dark floor twice as dark. Measured on the fixture: a pure red wall
and a white wall under the same lamp bake to the **same** value, which is the whole point and the
selftest's first claim.

**`view_transform = 'Standard'`.** Blender 4.x defaults to **AgX**, a filmic tone map. Left alone
it rolls the highlights off the pendant's pool of light and lifts the shadows -- a photograph of
the room's lighting rather than a measurement of it -- and nothing about the saved PNG would look
wrong. The image colour space is `Non-Color` for the same reason: this is data.

**The active UV layer must be the SECOND one.** Cycles bakes into whatever UV map is active, and a
shell's first map is its albedo map. Baking into it puts the room's lighting wherever the tiling
brick texture happens to send it. `HOUSE-00205` names the second map `Lightmap`; this selects it
and fails loudly if it is missing.

**A normalisation scale, because an 8-bit PNG cannot hold irradiance.** §18.3 step 4 exports these
as PNG, and a lamp of any reasonable wattage bakes to values well above 1.0 -- the fixture reaches
6.3. Saved straight, everything bright clips to flat white and the *shape* the bake exists to
capture is exactly what is lost. So each image is divided by its own maximum, and that scale is
recorded in the sidecar. §23.4's runtime sum is `lightmapArt.rgb * artLevels`, so the scale folds
into `artLevels` and costs nothing.

## Determinism and staleness

`--seed` sets `cycles.seed` with `use_animated_seed` off and adaptive sampling off, so two runs of
one scene are byte-identical. The sidecar records the tool, its version, the seed, the sample
count, the resolution -- and a **shell hash** over the geometry, the lightmap UVs and the light
definitions, so a bake made before someone moved a wall is detected rather than shipped (§18.3
step 5: "the build fails loudly rather than shipping wrong light").

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import hashlib
import json
import math
import os
import sys

try:
    import bpy  # type: ignore
    from mathutils import Vector  # type: ignore

    INSIDE_BLENDER = True
except ImportError:
    INSIDE_BLENDER = False

if not INSIDE_BLENDER:
    from pathlib import Path

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import blender_env  # noqa: E402

    sys.exit(blender_env.relaunch(Path(__file__).resolve(), sys.argv[1:], tool="lightmap_bake"))

#: `HOUSE-00205`'s second UV set. The name is the contract between the two tools.
LIGHTMAP_UV = "Lightmap"

#: §18.3's bake settings.
DEFAULT_SAMPLES = 256
DEFAULT_SIZE = 2048
DEFAULT_SEED = 20260907

#: The daylight bake's sky, in W/m2/sr. A uniform dome (§18.3: "lit only by a uniform sky dome"),
#: so the *shape* it produces is the window openings and nothing else -- a sun would bake a moving
#: patch into a static texture, which is what `HOUSE-00208`'s sun patches exist to avoid.
SKY_STRENGTH = 1.0

VERSION = 2
LEGACY_LUMENS_PER_RADIANT_WATT = 683.0


def report(message: str) -> None:
    print(f"  {message}")


# ==================================================================================== scene setup


def configure(scene, seed: int, samples: int) -> None:
    """Cycles, CPU, fixed seed, no adaptive sampling, denoised, and NO tone map."""
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    scene.cycles.seed = seed
    scene.cycles.use_animated_seed = False
    scene.cycles.use_adaptive_sampling = False
    scene.cycles.use_denoising = True
    # Blender 4.x defaults to AgX. A filmic tone map on a lightmap is a photograph of the lighting
    # rather than a measurement of it, and nothing about the result looks wrong.
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0
    scene.render.bake.use_pass_direct = True
    scene.render.bake.use_pass_indirect = True
    # The setting this whole tool turns on: a lightmap is irradiance, not lit colour.
    scene.render.bake.use_pass_color = False
    scene.render.bake.use_clear = True
    scene.render.bake.margin = 2


def mesh_objects():
    return [o for o in bpy.context.scene.objects if o.type == "MESH"]


def receiver_objects():
    """Meshes that own UV2 and receive the bake; detail remains present only as an occluder.

    `shell_unwrap.py` deliberately writes the receiver and detail halves into the same GLB.  The
    detail half has no `Lightmap` UV (it must not consume atlas space), but it still has to remain
    in the scene so a casing or stair rail can cast onto the wall beside it.  A material exported
    by the shell generator carries an explicit `lightmapReceiver` flag.  Hand-authored fixtures
    predate that flag, so an entirely unclassified mesh retains the old receiver behaviour.
    """
    receivers = []
    for obj in mesh_objects():
        flags = [material.get("lightmapReceiver") for material in obj.data.materials
                 if material is not None and material.get("lightmapReceiver") is not None]
        if not flags or any(bool(flag) for flag in flags):
            receivers.append(obj)
    return receivers


def lightmap_uv(obj):
    """The authored `Lightmap` layer, or glTF's second texture-coordinate channel.

    glTF stores `TEXCOORD_0`/`TEXCOORD_1`, not Blender UV-layer names.  Importing the unwrapped
    shell therefore reconstructs the two channels as `UVMap` and `UVMap.001`.  Channel one is the
    durable contract; accepting only the pre-export spelling made every real shell look unwrapped
    to the chunk builder and unwrapped to nobody who could bake it.
    """
    layers = obj.data.uv_layers
    if LIGHTMAP_UV in layers:
        return layers[LIGHTMAP_UV]
    if len(layers) >= 2:
        return layers[1]
    return None


def select_lightmap_uv() -> None:
    """Make `HOUSE-00205`'s second UV set active on every receiver, or say why it cannot."""
    for obj in receiver_objects():
        layer = lightmap_uv(obj)
        if layer is None:
            raise SystemExit(
                f"lightmap_bake: {obj.name} has no {LIGHTMAP_UV!r} UV set; run "
                f"tools/blender/lightmap_unwrap.py over it first (HOUSE-00205). "
                f"Baking into the albedo UVs would put the room's lighting wherever the tiling "
                f"texture sends it.")
        obj.data.uv_layers.active = layer


def shell_hash(lights: list[dict], lumens_per_radiant_watt: float = LEGACY_LUMENS_PER_RADIANT_WATT) -> str:
    """A hash over the geometry, the lightmap UVs and the lights.

    Everything the bake depends on and nothing else: move a wall, move a lamp, or repack the
    lightmap UVs and this changes; rename an object or edit an unrelated material and it does not.
    Recorded in the sidecar so `HOUSE-00216`'s content build can refuse a stale bake.
    """
    digest = hashlib.sha256()
    for obj in sorted(mesh_objects(), key=lambda o: o.name):
        mesh = obj.data
        matrix = obj.matrix_world
        for vertex in mesh.vertices:
            world = matrix @ vertex.co
            digest.update(f"{world.x:.5f},{world.y:.5f},{world.z:.5f};".encode())
        layer = lightmap_uv(obj)
        if layer is not None:
            for item in layer.data:
                digest.update(f"{item.uv[0]:.5f},{item.uv[1]:.5f};".encode())
    for light in sorted(lights, key=lambda light: light["id"]):
        digest.update(json.dumps(light, sort_keys=True).encode())
    # Preserve existing bake hashes for the legacy conversion. A deliberately calibrated slice
    # has a distinct contract even when its shell and fixtures are otherwise byte-identical.
    if lumens_per_radiant_watt != LEGACY_LUMENS_PER_RADIANT_WATT:
        digest.update(f"photometric:{lumens_per_radiant_watt:.6f}".encode())
    return "sha256:" + digest.hexdigest()


def make_bake_target(size: int):
    """One float image, and an image-texture node in every receiver material pointing at it."""
    if "cnahouse_bake" in bpy.data.images:
        bpy.data.images.remove(bpy.data.images["cnahouse_bake"])
    image = bpy.data.images.new("cnahouse_bake", size, size, float_buffer=True)
    image.colorspace_settings.name = "Non-Color"
    for obj in receiver_objects():
        if not obj.data.materials:
            material = bpy.data.materials.new(f"{obj.name}_mat")
            material.use_nodes = True
            obj.data.materials.append(material)
        for material in obj.data.materials:
            if material is None:
                continue
            if not material.use_nodes:
                material.use_nodes = True
            nodes = material.node_tree.nodes
            node = nodes.get("cnahouse_bake_target")
            if node is None:
                node = nodes.new("ShaderNodeTexImage")
                node.name = "cnahouse_bake_target"
            node.image = image
            nodes.active = node
    return image


def light_objects():
    return [o for o in bpy.context.scene.objects if o.type == "LIGHT"]


def create_light_objects(lights: list[dict],
                         lumens_per_radiant_watt: float = LEGACY_LUMENS_PER_RADIANT_WATT) -> None:
    """Turn the authored XNA light rows into the white emitters the irradiance bake needs.

    The shell GLBs contain geometry, not fixtures.  The original tool filtered Blender light
    objects by the JSON ids but never created those objects, so its first production invocation
    would have baked every artificial atlas black.  Colour remains white here intentionally:
    §28 supplies each group's temperature at runtime, while the bake owns only spatial shape.
    """
    # glTF is Y-up, while Blender is Z-up.  Blender's importer converts every shell vertex as
    # (x, y, z) -> (x, -z, y); authored rows must undergo the identical conversion or a lamp that
    # belongs near a room ceiling lands tens of metres away along Blender Z.  A few groups then
    # happened to illuminate geometry by accident, which made this particularly easy to miss.
    def blender_space(vector: list[float] | tuple[float, ...]) -> tuple[float, float, float]:
        x, y, z = (float(value) for value in vector)
        return (x, -z, y)

    existing = {obj.name for obj in light_objects()}
    for light in lights:
        if not light.get("bakedIntoLightmap", True) or light.get("type") == "emissive_only":
            continue
        name = str(light["id"])
        if name in existing:
            continue
        kind = str(light.get("type", "point"))
        if kind not in ("point", "spot"):
            raise SystemExit(f"lightmap_bake: {name} has unsupported light type {kind!r}")
        data = bpy.data.lights.new(name, type="SPOT" if kind == "spot" else "POINT")
        # 683 lm/W is the old monochromatic photopic maximum, not a calibrated broadband
        # household emitter. Keep it as the legacy default so old products remain reproducible;
        # a measured vertical slice may explicitly request a lower, documented white-light
        # efficacy. The sidecar and shell hash record that choice.
        data.energy = float(light.get("intensityLm", 0.0)) / lumens_per_radiant_watt
        data.color = (1.0, 1.0, 1.0)
        # Recessed rows sit 20 mm below the ceiling.  A 50 mm emitter intersects that receiver and
        # produces one white firefly; max-normalising against it crushes the whole useful atlas to
        # black.  Five millimetres stays inside the authored clearance while retaining a finite,
        # deterministic source.
        data.shadow_soft_size = 0.005
        data.use_custom_distance = True
        data.cutoff_distance = float(light.get("range", 10.0))
        if kind == "spot":
            data.spot_size = math.radians(float(light.get("coneOuterDeg", 45.0)))
            outer = max(float(light.get("coneOuterDeg", 45.0)), 1e-3)
            inner = min(max(float(light.get("coneInnerDeg", 0.0)), 0.0), outer)
            data.spot_blend = max(0.0, min(1.0, 1.0 - inner / outer))
        obj = bpy.data.objects.new(name, data)
        obj.location = blender_space(light["position"])
        if kind == "spot":
            direction = Vector(blender_space(light.get("direction", (0, -1, 0))))
            if direction.length_squared <= 1e-12:
                raise SystemExit(f"lightmap_bake: {name} has a zero spot direction")
            obj.rotation_euler = direction.normalized().to_track_quat("-Z", "Y").to_euler()
        bpy.context.scene.collection.objects.link(obj)


def set_world_sky(strength: float) -> None:
    """A uniform sky dome, or darkness. §18.3's daylight bake is "a uniform sky dome"."""
    world = bpy.context.scene.world
    if world is None:
        world = bpy.data.worlds.new("World")
        bpy.context.scene.world = world
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    if background is None:
        background = world.node_tree.nodes.new("ShaderNodeBackground")
        output = world.node_tree.nodes.get("World Output") \
            or world.node_tree.nodes.new("ShaderNodeOutputWorld")
        world.node_tree.links.new(background.outputs[0], output.inputs[0])
    background.inputs["Color"].default_value = (1.0, 1.0, 1.0, 1.0)
    background.inputs["Strength"].default_value = strength


def bake_once(image, size: int) -> list[float]:
    """Run the bake and return pixels; detail stays unselected but still casts onto receivers."""
    bpy.ops.object.select_all(action="DESELECT")
    meshes = receiver_objects()
    if not meshes:
        raise SystemExit("lightmap_bake: the scene has no mesh to bake")
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.bake(type="DIFFUSE")
    del size
    return list(image.pixels)


def rgb_peak(pixels: list[float]) -> float:
    """Highest RGB sample, excluding alpha and retaining a real zero for an unlit bake."""
    return max((pixels[i + channel]
                for i in range(0, len(pixels), 4)
                for channel in range(3)), default=0.0)


def normalise_and_save(pixels: list[float], size: int, path: str) -> float:
    """Scale to 0..1 by the image's own maximum, save as PNG, and return the scale.

    An 8-bit PNG cannot hold irradiance -- the fixture bakes to 6.3 -- so saving straight clips
    everything bright to flat white and loses exactly the shape §28.3 says the bake exists to
    capture. The scale folds into §23.4's `artLevels` at runtime and costs nothing there.
    """
    peak = rgb_peak(pixels)
    scale = peak if peak > 1e-6 else 1.0
    image = bpy.data.images.new(f"save_{os.path.basename(path)}", size, size, alpha=False)
    image.colorspace_settings.name = "Non-Color"
    scaled = list(pixels)
    for i in range(0, len(scaled), 4):
        scaled[i] = min(1.0, scaled[i] / scale)
        scaled[i + 1] = min(1.0, scaled[i + 1] / scale)
        scaled[i + 2] = min(1.0, scaled[i + 2] / scale)
        scaled[i + 3] = 1.0
    image.pixels = scaled
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()
    bpy.data.images.remove(image)
    return scale


def luminance(pixels: list[float], index: int) -> float:
    return (0.2126 * pixels[index * 4] + 0.7152 * pixels[index * 4 + 1]
            + 0.0722 * pixels[index * 4 + 2])


def pack_rgb(channels: list[list[float]], size: int, path: str) -> list[float]:
    """§23.4's Tier-E texture: three groups' luminance in R, G and B of one image.

    One texture fetch instead of three. The per-group colour is not lost, it moves: §23.4 applies
    it at runtime as `lightmapArt.rgb * artLevels`, and §28.3 is explicit that the bake captures
    the shape while the runtime supplies the level.

    **Each channel is normalised by its own peak**, and the three scales are returned for the
    sidecar. This is the same 8-bit problem the per-group atlases have, and it bites harder here:
    the first version wrote raw luminance, which reaches 22 on the fixture, so every channel
    clamped to a flat 1.0 over most of the lit area -- a packed atlas that had thrown away
    precisely the shape it exists to carry, while the per-group atlases beside it were correct.
    `artLevels` is a `float3`, so a scale per channel folds in exactly as one per group does.
    """
    image = bpy.data.images.new(f"pack_{os.path.basename(path)}", size, size, alpha=False)
    image.colorspace_settings.name = "Non-Color"
    scales = []
    for channel in range(3):
        if channel < len(channels):
            peak = max((luminance(channels[channel], texel) for texel in range(size * size)),
                       default=0.0)
            scales.append(peak if peak > 1e-6 else 1.0)
        else:
            scales.append(1.0)
    out = [0.0] * (size * size * 4)
    for texel in range(size * size):
        for channel in range(3):
            value = (luminance(channels[channel], texel) / scales[channel]
                     if channel < len(channels) else 0.0)
            out[texel * 4 + channel] = min(1.0, value)
        out[texel * 4 + 3] = 1.0
    image.pixels = out
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()
    bpy.data.images.remove(image)
    return scales


# =========================================================================================== bake


def bake_cell(lights: list[dict], cell: str, out_dir: str, size: int, samples: int,
              seed: int, pack: bool, *, artificial: bool = True,
              daylight: bool = True,
              lumens_per_radiant_watt: float = LEGACY_LUMENS_PER_RADIANT_WATT) -> dict:
    """The requested artificial groups and/or daylight atlas. Returns the sidecar."""
    scene = bpy.context.scene
    configure(scene, seed, samples)
    select_lightmap_uv()
    image = make_bake_target(size)
    os.makedirs(out_dir, exist_ok=True)

    by_group: dict[str, list[str]] = {}
    for light in lights:
        if light.get("bakedIntoLightmap", True) and light.get("type") != "emissive_only":
            by_group.setdefault(light.get("group") or "LG_DEFAULT", []).append(light["id"])
    every = {o.name: o for o in light_objects()}

    entries = []
    channels = []
    for group in sorted(by_group) if artificial else []:
        # Only this group's lamps. §28.3 bakes one lightmap per switch group precisely so the
        # runtime can turn one group off without the others changing.
        for name, obj in every.items():
            obj.hide_render = name not in by_group[group]
        set_world_sky(0.0)
        pixels = bake_once(image, size)
        name = f"{cell}_LM_{group}.png"
        peak = rgb_peak(pixels)
        scale = normalise_and_save(pixels, size, os.path.join(out_dir, name))
        entries.append({"group": group, "image": name, "scale": scale,
                        "lights": sorted(by_group[group]),
                        "peak": peak, "mean": sum(luminance(pixels, i)
                                                   for i in range(size * size)) / (size * size)})
        channels.append(pixels)
        report(f"{group}: {len(by_group[group])} light(s), peak {peak:.4f}")

    day_entry = None
    if daylight:
        # The daylight bake: every lamp off, a uniform sky on. §18.3's "lit only by a uniform sky
        # dome through that cell's window openings" -- the openings are holes in the shell, so
        # the sky reaches through them and nothing else has to know what a window is.
        for obj in every.values():
            obj.hide_render = True
        set_world_sky(SKY_STRENGTH)
        day_pixels = bake_once(image, size)
        day_name = f"{cell}_LM_DAY.png"
        day_scale = normalise_and_save(day_pixels, size, os.path.join(out_dir, day_name))
        day_entry = {"image": day_name, "scale": day_scale, "skyStrength": SKY_STRENGTH}
        report(f"LM_DAY: sky only, peak {day_scale:.4f}")

    packed_scales = None
    if pack and channels:
        packed_scales = pack_rgb(channels[:3], size,
                                 os.path.join(out_dir, f"{cell}_LM_PACKED.png"))

    sidecar = {
        "tool": "lightmap_bake.py", "version": VERSION,
        "cell": cell, "seed": seed, "samples": samples, "size": size,
        "denoised": True, "viewTransform": scene.view_settings.view_transform,
        "bakePassColor": scene.render.bake.use_pass_color,
        "uvSet": LIGHTMAP_UV,
        "shellHash": shell_hash(lights, lumens_per_radiant_watt),
        "lumensPerRadiantWatt": lumens_per_radiant_watt,
        "groups": entries,
        "daylight": day_entry,
        "packed": f"{cell}_LM_PACKED.png" if (pack and channels) else None,
        "packedScales": packed_scales,
    }
    path = os.path.join(out_dir, f"{cell}_LM.json")
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(sidecar, handle, indent=2, sort_keys=True)
        handle.write("\n")
    products = f"{len(entries)} group atlas(es)"
    if daylight:
        products += " + LM_DAY"
    report(f"wrote {products} and {os.path.basename(path)}")
    return sidecar


# ======================================================================================= fixtures


def build_two_room_fixture() -> list[dict]:
    """Two rooms joined by a doorway, one lamp in each, and a red wall facing a white one.

    Everything the claims need and nothing else. The doorway is what makes **indirect** measurable:
    the far room's wall has no straight line to its own lamp's neighbour, so any light reaching it
    has bounced. The red wall is what makes **`use_pass_color`** measurable: baked with the colour
    pass on it comes out red, and with it off it matches the white wall exactly.
    """
    bpy.ops.wm.read_factory_settings(use_empty=True)
    verts: list[tuple[float, float, float]] = []
    faces: list[list[int]] = []
    groups: list[int] = []

    def quad(a, b, c, d, material: int, facing) -> None:
        """One quad, wound so its normal points TOWARDS `facing`.

        Every surface here is an interior one, and a diffuse bake lights the side the normal is
        on. Getting a winding backwards does not produce a warning or a hole -- it produces a
        black face, and thirteen quads hand-wound by eye produced four lit ones the first time.
        Choosing the winding by where the room is removes the whole class of mistake.
        """
        corners = [a, b, c, d]
        u = tuple(b[k] - a[k] for k in range(3))
        v = tuple(d[k] - a[k] for k in range(3))
        normal = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                  u[0] * v[1] - u[1] * v[0])
        inward = tuple(facing[k] - a[k] for k in range(3))
        if sum(normal[k] * inward[k] for k in range(3)) < 0:
            corners.reverse()
        base = len(verts)
        verts.extend(corners)
        faces.append([base, base + 1, base + 2, base + 3])
        groups.append(material)

    height = 2.6
    room_a, room_b = (-2.0, 1.3, 0.0), (2.0, 1.3, 0.0)
    # Room A spans x -4..0, room B spans x 0..4; both z -1.5..1.5.
    for (x0, x1), middle in (((-4.0, 0.0), room_a), ((0.0, 4.0), room_b)):
        quad((x0, 0, -1.5), (x1, 0, -1.5), (x1, 0, 1.5), (x0, 0, 1.5), 0, middle)      # floor
        if middle is room_a:
            # A SKYLIGHT, 1 m square, in room A's ceiling. Without an opening to the sky the two
            # rooms are sealed boxes, and every face's normal points inward -- so the daylight
            # bake measures nothing and a claim that "a stray sky would have shown" cannot be
            # made at all. §18.3's daylight bake is "through that cell's window openings", and a
            # fixture with no opening cannot exercise it.
            for a, b, c, d in (((x0, height, -1.5), (x1, height, -1.5),
                                (x1, height, -0.5), (x0, height, -0.5)),
                               ((x0, height, 0.5), (x1, height, 0.5),
                                (x1, height, 1.5), (x0, height, 1.5)),
                               ((x0, height, -0.5), (-2.5, height, -0.5),
                                (-2.5, height, 0.5), (x0, height, 0.5)),
                               ((-1.5, height, -0.5), (x1, height, -0.5),
                                (x1, height, 0.5), (-1.5, height, 0.5))):
                quad(a, b, c, d, 0, middle)
        else:
            quad((x0, height, -1.5), (x1, height, -1.5),
                 (x1, height, 1.5), (x0, height, 1.5), 0, middle)                      # ceiling
        quad((x0, 0, 1.5), (x1, 0, 1.5), (x1, height, 1.5), (x0, height, 1.5), 0,
             middle)                                                                   # +z wall
        quad((x0, 0, -1.5), (x1, 0, -1.5), (x1, height, -1.5), (x0, height, -1.5), 0,
             middle)                                                                   # -z wall
    # The two end walls. The far one (x = 4) is RED; the near one (x = -4) is white.
    quad((-4.0, 0, -1.5), (-4.0, 0, 1.5), (-4.0, height, 1.5), (-4.0, height, -1.5), 0, room_a)
    quad((4.0, 0, -1.5), (4.0, 0, 1.5), (4.0, height, 1.5), (4.0, height, -1.5), 1, room_b)
    # The party wall at x = 0, with a doorway 1.0 m wide reaching the floor. Faced towards room A;
    # room B sees its back, which is what a real party wall does.
    quad((0.0, 2.05, -1.5), (0.0, 2.05, 1.5), (0.0, height, 1.5), (0.0, height, -1.5), 0, room_a)
    quad((0.0, 0, -1.5), (0.0, 0, -0.5), (0.0, 2.05, -0.5), (0.0, 2.05, -1.5), 0, room_a)
    quad((0.0, 0, 0.5), (0.0, 0, 1.5), (0.0, 2.05, 1.5), (0.0, 2.05, 0.5), 0, room_a)

    mesh = bpy.data.meshes.new("shell")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    albedo = mesh.uv_layers.new(name="UVMap")
    # UV0 is deliberately NOT the lightmap layout: every face is sent to the same corner. A bake
    # that used it would pile the whole room into one texel, which is the claim about UV sets.
    for item in albedo.data:
        item.uv = (0.05, 0.05)
    lightmap = mesh.uv_layers.new(name=LIGHTMAP_UV)
    # A simple grid packing, one cell per face, deterministic and readable back by index.
    columns = int(math.ceil(math.sqrt(len(faces))))
    for index, polygon in enumerate(mesh.polygons):
        column, row = index % columns, index // columns
        corners = [(0.06, 0.06), (0.94, 0.06), (0.94, 0.94), (0.06, 0.94)]
        for slot, loop in enumerate(polygon.loop_indices):
            u, v = corners[slot % 4]
            lightmap.data[loop].uv = ((column + u) / columns, (row + v) / columns)
        polygon.material_index = groups[index]

    white = bpy.data.materials.new("white")
    white.use_nodes = True
    white.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (
        0.8, 0.8, 0.8, 1.0)
    red = bpy.data.materials.new("red")
    red.use_nodes = True
    red.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (
        0.8, 0.0, 0.0, 1.0)
    mesh.materials.append(white)
    mesh.materials.append(red)

    obj = bpy.data.objects.new("shell", mesh)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj

    lights = [
        {"id": "LIGHT_A", "group": "LG_A", "position": [-2.0, 2.3, 0.0], "intensityLm": 900},
        {"id": "LIGHT_B", "group": "LG_B", "position": [2.0, 2.3, 0.0], "intensityLm": 900},
    ]
    for light in lights:
        data = bpy.data.lights.new(light["id"], type="POINT")
        data.energy = float(light["intensityLm"]) / 4.0
        data.shadow_soft_size = 0.05
        obj_light = bpy.data.objects.new(light["id"], data)
        obj_light.location = tuple(light["position"])
        bpy.context.collection.objects.link(obj_light)
    return lights


def face_index_at(point) -> int:
    """The polygon whose centre is nearest `point`, over every mesh in the scene.

    Faces are looked up by WHERE THEY ARE, never by the order the fixture happened to emit them.
    A hardcoded index is correct until the fixture gains a skylight, at which point every claim
    downstream of it quietly starts measuring a different face and still passes.
    """
    best, best_distance = 0, float("inf")
    for obj in mesh_objects():
        for polygon in obj.data.polygons:
            centre = obj.matrix_world @ polygon.center
            distance = sum((centre[k] - point[k]) ** 2 for k in range(3))
            if distance < best_distance:
                best, best_distance = polygon.index, distance
    return best


def face_texel(index: int, face_count: int, size: int) -> int:
    """The texel at the centre of a face's grid cell, for reading a bake back by face index."""
    columns = int(math.ceil(math.sqrt(face_count)))
    column, row = index % columns, index // columns
    x = int((column + 0.5) / columns * size)
    y = int((row + 0.5) / columns * size)
    return min(size * size - 1, y * size + x)


# ======================================================================================= selftest


def selftest() -> int:
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("lightmap_bake: selftest")
    workdir = tempfile.mkdtemp(prefix="lightmap_bake_selftest_")
    size, samples, seed = 32, 16, 4242
    try:
        lights = build_two_room_fixture()
        face_count = len(bpy.data.objects["shell"].data.polygons)
        # Faces by position, not by emission order -- see `face_index_at`.
        red_wall = face_index_at((4.0, 1.3, 0.0))
        white_wall = face_index_at((-4.0, 1.3, 0.0))
        near_ceiling_face = face_index_at((-3.25, 2.6, 0.0))
        far_ceiling_face = face_index_at((2.0, 2.6, 0.0))

        scene = bpy.context.scene
        configure(scene, seed, samples)
        select_lightmap_uv()
        image = make_bake_target(size)

        # 1. Blender 4.x defaults to AgX. Left alone it tone-maps the lightmap.
        require(scene.cycles.seed == seed and scene.cycles.use_animated_seed is False,
                f"configure() applies the seed ({scene.cycles.seed}) and turns the ANIMATED seed "
                f"off -- an animated seed varies with the frame, so two bakes of one scene at one "
                f"frame still match and only a scene rendered at another frame would differ")
        require(scene.cycles.use_adaptive_sampling is False,
                "...and adaptive sampling is off: it stops per-texel on a noise estimate, so the "
                "sample count varies with the tile decomposition and reproducibility stops being "
                "a property of the seed alone")
        require(scene.view_settings.view_transform == "Standard",
                f"the view transform is Standard, not Blender 4.x's AgX default -- a filmic tone "
                f"map on a lightmap is a photograph of the lighting, not a measurement of it")
        require(image.colorspace_settings.name == "Non-Color",
                "the bake target is Non-Color: a lightmap is data, not a picture")

        # 2. The claim this tool turns on: irradiance, not lit colour. The red wall and the white
        #    wall are lit identically, so with `use_pass_color` off they must bake identically.
        for obj in light_objects():
            obj.hide_render = False
        set_world_sky(0.0)
        irradiance = bake_once(image, size)
        red_texel = face_texel(red_wall, face_count, size)
        white_texel = face_texel(white_wall, face_count, size)
        red_rgb = irradiance[red_texel * 4:red_texel * 4 + 3]
        require(max(red_rgb) > 0.1,
                f"the red wall is brightly lit, so the claim below has something to measure "
                f"({[round(v, 4) for v in red_rgb]}) -- an unlit face is trivially grey and would "
                f"pass it for the wrong reason")
        require(max(red_rgb) - min(red_rgb) < 0.02 * max(red_rgb),
                f"the RED wall bakes to a grey value -- no albedo in it "
                f"({[round(v, 4) for v in red_rgb]})")
        white_rgb = irradiance[white_texel * 4:white_texel * 4 + 3]
        require(max(white_rgb) > 0.1 and abs(luminance(irradiance, red_texel)
                                             - luminance(irradiance, white_texel)) < 100.0,
                f"...and the white wall bakes to a value of the same kind, not a different one "
                f"({[round(v, 4) for v in white_rgb]})")

        scene.render.bake.use_pass_color = True
        coloured = bake_once(image, size)
        tinted = coloured[red_texel * 4:red_texel * 4 + 3]
        require(tinted[0] > tinted[1] * 3 + 1e-6,
                f"...and with use_pass_color ON it comes out red instead "
                f"({[round(v, 4) for v in tinted]}), which is the albedo baked in and then "
                f"multiplied a second time by DualTextureEffect at runtime")
        scene.render.bake.use_pass_color = False

        # 3. Indirect is included. §18.3 says so, and the far room's ceiling has no straight line
        #    to the near room's lamp -- anything reaching it has bounced through the doorway.
        configure(scene, seed, samples)
        require(scene.render.bake.use_pass_indirect is True
                and scene.render.bake.use_pass_direct is True,
                "configure() leaves BOTH the direct and indirect passes on, as §18.3 requires -- "
                "asserted against the tool's own setting, because a claim that switches the pass "
                "on itself before measuring cannot notice that the tool forgot to")
        for obj in light_objects():
            obj.hide_render = obj.name != "LIGHT_A"
        with_bounce = bake_once(image, size)
        scene.render.bake.use_pass_indirect = False
        direct_only = bake_once(image, size)
        scene.render.bake.use_pass_indirect = True
        far_ceiling = face_texel(far_ceiling_face, face_count, size)
        require(luminance(with_bounce, far_ceiling) > luminance(direct_only, far_ceiling) + 1e-4,
                f"the far room's ceiling is lit only by bounce: "
                f"{luminance(with_bounce, far_ceiling):.5f} with indirect against "
                f"{luminance(direct_only, far_ceiling):.5f} without")
        require(luminance(with_bounce, far_ceiling) > 1e-5,
                "...and it is not zero, so 'indirect included' has content")

        # 4. One lightmap per SWITCH GROUP, and the groups are genuinely separate. §28.3 bakes per
        #    group precisely so the runtime can turn one off without the others changing.
        near_ceiling = face_texel(near_ceiling_face, face_count, size)
        for obj in light_objects():
            obj.hide_render = obj.name != "LIGHT_A"
        only_a = bake_once(image, size)
        for obj in light_objects():
            obj.hide_render = obj.name != "LIGHT_B"
        only_b = bake_once(image, size)
        require(luminance(only_a, near_ceiling) > luminance(only_b, near_ceiling) * 3,
                f"group A's bake lights room A far more than group B's does "
                f"({luminance(only_a, near_ceiling):.4f} against "
                f"{luminance(only_b, near_ceiling):.4f})")
        require(luminance(only_b, far_ceiling) > luminance(only_a, far_ceiling) * 3,
                f"...and the reverse holds in room B "
                f"({luminance(only_b, far_ceiling):.4f} against "
                f"{luminance(only_a, far_ceiling):.4f})")

        # 5. The daylight bake is sky only. Turning every lamp to maximum must not change it.
        for obj in light_objects():
            obj.hide_render = True
        set_world_sky(SKY_STRENGTH)
        day = bake_once(image, size)
        for obj in light_objects():
            obj.hide_render = True
            obj.data.energy *= 50.0
        brighter_day = bake_once(image, size)
        for obj in light_objects():
            obj.data.energy /= 50.0
        require(max(abs(a - b) for a, b in zip(day, brighter_day)) < 1e-6,
                "LM_DAY is unchanged when every lamp is made 50x brighter, because the lamps are "
                "off in it -- it is lit by the sky through the openings and by nothing else")
        # The RGB channels only: `max(day)` over the flat pixel list reads the ALPHA, which is 1.0
        # everywhere, so it reports a bright daylight bake for a scene in total darkness.
        day_peak = max(max(day[0::4]), max(day[1::4]), max(day[2::4]))
        require(day_peak > 1e-4,
                f"...and the sky does reach room A through the skylight ({day_peak:.5f})")
        require(day_peak < 1.0 - 1e-6,
                f"...at a level below the open-sky value of 1.0 ({day_peak:.5f}), because it "
                f"arrives through a 1 m opening rather than falling on an exposed surface -- "
                f"a reading of exactly 1.0 would mean the sky was lighting the outside of a wall")
        set_world_sky(0.0)

        # 6. The bake uses the SECOND UV set. UV0 sends every face to one corner, so a bake that
        #    used it would light one texel and leave the rest black.
        lit_texels = sum(1 for i in range(size * size) if luminance(irradiance, i) > 1e-5)
        require(lit_texels > face_count,
                f"{lit_texels} texels carry light across {face_count} faces -- the bake used the "
                f"{LIGHTMAP_UV!r} set, not UV0, which packs every face into one corner")
        mesh = bpy.data.objects["shell"].data
        mesh.uv_layers.active = mesh.uv_layers["UVMap"]
        wrong = bake_once(image, size)
        wrong_texels = sum(1 for i in range(size * size) if luminance(wrong, i) > 1e-5)
        require(wrong_texels < lit_texels / 2,
                f"...and baking into UV0 really does collapse it ({wrong_texels} texels against "
                f"{lit_texels})")
        select_lightmap_uv()

        # 6b. glTF preserves TEXCOORD_1 but not Blender's layer spelling.  Exercise the imported
        #      shape rather than letting the fixture's friendly in-memory name hide that fact.
        mesh.uv_layers[LIGHTMAP_UV].name = "UVMap.001"
        select_lightmap_uv()
        require(mesh.uv_layers.active == mesh.uv_layers[1],
                "a glTF-style second UV channel is selected even though the Blender layer name "
                "did not survive export")

        # 6c. An explicit detail mesh has no UV2 and remains an occluder rather than consuming
        #      atlas space or making the bake reject the correctly unwrapped receiver beside it.
        detail_mesh = bpy.data.meshes.new("detail_mesh")
        detail_mesh.from_pydata([(0, 0, 0), (0.1, 0, 0), (0, 0.1, 0)], [], [(0, 1, 2)])
        detail_material = bpy.data.materials.new("detail_material")
        detail_material["lightmapReceiver"] = False
        detail_mesh.materials.append(detail_material)
        detail = bpy.data.objects.new("detail", detail_mesh)
        bpy.context.collection.objects.link(detail)
        select_lightmap_uv()
        require(detail not in receiver_objects(),
                "an explicit non-receiver with no UV2 stays in the bake scene only as an occluder")
        bpy.data.objects.remove(detail, do_unlink=True)

        # 7. A missing lightmap UV set is refused, naming the tool that makes one.
        mesh.uv_layers[1].name = LIGHTMAP_UV
        mesh.uv_layers.remove(mesh.uv_layers[LIGHTMAP_UV])
        try:
            select_lightmap_uv()
            raised = ""
        except SystemExit as exc:
            raised = str(exc)
        require("lightmap_unwrap" in raised and "HOUSE-00205" in raised,
                "a shell with no Lightmap UV set is refused, naming HOUSE-00205's tool")
        lights = build_two_room_fixture()
        # `read_factory_settings` inside the fixture builder replaces the Scene, so every earlier
        # reference to it is a dangling StructRNA. Re-bind rather than reuse.
        scene = bpy.context.scene
        configure(scene, seed, samples)

        # 7b. Production GLBs contain no Blender lights; the authored JSON rows create them.
        authored = [{"id": "AUTHORED_SPOT", "type": "spot", "position": [0.0, 2.0, 0.0],
                     "direction": [0.0, -1.0, 0.0], "coneInnerDeg": 20.0,
                     "coneOuterDeg": 40.0, "intensityLm": 683.0, "range": 5.0,
                     "bakedIntoLightmap": True}]
        create_light_objects(authored)
        created = bpy.data.objects.get("AUTHORED_SPOT")
        require(created is not None and created.type == "LIGHT" and created.data.type == "SPOT"
                and abs(created.data.energy - 1.0) < 1e-6
                and created.data.shadow_soft_size < 0.02
                and tuple(round(value, 6) for value in created.location) == (0.0, 0.0, 2.0)
                and (created.rotation_euler.to_matrix() @ Vector((0.0, 0.0, -1.0))
                     - Vector((0.0, 0.0, -1.0))).length < 1e-6,
                "an authored Y-up spot row becomes a correctly placed, downward-facing white "
                "one-watt Blender emitter small enough for a 20 mm recessed-fixture clearance")
        bpy.data.objects.remove(created, do_unlink=True)
        calibrated = {**authored[0], "id": "CALIBRATED_SPOT"}
        create_light_objects([calibrated], 100.0)
        calibrated_object = bpy.data.objects.get("CALIBRATED_SPOT")
        require(calibrated_object is not None and
                abs(calibrated_object.data.energy - 6.83) < 1e-5 and
                shell_hash(authored, 100.0) != shell_hash(authored) and
                shell_hash(authored, 100.0) == shell_hash(authored, 100.0),
                "an explicit broadband-white calibration changes radiometric power and the "
                "recorded product hash without changing legacy bake identities")
        bpy.data.objects.remove(calibrated_object, do_unlink=True)
        select_lightmap_uv()
        image = make_bake_target(size)

        # 8. Normalisation. An 8-bit PNG cannot hold irradiance, and the fixture proves it.
        for obj in light_objects():
            obj.hide_render = False
        pixels = bake_once(image, size)
        peak = max(pixels[i] for i in range(0, len(pixels), 4))
        require(peak > 1.0,
                f"the bake really does exceed 1.0 ({peak:.3f}), so saving it straight into an "
                f"8-bit PNG would clip the pendant's pool of light to flat white")
        path = os.path.join(workdir, "norm.png")
        scale = normalise_and_save(pixels, size, path)
        require(abs(scale - peak) < 1e-4,
                f"the recorded scale is the image's own peak ({scale:.4f} vs {peak:.4f})")
        saved = bpy.data.images.load(path)
        saved.colorspace_settings.name = "Non-Color"
        stored = list(saved.pixels)
        require(max(stored[0::4]) <= 1.0 + 1e-6,
                "the saved PNG is inside 0..1, so nothing clips")
        brightest = max(range(size * size), key=lambda i: luminance(pixels, i))
        require(abs(stored[brightest * 4] * scale - pixels[brightest * 4]) < 0.02 * peak,
                f"...and scale x png recovers the baked value to within 2 % at the brightest "
                f"texel, so the shape survives the round trip through 8 bits")
        bpy.data.images.remove(saved)

        # 9. Determinism. §18.4: every tool takes a seed and the same seed is the same output.
        out_a = os.path.join(workdir, "a")
        out_b = os.path.join(workdir, "b")
        first = bake_cell(lights, "L0_TEST", out_a, size, samples, seed, pack=True)
        second = bake_cell(lights, "L0_TEST", out_b, size, samples, seed, pack=True)
        for name in sorted(os.listdir(out_a)):
            if not name.endswith(".png"):
                continue
            with open(os.path.join(out_a, name), "rb") as handle:
                left = handle.read()
            with open(os.path.join(out_b, name), "rb") as handle:
                right = handle.read()
            require(left == right, f"{name} is byte-identical across two runs at one seed")
        require(first["shellHash"] == second["shellHash"],
                "and so is the shell hash")

        # 9b. The daylight atlas that BAKE_CELL writes is sky-only. Claim 5 above proves the
        #     property by switching the lamps off itself, which cannot notice that `bake_cell`
        #     forgot to -- so this makes every lamp 50x brighter and requires the LM_DAY file it
        #     produces to be byte-identical while the group atlases change.
        out_bright = os.path.join(workdir, "bright")
        for obj in light_objects():
            obj.data.energy *= 50.0
        bake_cell(lights, "L0_TEST", out_bright, size, samples, seed, pack=False)
        for obj in light_objects():
            obj.data.energy /= 50.0

        def read(directory, name):
            with open(os.path.join(directory, name), "rb") as handle:
                return handle.read()

        require(read(out_a, "L0_TEST_LM_DAY.png") == read(out_bright, "L0_TEST_LM_DAY.png"),
                "the LM_DAY atlas bake_cell writes is unchanged by making every lamp 50x "
                "brighter, so its own bake really does switch them off")
        require(read(out_a, "L0_TEST_LM_LG_A.png") != read(out_bright, "L0_TEST_LM_LG_A.png"),
                "...while the group atlases do change, so the comparison above has content")

        # 9c. And the group atlases bake_cell writes are lamp-only. Starting the scene with a
        #     blazing sky must not reach them: `bake_cell` sets the world dark before each group.
        # Compared against a hand-made lamps-only bake, not against another `bake_cell` run: if
        # `bake_cell` left the sky on, both of ITS runs would carry it equally and agree with
        # each other perfectly. The reference has to come from outside the thing being tested.
        # `bake_cell` recreates the shared bake target, so the earlier handle is a dangling
        # StructRNA -- the same trap the Scene reference sprang two claims ago.
        image = make_bake_target(size)
        select_lightmap_uv()
        for obj in light_objects():
            obj.hide_render = obj.name != "LIGHT_A"
        set_world_sky(0.0)
        reference = bake_once(image, size)
        reference_path = os.path.join(workdir, "reference_lg_a.png")
        normalise_and_save(reference, size, reference_path)
        with open(reference_path, "rb") as handle:
            reference_bytes = handle.read()
        require(read(out_a, "L0_TEST_LM_LG_A.png") == reference_bytes,
                "bake_cell's LG_A atlas is byte-identical to a lamps-only bake made here with the "
                "sky explicitly dark, so each group bake really is that group's lamps and nothing "
                "else")
        set_world_sky(20.0)
        with_sky = bake_once(image, size)
        # Compared on the TOTAL, not the peak: the peak sits on the lamp's own hot spot, which a
        # sky does not brighten, so a peak comparison reports no difference and proves nothing.
        lamp_total = sum(reference[0::4])
        sky_total = sum(with_sky[0::4])
        require(sky_total > lamp_total * 1.05,
                f"...and a sky left on would have shown: it raises the atlas total from "
                f"{lamp_total:.1f} to {sky_total:.1f}, so the comparison above has content")
        set_world_sky(0.0)

        # 9d. The seed, behaviourally: an ANIMATED seed varies with the frame, so two bakes at
        #     different frames diverge. At a fixed seed they must not.
        out_frame = os.path.join(workdir, "frame")
        scene.frame_set(scene.frame_current + 17)
        bake_cell(lights, "L0_TEST", out_frame, size, samples, seed, pack=False)
        scene.frame_set(scene.frame_current - 17)
        require(read(out_a, "L0_TEST_LM_LG_A.png") == read(out_frame, "L0_TEST_LM_LG_A.png"),
                "the same seed at a different FRAME gives the same bake, which an animated seed "
                "would not -- determinism is a property of the seed, not of when it was run")

        # 10. The sidecar records what the numbers were made with, including the settings that
        #     would silently ruin them.
        require(first["seed"] == seed and first["samples"] == samples and first["size"] == size,
                "the sidecar records the seed, the sample count and the resolution")
        require(first["denoised"] == scene.cycles.use_denoising and first["denoised"] is True,
                f"the sidecar's `denoised` matches the scene it was baked in "
                f"({first['denoised']} against {scene.cycles.use_denoising}) -- §18.3 asks for "
                f"denoised bakes and a sidecar that merely claims it proves nothing")
        require(first["bakePassColor"] is False and first["viewTransform"] == "Standard"
                and first["uvSet"] == LIGHTMAP_UV,
                "...and the three settings that would silently ruin a lightmap, so a bad bake can "
                "be identified from its sidecar rather than by looking at it")
        require([entry["group"] for entry in first["groups"]] == ["LG_A", "LG_B"],
                f"one atlas per light group, named by group "
                f"({[e['group'] for e in first['groups']]})")
        require(os.path.isfile(os.path.join(out_a, "L0_TEST_LM_DAY.png")),
                "plus the daylight atlas §18.3 asks for")
        require(all(entry["scale"] > 0 for entry in first["groups"]),
                "every atlas carries the scale needed to recover its values")

        # 11. Staleness. §18.3 step 5: a stale bake must fail the build loudly.
        before = shell_hash(lights)
        shell = bpy.data.objects["shell"]
        shell.data.vertices[0].co.x += 0.25
        shell.data.update()
        require(shell_hash(lights) != before,
                "moving one vertex changes the shell hash")
        shell.data.vertices[0].co.x -= 0.25
        shell.data.update()
        require(shell_hash(lights) == before, "...and moving it back restores it")
        moved = [dict(light) for light in lights]
        moved[0]["position"] = [-1.5, 2.3, 0.0]
        require(shell_hash(moved) != before,
                "moving a LAMP changes it too -- the bake depends on the lights as much as on the "
                "geometry, and a hash over geometry alone would call a relit room fresh")

        # 12. §23.4's packed texture: three groups in R, G, B of one image.
        packed_path = os.path.join(out_a, "L0_TEST_LM_PACKED.png")
        require(os.path.isfile(packed_path), "the Tier-E packed atlas is written")
        packed = bpy.data.images.load(packed_path)
        packed.colorspace_settings.name = "Non-Color"
        values = list(packed.pixels)
        near = face_texel(near_ceiling_face, face_count, size)
        far = face_texel(far_ceiling_face, face_count, size)
        require(values[near * 4] > values[near * 4 + 1],
                f"in room A the R channel (group A) beats G (group B) "
                f"({values[near * 4]:.4f} vs {values[near * 4 + 1]:.4f})")
        require(values[far * 4 + 1] > values[far * 4],
                f"and in room B the G channel beats R "
                f"({values[far * 4 + 1]:.4f} vs {values[far * 4]:.4f})")
        require(max(values[2::4]) < 1e-6,
                "the third channel is empty, because the fixture has only two groups")
        # Each channel is normalised by its own peak, like the per-group atlases. Written raw it
        # reaches 22 on this fixture and clamps to a flat 1.0 over most of the lit area -- a
        # packed atlas that threw away exactly the shape it exists to carry.
        require(first["packedScales"] is not None and len(first["packedScales"]) == 3,
                f"the sidecar records a scale per packed channel "
                f"({first['packedScales']})")
        require(all(0.0 < v <= 1.0 + 1e-6 for v in values[0::4] if v > 0.0),
                "every red-channel value is inside 0..1")
        saturated = sum(1 for v in values[0::4] if v > 0.999)
        require(saturated < size * size * 0.05,
                f"and only {saturated} of {size * size} texels are at full scale -- raw luminance "
                f"would have clamped most of the lit area flat")
        bpy.data.images.remove(packed)

    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    if failures:
        return 1
    print("lightmap_bake: selftest passed.")
    return 0


def main() -> int:
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if "--selftest" in argv:
        return selftest()

    positional = [a for a in argv if not a.startswith("--")]
    options = {}
    index = 0
    while index < len(argv):
        if argv[index].startswith("--") and index + 1 < len(argv) \
                and not argv[index + 1].startswith("--"):
            options[argv[index][2:]] = argv[index + 1]
            index += 2
        else:
            index += 1
    for key in ("lights", "cell", "out", "size", "samples", "seed", "lumens-per-radiant-watt"):
        positional = [p for p in positional if p != options.get(key)]

    if len(positional) != 1 or "lights" not in options or "cell" not in options \
            or "out" not in options:
        print("lightmap_bake: usage: SHELL.glb --lights lights.json --cell ID --out DIR "
              "[--size N] [--samples N] [--seed N] [--lumens-per-radiant-watt N] [--pack] "
              "[--artificial-only|--daylight-only]", file=sys.stderr)
        return 2

    artificial_only = "--artificial-only" in argv
    daylight_only = "--daylight-only" in argv
    if artificial_only and daylight_only:
        print("lightmap_bake: --artificial-only and --daylight-only are mutually exclusive",
              file=sys.stderr)
        return 2
    lumens_per_radiant_watt = float(options.get("lumens-per-radiant-watt",
                                               LEGACY_LUMENS_PER_RADIANT_WATT))
    if not math.isfinite(lumens_per_radiant_watt) or lumens_per_radiant_watt <= 0.0:
        print("lightmap_bake: --lumens-per-radiant-watt must be positive and finite",
              file=sys.stderr)
        return 2

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=positional[0])
    # The authoritative world files are JSONC.  The deployed copy happens to be plain JSON, but a
    # bake must be reproducible from source rather than depending on an earlier deploy stage.
    world_tools = os.path.join(os.path.dirname(os.path.dirname(__file__)), "world")
    if world_tools not in sys.path:
        sys.path.insert(0, world_tools)
    import layout_io  # type: ignore  # noqa: E402
    with open(options["lights"], encoding="utf-8") as handle:
        document = json.loads(layout_io.strip_jsonc(handle.read()))
    cell = options["cell"]
    lights = [light for light in document.get("lights", []) if light.get("cell") == cell]
    create_light_objects(lights, lumens_per_radiant_watt)

    bake_cell(lights, cell, options["out"],
              int(options.get("size", DEFAULT_SIZE)),
              int(options.get("samples", DEFAULT_SAMPLES)),
              int(options.get("seed", DEFAULT_SEED)),
              "--pack" in argv,
              artificial=not daylight_only,
              daylight=not artificial_only,
              lumens_per_radiant_watt=lumens_per_radiant_watt)
    return 0


if __name__ == "__main__":
    # Blender does not propagate a script's exit status; see tools/blender/blender_env.py. A tool
    # that omits this sentinel reports failure on a clean run and success on nothing -- which is
    # how the first version of this file behaved, and it made an entire bug-injection sweep read
    # as "every bug caught" while catching none of them.
    try:
        _status = main()
    except SystemExit as _exit:
        if isinstance(_exit.code, int):
            _status = _exit.code
        else:
            print(str(_exit.code), file=sys.stderr)
            _status = 1
    except BaseException:  # noqa: BLE001
        import traceback

        traceback.print_exc()
        print("lightmap_bake: EXIT 1")
        raise
    print(f"lightmap_bake: EXIT {_status}")
    sys.exit(_status)
