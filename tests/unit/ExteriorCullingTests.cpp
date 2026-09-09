// SPDX-License-Identifier: MIT
//
// `HOUSE-00678`. §25.6's steps 1 and 2 over `HOUSE-00677`'s hierarchy: frustum culling of the
// loose BVH, with the per-category distance cutoffs.
//
// **The test that matters is the one that compares the hierarchy with brute force.** §25's one
// unforgivable failure is over-culling -- something visible that was not drawn -- and a hierarchy
// is precisely a promise that a cheap answer equals an expensive one. Every pose below is
// answered twice, once by the walk and once by testing all 4 100 instances one at a time, and the
// two sets must be identical. Everything else here is about the walk being CHEAPER, which is only
// worth checking once it is right.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include "cnahouse/visibility/ExteriorCulling.hpp"

namespace
{
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::DistanceToBox;
    using cnahouse::visibility::ExteriorBvh;
    using cnahouse::visibility::ExteriorCuller;
    using cnahouse::visibility::ExteriorInstance;
    using cnahouse::visibility::PropCategory;
    using cnahouse::visibility::WithinCullDistance;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::BoundingFrustum;
    using Microsoft::Xna::Framework::ContainmentType;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;

    constexpr float kDegrees = std::numbers::pi_v<float> / 180.0F;

    /// The same stated generator `ExteriorBvhTests` uses: `<random>`'s distributions are not
    /// specified by the standard and this test's numbers must be the same on every machine.
    class Numbers
    {
    public:
        explicit Numbers(std::uint32_t seed) noexcept
            : state_(seed)
        {
        }

        [[nodiscard]] float Next(float low, float high) noexcept
        {
            state_ = state_ * 1664525u + 1013904223u;
            const float unit = static_cast<float>(state_ >> 8u) / static_cast<float>(1u << 24u);
            return low + unit * (high - low);
        }

    private:
        std::uint32_t state_;
    };

    ExteriorInstance
    Instance(const std::string& name, PropCategory category, const Vector3& centre, float radius)
    {
        ExteriorInstance instance;
        instance.id = cnahouse::util::Intern(name);
        instance.category = category;
        instance.bounds = BoundingBox(Vector3(centre.X - radius, centre.Y - radius, centre.Z - radius),
                                      Vector3(centre.X + radius, centre.Y + radius, centre.Z + radius));
        return instance;
    }

    std::vector<ExteriorInstance> APlotOf(int count)
    {
        Numbers numbers(20260909u);
        std::vector<ExteriorInstance> instances;
        instances.reserve(static_cast<std::size_t>(count));
        constexpr std::array<PropCategory, 6> kKinds{PropCategory::SmallProp,
                                                     PropCategory::GardenFurniture,
                                                     PropCategory::Fence,
                                                     PropCategory::Tree,
                                                     PropCategory::NeighbourhoodLod0,
                                                     PropCategory::Impostor};
        for (int i = 0; i < count; ++i)
        {
            const PropCategory kind = kKinds[static_cast<std::size_t>(i) % kKinds.size()];
            instances.push_back(Instance("EXT_" + std::to_string(i),
                                         kind,
                                         Vector3(numbers.Next(-200.0F, 200.0F),
                                                 numbers.Next(0.0F, 6.0F),
                                                 numbers.Next(-200.0F, 200.0F)),
                                         kind == PropCategory::Tree ? 3.0F : 0.6F));
        }
        return instances;
    }

    /// §44's lens at §10.3's far plane, looking @p yawDegrees from north (§14: -Z, east positive).
    ClipFrustum ConeAt(const Vector3& eye, float yawDegrees)
    {
        const Vector3 target(
            eye.X + std::sin(yawDegrees * kDegrees), eye.Y, eye.Z - std::cos(yawDegrees * kDegrees));
        const Matrix view = Matrix::CreateLookAt(eye, target, Vector3::Up);
        const Matrix projection =
            Matrix::CreatePerspectiveFieldOfView(70.0F * kDegrees, 16.0F / 9.0F, 0.1F, 420.0F);
        return ClipFrustum(BoundingFrustum(view * projection));
    }

    /// The expensive answer: every instance, one at a time, by the same two rules.
    std::set<std::uint32_t>
    BruteForce(const ExteriorBvh& bvh, std::span<const ClipFrustum> cones, const Vector3& eye, float scale)
    {
        std::set<std::uint32_t> visible;
        for (std::uint32_t i = 0; i < bvh.Instances().size(); ++i)
        {
            const ExteriorInstance& instance = bvh.Instances()[i];
            if (!WithinCullDistance(instance.category, DistanceToBox(instance.bounds, eye), scale))
            {
                continue;
            }
            for (const ClipFrustum& cone : cones)
            {
                if (cone.Contains(instance.bounds) != ContainmentType::Disjoint)
                {
                    visible.insert(i);
                    break;
                }
            }
        }
        return visible;
    }

    std::set<std::uint32_t> AsSet(std::span<const std::uint32_t> indices)
    {
        return std::set<std::uint32_t>(indices.begin(), indices.end());
    }

} // namespace

TEST(ExteriorCullingTests, TheHierarchyAgreesWithTestingEveryInstanceFromEveryPose)
{
    // The no-over-culling claim, made 40 times. A hierarchy is a promise that a cheap answer
    // equals an expensive one, and this is that promise checked rather than assumed.
    IdRegistry::ResetForTesting();
    ExteriorBvh bvh;
    bvh.Build(APlotOf(4100));

    ExteriorCuller culler;
    int poses = 0;
    long long tested = 0;
    long long drawn = 0;
    for (const float x : {-150.0F, -40.0F, 0.0F, 60.0F, 175.0F})
    {
        for (const float yaw : {0.0F, 45.0F, 90.0F, 135.0F, 180.0F, 225.0F, 270.0F, 315.0F})
        {
            const Vector3 eye(x, 1.68F, x * 0.5F);
            const std::array<ClipFrustum, 1> cones{ConeAt(eye, yaw)};
            culler.Cull(bvh, cones, eye);
            const std::set<std::uint32_t> expected = BruteForce(bvh, cones, eye, 1.0F);
            EXPECT_EQ(AsSet(culler.Instances()), expected)
                << "at (" << x << ", " << x * 0.5F << ") facing " << yaw << " degrees";
            ++poses;
            tested += culler.Statistics().instancesTested;
            drawn += culler.Statistics().instancesDrawn;
        }
    }
    std::printf("  %d poses over 4 100 instances: %lld instance test(s) and %lld drawn, "
                "against %d tests a pose for brute force\n",
                poses,
                tested,
                drawn,
                4100 * poses);
    EXPECT_EQ(poses, 40);
    EXPECT_GT(drawn, 0) << "every pose drew nothing, so the comparison was vacuous";
    // The whole point: the hierarchy answers in a fraction of the tests brute force needs.
    // Measured at 62 015 against 164 000, which is 38 %; bounded at half, because the ratio
    // depends on how much of the plot a 70 degree cone sees and that is a property of the pose
    // list rather than of the walk.
    EXPECT_LT(tested, static_cast<long long>(4100 * poses) / 2);
}

TEST(ExteriorCullingTests, TheHierarchyAgreesWithBruteForceAtEveryViewDistanceSetting)
{
    IdRegistry::ResetForTesting();
    ExteriorBvh bvh;
    bvh.Build(APlotOf(4100));
    ExteriorCuller culler;

    const Vector3 eye(0.0F, 1.68F, 0.0F);
    const std::array<ClipFrustum, 1> cones{ConeAt(eye, 30.0F)};
    // Inside §68's band, and outside it in both directions -- the clamp has to be the same one the
    // per-instance test applies, or the node rejection and the instance rejection disagree.
    for (const float scale : {0.6F, 0.85F, 1.0F, 1.4F, 0.2F, 9.0F})
    {
        culler.Cull(bvh, cones, eye, scale);
        EXPECT_EQ(AsSet(culler.Instances()), BruteForce(bvh, cones, eye, scale))
            << "at view distance " << scale;
    }

    // And the setting really does change the answer, or the loop above proves nothing.
    culler.Cull(bvh, cones, eye, 0.6F);
    const std::size_t near = culler.Instances().size();
    culler.Cull(bvh, cones, eye, 1.4F);
    const std::size_t far = culler.Instances().size();
    std::printf("  §68's view distance: %zu instance(s) at 0.6x, %zu at 1.4x\n", near, far);
    EXPECT_GT(far, near) << "§68's view-distance setting changed nothing";
}

TEST(ExteriorCullingTests, ACategoryIsDrawnAtItsStatedDistanceAndNotOneMetreFurther)
{
    // §26.4's boundary rule, which `HOUSE-00674` fixed the arithmetic for: the stated distance is
    // the last one at which a thing is drawn, not the first at which it is not.
    IdRegistry::ResetForTesting();

    struct Case
    {
        PropCategory category;
        float distance;
    };

    // §25.6's own numbers.
    for (const Case sample : {Case{PropCategory::SmallProp, 45.0F},
                              Case{PropCategory::GardenFurniture, 70.0F},
                              Case{PropCategory::Fence, 120.0F},
                              Case{PropCategory::Tree, 180.0F},
                              Case{PropCategory::NeighbourhoodLod0, 90.0F},
                              Case{PropCategory::NeighbourhoodLod2, 300.0F}})
    {
        const Vector3 eye(0.0F, 0.0F, 0.0F);
        // Due north of the eye, so it is squarely in a cone that faces north.
        std::vector<ExteriorInstance> instances;
        instances.push_back(
            Instance("AT_THE_LIMIT", sample.category, Vector3(0.0F, 0.0F, -(sample.distance + 0.5F)), 0.5F));
        instances.push_back(
            Instance("JUST_BEYOND", sample.category, Vector3(0.0F, 0.0F, -(sample.distance + 1.5F)), 0.5F));
        ExteriorBvh bvh;
        bvh.Build(instances);
        ExteriorCuller culler;
        const std::array<ClipFrustum, 1> cones{ConeAt(eye, 0.0F)};
        culler.Cull(bvh, cones, eye);

        // The near box's far face is exactly at the stated distance; the far one's is 1 m past it.
        ASSERT_EQ(culler.Instances().size(), 1U)
            << cnahouse::visibility::CategoryName(sample.category) << " at " << sample.distance;
        EXPECT_EQ(bvh.Instances()[culler.Instances()[0]].id, cnahouse::util::Intern("AT_THE_LIMIT"));
        EXPECT_EQ(culler.Statistics().instancesCulledByDistance, 1);
    }
}

TEST(ExteriorCullingTests, SeveralConesAreAUnionAndNothingIsDrawnTwice)
{
    // `EXT_WORLD` is reached through every window at once, so it arrives with up to
    // `kMaxFrustaPerCell` cones. An instance in any of them is on screen, and one in two of them
    // is still one instance.
    IdRegistry::ResetForTesting();
    ExteriorBvh bvh;
    bvh.Build(APlotOf(4100));
    ExteriorCuller culler;

    const Vector3 eye(0.0F, 1.68F, 0.0F);
    const std::array<ClipFrustum, 1> north{ConeAt(eye, 0.0F)};
    const std::array<ClipFrustum, 1> east{ConeAt(eye, 90.0F)};
    // Overlapping deliberately: 0 and 45 share most of their field.
    const std::array<ClipFrustum, 3> all{ConeAt(eye, 0.0F), ConeAt(eye, 45.0F), ConeAt(eye, 90.0F)};

    culler.Cull(bvh, north, eye);
    const std::set<std::uint32_t> seenNorth = AsSet(culler.Instances());
    const int testedNorth = culler.Statistics().instancesTested;
    culler.Cull(bvh, east, eye);
    const std::set<std::uint32_t> seenEast = AsSet(culler.Instances());
    const int testedEast = culler.Statistics().instancesTested;
    const std::array<ClipFrustum, 1> middle{ConeAt(eye, 45.0F)};
    culler.Cull(bvh, middle, eye);
    const int testedMiddle = culler.Statistics().instancesTested;
    culler.Cull(bvh, all, eye);
    const std::set<std::uint32_t> seenAll = AsSet(culler.Instances());

    ASSERT_FALSE(seenNorth.empty());
    ASSERT_FALSE(seenEast.empty());
    EXPECT_TRUE(std::includes(seenAll.begin(), seenAll.end(), seenNorth.begin(), seenNorth.end()))
        << "an instance the north cone found is missing from the three-cone answer";
    EXPECT_TRUE(std::includes(seenAll.begin(), seenAll.end(), seenEast.begin(), seenEast.end()));
    EXPECT_EQ(seenAll, BruteForce(bvh, all, eye, 1.0F));

    // Ascending and without repeats, which is what makes the answer a set the renderer can walk.
    EXPECT_EQ(culler.Instances().size(), seenAll.size()) << "an instance was listed twice";
    EXPECT_TRUE(std::is_sorted(culler.Instances().begin(), culler.Instances().end()));
    EXPECT_EQ(culler.Statistics().instancesDrawn, static_cast<int>(seenAll.size()));
    // And three cones cost far LESS than three walks, which is what `HOUSE-00699` bought: the
    // hierarchy is walked once carrying all three, so an instance in the overlap -- 0° and 45°
    // share most of their field -- is reached once and tested until the first cone that can see
    // it, instead of once per cone. The bound is what makes this a test rather than a printf: it
    // is not that the union is cheaper on this plot, it is that no instance is visited twice.
    const int testedAll = culler.Statistics().instancesTested;
    std::printf("  one cone versus three: %zu drawn, %d instance test(s) against %d + %d + %d for "
                "the three walks separately\n",
                seenAll.size(),
                testedAll,
                testedNorth,
                testedMiddle,
                testedEast);
    EXPECT_LT(testedAll, testedNorth + testedMiddle + testedEast)
        << "three cones cost three walks, so the hierarchy is still being walked once per cone";
    EXPECT_GE(testedAll, std::max({testedNorth, testedMiddle, testedEast}))
        << "the union tested fewer instances than one of its own cones, which means it missed some";
}

TEST(ExteriorCullingTests, ANodeWhollyInsideAConeStopsTestingItsSubtree)
{
    IdRegistry::ResetForTesting();
    ExteriorBvh bvh;
    bvh.Build(APlotOf(4100));
    ExteriorCuller culler;

    // Standing off the end of the plot looking back along it, which is the pose that puts whole
    // subtrees inside the field: from inside the plot the near leaves straddle the eye and no box
    // can be wholly in front of the near plane.
    const Vector3 eye(0.0F, 1.68F, 350.0F);
    const std::array<ClipFrustum, 1> cones{ConeAt(eye, 0.0F)};
    culler.Cull(bvh, cones, eye);

    EXPECT_GT(culler.Statistics().nodesFullyInside, 0)
        << "no node was ever found wholly inside, so the shortcut is never taken";
    // And it is carried DOWN, not merely noticed: a node under one that was found inside tests
    // nothing at all against the cone.
    EXPECT_GT(culler.Statistics().nodesSkippedFrustumTest, 0)
        << "every node tested itself, so the flag is noticed and not carried down";
    EXPECT_LT(culler.Statistics().frustumTests,
              culler.Statistics().nodesVisited + culler.Statistics().instancesTested)
        << "every node and instance was tested against the cone, so the shortcut stops at the "
           "node that found it";
    // Correct with the shortcut taken, which is the only thing that makes it worth taking.
    EXPECT_EQ(AsSet(culler.Instances()), BruteForce(bvh, cones, eye, 1.0F));
    // Inside a fully-contained subtree the frustum test is skipped, so nothing there can be
    // rejected by it.
    EXPECT_GE(culler.Statistics().instancesTested,
              culler.Statistics().instancesCulledByFrustum + culler.Statistics().instancesDrawn);
}

TEST(ExteriorCullingTests, TheDistanceIsToTheBoxAndNotToItsCentre)
{
    // A fence run is 400 m long. Measured from its centre it is 200 m from either end of the plot
    // and culled at 120; measured from the box it is under the player's hand.
    IdRegistry::ResetForTesting();
    std::vector<ExteriorInstance> instances;
    ExteriorInstance fence;
    fence.id = cnahouse::util::Intern("FENCE_WEST");
    fence.category = PropCategory::Fence;
    fence.bounds = BoundingBox(Vector3(-200.0F, 0.0F, -1.0F), Vector3(200.0F, 1.8F, -0.9F));
    instances.push_back(fence);
    ExteriorBvh bvh;
    bvh.Build(instances);

    ExteriorCuller culler;
    // Standing at one end of it, looking along it. Its centre is 190 m away, past §25.6's 120 m
    // for a fence; its nearest point is under a metre.
    const Vector3 eye(-190.0F, 1.68F, 0.0F);
    // The arithmetic itself, and DIAGONALLY -- because the walk and the brute-force reference
    // share this function, so an error in it moves both answers together and the comparison
    // cannot see it. A unit cube and a point 3 m off one face and 4 m off another is 5 m away,
    // not 7: the length of the offset, not the sum of its parts.
    const BoundingBox unit(Vector3(-1.0F, -1.0F, -1.0F), Vector3(1.0F, 1.0F, 1.0F));
    EXPECT_FLOAT_EQ(DistanceToBox(unit, Vector3(4.0F, 0.0F, 0.0F)), 3.0F);
    EXPECT_FLOAT_EQ(DistanceToBox(unit, Vector3(4.0F, 5.0F, 0.0F)), 5.0F);
    EXPECT_FLOAT_EQ(DistanceToBox(unit, Vector3(-4.0F, -5.0F, 1.0F)), 5.0F) << "and in the other corner";
    EXPECT_FLOAT_EQ(DistanceToBox(unit, Vector3(0.0F, 0.0F, 0.0F)), 0.0F)
        << "a point inside the box is zero metres from it";
    EXPECT_FLOAT_EQ(DistanceToBox(unit, Vector3(1.0F, 1.0F, 1.0F)), 0.0F) << "and so is one on its face";
    EXPECT_FLOAT_EQ(DistanceToBox(fence.bounds, Vector3(0.0F, 1.0F, -0.95F)), 0.0F);

    const std::array<ClipFrustum, 1> cones{ConeAt(eye, 90.0F)};
    culler.Cull(bvh, cones, eye);
    EXPECT_EQ(culler.Instances().size(), 1U) << "the fence under the player's hand was culled";
}

TEST(ExteriorCullingTests, NoConesMeansTheExteriorWasNotReachedAtAll)
{
    // An empty span is NOT the identity frustum: `EXT_WORLD` that the portal walk never reached
    // draws nothing, and a `ClipFrustum` with no planes contains everything.
    IdRegistry::ResetForTesting();
    ExteriorBvh bvh;
    bvh.Build(APlotOf(600));
    ExteriorCuller culler;
    culler.Cull(bvh, {}, Vector3(0.0F, 1.68F, 0.0F));
    EXPECT_TRUE(culler.Instances().empty());
    EXPECT_EQ(culler.Statistics().nodesVisited, 0);
    EXPECT_EQ(culler.Statistics().instancesDrawn, 0);

    // And a cone with no planes DOES contain everything, which is the identity the traversal
    // starts from -- the difference is the span, not the frustum.
    const std::array<ClipFrustum, 1> everything{ClipFrustum{}};
    culler.Cull(bvh, everything, Vector3(0.0F, 1.68F, 0.0F));
    EXPECT_GT(culler.Instances().size(), 0U);
    EXPECT_EQ(AsSet(culler.Instances()), BruteForce(bvh, everything, Vector3(0.0F, 1.68F, 0.0F), 1.0F));
}

TEST(ExteriorCullingTests, AnEmptyExteriorIsNotAnError)
{
    IdRegistry::ResetForTesting();
    ExteriorBvh bvh;
    ExteriorCuller culler;
    const Vector3 eye(0.0F, 1.68F, 0.0F);
    const std::array<ClipFrustum, 1> cones{ConeAt(eye, 0.0F)};
    culler.Cull(bvh, cones, eye);
    EXPECT_TRUE(culler.Instances().empty());
    EXPECT_EQ(culler.Statistics().nodesVisited, 0);
}

TEST(ExteriorCullingTests, TheSubtreeSummaryRetiresWholeBranchesByDistance)
{
    // What the node's `maxCullDistance` is for: a cluster of small props 300 m away is one
    // comparison, not 500. Two clusters, one near and one far, and the far one costs a node.
    IdRegistry::ResetForTesting();
    std::vector<ExteriorInstance> instances;
    for (int i = 0; i < 500; ++i)
    {
        instances.push_back(Instance(
            "NEAR_" + std::to_string(i),
            PropCategory::SmallProp,
            Vector3(static_cast<float>(i % 25) * 0.8F, 0.0F, -10.0F - static_cast<float>(i / 25) * 0.8F),
            0.3F));
    }
    for (int i = 0; i < 500; ++i)
    {
        instances.push_back(Instance(
            "FAR_" + std::to_string(i),
            PropCategory::SmallProp,
            Vector3(static_cast<float>(i % 25) * 0.8F, 0.0F, -300.0F - static_cast<float>(i / 25) * 0.8F),
            0.3F));
    }
    // A third cluster ON THE BOUNDARY §68's setting moves: 55 m is past a small prop's 45 m at
    // 1.0x and inside its 63 m at 1.4x. A node rejected against its UNSCALED distance culls this
    // whole cluster at the widest setting, and nothing else in this file is close enough to a
    // node's own limit to notice.
    for (int i = 0; i < 200; ++i)
    {
        instances.push_back(Instance(
            "MID_" + std::to_string(i),
            PropCategory::SmallProp,
            Vector3(static_cast<float>(i % 20) * 0.5F, 0.0F, -55.0F - static_cast<float>(i / 20) * 0.5F),
            0.3F));
    }
    ExteriorBvh bvh;
    bvh.Build(instances);
    ExteriorCuller culler;
    const Vector3 eye(10.0F, 1.68F, 0.0F);
    const std::array<ClipFrustum, 1> cones{ConeAt(eye, 0.0F)};
    culler.Cull(bvh, cones, eye);

    std::printf("  1 200 small props, 500 of them 300 m away: %d node(s) visited, %d retired by "
                "distance, %d instance test(s)\n",
                culler.Statistics().nodesVisited,
                culler.Statistics().nodesCulledByDistance,
                culler.Statistics().instancesTested);
    EXPECT_GT(culler.Statistics().nodesCulledByDistance, 0)
        << "not one branch was retired by distance, so the node summary is doing nothing";
    EXPECT_LT(culler.Statistics().instancesTested, 900) << "the far cluster was tested instance by instance";

    // Every setting, against brute force: a node's rejection has to use the SAME scaled distance
    // the instances inside it will be tested by, or the widest setting culls the mid cluster.
    for (const float scale : {0.6F, 1.0F, 1.2F, 1.4F})
    {
        culler.Cull(bvh, cones, eye, scale);
        EXPECT_EQ(AsSet(culler.Instances()), BruteForce(bvh, cones, eye, scale))
            << "at view distance " << scale;
    }
    culler.Cull(bvh, cones, eye, 1.0F);
    const std::size_t atOne = culler.Instances().size();
    culler.Cull(bvh, cones, eye, 1.4F);
    EXPECT_GT(culler.Instances().size(), atOne)
        << "the mid cluster is not on the boundary the setting moves, so nothing was proved";
}
