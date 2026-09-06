// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <string>

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"

#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/FrameTimer.hpp"
#include "cnahouse/app/Platform.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/player/KeyboardMouseSource.hpp"
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
        void DrawHud();

        Options options_;
        Settings settings_;
        Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
        FrameTimer timer_;
        Platform platform_;
        player::KeyboardMouseSource input_;
        ui::TextRenderer text_;
        /// A short average, so the HUD's number is readable rather than flickering every frame.
        float smoothedDelta_ = 1.0f / 60.0f;

        class Hud;
        std::unique_ptr<Hud> hud_;

        std::uint64_t framesDrawn_ = 0;
        std::uint64_t frameLimit_ = 0;
        int exitCode_ = 0;
        bool contentLoaded_ = false;
    };

} // namespace cnahouse::app
