// SPDX-License-Identifier: MIT
#include "cnahouse/app/CnaHouseGame.hpp"

#include <format>
#include <optional>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
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

    void CnaHouseGame::Initialize()
    {
        Game::Initialize();
        Log::Info(LogCat::App, "{}", VersionLine());
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

        // The font is the first content this project loads, and it is allowed to be absent: a build
        // whose content tree has not been generated yet must still start and still say so, or the
        // first thing a new contributor sees is a crash. `docs/conventions.md` §5.2 puts the catch
        // here, at the content boundary, and nowhere else.
        try
        {
            hud_->font.emplace(
                getContentProperty().Load<Microsoft::Xna::Framework::Graphics::SpriteFont>("Fonts/Hud"));
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

        // Escape exits. The one input the application itself owns; everything else goes through
        // `IInputSource` (`HOUSE-00140`) so no system reads the keyboard directly.
        const auto keyboard = Microsoft::Xna::Framework::Input::Keyboard::GetState();
        if (keyboard.IsKeyDown(Microsoft::Xna::Framework::Input::Keys::Escape))
        {
            Log::Info(LogCat::App, "Escape pressed; exiting after {} frames", framesDrawn_);
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
        hud_->batch.Begin();
        hud_->batch.DrawString(*hud_->font,
                               VersionLine(),
                               Microsoft::Xna::Framework::Vector2(12.0f, 10.0f),
                               Microsoft::Xna::Framework::Color::White);
        hud_->batch.End();
    }

} // namespace cnahouse::app
