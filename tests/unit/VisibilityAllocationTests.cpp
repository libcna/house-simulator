// SPDX-License-Identifier: MIT
//
// `HOUSE-00695`'s acceptance: *"no allocation in the steady state; the work queue is a
// fixed-capacity ring."*
//
// **Asserted with a counting `operator new`, because the claim is about `malloc` and nothing else
// can see `malloc`.** A test that measured TIME would pass a walk that allocated on a warm cache
// and fail one that did not on a busy machine; a test that read `capacity()` would assert the
// shape of one container rather than the property. Replacing the global allocator is program-wide
// by definition -- every other test in this binary allocates through the counter too -- which
// costs one relaxed increment and buys the only direct evidence there is.
//
// **The steady state is the SECOND run of a pose, not the first.** The first fills the visible
// list and warms whatever the world data caches; §25's claim is that a frame after that one costs
// no allocation at all, which is what a 60 Hz walk actually consists of.
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <new>
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/ChunkCulling.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/world/ChunkReader.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

#include "unit/VisibilityPoses.hpp"

namespace cnahouse::testsupport
{
    /// @brief Every `operator new` in this binary, counted. Relaxed: it is read between calls,
    ///        never raced on.
    std::atomic<std::size_t> g_allocations{0};

    [[nodiscard]] std::size_t Allocations() noexcept
    {
        return g_allocations.load(std::memory_order_relaxed);
    }
} // namespace cnahouse::testsupport

namespace
{
    void* Allocate(std::size_t size)
    {
        cnahouse::testsupport::g_allocations.fetch_add(1, std::memory_order_relaxed);
        // Zero is a legal request and must still return a distinct pointer.
        void* memory = std::malloc(size == 0 ? 1 : size);
        if (memory == nullptr)
        {
            throw std::bad_alloc();
        }
        return memory;
    }

    void* AllocateAligned(std::size_t size, std::size_t alignment)
    {
        cnahouse::testsupport::g_allocations.fetch_add(1, std::memory_order_relaxed);
        // `aligned_alloc` requires a size that is a multiple of the alignment.
        const std::size_t rounded = ((size == 0 ? 1 : size) + alignment - 1) / alignment * alignment;
        void* memory = std::aligned_alloc(alignment, rounded);
        if (memory == nullptr)
        {
            throw std::bad_alloc();
        }
        return memory;
    }
} // namespace

void* operator new(std::size_t size)
{
    return Allocate(size);
}

void* operator new[](std::size_t size)
{
    return Allocate(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    cnahouse::testsupport::g_allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(size == 0 ? 1 : size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    cnahouse::testsupport::g_allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(size == 0 ? 1 : size);
}

void* operator new(std::size_t size, std::align_val_t alignment)
{
    return AllocateAligned(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment)
{
    return AllocateAligned(size, static_cast<std::size_t>(alignment));
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, const std::nothrow_t&) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, const std::nothrow_t&) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::align_val_t) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, std::align_val_t) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t, std::align_val_t) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept
{
    std::free(memory);
}

namespace
{
    namespace world = cnahouse::world;

    using cnahouse::app::FrameContext;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::testsupport::Allocations;
    using cnahouse::testsupport::kVisibilityPoses;
    using cnahouse::testsupport::VisibilityPose;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ChunkCuller;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::kMaxQueuedCones;
    using cnahouse::visibility::VisibilitySystem;
    using Microsoft::Xna::Framework::Vector3;

    bool ContentIsBuilt()
    {
        return std::filesystem::exists("content/world/layout.cells.json");
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

    CameraView Standing(std::string_view cellName, float x, float y, float z, float yawDegrees)
    {
        CameraView view;
        view.cell = cnahouse::util::Intern(std::string(cellName));
        PlayerState state;
        state.position = Vector3(x, y + state.Rise(), z);
        state.yaw = yawDegrees * 3.14159265F / 180.0F;
        FirstPersonCamera camera;
        camera.SetAspect(16.0F / 9.0F);
        camera.Update(state, kPlayerEyeHeight, 0.0F);
        view.eye = camera.Pose().eye;
        view.viewProjection = camera.View() * camera.Projection();
        view.frustum = ClipFrustum(camera.Frustum());
        view.nearPlane = camera.Frustum().getNearProperty();
        view.farPlane = camera.Frustum().getFarProperty();
        return view;
    }

    /// The centre of @p cell's first footprint, which is somewhere a body always fits.
    CameraView StandingInTheMiddleOf(const world::WorldData& data, const world::Cell& cell, float yaw)
    {
        const world::Level* level = data.FindLevel(cell.level);
        const world::Footprint& box = cell.boxes.front();
        return Standing(IdRegistry::NameOf(cell.id),
                        (box.minX + box.maxX) * 0.5F,
                        level == nullptr ? 0.0F : level->ffl,
                        (box.minZ + box.maxZ) * 0.5F,
                        yaw);
    }

} // namespace

TEST(VisibilityAllocationTests, TheCounterCountsWhatItClaimsTo)
{
    // The test the other two rest on: a counter that never moved would make them both pass by
    // saying nothing. One deliberate allocation, one deliberate release.
    const std::size_t before = Allocations();
    {
        std::vector<int> forced;
        forced.reserve(64);
        EXPECT_EQ(forced.capacity(), 64U);
    }
    EXPECT_GT(Allocations(), before) << "the global operator new was not replaced";
}

TEST(VisibilityAllocationTests, TheWalkAllocatesNothingInTheSteadyState)
{
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);

    FrameContext frame;
    int worstPeak = 0;
    std::string worstPose;
    for (const VisibilityPose& pose : kVisibilityPoses)
    {
        for (const world::Portal& portal : data.Portals())
        {
            system.SetAperture(portal.id, pose.doorsOpen ? 1.0F : 0.0F);
        }
        const CameraView view = Standing(pose.cell, pose.x, pose.y, pose.z, pose.yawDegrees);
        system.SetCamera(view);

        // The first run is the one allowed to grow anything that grows.
        frame.frameIndex = 1;
        system.Update(frame);

        const std::size_t before = Allocations();
        for (int again = 0; again < 8; ++again)
        {
            frame.frameIndex = static_cast<std::uint64_t>(again) + 2u;
            system.Update(frame);
        }
        EXPECT_EQ(Allocations(), before)
            << "the walk allocated " << (Allocations() - before) << " time(s) at pose " << pose.name;

        if (system.Stats().queuePeak > worstPeak)
        {
            worstPeak = system.Stats().queuePeak;
            worstPose = std::string(pose.name);
        }
        EXPECT_EQ(system.Stats().queueDropped, 0)
            << "the ring ran out of room at pose " << pose.name << ", so a cone was never expanded";
    }
    std::printf("  the deepest work queue over §25.8's 24 poses: %d cone(s) at %s, of "
                "kMaxQueuedCones = %zu\n",
                worstPeak,
                worstPose.c_str(),
                kMaxQueuedCones);
    EXPECT_GT(worstPeak, 0) << "no pose queued anything, so the peak says nothing";
}

TEST(VisibilityAllocationTests, TheRingCoversTheWholeHouseWithEveryDoorOpen)
{
    // The capacity is a number in a header, and this is what makes it a MEASURED one: every cell
    // in the house at four headings with every door open, which is the arrangement no player will
    // make and every bound must survive.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }

    FrameContext frame;
    frame.frameIndex = 1;
    int worstPeak = 0;
    std::string worstCell;
    int dropped = 0;
    int poses = 0;
    for (const world::Cell& cell : data.Cells())
    {
        if (IdRegistry::NameOf(cell.id).starts_with("CELL_"))
        {
            continue; // §65's container interiors are not places a body stands.
        }
        for (const float yaw : {0.0F, 90.0F, 180.0F, 270.0F})
        {
            system.SetCamera(StandingInTheMiddleOf(data, cell, yaw));
            system.Update(frame);
            ++poses;
            dropped += system.Stats().queueDropped;
            if (system.Stats().queuePeak > worstPeak)
            {
                worstPeak = system.Stats().queuePeak;
                worstCell = std::string(IdRegistry::NameOf(cell.id));
            }
        }
    }

    std::printf("  over %d pose(s) with every door open, the deepest work queue is %d cone(s) "
                "(%s), %.0f %% of kMaxQueuedCones = %zu\n",
                poses,
                worstPeak,
                worstCell.c_str(),
                100.0 * static_cast<double>(worstPeak) / static_cast<double>(kMaxQueuedCones),
                kMaxQueuedCones);
    EXPECT_EQ(dropped, 0) << "the ring overflowed somewhere in the house";
    // Room to spare, and said as a number rather than as a hope: the capacity is there to absorb
    // a house this one is not, so half of it standing empty at the worst pose is the point.
    EXPECT_LT(static_cast<std::size_t>(worstPeak) * 2u, kMaxQueuedCones)
        << "the worst frontier is over half the ring, which is too close to the edge";
}
