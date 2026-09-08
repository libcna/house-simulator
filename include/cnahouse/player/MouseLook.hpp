// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/player/IInputSource.hpp"

namespace cnahouse::player
{

    /// @brief Where the player is looking: §14's yaw, and §44's pitch.
    ///
    /// The yaw is the BODY's -- the controller walks along it (§43.2) -- and the pitch is the
    /// camera's alone, because §43.1's capsule does not lean. They are one struct because a mouse
    /// moves both with one motion and splitting them would put half of that in two places.
    struct LookAngles
    {
        /// @brief Radians. §14: 0 looks north (-Z), positive turns EAST, and it is kept in
        ///        (-π, π] so that a player who spins for an hour has the same number as one who
        ///        did not -- a `float` that reaches 10 000 rad has lost a thousandth of a radian
        ///        of precision, which is a visible drift in a save file.
        float yaw = 0.0F;
        /// @brief Radians, positive UP, and never outside §44's ±85°.
        ///
        /// **Short of the pole, not at it.** At exactly ±90° the view has no horizon to level
        /// against and the smallest yaw becomes a spin; 5° of margin is what every first-person
        /// game leaves, and §44 states this one.
        float pitch = 0.0F;
    };

    /// @brief One frame of §44's mouse look (`HOUSE-00622`).
    ///
    /// **The sensitivity is not applied here.** `InputState::look` arrives in RADIANS:
    /// `IInputSource` owns the pixels, the 0.0022 rad/px and the invert-Y (`HOUSE-00140`), because
    /// a system that polled the mouse could not be replayed, remapped or tested without a window.
    /// Applying a multiplier here as well would be the same setting twice, and the second one
    /// would be the one nobody could find.
    ///
    /// @param available `IInputSource::LookAvailable()`. False when the window has focus but the
    ///        snapshot did not advance -- an unfocused window and a still hand are
    ///        indistinguishable from here (`HOUSE-00100`), and neither should move the view.
    void ApplyLook(LookAngles& angles, const InputState& input, bool available) noexcept;

    /// @brief §44's pitch limit in radians: ±85°, and `Clamped` is where it is applied
    ///        (`HOUSE-00623`).
    ///
    /// Roll needs no function of its own and has none: §44 says *"Roll is always zero"*, and
    /// `FirstPersonCamera` builds its basis from the yaw's horizontal right vector, so there is
    /// nowhere for a roll to come from. The test that says so is the camera's.
    [[nodiscard]] float ClampedPitch(float pitch) noexcept;

} // namespace cnahouse::player
