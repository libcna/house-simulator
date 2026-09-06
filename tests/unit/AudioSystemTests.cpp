// SPDX-License-Identifier: MIT
//
// `HOUSE-00154` and the mix half of `HOUSE-00155`. Everything here runs without an audio device and
// without a `Game`, which is the point: the states this class exists to handle are the ones where
// there IS no device, and a test that needed one could not reach them.
#include <gtest/gtest.h>

#include "cnahouse/audio/AudioSystem.hpp"

namespace
{
    using cnahouse::audio::AudioState;
    using cnahouse::audio::AudioSystem;
    using cnahouse::audio::Category;
    using cnahouse::audio::CategoryName;

    TEST(AudioSystemTests, NoAudioIsSilentImmediatelyAndNeverTouchesTheDevice)
    {
        // `--no-audio` is the option someone reaches for when the device is what is broken, so it
        // must not be implemented as "try, then fail gracefully".
        const AudioSystem audio(false);
        EXPECT_EQ(audio.State(), AudioState::Silent);
        EXPECT_FALSE(audio.IsReady());
        EXPECT_FALSE(audio.IsWaitingForGesture()) << "it is not waiting; it is deliberately off";
        EXPECT_NE(audio.SilentReason().find("--no-audio"), std::string::npos) << audio.SilentReason();
    }

    TEST(AudioSystemTests, AudioStartsWaitingForAGestureRatherThanReady)
    {
        // A browser refuses to open an audio device before the user has interacted with the page.
        // The gate is uniform on every platform so the path is exercised in every build.
        const AudioSystem audio(true);
        EXPECT_EQ(audio.State(), AudioState::Waiting);
        EXPECT_TRUE(audio.IsWaitingForGesture());
        EXPECT_FALSE(audio.IsReady()) << "waiting is not ready, and playing here would be silent";
    }

    TEST(AudioSystemTests, TheGestureIsIdempotentSoItCanBeWiredToEveryKeyPress)
    {
        AudioSystem audio(false); // silent, so this test needs no device at all
        EXPECT_FALSE(audio.NoteUserGesture()) << "already decided; nothing to change";
        EXPECT_FALSE(audio.NoteUserGesture());
        EXPECT_EQ(audio.State(), AudioState::Silent);
    }

    TEST(AudioSystemTests, VolumeIsZeroUntilAudioIsActuallyReady)
    {
        // 0 rather than the mix, so a caller that forgot to check cannot play into a device that is
        // not there. Silence is the supported behaviour, not a failure to be worked around.
        const AudioSystem waiting(true);
        EXPECT_FLOAT_EQ(waiting.EffectiveVolume(Category::World), 0.0f);

        const AudioSystem off(false);
        EXPECT_FLOAT_EQ(off.EffectiveVolume(Category::World), 0.0f);
        EXPECT_FLOAT_EQ(off.EffectiveVolume(Category::Ui), 0.0f);
    }

    TEST(AudioSystemTests, TheCategoryDefaultsAreTheOnesSectionSixtyEightNames)
    {
        const AudioSystem audio(true);
        EXPECT_FLOAT_EQ(audio.MasterVolume(), 0.80f);
        EXPECT_FLOAT_EQ(audio.CategoryVolume(Category::Ambience), 0.75f);
        EXPECT_FLOAT_EQ(audio.CategoryVolume(Category::World), 1.00f);
        EXPECT_FLOAT_EQ(audio.CategoryVolume(Category::Animals), 0.90f);
        EXPECT_FLOAT_EQ(audio.CategoryVolume(Category::Media), 0.70f);
        EXPECT_FLOAT_EQ(audio.CategoryVolume(Category::Ui), 0.60f);
    }

    TEST(AudioSystemTests, VolumesAreClampedRatherThanTrusted)
    {
        // They arrive from `settings.json`, which is user-editable by design, and a master volume of
        // 4.0 would not be loud -- it would clip every mix on the machine.
        AudioSystem audio(true);
        audio.SetMasterVolume(4.0f);
        EXPECT_FLOAT_EQ(audio.MasterVolume(), 1.0f);
        audio.SetMasterVolume(-1.0f);
        EXPECT_FLOAT_EQ(audio.MasterVolume(), 0.0f);

        audio.SetCategoryVolume(Category::Ui, 9.0f);
        EXPECT_FLOAT_EQ(audio.CategoryVolume(Category::Ui), 1.0f);
        audio.SetCategoryVolume(Category::Ui, -9.0f);
        EXPECT_FLOAT_EQ(audio.CategoryVolume(Category::Ui), 0.0f);
    }

    TEST(AudioSystemTests, TheCountSentinelIsNotAUsableCategory)
    {
        AudioSystem audio(true);
        audio.SetCategoryVolume(Category::Count, 1.0f);
        EXPECT_FLOAT_EQ(audio.CategoryVolume(Category::Count), 0.0f);
        EXPECT_FLOAT_EQ(audio.EffectiveVolume(Category::Count), 0.0f);
    }

    TEST(AudioSystemTests, EveryCategoryHasAName)
    {
        // The names go into the audio debug overlay (§69's F7) and into a bug report; a category
        // added without one has to fail here rather than show up as "?" months later.
        for (std::size_t i = 0; i < static_cast<std::size_t>(Category::Count); ++i)
        {
            EXPECT_NE(CategoryName(static_cast<Category>(i)), "?") << "category " << i;
        }
    }

    TEST(AudioSystemTests, TheSummaryLineSaysWhyThereIsNoSound)
    {
        // It goes into the log header and the bug-report footer, so "audio silent" alone would be
        // the least useful thing it could say.
        const AudioSystem off(false);
        const std::string summary = off.Summary();
        EXPECT_NE(summary.find("silent"), std::string::npos) << summary;
        EXPECT_NE(summary.find("--no-audio"), std::string::npos) << summary;

        const AudioSystem waiting(true);
        EXPECT_NE(waiting.Summary().find("gesture"), std::string::npos) << waiting.Summary();
    }
} // namespace
