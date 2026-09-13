// SPDX-License-Identifier: MIT
#include "cnahouse/lighting/LightingSystem.hpp"

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/environment/MoonLight.hpp"
#include "cnahouse/environment/MoonModel.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/SunLight.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/lighting/PlanckianLut.hpp"
#include <algorithm>
#include <cmath>

#include "cnahouse/app/FrameTimer.hpp"
#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::lighting
{

    LightingSystem::LightingSystem(const world::WorldData& world,
                                   const ShadingGrid& shading,
                                   const environment::SimClock& clock,
                                   std::span<const visibility::PortalRuntime> portals)
        : daylight_(world, shading)
        , borrowed_(world, portals)
        , clock_(&clock)
        , cloudCover_(world.GetInitialState().weather.state.cloudCover)
    {
        // One state per cell, in the world's order, so a consumer can iterate `Cells()` beside any
        // other per-cell array the world produced without a lookup.
        const std::span<const world::Cell> worldCells = world.Cells();
        cells_.reserve(worldCells.size());
        outdoorCells_.reserve(worldCells.size());
        daylightLevels_.resize(worldCells.size());
        borrowedLevels_.resize(worldCells.size());
        cellGroups_.reserve(worldCells.size());
        cellIndex_.reserve(worldCells.size());
        for (const world::Cell& cell : worldCells)
        {
            cellIndex_.emplace(cell.id.Value(), cells_.size());
            RoomLightState state;
            state.cell = cell.id;
            cells_.push_back(state);
            // `CellKind::Exterior` alone includes the enclosed shed; `VisibilityHint::Open` alone
            // includes indoor stair wells. Together they are the 17 sky-open outdoor cells.
            outdoorCells_.push_back(cell.kind == world::CellKind::Exterior &&
                                    cell.visibilityHint == world::VisibilityHint::Open);
        }

        // The groups come from the LIGHTS: a group's default state is a property of its fixtures,
        // and its lumens are their sum. A group named by a cell but owning no fixture would light
        // nothing, and is left out rather than given a zero-lumen entry that divides badly later.
        for (const world::Light& light : world.Lights())
        {
            if (!light.group.IsValid())
            {
                continue;
            }
            const auto found = groupIndex_.find(light.group.Value());
            if (found == groupIndex_.end())
            {
                groupIndex_.emplace(light.group.Value(), groups_.size());
                SwitchGroupState group;
                group.group = light.group;
                group.on = light.defaultOn;
                groups_.push_back(group);
                const float lumens = std::max(light.intensityLm, 0.0F);
                groupLumens_.push_back(lumens);
                const auto color = PlanckianRgb(light.colorK);
                groupColors_.emplace_back(color.X * lumens, color.Y * lumens, color.Z * lumens);
            }
            else
            {
                // §28.2: a group is a set of fixtures on one switch. They agree about `defaultOn`
                // in the authored data; where they would not, ON wins, because a switch that is on
                // lights every fixture it controls and a disagreement is a data defect that should
                // be visible rather than silently resolved to off.
                groups_[found->second].on = groups_[found->second].on || light.defaultOn;
                const float lumens = std::max(light.intensityLm, 0.0F);
                groupLumens_[found->second] += lumens;
                const auto color = PlanckianRgb(light.colorK);
                auto& accumulated = groupColors_[found->second];
                accumulated.X += color.X * lumens;
                accumulated.Y += color.Y * lumens;
                accumulated.Z += color.Z * lumens;
            }
        }
        for (std::size_t index = 0; index < groupColors_.size(); ++index)
        {
            const float lumens = groupLumens_[index];
            if (lumens > 0.0F)
            {
                groupColors_[index].X /= lumens;
                groupColors_[index].Y /= lumens;
                groupColors_[index].Z /= lumens;
            }
        }

        // The cells' group lists, packed. Only groups that actually own a fixture are kept, and
        // the lumens are totalled once because the denominator of `artificial` never changes.
        for (std::size_t index = 0; index < worldCells.size(); ++index)
        {
            CellGroups packed;
            packed.first = cellGroupIds_.size();
            for (const util::Id& group : worldCells[index].lightGroups)
            {
                const auto found = groupIndex_.find(group.Value());
                if (found == groupIndex_.end())
                {
                    continue;
                }
                cellGroupIds_.push_back(group);
                packed.totalLumens += groupLumens_[found->second];
            }
            packed.count = cellGroupIds_.size() - packed.first;
            cellGroups_.push_back(packed);
        }
    }

    void LightingSystem::Update(const app::FrameContext& frame)
    {
        computedForFrame_ = frame.frameIndex;
        sun_ = environment::SunPositionFor(*clock_);
        moon_ = environment::MoonPositionFor(*clock_);
        moonPhase_ = environment::MoonPhaseFor(*clock_, moon_, sun_);
        const environment::SunShading shading = environment::SunShadingFor(sun_, cloudCover_);
        sunKey_.direction = environment::SunDirection(sun_);
        sunKey_.diffuseColor = Microsoft::Xna::Framework::Vector3(shading.color.X * shading.directIntensity,
                                                                  shading.color.Y * shading.directIntensity,
                                                                  shading.color.Z * shading.directIntensity);
        const environment::MoonShading moonShading =
            environment::MoonShadingFor(moon_, moonPhase_, cloudCover_);
        moonKey_.direction = environment::MoonDirection(moon_);
        moonKey_.diffuseColor =
            Microsoft::Xna::Framework::Vector3(moonShading.color.X * moonShading.intensity,
                                               moonShading.color.Y * moonShading.intensity,
                                               moonShading.color.Z * moonShading.intensity);
        moonKeyActive_ = moonShading.intensity > 0.0F;
        sunComputed_ = true;

        daylight_.Evaluate(sun_.altitudeDeg, sun_.azimuthDeg, cloudCover_, daylightLevels_);
        for (std::size_t index = 0; index < cells_.size(); ++index)
        {
            cells_[index].daylight = daylightLevels_[index];
            const CellGroups& packed = cellGroups_[index];
            float lit = 0.0F;
            Microsoft::Xna::Framework::Vector3 colorLumens;
            for (std::size_t offset = 0; offset < packed.count; ++offset)
            {
                const util::Id group = cellGroupIds_[packed.first + offset];
                const auto found = groupIndex_.find(group.Value());
                const std::size_t groupIndex = found->second;
                const float contribution = groups_[groupIndex].Level() * groupLumens_[groupIndex];
                lit += contribution;
                colorLumens.X += groupColors_[groupIndex].X * contribution;
                colorLumens.Y += groupColors_[groupIndex].Y * contribution;
                colorLumens.Z += groupColors_[groupIndex].Z * contribution;
            }
            // Lumen-weighted, so a kitchen's four 1 200 lm down-lights and its one 60 lm cabinet
            // strip contribute what they actually emit. A room with no fixtures is 0 and not a
            // division by zero.
            cells_[index].artificial =
                packed.totalLumens > 0.0F ? std::clamp(lit / packed.totalLumens, 0.0F, 1.0F) : 0.0F;
            cells_[index].artificialColor =
                lit > 0.0F ? Microsoft::Xna::Framework::Vector3(
                                 colorLumens.X / lit, colorLumens.Y / lit, colorLumens.Z / lit)
                           : Microsoft::Xna::Framework::Vector3();
        }
        borrowed_.Evaluate(cells_, borrowedLevels_);
        for (std::size_t index = 0; index < cells_.size(); ++index)
        {
            cells_[index].borrowed = borrowedLevels_[index];
        }
    }

    bool LightingSystem::SetGroupOn(util::Id group, bool on) noexcept
    {
        SwitchGroupState* state = FindGroupMutable(group);
        if (state == nullptr)
        {
            return false;
        }
        state->on = on;
        return true;
    }

    bool LightingSystem::SetGroupDimmer(util::Id group, float dimmer) noexcept
    {
        if (!std::isfinite(dimmer))
        {
            // A settings file and a console command are both user-editable text. A NaN dimmer would
            // make one room's level NaN and everything that read it, silently.
            return false;
        }
        SwitchGroupState* state = FindGroupMutable(group);
        if (state == nullptr)
        {
            return false;
        }
        state->dimmer = std::clamp(dimmer, 0.0F, 1.0F);
        return true;
    }

    const SwitchGroupState* LightingSystem::FindGroup(util::Id group) const noexcept
    {
        const auto found = groupIndex_.find(group.Value());
        return found == groupIndex_.end() ? nullptr : &groups_[found->second];
    }

    SwitchGroupState* LightingSystem::FindGroupMutable(util::Id group) noexcept
    {
        const auto found = groupIndex_.find(group.Value());
        return found == groupIndex_.end() ? nullptr : &groups_[found->second];
    }

    const RoomLightState* LightingSystem::FindCell(util::Id cell) const noexcept
    {
        const auto found = cellIndex_.find(cell.Value());
        return found == cellIndex_.end() ? nullptr : &cells_[found->second];
    }

    std::span<const util::Id> LightingSystem::GroupsForCell(util::Id cell) const noexcept
    {
        const auto found = cellIndex_.find(cell.Value());
        if (found == cellIndex_.end())
        {
            return {};
        }
        const CellGroups& packed = cellGroups_[found->second];
        return std::span<const util::Id>(cellGroupIds_.data() + packed.first, packed.count);
    }

    float LightingSystem::GroupLevelInCell(util::Id cell, util::Id group) const noexcept
    {
        const std::span<const util::Id> lighting = GroupsForCell(cell);
        if (std::find(lighting.begin(), lighting.end(), group) == lighting.end())
        {
            return 0.0F;
        }
        const SwitchGroupState* state = FindGroup(group);
        return state == nullptr ? 0.0F : state->Level();
    }

    float LightingSystem::GroupLumens(util::Id group) const noexcept
    {
        const auto found = groupIndex_.find(group.Value());
        return found == groupIndex_.end() ? 0.0F : groupLumens_[found->second];
    }

    Microsoft::Xna::Framework::Vector3 LightingSystem::GroupColor(util::Id group) const noexcept
    {
        const auto found = groupIndex_.find(group.Value());
        return found == groupIndex_.end() ? Microsoft::Xna::Framework::Vector3()
                                          : groupColors_[found->second];
    }

    bool LightingSystem::SetCloudCover(float cloudCover) noexcept
    {
        if (!std::isfinite(cloudCover))
        {
            return false;
        }
        cloudCover_ = std::clamp(cloudCover, 0.0F, 1.0F);
        return true;
    }

    const CelestialKeyLight* LightingSystem::SunKeyForCell(util::Id cell) const noexcept
    {
        const auto found = cellIndex_.find(cell.Value());
        if (found == cellIndex_.end() || !sunComputed_ ||
            sun_.altitudeDeg < environment::kRefractedHorizonDeg)
        {
            return nullptr;
        }
        const std::size_t index = found->second;
        return outdoorCells_[index] || cells_[index].DaylightIsKey() ? &sunKey_ : nullptr;
    }

    const CelestialKeyLight* LightingSystem::MoonKeyForCell(util::Id cell) const noexcept
    {
        const auto found = cellIndex_.find(cell.Value());
        if (found == cellIndex_.end() || !sunComputed_ || !environment::MoonlightMayBeKey(sun_.altitudeDeg) ||
            !moonKeyActive_)
        {
            return nullptr;
        }
        return outdoorCells_[found->second] ? &moonKey_ : nullptr;
    }

    const CelestialKeyLight* LightingSystem::CelestialKeyForCell(util::Id cell) const noexcept
    {
        if (const CelestialKeyLight* sun = SunKeyForCell(cell); sun != nullptr)
        {
            return sun;
        }
        return MoonKeyForCell(cell);
    }

} // namespace cnahouse::lighting
