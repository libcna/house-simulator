// SPDX-License-Identifier: MIT
//
// `HOUSE-00629`. The dip the eye does when a fall ends, and the way it comes back.
//
// §43.1 gives the fall -- terminal 12 m/s, a hard landing past 2.4 m -- and §47.2 gives the
// `land_soft` / `land_hard` one-shot it triggers. What the camera does at that moment is this
// task's, and the shape is the whole of it: the eye must not JUMP on the landing frame, must not
// bounce coming back, and must draw the same curve on a machine at 30 frames a second and one at
// 240, because a quarter of a second is long enough to watch.
#include <algorithm>
#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/physics/Fall.hpp"
#include "cnahouse/player/LandingDip.hpp"

namespace
{
    using cnahouse::physics::kHardLandingDrop;
    using cnahouse::player::kDipOmega;
    using cnahouse::player::kDipPerMetre;
    using cnahouse::player::kMaxDip;
    using cnahouse::player::LandingDip;

    /// Runs the dip for @p seconds and keeps every sample.
    std::vector<float> Recover(LandingDip& dip, float seconds, float dt)
    {
        std::vector<float> offsets;
        for (float t = 0.0F; t < seconds; t += dt)
        {
            dip.Update(dt);
            offsets.push_back(dip.Offset());
        }
        return offsets;
    }

    float Deepest(const std::vector<float>& offsets)
    {
        return *std::min_element(offsets.begin(), offsets.end());
    }

} // namespace

TEST(LandingDipTests, TheDepthIsTheDropUntilTheKneesRunOut)
{
    EXPECT_FLOAT_EQ(kDipPerMetre, 0.035F);
    EXPECT_FLOAT_EQ(kMaxDip, 0.09F);
    EXPECT_FLOAT_EQ(kDipOmega, 16.0F);

    // §43.1's hard landing is 2.4 m, and that is where the linear part is still in charge.
    EXPECT_FLOAT_EQ(LandingDip::DepthFor(kHardLandingDrop), kDipPerMetre * kHardLandingDrop);
    EXPECT_LT(LandingDip::DepthFor(kHardLandingDrop), kMaxDip);

    // Past it the cap takes over: a fall from the roof and a fall from the first floor land the
    // same way, because the knees have already done everything they can.
    EXPECT_FLOAT_EQ(LandingDip::DepthFor(10.0F), kMaxDip);
    EXPECT_FLOAT_EQ(LandingDip::DepthFor(100.0F), kMaxDip);

    // Stepping off a kerb is a tenth of §44's head bob, which is the point: the dip is a landing
    // and not a jolt at every doorstep.
    EXPECT_NEAR(LandingDip::DepthFor(0.10F), 0.0035F, 1e-6F);
    EXPECT_FLOAT_EQ(LandingDip::DepthFor(0.0F), 0.0F);
    EXPECT_FLOAT_EQ(LandingDip::DepthFor(-1.0F), 0.0F) << "a fall upwards is not a landing";
}

TEST(LandingDipTests, TheEyeDoesNotJumpOnTheLandingFrameAndReachesTheDepthAfterIt)
{
    // What lands is a VELOCITY. A dip applied by moving the eye down and letting it spring back
    // starts with a step -- the eye is somewhere else on the landing frame than it was on the one
    // before -- and that step is the pop the dip exists to smooth.
    LandingDip dip;
    dip.Land(kHardLandingDrop);
    EXPECT_FLOAT_EQ(dip.Offset(), 0.0F) << "the eye moved before a single frame was integrated";
    EXPECT_LT(dip.Velocity(), 0.0F) << "the landing did not push the eye downwards";

    const float dt = 1.0F / 240.0F;
    const std::vector<float> offsets = Recover(dip, 0.5F, dt);

    // The bottom of the dip is the depth the drop asks for...
    EXPECT_NEAR(Deepest(offsets), -LandingDip::DepthFor(kHardLandingDrop), 1e-4F);

    // ...and it arrives at 1/ω, which is where a critically damped spring kicked from rest turns
    // round. 62 ms: fast enough to read as an impact rather than as the floor sinking.
    const auto lowest =
        static_cast<std::size_t>(std::min_element(offsets.begin(), offsets.end()) - offsets.begin());
    EXPECT_NEAR(static_cast<float>(lowest + 1) * dt, 1.0F / kDipOmega, 0.01F);
}

TEST(LandingDipTests, TheRecoveryComesBackWithoutBouncing)
{
    // Critically damped is the whole specification: any less and the eye rises past where it
    // started and comes back down, which is a nod the player did not do.
    LandingDip dip;
    dip.Land(1.0F);
    const std::vector<float> offsets = Recover(dip, 0.5F, 1.0F / 240.0F);

    for (const float offset : offsets)
    {
        EXPECT_LE(offset, 0.0F) << "the landing lifted the eye";
    }

    // Nine tenths of the way home by a third of a second, and the last millimetre gone by 0.5 s:
    // a fast give
    // and a slow return, which is the shape of a knee rather than of a spring being let go.
    const float depth = LandingDip::DepthFor(1.0F);
    EXPECT_LT(std::abs(offsets[static_cast<std::size_t>(80U)]), 0.1F * depth);
    EXPECT_LT(std::abs(offsets.back()), 1e-3F);

    // ...and monotone on the way back up, which is the same statement without a tolerance in it.
    const auto lowest =
        static_cast<std::size_t>(std::min_element(offsets.begin(), offsets.end()) - offsets.begin());
    for (std::size_t i = lowest + 1; i < offsets.size(); ++i)
    {
        EXPECT_GE(offsets[i], offsets[i - 1]) << "the recovery went back down at " << i;
    }
}

TEST(LandingDipTests, TheSameCurveAtEveryFrameRate)
{
    // The dip is a SHAPE the player watches, so it has to be the same shape everywhere. This is
    // why the spring is stepped by its closed form rather than integrated: between impulses the
    // target is zero and stays zero, so the exact solution is available -- which it is not for
    // `EyeSpring`, whose target moves every frame.
    // Sampled at three times that every one of these frame rates lands on exactly, so what is
    // compared is the CURVE and not where the samples happened to fall on it.
    const float rates[] = {30.0F, 60.0F, 120.0F, 240.0F};
    for (const float when : {0.1F, 0.2F, 0.3F})
    {
        float first = 0.0F;
        for (std::size_t i = 0; i < std::size(rates); ++i)
        {
            LandingDip dip;
            dip.Land(2.0F);
            const int frames = static_cast<int>(std::lround(when * rates[i]));
            for (int frame = 0; frame < frames; ++frame)
            {
                dip.Update(1.0F / rates[i]);
            }
            if (i == 0)
            {
                first = dip.Offset();
                EXPECT_LT(first, -1e-3F) << "the fixture measured a dip that had already gone at " << when;
            }
            EXPECT_NEAR(dip.Offset(), first, 1e-5F) << "at " << rates[i] << " fps, " << when << " s in";
        }
    }
}

TEST(LandingDipTests, LandingAgainMidDipTakesTheDeeperOfTheTwo)
{
    // Two landings inside a quarter of a second is a bounce down a flight of stairs. Adding their
    // dips would put the eye through the tread.
    const float dt = 1.0F / 240.0F;

    LandingDip once;
    once.Land(kHardLandingDrop);
    std::vector<float> alone = Recover(once, 0.5F, dt);

    LandingDip twice;
    twice.Land(kHardLandingDrop);
    Recover(twice, 0.03F, dt);
    twice.Land(0.30F); // a kerb, in the middle of the fall's own dip
    const std::vector<float> both = Recover(twice, 0.5F, dt);

    EXPECT_NEAR(Deepest(both), Deepest(alone), 2e-3F) << "the shallower landing deepened the dip";

    // ...and the deeper one arriving second does take over, or the rule would be "the first wins".
    LandingDip deeper;
    deeper.Land(0.30F);
    Recover(deeper, 0.03F, dt);
    deeper.Land(kHardLandingDrop);
    EXPECT_LT(Deepest(Recover(deeper, 0.5F, dt)), -LandingDip::DepthFor(kHardLandingDrop) * 0.9F);
}

TEST(LandingDipTests, ABounceDownAFlightStaysInsideTheKneesEitherWay)
{
    // The bound the "deeper wins" rule leaves: an impulse arriving while the eye is already down
    // starts from where it is, so a pathological sequence can go past `kMaxDip` -- but not far,
    // and never anywhere near the 1.68 m the eye has under it. Sixteen risers, landed on each.
    LandingDip dip;
    const float dt = 1.0F / 240.0F;
    float deepest = 0.0F;
    for (int riser = 0; riser < 16; ++riser)
    {
        dip.Land(0.18F);
        for (int frame = 0; frame < 12; ++frame)
        {
            dip.Update(dt);
            deepest = std::min(deepest, dip.Offset());
        }
    }
    EXPECT_GT(deepest, -2.0F * kMaxDip);
}

TEST(LandingDipTests, NothingHappensWithoutALandingOrATick)
{
    LandingDip dip;
    Recover(dip, 0.1F, 1.0F / 120.0F);
    EXPECT_FLOAT_EQ(dip.Offset(), 0.0F) << "the eye dipped without landing on anything";

    dip.Land(0.0F);
    EXPECT_FLOAT_EQ(dip.Velocity(), 0.0F);

    // A paused frame advances nothing rather than dividing by it.
    dip.Land(2.0F);
    const float velocity = dip.Velocity();
    dip.Update(0.0F);
    dip.Update(-0.5F);
    EXPECT_FLOAT_EQ(dip.Offset(), 0.0F);
    EXPECT_FLOAT_EQ(dip.Velocity(), velocity);

    // ...and a teleport puts the eye back where it was with no motion left in it (§57).
    Recover(dip, 0.05F, 1.0F / 120.0F);
    ASSERT_LT(dip.Offset(), 0.0F);
    dip.Reset();
    EXPECT_FLOAT_EQ(dip.Offset(), 0.0F);
    EXPECT_FLOAT_EQ(dip.Velocity(), 0.0F);
}
