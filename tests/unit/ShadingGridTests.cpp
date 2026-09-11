// SPDX-License-Identifier: MIT
//
// `HOUSE-01263`. The reader for `shading.bin` — §28.4's fourth factor, and the file
// `docs/shading-format.md` said *"the runtime reader is `HOUSE-01263`'s daylight model and does
// not exist yet"* about until now.
//
// Two kinds of claim here and they are different in kind. The **format** claims are made against
// bytes this file builds, because a reader is only as good as what it refuses and a truncated or
// forked file has to be rejected rather than half-read. The **interpolation** claims are made
// against the grid's own arithmetic — a node is read back exactly, a midpoint is the mean of its
// neighbours, the azimuth wraps and the altitude clamps — because those are properties, not
// numbers, and a property survives a re-bake where a number does not.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/MemoryStream.hpp"

#include "cnahouse/lighting/ShadingGrid.hpp"

namespace
{
    using cnahouse::lighting::ShadingGrid;
    using cnahouse::lighting::WindowShading;
    using cnahouse::util::Id;
    using cnahouse::util::IdRegistry;

    constexpr const char* kBaked = "content/world/shading.bin";

    /// A well-formed one-window file, built here so a test can then damage one field of it.
    struct Builder
    {
        std::vector<std::uint8_t> bytes;

        void U32(std::uint32_t value)
        {
            for (int shift = 0; shift < 32; shift += 8)
            {
                bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
            }
        }

        void U16(std::uint16_t value)
        {
            bytes.push_back(static_cast<std::uint8_t>(value & 0xFFu));
            bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
        }

        void F32(float value)
        {
            std::uint32_t raw = 0;
            static_assert(sizeof(raw) == sizeof(value));
            std::memcpy(&raw, &value, sizeof(raw));
            U32(raw);
        }

        void Name(const std::string& text)
        {
            U16(static_cast<std::uint16_t>(text.size()));
            bytes.insert(bytes.end(), text.begin(), text.end());
        }
    };

    /// @param fill `grid[a][z]` for the single window.
    std::vector<std::uint8_t> OneWindow(const std::vector<std::uint8_t>& grid,
                                        std::uint32_t magic = ShadingGrid::kMagic,
                                        std::uint32_t version = ShadingGrid::kVersion,
                                        std::uint32_t flags = 0,
                                        std::uint32_t altitudes = ShadingGrid::kAltitudeSteps,
                                        std::uint32_t azimuths = ShadingGrid::kAzimuthSteps)
    {
        Builder builder;
        builder.U32(magic);
        builder.U32(version);
        builder.U32(flags);
        builder.U32(altitudes);
        builder.U32(azimuths);
        builder.U32(4);
        builder.U32(1);
        builder.Name("WIN_TEST_1");
        builder.Name("CELL_TEST");
        builder.F32(0.0F);
        builder.F32(0.0F);
        builder.F32(1.0F);
        builder.bytes.insert(builder.bytes.end(), grid.begin(), grid.end());
        return builder.bytes;
    }

    std::vector<std::uint8_t> FlatGrid(std::uint8_t value)
    {
        return std::vector<std::uint8_t>(
            static_cast<std::size_t>(ShadingGrid::kAltitudeSteps) * ShadingGrid::kAzimuthSteps, value);
    }

    cnahouse::util::Result<ShadingGrid> ReadBytes(const std::vector<std::uint8_t>& bytes)
    {
        System::IO::MemoryStream stream(reinterpret_cast<const System::IO::bytecs*>(bytes.data()),
                                        static_cast<System::IO::intcs>(bytes.size()),
                                        false);
        return ShadingGrid::Read(stream, "fixture");
    }

} // namespace

TEST(ShadingGridTests, AWellFormedFileReadsBack)
{
    const auto grid = ReadBytes(OneWindow(FlatGrid(255)));
    ASSERT_TRUE(grid) << grid.Error().ToString();
    EXPECT_EQ(grid->WindowCount(), 1U);
    EXPECT_EQ(grid->Samples(), 4U);
    const WindowShading* window = grid->Find(Id::Of("WIN_TEST_1"));
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(window->cell, Id::Of("CELL_TEST"));
    EXPECT_FLOAT_EQ(window->normal.Z, 1.0F);
    EXPECT_FLOAT_EQ(grid->Factor(Id::Of("WIN_TEST_1"), 45.0, 180.0), 1.0F);
}

TEST(ShadingGridTests, EveryWayTheFileCanBeWrongIsRefused)
{
    // A reader that half-accepts a damaged file is worse than no reader: the house is then lit
    // from a grid nobody can reproduce. Each case damages exactly one field of a file that is
    // otherwise valid, so a rejection cannot be blamed on something else.
    struct Case
    {
        const char* what;
        std::vector<std::uint8_t> bytes;
    };

    std::vector<Case> cases;
    cases.push_back({"the magic is not CSHF", OneWindow(FlatGrid(128), 0x44414221u)});
    cases.push_back({"an unknown version", OneWindow(FlatGrid(128), ShadingGrid::kMagic, 2u)});
    cases.push_back(
        {"an unknown flag bit", OneWindow(FlatGrid(128), ShadingGrid::kMagic, ShadingGrid::kVersion, 1u)});
    cases.push_back({"a grid of the wrong shape",
                     OneWindow(FlatGrid(128), ShadingGrid::kMagic, ShadingGrid::kVersion, 0u, 8u)});

    std::vector<std::uint8_t> shortGrid = OneWindow(FlatGrid(128));
    shortGrid.resize(shortGrid.size() - 10);
    cases.push_back({"a grid cut short", shortGrid});

    std::vector<std::uint8_t> truncatedHeader = OneWindow(FlatGrid(128));
    truncatedHeader.resize(9);
    cases.push_back({"a header cut short", truncatedHeader});

    std::vector<std::uint8_t> trailing = OneWindow(FlatGrid(128));
    trailing.push_back(0);
    cases.push_back({"a byte after the last window", trailing});

    Builder empty;
    empty.U32(ShadingGrid::kMagic);
    empty.U32(ShadingGrid::kVersion);
    empty.U32(0);
    empty.U32(ShadingGrid::kAltitudeSteps);
    empty.U32(ShadingGrid::kAzimuthSteps);
    empty.U32(4);
    empty.U32(1);
    empty.Name("");
    cases.push_back({"a window with an empty name", empty.bytes});

    for (const Case& probe : cases)
    {
        const auto grid = ReadBytes(probe.bytes);
        EXPECT_FALSE(grid) << probe.what << " was ACCEPTED";
    }
    std::printf("  %zu damaged file(s), all refused\n", cases.size());
}

TEST(ShadingGridTests, TheSameWindowTwiceIsRefusedRatherThanSilentlyShadowed)
{
    Builder builder;
    builder.U32(ShadingGrid::kMagic);
    builder.U32(ShadingGrid::kVersion);
    builder.U32(0);
    builder.U32(ShadingGrid::kAltitudeSteps);
    builder.U32(ShadingGrid::kAzimuthSteps);
    builder.U32(4);
    builder.U32(2);
    const std::vector<std::uint8_t> grid = FlatGrid(200);
    for (int copy = 0; copy < 2; ++copy)
    {
        builder.Name("WIN_TWICE");
        builder.Name("CELL_TEST");
        builder.F32(0.0F);
        builder.F32(0.0F);
        builder.F32(1.0F);
        builder.bytes.insert(builder.bytes.end(), grid.begin(), grid.end());
    }
    EXPECT_FALSE(ReadBytes(builder.bytes))
        << "two grids for one window is a writer defect, and the second one silently winning is "
           "how a window gets a factor nobody can account for";
}

TEST(ShadingGridTests, ANodeReadsBackExactlyAndAMidpointIsTheMeanOfItsNeighbours)
{
    // The grid is NODES: altitude 0…90 over 12 samples, azimuth 0…345 over 24. Asking at a node
    // must give that node, and asking halfway between two must give their mean -- which is what
    // makes the bilinear lookup worth having over a nearest-node one.
    std::vector<std::uint8_t> grid = FlatGrid(0);
    const std::size_t azimuths = ShadingGrid::kAzimuthSteps;
    grid[0 * azimuths + 0] = 0;
    grid[0 * azimuths + 1] = 200;
    const auto read = ReadBytes(OneWindow(grid));
    ASSERT_TRUE(read) << read.Error().ToString();
    const Id window = Id::Of("WIN_TEST_1");

    EXPECT_NEAR(read->Factor(window, 0.0, 0.0), 0.0F, 1e-6F) << "node (0, 0)";
    EXPECT_NEAR(read->Factor(window, 0.0, 15.0), 200.0F / 255.0F, 1e-6F) << "node (0, 1)";
    EXPECT_NEAR(read->Factor(window, 0.0, 7.5), 100.0F / 255.0F, 1e-6F) << "halfway between them";
}

TEST(ShadingGridTests, TheAzimuthAxisWrapsWithNoSeamDueNorth)
{
    // Node 23 is 345° and its neighbour is node 0 at 360° = 0°. A clamp here would put a seam due
    // north that a shadow crosses twice a day -- and it would look like a shading bug in the data.
    std::vector<std::uint8_t> grid = FlatGrid(0);
    const std::size_t azimuths = ShadingGrid::kAzimuthSteps;
    grid[0 * azimuths + 23] = 255;
    grid[0 * azimuths + 0] = 0;
    const auto read = ReadBytes(OneWindow(grid));
    ASSERT_TRUE(read);
    const Id window = Id::Of("WIN_TEST_1");

    EXPECT_NEAR(read->Factor(window, 0.0, 345.0), 1.0F, 1e-6F);
    EXPECT_NEAR(read->Factor(window, 0.0, 352.5), 0.5F, 1e-5F) << "halfway from node 23 to node 0";
    EXPECT_NEAR(read->Factor(window, 0.0, 360.0), 0.0F, 1e-6F) << "360 is 0, not off the end";
    // ...and a negative or multi-turn azimuth is the same direction, not an out-of-range access.
    EXPECT_NEAR(read->Factor(window, 0.0, -15.0), 1.0F, 1e-6F);
    EXPECT_NEAR(read->Factor(window, 0.0, 705.0), 1.0F, 1e-6F);
}

TEST(ShadingGridTests, TheAltitudeAxisClampsBecauseItsEndsAreMeasured)
{
    // 0° and 90° are sampled nodes rather than extrapolated, so there is nothing beyond them to
    // extrapolate towards. A sun below the horizon is `SkyExposure`'s business and not this file's.
    std::vector<std::uint8_t> grid = FlatGrid(0);
    const std::size_t azimuths = ShadingGrid::kAzimuthSteps;
    for (std::size_t z = 0; z < azimuths; ++z)
    {
        grid[0 * azimuths + z] = 60;
        grid[11 * azimuths + z] = 250;
    }
    const auto read = ReadBytes(OneWindow(grid));
    ASSERT_TRUE(read);
    const Id window = Id::Of("WIN_TEST_1");
    EXPECT_NEAR(read->Factor(window, 0.0, 100.0), 60.0F / 255.0F, 1e-6F);
    EXPECT_NEAR(read->Factor(window, -30.0, 100.0), 60.0F / 255.0F, 1e-6F) << "below the horizon";
    EXPECT_NEAR(read->Factor(window, 90.0, 100.0), 250.0F / 255.0F, 1e-6F);
    EXPECT_NEAR(read->Factor(window, 180.0, 100.0), 250.0F / 255.0F, 1e-6F) << "past the zenith";
}

TEST(ShadingGridTests, AnUnknownWindowIsUnshadedAndSaysSo)
{
    // The decision recorded on `Unshaded`: a missing bake is a content-pipeline state, not a
    // corrupt one, and an unshaded window is wrong in a direction a person can SEE.
    const auto read = ReadBytes(OneWindow(FlatGrid(0)));
    ASSERT_TRUE(read);
    EXPECT_FALSE(read->Contains(Id::Of("WIN_NO_SUCH_WINDOW")));
    EXPECT_FLOAT_EQ(read->Factor(Id::Of("WIN_NO_SUCH_WINDOW"), 45.0, 180.0), 1.0F);
    EXPECT_EQ(ShadingGrid::Unshaded().WindowCount(), 0U);
    EXPECT_FLOAT_EQ(ShadingGrid::Unshaded().Factor(Id::Of("WIN_TEST_1"), 45.0, 180.0), 1.0F);
}

TEST(ShadingGridTests, ANonFiniteAngleIsUnshadedRatherThanAnOutOfRangeRead)
{
    const auto read = ReadBytes(OneWindow(FlatGrid(0)));
    ASSERT_TRUE(read);
    const Id window = Id::Of("WIN_TEST_1");
    EXPECT_FLOAT_EQ(read->Factor(window, std::nan(""), 180.0), 1.0F);
    EXPECT_FLOAT_EQ(read->Factor(window, 45.0, std::nan("")), 1.0F);
    EXPECT_FLOAT_EQ(read->Factor(window, std::numeric_limits<double>::infinity(), 180.0), 1.0F);
}

TEST(ShadingGridTests, TheBakedFileTheContentBuildProducesReadsAndIsTheHouse)
{
    // The committed reader against the real baked file, which is what `HOUSE-01279` produced. A
    // format test that only ever reads bytes it wrote itself proves the two agree and nothing
    // about whether either matches the tool.
    if (!std::filesystem::exists(kBaked))
    {
        GTEST_SKIP() << kBaked << " is not baked; run `build_content.py --only world`";
    }
    const auto grid = ShadingGrid::ReadFromTitle(kBaked);
    ASSERT_TRUE(grid) << grid.Error().ToString();
    EXPECT_EQ(grid->WindowCount(), 64U) << "§12.6 schedules 64 windows";
    EXPECT_EQ(grid->Samples(), 4U);

    int shaded = 0;
    int unshaded = 0;
    float lowest = 1.0F;
    float highest = 0.0F;
    for (const WindowShading& window : grid->Windows())
    {
        // Every normal is an axis-aligned unit vector: §15's portals are axis-aligned planes.
        const float length = std::sqrt(window.normal.X * window.normal.X + window.normal.Y * window.normal.Y +
                                       window.normal.Z * window.normal.Z);
        EXPECT_NEAR(length, 1.0F, 1e-5F) << IdRegistry::NameOf(window.window);
        float best = 0.0F;
        for (const std::uint8_t node : window.grid)
        {
            best = std::max(best, static_cast<float>(node) / 255.0F);
        }
        lowest = std::min(lowest, best);
        highest = std::max(highest, best);
        (best > 0.0F ? shaded : unshaded) += 1;
    }
    EXPECT_GE(shaded, 60) << "most windows must see the sun from somewhere";
    std::printf("  %zu window(s); %d see the sun from some direction, %d from none; "
                "best-node factor %.3f-%.3f\n",
                grid->WindowCount(),
                shaded,
                unshaded,
                static_cast<double>(lowest),
                static_cast<double>(highest));
}
