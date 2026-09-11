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
#include "cnahouse/lighting/LightingSystem.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::app::FrameContext;
    using cnahouse::lighting::kAmbientFloor;
    using cnahouse::lighting::kDaylightKeyThreshold;
    using cnahouse::lighting::LightingSystem;
    using cnahouse::lighting::RoomLightState;
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
        EXPECT_TRUE(world::WorldLoader::LoadLights("content/world", contents).HasValue());
        auto built = world::WorldData::Create(std::move(contents));
        EXPECT_TRUE(built) << built.Error().ToString();
        return std::move(built.Value());
    }

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
    const world::WorldData world = LoadWorld();
    const LightingSystem lighting(world);

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
        EXPECT_NE(lighting.FindGroup(Id(group)), nullptr);
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
    const world::WorldData world = LoadWorld();
    LightingSystem lighting(world);
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
    const world::WorldData world = LoadWorld();
    LightingSystem lighting(world);
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
    const world::WorldData world = LoadWorld();
    LightingSystem lighting(world);

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

TEST(LightingSystemTests, ADimmerScalesTheGroupAndIsRefusedWhenItIsNotANumber)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    const world::WorldData world = LoadWorld();
    LightingSystem lighting(world);
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
    const world::WorldData world = LoadWorld();
    LightingSystem lighting(world);
    const Id nonsense = Id::Of("LG_THERE_IS_NO_SUCH_GROUP");
    EXPECT_FALSE(lighting.SetGroupOn(nonsense, true));
    EXPECT_FALSE(lighting.SetGroupDimmer(nonsense, 0.5F));
    EXPECT_EQ(lighting.FindGroup(nonsense), nullptr);
    EXPECT_EQ(lighting.FindCell(Id::Of("NO_SUCH_CELL")), nullptr);
    EXPECT_TRUE(lighting.GroupsForCell(Id::Of("NO_SUCH_CELL")).empty());
    EXPECT_FLOAT_EQ(lighting.GroupLevelInCell(Id::Of("NO_SUCH_CELL"), nonsense), 0.0F);
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
    const world::WorldData world = LoadWorld();
    LightingSystem lighting(world);
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
    const world::WorldData world = LoadWorld();
    LightingSystem lighting(world);
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

TEST(LightingSystemTests, TheDaylightAndBorrowedFieldsAreZeroAndThatIsDeliberate)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/layout.lights.json";
    }
    // **This test exists to be deleted.** `HOUSE-01251` is the skeleton: `daylight` is
    // `HOUSE-01263`'s and `borrowed` is `HOUSE-01265`'s, and both are left at zero rather than
    // given a plausible number nothing computed. Asserting the zero is what stops "the daylight
    // model is not written yet" from being indistinguishable from "the daylight model is broken",
    // and it will fail the day either one lands -- which is the point.
    const world::WorldData world = LoadWorld();
    LightingSystem lighting(world);
    lighting.Update(Frame(6));
    for (const RoomLightState& cell : lighting.Cells())
    {
        EXPECT_FLOAT_EQ(cell.daylight, 0.0F) << "HOUSE-01263 has landed; delete this test";
        EXPECT_FLOAT_EQ(cell.borrowed, 0.0F) << "HOUSE-01265 has landed; delete this test";
    }
}
