#!/usr/bin/env bash
# Builds the generated content a fresh checkout lacks -- the deployed world, collision, nav, the
# .cnb banks -- the way a developer's tree has it, then re-configures and rebuilds the preset so the
# build tree gets its copy of content/world (the copy target exists only when content/world did at
# configure time). Unit and integration tests read that content; without it they fail where they
# should have run (CNA plans/plan_apple_m4.md AM4-258).
#
# The house shell comes first: build_content.py has no stage for it, and without build/shell the
# `chunks` and `shading` stages skip, leaving content/world/chunks.bin and shading.bin absent -- the
# files most unit and integration tests open. It needs Blender (with numpy for Blender's Python),
# as the Web job's content step already does (AM4-276).
#
# Then its lightmap UVs: since HOUSE-00923 `chunks` draws interior receivers (floors, walls,
# ceilings) with DualTextureEffect and needs their second UV channel from build/shell-lm, so a tree
# without it stops at B1_CELLAR's MAT_FLOOR_STONE. `shell_unwrap.py` is a Blender UV unwrap, not a
# Cycles bake -- 78 atlases in about 8 s (owner decision 2026-10-09, CNA plans/plan_apple_m4.md
# AM4-314).
#
# Usage: tools/ci/ci_content.sh <configure-preset> <its build directory>
set -euo pipefail
preset="${1:?usage: ci_content.sh <configure-preset> <build directory>}"
build_dir="${2:?usage: ci_content.sh <configure-preset> <build directory>}"

export CNA_CONTENT="${CNA_CONTENT:-${build_dir}/CNA_BUILD/cna-content}"
python3 tools/blender/house_shell_gen.py
python3 tools/blender/shell_unwrap.py
python3 tools/ci/build_content.py
cmake --preset "${preset}"
cmake --build --preset "${preset}" -j"$(nproc)"
