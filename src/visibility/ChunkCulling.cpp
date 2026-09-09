// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/ChunkCulling.hpp"

#include "cnahouse/world/ChunkData.hpp"
#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::visibility
{

    ChunkCuller::ChunkCuller(const world::ChunkLibrary& library, const world::WorldData* world)
        : library_(&library)
        , world_(world)
    {
        for (std::uint32_t index = 0; index < library.chunks.size(); ++index)
        {
            const world::Chunk& chunk = library.chunks[index];
            if (chunk.cell >= library.cells.size())
            {
                // A chunk whose cell index is out of range is a corrupt file, and `ChunkReader`
                // has already refused those -- but the loop is over a `uint16` and this is the one
                // place that would index the name table with it.
                continue;
            }
            byCell_[util::Intern(library.cells[chunk.cell]).Value()].push_back(index);
        }
    }

    void ChunkCuller::Cull(std::span<const VisibleCell> visible)
    {
        drawn_.clear();
        stats_ = Stats{};
        if (library_ == nullptr)
        {
            return;
        }

        for (const VisibleCell& cell : visible)
        {
            CullCell(cell, cell.cell, false);

            // §12's nested cells, against the cones of the room they stand in (`HOUSE-00488`).
            //
            // **A sub-cell's OUTSIDE is the room's furniture.** The front of the refrigerator and
            // the underside of the garage loft are geometry a body in the kitchen or the garage is
            // looking straight at, and §17.4 puts them in the sub-cell's own chunk. §54 says a
            // container "falls out of the visibility system rather than being a special case", and
            // it does -- for its INSIDE, which its own shut door hides, and which is why this is
            // here and not in the walk: making the fridge VISIBLE would light it and open an audio
            // path through a shut door. Only its geometry is wanted, and only in the frame that
            // already draws the room around it. Measured: 18 606 pixels of the refrigerator and
            // 12 108 of the loft vanished the moment culling was turned on.
            if (world_ == nullptr)
            {
                continue;
            }
            for (const std::uint32_t index : world_->ChildrenOf(cell.cell))
            {
                if (index < world_->Cells().size())
                {
                    CullCell(cell, world_->Cells()[index].id, true);
                }
            }
        }
    }

    void ChunkCuller::CullCell(const VisibleCell& cell, util::Id owner, bool nested)
    {
        const auto found = byCell_.find(owner.Value());
        if (found == byCell_.end())
        {
            // A cell with no geometry at all: `EXT_ROAD` is a cell for the graph's purposes and
            // has nothing to draw.
            return;
        }
        ++stats_.cellsTested;

        for (const std::uint32_t index : found->second)
        {
            ++stats_.chunksTested;
            const world::Chunk& chunk = library_->chunks[index];

            bool seen = false;
            for (std::size_t i = 0; i < cell.frustumCount && !seen; ++i)
            {
                // One cone is enough: the cones are alternative views of the same room, so a
                // chunk in any of them is on screen.
                seen = cell.frusta[i].Intersects(chunk.bounds);
            }
            if (seen)
            {
                drawn_.push_back(index);
                ++stats_.chunksDrawn;
                stats_.chunksFromNested += nested ? 1 : 0;
            }
            else
            {
                ++stats_.chunksCulled;
            }
        }
    }

} // namespace cnahouse::visibility
