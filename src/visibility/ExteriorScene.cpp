// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/ExteriorScene.hpp"

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/world/ChunkData.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldTypes.hpp"

namespace cnahouse::visibility
{
    namespace
    {
        [[nodiscard]] bool StartsWith(std::string_view text, std::string_view prefix) noexcept
        {
            return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
        }
    } // namespace

    std::uint32_t ExteriorScene::ChunkOf(std::uint32_t index) const noexcept
    {
        const auto held = bvh.Instances();
        if (index >= held.size())
        {
            return 0u;
        }
        // `Build` reorders, so the chunk cannot be the position: it is carried on the id, which
        // is `chunk + 1` so that a zero id -- `util::Id`'s "no id" -- is never a valid chunk.
        const std::uint32_t value = held[index].id.Value();
        return value == 0u ? 0u : value - 1u;
    }

    PropCategory CategoryForMaterial(std::string_view material) noexcept
    {
        // The ground and what is fixed to it: terrain tiles, the road and its markings, and -- as
        // `HOUSE-00494` files them -- the house's own roofs and chimney, which are the house and
        // are not distance-culled at the scale of its own property.
        if (StartsWith(material, "TERRAIN_") || StartsWith(material, "ROAD_"))
        {
            return PropCategory::Ground;
        }
        if (StartsWith(material, "FENCE_") || StartsWith(material, "GATE_"))
        {
            return PropCategory::Fence;
        }
        if (StartsWith(material, "GARDEN_"))
        {
            return PropCategory::GardenFurniture;
        }
        if (StartsWith(material, "TREE_") || StartsWith(material, "VEG_"))
        {
            return PropCategory::Tree;
        }
        if (StartsWith(material, "NB_IMPOSTOR") || StartsWith(material, "NB_HORIZON"))
        {
            return PropCategory::Impostor;
        }
        if (StartsWith(material, "NB_"))
        {
            return PropCategory::NeighbourhoodLod0;
        }
        // Everything else, `BLOCKOUT_*` included. A category only ever REMOVES something at
        // distance, so an unrecognised material takes the one that removes nothing: guessing
        // "small prop" at 45 m would cull the house's own roof from the far end of the garden.
        return PropCategory::Ground;
    }

    bool IsExteriorSkinMaterial(std::string_view material) noexcept
    {
        return StartsWith(material, "MAT_SIDING_") || material == "MAT_BRICK_WATER_TABLE" ||
               material == "MAT_BRICK_WATER_TABLE_WET";
    }

    ExteriorScene BuildExteriorScene(const world::ChunkLibrary& library, const world::WorldData& world)
    {
        ExteriorScene scene;
        scene.instances.reserve(library.chunks.size());
        for (std::uint32_t index = 0; index < library.chunks.size(); ++index)
        {
            const world::Chunk& chunk = library.chunks[index];
            if (chunk.cell >= library.cells.size() || chunk.material >= library.materials.size())
            {
                continue;
            }
            const world::Cell* cell = world.FindCell(util::Id::Of(library.cells[chunk.cell]));
            const std::string_view material = library.materials[chunk.material];
            if (cell == nullptr ||
                (cell->kind != world::CellKind::Exterior && !IsExteriorSkinMaterial(material)))
            {
                continue;
            }
            ExteriorInstance instance;
            instance.id = util::Id(index + 1u);
            instance.category = CategoryForMaterial(material);
            instance.bounds = chunk.bounds;
            scene.instances.push_back(instance);
        }
        scene.bvh.Build(scene.instances);
        return scene;
    }

    void GatherExteriorCones(const world::WorldData& world,
                             std::span<const VisibleCell> visible,
                             std::vector<ClipFrustum>& out)
    {
        out.clear();
        for (const VisibleCell& cell : visible)
        {
            const world::Cell* found = world.FindCell(cell.cell);
            if (found == nullptr || found->kind != world::CellKind::Exterior)
            {
                continue;
            }
            for (std::size_t index = 0; index < cell.frustumCount; ++index)
            {
                out.push_back(cell.frusta[index]);
            }
        }
    }

} // namespace cnahouse::visibility
