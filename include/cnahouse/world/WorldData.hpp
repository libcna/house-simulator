// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cnahouse/util/Result.hpp"
#include "cnahouse/world/WorldTypes.hpp"

/// @file
/// `WorldData` -- the whole house, in memory, immutable (`HOUSE-00342`, `cna-house.md` §15).

namespace cnahouse::world
{
    namespace detail
    {
        /// @brief `[first, first + count)` into one of `WorldData`'s flat per-cell arrays.
        ///
        /// In `detail` rather than nested in the class because the indexing helpers that build it
        /// live in an anonymous namespace in the translation unit, where a private nested type is
        /// not reachable -- and making it public would put a storage detail into the API that every
        /// subsystem reads.
        struct CellRange
        {
            std::uint32_t first = 0;
            std::uint32_t count = 0;
        };
    } // namespace detail

    /// @brief The loaded world: eleven row tables plus the indices every subsystem asks for.
    ///
    /// **Immutable by construction, not by convention.** `WorldLoader` (`HOUSE-00343`…) fills a
    /// `WorldData::Contents` and hands it to `Create`, which validates the ids, builds the indices
    /// and moves the rows in. After that there is no non-const accessor and no way to add a row.
    /// That is what lets every other system hold a `const WorldData&` and a raw index into it for
    /// the life of the process: the visibility solver's per-cell portal list, the audio solver's
    /// per-cell zone, and the physics step's cell lookup are all pointers into vectors that will
    /// not move.
    ///
    /// **What `Create` checks and what it does not.** It checks the two things an index cannot be
    /// built without: that no id appears twice, and that a row's id is not zero. Everything else --
    /// that a portal's cells exist, that its rectangle is in their plane, that the graph is
    /// connected -- is `validate_world.py`'s eleven rules (`HOUSE-00358`) and `WorldValidator`'s
    /// (`HOUSE-00357`). Repeating them here would be a third copy that can disagree with the other
    /// two, and the loader is not the place to discover that a house is unwalkable.
    ///
    /// **Lookup by id is a hash map; lookup by point is not here.** A point-to-cell query runs on
    /// every physics step and every camera move, and §16.4 answers it with an incremental test, a
    /// neighbour walk and a 2 m grid -- that is `SpatialIndex` (`HOUSE-00356`). What this class
    /// provides is the primitive those three steps are built from: `CellContains`.
    class WorldData
    {
    public:
        /// @brief The rows, before they are indexed. The loader's output and `Create`'s input.
        ///
        /// A separate type rather than a half-built `WorldData` so that "a world under
        /// construction" and "a world" are different types and cannot be confused: there is no
        /// moment at which a `WorldData` exists with an index that does not match its rows.
        struct Contents
        {
            std::vector<Level> levels;
            Construction construction;
            std::vector<HvacBranch> hvac;
            std::vector<PlumbingStack> plumbing;
            std::vector<Cell> cells;
            std::vector<Portal> portals;
            std::vector<Opening> openings;
            std::vector<StairFlight> stairs;
            std::vector<Light> lights;
            std::vector<Material> materials;
            std::vector<Prop> props;
            std::vector<NavNode> navNodes;
            std::vector<NavEdge> navEdges;
            std::vector<NavMarker> navMarkers;
            std::vector<NavForbidden> navForbidden;
            std::vector<AudioZone> audioZones;
            std::vector<AudioEmitter> audioEmitters;
            std::vector<AudioTransmission> audioTransmission;
            std::vector<Interactable> interactables;
            InitialState initialState;
            Exterior exterior;
        };

        /// @brief Indexes @p contents and takes ownership of it.
        ///
        /// @return `Duplicate` naming both rows when an id appears twice, `InvalidData` when a row
        ///         has no id at all. Ids are unique across every kind, not within one
        ///         (`docs/world-format.md`), because a reference in the data does not carry the
        ///         kind it points at.
        [[nodiscard]] static util::Result<WorldData> Create(Contents&& contents);

        WorldData(const WorldData&) = delete;
        WorldData& operator=(const WorldData&) = delete;
        WorldData(WorldData&&) noexcept = default;
        WorldData& operator=(WorldData&&) noexcept = default;
        ~WorldData() = default;

        // --- the tables ---------------------------------------------------------------------------
        [[nodiscard]] std::span<const Level> Levels() const noexcept
        {
            return m_contents.levels;
        }

        [[nodiscard]] const Construction& GetConstruction() const noexcept
        {
            return m_contents.construction;
        }

        [[nodiscard]] std::span<const PlumbingStack> PlumbingStacks() const noexcept
        {
            return m_contents.plumbing;
        }

        [[nodiscard]] std::span<const HvacBranch> HvacBranches() const noexcept
        {
            return m_contents.hvac;
        }

        [[nodiscard]] std::span<const Cell> Cells() const noexcept
        {
            return m_contents.cells;
        }

        [[nodiscard]] std::span<const Portal> Portals() const noexcept
        {
            return m_contents.portals;
        }

        [[nodiscard]] std::span<const Opening> Openings() const noexcept
        {
            return m_contents.openings;
        }

        [[nodiscard]] std::span<const StairFlight> Stairs() const noexcept
        {
            return m_contents.stairs;
        }

        [[nodiscard]] std::span<const Light> Lights() const noexcept
        {
            return m_contents.lights;
        }

        [[nodiscard]] std::span<const Material> Materials() const noexcept
        {
            return m_contents.materials;
        }

        [[nodiscard]] std::span<const Prop> Props() const noexcept
        {
            return m_contents.props;
        }

        [[nodiscard]] std::span<const NavNode> NavNodes() const noexcept
        {
            return m_contents.navNodes;
        }

        [[nodiscard]] std::span<const NavEdge> NavEdges() const noexcept
        {
            return m_contents.navEdges;
        }

        [[nodiscard]] std::span<const NavMarker> NavMarkers() const noexcept
        {
            return m_contents.navMarkers;
        }

        /// @brief The cells a species must not enter. Named for the thing, not for the row type:
        /// a member function called `NavForbidden` would change what the *type* `NavForbidden`
        /// means inside the class, which `-Wchanges-meaning` rightly refuses.
        [[nodiscard]] std::span<const struct NavForbidden> ForbiddenZones() const noexcept
        {
            return m_contents.navForbidden;
        }

        [[nodiscard]] std::span<const AudioZone> AudioZones() const noexcept
        {
            return m_contents.audioZones;
        }

        [[nodiscard]] std::span<const AudioEmitter> AudioEmitters() const noexcept
        {
            return m_contents.audioEmitters;
        }

        [[nodiscard]] std::span<const AudioTransmission> AudioTransmissions() const noexcept
        {
            return m_contents.audioTransmission;
        }

        /// @brief The named loss pair, or null when the table does not have that kind.
        [[nodiscard]] const AudioTransmission* FindTransmission(std::string_view kind) const noexcept;

        [[nodiscard]] std::span<const Interactable> Interactables() const noexcept
        {
            return m_contents.interactables;
        }

        [[nodiscard]] const Interactable* FindInteractable(util::Id id) const noexcept;

        [[nodiscard]] const InitialState& GetInitialState() const noexcept
        {
            return m_contents.initialState;
        }

        [[nodiscard]] const Exterior& GetExterior() const noexcept
        {
            return m_contents.exterior;
        }

        // --- lookup by id -------------------------------------------------------------------------
        // Every one of these returns a pointer that is null when the id is unknown, rather than a
        // reference to a default row. A missing cell is a question the caller has to answer -- the
        // physics step clamps to the last good cell, the loader reports it as a load error -- and a
        // silent empty row would let both of them carry on with a room that has no floor.
        [[nodiscard]] const Level* FindLevel(util::Id id) const noexcept;
        [[nodiscard]] const Cell* FindCell(util::Id id) const noexcept;
        [[nodiscard]] const Portal* FindPortal(util::Id id) const noexcept;
        [[nodiscard]] const Opening* FindOpening(util::Id id) const noexcept;
        [[nodiscard]] const StairFlight* FindStair(util::Id id) const noexcept;
        [[nodiscard]] const Light* FindLight(util::Id id) const noexcept;
        [[nodiscard]] const Material* FindMaterial(util::Id id) const noexcept;
        [[nodiscard]] const Prop* FindProp(util::Id id) const noexcept;
        [[nodiscard]] const PlumbingStack* FindPlumbingStack(util::Id id) const noexcept;

        // --- the per-cell indices -----------------------------------------------------------------
        // Spans into one flat array each, so a cell's portals are contiguous and a walk over them
        // does not chase a pointer per row. Empty for an unknown cell, which is the same answer as
        // for a cell with none: a caller iterating a span does not need to distinguish them, and a
        // caller that does asks `FindCell` first.
        [[nodiscard]] std::span<const std::uint32_t> PortalsOf(util::Id cell) const noexcept;
        [[nodiscard]] std::span<const std::uint32_t> LightsOf(util::Id cell) const noexcept;
        [[nodiscard]] std::span<const std::uint32_t> PropsOf(util::Id cell) const noexcept;

        /// @brief The lights in one switch group.
        ///
        /// Grouped as well as celled, because the two questions are asked by different systems for
        /// different reasons: the renderer asks "what lights this cell", a switch asks "what does
        /// this group toggle", and a group crosses cells -- the stair-hall group lights three
        /// floors from one plate.
        [[nodiscard]] std::span<const std::uint32_t> LightsInGroup(util::Id group) const noexcept;

        /// @brief The cell on the other side of @p portal from @p cell, or an invalid id.
        [[nodiscard]] util::Id OtherSide(const Portal& portal, util::Id cell) const noexcept;

        // --- geometry -----------------------------------------------------------------------------

        /// @brief @p cell's floor and ceiling Y.
        ///
        /// `yOverride` wins; otherwise the level's `ffl` to its `ceiling`. A cell on a
        /// rafter-bounded level with no `yOverride` has no answer -- inventing one would put a
        /// ceiling through the roof -- so this returns `NotFound` rather than a plausible number.
        [[nodiscard]] util::Result<Extent> ExtentOf(const Cell& cell) const;

        /// @brief Is @p point inside @p cell, allowing @p margin of hysteresis?
        ///
        /// The margin is §16.4's 5 cm: without it a player standing exactly on a boundary flips
        /// between two cells every frame, and every system that keys off the current cell -- audio,
        /// residency, visibility -- churns with them.
        [[nodiscard]] bool CellContains(const Cell& cell,
                                        const Microsoft::Xna::Framework::Vector3& point,
                                        float margin = 0.0F) const;

        /// @brief The total footprint area of @p cell, in square metres.
        [[nodiscard]] static float FootprintArea(const Cell& cell) noexcept;

        [[nodiscard]] std::size_t Size() const noexcept;

    private:
        using Index = std::unordered_map<util::Id, std::uint32_t>;

        WorldData() = default;

        [[nodiscard]] static const std::uint32_t* Lookup(const Index& index, util::Id id) noexcept;

        Contents m_contents;

        Index m_levelIndex;
        Index m_cellIndex;
        Index m_portalIndex;
        Index m_openingIndex;
        Index m_stairIndex;
        Index m_lightIndex;
        Index m_materialIndex;
        Index m_propIndex;
        Index m_plumbingIndex;
        Index m_interactableIndex;

        /// Cell id -> the slice of the flat array below that belongs to it.
        std::unordered_map<util::Id, detail::CellRange> m_portalsOfCell;
        std::unordered_map<util::Id, detail::CellRange> m_lightsOfCell;
        std::unordered_map<util::Id, detail::CellRange> m_propsOfCell;
        std::unordered_map<util::Id, detail::CellRange> m_lightsOfGroup;
        std::vector<std::uint32_t> m_portalsByCell;
        std::vector<std::uint32_t> m_lightsByCell;
        std::vector<std::uint32_t> m_propsByCell;
        std::vector<std::uint32_t> m_lightsByGroup;
    };

} // namespace cnahouse::world
