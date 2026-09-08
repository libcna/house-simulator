// SPDX-License-Identifier: MIT
#pragma once

#include <string_view>

#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace cnahouse::player
{

    /// @brief §68's three head-bob levels. `Subtle` is the default the settings table gives.
    enum class HeadBobLevel
    {
        Off,
        Subtle,
        Normal
    };

    [[nodiscard]] std::string_view HeadBobLevelName(HeadBobLevel level) noexcept;
    [[nodiscard]] HeadBobLevel HeadBobLevelFromName(std::string_view name, HeadBobLevel fallback) noexcept;

    /// @brief §62.4's stride: one footstep every 0.75 m walking, every 0.95 m at the fast walk.
    ///
    /// A DISTANCE and not a period, which is what keeps the feet under the body: a cadence in
    /// seconds would take the same number of steps to cross a room at either speed.
    inline constexpr float kBobStrideNormal = 0.75F;
    inline constexpr float kBobStrideFast = 0.95F;

    /// @brief §44's *"very small vertical bob, amplitude 0.012 m · (speed/1.35)"*.
    ///
    /// 1.35 m/s is §43.2's walk, so the constant IS the amplitude at a walking pace and the
    /// fraction is what makes a slower or faster one move less or more. §44's rule for the size:
    /// *"any bob large enough to notice is too large"*.
    inline constexpr float kBobAmplitude = 0.012F;
    inline constexpr float kBobReferenceSpeed = 1.35F;

    /// @brief §44's *"0.35° lateral sway"*, and it is a sway of the AIM and not of the horizon.
    ///
    /// §44 gives this one in degrees while saying two lines earlier that *"roll is always zero"*
    /// during ordinary walking, so the axis it turns about is the vertical one: the view drifts
    /// left and right by a third of a degree as the weight passes from foot to foot, and the
    /// horizon stays level. A roll would be the usual reading of "lateral sway" and it is the one
    /// §44 rules out.
    inline constexpr float kBobSwayDegrees = 0.35F;

    /// @brief What `Normal` multiplies §44's numbers by.
    ///
    /// §44's amplitude describes the DEFAULT, which §68 calls `Subtle`, so `Normal` is the level a
    /// player picks when they want to feel their footsteps. Twice is the smallest multiple that
    /// reads as a decision rather than as rounding, and it is a tuning number: `HOUSE-00632` walks
    /// the house and records the final one.
    inline constexpr float kNormalBobScale = 2.0F;

    /// @brief What the bob adds to the view: a height and a turn, both zero at every foot plant.
    struct HeadBobOffset
    {
        /// @brief Metres ABOVE the smoothed eye height. Never negative: standing height is the
        ///        bottom of a walking cycle, and the head rises over the leg that carries it.
        float vertical = 0.0F;
        /// @brief Radians added to the view's yaw. Left on one step and right on the next.
        float yaw = 0.0F;
    };

    /// @brief §44's head motion, on §62.4's cadence (`HOUSE-00627`).
    ///
    /// **This owns the step accumulator, and it owns it for the footsteps too.** §62.4's director
    /// (phase 12) needs exactly the same count -- one step per 0.75 m -- and two accumulators
    /// would let the sound and the view disagree about when a foot lands, which is the one thing
    /// a bob has to get right. So the accumulator runs at every level INCLUDING `Off`: a camera
    /// setting must not change when the house makes a footstep sound.
    ///
    /// **Everything is a function of distance travelled, not of time.** Halve the frame rate and
    /// the body covers the same ground per second, crosses the same strides, and the eye is in the
    /// same place -- which is what makes `HOUSE-00632`'s tuning mean anything on another machine.
    class HeadBob
    {
    public:
        void SetLevel(HeadBobLevel level) noexcept
        {
            level_ = level;
        }

        [[nodiscard]] HeadBobLevel Level() const noexcept
        {
            return level_;
        }

        /// @brief Advances the cadence by one frame's travel and returns the view's offset.
        ///
        /// @param travel the frame's motion in world space; only its HORIZONTAL part is a stride,
        ///        because a stair is climbed one riser at a time and §48.2 stiffens the eye
        ///        spring rather than bobbing per tread.
        /// @param dt the frame's length, which with @p travel gives the speed §44 scales by --
        ///        derived here rather than passed, so the two can never disagree.
        /// @param onGround a body in the air is taking no steps. It finishes the step it was in
        ///        and then holds still, which leaves the offset at zero for the landing.
        /// @param fastWalk §43.2's mode, which is §62.4's other stride.
        HeadBobOffset Update(const Microsoft::Xna::Framework::Vector3& travel,
                             float dt,
                             bool onGround,
                             bool fastWalk) noexcept;

        /// @brief Footsteps crossed since the last call, and clears the count.
        ///
        /// The reason this class holds the accumulator: §62.4's footsteps come from here.
        [[nodiscard]] int TakeSteps() noexcept;

        /// @brief Where in the two-step cycle the body is, in steps. A whole number is a plant.
        [[nodiscard]] float StepPhase() const noexcept
        {
            return phase_;
        }

        /// @brief Puts the body back on a foot plant. For a teleport (§57), where the distance
        ///        between one frame and the next is not a walk.
        void Reset() noexcept;

        /// @brief The offset the current phase and speed give. `Update` returns this; a caller
        ///        that needs it twice in a frame (the view and a test) does not have to store it.
        [[nodiscard]] HeadBobOffset Offsets() const noexcept;

    private:
        HeadBobLevel level_ = HeadBobLevel::Subtle;
        /// @brief In STEPS, wrapped over the two-step cycle the sway takes to come back.
        float phase_ = 0.0F;
        int steps_ = 0;
        /// @brief The last frame's ground speed, which is what §44's amplitude scales by. Zero in
        ///        the air and zero when standing still, so both stop the bob without a branch.
        float speed_ = 0.0F;
    };

} // namespace cnahouse::player
