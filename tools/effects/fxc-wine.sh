#!/usr/bin/env bash
# fxc-wine.sh -- run a Windows `fxc.exe` under Wine with Unix paths translated.
#
# Why this exists (measured, HOUSE-00087). `cna-content --fx-compiler-launcher wine` hands `fxc.exe`
# the arguments it built, and those carry ordinary Unix absolute paths:
#
#     wine fxc.exe /nologo /T fx_2_0 /Fo /tmp/cna-fx-0-.../effect.fxb ... /rv/.../P1Effect.fx
#
# `fxc` is a Windows tool and uses `/` to introduce OPTIONS, so it reads `/tmp/...` as a switch:
#
#     error: Unknown or invalid option '/tmp/cna-fx-0-140733683835328/effect.fxb', use /? for help
#
# The build is not broken and CNA is not wrong -- the two conventions simply collide, and
# `--fx-compiler-launcher` is the seam provided for exactly this. This script is that launcher: it
# translates every argument that names an existing path, or that follows a path-taking option, into
# its Windows form (`Z:\tmp\...`), and passes everything else through untouched.
#
# This is OFFLINE TOOLING. It is not runtime code, it is not subject to the XNA-only rule, and it
# runs only when a Tier E build is asked for. A machine without Wine or without an `fxc` simply does
# not build Tier E, which is exactly what ADR-0003 requires.
#
# Usage (as CNA invokes it):
#     fxc-wine.sh <path-to-fxc.exe> <fxc arguments...>
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <fxc.exe> [arguments...]" >&2
    exit 2
fi

if ! command -v wine >/dev/null 2>&1; then
    echo "fxc-wine.sh: wine is not installed; Tier E cannot be built on this machine." >&2
    exit 127
fi

# `winepath -w` is the authority on the mapping and honours whatever drive mappings the prefix has;
# the Z: fallback is only for a prefix where winepath is unavailable.
to_windows() {
    local p="$1"
    if command -v winepath >/dev/null 2>&1; then
        winepath -w "$p" 2>/dev/null || printf 'Z:%s' "${p//\//\\}"
    else
        printf 'Z:%s' "${p//\//\\}"
    fi
}

args=()
expect_path=0
for arg in "$@"; do
    if [[ $expect_path -eq 1 ]]; then
        args+=("$(to_windows "$arg")")
        expect_path=0
        continue
    fi
    case "$arg" in
        # options whose NEXT argument is a path
        /Fo|/Fc|/Fh|/Fe|/Fx|/I)
            args+=("$arg")
            expect_path=1
            ;;
        # the same options written joined, e.g. /Fo/tmp/x.fxb
        /Fo*|/Fc*|/Fh*|/Fe*|/Fx*|/I*)
            args+=("${arg:0:3}$(to_windows "${arg:3}")")
            ;;
        # A bare argument that starts with "/" is ambiguous: it is either a Unix path or an fxc
        # switch. Only an argument that names something that ACTUALLY EXISTS is translated -- the
        # source file does, and `/T` or `/nologo` do not. Testing the parent directory instead would
        # translate `/T` too, because `dirname /T` is `/`, which is the bug this comment exists to
        # stop anyone reintroducing.
        /*)
            if [[ -e "$arg" ]]; then
                args+=("$(to_windows "$arg")")
            else
                args+=("$arg")
            fi
            ;;
        *)
            args+=("$arg")
            ;;
    esac
done

# WINEDEBUG silences the fixme spam that would otherwise be parsed as compiler diagnostics.
exec env WINEDEBUG="${WINEDEBUG:--all}" wine "${args[@]}"
