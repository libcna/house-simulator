// SPDX-License-Identifier: MIT
//
// `HOUSE-00148` and `HOUSE-00149`.
#include <gtest/gtest.h>

#include <thread>

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/debug/Timing.hpp"

namespace
{
    using cnahouse::app::UpdateStage;
    using cnahouse::debug::Counter;
    using cnahouse::debug::Counters;
    using cnahouse::debug::Timing;

    TEST(CountersTests, AFreshCounterReportsZeroRatherThanGarbage)
    {
        Counters counters;
        const auto handle = counters.Resolve("draws");
        ASSERT_NE(handle, Counters::kInvalid);
        const Counter* counter = counters.Find("draws");
        ASSERT_NE(counter, nullptr);
        EXPECT_EQ(counter->Min(), 0);
        EXPECT_EQ(counter->Max(), 0);
        EXPECT_DOUBLE_EQ(counter->Average(), 0.0);
    }

    TEST(CountersTests, ResolvingTheSameNameTwiceGivesTheSameHandle)
    {
        // A system resolves once at construction; lookup by string every frame would put a string hash
        // on a per-frame path for no reason.
        Counters counters;
        EXPECT_EQ(counters.Resolve("draws"), counters.Resolve("draws"));
        EXPECT_NE(counters.Resolve("draws"), counters.Resolve("cells"));
        EXPECT_EQ(counters.All().size(), 2u);
    }

    TEST(CountersTests, TheWindowGivesMinAverageAndMax)
    {
        Counters counters;
        const auto handle = counters.Resolve("draws");
        for (int value : {10, 20, 30})
        {
            counters.Set(handle, value);
            counters.BeginFrame();
        }
        const Counter* counter = counters.Find("draws");
        ASSERT_NE(counter, nullptr);
        EXPECT_EQ(counter->Min(), 10);
        EXPECT_EQ(counter->Max(), 30);
        EXPECT_DOUBLE_EQ(counter->Average(), 20.0);
        EXPECT_EQ(counter->SampleCount(), 3u);
    }

    TEST(CountersTests, TheCurrentValueIsZeroedEachFrame)
    {
        // A counter accumulated with `Add` all frame must start the next frame at zero, or "draws this
        // frame" silently becomes "draws since launch".
        Counters counters;
        const auto handle = counters.Resolve("draws");
        counters.Add(handle, 5);
        counters.Add(handle, 5);
        ASSERT_EQ(counters.All()[handle].current, 10);
        counters.BeginFrame();
        EXPECT_EQ(counters.All()[handle].current, 0);
    }

    TEST(CountersTests, TheWindowIsBoundedAndForgetsOldSpikes)
    {
        // The point of a window: a spike four seconds ago is not what the player is looking at now.
        Counters counters;
        const auto handle = counters.Resolve("draws");
        counters.Set(handle, 10000);
        counters.BeginFrame();
        for (std::size_t i = 0; i < Counter::kWindow; ++i)
        {
            counters.Set(handle, 10);
            counters.BeginFrame();
        }
        const Counter* counter = counters.Find("draws");
        ASSERT_NE(counter, nullptr);
        EXPECT_EQ(counter->Max(), 10) << "the spike has aged out of the window";
        EXPECT_EQ(counter->SampleCount(), Counter::kWindow);
    }

    TEST(CountersTests, AnInvalidHandleIsIgnoredRatherThanCrashing)
    {
        // A system that failed to resolve must not take the frame down with it; the counter simply
        // reads zero, which is visibly wrong in the overlay and harmless everywhere else.
        Counters counters;
        counters.Add(Counters::kInvalid, 1);
        counters.Set(9999, 1);
        SUCCEED();
    }

    TEST(TimingTests, AScopeRecordsIntoItsStage)
    {
        Timing timing;
        {
            const Timing::Scope scope(timing, UpdateStage::Visibility);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        timing.BeginFrame();
        EXPECT_GT(timing.LastMilliseconds(UpdateStage::Visibility), 1.0);
        EXPECT_DOUBLE_EQ(timing.LastMilliseconds(UpdateStage::Audio), 0.0);
    }

    TEST(TimingTests, TimeWithinAFrameAccumulatesAcrossScopes)
    {
        // `Physics` runs up to four times per frame (`FrameTimer::kMaxFixedSteps`), and a budget cares
        // what the FRAME cost rather than what the last substep cost.
        Timing timing;
        for (int i = 0; i < 4; ++i)
        {
            const Timing::Scope scope(timing, UpdateStage::Physics);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        timing.BeginFrame();
        EXPECT_GT(timing.LastMilliseconds(UpdateStage::Physics), 3.0)
            << "four substeps of ~1 ms must total ~4 ms, not ~1 ms";
    }

    TEST(TimingTests, TheAccumulatorResetsEachFrame)
    {
        Timing timing;
        {
            const Timing::Scope scope(timing, UpdateStage::Animation);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        timing.BeginFrame();
        const double first = timing.LastMilliseconds(UpdateStage::Animation);
        ASSERT_GT(first, 1.0);

        timing.BeginFrame();
        EXPECT_DOUBLE_EQ(timing.LastMilliseconds(UpdateStage::Animation), 0.0)
            << "a stage that did no work this frame costs nothing, not what it cost last frame";
    }

    TEST(TimingTests, TheTotalIsTheSumOfTheStages)
    {
        Timing timing;
        timing.Record(UpdateStage::Input, 1.0);
        timing.Record(UpdateStage::Visibility, 2.0);
        timing.Record(UpdateStage::Audio, 0.5);
        timing.BeginFrame();
        EXPECT_NEAR(timing.TotalAverageMilliseconds(), 3.5, 1e-9);
    }

    TEST(TimingTests, MaxRemembersTheWorstFrameInTheWindow)
    {
        // The number that matters for a hitch. An average of 4 ms with a maximum of 40 is a very
        // different game from an average of 4 ms with a maximum of 5.
        Timing timing;
        timing.Record(UpdateStage::Visibility, 40.0);
        timing.BeginFrame();
        for (int i = 0; i < 10; ++i)
        {
            timing.Record(UpdateStage::Visibility, 4.0);
            timing.BeginFrame();
        }
        EXPECT_DOUBLE_EQ(timing.MaxMilliseconds(UpdateStage::Visibility), 40.0);
        EXPECT_LT(timing.AverageMilliseconds(UpdateStage::Visibility), 10.0);
    }

    TEST(TimingTests, ResetClearsEverything)
    {
        Timing timing;
        timing.Record(UpdateStage::Input, 5.0);
        timing.BeginFrame();
        timing.Reset();
        EXPECT_DOUBLE_EQ(timing.AverageMilliseconds(UpdateStage::Input), 0.0);
        EXPECT_DOUBLE_EQ(timing.TotalAverageMilliseconds(), 0.0);
    }

} // namespace
