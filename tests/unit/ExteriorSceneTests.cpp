// SPDX-License-Identifier: MIT
//
// `HOUSE-00700`. The outdoors on §25.6's instance path.
//
// §25.6 built its hierarchy in `HOUSE-00677` and its traversal in `HOUSE-00678`, and until this
// **nothing put the ground in it**: `HOUSE-00780` filed the terrain, the road, the fences and the
// garden structures as per-cell CHUNKS as an interim, so a lawn was drawn only when the portal
// walk happened to reach the exterior cell it was filed under. §25.6 says in terms that this
// cannot work -- *"`EXT_WORLD` is one enormous cell, so portal traversal cannot help inside it"*.
//
// What is proved here is the translation: which chunks become instances, what category each one
// takes, and that a chunk can still be found after the hierarchy has reordered them -- which is
// the one thing a BVH is guaranteed to do to a list.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/visibility/ExteriorScene.hpp"
#include "cnahouse/world/ChunkData.hpp"
#include "cnahouse/world/ChunkReader.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

namespace
{
    using cnahouse::visibility::BuildExteriorScene;
    using cnahouse::visibility::CategoryForMaterial;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::ExteriorScene;
    using cnahouse::visibility::GatherExteriorCones;
    using cnahouse::visibility::IsExteriorDoorMaterial;
    using cnahouse::visibility::IsExteriorSkinMaterial;
    using cnahouse::visibility::IsExteriorWindowMaterial;
    using cnahouse::visibility::PropCategory;
    using cnahouse::visibility::VisibleCell;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    bool ContentIsBuilt()
    {
        return std::filesystem::exists("content/world/layout.cells.json") &&
               std::filesystem::exists("content/world/chunks.bin");
    }

    world::WorldData LoadWorld()
    {
        world::WorldData::Contents contents;
        EXPECT_TRUE(world::WorldLoader::LoadLevels("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadCells("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadPortals("content/world", contents).HasValue());
        auto built = world::WorldData::Create(std::move(contents));
        EXPECT_TRUE(built) << built.Error().ToString();
        return std::move(built.Value());
    }

    world::ChunkLibrary LoadChunks()
    {
        System::IO::FileStream stream(
            "content/world/chunks.bin", System::IO::FileMode::Open, System::IO::FileAccess::Read);
        auto library = world::ChunkReader::Read(stream, "content/world/chunks.bin");
        EXPECT_TRUE(library) << (library ? std::string() : library.Error().Message());
        return library ? std::move(*library) : world::ChunkLibrary{};
    }

    /// A chunk with nothing in it but the three things `BuildExteriorScene` reads.
    world::Chunk Piece(std::uint16_t cell, std::uint16_t material, float x, float z)
    {
        world::Chunk chunk;
        chunk.cell = cell;
        chunk.material = material;
        chunk.bounds = BoundingBox(Vector3(x, 0.0F, z), Vector3(x + 1.0F, 1.0F, z + 1.0F));
        return chunk;
    }

} // namespace

TEST(ExteriorSceneTests, TheCategoryComesFromTheMaterialsOwnPrefix)
{
    // A chunk IS one material (§17.4), and the content build already groups the outdoors by
    // prefix, so the category is read off the name rather than kept in a second table that could
    // disagree with it.
    EXPECT_EQ(CategoryForMaterial("TERRAIN_grass"), PropCategory::Ground);
    EXPECT_EQ(CategoryForMaterial("TERRAIN_asphalt"), PropCategory::Ground);
    EXPECT_EQ(CategoryForMaterial("ROAD_paint"), PropCategory::Ground);
    EXPECT_EQ(CategoryForMaterial("FENCE_board"), PropCategory::Fence);
    EXPECT_EQ(CategoryForMaterial("GATE_ornamental"), PropCategory::Fence);
    EXPECT_EQ(CategoryForMaterial("MAT_OUTDOOR_FENCE_BOARD"), PropCategory::Fence);
    EXPECT_EQ(CategoryForMaterial("MAT_OUTDOOR_FENCE_METAL"), PropCategory::Fence);
    EXPECT_EQ(CategoryForMaterial("GARDEN_bed"), PropCategory::GardenFurniture);
    EXPECT_EQ(CategoryForMaterial("TREE_maple"), PropCategory::Tree);
    EXPECT_EQ(CategoryForMaterial("VEG_shrub"), PropCategory::Tree);
    EXPECT_EQ(CategoryForMaterial("MAT_VEGETATION_TREE_FOLIAGE"), PropCategory::Tree);
    EXPECT_EQ(CategoryForMaterial("MAT_VEGETATION_BARK"), PropCategory::Tree);
    EXPECT_EQ(CategoryForMaterial("NB_HORIZON"), PropCategory::Impostor);
    EXPECT_EQ(CategoryForMaterial("NB_IMPOSTOR_row"), PropCategory::Impostor);
    // ...and the neighbourhood's SOLID geometry is not an impostor, which is the pair of prefixes
    // that has to be tested together or the order of the tests is doing the work.
    EXPECT_EQ(CategoryForMaterial("NB_WALL"), PropCategory::NeighbourhoodLod0);
    EXPECT_EQ(CategoryForMaterial("NB_WINDOW_GLOW"), PropCategory::NeighbourhoodLod0);

    // **The unrecognised answer is the one that removes nothing.** A category only ever culls at
    // distance, so guessing `SmallProp` at 45 m for a name nobody has classified yet would take
    // the house's own roof out of the picture from the far end of the garden. `Ground` is not
    // distance culled at all (`kCullDistances` gives it §10.3's far plane), so an unknown material
    // is drawn until the frustum says otherwise -- visibly wrong beats invisibly missing.
    EXPECT_EQ(CategoryForMaterial("BLOCKOUT_exterior"), PropCategory::Ground);
    EXPECT_EQ(CategoryForMaterial("BLOCKOUT_roof"), PropCategory::Ground);
    EXPECT_EQ(CategoryForMaterial(""), PropCategory::Ground);
    EXPECT_EQ(CategoryForMaterial("TERRAIN"), PropCategory::Ground) << "no underscore, no prefix";
    // A prefix is a PREFIX. A material that merely contains the word is not one.
    EXPECT_EQ(CategoryForMaterial("PROP_FENCE_post"), PropCategory::Ground);
}

TEST(ExteriorSceneTests, TheHouseOuterSkinIsAnExteriorInstance)
{
    EXPECT_TRUE(IsExteriorSkinMaterial("MAT_SIDING_WARM_WHITE"));
    EXPECT_TRUE(IsExteriorSkinMaterial("MAT_SIDING_SAGE_WET"));
    EXPECT_TRUE(IsExteriorSkinMaterial("MAT_BRICK_WATER_TABLE"));
    EXPECT_TRUE(IsExteriorSkinMaterial("MAT_BRICK_WATER_TABLE_WET"));
    EXPECT_FALSE(IsExteriorSkinMaterial("MAT_PAINT_WARM_WHITE"));
    EXPECT_FALSE(IsExteriorSkinMaterial("MAT_TRIM_PAINTED_WHITE"));
    EXPECT_FALSE(IsExteriorSkinMaterial("BLOCKOUT_wall"));
}

TEST(ExteriorSceneTests, OutsideWindowRolesDoNotIncludeIndoorTrimOrBorrowedGlass)
{
    EXPECT_TRUE(IsExteriorWindowMaterial("MAT_WINDOW_FRAME_WHITE"));
    EXPECT_TRUE(IsExteriorWindowMaterial("MAT_WINDOW_SHUTTER_BLACK"));
    EXPECT_TRUE(IsExteriorWindowMaterial("MAT_WINDOW_GLASS_CLEAR"));
    EXPECT_TRUE(IsExteriorWindowMaterial("MAT_WINDOW_GLASS_OBSCURED"));
    EXPECT_FALSE(IsExteriorWindowMaterial("MAT_DOOR_HARDWOOD"));
    EXPECT_FALSE(IsExteriorWindowMaterial("MAT_GLASS_CLEAR"));
    EXPECT_FALSE(IsExteriorWindowMaterial("MAT_GLASS_OBSCURED"));
    EXPECT_FALSE(IsExteriorWindowMaterial("MAT_SIDING_WARM_WHITE"));
}

TEST(ExteriorSceneTests, OutsideDoorRolesDoNotIncludeOrdinaryRoomJoinery)
{
    EXPECT_TRUE(IsExteriorDoorMaterial("MAT_EXTERIOR_DOOR_HARDWOOD"));
    EXPECT_TRUE(IsExteriorDoorMaterial("MAT_EXTERIOR_DOOR_PANEL_HARDWOOD"));
    EXPECT_TRUE(IsExteriorDoorMaterial("MAT_EXTERIOR_DOOR_PAINTED"));
    EXPECT_TRUE(IsExteriorDoorMaterial("MAT_EXTERIOR_DOOR_GARAGE_PAINTED"));
    EXPECT_TRUE(IsExteriorDoorMaterial("MAT_EXTERIOR_DOOR_GARAGE_PANEL_PAINTED"));
    EXPECT_TRUE(IsExteriorDoorMaterial("MAT_EXTERIOR_DOOR_HARDWARE_BRONZE"));
    EXPECT_FALSE(IsExteriorDoorMaterial("MAT_DOOR_HARDWOOD"));
    EXPECT_FALSE(IsExteriorDoorMaterial("MAT_DOOR_PAINTED"));
    EXPECT_FALSE(IsExteriorDoorMaterial("MAT_WINDOW_FRAME_WHITE"));
}

TEST(ExteriorSceneTests, OnlyExteriorSpaceSkinAndOutsideWindowChunksBecomeInstances)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world";
    }
    const world::WorldData world = LoadWorld();

    world::ChunkLibrary library;
    library.cells = {"EXT_FRONTYARD_E", "B1_GYM", "EXT_BACKYARD", "NOT_A_CELL_AT_ALL"};
    library.materials = {"TERRAIN_grass",
                         "BLOCKOUT_wall",
                         "MAT_SIDING_WARM_WHITE",
                         "MAT_BRICK_WATER_TABLE",
                         "MAT_WINDOW_FRAME_WHITE",
                         "MAT_WINDOW_GLASS_CLEAR",
                         "MAT_DOOR_HARDWOOD",
                         "MAT_GLASS_CLEAR",
                         "MAT_EXTERIOR_DOOR_HARDWOOD",
                         "MAT_WINDOW_SHUTTER_BLACK"};
    library.chunks.push_back(Piece(0, 0, 0.0F, 0.0F));   // exterior: in
    library.chunks.push_back(Piece(1, 1, 2.0F, 0.0F));   // a room's inner wall: out
    library.chunks.push_back(Piece(2, 0, 4.0F, 0.0F));   // exterior: in
    library.chunks.push_back(Piece(1, 2, 6.0F, 0.0F));   // room-owned siding: in
    library.chunks.push_back(Piece(1, 3, 8.0F, 0.0F));   // room-owned water table: in
    library.chunks.push_back(Piece(3, 2, 10.0F, 0.0F));  // no such cell: out
    library.chunks.push_back(Piece(9, 2, 12.0F, 0.0F));  // cell index past the end: out
    library.chunks.push_back(Piece(0, 10, 14.0F, 0.0F)); // material index past the end: out
    library.chunks.push_back(Piece(1, 4, 16.0F, 0.0F));  // room-owned outside frame: in
    library.chunks.push_back(Piece(1, 5, 18.0F, 0.0F));  // room-owned outside glass: in
    library.chunks.push_back(Piece(1, 6, 20.0F, 0.0F));  // ordinary room skirting: out
    library.chunks.push_back(Piece(1, 7, 22.0F, 0.0F));  // borrowed indoor glass: out
    library.chunks.push_back(Piece(1, 8, 24.0F, 0.0F));  // room-owned outside door: in
    library.chunks.push_back(Piece(1, 9, 26.0F, 0.0F));  // room-owned outside shutter: in

    const ExteriorScene scene = BuildExteriorScene(library, world);
    ASSERT_EQ(scene.instances.size(), 8u);
    std::vector<bool> seen(library.chunks.size(), false);
    for (std::uint32_t index = 0; index < scene.instances.size(); ++index)
    {
        seen[scene.ChunkOf(index)] = true;
    }
    EXPECT_TRUE(seen[0u]);
    EXPECT_TRUE(seen[2u]);
    EXPECT_TRUE(seen[3u]);
    EXPECT_TRUE(seen[4u]);
    EXPECT_TRUE(seen[8u]);
    EXPECT_TRUE(seen[9u]);
    EXPECT_TRUE(seen[12u]);
    EXPECT_TRUE(seen[13u]);
    EXPECT_FALSE(seen[10u]);
    EXPECT_FALSE(seen[11u]);
    EXPECT_FALSE(scene.Empty());

    // A room's inner wall is not merely absent from the hierarchy, it is not REACHABLE through it.
    for (std::uint32_t index = 0; index < scene.bvh.Instances().size(); ++index)
    {
        EXPECT_NE(scene.ChunkOf(index), 1u) << "a room's inner wall got into §25.6's hierarchy";
    }
}

TEST(ExteriorSceneTests, AWorldWithNoExteriorCellsMakesAnEmptyScene)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world";
    }
    const world::WorldData world = LoadWorld();

    world::ChunkLibrary library;
    library.cells = {"B1_GYM"};
    library.materials = {"BLOCKOUT_wall"};
    library.chunks.push_back(Piece(0, 0, 0.0F, 0.0F));

    const ExteriorScene scene = BuildExteriorScene(library, world);
    EXPECT_TRUE(scene.Empty());
    EXPECT_TRUE(scene.bvh.Instances().empty());
    // ...and asking a scene that holds nothing which chunk position 0 came from is answered, not
    // undefined: 0 is `util::Id`'s "no id" and the caller draws nothing.
    EXPECT_EQ(scene.ChunkOf(0u), 0u);
    EXPECT_EQ(scene.ChunkOf(4000u), 0u);
}

TEST(ExteriorSceneTests, TheChunkIsFoundAgainAfterTheHierarchyHasReorderedIt)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world";
    }
    const world::WorldData world = LoadWorld();

    // Sixty-four pieces on an 8x8 grid, submitted in ROW order. A BVH splits by axis and cannot
    // leave a grid in row order, which is the whole reason `ExteriorScene` carries the chunk on
    // the instance's id instead of trusting the position -- an earlier draft used the position and
    // drew the right count of the wrong lawns.
    world::ChunkLibrary library;
    library.cells = {"EXT_WORLD"};
    library.materials = {"TERRAIN_grass"};
    for (int row = 0; row < 8; ++row)
    {
        for (int column = 0; column < 8; ++column)
        {
            library.chunks.push_back(
                Piece(0, 0, static_cast<float>(column) * 4.0F, static_cast<float>(row) * 4.0F));
        }
    }

    const ExteriorScene scene = BuildExteriorScene(library, world);
    ASSERT_EQ(scene.instances.size(), library.chunks.size());

    const std::span<const cnahouse::visibility::ExteriorInstance> held = scene.bvh.Instances();
    ASSERT_EQ(held.size(), library.chunks.size());
    bool reordered = false;
    std::vector<bool> seen(library.chunks.size(), false);
    for (std::uint32_t index = 0; index < held.size(); ++index)
    {
        const std::uint32_t chunk = scene.ChunkOf(index);
        ASSERT_LT(chunk, library.chunks.size());
        EXPECT_FALSE(seen[chunk]) << "chunk " << chunk << " came back twice";
        seen[chunk] = true;
        reordered = reordered || chunk != index;
        // The box the hierarchy holds is the box that chunk has, which is what makes the mapping
        // the right one rather than merely a permutation.
        EXPECT_FLOAT_EQ(held[index].bounds.Min.X, library.chunks[chunk].bounds.Min.X) << "position " << index;
        EXPECT_FLOAT_EQ(held[index].bounds.Min.Z, library.chunks[chunk].bounds.Min.Z) << "position " << index;
    }
    EXPECT_TRUE(std::all_of(seen.begin(), seen.end(), [](bool hit) { return hit; }));
    EXPECT_TRUE(reordered) << "the hierarchy left a grid in row order, so this test proves nothing";
}

TEST(ExteriorSceneTests, TheWholePropertysOutdoorsIsInTheHierarchy)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world";
    }
    const world::WorldData world = LoadWorld();
    const world::ChunkLibrary library = LoadChunks();
    const ExteriorScene scene = BuildExteriorScene(library, world);

    // Counted the slow way from the world's cell kinds and authored material vocabulary, so the
    // two agreeing is a measurement and not this test reading the answer off its instances.
    std::size_t expected = 0;
    std::size_t ground = 0;
    std::size_t outerSkin = 0;
    std::size_t outsideWindows = 0;
    std::size_t outsideDoors = 0;
    for (const world::Chunk& chunk : library.chunks)
    {
        if (chunk.cell >= library.cells.size() || chunk.material >= library.materials.size())
        {
            continue;
        }
        const world::Cell* cell = world.FindCell(cnahouse::util::Intern(library.cells[chunk.cell]));
        if (cell == nullptr)
        {
            continue;
        }
        const std::string_view material = library.materials[chunk.material];
        const bool skin = IsExteriorSkinMaterial(material);
        const bool window = IsExteriorWindowMaterial(material);
        const bool door = IsExteriorDoorMaterial(material);
        if (cell->kind != world::CellKind::Exterior && !skin && !window && !door)
        {
            continue;
        }
        ++expected;
        outerSkin += skin ? 1u : 0u;
        outsideWindows += window ? 1u : 0u;
        outsideDoors += door ? 1u : 0u;
        ground += CategoryForMaterial(material) == PropCategory::Ground ? 1u : 0u;
    }
    EXPECT_EQ(scene.instances.size(), expected);
    EXPECT_GT(expected, 20u) << "the property has a lawn, a road, fences and a garden";
    EXPECT_GT(outerSkin, 50u) << "the canonical house facade was omitted from the hierarchy";
    EXPECT_GT(outsideWindows, 40u) << "weather-facing frames and glazing stayed room-culled";
    EXPECT_EQ(outsideDoors, 8u)
        << "both entries need body, panel and hardware chunks; the garage needs body and panels";
    EXPECT_GT(ground, 0u) << "no chunk of the outdoors is the ground itself";

    // Every instance is inside the hierarchy's own root box, which is the invariant a wrong bounds
    // copy breaks and nothing else here would notice.
    ASSERT_FALSE(scene.bvh.Nodes().empty());
    const BoundingBox root = scene.bvh.Nodes().front().bounds;
    for (const cnahouse::visibility::ExteriorInstance& instance : scene.bvh.Instances())
    {
        EXPECT_GE(instance.bounds.Min.X, root.Min.X);
        EXPECT_LE(instance.bounds.Max.X, root.Max.X);
        EXPECT_GE(instance.bounds.Min.Y, root.Min.Y);
        EXPECT_LE(instance.bounds.Max.Y, root.Max.Y);
    }
}

TEST(ExteriorSceneTests, OnlyTheOutdoorCellsConesReachTheHierarchy)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world";
    }
    const world::WorldData world = LoadWorld();

    // A walk's answer with both kinds in it, and each cell carrying a different number of cones so
    // the result says WHOSE they are and not merely how many there were.
    std::vector<VisibleCell> visible;
    const auto add = [&visible](std::string_view name, std::size_t cones)
    {
        VisibleCell cell;
        cell.cell = cnahouse::util::Intern(std::string(name));
        cell.frustumCount = cones;
        visible.push_back(cell);
    };
    add("L0_KITCHEN", 3);        // a room: no cones, however many it has
    add("EXT_FRONTYARD_E", 2);   // outdoors: both
    add("B1_GYM", 4);            // a basement: none
    add("EXT_BACKYARD", 1);      // outdoors: one
    add("NOT_A_CELL_AT_ALL", 4); // not in the world at all: none, and no crash

    std::vector<ClipFrustum> cones{ClipFrustum{}, ClipFrustum{}};
    GatherExteriorCones(world, visible, cones);
    EXPECT_EQ(cones.size(), 3u) << "the two yards' three cones, and nobody else's";

    // ...and the case that matters most, because it is the one that costs nothing when it is right
    // and costs the whole outdoors when it is wrong: a body in a basement that sees no yard.
    std::vector<VisibleCell> indoors;
    visible.swap(indoors);
    indoors.resize(1);
    indoors.front().cell = cnahouse::util::Intern("B1_GYM");
    indoors.front().frustumCount = 4;
    GatherExteriorCones(world, indoors, cones);
    EXPECT_TRUE(cones.empty()) << "a windowless basement submitted the outdoors to the hierarchy";

    // The output is CLEARED and not appended to: it is reused every frame, so a walk that finds
    // nothing has to leave nothing behind.
    GatherExteriorCones(world, {}, cones);
    EXPECT_TRUE(cones.empty());
}
