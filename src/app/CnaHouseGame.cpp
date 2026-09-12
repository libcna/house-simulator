// SPDX-License-Identifier: MIT
#include <array>

#include "cnahouse/app/CnaHouseGame.hpp"

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/world/WorldLoader.hpp"

#include <format>
#include <optional>
#include <stdexcept>

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

#include "cnahouse/debug/PlayerCommands.hpp"
#include "cnahouse/debug/Screenshot.hpp"
#include "cnahouse/debug/TimeCommands.hpp"
#include "cnahouse/debug/VisibilityCommands.hpp"
#include "cnahouse/debug/WeatherCommands.hpp"
#include "cnahouse/environment/SunLight.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/persistence/DesktopSaveStore.hpp"
#include "cnahouse/rendering/SkySystem.hpp"
#include "cnahouse/rendering/StaticGeometryPass.hpp"
#include "cnahouse/ui/LoadingScreen.hpp"
#include "cnahouse/util/Log.hpp"
#include "cnahouse/world/ChunkReader.hpp"

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
    /// `HOUSE-00201`. The smoke scene draws in `OpaqueDynamic` -- the pass a prop belongs in --
    /// rather than in a pass of its own, so what it exercises is the frame's real shape.
    class CnaHouseGame::SmokePass final : public rendering::IRenderPass
    {
    public:
        explicit SmokePass(CnaHouseGame& game) noexcept
            : game_(&game)
        {
        }

        void Draw(rendering::PassContext& context) override
        {
            game_->smoke_->Draw(context.device);
        }

        [[nodiscard]] bool IsActive() const override
        {
            return game_->contentLoaded_ && game_->smoke_ != nullptr;
        }

        [[nodiscard]] bool DisturbsDeviceState() const override
        {
            // The pass binds its own vertex and index buffers and applies an effect pass, none of
            // which `StateTracker` predicts.
            return true;
        }

    private:
        CnaHouseGame* game_;
    };

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
        inputConfig.smoothing = settings_.lookSmoothing;
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

        if (options_.scene.has_value() &&
            (*options_.scene == kBlockoutScene || *options_.scene == kBackFaceScene ||
             *options_.scene == kWalkScene))
        {
            LoadBlockout();
            if (*options_.scene == kWalkScene)
            {
                LoadWalk();
            }
            LoadHudFont();
            contentLoaded_ = true;
            return;
        }

        if (options_.scene.has_value() && *options_.scene == content::SmokeScene::kSceneName)
        {
            // No loading screen in the smoke scene. The title screen covers the frame, and a smoke
            // test whose picture is the title screen proves nothing about the six assets under it.
            smoke_ = std::make_unique<content::SmokeScene>(
                getContentProperty(), effectContent_.get(), audio_, tier_.IsTierE());
            smoke_->Load();
            renderer_.Install(rendering::Pass::OpaqueDynamic, std::make_unique<SmokePass>(*this));
            // The audio gate is opened directly rather than by a keypress: `--scene` is a
            // non-interactive entry point, and a sound that never plays because nobody pressed a
            // key would look exactly like a sound that failed to load (`HOUSE-00155`).
            if (audio_.NoteUserGesture())
            {
                Log::Info(LogCat::Audio, "{}", audio_.Summary());
            }
            LoadHudFont();
            contentLoaded_ = true;
            return;
        }

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

        LoadHudFont();
        contentLoaded_ = true;
    }

    void CnaHouseGame::LoadWalk()
    {
        // §16's world first: the collision is indexed by cell NAME, and which cell a body is in is
        // a question only the world data can answer (§16.4).
        world::WorldData::Contents contents;
        const auto levels = world::WorldLoader::LoadLevels("content/world", contents);
        const auto cells = world::WorldLoader::LoadCells("content/world", contents);
        const auto portals = world::WorldLoader::LoadPortals("content/world", contents);
        const auto openings = world::WorldLoader::LoadOpenings("content/world", contents);
        const auto interactables = world::WorldLoader::LoadInteractables("content/world", contents);
        const auto initialState = world::WorldLoader::LoadInitialState("content/world", contents);
        const auto weather = world::WorldLoader::LoadWeather("content/world", contents);
        // §28's fixtures, for `LightingSystem` (`HOUSE-01251`). 243 rows; the loader is the same
        // one the tests use, so a lights file that would fail CI fails here too.
        const auto lights = world::WorldLoader::LoadLights("content/world", contents);
        if (!levels || !cells || !portals || !openings || !interactables || !initialState || !lights ||
            !weather)
        {
            Log::Error(LogCat::Content,
                       "--scene=walk: the world did not load; drawing from the fixed camera");
            return;
        }
        auto built = world::WorldData::Create(std::move(contents));
        if (!built)
        {
            Log::Error(LogCat::Content, "--scene=walk: {}", built.Error().ToString());
            return;
        }
        world_.emplace(std::move(built.Value()));
        const world::WeatherStart& weatherStart = world_->GetInitialState().weather;
        auto weatherSystem = weather::WeatherSystem::Create(world_->WeatherArchetypes(),
                                                            world_->WeatherTransitions(),
                                                            world_->WeatherRates(),
                                                            weatherStart.state,
                                                            weatherStart.target,
                                                            weatherStart.targetExpiryMinutes);
        if (!weatherSystem)
        {
            Log::Error(LogCat::Content, "--scene=walk: {}", weatherSystem.Error().ToString());
            world_.reset();
            return;
        }
        weather_.emplace(std::move(weatherSystem.Value()));
        if (settings_.weatherMode == WeatherMode::Fixed)
        {
            weather_->SetAutomaticTransitions(false);
            const auto selected = std::ranges::find_if(world_->WeatherArchetypes(),
                                                       [this](const weather::WeatherArchetype& archetype)
                                                       {
                                                           return !archetype.modifier &&
                                                                  util::IdRegistry::NameOf(archetype.id) ==
                                                                      settings_.fixedWeatherArchetype;
                                                       });
            if (selected != world_->WeatherArchetypes().end())
            {
                weather_->TargetArchetypeControl() = selected->id;
            }
            else
            {
                Log::Warn(LogCat::Content,
                          "fixed weather '{}' is not an authored state; keeping {}",
                          settings_.fixedWeatherArchetype,
                          util::IdRegistry::NameOf(weather_->TargetArchetype()));
            }
        }
        else if (settings_.weatherMode == WeatherMode::Off)
        {
            weather_->SetAutomaticTransitions(false);
            weather_->TransitionsPausedControl() = true;
        }
        index_.emplace(world::SpatialIndex::Build(*world_));

        try
        {
            System::IO::FileStream stream(
                "content/world/collision.bin", System::IO::FileMode::Open, System::IO::FileAccess::Read);
            auto loaded = physics::CollisionLoader::Read(stream, "content/world/collision.bin");
            if (!loaded)
            {
                Log::Error(LogCat::Content, "--scene=walk: {}", loaded.Error().Message());
                weather_.reset();
                world_.reset();
                index_.reset();
                return;
            }
            collision_.emplace(std::move(loaded.Value()));
        }
        catch (const std::exception& e)
        {
            Log::Error(LogCat::Content, "--scene=walk: collision.bin: {}", e.what());
            weather_.reset();
            world_.reset();
            index_.reset();
            return;
        }

        // §12's front hall, unless `--player` says otherwise. The middle of a named cell rather
        // than a coordinate somebody measured off a plan: a spawn that is 20 mm inside a wall
        // spends its first frames being shoved out, and the shove is the first thing a screenshot
        // would catch.
        Microsoft::Xna::Framework::Vector3 feet(0.0F, 0.0F, 0.0F);
        if (options_.player.has_value())
        {
            const auto& stand = *options_.player;
            feet = Microsoft::Xna::Framework::Vector3(stand[0], stand[1], stand[2]);
            look_.yaw = Microsoft::Xna::Framework::MathHelper::ToRadians(stand[3]);
            look_.pitch = player::ClampedPitch(Microsoft::Xna::Framework::MathHelper::ToRadians(stand[4]));
        }
        else if (const world::Cell* hall = world_->FindCell(util::Intern("L0_HALL")); hall != nullptr)
        {
            const world::Level* level = world_->FindLevel(hall->level);
            const world::Footprint& box = hall->boxes.front();
            feet = Microsoft::Xna::Framework::Vector3((box.minX + box.maxX) * 0.5F,
                                                      level == nullptr ? 0.0F : level->ffl,
                                                      (box.minZ + box.maxZ) * 0.5F);
        }

        player_ = player::PlayerState{};
        player_.position =
            Microsoft::Xna::Framework::Vector3(feet.X, feet.Y + player_.Rise() + 0.02F, feet.Z);
        player_.yaw = look_.yaw;
        player_.fastWalk = settings_.fastWalk;
        tracker_.Forget();
        tracker_.Update(*world_, *index_, player_.position);
        if (!tracker_.Current().IsValid())
        {
            Log::Error(LogCat::Content,
                       "--scene=walk: ({:.2f}, {:.2f}, {:.2f}) is not in any cell",
                       feet.X,
                       feet.Y,
                       feet.Z);
            weather_.reset();
            world_.reset();
            index_.reset();
            collision_.reset();
            return;
        }

        // §25's walk, over the world just loaded. It runs every frame from here on and `F3`
        // reports it; what the draw list is built from is a separate decision and
        // `BuildRenderList` is the one place that makes it.
        visibility_.emplace(*world_);
        auto shading = lighting::ShadingGrid::ReadFromTitle("content/world/shading.bin");
        if (shading)
        {
            shading_ = std::move(shading.Value());
        }
        else
        {
            // A checkout without Blender can still run in the house. The fallback is deliberately
            // bright (unoccluded), so a missing bake is visible and never mistaken for night.
            shading_ = lighting::ShadingGrid::Unshaded();
            Log::Warn(LogCat::Content,
                      "--scene=walk: {}; daylight uses unshaded windows",
                      shading.Error().ToString());
        }
        // §28.1 and §32.2, at `UpdateStage::Lighting` (`HOUSE-01251`, `HOUSE-01564`). Built once
        // over the cells, fixtures and windows; every frame reads the one simulation clock.
        lighting_.emplace(*world_, shading_, clock_, visibility_->Portals());
        auto skyDome = rendering::SkyDomeReader::ReadFromTitle("content/world/sky_dome.bin");
        auto skyColours = rendering::SkyColourModelReader::ReadFromTitle("content/world/layout.sky.json");
        if (skyDome && skyColours)
        {
            auto sky = std::make_unique<rendering::SkySystem>(
                blockoutCamera_, std::move(*skyDome), std::move(*skyColours));
            skySystem_ = sky.get();
            renderer_.Install(rendering::Pass::Sky, std::move(sky));
        }
        else
        {
            // The walk remains usable in a source checkout that has not built generated content,
            // but the missing layer is never silent or disguised as a plausible sky.
            Log::Error(LogCat::Content,
                       "--scene=walk: {}",
                       skyDome ? skyColours.Error().ToString() : skyDome.Error().ToString());
        }
        if (blockoutChunks_ != nullptr)
        {
            // With the world, so §12's nested cells are drawn with the room they stand in
            // (`HOUSE-00488`): a fridge whose door is shut is still a fridge in the kitchen.
            chunkCuller_.emplace(*blockoutChunks_, &*world_);
            // §25.6's hierarchy over the exterior chunks, built once (`HOUSE-00700`). The
            // outdoors is not a room and the portal walk was never the thing that was supposed to
            // reach it.
            exteriorScene_.emplace(visibility::BuildExteriorScene(*blockoutChunks_, *world_));
            Log::Info(LogCat::Content,
                      "§25.6: {} exterior instance(s) in the hierarchy",
                      exteriorScene_->instances.size());
        }
        // `--no-cull` is the same switch `cull off` throws, set before the first frame: a render
        // test drives the game through `Options` and cannot type into a console.
        cullingEnabled_ = !options_.noCull;
        debug::RegisterVisibilityCommands(console_, debug::VisibilityCommandContext{&cullingEnabled_});
        // §35.2's day length is a SETTING, so the clock's rate comes from the settings file rather
        // than from the constant: a player who chose the slow preset gets it from the first frame
        // and not after opening the menu (`HOUSE-01533`).
        clock_.timeScale =
            environment::TimeScaleForDayLength(static_cast<double>(settings_.dayLengthRealMinutes));
        // §35.2b's table: *"Starting season: Spring -- a new game begins at the vernal equinox."*
        // §35.1's epoch is 1 January, so a clock left at zero would start every session in the
        // middle of winter (`HOUSE-01543`). There is no save to load a time from yet; when there
        // is (`HOUSE-01538`), this is the value it replaces.
        clock_.SetCalendar(environment::kNewGameCalendarDays);
        debug::RegisterTimeCommands(console_, debug::TimeCommandContext{&clock_});
        debug::RegisterWeatherCommands(console_,
                                       debug::WeatherCommandContext{world_->WeatherArchetypes(),
                                                                    &weather_->TargetArchetypeControl(),
                                                                    &weather_->TransitionsPausedControl()});
        debug::RegisterPlayerCommands(console_,
                                      debug::PlayerCommandContext{&player_, &tracker_, &*world_, &*index_});

        // §71's `F9` draws through this, and it needs a device -- which is why it is built here
        // and not with the other members.
        debugDraw_ = std::make_unique<debug::DebugDraw>(getGraphicsDeviceProperty());
        view_.Camera().SetFieldOfView(settings_.fieldOfView);
        view_.Camera().SetViewport(settings_.backBufferWidth, settings_.backBufferHeight);
        view_.Bob().SetLevel(settings_.headBob);
        walking_ = true;

        // Half a second of standing still before anything is drawn: the body was spawned 20 mm
        // clear of the floor and §49.3's step is what puts it down. A screenshot taken on frame
        // one would otherwise be taken 20 mm high, and worse, the eye would still be springing.
        const player::InputState still;
        for (int step = 0; step < 60; ++step)
        {
            const physics::CollisionCell* cell =
                collision_->Cell(util::IdRegistry::NameOf(tracker_.Current()));
            if (cell == nullptr)
            {
                break;
            }
            player_.cellId = cell->id;
            const player::PlayerStepReport report =
                player::PlayerStep(*collision_, *cell, broad_, player_, still, player::kFixedStepSeconds);
            tracker_.Update(*world_, *index_, player_.Feet());
            view_.Update(player_, report, look_.pitch, player::kFixedStepSeconds);
        }
        view_.Snap(player_, look_.pitch);
        ApplyPlayerCamera();

        Log::Info(LogCat::App,
                  "walking in {} at ({:.2f}, {:.2f}, {:.2f}), yaw {:.0f} deg",
                  util::IdRegistry::NameOf(tracker_.Current()),
                  player_.Feet().X,
                  player_.Feet().Y,
                  player_.Feet().Z,
                  static_cast<double>(look_.yaw * 180.0F / 3.14159265F));
    }

    void CnaHouseGame::UpdateWalk(float deltaSeconds)
    {
        // §44's mouse look, from the source that owns the devices (`HOUSE-00622`).
        player::ApplyLook(look_, Input().Current(), Input().LookAvailable());
        player_.yaw = look_.yaw;

        // §49.3: dt = 1/120 s, accumulated from the frame time, at most four steps. The cap is
        // what stops a loading hitch from being simulated in full and walking the body through a
        // wall at the far side of it.
        const int steps = player::FixedSteps(stepAccumulator_, deltaSeconds);
        fixedSteps_ += static_cast<std::uint64_t>(steps);
        for (int step = 0; step < steps; ++step)
        {
            const physics::CollisionCell* cell =
                collision_->Cell(util::IdRegistry::NameOf(tracker_.Current()));
            if (cell == nullptr)
            {
                break;
            }
            player_.cellId = cell->id;
            const player::PlayerStepReport report = player::PlayerStep(
                *collision_, *cell, broad_, player_, Input().Current(), player::kFixedStepSeconds);
            if (report.walkModeChanged)
            {
                // D-09: the walk mode is a SETTING, so the game writes it back rather than the
                // controller keeping a second copy of it.
                settings_.fastWalk = player_.fastWalk;
            }
            tracker_.Update(*world_, *index_, player_.Feet());
            view_.Update(player_, report, look_.pitch, player::kFixedStepSeconds);
            if (const physics::CollisionCell* now =
                    collision_->Cell(util::IdRegistry::NameOf(tracker_.Current()));
                now != nullptr)
            {
                // §44's near-surface pull-back, in the cell the body ENDED the step in.
                view_.ProbeSurfaces(*collision_, *now, broad_);
            }
        }
        ApplyPlayerCamera();
    }

    void CnaHouseGame::ApplyPlayerCamera()
    {
        // §44's camera, as the renderer's `rendering::Camera`: an eye, a target and a lens. The
        // near plane is the VIEW's, because `HOUSE-00628` moves it when the eye is against a wall.
        const player::CameraPose& pose = view_.Camera().Pose();
        blockoutCamera_.eye = pose.eye;
        blockoutCamera_.target = Microsoft::Xna::Framework::Vector3(
            pose.eye.X + pose.forward.X, pose.eye.Y + pose.forward.Y, pose.eye.Z + pose.forward.Z);
        blockoutCamera_.fieldOfViewDegrees = view_.Camera().EffectiveFieldOfViewDegrees();
        blockoutCamera_.nearPlane = view_.Camera().NearPlane();
        blockoutCamera_.farPlane = player::kFarPlane;
    }

    debug::WorldSnapshot CnaHouseGame::WalkSnapshot() const
    {
        debug::WorldSnapshot snapshot;
        snapshot.cell = util::IdRegistry::NameOf(tracker_.Current());
        snapshot.cellFoundBy = tracker_.LastStep();
        if (world_.has_value())
        {
            if (const world::Cell* cell = world_->FindCell(tracker_.Current()); cell != nullptr)
            {
                snapshot.level = util::IdRegistry::NameOf(cell->level);
            }
        }
        snapshot.position = player_.Feet();
        snapshot.yaw = look_.yaw;
        snapshot.pitch = look_.pitch;
        snapshot.speed = player_.alongSlopeSpeed;
        snapshot.onGround = player_.onGround;
        snapshot.crouched = player_.crouched;
        snapshot.fastWalk = player_.fastWalk;
        snapshot.ground = player_.groundKind;
        // The gap the probe last reported is not kept on the state -- a body that is `onGround`
        // is ON it, and §49.3's step already used the number to put it there. Zero says "resting".
        snapshot.groundGap = 0.0F;
        if (collision_.has_value() && player_.surface < collision_->surfaces.size())
        {
            snapshot.surface = collision_->surfaces[player_.surface];
        }
        return snapshot;
    }

    void CnaHouseGame::LoadBlockout()
    {
        auto library = world::ChunkReader::ReadFromTitle("content/world/chunks.bin");
        if (!library)
        {
            Log::Error(LogCat::Content,
                       "the blockout could not be loaded: {} ({})",
                       library.Error().Message(),
                       library.Error().Context());
            return;
        }
        blockoutChunks_ = std::make_unique<world::ChunkLibrary>(std::move(*library));
        blockoutCells_ = std::make_unique<world::CellRuntime>(getGraphicsDeviceProperty(), *blockoutChunks_);

        std::size_t failed = 0;
        for (const std::string& cell : blockoutChunks_->cells)
        {
            if (!blockoutCells_->Load(cell))
            {
                ++failed;
            }
        }

        // §12.1's house from the road: the plot's front boundary is z = 0 and the front wall is at
        // z = -11.6, so an eye on the near verge looking north sees the whole elevation. Slightly
        // east of the centre line and above head height, so the garage wing reads as a wing rather
        // than as part of the front wall -- a dead-on elevation is the one view that cannot show
        // whether the house has any depth.
        blockoutCamera_.eye = Microsoft::Xna::Framework::Vector3(17.0f, 14.0f, 17.0f);
        blockoutCamera_.target = Microsoft::Xna::Framework::Vector3(-1.0f, 5.0f, -19.0f);
        if (options_.camera.has_value())
        {
            // `--camera` wins, which is what `HOUSE-00483`'s twenty poses drive and what a bug
            // report carries so that a view can be looked at again.
            const auto& pose = *options_.camera;
            blockoutCamera_.eye = Microsoft::Xna::Framework::Vector3(pose[0], pose[1], pose[2]);
            blockoutCamera_.target = Microsoft::Xna::Framework::Vector3(pose[3], pose[4], pose[5]);
        }
        blockoutCamera_.fieldOfViewDegrees = 55.0f;
        // NOT §70.2's 0.10 m. That near plane is for a player camera that can stand against a
        // wall, and paired with a far plane past the 400 m world box it gives a depth ratio of
        // 4000:1 -- which stipples every coplanar surface in the house with z-fighting, measured
        // by looking at the first frame this scene ever drew. The blockout is looked at from
        // outside, so it can afford a near plane that leaves the depth buffer some precision.
        blockoutCamera_.nearPlane = 0.5f;
        blockoutCamera_.farPlane = 300.0f;
        // The free-fly camera takes over from wherever the fixed one was pointing, so pressing a
        // key does not jump the view (`HOUSE-00476`). With no input at all it changes nothing,
        // which is what makes `blockout-01` a fixed frame.
        freeFly_.Adopt(blockoutCamera_);
        auto pass = std::make_unique<rendering::StaticGeometryPass>(
            *blockoutChunks_, *blockoutCells_, blockoutCamera_, renderList_);
        // `--scene=blockout-normals` is the same house with the culling reversed (`HOUSE-00478`).
        // A separate scene name rather than a key, because what looks at it is a render test.
        pass->SetShowBackFaces(options_.scene.has_value() && *options_.scene == kBackFaceScene);
        renderer_.Install(rendering::Pass::OpaqueStatic, std::move(pass));

        Log::Info(LogCat::Content,
                  "blockout: {} chunk(s) over {} cell(s), {} resident, {} MB uploaded{}",
                  blockoutChunks_->chunks.size(),
                  blockoutChunks_->cells.size(),
                  blockoutCells_->ResidentChunks(),
                  static_cast<double>(blockoutCells_->ResidentBytes()) / (1024.0 * 1024.0),
                  failed == 0 ? "" : " -- SOME CELLS FAILED");
    }

    void CnaHouseGame::LoadHudFont()
    {
        // The font is the first content this project loads, and it is allowed to be absent: a build
        // whose content tree has not been generated yet must still start and still say so, or the
        // first thing a new contributor sees is a crash. `docs/conventions.md` §5.2 puts the catch
        // here, at the content boundary, and nowhere else.
        try
        {
            hud_->font.emplace(
                getContentProperty().Load<Microsoft::Xna::Framework::Graphics::SpriteFont>("Fonts/ui-16"));
            text_.SetFont(&*hud_->font);
        }
        catch (const std::exception& e)
        {
            Log::Warn(LogCat::Content,
                      "the HUD font 'Fonts/ui-16' did not load, so the version string will not be "
                      "drawn: {}",
                      e.what());
        }
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
        // BEFORE `hud_`, and before the content manager unloads: the scene holds a `VideoPlayer`
        // whose decoder must stop while its `Video` is still alive.
        smoke_.reset();
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
            // §35.1: *"`Update` accumulates `gameTime.ElapsedGameTime · timeScale`"*, from the
            // UNCLAMPED delta -- see `clock_`'s declaration and `HOUSE-01540`.
            clock_.Advance(static_cast<double>(frame.realDeltaSeconds));
            counters_.BeginFrame();
            timing_.BeginFrame();
            overlay_.PushFrameTime(frame.deltaSeconds * 1000.0f);

            // The ONE place the devices are read (`HOUSE-00140`). Every system downstream sees
            // `InputState`, which is expressed in game terms, so none of them can be written against a
            // key.
            {
                const debug::Timing::Scope scope(timing_, UpdateStage::Input);
                Input().Update(frame.deltaSeconds);
            }

            if (weather_.has_value())
            {
                const debug::Timing::Scope scope(timing_, UpdateStage::Weather);
                const environment::SunPosition sun = environment::SunPositionFor(clock_);
                const environment::SunShading shading =
                    environment::SunShadingFor(sun, weather_->State().cloudCover);
                const float directSunlight =
                    sun.altitudeDeg > environment::kRefractedHorizonDeg ? shading.directIntensity : 0.0F;
                const float simulatedMinutes =
                    frame.realDeltaSeconds * static_cast<float>(clock_.timeScale / 60.0);
                if (const util::Result<void> advanced =
                        weather_->Advance(simulatedMinutes,
                                          clock_.Season(),
                                          static_cast<float>(clock_.OutdoorBaseTemperatureC()),
                                          directSunlight);
                    !advanced)
                {
                    throw std::runtime_error(advanced.Error().ToString());
                }
            }

            if (walking_ && !visibilityFrozen_)
            {
                // §49.3's fixed steps, then §44's view over them. The free-fly camera is NOT
                // updated here: two things steering one camera is a fight, and in this scene the
                // body wins -- flying is what `--scene=blockout` is for, and what §25.8's `F5`
                // borrows below.
                const debug::Timing::Scope scope(timing_, UpdateStage::Physics);
                UpdateWalk(frame.deltaSeconds);
            }
            else if (walking_)
            {
                // §25.8's `F5`. The body stands still and the camera flies: the input that walked
                // it now steers the inspection camera, which is what lets a reader leave the room
                // and look back at the cones that decided what was in it. The walk itself is not
                // recomputed -- `UpdateVisibility` returns at once -- so what `F3` reports and
                // `F4` draws is the frame the freeze caught and not the one on screen.
                freeFly_.Update(Input().Current(), Input().LookAvailable(), frame.deltaSeconds);
                freeFly_.ApplyTo(blockoutCamera_);
            }
            if (lighting_.has_value())
            {
                // §7.5's stage 6. Ahead of visibility because `UpdateStage` puts it there and for
                // the reason `UpdateStage` puts it there: §23.3 picks a cell's additive passes
                // from its levels, so the levels have to be this frame's before anything decides
                // what to draw. It runs whether or not the body is walking -- a room's lights are
                // on or off regardless of who is looking at it.
                const debug::Timing::Scope scope(timing_, UpdateStage::Lighting);
                lighting_->Update(frame);
                // Drawing consumes the lighting stage's one solar answer. There is no second
                // SunModel in rendering, so room light and the disc cannot disagree within a frame
                // about where the sun is or how clouds attenuate it.
                if (skySystem_ != nullptr)
                {
                    skySystem_->SetSun(lighting_->Sun(), lighting_->CloudCover());
                }
            }
            if (walking_ && visibility_.has_value())
            {
                // §7.5's stage 11, after the body has moved and the camera is where the frame will
                // be drawn from -- which is why it is not inside `UpdateWalk`'s fixed-step loop.
                const debug::Timing::Scope scope(timing_, UpdateStage::Visibility);
                UpdateVisibility(frame);
            }
            else if (blockoutCells_ != nullptr)
            {
                // `HOUSE-00476`. The debug camera flies; nothing else in this scene moves. Driven
                // from `Update` so its speed is in metres per SECOND and does not change with the
                // frame rate -- which matters because this camera is what a person is holding when
                // they read the frame timings it changes.
                freeFly_.Update(Input().Current(), Input().LookAvailable(), frame.deltaSeconds);
                freeFly_.ApplyTo(blockoutCamera_);
            }

            if (smoke_ != nullptr)
            {
                // `HOUSE-00201`. Driven from `Update` and not from the draw, because starting a
                // sound or advancing a decoder inside a draw would make the frame's cost depend on
                // whether it was a capture frame.
                smoke_->Update(frame.deltaSeconds);
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
            if (menus_.Update(Input().Current(), frame.deltaSeconds))
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

            if (frameLimit_ != 0)
            {
                // The RAW delta, not the smoothed one. A perf number taken from an exponential
                // average is a number about the average, not about the frame -- and the whole point
                // of recording samples rather than a mean is to be able to take a median and see a
                // hitch.
                frameTimes_.push_back(frame.deltaSeconds * 1000.0f);
            }

            // §44's cursor: hidden and held during play, back the moment a menu, the `Alt` key
            // or the window manager wants it (`HOUSE-00624`). One policy rather than three call
            // sites, because whichever ran last would otherwise have the final say.
            player::CaptureRequest capture;
            capture.windowActive = getIsActiveProperty();
            capture.menuOpen = !menus_.Empty();
            capture.freeCursorHeld = Input().Current().freeCursorHeld;
            if (mouseCapture_.Update(capture))
            {
                input_.SetMouseCaptured(mouseCapture_.Captured());
                setIsMouseVisibleProperty(mouseCapture_.CursorVisible());
                Log::Info(LogCat::App, "mouse {}", mouseCapture_.Captured() ? "captured" : "released");
            }

#if CNAHOUSE_DEBUG_TOOLS
            if (Input().Current().screenshotPressed && pendingScreenshot_.empty())
            {
                pendingScreenshot_ = debug::Screenshot::TimestampedName(".");
            }
            if (Input().Current().toggleOverlayPressed)
            {
                overlay_.Toggle();
                Log::Info(LogCat::Debug, "performance overlay {}", overlay_.Visible() ? "shown" : "hidden");
            }
            if (Input().Current().toggleWorldOverlayPressed)
            {
                worldOverlay_.Toggle();
                Log::Info(LogCat::Debug, "world overlay {}", worldOverlay_.Visible() ? "shown" : "hidden");
            }
            if (Input().Current().toggleVisibilityOverlayPressed)
            {
                visibilityOverlay_.Toggle();
                Log::Info(LogCat::Debug,
                          "visibility overlay {}",
                          visibilityOverlay_.Visible() ? "shown" : "hidden");
            }
            if (Input().Current().toggleVisibilityGeometryPressed)
            {
                visibilityGeometry_.Toggle();
                Log::Info(LogCat::Debug,
                          "visibility geometry {}",
                          visibilityGeometry_.Visible() ? "shown" : "hidden");
            }
            if (Input().Current().toggleEnvironmentOverlayPressed)
            {
                environmentOverlay_.Toggle();
                Log::Info(LogCat::Debug,
                          "environment overlay {}",
                          environmentOverlay_.Visible() ? "shown" : "hidden");
            }
            if (Input().Current().toggleFreezeVisibilityPressed && walking_)
            {
                visibilityFrozen_ = !visibilityFrozen_;
                if (visibilityFrozen_)
                {
                    // Adopted, so the inspection camera starts exactly where the eye was: a
                    // detach that jumped would lose the view a reader pressed `F5` to keep.
                    freeFly_.Adopt(blockoutCamera_);
                }
                Log::Info(LogCat::Debug,
                          "visibility {}",
                          visibilityFrozen_ ? "FROZEN; the camera has detached from the body"
                                            : "running again");
            }
            if (Input().Current().togglePhysicsOverlayPressed)
            {
                physicsOverlay_.Toggle();
                Log::Info(
                    LogCat::Debug, "physics overlay {}", physicsOverlay_.Visible() ? "shown" : "hidden");
            }
#endif

            if (Input().Current().cancelPressed)
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
            // `--screenshot-frame` waits, rather than capturing whatever frame 1 happens to hold.
            // A video's first decoded frame arrives when the decoder produces it, and `HOUSE-00201`
            // needs that frame to be on screen for the capture to mean anything.
            if (!pendingScreenshot_.empty() && framesDrawn_ + 1 >= options_.screenshotFrame)
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
            if (fixedStepLimit_ != 0 && fixedSteps_ >= fixedStepLimit_)
            {
                Log::Info(LogCat::App,
                          "fixed-step limit of {} reached after {} frames; exiting",
                          fixedStepLimit_,
                          framesDrawn_);
                Exit();
            }
            else if (frameLimit_ != 0 && framesDrawn_ >= frameLimit_)
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
        BuildRenderList();
        getGraphicsDeviceProperty().Clear(ClearColour());
        rendering::PassContext context{getGraphicsDeviceProperty(), *states_, counters_, smoothedDelta_};
        renderer_.Draw(context);
        // AFTER the passes and before the HUD: §71's `F9` annotates the world it is drawn over,
        // and a wireframe under the geometry is a wireframe nobody can see.
        DrawPhysicsOverlay();
    }

    void CnaHouseGame::UpdateVisibility(const FrameContext& frame)
    {
        if (visibilityFrozen_)
        {
            // §25.8's whole point: the answer stays as it was so the camera can go and look at
            // it. Not even the chunk cull re-runs -- it is part of the same answer.
            return;
        }
        visibility::CameraView view;
        view.cell = tracker_.Current();
        const player::FirstPersonCamera& camera = view_.Camera();
        view.eye = camera.Pose().eye;
        view.viewProjection = camera.View() * camera.Projection();
        view.frustum = visibility::ClipFrustum(camera.Frustum());
        view.nearPlane = camera.Frustum().getNearProperty();
        view.farPlane = camera.Frustum().getFarProperty();
        visibility_->SetCamera(view);
        visibility_->Update(frame);
        if (chunkCuller_.has_value())
        {
            chunkCuller_->Cull(visibility_->Visible());
            CullExterior();
        }
        if (visibilityGeometry_.Visible())
        {
            // Built only while it is up: §25.8's geometry is thousands of segments over a house
            // with every door open, and a frame that is not showing it should not pay for it.
            visibilityGeometry_.Build(
                *world_, visibility_->Visible(), visibility_->Portals(), view_.Camera().Pose().eye);
        }
    }

    debug::VisibilitySnapshot CnaHouseGame::VisibilitySnapshot() const
    {
        debug::VisibilitySnapshot snapshot;
        if (!visibility_.has_value() || !world_.has_value())
        {
            return snapshot;
        }
        snapshot.cell = util::IdRegistry::NameOf(tracker_.Current());
        snapshot.eye = view_.Camera().Pose().eye;
        snapshot.yaw = view_.Camera().Pose().yaw;
        snapshot.cellsInWorld = static_cast<int>(world_->Cells().size());
        snapshot.traversal = visibility_->Stats();
        for (const visibility::VisibleCell& cell : visibility_->Visible())
        {
            debug::VisibleCellLine row;
            row.cell = util::IdRegistry::NameOf(cell.cell);
            row.depth = cell.depth;
            row.cones = static_cast<int>(cell.frustumCount);
            row.conesDropped = cell.frustaDropped;
            row.diffuse = visibility::Has(cell.flags, visibility::ConeFlags::Diffuse);
            snapshot.visible.push_back(std::move(row));
        }
        if (chunkCuller_.has_value())
        {
            snapshot.chunksDrawn = chunkCuller_->Statistics().chunksDrawn;
            snapshot.chunksTested = chunkCuller_->Statistics().chunksTested;
        }
        if (exteriorScene_.has_value() && !exteriorScene_->Empty())
        {
            // §25.6's three counters, which were -1 until `HOUSE-00700` gave the hierarchy
            // something real to hold. `exteriorDrawn` is what the walk over it kept and NOT what
            // the frame added: the two differ by the chunks §25.2 had already found, and F3 says
            // what each system decided rather than what survived the union.
            snapshot.exteriorDrawn = exteriorCuller_.Statistics().instancesDrawn;
            snapshot.exteriorTested = exteriorCuller_.Statistics().instancesTested;
            snapshot.exteriorNodes = exteriorCuller_.Statistics().nodesVisited;
        }
        snapshot.drawCalls = renderList_.DrawCalls();
        snapshot.stateChanges = renderList_.StateChanges();
        // §25.1's step 5 is built from residency and not from the walk above (`BuildRenderList`
        // says why), so the overlay says so rather than reporting a culling system that is not
        // culling. `HOUSE-00684` is what turns this to `ON`.
        snapshot.cullingApplied = CullingApplied();
        snapshot.frozen = visibilityFrozen_;
        snapshot.walkFrame = visibility_->Frame();
        snapshot.inspectionEye = blockoutCamera_.eye;
        return snapshot;
    }

    bool CnaHouseGame::CullingApplied() const noexcept
    {
        // Three things have to be true, and they are three different questions: the player has not
        // typed `cull off`, the scene has a walk to cull with, and that walk has actually run.
        return cullingEnabled_ && walking_ && chunkCuller_.has_value() && visibility_.has_value() &&
               !visibility_->Visible().empty();
    }

    void CnaHouseGame::AddExteriorChunks(const Microsoft::Xna::Framework::Vector3& eye)
    {
        if (exteriorChunks_.empty())
        {
            return;
        }
        // The walk's own answer, sorted, so the difference is one linear pass rather than a set.
        std::vector<std::uint32_t> already(chunkCuller_->Chunks().begin(), chunkCuller_->Chunks().end());
        std::sort(already.begin(), already.end());
        std::vector<std::uint32_t> extra;
        extra.reserve(exteriorChunks_.size());
        std::set_difference(exteriorChunks_.begin(),
                            exteriorChunks_.end(),
                            already.begin(),
                            already.end(),
                            std::back_inserter(extra));
        exteriorAdded_ = extra.size();
        renderList_.AddChunks(*blockoutChunks_, extra, eye);
    }

    void CnaHouseGame::CullExterior()
    {
        // §25.6's steps 1 and 2 (`HOUSE-00700`). The cones are every way the OUTDOORS is being
        // seen this frame: each visible exterior cell's own. An instance in any of them is on
        // screen, and an exterior cell the walk never reached contributes nothing -- which is the
        // point, because its GEOMETRY is still tested against the cones of the ones it did reach.
        exteriorChunks_.clear();
        exteriorCones_.clear();
        if (!exteriorScene_.has_value() || exteriorScene_->Empty() || !world_.has_value())
        {
            return;
        }
        visibility::GatherExteriorCones(*world_, visibility_->Visible(), exteriorCones_);
        if (exteriorCones_.empty())
        {
            return;
        }
        exteriorCuller_.Cull(exteriorScene_->bvh, exteriorCones_, view_.Camera().Pose().eye);
        for (const std::uint32_t instance : exteriorCuller_.Instances())
        {
            exteriorChunks_.push_back(exteriorScene_->ChunkOf(instance));
        }
        std::sort(exteriorChunks_.begin(), exteriorChunks_.end());
    }

    void CnaHouseGame::BuildRenderList()
    {
        renderList_.Clear();
        // A frame that does not take §25.6's path added nothing from it, and saying so here rather
        // than leaving last frame's number standing is what keeps the count a fact about THIS
        // frame -- `cull off` and the residency path both come through here without calling
        // `AddExteriorChunks` at all.
        exteriorAdded_ = 0u;
        if (blockoutChunks_ == nullptr || blockoutCells_ == nullptr)
        {
            return;
        }
        const Microsoft::Xna::Framework::Vector3 eye =
            walking_ ? view_.Camera().Pose().eye : blockoutCamera_.eye;
        if (CullingApplied())
        {
            // §25.1's steps 1-3, all the way through to the draw list: the walk's answer, then the
            // chunks of it that are in one of its cones...
            renderList_.AddChunks(*blockoutChunks_, chunkCuller_->Chunks(), eye);
            // ...and §25.6's, which is a different question about a different structure
            // (`HOUSE-00700`). Added rather than replacing: the two sets overlap wherever the
            // walk did reach an exterior cell, and a chunk drawn twice is a chunk drawn twice, so
            // what goes in is the exterior set MINUS what the walk already found.
            AddExteriorChunks(eye);
            return;
        }
        // Everything resident. Two ways to get here and they are different situations: §71's
        // `cull off`, which `HOUSE-00688` uses to render the same pose twice; and a scene with no
        // walk at all -- `--scene=blockout` looks at the house from the road, where §16.4 has no
        // cell for the camera and a portal walk has nowhere to start.
        renderList_.AddChunks(*blockoutChunks_, blockoutCells_->ResidentChunkIndices(), eye);
    }

    Microsoft::Xna::Framework::Matrix CnaHouseGame::DebugView() const
    {
        // The camera the FRAME is drawn with, which is the body's eye until §25.8's `F5` detaches
        // it. Drawing world-space annotations through the body's camera while the picture came
        // from the inspection camera would put the cones somewhere they are not.
        return blockoutCamera_.View();
    }

    Microsoft::Xna::Framework::Matrix CnaHouseGame::DebugProjection()
    {
        const auto& viewport = getGraphicsDeviceProperty().getViewportProperty();
        const float aspect = viewport.getHeightProperty() > 0
                                 ? static_cast<float>(viewport.getWidthProperty()) /
                                       static_cast<float>(viewport.getHeightProperty())
                                 : 16.0f / 9.0f;
        return blockoutCamera_.Projection(aspect);
    }

    void CnaHouseGame::DrawPhysicsOverlay()
    {
#if CNAHOUSE_DEBUG_TOOLS
        if (!walking_ || debugDraw_ == nullptr)
        {
            return;
        }
        const bool wantsPhysics = physicsOverlay_.Visible() && collision_.has_value();
        if (!wantsPhysics && !visibilityGeometry_.Visible())
        {
            return;
        }
        if (!wantsPhysics)
        {
            // §25.8's `F4` alone: one `Begin`, the cones, one `Flush`.
            debugDraw_->Begin(DebugView(), DebugProjection());
            visibilityGeometry_.Draw(*debugDraw_);
            debugDraw_->Flush();
            return;
        }
        const physics::CollisionCell* cell = collision_->Cell(util::IdRegistry::NameOf(tracker_.Current()));
        debug::PhysicsOverlayBody body;
        body.capsule = player_.Body();
        body.velocity = player_.velocity;
        body.dt = player::kFixedStepSeconds;
        body.cell = cell;
        // §71 says *"in the visible cells"*, and phase 9's visible set does not exist yet: the one
        // the body is in is what there is, and it is also the one §49.3 collides against.
        const std::array<const physics::CollisionCell*, 1> cells{cell};
        physicsOverlay_.Build(*collision_, std::span(cells.data(), cell == nullptr ? 0U : 1U), broad_, body);

        debugDraw_->Begin(DebugView(), DebugProjection());
        physicsOverlay_.Draw(*debugDraw_);
        visibilityGeometry_.Draw(*debugDraw_);
        debugDraw_->Flush();
#endif
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
        if (smoke_ != nullptr)
        {
            smoke_->DrawOverlay(hud_->batch, text_);
        }
        // Player-facing and therefore absent from the title/menu stack and independent of the F8
        // debug overlay. The persisted setting is the one switch for it (`HOUSE-01546`).
        if (settings_.showEnvironmentReadout && menus_.Empty())
        {
            environmentReadout_.Draw(hud_->batch, text_, clock_);
        }

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
        if (walking_)
        {
            worldOverlay_.Draw(hud_->batch, text_, WalkSnapshot());
            visibilityOverlay_.Draw(hud_->batch, text_, VisibilitySnapshot());
        }
        // NOT gated on `walking_`: §35's clock runs in every scene, and the blockout scene is
        // where somebody watching the sun move would be standing.
        environmentOverlay_.Draw(hud_->batch, text_, clock_, weather_.has_value() ? &*weather_ : nullptr);
#if CNAHOUSE_DEBUG_TOOLS
        if (walking_ && physicsOverlay_.Visible())
        {
            // Bottom-left, because §71's `F1`, `F2` and `F9` all start at the same top corner and
            // two of them at once would be one unreadable pile. `F9`'s text is four lines about
            // the body; the rest of it is the wireframe.
            const std::vector<std::string> lines = physicsOverlay_.Lines();
            constexpr float kLineHeight = 18.0f;
            for (std::size_t i = 0; i < lines.size(); ++i)
            {
                const float y = 880.0f - static_cast<float>(lines.size() - 1 - i) * kLineHeight;
                text_.DrawShadowed(hud_->batch,
                                   lines[i],
                                   Microsoft::Xna::Framework::Vector2(12.0f, y),
                                   ui::Anchor::BottomLeft,
                                   Microsoft::Xna::Framework::Color::White);
            }
        }
#endif
#endif
        hud_->batch.End();
    }

} // namespace cnahouse::app
