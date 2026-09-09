// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/ExteriorBvh.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace cnahouse::visibility
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        [[nodiscard]] float CentreOn(const Xna::BoundingBox& box, int axis) noexcept
        {
            switch (axis)
            {
                case 0:
                    return (box.Min.X + box.Max.X) * 0.5F;
                case 1:
                    return (box.Min.Y + box.Max.Y) * 0.5F;
                default:
                    return (box.Min.Z + box.Max.Z) * 0.5F;
            }
        }

        /// The axis @p box is longest on. Ties go to the lower axis, so the split is a function of
        /// the geometry and not of whatever order the instances arrived in.
        [[nodiscard]] int LongestAxis(const Xna::BoundingBox& box) noexcept
        {
            const float x = box.Max.X - box.Min.X;
            const float y = box.Max.Y - box.Min.Y;
            const float z = box.Max.Z - box.Min.Z;
            if (x >= y && x >= z)
            {
                return 0;
            }
            return y >= z ? 1 : 2;
        }

        [[nodiscard]] Xna::BoundingBox Union(const Xna::BoundingBox& a, const Xna::BoundingBox& b) noexcept
        {
            return Xna::BoundingBox(
                Xna::Vector3(
                    std::min(a.Min.X, b.Min.X), std::min(a.Min.Y, b.Min.Y), std::min(a.Min.Z, b.Min.Z)),
                Xna::Vector3(
                    std::max(a.Max.X, b.Max.X), std::max(a.Max.Y, b.Max.Y), std::max(a.Max.Z, b.Max.Z)));
        }

    } // namespace

    void ExteriorBvh::Build(std::span<const ExteriorInstance> instances)
    {
        instances_.assign(instances.begin(), instances.end());
        nodes_.clear();
        stats_ = Stats{};
        stats_.instances = static_cast<int>(instances_.size());
        if (instances_.empty())
        {
            return;
        }
        // 1 + 8 + 64 is the most three levels of eight can produce, so the build never reallocates.
        nodes_.reserve(1u + kBranching + kBranching * kBranching);
        nodes_.resize(1u);
        FillNode(0u, 0u, static_cast<std::uint32_t>(instances_.size()), 0);

        stats_.nodes = static_cast<int>(nodes_.size());
        for (const BvhNode& node : nodes_)
        {
            if (node.IsLeaf())
            {
                ++stats_.leaves;
                stats_.maxLeafSize = std::max(stats_.maxLeafSize, static_cast<int>(node.count));
            }
        }
    }

    void ExteriorBvh::FillNode(std::uint32_t self, std::uint32_t first, std::uint32_t count, int level)
    {
        stats_.levels = std::max(stats_.levels, level + 1);

        // The node's own box and category mask, from what is actually under it -- which is what
        // makes the hierarchy loose: siblings overlap exactly where their contents do.
        Xna::BoundingBox bounds = instances_[first].bounds;
        std::uint8_t categories = 0u;
        for (std::uint32_t i = first; i < first + count; ++i)
        {
            bounds = Union(bounds, instances_[i].bounds);
            categories = static_cast<std::uint8_t>(categories | CategoryBit(instances_[i].category));
        }
        float maxDistance = 0.0F;
        for (std::size_t c = 0; c < static_cast<std::size_t>(PropCategory::Count); ++c)
        {
            if ((categories & CategoryBit(static_cast<PropCategory>(c))) != 0u)
            {
                maxDistance = std::max(maxDistance, kCullDistances[c]);
            }
        }
        nodes_[self].bounds = bounds;
        nodes_[self].categories = categories;
        nodes_[self].maxCullDistance = maxDistance;
        nodes_[self].first = first;
        nodes_[self].count = count;
        nodes_[self].childCount = 0u;

        if (level + 1 >= kLevels || count <= kMinToSplit)
        {
            return;
        }

        // Three median splits along whatever is longest at each step, which is what turns one
        // range into eight spatial ones. The axis is re-chosen per split rather than fixed per
        // level: the plot is 400 m across and a few metres tall, so a level that split Y first
        // would spend one of its three cuts separating the grass from the grass.
        std::array<std::uint32_t, kBranching + 1> edges{};
        edges[0] = first;
        edges[1] = first + count;
        std::size_t parts = 1;
        for (int round = 0; round < 3; ++round)
        {
            std::array<std::uint32_t, kBranching + 1> next{};
            std::size_t written = 0;
            next[written++] = edges[0];
            for (std::size_t part = 0; part < parts; ++part)
            {
                const std::uint32_t begin = edges[part];
                const std::uint32_t end = edges[part + 1];
                const std::uint32_t size = end - begin;
                if (size < 2u)
                {
                    next[written++] = end;
                    continue;
                }
                Xna::BoundingBox partBounds = instances_[begin].bounds;
                for (std::uint32_t i = begin + 1; i < end; ++i)
                {
                    partBounds = Union(partBounds, instances_[i].bounds);
                }
                const int axis = LongestAxis(partBounds);
                const std::uint32_t middle = begin + size / 2u;
                std::nth_element(instances_.begin() + begin,
                                 instances_.begin() + middle,
                                 instances_.begin() + end,
                                 [axis](const ExteriorInstance& a, const ExteriorInstance& b)
                                 {
                                     const float ca = CentreOn(a.bounds, axis);
                                     const float cb = CentreOn(b.bounds, axis);
                                     // The id breaks a tie, so a row of identical fence posts is
                                     // partitioned the same way on every machine. `nth_element`
                                     // is not stable and would otherwise leave the order to the
                                     // standard library's introselect.
                                     return ca != cb ? ca < cb : a.id.Value() < b.id.Value();
                                 });
                next[written++] = middle;
                next[written++] = end;
            }
            edges = next;
            parts = written - 1;
        }

        // The eight ranges become eight CONSECUTIVE nodes, allocated before any of them is filled.
        // Filling one first would put its whole subtree between it and its next sibling, and then
        // `firstChild` plus an index would name a grandchild -- which is the bug a flat BVH has
        // exactly once.
        // Always eight, and never an empty one: a node only gets here with more than
        // `kMinToSplit` instances, and halving a range of nine or more three times leaves eight
        // parts of at least one each. So there is no empty-child case to guard against -- and a
        // guard for one would be a branch no input can take, which is worse than none.
        std::array<std::uint32_t, kBranching> begins{};
        std::array<std::uint32_t, kBranching> sizes{};
        std::uint8_t childCount = 0u;
        for (std::size_t part = 0; part < parts; ++part)
        {
            begins[childCount] = edges[part];
            sizes[childCount] = edges[part + 1] - edges[part];
            ++childCount;
        }
        const std::uint32_t firstChild = static_cast<std::uint32_t>(nodes_.size());
        nodes_.resize(nodes_.size() + childCount);
        nodes_[self].firstChild = firstChild;
        nodes_[self].childCount = childCount;
        for (std::uint8_t child = 0; child < childCount; ++child)
        {
            FillNode(firstChild + child, begins[child], sizes[child], level + 1);
        }
    }

} // namespace cnahouse::visibility
