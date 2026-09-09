// SPDX-License-Identifier: MIT
//
// `HOUSE-00694`: what §25 costs in §70.6's ten worst-case scenarios, against §71.2's visibility
// row -- 0.55 ms typical, 1.20 ms worst case, 1.6 ms hard fail, inside a 16.67 ms frame.
//
// **The ten scenarios are §70.6's own, named as it names them**, because a perf test whose
// scenarios drifted from the ones the budget was written for measures a different house. What
// each row contributes to §25 is only its CAMERA POSE and its DOOR STATE: the visibility stage
// does not know it is raining, does not know a pet is being skinned, and does not know a cupboard
// is open. So the thunderstorm and the blizzard are an outdoor pose each, the pets are an indoor
// one, and the note beside each row says what the rest of that scenario is charged to instead.
//
// **Where §70.6 gives a heading, that heading is used; where it does not, all four cardinals are
// measured and the WORST is the row.** These are worst-case scenarios: a heading chosen by hand
// would be a heading chosen to pass.
//
// **What is measured is §25.1's steps 1-4** -- the portal walk, the per-cell chunk test and
// §25.6's exterior hierarchy -- because that is what §71.2's visibility row covers and what
// `CnaHouseGame` charges to `UpdateStage::Visibility`. Step 5, the sort, is measured too and
// reported separately: the game builds the render list inside `Draw`, so its cost belongs to the
// draw-submission row and adding it here would make visibility look 3× its size.
//
// **The exterior instances are synthetic and say so.** §25.6's ~4 100 real ones -- the vegetation
// of `HOUSE-00772`, the neighbourhood of `HOUSE-00852` -- are Phase 10 content that does not exist
// yet, and a hierarchy of nothing measures nothing. The garden below is 4 100 instances in §25.6's
// eight categories spread over §10.3's property and neighbourhood extents, which is the right SIZE
// and shape of problem; when the real set lands, this number is the one to re-take.
//
// Perf tests are NEVER gating (§70.4): this machine runs ten build agents, and a test that fails
// for the neighbours' load is a test everybody learns to ignore. The budget is printed and
// compared; the assertion catches only something categorically slower.
//
// **The build type is printed with the number, because a number without it means nothing** -- and
// a `Debug` build here is an UPPER BOUND on the shipped cost: `-O0`, with CNA's `Vector3` and
// `Plane` operators called out of line for every plane test in the walk. An optimised build of the
// same code cannot be slower.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/ChunkCulling.hpp"
#include "cnahouse/visibility/ExteriorCulling.hpp"
#include "cnahouse/visibility/RenderList.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/world/ChunkReader.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

namespace
{
    namespace world = cnahouse::world;

    using cnahouse::app::FrameContext;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ChunkCuller;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::ExteriorBvh;
    using cnahouse::visibility::ExteriorCones;
    using cnahouse::visibility::ExteriorCuller;
    using cnahouse::visibility::ExteriorInstance;
    using cnahouse::visibility::PropCategory;
    using cnahouse::visibility::RenderList;
    using cnahouse::visibility::VisibilitySystem;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    /// §71.2's visibility row, in milliseconds a frame.
    constexpr double kTypicalMs = 0.55;
    constexpr double kWorstMs = 1.20;
    constexpr double kHardFailMs = 1.60;
    /// §71.2's draw-submission row, which step 5 is charged against rather than the one above.
    constexpr double kSubmissionTypicalMs = 1.60;

    constexpr float kDegrees = 3.14159265F / 180.0F;

    /// Which doors the scenario has open. §65.6 starts every door shut; §70.6's first two rows
    /// open all of them, and its basement row opens the basement's.
    enum class Doors
    {
        AllShut,
        AllOpen,
        BasementOnly,
    };

    struct Scenario
    {
        /// §70.6's row, as it words it.
        std::string_view name;
        std::string_view cell;
        /// §14's heading in degrees, used only when `sweep` is false.
        float yawDegrees;
        float pitchDegrees;
        float fovDegrees;
        Doors doors;
        /// True where §70.6 names no heading: all four cardinals are measured and the worst wins.
        bool sweep;
        /// What the rest of that scenario stresses, and where it is charged.
        std::string_view rest;
    };

    /// §70.6's first nine. The tenth is a walk rather than a pose and has its own test.
    constexpr std::array<Scenario, 9> kScenarios{{
        {"all 62 doors open, L0_FOYER looking north",
         "L0_FOYER",
         0.0F,
         0.0F,
         cnahouse::player::kDefaultFovDegrees,
         Doors::AllOpen,
         false,
         "maximum portal traversal and visible-cell count -- all of it visibility's"},
        {"all doors open, L0_STAIR_MAIN looking up",
         "L0_STAIR_MAIN",
         0.0F,
         60.0F,
         cnahouse::player::kDefaultFovDegrees,
         Doors::AllOpen,
         false,
         "three floors visible at once -- all of it visibility's"},
        // "The widest view" is §44's widest lens, which is the camera setting that reaches most of
        // the neighbourhood. North is towards the house: the road corridor is §10.3's z 0..+13.4
        // and the house is at z -27..-14.
        {"the road at the widest view",
         "EXT_ROAD",
         0.0F,
         0.0F,
         cnahouse::player::kMaxFovDegrees,
         Doors::AllShut,
         false,
         "neighbourhood, vegetation, terrain, LOD -- §25.6's hierarchy is visibility's, the LOD "
         "selection is §26's"},
        {"L0_KITCHEN at noon with every container open",
         "L0_KITCHEN",
         0.0F,
         0.0F,
         cnahouse::player::kDefaultFovDegrees,
         Doors::AllOpen,
         true,
         "densest interior; the container leaves are props, charged to draw submission"},
        {"L3_STORE_W with the attic light on",
         "L3_STORE_W",
         0.0F,
         0.0F,
         cnahouse::player::kDefaultFovDegrees,
         Doors::AllOpen,
         true,
         "most dressing props, worst headroom; the light is §24's"},
        {"B1_HALL with every basement door open",
         "B1_HALL",
         0.0F,
         0.0F,
         cnahouse::player::kDefaultFovDegrees,
         Doors::BasementOnly,
         true,
         "deepest chain with no daylight -- all of it visibility's"},
        {"thunderstorm at 21:00 on the terrace",
         "EXT_TERRACE",
         0.0F,
         0.0F,
         cnahouse::player::kDefaultFovDegrees,
         Doors::AllShut,
         true,
         "particles, lightning, wetness and voices are §29's, §63's and §64's; the pose is ours"},
        {"blizzard at 03:00 on the road",
         "EXT_ROAD",
         0.0F,
         0.0F,
         cnahouse::player::kDefaultFovDegrees,
         Doors::AllShut,
         true,
         "snow particles, shells and fog are §29's; the pose is ours, at §44's default lens"},
        // §12's mirrors are in `L0_FOYER`, `L1_MASTER_CLOSET` and the gym's mirror WALL, which is
        // the one an avatar is seen full-length in. What the scenario stresses is skinning; what
        // it costs §25 is a basement room with two windows onto the west front yard.
        {"third person with both pets, the gym's mirror wall",
         "B1_GYM",
         0.0F,
         0.0F,
         cnahouse::player::kDefaultFovDegrees,
         Doors::AllOpen,
         true,
         "maximum skinning, which is §23's animation row"},
    }};

    constexpr int kWarmUp = 20;
    constexpr int kBlocks = 25;
    constexpr int kPerBlock = 4;

    const char* BuildType()
    {
#if defined(NDEBUG)
        return "optimised (NDEBUG)";
#else
        return "Debug (-O0, and CNA's Vector3/Plane operators are out of line)";
#endif
    }

    constexpr std::string_view kChunkPath = "content/world/chunks.bin";

    bool ContentIsBuilt()
    {
        return std::filesystem::exists("content/world/layout.cells.json") &&
               std::filesystem::exists(kChunkPath);
    }

    /// §17.4's chunks, read through a `FileStream` rather than the title container.
    ///
    /// The other suites open this file with `ReadFromTitle`, which resolves against the RUNNING
    /// BINARY's directory -- and this binary is deliberately run from more than one build tree: the
    /// numbers below are taken from an optimised build, and the debug tree beside it is what the
    /// day-to-day suite uses. A repository-relative path is the one thing both agree on, and it is
    /// what every other load in this file already uses.
    cnahouse::util::Result<world::ChunkLibrary> LoadChunks()
    {
        const std::unique_ptr<System::IO::FileStream> stream(new System::IO::FileStream(
            std::string(kChunkPath), System::IO::FileMode::Open, System::IO::FileAccess::Read));
        return world::ChunkReader::Read(*stream, kChunkPath);
    }

    world::WorldData LoadWorld()
    {
        world::WorldData::Contents contents;
        EXPECT_TRUE(world::WorldLoader::LoadLevels("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadCells("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadPortals("content/world", contents).HasValue());
        auto built = world::WorldData::Create(std::move(contents));
        EXPECT_TRUE(built) << built.Error().ToString();
        return std::move(built.Value());
    }

    /// A camera standing in the middle of @p cellName's first footprint, on that level's floor.
    ///
    /// The CENTRE rather than an authored point, because §70.6 names a room and a heading and
    /// nothing else: a hand-authored position is one more number to keep true, and the middle of a
    /// room is somewhere a body always fits.
    CameraView Standing(const world::WorldData& data,
                        std::string_view cellName,
                        float yawDegrees,
                        float pitchDegrees,
                        float fovDegrees)
    {
        CameraView view;
        view.cell = cnahouse::util::Intern(std::string(cellName));
        const world::Cell* cell = data.FindCell(view.cell);
        EXPECT_NE(cell, nullptr) << cellName;
        if (cell == nullptr)
        {
            return view;
        }
        const world::Level* level = data.FindLevel(cell->level);
        const world::Footprint& box = cell->boxes.front();

        PlayerState state;
        state.position = Vector3((box.minX + box.maxX) * 0.5F,
                                 (level == nullptr ? 0.0F : level->ffl) + state.Rise(),
                                 (box.minZ + box.maxZ) * 0.5F);
        state.yaw = yawDegrees * kDegrees;
        FirstPersonCamera camera;
        camera.SetAspect(16.0F / 9.0F);
        camera.SetFieldOfView(fovDegrees);
        camera.Update(state, kPlayerEyeHeight, pitchDegrees * kDegrees);

        view.eye = camera.Pose().eye;
        view.viewProjection = camera.View() * camera.Projection();
        view.frustum = ClipFrustum(camera.Frustum());
        view.nearPlane = camera.Frustum().getNearProperty();
        view.farPlane = camera.Frustum().getFarProperty();
        return view;
    }

    void SetDoors(const world::WorldData& data, VisibilitySystem& system, Doors doors)
    {
        for (const world::Portal& portal : data.Portals())
        {
            bool open = doors == Doors::AllOpen;
            if (doors == Doors::BasementOnly)
            {
                open = IdRegistry::NameOf(portal.cellA).starts_with("B1_") &&
                       IdRegistry::NameOf(portal.cellB).starts_with("B1_");
            }
            system.SetAperture(portal.id, open ? 1.0F : 0.0F);
        }
    }

    /// §25.6's ~4 100 instances, in its eight categories, over §10.3's extents.
    ///
    /// Deterministic (a fixed LCG), so two runs of this test measure the same garden and a
    /// regression is a regression rather than a different random layout. The four near categories
    /// are inside the property and the road corridor; the four neighbourhood ones are spread over
    /// the visual shell, which is what makes the distance cull do any work at all.
    std::vector<ExteriorInstance> AGardenAndANeighbourhood()
    {
        constexpr std::array<std::pair<PropCategory, int>, 8> kMix{{
            {PropCategory::SmallProp, 1200},
            {PropCategory::GardenFurniture, 200},
            {PropCategory::Fence, 700},
            {PropCategory::Tree, 500},
            {PropCategory::NeighbourhoodLod0, 300},
            {PropCategory::NeighbourhoodLod1, 400},
            {PropCategory::NeighbourhoodLod2, 500},
            {PropCategory::Impostor, 300},
        }};

        std::uint32_t state = 20260909u;
        auto next = [&state](float low, float high)
        {
            state = state * 1664525u + 1013904223u;
            const float unit = static_cast<float>(state >> 8u) / static_cast<float>(1u << 24u);
            return low + unit * (high - low);
        };

        std::vector<ExteriorInstance> instances;
        instances.reserve(4100u);
        int serial = 0;
        for (const auto& [category, count] : kMix)
        {
            const bool isNear = category == PropCategory::SmallProp ||
                                category == PropCategory::GardenFurniture ||
                                category == PropCategory::Fence || category == PropCategory::Tree;
            for (int i = 0; i < count; ++i)
            {
                ExteriorInstance instance;
                instance.id = cnahouse::util::Intern("EXT_INSTANCE_" + std::to_string(serial++));
                instance.category = category;
                // A tree is the big one and a small prop the little one; the box is what §25.6's
                // distance test measures to, so its SIZE matters and a point would flatter it.
                const float radius = category == PropCategory::Tree                ? 3.0F
                                     : category == PropCategory::NeighbourhoodLod0 ? 6.0F
                                     : category == PropCategory::SmallProp         ? 0.4F
                                                                                   : 2.0F;
                // §10.3: the property is x -22.5..+22.5, z -48..0, and the road corridor reaches
                // z +13.4; the neighbourhood shell is x -220..+220, z -260..+180.
                const Vector3 centre =
                    isNear ? Vector3(next(-35.0F, 35.0F), next(0.0F, 6.0F), next(-48.0F, 13.4F))
                           : Vector3(next(-220.0F, 220.0F), next(0.0F, 12.0F), next(-260.0F, 180.0F));
                instance.bounds =
                    BoundingBox(Vector3(centre.X - radius, centre.Y - radius, centre.Z - radius),
                                Vector3(centre.X + radius, centre.Y + radius, centre.Z + radius));
                instances.push_back(instance);
            }
        }
        return instances;
    }

    struct Measurement
    {
        double walkMs = 0.0;
        double chunkMs = 0.0;
        double exteriorMs = 0.0;
        double sortMs = 0.0;
        /// The slowest BLOCK's steps 1-4, which on a machine with ten build agents on it is as
        /// much a measurement of the neighbours as of the walk. Reported, never asserted.
        double worstMs = 0.0;
        int cells = 0;
        int chunks = 0;
        int instances = 0;
        int traversals = 0;
        /// §25.7's cones into the outdoors, which is what §25.6's hierarchy is walked once per.
        int cones = 0;
        bool conesDegraded = false;
        /// Instance-versus-cone tests. Larger than the instances drawn by however many cones
        /// retested what an earlier one had already accepted -- the number that says whether the
        /// cost is the garden or the number of windows onto it.
        int instancesTested = 0;
        float yawDegrees = 0.0F;

        [[nodiscard]] double StagesMs() const noexcept
        {
            return walkMs + chunkMs + exteriorMs;
        }
    };

    double Median(std::vector<double>& samples)
    {
        std::sort(samples.begin(), samples.end());
        return samples[samples.size() / 2];
    }

    Measurement MeasurePose(const world::WorldData& data,
                            const world::ChunkLibrary& library,
                            const ExteriorBvh& bvh,
                            VisibilitySystem& system,
                            ChunkCuller& chunkCuller,
                            const CameraView& view)
    {
        ExteriorCones cones;
        ExteriorCuller exterior;
        RenderList list;
        FrameContext frame;
        frame.frameIndex = 1;
        system.SetCamera(view);

        const auto pipeline = [&]
        {
            system.Update(frame);
            chunkCuller.Cull(system.Visible());
            cones.Collect(data, system.Visible(), view.frustum);
            exterior.Cull(bvh, cones.Cones(), view.eye);
            list.Clear();
            list.AddChunks(library, chunkCuller.Chunks(), view.eye);
            list.Sort();
        };
        for (int i = 0; i < kWarmUp; ++i)
        {
            pipeline();
        }

        using Clock = std::chrono::steady_clock;
        const auto ms = [](Clock::time_point from, Clock::time_point to)
        { return std::chrono::duration<double, std::milli>(to - from).count() / double{kPerBlock}; };

        std::vector<double> walk;
        std::vector<double> chunk;
        std::vector<double> ext;
        std::vector<double> sort;
        std::vector<double> stages;
        for (int block = 0; block < kBlocks; ++block)
        {
            const auto t0 = Clock::now();
            for (int i = 0; i < kPerBlock; ++i)
            {
                system.Update(frame);
            }
            const auto t1 = Clock::now();
            for (int i = 0; i < kPerBlock; ++i)
            {
                chunkCuller.Cull(system.Visible());
            }
            const auto t2 = Clock::now();
            for (int i = 0; i < kPerBlock; ++i)
            {
                cones.Collect(data, system.Visible(), view.frustum);
                exterior.Cull(bvh, cones.Cones(), view.eye);
            }
            const auto t3 = Clock::now();
            for (int i = 0; i < kPerBlock; ++i)
            {
                list.Clear();
                list.AddChunks(library, chunkCuller.Chunks(), view.eye);
                list.Sort();
            }
            const auto t4 = Clock::now();
            walk.push_back(ms(t0, t1));
            chunk.push_back(ms(t1, t2));
            ext.push_back(ms(t2, t3));
            sort.push_back(ms(t3, t4));
            stages.push_back(ms(t0, t3));
        }

        Measurement result;
        result.walkMs = Median(walk);
        result.chunkMs = Median(chunk);
        result.exteriorMs = Median(ext);
        result.sortMs = Median(sort);
        result.worstMs = *std::max_element(stages.begin(), stages.end());
        result.cells = static_cast<int>(system.Visible().size());
        result.chunks = chunkCuller.Statistics().chunksDrawn;
        result.instances = exterior.Statistics().instancesDrawn;
        result.traversals = system.Stats().portalsCrossed;
        result.cones = static_cast<int>(cones.Cones().size());
        result.conesDegraded = cones.Degraded();
        result.instancesTested = exterior.Statistics().instancesTested;
        return result;
    }

    /// One scenario: its own heading, or the worst of the four cardinals where §70.6 names none.
    Measurement MeasureScenario(const world::WorldData& data,
                                const world::ChunkLibrary& library,
                                const ExteriorBvh& bvh,
                                VisibilitySystem& system,
                                ChunkCuller& chunkCuller,
                                const Scenario& scenario)
    {
        SetDoors(data, system, scenario.doors);
        Measurement worst;
        const std::array<float, 4> cardinals{0.0F, 90.0F, 180.0F, 270.0F};
        const std::size_t headings = scenario.sweep ? cardinals.size() : 1u;
        for (std::size_t i = 0; i < headings; ++i)
        {
            const float yaw = scenario.sweep ? cardinals[i] : scenario.yawDegrees;
            const CameraView view =
                Standing(data, scenario.cell, yaw, scenario.pitchDegrees, scenario.fovDegrees);
            Measurement one = MeasurePose(data, library, bvh, system, chunkCuller, view);
            one.yawDegrees = yaw;
            if (one.StagesMs() > worst.StagesMs())
            {
                worst = one;
            }
        }
        return worst;
    }

} // namespace

TEST(VisibilityCostTests, TheNineFixedScenariosAgainstTheFrameBudget)
{
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    const world::WorldData data = LoadWorld();
    auto library = LoadChunks();
    ASSERT_TRUE(library) << library.Error().ToString();

    ExteriorBvh bvh;
    bvh.Build(AGardenAndANeighbourhood());
    VisibilitySystem system(data);
    ChunkCuller chunkCuller(*library);

    std::printf("[ visibility ] build %s\n"
                "[ visibility ] §71.2's visibility row: %.2f ms typical, %.2f ms worst, %.2f ms hard "
                "fail. §25.1 steps 1-4; step 5 (sort) is the draw-submission row's.\n"
                "[ visibility ] exterior hierarchy: %zu synthetic instances (§25.6's ~4 100; the real "
                "set is HOUSE-00772 and HOUSE-00852)\n",
                BuildType(),
                kTypicalMs,
                kWorstMs,
                kHardFailMs,
                bvh.Instances().size());
    std::printf("[ visibility ] %-44s %5s %5s %6s %7s %7s %7s %7s %6s %7s\n",
                "scenario",
                "yaw",
                "cells",
                "chunks",
                "walk",
                "chunk",
                "ext",
                "1-4",
                "% bud",
                "sort");

    std::vector<double> totals;
    const Scenario* worstScenario = nullptr;
    double worstTotal = 0.0;
    double worstSort = 0.0;
    for (const Scenario& scenario : kScenarios)
    {
        const Measurement measured = MeasureScenario(data, *library, bvh, system, chunkCuller, scenario);
        totals.push_back(measured.StagesMs());
        if (measured.StagesMs() > worstTotal)
        {
            worstTotal = measured.StagesMs();
            worstScenario = &scenario;
            worstSort = measured.sortMs;
        }
        std::printf("[ visibility ] %-44s %5.0f %5d %6d %7.4f %7.4f %7.4f %7.4f %5.0f%% %7.4f\n",
                    std::string(scenario.name).c_str(),
                    static_cast<double>(measured.yawDegrees),
                    measured.cells,
                    measured.chunks,
                    measured.walkMs,
                    measured.chunkMs,
                    measured.exteriorMs,
                    measured.StagesMs(),
                    100.0 * measured.StagesMs() / kTypicalMs,
                    measured.sortMs);
        std::printf("[ visibility ]     %d portal crossing(s); %d cone(s) outdoors%s, %d instance test(s) "
                    "for %d instance(s) drawn; worst block %.4f ms. The rest: %s\n",
                    measured.traversals,
                    measured.cones,
                    measured.conesDegraded ? " (capped, standing the camera's own frustum in)" : "",
                    measured.instancesTested,
                    measured.instances,
                    measured.worstMs,
                    std::string(scenario.rest).c_str());
    }

    ASSERT_NE(worstScenario, nullptr);
    const double typical = Median(totals);
    std::printf("[ visibility ] median scenario %.4f ms -- %.0f %% of §71.2's %.2f ms typical\n"
                "[ visibility ] worst scenario \"%s\" %.4f ms -- %.0f %% of its %.2f ms worst case\n"
                "[ visibility ] its step 5 (sort) %.4f ms -- %.0f %% of the %.2f ms draw-submission row\n",
                typical,
                100.0 * typical / kTypicalMs,
                kTypicalMs,
                std::string(worstScenario->name).c_str(),
                worstTotal,
                100.0 * worstTotal / kWorstMs,
                kWorstMs,
                worstSort,
                100.0 * worstSort / kSubmissionTypicalMs,
                kSubmissionTypicalMs);

    // Never gating (§70.4), and the categorical bound the other perf tests use: three times the
    // budget is not noise on a loaded machine, it is a different algorithm.
    EXPECT_LT(typical, 3.0 * kTypicalMs) << "the median scenario is categorically over §71.2's budget";
    EXPECT_LT(worstTotal, 3.0 * kWorstMs) << "the worst scenario is categorically over §71.2's budget";
}

TEST(VisibilityCostTests, TheNinetySecondWalkAgainstTheFrameBudget)
{
    // §70.6's tenth row. A walk is a DISTRIBUTION and not a pose, and the thing it stresses --
    // residency churn and cell transitions -- is exactly the property that makes a cached answer
    // useless: §25's walk is recomputed from the camera cell every frame, so what a 90-second
    // route costs is what the poses along it cost. Every cell of the house at four headings is a
    // superset of any route through it, which is why the whole house is walked here rather than
    // one authored path: a path is a sample, and a sample of a worst case is not a worst case.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    const world::WorldData data = LoadWorld();
    auto library = LoadChunks();
    ASSERT_TRUE(library) << library.Error().ToString();

    ExteriorBvh bvh;
    bvh.Build(AGardenAndANeighbourhood());
    VisibilitySystem system(data);
    ChunkCuller chunkCuller(*library);
    // Every door open: §70.6's walk is through the whole house, and a walk that opened doors as it
    // reached them would measure the shut half of the house for the first half of the route.
    SetDoors(data, system, Doors::AllOpen);

    ExteriorCones cones;
    ExteriorCuller exterior;
    FrameContext frame;
    frame.frameIndex = 1;

    std::vector<double> perPose;
    std::string worstCell;
    float worstYaw = 0.0F;
    double worstMs = 0.0;
    int poses = 0;
    for (const world::Cell& cell : data.Cells())
    {
        const std::string_view name = IdRegistry::NameOf(cell.id);
        // A route does not go through the fridge. §65's container interiors are cells so that
        // their contents have somewhere to live, not places a body stands.
        if (name.starts_with("CELL_"))
        {
            continue;
        }
        for (const float yaw : {0.0F, 90.0F, 180.0F, 270.0F})
        {
            const CameraView view = Standing(data, name, yaw, 0.0F, cnahouse::player::kDefaultFovDegrees);
            system.SetCamera(view);
            // Three samples, keep the FASTEST: one sample a pose on a machine running ten build
            // agents measures the neighbours. The fastest of three is the least disturbed run,
            // which is the cost of the pose rather than the cost of the afternoon.
            double best = 0.0;
            for (int sample = 0; sample < 3; ++sample)
            {
                const auto started = std::chrono::steady_clock::now();
                system.Update(frame);
                chunkCuller.Cull(system.Visible());
                cones.Collect(data, system.Visible(), view.frustum);
                exterior.Cull(bvh, cones.Cones(), view.eye);
                const double elapsed =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                        .count();
                best = sample == 0 ? elapsed : std::min(best, elapsed);
            }
            perPose.push_back(best);
            ++poses;
            if (best > worstMs)
            {
                worstMs = best;
                worstCell = std::string(name);
                worstYaw = yaw;
            }
        }
    }

    ASSERT_FALSE(perPose.empty());
    std::vector<double> sorted = perPose;
    const double median = Median(sorted);
    const double p95 = sorted[static_cast<std::size_t>(0.95 * static_cast<double>(sorted.size()))];

    std::printf("[ visibility ] build %s\n"
                "[ visibility ] the 90-second walk, as every cell at four headings: %d poses, "
                "median %.4f ms (%.0f %% of §71.2's %.2f ms typical), p95 %.4f ms, worst %.4f ms "
                "(%.0f %% of its %.2f ms worst case) at %s facing %.0f deg\n",
                BuildType(),
                poses,
                median,
                100.0 * median / kTypicalMs,
                kTypicalMs,
                p95,
                worstMs,
                100.0 * worstMs / kWorstMs,
                kWorstMs,
                worstCell.c_str(),
                static_cast<double>(worstYaw));

    EXPECT_LT(median, 3.0 * kTypicalMs) << "the median frame of the walk is categorically over budget";
    EXPECT_LT(worstMs, 3.0 * kWorstMs) << "the worst frame of the walk is categorically over budget";
}
