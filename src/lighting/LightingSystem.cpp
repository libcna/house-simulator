// SPDX-License-Identifier: MIT
#include "cnahouse/lighting/LightingSystem.hpp"

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/environment/MoonLight.hpp"
#include "cnahouse/environment/MoonModel.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/SunLight.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/lighting/PlanckianLut.hpp"
#include "cnahouse/rendering/SkySystem.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

#include "cnahouse/app/FrameTimer.hpp"
#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::lighting
{
    namespace
    {
        using Microsoft::Xna::Framework::Vector3;

        Vector3 NormalizedOr(const Vector3& value, const Vector3& fallback) noexcept
        {
            const float lengthSquared = value.X * value.X + value.Y * value.Y + value.Z * value.Z;
            if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0e-8F)
            {
                return fallback;
            }
            const float inverse = 1.0F / std::sqrt(lengthSquared);
            return Vector3(value.X * inverse, value.Y * inverse, value.Z * inverse);
        }

        Vector3 DaylightFillDirection(world::Orientation orientation) noexcept
        {
            constexpr float kVertical = -0.35F;
            constexpr float kHorizontal = 0.9367497F;
            constexpr float kDiagonal = 0.662382F;
            switch (orientation)
            {
                case world::Orientation::N:
                    return Vector3(0.0F, kVertical, kHorizontal);
                case world::Orientation::NE:
                    return Vector3(-kDiagonal, kVertical, kDiagonal);
                case world::Orientation::E:
                    return Vector3(-kHorizontal, kVertical, 0.0F);
                case world::Orientation::SE:
                    return Vector3(-kDiagonal, kVertical, -kDiagonal);
                case world::Orientation::S:
                    return Vector3(0.0F, kVertical, -kHorizontal);
                case world::Orientation::SW:
                    return Vector3(kDiagonal, kVertical, -kDiagonal);
                case world::Orientation::W:
                    return Vector3(kHorizontal, kVertical, 0.0F);
                case world::Orientation::NW:
                    return Vector3(kDiagonal, kVertical, kDiagonal);
            }
            return Vector3(0.0F, -1.0F, 0.0F);
        }

        bool MinuteInWindow(int minute, int first, int last) noexcept
        {
            return first <= last ? minute >= first && minute < last : minute >= first || minute < last;
        }
    } // namespace

    int DuskSensorOffsetMinutes(util::Id fixture) noexcept
    {
        constexpr std::uint32_t kSlots = 17u;
        constexpr int kHalfRange = 8;
        return static_cast<int>(fixture.Value() % kSlots) - kHalfRange;
    }

    bool DuskSensorOn(const environment::SimClock& clock, util::Id fixture) noexcept
    {
        constexpr double kDuskAltitudeDeg = -4.0;
        environment::SimClock shifted = clock;
        shifted.epochSeconds -= static_cast<double>(DuskSensorOffsetMinutes(fixture)) * 60.0;
        return environment::SunPositionFor(shifted).altitudeDeg <= kDuskAltitudeDeg;
    }

    int LightScheduleOffsetMinutes(util::Id group) noexcept
    {
        return DuskSensorOffsetMinutes(group);
    }

    bool LightScheduleOn(world::LightScheduleClass scheduleClass,
                         const environment::SimClock& clock,
                         util::Id group) noexcept
    {
        environment::SimClock shifted = clock;
        shifted.epochSeconds -= static_cast<double>(LightScheduleOffsetMinutes(group)) * 60.0;
        constexpr double kDuskAltitudeDeg = -4.0;
        const bool dark = environment::SunPositionFor(shifted).altitudeDeg <= kDuskAltitudeDeg;
        const environment::CivilTime wall = shifted.Wall();
        const int minute = wall.hour * 60 + wall.minute;
        switch (scheduleClass)
        {
            case world::LightScheduleClass::Off:
                return false;
            case world::LightScheduleClass::Living:
                return dark && MinuteInWindow(minute, 17 * 60, 30);
            case world::LightScheduleClass::Bedroom:
                return dark && MinuteInWindow(minute, 18 * 60 + 30, 23 * 60 + 45);
            case world::LightScheduleClass::Wet:
                // Wet rooms have no light interaction and several have little useful daylight.
                // Their practical is the automatic readability guarantee, not decoration.
                return true;
            case world::LightScheduleClass::Task:
                return dark && MinuteInWindow(minute, 16 * 60 + 30, 24 * 60);
            case world::LightScheduleClass::Circulation:
                // The same applies to windowless halls and stair cores. With no player-operated
                // switches, a night-only schedule made required routes black at noon.
                return true;
            case world::LightScheduleClass::Dusk:
                return dark;
        }
        return false;
    }

    float BulbTransitionLevel(world::BulbClass bulbClass, float elapsedSeconds) noexcept
    {
        const float elapsed = std::isfinite(elapsedSeconds) ? std::max(elapsedSeconds, 0.0F) : 0.0F;
        const auto segment = [elapsed](float startTime, float endTime, float startLevel, float endLevel)
        {
            const float t = std::clamp((elapsed - startTime) / (endTime - startTime), 0.0F, 1.0F);
            return startLevel + (endLevel - startLevel) * t;
        };

        switch (bulbClass)
        {
            case world::BulbClass::Led:
                return 1.0F;
            case world::BulbClass::Filament:
            {
                constexpr float kRampSeconds = 0.12F;
                const float t = std::clamp(elapsed / kRampSeconds, 0.0F, 1.0F);
                return t * t * (3.0F - 2.0F * t);
            }
            case world::BulbClass::Fluorescent:
                // A fixed strike pattern: flash, dropout, restrike, second dip, then settle.
                // There is no RNG or wall clock here, so replay/capture timing stays deterministic.
                if (elapsed < 0.04F)
                {
                    return segment(0.00F, 0.04F, 0.00F, 0.85F);
                }
                if (elapsed < 0.08F)
                {
                    return segment(0.04F, 0.08F, 0.85F, 0.05F);
                }
                if (elapsed < 0.14F)
                {
                    return segment(0.08F, 0.14F, 0.05F, 0.95F);
                }
                if (elapsed < 0.20F)
                {
                    return segment(0.14F, 0.20F, 0.95F, 0.15F);
                }
                if (elapsed < 0.28F)
                {
                    return segment(0.20F, 0.28F, 0.15F, 0.80F);
                }
                if (elapsed < 0.40F)
                {
                    return segment(0.28F, 0.40F, 0.80F, 1.00F);
                }
                return 1.0F;
        }
        return 0.0F;
    }

    float PointLightAttenuation(float distance, float range) noexcept
    {
        if (!std::isfinite(distance) || !std::isfinite(range) || distance < 0.0F || range <= 0.0F ||
            distance > range)
        {
            return 0.0F;
        }
        const float normalized = distance / range;
        return 1.0F / (1.0F + normalized * normalized);
    }

    float SpotLightAttenuation(float directionCosine, float innerConeDeg, float outerConeDeg) noexcept
    {
        if (!std::isfinite(directionCosine) || !std::isfinite(innerConeDeg) || !std::isfinite(outerConeDeg) ||
            innerConeDeg < 0.0F || outerConeDeg <= 0.0F || innerConeDeg > outerConeDeg ||
            outerConeDeg > 180.0F)
        {
            return 0.0F;
        }
        constexpr float kDegreesToRadians = std::numbers::pi_v<float> / 180.0F;
        const float innerCosine = std::cos(0.5F * innerConeDeg * kDegreesToRadians);
        const float outerCosine = std::cos(0.5F * outerConeDeg * kDegreesToRadians);
        const float cosine = std::clamp(directionCosine, -1.0F, 1.0F);
        if (cosine >= innerCosine)
        {
            return 1.0F;
        }
        if (cosine <= outerCosine || innerCosine <= outerCosine)
        {
            return 0.0F;
        }
        const float t = (cosine - outerCosine) / (innerCosine - outerCosine);
        return t * t * (3.0F - 2.0F * t);
    }

    Microsoft::Xna::Framework::Vector3
    OutdoorSkyIrradianceFor(const Microsoft::Xna::Framework::Vector3& displaySky,
                            const Microsoft::Xna::Framework::Vector3& solarTint,
                            float skyDiffuseIntensity,
                            float twilightAmbientFactor) noexcept
    {
        using Microsoft::Xna::Framework::Vector3;
        if (!std::isfinite(displaySky.X) || !std::isfinite(displaySky.Y) || !std::isfinite(displaySky.Z) ||
            !std::isfinite(solarTint.X) || !std::isfinite(solarTint.Y) || !std::isfinite(solarTint.Z))
        {
            return Vector3(0.0F, 0.0F, 0.0F);
        }
        const float peak = std::max({0.0F, displaySky.X, displaySky.Y, displaySky.Z});
        const float daylight =
            std::isfinite(skyDiffuseIntensity) ? std::clamp(skyDiffuseIntensity, 0.0F, 1.0F) : 0.0F;
        const float twilight =
            std::isfinite(twilightAmbientFactor) ? std::clamp(twilightAmbientFactor, 0.0F, 1.0F) : 0.0F;
        const Vector3 skyTint = peak > 1.0e-6F ? Vector3(std::max(0.0F, displaySky.X) / peak,
                                                         std::max(0.0F, displaySky.Y) / peak,
                                                         std::max(0.0F, displaySky.Z) / peak)
                                               : Vector3(0.0F, 0.0F, 0.0F);
        // Sky appearance is deliberately saturated blue; surface irradiance averages light from
        // the whole hemisphere and is markedly less blue. Mix the existing solar colour anchors
        // with normalized sky chroma, then let the existing cloud/daylight scalar set brightness.
        constexpr float kSkyChromaticShare = 0.25F;
        const Vector3 dayTint((1.0F - kSkyChromaticShare) * std::clamp(solarTint.X, 0.0F, 1.0F) +
                                  kSkyChromaticShare * skyTint.X,
                              (1.0F - kSkyChromaticShare) * std::clamp(solarTint.Y, 0.0F, 1.0F) +
                                  kSkyChromaticShare * skyTint.Y,
                              (1.0F - kSkyChromaticShare) * std::clamp(solarTint.Z, 0.0F, 1.0F) +
                                  kSkyChromaticShare * skyTint.Z);
        const float nightScale = 1.0F - twilight;
        return Vector3(dayTint.X * daylight + std::max(0.0F, displaySky.X) * nightScale,
                       dayTint.Y * daylight + std::max(0.0F, displaySky.Y) * nightScale,
                       dayTint.Z * daylight + std::max(0.0F, displaySky.Z) * nightScale);
    }

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
        objectFixturesByCell_.resize(worldCells.size());
        staticDetailFixturesByCell_.resize(worldCells.size());
        staticDetailFixtureLumens_.resize(worldCells.size());
        staticDetailSpillFixturesByCell_.resize(worldCells.size());
        staticDetailSpillFixtureLumens_.resize(worldCells.size());
        crossCellFixturesByCell_.resize(worldCells.size());
        crossCellFixtureLumens_.resize(worldCells.size());
        dominantSurfaceColors_.reserve(worldCells.size());
        daylightFillDirections_.reserve(worldCells.size());
        hasDaylightFillDirection_.reserve(worldCells.size());
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
            const world::MaterialDef* dominant =
                world.FindMaterial(cell.wallMaterial.IsValid() ? cell.wallMaterial : cell.floorMaterial);
            dominantSurfaceColors_.push_back(dominant != nullptr ? dominant->tint
                                                                 : Vector3(0.70F, 0.70F, 0.70F));
            hasDaylightFillDirection_.push_back(cell.daylight.orientation.has_value());
            daylightFillDirections_.push_back(cell.daylight.orientation.has_value()
                                                  ? DaylightFillDirection(*cell.daylight.orientation)
                                                  : Vector3());
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
                groupBulbClasses_.push_back(light.bulbClass);
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
        groupScheduleClasses_.assign(groups_.size(), world::LightScheduleClass::Off);
        groupHasSchedule_.assign(groups_.size(), false);
        groupOverrides_.assign(groups_.size(), -1);
        for (const world::LightSchedule& schedule : world.LightSchedules())
        {
            const auto found = groupIndex_.find(schedule.group.Value());
            if (found == groupIndex_.end())
            {
                continue;
            }
            groupScheduleClasses_[found->second] = schedule.scheduleClass;
            groupHasSchedule_[found->second] = true;
        }

        // Keep the effect-facing candidates cell-local and allocation-free at draw time. Ranking
        // uses live group output later, so switching and bulb envelopes cannot disagree with the
        // lightmaps or emissive fixtures.
        objectFixtures_.reserve(world.Lights().size());
        std::vector<std::vector<std::size_t>> spillGroupsByCell(worldCells.size());
        for (const world::Light& light : world.Lights())
        {
            if (light.type == world::LightType::EmissiveOnly || light.intensityLm <= 0.0F)
            {
                continue;
            }
            const auto cell = cellIndex_.find(light.cell.Value());
            const auto group = groupIndex_.find(light.group.Value());
            if (cell == cellIndex_.end() || group == groupIndex_.end())
            {
                continue;
            }
            ObjectFixture fixture;
            fixture.groupIndex = group->second;
            fixture.position = light.position;
            fixture.direction = NormalizedOr(light.direction, Vector3(0.0F, -1.0F, 0.0F));
            fixture.color = PlanckianRgb(light.colorK);
            fixture.lumens = light.intensityLm;
            fixture.range = light.range;
            fixture.coneInnerDeg = light.coneInnerDeg;
            fixture.coneOuterDeg = light.coneOuterDeg;
            fixture.positional = light.type != world::LightType::Directional;
            fixture.spot = light.type == world::LightType::Spot;
            const std::size_t fixtureIndex = objectFixtures_.size();
            objectFixturesByCell_[cell->second].push_back(fixtureIndex);
            staticDetailFixturesByCell_[cell->second].push_back(fixtureIndex);
            for (const util::Id receiverId : light.spillCells)
            {
                const auto receiver = cellIndex_.find(receiverId.Value());
                if (receiver == cellIndex_.end())
                {
                    continue;
                }
                staticDetailFixturesByCell_[receiver->second].push_back(fixtureIndex);
                staticDetailSpillFixturesByCell_[receiver->second].push_back(fixtureIndex);
                std::vector<std::size_t>& spillGroups = spillGroupsByCell[receiver->second];
                if (std::find(spillGroups.begin(), spillGroups.end(), group->second) == spillGroups.end())
                {
                    spillGroups.push_back(group->second);
                }
            }
            objectFixtures_.push_back(fixture);
        }
        groupTransitionElapsed_.resize(groups_.size());
        groupTransitionLevels_.resize(groups_.size());
        groupPreviousOn_.resize(groups_.size());

        // Automatic fixtures are still grouped into §23.3's one atlas per switch group. During
        // the sixteen-minute stagger the combined Tier-S atlas therefore uses the lumen-weighted
        // active fraction; once night is established it is exactly the authored full group. The
        // individual ids remain here so later emissive/glow draws can use the same timing without
        // inventing another random offset.
        duskControlledGroups_.assign(groups_.size(), false);
        duskLitLumens_.resize(groups_.size());
        duskFixtures_.reserve(world.Lights().size());
        for (const world::Light& light : world.Lights())
        {
            if (!light.duskSensor)
            {
                continue;
            }
            const auto found = groupIndex_.find(light.group.Value());
            if (found == groupIndex_.end())
            {
                continue;
            }
            duskControlledGroups_[found->second] = true;
            duskFixtures_.push_back(DuskFixture{light.id, found->second, std::max(light.intensityLm, 0.0F)});
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

            // The ordinary cell denominator already contains every lumen in its locally owned
            // groups. Add each explicitly spilled foreign group once; fixtures within a group
            // still keep their individual positions and compete for the two direct-light slots.
            staticDetailFixtureLumens_[index] = packed.totalLumens;
            for (const std::size_t groupIndex : spillGroupsByCell[index])
            {
                const util::Id groupId = groups_[groupIndex].group;
                staticDetailSpillFixtureLumens_[index] += groupLumens_[groupIndex];
                if (std::find(worldCells[index].lightGroups.begin(),
                              worldCells[index].lightGroups.end(),
                              groupId) == worldCells[index].lightGroups.end())
                {
                    staticDetailFixtureLumens_[index] += groupLumens_[groupIndex];
                }
            }

            // A foreign artificial binding proves that an authored fixed source illuminates this
            // receiver. Keep those candidates separate: ordinary room objects still see only
            // owning-cell groups, while weather-facing Basic detail can match the baked skin.
            for (const world::CellLightmapGroup& binding : worldCells[index].lightmaps.artificial)
            {
                if (std::find(worldCells[index].lightGroups.begin(),
                              worldCells[index].lightGroups.end(),
                              binding.group) != worldCells[index].lightGroups.end())
                {
                    continue;
                }
                const auto group = groupIndex_.find(binding.group.Value());
                if (group == groupIndex_.end())
                {
                    continue;
                }
                crossCellFixtureLumens_[index] += groupLumens_[group->second];
                for (std::size_t fixtureIndex = 0; fixtureIndex < objectFixtures_.size(); ++fixtureIndex)
                {
                    if (objectFixtures_[fixtureIndex].groupIndex == group->second)
                    {
                        crossCellFixturesByCell_[index].push_back(fixtureIndex);
                    }
                }
            }
        }
    }

    LightingSystem::LightingSystem(const world::WorldData& world,
                                   const ShadingGrid& shading,
                                   const environment::SimClock& clock,
                                   std::span<const visibility::PortalRuntime> portals,
                                   const rendering::SkyColourModel& skyColourModel)
        : LightingSystem(world, shading, clock, portals)
    {
        skyColourModel_ = std::make_unique<rendering::SkyColourModel>(skyColourModel);
    }

    LightingSystem::~LightingSystem() = default;

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
        const Microsoft::Xna::Framework::Vector3 skyColour =
            skyColourModel_ != nullptr
                ? rendering::SkyAmbientColourFor(*skyColourModel_, sun_, moon_, moonPhase_, cloudCover_)
                : Microsoft::Xna::Framework::Vector3(shading.color.X * shading.skyDiffuseIntensity,
                                                     shading.color.Y * shading.skyDiffuseIntensity,
                                                     shading.color.Z * shading.skyDiffuseIntensity);
        skyAmbientColor_ = skyColour;
        outdoorSkyIrradianceColor_ =
            OutdoorSkyIrradianceFor(skyColour,
                                    shading.color,
                                    shading.skyDiffuseIntensity,
                                    static_cast<float>(environment::TwilightAmbientFactor(sun_.altitudeDeg)));

        std::fill(duskLitLumens_.begin(), duskLitLumens_.end(), 0.0F);
        for (const DuskFixture& fixture : duskFixtures_)
        {
            if (DuskSensorOn(*clock_, fixture.id))
            {
                duskLitLumens_[fixture.groupIndex] += fixture.lumens;
            }
        }
        for (std::size_t index = 0; index < groups_.size(); ++index)
        {
            if (groupOverrides_[index] >= 0)
            {
                groups_[index].on = groupOverrides_[index] != 0;
                continue;
            }
            if (duskControlledGroups_[index])
            {
                const float level = groupLumens_[index] > 0.0F
                                        ? std::clamp(duskLitLumens_[index] / groupLumens_[index], 0.0F, 1.0F)
                                        : 0.0F;
                groups_[index].on = level > 0.0F;
                groups_[index].dimmer = level;
                continue;
            }
            if (!groupHasSchedule_[index])
            {
                continue;
            }
            groups_[index].on = LightScheduleOn(groupScheduleClasses_[index], *clock_, groups_[index].group);
            groups_[index].dimmer = 1.0F;
        }
        AdvanceBulbTransitions(frame.deltaSeconds);

        daylight_.Evaluate(sun_.altitudeDeg, sun_.azimuthDeg, cloudCover_, daylightLevels_);
        for (std::size_t index = 0; index < cells_.size(); ++index)
        {
            cells_[index].daylight = daylightLevels_[index];
            const float ambientLevel = outdoorCells_[index] ? 1.0F : cells_[index].daylight;
            cells_[index].skyAmbientColor = Microsoft::Xna::Framework::Vector3(
                skyColour.X * ambientLevel, skyColour.Y * ambientLevel, skyColour.Z * ambientLevel);
            cells_[index].daylightTint =
                Microsoft::Xna::Framework::Vector3(skyColour.X * cells_[index].daylight,
                                                   skyColour.Y * cells_[index].daylight,
                                                   skyColour.Z * cells_[index].daylight);
            const CellGroups& packed = cellGroups_[index];
            float lit = 0.0F;
            Microsoft::Xna::Framework::Vector3 colorLumens;
            for (std::size_t offset = 0; offset < packed.count; ++offset)
            {
                const util::Id group = cellGroupIds_[packed.first + offset];
                const auto found = groupIndex_.find(group.Value());
                const std::size_t groupIndex = found->second;
                const float contribution = GroupOutputLevel(groupIndex) * groupLumens_[groupIndex];
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
            cells_[index].exposureTarget = ExposureTargetFor(cells_[index], outdoorCells_[index]);
        }
        const RoomLightState* observed = FindCell(cameraCell_);
        if (observed != nullptr)
        {
            cameraExposure_.Advance(observed->exposureTarget, frame.deltaSeconds);
        }
    }

    bool LightingSystem::SetGroupOn(util::Id group, bool on) noexcept
    {
        const auto found = groupIndex_.find(group.Value());
        if (found == groupIndex_.end())
        {
            return false;
        }
        groups_[found->second].on = on;
        groupOverrides_[found->second] = on ? 1 : 0;
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

    float LightingSystem::GroupOutputLevel(util::Id group) const noexcept
    {
        const auto found = groupIndex_.find(group.Value());
        return found == groupIndex_.end() ? 0.0F : GroupOutputLevel(found->second);
    }

    world::BulbClass LightingSystem::GroupBulbClass(util::Id group) const noexcept
    {
        const auto found = groupIndex_.find(group.Value());
        return found == groupIndex_.end() ? world::BulbClass::Filament : groupBulbClasses_[found->second];
    }

    float LightingSystem::GroupOutputLevel(std::size_t groupIndex) const noexcept
    {
        if (groupIndex >= groups_.size() || !groups_[groupIndex].on)
        {
            return 0.0F;
        }
        return std::clamp(groupTransitionLevels_[groupIndex] * groups_[groupIndex].dimmer, 0.0F, 1.0F);
    }

    void LightingSystem::AdvanceBulbTransitions(float deltaSeconds) noexcept
    {
        if (!bulbTransitionsInitialized_)
        {
            for (std::size_t index = 0; index < groups_.size(); ++index)
            {
                groupPreviousOn_[index] = groups_[index].on;
                groupTransitionLevels_[index] = groups_[index].on ? 1.0F : 0.0F;
            }
            bulbTransitionsInitialized_ = true;
            return;
        }

        const float delta = std::isfinite(deltaSeconds) ? std::max(deltaSeconds, 0.0F) : 0.0F;
        for (std::size_t index = 0; index < groups_.size(); ++index)
        {
            const bool on = groups_[index].on;
            if (on != groupPreviousOn_[index])
            {
                groupPreviousOn_[index] = on;
                groupTransitionElapsed_[index] = 0.0F;
                groupTransitionLevels_[index] = 0.0F;
            }
            if (!on)
            {
                groupTransitionLevels_[index] = 0.0F;
                continue;
            }
            groupTransitionElapsed_[index] += delta;
            groupTransitionLevels_[index] =
                BulbTransitionLevel(groupBulbClasses_[index], groupTransitionElapsed_[index]);
        }
    }

    bool LightingSystem::IsGroupDuskControlled(util::Id group) const noexcept
    {
        const auto found = groupIndex_.find(group.Value());
        return found != groupIndex_.end() && duskControlledGroups_[found->second];
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
        return GroupOutputLevel(group);
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

    ObjectLightAssignment LightingSystem::FixtureLightsForObject(std::span<const std::size_t> fixtureIndices,
                                                                 float denominatorLumens,
                                                                 const Vector3& objectCentre) const noexcept
    {
        ObjectLightAssignment assignment;

        struct RankedFixture
        {
            float emitted = -1.0F;
            std::size_t fixtureIndex = 0u;
            Vector3 direction;
        };

        std::array<RankedFixture, 2> brightest;
        for (const std::size_t fixtureIndex : fixtureIndices)
        {
            const ObjectFixture& fixture = objectFixtures_[fixtureIndex];
            float attenuation = 1.0F;
            Vector3 direction = fixture.direction;
            if (fixture.positional)
            {
                const Vector3 toObject(objectCentre.X - fixture.position.X,
                                       objectCentre.Y - fixture.position.Y,
                                       objectCentre.Z - fixture.position.Z);
                const float distanceSquared =
                    toObject.X * toObject.X + toObject.Y * toObject.Y + toObject.Z * toObject.Z;
                if (!std::isfinite(distanceSquared))
                {
                    continue;
                }
                const float distance = std::sqrt(std::max(distanceSquared, 0.0F));
                attenuation = PointLightAttenuation(distance, fixture.range);
                if (attenuation <= 0.0F)
                {
                    continue;
                }
                direction = NormalizedOr(toObject, fixture.direction);
                if (fixture.spot)
                {
                    const float directionCosine = fixture.direction.X * direction.X +
                                                  fixture.direction.Y * direction.Y +
                                                  fixture.direction.Z * direction.Z;
                    attenuation *=
                        SpotLightAttenuation(directionCosine, fixture.coneInnerDeg, fixture.coneOuterDeg);
                    if (attenuation <= 0.0F)
                    {
                        continue;
                    }
                }
            }
            const float emitted = fixture.lumens * GroupOutputLevel(fixture.groupIndex) * attenuation;
            const RankedFixture ranked{emitted, fixtureIndex, direction};
            if (emitted > brightest[0].emitted)
            {
                brightest[1] = brightest[0];
                brightest[0] = ranked;
            }
            else if (emitted > brightest[1].emitted)
            {
                brightest[1] = ranked;
            }
        }
        for (std::size_t slot = 0; slot < brightest.size(); ++slot)
        {
            if (brightest[slot].emitted <= 0.0F || denominatorLumens <= 0.0F)
            {
                continue;
            }
            const ObjectFixture& fixture = objectFixtures_[brightest[slot].fixtureIndex];
            const float share = std::clamp(brightest[slot].emitted / denominatorLumens, 0.0F, 1.0F);
            assignment.slots[slot] = ObjectDirectionalLight{
                brightest[slot].direction,
                Vector3(share * fixture.color.X, share * fixture.color.Y, share * fixture.color.Z)};
        }
        return assignment;
    }

    ObjectLightAssignment LightingSystem::WithReceiverBounce(ObjectLightAssignment assignment,
                                                             std::size_t cellIndex) const noexcept
    {
        if (assignment.slots[0].has_value())
        {
            const ObjectDirectionalLight& key = *assignment.slots[0];
            const Vector3 fillDirection =
                assignment.slots[1].has_value() ? assignment.slots[1]->direction : Vector3();
            const Vector3 fillColor =
                assignment.slots[1].has_value() ? assignment.slots[1]->diffuseColor : Vector3();
            const Vector3 bounceDirection =
                NormalizedOr(Vector3(-(key.direction.X + fillDirection.X),
                                     -(key.direction.Y + fillDirection.Y),
                                     -(key.direction.Z + fillDirection.Z)),
                             Vector3(-key.direction.X, -key.direction.Y, -key.direction.Z));
            const Vector3& surface = dominantSurfaceColors_[cellIndex];
            constexpr float kBounce = 0.18F;
            assignment.slots[2] =
                ObjectDirectionalLight{bounceDirection,
                                       Vector3(kBounce * surface.X * (key.diffuseColor.X + fillColor.X),
                                               kBounce * surface.Y * (key.diffuseColor.Y + fillColor.Y),
                                               kBounce * surface.Z * (key.diffuseColor.Z + fillColor.Z))};
        }
        return assignment;
    }

    ObjectLightAssignment
    LightingSystem::DirectionalLightsForObject(util::Id cell, const Vector3& objectCentre) const noexcept
    {
        ObjectLightAssignment assignment;
        const auto found = cellIndex_.find(cell.Value());
        if (found == cellIndex_.end())
        {
            return assignment;
        }
        const std::size_t cellIndex = found->second;

        if (const CelestialKeyLight* celestial = CelestialKeyForCell(cell); celestial != nullptr)
        {
            assignment.slots[0] = ObjectDirectionalLight{celestial->direction, celestial->diffuseColor};
            if (hasDaylightFillDirection_[cellIndex] && cells_[cellIndex].daylight > 0.0F)
            {
                constexpr float kWindowFill = 0.35F;
                assignment.slots[1] = ObjectDirectionalLight{daylightFillDirections_[cellIndex],
                                                             Vector3(kWindowFill * skyAmbientColor_.X,
                                                                     kWindowFill * skyAmbientColor_.Y,
                                                                     kWindowFill * skyAmbientColor_.Z)};
            }
        }
        else
        {
            assignment = FixtureLightsForObject(
                objectFixturesByCell_[cellIndex], cellGroups_[cellIndex].totalLumens, objectCentre);
        }
        return WithReceiverBounce(assignment, cellIndex);
    }

    ObjectLightAssignment
    LightingSystem::StaticDetailLightsForObject(util::Id cell, const Vector3& objectCentre) const noexcept
    {
        if (CelestialKeyForCell(cell) != nullptr)
        {
            return DirectionalLightsForObject(cell, objectCentre);
        }
        return StaticFixtureLightsForObject(cell, objectCentre);
    }

    ObjectLightAssignment
    LightingSystem::StaticFixtureLightsForObject(util::Id cell, const Vector3& objectCentre) const noexcept
    {
        const auto found = cellIndex_.find(cell.Value());
        if (found == cellIndex_.end())
        {
            return {};
        }
        const std::size_t cellIndex = found->second;
        ObjectLightAssignment assignment = WithReceiverBounce(
            FixtureLightsForObject(
                staticDetailFixturesByCell_[cellIndex], staticDetailFixtureLumens_[cellIndex], objectCentre),
            cellIndex);
        const ObjectLightAssignment spill =
            FixtureLightsForObject(staticDetailSpillFixturesByCell_[cellIndex],
                                   staticDetailSpillFixtureLumens_[cellIndex],
                                   objectCentre);
        for (std::size_t slot = 0; slot < 2; ++slot)
        {
            if (!spill.slots[slot].has_value())
            {
                continue;
            }
            assignment.spillDiffuseColor.X += spill.slots[slot]->diffuseColor.X;
            assignment.spillDiffuseColor.Y += spill.slots[slot]->diffuseColor.Y;
            assignment.spillDiffuseColor.Z += spill.slots[slot]->diffuseColor.Z;
        }
        return assignment;
    }

    ObjectLightAssignment
    LightingSystem::CrossCellReceiverLightsForObject(util::Id receiverCell,
                                                     const Vector3& objectCentre) const noexcept
    {
        const auto found = cellIndex_.find(receiverCell.Value());
        if (found == cellIndex_.end())
        {
            return {};
        }
        const std::size_t cellIndex = found->second;
        return WithReceiverBounce(FixtureLightsForObject(crossCellFixturesByCell_[cellIndex],
                                                         crossCellFixtureLumens_[cellIndex],
                                                         objectCentre),
                                  cellIndex);
    }

} // namespace cnahouse::lighting
