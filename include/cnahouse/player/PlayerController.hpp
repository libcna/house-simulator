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

    /// @brief §43.2's normal walk, in m/s. The fast walk and its toggle are `HOUSE-00556`.
    inline constexpr float kWalkSpeed = 1.35F;
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
        physics::FallState fall;

        /// @brief What `GroundProbe` last said. §60 reads `groundKind`, §31 reads `surface`, and
        ///        `HOUSE-00559`'s cell tracking reads `cellId`.
        bool onGround = true;
        Microsoft::Xna::Framework::Vector3 groundNormal{0.0F, 1.0F, 0.0F};
        std::uint16_t surface = 0u;
        physics::CollisionKind groundKind = physics::CollisionKind::Floor;
        std::string_view cellId;

        /// @brief The eye, which is what §45's camera and §50.1's targeting ray start from.
        [[nodiscard]] Microsoft::Xna::Framework::Vector3 Eye() const
        {
            return Microsoft::Xna::Framework::Vector3(
                position.X, position.Y - kPlayerHalfHeight - kPlayerRadius + kPlayerEyeHeight, position.Z);
        }

        /// @brief The body, at wherever it currently is.
        [[nodiscard]] physics::Capsule Body() const
        {
            return physics::Capsule{position, kPlayerHalfHeight, kPlayerRadius};
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
