// SPDX-License-Identifier: MIT
//
// `HOUSE-00675`. §25.1's step 5: *"sort by pass/effect/material"*.
//
// The list is the last thing visibility does and the first thing the renderer reads, so what is
// checked here is the ORDER and nothing about drawing: that the key is the one §25.1 names, that
// §7.5's transparent pass overrides it, that sorting actually removes state changes rather than
// being a no-op over content that happened to arrive in order, and that the result is the same on
// every run -- a render-regression fixture compares pixels, and two runs that submit the same
// geometry in a different order are two different frames.
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/ChunkCulling.hpp"
#include "cnahouse/visibility/RenderList.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/world/ChunkReader.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::app::FrameContext;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::rendering::Pass;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ChunkCuller;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::RenderItem;
    using cnahouse::visibility::RenderList;
    using cnahouse::visibility::VisibilitySystem;
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

    CameraView Standing(const world::WorldData& data, std::string_view cellName, float yawDegrees)
    {
        CameraView view;
        view.cell = cnahouse::util::Intern(std::string(cellName));
        const world::Cell* cell = data.FindCell(view.cell);
        EXPECT_NE(cell, nullptr) << cellName;
        if (cell == nullptr)
        {
            return view;
        }
        const world::Level* level = data.FindLevel(cell->level);
        const world::Footprint& box = cell->boxes.front();

        PlayerState state;
        state.position = Vector3((box.minX + box.maxX) * 0.5F,
                                 (level == nullptr ? 0.0F : level->ffl) + state.Rise(),
                                 (box.minZ + box.maxZ) * 0.5F);
        state.yaw = yawDegrees * 3.14159265F / 180.0F;
        FirstPersonCamera camera;
        camera.SetAspect(16.0F / 9.0F);
        camera.Update(state, kPlayerEyeHeight, 0.0F);

        view.eye = camera.Pose().eye;
        view.viewProjection = camera.View() * camera.Projection();
        view.frustum = ClipFrustum(camera.Frustum());
        view.nearPlane = camera.Frustum().getNearProperty();
        view.farPlane = camera.Frustum().getFarProperty();
        return view;
    }

    FrameContext Frame(std::uint64_t index)
    {
        FrameContext frame;
        frame.frameIndex = index;
        return frame;
    }

    /// The sort key of §25.1's step 5, for the passes that use it.
    std::tuple<int, int, int, std::uint32_t> Key(const RenderItem& item)
    {
        return {static_cast<int>(item.pass),
                static_cast<int>(item.effect),
                static_cast<int>(item.material),
                item.geometry};
    }

    /// Every chunk in the house, so the list is asked the worst question §71.2 has an answer for.
    std::vector<std::uint32_t> AllChunks(const world::ChunkLibrary& library)
    {
        std::vector<std::uint32_t> all(library.chunks.size());
        for (std::uint32_t i = 0; i < library.chunks.size(); ++i)
        {
            all[i] = i;
        }
        return all;
    }

    RenderItem Item(Pass pass, world::EffectTier effect, std::uint16_t material, std::uint32_t geometry)
    {
        RenderItem item;
        item.pass = pass;
        item.effect = effect;
        item.material = material;
        item.geometry = geometry;
        return item;
    }

} // namespace

TEST(RenderListTests, ThePassAndTheEffectAreReadOffTheLayoutRatherThanGuessed)
{
    // `docs/chunk-format.md` §3: the layout COMES FROM the material's `effectTierS`, so these two
    // functions read back what the content build wrote rather than deciding anything.
    EXPECT_EQ(cnahouse::visibility::EffectForLayout(world::ChunkLayout::Basic), world::EffectTier::Basic);
    EXPECT_EQ(cnahouse::visibility::EffectForLayout(world::ChunkLayout::Dual),
              world::EffectTier::DualTexture);
    EXPECT_EQ(cnahouse::visibility::EffectForLayout(world::ChunkLayout::AlphaTest),
              world::EffectTier::AlphaTest);

    // §7.5's E comes before F so its cut-outs write the depth the sorted pass tests against, and
    // `alphatest` is the one alpha mode `chunks.bin` carries -- because it is a different vertex
    // layout and not merely a different blend state.
    EXPECT_EQ(cnahouse::visibility::PassForLayout(world::ChunkLayout::AlphaTest), Pass::AlphaTest);
    EXPECT_EQ(cnahouse::visibility::PassForLayout(world::ChunkLayout::Basic), Pass::OpaqueStatic);
    EXPECT_EQ(cnahouse::visibility::PassForLayout(world::ChunkLayout::Dual), Pass::OpaqueStatic);
}

TEST(RenderListTests, EveryVisibleChunkBecomesOneItemCarryingItsOwnKey)
{
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    const world::WorldData data = LoadWorld();
    auto library = world::ChunkReader::ReadFromTitle("content/world/chunks.bin");
    ASSERT_TRUE(library) << library.Error().ToString();

    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }
    const CameraView view = Standing(data, "L0_HALL", 0.0F);
    system.SetCamera(view);
    system.Update(Frame(1));

    ChunkCuller culler(*library);
    culler.Cull(system.Visible());

    RenderList list;
    list.AddChunks(*library, culler.Chunks(), view.eye);

    ASSERT_EQ(list.Size(), culler.Chunks().size()) << "a chunk was dropped or duplicated";
    ASSERT_EQ(static_cast<std::size_t>(list.DrawCalls()), culler.Chunks().size());

    // The item's key is the chunk's own, not something derived a second time.
    std::size_t at = 0;
    for (const std::uint32_t index : culler.Chunks())
    {
        const RenderItem& item = list.Items()[at++];
        const world::Chunk& chunk = library->chunks[index];
        EXPECT_EQ(item.geometry, index);
        EXPECT_EQ(item.material, chunk.material);
        EXPECT_EQ(item.effect, cnahouse::visibility::EffectForLayout(chunk.layout));
        EXPECT_EQ(item.pass, cnahouse::visibility::PassForLayout(chunk.layout));
    }
    EXPECT_FALSE(list.IsSorted()) << "adding must not claim the list is in order";
}

TEST(RenderListTests, TheKeyIsPassThenEffectThenMaterialThenGeometry)
{
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    auto library = world::ChunkReader::ReadFromTitle("content/world/chunks.bin");
    ASSERT_TRUE(library) << library.Error().ToString();

    RenderList list;
    list.AddChunks(*library, AllChunks(*library), Vector3(0.0F, 1.68F, 0.0F));

    // Plus the passes the blockout has no geometry for yet: §7.5's D and E, added out of order and
    // interleaved, so the FIRST key is exercised by more than one value.
    list.Add(Item(Pass::AlphaTest, world::EffectTier::AlphaTest, 7, 900));
    list.Add(Item(Pass::OpaqueDynamic, world::EffectTier::Skinned, 3, 902));
    list.Add(Item(Pass::AlphaTest, world::EffectTier::AlphaTest, 2, 901));
    list.Add(Item(Pass::OpaqueDynamic, world::EffectTier::Basic, 9, 903));
    list.Sort();

    EXPECT_TRUE(list.IsSorted());
    ASSERT_GT(list.Size(), 4U);
    for (std::size_t i = 1; i < list.Size(); ++i)
    {
        EXPECT_LE(Key(list.Items()[i - 1]), Key(list.Items()[i])) << "item " << i << " is out of order";
    }

    // §7.5's order, and the sort cannot invent one: the passes come out in the enum's order because
    // the enum's order IS the frame's.
    Pass previous = Pass::Shadow;
    for (const RenderItem& item : list.Items())
    {
        EXPECT_LE(static_cast<int>(previous), static_cast<int>(item.pass));
        previous = item.pass;
    }
}

TEST(RenderListTests, SortingCollapsesTheStateChangesToOnePerDistinctKey)
{
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    auto library = world::ChunkReader::ReadFromTitle("content/world/chunks.bin");
    ASSERT_TRUE(library) << library.Error().ToString();

    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }
    const CameraView view = Standing(data, "L0_HALL", 0.0F);
    system.SetCamera(view);
    system.Update(Frame(1));

    ChunkCuller culler(*library);
    culler.Cull(system.Visible());

    RenderList list;
    list.AddChunks(*library, culler.Chunks(), view.eye);
    // The order the visible set arrives in: cell by cell, and every cell has a floor, walls and a
    // ceiling, so the same handful of materials is revisited once per room.
    const int unsorted = list.StateChanges();
    list.Sort();
    const int sorted = list.StateChanges();

    std::set<std::tuple<int, int, int>> distinct;
    for (const RenderItem& item : list.Items())
    {
        distinct.insert({static_cast<int>(item.pass), static_cast<int>(item.effect), item.material});
    }

    std::printf("  from L0_HALL: %zu chunks, %d state changes as they arrive, %d sorted, "
                "%zu distinct (pass, effect, material) keys\n",
                list.Size(),
                unsorted,
                sorted,
                distinct.size());

    ASSERT_FALSE(distinct.empty());
    // The minimum the content allows: one per distinct key, less the first.
    EXPECT_EQ(sorted, static_cast<int>(distinct.size()) - 1);
    EXPECT_LT(sorted, unsorted) << "the sort changed nothing, so it is either broken or unnecessary";
}

TEST(RenderListTests, TheWholeHouseAtOnceFitsSection71Point2sBudget)
{
    // The worst question the static list can be asked: no culling at all, every chunk in the house.
    // If THAT fits, the culled frame does.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    auto library = world::ChunkReader::ReadFromTitle("content/world/chunks.bin");
    ASSERT_TRUE(library) << library.Error().ToString();

    RenderList list;
    list.AddChunks(*library, AllChunks(*library), Vector3(0.0F, 1.68F, 0.0F));
    list.Sort();

    std::array<int, static_cast<std::size_t>(Pass::Count)> perPass{};
    for (const RenderItem& item : list.Items())
    {
        ++perPass[static_cast<std::size_t>(item.pass)];
    }
    std::printf("  the whole house: %d draw calls, %d state changes (opaque static %d, alpha test %d)\n",
                list.DrawCalls(),
                list.StateChanges(),
                perPass[static_cast<std::size_t>(Pass::OpaqueStatic)],
                perPass[static_cast<std::size_t>(Pass::AlphaTest)]);

    // §71.2: 620 draw calls typically, 90 state changes typically.
    EXPECT_LE(list.DrawCalls(), 620);
    EXPECT_LE(list.StateChanges(), 90);
}

TEST(RenderListTests, TheTransparentPassIsBackToFrontByCellThenObjectWhateverItsMaterialsAre)
{
    // §23.6: a cell is the coarse key and an object the fine one. The object at 100 m deliberately
    // belongs to the nearer cell: a global object sort would put it first, while the required cell
    // grouping puts both objects of the farther cell before it.
    RenderList list;
    for (const auto& [material, cellDepth, objectDepth] :
         {std::tuple<std::uint16_t, float, float>{1, 10.0F, 100.0F},
          std::tuple<std::uint16_t, float, float>{2, 20.0F, 2.0F},
          std::tuple<std::uint16_t, float, float>{3, 20.0F, 9.0F}})
    {
        RenderItem item = Item(Pass::Transparent, world::EffectTier::Basic, material, material);
        item.cellDepth = cellDepth;
        item.objectDepth = objectDepth;
        list.Add(item);
    }
    list.Sort();

    ASSERT_EQ(list.Size(), 3U);
    EXPECT_EQ(list.Items()[0].material, 3) << "the farther object in the farther cell goes first";
    EXPECT_EQ(list.Items()[1].material, 2) << "the farther cell must remain contiguous";
    EXPECT_EQ(list.Items()[2].material, 1) << "global object distance overrode cell distance";

    // And the opaque passes do NOT sort by depth: front-to-back would be fewer overdrawn pixels and
    // more state changes, and §25.1 chose the state changes.
    RenderList opaque;
    for (const auto& [material, depth] :
         {std::pair<std::uint16_t, float>{5, 1.0F}, std::pair<std::uint16_t, float>{4, 30.0F}})
    {
        RenderItem item = Item(Pass::OpaqueStatic, world::EffectTier::Basic, material, material);
        item.objectDepth = depth;
        opaque.Add(item);
    }
    opaque.Sort();
    EXPECT_EQ(opaque.Items()[0].material, 4) << "an opaque pass sorted by depth, not by material";
}

TEST(RenderListTests, ItemsForIsThatPassAndOnlyThatPassAndSortsIfItMust)
{
    RenderList list;
    list.Add(Item(Pass::Transparent, world::EffectTier::Basic, 1, 10));
    list.Add(Item(Pass::OpaqueStatic, world::EffectTier::DualTexture, 2, 11));
    list.Add(Item(Pass::AlphaTest, world::EffectTier::AlphaTest, 3, 12));
    list.Add(Item(Pass::OpaqueStatic, world::EffectTier::Basic, 4, 13));
    list.Add(Item(Pass::AlphaTest, world::EffectTier::AlphaTest, 5, 14));

    ASSERT_FALSE(list.IsSorted());
    EXPECT_EQ(list.ItemsFor(Pass::OpaqueStatic).size(), 2U);
    EXPECT_TRUE(list.IsSorted()) << "an unsorted list must not be binary-searched";

    // Every slice, and its CONTENTS: a slice of the right length taken from the wrong offset is
    // the failure this checks for -- the first pass present starts at index 0, so a length alone
    // cannot tell a correct offset from none at all.
    std::size_t total = 0;
    for (int i = 0; i < static_cast<int>(Pass::Count); ++i)
    {
        const Pass pass = static_cast<Pass>(i);
        const std::span<const RenderItem> slice = list.ItemsFor(pass);
        total += slice.size();
        for (const RenderItem& item : slice)
        {
            EXPECT_EQ(item.pass, pass) << "the slice for " << cnahouse::rendering::PassName(pass)
                                       << " carries an item from another pass";
        }
    }
    EXPECT_EQ(total, list.Size()) << "the slices must partition the list";

    EXPECT_EQ(list.ItemsFor(Pass::AlphaTest).size(), 2U);
    EXPECT_EQ(list.ItemsFor(Pass::Transparent).size(), 1U);
    EXPECT_TRUE(list.ItemsFor(Pass::Sky).empty()) << "a pass with nothing in it is an empty slice";
    EXPECT_TRUE(list.ItemsFor(Pass::Hud).empty()) << "and so is the last one";
}

TEST(RenderListTests, ClearKeepsTheMemorySoASteadyFrameAllocatesNothing)
{
    RenderList list;
    for (std::uint32_t i = 0; i < 200; ++i)
    {
        list.Add(Item(Pass::OpaqueStatic, world::EffectTier::Basic, static_cast<std::uint16_t>(i), i));
    }
    const std::size_t capacity = list.Capacity();
    ASSERT_GE(capacity, 200U);

    list.Clear();
    EXPECT_EQ(list.Size(), 0U);
    EXPECT_EQ(list.DrawCalls(), 0);
    EXPECT_EQ(list.StateChanges(), 0);
    EXPECT_TRUE(list.IsSorted()) << "an empty list is in order";
    // The buffer, not the address: an allocator handed a block back will hand the same one out
    // again, so a pointer comparison would pass over a `Clear` that freed everything.
    EXPECT_EQ(list.Capacity(), capacity) << "Clear released the buffer, so every frame allocates";

    for (std::uint32_t i = 0; i < 200; ++i)
    {
        list.Add(Item(Pass::OpaqueStatic, world::EffectTier::Basic, static_cast<std::uint16_t>(i), i));
    }
    EXPECT_EQ(list.Capacity(), capacity) << "refilling to the same size reallocated";
}

TEST(RenderListTests, AStateChangeIsAnyOfThreeThingsChanging)
{
    // Injected and missed until this test existed: counting only the MATERIAL passes over the real
    // house, where every material has exactly one effect and every chunk is opaque, so the three
    // components change together and cannot be told apart. Here they are deliberately pulled apart
    // -- one material id drawn by two effects in two passes -- because the renderer rebinds on any
    // of the three and a count that watches one of them under-reports §71.2's budget.
    RenderList list;
    list.Add(Item(Pass::OpaqueStatic, world::EffectTier::Basic, 4, 0));
    list.Add(Item(Pass::OpaqueStatic, world::EffectTier::DualTexture, 4, 1));
    list.Add(Item(Pass::AlphaTest, world::EffectTier::DualTexture, 4, 2));
    list.Sort();

    ASSERT_EQ(list.Size(), 3U);
    // One material throughout: the effect changes once and the pass once.
    EXPECT_EQ(list.StateChanges(), 2);

    // And the same three items under one effect and one pass, differing only in material, cost the
    // same -- the count does not weigh the three components, it counts them.
    RenderList byMaterial;
    byMaterial.Add(Item(Pass::OpaqueStatic, world::EffectTier::Basic, 4, 0));
    byMaterial.Add(Item(Pass::OpaqueStatic, world::EffectTier::Basic, 5, 1));
    byMaterial.Add(Item(Pass::OpaqueStatic, world::EffectTier::Basic, 6, 2));
    byMaterial.Sort();
    EXPECT_EQ(byMaterial.StateChanges(), 2);

    // Three draws that change nothing are no state changes at all.
    RenderList same;
    for (std::uint32_t i = 0; i < 3; ++i)
    {
        same.Add(Item(Pass::OpaqueStatic, world::EffectTier::Basic, 4, i));
    }
    same.Sort();
    EXPECT_EQ(same.StateChanges(), 0);
}

TEST(RenderListTests, AChunkIndexPastTheLibraryIsSkippedRatherThanDereferenced)
{
    // The visible set and the chunk file agree only because the content build says so
    // (`worldHash`); a mismatch must not be a read past the end of a vector.
    world::ChunkLibrary library;
    library.materials = {"BLOCKOUT_wall"};
    library.cells = {"L0_HALL"};
    world::Chunk chunk;
    chunk.material = 0;
    chunk.layout = world::ChunkLayout::Dual;
    chunk.bounds =
        Microsoft::Xna::Framework::BoundingBox(Vector3(0.0F, 0.0F, 0.0F), Vector3(2.0F, 2.0F, 2.0F));
    library.chunks.push_back(chunk);

    RenderList list;
    const std::array<std::uint32_t, 3> indices{0u, 1u, 4000000u};
    list.AddChunks(library, indices, Vector3(0.0F, 0.0F, 0.0F));
    EXPECT_EQ(list.Size(), 1U);
    EXPECT_EQ(list.Items()[0].geometry, 0u);
}

TEST(RenderListTests, TheObjectAndCellDepthsAreMetresFromTheEyeToTheirCentres)
{
    world::ChunkLibrary library;
    library.materials = {"BLOCKOUT_wall"};
    library.cells = {"L0_HALL"};
    world::Chunk chunk;
    chunk.bounds =
        Microsoft::Xna::Framework::BoundingBox(Vector3(2.0F, 0.0F, -1.0F), Vector3(4.0F, 4.0F, 1.0F));
    library.chunks.push_back(chunk);

    RenderList list;
    const std::array<std::uint32_t, 1> indices{0u};
    // The centre is (3, 2, 0); the eye is 3 m below it and 4 m to its west, so 5 m away.
    list.AddChunks(library, indices, Vector3(-1.0F, -1.0F, 0.0F));
    ASSERT_EQ(list.Size(), 1U);
    EXPECT_FLOAT_EQ(list.Items()[0].objectDepth, 5.0F);
    EXPECT_FLOAT_EQ(list.Items()[0].cellDepth, 5.0F);
}

TEST(RenderListTests, BlendMaterialsEnterTransparencyThroughTheRuntimeRegistry)
{
    IdRegistry::ResetForTesting();
    world::WorldData::Contents contents;
    for (const auto& [name, alphaMode] :
         {std::pair<std::string_view, world::AlphaMode>{"MAT_OPAQUE", world::AlphaMode::Opaque},
          std::pair<std::string_view, world::AlphaMode>{"MAT_BLEND", world::AlphaMode::Blend},
          std::pair<std::string_view, world::AlphaMode>{"MAT_MASK", world::AlphaMode::Mask}})
    {
        world::MaterialDef material;
        material.id = cnahouse::util::Intern(name);
        material.alphaMode = alphaMode;
        contents.materials.push_back(material);
    }
    auto built = world::WorldData::Create(std::move(contents));
    ASSERT_TRUE(built) << built.Error().ToString();

    world::ChunkLibrary library;
    library.cells = {"FAR_CELL", "NEAR_CELL"};
    library.materials = {"MAT_OPAQUE", "MAT_BLEND", "MAT_MASK"};
    auto addChunk = [&](std::uint16_t cell, std::uint16_t material, world::ChunkLayout layout, float x)
    {
        world::Chunk chunk;
        chunk.cell = cell;
        chunk.material = material;
        chunk.layout = layout;
        chunk.bounds = Microsoft::Xna::Framework::BoundingBox(Vector3(x - 1.0F, 0.0F, -1.0F),
                                                              Vector3(x + 1.0F, 2.0F, 1.0F));
        library.chunks.push_back(std::move(chunk));
    };
    addChunk(0u, 1u, world::ChunkLayout::Basic, 12.0F);
    addChunk(0u, 1u, world::ChunkLayout::Basic, 8.0F);
    addChunk(1u, 0u, world::ChunkLayout::Basic, 2.0F);
    addChunk(1u, 2u, world::ChunkLayout::AlphaTest, 3.0F);

    const std::array<std::uint32_t, 4> indices{0u, 1u, 2u, 3u};
    RenderList list;
    list.AddChunks(library, indices, Vector3(0.0F, 1.0F, 0.0F), &*built);
    list.Sort();

    const auto transparent = list.ItemsFor(Pass::Transparent);
    ASSERT_EQ(transparent.size(), 2U);
    EXPECT_FLOAT_EQ(transparent[0].cellDepth, transparent[1].cellDepth)
        << "two objects of one cell need one coarse sort key";
    EXPECT_GT(transparent[0].objectDepth, transparent[1].objectDepth)
        << "objects inside the cell are ordered farthest first";
    EXPECT_EQ(list.ItemsFor(Pass::OpaqueStatic).size(), 1U);
    EXPECT_EQ(list.ItemsFor(Pass::AlphaTest).size(), 1U)
        << "the distinct alpha-test layout remains the earlier depth-writing pass";

    RenderList withoutRegistry;
    withoutRegistry.AddChunks(library, indices, Vector3(0.0F, 1.0F, 0.0F));
    EXPECT_TRUE(withoutRegistry.ItemsFor(Pass::Transparent).empty())
        << "a missing registry must not be replaced by a material-name guess";
}

TEST(RenderListTests, ItemsTheKeyCannotSeparateKeepTheOrderTheyArrivedIn)
{
    // A stable sort, deliberately: the visible set is built in a defined order, and two transparent
    // panes at the same distance must not swap between two runs of the same binary -- a render
    // fixture compares pixels, and the nearer-drawn one is the one on top.
    //
    // **Forty and not four**, because four would prove nothing: libstdc++'s introsort falls back to
    // insertion sort at sixteen elements and is stable there by accident, so a list short enough to
    // insertion-sort cannot tell a stable sort from an unstable one. Above the threshold it
    // partitions, and an unstable sort visibly shuffles equal keys.
    constexpr int kCount = 40;
    RenderList list;
    for (int i = 0; i < kCount; ++i)
    {
        // Descending materials, so an unstable sort that happened to leave them in the order a
        // material key would want is still a failure here.
        RenderItem item = Item(Pass::Transparent,
                               world::EffectTier::Basic,
                               static_cast<std::uint16_t>(kCount - i),
                               static_cast<std::uint32_t>(kCount - i));
        item.cellDepth = 3.0F;
        item.objectDepth = 3.0F;
        list.Add(item);
    }
    list.Sort();

    ASSERT_EQ(list.Size(), static_cast<std::size_t>(kCount));
    for (int i = 0; i < kCount; ++i)
    {
        EXPECT_EQ(list.Items()[static_cast<std::size_t>(i)].material, kCount - i)
            << "equal keys were reordered at " << i;
    }
}

TEST(RenderListTests, TheSameFrameBuiltTwiceIsTheSameList)
{
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    auto library = world::ChunkReader::ReadFromTitle("content/world/chunks.bin");
    ASSERT_TRUE(library) << library.Error().ToString();

    const Vector3 eye(1.5F, 1.68F, -3.0F);
    RenderList first;
    RenderList second;
    first.AddChunks(*library, AllChunks(*library), eye);
    first.Sort();
    second.AddChunks(*library, AllChunks(*library), eye);
    second.Sort();

    ASSERT_EQ(first.Size(), second.Size());
    for (std::size_t i = 0; i < first.Size(); ++i)
    {
        EXPECT_EQ(Key(first.Items()[i]), Key(second.Items()[i])) << "item " << i;
        EXPECT_FLOAT_EQ(first.Items()[i].cellDepth, second.Items()[i].cellDepth);
        EXPECT_FLOAT_EQ(first.Items()[i].objectDepth, second.Items()[i].objectDepth);
    }
}
