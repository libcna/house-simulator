// SPDX-License-Identifier: MIT
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/player/FilmingTour.hpp"

namespace
{
    const std::string kRoute = R"({"filmingTour":{"route":[
        {"feet":[0,0,0],"cell":"ROOM_A","crouched":false,"pauseSeconds":8},
        {"feet":[1,0,0],"cell":"ROOM_B","crouched":false,"pauseSeconds":8},
        {"feet":[2,0,0],"cell":"ROOM_C","crouched":true,"pauseSeconds":8}]}})";

    std::string Fixture(const std::string& text)
    {
        const auto path = std::filesystem::path(CNAHOUSE_TEST_OUTPUT_DIR) / "house-03572-policy.json";
        std::ofstream file(path);
        file << text;
        return path.string();
    }
} // namespace

TEST(FilmingTourPolicyTests, OptionalDataAndMalformedDataDoNotKeepAStaleRoute)
{
    cnahouse::player::FilmingTour tour;
    ASSERT_TRUE(tour.Load(Fixture(kRoute)));
    EXPECT_EQ(tour.Route().size(), 3U);
    ASSERT_TRUE(tour.Load(Fixture("{}")));
    EXPECT_TRUE(tour.Route().empty());
    EXPECT_FALSE(tour.Load(Fixture(
        R"({"filmingTour":{"route":[{"feet":[0,0,0],"cell":"ROOM_A","crouched":false,"pauseSeconds":-1}]}})")));
    EXPECT_TRUE(tour.Route().empty());
    EXPECT_FALSE(tour.Load(Fixture(R"({"filmingTour":{"route":true}})")));
}

TEST(FilmingTourPolicyTests, StartingElsewhereFollowsTheForwardLoopWithoutWritingTheBody)
{
    cnahouse::player::FilmingTour tour;
    ASSERT_TRUE(tour.Load(Fixture(kRoute)));
    cnahouse::player::PlayerState body;
    body.position = {1.0F, body.Rise(), 0.0F};
    const auto original = body.position;
    cnahouse::player::LookAngles look;
    cnahouse::physics::CollisionWorld collision;
    collision.cells.emplace_back().id = "ROOM_A";
    collision.cells.emplace_back().id = "ROOM_B";
    cnahouse::physics::BroadPhase broad;
    ASSERT_TRUE(tour.Start(body, "ROOM_B", collision, broad));
    const auto input = tour.Step(body, look, 1.0F / 120.0F);
    EXPECT_EQ(input.move.X, 0.0F);
    EXPECT_EQ(tour.Index(), 1U);
    ASSERT_EQ(tour.Visited().size(), 1U);
    EXPECT_EQ(tour.Visited().front(), "ROOM_B");
    EXPECT_EQ(body.position, original) << "the policy may only emit intent";
    (void)tour.Step(body, look, 8.0F);
    EXPECT_EQ(tour.Index(), 2U);
    EXPECT_GT(tour.Step(body, look, 1.0F / 120.0F).move.X, 0.0F);
    tour.Stop();
    EXPECT_FALSE(tour.Active());
    EXPECT_EQ(tour.Step(body, look, 1.0F / 120.0F).move.X, 0.0F);
    EXPECT_FALSE(tour.Start(body, "MISSING_ROOM", collision, broad));
}

TEST(FilmingTourPolicyTests, PanoramaPitchConvergesThroughTheRealMouseLookConvention)
{
    for (const float initialPitch : {-0.7F, 0.0F, 0.7F})
    {
        cnahouse::player::FilmingTour tour;
        ASSERT_TRUE(tour.Load(Fixture(kRoute)));
        cnahouse::player::PlayerState body;
        body.position = {1.0F, body.Rise(), 0.0F};
        cnahouse::player::LookAngles look;
        look.pitch = initialPitch;
        cnahouse::physics::CollisionWorld collision;
        collision.cells.emplace_back().id = "ROOM_B";
        cnahouse::physics::BroadPhase broad;
        ASSERT_TRUE(tour.Start(body, "ROOM_B", collision, broad));
        for (unsigned int step = 0; step < 480U; ++step)
        {
            const auto input = tour.Step(body, look, 1.0F / 120.0F);
            EXPECT_LE(std::fabs(input.look.Y), 0.002501F);
            cnahouse::player::ApplyLook(look, input, true);
        }
        EXPECT_NEAR(look.pitch, -3.14159265359F / 36.0F, 1.0e-4F)
            << "panorama must show the room, not diverge to the +85 degree ceiling clamp";
    }
}

TEST(FilmingTourPolicyTests, JoiningNeverCrossesASolidSameCellWallOrEnablesNoclip)
{
    cnahouse::player::FilmingTour tour;
    ASSERT_TRUE(tour.Load(Fixture(kRoute)));
    cnahouse::player::PlayerState body;
    body.position = {0.0F, body.Rise(), 0.0F};
    const auto original = body.position;
    cnahouse::physics::CollisionWorld collision;
    auto& cell = collision.cells.emplace_back();
    cell.id = "ROOM_B";
    cell.nx = 3U;
    cell.nz = 2U;
    cell.originX = -1.0F;
    cell.originZ = -1.0F;
    cell.shapes = {0U};
    cell.buckets.assign(6U, {0U});
    cnahouse::physics::CollisionObb wall;
    wall.centre = {0.5F, 1.0F, 0.0F};
    wall.halfExtents = {0.05F, 1.0F, 1.0F};
    collision.obbs.push_back(wall);
    cnahouse::physics::BroadPhase broad;
    EXPECT_FALSE(tour.Start(body, "ROOM_B", collision, broad));
    EXPECT_FALSE(tour.Active());
    EXPECT_EQ(body.position, original);
    body.noclip = true;
    EXPECT_FALSE(tour.Start(body, "ROOM_B", collision, broad));
}

TEST(FilmingTourPolicyTests, ABlockedWalkStopsInPlaceInsteadOfSkippingOrTeleporting)
{
    cnahouse::player::FilmingTour tour;
    ASSERT_TRUE(tour.Load(Fixture(kRoute)));
    cnahouse::player::PlayerState body;
    body.position = {1.0F, body.Rise(), 0.0F};
    const auto original = body.position;
    cnahouse::player::LookAngles look;
    cnahouse::physics::CollisionWorld collision;
    collision.cells.emplace_back().id = "ROOM_B";
    cnahouse::physics::BroadPhase broad;
    ASSERT_TRUE(tour.Start(body, "ROOM_B", collision, broad));
    (void)tour.Step(body, look, 0.01F);
    (void)tour.Step(body, look, 8.0F);
    for (unsigned int step = 0; step < 130U && tour.Active(); ++step)
    {
        (void)tour.Step(body, look, 0.1F); // The real controller is unable to make progress.
    }
    EXPECT_FALSE(tour.Active());
    EXPECT_TRUE(tour.Stuck());
    EXPECT_FALSE(tour.Completed());
    EXPECT_EQ(tour.Index(), 2U);
    ASSERT_EQ(tour.Visited().size(), 1U);
    EXPECT_EQ(body.position, original);
    EXPECT_EQ(tour.Step(body, look, 0.1F).move, Microsoft::Xna::Framework::Vector2());
}
