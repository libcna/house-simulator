// SPDX-License-Identifier: MIT
#include "cnahouse/player/HeadBob.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace cnahouse::player
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        constexpr float kPi = std::numbers::pi_v<float>;
        constexpr float kDegreesToRadians = kPi / 180.0F;

        /// The sway takes two steps to come back to where it started -- left foot, right foot --
        /// so that is the cycle the phase is kept inside. Wrapping matters: a walk across the plot
        /// is 400 m, which is 533 steps, and a float phase that counted them all would be adding
        /// centimetres of stride to a number in the hundreds.
        constexpr float kCycleSteps = 2.0F;

        float Scale(HeadBobLevel level) noexcept
        {
            switch (level)
            {
                case HeadBobLevel::Off:
                    return 0.0F;
                case HeadBobLevel::Subtle:
                    return 1.0F;
                case HeadBobLevel::Normal:
                    return kNormalBobScale;
            }
            return 0.0F;
        }
    } // namespace

    std::string_view HeadBobLevelName(HeadBobLevel level) noexcept
    {
        switch (level)
        {
            case HeadBobLevel::Off:
                return "off";
            case HeadBobLevel::Subtle:
                return "subtle";
            case HeadBobLevel::Normal:
                return "normal";
        }
        return "?";
    }

    HeadBobLevel HeadBobLevelFromName(std::string_view name, HeadBobLevel fallback) noexcept
    {
        if (name == "off")
        {
            return HeadBobLevel::Off;
        }
        if (name == "subtle")
        {
            return HeadBobLevel::Subtle;
        }
        if (name == "normal")
        {
            return HeadBobLevel::Normal;
        }
        return fallback;
    }

    void HeadBob::Reset() noexcept
    {
        phase_ = 0.0F;
        steps_ = 0;
    }

    int HeadBob::TakeSteps() noexcept
    {
        const int taken = steps_;
        steps_ = 0;
        return taken;
    }

    HeadBobOffset HeadBob::Update(const Xna::Vector3& travel, float dt, bool onGround, bool fastWalk) noexcept
    {
        if (!(dt > 0.0F))
        {
            // A paused frame is not a stride. Returning the offset the phase already has keeps the
            // view still rather than snapping it to neutral for one frame.
            return Offsets();
        }

        // Horizontal only. The vertical part of a frame's travel is a stair or a fall, and §62.4
        // counts a stair in risers rather than in metres.
        const float distance = std::sqrt(travel.X * travel.X + travel.Z * travel.Z);
        const float stride = fastWalk ? kBobStrideFast : kBobStrideNormal;
        const float before = phase_;
        float advanced = phase_ + distance / stride;

        if (!onGround)
        {
            // A body in the air is taking no steps -- but it was taking one when it left the
            // ground, and cutting that off mid-swing is a jump in the view. It finishes the step
            // it is in and then stops, which leaves the eye exactly at rest for the landing.
            //
            // The plant is taken as the WHOLE NUMBER rather than as this frame's advance clamped
            // to it: `phase + (ceil(phase) - phase)` is 0.99999994 as often as it is 1, and a
            // phase that stops a hundred-millionth short of a plant never reports the step and
            // leaves the eye a hundred-millionth off the floor for ever.
            advanced = std::min(advanced, std::ceil(phase_));
        }

        phase_ = advanced;
        // Every whole number crossed is a foot on the floor -- the count §62.4's footsteps read.
        steps_ += static_cast<int>(std::floor(phase_)) - static_cast<int>(std::floor(before));
        phase_ = std::fmod(phase_, kCycleSteps);

        speed_ = onGround ? distance / dt : 0.0F;
        return Offsets();
    }

    HeadBobOffset HeadBob::Offsets() const noexcept
    {
        const float amplitude = kBobAmplitude * Scale(level_) * speed_ / kBobReferenceSpeed;
        if (!(amplitude > 0.0F))
        {
            return HeadBobOffset{};
        }

        HeadBobOffset offset;
        // Zero at the plant, `amplitude` at mid-stance: standing height is the BOTTOM of a walking
        // cycle, because the head rises over the leg that carries it. The eye therefore never
        // drops below the height §43.1 gives it, and every foot plant is at that height, so the
        // waveform is continuous through a stop, a teleport and a landing alike.
        offset.vertical = amplitude * 0.5F * (1.0F - std::cos(2.0F * kPi * phase_));
        // ...and the sway is the same cycle at half the rate: one way on the left foot, the other
        // on the right, through zero at each plant.
        const float sway = kBobSwayDegrees * kDegreesToRadians * Scale(level_) * speed_ / kBobReferenceSpeed;
        offset.yaw = sway * std::sin(kPi * phase_);
        return offset;
    }

} // namespace cnahouse::player
