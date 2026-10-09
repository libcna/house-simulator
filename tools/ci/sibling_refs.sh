#!/usr/bin/env bash
# Chooses the branch of each libcna sibling (cna, sharp-runtime, easy-gl, meta-gl) a CI build checks
# out, and prints it as "<name>=<branch>" lines for $GITHUB_OUTPUT (`sharp-runtime` becomes
# `sharp_runtime`).
#
# A sibling is built on the first candidate branch it actually has -- a topic or campaign branch
# pushed to all the repositories at once builds against itself -- otherwise on `next`, CNA's
# integration line, and on `develop` for a sibling with no `next` (easy-gl, meta-gl). This
# repository's `develop` was written against CNA's `next`: the sibling used to be checked out from
# `openeggbert/cnanext`, which no longer exists (CNA plans/plan_apple_m4.md AM4-232). `develop`,
# `main` and `master` are never matched by name, because the siblings' branches of those names are
# not the line this repository builds against (CNA's `develop` trails `next` by thousands of
# commits).
#
# Usage: tools/ci/sibling_refs.sh "<candidate branches, space separated>" <repo> [<repo> ...]
set -euo pipefail

candidates="${1:-}"
shift

has_branch() {
    git ls-remote --exit-code --heads "https://github.com/libcna/$1.git" "$2" >/dev/null 2>&1
}

for repo in "$@"; do
    chosen=""
    for branch in ${candidates}; do
        case "${branch}" in
            develop|main|master) continue ;;
        esac
        if has_branch "${repo}" "${branch}"; then
            chosen="${branch}"
            break
        fi
    done
    if [[ -z "${chosen}" ]]; then
        if has_branch "${repo}" next; then chosen=next; else chosen=develop; fi
    fi
    echo "${repo//-/_}=${chosen}"
done
