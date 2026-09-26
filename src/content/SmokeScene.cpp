// SPDX-License-Identifier: MIT
//
// `HOUSE-00201`. The content smoke scene: one model, one texture, one vendored Noto face, one
// sound, one compiled effect and one video, each carried all the way to the thing that consumes it.
//
// The six loads are six separate `try` blocks and not one, deliberately. A scene that stopped at
// the first missing asset would report the model and say nothing about the other five -- and the
// question this scene exists to answer is *which* of the six is broken.
#include "cnahouse/content/SmokeScene.hpp"

#include <algorithm>
#include <format>

#include "Microsoft/Xna/Framework/Audio/SoundState.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectParameter.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectParameterCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechniqueCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMesh.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPart.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelMeshPartCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "Microsoft/Xna/Framework/Vector4.hpp"

#include "cnahouse/audio/AudioSystem.hpp"
#include "cnahouse/ui/TextRenderer.hpp"
#include "cnahouse/util/Log.hpp"

namespace cnahouse::content
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;
        using util::Log;
        using util::LogCat;

        /// The camera. Fixed, and fixed on purpose: a smoke frame that moved could not be compared
        /// against a committed reference, and there is nothing here to look around at.
        constexpr float kFieldOfViewDegrees = 40.0f;
        const Xna::Vector3 kEye(1.35f, 0.95f, 1.75f);
        const Xna::Vector3 kTarget(0.0f, 0.36f, 0.0f);

        /// The tint the compiled effect multiplies the texture by. NOT white: white would make a
        /// frame drawn through the effect and a frame drawn without it look identical, which is
        /// exactly the mistake this scene is supposed to catch.
        const Xna::Vector4 kTint(1.0f, 0.86f, 0.62f, 1.0f);

        std::string Describe(const std::exception& error)
        {
            const std::string text = error.what();
            // One line, and bounded: a `ContentLoadException` can carry a paragraph, and the report
            // is read in a log and in a test failure message.
            const std::size_t newline = text.find('\n');
            std::string first = newline == std::string::npos ? text : text.substr(0, newline);
            if (first.size() > 200)
            {
                first = first.substr(0, 197) + "...";
            }
            return first;
        }

    } // namespace

    std::vector<const SmokeItem*> SmokeReport::Items() const
    {
        return {&model, &texture, &font, &sound, &effect, &video};
    }

    bool SmokeReport::AllRequiredLoaded() const
    {
        for (const SmokeItem* item : Items())
        {
            if (item == &effect && !tierEAvailable)
            {
                continue;
            }
            if (!item->loaded)
            {
                return false;
            }
        }
        return true;
    }

    std::string SmokeReport::ToString() const
    {
        static constexpr const char* kLabels[] = {"model", "texture", "font", "sound", "effect", "video"};
        std::string out;
        const auto items = Items();
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            const SmokeItem& item = *items[index];
            out += std::format("{:<8} {:<26} {}\n",
                               kLabels[index],
                               item.contentName,
                               item.loaded ? item.detail : ("NOT LOADED: " + item.error));
        }
        out += std::format("drawn with {} ({} draw call(s)); sound {}; video {} at {:.2f} s, "
                           "{} frame(s) shown\n",
                           techniqueDrawn.empty() ? "nothing" : techniqueDrawn,
                           effectDrawCalls,
                           soundStarted ? "playing" : ("NOT playing (" + soundState + ")"),
                           videoAdvanced ? "advanced" : "did NOT advance",
                           videoPositionSeconds,
                           videoFramesShown);
        return out;
    }

    SmokeScene::SmokeScene(Xna::Content::ContentManager& content,
                           Xna::Content::ContentManager* effects,
                           audio::AudioSystem& audio,
                           bool tierE)
        : content_(content)
        , effects_(effects)
        , audio_(audio)
    {
        report_.model.contentName = kModelName;
        report_.texture.contentName = kTextureName;
        report_.font.contentName = kFontName;
        report_.sound.contentName = kSoundName;
        report_.effect.contentName = kEffectName;
        report_.video.contentName = kVideoName;
        report_.tierEAvailable = tierE && effects != nullptr;
    }

    SmokeScene::~SmokeScene()
    {
        // The player is stopped before the video it points at is destroyed. A decoder still
        // draining from a freed `Video` is a shutdown crash that reproduces once in twenty runs.
        if (player_.has_value())
        {
            try
            {
                player_->Stop();
            }
            catch (const std::exception& error)
            {
                Log::Warn(LogCat::Content, "the smoke video did not stop cleanly: {}", error.what());
            }
        }
    }

    void SmokeScene::Load()
    {
        LoadModel();
        LoadTexture();
        LoadFont();
        LoadSound();
        LoadEffect();
        LoadVideo();
        Log::Info(LogCat::Content, "content smoke scene:\n{}", report_.ToString());
    }

    void SmokeScene::LoadModel()
    {
        try
        {
            model_.emplace(content_.Load<Xna::Graphics::Model>(kModelName));
            // Indexed, not range-for. `ModelMeshCollection::begin` is `CNAEXT`-marked, so a
            // range-based loop over it is a forbidden call that `check_xna_only.py` would not see
            // -- it is reached through iteration, not through a named symbol (ADR-0001, §1).
            const auto& meshes = model_->getMeshesProperty();
            int parts = 0;
            for (int index = 0; index < meshes.getCountProperty(); ++index)
            {
                parts += meshes[index]->getMeshPartsProperty().getCountProperty();
            }
            report_.model.loaded = true;
            report_.model.detail = std::format("{} mesh(es), {} part(s), {} bone(s)",
                                               meshes.getCountProperty(),
                                               parts,
                                               model_->getBonesProperty().getCountProperty());
        }
        catch (const std::exception& error)
        {
            report_.model.error = Describe(error);
        }
    }

    void SmokeScene::LoadTexture()
    {
        try
        {
            texture_.emplace(content_.Load<Xna::Graphics::Texture2D>(kTextureName));
            report_.texture.loaded = true;
            report_.texture.detail = std::format("{}x{}, {} level(s)",
                                                 texture_->getWidthProperty(),
                                                 texture_->getHeightProperty(),
                                                 texture_->getLevelCountProperty());
        }
        catch (const std::exception& error)
        {
            report_.texture.error = Describe(error);
        }
    }

    void SmokeScene::LoadFont()
    {
        try
        {
            font_.emplace(content_.Load<Xna::Graphics::SpriteFont>(kFontName));
            report_.font.loaded = true;
            report_.font.detail = std::format("line spacing {}, {} glyph(s)",
                                              font_->getLineSpacingProperty(),
                                              font_->getCharactersProperty().size());
        }
        catch (const std::exception& error)
        {
            report_.font.error = Describe(error);
        }
    }

    void SmokeScene::LoadSound()
    {
        try
        {
            sound_.emplace(content_.Load<Xna::Audio::SoundEffect>(kSoundName));
            report_.sound.loaded = true;
            report_.sound.detail =
                std::format("{:.3f} s", sound_->getDurationProperty().getTotalSecondsProperty());
        }
        catch (const std::exception& error)
        {
            report_.sound.error = Describe(error);
        }
    }

    void SmokeScene::LoadEffect()
    {
        if (!report_.tierEAvailable)
        {
            // Not a failure. ADR-0003 makes Tier S complete, and a Tier-S build has no compiled
            // effect tree to load from -- saying so is different from saying the load failed.
            report_.effect.error = "Tier S: this session has no compiled effect set";
            return;
        }
        try
        {
            effect_ = effects_->Load<std::shared_ptr<Xna::Graphics::Effect>>(kEffectName);
            if (effect_ == nullptr)
            {
                report_.effect.error = "the effect loaded as null";
                return;
            }
            report_.effect.loaded = true;
            report_.effect.detail = std::format("{} technique(s), {} parameter(s)",
                                                effect_->getTechniquesProperty().getCountProperty(),
                                                effect_->getParametersProperty().getCountProperty());
        }
        catch (const std::exception& error)
        {
            report_.effect.error = Describe(error);
        }
    }

    void SmokeScene::LoadVideo()
    {
        try
        {
            video_.emplace(content_.Load<Xna::Media::Video>(kVideoName));
            player_.emplace();
            report_.video.loaded = true;
            report_.video.detail = std::format("{}x{}, {:.2f} fps, {:.2f} s",
                                               static_cast<int>(video_->getWidthProperty()),
                                               static_cast<int>(video_->getHeightProperty()),
                                               static_cast<double>(video_->getFramesPerSecondProperty()),
                                               video_->getDurationProperty().getTotalSecondsProperty());
        }
        catch (const std::exception& error)
        {
            report_.video.error = Describe(error);
        }
    }

    void SmokeScene::Update(float deltaSeconds)
    {
        elapsed_ += deltaSeconds;

        // The sound is started ONCE, and only after the audio device has actually opened.
        // `HOUSE-00155`: the device is silent until a user gesture, and calling `Play` before that
        // is a sound that never happens rather than a sound that is queued.
        if (sound_.has_value() && !soundAttempted_ && audio_.IsReady())
        {
            try
            {
                // An INSTANCE rather than the fire-and-forget `Play()`, because the instance is
                // what a test can ask about afterwards. A `Play()` that returned true says the call
                // was accepted, not that anything reached the mixer.
                soundInstance_.emplace(sound_->CreateInstance());
                // An LVALUE, deliberately. `setVolumeProperty` has a `float&&` overload that is
                // `CNAEXT`-marked, and a prvalue argument selects it -- a forbidden call in which
                // no forbidden identifier appears (ADR-0001, `check_xna_strict.py`). This exact
                // line was caught by that gate rather than by review.
                // The retained M8 mix has only footsteps, ambience and weather. The smoke-scene
                // chime is diagnostic content, so it follows the general ambience level.
                float volume = audio_.EffectiveVolume(audio::Category::Ambience);
                soundInstance_->setVolumeProperty(volume);
                soundAttempted_ = true;
                soundInstance_->Play();
                // Read BACK, not assumed. `Play()` returns void, so the only evidence that the
                // sound reached the mixer is what the mixer then says it is doing.
                const auto state = soundInstance_->getStateProperty();
                report_.soundStarted = state == Xna::Audio::SoundState::Playing;
                report_.soundState = state == Xna::Audio::SoundState::Playing  ? "playing"
                                     : state == Xna::Audio::SoundState::Paused ? "paused"
                                                                               : "stopped";
            }
            catch (const std::exception& error)
            {
                soundAttempted_ = true;
                report_.sound.error = Describe(error);
                report_.soundState = "threw";
            }
        }

        if (!player_.has_value() || !video_.has_value())
        {
            return;
        }

        try
        {
            if (player_->getStateProperty() == Xna::Media::MediaState::Stopped)
            {
                const bool looped = true;
                player_->setIsLoopedProperty(looped);
                // Muted: the scene already plays a sound of its own, and two 440 Hz tones at once
                // prove less than one does.
                const bool muted = true;
                player_->setIsMutedProperty(muted);
                player_->Play(&*video_);
            }

            const double position = player_->getPlayPositionProperty().getTotalSecondsProperty();
            if (lastVideoPosition_ >= 0.0 && position > lastVideoPosition_ + 1e-6)
            {
                report_.videoAdvanced = true;
            }
            lastVideoPosition_ = position;
            report_.videoPositionSeconds = std::max(report_.videoPositionSeconds, position);
        }
        catch (const std::exception& error)
        {
            // `HOUSE-00099`/BL-05: on a build without the FFmpeg backend `Play` throws and says so.
            // That is a platform fact, not a broken asset, and the report records it as such.
            report_.video.error = Describe(error);
            player_.reset();
        }
    }

    void SmokeScene::Draw(Xna::Graphics::GraphicsDevice& device)
    {
        if (!model_.has_value())
        {
            return;
        }

        const auto& viewport = device.getViewportProperty();
        const float aspect = viewport.getHeightProperty() > 0
                                 ? static_cast<float>(viewport.getWidthProperty()) /
                                       static_cast<float>(viewport.getHeightProperty())
                                 : 1.0f;
        const Xna::Matrix world = Xna::Matrix::getIdentityProperty();
        const Xna::Matrix view = Xna::Matrix::CreateLookAt(kEye, kTarget, Xna::Vector3::Up);
        const Xna::Matrix projection = Xna::Matrix::CreatePerspectiveFieldOfView(
            Xna::MathHelper::ToRadians(kFieldOfViewDegrees), aspect, 0.1f, 20.0f);

        if (effect_ == nullptr || !texture_.has_value())
        {
            // Tier S. `Model::Draw` uses the effects the processor gave the parts, which is a
            // `BasicEffect` -- still an XNA `Effect`, still a real draw, and the report says which
            // one drew so the two cases are never confused for each other.
            report_.techniqueDrawn = kStockTechnique;
            model_->Draw(world, view, projection);
            return;
        }

        auto& parameters = effect_->getParametersProperty();
        Xna::Graphics::EffectParameter* worldViewProjection = parameters["WorldViewProj"];
        Xna::Graphics::EffectParameter* tint = parameters["TintColor"];
        Xna::Graphics::EffectParameter* base = parameters["BaseTexture"];
        Xna::Graphics::EffectTechnique* technique = effect_->getTechniquesProperty()[kTechnique];
        if (worldViewProjection == nullptr || tint == nullptr || base == nullptr || technique == nullptr)
        {
            report_.effect.error = "the compiled effect is missing a parameter or the technique";
            report_.techniqueDrawn = kStockTechnique;
            model_->Draw(world, view, projection);
            return;
        }

        effect_->setCurrentTechniqueProperty(technique);
        worldViewProjection->SetValue(world * view * projection);
        tint->SetValue(kTint);
        base->SetValue(&*texture_);
        report_.techniqueDrawn = kTechnique;

        // The parts are drawn by hand rather than by handing the model a different effect. Both are
        // XNA, and this one does not mutate the loaded asset: a `ModelMeshPart` whose effect had
        // been swapped would stay swapped for every other user of the same cached `Model`.
        const auto& meshes = model_->getMeshesProperty();
        for (int meshIndex = 0; meshIndex < meshes.getCountProperty(); ++meshIndex)
        {
            const Xna::Graphics::ModelMesh* mesh = meshes[meshIndex];
            if (mesh == nullptr)
            {
                continue;
            }
            const auto& parts = mesh->getMeshPartsProperty();
            for (int partIndex = 0; partIndex < parts.getCountProperty(); ++partIndex)
            {
                const Xna::Graphics::ModelMeshPart* part = parts[partIndex];
                if (part == nullptr || part->getVertexBufferProperty() == nullptr ||
                    part->getIndexBufferProperty() == nullptr || part->getPrimitiveCountProperty() <= 0)
                {
                    continue;
                }
                device.SetVertexBuffer(part->getVertexBufferProperty());
                device.setIndicesProperty(part->getIndexBufferProperty());

                auto& passes = effect_->getCurrentTechniqueProperty()->getPassesProperty();
                for (int pass = 0; pass < passes.getCountProperty(); ++pass)
                {
                    passes[pass]->Apply();
                    device.DrawIndexedPrimitives(Xna::Graphics::PrimitiveType::TriangleList,
                                                 part->getVertexOffsetProperty(),
                                                 0,
                                                 part->getNumVerticesProperty(),
                                                 part->getStartIndexProperty(),
                                                 part->getPrimitiveCountProperty());
                    ++report_.effectDrawCalls;
                }
            }
        }
    }

    void SmokeScene::DrawOverlay(Xna::Graphics::SpriteBatch& batch, ui::TextRenderer& text)
    {
        const float scale = text.Scale();
        const auto rectangle = [scale](int x, int y, int size)
        {
            return Xna::Rectangle(static_cast<int>(static_cast<float>(x) * scale),
                                  static_cast<int>(static_cast<float>(y) * scale),
                                  static_cast<int>(static_cast<float>(size) * scale),
                                  static_cast<int>(static_cast<float>(size) * scale));
        };

        if (player_.has_value())
        {
            try
            {
                if (Xna::Graphics::Texture2D* frame = player_->GetTexture(); frame != nullptr)
                {
                    ++report_.videoFramesShown;
                    batch.Draw(
                        *frame, rectangle(kVideoPanelX, kVideoPanelY, kVideoPanelSize), Xna::Color::White);
                }
            }
            catch (const std::exception& error)
            {
                report_.video.error = Describe(error);
                player_.reset();
            }
        }

        if (texture_.has_value())
        {
            batch.Draw(
                *texture_, rectangle(kTexturePanelX, kTexturePanelY, kTexturePanelSize), Xna::Color::White);
        }

        // The font is proved by DRAWING with it, not by having loaded it. `TextRenderer` already
        // holds the HUD face; this line asks for the scene's own, so a build where `Fonts/ui-22`
        // failed shows a shorter list rather than the same picture.
        if (font_.has_value())
        {
            const Xna::Graphics::SpriteFont* previous = text.Font();
            text.SetFont(&*font_);
            float y = 120.0f;
            static constexpr const char* kLabels[] = {"model", "texture", "font", "sound", "effect", "video"};
            const auto items = report_.Items();
            for (std::size_t index = 0; index < items.size(); ++index)
            {
                const SmokeItem& item = *items[index];
                text.DrawShadowed(
                    batch,
                    std::format("{:<8} {}", kLabels[index], item.loaded ? item.detail : ("-- " + item.error)),
                    Xna::Vector2(60.0f, y),
                    ui::Anchor::TopLeft,
                    item.loaded ? Xna::Color::White : Xna::Color::DarkOrange);
                y += 34.0f;
            }
            text.DrawShadowed(batch,
                              std::format("drawn with {}", report_.techniqueDrawn),
                              Xna::Vector2(60.0f, y + 10.0f),
                              ui::Anchor::TopLeft,
                              Xna::Color::White);
            text.SetFont(previous);
        }
    }

} // namespace cnahouse::content
