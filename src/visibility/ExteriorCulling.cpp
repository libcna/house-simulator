// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/ExteriorCulling.hpp"

#include <algorithm>
#include <cmath>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"

#include "Microsoft/Xna/Framework/ContainmentType.hpp"
#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::visibility
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        [[nodiscard]] float Outside(float value, float low, float high) noexcept
        {
            if (value < low)
            {
                return low - value;
            }
            return value > high ? value - high : 0.0F;
        }
    } // namespace

    float DistanceToBox(const Xna::BoundingBox& box, const Xna::Vector3& point) noexcept
    {
        const float x = Outside(point.X, box.Min.X, box.Max.X);
        const float y = Outside(point.Y, box.Min.Y, box.Max.Y);
        const float z = Outside(point.Z, box.Min.Z, box.Max.Z);
        return std::sqrt(x * x + y * y + z * z);
    }

    void ExteriorCones::Collect(const world::WorldData& world,
                                std::span<const VisibleCell> visible,
                                const ClipFrustum& camera)
    {
        cones_.clear();
        rects_.clear();
        cellsOutside_ = 0;
        conesMerged_ = 0;
        degraded_ = false;

        for (const VisibleCell& cell : visible)
        {
            const world::Cell* row = world.FindCell(cell.cell);
            if (row == nullptr || row->kind != world::CellKind::Exterior)
            {
                continue;
            }
            ++cellsOutside_;
            for (std::size_t i = 0; i < cell.frustumCount; ++i)
            {
                // §25.2's containment approximation, reused: a cone whose screen rectangle is
                // inside one already collected can only see what that one sees, and walking the
                // hierarchy again for it would cost a whole traversal to find that out.
                bool covered = false;
                for (const NdcRect& kept : rects_)
                {
                    if (kept.Contains(cell.rects[i]))
                    {
                        covered = true;
                        break;
                    }
                }
                if (covered)
                {
                    ++conesMerged_;
                    continue;
                }
                if (cones_.size() >= kMaxCones)
                {
                    // The cap. Everything collected so far is discarded rather than kept
                    // alongside the camera, because the camera's frustum already contains every
                    // one of them -- it is what they were all reduced from.
                    degraded_ = true;
                    cones_.assign(1, camera);
                    rects_.clear();
                    return;
                }
                cones_.push_back(cell.frusta[i]);
                rects_.push_back(cell.rects[i]);
            }
        }
    }

    void ExteriorCuller::Cull(const ExteriorBvh& bvh,
                              std::span<const ClipFrustum> cones,
                              const Xna::Vector3& eye,
                              float viewDistanceScale)
    {
        drawn_.clear();
        stats_ = Stats{};
        const BvhNode* root = bvh.Root();
        if (root == nullptr || cones.empty())
        {
            // The empty-span case would fall out of the loop below anyway; it is short-circuited
            // here to say so, and to skip sizing a mask nothing will write to.
            return;
        }

        found_.assign(bvh.Instances().size(), 0u);
        // Clamped ONCE, and by the same function `CullDistanceFor` uses: a node rejected at an
        // unclamped scale would cull instances the per-instance test would have kept.
        const float scale = ClampViewDistanceScale(viewDistanceScale);
        // ONE walk carrying every cone, and not one walk per cone (`HOUSE-00699`). §25.6's step 2
        // -- the distance -- does not depend on the cone at all, so a walk per cone paid for the
        // same square root once per opening onto the garden; measured, `L0_KITCHEN` reaches the
        // outdoors through seven and was testing 5 639 instances to draw 513.
        //
        // Thirty-two at a time because the live set is a bit per cone. `ExteriorCones` collects at
        // most `kMaxCones` = 8, so the loop runs once; it is a loop rather than a cap because a
        // cap would silently cull whatever the thirty-third cone could see, and this walk is not
        // allowed to be the thing that loses a tree.
        constexpr std::size_t kConesPerPass = 32;
        for (std::size_t base = 0; base < cones.size(); base += kConesPerPass)
        {
            const std::size_t count = std::min(kConesPerPass, cones.size() - base);
            const std::uint32_t live =
                count == kConesPerPass ? ~std::uint32_t{0} : (std::uint32_t{1} << count) - std::uint32_t{1};
            Visit(bvh, *root, cones.subspan(base, count), live, 0u, eye, scale);
        }

        for (std::uint32_t i = 0; i < found_.size(); ++i)
        {
            if (found_[i] != 0u)
            {
                drawn_.push_back(i);
            }
        }
        stats_.instancesDrawn = static_cast<int>(drawn_.size());
    }

    void ExteriorCuller::Visit(const ExteriorBvh& bvh,
                               const BvhNode& node,
                               std::span<const ClipFrustum> cones,
                               std::uint32_t live,
                               std::uint32_t inside,
                               const Xna::Vector3& eye,
                               float scale)
    {
        ++stats_.nodesVisited;
        if (inside != 0u)
        {
            ++stats_.nodesSkippedFrustumTest;
        }

        // §25.6's step 2 first, and against the node's OWN summary: the largest distance anything
        // under it is drawn at. Nothing inside can outlive that, so one comparison retires the
        // whole subtree -- which is the entire reason the mask and the distance are on the node.
        //
        // It is also the reason the cones share a walk: this test has no cone in it, so a walk per
        // cone asked it once per cone and got the same answer every time.
        if (DistanceToBox(node.bounds, eye) > node.maxCullDistance * scale)
        {
            ++stats_.nodesCulledByDistance;
            return;
        }

        std::uint32_t stillLive = 0u;
        std::uint32_t stillInside = 0u;
        for (std::size_t i = 0; i < cones.size(); ++i)
        {
            const std::uint32_t bit = std::uint32_t{1} << i;
            if ((live & bit) == 0u)
            {
                continue;
            }
            if ((inside & bit) != 0u)
            {
                // A parent was wholly inside this cone, so this node is too: nothing to test.
                stillLive |= bit;
                stillInside |= bit;
                continue;
            }
            ++stats_.frustumTests;
            const Xna::ContainmentType containment = cones[i].Contains(node.bounds);
            if (containment == Xna::ContainmentType::Disjoint)
            {
                continue;
            }
            stillLive |= bit;
            if (containment == Xna::ContainmentType::Contains)
            {
                // Everything below is inside too, so the frustum test is done for this subtree.
                stillInside |= bit;
                ++stats_.nodesFullyInside;
            }
        }

        if (stillLive == 0u)
        {
            ++stats_.nodesCulledByFrustum;
            return;
        }

        if (!node.IsLeaf())
        {
            for (std::uint8_t child = 0; child < node.childCount; ++child)
            {
                Visit(bvh, bvh.Nodes()[node.firstChild + child], cones, stillLive, stillInside, eye, scale);
            }
            return;
        }

        for (std::uint32_t i = node.first; i < node.first + node.count; ++i)
        {
            ++stats_.instancesTested;
            const ExteriorInstance& instance = bvh.Instances()[i];
            if (!WithinCullDistance(instance.category, DistanceToBox(instance.bounds, eye), scale))
            {
                ++stats_.instancesCulledByDistance;
                continue;
            }
            if (stillInside != 0u)
            {
                // Some cone holds this whole leaf, so it holds every instance in it.
                found_[i] = 1u;
                continue;
            }
            bool seen = false;
            for (std::size_t cone = 0; cone < cones.size() && !seen; ++cone)
            {
                if ((stillLive & (std::uint32_t{1} << cone)) == 0u)
                {
                    continue;
                }
                ++stats_.frustumTests;
                // The FIRST cone that can see it is enough: the cones are alternative views of the
                // same garden and an instance in any of them is on screen. Asking the rest would
                // be work for an answer that cannot change.
                seen = cones[cone].Contains(instance.bounds) != Xna::ContainmentType::Disjoint;
            }
            if (!seen)
            {
                ++stats_.instancesCulledByFrustum;
                continue;
            }
            found_[i] = 1u;
        }
    }

} // namespace cnahouse::visibility
