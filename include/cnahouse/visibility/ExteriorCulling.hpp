// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/visibility/ClipFrustum.hpp"
#include "cnahouse/visibility/ExteriorBvh.hpp"
#include "cnahouse/visibility/PortalArea.hpp"
#include "cnahouse/visibility/PortalTraversal.hpp"

namespace cnahouse::world
{
    class WorldData;
}

namespace cnahouse::visibility
{

    /// @brief §25.6's steps 1 and 2 over `HOUSE-00677`'s hierarchy (`HOUSE-00678`).
    ///
    /// *"Frustum culling of a loose 3-level bounding-volume hierarchy ... distance culling per
    /// category."* The two are one walk and not two passes, because that is where they are cheap:
    /// a node carries the largest §25.6 distance anything under it is drawn at, so a subtree of
    /// fences 200 m away is rejected by one comparison instead of by testing every fence in it.
    ///
    /// **The distance is to the instance's BOX, not to its centre.** A fence run is 40 m long and
    /// a terrain tile is 80 m square; measuring from the centre would cull the half of a fence the
    /// player is standing next to, because the other end of it is far away. Distance to the box is
    /// zero when the eye is inside it, which is the right answer for a terrain tile.
    ///
    /// **A node fully inside a cone stops testing.** Its whole subtree is inside too, so the
    /// frustum test is skipped from there down and only the distance test remains -- which is most
    /// of the win on the frames that draw the most.
    class ExteriorCuller
    {
    public:
        struct Stats
        {
            int nodesVisited = 0;
            int nodesCulledByFrustum = 0;
            int nodesCulledByDistance = 0;
            /// @brief Nodes found wholly inside a cone, whose subtree therefore skipped the
            ///        frustum test. The number that says whether that shortcut is worth its branch.
            int nodesFullyInside = 0;
            /// @brief Instance tests done. Larger than `instancesDrawn + culled` when `EXT_WORLD`
            ///        is reached through more than one opening: the second cone retests what the
            ///        first already accepted, and this counts WORK.
            int instancesTested = 0;
            int instancesCulledByFrustum = 0;
            int instancesCulledByDistance = 0;
            /// @brief The unique instances in `Instances()`, whatever how many cones found them.
            int instancesDrawn = 0;
            /// @brief Nodes entered already known to be inside a cone, which therefore tested
            ///        nothing against it.
            ///
            /// Zero means the fully-inside flag is being NOTICED but not carried down: every node
            /// would then test itself, and the shortcut would save only the instances in a leaf
            /// that is wholly inside on its own.
            int nodesSkippedFrustumTest = 0;
            /// @brief Plane tests against a cone -- nodes and instances together.
            ///
            /// The number the fully-inside shortcut exists to lower, and the only way to see that
            /// it is being carried DOWN rather than merely noticed: without the flag being passed
            /// to the children, every node and every instance is tested and this equals
            /// `nodesVisited + instancesTested` exactly.
            int frustumTests = 0;
        };

        /// @brief Walks @p bvh against @p cones and leaves the visible instances in `Instances()`.
        ///
        /// @param cones `EXT_WORLD`'s cones from §25.2's traversal -- up to `kMaxFrustaPerCell` of
        ///        them, because the garden can be seen through several windows at once. An
        ///        instance in ANY of them is on screen. An EMPTY span means the exterior cell was
        ///        not reached at all and nothing is drawn: it is not the identity frustum, which is
        ///        what a `ClipFrustum` with no planes would be.
        /// @param viewDistanceScale §68's setting, clamped to its band exactly as
        ///        `CullDistanceFor` clamps it.
        void Cull(const ExteriorBvh& bvh,
                  std::span<const ClipFrustum> cones,
                  const Microsoft::Xna::Framework::Vector3& eye,
                  float viewDistanceScale = 1.0F);

        /// @brief Indices into `ExteriorBvh::Instances()`, ascending and without repeats.
        [[nodiscard]] std::span<const std::uint32_t> Instances() const noexcept
        {
            return drawn_;
        }

        [[nodiscard]] const Stats& Statistics() const noexcept
        {
            return stats_;
        }

    private:
        std::vector<std::uint32_t> drawn_;
        /// @brief One byte per instance: which cone found it does not matter, only that one did.
        std::vector<std::uint8_t> found_;
        Stats stats_;

        void Visit(const ExteriorBvh& bvh,
                   const BvhNode& node,
                   const ClipFrustum& cone,
                   const Microsoft::Xna::Framework::Vector3& eye,
                   float scale,
                   bool inside);
    };

    /// @brief §25.7's indoor to outdoor crossing, gathered (`HOUSE-00679`).
    ///
    /// *"Crossing an exterior door is just another portal traversal, so there is no special
    /// case"* -- and there is none here either: §25.2's walk reaches the yards through their
    /// windows and doors and reduces a cone for each, exactly as it does for a room. What this
    /// does is the one thing the walk cannot: collect those cones into the list §25.6's hierarchy
    /// is culled against, because the outdoors is many cells in this layout and one hierarchy.
    ///
    /// **Deduplicated by §25.2's own rule.** A cone whose screen rectangle is inside one already
    /// collected adds nothing but a second walk of the whole tree, and the walk has already used
    /// that approximation to decide which cells to visit at all.
    ///
    /// **The cap degrades to the camera, never to nothing.** Eighteen exterior cells with four
    /// cones each is seventy-two walks of the hierarchy, which is not a frame budget. Past
    /// `kMaxCones` the collection throws its cones away and stands the CAMERA's frustum in their
    /// place: that is a superset of every cone the walk could have produced, so the frame
    /// over-draws the garden and never over-culls it -- the only direction §25 tolerates.
    class ExteriorCones
    {
    public:
        /// @brief Eight, which is two rooms' worth of openings onto the garden.
        static constexpr std::size_t kMaxCones = 8;

        /// @brief Gathers the cones of every visible EXTERIOR cell.
        ///
        /// @param camera the unreduced camera frustum, used only when the cap overflows.
        void Collect(const world::WorldData& world,
                     std::span<const VisibleCell> visible,
                     const ClipFrustum& camera);

        [[nodiscard]] std::span<const ClipFrustum> Cones() const noexcept
        {
            return cones_;
        }

        /// @brief Exterior cells the walk reached. Zero means the outdoors is not in view at all.
        [[nodiscard]] int CellsOutside() const noexcept
        {
            return cellsOutside_;
        }

        /// @brief Cones the containment rule threw away as already covered.
        [[nodiscard]] int ConesMerged() const noexcept
        {
            return conesMerged_;
        }

        /// @brief Whether the cap overflowed and the camera's own frustum is standing in.
        [[nodiscard]] bool Degraded() const noexcept
        {
            return degraded_;
        }

    private:
        std::vector<ClipFrustum> cones_;
        std::vector<NdcRect> rects_;
        int cellsOutside_ = 0;
        int conesMerged_ = 0;
        bool degraded_ = false;
    };

    /// @brief Metres from @p point to the nearest point of @p box; zero when it is inside.
    [[nodiscard]] float DistanceToBox(const Microsoft::Xna::Framework::BoundingBox& box,
                                      const Microsoft::Xna::Framework::Vector3& point) noexcept;

} // namespace cnahouse::visibility
