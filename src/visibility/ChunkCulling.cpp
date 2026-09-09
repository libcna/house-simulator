// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/ChunkCulling.hpp"

#include "cnahouse/world/ChunkData.hpp"

namespace cnahouse::visibility
{

    ChunkCuller::ChunkCuller(const world::ChunkLibrary& library)
        : library_(&library)
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
            const auto found = byCell_.find(cell.cell.Value());
            if (found == byCell_.end())
            {
                // A cell with no geometry at all: `EXT_ROAD` and the container sub-cells are
                // cells for the graph's purposes and have nothing to draw.
                continue;
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
                }
                else
                {
                    ++stats_.chunksCulled;
                }
            }
        }
    }

} // namespace cnahouse::visibility
