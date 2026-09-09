// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/visibility/PortalTraversal.hpp"

namespace cnahouse::world
{
    struct ChunkLibrary;
    class WorldData;
} // namespace cnahouse::world

namespace cnahouse::visibility
{

    /// @brief §25.1's step 3 for static geometry: *"per-cell chunk/instance AABB vs. frusta"*
    ///        (`HOUSE-00672`).
    ///
    /// **The cell being visible is not the same as its geometry being visible.** A room seen
    /// through a doorway contributes the wall opposite the door and nothing else; §17.4 splits a
    /// cell into chunks by material precisely so that this test can throw most of it away. Without
    /// this step the portal walk saves draw calls only at the room boundary, which is where the
    /// least of the cost is.
    ///
    /// **Each chunk is tested against the cell's OWN cones**, of which there are up to four
    /// (`kMaxFrustaPerCell`), and one hit is enough: the cones are alternative views of the same
    /// room, so a chunk in any of them is on screen.
    class ChunkCuller
    {
    public:
        /// @brief Indexes @p library by cell id once, so the per-frame work is arithmetic.
        ///
        /// The library keys cells by NAME and the visible set carries `util::Id`s; hashing a
        /// string per cell per frame would be a lookup in the middle of the hot loop for a mapping
        /// that never changes.
        ///
        /// @param world optional, and what it buys is §12's nested cells (`HOUSE-00488`): a
        ///        container's shell and the garage loft's underside are geometry that stands
        ///        inside another cell's volume and belongs to a chunk of their own. Without the
        ///        world this culler cannot know that, and the fridge disappears from the kitchen
        ///        the moment its door is shut.
        explicit ChunkCuller(const world::ChunkLibrary& library, const world::WorldData* world = nullptr);

        struct Stats
        {
            int cellsTested = 0;
            int chunksTested = 0;
            int chunksDrawn = 0;
            /// @brief Chunks inside a VISIBLE cell that no cone of that cell could see. This is
            ///        the number that says whether the step is worth its cost.
            int chunksCulled = 0;
            /// @brief Chunks drawn because a cell NESTED in a visible one owns them: the front of
            ///        the refrigerator, the underside of the garage loft (`HOUSE-00488`).
            int chunksFromNested = 0;
        };

        void Cull(std::span<const VisibleCell> visible);

        /// @brief Indices into `ChunkLibrary::chunks`, in cell order then file order.
        [[nodiscard]] std::span<const std::uint32_t> Chunks() const noexcept
        {
            return drawn_;
        }

        [[nodiscard]] const Stats& Statistics() const noexcept
        {
            return stats_;
        }

    private:
        /// @brief One cell's chunks against one cell's cones. `nested` only for the counter.
        void CullCell(const VisibleCell& cell, util::Id owner, bool nested);

        const world::ChunkLibrary* library_ = nullptr;
        const world::WorldData* world_ = nullptr;
        /// @brief Cell id -> the chunk indices belonging to it.
        std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> byCell_;
        std::vector<std::uint32_t> drawn_;
        Stats stats_;
    };

} // namespace cnahouse::visibility
