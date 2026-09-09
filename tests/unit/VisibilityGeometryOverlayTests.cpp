// SPDX-License-Identifier: MIT
//
// `HOUSE-00682`. §25.8's `F4`: *"the visible cell set as coloured wireframe boxes, the active
// portals as filled quads, and the reduced frusta as wire pyramids."*
//
// A visibility bug is invisible by construction -- the symptom is geometry that is NOT there -- so
// the only way to see one is to draw the decision. This checks that what would be drawn IS the
// decision: the rooms the walk reached, the openings of those rooms with their latch state, and a
// pyramid on the aperture each cone was actually reduced through.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/debug/VisibilityGeometryOverlay.hpp"
#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::app::FrameContext;
    using cnahouse::debug::VisibilityGeometryOverlay;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::VisibilitySystem;
    using cnahouse::visibility::VisibleCell;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

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

    CameraView Standing(const world::WorldData& data, std::string_view cellName, float yawDegrees)
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

    FrameContext Frame(std::uint64_t index)
    {
        FrameContext frame;
        frame.frameIndex = index;
        return frame;
    }

    bool NearlyEqual(const Vector3& a, const Vector3& b)
    {
        return std::abs(a.X - b.X) < 1e-4F && std::abs(a.Y - b.Y) < 1e-4F && std::abs(a.Z - b.Z) < 1e-4F;
    }

} // namespace

TEST(VisibilityGeometryOverlayTests, ItDrawsTheRoomsTheWalkReachedAndNoOthers)
{
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }
    const CameraView view = Standing(data, "L0_HALL", 0.0F);
    system.SetCamera(view);
    system.Update(Frame(1));

    VisibilityGeometryOverlay overlay;
    overlay.Build(data, system.Visible(), system.Portals(), view.eye);

    std::printf("  %s\n", overlay.Line().c_str());
    ASSERT_FALSE(system.Visible().empty());
    EXPECT_EQ(overlay.Dropped(), 0U) << "the house needed more segments than the overlay's cap";
    EXPECT_GT(overlay.Segments().size(), 0U);

    // Twelve segments per footprint box, and a cell can have several: the wireframe is per BOX,
    // because an L-shaped room drawn as its bounding box claims a corner it does not have.
    std::size_t boxes = 0;
    for (const VisibleCell& entry : system.Visible())
    {
        const world::Cell* cell = data.FindCell(entry.cell);
        ASSERT_NE(cell, nullptr);
        boxes += cell->boxes.size();
    }
    EXPECT_GT(boxes, system.Visible().size()) << "no visible cell has more than one box, so the "
                                                 "per-box claim is not exercised here";

    // Counted, not bounded: the wireframes are the only segments drawn in a depth-ramp colour, so
    // twelve edges per FOOTPRINT is a number this test can insist on -- and one box per cell would
    // be twelve fewer for every L-shaped room in the visible set.
    std::size_t wireframe = 0;
    for (const VisibilityGeometryOverlay::Segment& segment : overlay.Segments())
    {
        for (int depth = 0; depth <= 6; ++depth)
        {
            const Microsoft::Xna::Framework::Color ramp = VisibilityGeometryOverlay::ColourForDepth(depth);
            if (segment.colour.getRProperty() == ramp.getRProperty() &&
                segment.colour.getGProperty() == ramp.getGProperty() &&
                segment.colour.getBProperty() == ramp.getBProperty())
            {
                ++wireframe;
                break;
            }
        }
    }
    EXPECT_EQ(wireframe, 12U * boxes) << "the wireframe is not twelve edges per footprint box";

    // Every drawn segment is inside the union of the visible cells' boxes, grown by the portal
    // rectangles that stick out of them -- which is to say nothing from a room the walk refused.
    // Checked as a bounding box, which is the claim that a cell the walk did not reach is not
    // drawn: an unreached room outside this box would enlarge it.
    float minX = 1e9F;
    float maxX = -1e9F;
    for (const VisibilityGeometryOverlay::Segment& segment : overlay.Segments())
    {
        minX = std::min({minX, segment.from.X, segment.to.X});
        maxX = std::max({maxX, segment.from.X, segment.to.X});
    }
    float cellMinX = 1e9F;
    float cellMaxX = -1e9F;
    for (const VisibleCell& entry : system.Visible())
    {
        for (const world::Footprint& box : data.FindCell(entry.cell)->boxes)
        {
            cellMinX = std::min(cellMinX, box.minX);
            cellMaxX = std::max(cellMaxX, box.maxX);
        }
    }
    // The cones reach the eye and the portals reach their own rectangles, so a little slack --
    // but not a room's worth.
    EXPECT_GE(minX, cellMinX - 1.0F) << "something west of every visible room was drawn";
    EXPECT_LE(maxX, cellMaxX + 1.0F) << "something east of every visible room was drawn";
}

TEST(VisibilityGeometryOverlayTests, AConesPyramidStandsOnTheApertureItWasReducedThrough)
{
    // §25.8's *"reduced frusta as wire pyramids"*. The planes alone have no corners, so the walk
    // records the polygon `ClipRectToFrustum` produced and `ReduceFrustum` consumed -- and the
    // pyramid is the eye joined to that polygon. Anything else would be a picture of a frustum
    // that is not the one the traversal used.
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
    const CameraView view = Standing(data, "L0_HALL", 0.0F);
    system.SetCamera(view);
    system.Update(Frame(2));

    VisibilityGeometryOverlay overlay;
    overlay.Build(data, system.Visible(), system.Portals(), view.eye);

    int apertures = 0;
    int cameraCones = 0;
    for (const VisibleCell& entry : system.Visible())
    {
        for (std::size_t i = 0; i < entry.frustumCount; ++i)
        {
            if (entry.apertures[i].Points().empty())
            {
                ++cameraCones;
                continue;
            }
            ++apertures;
            // Every vertex of the aperture is joined to the eye.
            for (const Vector3& point : entry.apertures[i].Points())
            {
                const bool joined =
                    std::any_of(overlay.Segments().begin(),
                                overlay.Segments().end(),
                                [&](const VisibilityGeometryOverlay::Segment& s)
                                {
                                    return (NearlyEqual(s.from, view.eye) && NearlyEqual(s.to, point)) ||
                                           (NearlyEqual(s.to, view.eye) && NearlyEqual(s.from, point));
                                });
                EXPECT_TRUE(joined) << "an aperture vertex has no edge to the eye";
            }
            // ...and the BASE. A pyramid is the apex edges and the polygon they stand on; without
            // the outline what is drawn is a bundle of rays, and the shape of the aperture -- which
            // is the whole thing §25.2 computed -- is invisible.
            const std::span<const Vector3> points = entry.apertures[i].Points();
            for (std::size_t at = 0; at < points.size(); ++at)
            {
                const Vector3& from = points[at];
                const Vector3& to = points[(at + 1) % points.size()];
                const bool edged =
                    std::any_of(overlay.Segments().begin(),
                                overlay.Segments().end(),
                                [&](const VisibilityGeometryOverlay::Segment& s)
                                {
                                    return (NearlyEqual(s.from, from) && NearlyEqual(s.to, to)) ||
                                           (NearlyEqual(s.to, from) && NearlyEqual(s.from, to));
                                });
                EXPECT_TRUE(edged) << "the pyramid has no base: aperture edge " << at << " is not drawn";
            }
        }
    }
    EXPECT_GT(apertures, 0) << "no cone had an aperture, so no pyramid was checked";
    EXPECT_EQ(cameraCones, 1) << "exactly one cone -- the camera's own -- is reduced through "
                                 "nothing, and it must not draw a pyramid on an empty polygon";
    std::printf("  %d cone(s) with an aperture, %d without\n", apertures, cameraCones);
}

TEST(VisibilityGeometryOverlayTests, AShutDoorIsDrawnAndDrawnDifferently)
{
    // The question this overlay answers is *"why is that room dark"*, and the commonest answer is
    // a shut door. Drawing only the openings the walk crossed would hide exactly the thing the
    // reader is looking for.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    const CameraView view = Standing(data, "L0_HALL", 0.0F);

    // §65.6's starting state: every leafed portal shut.
    system.SetCamera(view);
    system.Update(Frame(3));
    VisibilityGeometryOverlay shut;
    shut.Build(data, system.Visible(), system.Portals(), view.eye);

    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }
    system.SetCamera(view);
    system.Update(Frame(4));
    VisibilityGeometryOverlay open;
    open.Build(data, system.Visible(), system.Portals(), view.eye);

    std::printf("  doors shut: %s\n  doors open: %s\n", shut.Line().c_str(), open.Line().c_str());
    EXPECT_NE(shut.Quads().size(), 0U) << "not one portal quad was drawn";

    // A quad is drawn for every opening of every visible cell, latched or not -- and the two
    // states are different colours, or the overlay cannot be used to tell them apart.
    const auto colours = [](const VisibilityGeometryOverlay& overlay)
    {
        std::size_t distinct = 0;
        for (const VisibilityGeometryOverlay::Quad& quad : overlay.Quads())
        {
            distinct += quad.colour.getRProperty() > quad.colour.getBProperty() ? 1U : 0U;
        }
        return distinct;
    };
    EXPECT_GT(colours(shut), 0U) << "with every door shut, not one was drawn as shut";
    EXPECT_LT(colours(open), colours(shut)) << "opening every door changed no portal's colour";
}

TEST(VisibilityGeometryOverlayTests, TheDepthRampIsAKeyNobodyHasToLookUp)
{
    // A ramp and not a palette: the number a reader wants is HOW FAR through the graph a room is,
    // and a ramp answers it at a glance. It must not wrap -- a colour that came back round to
    // green would say "next to the camera" about the furthest room in the house.
    const Microsoft::Xna::Framework::Color near = VisibilityGeometryOverlay::ColourForDepth(0);
    const Microsoft::Xna::Framework::Color mid = VisibilityGeometryOverlay::ColourForDepth(3);
    const Microsoft::Xna::Framework::Color far = VisibilityGeometryOverlay::ColourForDepth(6);
    EXPECT_LT(near.getRProperty(), mid.getRProperty());
    EXPECT_LT(mid.getRProperty(), far.getRProperty());
    EXPECT_GT(near.getGProperty(), mid.getGProperty());
    EXPECT_GT(mid.getGProperty(), far.getGProperty());

    const Microsoft::Xna::Framework::Color beyond = VisibilityGeometryOverlay::ColourForDepth(40);
    EXPECT_EQ(beyond.getRProperty(), far.getRProperty()) << "the ramp wrapped past its deepest step";
    EXPECT_EQ(beyond.getGProperty(), far.getGProperty());
    const Microsoft::Xna::Framework::Color negative = VisibilityGeometryOverlay::ColourForDepth(-3);
    EXPECT_EQ(negative.getRProperty(), near.getRProperty());
}

TEST(VisibilityGeometryOverlayTests, ItSaysWhenItRanOutRatherThanQuietlyStopping)
{
    // `DebugDraw` drops silently when its buffer fills. A debugging tool that quietly stops
    // showing some of the world is worse than one that says it ran out, which is why the cap is
    // here, counted, and on the screen.
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
    const CameraView view = Standing(data, "L0_HALL", 0.0F);
    system.SetCamera(view);
    system.Update(Frame(5));

    // The visible set repeated until the cap is passed, which is the only way to reach it from
    // §12's house -- and the honest way to check that reaching it is reported.
    std::vector<VisibleCell> many;
    while (many.size() < 400)
    {
        many.insert(many.end(), system.Visible().begin(), system.Visible().end());
    }
    VisibilityGeometryOverlay overlay;
    overlay.Build(data, many, system.Portals(), view.eye);

    EXPECT_EQ(overlay.Segments().size(), VisibilityGeometryOverlay::kMaxSegments);
    EXPECT_GT(overlay.Dropped(), 0U);
    EXPECT_NE(overlay.Line().find("DROPPED"), std::string::npos) << overlay.Line();

    // ...and a build that fits says nothing about dropping.
    overlay.Build(data, system.Visible(), system.Portals(), view.eye);
    EXPECT_EQ(overlay.Dropped(), 0U);
    EXPECT_EQ(overlay.Line().find("DROPPED"), std::string::npos) << overlay.Line();
}

TEST(VisibilityGeometryOverlayTests, RebuildingReplacesTheFrameRatherThanAddingToIt)
{
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    const CameraView view = Standing(data, "L0_HALL", 0.0F);
    system.SetCamera(view);
    system.Update(Frame(6));

    VisibilityGeometryOverlay overlay;
    overlay.Build(data, system.Visible(), system.Portals(), view.eye);
    const std::size_t once = overlay.Segments().size();
    const std::size_t quads = overlay.Quads().size();
    overlay.Build(data, system.Visible(), system.Portals(), view.eye);
    EXPECT_EQ(overlay.Segments().size(), once) << "the second frame was added to the first";
    EXPECT_EQ(overlay.Quads().size(), quads);

    overlay.Build(data, {}, system.Portals(), view.eye);
    EXPECT_TRUE(overlay.Segments().empty()) << "an empty visible set left the last frame on screen";
    EXPECT_TRUE(overlay.Quads().empty());
}

TEST(VisibilityGeometryOverlayTests, ItIsHiddenUntilItIsToggled)
{
    VisibilityGeometryOverlay overlay;
    EXPECT_FALSE(overlay.Visible());
    overlay.Toggle();
    EXPECT_TRUE(overlay.Visible());
    overlay.SetVisible(false);
    EXPECT_FALSE(overlay.Visible());
}
