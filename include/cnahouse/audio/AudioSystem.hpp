// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <exception>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "cnahouse/audio/AmbienceDirector.hpp"
#include "cnahouse/util/Ids.hpp"

namespace Microsoft::Xna::Framework::Audio
{
    class SoundEffect;
}

namespace cnahouse::content
{
    class ContentRegistry;
}

namespace cnahouse::world
{
    struct AudioBank;
}

namespace cnahouse::audio
{

    /// @brief The three retained mix categories in `plan.md` milestone M8.
    enum class Category : std::uint8_t
    {
        Footsteps,
        Ambience,
        Weather,
        Count,
    };

    [[nodiscard]] std::string_view CategoryName(Category category) noexcept;

    /// @brief What the audio device is doing.
    enum class AudioState : std::uint8_t
    {
        /// @brief The device has not been touched yet. It is not touched until a user gesture.
        Waiting,
        /// @brief The mixer came up. Sound can play.
        Ready,
        /// @brief There is no audio, and the game runs normally without it.
        Silent,
    };

    [[nodiscard]] std::string_view AudioStateName(AudioState state) noexcept;

    /// @brief The audio device, the mix, and the user-gesture gate.
    ///
    /// **Silence is a supported way to run this game, not an error path.** `--no-audio`, a machine
    /// with no sound card, a container with no `/dev/snd` and a browser tab that has never been
    /// clicked all end in `Silent`, and every one of them leaves the game fully playable. That is
    /// `HOUSE-00154`'s acceptance criterion and it is why nothing here throws.
    ///
    /// **The device is not opened until `NoteUserGesture()`.** Browsers refuse to start audio before
    /// the user has interacted with the page, and a gate that existed only on Web would be a path
    /// nobody exercised until the Web build — so the gate is uniform on every platform and the title
    /// screen's "press any key" is what opens the device everywhere (`HOUSE-00155`).
    ///
    /// **The voice ceiling is ours to impose.** MEASURED (`HOUSE-00097`): 512 of 512 looping
    /// instances reported `Playing` and **CNA refused nothing**. There is no hardware limit
    /// discoverable through the XNA surface, so a budget is a design decision this project enforces
    /// itself; the counter here is where that starts.
    class AudioSystem
    {
    public:
        /// @brief M8's compact mix: master 80 %, footsteps 85 %, ambience/weather 75 %.
        explicit AudioSystem(bool enabled) noexcept;
        ~AudioSystem();

        AudioSystem(const AudioSystem&) = delete;
        AudioSystem& operator=(const AudioSystem&) = delete;

        /// @brief Records that the user has interacted, and opens the device if it can be opened.
        ///
        /// Idempotent: the second call does nothing, so it is safe to wire to every key press.
        /// @return true if this call was the one that changed the state.
        bool NoteUserGesture();

        [[nodiscard]] AudioState State() const noexcept
        {
            return state_;
        }

        /// @brief The device accepted playback; this is not proof of audible speaker output.
        [[nodiscard]] bool IsReady() const noexcept
        {
            return state_ == AudioState::Ready;
        }

        /// @brief True while the game is waiting for the gesture that opens the device.
        [[nodiscard]] bool IsWaitingForGesture() const noexcept
        {
            return state_ == AudioState::Waiting && enabled_;
        }

        /// @brief Why there is no sound, or empty when there is. Goes into the bug-report header.
        [[nodiscard]] const std::string& SilentReason() const noexcept
        {
            return silentReason_;
        }

        void SetMasterVolume(float volume) noexcept;

        [[nodiscard]] float MasterVolume() const noexcept
        {
            return master_;
        }

        /// @brief Mutes the global mix without destroying the configured master volume.
        void SetMuted(bool muted) noexcept;

        [[nodiscard]] bool IsMuted() const noexcept
        {
            return muted_;
        }

        void SetCategoryVolume(Category category, float volume) noexcept;
        [[nodiscard]] float CategoryVolume(Category category) const noexcept;

        /// @brief What a sound in @p category should actually be played at: master × category, and
        ///        **0 whenever audio is not ready**, so a caller cannot accidentally play into a
        ///        device that is not there.
        [[nodiscard]] float EffectiveVolume(Category category) const noexcept;

        /// @brief Plays one cached fire-and-forget sound through the compact mix.
        ///
        /// The XNA master is already applied globally, so @p gain is multiplied by the category
        /// only. A missing/lost device remains a supported silent state.
        [[nodiscard]] bool PlayOneShot(Microsoft::Xna::Framework::Audio::SoundEffect* sound,
                                       Category category,
                                       float gain,
                                       float pitch = 0.0F,
                                       float pan = 0.0F) noexcept;

        /// @brief Accepted one-shot requests, not a claim that a speaker was heard.
        [[nodiscard]] std::uint64_t OneShotsPlayed() const noexcept
        {
            return oneShotsPlayed_;
        }

        /// @brief Resolves authored bank sample ids through the asset manifest.
        ///
        /// A bad bank is omitted, reported and therefore silent. Other banks remain available.
        void LoadBanks(std::span<const world::AudioBank> banks, const content::ContentRegistry& registry);

        /// @brief Resolved content names for a bank, or an empty span for a missing/silent bank.
        [[nodiscard]] std::span<const std::string> Bank(util::Id id) const noexcept;

        [[nodiscard]] float BankGain(util::Id id) const noexcept;

        struct AmbienceBankSounds
        {
            std::span<Microsoft::Xna::Framework::Audio::SoundEffect* const> sounds;
            float gain = 0.0F;
        };

        /// Fixed interior/day/night loops, using cached standard-XNA SoundEffects.
        void StartAmbience(AmbienceBankSounds interior,
                           AmbienceBankSounds exteriorDay,
                           AmbienceBankSounds exteriorNight) noexcept;
        void UpdateAmbience(const AmbienceMix& mix) noexcept;
        void StopAmbience() noexcept;

        [[nodiscard]] bool AmbienceStarted() const noexcept
        {
            return ambience_ != nullptr;
        }

        [[nodiscard]] std::size_t AmbienceVoiceCount() const noexcept;

        struct WeatherBankSound
        {
            Microsoft::Xna::Framework::Audio::SoundEffect* sound = nullptr;
            std::string contentName;
            float gain = 0.0F;
        };

        void StartWeather(const std::array<WeatherBankSound, 4>& banks,
                          std::string_view contentRoot) noexcept;
        void UpdateWeather(const WeatherMix& mix) noexcept;

        [[nodiscard]] bool WeatherAttempted() const noexcept
        {
            return weatherAttempted_;
        }

        [[nodiscard]] std::size_t WeatherVoiceCount() const noexcept;

        [[nodiscard]] const std::string& WeatherProblem() const noexcept
        {
            return weatherProblem_;
        }

        [[nodiscard]] WeatherMix CurrentWeatherMix() const noexcept;

        [[nodiscard]] std::size_t BankCount() const noexcept
        {
            return banks_.size();
        }

        [[nodiscard]] const std::vector<std::string>& BankProblems() const noexcept
        {
            return bankProblems_;
        }

        /// @brief One line for the log header and the bug-report footer.
        [[nodiscard]] std::string Summary() const;

    private:
        /// @brief Opens the mixer, or records why it could not be opened. Called once, on gesture.
        void OpenDevice();
        void ApplyDeviceVolume() noexcept;
        void RecordDeviceLoss(const std::exception& error) noexcept;

        struct AmbienceVoices;
        struct WeatherVoices;

        struct ResolvedBank
        {
            float gain = 1.0F;
            std::vector<std::string> samples;
        };

        static constexpr std::size_t kCategoryCount = static_cast<std::size_t>(Category::Count);

        bool enabled_;
        AudioState state_ = AudioState::Waiting;
        std::string silentReason_;
        float master_ = 0.80f;
        bool muted_ = false;
        std::uint64_t oneShotsPlayed_ = 0U;
        std::array<float, kCategoryCount> categories_{};
        std::unordered_map<util::Id, ResolvedBank> banks_;
        std::vector<std::string> bankProblems_;
        mutable std::unordered_set<util::Id> missingBanksReported_;
        std::unique_ptr<AmbienceVoices> ambience_;
        std::unique_ptr<WeatherVoices> weather_;
        bool weatherAttempted_ = false;
        std::string weatherProblem_;
    };

} // namespace cnahouse::audio
