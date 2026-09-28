// SPDX-License-Identifier: MIT
//
// `HOUSE-01251`. §28.1's per-frame lighting loop, against the house's own 243 fixtures in 135
// switch groups.
//
// The claims here are about the ARITHMETIC of a room's level and about the data it reads, and they
// are made against the authored world rather than a fixture: a lighting system that behaves on a
// two-room toy and divides by zero on `L0_PANTRY` has not been tested. The one number that needed
// deciding -- how several groups of different sizes combine into one level -- is checked against a
// property (a kitchen's cabinet strip must not count as much as its down-lights) rather than
// against a restatement of the formula.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/app/FrameTimer.hpp"
#include "cnahouse/environment/MoonLight.hpp"
#include "cnahouse/environment/MoonModel.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/SunLight.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/lighting/LightingSystem.hpp"
#include "cnahouse/lighting/PlanckianLut.hpp"
#include "cnahouse/lighting/ShadingGrid.hpp"
#include "cnahouse/rendering/SkySystem.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::app::FrameContext;
    using cnahouse::lighting::BulbTransitionLevel;
    using cnahouse::lighting::CelestialKeyLight;
    using cnahouse::lighting::DuskSensorOffsetMinutes;
    using cnahouse::lighting::DuskSensorOn;
    using cnahouse::lighting::kAmbientFloor;
    using cnahouse::lighting::kDaylightKeyThreshold;
    using cnahouse::lighting::LightingSystem;
    using cnahouse::lighting::LightScheduleOffsetMinutes;
    using cnahouse::lighting::LightScheduleOn;
    using cnahouse::lighting::ObjectLightAssignment;
    using cnahouse::lighting::OutdoorSkyIrradianceFor;
    using cnahouse::lighting::PlanckianRgb;
    using cnahouse::lighting::PointLightAttenuation;
    using cnahouse::lighting::RoomLightState;
    using cnahouse::lighting::ShadingGrid;
    using cnahouse::lighting::SwitchGroupState;
    using cnahouse::util::Id;
    namespace world = cnahouse::world;

    bool ContentIsBuilt()
    {
        return std::filesystem::exists("content/world/layout.lights.json");
    }

    world::WorldData LoadWorld()
    {
        world::WorldData::Contents contents;
        EXPECT_TRUE(world::WorldLoader::LoadLevels("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadCells("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadPortals("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadOpenings("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadLights("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadInteractables("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadInitialState("content/world", contents).HasValue());
        auto built = world::WorldData::Create(std::move(contents));
        EXPECT_TRUE(built) << built.Error().ToString();
        return std::move(built.Value());
    }

    ShadingGrid LoadShading()
    {
        auto grid = ShadingGrid::ReadFromTitle("content/world/shading.bin");
        return grid ? std::move(grid.Value()) : ShadingGrid::Unshaded();
    }

    cnahouse::rendering::SkyColourModel LoadSkyColours()
    {
        auto model =
            cnahouse::rendering::SkyColourModelReader::ReadFromTitle("content/world/layout.sky.json");
        EXPECT_TRUE(model) << (model ? std::string() : model.Error().ToString());
        return std::move(model.Value());
    }

    struct HouseLighting
    {
        world::WorldData world = LoadWorld();
        ShadingGrid shading = LoadShading();
        cnahouse::environment::SimClock clock;
        cnahouse::visibility::VisibilitySystem visibility{world};
        cnahouse::rendering::SkyColourModel skyColours = LoadSkyColours();
        LightingSystem lighting{world, shading, clock, visibility.Portals(), skyColours};
    };

    FrameContext Frame(std::uint64_t index)
    {
        FrameContext frame;
        frame.frameIndex = index;
        frame.deltaSeconds = 1.0F / 60.0F;
        frame.realDeltaSeconds = 1.0F / 60.0F;
        return frame;
    }

} // namespace

TEST(LightingSystemTests, ThreeBulbFamiliesHaveTheAuthoredSwitchOnShapes)
{
    using world::BulbClass;
    EXPECT_FLOAT_EQ(BulbTransitionLevel(BulbClass::Led, 0.0F), 1.0F);
    EXPECT_FLOAT_EQ(BulbTransitionLevel(BulbClass::Led, 0.01F), 1.0F);

    EXPECT_FLOAT_EQ(BulbTransitionLevel(BulbClass::Filament, 0.0F), 0.0F);
    EXPECT_NEAR(BulbTransitionLevel(BulbClass::Filament, 0.06F), 0.5F, 1.0e-6F);
    EXPECT_FLOAT_EQ(BulbTransitionLevel(BulbClass::Filament, 0.12F), 1.0F);

    const float firstStrike = BulbTransitionLevel(BulbClass::Fluorescent, 0.04F);
    const float firstDrop = BulbTransitionLevel(BulbClass::Fluorescent, 0.08F);
    const float restrike = BulbTransitionLevel(BulbClass::Fluorescent, 0.14F);
    const float secondDrop = BulbTransitionLevel(BulbClass::Fluorescent, 0.20F);
    EXPECT_GT(firstStrike, firstDrop);
    EXPECT_GT(restrike, firstDrop);
    EXPECT_GT(restrike, secondDrop);
    EXPECT_FLOAT_EQ(BulbTransitionLevel(BulbClass::Fluorescent, 0.40F), 1.0F);
    EXPECT_FLOAT_EQ(BulbTransitionLevel(BulbClass::Fluorescent, 8.0F), 1.0F);

    EXPECT_FLOAT_EQ(BulbTransitionLevel(BulbClass::Filament, -1.0F), 0.0F);
    EXPECT_FLOAT_EQ(BulbTransitionLevel(BulbClass::Filament, std::nan("")), 0.0F);
}

TEST(LightingSystemTests, AuthoredGroupsDriveOneSharedTransitionThroughEveryLightingConsumer)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    LightingSystem& lighting = house.lighting;
    const Id filament = Id::Of("LG_L0_PANTRY_MAIN");
    const Id led = Id::Of("LG_L0_KITCHEN_UNDERCAB");
    const Id fluorescent = Id::Of("LG_L0_GARAGE_MAIN");
    EXPECT_EQ(lighting.GroupBulbClass(filament), world::BulbClass::Filament);
    EXPECT_EQ(lighting.GroupBulbClass(led), world::BulbClass::Led);
    EXPECT_EQ(lighting.GroupBulbClass(fluorescent), world::BulbClass::Fluorescent);

    ASSERT_TRUE(lighting.SetGroupOn(filament, false));
    ASSERT_TRUE(lighting.SetGroupOn(led, false));
    ASSERT_TRUE(lighting.SetGroupOn(fluorescent, false));
    lighting.Update(Frame(1));

    ASSERT_TRUE(lighting.SetGroupOn(filament, true));
    ASSERT_TRUE(lighting.SetGroupOn(led, true));
    ASSERT_TRUE(lighting.SetGroupOn(fluorescent, true));
    FrameContext strike = Frame(2);
    strike.deltaSeconds = 0.04F;
    lighting.Update(strike);
    const float filamentFirst = lighting.GroupOutputLevel(filament);
    const float fluorescentFirst = lighting.GroupOutputLevel(fluorescent);
    EXPECT_GT(filamentFirst, 0.0F);
    EXPECT_LT(filamentFirst, 1.0F);
    EXPECT_FLOAT_EQ(lighting.GroupOutputLevel(led), 1.0F);
    EXPECT_NEAR(fluorescentFirst, 0.85F, 1.0e-6F);

    FrameContext dropout = Frame(3);
    dropout.deltaSeconds = 0.04F;
    lighting.Update(dropout);
    EXPECT_GT(lighting.GroupOutputLevel(filament), filamentFirst);
    EXPECT_LT(lighting.GroupOutputLevel(fluorescent), fluorescentFirst);

    FrameContext settled = Frame(4);
    settled.deltaSeconds = 0.50F;
    lighting.Update(settled);
    EXPECT_FLOAT_EQ(lighting.GroupOutputLevel(filament), 1.0F);
    EXPECT_FLOAT_EQ(lighting.GroupOutputLevel(led), 1.0F);
    EXPECT_FLOAT_EQ(lighting.GroupOutputLevel(fluorescent), 1.0F);

    ASSERT_TRUE(lighting.SetGroupOn(filament, false));
    ASSERT_TRUE(lighting.SetGroupOn(led, false));
    ASSERT_TRUE(lighting.SetGroupOn(fluorescent, false));
    lighting.Update(Frame(5));
    EXPECT_FLOAT_EQ(lighting.GroupOutputLevel(filament), 0.0F);
    EXPECT_FLOAT_EQ(lighting.GroupOutputLevel(led), 0.0F);
    EXPECT_FLOAT_EQ(lighting.GroupOutputLevel(fluorescent), 0.0F);
}

TEST(LightingSystemTests, OutdoorBakedSkinUsesDaylightEnergyWithoutDisplaySkySaturation)
{
    const Microsoft::Xna::Framework::Vector3 display(0.45F, 0.55F, 0.80F);
    const Microsoft::Xna::Framework::Vector3 solar(1.0F, 0.95F, 0.90F);
    const auto clear = OutdoorSkyIrradianceFor(display, solar, 1.0F, 1.0F);
    EXPECT_NEAR(clear.X, 0.890625F, 1.0e-6F);
    EXPECT_NEAR(clear.Y, 0.884375F, 1.0e-6F);
    EXPECT_NEAR(clear.Z, 0.925F, 1.0e-6F);
    EXPECT_GT(clear.X / clear.Z, display.X / display.Z);

    const auto cloudy = OutdoorSkyIrradianceFor(display, solar, 0.65F, 1.0F);
    EXPECT_NEAR(cloudy.Z, 0.60125F, 1.0e-6F);
    EXPECT_LT(cloudy.X, clear.X);
    EXPECT_LT(cloudy.Y, clear.Y);
}

TEST(LightingSystemTests, OutdoorSkyTwilightFallsBackToTheDimNightDisplayGradient)
{
    const Microsoft::Xna::Framework::Vector3 display(0.01F, 0.02F, 0.04F);
    const Microsoft::Xna::Framework::Vector3 solar(1.0F, 0.5F, 0.3F);
    const auto night = OutdoorSkyIrradianceFor(display, solar, 0.0F, 0.0F);
    EXPECT_FLOAT_EQ(night.X, display.X);
    EXPECT_FLOAT_EQ(night.Y, display.Y);
    EXPECT_FLOAT_EQ(night.Z, display.Z);
    const auto dusk = OutdoorSkyIrradianceFor(display, solar, 0.05F, 0.50F);
    EXPECT_GT(dusk.Z, night.Z);
    EXPECT_LT(dusk.Z, 0.06F);
    const auto dark = OutdoorSkyIrradianceFor(display, solar, 0.0F, 1.0F);
    EXPECT_FLOAT_EQ(dark.Z, 0.0F);
}

TEST(LightingSystemTests, OutdoorSkyIrradianceRejectsNonFiniteInputsWithoutPoisoningAFrame)
{
    const Microsoft::Xna::Framework::Vector3 display(0.40F, 0.50F, 0.70F);
    const Microsoft::Xna::Framework::Vector3 solar(1.0F, 0.95F, 0.90F);
    const auto badLight =
        OutdoorSkyIrradianceFor(display, solar, std::numeric_limits<float>::quiet_NaN(), 1.0F);
    EXPECT_FLOAT_EQ(badLight.X, 0.0F);
    const auto badColour = OutdoorSkyIrradianceFor(
        Microsoft::Xna::Framework::Vector3(std::numeric_limits<float>::infinity(), 0.50F, 0.70F),
        solar,
        0.8F,
        1.0F);
    EXPECT_FLOAT_EQ(badColour.X, 0.0F);
    EXPECT_FLOAT_EQ(badColour.Y, 0.0F);
    EXPECT_FLOAT_EQ(badColour.Z, 0.0F);
}

TEST(LightingSystemTests, DuskOffsetsAreStableBoundedAndActuallyStaggerTheAuthoredFixtures)
{
    EXPECT_EQ(DuskSensorOffsetMinutes(Id::Of("LIGHT_L0_PORCH_LANTERN_1")), -8);
    EXPECT_EQ(DuskSensorOffsetMinutes(Id::Of("LIGHT_L0_PORCH_LANTERN_2")), -4);

    int earliest = 8;
    int latest = -8;
    for (int index = 1; index <= 9; ++index)
    {
        const Id fixture = Id::Of("LIGHT_EXT_STREET_" + std::to_string(index));
        const int offset = DuskSensorOffsetMinutes(fixture);
        EXPECT_GE(offset, -8);
        EXPECT_LE(offset, 8);
        earliest = std::min(earliest, offset);
        latest = std::max(latest, offset);
    }
    EXPECT_LE(earliest, -5);
    EXPECT_GE(latest, 5);
}

TEST(LightingSystemTests, OneFixtureReadsTheSameSunAsTheClockAndCrossesAtNight)
{
    cnahouse::environment::SimClock clock;
    clock.calendarDaysPerSimDay = 1.0;
    cnahouse::environment::CivilTime time;
    time.year = 2031;
    time.month = 6;
    time.day = 14;
    time.hour = 12;
    clock.SetStandard(time);
    EXPECT_FALSE(DuskSensorOn(clock, Id::Of("LIGHT_L0_PORCH_LANTERN_1")));

    time.hour = 22;
    clock.SetStandard(time);
    EXPECT_TRUE(DuskSensorOn(clock, Id::Of("LIGHT_L0_PORCH_LANTERN_1")));
}

TEST(LightScheduleTests, RoomsWithoutReliableDaylightStayLitWhileOtherClassesFollowTheirWindows)
{
    cnahouse::environment::SimClock clock;
    clock.calendarDaysPerSimDay = 1.0;
    clock.dstRulesUS = false;
    cnahouse::environment::CivilTime time;
    time.year = 2031;
    time.month = 1;
    time.day = 15;
    const Id group = Id::Of("LG_TEST_SCHEDULE");

    time.hour = 12;
    clock.SetStandard(time);
    for (const world::LightScheduleClass scheduleClass : {world::LightScheduleClass::Living,
                                                          world::LightScheduleClass::Bedroom,
                                                          world::LightScheduleClass::Task,
                                                          world::LightScheduleClass::Dusk})
    {
        EXPECT_FALSE(LightScheduleOn(scheduleClass, clock, group));
    }
    EXPECT_TRUE(LightScheduleOn(world::LightScheduleClass::Wet, clock, group));
    EXPECT_TRUE(LightScheduleOn(world::LightScheduleClass::Circulation, clock, group));

    time.hour = 22;
    clock.SetStandard(time);
    EXPECT_TRUE(LightScheduleOn(world::LightScheduleClass::Living, clock, group));
    EXPECT_TRUE(LightScheduleOn(world::LightScheduleClass::Bedroom, clock, group));
    EXPECT_TRUE(LightScheduleOn(world::LightScheduleClass::Wet, clock, group));
    EXPECT_TRUE(LightScheduleOn(world::LightScheduleClass::Task, clock, group));
    EXPECT_TRUE(LightScheduleOn(world::LightScheduleClass::Circulation, clock, group));
    EXPECT_TRUE(LightScheduleOn(world::LightScheduleClass::Dusk, clock, group));
    EXPECT_FALSE(LightScheduleOn(world::LightScheduleClass::Off, clock, group));

    time.hour = 1;
    clock.SetStandard(time);
    EXPECT_FALSE(LightScheduleOn(world::LightScheduleClass::Living, clock, group));
    EXPECT_FALSE(LightScheduleOn(world::LightScheduleClass::Bedroom, clock, group));
    EXPECT_TRUE(LightScheduleOn(world::LightScheduleClass::Wet, clock, group));
    EXPECT_FALSE(LightScheduleOn(world::LightScheduleClass::Task, clock, group));
    EXPECT_TRUE(LightScheduleOn(world::LightScheduleClass::Circulation, clock, group));
}

TEST(LightingSystemTests, NoonScheduleLightsTheReportedHallPowderRoomAndStair)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    cnahouse::environment::CivilTime noon;
    noon.year = 2031;
    noon.month = 6;
    noon.day = 14;
    noon.hour = 12;
    house.clock.SetStandard(noon);
    house.lighting.Update(Frame(1));

    for (const Id group :
         {Id::Of("LG_L0_HALL_MAIN"), Id::Of("LG_L0_WC1_MAIN"), Id::Of("LG_L0_STAIR_MAIN_MAIN")})
    {
        const SwitchGroupState* state = house.lighting.FindGroup(group);
        ASSERT_NE(state, nullptr);
        EXPECT_TRUE(state->on);
        EXPECT_GT(house.lighting.GroupOutputLevel(group), 0.0F);
    }
    for (const Id cell : {Id::Of("L0_HALL"), Id::Of("L0_WC1"), Id::Of("L0_STAIR_MAIN")})
    {
        const RoomLightState* state = house.lighting.FindCell(cell);
        ASSERT_NE(state, nullptr);
        EXPECT_GT(state->artificial, 0.0F);
    }

    struct Receiver
    {
        Id cell;
        Id group;
    };

    for (const Receiver receiver : {Receiver{Id::Of("L0_HALL"), Id::Of("LG_L0_HALL_MAIN")},
                                    Receiver{Id::Of("L0_WC1"), Id::Of("LG_L0_WC1_MAIN")},
                                    Receiver{Id::Of("L0_STAIR_MAIN"), Id::Of("LG_L0_STAIR_MAIN_MAIN")}})
    {
        const world::Cell* cell = house.world.FindCell(receiver.cell);
        ASSERT_NE(cell, nullptr);
        const auto binding = std::find_if(cell->lightmaps.artificial.begin(),
                                          cell->lightmaps.artificial.end(),
                                          [&](const world::CellLightmapGroup& candidate)
                                          { return candidate.group == receiver.group; });
        ASSERT_NE(binding, cell->lightmaps.artificial.end());
        EXPECT_GE(binding->texture.receiverMean, 0.10F)
            << "an automatic route light must have enough baked receiver energy to remain visible";
    }
}

TEST(LightingSystemTests, DimBedroomsKeepTheirMainFixturesOnAtNoonAndNight)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    cnahouse::environment::CivilTime time;
    time.year = 2031;
    time.month = 6;
    time.day = 14;

    struct Case
    {
        Id cell;
        Id mainGroup;
        Id accentGroup;
    };

    const std::array cases{
        Case{Id::Of("L1_MASTER_BED"), Id::Of("LG_L1_MASTER_BED_MAIN"), Id::Of("LG_L1_MASTER_BED_BEDSIDE")},
        Case{Id::Of("L1_BED5"), Id::Of("LG_L1_BED5_MAIN"), Id::Of("LG_L1_BED5_BEDSIDE")},
        Case{Id::Of("L2_BED7"), Id::Of("LG_L2_BED7_MAIN"), Id::Of("LG_L2_BED7_BEDSIDE")}};
    for (const int hour : {10, 23})
    {
        time.hour = hour;
        house.clock.SetStandard(time);
        house.lighting.Update(Frame(static_cast<std::uint64_t>(hour)));
        for (const Case check : cases)
        {
            const SwitchGroupState* main = house.lighting.FindGroup(check.mainGroup);
            const SwitchGroupState* accent = house.lighting.FindGroup(check.accentGroup);
            const RoomLightState* room = house.lighting.FindCell(check.cell);
            ASSERT_NE(main, nullptr);
            ASSERT_NE(accent, nullptr);
            ASSERT_NE(room, nullptr);
            EXPECT_TRUE(main->on) << check.cell.Value() << " at " << hour << ":00";
            EXPECT_GT(house.lighting.GroupOutputLevel(check.mainGroup), 0.0F);
            EXPECT_GT(room->artificial, 0.0F);
            EXPECT_EQ(accent->on,
                      LightScheduleOn(world::LightScheduleClass::Bedroom, house.clock, check.accentGroup))
                << "the bedside mood schedule is not part of the correction";
        }
    }
}

TEST(LightingSystemTests, TheMovingReviewOutliersHaveRealReceiverEnergy)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed lighting world";
    }
    HouseLighting house;
    cnahouse::environment::CivilTime time;
    time.year = 2031;
    time.month = 6;
    time.day = 14;
    for (const int hour : {10, 23})
    {
        time.hour = hour;
        house.clock.SetStandard(time);
        FrameContext settled = Frame(static_cast<std::uint64_t>(hour));
        settled.deltaSeconds = 1.0F; // Do not mistake the normal incandescent warm-up for a dark bake.
        house.lighting.Update(settled);
        for (const auto& [cellId, group] :
             {std::pair{Id::Of("B1_STOR2"), Id::Of("LG_B1_STOR2_MAIN")},
              std::pair{Id::Of("B1_LAUNDRY2"), Id::Of("LG_B1_LAUNDRY2_MAIN")},
              std::pair{Id::Of("L0_GARAGE"), Id::Of("LG_L0_GARAGE_MAIN")},
              std::pair{Id::Of("L0_GARAGE_LOFT"), Id::Of("LG_L0_GARAGE_LOFT_MAIN")}})
        {
            const world::Cell* cell = house.world.FindCell(cellId);
            ASSERT_NE(cell, nullptr);
            const auto binding =
                std::find_if(cell->lightmaps.artificial.begin(),
                             cell->lightmaps.artificial.end(),
                             [&](const world::CellLightmapGroup& row) { return row.group == group; });
            ASSERT_NE(binding, cell->lightmaps.artificial.end());
            EXPECT_GE(binding->texture.receiverMean * house.lighting.GroupLevelInCell(cellId, group), 0.10F)
                << "switch-on alone is insufficient: the moving GPU review found dark receivers";
        }
    }
}

TEST(LightingSystemTests, CorrectedBasementPropsDoNotZeroTheirDiffuseResponse)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed lighting world";
    }
    world::WorldData::Contents contents;
    ASSERT_TRUE(world::WorldLoader::LoadProps("content/world", contents));
    ASSERT_TRUE(world::WorldLoader::LoadMaterials("content/world", contents));
    for (const Id id : {Id::Of("PROP_B1_LAUNDRY2_DRYER"), Id::Of("PROP_B1_WORKSHOP_PAINT_TIN")})
    {
        const auto prop = std::find_if(contents.props.begin(),
                                       contents.props.end(),
                                       [&](const world::Prop& row) { return row.id == id; });
        ASSERT_NE(prop, contents.props.end());
        const auto material =
            std::find_if(contents.materials.begin(),
                         contents.materials.end(),
                         [&](const world::MaterialDef& row) { return row.id == prop->material; });
        ASSERT_NE(material, contents.materials.end());
        // A specular-only conductor override was black despite a readable, lit room.
        // Reuse the existing Basic-compatible hardware finish, not a global material floor.
        EXPECT_GT(material->tint.X, 0.0F);
        EXPECT_GT(material->tint.Y, 0.0F);
        EXPECT_GT(material->tint.Z, 0.0F);
    }
}

TEST(LightScheduleTests, GroupOffsetsAreStableBoundedAndNotAllEqual)
{
    const std::array groups{Id::Of("LG_B1_HALL_MAIN_C"),
                            Id::Of("LG_L0_LIVING_MAIN"),
                            Id::Of("LG_L1_MASTER_BED_MAIN"),
                            Id::Of("LG_L2_LIBRARY_MAIN"),
                            Id::Of("LG_L3_ROOM_MAIN")};
    int first = LightScheduleOffsetMinutes(groups.front());
    bool differs = false;
    for (const Id group : groups)
    {
        const int offset = LightScheduleOffsetMinutes(group);
        EXPECT_EQ(offset, LightScheduleOffsetMinutes(group));
        EXPECT_GE(offset, -8);
        EXPECT_LE(offset, 8);
        differs = differs || offset != first;
    }
    EXPECT_TRUE(differs);
}

TEST(LightScheduleTests, RepresentativeGroupOnEveryInteriorLevelFollowsItsAutomaticClass)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    house.clock.calendarDaysPerSimDay = 1.0;
    house.clock.dstRulesUS = false;
    cnahouse::environment::CivilTime time;
    time.year = 2031;
    time.month = 1;
    time.day = 15;

    struct Case
    {
        Id group;
        Id cell;
        bool dayOn;
    };

    const std::array cases{
        Case{Id::Of("LG_B1_HALL_MAIN_C"), Id::Of("B1_HALL"), true},
        Case{Id::Of("LG_L0_LIVING_MAIN"), Id::Of("L0_LIVING"), false},
        Case{Id::Of("LG_L1_MASTER_BED_BEDSIDE"), Id::Of("L1_MASTER_BED"), false},
        Case{Id::Of("LG_L2_LIBRARY_MAIN"), Id::Of("L2_LIBRARY"), true},
        Case{Id::Of("LG_L3_ROOM_MAIN"), Id::Of("L3_ROOM"), true},
    };

    time.hour = 12;
    house.clock.SetStandard(time);
    house.lighting.Update(Frame(100));
    std::array<float, cases.size()> dayLevels{};
    for (std::size_t index = 0; index < cases.size(); ++index)
    {
        EXPECT_EQ(house.lighting.FindGroup(cases[index].group)->on, cases[index].dayOn);
        dayLevels[index] = house.lighting.FindCell(cases[index].cell)->artificial;
    }

    time.hour = 22;
    house.clock.SetStandard(time);
    FrameContext night = Frame(101);
    night.deltaSeconds = 0.5F;
    house.lighting.Update(night);
    for (std::size_t index = 0; index < cases.size(); ++index)
    {
        EXPECT_TRUE(house.lighting.FindGroup(cases[index].group)->on);
        if (cases[index].dayOn)
        {
            // The main fixture remains on, while the cell's accent groups may join it at night.
            EXPECT_GE(house.lighting.FindCell(cases[index].cell)->artificial, dayLevels[index]);
        }
        else
        {
            EXPECT_GT(house.lighting.FindCell(cases[index].cell)->artificial, dayLevels[index]);
        }
    }
}

TEST(LightScheduleTests, DeepShowcaseRoomsKeepTheirMainFixtureOnAtNoon)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    house.clock.calendarDaysPerSimDay = 1.0;
    house.clock.dstRulesUS = false;
    cnahouse::environment::CivilTime time;
    time.year = 2031;
    time.month = 1;
    time.day = 15;
    time.hour = 12;
    house.clock.SetStandard(time);
    house.lighting.Update(Frame(100));

    for (const Id group : {Id::Of("LG_B1_CINEMA_MAIN"),
                           Id::Of("LG_L0_DINING_CHANDELIER"),
                           Id::Of("LG_L2_LIBRARY_MAIN"),
                           Id::Of("LG_L2_SITTING_MAIN"),
                           Id::Of("LG_L3_ROOM_MAIN")})
    {
        const SwitchGroupState* state = house.lighting.FindGroup(group);
        ASSERT_NE(state, nullptr);
        EXPECT_TRUE(state->on) << "a deep showcase room needs its authored fixture by day";
    }
    for (const Id group : {Id::Of("LG_B1_CINEMA_AISLE"),
                           Id::Of("LG_L0_DINING_SIDE"),
                           Id::Of("LG_L2_LIBRARY_READING"),
                           Id::Of("LG_L2_SITTING_READING"),
                           Id::Of("LG_L3_ROOM_DESK")})
    {
        const SwitchGroupState* state = house.lighting.FindGroup(group);
        ASSERT_NE(state, nullptr);
        EXPECT_FALSE(state->on) << "the decorative and task accents retain their evening schedule";
    }
}

TEST(LightingSystemTests, AWindowlessDiningRoomUsesBorrowedDaylightOnlyDuringDay)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    house.clock.calendarDaysPerSimDay = 1.0;
    house.clock.dstRulesUS = false;
    cnahouse::environment::CivilTime time;
    time.year = 2031;
    time.month = 1;
    time.day = 15;
    time.hour = 12;
    house.clock.SetStandard(time);
    house.lighting.Update(Frame(100));
    const RoomLightState* dining = house.lighting.FindCell(Id::Of("L0_DINING"));
    ASSERT_NE(dining, nullptr);
    EXPECT_FLOAT_EQ(dining->daylight, 0.0F) << "the dining room has no window of its own";
    EXPECT_GT(dining->daylightTint.X, 0.0F) << "an open neighbour should reach its daylight bake";

    time.hour = 23;
    house.clock.SetStandard(time);
    house.lighting.Update(Frame(101));
    dining = house.lighting.FindCell(Id::Of("L0_DINING"));
    ASSERT_NE(dining, nullptr);
    EXPECT_EQ(dining->daylightTint, Microsoft::Xna::Framework::Vector3())
        << "artificial light behind an open door must not masquerade as daylight";
}

TEST(LightScheduleTests, WindowlessBasementTaskRoomsKeepTheirMainFixturesOnAtNoon)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    house.clock.calendarDaysPerSimDay = 1.0;
    house.clock.dstRulesUS = false;
    cnahouse::environment::CivilTime time;
    time.year = 2031;
    time.month = 1;
    time.day = 15;
    time.hour = 10;
    time.minute = 30;
    house.clock.SetStandard(time);
    FrameContext day = Frame(200);
    day.deltaSeconds = 1.0F;
    house.lighting.Update(day);

    const std::array mainGroups{
        std::pair{Id::Of("LG_B1_GYM_MAIN"), Id::Of("B1_GYM")},
        std::pair{Id::Of("LG_B1_HOBBY_MAIN"), Id::Of("B1_HOBBY")},
        std::pair{Id::Of("LG_B1_WORKSHOP_MAIN"), Id::Of("B1_WORKSHOP")},
    };
    for (const auto& [group, cell] : mainGroups)
    {
        ASSERT_NE(house.lighting.FindGroup(group), nullptr);
        EXPECT_TRUE(house.lighting.FindGroup(group)->on);
        EXPECT_GT(house.lighting.FindCell(cell)->artificial, 0.40F);
    }
    // The task accents retain their authored evening schedule; only the functional room
    // fixtures need to be on throughout the day in these windowless basement cells.
    for (const Id group :
         {Id::Of("LG_B1_GYM_MIRROR"), Id::Of("LG_B1_HOBBY_TABLE"), Id::Of("LG_B1_WORKSHOP_BENCH")})
    {
        ASSERT_NE(house.lighting.FindGroup(group), nullptr);
        EXPECT_FALSE(house.lighting.FindGroup(group)->on);
    }
}

TEST(LightScheduleTests, FoyerMainFixtureLightsTheStairApproachAtNoon)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    house.clock.calendarDaysPerSimDay = 1.0;
    house.clock.dstRulesUS = false;
    cnahouse::environment::CivilTime time;
    time.year = 2031;
    time.month = 1;
    time.day = 15;
    time.hour = 10;
    time.minute = 30;
    house.clock.SetStandard(time);
    FrameContext day = Frame(200);
    day.deltaSeconds = 1.0F;
    house.lighting.Update(day);

    const Id mainGroup = Id::Of("LG_L0_FOYER_MAIN");
    const RoomLightState* foyer = house.lighting.FindCell(Id::Of("L0_FOYER"));
    ASSERT_NE(house.lighting.FindGroup(mainGroup), nullptr);
    ASSERT_NE(foyer, nullptr);
    EXPECT_TRUE(house.lighting.FindGroup(mainGroup)->on);
    EXPECT_GT(foyer->artificial, 0.50F);
}

TEST(LightScheduleTests, EveryAccessibleLitCellHasAnAutomaticGroup)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    const world::WorldData world = LoadWorld();
    for (const world::Cell& cell : world.Cells())
    {
        if (cell.parent.IsValid() || cell.lightGroups.empty())
        {
            continue;
        }
        const bool hasAutomaticGroup =
            std::any_of(cell.lightGroups.begin(),
                        cell.lightGroups.end(),
                        [&world](const Id group)
                        {
                            const auto schedule = std::find_if(world.LightSchedules().begin(),
                                                               world.LightSchedules().end(),
                                                               [group](const world::LightSchedule& row)
                                                               { return row.group == group; });
                            return schedule != world.LightSchedules().end() &&
                                   schedule->scheduleClass != world::LightScheduleClass::Off;
                        });
        EXPECT_TRUE(hasAutomaticGroup) << cell.name << " has fixtures that can never turn on";
    }
}

TEST(LightScheduleTests, ExplicitGroupStateOverridesTheSchedule)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    house.clock.calendarDaysPerSimDay = 1.0;
    house.clock.dstRulesUS = false;
    cnahouse::environment::CivilTime time;
    time.year = 2031;
    time.month = 1;
    time.day = 15;
    const Id living = Id::Of("LG_L0_LIVING_MAIN");

    time.hour = 12;
    house.clock.SetStandard(time);
    ASSERT_TRUE(house.lighting.SetGroupOn(living, true));
    house.lighting.Update(Frame(110));
    EXPECT_TRUE(house.lighting.FindGroup(living)->on);

    time.hour = 22;
    house.clock.SetStandard(time);
    ASSERT_TRUE(house.lighting.SetGroupOn(living, false));
    house.lighting.Update(Frame(111));
    EXPECT_FALSE(house.lighting.FindGroup(living)->on);
}

TEST(LightingSystemTests, EveryGroupTheLightsNameGetsAState)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    const world::WorldData& world = house.world;
    const LightingSystem& lighting = house.lighting;

    // Every light's group has a state, and no state was invented for a group no light belongs to.
    std::vector<std::uint32_t> fromLights;
    for (const world::Light& light : world.Lights())
    {
        if (light.group.IsValid())
        {
            fromLights.push_back(light.group.Value());
        }
    }
    std::sort(fromLights.begin(), fromLights.end());
    fromLights.erase(std::unique(fromLights.begin(), fromLights.end()), fromLights.end());
    EXPECT_EQ(lighting.Groups().size(), fromLights.size());
    for (const std::uint32_t group : fromLights)
    {
        const Id id(group);
        EXPECT_NE(lighting.FindGroup(id), nullptr);

        Microsoft::Xna::Framework::Vector3 expected;
        float totalLumens = 0.0F;
        for (const world::Light& light : world.Lights())
        {
            if (light.group != id)
            {
                continue;
            }
            const float lumens = std::max(light.intensityLm, 0.0F);
            const auto color = PlanckianRgb(light.colorK);
            expected.X += color.X * lumens;
            expected.Y += color.Y * lumens;
            expected.Z += color.Z * lumens;
            totalLumens += lumens;
        }
        ASSERT_GT(totalLumens, 0.0F);
        expected.X /= totalLumens;
        expected.Y /= totalLumens;
        expected.Z /= totalLumens;
        const auto actual = lighting.GroupColor(id);
        EXPECT_NEAR(actual.X, expected.X, 1e-6F);
        EXPECT_NEAR(actual.Y, expected.Y, 1e-6F);
        EXPECT_NEAR(actual.Z, expected.Z, 1e-6F);
    }
    EXPECT_EQ(lighting.Cells().size(), world.Cells().size());
    std::printf("  %zu cell(s), %zu switch group(s) over %zu fixture(s)\n",
                lighting.Cells().size(),
                lighting.Groups().size(),
                world.Lights().size());
}

TEST(LightingSystemTests, FirstUpdateAppliesTheAuthoredScheduleAndEntryLightsCanBeOverridden)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    // The first update replaces fixture defaults with the one schedule row for each group.
    HouseLighting house;
    const world::WorldData& world = house.world;
    LightingSystem& lighting = house.lighting;
    lighting.Update(Frame(1));

    for (const SwitchGroupState& state : lighting.Groups())
    {
        bool expectedOn = false;
        const auto schedule =
            std::find_if(world.LightSchedules().begin(),
                         world.LightSchedules().end(),
                         [&state](const world::LightSchedule& row) { return row.group == state.group; });
        ASSERT_NE(schedule, world.LightSchedules().end());
        expectedOn = LightScheduleOn(schedule->scheduleClass, house.clock, state.group);
        if (lighting.IsGroupDuskControlled(state.group))
        {
            expectedOn = std::any_of(world.Lights().begin(),
                                     world.Lights().end(),
                                     [&house, &state](const world::Light& light)
                                     {
                                         return light.group == state.group && light.duskSensor &&
                                                DuskSensorOn(house.clock, light.id);
                                     });
        }
        EXPECT_EQ(state.on, expectedOn) << "initial group state disagrees with its control owner";
    }
    for (const RoomLightState& cell : lighting.Cells())
    {
        const auto groups = lighting.GroupsForCell(cell.cell);
        const bool hasOnGroup = std::any_of(
            groups.begin(), groups.end(), [&lighting](const Id id) { return lighting.FindGroup(id)->on; });
        if (hasOnGroup)
        {
            EXPECT_GT(cell.artificial, 0.0F);
            EXPECT_GT(cell.Level(), kAmbientFloor);
        }
        else
        {
            EXPECT_FLOAT_EQ(cell.artificial, 0.0F);
            EXPECT_EQ(cell.artificialColor, Microsoft::Xna::Framework::Vector3());
            EXPECT_GE(cell.Level(), kAmbientFloor);
        }
    }

    const Id foyerMain = Id::Of("LG_L0_FOYER_MAIN");
    const Id hallMain = Id::Of("LG_L0_HALL_MAIN");
    const Id diningChandelier = Id::Of("LG_L0_DINING_CHANDELIER");
    ASSERT_NE(lighting.FindGroup(foyerMain), nullptr);
    ASSERT_NE(lighting.FindGroup(hallMain), nullptr);
    ASSERT_NE(lighting.FindGroup(diningChandelier), nullptr);
    EXPECT_TRUE(lighting.FindGroup(foyerMain)->on);
    EXPECT_TRUE(lighting.FindGroup(hallMain)->on);
    EXPECT_TRUE(lighting.FindGroup(diningChandelier)->on)
        << "the windowless dining room should be readable on the new-game route";
    const float foyerScheduled = lighting.FindCell(Id::Of("L0_FOYER"))->artificial;
    const float hallScheduled = lighting.FindCell(Id::Of("L0_HALL"))->artificial;
    EXPECT_TRUE(lighting.SetGroupOn(foyerMain, false));
    EXPECT_TRUE(lighting.SetGroupOn(hallMain, false));
    lighting.Update(Frame(2));
    const RoomLightState* foyer = lighting.FindCell(Id::Of("L0_FOYER"));
    const RoomLightState* hall = lighting.FindCell(Id::Of("L0_HALL"));
    ASSERT_NE(foyer, nullptr);
    ASSERT_NE(hall, nullptr);
    EXPECT_FALSE(lighting.FindGroup(foyerMain)->on);
    EXPECT_FALSE(lighting.FindGroup(hallMain)->on);
    EXPECT_LT(foyer->artificial, foyerScheduled);
    EXPECT_LT(hall->artificial, hallScheduled);
}

TEST(LightingSystemTests, AllTwentyEightAuthoredDuskFixturesFollowDayNightWithAVisibleStagger)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    house.clock.calendarDaysPerSimDay = 1.0;
    const Id porch = Id::Of("LG_L0_PORCH_LANTERN");
    const Id facadeUplights = Id::Of("LG_EXT_FACADE_UPLIGHT");
    const Id garageLanterns = Id::Of("LG_EXT_GARAGE_LANTERN");
    const Id drivewayEdge = Id::Of("LG_EXT_DRIVEWAY_EDGE");
    const Id terrace = Id::Of("LG_EXT_TERRACE_MAIN");
    const Id street = Id::Of("LG_EXT_STREET");
    const Id neighbours = Id::Of("LG_EXT_NEIGHBOUR_PORCH");
    ASSERT_TRUE(house.lighting.IsGroupDuskControlled(porch));
    ASSERT_TRUE(house.lighting.IsGroupDuskControlled(facadeUplights));
    ASSERT_TRUE(house.lighting.IsGroupDuskControlled(garageLanterns));
    ASSERT_TRUE(house.lighting.IsGroupDuskControlled(drivewayEdge));
    ASSERT_TRUE(house.lighting.IsGroupDuskControlled(terrace));
    ASSERT_TRUE(house.lighting.IsGroupDuskControlled(street));
    ASSERT_TRUE(house.lighting.IsGroupDuskControlled(neighbours));
    EXPECT_EQ(std::count_if(house.world.Lights().begin(),
                            house.world.Lights().end(),
                            [](const world::Light& light) { return light.duskSensor; }),
              28);

    cnahouse::environment::CivilTime time;
    time.year = 2031;
    time.month = 6;
    time.day = 14;
    time.hour = 12;
    house.clock.SetStandard(time);
    house.lighting.Update(Frame(200));
    EXPECT_FALSE(house.lighting.FindGroup(porch)->on);
    EXPECT_FLOAT_EQ(house.lighting.FindGroup(porch)->Level(), 0.0F);
    EXPECT_FALSE(house.lighting.FindGroup(facadeUplights)->on);
    EXPECT_FALSE(house.lighting.FindGroup(garageLanterns)->on);
    EXPECT_FALSE(house.lighting.FindGroup(drivewayEdge)->on);
    EXPECT_FALSE(house.lighting.FindGroup(street)->on);
    EXPECT_FALSE(house.lighting.FindGroup(neighbours)->on);

    time.hour = 22;
    house.clock.SetStandard(time);
    house.lighting.Update(Frame(201));
    EXPECT_TRUE(house.lighting.FindGroup(porch)->on);
    EXPECT_FLOAT_EQ(house.lighting.FindGroup(porch)->Level(), 1.0F);
    EXPECT_TRUE(house.lighting.FindGroup(facadeUplights)->on);
    EXPECT_FLOAT_EQ(house.lighting.FindGroup(facadeUplights)->Level(), 1.0F);
    EXPECT_TRUE(house.lighting.FindGroup(garageLanterns)->on);
    EXPECT_FLOAT_EQ(house.lighting.FindGroup(garageLanterns)->Level(), 1.0F);
    EXPECT_TRUE(house.lighting.FindGroup(drivewayEdge)->on);
    EXPECT_FLOAT_EQ(house.lighting.FindGroup(drivewayEdge)->Level(), 1.0F);
    EXPECT_TRUE(house.lighting.FindGroup(street)->on);
    EXPECT_FLOAT_EQ(house.lighting.FindGroup(street)->Level(), 1.0F);
    EXPECT_TRUE(house.lighting.FindGroup(neighbours)->on);
    EXPECT_FLOAT_EQ(house.lighting.FindGroup(neighbours)->Level(), 1.0F);

    bool sawPorchStagger = false;
    time.hour = 18;
    for (int minute = 0; minute < 240; ++minute)
    {
        time.hour = 18 + minute / 60;
        time.minute = minute % 60;
        house.clock.SetStandard(time);
        house.lighting.Update(Frame(static_cast<std::uint64_t>(300 + minute)));
        const float level = house.lighting.FindGroup(porch)->Level();
        sawPorchStagger = sawPorchStagger || (level > 0.0F && level < 1.0F);
    }
    EXPECT_TRUE(sawPorchStagger) << "the four staggered porch fixtures switched together";
}

TEST(LightingSystemTests, KitchenMainAndIslandDefaultOnAndTheirSwitchesRemoveBorrowedHallLight)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    LightingSystem& lighting = house.lighting;
    const Id kitchenMain = Id::Of("LG_L0_KITCHEN_MAIN");
    const Id kitchenIsland = Id::Of("LG_L0_KITCHEN_ISLAND");
    const SwitchGroupState* mainGroup = lighting.FindGroup(kitchenMain);
    const SwitchGroupState* islandGroup = lighting.FindGroup(kitchenIsland);
    ASSERT_NE(mainGroup, nullptr);
    ASSERT_NE(islandGroup, nullptr);
    ASSERT_TRUE(mainGroup->on) << "the canonical kitchen main practical should start on";
    ASSERT_TRUE(islandGroup->on) << "the canonical kitchen island pendants should start on";

    // Isolate the permanent kitchen/hall cased opening from the already-on entry fixtures.
    ASSERT_TRUE(lighting.SetGroupOn(Id::Of("LG_L0_FOYER_MAIN"), false));
    ASSERT_TRUE(lighting.SetGroupOn(Id::Of("LG_L0_HALL_MAIN"), false));
    ASSERT_TRUE(lighting.SetGroupOn(Id::Of("LG_L0_KITCHEN_SINK"), false));
    ASSERT_TRUE(lighting.SetGroupOn(Id::Of("LG_L0_KITCHEN_UNDERCAB"), false));
    cnahouse::environment::CivilTime evening;
    evening.year = 2031;
    evening.month = 6;
    evening.day = 21;
    evening.hour = 22;
    house.clock.SetStandard(evening);
    lighting.Update(Frame(3));
    const RoomLightState* litKitchen = lighting.FindCell(Id::Of("L0_KITCHEN"));
    const RoomLightState* litHall = lighting.FindCell(Id::Of("L0_HALL"));
    ASSERT_NE(litKitchen, nullptr);
    ASSERT_NE(litHall, nullptr);
    ASSERT_GT(litKitchen->artificial, 0.0F);
    const float borrowedFromKitchen = litHall->borrowed;
    EXPECT_GT(borrowedFromKitchen, 0.0F);

    ASSERT_TRUE(lighting.SetGroupOn(kitchenMain, false));
    ASSERT_TRUE(lighting.SetGroupOn(kitchenIsland, false));
    lighting.Update(Frame(4));
    const RoomLightState* darkKitchen = lighting.FindCell(Id::Of("L0_KITCHEN"));
    const RoomLightState* darkHall = lighting.FindCell(Id::Of("L0_HALL"));
    ASSERT_NE(darkKitchen, nullptr);
    ASSERT_NE(darkHall, nullptr);
    EXPECT_FLOAT_EQ(darkKitchen->artificial, 0.0F);
    EXPECT_LT(darkHall->borrowed, borrowedFromKitchen);
}

TEST(LightingSystemTests, FamilyMainDefaultsOnAndBorrowsThroughTheKitchenOpening)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    LightingSystem& lighting = house.lighting;
    const Id familyMain = Id::Of("LG_L0_FAMILY_MAIN");
    const SwitchGroupState* mainGroup = lighting.FindGroup(familyMain);
    ASSERT_NE(mainGroup, nullptr);
    ASSERT_TRUE(mainGroup->on) << "the connected family practical should start on";

    // Leave only the permanent kitchen/family opening as a contributor in the evening schedule.
    ASSERT_TRUE(lighting.SetGroupOn(Id::Of("LG_L0_FOYER_MAIN"), false));
    ASSERT_TRUE(lighting.SetGroupOn(Id::Of("LG_L0_HALL_MAIN"), false));
    ASSERT_TRUE(lighting.SetGroupOn(Id::Of("LG_L0_KITCHEN_MAIN"), false));
    ASSERT_TRUE(lighting.SetGroupOn(Id::Of("LG_L0_FAMILY_MEDIA"), false));
    ASSERT_TRUE(lighting.SetGroupOn(Id::Of("LG_L0_FAMILY_READING"), false));
    cnahouse::environment::CivilTime evening;
    evening.year = 2031;
    evening.month = 6;
    evening.day = 21;
    evening.hour = 22;
    house.clock.SetStandard(evening);
    lighting.Update(Frame(5));
    const RoomLightState* litFamily = lighting.FindCell(Id::Of("L0_FAMILY"));
    const RoomLightState* adjacentKitchen = lighting.FindCell(Id::Of("L0_KITCHEN"));
    ASSERT_NE(litFamily, nullptr);
    ASSERT_NE(adjacentKitchen, nullptr);
    ASSERT_GT(litFamily->artificial, 0.0F);
    const float borrowedFromFamily = adjacentKitchen->borrowed;
    EXPECT_GT(borrowedFromFamily, 0.0F);

    ASSERT_TRUE(lighting.SetGroupOn(familyMain, false));
    lighting.Update(Frame(6));
    const RoomLightState* darkFamily = lighting.FindCell(Id::Of("L0_FAMILY"));
    const RoomLightState* darkerKitchen = lighting.FindCell(Id::Of("L0_KITCHEN"));
    ASSERT_NE(darkFamily, nullptr);
    ASSERT_NE(darkerKitchen, nullptr);
    EXPECT_FLOAT_EQ(darkFamily->artificial, 0.0F);
    EXPECT_LT(darkerKitchen->borrowed, borrowedFromFamily);
}

TEST(LightingSystemTests, LivingPianoAccentAndPhysicalMainPracticalsDefaultOn)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    const SwitchGroupState* piano = house.lighting.FindGroup(Id::Of("LG_L0_LIVING_PIANO"));
    const SwitchGroupState* main = house.lighting.FindGroup(Id::Of("LG_L0_LIVING_MAIN"));
    ASSERT_NE(piano, nullptr);
    ASSERT_NE(main, nullptr);
    EXPECT_TRUE(piano->on) << "the formal-room focal piece needs its authored accent";
    EXPECT_TRUE(main->on) << "the broad physical practicals make the formal room readable";
}

TEST(LightingSystemTests, DaylitFixedDetailCanReceiveItsOwnSwitchedFixturesWithoutNeighbourLeak)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    cnahouse::environment::CivilTime noon;
    noon.year = 2031;
    noon.month = 6;
    noon.day = 21;
    noon.hour = 12;
    house.clock.SetStandard(noon);
    for (const SwitchGroupState& group : house.lighting.Groups())
    {
        ASSERT_TRUE(house.lighting.SetGroupOn(group.group, false));
    }
    const Id living = Id::Of("L0_LIVING");
    const Id bedroom = Id::Of("L1_BED2");
    const Microsoft::Xna::Framework::Vector3 centre(-5.20F, 1.40F, -17.25F);
    house.lighting.Update(Frame(20));
    ASSERT_NE(house.lighting.SunKeyForCell(living), nullptr)
        << "this probes a room where the ordinary stock-effect slots choose the sun";
    EXPECT_FALSE(house.lighting.StaticFixtureLightsForObject(living, centre).slots[0].has_value());

    ASSERT_TRUE(house.lighting.SetGroupOn(Id::Of("LG_L0_LIVING_MAIN"), true));
    house.lighting.Update(Frame(21));
    const ObjectLightAssignment local = house.lighting.StaticFixtureLightsForObject(living, centre);
    ASSERT_TRUE(local.slots[0].has_value())
        << "sunlight must not erase an active, authored practical from fixed-detail lighting";
    EXPECT_GT(local.slots[0]->diffuseColor.X, 0.0F);
    EXPECT_FALSE(
        house.lighting
            .StaticFixtureLightsForObject(bedroom, Microsoft::Xna::Framework::Vector3(4.0F, 4.3F, -23.0F))
            .slots[0]
            .has_value())
        << "the living light is not a general neighbouring-room fill";
}

TEST(LightingSystemTests, TurningOnEveryGroupInARoomLightsItExactlyFully)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    LightingSystem& lighting = house.lighting;
    for (const SwitchGroupState& group : lighting.Groups())
    {
        EXPECT_TRUE(lighting.SetGroupOn(group.group, true));
    }
    lighting.Update(Frame(2));

    int lit = 0;
    int unlit = 0;
    for (const RoomLightState& cell : lighting.Cells())
    {
        if (lighting.GroupsForCell(cell.cell).empty())
        {
            // A cell with no fixtures cannot be lit by switches, and must not be a division by
            // zero: §16's 96 cells include closets, shafts and the outdoors.
            EXPECT_FLOAT_EQ(cell.artificial, 0.0F);
            ++unlit;
            continue;
        }
        EXPECT_FLOAT_EQ(cell.artificial, 1.0F) << "cell " << cell.cell.Value();
        EXPECT_FLOAT_EQ(cell.Level(), 1.0F);
        ++lit;
    }
    EXPECT_GT(lit, 60) << "the house has 86 cells with light groups";
    std::printf("  %d cell(s) fully lit, %d with no fixtures at all\n", lit, unlit);
}

TEST(LightingSystemTests, ARoomsLevelIsWeightedByLumensAndNotByFixtureCount)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    // The one arithmetic decision in this task. Find a cell with two groups of genuinely different
    // size and assert that the BIGGER one moves the room further -- which a mean over groups would
    // not do, and which is the whole reason a kitchen's cabinet strip is not a fifth of its light.
    HouseLighting house;
    const world::WorldData& world = house.world;
    LightingSystem& lighting = house.lighting;

    bool found = false;
    for (const world::Cell& cell : world.Cells())
    {
        const std::span<const Id> groups = lighting.GroupsForCell(cell.id);
        if (groups.size() < 2)
        {
            continue;
        }
        Id brightest = groups.front();
        Id dimmest = groups.front();
        for (const Id& group : groups)
        {
            if (lighting.GroupLumens(group) > lighting.GroupLumens(brightest))
            {
                brightest = group;
            }
            if (lighting.GroupLumens(group) < lighting.GroupLumens(dimmest))
            {
                dimmest = group;
            }
        }
        if (lighting.GroupLumens(brightest) <= lighting.GroupLumens(dimmest) * 1.5F)
        {
            continue;
        }

        for (const SwitchGroupState& group : lighting.Groups())
        {
            lighting.SetGroupOn(group.group, false);
        }
        lighting.SetGroupOn(brightest, true);
        lighting.Update(Frame(3));
        const float withBrightest = lighting.FindCell(cell.id)->artificial;

        lighting.SetGroupOn(brightest, false);
        lighting.SetGroupOn(dimmest, true);
        lighting.Update(Frame(4));
        const float withDimmest = lighting.FindCell(cell.id)->artificial;

        EXPECT_GT(withBrightest, withDimmest)
            << "cell " << cell.name << ": the brighter group did not light the room more";
        std::printf("  %s: %.0f lm group gives %.3f, %.0f lm group gives %.3f\n",
                    cell.name.c_str(),
                    static_cast<double>(lighting.GroupLumens(brightest)),
                    static_cast<double>(withBrightest),
                    static_cast<double>(lighting.GroupLumens(dimmest)),
                    static_cast<double>(withDimmest));
        found = true;
        break;
    }
    EXPECT_TRUE(found) << "no cell has two groups of different size, so the weighting is untested";
}

TEST(LightingSystemTests, ACellsArtificialColourTracksOnlyTheGroupsThatAreOn)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    LightingSystem& lighting = house.lighting;

    bool found = false;
    for (const world::Cell& cell : house.world.Cells())
    {
        const std::span<const Id> groups = lighting.GroupsForCell(cell.id);
        if (groups.size() < 2)
        {
            continue;
        }
        Id warmest = groups.front();
        Id coolest = groups.front();
        for (const Id& group : groups)
        {
            if (lighting.GroupColor(group).Z < lighting.GroupColor(warmest).Z)
            {
                warmest = group;
            }
            if (lighting.GroupColor(group).Z > lighting.GroupColor(coolest).Z)
            {
                coolest = group;
            }
        }
        if (lighting.GroupColor(coolest).Z - lighting.GroupColor(warmest).Z < 0.1F)
        {
            continue;
        }

        for (const SwitchGroupState& group : lighting.Groups())
        {
            lighting.SetGroupOn(group.group, false);
        }
        lighting.SetGroupOn(warmest, true);
        lighting.Update(Frame(30));
        const auto warmOnly = lighting.FindCell(cell.id)->artificialColor;
        EXPECT_NEAR(warmOnly.X, lighting.GroupColor(warmest).X, 1e-6F);
        EXPECT_NEAR(warmOnly.Y, lighting.GroupColor(warmest).Y, 1e-6F);
        EXPECT_NEAR(warmOnly.Z, lighting.GroupColor(warmest).Z, 1e-6F);

        lighting.SetGroupOn(warmest, false);
        lighting.SetGroupOn(coolest, true);
        lighting.Update(Frame(31));
        const auto coolOnly = lighting.FindCell(cell.id)->artificialColor;
        EXPECT_NEAR(coolOnly.X, lighting.GroupColor(coolest).X, 1e-6F);
        EXPECT_NEAR(coolOnly.Y, lighting.GroupColor(coolest).Y, 1e-6F);
        EXPECT_NEAR(coolOnly.Z, lighting.GroupColor(coolest).Z, 1e-6F);
        EXPECT_GT(coolOnly.Z, warmOnly.Z);

        lighting.SetGroupOn(warmest, true);
        lighting.Update(Frame(32));
        const float warmLumens = lighting.GroupLumens(warmest);
        const float coolLumens = lighting.GroupLumens(coolest);
        const auto mixed = lighting.FindCell(cell.id)->artificialColor;
        const auto warmColor = lighting.GroupColor(warmest);
        const auto coolColor = lighting.GroupColor(coolest);
        EXPECT_NEAR(
            mixed.Z, (warmColor.Z * warmLumens + coolColor.Z * coolLumens) / (warmLumens + coolLumens), 1e-6F)
            << "the active colour was averaged by group count rather than lumens";
        found = true;
        break;
    }
    EXPECT_TRUE(found) << "no room has two visibly different colour temperatures";
}

TEST(LightingSystemTests, ADimmerScalesTheGroupAndIsRefusedWhenItIsNotANumber)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    LightingSystem& lighting = house.lighting;
    ASSERT_FALSE(lighting.Groups().empty());
    const Id group = lighting.Groups().front().group;

    lighting.SetGroupOn(group, true);
    EXPECT_TRUE(lighting.SetGroupDimmer(group, 0.5F));
    EXPECT_FLOAT_EQ(lighting.FindGroup(group)->Level(), 0.5F);

    // Out of range is clamped, not extrapolated: a settings file must not make a room brighter
    // than a room can be, nor negative.
    EXPECT_TRUE(lighting.SetGroupDimmer(group, 4.0F));
    EXPECT_FLOAT_EQ(lighting.FindGroup(group)->Level(), 1.0F);
    EXPECT_TRUE(lighting.SetGroupDimmer(group, -2.0F));
    EXPECT_FLOAT_EQ(lighting.FindGroup(group)->Level(), 0.0F);

    // ...and a NaN is refused outright rather than clamped, because clamping a NaN keeps the NaN.
    EXPECT_TRUE(lighting.SetGroupDimmer(group, 0.75F));
    EXPECT_FALSE(lighting.SetGroupDimmer(group, std::nan("")));
    EXPECT_FLOAT_EQ(lighting.FindGroup(group)->Level(), 0.75F) << "the refused value was applied";

    // A switch that is OFF is off whatever the dimmer says.
    lighting.SetGroupOn(group, false);
    EXPECT_FLOAT_EQ(lighting.FindGroup(group)->Level(), 0.0F);
}

TEST(LightingSystemTests, AnUnknownGroupOrCellIsReportedRatherThanInvented)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    LightingSystem& lighting = house.lighting;
    const Id nonsense = Id::Of("LG_THERE_IS_NO_SUCH_GROUP");
    EXPECT_FALSE(lighting.SetGroupOn(nonsense, true));
    EXPECT_FALSE(lighting.SetGroupDimmer(nonsense, 0.5F));
    EXPECT_EQ(lighting.FindGroup(nonsense), nullptr);
    EXPECT_EQ(lighting.FindCell(Id::Of("NO_SUCH_CELL")), nullptr);
    EXPECT_TRUE(lighting.GroupsForCell(Id::Of("NO_SUCH_CELL")).empty());
    EXPECT_FLOAT_EQ(lighting.GroupLevelInCell(Id::Of("NO_SUCH_CELL"), nonsense), 0.0F);
    EXPECT_EQ(lighting.GroupColor(nonsense), Microsoft::Xna::Framework::Vector3());
}

TEST(LightingSystemTests, AGroupOnlyLightsTheCellsThatListIt)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    // §23.3 draws one additive pass per group per cell, so `GroupLevelInCell` is what picks the
    // pass. A group that reported a level in a room it does not light would put a kitchen's light
    // on a bedroom wall.
    HouseLighting house;
    const world::WorldData& world = house.world;
    LightingSystem& lighting = house.lighting;
    for (const SwitchGroupState& group : lighting.Groups())
    {
        lighting.SetGroupOn(group.group, true);
    }
    lighting.Update(Frame(5));

    int pairs = 0;
    for (const world::Cell& cell : world.Cells())
    {
        const std::span<const Id> own = lighting.GroupsForCell(cell.id);
        for (const SwitchGroupState& group : lighting.Groups())
        {
            const bool lightsIt = std::find(own.begin(), own.end(), group.group) != own.end();
            const float level = lighting.GroupLevelInCell(cell.id, group.group);
            if (lightsIt)
            {
                EXPECT_FLOAT_EQ(level, 1.0F);
                ++pairs;
            }
            else
            {
                EXPECT_FLOAT_EQ(level, 0.0F);
            }
        }
    }
    EXPECT_GT(pairs, 100) << "the house's cells name " << pairs << " (cell, group) pairs in all";
}

TEST(LightingSystemTests, TheFrameIndexIsPublishedSoAConsumerCanTellItIsReadingThisFrame)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    // The same guard `VisibilitySystem` carries, for the same reason: a stage accidentally ordered
    // before `UpdateStage::Lighting` reads last frame's levels and nothing else would say so.
    HouseLighting house;
    LightingSystem& lighting = house.lighting;
    EXPECT_EQ(lighting.ComputedForFrame(), 0U);
    lighting.Update(Frame(17));
    EXPECT_EQ(lighting.ComputedForFrame(), 17U);
}

TEST(LightingSystemTests, TheLevelSaturatesRatherThanSummingPastOne)
{
    // `RoomLightState::Level` on its own, with no world: the one formula §28.1 leaves to be
    // decided. Two full sources must not make a room twice as bright as it can be, and a second
    // source must still do something -- §30 has a row about a player watching exactly that.
    RoomLightState state;
    EXPECT_FLOAT_EQ(state.Level(), kAmbientFloor) << "nothing lit is the floor, not zero";

    state.artificial = 1.0F;
    EXPECT_FLOAT_EQ(state.Level(), 1.0F);

    state.daylight = 1.0F;
    EXPECT_FLOAT_EQ(state.Level(), 1.0F) << "two full sources are still one fully lit room";

    state = RoomLightState{};
    state.artificial = 0.4F;
    const float aloneL = state.Level();
    state.daylight = 0.4F;
    const float togetherL = state.Level();
    EXPECT_GT(togetherL, aloneL) << "adding a second source did nothing";
    EXPECT_LE(togetherL, 1.0F);

    // Monotone in each source, which is the property every consumer will assume.
    float previous = 0.0F;
    for (float step = 0.0F; step <= 1.0F; step += 0.05F)
    {
        RoomLightState probe;
        probe.artificial = 0.3F;
        probe.daylight = step;
        const float level = probe.Level();
        EXPECT_GE(level, previous - 1e-6F);
        previous = level;
    }
}

TEST(LightingSystemTests, DaylightIsKeyOnlyAboveSectionTwentyEightsThreshold)
{
    // §28.5 assigns `DirectionalLight0` from the sun when daylight ≥ 0.15 and from a fixture
    // otherwise. The threshold is a number in the architecture, so it is a named constant and this
    // is where it is pinned to it.
    EXPECT_FLOAT_EQ(kDaylightKeyThreshold, 0.15F);
    RoomLightState state;
    state.daylight = 0.14F;
    EXPECT_FALSE(state.DaylightIsKey());
    state.daylight = 0.15F;
    EXPECT_TRUE(state.DaylightIsKey());
}

TEST(LightingSystemTests, ObjectsReceiveFixtureKeyFillAndSurfaceTintedBounceInStableSlots)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    house.clock.calendarDaysPerSimDay = 1.0;
    cnahouse::environment::CivilTime night;
    night.year = 2031;
    night.month = 6;
    night.day = 21;
    night.hour = 22;
    house.clock.SetStandard(night);
    ASSERT_TRUE(house.lighting.SetGroupOn(Id::Of("LG_EXT_WALK_PATH"), false));
    ASSERT_TRUE(house.lighting.SetGroupOn(Id::Of("LG_EXT_DRIVEWAY_FLOOD"), false));
    ASSERT_TRUE(house.lighting.SetGroupOn(Id::Of("LG_L0_GARAGE_MAIN"), false));
    ASSERT_TRUE(house.lighting.SetGroupOn(Id::Of("LG_L0_GARAGE_OPENER"), false));
    house.lighting.Update(Frame(30));

    const Id porch = Id::Of("L0_PORCH");
    const Microsoft::Xna::Framework::Vector3 centre(0.0F, 0.57F, -12.95F);
    const ObjectLightAssignment lights = house.lighting.DirectionalLightsForObject(porch, centre);
    ASSERT_TRUE(lights.slots[0].has_value());
    ASSERT_TRUE(lights.slots[1].has_value());
    ASSERT_TRUE(lights.slots[2].has_value());
    // The two broad semi-flush fixtures are the strongest direct porch sources at deck centre;
    // the wall lanterns remain in the same group and dominate the closer door receiver below.
    const float dx = 2.45F;
    const float dy = 0.57F - 3.17F;
    const float dz = 0.0F;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    EXPECT_NEAR(lights.slots[0]->direction.X, dx / distance, 1.0e-6F);
    EXPECT_NEAR(lights.slots[0]->direction.Y, dy / distance, 1.0e-6F);
    EXPECT_NEAR(lights.slots[0]->direction.Z, dz / distance, 1.0e-6F);
    EXPECT_NEAR(lights.slots[1]->direction.X, -dx / distance, 1.0e-6F);
    EXPECT_NEAR(lights.slots[1]->direction.Y, dy / distance, 1.0e-6F);
    EXPECT_NEAR(lights.slots[1]->direction.Z, dz / distance, 1.0e-6F);
    EXPECT_NEAR(lights.slots[0]->diffuseColor.X, lights.slots[1]->diffuseColor.X, 1.0e-6F)
        << "the symmetric porch receiver must retain equal energy after distance attenuation";
    const float pointAttenuation = PointLightAttenuation(distance, 7.28F);
    const float ceilingShare = 1000.0F * pointAttenuation / 2800.0F;
    EXPECT_NEAR(lights.slots[0]->diffuseColor.X, ceilingShare * PlanckianRgb(2700.0F).X, 1.0e-6F);
    const float bounceLength = std::sqrt(dy * dy + dz * dz);
    EXPECT_NEAR(lights.slots[2]->direction.X, 0.0F, 1.0e-6F);
    EXPECT_NEAR(lights.slots[2]->direction.Y, -dy / bounceLength, 1.0e-6F);
    EXPECT_NEAR(lights.slots[2]->direction.Z, -dz / bounceLength, 1.0e-6F);
    EXPECT_GT(lights.slots[2]->diffuseColor.X, 0.0F);
    EXPECT_LT(lights.slots[2]->diffuseColor.X,
              lights.slots[0]->diffuseColor.X + lights.slots[1]->diffuseColor.X);

    // The same two sources have an explicit foreign lightmap binding on the foyer facade. Basic
    // detail on that receiver (the entry leaf and hardware) must use those porch sources, while a
    // cell with no foreign binding must not gain a neighbouring light by proximity alone.
    const Id foyer = Id::Of("L0_FOYER");
    const Microsoft::Xna::Framework::Vector3 doorCentre(0.0F, 1.65F, -14.15F);
    const ObjectLightAssignment receiverLights =
        house.lighting.CrossCellReceiverLightsForObject(foyer, doorCentre);
    ASSERT_TRUE(receiverLights.slots[0].has_value());
    ASSERT_TRUE(receiverLights.slots[1].has_value());
    ASSERT_TRUE(receiverLights.slots[2].has_value());
    EXPECT_NEAR(receiverLights.slots[0]->direction.X, -receiverLights.slots[1]->direction.X, 1.0e-6F);
    EXPECT_GT(receiverLights.slots[0]->diffuseColor.X, receiverLights.slots[0]->diffuseColor.Z);
    EXPECT_FALSE(house.lighting.CrossCellReceiverLightsForObject(porch, centre).slots[0].has_value());

    // The two outer fixtures also name only the two facade owners physically below their bays.
    // This keeps the wide entry readable without leaking the porch group into unrelated rooms.
    const ObjectLightAssignment livingFacade = house.lighting.CrossCellReceiverLightsForObject(
        Id::Of("L0_LIVING"), Microsoft::Xna::Framework::Vector3(-3.0F, 1.7F, -14.15F));
    const ObjectLightAssignment stairFacade = house.lighting.CrossCellReceiverLightsForObject(
        Id::Of("L0_STAIR_MAIN"), Microsoft::Xna::Framework::Vector3(3.0F, 1.7F, -14.15F));
    ASSERT_TRUE(livingFacade.slots[0].has_value());
    ASSERT_TRUE(stairFacade.slots[0].has_value());
    EXPECT_GT(livingFacade.slots[0]->diffuseColor.X, livingFacade.slots[0]->diffuseColor.Z);
    EXPECT_GT(stairFacade.slots[0]->diffuseColor.X, stairFacade.slots[0]->diffuseColor.Z);

    // The front stair is fixed Basic detail in EXT_WALK. Its path schedule is explicitly
    // overridden off above so this assertion isolates the two range-bounded porch sources.
    const Id walk = Id::Of("EXT_WALK");
    const Microsoft::Xna::Framework::Vector3 stepCentre(0.0F, 0.285F, -11.1375F);
    EXPECT_FALSE(house.lighting.DirectionalLightsForObject(walk, stepCentre).slots[0].has_value());
    const ObjectLightAssignment stepLights = house.lighting.StaticDetailLightsForObject(walk, stepCentre);
    ASSERT_TRUE(stepLights.slots[0].has_value());
    ASSERT_TRUE(stepLights.slots[1].has_value());
    ASSERT_TRUE(stepLights.slots[2].has_value());
    EXPECT_NEAR(stepLights.slots[0]->direction.X, -stepLights.slots[1]->direction.X, 1.0e-6F);
    EXPECT_GT(stepLights.slots[0]->diffuseColor.X, stepLights.slots[0]->diffuseColor.Z);
    EXPECT_GT(stepLights.spillDiffuseColor.X, stepLights.spillDiffuseColor.Z);
    EXPECT_GT(stepLights.spillDiffuseColor.Z, 0.0F);

    // The garage flood aims away from the wall, so it is an unbaked fixed-detail spill rather than
    // a fake facade lightmap. Its dusk schedule is explicitly overridden off above, then on below,
    // proving that it reaches the garage door only through its explicit receiver id.
    // The terrain tile under the driveway is resident in EXT_SIDEYARD_E by largest overlap, so
    // that exact receiver is named too; otherwise switching the flood would light the door but
    // leave the asphalt beneath its cone byte-identical to the off frame.
    const Id garage = Id::Of("L0_GARAGE");
    const Id drivewayTerrain = Id::Of("EXT_SIDEYARD_E");
    const Id garageFlood = Id::Of("LG_EXT_DRIVEWAY_FLOOD");
    const Id garageLanterns = Id::Of("LG_EXT_GARAGE_LANTERN");
    const Microsoft::Xna::Framework::Vector3 garageDoorCentre(13.20F, 1.35F, -13.30F);
    const Microsoft::Xna::Framework::Vector3 drivewayCentre(13.0F, 0.0F, -8.5F);
    const Microsoft::Xna::Framework::Vector3 outsideFloodCone(22.0F, 0.0F, -13.0F);
    const ObjectLightAssignment garageLanternSpill =
        house.lighting.StaticDetailLightsForObject(garage, garageDoorCentre);
    const ObjectLightAssignment garageLanternReceiver =
        house.lighting.CrossCellReceiverLightsForObject(garage, garageDoorCentre);
    ASSERT_TRUE(garageLanternSpill.slots[0].has_value());
    ASSERT_TRUE(garageLanternSpill.slots[1].has_value());
    ASSERT_TRUE(garageLanternReceiver.slots[0].has_value());
    ASSERT_TRUE(garageLanternReceiver.slots[1].has_value());
    EXPECT_GT(garageLanternSpill.slots[0]->diffuseColor.X, garageLanternSpill.slots[0]->diffuseColor.Z);
    EXPECT_GT(garageLanternSpill.spillDiffuseColor.X, garageLanternSpill.spillDiffuseColor.Z);
    EXPECT_FALSE(
        house.lighting.StaticDetailLightsForObject(drivewayTerrain, drivewayCentre).slots[0].has_value());
    ASSERT_TRUE(house.lighting.SetGroupOn(garageFlood, true));
    house.lighting.Update(Frame(31));
    EXPECT_FALSE(house.lighting.DirectionalLightsForObject(garage, garageDoorCentre).slots[0].has_value());
    const ObjectLightAssignment receiverWithFlood =
        house.lighting.CrossCellReceiverLightsForObject(garage, garageDoorCentre);
    ASSERT_TRUE(receiverWithFlood.slots[0].has_value());
    ASSERT_TRUE(receiverWithFlood.slots[1].has_value());
    EXPECT_GT(receiverWithFlood.slots[0]->diffuseColor.X, receiverWithFlood.slots[0]->diffuseColor.Z)
        << "the unbaked work flood must not replace the warm carriage-light receiver pass";
    const ObjectLightAssignment garageSpill =
        house.lighting.StaticDetailLightsForObject(garage, garageDoorCentre);
    ASSERT_TRUE(garageSpill.slots[0].has_value());
    ASSERT_TRUE(garageSpill.slots[2].has_value());
    EXPECT_GT(garageSpill.spillDiffuseColor.X, 0.0F);
    EXPECT_GT(garageSpill.spillDiffuseColor.Y, garageSpill.spillDiffuseColor.Z);
    const ObjectLightAssignment drivewaySpill =
        house.lighting.StaticDetailLightsForObject(drivewayTerrain, drivewayCentre);
    ASSERT_TRUE(drivewaySpill.slots[0].has_value());
    EXPECT_GT(drivewaySpill.spillDiffuseColor.X, 0.0F);
    EXPECT_GT(drivewaySpill.spillDiffuseColor.Y, drivewaySpill.spillDiffuseColor.Z);
    EXPECT_FALSE(
        house.lighting.StaticDetailLightsForObject(drivewayTerrain, outsideFloodCone).slots[0].has_value());
    ASSERT_TRUE(house.lighting.SetGroupOn(garageFlood, false));
    house.lighting.Update(Frame(32));
    EXPECT_TRUE(house.lighting.FindGroup(garageLanterns)->on)
        << "the automatic carriage lights are independent of the work-flood override";
    EXPECT_TRUE(house.lighting.StaticDetailLightsForObject(garage, garageDoorCentre).slots[0].has_value());

    ASSERT_TRUE(house.lighting.SetGroupOn(Id::Of("LG_L0_PORCH_LANTERN"), false));
    const ObjectLightAssignment dark = house.lighting.DirectionalLightsForObject(porch, centre);
    EXPECT_FALSE(dark.slots[0].has_value());
    EXPECT_FALSE(dark.slots[1].has_value());
    EXPECT_FALSE(dark.slots[2].has_value());
    EXPECT_FALSE(house.lighting.CrossCellReceiverLightsForObject(foyer, doorCentre).slots[0].has_value());
    const ObjectLightAssignment darkSteps = house.lighting.StaticDetailLightsForObject(walk, stepCentre);
    EXPECT_FALSE(darkSteps.slots[0].has_value());
    EXPECT_EQ(darkSteps.spillDiffuseColor, Microsoft::Xna::Framework::Vector3());
    EXPECT_FALSE(
        house.lighting.DirectionalLightsForObject(Id::Of("NO_SUCH_CELL"), centre).slots[0].has_value());
}

TEST(LightingSystemTests, PointFixtureAttenuationIsBoundedAndObjectsOutsideRangeReceiveNoFixture)
{
    EXPECT_FLOAT_EQ(PointLightAttenuation(0.0F, 6.5F), 1.0F);
    EXPECT_FLOAT_EQ(PointLightAttenuation(6.5F, 6.5F), 0.5F);
    EXPECT_FLOAT_EQ(PointLightAttenuation(13.0F, 6.5F), 0.0F);
    EXPECT_FLOAT_EQ(PointLightAttenuation(-1.0F, 6.5F), 0.0F);
    EXPECT_FLOAT_EQ(PointLightAttenuation(1.0F, 0.0F), 0.0F);
    EXPECT_FLOAT_EQ(PointLightAttenuation(std::numeric_limits<float>::infinity(), 6.5F), 0.0F);

    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    house.clock.calendarDaysPerSimDay = 1.0;
    cnahouse::environment::CivilTime night;
    night.year = 2031;
    night.month = 6;
    night.day = 21;
    night.hour = 22;
    house.clock.SetStandard(night);
    house.lighting.Update(Frame(31));

    const ObjectLightAssignment outside = house.lighting.DirectionalLightsForObject(
        Id::Of("L0_PORCH"), Microsoft::Xna::Framework::Vector3(0.0F, 0.57F, 100.0F));
    EXPECT_FALSE(outside.slots[0].has_value());
    EXPECT_FALSE(outside.slots[1].has_value());
    EXPECT_FALSE(outside.slots[2].has_value());
}

TEST(LightingSystemTests, SpotFixtureAttenuationUsesAuthoredFullConeAngles)
{
    using cnahouse::lighting::SpotLightAttenuation;
    constexpr float kPi = std::numbers::pi_v<float>;
    const auto cosineAtDegrees = [](float degrees) { return std::cos(degrees * kPi / 180.0F); };

    EXPECT_FLOAT_EQ(SpotLightAttenuation(1.0F, 40.0F, 70.0F), 1.0F);
    EXPECT_FLOAT_EQ(SpotLightAttenuation(cosineAtDegrees(20.0F), 40.0F, 70.0F), 1.0F);
    const float feather = SpotLightAttenuation(cosineAtDegrees(27.5F), 40.0F, 70.0F);
    EXPECT_GT(feather, 0.0F);
    EXPECT_LT(feather, 1.0F);
    EXPECT_FLOAT_EQ(SpotLightAttenuation(cosineAtDegrees(35.0F), 40.0F, 70.0F), 0.0F);
    EXPECT_FLOAT_EQ(SpotLightAttenuation(-1.0F, 40.0F, 70.0F), 0.0F);
    EXPECT_FLOAT_EQ(SpotLightAttenuation(1.0F, 80.0F, 70.0F), 0.0F);
    EXPECT_FLOAT_EQ(SpotLightAttenuation(std::numeric_limits<float>::quiet_NaN(), 40.0F, 70.0F), 0.0F);
}

TEST(LightingSystemTests, TheFramePublishesTheSunAndTheDaylightModelInWorldCellOrder)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    house.clock.calendarDaysPerSimDay = 1.0;
    cnahouse::environment::CivilTime noon;
    noon.year = 2031;
    noon.month = 6;
    noon.day = 21;
    noon.hour = 12;
    house.clock.SetStandard(noon);

    LightingSystem& lighting = house.lighting;
    lighting.Update(Frame(6));
    const auto expectedSun = cnahouse::environment::SunPositionFor(house.clock);
    EXPECT_NEAR(lighting.Sun().altitudeDeg, expectedSun.altitudeDeg, 1e-10);
    EXPECT_NEAR(lighting.Sun().azimuthDeg, expectedSun.azimuthDeg, 1e-10);
    const auto& sky = lighting.SkyAmbientColor();
    const auto& outdoorEnergy = lighting.OutdoorSkyIrradianceColor();
    EXPECT_GT(std::max({outdoorEnergy.X, outdoorEnergy.Y, outdoorEnergy.Z}), std::max({sky.X, sky.Y, sky.Z}))
        << "a clear June noon outer skin must not inherit the dim display-gradient scalar";

    const cnahouse::lighting::DaylightModel oracle(house.world, house.shading);
    std::vector<float> expected(house.world.Cells().size());
    oracle.Evaluate(expectedSun.altitudeDeg, expectedSun.azimuthDeg, lighting.CloudCover(), expected);
    int lit = 0;
    for (std::size_t index = 0; index < lighting.Cells().size(); ++index)
    {
        const RoomLightState& cell = lighting.Cells()[index];
        EXPECT_FLOAT_EQ(cell.daylight, expected[index]) << house.world.Cells()[index].name;
        EXPECT_GE(cell.borrowed, 0.0F);
        EXPECT_LE(cell.borrowed, 1.0F);
        lit += cell.daylight > 0.0F ? 1 : 0;
    }
    EXPECT_GT(lit, 25) << "the wired system left the June-noon house dark";
}

TEST(LightingSystemTests, KitchenLightSpillsIntoTheHallAndItsDoorBrightensThePantry)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    for (const SwitchGroupState& group : house.lighting.Groups())
    {
        ASSERT_TRUE(house.lighting.SetGroupOn(group.group, false));
    }
    cnahouse::environment::CivilTime midnight;
    midnight.year = 2031;
    midnight.month = 6;
    midnight.day = 21;
    midnight.hour = 0;
    house.clock.SetStandard(midnight);
    house.lighting.Update(Frame(20));

    const Id kitchen = Id::Of("L0_KITCHEN");
    const Id hall = Id::Of("L0_HALL");
    const Id pantry = Id::Of("L0_PANTRY");
    const Id kitchenMain = Id::Of("LG_L0_KITCHEN_MAIN");
    ASSERT_TRUE(house.lighting.SetGroupOn(kitchenMain, true));
    house.lighting.Update(Frame(21));
    const RoomLightState* kitchenState = house.lighting.FindCell(kitchen);
    const RoomLightState* hallState = house.lighting.FindCell(hall);
    const RoomLightState* pantryState = house.lighting.FindCell(pantry);
    ASSERT_NE(kitchenState, nullptr);
    ASSERT_NE(hallState, nullptr);
    ASSERT_NE(pantryState, nullptr);
    EXPECT_GT(kitchenState->artificial, 0.0F);
    EXPECT_FLOAT_EQ(hallState->artificial, 0.0F);
    EXPECT_GT(hallState->borrowed, 0.0F) << "the permanent kitchen-to-hall cased opening passed no light";

    // HOUSE-03224 starts the pantry door at its authored 0.90 walkthrough pose, so shut it first
    // to measure the dark side, then open it fully.
    const Id pantryDoor = Id::Of("P_L0_KITCHEN__L0_PANTRY");
    ASSERT_TRUE(house.visibility.SetAperture(pantryDoor, 0.0F));
    house.lighting.Update(Frame(22));
    pantryState = house.lighting.FindCell(pantry);
    ASSERT_NE(pantryState, nullptr);
    const float pantryClosed = pantryState->borrowed;

    ASSERT_TRUE(house.visibility.SetAperture(pantryDoor, 1.0F));
    house.lighting.Update(Frame(23));
    pantryState = house.lighting.FindCell(pantry);
    ASSERT_NE(pantryState, nullptr);
    EXPECT_GT(pantryState->borrowed, pantryClosed)
        << "opening the actual kitchen door did not brighten its dark neighbour";
    EXPECT_LE(pantryState->borrowed, kitchenState->artificial * 0.35F + 1e-6F);
    std::printf("  kitchen spill: hall %.4f; pantry %.4f closed -> %.4f open\n",
                static_cast<double>(hallState->borrowed),
                static_cast<double>(pantryClosed),
                static_cast<double>(pantryState->borrowed));
}

TEST(LightingSystemTests, TheSunIsDirectionalLightZeroOutdoorsAndAboveTheIndoorThreshold)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    house.clock.calendarDaysPerSimDay = 1.0;
    cnahouse::environment::CivilTime noon;
    noon.year = 2031;
    noon.month = 6;
    noon.day = 21;
    noon.hour = 12;
    house.clock.SetStandard(noon);
    house.lighting.Update(Frame(7));

    Id outdoors;
    Id daylitRoom;
    Id darkRoom;
    for (const world::Cell& cell : house.world.Cells())
    {
        if (cell.kind == world::CellKind::Exterior && cell.visibilityHint == world::VisibilityHint::Open)
        {
            outdoors = cell.id;
        }
        const RoomLightState* state = house.lighting.FindCell(cell.id);
        ASSERT_NE(state, nullptr);
        if (cell.kind != world::CellKind::Exterior && state->DaylightIsKey())
        {
            daylitRoom = cell.id;
        }
        if (cell.kind != world::CellKind::Exterior && state->daylight == 0.0F)
        {
            darkRoom = cell.id;
        }
    }
    ASSERT_TRUE(outdoors.IsValid());
    ASSERT_TRUE(daylitRoom.IsValid());
    ASSERT_TRUE(darkRoom.IsValid());

    const CelestialKeyLight* outdoorKey = house.lighting.SunKeyForCell(outdoors);
    const CelestialKeyLight* indoorKey = house.lighting.SunKeyForCell(daylitRoom);
    ASSERT_NE(outdoorKey, nullptr);
    ASSERT_NE(indoorKey, nullptr);
    EXPECT_EQ(house.lighting.CelestialKeyForCell(outdoors), outdoorKey)
        << "the combined selector did not preserve the daytime sun";
    EXPECT_EQ(house.lighting.SunKeyForCell(darkRoom), nullptr);
    EXPECT_EQ(house.lighting.SunKeyForCell(Id::Of("NO_SUCH_CELL")), nullptr);

    const auto direction = cnahouse::environment::SunDirection(house.lighting.Sun());
    const auto shading =
        cnahouse::environment::SunShadingFor(house.lighting.Sun(), house.lighting.CloudCover());
    EXPECT_NEAR(outdoorKey->direction.X, direction.X, 1e-6F);
    EXPECT_NEAR(outdoorKey->direction.Y, direction.Y, 1e-6F);
    EXPECT_NEAR(outdoorKey->direction.Z, direction.Z, 1e-6F);
    EXPECT_NEAR(outdoorKey->diffuseColor.X, shading.color.X * shading.directIntensity, 1e-6F);
    EXPECT_NEAR(outdoorKey->diffuseColor.Y, shading.color.Y * shading.directIntensity, 1e-6F);
    EXPECT_NEAR(outdoorKey->diffuseColor.Z, shading.color.Z * shading.directIntensity, 1e-6F);

    cnahouse::environment::CivilTime midnight = noon;
    midnight.hour = 0;
    house.clock.SetStandard(midnight);
    house.lighting.Update(Frame(8));
    EXPECT_LT(house.lighting.Sun().altitudeDeg, cnahouse::environment::kRefractedHorizonDeg);
    EXPECT_EQ(house.lighting.SunKeyForCell(outdoors), nullptr) << "the sun is below the horizon";
}

TEST(LightingSystemTests, AFullClearMoonBecomesTheOutdoorKeyOnlyOnADarkNight)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    house.clock.calendarDaysPerSimDay = 1.0;
    Id outdoors;
    Id indoors;
    for (const world::Cell& cell : house.world.Cells())
    {
        if (cell.kind == world::CellKind::Exterior && cell.visibilityHint == world::VisibilityHint::Open)
        {
            outdoors = cell.id;
        }
        else if (cell.kind != world::CellKind::Exterior)
        {
            indoors = cell.id;
        }
    }
    ASSERT_TRUE(outdoors.IsValid());
    ASSERT_TRUE(indoors.IsValid());
    ASSERT_TRUE(house.lighting.SetCloudCover(0.0F));

    bool foundMoonlitNight = false;
    cnahouse::environment::CivilTime time;
    time.year = 2031;
    time.month = 1;
    for (int day = 1; day <= 60 && !foundMoonlitNight; ++day)
    {
        const std::int64_t dayIndex = cnahouse::environment::DaysFromCivil(2031, 1, 1) + day - 1;
        const cnahouse::environment::CivilTime date = cnahouse::environment::CivilFromDays(dayIndex);
        time.year = date.year;
        time.month = date.month;
        time.day = date.day;
        for (int hour = 0; hour < 24; ++hour)
        {
            time.hour = hour;
            house.clock.SetStandard(time);
            house.lighting.Update(Frame(static_cast<std::uint64_t>(100 + day * 24 + hour)));
            if (house.lighting.Sun().altitudeDeg < cnahouse::environment::kMoonlightSunCutoffDeg &&
                house.lighting.Moon().altitudeDeg > 30.0 &&
                house.lighting.LunarPhase().illuminatedFraction > 0.95)
            {
                foundMoonlitNight = true;
                break;
            }
        }
    }
    ASSERT_TRUE(foundMoonlitNight) << "two lunations contained no high, full moon at night";

    const CelestialKeyLight* moonKey = house.lighting.MoonKeyForCell(outdoors);
    ASSERT_NE(moonKey, nullptr);
    EXPECT_EQ(house.lighting.CelestialKeyForCell(outdoors), moonKey);
    EXPECT_EQ(house.lighting.MoonKeyForCell(indoors), nullptr)
        << "§33.4 assigns moonlight only to outdoor objects";
    EXPECT_GT(moonKey->diffuseColor.X, 0.0F);
    EXPECT_GT(moonKey->diffuseColor.Y, moonKey->diffuseColor.X);
    EXPECT_GT(moonKey->diffuseColor.Z, moonKey->diffuseColor.Y);

    const auto expectedDirection = cnahouse::environment::MoonDirection(house.lighting.Moon());
    EXPECT_NEAR(moonKey->direction.X, expectedDirection.X, 1e-6F);
    EXPECT_NEAR(moonKey->direction.Y, expectedDirection.Y, 1e-6F);
    EXPECT_NEAR(moonKey->direction.Z, expectedDirection.Z, 1e-6F);
    std::printf("  moon key: %04d-%02d-%02d %02d:00, phase %.3f, altitude %.1f deg, blue %.6f\n",
                time.year,
                time.month,
                time.day,
                time.hour,
                house.lighting.LunarPhase().illuminatedFraction,
                house.lighting.Moon().altitudeDeg,
                static_cast<double>(moonKey->diffuseColor.Z));
}

TEST(LightingSystemTests, CloudCoverIsContinuousClampedAndCannotBecomeNotANumber)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    EXPECT_FLOAT_EQ(house.lighting.CloudCover(), house.world.GetInitialState().weather.state.cloudCover)
        << "initialstate.json did not reach the lighting system";
    EXPECT_TRUE(house.lighting.SetCloudCover(2.0F));
    EXPECT_FLOAT_EQ(house.lighting.CloudCover(), 1.0F);
    EXPECT_TRUE(house.lighting.SetCloudCover(-1.0F));
    EXPECT_FLOAT_EQ(house.lighting.CloudCover(), 0.0F);
    EXPECT_TRUE(house.lighting.SetCloudCover(0.4F));
    EXPECT_FALSE(house.lighting.SetCloudCover(std::nanf("")));
    EXPECT_FLOAT_EQ(house.lighting.CloudCover(), 0.4F) << "the refused NaN was applied";
}

TEST(LightingSystemTests, AuthoredSkyColoursDriveOutdoorAmbientAndTheLmDayTint)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    house.clock.calendarDaysPerSimDay = 1.0;
    cnahouse::environment::CivilTime noon;
    noon.year = 2031;
    noon.month = 6;
    noon.day = 21;
    noon.hour = 12;
    house.clock.SetStandard(noon);
    ASSERT_TRUE(house.lighting.SetCloudCover(1.0F));
    house.lighting.Update(Frame(900));

    const RoomLightState* outdoors = house.lighting.FindCell(Id::Of("EXT_WORLD"));
    ASSERT_NE(outdoors, nullptr);
    // Full overcast collapses every altitude and azimuth to the one authored grey. This is an
    // independent content checkpoint, not an expected value computed by the production sampler.
    EXPECT_NEAR(outdoors->skyAmbientColor.X, 0.370F, 1.0e-6F);
    EXPECT_NEAR(outdoors->skyAmbientColor.Y, 0.400F, 1.0e-6F);
    EXPECT_NEAR(outdoors->skyAmbientColor.Z, 0.440F, 1.0e-6F);
    EXPECT_EQ(house.lighting.SkyAmbientColor(), outdoors->skyAmbientColor)
        << "the public outdoor value diverged from the sky-open cell states";
    EXPECT_EQ(outdoors->daylightTint, Microsoft::Xna::Framework::Vector3())
        << "outdoor geometry has no baked interior LM_DAY pass";

    const RoomLightState* daylit = nullptr;
    const RoomLightState* windowless = nullptr;
    for (const RoomLightState& cell : house.lighting.Cells())
    {
        if (cell.daylight > 0.20F && daylit == nullptr)
        {
            daylit = &cell;
        }
        if (cell.daylight == 0.0F && cell.cell != Id::Of("EXT_WORLD") && windowless == nullptr)
        {
            const world::Cell* row = house.world.FindCell(cell.cell);
            if (row != nullptr && row->kind != world::CellKind::Exterior)
            {
                windowless = &cell;
            }
        }
    }
    ASSERT_NE(daylit, nullptr);
    ASSERT_NE(windowless, nullptr);
    EXPECT_NEAR(daylit->daylightTint.X, 0.370F * daylit->daylight, 1.0e-6F);
    EXPECT_NEAR(daylit->daylightTint.Y, 0.400F * daylit->daylight, 1.0e-6F);
    EXPECT_NEAR(daylit->daylightTint.Z, 0.440F * daylit->daylight, 1.0e-6F);
    EXPECT_EQ(daylit->skyAmbientColor, daylit->daylightTint)
        << "interior ambient and LM_DAY sampled different skies";
    EXPECT_EQ(windowless->skyAmbientColor, Microsoft::Xna::Framework::Vector3());
    EXPECT_GT(windowless->daylightTint.X, 0.0F)
        << "the windowless receiver should retain sky borrowed through its open portal";
    EXPECT_NEAR(windowless->daylightTint.Y, windowless->daylightTint.X * (0.400F / 0.370F), 1.0e-6F)
        << "borrowed daylight changed the authored sky chroma";
}

TEST(LightingSystemTests, ClearSkyAmbientChangesContinuouslyFromWarmHorizonToBlueDay)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    HouseLighting house;
    house.clock.calendarDaysPerSimDay = 1.0;
    ASSERT_TRUE(house.lighting.SetCloudCover(0.0F));

    cnahouse::environment::CivilTime time;
    time.year = 2031;
    time.month = 6;
    time.day = 21;
    time.hour = 12;
    house.clock.SetStandard(time);
    house.lighting.Update(Frame(910));
    const RoomLightState* outdoors = house.lighting.FindCell(Id::Of("EXT_WORLD"));
    ASSERT_NE(outdoors, nullptr);
    const auto day = outdoors->skyAmbientColor;
    EXPECT_LT(day.X, day.Y);
    EXPECT_LT(day.Y, day.Z) << "the clear daytime ambient did not take the blue sky gradient";

    // Find the minute closest to the authored -0.58-degree sunset row rather than embedding a
    // locale-dependent wall-clock guess. The clock and sky still cross their real shared seam.
    double closest = 1.0e9;
    Microsoft::Xna::Framework::Vector3 sunset;
    for (int minute = 15 * 60; minute < 22 * 60; ++minute)
    {
        time.hour = minute / 60;
        time.minute = minute % 60;
        house.clock.SetStandard(time);
        house.lighting.Update(Frame(static_cast<std::uint64_t>(1000 + minute)));
        const double distance = std::abs(house.lighting.Sun().altitudeDeg + 0.58);
        if (distance < closest)
        {
            closest = distance;
            sunset = house.lighting.FindCell(Id::Of("EXT_WORLD"))->skyAmbientColor;
        }
    }
    EXPECT_LT(closest, 0.20) << "the summer-day scan missed the authored sunset anchor";
    EXPECT_GT(sunset.X, sunset.Y);
    EXPECT_GT(sunset.Y, sunset.Z) << "the sky contribution did not carry sunset warmth";

    // A one-minute scan also bounds continuity at the system boundary: no tint channel may jump
    // while the sun moves through a continuously interpolated LUT.
    float worstStep = 0.0F;
    Microsoft::Xna::Framework::Vector3 previous;
    bool havePrevious = false;
    for (int minute = 17 * 60; minute <= 21 * 60; ++minute)
    {
        time.hour = minute / 60;
        time.minute = minute % 60;
        house.clock.SetStandard(time);
        house.lighting.Update(Frame(static_cast<std::uint64_t>(3000 + minute)));
        const auto current = house.lighting.FindCell(Id::Of("EXT_WORLD"))->skyAmbientColor;
        if (havePrevious)
        {
            worstStep = std::max({worstStep,
                                  std::abs(current.X - previous.X),
                                  std::abs(current.Y - previous.Y),
                                  std::abs(current.Z - previous.Z)});
        }
        previous = current;
        havePrevious = true;
    }
    EXPECT_LT(worstStep, 0.025F) << "the sky ambient stepped between adjacent civil minutes";
}

TEST(LightingSystemTests, CameraExposureFollowsTheObservedCellsPublishedTarget)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.cells.json";
    }
    HouseLighting house;
    LightingSystem& lighting = house.lighting;
    const Id outside = Id::Of("EXT_WORLD");
    const Id living = Id::Of("L0_LIVING");

    lighting.SetCameraCell(outside);
    lighting.Update(Frame(1));
    const RoomLightState* exteriorState = lighting.FindCell(outside);
    ASSERT_NE(exteriorState, nullptr);
    EXPECT_FLOAT_EQ(lighting.CameraExposureScale(), exteriorState->exposureTarget);
    EXPECT_LT(lighting.CameraExposureScale(), 1.0F);
    EXPECT_GT(lighting.CameraExposureTintAlpha(), 0.0F);

    lighting.SetCameraCell(living);
    FrameContext transition = Frame(2);
    transition.deltaSeconds = 0.9F;
    lighting.Update(transition);
    const RoomLightState* livingState = lighting.FindCell(living);
    ASSERT_NE(livingState, nullptr);
    EXPECT_EQ(lighting.CameraCell(), living);
    EXPECT_GT(livingState->exposureTarget, exteriorState->exposureTarget);
    EXPECT_GT(lighting.CameraExposureScale(), exteriorState->exposureTarget);
    EXPECT_LT(lighting.CameraExposureScale(), livingState->exposureTarget)
        << "entering a dark room adapts over §25.7's 2.2 seconds rather than snapping";
}
