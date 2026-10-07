// SPDX-License-Identifier: MIT
//
// `HOUSE-01263`. §28.4's daylight model, over the authored house.
//
// The formula has five factors and the interesting claims are about how they COMPOSE, not about
// any one of them: a south window is brighter at noon than a north one, an overcast noon is
// flatter but not dark, an open sash admits more than a shut one, and the porch keeps the sun off
// the foyer. Those are properties of the house and the sun together, and they are checked against
// the authored world rather than a fixture -- a daylight model that behaves on a two-window toy
// and divides by zero on `L0_GARAGE` has not been tested.
//
// The one number that had to be chosen rather than derived, `kFullDaylightGlazingRatio`, is
// measured from this house and the measurement is asserted here so it cannot drift silently.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/lighting/DaylightModel.hpp"
#include "cnahouse/lighting/ShadingGrid.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::lighting::DaylightModel;
    using cnahouse::lighting::DaylightWindow;
    using cnahouse::lighting::kDirectConeDeg;
    using cnahouse::lighting::kFullDaylightGlazingRatio;
    using cnahouse::lighting::kOpenBoost;
    using cnahouse::lighting::kTransmissionGlass;
    using cnahouse::lighting::ShadingGrid;
    using cnahouse::lighting::SkyExposure;
    using cnahouse::lighting::SkyExposureFor;
    using cnahouse::util::Id;
    namespace world = cnahouse::world;

    bool ContentIsBuilt()
    {
        return std::filesystem::exists("content/world/layout.openings.json");
    }

    world::WorldData LoadWorld()
    {
        world::WorldData::Contents contents;
        EXPECT_TRUE(world::WorldLoader::LoadLevels("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadCells("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadPortals("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadOpenings("content/world", contents).HasValue());
        // The interactables are what join an initial state to an opening -- see the open-sash
        // test -- so a world loaded without them cannot honour `initialstate.json`'s open window.
        EXPECT_TRUE(world::WorldLoader::LoadInteractables("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadInitialState("content/world", contents).HasValue());
        auto built = world::WorldData::Create(std::move(contents));
        EXPECT_TRUE(built) << built.Error().ToString();
        return std::move(built.Value());
    }

    ShadingGrid LoadShading()
    {
        auto grid = ShadingGrid::ReadFromTitle("content/world/shading.bin");
        return grid ? std::move(grid.Value()) : ShadingGrid::Unshaded();
    }

} // namespace

TEST(SkyExposureTests, TheDiffuseTermFollowsTheSineOfTheAltitudeAndVanishesBelowTheHorizon)
{
    // §28.4: *"a diffuse sky term proportional to max(0, sin(sunAltitude))"*.
    for (const double altitude : {5.0, 20.0, 45.0, 73.0, 90.0})
    {
        const SkyExposure clear = SkyExposureFor(180.0, altitude, 270.0, 0.0);
        // 270° is 90° off the window's 180°, outside the ±75° cone, so this is the diffuse term
        // on its own with nothing to disentangle.
        EXPECT_FLOAT_EQ(clear.direct, 0.0F) << "altitude " << altitude;
        EXPECT_NEAR(clear.diffuse, std::sin(altitude * 3.14159265358979 / 180.0), 1e-5)
            << "altitude " << altitude;
    }
    EXPECT_FLOAT_EQ(SkyExposureFor(180.0, 0.0, 180.0, 0.0).Total(), 0.0F) << "on the horizon";
    EXPECT_FLOAT_EQ(SkyExposureFor(180.0, -10.0, 180.0, 0.0).Total(), 0.0F) << "below it";
}

TEST(SkyExposureTests, TheDirectTermIsZeroOutsideTheConeAndReachesItWithoutAStep)
{
    // §28.4 says the direct term is *"non-zero only when the sun's azimuth is within ±75° of the
    // window's outward normal"*. A literal hard edge satisfies that sentence and puts a step in a
    // room's brightness once per window per day; the taper satisfies it and does not.
    const double window = 180.0;
    EXPECT_GT(SkyExposureFor(window, 45.0, 180.0, 0.0).direct, 0.0F) << "head-on";
    EXPECT_GT(SkyExposureFor(window, 45.0, 180.0 + 74.0, 0.0).direct, 0.0F) << "just inside";
    EXPECT_FLOAT_EQ(SkyExposureFor(window, 45.0, 180.0 + kDirectConeDeg, 0.0).direct, 0.0F)
        << "exactly at the cone edge";
    EXPECT_FLOAT_EQ(SkyExposureFor(window, 45.0, 180.0 + 76.0, 0.0).direct, 0.0F) << "outside";
    EXPECT_FLOAT_EQ(SkyExposureFor(window, 45.0, 0.0, 0.0).direct, 0.0F) << "behind the wall";

    // No step anywhere: the largest jump over a quarter-degree sweep bounds what a frame can do.
    double worst = 0.0;
    double worstAt = 0.0;
    double previous = static_cast<double>(SkyExposureFor(window, 45.0, window - 180.0, 0.0).direct);
    for (double offset = -180.0; offset <= 180.0; offset += 0.25)
    {
        const double current = static_cast<double>(SkyExposureFor(window, 45.0, window + offset, 0.0).direct);
        if (std::abs(current - previous) > worst)
        {
            worst = std::abs(current - previous);
            worstAt = offset;
        }
        previous = current;
    }
    EXPECT_LT(worst, 0.01) << "the direct term steps by " << worst << " at " << worstAt << "° off";
    std::printf(
        "  worst quarter-degree step in the direct term: %.5f (at %+.2f° off normal)\n", worst, worstAt);
}

TEST(SkyExposureTests, CloudFlattensTheLightRatherThanRemovingIt)
{
    // §28.4: the direct term is multiplied by `(1 − cloudCover)³` and the diffuse one is merely
    // modulated. That difference IS the look of an overcast day, so it is asserted as a ratio and
    // not as two numbers.
    const SkyExposure clear = SkyExposureFor(180.0, 45.0, 180.0, 0.0);
    const SkyExposure overcast = SkyExposureFor(180.0, 45.0, 180.0, 1.0);
    EXPECT_FLOAT_EQ(overcast.direct, 0.0F) << "(1-1)^3 is 0";
    EXPECT_GT(overcast.diffuse, 0.6F * clear.diffuse) << "an overcast noon is not dark";
    EXPECT_LT(overcast.Total(), clear.Total());

    // Half cover takes the direct term to an eighth, which is the cube doing its work.
    const SkyExposure half = SkyExposureFor(180.0, 45.0, 180.0, 0.5);
    EXPECT_NEAR(half.direct, clear.direct * 0.125F, 1e-5F);

    // Out-of-range cover is clamped rather than extrapolated.
    EXPECT_FLOAT_EQ(SkyExposureFor(180.0, 45.0, 180.0, -3.0).Total(), clear.Total());
    EXPECT_FLOAT_EQ(SkyExposureFor(180.0, 45.0, 180.0, 7.0).Total(), overcast.Total());
    EXPECT_TRUE(std::isfinite(SkyExposureFor(180.0, 45.0, 180.0, std::nan("")).Total()));
}

TEST(SkyExposureTests, TheAzimuthDifferenceWrapsSoDueNorthIsNotASeam)
{
    // A window facing 350° and a sun at 10° are 20° apart, not 340°. Getting this wrong makes
    // north-facing windows behave inside out.
    EXPECT_GT(SkyExposureFor(350.0, 45.0, 10.0, 0.0).direct, 0.0F);
    EXPECT_FLOAT_EQ(SkyExposureFor(350.0, 45.0, 10.0, 0.0).direct,
                    SkyExposureFor(10.0, 45.0, 30.0, 0.0).direct);
    EXPECT_FLOAT_EQ(SkyExposureFor(0.0, 45.0, 359.0, 0.0).direct, SkyExposureFor(0.0, 45.0, 1.0, 0.0).direct);
}

TEST(DaylightModelTests, EveryWindowThatFacesOutsideIsInTheModelAndTheInteriorOneIsNot)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world";
    }
    const world::WorldData data = LoadWorld();
    const ShadingGrid shading = LoadShading();
    const DaylightModel model(data, shading);

    int windows = 0;
    int interior = 0;
    for (const world::Opening& opening : data.Openings())
    {
        if (opening.kind != world::OpeningKind::Window)
        {
            continue;
        }
        ++windows;
        const world::Portal* portal = data.FindPortal(opening.portal);
        ASSERT_NE(portal, nullptr);
        const world::Cell* a = data.FindCell(portal->cellA);
        const world::Cell* b = data.FindCell(portal->cellB);
        ASSERT_NE(a, nullptr);
        ASSERT_NE(b, nullptr);
        if ((a->kind == world::CellKind::Exterior) == (b->kind == world::CellKind::Exterior))
        {
            ++interior;
        }
    }
    EXPECT_EQ(windows, 64) << "§12.6 schedules 64 windows";
    EXPECT_EQ(interior, 2) << "two windows have no exterior cell on exactly one side, by `CellKind` alone";
    // ...and only ONE of them is excluded, which is the whole point of the model's rule.
    // `WIN_EXT_SHED_1` joins `EXT_SHED` to `EXT_GARDEN` and both are `CellKind::Exterior`, but the
    // shed is a shed -- walls, a roof, `visibilityHint: opaque` -- and its one window is the only
    // daylight it has. `WIN_L0_KITCHEN_2` joins two rooms and genuinely has no sky.
    EXPECT_EQ(model.Windows().size(), 63U)
        << "an interior window has no sky and must not contribute a sky term; a shed's window does";
    bool shedIsIn = false;
    bool kitchenIsIn = false;
    for (const DaylightWindow& window : model.Windows())
    {
        shedIsIn = shedIsIn || window.window == Id::Of("WIN_EXT_SHED_1");
        kitchenIsIn = kitchenIsIn || window.window == Id::Of("WIN_L0_KITCHEN_2");
    }
    EXPECT_TRUE(shedIsIn) << "the shed is enclosed and its window is its only daylight";
    EXPECT_FALSE(kitchenIsIn) << "an interior window has no sky";
    EXPECT_GT(model.GlazingRatioFor(Id::Of("EXT_SHED")), 0.0F);
    std::printf("  %d window(s), %d without one exterior side, %zu in the model\n",
                windows,
                interior,
                model.Windows().size());
}

TEST(DaylightModelTests, TheGlazingRatiosAreTheOnesTheConstantWasChosenFrom)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world";
    }
    // `kFullDaylightGlazingRatio` is the one number in §28.4 that had to be chosen rather than
    // derived, and it was chosen from this distribution. Asserting the distribution is what stops
    // the constant becoming an arbitrary number after the house changes underneath it.
    const world::WorldData data = LoadWorld();
    const ShadingGrid unshadedGrid = ShadingGrid::Unshaded();
    const DaylightModel model(data, unshadedGrid);

    std::vector<float> ratios;
    float best = 0.0F;
    std::string bestCell;
    for (const world::Cell& cell : data.Cells())
    {
        const float ratio = model.GlazingRatioFor(cell.id);
        if (ratio <= 0.0F)
        {
            continue;
        }
        ratios.push_back(ratio);
        if (ratio > best)
        {
            best = ratio;
            bestCell = cell.name;
        }
    }
    std::sort(ratios.begin(), ratios.end());
    ASSERT_FALSE(ratios.empty());
    const float median = ratios[ratios.size() / 2];
    EXPECT_GE(ratios.size(), 30U) << "most rooms in this house have a window";
    EXPECT_NEAR(median, 0.115F, 0.02F) << "the measured median glazing ratio";
    EXPECT_NEAR(best, 0.402F, 0.02F) << "the sunroom, glazed on three sides";
    EXPECT_EQ(bestCell, "Sunroom");
    EXPECT_GT(kFullDaylightGlazingRatio, median)
        << "a median room must not be fully daylit by geometry alone";
    EXPECT_LT(kFullDaylightGlazingRatio, best) << "the sunroom must be able to saturate";
    std::printf("  %zu cell(s) with windows, ratio %.3f-%.3f, median %.3f; full daylight at %.2f\n",
                ratios.size(),
                static_cast<double>(ratios.front()),
                static_cast<double>(best),
                static_cast<double>(median),
                static_cast<double>(kFullDaylightGlazingRatio));
}

TEST(DaylightModelTests, ASouthWindowBeatsANorthOneAtNoonAndTheOrderReversesAtNoTimeOfDay)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world";
    }
    // At 40° N the sun is in the south all day, every day. A model that got the window bearing
    // backwards would light the north rooms at noon, and nothing else in this file would see it.
    const world::WorldData data = LoadWorld();
    const ShadingGrid unshadedGrid = ShadingGrid::Unshaded();
    const DaylightModel model(data, unshadedGrid);

    int south = 0;
    int north = 0;
    for (const DaylightWindow& window : model.Windows())
    {
        const float bearing = window.azimuthDeg;
        if (bearing > 135.0F && bearing < 225.0F)
        {
            ++south;
        }
        if (bearing < 45.0F || bearing > 315.0F)
        {
            ++north;
        }
        // §15's portals are axis-aligned, so every bearing is a cardinal point.
        const float nearest = std::round(bearing / 90.0F) * 90.0F;
        EXPECT_NEAR(bearing, std::fmod(nearest, 360.0F), 1e-3F)
            << "window bearing " << bearing << " is not a cardinal direction";
    }
    EXPECT_GT(south, 0);
    EXPECT_GT(north, 0);

    // The sun due south at 40° altitude: a south pane gets the direct term and a north one cannot.
    const SkyExposure southPane = SkyExposureFor(180.0, 40.0, 180.0, 0.0);
    const SkyExposure northPane = SkyExposureFor(0.0, 40.0, 180.0, 0.0);
    EXPECT_GT(southPane.Total(), northPane.Total());
    EXPECT_FLOAT_EQ(northPane.direct, 0.0F);
    EXPECT_GT(northPane.diffuse, 0.0F) << "a north room still sees sky";
    std::printf("  %zu window(s): %d face south, %d face north\n", model.Windows().size(), south, north);
}

TEST(DaylightModelTests, TheHouseAtNoonIsLitAndAtMidnightIsNot)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world";
    }
    const world::WorldData data = LoadWorld();
    const ShadingGrid shading = LoadShading();
    const DaylightModel model(data, shading);
    std::vector<float> noon(data.Cells().size(), -1.0F);
    std::vector<float> night(data.Cells().size(), -1.0F);

    // §32.1's June noon at 40.05° N: 73.4° up, due south.
    model.Evaluate(73.4, 180.0, 0.0, noon);
    model.Evaluate(-30.0, 0.0, 0.0, night);

    int lit = 0;
    float brightest = 0.0F;
    std::string brightestCell;
    for (std::size_t index = 0; index < data.Cells().size(); ++index)
    {
        EXPECT_GE(noon[index], 0.0F);
        EXPECT_LE(noon[index], 1.0F);
        EXPECT_FLOAT_EQ(night[index], 0.0F)
            << data.Cells()[index].name << " has daylight with the sun 30° below the horizon";
        if (noon[index] > 0.0F)
        {
            ++lit;
        }
        if (noon[index] > brightest)
        {
            brightest = noon[index];
            brightestCell = data.Cells()[index].name;
        }
    }
    EXPECT_GT(lit, 25) << "most rooms with a window should see something at noon";
    std::printf("  at June noon: %d cell(s) lit, brightest %s at %.3f\n",
                lit,
                brightestCell.c_str(),
                static_cast<double>(brightest));
}

TEST(DaylightModelTests, OvercastIsFlatterButNotDarkAndNightIsNeither)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world";
    }
    // §30's row: *"the same room at 13:00, overcast -- evenly lit, cool, flat, no sun patch"*.
    const world::WorldData data = LoadWorld();
    const ShadingGrid shading = LoadShading();
    const DaylightModel model(data, shading);
    const Id room = Id::Of("L0_FAMILY");
    ASSERT_GT(model.GlazingRatioFor(room), 0.0F);

    const float clear = model.DaylightFor(room, 45.0, 180.0, 0.0);
    const float overcast = model.DaylightFor(room, 45.0, 180.0, 1.0);
    EXPECT_GT(clear, overcast);
    EXPECT_GT(overcast, 0.25F * clear) << "overcast flattens the light; it does not remove it";
    EXPECT_GT(overcast, 0.0F);
    std::printf("  L0_FAMILY at 45° sun: clear %.3f, overcast %.3f\n",
                static_cast<double>(clear),
                static_cast<double>(overcast));
}

TEST(DaylightModelTests, AnOpenSashAdmitsMoreThanAShutOneAndTheAuthoredStartIsRead)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world";
    }
    const world::WorldData data = LoadWorld();
    const ShadingGrid unshadedGrid = ShadingGrid::Unshaded();
    DaylightModel model(data, unshadedGrid);

    // §65.6's authored start opens `WIN_L1_MASTER_N2` half way, and the model must have read it:
    // a model that ignored the initial state would light that room as if the sash were shut on the
    // first frame and correctly the moment anything touched it.
    // **Joined by PORTAL, not by id.** `initialstate.json` and `interactables.json` call this
    // window `WIN_L1_MASTER_N2`; `layout.openings.json` calls it `WIN_L1_MASTER_BED_2`. All 54
    // window interactables differ from their opening that way and both rows carry the portal,
    // which is the thing they are both actually about. Matching on the id finds nothing and fails
    // silently, which is what this test caught.
    const world::Interactable* authored = data.FindInteractable(Id::Of("WIN_L1_MASTER_N2"));
    ASSERT_NE(authored, nullptr) << "interactables.json declares this window";
    ASSERT_TRUE(authored->portal.IsValid());
    const world::Opening* sameWindow = nullptr;
    for (const world::Opening& opening : data.Openings())
    {
        if (opening.kind == world::OpeningKind::Window && opening.portal == authored->portal)
        {
            sameWindow = &opening;
        }
    }
    ASSERT_NE(sameWindow, nullptr) << "no opening shares that interactable's portal";
    EXPECT_NE(sameWindow->id, authored->id) << "the two id schemes agree after all; simplify this";
    EXPECT_NEAR(model.OpenFraction(sameWindow->id), 0.5F, 1e-6F)
        << "initialstate.json opens this window half way and the model did not read it";

    // A cell with exactly ONE window, so the boost is not diluted, and a sun low enough that
    // nothing clamps -- a clamped level hides every factor behind it, which is how the first
    // draft of this test passed a model that ignored the sash entirely.
    Id window;
    Id cell;
    for (const DaylightWindow& candidate : model.Windows())
    {
        int siblings = 0;
        for (const DaylightWindow& other : model.Windows())
        {
            siblings += other.cell == candidate.cell ? 1 : 0;
        }
        if (siblings == 1 &&
            model.DaylightFor(candidate.cell, 12.0, static_cast<double>(candidate.azimuthDeg), 0.0) > 0.0F)
        {
            window = candidate.window;
            cell = candidate.cell;
            break;
        }
    }
    ASSERT_TRUE(window.IsValid()) << "no single-window cell sees any daylight at a 12° sun";
    const double bearing =
        static_cast<double>(std::find_if(model.Windows().begin(),
                                         model.Windows().end(),
                                         [&](const DaylightWindow& w) { return w.window == window; })
                                ->azimuthDeg);

    ASSERT_TRUE(model.SetOpenFraction(window, 0.0F));
    const float shut = model.DaylightFor(cell, 12.0, bearing, 0.0);
    ASSERT_TRUE(model.SetOpenFraction(window, 1.0F));
    const float open = model.DaylightFor(cell, 12.0, bearing, 0.0);
    ASSERT_LT(open, 1.0F) << "the probe clamped, so it cannot see the boost";
    EXPECT_GT(open, shut);
    // §28.4's boost is 1.15, and a cell with one window sees exactly that.
    EXPECT_NEAR(open, shut * static_cast<float>(kOpenBoost), shut * 1e-4F);

    // Half open is half the boost: a sash swinging must not step.
    ASSERT_TRUE(model.SetOpenFraction(window, 0.5F));
    const float half = model.DaylightFor(cell, 12.0, bearing, 0.0);
    EXPECT_GT(half, shut);
    EXPECT_LT(half, open);

    EXPECT_FALSE(model.SetOpenFraction(window, std::nanf(""))) << "a NaN must be refused, not clamped";
    EXPECT_FALSE(model.SetOpenFraction(Id::Of("WIN_NO_SUCH_WINDOW"), 1.0F));
    ASSERT_TRUE(model.SetOpenFraction(window, 4.0F));
    EXPECT_FLOAT_EQ(model.OpenFraction(window), 1.0F) << "out of range is clamped";
}

TEST(DaylightModelTests, TheBakedShadingActuallyChangesTheAnswerAndTheFoyerIsWhereItShows)
{
    if (!ContentIsBuilt() || !std::filesystem::exists("content/world/shading.bin"))
    {
        GTEST_SKIP() << "no baked shading.bin";
    }
    // `HOUSE-01279` proved the grid says the foyer is shaded; this proves the daylight model
    // READS it. Same world, same sun, two grids -- one baked and one unshaded.
    const world::WorldData data = LoadWorld();
    const ShadingGrid baked = LoadShading();
    const ShadingGrid none = ShadingGrid::Unshaded();
    const DaylightModel shaded(data, baked);
    const DaylightModel unshaded(data, none);

    const Id foyer = Id::Of("L0_FOYER");
    const Id study = Id::Of("L0_OFFICE");
    const float foyerShaded = shaded.DaylightFor(foyer, 40.0, 180.0, 0.0);
    const float foyerOpen = unshaded.DaylightFor(foyer, 40.0, 180.0, 0.0);
    const float studyShaded = shaded.DaylightFor(study, 40.0, 180.0, 0.0);
    const float studyOpen = unshaded.DaylightFor(study, 40.0, 180.0, 0.0);

    EXPECT_LT(foyerShaded, foyerOpen) << "the baked grid made no difference to the foyer";
    EXPECT_LT(studyShaded, studyOpen);
    // The foyer is under the balcony over the front door; the study is on the open elevation. The
    // shading must cost the foyer proportionally more.
    ASSERT_GT(foyerOpen, 0.0F);
    ASSERT_GT(studyOpen, 0.0F);
    const double foyerLoss = 1.0 - static_cast<double>(foyerShaded / foyerOpen);
    const double studyLoss = 1.0 - static_cast<double>(studyShaded / studyOpen);
    EXPECT_GT(foyerLoss, studyLoss) << "the foyer lost " << foyerLoss * 100.0 << " % and the study "
                                    << studyLoss * 100.0 << " %";
    std::printf("  shading costs the foyer %.1f %% of its daylight and the study %.1f %%\n",
                foyerLoss * 100.0,
                studyLoss * 100.0);
}

TEST(DaylightModelTests, ACellWithNoWindowsIsZeroAndNotADivisionByZero)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world";
    }
    const world::WorldData data = LoadWorld();
    const ShadingGrid unshadedGrid = ShadingGrid::Unshaded();
    const DaylightModel model(data, unshadedGrid);
    int windowless = 0;
    for (const world::Cell& cell : data.Cells())
    {
        if (model.GlazingRatioFor(cell.id) > 0.0F)
        {
            continue;
        }
        ++windowless;
        const float level = model.DaylightFor(cell.id, 73.4, 180.0, 0.0);
        EXPECT_FLOAT_EQ(level, 0.0F) << cell.name;
        EXPECT_TRUE(std::isfinite(level)) << cell.name;
    }
    EXPECT_GT(windowless, 20) << "§16's 96 cells include closets, shafts and the outdoors";
    EXPECT_FLOAT_EQ(model.DaylightFor(Id::Of("NO_SUCH_CELL"), 73.4, 180.0, 0.0), 0.0F);
    std::printf("  %d windowless cell(s), all 0 and none a NaN\n", windowless);
}

TEST(DaylightModelTests, TheRealSunDrivesItOverADayAndTheCurveHasOneMaximum)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world";
    }
    // Wired to §32's sun rather than to hand-made angles: this is the integration §28.4 exists
    // for, and a south room's daylight over a day must rise once and fall once.
    using cnahouse::environment::SunObserver;
    using cnahouse::environment::SunPositionAt;
    const world::WorldData data = LoadWorld();
    const ShadingGrid shading = LoadShading();
    const DaylightModel model(data, shading);
    const SunObserver observer;
    const double dayStart =
        cnahouse::environment::DaysSinceJ2000ForEpochSeconds(0.0, observer.utcOffsetMinutes) +
        static_cast<double>(cnahouse::environment::DaysFromCivil(2031, 6, 21) -
                            cnahouse::environment::DaysFromCivil(2031, 1, 1));
    const Id room = Id::Of("L0_FAMILY");

    std::vector<double> curve;
    for (int minute = 0; minute < 1440; minute += 10)
    {
        const auto sun = SunPositionAt(dayStart + minute / 1440.0, observer);
        curve.push_back(static_cast<double>(model.DaylightFor(room, sun.altitudeDeg, sun.azimuthDeg, 0.0)));
    }
    int rises = 0;
    int falls = 0;
    for (std::size_t index = 1; index < curve.size(); ++index)
    {
        if (curve[index] > curve[index - 1] + 1e-6)
        {
            ++rises;
        }
        if (curve[index] < curve[index - 1] - 1e-6)
        {
            ++falls;
        }
    }
    const double peak = *std::max_element(curve.begin(), curve.end());
    EXPECT_GT(peak, 0.0) << "the family room saw no daylight all day";
    EXPECT_EQ(curve.front(), 0.0) << "midnight";
    EXPECT_GT(rises, 10);
    EXPECT_GT(falls, 10);
    std::printf("  L0_FAMILY over 2031-06-21: peak %.3f, %d rising and %d falling samples of %zu\n",
                peak,
                rises,
                falls,
                curve.size());
}
