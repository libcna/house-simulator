// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"

#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/FrameTimer.hpp"
#include "cnahouse/app/Platform.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/audio/AudioSystem.hpp"
#include "cnahouse/content/SmokeScene.hpp"
#include "cnahouse/debug/Console.hpp"
#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/debug/DebugDraw.hpp"
#include "cnahouse/debug/EnvironmentOverlay.hpp"
#include "cnahouse/debug/FreeFlyCamera.hpp"
#include "cnahouse/debug/Overlay.hpp"
#include "cnahouse/debug/PhysicsOverlay.hpp"
#include "cnahouse/debug/Timing.hpp"
#include "cnahouse/debug/VisibilityGeometryOverlay.hpp"
#include "cnahouse/debug/VisibilityOverlay.hpp"
#include "cnahouse/debug/WorldOverlay.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/lighting/LightingSystem.hpp"
#include "cnahouse/lighting/ShadingGrid.hpp"
#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionData.hpp"
#include "cnahouse/player/CellTracker.hpp"
#include "cnahouse/player/FirstPersonView.hpp"
#include "cnahouse/player/FixedStep.hpp"
#include "cnahouse/player/KeyboardMouseSource.hpp"
#include "cnahouse/player/MouseCapture.hpp"
#include "cnahouse/player/MouseLook.hpp"
#include "cnahouse/player/TouchSource.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/Quality.hpp"
#include "cnahouse/rendering/RenderTier.hpp"
#include "cnahouse/rendering/Renderer.hpp"
#include "cnahouse/rendering/StateTracker.hpp"
#include "cnahouse/ui/EnvironmentReadout.hpp"
#include "cnahouse/ui/LoadingScreen.hpp"
#include "cnahouse/ui/MenuStack.hpp"
#include "cnahouse/ui/TextRenderer.hpp"
#include "cnahouse/visibility/ChunkCulling.hpp"
#include "cnahouse/visibility/ExteriorCulling.hpp"
#include "cnahouse/visibility/ExteriorScene.hpp"
#include "cnahouse/visibility/RenderList.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/weather/CoverageMask.hpp"
#include "cnahouse/weather/RainParticles.hpp"
#include "cnahouse/weather/WeatherSystem.hpp"
#include "cnahouse/world/CellRuntime.hpp"
#include "cnahouse/world/ChunkData.hpp"
#include "cnahouse/world/SpatialIndex.hpp"
#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::rendering
{
    struct FogParams;
    class MaterialBinder;
    class ParticleRenderer;
    class SkySystem;
} // namespace cnahouse::rendering

namespace cnahouse::content
{
    struct Caches;
}

namespace cnahouse::app
{

    /// @brief The application. One `Game` subclass, and the only place XNA's lifetime is touched.
    ///
    /// XNA-only throughout (ADR-0001): `Game`, `GraphicsDeviceManager`, `GraphicsDevice`,
    /// `ContentManager`, `SpriteBatch`. No `CNA::` anything, no capability query, no native handle.
    ///
    /// **`Run()` is the only lifetime CNA offers on desktop** (measured, `HOUSE-00062`), so the whole
    /// application lives inside it. `Update` and `Draw` are the two per-frame entry points and every
    /// system is driven from them in the fixed order of `cna-house.md` §7.5.
    class CnaHouseGame final : public Microsoft::Xna::Framework::Game
    {
    public:
        explicit CnaHouseGame(Options options, Settings settings);
        ~CnaHouseGame() override;

        CnaHouseGame(const CnaHouseGame&) = delete;
        CnaHouseGame& operator=(const CnaHouseGame&) = delete;

        /// @brief Non-zero if the session ended in a way the shell should report.
        [[nodiscard]] int ExitCode() const noexcept
        {
            return exitCode_;
        }

        /// @brief Frames drawn. Used by the headless integration tests.
        [[nodiscard]] std::uint64_t FramesDrawn() const noexcept
        {
            return framesDrawn_;
        }

        /// @brief The graphics device's applied display mode, for an end-to-end input test.
        [[nodiscard]] bool FullscreenForTesting() const;

        /// @brief Stops after this many frames. 0 means run until the user exits.
        void SetFrameLimit(std::uint64_t frames) noexcept
        {
            frameLimit_ = frames;
        }

        /// @brief Render into an offscreen target and force one-texel completion each frame.
        ///
        /// Performance tests opt into this before `Run`. Normal sessions never allocate the target
        /// or pay the readback. The paired vectors separate CPU command submission from elapsed time
        /// through GPU completion, following the phase-1 probe's measured one-texel fence.
        void EnableGpuCompletionSamplingForTesting() noexcept
        {
            gpuCompletionSampling_ = true;
        }

        [[nodiscard]] const std::vector<float>& RenderSubmitTimesForTesting() const noexcept
        {
            return renderSubmitTimes_;
        }

        [[nodiscard]] const std::vector<float>& GpuCompletionTimesForTesting() const noexcept
        {
            return gpuCompletionTimes_;
        }

        /// @brief Stops after this many of §49.3's fixed steps. 0 means no limit.
        ///
        /// **The limit a simulation test wants.** A frame limit measures the MACHINE: the fixed
        /// step is fed by real frame time, so the same 250 frames simulate a quarter of a second
        /// on a fast build and two seconds on a slow one -- and a test that asserts how far a body
        /// walked is then asserting how quickly the frames went by. `HOUSE-00684` made the frames
        /// four times cheaper and moved every such test at once, which is what made this seam
        /// necessary rather than merely tidy. Counted in steps, a walk is the same walk everywhere.
        ///
        /// Both limits apply; whichever is reached first stops the loop, so a frame limit is still
        /// the backstop for a session whose steps never run at all.
        void SetFixedStepLimit(std::uint64_t steps) noexcept
        {
            fixedStepLimit_ = steps;
        }

        /// @brief Drives the game from a scripted input source instead of the devices.
        ///
        /// The one seam the player wiring needs to be testable without a window: everything after
        /// `IInputSource` is expressed in game terms (`HOUSE-00140`), so a test that can supply one
        /// can walk the body through the house and assert where it ended up. Null restores the
        /// keyboard and mouse. The pointer is not owned; it must outlive the `Run()` that uses it.
        void SetInputSourceForTesting(player::IInputSource* source) noexcept
        {
            scriptedInput_ = source;
        }

        /// @brief Where the body is standing, for `HOUSE-00633`'s integration test.
        ///
        /// Readable after `Run()` because the alternative is asserting on the log line, and a walk
        /// that ended somewhere unexpected is a fact about the SIMULATION rather than about what
        /// it printed.
        [[nodiscard]] const player::PlayerState& PlayerForTesting() const noexcept
        {
            return player_;
        }

        /// @brief §25.1's step 5 for the last frame drawn (`HOUSE-00676`).
        ///
        /// Readable after `Run()` because what the passes were asked to draw is a fact about the
        /// FRAME, and the only alternative is to infer it from the picture -- which cannot tell a
        /// chunk that was culled from one that was drawn behind another.
        [[nodiscard]] const visibility::RenderList& RenderListForTesting() const noexcept
        {
            return renderList_;
        }

        /// @brief §71's counters, so an integration test can read what a frame actually submitted.
        [[nodiscard]] const debug::Counters& CountersForTesting() const noexcept
        {
            return counters_;
        }

        [[nodiscard]] const debug::Timing& TimingForTesting() const noexcept
        {
            return timing_;
        }

        /// @brief §71's console, so a test can type a command the way a person would.
        [[nodiscard]] debug::Console& ConsoleForTesting() noexcept
        {
            return console_;
        }

        /// @brief §25.8's `F5`: whether the walk is frozen and the camera detached.
        [[nodiscard]] bool VisibilityFrozenForTesting() const noexcept
        {
            return visibilityFrozen_;
        }

        /// @brief Where the frame is drawn from, which is the BODY's eye until `F5` detaches it.
        [[nodiscard]] const rendering::Camera& DrawCameraForTesting() const noexcept
        {
            return blockoutCamera_;
        }

        /// @brief The view matrix §25.8's `F4` and §71's `F9` are drawn through.
        ///
        /// Exposed because the alternative is a render fixture with an overlay up: the annotations
        /// are world-space, so drawing them through the wrong camera puts them somewhere they are
        /// not, and after `F5` the wrong camera is the one the body is still holding.
        [[nodiscard]] Microsoft::Xna::Framework::Matrix DebugViewForTesting() const
        {
            return DebugView();
        }

        /// @brief §25.8's `F4`, for the test that presses the key.
        [[nodiscard]] const debug::VisibilityGeometryOverlay& VisibilityGeometryForTesting() const noexcept
        {
            return visibilityGeometry_;
        }

        /// @brief §25.8's `F3`, for the test that presses the key.
        [[nodiscard]] const debug::VisibilityOverlay& VisibilityOverlayForTesting() const noexcept
        {
            return visibilityOverlay_;
        }

        /// @brief What `F3` would show about the last frame.
        [[nodiscard]] debug::VisibilitySnapshot VisibilitySnapshotForTesting() const
        {
            return VisibilitySnapshot();
        }

        /// @brief §71's `F9`, for the test that presses the key.
        [[nodiscard]] const debug::PhysicsOverlay& PhysicsOverlayForTesting() const noexcept
        {
            return physicsOverlay_;
        }

        /// @brief §49.3's fixed steps run so far. The simulated clock, in 1/120 s units.
        [[nodiscard]] std::uint64_t FixedStepsForTesting() const noexcept
        {
            return fixedSteps_;
        }

        /// @brief §44's camera, after the last frame. Also `HOUSE-00633`'s.
        [[nodiscard]] const player::FirstPersonView& ViewForTesting() const noexcept
        {
            return view_;
        }

        /// @brief The cell the body was last found in, or an invalid id if it is not walking.
        [[nodiscard]] util::Id CellForTesting() const noexcept
        {
            return tracker_.Current();
        }

        /// @brief How many §17.4 chunks are on the GPU: what `cull off` draws, and the ceiling
        ///        every culled frame is measured against.
        ///
        /// Read rather than written down, because it is a CONTENT number: the shell is generated,
        /// and a test that spelled it out would have to be edited by whoever next changes a wall
        /// -- which teaches people to edit the number rather than read the failure.
        [[nodiscard]] std::size_t ResidentChunksForTesting() const noexcept
        {
            return blockoutCells_ == nullptr ? 0u : blockoutCells_->ResidentChunkIndices().size();
        }

        /// @brief §71's `F8`, for the test that presses the key.
        [[nodiscard]] const debug::EnvironmentOverlay& EnvironmentOverlayForTesting() const noexcept
        {
            return environmentOverlay_;
        }

        /// @brief §35's clock, as it stands after the frames this session has run.
        ///
        /// The one thing a test cannot get at any other way: the clock is advanced inside
        /// `Update` and nothing else reads it yet. What this is for is the claim that it is WIRED
        /// -- that a session which ran frames has a clock that moved, at the rate the settings
        /// file asked for.
        [[nodiscard]] const environment::SimClock& ClockForTesting() const noexcept
        {
            return clock_;
        }

        /// @brief §28's per-cell lighting after a walk session, or null outside that scene.
        [[nodiscard]] const lighting::LightingSystem* LightingForTesting() const noexcept
        {
            return lighting_.has_value() ? &*lighting_ : nullptr;
        }

        [[nodiscard]] const weather::WeatherSystem* WeatherForTesting() const noexcept
        {
            return weather_.has_value() ? &*weather_ : nullptr;
        }

        /// @brief §71's current celestial diagnostic snapshot, absent outside a lit world scene.
        [[nodiscard]] std::optional<debug::CelestialOverlayState> CelestialSnapshot() const noexcept;

        /// @brief The version line drawn in the corner and printed at startup.
        [[nodiscard]] static std::string VersionLine();

        /// @brief The version line plus what this SESSION is actually running.
        ///
        /// `VersionLine()` names what the build contains -- it is static because that is a build
        /// fact and the tests ask for it without a `Game`. This one names the build fact *and* the
        /// active tier, and the two differ whenever `--tier=s` or a failed Tier-E load narrowed it.
        /// The drawn corner line and the startup banner use this one, because a screenshot that
        /// reported only the build fact would misattribute the frame it is a screenshot of.
        [[nodiscard]] std::string SessionLine() const;

        /// @brief The tier this session settled on, readable after `Run()` has returned.
        ///
        /// Read by the Tier-fallback integration test, which is the only way to check the ADR-0003
        /// narrowing end to end: the decision is made inside `LoadContent`, so nothing outside a
        /// frame can observe it otherwise.
        [[nodiscard]] const rendering::RenderTier& Tier() const noexcept
        {
            return tier_;
        }

        /// @brief The resolved §68 Graphics settings this session is running. Valid after
        ///        `LoadContent`.
        [[nodiscard]] const rendering::QualitySettings& Quality() const noexcept
        {
            return quality_;
        }

        /// @brief The audio device and mix. Readable after `Run()` for the integration tests.
        [[nodiscard]] const audio::AudioSystem& Audio() const noexcept
        {
            return audio_;
        }

        /// @brief Every frame's CPU delta in milliseconds, oldest first.
        ///
        /// **Recorded only while a frame limit is set**, which is exactly the benchmark and test
        /// case. An unbounded play session would grow this vector forever for a number nobody reads,
        /// and a perf harness that had to sample from outside could not see individual frames at all.
        [[nodiscard]] const std::vector<float>& FrameTimes() const noexcept
        {
            return frameTimes_;
        }

        /// @brief The screen stack. Readable after `Run()` for the integration tests.
        [[nodiscard]] const ui::MenuStack& Menus() const noexcept
        {
            return menus_;
        }

        /// @brief What `--scene=content-smoke` established, or `nullptr` in any other session.
        ///
        /// Readable after `Run()` for the same reason `Tier()` is: the six loads happen inside
        /// `LoadContent` and the video's advance inside `Update`, so nothing outside a frame can
        /// observe either otherwise (`HOUSE-00201`).
        [[nodiscard]] const content::SmokeReport* SmokeReport() const noexcept
        {
            return smoke_ == nullptr ? nullptr : &smoke_->Report();
        }

        /// @brief The frame-time line: milliseconds and the frames-per-second it implies.
        ///
        /// Both, deliberately. Milliseconds is the number a budget is written in and the one that
        /// adds up across systems; frames per second is the number a person feels. Showing only fps
        /// hides that 60 → 50 is a bigger regression than 30 → 28.
        [[nodiscard]] static std::string FrameTimeLine(float deltaSeconds);

        /// @brief What this build and machine can do. Populated at `Initialize`.
        [[nodiscard]] const Platform& GetPlatform() const noexcept
        {
            return platform_;
        }

    protected:
        void Initialize() override;
        void LoadContent() override;
        void UnloadContent() override;
        void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;
        void OnActivated(System::Object* sender, const System::EventArgs& args) override;
        void OnDeactivated(System::Object* sender, const System::EventArgs& args) override;

    private:
        /// @brief Everything the frame draws, into whatever target is currently bound.
        ///
        /// Separated from `Draw` so a capture frame renders exactly what the player sees, rather
        /// than a second code path that could drift from it -- which is what makes a screenshot
        /// usable as a regression fixture at all.
        void RenderFrame();
        void DrawHud();
        void ConfigureTouchInput();

        /// @brief The `Pass::Hud` implementation, defined in the .cpp because it is an adapter onto
        ///        `DrawHud` and nothing else needs its name.
        class HudPass;

        /// @brief The `Pass::OpaqueDynamic` adapter onto the content smoke scene (`HOUSE-00201`).
        class SmokePass;

        /// @brief Loads `Fonts/ui-16` into the HUD, or logs why it could not. Called once.
        void LoadHudFont();

        /// @brief Loads the Tier-E effect set, or falls back to Tier S. Called once, from
        ///        `LoadContent`.
        void ActivateTierE();

        /// @brief Settles the quality preset and its resolved settings. Called once, AFTER
        ///        `ActivateTierE`, because both depend on the final tier.
        void ResolveQuality();

        /// @brief Records a crash, attempts an emergency save, and asks the game to stop.
        void HandleCrash(std::string_view where, const std::exception* what);

        Options options_;
        Settings settings_;
        Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
        FrameTimer timer_;
        Platform platform_;
        rendering::RenderTier tier_;

        /// Shared typed content caches. Declared before `renderer_` so renderer-owned passes release
        /// effects and texture lookup closures before the cached textures are destroyed.
        std::unique_ptr<content::Caches> caches_;
        /// The shared stock-effect pool for binder-backed material passes (`HOUSE-00897`). It
        /// outlives the renderer-owned passes that borrow it because it is declared first.
        std::unique_ptr<rendering::MaterialBinder> materialBinder_;
        /// The one live §31.5 atmosphere value borrowed by binder-backed exterior passes.
        std::unique_ptr<rendering::FogParams> exteriorFog_;
        /// The fixed XNA particle stream borrowed by the transparent pass.
        std::unique_ptr<rendering::ParticleRenderer> particleRenderer_;
        /// The retained §37.1 fixed rain positions that feed the shared stream.
        std::unique_ptr<weather::RainParticles> rainParticles_;
        /// §37.2's offline-authored roof and soffit heights, shared by every rain update.
        std::optional<weather::CoverageMask> coverageMask_;

        /// The draw side of `cna-house.md` §7.5. Constructed with the tier, so the two Tier-E-only
        /// passes are gated in ONE place rather than at each pass. Only the HUD pass is installed
        /// today; phases 12 onwards install the rest and the frame's shape does not change when
        /// they do.
        rendering::Renderer renderer_;

        /// Needs a `GraphicsDevice`, so it cannot be a plain member: built in `Initialize`, once
        /// the device exists.
        std::optional<rendering::StateTracker> states_;

        /// The §68 Graphics tab, resolved: the preset's row with everything this build and profile
        /// cannot do removed. Settled at the END of `LoadContent`, because it depends on the final
        /// tier and the tier is not final until the Tier-E effect set has been tried.
        rendering::QualitySettings quality_;

        /// A second `ContentManager`, over the `.xnb` effect tree. `HOUSE-00076` measured that one
        /// built with a null service provider throws at the first load, so it takes the `Game`'s.
        std::unique_ptr<Microsoft::Xna::Framework::Content::ContentManager> effectContent_;
        /// Constructed silent and opened by the first user gesture (`HOUSE-00155`), which is what a
        /// browser requires and what the desktop build therefore does too, so the path is exercised
        /// everywhere rather than only in the build that needs it.
        audio::AudioSystem audio_;
        player::KeyboardMouseSource input_;
        player::TouchSource touchInput_;
        player::MouseCapturePolicy mouseCapture_;
        ui::TextRenderer text_;
        ui::EnvironmentReadout environmentReadout_;
        /// The screen stack of §67.3. The loading/title screen is pushed onto it at `LoadContent`
        /// and pops itself once the player has pressed something AND content is ready.
        ui::MenuStack menus_;
        ui::MenuCommand pendingMenuCommand_ = ui::MenuCommand::None;
        ui::ControlScheme pendingControlScheme_ = ui::ControlScheme::KeyboardMouse;
        ui::ControlsHint controlsHint_;
        bool controlsHintShown_ = false;
        /// Non-owning, valid only while the loading screen is on the stack. Cleared the frame the
        /// stack empties, which is the only frame it can dangle in.
        ui::LoadingScreen* loading_ = nullptr;
        /// A short average, so the HUD's number is readable rather than flickering every frame.
        float smoothedDelta_ = 1.0f / 60.0f;
        /// Per-frame CPU deltas, in milliseconds. Filled only when `frameLimit_ != 0`.
        std::vector<float> frameTimes_;

        // The measurements are compiled ALWAYS; only the overlay that presents them is gated on
        // `CNAHOUSE_DEBUG_TOOLS`. A counter that exists only in a debug build cannot be asserted by
        // a perf test, which would make the perf tests measure a different program.
        debug::Counters counters_;
        debug::Timing timing_;
        debug::Overlay overlay_;

        /// The render target the frame is drawn into when a screenshot is wanted.
        ///
        /// There is no XNA way to read the presented back buffer, so a capture frame is rendered
        /// into a target of the same size and saved from there. That also makes the image
        /// independent of what the compositor did with the window -- no title bar, no cursor,
        /// nothing on top -- which is the only way a screenshot is usable as a regression fixture.
        class Capture;
        std::unique_ptr<Capture> capture_;
        std::string pendingScreenshot_;
        bool exitAfterScreenshot_ = false;

        /// Test-only full-frame GPU fence. Kept beside screenshot capture because both use the same
        /// truthful XNA mechanism: render to a preserved target, then read one texel.
        bool gpuCompletionSampling_ = false;
        std::vector<float> renderSubmitTimes_;
        std::vector<float> gpuCompletionTimes_;

        /// @brief Set when `Update` or `Draw` threw. The frame after, the game stops.
        ///
        /// **A crash boundary is not a `catch (...)` that swallows** -- `docs/conventions.md` §5.4
        /// forbids exactly that. It catches, logs what was thrown with the frame it happened in,
        /// attempts an emergency save, and then *stops*, because a game that keeps running after an
        /// unhandled exception is a game producing a second, less comprehensible failure.
        bool crashed_ = false;
        std::string crashMessage_;

        /// @brief `--scene=blockout`: the static shell, drawn and nothing else (`HOUSE-00475`).
        static constexpr const char* kBlockoutScene = "blockout";
        /// @brief `--scene=blockout-normals`: the same house with the culling reversed, so that
        ///        every pixel drawn is a face that should not have been visible (`HOUSE-00478`).
        static constexpr const char* kBackFaceScene = "blockout-normals";
        /// @brief `--scene=walk`: the same shell with a BODY in it, seen through its eyes
        ///        (`HOUSE-00633`).
        ///
        /// The difference from `blockout` is not the geometry, it is who is looking: §49's capsule
        /// stands on the floor, §44's camera sits 1.68 m over its soles behind a 70° lens, and the
        /// keys walk it. That is what makes a frame from here a picture of the GAME rather than a
        /// picture of the model -- an eye at head height cannot float through a wall to get a
        /// better angle, and a ceiling 20 mm too low is obvious from under it and invisible from
        /// outside.
        static constexpr const char* kWalkScene = "walk";

        /// @brief Reads `chunks.bin`, makes every cell resident, installs `Pass::OpaqueStatic`.
        ///
        /// Failure is logged and the scene is empty rather than fatal: a blockout that could not
        /// load should say so and still draw a frame, because the frame is how anyone would see
        /// that it had not.
        void LoadBlockout();

        /// @brief Loads §16's world and §49.2's collision and stands a body in it.
        ///
        /// After `LoadBlockout`, because the walk scene is the blockout with somebody in it. A
        /// failure here leaves `walking_` false and the scene draws from the fixed camera, which
        /// is the same rule the blockout follows: say so, and still draw a frame.
        void LoadWalk();

        /// @brief §49.3's fixed steps for one frame, then §44's view over them.
        void UpdateWalk(float deltaSeconds);

        /// @brief Copies §44's camera into the renderer's, which is what the pass draws through.
        void ApplyPlayerCamera();
        void ApplyAudioAndControlSettings();
        void ApplyGraphicsSettings(ui::SettingsControl control);
        void ApplyEnvironmentSettings(ui::SettingsControl control);
        void ApplyChangedSetting(ui::SettingsControl control);
        void OpenSettings();
        void OpenMainMenu();
        void OpenPauseMenu();
        void OpenCredits();
        void QueueMenuCommand(ui::MenuCommand command, ui::ControlScheme scheme) noexcept;
        void HandleMenuCommand();
        void StartHouse(ui::ControlScheme scheme);

        /// @brief Whether this frame's draw list is built from §25's visible set.
        ///
        /// Three separate questions: `cull on`, a scene that HAS a walk, and a walk that has run.
        /// A blockout camera on the road is in no cell, so §25 has nowhere to start and the
        /// answer is no -- which is not the same as having been turned off.
        [[nodiscard]] bool CullingApplied() const noexcept;

        /// @brief §25.1's step 5 for this frame: fills `renderList_` and lets the passes sort it.
        ///
        /// Called at the top of `RenderFrame`, because it needs the camera the frame will be drawn
        /// with and the passes read it immediately after.
        void BuildRenderList();
        /// @brief §25.6's steps 1 and 2 over `exteriorScene_`, once a frame (`HOUSE-00700`).
        void CullExterior();
        /// @brief Adds what §25.6 found and §25.2's walk did not, so nothing is drawn twice.
        void AddExteriorChunks(const Microsoft::Xna::Framework::Vector3& eye);

    public:
        /// @brief The chunks §25.6 added to the last frame's draw list over and above §25.2's.
        ///
        /// The one number a test cannot get from the F3 snapshot: it reports what each SYSTEM
        /// decided, and this is what survived the union of the two.
        [[nodiscard]] std::size_t ExteriorChunksAddedForTesting() const noexcept
        {
            return exteriorAdded_;
        }

    private:
        /// @brief The view and projection the FRAME is drawn with, for world-space annotations.
        ///
        /// The body's eye until §25.8's `F5` detaches the camera. Drawing the cones through the
        /// body's camera while the picture came from the inspection one would put them somewhere
        /// they are not, which is the one thing a freeze must not do.
        [[nodiscard]] Microsoft::Xna::Framework::Matrix DebugView() const;
        [[nodiscard]] Microsoft::Xna::Framework::Matrix DebugProjection();

        /// @brief Builds and draws §71's `F9` and §25.8's `F4` through `debugDraw_`.
        ///
        /// ONE `Begin`/`Flush` pair for both: `DebugDraw::Begin` discards what is queued, so two
        /// overlays each opening their own would leave only the second on screen -- and the two
        /// are most useful together, a capsule standing inside the cones that decided what it can
        /// see.
        void DrawPhysicsOverlay();

        /// @brief What §69's `F2` shows about this frame.
        [[nodiscard]] debug::WorldSnapshot WalkSnapshot() const;

        /// @brief §25's walk for this frame, and §25.1's step 3 over its answer.
        ///
        /// Runs whenever the walk scene has a world: the visible set is computed every frame and
        /// `F3` reports it, whether or not the draw list is built from it. That is deliberate --
        /// a culling system nobody can see the answer of is one nobody can debug, and
        /// `HOUSE-00684`'s `cull off` will need exactly this separation anyway.
        void UpdateVisibility(const FrameContext& frame);

        /// @brief The room containing the camera eye, without §16.4's sticky body-cell margin.
        ///
        /// Gameplay keeps the prior cell for 5 cm to stabilize collision and room transitions;
        /// rendering cannot, because a doorway's portal is already behind the eye in that band.
        [[nodiscard]] util::Id VisualCell() const;

        /// @brief What §25.8's `F3` shows about this frame.
        [[nodiscard]] debug::VisibilitySnapshot VisibilitySnapshot() const;

        class Hud;
        std::unique_ptr<Hud> hud_;

        /// The content smoke scene of `HOUSE-00201`, or null. Constructed in `LoadContent` when
        /// `--scene=content-smoke` asked for it, and installed as the `OpaqueDynamic` pass -- the
        /// pass a prop belongs in, so the scene exercises the frame's real shape rather than a
        /// bypass of it.
        std::unique_ptr<content::SmokeScene> smoke_;

        /// The blockout of `HOUSE-00475`, or empty. `--scene=blockout` loads `chunks.bin`, makes
        /// every cell resident and installs `Pass::OpaqueStatic`; anything else leaves all three
        /// alone, because the house is 2.9 MB of geometry and the title screen does not need it.
        ///
        /// Declared in this order and not another: `blockoutCells_` holds buffers that name
        /// `blockoutChunks_`'s data and the device, so it must be destroyed before either. Members
        /// are destroyed in reverse declaration order, which makes this ordering the guarantee
        /// rather than a comment about one.
        std::unique_ptr<world::ChunkLibrary> blockoutChunks_;
        std::unique_ptr<world::CellRuntime> blockoutCells_;
        rendering::Camera blockoutCamera_;
        debug::FreeFlyCamera freeFly_;
        /// §25.1's step 5 (`HOUSE-00675`): the frame's sorted draw list, rebuilt every frame and
        /// read by the passes. Owned here rather than by a pass because §17.1 gives it to the
        /// renderer and not to any one pass -- several of them read their own slice of the one
        /// list, and the sort that makes those slices contiguous has to happen once.
        visibility::RenderList renderList_;

        /// §16's world and §49.2's collision, loaded only by `--scene=walk`. Held as options
        /// because both are large and neither has a meaningful empty state.
        std::optional<world::WorldData> world_;
        std::optional<world::SpatialIndex> index_;
        std::optional<physics::CollisionWorld> collision_;
        physics::BroadPhase broad_;
        player::PlayerState player_;
        player::LookAngles look_;
        player::FirstPersonView view_;
        player::CellTracker tracker_;
        debug::WorldOverlay worldOverlay_;
        /// §25.8's `F3`, and the walk it reports. Both exist only in the walk scene: the blockout
        /// camera is outside the house and in no cell, and §16.4 has no answer for it.
        debug::VisibilityOverlay visibilityOverlay_;
        debug::VisibilityGeometryOverlay visibilityGeometry_;
        /// @brief §71's `F8`, whose time section is `HOUSE-01537`'s.
        debug::EnvironmentOverlay environmentOverlay_;
        /// @brief §35.1's clock, advanced once a frame from the REAL delta (`HOUSE-01536`).
        ///
        /// One clock, here, because §35.1 says *"everything time-dependent reads it; nothing else
        /// keeps its own"*. It takes `FrameContext::realDeltaSeconds` and not the clamped one: a
        /// hitch advances the afternoon by the time that really passed (`HOUSE-01540`), while
        /// §49.3's accumulator runs the simulation slow for that frame.
        environment::SimClock clock_;
        /// §36 and §42's sole live weather vector, target, expiry, RNG and transition snapshot.
        std::optional<weather::WeatherSystem> weather_;
        std::optional<visibility::VisibilitySystem> visibility_;
        /// @brief §22's baked window occlusion. It outlives `lighting_`, whose daylight model
        ///        keeps a pointer to it.
        lighting::ShadingGrid shading_ = lighting::ShadingGrid::Unshaded();
        /// @brief §28.1's per-room lighting, at `UpdateStage::Lighting` (`HOUSE-01251`).
        std::optional<lighting::LightingSystem> lighting_;
        /// @brief Non-owning handle to the `Pass::Sky` object owned by `renderer_`.
        ///
        /// The lighting stage copies its one current sun into the pass before drawing. The pass is
        /// installed only for a successfully loaded walk world and remains owned by `renderer_`.
        rendering::SkySystem* skySystem_ = nullptr;
        std::optional<visibility::ChunkCuller> chunkCuller_;
        /// @brief §25.6's hierarchy over the exterior chunks, and the walk over it
        ///        (`HOUSE-00700`).
        ///
        /// The outdoors is not a room: §25.6 says portal traversal cannot help inside `EXT_WORLD`,
        /// and a lawn filed under a yard the walk did not reach was a lawn nobody drew. These two
        /// answer the exterior; `chunkCuller_` answers the rooms.
        std::optional<visibility::ExteriorScene> exteriorScene_;
        visibility::ExteriorCuller exteriorCuller_;
        std::vector<visibility::ClipFrustum> exteriorCones_;
        std::vector<std::uint32_t> exteriorChunks_;
        /// @brief How many chunks §25.6 put in this frame's draw list that §25.2's walk had NOT
        ///        already found.
        ///
        /// The draw list is the union of two answers, so neither system's own count describes it
        /// and `drawCalls == chunksDrawn` stopped being true the moment the outdoors joined. This
        /// is the difference, and it is what makes the whole statement checkable again.
        std::size_t exteriorAdded_ = 0u;
        /// §25.8's `F5`: the walk stops being recomputed and the camera leaves the body, so what
        /// was culled can be flown out to and looked at. *"The single most useful debugging tool
        /// for a portal system."*
        bool visibilityFrozen_ = false;
        /// §71's `cull off|on` (`HOUSE-00684`). On by default -- §25 exists to be used -- and off
        /// is what `HOUSE-00688` compares against: the walk still runs and `F3` still reports it,
        /// and what stops is the draw list being built from its answer.
        bool cullingEnabled_ = true;
        /// §71's command registry. The console has no prompt yet (§71's own task); the commands
        /// are registered against it and driven by the tests, which is what `HOUSE-00563`
        /// established.
        debug::Console console_;
        /// §71's `F9`, and the line renderer it draws through. Both exist only in the walk scene:
        /// `DebugDraw` needs a device, and the overlay needs a `CollisionWorld` and a camera --
        /// which is exactly what `HOUSE-00620` recorded as the reason `F9` was not wired yet.
        debug::PhysicsOverlay physicsOverlay_;
        std::unique_ptr<debug::DebugDraw> debugDraw_;
        /// §49.3's leftover time: the fixed step is 1/120 s and a frame is not.
        float stepAccumulator_ = 0.0F;
        bool lifecyclePaused_ = false;
        /// @brief A frame-edge survives zero-step frames and reaches exactly one physics step.
        bool pendingRunToggle_ = false;
        bool walking_ = false;
        std::uint64_t fixedSteps_ = 0;
        /// @brief Set by `SetInputSourceForTesting`; null selects the platform's normal source.
        player::IInputSource* scriptedInput_ = nullptr;

        /// @brief Whichever source is driving this session.
        [[nodiscard]] player::IInputSource& Input() noexcept
        {
            if (scriptedInput_ != nullptr)
            {
                return *scriptedInput_;
            }
            return TouchHudVisible() ? static_cast<player::IInputSource&>(touchInput_)
                                     : static_cast<player::IInputSource&>(input_);
        }

        [[nodiscard]] bool TouchHudVisible() const noexcept
        {
            return platform_.hasTouch && !platform_.hasKeyboard;
        }

        std::uint64_t framesDrawn_ = 0;
        std::uint64_t frameLimit_ = 0;
        std::uint64_t fixedStepLimit_ = 0;
        int exitCode_ = 0;
        bool contentLoaded_ = false;
    };

} // namespace cnahouse::app
