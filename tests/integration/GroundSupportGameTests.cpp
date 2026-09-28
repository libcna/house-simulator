// SPDX-License-Identifier: MIT
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/util/Log.hpp"

TEST(GroundSupportGameTests, SlowGarageStepsAndNormalPorchUseTheRealController)
{
    const char* driver = std::getenv("SDL_VIDEODRIVER");
    if (driver == nullptr || std::string_view(driver) != "offscreen")
    {
        GTEST_SKIP() << "isolated GPU renderer required; never open the monitor";
    }
    ASSERT_EQ(std::getenv("DISPLAY"), nullptr);
    ASSERT_EQ(std::getenv("WAYLAND_DISPLAY"), nullptr);
    using cnahouse::app::CnaHouseGame;

    struct Case
    {
        const char* name;
        std::array<float, 5> pose;
        float movement;
        const char* arrival;
        bool porch = false;
    };

    const Case cases[] = {
        {"garage-slow-up", {9.8F, 0.15F, -16.36266F, 270.0F, -5.0F}, 0.35F, "L0_MUDROOM"},
        {"garage-normal-up", {9.8F, 0.15F, -16.36266F, 270.0F, -5.0F}, 1.0F, "L0_MUDROOM"},
        {"garage-normal-down", {8.3F, 0.6F, -16.36266F, 90.0F, -5.0F}, 1.0F, "L0_GARAGE"},
        // The 1 m doorway is centred at x=0; x=0.5 aims into its jamb, not through it.
        {"porch-normal-up", {0.0F, 0.0F, -9.9F, 0.0F, -5.0F}, 1.0F, "L0_FOYER", true},
        {"gate-road-to-walk", {0.0F, 0.0F, 5.2F, 0.0F, -5.0F}, 1.0F, "EXT_WALK"},
        {"gate-walk-to-road", {0.0F, 0.0F, -3.0F, 180.0F, -5.0F}, 1.0F, "EXT_ROAD"},
    };
    const auto original = std::filesystem::current_path();
    const auto buildRoot = std::filesystem::path(CNAHOUSE_TEST_CONTENT_ROOT).parent_path();
    const char* review = std::getenv("HOUSE_SUPPORT_REVIEW");
    const std::string phase = review != nullptr && std::string_view(review) == "before" ? "before" : "after";
    for (const auto& crossing : cases)
    {
        if (const char* requested = std::getenv("HOUSE_SUPPORT_CASE");
            requested != nullptr && std::string_view(requested) != crossing.name)
        {
            continue;
        }
        const auto output =
            std::filesystem::path(CNAHOUSE_TEST_OUTPUT_DIR) / ("house-03642-" + phase) / crossing.name;
        std::filesystem::create_directories(output);
        std::set<std::filesystem::path> before;
        for (const auto& entry : std::filesystem::directory_iterator(buildRoot))
        {
            if (entry.path().filename().string().starts_with("cna-house-") &&
                entry.path().extension() == ".png")
            {
                before.insert(entry.path());
            }
        }
        std::filesystem::current_path(buildRoot);
        cnahouse::util::Log::ResetForTesting();
        cnahouse::app::Options options;
        options.scene = "walk";
        options.player = crossing.pose;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.timeOfDay = 10.5F;
        options.freezeTime = true;
        options.weather = "W_CLEAR";
        auto settings = cnahouse::app::Settings::Defaults();
        settings.fastWalk = false;
        settings.backBufferWidth = 960;
        settings.backBufferHeight = 540;
        settings.verticalSync = false;
        CnaHouseGame game(options, settings);

        class WalkInput final : public cnahouse::player::IInputSource
        {
        public:
            WalkInput(CnaHouseGame& game, const Case& crossing)
                : game_(game)
                , crossing_(crossing)
            {
            }

            void Update(float) override
            {
                state_ = {};
                state_.move.Y = crossing_.movement;
                const auto steps = game_.FixedStepsForTesting();
                if (steps >= shotAt_)
                {
                    state_.screenshotPressed = true;
                    shotAt_ += 120U;
                }
                const std::string_view cell = cnahouse::util::IdRegistry::NameOf(game_.CellForTesting());
                const auto feet = game_.PlayerForTesting().Feet();
                const bool passedGate =
                    std::string_view(crossing_.name) == "gate-road-to-walk"   ? feet.Z < -2.0F
                    : std::string_view(crossing_.name) == "gate-walk-to-road" ? feet.Z > 4.0F
                                                                              : true;
                if (cell == crossing_.arrival && passedGate && (!crossing_.porch || feet.Z < -15.2F))
                {
                    if (arrived_)
                    {
                        game_.Exit();
                    }
                    else
                    {
                        arrived_ = true;
                        state_.screenshotPressed = true;
                    }
                }
            }

            const cnahouse::player::InputState& Current() const noexcept override
            {
                return state_;
            }

            bool LookAvailable() const noexcept override
            {
                return false;
            }

            CnaHouseGame& game_;
            const Case& crossing_;
            cnahouse::player::InputState state_;
            bool arrived_ = false;
            std::uint64_t shotAt_ = 0U;
        } input(game, crossing);

        game.SetInputSourceForTesting(&input);
        game.SetFixedStepLimit(2400U);
        game.SetFrameLimit(30000U);
        game.Run();
        std::vector<std::filesystem::path> shots;
        for (const auto& entry : std::filesystem::directory_iterator(buildRoot))
        {
            if (entry.path().filename().string().starts_with("cna-house-") &&
                entry.path().extension() == ".png" && !before.contains(entry.path()))
            {
                shots.push_back(entry.path());
            }
        }
        std::sort(shots.begin(), shots.end());
        for (std::size_t i = 0; i < shots.size(); ++i)
        {
            std::filesystem::rename(shots[i], output / (std::to_string(i) + ".png"));
        }
        std::filesystem::current_path(original);
        EXPECT_EQ(game.ExitCode(), 0) << crossing.name;
        EXPECT_TRUE(input.arrived_) << crossing.name << " feet " << game.PlayerForTesting().Feet().X << ' '
                                    << game.PlayerForTesting().Feet().Y << ' '
                                    << game.PlayerForTesting().Feet().Z;
        EXPECT_FALSE(game.PlayerForTesting().fastWalk);
        EXPECT_FALSE(game.PlayerForTesting().noclip);
    }
}
