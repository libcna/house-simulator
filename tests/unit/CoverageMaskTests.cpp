// SPDX-License-Identifier: MIT
//
// `HOUSE-01744`. The runtime reader is tested independently from the Python writer, then the
// authored field proves the porch boundary and the rain submission invariant end to end.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"
#include "System/IO/MemoryStream.hpp"

#include "cnahouse/rendering/ParticleRenderer.hpp"
#include "cnahouse/weather/CoverageMask.hpp"
#include "cnahouse/weather/RainParticles.hpp"

namespace
{
    namespace Xna = Microsoft::Xna::Framework;
    using cnahouse::weather::CoverageMask;

    struct Bytes
    {
        std::vector<std::uint8_t> data;

        void U16(std::uint16_t value)
        {
            data.push_back(static_cast<std::uint8_t>(value & 0xffu));
            data.push_back(static_cast<std::uint8_t>(value >> 8u));
        }

        void U32(std::uint32_t value)
        {
            for (int shift = 0; shift < 32; shift += 8)
            {
                data.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
            }
        }

        void F32(float value)
        {
            std::uint32_t bits = 0u;
            std::memcpy(&bits, &value, sizeof(bits));
            U32(bits);
        }

        void Name(const std::string& value)
        {
            U16(static_cast<std::uint16_t>(value.size()));
            data.insert(data.end(), value.begin(), value.end());
        }
    };

    std::vector<std::uint8_t> Grid(std::uint32_t width,
                                   std::uint32_t height,
                                   const std::vector<float>& samples,
                                   std::uint32_t magic = CoverageMask::kMagic,
                                   std::uint32_t version = CoverageMask::kVersion,
                                   std::uint32_t flags = 0u,
                                   float cell = CoverageMask::kCellSizeMetres)
    {
        Bytes bytes;
        bytes.U32(magic);
        bytes.U32(version);
        bytes.U32(flags);
        bytes.Name("worldhash");
        bytes.F32(0.0F);
        bytes.F32(0.0F);
        bytes.F32(cell);
        bytes.F32(-3.0F);
        bytes.U32(width);
        bytes.U32(height);
        for (const float sample : samples)
        {
            bytes.F32(sample);
        }
        return bytes.data;
    }

    cnahouse::util::Result<CoverageMask> ReadBytes(const std::vector<std::uint8_t>& bytes)
    {
        System::IO::MemoryStream stream(reinterpret_cast<const System::IO::bytecs*>(bytes.data()),
                                        static_cast<System::IO::intcs>(bytes.size()),
                                        false);
        return CoverageMask::Read(stream, "fixture");
    }

    CoverageMask AuthoredMask()
    {
        const std::string path = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world/coverage.bin";
        if (!std::filesystem::exists(path))
        {
            return {};
        }
        System::IO::FileStream stream(path, System::IO::FileMode::Open, System::IO::FileAccess::Read);
        auto mask = CoverageMask::Read(stream, path);
        EXPECT_TRUE(mask) << mask.Error().ToString();
        return mask ? std::move(mask.Value()) : CoverageMask{};
    }
} // namespace

TEST(CoverageMaskTests, OpenSkyAndFiniteCoverHaveOppositeExplicitSemantics)
{
    const auto mask = ReadBytes(Grid(2u, 1u, {3.0F, std::numeric_limits<float>::infinity()}));
    ASSERT_TRUE(mask) << mask.Error().ToString();
    EXPECT_EQ(mask->WorldHash(), "worldhash");
    EXPECT_EQ(mask->Width(), 2u);
    EXPECT_EQ(mask->Height(), 1u);
    EXPECT_EQ(mask->CoveredSampleCount(), 1u);
    EXPECT_FLOAT_EQ(mask->HeightAt(0.25F, 0.25F), 3.0F);
    EXPECT_TRUE(std::isinf(mask->HeightAt(0.75F, 0.25F)));
    EXPECT_TRUE(std::isinf(mask->HeightAt(-1.0F, 0.25F)));

    Xna::Vector3 below(0.25F, 2.0F, 0.25F);
    EXPECT_FALSE(mask->IsExposed(below));
    EXPECT_TRUE(mask->TeleportSheltered(below, 9.0F));
    EXPECT_FLOAT_EQ(below.Y, 9.0F);
    EXPECT_TRUE(mask->IsExposed(below));

    Xna::Vector3 open(0.75F, -20.0F, 0.25F);
    EXPECT_TRUE(mask->IsExposed(open));
    EXPECT_FALSE(mask->TeleportSheltered(open, 9.0F));
}

TEST(CoverageMaskTests, CorruptHeadersDimensionsAndSamplesAreRefused)
{
    struct Case
    {
        const char* name;
        std::vector<std::uint8_t> bytes;
    };

    std::vector<Case> cases;
    cases.push_back({"magic", Grid(1u, 1u, {1.0F}, 0x21444142u)});
    cases.push_back({"version", Grid(1u, 1u, {1.0F}, CoverageMask::kMagic, 2u)});
    cases.push_back({"flags", Grid(1u, 1u, {1.0F}, CoverageMask::kMagic, 1u, 1u)});
    cases.push_back({"cell", Grid(1u, 1u, {1.0F}, CoverageMask::kMagic, CoverageMask::kVersion, 0u, 1.0F)});
    cases.push_back({"empty", Grid(0u, 1u, {})});
    cases.push_back({"nan", Grid(1u, 1u, {std::numeric_limits<float>::quiet_NaN()})});
    cases.push_back({"below ground", Grid(1u, 1u, {-4.0F})});
    auto truncated = Grid(1u, 1u, {1.0F});
    truncated.pop_back();
    cases.push_back({"truncated", std::move(truncated)});
    auto trailing = Grid(1u, 1u, {1.0F});
    trailing.push_back(0u);
    cases.push_back({"trailing", std::move(trailing)});

    for (const Case& row : cases)
    {
        EXPECT_FALSE(ReadBytes(row.bytes)) << row.name;
    }
}

TEST(CoverageMaskTests, AuthoredPropertyKeepsThePorchBoundaryAndEverySubmittedDropExposed)
{
    const std::string path = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world/coverage.bin";
    if (!std::filesystem::exists(path))
    {
        GTEST_SKIP() << "no generated coverage.bin";
    }
    const CoverageMask mask = AuthoredMask();
    ASSERT_EQ(mask.Width(), 160u);
    ASSERT_EQ(mask.Height(), 128u);
    EXPECT_EQ(mask.CoveredSampleCount(), 1635u);
    EXPECT_NEAR(mask.HeightAt(0.0F, -11.9F), 3.30F, 0.01F);
    EXPECT_TRUE(std::isinf(mask.HeightAt(0.0F, -11.3F)));
    EXPECT_NEAR(mask.HeightAt(-18.0F, -42.0F), 2.35F, 0.01F);

    cnahouse::rendering::Camera camera;
    cnahouse::rendering::ParticleRenderer renderer(camera);
    cnahouse::weather::RainParticles rain;
    cnahouse::weather::WeatherState state;
    state.precipType = cnahouse::weather::PrecipType::Rain;
    state.precipIntensity = 1.0F;
    const Xna::Vector3 eye(0.0F, 2.25F, -13.0F);
    ASSERT_TRUE(rain.Update(0.0F, eye, state, cnahouse::rendering::ParticleQuality::High, renderer, &mask));

    const float volumeTop = eye.Y + 0.5F * cnahouse::weather::kPrecipitationHeightMetres;
    std::size_t teleported = 0u;
    for (const Xna::Vector3& position : rain.Positions())
    {
        if (std::isfinite(mask.HeightAt(position.X, position.Z)) && position.Y == volumeTop)
        {
            ++teleported;
        }
    }
    EXPECT_GT(teleported, 0u);
    ASSERT_GT(renderer.Particles().size(), 0u);
    for (const cnahouse::rendering::ParticleQuad& particle : renderer.Particles())
    {
        EXPECT_TRUE(mask.IsExposed(particle.centre))
            << "a rain particle was submitted at or below its finite covered surface";
    }

    ASSERT_TRUE(rain.Update(0.0F,
                            Xna::Vector3(5.70F, 2.30F, -23.05F),
                            state,
                            cnahouse::rendering::ParticleQuality::High,
                            renderer,
                            &mask));
    ASSERT_GT(renderer.Particles().size(), 0u)
        << "open-sky rain outside must remain visible through a closed window";
    for (const cnahouse::rendering::ParticleQuad& particle : renderer.Particles())
    {
        EXPECT_TRUE(mask.IsExposed(particle.centre))
            << "an indoor camera submitted a drop under the house roof";
    }
}
