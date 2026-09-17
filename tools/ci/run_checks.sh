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
# HOUSE-01259: the close-range porch fixture is a deterministic project-authored GLB, and its
# exact two-slot split is what lets the runtime switch only the diffuser rather than the cage.
run_gate "porch-lantern" python3 tools/assets/porch_lantern.py --check
# HOUSE-01047: the manually switched garage wall pack is another physical linked fixture, not a
# bare light point or a one-off binary checked in without its deterministic source.
run_gate "garage-floodlight" python3 tools/assets/garage_floodlight.py --check
# HOUSE-01040: the project-authored close-range kitchen joinery must remain reproducible from
# its measured Blender source, including the same small collidable GLBs and manifest hashes.
run_gate "kitchen-builtins" python3 tools/assets/kitchen_builtins_prepare.py --check
# HOUSE-01049: the measured counter stools and three restrained worktop groupings remain
# reproducible from their authored Blender source, with exact material slots and placement.
run_gate "kitchen-dressing" python3 tools/assets/kitchen_dressing_prepare.py --check
# HOUSE-01283: the island's three linked emitters reuse one deterministic close-range fixture;
# the gate protects its physical shade/optical-point contract and canonical alignment.
run_gate "kitchen-pendant" python3 tools/assets/kitchen_pendant_prepare.py --check
# HOUSE-01284: the existing range-task group is four physical hood-mounted pucks rather than
# ceiling points; the gate protects the exact model, placement and short downward-light contract.
run_gate "kitchen-task-puck" python3 tools/assets/kitchen_task_puck_prepare.py --check
# HOUSE-01050: the living-room source's television and `BlackMarble` media slot must not regress
# to the cream upholstery mapping that made the family focal wall a set of blank rectangles.
run_gate "family-media-finish" python3 tools/assets/family_media_finish.py --check
# HOUSE-01051/HOUSE-01054: four project-authored secondary props remain deterministic, measured
# and linked to canonical placements; the dog bed retains its finished soft component/material set.
run_gate "family-secondary" python3 tools/assets/family_secondary_prepare.py --check
# HOUSE-01052: the close-range family sofa remains the pinned, attributed Wayfair geometry after
# deterministic XNA-compatible variant selection, grounding and bounded proxy generation.
run_gate "family-sofa" python3 tools/assets/family_sofa_prepare.py --check
# HOUSE-01055: all three close-range route armchairs remain the pinned CC0 Wayfair geometry after
# deterministic label removal, canonical material mapping, grounding and proxy generation.
run_gate "visual-slice-chair" python3 tools/assets/visual_slice_chair_prepare.py --check
# HOUSE-01057: the formal sofa stays within the hero-furniture budget after topology-aware
# preparation, retains its five authored base-colour maps and keeps a valid LOD/proxy ladder.
run_gate "formal-living-sofa" python3 tools/blender/living_sofa_prepare.py --check
# HOUSE-01058: the formerly empty piano wall remains a measured bench/art/physical-light
# composition, with the existing default-on accent linked to the exact diffuser slot.
run_gate "living-piano-vignette" python3 tools/assets/living_piano_vignette_prepare.py --check
# HOUSE-01053: the family focal wall retains the pinned CC0 low cabinet, exact canonical roles,
# measured television gap and family-only placement without unsupported source metadata.
run_gate "family-media-console" python3 tools/assets/family_media_console_prepare.py --check
# HOUSE-01285: the family room's four main lights are physical linked fixtures, and its existing
# floor lamp emits from its real shade rather than from an unrelated bare point in the room.
run_gate "family-ceiling-light" python3 tools/assets/family_ceiling_light_prepare.py --check
# HOUSE-01045: the formal-room piano is a close-range deterministic authored asset, not a
# one-off binary or a cuboid standing in for furniture.
run_gate "living-piano" python3 tools/assets/living_piano_prepare.py --check
# HOUSE-01048: the formal table, chair and physical chandelier remain reproducible measured assets.
run_gate "dining-suite" python3 tools/assets/dining_suite_prepare.py --check
# And the credits document cannot drift from the manifest it is generated from (`HOUSE-00198`).
run_gate "licences-selftest" python3 tools/assets/verify_licences.py --selftest
run_gate "licences"   python3 tools/assets/verify_licences.py --check
# `HOUSE-00296`. The 34 fixed ambientCG selections are three-map sets, not an unstructured pile of
# PNGs: exact category counts, equal channel dimensions, opaque RGBA data and the retained
# sphere-and-floor review all remain executable acceptance criteria.
run_gate "base-materials-selftest" python3 tools/assets/ambientcg_materials.py --selftest
run_gate "base-materials" python3 tools/assets/ambientcg_materials.py --check
run_gate "base-material-mapping" python3 tools/assets/pbr_to_stock.py --check-base-materials
run_gate "base-material-previews" python3 tools/blender/material_preview.py --check
run_gate "interior-paint-variants" python3 tools/assets/paint_variants.py --check
run_gate "interior-floor-materials" python3 tools/assets/floor_materials.py --check
run_gate "exterior-materials" python3 tools/assets/exterior_materials.py --check
run_gate "outdoor-static-materials" python3 tools/assets/outdoor_static_materials.py --check
run_gate "glass-water-materials" python3 tools/assets/glass_water_materials.py --check
run_gate "wet-materials" python3 tools/assets/wet_materials.py --check
run_gate "snow-materials-selftest" python3 tools/assets/snow_materials.py --selftest
run_gate "snow-materials" python3 tools/assets/snow_materials.py --check
run_gate "room-palettes-selftest" python3 tools/world/room_palettes.py --selftest
run_gate "room-palettes" python3 tools/world/room_palettes.py --check
# `HOUSE-00297`. Counts alone would accept repeated files and a model lying on its side: the set
# gate checks pinned source identities, hashes, per-age scale, triangle/LOD ratios and textures;
# the compact render sheets retain the alpha-aware visual review.
run_gate "vegetation-selftest" python3 tools/assets/polyhaven_vegetation.py --selftest
run_gate "vegetation" python3 tools/assets/polyhaven_vegetation.py --check
run_gate "vegetation-previews" python3 tools/blender/vegetation_preview.py --check
# `HOUSE-00203`. The committed report is generated from the manifest ALONE, so this gate needs no
# build tree and gives the same answer everywhere. Its compiled column is empty by design; pass
# --content/--effects by hand for the numbers the pack budgets are written against.
run_gate "budget"     python3 tools/ci/budget_report.py --check
# `HOUSE-00217`. The stage table in docs/content-build.md is generated from the pipeline
# graph, so a stage added without regenerating it is a documented order that is no longer
# the order. This is the same idiom the budget report uses, for the same reason.
run_gate "content-doc" python3 tools/ci/build_content.py --check-docs
# `HOUSE-00227`. The graph's own claims, next to the table generated from it: the four generators
# that write the exterior `.glb` tree are stages, and `chunks` hashes what they wrote. Until this
# task none of them was run by anything, and `build/terrain` held tiles three hours older than the
# height field they are drawn from while every render reference of the outdoors pictured them.
run_gate "content-graph" python3 tools/ci/build_content.py --selftest
# `HOUSE-00341`. The sixteen world files' JSON Schemas are generated from one source so that the
# shared id pattern, vector and range cannot drift between them; docs/world-schema/ is what an
# editor and `validate_world.py` read, and a stale copy of it is worse than none.
run_gate "world-schema" python3 tools/world/world_schema.py --check
run_gate "world-rules" check_world
# `HOUSE-01707`. The validator's synthetic world is the mutation suite for all thirteen semantic
# rules. A schema change once made that fixture invalid, silently preventing every mutation from
# reaching the rules; running it here keeps the test of the gate as current as the gate itself.
run_gate "world-rules-selftest" python3 tools/world/validate_world.py --selftest
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
# `HOUSE-00785`. `fence_gen.py` refuses an exterior structure it has no builder for -- which is
# right -- and NOTHING RAN IT: `HOUSE-00775` added §10.4's stone wall to the layout and the
# generator has been unable to produce a single fence since, silently, while `build/fence` kept
# the files it wrote before. The neighbourhood grammar joins it here rather than waiting for the
# same thing to happen twice.
run_gate "fence-gen" python3 tools/world/fence_gen.py --selftest
run_gate "neighbourhood" python3 tools/world/neighbourhood_gen.py --selftest
# `HOUSE-00856`. The generator above wrote `build/neighbourhood/*.glb` for fifteen tasks and NOTHING
# read that directory. This is the tool that does, and its round-trip claims are what stop the
# writer and `NeighbourhoodReader` drifting apart.
run_gate "neighbourhood-bin" python3 tools/world/build_neighbourhood.py --selftest
run_gate "world-deploy" python3 tools/world/deploy_world.py --check
# Furniture-aware pet waypoints must keep conservative mesh broad-phase equivalent to the exact
# triangle-distance query. The fixture is cheap; the production graph is a content-build stage.
run_gate "nav-selftest" python3 tools/world/build_nav.py --selftest
# `HOUSE-00477`. §70.5 over the GENERATED SHELL, not over the layout: `validate_world.py` rule 10
# checks the numbers an author typed and this checks the geometry the generator made of them. It
# pins the exact set of problems the house has, so a new one fails the day it appears. Skipped,
# loudly, on a checkout that has not run `house_shell_gen.py` -- the shell is not committed.
run_gate "shell-realism" python3 tools/world/verify_shell.py --selftest
# `HOUSE-00482`. The shell is a build product and has no `assets-src/` row, so §20.1's provenance
# question is answered here instead: which generator, at which version, over which layout,
# producing which bytes. Needs Blender, and says so loudly on a checkout without a shell.
run_gate "shell-manifest" python3 tools/blender/house_shell_gen.py --check-manifest
# `HOUSE-00907`. The manifest proves which bytes were generated; this proves every shell slot in
# those bytes resolves to the exact room/opening/stair palette and no `BLOCKOUT_*` survived.
run_gate "shell-materials" python3 tools/blender/house_shell_gen.py --check-materials
# `HOUSE-00399`. An id is the only durable name anything has, and a save file is a list of them
# (§68). Renaming a room leaves the layout internally consistent and every save broken, so none of
# §15.7's twelve rules can see it. This gate can: the golden list is append-only, and an id that
# leaves it fails until a person deletes the line and says why.
run_gate "world-ids" python3 tools/world/id_golden.py --check
# `HOUSE-01532`. §35.1's calendar is checked against 500 conversions Python computed, and the value
# of that table is entirely in its coming from somewhere else. A hand-edited line is a weakened
# test that nothing else in the project would notice, so the table is regenerated and compared.
run_gate "calendar-table" python3 tools/ci/calendar_table.py --check
# `HOUSE-01561`. §32.1's sun is checked against 390 rise and set times published by the US Naval
# Observatory. Same reasoning as the calendar table and the same failure mode: the whole value of
# the fixture is that we did not compute it, so the extract is regenerated from the cached
# responses and compared. `--check` never touches the network; only `--fetch` does.
run_gate "suntimes-table" python3 tools/ci/suntimes_table.py --check
run_gate "suntimes-selftest" python3 tools/ci/suntimes_table.py --selftest
# `HOUSE-01601`. The compact lunar series is checked against 60 moonrise times published by the
# US Naval Observatory. The responses are cached so these two gates remain entirely offline.
run_gate "moontimes-table" python3 tools/ci/moontimes_table.py --check
run_gate "moontimes-selftest" python3 tools/ci/moontimes_table.py --selftest
# `HOUSE-01602`. The phase calculation is compared with 60 USNO-published noon illumination and
# waxing/waning observations, retained beside the rise-time fixture but regenerated independently.
run_gate "moonphases-table" python3 tools/ci/moonphases_table.py --check
run_gate "moonphases-selftest" python3 tools/ci/moonphases_table.py --selftest
# `HOUSE-01641`. The 32 committed gradient rows are generated from eleven art-direction anchors;
# the selftest expands the compact cloud and azimuth terms into the conceptual 32 x 8 x 16 table.
# Both matter: `--check` catches a hand edit and `--selftest` catches a plausible but broken model.
run_gate "sky-lut" python3 tools/world/sky_lut.py --check
run_gate "sky-lut-selftest" python3 tools/world/sky_lut.py --selftest
# `HOUSE-01646`. These are source textures, not screenshots: all three must remain exact generated
# 1024-square RGBA tiles, with a periodic seam and distinct sparse/body/overcast alpha profiles.
run_gate "cloud-textures" python3 tools/world/cloud_textures.py --check
run_gate "cloud-textures-selftest" python3 tools/world/cloud_textures.py --selftest
# `HOUSE-01605`. The committed near-side map is a deterministic projection of one hash-pinned
# NASA TIFF. Normal CI stays offline while checking both the exact asset and the projection maths.
run_gate "moon-albedo" python3 tools/assets/moon_albedo.py --check
run_gate "moon-albedo-selftest" python3 tools/assets/moon_albedo.py --selftest
# `HOUSE-01609`. The complete 9,110-row NASA HEASARC snapshot and the selected 1,500-row binary
# are both hash-pinned. The second gate independently exercises selection, known stars and every
# binary-header refusal without touching the network.
run_gate "star-catalogue" python3 tools/world/build_stars.py --check
run_gate "star-catalogue-selftest" python3 tools/world/build_stars.py --selftest
# `HOUSE-01642`. Geometry that begins with a ring of duplicate poles can look right while carrying
# zero-area triangles, so the offline mesh proves its topology before `SkySystem` ever uploads it.
run_gate "sky-dome-selftest" python3 tools/world/build_skydome.py --selftest
# `HOUSE-01543`. §36.3: *"Season is a continuous phase, never an enum... never a switch."* The
# season code being right is no protection at all against a consumer writing
# `switch (phase.primary)`, and nothing in a test of the season itself would notice. This is the
# rule enforced where it can be, at the point of use.
run_gate "season-usage" python3 tools/ci/check_season_usage.py
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
