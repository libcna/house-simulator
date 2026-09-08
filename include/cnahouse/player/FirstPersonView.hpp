// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/player/EyeProbe.hpp"
#include "cnahouse/player/EyeSpring.hpp"
#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/player/HeadBob.hpp"
#include "cnahouse/player/LandingDip.hpp"
#include "cnahouse/player/PlayerController.hpp"

namespace cnahouse::player
{

    /// @brief §44's view, assembled: the camera and the three things that move the eye inside it
    ///        (`HOUSE-00632`).
    ///
    /// **One assembly, because the order is a decision and not plumbing.** The eye height is
    /// sprung (`HOUSE-00561`), the bob is added AFTER the spring (`HOUSE-00627`), the landing dip
    /// is a third term on the same height (`HOUSE-00629`), and §44's near-surface pull-back
    /// happens LAST because it needs the view direction the rest of it produces
    /// (`HOUSE-00628`). Every caller that wanted a first-person view -- the tune pass, the render
    /// poses, the game -- would otherwise write that order out again, and the second one to write
    /// it would get it slightly different.
    ///
    /// **The spring tracks the eye's WORLD height, not its height above the feet.** Above the feet
    /// it is a constant -- §43.1's 1.68 m -- and a spring chasing a constant does nothing at all.
    /// What §44 asks for is that a step up does not jolt the view, and a step up moves the FEET:
    /// the eye is left behind in world space and catches up. The camera is then handed the
    /// difference, because that is what it takes.
    class FirstPersonView
    {
    public:
        /// @brief Puts the eye exactly where the body is, with nothing in flight.
        ///
        /// A spawn, a teleport (§57) and a save load: the distance between one frame and the next
        /// is not a walk, and everything here that integrates one -- the spring, the bob's
        /// cadence, the dip -- has to be told so.
        void Snap(const PlayerState& state, float pitch = 0.0F);

        /// @brief One frame: §49.3's step has already run and this is what it did.
        ///
        /// @param report the step's own account of itself -- the stairs flag stiffens the spring
        ///        (§48.2) and the landing drives the dip (§43.1).
        /// @param pitch §44's look pitch, from `ApplyLook` (`HOUSE-00622`).
        void Update(const PlayerState& state, const PlayerStepReport& report, float pitch, float dt);

        /// @brief §44's camera collision, which has to come after `Update` and needs the world.
        ///
        /// Separate rather than folded in, because it is the one part of the view that asks the
        /// collision world a question, and a caller that has no world -- a unit test of the bob,
        /// a tool drawing a pose -- must still be able to have a view.
        void ProbeSurfaces(const physics::CollisionWorld& world,
                           const physics::CollisionCell& cell,
                           physics::BroadPhase& broad);

        [[nodiscard]] const FirstPersonCamera& Camera() const noexcept
        {
            return camera_;
        }

        [[nodiscard]] FirstPersonCamera& Camera() noexcept
        {
            return camera_;
        }

        [[nodiscard]] HeadBob& Bob() noexcept
        {
            return bob_;
        }

        [[nodiscard]] const HeadBob& Bob() const noexcept
        {
            return bob_;
        }

        /// @brief The eye's world height as the spring has it, before the bob and the dip.
        [[nodiscard]] float SprungHeight() const noexcept
        {
            return spring_.Height();
        }

        /// @brief What the spring is chasing: the feet plus §43.1's eye height.
        [[nodiscard]] float TargetHeight() const noexcept
        {
            return target_;
        }

        [[nodiscard]] const HeadBobOffset& BobOffset() const noexcept
        {
            return bobOffset_;
        }

        [[nodiscard]] float DipOffset() const noexcept
        {
            return dip_.Offset();
        }

        [[nodiscard]] const EyeClearance& Clearance() const noexcept
        {
            return clearance_;
        }

    private:
        FirstPersonCamera camera_;
        EyeSpring spring_;
        HeadBob bob_;
        LandingDip dip_;
        HeadBobOffset bobOffset_;
        EyeClearance clearance_;
        Microsoft::Xna::Framework::Vector3 previous_;
        float target_ = 0.0F;
        bool started_ = false;
    };

} // namespace cnahouse::player
