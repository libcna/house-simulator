// SPDX-License-Identifier: MIT
#pragma once

namespace cnahouse::player
{

    /// @brief How far the eye dips per metre fallen, and the most it ever dips.
    ///
    /// §43.1 puts the hard landing at 2.4 m, so 0.035 m/m reaches 0.084 m there and the cap is the
    /// next centimetre up: a fall from the roof and a fall from the first floor land the same way,
    /// because past a certain height the knees have already done everything they can. §44's rule
    /// governs the small end -- *"any [head motion] large enough to notice is too large"* -- and a
    /// landing is the one moment the player is meant to notice, which is why the dip is seven
    /// times §44's bob at the top and a tenth of it stepping off a kerb.
    inline constexpr float kDipPerMetre = 0.035F;
    inline constexpr float kMaxDip = 0.09F;

    /// @brief The dip's spring, in rad/s.
    ///
    /// The bottom of the dip is at 1/ω, so 16 rad/s puts it 62 ms after the landing, with nine
    /// tenths of the recovery done by a third of a second and the last millimetre gone by 0.5 s. That is the
    /// shape of a knee: a fast give and a slow return. A softer spring reads as the floor sinking
    /// rather than as landing on it, and a stiffer one as a hit on the head.
    inline constexpr float kDipOmega = 16.0F;

    /// @brief §44's landing dip: the knee flex the eye does when a fall ends (`HOUSE-00629`).
    ///
    /// **An impulse, not a displacement.** A dip applied by moving the eye down and letting it
    /// spring back starts with a step -- the eye is somewhere else on the landing frame than it
    /// was on the one before -- and a step is the pop the dip exists to smooth. What lands is a
    /// VELOCITY: the eye is where it was, moving downwards, and the spring turns that into a dip
    /// and a recovery with no discontinuity anywhere in it.
    ///
    /// **Critically damped, and integrated exactly.** The target is zero and stays zero between
    /// impulses, so the closed form of `x'' = -2ω·x' - ω²·x` is available -- unlike `EyeSpring`,
    /// whose target moves every frame and which therefore solves a step at a time. Exactly is
    /// worth having here because the dip is a SHAPE: a quarter of a second the player watches, and
    /// it must be the same shape on a machine drawing 30 frames a second and one drawing 240.
    class LandingDip
    {
    public:
        /// @brief A fall of @p drop metres has just ended (§43.1's `Landing`).
        ///
        /// The impulse is sized so the dip's lowest point is `min(kDipPerMetre · drop, kMaxDip)`.
        /// Landing again while still dipping keeps whichever is deeper rather than adding them:
        /// two landings in a quarter of a second is a bounce down a flight, and adding the dips
        /// would put the eye through the floor.
        void Land(float drop) noexcept;

        /// @brief Advances the recovery by @p dt seconds.
        void Update(float dt) noexcept;

        /// @brief Metres to ADD to the eye height.
        ///
        /// Never positive, and never clamped to make it so: the impulse is only ever downwards
        /// and a critically damped spring does not overshoot, so this is a property of the spring
        /// rather than a rule applied on top of it.
        [[nodiscard]] float Offset() const noexcept
        {
            return offset_;
        }

        [[nodiscard]] float Velocity() const noexcept
        {
            return velocity_;
        }

        /// @brief Puts the eye back where it was, with no motion. For a teleport or a load.
        void Reset() noexcept
        {
            offset_ = 0.0F;
            velocity_ = 0.0F;
        }

        /// @brief The depth a landing from @p drop metres dips to.
        [[nodiscard]] static float DepthFor(float drop) noexcept;

    private:
        float offset_ = 0.0F;
        float velocity_ = 0.0F;
    };

} // namespace cnahouse::player
