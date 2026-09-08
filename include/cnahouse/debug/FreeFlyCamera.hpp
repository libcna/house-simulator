// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/player/IInputSource.hpp"
#include "cnahouse/rendering/Camera.hpp"

namespace cnahouse::debug
{

    /// @brief A camera that flies, for looking at the blockout (`HOUSE-00476`).
    ///
    /// **A debug tool, not the player camera.** It ignores collision, gravity, head height and the
    /// portal graph on purpose: what it is for is standing in the middle of a wall to see which side
    /// of it is inside out, and a camera that could not do that would need a second camera to
    /// inspect it with. §26's player camera is a different object with different rules.
    ///
    /// Reads `player::InputState`, which is intent rather than keys: `move` in the camera's own
    /// plane, `look` in radians, `run` to go faster, `jump`/`crouch` for straight up and down. So
    /// this class needs no window, no device and no XNA input to be exercised, and its claims are
    /// unit tests rather than a person flying about.
    class FreeFlyCamera
    {
    public:
        /// @brief Metres per second, walking. A house is 22 m across, so this crosses it in six
        ///        seconds -- fast enough not to be tedious, slow enough to stop where you meant to.
        static constexpr float kSpeed = 4.0f;
        /// @brief What `run` multiplies the speed by. Enough to cross the 400 m plot in a minute.
        static constexpr float kRunMultiplier = 6.0f;
        /// @brief The pitch limit, in radians: just under a right angle.
        ///
        /// Straight up is where a yaw-then-pitch camera loses its horizon and starts to roll, and
        /// the last degree is worth nothing to look at.
        static constexpr float kPitchLimit = 1.5533431f; // 89 degrees

        /// @brief Places the camera where @p camera is and points it where @p camera points.
        ///
        /// The yaw and pitch are recovered from the eye-to-target direction, so a fixed camera can
        /// be taken over mid-flight without the view jumping -- which is the whole point of being
        /// able to switch to this one while looking at something.
        void Adopt(const rendering::Camera& camera);

        /// @brief One frame of flight.
        /// @param lookAvailable false when the window is not focused or the pointer is not
        ///        captured; the look delta is then ignored rather than integrated, because an
        ///        unfocused window's mouse delta is garbage (`HOUSE-00100`).
        void Update(const player::InputState& input, bool lookAvailable, float deltaSeconds);

        /// @brief Writes the eye and target into @p camera, leaving its lens alone.
        void ApplyTo(rendering::Camera& camera) const;

        [[nodiscard]] const Microsoft::Xna::Framework::Vector3& Position() const noexcept
        {
            return position_;
        }

        [[nodiscard]] float Yaw() const noexcept
        {
            return yaw_;
        }

        [[nodiscard]] float Pitch() const noexcept
        {
            return pitch_;
        }

        /// @brief The unit vector the camera looks along.
        [[nodiscard]] Microsoft::Xna::Framework::Vector3 Forward() const;

        void SetPosition(const Microsoft::Xna::Framework::Vector3& position)
        {
            position_ = position;
        }

    private:
        Microsoft::Xna::Framework::Vector3 position_{0.0f, 1.7f, 0.0f};
        /// Radians about +Y. 0 looks along −Z, which §14 calls north.
        float yaw_ = 0.0f;
        /// Radians. Positive looks up.
        float pitch_ = 0.0f;
    };

} // namespace cnahouse::debug
