// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/app/ISystem.hpp"
#include "cnahouse/environment/MoonLight.hpp"
#include "cnahouse/environment/SunLight.hpp"
#include "cnahouse/lighting/BorrowedLightModel.hpp"
#include "cnahouse/lighting/DaylightModel.hpp"
#include "cnahouse/lighting/RoomLightState.hpp"
#include "cnahouse/util/Ids.hpp"

namespace cnahouse::environment
{
    struct SimClock;
}

namespace cnahouse::world
{
    class WorldData;
}

namespace cnahouse::rendering
{
    struct SkyColourModel;
}

namespace cnahouse::lighting
{

    /// @brief Celestial values copied into XNA's `DirectionalLight0` for an eligible object.
    ///
    /// XNA has no separate intensity parameter: the LUT colour is multiplied by the direct-beam
    /// intensity here, once, and the consumer writes @c diffuseColor to `DiffuseColor`. Keeping
    /// this value-shaped also makes `HOUSE-01261`'s later per-object assignment independent of an
    /// effect instance.
    struct CelestialKeyLight
    {
        Microsoft::Xna::Framework::Vector3 direction{0.0F, -1.0F, 0.0F};
        Microsoft::Xna::Framework::Vector3 diffuseColor{0.0F, 0.0F, 0.0F};
    };

    /// @brief §28.1's per-frame loop, at `UpdateStage::Lighting` (`HOUSE-01251`).
    ///
    /// **One state per cell, recomputed from the switch groups, and everything downstream reads
    /// the same answer.** The same argument `VisibilitySystem` makes: §23.3's additive passes,
    /// §28.5's per-object light assignment, §28.6's emissive materials and §30's observable
    /// behaviour all read a room's level, and if any of them recomputed it they could disagree
    /// within one frame about how bright a room is.
    ///
    /// This is §28.1's loop with `artificial` from the switch groups and `daylight` from
    /// `DaylightModel`. `HOUSE-01255` supplies the lumen-weighted Planckian artificial colour.
    /// `borrowed` is the bounded 2-hop flood through the live portal apertures (`HOUSE-01265`).
    ///
    /// **Nothing here allocates after `Build`.** The cells and the groups are fixed for the
    /// session — §15's data is const once loaded — so the states, the group table and the index
    /// maps are sized once and then only written to.
    class LightingSystem final : public app::ISystem
    {
    public:
        /// @brief Takes the world, baked window shading, §35's clock and live portal apertures.
        ///
        /// All four outlive the system. Groups are discovered from the LIGHTS and not from the
        /// cells' `lightGroups` lists, because a group's default state is a property of its
        /// fixtures; the cell lists say which groups light which room and are read for that. The
        /// starting cloud cover comes from the world's `initialstate.json`.
        LightingSystem(const world::WorldData& world,
                       const ShadingGrid& shading,
                       const environment::SimClock& clock,
                       std::span<const visibility::PortalRuntime> portals);

        /// @brief The normal runtime constructor, additionally sharing §31's authored sky model.
        ///
        /// The value is copied once because the renderer takes ownership of its own copy; only its
        /// 32 gradient rows and small scalar tables are retained, and no allocation occurs during
        /// `Update`. The four-argument overload remains the no-sky fallback for content failures.
        LightingSystem(const world::WorldData& world,
                       const ShadingGrid& shading,
                       const environment::SimClock& clock,
                       std::span<const visibility::PortalRuntime> portals,
                       const rendering::SkyColourModel& skyColourModel);

        ~LightingSystem();

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

        /// @brief Lumen-weighted Planckian hue of a group's fixtures; black for an unknown group.
        [[nodiscard]] Microsoft::Xna::Framework::Vector3 GroupColor(util::Id group) const noexcept;

        /// @brief Change the continuous weather input used by both daylight and the direct beam.
        ///
        /// Clamped to `[0, 1]`; a non-finite value is refused rather than poisoning every room.
        bool SetCloudCover(float cloudCover) noexcept;

        [[nodiscard]] float CloudCover() const noexcept
        {
            return cloudCover_;
        }

        /// @brief The current solar position, recomputed from §35's clock by every `Update`.
        [[nodiscard]] const environment::SunPosition& Sun() const noexcept
        {
            return sun_;
        }

        /// @brief The current lunar position and phase, evaluated at the same instant as `Sun()`.
        [[nodiscard]] const environment::MoonPosition& Moon() const noexcept
        {
            return moon_;
        }

        [[nodiscard]] const environment::MoonPhase& LunarPhase() const noexcept
        {
            return moonPhase_;
        }

        /// @brief The shared `DirectionalLight0` values, or null when the cell should use a bulb.
        ///
        /// Outdoors takes the sun whenever it is above the refracted horizon. Indoors takes it
        /// only when §28.5's daylight threshold is met. Unknown cells and night return null;
        /// `HOUSE-01261` supplies the fixture key/fill/bounce alternatives later.
        [[nodiscard]] const CelestialKeyLight* SunKeyForCell(util::Id cell) const noexcept;

        /// @brief §33.4's moon key, only for a sky-open exterior cell on a moonlit night.
        [[nodiscard]] const CelestialKeyLight* MoonKeyForCell(util::Id cell) const noexcept;

        /// @brief The one celestial key for @p cell: daylight sun first, then night-time moon.
        [[nodiscard]] const CelestialKeyLight* CelestialKeyForCell(util::Id cell) const noexcept;

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
        std::vector<Microsoft::Xna::Framework::Vector3> groupColors_;
        /// @brief Every cell's group ids, packed end to end; `CellGroups` indexes into it.
        std::vector<util::Id> cellGroupIds_;
        std::vector<CellGroups> cellGroups_;
        std::unordered_map<std::uint32_t, std::size_t> cellIndex_;
        std::unordered_map<std::uint32_t, std::size_t> groupIndex_;
        std::vector<bool> outdoorCells_;
        std::vector<float> daylightLevels_;
        std::vector<float> borrowedLevels_;
        std::unique_ptr<rendering::SkyColourModel> skyColourModel_;
        DaylightModel daylight_;
        BorrowedLightModel borrowed_;
        const environment::SimClock* clock_ = nullptr;
        environment::SunPosition sun_;
        environment::MoonPosition moon_;
        environment::MoonPhase moonPhase_;
        CelestialKeyLight sunKey_;
        CelestialKeyLight moonKey_;
        float cloudCover_ = 0.0F;
        bool sunComputed_ = false;
        bool moonKeyActive_ = false;
        std::uint64_t computedForFrame_ = 0;
    };

} // namespace cnahouse::lighting
