#!/usr/bin/env python3
"""Check committed project-authored kitchen built-ins and fridge against fresh Blender exports.

No network or third-party model cache is involved. The temporary Blender products are generated
under /tmp; no compiler or CMake build is run there. `--check` compares final collidable GLBs and
their manifest SHA-256 identities, making the source authoring script durable rather than a one-off
asset dump.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "blender"))
import blender_env  # noqa: E402
import gltf_io  # noqa: E402
import scale_check  # noqa: E402
sys.path.insert(0, str(REPO / "tools" / "world"))
import layout_io  # noqa: E402

NAMES = ("north_run", "island", "range_wall")
SCRIPT = REPO / "tools" / "blender" / "kitchen_builtins.py"
FRIDGE_SCRIPT = REPO / "tools" / "blender" / "kitchen_refrigerator.py"
COLLISION = REPO / "tools" / "blender" / "collision_proxy.py"
MODELS = REPO / "assets-src" / "Models" / "Furniture" / "Kitchen"
MANIFEST = REPO / "assets-src" / "assets.manifest.json"
ASSET_IDS = {"north_run": "MODEL_KITCHEN_NORTH_BASE_RUN",
             "island": "MODEL_KITCHEN_ISLAND",
             "range_wall": "MODEL_KITCHEN_RANGE_WALL"}
FRIDGE_ID = "MODEL_KITCHEN_REFRIGERATOR"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def checked_run(command: list[str], *, environment=None, sentinel: str | None = None) -> None:
    result = subprocess.run(command, cwd=REPO, env=environment,
                            capture_output=True, text=True, check=False)
    if result.returncode != 0 or (sentinel and sentinel not in result.stdout.splitlines()):
        tail = "\n".join((result.stdout + "\n" + result.stderr).splitlines()[-28:])
        raise RuntimeError(f"{command[0]} failed (exit {result.returncode}):\n{tail}")


def check_manifest_geometry(path: Path, row: dict) -> None:
    document, _ = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError(f"{path.name}: no authoritative POSITION bounds")
    measured = [high - low for low, high in zip(*bounds)]
    declared = row["geometry"]["boundsMetres"]
    if any(abs(actual - stored) > 0.005 for actual, stored in zip(measured, declared)):
        raise RuntimeError(f"{path.name}: geometry.boundsMetres {declared} != {measured}")
    visible = sum(document["accessors"][document["meshes"][node["mesh"]]
                  ["primitives"][0]["indices"]]["count"] // 3
                  for node in document["nodes"] if "mesh" in node and
                  not node.get("name", "").endswith("_COL"))
    if visible != row["geometry"]["triangles"]["LOD0"]:
        raise RuntimeError(f"{path.name}: declared LOD0 triangles do not equal {visible}")
    if row["geometry"]["collision"] not in {node.get("name") for node in document["nodes"]}:
        raise RuntimeError(f"{path.name}: declared collision proxy node is missing")
    stone_tops = [document["accessors"][primitive["attributes"]["POSITION"]]["max"][1]
                  for mesh in document["meshes"] for primitive in mesh["primitives"]
                  if "material" in primitive and
                  document["materials"][primitive["material"]]["name"] == "CAB_STONE"]
    if not stone_tops:
        raise RuntimeError(f"{path.name}: working-surface stone is missing")
    actual_top = max(stone_tops)
    expected_top = row["geometry"].get("counterHeightMetres", 0.94)
    if abs(actual_top - expected_top) > 0.005:
        raise RuntimeError(
            f"{path.name}: stone top is {actual_top:.3f} m, manifest says "
            f"{expected_top:.3f} m")


def check_range_wall_geometry(path: Path, row: dict) -> None:
    """Keep the measured cooking wall recognizable, not merely manifest-valid."""
    document, _ = gltf_io.read_model(path)
    visible_nodes = [node for node in document["nodes"] if "mesh" in node and
                     not node.get("name", "").endswith("_COL")]
    names = {node.get("name", "") for node in visible_nodes}
    required_fragments = ("range_body", "oven_glass", "backsplash", "hood_canopy",
                          "hood_chimney", "range_scribe_-1", "range_scribe_1")
    for fragment in required_fragments:
        if not any(fragment in name for name in names):
            raise RuntimeError(f"range_wall: missing authored {fragment} component")
    if sum("burner_" in name for name in names) != 4:
        raise RuntimeError("range_wall: expected four separately modelled burners")
    materials = {document["materials"][primitive["material"]]["name"]
                 for node in visible_nodes
                 for primitive in document["meshes"][node["mesh"]]["primitives"]
                 if "material" in primitive}
    expected = {"CAB_PAINT", "CAB_STONE", "CAB_STEEL", "CAB_TILE",
                "CAB_OVEN_GLASS"}
    if materials != expected:
        raise RuntimeError(f"range_wall: unexpected source finishes {sorted(materials)}")
    layout = layout_io.load_layout(REPO / "assets-src" / "world", ["props"])
    prop = layout_io.by_id(layout_io.rows(layout, "props"), "prop")[
        "PROP_L0_KITCHEN_RANGE_WALL"]
    if (prop["asset"] != "MODEL_KITCHEN_RANGE_WALL" or
            prop["cell"] != "L0_KITCHEN" or
            prop["position"] != [-7.75, 0.6, -25.075] or prop["yawDeg"] != 270 or
            prop["collision"] != "proxy"):
        raise RuntimeError("range_wall: canonical prop no longer fits the west working wall")


def check_refrigerator_geometry(path: Path, row: dict) -> None:
    document, blob = gltf_io.read_model(path)
    bounds = scale_check.accessor_bounds(document)
    if bounds is None:
        raise RuntimeError("refrigerator: no authoritative POSITION bounds")
    measured = [high - low for low, high in zip(*bounds)]
    if any(abs(actual - stored) > 0.005 for actual, stored in
           zip(measured, row["geometry"]["boundsMetres"])):
        raise RuntimeError(f"refrigerator: declared bounds != actual {measured}")
    if abs(measured[0] - 1.80) > 0.005 or abs(measured[1] - 2.70) > 0.005:
        raise RuntimeError(f"refrigerator: appliance bay/ceiling mismatched {measured}")
    appliance_tops = [document["accessors"][primitive["attributes"]["POSITION"]]
                      ["max"][1] for node in document["nodes"] if "mesh" in node and
                      "bridge_" not in node.get("name", "") and
                      not node.get("name", "").endswith("_COL")
                      for primitive in document["meshes"][node["mesh"]]["primitives"]]
    if not appliance_tops or abs(max(appliance_tops) - 1.95) > 0.005 or abs(
            max(appliance_tops) - row["geometry"]["applianceHeightMetres"]) > 0.005:
        raise RuntimeError("refrigerator: appliance itself differs from 1.95 m shell/manifest")
    visible = [primitive for node in document["nodes"] if "mesh" in node and
               not node.get("name", "").endswith("_COL")
               for primitive in document["meshes"][node["mesh"]]["primitives"]]
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for primitive in visible)
    if triangles != row["geometry"]["triangles"]["LOD0"]:
        raise RuntimeError(f"refrigerator: declared triangles != actual {triangles}")
    if not all("TEXCOORD_0" in primitive["attributes"] for primitive in visible):
        raise RuntimeError("refrigerator: a close-range panel lacks physical UV0")
    buffers = gltf_io.buffer_bytes(document, blob, path.parent)
    enamel_fronts = [node for node in document["nodes"] if "mesh" in node and
                     "_enamel_face" in node.get("name", "")]
    if len(enamel_fronts) != 2:
        raise RuntimeError("refrigerator: expected two independently authored closed fronts")
    for node in enamel_fronts:
        primitive = document["meshes"][node["mesh"]]["primitives"][0]
        uv = gltf_io.read_accessor(document, buffers,
                                   primitive["attributes"]["TEXCOORD_0"])
        span = [max(value[i] for value in uv) - min(value[i] for value in uv)
                for i in (0, 1)]
        if span[0] < 1.5 or span[1] < 3.0:
            raise RuntimeError(
                f"refrigerator: close front UV0 is stretched, not metre-tiled: {span}")
    materials = {document["materials"][primitive["material"]]["name"]
                 for primitive in visible if "material" in primitive}
    if materials != {"CAB_PAINT", "CAB_STEEL", "CAB_OAK"}:
        raise RuntimeError(f"refrigerator: unexpected approved finishes {materials}")
    if row["geometry"]["collision"] not in {node.get("name") for node in
                                              document["nodes"]}:
        raise RuntimeError("refrigerator: declared closed-appliance proxy missing")
    # The yaw=180 placement takes local -Z toward the kitchen (+Z). The assembled
    # centre must be the actual origin even though the door pulls protrude.
    if abs((bounds[0][2] + bounds[1][2]) * 0.5) > 0.005:
        raise RuntimeError(f"refrigerator: off-axis support origin {bounds}")
    layout = layout_io.load_layout(REPO / "assets-src" / "world",
                                   ["props", "openings", "portals"])
    prop = layout_io.by_id(layout_io.rows(layout, "props"), "prop")[
        "PROP_L0_KITCHEN_REFRIGERATOR"]
    if (prop["asset"] != FRIDGE_ID or prop["cell"] != "L0_KITCHEN" or
            prop["position"] != [1.2, 0.6, -26.6035] or prop["yawDeg"] != 180 or
            prop["collision"] != "proxy"):
        raise RuntimeError("refrigerator: canonical prop no longer aligns with appliance bay")
    world_front = prop["position"][2] - bounds[0][2]
    world_back = prop["position"][2] - bounds[1][2]
    if world_front < -26.30 or world_back < -27.05 or world_back > -26.95:
        raise RuntimeError(
            f"refrigerator: front/back no longer dress the shut portal "
            f"({world_front:.3f}, {world_back:.3f})")
    opening = layout_io.by_id(layout_io.rows(layout, "openings"), "opening")[
        "FRIDGE_L0_KITCHEN"]
    portal = layout_io.by_id(layout_io.rows(layout, "portals"), "portal")[
        "P_FRIDGE_INTERIOR"]
    if (opening["portal"] != "P_FRIDGE_INTERIOR" or
            opening["material"] != "MAT_DOOR_PAINTED" or
            portal["cellA"] != "L0_KITCHEN" or
            portal["cellB"] != "CELL_FRIDGE_INTERIOR" or
            portal["plane"] != {"axis": "z", "value": -26.45}):
        raise RuntimeError("refrigerator: canonical portal/front-material link changed")


def check() -> None:
    rows = {row["id"]: row for row in json.loads(MANIFEST.read_text())["assets"]}
    with tempfile.TemporaryDirectory(prefix="house01040-builtins-", dir="/tmp") as scratch:
        folder = Path(scratch)
        command = blender_env.build_command(SCRIPT, ["--out", str(folder)])
        if command is None:
            raise RuntimeError("Blender unavailable; cannot validate authored built-ins")
        checked_run(command, environment=blender_env.environment(),
                    sentinel="kitchen_builtins: EXIT 0")
        for name in NAMES:
            source = folder / f"{name}_raw.glb"
            generated = folder / f"{name}.glb"
            if not source.is_file():
                raise RuntimeError(f"{source}: Blender did not export this model")
            checked_run([sys.executable, str(COLLISION), str(source), str(generated),
                         "--mode", "box"])
            if not generated.is_file():
                raise RuntimeError(f"{generated}: collision exporter produced no GLB")
            expected = MODELS / generated.name
            if not expected.is_file():
                raise RuntimeError(f"{expected}: committed source model is missing")
            actual_hash = sha256(generated.read_bytes())
            committed_hash = sha256(expected.read_bytes())
            manifest_hash = rows[ASSET_IDS[name]]["sourceSha256"]
            if actual_hash != committed_hash or actual_hash != manifest_hash:
                raise RuntimeError(
                    f"{name}: regenerated={actual_hash}, committed={committed_hash}, "
                    f"manifest={manifest_hash}")
            check_manifest_geometry(expected, rows[ASSET_IDS[name]])
            if name == "range_wall":
                check_range_wall_geometry(expected, rows[ASSET_IDS[name]])
            print(f"kitchen_builtins_prepare: {name} {actual_hash[:16]} deterministic")
        fridge_raw = folder / "refrigerator_raw.glb"
        fridge_generated = folder / "refrigerator.glb"
        fridge_command = blender_env.build_command(
            FRIDGE_SCRIPT, ["--out", str(fridge_raw)])
        if fridge_command is None:
            raise RuntimeError("Blender unavailable; cannot validate authored refrigerator")
        checked_run(fridge_command, environment=blender_env.environment(),
                    sentinel="kitchen_refrigerator: EXIT 0")
        checked_run([sys.executable, str(COLLISION), str(fridge_raw),
                     str(fridge_generated), "--mode", "box"])
        expected = MODELS / fridge_generated.name
        if not expected.is_file():
            raise RuntimeError(f"{expected}: committed refrigerator model missing")
        actual_hash = sha256(fridge_generated.read_bytes())
        committed_hash = sha256(expected.read_bytes())
        manifest_hash = rows[FRIDGE_ID]["sourceSha256"]
        if actual_hash != committed_hash or actual_hash != manifest_hash:
            raise RuntimeError(
                f"refrigerator: regenerated={actual_hash}, committed={committed_hash}, "
                f"manifest={manifest_hash}")
        check_refrigerator_geometry(expected, rows[FRIDGE_ID])
        print(f"kitchen_builtins_prepare: refrigerator {actual_hash[:16]} deterministic")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", required=True)
    parser.parse_args()
    try:
        check()
    except (RuntimeError, KeyError, ValueError) as error:
        print(f"kitchen_builtins_prepare: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
