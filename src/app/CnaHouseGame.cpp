// SPDX-License-Identifier: MIT
#include "cnahouse/app/CnaHouseGame.hpp"

#include <format>
#include <optional>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsAdapter.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/util/Log.hpp"

namespace cnahouse::app
{
    namespace
    {
        using util::Log;
        using util::LogCat;

        /// The clear colour. Deliberately NOT `CornflowerBlue`: every XNA sample in existence is
        /// cornflower blue, so a screenshot of one proves nothing about which program produced it. This is
        /// a dark neutral the house's own lighting will read against, and `HOUSE-00127`'s "clears to a
        /// known colour" means this exact value.
        constexpr Microsoft::Xna::Framework::Color ClearColour()
        {
            return Microsoft::Xna::Framework::Color(18, 20, 24, 255);
        }

    } // namespace

    /// The HUD's XNA resources, hidden from the header so that including `CnaHouseGame.hpp` does not
    /// drag `SpriteBatch` and `SpriteFont` into every translation unit that only wants to construct
    /// the game.
    class CnaHouseGame::Hud
    {
    public:
        explicit Hud(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device)
            : batch(device)
        {
        }

        Microsoft::Xna::Framework::Graphics::SpriteBatch batch;
        // MEASURED: `SpriteFont` has no default constructor, so it cannot simply be a member waiting to
        // be assigned. `std::optional` is the honest shape anyway -- the font is genuinely absent until
        // it loads, and a build whose content tree has not been generated is allowed to start without
        // one. `has_value()` then IS the "did it load" question, with no separate flag to fall out of
        // step with it.
        std::optional<Microsoft::Xna::Framework::Graphics::SpriteFont> font;
    };

    CnaHouseGame::CnaHouseGame(Options options, Settings settings)
        : options_(std::move(options))
        , settings_(std::move(settings))
        , graphics_(this)
    {
        graphics_.setPreferredBackBufferWidthProperty(settings_.backBufferWidth);
        graphics_.setPreferredBackBufferHeightProperty(settings_.backBufferHeight);
        graphics_.setSynchronizeWithVerticalRetraceProperty(settings_.verticalSync);
        // Variable timestep: the fixed step is `FrameTimer`'s business, and letting XNA also enforce
        // one would give two accumulators disagreeing about how much time has passed.
        setIsFixedTimeStepProperty(false);
        getContentProperty().setRootDirectoryProperty(options_.contentRoot);
    }

    CnaHouseGame::~CnaHouseGame() = default;

    std::string CnaHouseGame::VersionLine()
    {
        return std::format("cna-house {} · {} · Tier {}",
                           CNAHOUSE_VERSION,
                           CNAHOUSE_RENDERER_NAME,
                           CNAHOUSE_TIER_E ? "S+E" : "S");
    }

    std::string CnaHouseGame::FrameTimeLine(float deltaSeconds)
    {
        const float milliseconds = deltaSeconds * 1000.0f;
        const float fps = deltaSeconds > 0.0f ? 1.0f / deltaSeconds : 0.0f;
        return std::format("{:5.2f} ms  {:5.1f} fps", milliseconds, fps);
    }

    void CnaHouseGame::Initialize()
    {
        Game::Initialize();

        platform_ = Platform::FromBuild();
        // `GraphicsAdapter` is plain XNA 4.0 -- NOT a CNA capability query. It is the one thing
        // about the machine this project is allowed to ask for, and it goes straight into the
        // bug-report header where it belongs.
        const auto& adapter =
            Microsoft::Xna::Framework::Graphics::GraphicsAdapter::getDefaultAdapterProperty();
        const auto& mode = adapter.getCurrentDisplayModeProperty();
        platform_.displayWidth = mode.getWidthProperty();
        platform_.displayHeight = mode.getHeightProperty();
        platform_.adapterDescription = adapter.getDescriptionProperty();

        player::InputConfig inputConfig;
        inputConfig.sensitivity = settings_.mouseSensitivity;
        inputConfig.invertY = settings_.invertY;
        inputConfig.recentreX = settings_.backBufferWidth / 2;
        inputConfig.recentreY = settings_.backBufferHeight / 2;
        input_.SetConfig(inputConfig);

        Log::Info(LogCat::App, "{}", platform_.Summary());
        Log::Info(LogCat::App,
                  "back buffer {}x{}, vsync {}, quality {}",
                  settings_.backBufferWidth,
                  settings_.backBufferHeight,
                  settings_.verticalSync ? "on" : "off",
                  QualityPresetName(settings_.quality));
    }

    void CnaHouseGame::LoadContent()
    {
        Game::LoadContent();
        hud_ = std::make_unique<Hud>(getGraphicsDeviceProperty());
        text_.SetViewport(settings_.backBufferWidth, settings_.backBufferHeight);

        // The font is the first content this project loads, and it is allowed to be absent: a build
        // whose content tree has not been generated yet must still start and still say so, or the
        // first thing a new contributor sees is a crash. `docs/conventions.md` §5.2 puts the catch
        // here, at the content boundary, and nowhere else.
        try
        {
            hud_->font.emplace(
                getContentProperty().Load<Microsoft::Xna::Framework::Graphics::SpriteFont>("Fonts/Hud"));
            text_.SetFont(&*hud_->font);
        }
        catch (const std::exception& e)
        {
            Log::Warn(LogCat::Content,
                      "the HUD font 'Fonts/Hud' did not load, so the version string will not be "
                      "drawn: {}",
                      e.what());
        }
        contentLoaded_ = true;
    }

    void CnaHouseGame::UnloadContent()
    {
        // Cleared BEFORE the font it points at is destroyed. A renderer holding a dangling font is
        // a use-after-free at shutdown, which is the hardest kind to reproduce.
        text_.SetFont(nullptr);
        hud_.reset();
        contentLoaded_ = false;
        Game::UnloadContent();
    }

    void CnaHouseGame::Update(Microsoft::Xna::Framework::GameTime& gameTime)
    {
        Game::Update(gameTime);

        const auto elapsed =
            static_cast<float>(gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());
        const FrameContext frame = timer_.Advance(elapsed);
        Log::BeginFrame(frame.frameIndex);

        // The ONE place the devices are read (`HOUSE-00140`). Every system downstream sees
        // `InputState`, which is expressed in game terms, so none of them can be written against a
        // key.
        input_.Update(frame.deltaSeconds);

        // A short exponential average. The instantaneous delta jitters by a millisecond or two
        // every frame, which makes the HUD number unreadable and makes a real regression invisible
        // inside the noise; 0.1 settles in about a fifth of a second, fast enough to see a hitch.
        smoothedDelta_ += (frame.deltaSeconds - smoothedDelta_) * 0.1f;

        if (input_.Current().cancelPressed)
        {
            Log::Info(LogCat::App, "cancel pressed; exiting after {} frames", framesDrawn_);
            Exit();
        }
    }

    void CnaHouseGame::Draw(const Microsoft::Xna::Framework::GameTime& gameTime)
    {
        Game::Draw(gameTime);
        getGraphicsDeviceProperty().Clear(ClearColour());
        DrawHud();

        ++framesDrawn_;
        if (frameLimit_ != 0 && framesDrawn_ >= frameLimit_)
        {
            Log::Info(LogCat::App, "frame limit of {} reached; exiting", frameLimit_);
            Exit();
        }
    }

    void CnaHouseGame::DrawHud()
    {
        if (!contentLoaded_ || hud_ == nullptr || !hud_->font.has_value())
        {
            return;
        }
        // MEASURED (`HOUSE-00065`): the content pipeline premultiplies alpha by default and
        // `SpriteBatch::Begin()` selects `BlendState::AlphaBlend`, which is the premultiplied blend --
        // so the default state is the correct one and nothing may be drawn with `NonPremultiplied`.
        // ONE batch for the whole HUD. `HOUSE-00106` measured a draw call at 8.15 us of CPU, so
        // a batch per string would spend more on submission than the rest of the frame does.
        hud_->batch.Begin();
        text_.DrawShadowed(hud_->batch,
                           VersionLine(),
                           Microsoft::Xna::Framework::Vector2(12.0f, 10.0f),
                           ui::Anchor::TopLeft,
                           Microsoft::Xna::Framework::Color::White);
        text_.DrawShadowed(hud_->batch,
                           FrameTimeLine(smoothedDelta_),
                           Microsoft::Xna::Framework::Vector2(12.0f, 10.0f),
                           ui::Anchor::TopRight,
                           Microsoft::Xna::Framework::Color::White);
        hud_->batch.End();
    }

} // namespace cnahouse::app
