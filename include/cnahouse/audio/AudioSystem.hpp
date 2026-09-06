// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace cnahouse::audio
{

    /// @brief The mix categories of `cna-house.md` §68's Audio tab.
    enum class Category : std::uint8_t
    {
        Ambience,
        World,
        Animals,
        Media,
        Ui,
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
        /// @brief §68's defaults: master 80 %, ambience 75 %, world 100 %, animals 90 %, media 70 %,
        ///        UI 60 %.
        explicit AudioSystem(bool enabled) noexcept;

        /// @brief Records that the user has interacted, and opens the device if it can be opened.
        ///
        /// Idempotent: the second call does nothing, so it is safe to wire to every key press.
        /// @return true if this call was the one that changed the state.
        bool NoteUserGesture();

        [[nodiscard]] AudioState State() const noexcept
        {
            return state_;
        }

        /// @brief True only when sound will actually be heard.
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

        void SetCategoryVolume(Category category, float volume) noexcept;
        [[nodiscard]] float CategoryVolume(Category category) const noexcept;

        /// @brief What a sound in @p category should actually be played at: master × category, and
        ///        **0 whenever audio is not ready**, so a caller cannot accidentally play into a
        ///        device that is not there.
        [[nodiscard]] float EffectiveVolume(Category category) const noexcept;

        /// @brief One line for the log header and the bug-report footer.
        [[nodiscard]] std::string Summary() const;

    private:
        /// @brief Opens the mixer, or records why it could not be opened. Called once, on gesture.
        void OpenDevice();

        static constexpr std::size_t kCategoryCount = static_cast<std::size_t>(Category::Count);

        bool enabled_;
        AudioState state_ = AudioState::Waiting;
        std::string silentReason_;
        float master_ = 0.80f;
        std::array<float, kCategoryCount> categories_{};
    };

} // namespace cnahouse::audio
