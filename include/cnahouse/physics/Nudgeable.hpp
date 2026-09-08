// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/physics/Sweep.hpp"
#include "cnahouse/util/Ids.hpp"

namespace cnahouse::physics
{
    class BroadPhase;

    /// @brief §49.4's twelve nudgeable props: *"a sphere or box with gravity, a support-plane
    ///        resolve, linear+angular damping, and a push impulse from the player capsule"*.
    ///
    /// **A capsule, not a box.** The collision layer sweeps capsules against the world and nothing
    /// else (`HOUSE-00543`, `HOUSE-00544`), and a football is a capsule with no segment while a
    /// waste bin is one with a short one. Giving twelve props their own box-versus-world sweep
    /// would double the amount of geometry code in the project to make a 0.40 m crate's corners
    /// slightly squarer, and every one of §49.4's four behaviours -- gravity, the support plane,
    /// the damping and the push -- is the same either way.
    ///
    /// **They are resolved against STATIC geometry only**, which is §49.4's own sentence and the
    /// reason the list has no sleep-fail and no stacking: two props cannot hold each other up, so
    /// a prop is either on the floor or falling, and nothing can wake it but the player.
    struct NudgeableProp
    {
        util::Id id;
        /// @brief The middle of the shape. A sphere's centre, a bin's mid-height.
        Microsoft::Xna::Framework::Vector3 position;
        /// @brief Half the capsule's segment. 0 for a ball.
        float halfHeight = 0.0F;
        float radius = 0.12F;
        /// @brief Kilograms. §49.4 gives no numbers; what mass does here is divide the push, so a
        ///        football (0.43 kg) shoots away and a crate (12 kg) shifts.
        float mass = 1.0F;
        Microsoft::Xna::Framework::Vector3 velocity;
        /// @brief Radians about +Y, and the rate it turns at. §49.2 allows a static shape one
        ///        rotation and this is the moving equivalent: a prop that spins as it rolls, and
        ///        never tips over, because a tipped capsule is not a capsule any more.
        float yaw = 0.0F;
        float spin = 0.0F;
        /// @brief Asleep on a support plane. Nothing but a push can clear it.
        bool resting = false;

        [[nodiscard]] Capsule Body() const
        {
            return Capsule{position, halfHeight, radius};
        }
    };

    /// @brief Metres per second squared, and the speed a prop stops accelerating at. The same
    ///        9.81 the player falls at -- one house, one gravity.
    inline constexpr float kPropGravity = 9.81F;
    inline constexpr float kPropTerminalSpeed = 12.0F;

    /// @brief Linear damping in the air, per second. Small: a thrown ball keeps its speed.
    inline constexpr float kPropAirDamping = 0.35F;
    /// @brief ...and on the ground, where it is friction and much larger. A football shoved at
    ///        2 m/s crosses about 1.4 m, which is what a kicked ball does across a kitchen.
    inline constexpr float kPropGroundDamping = 1.4F;
    /// @brief Angular damping, per second. A prop that stops sliding stops turning shortly after.
    inline constexpr float kPropSpinDamping = 2.0F;

    /// @brief Below this speed on a support plane a prop goes to sleep, in m/s and rad/s.
    ///
    /// **Sleep is a state and not an optimisation.** §49.4 says these never "sleep-fail", and the
    /// failure it means is a prop that jitters for ever on a floor because gravity puts a
    /// millimetre of velocity into it every step and the resolve takes it out again. A prop that
    /// is slow AND supported is asleep, and nothing but a push wakes it.
    inline constexpr float kPropSleepSpeed = 0.05F;
    inline constexpr float kPropSleepSpin = 0.20F;

    /// @brief How much of the player's speed a push transfers, for a 1 kg prop.
    ///
    /// A shove is not a kick: §49.4 says the player NUDGES these. At walking pace the football
    /// leaves at about a metre a second and a 12 kg crate barely shifts, which is the point of
    /// dividing by the mass.
    inline constexpr float kPushTransfer = 0.9F;
    /// @brief ...and the most a push can impart, whatever the mass. A body walking into a ball
    ///        does not launch it across the house.
    inline constexpr float kMaxPushSpeed = 2.5F;
    /// @brief The spin a push imparts, per metre a second of it, when the shove is off-centre.
    inline constexpr float kPushSpin = 1.5F;

    /// @brief What one step did to a prop.
    struct NudgeReport
    {
        /// @brief It is standing on something §43.1 would call a floor.
        bool supported = false;
        bool resting = false;
        /// @brief It met something it could not slide along and lost the rest of its step.
        bool blocked = false;
        /// @brief Metres it moved.
        float travelled = 0.0F;
        /// @brief Iterations §49.3's step 5 needed to get it out of the geometry, 0 when clear.
        int depenetrations = 0;
    };

    /// @brief One fixed step of §49.4's mini-physics for @p prop, against static geometry only.
    NudgeReport NudgeStep(const CollisionWorld& world,
                          const CollisionCell& cell,
                          BroadPhase& broad,
                          NudgeableProp& prop,
                          float dt);

    /// @brief The push a player's capsule gives @p prop when the two overlap.
    ///
    /// @param player where the player's body is, @param playerVelocity how fast it is going.
    /// @return true when the prop was touched, whether or not it moved.
    ///
    /// The impulse is along the horizontal part of the contact normal, so a body walking over a
    /// ball does not fire it into the floor; the vertical component of a push is what would let a
    /// player launch props by jumping on them, and there is no jump in this game anyway (§43.1).
    bool PushProp(NudgeableProp& prop,
                  const Capsule& player,
                  const Microsoft::Xna::Framework::Vector3& playerVelocity);

} // namespace cnahouse::physics
