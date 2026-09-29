// SPDX-License-Identifier: MIT
//
// `HOUSE-00135`'s target: the application itself, run for real frames, with no window.
//
// This is the test that would catch the failures unit tests structurally cannot -- a `Game` that
// throws during `Initialize`, content that cannot be found from the working directory, a `Draw`
// that leaves the device in a state the next frame rejects. It runs under the `headless` preset,
// which `HOUSE-00105` measured doing 600 frames with `DISPLAY` unset.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/debug/Console.hpp"
#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/debug/VisibilityGeometryOverlay.hpp"
#include "cnahouse/debug/VisibilityOverlay.hpp"
#include "cnahouse/environment/DayLength.hpp"
#include "cnahouse/environment/Season.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/lighting/LightingSystem.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/player/FirstPersonView.hpp"
#include "cnahouse/player/FixedStep.hpp"
#include "cnahouse/player/IInputSource.hpp"
#include "cnahouse/player/KeyboardMouseSource.hpp"
#include "cnahouse/ui/TextRenderer.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Log.hpp"
#include "unit/StairPath.hpp"

namespace
{
    using cnahouse::app::CnaHouseGame;
    using cnahouse::app::Options;
    using cnahouse::app::Settings;

    TEST(HeadlessRunTests, TheGameRunsRealFramesAndExitsCleanly)
    {
        cnahouse::util::Log::ResetForTesting();

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        Settings settings = Settings::Defaults();
        // Small, because the frame count is what is under test and the resolution is not.
        settings.backBufferWidth = 640;
        settings.backBufferHeight = 360;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(120);
        game.Run();

        EXPECT_GE(game.FramesDrawn(), 120u) << "the frame limit must actually stop the loop";
        EXPECT_EQ(game.ExitCode(), 0) << "and it must stop cleanly, not by throwing";
    }

    TEST(HeadlessRunTests, TheOpaquePassDrawsTheSortedListAndNotTheResidencyMap)
    {
        // `HOUSE-00676`. The pass walked the residency map itself until §25.1's step 5 existed;
        // now the frame builds a list, sorts it, and the pass submits its own slice. What that
        // buys is measurable and is measured here: the blockout colour is written once per RUN of
        // chunks sharing a material, so §71.2's *state changes* fall from very nearly one per
        // chunk to one per material.
        cnahouse::util::Log::ResetForTesting();

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.scene = "blockout";
        // The counts below are the High preset's: since `HOUSE-02405` the lower presets draw their
        // own vegetation LOD, and the auto-detected row depends on the build.
        options.quality = cnahouse::app::QualityPreset::High;
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 640;
        settings.backBufferHeight = 360;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(4);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        const auto& list = game.RenderListForTesting();
        ASSERT_GT(list.Size(), 0U) << "the frame drew from an empty list";
        EXPECT_TRUE(list.IsSorted()) << "the pass read its slice, which is what sorts the list";
        // HOUSE-03380's completed C3 furnishing pass brings the unculled house to 1,114 calls.
        // The 1,400-call design envelope still distinguishes one frame from FOUR accumulated frames:
        // a list not emptied between them would be four houses long and still draw a
        // correct-looking picture. This diagnostic remains far below §71.2's 1,400-call
        // worst-case envelope; the materials
        // stay truthful instead of being flattened into a shell finish to satisfy the snapshot.
        EXPECT_LE(list.DrawCalls(), 1400) << "the list was not cleared between frames";

        const cnahouse::debug::Counter* chunks = game.CountersForTesting().Find("static.chunks");
        const cnahouse::debug::Counter* states = game.CountersForTesting().Find("static.stateChanges");
        ASSERT_NE(chunks, nullptr);
        ASSERT_NE(states, nullptr);
        std::printf("  blockout: %lld chunk(s) submitted, %lld state change(s), list of %zu\n",
                    static_cast<long long>(chunks->Max()),
                    static_cast<long long>(states->Max()),
                    list.Size());

        // The static blockout diagnostic only draws its opaque slice. HOUSE-01037's one plant
        // in each furnished room and HOUSE-00772's canonical vegetation add *real* alpha-tested
        // leaf chunks to the whole sorted list; counting those as missing opaque draws would hide
        // the reason for the mismatch.
        std::size_t opaque = 0;
        std::size_t cutouts = 0;
        int opaqueChanges = 0;
        const cnahouse::visibility::RenderItem* previousOpaque = nullptr;
        for (const cnahouse::visibility::RenderItem& item : list.Items())
        {
            if (item.pass == cnahouse::rendering::Pass::AlphaTest)
            {
                ++cutouts;
                continue;
            }
            if (item.pass != cnahouse::rendering::Pass::OpaqueStatic)
            {
                continue;
            }
            if (previousOpaque != nullptr &&
                (item.effect != previousOpaque->effect || item.material != previousOpaque->material))
            {
                ++opaqueChanges;
            }
            previousOpaque = &item;
            ++opaque;
        }
        // HOUSE-03380 measured 50 world-wide alpha-test batches in the completed C3 house;
        // HOUSE-01748's wet-material split adds the outdoor wet variant as one sorted batch.
        // The set includes the formal sofa fringe, indoor plants and exterior trees.
        // Repeated plants remain sub-ranges of shared draws within each owner cell.
        EXPECT_EQ(cutouts, 51U) << "the formal sofa fringe, indoor plant leaves and exterior foliage batches";
        EXPECT_EQ(opaque + cutouts, list.Size()) << "unexpected pass items entered the blockout list";
        // Every opaque item was drawn: nothing in that slice named a chunk the runtime could
        // not find. Alpha-tested leaves belong to AlphaTestPass, not this debug opaque pass.
        EXPECT_EQ(static_cast<std::size_t>(chunks->Max()), opaque);
        // And the material was bound once per run, not once per chunk. The unculled diagnostic
        // reaches 131 at HOUSE-03380; each role contributes
        // one sorted material bind.
        // That remains below §71.2's 210 worst case; visible poses protect smaller rows.
        EXPECT_GT(states->Max(), 0);
        EXPECT_LE(states->Max(), 140);
        EXPECT_LT(states->Max(), chunks->Max() / 4)
            << "the sort bought nothing: the pass is rebinding almost per chunk";
        // The pass's own count and the list's agree, which is what says the two are counting the
        // same thing rather than each counting its own.
        EXPECT_EQ(states->Max(), opaqueChanges + 1)
            << "the opaque slice counts CHANGES between neighbours and the pass counts BINDS; "
               "the first bind is not a change";
        int sortedChanges = 0;
        const cnahouse::visibility::RenderItem* previous = nullptr;
        for (const cnahouse::visibility::RenderItem& item : list.Items())
        {
            if (previous != nullptr && (item.pass != previous->pass || item.effect != previous->effect ||
                                        item.material != previous->material))
            {
                ++sortedChanges;
            }
            previous = &item;
        }
        EXPECT_EQ(list.StateChanges(), sortedChanges)
            << "the list's metric must include the opaque-to-cutout boundary and each foliage "
               "material transition";
    }

    /// Presses one key on the first frame and holds nothing afterwards, which is what an EDGE is.
    class OneKeyPress final : public cnahouse::player::IInputSource
    {
    public:
        explicit OneKeyPress(bool cnahouse::player::InputState::* edge)
            : edge_(edge)
        {
        }

        void Update(float) override
        {
            // True on the FIRST update and never again, which is what an edge is -- and it has to
            // be set inside `Update` rather than in the constructor, because the game samples the
            // source at the top of the frame and would clear a press made before it.
            state_.*edge_ = !fired_;
            fired_ = true;
        }

        [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
        {
            return state_;
        }

        [[nodiscard]] bool LookAvailable() const noexcept override
        {
            return false;
        }

    private:
        cnahouse::player::InputState state_;
        bool cnahouse::player::InputState::* edge_;
        bool fired_ = false;
    };

    /// Dismisses the loading/audio gate, then chooses Start on the main menu.
    class StartMenuDriver final : public cnahouse::player::IInputSource
    {
    public:
        void Update(float) override
        {
            state_ = {};
            if (frame_ == 0U)
            {
                state_.anyPressed = true;
            }
            else if (frame_ == 1U)
            {
                state_.uiAcceptPressed = true;
            }
            ++frame_;
        }

        [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
        {
            return state_;
        }

        [[nodiscard]] bool LookAvailable() const noexcept override
        {
            return false;
        }

    private:
        cnahouse::player::InputState state_;
        std::uint64_t frame_ = 0;
    };

    /// Dismisses loading, moves from Start to Credits, and opens the generated document.
    class CreditsMenuDriver final : public cnahouse::player::IInputSource
    {
    public:
        void Update(float) override
        {
            state_ = {};
            state_.anyPressed = frame_ == 0U;
            state_.uiDownPressed = frame_ == 1U || frame_ == 2U;
            state_.uiAcceptPressed = frame_ == 3U;
            ++frame_;
        }

        [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
        {
            return state_;
        }

        [[nodiscard]] bool LookAvailable() const noexcept override
        {
            return false;
        }

    private:
        cnahouse::player::InputState state_;
        std::uint64_t frame_ = 0;
    };

    /// Opens Settings from the main menu, changes its first row, then uses Escape to go back.
    class SettingsBackDriver final : public cnahouse::player::IInputSource
    {
    public:
        void Update(float) override
        {
            state_ = {};
            state_.anyPressed = frame_ == 0U;
            state_.uiDownPressed = frame_ == 1U;
            state_.uiAcceptPressed = frame_ == 2U;
            state_.uiRightPressed = frame_ == 3U;
            state_.cancelPressed = frame_ == 4U;
            ++frame_;
        }

        [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
        {
            return state_;
        }

        [[nodiscard]] bool LookAvailable() const noexcept override
        {
            return false;
        }

    private:
        cnahouse::player::InputState state_;
        std::uint64_t frame_ = 0;
    };

    /// Starts the walk, opens pause with Escape, then presses Escape again to resume.
    class PauseBackDriver final : public cnahouse::player::IInputSource
    {
    public:
        void Update(float) override
        {
            state_ = {};
            state_.anyPressed = frame_ == 0U;
            state_.uiAcceptPressed = frame_ == 1U;
            state_.cancelPressed = frame_ == 2U || frame_ == 3U;
            ++frame_;
        }

        [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
        {
            return state_;
        }

        [[nodiscard]] bool LookAvailable() const noexcept override
        {
            return false;
        }

    private:
        cnahouse::player::InputState state_;
        std::uint64_t frame_ = 0;
    };

    TEST(HeadlessRunTests, LoadingHandsOffToMainMenuAndStartEntersTheHouse)
    {
        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        StartMenuDriver input;
        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        game.SetFrameLimit(4);
        game.Run();

        ASSERT_EQ(game.ExitCode(), 0);
        EXPECT_TRUE(game.Menus().Empty());
        EXPECT_FALSE(cnahouse::util::IdRegistry::NameOf(game.CellForTesting()).empty());
    }

    TEST(HeadlessRunTests, CreditsLoadsTheGeneratedAttributionDocumentThroughTitleContainer)
    {
        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CreditsMenuDriver input;
        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        game.SetFrameLimit(5);
        game.Run();

        ASSERT_EQ(game.ExitCode(), 0);
        ASSERT_NE(game.Menus().Top(), nullptr);
        ASSERT_EQ(game.Menus().Top()->Id(), cnahouse::ui::ScreenId::Credits);
        const auto* credits = dynamic_cast<const cnahouse::ui::CreditsScreen*>(game.Menus().Top());
        ASSERT_NE(credits, nullptr);
        EXPECT_GT(credits->LineCount(), 700U);
    }

    TEST(HeadlessRunTests, MouseRowsFollowTheLiveCanvasAndFontAfterDisplayChanges)
    {
        class ClickDrawnRows final : public cnahouse::player::IInputSource
        {
        public:
            explicit ClickDrawnRows(CnaHouseGame& game)
                : game_(game)
            {
            }

            void Update(float deltaSeconds) override
            {
                using Microsoft::Xna::Framework::Input::ButtonState;
                using Microsoft::Xna::Framework::Input::KeyboardState;
                using Microsoft::Xna::Framework::Input::Keys;
                using Microsoft::Xna::Framework::Input::MouseState;
                const auto config = game_.DesktopInputConfigForTesting();
                source_.SetConfig(config);
                const auto& viewport = game_.getGraphicsDeviceProperty().getViewportProperty();
                cnahouse::ui::TextRenderer drawnLayout;
                drawnLayout.SetViewport(viewport.getWidthProperty(),
                                        viewport.getHeightProperty(),
                                        viewport.getTitleSafeAreaProperty());
                const auto drawnCanvas = drawnLayout.LayoutBounds();
                if (frame_ == 5U || frame_ == 15U)
                {
                    EXPECT_NEAR(game_.UserSettings().fieldOfView, 85.0F, 0.1F);
                }
                if (frame_ == 9U || frame_ == 19U)
                {
                    EXPECT_NEAR(game_.UserSettings().fieldOfView, 65.0F, 0.1F);
                }
                if (frame_ == 13U || frame_ == 17U)
                {
                    EXPECT_EQ(game_.FullscreenForTesting(), frame_ == 13U);
                }
                const float row = frame_ == 2U                     ? 0.48F
                                  : frame_ == 6U                   ? 0.149F
                                  : frame_ == 12U || frame_ == 16U ? 0.187F
                                                                   : 0.262F;
                const float sliderX = frame_ == 8U || frame_ == 18U ? 0.6125F : 0.7375F;
                const int x =
                    drawnCanvas.X + (frame_ == 2U
                                         ? drawnCanvas.Width / 2
                                         : static_cast<int>(sliderX * static_cast<float>(drawnCanvas.Width)));
                const int y =
                    frame_ == 10U
                        ? config.viewportHeight - 1
                        : drawnCanvas.Y +
                              static_cast<int>(std::lround(row * static_cast<float>(drawnCanvas.Height) +
                                                           config.pointerOffsetY));
                const bool press = frame_ == 2U || frame_ == 4U || frame_ == 6U || frame_ == 8U ||
                                   frame_ == 10U || frame_ == 12U || frame_ == 14U || frame_ == 16U ||
                                   frame_ == 18U;
                source_.Apply(frame_ == 0U ? KeyboardState{Keys::Space} : KeyboardState({}),
                              MouseState(x,
                                         y,
                                         0,
                                         press ? ButtonState::Pressed : ButtonState::Released,
                                         ButtonState::Released,
                                         ButtonState::Released,
                                         ButtonState::Released,
                                         ButtonState::Released),
                              deltaSeconds);
                ++frame_;
            }

            const cnahouse::player::InputState& Current() const noexcept override
            {
                return source_.Current();
            }

            bool LookAvailable() const noexcept override
            {
                return false;
            }

        private:
            CnaHouseGame& game_;
            cnahouse::player::KeyboardMouseSource source_;
            unsigned int frame_ = 0U;
        };

        for (const auto size : {cnahouse::app::DisplaySize{800, 600},
                                cnahouse::app::DisplaySize{1600, 900},
                                cnahouse::app::DisplaySize{2000, 900}})
        {
            Options options;
            options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
            options.effectRoot = CNAHOUSE_TEST_EFFECT_ROOT;
            options.noAudio = true;
            options.screenshotFrame = 19U;
            const auto output = std::filesystem::path(CNAHOUSE_TEST_OUTPUT_DIR) / "house-03574";
            std::filesystem::create_directories(output);
            options.screenshot = (output / (std::to_string(size.width) + "-settings.png")).string();
            Settings settings = Settings::Defaults();
            settings.backBufferWidth = size.width;
            settings.backBufferHeight = size.height;
            settings.fieldOfView = 65.0F;
            settings.verticalSync = false;
            CnaHouseGame game(options, settings);
            ClickDrawnRows input(game);
            game.SetInputSourceForTesting(&input);
            game.SetFrameLimit(20U);
            game.Run();
            ASSERT_EQ(game.ExitCode(), 0);
            ASSERT_NE(game.Menus().Top(), nullptr);
            EXPECT_EQ(game.Menus().Top()->Id(), cnahouse::ui::ScreenId::SettingsMenu);
            EXPECT_NEAR(game.UserSettings().fieldOfView, 65.0F, 0.1F);
            EXPECT_FLOAT_EQ(game.UserSettings().masterVolume, settings.masterVolume);
            EXPECT_FLOAT_EQ(game.UserSettings().footstepsVolume, settings.footstepsVolume);
            EXPECT_FLOAT_EQ(game.UserSettings().ambienceVolume, settings.ambienceVolume);
            EXPECT_FLOAT_EQ(game.UserSettings().weatherVolume, settings.weatherVolume);
            EXPECT_TRUE(game.UserSettings().backBufferWidth != size.width ||
                        game.UserSettings().backBufferHeight != size.height)
                << "the DisplaySize click must exercise a real ApplyChanges before the second FOV click";
            const auto& config = game.DesktopInputConfigForTesting();
            EXPECT_EQ(config.viewportWidth, game.UserSettings().backBufferWidth);
            EXPECT_EQ(config.viewportHeight, game.UserSettings().backBufferHeight);
            EXPECT_GT(config.pointerOffsetY, 0.0F);
        }
    }

    TEST(HeadlessRunTests, EscapeReturnsFromChangedSettingsWithoutEndingTheSession)
    {
        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        SettingsBackDriver input;
        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        game.SetFrameLimit(8);
        game.Run();

        ASSERT_EQ(game.ExitCode(), 0);
        EXPECT_GE(game.FramesDrawn(), 8U) << "Escape in Settings terminated the application";
        ASSERT_NE(game.Menus().Top(), nullptr);
        EXPECT_EQ(game.Menus().Top()->Id(), cnahouse::ui::ScreenId::MainMenu);
    }

    TEST(HeadlessRunTests, EscapeOpensPauseThenASecondPressResumesWithoutEndingTheSession)
    {
        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        PauseBackDriver input;
        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        game.SetFrameLimit(8);
        game.Run();

        ASSERT_EQ(game.ExitCode(), 0);
        EXPECT_GE(game.FramesDrawn(), 8U) << "Escape in the walk or pause menu ended the application";
        EXPECT_TRUE(game.Menus().Empty()) << "one Escape must open pause and the next must resume";
    }

    TEST(HeadlessRunTests, AltEnterSwitchesTheGraphicsManagerInBothDirections)
    {
        for (const bool startFullscreen : {false, true})
        {
            OneKeyPress input(&cnahouse::player::InputState::toggleFullscreenPressed);
            Options options;
            options.headless = true;
            options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
            options.noAudio = true;
            Settings settings = Settings::Defaults();
            settings.backBufferWidth = 320;
            settings.backBufferHeight = 180;
            settings.fullscreen = startFullscreen;
            settings.verticalSync = false;

            CnaHouseGame game(options, settings);
            game.SetInputSourceForTesting(&input);
            game.SetFrameLimit(4);
            game.Run();
            ASSERT_EQ(game.ExitCode(), 0);
            EXPECT_EQ(game.FullscreenForTesting(), !startFullscreen)
                << "one fullscreen request must change the XNA graphics manager";
        }
    }

    TEST(HeadlessRunTests, InitialFullscreenSettingIsAppliedToTheGraphicsDevice)
    {
        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.fullscreen = true;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(4);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);
        EXPECT_TRUE(game.FullscreenForTesting());
    }

    TEST(HeadlessRunTests, TheSessionHasSection35sClockAndItRan)
    {
        // §35.1: *"everything time-dependent reads it; nothing else keeps its own."* Nothing reads
        // it yet -- §36's weather, §35.3's sun and the lighting phase are what will -- so the only
        // thing that can go wrong quietly is the wiring itself: a clock declared, registered with
        // the console, and never advanced.
        //
        // **The clamped-vs-real choice is NOT provable here**, and saying so is better than a
        // claim that looks like it proves it. `FrameContext`'s two deltas are the same number on
        // every frame that is not a hitch, and a headless run of 30 frames produces none; the
        // choice is proved in `ClockHitchTests`, where a 250 ms frame can be handed to the timer
        // directly.
        Options options;
        options.headless = true;
        options.scene.emplace("walk");
        Settings settings = Settings::Defaults();
        settings.verticalSync = false;
        settings.moonPhaseSpeedMultiplier = 6.0F;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(30);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        const cnahouse::environment::SimClock& clock = game.ClockForTesting();
        // §35.2's default day length is a SETTING, and the rate has to come from it rather than
        // from the constant -- a player who chose the slow preset gets it from the first frame.
        EXPECT_DOUBLE_EQ(
            clock.timeScale,
            cnahouse::environment::TimeScaleForDayLength(static_cast<double>(settings.dayLengthRealMinutes)));
        EXPECT_DOUBLE_EQ(clock.timeScale, 60.0) << "§35.2's chosen 60x did not reach the game";
        EXPECT_DOUBLE_EQ(clock.moonPhaseSpeedMultiplier, 6.0)
            << "§33.5's saved phase speed did not reach the shared clock";
        // §35.2b's table, end to end: *"Starting season: Spring -- a new game begins at the vernal
        // equinox."* §35.1's epoch is 1 January, so a clock left at zero starts every session in
        // the middle of winter, and nothing downstream would ever say so (`HOUSE-01543`).
        EXPECT_EQ(clock.Season().primary, static_cast<int>(cnahouse::environment::Season::Spring))
            << "a new game did not start in spring";
        EXPECT_LT(clock.YearFraction(), 0.01) << "a new game did not start at the vernal equinox";
        EXPECT_EQ(clock.Standard().month, 3) << "the equinox is in March";
        EXPECT_EQ(clock.Standard().year, 2031) << "the clock did not start in §35.1's epoch year";

        // Thirty frames of a headless run are milliseconds of wall clock, so what is asserted is
        // that the clock MOVED from where the new game put it and did not move absurdly -- not a
        // duration, which would be a measurement of the machine.
        const double moved = clock.CalendarDays() - cnahouse::environment::kNewGameCalendarDays;
        EXPECT_GT(moved, 0.0) << "thirty frames did not advance §35's clock at all";
        EXPECT_LT(moved, 1.0) << "thirty frames advanced the calendar by a day";
        EXPECT_TRUE(std::isfinite(clock.OutdoorBaseTemperatureC()));
    }

    TEST(HeadlessRunTests, TheWalkSessionAdvancesItsOneLiveWeatherSystem)
    {
        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(30);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        const cnahouse::weather::WeatherSystem* weather = game.WeatherForTesting();
        ASSERT_NE(weather, nullptr);
        EXPECT_EQ(cnahouse::util::IdRegistry::NameOf(weather->TargetArchetype()), "W_PARTLY");
        EXPECT_LT(weather->TargetExpiryMinutes(), 140.0F);
        // The remaining lifetime must stay positive so the initial target did not expire. Do not
        // impose a tighter wall-time bound: thirty software-rendered frames took 2.35 simulated
        // minutes when four integration tests shared a loaded worker, while passing in isolation.
        EXPECT_GT(weather->TargetExpiryMinutes(), 0.0F);
        EXPECT_FALSE(weather->TransitionsPaused());
    }

    TEST(HeadlessRunTests, PressingF8ShowsTheClockTheFrameActuallyRanOn)
    {
        // `HOUSE-01537`. §71's `F8`, end to end: the key reaches the overlay, and behind it the
        // clock this session has actually been advancing -- not a fresh one the panel made for
        // itself. `EnvironmentOverlayTests` proves what the lines SAY; what is proved here is that
        // pressing the key shows them, and that they are about this game's clock.
        cnahouse::util::Log::ResetForTesting();

        OneKeyPress input(&cnahouse::player::InputState::toggleEnvironmentOverlayPressed);

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        game.SetFrameLimit(30);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        EXPECT_TRUE(game.EnvironmentOverlayForTesting().Visible())
            << "one press of F8 did not show the overlay";

        // The panel reads the SESSION's clock. Thirty frames have advanced it, so the lines must
        // describe a clock that has moved -- a panel built over a default-constructed `SimClock`
        // would read exactly midnight on 1 January and pass every other claim in this file.
        const std::optional<cnahouse::debug::CelestialOverlayState> celestial = game.CelestialSnapshot();
        ASSERT_TRUE(celestial.has_value());
        const std::vector<std::string> lines = game.EnvironmentOverlayForTesting().Lines(
            game.ClockForTesting(), game.WeatherForTesting(), &*celestial);
        ASSERT_GE(lines.size(), 4U);
        EXPECT_EQ(lines[0], "F8  environment");
        EXPECT_GT(game.ClockForTesting().epochSeconds, 0.0);
        EXPECT_EQ(lines[2].find("epoch 0.0 s"), std::string::npos)
            << "the panel is reading a clock that has not run: " << lines[2];
        const std::string joined = [&lines]
        {
            std::string result;
            for (const std::string& line : lines)
            {
                result += line + '\n';
            }
            return result;
        }();
        EXPECT_NE(joined.find("weather  W_PARTLY"), std::string::npos) << joined;
        EXPECT_NE(joined.find("rng      "), std::string::npos) << joined;
        EXPECT_NE(joined.find("sun      alt "), std::string::npos) << joined;
        EXPECT_NE(joined.find("moon     alt "), std::string::npos) << joined;
        EXPECT_NE(joined.find("phase    "), std::string::npos) << joined;
        EXPECT_NE(joined.find("stars    draw "), std::string::npos) << joined;
        EXPECT_EQ(joined.find("celestial unavailable"), std::string::npos) << joined;
        const cnahouse::lighting::LightingSystem* lighting = game.LightingForTesting();
        ASSERT_NE(lighting, nullptr);
        EXPECT_DOUBLE_EQ(celestial->sun.altitudeDeg, lighting->Sun().altitudeDeg);
        EXPECT_DOUBLE_EQ(celestial->moon.azimuthDeg, lighting->Moon().azimuthDeg);
        EXPECT_DOUBLE_EQ(celestial->moonPhase.phase, lighting->LunarPhase().phase);
        for (const std::string& line : lines)
        {
            std::printf("  %s\n", line.c_str());
        }
    }

    TEST(HeadlessRunTests, PressingF3ShowsTheWalkTheFrameActuallyDid)
    {
        // `HOUSE-00681`. §25.8's `F3`, end to end: the key reaches the overlay, and behind it a
        // real §25.2 walk ran over §12's house with the camera the frame was drawn from. The
        // numbers are what phase 9 spent itself proving, and this is the first place a person can
        // see them without a debugger.
        cnahouse::util::Log::ResetForTesting();

        OneKeyPress input(&cnahouse::player::InputState::toggleVisibilityOverlayPressed);

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        // §12's kitchen, facing east: a room with doorways, so the walk has something to cross.
        options.player = std::array<float, 5>{-3.00f, 0.60f, -25.05f, 90.0f, 0.0f};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        game.SetFrameLimit(30);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        EXPECT_TRUE(game.VisibilityOverlayForTesting().Visible())
            << "one press of F3 did not show the overlay";

        const cnahouse::debug::VisibilitySnapshot snapshot = game.VisibilitySnapshotForTesting();
        const std::vector<std::string> lines = game.VisibilityOverlayForTesting().Lines(snapshot);
        for (const std::string& line : lines)
        {
            std::printf("  %s\n", line.c_str());
        }

        // A real walk, not an empty struct: the camera's own cell at least, and portals tested.
        ASSERT_FALSE(snapshot.visible.empty()) << "the walk reached nothing, not even its own cell";
        EXPECT_EQ(snapshot.visible.front().depth, 0) << "the camera's own cell is the walk's root";
        EXPECT_EQ(snapshot.cell, snapshot.visible.front().cell);
        EXPECT_GT(snapshot.traversal.portalsTested, 0);
        EXPECT_EQ(snapshot.cellsInWorld, 96) << "§16's house has 96 cells";
        // §71.2's hard fail is 30 visible cells; the authored static door poses stay under it.
        EXPECT_LE(snapshot.visible.size(), 30U);
        EXPECT_EQ(snapshot.traversal.cellsDropped, 0);
        // §25.1's step 3 ran over the walk's answer, and drew less than the whole house.
        EXPECT_GT(snapshot.chunksDrawn, 0);
        EXPECT_LT(snapshot.chunksDrawn, static_cast<int>(game.ResidentChunksForTesting()))
            << "the chunk cull kept every chunk in the house";
        EXPECT_LE(snapshot.chunksDrawn, snapshot.chunksTested);
        // §25.1 all the way through: the draw list IS the visible set's chunks (`HOUSE-00684`)
        // **plus §25.6's outdoors** (`HOUSE-00700`), which is a different structure answering a
        // different question -- portal traversal cannot help inside `EXT_WORLD`. The two sets
        // overlap wherever the walk did reach an exterior cell and the union subtracts that, so
        // the statement stays an equality rather than becoming an inequality nothing checks.
        EXPECT_TRUE(snapshot.cullingApplied);
        EXPECT_EQ(snapshot.drawCalls,
                  snapshot.chunksDrawn + static_cast<int>(game.ExteriorChunksAddedForTesting()))
            << "the draw list and the two culls disagree about what is being drawn";
        // ...and §25.6 ran at all, with its own three counters filled in rather than left at the
        // -1 that means "nothing did this".
        EXPECT_GT(snapshot.exteriorDrawn, 0) << "§25.6's hierarchy drew nothing from the road";
        EXPECT_GT(snapshot.exteriorNodes, 0);
        EXPECT_LE(snapshot.exteriorDrawn, snapshot.exteriorTested);
        // **The subtraction is doing something, and this is what says so.** The body starts on the
        // road looking at the house, so §25.2's walk reaches the front yards and already holds
        // some of their chunks; §25.6 finds those again through the hierarchy. Measured
        // 2026-09-10: 18 instances found, 16 added, so two were already in the list. A union that
        // did not subtract would add all 18 and draw two lawns twice -- and the equality above
        // cannot see that, because the count it compares against would grow by the same two.
        const int added = static_cast<int>(game.ExteriorChunksAddedForTesting());
        EXPECT_GT(added, 0) << "the outdoors contributed nothing at all to a frame on the road";
        EXPECT_LT(added, snapshot.exteriorDrawn)
            << "§25.6 found " << snapshot.exteriorDrawn << " instances and all " << added
            << " went into the list, so the walk's own chunks are being drawn a second time";
        // The visible sunroom contributes its real breakfast, bar, plant and fixture batches
        // through the open kitchen boundary. Measured 2026-09-23 after the authored leaf poses:
        // 115 of 620 calls remains a narrow visible-set result, with five calls of guard room.
        EXPECT_LT(snapshot.drawCalls, 120) << "the frame is still drawing most of the house";
    }

    TEST(HeadlessRunTests, PressingF4DrawsTheDecisionAndNotJustTheHouse)
    {
        // `HOUSE-00682`. §25.8's `F4`, end to end: the key reaches the overlay, the overlay is
        // built from the same walk `F3` reports, and what it holds is the DECISION -- the rooms
        // reached, their openings with their latch state, and a pyramid per cone.
        cnahouse::util::Log::ResetForTesting();

        OneKeyPress input(&cnahouse::player::InputState::toggleVisibilityGeometryPressed);

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        options.player = std::array<float, 5>{-3.00f, 0.60f, -25.05f, 90.0f, 0.0f};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        game.SetFrameLimit(30);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        const auto& overlay = game.VisibilityGeometryForTesting();
        EXPECT_TRUE(overlay.Visible()) << "one press of F4 did not show the overlay";
        std::printf("  %s\n", overlay.Line().c_str());
        EXPECT_GT(overlay.Segments().size(), 100U)
            << "the overlay is up and drew nothing: the walk reached rooms with walls";
        EXPECT_GT(overlay.Quads().size(), 0U) << "no portal quad was built";
        EXPECT_EQ(overlay.Dropped(), 0U) << "§12's house needed more segments than the cap";

        // It is built from the SAME walk `F3` reports, so the two agree about how many rooms
        // there are -- which is what makes reading them side by side worth anything.
        const cnahouse::debug::VisibilitySnapshot snapshot = game.VisibilitySnapshotForTesting();
        EXPECT_NE(overlay.Line().find(std::to_string(snapshot.visible.size()) + " cell(s)"),
                  std::string::npos)
            << overlay.Line() << " against F3's " << snapshot.visible.size() << " cells";
    }

    TEST(HeadlessRunTests, PressingF5FreezesTheWalkAndDetachesTheCamera)
    {
        // `HOUSE-00683`. §25.8: *"`F5` freezes the visibility computation so the camera can fly
        // out and inspect what was culled -- the single most useful debugging tool for a portal
        // system."* Both halves are checked: the walk stops being recomputed, and the camera stops
        // being the body's.
        cnahouse::util::Log::ResetForTesting();

        // Freezes on the first frame, then holds `W` for the rest of the run: with the walk frozen
        // the body must not move and the camera must.
        class FreezeThenFly final : public cnahouse::player::IInputSource
        {
        public:
            void Update(float) override
            {
                state_ = cnahouse::player::InputState{};
                state_.toggleFreezeVisibilityPressed = !fired_;
                fired_ = true;
                state_.move.Y = 1.0f;
            }

            [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
            {
                return state_;
            }

            [[nodiscard]] bool LookAvailable() const noexcept override
            {
                return false;
            }

        private:
            cnahouse::player::InputState state_;
            bool fired_ = false;
        };

        FreezeThenFly input;

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        // Clear circulation west of HOUSE-01040's island. The old (-3.00, -25.05) pose is now
        // correctly depenetrated from the measured island instead of standing in a bare kitchen.
        options.player = std::array<float, 5>{-5.50f, 0.60f, -24.50f, 90.0f, 0.0f};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        // Frames and not steps: §49.3's step does not run while frozen, so a step limit would
        // never be reached. Enough of them that the camera flies clear of the body at any frame
        // rate this build reaches.
        game.SetFrameLimit(400);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        EXPECT_TRUE(game.VisibilityFrozenForTesting()) << "one press of F5 did not freeze the walk";

        // The body stood still: `W` was held for sixty frames and §49.3's step never ran.
        const cnahouse::player::PlayerState& player = game.PlayerForTesting();
        EXPECT_NEAR(player.position.X, -5.50f, 0.05f) << "the body walked while the walk was frozen";
        EXPECT_NEAR(player.position.Z, -24.50f, 0.05f);
        EXPECT_EQ(game.FixedStepsForTesting(), 0U) << "§49.3's step ran during a freeze";

        // ...and the camera did not: the same `W` flew the inspection camera out of the room.
        const Microsoft::Xna::Framework::Vector3 eye = game.DrawCameraForTesting().eye;
        const Microsoft::Xna::Framework::Vector3 body = game.ViewForTesting().Camera().Pose().eye;
        const float moved =
            std::sqrt((eye.X - body.X) * (eye.X - body.X) + (eye.Y - body.Y) * (eye.Y - body.Y) +
                      (eye.Z - body.Z) * (eye.Z - body.Z));
        std::printf("  the inspection camera flew %.2f m from the body's eye\n", static_cast<double>(moved));
        EXPECT_GT(moved, 1.0f) << "the camera never detached from the body";

        // The world-space annotations are drawn through the camera the FRAME came from, which is
        // now the inspection camera. Through the body's, the frozen cones would be drawn 6 m from
        // where they are -- and a freeze that moved what it froze is worse than no freeze.
        const Microsoft::Xna::Framework::Matrix debugView = game.DebugViewForTesting();
        const Microsoft::Xna::Framework::Matrix drawView = game.DrawCameraForTesting().View();
        EXPECT_FLOAT_EQ(debugView.M41, drawView.M41) << "F4 is drawn through the wrong camera";
        EXPECT_FLOAT_EQ(debugView.M42, drawView.M42);
        EXPECT_FLOAT_EQ(debugView.M43, drawView.M43);
        const Microsoft::Xna::Framework::Matrix bodyView = game.ViewForTesting().Camera().View();
        // At least one of the three, not M41 in particular: a camera that flew straight down its
        // own view direction leaves the right-axis translation exactly where it was.
        EXPECT_TRUE(debugView.M41 != bodyView.M41 || debugView.M42 != bodyView.M42 ||
                    debugView.M43 != bodyView.M43)
            << "the body's camera and the frame's are the same, so nothing was proved";

        // The overlay says so, and says where the camera went.
        const cnahouse::debug::VisibilitySnapshot snapshot = game.VisibilitySnapshotForTesting();
        EXPECT_TRUE(snapshot.frozen);
        EXPECT_NEAR(snapshot.inspectionEye.X, eye.X, 1e-3f);
        // The walk it reports is still the one from the room, not from where the camera is now.
        EXPECT_EQ(snapshot.cell, "L0_KITCHEN");
        ASSERT_FALSE(snapshot.visible.empty());
        EXPECT_EQ(snapshot.visible.front().cell, "L0_KITCHEN");
        // And it STOPPED being recomputed. The body stands still while frozen, so a walk that kept
        // running would keep producing the same answer and look exactly like one that had stopped:
        // the frame it was computed for is the only thing that can tell the two apart.
        EXPECT_GT(game.FramesDrawn(), 100U);
        EXPECT_LT(snapshot.walkFrame, game.FramesDrawn() - 40U)
            << "the walk is still being recomputed every frame with F5 down";
        // Not `> 0`: the freeze lands in the FIRST frame's update, after that frame's walk, and
        // frame indices start at zero -- so the walk having run is what the non-empty visible set
        // above says, not what this number does.
    }

    TEST(HeadlessRunTests, FreezingDoesNotMoveTheViewItWasPressedToKeep)
    {
        // The detach ADOPTS the eye: a camera that started somewhere else would throw away the
        // view the reader pressed `F5` to keep, which is the one thing the freeze is for.
        cnahouse::util::Log::ResetForTesting();

        OneKeyPress input(&cnahouse::player::InputState::toggleFreezeVisibilityPressed);

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        options.player = std::array<float, 5>{-3.00f, 0.60f, -25.05f, 90.0f, 0.0f};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        game.SetFrameLimit(30);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);
        ASSERT_TRUE(game.VisibilityFrozenForTesting());

        // Nothing was held after the press, so the inspection camera has not flown anywhere: it is
        // still exactly where the body's eye is.
        const Microsoft::Xna::Framework::Vector3 eye = game.DrawCameraForTesting().eye;
        const Microsoft::Xna::Framework::Vector3 body = game.ViewForTesting().Camera().Pose().eye;
        EXPECT_NEAR(eye.X, body.X, 0.01f) << "the freeze moved the camera it was meant to hold";
        EXPECT_NEAR(eye.Y, body.Y, 0.01f);
        EXPECT_NEAR(eye.Z, body.Z, 0.01f);
    }

    /// Types a console line on a chosen frame and records the draw list either side of it.
    ///
    /// The only seam a test has for issuing a command MID-SESSION: the console is the game's, and
    /// `Run` does not return until the session is over. An input source is updated every frame, so
    /// it is where a scripted player's typing belongs.
    class ConsoleDriver final : public cnahouse::player::IInputSource
    {
    public:
        ConsoleDriver(CnaHouseGame& game, std::string line, int onFrame)
            : game_(&game)
            , line_(std::move(line))
            , onFrame_(onFrame)
        {
        }

        void Update(float) override
        {
            state_ = cnahouse::player::InputState{};
            ++frame_;
            if (frame_ == onFrame_)
            {
                result_ = game_->ConsoleForTesting().Execute(line_);
            }
            const int drawCalls = game_->VisibilitySnapshotForTesting().drawCalls;
            if (frame_ < onFrame_)
            {
                before_ = drawCalls;
            }
            else if (frame_ > onFrame_ + 1)
            {
                after_ = drawCalls;
            }
        }

        [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
        {
            return state_;
        }

        [[nodiscard]] bool LookAvailable() const noexcept override
        {
            return false;
        }

        [[nodiscard]] int Before() const noexcept
        {
            return before_;
        }

        [[nodiscard]] int After() const noexcept
        {
            return after_;
        }

        [[nodiscard]] const cnahouse::debug::CommandResult& Result() const noexcept
        {
            return result_;
        }

    private:
        CnaHouseGame* game_;
        std::string line_;
        int onFrame_;
        int frame_ = 0;
        int before_ = -1;
        int after_ = -1;
        cnahouse::player::InputState state_;
        cnahouse::debug::CommandResult result_;
    };

    TEST(HeadlessRunTests, ReviewOverridesDriveTheSharedClockWeatherAndLighting)
    {
        // Visual-review captures only compare like with like if their command-line inputs reach
        // the simulation. These options existed before HOUSE-01264 but were not consumed by the
        // walk scene, leaving every supposedly 10:30 clear capture at the authored 07:00 partly
        // cloudy start. Prove the complete route rather than trusting the HUD pixels.
        cnahouse::util::Log::ResetForTesting();
        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        options.timeOfDay = 10.5F;
        options.freezeTime = true;
        options.weather = "W_CLEAR";
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(4);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        const cnahouse::environment::CivilTime wall = game.ClockForTesting().Wall();
        EXPECT_EQ(wall.hour, 10);
        EXPECT_EQ(wall.minute, 30);
        EXPECT_DOUBLE_EQ(game.ClockForTesting().timeScale, 0.0);
        const cnahouse::weather::WeatherSystem* weather = game.WeatherForTesting();
        ASSERT_NE(weather, nullptr);
        EXPECT_EQ(cnahouse::util::IdRegistry::NameOf(weather->TargetArchetype()), "W_CLEAR");
        EXPECT_FALSE(weather->AutomaticTransitions());
        EXPECT_TRUE(weather->TransitionsPaused());
        EXPECT_GE(weather->State().cloudCover, 0.0F);
        EXPECT_LE(weather->State().cloudCover, 0.1F);
        const cnahouse::lighting::LightingSystem* lighting = game.LightingForTesting();
        ASSERT_NE(lighting, nullptr);
        EXPECT_FLOAT_EQ(lighting->CloudCover(), weather->State().cloudCover)
            << "the fixed review weather did not drive the daylight model";
    }

    TEST(HeadlessRunTests, CullOffDrawsTheWholeHouseAndCullOnDrawsTheVisibleSet)
    {
        // `HOUSE-00684`, end to end and through the console the way a person would type it. The
        // two numbers either side of the command are what phase 9 was for -- and `cull off` is
        // what `HOUSE-00688` renders its second frame with.
        cnahouse::util::Log::ResetForTesting();

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        options.player = std::array<float, 5>{-3.00f, 0.60f, -25.05f, 90.0f, 0.0f};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        // One `Game` at a time, in a scope of its own: CNA's device manager is a process-wide
        // thing and two live `CnaHouseGame`s crash before either of them draws.
        int before = 0;
        int after = 0;
        int resident = 0;
        bool appliedAtEnd = true;
        {
            CnaHouseGame game(options, settings);
            ConsoleDriver driver(game, "cull off", 10);
            game.SetInputSourceForTesting(&driver);
            game.SetFrameLimit(20);
            game.Run();
            ASSERT_EQ(game.ExitCode(), 0);
            ASSERT_TRUE(driver.Result().ok) << driver.Result().message;
            before = driver.Before();
            after = driver.After();
            // The house's own chunk count, ASKED rather than written down (`HOUSE-00485`): the
            // shell is generated, so a spelled-out number is one the next person to change a wall
            // has to edit, and a number people edit is a number nobody reads.
            resident = static_cast<int>(game.ResidentChunksForTesting());
            appliedAtEnd = game.VisibilitySnapshotForTesting().cullingApplied;
        }

        std::printf("  §12's house from L0_KITCHEN: %d draw call(s) with culling on, %d with "
                    "`cull off` (%d chunks resident)\n",
                    before,
                    after,
                    resident);

        ASSERT_GT(resident, 100) << "the house did not load";
        // On by default: §25 exists to be used.
        EXPECT_GT(before, 0);
        EXPECT_LT(before, resident / 4) << "culling on is barely culling anything";
        // ...and off means everything resident, which is the frame `HOUSE-00688` compares against.
        EXPECT_EQ(after, resident) << "`cull off` did not restore the whole house";
        EXPECT_FALSE(appliedAtEnd) << "the overlay still claims the frame was culled";

        // And `cull on` in a session that is already on changes nothing, which is what makes the
        // command a setting rather than a toggle.
        {
            CnaHouseGame again(options, settings);
            ConsoleDriver back(again, "cull on", 10);
            again.SetInputSourceForTesting(&back);
            again.SetFrameLimit(20);
            again.Run();
            ASSERT_TRUE(back.Result().ok) << back.Result().message;
            EXPECT_EQ(back.After(), back.Before()) << "`cull on` changed a frame already culled";
        }
    }

    TEST(HeadlessRunTests, PressingF9BuildsTheWireframeForTheCellTheBodyIsIn)
    {
        // `HOUSE-00620` recorded §71's `F9` as unwired because *"`DebugDraw::Begin` needs a view
        // and a projection and the loop has neither a camera nor a loaded `CollisionWorld` until
        // phase 8"*. `--scene=walk` has both, so this is the phase-8 review closing it: the key
        // reaches the overlay and the overlay builds the cell it is standing in.
        cnahouse::util::Log::ResetForTesting();

        OneKeyPress input(&cnahouse::player::InputState::togglePhysicsOverlayPressed);

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        options.player = std::array<float, 5>{-3.00f, 0.60f, -25.05f, 90.0f, 0.0f};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        game.SetFrameLimit(30);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        const auto& overlay = game.PhysicsOverlayForTesting();
        EXPECT_TRUE(overlay.Visible()) << "one press of F9 did not show the overlay";
        EXPECT_GT(overlay.Segments().size(), 100U)
            << "the overlay is visible and drew nothing: §12's kitchen has walls";
        EXPECT_EQ(overlay.Dropped(), 0U) << "the cell needed more segments than the overlay's cap";
        // §49.3's capsule is in there: the overlay draws the body it annotates.
        EXPECT_GT(overlay.Lines().size(), 2U);
    }

    TEST(HeadlessRunTests, TheWalkSceneStandsABodyInTheHouseAndLooksThroughItsEyes)
    {
        // `HOUSE-00633`'s wiring, end to end and with no window: §16's world and §49.2's collision
        // load, a body settles onto §12's floor, §49.3's fixed step runs from the frame time, and
        // §44's camera ends up where the eye is. The render poses assert what the frame LOOKS
        // like; this asserts that the thing behind it is a simulation and not a fixed camera.
        cnahouse::util::Log::ResetForTesting();

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        // §12's kitchen, on the floor, looking east.
        options.player = std::array<float, 5>{-3.00f, 0.60f, -25.05f, 90.0f, 0.0f};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(120);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        const auto& player = game.PlayerForTesting();
        EXPECT_EQ(cnahouse::util::IdRegistry::NameOf(game.CellForTesting()), "L0_KITCHEN")
            << "the body did not end up in the cell it was put in";
        EXPECT_TRUE(player.onGround) << "the body never landed on §12's floor";
        // §12's L0 finished floor level is +0.60, and a body standing on it has its feet there.
        EXPECT_NEAR(player.Feet().Y, 0.60f, 0.02f);
        EXPECT_NEAR(player.Feet().X, -3.00f, 0.05f) << "nothing pushed the body sideways";

        // §44's eye: 1.68 m over the soles, and the camera at exactly that point.
        const auto& camera = game.ViewForTesting().Camera();
        EXPECT_NEAR(camera.Pose().eye.Y, player.Feet().Y + cnahouse::player::kPlayerEyeHeight, 0.01f);
        EXPECT_NEAR(camera.Pose().eye.X, player.position.X, 1e-3f);
        // §14: yaw 90° looks east, so forward is +X.
        EXPECT_NEAR(camera.Pose().forward.X, 1.0f, 1e-3f);
        EXPECT_NEAR(camera.Pose().forward.Z, 0.0f, 1e-3f);
        // §44's lens and §10.3's near plane, which is what makes this a player camera rather than
        // the blockout's 55° one.
        EXPECT_FLOAT_EQ(camera.EffectiveFieldOfViewDegrees(), cnahouse::player::kDefaultFovDegrees);
        EXPECT_LE(camera.NearPlane(), cnahouse::player::kNearPlane);
    }

    /// Holds one intent for the whole session. `HOUSE-00140`'s interface is what makes this
    /// possible: everything after it is expressed in game terms, so a test can walk the body.
    class ScriptedInput final : public cnahouse::player::IInputSource
    {
    public:
        explicit ScriptedInput(cnahouse::player::InputState state, bool lookAvailable = false)
            : state_(state)
            , lookAvailable_(lookAvailable)
        {
        }

        void Update(float) override {}

        [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
        {
            return state_;
        }

        [[nodiscard]] bool LookAvailable() const noexcept override
        {
            return lookAvailable_;
        }

    private:
        cnahouse::player::InputState state_;
        bool lookAvailable_ = false;
    };

    TEST(HeadlessRunTests, CommandLineScreenshotKeepsItsAuthoredWalkPose)
    {
        const std::filesystem::path output =
            std::filesystem::path(CNAHOUSE_TEST_OUTPUT_DIR) / "fixed-walk-pose.png";
        std::filesystem::remove(output);
        cnahouse::player::InputState movingLook;
        movingLook.move.Y = 1.0F;
        movingLook.look.X = 0.2F;
        movingLook.look.Y = 0.2F;
        ScriptedInput input(movingLook, true);
        Options options;
        options.headless = true;
        options.scene = "walk";
        options.player = std::array{0.0F, 0.6F, -16.3F, 90.0F, 0.0F};
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.screenshot = output.string();
        options.screenshotFrame = 8;
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        const std::filesystem::path original = std::filesystem::current_path();
        std::filesystem::current_path(std::filesystem::path(CNAHOUSE_TEST_CONTENT_ROOT).parent_path());
        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        game.Run();
        std::filesystem::current_path(original);
        ASSERT_EQ(game.ExitCode(), 0);
        EXPECT_TRUE(std::filesystem::exists(output));
        EXPECT_NEAR(game.PlayerForTesting().Feet().X, 0.0F, 0.01F);
        EXPECT_NEAR(game.PlayerForTesting().Feet().Z, -16.3F, 0.01F);
        EXPECT_NEAR(game.ViewForTesting().Camera().Pose().forward.X, 1.0F, 0.01F);
        EXPECT_NEAR(game.ViewForTesting().Camera().Pose().forward.Y, 0.0F, 0.01F);
    }

    /// Actual renderer diagnostic for HOUSE-03637. Keep the production walk, visibility and
    /// screenshot paths, but neutralise desktop mouse warps so each captured frame differs only
    /// by the controller's forward movement. Opt-in because it performs repeated GPU readbacks;
    /// run with SDL_VIDEODRIVER=offscreen and no desktop display.
    class ThresholdCaptureInput final : public cnahouse::player::IInputSource
    {
    public:
        explicit ThresholdCaptureInput(const CnaHouseGame& game,
                                       std::array<float, 2> move = {0.0F, 1.0F},
                                       unsigned int captureFrames = 36)
            : game_(&game)
            , move_(move)
            , captureFrames_(captureFrames)
        {
        }

        void Update(float deltaSeconds) override
        {
            ++updates_;
            // The snapshot is from the frame just drawn. A zero-draw frame in this movement
            // interval was the original sky flash, even though a later static pose rendered.
            if (updates_ > 2 && updates_ <= captureFrames_ + 1)
            {
                const int draws = game_->VisibilitySnapshotForTesting().drawCalls;
                minDraws_ = std::min(minDraws_, draws);
                maxDraws_ = std::max(maxDraws_, draws);
            }
            state_ = {};
            // Loading and PNG readback can make a frame much longer than a normal game frame.
            // Bound the requested travel per captured frame so the camera does not jump completely
            // through the doorway between the two images we need to compare.
            if (updates_ > 1 && deltaSeconds > 0.0F)
            {
                const float amount = std::min(1.0F, 0.025F / (1.35F * deltaSeconds));
                state_.move.X = move_[0] * amount;
                state_.move.Y = move_[1] * amount;
            }
            state_.toggleVisibilityOverlayPressed = updates_ == 1;
            state_.screenshotPressed = updates_ <= captureFrames_;
        }

        [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
        {
            return state_;
        }

        [[nodiscard]] bool LookAvailable() const noexcept override
        {
            return false;
        }

        [[nodiscard]] int MinDraws() const noexcept
        {
            return minDraws_;
        }

        [[nodiscard]] int MaxDraws() const noexcept
        {
            return maxDraws_;
        }

    private:
        const CnaHouseGame* game_;
        std::array<float, 2> move_;
        unsigned int captureFrames_;
        unsigned int updates_ = 0;
        int minDraws_ = std::numeric_limits<int>::max();
        int maxDraws_ = 0;
        cnahouse::player::InputState state_;
    };

    TEST(HeadlessRunTests, GpuThresholdMovementCaptures)
    {
        if (std::getenv("HOUSE_THRESHOLD_GPU_CAPTURE") == nullptr)
        {
            GTEST_SKIP() << "GPU doorway frames are an opt-in visual review";
        }
        const char* videoDriver = std::getenv("SDL_VIDEODRIVER");
        if (videoDriver == nullptr || std::string_view(videoDriver) != "offscreen" ||
            std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr)
        {
            GTEST_SKIP() << "GPU doorway capture requires offscreen SDL and no desktop display";
        }

        struct Crossing
        {
            const char* name;
            std::array<float, 5> start;
            const char* arrival;
            std::array<float, 2> move{0.0F, 1.0F};
            unsigned int captureFrames = 36;
        };

        const std::array crossings{
            Crossing{"foyer-stair", {2.04F, 0.60F, -14.80F, 90.0F, 0.0F}, "L0_STAIR_MAIN"},
            Crossing{"stair-foyer", {2.35F, 0.60F, -14.80F, 270.0F, 0.0F}, "L0_FOYER"},
            // Owner's exact front-wall corner: the old portal captures faced
            // straight through the passage and missed the perpendicular seam.
            Crossing{
                "wall-foyer-corner-left", {0.80F, 0.60F, -16.50F, 145.0F, -2.0F}, "L0_FOYER", {-1.0F, 0.0F}},
            Crossing{
                "wall-foyer-corner-right", {0.80F, 0.60F, -16.50F, 145.0F, -2.0F}, "L0_FOYER", {1.0F, 0.0F}},
            Crossing{"wall-stair-corner-left",
                     {3.90F, 0.60F, -15.00F, 235.0F, -2.0F},
                     "L0_STAIR_MAIN",
                     {-1.0F, 0.0F}},
            Crossing{"wall-stair-corner-right",
                     {4.30F, 0.60F, -15.00F, 235.0F, -2.0F},
                     "L0_STAIR_MAIN",
                     {1.0F, 0.0F}},
            Crossing{"hall-wc1", {2.04F, 0.60F, -21.10F, 90.0F, 0.0F}, "L0_WC1"},
            Crossing{"wc1-hall", {2.37F, 0.60F, -21.10F, 270.0F, 0.0F}, "L0_HALL"},
            Crossing{"wc3-hall", {-5.70F, 3.65F, -20.84F, 180.0F, 0.0F}, "L1_HALL_W"},
            Crossing{"hall-wc3", {-5.70F, 3.65F, -20.48F, 0.0F, 0.0F}, "L1_WC3"},
            Crossing{"wc7-hall", {2.32F, -2.30F, -22.10F, 270.0F, 0.0F}, "B1_HALL"},
            Crossing{"hall-wc7", {2.03F, -2.30F, -22.10F, 90.0F, 0.0F}, "B1_WC7"},
            Crossing{"porch-foyer", {0.00F, 0.57F, -14.10F, 0.0F, 0.0F}, "L0_FOYER"},
            Crossing{"foyer-porch", {0.00F, 0.60F, -14.48F, 180.0F, 0.0F}, "L0_PORCH"},
            // The Juliet is facade-only and its door is authored shut. Review the moving view
            // from that inspection pose, but do not claim a new accessible balcony/door route.
            Crossing{"juliet-inward-view", {-0.55F, 6.55F, -14.10F, 0.0F, 0.0F}, "L2_BALCONY_JULIET"},
            Crossing{"front-balcony-landing", {0.10F, 3.65F, -14.10F, 0.0F, 0.0F}, "L1_LANDING"},
            Crossing{"landing-front-balcony", {0.10F, 3.65F, -14.50F, 180.0F, 0.0F}, "L1_BALCONY_FRONT"},
            // Use the slider's open half, not its fixed glass/stile, and put the threshold in
            // the short frame-capture interval. This is a view-transition test, not a tour route.
            Crossing{"rear-balcony-bedroom", {-1.65F, 3.65F, -27.35F, 180.0F, 0.0F}, "L1_MASTER_BED"},
            Crossing{"bedroom-rear-balcony", {-1.65F, 3.65F, -26.90F, 0.0F, 0.0F}, "L1_BALCONY_REAR"},
            Crossing{"garage-road-west", {0.00F, 0.00F, 3.00F, 39.0F, 0.0F}, "EXT_ROAD"},
            Crossing{"garage-road-east", {22.00F, 0.00F, 3.00F, 335.5F, 0.0F}, "EXT_ROAD"},
            // Move while looking at each transition, in both directions. For joins within a
            // capsule radius of the room edge, approach/retreat instead of an impossible strafe.
            // These stay in the room; positive draw counts alone cannot detect a narrow sky slit.
            Crossing{"wall-laundry-approach", {7.60F, 0.60F, -21.60F, 90.0F, 0.0F}, "L0_LAUNDRY"},
            Crossing{
                "wall-laundry-retreat", {7.90F, 0.60F, -21.60F, 90.0F, 0.0F}, "L0_LAUNDRY", {0.0F, -1.0F}},
            Crossing{"wall-bath3-approach", {7.30F, 3.65F, -21.60F, 90.0F, 0.0F}, "L1_BATH3"},
            Crossing{"wall-bath3-retreat", {7.30F, 3.65F, -21.60F, 90.0F, 0.0F}, "L1_BATH3", {0.0F, -1.0F}},
            Crossing{"wall-utility-left", {7.30F, -2.30F, -21.55F, 90.0F, 0.0F}, "B1_UTILITY", {-1.0F, 0.0F}},
            Crossing{"wall-utility-right", {7.30F, -2.30F, -21.85F, 90.0F, 0.0F}, "B1_UTILITY", {1.0F, 0.0F}},
            // The cupboard's 1.75 m head forces the normal controller to crouch. Capture longer,
            // rather than increasing its speed or lowering the actual-motion assertion.
            Crossing{"wall-understair-approach",
                     {4.48F, -2.30F, -21.10F, 270.0F, 0.0F},
                     "B1_UNDERSTAIR",
                     {0.0F, 1.0F},
                     72},
            Crossing{"wall-understair-retreat",
                     {4.20F, -2.30F, -21.10F, 270.0F, 0.0F},
                     "B1_UNDERSTAIR",
                     {0.0F, -1.0F},
                     72},
            Crossing{"wall-kitchen-left", {-6.55F, 0.60F, -25.80F, 0.0F, 0.0F}, "L0_KITCHEN", {-1.0F, 0.0F}},
            Crossing{"wall-kitchen-right", {-6.85F, 0.60F, -25.80F, 0.0F, 0.0F}, "L0_KITCHEN", {1.0F, 0.0F}},
            Crossing{"wall-family-left", {2.85F, 0.60F, -25.80F, 0.0F, 0.0F}, "L0_FAMILY", {-1.0F, 0.0F}},
            Crossing{"wall-family-right", {2.55F, 0.60F, -25.80F, 0.0F, 0.0F}, "L0_FAMILY", {1.0F, 0.0F}},
            Crossing{
                "wall-attic-west-left", {-3.75F, 9.30F, -18.70F, 180.0F, 0.0F}, "L3_ROOM", {-1.0F, 0.0F}},
            Crossing{
                "wall-attic-west-right", {-3.45F, 9.30F, -18.70F, 180.0F, 0.0F}, "L3_ROOM", {1.0F, 0.0F}},
            Crossing{"wall-attic-east-left", {3.45F, 9.30F, -18.70F, 180.0F, 0.0F}, "L3_ROOM", {-1.0F, 0.0F}},
            Crossing{"wall-attic-east-right", {3.75F, 9.30F, -18.70F, 180.0F, 0.0F}, "L3_ROOM", {1.0F, 0.0F}},
        };
        const char* requestedCase = std::getenv("HOUSE_THRESHOLD_CASE");
        const std::filesystem::path original = std::filesystem::current_path();
        const std::filesystem::path buildRoot =
            std::filesystem::path(CNAHOUSE_TEST_CONTENT_ROOT).parent_path();
        for (const float time : {10.5F, 23.0F})
        {
            for (const Crossing& crossing : crossings)
            {
                if (requestedCase != nullptr && std::string_view(requestedCase) != crossing.name)
                {
                    continue;
                }
                const std::filesystem::path output =
                    std::filesystem::path(CNAHOUSE_TEST_OUTPUT_DIR) / "threshold-movement" /
                    (std::string(crossing.name) + (time < 12.0F ? "-day" : "-night"));
                std::filesystem::create_directories(output);
                std::vector<std::filesystem::path> existing;
                for (const auto& entry : std::filesystem::directory_iterator(buildRoot))
                {
                    if (entry.path().filename().string().starts_with("cna-house-") &&
                        entry.path().extension() == ".png")
                    {
                        existing.push_back(entry.path());
                    }
                }
                // WorldLoader reads deployed files relative to the build root, not contentRoot.
                std::filesystem::current_path(buildRoot);
                cnahouse::util::Log::ResetForTesting();
                Options options;
                options.scene = "walk";
                options.player = crossing.start;
                options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
                options.noAudio = true;
                options.timeOfDay = time;
                options.freezeTime = true;
                Settings settings = Settings::Defaults();
                settings.backBufferWidth = 960;
                settings.backBufferHeight = 540;
                settings.verticalSync = true;
                CnaHouseGame game(options, settings);
                ThresholdCaptureInput input(game, crossing.move, crossing.captureFrames);
                game.SetInputSourceForTesting(&input);
                game.SetFrameLimit(crossing.captureFrames + 4);
                game.Run();
                std::filesystem::current_path(original);
                std::size_t captured = 0;
                for (const auto& entry : std::filesystem::directory_iterator(buildRoot))
                {
                    if (entry.path().filename().string().starts_with("cna-house-") &&
                        entry.path().extension() == ".png" &&
                        std::ranges::find(existing, entry.path()) == existing.end())
                    {
                        std::filesystem::rename(entry.path(), output / entry.path().filename());
                        ++captured;
                    }
                }
                ASSERT_EQ(game.ExitCode(), 0) << crossing.name;
                EXPECT_GE(captured, 3U) << crossing.name << " did not retain a movement sequence";
                EXPECT_GT(input.MinDraws(), 0) << crossing.name << " exposed the sky during traversal";
                std::printf(
                    "  %s at %.1f: %zu frames, draws %d..%d, x %.3f z %.3f, cell %.*s\n",
                    crossing.name,
                    static_cast<double>(time),
                    captured,
                    input.MinDraws(),
                    input.MaxDraws(),
                    static_cast<double>(game.PlayerForTesting().Feet().X),
                    static_cast<double>(game.PlayerForTesting().Feet().Z),
                    static_cast<int>(cnahouse::util::IdRegistry::NameOf(game.CellForTesting()).size()),
                    cnahouse::util::IdRegistry::NameOf(game.CellForTesting()).data());
                EXPECT_EQ(cnahouse::util::IdRegistry::NameOf(game.CellForTesting()), crossing.arrival)
                    << crossing.name;
                if (std::string_view(crossing.name).starts_with("wall-"))
                {
                    const auto feet = game.PlayerForTesting().Feet();
                    const float dx = feet.X - crossing.start[0];
                    const float dz = feet.Z - crossing.start[2];
                    const float yaw = crossing.start[3] * Microsoft::Xna::Framework::MathHelper::Pi / 180.0F;
                    const float moveX = crossing.move[0] * std::cos(yaw) + crossing.move[1] * std::sin(yaw);
                    const float moveZ = crossing.move[0] * std::sin(yaw) - crossing.move[1] * std::cos(yaw);
                    EXPECT_GT(dx * moveX + dz * moveZ, 0.20F)
                        << crossing.name << " must actually move in the requested review direction";
                }
            }
        }
    }

    // A review-only replay of the collision-resolved GrandTour, through the actual game
    // controller and renderer. No body/camera teleport, runtime tour feature or new manager.
    // Static lighting samples miss a wrong view direction and state changes while moving.
    TEST(HeadlessRunTests, GpuWholeHouseMovingLightingReview)
    {
        const char* tracePath = std::getenv("HOUSE_LIGHTING_WALK_TRACE");
        if (tracePath == nullptr)
        {
            GTEST_SKIP() << "moving whole-house GPU review requires a GrandTour trace";
        }
        const char* driver = std::getenv("SDL_VIDEODRIVER");
        ASSERT_TRUE(driver != nullptr && std::string_view(driver) == "offscreen");
        ASSERT_EQ(std::getenv("DISPLAY"), nullptr) << "never open the owner's monitor";
        ASSERT_EQ(std::getenv("WAYLAND_DISPLAY"), nullptr);

        struct Sample
        {
            Microsoft::Xna::Framework::Vector3 feet;
            std::string cell;
            bool arrival = false;
            bool crouched = false;
        };

        std::ifstream trace(tracePath);
        ASSERT_TRUE(trace.is_open()) << tracePath;
        std::vector<Sample> samples;
        Sample sample;
        while (trace >> sample.feet.X >> sample.feet.Y >> sample.feet.Z >> sample.cell >> sample.arrival >>
               sample.crouched)
        {
            samples.push_back(sample);
        }
        ASSERT_TRUE(trace.eof()) << "malformed walk trace";
        ASSERT_GT(samples.size(), 90U);
        std::size_t end = samples.size();
        if (const char* to = std::getenv("HOUSE_LIGHTING_WALK_TO"))
        {
            end = std::stoull(to);
            ASSERT_LE(end, samples.size());
            ASSERT_GT(end, 0U);
            samples.resize(end);
        }
        std::size_t first = 0U;
        if (const char* from = std::getenv("HOUSE_LIGHTING_WALK_FROM"))
        {
            first = std::stoull(from);
            ASSERT_LT(first, samples.size());
            samples.erase(samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(first));
        }
        std::set<std::string> expectedArrivals;
        for (const Sample& point : samples)
        {
            if (point.arrival)
            {
                expectedArrivals.insert(point.cell);
            }
        }
        const std::filesystem::path original = std::filesystem::current_path();
        const std::filesystem::path buildRoot =
            std::filesystem::path(CNAHOUSE_TEST_CONTENT_ROOT).parent_path();
        std::filesystem::current_path(buildRoot);
        const char* requestedTime = std::getenv("HOUSE_LIGHTING_WALK_TIME");
        const float time = requestedTime == nullptr ? 10.5F : std::stof(requestedTime);
        const std::filesystem::path output =
            std::filesystem::path(CNAHOUSE_TEST_OUTPUT_DIR) /
            ((time < 12.0F ? std::string("lighting-walk-day") : std::string("lighting-walk-night")) +
             (first == 0U ? "" : "-from" + std::to_string(first)) +
             (std::getenv("HOUSE_LIGHTING_WALK_TO") == nullptr ? "" : "-to" + std::to_string(end)));
        std::filesystem::create_directories(output);

        class WalkReviewInput final : public cnahouse::player::IInputSource
        {
        public:
            WalkReviewInput(CnaHouseGame& game, const std::vector<Sample>& samples)
                : game_(game)
                , samples_(samples)
            {
            }

            void Update(float) override
            {
                state_ = {};
                const auto& body = game_.PlayerForTesting();
                const auto feet = body.Feet();
                const std::uint64_t steps = game_.FixedStepsForTesting();
                const std::string cell(cnahouse::util::IdRegistry::NameOf(game_.CellForTesting()));
                if (cell != observedCell_ && !cell.empty())
                {
                    observedCell_ = cell;
                    entryFrames_ = 4U;
                }
                if (entryFrames_ > 0U)
                {
                    state_.screenshotPressed = true;
                    captures_.push_back(cell + "-entry" + std::to_string(4U - entryFrames_--));
                }
                if (index_ >= samples_.size())
                {
                    game_.Exit();
                    return;
                }
                if (steps - progressAt_ > 3600U)
                {
                    const auto target = samples_[index_].feet;
                    std::printf(
                        "  stuck sample %zu: feet (%.6f, %.6f, %.6f) toward (%.6f, %.6f, %.6f), crouch %d\n",
                        index_,
                        static_cast<double>(feet.X),
                        static_cast<double>(feet.Y),
                        static_cast<double>(feet.Z),
                        static_cast<double>(target.X),
                        static_cast<double>(target.Y),
                        static_cast<double>(target.Z),
                        body.crouched);
                    stuck_ = true;
                    game_.Exit();
                    return;
                }
                constexpr float pi = 3.14159265F;
                if (reviewing_)
                {
                    const std::uint64_t elapsed = steps - reviewAt_;
                    const float yaw =
                        elapsed <= 120U
                            ? 0.0F
                            : 2.0F * pi * static_cast<float>(std::min<std::uint64_t>(elapsed - 120U, 480U)) /
                                  480.0F;
                    state_.look.X = std::remainder(yaw - body.yaw, 2.0F * pi);
                    state_.crouch = samples_[index_].crouched;
                    if (elapsed >= 180U + 120U * reviewShot_ && reviewShot_ < 4U && !state_.screenshotPressed)
                    {
                        state_.screenshotPressed = true;
                        captures_.push_back(samples_[index_].cell + "-view" + std::to_string(reviewShot_++));
                    }
                    if (elapsed >= 600U)
                    {
                        reviewing_ = false;
                        ++index_;
                        progressAt_ = steps;
                    }
                    return;
                }
                while (index_ < samples_.size())
                {
                    const Sample& target = samples_[index_];
                    const float dx = target.feet.X - feet.X;
                    const float dz = target.feet.Z - feet.Z;
                    const float distance = std::hypot(dx, dz);
                    if (distance < 0.10F && std::fabs(target.feet.Y - feet.Y) < 0.16F)
                    {
                        progressAt_ = steps;
                        if (target.arrival)
                        {
                            reviewing_ = true;
                            reviewAt_ = steps;
                            reviewShot_ = 0U;
                            arrived_.insert(
                                std::string(cnahouse::util::IdRegistry::NameOf(game_.CellForTesting())));
                            std::printf("  lighting review %zu: %s at (%.3f, %.3f, %.3f)\n",
                                        arrived_.size(),
                                        target.cell.c_str(),
                                        static_cast<double>(feet.X),
                                        static_cast<double>(feet.Y),
                                        static_cast<double>(feet.Z));
                            std::fflush(stdout);
                            break;
                        }
                        ++index_;
                        continue;
                    }
                    state_.look.X = std::remainder(-body.yaw, 2.0F * pi);
                    state_.crouch = target.crouched;
                    if (distance > 0.001F)
                    {
                        // The trace was generated in yaw-zero world steering. Keep that input
                        // convention while walking; four inward/outward pans inspect every room.
                        state_.move = {dx / distance, -dz / distance};
                    }
                    break;
                }
            }

            [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
            {
                return state_;
            }

            [[nodiscard]] bool LookAvailable() const noexcept override
            {
                return true;
            }

            CnaHouseGame& game_;
            const std::vector<Sample>& samples_;
            cnahouse::player::InputState state_;
            std::size_t index_ = 0;
            std::uint64_t progressAt_ = 0;
            std::uint64_t reviewAt_ = 0;
            unsigned int reviewShot_ = 0;
            bool reviewing_ = false;
            bool stuck_ = false;
            std::set<std::string> arrived_;
            std::vector<std::string> captures_;
            std::string observedCell_;
            unsigned int entryFrames_ = 0U;
        };

        std::set<std::filesystem::path> before;
        for (const auto& file : std::filesystem::directory_iterator(buildRoot))
        {
            if (file.path().filename().string().starts_with("cna-house-") &&
                file.path().extension() == ".png")
            {
                before.insert(file.path());
            }
        }
        cnahouse::util::Log::ResetForTesting();
        Options options;
        options.scene = "walk";
        options.player = {samples.front().feet.X, samples.front().feet.Y, samples.front().feet.Z, 0.0F, 0.0F};
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.timeOfDay = time;
        options.freezeTime = true;
        options.weather = "W_CLEAR";
        Settings settings = Settings::Defaults();
        settings.fastWalk = true;
        settings.backBufferWidth = 960;
        settings.backBufferHeight = 540;
        settings.verticalSync = false;
        CnaHouseGame game(options, settings);
        WalkReviewInput input(game, samples);
        game.SetInputSourceForTesting(&input);
        game.SetReviewFrameStepForTesting(true);
        game.SetFixedStepLimit(200000U);
        game.SetFrameLimit(300000U);
        game.Run();
        std::vector<std::filesystem::path> captures;
        for (const auto& file : std::filesystem::directory_iterator(buildRoot))
        {
            if (file.path().filename().string().starts_with("cna-house-") &&
                file.path().extension() == ".png" && !before.contains(file.path()))
            {
                captures.push_back(file.path());
            }
        }
        std::sort(captures.begin(), captures.end());
        for (std::size_t i = 0; i < std::min(captures.size(), input.captures_.size()); ++i)
        {
            std::filesystem::rename(captures[i],
                                    output / (std::to_string(i) + "-" + input.captures_[i] + ".png"));
        }
        std::filesystem::current_path(original);
        EXPECT_EQ(game.ExitCode(), 0);
        EXPECT_FALSE(input.stuck_) << "trace sample " << input.index_ << " in "
                                   << cnahouse::util::IdRegistry::NameOf(game.CellForTesting());
        EXPECT_EQ(input.index_, samples.size());
        EXPECT_EQ(input.arrived_, expectedArrivals);
        EXPECT_EQ(captures.size(), input.captures_.size());
    }

    TEST(HeadlessRunTests, PowderRoomDoorwayIsTraversableInBothDirections)
    {
        struct WalkResult
        {
            std::string cell;
            Microsoft::Xna::Framework::Vector3 feet;
            int exitCode = 0;
        };

        const auto walk = [](const std::array<float, 5>& start)
        {
            cnahouse::player::InputState forward;
            forward.move.Y = 1.0F;
            ScriptedInput input(forward);
            Options options;
            options.headless = true;
            options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
            options.noAudio = true;
            options.scene = "walk";
            options.player = start;
            Settings settings = Settings::Defaults();
            settings.backBufferWidth = 320;
            settings.backBufferHeight = 180;
            settings.verticalSync = false;
            CnaHouseGame game(options, settings);
            game.SetInputSourceForTesting(&input);
            game.SetFixedStepLimit(240);
            game.SetFrameLimit(4000);
            game.Run();
            return WalkResult{std::string(cnahouse::util::IdRegistry::NameOf(game.CellForTesting())),
                              game.PlayerForTesting().Feet(),
                              game.ExitCode()};
        };

        cnahouse::util::Log::ResetForTesting();
        const auto out = walk({3.55F, 0.60F, -21.10F, 270.0F, 0.0F});
        std::printf("  powder-room exit ended in %s at x %.3f z %.3f\n",
                    out.cell.c_str(),
                    static_cast<double>(out.feet.X),
                    static_cast<double>(out.feet.Z));
        EXPECT_EQ(out.exitCode, 0);
        EXPECT_EQ(out.cell, "L0_HALL");
        EXPECT_LT(out.feet.X, 2.0F);

        const auto in = walk({1.00F, 0.60F, -21.10F, 90.0F, 0.0F});
        std::printf("  powder-room entry ended in %s at x %.3f z %.3f\n",
                    in.cell.c_str(),
                    static_cast<double>(in.feet.X),
                    static_cast<double>(in.feet.Z));
        EXPECT_EQ(in.exitCode, 0);
        EXPECT_EQ(in.cell, "L0_WC1");
        EXPECT_GT(in.feet.X, 2.40F);
    }

    TEST(HeadlessRunTests, TheRealWalkControllerReachesL1FromTheFoyer)
    {
        // The unit stair test changes collision cells by hand between legs. This starts at the
        // foyer in the running game, so its actual tracker, input path and camera must all survive
        // the opening and the entire first flight without a test-supplied cell transition.
        const std::string collisionPath = "content/world/collision.bin";
        if (!std::filesystem::exists(collisionPath))
        {
            GTEST_SKIP() << "no deployed collision; run tools/ci/build_content.py --only world";
        }
        const std::unique_ptr<System::IO::FileStream> stream(new System::IO::FileStream(
            collisionPath, System::IO::FileMode::Open, System::IO::FileAccess::Read));
        const auto loaded = cnahouse::physics::CollisionLoader::Read(*stream, collisionPath);
        ASSERT_TRUE(loaded) << loaded.Error().Message();
        const cnahouse::physics::CollisionCell* stair = loaded.Value().Cell("L0_STAIR_MAIN");
        ASSERT_NE(stair, nullptr);
        const auto segments = cnahouse::tests::SegmentsOf(loaded.Value(), *stair, 0.60F, 3.65F);
        ASSERT_FALSE(segments.empty());
        // Enter the west first flight directly from the foyer. The old east-lane detour
        // crossed the protected basement well edge and never represented a natural approach.
        std::vector<Microsoft::Xna::Framework::Vector3> route{{2.55F, 0.60F, -15.25F},
                                                              {2.85F, 0.60F, -15.10F}};
        const auto flight = cnahouse::tests::PathUp(segments);
        route.insert(route.end(), flight.begin(), flight.end());

        class RouteInput final : public cnahouse::player::IInputSource
        {
        public:
            RouteInput(CnaHouseGame& game, const std::vector<Microsoft::Xna::Framework::Vector3>& route)
                : game_(game)
                , route_(route)
            {
            }

            void Update(float) override
            {
                state_ = {};
                const auto& body = game_.PlayerForTesting();
                while (next_ < route_.size() && cnahouse::tests::Flat(body.position, route_[next_]) < 0.12F)
                {
                    ++next_;
                }
                if (next_ == route_.size())
                {
                    return;
                }
                const auto& target = route_[next_];
                const float wanted = std::atan2(target.X - body.position.X, body.position.Z - target.Z);
                const float difference = std::remainder(wanted - body.yaw, 6.2831853F);
                state_.look.X = std::clamp(difference, -0.20F, 0.20F);
                if (std::fabs(difference) < 0.10F)
                {
                    state_.move.Y = 1.0F;
                }
            }

            [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
            {
                return state_;
            }

            [[nodiscard]] bool LookAvailable() const noexcept override
            {
                return true;
            }

            [[nodiscard]] std::size_t Reached() const noexcept
            {
                return next_;
            }

        private:
            CnaHouseGame& game_;
            const std::vector<Microsoft::Xna::Framework::Vector3>& route_;
            cnahouse::player::InputState state_;
            std::size_t next_ = 0;
        };

        cnahouse::util::Log::ResetForTesting();
        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        options.player = std::array<float, 5>{1.55F, 0.60F, -16.00F, 90.0F, 0.0F};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;
        CnaHouseGame game(options, settings);
        RouteInput input(game, route);
        game.SetInputSourceForTesting(&input);
        game.SetFixedStepLimit(2400);
        game.SetFrameLimit(12000);
        game.Run();

        const auto& feet = game.PlayerForTesting().Feet();
        std::printf("  real stair route: reached %zu/%zu, cell %s, feet (%.2f, %.2f, %.2f)\n",
                    input.Reached(),
                    route.size(),
                    std::string(cnahouse::util::IdRegistry::NameOf(game.CellForTesting())).c_str(),
                    static_cast<double>(feet.X),
                    static_cast<double>(feet.Y),
                    static_cast<double>(feet.Z));
        EXPECT_EQ(game.ExitCode(), 0);
        EXPECT_EQ(input.Reached(), route.size());
        EXPECT_GT(feet.Y, 3.25F);
        EXPECT_EQ(cnahouse::util::IdRegistry::NameOf(game.CellForTesting()), "L1_STAIR_MAIN");
    }

    TEST(HeadlessRunTests, TheMainStairRunRailsStopARealControllerWalkOff)
    {
        if (!std::filesystem::exists("content/world/collision.bin"))
        {
            GTEST_SKIP() << "no deployed collision; run tools/ci/build_content.py --only world";
        }

        struct Probe
        {
            std::array<float, 5> start;
            float minimumFeetY;
        };

        // Both exposed sides face a deeper well. Earlier visible balusters had no collision:
        // a held sideways input dropped 1-2 metres through them in the real game loop.
        for (const Probe& probe : {Probe{{2.95F, 1.69F, -17.0F, 90.0F, 0.0F}, 1.35F},
                                   Probe{{4.15F, 2.80F, -17.0F, 270.0F, 0.0F}, 2.45F}})
        {
            cnahouse::player::InputState forward;
            forward.move.Y = 1.0F;
            ScriptedInput input(forward);
            Options options;
            options.headless = true;
            options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
            options.noAudio = true;
            options.scene = "walk";
            options.player = probe.start;
            Settings settings = Settings::Defaults();
            settings.backBufferWidth = 320;
            settings.backBufferHeight = 180;
            settings.verticalSync = false;
            CnaHouseGame game(options, settings);
            game.SetInputSourceForTesting(&input);
            game.SetFixedStepLimit(180);
            game.SetFrameLimit(4000);
            game.Run();
            const auto feet = game.PlayerForTesting().Feet();
            EXPECT_EQ(game.ExitCode(), 0);
            EXPECT_GT(feet.Y, probe.minimumFeetY) << "walked through a visible stair balustrade";
        }
    }

    TEST(HeadlessRunTests, HoldingForwardTraversesTheAtticAndItsGuardStopsWalkoff)
    {
        // HOUSE-03638: no waypoint steering or test-supplied cell changes. The running game
        // receives the same constant forward intent as a held W key and must track each exit.
        struct Probe
        {
            const char* name;
            std::array<float, 5> start;
            std::uint64_t steps;
            const char* endCell;
            float floor;
        };

        for (const Probe& probe :
             {Probe{"ascent", {5.85F, 6.55F, -14.85F, 0.0F, 0.0F}, 780, "L3_STORE_E", 9.30F},
              Probe{"descent", {5.85F, 9.30F, -19.15F, 180.0F, 0.0F}, 780, "L2_STAIR_ATTIC", 6.55F},
              Probe{"room", {5.85F, 9.30F, -19.65F, 270.0F, 0.0F}, 120, "L3_ROOM", 9.30F},
              Probe{"store", {5.85F, 9.30F, -19.65F, 0.0F, 0.0F}, 120, "L3_STORE_E", 9.30F},
              Probe{"guard", {7.20F, 9.30F, -18.0F, 270.0F, 0.0F}, 180, "L3_STAIR_HEAD", 9.30F}})
        {
            SCOPED_TRACE(probe.name);
            cnahouse::util::Log::ResetForTesting();
            cnahouse::player::InputState forward;
            forward.move.Y = 1.0F;
            ScriptedInput input(forward);
            Options options;
            options.headless = true;
            options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
            options.noAudio = true;
            options.scene = "walk";
            options.player = probe.start;
            options.timeOfDay = 10.5;
            options.freezeTime = true;
            Settings settings = Settings::Defaults();
            settings.backBufferWidth = 320;
            settings.backBufferHeight = 180;
            settings.verticalSync = false;
            CnaHouseGame game(options, settings);
            game.SetInputSourceForTesting(&input);
            game.SetFixedStepLimit(probe.steps);
            game.SetFrameLimit(16000);
            game.Run();
            const auto feet = game.PlayerForTesting().Feet();
            std::printf("  attic %s: %s, feet (%.3f, %.3f, %.3f)\n",
                        probe.name,
                        std::string(cnahouse::util::IdRegistry::NameOf(game.CellForTesting())).c_str(),
                        static_cast<double>(feet.X),
                        static_cast<double>(feet.Y),
                        static_cast<double>(feet.Z));
            EXPECT_EQ(game.ExitCode(), 0);
            EXPECT_EQ(cnahouse::util::IdRegistry::NameOf(game.CellForTesting()), probe.endCell);
            EXPECT_NEAR(feet.Y, probe.floor, 0.12F);
            if (std::string_view(probe.name) == "guard")
            {
                EXPECT_GE(feet.X, 6.75F) << "the player crossed the visible well guard";
            }
        }
    }

    TEST(HeadlessRunTests, BasementApproachesAndGarageLoftHaveContinuousSupport)
    {
        // HOUSE-03639: ordinary held-forward controller intent, with no waypoint steering.
        struct Probe
        {
            const char* name;
            std::array<float, 5> start;
            std::uint64_t steps;
            const char* endCell;
            float floor;
            bool moving;
        };

        for (const Probe& probe :
             {Probe{"ascent-left", {3.97F, -2.30F, -19.70F, 180.0F, 0.0F}, 780, "L0_STAIR_MAIN", 0.60F, true},
              Probe{
                  "ascent-right", {4.23F, -2.30F, -19.70F, 180.0F, 0.0F}, 780, "L0_STAIR_MAIN", 0.60F, true},
              Probe{"descent", {4.10F, 0.60F, -14.60F, 0.0F, 0.0F}, 780, "B1_STAIR", -2.30F, true},
              Probe{"loft-floor", {14.0F, 2.90F, -19.5F, 0.0F, 0.0F}, 180, "L0_GARAGE", 2.90F, false},
              Probe{"loft-edge", {14.0F, 2.90F, -19.5F, 180.0F, 0.0F}, 360, "L0_GARAGE", 2.90F, true}})
        {
            SCOPED_TRACE(probe.name);
            cnahouse::util::Log::ResetForTesting();
            cnahouse::player::InputState state;
            state.move.Y = probe.moving ? 1.0F : 0.0F;
            ScriptedInput input(state);
            Options options;
            options.headless = true;
            options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
            options.noAudio = true;
            options.scene = "walk";
            options.player = probe.start;
            options.timeOfDay = 10.5;
            options.freezeTime = true;
            Settings settings = Settings::Defaults();
            settings.backBufferWidth = 320;
            settings.backBufferHeight = 180;
            settings.verticalSync = false;
            CnaHouseGame game(options, settings);
            game.SetInputSourceForTesting(&input);
            game.SetFixedStepLimit(probe.steps);
            game.SetFrameLimit(16000);
            game.Run();
            const auto feet = game.PlayerForTesting().Feet();
            std::printf("  basement/loft %s: %s, feet (%.3f, %.3f, %.3f)\n",
                        probe.name,
                        std::string(cnahouse::util::IdRegistry::NameOf(game.CellForTesting())).c_str(),
                        static_cast<double>(feet.X),
                        static_cast<double>(feet.Y),
                        static_cast<double>(feet.Z));
            EXPECT_EQ(game.ExitCode(), 0);
            EXPECT_EQ(cnahouse::util::IdRegistry::NameOf(game.CellForTesting()), probe.endCell);
            EXPECT_NEAR(feet.Y, probe.floor, 0.12F);
        }
    }

    TEST(HeadlessRunTests, TheWalkSceneLoadsTheSunBakeAndPublishesDaylight)
    {
        // `HOUSE-01564`, end to end: the walk loader reads openings, interactables, initial state
        // and `shading.bin`, then the lighting stage reads the same clock the game advanced. Unit
        // tests prove each model; this catches a perfectly good model left unwired in the app.
        cnahouse::util::Log::ResetForTesting();
        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        ConsoleDriver driver(game, "time set 12:00", 2);
        game.SetInputSourceForTesting(&driver);
        game.SetFrameLimit(6);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);
        ASSERT_TRUE(driver.Result().ok) << driver.Result().message;

        const cnahouse::lighting::LightingSystem* lighting = game.LightingForTesting();
        ASSERT_NE(lighting, nullptr);
        EXPECT_GT(lighting->ComputedForFrame(), 0U);
        EXPECT_EQ(lighting->Cells().size(), 96U);
        const auto sun = cnahouse::environment::SunPositionFor(game.ClockForTesting());
        EXPECT_NEAR(lighting->Sun().altitudeDeg, sun.altitudeDeg, 1e-10);
        EXPECT_NEAR(lighting->Sun().azimuthDeg, sun.azimuthDeg, 1e-10);

        int daylit = 0;
        for (const cnahouse::lighting::RoomLightState& cell : lighting->Cells())
        {
            daylit += cell.daylight > 0.0F ? 1 : 0;
        }
        EXPECT_GT(daylit, 25) << "the walk loaded a daylight model but no windows reached it";
        const cnahouse::lighting::RoomLightState* outside =
            lighting->FindCell(cnahouse::util::Id::Of("EXT_WORLD"));
        ASSERT_NE(outside, nullptr);
        EXPECT_GT(outside->skyAmbientColor.Z, outside->skyAmbientColor.X)
            << "the walk loaded the sky dome but did not share its blue daytime colour with lighting";
        bool foundTintedLmDay = false;
        for (const cnahouse::lighting::RoomLightState& cell : lighting->Cells())
        {
            foundTintedLmDay = foundTintedLmDay || cell.daylightTint.Z > 0.0F;
        }
        EXPECT_TRUE(foundTintedLmDay) << "no room published the tint for its future LM_DAY pass";
        ASSERT_GT(sun.altitudeDeg, cnahouse::environment::kRefractedHorizonDeg);
        EXPECT_NE(lighting->SunKeyForCell(cnahouse::util::Id::Of("EXT_WORLD")), nullptr)
            << "outdoor objects did not receive the sun key";

        const cnahouse::debug::Counter* disc = game.CountersForTesting().Find("sun.disc.draws");
        ASSERT_NE(disc, nullptr) << "the walk loaded daylight but never installed the sky pass";
        EXPECT_EQ(disc->current, 1) << "the visible noon sun did not submit its quad this frame";
        EXPECT_EQ(disc->Max(), 1) << "the sky pass submitted more than one sun quad in a frame";
    }

    TEST(HeadlessRunTests, HoldingForwardWalksTheBodyAcrossTheRoomAtSectionFortyThreesSpeed)
    {
        // The wiring end to end, with the body MOVING: §49.3's fixed step driven from the frame
        // time, §43.2's speed, §16.4's cell tracking following the feet, and §44's camera arriving
        // where the eye is. Standing still proves none of those -- a game that never ran a step
        // would pass a test that only looked at where the body was put.
        cnahouse::util::Log::ResetForTesting();

        cnahouse::player::InputState forward;
        forward.move.Y = 1.0F;
        ScriptedInput input(forward);

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        // §12's basement hallway, at its south end: 12.8 m of straight, empty corridor, which is
        // the longest clear run in the house and the only place a speed can be measured without
        // the far wall stopping the body first.
        options.player = std::array<float, 5>{0.00f, -2.30f, -15.00f, 0.0f, 0.0f};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        // §49.3's steps and not frames: 240 steps is exactly two simulated seconds on every
        // machine, where 250 FRAMES was a quarter of a second here and two seconds on a slow
        // build -- and `HOUSE-00684` made the frames four times cheaper, which moved the number
        // this test used to depend on. Two seconds is about 2.6 m at §43.2's speed: well past the
        // metre and a half asserted below, and nowhere near the wall 12 m away. The frame limit
        // stays as the backstop for a session whose steps never run.
        game.SetFixedStepLimit(240);
        game.SetFrameLimit(4000);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        const auto& player = game.PlayerForTesting();
        // North is -Z (§14). How FAR it got depends on how long 400 headless frames took, so what
        // is asserted is that it walked at all, that it walked the right way, and that it did not
        // walk through the wall at the end of the hall (z = -27.10, less the capsule's radius).
        EXPECT_LT(player.Feet().Z, -16.50f) << "the body did not walk north";
        EXPECT_GT(player.Feet().Z, -27.10f + cnahouse::player::kPlayerRadius - 0.01f)
            << "the body walked through the back of the house";
        EXPECT_TRUE(game.CellForTesting().IsValid());
        EXPECT_NEAR(player.Feet().X, 0.0f, 0.30f) << "it wandered sideways";

        // §49.3's clock: the distance walked is §43.2's speed over the SIMULATED time, which is
        // the fixed steps and not the frame times. They agree only if each step advanced by
        // 1/120 s -- a step given the whole frame's time instead runs the world fast, and the body
        // arrives further down the hall than the simulation says it walked.
        const double simulated = static_cast<double>(game.FixedStepsForTesting()) *
                                 static_cast<double>(cnahouse::player::kFixedStepSeconds);
        const double walked = static_cast<double>(-15.00f - player.Feet().Z);
        // Said out loud rather than left to make the speed look wrong: a body stopped by the far
        // wall walked as far as the hall is long and no further, whatever its speed was.
        ASSERT_GT(player.Feet().Z, -26.0f) << "the walk reached the end of the hall; the speed below "
                                              "would be measuring §12's geometry and not §43.2's";
        std::printf("  %llu step(s) = %.3f s simulated, %.3f m walked (%.3f m/s)\n",
                    static_cast<unsigned long long>(game.FixedStepsForTesting()),
                    simulated,
                    walked,
                    walked / simulated);
        ASSERT_GT(simulated, 1.9) << "the game ran no steps to measure";
        // Integrate the actual fixed-step acceleration, not an obsolete fixed speed bound.
        double expectedTravel = 0.0;
        double speed = 0.0;
        const double dt = static_cast<double>(cnahouse::player::kFixedStepSeconds);
        for (std::uint64_t step = 0; step < game.FixedStepsForTesting(); ++step)
        {
            speed = std::min(static_cast<double>(cnahouse::player::kWalkSpeed),
                             speed + static_cast<double>(cnahouse::player::kAcceleration) * dt);
            expectedTravel += speed * dt;
        }
        EXPECT_NEAR(walked, expectedTravel, 0.02) << "actual travel differs from walk/acceleration policy";
        EXPECT_NEAR(player.Feet().Y, -2.30f, 0.02f) << "it left §12's basement floor";
        EXPECT_TRUE(game.CellForTesting().IsValid());

        // §44's camera followed it, and the eye is over the feet it has now rather than the ones
        // it started with.
        const auto& camera = game.ViewForTesting().Camera();
        EXPECT_NEAR(camera.Pose().eye.Z, player.position.Z, 1e-3f);
        EXPECT_NEAR(camera.Pose().eye.Y, player.Feet().Y + cnahouse::player::kPlayerEyeHeight, 0.02f);
    }

    TEST(HeadlessRunTests, WalkingThroughADoorwayTakesTheCellTrackingWithIt)
    {
        // §16.4's tracking, from the game loop rather than from a unit test: the body walks north
        // out of §12's foyer, and what it collides against on the far side of the opening is the
        // NEXT cell's geometry. A tracker that stopped updating would leave the body colliding
        // against the foyer -- which is to say, walking into the wall it just went through.
        cnahouse::util::Log::ResetForTesting();

        cnahouse::player::InputState forward;
        forward.move.Y = 1.0F;
        ScriptedInput input(forward);

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        options.player = std::array<float, 5>{0.00f, 0.60f, -15.00f, 0.0f, 0.0f};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        // Four simulated seconds, which is five metres of walking at §43.2's speed -- enough to
        // leave the foyer through the doorway from any starting jitter, and the same five metres
        // whatever the machine.
        game.SetFixedStepLimit(480);
        game.SetFrameLimit(6000);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        const auto& player = game.PlayerForTesting();
        std::printf("  ended in %s at z %.2f\n",
                    std::string(cnahouse::util::IdRegistry::NameOf(game.CellForTesting())).c_str(),
                    static_cast<double>(player.Feet().Z));
        EXPECT_LT(player.Feet().Z, -18.30f) << "the body never left the foyer";
        EXPECT_TRUE(game.CellForTesting().IsValid());
        EXPECT_NE(cnahouse::util::IdRegistry::NameOf(game.CellForTesting()), "L0_FOYER")
            << "the body walked out of the foyer and the cell tracking stayed in it";
    }

    TEST(HeadlessRunTests, ACameraCrossingADoorwaySeesTheNewRoomBeforeTheBodyCellHysteresisEnds)
    {
        // §16.4 keeps the old gameplay cell for 5 cm after the centre crosses a doorway. The
        // camera and its near plane are already in the new room; starting the render walk in
        // the sticky body cell drops that new room for a few frames and shows the sky through it.
        class ThresholdInput final : public cnahouse::player::IInputSource
        {
        public:
            explicit ThresholdInput(CnaHouseGame& game)
                : game_(game)
            {
                // A full 2.05 m/s walk advances 68 mm in a capped four-step frame, skipping
                // this test's 40 mm band. Analogue review input bounds travel to 27.4 mm;
                // the real controller, acceleration and cell hysteresis remain unchanged.
                state_.move.Y = 0.4F;
            }

            void Update(float) override
            {
                // Exercise the worst sampling cadence deliberately rather than depending on
                // how fast a shared machine draws. The next frame consumes four fixed steps.
                std::this_thread::sleep_for(std::chrono::milliseconds(35));
                const std::uint64_t steps = game_.FixedStepsForTesting();
                const std::uint64_t previousFrameSteps = steps - lastSteps_;
                lastSteps_ = steps;
                const auto& eye = game_.ViewForTesting().Camera().Pose().eye;
                if (cnahouse::util::IdRegistry::NameOf(game_.CellForTesting()) != "L0_HALL" ||
                    eye.Z >= -23.005F || eye.Z <= -23.045F)
                {
                    return;
                }
                sawBand_ = true;
                sawMaximumStepFrameInBand_ =
                    sawMaximumStepFrameInBand_ ||
                    previousFrameSteps ==
                        static_cast<std::uint64_t>(cnahouse::player::kMaxFixedStepsPerFrame);
                const auto snapshot = game_.VisibilitySnapshotForTesting();
                newRoomWasRoot_ = newRoomWasRoot_ && snapshot.cell == "L0_KITCHEN";
                newRoomWasVisible_ =
                    newRoomWasVisible_ && std::any_of(snapshot.visible.begin(),
                                                      snapshot.visible.end(),
                                                      [](const cnahouse::debug::VisibleCellLine& row)
                                                      { return row.cell == "L0_KITCHEN"; });
            }

            [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
            {
                return state_;
            }

            [[nodiscard]] bool LookAvailable() const noexcept override
            {
                return false;
            }

            [[nodiscard]] bool SawBand() const noexcept
            {
                return sawBand_;
            }

            [[nodiscard]] bool NewRoomWasRoot() const noexcept
            {
                return newRoomWasRoot_;
            }

            [[nodiscard]] bool SawMaximumStepFrameInBand() const noexcept
            {
                return sawMaximumStepFrameInBand_;
            }

            [[nodiscard]] bool NewRoomWasVisible() const noexcept
            {
                return newRoomWasVisible_;
            }

        private:
            CnaHouseGame& game_;
            cnahouse::player::InputState state_;
            std::uint64_t lastSteps_ = 0U;
            bool sawBand_ = false;
            bool sawMaximumStepFrameInBand_ = false;
            bool newRoomWasRoot_ = true;
            bool newRoomWasVisible_ = true;
        };

        cnahouse::util::Log::ResetForTesting();
        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        options.player = std::array<float, 5>{0.0F, 0.60F, -22.80F, 0.0F, 0.0F};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        ThresholdInput input(game);
        game.SetInputSourceForTesting(&input);
        game.SetFixedStepLimit(90);
        game.SetFrameLimit(3000);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);
        EXPECT_TRUE(input.SawBand()) << "the walking test never sampled the 5 cm doorway band";
        EXPECT_TRUE(input.SawMaximumStepFrameInBand())
            << "the threshold was not observed after the maximum catch-up batch";
        EXPECT_TRUE(input.NewRoomWasRoot()) << "render visibility followed the sticky body cell";
        EXPECT_TRUE(input.NewRoomWasVisible()) << "the room ahead vanished while crossing its threshold";
    }

    TEST(HeadlessRunTests, AShiftEdgeSurvivesZeroStepsAndTogglesOnlyOnceAcrossMultipleSteps)
    {
        class RunPulseInput final : public cnahouse::player::IInputSource
        {
        public:
            RunPulseInput(const CnaHouseGame& game, bool onZeroStep)
                : game_(game)
                , onZeroStep_(onZeroStep)
            {
            }

            void Update(float deltaSeconds) override
            {
                if (awaitingStepCount_)
                {
                    pulseSteps_ = game_.FixedStepsForTesting() - stepsBeforePulse_;
                    awaitingStepCount_ = false;
                }
                const int steps = cnahouse::player::FixedSteps(predictedAccumulator_, deltaSeconds);
                state_.runPressed = false;
                if (!sent_ && (onZeroStep_ ? steps == 0 : steps >= 2 && steps % 2 == 0))
                {
                    state_.runPressed = true;
                    sent_ = true;
                    stepsBeforePulse_ = game_.FixedStepsForTesting();
                    awaitingStepCount_ = true;
                }
                if (!sent_ && !onZeroStep_)
                {
                    // The sleep affects the NEXT frame's delta, not the current input frame.
                    // Pulse only when FixedSteps predicts an even multi-step frame, so a level
                    // accidentally applied on every step is guaranteed to cancel itself.
                    std::this_thread::sleep_for(std::chrono::milliseconds(17));
                }
            }

            [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
            {
                return state_;
            }

            [[nodiscard]] bool LookAvailable() const noexcept override
            {
                return false;
            }

            [[nodiscard]] bool Sent() const noexcept
            {
                return sent_;
            }

            [[nodiscard]] std::uint64_t PulseSteps() const noexcept
            {
                return pulseSteps_;
            }

        private:
            const CnaHouseGame& game_;
            cnahouse::player::InputState state_;
            float predictedAccumulator_ = 0.0F;
            bool onZeroStep_ = false;
            bool sent_ = false;
            bool awaitingStepCount_ = false;
            std::uint64_t stepsBeforePulse_ = 0;
            std::uint64_t pulseSteps_ = 0;
        };

        for (const bool onZeroStep : {true, false})
        {
            cnahouse::util::Log::ResetForTesting();
            Options options;
            options.headless = true;
            options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
            options.noAudio = true;
            options.scene = "walk";
            options.player = std::array<float, 5>{0.0F, 0.60F, -20.65F, 0.0F, 0.0F};
            Settings settings = Settings::Defaults();
            settings.backBufferWidth = 320;
            settings.backBufferHeight = 180;
            settings.verticalSync = false;
            settings.fastWalk = true; // A legacy preference must not start the game running.

            CnaHouseGame game(options, settings);
            RunPulseInput input(game, onZeroStep);
            game.SetInputSourceForTesting(&input);
            game.SetFixedStepLimit(60);
            game.SetFrameLimit(3000);
            game.Run();
            ASSERT_EQ(game.ExitCode(), 0);
            ASSERT_TRUE(input.Sent())
                << (onZeroStep ? "no zero-step input frame" : "no even multi-step frame");
            if (onZeroStep)
            {
                EXPECT_EQ(input.PulseSteps(), 0U);
            }
            else
            {
                EXPECT_GE(input.PulseSteps(), 2U);
                EXPECT_EQ(input.PulseSteps() % 2U, 0U);
            }
            EXPECT_TRUE(game.PlayerForTesting().fastWalk)
                << (onZeroStep ? "Shift edge was lost before a physics tick"
                               : "Shift edge toggled more than once in a multi-step frame");
            EXPECT_FALSE(game.UserSettings().fastWalk) << "running is not a saved preference";
        }
    }

    TEST(HeadlessRunTests, AWalkSceneAskedForAnImpossiblePlaceDrawsAFrameAnyway)
    {
        // Half a kilometre down the road is not in any cell -- §10.3's world box is 400 m across.
        // The scene says so and falls back to the fixed camera rather than refusing to start: a
        // screenshot of the wrong place is a bug report, and a game that will not run is a
        // mystery.
        cnahouse::util::Log::ResetForTesting();

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        options.player = std::array<float, 5>{500.0f, 0.0f, 500.0f, 0.0f, 0.0f};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(10);
        game.Run();

        EXPECT_EQ(game.ExitCode(), 0);
        EXPECT_GE(game.FramesDrawn(), 10u);
        EXPECT_FALSE(game.CellForTesting().IsValid()) << "it claimed to be walking somewhere";
    }

    TEST(HeadlessRunTests, TheVersionLineNamesTheBuildItCameFrom)
    {
        // Drawn in the corner and printed at startup, so it is the first thing on a screenshot and in
        // a bug report. It has to say which renderer and which tier, or a report is unattributable.
        const std::string line = CnaHouseGame::VersionLine();
        EXPECT_NE(line.find(CNAHOUSE_VERSION), std::string::npos) << line;
        EXPECT_NE(line.find(CNAHOUSE_RENDERER_NAME), std::string::npos) << line;
        EXPECT_NE(line.find("Tier"), std::string::npos) << line;
    }

    TEST(HeadlessRunTests, TheHudFontLoadsFromTheBuiltContentTree)
    {
        // The Phase-2 exit criterion has the version string on screen, which needs the font to be
        // there. Asserting on the log is what makes "it loaded" checkable without a GPU readback:
        // `LoadContent` logs a warning if and only if the font is absent.
        cnahouse::util::Log::ResetForTesting();
        Options options;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 240;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(5);
        game.Run();

        for (const auto& record : cnahouse::util::Log::Ring())
        {
            EXPECT_EQ(record.message.find("did not load"), std::string::npos)
                << "content that the build produced must be findable at run time: " << record.message;
        }
    }

    TEST(HeadlessRunTests, AMissingHudFontDoesNotStopTheGameStarting)
    {
        // A build whose content tree has not been generated must still start and still say so. The
        // first thing a new contributor sees should not be a crash.
        cnahouse::util::Log::ResetForTesting();
        Options options;
        options.contentRoot = "no-such-content-directory";
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 240;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(10);
        EXPECT_NO_THROW(game.Run());
        EXPECT_GE(game.FramesDrawn(), 10u);
    }

} // namespace
