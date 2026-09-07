// SPDX-License-Identifier: MIT
#include "cnahouse/world/WorldValidator.hpp"

#include "cnahouse/world/WorldLoader.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>

namespace
{
    using cnahouse::util::IdRegistry;
    using cnahouse::util::Intern;
    namespace world = cnahouse::world;

    /// The authored house, validated by the C++ implementation of §15.7.
    ///
    /// `tools/world/validate_world.py` says the same thing on every commit. Having both is the
    /// point: this session alone, the pair found the loader refusing a portal the Python accepted
    /// (`HOUSE-00378`) and silently dropping seven of §64.3's ten transmission classes
    /// (`HOUSE-00388`). A rule stated once is a rule nobody checks.
    TEST(WorldValidatorTest, TheAuthoredWorldPassesEveryRuleItCanSee)
    {
        IdRegistry::ResetForTesting();
        const std::string directory = "content/world";
        if (!std::filesystem::exists(directory + "/layout.cells.json"))
        {
            GTEST_SKIP() << "no deployed world; run tools/world/deploy_world.py";
        }

        // Loaded file by file rather than through `Load`, which requires all sixteen: materials
        // and props are `HOUSE-00385` and later, and waiting for them would mean the C++ rules
        // check nothing until then. Every file that exists is read.
        world::WorldData::Contents contents;
        using Loader = cnahouse::util::Result<void> (*)(std::string_view, world::WorldData::Contents&);
        for (const auto& [file, load] : std::initializer_list<std::pair<const char*, Loader>>{
                 {"layout.levels.json", &world::WorldLoader::LoadLevels},
                 {"layout.cells.json", &world::WorldLoader::LoadCells},
                 {"layout.portals.json", &world::WorldLoader::LoadPortals},
                 {"layout.openings.json", &world::WorldLoader::LoadOpenings},
                 {"layout.stairs.json", &world::WorldLoader::LoadStairs},
                 {"layout.lights.json", &world::WorldLoader::LoadLights},
                 {"layout.nav.json", &world::WorldLoader::LoadNav},
                 {"layout.audio.json", &world::WorldLoader::LoadAudio},
                 {"layout.exterior.json", &world::WorldLoader::LoadExterior},
                 {"interactables.json", &world::WorldLoader::LoadInteractables},
                 {"initialstate.json", &world::WorldLoader::LoadInitialState}})
        {
            if (!std::filesystem::exists(directory + "/" + file))
            {
                continue;
            }
            const auto read = load(directory, contents);
            ASSERT_TRUE(read) << file << ": " << read.Error().ToString();
        }

        auto built = world::WorldData::Create(std::move(contents));
        ASSERT_TRUE(built) << built.Error().ToString();

        const auto problems = world::WorldValidator::Validate(built.Value(), world::ValidationDepth::Full);
        std::string report;
        for (const world::ValidationProblem& problem : problems)
        {
            report += "\n  rule " + std::to_string(problem.rule) + ": " + problem.ToString();
        }
        EXPECT_TRUE(problems.empty()) << problems.size() << " problem(s):" << report;

        IdRegistry::ResetForTesting();
    }

    // ------------------------------------------------------------------ the rules bite ---
    //
    // A validator that has only ever passed is not a validator. Each of these builds the same
    // small house, breaks one thing, and asserts that the rule which owns that thing is the one
    // that fires -- the C++ half of `validate_world.py`'s injected-bug discipline.

    world::WorldData::Contents Fixture()
    {
        world::WorldData::Contents contents;

        world::Level ground;
        ground.id = Intern("L0");
        ground.ffl = 0.60F;
        ground.ceiling = 3.30F;
        contents.levels.push_back(ground);

        const auto room = [](const char* id, float minX, float maxX, float minZ, float maxZ)
        {
            world::Cell cell;
            cell.id = Intern(id);
            cell.level = Intern("L0");
            cell.kind = world::CellKind::Room;
            cell.boxes.push_back(world::Footprint{minX, maxX, minZ, maxZ});
            return cell;
        };
        contents.cells.push_back(room("L0_FOYER", -2.0F, 2.0F, 0.0F, 4.0F));
        contents.cells.push_back(room("L0_HALL", -2.0F, 2.0F, 4.0F, 10.0F));

        world::Portal door;
        door.id = Intern("P_FOYER__HALL");
        door.cellA = Intern("L0_FOYER");
        door.cellB = Intern("L0_HALL");
        door.axis = world::PlaneAxis::Z;
        door.planeValue = 4.0F;
        door.minU = -0.5F;
        door.maxU = 0.5F;
        door.minV = 0.60F;
        door.maxV = 2.70F;
        door.kind = world::PortalKind::Door;
        door.aperture = Intern("DOOR_FOYER");
        door.soundLossOpen = 0.109F;
        door.soundLossClosed = 0.842F;
        contents.portals.push_back(door);

        world::Opening leaf;
        leaf.id = Intern("DOOR_FOYER");
        leaf.kind = world::OpeningKind::Door;
        leaf.type = Intern("D_INT_PASSAGE");
        leaf.portal = Intern("P_FOYER__HALL");
        leaf.leaf.width = 0.86F;
        leaf.leaf.height = 2.05F;
        leaf.leaf.thickness = 0.04F;
        leaf.swing = Intern("L0_HALL");
        contents.openings.push_back(leaf);

        world::AudioTransmission hollow;
        hollow.kind = "door_hollow";
        hollow.open = 0.109F;
        hollow.closed = 0.842F;
        contents.audioTransmission.push_back(hollow);

        world::Light lamp;
        lamp.id = Intern("LIGHT_HALL");
        lamp.cell = Intern("L0_HALL");
        lamp.group = Intern("LG_HALL");
        lamp.position = {0.0F, 3.10F, 7.0F};
        lamp.range = 5.0F;
        contents.lights.push_back(lamp);
        contents.cells[1].lightGroups.push_back(Intern("LG_HALL"));

        return contents;
    }

    std::vector<world::ValidationProblem> ProblemsFor(world::WorldData::Contents&& contents)
    {
        auto built = world::WorldData::Create(std::move(contents));
        EXPECT_TRUE(built) << (built ? "" : built.Error().ToString());
        if (!built)
        {
            return {};
        }
        return world::WorldValidator::Validate(built.Value(), world::ValidationDepth::Full);
    }

    [[nodiscard]] bool Fired(const std::vector<world::ValidationProblem>& problems, int rule)
    {
        return std::any_of(problems.begin(),
                           problems.end(),
                           [rule](const world::ValidationProblem& problem) { return problem.rule == rule; });
    }

    TEST(WorldValidatorTest, TheFixtureItselfIsClean)
    {
        IdRegistry::ResetForTesting();
        const auto problems = ProblemsFor(Fixture());
        std::string report;
        for (const auto& problem : problems)
        {
            report += "\n  rule " + std::to_string(problem.rule) + ": " + problem.ToString();
        }
        EXPECT_TRUE(problems.empty()) << report;
        IdRegistry::ResetForTesting();
    }

    TEST(WorldValidatorTest, EachRuleCatchesItsOwnBreakage)
    {
        IdRegistry::ResetForTesting();

        // Rule 3: a second cell over the hall's footprint, declaring no parent.
        {
            auto contents = Fixture();
            world::Cell ghost = contents.cells[1];
            ghost.id = Intern("L0_GHOST");
            contents.cells.push_back(ghost);
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 3)) << "overlapping cells";
        }

        // Rule 5: the hall unreachable, because the only door is gone.
        {
            auto contents = Fixture();
            contents.portals.clear();
            contents.openings.clear();
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 5)) << "a room with no way in";
        }

        // Rule 6: the cell's light-group index left behind by its lights.
        {
            auto contents = Fixture();
            contents.cells[1].lightGroups.clear();
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 6)) << "a stale lightGroups index";
        }

        // Rule 6 again: a soundLoss that disagrees with §64.3's class for its leaf.
        {
            auto contents = Fixture();
            contents.portals[0].soundLossClosed = 0.937F;
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 6)) << "a hollow door at 24 dB";
        }

        // Rule 7: a second door with nothing to shut it. Not by emptying the openings -- an empty
        // openings file is "the leaves are not authored yet" and the rule stands down for it, the
        // same arrangement `validate_world.py` makes.
        {
            auto contents = Fixture();
            world::Portal second = contents.portals[0];
            second.id = Intern("P_HALL__WC");
            second.aperture = Intern("DOOR_WC");
            contents.portals.push_back(second);
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 7)) << "a portal with no leaf";
        }

        // Rule 7 again: the portal stops naming its leaf, which this format reads as "always open".
        {
            auto contents = Fixture();
            contents.portals[0].aperture = cnahouse::util::Id{};
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 7)) << "a door claiming to be a hole";
        }

        // Rule 9: a stack whose cell is not over its chase.
        {
            auto contents = Fixture();
            world::PlumbingStack stack;
            stack.id = Intern("STACK_A");
            stack.cells.push_back(Intern("L0_HALL"));
            stack.chase = world::Footprint{20.0F, 21.0F, 20.0F, 21.0F};
            contents.plumbing.push_back(stack);
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 9)) << "a stack over nothing";
        }

        // Rule 10: a 2.40 m interior door leaf.
        {
            auto contents = Fixture();
            contents.openings[0].leaf.height = 2.40F;
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 10)) << "a door nobody could hang";
        }

        // Rule 10 again: a light 30 m from the room it claims to be in.
        {
            auto contents = Fixture();
            contents.lights[0].position = {40.0F, 3.10F, 7.0F};
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 10)) << "a light in another county";
        }

        // Rule 11: an interactable nobody can reach, in a room nobody can stand up in.
        {
            auto contents = Fixture();
            world::Cell crawl;
            crawl.id = Intern("L0_CRAWL");
            crawl.level = Intern("L0");
            crawl.kind = world::CellKind::Closet;
            // Far from the hall on purpose: a crawl space next door would be reachable FROM the
            // hall, which is right and is what rule 11's neighbour search is for.
            crawl.boxes.push_back(world::Footprint{20.0F, 21.0F, 20.0F, 21.0F});
            crawl.yOverride = world::Extent{0.60F, 1.90F};
            contents.cells.push_back(crawl);

            world::Portal hole;
            hole.id = Intern("P_HALL__CRAWL");
            hole.cellA = Intern("L0_HALL");
            hole.cellB = Intern("L0_CRAWL");
            hole.axis = world::PlaneAxis::X;
            hole.planeValue = 2.0F;
            hole.minU = 4.2F;
            hole.maxU = 4.8F;
            hole.minV = 0.60F;
            hole.maxV = 1.80F;
            hole.kind = world::PortalKind::CasedOpening;
            contents.portals.push_back(hole);

            world::Interactable deep;
            deep.id = Intern("SWITCH_CRAWL");
            deep.kind = "light_switch";
            deep.cell = Intern("L0_CRAWL");
            deep.focusPoint = {20.95F, 1.80F, 20.95F};
            deep.state.Declare("LG_HALL", world::StateValue{false});
            contents.interactables.push_back(deep);
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 11))
                << "a switch in a room with nowhere to stand";
        }

        IdRegistry::ResetForTesting();
    }
} // namespace
