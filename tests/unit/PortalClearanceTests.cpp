// SPDX-License-Identifier: MIT
//
// `HOUSE-00616`: the guarantee that the player capsule fits through every portal they can walk
// through — and that the list of portals it does NOT fit through is a list somebody wrote down.
//
// `WorldValidator`'s rule 10 and `validate_world.py`'s already report a portal too small for
// §70.5's 0.62 × 1.95 m capsule. What neither of them can say is *which* portals are deliberately
// too small: both take `crouch` and `hatch` at face value, so marking a doorway `crouch` silences
// the check for ever and nothing notices. This test names the ten exemptions this house has. A
// new one has to be added here, in a commit, with a reason — which is the whole point of a
// guarantee test as opposed to a validation rule.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::util::IdRegistry;
    namespace world = cnahouse::world;

    /// §70.5. The same two numbers `WorldValidator` uses; repeated here on purpose, because a
    /// guarantee that read the checker's own constants would agree with it about a typo.
    constexpr float kCapsuleWidth = 0.62F;
    constexpr float kCapsuleHeight = 1.95F;

    /// Every portal the capsule is allowed not to fit through, and why. Sorted, so the failure
    /// message reads as a diff.
    const std::vector<std::string>& ExpectedExemptions()
    {
        static const std::vector<std::string> kExempt{
            "P_B1_HALL__B1_UNDERSTAIR",    // the under-stair cupboard: 1.55 m of leaf
            "P_FREEZER_INTERIOR",          // a chest freezer; nobody walks into it
            "P_FRIDGE_INTERIOR",           // nor into the refrigerator
            "P_L0_GARAGE__L0_GARAGE_LOFT", // a 0.90 m loft hatch reached by a ladder
            "P_L3_HEAD__L3_STORE_E",       // and §13.6's six attic stores, all under the
            "P_L3_ROOM__L3_STORE_N",       // rafters, all ducked into
            "P_L3_ROOM__L3_STORE_S",
            "P_L3_ROOM__L3_STORE_W",
            "P_L3_STORE_E__L3_STORE_N",
            "P_L3_STORE_W__L3_STORE_N",
        };
        return kExempt;
    }

    /// A portal a person is expected to walk through: not a window, not marked to be ducked.
    [[nodiscard]] bool IsWalkedThrough(const world::Portal& portal)
    {
        return portal.kind != world::PortalKind::Window && !portal.crouch &&
               portal.kind != world::PortalKind::Hatch;
    }

    TEST(PortalClearanceTests, TheCapsuleFitsEveryPortalItIsMeantTo)
    {
        IdRegistry::ResetForTesting();
        const std::string directory = "content/world";
        if (!std::filesystem::exists(directory + "/layout.portals.json"))
        {
            GTEST_SKIP() << "no deployed world; run tools/world/deploy_world.py";
        }

        world::WorldData::Contents contents;
        ASSERT_TRUE(world::WorldLoader::LoadLevels(directory, contents));
        ASSERT_TRUE(world::WorldLoader::LoadCells(directory, contents));
        ASSERT_TRUE(world::WorldLoader::LoadPortals(directory, contents));
        auto built = world::WorldData::Create(std::move(contents));
        ASSERT_TRUE(built) << built.Error().ToString();

        std::vector<std::string> tooSmall;
        std::vector<std::string> exempt;
        int walked = 0;
        // The tightest doorway in the house, reported whether or not it fails: "no violations"
        // says nothing about how close the house is to having one.
        float tightestWidth = 1e9F;
        float tightestHeight = 1e9F;
        std::string tightestName;
        for (const world::Portal& portal : built.Value().Portals())
        {
            if (portal.kind == world::PortalKind::Window)
            {
                continue;
            }
            if (!IsWalkedThrough(portal))
            {
                exempt.emplace_back(IdRegistry::NameOf(portal.id));
                continue;
            }
            ++walked;

            // A horizontal portal is a hole in a floor and is measured across its narrow side; a
            // vertical one is a doorway and is measured as width and height.
            if (portal.axis == world::PlaneAxis::Y)
            {
                if (std::min(portal.Width(), portal.Height()) < kCapsuleWidth - 1e-6F)
                {
                    tooSmall.emplace_back(IdRegistry::NameOf(portal.id));
                }
                continue;
            }
            if (portal.Width() < tightestWidth)
            {
                tightestWidth = portal.Width();
                tightestName = IdRegistry::NameOf(portal.id);
            }
            tightestHeight = std::min(tightestHeight, portal.Height());
            if (portal.Width() < kCapsuleWidth - 1e-6F || portal.Height() < kCapsuleHeight - 1e-6F)
            {
                tooSmall.emplace_back(IdRegistry::NameOf(portal.id));
            }
        }

        std::string report;
        for (const std::string& name : tooSmall)
        {
            report += "\n  " + name;
        }
        EXPECT_TRUE(tooSmall.empty())
            << tooSmall.size() << " portal(s) the player is expected to walk through are smaller "
            << "than the " << kCapsuleWidth << " x " << kCapsuleHeight << " m capsule:" << report;
        EXPECT_GT(walked, 100) << "the guarantee has to be over the whole house, not a corner of it";

        // The two numbers are §70.5's, pinned here rather than read from the checker: a guarantee
        // that took the checker's constants would agree with it about a typo.
        EXPECT_FLOAT_EQ(kCapsuleWidth, 0.62F);
        EXPECT_FLOAT_EQ(kCapsuleHeight, 1.95F);
        EXPECT_GE(tightestWidth, kCapsuleWidth) << "the tightest doorway is " << tightestName;
        EXPECT_GE(tightestHeight, kCapsuleHeight);
        std::printf("[ portals  ] %d walked-through portal(s); tightest %.3f m wide (%s), "
                    "%.3f m high, against the capsule's %.2f x %.2f\n",
                    walked,
                    static_cast<double>(tightestWidth),
                    tightestName.c_str(),
                    static_cast<double>(tightestHeight),
                    static_cast<double>(kCapsuleWidth),
                    static_cast<double>(kCapsuleHeight));

        std::sort(exempt.begin(), exempt.end());
        EXPECT_EQ(exempt, ExpectedExemptions())
            << "the set of portals exempt from the capsule has changed. Adding `crouch` to a "
               "portal silences rule 10 for ever; this list is where that has to be said out loud.";

        IdRegistry::ResetForTesting();
    }
} // namespace
