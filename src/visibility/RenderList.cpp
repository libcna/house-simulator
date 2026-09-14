// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/RenderList.hpp"

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/world/WorldData.hpp"
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

        [[nodiscard]] float DistanceTo(const Xna::Vector3& point, const Xna::Vector3& eye) noexcept
        {
            const float x = point.X - eye.X;
            const float y = point.Y - eye.Y;
            const float z = point.Z - eye.Z;
            return std::sqrt(x * x + y * y + z * z);
        }

        [[nodiscard]] rendering::Pass PassForChunk(const world::ChunkLibrary& library,
                                                   const world::Chunk& chunk,
                                                   const world::WorldData* world) noexcept
        {
            const rendering::Pass layoutPass = PassForLayout(chunk.layout);
            if (layoutPass == rendering::Pass::AlphaTest || world == nullptr ||
                chunk.material >= library.materials.size())
            {
                return layoutPass;
            }
            const world::MaterialDef* material =
                world->FindMaterial(util::Id::Of(library.materials[chunk.material]));
            return material != nullptr && material->alphaMode == world::AlphaMode::Blend
                       ? rendering::Pass::Transparent
                       : layoutPass;
        }
    } // namespace

    void RenderList::AddChunks(const world::ChunkLibrary& library,
                               std::span<const std::uint32_t> chunks,
                               const Xna::Vector3& eye,
                               const world::WorldData* world)
    {
        cellBounds_.assign(library.cells.size(), CellBoundsScratch{});
        for (const world::Chunk& chunk : library.chunks)
        {
            if (chunk.cell >= cellBounds_.size())
            {
                continue;
            }
            CellBoundsScratch& bounds = cellBounds_[chunk.cell];
            if (!bounds.present)
            {
                bounds.min = chunk.bounds.Min;
                bounds.max = chunk.bounds.Max;
                bounds.present = true;
                continue;
            }
            bounds.min.X = std::min(bounds.min.X, chunk.bounds.Min.X);
            bounds.min.Y = std::min(bounds.min.Y, chunk.bounds.Min.Y);
            bounds.min.Z = std::min(bounds.min.Z, chunk.bounds.Min.Z);
            bounds.max.X = std::max(bounds.max.X, chunk.bounds.Max.X);
            bounds.max.Y = std::max(bounds.max.Y, chunk.bounds.Max.Y);
            bounds.max.Z = std::max(bounds.max.Z, chunk.bounds.Max.Z);
        }
        cellDepths_.assign(library.cells.size(), 0.0F);
        for (std::size_t i = 0; i < cellBounds_.size(); ++i)
        {
            const CellBoundsScratch& bounds = cellBounds_[i];
            if (bounds.present)
            {
                cellDepths_[i] = DistanceTo((bounds.min + bounds.max) * 0.5F, eye);
            }
        }

        items_.reserve(items_.size() + chunks.size());
        for (const std::uint32_t index : chunks)
        {
            if (index >= library.chunks.size())
            {
                continue;
            }
            const world::Chunk& chunk = library.chunks[index];
            RenderItem item;
            item.pass = PassForChunk(library, chunk, world);
            item.effect = EffectForLayout(chunk.layout);
            item.material = chunk.material;
            item.geometry = index;
            item.objectDepth = DistanceTo(chunk.bounds, eye);
            item.cellDepth = chunk.cell < cellDepths_.size() ? cellDepths_[chunk.cell] : item.objectDepth;
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
                                 // §23.6: whole cells back to front, then objects within one cell.
                                 // The nearer pane is consequently blended over everything behind
                                 // it without interleaving two rooms for a material-state saving.
                                 if (a.cellDepth != b.cellDepth)
                                 {
                                     return a.cellDepth > b.cellDepth;
                                 }
                                 return a.objectDepth > b.objectDepth;
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
