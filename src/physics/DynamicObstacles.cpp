// SPDX-License-Identifier: MIT
#include "cnahouse/physics/DynamicObstacles.hpp"

namespace cnahouse::physics
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;
        const std::vector<DynamicObstacle> kEmpty;
    } // namespace

    void DynamicObstacles::BeginFrame() noexcept
    {
        for (auto& entry : byCell_)
        {
            // `clear()` and not `erase`: the vector keeps its capacity, so the refill allocates
            // nothing after the first frame. The map keeps its keys for the same reason -- a cell
            // that had a door last frame will probably have one again.
            entry.second.clear();
        }
        count_ = 0;
    }

    void DynamicObstacles::Add(std::string_view cellId, const DynamicObstacle& obstacle)
    {
        byCell_[std::string(cellId)].push_back(obstacle);
        ++count_;
    }

    std::span<const DynamicObstacle> DynamicObstacles::For(std::string_view cellId) const
    {
        const auto it = byCell_.find(std::string(cellId));
        if (it == byCell_.end())
        {
            return {kEmpty};
        }
        return {it->second};
    }

    DynamicSweepHit SweepDynamic(const DynamicObstacles& obstacles,
                                 std::string_view cellId,
                                 const Capsule& capsule,
                                 const Xna::Vector3& motion)
    {
        DynamicSweepHit result;
        const std::span<const DynamicObstacle> list = obstacles.For(cellId);
        for (std::uint32_t i = 0; i < list.size(); ++i)
        {
            const SweepHit hit = SweepCapsuleObb(capsule, motion, list[i].shape);
            // Earliest, not first found -- the same rule `SweepCell` follows, and for the same
            // reason: a body between a door and a pet is stopped by whichever it reaches first.
            if (hit.hit && (!result.hit || hit.time < result.time))
            {
                static_cast<SweepHit&>(result) = hit;
                result.obstacle = i;
                result.source = list[i].source;
                result.kind = list[i].kind;
            }
        }
        return result;
    }

    DynamicSweepHit
    OverlapDynamic(const DynamicObstacles& obstacles, std::string_view cellId, const Capsule& capsule)
    {
        DynamicSweepHit result;
        float deepest = 0.0f;
        const std::span<const DynamicObstacle> list = obstacles.For(cellId);
        for (std::uint32_t i = 0; i < list.size(); ++i)
        {
            const Overlap overlap = OverlapCapsuleObb(capsule, list[i].shape);
            if (!overlap.overlapped)
            {
                continue;
            }
            if (result.obstacle != DynamicSweepHit::kNothing && overlap.depth <= deepest)
            {
                continue;
            }
            // The DEEPEST, because §49.3's push-out goes along the deepest normal and a door
            // closing on a player pressed against a wall is the case that has two.
            deepest = overlap.depth;
            result.hit = true;
            result.time = 0.0f;
            result.startedInside = true;
            result.normal = overlap.normal;
            result.obstacle = i;
            result.source = list[i].source;
            result.kind = list[i].kind;
        }
        return result;
    }

} // namespace cnahouse::physics
