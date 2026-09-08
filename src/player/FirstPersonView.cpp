// SPDX-License-Identifier: MIT
#include "cnahouse/player/FirstPersonView.hpp"

namespace cnahouse::player
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        /// §43.1's eye above the soles: 1.68 m standing, 1.15 m crouched.
        float EyeHeightOf(const PlayerState& state)
        {
            return state.crouched ? kPlayerCrouchEyeHeight : kPlayerEyeHeight;
        }
    } // namespace

    void FirstPersonView::Snap(const PlayerState& state, float pitch)
    {
        previous_ = state.position;
        started_ = true;
        target_ = state.Feet().Y + EyeHeightOf(state);
        spring_.Snap(target_);
        bob_.Reset();
        dip_.Reset();
        bobOffset_ = HeadBobOffset{};
        clearance_ = EyeClearance{};
        camera_.Update(state, EyeHeightOf(state), pitch);
    }

    void
    FirstPersonView::Update(const PlayerState& state, const PlayerStepReport& report, float pitch, float dt)
    {
        if (!started_)
        {
            // A view that has never been snapped has no previous position, and the first frame's
            // "travel" would be the whole distance from the origin to wherever the body is.
            Snap(state, pitch);
            return;
        }

        const Xna::Vector3 travel(
            state.position.X - previous_.X, state.position.Y - previous_.Y, state.position.Z - previous_.Z);
        previous_ = state.position;

        // §48.2 stiffens the spring on a flight, *"so the view rises steadily rather than bobbing
        // per step"*. The step's own account of itself is what says so -- `PlayerStepReport` --
        // rather than the ground kind, because a body that has just left the top tread is still
        // climbing for as long as the spring is catching up.
        const bool stairs = report.stairs != PlayerStepReport::Stairs::None || state.OnStairs();
        target_ = state.Feet().Y + EyeHeightOf(state);
        const float sprung = spring_.Update(target_, dt, stairs);

        bobOffset_ = bob_.Update(travel, dt, state.onGround, state.fastWalk);

        if (report.landing != physics::Landing::None)
        {
            dip_.Land(report.landingDrop);
        }
        dip_.Update(dt);

        // The three terms, in the order the sections put them: the spring's WORLD height turned
        // back into a height above the feet, plus §44's bob, plus §43.1's landing dip. The bob and
        // the dip are added after the spring on purpose -- through it they would be a 1.8 Hz
        // wobble and a 62 ms impulse fed into a filter built to remove exactly those.
        const float aboveFeet = sprung - state.Feet().Y + dip_.Offset();
        camera_.Update(state, aboveFeet, pitch, bobOffset_);
        clearance_ = EyeClearance{};
    }

    void FirstPersonView::ProbeSurfaces(const physics::CollisionWorld& world,
                                        const physics::CollisionCell& cell,
                                        physics::BroadPhase& broad)
    {
        clearance_ = ProbeAhead(world, cell, broad, camera_.Pose());
        camera_.ApplyClearance(clearance_);
    }

} // namespace cnahouse::player
