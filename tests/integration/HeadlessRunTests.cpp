// SPDX-License-Identifier: MIT
//
// `HOUSE-00135`'s target: the application itself, run for real frames, with no window.
//
// This is the test that would catch the failures unit tests structurally cannot -- a `Game` that
// throws during `Initialize`, content that cannot be found from the working directory, a `Draw`
// that leaves the device in a state the next frame rejects. It runs under the `headless` preset,
// which `HOUSE-00105` measured doing 600 frames with `DISPLAY` unset.
#include <cmath>
#include <cstdio>

#include <gtest/gtest.h>

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
#include "cnahouse/player/FirstPersonView.hpp"
#include "cnahouse/player/IInputSource.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Log.hpp"

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
        // §71.2's 620 draw calls, over FOUR frames: a list that was not emptied between them would
        // be four houses long and would still draw a correct-looking picture.
        EXPECT_LE(list.DrawCalls(), 620) << "the list was not cleared between frames";

        const cnahouse::debug::Counter* chunks = game.CountersForTesting().Find("static.chunks");
        const cnahouse::debug::Counter* states = game.CountersForTesting().Find("static.stateChanges");
        ASSERT_NE(chunks, nullptr);
        ASSERT_NE(states, nullptr);
        std::printf("  blockout: %lld chunk(s) submitted, %lld state change(s), list of %zu\n",
                    static_cast<long long>(chunks->Max()),
                    static_cast<long long>(states->Max()),
                    list.Size());

        // Every item in the list was drawn: nothing in it named a chunk the runtime could not find.
        EXPECT_EQ(static_cast<std::size_t>(chunks->Max()), list.Size());
        // And the material was bound once per run, not once per chunk. §71.2 budgets 90 typically.
        EXPECT_GT(states->Max(), 0);
        EXPECT_LE(states->Max(), 90);
        EXPECT_LT(states->Max(), chunks->Max() / 4)
            << "the sort bought nothing: the pass is rebinding almost per chunk";
        // The pass's own count and the list's agree, which is what says the two are counting the
        // same thing rather than each counting its own.
        EXPECT_EQ(states->Max(), list.StateChanges() + 1)
            << "the list counts CHANGES between neighbours and the pass counts BINDS, so the pass "
               "is always one ahead -- the first bind is not a change";
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
        options.scene = "walk";
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
        EXPECT_GT(weather->TargetExpiryMinutes(), 139.0F);
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
        // §71.2's hard fail is 30 visible cells; §65.6's doors start shut, so this is far under it.
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
        EXPECT_LT(snapshot.drawCalls, 100) << "the frame is still drawing most of the house";
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
        options.player = std::array<float, 5>{-3.00f, 0.60f, -25.05f, 90.0f, 0.0f};
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
        EXPECT_NEAR(player.position.X, -3.00f, 0.05f) << "the body walked while the walk was frozen";
        EXPECT_NEAR(player.position.Z, -25.05f, 0.05f);
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
        explicit ScriptedInput(cnahouse::player::InputState state)
            : state_(state)
        {
        }

        void Update(float) override {}

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
    };

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
        // §43.2's 1.35 m/s, minus the 0.15 s the body spends reaching it (§43.2's 9.0 m/s²).
        EXPECT_LT(walked / simulated, 1.35 * 1.05) << "the body covered more ground than it had time for";
        EXPECT_GT(walked / simulated, 1.35 * 0.80) << "it walked slower than §43.2's speed";
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
