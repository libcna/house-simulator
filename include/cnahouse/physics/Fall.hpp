// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/physics/Move.hpp"

namespace cnahouse::physics
{

    /// @brief §43.1's gravity, in m/s².
    ///
    /// *"used only for stepping down and the short fall onto the terrace"* -- there is no jump in
    /// this game and nothing to fall off that is more than a storey high. That is why the whole of
    /// falling is this file and not a rigid-body integrator.
    inline constexpr float kGravity = 9.81F;

    /// @brief §43.1's terminal fall speed, in m/s.
    ///
    /// Reached after 1.22 s of falling, which is 7.3 m -- further than anything in the house. It
    /// earns its place anyway: it is what bounds the distance ONE fixed step can move a body to
    /// 0.10 m, and a step that cannot move a body further than 0.10 m cannot put it through a
    /// 0.15 m floor slab however the sweep behaves.
    inline constexpr float kTerminalFallSpeed = 12.0F;

    /// @brief §43.1: a fall further than this lands hard, in metres.
    inline constexpr float kHardLandingDrop = 2.4F;

    /// @brief What §47.2's `land_soft` / `land_hard` one-shot is chosen by.
    enum class Landing
    {
        /// @brief The body did not land this step: it is still on the ground, or still falling.
        None,
        Soft,
        Hard,
    };

    /// @brief A body's vertical state, carried from one fixed step to the next.
    struct FallState
    {
        /// @brief Downward speed in m/s, always ≥ 0. A body on the ground has none.
        float speed = 0.0F;
        /// @brief Whether the body is resting on something §43.1 says it can stand on.
        bool onGround = true;
        /// @brief The height the current fall STARTED at. Meaningless while `onGround`.
        ///
        /// The drop is measured from here and not from the top of the step, because a fall lasts
        /// eighty-four steps and each one of them is 15 mm.
        float fellFrom = 0.0F;
    };

    /// @brief What one step of falling did.
    struct FallStep
    {
        Microsoft::Xna::Framework::Vector3 position;
        FallState state;
        /// @brief `Soft` or `Hard` on the step the fall ENDED, `None` on every other.
        Landing landing = Landing::None;
        /// @brief How far the fall that just ended was, in metres. Zero unless it ended.
        float drop = 0.0F;
        /// @brief Whether the step was stopped by something too steep to land on (§43.1's 46°).
        ///        The body is at the contact and still falling: a slope sheds it sideways next
        ///        step, which is what a slope is for.
        bool slid = false;
    };

    /// @brief §49.3 step 1's gravity term, integrated over one fixed step of @p dt seconds.
    ///
    /// **Semi-implicit Euler, and the order matters.** The speed is advanced first and the body
    /// moved at the new speed, which is what makes the fall converge on the analytic answer from
    /// ABOVE rather than below -- a body that falls slightly too fast lands slightly early, and a
    /// body that falls slightly too slowly hangs. At §49.3's fixed 1/120 s the difference is
    /// 0.7 mm over a two-metre fall, and it is the same 0.7 mm every time, which is what §49.3's
    /// determinism clause asks for.
    ///
    /// @p state.onGround bodies are not integrated: a body standing still stays exactly still
    /// rather than accumulating a downward speed it can never spend. Taking it OFF the ground is
    /// the caller's business -- `MoveWithStepAssist` reports `airborne` when it walks a body off
    /// an edge, and that is the moment to clear the flag.
    [[nodiscard]] FallStep Fall(const CollisionWorld& world,
                                const CollisionCell& cell,
                                class BroadPhase& broad,
                                const Capsule& capsule,
                                const FallState& state,
                                float dt);

} // namespace cnahouse::physics
