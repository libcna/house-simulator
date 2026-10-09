#!/usr/bin/env bash
# Builds the generated content a fresh checkout lacks -- the deployed world, collision, nav, the
# .cnb banks -- the way a developer's tree has it, then re-configures and rebuilds the preset so the
# build tree gets its copy of content/world (the copy target exists only when content/world did at
# configure time). Unit and integration tests read that content; without it they fail where they
# should have run (CNA plans/plan_apple_m4.md AM4-258).
#
# Usage: tools/ci/ci_content.sh <configure-preset> <its build directory>
set -euo pipefail
preset="${1:?usage: ci_content.sh <configure-preset> <build directory>}"
build_dir="${2:?usage: ci_content.sh <configure-preset> <build directory>}"

export CNA_CONTENT="${CNA_CONTENT:-${build_dir}/CNA_BUILD/cna-content}"
python3 tools/ci/build_content.py
cmake --preset "${preset}"
cmake --build --preset "${preset}" -j"$(nproc)"
