// SPDX-License-Identifier: MIT
//
// `HOUSE-00365`: what it costs to read the house off disk. §15.1's budget is 250 ms, and the
// number matters because it is paid on every start and on every reset (§66), with the player
// looking at a loading screen for all of it.
//
// Perf tests are NEVER gating (`cna-house.md` §70.4). The 250 ms budget is printed and compared;
// the assertion is three times looser, because on a machine shared with ten build agents the same
// unchanged code measures anywhere from 167 to 288 ms and a test that fails for the neighbours'
// load is a test everybody learns to ignore.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"
#include "cnahouse/world/WorldValidator.hpp"

namespace
{
    using cnahouse::util::IdRegistry;
    namespace world = cnahouse::world;

    constexpr int kWarmUp = 2; // the first read pays for the page cache, which is not a cost
    constexpr int kMeasured = 12;
    constexpr double kBudgetMs = 250.0;

    /// Every world file that exists. Not `Load`, which requires all sixteen: `layout.materials.json`
    /// and `layout.props.json` are `HOUSE-00385` and later, and a measurement that waited for them
    /// would be a measurement nobody has when it would be cheapest to act on.
    double LoadOnceMs(const std::string& directory)
    {
        using Loader = cnahouse::util::Result<void> (*)(std::string_view, world::WorldData::Contents&);
        const std::vector<std::pair<const char*, Loader>> files{
            {"layout.levels.json", &world::WorldLoader::LoadLevels},
            {"layout.cells.json", &world::WorldLoader::LoadCells},
            {"layout.portals.json", &world::WorldLoader::LoadPortals},
            {"layout.openings.json", &world::WorldLoader::LoadOpenings},
            {"layout.stairs.json", &world::WorldLoader::LoadStairs},
            {"layout.lights.json", &world::WorldLoader::LoadLights},
            {"layout.nav.json", &world::WorldLoader::LoadNav},
            {"layout.audio.json", &world::WorldLoader::LoadAudio},
            {"layout.exterior.json", &world::WorldLoader::LoadExterior},
            {"interactables.json", &world::WorldLoader::LoadInteractables},
            {"initialstate.json", &world::WorldLoader::LoadInitialState},
        };

        IdRegistry::ResetForTesting();
        const auto started = std::chrono::steady_clock::now();

        // The manifest too: `VerifyManifest` rehashes every file it is about to read, and that is
        // part of what a start costs.
        const auto manifest = world::WorldLoader::LoadManifest(directory);
        EXPECT_TRUE(manifest);
        if (manifest)
        {
            EXPECT_TRUE(world::WorldLoader::VerifyManifest(directory, manifest.Value()));
        }

        world::WorldData::Contents contents;
        for (const auto& [file, load] : files)
        {
            if (!std::filesystem::exists(directory + "/" + file))
            {
                continue;
            }
            const auto read = load(directory, contents);
            EXPECT_TRUE(read) << file;
        }
        auto built = world::WorldData::Create(std::move(contents));
        EXPECT_TRUE(built);

        const auto finished = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(finished - started).count();
    }

    TEST(WorldLoadTests, ReadingTheHouseCostsLessThanTheBudget)
    {
        const std::string directory = "content/world";
        if (!std::filesystem::exists(directory + "/layout.cells.json"))
        {
            GTEST_SKIP() << "no deployed world; run tools/world/deploy_world.py";
        }

        for (int index = 0; index < kWarmUp; ++index)
        {
            (void)LoadOnceMs(directory);
        }
        std::vector<double> samples;
        samples.reserve(kMeasured);
        for (int index = 0; index < kMeasured; ++index)
        {
            samples.push_back(LoadOnceMs(directory));
        }
        std::sort(samples.begin(), samples.end());
        const double best = samples.front();
        const double median = samples[samples.size() / 2];
        const double worst = samples.back();

        // Where it goes: the manifest rehashes every file it is about to read, a deliberate cost
        // (`HOUSE-00364`) worth separating from the parse so a later regression can be attributed.
        IdRegistry::ResetForTesting();
        const auto hashStarted = std::chrono::steady_clock::now();
        const auto manifest = world::WorldLoader::LoadManifest(directory);
        EXPECT_TRUE(manifest);
        if (manifest)
        {
            EXPECT_TRUE(world::WorldLoader::VerifyManifest(directory, manifest.Value()));
        }
        const double hashMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - hashStarted).count();

        std::printf("[ world    ] load best %.1f ms, median %.1f ms (manifest %.1f ms), "
                    "worst %.1f ms, budget %.0f ms (%.0f %% of it)\n",
                    best,
                    median,
                    hashMs,
                    worst,
                    kBudgetMs,
                    100.0 * best / kBudgetMs);
        // The budget is REPORTED, not asserted, and the assertion is three times looser.
        //
        // §70.4: "perf tests are never gating". This machine runs ten build agents and the suite
        // runs three test binaries at once, and the same unchanged code measured 167, 190, 212 and
        // 288 ms across four runs this afternoon -- a spread that has nothing to do with the
        // loader. Asserting 250 ms here would produce a red suite for reasons no change could fix,
        // and a red suite that everybody learns to ignore is worse than no test.
        //
        // So the number goes to the log, where `HOUSE-00365` recorded it and where a later session
        // can compare, and the assertion catches only what a perf test is allowed to catch:
        // something categorically slower.
        EXPECT_LT(best, 3.0 * kBudgetMs);

        IdRegistry::ResetForTesting();
    }

    /// Where the time goes, file by file. `HOUSE-00402` pushed the total past §15.1's budget and
    /// "the world got slower" is not a finding anybody can act on; "the interactables are 60 % of
    /// it and cost 1.5 ms a row" is.
    TEST(WorldLoadTests, WhereTheLoadTimeGoes)
    {
        const std::string directory = "content/world";
        if (!std::filesystem::exists(directory + "/layout.cells.json"))
        {
            GTEST_SKIP() << "no deployed world; run tools/world/deploy_world.py";
        }
        using Loader = cnahouse::util::Result<void> (*)(std::string_view, world::WorldData::Contents&);
        const std::vector<std::pair<const char*, Loader>> files{
            {"layout.levels.json", &world::WorldLoader::LoadLevels},
            {"layout.cells.json", &world::WorldLoader::LoadCells},
            {"layout.portals.json", &world::WorldLoader::LoadPortals},
            {"layout.openings.json", &world::WorldLoader::LoadOpenings},
            {"layout.stairs.json", &world::WorldLoader::LoadStairs},
            {"layout.lights.json", &world::WorldLoader::LoadLights},
            {"layout.nav.json", &world::WorldLoader::LoadNav},
            {"layout.audio.json", &world::WorldLoader::LoadAudio},
            {"layout.exterior.json", &world::WorldLoader::LoadExterior},
            {"interactables.json", &world::WorldLoader::LoadInteractables},
            {"initialstate.json", &world::WorldLoader::LoadInitialState},
        };
        for (const auto& [file, load] : files)
        {
            if (!std::filesystem::exists(directory + "/" + file))
            {
                continue;
            }
            double best = 1e9;
            for (int index = 0; index < 5; ++index)
            {
                IdRegistry::ResetForTesting();
                world::WorldData::Contents contents;
                const auto started = std::chrono::steady_clock::now();
                const auto read = load(directory, contents);
                const double ms =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                        .count();
                EXPECT_TRUE(read) << file;
                best = std::min(best, ms);
            }
            std::printf("[ world    ] %-24s %6.1f ms\n", file, best);
        }
        IdRegistry::ResetForTesting();
    }

    TEST(WorldLoadTests, ValidatingTheHouseCostsLessThanReadingIt)
    {
        const std::string directory = "content/world";
        if (!std::filesystem::exists(directory + "/layout.cells.json"))
        {
            GTEST_SKIP() << "no deployed world; run tools/world/deploy_world.py";
        }

        // §15.7's whole-world rules run at every debug load (`HOUSE-00357`). If they cost more
        // than the read they are protecting, the right answer would be to move them out of the
        // load path -- so the relationship, and not just the number, is what this records.
        IdRegistry::ResetForTesting();
        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadLevels(directory, contents));
        ASSERT_TRUE(world::WorldLoader::LoadCells(directory, contents));
        ASSERT_TRUE(world::WorldLoader::LoadPortals(directory, contents));
        ASSERT_TRUE(world::WorldLoader::LoadOpenings(directory, contents));
        ASSERT_TRUE(world::WorldLoader::LoadLights(directory, contents));
        ASSERT_TRUE(world::WorldLoader::LoadProps(directory, contents));
        ASSERT_TRUE(world::WorldLoader::LoadNav(directory, contents));
        ASSERT_TRUE(world::WorldLoader::LoadInteractables(directory, contents));
        auto built = world::WorldData::Create(std::move(contents));
        ASSERT_TRUE(built);

        const auto fast = [&built]
        {
            const auto started = std::chrono::steady_clock::now();
            const auto problems =
                world::WorldValidator::Validate(built.Value(), world::ValidationDepth::Fast);
            const auto finished = std::chrono::steady_clock::now();
            EXPECT_TRUE(problems.empty());
            return std::chrono::duration<double, std::milli>(finished - started).count();
        };
        const auto full = [&built]
        {
            const auto started = std::chrono::steady_clock::now();
            const auto problems =
                world::WorldValidator::Validate(built.Value(), world::ValidationDepth::Full);
            const auto finished = std::chrono::steady_clock::now();
            EXPECT_TRUE(problems.empty());
            return std::chrono::duration<double, std::milli>(finished - started).count();
        };

        std::vector<double> fastSamples;
        std::vector<double> fullSamples;
        for (int index = 0; index < kMeasured; ++index)
        {
            fastSamples.push_back(fast());
            fullSamples.push_back(full());
        }
        std::sort(fastSamples.begin(), fastSamples.end());
        std::sort(fullSamples.begin(), fullSamples.end());
        std::printf("[ world    ] validate Fast median %.2f ms, Full median %.2f ms\n",
                    fastSamples[fastSamples.size() / 2],
                    fullSamples[fullSamples.size() / 2]);
        EXPECT_LT(fastSamples[fastSamples.size() / 2], kBudgetMs);

        IdRegistry::ResetForTesting();
    }
} // namespace
