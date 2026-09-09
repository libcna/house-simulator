// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Plane.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/visibility/ClipFrustum.hpp"
#include "cnahouse/visibility/PortalArea.hpp"
#include "cnahouse/visibility/PortalDepth.hpp"
#include "cnahouse/visibility/PortalRuntime.hpp"

namespace cnahouse::world
{
    class WorldData;
}

namespace cnahouse::visibility
{

    /// @brief §25.2's cap: *"append frustum to cell.frusta (cap kMaxFrustaPerCell = 4)"*.
    ///
    /// A room seen through four doorways at once is tested against four cones; a fifth would cost
    /// another test of every chunk in it for a view the other four have nearly covered. The cap
    /// is on the TESTS, not on visibility -- the cell stays visible and simply keeps the first
    /// four cones, which is conservative in the direction §25.4 requires only because those four
    /// are unioned rather than intersected.
    inline constexpr std::size_t kMaxFrustaPerCell = 4;

    /// @brief What a cone picked up on its way in (`HOUSE-00669`).
    enum class ConeFlags : std::uint8_t
    {
        None = 0,
        /// @brief §15.4's `translucent`: the view reached this cell through frosted glass.
        ///
        /// *"Passes, but the reduced frustum is marked diffuse so the target cell renders at
        /// LOD+1 and no small props."* It is sticky down the chain -- a room seen through a room
        /// seen through a frosted door is still being seen through frosted glass.
        Diffuse = 1,
    };

    [[nodiscard]] constexpr ConeFlags operator|(ConeFlags a, ConeFlags b) noexcept
    {
        return static_cast<ConeFlags>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
    }

    [[nodiscard]] constexpr bool Has(ConeFlags flags, ConeFlags one) noexcept
    {
        return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(one)) != 0;
    }

    /// @brief §71.2's hard stop on the visible set, and R-08's answer to a traversal that runs
    ///        away (`HOUSE-00671`).
    ///
    /// §71.2 budgets 9 visible cells typically, 22 in the worst case and calls 30 a **hard fail**.
    /// This is that number, enforced rather than hoped for: past it the walk keeps the thirty cells
    /// with the largest share of the screen and drops the rest, which is the one place in §25 where
    /// something the player CAN see is culled. It is counted for exactly that reason -- a frame
    /// that had to degrade is a frame worth knowing about, and R-08 asks for graceful degradation
    /// rather than a frame that misses its budget.
    ///
    /// Measured (`HOUSE-00668`): with EVERY door in this house open, the worst of 40 poses is 12.
    inline constexpr std::size_t kMaxVisibleCells = 30;

    /// @brief One cell the walk decided the camera can see, and through which cones.
    struct VisibleCell
    {
        util::Id cell;
        std::array<ClipFrustum, kMaxFrustaPerCell> frusta{};
        /// @brief The screen rectangle each frustum arrived with, for §25.2's containment skip.
        std::array<NdcRect, kMaxFrustaPerCell> rects{};
        std::size_t frustumCount = 0;
        /// @brief The SHALLOWEST depth this cell was reached at, which is what §25.2's depth caps
        ///        are measured against and what `F3` shows.
        int depth = 0;
        /// @brief Frusta the cap refused. Non-zero means this room is seen through more than four
        ///        openings at once, which §71's `F3` is the place to notice.
        int frustaDropped = 0;
        /// @brief The flags of every cone that reached this cell, ANDed together.
        ///
        /// **And, not or.** A cell is drawn once, so a room reached through both a frosted door
        /// and a clear one is not being seen through frosted glass: one clear view of it is enough
        /// to need its dressing props. `Diffuse` therefore survives only when EVERY way in was
        /// diffuse -- which is also the safe direction, because dropping detail from a room the
        /// player can see clearly is a visible loss and keeping it costs a few props.
        ConeFlags flags = ConeFlags::None;
    };

    /// @brief What the walk did, for §71's `F3` overlay and for the tests.
    ///
    /// Every counter here is a reason a portal was NOT crossed, and together they are the whole
    /// of §25.2's pseudocode: if the numbers do not add up, a branch was taken that is not in the
    /// design.
    struct TraversalStats
    {
        int cellsVisited = 0;
        int portalsTested = 0;
        int portalsCrossed = 0;
        int skippedClosed = 0;
        int skippedFacing = 0;
        int skippedDepth = 0;
        int skippedClipped = 0;
        int skippedArea = 0;
        int skippedContained = 0;
        int frustaDropped = 0;
        int maxDepth = 0;
        /// @brief Cells every cone into which came through §15.4's frosted glass.
        int diffuseCells = 0;
        /// @brief Cells §71.2's hard stop threw away. **Non-zero means something visible was
        ///        culled**, which is a frame that missed its budget rather than a frame that was
        ///        drawn wrong -- but it is still the loudest counter in this struct.
        int cellsDropped = 0;
    };

    /// @brief §25.2's portal walk: breadth-first from the camera's cell, one reduced frustum per
    ///        opening (`HOUSE-00668`).
    ///
    /// **Breadth-first and not depth-first, because the depth cap is a budget.** A depth-first
    /// walk spends its whole allowance down the first corridor it finds and then meets the same
    /// rooms again at a shallower depth from another direction, re-expanding them; breadth-first
    /// reaches every room at the shallowest depth it can be reached at, which is also the depth
    /// `maxDepthFor` is written against.
    ///
    /// **The object is reused between frames.** The queue and the visible list keep their
    /// capacity, so a steady state costs no allocation at all -- §71.2 gives visibility 0.55 ms
    /// typical and `HOUSE-00695` is where the allocation would otherwise be found.
    class PortalTraversal
    {
    public:
        /// @brief Everything the walk reads. Assembled by `VisibilitySystem` (`HOUSE-00670`).
        struct Input
        {
            const world::WorldData* world = nullptr;
            /// @brief One per portal, in `WorldData::Portals()` order (`HOUSE-00665`).
            std::span<const PortalRuntime> portals;
            util::Id cameraCell;
            Microsoft::Xna::Framework::Vector3 eye;
            /// @brief `View() * Projection()`, for the screen-area cutoff and the NDC rectangles.
            Microsoft::Xna::Framework::Matrix viewProjection;
            /// @brief The camera's own frustum, and the two planes every reduction keeps.
            ClipFrustum cameraFrustum;
            Microsoft::Xna::Framework::Plane nearPlane;
            Microsoft::Xna::Framework::Plane farPlane;
            /// @brief §25.2's depth table asks where the camera is standing.
            CameraSide side = CameraSide::Interior;
        };

        void Run(const Input& input);

        [[nodiscard]] std::span<const VisibleCell> Visible() const noexcept
        {
            return visible_;
        }

        [[nodiscard]] const TraversalStats& Stats() const noexcept
        {
            return stats_;
        }

        [[nodiscard]] const VisibleCell* Find(util::Id cell) const noexcept;

        [[nodiscard]] bool IsVisible(util::Id cell) const noexcept
        {
            return Find(cell) != nullptr;
        }

    private:
        struct Work
        {
            util::Id cell;
            ClipFrustum frustum;
            NdcRect rect;
            int depth = 0;
            ConeFlags flags = ConeFlags::None;
        };

        VisibleCell& Reach(util::Id cell, int depth, ConeFlags flags);

        /// @brief §71.2's hard stop, applied after the walk: keep the biggest, count the rest.
        void Degrade();

        std::vector<VisibleCell> visible_;
        std::vector<Work> queue_;
        std::vector<Microsoft::Xna::Framework::Plane> planes_;
        TraversalStats stats_;
    };

} // namespace cnahouse::visibility
