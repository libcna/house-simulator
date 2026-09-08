// SPDX-License-Identifier: MIT
#include "cnahouse/player/FirstPersonCamera.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace cnahouse::player
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        constexpr float kDegreesToRadians = std::numbers::pi_v<float> / 180.0F;

        Xna::Vector3 Cross(const Xna::Vector3& a, const Xna::Vector3& b)
        {
            return Xna::Vector3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
        }
    } // namespace

    void FirstPersonCamera::SetFieldOfView(float degrees) noexcept
    {
        fovDegrees_ = std::clamp(degrees, kMinFovDegrees, kMaxFovDegrees);
    }

    void FirstPersonCamera::SetAspect(float aspect) noexcept
    {
        if (aspect > 0.0F && std::isfinite(aspect))
        {
            aspect_ = aspect;
        }
    }

    void FirstPersonCamera::Update(const PlayerState& state, float eyeHeight, float pitch) noexcept
    {
        pose_.yaw = state.yaw;
        pose_.pitch = pitch;
        // §44: the eye is the body's feet plus the smoothed height, and NOT the capsule's centre.
        // A crouch drops the centre by exactly what the half-height loses (`PlayerState::Feet`),
        // so measuring from the feet is what makes 1.68 m standing and 1.15 m crouched one rule.
        const Xna::Vector3 feet = state.Feet();
        pose_.eye = Xna::Vector3(feet.X, feet.Y + eyeHeight, feet.Z);

        // §14: yaw 0 looks north (-Z) and positive turns east.
        const float cosYaw = std::cos(state.yaw);
        const float sinYaw = std::sin(state.yaw);
        const float cosPitch = std::cos(pitch);
        const float sinPitch = std::sin(pitch);
        pose_.forward = Xna::Vector3(sinYaw * cosPitch, sinPitch, -cosYaw * cosPitch);
        pose_.right = Xna::Vector3(cosYaw, 0.0F, sinYaw);
        pose_.up = Cross(pose_.right, pose_.forward);
    }

    Xna::Matrix FirstPersonCamera::View() const
    {
        const Xna::Vector3 target(
            pose_.eye.X + pose_.forward.X, pose_.eye.Y + pose_.forward.Y, pose_.eye.Z + pose_.forward.Z);
        return Xna::Matrix::CreateLookAt(pose_.eye, target, pose_.up);
    }

    Xna::Matrix FirstPersonCamera::Projection() const
    {
        return Xna::Matrix::CreatePerspectiveFieldOfView(
            fovDegrees_ * kDegreesToRadians, aspect_, kNearPlane, kFarPlane);
    }

} // namespace cnahouse::player
