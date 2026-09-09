// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/visibility/CullDistance.hpp"

namespace cnahouse::visibility
{

    /// @brief One static thing outdoors, as §25.6's hierarchy needs to see it (`HOUSE-00677`).
    ///
    /// §25.6 lists what they are: *"terrain tiles, road segments, fences, trees, neighbourhood
    /// groups, garden props"* -- about 4 100 of them in one cell, `EXT_WORLD`, where portal
    /// traversal cannot help at all.
    ///
    /// **A box and not a sphere**, which is the opposite of `DynamicInstance`'s choice and for the
    /// same reason: these do not move. A sphere is the bound that survives rotation, and nothing
    /// here rotates -- a fence run, a terrain tile and a road segment are all long and thin, and a
    /// sphere around one is mostly air.
    struct ExteriorInstance
    {
        util::Id id;
        /// @brief §25.6's step 2, carried on the instance because step 1 wants it too: a node that
        ///        holds nothing but trees can be rejected whole at 180 m.
        PropCategory category = PropCategory::SmallProp;
        Microsoft::Xna::Framework::BoundingBox bounds;
    };

    /// @brief One node of §25.6's hierarchy.
    ///
    /// A node is either interior -- `childCount` children starting at `firstChild` -- or a leaf,
    /// with `count` instances starting at `first` in `ExteriorBvh::Order()`. `childCount == 0`
    /// is what says which.
    struct BvhNode
    {
        Microsoft::Xna::Framework::BoundingBox bounds;
        /// @brief Which categories are anywhere under this node, one bit per `PropCategory`.
        ///
        /// The reason step 1 and step 2 meet in the hierarchy rather than after it: a subtree of
        /// nothing but fences is finished at 120 m, and the test is an `and` of two bytes.
        std::uint8_t categories = 0u;
        /// @brief The largest §25.6 distance any instance under this node is drawn at.
        ///
        /// Derived from `categories` at build time so the traversal's distance rejection is one
        /// comparison rather than a loop over eight bits. Metres, before §68's view-distance
        /// scale, which is a per-frame multiplier and therefore not baked in here.
        float maxCullDistance = 0.0F;
        std::uint32_t firstChild = 0u;
        std::uint8_t childCount = 0u;
        /// @brief Leaf only: the range in `Order()`.
        std::uint32_t first = 0u;
        std::uint32_t count = 0u;

        [[nodiscard]] bool IsLeaf() const noexcept
        {
            return childCount == 0u;
        }
    };

    /// @brief §25.6's *"loose 3-level bounding-volume hierarchy built at load time"*
    ///        (`HOUSE-00677`).
    ///
    /// **Why a hierarchy at all, when the rest of §25 is portals.** `EXT_WORLD` is one enormous
    /// cell, so the portal walk reaches it and then has nothing more to say: every exterior
    /// instance is in the one visible cell. Frustum-testing 4 100 boxes one at a time is 4 100
    /// tests for a frame that draws a few hundred of them, and §71.2 gives the whole visibility
    /// stage 0.55 ms.
    ///
    /// **Three levels, eight ways.** §25.6 asks for three, and eight children per level puts
    /// 4 100 instances into at most 64 leaves of about 64 each -- so a frustum that sees a wedge
    /// of the plot rejects most of it in 8 tests and refines in 8 more. Each level is three median
    /// splits along the longest axis of what it is splitting, which is what makes the eight
    /// children a spatial partition rather than an arbitrary eighth of a list.
    ///
    /// **"Loose" means an instance belongs to exactly one node.** The alternative -- a strict grid
    /// -- has to either split an instance that straddles a boundary or store it in every cell it
    /// touches, and a fence run 40 m long straddles everything. Here a node's box is the UNION of
    /// what is under it, so sibling boxes overlap where their contents do, and no instance is ever
    /// duplicated or cut. The cost is that a point can be inside two siblings; the traversal
    /// visits both, which is correct, and it is the reason this is not a quadtree.
    ///
    /// **Built once.** Everything in it is static (§25.6), so there is no insert, no remove and no
    /// refit -- and no code for them, because a BVH that could be edited would need a balance
    /// policy nothing in this project has a use for.
    class ExteriorBvh
    {
    public:
        /// @brief §25.6's three, counting the root.
        static constexpr int kLevels = 3;
        /// @brief Children per interior node: three median splits.
        static constexpr std::size_t kBranching = 8;
        /// @brief A node with no more than this is a leaf whatever level it is on.
        ///
        /// Splitting eight ways below this makes nodes that cost more to test than their contents
        /// would: at eight instances a node's own box test plus eight children is already more
        /// work than eight box tests.
        static constexpr std::size_t kMinToSplit = 8;

        struct Stats
        {
            int instances = 0;
            int nodes = 0;
            int leaves = 0;
            int maxLeafSize = 0;
            int levels = 0;
        };

        /// @brief Builds the hierarchy over @p instances, which are copied.
        ///
        /// Copied rather than borrowed because the tree reorders them: a leaf is a contiguous range
        /// so that traversal walks memory forwards, and a caller's vector is not ours to permute.
        void Build(std::span<const ExteriorInstance> instances);

        [[nodiscard]] std::span<const BvhNode> Nodes() const noexcept
        {
            return nodes_;
        }

        /// @brief The instances, in the order the build left them: every leaf is a range of this.
        [[nodiscard]] std::span<const ExteriorInstance> Instances() const noexcept
        {
            return instances_;
        }

        /// @brief The root, or `nullptr` when nothing was built.
        [[nodiscard]] const BvhNode* Root() const noexcept
        {
            return nodes_.empty() ? nullptr : &nodes_.front();
        }

        [[nodiscard]] const Stats& Statistics() const noexcept
        {
            return stats_;
        }

    private:
        std::vector<ExteriorInstance> instances_;
        std::vector<BvhNode> nodes_;
        Stats stats_;

        /// @brief Fills node @p self, which already exists, over `[first, first + count)`.
        ///
        /// The node is allocated by its PARENT, with its siblings, before any of them is filled --
        /// which is what makes `firstChild` plus an index name a child rather than a grandchild.
        void FillNode(std::uint32_t self, std::uint32_t first, std::uint32_t count, int level);
    };

    /// @brief The bit `BvhNode::categories` uses for @p category.
    [[nodiscard]] constexpr std::uint8_t CategoryBit(PropCategory category) noexcept
    {
        return static_cast<std::uint8_t>(1u << static_cast<unsigned>(category));
    }

} // namespace cnahouse::visibility
