// SPDX-License-Identifier: MIT
#include "cnahouse/audio/AudioSystem.hpp"

#include <algorithm>
#include <exception>
#include <format>
#include <stdexcept>
#include <unordered_set>

#include "Microsoft/Xna/Framework/Audio/NoAudioHardwareException.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffectInstance.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundState.hpp"

#include "cnahouse/content/ContentRegistry.hpp"
#include "cnahouse/util/Log.hpp"
#include "cnahouse/world/WorldTypes.hpp"

namespace cnahouse::audio
{
    using util::Log;
    using util::LogCat;

    struct AudioSystem::AmbienceVoices
    {
        enum class Bed : std::uint8_t
        {
            Interior,
            ExteriorDay,
            ExteriorNight,
        };

        struct Voice
        {
            Bed bed = Bed::Interior;
            float gain = 0.0F;
            std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffectInstance> instance;
        };

        AmbienceMix mix;
        std::vector<Voice> voices;
    };

    std::string_view CategoryName(Category category) noexcept
    {
        switch (category)
        {
            case Category::Footsteps:
                return "footsteps";
            case Category::Ambience:
                return "ambience";
            case Category::Weather:
                return "weather";
            case Category::Count:
                break;
        }
        return "?";
    }

    std::string_view AudioStateName(AudioState state) noexcept
    {
        switch (state)
        {
            case AudioState::Waiting:
                return "waiting for a user gesture";
            case AudioState::Ready:
                return "ready";
            case AudioState::Silent:
                return "silent";
        }
        return "?";
    }

    AudioSystem::AudioSystem(bool enabled) noexcept
        : enabled_(enabled)
    {
        // The compact M8 mix defaults, in enum order.
        categories_[static_cast<std::size_t>(Category::Footsteps)] = 0.85f;
        categories_[static_cast<std::size_t>(Category::Ambience)] = 0.75f;
        categories_[static_cast<std::size_t>(Category::Weather)] = 0.75f;

        if (!enabled_)
        {
            // `--no-audio` never touches the device at all, which is the point of it: it is the
            // option someone reaches for when the device is what is broken.
            state_ = AudioState::Silent;
            silentReason_ = "audio was disabled with --no-audio";
        }
    }

    AudioSystem::~AudioSystem() = default;

    bool AudioSystem::NoteUserGesture()
    {
        if (state_ != AudioState::Waiting)
        {
            return false; // already open, or deliberately silent; safe to call from every key press
        }
        OpenDevice();
        return true;
    }

    void AudioSystem::OpenDevice()
    {
        try
        {
            // MEASURED: `SoundEffect::setMasterVolumeProperty` is one of the five entry points that
            // force CNA's mixer up (`modules/audio/src/Xna/SoundEffect.cpp`), converting the
            // internal failure into `NoAudioHardwareException` exactly as FNA's `SoundEffect.Device()`
            // does. So setting the master volume IS the device probe, in plain XNA, with nothing
            // CNA-specific asked of it.
            //
            // The LVALUE matters. `setMasterVolumeProperty` has two overloads and the `float&&` one
            // is marked `CNAEXT`: passing a literal or a temporary would bind to it and violate
            // ADR-0001. `check_xna_only.py` cannot see this -- it is an overload-resolution
            // outcome, not a name -- so it is written out here and in `docs/conventions.md` §5a.
            const float volume = muted_ ? 0.0F : master_;
            Microsoft::Xna::Framework::Audio::SoundEffect::setMasterVolumeProperty(volume);
            state_ = AudioState::Ready;
            silentReason_.clear();
            Log::Info(LogCat::Audio,
                      "audio device opened; master volume {:.0f}%",
                      static_cast<double>(master_) * 100.0);
        }
        catch (const Microsoft::Xna::Framework::Audio::NoAudioHardwareException& e)
        {
            state_ = AudioState::Silent;
            silentReason_ = e.what();
            // Info, not Error: a machine with no sound card is a supported configuration, and an
            // error line would send someone looking for a fault that is not there.
            Log::Info(LogCat::Audio, "no audio hardware; the game runs silently: {}", silentReason_);
        }
        catch (const std::exception& e)
        {
            // Anything else IS unexpected, so it is a warning -- but it still ends in silence
            // rather than in a failed start, because `HOUSE-00154`'s acceptance is that a missing
            // device leaves the game fully playable and does not distinguish how it went missing.
            state_ = AudioState::Silent;
            silentReason_ = e.what();
            Log::Warn(LogCat::Audio, "the audio device could not be opened: {}", silentReason_);
        }
    }

    void AudioSystem::SetMasterVolume(float volume) noexcept
    {
        master_ = std::clamp(volume, 0.0f, 1.0f);
        ApplyDeviceVolume();
    }

    void AudioSystem::SetMuted(bool muted) noexcept
    {
        muted_ = muted;
        ApplyDeviceVolume();
    }

    void AudioSystem::ApplyDeviceVolume() noexcept
    {
        if (state_ != AudioState::Ready)
        {
            return;
        }
        try
        {
            const float value = muted_ ? 0.0F : master_; // lvalue: see OpenDevice
            Microsoft::Xna::Framework::Audio::SoundEffect::setMasterVolumeProperty(value);
        }
        catch (const std::exception& e)
        {
            // The device was there and has gone. Recorded, and the game keeps running.
            RecordDeviceLoss(e);
        }
    }

    void AudioSystem::SetCategoryVolume(Category category, float volume) noexcept
    {
        if (category == Category::Count)
        {
            return;
        }
        categories_[static_cast<std::size_t>(category)] = std::clamp(volume, 0.0f, 1.0f);
        if (category == Category::Ambience && ambience_ != nullptr)
        {
            UpdateAmbience(ambience_->mix);
        }
    }

    float AudioSystem::CategoryVolume(Category category) const noexcept
    {
        if (category == Category::Count)
        {
            return 0.0f;
        }
        return categories_[static_cast<std::size_t>(category)];
    }

    float AudioSystem::EffectiveVolume(Category category) const noexcept
    {
        if (!IsReady() || muted_)
        {
            // 0 rather than the mix, so that a caller which forgot to check cannot play into a
            // device that is not there. Silence is the supported behaviour, not a failure.
            return 0.0f;
        }
        return master_ * CategoryVolume(category);
    }

    bool AudioSystem::PlayOneShot(Microsoft::Xna::Framework::Audio::SoundEffect* sound,
                                  Category category,
                                  float gain,
                                  float pitch,
                                  float pan) noexcept
    {
        if (!IsReady() || muted_ || sound == nullptr || category == Category::Count)
        {
            return false;
        }
        try
        {
            const float volume = std::clamp(gain, 0.0F, 1.0F) * CategoryVolume(category);
            const bool played =
                sound->Play(volume, std::clamp(pitch, -1.0F, 1.0F), std::clamp(pan, -1.0F, 1.0F));
            if (played)
            {
                ++oneShotsPlayed_;
            }
            return played;
        }
        catch (const std::exception& error)
        {
            RecordDeviceLoss(error);
            return false;
        }
    }

    void AudioSystem::RecordDeviceLoss(const std::exception& error) noexcept
    {
        ambience_.reset();
        state_ = AudioState::Silent;
        silentReason_ = error.what();
        Log::Warn(LogCat::Audio, "the audio device was lost: {}", silentReason_);
    }

    void AudioSystem::LoadBanks(std::span<const world::AudioBank> banks,
                                const content::ContentRegistry& registry)
    {
        ambience_.reset();
        banks_.clear();
        bankProblems_.clear();
        missingBanksReported_.clear();

        for (const world::AudioBank& authored : banks)
        {
            ResolvedBank resolved;
            resolved.gain = authored.gain;
            bool valid = true;
            std::unordered_set<util::Id> seen;
            for (const util::Id sample : authored.samples)
            {
                if (!seen.insert(sample).second)
                {
                    const std::string problem = std::format("audio bank '{}' repeats sample '{}'",
                                                            util::IdRegistry::NameOf(authored.id),
                                                            util::IdRegistry::NameOf(sample));
                    bankProblems_.push_back(problem);
                    Log::Warn(LogCat::Audio, "{}; the bank is silent", problem);
                    valid = false;
                    break;
                }

                const content::AssetEntry* asset = registry.Find(sample);
                if (asset == nullptr || asset->kind != content::AssetKind::Sound)
                {
                    const std::string_view sampleName = util::IdRegistry::NameOf(sample);
                    const std::string problem = std::format(
                        "audio bank '{}' names {}, which is not a sound in assets.manifest.json",
                        util::IdRegistry::NameOf(authored.id),
                        sampleName.empty() ? "an unknown asset" : std::format("'{}'", sampleName));
                    bankProblems_.push_back(problem);
                    Log::Warn(LogCat::Audio, "{}; the bank is silent", problem);
                    valid = false;
                    break;
                }
                resolved.samples.push_back(asset->contentName);
            }

            if (!valid || resolved.samples.empty())
            {
                if (valid)
                {
                    const std::string problem =
                        std::format("audio bank '{}' has no samples", util::IdRegistry::NameOf(authored.id));
                    bankProblems_.push_back(problem);
                    Log::Warn(LogCat::Audio, "{}; the bank is silent", problem);
                }
                continue;
            }
            if (!banks_.emplace(authored.id, std::move(resolved)).second)
            {
                const std::string problem =
                    std::format("audio bank '{}' appears twice", util::IdRegistry::NameOf(authored.id));
                bankProblems_.push_back(problem);
                Log::Warn(LogCat::Audio, "{}; the duplicate is ignored", problem);
            }
        }

        Log::Info(LogCat::Audio,
                  "resolved {} audio bank(s); {} bank problem(s)",
                  banks_.size(),
                  bankProblems_.size());
    }

    std::span<const std::string> AudioSystem::Bank(util::Id id) const noexcept
    {
        const auto found = banks_.find(id);
        if (found != banks_.end())
        {
            return found->second.samples;
        }
        if (missingBanksReported_.insert(id).second)
        {
            Log::Warn(LogCat::Audio,
                      "audio bank '{}' is missing; playback remains silent",
                      util::IdRegistry::NameOf(id));
        }
        return {};
    }

    float AudioSystem::BankGain(util::Id id) const noexcept
    {
        const auto found = banks_.find(id);
        return found == banks_.end() ? 0.0F : found->second.gain;
    }

    void AudioSystem::StartAmbience(AmbienceBankSounds interior,
                                    AmbienceBankSounds exteriorDay,
                                    AmbienceBankSounds exteriorNight) noexcept
    {
        if (!IsReady() || ambience_ != nullptr)
        {
            return;
        }

        try
        {
            auto voices = std::make_unique<AmbienceVoices>();
            const auto add = [&voices](AmbienceVoices::Bed bed, AmbienceBankSounds bank)
            {
                if (bank.sounds.empty())
                {
                    return;
                }
                const float perVoiceGain = bank.gain / static_cast<float>(bank.sounds.size());
                for (Microsoft::Xna::Framework::Audio::SoundEffect* sound : bank.sounds)
                {
                    if (sound == nullptr)
                    {
                        continue;
                    }
                    auto instance = std::make_unique<Microsoft::Xna::Framework::Audio::SoundEffectInstance>(
                        sound->CreateInstance());
                    const bool looped = true; // lvalue selects the XNA overload; see ADR-0001
                    instance->setIsLoopedProperty(looped);
                    const float silent = 0.0F; // same overload rule for Volume
                    instance->setVolumeProperty(silent);
                    instance->Play();
                    if (instance->getStateProperty() != Microsoft::Xna::Framework::Audio::SoundState::Playing)
                    {
                        throw std::runtime_error("an ambience loop did not enter the playing state");
                    }
                    voices->voices.push_back(AmbienceVoices::Voice{bed, perVoiceGain, std::move(instance)});
                }
            };

            add(AmbienceVoices::Bed::Interior, interior);
            add(AmbienceVoices::Bed::ExteriorDay, exteriorDay);
            add(AmbienceVoices::Bed::ExteriorNight, exteriorNight);
            ambience_ = std::move(voices);
            Log::Info(LogCat::Audio, "started {} retained ambience loop voice(s)", ambience_->voices.size());
        }
        catch (const std::exception& error)
        {
            RecordDeviceLoss(error);
        }
    }

    void AudioSystem::UpdateAmbience(const AmbienceMix& mix) noexcept
    {
        if (ambience_ == nullptr || !IsReady())
        {
            return;
        }
        ambience_->mix = mix;
        try
        {
            const float category = CategoryVolume(Category::Ambience);
            for (AmbienceVoices::Voice& voice : ambience_->voices)
            {
                float bed = 0.0F;
                switch (voice.bed)
                {
                    case AmbienceVoices::Bed::Interior:
                        bed = mix.interior;
                        break;
                    case AmbienceVoices::Bed::ExteriorDay:
                        bed = mix.exteriorDay;
                        break;
                    case AmbienceVoices::Bed::ExteriorNight:
                        bed = mix.exteriorNight;
                        break;
                }
                const float volume = std::clamp(category * voice.gain * bed, 0.0F, 1.0F);
                voice.instance->setVolumeProperty(volume);
            }
        }
        catch (const std::exception& error)
        {
            RecordDeviceLoss(error);
        }
    }

    std::size_t AudioSystem::AmbienceVoiceCount() const noexcept
    {
        return ambience_ == nullptr ? 0U : ambience_->voices.size();
    }

    void AudioSystem::StopAmbience() noexcept
    {
        ambience_.reset();
    }

    std::string AudioSystem::Summary() const
    {
        if (state_ == AudioState::Ready)
        {
            return std::format("audio ready, master {:.0f}%{}",
                               static_cast<double>(master_) * 100.0,
                               muted_ ? ", muted" : "");
        }
        return std::format(
            "audio {}{}{}", AudioStateName(state_), silentReason_.empty() ? "" : ": ", silentReason_);
    }

} // namespace cnahouse::audio
