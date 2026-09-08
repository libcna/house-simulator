// SPDX-License-Identifier: MIT
#include "cnahouse/player/CellTracker.hpp"

namespace cnahouse::player
{

    bool CellTracker::Update(const world::WorldData& world,
                             const world::SpatialIndex& index,
                             const Microsoft::Xna::Framework::Vector3& point,
                             app::EventQueue* queue)
    {
        const util::Id found = index.Find(world, point, current_, &step_);
        if (found == current_)
        {
            return false;
        }

        // A lookup that finds NOTHING is not a reason to move the player somewhere: §16.4's step 4
        // hands back `EXT_WORLD` for a point outside the shell and an invalid id only when the
        // world data itself is wrong. Keeping the last good cell is what that clause asks for, and
        // it also means a momentary failure cannot make five systems cross-fade to nowhere.
        if (!found.IsValid())
        {
            return false;
        }

        const util::Id previous = current_;
        current_ = found;
        if (queue != nullptr)
        {
            queue->Publish(world::CellEntered{found, previous});
        }
        return true;
    }

} // namespace cnahouse::player
