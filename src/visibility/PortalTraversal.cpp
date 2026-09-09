// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/PortalTraversal.hpp"

#include <algorithm>
#include <span>

#include "cnahouse/visibility/ClipRect.hpp"
#include "cnahouse/visibility/PortalFacing.hpp"
#include "cnahouse/visibility/ReduceFrustum.hpp"
#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::visibility
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;
    }

    PortalTraversal::PortalTraversal()
    {
        // Twice §71.2's hard stop, because the walk reaches cells and `Degrade` trims them
        // afterwards: the list is at its longest before the cap is applied, not after it.
        visible_.reserve(kMaxVisibleCells * 2);
    }

    const VisibleCell* PortalTraversal::Find(util::Id cell) const noexcept
    {
        const auto found = std::find_if(
            visible_.begin(), visible_.end(), [cell](const VisibleCell& one) { return one.cell == cell; });
        return found == visible_.end() ? nullptr : &*found;
    }

    VisibleCell& PortalTraversal::Reach(util::Id cell, int depth, int allowance, ConeFlags flags)
    {
        for (VisibleCell& one : visible_)
        {
            if (one.cell == cell)
            {
                // Breadth-first, so the first arrival is the shallowest -- but a later one can
                // still tie, and taking the minimum says what the code means rather than relying
                // on the queue's order to say it.
                one.depth = std::min(one.depth, depth);
                // The LARGEST, because a cell reached two ways may continue down whichever chain
                // has the more allowance left: a room seen through a window and through an open
                // door is the door's chain from there on (`HOUSE-00680`).
                one.allowance = std::max(one.allowance, allowance);
                // §26.4's detail sets are dropped for a cell reached through frosted glass, so the
                // flag survives only while EVERY way in has it: one clear view of a room is enough
                // to need its dressing props.
                one.flags = static_cast<ConeFlags>(static_cast<std::uint8_t>(one.flags) &
                                                   static_cast<std::uint8_t>(flags));
                return one;
            }
        }
        visible_.push_back(VisibleCell{});
        visible_.back().cell = cell;
        visible_.back().depth = depth;
        visible_.back().allowance = allowance;
        // The camera's own cell needs no special case: it is queued with no flags, so this is
        // `None` for it and whatever the cone carried for everything else.
        visible_.back().flags = flags;
        ++stats_.cellsVisited;
        return visible_.back();
    }

    void PortalTraversal::Run(const Input& input)
    {
        // Cleared rather than reconstructed: `clear()` keeps the capacity, and the ring is an
        // array inside the object, so a steady state allocates nothing at all (§71.2 gives
        // visibility 0.55 ms and none of it should be `malloc`).
        visible_.clear();
        queue_.Clear();
        stats_ = TraversalStats{};
        if (input.world == nullptr || !input.cameraCell.IsValid())
        {
            return;
        }

        // The whole screen: the camera's own frustum covers all of it, so nothing can be
        // "contained" by it and skipped before the walk has started.
        const NdcRect whole{-1.0F, -1.0F, 1.0F, 1.0F};
        static_cast<void>(queue_.Push(Work{
            input.cameraCell, input.cameraFrustum, whole, ClippedPolygon{}, 0, kNoLimit, ConeFlags::None}));

        while (!queue_.Empty())
        {
            const Work work = queue_.Pop();
            VisibleCell& cell = Reach(work.cell, work.depth, work.allowance, work.flags);
            stats_.maxDepth = std::max(stats_.maxDepth, work.depth);

            // §25.2's containment skip, at the point the frustum is about to be USED: this cone
            // is inside one this cell has already been expanded with, so everything it could
            // reach has been reached.
            bool covered = false;
            for (std::size_t i = 0; i < cell.frustumCount; ++i)
            {
                covered = covered || cell.rects[i].Contains(work.rect);
            }
            if (covered)
            {
                ++stats_.skippedContained;
                continue;
            }

            if (cell.frustumCount < kMaxFrustaPerCell)
            {
                cell.frusta[cell.frustumCount] = work.frustum;
                cell.rects[cell.frustumCount] = work.rect;
                cell.apertures[cell.frustumCount] = work.aperture;
                ++cell.frustumCount;
            }
            else
            {
                // The cell stays visible; what is lost is a fifth cone to test its contents
                // against, and the four kept are wider than the fifth would have narrowed to.
                ++cell.frustaDropped;
                ++stats_.frustaDropped;
                continue;
            }

            // The cone's own planes, read where they already are: `ClipRectToFrustum` wants a
            // span and `ClipFrustum` holds a contiguous array, so the copy this used to make --
            // ten planes into a vector, once per visible cell -- was work for nothing.
            const std::span<const Xna::Plane> planes = work.frustum.Planes();

            for (const std::uint32_t index : input.world->PortalsOf(work.cell))
            {
                if (index >= input.portals.size())
                {
                    continue;
                }
                const world::Portal& portal = input.world->Portals()[index];
                const PortalRuntime& runtime = input.portals[index];
                ++stats_.portalsTested;

                // §25.3: a closed opaque door stops vision; a closed GLASS one does not.
                if (!runtime.PassesLight())
                {
                    ++stats_.skippedClosed;
                    continue;
                }
                if (PlaneFacesAway(portal, *input.world, work.cell, input.eye))
                {
                    ++stats_.skippedFacing;
                    continue;
                }
                // §25.2's cap belongs to the CHAIN and not to this one portal (`HOUSE-00680`).
                // *"Standing in the garden you should see one room through a window, not that room
                // plus everything behind its open door"* -- and a per-portal cap gives exactly
                // that: the window admits the chain at depth 1, and the room's own door, whose cap
                // is 2, then carries it on. Taking the minimum of every cap the chain has crossed
                // is what makes the window's 1 mean what §25.2 says it means.
                const int allowance = std::min(work.allowance, MaxDepthFor(portal, *input.world, input.side));
                if (work.depth >= allowance)
                {
                    ++stats_.skippedDepth;
                    continue;
                }

                const ClippedPolygon clipped = ClipRectToFrustum(runtime.WorldRect(), planes);
                if (clipped.Empty())
                {
                    ++stats_.skippedClipped;
                    continue;
                }
                if (!PortalContributes(clipped.Points(), input.viewProjection))
                {
                    ++stats_.skippedArea;
                    continue;
                }

                const util::Id other = portal.cellA == work.cell ? portal.cellB : portal.cellA;
                const NdcRect rect = NdcBounds(clipped.Points(), input.viewProjection);
                // §25.2's skip again, at the push: a cone inside one the target has already been
                // expanded with reaches nothing new, and testing it here saves queueing it.
                if (const VisibleCell* already = Find(other); already != nullptr)
                {
                    bool contained = false;
                    for (std::size_t i = 0; i < already->frustumCount; ++i)
                    {
                        contained = contained || already->rects[i].Contains(rect);
                    }
                    if (contained)
                    {
                        ++stats_.skippedContained;
                        continue;
                    }
                }

                const ReducedFrustum next =
                    ReduceFrustum(input.eye, clipped.Points(), input.nearPlane, input.farPlane);
                // §25.2: *"if p.opacity == translucent: next.flags |= DIFFUSE"*. The `|=` is the
                // point -- a cone that came through frosted glass stays diffuse however many
                // clear doorways it crosses afterwards, because the glass is still between the
                // camera and everything down that chain.
                const ConeFlags flags = portal.opacity == world::PortalOpacity::Translucent
                                            ? work.flags | ConeFlags::Diffuse
                                            : work.flags;
                // Counted on the PUSH and not before it: a cone the ring had no room for was
                // not crossed, whatever the walk decided, and `queueDropped` is where it went.
                if (queue_.Push(Work{other, next.frustum, rect, clipped, work.depth + 1, allowance, flags}))
                {
                    ++stats_.portalsCrossed;
                }
            }
        }

        stats_.queuePeak = static_cast<int>(queue_.Peak());
        stats_.queueDropped = static_cast<int>(queue_.Dropped());

        Degrade();

        for (const VisibleCell& cell : visible_)
        {
            stats_.diffuseCells += Has(cell.flags, ConeFlags::Diffuse) ? 1 : 0;
        }
    }

    void PortalTraversal::Degrade()
    {
        if (visible_.size() <= kMaxVisibleCells)
        {
            return;
        }

        // R-08's graceful degradation: keep the cells with the biggest share of the screen. The
        // camera's own cell is never a candidate -- it is `visible_[0]`, the room the player is
        // standing in, and dropping it would be a frame with no floor.
        //
        // Sorted only when the cap is exceeded, which measurement says is never in this house: the
        // ordinary path keeps the walk's breadth-first order, which is what `F3` reads.
        std::stable_sort(
            visible_.begin() + 1,
            visible_.end(),
            [](const VisibleCell& a, const VisibleCell& b)
            {
                const auto area = [](const VisibleCell& cell)
                {
                    float biggest = 0.0F;
                    for (std::size_t i = 0; i < cell.frustumCount; ++i)
                    {
                        const NdcRect& rect = cell.rects[i];
                        biggest = std::max(
                            biggest, rect.Empty() ? 0.0F : (rect.maxX - rect.minX) * (rect.maxY - rect.minY));
                    }
                    return biggest;
                };
                return area(a) > area(b);
            });
        stats_.cellsDropped = static_cast<int>(visible_.size() - kMaxVisibleCells);
        visible_.resize(kMaxVisibleCells);
    }

} // namespace cnahouse::visibility
