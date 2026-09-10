// SPDX-License-Identifier: MIT
//
// `HOUSE-02391`. §26.1's selection metric and its hysteresis. The task's own acceptance is
// *"proving no oscillation at a boundary over a 600-frame approach"*, which is the last test here
// and the reason the deadband exists: a player walking slowly toward a neighbour crosses 90 px at
// a few millimetres a frame, and without hysteresis the house swaps mesh several times a second.
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/LodSelection.hpp"

namespace
{
    using cnahouse::visibility::Coarsen;
    using cnahouse::visibility::kLodHysteresis;
    using cnahouse::visibility::kLodThresholds;
    using cnahouse::visibility::LevelFor;
    using cnahouse::visibility::LodLevel;
    using cnahouse::visibility::LodThreshold;
    using cnahouse::visibility::ProjectedHeight;
    using cnahouse::visibility::SelectLod;

    /// §44's default lens at 1080p, which is what the thresholds were written against.
    constexpr float kViewportHeight = 1080.0F;
    constexpr float kFovY = cnahouse::player::kDefaultFovDegrees * std::numbers::pi_v<float> / 180.0F;

    /// The distance at which a sphere of @p radius projects to exactly @p height pixels --
    /// §26.1's formula solved the other way, so a test can stand an object ON a threshold rather
    /// than near one.
    float DistanceFor(float radius, float height)
    {
        return (2.0F * radius * kViewportHeight) / (2.0F * height * std::tan(kFovY * 0.5F));
    }

} // namespace

TEST(LodTests, TheMetricIsSection26sFormula)
{
    // Worked by hand from §26.1: h = 2·r·H / (2·d·tan(fovY/2)).
    const float radius = 5.0F;
    const float distance = 90.0F;
    const float expected = (2.0F * radius * kViewportHeight) / (2.0F * distance * std::tan(kFovY * 0.5F));
    EXPECT_NEAR(ProjectedHeight(radius, distance, kViewportHeight, kFovY), expected, 1e-3F);

    // Twice as far is half as tall, and twice as big is twice as tall. Both are the formula's
    // shape rather than its arithmetic, and a sign or a factor of two lands on one of them.
    const float near = ProjectedHeight(radius, 50.0F, kViewportHeight, kFovY);
    EXPECT_NEAR(ProjectedHeight(radius, 100.0F, kViewportHeight, kFovY), near * 0.5F, 1e-3F);
    EXPECT_NEAR(ProjectedHeight(radius * 2.0F, 50.0F, kViewportHeight, kFovY), near * 2.0F, 1e-3F);

    // Pixels, so the viewport is in it: the same house at 4K is twice the height it is at 1080p
    // and legitimately chooses a finer level.
    EXPECT_NEAR(ProjectedHeight(radius, 50.0F, kViewportHeight * 2.0F, kFovY), near * 2.0F, 1e-2F);
    // ...and a wider lens makes everything smaller, which is why the EFFECTIVE fov is the one to
    // pass: a window narrower than 4:3 opens the lens and every object shrinks with it.
    EXPECT_LT(ProjectedHeight(radius, 50.0F, kViewportHeight, kFovY * 1.4F), near);
}

TEST(LodTests, ACameraInsideTheSphereGetsAFiniteAnswer)
{
    // §26.1 clamps `d` to `> r` and this is why: the projection has no height to give when the
    // object surrounds the eye, and an unclamped 0 distance is a division by zero that reaches
    // the comparison as a NaN -- which compares false against every threshold and would silently
    // CULL the thing the camera is standing inside.
    const float height = ProjectedHeight(5.0F, 0.0F, kViewportHeight, kFovY);
    EXPECT_TRUE(std::isfinite(height));
    EXPECT_GT(height, 0.0F);
    EXPECT_EQ(LevelFor(height), LodLevel::Lod0);

    // A thing with no size projects to nothing, whatever the distance.
    EXPECT_FLOAT_EQ(ProjectedHeight(0.0F, 10.0F, kViewportHeight, kFovY), 0.0F);
    EXPECT_FLOAT_EQ(ProjectedHeight(-1.0F, 10.0F, kViewportHeight, kFovY), 0.0F);
    EXPECT_EQ(LevelFor(ProjectedHeight(0.0F, 10.0F, kViewportHeight, kFovY)), LodLevel::Culled);
    // A viewport or a lens of nothing is a frame with no pixels in it, not a NaN.
    EXPECT_FLOAT_EQ(ProjectedHeight(5.0F, 10.0F, 0.0F, kFovY), 0.0F);
    EXPECT_FLOAT_EQ(ProjectedHeight(5.0F, 10.0F, kViewportHeight, 0.0F), 0.0F);
}

TEST(LodTests, EachThresholdIsSection26sOwnNumber)
{
    EXPECT_FLOAT_EQ(LodThreshold(LodLevel::Lod0), 220.0F);
    EXPECT_FLOAT_EQ(LodThreshold(LodLevel::Lod1), 90.0F);
    EXPECT_FLOAT_EQ(LodThreshold(LodLevel::Lod2), 30.0F);
    EXPECT_FLOAT_EQ(LodThreshold(LodLevel::Impostor), 8.0F);
    EXPECT_FLOAT_EQ(LodThreshold(LodLevel::Culled), 0.0F)
        << "culled is what is left below the last threshold; it has none of its own";

    // Exactly ON the threshold is the finer level: §26.1's table says `h >= 220 px`, and the one
    // place a `>` would hide is the boundary every hysteresis test then stands on.
    EXPECT_EQ(LevelFor(220.0F), LodLevel::Lod0);
    EXPECT_EQ(LevelFor(219.99F), LodLevel::Lod1);
    EXPECT_EQ(LevelFor(90.0F), LodLevel::Lod1);
    EXPECT_EQ(LevelFor(89.99F), LodLevel::Lod2);
    EXPECT_EQ(LevelFor(30.0F), LodLevel::Lod2);
    EXPECT_EQ(LevelFor(29.99F), LodLevel::Impostor);
    EXPECT_EQ(LevelFor(8.0F), LodLevel::Impostor);
    EXPECT_EQ(LevelFor(7.99F), LodLevel::Culled);
    EXPECT_EQ(LevelFor(0.0F), LodLevel::Culled);
}

TEST(LodTests, TheDeadbandIsBelowTheLevelYouAreAtAndNotTheOneYouWouldFallTo)
{
    // At LOD1 (90 px) the object holds LOD1 down to 76.5 px -- 0.85 x 90 -- and takes LOD1 again
    // only back at 90. A deadband measured against the level BELOW would be 0.85 x 30 = 25.5,
    // which is not a deadband at all: it would let the object fall to LOD2 at 89.99.
    EXPECT_EQ(SelectLod(85.0F, LodLevel::Lod1), LodLevel::Lod1);
    EXPECT_EQ(SelectLod(77.0F, LodLevel::Lod1), LodLevel::Lod1);
    EXPECT_EQ(SelectLod(76.0F, LodLevel::Lod1), LodLevel::Lod2);
    EXPECT_EQ(SelectLod(85.0F, LodLevel::Lod2), LodLevel::Lod2) << "not finer until 90";
    EXPECT_EQ(SelectLod(90.0F, LodLevel::Lod2), LodLevel::Lod1);

    // The same shape at every boundary, from the constants rather than from four copied numbers.
    for (std::size_t index = 0; index < kLodThresholds.size(); ++index)
    {
        const auto level = static_cast<LodLevel>(index);
        const float threshold = kLodThresholds[index];
        EXPECT_EQ(SelectLod(threshold * 0.99F, level), level)
            << cnahouse::visibility::LodLevelName(level) << " fell at 1 % under its threshold";
        EXPECT_EQ(SelectLod(threshold * (kLodHysteresis - 0.01F), level), static_cast<LodLevel>(index + 1u))
            << cnahouse::visibility::LodLevelName(level) << " held past its deadband";
    }
}

TEST(LodTests, AnObjectSeenForTheFirstTimeTakesTheMetricsAnswer)
{
    // `Culled` is the seed for something never drawn, and from there the deadband must not hold it
    // back: an impostor that pops into view has no level to be sticky about.
    EXPECT_EQ(SelectLod(300.0F, LodLevel::Culled), LodLevel::Lod0);
    EXPECT_EQ(SelectLod(50.0F, LodLevel::Culled), LodLevel::Lod2);
    EXPECT_EQ(SelectLod(9.0F, LodLevel::Culled), LodLevel::Impostor);
    EXPECT_EQ(SelectLod(1.0F, LodLevel::Culled), LodLevel::Culled);
}

TEST(LodTests, ACameraCutCrossesEveryBandItNeedsToInOneCall)
{
    // The deadband absorbs dithering; it must not slow a REAL change down. A cut from the street
    // to the far side of the world is one frame, and an object that took four frames to reach its
    // level would draw a LOD0 house at 3 px for three of them.
    EXPECT_EQ(SelectLod(1.0F, LodLevel::Lod0), LodLevel::Culled);
    EXPECT_EQ(SelectLod(400.0F, LodLevel::Culled), LodLevel::Lod0);
    EXPECT_EQ(SelectLod(35.0F, LodLevel::Lod0), LodLevel::Lod2);
}

TEST(LodTests, TheBiasIsAppliedToTheSelectedLevelAndSaturates)
{
    EXPECT_EQ(Coarsen(LodLevel::Lod0, 1), LodLevel::Lod1);
    EXPECT_EQ(Coarsen(LodLevel::Lod0, 2), LodLevel::Lod2);
    EXPECT_EQ(Coarsen(LodLevel::Lod1, -1), LodLevel::Lod0);
    EXPECT_EQ(Coarsen(LodLevel::Lod0, 0), LodLevel::Lod0);
    // §15.4's "+1 through frosted glass" and §26.1's Web/Android "+1" are the same operation, and
    // both saturate rather than wrapping: a bias of +9 is culled, not LOD0 again.
    EXPECT_EQ(Coarsen(LodLevel::Lod0, 9), LodLevel::Culled);
    EXPECT_EQ(Coarsen(LodLevel::Culled, 1), LodLevel::Culled);
    EXPECT_EQ(Coarsen(LodLevel::Lod0, -9), LodLevel::Lod0);
}

TEST(LodTests, NoOscillationOverA600FrameApproachAcrossABoundary)
{
    // The task's own acceptance. A neighbour house -- §11.4's LOD0 band starts at 90 m -- with a
    // 7 m bounding sphere, approached at walking pace across LOD1's 90 px boundary and then walked
    // back again. Real motion, so the height dithers over the threshold the way a body's does.
    constexpr float radius = 7.0F;
    const float boundary = DistanceFor(radius, kLodThresholds[1]);

    std::vector<LodLevel> seen;
    LodLevel level = LodLevel::Culled;
    // 300 frames in, 300 back out, straddling the boundary by a few centimetres either way -- the
    // worst case for a selector, and the one a player standing still at a doorway produces.
    for (int frame = 0; frame < 600; ++frame)
    {
        const float phase = static_cast<float>(frame < 300 ? frame : 600 - frame);
        const float distance = boundary + 0.30F - phase * 0.002F;
        level = SelectLod(ProjectedHeight(radius, distance, kViewportHeight, kFovY), level);
        seen.push_back(level);
    }

    int changes = 0;
    for (std::size_t i = 1; i < seen.size(); ++i)
    {
        changes += seen[i] != seen[i - 1] ? 1 : 0;
    }
    // In and out again is at most two switches. Without the deadband this same walk crosses the
    // threshold on almost every frame of the middle: the assertion is not "few" but "the number a
    // single approach and a single retreat can honestly produce".
    EXPECT_LE(changes, 2) << "the level changed " << changes << " times over 600 frames";
    EXPECT_GE(changes, 1) << "the walk never crossed the boundary at all, so it proves nothing";
}

TEST(LodTests, WithoutTheDeadbandTheSameWalkWouldChatter)
{
    // The control for the test above: the same 600 frames put through the metric ALONE. If this
    // did not chatter, the one above would pass with the hysteresis removed and would be proving
    // nothing -- which is exactly how a deadband test goes quietly wrong.
    constexpr float radius = 7.0F;
    const float boundary = DistanceFor(radius, kLodThresholds[1]);

    int changes = 0;
    LodLevel previous = LodLevel::Culled;
    for (int frame = 0; frame < 600; ++frame)
    {
        // A hover, not an approach: a body standing at the boundary with §44's head bob on it.
        const float distance = boundary + 0.02F * std::sin(static_cast<float>(frame) * 0.7F);
        const LodLevel level = LevelFor(ProjectedHeight(radius, distance, kViewportHeight, kFovY));
        changes += frame > 0 && level != previous ? 1 : 0;
        previous = level;
    }
    EXPECT_GT(changes, 50) << "the control did not chatter, so the deadband test proves nothing";

    // ...and the selector with the deadband, over the identical motion, does not move at all.
    LodLevel level = LodLevel::Lod1;
    int held = 0;
    for (int frame = 0; frame < 600; ++frame)
    {
        const float distance = boundary + 0.02F * std::sin(static_cast<float>(frame) * 0.7F);
        const LodLevel next = SelectLod(ProjectedHeight(radius, distance, kViewportHeight, kFovY), level);
        held += next != level ? 1 : 0;
        level = next;
    }
    EXPECT_EQ(held, 0) << "the deadband let a 2 cm hover move the level " << held << " times";
}
