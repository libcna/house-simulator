#!/usr/bin/env python3
"""HOUSE-02848: lint the portable texture and render-pass contract for Web.

The manifest defines which source PNGs ship. CNA's TextureProcessor defaults to one mip and
NoChange (RGBA8 for these PNGs), so every packaged texture must explicitly request its mip chain;
a non-Color override is forbidden. The world material table must name only those packaged
textures. Runtime source must not introduce an MRT, stencil, geometry or tessellation path.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "world"))
import layout_io  # noqa: E402


ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "assets-src"
CONFIG = ASSETS / "Textures" / ".cna-content.json"
MANIFEST = ASSETS / "assets.manifest.json"
MATERIALS = ASSETS / "world" / "layout.materials.json"
FORBIDDEN_RENDER_API = re.compile(
    r"\b(?:SetRenderTargets|RenderTargetBinding|ReferenceStencil|StencilEnable|"
    r"Depth24Stencil8|GeometryShader|HullShader|DomainShader|Tessellation)\b"
)


def check_texture_data(manifest: dict, config: dict, materials: dict) -> tuple[list[str], int]:
    problems: list[str] = []
    runtime: dict[str, str] = {}
    for row in manifest.get("assets", []):
        if row.get("kind") != "texture" or row.get("notPackaged"):
            continue
        source = row.get("sourceFile", "")
        content = row.get("contentName", "")
        if not source.startswith("assets-src/Textures/") or not content.startswith("Textures/"):
            problems.append(f"{row.get('id')}: packaged texture has no matching source/content path")
            continue
        name = source.removeprefix("assets-src/Textures/")
        if content != "Textures/" + name.removesuffix(".png") or not name.endswith(".png"):
            problems.append(f"{row.get('id')}: source and content names disagree")
            continue
        runtime[name] = content
        if not (ROOT / source).is_file():
            problems.append(f"{source}: packaged texture source is missing")

    configured = config.get("assets", {})
    for name in sorted(runtime):
        params = configured.get(name, {}).get("parameters", {})
        if params.get("generateMipmaps") != {"type": "bool", "value": True}:
            problems.append(f"{name}: generateMipmaps must be true (CNA otherwise writes one level)")
        # CNA's NoChange default preserves these PNGs as RGBA8/SurfaceFormat::Color. Keep the
        # existing generator-owned registrations intact; reject any explicit non-Color override.
        if params.get("textureFormat", {"type": "string", "value": "Color"}) != \
                {"type": "string", "value": "Color"}:
            problems.append(f"{name}: textureFormat must be Color")
    for name in sorted(set(configured) - set(runtime)):
        problems.append(f"{name}: content configuration names a non-packaged texture")

    content_names = set(runtime.values())
    references = set()
    for material in materials.get("materials", []):
        for field in ("albedo", "normal"):
            value = material.get(field)
            if value is None:
                continue
            references.add(value)
            if value not in content_names:
                problems.append(f"{material.get('id')}/{field}: {value} is not a packaged Color texture")
    if not runtime or not references:
        problems.append("no packaged textures or material references were found")
    return problems, len(runtime)


def check_render_source(root: Path) -> list[str]:
    problems = []
    for folder in ("src", "include"):
        for path in sorted((root / folder).rglob("*")):
            if path.suffix not in {".cpp", ".hpp", ".h"}:
                continue
            for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                code = line.split("//", 1)[0]
                match = FORBIDDEN_RENDER_API.search(code)
                if match:
                    problems.append(f"{path.relative_to(root)}:{number}: forbidden Web render path {match.group()}")
    return problems


def selftest() -> None:
    manifest = {"assets": [{"id": "T", "kind": "texture", "sourceFile":
                            "assets-src/Textures/Fallback/grey.png", "contentName":
                            "Textures/Fallback/grey"}]}
    config = {"assets": {"Fallback/grey.png": {"parameters": {
        "generateMipmaps": {"type": "bool", "value": True}}}}}
    materials = {"materials": [{"id": "M", "albedo": "Textures/Fallback/grey"}]}
    assert not check_texture_data(manifest, config, materials)[0]
    config["assets"]["Fallback/grey.png"]["parameters"]["generateMipmaps"]["value"] = False
    assert any("generateMipmaps" in p for p in check_texture_data(manifest, config, materials)[0])
    config["assets"]["Fallback/grey.png"]["parameters"]["generateMipmaps"]["value"] = True
    config["assets"]["Fallback/grey.png"]["parameters"]["textureFormat"] = {
        "type": "string", "value": "DxtCompressed"}
    assert any("textureFormat" in p for p in check_texture_data(manifest, config, materials)[0])
    materials["materials"][0]["normal"] = "Textures/Missing"
    assert any("not a packaged" in p for p in check_texture_data(manifest, config, materials)[0])
    assert FORBIDDEN_RENDER_API.search("device.SetRenderTargets(targets)")
    assert FORBIDDEN_RENDER_API.search("DepthFormat::Depth24Stencil8")
    print("check_web_materials: selftest passed")


def main() -> int:
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        selftest()
        return 0
    if len(sys.argv) != 1:
        print("usage: check_web_materials.py [--selftest]", file=sys.stderr)
        return 2
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    config = json.loads(CONFIG.read_text(encoding="utf-8"))
    materials = json.loads(layout_io.strip_jsonc(MATERIALS.read_text(encoding="utf-8")))
    problems, count = check_texture_data(manifest, config, materials)
    problems += check_render_source(ROOT)
    for problem in problems:
        print(f"check_web_materials: {problem}", file=sys.stderr)
    if problems:
        return 1
    print(f"check_web_materials: {count} packaged Color textures have pre-generated mips; "
          "material references and Web render path clean")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
