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

        // Loaded file by file rather than through `Load`, which requires all sixteen. Every file
        // that exists is read in dependency order, including props now that visible light fixtures
        // make the light-to-prop edge part of rule 6.
        world::WorldData::Contents contents;
        using Loader = cnahouse::util::Result<void> (*)(std::string_view, world::WorldData::Contents&);
        for (const auto& [file, load] : std::initializer_list<std::pair<const char*, Loader>>{
                 {"layout.levels.json", &world::WorldLoader::LoadLevels},
                 {"layout.materials.json", &world::WorldLoader::LoadMaterials},
                 {"layout.cells.json", &world::WorldLoader::LoadCells},
                 {"layout.props.json", &world::WorldLoader::LoadProps},
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

    /// A rule-10 problem whose message contains @p needle. The §70.5 rows all live in rule 10, so
    /// "rule 10 fired" is not enough to say WHICH of them did.
    [[nodiscard]] bool Said(const std::vector<world::ValidationProblem>& problems, const char* needle)
    {
        return std::any_of(problems.begin(),
                           problems.end(),
                           [needle](const world::ValidationProblem& problem)
                           { return problem.ToString().find(needle) != std::string::npos; });
    }

    /// Adds a window to @p contents, sill @p sillY above the world origin, of @p type.
    void AddWindow(world::WorldData::Contents& contents, const char* type, float sillY)
    {
        world::Portal glazing;
        glazing.id = Intern("P_FOYER__EXT__W1");
        glazing.cellA = Intern("L0_FOYER");
        glazing.cellB = Intern("L0_HALL");
        glazing.axis = world::PlaneAxis::Z;
        glazing.planeValue = 4.0F;
        glazing.minU = -1.6F;
        glazing.maxU = -0.7F;
        glazing.minV = sillY;
        glazing.maxV = sillY + 1.50F;
        glazing.kind = world::PortalKind::Window;
        glazing.aperture = Intern("WIN_FOYER_1");
        glazing.soundLossOpen = 0.109F;
        glazing.soundLossClosed = 0.921F;
        contents.portals.push_back(glazing);

        world::Opening sash;
        sash.id = Intern("WIN_FOYER_1");
        sash.kind = world::OpeningKind::Window;
        sash.type = Intern(type);
        sash.portal = Intern("P_FOYER__EXT__W1");
        sash.leaf.width = 0.90F;
        sash.leaf.height = 1.50F;
        sash.leaf.thickness = 0.03F;
        sash.swing = Intern("L0_FOYER");
        contents.openings.push_back(sash);

        world::AudioTransmission single;
        single.kind = "window_single";
        single.open = 0.109F;
        single.closed = 0.921F;
        contents.audioTransmission.push_back(single);
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

    TEST(WorldValidatorTest, ObsoletePetWaypointsDoNotConstrainFurniture)
    {
        IdRegistry::ResetForTesting();
        auto contents = Fixture();

        world::Prop chair;
        chair.id = Intern("PROP_HALL_CHAIR");
        chair.asset = Intern("MODEL_CHAIR");
        chair.cell = Intern("L0_HALL");
        chair.position = {0.0F, 0.60F, 7.0F};
        contents.props.push_back(chair);

        world::NavNode oldHallNode;
        oldHallNode.id = Intern("NAV_HALL_OLD");
        oldHallNode.cell = chair.cell;
        oldHallNode.position = chair.position;
        contents.navNodes.push_back(oldHallNode);

        world::NavNode isolatedFoyerNode;
        isolatedFoyerNode.id = Intern("NAV_FOYER_OLD");
        isolatedFoyerNode.cell = Intern("L0_FOYER");
        isolatedFoyerNode.position = {0.0F, 0.60F, 2.0F};
        contents.navNodes.push_back(isolatedFoyerNode);

        // No edge joins the two historical nodes. The player portal graph is still complete, and
        // HOUSE-03301 makes that the only connectivity question rule 5 owns.
        EXPECT_FALSE(Fired(ProblemsFor(std::move(contents)), 5));
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

        // Rule 6: one combined group cannot have two different control owners.
        {
            auto contents = Fixture();
            world::Light second = contents.lights.front();
            second.id = Intern("LIGHT_HALL_DUSK");
            second.duskSensor = true;
            contents.lights.push_back(second);
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 6)) << "a mixed-control light group";
        }

        // Rule 6: one baked atlas cannot follow two physical switch-on envelopes.
        {
            auto contents = Fixture();
            world::Light second = contents.lights.front();
            second.id = Intern("LIGHT_HALL_LED");
            second.bulbClass = world::BulbClass::Led;
            contents.lights.push_back(second);
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 6)) << "a mixed-class light group";
        }

        // Rule 6: an automatic fixture's initial state comes from the sun, never `defaultOn`.
        {
            auto contents = Fixture();
            contents.lights.front().duskSensor = true;
            contents.lights.front().defaultOn = true;
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 6)) << "a dusk light defaulted on";
        }

        // Rule 6: an automatic group has no wall switch whose state the next solar update would
        // immediately overwrite.
        {
            auto contents = Fixture();
            contents.lights.front().duskSensor = true;
            world::Interactable plate;
            plate.id = Intern("SWITCH_HALL");
            plate.kind = "light_switch";
            plate.cell = Intern("L0_HALL");
            plate.state.Declare("LG_HALL", world::StateValue{false});
            contents.interactables.push_back(plate);
            EXPECT_TRUE(Said(ProblemsFor(std::move(contents)), "also has wall switch"))
                << "a dusk-controlled group with a manual plate";
        }

        // Rule 6: a static-detail spill names another real receiver, never its source cell.
        {
            auto contents = Fixture();
            contents.lights.front().spillCells.push_back(Intern("L0_MISSING"));
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 6)) << "an unknown spill receiver";
        }
        {
            auto contents = Fixture();
            contents.lights.front().spillCells.push_back(contents.lights.front().cell);
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 6)) << "a redundant spill receiver";
        }

        // Rule 6: a visible emitter is a real prop in the same cell, not an unchecked string.
        {
            auto contents = Fixture();
            contents.lights.front().fixtureProp = Intern("PROP_MISSING_FIXTURE");
            contents.lights.front().emissiveMaterialSlot = "Shade";
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 6)) << "a missing fixture prop";
        }
        {
            auto contents = Fixture();
            world::Prop fixture;
            fixture.id = Intern("PROP_DYNAMIC_FIXTURE");
            fixture.asset = Intern("MODEL_DYNAMIC_FIXTURE");
            fixture.cell = contents.lights.front().cell;
            fixture.isStatic = false;
            contents.props.push_back(fixture);
            contents.lights.front().fixtureProp = fixture.id;
            contents.lights.front().emissiveMaterialSlot = "Shade";
            EXPECT_TRUE(Fired(ProblemsFor(std::move(contents)), 6)) << "a dynamic fixture prop";
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

        // ---- §70.5's remaining layout rows (`HOUSE-00360`) ----------------------------------

        // A sill 1.50 m over the floor. The foyer's floor is L0's 0.60 ffl, so 2.10 is 1.50 up.
        {
            auto contents = Fixture();
            AddWindow(contents, "W_DH_STD", 2.10F);
            const auto problems = ProblemsFor(std::move(contents));
            EXPECT_TRUE(Said(problems, "sill is 1.50")) << "a window at head height";
        }

        // ...and 0.90 up is §12.6's own number for a `W_DH_STD`, so it passes. A band that only
        // ever rejects is not a band.
        {
            auto contents = Fixture();
            AddWindow(contents, "W_DH_STD", 1.50F);
            EXPECT_FALSE(Said(ProblemsFor(std::move(contents)), "sill is")) << "a 0.90 m sill";
        }

        // The exemption is the declared TYPE, never the measurement: obscured privacy glazing sits
        // above eye level because that is what it is for.
        {
            auto contents = Fixture();
            AddWindow(contents, "W_BATH", 2.10F);
            EXPECT_FALSE(Said(ProblemsFor(std::move(contents)), "sill is")) << "a bathroom window";
        }

        // A light switch 1.60 m up, and a door handle 1.25 m up. Two DIFFERENT bands: a check that
        // used one for both would pass a switch at handle height in either direction.
        {
            auto contents = Fixture();
            world::Interactable plate;
            plate.id = Intern("SWITCH_HALL");
            plate.kind = "light_switch";
            plate.cell = Intern("L0_HALL");
            plate.focusPoint = {1.90F, 2.20F, 5.00F};
            plate.state.Declare("LG_HALL", world::StateValue{false});
            contents.interactables.push_back(plate);

            world::Interactable handle;
            handle.id = Intern("DOOR_FOYER_HANDLE");
            handle.kind = "door";
            handle.cell = Intern("L0_HALL");
            handle.focusPoint = {1.94F, 1.85F, 4.20F};
            contents.interactables.push_back(handle);

            const auto problems = ProblemsFor(std::move(contents));
            EXPECT_TRUE(Said(problems, "switch centre is 1.60")) << "a switch out of reach";
            EXPECT_TRUE(Said(problems, "handle centre is 1.25")) << "a handle at switch height";
        }

        // A room whose NAME says it is a WC, at 1.60 m². The layout has no `function` field, so
        // the name is the only place the data says what a room is for.
        {
            auto contents = Fixture();
            contents.cells[1].name = "WC 1";
            contents.cells[1].boxes[0] = world::Footprint{-2.0F, -1.2F, 4.0F, 6.0F};
            EXPECT_TRUE(Said(ProblemsFor(std::move(contents)), "under §70.5's 1.80 m²"))
                << "a WC you cannot turn round in";
        }

        // ...and the same room with a name that claims nothing has no minimum at all.
        {
            auto contents = Fixture();
            contents.cells[1].name = "Meter Cupboard";
            contents.cells[1].boxes[0] = world::Footprint{-2.0F, -1.2F, 4.0F, 6.0F};
            EXPECT_FALSE(Said(ProblemsFor(std::move(contents)), "for what its name says it is"))
                << "an unnamed function has no minimum";
        }

        // A corridor 0.80 m across, measured on its NARROW axis. It is 2 m long, and a check that
        // took the larger dimension would call it 2 m wide and pass it.
        {
            auto contents = Fixture();
            contents.cells[1].kind = world::CellKind::Corridor;
            contents.cells[1].boxes[0] = world::Footprint{-2.0F, -1.2F, 4.0F, 6.0F};
            EXPECT_TRUE(Said(ProblemsFor(std::move(contents)), "0.80 m across"))
                << "a corridor you walk down sideways";
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
