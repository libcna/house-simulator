# cna-house -- root build.
#
# `cna-house` consumes `../cna` and `../sharp-runtime` as SIBLING SOURCE CHECKOUTS by
# `add_subdirectory` (`cna-house.md` §8.1). No vendoring, no submodules, no fetch: the sibling is
# built as part of this build, from the source the developer has, so a CNA change is visible here
# immediately and a CNA version is never silently pinned.
#
# Build directories are the openeggbert closed list -- `build/`, `build-asan/`, `build-ubsan/`,
# `build-tsan/`, `build-probe/`, `build-consumer/` -- in this repository, never in `/tmp` or a
# session scratchpad. `CMakePresets.json` encodes them; see `HOUSE-00035`.
cmake_minimum_required(VERSION 3.24)

project(cna_house
        VERSION 1.1.0
        DESCRIPTION "A house you can walk around in, built on XNA 4.0 as CNA implements it"
        LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ------------------------------------------------------------------------------------------------
# The sibling checkouts
# ------------------------------------------------------------------------------------------------
set(CNAHOUSE_CNA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../cna"
    CACHE PATH "The CNA checkout this build consumes")
set(CNAHOUSE_SHARP_RUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../sharp-runtime"
    CACHE PATH "The sharp-runtime checkout this build consumes")

foreach(_sibling "${CNAHOUSE_CNA_ROOT}" "${CNAHOUSE_SHARP_RUNTIME_ROOT}")
    if(NOT EXISTS "${_sibling}/CMakeLists.txt")
        message(FATAL_ERROR
            "cna-house: no CMake project at '${_sibling}'.\n"
            "  CNA and sharp-runtime are SIBLING CHECKOUTS, not submodules -- "
            "`git submodule update --init` will not fetch them.\n"
            "  Clone them beside this repository, or point -DCNAHOUSE_CNA_ROOT / "
            "-DCNAHOUSE_SHARP_RUNTIME_ROOT at them.")
    endif()
endforeach()

# ------------------------------------------------------------------------------------------------
# The CNA configuration this project pins (`cna-house.md` §8.1)
# ------------------------------------------------------------------------------------------------
# The renderer is fixed at CONFIGURE time and standard XNA 4.0 cannot change it afterwards, so a
# separate binary is produced per renderer (`cna-house.md` §7.3). There is deliberately no
# `--renderer` command-line option (`HOUSE-00132`).
set(CNAHOUSE_RENDERER "OPENGLES3" CACHE STRING
    "The CNA renderer this build targets: OPENGLES3 (shipping), HEADLESS (CI), OPENGL33 (dev)")
set_property(CACHE CNAHOUSE_RENDERER PROPERTY STRINGS OPENGLES3 HEADLESS OPENGL33 WEBGL2)

set(CNA_GRAPHICS_RENDERER "${CNAHOUSE_RENDERER}" CACHE STRING "" FORCE)
set(CNA_PLATFORM          "SDL3"                 CACHE STRING "" FORCE)
set(CNA_AUDIO_PLATFORM    "SDL3"                 CACHE STRING "" FORCE)
set(CNA_SHARP_RUNTIME_ROOT "${CNAHOUSE_SHARP_RUNTIME_ROOT}" CACHE PATH "" FORCE)

# ADR-0001. This is the one option that is never weakened, in any configuration, by anyone. With it
# off, every file in `modules/graphics-ext/` is compiled out and the forbidden layer does not exist
# in the binary -- `HOUSE-00063` measured 0 `CNA::Graphics::` symbols against a control of 6 277.
set(CNA_CNAEXT   OFF CACHE BOOL "" FORCE)
set(CNA_DEVICES  OFF CACHE BOOL "" FORCE)

set(CNA_EASYGL_COMPILED_EFFECTS ON   CACHE BOOL   "" FORCE)   # Tier E; see cmake/TierSelection.cmake
set(CNA_ENABLE_VIDEO            "AUTO" CACHE STRING "" FORCE) # the television, phase 21
set(CNA_ENABLE_NET              OFF  CACHE BOOL   "" FORCE)   # ADR-0011: no networking
set(CNA_BUILD_TESTS             OFF  CACHE BOOL   "" FORCE)
set(CNA_BUILD_EXAMPLES          OFF  CACHE BOOL   "" FORCE)
set(SHARP_RUNTIME_BUILD_TESTS   OFF  CACHE BOOL   "" FORCE)

# MEASURED (`HOUSE-00076`): `Text.Json` is NOT in CNA's default component closure and CNA does not
# link it, so its include directories never reach a consumer through the `CNA` target. `cna-house`'s
# world data, save files and animation sidecars are all `System::Text::Json`, so the component is
# selected here AND linked explicitly by `cnahouse_core` below. Selecting it without linking it
# compiles nothing; linking it without selecting it does not exist as a target.
set(SHARP_RUNTIME_COMPONENTS
    "Core.Base;IO;Collections.Core;Collections.ObjectModel;Runtime;Threading;Text;Text.Json;Globalization;Storage;Security.Cryptography;Xml.Serialization"
    CACHE STRING "" FORCE)

add_subdirectory("${CNAHOUSE_CNA_ROOT}" CNA_BUILD)

# `cna-house` compiles at `-Wall -Wextra -Wpedantic -Werror` (`HOUSE-00124`). CNA and sharp-runtime
# do not, and they are entitled not to: `-Wpedantic` fires on sharp-runtime's `unsigned __int128`
# and `-Wshadow` on several CNA headers, and neither is a defect. Promoting a sibling's warnings to
# OUR errors would mean this project's policy breaks someone else's code, which is not ours to do.
#
# So every target the siblings define has its interface includes marked SYSTEM. Walking the whole
# subdirectory tree rather than naming `CNA` and a couple of components is deliberate: the include
# that actually broke the build first was sharp-runtime's `Decimal.hpp`, reached transitively
# through `Game.hpp`, which naming targets by hand would have missed.
function(cnahouse_mark_includes_system directory)
    get_property(_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_target IN LISTS _targets)
        get_target_property(_type ${_target} TYPE)
        if(_type STREQUAL "UTILITY")
            continue()
        endif()
        get_target_property(_includes ${_target} INTERFACE_INCLUDE_DIRECTORIES)
        if(_includes)
            set_target_properties(${_target} PROPERTIES
                INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_includes}")
        endif()
    endforeach()
    get_property(_children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(_child IN LISTS _children)
        cnahouse_mark_includes_system("${_child}")
    endforeach()
endfunction()

cnahouse_mark_includes_system("${CNAHOUSE_CNA_ROOT}")

include(TierSelection)

# ------------------------------------------------------------------------------------------------
# Warnings (`HOUSE-00124`)
# ------------------------------------------------------------------------------------------------
# Applied to `cna-house`'s OWN targets only, through an interface library. Promoting warnings to
# errors across `add_subdirectory`d siblings would make this project's policy break someone else's
# build, which is not ours to do.
add_library(cnahouse_warnings INTERFACE)
target_compile_options(cnahouse_warnings INTERFACE
    -Wall -Wextra -Wpedantic
    -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Woverloaded-virtual
    -Wconversion -Wsign-conversion -Wdouble-promotion -Wnull-dereference
    $<$<BOOL:${CNAHOUSE_WERROR}>:-Werror>)

option(CNAHOUSE_WERROR "Promote warnings to errors in cna-house's own targets" ON)

# ------------------------------------------------------------------------------------------------
# The library and the executable (`HOUSE-00123`)
# ------------------------------------------------------------------------------------------------
# Everything except `main` lives in `cnahouse_core`, so the test targets link the same code the
# game runs rather than a recompiled variant of it.
add_subdirectory(src)

add_executable(cna-house src/app/Main.cpp)
target_link_libraries(cna-house PRIVATE cnahouse_core cnahouse_warnings)
set_target_properties(cna-house PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# ------------------------------------------------------------------------------------------------
# Content (`HOUSE-00126`)
# ------------------------------------------------------------------------------------------------
# Two trees, in two containers, for measured reasons:
#
#   `content/`      the `.cnb` tree -- models, fonts, audio, video, world data. The default.
#   `content-fx/`   the `.xnb` tree -- compiled effects, and later the DXT textures, because
#                   `HOUSE-00111` measured that CNB texture schema 1 is frozen to `Rgba8` and a
#                   `textureFormat` asking for DXT is warned about and silently kept uncompressed.
#
# `HOUSE-00064` measured that **`.xnb` wins** the content resolution order, so a deliberately mixed
# tree is coherent and an accidental one silently shadows what the build just produced. Separate
# output roots are what keep it deliberate.
#
# `cna_add_content` has no format option -- it always produces `.cnb` -- so the two trees cannot
# share a source root: a `.fx` under a `.cnb` build is attempted and fails. The `.cnb` build
# therefore walks the asset subdirectories EXCEPT `Effects/`, and Tier E's `.xnb` build owns that
# one. Naming the subdirectories is not a workaround; it is the same split the containers already
# force, made visible.
include(ToolContentPipeline OPTIONAL)

set(CNAHOUSE_CNB_ASSET_DIRS Models Textures Audio Fonts world)
set(CNAHOUSE_CONTENT_EXECUTABLE "" CACHE FILEPATH
    "Host cna-content executable used by cross-compiling configurations")

set(_cnahouse_content_tool_args "")
if(CNAHOUSE_CONTENT_EXECUTABLE)
    if(NOT EXISTS "${CNAHOUSE_CONTENT_EXECUTABLE}")
        message(FATAL_ERROR
            "cna-house: CNAHOUSE_CONTENT_EXECUTABLE does not exist: "
            "'${CNAHOUSE_CONTENT_EXECUTABLE}'")
    endif()
    list(APPEND _cnahouse_content_tool_args
        CONTENT_EXECUTABLE "${CNAHOUSE_CONTENT_EXECUTABLE}")
elseif(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "cna-house: a cross build needs a host cna-content tool. Build the native 'cna_content_tool' "
        "target first, then set CNAHOUSE_CONTENT_EXECUTABLE to that executable.")
endif()

# `Media/` is built differently, and the reason is MEASURED (`HOUSE-00201`). A `Video` (and a
# `Song`) compiles to a small metadata `.cnb` plus a byte-identical copy of the media file, and the
# runtime resolves that copy through `ContentManager::BuildAssetPath` -- that is, **relative to the
# CONTENT ROOT**, not to the `.cnb` beside it. Every other asset type is resolved relative to the
# output directory it was built into, which is why building one subdirectory at a time works for
# them and cannot work for media: with `OUTPUT_DIR = content/Video`, a stream reference of
# `smoke_clip.ogv` deploys to `content/Video/smoke_clip.ogv` and is looked for at
# `content/smoke_clip.ogv`, and a reference of `Video/smoke_clip.ogv` deploys to
# `content/Video/Video/smoke_clip.ogv` and is looked for at `content/Video/smoke_clip.ogv`. The two
# never meet.
#
# So the media tree is built with the content root AS its output directory, and carries the
# `Video/` component in its own source layout instead. `assets-src/Media/Video/clip.ogv` then
# produces `content/Video/clip.cnb` and `content/Video/clip.ogv`, the content name is
# `Video/clip` exactly as §21.6 has it, and nothing is copied twice.
set(CNAHOUSE_CNB_MEDIA_DIR Media)

if(COMMAND cna_add_content)
    set(_content_targets "")
    foreach(_dir IN LISTS CNAHOUSE_CNB_ASSET_DIRS CNAHOUSE_CNB_MEDIA_DIR)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets-src/${_dir}")
            continue()
        endif()
        file(GLOB_RECURSE _sources "${CMAKE_CURRENT_SOURCE_DIR}/assets-src/${_dir}/*")
        list(FILTER _sources EXCLUDE REGEX "/\\.gitkeep$")
        if(NOT _sources)
            continue()   # an empty subdirectory is not an error; most of them start empty
        endif()
        set(_out "${CMAKE_BINARY_DIR}/content/${_dir}")
        if(_dir STREQUAL "${CNAHOUSE_CNB_MEDIA_DIR}")
            set(_out "${CMAKE_BINARY_DIR}/content")
        endif()
        cna_add_content(
            TARGET cnahouse_content_${_dir}
            SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets-src/${_dir}"
            OUTPUT_DIR "${_out}"
            ${_cnahouse_content_tool_args})
        list(APPEND _content_targets cnahouse_content_${_dir})
    endforeach()

    add_custom_target(cnahouse_content)
    if(_content_targets)
        add_dependencies(cnahouse_content ${_content_targets})
    endif()
    add_dependencies(cna-house cnahouse_content)
else()
    message(WARNING
        "cna-house: cna_add_content is unavailable, so the content tree is not built by this "
        "configuration. The application starts without it and logs what it could not load.")
endif()

# HOUSE-02891: keep the first browser proof intentionally smaller than the full game. It exercises
# the real WEBGL2 renderer, the blocking XNA Game loop through Asyncify, JS-lowered exceptions and
# an Emscripten data preload containing one compiled SoundEffect. The full `cna-house` target is
# brought across only by HOUSE-02892 after this boundary is known good.
if(EMSCRIPTEN)
    # The full application uses the same proved WebGL2/exception/Asyncify contract as the bounded
    # spike below. Keep the desktop content layout under the same absolute MEMFS paths that
    # Main.cpp derives from XNA's title location; this is a single upfront preload, not streaming.
    target_link_libraries(cna-house PRIVATE
        CNA::EmscriptenExceptionAbi
        CNA::EmscriptenAsyncify)
    # HOUSE-02899: Emscripten's own preload status drives this one-page loading shell; the
    # existing in-game LoadingScreen takes over after the pack has been prepared.
    target_link_options(cna-house PRIVATE
        -sALLOW_MEMORY_GROWTH=1
        -sSTACK_SIZE=1048576
        -sINITIAL_MEMORY=134217728
        -sFORCE_FILESYSTEM=1
        -sEXIT_RUNTIME=1
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/cmake/web_shell.html"
        "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/cmake/web_pointer_lock_pre.js"
        "SHELL:--preload-file ${CMAKE_BINARY_DIR}/web-stage/content@/content"
        "SHELL:--preload-file ${CMAKE_BINARY_DIR}/web-stage/content-fx@/content-fx")
    set_property(TARGET cna-house APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/web_shell.html"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/web_pointer_lock_pre.js")
    set_target_properties(cna-house PROPERTIES SUFFIX ".html")
    cna_apply_emscripten_renderer_link_contract(cna-house)

    set(_web_spike_pack "${CMAKE_BINARY_DIR}/web-spike-pack/content")
    cna_add_content(
        TARGET cnahouse_web_spike_content
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets-src/Audio/Smoke"
        OUTPUT_DIR "${_web_spike_pack}/Audio/Smoke"
        ${_cnahouse_content_tool_args})

    add_executable(cna-house-web-spike src/app/WebSpike.cpp)
    target_link_libraries(cna-house-web-spike PRIVATE
        CNA
        ${RENDERER_TARGET}
        SHARP_RUNTIME
        CNA::EmscriptenExceptionAbi
        CNA::EmscriptenAsyncify
        cnahouse_warnings)
    target_link_options(cna-house-web-spike PRIVATE
        -sALLOW_MEMORY_GROWTH=1
        -sSTACK_SIZE=1048576
        -sINITIAL_MEMORY=134217728
        -sFORCE_FILESYSTEM=1
        -sEXIT_RUNTIME=1
        "SHELL:--preload-file ${_web_spike_pack}@/content")
    set_target_properties(cna-house-web-spike PROPERTIES
        SUFFIX ".html"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    cna_apply_emscripten_renderer_link_contract(cna-house-web-spike)
    add_dependencies(cna-house-web-spike cnahouse_web_spike_content)
endif()

# The generated attribution document is runtime UI data, not compiled content. Keep it under the
# same title-relative content tree as the rest of the read-only payload so Linux, Web and Android
# all reach it through XNA's TitleContainer rather than a process-working-directory filesystem path.
set(CNAHOUSE_CREDITS_DOCUMENT "${CMAKE_BINARY_DIR}/content/Credits/THIRD-PARTY-ASSETS.md")
add_custom_command(
    OUTPUT "${CNAHOUSE_CREDITS_DOCUMENT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/content/Credits"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/licenses/THIRD-PARTY-ASSETS.md" "${CNAHOUSE_CREDITS_DOCUMENT}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/licenses/THIRD-PARTY-ASSETS.md"
    COMMENT "Copying the generated in-game credits document"
    VERBATIM)
add_custom_target(cnahouse_credits DEPENDS "${CNAHOUSE_CREDITS_DOCUMENT}")
add_dependencies(cnahouse_core cnahouse_credits)

# --- the generated world binaries ----------------------------------------------------------------
#
# `HOUSE-00475`. `deploy_world.py` and the five generators after it write into the REPOSITORY's
# `content/world/` -- the deployed layout, `collision.bin`, `nav.bin`, `chunks.bin` and the rest --
# and `TitleContainer` resolves against the BUILD tree's `content/`. Nothing had read one of those
# files at runtime before, so nobody had noticed that the two roots never met.
#
# Copied rather than generated here: producing `chunks.bin` needs the Blender shell, which is 99
# `.glb` and minutes of work, and a CMake build is not the place for it.
# `tools/ci/build_content.py --only world` produces them; this puts what exists where the game
# looks. Absent files are not an error -- a checkout that has never run the content build still
# builds and runs, and says what it could not load.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/content/world")
    add_custom_target(cnahouse_world_content ALL
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/content/world"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/content/world" "${CMAKE_BINARY_DIR}/content/world"
        COMMENT "Copying the deployed world data into the build tree's content root"
        VERBATIM)
    add_dependencies(cna-house cnahouse_world_content)
endif()

# --- Tier E's effect tree ------------------------------------------------------------------------
#
# Gated on `CNAHOUSE_TIER_E` **and** on an `fxc` actually being found, because the two are different
# questions: the build may want Tier E and the machine may not be able to produce it. A machine
# without Wine or without the DirectX SDK simply builds no effects, which ADR-0003 already allows.
#
# The launcher is `tools/effects/fxc-wine.sh`, not bare `wine`. `HOUSE-00087` measured why:
# `cna-content` builds the `fxc` command line with Unix absolute paths and `fxc` reads a leading
# `/` as an option, so the documented `--fx-compiler-launcher wine` fails with
# "Unknown or invalid option '/tmp/cna-fx-0-.../effect.fxb'".
set(CNAHOUSE_FXC "" CACHE FILEPATH "An fxc-compatible effect compiler for Tier E")
set(CNAHOUSE_FXC_LAUNCHER "${CMAKE_CURRENT_SOURCE_DIR}/tools/effects/fxc-wine.sh"
    CACHE FILEPATH "Program to run the effect compiler through; see HOUSE-00087")

file(GLOB_RECURSE CNAHOUSE_EFFECT_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/assets-src/Effects/*.fx")

# Whether the committed baseline `.xnb` can be used instead of compiling. `HOUSE-00185`/BL-04: the
# compiled effects are committed BESIDE their `.fx` precisely so a contributor with no Wine and no
# DirectX SDK can still build and run Tier E. `tools/effects/build_effects.sh` produces them and
# records the compiler identity in `assets-src/Effects/COMPILER.txt`.
#
# Compiling WINS when a compiler is available, and that is the right way round: a source edited
# without re-running the script would otherwise be silently ignored in favour of a stale committed
# file, which is the failure mode a baseline like this always eventually produces.
if(CNAHOUSE_TIER_E AND CNAHOUSE_EFFECT_SOURCES)
    set(_effect_outputs "")
    set(_effect_mode "")

    set(_can_compile OFF)
    if(CNAHOUSE_FXC AND EXISTS "${CNAHOUSE_FXC}")
        find_program(CNAHOUSE_CNA_CONTENT_TOOL
            NAMES cna-content
            HINTS "${CNAHOUSE_CNA_ROOT}/build" "${CMAKE_BINARY_DIR}/CNA_BUILD"
            NO_DEFAULT_PATH)
        if(CNAHOUSE_CNA_CONTENT_TOOL)
            set(_can_compile ON)
        endif()
    endif()

    foreach(_fx IN LISTS CNAHOUSE_EFFECT_SOURCES)
        cmake_path(GET _fx STEM _stem)
        cmake_path(REPLACE_EXTENSION _fx LAST_ONLY "xnb" OUTPUT_VARIABLE _committed)
        set(_out "${CMAKE_BINARY_DIR}/content-fx/Effects/${_stem}.xnb")

        if(_can_compile)
            add_custom_command(
                OUTPUT "${_out}"
                COMMAND "${CNAHOUSE_CNA_CONTENT_TOOL}" build "${_fx}" -o "${_out}"
                        --format xnb
                        --fx-compiler "${CNAHOUSE_FXC}"
                        --fx-compiler-launcher "${CNAHOUSE_FXC_LAUNCHER}"
                DEPENDS "${_fx}" "${CNAHOUSE_FXC_LAUNCHER}"
                COMMENT "Compiling effect ${_stem}.fx through fxc"
                VERBATIM)
            list(APPEND _effect_outputs "${_out}")
            set(_effect_mode "the effect compiler")
        elseif(EXISTS "${_committed}")
            # COPIED, not compiled. It depends on the committed `.xnb` and not on the `.fx`, because
            # this machine cannot turn one into the other -- making it depend on the source would
            # produce a target that re-copies forever without ever becoming up to date.
            add_custom_command(
                OUTPUT "${_out}"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_committed}" "${_out}"
                DEPENDS "${_committed}"
                COMMENT "Using the committed baseline ${_stem}.xnb (no effect compiler here)"
                VERBATIM)
            list(APPEND _effect_outputs "${_out}")
            set(_effect_mode "the committed baseline")
        endif()
    endforeach()

    if(_effect_outputs)
        add_custom_target(cnahouse_effects DEPENDS ${_effect_outputs})
        add_dependencies(cna-house cnahouse_effects)
        message(STATUS "cna-house: Tier E effects come from ${_effect_mode}")
    else()
        # Tier E was asked for, there are sources, and neither route is available. NOT an error:
        # ADR-0003 makes Tier S complete, and `ActivateTierE` falls back at load with one logged
        # line. Said out loud so it is not a surprise at run time.
        message(STATUS
            "cna-house: Tier E is on, but there is no effect compiler and no committed baseline "
            ".xnb -- no effects are built. Set -DCNAHOUSE_FXC=<fxc.exe>, or run "
            "tools/effects/build_effects.sh on a machine that has one and commit its output. "
            "Tier S is unaffected (ADR-0003).")
    endif()
endif()

# --- content (`HOUSE-00216`) ----------------------------------------------------------------------
#
# One target that runs the validators, then the generators, then `cna-content` -- in an order
# derived from a declared graph rather than written by hand. NOT part of `all`: the content build
# reads and writes `assets-src/` and `content/`, and a code build should not.
#
#     cmake --build build --target content
#     cmake --build build --target content-plan     # print the plan, run nothing
add_custom_target(content
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tools/ci/build_content.py"
            --stamps "${CMAKE_BINARY_DIR}/content-stamps.json"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "content: building the content tree"
    USES_TERMINAL
    VERBATIM)

add_custom_target(content-plan
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tools/ci/build_content.py"
            --stamps "${CMAKE_BINARY_DIR}/content-stamps.json" --dry-run
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "content-plan: what a content build would do"
    USES_TERMINAL
    VERBATIM)

# --- content-verify (`HOUSE-00199`) ---------------------------------------------------------------
#
# `cna-house.md` §18.4: rebuild everything and assert the hashes match; drift is a build failure.
# NOT part of `all`: it builds every content root twice and is a check someone asks for, not a cost
# every incremental build pays.
#
#     cmake --build build --target content-verify
add_custom_target(content-verify
    COMMAND "${CMAKE_COMMAND}" -E echo "Verifying content determinism..."
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tools/ci/content_verify.py"
            --work-dir "${CMAKE_BINARY_DIR}/content-verify"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "content-verify: rebuilding every content root twice and comparing"
    VERBATIM)

# --- the Web preload ----------------------------------------------------------------------------
#
# HOUSE-02850: the preload is the shipped content only -- no `notPackaged` inputs, no `dev` fixtures
# and the Web preset's LOD level of the chunk library -- and the build fails when it breaks §71.4's
# download limits (60 MB a pack, 180 MB in all, compressed). Defined last so that every content
# target it waits for already exists.
if(EMSCRIPTEN)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_custom_target(cnahouse_web_stage
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/ci/stage_content.py"
                --content "${CMAKE_BINARY_DIR}/content" --effects "${CMAKE_BINARY_DIR}/content-fx"
                --out "${CMAKE_BINARY_DIR}/web-stage" --lod-level 1
        COMMENT "Staging the Web preload"
        VERBATIM)
    # After the link, because the page, script and wasm are part of the download too.
    add_custom_command(TARGET cna-house POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/ci/web_budget.py"
                --content "${CMAKE_BINARY_DIR}/web-stage/content"
                --effects "${CMAKE_BINARY_DIR}/web-stage/content-fx"
                --program "$<TARGET_FILE:cna-house>"
                          "${CMAKE_BINARY_DIR}/cna-house.js" "${CMAKE_BINARY_DIR}/cna-house.wasm"
                --data "${CMAKE_BINARY_DIR}/cna-house.data"
        COMMENT "Checking the Web download against its budget"
        VERBATIM)
    foreach(_producer cnahouse_content cnahouse_world_content cnahouse_effects)
        if(TARGET ${_producer})
            add_dependencies(cnahouse_web_stage ${_producer})
        endif()
    endforeach()
    add_dependencies(cna-house cnahouse_web_stage)
endif()

# ------------------------------------------------------------------------------------------------
# Tests (`HOUSE-00125`)
# ------------------------------------------------------------------------------------------------
option(CNAHOUSE_BUILD_TESTS "Build the cna-house test targets" ON)
if(CNAHOUSE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "cna-house: renderer ${CNAHOUSE_RENDERER}, Tier E ${CNAHOUSE_TIER_E}, "
               "debug tools ${CNAHOUSE_DEBUG_TOOLS}, warnings-as-errors ${CNAHOUSE_WERROR}")
