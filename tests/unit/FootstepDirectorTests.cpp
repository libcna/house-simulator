// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "cnahouse/audio/AudioSystem.hpp"
#include "cnahouse/audio/FootstepDirector.hpp"
#include "cnahouse/content/ContentRegistry.hpp"
#include "cnahouse/world/WorldTypes.hpp"

namespace
{
    using cnahouse::audio::AudioSystem;
    using cnahouse::audio::Footstep;
    using cnahouse::audio::FootstepDirector;
    using cnahouse::audio::FootstepStep;
    using cnahouse::util::Intern;

    struct Fixture
    {
        Fixture()
            : director(audio, 0x01919ULL)
        {
            const auto loaded = registry.LoadFromJson(
                R"({"schema":"cna-house/assets/1","assets":[
                  {"id":"STEP_WOOD_1","contentName":"Audio/wood-1","kind":"sound","residencyPack":"audio-core"},
                  {"id":"STEP_WOOD_2","contentName":"Audio/wood-2","kind":"sound","residencyPack":"audio-core"},
                  {"id":"STEP_WOOD_3","contentName":"Audio/wood-3","kind":"sound","residencyPack":"audio-core"},
                  {"id":"STEP_WOOD_4","contentName":"Audio/wood-4","kind":"sound","residencyPack":"audio-core"},
                  {"id":"STEP_WOOD_5","contentName":"Audio/wood-5","kind":"sound","residencyPack":"audio-core"},
                  {"id":"STEP_WOOD_6","contentName":"Audio/wood-6","kind":"sound","residencyPack":"audio-core"},
                  {"id":"STEP_TILE_1","contentName":"Audio/tile-1","kind":"sound","residencyPack":"audio-core"},
                  {"id":"STEP_TILE_2","contentName":"Audio/tile-2","kind":"sound","residencyPack":"audio-core"},
                  {"id":"STEP_TILE_3","contentName":"Audio/tile-3","kind":"sound","residencyPack":"audio-core"},
                  {"id":"STEP_TILE_4","contentName":"Audio/tile-4","kind":"sound","residencyPack":"audio-core"},
                  {"id":"STEP_TILE_5","contentName":"Audio/tile-5","kind":"sound","residencyPack":"audio-core"},
                  {"id":"STEP_TILE_6","contentName":"Audio/tile-6","kind":"sound","residencyPack":"audio-core"}
                ]})",
                "fixture-manifest.json");
            if (!loaded)
            {
                ADD_FAILURE() << loaded.Error().ToString();
                return;
            }

            cnahouse::world::AudioBank wood;
            wood.id = Intern("BANK_WOOD");
            wood.surfaces = {"hardwood"};
            for (int i = 1; i <= 6; ++i)
            {
                wood.samples.push_back(Intern("STEP_WOOD_" + std::to_string(i)));
            }
            wood.gain = 0.8F;

            cnahouse::world::AudioBank tile;
            tile.id = Intern("BANK_TILE");
            tile.surfaces = {"tile"};
            for (int i = 1; i <= 6; ++i)
            {
                tile.samples.push_back(Intern("STEP_TILE_" + std::to_string(i)));
            }
            banks = {wood, tile};
            audio.LoadBanks(banks, registry);
            director.BindBanks(banks);
        }

        [[nodiscard]] std::vector<Footstep>
        Walk(std::string_view surface, float speed, float seconds, bool fast)
        {
            constexpr float dt = 1.0F / 120.0F;
            const int steps = static_cast<int>(std::round(seconds / dt));
            std::vector<Footstep> heard;
            for (int i = 0; i < steps; ++i)
            {
                FootstepStep sample;
                sample.surface = surface;
                sample.distanceMeters = speed * dt;
                sample.onGround = true;
                sample.fastWalk = fast;
                if (auto sound = director.Advance(sample); sound.has_value())
                {
                    heard.push_back(*sound);
                }
            }
            return heard;
        }

        AudioSystem audio{false};
        cnahouse::content::ContentRegistry registry;
        std::array<cnahouse::world::AudioBank, 2> banks;
        FootstepDirector director;
    };

    TEST(FootstepDirectorTests, CadenceMatchesSpeedAcrossModesAndSurfaces)
    {
        for (const std::string_view surface : {std::string_view("hardwood"), std::string_view("tile")})
        {
            Fixture walking;
            const auto normal = walking.Walk(surface, 1.35F, 20.0F, false);
            const float expectedNormal = 1.35F * 20.0F / FootstepDirector::kWalkStrideMeters;
            EXPECT_NEAR(static_cast<float>(normal.size()), expectedNormal, expectedNormal * 0.05F);

            Fixture fastWalking;
            const auto fast = fastWalking.Walk(surface, 2.05F, 20.0F, true);
            const float expectedFast = 2.05F * 20.0F / FootstepDirector::kFastStrideMeters;
            EXPECT_NEAR(static_cast<float>(fast.size()), expectedFast, expectedFast * 0.05F);
        }
    }

    TEST(FootstepDirectorTests, SurfaceLookupSelectsTheBoundBank)
    {
        Fixture fixture;
        const auto wood = fixture.Walk("hardwood", 1.5F, 0.5F, false);
        ASSERT_EQ(wood.size(), 1U);
        EXPECT_EQ(wood.front().bank, Intern("BANK_WOOD"));
        EXPECT_EQ(wood.front().sample, "Audio/wood-1");

        fixture.director.Reset();
        const auto tile = fixture.Walk("tile", 1.5F, 0.5F, false);
        ASSERT_EQ(tile.size(), 1U);
        EXPECT_EQ(tile.front().bank, Intern("BANK_TILE"));
        EXPECT_EQ(tile.front().sample, "Audio/tile-1");
    }

    TEST(FootstepDirectorTests, RoundRobinDoesNotRepeatWithinFourAndVariationIsBounded)
    {
        Fixture fixture;
        const auto heard = fixture.Walk("hardwood", 1.5F, 6.0F, false);
        ASSERT_GE(heard.size(), 10U);
        for (std::size_t i = 0; i < heard.size(); ++i)
        {
            EXPECT_GE(heard[i].pitch, -0.04F);
            EXPECT_LT(heard[i].pitch, 0.04F);
            EXPECT_GE(heard[i].volume, 0.8F * 0.90F);
            EXPECT_LT(heard[i].volume, 0.8F * 1.10F);
            for (std::size_t back = 1; back <= 4 && back <= i; ++back)
            {
                EXPECT_NE(heard[i].sample, heard[i - back].sample);
            }
        }
    }

    TEST(FootstepDirectorTests, StairsProduceExactlyOneStepPerRiser)
    {
        Fixture fixture;
        constexpr float rise = 0.1794118F;
        constexpr int risers = 17;
        int heard = 0;
        for (int riser = 0; riser < risers; ++riser)
        {
            for (int half = 0; half < 2; ++half)
            {
                FootstepStep step;
                step.surface = "hardwood";
                step.distanceMeters = 0.6F;
                step.verticalDistanceMeters = rise * 0.5F;
                step.riserHeightMeters = rise;
                step.onGround = true;
                heard += fixture.director.Advance(step).has_value() ? 1 : 0;
            }
        }
        EXPECT_EQ(heard, risers);
    }

    TEST(FootstepDirectorTests, AirborneAndUnknownSurfacesAreSilentAndResetCadence)
    {
        Fixture fixture;
        FootstepStep halfStride{"hardwood", 0.5F, 0.0F, 0.0F, true, false};
        EXPECT_FALSE(fixture.director.Advance(halfStride).has_value());

        FootstepStep airborne{"hardwood", 0.5F, 0.0F, 0.0F, false, false};
        EXPECT_FALSE(fixture.director.Advance(airborne).has_value());
        EXPECT_FALSE(fixture.director.Advance(halfStride).has_value())
            << "airborne distance leaked into the next grounded stride";

        FootstepStep unknown{"marshmallow", 1.0F, 0.0F, 0.0F, true, false};
        EXPECT_FALSE(fixture.director.Advance(unknown).has_value());
    }

} // namespace
