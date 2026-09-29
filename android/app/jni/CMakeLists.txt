cmake_minimum_required(VERSION 3.24)
project(cnahouse_android LANGUAGES C CXX)

# Gradle invokes Ninja directly, without forwarding --max-workers. Keep native
# compiler/linker concurrency bounded independently of Gradle's worker count.
set_property(GLOBAL PROPERTY JOB_POOLS compile_pool=4 link_pool=1)
set(CMAKE_JOB_POOL_COMPILE compile_pool)
set(CMAKE_JOB_POOL_LINK link_pool)

get_filename_component(HOUSE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

# Clang fuses a*b+c into one FMA on arm64 (-ffp-contract=on), which rounds once where every other
# House build (desktop gcc, x86_64, wasm) rounds twice. The movement and collision code then lands
# a capsule a few ULPs elsewhere, enough for the filming tour's recorded route to stall on the
# basement stair; with contraction off, arm64 simulates what the other builds simulate.
add_compile_options(-ffp-contract=off)

# The production House CMake target owns the source list and CNA configuration. Gradle
# builds only this shared Android entry point and its real cnahouse_core dependencies;
# host-only CNA tools and the desktop executable are excluded from this build graph.
set(CNAHOUSE_TIER_E OFF CACHE BOOL "" FORCE)
set(CNAHOUSE_DEBUG_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory("${HOUSE_ROOT}" house_build EXCLUDE_FROM_ALL)

# SDLActivity dlsym()s the game's Android entry point in libmain.so. Rename the
# existing main during compilation, with its C linkage declared in Main.cpp.
add_library(main SHARED "${HOUSE_ROOT}/src/app/Main.cpp")
target_compile_definitions(main PRIVATE main=SDL_main)
target_link_libraries(main PRIVATE cnahouse_core cnahouse_warnings)
