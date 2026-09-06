// SPDX-License-Identifier: MIT
#include "cnahouse/audio/AudioSystem.hpp"

#include <algorithm>
#include <exception>
#include <format>

#include "Microsoft/Xna/Framework/Audio/NoAudioHardwareException.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"

#include "cnahouse/util/Log.hpp"

namespace cnahouse::audio
{
    using util::Log;
    using util::LogCat;

    std::string_view CategoryName(Category category) noexcept
    {
        switch (category)
        {
            case Category::Ambience:
                return "ambience";
            case Category::World:
                return "world";
            case Category::Animals:
                return "animals";
            case Category::Media:
                return "media";
            case Category::Ui:
                return "ui";
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
        // `cna-house.md` §68's Audio tab defaults, in the enum's order.
        categories_[static_cast<std::size_t>(Category::Ambience)] = 0.75f;
        categories_[static_cast<std::size_t>(Category::World)] = 1.00f;
        categories_[static_cast<std::size_t>(Category::Animals)] = 0.90f;
        categories_[static_cast<std::size_t>(Category::Media)] = 0.70f;
        categories_[static_cast<std::size_t>(Category::Ui)] = 0.60f;

        if (!enabled_)
        {
            // `--no-audio` never touches the device at all, which is the point of it: it is the
            // option someone reaches for when the device is what is broken.
            state_ = AudioState::Silent;
            silentReason_ = "audio was disabled with --no-audio";
        }
    }

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
            const float volume = master_;
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
        if (state_ != AudioState::Ready)
        {
            return;
        }
        try
        {
            const float value = master_; // lvalue: see OpenDevice
            Microsoft::Xna::Framework::Audio::SoundEffect::setMasterVolumeProperty(value);
        }
        catch (const std::exception& e)
        {
            // The device was there and has gone. Recorded, and the game keeps running.
            state_ = AudioState::Silent;
            silentReason_ = e.what();
            Log::Warn(LogCat::Audio, "the audio device was lost: {}", silentReason_);
        }
    }

    void AudioSystem::SetCategoryVolume(Category category, float volume) noexcept
    {
        if (category == Category::Count)
        {
            return;
        }
        categories_[static_cast<std::size_t>(category)] = std::clamp(volume, 0.0f, 1.0f);
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
        if (!IsReady())
        {
            // 0 rather than the mix, so that a caller which forgot to check cannot play into a
            // device that is not there. Silence is the supported behaviour, not a failure.
            return 0.0f;
        }
        return master_ * CategoryVolume(category);
    }

    std::string AudioSystem::Summary() const
    {
        if (state_ == AudioState::Ready)
        {
            return std::format("audio ready, master {:.0f}%", static_cast<double>(master_) * 100.0);
        }
        return std::format(
            "audio {}{}{}", AudioStateName(state_), silentReason_.empty() ? "" : ": ", silentReason_);
    }

} // namespace cnahouse::audio
