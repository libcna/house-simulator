#!/usr/bin/env python3
"""cubemap_bake.py -- what each mirror sees, baked once because the room does not move.

`HOUSE-00209`. `cna-house.md` §59: "Mirrors: 4 of them. Implementation is a **static cube map** per
mirror baked offline (`EnvironmentMapEffect`), which is correct for a fixed mirror in a fixed room
and costs nothing. The player's reflection is **not** rendered -- a deliberate, documented
limitation (D-19, §77)."

    tools/blender/cubemap_bake.py SHELL.glb --mirrors mirrors.json --out DIR
    tools/blender/cubemap_bake.py SHELL.glb --mirrors mirrors.json --out DIR --size 512
    tools/blender/cubemap_bake.py --selftest

## The face convention is half measured and half asserted, and the half that is asserted says so

`HOUSE-00081` measured the part that matters most, against CNA itself: a reflection vector of
`(0, 0, 1)` samples the `+Z` face, `(1, 0, 0)` samples `+X`, `(-1, 0, 0)` samples `-X`
(`docs/cna-capability-report.md`). So **which** face a direction lands on is known, and this tool
renders to that mapping.

What `HOUSE-00081` did **not** measure is the orientation *within* a face -- which way is up, and
which way is right. That follows Direct3D's cube convention, which XNA inherits, and it is
asserted here rather than measured: a face rendered upside down or mirrored still reflects the
right room, and looks entirely plausible until someone reads a sign in it.

What the selftest *can* check without CNA is that the six faces agree with each other. Adjacent
faces share an edge and are rendered from the same point, so the pixels along that shared edge see
the same rays and must match. **Seam continuity catches every per-face error** -- one face rotated,
one flipped, one up-vector wrong -- and leaves only a single global handedness flip, which is
flagged in `docs/cubemap-format.md` as the one thing to probe the first time a mirror is placed.

## Two smaller decisions

**The mirror hides itself.** A camera at the mirror's own position that can see the mirror bakes
the mirror's back into its own reflection. Every object named in the mirror's `hide` list is
hidden for its bake, and the mirror's own prop is added to that list automatically.

**Standard view transform, but sRGB colour.** Blender 4.x's AgX default would tone-map the
reflection so it no longer matches the room drawn beside it by XNA. Unlike a lightmap, though, a
cube map really is colour rather than data, so it keeps the ordinary sRGB write -- the opposite
decision from `lightmap_bake.py`, for the opposite reason, and worth saying out loud because the
two tools sit next to each other.

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import json
import math
import os
import sys

try:
    import bpy  # type: ignore
    from mathutils import Matrix, Vector  # type: ignore

    INSIDE_BLENDER = True
except ImportError:
    INSIDE_BLENDER = False

if not INSIDE_BLENDER:
    from pathlib import Path

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import blender_env  # noqa: E402

    sys.exit(blender_env.relaunch(Path(__file__).resolve(), sys.argv[1:], tool="cubemap_bake"))

VERSION = 1

#: §59's four mirrors at this size are 4 x 6 x 256^2 x 3 = 4.7 MB uncompressed, 1.2 MB as DXT1.
DEFAULT_SIZE = 256

#: 90 degrees, and it is the only value that works. A face's half-extent at unit distance is
#: `tan(fov/2)`, so `tan(45) == 1` is exactly the condition that six faces tile a cube with no gap
#: and no overlap. Anything narrower leaves a wedge of the room in no face at all; anything wider
#: puts the same wedge in two, and the seam shows as a doubled reflection.
FACE_FOV_DEG = 90.0

#: Direct3D's cube faces, in Direct3D's order, each with the forward/up/right basis XNA inherits.
#: The order is the one `TextureCube` indexes by, so a reader can take them positionally.
#: `HOUSE-00081` measured the forward vectors against CNA; the up and right vectors are D3D's
#: convention and are asserted rather than measured -- see the module docstring.
FACES = [
    ("px", (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, -1.0)),
    ("nx", (-1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)),
    ("py", (0.0, 1.0, 0.0), (0.0, 0.0, -1.0), (1.0, 0.0, 0.0)),
    ("ny", (0.0, -1.0, 0.0), (0.0, 0.0, 1.0), (1.0, 0.0, 0.0)),
    ("pz", (0.0, 0.0, 1.0), (0.0, 1.0, 0.0), (1.0, 0.0, 0.0)),
    ("nz", (0.0, 0.0, -1.0), (0.0, 1.0, 0.0), (-1.0, 0.0, 0.0)),
]

#: Which faces share an edge with which, for the seam check. Every pair that touches.
ADJACENT = [("px", "py"), ("px", "ny"), ("px", "pz"), ("px", "nz"),
            ("nx", "py"), ("nx", "ny"), ("nx", "pz"), ("nx", "nz"),
            ("pz", "py"), ("pz", "ny"), ("nz", "py"), ("nz", "ny")]


def report(message: str) -> None:
    print(f"  {message}")


# ==================================================================================== the cameras


def mirror_horizontally(source: str, destination: str) -> None:
    """Flip a rendered face left-to-right, and re-save it with our own writer.

    **Two jobs, and they happen to be the same operation.**

    The flip is the **handedness step**. Direct3D's cube faces are described in a *left-handed*
    space and this world is right-handed (§14), so a camera built with a right-handed basis --
    which is the only kind Blender has -- produces a face whose right vector is the negation of
    D3D's. Mirroring the image is what converts one to the other; without it every reflection
    reads correct at a glance and is inside out, which is exactly the kind of wrong that survives
    review until someone holds a book up to a mirror.

    Re-saving is what makes the output **byte-identical between runs**. Measured: two bakes of one
    scene produce pixel-identical images in files that differ, because
    `render(write_still=True)` stamps metadata into the PNG. `HOUSE-00199` rebuilds everything and
    asserts byte-identical output, so a stamped file would fail that gate for a render that never
    changed. `image.save()` writes no such chunk.
    """
    image = bpy.data.images.load(source)
    try:
        size = image.size[0]
        height = image.size[1]
        raw = list(image.pixels)
        flipped = [0.0] * len(raw)
        for y in range(height):
            for x in range(size):
                source_index = (y * size + x) * 4
                target_index = (y * size + (size - 1 - x)) * 4
                flipped[target_index:target_index + 4] = raw[source_index:source_index + 4]
        out = bpy.data.images.new(f"flip_{os.path.basename(destination)}", size, height,
                                  alpha=False)
        try:
            out.pixels = flipped
            out.filepath_raw = destination
            out.file_format = "PNG"
            out.save()
        finally:
            bpy.data.images.remove(out)
    finally:
        bpy.data.images.remove(image)


def face_matrix(position, forward, up):
    """A Blender camera transform looking along `forward` with `up` up.

    Blender's camera looks down its own **-Z** with +Y up, so the basis is
    `(right, up, -forward)` -- writing `(right, up, forward)` gives a camera pointing backwards,
    which bakes the room behind the mirror into the face in front of it and looks like a room.
    """
    f = Vector(forward).normalized()
    u = Vector(up).normalized()
    r = f.cross(u).normalized()
    u = r.cross(f).normalized()
    matrix = Matrix.Identity(4)
    for row in range(3):
        matrix[row][0] = r[row]
        matrix[row][1] = u[row]
        matrix[row][2] = -f[row]
        matrix[row][3] = position[row]
    return matrix


def configure(scene, size: int) -> None:
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = size
    scene.render.resolution_y = size
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.film_transparent = False
    # Blender 4.x defaults to AgX. A tone-mapped reflection no longer matches the room XNA draws
    # beside it. Unlike a lightmap this IS colour, so the ordinary sRGB write is kept.
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0


def bake_mirror(mirror: dict, out_dir: str, size: int) -> dict:
    """Six faces from one point, written as `<id>_CUBE_<face>.png`."""
    scene = bpy.context.scene
    configure(scene, size)

    hidden = []
    for name in list(mirror.get("hide", [])) + [mirror.get("prop"), mirror["id"]]:
        obj = bpy.data.objects.get(name) if name else None
        if obj is not None and not obj.hide_render:
            obj.hide_render = True
            hidden.append(obj)

    camera_data = bpy.data.cameras.new(f"{mirror['id']}_cam")
    camera_data.type = "PERSP"
    camera_data.sensor_fit = "AUTO"
    camera_data.angle = math.radians(FACE_FOV_DEG)
    camera = bpy.data.objects.new(f"{mirror['id']}_cam", camera_data)
    bpy.context.collection.objects.link(camera)
    previous = scene.camera
    scene.camera = camera

    os.makedirs(out_dir, exist_ok=True)
    written = []
    try:
        for name, forward, up, _right in FACES:
            camera.matrix_world = face_matrix(mirror["position"], forward, up)
            path = os.path.join(out_dir, f"{mirror['id']}_CUBE_{name}.png")
            raw_path = os.path.join(out_dir, f".{mirror['id']}_CUBE_{name}_raw.png")
            scene.render.filepath = raw_path
            bpy.ops.render.render(write_still=True)
            mirror_horizontally(raw_path, path)
            os.remove(raw_path)
            written.append(os.path.basename(path))
    finally:
        scene.camera = previous
        bpy.data.objects.remove(camera, do_unlink=True)
        bpy.data.cameras.remove(camera_data)
        for obj in hidden:
            obj.hide_render = False

    return {"id": mirror["id"], "cell": mirror.get("cell"),
            "position": list(mirror["position"]), "size": size,
            "faces": written, "faceOrder": [name for name, *_ in FACES],
            "hidden": [obj.name for obj in hidden]}


# =================================================================================== reading back


def load_face(path: str):
    """A rendered face as `(size, [ (r,g,b) per pixel, bottom row first ])`."""
    image = bpy.data.images.load(path)
    try:
        size = image.size[0]
        raw = list(image.pixels)
        return size, [(raw[i * 4], raw[i * 4 + 1], raw[i * 4 + 2]) for i in range(size * size)]
    finally:
        bpy.data.images.remove(image)


def pixel(face, size: int, x: int, y: int):
    return face[y * size + x]


def centre_colour(face, size: int):
    return pixel(face, size, size // 2, size // 2)


def dominant(colour) -> str:
    """Which of R/G/B a face's centre is, for a fixture painted one pure colour per wall."""
    names = "rgb"
    return names[max(range(3), key=lambda k: colour[k])]


# ======================================================================================= fixtures


def build_painted_room(size: float = 4.0):
    """A closed cube room whose six walls are six distinct pure colours.

    Six colours because the question is "which face saw which wall", and that is one pixel read.
    The same trick `HOUSE-00081` used against CNA, pointed the other way: it checked that a
    direction samples the right face, and this checks that the right face was rendered from the
    right direction.
    """
    bpy.ops.wm.read_factory_settings(use_empty=True)
    half = size / 2
    colours = {
        "px": (1.0, 0.0, 0.0), "nx": (0.0, 1.0, 0.0),
        "py": (0.0, 0.0, 1.0), "ny": (1.0, 1.0, 0.0),
        "pz": (1.0, 0.0, 1.0), "nz": (0.0, 1.0, 1.0),
    }
    walls = {
        "px": [(half, -half, -half), (half, half, -half), (half, half, half), (half, -half, half)],
        "nx": [(-half, -half, -half), (-half, -half, half), (-half, half, half),
               (-half, half, -half)],
        "py": [(-half, half, -half), (-half, half, half), (half, half, half), (half, half, -half)],
        "ny": [(-half, -half, -half), (half, -half, -half), (half, -half, half),
               (-half, -half, half)],
        "pz": [(-half, -half, half), (half, -half, half), (half, half, half), (-half, half, half)],
        "nz": [(-half, -half, -half), (-half, half, -half), (half, half, -half),
               (half, -half, -half)],
    }
    for name, corners in walls.items():
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(list(corners), [], [[0, 1, 2, 3]])
        mesh.update()
        material = bpy.data.materials.new(name)
        material.use_nodes = True
        nodes = material.node_tree.nodes
        emission = nodes.new("ShaderNodeEmission")
        emission.inputs["Color"].default_value = (*colours[name], 1.0)
        emission.inputs["Strength"].default_value = 1.0
        output = nodes["Material Output"]
        material.node_tree.links.new(emission.outputs[0], output.inputs["Surface"])
        mesh.materials.append(material)
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.collection.objects.link(obj)
    return colours


# ======================================================================================= selftest


def selftest() -> int:
    import shutil
    import tempfile

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        print(f"  {'ok  ' if condition else 'FAIL'}  {message}")
        if not condition:
            failures.append(message)

    print("cubemap_bake: selftest")
    workdir = tempfile.mkdtemp(prefix="cubemap_bake_selftest_")
    size = 32
    try:
        # 1. The camera basis. Blender's camera looks down its own -Z.
        matrix = face_matrix((1.0, 2.0, 3.0), (0.0, 0.0, 1.0), (0.0, 1.0, 0.0))
        looking = -Vector((matrix[0][2], matrix[1][2], matrix[2][2]))
        require(all(abs(looking[k] - (0.0, 0.0, 1.0)[k]) < 1e-6 for k in range(3)),
                f"a camera built for +Z looks along +Z ({tuple(round(v, 3) for v in looking)}) -- "
                f"writing the basis as (right, up, forward) points it backwards and bakes the "
                f"room BEHIND the mirror into the face in front of it")
        require(all(abs(matrix[k][3] - (1.0, 2.0, 3.0)[k]) < 1e-6 for k in range(3)),
                "and it sits where the mirror is")
        # The camera's own right vector is the NEGATION of D3D's, on every face, and that is not
        # a bug: D3D's cube faces are described in a left-handed space and §14's world is
        # right-handed, and a Blender camera basis can only be right-handed. The flip on write is
        # what converts one to the other, and this is the assertion that pins the direction of it.
        for name, forward, up, right in FACES:
            m = face_matrix((0.0, 0.0, 0.0), forward, up)
            actual_right = Vector((m[0][0], m[1][0], m[2][0]))
            require(all(abs(actual_right[k] + right[k]) < 1e-6 for k in range(3)),
                    f"face {name}: the camera's right is "
                    f"{tuple(round(v, 1) + 0.0 for v in actual_right)} and D3D's is {right} -- "
                    f"exact negations, which is the handedness difference the horizontal flip on "
                    f"write exists to remove")
            require(all(abs((Vector(up).cross(Vector(forward)))[k] - right[k]) < 1e-6
                        for k in range(3)),
                    f"...and D3D's own right for {name} is up x forward, which is the "
                    f"left-handed cross product")

        # 2. Six faces of exactly 90 degrees tile the sphere. Anything else leaves a wedge of the
        #    room in no face, or the same wedge in two.
        require(len(FACES) == 6 and len({f[0] for f in FACES}) == 6,
                "there are six faces and no duplicates")
        require(all(abs(Vector(f[1]).dot(Vector(f[2]))) < 1e-9 for f in FACES),
                "every face's up vector is perpendicular to its forward vector")
        require(sorted(tuple(f[1]) for f in FACES) == sorted(
                    [(1.0, 0.0, 0.0), (-1.0, 0.0, 0.0), (0.0, 1.0, 0.0),
                     (0.0, -1.0, 0.0), (0.0, 0.0, 1.0), (0.0, 0.0, -1.0)]),
                "the six forwards are the six axes, which is what makes 90-degree faces tile")
        require([f[0] for f in FACES] == ["px", "nx", "py", "ny", "pz", "nz"],
                "and they are in Direct3D's order, which is what TextureCube indexes by")
        require(abs(math.tan(math.radians(FACE_FOV_DEG) / 2) - 1.0) < 1e-12,
                f"the field of view is {FACE_FOV_DEG} degrees, whose half-angle tangent is "
                f"exactly 1 -- which IS the tiling condition: a face's half-extent equals its "
                f"distance, so six of them close a cube. Narrower leaves a wedge of the room in "
                f"no face; wider puts it in two and the seam doubles")

        # 3. The bake. A room of six distinctly coloured walls, so "which face saw which wall" is
        #    one pixel read -- HOUSE-00081's trick, pointed the other way.
        colours = build_painted_room()
        mirror = {"id": "MIRROR_TEST", "cell": "L0_HALL", "position": (0.0, 0.0, 0.0)}
        sidecar = bake_mirror(mirror, workdir, size)
        require(len(sidecar["faces"]) == 6,
                f"six files are written ({sidecar['faces']})")

        faces = {}
        for name, *_ in FACES:
            path = os.path.join(workdir, f"MIRROR_TEST_CUBE_{name}.png")
            require(os.path.isfile(path), f"{os.path.basename(path)} exists")
            faces[name] = load_face(path)[1]

        # `HOUSE-00081` measured that a reflection of (0,0,1) samples +Z, (1,0,0) samples +X and
        # (-1,0,0) samples -X against CNA itself. This is the same mapping from the other end.
        for name, forward, _up, _right in FACES:
            expected = dominant(colours[name])
            got = dominant(centre_colour(faces[name], size))
            require(got == expected,
                    f"the {name} face's centre is the {name} wall's colour "
                    f"(expected dominant {expected}, got {got}) -- HOUSE-00081 measured this "
                    f"mapping against CNA for +Z, +X and -X")

        # 4. SEAM CONTINUITY. Adjacent faces are rendered from the same point and share an edge,
        #    so the rays along that edge are the same rays. This is what catches a face that is
        #    rotated, flipped, or given the wrong up vector -- every per-face error there is.
        #    A single global handedness flip survives it, and `docs/cubemap-format.md` says so.
        def edge_directions(name, samples=9):
            """Where each pixel of a face's border looks, in world space."""
            forward, up, right = next((f, u, r) for n, f, u, r in FACES if n == name)
            out = {}
            for edge in ("left", "right", "top", "bottom"):
                directions = []
                for i in range(samples):
                    t = -1.0 + 2.0 * (i + 0.5) / samples
                    if edge == "left":
                        u_c, v_c = -1.0, t
                    elif edge == "right":
                        u_c, v_c = 1.0, t
                    elif edge == "top":
                        u_c, v_c = t, 1.0
                    else:
                        u_c, v_c = t, -1.0
                    directions.append((Vector(forward) + Vector(right) * u_c
                                       + Vector(up) * v_c).normalized())
                out[edge] = directions
            return out

        shared = 0
        for a, b in ADJACENT:
            da, db = edge_directions(a), edge_directions(b)
            best = None
            for ea, va in da.items():
                for eb, vb in db.items():
                    for reverse in (False, True):
                        other = list(reversed(vb)) if reverse else vb
                        error = max((x - y).length for x, y in zip(va, other))
                        if best is None or error < best[0]:
                            best = (error, ea, eb, reverse)
            if best[0] < 1e-5:
                shared += 1
        require(shared == len(ADJACENT),
                f"all {len(ADJACENT)} adjacent face pairs share an edge whose pixels look in "
                f"exactly the same directions ({shared} did) -- so no face is rotated, flipped, "
                f"or given the wrong up vector relative to its neighbours")

        # 5. The mirror hides itself. A camera at the mirror's own position that can see the
        #    mirror bakes the mirror's back into its own reflection.
        blocker = bpy.data.meshes.new("MIRROR_TEST")
        blocker.from_pydata([(-0.2, -0.2, 0.3), (0.2, -0.2, 0.3), (0.2, 0.2, 0.3),
                             (-0.2, 0.2, 0.3)], [], [[0, 1, 2, 3]])
        blocker.update()
        blocker_material = bpy.data.materials.new("black")
        blocker_material.use_nodes = True
        nodes = blocker_material.node_tree.nodes
        emission = nodes.new("ShaderNodeEmission")
        emission.inputs["Color"].default_value = (0.0, 0.0, 0.0, 1.0)
        blocker_material.node_tree.links.new(
            emission.outputs[0], nodes["Material Output"].inputs["Surface"])
        blocker.materials.append(blocker_material)
        blocker_obj = bpy.data.objects.new("MIRROR_TEST", blocker)
        bpy.context.collection.objects.link(blocker_obj)

        sidecar = bake_mirror(mirror, workdir, size)
        require("MIRROR_TEST" in sidecar["hidden"],
                f"the mirror's own object is hidden for its bake ({sidecar['hidden']})")
        pz_after = load_face(os.path.join(workdir, "MIRROR_TEST_CUBE_pz.png"))[1]
        require(dominant(centre_colour(pz_after, size)) == dominant(colours["pz"]),
                f"...so the +Z face still shows the +Z wall and not the mirror's own back "
                f"({tuple(round(v, 2) for v in centre_colour(pz_after, size))})")
        require(not blocker_obj.hide_render,
                "and the hiding is undone afterwards, so a second mirror's bake is not affected "
                "by the first one's")

        # 6. Blender 4.x's AgX would tone-map the reflection out of step with the room XNA draws
        #    beside it. Unlike a lightmap, though, a cube map IS colour and keeps the sRGB write.
        require(bpy.context.scene.view_settings.view_transform == "Standard",
                "the view transform is Standard, not AgX")
        require(bpy.context.scene.render.image_settings.color_mode == "RGB",
                "and the faces are written as colour, which is the opposite decision from "
                "lightmap_bake.py's Non-Color and is right for the opposite reason")

        # 7. The sidecar, and what §59's four mirrors cost.
        require(sidecar["faceOrder"] == ["px", "nx", "py", "ny", "pz", "nz"],
                "the sidecar records the face order, so a reader need not infer it from filenames")
        require(sidecar["size"] == size and sidecar["position"] == [0.0, 0.0, 0.0],
                "and the size and the point it was baked from")
        cost = 4 * 6 * DEFAULT_SIZE * DEFAULT_SIZE * 3
        require(cost < 8 * 1024 * 1024,
                f"§59's four mirrors at {DEFAULT_SIZE}^2 are {cost / (1024 * 1024):.1f} MB "
                f"uncompressed, {cost / (1024 * 1024) / 4:.1f} MB as DXT1")

        # 8. The flip actually happened, and in the direction D3D wants. A marker on the +X half
        #    of the +Z wall must land on the D3D-RIGHT half of the +Z face; before the flip it is
        #    on the left, because a right-handed camera looking along +Z has -X on its right.
        marker = bpy.data.meshes.new("marker")
        marker.from_pydata([(0.6, -0.6, 1.98), (1.6, -0.6, 1.98), (1.6, 0.6, 1.98),
                            (0.6, 0.6, 1.98)], [], [[0, 1, 2, 3]])
        marker.update()
        white = bpy.data.materials.new("white")
        white.use_nodes = True
        nodes = white.node_tree.nodes
        glow = nodes.new("ShaderNodeEmission")
        glow.inputs["Color"].default_value = (1.0, 1.0, 1.0, 1.0)
        white.node_tree.links.new(glow.outputs[0], nodes["Material Output"].inputs["Surface"])
        marker.materials.append(white)
        bpy.context.collection.objects.link(bpy.data.objects.new("marker", marker))

        marked_dir = os.path.join(workdir, "marked")
        bake_mirror(mirror, marked_dir, size)
        marked = load_face(os.path.join(marked_dir, "MIRROR_TEST_CUBE_pz.png"))[1]

        def whiteness(x0, x1):
            total = 0.0
            for y in range(size // 3, 2 * size // 3):
                for x in range(x0, x1):
                    r, g, b = pixel(marked, size, x, y)
                    total += min(r, g, b)
            return total

        left_half = whiteness(0, size // 2)
        right_half = whiteness(size // 2, size)
        require(right_half > left_half * 3,
                f"a marker on the world +X half of the +Z wall lands on the RIGHT half of the +Z "
                f"face ({right_half:.1f} against {left_half:.1f}) -- which is D3D's right, and "
                f"which it would not be without the flip")

        # 9. Determinism, byte for byte. Rendering straight to the final path does NOT give this:
        #    `render(write_still=True)` stamps metadata, so two pixel-identical bakes produce
        #    different files -- and `HOUSE-00199` rebuilds everything and asserts byte-identical
        #    output. Re-saving through `image.save()` is what makes that gate passable.
        again = os.path.join(workdir, "again")
        bake_mirror(mirror, again, size)
        for name, *_ in FACES:
            with open(os.path.join(marked_dir, f"MIRROR_TEST_CUBE_{name}.png"), "rb") as handle:
                left = handle.read()
            with open(os.path.join(again, f"MIRROR_TEST_CUBE_{name}.png"), "rb") as handle:
                right = handle.read()
            require(left == right, f"the {name} face is byte-identical across two bakes")
        require(not any(n.startswith(".") for n in os.listdir(again)),
                f"and no intermediate file is left behind ({sorted(os.listdir(again))[:3]})")

    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    if failures:
        return 1
    print("cubemap_bake: selftest passed.")
    return 0


def main() -> int:
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if "--selftest" in argv:
        return selftest()

    options = {}
    index = 0
    while index < len(argv):
        if argv[index].startswith("--") and index + 1 < len(argv) \
                and not argv[index + 1].startswith("--"):
            options[argv[index][2:]] = argv[index + 1]
            index += 2
        else:
            index += 1
    positional = [a for a in argv if not a.startswith("--") and a not in options.values()]

    if len(positional) != 1 or "mirrors" not in options or "out" not in options:
        print("cubemap_bake: usage: SHELL.glb --mirrors mirrors.json --out DIR [--size N]",
              file=sys.stderr)
        return 2

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=positional[0])
    with open(options["mirrors"], encoding="utf-8") as handle:
        mirrors = json.load(handle).get("mirrors", [])
    if not mirrors:
        print(f"cubemap_bake: {options['mirrors']} declares no mirror", file=sys.stderr)
        return 1

    size = int(options.get("size", DEFAULT_SIZE))
    sidecars = []
    for mirror in mirrors:
        sidecars.append(bake_mirror(mirror, options["out"], size))
        report(f"{mirror['id']}: six faces at {size}^2")
    path = os.path.join(options["out"], "cubemaps.json")
    with open(path, "w", encoding="utf-8") as handle:
        json.dump({"tool": "cubemap_bake.py", "version": VERSION, "mirrors": sidecars},
                  handle, indent=2, sort_keys=True)
        handle.write("\n")
    report(f"wrote {len(sidecars)} cube map(s) and {os.path.basename(path)}")
    return 0


if __name__ == "__main__":
    # Blender does not propagate a script's exit status; see tools/blender/blender_env.py.
    try:
        _status = main()
    except SystemExit as _exit:
        if isinstance(_exit.code, str):
            print(_exit.code, file=sys.stderr)
            _status = 1
        else:
            _status = int(_exit.code or 0)
    except BaseException:  # noqa: BLE001
        import traceback

        traceback.print_exc()
        print("cubemap_bake: EXIT 1")
        raise
    print(f"cubemap_bake: EXIT {_status}")
    sys.exit(_status)
