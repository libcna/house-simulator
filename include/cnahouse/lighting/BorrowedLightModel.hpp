// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "cnahouse/lighting/RoomLightState.hpp"
#include "cnahouse/visibility/PortalRuntime.hpp"

namespace cnahouse::world
{
    class WorldData;
}

namespace cnahouse::lighting
{

    /// @brief Section 28.4's bounded two-hop spill through the live portal graph.
    ///
    /// The immutable graph and all scratch storage are built once. `Evaluate` therefore only
    /// clears and writes fixed-size arrays: opening a door changes its `PortalRuntime`, not this
    /// model's topology, and lighting remains allocation-free after the world is loaded.
    class BorrowedLightModel
    {
    public:
        BorrowedLightModel(const world::WorldData& world, std::span<const visibility::PortalRuntime> portals);

        BorrowedLightModel(const BorrowedLightModel&) = delete;
        BorrowedLightModel& operator=(const BorrowedLightModel&) = delete;
        BorrowedLightModel(BorrowedLightModel&&) noexcept = default;
        BorrowedLightModel& operator=(BorrowedLightModel&&) noexcept = default;
        ~BorrowedLightModel() = default;

        /// @brief Writes one borrowed level per source cell, in world-cell order.
        ///
        /// Only local artificial light and daylight are sources. Borrowed light and the ambient
        /// floor are deliberately excluded: feeding either back would turn a two-hop flood into an
        /// unbounded recurrence and would make an entirely dark chain manufacture light.
        void Evaluate(std::span<const RoomLightState> states, std::span<float> borrowed) noexcept;

        /// @brief The same portal transfer with only direct daylight as a source.
        ///
        /// Windowless receivers have a daylight atlas, but no window of their own. This result
        /// scales that atlas without mistaking neighbouring artificial fixtures for daylight.
        void EvaluateDaylight(std::span<const RoomLightState> states, std::span<float> borrowed) noexcept;

    private:
        struct Edge
        {
            std::size_t from = 0;
            std::size_t to = 0;
            std::size_t portal = 0;
            /// @brief `portalArea / targetCellArea * 0.30`, before the live aperture.
            float scale = 0.0F;
        };

        [[nodiscard]] float Transfer(const Edge& edge) const noexcept;
        void EvaluateSources(std::span<const RoomLightState> states,
                             std::span<float> borrowed,
                             bool daylightOnly) noexcept;

        std::span<const visibility::PortalRuntime> portals_;
        std::vector<Edge> edges_;
        std::vector<float> firstHop_;
        std::vector<float> secondHop_;
    };

} // namespace cnahouse::lighting
