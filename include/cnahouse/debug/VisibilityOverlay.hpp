// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/visibility/PortalTraversal.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class SpriteBatch;
}

namespace cnahouse::ui
{
    class TextRenderer;
}

namespace cnahouse::debug
{

    /// @brief One row of §25.8's visible list.
    struct VisibleCellLine
    {
        /// @brief The cell's NAME. `util::Id` is a 32-bit hash and this is read by a person; the
        ///        reverse map belongs to the caller, which has `IdRegistry` open anyway.
        std::string cell;
        int depth = 0;
        int cones = 0;
        int conesDropped = 0;
        /// @brief §26.4: every way into this room was through frosted glass, so its dressing goes.
        bool diffuse = false;
    };

    /// @brief What §25.8's `F3` is looking at, for one frame (`HOUSE-00681`).
    ///
    /// Everything here is a number another system already produced. The overlay measures nothing
    /// and asks nothing, which is what makes what it says assertable in a unit test instead of
    /// something a person has to look at.
    struct VisibilitySnapshot
    {
        /// @brief §16.4's answer for the eye, and where it is.
        std::string cell;
        Microsoft::Xna::Framework::Vector3 eye;
        float yaw = 0.0F;
        /// @brief Cells in the world, which is what "visible 9 / 95" is out of.
        int cellsInWorld = 0;

        visibility::TraversalStats traversal;
        std::vector<VisibleCellLine> visible;

        /// @brief §25.1's step 3 for static geometry, or -1 where nothing ran it this frame.
        int chunksDrawn = -1;
        int chunksTested = -1;
        /// @brief The same for the things that move.
        int instancesDrawn = -1;
        int instancesTested = -1;
        /// @brief §25.6's hierarchy: instances drawn, and the node and instance tests it took.
        int exteriorDrawn = -1;
        int exteriorTested = -1;
        int exteriorNodes = -1;

        /// @brief §25.1's step 5, and §71.2's two budgets.
        int drawCalls = -1;
        int stateChanges = -1;

        /// @brief Whether the frame's draw list was built from this visible set at all.
        ///
        /// False says the numbers above describe a walk whose answer nothing acted on, which is
        /// exactly the state `HOUSE-00684`'s `cull off` puts the game in -- and the state the game
        /// is in before that command exists. An overlay that did not say so would be reporting a
        /// culling system that is not culling.
        bool cullingApplied = false;

        /// @brief §25.8's `F5`: the walk is frozen and the camera has detached from the body.
        ///
        /// While this is true every number above describes an OLD frame -- the one the freeze
        /// caught -- and the eye it was computed from is not where the picture is being drawn
        /// from. Saying so is the difference between a frozen overlay and a broken one.
        bool frozen = false;
        /// @brief The frame index the walk above was computed for.
        ///
        /// Stops advancing the moment `F5` freezes, which is the only way to SEE that the walk has
        /// stopped: the body stands still while frozen, so a walk that kept running would keep
        /// producing the same answer and look exactly like one that had stopped.
        std::uint64_t walkFrame = 0;
        /// @brief Where the detached camera is, while `frozen`. Ignored otherwise.
        Microsoft::Xna::Framework::Vector3 inspectionEye;
    };

    /// @brief §25.8's `F3`: *"cells visible ... traversals ... portals tested ... maxdepth"*, and
    ///        the visible list (`HOUSE-00681`).
    ///
    /// A presenter, like §69's `F2` and §71's `F1` and `F9`. §25.8 calls this *"the single most
    /// useful debugging tool for a portal system"* about `F5`; this is what `F5` freezes, and the
    /// reason it is worth freezing is that every counter here is a REASON a portal was not
    /// crossed. A cell that should be visible and is not is one of six numbers, and the overlay
    /// names all six rather than reporting a total.
    class VisibilityOverlay
    {
    public:
        /// @brief How many cells of the visible list are listed before it is summarised.
        ///
        /// §25's cap is thirty (`kMaxVisibleCells`) and a screen is not thirty lines tall next to
        /// everything else on it. Twelve is §71.2's worst case plus a little, so the list is whole
        /// on every frame that is inside budget and says how many it left out on the ones that are
        /// not -- which is the frame a reader most wants the summary of.
        static constexpr std::size_t kMaxListed = 12;

        void SetVisible(bool visible) noexcept
        {
            visible_ = visible;
        }

        void Toggle() noexcept
        {
            visible_ = !visible_;
        }

        [[nodiscard]] bool Visible() const noexcept
        {
            return visible_;
        }

        /// @brief The lines the overlay would draw, top to bottom.
        [[nodiscard]] std::vector<std::string> Lines(const VisibilitySnapshot& snapshot) const;

        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                  const ui::TextRenderer& text,
                  const VisibilitySnapshot& snapshot) const;

    private:
        bool visible_ = false;
    };

} // namespace cnahouse::debug
