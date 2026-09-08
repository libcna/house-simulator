// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"

#include "cnahouse/physics/CollisionData.hpp"

namespace cnahouse::physics
{

    /// @brief The per-cell loose 1 m grid, asked what a swept box might hit (`HOUSE-00542`).
    ///
    /// `cna-house.md` §49.2: *"a per-cell loose grid (1 m) indexes them, so a capsule sweep tests
    /// ~6 shapes, not 900."* `CollisionLoader` reads the grid; this is the query over it, and it is
    /// the only thing between a sweep and every shape in the room.
    ///
    /// **Reused, not constructed per query.** A sweep runs several times a frame per body, and a
    /// broad phase that allocated a result vector each time would cost more than the narrow phase
    /// it feeds. The results and the de-duplication stamp live here and are reused; `Query` returns
    /// a reference into this object that the next `Query` invalidates, which is what a caller
    /// wants and what the comment on it says.
    ///
    /// **A shape spans several buckets and is returned once.** `docs/collision-format.md` §3.4: a
    /// shape is listed in every bucket its AABB overlaps, so a floor slab is in all of them. A
    /// broad phase that returned it once per bucket would hand the narrow phase the same slab
    /// nine times and call it nine tests.
    class BroadPhase
    {
    public:
        /// @brief GLOBAL shape indices whose bucket the query box overlaps, each once.
        ///
        /// The reference is valid until the next call on this object. Global, not local: the
        /// caller is about to index `CollisionWorld::obbs` and `meshes` with it, and a local index
        /// is meaningless outside the cell it came from.
        ///
        /// A box that misses the grid entirely, or is inverted, returns an empty list rather than
        /// clamping to the nearest bucket -- a query outside a cell has no candidates in it, and
        /// answering with the nearest ones is how a body gets stopped by a wall in another room.
        [[nodiscard]] const std::vector<std::uint32_t>&
        Query(const CollisionCell& cell, const Microsoft::Xna::Framework::BoundingBox& box);

        /// @brief Buckets the last `Query` looked in. What the grid saved is this against `nx*nz`.
        [[nodiscard]] std::size_t BucketsVisited() const noexcept
        {
            return bucketsVisited_;
        }

        /// @brief Entries the last `Query` walked, before de-duplication.
        [[nodiscard]] std::size_t EntriesWalked() const noexcept
        {
            return entriesWalked_;
        }

    private:
        std::vector<std::uint32_t> results_;
        /// Per LOCAL index, the serial of the query that last added it. A stamp rather than a
        /// clear, so a query costs nothing for the shapes it does not touch.
        std::vector<std::uint32_t> stamp_;
        std::uint32_t serial_ = 0u;
        std::size_t bucketsVisited_ = 0u;
        std::size_t entriesWalked_ = 0u;
    };

} // namespace cnahouse::physics
