// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <string>

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"

#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/FrameTimer.hpp"
#include "cnahouse/app/Settings.hpp"

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

        class Hud;
        std::unique_ptr<Hud> hud_;

        std::uint64_t framesDrawn_ = 0;
        std::uint64_t frameLimit_ = 0;
        int exitCode_ = 0;
        bool contentLoaded_ = false;
    };

} // namespace cnahouse::app
