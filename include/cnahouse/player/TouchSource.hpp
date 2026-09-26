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
        int viewportWidth = 1600;
        int viewportHeight = 900;
        float lookSensitivity = 1.0F;
        bool invertY = false;
    };

    /// @brief Turns XNA touch snapshots into independent movement, look and menu-tap intent.
    ///
    /// Finger IDs own their roles until release, even if TouchCollection changes order. M15's
    /// following HUD task supplies the visual stick and button regions; this source owns device
    /// sampling and gesture edges so no game system needs to read TouchPanel directly.
    class TouchSource final : public IInputSource
    {
    public:
        explicit TouchSource(TouchConfig config = {});

        void Update(float deltaSeconds) override;
        void Apply(const Microsoft::Xna::Framework::Input::Touch::TouchCollection& touches,
                   float deltaSeconds);

        [[nodiscard]] const InputState& Current() const noexcept override
        {
            return state_;
        }

        [[nodiscard]] bool LookAvailable() const noexcept override
        {
            return lookAvailable_;
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
        bool lookAvailable_ = false;
    };

} // namespace cnahouse::player
