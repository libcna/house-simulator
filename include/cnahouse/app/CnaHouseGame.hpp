// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"

#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/FrameTimer.hpp"
#include "cnahouse/app/Platform.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/debug/Overlay.hpp"
#include "cnahouse/debug/Timing.hpp"
#include "cnahouse/player/KeyboardMouseSource.hpp"
#include "cnahouse/rendering/RenderTier.hpp"
#include "cnahouse/rendering/Renderer.hpp"
#include "cnahouse/rendering/StateTracker.hpp"
#include "cnahouse/ui/TextRenderer.hpp"

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

        /// @brief Stops after this many frames. 0 means run until the user exits.
        void SetFrameLimit(std::uint64_t frames) noexcept
        {
            frameLimit_ = frames;
        }

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

    private:
        /// @brief Everything the frame draws, into whatever target is currently bound.
        ///
        /// Separated from `Draw` so a capture frame renders exactly what the player sees, rather
        /// than a second code path that could drift from it -- which is what makes a screenshot
        /// usable as a regression fixture at all.
        void RenderFrame();
        void DrawHud();

        /// @brief The `Pass::Hud` implementation, defined in the .cpp because it is an adapter onto
        ///        `DrawHud` and nothing else needs its name.
        class HudPass;

        /// @brief Loads the Tier-E effect set, or falls back to Tier S. Called once, from
        ///        `LoadContent`.
        void ActivateTierE();

        /// @brief Records a crash, attempts an emergency save, and asks the game to stop.
        void HandleCrash(std::string_view where, const std::exception* what);

        Options options_;
        Settings settings_;
        Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
        FrameTimer timer_;
        Platform platform_;
        rendering::RenderTier tier_;

        /// The draw side of `cna-house.md` §7.5. Constructed with the tier, so the two Tier-E-only
        /// passes are gated in ONE place rather than at each pass. Only the HUD pass is installed
        /// today; phases 12 onwards install the rest and the frame's shape does not change when
        /// they do.
        rendering::Renderer renderer_;

        /// Needs a `GraphicsDevice`, so it cannot be a plain member: built in `Initialize`, once
        /// the device exists.
        std::optional<rendering::StateTracker> states_;

        /// A second `ContentManager`, over the `.xnb` effect tree. `HOUSE-00076` measured that one
        /// built with a null service provider throws at the first load, so it takes the `Game`'s.
        std::unique_ptr<Microsoft::Xna::Framework::Content::ContentManager> effectContent_;
        player::KeyboardMouseSource input_;
        ui::TextRenderer text_;
        /// A short average, so the HUD's number is readable rather than flickering every frame.
        float smoothedDelta_ = 1.0f / 60.0f;

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

        /// @brief Set when `Update` or `Draw` threw. The frame after, the game stops.
        ///
        /// **A crash boundary is not a `catch (...)` that swallows** -- `docs/conventions.md` §5.4
        /// forbids exactly that. It catches, logs what was thrown with the frame it happened in,
        /// attempts an emergency save, and then *stops*, because a game that keeps running after an
        /// unhandled exception is a game producing a second, less comprehensible failure.
        bool crashed_ = false;
        std::string crashMessage_;

        class Hud;
        std::unique_ptr<Hud> hud_;

        std::uint64_t framesDrawn_ = 0;
        std::uint64_t frameLimit_ = 0;
        int exitCode_ = 0;
        bool contentLoaded_ = false;
    };

} // namespace cnahouse::app
