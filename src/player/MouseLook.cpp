// SPDX-License-Identifier: MIT
#include "cnahouse/player/MouseLook.hpp"

#include "cnahouse/player/FirstPersonCamera.hpp"

#include <algorithm>
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
        // §44's ±85°, applied where the pitch CHANGES rather than where it is read: a clamp at
        // the reading end leaves the stored angle drifting past the pole while the mouse is
        // pushed, and the view then takes the same distance back before it moves at all.
        angles.pitch = ClampedPitch(angles.pitch - input.look.Y);
    }

    float ClampedPitch(float pitch) noexcept
    {
        constexpr float kLimit = kMaxPitchDegrees * std::numbers::pi_v<float> / 180.0F;
        return std::clamp(pitch, -kLimit, kLimit);
    }

} // namespace cnahouse::player
