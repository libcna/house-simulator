// SPDX-License-Identifier: MIT
//
// `HOUSE-00563`. §71's `teleport <cellId>` and `noclip`, and the console registry they are the
// first two commands of -- §71 lists eighteen, and every later one plugs into this rather than
// growing a parser of its own.
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/debug/PlayerCommands.hpp"

namespace
{
    using cnahouse::debug::CommandResult;
    using cnahouse::debug::Console;
    using cnahouse::debug::PlayerCommandContext;
    using cnahouse::debug::RegisterPlayerCommands;
    using cnahouse::player::CellTracker;
    using cnahouse::player::InputState;
    using cnahouse::player::PlayerState;
    using cnahouse::util::Intern;

    namespace world = cnahouse::world;

    world::Cell MakeCell(std::string_view id, std::string_view level, world::Footprint box)
    {
        world::Cell cell;
        cell.id = Intern(id);
        cell.level = Intern(level);
        cell.kind = world::CellKind::Room;
        cell.boxes = {box};
        return cell;
    }

    world::WorldData::Contents Fixture()
    {
        world::WorldData::Contents contents;
        world::Level l0;
        l0.id = Intern("L0");
        l0.ffl = 0.60F;
        l0.ceiling = 3.30F;
        l0.structureDepth = 0.35F;
        world::Level l3;
        l3.id = Intern("L3");
        l3.ffl = 9.30F;
        l3.ceiling = 13.90F;
        l3.structureDepth = 0.35F;
        contents.levels = {l0, l3};
        contents.cells = {
            MakeCell("L0_KITCHEN", "L0", world::Footprint{-4.0F, 0.0F, -4.0F, 0.0F}),
            MakeCell("L0_HALL", "L0", world::Footprint{0.0F, 4.0F, -4.0F, 0.0F}),
            MakeCell("L3_ATTIC", "L3", world::Footprint{2.0F, 6.0F, 2.0F, 6.0F}),
        };

        // The kitchen and the hall share the x = 0 wall and a door in it, which is what makes
        // §16.4's neighbour walk reachable from the kitchen -- and therefore what makes it
        // visible whether a teleport cleared the tracker or left it to walk from a stale cell.
        world::Portal door;
        door.id = Intern("P_KITCHEN_HALL");
        door.cellA = Intern("L0_KITCHEN");
        door.cellB = Intern("L0_HALL");
        door.axis = world::PlaneAxis::X;
        door.planeValue = 0.0F;
        door.minU = -2.5F;
        door.maxU = -1.6F;
        door.minV = 0.60F;
        door.maxV = 2.65F;
        contents.portals = {door};
        return contents;
    }

} // namespace

TEST(ConsoleCommandTests, TheConsoleFindsCommandsAndSaysWhenItCannot)
{
    Console console;
    int calls = 0;
    console.Register("ping",
                     "ping",
                     [&](std::span<const std::string_view>) -> CommandResult
                     {
                         ++calls;
                         return {true, "pong"};
                     });

    EXPECT_TRUE(console.Execute("ping").ok);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(console.Execute("ping").message, "pong");

    const CommandResult unknown = console.Execute("wibble");
    EXPECT_FALSE(unknown.ok);
    EXPECT_NE(unknown.message.find("wibble"), std::string::npos)
        << "it did not say WHICH command it did not know";

    // Pressing return at a prompt is not an error.
    EXPECT_TRUE(console.Execute("").ok);
    EXPECT_TRUE(console.Execute("   ").ok);
    EXPECT_EQ(calls, 2) << "an empty line ran a handler";
}

TEST(ConsoleCommandTests, ArgumentsAreTheWordsAfterTheName)
{
    Console console;
    std::vector<std::string> got;
    console.Register("say",
                     "say <words...>",
                     [&](std::span<const std::string_view> args) -> CommandResult
                     {
                         got.clear();
                         for (const std::string_view word : args)
                         {
                             got.emplace_back(word);
                         }
                         return {true, {}};
                     });

    (void)console.Execute("  say   one    two three  ");
    ASSERT_EQ(got.size(), 3u) << "runs of whitespace were not one separator";
    EXPECT_EQ(got[0], "one");
    EXPECT_EQ(got[1], "two");
    EXPECT_EQ(got[2], "three");

    (void)console.Execute("say");
    EXPECT_TRUE(got.empty()) << "a command with no arguments got some";
}

TEST(ConsoleCommandTests, RegisteringANameAgainReplacesIt)
{
    // What a hot reload of a subsystem needs, and the alternative is two handlers for one name
    // with the winner decided by registration order.
    Console console;
    console.Register(
        "x", "x", [](std::span<const std::string_view>) -> CommandResult { return {true, "first"}; });
    console.Register(
        "x", "x <new>", [](std::span<const std::string_view>) -> CommandResult { return {true, "second"}; });
    EXPECT_EQ(console.Execute("x").message, "second");
    EXPECT_EQ(console.Names().size(), 1u);
    EXPECT_EQ(console.Usage("x"), "x <new>");
}

TEST(ConsoleCommandTests, TeleportPutsTheBodyOnTheNamedCellsFLOOR)
{
    // The middle of a cell is in the AIR -- a cell is a volume -- and a body dropped there falls,
    // which turns a debugging aid into a fall and a landing sound.
    auto loaded = world::WorldData::Create(Fixture());
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const world::WorldData& data = loaded.Value();
    const auto index = world::SpatialIndex::Build(data);

    PlayerState player;
    CellTracker tracker;
    Console console;
    RegisterPlayerCommands(console, PlayerCommandContext{&player, &tracker, &data, &index});

    // Teleported out of a run and a fall, because "arrives at rest" is only a claim about
    // something that had speed to lose.
    player.velocity = Microsoft::Xna::Framework::Vector3(1.9F, 0.0F, -1.2F);
    player.fall.onGround = false;
    player.fall.speed = 6.5F;
    player.fall.fellFrom = 12.0F;

    const CommandResult result = console.Execute("teleport L3_ATTIC");
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_NE(result.message.find("L3_ATTIC"), std::string::npos);
    EXPECT_NEAR(player.position.X, 4.0F, 1e-4F);
    EXPECT_NEAR(player.position.Z, 4.0F, 1e-4F);
    // L3's ffl is 9.30, and the body stands on it rather than hovering at the cell's mid-height.
    EXPECT_NEAR(player.position.Y - player.Rise(), 9.30F, 0.01F);
    // ...and it arrives at rest, not carrying the run that took it there, and not still falling
    // from a height on the other side of the house -- which would land hard on arrival.
    EXPECT_FLOAT_EQ(player.velocity.X, 0.0F);
    EXPECT_FLOAT_EQ(player.velocity.Z, 0.0F);
    EXPECT_TRUE(player.fall.onGround);
    EXPECT_FLOAT_EQ(player.fall.speed, 0.0F);
}

TEST(ConsoleCommandTests, TeleportRetellsTheCellTrackerWhereItIs)
{
    // §16.4's incremental test would answer a query about the attic with the kitchen the player
    // just left, expanded by 5 cm. A teleport has to send the next lookup to the grid.
    auto loaded = world::WorldData::Create(Fixture());
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const world::WorldData& data = loaded.Value();
    const auto index = world::SpatialIndex::Build(data);

    PlayerState player;
    CellTracker tracker;
    Console console;
    RegisterPlayerCommands(console, PlayerCommandContext{&player, &tracker, &data, &index});

    ASSERT_TRUE(console.Execute("teleport L0_KITCHEN").ok);
    EXPECT_EQ(tracker.Current(), Intern("L0_KITCHEN"));
    EXPECT_EQ(tracker.LastStep(), world::SpatialIndex::Step::Grid);

    // Next door, through a portal. The ANSWER would come out right either way here -- the
    // neighbour walk checks containment before it believes itself -- so what is asserted is the
    // STEP: the lookup began at the grid, with no previous cell. That is the only arrangement
    // that is also right when the destination lands inside the 5 cm hysteresis of the cell the
    // player was in, where the incremental test answers with the old cell and never looks.
    ASSERT_TRUE(console.Execute("teleport L0_HALL").ok);
    EXPECT_EQ(tracker.Current(), Intern("L0_HALL")) << "the tracker was left in the old cell";
    EXPECT_EQ(tracker.LastStep(), world::SpatialIndex::Step::Grid)
        << "the lookup walked out of the cell the player had already left";

    // ...and across the house, where a stale cell has nothing useful to say at all.
    ASSERT_TRUE(console.Execute("teleport L3_ATTIC").ok);
    EXPECT_EQ(tracker.Current(), Intern("L3_ATTIC"));
    EXPECT_EQ(tracker.LastStep(), world::SpatialIndex::Step::Grid);

    // Teleporting to where it already is is still a teleport: the cache is not evidence.
    ASSERT_TRUE(console.Execute("teleport L3_ATTIC").ok);
    EXPECT_EQ(tracker.LastStep(), world::SpatialIndex::Step::Grid);
}

TEST(ConsoleCommandTests, TeleportSaysWhatWasWrongAndWhatItWanted)
{
    auto loaded = world::WorldData::Create(Fixture());
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const world::WorldData& data = loaded.Value();
    const auto index = world::SpatialIndex::Build(data);

    PlayerState player;
    CellTracker tracker;
    Console console;
    RegisterPlayerCommands(console, PlayerCommandContext{&player, &tracker, &data, &index});
    const auto before = player.position;

    const CommandResult missing = console.Execute("teleport L9_NOWHERE");
    EXPECT_FALSE(missing.ok);
    EXPECT_NE(missing.message.find("L9_NOWHERE"), std::string::npos)
        << "a console user's next move is to check their spelling, and this did not help";

    const CommandResult noArgs = console.Execute("teleport");
    EXPECT_FALSE(noArgs.ok);
    EXPECT_NE(noArgs.message.find("usage"), std::string::npos);

    // A second word is a typo -- a cell id with a space in it, most likely -- and obeying the
    // first half of it would move the player somewhere they did not ask for.
    const CommandResult extra = console.Execute("teleport L3_ATTIC please");
    EXPECT_FALSE(extra.ok) << "a stray second argument was ignored rather than reported";
    EXPECT_NE(extra.message.find("usage"), std::string::npos);

    EXPECT_FLOAT_EQ(player.position.X, before.X) << "a failed teleport moved the player anyway";
}

TEST(ConsoleCommandTests, NoclipTogglesAndTakesAnExplicitState)
{
    PlayerState player;
    Console console;
    RegisterPlayerCommands(console, PlayerCommandContext{&player, nullptr, nullptr, nullptr});

    EXPECT_FALSE(player.noclip);
    EXPECT_TRUE(console.Execute("noclip").ok);
    EXPECT_TRUE(player.noclip);
    (void)console.Execute("noclip");
    EXPECT_FALSE(player.noclip) << "it did not toggle back";

    // An explicit state, so a script does not have to know what it was.
    (void)console.Execute("noclip on");
    EXPECT_TRUE(player.noclip);
    (void)console.Execute("noclip on");
    EXPECT_TRUE(player.noclip) << "'on' twice turned it off";
    (void)console.Execute("noclip off");
    EXPECT_FALSE(player.noclip);

    EXPECT_FALSE(console.Execute("noclip sideways").ok);
}

TEST(ConsoleCommandTests, LeavingNoclipDropsTheVelocityItFlewOnUpon)
{
    // Coming back to a world that cares about collision, carrying the speed of a body that was
    // ignoring it, is how a debugging aid ends with the player through a wall.
    PlayerState player;
    Console console;
    RegisterPlayerCommands(console, PlayerCommandContext{&player, nullptr, nullptr, nullptr});
    (void)console.Execute("noclip on");
    player.velocity = Microsoft::Xna::Framework::Vector3(9.0F, 0.0F, -9.0F);
    player.fall.onGround = false;
    player.fall.speed = 7.0F;

    (void)console.Execute("noclip off");
    EXPECT_FLOAT_EQ(player.velocity.X, 0.0F);
    EXPECT_FLOAT_EQ(player.velocity.Z, 0.0F);
    EXPECT_TRUE(player.fall.onGround);
    EXPECT_FLOAT_EQ(player.fall.speed, 0.0F);
}

TEST(ConsoleCommandTests, ACommandWithNoPlayerSaysSoRatherThanCrashing)
{
    // A console is available before a level is; `teleport` typed at a title screen must answer.
    Console console;
    RegisterPlayerCommands(console, PlayerCommandContext{});
    EXPECT_FALSE(console.Execute("teleport L0_KITCHEN").ok);
    EXPECT_FALSE(console.Execute("noclip").ok);
}
