// SPDX-License-Identifier: MIT
#include "cnahouse/player/PlayerController.hpp"

#include <algorithm>
#include <cmath>

#include "cnahouse/physics/BroadPhase.hpp"

namespace cnahouse::player
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;
        using namespace cnahouse::physics;

        /// The input's 2-D wish turned into a world direction by the body's yaw.
        ///
        /// §14: yaw 0 looks north, which is -Z, and positive yaw turns EAST. So forward is
        /// `(sin, 0, -cos)` and right is `(cos, 0, sin)` -- the same pair `FreeFlyCamera` uses,
        /// deliberately, because two different answers to "which way is forward" is a bug that
        /// shows up as the player walking sideways.
        Xna::Vector3 Wish(const InputState& input, float yaw)
        {
            const float sinYaw = std::sin(yaw);
            const float cosYaw = std::cos(yaw);
            const Xna::Vector3 forward(sinYaw, 0.0F, -cosYaw);
            const Xna::Vector3 right(cosYaw, 0.0F, sinYaw);
            Xna::Vector3 wish(forward.X * input.move.Y + right.X * input.move.X,
                              0.0F,
                              forward.Z * input.move.Y + right.Z * input.move.X);
            // The source normalises its own stick (`KeyboardMouseSource`), but an `IInputSource`
            // is an interface and a replay or a gamepad could hand over more than one.
            const float length = std::sqrt(wish.X * wish.X + wish.Z * wish.Z);
            if (length > 1.0F)
            {
                wish = Xna::Vector3(wish.X / length, 0.0F, wish.Z / length);
            }
            return wish;
        }

    } // namespace

    PlayerStepReport PlayerStep(const CollisionWorld& world,
                                const CollisionCell& cell,
                                BroadPhase& broad,
                                PlayerState& state,
                                const InputState& input,
                                float dt)
    {
        PlayerStepReport report;
        if (dt <= 0.0F)
        {
            return report;
        }

        // 1. The desired velocity, approached at §43.2's rates rather than snapped to.
        //    Accelerating and decelerating at DIFFERENT rates is what makes the body feel like it
        //    has weight without feeling floaty: it takes 0.15 s to reach a walk and 0.10 s to stop.
        const Xna::Vector3 wish = Wish(input, state.yaw);
        const Xna::Vector3 desired(wish.X * kWalkSpeed, 0.0F, wish.Z * kWalkSpeed);
        const Xna::Vector3 gap(desired.X - state.velocity.X, 0.0F, desired.Z - state.velocity.Z);
        const float gapLength = std::sqrt(gap.X * gap.X + gap.Z * gap.Z);
        const bool wanted = wish.X != 0.0F || wish.Z != 0.0F;
        const float rate = (wanted ? kAcceleration : kDeceleration) * dt;
        if (gapLength <= rate || gapLength < 1.0e-6F)
        {
            state.velocity = desired;
        }
        else
        {
            state.velocity = Xna::Vector3(state.velocity.X + gap.X / gapLength * rate,
                                          0.0F,
                                          state.velocity.Z + gap.Z / gapLength * rate);
        }

        // 2 and 4. Slide, and lift over a kerb if the slide was blocked.
        Capsule body = state.Body();
        const StepAssist moved = MoveWithStepAssist(
            world, cell, broad, body, Xna::Vector3(state.velocity.X * dt, 0.0F, state.velocity.Z * dt));
        body.centre = moved.position;
        report.blocked = moved.slide.blocked;
        report.steppedUp = moved.steppedUp;
        report.steppedDown = moved.steppedDown;
        report.airborne = moved.airborne;

        // Walking into a wall must not leave the body still trying: a velocity that survives the
        // contact accelerates into it for as long as the key is held, and the first frame the wall
        // ends the body shoots out at full speed.
        if (moved.slide.contacts > 0)
        {
            const Xna::Vector3& n = moved.slide.lastNormal;
            const float into = state.velocity.X * n.X + state.velocity.Z * n.Z;
            if (into < 0.0F)
            {
                state.velocity =
                    Xna::Vector3(state.velocity.X - n.X * into, 0.0F, state.velocity.Z - n.Z * into);
            }
        }

        // 3. What is under it. The PROBE decides whether the body is falling, not the move: a
        //    downward sweep during the move can be answered by a wall the body is leaning on, and
        //    §49.3 makes step 3 the step that sets `onGround` for exactly that reason.
        GroundProbeResult ground = GroundProbe(world, cell, broad, body);
        if (!ground.onGround && state.fall.onGround)
        {
            state.fall.onGround = false;
            state.fall.speed = 0.0F;
            state.fall.fellFrom = body.centre.Y;
        }

        // 1b. Gravity, for a body with nothing under it. Falling is a separate sweep from walking
        //     because one 3-D sweep cannot tell "walked into a wall" from "landed on a floor", and
        //     the two want different answers.
        if (!state.fall.onGround)
        {
            const FallStep fell = Fall(world, cell, broad, body, state.fall, dt);
            body.centre = fell.position;
            state.fall = fell.state;
            report.landing = fell.landing;
            report.landingDrop = fell.drop;
            if (fell.state.onGround)
            {
                ground = GroundProbe(world, cell, broad, body); // it landed; ask again
            }
        }
        state.onGround = ground.onGround;
        state.groundNormal = ground.onGround ? ground.normal : Xna::Vector3(0.0F, 1.0F, 0.0F);
        state.surface = ground.surface;
        state.groundKind = ground.kind;
        state.cellId = ground.cellId;
        if (ground.onGround)
        {
            // The probe is the authority on standing, so a body it finds ground under is not
            // falling -- whatever the step assist thought a moment ago.
            state.fall.onGround = true;
            state.fall.speed = 0.0F;
        }

        // 5. Depenetrate, last, so that whatever the four steps above left is put right before
        //    anything reads the position.
        const Depenetration out = Depenetrate(world, cell, broad, body);
        body.centre = Xna::Vector3(
            body.centre.X + out.offset.X, body.centre.Y + out.offset.Y, body.centre.Z + out.offset.Z);
        report.depenetrations = out.iterations;
        report.depenetrated = out.resolved;

        state.position = body.centre;
        return report;
    }

} // namespace cnahouse::player
