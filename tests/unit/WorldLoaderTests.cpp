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
#include <utility>
#include <vector>

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

        static std::string Materials()
        {
            return R"({
              "schema": "cna-house/materials/1",
              "materials": [
                {
                  "id": "MAT_TILE_PORCELAIN_GREY", "class": "tile",
                  "albedo": "Textures/Architecture/tile-porcelain-grey", "normal": null,
                  "lightmapChannel": 1, "tint": [1.0, 1.0, 1.0],
                  "specularPower": 48.0, "specularColor": [0.30, 0.30, 0.30],
                  "alphaMode": "opaque", "alphaCutoff": null, "twoSided": false,
                  "uvScale": [4.0, 4.0],
                  "wetResponse": {"albedoDarken": 0.22, "specularBoost": 2.1, "powerBoost": 2.5},
                  "snowResponse": {"coverable": true, "slopeLimitDeg": 40},
                  "footstepSurface": "tile", "audioAbsorption": 0.06,
                  "effectTierS": "DualTexture", "effectTierE": "RoomLit"
                },
                { "id": "MAT_LEAF", "class": "foliage", "alphaMode": "mask", "alphaCutoff": 0.5,
                  "twoSided": true },
                { "id": "MAT_LAMP", "class": "emissive" },
                { "id": "MAT_DECK_WET", "class": "wet_wood" },
                { "id": "MAT_DRIVE_SNOW", "class": "snow_asphalt" }
              ]
            })";
        }

        static std::string Cells()
        {
            return R"({
              "schema": "cna-house/cells/1",
              "cells": [
                {
                  "id": "L0_KITCHEN", "level": "L0", "name": "Kitchen", "kind": "room",
                  "boxes": [{ "x": [-8.20, 2.20], "z": [-27.10, -23.00] }],
                  "yOverride": null,
                  "floorMaterial": "MAT_TILE_PORCELAIN_GREY",
                  "wallMaterial": "MAT_PAINT_WARM_WHITE",
                  "ceilingMaterial": "MAT_PAINT_FLAT_WHITE",
                  "footstepSurface": "tile",
                  "acoustic": { "roomTone": "AMB_KITCHEN", "absorption": 0.28,
                                "reverbHint": "small_hard" },
                  "thermal": { "heated": true, "ductBranch": "DUCT_L0_W" },
                  "lightGroups": ["LG_L0_KITCHEN_MAIN", "LG_L0_KITCHEN_UNDERCAB"],
                  "daylight": { "windowIds": ["W_L0_KITCHEN_N1", "W_L0_KITCHEN_N2"],
                                "orientation": "NE", "exposure": 0.55 },
                  "residencyPack": "house-l0", "lodBias": 0,
                  "visibilityHint": "opaque", "navMeshRegion": "NAV_L0_KITCHEN"
                },
                {
                  "id": "L0_HALL", "level": "L0", "kind": "corridor",
                  "boxes": [{ "x": [-2.0, 2.0], "z": [4.0, 10.0] },
                            { "x": [2.0, 6.0], "z": [8.0, 10.0] }]
                },
                {
                  "id": "L0_STAIR", "level": "L0", "kind": "stair",
                  "boxes": [{ "x": [-6.0, -2.0], "z": [4.0, 8.0] }],
                  "yOverride": [0.60, 3.65],
                  "visibilityHint": "open"
                },
                {
                  "id": "L0_TERRACE", "level": "L0", "kind": "exterior",
                  "boxes": [{ "x": [-2.0, 2.0], "z": [-4.0, 0.0] }]
                },
                {
                  "id": "L0_WC1", "level": "L0", "kind": "room",
                  "boxes": [{ "x": [2.0, 4.0], "z": [4.0, 6.0] }]
                },
                {
                  "id": "L1_LANDING", "level": "L1", "kind": "room",
                  "boxes": [{ "x": [-6.0, -2.0], "z": [4.0, 8.0] }]
                }
              ]
            })";
        }

        /// Four portals over `Cells()`: an opening, a door, the horizontal stair well, and a
        /// window with a traversal cap.
        ///
        /// The hall's face on `x = 2` runs `z 4..10`; the WC's runs `z 4..6`. The difference is
        /// what the "in one cell's wall and not the other's" test turns on, and it is the reason
        /// the plane check looks at both sides rather than the first.
        static std::string Portals()
        {
            return R"({
              "schema": "cna-house/portals/1",
              "portals": [
                { "id": "P_HALL__STAIR", "cellA": "L0_HALL", "cellB": "L0_STAIR",
                  "plane": { "axis": "x", "value": -2.0 },
                  "rect": { "u": [5.0, 6.0], "v": [0.60, 2.65] },
                  "kind": "cased_opening", "aperture": null, "opacity": "open",
                  "maxDepth": null },
                { "id": "P_HALL__WC1", "cellA": "L0_HALL", "cellB": "L0_WC1",
                  "plane": { "axis": "x", "value": 2.0 },
                  "rect": { "u": [4.6, 5.5], "v": [0.60, 2.62] },
                  "kind": "door", "aperture": "DOOR_L0_WC1",
                  "opacity": "opaque_when_closed", "maxDepth": null,
                  "soundLoss": { "open": 0.05, "closed": 0.55 } },
                { "id": "P_STAIR__LANDING", "cellA": "L0_STAIR", "cellB": "L1_LANDING",
                  "plane": { "axis": "y", "value": 3.65 },
                  "rect": { "u": [-5.5, -2.5], "v": [4.5, 7.5] },
                  "kind": "stair_well", "opacity": "open" },
                { "id": "P_HALL__WC1_GLASS", "cellA": "L0_HALL", "cellB": "L0_WC1",
                  "plane": { "axis": "x", "value": 2.0 },
                  "rect": { "u": [4.0, 4.5], "v": [1.00, 1.60] },
                  "kind": "window", "opacity": "glass", "maxDepth": 2 }
              ]
            })";
        }

        /// One portal file holding the WC door with one field replaced.
        ///
        /// @p patch is a whole `"key": value` pair; the key it names replaces the default, or is
        /// added if the default does not have it. Writing the tests as a patch rather than as
        /// eleven near-identical JSON blobs is what keeps the ONE thing each case changes visible.
        static std::string OnePortal(const std::string& patch)
        {
            std::vector<std::pair<std::string, std::string>> fields{
                {"\"id\"", "\"P_HALL__WC1\""},
                {"\"cellA\"", "\"L0_HALL\""},
                {"\"cellB\"", "\"L0_WC1\""},
                {"\"plane\"", R"({ "axis": "x", "value": 2.0 })"},
                {"\"rect\"", R"({ "u": [4.6, 5.5], "v": [0.60, 2.62] })"},
                {"\"kind\"", "\"door\""},
            };

            const std::size_t colon = patch.find(':');
            std::string key = patch.substr(0, colon);
            while (!key.empty() && (key.front() == ' ' || key.front() == '\n'))
            {
                key.erase(key.begin());
            }
            while (!key.empty() && (key.back() == ' ' || key.back() == '\n'))
            {
                key.pop_back();
            }
            const std::string value = patch.substr(colon + 1);

            bool replaced = false;
            for (auto& field : fields)
            {
                if (field.first == key)
                {
                    field.second = value;
                    replaced = true;
                }
            }
            if (!replaced)
            {
                fields.emplace_back(key, value);
            }

            std::string text = R"({"schema": "cna-house/portals/1", "portals": [{)";
            for (std::size_t index = 0; index < fields.size(); ++index)
            {
                if (index != 0)
                {
                    text += ',';
                }
                text += fields[index].first + ':' + fields[index].second;
            }
            return text + "}]}";
        }

        /// The stair well with one field replaced, for the horizontal-plane cases.
        static std::string OneStairWell(const std::string& patch)
        {
            std::string row = R"({ "id": "P_STAIR__LANDING", "cellA": "L0_STAIR",
                                   "cellB": "L1_LANDING",
                                   "plane": { "axis": "y", "value": 3.65 },
                                   "rect": { "u": [-5.5, -2.5], "v": [4.5, 7.5] },
                                   "kind": "stair_well" })";
            const std::size_t colon = patch.find(':');
            const std::string key = patch.substr(0, colon);
            const std::size_t at = row.find(key);
            // Every patch this helper is given names a field the row already has; a typo in the
            // key would otherwise leave the row unchanged and the test would pass for no reason.
            if (at == std::string::npos)
            {
                ADD_FAILURE() << "the stair-well row has no field " << key;
                return "{}";
            }
            const std::size_t end = row.find('\n', at);
            row.replace(at, (end == std::string::npos ? row.size() : end) - at, patch + ",");
            return R"({"schema": "cna-house/portals/1", "portals": [)" + row + "]}";
        }

        /// Levels, cells and portals, loaded in the order the loader reads them.
        [[nodiscard]] cnahouse::util::Result<void> LoadUpToPortals(world::WorldData::Contents& contents) const
        {
            Write("layout.levels.json", Levels());
            Write("layout.cells.json", Cells());
            Write("layout.portals.json", Portals());
            if (const auto levels = world::WorldLoader::LoadLevels(directory_, contents); !levels)
            {
                return levels.Error();
            }
            if (const auto cells = world::WorldLoader::LoadCells(directory_, contents); !cells)
            {
                return cells.Error();
            }
            return world::WorldLoader::LoadPortals(directory_, contents);
        }

        /// Three leaves over `Portals()`: a hinged door, a slider that does not swing, and a
        /// solid lockable door hinged on the other side.
        static std::string Openings()
        {
            return R"({
              "schema": "cna-house/openings/1",
              "openings": [
                {
                  "id": "DOOR_L0_WC1", "kind": "door", "portal": "P_HALL__WC1",
                  "leaf": { "width": 0.86, "height": 2.04, "thickness": 0.040 },
                  "hinge": "left", "swing": "into_L0_WC1", "maxAngleDeg": 95.0,
                  "frame": { "asset": "MODEL_DOOR_FRAME_INT_01", "casing": 0.070 },
                  "asset": "MODEL_DOOR_LEAF_PANEL_01", "material": "MAT_PAINT_TRIM_WHITE",
                  "solid": false, "lockable": false
                },
                {
                  "id": "WIN_L0_WC1", "kind": "window", "portal": "P_HALL__WC1_GLASS",
                  "leaf": { "width": 0.50, "height": 0.60, "thickness": 0.006 },
                  "hinge": null, "swing": null, "maxAngleDeg": 0.0
                },
                {
                  "id": "DOOR_L0_STAIR", "kind": "door", "portal": "P_HALL__STAIR",
                  "leaf": { "width": 0.90, "height": 2.04, "thickness": 0.045 },
                  "hinge": "right", "swing": "into_L0_HALL", "maxAngleDeg": 90.0,
                  "solid": true, "lockable": true
                }
              ]
            })";
        }

        /// Two flights: the main stair with a half-landing, and a short one that states no
        /// `collisionRamp` so the default can be seen.
        static std::string Stairs()
        {
            return R"({
              "schema": "cna-house/stairs/1",
              "flights": [
                {
                  "id": "STAIR_L0_L1_MAIN", "fromCell": "L0_STAIR", "toCell": "L1_LANDING",
                  "risers": 17, "rise": 0.17941176, "going": 0.280, "width": 1.20,
                  "landings": [{ "at": 9, "depth": 1.20 }],
                  "collisionRamp": true, "surface": "wood"
                },
                {
                  "id": "STAIR_L0_TERRACE", "fromCell": "L0_HALL", "toCell": "L0_TERRACE",
                  "risers": 3, "rise": 0.15, "going": 0.30, "width": 1.00
                }
              ]
            })";
        }

        /// The smallest world the loader can finish on: a manifest and the files it lists.
        void WriteMinimalWorld() const
        {
            Write("layout.levels.json", Levels());
            WriteManifest({"layout.levels.json"});
        }

        /// The same, plus everything the loader can read so far, for the tests that go all the
        /// way through `Load`.
        void WriteWorldWithMaterials() const
        {
            Write("layout.levels.json", Levels());
            Write("layout.materials.json", Materials());
            Write("layout.cells.json", Cells());
            Write("layout.portals.json", Portals());
            Write("layout.openings.json", Openings());
            Write("layout.stairs.json", Stairs());
            WriteManifest({"layout.levels.json",
                           "layout.materials.json",
                           "layout.cells.json",
                           "layout.portals.json",
                           "layout.openings.json",
                           "layout.stairs.json"});
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

    // --- the materials --------------------------------------------------------------------------

    TEST_F(WorldLoaderTest, AMaterialIsReadWithEveryFieldItCarries)
    {
        Write("layout.materials.json", Materials());
        world::WorldData::Contents contents;
        const auto materials = world::WorldLoader::LoadMaterials(directory_, contents);
        ASSERT_TRUE(materials) << materials.Error().ToString();

        ASSERT_EQ(contents.materials.size(), 5U);
        const world::Material& tile = contents.materials[0];
        EXPECT_EQ(tile.id, Intern("MAT_TILE_PORCELAIN_GREY"));
        EXPECT_EQ(tile.materialClass, world::MaterialClass::Tile);
        EXPECT_EQ(tile.surfaceState, world::SurfaceState::Dry);
        EXPECT_EQ(tile.albedo, "Textures/Architecture/tile-porcelain-grey");
        EXPECT_EQ(tile.normal, "") << "a null texture is not specified, not a path called \"null\"";
        EXPECT_EQ(tile.lightmapChannel, 1);
        EXPECT_FLOAT_EQ(tile.specularPower, 48.0F);
        EXPECT_EQ(tile.alphaMode, world::AlphaMode::Opaque);
        EXPECT_FALSE(tile.alphaCutoff.has_value());
        EXPECT_FLOAT_EQ(tile.uvScaleU, 4.0F);
        EXPECT_FLOAT_EQ(tile.wet.albedoDarken, 0.22F);
        EXPECT_TRUE(tile.snow.coverable);
        EXPECT_FLOAT_EQ(tile.snow.slopeLimitDeg, 40.0F);
        EXPECT_EQ(tile.footstepSurface, "tile");
        EXPECT_FLOAT_EQ(tile.audioAbsorption, 0.06F);
        EXPECT_EQ(tile.effectTierS, world::EffectTier::DualTexture);
        EXPECT_EQ(tile.effectTierE, "RoomLit");
    }

    TEST_F(WorldLoaderTest, AWetOrSnowyClassIsTheSameClassInAnotherState)
    {
        // §22.2's `wet_<class>` and `snow_<class>` are derived spellings, not sixty classes:
        // `wet_wood` is wood with a darkened albedo. Reading them apart is what lets the effect
        // fallback consult one table of twenty rows.
        Write("layout.materials.json", Materials());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadMaterials(directory_, contents));

        const world::Material& wet = contents.materials[3];
        EXPECT_EQ(wet.materialClass, world::MaterialClass::Wood);
        EXPECT_EQ(wet.surfaceState, world::SurfaceState::Wet);
        EXPECT_EQ(world::SpellMaterialClass({wet.materialClass, wet.surfaceState}), "wet_wood");

        const world::Material& snowy = contents.materials[4];
        EXPECT_EQ(snowy.materialClass, world::MaterialClass::Asphalt);
        EXPECT_EQ(snowy.surfaceState, world::SurfaceState::Snowy);
    }

    TEST_F(WorldLoaderTest, AClassOutsideParagraph22Point2IsRefusedAndQuotedAsAuthored)
    {
        // The message must quote what was WRITTEN. Told that `marble` is not a class, an author
        // looking at a field that says `wet_marble` goes hunting for a field that does not exist.
        Write("layout.materials.json",
              R"({"schema": "cna-house/materials/1",
                  "materials": [{"id": "MAT_X", "class": "wet_marble"}]})");
        world::WorldData::Contents contents;
        const auto materials = world::WorldLoader::LoadMaterials(directory_, contents);
        ASSERT_FALSE(materials);
        EXPECT_EQ(materials.Error().Code(), ErrorCode::InvalidData);
        EXPECT_NE(materials.Error().Message().find("wet_marble"), std::string::npos)
            << materials.Error().ToString();
        EXPECT_NE(materials.Error().Message().find("concrete"), std::string::npos)
            << "and list the vocabulary: " << materials.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AnAbsentEffectTierFallsBackToTheClassTable)
    {
        // §22.2's table is the *documented* fallback, and `build_chunks.py` applies the same one to
        // choose a vertex layout. The `foliage` row states no tier and must come out AlphaTest.
        Write("layout.materials.json", Materials());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadMaterials(directory_, contents));

        EXPECT_EQ(contents.materials[1].materialClass, world::MaterialClass::Foliage);
        EXPECT_EQ(contents.materials[1].effectTierS, world::EffectTier::AlphaTest)
            << "foliage with no effectTierS must fall back to §22.2's AlphaTestEffect";
        EXPECT_EQ(contents.materials[2].effectTierS, world::EffectTier::Basic)
            << "and emissive to BasicEffect";
    }

    TEST_F(WorldLoaderTest, TheClassTableAgreesWithBuildChunksRowForRow)
    {
        // The two readings of §22.2 that must not drift: this one and `build_chunks.py`'s
        // `CLASS_TO_LAYOUT`. A chunk built with one vertex layout and drawn with the effect the
        // other chose is a wrong-looking surface nobody can trace back to a table.
        using world::DefaultEffectTier;
        using MC = world::MaterialClass;
        using ET = world::EffectTier;

        for (const MC value : {MC::Paint,
                               MC::Wood,
                               MC::Carpet,
                               MC::Tile,
                               MC::Stone,
                               MC::Concrete,
                               MC::Asphalt,
                               MC::Gravel,
                               MC::Grass,
                               MC::Soil})
        {
            EXPECT_EQ(DefaultEffectTier(value), ET::DualTexture) << world::ToStringView(value);
        }
        for (const MC value : {MC::Metal, MC::Plastic, MC::Glass, MC::Fabric, MC::Water, MC::Emissive})
        {
            EXPECT_EQ(DefaultEffectTier(value), ET::Basic) << world::ToStringView(value);
        }
        for (const MC value : {MC::Foliage, MC::Hair})
        {
            EXPECT_EQ(DefaultEffectTier(value), ET::AlphaTest) << world::ToStringView(value);
        }
        // `build_chunks.py` has no layout for these two and refuses to batch a static prop wearing
        // one, because a skinned prop is an animated prop and batching it would freeze it in its
        // bind pose inside a wall. Here they are the effect §22.2 names.
        for (const MC value : {MC::Skin, MC::Fur})
        {
            EXPECT_EQ(DefaultEffectTier(value), ET::Skinned) << world::ToStringView(value);
        }
    }

    TEST_F(WorldLoaderTest, AMaskedMaterialWithoutACutoffIsRefused)
    {
        // There is no threshold to test against, and `AlphaTestEffect` would quietly use its own
        // default rather than the author's -- a foliage card with the wrong fringe, everywhere.
        Write("layout.materials.json",
              R"({"schema": "cna-house/materials/1",
                  "materials": [{"id": "MAT_LEAF", "class": "foliage", "alphaMode": "mask"}]})");
        world::WorldData::Contents contents;
        const auto materials = world::WorldLoader::LoadMaterials(directory_, contents);
        ASSERT_FALSE(materials);
        EXPECT_NE(materials.Error().Context().find("alphaCutoff"), std::string::npos)
            << materials.Error().ToString();
    }

    TEST_F(WorldLoaderTest, ARangeCheckedFieldOutsideItsRangeIsRefused)
    {
        for (const std::string row :
             {R"({"id": "MAT_X", "class": "tile", "lightmapChannel": 2})",
              R"({"id": "MAT_X", "class": "tile", "snowResponse": {"slopeLimitDeg": 400}})"})
        {
            Write("layout.materials.json",
                  R"({"schema": "cna-house/materials/1", "materials": [)" + row + "]}");
            world::WorldData::Contents contents;
            const auto materials = world::WorldLoader::LoadMaterials(directory_, contents);
            ASSERT_FALSE(materials) << "accepted " << row;
            EXPECT_EQ(materials.Error().Code(), ErrorCode::OutOfRange) << row;
        }
    }

    TEST_F(WorldLoaderTest, AMaterialsFileIsRequiredBeforeTheCellsThatNameIt)
    {
        WriteMinimalWorld();
        world::WorldData::Contents contents;
        const auto materials = world::WorldLoader::LoadMaterials(directory_, contents);
        ASSERT_FALSE(materials);
        EXPECT_EQ(materials.Error().Code(), ErrorCode::NotFound);
    }

    // --- the cells --------------------------------------------------------------------------

    TEST_F(WorldLoaderTest, ACellIsReadWithEveryFieldItCarries)
    {
        Write("layout.cells.json", Cells());
        world::WorldData::Contents contents;
        const auto cells = world::WorldLoader::LoadCells(directory_, contents);
        ASSERT_TRUE(cells) << cells.Error().ToString();

        ASSERT_EQ(contents.cells.size(), 6U);
        const world::Cell& kitchen = contents.cells[0];
        EXPECT_EQ(kitchen.id, Intern("L0_KITCHEN"));
        EXPECT_EQ(kitchen.level, Intern("L0"));
        EXPECT_EQ(kitchen.name, "Kitchen");
        EXPECT_EQ(kitchen.kind, world::CellKind::Room);
        EXPECT_EQ(kitchen.floorMaterial, Intern("MAT_TILE_PORCELAIN_GREY"));
        EXPECT_EQ(kitchen.footstepSurface, "tile");
        EXPECT_EQ(kitchen.acoustic.roomTone, Intern("AMB_KITCHEN"));
        EXPECT_FLOAT_EQ(kitchen.acoustic.absorption, 0.28F);
        EXPECT_EQ(kitchen.acoustic.reverbHint, "small_hard");
        EXPECT_TRUE(kitchen.thermal.heated);
        EXPECT_EQ(kitchen.thermal.ductBranch, Intern("DUCT_L0_W"));
        ASSERT_EQ(kitchen.lightGroups.size(), 2U);
        EXPECT_EQ(kitchen.lightGroups[1], Intern("LG_L0_KITCHEN_UNDERCAB"));
        ASSERT_EQ(kitchen.daylight.windowIds.size(), 2U);
        ASSERT_TRUE(kitchen.daylight.orientation.has_value());
        // `NE` and not the `N` of world-format.md's example, deliberately: `N` is the first value
        // of the enum, so a reader that ignored the field entirely would still pass.
        EXPECT_EQ(*kitchen.daylight.orientation, world::Orientation::NE);
        EXPECT_FLOAT_EQ(kitchen.daylight.exposure, 0.55F);
        EXPECT_EQ(kitchen.residencyPack, "house-l0");
        EXPECT_EQ(kitchen.lodBias, 0);
        EXPECT_EQ(kitchen.visibilityHint, world::VisibilityHint::Opaque);
        EXPECT_EQ(kitchen.navMeshRegion, Intern("NAV_L0_KITCHEN"));
    }

    TEST_F(WorldLoaderTest, AMultiBoxCellIsAUnionAndNotABoundingBox)
    {
        // The L-shaped hall. Its two boxes meet along one edge and the notch beside them belongs to
        // the room next door -- which is why a cell carries a LIST and why `CellContains` walks it.
        Write("layout.cells.json", Cells());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, contents));

        const world::Cell& hall = contents.cells[1];
        ASSERT_EQ(hall.boxes.size(), 2U);
        EXPECT_FLOAT_EQ(hall.boxes[0].minX, -2.0F);
        EXPECT_FLOAT_EQ(hall.boxes[1].maxZ, 10.0F);
        EXPECT_FLOAT_EQ(world::WorldData::FootprintArea(hall), 32.0F);
    }

    TEST_F(WorldLoaderTest, ACellWithNoBoxesIsRefused)
    {
        Write("layout.cells.json",
              R"({"schema": "cna-house/cells/1",
                  "cells": [{"id": "L0_GHOST", "level": "L0", "kind": "room", "boxes": []}]})");
        world::WorldData::Contents contents;
        const auto cells = world::WorldLoader::LoadCells(directory_, contents);
        ASSERT_FALSE(cells);
        EXPECT_EQ(cells.Error().Code(), ErrorCode::InvalidData);
        EXPECT_NE(cells.Error().Context().find("boxes"), std::string::npos) << cells.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AnExplicitYOverrideWinsAndANullOneDefersToTheLevel)
    {
        Write("layout.cells.json", Cells());
        Write("layout.levels.json", Levels());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, contents));
        ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, contents));

        const auto loaded = world::WorldData::Create(std::move(contents));
        ASSERT_TRUE(loaded) << loaded.Error().ToString();

        // The stair pierces the slab it climbs through, so it says so.
        const world::Cell* stair = loaded.Value().FindCell(Intern("L0_STAIR"));
        ASSERT_NE(stair, nullptr);
        ASSERT_TRUE(stair->yOverride.has_value());
        const auto stairExtent = loaded.Value().ExtentOf(*stair);
        ASSERT_TRUE(stairExtent);
        EXPECT_FLOAT_EQ(stairExtent.Value().ceilingY, 3.65F);

        // The kitchen does not, so it takes L0's ffl..ceiling.
        const world::Cell* kitchen = loaded.Value().FindCell(Intern("L0_KITCHEN"));
        ASSERT_NE(kitchen, nullptr);
        EXPECT_FALSE(kitchen->yOverride.has_value());
        const auto kitchenExtent = loaded.Value().ExtentOf(*kitchen);
        ASSERT_TRUE(kitchenExtent);
        EXPECT_FLOAT_EQ(kitchenExtent.Value().floorY, 0.60F);
        EXPECT_FLOAT_EQ(kitchenExtent.Value().ceilingY, 3.30F);
    }

    TEST_F(WorldLoaderTest, AnInvertedYOverrideIsRefused)
    {
        // Read as authored it would be a room whose ceiling is below its floor, and every
        // `CellContains` in it would answer false -- a room the player falls through, not a
        // rectangle nobody notices.
        Write("layout.cells.json",
              R"({"schema": "cna-house/cells/1",
                  "cells": [{"id": "L0_A", "level": "L0", "kind": "room",
                             "boxes": [{"x": [0, 1], "z": [0, 1]}],
                             "yOverride": [3.65, 0.60]}]})");
        world::WorldData::Contents contents;
        const auto cells = world::WorldLoader::LoadCells(directory_, contents);
        ASSERT_FALSE(cells);
        EXPECT_NE(cells.Error().Message().find("floor < ceiling"), std::string::npos)
            << cells.Error().ToString();
    }

    TEST_F(WorldLoaderTest, ACellKindOutsideTheVocabularyIsRefused)
    {
        Write("layout.cells.json",
              R"({"schema": "cna-house/cells/1",
                  "cells": [{"id": "L0_A", "level": "L0", "kind": "conservatory",
                             "boxes": [{"x": [0, 1], "z": [0, 1]}]}]})");
        world::WorldData::Contents contents;
        const auto cells = world::WorldLoader::LoadCells(directory_, contents);
        ASSERT_FALSE(cells);
        EXPECT_NE(cells.Error().Context().find("kind"), std::string::npos) << cells.Error().ToString();
        EXPECT_NE(cells.Error().Message().find("corridor"), std::string::npos)
            << "the message must list the vocabulary: " << cells.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AnOmittedOptionalBlockLeavesItsDefaultsAndNotGarbage)
    {
        // The terrace states nothing but the four required fields. Every optional block must come
        // out as its documented default, and every unspecified reference as an INVALID id -- two
        // cells with no room tone must not end up sharing one.
        Write("layout.cells.json", Cells());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, contents));

        const world::Cell& terrace = contents.cells[3];
        EXPECT_EQ(terrace.kind, world::CellKind::Exterior);
        EXPECT_FALSE(terrace.floorMaterial.IsValid());
        EXPECT_FALSE(terrace.acoustic.roomTone.IsValid());
        EXPECT_FALSE(terrace.thermal.heated);
        EXPECT_TRUE(terrace.lightGroups.empty());
        EXPECT_FALSE(terrace.daylight.orientation.has_value()) << "an unstated orientation is not North";
        EXPECT_EQ(terrace.visibilityHint, world::VisibilityHint::Opaque);
        EXPECT_EQ(terrace.residencyPack, "");
        EXPECT_NE(terrace.acoustic.roomTone, contents.cells[0].acoustic.roomTone)
            << "a cell with no room tone must not share the kitchen's";
    }

    TEST_F(WorldLoaderTest, AnOpenVisibilityHintIsRead)
    {
        Write("layout.cells.json", Cells());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, contents));
        EXPECT_EQ(contents.cells[2].visibilityHint, world::VisibilityHint::Open);
    }

    TEST_F(WorldLoaderTest, TheLoaderDoesNotResolveCrossFileReferences)
    {
        // Deliberate. Resolution is §15.7 rule 6, owned by `validate_world.py` and mirrored by
        // `WorldValidator` (`HOUSE-00357`); a third reading here could disagree with both. What the
        // loader guarantees is that the id is INTERNED, so the validator can name it.
        Write("layout.cells.json",
              R"({"schema": "cna-house/cells/1",
                  "cells": [{"id": "L0_A", "level": "L9_NOWHERE", "kind": "room",
                             "boxes": [{"x": [0, 1], "z": [0, 1]}],
                             "floorMaterial": "MAT_DOES_NOT_EXIST"}]})");
        world::WorldData::Contents contents;
        const auto cells = world::WorldLoader::LoadCells(directory_, contents);
        ASSERT_TRUE(cells) << cells.Error().ToString();
        EXPECT_EQ(contents.cells[0].level, Intern("L9_NOWHERE"));
        EXPECT_EQ(contents.cells[0].floorMaterial, Intern("MAT_DOES_NOT_EXIST"));
    }

    // --- the portals ------------------------------------------------------------------------

    TEST_F(WorldLoaderTest, APortalIsReadWithEveryFieldItCarries)
    {
        world::WorldData::Contents contents;
        ASSERT_TRUE(LoadUpToPortals(contents));

        ASSERT_EQ(contents.portals.size(), 4U);
        const world::Portal& door = contents.portals[1];
        EXPECT_EQ(door.id, Intern("P_HALL__WC1"));
        EXPECT_EQ(door.cellA, Intern("L0_HALL"));
        EXPECT_EQ(door.cellB, Intern("L0_WC1"));
        EXPECT_EQ(door.axis, world::PlaneAxis::X);
        EXPECT_FLOAT_EQ(door.planeValue, 2.0F);
        EXPECT_NEAR(door.Width(), 0.90F, 1e-5F);
        EXPECT_NEAR(door.Height(), 2.02F, 1e-5F);
        EXPECT_EQ(door.kind, world::PortalKind::Door);
        EXPECT_EQ(door.aperture, Intern("DOOR_L0_WC1"));
        EXPECT_EQ(door.opacity, world::PortalOpacity::OpaqueWhenClosed);
        EXPECT_FLOAT_EQ(door.soundLossOpen, 0.05F);
        EXPECT_FLOAT_EQ(door.soundLossClosed, 0.55F);
        EXPECT_FALSE(door.crouch);
    }

    TEST_F(WorldLoaderTest, ANullMaxDepthIsNoCapAndNotACapOfZero)
    {
        // The difference between a glazed door you can see through and a bricked-up one. Read as 0
        // the traversal would stop AT the portal.
        world::WorldData::Contents contents;
        ASSERT_TRUE(LoadUpToPortals(contents));
        EXPECT_FALSE(contents.portals[1].maxDepth.has_value());
        ASSERT_TRUE(contents.portals[3].maxDepth.has_value());
        EXPECT_EQ(*contents.portals[3].maxDepth, 2);
    }

    TEST_F(WorldLoaderTest, AHorizontalPortalIsReadAndChecked)
    {
        // A `stair_well` is in the vocabulary and is not a hole in a wall. On a `y` plane `u` is
        // world X and `v` is world Z, and the plane must be the boundary the two cells share.
        world::WorldData::Contents contents;
        ASSERT_TRUE(LoadUpToPortals(contents));

        const world::Portal& well = contents.portals[2];
        EXPECT_EQ(well.kind, world::PortalKind::StairWell);
        EXPECT_EQ(well.axis, world::PlaneAxis::Y);
        EXPECT_FLOAT_EQ(well.planeValue, 3.65F);
    }

    TEST_F(WorldLoaderTest, AHorizontalPortalAtTheWrongHeightOrOutsideTheFootprintIsRefused)
    {
        // The `y` case is checked as hard as the wall case. A stair well at the wrong height joins
        // two floors that do not meet there, and the visibility solver would see through a slab.
        for (const auto& [patch, expected] : std::vector<std::pair<std::string, std::string>>{
                 {R"("plane": { "axis": "y", "value": 4.20 })", "neither the floor nor the ceiling"},
                 {R"("rect": { "u": [-9.0, -8.0], "v": [4.5, 7.5] })", "footprint"}})
        {
            world::WorldData::Contents contents;
            Write("layout.levels.json", Levels());
            Write("layout.cells.json", Cells());
            Write("layout.portals.json", OneStairWell(patch));
            ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, contents));
            ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, contents));

            const auto portals = world::WorldLoader::LoadPortals(directory_, contents);
            ASSERT_FALSE(portals) << "accepted " << patch;
            EXPECT_NE(portals.Error().Message().find(expected), std::string::npos)
                << portals.Error().ToString();
        }
    }

    TEST_F(WorldLoaderTest, APortalNotInItsWallIsRefusedAndBothSidesAreChecked)
    {
        // §15.7 rule 4, checked here because the rectangle is what the visibility clip uses every
        // frame: one that is not in the wall it claims does not fail, it produces a frustum that is
        // silently wrong and a room that flickers.
        for (const auto& [patch, expected] : std::vector<std::pair<std::string, std::string>>{
                 // cellA has no face at x = 2.5, and neither does cellB.
                 {R"("plane": { "axis": "x", "value": 2.5 })", "no face"},
                 // In the wall, but the opening runs past the end of the WC's side of it.
                 {R"("rect": { "u": [3.0, 7.0], "v": [0.60, 2.62] })", "not inside any run"},
                 // In the wall and in the run, but taller than the room.
                 {R"("rect": { "u": [4.6, 5.5], "v": [0.60, 9.00] })", "vertical extent"}})
        {
            world::WorldData::Contents contents;
            Write("layout.levels.json", Levels());
            Write("layout.cells.json", Cells());
            Write("layout.portals.json", OnePortal(patch));
            ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, contents));
            ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, contents));

            const auto portals = world::WorldLoader::LoadPortals(directory_, contents);
            ASSERT_FALSE(portals) << "accepted " << patch;
            EXPECT_NE(portals.Error().Message().find(expected), std::string::npos)
                << portals.Error().ToString();
        }
    }

    TEST_F(WorldLoaderTest, APortalInOneCellsWallAndNotTheOthersIsRefused)
    {
        // Both sides, not the first. The wall is shared, so a rectangle in one cell's face and not
        // the other's is a hole into the middle of a wall.
        world::WorldData::Contents contents;
        Write("layout.levels.json", Levels());
        Write("layout.cells.json", Cells());
        // The WC's face on x = 2 runs z 4..6; the hall's runs z 4..10. u = [6.5, 7.0] is inside the
        // hall's run and outside the WC's.
        Write("layout.portals.json", OnePortal(R"("rect": { "u": [6.5, 7.0], "v": [0.60, 2.62] })"));
        ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, contents));
        ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, contents));

        const auto portals = world::WorldLoader::LoadPortals(directory_, contents);
        ASSERT_FALSE(portals);
        EXPECT_NE(portals.Error().Message().find("L0_WC1"), std::string::npos)
            << "the message must name the side that fails: " << portals.Error().ToString();
    }

    TEST_F(WorldLoaderTest, APortalIntoItsOwnCellIsRefused)
    {
        world::WorldData::Contents contents;
        Write("layout.levels.json", Levels());
        Write("layout.cells.json", Cells());
        Write("layout.portals.json", OnePortal(R"("cellB": "L0_HALL")"));
        ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, contents));
        ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, contents));

        const auto portals = world::WorldLoader::LoadPortals(directory_, contents);
        ASSERT_FALSE(portals);
        EXPECT_NE(portals.Error().Message().find("itself"), std::string::npos) << portals.Error().ToString();
    }

    TEST_F(WorldLoaderTest, APortalNamingACellThatDoesNotExistIsRuleSixAndNotThisCheck)
    {
        // The plane check has nothing to compare against and must not invent a second, worse
        // message for a dangling reference that `WorldValidator` reports precisely.
        world::WorldData::Contents contents;
        Write("layout.levels.json", Levels());
        Write("layout.cells.json", Cells());
        Write("layout.portals.json", OnePortal(R"("cellB": "L0_NOWHERE")"));
        ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, contents));
        ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, contents));

        const auto portals = world::WorldLoader::LoadPortals(directory_, contents);
        ASSERT_TRUE(portals) << portals.Error().ToString();
        ASSERT_EQ(contents.portals.size(), 1U);
        EXPECT_EQ(contents.portals[0].cellB, Intern("L0_NOWHERE"));
    }

    TEST_F(WorldLoaderTest, AnInvertedOrNegativeRangeIsRefused)
    {
        for (const std::string patch : {R"("rect": { "u": [5.5, 4.6], "v": [0.60, 2.62] })",
                                        R"("rect": { "u": [4.6, 5.5], "v": [2.62, 2.62] })",
                                        R"("maxDepth": -1)"})
        {
            world::WorldData::Contents contents;
            Write("layout.levels.json", Levels());
            Write("layout.cells.json", Cells());
            Write("layout.portals.json", OnePortal(patch));
            ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, contents));
            ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, contents));
            EXPECT_FALSE(world::WorldLoader::LoadPortals(directory_, contents)) << "accepted " << patch;
        }
    }

    TEST_F(WorldLoaderTest, APortalKindOrOpacityOutsideItsVocabularyIsRefused)
    {
        for (const std::string patch : {R"("kind": "portcullis")", R"("opacity": "frosted")"})
        {
            world::WorldData::Contents contents;
            Write("layout.levels.json", Levels());
            Write("layout.cells.json", Cells());
            Write("layout.portals.json", OnePortal(patch));
            ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, contents));
            ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, contents));
            EXPECT_FALSE(world::WorldLoader::LoadPortals(directory_, contents)) << "accepted " << patch;
        }
    }

    TEST_F(WorldLoaderTest, PortalsAreGroupedUnderBothCellsOnceTheModelIsBuilt)
    {
        world::WorldData::Contents contents;
        ASSERT_TRUE(LoadUpToPortals(contents));
        const auto loaded = world::WorldData::Create(std::move(contents));
        ASSERT_TRUE(loaded) << loaded.Error().ToString();

        EXPECT_EQ(loaded.Value().PortalsOf(Intern("L0_HALL")).size(), 3U);
        EXPECT_EQ(loaded.Value().PortalsOf(Intern("L0_WC1")).size(), 2U)
            << "the door and the glazed panel beside it";
        const world::Portal* door = loaded.Value().FindPortal(Intern("P_HALL__WC1"));
        ASSERT_NE(door, nullptr);
        EXPECT_EQ(loaded.Value().OtherSide(*door, Intern("L0_WC1")), Intern("L0_HALL"));
    }

    // --- the openings -----------------------------------------------------------------------

    TEST_F(WorldLoaderTest, AnOpeningIsReadWithEveryFieldItCarries)
    {
        Write("layout.openings.json", Openings());
        world::WorldData::Contents contents;
        const auto openings = world::WorldLoader::LoadOpenings(directory_, contents);
        ASSERT_TRUE(openings) << openings.Error().ToString();

        ASSERT_EQ(contents.openings.size(), 3U);
        const world::Opening& door = contents.openings[0];
        EXPECT_EQ(door.id, Intern("DOOR_L0_WC1"));
        EXPECT_EQ(door.kind, world::OpeningKind::Door);
        EXPECT_EQ(door.portal, Intern("P_HALL__WC1"));
        EXPECT_FLOAT_EQ(door.leaf.width, 0.86F);
        EXPECT_FLOAT_EQ(door.leaf.height, 2.04F);
        EXPECT_FLOAT_EQ(door.leaf.thickness, 0.040F);
        ASSERT_TRUE(door.hinge.has_value());
        EXPECT_EQ(*door.hinge, world::HingeSide::Left);
        EXPECT_EQ(door.swing, "into_L0_WC1");
        EXPECT_FLOAT_EQ(door.maxAngleDeg, 95.0F);
        EXPECT_EQ(door.frameAsset, Intern("MODEL_DOOR_FRAME_INT_01"));
        EXPECT_FLOAT_EQ(door.casing, 0.070F);
        EXPECT_EQ(door.asset, Intern("MODEL_DOOR_LEAF_PANEL_01"));
        EXPECT_EQ(door.material, Intern("MAT_PAINT_TRIM_WHITE"));
        EXPECT_FALSE(door.solid);
        EXPECT_FALSE(door.lockable);
    }

    TEST_F(WorldLoaderTest, ANullHingeIsNotHingedAndNotHingedLeft)
    {
        // A slider does not swing, and `Left` is the first value of the enum -- so a reader that
        // ignored the field would look identical on a door and wrong on every slider in the house.
        Write("layout.openings.json", Openings());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadOpenings(directory_, contents));

        const world::Opening& slider = contents.openings[1];
        EXPECT_EQ(slider.kind, world::OpeningKind::Window);
        EXPECT_FALSE(slider.hinge.has_value());
        EXPECT_EQ(slider.swing, "");
        // And the door beside it hinges RIGHT, not left, so the value is read and not defaulted.
        EXPECT_TRUE(contents.openings[2].hinge.has_value());
        EXPECT_EQ(*contents.openings[2].hinge, world::HingeSide::Right);
    }

    TEST_F(WorldLoaderTest, ASolidLeafIsReadBecauseTheAudioSolveReadsItAndNotTheAsset)
    {
        // §64.3: a hollow-core door is 16 dB closed and a solid one 24. The difference lives here,
        // in the layout, not in the `.glb`.
        Write("layout.openings.json", Openings());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadOpenings(directory_, contents));

        EXPECT_FALSE(contents.openings[0].solid);
        EXPECT_TRUE(contents.openings[2].solid);
        EXPECT_TRUE(contents.openings[2].lockable);
    }

    TEST_F(WorldLoaderTest, ALeafWithNoSizeIsRefused)
    {
        for (const std::string row :
             {R"({"id": "D", "kind": "door", "portal": "P", "leaf": {"height": 2.04}})",
              R"({"id": "D", "kind": "door", "portal": "P", "leaf": {"width": 0.0, "height": 2.04}})",
              R"({"id": "D", "kind": "door", "portal": "P",
                  "leaf": {"width": 0.86, "height": -2.04}})"})
        {
            Write("layout.openings.json", R"({"schema": "cna-house/openings/1", "openings": [)" + row + "]}");
            world::WorldData::Contents contents;
            const auto openings = world::WorldLoader::LoadOpenings(directory_, contents);
            ASSERT_FALSE(openings) << "accepted " << row;
            // Absent and zero are refused by different rules -- one required, one positive -- and
            // both messages must name the dimension, because "a leaf is wrong" sends nobody
            // anywhere.
            EXPECT_TRUE(openings.Error().Context().find("width") != std::string::npos ||
                        openings.Error().Context().find("height") != std::string::npos)
                << openings.Error().ToString();
        }
    }

    TEST_F(WorldLoaderTest, ALeafThatOpensPastAHalfTurnIsRefused)
    {
        Write("layout.openings.json",
              R"({"schema": "cna-house/openings/1",
                  "openings": [{"id": "D", "kind": "door", "portal": "P",
                                "leaf": {"width": 0.86, "height": 2.04},
                                "maxAngleDeg": 270.0}]})");
        world::WorldData::Contents contents;
        const auto openings = world::WorldLoader::LoadOpenings(directory_, contents);
        ASSERT_FALSE(openings);
        EXPECT_EQ(openings.Error().Code(), ErrorCode::OutOfRange);
    }

    TEST_F(WorldLoaderTest, TheOpeningToPortalBijectionIsNotTheLoadersToCheck)
    {
        // §15.7 rule 7 is a statement about two whole files -- every door has exactly one portal
        // AND no portal has two leaves -- and the loader has read one of them. `validate_world.py`
        // and `WorldValidator` own it; a partial check here would report the wrong half.
        Write("layout.openings.json",
              R"({"schema": "cna-house/openings/1",
                  "openings": [{"id": "D", "kind": "door", "portal": "P_DOES_NOT_EXIST",
                                "leaf": {"width": 0.86, "height": 2.04}}]})");
        world::WorldData::Contents contents;
        const auto openings = world::WorldLoader::LoadOpenings(directory_, contents);
        ASSERT_TRUE(openings) << openings.Error().ToString();
        ASSERT_EQ(contents.openings.size(), 1U);
        EXPECT_EQ(contents.openings[0].portal, Intern("P_DOES_NOT_EXIST"));
    }

    // --- the stairs -------------------------------------------------------------------------

    TEST_F(WorldLoaderTest, AFlightIsReadWithItsLandings)
    {
        Write("layout.stairs.json", Stairs());
        world::WorldData::Contents contents;
        const auto stairs = world::WorldLoader::LoadStairs(directory_, contents);
        ASSERT_TRUE(stairs) << stairs.Error().ToString();

        ASSERT_EQ(contents.stairs.size(), 2U);
        const world::StairFlight& main = contents.stairs[0];
        EXPECT_EQ(main.id, Intern("STAIR_L0_L1_MAIN"));
        EXPECT_EQ(main.fromCell, Intern("L0_STAIR"));
        EXPECT_EQ(main.toCell, Intern("L1_LANDING"));
        EXPECT_EQ(main.risers, 17);
        EXPECT_FLOAT_EQ(main.going, 0.280F);
        EXPECT_FLOAT_EQ(main.width, 1.20F);
        ASSERT_EQ(main.landings.size(), 1U);
        EXPECT_EQ(main.landings[0].at, 9);
        EXPECT_FLOAT_EQ(main.landings[0].depth, 1.20F);
        EXPECT_TRUE(main.collisionRamp);
        EXPECT_EQ(main.surface, "wood");
    }

    TEST_F(WorldLoaderTest, CollisionRampDefaultsToTrue)
    {
        // The default `build_collision.py` uses. Defaulting the other way would silently give
        // every flight in the house a box per step where it asked for two wedges.
        Write("layout.stairs.json", Stairs());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadStairs(directory_, contents));
        EXPECT_TRUE(contents.stairs[1].collisionRamp) << "the second flight states nothing";
    }

    TEST_F(WorldLoaderTest, AFlightSegmentsIntoTheRunsAndLandingsTheContentBuildBakes)
    {
        // The same walk `build_collision.py` does: consume risers until the next landing, emit the
        // run, emit the landing, repeat. Deriving it in one place from the authored row is what
        // stops the runtime and the baked wedges disagreeing about where a flight turns.
        world::StairFlight flight;
        flight.risers = 17;
        flight.rise = 3.05F / 17.0F;
        flight.going = 0.280F;
        flight.landings.push_back(world::Landing{9, 1.20F});

        const auto segments = world::SegmentFlight(flight);
        ASSERT_EQ(segments.size(), 3U);

        EXPECT_FALSE(segments[0].isLanding);
        EXPECT_EQ(segments[0].fromRiser, 0);
        EXPECT_EQ(segments[0].risers, 9);
        EXPECT_NEAR(segments[0].length, 9 * 0.280F, 1e-5F);

        EXPECT_TRUE(segments[1].isLanding);
        EXPECT_EQ(segments[1].fromRiser, 9);
        EXPECT_FLOAT_EQ(segments[1].length, 1.20F);
        EXPECT_FLOAT_EQ(segments[1].height, 0.0F) << "a landing is flat";

        EXPECT_FALSE(segments[2].isLanding);
        EXPECT_EQ(segments[2].risers, 8);

        // Every riser is accounted for exactly once, which is the property that matters: a
        // segmentation that lost one would put the top of the flight below the floor it reaches.
        std::int32_t climbed = 0;
        float height = 0.0F;
        for (const auto& segment : segments)
        {
            climbed += segment.risers;
            height += segment.height;
        }
        EXPECT_EQ(climbed, flight.risers);
        EXPECT_NEAR(height, flight.Climb(), 1e-4F);
        EXPECT_NEAR(world::TotalRun(flight), 17 * 0.280F + 1.20F, 1e-5F);
    }

    TEST_F(WorldLoaderTest, AFlightWithNoLandingIsOneRun)
    {
        // `build_collision.py`'s own selftest fixture: 8 risers, 0.18 rise, 0.28 going, no
        // landings. One wedge, and this must agree with it.
        world::StairFlight flight;
        flight.risers = 8;
        flight.rise = 0.18F;
        flight.going = 0.28F;

        const auto segments = world::SegmentFlight(flight);
        ASSERT_EQ(segments.size(), 1U);
        EXPECT_EQ(segments[0].risers, 8);
        EXPECT_NEAR(segments[0].length, 8 * 0.28F, 1e-5F);
        EXPECT_NEAR(segments[0].height, 8 * 0.18F, 1e-5F);
    }

    TEST_F(WorldLoaderTest, ALandingOnTheRiserARunStartsOnDoesNotProduceAZeroLengthWedge)
    {
        // Without the clamp this loops for ever on the same step, or emits a wedge with no
        // length. A landing at riser 0 is the bottom of the flight, not a turn in the middle.
        world::StairFlight flight;
        flight.risers = 4;
        flight.rise = 0.18F;
        flight.going = 0.28F;
        flight.landings.push_back(world::Landing{0, 1.0F});

        const auto segments = world::SegmentFlight(flight);
        ASSERT_FALSE(segments.empty());
        for (const auto& segment : segments)
        {
            EXPECT_GT(segment.length, 0.0F);
        }
        std::int32_t climbed = 0;
        for (const auto& segment : segments)
        {
            climbed += segment.risers;
        }
        EXPECT_EQ(climbed, 4);
    }

    TEST_F(WorldLoaderTest, AFlightWithNoRisersOrNegativeDimensionsIsRefused)
    {
        for (const std::string row :
             {R"({"id": "S", "fromCell": "A", "toCell": "B", "risers": 0, "rise": 0.18,
                  "going": 0.28, "width": 1.0})",
              R"({"id": "S", "fromCell": "A", "toCell": "B", "risers": 8, "rise": -0.18,
                  "going": 0.28, "width": 1.0})",
              R"({"id": "S", "fromCell": "A", "toCell": "B", "risers": 8, "rise": 0.18,
                  "going": 0.0, "width": 1.0})"})
        {
            Write("layout.stairs.json", R"({"schema": "cna-house/stairs/1", "flights": [)" + row + "]}");
            world::WorldData::Contents contents;
            EXPECT_FALSE(world::WorldLoader::LoadStairs(directory_, contents)) << "accepted " << row;
        }
    }

    TEST_F(WorldLoaderTest, ALandingBeyondTheTopOfItsFlightIsRefused)
    {
        Write("layout.stairs.json",
              R"({"schema": "cna-house/stairs/1",
                  "flights": [{"id": "S", "fromCell": "A", "toCell": "B", "risers": 8,
                               "rise": 0.18, "going": 0.28, "width": 1.0,
                               "landings": [{"at": 20, "depth": 1.2}]}]})");
        world::WorldData::Contents contents;
        const auto stairs = world::WorldLoader::LoadStairs(directory_, contents);
        ASSERT_FALSE(stairs);
        EXPECT_EQ(stairs.Error().Code(), ErrorCode::OutOfRange);
    }

    // --- the whole load -----------------------------------------------------------------------

    TEST_F(WorldLoaderTest, LoadProducesAWorldDataWithItsIndicesBuilt)
    {
        WriteWorldWithMaterials();
        const auto world = world::WorldLoader::Load(directory_);
        ASSERT_TRUE(world) << world.Error().ToString();

        EXPECT_EQ(world.Value().Levels().size(), 3U);
        ASSERT_NE(world.Value().FindLevel(Intern("L1")), nullptr);
        EXPECT_FLOAT_EQ(world.Value().FindLevel(Intern("L1"))->ffl, 3.65F);
        EXPECT_NE(world.Value().FindPlumbingStack(Intern("STACK_A")), nullptr);
        EXPECT_FLOAT_EQ(world.Value().GetConstruction().roofPitch, 0.594F);
        EXPECT_EQ(world.Value().Materials().size(), 5U);
        ASSERT_NE(world.Value().FindMaterial(Intern("MAT_LEAF")), nullptr);
        EXPECT_EQ(world.Value().FindMaterial(Intern("MAT_LEAF"))->effectTierS, world::EffectTier::AlphaTest);
        EXPECT_EQ(world.Value().Cells().size(), 6U);
        EXPECT_NE(world.Value().FindCell(Intern("L0_HALL")), nullptr);
        EXPECT_EQ(world.Value().Portals().size(), 4U);
        EXPECT_EQ(world.Value().Openings().size(), 3U);
        EXPECT_EQ(world.Value().Stairs().size(), 2U);
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
        Write("layout.materials.json", Materials());
        Write("layout.cells.json", Cells());
        Write("layout.portals.json", Portals());
        Write("layout.openings.json", Openings());
        Write("layout.stairs.json", Stairs());
        WriteManifest({"layout.levels.json",
                       "layout.materials.json",
                       "layout.cells.json",
                       "layout.portals.json",
                       "layout.openings.json",
                       "layout.stairs.json"});

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
