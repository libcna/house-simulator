// SPDX-License-Identifier: MIT
#include "cnahouse/player/LandingDip.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace cnahouse::player
{
    namespace
    {
        /// A critically damped spring kicked from rest with velocity `v` reaches its extreme at
        /// `t = 1/ω`, where `x = v/(ω·e)`. Turning the depth wanted back into the velocity that
        /// produces it is that relation the other way round.
        constexpr float kEuler = std::numbers::e_v<float>;
    } // namespace

    float LandingDip::DepthFor(float drop) noexcept
    {
        return std::clamp(kDipPerMetre * drop, 0.0F, kMaxDip);
    }

    void LandingDip::Land(float drop) noexcept
    {
        const float depth = DepthFor(drop);
        if (!(depth > 0.0F))
        {
            return;
        }

        // Downwards, and sized so the LOWEST point of the dip is the depth asked for -- not the
        // starting position, which is still exactly where the eye already was.
        const float velocity = -depth * kDipOmega * kEuler;
        // Deeper wins; a second landing does not add to the first. Two landings inside a quarter
        // of a second is a bounce down a flight of stairs, and adding their dips would put the eye
        // through the tread.
        velocity_ = std::min(velocity_, velocity);
    }

    void LandingDip::Update(float dt) noexcept
    {
        if (!(dt > 0.0F))
        {
            return;
        }

        // The exact solution of `x'' = -2ω·x' - ω²·x` over `dt`:
        //
        //   x(t) = (x + (v + ω·x)·t)·e^(−ω·t)
        //   v(t) = (v − ω·(v + ω·x)·t)·e^(−ω·t)
        //
        // Both come from the same `(A + B·t)·e^(−ω·t)`, and stepping them exactly is what makes
        // the dip the same shape at 30 frames a second and at 240 -- which matters because the
        // dip is something the player watches for a quarter of a second rather than a correction
        // they never see.
        const float decay = std::exp(-kDipOmega * dt);
        const float b = velocity_ + kDipOmega * offset_;
        const float offset = (offset_ + b * dt) * decay;
        const float velocity = (velocity_ - kDipOmega * b * dt) * decay;
        offset_ = offset;
        velocity_ = velocity;

        // No clamp to keep the offset negative. A landing does not lift, and that is a PROPERTY
        // of this spring rather than something to enforce afterwards: the impulse is only ever
        // downwards and a critically damped spring does not overshoot, so `x` starts at zero,
        // goes down and comes back to it. Clamping anyway would silence the one thing worth
        // hearing -- an injected under-damped spring, which bounces the eye up past where it
        // started -- so the test asserts the property and the code does not fake it.
    }

} // namespace cnahouse::player
