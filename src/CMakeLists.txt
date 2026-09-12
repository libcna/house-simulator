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
    animation/Animation.cpp
    animation/AnimationCache.cpp
    animation/ChanimReader.cpp
    app/CnaHouseGame.cpp
    app/ISystem.cpp
    app/Services.cpp
    app/EventQueue.cpp
    app/Settings.cpp
    app/CommandLine.cpp
    app/FrameTimer.cpp
    app/Platform.cpp
    audio/AudioSystem.cpp

    # environment -- §35's clock, which everything time-dependent reads and nothing duplicates
    # lighting -- §28's per-room state, the one answer everything downstream reads
    lighting/BorrowedLightModel.cpp
    lighting/LightingSystem.cpp
    lighting/PlanckianLut.cpp
    lighting/ShadingGrid.cpp
    lighting/DaylightModel.cpp
    lighting/RoomLightState.cpp

    environment/DayLength.cpp
    environment/Season.cpp
    environment/Temperature.cpp
    environment/SimClock.cpp
    environment/SunModel.cpp
    environment/SunLight.cpp

    # weather -- §36's continuous state vector; the state machine follows in phase 26
    weather/PrecipitationTransition.cpp
    weather/SnowAccumulation.cpp
    weather/SurfaceWetness.cpp
    weather/WeatherBlend.cpp
    weather/WeatherRateLimiter.cpp
    weather/WeatherSampler.cpp
    weather/WeatherState.cpp

    # player -- input first, because nothing else may read the devices
    player/BoundaryGuard.cpp
    player/CellTracker.cpp
    player/EyeProbe.cpp
    player/EyeSpring.cpp
    player/FirstPersonCamera.cpp
    player/FirstPersonView.cpp
    player/HeadBob.cpp
    player/LandingDip.cpp
    player/MouseCapture.cpp
    player/MouseLook.cpp
    player/KeyboardMouseSource.cpp
    player/PlayerController.cpp

    # content
    content/ContentRegistry.cpp
    content/Caches.cpp
    content/SmokeScene.cpp

    # world -- the data model the house is (`cna-house.md` §15). Above rendering because every
    # system below reads it and none of them writes it.
    world/WorldTypes.cpp
    world/WorldData.cpp
    physics/BroadPhase.cpp
    physics/CollisionLoader.cpp
    physics/DynamicObstacles.cpp
    physics/Fall.cpp
    physics/Ground.cpp
    physics/Move.cpp
    physics/Nudgeable.cpp
    physics/RayCast.cpp
    physics/Sweep.cpp
    physics/Terrain.cpp
    world/CellRuntime.cpp
    world/ChunkReader.cpp
    world/NeighbourhoodReader.cpp
    world/WorldLoader.cpp
    world/WorldValidator.cpp
    world/InteractableExpr.cpp
    world/SpatialIndex.cpp

    # visibility -- §25's portal walk. Reads the world, writes nothing in it.
    visibility/ChunkCulling.cpp
    visibility/ClipFrustum.cpp
    visibility/CullDistance.cpp
    visibility/LodSelection.cpp
    visibility/DetailSets.cpp
    visibility/ExteriorBvh.cpp
    visibility/ExteriorCulling.cpp
    visibility/ExteriorScene.cpp
    visibility/ClipRect.cpp
    visibility/InstanceCulling.cpp
    visibility/PortalArea.cpp
    visibility/PortalTraversal.cpp
    visibility/VisibilitySystem.cpp
    visibility/ReduceFrustum.cpp
    visibility/RenderList.cpp
    visibility/PortalRuntime.cpp
    visibility/PortalFacing.cpp
    visibility/PortalDepth.cpp

    # persistence
    persistence/DesktopSaveStore.cpp

    # rendering
    rendering/RenderTier.cpp
    rendering/Renderer.cpp
    rendering/Camera.cpp
    rendering/MaterialBinder.cpp
    rendering/Quality.cpp
    rendering/RenderStates.cpp
    rendering/StaticGeometryPass.cpp
    rendering/StateTracker.cpp
    rendering/SunDiscPass.cpp
    rendering/WindowGlow.cpp

    # ui
    ui/EnvironmentReadout.cpp
    ui/LoadingScreen.cpp
    ui/MenuStack.cpp
    ui/TextRenderer.cpp

    # debug -- compiled always; the OVERLAY is what CNAHOUSE_DEBUG_TOOLS gates, not the counters,
    # because a counter that only exists in a debug build cannot be asserted by a perf test.
    debug/Console.cpp
    debug/Counters.cpp
    debug/PlayerCommands.cpp
    debug/EnvironmentOverlay.cpp
    debug/TimeCommands.cpp
    debug/VisibilityCommands.cpp
    debug/FreeFlyCamera.cpp
    debug/Timing.cpp
    debug/DebugDraw.cpp
    debug/Overlay.cpp
    debug/VisibilityGeometryOverlay.cpp
    debug/VisibilityOverlay.cpp
    debug/WorldOverlay.cpp
    debug/PhysicsOverlay.cpp
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
