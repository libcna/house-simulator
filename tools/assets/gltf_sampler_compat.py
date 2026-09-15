#!/usr/bin/env python3
"""Normalize imported furniture GLBs for CNA's single-level, XNA-shaped model path.

Some source GLBs request mipmapped sampling even though the importer creates a
single-level texture from their embedded images (GLTF-206). Preserve the
nearest/linear choice while removing only the unsupported mipmap requirement.
The source also advertises transmission/volume in some materials. The static
house path cannot render physical transmission; those decorative surfaces use
their original base-color texture as an opaque fallback instead. Stale
extension declarations are removed. Source bytes change, so rehash the asset
manifest afterward.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from gltf_io import read_model, write_glb

NON_MIP_FILTER = {9984: 9728, 9985: 9729, 9986: 9728, 9987: 9729}
UNSUPPORTED = {"KHR_materials_transmission", "KHR_materials_volume"}


def normalize_document(document: dict) -> tuple[int, int]:
    changed = 0
    for sampler in document.get("samplers", []):
        old = sampler.get("minFilter")
        if old in NON_MIP_FILTER:
            sampler["minFilter"] = NON_MIP_FILTER[old]
            changed += 1
    removed = 0
    for material in document.get("materials", []):
        extensions = material.get("extensions", {})
        for name in UNSUPPORTED & extensions.keys():
            del extensions[name]
            removed += 1
        if not extensions:
            material.pop("extensions", None)
    for key in ("extensionsUsed", "extensionsRequired"):
        declarations = document.get(key, [])
        document[key] = [name for name in declarations if name not in UNSUPPORTED]
        if not document[key]:
            document.pop(key, None)
    return changed, removed


def normalize(path: Path, check: bool) -> int:
    document, blob = read_model(path)
    before = (document.get("extensionsUsed"), document.get("extensionsRequired"))
    changed, removed = normalize_document(document)
    stale = before != (document.get("extensionsUsed"), document.get("extensionsRequired"))
    if (changed or removed or stale) and not check:
        write_glb(path, document, blob)
    print(f"{path}: {changed} mip sampler(s), {removed} transmission/volume extension(s), "
          f"stale declaration={stale}")
    return changed + removed + int(stale)


def selftest() -> None:
    document = {"samplers": [{"minFilter": 9984}, {"minFilter": 9987}],
                "materials": [{"extensions": {"KHR_materials_transmission":
                                              {"transmissionFactor": 1},
                                              "KHR_materials_volume": {}}}],
                "extensionsUsed": ["KHR_materials_transmission", "KHR_materials_volume"]}
    assert normalize_document(document) == (2, 2)
    assert [sampler["minFilter"] for sampler in document["samplers"]] == [9728, 9729]
    assert "extensions" not in document["materials"][0]
    assert "extensionsUsed" not in document
    print("gltf_sampler_compat: selftest passed")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="*", type=Path)
    parser.add_argument("--check", action="store_true", help="refuse source GLBs with mip filters")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        selftest()
        return 0
    if not args.paths:
        parser.error("supply one or more .glb paths")
    changed = sum(normalize(path, args.check) for path in args.paths)
    return 1 if args.check and changed else 0


if __name__ == "__main__":
    sys.exit(main())
