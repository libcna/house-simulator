# TierSelection.cmake -- where the Tier-E decision is made, and the ONLY place it is made.
#
# `HOUSE-00122`. ADR-0003 splits rendering into Tier S (stock XNA effects, always present) and
# Tier E (compiled `.fx`, optional). Which tier a binary has is a **build fact**, decided here from
# the CNA configuration this build has just set, and baked into the binary. No runtime code queries
# the device, because `GraphicsDevice::SupportsCapability` is forbidden by ADR-0001 and because a
# capability query answers a different question than "was this shader compiled into my content".
#
# The rule is asymmetric on purpose: a user may force Tier E OFF, never ON. Forcing it on where the
# renderer cannot load a compiled effect would produce a binary that fails at content load, and the
# whole point of deciding at configure time is that it cannot.

include_guard(GLOBAL)

option(CNAHOUSE_TIER_E "Build the Tier E renderer path (compiled .fx effects)" ON)
# `cna-house.md` §69: ON for `Debug` and `RelWithDebInfo`, OFF for `Release`. Derived from the build
# type rather than defaulted to ON, because "ON everywhere" is what a shipped build with an F1
# overlay in it looks like -- and `HOUSE-00165` found exactly that: a Release binary printing
# "debug on". A user may still force either way; only the DEFAULT follows the build type.
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set(_debug_tools_default OFF)
else()
    set(_debug_tools_default ON)
endif()
option(CNAHOUSE_DEBUG_TOOLS "Build the debug overlay, DebugDraw and the developer commands"
       ${_debug_tools_default})

# Which CNA option gates compiled effects depends on the renderer family. Only the families
# `cna-house` targets are listed; an unlisted one is refused rather than guessed at, because
# guessing produces a build that compiles and then fails at content load.
set(_tier_e_supported OFF)
set(_tier_e_reason "")
if(CNAHOUSE_RENDERER STREQUAL "OPENGLES3" OR CNAHOUSE_RENDERER STREQUAL "OPENGL33"
        OR CNAHOUSE_RENDERER STREQUAL "WEBGL2")
    if(CNA_EASYGL_COMPILED_EFFECTS)
        set(_tier_e_supported ON)
    else()
        set(_tier_e_reason "CNA_EASYGL_COMPILED_EFFECTS is OFF")
    endif()
elseif(CNAHOUSE_RENDERER STREQUAL "HEADLESS")
    set(_tier_e_reason "the HEADLESS renderer rasterises nothing, so no effect can be exercised")
else()
    set(_tier_e_reason "renderer '${CNAHOUSE_RENDERER}' has no known compiled-effect option")
endif()

if(CNAHOUSE_TIER_E AND NOT _tier_e_supported)
    message(STATUS "cna-house: Tier E requested but unavailable -- ${_tier_e_reason}. "
                   "Building Tier S only, which is complete by design (ADR-0003).")
    set(CNAHOUSE_TIER_E OFF CACHE BOOL "" FORCE)
endif()

if(CNAHOUSE_TIER_E)
    message(STATUS "cna-house: Tier E ON")
else()
    message(STATUS "cna-house: Tier E OFF -- Tier S is the whole renderer")
endif()
