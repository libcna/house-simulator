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
#include <cmath>
#include <cstdio>
#include <filesystem>
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
    using cnahouse::lighting::CelestialKeyLight;
    using cnahouse::lighting::kAmbientFloor;
    using cnahouse::lighting::kDaylightKeyThreshold;
    using cnahouse::lighting::LightingSystem;
    using cnahouse::lighting::PlanckianRgb;
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

TEST(LightingSystemTests, TheHouseStartsWithEveryLightOffBecauseThatIsWhatTheDataSays)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    // `layout.lights.json` says so in terms: *"a house with every light burning is not it"*. This
    // asserts the system READ that rather than defaulting to it, by checking against the data.
    HouseLighting house;
    const world::WorldData& world = house.world;
    LightingSystem& lighting = house.lighting;
    lighting.Update(Frame(1));

    for (const world::Light& light : world.Lights())
    {
        const SwitchGroupState* group = lighting.FindGroup(light.group);
        ASSERT_NE(group, nullptr);
        if (light.defaultOn)
        {
            EXPECT_TRUE(group->on) << "a fixture with defaultOn left its group off";
        }
    }
    for (const RoomLightState& cell : lighting.Cells())
    {
        EXPECT_FLOAT_EQ(cell.artificial, 0.0F);
        EXPECT_EQ(cell.artificialColor, Microsoft::Xna::Framework::Vector3());
        // ...and a dark room is still not black: §30's first row.
        EXPECT_FLOAT_EQ(cell.Level(), kAmbientFloor);
    }
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
    const float pantryClosed = pantryState->borrowed;

    ASSERT_TRUE(house.visibility.SetAperture(Id::Of("P_L0_KITCHEN__L0_PANTRY"), 1.0F));
    house.lighting.Update(Frame(22));
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
    EXPECT_EQ(windowless->daylightTint, Microsoft::Xna::Framework::Vector3());
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
