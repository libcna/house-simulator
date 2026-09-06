#!/usr/bin/env bash
# build_effects.sh -- compile `assets-src/Effects/*.fx` to committed `.xnb`, reproducibly.
#
# `HOUSE-00184`. This is the script that PRODUCES the baseline `.xnb` files committed beside their
# `.fx` sources (`HOUSE-00185`, BL-04), so that a contributor with no Wine and no DirectX SDK can
# still build and run Tier E. CMake does not call it: CMake compiles into the build tree when it
# can, and falls back to these committed files when it cannot. This script is run by hand, by
# whoever changes an effect, and its output is committed with that change.
#
# **Why a script rather than "run cna-content".** Three things have to be recorded together or the
# committed bytes are unreproducible: which compiler produced them, which `cna-content` produced
# them, and which source they came from. `assets-src/Effects/COMPILER.txt` is that record, and it is
# committed alongside the `.xnb`. Without it, "the effects are stale" is a question nobody can
# answer.
#
# This is OFFLINE TOOLING. Not runtime code, not subject to the XNA-only rule.
#
# Usage:
#     tools/effects/build_effects.sh                       # uses $CNAHOUSE_FXC or CNA_FXC
#     tools/effects/build_effects.sh --fxc /path/fxc.exe
#     tools/effects/build_effects.sh --check              # verify, change nothing
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
effects_src="${repo_root}/assets-src/Effects"
record="${effects_src}/COMPILER.txt"
manifest="${effects_src}/.cna-content-manifest.json"
launcher="${repo_root}/tools/effects/fxc-wine.sh"

fxc="${CNAHOUSE_FXC:-${CNA_FXC:-}}"
content=""
check_only=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --fxc)          fxc="$2"; shift 2 ;;
        --cna-content)  content="$2"; shift 2 ;;
        --check)        check_only=1; shift ;;
        -h|--help)      sed -n '2,25p' "$0"; exit 0 ;;
        *)              echo "build_effects.sh: unknown option '$1'" >&2; exit 2 ;;
    esac
done

die() { echo "build_effects.sh: $*" >&2; exit 1; }

# `cna-content` is REUSED, never rebuilt: it is a host tool that already exists in the CNA checkout,
# and openeggbert build rule 2 is explicit that an existing build directory is used rather than a
# new one created. Search the places it actually lands, in order of specificity.
if [[ -z "$content" ]]; then
    for candidate in \
        "${repo_root}/build/CNA_BUILD/cna-content" \
        "${repo_root}/build-consumer/CNA_BUILD/cna-content" \
        "${repo_root}/../cnanext/build/cna-content"
    do
        if [[ -x "$candidate" ]]; then content="$candidate"; break; fi
    done
fi
[[ -n "$content" ]] || die "no cna-content found; pass --cna-content <path>"
[[ -x "$content" ]] || die "cna-content at '$content' is not executable"

shopt -s nullglob
sources=("${effects_src}"/*.fx)
shopt -u nullglob
if [[ ${#sources[@]} -eq 0 ]]; then
    echo "build_effects.sh: no .fx sources under ${effects_src}; nothing to do."
    exit 0
fi

# --- the identity of everything that decides the output bytes ------------------------------------
#
# `cna-content` already puts the compiler's own reported identity into the build fingerprint, so a
# different compiler rebuilds rather than reusing artifacts. What it CANNOT do is tell a reader of
# this repository which compiler produced the committed file six months ago. That is this record.
identity_lines() {
    printf 'generated-by: tools/effects/build_effects.sh\n'
    # MEASURED: `cna-content` has no `--version` -- it answers
    # "error: the first argument must be 'build' or 'clean'." -- so the binary's own hash IS its
    # identity here. A line that always read "unknown" would be worse than no line.
    printf 'cna-content-sha256: %s\n' "$(sha256sum "$content" | cut -d' ' -f1)"
    if [[ -n "$fxc" && -f "$fxc" ]]; then
        printf 'fxc-path-basename: %s\n' "$(basename "$fxc")"
        printf 'fxc-sha256: %s\n' "$(sha256sum "$fxc" | cut -d' ' -f1)"
        printf 'fxc-size-bytes: %s\n' "$(stat -c%s "$fxc")"
    fi
    if command -v wine >/dev/null 2>&1; then
        printf 'wine: %s\n' "$(wine --version 2>/dev/null || echo unknown)"
    fi
    # The pipeline's own manifest records the compiler's REPORTED version inside the processor
    # identity -- `CNA.EffectSourceProcessor/1+fxc-9.29.952.3111-fx_2_0` -- which is a far better
    # provenance line than a file hash, because it is the number Microsoft put in the binary rather
    # than a property of one copy of it. Extracted here rather than parsed by anyone later.
    if [[ -f "$manifest" ]]; then
        local reported
        reported="$(grep -o 'fxc-[0-9.]*-fx_[0-9_]*' "$manifest" | head -1 || true)"
        [[ -n "$reported" ]] && printf 'fxc-reported-version: %s\n' "$reported"
    fi
    for source in "${sources[@]}"; do
        printf 'source %s: %s\n' "$(basename "$source")" "$(sha256sum "$source" | cut -d' ' -f1)"
    done
    for source in "${sources[@]}"; do
        local out="${source%.fx}.xnb"
        if [[ -f "$out" ]]; then
            printf 'output %s: %s\n' "$(basename "$out")" "$(sha256sum "$out" | cut -d' ' -f1)"
        fi
    done
}
# NOTE: no timestamp and no hostname. A record that changed on every run would produce a diff on
# every run, and a file that always has a diff is a file nobody reads.

if [[ $check_only -eq 1 ]]; then
    # `--check` answers one question: do the committed `.xnb` files match the committed `.fx`
    # sources, according to the record? It needs no compiler, which is the point -- it is the check
    # a contributor without Wine, and CI without Wine, can both run.
    [[ -f "$record" ]] || die "no ${record}; run this script without --check first"
    status=0
    for source in "${sources[@]}"; do
        name="$(basename "$source")"
        want="$(sha256sum "$source" | cut -d' ' -f1)"
        have="$(grep -F "source ${name}: " "$record" | tail -1 | awk '{print $3}')" || true
        if [[ "$want" != "$have" ]]; then
            echo "STALE: ${name} has changed since the committed .xnb was built" >&2
            echo "  source now  ${want}" >&2
            echo "  recorded    ${have:-<absent>}" >&2
            status=1
        fi
        out="${source%.fx}.xnb"
        if [[ ! -f "$out" ]]; then
            echo "MISSING: ${out} is not committed" >&2
            status=1
            continue
        fi
        want_out="$(sha256sum "$out" | cut -d' ' -f1)"
        have_out="$(grep -F "output $(basename "$out"): " "$record" | tail -1 | awk '{print $3}')" || true
        if [[ "$want_out" != "$have_out" ]]; then
            echo "STALE: $(basename "$out") does not match the recorded hash" >&2
            status=1
        fi
    done
    if [[ $status -eq 0 ]]; then
        echo "build_effects.sh: the committed effects match their sources (${#sources[@]} effect(s))."
    fi
    exit $status
fi

[[ -n "$fxc" ]] || die "no fxc; set CNAHOUSE_FXC or pass --fxc <path to fxc.exe>"
[[ -f "$fxc" ]] || die "fxc at '$fxc' does not exist"
[[ -x "$launcher" ]] || die "the launcher ${launcher} is missing or not executable"

for source in "${sources[@]}"; do
    out="${source%.fx}.xnb"
    echo "build_effects.sh: ${source} -> ${out}"
    # Straight into `assets-src/`, beside the source, because that is where the committed baseline
    # lives (BL-04, §18.4). The build tree's copy is CMake's business and is produced separately.
    "$content" build "$source" -o "$out" \
        --format xnb \
        --fx-compiler "$fxc" \
        --fx-compiler-launcher "$launcher"
done

identity_lines > "$record"
echo "build_effects.sh: recorded the compiler and source identities in ${record}"

# The pipeline's bookkeeping does NOT stay in the source tree. Removing the manifest also means the
# next baseline build is cold, which is what is wanted before committing bytes: an incremental
# "skip" would let a stale `.xnb` survive a change nobody noticed.
rm -f "$manifest" "${effects_src}/.cna-content.lock"
