// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/player/PlayerController.hpp"

namespace cnahouse::player
{

    /// @brief §44's field of view: 70° vertical by default, settings-controlled 55°-95°.
    ///
    /// VERTICAL, because that is the one XNA's `CreatePerspectiveFieldOfView` takes and because a
    /// vertical angle is the one that does not change when the window does. §44's "≈ 100°
    /// horizontal at 16:9" is this number seen the other way round, and a test asserts the two
    /// agree rather than carrying the second as a number of its own.
    inline constexpr float kDefaultFovDegrees = 70.0F;
    inline constexpr float kMinFovDegrees = 55.0F;
    inline constexpr float kMaxFovDegrees = 95.0F;

    /// @brief §10.3's clip planes: 0.10 m and 420 m.
    ///
    /// *"Depth precision at 24-bit with a 0.10 m near plane is adequate for interiors"* -- and the
    /// far plane has to reach the neighbourhood shell at 220 m and the road at 35 m with room to
    /// spare, while the sky dome at 900 m is drawn with depth writes off so it never meets it.
    inline constexpr float kNearPlane = 0.10F;
    inline constexpr float kFarPlane = 420.0F;

    /// @brief §44's pitch limit. `HOUSE-00623` applies it; the number lives with the camera it
    ///        belongs to, so there is one of it.
    inline constexpr float kMaxPitchDegrees = 85.0F;

    /// @brief Where the camera is and which way it faces, in world space.
    struct CameraPose
    {
        Microsoft::Xna::Framework::Vector3 eye;
        Microsoft::Xna::Framework::Vector3 forward;
        Microsoft::Xna::Framework::Vector3 right;
        Microsoft::Xna::Framework::Vector3 up;
        float yaw = 0.0F;
        float pitch = 0.0F;
    };

    /// @brief §44's first-person camera: the eye the controller carries, a view and a projection
    ///        (`HOUSE-00621`).
    ///
    /// **The eye height is given, not computed.** §44 asks for it *"smoothed by a critically-damped
    /// spring (ω = 18 rad/s) so step-ups and stair climbing do not jolt the view"*, and that spring
    /// is `EyeSpring` (`HOUSE-00561`), which the controller owns because §48.2 stiffens it on
    /// stairs. A camera that ran its own would be a second answer to "how high is the eye".
    ///
    /// **Roll is always zero, by construction rather than by assertion.** The right vector comes
    /// from the YAW alone -- §14's `(cos yaw, 0, sin yaw)` -- so it is horizontal whatever the
    /// pitch does, and the up vector is what the other two leave. Building the basis this way also
    /// survives a pitch of exactly ±90°, where `CreateLookAt` with a world up has no answer.
    class FirstPersonCamera
    {
    public:
        /// @brief Sets §44's vertical field of view in DEGREES, clamped to its band.
        void SetFieldOfView(float degrees) noexcept;

        [[nodiscard]] float FieldOfViewDegrees() const noexcept
        {
            return fovDegrees_;
        }

        /// @brief Width over height. A zero or negative aspect is ignored: a window minimised to
        ///        nothing must not put a NaN in the projection matrix and then in everything the
        ///        renderer multiplies by it.
        void SetAspect(float aspect) noexcept;

        [[nodiscard]] float Aspect() const noexcept
        {
            return aspect_;
        }

        /// @brief Places the camera from the body and its smoothed eye height.
        ///
        /// @param eyeHeight metres above the FEET, which is what §43.1's 1.68 m standing and
        ///        1.15 m crouched are, and what `EyeSpring` smooths.
        void Update(const PlayerState& state, float eyeHeight, float pitch) noexcept;

        [[nodiscard]] const CameraPose& Pose() const noexcept
        {
            return pose_;
        }

        [[nodiscard]] Microsoft::Xna::Framework::Matrix View() const;
        [[nodiscard]] Microsoft::Xna::Framework::Matrix Projection() const;

    private:
        CameraPose pose_;
        float fovDegrees_ = kDefaultFovDegrees;
        float aspect_ = 16.0F / 9.0F;
    };

} // namespace cnahouse::player
