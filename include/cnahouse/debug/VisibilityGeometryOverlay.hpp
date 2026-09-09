// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/visibility/PortalRuntime.hpp"
#include "cnahouse/visibility/PortalTraversal.hpp"

namespace cnahouse::world
{
    class WorldData;
}

namespace cnahouse::debug
{

    class DebugDraw;

    /// @brief §25.8's `F4`: *"the visible cell set as coloured wireframe boxes, the active portals
    ///        as filled quads, and the reduced frusta as wire pyramids"* (`HOUSE-00682`).
    ///
    /// **A visibility bug is invisible by construction.** The symptom is geometry that is NOT
    /// there, so the only way to see one is to draw the decision: which rooms the walk reached,
    /// which openings it crossed to reach them, and the shape of the cone it carried through each.
    /// `F3` gives the counts; this gives the geometry those counts are about, and the two are read
    /// together.
    ///
    /// **It builds into segments and quads of its own rather than drawing straight through
    /// `DebugDraw`.** Same reason as §71's `F9`: what it would draw is then assertable in a unit
    /// test with no device, and the cap is enforced where it can be counted and reported instead
    /// of silently swallowed by the vertex buffer.
    class VisibilityGeometryOverlay
    {
    public:
        /// @brief The most segments one build may produce.
        ///
        /// `DebugDraw` holds 65 536 line vertices -- 32 768 segments -- and `F9` reserves a third
        /// of them. This takes another third, so both can be up at once and neither can starve the
        /// other; what it cannot fit it counts and says on screen.
        static constexpr std::size_t kMaxSegments = 10000;

        struct Segment
        {
            Microsoft::Xna::Framework::Vector3 from;
            Microsoft::Xna::Framework::Vector3 to;
            Microsoft::Xna::Framework::Color colour;
        };

        struct Quad
        {
            Microsoft::Xna::Framework::Vector3 corners[4];
            Microsoft::Xna::Framework::Color colour;
        };

        /// @brief The colour a cell reached at @p depth is drawn in.
        ///
        /// A ramp and not a palette: the number a reader wants off this overlay is *how far away
        /// through the graph* a room is, and a ramp answers it without a key. Green at the camera,
        /// through yellow, to red at §25.2's deepest chain.
        [[nodiscard]] static Microsoft::Xna::Framework::Color ColourForDepth(int depth);

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

        /// @brief Rebuilds the frame's geometry from the walk's own answer.
        ///
        /// @param portals the runtimes, indexed as `world.Portals()` is, for the world rectangles
        ///        and the latch state -- an opening that light does not pass is drawn too, in its
        ///        own colour, because *why* a room is dark is the question this overlay answers.
        /// @param eye where the cones' pyramids have their apex.
        void Build(const world::WorldData& world,
                   std::span<const visibility::VisibleCell> visible,
                   std::span<const visibility::PortalRuntime> portals,
                   const Microsoft::Xna::Framework::Vector3& eye);

        [[nodiscard]] std::span<const Segment> Segments() const noexcept
        {
            return segments_;
        }

        [[nodiscard]] std::span<const Quad> Quads() const noexcept
        {
            return quads_;
        }

        /// @brief Segments the last build wanted and could not have.
        [[nodiscard]] std::size_t Dropped() const noexcept
        {
            return dropped_;
        }

        /// @brief The one-line summary drawn beside the geometry.
        [[nodiscard]] std::string Line() const;

        /// @brief Queues the last build. Nothing happens when it is not visible.
        void Draw(DebugDraw& draw) const;

    private:
        void Add(const Microsoft::Xna::Framework::Vector3& from,
                 const Microsoft::Xna::Framework::Vector3& to,
                 Microsoft::Xna::Framework::Color colour);
        void AddPolygon(std::span<const Microsoft::Xna::Framework::Vector3> points,
                        Microsoft::Xna::Framework::Color colour);

        bool visible_ = false;
        std::vector<Segment> segments_;
        std::vector<Quad> quads_;
        std::size_t dropped_ = 0;
        int cells_ = 0;
        int openPortals_ = 0;
        int closedPortals_ = 0;
        int cones_ = 0;
    };

} // namespace cnahouse::debug
