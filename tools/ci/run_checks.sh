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
# `cna-house.md` §20.1: no row, no build. An unlisted file under `assets-src/` is a file whose
# licence nobody has looked at, and a gate is the only moment anyone reliably looks (`HOUSE-00196`).
run_gate "manifest"   python3 tools/ci/check_manifest.py
# And the credits document cannot drift from the manifest it is generated from (`HOUSE-00198`).
run_gate "licences"   python3 tools/assets/verify_licences.py --check
# `HOUSE-00203`. The committed report is generated from the manifest ALONE, so this gate needs no
# build tree and gives the same answer everywhere. Its compiled column is empty by design; pass
# --content/--effects by hand for the numbers the pack budgets are written against.
run_gate "budget"     python3 tools/ci/budget_report.py --check
# `HOUSE-00217`. The stage table in docs/content-build.md is generated from the pipeline
# graph, so a stage added without regenerating it is a documented order that is no longer
# the order. This is the same idiom the budget report uses, for the same reason.
run_gate "content-doc" python3 tools/ci/build_content.py --check-docs
# `HOUSE-00341`. The sixteen world files' JSON Schemas are generated from one source so that the
# shared id pattern, vector and range cannot drift between them; docs/world-schema/ is what an
# editor and `validate_world.py` read, and a stale copy of it is worse than none.
run_gate "world-schema" python3 tools/world/world_schema.py --check
# `HOUSE-00280`. The surface map is counted from the manifest, so importing or dropping a
# footstep sample changes it. A stale map is HOUSE-00281 sourcing the wrong list.
run_gate "footsteps" python3 tools/assets/footstep_map.py --check
# Every glTF in the tree must import cleanly, warnings included (`HOUSE-00186`). Cheap while the
# tree is small; when it is not, it moves to the content job.
run_gate "gltf"       python3 tools/assets/gltf_validate.py
# Size and origin, driven by the manifest's `category` (`HOUSE-00187`, `HOUSE-00188`). A scale error
# is the commonest defect in a downloaded asset and the hardest to see in isolation.
run_gate "scale"      python3 tools/assets/scale_check.py
run_gate "origin"     python3 tools/assets/origin_check.py
# `HOUSE-00225`. Two silent failures: a `.chanim` whose joint list does not resolve in the compiled
# model's `Model::Bones` -- fatal at LOAD, in the game, for an asset that built cleanly -- and a
# source `.glb` with two skins. The first needs a build tree and is skipped without one; the second
# is checked over the whole source tree either way.
run_gate "anim"       python3 tools/ci/check_anim_assets.py
# Every `.spritefont` must rasterise a face FROM THIS REPOSITORY (`HOUSE-00200`). The content
# pipeline only WARNS when it falls back to an installed font, and a warning does not stop a build
# that then embeds the host's glyphs; this gate does. It also catches a region asking for a
# character the face cannot draw, which the pipeline treats as fatal but only once it runs.
run_gate "fonts"      python3 tools/ci/check_fonts.py
# The XNA-only rule, asked of the COMPILER rather than of the source text (`HOUSE-00168`).
# `check_xna_only.py` matches identifiers and therefore cannot see a call that reaches a CNAEXT
# member through overload resolution -- `KeyboardState{}`, `Color(byte,byte,byte,byte)`,
# `setIsLoopedProperty(true)`. This recompiles every translation unit with `CNA_STRICT_XNA_API`,
# which turns CNA's own CNAEXT tag into `[[deprecated]]`, and reports what the compiler actually
# chose.
#
# NOT in the pre-commit path: it needs a compile database and about 25 s, and the hook's whole
# value is being fast enough that nobody disables it. CI runs the full script, so CI runs this.
if [[ "$MODE" != "staged" ]]; then
    run_gate "xna-strict" python3 tools/ci/check_xna_strict.py --all
fi

echo
if [[ ${#FAILED[@]} -eq 0 ]]; then
    printf '%s%sall gates green%s\n' "$BOLD" "$GREEN" "$RESET"
    exit 0
fi

printf '%s%sFAILED: %s%s\n' "$BOLD" "$RED" "${FAILED[*]}" "$RESET"
exit 1
