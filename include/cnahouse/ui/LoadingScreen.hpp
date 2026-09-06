// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>

#include "cnahouse/ui/MenuStack.hpp"

namespace cnahouse::ui
{

    /// @brief The loading screen, which is also the title screen, which is also the audio gate.
    ///
    /// **All three are one screen deliberately** (`cna-house.md` §7.4 and the Web row of §9): a
    /// browser will not open an audio device until the user has interacted with the page, so
    /// something has to wait for a key press before play starts. Making that the loading screen
    /// means the wait costs nothing — the player reads "press any key" while content is still
    /// loading, and the gate is already satisfied by the time it would have blocked anything.
    ///
    /// It is on every platform, not only on Web. A gate that existed only in the browser build
    /// would be a path nobody exercised until the browser build (`HOUSE-00155`).
    class LoadingScreen final : public IScreen
    {
    public:
        /// @param versionLine what the corner of every frame says; shown here too, because the
        ///        title screen is the most likely thing in a bug report's first screenshot.
        /// @param onGesture called ONCE, when the player first presses something. This is where the
        ///        audio device is opened.
        LoadingScreen(std::string versionLine, std::function<void()> onGesture);

        [[nodiscard]] ScreenId Id() const override
        {
            return ScreenId::Loading;
        }

        /// @brief Marks loading finished. Until this is called the screen will not dismiss.
        void SetReady(bool ready) noexcept
        {
            ready_ = ready;
        }

        [[nodiscard]] bool IsReady() const noexcept
        {
            return ready_;
        }

        /// @brief Whether the user's gesture has been seen. Used by the tests and the overlay.
        [[nodiscard]] bool GestureSeen() const noexcept
        {
            return gestureSeen_;
        }

        /// @brief How long the screen has been up, in seconds. Drives the prompt's pulse.
        [[nodiscard]] float Elapsed() const noexcept
        {
            return elapsed_;
        }

        ScreenAction Update(const player::InputState& input, float deltaSeconds) override;

        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                  const TextRenderer& text) const override;

        /// @brief Opaque: nothing is behind it yet.
        [[nodiscard]] bool IsTranslucent() const override
        {
            return false;
        }

        /// @brief The world does not simulate while the title screen is up.
        ///
        /// Unlike the pause menu (§67.3, which deliberately keeps the clock running), there is
        /// nothing to keep running here: the house has not been built yet.
        [[nodiscard]] bool PausesWorld() const override
        {
            return true;
        }

    private:
        std::string versionLine_;
        std::function<void()> onGesture_;
        bool ready_ = false;
        bool gestureSeen_ = false;
        float elapsed_ = 0.0f;
    };

} // namespace cnahouse::ui
