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

        /// §43.2's modifier table, as one number.
        float SpeedFactor(const InputState& input, const PlayerState& state)
        {
            // The DIRECTIONAL part first, as the ellipse whose axes are the table's own numbers.
            // Multiplying the two instead would make a backwards strafe 0.61 of a walk -- slower
            // than either of the things it is a mixture of, which is not what a mixture means.
            float forward = input.move.Y;
            float lateral = input.move.X;
            const float length = std::sqrt(forward * forward + lateral * lateral);
            float factor = 1.0F;
            if (length > 1.0e-6F)
            {
                forward /= length;
                lateral /= length;
                const float alongLimit = forward >= 0.0F ? 1.0F : kBackwardsFactor;
                const float along = forward / alongLimit;
                const float across = lateral / kStrafeFactor;
                factor = 1.0F / std::sqrt(along * along + across * across);
            }

            // The STATE part multiplies: being on a stair, crouched, carrying something and in
            // deep snow are four independent facts, and a body doing all four is slowed by all
            // four.
            if (state.groundKind == CollisionKind::Stair)
            {
                factor *= kStairsFactor;
            }
            if (state.crouched)
            {
                factor *= kCrouchFactor;
            }
            if (state.carrying)
            {
                factor *= kCarryingFactor;
            }
            if (state.snowDepth > kDeepSnowDepth)
            {
                factor *= kDeepSnowFactor;
            }
            return factor;
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
        // §43.2's toggle, taken from the EDGE. A level would flip the mode on every tick the key
        // is held, which is 120 flips a second and reads as neither mode.
        if (input.runPressed)
        {
            state.fastWalk = !state.fastWalk;
            report.walkModeChanged = true;
        }
        const float speed = (state.fastWalk ? kFastWalkSpeed : kWalkSpeed) * SpeedFactor(input, state);

        const Xna::Vector3 wish = Wish(input, state.yaw);
        report.speedFactor = speed / (state.fastWalk ? kFastWalkSpeed : kWalkSpeed);
        const Xna::Vector3 desired(wish.X * speed, 0.0F, wish.Z * speed);
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

        // §43.1's attic crouch, decided before the move and never asked for by the player.
        //
        // Two questions, in this order. Does the STANDING body still fit where it is? If not,
        // crouch -- an attic's rafters come down over a body that walked in upright, and a body
        // that only checked on the way in would end up inside them. And if it is crouched, does
        // the standing body fit again? If so, stand -- but only then, or the swap puts the head
        // through the roof.
        //
        // The feet stay put through both: the body shrinks towards the floor, so the centre drops
        // by exactly what the half-height loses.
        {
            const Xna::Vector3 feet = state.Feet();
            const bool wasCrouched = state.crouched;
            const Capsule standing{Xna::Vector3(feet.X, feet.Y + kPlayerHalfHeight + kPlayerRadius, feet.Z),
                                   kPlayerHalfHeight,
                                   kPlayerRadius};
            // TOUCHING is not a reason to crouch. A body resting against a wall is touching it --
            // that is what resting is -- and a fit test that counted contact as "does not fit"
            // crouched the player at every wall they leaned on (`kContactTolerance`, the same
            // distinction the depenetration needed).
            const bool standingFits =
                OverlapCell(world, cell, broad, standing).depth <= physics::kContactTolerance;
            state.crouched = !standingFits;
            if (state.crouched != wasCrouched)
            {
                report.crouchChanged = true;
                state.position = Xna::Vector3(feet.X, feet.Y + state.Rise(), feet.Z);
            }
        }

        // 2 and 4. Slide, and lift over a kerb if the slide was blocked.
        Capsule body = state.Body();
        StepAssist moved = MoveWithStepAssist(
            world, cell, broad, body, Xna::Vector3(state.velocity.X * dt, 0.0F, state.velocity.Z * dt));
        // Blocked by something the body would fit under? A low HEADER over a doorway is the case:
        // the room it is standing in is 2.4 m and the opening is 1.4 m, so the standing test a few
        // lines up sees nothing wrong -- the body does fit where it IS -- and what stops it is a
        // vertical face entirely above its waist, whose normal is horizontal like any wall's.
        //
        // So the question is not what the contact's normal was. It is whether crouching gets the
        // body further, asked by trying it, once -- exactly as the kerb assist lifts and retries.
        // (A sloping rafter needs none of this: it comes down over the body's own feet first, and
        // the standing test catches it there.)
        if (moved.slide.contacts > 0 && !state.crouched)
        {
            const Xna::Vector3 feet = state.Feet();
            PlayerState lowered = state;
            lowered.crouched = true;
            lowered.position = Xna::Vector3(feet.X, feet.Y + lowered.Rise(), feet.Z);
            const Capsule small = lowered.Body();
            if (OverlapCell(world, cell, broad, small).depth <= physics::kContactTolerance)
            {
                const StepAssist under =
                    MoveWithStepAssist(world,
                                       cell,
                                       broad,
                                       small,
                                       Xna::Vector3(state.velocity.X * dt, 0.0F, state.velocity.Z * dt));
                const float was =
                    std::sqrt((moved.position.X - body.centre.X) * (moved.position.X - body.centre.X) +
                              (moved.position.Z - body.centre.Z) * (moved.position.Z - body.centre.Z));
                const float now =
                    std::sqrt((under.position.X - small.centre.X) * (under.position.X - small.centre.X) +
                              (under.position.Z - small.centre.Z) * (under.position.Z - small.centre.Z));
                if (now > was + kMotionEpsilon)
                {
                    state.crouched = true;
                    report.crouchChanged = true;
                    body = small;
                    moved = under;
                }
            }
        }

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

        // §48.2's slope, measured ALONG the direction of travel. The gradient of a plane with
        // normal `n` in the horizontal direction `d` is `-(n.x·d.x + n.z·d.z) / n.y`; its arctangent
        // is the angle, positive uphill.
        //
        // Along the TRAVEL and not down the surface's steepest line, because a body crossing a
        // half-landing or walking along a ramp's contour is not climbing anything, and §48.2 gives
        // that case the in-place turn clips rather than `stair_up`.
        {
            const float horizontal =
                std::sqrt(state.velocity.X * state.velocity.X + state.velocity.Z * state.velocity.Z);
            state.groundSlopeDeg = 0.0F;
            state.alongSlopeSpeed = horizontal;
            if (horizontal > 1.0e-4F && state.groundNormal.Y > 1.0e-4F)
            {
                const float dx = state.velocity.X / horizontal;
                const float dz = state.velocity.Z / horizontal;
                const float gradient =
                    -(state.groundNormal.X * dx + state.groundNormal.Z * dz) / state.groundNormal.Y;
                state.groundSlopeDeg = std::atan(gradient) * 180.0F / 3.14159265F;
                // The hypotenuse: a body covering `horizontal` metres across the map covers
                // `horizontal · √(1 + g²)` along the ground it is walking on.
                state.alongSlopeSpeed = horizontal * std::sqrt(1.0F + gradient * gradient);
            }
            if (state.OnStairs() && std::fabs(state.groundSlopeDeg) > kStairAnimationSlopeDegrees)
            {
                report.stairs = state.groundSlopeDeg > 0.0F ? PlayerStepReport::Stairs::Up
                                                            : PlayerStepReport::Stairs::Down;
            }
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
