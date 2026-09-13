// SPDX-License-Identifier: MIT
//
// `HOUSE-00158`. This is an INTEGRATION test rather than a unit test for one reason: `StateTracker`
// exists to talk to a `GraphicsDevice`, and a test with a mock device in place of a real one would
// verify only that the mock and the tracker agree. Under the `headless` preset it gets a real
// device with no window, so `setBlendStateProperty` and friends are really called and a state the
// device rejects is a failure here rather than in a nightly render job.
#include <functional>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"

#include "cnahouse/rendering/RenderStates.hpp"
#include "cnahouse/rendering/StateTracker.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using cnahouse::rendering::CullPolicy;
    using cnahouse::rendering::StateFor;
    using cnahouse::rendering::StateTracker;

    StateTracker::Counts RunWithTracker(const std::function<void(StateTracker&)>& body)
    {
        StateTracker::Counts counts;
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                StateTracker tracker(device);
                body(tracker);
                counts = tracker.Current();
            });
        host.Run();
        EXPECT_TRUE(host.Ran()) << "the frame that does the measuring never ran";
        EXPECT_EQ(host.Failure(), "") << "the device rejected a state the tracker set";
        return counts;
    }

    TEST(StateTrackerTests, TheFirstSetOfEachKindIsAppliedAndTheRepeatIsSkipped)
    {
        const auto counts = RunWithTracker(
            [](StateTracker& tracker)
            {
                tracker.SetBlend(Gfx::BlendState::Opaque);
                tracker.SetBlend(Gfx::BlendState::Opaque);
                tracker.SetDepthStencil(Gfx::DepthStencilState::Default);
                tracker.SetDepthStencil(Gfx::DepthStencilState::Default);
                tracker.SetRasterizer(StateFor(CullPolicy::ImportedFront));
                tracker.SetRasterizer(StateFor(CullPolicy::ImportedFront));
            });

        EXPECT_EQ(counts.blendApplied, 1u);
        EXPECT_EQ(counts.blendSkipped, 1u);
        EXPECT_EQ(counts.depthApplied, 1u);
        EXPECT_EQ(counts.depthSkipped, 1u);
        EXPECT_EQ(counts.rasterApplied, 1u);
        EXPECT_EQ(counts.rasterSkipped, 1u);
        EXPECT_EQ(counts.TotalApplied(), 3u);
        EXPECT_EQ(counts.TotalSkipped(), 3u);
    }

    TEST(StateTrackerTests, ADifferentStateOfTheSameKindIsNotSkipped)
    {
        // The failure this rules out is a tracker that compares by KIND rather than by object and
        // therefore never changes anything after the first frame.
        const auto counts = RunWithTracker(
            [](StateTracker& tracker)
            {
                tracker.SetRasterizer(StateFor(CullPolicy::ImportedFront));
                tracker.SetRasterizer(StateFor(CullPolicy::Mirrored));
                tracker.SetRasterizer(StateFor(CullPolicy::TwoSided));
                tracker.SetBlend(Gfx::BlendState::Opaque);
                tracker.SetBlend(Gfx::BlendState::AlphaBlend);
            });

        EXPECT_EQ(counts.rasterApplied, 3u);
        EXPECT_EQ(counts.rasterSkipped, 0u);
        EXPECT_EQ(counts.blendApplied, 2u);
        EXPECT_EQ(counts.blendSkipped, 0u);
    }

    TEST(StateTrackerTests, InvalidateForcesTheNextSetOfEveryKind)
    {
        // The case this protects: `SpriteBatch::End` restores several states at once, so a tracker
        // that kept believing what it set before the batch would skip the set that was needed and
        // draw the next thing with the sprite batch's blend state.
        const auto counts = RunWithTracker(
            [](StateTracker& tracker)
            {
                tracker.SetBlend(Gfx::BlendState::Opaque);
                tracker.SetDepthStencil(Gfx::DepthStencilState::Default);
                tracker.SetRasterizer(StateFor(CullPolicy::ImportedFront));

                tracker.Invalidate();

                tracker.SetBlend(Gfx::BlendState::Opaque);
                tracker.SetDepthStencil(Gfx::DepthStencilState::Default);
                tracker.SetRasterizer(StateFor(CullPolicy::ImportedFront));
            });

        EXPECT_EQ(counts.TotalApplied(), 6u) << "every one of the three was set twice, for real";
        EXPECT_EQ(counts.TotalSkipped(), 0u);
    }

    TEST(StateTrackerTests, BeginFrameMovesTheCountsToLastFrameAndInvalidates)
    {
        // The counters are the point of the class -- a renderer that changes blend state 900 times
        // to issue 300 draws is sorting its render list wrongly, and that is invisible without a
        // per-frame number to look at.
        RunWithTracker(
            [](StateTracker& tracker)
            {
                tracker.SetBlend(Gfx::BlendState::Opaque);
                tracker.SetBlend(Gfx::BlendState::Opaque);
                EXPECT_EQ(tracker.Current().blendApplied, 1u);
                EXPECT_EQ(tracker.Current().blendSkipped, 1u);

                tracker.BeginFrame();

                EXPECT_EQ(tracker.LastFrame().blendApplied, 1u) << "the finished frame is readable";
                EXPECT_EQ(tracker.LastFrame().blendSkipped, 1u);
                EXPECT_EQ(tracker.Current().TotalApplied(), 0u) << "the new frame starts at zero";
                EXPECT_EQ(tracker.Current().TotalSkipped(), 0u);

                // And it invalidated, so this is a real set rather than a skip.
                tracker.SetBlend(Gfx::BlendState::Opaque);
                EXPECT_EQ(tracker.Current().blendApplied, 1u);
                EXPECT_EQ(tracker.Current().blendSkipped, 0u);
            });
    }

} // namespace
