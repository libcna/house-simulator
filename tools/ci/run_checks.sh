#!/usr/bin/env bash
# Run every static gate CI runs, in the order CI runs them. This is what a pre-commit hook and a
# contributor both invoke; keeping one script means the local answer and the CI answer cannot
# disagree.
#
# Usage:
#   tools/ci/run_checks.sh              check everything
#   tools/ci/run_checks.sh --staged     check only files staged for commit (used by the hook)
#   tools/ci/run_checks.sh --fix        reformat in place, then check
#
# Exit status: 0 all gates green, 1 at least one gate failed.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 1

MODE="all"
FIX=0
for arg in "$@"; do
    case "$arg" in
        --staged) MODE="staged" ;;
        --fix)    FIX=1 ;;
        -h|--help)
            sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo "run_checks: unknown option '$arg'" >&2
            exit 1
            ;;
    esac
done

if [[ -t 1 ]]; then
    BOLD=$'\033[1m'; RED=$'\033[31m'; GREEN=$'\033[32m'; RESET=$'\033[0m'
else
    BOLD=""; RED=""; GREEN=""; RESET=""
fi

FAILED=()

run_gate()
{
    local name="$1"; shift
    printf '%s==> %s%s\n' "$BOLD" "$name" "$RESET"
    if "$@"; then
        return 0
    fi
    FAILED+=("$name")
    return 1
}

# ---------------------------------------------------------------------------------------------
# Which C++ files to format-check
# ---------------------------------------------------------------------------------------------

cxx_files()
{
    if [[ "$MODE" == "staged" ]]; then
        git diff --cached --name-only --diff-filter=ACMR \
            -- src include tests \
            | grep -E '\.(cpp|cc|cxx|hpp|h|hh|hxx|inl|ipp)$' || true
    else
        find src include tests -type f \
            \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' \
               -o -name '*.hpp' -o -name '*.h' -o -name '*.hh' -o -name '*.hxx' \
               -o -name '*.inl' -o -name '*.ipp' \) 2>/dev/null | sort
    fi
}

check_format()
{
    if ! command -v clang-format >/dev/null 2>&1; then
        echo "clang-format not found -- install it; CI will run this gate regardless." >&2
        return 1
    fi

    local files
    mapfile -t files < <(cxx_files)
    if [[ ${#files[@]} -eq 0 ]]; then
        echo "no C++ files to check"
        return 0
    fi

    if [[ $FIX -eq 1 ]]; then
        clang-format -i "${files[@]}" || return 1
    fi
    clang-format --dry-run -Werror "${files[@]}"
}

# ---------------------------------------------------------------------------------------------

run_gate "layout"     python3 tools/ci/check_layout.py
run_gate "xna-only"   python3 tools/ci/check_xna_only.py
run_gate "clang-format" check_format
# Needs no compiler and no Wine, which is exactly why it can be a gate: it compares the committed
# `.xnb` against the committed `.fx` by hash (`HOUSE-00185`). A stale baseline is otherwise invisible
# on every machine that cannot compile effects -- which is most of them.
run_gate "effects-baseline" tools/effects/build_effects.sh --check

echo
if [[ ${#FAILED[@]} -eq 0 ]]; then
    printf '%s%sall gates green%s\n' "$BOLD" "$GREEN" "$RESET"
    exit 0
fi

printf '%s%sFAILED: %s%s\n' "$BOLD" "$RED" "${FAILED[*]}" "$RESET"
exit 1
