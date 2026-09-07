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
        CNAHOUSE_TEST_OUTPUT_DIR="${CMAKE_BINARY_DIR}/test-output")
    set_target_properties(${_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    # The working directory is the REPOSITORY ROOT, not the build directory, so a test's fixture
    # paths are stable wherever the build tree lives. This is not a detail: running CNA's own suites
    # from the wrong directory made 64 tests "fail" during HOUSE-00118 purely on unresolved fixture
    # paths, and the same trap is one line away here.
    gtest_discover_tests(${_target}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        PROPERTIES LABELS "${kind}"
        DISCOVERY_MODE PRE_TEST)
endfunction()

foreach(_kind unit integration render perf)
    cnahouse_test_target(${_kind})
endforeach()
