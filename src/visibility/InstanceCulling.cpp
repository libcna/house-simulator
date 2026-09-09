// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/InstanceCulling.hpp"

namespace cnahouse::visibility
{

    void InstanceCuller::Cull(std::span<const VisibleCell> visible,
                              std::span<const DynamicInstance> instances)
    {
        drawn_.clear();
        lookup_.clear();
        stats_ = Stats{};
        for (const VisibleCell& cell : visible)
        {
            lookup_.push_back(&cell);
        }

        for (std::uint32_t index = 0; index < instances.size(); ++index)
        {
            const DynamicInstance& instance = instances[index];
            ++stats_.tested;

            const VisibleCell* cell = nullptr;
            for (const VisibleCell* candidate : lookup_)
            {
                if (candidate->cell == instance.cell)
                {
                    cell = candidate;
                    break;
                }
            }
            if (cell == nullptr)
            {
                // The room it is standing in was culled, so it is culled -- and it cost one
                // comparison rather than four plane tests, which is the whole reason §25's order
                // is rooms first and things second.
                ++stats_.culledByCell;
                continue;
            }

            bool seen = false;
            for (std::size_t i = 0; i < cell->frustumCount && !seen; ++i)
            {
                seen = cell->frusta[i].Intersects(instance.bounds);
            }
            if (seen)
            {
                drawn_.push_back(index);
                ++stats_.drawn;
            }
            else
            {
                ++stats_.culledByCone;
            }
        }
    }

} // namespace cnahouse::visibility
