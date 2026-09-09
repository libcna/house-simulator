// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/RenderList.hpp"

#include <algorithm>
#include <cmath>

namespace cnahouse::visibility
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        [[nodiscard]] float DistanceTo(const Xna::BoundingBox& bounds, const Xna::Vector3& eye) noexcept
        {
            const float x = (bounds.Min.X + bounds.Max.X) * 0.5F - eye.X;
            const float y = (bounds.Min.Y + bounds.Max.Y) * 0.5F - eye.Y;
            const float z = (bounds.Min.Z + bounds.Max.Z) * 0.5F - eye.Z;
            return std::sqrt(x * x + y * y + z * z);
        }
    } // namespace

    void RenderList::AddChunks(const world::ChunkLibrary& library,
                               std::span<const std::uint32_t> chunks,
                               const Xna::Vector3& eye)
    {
        items_.reserve(items_.size() + chunks.size());
        for (const std::uint32_t index : chunks)
        {
            if (index >= library.chunks.size())
            {
                continue;
            }
            const world::Chunk& chunk = library.chunks[index];
            RenderItem item;
            item.pass = PassForLayout(chunk.layout);
            item.effect = EffectForLayout(chunk.layout);
            item.material = chunk.material;
            item.geometry = index;
            // Measured for every item and not only the transparent ones: the branch that would skip
            // it costs about what the square root does, and the number is what §71's overlay prints
            // next to a chunk when it is asked why something is being drawn.
            item.depth = DistanceTo(chunk.bounds, eye);
            items_.push_back(item);
        }
        sorted_ = false;
    }

    void RenderList::Sort()
    {
        // `stable_sort`, so two items the comparator calls equal keep the order they were added in.
        // That is what makes the frame identical on every machine: the visible set is built in a
        // defined order, and an unstable sort would throw that order away and let a render-regression
        // fixture disagree with itself between two runs of the same binary.
        std::stable_sort(items_.begin(),
                         items_.end(),
                         [](const RenderItem& a, const RenderItem& b)
                         {
                             if (a.pass != b.pass)
                             {
                                 return a.pass < b.pass;
                             }
                             if (a.pass == rendering::Pass::Transparent)
                             {
                                 // §7.5's F: back to front, whatever the material is. Farthest
                                 // first, so the nearer pane blends over what is already there.
                                 return a.depth > b.depth;
                             }
                             if (a.effect != b.effect)
                             {
                                 return a.effect < b.effect;
                             }
                             if (a.material != b.material)
                             {
                                 return a.material < b.material;
                             }
                             return a.geometry < b.geometry;
                         });
        sorted_ = true;
    }

    std::span<const RenderItem> RenderList::ItemsFor(rendering::Pass pass)
    {
        if (!sorted_)
        {
            Sort();
        }
        const auto first =
            std::lower_bound(items_.begin(),
                             items_.end(),
                             pass,
                             [](const RenderItem& item, rendering::Pass value) { return item.pass < value; });
        const auto last =
            std::upper_bound(first,
                             items_.end(),
                             pass,
                             [](rendering::Pass value, const RenderItem& item) { return value < item.pass; });
        return std::span<const RenderItem>(items_.data() + (first - items_.begin()),
                                           static_cast<std::size_t>(last - first));
    }

    int RenderList::StateChanges() const noexcept
    {
        int changes = 0;
        for (std::size_t i = 1; i < items_.size(); ++i)
        {
            const RenderItem& previous = items_[i - 1];
            const RenderItem& item = items_[i];
            if (item.pass != previous.pass || item.effect != previous.effect ||
                item.material != previous.material)
            {
                ++changes;
            }
        }
        return changes;
    }

} // namespace cnahouse::visibility
