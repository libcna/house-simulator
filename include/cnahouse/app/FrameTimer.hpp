// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace cnahouse::app
{

    /// @brief Everything a system needs to know about the frame it is being updated for.
    ///
    /// Passed by const reference to every `ISystem::Update`, so a system never reaches for a global
    /// clock and a test can drive one deterministically by constructing this directly.
    struct FrameContext
    {
        /// @brief Seconds since the previous frame, unclamped. For diagnostics only.
        float realDeltaSeconds = 0.0f;
        /// @brief Seconds since the previous frame, clamped. **This is what systems integrate.**
        float deltaSeconds = 0.0f;
        /// @brief Seconds since the session began, accumulated from the clamped delta.
        double totalSeconds = 0.0;
        std::uint64_t frameIndex = 0;
        /// @brief How many fixed steps the physics/animation systems should run this frame.
        int fixedSteps = 0;
        /// @brief The duration of one fixed step, in seconds.
        float fixedStepSeconds = 0.0f;
    };

    /// @brief Turns wall-clock time into a `FrameContext`, with the two protections a game needs.
    ///
    /// **Clamping.** A 250 ms hitch — a texture upload, a pack load, the window manager — must not be
    /// integrated as 250 ms, or the player teleports through a wall and the dog walks across the room.
    /// The delta is clamped, which means the simulation runs slow for one frame rather than wrong.
    ///
    /// **A bounded accumulator.** A fixed-step accumulator that runs as many steps as the elapsed time
    /// asks for is the classic spiral of death: a frame that took too long asks for more steps, which
    /// take longer, which asks for more steps. `kMaxFixedSteps` bounds it, and the residue is discarded
    /// rather than carried, because carrying it is what makes the spiral compound.
    ///
    /// `HOUSE-00139`'s acceptance names the case exactly: a 250 ms hitch must clamp to 4 substeps and
    /// must not spiral.
    class FrameTimer
    {
    public:
        /// @brief The longest delta that will ever be integrated. 100 ms = 10 fps.
        static constexpr float kMaxDeltaSeconds = 0.100f;
        /// @brief The fixed step: **120 Hz**, which is what `cna-house.md` §49.3 and §7's pipeline
        /// both name — "fixed step dt = 1/120 s, accumulated from GameTime, max 4 steps per frame".
        ///
        /// `HOUSE-00139` implemented 60 and `HOUSE-00549` corrected it. The rate is not a taste:
        /// §49.3's sweep does three collide-and-slide iterations per step, and a player at 6 m/s
        /// covers 100 mm in a 60 Hz step against a 0.62 m capsule — enough to tunnel a doorway
        /// jamb on a diagonal. At 120 Hz it is 50 mm.
        static constexpr float kFixedStepSeconds = 1.0f / 120.0f;
        /// @brief The most fixed steps one frame may run. §49.3's own number: 4 steps = 33 ms of
        /// simulation, so a frame slower than 30 FPS runs the simulation slow rather than long.
        static constexpr int kMaxFixedSteps = 4;

        /// @brief Produces the context for a frame that took @p realDeltaSeconds of wall clock.
        ///
        /// Taking the measured delta as a parameter rather than reading a clock is what makes the
        /// hitch behaviour unit-testable without sleeping.
        [[nodiscard]] FrameContext Advance(float realDeltaSeconds) noexcept;

        /// @brief How many fixed steps have been dropped by the cap since the session began.
        ///
        /// Not a curiosity: a non-zero value means the simulation is running slower than real time,
        /// which is exactly the thing a player reports as "it feels sluggish" and which is otherwise
        /// invisible. The debug overlay shows it.
        [[nodiscard]] std::uint64_t DroppedFixedSteps() const noexcept
        {
            return droppedSteps_;
        }

        void Reset() noexcept;

    private:
        double totalSeconds_ = 0.0;
        std::uint64_t frameIndex_ = 0;
        float accumulator_ = 0.0f;
        std::uint64_t droppedSteps_ = 0;
    };

} // namespace cnahouse::app
