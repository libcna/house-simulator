// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string_view>

#include "cnahouse/app/FrameTimer.hpp"

namespace cnahouse::app
{

    /// @brief The twelve update stages of `cna-house.md` §7.5, in the order they run.
    ///
    /// **The order is fixed and it is data, not convention.** Each stage reads state the ones before
    /// it produced: interaction opens a door, the door animates its aperture, the aperture changes
    /// which portals are open, visibility walks those portals. Reorder two and the frame is one frame
    /// stale in a way that shows up as a door you can see through before it has opened.
    ///
    /// Declaring the order as an enum rather than as the sequence of calls in some `Update` function
    /// means it can be asserted, timed per stage, and printed in the debug overlay -- and means a new
    /// system has to be *placed* rather than appended.
    enum class UpdateStage : std::uint8_t
    {
        Input = 0,   ///< PlayerInput, InteractionIntent
        Clock,       ///< simulated time, sun/moon vectors, sky state
        Weather,     ///< weather state, transition, precipitation spawn budget
        Interaction, ///< apply queued actions, mutate interactable states
        Apertures,   ///< doors and windows animate; portal aperture changes are published
        Lighting,    ///< per-room artificial and daylight levels, dominant lights
        Physics,     ///< player, pets, camera collision -- the only fixed-step stage
        Animation,   ///< ClipPlayer evaluation, bone palettes
        Pets,        ///< behaviour, navigation
        Audio,       ///< listener, emitters, portal-path gains, voice manager
        Visibility,  ///< camera cell, portal traversal, visible cell set and frusta
        Residency,   ///< asset residency requests
        Count,
    };

    [[nodiscard]] std::string_view UpdateStageName(UpdateStage stage) noexcept;

    /// @brief A system: something that runs once per frame at a known stage.
    ///
    /// Systems do not know about each other. They read and write state held by the objects the service
    /// container owns, and they communicate through the event queue -- never by calling each other,
    /// because a call is an ordering constraint that does not appear in `UpdateStage` and therefore
    /// cannot be reasoned about.
    class ISystem
    {
    public:
        virtual ~ISystem() = default;

        /// @brief Which stage this system runs in. Fixed for the system's lifetime.
        [[nodiscard]] virtual UpdateStage Stage() const noexcept = 0;

        /// @brief The name used in logs, the debug overlay and the per-stage timing breakdown.
        [[nodiscard]] virtual std::string_view Name() const noexcept = 0;

        /// @brief Runs one frame's worth of work.
        ///
        /// The `Physics` stage is the exception to "one frame's worth": it runs `frame.fixedSteps`
        /// times per frame, which is why the count is on `FrameContext` rather than hidden in a timer
        /// only the physics system can see.
        virtual void Update(const FrameContext& frame) = 0;
    };

} // namespace cnahouse::app
