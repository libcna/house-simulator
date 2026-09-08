// SPDX-License-Identifier: MIT
#pragma once

namespace cnahouse::player
{

    /// @brief §44's eye-height spring, in rad/s.
    inline constexpr float kEyeSpringOmega = 18.0F;
    /// @brief §48.2's stiffened one, for a body on stairs.
    ///
    /// *"The eye-height spring is stiffened (ω = 24) on stairs so the view rises steadily rather
    /// than bobbing per step."* A climb is a sequence of small lifts arriving at 120 Hz, and a
    /// spring soft enough to hide a single kerb turns that sequence into a wallow.
    inline constexpr float kEyeSpringStairsOmega = 24.0F;

    /// @brief The smoothed eye height (§44), so that a step up does not jolt the view.
    ///
    /// **Critically damped, and that is the whole specification.** A critically damped spring is
    /// the one that reaches its target in the shortest time WITHOUT overshooting: any less damping
    /// and the view bounces past the top of a step, any more and it lags. There is one parameter,
    /// ω, and §44 and §48.2 give its two values -- nothing here is tuned by feel.
    ///
    /// The integrator is the semi-implicit form, which is stable for any ω and dt this game will
    /// ever use. The explicit form is one line shorter and blows up when `ω·dt` approaches 2 --
    /// which at ω = 24 is a frame of 83 ms, well inside what a loading hitch produces.
    class EyeSpring
    {
    public:
        /// @brief Places the eye AT @p height with no motion. For a spawn, a teleport, a save
        ///        load -- anywhere the view should not travel to its new position.
        void Snap(float height) noexcept
        {
            height_ = height;
            velocity_ = 0.0F;
        }

        /// @brief Advances towards @p target by @p dt seconds.
        ///
        /// @param stiff true on stairs (§48.2), which selects ω = 24 rather than 18.
        /// @return the smoothed height.
        float Update(float target, float dt, bool stiff = false) noexcept;

        [[nodiscard]] float Height() const noexcept
        {
            return height_;
        }

        /// @brief How fast the eye is currently moving, in m/s. Zero when it has settled.
        [[nodiscard]] float Velocity() const noexcept
        {
            return velocity_;
        }

    private:
        float height_ = 0.0F;
        float velocity_ = 0.0F;
    };

} // namespace cnahouse::player
