// SPDX-License-Identifier: MIT
//
// `HOUSE-00691`. §25.3's partially-open-door rule, at the level it decides anything:
//
// > **A door leaf occludes geometry inside the target cell; it does not shrink the portal.**
//
// The portal is the DOORWAY -- a hole in the wall -- and the leaf has swung out of that plane into
// one of the two cells. So above §25.3's latch the aperture is the whole doorway rectangle however
// far the door happens to be open, and a door ajar by a tenth reveals a thin slice of the room
// because the LEAF is in the way, not because the portal was narrow.
//
// `PortalRuntimeTests` asserts the latch on one portal. This asserts what the latch DOES: which
// rooms a body standing in front of the door can see, over every door in §12's house.
#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/PortalRuntime.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"

#include "unit/DoorStand.hpp"

namespace
{
    namespace world = cnahouse::world;
    using cnahouse::app::FrameContext;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::testsupport::ContentIsBuilt;
    using cnahouse::testsupport::LoadWorld;
    using cnahouse::testsupport::Stand;
    using cnahouse::testsupport::StandBefore;
    using cnahouse::util::Id;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::PortalRuntime;
    using cnahouse::visibility::VisibilitySystem;
    using cnahouse::visibility::VisibleCell;
    using Microsoft::Xna::Framework::Vector3;

    /// What the walk sees from @p stand with @p portal open @p fraction and everything else shut.
    std::set<std::string> SeenWith(const world::WorldData& data,
                                   VisibilitySystem& system,
                                   const Stand& stand,
                                   Id cell,
                                   Id ajar,
                                   float fraction)
    {
        for (const world::Portal& portal : data.Portals())
        {
            system.SetAperture(portal.id, portal.id == ajar ? fraction : 0.0F);
        }
        PlayerState state;
        state.position = Vector3(stand.feet.X, stand.feet.Y + state.Rise(), stand.feet.Z);
        state.yaw = stand.yawDegrees * 3.14159265F / 180.0F;
        FirstPersonCamera camera;
        camera.SetAspect(16.0F / 9.0F);
        camera.Update(state, kPlayerEyeHeight, 0.0F);

        CameraView view;
        view.cell = cell;
        view.eye = camera.Pose().eye;
        view.viewProjection = camera.View() * camera.Projection();
        view.frustum = ClipFrustum(camera.Frustum());
        view.nearPlane = camera.Frustum().getNearProperty();
        view.farPlane = camera.Frustum().getFarProperty();
        system.SetCamera(view);
        FrameContext frame;
        frame.frameIndex = 1;
        system.Update(frame);

        std::set<std::string> names;
        for (const VisibleCell& seen : system.Visible())
        {
            names.insert(std::string(IdRegistry::NameOf(seen.cell)));
        }
        return names;
    }

    /// The doors this rule is about: opaque when closed, with a wall on the near side to stand at.
    struct Case
    {
        const world::Portal* portal;
        Id side;
        Id beyond;
        Stand stand;
    };

    std::vector<Case> AjarCases(const world::WorldData& data)
    {
        std::vector<Case> cases;
        for (const world::Portal& portal : data.Portals())
        {
            if (!portal.aperture.IsValid() || portal.opacity != world::PortalOpacity::OpaqueWhenClosed)
            {
                continue;
            }
            for (const Id side : {portal.cellA, portal.cellB})
            {
                const world::Cell* here = data.FindCell(side);
                const Id beyond = side == portal.cellA ? portal.cellB : portal.cellA;
                if (here == nullptr || data.FindCell(beyond) == nullptr)
                {
                    continue;
                }
                // Rooms joined a second time by glass cannot say anything about a door's
                // aperture: the other way through is open whatever the door does.
                const bool glazedToo =
                    std::any_of(data.Portals().begin(),
                                data.Portals().end(),
                                [&](const world::Portal& sibling)
                                {
                                    const bool sameTwo =
                                        (sibling.cellA == portal.cellA && sibling.cellB == portal.cellB) ||
                                        (sibling.cellA == portal.cellB && sibling.cellB == portal.cellA);
                                    return sibling.id != portal.id && sameTwo &&
                                           sibling.opacity != world::PortalOpacity::OpaqueWhenClosed;
                                });
                if (glazedToo)
                {
                    continue;
                }
                const Stand stand = StandBefore(data, portal, *here);
                if (stand.valid)
                {
                    cases.push_back(Case{&portal, side, beyond, stand});
                }
            }
        }
        return cases;
    }

} // namespace

TEST(AjarDoorTests, ATwoPerCentDoorCullsAndATenPerCentOneDoesNot)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    const std::vector<Case> cases = AjarCases(data);
    ASSERT_GT(cases.size(), 60U) << "too few doors to say anything about a rule";

    for (const Case& sample : cases)
    {
        const std::string other = std::string(IdRegistry::NameOf(sample.beyond));
        const std::string name = std::string(IdRegistry::NameOf(sample.portal->id));
        // §25.2: `if p.opacity == opaque_when_closed and p.apertureFraction <= 0.05: continue`.
        EXPECT_FALSE(
            SeenWith(data, system, sample.stand, sample.side, sample.portal->id, 0.02F).contains(other))
            << name << " at 0.02 let " << other << " through";
        EXPECT_TRUE(
            SeenWith(data, system, sample.stand, sample.side, sample.portal->id, 0.10F).contains(other))
            << name << " at 0.10 did not let " << other << " through";
    }
    std::printf("  %zu door/side case(s): 0.02 culls, 0.10 does not\n", cases.size());
}

TEST(AjarDoorTests, ADoorAjarShowsExactlyWhatAWideOpenOneShows)
{
    // §25.3's whole point. *"The moment `apertureFraction > 0.05` the portal's aperture is the FULL
    // doorway rectangle"* -- so a door open a tenth and a door open all the way reach the same
    // rooms through the same cone. A system that scaled the portal rectangle by the aperture would
    // pass the test above and fail this one, and it would look plausible on screen: a door ajar
    // would reveal a thin slice of the room. It would be the wrong reason -- the leaf is what
    // hides the rest, and the leaf is an object in the room, not a smaller doorway.
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    const std::vector<Case> cases = AjarCases(data);

    int compared = 0;
    for (const Case& sample : cases)
    {
        const std::set<std::string> ajar =
            SeenWith(data, system, sample.stand, sample.side, sample.portal->id, 0.10F);
        const std::set<std::string> wide =
            SeenWith(data, system, sample.stand, sample.side, sample.portal->id, 1.00F);
        EXPECT_EQ(ajar, wide) << IdRegistry::NameOf(sample.portal->id)
                              << " shows a different room ajar than wide open";
        ++compared;
    }
    std::printf("  %d door(s) compared at 0.10 against 1.00, all the same\n", compared);
    EXPECT_GT(compared, 60);
}

TEST(AjarDoorTests, TheHysteresisBandDoesNotFlickerTheRoomBehindTheDoor)
{
    // §25.3's second refinement: closed below 0.05, open above 0.08, and between the two NOTHING
    // changes. A door settling shut passes through that band; without the latch the room behind it
    // -- its chunks, its props, its lights -- would appear and vanish for a frame or two on the
    // way, which is the one thing a hysteresis is for.
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    const std::vector<Case> cases = AjarCases(data);
    ASSERT_FALSE(cases.empty());

    const Case& sample = cases.front();
    const std::string other = std::string(IdRegistry::NameOf(sample.beyond));

    // Opening: 0.06 is inside the band and the door has not latched open yet.
    EXPECT_FALSE(SeenWith(data, system, sample.stand, sample.side, sample.portal->id, 0.06F).contains(other))
        << "a door creeping open latched at 0.06, under §25.3's 0.08";
    // ...and then it does.
    EXPECT_TRUE(SeenWith(data, system, sample.stand, sample.side, sample.portal->id, 0.09F).contains(other));
    // Closing: 0.06 is inside the band again and the door has NOT latched shut, because it is
    // coming from the other side of it.
    EXPECT_TRUE(SeenWith(data, system, sample.stand, sample.side, sample.portal->id, 0.06F).contains(other))
        << "a door settling shut unlatched at 0.06, over §25.3's 0.05 -- the room flickers";
    EXPECT_FALSE(SeenWith(data, system, sample.stand, sample.side, sample.portal->id, 0.04F).contains(other));

    EXPECT_FLOAT_EQ(PortalRuntime::kClosedBelow, 0.05F);
    EXPECT_FLOAT_EQ(PortalRuntime::kOpenAbove, 0.08F);
    std::printf("  the band is %.2f to %.2f and the room behind %s does not flicker in it\n",
                static_cast<double>(PortalRuntime::kClosedBelow),
                static_cast<double>(PortalRuntime::kOpenAbove),
                std::string(IdRegistry::NameOf(sample.portal->id)).c_str());
}
