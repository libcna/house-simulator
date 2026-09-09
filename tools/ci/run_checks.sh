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

# `HOUSE-00363`. The twelve rules of §15.7 over the authored layout. Skipped, loudly, until
# `HOUSE-00366` writes the first world file -- a gate over nothing must say so rather than print
# a green line that means "there was nothing to check". Unlike the rest of this script it needs
# `jsonschema`, so a checkout without it is told, not quietly passed.
check_world()
{
    if ! compgen -G "assets-src/world/*.json" >/dev/null; then
        echo "no world files yet (HOUSE-00366 writes the first) -- nothing to validate"
        return 0
    fi
    if ! python3 -c 'import jsonschema' 2>/dev/null; then
        echo "validate_world needs jsonschema: python3 -m pip install --user jsonschema" >&2
        return 1
    fi
    python3 tools/world/validate_world.py assets-src/world
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
run_gate "world-rules" check_world
# `HOUSE-00421`. The deployed copy is what the game reads: `content/world/` is the authored JSONC
# with its comments stripped, plus the `world.manifest.json` that hashes those bytes
# (`HOUSE-00364`). A stale deploy is a house that does not match the one in the repository, and
# nothing in the frame would say so. `--check` writes nothing and passes when nothing is deployed
# yet, which is the state a fresh checkout is in.
# `HOUSE-00380`. §12.6's counts table is maintained by hand and has already been found fifteen
# windows out (`HOUSE-00376`). This compares it, cell by cell, with the layout, and regenerates
# docs/window-schedule.md -- the per-window schedule §12.6 said lived in a file that never existed.
# `HOUSE-00396`. §16.1-§16.3 are written by hand and were designed before the layout existed.
# This checks both directions: every portal §16's adjacency tables name exists, and every §16.3
# metric row states the number the layout actually measures.
run_gate "graph-report" python3 tools/world/report_graph.py assets-src/world --check
run_gate "window-schedule" python3 tools/world/window_schedule.py --check
# `HOUSE-00398`. The plans are drawn from the layout, so a stale SVG is a plan of a house that no
# longer exists -- which is worse than no plan, because somebody will act on it.
run_gate "floor-plans" python3 tools/world/floor_plans.py --check
# `HOUSE-00761`. §11.5's ground is GENERATED from the layout's slope and pads, so a committed
# `terrain.png` that no longer matches the layout is a lawn under a terrace that has moved. This
# gate was missing until `HOUSE-00553` needed the ground to collide with and found three balconies
# in it -- the artefact had a `--check` and nothing ran it.
run_gate "terrain-gen" python3 tools/world/terrain_gen.py --check
run_gate "world-deploy" python3 tools/world/deploy_world.py --check
# `HOUSE-00477`. §70.5 over the GENERATED SHELL, not over the layout: `validate_world.py` rule 10
# checks the numbers an author typed and this checks the geometry the generator made of them. It
# pins the exact set of problems the house has, so a new one fails the day it appears. Skipped,
# loudly, on a checkout that has not run `house_shell_gen.py` -- the shell is not committed.
run_gate "shell-realism" python3 tools/world/verify_shell.py --selftest
# `HOUSE-00482`. The shell is a build product and has no `assets-src/` row, so §20.1's provenance
# question is answered here instead: which generator, at which version, over which layout,
# producing which bytes. Needs Blender, and says so loudly on a checkout without a shell.
run_gate "shell-manifest" python3 tools/blender/house_shell_gen.py --check-manifest
# `HOUSE-00399`. An id is the only durable name anything has, and a save file is a list of them
# (§68). Renaming a room leaves the layout internally consistent and every save broken, so none of
# §15.7's twelve rules can see it. This gate can: the golden list is append-only, and an id that
# leaves it fails until a person deletes the line and says why.
run_gate "world-ids" python3 tools/world/id_golden.py --check
# `HOUSE-00400`. §13's room schedule is 94 rows of hand-maintained numbers over data that changes
# every time a room does. Walking it against the layout found 52 disagreements, including a garage
# 5 m² too small and a central hall with three doorways written as `0`.
run_gate "room-schedule" python3 tools/world/room_schedule.py --check
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
