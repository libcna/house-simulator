// SPDX-License-Identifier: MIT
//
// `HOUSE-00677`. §25.6's *"loose 3-level bounding-volume hierarchy built at load time over the
// ~4 100 exterior instances"*.
//
// `EXT_WORLD` is one cell, so the portal walk reaches it and has nothing more to say: this is the
// only structure between the frame and every tree, fence and neighbouring house at once. What is
// checked here is what a hierarchy has to be right about before a traversal can trust it -- that
// it holds everything exactly once, that a node's box really contains its subtree, and that the
// category summary a node carries is the truth about what is under it and not an approximation.
#include <algorithm>
#include <array>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/visibility/ExteriorBvh.hpp"

namespace
{
    using cnahouse::util::Id;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::BvhNode;
    using cnahouse::visibility::CategoryBit;
    using cnahouse::visibility::ExteriorBvh;
    using cnahouse::visibility::ExteriorInstance;
    using cnahouse::visibility::kCullDistances;
    using cnahouse::visibility::PropCategory;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    /// A stated generator rather than `<random>`'s distributions, whose output is not specified
    /// by the standard: this test's numbers have to be the same on every machine that runs it.
    class Numbers
    {
    public:
        explicit Numbers(std::uint32_t seed) noexcept
            : state_(seed)
        {
        }

        /// @brief The next value in `[low, high)`.
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
    Instance(const std::string& name, PropCategory category, const Vector3& centre, const Vector3& half)
    {
        ExteriorInstance instance;
        instance.id = cnahouse::util::Intern(name);
        instance.category = category;
        instance.bounds = BoundingBox(Vector3(centre.X - half.X, centre.Y - half.Y, centre.Z - half.Z),
                                      Vector3(centre.X + half.X, centre.Y + half.Y, centre.Z + half.Z));
        return instance;
    }

    /// §25.6's plot: 400 m across, a few metres tall, with the categories §25.6 names.
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
            const float radius = kind == PropCategory::Tree ? 3.0F : 0.6F;
            instances.push_back(Instance("EXT_" + std::to_string(i),
                                         kind,
                                         Vector3(numbers.Next(-200.0F, 200.0F),
                                                 numbers.Next(0.0F, 6.0F),
                                                 numbers.Next(-200.0F, 200.0F)),
                                         Vector3(radius, radius, radius)));
        }
        return instances;
    }

    bool Contains(const BoundingBox& outer, const BoundingBox& inner) noexcept
    {
        return outer.Min.X <= inner.Min.X && outer.Min.Y <= inner.Min.Y && outer.Min.Z <= inner.Min.Z &&
               outer.Max.X >= inner.Max.X && outer.Max.Y >= inner.Max.Y && outer.Max.Z >= inner.Max.Z;
    }

    /// Every leaf's range, in the order a depth-first walk finds them.
    std::vector<const BvhNode*> LeavesOf(const ExteriorBvh& bvh, const BvhNode& node)
    {
        if (node.IsLeaf())
        {
            return {&node};
        }
        std::vector<const BvhNode*> leaves;
        for (std::uint8_t child = 0; child < node.childCount; ++child)
        {
            const BvhNode& kid = bvh.Nodes()[node.firstChild + child];
            const std::vector<const BvhNode*> below = LeavesOf(bvh, kid);
            leaves.insert(leaves.end(), below.begin(), below.end());
        }
        return leaves;
    }

} // namespace

TEST(ExteriorBvhTests, EveryInstanceIsUnderExactlyOneLeaf)
{
    // The property everything else rests on. A hierarchy that dropped an instance would cull
    // something that is there, and one that held it twice would draw it twice -- and both look
    // like a traversal bug from the outside.
    IdRegistry::ResetForTesting();
    const std::vector<ExteriorInstance> plot = APlotOf(4100);
    ExteriorBvh bvh;
    bvh.Build(plot);

    const BvhNode* root = bvh.Root();
    ASSERT_NE(root, nullptr);
    std::vector<int> seen(plot.size(), 0);
    std::size_t total = 0;
    for (const BvhNode* leaf : LeavesOf(bvh, *root))
    {
        for (std::uint32_t i = leaf->first; i < leaf->first + leaf->count; ++i)
        {
            ASSERT_LT(i, plot.size());
            ++seen[i];
        }
        total += leaf->count;
    }
    EXPECT_EQ(total, plot.size());
    EXPECT_EQ(std::count(seen.begin(), seen.end(), 1), static_cast<long>(plot.size()))
        << "some instance is in no leaf, or in two";

    // And the SAME instances came out, not merely the same number of them.
    std::set<std::uint32_t> before;
    std::set<std::uint32_t> after;
    for (const ExteriorInstance& instance : plot)
    {
        before.insert(instance.id.Value());
    }
    for (const ExteriorInstance& instance : bvh.Instances())
    {
        after.insert(instance.id.Value());
    }
    EXPECT_EQ(before, after);
}

TEST(ExteriorBvhTests, ANodesBoxContainsEverythingUnderIt)
{
    IdRegistry::ResetForTesting();
    ExteriorBvh bvh;
    bvh.Build(APlotOf(4100));
    ASSERT_NE(bvh.Root(), nullptr);

    for (const BvhNode& node : bvh.Nodes())
    {
        for (std::uint32_t i = node.first; i < node.first + node.count; ++i)
        {
            EXPECT_TRUE(Contains(node.bounds, bvh.Instances()[i].bounds))
                << "a node's box does not contain instance " << i << " of its own range";
        }
        for (std::uint8_t child = 0; child < node.childCount; ++child)
        {
            EXPECT_TRUE(Contains(node.bounds, bvh.Nodes()[node.firstChild + child].bounds))
                << "a node's box does not contain a child's";
        }
    }
}

TEST(ExteriorBvhTests, AChildIndexNamesAChildAndNotAGrandchild)
{
    // The bug a flat BVH has exactly once: fill a child before allocating its siblings and its
    // whole subtree lands between the two, so `firstChild + 1` is a grandchild. The ranges are
    // what expose it -- a child's range is inside its parent's and a grandchild's is smaller.
    IdRegistry::ResetForTesting();
    ExteriorBvh bvh;
    bvh.Build(APlotOf(4100));

    std::uint32_t interior = 0;
    for (std::uint32_t n = 0; n < bvh.Nodes().size(); ++n)
    {
        const BvhNode& node = bvh.Nodes()[n];
        if (node.IsLeaf())
        {
            continue;
        }
        ++interior;
        // Eight, always: a node only splits with more than `kMinToSplit` instances, and halving a
        // range of nine or more three times leaves eight parts of at least one each.
        EXPECT_EQ(node.childCount, ExteriorBvh::kBranching) << "node " << n;
        std::uint32_t covered = 0;
        std::uint32_t at = node.first;
        for (std::uint8_t child = 0; child < node.childCount; ++child)
        {
            const BvhNode& kid = bvh.Nodes()[node.firstChild + child];
            EXPECT_EQ(kid.first, at) << "node " << n << "'s children do not tile its range";
            EXPECT_GT(kid.count, 0u) << "an empty child was allocated";
            at += kid.count;
            covered += kid.count;
        }
        EXPECT_EQ(covered, node.count) << "node " << n << "'s children cover the wrong range";
    }
    EXPECT_GT(interior, 0u) << "nothing was split, so nothing was checked";
}

TEST(ExteriorBvhTests, ANodeSummarisesTheCategoriesUnderItExactly)
{
    // §25.6's two steps meet here: a subtree of nothing but fences is finished at 120 m, and the
    // traversal must be able to believe that. A mask that claimed MORE than is under it costs a
    // wasted descent; one that claimed less culls something that is there.
    IdRegistry::ResetForTesting();
    ExteriorBvh bvh;
    bvh.Build(APlotOf(4100));

    for (const BvhNode& node : bvh.Nodes())
    {
        std::uint8_t expected = 0u;
        float distance = 0.0F;
        for (std::uint32_t i = node.first; i < node.first + node.count; ++i)
        {
            const PropCategory category = bvh.Instances()[i].category;
            expected = static_cast<std::uint8_t>(expected | CategoryBit(category));
            distance = std::max(distance, kCullDistances[static_cast<std::size_t>(category)]);
        }
        EXPECT_EQ(node.categories, expected);
        EXPECT_FLOAT_EQ(node.maxCullDistance, distance);
    }
}

TEST(ExteriorBvhTests, AOneCategorySubtreeCarriesOneBitAndThatCategorysDistance)
{
    IdRegistry::ResetForTesting();
    std::vector<ExteriorInstance> trees;
    for (int i = 0; i < 200; ++i)
    {
        trees.push_back(Instance("TREE_" + std::to_string(i),
                                 PropCategory::Tree,
                                 Vector3(static_cast<float>(i), 0.0F, 0.0F),
                                 Vector3(3.0F, 6.0F, 3.0F)));
    }
    ExteriorBvh bvh;
    bvh.Build(trees);
    ASSERT_NE(bvh.Root(), nullptr);
    for (const BvhNode& node : bvh.Nodes())
    {
        EXPECT_EQ(node.categories, CategoryBit(PropCategory::Tree));
        // §25.6's trees: 180 m, and nothing under this node outlives it.
        EXPECT_FLOAT_EQ(node.maxCullDistance, 180.0F);
    }
}

TEST(ExteriorBvhTests, ThreeLevelsEightWaysAtSection25Point6sScale)
{
    IdRegistry::ResetForTesting();
    ExteriorBvh bvh;
    bvh.Build(APlotOf(4100));
    const ExteriorBvh::Stats& stats = bvh.Statistics();
    std::printf("  4 100 instances: %d node(s), %d leaf/leaves, %d levels, worst leaf %d\n",
                stats.nodes,
                stats.leaves,
                stats.levels,
                stats.maxLeafSize);

    EXPECT_EQ(stats.instances, 4100);
    EXPECT_EQ(stats.levels, ExteriorBvh::kLevels) << "§25.6 asks for three";
    // 1 + 8 + 64 is the most three levels of eight can be.
    EXPECT_LE(stats.nodes, 1 + 8 + 64);
    EXPECT_EQ(stats.leaves, 64) << "the plot is big enough to fill the tree";
    // 4 100 over 64 leaves is 64 each, and the median split keeps them even to within a couple.
    EXPECT_LE(stats.maxLeafSize, 70);
    EXPECT_GE(stats.maxLeafSize, 60);
    const BvhNode* root = bvh.Root();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->count, 4100u);
    EXPECT_EQ(root->childCount, 8u);
}

TEST(ExteriorBvhTests, TheChildrenPartitionSPACEAndNotMerelyTheList)
{
    // A hierarchy whose every node's box is the whole plot is a correct hierarchy that rejects
    // nothing: the counts would still be even, the ranges would still tile, and the traversal
    // would still visit all 4 100 instances. What makes it worth building is that a child's box
    // is a small part of its parent's, and that is a property of the SPLIT AXIS -- which is why
    // the axis is re-chosen from the geometry at every cut.
    IdRegistry::ResetForTesting();
    ExteriorBvh bvh;
    bvh.Build(APlotOf(4100));
    const BvhNode* rootNode = bvh.Root();
    ASSERT_NE(rootNode, nullptr);

    const BvhNode& root = *rootNode;
    const double rootArea = static_cast<double>(root.bounds.Max.X - root.bounds.Min.X) *
                            static_cast<double>(root.bounds.Max.Z - root.bounds.Min.Z);
    ASSERT_GT(rootArea, 0.0);

    double worst = 0.0;
    double total = 0.0;
    for (std::uint8_t child = 0; child < root.childCount; ++child)
    {
        const BvhNode& kid = bvh.Nodes()[root.firstChild + child];
        const double area = static_cast<double>(kid.bounds.Max.X - kid.bounds.Min.X) *
                            static_cast<double>(kid.bounds.Max.Z - kid.bounds.Min.Z);
        worst = std::max(worst, area / rootArea);
        total += area / rootArea;
    }
    // AREA is not the test, and this is why: six cuts all on X give sixty-four slabs of a
    // sixty-fourth of the plot each, exactly the area a good split gives. What separates them is
    // the SHAPE -- a 6 m by 400 m slab intersects nearly every frustum, and a 50 m square does
    // not -- so the claim is about how square a leaf is.
    std::vector<double> aspects;
    for (const BvhNode* leaf : LeavesOf(bvh, root))
    {
        const double dx = static_cast<double>(leaf->bounds.Max.X - leaf->bounds.Min.X);
        const double dz = static_cast<double>(leaf->bounds.Max.Z - leaf->bounds.Min.Z);
        ASSERT_GT(std::min(dx, dz), 0.0);
        aspects.push_back(std::max(dx, dz) / std::min(dx, dz));
    }
    std::sort(aspects.begin(), aspects.end());
    ASSERT_FALSE(aspects.empty());
    const double median = aspects[aspects.size() / 2];
    std::printf("  the root's 8 children cover %.2f of its footprint each on average (worst %.2f); "
                "the median leaf is %.2f times longer than it is wide, the worst %.2f\n",
                total / 8.0,
                worst,
                median,
                aspects.back());

    // Measured at 1.10 median and 1.32 worst on this plot; every cut on one axis gives 35 and 43.
    EXPECT_LT(median, 2.0) << "the leaves are slabs, so the split is not choosing its axis";
    EXPECT_LT(aspects.back(), 4.0) << "some leaf is a slab";
    EXPECT_LT(worst, 0.45) << "a child covers nearly the whole plot";
    EXPECT_LT(total, 2.5) << "the children together cover the plot more than twice over";
}

TEST(ExteriorBvhTests, ALongInstanceIsHeldOnceRatherThanSplitOrDuplicated)
{
    // The loose property, and the reason this is not a grid. A fence run crosses the whole plot
    // and touches every cell of any partition that could be drawn on it; a grid has to cut it or
    // store it eight times, and this stores it once and lets the sibling boxes overlap.
    IdRegistry::ResetForTesting();
    std::vector<ExteriorInstance> instances = APlotOf(600);
    instances.push_back(
        Instance("FENCE_WEST", PropCategory::Fence, Vector3(0.0F, 1.0F, 0.0F), Vector3(200.0F, 0.9F, 0.1F)));
    ExteriorBvh bvh;
    bvh.Build(instances);

    int found = 0;
    const Id fence = cnahouse::util::Intern("FENCE_WEST");
    for (const ExteriorInstance& instance : bvh.Instances())
    {
        if (instance.id == fence)
        {
            ++found;
        }
    }
    EXPECT_EQ(found, 1) << "the fence was duplicated or lost";
    EXPECT_EQ(bvh.Statistics().instances, 601);

    // And its own node's box really does hold all 400 m of it, so the traversal that reaches the
    // node reaches the fence.
    const BvhNode* rootNode = bvh.Root();
    ASSERT_NE(rootNode, nullptr);
    bool held = false;
    for (const BvhNode* leaf : LeavesOf(bvh, *rootNode))
    {
        for (std::uint32_t i = leaf->first; i < leaf->first + leaf->count; ++i)
        {
            if (bvh.Instances()[i].id == fence)
            {
                held = true;
                EXPECT_TRUE(Contains(leaf->bounds, bvh.Instances()[i].bounds));
            }
        }
    }
    EXPECT_TRUE(held);

    // Sibling boxes overlapping is the COST of that, and it is real -- recorded rather than
    // asserted away, because a traversal that assumed disjoint siblings would be wrong.
    int overlapping = 0;
    const BvhNode& root = *rootNode;
    for (std::uint8_t a = 0; a < root.childCount; ++a)
    {
        for (std::uint8_t b = static_cast<std::uint8_t>(a + 1); b < root.childCount; ++b)
        {
            const BvhNode& first = bvh.Nodes()[root.firstChild + a];
            const BvhNode& second = bvh.Nodes()[root.firstChild + b];
            if (first.bounds.Min.X <= second.bounds.Max.X && first.bounds.Max.X >= second.bounds.Min.X &&
                first.bounds.Min.Z <= second.bounds.Max.Z && first.bounds.Max.Z >= second.bounds.Min.Z)
            {
                ++overlapping;
            }
        }
    }
    std::printf("  %d of the root's 28 sibling pairs overlap, which is what \"loose\" costs\n", overlapping);
    EXPECT_GT(overlapping, 0) << "no sibling overlaps at all, so this is a grid and not a loose BVH";
}

TEST(ExteriorBvhTests, TheSameInstancesInAnotherOrderGiveTheSameTree)
{
    // Built at LOAD time from a file, and the file's order is not something a frame should depend
    // on. The median split is a total order -- the centre, then the id -- so which side of a cut
    // an instance lands on is a fact about the instance and not about where it was in the list.
    IdRegistry::ResetForTesting();
    const std::vector<ExteriorInstance> plot = APlotOf(1000);
    std::vector<ExteriorInstance> shuffled = plot;
    // A stated permutation rather than `std::shuffle`, whose result is implementation-defined.
    std::rotate(shuffled.begin(), shuffled.begin() + 337, shuffled.end());
    std::reverse(shuffled.begin(), shuffled.begin() + 500);

    ExteriorBvh one;
    ExteriorBvh two;
    one.Build(plot);
    two.Build(shuffled);

    ASSERT_EQ(one.Nodes().size(), two.Nodes().size());
    for (std::size_t n = 0; n < one.Nodes().size(); ++n)
    {
        const BvhNode& a = one.Nodes()[n];
        const BvhNode& b = two.Nodes()[n];
        EXPECT_EQ(a.first, b.first) << "node " << n;
        EXPECT_EQ(a.count, b.count) << "node " << n;
        EXPECT_EQ(a.childCount, b.childCount) << "node " << n;
        EXPECT_EQ(a.categories, b.categories) << "node " << n;
        EXPECT_FLOAT_EQ(a.bounds.Min.X, b.bounds.Min.X) << "node " << n;
        EXPECT_FLOAT_EQ(a.bounds.Max.Z, b.bounds.Max.Z) << "node " << n;
    }

    // The leaves hold the same instances, which is the claim that matters: within one leaf the
    // order is `nth_element`'s business and the traversal reads the whole leaf anyway.
    const BvhNode* rootOne = one.Root();
    const BvhNode* rootTwo = two.Root();
    ASSERT_NE(rootOne, nullptr);
    ASSERT_NE(rootTwo, nullptr);
    const std::vector<const BvhNode*> leavesOne = LeavesOf(one, *rootOne);
    const std::vector<const BvhNode*> leavesTwo = LeavesOf(two, *rootTwo);
    ASSERT_EQ(leavesOne.size(), leavesTwo.size());
    for (std::size_t leaf = 0; leaf < leavesOne.size(); ++leaf)
    {
        std::set<std::uint32_t> first;
        std::set<std::uint32_t> second;
        for (std::uint32_t i = leavesOne[leaf]->first; i < leavesOne[leaf]->first + leavesOne[leaf]->count;
             ++i)
        {
            first.insert(one.Instances()[i].id.Value());
        }
        for (std::uint32_t i = leavesTwo[leaf]->first; i < leavesTwo[leaf]->first + leavesTwo[leaf]->count;
             ++i)
        {
            second.insert(two.Instances()[i].id.Value());
        }
        EXPECT_EQ(first, second) << "leaf " << leaf << " holds different instances";
    }
}

TEST(ExteriorBvhTests, InstancesAtTheSamePlaceAreSplitTheSameWayEveryTime)
{
    // What the id tie-break is for, and the case the random plot above cannot produce: a run of
    // identical fence posts has identical centres, so the median split has nothing geometric to
    // separate them by. `nth_element` is not stable, so without a tie-break which posts land on
    // which side is introselect's business -- and a load-time structure whose shape depends on
    // that is one whose frame does.
    IdRegistry::ResetForTesting();
    std::vector<ExteriorInstance> posts;
    for (int i = 0; i < 64; ++i)
    {
        posts.push_back(Instance("POST_" + std::to_string(i),
                                 PropCategory::Fence,
                                 Vector3(10.0F, 1.0F, -4.0F),
                                 Vector3(0.05F, 0.9F, 0.05F)));
    }
    for (int i = 0; i < 64; ++i)
    {
        posts.push_back(Instance("POST_FAR_" + std::to_string(i),
                                 PropCategory::Fence,
                                 Vector3(-90.0F, 1.0F, 60.0F),
                                 Vector3(0.05F, 0.9F, 0.05F)));
    }

    std::vector<ExteriorInstance> other = posts;
    std::reverse(other.begin(), other.end());

    ExteriorBvh one;
    ExteriorBvh two;
    one.Build(posts);
    two.Build(other);

    ASSERT_EQ(one.Nodes().size(), two.Nodes().size());
    // The LEAF is what has to be the same, not the order inside it: `nth_element` places the
    // median and partitions around it, and says nothing about the order on either side. The
    // traversal reads a whole leaf, so a leaf is a set -- and the claim is that the SETS are a
    // fact about the posts rather than about which end of the file they were written at.
    const BvhNode* rootOne = one.Root();
    const BvhNode* rootTwo = two.Root();
    ASSERT_NE(rootOne, nullptr);
    ASSERT_NE(rootTwo, nullptr);
    const std::vector<const BvhNode*> leavesOne = LeavesOf(one, *rootOne);
    const std::vector<const BvhNode*> leavesTwo = LeavesOf(two, *rootTwo);
    ASSERT_EQ(leavesOne.size(), leavesTwo.size());
    ASSERT_GT(leavesOne.size(), 1U) << "one leaf would make the claim vacuous";
    for (std::size_t leaf = 0; leaf < leavesOne.size(); ++leaf)
    {
        ASSERT_EQ(leavesOne[leaf]->first, leavesTwo[leaf]->first);
        ASSERT_EQ(leavesOne[leaf]->count, leavesTwo[leaf]->count);
        std::set<std::uint32_t> first;
        std::set<std::uint32_t> second;
        for (std::uint32_t i = leavesOne[leaf]->first; i < leavesOne[leaf]->first + leavesOne[leaf]->count;
             ++i)
        {
            first.insert(one.Instances()[i].id.Value());
            second.insert(two.Instances()[i].id.Value());
        }
        EXPECT_EQ(first, second) << "leaf " << leaf << " holds different posts";
    }
}

TEST(ExteriorBvhTests, ASmallPlotIsOneLeafAndAnEmptyOneIsNoTree)
{
    IdRegistry::ResetForTesting();
    ExteriorBvh bvh;
    bvh.Build({});
    EXPECT_EQ(bvh.Root(), nullptr) << "an empty exterior has no root to traverse";
    EXPECT_EQ(bvh.Statistics().instances, 0);
    EXPECT_EQ(bvh.Statistics().nodes, 0);
    EXPECT_TRUE(bvh.Nodes().empty());

    // And emptying a tree that HAD something in it leaves nothing behind, which is the direction
    // that can leave a stale root pointing at instances that are gone.
    bvh.Build(APlotOf(200));
    ASSERT_NE(bvh.Root(), nullptr);
    bvh.Build({});
    EXPECT_EQ(bvh.Root(), nullptr) << "the old tree survived a rebuild from nothing";
    EXPECT_TRUE(bvh.Nodes().empty());
    EXPECT_TRUE(bvh.Instances().empty());
    EXPECT_EQ(bvh.Statistics().nodes, 0);

    // At or below `kMinToSplit` a node stays whole: eight children plus their boxes is more work
    // than testing eight instances.
    std::vector<ExteriorInstance> few;
    for (int i = 0; i < static_cast<int>(ExteriorBvh::kMinToSplit); ++i)
    {
        few.push_back(Instance("BIN_" + std::to_string(i),
                               PropCategory::SmallProp,
                               Vector3(static_cast<float>(i) * 5.0F, 0.0F, 0.0F),
                               Vector3(0.4F, 0.5F, 0.4F)));
    }
    bvh.Build(few);
    const BvhNode* root = bvh.Root();
    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(root->IsLeaf());
    EXPECT_EQ(bvh.Statistics().nodes, 1);
    EXPECT_EQ(bvh.Statistics().leaves, 1);
    EXPECT_EQ(bvh.Statistics().levels, 1);
    EXPECT_EQ(root->count, ExteriorBvh::kMinToSplit);

    // One more and it splits.
    few.push_back(Instance(
        "BIN_EXTRA", PropCategory::SmallProp, Vector3(99.0F, 0.0F, 0.0F), Vector3(0.4F, 0.5F, 0.4F)));
    bvh.Build(few);
    const BvhNode* split = bvh.Root();
    ASSERT_NE(split, nullptr);
    EXPECT_FALSE(split->IsLeaf());
    EXPECT_GT(bvh.Statistics().leaves, 1);
}

TEST(ExteriorBvhTests, RebuildingReplacesTheTreeRatherThanAddingToIt)
{
    IdRegistry::ResetForTesting();
    ExteriorBvh bvh;
    bvh.Build(APlotOf(300));
    const int firstNodes = bvh.Statistics().nodes;
    EXPECT_EQ(bvh.Statistics().instances, 300);
    bvh.Build(APlotOf(300));
    EXPECT_EQ(bvh.Statistics().instances, 300);
    EXPECT_EQ(bvh.Statistics().nodes, firstNodes);
    EXPECT_EQ(bvh.Instances().size(), 300u);
}
