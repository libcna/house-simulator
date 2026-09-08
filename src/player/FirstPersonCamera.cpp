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
        constexpr float kRadiansToDegrees = 180.0F / std::numbers::pi_v<float>;

        Xna::Vector3 Cross(const Xna::Vector3& a, const Xna::Vector3& b)
        {
            return Xna::Vector3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
        }
    } // namespace

    float HorizontalFovDegrees(float verticalDegrees, float aspect) noexcept
    {
        return 2.0F * std::atan(aspect * std::tan(verticalDegrees * 0.5F * kDegreesToRadians)) *
               kRadiansToDegrees;
    }

    float VerticalFovDegrees(float horizontalDegrees, float aspect) noexcept
    {
        return 2.0F * std::atan(std::tan(horizontalDegrees * 0.5F * kDegreesToRadians) / aspect) *
               kRadiansToDegrees;
    }

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

    void FirstPersonCamera::SetViewport(int width, int height) noexcept
    {
        // Both, not either: a viewport of -1600 by -900 divides out to 16:9, and a window that
        // reports negative pixels has not told us its shape -- it has told us it has none.
        if (width > 0 && height > 0)
        {
            SetAspect(static_cast<float>(width) / static_cast<float>(height));
        }
    }

    float FirstPersonCamera::EffectiveFieldOfViewDegrees() const noexcept
    {
        // Every window a display comes in is 4:3 or wider, and there the SETTING is the answer:
        // §44's angle is vertical, so a wider window is one that shows more of the room and not
        // one that shows the same room bigger.
        if (aspect_ >= kNarrowestSupportedAspect)
        {
            return fovDegrees_;
        }
        // Below that, opening the lens is the lesser evil. Holding the vertical on a portrait
        // window leaves 43° of horizontal field at the default setting, and a corridor seen
        // through 43° cannot be walked down; the distortion of a wider lens can at least be
        // looked past.
        const float floorHorizontal = HorizontalFovDegrees(fovDegrees_, kNarrowestSupportedAspect);
        return std::min(VerticalFovDegrees(floorHorizontal, aspect_), kMaxEffectiveFovDegrees);
    }

    float FirstPersonCamera::HorizontalFieldOfViewDegrees() const noexcept
    {
        return HorizontalFovDegrees(EffectiveFieldOfViewDegrees(), aspect_);
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
            EffectiveFieldOfViewDegrees() * kDegreesToRadians, aspect_, kNearPlane, kFarPlane);
    }

} // namespace cnahouse::player
