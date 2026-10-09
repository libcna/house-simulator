#!/usr/bin/env bash
# Chooses the branch of each libcna sibling (cna, sharp-runtime) a CI build checks out, and prints
# it as "<name>=<branch>" lines for $GITHUB_OUTPUT (`sharp-runtime` becomes `sharp_runtime`).
#
# A sibling is built on the first candidate branch it actually has -- a topic or campaign branch
# pushed to all three repositories at once builds against itself -- and otherwise on `next`, CNA's
# integration line. This repository's `develop` was written against that line: the sibling used to
# be checked out from `openeggbert/cnanext`, which no longer exists (CNA plans/plan_apple_m4.md
# AM4-232). `develop`, `main` and `master` are never matched by name, because the siblings' branches
# of those names are not the line this repository builds against (CNA's `develop` trails `next` by
# thousands of commits).
#
# Usage: tools/ci/sibling_refs.sh "<candidate branches, space separated>" <repo> [<repo> ...]
set -euo pipefail

candidates="${1:-}"
shift

for repo in "$@"; do
    chosen=next
    for branch in ${candidates}; do
        case "${branch}" in
            develop|main|master) continue ;;
        esac
        if git ls-remote --exit-code --heads "https://github.com/libcna/${repo}.git" "${branch}" \
            >/dev/null 2>&1; then
            chosen="${branch}"
            break
        fi
    done
    echo "${repo//-/_}=${chosen}"
done
