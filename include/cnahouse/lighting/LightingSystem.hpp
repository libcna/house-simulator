// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cnahouse/app/ISystem.hpp"
#include "cnahouse/lighting/RoomLightState.hpp"
#include "cnahouse/util/Ids.hpp"

namespace cnahouse::world
{
    class WorldData;
}

namespace cnahouse::lighting
{

    /// @brief §28.1's per-frame loop, at `UpdateStage::Lighting` (`HOUSE-01251`).
    ///
    /// **One state per cell, recomputed from the switch groups, and everything downstream reads
    /// the same answer.** The same argument `VisibilitySystem` makes: §23.3's additive passes,
    /// §28.5's per-object light assignment, §28.6's emissive materials and §30's observable
    /// behaviour all read a room's level, and if any of them recomputed it they could disagree
    /// within one frame about how bright a room is.
    ///
    /// This is §28.1's loop with two of its four lines filled in — `artificial` from the switch
    /// groups, and the combination in `RoomLightState::Level`. `daylight` is `HOUSE-01263`'s,
    /// `borrowed` is `HOUSE-01265`'s and `ambientColor` needs `HOUSE-01255`'s Planckian
    /// conversion; each is a **field left at zero and named**, never a plausible number nothing
    /// computed.
    ///
    /// **Nothing here allocates after `Build`.** The cells and the groups are fixed for the
    /// session — §15's data is const once loaded — so the states, the group table and the index
    /// maps are sized once and then only written to.
    class LightingSystem final : public app::ISystem
    {
    public:
        /// @brief Takes the world: one `RoomLightState` per cell, one `SwitchGroupState` per group.
        ///
        /// The world outlives the system. Groups are discovered from the LIGHTS and not from the
        /// cells' `lightGroups` lists, because a group's default state is a property of its
        /// fixtures; the cell lists say which groups light which room and are read for that.
        explicit LightingSystem(const world::WorldData& world);

        [[nodiscard]] app::UpdateStage Stage() const noexcept override
        {
            return app::UpdateStage::Lighting;
        }

        [[nodiscard]] std::string_view Name() const noexcept override
        {
            return "lighting";
        }

        /// @brief §28.1's loop. Cheap and unconditional: 96 cells over 135 groups is nothing, and
        ///        a dirty flag would be a second thing that can be wrong.
        void Update(const app::FrameContext& frame) override;

        /// @brief Throws the switch on a group. Returns false when there is no such group.
        bool SetGroupOn(util::Id group, bool on) noexcept;

        /// @brief §28.1's `fixtureDimmer(g)`. Clamped to `[0, 1]`; a non-finite value is refused.
        bool SetGroupDimmer(util::Id group, float dimmer) noexcept;

        [[nodiscard]] const SwitchGroupState* FindGroup(util::Id group) const noexcept;

        [[nodiscard]] std::span<const SwitchGroupState> Groups() const noexcept
        {
            return groups_;
        }

        /// @brief The cell's state after the last `Update`, or `nullptr` for an unknown cell.
        [[nodiscard]] const RoomLightState* FindCell(util::Id cell) const noexcept;

        [[nodiscard]] std::span<const RoomLightState> Cells() const noexcept
        {
            return cells_;
        }

        /// @brief The groups that light @p cell, in the order `layout.cells.json` lists them.
        [[nodiscard]] std::span<const util::Id> GroupsForCell(util::Id cell) const noexcept;

        /// @brief The level one group contributes to @p cell right now, for §23.3's `LM_ART_<g>`
        ///        pass. Zero when the group does not light that cell.
        [[nodiscard]] float GroupLevelInCell(util::Id cell, util::Id group) const noexcept;

        /// @brief The frame `Update` last ran for, so a consumer can assert it is reading THIS
        ///        frame's answer rather than the previous one's.
        [[nodiscard]] std::uint64_t ComputedForFrame() const noexcept
        {
            return computedForFrame_;
        }

        /// @brief Total luminous flux of a group's fixtures, in lumens. The weight in `artificial`.
        [[nodiscard]] float GroupLumens(util::Id group) const noexcept;

    private:
        struct CellGroups
        {
            std::size_t first = 0;
            std::size_t count = 0;
            /// @brief Σ lumens over the cell's groups. The denominator, computed once.
            float totalLumens = 0.0F;
        };

        [[nodiscard]] SwitchGroupState* FindGroupMutable(util::Id group) noexcept;

        std::vector<RoomLightState> cells_;
        std::vector<SwitchGroupState> groups_;
        std::vector<float> groupLumens_;
        /// @brief Every cell's group ids, packed end to end; `CellGroups` indexes into it.
        std::vector<util::Id> cellGroupIds_;
        std::vector<CellGroups> cellGroups_;
        std::unordered_map<std::uint32_t, std::size_t> cellIndex_;
        std::unordered_map<std::uint32_t, std::size_t> groupIndex_;
        std::uint64_t computedForFrame_ = 0;
    };

} // namespace cnahouse::lighting
