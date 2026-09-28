// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <numbers>

#include "System/IO/MemoryStream.hpp"
#include "cnahouse/audio/AmbienceDirector.hpp"
#include "cnahouse/audio/SkyExposure.hpp"
#include "cnahouse/audio/WeatherPcm.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/Terrain.hpp"
#include "cnahouse/weather/CoverageMask.hpp"
#include "cnahouse/world/ChunkReader.hpp"

namespace
{
    using cnahouse::audio::AmbienceDirector;
    using cnahouse::audio::SkyExposure;
    using cnahouse::audio::WeatherPcm;
    using cnahouse::world::CellKind;

    std::vector<std::uint8_t> Fixture()
    {
        std::vector<std::uint8_t> bytes;
        const auto integer = [&bytes](std::uint32_t value, int size)
        {
            for (int index = 0; index < size; ++index)
            {
                bytes.push_back(static_cast<std::uint8_t>((value >> (8 * index)) & 255U));
            }
        };
        const auto text = [&](std::string_view value)
        {
            integer(static_cast<std::uint32_t>(value.size()), 2);
            bytes.insert(bytes.end(), value.begin(), value.end());
        };
        integer(0x594B5343U, 4);
        integer(2U, 4);
        integer(0U, 4);
        text("world");
        integer(512U, 4);
        integer(8U, 4);
        for (const auto name : {"N", "NE", "E", "SE", "S", "SW", "W", "NW"})
        {
            text(name);
        }
        integer(1U, 4);
        text("L0_ROOM");
        for (int axis = 0; axis < 3; ++axis)
        {
            integer(0U, 4);
        }
        integer(16U, 4);
        for (int value = 0; value < 9; ++value)
        {
            integer(std::bit_cast<std::uint32_t>(0.25F), 4);
        }
        return bytes;
    }

    auto ReadSky(const std::vector<std::uint8_t>& bytes, std::string_view hash = "world")
    {
        System::IO::MemoryStream stream(bytes.data(), static_cast<int>(bytes.size()), false);
        return SkyExposure::Read(stream, "fixture", hash);
    }

    TEST(WeatherAudioTests, SkyBakeIsStrictAndHashBound)
    {
        const auto bytes = Fixture();
        const auto loaded = ReadSky(bytes);
        ASSERT_TRUE(loaded) << loaded.Error().ToString();
        EXPECT_EQ(loaded->CellCount(), 1U);
        EXPECT_FLOAT_EQ(loaded->At(cnahouse::util::Intern("L0_ROOM")), 0.25F);
        EXPECT_FLOAT_EQ(loaded->At(cnahouse::util::Intern("L0_MISSING")), 0.0F);
        EXPECT_FALSE(ReadSky(bytes, "different"));
        for (std::size_t size = 0; size < bytes.size(); ++size)
        {
            EXPECT_FALSE(ReadSky({bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size)})) << size;
        }
        for (const std::size_t at : {0U, 4U, 8U, 22U, 23U, 29U, 55U, 83U, 87U})
        {
            auto corrupt = bytes;
            ASSERT_LT(at, corrupt.size());
            corrupt[at] = 255U;
            EXPECT_FALSE(ReadSky(corrupt)) << at;
        }
        auto trailing = bytes;
        trailing.push_back(0U);
        EXPECT_FALSE(ReadSky(trailing));
    }

    cnahouse::weather::WeatherState Rain()
    {
        cnahouse::weather::WeatherState weather;
        weather.precipType = cnahouse::weather::PrecipType::Rain;
        weather.precipIntensity = 1.0F;
        weather.windSpeed = 12.0F;
        return weather;
    }

    TEST(WeatherAudioTests, ExposureAndRoofTransmissionHaveTheRequiredHierarchy)
    {
        AmbienceDirector director;
        const auto outdoors = director.AdvanceWeather(CellKind::Exterior, 1.0F, INFINITY, 0.0F, Rain(), 0.0F);
        EXPECT_FLOAT_EQ(outdoors.layers[1], 1.0F);
        EXPECT_FLOAT_EQ(outdoors.layers[3], 1.0F);
        EXPECT_FLOAT_EQ(outdoors.dull, 0.0F);
        director.Reset();
        const auto basement = director.AdvanceWeather(CellKind::Room, 0.0F, 12.0F, 1.0F, Rain(), 0.0F);
        EXPECT_FLOAT_EQ(basement.layers[1], 0.0F);
        EXPECT_FLOAT_EQ(basement.layers[3], 0.0F);
        EXPECT_FLOAT_EQ(basement.dull, 1.0F);
        director.Reset();
        const auto attic = director.AdvanceWeather(CellKind::Room, 0.0F, 3.0F, 0.0F, Rain(), 0.0F);
        EXPECT_GT(attic.layers[1], 0.9F);
        director.Reset();
        const auto interior = director.AdvanceWeather(CellKind::Room, 0.001F, 10.0F, 0.0F, Rain(), 0.0F);
        director.Reset();
        const auto glazed = director.AdvanceWeather(CellKind::Room, 0.10F, 10.0F, 0.0F, Rain(), 0.0F);
        EXPECT_GT(glazed.layers[1], interior.layers[1] * 2.0F);
        EXPECT_LT(glazed.layers[1], outdoors.layers[1]);
        EXPECT_GT(glazed.dull, outdoors.dull);
        director.Reset();
        const auto windowless = director.AdvanceWeather(CellKind::Room, 0.0F, 15.0F, 0.0F, Rain(), 0.0F);
        EXPECT_GE(windowless.layers[1], 0.3F);
        director.Reset();
        const auto sheltered = director.AdvanceWeather(CellKind::Room, 0.2F, 15.0F, 1.0F, Rain(), 0.0F);
        EXPECT_FLOAT_EQ(sheltered.layers[1] + sheltered.layers[3], 0.0F);
    }

    TEST(WeatherAudioTests, WeatherAndCellChangesFadeWithoutRevivingSnowAudio)
    {
        AmbienceDirector director;
        (void)director.AdvanceWeather(CellKind::Exterior, 1.0F, INFINITY, 0.0F, Rain(), 0.0F);
        const auto halfway = director.AdvanceWeather(CellKind::Room, 0.0F, 12.0F, 1.0F, Rain(), 0.4F);
        EXPECT_FLOAT_EQ(halfway.layers[1], 0.5F);
        EXPECT_FLOAT_EQ(halfway.dull, 0.5F);
        const auto invalid = director.AdvanceWeather(CellKind::Room, 0.0F, 12.0F, 1.0F, Rain(), -1.0F);
        EXPECT_EQ(invalid.layers, halfway.layers);
        auto weather = Rain();
        weather.precipType = cnahouse::weather::PrecipType::Snow;
        const auto snow = director.AdvanceWeather(CellKind::Exterior, 1.0F, INFINITY, 0.0F, weather, 1.0F);
        EXPECT_FLOAT_EQ(snow.layers[0] + snow.layers[1], 0.0F);
        director.Reset();
        weather.precipIntensity = 0.2F;
        weather.precipType = cnahouse::weather::PrecipType::Rain;
        weather.windSpeed = 1.0F;
        const auto calm = director.AdvanceWeather(CellKind::Exterior, 1.0F, INFINITY, 0.0F, weather, 0.0F);
        EXPECT_GT(calm.layers[0], 0.0F);
        EXPECT_FLOAT_EQ(calm.layers[1], 0.0F);
        EXPECT_GT(calm.layers[2], 0.0F);
        EXPECT_FLOAT_EQ(calm.layers[3], 0.0F);
    }

    TEST(WeatherAudioTests, AllFourExistingLoopsDecodeAndRejectCorruption)
    {
        for (const auto name : {"rain-calm/Ambiance_Rain_Calm",
                                "rain-strong/Ambiance_Rain_Strong",
                                "wind-calm/Ambiance_Wind_Calm",
                                "wind-forest/Ambiance_Wind_Forest"})
        {
            const std::string path =
                std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/Audio/ambience/" + name + "_Loop_Stereo.cnb";
            std::ifstream file(path, std::ios::binary);
            ASSERT_TRUE(file) << path;
            std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
            const auto pcm = WeatherPcm::Decode(bytes);
            ASSERT_TRUE(pcm) << pcm.Error().ToString() << path;
            EXPECT_EQ(pcm->channels, 2);
            EXPECT_EQ(pcm->sampleRate, 48000);
            EXPECT_FALSE(pcm->samples.empty());
            for (const std::size_t at : {0U, 16U, 32U, 44U, 80U, 207U})
            {
                auto corrupt = bytes;
                corrupt[at] ^= 128U;
                EXPECT_FALSE(WeatherPcm::Decode(corrupt)) << at;
            }
            auto corrupt = bytes;
            corrupt.back() ^= 1U;
            EXPECT_FALSE(WeatherPcm::Decode(corrupt));
            bytes.resize(bytes.size() / 2U);
            EXPECT_FALSE(WeatherPcm::Decode(bytes));
        }
    }

    double FilterGain(double frequency)
    {
        WeatherPcm pcm;
        pcm.channels = 2;
        pcm.sampleRate = 48000;
        double before = 0.0;
        for (int frame = 0; frame < 48000; ++frame)
        {
            const auto sample = static_cast<int>(12000.0 * std::sin(2.0 * std::numbers::pi * frequency *
                                                                    static_cast<double>(frame) / 48000.0));
            const auto bits = static_cast<std::uint16_t>(sample);
            pcm.samples.push_back(static_cast<std::uint8_t>(bits & 255U));
            pcm.samples.push_back(static_cast<std::uint8_t>(bits >> 8U));
            pcm.samples.push_back(0U);
            pcm.samples.push_back(0U);
            before += static_cast<double>(sample * sample);
        }
        pcm.LowPass();
        double after = 0.0;
        for (std::size_t at = 0; at < pcm.samples.size(); at += 4U)
        {
            const unsigned bits =
                static_cast<unsigned>(pcm.samples[at]) | (static_cast<unsigned>(pcm.samples[at + 1U]) << 8U);
            const int sample = bits >= 32768U ? static_cast<int>(bits) - 65536 : static_cast<int>(bits);
            after += static_cast<double>(sample * sample);
            EXPECT_EQ(pcm.samples[at + 2U], 0U);
            EXPECT_EQ(pcm.samples[at + 3U], 0U);
        }
        return std::sqrt(after / before);
    }

    TEST(WeatherAudioTests, LowPassReallyRemovesHighFrequenciesWithoutStereoLeak)
    {
        EXPECT_GT(FilterGain(100.0), 0.98);
        EXPECT_LT(FilterGain(6000.0), 0.03);
        WeatherPcm invalid;
        EXPECT_THROW(invalid.LowPass(), std::runtime_error);
    }

    double PcmEnergy(const WeatherPcm& pcm)
    {
        double energy = 0.0;
        for (std::size_t at = 0; at < pcm.samples.size(); at += 2U)
        {
            const unsigned bits =
                static_cast<unsigned>(pcm.samples[at]) | (static_cast<unsigned>(pcm.samples[at + 1U]) << 8U);
            const double sample = bits >= 32768U ? static_cast<int>(bits) - 65536 : static_cast<int>(bits);
            energy += sample * sample;
        }
        return energy;
    }

    TEST(WeatherAudioTests, FilteredRetainedLoopsPreserveLevelWithoutClippingOrBoostingSilence)
    {
        for (const auto name : {"rain-calm/Ambiance_Rain_Calm",
                                "rain-strong/Ambiance_Rain_Strong",
                                "wind-calm/Ambiance_Wind_Calm",
                                "wind-forest/Ambiance_Wind_Forest"})
        {
            auto pcm = WeatherPcm::ReadFromTitle(std::string(CNAHOUSE_TEST_CONTENT_ROOT) +
                                                 "/Audio/ambience/" + name + "_Loop_Stereo.cnb");
            ASSERT_TRUE(pcm) << pcm.Error().ToString();
            const double before = PcmEnergy(*pcm);
            const float gain = pcm->LowPass(true);
            EXPECT_GE(gain, 1.0F);
            EXPECT_LE(gain, 8.0F);
            const double ratio = std::sqrt(PcmEnergy(*pcm) / before);
            EXPECT_NEAR(ratio, 1.0, 0.01) << name;
            for (std::size_t at = 0; at < pcm->samples.size(); at += 2U)
            {
                const unsigned bits = static_cast<unsigned>(pcm->samples[at]) |
                                      (static_cast<unsigned>(pcm->samples[at + 1U]) << 8U);
                const int sample = bits >= 32768U ? static_cast<int>(bits) - 65536 : static_cast<int>(bits);
                ASSERT_LE(std::abs(sample), 29490) << name;
            }
        }
        WeatherPcm silent{{0U, 0U, 0U, 0U}, 48000, 2, 0, 1};
        EXPECT_FLOAT_EQ(silent.LowPass(true), 1.0F);
        EXPECT_EQ(silent.samples, (std::vector<std::uint8_t>{0U, 0U, 0U, 0U}));
    }

    TEST(WeatherAudioTests, AuthoredBakeAndCoverMeetTheThreeRequiredListeningCases)
    {
        const auto chunks = cnahouse::world::ChunkReader::ReadFromTitle(
            std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world/chunks.bin");
        ASSERT_TRUE(chunks) << chunks.Error().ToString();
        const auto sky = SkyExposure::ReadFromTitle(
            std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world/skyexposure.bin", chunks->worldHash);
        ASSERT_TRUE(sky) << sky.Error().ToString();
        EXPECT_EQ(sky->CellCount(), chunks->cells.size());
        const auto cover = cnahouse::weather::CoverageMask::ReadFromTitle(
            std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world/coverage.bin");
        ASSERT_TRUE(cover) << cover.Error().ToString();
        const auto collision = cnahouse::physics::CollisionLoader::ReadFromTitle(
            std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world/collision.bin");
        ASSERT_TRUE(collision) << collision.Error().ToString();
        const auto mix = [&](std::string_view id, float x, float eyeY, float z, float floorY)
        {
            AmbienceDirector director;
            return director.AdvanceWeather(CellKind::Room,
                                           sky->At(cnahouse::util::Intern(id)),
                                           cover->HeightAt(x, z) - eyeY,
                                           cnahouse::physics::TerrainAt(collision->terrain, x, z).height -
                                               floorY,
                                           Rain(),
                                           0.0F);
        };
        const auto basement = mix("B1_CINEMA", 5.45F, -0.62F, -25.05F, -2.30F);
        const auto attic = mix("L3_STORE_W", -9.35F, 10.98F, -20.7F, 9.30F);
        const auto hall = mix("L0_HALL", 0.0F, 2.28F, -20.65F, 0.60F);
        const auto sunroom = mix("L0_SUNROOM", -2.0F, 2.28F, -29.6F, 0.60F);
        EXPECT_FLOAT_EQ(basement.layers[1] + basement.layers[3], 0.0F);
        EXPECT_GT(attic.layers[1], 0.9F);
        EXPECT_GT(sunroom.layers[1], hall.layers[1] * 2.0F);
        EXPECT_GT(sunroom.layers[3], hall.layers[3] * 2.0F);
        EXPECT_GT(attic.dull, 0.9F);
    }
} // namespace
