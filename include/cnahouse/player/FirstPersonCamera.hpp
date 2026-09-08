// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/player/HeadBob.hpp"
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

    /// @brief The aspect §44's two field-of-view numbers are stated at.
    ///
    /// *"70° vertical by default (102.4° horizontal at 16:9)"* is one lens described twice, and
    /// 16:9 is where the two descriptions meet. Nothing is held AT this aspect -- it is the
    /// reference the other two constants below are read against.
    inline constexpr float kDesignAspect = 16.0F / 9.0F;

    /// @brief The narrowest window shape the horizontal field of view is allowed to shrink to.
    ///
    /// §44 gives a VERTICAL angle, so a wider window shows more of the room and a narrower one
    /// shows less -- the ordinary "Hor+" behaviour, and the right one: the vertical angle is what
    /// sets the apparent size of everything on screen, so holding it is what makes a doorway the
    /// same doorway on every display. 4:3 is where that stops being harmless. Below it the
    /// horizontal field falls away fast (a portrait window at 70° vertical sees 43° of the room,
    /// which is a letterbox turned on its side and no way to walk down a corridor), so from here
    /// down the lens OPENS instead: the vertical grows to hold the horizontal angle 4:3 has.
    ///
    /// The floor is deliberately the narrowest shape a DISPLAY comes in rather than the design
    /// aspect. Between 4:3 and 16:9 a player sees less of the room than the screenshot in §44 --
    /// that is what a narrower monitor is -- and nothing is distorted to hide it.
    inline constexpr float kNarrowestSupportedAspect = 4.0F / 3.0F;

    /// @brief The hard stop on what the narrow-window rule may open the lens to.
    ///
    /// The rule above has no natural limit: as the window approaches zero width the vertical
    /// angle it asks for approaches 180°, where the projection matrix is degenerate and the
    /// frame is a fish-eye with the room squeezed into a few pixels in the middle. A window that
    /// tall is not a shape this game is played in; the cap keeps the matrix sane instead of
    /// pretending to serve it.
    inline constexpr float kMaxEffectiveFovDegrees = 120.0F;

    /// @brief §44's second number from its first: `tan(h/2) = aspect · tan(v/2)`.
    ///
    /// A free function because the relation is the design's, not the camera's: `HOUSE-00621`'s
    /// test reads §44's 102.4° out of its 70°, and the debug overlays quote both.
    /// @pre @p aspect is positive and finite -- `FirstPersonCamera::SetAspect` is the gate.
    [[nodiscard]] float HorizontalFovDegrees(float verticalDegrees, float aspect) noexcept;

    /// @brief The same relation the other way: the vertical angle that gives @p horizontalDegrees.
    /// @pre @p aspect is positive and finite.
    [[nodiscard]] float VerticalFovDegrees(float horizontalDegrees, float aspect) noexcept;

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

        /// @brief The same, from a viewport's pixels.
        ///
        /// The division belongs HERE rather than at every call site, because a back buffer is two
        /// integers and `width / height` written on integers is a zero-divide before `SetAspect`
        /// ever sees it. It is also the only place that can tell a nonsense viewport from a
        /// plausible one: a width and a height that are both negative divide out to a perfectly
        /// respectable 16:9, which is exactly the shape of answer a float division launders.
        void SetViewport(int width, int height) noexcept;

        [[nodiscard]] float Aspect() const noexcept
        {
            return aspect_;
        }

        /// @brief The vertical angle the projection actually uses.
        ///
        /// Equal to the SETTING at 4:3 and every wider window, which is every window a display
        /// makes; wider than the setting below that, by `kNarrowestSupportedAspect`'s rule, and
        /// never past `kMaxEffectiveFovDegrees`.
        [[nodiscard]] float EffectiveFieldOfViewDegrees() const noexcept;

        /// @brief What the player can see left to right, for the settings screen and §71's
        ///        overlays -- derived here rather than stored, so there is one lens.
        [[nodiscard]] float HorizontalFieldOfViewDegrees() const noexcept;

        /// @brief Places the camera from the body and its smoothed eye height.
        ///
        /// @param eyeHeight metres above the FEET, which is what §43.1's 1.68 m standing and
        ///        1.15 m crouched are, and what `EyeSpring` smooths.
        /// @param bob §44's head motion (`HOUSE-00627`), applied AFTER the spring. Through the
        ///        spring it would be a 1.8 Hz wobble fed into a filter that is there to remove
        ///        one, and what came out would be neither the bob nor the smoothing.
        void Update(const PlayerState& state,
                    float eyeHeight,
                    float pitch,
                    const HeadBobOffset& bob = {}) noexcept;

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
