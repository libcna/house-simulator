// SPDX-License-Identifier: MIT
//
// `HOUSE-00343`. `world.manifest.json` and `layout.levels.json`, read from a real directory.
//
// The fixtures are written to a temporary directory and read back through `System::IO`, not fed to
// the parser as strings. The manifest's whole job is a statement about a *directory* -- every
// member is here, and nothing here is unlisted -- and a test that never touched a directory could
// not fail on either half of it.
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::util::ErrorCode;
    using cnahouse::util::IdRegistry;
    using cnahouse::util::Intern;

    namespace world = cnahouse::world;

    /// A world directory under the build tree's test-output root.
    ///
    /// `std::filesystem` here and nowhere else: `docs/conventions.md` §5 confines it to
    /// `SaveStore`'s desktop implementation in the SHIPPING code, and this is a test creating its
    /// own scratch directory. The code under test reads it through `System::IO`, which is the part
    /// that has to be portable.
    class WorldLoaderTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            IdRegistry::ResetForTesting();
            directory_ = std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/world-loader-" +
                         ::testing::UnitTest::GetInstance()->current_test_info()->name();
            std::filesystem::remove_all(directory_);
            std::filesystem::create_directories(directory_);
        }

        void TearDown() override
        {
            std::filesystem::remove_all(directory_);
            IdRegistry::ResetForTesting();
        }

        void Write(const std::string& name, const std::string& text) const
        {
            std::ofstream out(directory_ + "/" + name, std::ios::binary | std::ios::trunc);
            out << text;
        }

        /// A manifest listing exactly the files the test has written, with placeholder hashes.
        /// The hashes are not checked here -- that is `HOUSE-00364` -- but they are required to be
        /// present and well formed, so the fixture carries real-looking ones.
        void WriteManifest(const std::vector<std::string>& members) const
        {
            std::string text = R"({"schema": "cna-house/manifest/1",)"
                               R"("worldHash": "sha256:)" +
                               std::string(64, '0') + R"(","members": [)";
            for (std::size_t index = 0; index < members.size(); ++index)
            {
                if (index != 0)
                {
                    text += ',';
                }
                text += R"({"file": ")" + members[index] + R"(", "sha256": "sha256:)" + std::string(63, 'a') +
                        std::to_string(index % 10) + R"("})";
            }
            text += "]}";
            Write("world.manifest.json", text);
        }

        static std::string Levels()
        {
            return R"({
              "schema": "cna-house/levels/1",
              "units": "metres",
              "north": "-Z",
              "levels": [
                { "id": "L0", "name": "Main Floor", "ffl": 0.60, "ceiling": 3.30,
                  "structureDepth": 0.35 },
                { "id": "L1", "name": "Upper Floor", "ffl": 3.65, "ceiling": 6.20,
                  "structureDepth": 0.35 },
                { "id": "L3", "name": "Attic", "ffl": 9.30, "ceiling": null,
                  "structureDepth": 0.35, "roof": "ROOF_MAIN" }
              ],
              "construction": {
                "wallExterior": 0.30, "wallPartition": 0.15, "wallPlumbing": 0.20,
                "wallGarage": 0.25, "foundationWall": 0.30, "kneeWallHeight": 1.20,
                "ridgeY": 14.30, "roofPitch": 0.594, "skirting": 0.14, "cornice": 0.11,
                "balustrade": 0.95, "railing": 1.10
              },
              "plumbing": {
                "stacks": [
                  { "id": "STACK_A", "cells": ["L0_WC1", "L1_WC4"],
                    "chase": { "x": [2.20, 4.90], "z": [-22.00, -20.20] },
                    "dropTo": "B1_UTILITY" },
                  { "id": "STACK_F", "cells": ["B1_WC7"],
                    "chase": { "x": [0.00, 1.00], "z": [0.00, 1.00] },
                    "dropTo": null }
                ]
              }
            })";
        }

        /// The smallest world the loader can finish on: a manifest and the levels it lists.
        void WriteMinimalWorld() const
        {
            Write("layout.levels.json", Levels());
            WriteManifest({"layout.levels.json"});
        }

        std::string directory_;
    };

    // --- the manifest ---------------------------------------------------------------------------

    TEST_F(WorldLoaderTest, AManifestReadsItsMembersInOrder)
    {
        WriteMinimalWorld();
        const auto manifest = world::WorldLoader::LoadManifest(directory_);
        ASSERT_TRUE(manifest) << manifest.Error().ToString();
        EXPECT_EQ(manifest.Value().version, 1);
        EXPECT_EQ(manifest.Value().members.size(), 1U);
        EXPECT_EQ(manifest.Value().members[0].file, "layout.levels.json");
        EXPECT_EQ(manifest.Value().worldHash, "sha256:" + std::string(64, '0'));
    }

    TEST_F(WorldLoaderTest, AMemberThatIsNotInTheDirectoryIsALoadError)
    {
        Write("layout.levels.json", Levels());
        WriteManifest({"layout.levels.json", "layout.cells.json"});

        const auto manifest = world::WorldLoader::LoadManifest(directory_);
        ASSERT_FALSE(manifest);
        EXPECT_EQ(manifest.Error().Code(), ErrorCode::NotFound);
        EXPECT_NE(manifest.Error().Message().find("layout.cells.json"), std::string::npos)
            << manifest.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AWorldFileTheManifestDoesNotListIsAlsoALoadError)
    {
        // The other direction, and the one that is easy to leave out. An unlisted world file is a
        // second source of truth that nothing hashes, so a save's `worldHash` would not change
        // when it did.
        Write("layout.levels.json", Levels());
        Write("layout.cells.json", R"({"schema": "cna-house/cells/1", "cells": []})");
        WriteManifest({"layout.levels.json"});

        const auto manifest = world::WorldLoader::LoadManifest(directory_);
        ASSERT_FALSE(manifest);
        EXPECT_EQ(manifest.Error().Code(), ErrorCode::InvalidData);
        EXPECT_NE(manifest.Error().Message().find("layout.cells.json"), std::string::npos)
            << manifest.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AFileThatIsNotAWorldFileIsNotTheManifestsBusiness)
    {
        // §15.1 speaks for sixteen names. A README, a `.gitkeep` or the ASSET manifest sitting in
        // the same directory is not an unlisted world file, and reporting it would train an author
        // to ignore the rule that catches the real case above.
        WriteMinimalWorld();
        Write("README.md", "notes\n");
        Write("assets.manifest.json", R"({"schema": "cna-house/assets/1", "assets": []})");

        const auto manifest = world::WorldLoader::LoadManifest(directory_);
        EXPECT_TRUE(manifest) << manifest.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AMemberListedTwiceIsRefused)
    {
        Write("layout.levels.json", Levels());
        WriteManifest({"layout.levels.json", "layout.levels.json"});

        const auto manifest = world::WorldLoader::LoadManifest(directory_);
        ASSERT_FALSE(manifest);
        EXPECT_EQ(manifest.Error().Code(), ErrorCode::Duplicate);
    }

    TEST_F(WorldLoaderTest, AMissingManifestNamesTheFileAndNotTheDirectory)
    {
        const auto manifest = world::WorldLoader::LoadManifest(directory_);
        ASSERT_FALSE(manifest);
        EXPECT_EQ(manifest.Error().Code(), ErrorCode::NotFound);
        EXPECT_NE(manifest.Error().Context().find("world.manifest.json"), std::string::npos)
            << manifest.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AMissingDirectoryIsNotAMissingFile)
    {
        const auto manifest = world::WorldLoader::LoadManifest(directory_ + "/nowhere");
        ASSERT_FALSE(manifest);
        EXPECT_EQ(manifest.Error().Code(), ErrorCode::NotFound);
        EXPECT_NE(manifest.Error().Message().find("directory"), std::string::npos)
            << manifest.Error().ToString();
    }

    // --- the schema header ----------------------------------------------------------------------

    TEST_F(WorldLoaderTest, AFileHoldingAnotherKindIsRefusedBeforeAnyRowIsRead)
    {
        // The one mistake that makes every subsequent message misleading. Deployed under the wrong
        // name, a cells file would produce forty "expected a level, found ..." errors, none of
        // which say the real thing.
        Write("layout.levels.json", R"({"schema": "cna-house/cells/1", "cells": []})");
        WriteManifest({"layout.levels.json"});

        world::WorldData::Contents contents;
        const auto levels = world::WorldLoader::LoadLevels(directory_, contents);
        ASSERT_FALSE(levels);
        EXPECT_EQ(levels.Error().Code(), ErrorCode::SchemaMismatch);
        EXPECT_NE(levels.Error().Message().find("cells"), std::string::npos) << levels.Error().ToString();
        EXPECT_TRUE(contents.levels.empty()) << "nothing may be read out of the wrong file";
    }

    TEST_F(WorldLoaderTest, AMalformedHeaderIsRefused)
    {
        for (const std::string header :
             {R"("levels/1")", R"("cna-house/levels")", R"("cna-house/levels/one")", R"("")"})
        {
            Write("layout.levels.json", R"({"schema": )" + header + R"(, "levels": []})");
            world::WorldData::Contents contents;
            const auto levels = world::WorldLoader::LoadLevels(directory_, contents);
            ASSERT_FALSE(levels) << "accepted " << header;
            EXPECT_EQ(levels.Error().Code(), ErrorCode::SchemaMismatch) << header;
        }
    }

    // --- the levels -------------------------------------------------------------------------

    TEST_F(WorldLoaderTest, TheLevelsAndTheirConstructionConstantsAreRead)
    {
        WriteMinimalWorld();
        world::WorldData::Contents contents;
        const auto levels = world::WorldLoader::LoadLevels(directory_, contents);
        ASSERT_TRUE(levels) << levels.Error().ToString();

        ASSERT_EQ(contents.levels.size(), 3U);
        EXPECT_EQ(contents.levels[0].id, Intern("L0"));
        EXPECT_EQ(contents.levels[0].name, "Main Floor");
        EXPECT_FLOAT_EQ(contents.levels[0].ffl, 0.60F);
        EXPECT_FLOAT_EQ(contents.levels[0].structureDepth, 0.35F);

        EXPECT_FLOAT_EQ(contents.construction.wallExterior, 0.30F);
        EXPECT_FLOAT_EQ(contents.construction.wallPartition, 0.15F);
        EXPECT_FLOAT_EQ(contents.construction.ridgeY, 14.30F);
        EXPECT_FLOAT_EQ(contents.construction.railing, 1.10F);
    }

    TEST_F(WorldLoaderTest, ANullCeilingIsAbsentAndNotZero)
    {
        // `docs/world-format.md`: `null` means "not specified", **never** a synonym for zero. Read
        // as 0.0 the attic would be a level whose ceiling is below its floor, and every cell in it
        // would have a negative height that looks like a geometry bug rather than a read bug.
        WriteMinimalWorld();
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, contents));

        ASSERT_EQ(contents.levels.size(), 3U);
        EXPECT_TRUE(contents.levels[0].ceiling.has_value());
        EXPECT_FLOAT_EQ(*contents.levels[0].ceiling, 3.30F);
        EXPECT_FALSE(contents.levels[2].ceiling.has_value())
            << "the attic is bounded by rafters, and null must not become 0.0";
        EXPECT_EQ(contents.levels[2].roof, Intern("ROOF_MAIN"));
    }

    TEST_F(WorldLoaderTest, ACeilingOfTheWrongTypeIsStillAnError)
    {
        // "Optional" is about absence, not about type. A ceiling of `"3.30"` is an authoring
        // mistake and reading it as "not specified" would hide it behind a rafter-bounded level.
        Write("layout.levels.json",
              R"({"schema": "cna-house/levels/1",
                  "levels": [{"id": "L0", "ffl": 0.6, "ceiling": "3.30"}],
                  "construction": {}})");
        world::WorldData::Contents contents;
        const auto levels = world::WorldLoader::LoadLevels(directory_, contents);
        ASSERT_FALSE(levels);
        EXPECT_NE(levels.Error().Context().find("ceiling"), std::string::npos) << levels.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AnUnspecifiedReferenceIsAnInvalidIdAndNotTheEmptyString)
    {
        // Two levels with no `roof` must not end up pointing at the same thing. `Id{}` is invalid;
        // `Intern("")` would be a perfectly good id that every unspecified reference in the file
        // shares.
        WriteMinimalWorld();
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, contents));

        EXPECT_FALSE(contents.levels[0].roof.IsValid());
        EXPECT_FALSE(contents.levels[1].roof.IsValid());
        EXPECT_NE(contents.levels[0].roof, Intern(""));
        EXPECT_TRUE(contents.levels[2].roof.IsValid());
    }

    TEST_F(WorldLoaderTest, TheErrorNamesTheFileAndTheJsonPath)
    {
        // `HOUSE-00028`'s whole point, kept intact one layer up: the message a person reads has to
        // send them to a line, not to a file.
        Write("layout.levels.json",
              R"({"schema": "cna-house/levels/1",
                  "levels": [{"id": "L0", "ffl": 0.6},
                             {"id": "L1", "ffl": "high"}],
                  "construction": {}})");
        world::WorldData::Contents contents;
        const auto levels = world::WorldLoader::LoadLevels(directory_, contents);
        ASSERT_FALSE(levels);

        const std::string context = levels.Error().Context();
        EXPECT_NE(context.find("layout.levels.json"), std::string::npos) << context;
        EXPECT_NE(context.find("levels"), std::string::npos) << context;
        EXPECT_NE(context.find('1'), std::string::npos)
            << "the SECOND row is the broken one and the path must say so: " << context;
        EXPECT_NE(context.find("ffl"), std::string::npos) << context;
    }

    TEST_F(WorldLoaderTest, AWorldWithNoLevelsIsRefused)
    {
        Write("layout.levels.json", R"({"schema": "cna-house/levels/1", "levels": [], "construction": {}})");
        world::WorldData::Contents contents;
        const auto levels = world::WorldLoader::LoadLevels(directory_, contents);
        ASSERT_FALSE(levels);
        EXPECT_EQ(levels.Error().Code(), ErrorCode::InvalidData);
    }

    TEST_F(WorldLoaderTest, AMissingConstructionBlockIsRefused)
    {
        // Every wall thickness derives from it. Defaulted to zero the house would be built from
        // zero-thickness walls, which looks like a rendering bug and is a data bug.
        Write("layout.levels.json",
              R"({"schema": "cna-house/levels/1",
                  "levels": [{"id": "L0", "ffl": 0.6, "ceiling": 3.3}]})");
        world::WorldData::Contents contents;
        const auto levels = world::WorldLoader::LoadLevels(directory_, contents);
        ASSERT_FALSE(levels);
    }

    // --- the plumbing stacks ----------------------------------------------------------------

    TEST_F(WorldLoaderTest, ThePlumbingStacksAreRead)
    {
        WriteMinimalWorld();
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, contents));

        ASSERT_EQ(contents.plumbing.size(), 2U);
        EXPECT_EQ(contents.plumbing[0].id, Intern("STACK_A"));
        EXPECT_EQ(contents.plumbing[0].cells.size(), 2U);
        EXPECT_EQ(contents.plumbing[0].cells[1], Intern("L1_WC4"));
        EXPECT_FLOAT_EQ(contents.plumbing[0].chase.minX, 2.20F);
        EXPECT_FLOAT_EQ(contents.plumbing[0].chase.maxZ, -20.20F);
        EXPECT_EQ(contents.plumbing[0].dropTo, Intern("B1_UTILITY"));
        EXPECT_FALSE(contents.plumbing[1].dropTo.IsValid()) << "a null dropTo is not an id";
    }

    TEST_F(WorldLoaderTest, AnInvertedRangeIsRefusedWhereItIsRead)
    {
        // Rule 2 would catch it too, but not until `validate_world.py` runs, and by then a
        // `Footprint` with min > max has already made `Contains` answer false everywhere -- which
        // looks like a missing room rather than an inverted rectangle.
        Write("layout.levels.json",
              R"({"schema": "cna-house/levels/1",
                  "levels": [{"id": "L0", "ffl": 0.6, "ceiling": 3.3}],
                  "construction": {},
                  "plumbing": {"stacks": [
                    {"id": "STACK_A", "cells": ["L0_WC1"],
                     "chase": {"x": [4.90, 2.20], "z": [-22.0, -20.2]}}]}})");
        world::WorldData::Contents contents;
        const auto levels = world::WorldLoader::LoadLevels(directory_, contents);
        ASSERT_FALSE(levels);
        EXPECT_EQ(levels.Error().Code(), ErrorCode::InvalidData);
        EXPECT_NE(levels.Error().Message().find("min < max"), std::string::npos) << levels.Error().ToString();
    }

    TEST_F(WorldLoaderTest, ARangeOfThreeNumbersIsRefused)
    {
        Write("layout.levels.json",
              R"({"schema": "cna-house/levels/1",
                  "levels": [{"id": "L0", "ffl": 0.6, "ceiling": 3.3}],
                  "construction": {},
                  "plumbing": {"stacks": [
                    {"id": "STACK_A", "cells": ["L0_WC1"],
                     "chase": {"x": [0.0, 1.0, 2.0], "z": [0.0, 1.0]}}]}})");
        world::WorldData::Contents contents;
        ASSERT_FALSE(world::WorldLoader::LoadLevels(directory_, contents));
    }

    // --- the whole load -----------------------------------------------------------------------

    TEST_F(WorldLoaderTest, LoadProducesAWorldDataWithItsIndicesBuilt)
    {
        WriteMinimalWorld();
        const auto world = world::WorldLoader::Load(directory_);
        ASSERT_TRUE(world) << world.Error().ToString();

        EXPECT_EQ(world.Value().Levels().size(), 3U);
        ASSERT_NE(world.Value().FindLevel(Intern("L1")), nullptr);
        EXPECT_FLOAT_EQ(world.Value().FindLevel(Intern("L1"))->ffl, 3.65F);
        EXPECT_NE(world.Value().FindPlumbingStack(Intern("STACK_A")), nullptr);
        EXPECT_FLOAT_EQ(world.Value().GetConstruction().roofPitch, 0.594F);
    }

    TEST_F(WorldLoaderTest, LoadStopsAtTheManifestWhenTheManifestIsWrong)
    {
        // The manifest is read first because it says what the world IS. Reading the levels of a
        // world whose file list is wrong would produce a `WorldData` that is a subset of a house
        // and does not know it.
        Write("layout.levels.json", Levels());
        WriteManifest({"layout.levels.json", "layout.portals.json"});

        const auto world = world::WorldLoader::Load(directory_);
        ASSERT_FALSE(world);
        EXPECT_EQ(world.Error().Code(), ErrorCode::NotFound);
        EXPECT_NE(world.Error().Message().find("layout.portals.json"), std::string::npos);
    }

    TEST_F(WorldLoaderTest, ADuplicateLevelIdIsCaughtWhereTheModelIsBuilt)
    {
        Write("layout.levels.json",
              R"({"schema": "cna-house/levels/1",
                  "levels": [{"id": "L0", "ffl": 0.6, "ceiling": 3.3},
                             {"id": "L0", "ffl": 3.6, "ceiling": 6.2}],
                  "construction": {}})");
        WriteManifest({"layout.levels.json"});

        const auto world = world::WorldLoader::Load(directory_);
        ASSERT_FALSE(world);
        EXPECT_EQ(world.Error().Code(), ErrorCode::Duplicate);
        EXPECT_NE(world.Error().Message().find("L0"), std::string::npos) << world.Error().ToString();
    }

    TEST_F(WorldLoaderTest, TheFileListIsTheSixteenOfTheArchitecture)
    {
        const auto names = world::WorldLoader::FileNames();
        EXPECT_EQ(names.size(), 16U);
        EXPECT_EQ(names[0], "world.manifest.json") << "the index is read first";
        EXPECT_EQ(names[1], "layout.levels.json") << "and the levels before anything that names one";

        // The order is the DEPENDENCY order, not §15.1's printing order: cells name levels and
        // materials, portals name cells. A reference must always be into something already read.
        const auto position = [&names](std::string_view name)
        {
            return static_cast<std::size_t>(
                std::distance(names.begin(), std::find(names.begin(), names.end(), name)));
        };
        EXPECT_LT(position("layout.levels.json"), position("layout.cells.json"));
        EXPECT_LT(position("layout.materials.json"), position("layout.cells.json"));
        EXPECT_LT(position("layout.cells.json"), position("layout.portals.json"));
        EXPECT_LT(position("layout.portals.json"), position("layout.openings.json"));
        EXPECT_LT(position("layout.cells.json"), position("layout.props.json"));
    }

    TEST_F(WorldLoaderTest, JoinProducesOneSeparatorWhateverItIsGiven)
    {
        EXPECT_EQ(world::WorldLoader::Join("a", "b.json"), "a/b.json");
        EXPECT_EQ(world::WorldLoader::Join("a/", "b.json"), "a/b.json");
        EXPECT_EQ(world::WorldLoader::Join("a///", "b.json"), "a/b.json");
    }
} // namespace
