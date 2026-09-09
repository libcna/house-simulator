// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>

#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"
#include "Microsoft/Xna/Framework/Input/MouseState.hpp"

#include "cnahouse/player/IInputSource.hpp"

namespace cnahouse::player
{

    /// @brief Sensitivity, inversion and the recentring target. Owned by the source, applied once.
    struct InputConfig
    {
        /// @brief Radians of yaw per pixel of mouse motion, before the user's sensitivity multiplier.
        ///
        /// 0.0022 rad/px puts a 360-degree turn at roughly 2 850 px, which is the range most players
        /// are used to. It is a constant rather than a setting because the SETTING is the multiplier;
        /// exposing both would let a user find two ways to mean the same thing.
        static constexpr float kRadiansPerPixel = 0.0022f;

        float sensitivity = 1.0f;
        bool invertY = false;

        int recentreX = 0;
        int recentreY = 0;

        /// @brief §44's *"optional raw-ish smoothing over 2 frames, default off"*.
        ///
        /// The average of this frame's delta and the last one. Two frames because that is the
        /// shortest average there is: it takes the jitter off a cheap mouse and costs half a
        /// frame of latency, and anything longer is a mouse that arrives late.
        bool smoothing = false;
    };

    /// @brief The desktop input source: keyboard for movement, mouse for look.
    ///
    /// **The mouse handling is written against what `HOUSE-00100` measured, not against what one would
    /// assume.** `Mouse::GetState` returns a snapshot the platform layer maintains from motion events,
    /// not a live cursor query, so:
    ///
    ///  * the previous position is seeded from the first real sample rather than assumed to be the
    ///    window centre -- assuming the centre produces one enormous bogus delta on the first frame,
    ///    which is the classic "camera snaps on startup" bug;
    ///  * a frame whose sample equals the recentre target exactly is treated as "no motion", because
    ///    that is what the recentring wrote and not what the player did;
    ///  * `LookAvailable()` is false until a sample has actually differed from its predecessor, so a
    ///    window that has focus but no pointer over it contributes nothing.
    class KeyboardMouseSource final : public IInputSource
    {
    public:
        explicit KeyboardMouseSource(InputConfig config = {});

        void Update(float deltaSeconds) override;

        [[nodiscard]] const InputState& Current() const noexcept override
        {
            return state_;
        }

        [[nodiscard]] bool LookAvailable() const noexcept override
        {
            return lookAvailable_;
        }

        void SetConfig(const InputConfig& config) noexcept
        {
            config_ = config;
        }

        [[nodiscard]] const InputConfig& Config() const noexcept
        {
            return config_;
        }

        /// @brief Whether the source is recentring the pointer each frame.
        ///
        /// Off in menus and while a debug overlay wants a cursor; on during play.
        void SetMouseCaptured(bool captured) noexcept;

        [[nodiscard]] bool MouseCaptured() const noexcept
        {
            return captured_;
        }

        /// @brief Feeds one frame of already-sampled device state, for tests and for replay.
        ///
        /// The production path calls `Update`, which samples XNA and then calls this. Separating them
        /// is what makes the mouse behaviour testable without a window -- and `HOUSE-00100` is open
        /// precisely because that probe could not get real pointer motion.
        void Apply(const Microsoft::Xna::Framework::Input::KeyboardState& keyboard,
                   const Microsoft::Xna::Framework::Input::MouseState& mouse,
                   float deltaSeconds);

    private:
        /// The keys whose EDGES this source reports. Storing six booleans rather than a whole
        /// `KeyboardState` is not a micro-optimisation: `KeyboardState`'s default constructor is
        /// **`CNAEXT`-marked** in CNA (only the `initializer_list` one is plain XNA 4.0), so a member
        /// of that type cannot be default-constructed without reaching for an extension ADR-0001
        /// forbids. Six booleans are also all this class actually needs.
        enum class Edge : std::size_t
        {
            Interact = 0,
            WalkMode,
            Cancel,
            Menu,
            ToggleOverlay,
            ToggleWorldOverlay,
            ToggleVisibilityOverlay,
            ToggleVisibilityGeometry,
            ToggleFreezeVisibility,
            TogglePhysicsOverlay,
            Screenshot,
            Count,
        };

        InputConfig config_;
        InputState state_;
        std::array<bool, static_cast<std::size_t>(Edge::Count)> previousEdges_{};
        /// Not one of the `Edge` slots: this is "was ANYTHING down", not one named action, and
        /// giving it a slot would put it in a table whose entries are all bound keys.
        bool anyDownPreviously_ = false;
        int previousMouseX_ = 0;
        int previousMouseY_ = 0;
        bool hasPreviousMouse_ = false;
        /// The previous frame's look, for §44's two-frame average. Zeroed whenever the history is
        /// dropped -- a capture change or a frame with no motion -- for the same reason the
        /// position history is: the delta from before a menu is not this frame's aim.
        float previousLookX_ = 0.0F;
        float previousLookY_ = 0.0F;
        bool captured_ = false;
        bool lookAvailable_ = false;
    };

} // namespace cnahouse::player
