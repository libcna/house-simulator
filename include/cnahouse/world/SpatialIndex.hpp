// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "cnahouse/world/WorldData.hpp"

/// @file
/// `SpatialIndex` -- point to cell, on every physics step (`HOUSE-00356`, `cna-house.md` §16.4).

namespace cnahouse::world
{

    /// @brief §16.4's point→cell lookup: an incremental test, a neighbour walk, and a grid.
    ///
    /// The query runs on every physics step and every camera move, so the three steps are ordered
    /// by how often each one answers:
    ///
    /// 1. **Incremental.** Test the point against the cell the caller was in last, expanded by
    ///    5 cm. This is the answer 99.9 % of the time and costs one loop over a handful of boxes.
    /// 2. **Neighbour walk.** Else test the cells reachable through that cell's portals. A player
    ///    who left a room is almost always in the room next door.
    /// 3. **Grid.** Else look the point up in a uniform 2 m grid built at load. This is also the
    ///    entry point for spawning, teleporting and loading a save, where there is no last cell.
    ///
    /// **The hysteresis is not a rounding tolerance.** Without it a player standing exactly on a
    /// boundary flips between two cells every frame, and audio, residency and visibility churn with
    /// them. It applies to step 1 only: staying put is sticky, arriving is not, and a margin on the
    /// grid step would make two cells claim the same point with no way to choose.
    ///
    /// **There is no fourth step here.** §16.4's step 4 assigns `EXT_WORLD` to a point in no cell,
    /// and that is a room name — the one thing `CLAUDE.md` §3 says C++ must never contain. `Find`
    /// returns an invalid id and the controller, which already owns the "clamp to the last good
    /// cell" diagnostic, decides.
    class SpatialIndex
    {
    public:
        /// §16.4's grid pitch, in metres.
        static constexpr float kCellSize = 2.0F;

        /// §16.4's hysteresis on the incremental test, in metres.
        static constexpr float kHysteresis = 0.05F;

        /// @brief Buckets every cell of @p world by the 2 m squares its footprint covers.
        [[nodiscard]] static SpatialIndex Build(const WorldData& world);

        /// @brief Which of §16.4's steps produced an answer.
        ///
        /// Steps 2 and 3 give the *same* answer -- the neighbour walk is an optimisation, not a
        /// different rule -- so nothing about the returned id can show that the walk ran. Reporting
        /// the step is what makes "the walk answers before the grid does" a claim a test can make,
        /// and it is what §70.6's performance scenarios and the debug overlay want to read anyway.
        enum class Step : std::uint8_t
        {
            /// The cell the caller was already in, within the hysteresis. 99.9 % of queries.
            Incremental,
            /// A cell through one of that cell's portals.
            Neighbour,
            /// The 2 m grid. Also every spawn, teleport and save load.
            Grid,
            /// No cell contains the point.
            None,
        };

        /// @brief The cell containing @p point, or an invalid id.
        ///
        /// @param current the cell the caller was in last, for steps 1 and 2. An invalid id skips
        ///        straight to the grid, which is what a spawn or a save load wants.
        /// @param answeredBy optional: which step answered.
        [[nodiscard]] util::Id Find(const WorldData& world,
                                    const Microsoft::Xna::Framework::Vector3& point,
                                    util::Id current = {},
                                    Step* answeredBy = nullptr) const;

        /// @brief The cell indices whose footprint touches the 2 m square containing `(x, z)`.
        [[nodiscard]] std::span<const std::uint32_t> Bucket(float x, float z) const noexcept;

        [[nodiscard]] std::size_t BucketCount() const noexcept;

        /// @brief The most cells in any one bucket. §16.4 expects ≤ 6.
        [[nodiscard]] std::size_t LargestBucket() const noexcept;

        /// @brief The bucket coordinate of a world coordinate. Exposed for tests and diagnostics.
        [[nodiscard]] static std::int32_t Coordinate(float value) noexcept;

    private:
        [[nodiscard]] static std::uint64_t KeyOf(std::int32_t x, std::int32_t z) noexcept;

        std::unordered_map<std::uint64_t, detail::CellRange> m_buckets;
        std::vector<std::uint32_t> m_flat;
        std::size_t m_largest = 0;
    };

} // namespace cnahouse::world
