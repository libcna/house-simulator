# `cnahouse_core` -- everything except `main`.
#
# The game and every test target link the SAME library, so a test exercises the code that ships
# rather than a recompiled variant of it. One source list, grouped by subsystem in the order
# `cna-house.md` §7.5 updates them, so a reader can find a file from the architecture document.
add_library(cnahouse_core STATIC
    # util -- no dependencies on anything else in the project
    util/Log.cpp
    util/Ids.cpp
    util/Rng.cpp
    util/Json.cpp

    # app -- the frame, the services, the settings
    app/CnaHouseGame.cpp
    app/ISystem.cpp
    app/Services.cpp
    app/EventQueue.cpp
    app/Settings.cpp
    app/CommandLine.cpp
    app/FrameTimer.cpp
    app/Platform.cpp
    audio/AudioSystem.cpp

    # player -- input first, because nothing else may read the devices
    player/KeyboardMouseSource.cpp

    # content
    content/ContentRegistry.cpp
    content/Caches.cpp

    # persistence
    persistence/DesktopSaveStore.cpp

    # rendering
    rendering/RenderTier.cpp
    rendering/Renderer.cpp
    rendering/MaterialBinder.cpp
    rendering/Quality.cpp
    rendering/RenderStates.cpp
    rendering/StateTracker.cpp

    # ui
    ui/TextRenderer.cpp

    # debug -- compiled always; the OVERLAY is what CNAHOUSE_DEBUG_TOOLS gates, not the counters,
    # because a counter that only exists in a debug build cannot be asserted by a perf test.
    debug/Counters.cpp
    debug/Timing.cpp
    debug/DebugDraw.cpp
    debug/Overlay.cpp
    debug/Screenshot.cpp
)

target_include_directories(cnahouse_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(cnahouse_core
    PUBLIC
        CNA
        # MEASURED (HOUSE-00076): CNA does not link Text.Json, so its include directories do not
        # reach us through the CNA target. Selecting the component is necessary but not sufficient.
        SharpRuntime::Text.Json
    PRIVATE
        cnahouse_warnings)

target_compile_definitions(cnahouse_core PUBLIC
    CNAHOUSE_VERSION="${PROJECT_VERSION}"
    CNAHOUSE_RENDERER_NAME="${CNAHOUSE_RENDERER}"
    CNAHOUSE_TIER_E=$<BOOL:${CNAHOUSE_TIER_E}>
    CNAHOUSE_DEBUG_TOOLS=$<BOOL:${CNAHOUSE_DEBUG_TOOLS}>)
