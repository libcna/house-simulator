#!/usr/bin/env python3
"""skin_split.py -- split a multi-skin source `.glb` into one single-skin `.glb` per skin.

`HOUSE-00224`. `cna-house.md` §47.0's rule is offline and absolute: a `.glb` entering the build has
exactly one skin, so the runtime never needs `getSkinsEXTProperty()` and ADR-0001 holds without an
exception. `HOUSE-00076` measured that **CNA enforces this for us** -- `CNA.ModelProcessor` refuses
a multi-skin glTF outright -- and that the pipeline's own `generateChildAssets: true` is the
default route. This tool is the documented fallback for sources whose generated child names are
unacceptable, and §21.3 says so.

## What a part keeps, and why each choice is not an optimisation

* **The WHOLE node graph, every joint of every skin.** The parts have to reassemble on one shared
  skeleton, evaluated once, so the same clip drives all of them. Pruning the other part's joints
  would give each part its own bone numbering and make a shared `.chanim` impossible.
* **Every animation channel that drives one of THIS part's joints**, and no others. That last
  clause is measured, not tidiness: `CNA.GltfImporter` warns *"Clip 'sway' has 1 channel(s) whose
  target node is not a joint of this skin -- they drive nothing in this palette, and are skipped"*,
  and `gltf_validate.py` treats an importer warning as an error, so a part carrying channels CNA
  will discard cannot enter the build. Nothing is lost by dropping them: CNA drops them anyway, the
  runtime's animation comes from the `.chanim` sidecar rather than `Model::Tag` (§47.0, and BL-01
  makes that `Tag` null in any case), and the attachment record NAMES every channel that was
  dropped so the omission is visible rather than silent. An animation left with no channels at all
  is dropped whole.
* **Only the meshes this skin drives**, and only the materials, textures, images and samplers those
  meshes reach.
* **Only the buffer bytes still referenced.** `p1-split-skins.py` deliberately kept the whole
  buffer and recorded that it did not prune, because its job was to establish the SHAPE of the
  split. This is the production tool, so it prunes. The fixture's parts come out at 64-65 % of the
  source rather than 100 % each, and that number is modest only because a four-vertex fixture is
  mostly JSON header; on a real character the mesh data dominates and the saving is close to the
  fraction of the geometry that went to the other part.

## The attachment record

Each part gets a `.attach.json` naming the shared skeleton root, the node it hangs from, and **its
skin's joint names in blend-index order**. `HOUSE-00074` measured that vertex blend indices are
skin-local, so that name list IS the binding — it is the same list `anim_extract.py` writes into a
`.chanim`, and the reason the runtime never asks CNA for a skins view.

## What "equivalent" is proved to mean

Not "the file loads". For every vertex of every kept mesh, the **skinned world position** is
computed from the original file and from the part, at the bind pose and at an animated pose:

    p' = sum_j  w_j * (jointWorld_j * inverseBind_j) * p

and the two must agree to floating-point noise. A split that reordered the joints, dropped an
inverse bind matrix or pruned a live accessor changes that number; a split that only renumbered
things does not.

    tools/assets/skin_split.py character.glb --out assets-src/Models/Characters
    tools/assets/skin_split.py character.glb --out <dir> --verify
    tools/assets/skin_split.py --make-fixture <dir>
    tools/assets/skin_split.py --selftest

Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import json
import math
import shutil
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gltf_io  # noqa: E402
import measure_stride  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

SplitError = gltf_io.GltfError

#: How far two skinned positions may differ and still be called equivalent. 1e-5 m is 10 microns:
#: far below anything a renderer can show and far above float32 accumulation over four influences.
EQUIVALENCE_TOLERANCE = 1e-5


# ------------------------------------------------------------------------------------ skinning ----

def _multiply(a: list[float], b: list[float]) -> list[float]:
    """Column-major 4x4 multiply, `a * b` in the column-vector convention glTF uses."""
    out = [0.0] * 16
    for column in range(4):
        for row in range(4):
            out[column * 4 + row] = sum(a[k * 4 + row] * b[column * 4 + k] for k in range(4))
    return out


def _transform(matrix: list[float], point) -> tuple[float, float, float]:
    return tuple(sum(matrix[k * 4 + row] * (point[k] if k < 3 else 1.0) for k in range(4))
                 for row in range(3))


def _trs(node: dict) -> list[float]:
    x, y, z, w = node.get("rotation", (0.0, 0.0, 0.0, 1.0))
    scale = node.get("scale", (1.0, 1.0, 1.0))
    translation = node.get("translation", (0.0, 0.0, 0.0))
    rot = [
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ]
    matrix = [0.0] * 16
    for column in range(3):
        for row in range(3):
            matrix[column * 4 + row] = rot[row][column] * scale[column]
    matrix[12], matrix[13], matrix[14] = translation[0], translation[1], translation[2]
    matrix[15] = 1.0
    return matrix


def node_world_matrices(document: dict, buffers: list[bytes], time: float | None) -> list[list[float]]:
    """Every node's world matrix, at the bind pose or at `time` of the first animation."""
    nodes = document.get("nodes", [])
    parent = [-1] * len(nodes)
    for index, node in enumerate(nodes):
        for child in node.get("children", []):
            parent[child] = index

    local = [_trs(node) for node in nodes]
    if time is not None and document.get("animations"):
        rig = measure_stride.Rig(document, buffers)
        tracks = rig.tracks(document["animations"][0])
        for index, node in enumerate(nodes):
            node_tracks = tracks.get(index, {})
            if not node_tracks:
                continue
            posed = dict(node)
            for path in ("translation", "rotation", "scale"):
                if path in node_tracks:
                    posed[path] = node_tracks[path].at(time)
            local[index] = _trs(posed)

    world: list[list[float] | None] = [None] * len(nodes)

    def resolve(index: int) -> list[float]:
        if world[index] is None:
            world[index] = (local[index] if parent[index] == -1
                            else _multiply(resolve(parent[index]), local[index]))
        return world[index]

    return [resolve(index) for index in range(len(nodes))]


def skinned_positions(document: dict, buffers: list[bytes], skin_index: int,
                      time: float | None = None,
                      world: list[list[float]] | None = None) -> list[tuple[float, float, float]]:
    """Every vertex of every mesh driven by skin `skin_index`, in world space.

    This is what "equivalent" means. It is the same arithmetic `SkinnedEffect` does on the GPU,
    written once here so a split can be checked rather than believed.

    `world` supplies the pose from OUTSIDE the file, and that is the whole point when comparing a
    part against its source. §47.0's multi-part character is "several `Model`s sharing a single
    skeleton and a single bone palette": the shared skeleton is evaluated ONCE and drives every
    part. A part evaluated from its own animation would be a different claim -- and, since the
    channels that do not reach a part's joints are pruned, a false one.
    """
    skin = document["skins"][skin_index]
    joints = skin["joints"]
    if world is None:
        world = node_world_matrices(document, buffers, time)
    inverse_binds = (gltf_io.read_accessor(document, buffers, skin["inverseBindMatrices"])
                     if "inverseBindMatrices" in skin else
                     [(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)] * len(joints))
    palette = [_multiply(world[joints[slot]], list(inverse_binds[slot]))
               for slot in range(len(joints))]

    out: list[tuple[float, float, float]] = []
    for node in document.get("nodes", []):
        if node.get("skin") != skin_index or "mesh" not in node:
            continue
        mesh = document["meshes"][node["mesh"]]
        for primitive in mesh.get("primitives", []):
            attributes = primitive["attributes"]
            positions = gltf_io.read_accessor(document, buffers, attributes["POSITION"])
            indices = gltf_io.read_accessor(document, buffers, attributes["JOINTS_0"])
            weights = gltf_io.read_accessor(document, buffers, attributes["WEIGHTS_0"])
            for vertex in range(len(positions)):
                accumulated = [0.0, 0.0, 0.0]
                for influence in range(4):
                    weight = weights[vertex][influence]
                    if weight == 0.0:
                        continue
                    slot = int(indices[vertex][influence])
                    moved = _transform(palette[slot], positions[vertex])
                    for axis in range(3):
                        accumulated[axis] += weight * moved[axis]
                # The skinning palette already carries the joints' world transforms, so the mesh
                # node's own transform is NOT applied on top -- that is what the inverse bind pose
                # removed. This is the same arithmetic `SkinnedEffect` does on the GPU.
                out.append(tuple(accumulated))
    return out


# ------------------------------------------------------------------------------------- pruning ----

def _live_accessors(document: dict) -> set[int]:
    live: set[int] = set()
    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            live.update(primitive.get("attributes", {}).values())
            if "indices" in primitive:
                live.add(primitive["indices"])
            for target in primitive.get("targets", []):
                live.update(target.values())
    for skin in document.get("skins", []):
        if "inverseBindMatrices" in skin:
            live.add(skin["inverseBindMatrices"])
    for animation in document.get("animations", []):
        for sampler in animation.get("samplers", []):
            live.add(sampler["input"])
            live.add(sampler["output"])
    return live


def prune(document: dict, buffers: list[bytes]) -> tuple[dict, bytes]:
    """Drop every accessor, bufferView, image and buffer byte nothing reaches, and renumber.

    The probe deliberately skipped this and said so; a production splitter that shipped the whole
    source buffer in every part would make a two-part character cost twice what it did before.
    """
    keep = sorted(_live_accessors(document))
    remap = {old: new for new, old in enumerate(keep)}

    blob = bytearray()
    views: list[dict] = []
    view_remap: dict[int, int] = {}

    def append(payload: bytes, stride: int | None, target: int | None) -> int:
        while len(blob) % 4:
            blob.append(0)
        view = {"buffer": 0, "byteOffset": len(blob), "byteLength": len(payload)}
        if stride is not None:
            view["byteStride"] = stride
        if target is not None:
            view["target"] = target
        blob.extend(payload)
        views.append(view)
        return len(views) - 1

    def keep_view(index: int) -> int:
        if index not in view_remap:
            view = document["bufferViews"][index]
            start = view.get("byteOffset", 0)
            data = buffers[view.get("buffer", 0)][start:start + view["byteLength"]]
            view_remap[index] = append(data, view.get("byteStride"), view.get("target"))
        return view_remap[index]

    accessors = []
    for old in keep:
        accessor = dict(document["accessors"][old])
        if accessor.get("bufferView") is not None:
            accessor["bufferView"] = keep_view(accessor["bufferView"])
        accessors.append(accessor)

    images = []
    for image in document.get("images", []):
        image = dict(image)
        if "bufferView" in image:
            image["bufferView"] = keep_view(image["bufferView"])
        images.append(image)

    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            primitive["attributes"] = {k: remap[v] for k, v in primitive["attributes"].items()}
            if "indices" in primitive:
                primitive["indices"] = remap[primitive["indices"]]
            if "targets" in primitive:
                primitive["targets"] = [{k: remap[v] for k, v in t.items()}
                                        for t in primitive["targets"]]
    for skin in document.get("skins", []):
        if "inverseBindMatrices" in skin:
            skin["inverseBindMatrices"] = remap[skin["inverseBindMatrices"]]
    for animation in document.get("animations", []):
        for sampler in animation.get("samplers", []):
            sampler["input"] = remap[sampler["input"]]
            sampler["output"] = remap[sampler["output"]]

    document["accessors"] = accessors
    document["bufferViews"] = views
    if images:
        document["images"] = images
    document["buffers"] = [{"byteLength": len(blob)}] if blob else []
    return document, bytes(blob)


def _prune_materials(document: dict) -> None:
    """Keep only the materials the kept primitives reach, and the textures those materials reach."""
    used = sorted({primitive["material"] for mesh in document.get("meshes", [])
                   for primitive in mesh.get("primitives", []) if "material" in primitive})
    if not document.get("materials"):
        return
    remap = {old: new for new, old in enumerate(used)}
    materials = [document["materials"][old] for old in used]

    texture_refs: list[dict] = []
    for material in materials:
        pbr = material.get("pbrMetallicRoughness", {})
        for key in ("baseColorTexture", "metallicRoughnessTexture"):
            if key in pbr:
                texture_refs.append(pbr[key])
        for key in ("normalTexture", "occlusionTexture", "emissiveTexture"):
            if key in material:
                texture_refs.append(material[key])

    used_textures = sorted({ref["index"] for ref in texture_refs})
    texture_remap = {old: new for new, old in enumerate(used_textures)}
    textures = [document.get("textures", [])[old] for old in used_textures]
    for ref in texture_refs:
        ref["index"] = texture_remap[ref["index"]]

    used_images = sorted({t["source"] for t in textures if "source" in t})
    image_remap = {old: new for new, old in enumerate(used_images)}
    images = [document.get("images", [])[old] for old in used_images]
    used_samplers = sorted({t["sampler"] for t in textures if "sampler" in t})
    sampler_remap = {old: new for new, old in enumerate(used_samplers)}
    samplers = [document.get("samplers", [])[old] for old in used_samplers]
    for texture in textures:
        if "source" in texture:
            texture["source"] = image_remap[texture["source"]]
        if "sampler" in texture:
            texture["sampler"] = sampler_remap[texture["sampler"]]

    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            if "material" in primitive:
                primitive["material"] = remap[primitive["material"]]

    document["materials"] = materials
    for key, value in (("textures", textures), ("images", images), ("samplers", samplers)):
        if value:
            document[key] = value
        else:
            document.pop(key, None)


# --------------------------------------------------------------------------------------- split ----

def split(path: Path, out: Path, prefix: str | None = None) -> list[dict]:
    document, blob = gltf_io.read_model(path)
    buffers = gltf_io.buffer_bytes(document, blob, path.parent)
    skins = document.get("skins", [])
    if len(skins) < 2:
        raise SplitError(f"{path.name} has {len(skins)} skin(s); there is nothing to split. A "
                         f"single-skin file already satisfies cna-house.md §47.0.")

    nodes = document.get("nodes", [])
    scene = document.get("scenes", [{}])[document.get("scene", 0)]
    roots = scene.get("nodes", [])
    skeleton_root = nodes[roots[0]].get("name") if roots else None
    base = prefix if prefix is not None else path.stem
    out.mkdir(parents=True, exist_ok=True)

    records = []
    for index, skin in enumerate(skins):
        part = json.loads(json.dumps(document))
        mesh_nodes = [i for i, node in enumerate(part["nodes"])
                      if node.get("skin") == index and "mesh" in node]
        if not mesh_nodes:
            raise SplitError(f"{path.name}: skin {index} "
                             f"('{skin.get('name', index)}') drives no mesh, so a part for it "
                             f"would be empty")

        mesh_ids = [part["nodes"][i]["mesh"] for i in mesh_nodes]
        mesh_remap = {old: new for new, old in enumerate(sorted(set(mesh_ids)))}
        part["meshes"] = [document["meshes"][old] for old in sorted(set(mesh_ids))]
        part["skins"] = [json.loads(json.dumps(skin))]

        # EVERY node is kept -- the shared skeleton is what lets one clip drive both parts. What is
        # dropped is the other skins' mesh and skin BINDINGS, not the nodes themselves.
        for i, node in enumerate(part["nodes"]):
            if i in mesh_nodes:
                node["skin"] = 0
                node["mesh"] = mesh_remap[node["mesh"]]
            else:
                node.pop("skin", None)
                node.pop("mesh", None)

        skin_joints = set(skin["joints"])
        dropped_channels: list[str] = []
        kept_animations = []
        for animation in part.get("animations", []):
            channels = []
            samplers = []
            for channel in animation.get("channels", []):
                target = channel.get("target", {})
                node = target.get("node")
                if node is not None and node not in skin_joints:
                    dropped_channels.append(
                        f"{animation.get('name', '')}:{nodes[node].get('name', node)}."
                        f"{target.get('path', '')}")
                    continue
                sampler = animation["samplers"][channel["sampler"]]
                samplers.append(sampler)
                channels.append({**channel, "sampler": len(samplers) - 1})
            if channels:
                kept_animations.append({**animation, "channels": channels, "samplers": samplers})
        if kept_animations:
            part["animations"] = kept_animations
        else:
            part.pop("animations", None)

        _prune_materials(part)
        part, part_blob = prune(part, buffers)
        part.setdefault("asset", {})["generator"] = "cna-house tools/assets/skin_split.py"

        name = f"{base}_{skin.get('name', f'skin{index}')}"
        target = out / f"{name}.glb"
        gltf_io.write_glb(target, part, part_blob)

        record = {
            "part": name,
            "file": target.name,
            "source": path.name,
            "skin": skin.get("name", f"skin{index}"),
            "skinIndex": index,
            # The record's whole point: every part hangs off the same skeleton, so one evaluated
            # pose drives all of them and one `.chanim` describes all of them.
            "skeletonRoot": skeleton_root,
            "attachmentBone": (nodes[skin["skeleton"]].get("name") if "skeleton" in skin
                               else skeleton_root),
            # `HOUSE-00074`: blend indices are SKIN-LOCAL, so this list is the binding.
            "joints": [nodes[j].get("name", f"node{j}") for j in skin["joints"]],
            "meshNodes": [nodes[i].get("name", f"node{i}") for i in mesh_nodes],
            "bufferPruned": True,
            # NAMED, not merely counted. These are the channels CNA's importer would have skipped
            # with a warning; recording which ones went is what stops "the tail stopped following
            # the body" being an unexplained observation six months from now.
            "droppedChannels": sorted(dropped_channels),
            "sourceBytes": path.stat().st_size,
            "partBytes": target.stat().st_size,
        }
        (out / f"{name}.attach.json").write_text(
            json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        records.append(record)
    return records


def verify(path: Path, parts: list[Path], records: list[dict]) -> list[str]:
    """Every part's skinned vertices must match the source's, at rest and in motion."""
    problems: list[str] = []
    document, blob = gltf_io.read_model(path)
    buffers = gltf_io.buffer_bytes(document, blob, path.parent)

    times: list[float | None] = [None]
    if document.get("animations"):
        # Rest is not enough. A split that dropped an animation sampler, or renumbered one wrongly,
        # produces a part that is exactly right until it moves.
        sampler = document["animations"][0]["samplers"][0]
        keys = [v[0] for v in gltf_io.read_accessor(document, buffers, sampler["input"])]
        times.append(keys[0] + 0.5 * (keys[-1] - keys[0]))

    for record, part_path in zip(records, parts):
        part_document, part_blob = gltf_io.read_model(part_path)
        part_buffers = gltf_io.buffer_bytes(part_document, part_blob, part_path.parent)

        if len(part_document.get("skins", [])) != 1:
            problems.append(f"{part_path.name} declares "
                            f"{len(part_document.get('skins', []))} skins, not one")
            continue
        source_joints = [document["nodes"][j].get("name")
                         for j in document["skins"][record["skinIndex"]]["joints"]]
        part_joints = [part_document["nodes"][j].get("name")
                       for j in part_document["skins"][0]["joints"]]
        if source_joints != part_joints:
            problems.append(f"{part_path.name}: the joint order changed, so every blend index now "
                            f"means a different bone\n    was {source_joints}\n    now {part_joints}")
            continue

        for time in times:
            # BOTH posed by the SOURCE's skeleton. That is the runtime arrangement (§47.0: one
            # skeleton, one palette, several models) and it is the only comparison that means
            # anything once a part's out-of-skin channels have been pruned.
            pose = node_world_matrices(document, buffers, time)
            expected = skinned_positions(document, buffers, record["skinIndex"], time, pose)
            actual = skinned_positions(part_document, part_buffers, 0, time, pose)
            when = "at the bind pose" if time is None else f"at t = {time:.3f}"
            if len(expected) != len(actual):
                problems.append(f"{part_path.name}: {len(actual)} skinned vertices {when}, "
                                f"not {len(expected)}")
                continue
            worst = max((math.dist(a, b) for a, b in zip(expected, actual)), default=0.0)
            if worst > EQUIVALENCE_TOLERANCE:
                problems.append(f"{part_path.name}: a skinned vertex moves {worst:.6f} m {when}, "
                                f"above the {EQUIVALENCE_TOLERANCE} m tolerance")
    return problems


# ------------------------------------------------------------------------------------ fixture ----

def make_fixture(directory: Path) -> Path:
    """A two-skin character: a body on three joints and a tail on two, sharing one root.

    Deliberately shaped so a wrong split is visible rather than merely different:

    * the two skins share the ROOT joint and nothing else, so a part that pruned the shared
      skeleton would move under animation while its own joints stayed put;
    * skin B's joints are listed in an order that is NOT their node order, so a splitter that
      rebuilt the list from the node graph produces a different binding;
    * each mesh has its own material and its own embedded texture, so material pruning has
      something to prune and something it must not;
    * one animation rotates the shared root, so the parts can only agree if both still see it.
    """
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / "character.glb"

    # Nodes: 0 Root, 1 Spine, 2 Head, 3 TailA, 4 TailB, 5 BodyMesh, 6 TailMesh
    names = ["Root", "Spine", "Head", "TailA", "TailB", "BodyMesh", "TailMesh"]
    children = {0: [1, 3, 5, 6], 1: [2], 3: [4]}
    rest = [(0.0, 0.0, 0.0), (0.0, 0.6, 0.0), (0.0, 0.5, 0.0),
            (0.0, 0.1, -0.3), (0.0, 0.0, -0.4), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)]

    blob = bytearray()
    views: list[dict] = []
    accessors: list[dict] = []

    def add(payload: bytes, accessor: dict) -> int:
        while len(blob) % 4:
            blob.append(0)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(payload)})
        blob.extend(payload)
        accessor["bufferView"] = len(views) - 1
        accessors.append(accessor)
        return len(accessors) - 1

    def mesh_accessors(points, joint_slots):
        a_pos = add(b"".join(struct.pack("<3f", *p) for p in points),
                    {"componentType": 5126, "count": len(points), "type": "VEC3",
                     "min": [min(p[i] for p in points) for i in range(3)],
                     "max": [max(p[i] for p in points) for i in range(3)]})
        a_joints = add(b"".join(struct.pack("<4H", *slots) for slots in joint_slots),
                       {"componentType": 5123, "count": len(points), "type": "VEC4"})
        a_weights = add(b"".join(struct.pack("<4f", 1.0, 0.0, 0.0, 0.0) for _ in points),
                        {"componentType": 5126, "count": len(points), "type": "VEC4"})
        a_index = add(struct.pack("<6H", 0, 1, 2, 0, 2, 3),
                      {"componentType": 5123, "count": 6, "type": "SCALAR",
                       "min": [0], "max": [3]})
        return a_pos, a_joints, a_weights, a_index

    body = mesh_accessors([(-0.2, 0.0, 0.0), (0.2, 0.0, 0.0), (0.2, 1.2, 0.0), (-0.2, 1.2, 0.0)],
                          [(0, 0, 0, 0), (1, 0, 0, 0), (2, 0, 0, 0), (2, 0, 0, 0)])
    tail = mesh_accessors([(-0.05, 0.1, -0.3), (0.05, 0.1, -0.3),
                           (0.05, 0.0, -0.8), (-0.05, 0.0, -0.8)],
                          [(0, 0, 0, 0), (0, 0, 0, 0), (1, 0, 0, 0), (1, 0, 0, 0)])

    def world_of(node: int) -> tuple[float, float, float]:
        parent = next((p for p, kids in children.items() if node in kids), None)
        base = world_of(parent) if parent is not None else (0.0, 0.0, 0.0)
        return tuple(base[i] + rest[node][i] for i in range(3))

    def inverse_binds(joint_nodes):
        payload = bytearray()
        for node in joint_nodes:
            position = world_of(node)
            payload += b"".join(struct.pack("<f", v) for v in
                                [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
                                 -position[0], -position[1], -position[2], 1])
        return add(bytes(payload),
                   {"componentType": 5126, "count": len(joint_nodes), "type": "MAT4"})

    body_joints = [0, 1, 2]
    # NOT node order: TailB before TailA. A splitter that rebuilt the list from the node graph
    # would silently reverse the binding, and the fixture exists to catch exactly that.
    tail_joints = [4, 3]
    a_body_ibm = inverse_binds(body_joints)
    a_tail_ibm = inverse_binds(tail_joints)

    times = [0.0, 0.5, 1.0]
    angle = math.radians(25.0)
    rotations = [(0.0, 0.0, 0.0, 1.0),
                 (0.0, math.sin(angle / 2), 0.0, math.cos(angle / 2)),
                 (0.0, 0.0, 0.0, 1.0)]
    a_time = add(b"".join(struct.pack("<f", t) for t in times),
                 {"componentType": 5126, "count": len(times), "type": "SCALAR",
                  "min": [0.0], "max": [1.0]})
    a_rot = add(b"".join(struct.pack("<4f", *r) for r in rotations),
                {"componentType": 5126, "count": len(rotations), "type": "VEC4"})

    # Two small embedded textures, one per material, so material pruning has real work.
    import zlib

    def png(colour):
        raw = b"".join(b"\x00" + bytes(colour) * 4 for _ in range(4))

        def chunk(tag, data):
            body_bytes = tag + data
            return (struct.pack(">I", len(data)) + body_bytes
                    + struct.pack(">I", zlib.crc32(body_bytes) & 0xFFFFFFFF))

        return (b"\x89PNG\r\n\x1a\n"
                + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 6, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))

    image_views = []
    for colour in ((200, 60, 60, 255), (60, 60, 200, 255)):
        data = png(colour)
        while len(blob) % 4:
            blob.append(0)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(data)})
        blob.extend(data)
        image_views.append(len(views) - 1)

    nodes = []
    for index, name in enumerate(names):
        node: dict = {"name": name, "translation": list(rest[index])}
        if index in children:
            node["children"] = children[index]
        nodes.append(node)
    nodes[5].update({"mesh": 0, "skin": 0})
    nodes[6].update({"mesh": 1, "skin": 1})

    document = {
        "asset": {"version": "2.0", "generator": "cna-house skin_split.py --make-fixture"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": nodes,
        "skins": [
            {"name": "Body", "joints": body_joints, "inverseBindMatrices": a_body_ibm,
             "skeleton": 0},
            {"name": "Tail", "joints": tail_joints, "inverseBindMatrices": a_tail_ibm,
             "skeleton": 3},
        ],
        "meshes": [
            {"name": "Body", "primitives": [
                {"attributes": {"POSITION": body[0], "JOINTS_0": body[1], "WEIGHTS_0": body[2]},
                 "indices": body[3], "mode": 4, "material": 0}]},
            {"name": "Tail", "primitives": [
                {"attributes": {"POSITION": tail[0], "JOINTS_0": tail[1], "WEIGHTS_0": tail[2]},
                 "indices": tail[3], "mode": 4, "material": 1}]},
        ],
        "materials": [
            {"name": "BodyMat", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}},
            {"name": "TailMat", "pbrMetallicRoughness": {"baseColorTexture": {"index": 1}}},
        ],
        "textures": [{"sampler": 0, "source": 0}, {"sampler": 0, "source": 1}],
        "images": [{"bufferView": image_views[0], "mimeType": "image/png"},
                   {"bufferView": image_views[1], "mimeType": "image/png"}],
        "samplers": [{"wrapS": 10497, "wrapT": 10497}],
        "animations": [{"name": "sway", "channels": [
            {"sampler": 0, "target": {"node": 0, "path": "rotation"}}],
            "samplers": [{"input": a_time, "output": a_rot, "interpolation": "LINEAR"}]}],
        "accessors": accessors, "bufferViews": views, "buffers": [{"byteLength": len(blob)}],
    }
    gltf_io.write_glb(path, document, bytes(blob))
    return path


# ----------------------------------------------------------------------------------- selftest ----

def selftest() -> int:
    failures = 0

    def require(condition: bool, message: str) -> None:
        nonlocal failures
        if condition:
            print(f"  {message}")
        else:
            print(f"  SELFTEST FAILED: {message}", file=sys.stderr)
            failures += 1

    workspace = Path(tempfile.mkdtemp(prefix="skin_split_selftest_"))
    try:
        source = make_fixture(workspace / "src")
        out = workspace / "parts"
        records = split(source, out)
        parts = [out / record["file"] for record in records]

        require(len(records) == 2, f"a two-skin source produces {len(records)} part(s)")

        # 1. THE claim: every part is single-skin. §47.0's rule is what this tool exists for.
        for part in parts:
            document, _ = gltf_io.read_model(part)
            require(len(document.get("skins", [])) == 1,
                    f"{part.name} declares {len(document.get('skins', []))} skin(s)")
            require(len(document.get("meshes", [])) == 1,
                    f"{part.name} keeps only its own mesh "
                    f"({len(document.get('meshes', []))})")
            require(len(document.get("nodes", [])) == 7,
                    f"{part.name} keeps the WHOLE node graph so the parts share one skeleton "
                    f"({len(document.get('nodes', []))} nodes)")
        body_document, _ = gltf_io.read_model(out / "character_Body.glb")
        require(len(body_document.get("animations", [])) == 1,
                "the body part keeps the animation: the shared Root IS one of its joints")
        tail_document, _ = gltf_io.read_model(out / "character_Tail.glb")
        require("animations" not in tail_document,
                "the tail part drops the animation entirely: its only channel targets Root, which "
                "is NOT one of the tail's joints, and CNA would discard it with a warning that "
                "gltf_validate treats as an error")
        tail_record = next(r for r in records if r["skin"] == "Tail")
        require(tail_record["droppedChannels"] == ["sway:Root.rotation"],
                f"...and the record NAMES what was dropped: {tail_record['droppedChannels']}")
        body_record = next(r for r in records if r["skin"] == "Body")
        require(body_record["droppedChannels"] == [],
                f"the body part drops nothing ({body_record['droppedChannels']})")

        # 2. Transform equivalence -- the claim that "equivalent" is checked rather than believed.
        #    Both sides are posed by the SOURCE's shared skeleton, which is the runtime arrangement.
        problems = verify(source, parts, records)
        require(not problems, f"every part's skinned vertices match the source's under the SHARED "
                              f"skeleton's pose, at rest and in motion ({len(problems)} problem(s))")
        for problem in problems:
            print(f"      {problem}", file=sys.stderr)

        # 3. The joint order is preserved EXACTLY, including the fixture's deliberately
        #    out-of-node-order tail. A splitter that rebuilt the list from the node graph would
        #    reverse it, and every blend index would then mean a different bone.
        tail = next(r for r in records if r["skin"] == "Tail")
        require(tail["joints"] == ["TailB", "TailA"],
                f"the tail's joint order survives unchanged: {tail['joints']}")
        require(tail["attachmentBone"] == "TailA",
                f"the tail's attachment bone is recorded ({tail['attachmentBone']})")
        require(all(r["skeletonRoot"] == "Root" for r in records),
                "both parts record the same shared skeleton root")

        # 4. Pruning is real: each part is much smaller than the source, and the materials and
        #    textures the other part used are gone.
        for record in records:
            require(record["partBytes"] < record["sourceBytes"] * 0.7,
                    f"{record['file']} is {record['partBytes']} bytes against the source's "
                    f"{record['sourceBytes']} ({100 * record['partBytes'] / record['sourceBytes']:.0f} %)")
        document, _ = gltf_io.read_model(parts[0])
        require(len(document.get("materials", [])) == 1
                and document["materials"][0]["name"] == "BodyMat",
                f"the body part keeps one material, its own "
                f"({[m['name'] for m in document.get('materials', [])]})")
        require(len(document.get("images", [])) == 1,
                f"...and one image ({len(document.get('images', []))})")

        # 5. Determinism.
        again = split(source, workspace / "parts2")
        require(all((out / r["file"]).read_bytes() == (workspace / "parts2" / r["file"]).read_bytes()
                    for r in again),
                "a second split writes byte-identical parts")

        # 6. A single-skin file is refused, with the reason, rather than silently copied.
        try:
            split(parts[0], workspace / "nope")
            require(False, "a single-skin file is refused")
        except SplitError as error:
            require("nothing to split" in str(error),
                    f"a single-skin file is refused: {error}")

        # 7. `verify()` must FAIL on a broken part, or it is decoration. Reversing a part's joint
        #    list is the exact mistake the fixture's out-of-order tail is shaped to catch.
        damaged = workspace / "damaged"
        damaged.mkdir()
        document, blob = gltf_io.read_model(parts[1])
        document["skins"][0]["joints"] = list(reversed(document["skins"][0]["joints"]))
        gltf_io.write_glb(damaged / parts[1].name, document, blob)
        broken = verify(source, [damaged / parts[1].name], [records[1]])
        require(any("joint order changed" in p for p in broken),
                f"verify() catches a reversed joint list ({broken})")

        # ...and a part whose INVERSE BIND matrices are wrong, which the joint-name check cannot
        # see. That is the failure that matters here: since the pose comes from the shared
        # skeleton, a part's own node translations are overridden and genuinely do not affect the
        # result -- an earlier version of this injection moved one and correctly found nothing.
        document, blob = gltf_io.read_model(parts[0])
        accessor = document["accessors"][document["skins"][0]["inverseBindMatrices"]]
        view = document["bufferViews"][accessor["bufferView"]]
        mutable = bytearray(blob)
        # Element 13 of the first matrix is its Y translation: shift the whole skin 0.3 m.
        offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0) + 13 * 4
        (original,) = struct.unpack_from("<f", mutable, offset)
        struct.pack_into("<f", mutable, offset, original + 0.3)
        gltf_io.write_glb(damaged / parts[0].name, document, bytes(mutable))
        moved = verify(source, [damaged / parts[0].name], [records[0]])
        require(any("skinned vertex moves" in p for p in moved),
                f"verify() catches a corrupted inverse bind matrix ({moved})")

        # ...and a part whose vertex positions have moved.
        document, blob = gltf_io.read_model(parts[0])
        accessor = document["accessors"][document["meshes"][0]["primitives"][0]
                                         ["attributes"]["POSITION"]]
        view = document["bufferViews"][accessor["bufferView"]]
        mutable = bytearray(blob)
        struct.pack_into("<f", mutable, view.get("byteOffset", 0), 9.0)
        gltf_io.write_glb(damaged / parts[0].name, document, bytes(mutable))
        bent = verify(source, [damaged / parts[0].name], [records[0]])
        require(any("skinned vertex moves" in p for p in bent),
                f"verify() catches a moved vertex ({bent})")

    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    if failures:
        return 1
    print("skin_split: selftest passed.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("model", nargs="?", type=Path)
    parser.add_argument("--out", type=Path, help="where the parts are written")
    parser.add_argument("--prefix", help="part name prefix; the source's stem by default")
    parser.add_argument("--verify", action="store_true",
                        help="check every part's skinned vertices against the source's")
    parser.add_argument("--manifest", action="store_true",
                        help="record each part's attachment in assets-src/assets.manifest.json")
    parser.add_argument("--make-fixture", type=Path, metavar="DIR")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if args.make_fixture is not None:
        path = make_fixture(args.make_fixture)
        print(f"{path}  {path.stat().st_size} bytes")
        return 0
    if args.model is None or args.out is None:
        parser.print_help()
        return 2

    try:
        records = split(args.model, args.out, args.prefix)
    except SplitError as error:
        print(f"skin_split: {error}", file=sys.stderr)
        return 1

    for record in records:
        print(f"{record['file']:<32} {record['partBytes']:>8,} bytes  "
              f"({100 * record['partBytes'] / record['sourceBytes']:.0f} % of the source)  "
              f"attaches at {record['attachmentBone']}")
        print(f"  joints  {', '.join(record['joints'])}")

    if args.manifest:
        # UPDATES rows, never creates them. A row carries a licence, an origin and four permission
        # booleans that a splitter cannot know; provenance comes first and the attachment second.
        import manifest as manifest_tool

        document = manifest_tool.load()
        by_file = {row.get("sourceFile", ""): row for row in document["assets"]}
        missing = []
        for record in records:
            relative = str((args.out / record["file"]).resolve().relative_to(REPO)) \
                if (args.out / record["file"]).resolve().is_relative_to(REPO) else ""
            row = by_file.get(relative)
            if row is None:
                missing.append(relative or record["file"])
                continue
            row["attachment"] = {"source": record["source"],
                                 "skeletonRoot": record["skeletonRoot"],
                                 "attachmentBone": record["attachmentBone"],
                                 "joints": record["joints"]}
            print(f"  manifest: recorded {record['file']}'s attachment at "
                  f"{record['attachmentBone']}")
        problems = manifest_tool.validate(document)
        if problems:
            print("skin_split: the manifest would be invalid; nothing was written:", file=sys.stderr)
            for problem in problems:
                print(f"  {problem}", file=sys.stderr)
            return 1
        manifest_tool.save(document)
        for name in missing:
            print(f"skin_split: {name} has no manifest row yet; add one with "
                  f"tools/assets/manifest.py add before recording its attachment", file=sys.stderr)
        if missing:
            return 1

    if args.verify:
        problems = verify(args.model, [args.out / r["file"] for r in records], records)
        for problem in problems:
            print(f"skin_split: {problem}", file=sys.stderr)
        if problems:
            return 1
        print("skin_split: every part's skinned vertices match the source's, at rest and in motion.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
