// SPDX-License-Identifier: MIT
#pragma once

#include <optional>

#include "Microsoft/Xna/Framework/Input/Touch/TouchCollection.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/player/IInputSource.hpp"

namespace cnahouse::player
{
    /// @brief Dimensions and sensitivity of the touch-only walkthrough input.
    struct TouchConfig
    {
        static constexpr float kStickRadiusVirtual = 180.0F;
        static constexpr float kButtonInsetVirtual = 160.0F;
        int viewportWidth = 1600;
        int viewportHeight = 900;
        int layoutX = 0;
        int layoutY = 0;
        /// @brief Zero uses the whole viewport; the game supplies TextRenderer's safe canvas.
        int layoutWidth = 0;
        int layoutHeight = 0;
        float lookSensitivity = 1.0F;
        bool invertY = false;
    };

    /// @brief Turns XNA touch snapshots into independent movement, look and menu-tap intent.
    ///
    /// Finger IDs own their roles until release, even if TouchCollection changes order. The
    /// source owns device sampling and gesture edges so no game system reads TouchPanel directly.
    class TouchSource final : public IInputSource
    {
    public:
        explicit TouchSource(TouchConfig config = {});

        void Update(float deltaSeconds) override;
        void Apply(const Microsoft::Xna::Framework::Input::Touch::TouchCollection& touches,
                   float deltaSeconds);

        /// @brief Discard fingers held before focus/background loss without changing the control layout.
        void Reset() noexcept;

        /// @brief Whether XNA currently reports an active touch, for Web's first-touch selection.
        [[nodiscard]] bool HasActiveTouch() const;

        [[nodiscard]] const InputState& Current() const noexcept override
        {
            return state_;
        }

        [[nodiscard]] bool LookAvailable() const noexcept override
        {
            return lookAvailable_;
        }

        /// @brief Active floating-stick geometry in back-buffer pixels, if a finger owns it.
        [[nodiscard]] std::optional<Microsoft::Xna::Framework::Vector2> StickOrigin() const noexcept;
        [[nodiscard]] std::optional<Microsoft::Xna::Framework::Vector2> StickPosition() const noexcept;

        /// @brief Use a desktop mouse as one test finger; production Android still reads TouchPanel.
        void SetMouseEmulation(bool enabled) noexcept
        {
            emulateMouse_ = enabled;
        }

        void SetButtonsEnabled(bool enabled) noexcept
        {
            buttonsEnabled_ = enabled;
        }

        void SetConfig(const TouchConfig& config) noexcept
        {
            config_ = config;
        }

    private:
        struct Finger
        {
            int id;
            Microsoft::Xna::Framework::Vector2 origin;
            Microsoft::Xna::Framework::Vector2 previous;
        };

        TouchConfig config_;
        InputState state_{};
        std::optional<Finger> stick_;
        std::optional<Finger> look_;
        std::optional<int> menuFinger_;
        std::optional<int> speedFinger_;
        bool lookAvailable_ = false;
        bool emulateMouse_ = false;
        bool mouseWasDown_ = false;
        bool buttonsEnabled_ = false;
    };

} // namespace cnahouse::player
