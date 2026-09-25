#!/usr/bin/env bash
# One test entry point for local and CI runs. A sporadic failure without its GoogleTest output is
# not actionable, so output-on-failure is unconditional rather than left to each caller to remember.
set -euo pipefail

exec ctest --output-on-failure "$@"
