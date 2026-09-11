# The four test targets of `cna-house.md` §70.4, kept separate because they have different costs
# and different CI jobs:
#
#   unit         pure logic, no GraphicsDevice, milliseconds -- runs on every push
#   integration  a real Game under the HEADLESS renderer, no window -- runs on every push
#   render       render-target readback under OPENGLES3, needs a GPU -- nightly, under Xvfb
#   perf         timing, needs a quiet machine to mean anything -- nightly, never gating
#
# GoogleTest comes from the sibling CNA checkout's vendored copy rather than being fetched, for the
# same reason CNA and sharp-runtime are siblings: no network in the build, and one copy on disk.
include(GoogleTest)

set(_gtest_root "${CNAHOUSE_CNA_ROOT}/vendor/googletest")
if(NOT TARGET gtest_main AND EXISTS "${_gtest_root}/CMakeLists.txt")
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory("${_gtest_root}" GOOGLETEST EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET gtest_main)
    message(WARNING "cna-house: no GoogleTest at ${_gtest_root}; test targets are not built.")
    return()
endif()

# `HOUSE-00223`. The `.chanim` round-trip fixture, generated at BUILD time rather than committed:
# a checked-in binary produced by a tool in the same repository goes stale against that tool without
# anyone noticing, which is the whole reason no compiled content is committed either.
#
# `anim_extract.py` writes it and `ChanimRoundTripTests` reads it with the real C++ reader. Neither
# side can make that check alone: `ChanimReaderTests` builds its bytes by hand, so a writer that
# emitted transposed matrices or a `boneFirstKey` off by one would pass every other test here.
set(CNAHOUSE_ANIM_FIXTURE "${CMAKE_BINARY_DIR}/test-fixtures/anim/character.chanim")
find_program(CNAHOUSE_PYTHON3 NAMES python3 python)
if(CNAHOUSE_PYTHON3)
    set(_anim_source "${CMAKE_BINARY_DIR}/test-fixtures/anim/character.glb")
    add_custom_command(
        OUTPUT "${CNAHOUSE_ANIM_FIXTURE}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/test-fixtures/anim"
        COMMAND "${CNAHOUSE_PYTHON3}" "${CMAKE_SOURCE_DIR}/tools/assets/anim_extract.py"
                --make-fixture "${CMAKE_BINARY_DIR}/test-fixtures/anim"
        COMMAND "${CNAHOUSE_PYTHON3}" "${CMAKE_SOURCE_DIR}/tools/assets/anim_extract.py"
                "${_anim_source}" -o "${CNAHOUSE_ANIM_FIXTURE}"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/assets/anim_extract.py"
                "${CMAKE_SOURCE_DIR}/tools/assets/measure_stride.py"
                "${CMAKE_SOURCE_DIR}/tools/assets/gltf_io.py"
        COMMENT "Generating the .chanim round-trip fixture with anim_extract.py"
        VERBATIM)
    add_custom_target(cnahouse_anim_fixture DEPENDS "${CNAHOUSE_ANIM_FIXTURE}")
else()
    message(WARNING "cna-house: no python3, so the .chanim round-trip fixture is not generated "
                    "and ChanimRoundTripTests will not build.")
endif()

# `HOUSE-00474`. The same idiom for `chunks.bin`, for the same reason: `ChunkReaderTests` builds
# its bytes by hand and therefore cannot see the writer and the reader disagreeing about field
# order. `build_chunks.py --fixture` writes a file whose every number is stated in the tool, and
# `ChunkRoundTripTests` asserts those numbers with the real C++ reader.
set(CNAHOUSE_CHUNK_FIXTURE "${CMAKE_BINARY_DIR}/test-fixtures/chunks/fixture.bin")
if(CNAHOUSE_PYTHON3)
    add_custom_command(
        OUTPUT "${CNAHOUSE_CHUNK_FIXTURE}"
        COMMAND "${CNAHOUSE_PYTHON3}" "${CMAKE_SOURCE_DIR}/tools/world/build_chunks.py"
                --fixture "${CNAHOUSE_CHUNK_FIXTURE}"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/world/build_chunks.py"
                "${CMAKE_SOURCE_DIR}/tools/world/build_collision.py"
        COMMENT "Generating the chunks.bin round-trip fixture with build_chunks.py"
        VERBATIM)
    add_custom_target(cnahouse_chunk_fixture DEPENDS "${CNAHOUSE_CHUNK_FIXTURE}")
else()
    message(WARNING "cna-house: no python3, so the chunks.bin round-trip fixture is not generated "
                    "and ChunkRoundTripTests will not build.")
endif()

# `HOUSE-00856`. The same idiom again for `neighbourhood.bin`, whose vertices are in the ASSET's
# own space rather than the world's -- a writer and a reader that disagreed about that would still
# both look right on their own hand-written bytes.
set(CNAHOUSE_NEIGHBOURHOOD_FIXTURE "${CMAKE_BINARY_DIR}/test-fixtures/neighbourhood/fixture.bin")
if(CNAHOUSE_PYTHON3)
    add_custom_command(
        OUTPUT "${CNAHOUSE_NEIGHBOURHOOD_FIXTURE}"
        COMMAND "${CNAHOUSE_PYTHON3}" "${CMAKE_SOURCE_DIR}/tools/world/build_neighbourhood.py"
                --fixture "${CNAHOUSE_NEIGHBOURHOOD_FIXTURE}"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/world/build_neighbourhood.py"
                "${CMAKE_SOURCE_DIR}/tools/world/build_chunks.py"
                "${CMAKE_SOURCE_DIR}/tools/world/build_collision.py"
                "${CMAKE_SOURCE_DIR}/tools/world/neighbourhood_gen.py"
        COMMENT "Generating the neighbourhood.bin round-trip fixture with build_neighbourhood.py"
        VERBATIM)
    add_custom_target(cnahouse_neighbourhood_fixture DEPENDS "${CNAHOUSE_NEIGHBOURHOOD_FIXTURE}")
else()
    message(WARNING "cna-house: no python3, so the neighbourhood.bin round-trip fixture is not "
                    "generated and NeighbourhoodRoundTripTests will not build.")
endif()

# `HOUSE-00541`. The same idiom again for `collision.bin`, and `HOUSE-00225` is why there are three
# of them: a reader and a writer each tested against their own hand-written fixtures both passed
# while producing and expecting different bytes.
set(CNAHOUSE_COLLISION_FIXTURE "${CMAKE_BINARY_DIR}/test-fixtures/collision/fixture.bin")
if(CNAHOUSE_PYTHON3)
    add_custom_command(
        OUTPUT "${CNAHOUSE_COLLISION_FIXTURE}"
        COMMAND "${CNAHOUSE_PYTHON3}" "${CMAKE_SOURCE_DIR}/tools/world/build_collision.py"
                --fixture "${CNAHOUSE_COLLISION_FIXTURE}"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/world/build_collision.py"
        COMMENT "Generating the collision.bin round-trip fixture with build_collision.py"
        VERBATIM)
    add_custom_target(cnahouse_collision_fixture DEPENDS "${CNAHOUSE_COLLISION_FIXTURE}")
else()
    message(WARNING "cna-house: no python3, so the collision.bin round-trip fixture is not "
                    "generated and CollisionRoundTripTests will not build.")
endif()

function(cnahouse_test_target kind)
    set(_target "cnahouse_${kind}_tests")
    file(GLOB_RECURSE _sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${kind}/*.cpp")
    if(NOT _sources)
        return()
    endif()
    add_executable(${_target} ${_sources})
    target_link_libraries(${_target} PRIVATE cnahouse_core cnahouse_warnings gtest_main)
    target_include_directories(${_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    # Tests run from the repository root so their FIXTURE paths are stable, but the content the
    # build produced is in the build tree. The two roots are different questions and are answered
    # separately rather than by moving one of them.
    # A third root, for what a test WRITES. Render fixtures capture PNGs, and they belong in the
    # build tree rather than beside the sources they are compared against -- a `git status` that
    # lists a captured frame every time the suite runs teaches people to ignore `git status`.
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test-output")
    # The EFFECT root is separate for the same reason the game keeps it separate (`HOUSE-00064`):
    # a stale `.xnb` in the `.cnb` tree shadows what the build just produced. A test that wants
    # Tier E has to be told where the `.xnb` tree is (`HOUSE-00201`).
    target_compile_definitions(${_target} PRIVATE
        CNAHOUSE_TEST_CONTENT_ROOT="${CMAKE_BINARY_DIR}/content"
        CNAHOUSE_TEST_EFFECT_ROOT="${CMAKE_BINARY_DIR}/content-fx"
        CNAHOUSE_TEST_OUTPUT_DIR="${CMAKE_BINARY_DIR}/test-output"
        CNAHOUSE_TEST_ANIM_FIXTURE="${CNAHOUSE_ANIM_FIXTURE}"
        CNAHOUSE_TEST_CHUNK_FIXTURE="${CNAHOUSE_CHUNK_FIXTURE}"
        CNAHOUSE_TEST_COLLISION_FIXTURE="${CNAHOUSE_COLLISION_FIXTURE}"
        CNAHOUSE_TEST_NEIGHBOURHOOD_FIXTURE="${CNAHOUSE_NEIGHBOURHOOD_FIXTURE}")
    if(TARGET cnahouse_anim_fixture)
        add_dependencies(${_target} cnahouse_anim_fixture)
    endif()
    if(TARGET cnahouse_chunk_fixture)
        add_dependencies(${_target} cnahouse_chunk_fixture)
    endif()
    if(TARGET cnahouse_collision_fixture)
        add_dependencies(${_target} cnahouse_collision_fixture)
    endif()
    if(TARGET cnahouse_neighbourhood_fixture)
        add_dependencies(${_target} cnahouse_neighbourhood_fixture)
    endif()
    set_target_properties(${_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    # The working directory is the REPOSITORY ROOT, not the build directory, so a test's fixture
    # paths are stable wherever the build tree lives. This is not a detail: running CNA's own suites
    # from the wrong directory made 64 tests "fail" during HOUSE-00118 purely on unresolved fixture
    # paths, and the same trap is one line away here.
    # `HOUSE-00495`: a test that DRAWS reads the game's own content root -- the copy under the
    # build tree -- and `cnahouse_world_content` refreshes that copy at BUILD time. A content
    # rebuild that is not followed by a C++ build therefore leaves every render and integration
    # test looking at the previous world, and passing against it: the render suite reported thirty
    # green frames over a `chunks.bin` that predated the outdoors being chunked at all. The fixture
    # below re-runs the copy before they do.
    set(_properties LABELS "${kind}")
    if(kind STREQUAL "render" OR kind STREQUAL "integration")
        list(APPEND _properties FIXTURES_REQUIRED world_content)
    endif()
    # `DISCOVERY_MODE PRE_TEST` means the test list is regenerated by running the binary, and
    # until that has happened in a given build tree `ctest -N` reports whatever the LAST build left
    # behind. Two `ctest -N -L unit` calls minutes apart legitimately answered 142 and 1141 on
    # 2026-09-11, and a session that had inherited a stale tree read the difference as a thousand
    # tests disappearing. **The authoritative count is the binary's own**:
    #
    #     ./build/cnahouse_unit_tests --gtest_list_tests | grep -c '^  '
    #
    # Check that before believing a change in `ctest`'s totals.
    gtest_discover_tests(${_target}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        PROPERTIES ${_properties}
        DISCOVERY_MODE PRE_TEST)
endfunction()

if(TARGET cnahouse_world_content)
    add_test(NAME world-content-current
             COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
                     --target cnahouse_world_content)
    set_tests_properties(world-content-current PROPERTIES FIXTURES_SETUP world_content)
endif()

foreach(_kind unit integration render perf)
    cnahouse_test_target(${_kind})
endforeach()
