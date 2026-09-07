// SPDX-License-Identifier: MIT
#include "cnahouse/world/SpatialIndex.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace cnahouse::world
{
    std::int32_t SpatialIndex::Coordinate(float value) noexcept
    {
        // `floor`, not a truncating cast: a cast rounds toward zero, so every bucket touching the
        // origin would be twice as wide as the rest and the two either side of it would overlap.
        return static_cast<std::int32_t>(std::floor(value / kCellSize));
    }

    std::uint64_t SpatialIndex::KeyOf(std::int32_t x, std::int32_t z) noexcept
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) |
               static_cast<std::uint64_t>(static_cast<std::uint32_t>(z));
    }

    SpatialIndex SpatialIndex::Build(const WorldData& world)
    {
        SpatialIndex index;

        // Gathered as (key, cell) pairs first and grouped afterwards, so the flat array is one
        // allocation and a bucket's cells are contiguous. The same arrangement `WorldData` uses
        // for its per-cell lists, for the same reason: this is walked every frame.
        std::vector<std::pair<std::uint64_t, std::uint32_t>> pairs;
        for (std::size_t position = 0; position < world.Cells().size(); ++position)
        {
            const Cell& cell = world.Cells()[position];
            for (const Footprint& box : cell.boxes)
            {
                const std::int32_t x0 = Coordinate(box.minX);
                const std::int32_t x1 = Coordinate(box.maxX);
                const std::int32_t z0 = Coordinate(box.minZ);
                const std::int32_t z1 = Coordinate(box.maxZ);
                for (std::int32_t x = x0; x <= x1; ++x)
                {
                    for (std::int32_t z = z0; z <= z1; ++z)
                    {
                        pairs.emplace_back(KeyOf(x, z), static_cast<std::uint32_t>(position));
                    }
                }
            }
        }

        // A cell with several boxes can land in one bucket more than once. Sorting and uniquing is
        // cheaper than a per-bucket set, and it also makes the bucket contents deterministic,
        // which is what lets a test compare two builds.
        std::sort(pairs.begin(), pairs.end());
        pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());

        index.m_flat.reserve(pairs.size());
        for (const auto& [key, cell] : pairs)
        {
            auto entry = index.m_buckets.find(key);
            if (entry == index.m_buckets.end())
            {
                index.m_buckets.emplace(
                    key, detail::CellRange{static_cast<std::uint32_t>(index.m_flat.size()), 1U});
            }
            else
            {
                ++entry->second.count;
            }
            index.m_flat.push_back(cell);
        }

        for (const auto& [key, range] : index.m_buckets)
        {
            index.m_largest = std::max(index.m_largest, static_cast<std::size_t>(range.count));
        }
        return index;
    }

    std::span<const std::uint32_t> SpatialIndex::Bucket(float x, float z) const noexcept
    {
        const auto entry = m_buckets.find(KeyOf(Coordinate(x), Coordinate(z)));
        if (entry == m_buckets.end())
        {
            return {};
        }
        return std::span<const std::uint32_t>(m_flat).subspan(entry->second.first, entry->second.count);
    }

    std::size_t SpatialIndex::BucketCount() const noexcept
    {
        return m_buckets.size();
    }

    std::size_t SpatialIndex::LargestBucket() const noexcept
    {
        return m_largest;
    }

    util::Id SpatialIndex::Find(const WorldData& world,
                                const Microsoft::Xna::Framework::Vector3& point,
                                util::Id current,
                                Step* answeredBy) const
    {
        const auto answer = [answeredBy](Step step, util::Id id)
        {
            if (answeredBy != nullptr)
            {
                *answeredBy = step;
            }
            return id;
        };

        // 1. Incremental. The hysteresis is here and nowhere else: staying put is sticky, arriving
        //    is not. A margin on the grid step below would let two cells claim one point with no
        //    way to choose between them.
        if (const Cell* cell = world.FindCell(current); cell != nullptr)
        {
            if (world.CellContains(*cell, point, kHysteresis))
            {
                return answer(Step::Incremental, current);
            }

            // 2. Neighbour walk, through the portals of the cell just left. A player who left a
            //    room is almost always in the room next door, and this answers without touching
            //    the grid at all.
            for (const std::uint32_t index : world.PortalsOf(current))
            {
                const util::Id other = world.OtherSide(world.Portals()[index], current);
                const Cell* neighbour = world.FindCell(other);
                if (neighbour != nullptr && world.CellContains(*neighbour, point))
                {
                    return answer(Step::Neighbour, other);
                }
            }
        }

        // 3. The grid. Also the entry point for spawning, teleporting and loading a save, where
        //    there is no last cell to start from.
        for (const std::uint32_t index : Bucket(point.X, point.Z))
        {
            const Cell& cell = world.Cells()[index];
            if (world.CellContains(cell, point))
            {
                return answer(Step::Grid, cell.id);
            }
        }

        // 4. No cell. §16.4 assigns `EXT_WORLD` here; that is a room name and belongs to the
        //    caller, not to this file.
        return answer(Step::None, util::Id{});
    }

} // namespace cnahouse::world
