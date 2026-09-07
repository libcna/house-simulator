// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffectInstance.hpp"
#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Media/Video/Video.hpp"
#include "Microsoft/Xna/Framework/Media/Video/VideoPlayer.hpp"

namespace Microsoft::Xna::Framework
{
    namespace Content
    {
        class ContentManager;
    }

    namespace Graphics
    {
        class GraphicsDevice;
        class SpriteBatch;
    } // namespace Graphics
} // namespace Microsoft::Xna::Framework

namespace cnahouse
{
    namespace audio
    {
        class AudioSystem;
    }

    namespace rendering
    {
        class RenderTier;
    }

    namespace ui
    {
        class TextRenderer;
    }
} // namespace cnahouse

namespace cnahouse::content
{

    /// @brief What one of the six content types did, in words a test and a log can both use.
    ///
    /// **A failure is a recorded fact, not an exception.** The whole value of a smoke scene is that
    /// it tells you which of the six is broken; a scene that threw on the first missing asset would
    /// report the model and say nothing about the other five, which is the least useful moment to
    /// stop looking.
    struct SmokeItem
    {
        /// @brief The CONTENT NAME asked for -- never an OS path (`cna-house.md` §8.3).
        std::string contentName;
        bool loaded = false;
        /// @brief What was actually found: dimensions, duration, mesh count. Empty until loaded.
        std::string detail;
        /// @brief Why it did not load. Empty when it did.
        std::string error;
    };

    /// @brief Everything the smoke scene established, in one readable record.
    struct SmokeReport
    {
        SmokeItem model;
        SmokeItem texture;
        SmokeItem font;
        SmokeItem sound;
        SmokeItem effect;
        SmokeItem video;

        /// @brief The technique the model was drawn with. Names WHICH effect reached the draw.
        std::string techniqueDrawn;
        /// @brief Draw calls the scene issued through the compiled effect.
        int effectDrawCalls = 0;
        /// @brief True once the mixer has reported the instance PLAYING.
        ///
        /// Read back from `SoundEffectInstance::getStateProperty` rather than set beside the
        /// `Play()` call. Setting it beside the call records that the code path ran, which an
        /// injected removal of `Play()` itself satisfies -- measured, `HOUSE-00201`.
        bool soundStarted = false;
        /// @brief What the mixer said the instance was doing, for a failure message that diagnoses.
        std::string soundState = "not attempted";
        /// @brief True once `VideoPlayer::getPlayPositionProperty` has actually moved.
        ///
        /// Separate from "a texture came back": a player that hands back frame 0 forever satisfies
        /// every check that only asks whether `GetTexture` returned something.
        bool videoAdvanced = false;
        /// @brief The furthest play position observed, in seconds.
        double videoPositionSeconds = 0.0;
        /// @brief Frames in which `GetTexture` returned a texture.
        int videoFramesShown = 0;

        /// @brief Tier E only. False means the compiled-effect item is legitimately absent.
        bool tierEAvailable = false;

        [[nodiscard]] std::vector<const SmokeItem*> Items() const;
        /// @brief Every item that had to load for this build, loaded.
        ///
        /// The compiled effect is excluded on a Tier-S build, because ADR-0003 says Tier S is
        /// complete without one -- requiring it there would make the smoke scene contradict the
        /// architecture it is smoke-testing.
        [[nodiscard]] bool AllRequiredLoaded() const;
        [[nodiscard]] std::string ToString() const;
    };

    /// @brief `--scene=content-smoke`: one of each content type, loaded, drawn and played.
    ///
    /// `HOUSE-00201`. The point is not that six `Load<T>` calls return -- the unit tests already
    /// cover that shape -- but that each asset **reaches the thing that consumes it**: the model
    /// reaches a draw call, the texture reaches a sampler, the font reaches a glyph, the sound
    /// reaches the mixer, the effect reaches a shader, and the video's frame advances. Each of
    /// those is a different failure, and each of them has silently shipped in some project.
    ///
    /// XNA-only (ADR-0001): `Model`, `Texture2D`, `SpriteFont`, `SoundEffect`, `Effect`, `Video`,
    /// `VideoPlayer`, `SpriteBatch`, `GraphicsDevice`, `Matrix`. No `CNA::` anything.
    class SmokeScene
    {
    public:
        /// @brief The name `--scene` takes.
        static constexpr const char* kSceneName = "content-smoke";

        /// The six content names, in one place, so a test asserts against the same strings the
        /// scene loads rather than a copy that can drift from them.
        static constexpr const char* kModelName = "Models/Smoke/marker";
        static constexpr const char* kTextureName = "Textures/Smoke/quadrants";
        static constexpr const char* kFontName = "Fonts/ui-22";
        static constexpr const char* kSoundName = "Audio/Smoke/chime";
        static constexpr const char* kEffectName = "Effects/P1Probe";
        static constexpr const char* kVideoName = "Video/smoke_clip";

        /// @brief The technique used when the compiled effect is available.
        static constexpr const char* kTechnique = "Textured";
        /// @brief What the model is drawn with when it is not.
        static constexpr const char* kStockTechnique = "BasicEffect";

        SmokeScene(Microsoft::Xna::Framework::Content::ContentManager& content,
                   Microsoft::Xna::Framework::Content::ContentManager* effects,
                   audio::AudioSystem& audio,
                   bool tierE);
        ~SmokeScene();

        SmokeScene(const SmokeScene&) = delete;
        SmokeScene& operator=(const SmokeScene&) = delete;

        /// @brief Loads all six. Never throws; every failure lands in the report.
        void Load();

        /// @brief Starts the sound and the video, and watches the video's position advance.
        void Update(float deltaSeconds);

        /// @brief The model, through the compiled effect when there is one.
        void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

        /// @brief The video frame, the source texture, and a line of text per content type.
        void DrawOverlay(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch, ui::TextRenderer& text);

        [[nodiscard]] const SmokeReport& Report() const noexcept
        {
            return report_;
        }

        /// @brief Where the video frame is drawn, in the 1600x900 virtual canvas of §67.2.
        ///
        /// Public because the render fixture must exclude exactly this rectangle: the frame the
        /// player has reached depends on wall-clock time, so it is the one part of the picture that
        /// cannot be compared against a committed reference. Excluding the named rectangle keeps
        /// the rest of the frame strict (`ImageCompare.hpp`), and the integration test -- which can
        /// watch several frames -- is what proves the video advanced.
        static constexpr int kVideoPanelX = 1180;
        static constexpr int kVideoPanelY = 120;
        static constexpr int kVideoPanelSize = 256;

        /// @brief Where the source texture is drawn, for comparison beside the model.
        static constexpr int kTexturePanelX = 1180;
        static constexpr int kTexturePanelY = 440;
        static constexpr int kTexturePanelSize = 256;

    private:
        void LoadModel();
        void LoadTexture();
        void LoadFont();
        void LoadSound();
        void LoadEffect();
        void LoadVideo();

        Microsoft::Xna::Framework::Content::ContentManager& content_;
        Microsoft::Xna::Framework::Content::ContentManager* effects_;
        audio::AudioSystem& audio_;
        bool tierE_;

        std::optional<Microsoft::Xna::Framework::Graphics::Model> model_;
        std::optional<Microsoft::Xna::Framework::Graphics::Texture2D> texture_;
        std::optional<Microsoft::Xna::Framework::Graphics::SpriteFont> font_;
        std::optional<Microsoft::Xna::Framework::Audio::SoundEffect> sound_;
        std::optional<Microsoft::Xna::Framework::Audio::SoundEffectInstance> soundInstance_;
        std::shared_ptr<Microsoft::Xna::Framework::Graphics::Effect> effect_;
        std::optional<Microsoft::Xna::Framework::Media::Video> video_;
        std::optional<Microsoft::Xna::Framework::Media::VideoPlayer> player_;

        SmokeReport report_;
        /// Tried once. Without this the retry loop would restart a sound that is genuinely
        /// unplayable on every frame for the life of the session.
        bool soundAttempted_ = false;
        double lastVideoPosition_ = -1.0;
        float elapsed_ = 0.0f;
    };

} // namespace cnahouse::content
