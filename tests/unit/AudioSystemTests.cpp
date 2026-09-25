// SPDX-License-Identifier: MIT
//
// `HOUSE-00154` and the mix half of `HOUSE-00155`. Everything here runs without an audio device and
// without a `Game`, which is the point: the states this class exists to handle are the ones where
// there IS no device, and a test that needed one could not reach them.
#include <gtest/gtest.h>

#include "cnahouse/audio/AudioSystem.hpp"
#include "cnahouse/content/ContentRegistry.hpp"
#include "cnahouse/world/WorldTypes.hpp"

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
        EXPECT_FLOAT_EQ(waiting.EffectiveVolume(Category::Footsteps), 0.0f);

        const AudioSystem off(false);
        EXPECT_FLOAT_EQ(off.EffectiveVolume(Category::Footsteps), 0.0f);
        EXPECT_FLOAT_EQ(off.EffectiveVolume(Category::Weather), 0.0f);
    }

    TEST(AudioSystemTests, TheCategoryDefaultsAreTheCompactM8Mix)
    {
        const AudioSystem audio(true);
        EXPECT_FLOAT_EQ(audio.MasterVolume(), 0.80f);
        EXPECT_FLOAT_EQ(audio.CategoryVolume(Category::Footsteps), 0.85f);
        EXPECT_FLOAT_EQ(audio.CategoryVolume(Category::Ambience), 0.75f);
        EXPECT_FLOAT_EQ(audio.CategoryVolume(Category::Weather), 0.75f);
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

        audio.SetCategoryVolume(Category::Weather, 9.0f);
        EXPECT_FLOAT_EQ(audio.CategoryVolume(Category::Weather), 1.0f);
        audio.SetCategoryVolume(Category::Weather, -9.0f);
        EXPECT_FLOAT_EQ(audio.CategoryVolume(Category::Weather), 0.0f);
    }

    TEST(AudioSystemTests, MutePreservesTheConfiguredMasterAndCanBeChangedLive)
    {
        AudioSystem audio(true);
        audio.SetMasterVolume(0.42F);
        audio.SetMuted(true);
        EXPECT_TRUE(audio.IsMuted());
        EXPECT_FLOAT_EQ(audio.MasterVolume(), 0.42F);

        audio.SetMuted(false);
        EXPECT_FALSE(audio.IsMuted());
        EXPECT_FLOAT_EQ(audio.MasterVolume(), 0.42F);
    }

    TEST(AudioSystemTests, BanksResolveSoundIdsThroughTheManifest)
    {
        cnahouse::content::ContentRegistry registry;
        ASSERT_TRUE(registry.LoadFromJson(
            R"({"schema":"cna-house/assets/1","assets":[
              {"id":"SOUND_STEP_A","contentName":"Audio/step-a","kind":"sound","residencyPack":"audio-core"},
              {"id":"SOUND_STEP_B","contentName":"Audio/step-b","kind":"sound","residencyPack":"audio-core"}
            ]})",
            "assets.manifest.json"));

        cnahouse::world::AudioBank authored;
        authored.id = cnahouse::util::Intern("BANK_STEPS");
        authored.samples = {cnahouse::util::Intern("SOUND_STEP_A"), cnahouse::util::Intern("SOUND_STEP_B")};
        authored.gain = 0.65F;

        AudioSystem audio(false);
        audio.LoadBanks(std::span<const cnahouse::world::AudioBank>(&authored, 1U), registry);

        ASSERT_EQ(audio.BankCount(), 1U);
        EXPECT_TRUE(audio.BankProblems().empty());
        const auto samples = audio.Bank(authored.id);
        ASSERT_EQ(samples.size(), 2U);
        EXPECT_EQ(samples[0], "Audio/step-a");
        EXPECT_EQ(samples[1], "Audio/step-b");
        EXPECT_FLOAT_EQ(audio.BankGain(authored.id), 0.65F);
    }

    TEST(AudioSystemTests, AMissingOrNonSoundBankIsReportedAndSilent)
    {
        cnahouse::content::ContentRegistry registry;
        ASSERT_TRUE(registry.LoadFromJson(
            R"({"schema":"cna-house/assets/1","assets":[
              {"id":"TEXTURE_NOT_SOUND","contentName":"Textures/nope","kind":"texture","residencyPack":"core"}
            ]})",
            "assets.manifest.json"));

        cnahouse::world::AudioBank bad;
        bad.id = cnahouse::util::Intern("BANK_BAD");
        bad.samples = {cnahouse::util::Intern("TEXTURE_NOT_SOUND")};

        AudioSystem audio(false);
        audio.LoadBanks(std::span<const cnahouse::world::AudioBank>(&bad, 1U), registry);

        EXPECT_EQ(audio.BankCount(), 0U);
        ASSERT_EQ(audio.BankProblems().size(), 1U);
        EXPECT_NE(audio.BankProblems()[0].find("not a sound"), std::string::npos);
        EXPECT_TRUE(audio.Bank(bad.id).empty());
        EXPECT_TRUE(audio.Bank(cnahouse::util::Intern("BANK_MISSING")).empty());
        EXPECT_FLOAT_EQ(audio.BankGain(bad.id), 0.0F);
        EXPECT_EQ(audio.State(), AudioState::Silent) << "bad content must not re-open audio";
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
