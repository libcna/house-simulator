#!/usr/bin/env python3
"""check_xna_only.py -- the XNA-only build gate for cna-house.

Implements the static gates of `cna-house.md` section 70.1 and the policy of
`docs/decisions/ADR-0001-xna-only.md`.

THERE IS NO ALLOWLIST.

This script consults no exception file, no per-symbol allowlist and no deviation register.
`docs/xna-deviations.md` is *not* read: it records the project-owned `cnahouse::` subsystems and
grants permission to call nothing. A violation therefore cannot be argued into the build -- it can
only be rewritten as `cnahouse::` code, which is what an XNA 4.0 developer would have had to do
anyway.

Seventeen rejected classes, each with a planted-violation fixture in --selftest:

    forbidden-include     a #include of a CNA/ or native-graphics header
    cna-namespace         any CNA:: reference
    ext-identifier        any identifier carrying an embedded uppercase EXT marker
    cnaext-convenience    a named CNAEXT convenience call on an otherwise-XNA type
    capability-query      SupportsCapability and friends
    forbidden-effect-api  ShaderEffect / PbrEffect / SkinnedPbrEffect / AvatarRenderer
    model-tag             a Model::Tag / getTagProperty read
    cna-animation-type    CNA's sample-derived SkinningData / AnimationClip / AnimationPlayer
    native-graphics       GL / GLES / EGL / Vulkan / WebGPU / D3D / Metal / SDL-rendering symbols
    cmake-cnaext          CNA_CNAEXT enabled in any CMake input or cache
    weather-boolean       an is(Raining|Snowing|Windy|Stormy) member
    std-filesystem        std::filesystem outside SaveStore
    posix-file            fopen / opendir / unlink and friends outside SaveStore
    std-thread            std::thread / std::async: v1 is single-threaded
    custom-main-loop      a browser/native loop that bypasses XNA Game::Run
    shader-source         GLSL / SPIR-V / WGSL / Metal shader source anywhere in the tree
    fx-placement          a .fx / .fxh outside assets-src/Effects/

Usage:
    check_xna_only.py [--root DIR] [--format text|json]
    check_xna_only.py --selftest

Exit status: 0 clean, 1 violations found, 2 usage error, 3 self-test failure.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

# --------------------------------------------------------------------------------------------
# What is scanned
# --------------------------------------------------------------------------------------------

# Game code: the XNA-only rule applies here in full.
RUNTIME_ROOTS = ("src", "include")

# Test code: not the shipped runtime, but it exercises the game through the same API and has no
# business touching CNA either. The std::filesystem restriction is a *game code* rule
# (`cna-house.md` section 8.3) and so does not extend here; everything else does.
TEST_ROOTS = ("tests",)

CXX_SUFFIXES = {".cpp", ".cc", ".cxx", ".hpp", ".h", ".hh", ".hxx", ".inl", ".ipp"}

CMAKE_NAMES = {"CMakeLists.txt", "CMakeCache.txt", "CMakePresets.json", "CMakeUserPresets.json"}
CMAKE_SUFFIXES = {".cmake"}

# Shader sources that are not XNA Effects. Custom shaders in this project are `.fx` compiled to
# Direct3D 9 Effect-Framework bytecode (ADR-0003); GLSL and SPIR-V would mean `ShaderEffect`,
# which is Tier C.
SHADER_SOURCE_SUFFIXES = {
    ".glsl", ".vert", ".frag", ".geom", ".tesc", ".tese", ".comp", ".vsh", ".fsh",
    ".spv", ".spvasm", ".wgsl", ".metal",
}
FX_SUFFIXES = {".fx", ".fxh"}
FX_ROOT = "assets-src/Effects"

# Never descend into these, wherever they appear.
SKIP_DIRS_ANYWHERE = {".git", "__pycache__", ".idea", ".vscode", ".vs", ".cache", "node_modules"}

# Skipped only at the repository root. `src/content/` is game code and must be scanned; the
# generated `content/` tree beside it must not, and neither must a build directory.
SKIP_DIRS_AT_ROOT = {
    "build", "build-asan", "build-ubsan", "build-tsan", "build-probe", "build-consumer",
    "content",
}

# The one file family permitted to use std::filesystem: `SaveStore`'s desktop implementation.
# This is the policy of `cna-house.md` section 8.3 -- saves reach the disk through
# StorageDevice/ISaveStore so that the Web port is a swap, not a rewrite -- and not an
# exception mechanism: it is keyed on the file name the policy names, and admits no symbol
# that any other rule rejects.
SAVESTORE_STEM_PREFIX = "SaveStore"

# --------------------------------------------------------------------------------------------
# Rules
# --------------------------------------------------------------------------------------------

RULE_HELP = {
    "forbidden-include": "Include only Microsoft/Xna/..., System/..., the C++ standard library "
                         "and cnahouse/... headers.",
    "cna-namespace": "CNA:: is Tier C (ADR-0001). Write the behaviour in cnahouse:: instead.",
    "ext-identifier": "CNAEXT-marked convenience API is Tier C. There is no XNA 4.0 equivalent "
                      "to reach for, so write it yourself.",
    "cnaext-convenience": "This is a CNA extension on an otherwise-XNA type, not XNA 4.0 API.",
    "capability-query": "XNA 4.0 has no capability query. Tier selection is a build fact plus a "
                        "guarded content load (ADR-0003).",
    "forbidden-effect-api": "Custom shaders are XNA Effects compiled from .fx (ADR-0003).",
    "model-tag": "The runtime never reads Model::Tag (BL-01). Animation data travels in a "
                 ".chanim sidecar (OWN-07).",
    "cna-animation-type": "These are CNA's copies of Skinned Model *Sample* classes, never XNA "
                          "Framework API. Use cnahouse::anim (OWN-01, OWN-02).",
    "native-graphics": "The runtime talks to the GPU only through XNA's GraphicsDevice.",
    "cmake-cnaext": "CNA_CNAEXT must stay OFF: with it off the engine layer is not in the binary.",
    "weather-boolean": "Weather quantities are continuous. Use the float, not a boolean "
                       "(cna-house.md section 36.1).",
    "std-filesystem": "Game code reaches the disk through StorageDevice / ISaveStore so the Web "
                      "port is a swap, not a rewrite (cna-house.md section 8.3).",
    "posix-file": "The same rule as std-filesystem, by the other door: no POSIX or C file call "
                  "outside SaveStore's desktop implementation (cna-house.md section 8.3).",
    "std-thread": "v1 is single-threaded; Emscripten threading changes the module ABI and needs "
                  "COOP/COEP headers, and HOUSE-02451 measures before anyone pays that "
                  "(cna-house.md section 8.4).",
    "custom-main-loop": "XNA Game::Run owns the loop on every platform; browser/native loop APIs "
                        "would bypass CNA's Asyncify integration (cna-house.md section 80.1).",
    "shader-source": "GLSL/SPIR-V source would mean CNA::Graphics::ShaderEffect, which is Tier C. "
                     "Author .fx under assets-src/Effects/ instead.",
    "fx-placement": "Effect sources live in assets-src/Effects/ so the content build and this "
                    "gate can both find them.",
}

# Identifier-level denylists. These are denylists, not allowlists: nothing here can grant access.
FORBIDDEN_IDENTIFIERS = {
    "cnaext-convenience": {
        "CNAEXT", "setOwnedResources", "getSkinsEXTProperty",
        "SetAssemblyTitleEXT", "GetAssemblyTitleEXT",
    },
    "capability-query": {"SupportsCapability", "GraphicsCapability"},
    "forbidden-effect-api": {"ShaderEffect", "PbrEffect", "SkinnedPbrEffect", "AvatarRenderer",
                             "SkinnedModelEXT"},
    "model-tag": {"getTagProperty", "setTagProperty", "TagProperty"},
    "cna-animation-type": {"SkinningData", "AnimationClip", "AnimationPlayer"},
}

# Regexes applied to comment- and literal-stripped C++ text.
CODE_PATTERNS = [
    ("cna-namespace", re.compile(r"\bCNA\s*::")),
    ("model-tag", re.compile(r"\bModel\s*::\s*Tag\b")),
    ("cna-animation-type", re.compile(r"\bGraphics\s*::\s*Keyframe\b")),
    # Native graphics: entry points are calls, constants are SCREAMING_CASE with a fixed prefix.
    ("native-graphics", re.compile(r"\bgl[A-Z]\w*\s*\(")),
    ("native-graphics", re.compile(r"\begl[A-Z]\w*\s*\(")),
    ("native-graphics", re.compile(r"\bvk[A-Z]\w*\s*\(")),
    ("native-graphics", re.compile(r"\b(?:GL|EGL|VK|WGPU|MTL)_[A-Z0-9_]+\b")),
    ("native-graphics", re.compile(r"\b(?:Vk|MTL|WGPU)[A-Z]\w*\b")),
    ("native-graphics", re.compile(r"\bwgpu[A-Z]\w*\b")),
    ("native-graphics", re.compile(r"\b(?:ID3D|IDirect3D|D3D)\w*\b")),
    ("native-graphics", re.compile(r"\bSDL_\w+\b")),
    ("native-graphics", re.compile(r"\b(?:glslang|SPIRV|spvc?)[A-Za-z_]\w*\b")),
    ("std-filesystem", re.compile(r"\bstd\s*::\s*filesystem\b")),
    # `HOUSE-02841`. §8.3: *"No POSIX filesystem use outside `SaveStore`'s desktop
    # implementation."* `std::filesystem` was already linted; the C and POSIX doors into the same
    # room were not, and `std::fopen` is the one a person reaches for without thinking.
    #
    # Only names that cannot be anything else. `remove` and `rename` are deliberately absent:
    # `std::remove` is `<algorithm>`'s and flagging it would train people to write exemptions.
    ("posix-file", re.compile(r"\b(?:fopen|freopen|fdopen|fileno|popen)\s*\(")),
    ("posix-file", re.compile(r"\b(?:mkdir|rmdir|unlink|opendir|readdir|closedir|getcwd|chdir|"
                              r"lstat|realpath|symlink)\s*\(")),
    # `HOUSE-02841`. §8.4: *"v1 is single-threaded. Everything runs on the game thread."* The
    # reason is in §8.4 and is not style: Emscripten threading changes the ABI of the whole module
    # and needs COOP/COEP headers, and `HOUSE-02451` has to MEASURE the load time before anyone
    # pays that. A thread that appears before that measurement is a Web port nobody can ship.
    ("std-thread", re.compile(r"\bstd\s*::\s*(?:thread|jthread|async)\b")),
    # `HOUSE-02843`. CNA's `Game::Run` owns the loop and integrates with requestAnimationFrame
    # through Asyncify. Calling either browser loop API directly creates a second owner and hangs
    # or recurses on Web. Ordinary finite `while`/`for` loops are not frame loops and deliberately
    # are not linted.
    ("custom-main-loop", re.compile(r"\b(?:emscripten_set_main_loop|"
                                    r"emscripten_request_animation_frame_loop|"
                                    r"requestAnimationFrame)\b")),
]

INCLUDE_RE = re.compile(r"^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]")

#: §8.4's single thread, as headers. `<mutex>` and `<atomic>` are NOT here: a single-threaded
#: program has no use for them either, but they also appear in a test's allocation counter and in
#: third-party headers, and a rule that fires on those is a rule people turn off.
THREAD_INCLUDE_NAMES = frozenset(("thread", "future", "latch", "barrier", "stop_token",
                                  "semaphore", "condition_variable"))

FORBIDDEN_INCLUDE_PREFIXES = ("CNA/",)
NATIVE_INCLUDE_PREFIXES = (
    "GL/", "GLES/", "GLES2/", "GLES3/", "GLES31/", "EGL/", "KHR/", "glad/", "GLFW/",
    "vulkan/", "webgpu/", "wgpu/", "SDL/", "SDL2/", "SDL3/", "Metal/", "QuartzCore/",
)
NATIVE_INCLUDE_NAMES = (
    "SDL.h", "SDL_render.h", "SDL3.h", "glad.h", "d3d9.h", "d3d11.h", "d3d12.h", "dxgi.h",
    "vulkan.h", "webgpu.h",
)

IDENTIFIER_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
EMBEDDED_EXT_RE = re.compile(r"[a-z0-9]EXT")
WEATHER_BOOL_RE = re.compile(r"[Ii]s(?:Raining|Snowing|Windy|Stormy)\b")

#: Kept alongside the declaration rule below, and not replaced by it: this one fires on the
#: IDENTIFIER wherever it appears, so `auto IsRaining() -> bool` and a call to one are caught even
#: though neither has `bool` in front of a name. Two cheap rules that overlap beat one that has to
#: be right about C++ syntax.
#:
#: `HOUSE-01700`. The rule above is the one `HOUSE-00021` wrote, and it catches four names.
#:
#: Measured 2026-09-11 against eighteen shapes a person would actually write: it caught
#: `isRaining`, `IsSnowing`, `isWindy` and `isStormy`, and **missed the other fourteen** --
#: `raining`, `snowing_`, `hasRain`, `rainActive`, `isHailing`, `isThundering`, `isFoggy`,
#: `isOvercast`, `wasRaining`, `stormActive`, `windGusting`, `snowOnGround`, `precipitating`, and
#: `isRainingNow`, which differs from a name it does catch by one word. §36.1 says *"there is no
#: `isRaining` boolean anywhere in the codebase"* and means the kind and not the spelling.
#:
#: So the rule below is about a **`bool` DECLARATION whose name names a weather quantity**, which
#: is what §36.1 forbids, rather than about four spellings of it. Only `bool` is examined: `float
#: snowDepth` and `float cloudCover` are §36.1's own field names and must never be flagged.
BOOL_DECL_RE = re.compile(r"\bbool\s+([A-Za-z_][A-Za-z0-9_]*)")

#: An identifier split into lower-case words: `isRainingNow` -> `is`, `raining`, `now`.
#:
#: **Tokenising is the whole reason this is not a substring search.** `terrain` contains `rain`
#: and `window` contains `wind`, and a substring rule flags `bool useTerrain`, `bool terrain` and
#: `bool windowActive` -- all three of which exist in this repository today and none of which has
#: anything to do with the weather.
IDENT_WORDS_RE = re.compile(r"[A-Z]+(?![a-z])|[A-Z][a-z0-9]*|[a-z0-9]+")


def _weather_words() -> frozenset:
    """Every token that makes a `bool` a weather state. Explicit, so it can be read and argued with.

    Two groups, and the split matters. A **state** word names a weather condition on its own, so a
    `bool` of it is the offence however it is spelt: `bool snowOnGround` is exactly the flag §36.1
    forbids. A **quantity** word names something §36.1 stores as a float -- a wind speed, a fog
    density, a cloud cover, a gust factor, a wetness -- and its bare noun is legitimate all over the
    codebase, so only the adjective or the participle is the offence: `bool windy` yes, `bool
    windowActive` no, and `bool fogEnabled` no, because §31.5's fog switch is a renderer state and
    genuinely is a boolean.
    """
    states = ("rain", "snow", "sleet", "hail", "storm", "thunder", "lightning", "drizzle",
              "blizzard", "overcast", "humid", "precip", "precipitation")
    words = set()
    for stem in states:
        for suffix in ("", "s", "y", "ing", "ed"):
            words.add(stem + suffix)
        # A stem ending in `e` drops it before a vowel: drizzle -> drizzling, drizzled.
        if stem.endswith("e"):
            words.update((stem[:-1] + "ing", stem + "d", stem[:-1] + "y"))
    # The irregulars, spelt out rather than produced by a doubling rule nobody would trust, and
    # the participles of the QUALIFIED nouns -- `bool windy` and `bool windGusting` are flags,
    # `float windSpeed` and `bool windowActive` are not.
    words.update(("foggy", "windy", "cloudy", "gusty", "gusting", "misty", "wet", "wetting",
                  "precipitating", "raining", "snowing", "hailing", "thundering", "stormy",
                  "drizzling", "sleeting", "blizzarding"))
    return frozenset(words)


WEATHER_WORDS = _weather_words()


def weather_boolean_offence(name: str) -> str | None:
    """The token that makes @p name a weather boolean, or `None`."""
    for token in IDENT_WORDS_RE.findall(name):
        lowered = token.lower()
        if lowered in WEATHER_WORDS:
            return lowered
    return None

CMAKE_CNAEXT_RE = re.compile(r"CNA_CNAEXT[^\r\n]*")
TRUTHY_RE = re.compile(r"\b(ON|TRUE|YES|Y|1)\b", re.IGNORECASE)
FALSY_RE = re.compile(r"\b(OFF|FALSE|NO|N|0)\b", re.IGNORECASE)


@dataclass(frozen=True)
class Violation:
    path: str
    line: int
    rule: str
    text: str

    def render(self) -> str:
        return (f"{self.path}:{self.line}: [{self.rule}] {self.text}\n"
                f"    -> {RULE_HELP[self.rule]}")


# --------------------------------------------------------------------------------------------
# C++ text preparation
# --------------------------------------------------------------------------------------------

def strip_cxx(text: str) -> str:
    """Blank out comments and string/char literals, preserving line and column positions.

    Symbol rules run on the result, so prose that *names* a forbidden symbol -- a comment
    explaining why it is forbidden, a log message, a test fixture string -- is not a violation.
    Calling one is.
    """
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        two = text[i:i + 2]
        if two == "//":
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
        elif two == "/*":
            out.append("  ")
            i += 2
            while i < n and text[i:i + 2] != "*/":
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            if i < n:
                out.append("  ")
                i += 2
        elif c == 'R' and text[i:i + 2] == 'R"':
            close = text.find("(", i + 2)
            if close == -1:
                out.append(c)
                i += 1
                continue
            delim = text[i + 2:close]
            end = text.find(')' + delim + '"', close)
            end = n if end == -1 else end + len(delim) + 2
            for j in range(i, end):
                out.append("\n" if text[j] == "\n" else " ")
            i = end
        elif c in "\"'":
            quote = c
            out.append(" ")
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\" and i + 1 < n:
                    out.append("  ")
                    i += 2
                    continue
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            if i < n:
                out.append(" ")
                i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


# --------------------------------------------------------------------------------------------
# Checks
# --------------------------------------------------------------------------------------------

def check_includes(rel: str, raw_lines: list[str], out: list[Violation]) -> None:
    for number, line in enumerate(raw_lines, start=1):
        match = INCLUDE_RE.match(line)
        if not match:
            continue
        header = match.group(1)
        if header.startswith(FORBIDDEN_INCLUDE_PREFIXES):
            out.append(Violation(rel, number, "forbidden-include",
                                 f'#include "{header}" -- CNA headers are Tier C'))
        elif header.startswith(NATIVE_INCLUDE_PREFIXES) or header in NATIVE_INCLUDE_NAMES:
            out.append(Violation(rel, number, "native-graphics",
                                 f'#include "{header}" -- native graphics header'))
        elif header == "filesystem":
            out.append(Violation(rel, number, "std-filesystem",
                                 "#include <filesystem>"))
        elif header in THREAD_INCLUDE_NAMES:
            out.append(Violation(rel, number, "std-thread", f"#include <{header}>"))


def check_code(rel: str, code: str, out: list[Violation], *,
               exempt: frozenset = frozenset()) -> None:
    lines = code.split("\n")
    for number, line in enumerate(lines, start=1):
        for declared in BOOL_DECL_RE.findall(line):
            offence = weather_boolean_offence(declared)
            if offence is not None:
                out.append(Violation(rel, number, "weather-boolean", f"bool {declared}"))
        for rule, pattern in CODE_PATTERNS:
            if rule in exempt:
                continue
            match = pattern.search(line)
            if match:
                out.append(Violation(rel, number, rule, match.group(0).strip()))
        for ident in IDENTIFIER_RE.findall(line):
            if EMBEDDED_EXT_RE.search(ident):
                out.append(Violation(rel, number, "ext-identifier", ident))
                continue
            if WEATHER_BOOL_RE.search(ident):
                out.append(Violation(rel, number, "weather-boolean", ident))
                continue
            for rule, names in FORBIDDEN_IDENTIFIERS.items():
                if ident in names:
                    out.append(Violation(rel, number, rule, ident))
                    break


def check_cmake(rel: str, raw_lines: list[str], out: list[Violation]) -> None:
    for number, line in enumerate(raw_lines, start=1):
        for hit in CMAKE_CNAEXT_RE.findall(line):
            tail = hit.split("CNA_CNAEXT", 1)[1]
            truthy = TRUTHY_RE.search(tail)
            falsy = FALSY_RE.search(tail)
            if truthy and (falsy is None or truthy.start() < falsy.start()):
                out.append(Violation(rel, number, "cmake-cnaext", hit.strip()))


def check_placement(rel: str, suffix: str, out: list[Violation]) -> None:
    if suffix in SHADER_SOURCE_SUFFIXES:
        out.append(Violation(rel, 1, "shader-source", f"{suffix} shader source in the tree"))
    elif suffix in FX_SUFFIXES and not rel.startswith(FX_ROOT + "/"):
        out.append(Violation(rel, 1, "fx-placement", f"{rel} is outside {FX_ROOT}/"))


# --------------------------------------------------------------------------------------------
# Walk
# --------------------------------------------------------------------------------------------

def iter_files(root: Path):
    for dirpath, dirnames, filenames in os.walk(root):
        at_root = Path(dirpath) == root
        dirnames[:] = sorted(
            d for d in dirnames
            if d not in SKIP_DIRS_ANYWHERE and not (at_root and d in SKIP_DIRS_AT_ROOT))
        for name in sorted(filenames):
            yield Path(dirpath) / name


def scan(root: Path) -> list[Violation]:
    root = root.resolve()
    violations: list[Violation] = []
    for path in iter_files(root):
        rel = path.relative_to(root).as_posix()
        top = rel.split("/", 1)[0]
        suffix = path.suffix

        check_placement(rel, suffix, violations)

        if path.name in CMAKE_NAMES or suffix in CMAKE_SUFFIXES:
            try:
                raw = path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            check_cmake(rel, raw.split("\n"), violations)

        if suffix not in CXX_SUFFIXES:
            continue
        if top not in RUNTIME_ROOTS and top not in TEST_ROOTS:
            continue
        try:
            raw = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue

        raw_lines = raw.split("\n")
        exempt = exempt_rules_for(rel, top, path.stem)
        check_includes_except(rel, raw_lines, violations, exempt)
        check_code(rel, strip_cxx(raw), violations, exempt=exempt)

    violations.sort(key=lambda v: (v.path, v.line, v.rule))
    return violations


REPO = Path(__file__).resolve().parents[2]

PATH_EXEMPTIONS: dict[str, tuple[frozenset, str]] = {}


def exempt_rules_for(rel: str, top: str, stem: str) -> frozenset:
    """Which rules @p rel may break.

    Three sources, and they are different kinds of thing. **Tests** may reach the disk and may
    start a thread: §8.3 and §8.4 are rules about the GAME, and a test that reads a committed PNG
    or checks a counter under contention is not the game. **`SaveStore`** is where §8.3 says the
    disk access belongs. **`PATH_EXEMPTIONS`** is one named file with a reason and a task id.
    """
    rules: set = set()
    if top in TEST_ROOTS:
        rules.update(("std-filesystem", "posix-file", "std-thread"))
    if stem.startswith(SAVESTORE_STEM_PREFIX):
        rules.update(("std-filesystem", "posix-file"))
    entry = PATH_EXEMPTIONS.get(rel)
    if entry is not None:
        rules.update(entry[0])
    return frozenset(rules)


def check_includes_except(rel: str, raw_lines: list[str], out: list[Violation],
                          exempt: frozenset) -> None:
    """`check_includes`, with the rules in @p exempt dropped.

    Filtering AFTER the fact rather than threading the exemption through `check_includes`: the
    include scanner answers one question per line and there is nothing to skip inside it.
    """
    collected: list[Violation] = []
    check_includes(rel, raw_lines, collected)
    out.extend(v for v in collected if v.rule not in exempt)


# --------------------------------------------------------------------------------------------
# Self-test: one planted violation per rejected class
# --------------------------------------------------------------------------------------------

CLEAN_SOURCE = """// SPDX-License-Identifier: MS-PL
#pragma once

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "cnahouse/util/Result.hpp"

#include <vector>

// This comment names CNA::Graphics::ShaderEffect, SupportsCapability, getSkinsEXTProperty and
// std::filesystem on purpose: prose about the rule is not a violation of it.
namespace cnahouse::rendering
{
    class RoomPass
    {
    public:
        void Draw(Microsoft::Xna::Framework::Graphics::BasicEffect& effect, float rainfall);

    private:
        std::vector<int> m_chunks;
        float m_rainfallRate = 0.0F;
    };
}
"""

FIXTURES: dict[str, tuple[str, str]] = {
    "forbidden-include": (
        "src/rendering/PlantedInclude.cpp",
        '#include "CNA/Graphics/SkyEngine.hpp"\nvoid f() {}\n'),
    "cna-namespace": (
        "src/rendering/PlantedNamespace.cpp",
        "void f()\n{\n    CNA::Graphics::HdrPipeline pipeline;\n}\n"),
    "ext-identifier": (
        "src/content/PlantedExt.cpp",
        "void f(Model& model)\n{\n    auto skins = model.getSkinsEXTProperty();\n}\n"),
    "cnaext-convenience": (
        "src/content/PlantedConvenience.cpp",
        "void f(Model& model)\n{\n    model.setOwnedResources(true);\n}\n"),
    "capability-query": (
        "src/rendering/PlantedCapability.cpp",
        "bool f(GraphicsDevice& device)\n{\n    return device.SupportsCapability(7);\n}\n"),
    "forbidden-effect-api": (
        "src/rendering/PlantedEffectApi.cpp",
        "void f()\n{\n    ShaderEffect effect;\n}\n"),
    "model-tag": (
        "src/animation/PlantedTag.cpp",
        "void f(Model& model)\n{\n    auto tag = Model::Tag;\n}\n"),
    "cna-animation-type": (
        "src/animation/PlantedAnimType.cpp",
        "void f()\n{\n    AnimationPlayer player;\n}\n"),
    "native-graphics": (
        "src/rendering/PlantedNative.cpp",
        "void f()\n{\n    glBindTexture(0, 0);\n}\n"),
    "cmake-cnaext": (
        "CMakeLists.txt",
        'set(CNA_CNAEXT ON CACHE BOOL "" FORCE)\n'),
    # `HOUSE-01700`: deliberately NOT `bool isRaining`, which the rule `HOUSE-00021` wrote already
    # caught. `bool snowOnGround` is one of the fourteen realistic shapes that rule missed, so this
    # fixture fails against the old rule and passes against the new one -- which is what makes it
    # evidence rather than decoration. The four original spellings are covered by `must_reject`.
    "weather-boolean": (
        "src/weather/PlantedBoolean.hpp",
        "struct WeatherState\n{\n    bool snowOnGround = false;\n};\n"),
    "std-filesystem": (
        "src/persistence/PlantedFilesystem.cpp",
        "#include <filesystem>\nvoid f()\n{\n    std::filesystem::path p;\n}\n"),
    "posix-file": (
        "src/persistence/PlantedPosix.cpp",
        "void f()\n{\n    FILE* handle = std::fopen(\"save.bin\", \"w\");\n}\n"),
    "std-thread": (
        "src/app/PlantedThread.cpp",
        "#include <thread>\nvoid f()\n{\n    std::thread worker;\n}\n"),
    "custom-main-loop": (
        "src/app/PlantedMainLoop.cpp",
        "void f()\n{\n    emscripten_set_main_loop(nullptr, 0, true);\n}\n"),
    "shader-source": (
        "assets-src/Effects/planted.frag",
        "void main() {}\n"),
    "fx-placement": (
        "src/rendering/planted.fx",
        "technique T { pass P { } }\n"),
}


def _write(root: Path, rel: str, text: str) -> None:
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def _clean_tree(root: Path) -> None:
    _write(root, "src/rendering/RoomPass.hpp", CLEAN_SOURCE)
    _write(root, "include/cnahouse/rendering/RoomPass.hpp", CLEAN_SOURCE)
    _write(root, "tests/unit/rendering/RoomPassTests.cpp",
           '#include <filesystem>\n#include "cnahouse/rendering/RoomPass.hpp"\n'
           "void t()\n{\n    std::filesystem::path fixture;\n}\n")
    _write(root, "src/persistence/SaveStore.cpp",
           '#include <filesystem>\n#include "cnahouse/persistence/SaveStore.hpp"\n'
           "namespace cnahouse::persistence\n{\n"
           "    void SaveStore::Flush()\n    {\n"
           "        std::filesystem::path path;\n    }\n}\n")
    _write(root, "assets-src/Effects/RoomLit.fx", "technique Lit { pass P { } }\n")
    _write(root, "CMakeLists.txt",
           'set(CNA_CNAEXT OFF CACHE BOOL "" FORCE)\n'
           'set(CNA_GRAPHICS_RENDERER "OPENGLES3" CACHE STRING "" FORCE)\n')
    # A tree that *looks* like it offers an exception mechanism. The scanner must ignore it
    # completely: there is no allowlist, and adding one changes nothing.
    _write(root, "tools/ci/xna-allowlist.txt", "CNA::Graphics::HdrPipeline\nShaderEffect\n")
    _write(root, ".xna-allow", "*\n")


def selftest() -> int:
    failures: list[str] = []

    with tempfile.TemporaryDirectory(prefix="cnahouse-xnaonly-") as tmp:
        root = Path(tmp)
        _clean_tree(root)
        clean = scan(root)
        if clean:
            failures.append("the clean tree was rejected:\n  " +
                            "\n  ".join(v.render() for v in clean))

    for rule, (rel, body) in FIXTURES.items():
        with tempfile.TemporaryDirectory(prefix="cnahouse-xnaonly-") as tmp:
            root = Path(tmp)
            _clean_tree(root)
            _write(root, rel, body)
            found = scan(root)
            hit = [v for v in found if v.rule == rule and v.path == rel]
            if not hit:
                failures.append(f"planted [{rule}] in {rel} was NOT detected; "
                                f"scanner reported: {[ (v.rule, v.path) for v in found ] or 'nothing'}")

    # `HOUSE-01700`. §36.1: *"there is no `isRaining` boolean anywhere in the codebase; a lint
    # check in CI rejects one."* One planted fixture proves the rule fires; it does not prove the
    # rule is the RIGHT SHAPE, and the shape is the whole of this claim. These are the names a
    # person would actually write, with the ones the rule must NOT fire on beside them -- all
    # three of which exist in this repository today.
    must_reject = (
        "isRaining", "IsSnowing", "isWindy", "isStormy",   # `HOUSE-00021`'s four
        "isRainingNow",                                     # ...and one word past them
        "raining", "snowing_", "hasRain", "rainActive", "wasRaining", "snowOnGround",
        "isHailing", "isThundering", "isFoggy", "isOvercast", "stormActive", "windGusting",
        "precipitating", "isSleeting", "blizzardActive", "drizzling", "lightningNow",
    )
    must_accept = (
        "useTerrain", "terrain", "terrainTile",   # `rain` inside `terrain`
        "windowActive", "windowSill",             # `wind` inside `window`
        "fogEnabled",                             # §31.5's fog switch really is a boolean
        "enabled", "visible", "openFraction", "downspout", "reading", "training", "brainstem",
    )
    for name in must_reject:
        if weather_boolean_offence(name) is None:
            failures.append(f"`bool {name}` is a weather boolean §36.1 forbids and the lint "
                            f"does not reject it")
    for name in must_accept:
        offence = weather_boolean_offence(name)
        if offence is not None:
            failures.append(f"`bool {name}` is not a weather boolean and the lint rejects it "
                            f"on the token {offence!r} -- a false positive teaches people to "
                            f"add exemptions")
    # ...and the rule reads a `bool` DECLARATION, not any mention: §36.1's own field names are
    # floats and must survive.
    for line in ("float snowDepth = 0.0F;", "float cloudCover = 0.0F;",
                 "float windSpeed = 0.0F;", "float precipIntensity = 0.0F;",
                 "float thunderIntensity = 0.0F;", "float surfaceWetness = 0.0F;"):
        planted_out: list[Violation] = []
        check_code("src/weather/WeatherState.hpp", line, planted_out)
        if any(v.rule == "weather-boolean" for v in planted_out):
            failures.append(f"§36.1's own continuous field was rejected: {line!r}")
    # ...and a `bool` one of them is exactly what it is for.
    planted_out = []
    check_code("src/weather/WeatherState.hpp", "    bool snowDepth = false;", planted_out)
    if not any(v.rule == "weather-boolean" for v in planted_out):
        failures.append("`bool snowDepth` -- the float turned into a flag -- was not rejected")

    expected = set(RULE_HELP)
    planted = set(FIXTURES)
    if planted != expected:
        failures.append(f"fixture set does not cover every rejected class: "
                        f"missing {sorted(expected - planted)}, extra {sorted(planted - expected)}")
    if len(FIXTURES) != 17:
        failures.append(f"expected 17 planted-violation fixtures, found {len(FIXTURES)}")

    # `HOUSE-02841`. The exemptions, both directions.
    #
    # A test may reach the disk and may start a thread; the GAME may not. An exemption that has
    # stopped being needed has to FAIL, or the list silently becomes a list of things nobody
    # checks any more -- which is how an allowlist turns into a permission.
    for rel, (rules, reason) in PATH_EXEMPTIONS.items():
        if not (REPO / rel).exists():
            failures.append(f"exemption for {rel} names a file that no longer exists")
            continue
        if not reason.strip():
            failures.append(f"exemption for {rel} has no reason")
        stripped = strip_cxx((REPO / rel).read_text(encoding="utf-8", errors="replace"))
        needed: list[Violation] = []
        check_code(rel, stripped, needed)
        still = {v.rule for v in needed} & rules
        if still != set(rules):
            failures.append(f"exemption for {rel} covers {sorted(rules)} but the file only "
                            f"breaks {sorted(still) or 'nothing'} now -- delete the stale entry")
    # ...and a test really is exempt, while the same code in src/ really is not.
    for root, rel, expect in (("tests", "tests/unit/PlantedIo.cpp", False),
                              ("src", "src/util/PlantedIo.cpp", True)):
        probe: list[Violation] = []
        check_code(rel, "void f() { std::fopen(\"x\", \"r\"); }", probe,
                   exempt=exempt_rules_for(rel, root, "PlantedIo"))
        got = any(v.rule == "posix-file" for v in probe)
        if got != expect:
            failures.append(f"{rel}: posix-file {'was not' if expect else 'was'} reported "
                            f"and should {'have been' if expect else 'not have been'}")
    # ...and `SaveStore`'s desktop implementation is where §8.3 puts the disk access.
    probe = []
    check_code("src/persistence/SaveStoreDesktop.cpp", "void f() { std::fopen(\"s\", \"w\"); }",
               probe, exempt=exempt_rules_for("src/persistence/SaveStoreDesktop.cpp", "src",
                                              "SaveStoreDesktop"))
    if any(v.rule == "posix-file" for v in probe):
        failures.append("SaveStore's own implementation was rejected for reaching the disk")

    if failures:
        print("check_xna_only self-test FAILED", file=sys.stderr)
        for failure in failures:
            print("  - " + failure, file=sys.stderr)
        return 3

    print(f"check_xna_only self-test passed: {len(FIXTURES)} planted-violation fixtures "
          f"detected, clean tree accepted, no allowlist consulted.")
    print(f"  weather-boolean: {len(must_reject)} name(s) rejected, {len(must_accept)} accepted, "
          f"over {len(WEATHER_WORDS)} weather word(s) (`HOUSE-01700`).")
    print(f"  exemptions: {len(PATH_EXEMPTIONS)} path(s), each still needed (`HOUSE-02841`).")
    return 0


# --------------------------------------------------------------------------------------------

def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=None,
                        help="repository root to scan (default: the repository this script is in)")
    parser.add_argument("--format", choices=("text", "json"), default="text")
    parser.add_argument("--selftest", action="store_true",
                        help="run the planted-violation fixtures and exit")
    args = parser.parse_args(argv)

    if args.selftest:
        return selftest()

    root = Path(args.root) if args.root else Path(__file__).resolve().parents[2]
    if not root.is_dir():
        print(f"check_xna_only: not a directory: {root}", file=sys.stderr)
        return 2

    violations = scan(root)

    if args.format == "json":
        print(json.dumps([v.__dict__ for v in violations], indent=2))
    else:
        for violation in violations:
            print(violation.render())
        if violations:
            print(f"\ncheck_xna_only: {len(violations)} violation(s) in {root}", file=sys.stderr)
            print("There is no allowlist. Rewrite the code in cnahouse:: -- see "
                  "docs/decisions/ADR-0001-xna-only.md.", file=sys.stderr)
        else:
            print(f"check_xna_only: clean ({root})")
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
