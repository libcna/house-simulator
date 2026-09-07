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
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <variant>
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

        /// A manifest listing exactly the files the test has written, with REAL hashes.
        ///
        /// Real, because `Load` verifies them (`HOUSE-00364`). Computed with the code under test,
        /// which would be circular on its own -- so `TheWorldHashMatchesTheDefinitionByteForByte`
        /// pins the definition against a literal the Python writer produced, and that is what
        /// makes the two implementations one definition rather than two that happen to agree.
        void WriteManifest(const std::vector<std::string>& members) const
        {
            std::vector<world::WorldManifest::Member> rows;
            for (const std::string& file : members)
            {
                const auto hash = world::WorldLoader::HashFile(directory_ + "/" + file);
                rows.push_back({file, hash ? hash.Value() : std::string("sha256:")});
            }

            std::string text = R"({"schema": "cna-house/manifest/1", "worldHash": ")" +
                               world::WorldLoader::ComputeWorldHash(rows) + R"(", "members": [)";
            for (std::size_t index = 0; index < rows.size(); ++index)
            {
                if (index != 0)
                {
                    text += ',';
                }
                text +=
                    R"({"file": ")" + rows[index].file + R"(", "sha256": ")" + rows[index].sha256 + R"("})";
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
                  "id": "DOOR_L0_WC1", "kind": "door", "type": "D_INT_PRIVACY",
                  "portal": "P_HALL__WC1",
                  "leaf": { "width": 0.86, "height": 2.04, "thickness": 0.040 },
                  "hinge": "left", "swing": "L0_WC1", "maxAngleDeg": 95.0,
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
                  "hinge": "right", "swing": "L0_HALL", "maxAngleDeg": 90.0,
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

        /// Four lights: a point, a spot with a cone and a fixture, and two in one switch group
        /// that spans two cells -- because a group crossing cells is what the group index is for.
        static std::string Lights()
        {
            return R"({
              "schema": "cna-house/lights/1",
              "lights": [
                { "id": "LIGHT_L0_KITCHEN_MAIN", "cell": "L0_KITCHEN",
                  "group": "LG_L0_KITCHEN_MAIN", "type": "point",
                  "position": [-3.0, 3.10, -25.0], "colorK": 2700, "intensityLm": 1600.0,
                  "range": 6.0, "defaultOn": true },
                { "id": "LIGHT_L0_KITCHEN_SINK", "cell": "L0_KITCHEN",
                  "group": "LG_L0_KITCHEN_SINK", "type": "spot",
                  "position": [-4.0, 3.10, -26.0], "direction": [0.20, -0.90, 0.40],
                  "colorK": 3000, "intensityLm": 420.0, "range": 4.0,
                  "coneInnerDeg": 22.0, "coneOuterDeg": 38.0,
                  "fixtureProp": "PROP_L0_KITCHEN_DOWNLIGHT",
                  "castsBlobShadow": true, "bakedIntoLightmap": true, "defaultOn": true },
                { "id": "LIGHT_L0_STAIR_LOW", "cell": "L0_STAIR", "group": "LG_L0_STAIR",
                  "type": "point", "position": [-4.0, 3.00, 6.0], "colorK": 2700,
                  "intensityLm": 800.0, "bakedIntoLightmap": true, "castsBlobShadow": false,
                  "defaultOn": true },
                { "id": "LIGHT_L1_STAIR_HIGH", "cell": "L1_LANDING", "group": "LG_L0_STAIR",
                  "type": "point", "position": [-4.0, 6.00, 6.0], "colorK": 2700,
                  "intensityLm": 800.0, "defaultOn": false }
              ]
            })";
        }

        /// Three placements: a dynamic appliance, a static worktop that states almost nothing,
        /// and a WC pan that drains to a §12.5 stack.
        static std::string Props()
        {
            return R"({
              "schema": "cna-house/props/1",
              "props": [
                {
                  "id": "PROP_L0_KITCHEN_FRIDGE", "asset": "MODEL_PROP_KITCHEN_FRIDGE_01",
                  "cell": "L0_KITCHEN",
                  "position": [1.20, 0.60, -26.70], "yawDeg": 180.0, "scale": 1.0,
                  "static": false, "lodGroup": "LODG_APPLIANCE", "collision": "proxy",
                  "material": null, "interactable": "FRIDGE_L0_KITCHEN"
                },
                {
                  "id": "PROP_L0_KITCHEN_WORKTOP", "asset": "MODEL_PROP_WORKTOP_01",
                  "cell": "L0_KITCHEN", "position": [-2.00, 0.60, -26.90]
                },
                {
                  "id": "PROP_L0_WC1_PAN", "asset": "MODEL_PROP_WC_PAN_01", "cell": "L0_WC1",
                  "position": [3.00, 0.60, 5.00], "yawDeg": 90.0, "scale": 0.98,
                  "collision": "box", "plumbing": "STACK_A"
                }
              ]
            })";
        }

        /// Three nodes, two edges -- one through a door and one inside a room -- and one of each
        /// marker kind, with the perch reserved for the cat.
        static std::string Nav()
        {
            return R"({
              "schema": "cna-house/nav/1",
              "nodes": [
                { "id": "NAV_L0_KITCHEN_C", "cell": "L0_KITCHEN",
                  "position": [-3.0, 0.60, -25.0], "kind": "floor" },
                { "id": "NAV_L0_HALL_S", "cell": "L0_HALL", "position": [0.0, 0.60, 5.0],
                  "kind": "floor" },
                { "id": "NAV_L0_HALL_N", "cell": "L0_HALL", "position": [0.0, 0.60, 9.0],
                  "kind": "floor" }
              ],
              "edges": [
                { "a": "NAV_L0_KITCHEN_C", "b": "NAV_L0_HALL_S", "portal": "P_HALL__WC1",
                  "cost": 4.2, "species": ["dog", "cat"] },
                { "a": "NAV_L0_HALL_S", "b": "NAV_L0_HALL_N", "cost": 4.0 }
              ],
              "perches": [
                { "id": "PERCH_L1_WINDOWSEAT", "cell": "L1_LANDING",
                  "position": [0.4, 4.10, -15.1], "species": ["cat"] }
              ],
              "beds": [
                { "id": "BED_DOG_FAMILY", "cell": "L0_HALL", "prop": "PROP_DOG_BED",
                  "species": ["dog"] }
              ],
              "bowls": [
                { "id": "BOWL_WATER", "cell": "L0_KITCHEN", "position": [0.0, 0.60, -24.0] }
              ],
              "forbidden": [ { "cell": "L0_GARAGE", "species": ["cat"] } ]
            })";
        }

        /// Two zones (one with no ambience bed), one emitter, and §64.3's two door rows.
        static std::string Audio()
        {
            return R"({
              "schema": "cna-house/audio/1",
              "zones": [
                { "id": "AZ_L0_KITCHEN", "cell": "L0_KITCHEN", "bed": "AMB_KITCHEN",
                  "gain": 0.55 },
                { "id": "AZ_L0_STAIR", "cell": "L0_STAIR", "bed": null, "gain": 0.30 }
              ],
              "emitters": [
                { "id": "EM_FRIDGE_HUM", "cell": "L0_KITCHEN", "position": [1.20, 0.90, -26.70],
                  "loop": "AMB_FRIDGE_HUM", "gain": 0.35, "radius": 4.0,
                  "interactable": "FRIDGE_L0_KITCHEN" }
              ],
              "transmission": {
                "door_hollow": { "open": 0.05, "closed": 0.55 },
                "door_solid":  { "open": 0.05, "closed": 0.78 }
              }
            })";
        }

        /// Terrain, a three-point road, one fence with a gate, two neighbour buildings (one of
        /// them always an impostor) and two vegetation groups.
        static std::string Exterior()
        {
            return R"({
              "schema": "cna-house/exterior/1",
              "terrain": {
                "heightfield": "Textures/Terrain/plot-height",
                "size": [96.0, 128.0], "origin": [-48.0, 0.0, -64.0], "yScale": 12.5,
                "material": "MAT_GRASS_LAWN"
              },
              "road": {
                "centreline": [[-40.0, 0.0, 30.0], [0.0, 0.0, 30.0], [40.0, 0.0, 31.0]],
                "width": 6.5, "material": "MAT_ASPHALT"
              },
              "fences": [
                { "id": "FENCE_REAR", "asset": "MODEL_FENCE_PANEL_01",
                  "path": [[-20.0, 0.0, -40.0], [20.0, 0.0, -40.0], [20.0, 0.0, -10.0]],
                  "height": 1.80, "gate": "GATE_REAR" }
              ],
              "neighbourhood": [
                { "id": "NB_EAST", "asset": "MODEL_HOUSE_NEIGHBOUR_01",
                  "position": [30.0, 0.0, -10.0], "yawDeg": 90.0,
                  "lodGroup": "LODG_NEIGHBOUR", "impostorFrom": 45.0 },
                { "id": "NB_FAR", "asset": "MODEL_HOUSE_NEIGHBOUR_02",
                  "position": [80.0, 0.0, 60.0], "impostorFrom": 0.0 }
              ],
              "vegetation": [
                { "id": "VEG_BIRCH", "asset": "MODEL_TREE_BIRCH_01", "instances": [
                    { "position": [-12.0, 0.0, 10.0], "yawDeg": 15.0, "scale": 0.90 },
                    { "position": [-9.0, 0.0, 14.0], "yawDeg": 200.0, "scale": 1.15 },
                    { "position": [-14.0, 0.0, 18.0] } ] },
                { "id": "VEG_HEDGE", "asset": "MODEL_HEDGE_01", "instances": [
                    { "position": [0.0, 0.0, 26.0], "scale": 1.05 } ] }
              ]
            })";
        }

        /// The refrigerator of `world-format.md`'s example, and a light switch that states no
        /// `when` at all -- the case most of the 640 rows are.
        static std::string Interactables()
        {
            return R"JSON({
              "schema": "cna-house/interactables/1",
              "interactables": [
                {
                  "id": "FRIDGE_L0_KITCHEN", "kind": "refrigerator", "cell": "L0_KITCHEN",
                  "prop": "PROP_L0_KITCHEN_FRIDGE",
                  "focus":  { "point": [1.20, 1.40, -26.40], "normal": [0, 0, 1],
                              "radius": 0.85 },
                  "bounds": { "min": [0.30, 0.60, -27.05], "max": [2.10, 2.55, -26.35] },
                  "state":  { "doorOpen": false, "temperatureC": 4.0, "programme": "eco" },
                  "actions": [
                    { "verb": "Open",  "when": "state.doorOpen == false",
                      "do": "state.doorOpen = true",
                      "sound": "SFX_FRIDGE_OPEN",  "anim": "door", "duration": 0.9 },
                    { "verb": "Close", "when": "state.doorOpen == true",
                      "do": "state.doorOpen = false",
                      "sound": "SFX_FRIDGE_CLOSE", "anim": "door", "duration": 0.7 }
                  ],
                  "childInteractables": ["FRIDGE_ITEM_MILK_1"],
                  "persist": ["doorOpen"],
                  "audio":  { "loop": "AMB_FRIDGE_HUM", "emitter": [1.20, 0.90, -26.70] },
                  "portal": "P_FRIDGE_INTERIOR"
                },
                {
                  "id": "SWITCH_L0_HALL", "kind": "light_switch", "cell": "L0_HALL",
                  "focus": { "point": [1.90, 1.20, 5.00], "normal": [-1, 0, 0], "radius": 0.05 },
                  "state": { "on": false },
                  "actions": [ { "verb": "Toggle", "do": "toggle(state.on)",
                                 "sound": "SFX_SWITCH" } ]
                }
              ]
            })JSON";
        }

        /// The fridge's `Open` action with one field replaced, for the expression cases.
        static std::string OneInteractable(const std::string& patch)
        {
            const std::size_t colon = patch.find(':');
            const std::string key = patch.substr(0, colon);
            std::vector<std::pair<std::string, std::string>> action{
                {"\"verb\"", "\"Open\""},
                {"\"when\"", "\"state.doorOpen == false\""},
                {"\"do\"", "\"state.doorOpen = true\""},
            };
            bool replaced = false;
            for (auto& field : action)
            {
                if (field.first == key)
                {
                    field.second = patch.substr(colon + 1);
                    replaced = true;
                }
            }
            if (!replaced)
            {
                ADD_FAILURE() << "the Open action has no field " << key;
            }

            std::string body;
            for (std::size_t index = 0; index < action.size(); ++index)
            {
                if (index != 0)
                {
                    body += ',';
                }
                body += action[index].first + ':' + action[index].second;
            }
            return R"({"schema": "cna-house/interactables/1",
                       "interactables": [{"id": "FRIDGE_L0_KITCHEN", "kind": "refrigerator",
                                          "cell": "L0_KITCHEN",
                                          "focus": {"point": [1.2, 1.4, -26.4]},
                                          "state": {"doorOpen": false},
                                          "actions": [{)" +
                   body + "}]}]}";
        }

        /// `world-format.md`'s example, with the fridge starting colder than it declares.
        static std::string InitialState()
        {
            return R"({
              "schema": "cna-house/initialstate/1",
              "player":  { "cell": "L0_FOYER", "position": [0.0, 0.60, -18.4], "yawDeg": 90.0 },
              "clock":   { "epochSeconds": 21600.0, "timeScale": 60.0,
                           "latitudeDeg": 40.05, "longitudeDeg": -75.30,
                           "utcOffsetMinutes": -300 },
              "weather": { "target": "W_PARTLY", "cloudCover": 0.35, "windSpeed": 2.4 },
              "interactables": { "FRIDGE_L0_KITCHEN": { "temperatureC": 2.5 } },
              "pets":    { "PET_DOG": { "cell": "L0_FAMILY", "state": "Lie" } }
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
            Write("layout.lights.json", Lights());
            Write("layout.props.json", Props());
            Write("layout.nav.json", Nav());
            Write("layout.audio.json", Audio());
            Write("layout.exterior.json", Exterior());
            Write("interactables.json", Interactables());
            Write("initialstate.json", InitialState());
            WriteManifest({"layout.levels.json",
                           "layout.materials.json",
                           "layout.cells.json",
                           "layout.portals.json",
                           "layout.openings.json",
                           "layout.stairs.json",
                           "layout.lights.json",
                           "layout.props.json",
                           "layout.nav.json",
                           "layout.audio.json",
                           "layout.exterior.json",
                           "interactables.json",
                           "initialstate.json"});
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
        EXPECT_EQ(manifest.Value().worldHash,
                  world::WorldLoader::ComputeWorldHash(
                      std::vector<world::WorldManifest::Member>{manifest.Value().members[0]}));
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

    TEST_F(WorldLoaderTest, TheWorldHashMatchesTheDefinitionByteForByte)
    {
        // The cross-implementation check, and the reason this task has two halves. The literal is
        // what `tools/world/world_manifest.py` produces for the same two members; if the C++ and
        // the Python ever drift, every save written by one is stale to the other.
        const std::vector<world::WorldManifest::Member> members{
            {"layout.levels.json", "sha256:" + std::string(64, '0')},
            {"layout.cells.json", "sha256:" + std::string(64, 'a')},
        };
        EXPECT_EQ(world::WorldLoader::ComputeWorldHash(members),
                  "sha256:d8e3c37a0b0e853ce670d6020e6889f2d50c3209b0bd672bb76a7f525145191d");

        // And a file hash is of the BYTES: the well-known SHA-256 of nothing at all.
        Write("empty.json", "");
        const auto empty = world::WorldLoader::HashFile(directory_ + "/empty.json");
        ASSERT_TRUE(empty) << empty.Error().ToString();
        EXPECT_EQ(empty.Value(), "sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    }

    TEST_F(WorldLoaderTest, AMemberWhoseBytesChangedIsALoadError)
    {
        // §15.1: a member whose hash does not match is a load-time error. The two ways to get here
        // are an edit without regenerating the manifest and a file that arrived corrupt, so the
        // message carries both hashes and the command that fixes the first.
        WriteWorldWithMaterials();
        Write("layout.cells.json", Cells() + "\n");

        const auto loaded = world::WorldLoader::Load(directory_);
        ASSERT_FALSE(loaded);
        EXPECT_EQ(loaded.Error().Code(), ErrorCode::ChecksumMismatch);
        EXPECT_NE(loaded.Error().Message().find("layout.cells.json"), std::string::npos)
            << loaded.Error().ToString();
        EXPECT_NE(loaded.Error().Message().find("world_manifest.py"), std::string::npos)
            << "and say how to fix it: " << loaded.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AWorldHashThatDoesNotCoverItsMemberListIsALoadError)
    {
        // A save compares this number to decide whether the world moved under it, so a wrong one
        // is a decision made on nothing.
        Write("layout.levels.json", Levels());
        const auto hash = world::WorldLoader::HashFile(directory_ + "/layout.levels.json");
        ASSERT_TRUE(hash);
        Write("world.manifest.json",
              R"({"schema": "cna-house/manifest/1", "worldHash": "sha256:)" + std::string(64, 'f') +
                  R"(", "members": [{"file": "layout.levels.json", "sha256": ")" + hash.Value() + R"("}]})");

        const auto manifest = world::WorldLoader::LoadManifest(directory_);
        ASSERT_TRUE(manifest) << manifest.Error().ToString();
        const auto verified = world::WorldLoader::VerifyManifest(directory_, manifest.Value());
        ASSERT_FALSE(verified);
        EXPECT_EQ(verified.Error().Code(), ErrorCode::ChecksumMismatch);
        EXPECT_NE(verified.Error().Context().find("worldHash"), std::string::npos)
            << verified.Error().ToString();
    }

    TEST_F(WorldLoaderTest, TheWorldHashDependsOnTheOrderOfTheMemberList)
    {
        // The load order is part of what a save was taken against, so the same files in another
        // order are a different world.
        const std::vector<world::WorldManifest::Member> forward{
            {"a.json", "sha256:" + std::string(64, '1')},
            {"b.json", "sha256:" + std::string(64, '2')},
        };
        const std::vector<world::WorldManifest::Member> backward{forward[1], forward[0]};
        EXPECT_NE(world::WorldLoader::ComputeWorldHash(forward),
                  world::WorldLoader::ComputeWorldHash(backward));
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

    TEST_F(WorldLoaderTest, AWindowInTheWallIsAcceptedAndAGapIsNot)
    {
        // §15.7 rule 4's wall case (`HOUSE-00376`). Two rooms either side of a partition share a
        // coordinate, but a room and the yard outside it do not: the room stops at the interior
        // face, the yard at the exterior one, and the window is in the 0.30 m between them. The
        // check must widen exactly that far and no further.
        const std::string cells = R"({
          "schema": "cna-house/cells/1",
          "cells": [
            { "id": "L0_HALL", "level": "L0", "kind": "corridor",
              "boxes": [{ "x": [-2.0, 2.0], "z": [-27.10, -14.30] }] },
            { "id": "EXT_YARD", "level": "L0", "kind": "exterior",
              "boxes": [{ "x": [-2.0, 2.0], "z": [-14.00, -4.00] }],
              "yOverride": [0.0, 20.0] }
          ]
        })";
        const auto portal = [](const std::string& plane)
        {
            return R"({
              "schema": "cna-house/portals/1",
              "portals": [
                { "id": "P_HALL__YARD", "cellA": "L0_HALL", "cellB": "EXT_YARD",
                  "plane": )" +
                   plane + R"(,
                  "rect": { "u": [-0.6, 0.6], "v": [1.50, 3.00] },
                  "kind": "window", "opacity": "glass", "aperture": "WIN_HALL_1" }
              ]
            })";
        };

        // On the room's own face, a full wall from the yard's. This is where every window in the
        // house sits, and it is the case binary floating point breaks: -14.0 - -14.3 is
        // 0.30000001, so a `<= 0.30` written without slack rejects all 66 of them.
        const std::vector<std::string> planes{R"({ "axis": "z", "value": -14.30 })",
                                              R"({ "axis": "z", "value": -14.15 })",
                                              R"({ "axis": "z", "value": -14.00 })"};
        for (const std::string& plane : planes)
        {
            world::WorldData::Contents contents;
            Write("layout.levels.json", Levels());
            Write("layout.cells.json", cells);
            Write("layout.portals.json", portal(plane));
            ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, contents));
            ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, contents));
            const auto loaded = world::WorldLoader::LoadPortals(directory_, contents);
            EXPECT_TRUE(loaded) << plane << ": " << (loaded ? "" : loaded.Error().ToString());
        }

        // ...and not one centimetre further. A plane past the room's own face is in open air, not
        // in the wall.
        world::WorldData::Contents contents;
        Write("layout.levels.json", Levels());
        Write("layout.cells.json", cells);
        Write("layout.portals.json", portal(R"({ "axis": "z", "value": -14.60 })"));
        ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, contents));
        ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, contents));
        const auto refused = world::WorldLoader::LoadPortals(directory_, contents);
        ASSERT_FALSE(refused);
        EXPECT_NE(refused.Error().Message().find("no face"), std::string::npos) << refused.Error().ToString();
    }

    TEST_F(WorldLoaderTest, APortalIntoASubCellIsCheckedAgainstTheSubCellsOwnFace)
    {
        // A container or a mezzanine is inside its parent, so the opening is in the CHILD's face
        // -- the fridge door, the chest lid, the garage loft hatch -- and the parent has no face
        // there at all (`HOUSE-00373`, `HOUSE-00377`). Checking the parent would refuse every one.
        const std::string cells = R"({
          "schema": "cna-house/cells/1",
          "cells": [
            { "id": "L0_HALL", "level": "L0", "kind": "corridor",
              "boxes": [{ "x": [-2.0, 2.0], "z": [4.0, 10.0] }] },
            { "id": "CELL_FRIDGE", "level": "L0", "kind": "closet", "parent": "L0_HALL",
              "boxes": [{ "x": [0.0, 1.0], "z": [5.0, 6.0] }],
              "yOverride": [0.70, 2.45] }
          ]
        })";
        const auto portal = [](const std::string& plane, const std::string& rect)
        {
            return R"({
              "schema": "cna-house/portals/1",
              "portals": [
                { "id": "P_HALL__FRIDGE", "cellA": "L0_HALL", "cellB": "CELL_FRIDGE",
                  "plane": )" +
                   plane + R"(, "rect": )" + rect + R"(,
                  "kind": "door", "opacity": "opaque_when_closed", "aperture": "FRIDGE_L0_HALL" }
              ]
            })";
        };

        world::WorldData::Contents contents;
        Write("layout.levels.json", Levels());
        Write("layout.cells.json", cells);
        Write("layout.portals.json",
              portal(R"({ "axis": "z", "value": 6.0 })", R"({ "u": [0.0, 1.0], "v": [0.70, 2.45] })"));
        ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, contents));
        ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, contents));
        const auto accepted = world::WorldLoader::LoadPortals(directory_, contents);
        EXPECT_TRUE(accepted) << (accepted ? "" : accepted.Error().ToString());

        // ...and a plane in NEITHER face is still refused, so the nested path is a different
        // question and not an exemption.
        world::WorldData::Contents astray;
        Write("layout.portals.json",
              portal(R"({ "axis": "z", "value": 5.5 })", R"({ "u": [0.0, 1.0], "v": [0.70, 2.45] })"));
        ASSERT_TRUE(world::WorldLoader::LoadLevels(directory_, astray));
        ASSERT_TRUE(world::WorldLoader::LoadCells(directory_, astray));
        const auto refused = world::WorldLoader::LoadPortals(directory_, astray);
        ASSERT_FALSE(refused);
        EXPECT_NE(refused.Error().Message().find("has none on"), std::string::npos)
            << refused.Error().ToString();
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
        EXPECT_EQ(door.type, Intern("D_INT_PRIVACY"));
        // A cell id, not prose: the format said `"into_L0_WC1"` until `HOUSE-00378` made it a
        // reference that rule 6 resolves.
        EXPECT_EQ(door.swing, Intern("L0_WC1"));
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
        EXPECT_FALSE(slider.swing.IsValid());
        // ...and a row with no `type` leaves it unset rather than inventing one.
        EXPECT_FALSE(slider.type.IsValid());
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

    // --- the lights -------------------------------------------------------------------------

    TEST_F(WorldLoaderTest, ALightIsReadWithEveryFieldItCarries)
    {
        Write("layout.lights.json", Lights());
        world::WorldData::Contents contents;
        const auto lights = world::WorldLoader::LoadLights(directory_, contents);
        ASSERT_TRUE(lights) << lights.Error().ToString();

        ASSERT_EQ(contents.lights.size(), 4U);
        const world::Light& spot = contents.lights[1];
        EXPECT_EQ(spot.id, Intern("LIGHT_L0_KITCHEN_SINK"));
        EXPECT_EQ(spot.cell, Intern("L0_KITCHEN"));
        EXPECT_EQ(spot.group, Intern("LG_L0_KITCHEN_SINK"));
        EXPECT_EQ(spot.type, world::LightType::Spot);
        EXPECT_FLOAT_EQ(spot.position.Y, 3.10F);
        // All three components, and none of them the straight-down [0, -1, 0] a reader that
        // ignored the field might plausibly default to.
        EXPECT_FLOAT_EQ(spot.direction.X, 0.20F);
        EXPECT_FLOAT_EQ(spot.direction.Y, -0.90F);
        EXPECT_FLOAT_EQ(spot.direction.Z, 0.40F);
        EXPECT_FLOAT_EQ(spot.colorK, 3000.0F);
        EXPECT_FLOAT_EQ(spot.intensityLm, 420.0F);
        EXPECT_FLOAT_EQ(spot.range, 4.0F);
        EXPECT_FLOAT_EQ(spot.coneInnerDeg, 22.0F);
        EXPECT_FLOAT_EQ(spot.coneOuterDeg, 38.0F);
        EXPECT_EQ(spot.fixtureProp, Intern("PROP_L0_KITCHEN_DOWNLIGHT"));
        EXPECT_TRUE(spot.castsBlobShadow);
        EXPECT_TRUE(spot.bakedIntoLightmap);
        EXPECT_TRUE(spot.defaultOn);
    }

    TEST_F(WorldLoaderTest, BakedAndBlobShadowAreIndependent)
    {
        // The file says so in as many words: a baked light still needs a blob shadow for the
        // dynamic objects the bake never saw. Reading one from the other would lose every moving
        // shadow in a room that was lit offline.
        Write("layout.lights.json", Lights());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadLights(directory_, contents));

        EXPECT_TRUE(contents.lights[1].bakedIntoLightmap);
        EXPECT_TRUE(contents.lights[1].castsBlobShadow);
        EXPECT_TRUE(contents.lights[2].bakedIntoLightmap);
        EXPECT_FALSE(contents.lights[2].castsBlobShadow);
        EXPECT_FALSE(contents.lights[3].defaultOn) << "and a light may start off";
    }

    TEST_F(WorldLoaderTest, ASpotWithNoDirectionIsRefused)
    {
        // `Vector3::Zero` normalises to a NaN: a black room at run time, and there is nothing in
        // the frame to look at that says why.
        Write("layout.lights.json",
              R"({"schema": "cna-house/lights/1",
                  "lights": [{"id": "L", "cell": "C", "group": "G", "type": "spot",
                              "position": [0, 3, 0]}]})");
        world::WorldData::Contents contents;
        const auto lights = world::WorldLoader::LoadLights(directory_, contents);
        ASSERT_FALSE(lights);
        EXPECT_NE(lights.Error().Context().find("direction"), std::string::npos) << lights.Error().ToString();
    }

    TEST_F(WorldLoaderTest, APointLightNeedsNoDirection)
    {
        Write("layout.lights.json", Lights());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadLights(directory_, contents));
        EXPECT_EQ(contents.lights[0].type, world::LightType::Point);
    }

    TEST_F(WorldLoaderTest, AColourTemperatureOutsideThePhysicalRangeIsRefused)
    {
        // A missing zero on 2 700 puts a kitchen under a match. 1 000 K is a candle and 12 000 K
        // is a clear north sky; outside that is a typo, not a choice.
        for (const std::string colour : {"270", "27000"})
        {
            Write("layout.lights.json",
                  R"({"schema": "cna-house/lights/1",
                      "lights": [{"id": "L", "cell": "C", "group": "G", "type": "point",
                                  "position": [0, 3, 0], "colorK": )" +
                      colour + "}]}");
            world::WorldData::Contents contents;
            const auto lights = world::WorldLoader::LoadLights(directory_, contents);
            ASSERT_FALSE(lights) << "accepted " << colour;
            EXPECT_EQ(lights.Error().Code(), ErrorCode::OutOfRange);
        }
    }

    TEST_F(WorldLoaderTest, AnInnerConeOutsideItsOuterOneIsRefused)
    {
        // The falloff runs backwards and the spot gets a dark centre, which reads as a shader bug.
        Write("layout.lights.json",
              R"({"schema": "cna-house/lights/1",
                  "lights": [{"id": "L", "cell": "C", "group": "G", "type": "spot",
                              "position": [0, 3, 0], "direction": [0, -1, 0],
                              "coneInnerDeg": 50.0, "coneOuterDeg": 30.0}]})");
        world::WorldData::Contents contents;
        const auto lights = world::WorldLoader::LoadLights(directory_, contents);
        ASSERT_FALSE(lights);
        EXPECT_NE(lights.Error().Message().find("inside"), std::string::npos) << lights.Error().ToString();
    }

    TEST_F(WorldLoaderTest, ANegativeIntensityOrRangeIsRefused)
    {
        for (const std::string field : {"intensityLm", "range"})
        {
            Write("layout.lights.json",
                  R"({"schema": "cna-house/lights/1",
                      "lights": [{"id": "L", "cell": "C", "group": "G", "type": "point",
                                  "position": [0, 3, 0], ")" +
                      field + R"(": -1.0}]})");
            world::WorldData::Contents contents;
            EXPECT_FALSE(world::WorldLoader::LoadLights(directory_, contents))
                << "accepted a negative " << field;
        }
    }

    TEST_F(WorldLoaderTest, LightsAreIndexedByGroupAsWellAsByCell)
    {
        // A switch asks "what does this group toggle" and the renderer asks "what lights this
        // cell", and a group crosses cells: the stair-hall group lights two rooms from one plate.
        Write("layout.lights.json", Lights());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadLights(directory_, contents));
        const auto loaded = world::WorldData::Create(std::move(contents));
        ASSERT_TRUE(loaded) << loaded.Error().ToString();

        EXPECT_EQ(loaded.Value().LightsOf(Intern("L0_KITCHEN")).size(), 2U);
        EXPECT_EQ(loaded.Value().LightsInGroup(Intern("LG_L0_STAIR")).size(), 2U) << "one group, two cells";
        const auto group = loaded.Value().LightsInGroup(Intern("LG_L0_STAIR"));
        ASSERT_EQ(group.size(), 2U);
        EXPECT_NE(loaded.Value().Lights()[group[0]].cell, loaded.Value().Lights()[group[1]].cell);
        EXPECT_TRUE(loaded.Value().LightsInGroup(Intern("LG_NOWHERE")).empty());
    }

    // --- the props --------------------------------------------------------------------------

    TEST_F(WorldLoaderTest, APropIsReadWithEveryFieldItCarries)
    {
        Write("layout.props.json", Props());
        world::WorldData::Contents contents;
        const auto props = world::WorldLoader::LoadProps(directory_, contents);
        ASSERT_TRUE(props) << props.Error().ToString();

        ASSERT_EQ(contents.props.size(), 3U);
        const world::Prop& fridge = contents.props[0];
        EXPECT_EQ(fridge.id, Intern("PROP_L0_KITCHEN_FRIDGE"));
        EXPECT_EQ(fridge.asset, Intern("MODEL_PROP_KITCHEN_FRIDGE_01"));
        EXPECT_EQ(fridge.cell, Intern("L0_KITCHEN"));
        EXPECT_FLOAT_EQ(fridge.position.X, 1.20F);
        EXPECT_FLOAT_EQ(fridge.yawDeg, 180.0F);
        EXPECT_FLOAT_EQ(fridge.scale, 1.0F);
        EXPECT_FALSE(fridge.isStatic);
        EXPECT_EQ(fridge.lodGroup, Intern("LODG_APPLIANCE"));
        EXPECT_EQ(fridge.collision, world::PropCollision::Proxy);
        EXPECT_FALSE(fridge.material.IsValid()) << "null means the asset's own materials";
        EXPECT_EQ(fridge.interactable, Intern("FRIDGE_L0_KITCHEN"));
    }

    TEST_F(WorldLoaderTest, StaticDefaultsToTrueBecauseBatchingAssumesIt)
    {
        // §17.4: a prop that never moves is batched offline, and a row has to SAY false to become
        // a DynamicInstance. The two failure modes are not symmetric -- defaulting the other way
        // would silently un-batch the whole house, a draw-call regression nobody would trace back
        // to a default.
        Write("layout.props.json", Props());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadProps(directory_, contents));

        EXPECT_FALSE(contents.props[0].isStatic) << "the fridge says false";
        EXPECT_TRUE(contents.props[1].isStatic) << "the worktop says nothing";
        EXPECT_EQ(contents.props[1].collision, world::PropCollision::Proxy)
            << "and collision defaults to proxy";
        EXPECT_FLOAT_EQ(contents.props[1].scale, 1.0F);
    }

    TEST_F(WorldLoaderTest, APlumbingFixtureNamesTheStackItDrainsTo)
    {
        // The field that makes §15.7 rule 9 checkable at all: without it "every fixture's cell
        // appears in a declared stack" has no way to say which props are fixtures.
        Write("layout.props.json", Props());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadProps(directory_, contents));

        EXPECT_EQ(contents.props[2].plumbing, Intern("STACK_A"));
        // 0.98 and not 1.0, because 1.0 is the default: a reader that ignored the field would
        // have passed against a fixture that agreed with it by accident.
        EXPECT_FLOAT_EQ(contents.props[2].scale, 0.98F);
        EXPECT_FALSE(contents.props[0].plumbing.IsValid()) << "a fridge is not on a drain";
    }

    TEST_F(WorldLoaderTest, AScaleOfZeroOrLessIsRefused)
    {
        // Zero collapses the prop to a point and a negative scale turns it inside out. Both read
        // as a broken model rather than as a broken row.
        for (const std::string scale : {"0.0", "-1.0"})
        {
            Write("layout.props.json",
                  R"({"schema": "cna-house/props/1",
                      "props": [{"id": "P", "asset": "A", "cell": "C",
                                 "position": [0, 0, 0], "scale": )" +
                      scale + "}]}");
            world::WorldData::Contents contents;
            const auto props = world::WorldLoader::LoadProps(directory_, contents);
            ASSERT_FALSE(props) << "accepted a scale of " << scale;
            EXPECT_NE(props.Error().Context().find("scale"), std::string::npos) << props.Error().ToString();
        }
    }

    TEST_F(WorldLoaderTest, ACollisionModeOutsideItsVocabularyIsRefused)
    {
        Write("layout.props.json",
              R"({"schema": "cna-house/props/1",
                  "props": [{"id": "P", "asset": "A", "cell": "C", "position": [0, 0, 0],
                             "collision": "convex_hull"}]})");
        world::WorldData::Contents contents;
        const auto props = world::WorldLoader::LoadProps(directory_, contents);
        ASSERT_FALSE(props);
        EXPECT_NE(props.Error().Message().find("proxy"), std::string::npos) << props.Error().ToString();
    }

    TEST_F(WorldLoaderTest, PropsAreGroupedByTheirCell)
    {
        Write("layout.props.json", Props());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadProps(directory_, contents));
        const auto loaded = world::WorldData::Create(std::move(contents));
        ASSERT_TRUE(loaded) << loaded.Error().ToString();

        EXPECT_EQ(loaded.Value().PropsOf(Intern("L0_KITCHEN")).size(), 2U);
        EXPECT_EQ(loaded.Value().PropsOf(Intern("L0_WC1")).size(), 1U);
        EXPECT_TRUE(loaded.Value().PropsOf(Intern("L0_TERRACE")).empty());
    }

    // --- the nav graph ----------------------------------------------------------------------

    TEST_F(WorldLoaderTest, TheNavGraphIsReadWithItsNodesEdgesAndMarkers)
    {
        Write("layout.nav.json", Nav());
        world::WorldData::Contents contents;
        const auto nav = world::WorldLoader::LoadNav(directory_, contents);
        ASSERT_TRUE(nav) << nav.Error().ToString();

        ASSERT_EQ(contents.navNodes.size(), 3U);
        EXPECT_EQ(contents.navNodes[0].id, Intern("NAV_L0_KITCHEN_C"));
        EXPECT_EQ(contents.navNodes[0].cell, Intern("L0_KITCHEN"));
        EXPECT_FLOAT_EQ(contents.navNodes[0].position.Y, 0.60F);
        EXPECT_EQ(contents.navNodes[0].kind, "floor");

        ASSERT_EQ(contents.navEdges.size(), 2U);
        EXPECT_EQ(contents.navEdges[0].a, Intern("NAV_L0_KITCHEN_C"));
        EXPECT_EQ(contents.navEdges[0].b, Intern("NAV_L0_HALL_S"));
        EXPECT_FLOAT_EQ(contents.navEdges[0].cost, 4.2F);

        ASSERT_EQ(contents.navMarkers.size(), 3U);
        ASSERT_EQ(contents.navForbidden.size(), 1U);
        EXPECT_EQ(contents.navForbidden[0].cell, Intern("L0_GARAGE"));
    }

    TEST_F(WorldLoaderTest, AnEdgeCrossingAPortalNamesIt)
    {
        // The reason there is one authored graph and not two: a closed door closes the route for
        // the pets exactly as it does for vision and sound, and it does so because the edge says
        // which door.
        Write("layout.nav.json", Nav());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadNav(directory_, contents));

        EXPECT_EQ(contents.navEdges[0].portal, Intern("P_HALL__WC1"));
        EXPECT_FALSE(contents.navEdges[1].portal.IsValid()) << "an edge inside one room crosses no door";
    }

    TEST_F(WorldLoaderTest, AnAbsentSpeciesListMeansBothAndAnEmptyOneIsRefused)
    {
        // §61's answer for most of the graph is "both", so absent means both. An EMPTY list is not
        // the same thing: it is a row that does nothing, and far more likely a mistake.
        Write("layout.nav.json", Nav());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadNav(directory_, contents));

        EXPECT_EQ(contents.navEdges[1].species, world::Species::Both) << "states nothing";
        EXPECT_EQ(contents.navEdges[0].species, world::Species::Both) << "states both";
        EXPECT_EQ(contents.navMarkers[0].species, world::Species::Cat) << "a perch is the cat's";
        EXPECT_FALSE(Includes(contents.navMarkers[0].species, world::Species::Dog));

        Write("layout.nav.json",
              R"({"schema": "cna-house/nav/1", "nodes": [], "edges": [],
                  "perches": [{"id": "P", "cell": "C", "position": [0,0,0], "species": []}]})");
        world::WorldData::Contents empty;
        const auto nav = world::WorldLoader::LoadNav(directory_, empty);
        ASSERT_FALSE(nav);
        EXPECT_NE(nav.Error().Message().find("at least one"), std::string::npos) << nav.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AForbiddenZoneMustSayWhomItForbids)
    {
        // The one place an absent species list would read as a rule and do nothing.
        Write("layout.nav.json",
              R"({"schema": "cna-house/nav/1", "nodes": [], "edges": [],
                  "forbidden": [{"cell": "L0_GARAGE"}]})");
        world::WorldData::Contents contents;
        const auto nav = world::WorldLoader::LoadNav(directory_, contents);
        ASSERT_FALSE(nav);
        EXPECT_NE(nav.Error().Message().find("forbids nobody"), std::string::npos) << nav.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AMarkerIsPlacedByPositionOrByPropAndNeitherIsRefused)
    {
        // §61 uses both: a windowsill perch is a point, a dog bed is wherever the bed prop ended
        // up. A marker with neither is a marker nowhere.
        Write("layout.nav.json", Nav());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadNav(directory_, contents));

        EXPECT_FALSE(contents.navMarkers[0].prop.IsValid()) << "the perch is a point";
        EXPECT_FLOAT_EQ(contents.navMarkers[0].position.Y, 4.10F);
        EXPECT_EQ(contents.navMarkers[1].prop, Intern("PROP_DOG_BED")) << "the bed is a prop";

        Write("layout.nav.json",
              R"({"schema": "cna-house/nav/1", "nodes": [], "edges": [],
                  "beds": [{"id": "BED_NOWHERE", "species": ["dog"]}]})");
        world::WorldData::Contents nowhere;
        const auto nav = world::WorldLoader::LoadNav(directory_, nowhere);
        ASSERT_FALSE(nav);
        EXPECT_NE(nav.Error().Message().find("neither"), std::string::npos) << nav.Error().ToString();
    }

    TEST_F(WorldLoaderTest, TheThreeMarkerArraysKeepTheirKind)
    {
        // Perches, beds and bowls differ only in their name in the file and are read into one
        // list. Losing the kind would put the cat's water in the dog's bed.
        Write("layout.nav.json", Nav());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadNav(directory_, contents));

        ASSERT_EQ(contents.navMarkers.size(), 3U);
        EXPECT_EQ(contents.navMarkers[0].kind, world::MarkerKind::Perch);
        EXPECT_EQ(contents.navMarkers[1].kind, world::MarkerKind::Bed);
        EXPECT_EQ(contents.navMarkers[2].kind, world::MarkerKind::Bowl);
    }

    TEST_F(WorldLoaderTest, ANegativeEdgeCostOrASelfEdgeIsRefused)
    {
        for (const std::string edge :
             {R"({"a": "N1", "b": "N2", "cost": -1.0})", R"({"a": "N1", "b": "N1"})"})
        {
            Write("layout.nav.json",
                  R"({"schema": "cna-house/nav/1", "nodes": [], "edges": [)" + edge + "]}");
            world::WorldData::Contents contents;
            EXPECT_FALSE(world::WorldLoader::LoadNav(directory_, contents)) << "accepted " << edge;
        }
    }

    TEST_F(WorldLoaderTest, AnUnknownSpeciesIsRefused)
    {
        Write("layout.nav.json",
              R"({"schema": "cna-house/nav/1", "nodes": [], "edges": [],
                  "forbidden": [{"cell": "C", "species": ["ferret"]}]})");
        world::WorldData::Contents contents;
        const auto nav = world::WorldLoader::LoadNav(directory_, contents);
        ASSERT_FALSE(nav);
        EXPECT_NE(nav.Error().Message().find("ferret"), std::string::npos) << nav.Error().ToString();
    }

    // --- the audio --------------------------------------------------------------------------

    TEST_F(WorldLoaderTest, TheAudioZonesEmittersAndTransmissionTableAreRead)
    {
        Write("layout.audio.json", Audio());
        world::WorldData::Contents contents;
        const auto audio = world::WorldLoader::LoadAudio(directory_, contents);
        ASSERT_TRUE(audio) << audio.Error().ToString();

        ASSERT_EQ(contents.audioZones.size(), 2U);
        EXPECT_EQ(contents.audioZones[0].id, Intern("AZ_L0_KITCHEN"));
        EXPECT_EQ(contents.audioZones[0].cell, Intern("L0_KITCHEN"));
        EXPECT_EQ(contents.audioZones[0].bed, Intern("AMB_KITCHEN"));
        EXPECT_FLOAT_EQ(contents.audioZones[0].gain, 0.55F);
        EXPECT_FALSE(contents.audioZones[1].bed.IsValid()) << "a zone may have no bed";

        ASSERT_EQ(contents.audioEmitters.size(), 1U);
        const world::AudioEmitter& fridge = contents.audioEmitters[0];
        EXPECT_EQ(fridge.id, Intern("EM_FRIDGE_HUM"));
        EXPECT_EQ(fridge.cell, Intern("L0_KITCHEN"));
        EXPECT_FLOAT_EQ(fridge.position.Y, 0.90F);
        EXPECT_EQ(fridge.loop, Intern("AMB_FRIDGE_HUM"));
        EXPECT_FLOAT_EQ(fridge.gain, 0.35F);
        EXPECT_FLOAT_EQ(fridge.radius, 4.0F);
        EXPECT_EQ(fridge.interactable, Intern("FRIDGE_L0_KITCHEN"));

        ASSERT_EQ(contents.audioTransmission.size(), 2U);
        EXPECT_EQ(contents.audioTransmission[0].kind, "door_hollow");
        EXPECT_FLOAT_EQ(contents.audioTransmission[0].closed, 0.55F);
        EXPECT_FLOAT_EQ(contents.audioTransmission[1].closed, 0.78F);
    }

    TEST_F(WorldLoaderTest, AnEmitterCarriesItsCellBecauseTheSolverStartsFromCells)
    {
        // ADR-0010: the portal-path solve starts from cells, not from positions. A point alone
        // would have to be located first, on every voice, every frame.
        Write("layout.audio.json",
              R"({"schema": "cna-house/audio/1", "zones": [],
                  "emitters": [{"id": "E", "position": [0, 1, 0]}]})");
        world::WorldData::Contents contents;
        const auto audio = world::WorldLoader::LoadAudio(directory_, contents);
        ASSERT_FALSE(audio);
        EXPECT_NE(audio.Error().Context().find("cell"), std::string::npos) << audio.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AGainOutsideZeroToOneIsRefused)
    {
        // A gain is a linear multiplier and above 1 it clips -- a distortion in an ambience bed
        // follows the player from room to room.
        for (const std::string value : {"-0.1", "1.5"})
        {
            Write("layout.audio.json",
                  R"({"schema": "cna-house/audio/1",
                      "zones": [{"id": "Z", "cell": "C", "gain": )" +
                      value + "}]}");
            world::WorldData::Contents contents;
            const auto audio = world::WorldLoader::LoadAudio(directory_, contents);
            ASSERT_FALSE(audio) << "accepted a gain of " << value;
            EXPECT_EQ(audio.Error().Code(), ErrorCode::OutOfRange);
        }
    }

    TEST_F(WorldLoaderTest, ANegativeEmitterRadiusIsRefused)
    {
        Write("layout.audio.json",
              R"({"schema": "cna-house/audio/1", "zones": [],
                  "emitters": [{"id": "E", "cell": "C", "position": [0, 1, 0],
                                "radius": -2.0}]})");
        world::WorldData::Contents contents;
        const auto audio = world::WorldLoader::LoadAudio(directory_, contents);
        ASSERT_FALSE(audio);
        EXPECT_EQ(audio.Error().Code(), ErrorCode::OutOfRange);
    }

    TEST_F(WorldLoaderTest, ATransmissionLossThatFallsWhenTheDoorClosesIsRefused)
    {
        // Closing a door cannot make it quieter to shut than to leave open. A sign-flipped pair
        // sounds exactly like a broken audio system and nothing points at the data.
        Write("layout.audio.json",
              R"({"schema": "cna-house/audio/1", "zones": [],
                  "transmission": {"door_solid": {"open": 0.60, "closed": 0.20}}})");
        world::WorldData::Contents contents;
        const auto audio = world::WorldLoader::LoadAudio(directory_, contents);
        ASSERT_FALSE(audio);
        EXPECT_NE(audio.Error().Message().find("at least as much"), std::string::npos)
            << audio.Error().ToString();
    }

    TEST_F(WorldLoaderTest, ATransmissionLossOutsideZeroToOneIsRefused)
    {
        Write("layout.audio.json",
              R"({"schema": "cna-house/audio/1", "zones": [],
                  "transmission": {"door_solid": {"open": 0.05, "closed": 1.40}}})");
        world::WorldData::Contents contents;
        const auto audio = world::WorldLoader::LoadAudio(directory_, contents);
        ASSERT_FALSE(audio);
        EXPECT_EQ(audio.Error().Code(), ErrorCode::OutOfRange);
    }

    TEST_F(WorldLoaderTest, TheTransmissionTableIsLookedUpByName)
    {
        Write("layout.audio.json", Audio());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadAudio(directory_, contents));
        const auto loaded = world::WorldData::Create(std::move(contents));
        ASSERT_TRUE(loaded) << loaded.Error().ToString();

        const world::AudioTransmission* solid = loaded.Value().FindTransmission("door_solid");
        ASSERT_NE(solid, nullptr);
        EXPECT_FLOAT_EQ(solid->closed, 0.78F);
        EXPECT_EQ(loaded.Value().FindTransmission("portcullis"), nullptr);
    }

    // --- the exterior -----------------------------------------------------------------------

    TEST_F(WorldLoaderTest, TheExteriorIsReadWithEveryPieceItCarries)
    {
        Write("layout.exterior.json", Exterior());
        world::WorldData::Contents contents;
        const auto exterior = world::WorldLoader::LoadExterior(directory_, contents);
        ASSERT_TRUE(exterior) << exterior.Error().ToString();

        EXPECT_EQ(contents.exterior.terrain.heightfield, "Textures/Terrain/plot-height");
        EXPECT_FLOAT_EQ(contents.exterior.terrain.sizeX, 96.0F);
        EXPECT_FLOAT_EQ(contents.exterior.terrain.sizeZ, 128.0F);
        EXPECT_FLOAT_EQ(contents.exterior.terrain.yScale, 12.5F);
        EXPECT_EQ(contents.exterior.terrain.material, Intern("MAT_GRASS_LAWN"));

        ASSERT_EQ(contents.exterior.road.centreline.size(), 3U);
        EXPECT_FLOAT_EQ(contents.exterior.road.width, 6.5F);

        ASSERT_EQ(contents.exterior.fences.size(), 1U);
        EXPECT_EQ(contents.exterior.fences[0].id, Intern("FENCE_REAR"));
        ASSERT_EQ(contents.exterior.fences[0].path.size(), 3U);
        EXPECT_FLOAT_EQ(contents.exterior.fences[0].height, 1.80F);
        EXPECT_EQ(contents.exterior.fences[0].gate, Intern("GATE_REAR"));

        ASSERT_EQ(contents.exterior.neighbourhood.size(), 2U);
        EXPECT_FLOAT_EQ(contents.exterior.neighbourhood[0].yawDeg, 90.0F);
        EXPECT_FLOAT_EQ(contents.exterior.neighbourhood[0].impostorFrom, 45.0F);
        EXPECT_FLOAT_EQ(contents.exterior.neighbourhood[1].impostorFrom, 0.0F)
            << "always an impostor is a real choice for the far row";
    }

    TEST_F(WorldLoaderTest, VegetationStaysGroupedByAssetAsTheFileWritesIt)
    {
        // §17.4 draws it instanced where that measures faster, and that needs the grouping in the
        // data rather than rebuilt at load from one row per plant.
        Write("layout.exterior.json", Exterior());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadExterior(directory_, contents));

        ASSERT_EQ(contents.exterior.vegetation.size(), 2U);
        EXPECT_EQ(contents.exterior.vegetation[0].asset, Intern("MODEL_TREE_BIRCH_01"));
        ASSERT_EQ(contents.exterior.vegetation[0].instances.size(), 3U);
        EXPECT_EQ(contents.exterior.vegetation[1].instances.size(), 1U);
        EXPECT_FLOAT_EQ(contents.exterior.vegetation[0].instances[1].scale, 1.15F);
        EXPECT_FLOAT_EQ(contents.exterior.vegetation[0].instances[2].scale, 1.0F)
            << "an instance that states no scale is unscaled";
    }

    TEST_F(WorldLoaderTest, APathOfOnePointIsRefused)
    {
        // It would draw nothing and, worse, a fence built from it would occupy no ground at all --
        // a garden with a gap nobody authored.
        Write("layout.exterior.json",
              R"({"schema": "cna-house/exterior/1",
                  "terrain": {"heightfield": "h", "size": [10, 10], "origin": [0,0,0]},
                  "fences": [{"id": "F", "asset": "A", "path": [[0,0,0]]}]})");
        world::WorldData::Contents contents;
        const auto exterior = world::WorldLoader::LoadExterior(directory_, contents);
        ASSERT_FALSE(exterior);
        EXPECT_NE(exterior.Error().Message().find("at least 2"), std::string::npos)
            << exterior.Error().ToString();
    }

    TEST_F(WorldLoaderTest, ATerrainWithNoExtentIsRefused)
    {
        Write("layout.exterior.json",
              R"({"schema": "cna-house/exterior/1",
                  "terrain": {"heightfield": "h", "size": [0, 128], "origin": [0,0,0]}})");
        world::WorldData::Contents contents;
        const auto exterior = world::WorldLoader::LoadExterior(directory_, contents);
        ASSERT_FALSE(exterior);
        EXPECT_NE(exterior.Error().Context().find("size"), std::string::npos) << exterior.Error().ToString();
    }

    TEST_F(WorldLoaderTest, ANegativeImpostorDistanceIsRefused)
    {
        // Zero means "always an impostor", a real choice for the far row of houses. Negative is a
        // sign error that would swap the two branches and draw a full mesh at the horizon.
        Write("layout.exterior.json",
              R"({"schema": "cna-house/exterior/1",
                  "terrain": {"heightfield": "h", "size": [10, 10], "origin": [0,0,0]},
                  "neighbourhood": [{"id": "N", "asset": "A", "position": [0,0,0],
                                     "impostorFrom": -20.0}]})");
        world::WorldData::Contents contents;
        const auto exterior = world::WorldLoader::LoadExterior(directory_, contents);
        ASSERT_FALSE(exterior);
        EXPECT_EQ(exterior.Error().Code(), ErrorCode::OutOfRange);
    }

    TEST_F(WorldLoaderTest, APointOfTheWrongLengthIsRefused)
    {
        Write("layout.exterior.json",
              R"({"schema": "cna-house/exterior/1",
                  "terrain": {"heightfield": "h", "size": [10, 10], "origin": [0,0,0]},
                  "road": {"centreline": [[0,0], [10,0,0]], "width": 6.0}})");
        world::WorldData::Contents contents;
        const auto exterior = world::WorldLoader::LoadExterior(directory_, contents);
        ASSERT_FALSE(exterior);
        EXPECT_NE(exterior.Error().Message().find("[x, y, z]"), std::string::npos)
            << exterior.Error().ToString();
    }

    // --- the interactables ------------------------------------------------------------------

    TEST_F(WorldLoaderTest, AnInteractableIsReadWithItsStateAndItsParsedActions)
    {
        Write("interactables.json", Interactables());
        world::WorldData::Contents contents;
        const auto items = world::WorldLoader::LoadInteractables(directory_, contents);
        ASSERT_TRUE(items) << items.Error().ToString();

        ASSERT_EQ(contents.interactables.size(), 2U);
        const world::Interactable& fridge = contents.interactables[0];
        EXPECT_EQ(fridge.id, Intern("FRIDGE_L0_KITCHEN"));
        EXPECT_EQ(fridge.kind, "refrigerator");
        EXPECT_EQ(fridge.cell, Intern("L0_KITCHEN"));
        EXPECT_EQ(fridge.prop, Intern("PROP_L0_KITCHEN_FRIDGE"));
        EXPECT_FLOAT_EQ(fridge.focusPoint.Y, 1.40F);
        EXPECT_FLOAT_EQ(fridge.focusRadius, 0.85F);
        EXPECT_FLOAT_EQ(fridge.boundsMax.Y, 2.55F);
        EXPECT_EQ(fridge.audioLoop, Intern("AMB_FRIDGE_HUM"));
        EXPECT_EQ(fridge.portal, Intern("P_FRIDGE_INTERIOR"));
        ASSERT_EQ(fridge.childInteractables.size(), 1U);
        EXPECT_EQ(fridge.childInteractables[0], Intern("FRIDGE_ITEM_MILK_1"));

        ASSERT_EQ(fridge.state.Fields().size(), 3U);
        EXPECT_EQ(fridge.state.Fields()[0].name, "doorOpen");
        ASSERT_NE(fridge.state.Find("doorOpen"), nullptr);
        ASSERT_NE(fridge.state.Find("temperatureC"), nullptr);
        ASSERT_NE(fridge.state.Find("programme"), nullptr);
        EXPECT_FALSE(std::get<bool>(fridge.state.Find("doorOpen")->value));
        EXPECT_DOUBLE_EQ(std::get<double>(fridge.state.Find("temperatureC")->value), 4.0);
        EXPECT_EQ(std::get<std::string>(fridge.state.Find("programme")->value), "eco");

        ASSERT_EQ(fridge.actions.size(), 2U);
        EXPECT_EQ(fridge.actions[0].verb, "Open");
        EXPECT_EQ(fridge.actions[0].sound, Intern("SFX_FRIDGE_OPEN"));
        EXPECT_EQ(fridge.actions[0].anim, "door");
        EXPECT_FLOAT_EQ(fridge.actions[0].duration, 0.9F);
    }

    TEST_F(WorldLoaderTest, AnActionsPredicateAndEffectRunAgainstTheRowsOwnState)
    {
        // The whole point of reading `state` before `actions`: the expressions are parsed against
        // the fields this row declares, so the same text is valid here and a load error next door.
        Write("interactables.json", Interactables());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadInteractables(directory_, contents));

        world::Interactable& fridge = contents.interactables[0];
        const auto canOpen = fridge.actions[0].when.Evaluate(fridge.state);
        ASSERT_TRUE(canOpen);
        EXPECT_TRUE(canOpen.Value()) << "the door starts shut, so Open is available";

        ASSERT_TRUE(fridge.actions[0].effect.Apply(fridge.state));
        EXPECT_TRUE(std::get<bool>(fridge.state.Find("doorOpen")->value));

        const auto stillOpenable = fridge.actions[0].when.Evaluate(fridge.state);
        ASSERT_TRUE(stillOpenable);
        EXPECT_FALSE(stillOpenable.Value()) << "and not once it is open";

        const auto canClose = fridge.actions[1].when.Evaluate(fridge.state);
        ASSERT_TRUE(canClose);
        EXPECT_TRUE(canClose.Value());
    }

    TEST_F(WorldLoaderTest, AnUnknownTokenNamesTheFileTheIdAndTheToken)
    {
        // `HOUSE-00354`'s acceptance criterion, in as many words. The parser supplies the token
        // and the offset; the loader supplies the file and the id, and without those a message
        // about `doorAjar` could be about any of 640 rows.
        Write("interactables.json", OneInteractable(R"("when": "state.doorAjar == true")"));
        world::WorldData::Contents contents;
        const auto items = world::WorldLoader::LoadInteractables(directory_, contents);
        ASSERT_FALSE(items);

        const std::string context = items.Error().Context();
        EXPECT_NE(context.find("interactables.json"), std::string::npos) << context;
        EXPECT_NE(context.find("FRIDGE_L0_KITCHEN"), std::string::npos) << context;
        EXPECT_NE(context.find("Open"), std::string::npos) << "and which action of that row: " << context;
        EXPECT_NE(items.Error().Message().find("doorAjar"), std::string::npos) << items.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AnUnknownTokenInTheEffectIsCaughtToo)
    {
        Write("interactables.json", OneInteractable(R"JSON("do": "setDoor(true)")JSON"));
        world::WorldData::Contents contents;
        const auto items = world::WorldLoader::LoadInteractables(directory_, contents);
        ASSERT_FALSE(items);
        EXPECT_NE(items.Error().Message().find("setDoor"), std::string::npos) << items.Error().ToString();
        EXPECT_NE(items.Error().Context().find("do"), std::string::npos) << items.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AnAbsentWhenIsAlwaysTrueAndAnAbsentDoChangesNothing)
    {
        // Most of the 640 rows have no condition, and an action whose whole effect is a sound has
        // nothing to assign.
        Write("interactables.json", Interactables());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadInteractables(directory_, contents));

        const world::Interactable& lightSwitch = contents.interactables[1];
        ASSERT_EQ(lightSwitch.actions.size(), 1U);
        EXPECT_TRUE(lightSwitch.actions[0].when.IsAlwaysTrue());
        EXPECT_FALSE(lightSwitch.actions[0].effect.IsEmpty()) << "it does toggle the light";
    }

    TEST_F(WorldLoaderTest, AStateFieldThatIsNotABooleanNumberOrStringIsRefused)
    {
        // `contents[]` in §50.4 is a behaviour's own storage, not an expression field. Accepting
        // it here would let a predicate name something it can never compare.
        Write("interactables.json",
              R"({"schema": "cna-house/interactables/1",
                  "interactables": [{"id": "I", "kind": "container", "cell": "C",
                                     "focus": {"point": [0, 1, 0]},
                                     "state": {"contents": []},
                                     "actions": []}]})");
        world::WorldData::Contents contents;
        const auto items = world::WorldLoader::LoadInteractables(directory_, contents);
        ASSERT_FALSE(items);
        EXPECT_NE(items.Error().Message().find("contents"), std::string::npos) << items.Error().ToString();
    }

    TEST_F(WorldLoaderTest, APersistedFieldThatIsNotAStateFieldIsRefused)
    {
        // `persist` names exactly the fields the save carries. A name that is not a state field is
        // one the save would look for and never find, and §65's ~90 KB budget depends on the list
        // being exactly right.
        Write("interactables.json",
              R"({"schema": "cna-house/interactables/1",
                  "interactables": [{"id": "I", "kind": "container", "cell": "C",
                                     "focus": {"point": [0, 1, 0]},
                                     "state": {"doorOpen": false},
                                     "persist": ["doorOpen", "removedItems"],
                                     "actions": []}]})");
        world::WorldData::Contents contents;
        const auto items = world::WorldLoader::LoadInteractables(directory_, contents);
        ASSERT_FALSE(items);
        EXPECT_NE(items.Error().Message().find("removedItems"), std::string::npos)
            << items.Error().ToString();
        EXPECT_NE(items.Error().Message().find("doorOpen"), std::string::npos)
            << "and list what this row does declare: " << items.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AnInteractableWithoutAFocusIsRefused)
    {
        // §15.7 rule 11 proves the focus point reachable. A row with none is a row the proof
        // silently skips, which is the one failure a reachability check must not have.
        Write("interactables.json",
              R"({"schema": "cna-house/interactables/1",
                  "interactables": [{"id": "I", "kind": "switch", "cell": "C",
                                     "actions": []}]})");
        world::WorldData::Contents contents;
        const auto items = world::WorldLoader::LoadInteractables(directory_, contents);
        ASSERT_FALSE(items);
        EXPECT_NE(items.Error().Context().find("focus"), std::string::npos) << items.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AnInvertedBoundsBoxIsRefused)
    {
        Write("interactables.json",
              R"({"schema": "cna-house/interactables/1",
                  "interactables": [{"id": "I", "kind": "switch", "cell": "C",
                                     "focus": {"point": [0, 1, 0]},
                                     "bounds": {"min": [0, 0, 0], "max": [1, -1, 1]},
                                     "actions": []}]})");
        world::WorldData::Contents contents;
        const auto items = world::WorldLoader::LoadInteractables(directory_, contents);
        ASSERT_FALSE(items);
        EXPECT_NE(items.Error().Message().find("nothing is ever inside"), std::string::npos)
            << items.Error().ToString();
    }

    TEST_F(WorldLoaderTest, TheStateKeepsTheOrderTheFileWritesIt)
    {
        // The order is what a diagnostic lists, and what a slot index means. Sorted or hashed, the
        // "this row declares ..." message would differ between machines.
        Write("interactables.json", Interactables());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadInteractables(directory_, contents));

        const auto fields = contents.interactables[0].state.Fields();
        ASSERT_EQ(fields.size(), 3U);
        EXPECT_EQ(fields[0].name, "doorOpen");
        EXPECT_EQ(fields[1].name, "temperatureC");
        EXPECT_EQ(fields[2].name, "programme");
    }

    // --- the initial state ------------------------------------------------------------------

    TEST_F(WorldLoaderTest, TheInitialStateIsRead)
    {
        Write("interactables.json", Interactables());
        Write("initialstate.json", InitialState());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadInteractables(directory_, contents));
        const auto initial = world::WorldLoader::LoadInitialState(directory_, contents);
        ASSERT_TRUE(initial) << initial.Error().ToString();

        EXPECT_EQ(contents.initialState.player.cell, Intern("L0_FOYER"));
        EXPECT_FLOAT_EQ(contents.initialState.player.position.Z, -18.4F);
        EXPECT_FLOAT_EQ(contents.initialState.player.yawDeg, 90.0F);

        EXPECT_DOUBLE_EQ(contents.initialState.clock.epochSeconds, 21600.0);
        EXPECT_FLOAT_EQ(contents.initialState.clock.timeScale, 60.0F);
        EXPECT_FLOAT_EQ(contents.initialState.clock.latitudeDeg, 40.05F);
        EXPECT_EQ(contents.initialState.clock.utcOffsetMinutes, -300);

        EXPECT_EQ(contents.initialState.weather.target, Intern("W_PARTLY"));
        EXPECT_FLOAT_EQ(contents.initialState.weather.cloudCover, 0.35F);

        ASSERT_EQ(contents.initialState.pets.size(), 1U);
        EXPECT_EQ(contents.initialState.pets[0].id, Intern("PET_DOG"));
        EXPECT_EQ(contents.initialState.pets[0].cell, Intern("L0_FAMILY"));
        EXPECT_EQ(contents.initialState.pets[0].state, "Lie");
    }

    TEST_F(WorldLoaderTest, AnOpeningStateIsOnlyTheFieldsItNames)
    {
        // A partial table on purpose: `interactables.json` already declares every field and its
        // default, so a row that repeated all of them would be a second place to change one.
        Write("interactables.json", Interactables());
        Write("initialstate.json", InitialState());
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadInteractables(directory_, contents));
        ASSERT_TRUE(world::WorldLoader::LoadInitialState(directory_, contents));

        ASSERT_EQ(contents.initialState.interactables.size(), 1U);
        const world::InteractableStart& fridge = contents.initialState.interactables[0];
        EXPECT_EQ(fridge.id, Intern("FRIDGE_L0_KITCHEN"));
        ASSERT_EQ(fridge.overrides.Fields().size(), 1U)
            << "it names one field of three, and the other two keep their declared defaults";
        ASSERT_NE(fridge.overrides.Find("temperatureC"), nullptr);
        EXPECT_DOUBLE_EQ(std::get<double>(fridge.overrides.Find("temperatureC")->value), 2.5);
    }

    TEST_F(WorldLoaderTest, AnOpeningValueForAFieldTheInteractableDoesNotHaveIsRefused)
    {
        // This file is what every delta save is taken against (§65.6), so a field here that the
        // interactable does not declare is a value the save carries for ever and nothing reads.
        Write("interactables.json", Interactables());
        Write("initialstate.json",
              R"({"schema": "cna-house/initialstate/1",
                  "player": {"cell": "C", "position": [0, 0, 0]},
                  "clock": {"epochSeconds": 0, "timeScale": 1},
                  "interactables": {"FRIDGE_L0_KITCHEN": {"doorAjar": true}}})");
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadInteractables(directory_, contents));
        const auto initial = world::WorldLoader::LoadInitialState(directory_, contents);
        ASSERT_FALSE(initial);
        EXPECT_NE(initial.Error().Message().find("doorAjar"), std::string::npos)
            << initial.Error().ToString();
        EXPECT_NE(initial.Error().Message().find("temperatureC"), std::string::npos)
            << "and list what it does declare: " << initial.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AnOpeningValueOfTheWrongTypeIsRefused)
    {
        Write("interactables.json", Interactables());
        Write("initialstate.json",
              R"({"schema": "cna-house/initialstate/1",
                  "player": {"cell": "C", "position": [0, 0, 0]},
                  "clock": {"epochSeconds": 0, "timeScale": 1},
                  "interactables": {"FRIDGE_L0_KITCHEN": {"doorOpen": 0.5}}})");
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadInteractables(directory_, contents));
        const auto initial = world::WorldLoader::LoadInitialState(directory_, contents);
        ASSERT_FALSE(initial);
        EXPECT_NE(initial.Error().Message().find("different type"), std::string::npos)
            << initial.Error().ToString();
    }

    TEST_F(WorldLoaderTest, AnOpeningStateForAnInteractableThatDoesNotExistIsRuleSix)
    {
        // The field check needs the interactable, and a dangling id already has one owner in
        // §15.7 rule 6. A second message for it here would be a worse one.
        Write("interactables.json", Interactables());
        Write("initialstate.json",
              R"({"schema": "cna-house/initialstate/1",
                  "player": {"cell": "C", "position": [0, 0, 0]},
                  "clock": {"epochSeconds": 0, "timeScale": 1},
                  "interactables": {"NO_SUCH_THING": {"anything": true}}})");
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadInteractables(directory_, contents));
        const auto initial = world::WorldLoader::LoadInitialState(directory_, contents);
        ASSERT_TRUE(initial) << initial.Error().ToString();
        ASSERT_EQ(contents.initialState.interactables.size(), 1U);
        EXPECT_EQ(contents.initialState.interactables[0].id, Intern("NO_SUCH_THING"));
    }

    TEST_F(WorldLoaderTest, AClockOrWeatherOutsideItsRangeIsRefused)
    {
        const std::vector<std::pair<std::string, std::string>> broken{
            {R"("clock": {"epochSeconds": 0, "timeScale": -1})", "runs backwards"},
            {R"("clock": {"epochSeconds": 0, "timeScale": 1, "latitudeDeg": 120})", "latitude"},
            {R"("clock": {"epochSeconds": 0, "timeScale": 1, "longitudeDeg": 400})", "longitude"},
        };
        for (const auto& [clock, fragment] : broken)
        {
            Write("initialstate.json",
                  R"({"schema": "cna-house/initialstate/1",
                      "player": {"cell": "C", "position": [0, 0, 0]}, )" +
                      clock + "}");
            world::WorldData::Contents contents;
            const auto initial = world::WorldLoader::LoadInitialState(directory_, contents);
            ASSERT_FALSE(initial) << "accepted " << clock;
            EXPECT_NE(initial.Error().Message().find(fragment), std::string::npos)
                << initial.Error().ToString();
        }

        Write("initialstate.json",
              R"({"schema": "cna-house/initialstate/1",
                  "player": {"cell": "C", "position": [0, 0, 0]},
                  "clock": {"epochSeconds": 0, "timeScale": 1},
                  "weather": {"cloudCover": 1.4}})");
        world::WorldData::Contents contents;
        const auto initial = world::WorldLoader::LoadInitialState(directory_, contents);
        ASSERT_FALSE(initial);
        EXPECT_EQ(initial.Error().Code(), ErrorCode::OutOfRange);
    }

    TEST_F(WorldLoaderTest, ThePlayerMustNameTheCellItStartsIn)
    {
        // Without it §16.4 step 4 assigns `EXT_WORLD`: the game starts the player outside the
        // house it just loaded, and nothing in the frame says the spawn row was incomplete.
        Write("initialstate.json",
              R"({"schema": "cna-house/initialstate/1",
                  "player": {"position": [0, 0.6, 0]},
                  "clock": {"epochSeconds": 0, "timeScale": 1}})");
        world::WorldData::Contents contents;
        const auto initial = world::WorldLoader::LoadInitialState(directory_, contents);
        ASSERT_FALSE(initial);
        EXPECT_NE(initial.Error().Context().find("cell"), std::string::npos) << initial.Error().ToString();
    }

    TEST_F(WorldLoaderTest, ThePlayerAndTheClockAreRequired)
    {
        for (const std::string body : {R"("clock": {"epochSeconds": 0, "timeScale": 1})",
                                       R"("player": {"cell": "C", "position": [0, 0, 0]})"})
        {
            Write("initialstate.json", R"({"schema": "cna-house/initialstate/1", )" + body + "}");
            world::WorldData::Contents contents;
            EXPECT_FALSE(world::WorldLoader::LoadInitialState(directory_, contents))
                << "accepted a file with only " << body;
        }
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
        EXPECT_EQ(world.Value().Lights().size(), 4U);
        EXPECT_EQ(world.Value().Props().size(), 3U);
        EXPECT_EQ(world.Value().NavNodes().size(), 3U);
        EXPECT_EQ(world.Value().NavMarkers().size(), 3U);
        EXPECT_EQ(world.Value().AudioZones().size(), 2U);
        EXPECT_NE(world.Value().FindTransmission("door_solid"), nullptr);
        EXPECT_EQ(world.Value().GetExterior().vegetation.size(), 2U);
        EXPECT_EQ(world.Value().Interactables().size(), 2U);
        EXPECT_NE(world.Value().FindInteractable(Intern("SWITCH_L0_HALL")), nullptr);
        EXPECT_EQ(world.Value().GetInitialState().player.cell, Intern("L0_FOYER"));
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
        Write("layout.lights.json", Lights());
        Write("layout.props.json", Props());
        Write("layout.nav.json", Nav());
        Write("layout.audio.json", Audio());
        Write("layout.exterior.json", Exterior());
        Write("interactables.json", Interactables());
        Write("initialstate.json", InitialState());
        WriteManifest({"layout.levels.json",
                       "layout.materials.json",
                       "layout.cells.json",
                       "layout.portals.json",
                       "layout.openings.json",
                       "layout.stairs.json",
                       "layout.lights.json",
                       "layout.props.json",
                       "layout.nav.json",
                       "layout.audio.json",
                       "layout.exterior.json",
                       "interactables.json",
                       "initialstate.json"});

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

    // --- the authored world ------------------------------------------------------------------

    /// The real world, deployed, not a fixture.
    ///
    /// Every test above builds its own JSON, which proves the reader and proves nothing about the
    /// house. This one opens what `HOUSE-00366` onwards actually authored, with the loader the
    /// game uses, and grows a claim per file as each is written. The gates already run
    /// `world_schema.py` and `validate_world.py` over the same directory; what they cannot do is
    /// say that the **C++** reader agrees with them.
    ///
    /// It reads `content/world/` and not `assets-src/world/`, which is the point of
    /// `HOUSE-00421`: the authored files are JSONC and `System::Text::Json` is not. Tests run from
    /// the repository root (`tests/CMakeLists.txt`), so both paths are stable.
    TEST(AuthoredWorldTest, TheAuthoredLevelsLoadWithTheRealLoader)
    {
        IdRegistry::ResetForTesting();
        const std::string directory = "content/world";
        if (!std::filesystem::exists(directory + "/layout.levels.json"))
        {
            GTEST_SKIP() << "no deployed world; run tools/world/deploy_world.py";
        }

        const auto manifest = world::WorldLoader::LoadManifest(directory);
        ASSERT_TRUE(manifest) << manifest.Error().ToString();
        ASSERT_TRUE(world::WorldLoader::VerifyManifest(directory, manifest.Value()))
            << "the committed manifest must cover the committed files";

        world::WorldData::Contents contents;
        const auto levels = world::WorldLoader::LoadLevels(directory, contents);
        ASSERT_TRUE(levels) << levels.Error().ToString();

        // §12.2's five levels, in order, with the elevations the architecture states.
        ASSERT_EQ(contents.levels.size(), 5U);
        const std::vector<std::pair<const char*, float>> expected{
            {"B1", -2.30F}, {"L0", 0.60F}, {"L1", 3.65F}, {"L2", 6.55F}, {"L3", 9.30F}};
        for (std::size_t index = 0; index < expected.size(); ++index)
        {
            EXPECT_EQ(contents.levels[index].id, Intern(expected[index].first));
            EXPECT_FLOAT_EQ(contents.levels[index].ffl, expected[index].second) << expected[index].first;
        }

        // The one level whose ceiling is null, because rafters bound it and not a plane.
        for (std::size_t index = 0; index < 4U; ++index)
        {
            EXPECT_TRUE(contents.levels[index].ceiling.has_value())
                << contents.levels[index].name << " has a ceiling plane";
        }
        EXPECT_FALSE(contents.levels[4].ceiling.has_value()) << "the attic does not";
        EXPECT_EQ(contents.levels[4].roof, Intern("ROOF_MAIN"));

        // The structural depth is not decoration: every storey's ceiling plus it is the next
        // storey's floor, which is what makes the elevations a section rather than five numbers.
        for (std::size_t index = 0; index + 1 < 4U; ++index)
        {
            const world::Level& below = contents.levels[index];
            ASSERT_TRUE(below.ceiling.has_value());
            EXPECT_NEAR(*below.ceiling + below.structureDepth, contents.levels[index + 1].ffl, 1e-4F)
                << below.name << " to " << contents.levels[index + 1].name;
        }

        EXPECT_FLOAT_EQ(contents.construction.wallExterior, 0.30F);
        EXPECT_FLOAT_EQ(contents.construction.wallPartition, 0.15F);
        EXPECT_FLOAT_EQ(contents.construction.ridgeY, 14.30F);
        // §12.1's 7:12, as a slope. Read as radians it would be 0.528 and the roof would be a
        // different roof; a test is the cheapest place to say which reading this is.
        EXPECT_NEAR(contents.construction.roofPitch, 7.0F / 12.0F, 1e-4F);

        // §12.5's six stacks (`HOUSE-00386`). Two of them branch on one floor -- the kitchen sink
        // with the sunroom's wet bar, and the two basement fixtures -- which is what plumbing
        // does and what §15.7 rule 9 stopped forbidding.
        ASSERT_EQ(contents.plumbing.size(), 6U);
        EXPECT_EQ(contents.plumbing[0].id, Intern("STACK_A"));
        EXPECT_EQ(contents.plumbing[0].dropTo, Intern("B1_UTILITY"));
        const auto stack = [&contents](const char* name) -> const world::PlumbingStack&
        {
            return *std::find_if(contents.plumbing.begin(),
                                 contents.plumbing.end(),
                                 [name](const world::PlumbingStack& row) { return row.id == Intern(name); });
        };
        EXPECT_EQ(stack("STACK_E").cells.size(), 2U) << "the kitchen sink and the wet bar";
        EXPECT_FALSE(stack("STACK_F").dropTo.IsValid())
            << "the basement stack drops into an ejector pit, which is machinery and not a cell";

        IdRegistry::ResetForTesting();
    }

    TEST(AuthoredWorldTest, TheAuthoredCellsPortalsAndLeavesLoadWithTheRealLoader)
    {
        IdRegistry::ResetForTesting();
        const std::string directory = "content/world";
        if (!std::filesystem::exists(directory + "/layout.openings.json"))
        {
            GTEST_SKIP() << "no deployed world; run tools/world/deploy_world.py";
        }

        world::WorldData::Contents contents;
        for (const auto& [name, load] : std::initializer_list<
                 std::pair<const char*,
                           cnahouse::util::Result<void> (*)(std::string_view, world::WorldData::Contents&)>>{
                 {"levels", &world::WorldLoader::LoadLevels},
                 {"cells", &world::WorldLoader::LoadCells},
                 {"portals", &world::WorldLoader::LoadPortals},
                 {"openings", &world::WorldLoader::LoadOpenings}})
        {
            const cnahouse::util::Result<void> loaded = load(directory, contents);
            ASSERT_TRUE(loaded) << name << ": " << loaded.Error().ToString();
        }

        // §16.3, measured: 75 rooms, 3 nested sub-cells and 18 exterior cells.
        EXPECT_EQ(contents.cells.size(), 96U);
        EXPECT_EQ(std::count_if(contents.cells.begin(),
                                contents.cells.end(),
                                [](const world::Cell& cell) { return cell.parent.IsValid(); }),
                  3);

        EXPECT_EQ(contents.portals.size(), 179U);
        EXPECT_EQ(std::count_if(contents.portals.begin(),
                                contents.portals.end(),
                                [](const world::Portal& portal)
                                { return portal.kind == world::PortalKind::Window; }),
                  66);

        // §15.7 rule 7's bijection, asserted by the OTHER implementation. `validate_world.py`
        // makes the same statement in Python over the authored files; this makes it in C++ over
        // the deployed ones, and the day the two disagree one of them is wrong about the house.
        EXPECT_EQ(contents.openings.size(), 133U);
        std::map<cnahouse::util::Id, cnahouse::util::Id> leafOf;
        for (const world::Opening& opening : contents.openings)
        {
            EXPECT_TRUE(opening.portal.IsValid()) << "every leaf names a portal";
            EXPECT_TRUE(leafOf.emplace(opening.portal, opening.id).second)
                << "a portal carries exactly one leaf";
        }
        for (const world::Portal& portal : contents.portals)
        {
            if (portal.kind == world::PortalKind::CasedOpening || portal.kind == world::PortalKind::StairWell)
            {
                continue;
            }
            const auto found = leafOf.find(portal.id);
            ASSERT_NE(found, leafOf.end()) << "a shut-able portal has a leaf";
            // ...and names it back. `aperture` unset means "always fully open", so a door that
            // leaves it unset claims to be a hole (`HOUSE-00378`).
            EXPECT_EQ(portal.aperture, found->second);
        }

        IdRegistry::ResetForTesting();
    }

    TEST(AuthoredWorldTest, TheAuthoredFlightsClimbWhatTheyClaim)
    {
        IdRegistry::ResetForTesting();
        const std::string directory = "content/world";
        if (!std::filesystem::exists(directory + "/layout.stairs.json"))
        {
            GTEST_SKIP() << "no deployed world; run tools/world/deploy_world.py";
        }

        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadLevels(directory, contents));
        ASSERT_TRUE(world::WorldLoader::LoadCells(directory, contents));
        const auto flights = world::WorldLoader::LoadStairs(directory, contents);
        ASSERT_TRUE(flights) << flights.Error().ToString();

        // §12.4's eight flights, and §15.7 rule 8 asserted by the other implementation: four climb
        // between storeys and the levels' FFLs say what that is; four join two cells on one level
        // and declare it themselves, because one level has no level difference to check against.
        ASSERT_EQ(contents.stairs.size(), 8U);
        std::map<cnahouse::util::Id, float> fflOf;
        for (const world::Level& level : contents.levels)
        {
            fflOf.emplace(level.id, level.ffl);
        }
        const auto levelOf = [&contents](cnahouse::util::Id cellId)
        {
            const auto found = std::find_if(contents.cells.begin(),
                                            contents.cells.end(),
                                            [cellId](const world::Cell& cell) { return cell.id == cellId; });
            return found == contents.cells.end() ? cnahouse::util::Id{} : found->level;
        };

        int declared = 0;
        for (const world::StairFlight& flight : contents.stairs)
        {
            const cnahouse::util::Id from = levelOf(flight.fromCell);
            const cnahouse::util::Id to = levelOf(flight.toCell);
            ASSERT_TRUE(from.IsValid() && to.IsValid()) << "a flight names two cells that exist";
            if (flight.fromY.has_value())
            {
                ++declared;
                EXPECT_NEAR(flight.Climb(), *flight.toY - *flight.fromY, 1e-3F)
                    << "flight " << flight.risers << " x " << flight.rise;
                continue;
            }
            EXPECT_NE(from, to) << "a flight between two cells on one level declares fromY/toY";
            EXPECT_NEAR(flight.Climb(), std::abs(fflOf.at(to) - fflOf.at(from)), 1e-3F);
        }
        EXPECT_EQ(declared, 4) << "the porch, both terrace flights and the garage steps";

        IdRegistry::ResetForTesting();
    }

    TEST(AuthoredWorldTest, TheAuthoredWorldIsPlainJsonAndTheSourceIsNot)
    {
        // The two halves of the format, both asserted, because until `HOUSE-00421` only one of
        // them was built. The authored file carries comments and the deployed one must not: a
        // deploy that copied bytes would pass every other test here and fail in the game.
        if (!std::filesystem::exists("assets-src/world/layout.levels.json"))
        {
            GTEST_SKIP() << "no authored world yet";
        }
        const auto read = [](const std::string& path)
        {
            std::ifstream in(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        };

        EXPECT_NE(read("assets-src/world/layout.levels.json").find("//"), std::string::npos)
            << "the authored file is JSONC and this one uses comments";

        ASSERT_TRUE(std::filesystem::exists("content/world/layout.levels.json"))
            << "run tools/world/deploy_world.py";
        EXPECT_EQ(read("content/world/layout.levels.json").find("//"), std::string::npos)
            << "and the deployed file must have none, because System::Text::Json refuses them";
    }

    TEST_F(WorldLoaderTest, JoinProducesOneSeparatorWhateverItIsGiven)
    {
        EXPECT_EQ(world::WorldLoader::Join("a", "b.json"), "a/b.json");
        EXPECT_EQ(world::WorldLoader::Join("a/", "b.json"), "a/b.json");
        EXPECT_EQ(world::WorldLoader::Join("a///", "b.json"), "a/b.json");
    }
} // namespace
