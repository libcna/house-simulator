// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/util/Log.hpp"

namespace
{
    using cnahouse::app::CnaHouseGame;
    using cnahouse::app::Options;
    using cnahouse::app::Settings;

    bool PrivateGpuAvailable()
    {
        const char* driver = std::getenv("SDL_VIDEODRIVER");
        return driver != nullptr && std::string_view(driver) == "offscreen" &&
               std::getenv("DISPLAY") == nullptr && std::getenv("WAYLAND_DISPLAY") == nullptr;
    }
} // namespace

class CinemaStopTests : public testing::TestWithParam<int>
{
};

TEST_P(CinemaStopTests, StopsAtCurrentPositionAndReturnsOrdinaryControllerInput)
{
    if (!PrivateGpuAvailable())
    {
        GTEST_SKIP() << "use the isolated offscreen GPU runner";
    }
    const auto original = std::filesystem::current_path();
    std::filesystem::current_path(std::filesystem::path(CNAHOUSE_TEST_CONTENT_ROOT).parent_path());
    cnahouse::util::Log::ResetForTesting();
    Options options;
    options.scene = "walk";
    options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
    options.noAudio = true;
    Settings settings = Settings::Defaults();
    settings.backBufferWidth = 640;
    settings.backBufferHeight = 360;
    settings.verticalSync = false;
    CnaHouseGame game(options, settings);

    class ToggleInput final : public cnahouse::player::IInputSource
    {
    public:
        ToggleInput(CnaHouseGame& game, int stopMode)
            : game_(game)
            , stopMode_(stopMode)
        {
        }

        void Update(float) override
        {
            state_ = {};
            if (frame_ == 1U)
            {
                state_.cinemaPressed = true;
            }
            if (frame_ == 16U)
            {
                EXPECT_TRUE(game_.FilmingTourForTesting().Active());
                stoppedAt_ = game_.PlayerForTesting().Feet();
                state_.cinemaPressed = stopMode_ == 0;
                state_.cancelPressed = stopMode_ == 1;
                state_.menuPressed = stopMode_ == 2;
            }
            if (frame_ == 20U && stopMode_ != 0)
            {
                EXPECT_FALSE(game_.FilmingTourForTesting().Active());
                ASSERT_NE(game_.Menus().Top(), nullptr);
                EXPECT_EQ(game_.Menus().Top()->Id(), cnahouse::ui::ScreenId::PauseMenu);
                state_.cancelPressed = true; // Esc resumes, never quits the application.
            }
            if (frame_ > 20U)
            {
                state_.move.Y = 1.0F;
            }
            ++frame_;
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
        cnahouse::player::InputState state_;
        Microsoft::Xna::Framework::Vector3 stoppedAt_;
        unsigned int frame_ = 0U;
        int stopMode_;
    } input(game, GetParam());

    game.SetInputSourceForTesting(&input);
    game.SetReviewFrameStepForTesting(true);
    game.SetFrameLimit(36U);
    game.Run();
    std::filesystem::current_path(original);
    EXPECT_EQ(game.ExitCode(), 0);
    EXPECT_FALSE(game.FilmingTourForTesting().Active());
    EXPECT_FALSE(game.FilmingTourForTesting().Completed());
    EXPECT_TRUE(game.Menus().Empty());
    EXPECT_GE(game.FramesDrawn(), 36U) << "Esc returns from the pause menu instead of exiting";
    const auto end = game.PlayerForTesting().Feet();
    const float distance = std::hypot(end.X - input.stoppedAt_.X, end.Z - input.stoppedAt_.Z);
    EXPECT_GT(distance, 0.2F);
    EXPECT_LT(distance, 1.5F) << "stopping never returns/teleports to a saved starting pose";
}

INSTANTIATE_TEST_SUITE_P(CAndEscAndTab, CinemaStopTests, testing::Values(0, 1, 2));

TEST(CinemaGameTests, CompleteActualGpuTourReview)
{
    if (std::getenv("HOUSE_CINEMA_GPU_REVIEW") == nullptr)
    {
        GTEST_SKIP() << "explicit full-tour capture opt-in; never open the owner's monitor";
    }
    ASSERT_TRUE(PrivateGpuAvailable());
    const auto original = std::filesystem::current_path();
    const auto buildRoot = std::filesystem::path(CNAHOUSE_TEST_CONTENT_ROOT).parent_path();
    const auto output = std::filesystem::path(CNAHOUSE_TEST_OUTPUT_DIR) / "cinema-review";
    std::filesystem::create_directories(output);
    std::set<std::filesystem::path> before;
    for (const auto& entry : std::filesystem::directory_iterator(buildRoot))
    {
        if (entry.path().filename().string().starts_with("cna-house-") && entry.path().extension() == ".png")
        {
            before.insert(entry.path());
        }
    }
    std::filesystem::current_path(buildRoot);
    cnahouse::util::Log::ResetForTesting();
    Options options;
    options.scene = "walk";
    options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
    options.noAudio = true;
    options.timeOfDay = 10.5F;
    options.freezeTime = true;
    options.weather = "W_CLEAR";
    Settings settings = Settings::Defaults();
    settings.backBufferWidth = 640;
    settings.backBufferHeight = 360;
    settings.verticalSync = false;
    CnaHouseGame game(options, settings);

    class ReviewInput final : public cnahouse::player::IInputSource
    {
    public:
        ReviewInput(CnaHouseGame& game, const std::filesystem::path& index)
            : game_(game)
            , index_(index)
        {
        }

        void Update(float) override
        {
            state_ = {};
            if (frame_ == 1U)
            {
                state_.cinemaPressed = true;
            }
            if (frame_ > 1U && game_.FilmingTourForTesting().Active())
            {
                EXPECT_LT(std::fabs(game_.ViewForTesting().Camera().Pose().pitch), 0.301F)
                    << "the actual game camera must show the room/path, not the pitch clamp";
            }
            if (frame_ > 1U &&
                (game_.FilmingTourForTesting().Completed() || game_.FilmingTourForTesting().Stuck()))
            {
                game_.Exit();
                return;
            }
            // Every frame still renders and checks actual camera pitch. Retain two
            // original PNGs per simulated second for the bounded full-tour review.
            if (frame_ % 15U == 0U)
            {
                state_.screenshotPressed = true;
                const auto feet = game_.PlayerForTesting().Feet();
                index_ << captures_++ << '\t' << frame_ << '\t' << game_.FilmingTourForTesting().Index()
                       << '\t' << cnahouse::util::IdRegistry::NameOf(game_.CellForTesting()) << '\t' << feet.X
                       << '\t' << feet.Y << '\t' << feet.Z << '\t'
                       << game_.ViewForTesting().Camera().Pose().yaw << '\t'
                       << game_.ViewForTesting().Camera().Pose().pitch << '\n';
            }
            ++frame_;
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
        cnahouse::player::InputState state_;
        std::ofstream index_;
        unsigned int frame_ = 0U;
        std::size_t captures_ = 0U;
    } input(game, output / "frames.tsv");

    ASSERT_TRUE(input.index_.is_open());
    game.SetInputSourceForTesting(&input);
    game.SetReviewFrameStepForTesting(true);
    game.SetFixedStepLimit(600000U);
    game.SetFrameLimit(160000U);
    game.Run();
    input.index_.flush();
    std::vector<std::filesystem::path> captures;
    for (const auto& entry : std::filesystem::directory_iterator(buildRoot))
    {
        if (entry.path().filename().string().starts_with("cna-house-") &&
            entry.path().extension() == ".png" && !before.contains(entry.path()))
        {
            captures.push_back(entry.path());
        }
    }
    std::sort(captures.begin(), captures.end());
    for (std::size_t i = 0; i < captures.size(); ++i)
    {
        char name[32];
        std::snprintf(name, sizeof(name), "%06zu.png", i);
        std::filesystem::rename(captures[i], output / name);
    }
    std::filesystem::current_path(original);
    EXPECT_EQ(game.ExitCode(), 0);
    EXPECT_EQ(captures.size(), input.captures_);
    EXPECT_FALSE(game.FilmingTourForTesting().Stuck()) << game.FilmingTourForTesting().Index();
    EXPECT_TRUE(game.FilmingTourForTesting().Completed());
    const auto& visited = game.FilmingTourForTesting().Visited();
    EXPECT_EQ(std::set<std::string>(visited.begin(), visited.end()).size(), 90U);
    std::printf(
        "actual GPU filming tour: %u frames, %llu fixed steps, %zu captures, %zu room/grounds views\n",
        input.frame_,
        static_cast<unsigned long long>(game.FixedStepsForTesting()),
        captures.size(),
        visited.size());
}
