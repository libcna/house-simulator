// SPDX-License-Identifier: MIT
//
// `HOUSE-00562`. §71's `F9`: the collision shapes in the visible cells, the player capsule, the
// ground probe and the sweep the next step would run.
//
// An overlay is the one kind of code that is never noticed when it goes wrong -- nobody files a
// bug saying the debug view drew the wrong wall -- so what is asserted here is the CONTENT: which
// shapes were drawn, what colour they are, and that the numbers on the screen are the numbers the
// physics actually produced.
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/debug/PhysicsOverlay.hpp"
#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"

namespace
{
    using cnahouse::debug::PhysicsOverlay;
    using cnahouse::debug::PhysicsOverlayBody;
    using cnahouse::debug::Segment;
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CellSweepHit;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionMesh;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::kContactTolerance;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector3;

    /// 12 edges a box, and the capsule's two rings, four cap arcs and four sides.
    constexpr std::size_t kBoxSegments = 12u;
    constexpr std::size_t kCapsuleSegments =
        2u * PhysicsOverlay::kCircleSegments + 4u * (PhysicsOverlay::kCircleSegments / 2u) + 4u;

    CollisionObb
    Box(const Vector3& centre, const Vector3& half, CollisionKind kind, std::uint16_t surface = 0u)
    {
        CollisionObb obb;
        obb.centre = centre;
        obb.halfExtents = half;
        obb.kind = kind;
        obb.surface = surface;
        return obb;
    }

    /// One cell holding @p shapes, on a grid coarse enough that every bucket holds everything --
    /// the broad phase has its own tests and is not what is being measured here.
    CollisionCell Cell(std::string id, std::vector<std::uint32_t> shapes)
    {
        CollisionCell cell;
        cell.id = std::move(id);
        cell.bounds = BoundingBox(Vector3(-8.0f, -8.0f, -8.0f), Vector3(8.0f, 8.0f, 8.0f));
        cell.nx = 16u;
        cell.nz = 16u;
        cell.originX = -8.0f;
        cell.originZ = -8.0f;
        cell.shapes = std::move(shapes);
        cell.buckets.assign(256u, {});
        for (std::uint16_t local = 0; local < static_cast<std::uint16_t>(cell.shapes.size()); ++local)
        {
            for (auto& bucket : cell.buckets)
            {
                bucket.push_back(local);
            }
        }
        return cell;
    }

    std::size_t CountColour(std::span<const Segment> segments, Color colour)
    {
        std::size_t count = 0;
        for (const Segment& segment : segments)
        {
            if (segment.colour == colour)
            {
                ++count;
            }
        }
        return count;
    }

    bool Mentions(const std::vector<std::string>& lines, std::string_view text)
    {
        for (const std::string& line : lines)
        {
            if (line.find(text) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    /// A body standing 2 mm over y = 0, which is where §49.3 leaves one.
    Capsule Body(float x, float z, float feet = 0.0f)
    {
        return Capsule{Vector3(x, feet + 0.902f, z), 0.60f, 0.30f};
    }

} // namespace

TEST(PhysicsOverlayTests, TheKindsABodyTreatsDifferentlyLookDifferent)
{
    // Walk on it, walk into it, climb it: if those three share a colour the overlay cannot be used
    // to answer the question it is for, which is "why did the body stop there".
    const Color floor = PhysicsOverlay::ColourOf(CollisionKind::Floor);
    const Color wall = PhysicsOverlay::ColourOf(CollisionKind::Wall);
    const Color stair = PhysicsOverlay::ColourOf(CollisionKind::Stair);
    EXPECT_NE(floor.getPackedValueProperty(), wall.getPackedValueProperty());
    EXPECT_NE(floor.getPackedValueProperty(), stair.getPackedValueProperty());
    EXPECT_NE(wall.getPackedValueProperty(), stair.getPackedValueProperty());

    // And every kind in the enum has one, including the two that are easy to forget.
    for (const CollisionKind kind : {CollisionKind::Floor,
                                     CollisionKind::Ceiling,
                                     CollisionKind::Wall,
                                     CollisionKind::Stair,
                                     CollisionKind::Prop,
                                     CollisionKind::Exterior})
    {
        EXPECT_NE(PhysicsOverlay::ColourOf(kind).getPackedValueProperty(), 0u) << static_cast<int>(kind);
    }
}

TEST(PhysicsOverlayTests, OnlyTheVisibleCellsAreDrawnAndASharedWallOnlyOnce)
{
    // §71 says "in the visible cells", and the wall between two rooms is in both of their shape
    // lists -- drawing it twice doubles the cost of every corridor in the house.
    CollisionWorld world;
    world.gridCell = 1.0f;
    world.surfaces = {"plaster"};
    world.obbs = {Box(Vector3(-2.0f, 1.0f, 0.0f), Vector3(0.1f, 1.0f, 2.0f), CollisionKind::Wall),
                  Box(Vector3(0.0f, 1.0f, 0.0f), Vector3(0.1f, 1.0f, 2.0f), CollisionKind::Wall),
                  Box(Vector3(2.0f, 1.0f, 0.0f), Vector3(0.1f, 1.0f, 2.0f), CollisionKind::Wall)};
    world.cells = {Cell("L0_WEST", {0u, 1u}), Cell("L0_EAST", {1u, 2u})};

    BroadPhase broad;
    PhysicsOverlay overlay;
    PhysicsOverlayBody body;
    body.capsule = Body(-1.0f, 0.0f);

    const std::array<const CollisionCell*, 1> west{&world.cells[0]};
    overlay.Build(world, west, broad, body);
    EXPECT_EQ(overlay.Segments().size(), 2u * kBoxSegments + kCapsuleSegments);

    const std::array<const CollisionCell*, 2> both{&world.cells[0], &world.cells[1]};
    overlay.Build(world, both, broad, body);
    EXPECT_EQ(overlay.Segments().size(), 3u * kBoxSegments + kCapsuleSegments)
        << "the wall both cells list was drawn twice";
    EXPECT_TRUE(Mentions(overlay.Lines(), "2 cell(s)"));
    EXPECT_TRUE(Mentions(overlay.Lines(), "3 obb"));
}

TEST(PhysicsOverlayTests, AYawedBoxIsDrawnYawed)
{
    // §49.2 lets a prop's proxy carry a yaw, and `BoundingBox` cannot hold one -- which is the
    // reason this overlay emits its own lines instead of calling `DebugDraw::Box`.
    CollisionWorld world;
    world.surfaces = {"wood"};
    CollisionObb prop = Box(Vector3(0.0f, 0.5f, 0.0f), Vector3(1.0f, 0.5f, 0.25f), CollisionKind::Prop);
    prop.yaw = 1.5707963f; // a quarter turn: the long axis now runs along z
    world.obbs = {prop};
    world.cells = {Cell("L0_ROOM", {0u})};

    BroadPhase broad;
    PhysicsOverlay overlay;
    PhysicsOverlayBody body;
    body.capsule = Body(4.0f, 4.0f);
    const std::array<const CollisionCell*, 1> cells{&world.cells[0]};
    overlay.Build(world, cells, broad, body);

    float maxX = 0.0f;
    float maxZ = 0.0f;
    for (const Segment& segment : overlay.Segments())
    {
        if (segment.colour == PhysicsOverlay::ColourOf(CollisionKind::Prop))
        {
            maxX = std::max({maxX, std::abs(segment.from.X), std::abs(segment.to.X)});
            maxZ = std::max({maxZ, std::abs(segment.from.Z), std::abs(segment.to.Z)});
        }
    }
    EXPECT_NEAR(maxX, 0.25f, 1e-4f) << "the box was drawn in its own frame, unrotated";
    EXPECT_NEAR(maxZ, 1.0f, 1e-4f);
}

TEST(PhysicsOverlayTests, TheCapsuleIsRedOnlyWhenItIsSomewhereItShouldNotBe)
{
    CollisionWorld world;
    world.surfaces = {"plaster"};
    world.obbs = {Box(Vector3(0.0f, 1.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f), CollisionKind::Wall)};
    world.cells = {Cell("L0_ROOM", {0u})};
    const std::array<const CollisionCell*, 1> cells{&world.cells[0]};

    BroadPhase broad;
    PhysicsOverlay overlay;

    PhysicsOverlayBody clear;
    clear.capsule = Body(3.0f, 3.0f);
    clear.cell = &world.cells[0];
    overlay.Build(world, cells, broad, clear);
    EXPECT_EQ(CountColour(overlay.Segments(), Color::Red), 0u);
    EXPECT_EQ(CountColour(overlay.Segments(), Color::Yellow), kCapsuleSegments);
    EXPECT_TRUE(Mentions(overlay.Lines(), "overlap  clear"));

    PhysicsOverlayBody buried;
    buried.capsule = Capsule{Vector3(0.0f, 1.0f, 0.0f), 0.60f, 0.30f};
    buried.cell = &world.cells[0];
    overlay.Build(world, cells, broad, buried);
    EXPECT_EQ(CountColour(overlay.Segments(), Color::Yellow), 0u) << "a body inside a wall looked fine";
    // The capsule, plus the one line showing which way §49.3's step 5 will push it out.
    EXPECT_EQ(CountColour(overlay.Segments(), Color::Red), kCapsuleSegments + 1u);
    EXPECT_TRUE(Mentions(overlay.Lines(), "INSIDE"));
    EXPECT_TRUE(overlay.Overlap().overlapped);

    // Resting against the wall. Touching is not penetrating (§49.3's contact tolerance) and a
    // slide leaves a body exactly here every time it walks into something, so an overlay that
    // painted that red would be red in every corridor in the house.
    PhysicsOverlayBody leaning;
    leaning.capsule = Capsule{Vector3(1.30f, 1.0f, 0.0f), 0.60f, 0.30f};
    leaning.cell = &world.cells[0];
    overlay.Build(world, cells, broad, leaning);
    ASSERT_TRUE(overlay.Overlap().overlapped) << "the fixture is not even touching; nothing is tested";
    ASSERT_LE(overlay.Overlap().depth, kContactTolerance);
    EXPECT_EQ(CountColour(overlay.Segments(), Color::Red), 0u)
        << "a body resting against a wall was drawn as buried in it";
    EXPECT_EQ(CountColour(overlay.Segments(), Color::Yellow), kCapsuleSegments);
    EXPECT_TRUE(Mentions(overlay.Lines(), "touching"));
}

TEST(PhysicsOverlayTests, TheProbeSaysWhatTheBodyIsStandingOn)
{
    CollisionWorld world;
    world.surfaces = {"oak"};
    world.obbs = {Box(Vector3(0.0f, -0.25f, 0.0f), Vector3(4.0f, 0.25f, 4.0f), CollisionKind::Floor, 0u)};
    world.cells = {Cell("L0_ROOM", {0u})};
    const std::array<const CollisionCell*, 1> cells{&world.cells[0]};

    BroadPhase broad;
    PhysicsOverlay overlay;
    PhysicsOverlayBody body;
    body.capsule = Body(0.0f, 0.0f);
    body.cell = &world.cells[0];
    overlay.Build(world, cells, broad, body);

    ASSERT_TRUE(overlay.Ground().onGround);
    EXPECT_TRUE(Mentions(overlay.Lines(), "floor 'oak'")) << "the overlay did not name the surface";
    EXPECT_TRUE(Mentions(overlay.Lines(), "2 mm")) << "the 2 mm of daylight under the body went unsaid";
    EXPECT_TRUE(Mentions(overlay.Lines(), "slope 0.0deg"));

    // The cross is AT the height the probe found, not at the feet: the difference between those
    // two is the thing the probe exists to measure.
    std::size_t atGround = 0;
    for (const Segment& segment : overlay.Segments())
    {
        const bool onTheHeight = std::abs(segment.from.Y - overlay.Ground().height) < 1e-5f &&
                                 std::abs(segment.to.Y - overlay.Ground().height) < 1e-5f;
        // Under the BODY, which is what excludes the floor's own top edges: they are at the same
        // height and run along the far side of the room.
        const bool underTheBody = std::abs(segment.from.X - body.capsule.centre.X) < 0.31f &&
                                  std::abs(segment.to.X - body.capsule.centre.X) < 0.31f &&
                                  std::abs(segment.from.Z - body.capsule.centre.Z) < 0.31f &&
                                  std::abs(segment.to.Z - body.capsule.centre.Z) < 0.31f;
        if (onTheHeight && underTheBody)
        {
            ++atGround;
        }
    }
    EXPECT_EQ(atGround, 2u) << "the contact cross is missing";
}

TEST(PhysicsOverlayTests, GroundThatIsTooSteepToStandOnIsNotDrawnAsGround)
{
    // §43.1's 46° limit. A body on a 60° face is not standing, and an overlay that draws that in
    // the same green as a floor is telling the reader the opposite of what the physics decided.
    CollisionWorld world;
    world.surfaces = {"rock"};
    CollisionMesh ramp;
    ramp.kind = CollisionKind::Exterior;
    ramp.surface = 0u;
    // tan 60 deg = 1.732, so six metres of z rise 10.392 and the surface at z = 0 is at 5.196.
    ramp.vertices = {Vector3(-3.0f, 0.0f, -3.0f),
                     Vector3(3.0f, 0.0f, -3.0f),
                     Vector3(-3.0f, 10.392f, 3.0f),
                     Vector3(3.0f, 10.392f, 3.0f)};
    ramp.indices = {0u, 1u, 2u, 2u, 1u, 3u};
    ramp.bounds = BoundingBox(Vector3(-3.0f, 0.0f, -3.0f), Vector3(3.0f, 10.392f, 3.0f));
    world.meshes = {ramp};
    world.cells = {Cell("EXT_LOT", {0u})};
    const std::array<const CollisionCell*, 1> cells{&world.cells[0]};

    BroadPhase broad;
    PhysicsOverlay overlay;
    PhysicsOverlayBody body;
    // Over the middle of the ramp, feet just above its surface.
    body.capsule = Capsule{Vector3(0.0f, 5.196f + 0.902f, 0.0f), 0.60f, 0.30f};
    body.cell = &world.cells[0];
    overlay.Build(world, cells, broad, body);

    ASSERT_TRUE(overlay.Ground().steep) << "the 60 degree face was not steep";
    EXPECT_FALSE(overlay.Ground().onGround);
    // The probe line, the cross and the normal, and nothing else in the overlay is orange.
    EXPECT_EQ(CountColour(overlay.Segments(), Color::Orange), 4u);
    EXPECT_EQ(CountColour(overlay.Segments(), Color::Lime), 0u) << "steep ground was drawn as floor";
    EXPECT_TRUE(Mentions(overlay.Lines(), "STEEP"));
    // Two triangles, three edges each: a mesh is drawn as its triangles and not as its bounds.
    EXPECT_EQ(CountColour(overlay.Segments(), PhysicsOverlay::ColourOf(CollisionKind::Exterior)), 6u);
    EXPECT_TRUE(Mentions(overlay.Lines(), "1 mesh (2 tri)"));
}

TEST(PhysicsOverlayTests, TheSweepDrawnIsTheOneTheNextStepWouldRun)
{
    CollisionWorld world;
    world.surfaces = {"plaster"};
    world.obbs = {Box(Vector3(1.0f, 1.0f, 0.0f), Vector3(0.1f, 1.0f, 2.0f), CollisionKind::Wall),
                  Box(Vector3(0.0f, -0.25f, 0.0f), Vector3(4.0f, 0.25f, 4.0f), CollisionKind::Floor)};
    world.cells = {Cell("L0_ROOM", {0u, 1u})};
    const std::array<const CollisionCell*, 1> cells{&world.cells[0]};

    BroadPhase broad;
    PhysicsOverlay overlay;

    // At rest there is nothing to sweep, and saying "clear" would be a lie about a sweep that was
    // never run.
    PhysicsOverlayBody still;
    still.capsule = Body(0.0f, 0.0f);
    still.cell = &world.cells[0];
    overlay.Build(world, cells, broad, still);
    EXPECT_TRUE(Mentions(overlay.Lines(), "at rest"));
    EXPECT_FALSE(overlay.Sweep().hit);

    // Walking at the wall: the face is at x = 0.9, the capsule's is at 0.30, so 0.005 m of gap.
    PhysicsOverlayBody walking = still;
    walking.capsule.centre = Vector3(0.595f, walking.capsule.centre.Y, 0.0f);
    walking.velocity = Vector3(1.35f, 0.0f, 0.0f);
    walking.dt = 1.0f / 120.0f;
    overlay.Build(world, cells, broad, walking);

    const Vector3 motion(walking.velocity.X * walking.dt, 0.0f, 0.0f);
    const CellSweepHit direct = SweepCell(world, world.cells[0], broad, walking.capsule, motion);
    ASSERT_TRUE(direct.hit) << "the fixture does not block the step; the test proves nothing";
    EXPECT_EQ(overlay.Sweep().hit, direct.hit);
    EXPECT_FLOAT_EQ(overlay.Sweep().time, direct.time) << "the overlay drew a different sweep";
    EXPECT_EQ(overlay.Sweep().shape, direct.shape);
    EXPECT_TRUE(Mentions(overlay.Lines(), "BLOCKED"));
    EXPECT_TRUE(Mentions(overlay.Lines(), "'plaster'"));

    // The travel and the contact normal are red; the part of the step that will NOT happen is grey.
    EXPECT_EQ(CountColour(overlay.Segments(), Color::Red), 2u);
    EXPECT_EQ(CountColour(overlay.Segments(), Color::Gray), 1u);

    // And the travel STOPS where the sweep said, not where the step wanted to go: the gap between
    // those two is the whole point of drawing it.
    bool foundTravel = false;
    for (const Segment& segment : overlay.Segments())
    {
        if (segment.colour == Color::Red && std::abs(segment.from.X - walking.capsule.centre.X) < 1e-6f &&
            std::abs(segment.from.Z - walking.capsule.centre.Z) < 1e-6f)
        {
            foundTravel = true;
            EXPECT_NEAR(segment.to.X, walking.capsule.centre.X + motion.X * direct.time, 1e-6f)
                << "the travel line ran the whole step instead of stopping at the contact";
        }
    }
    EXPECT_TRUE(foundTravel) << "nothing was drawn from the capsule towards the wall";

    // ...and a step with room to finish says so, in a colour that is not the blocked one.
    PhysicsOverlayBody roaming = still;
    roaming.velocity = Vector3(0.0f, 0.0f, 1.35f);
    overlay.Build(world, cells, broad, roaming);
    EXPECT_FALSE(overlay.Sweep().hit);
    EXPECT_EQ(CountColour(overlay.Segments(), Color::Red), 0u);
    EXPECT_TRUE(Mentions(overlay.Lines(), "clear"));
}

TEST(PhysicsOverlayTests, WithoutACellItDrawsTheWorldAndTheBodyAndSaysWhyThereIsNoMore)
{
    // The free-fly camera is outside the cell graph on purpose (§69), and an overlay that
    // crashed or silently drew a probe from a cell it is not in would be worse than one that says
    // it cannot.
    CollisionWorld world;
    world.surfaces = {"plaster"};
    world.obbs = {Box(Vector3(0.0f, 1.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f), CollisionKind::Wall)};
    world.cells = {Cell("L0_ROOM", {0u})};
    const std::array<const CollisionCell*, 1> cells{&world.cells[0]};

    BroadPhase broad;
    PhysicsOverlay overlay;
    PhysicsOverlayBody body;
    body.capsule = Body(5.0f, 5.0f);
    body.velocity = Vector3(1.35f, 0.0f, 0.0f);
    body.cell = nullptr;
    overlay.Build(world, cells, broad, body);

    EXPECT_EQ(overlay.Segments().size(), kBoxSegments + kCapsuleSegments);
    EXPECT_FALSE(overlay.Ground().onGround);
    EXPECT_FALSE(overlay.Sweep().hit);
    EXPECT_TRUE(Mentions(overlay.Lines(), "no cell"));
}

TEST(PhysicsOverlayTests, ADenseWorldIsCappedAndSaysHowMuchItDropped)
{
    // `DebugDraw` drops silently at 65 536 vertices. A debugging tool that quietly stops showing
    // part of the world is worse than one that says it ran out of room.
    CollisionWorld world;
    world.surfaces = {"plaster"};
    std::vector<std::uint32_t> shapes;
    const std::uint32_t count = static_cast<std::uint32_t>(PhysicsOverlay::kMaxSegments / kBoxSegments) + 20u;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        world.obbs.push_back(Box(Vector3(static_cast<float>(i) * 0.01f, 1.0f, 0.0f),
                                 Vector3(0.05f, 0.05f, 0.05f),
                                 CollisionKind::Prop));
        shapes.push_back(i);
    }
    world.cells = {Cell("L0_CLUTTER", shapes)};
    const std::array<const CollisionCell*, 1> cells{&world.cells[0]};

    BroadPhase broad;
    PhysicsOverlay overlay;
    PhysicsOverlayBody body;
    body.capsule = Body(-5.0f, -5.0f);
    overlay.Build(world, cells, broad, body);

    EXPECT_EQ(overlay.Segments().size(), PhysicsOverlay::kMaxSegments);
    EXPECT_GT(overlay.Dropped(), 0u);
    EXPECT_TRUE(Mentions(overlay.Lines(), "DROPPED"));

    // ...and a build that fits says nothing about dropping, because a warning that is always there
    // is not a warning.
    world.cells = {Cell("L0_TIDY", {0u, 1u, 2u})};
    const std::array<const CollisionCell*, 1> tidy{&world.cells[0]};
    overlay.Build(world, tidy, broad, body);
    EXPECT_EQ(overlay.Dropped(), 0u);
    EXPECT_FALSE(Mentions(overlay.Lines(), "DROPPED"));
}

TEST(PhysicsOverlayTests, ItIsHiddenUntilItIsAskedFor)
{
    PhysicsOverlay overlay;
    EXPECT_FALSE(overlay.Visible());
    EXPECT_TRUE(overlay.Lines().empty()) << "it had something to say before it was ever built";
    overlay.Toggle();
    EXPECT_TRUE(overlay.Visible());
    overlay.Toggle();
    EXPECT_FALSE(overlay.Visible());
    overlay.SetVisible(true);
    EXPECT_TRUE(overlay.Visible());
}

TEST(PhysicsOverlayTests, TheRealHouseFitsAndSaysTheSameThingTwice)
{
    const std::string path = "content/world/collision.bin";
    System::IO::FileStream* probe = nullptr;
    try
    {
        probe = new System::IO::FileStream(path, System::IO::FileMode::Open, System::IO::FileAccess::Read);
    }
    catch (const std::exception&)
    {
        GTEST_SKIP() << "no " << path << "; run tools/ci/build_content.py --only world";
    }
    const std::unique_ptr<System::IO::FileStream> stream(probe);
    const auto world = CollisionLoader::Read(*stream, path);
    ASSERT_TRUE(world) << world.Error().Message();

    BroadPhase broad;
    PhysicsOverlay overlay;
    std::size_t drawn = 0;
    std::size_t worst = 0;
    std::string worstCell;
    for (const CollisionCell& cell : world->cells)
    {
        if (cell.shapes.empty())
        {
            continue;
        }
        const std::array<const CollisionCell*, 1> cells{&cell};
        PhysicsOverlayBody body;
        body.capsule = Capsule{Vector3(cell.originX + static_cast<float>(cell.nx) * 0.5f,
                                       cell.bounds.Min.Y + 0.902f,
                                       cell.originZ + static_cast<float>(cell.nz) * 0.5f),
                               0.60f,
                               0.30f};
        body.velocity = Vector3(1.35f, 0.0f, 0.0f);
        body.cell = &cell;
        overlay.Build(*world, cells, broad, body);
        ++drawn;
        if (overlay.Segments().size() > worst)
        {
            worst = overlay.Segments().size();
            worstCell = cell.id;
        }

        for (const Segment& segment : overlay.Segments())
        {
            ASSERT_FALSE(std::isnan(segment.from.X) || std::isnan(segment.from.Y) ||
                         std::isnan(segment.from.Z) || std::isnan(segment.to.X) || std::isnan(segment.to.Y) ||
                         std::isnan(segment.to.Z))
                << cell.id;
        }

        // A rebuild of the same frame is the same frame: an overlay that accumulated would grow
        // until it started dropping, and the first symptom would be geometry going missing.
        const std::size_t once = overlay.Segments().size();
        const std::vector<std::string> said = overlay.Lines();
        overlay.Build(*world, cells, broad, body);
        ASSERT_EQ(overlay.Segments().size(), once) << cell.id;
        ASSERT_EQ(overlay.Lines(), said) << cell.id;
    }

    ASSERT_GT(drawn, 0u) << "no cell in the house had a shape in it";
    std::cout << "  " << drawn << " cell(s) drawn; worst " << worst << " segment(s) in " << worstCell
              << " of " << PhysicsOverlay::kMaxSegments << "\n";
    EXPECT_LE(worst, PhysicsOverlay::kMaxSegments)
        << "a real cell of this house does not fit in the overlay's budget";
}
