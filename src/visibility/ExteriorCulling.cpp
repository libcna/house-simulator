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
        for (const ClipFrustum& cone : cones)
        {
            Visit(bvh, *root, cone, eye, scale, false);
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
                               const ClipFrustum& cone,
                               const Xna::Vector3& eye,
                               float scale,
                               bool inside)
    {
        ++stats_.nodesVisited;
        if (inside)
        {
            ++stats_.nodesSkippedFrustumTest;
        }

        // §25.6's step 2 first, and against the node's OWN summary: the largest distance anything
        // under it is drawn at. Nothing inside can outlive that, so one comparison retires the
        // whole subtree -- which is the entire reason the mask and the distance are on the node.
        if (DistanceToBox(node.bounds, eye) > node.maxCullDistance * scale)
        {
            ++stats_.nodesCulledByDistance;
            return;
        }

        if (!inside)
        {
            ++stats_.frustumTests;
            const Xna::ContainmentType containment = cone.Contains(node.bounds);
            if (containment == Xna::ContainmentType::Disjoint)
            {
                ++stats_.nodesCulledByFrustum;
                return;
            }
            if (containment == Xna::ContainmentType::Contains)
            {
                // Everything below is inside too, so the frustum test is done for this subtree.
                inside = true;
                ++stats_.nodesFullyInside;
            }
        }

        if (!node.IsLeaf())
        {
            for (std::uint8_t child = 0; child < node.childCount; ++child)
            {
                Visit(bvh, bvh.Nodes()[node.firstChild + child], cone, eye, scale, inside);
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
            if (!inside)
            {
                ++stats_.frustumTests;
                if (cone.Contains(instance.bounds) == Xna::ContainmentType::Disjoint)
                {
                    ++stats_.instancesCulledByFrustum;
                    continue;
                }
            }
            found_[i] = 1u;
        }
    }

} // namespace cnahouse::visibility
