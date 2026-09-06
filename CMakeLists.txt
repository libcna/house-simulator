# cna-house -- root build.
#
# `cna-house` consumes `../cnanext` and `../sharp-runtimenext` as SIBLING SOURCE CHECKOUTS by
# `add_subdirectory` (`cna-house.md` §8.1). No vendoring, no submodules, no fetch: the sibling is
# built as part of this build, from the source the developer has, so a CNA change is visible here
# immediately and a CNA version is never silently pinned.
#
# Build directories are the openeggbert closed list -- `build/`, `build-asan/`, `build-ubsan/`,
# `build-tsan/`, `build-probe/`, `build-consumer/` -- in this repository, never in `/tmp` or a
# session scratchpad. `CMakePresets.json` encodes them; see `HOUSE-00035`.
cmake_minimum_required(VERSION 3.24)

project(cna_house
        VERSION 0.1.0
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
set(CNAHOUSE_CNA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../cnanext"
    CACHE PATH "The CNA checkout this build consumes")
set(CNAHOUSE_SHARP_RUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../sharp-runtimenext"
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

set(CNAHOUSE_CNB_ASSET_DIRS Models Textures Audio Fonts Video world)

if(COMMAND cna_add_content)
    set(_content_targets "")
    foreach(_dir IN LISTS CNAHOUSE_CNB_ASSET_DIRS)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets-src/${_dir}")
            continue()
        endif()
        file(GLOB_RECURSE _sources "${CMAKE_CURRENT_SOURCE_DIR}/assets-src/${_dir}/*")
        list(FILTER _sources EXCLUDE REGEX "/\\.gitkeep$")
        if(NOT _sources)
            continue()   # an empty subdirectory is not an error; most of them start empty
        endif()
        cna_add_content(
            TARGET cnahouse_content_${_dir}
            SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets-src/${_dir}"
            OUTPUT_DIR "${CMAKE_BINARY_DIR}/content/${_dir}")
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

if(CNAHOUSE_TIER_E AND CNAHOUSE_EFFECT_SOURCES)
    if(NOT CNAHOUSE_FXC OR NOT EXISTS "${CNAHOUSE_FXC}")
        message(STATUS
            "cna-house: Tier E is on but no effect compiler is configured, so no .fx is built. "
            "Set -DCNAHOUSE_FXC=<path to fxc.exe>; Tier S is unaffected (ADR-0003).")
    else()
        find_program(CNAHOUSE_CNA_CONTENT_TOOL
            NAMES cna-content
            HINTS "${CNAHOUSE_CNA_ROOT}/build" "${CMAKE_BINARY_DIR}/CNA_BUILD"
            NO_DEFAULT_PATH)
        if(NOT CNAHOUSE_CNA_CONTENT_TOOL)
            message(STATUS "cna-house: no cna-content tool found; no .fx is built.")
        else()
            set(_effect_outputs "")
            foreach(_fx IN LISTS CNAHOUSE_EFFECT_SOURCES)
                cmake_path(GET _fx STEM _stem)
                set(_out "${CMAKE_BINARY_DIR}/content-fx/Effects/${_stem}.xnb")
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
            endforeach()
            add_custom_target(cnahouse_effects DEPENDS ${_effect_outputs})
            add_dependencies(cna-house cnahouse_effects)
        endif()
    endif()
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
