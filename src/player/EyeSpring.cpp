// SPDX-License-Identifier: MIT
#include "cnahouse/player/EyeSpring.hpp"

namespace cnahouse::player
{

    float EyeSpring::Update(float target, float dt, bool stiff) noexcept
    {
        if (dt <= 0.0F)
        {
            return height_;
        }
        const float omega = stiff ? kEyeSpringStairsOmega : kEyeSpringOmega;

        // The critically damped spring `x'' = -2ω·x' - ω²·(x - target)`, integrated semi-implicitly:
        // the new velocity is solved for FIRST, using the position it is about to produce, and the
        // position then follows it.
        //
        //   v' = (v - dt·ω²·(x - target)) / (1 + 2ω·dt + (ω·dt)²)
        //   x' = x + dt·v'
        //
        // The damping is exactly `2ω` and never a tuned fraction of it -- that is what "critically
        // damped" means, and it is why this has ONE parameter rather than two: it is the unique
        // amount that reaches the target in the shortest time without overshooting it.
        //
        // Solved rather than stepped explicitly, because the explicit pair diverges once `ω·dt`
        // passes 2 -- at §48.2's ω = 24 that is an 83 ms frame, which is a loading hitch and not a
        // hypothetical. Here the denominator only grows with dt, so a long frame settles the eye
        // instead of launching it.
        const float denominator = 1.0F + 2.0F * omega * dt + omega * omega * dt * dt;
        velocity_ = (velocity_ - dt * omega * omega * (height_ - target)) / denominator;
        height_ += dt * velocity_;
        return height_;
    }

} // namespace cnahouse::player
