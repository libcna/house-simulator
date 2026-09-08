// SPDX-License-Identifier: MIT
#pragma once

#include <string_view>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/physics/Fall.hpp"
#include "cnahouse/physics/Ground.hpp"
#include "cnahouse/player/IInputSource.hpp"

namespace cnahouse::player
{

    /// @brief §43.1's body: a capsule of radius 0.30 m standing 1.80 m tall.
    inline constexpr float kPlayerRadius = 0.30F;
    /// @brief Half the distance between the cap centres of a STANDING body: 0.90 - 0.30.
    inline constexpr float kPlayerHalfHeight = 0.60F;
    /// @brief §43.1's eye, measured from the feet.
    inline constexpr float kPlayerEyeHeight = 1.68F;

    /// @brief §43.1's crouched body: 1.25 m tall, so a half-height of 1.25/2 - 0.30.
    inline constexpr float kPlayerCrouchHalfHeight = 0.325F;
    /// @brief §43.1's crouched eye, measured from the feet.
    inline constexpr float kPlayerCrouchEyeHeight = 1.15F;

    /// @brief §43.2's normal walk, in m/s: *"the measured average human walking speed"*.
    inline constexpr float kWalkSpeed = 1.35F;
    /// @brief §43.2's fast walk, in m/s.
    ///
    /// *"A brisk walk, not a run. Above ~2.2 m/s a human transitions to a jog, and the brief is
    /// explicit that this is still walking."* -- which is why it is 2.05 and not 2.5.
    inline constexpr float kFastWalkSpeed = 2.05F;
    /// @brief §43.2's directional and state modifiers.
    ///
    /// The three DIRECTIONAL ones are not multiplied together: a body backing away diagonally is
    /// not travelling at 0.72 x 0.85 of a walk. They are the axes of an ellipse the speed is
    /// limited by, so each pure direction is exactly its own number and a diagonal falls between
    /// the two it is between -- 0.777 for a backwards strafe, which is neither 0.72 nor 0.85 nor
    /// 0.61.
    inline constexpr float kBackwardsFactor = 0.72F;
    inline constexpr float kStrafeFactor = 0.85F;
    /// @brief The STATE modifiers, which do multiply: they are independent facts about the body.
    inline constexpr float kStairsFactor = 0.72F;
    inline constexpr float kCrouchFactor = 0.55F;
    inline constexpr float kCarryingFactor = 0.94F;
    inline constexpr float kDeepSnowFactor = 0.80F;
    /// @brief §43.2's `snowDepth > 0.12`, in metres.
    inline constexpr float kDeepSnowDepth = 0.12F;

    /// @brief §48.2: on a `Stairs` surface steeper than this, §47.2 blends to `stair_up` or
    ///        `stair_down`. In degrees.
    ///
    /// **Not the same question as §43.1's 46° slope limit**, which asks whether a surface can be
    /// stood on at all. This one asks whether the body is CLIMBING, and a half-landing is a
    /// `Stairs` surface that is flat -- so a body crossing one must not be given a climbing
    /// animation, and §48.2 gives it the in-place turn clips instead.
    inline constexpr float kStairAnimationSlopeDegrees = 15.0F;

    /// @brief §43.2's acceleration and deceleration, in m/s².
    ///
    /// *"Reaches full speed in ~0.15 s -- responsive, not floaty"*, and 1.35 / 9.0 IS 0.15 s: the
    /// two numbers in that table are one decision written twice, and a test asserts they agree.
    inline constexpr float kAcceleration = 9.0F;
    inline constexpr float kDeceleration = 13.0F;

    /// @brief Everything the controller carries from one fixed step to the next.
    struct PlayerState
    {
        /// @brief The capsule's CENTRE, not its feet.
        Microsoft::Xna::Framework::Vector3 position;
        /// @brief Horizontal velocity in m/s. The vertical component lives in `fall`, because
        ///        falling has state of its own -- how far, and from where.
        Microsoft::Xna::Framework::Vector3 velocity;
        /// @brief Radians. §14: 0 looks north (-Z) and positive turns EAST.
        float yaw = 0.0F;

        /// @brief Crouched, which in this house means the attic (§43.1). **Automatic**: the
        ///        controller sets it from the head-room and the player never asks for it.
        bool crouched = false;
        /// @brief Carrying an item (§50.5). The interaction system sets it.
        bool carrying = false;
        /// @brief Snow underfoot, in metres. §36's weather sets it; deeper than
        ///        `kDeepSnowDepth` slows the body down.
        float snowDepth = 0.0F;

        /// @brief `noclip` (§71): collision, gravity and the ground are all switched off.
        ///
        /// A development command and never reachable in a shipped build. It exists because the
        /// alternative when a room will not load or a body is stuck is to rebuild the level, and
        /// because §49.5's guarantee tests need a way to PUT a body somewhere no walk can reach.
        bool noclip = false;

        /// @brief §43.2's walk mode. `Shift` TOGGLES it; it is not hold-to-sprint.
        ///
        /// D-09 makes it a PREFERENCE rather than world state, so the app mirrors it into
        /// `Settings` and restores it from there -- it survives a save/load and a *Reset House*.
        /// The controller owns the toggle and nothing else about it.
        bool fastWalk = false;
        physics::FallState fall;

        /// @brief What `GroundProbe` last said. §60 reads `groundKind`, §31 reads `surface`, and
        ///        `HOUSE-00559`'s cell tracking reads `cellId`.
        bool onGround = true;
        Microsoft::Xna::Framework::Vector3 groundNormal{0.0F, 1.0F, 0.0F};
        std::uint16_t surface = 0u;
        physics::CollisionKind groundKind = physics::CollisionKind::Floor;
        std::string_view cellId;

        /// @brief The ground's slope ALONG the direction of travel, in degrees. Positive is up.
        ///
        /// Along the travel and not down the surface's steepest line: a body crossing a stair's
        /// half-landing, or walking along a ramp's contour, is not climbing anything, and §48.2's
        /// `stair_up` would be the wrong clip for it.
        float groundSlopeDeg = 0.0F;

        /// @brief Metres a second ALONG the ground rather than across the map.
        ///
        /// §48.2: *"the clip rate is matched to the along-slope speed, not the horizontal
        /// component, so the feet keep up with the actual travel."* On a 32° flight that is 18 %
        /// more than the horizontal speed, which is the difference between feet that walk and
        /// feet that skate.
        float alongSlopeSpeed = 0.0F;

        /// @brief §48.1's `SurfaceKind::Stairs`: the body is on a stair ramp.
        ///
        /// Offered as a name because that is how §48.2 and §48.3 refer to it, and NOT as a second
        /// enum -- which would be a second place for the collider and the player to disagree about
        /// what a stair is.
        [[nodiscard]] bool OnStairs() const noexcept
        {
            return groundKind == physics::CollisionKind::Stair;
        }

        /// @brief Half the body's segment: §43.1's standing 0.60 or crouched 0.325.
        [[nodiscard]] float HalfHeight() const
        {
            return crouched ? kPlayerCrouchHalfHeight : kPlayerHalfHeight;
        }

        /// @brief Feet to centre, which is what a crouch changes and a position does not.
        [[nodiscard]] float Rise() const
        {
            return HalfHeight() + kPlayerRadius;
        }

        /// @brief The soles. **The invariant a crouch preserves**: the body shrinks towards the
        ///        floor, so its centre drops by exactly what its half-height loses.
        [[nodiscard]] Microsoft::Xna::Framework::Vector3 Feet() const
        {
            return Microsoft::Xna::Framework::Vector3(position.X, position.Y - Rise(), position.Z);
        }

        /// @brief The eye, which is what §45's camera and §50.1's targeting ray start from.
        [[nodiscard]] Microsoft::Xna::Framework::Vector3 Eye() const
        {
            const float height = crouched ? kPlayerCrouchEyeHeight : kPlayerEyeHeight;
            return Microsoft::Xna::Framework::Vector3(position.X, position.Y - Rise() + height, position.Z);
        }

        /// @brief The body, at wherever it currently is and whichever shape it currently has.
        [[nodiscard]] physics::Capsule Body() const
        {
            return physics::Capsule{position, HalfHeight(), kPlayerRadius};
        }
    };

    /// @brief What one fixed step did, for `HOUSE-00562`'s overlay and for the tests.
    struct PlayerStepReport
    {
        /// @brief The slide used all three of its iterations and motion was left over.
        bool blocked = false;
        bool steppedUp = false;
        bool steppedDown = false;
        /// @brief `Soft` or `Hard` on the step a fall ended, `None` otherwise (§47.2's one-shot).
        physics::Landing landing = physics::Landing::None;
        /// @brief How far that fall was, in metres.
        float landingDrop = 0.0F;
        /// @brief Push-out iterations §49.3's step 5 needed, and whether they were enough.
        int depenetrations = 0;
        bool depenetrated = true;
        /// @brief The body was over nothing this step.
        bool airborne = false;
        /// @brief `Shift` was pressed and the walk mode changed. §43.2 plays a UI tick and shows
        ///        a HUD glyph on this, and the app writes the new mode to `Settings`.
        bool walkModeChanged = false;
        /// @brief Everything §43.2's modifier table did to the walk this step, as one number.
        ///        1.0 is an unmodified forward walk. §47's locomotion blend reads it.
        float speedFactor = 1.0F;
        /// @brief The body crouched or stood up this step. §47.2 blends to `crouch_*` on it.
        bool crouchChanged = false;

        /// @brief §48.2's answer for §47.2's state machine.
        enum class Stairs
        {
            /// @brief Not on a stair ramp, or on one flat enough to walk normally.
            None,
            Up,
            Down,
        };
        Stairs stairs = Stairs::None;
    };

    /// @brief §49.3's fixed step, composed (`HOUSE-00555`).
    ///
    /// ```
    /// 1. integrate desired velocity (input, gravity, wind for open doors)
    /// 2. for iteration in 0..2: sweep, move, slide
    /// 3. GroundProbe
    /// 4. StepUp
    /// 5. Depenetrate
    /// ```
    ///
    /// **Steps 2 and 4 are run together, and that is a decision.** `MoveWithStepAssist` slides and
    /// then, only if the slide was blocked, lifts and retries -- which is what step 4 says to do.
    /// Running them apart would mean sliding twice for every kerb in the house, and the step-down
    /// that keeps a body on a staircase belongs to the same move.
    ///
    /// **Gravity is not folded into the slide's motion.** §49.3's step 1 lists it with the input,
    /// but a single 3-D sweep cannot tell "walked into a wall" from "landed on a floor", and the
    /// two want different answers: one slides, the other ends a fall and fires §47.2's landing.
    /// The horizontal move and the fall are therefore separate sweeps, and `GroundProbe` is what
    /// decides which of them the next step gets.
    ///
    /// @p input is read but never written: an `IInputSource` owns edges (`HOUSE-00140`).
    PlayerStepReport PlayerStep(const physics::CollisionWorld& world,
                                const physics::CollisionCell& cell,
                                physics::BroadPhase& broad,
                                PlayerState& state,
                                const InputState& input,
                                float dt);

} // namespace cnahouse::player
