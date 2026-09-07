// SPDX-License-Identifier: MIT
#include "cnahouse/world/WorldData.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace cnahouse::world
{
    namespace
    {
        /// @brief The name an id was interned from, or its hexadecimal value if it never was.
        ///
        /// A duplicate-id message that says `0x9a3f21c4 appears twice` is a message nobody can act
        /// on; `L0_KITCHEN appears twice` sends the author to the right line. The fallback exists
        /// because a test may build a `WorldData` from ids made with `Id::Of`, which does not
        /// register the name.
        [[nodiscard]] std::string Name(util::Id id)
        {
            const std::string_view name = util::IdRegistry::NameOf(id);
            if (!name.empty())
            {
                return std::string(name);
            }
            return "id " + std::to_string(id.Value());
        }

        /// @brief Adds every row's id to @p index and to @p seen, refusing a repeat.
        ///
        /// @p seen spans every kind, because `docs/world-format.md` requires ids to be unique
        /// across every kind rather than within one: a reference in the data is a bare id and does
        /// not carry the kind it points at, so a light group sharing a name with a material makes
        /// a resolved reference answer a question nobody asked.
        template<typename Row>
        [[nodiscard]] util::Result<void> BuildIndex(const std::vector<Row>& rows,
                                                    std::string_view what,
                                                    std::unordered_map<util::Id, std::uint32_t>& index,
                                                    std::unordered_map<util::Id, std::string>& seen)
        {
            index.reserve(rows.size());
            for (std::size_t position = 0; position < rows.size(); ++position)
            {
                const util::Id id = rows[position].id;
                if (!id.IsValid())
                {
                    return util::Err(util::ErrorCode::InvalidData,
                                     std::string("a ") + std::string(what) + " row has no id",
                                     std::string(what) + "[" + std::to_string(position) + "]");
                }
                const auto existing = seen.find(id);
                if (existing != seen.end())
                {
                    return util::Err(util::ErrorCode::Duplicate,
                                     Name(id) + " is already used by a " + existing->second +
                                         " row; ids are unique across every kind, not within one",
                                     std::string(what) + "[" + std::to_string(position) + "]");
                }
                seen.emplace(id, std::string(what));
                index.emplace(id, static_cast<std::uint32_t>(position));
            }
            return util::Ok();
        }

        /// @brief Groups row indices by the cell they belong to, into one flat array plus ranges.
        ///
        /// One array rather than a vector per cell: a house has 95 cells and a few thousand rows,
        /// and the visibility solver walks a cell's portals every frame. Contiguous indices are one
        /// cache line; ninety-five separate vectors are ninety-five allocations and a pointer chase
        /// per cell.
        template<typename Row, typename CellOf>
        void GroupByCell(const std::vector<Row>& rows,
                         CellOf cellOf,
                         std::vector<std::uint32_t>& flat,
                         std::unordered_map<util::Id, detail::CellRange>& ranges)
        {
            std::vector<std::pair<util::Id, std::uint32_t>> pairs;
            pairs.reserve(rows.size());
            for (std::size_t position = 0; position < rows.size(); ++position)
            {
                const util::Id cell = cellOf(rows[position]);
                if (cell.IsValid())
                {
                    pairs.emplace_back(cell, static_cast<std::uint32_t>(position));
                }
            }

            // By cell, then by original row order inside a cell. The second half matters: the order
            // rows are drawn and solved in is the order they were authored, so a diff of a frame
            // capture reflects a change to the data and not a change to a hash seed.
            std::stable_sort(pairs.begin(),
                             pairs.end(),
                             [](const auto& a, const auto& b) { return a.first.Value() < b.first.Value(); });

            flat.clear();
            flat.reserve(pairs.size());
            ranges.clear();
            for (const auto& [cell, position] : pairs)
            {
                auto entry = ranges.find(cell);
                if (entry == ranges.end())
                {
                    ranges.emplace(cell, detail::CellRange{static_cast<std::uint32_t>(flat.size()), 1U});
                }
                else
                {
                    ++entry->second.count;
                }
                flat.push_back(position);
            }
        }
    } // namespace

    util::Result<WorldData> WorldData::Create(Contents&& contents)
    {
        WorldData world;
        world.m_contents = std::move(contents);

        std::unordered_map<util::Id, std::string> seen;
        const auto& rows = world.m_contents;

        // Order is the order of `cna-house.md` §15.1's table, so that the FIRST duplicate reported
        // for a pair of files is always the same one whichever machine ran the load.
        if (auto step = BuildIndex(rows.levels, "level", world.m_levelIndex, seen); !step)
        {
            return step.Error();
        }
        if (auto step = BuildIndex(rows.cells, "cell", world.m_cellIndex, seen); !step)
        {
            return step.Error();
        }
        if (auto step = BuildIndex(rows.portals, "portal", world.m_portalIndex, seen); !step)
        {
            return step.Error();
        }
        if (auto step = BuildIndex(rows.openings, "opening", world.m_openingIndex, seen); !step)
        {
            return step.Error();
        }
        if (auto step = BuildIndex(rows.stairs, "stair flight", world.m_stairIndex, seen); !step)
        {
            return step.Error();
        }
        if (auto step = BuildIndex(rows.lights, "light", world.m_lightIndex, seen); !step)
        {
            return step.Error();
        }
        if (auto step = BuildIndex(rows.materials, "material", world.m_materialIndex, seen); !step)
        {
            return step.Error();
        }
        if (auto step = BuildIndex(rows.props, "prop", world.m_propIndex, seen); !step)
        {
            return step.Error();
        }
        if (auto step = BuildIndex(rows.plumbing, "plumbing stack", world.m_plumbingIndex, seen); !step)
        {
            return step.Error();
        }

        GroupByCell(
            world.m_contents.lights,
            [](const Light& row) { return row.cell; },
            world.m_lightsByCell,
            world.m_lightsOfCell);
        GroupByCell(
            world.m_contents.props,
            [](const Prop& row) { return row.cell; },
            world.m_propsByCell,
            world.m_propsOfCell);
        // Lights are grouped twice, by cell and by switch group, because the two questions are
        // asked by different systems for different reasons: the renderer asks "what lights this
        // cell", a switch asks "what does this group toggle", and a group crosses cells -- the
        // stair-hall group lights three floors from one plate.
        GroupByCell(
            world.m_contents.lights,
            [](const Light& row) { return row.group; },
            world.m_lightsByGroup,
            world.m_lightsOfGroup);

        // A portal belongs to BOTH its cells, so it is grouped twice. Every caller of `PortalsOf`
        // -- the visibility solver, the nav walk, the audio transmission solve -- asks "what leads
        // out of this cell", and a portal listed only under `cellA` would be invisible from the
        // room on the other side of it.
        {
            std::vector<std::pair<util::Id, std::uint32_t>> pairs;
            pairs.reserve(world.m_contents.portals.size() * 2U);
            for (std::size_t position = 0; position < world.m_contents.portals.size(); ++position)
            {
                const Portal& portal = world.m_contents.portals[position];
                for (const util::Id side : {portal.cellA, portal.cellB})
                {
                    if (side.IsValid())
                    {
                        pairs.emplace_back(side, static_cast<std::uint32_t>(position));
                    }
                }
            }
            std::stable_sort(pairs.begin(),
                             pairs.end(),
                             [](const auto& a, const auto& b) { return a.first.Value() < b.first.Value(); });
            world.m_portalsByCell.reserve(pairs.size());
            for (const auto& [cell, position] : pairs)
            {
                auto entry = world.m_portalsOfCell.find(cell);
                if (entry == world.m_portalsOfCell.end())
                {
                    world.m_portalsOfCell.emplace(
                        cell,
                        detail::CellRange{static_cast<std::uint32_t>(world.m_portalsByCell.size()), 1U});
                }
                else
                {
                    ++entry->second.count;
                }
                world.m_portalsByCell.push_back(position);
            }
        }

        return world;
    }

    const std::uint32_t* WorldData::Lookup(const Index& index, util::Id id) noexcept
    {
        const auto entry = index.find(id);
        return entry == index.end() ? nullptr : &entry->second;
    }

    const Level* WorldData::FindLevel(util::Id id) const noexcept
    {
        const std::uint32_t* at = Lookup(m_levelIndex, id);
        return at == nullptr ? nullptr : &m_contents.levels[*at];
    }

    const Cell* WorldData::FindCell(util::Id id) const noexcept
    {
        const std::uint32_t* at = Lookup(m_cellIndex, id);
        return at == nullptr ? nullptr : &m_contents.cells[*at];
    }

    const Portal* WorldData::FindPortal(util::Id id) const noexcept
    {
        const std::uint32_t* at = Lookup(m_portalIndex, id);
        return at == nullptr ? nullptr : &m_contents.portals[*at];
    }

    const Opening* WorldData::FindOpening(util::Id id) const noexcept
    {
        const std::uint32_t* at = Lookup(m_openingIndex, id);
        return at == nullptr ? nullptr : &m_contents.openings[*at];
    }

    const StairFlight* WorldData::FindStair(util::Id id) const noexcept
    {
        const std::uint32_t* at = Lookup(m_stairIndex, id);
        return at == nullptr ? nullptr : &m_contents.stairs[*at];
    }

    const Light* WorldData::FindLight(util::Id id) const noexcept
    {
        const std::uint32_t* at = Lookup(m_lightIndex, id);
        return at == nullptr ? nullptr : &m_contents.lights[*at];
    }

    const Material* WorldData::FindMaterial(util::Id id) const noexcept
    {
        const std::uint32_t* at = Lookup(m_materialIndex, id);
        return at == nullptr ? nullptr : &m_contents.materials[*at];
    }

    const Prop* WorldData::FindProp(util::Id id) const noexcept
    {
        const std::uint32_t* at = Lookup(m_propIndex, id);
        return at == nullptr ? nullptr : &m_contents.props[*at];
    }

    const PlumbingStack* WorldData::FindPlumbingStack(util::Id id) const noexcept
    {
        const std::uint32_t* at = Lookup(m_plumbingIndex, id);
        return at == nullptr ? nullptr : &m_contents.plumbing[*at];
    }

    namespace
    {
        [[nodiscard]] std::span<const std::uint32_t>
        Slice(const std::unordered_map<util::Id, detail::CellRange>& ranges,
              const std::vector<std::uint32_t>& flat,
              util::Id cell) noexcept
        {
            const auto entry = ranges.find(cell);
            if (entry == ranges.end())
            {
                return {};
            }
            return std::span<const std::uint32_t>(flat).subspan(entry->second.first, entry->second.count);
        }
    } // namespace

    std::span<const std::uint32_t> WorldData::PortalsOf(util::Id cell) const noexcept
    {
        return Slice(m_portalsOfCell, m_portalsByCell, cell);
    }

    std::span<const std::uint32_t> WorldData::LightsOf(util::Id cell) const noexcept
    {
        return Slice(m_lightsOfCell, m_lightsByCell, cell);
    }

    std::span<const std::uint32_t> WorldData::PropsOf(util::Id cell) const noexcept
    {
        return Slice(m_propsOfCell, m_propsByCell, cell);
    }

    std::span<const std::uint32_t> WorldData::LightsInGroup(util::Id group) const noexcept
    {
        return Slice(m_lightsOfGroup, m_lightsByGroup, group);
    }

    util::Id WorldData::OtherSide(const Portal& portal, util::Id cell) const noexcept
    {
        if (portal.cellA == cell)
        {
            return portal.cellB;
        }
        if (portal.cellB == cell)
        {
            return portal.cellA;
        }
        return {};
    }

    util::Result<Extent> WorldData::ExtentOf(const Cell& cell) const
    {
        if (cell.yOverride.has_value())
        {
            return *cell.yOverride;
        }
        const Level* level = FindLevel(cell.level);
        if (level == nullptr)
        {
            return util::Err(util::ErrorCode::NotFound,
                             Name(cell.id) + " is on level " + Name(cell.level) +
                                 ", which is not in layout.levels.json",
                             "cell " + Name(cell.id));
        }
        if (!level->ceiling.has_value())
        {
            return util::Err(util::ErrorCode::InvalidData,
                             Name(cell.id) + " is on level " + Name(level->id) +
                                 ", whose ceiling is null because it is bounded by rafters, so "
                                 "the cell must declare its own yOverride",
                             "cell " + Name(cell.id));
        }
        return Extent{level->ffl, *level->ceiling};
    }

    bool WorldData::CellContains(const Cell& cell,
                                 const Microsoft::Xna::Framework::Vector3& point,
                                 float margin) const
    {
        const util::Result<Extent> extent = ExtentOf(cell);
        if (!extent)
        {
            return false;
        }
        if (!extent.Value().Contains(point.Y, margin))
        {
            return false;
        }
        return std::any_of(cell.boxes.begin(),
                           cell.boxes.end(),
                           [&](const Footprint& box) { return box.Contains(point.X, point.Z, margin); });
    }

    float WorldData::FootprintArea(const Cell& cell) noexcept
    {
        float total = 0.0F;
        for (const Footprint& box : cell.boxes)
        {
            total += box.Area();
        }
        return total;
    }

    std::size_t WorldData::Size() const noexcept
    {
        return m_contents.levels.size() + m_contents.cells.size() + m_contents.portals.size() +
               m_contents.openings.size() + m_contents.stairs.size() + m_contents.lights.size() +
               m_contents.materials.size() + m_contents.props.size() + m_contents.navNodes.size() +
               m_contents.navEdges.size() + m_contents.navMarkers.size() + m_contents.audioZones.size() +
               m_contents.audioEmitters.size();
    }

} // namespace cnahouse::world
