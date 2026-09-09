// SPDX-License-Identifier: MIT
//
// `HOUSE-00474`. `CellRuntime` uploads a cell's chunks into real `VertexBuffer`s and
// `IndexBuffer`s, so testing it needs a real `GraphicsDevice`. It lives here rather than in the
// unit suite for that reason alone; nothing below draws anything.
//
// The claim that matters most is the one about the CARRIER. CNA has no generic
// `VertexBuffer::SetData<T>`, so a `dual` chunk -- `Position` `TexCoord0` `TexCoord1`, which is
// what `DualTextureEffect` reads -- is uploaded through `VertexPositionNormalTexture` under a
// declaration that reads its bytes as two texture coordinates. That works because CNA says it
// does: *"this buffer may carry any declaration the caller chose, so every declared element still
// has to fit in the bytes actually uploaded"*. `TheCarriersStreamIsSixFloatsThenTwo` measures the
// stream order that rests on, rather than trusting the comment that states it.
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElementFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElementUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/world/CellRuntime.hpp"
#include "cnahouse/world/ChunkReader.hpp"

namespace
{
    using cnahouse::util::ErrorCode;
    using cnahouse::world::CellRuntime;
    using cnahouse::world::ChunkLayout;
    using cnahouse::world::ChunkLibrary;
    using cnahouse::world::ChunkReader;
    using namespace Microsoft::Xna::Framework::Graphics;

    ChunkLibrary LoadFixture()
    {
        System::IO::FileStream stream(std::string(CNAHOUSE_TEST_CHUNK_FIXTURE),
                                      System::IO::FileMode::Open,
                                      System::IO::FileAccess::Read);
        auto library = ChunkReader::Read(stream, CNAHOUSE_TEST_CHUNK_FIXTURE);
        EXPECT_TRUE(library) << (library ? std::string() : library.Error().Message());
        return library ? std::move(*library) : ChunkLibrary{};
    }

    /// Runs @p body once inside a live device and reports whatever it threw.
    class DeviceHost final : public Microsoft::Xna::Framework::Game
    {
    public:
        explicit DeviceHost(std::function<void(GraphicsDevice&)> body)
            : gdm_(this)
            , body_(std::move(body))
        {
            gdm_.setPreferredBackBufferWidthProperty(64);
            gdm_.setPreferredBackBufferHeightProperty(64);
            gdm_.setSynchronizeWithVerticalRetraceProperty(false);
            setIsFixedTimeStepProperty(false);
        }

        std::string failure;

    protected:
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override
        {
            Game::Draw(gameTime);
            if (done_)
            {
                return;
            }
            done_ = true;
            try
            {
                body_(getGraphicsDeviceProperty());
            }
            catch (const std::exception& e)
            {
                failure = e.what();
            }
            Exit();
        }

    private:
        Microsoft::Xna::Framework::GraphicsDeviceManager gdm_;
        std::function<void(GraphicsDevice&)> body_;
        bool done_ = false;
    };

    void WithDevice(std::function<void(GraphicsDevice&)> body)
    {
        DeviceHost host(std::move(body));
        host.Run();
        ASSERT_TRUE(host.failure.empty()) << host.failure;
    }

} // namespace

TEST(CellRuntimeRenderTests, EveryChunkOfACellBecomesABufferPair)
{
    const ChunkLibrary library = LoadFixture();
    ASSERT_EQ(library.chunks.size(), 3u);
    WithDevice(
        [&library](GraphicsDevice& device)
        {
            CellRuntime runtime(device, library);
            EXPECT_EQ(runtime.ResidentCells(), 0u);
            ASSERT_TRUE(runtime.Load("L0_HALL"));
            ASSERT_TRUE(runtime.Load("L0_LOUNGE"));
            EXPECT_EQ(runtime.ResidentCells(), 2u);
            EXPECT_EQ(runtime.ResidentChunks(), 3u);

            const auto* hall = runtime.Chunks("L0_HALL");
            ASSERT_NE(hall, nullptr);
            ASSERT_EQ(hall->size(), 2u);
            for (const auto& chunk : *hall)
            {
                EXPECT_NE(chunk.vertices.get(), nullptr);
                EXPECT_NE(chunk.indices.get(), nullptr);
                EXPECT_EQ(chunk.primitiveCount, library.chunks[chunk.chunk].indexCount / 3u);
            }
            // Two triangles in the dual chunk, one in the trim: `DrawIndexedPrimitives` counts
            // primitives, not indices, and the difference is a third of the geometry.
            EXPECT_EQ((*hall)[0].primitiveCount, 2u);
            EXPECT_EQ((*hall)[1].primitiveCount, 1u);
        });
}

TEST(CellRuntimeRenderTests, LoadingIsIdempotentAndUnloadingReleases)
{
    const ChunkLibrary library = LoadFixture();
    WithDevice(
        [&library](GraphicsDevice& device)
        {
            CellRuntime runtime(device, library);
            ASSERT_TRUE(runtime.Load("L0_HALL"));
            const std::uint64_t once = runtime.ResidentBytes();
            ASSERT_TRUE(runtime.Load("L0_HALL"));
            EXPECT_EQ(runtime.ResidentCells(), 1u);
            EXPECT_EQ(runtime.ResidentBytes(), once) << "loading a resident cell uploaded it again";

            runtime.Unload("L0_HALL");
            EXPECT_FALSE(runtime.IsResident("L0_HALL"));
            EXPECT_EQ(runtime.Chunks("L0_HALL"), nullptr);
            EXPECT_EQ(runtime.ResidentBytes(), 0u);
            runtime.Unload("L0_HALL"); // Unloading what is not resident does nothing.
            EXPECT_EQ(runtime.ResidentCells(), 0u);

            ASSERT_TRUE(runtime.Load("L0_HALL"));
            ASSERT_TRUE(runtime.Load("L0_LOUNGE"));
            runtime.UnloadAll();
            EXPECT_EQ(runtime.ResidentCells(), 0u);
            EXPECT_EQ(runtime.ResidentChunks(), 0u);
        });
}

TEST(CellRuntimeRenderTests, TheChunkIndexIsTheDrawListsWayBackToTheBuffers)
{
    // `HOUSE-00676`. Residency is per CELL and a draw list is per chunk, and the sort has thrown
    // the cell grouping away on purpose -- so a `RenderItem` naming chunk 7 has to reach chunk 7's
    // two buffers without knowing which room it came from.
    const ChunkLibrary library = LoadFixture();
    ASSERT_EQ(library.chunks.size(), 3u);
    WithDevice(
        [&library](GraphicsDevice& device)
        {
            CellRuntime runtime(device, library);
            EXPECT_TRUE(runtime.ResidentChunkIndices().empty());
            EXPECT_EQ(runtime.Find(0u), nullptr) << "nothing is resident, so nothing is findable";

            ASSERT_TRUE(runtime.Load("L0_LOUNGE"));
            ASSERT_TRUE(runtime.Load("L0_HALL"));
            // ASCENDING, and therefore the file's order rather than the order the cells arrived
            // in: two sessions that loaded the same rooms the other way round must submit the
            // same frame.
            const std::vector<std::uint32_t> indices(runtime.ResidentChunkIndices().begin(),
                                                     runtime.ResidentChunkIndices().end());
            EXPECT_EQ(indices, (std::vector<std::uint32_t>{0u, 1u, 2u}));

            for (const std::uint32_t index : indices)
            {
                const auto* found = runtime.Find(index);
                ASSERT_NE(found, nullptr) << "chunk " << index;
                EXPECT_EQ(found->chunk, index) << "the index found somebody else's chunk";
                EXPECT_NE(found->vertices.get(), nullptr);
                EXPECT_EQ(found->primitiveCount, library.chunks[index].indexCount / 3u);
            }
            EXPECT_EQ(runtime.Find(static_cast<std::uint32_t>(library.chunks.size())), nullptr)
                << "an index past the file must not be dereferenced";
            EXPECT_EQ(runtime.Find(4000000u), nullptr);

            // Unloading takes its chunks out of both answers, rather than leaving a pointer into
            // a vector that has just been erased.
            const auto* hall = runtime.Chunks("L0_HALL");
            ASSERT_NE(hall, nullptr);
            const std::uint32_t gone = hall->front().chunk;
            runtime.Unload("L0_HALL");
            EXPECT_EQ(runtime.Find(gone), nullptr);
            EXPECT_EQ(runtime.ResidentChunkIndices().size(), runtime.ResidentChunks());

            runtime.UnloadAll();
            EXPECT_TRUE(runtime.ResidentChunkIndices().empty());
        });
}

TEST(CellRuntimeRenderTests, TheResidentIndicesAreTheFilesOrderAndNotTheCellMaps)
{
    // The guarantee the fixture cannot check. `chunks.bin` happens to list its chunks grouped by
    // cell in the same order the cell table sorts them, so walking the resident MAP gives ascending
    // indices there by coincidence. Nothing in `docs/chunk-format.md` promises that, so this
    // library is built by hand with the two orders deliberately disagreeing: cell `A` owns chunk 1
    // and cell `B` owns chunks 0 and 2.
    ChunkLibrary library;
    library.cells = {"A_ROOM", "B_ROOM"};
    library.materials = {"BLOCKOUT_wall"};
    for (const std::uint16_t cell : {std::uint16_t{1}, std::uint16_t{0}, std::uint16_t{1}})
    {
        cnahouse::world::Chunk chunk;
        chunk.cell = cell;
        chunk.material = 0u;
        chunk.layout = ChunkLayout::Basic;
        chunk.vertexCount = 3u;
        chunk.vertices.assign(3u * cnahouse::world::ChunkVertexStride(ChunkLayout::Basic), 0u);
        chunk.indexCount = 3u;
        chunk.indices.assign(3u * sizeof(std::uint16_t), 0u);
        library.chunks.push_back(std::move(chunk));
    }

    WithDevice(
        [&library](GraphicsDevice& device)
        {
            CellRuntime runtime(device, library);
            // Loaded in the order that makes the mistake visible: the cell holding the LATER
            // chunks first.
            ASSERT_TRUE(runtime.Load("B_ROOM"));
            ASSERT_TRUE(runtime.Load("A_ROOM"));
            const std::vector<std::uint32_t> indices(runtime.ResidentChunkIndices().begin(),
                                                     runtime.ResidentChunkIndices().end());
            EXPECT_EQ(indices, (std::vector<std::uint32_t>{0u, 1u, 2u}))
                << "the cell map's order reached the draw list instead of the file's";
            for (const std::uint32_t index : indices)
            {
                ASSERT_NE(runtime.Find(index), nullptr) << index;
                EXPECT_EQ(runtime.Find(index)->chunk, index);
            }
        });
}

TEST(CellRuntimeRenderTests, ACellTheFileDoesNotHaveIsNotFound)
{
    const ChunkLibrary library = LoadFixture();
    WithDevice(
        [&library](GraphicsDevice& device)
        {
            CellRuntime runtime(device, library);
            const auto result = runtime.Load("L9_NOWHERE");
            ASSERT_FALSE(result);
            EXPECT_EQ(result.Error().Code(), ErrorCode::NotFound);
            EXPECT_NE(result.Error().Message().find("L9_NOWHERE"), std::string::npos);
            EXPECT_EQ(runtime.ResidentCells(), 0u);
        });
}

TEST(CellRuntimeRenderTests, TheDualLayoutUploadsFourBytesWiderThanTheFileStores)
{
    // The file packs a `dual` vertex in 28 bytes. The only public upload path wide enough carries
    // 32, so four bytes a vertex are uploaded and never read. `ResidentBytes` counts what was
    // actually uploaded, which is the number that matters for VRAM and the one that would
    // otherwise be quietly assumed to be the file's.
    const ChunkLibrary library = LoadFixture();
    EXPECT_EQ(CellRuntime::UploadStride(ChunkLayout::Basic), 32u);
    EXPECT_EQ(CellRuntime::UploadStride(ChunkLayout::Dual), 32u);
    EXPECT_EQ(CellRuntime::UploadStride(ChunkLayout::AlphaTest), 20u);
    WithDevice(
        [&library](GraphicsDevice& device)
        {
            CellRuntime runtime(device, library);
            ASSERT_TRUE(runtime.Load("L0_HALL"));
            // 4 dual vertices at 32 + 6 indices at 2, then 3 basic at 32 + 3 indices at 2.
            EXPECT_EQ(runtime.ResidentBytes(), 4u * 32u + 6u * 2u + 3u * 32u + 3u * 2u);
            ASSERT_TRUE(runtime.Load("L0_LOUNGE"));
            EXPECT_EQ(runtime.ResidentBytes(), 4u * 32u + 6u * 2u + 3u * 32u + 3u * 2u + 3u * 20u + 3u * 2u);
        });
}

TEST(CellRuntimeRenderTests, EachLayoutDeclaresWhatItsOwnEffectReads)
{
    const auto& basic = CellRuntime::DeclarationFor(ChunkLayout::Basic);
    const auto& dual = CellRuntime::DeclarationFor(ChunkLayout::Dual);
    const auto& alpha = CellRuntime::DeclarationFor(ChunkLayout::AlphaTest);
    EXPECT_EQ(basic.getVertexStrideProperty(), 32);
    EXPECT_EQ(dual.getVertexStrideProperty(), 32);
    EXPECT_EQ(alpha.getVertexStrideProperty(), 20);

    // `basic` has a normal and one texture coordinate; `dual` has no normal and two.
    const auto& basicElements = basic.GetVertexElements();
    ASSERT_EQ(basicElements.size(), 3u);
    EXPECT_EQ(basicElements[1].getVertexElementUsageProperty(), VertexElementUsage::Normal);

    const auto& dualElements = dual.GetVertexElements();
    ASSERT_EQ(dualElements.size(), 3u);
    for (const auto& element : dualElements)
    {
        EXPECT_NE(element.getVertexElementUsageProperty(), VertexElementUsage::Normal)
            << "DualTextureEffect is unlit -- the lightmap IS the lighting";
    }
    EXPECT_EQ(dualElements[1].getVertexElementUsageProperty(), VertexElementUsage::TextureCoordinate);
    EXPECT_EQ(dualElements[1].getUsageIndexProperty(), 0);
    EXPECT_EQ(dualElements[1].getOffsetProperty(), 12);
    EXPECT_EQ(dualElements[2].getUsageIndexProperty(), 1);
    EXPECT_EQ(dualElements[2].getOffsetProperty(), 20);
    // Both UV elements are `Vector2`, so together they cover bytes 12..28 of the 32 uploaded.
    EXPECT_EQ(dualElements[2].getVertexElementFormatProperty(), VertexElementFormat::Vector2);
}

TEST(CellRuntimeRenderTests, TheCarriersStreamIsSixFloatsThenTwo)
{
    // The whole dual path rests on `VertexPositionNormalTexture` reaching the GPU as
    // `x y z nx ny nz u v`, eight floats in that order. Nothing in this project can see the GPU's
    // copy, but `GetData` reads back CNA's own shadow of exactly those bytes, so a change to the
    // stream layout would fail here rather than in a frame nobody looks at closely.
    WithDevice(
        [](GraphicsDevice& device)
        {
            const VertexDeclaration declaration = CellRuntime::DeclarationFor(ChunkLayout::Dual);
            VertexBuffer buffer(device, declaration, 2, BufferUsage::None);
            const VertexPositionNormalTexture written[2] = {
                VertexPositionNormalTexture(Microsoft::Xna::Framework::Vector3(1.0f, 2.0f, 3.0f),
                                            Microsoft::Xna::Framework::Vector3(0.125f, 0.25f, 0.5f),
                                            Microsoft::Xna::Framework::Vector2(0.75f, 0.875f)),
                VertexPositionNormalTexture(Microsoft::Xna::Framework::Vector3(4.0f, 5.0f, 6.0f),
                                            Microsoft::Xna::Framework::Vector3(0.0625f, 0.375f, 0.625f),
                                            Microsoft::Xna::Framework::Vector2(0.9375f, 0.96875f))};
            buffer.SetData(written, 2);

            VertexPositionNormalTexture read[2] = {};
            buffer.GetData(read, 2);
            // The six floats the dual declaration reads as Position + TEXCOORD0 + the first half of
            // TEXCOORD1 come back exactly where they were put.
            EXPECT_FLOAT_EQ(read[0].Position.X, 1.0f);
            EXPECT_FLOAT_EQ(read[0].Position.Z, 3.0f);
            EXPECT_FLOAT_EQ(read[0].Normal.X, 0.125f);           // TEXCOORD0.x, at byte 12
            EXPECT_FLOAT_EQ(read[0].Normal.Y, 0.25f);            // TEXCOORD0.y, at byte 16
            EXPECT_FLOAT_EQ(read[0].Normal.Z, 0.5f);             // TEXCOORD1.x, at byte 20
            EXPECT_FLOAT_EQ(read[0].TextureCoordinate.X, 0.75f); // TEXCOORD1.y, at byte 24
            EXPECT_FLOAT_EQ(read[1].Position.X, 4.0f);
            EXPECT_FLOAT_EQ(read[1].Normal.Z, 0.625f);
            EXPECT_FLOAT_EQ(read[1].TextureCoordinate.X, 0.9375f);
        });
}

TEST(CellRuntimeRenderTests, TheDualChunksUvSetsArriveInTheDeclaredSlots)
{
    // End to end over the real file: the fixture's first dual vertex has TEXCOORD0 (0.125, 0.25)
    // and TEXCOORD1 (0.5, 0.75), and the declaration puts them at bytes 12 and 20. Reading the
    // buffer back through the carrier is reading those bytes.
    const ChunkLibrary library = LoadFixture();
    WithDevice(
        [&library](GraphicsDevice& device)
        {
            CellRuntime runtime(device, library);
            ASSERT_TRUE(runtime.Load("L0_HALL"));
            const auto* hall = runtime.Chunks("L0_HALL");
            ASSERT_NE(hall, nullptr);
            ASSERT_FALSE(hall->empty());
            ASSERT_EQ(library.chunks[(*hall)[0].chunk].layout, ChunkLayout::Dual);
            // A `WriteOnly` buffer cannot be read back, which is the right usage for geometry uploaded
            // once; the bytes it was given are asserted through the carrier above and through
            // `ChunkRoundTripTests` on the way in, so what is left to check here is the shape.
            EXPECT_EQ(library.chunks[(*hall)[0].chunk].vertexCount, 4u);
            EXPECT_EQ((*hall)[0].bytes, 4u * 32u + 6u * 2u);
        });
}
