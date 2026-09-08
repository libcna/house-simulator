// SPDX-License-Identifier: MIT
#include "cnahouse/player/MouseLook.hpp"

#include <cmath>
#include <numbers>

namespace cnahouse::player
{
    namespace
    {
        constexpr float kTwoPi = 2.0F * std::numbers::pi_v<float>;

        /// Into (-π, π]. `fmod` alone leaves the sign of the input, which puts a body that turned
        /// west the long way at -π rather than +π and makes two equal facings compare unequal.
        float Wrapped(float radians) noexcept
        {
            if (radians > -std::numbers::pi_v<float> && radians <= std::numbers::pi_v<float>)
            {
                // Already in range, and returned UNTOUCHED. `fmod(x + π, 2π) - π` is arithmetic
                // about π, and for a yaw of 0.0022 rad -- one pixel of §44's mouse look -- it
                // gives back 0.0021998882: the answer is right to six figures and wrong in the
                // last bit, every frame, for a wrap that was not needed.
                return radians;
            }
            float wrapped = std::fmod(radians + std::numbers::pi_v<float>, kTwoPi);
            if (wrapped <= 0.0F)
            {
                wrapped += kTwoPi;
            }
            return wrapped - std::numbers::pi_v<float>;
        }
    } // namespace

    void ApplyLook(LookAngles& angles, const InputState& input, bool available) noexcept
    {
        if (!available)
        {
            return;
        }
        // X to the right turns EAST, which §14 says is positive yaw.
        angles.yaw = Wrapped(angles.yaw + input.look.X);
        // ...and the mouse forward looks UP. Screen Y grows downward, so the source's `look.Y` is
        // positive when the hand moves back; pitch is positive up, so it is subtracted. The
        // invert-Y setting has already been applied by the source and must not be applied twice.
        angles.pitch -= input.look.Y;
    }

} // namespace cnahouse::player
