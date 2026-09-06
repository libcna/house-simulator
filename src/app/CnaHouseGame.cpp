// SPDX-License-Identifier: MIT
#include "cnahouse/app/CnaHouseGame.hpp"

#include <format>
#include <optional>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/GameServiceContainer.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsAdapter.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/debug/Screenshot.hpp"
#include "cnahouse/persistence/DesktopSaveStore.hpp"
#include "cnahouse/ui/LoadingScreen.hpp"
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
        // Constructed from the REQUESTED tier, which `ResolveTier` already narrows to what this
        // binary contains. `ActivateTierE` narrows it a second time if the content does not load.
        , tier_(options_.tier)
        , renderer_(tier_)
        , audio_(!options_.noAudio)
    {
        graphics_.setPreferredBackBufferWidthProperty(settings_.backBufferWidth);
        graphics_.setPreferredBackBufferHeightProperty(settings_.backBufferHeight);
        graphics_.setSynchronizeWithVerticalRetraceProperty(settings_.verticalSync);
        // Variable timestep: the fixed step is `FrameTimer`'s business, and letting XNA also enforce
        // one would give two accumulators disagreeing about how much time has passed.
        setIsFixedTimeStepProperty(false);
        getContentProperty().setRootDirectoryProperty(options_.contentRoot);
    }

    /// The render target a capture frame is drawn into. Created on demand, because most sessions
    /// never take a screenshot and a spare full-size target is several megabytes of GPU memory.
    class CnaHouseGame::Capture
    {
    public:
        Capture(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, int width, int height)
            : target(device,
                     width,
                     height,
                     false,
                     Microsoft::Xna::Framework::Graphics::SurfaceFormat::Color,
                     Microsoft::Xna::Framework::Graphics::DepthFormat::Depth24,
                     0,
                     // MEASURED (`HOUSE-00079`): the default is `DiscardContents`, so a target that
                     // is unbound and then read gives nothing. `PreserveContents` is required here.
                     Microsoft::Xna::Framework::Graphics::RenderTargetUsage::PreserveContents)
        {
        }

        Microsoft::Xna::Framework::Graphics::RenderTarget2D target;
    };

    CnaHouseGame::~CnaHouseGame() = default;

    /// The HUD as a §7.5 pass. An adapter, deliberately: `DrawHud` is where the HUD is drawn and
    /// duplicating it here to satisfy an interface would be the dead abstraction layer this project
    /// forbids. What the adapter adds is real -- the pass declares that it disturbs device state, so
    /// `Renderer` invalidates the tracker after it, and it declares itself inactive when there is no
    /// font, so an empty HUD is a *skipped* pass in the counters rather than a pass that silently
    /// did nothing.
    class CnaHouseGame::HudPass final : public rendering::IRenderPass
    {
    public:
        explicit HudPass(CnaHouseGame& game) noexcept
            : game_(&game)
        {
        }

        void Draw(rendering::PassContext&) override
        {
            game_->DrawHud();
        }

        [[nodiscard]] bool IsActive() const override
        {
            return game_->contentLoaded_ && game_->hud_ != nullptr && game_->hud_->font.has_value();
        }

        [[nodiscard]] bool DisturbsDeviceState() const override
        {
            // `SpriteBatch::Begin`/`End` sets and restores blend, depth, rasteriser and sampler
            // state together. MEASURED (`HOUSE-00065`): `Begin()` selects `BlendState::AlphaBlend`.
            return true;
        }

    private:
        CnaHouseGame* game_;
    };

    std::string CnaHouseGame::VersionLine()
    {
        return std::format("cna-house {} · {} · Tier {}",
                           CNAHOUSE_VERSION,
                           CNAHOUSE_RENDERER_NAME,
                           CNAHOUSE_TIER_E ? "S+E" : "S");
    }

    std::string CnaHouseGame::SessionLine() const
    {
        // Only when they disagree, so the common case stays short: a Tier-S-only build already says
        // "Tier S" and repeating it would be noise in the corner of every frame.
        if (rendering::RenderTier::CompiledIn() && !tier_.IsTierE())
        {
            return VersionLine() + " · running S";
        }
        return VersionLine();
    }

    std::string CnaHouseGame::FrameTimeLine(float deltaSeconds)
    {
        const float milliseconds = deltaSeconds * 1000.0f;
        const float fps = deltaSeconds > 0.0f ? 1.0f / deltaSeconds : 0.0f;
        return std::format("{:5.2f} ms  {:5.1f} fps", milliseconds, fps);
    }

    void CnaHouseGame::Initialize()
    {
        // EVERYTHING that `LoadContent` reads is set BEFORE the base call, and that is MEASURED,
        // not stylistic: CNA's `Game::Initialize()` calls `LoadContent()` at its end
        // (`modules/runtime/src/Game.cpp`), exactly as XNA 4.0 does. A field assigned after the
        // base call is therefore still empty while content loads -- which is where the render tier
        // and the quality preset are resolved, from exactly these fields. The first version of this
        // function set them afterwards and auto-detect ran against a blank profile: it logged
        // "adapter unknown" and forced anisotropy to 1 on a machine that has it.
        platform_ = Platform::FromBuild();
        // `GraphicsAdapter` is plain XNA 4.0 -- NOT a CNA capability query. It is the one thing
        // about the machine this project is allowed to ask for, and it goes straight into the
        // bug-report header where it belongs. It is a static adapter query, so it answers before
        // the device exists.
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

        if (options_.screenshot.has_value())
        {
            // `--screenshot` takes one frame and exits, which is what makes it usable from a script
            // and from the render regression harness.
            pendingScreenshot_ = *options_.screenshot;
            exitAfterScreenshot_ = true;
        }

        Game::Initialize();

        // AFTER the base call, because both need the `GraphicsDevice` that it is what creates.
        // Installing the HUD pass here rather than in `LoadContent` keeps the pass list a fact
        // about the build rather than about what content happened to load -- `HudPass::IsActive` is
        // what answers the content question, once per frame.
        states_.emplace(getGraphicsDeviceProperty());
        renderer_.Install(rendering::Pass::Hud, std::make_unique<HudPass>(*this));

        Log::Info(LogCat::App, "{}", platform_.Summary());
        Log::Info(LogCat::Audio, "{}", audio_.Summary());
        Log::Info(LogCat::App,
                  "back buffer {}x{}, vsync {}",
                  settings_.backBufferWidth,
                  settings_.backBufferHeight,
                  settings_.verticalSync ? "on" : "off");
    }

    void CnaHouseGame::LoadContent()
    {
        Game::LoadContent();
        hud_ = std::make_unique<Hud>(getGraphicsDeviceProperty());
        text_.SetViewport(settings_.backBufferWidth, settings_.backBufferHeight);
        ActivateTierE();
        // AFTER `ActivateTierE`, never before: a failed Tier-E load narrows the tier, and a quality
        // resolved against the pre-narrowing tier would offer post-processing that cannot run.
        ResolveQuality();

        // The loading screen IS the title screen IS the audio gate (`HOUSE-00155`,
        // `HOUSE-00156`). Pushed before anything else so the player has something to press during
        // load rather than after it.
        auto loading =
            std::make_unique<ui::LoadingScreen>(SessionLine(),
                                                [this]
                                                {
                                                    if (audio_.NoteUserGesture())
                                                    {
                                                        Log::Info(LogCat::Audio, "{}", audio_.Summary());
                                                    }
                                                });
        loading_ = loading.get();
        menus_.Replace(std::move(loading));

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

    void CnaHouseGame::ActivateTierE()
    {
        // ADR-0003 and `HOUSE-00161`: Tier E is activated by TRYING, not by asking. A capability
        // query would answer the wrong question -- a device that supports compiled effects can
        // still be handed a build whose content tree has none -- and `SupportsCapability` is
        // forbidden by ADR-0001 besides.
        if constexpr (!rendering::RenderTier::CompiledIn())
        {
            // `if constexpr`, not a plain `if`. `HOUSE-00160`'s acceptance is that the Tier-E branch
            // is ABSENT from a Tier-S binary, and an ordinary branch leaves the second
            // `ContentManager`, the effect-set load and the asset names in the image -- merely
            // unreached, which is not the same claim. Verified on the `headless` preset, where
            // `TierSelection.cmake` turns Tier E off on its own: `Effects/` does not appear in the
            // binary's strings at all.
            Log::Info(LogCat::Rendering, "Tier S: this binary has no Tier E compiled in");
        }
        else
        {
            if (!tier_.IsTierE())
            {
                Log::Info(LogCat::Rendering, "Tier S (requested, or narrowed by --tier=s)");
                return;
            }

            if (effectContent_ == nullptr)
            {
                // MEASURED (`HOUSE-00076`): `ContentManager(nullptr)` throws "no GraphicsDevice is
                // available from the service provider" at the first load, so it takes the Game's.
                //
                // A SECOND content manager, because `HOUSE-00064` measured that `.xnb` wins the
                // resolution order over `.cnb` within one root: the two trees are two roots so that
                // the Tier-E `.xnb` effects cannot shadow a Tier-S `.cnb` asset of the same name.
                effectContent_ = std::make_unique<Microsoft::Xna::Framework::Content::ContentManager>(
                    &getServicesProperty());
                effectContent_->setRootDirectoryProperty(options_.effectRoot);
            }

            try
            {
                // The whole Tier-E effect set in ONE try, so a partial load is impossible: half a
                // tier is a renderer that works until it reaches the pass whose effect is missing,
                // which would fail in the middle of a frame rather than at load.
                //
                // Only `P1Probe` exists so far; phase 12 onwards adds the real set here, and the
                // shape of this function does not change when it does.
                auto probe =
                    effectContent_->Load<std::shared_ptr<Microsoft::Xna::Framework::Graphics::Effect>>(
                        "Effects/P1Probe");
                if (probe == nullptr)
                {
                    tier_.FallBackToS("the Tier E effect set loaded as null");
                    return;
                }
                Log::Info(LogCat::Rendering, "Tier E active: the compiled effect set loaded");
            }
            catch (const std::exception& e)
            {
                // `ContentLoadException` and `NotSupportedException` both land here, and both mean
                // the same thing to the caller: this binary cannot draw Tier E. Logged ONCE, with
                // the failing asset, and the settings toggle is disabled by `TierEselectable()`.
                tier_.FallBackToS(e.what());
            }
        }
    }

    void CnaHouseGame::ResolveQuality()
    {
        const bool detected = !options_.quality.has_value();
        settings_.quality = detected ? rendering::AutoDetect(platform_, tier_) : *options_.quality;

        const rendering::QualitySettings requested = rendering::SettingsFor(settings_.quality);
        quality_ = rendering::Restrict(requested, platform_, tier_);

        // ALWAYS logged, and with the resolved knobs rather than only the preset name. A bug report
        // that says "high" is ambiguous -- the same preset draws differently on a Tier-S build and
        // on a profile without float render targets -- and this is the line that disambiguates it.
        Log::Info(LogCat::Rendering,
                  "quality {} ({}): shadows {}, particles {}, view {:.2f}x, lod {:+d}, "
                  "anisotropy {}x, post-processing {}",
                  QualityPresetName(settings_.quality),
                  detected ? "auto-detected" : "requested",
                  rendering::ShadowQualityName(quality_.shadows),
                  rendering::ParticleQualityName(quality_.particles),
                  static_cast<double>(quality_.viewDistance),
                  quality_.lodBias,
                  quality_.anisotropy,
                  quality_.postProcessing ? "on" : "off");

        if (quality_.shadows != requested.shadows || quality_.postProcessing != requested.postProcessing ||
            quality_.anisotropy != requested.anisotropy)
        {
            // A second line, only when the effective feature set actually took something away, so
            // that "the preset you asked for is not what you got" is never silent.
            Log::Info(LogCat::Rendering,
                      "the {} row was narrowed by this build's effective feature set "
                      "(cna-house.md §68); the line above is what is drawn",
                      QualityPresetName(settings_.quality));
        }
    }

    void CnaHouseGame::UnloadContent()
    {
        // Cleared BEFORE the font it points at is destroyed. A renderer holding a dangling font is
        // a use-after-free at shutdown, which is the hardest kind to reproduce.
        text_.SetFont(nullptr);
        hud_.reset();
        effectContent_.reset();
        contentLoaded_ = false;
        Game::UnloadContent();
    }

    void CnaHouseGame::Update(Microsoft::Xna::Framework::GameTime& gameTime)
    {
        if (crashed_)
        {
            Exit();
            return;
        }
        try
        {
            Game::Update(gameTime);

            const auto elapsed =
                static_cast<float>(gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());
            const FrameContext frame = timer_.Advance(elapsed);
            Log::BeginFrame(frame.frameIndex);
            counters_.BeginFrame();
            timing_.BeginFrame();
            overlay_.PushFrameTime(frame.deltaSeconds * 1000.0f);

            // The ONE place the devices are read (`HOUSE-00140`). Every system downstream sees
            // `InputState`, which is expressed in game terms, so none of them can be written against a
            // key.
            {
                const debug::Timing::Scope scope(timing_, UpdateStage::Input);
                input_.Update(frame.deltaSeconds);
            }

            // Content is loaded by the time the first frame updates, so the title screen is free to
            // dismiss as soon as the player presses something. This is the line phases 3 onwards
            // replace with the real residency check.
            if (loading_ != nullptr)
            {
                loading_->SetReady(contentLoaded_);
            }

            // The screen stack owns the input while any screen is up. That is what routes the
            // user-gesture audio gate (`HOUSE-00155`): the loading screen's own `Update` sees
            // `anyPressed` and calls back into `audio_`, so there is ONE place the gesture is
            // recognised rather than one in the game and one in the screen.
            if (menus_.Update(input_.Current(), frame.deltaSeconds))
            {
                Exit();
            }
            if (menus_.Empty())
            {
                loading_ = nullptr;
            }

            // A short exponential average. The instantaneous delta jitters by a millisecond or two
            // every frame, which makes the HUD number unreadable and makes a real regression invisible
            // inside the noise; 0.1 settles in about a fifth of a second, fast enough to see a hitch.
            smoothedDelta_ += (frame.deltaSeconds - smoothedDelta_) * 0.1f;

#if CNAHOUSE_DEBUG_TOOLS
            if (input_.Current().screenshotPressed && pendingScreenshot_.empty())
            {
                pendingScreenshot_ = debug::Screenshot::TimestampedName(".");
            }
            if (input_.Current().toggleOverlayPressed)
            {
                overlay_.Toggle();
                Log::Info(LogCat::Debug, "performance overlay {}", overlay_.Visible() ? "shown" : "hidden");
            }
#endif

            if (input_.Current().cancelPressed)
            {
                Log::Info(LogCat::App, "cancel pressed; exiting after {} frames", framesDrawn_);
                Exit();
            }
        }
        catch (const std::exception& e)
        {
            HandleCrash("Update", &e);
        }
        catch (...)
        {
            // NOT a swallow. `docs/conventions.md` §5.4 forbids a `catch (...)` that hides a
            // failure; this one records that something not derived from `std::exception` escaped,
            // which is itself the most useful fact available about it, and then stops.
            HandleCrash("Update", nullptr);
        }
    }

    void CnaHouseGame::Draw(const Microsoft::Xna::Framework::GameTime& gameTime)
    {
        if (crashed_)
        {
            Exit();
            return;
        }
        try
        {
            Game::Draw(gameTime);

            // A pending screenshot renders the SAME frame into a capture target first, then to the
            // back buffer. Rendering it twice rather than reading the presented buffer back is not
            // wasteful thinking: XNA offers no way to read the back buffer, and drawing into a target
            // also makes the image independent of the compositor -- no title bar, no cursor, nothing on
            // top -- which is the only form usable as a regression fixture (`HOUSE-00164`).
            if (!pendingScreenshot_.empty())
            {
                if (capture_ == nullptr)
                {
                    capture_ = std::make_unique<Capture>(
                        getGraphicsDeviceProperty(), settings_.backBufferWidth, settings_.backBufferHeight);
                }
                getGraphicsDeviceProperty().SetRenderTarget(&capture_->target);
                RenderFrame();
                getGraphicsDeviceProperty().SetRenderTarget(nullptr);

                if (auto saved = debug::Screenshot::Save(
                        getGraphicsDeviceProperty(), capture_->target, pendingScreenshot_);
                    !saved)
                {
                    Log::Error(LogCat::Debug, "screenshot failed: {}", saved.Error().ToString());
                    exitCode_ = 1;
                }
                pendingScreenshot_.clear();
                if (exitAfterScreenshot_)
                {
                    Exit();
                }
            }

            RenderFrame();

            ++framesDrawn_;
            if (frameLimit_ != 0 && framesDrawn_ >= frameLimit_)
            {
                Log::Info(LogCat::App, "frame limit of {} reached; exiting", frameLimit_);
                Exit();
            }
        }
        catch (const std::exception& e)
        {
            HandleCrash("Draw", &e);
        }
        catch (...)
        {
            HandleCrash("Draw", nullptr);
        }
    }

    void CnaHouseGame::HandleCrash(std::string_view where, const std::exception* what)
    {
        if (crashed_)
        {
            return; // a second failure while handling the first must not recurse
        }
        crashed_ = true;
        crashMessage_ = what != nullptr ? what->what() : "a non-std exception";
        exitCode_ = 1;

        Log::Fatal(
            LogCat::App, "unhandled exception in {} at frame {}: {}", where, framesDrawn_, crashMessage_);

        // The emergency save is attempted BEFORE anything else, and its own failure is reported
        // rather than allowed to mask the original crash -- which is the classic way a crash report
        // ends up describing the handler instead of the bug.
        try
        {
            auto store = persistence::DesktopSaveStore::Open();
            if (store)
            {
                const std::string report =
                    std::format("{{\n  \"schema\": \"cna-house/crash/1\",\n  \"version\": \"{}\",\n"
                                "  \"where\": \"{}\",\n  \"frame\": {},\n  \"message\": \"{}\",\n"
                                "  \"platform\": \"{}\"\n}}\n",
                                CNAHOUSE_VERSION,
                                where,
                                framesDrawn_,
                                crashMessage_,
                                platform_.Summary());
                if (auto saved = (*store)->Write("crash.json", report); !saved)
                {
                    Log::Error(LogCat::Persistence,
                               "the crash report could not be written: {}",
                               saved.Error().ToString());
                }
                else
                {
                    Log::Info(
                        LogCat::Persistence, "crash report written to {}/crash.json", (*store)->Location());
                }
            }
            else
            {
                Log::Error(
                    LogCat::Persistence, "no save store for the crash report: {}", store.Error().ToString());
            }
        }
        catch (const std::exception& e)
        {
            Log::Error(LogCat::Persistence, "the crash handler itself failed: {}", e.what());
        }

        Exit();
    }

    void CnaHouseGame::RenderFrame()
    {
        getGraphicsDeviceProperty().Clear(ClearColour());
        rendering::PassContext context{getGraphicsDeviceProperty(), *states_, counters_, smoothedDelta_};
        renderer_.Draw(context);
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
        // The menus draw INSIDE the HUD's one batch (`HOUSE-00106`: a draw call is 8.15 us, so a
        // second batch would cost more than everything in it) and BEFORE the corner lines, so the
        // version and frame time stay readable over a title screen.
        menus_.Draw(hud_->batch, text_);

        text_.DrawShadowed(hud_->batch,
                           SessionLine(),
                           Microsoft::Xna::Framework::Vector2(12.0f, 10.0f),
                           ui::Anchor::TopLeft,
                           Microsoft::Xna::Framework::Color::White);
        text_.DrawShadowed(hud_->batch,
                           FrameTimeLine(smoothedDelta_),
                           Microsoft::Xna::Framework::Vector2(12.0f, 10.0f),
                           ui::Anchor::TopRight,
                           Microsoft::Xna::Framework::Color::White);
#if CNAHOUSE_DEBUG_TOOLS
        overlay_.Draw(hud_->batch, text_, platform_, timing_, counters_);
#endif
        hud_->batch.End();
    }

} // namespace cnahouse::app
