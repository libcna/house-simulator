// SPDX-License-Identifier: MIT
#include "cnahouse/debug/FreeFlyCamera.hpp"

#include <algorithm>
#include <cmath>

#include "Microsoft/Xna/Framework/MathHelper.hpp"

namespace cnahouse::debug
{
    namespace Xna = Microsoft::Xna::Framework;

    namespace
    {
        /// Wraps to (-pi, pi], so yaw can be turned about all day without losing precision.
        float WrapAngle(float radians) noexcept
        {
            return Xna::MathHelper::WrapAngle(radians);
        }

    } // namespace

    Xna::Vector3 FreeFlyCamera::Forward() const
    {
        const float cosPitch = std::cos(pitch_);
        // Yaw 0 looks along -Z, which §14 calls north, and positive yaw turns EAST -- §14's
        // "clockwise seen from above". A quarter turn therefore has to land on +X, and the sign of
        // the sine is the whole of that: with it the other way round the camera turns west and
        // every other angle in the file still looks self-consistent.
        return Xna::Vector3(std::sin(yaw_) * cosPitch, std::sin(pitch_), -std::cos(yaw_) * cosPitch);
    }

    void FreeFlyCamera::Adopt(const rendering::Camera& camera)
    {
        position_ = camera.eye;
        const Xna::Vector3 direction = Xna::Vector3(
            camera.target.X - camera.eye.X, camera.target.Y - camera.eye.Y, camera.target.Z - camera.eye.Z);
        const float horizontal = std::sqrt(direction.X * direction.X + direction.Z * direction.Z);
        if (horizontal <= 1e-6f && std::fabs(direction.Y) <= 1e-6f)
        {
            // A camera whose target is its own eye has no direction to adopt. Keeping the current
            // one is the only answer that does not invent a view.
            return;
        }
        yaw_ = WrapAngle(std::atan2(direction.X, -direction.Z));
        pitch_ = std::clamp(std::atan2(direction.Y, horizontal), -kPitchLimit, kPitchLimit);
    }

    void FreeFlyCamera::Update(const player::InputState& input, bool lookAvailable, float deltaSeconds)
    {
        if (lookAvailable)
        {
            yaw_ = WrapAngle(yaw_ + input.look.X);
            pitch_ = std::clamp(pitch_ + input.look.Y, -kPitchLimit, kPitchLimit);
        }

        const float speed = kSpeed * (input.run ? kRunMultiplier : 1.0f) * deltaSeconds;
        const Xna::Vector3 forward = Forward();
        // Strafe stays HORIZONTAL however far the camera is pitched: a right vector that tilted
        // with the view would make sideways motion climb, and the one thing a camera used to
        // inspect a wall must do is hold its height while it slides along one.
        const Xna::Vector3 right(std::cos(yaw_), 0.0f, std::sin(yaw_));

        position_.X += (forward.X * input.move.Y + right.X * input.move.X) * speed;
        position_.Y += forward.Y * input.move.Y * speed;
        position_.Z += (forward.Z * input.move.Y + right.Z * input.move.X) * speed;

        // Straight up and down, in WORLD terms, whatever the camera is looking at.
        const float vertical = (input.jump ? 1.0f : 0.0f) - (input.crouch ? 1.0f : 0.0f);
        position_.Y += vertical * speed;
    }

    void FreeFlyCamera::ApplyTo(rendering::Camera& camera) const
    {
        const Xna::Vector3 forward = Forward();
        camera.eye = position_;
        camera.target =
            Xna::Vector3(position_.X + forward.X, position_.Y + forward.Y, position_.Z + forward.Z);
    }

} // namespace cnahouse::debug
