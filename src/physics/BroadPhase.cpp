// SPDX-License-Identifier: MIT
#include "cnahouse/physics/BroadPhase.hpp"

#include <cmath>

namespace cnahouse::physics
{
    namespace Xna = Microsoft::Xna::Framework;

    const std::vector<std::uint32_t>& BroadPhase::Query(const CollisionCell& cell,
                                                        const Xna::BoundingBox& box)
    {
        results_.clear();
        bucketsVisited_ = 0u;
        entriesWalked_ = 0u;
        if (cell.nx == 0u || cell.nz == 0u || cell.buckets.empty() || cell.shapes.empty())
        {
            return results_;
        }
        // `!(min <= max)` so an inverted box AND a NaN one both come back empty. A NaN compares
        // false against everything, and a query nobody can answer must answer nothing rather than
        // the whole cell.
        if (!(box.Min.X <= box.Max.X) || !(box.Min.Z <= box.Max.Z))
        {
            return results_;
        }

        // The grid is 1 m in x/z (§49.2). A third axis would multiply buckets without dividing
        // shapes -- the floor slab alone spans every bucket of every vertical layer -- and the
        // vertical reject is one comparison against the shape's own AABB, which the sweep does
        // anyway. So the query's Y is not used here and is the narrow phase's business.
        const float lowX = (box.Min.X - cell.originX);
        const float highX = (box.Max.X - cell.originX);
        const float lowZ = (box.Min.Z - cell.originZ);
        const float highZ = (box.Max.Z - cell.originZ);
        if (highX < 0.0f || highZ < 0.0f)
        {
            return results_;
        }
        const auto nx = static_cast<float>(cell.nx);
        const auto nz = static_cast<float>(cell.nz);
        if (lowX >= nx || lowZ >= nz)
        {
            return results_;
        }

        const auto i0 = static_cast<std::uint32_t>(std::max(0.0f, std::floor(lowX)));
        const auto j0 = static_cast<std::uint32_t>(std::max(0.0f, std::floor(lowZ)));
        const auto i1 = static_cast<std::uint32_t>(std::min(nx - 1.0f, std::floor(highX)));
        const auto j1 = static_cast<std::uint32_t>(std::min(nz - 1.0f, std::floor(highZ)));

        ++serial_;
        if (stamp_.size() < cell.shapes.size())
        {
            stamp_.assign(cell.shapes.size(), 0u);
        }
        if (serial_ == 0u)
        {
            // Wrapped after 4 294 967 295 queries -- an hour and a half at a million a second.
            // Cheaper to handle than to argue about: clear the stamps and start again.
            stamp_.assign(stamp_.size(), 0u);
            serial_ = 1u;
        }

        for (std::uint32_t j = j0; j <= j1; ++j)
        {
            for (std::uint32_t i = i0; i <= i1; ++i)
            {
                const std::vector<std::uint16_t>& bucket =
                    cell.buckets[static_cast<std::size_t>(j) * cell.nx + i];
                ++bucketsVisited_;
                for (const std::uint16_t local : bucket)
                {
                    ++entriesWalked_;
                    if (local >= stamp_.size() || stamp_[local] == serial_)
                    {
                        continue;
                    }
                    stamp_[local] = serial_;
                    results_.push_back(cell.shapes[local]);
                }
            }
        }
        return results_;
    }

} // namespace cnahouse::physics
