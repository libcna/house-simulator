// SPDX-License-Identifier: MIT
#include "cnahouse/debug/VisibilityGeometryOverlay.hpp"

#include <algorithm>
#include <format>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"

#include "cnahouse/debug/DebugDraw.hpp"
#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::debug
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        /// A portal light passes, and one it does not. §25.3's latch is the difference, and it is
        /// the answer to "why is that room dark" -- so the two are drawn, and drawn differently.
        const Xna::Color kOpenPortal(80, 200, 255, 140);
        const Xna::Color kClosedPortal(200, 60, 60, 140);
        /// The cones: white, because they are the thing being reasoned about and everything else
        /// on screen is context for them.
        const Xna::Color kCone(255, 255, 255, 200);
    } // namespace

    Xna::Color VisibilityGeometryOverlay::ColourForDepth(int depth)
    {
        // §25.2's deepest interior chain is six; past it the ramp holds rather than wrapping,
        // because a colour that came back round to green would say "next to the camera" about the
        // furthest room in the house.
        const int step = std::clamp(depth, 0, 6);
        // `Color(int,int,int,int)` and not the byte constructor: ADR-0001's strict gate resolves
        // four `std::uint8_t`s to a CNAEXT overload that names no forbidden identifier and so is
        // invisible to the identifier lint. The ints are XNA's own.
        return Xna::Color(40 + step * 35, 255 - step * 30, 60, 255);
    }

    void VisibilityGeometryOverlay::Add(const Xna::Vector3& from, const Xna::Vector3& to, Xna::Color colour)
    {
        if (segments_.size() >= kMaxSegments)
        {
            ++dropped_;
            return;
        }
        segments_.push_back(Segment{from, to, colour});
    }

    void VisibilityGeometryOverlay::AddPolygon(std::span<const Xna::Vector3> points, Xna::Color colour)
    {
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            Add(points[i], points[(i + 1) % points.size()], colour);
        }
    }

    void VisibilityGeometryOverlay::Build(const world::WorldData& world,
                                          std::span<const visibility::VisibleCell> visible,
                                          std::span<const visibility::PortalRuntime> portals,
                                          const Xna::Vector3& eye)
    {
        segments_.clear();
        quads_.clear();
        dropped_ = 0;
        cells_ = 0;
        openPortals_ = 0;
        closedPortals_ = 0;
        cones_ = 0;

        for (const visibility::VisibleCell& entry : visible)
        {
            const world::Cell* cell = world.FindCell(entry.cell);
            if (cell == nullptr)
            {
                continue;
            }
            ++cells_;
            const Xna::Color colour = ColourForDepth(entry.depth);

            // §12's cells are one or more footprints between the level's floor and ceiling, so
            // that is what a wireframe of one is -- a box per footprint, not a box per cell: an
            // L-shaped room drawn as its bounding box would claim to contain the corner it does
            // not have.
            const util::Result<world::Extent> extent = world.ExtentOf(*cell);
            const float floorY = extent ? extent->floorY : 0.0F;
            const float ceilingY = extent ? extent->ceilingY : floorY + 2.4F;
            for (const world::Footprint& box : cell->boxes)
            {
                const Xna::BoundingBox bounds(Xna::Vector3(box.minX, floorY, box.minZ),
                                              Xna::Vector3(box.maxX, ceilingY, box.maxZ));
                const Xna::Vector3& lo = bounds.Min;
                const Xna::Vector3& hi = bounds.Max;
                const Xna::Vector3 corners[8] = {Xna::Vector3(lo.X, lo.Y, lo.Z),
                                                 Xna::Vector3(hi.X, lo.Y, lo.Z),
                                                 Xna::Vector3(hi.X, lo.Y, hi.Z),
                                                 Xna::Vector3(lo.X, lo.Y, hi.Z),
                                                 Xna::Vector3(lo.X, hi.Y, lo.Z),
                                                 Xna::Vector3(hi.X, hi.Y, lo.Z),
                                                 Xna::Vector3(hi.X, hi.Y, hi.Z),
                                                 Xna::Vector3(lo.X, hi.Y, hi.Z)};
                for (int i = 0; i < 4; ++i)
                {
                    Add(corners[i], corners[(i + 1) % 4], colour);
                    Add(corners[4 + i], corners[4 + (i + 1) % 4], colour);
                    Add(corners[i], corners[4 + i], colour);
                }
            }

            // The openings of every visible cell, whether or not the walk crossed them: a portal
            // the walk refused is exactly what a reader is looking for when a room they expected
            // is not on screen.
            for (const std::uint32_t index : world.PortalsOf(entry.cell))
            {
                if (index >= portals.size())
                {
                    continue;
                }
                const visibility::PortalRuntime& runtime = portals[index];
                const auto& rect = runtime.WorldRect();
                const bool passes = runtime.PassesLight();
                passes ? ++openPortals_ : ++closedPortals_;
                if (quads_.size() < kMaxSegments)
                {
                    Quad quad;
                    quad.corners[0] = rect[0];
                    quad.corners[1] = rect[1];
                    quad.corners[2] = rect[2];
                    quad.corners[3] = rect[3];
                    quad.colour = passes ? kOpenPortal : kClosedPortal;
                    quads_.push_back(quad);
                }
                AddPolygon(std::span(rect.data(), rect.size()), passes ? kOpenPortal : kClosedPortal);
            }

            // §25.8's wire pyramids: the eye, and the polygon the cone was reduced through. The
            // camera's own cone has no aperture -- it was reduced through nothing -- so it draws
            // no pyramid, which is right: its shape is the screen.
            for (std::size_t i = 0; i < entry.frustumCount; ++i)
            {
                const std::span<const Xna::Vector3> aperture = entry.apertures[i].Points();
                if (aperture.empty())
                {
                    continue;
                }
                ++cones_;
                AddPolygon(aperture, kCone);
                for (const Xna::Vector3& point : aperture)
                {
                    Add(eye, point, kCone);
                }
            }
        }
    }

    std::string VisibilityGeometryOverlay::Line() const
    {
        return std::format("F4  {} cell(s)  {} portal(s) open, {} shut  {} cone(s)  "
                           "{} segment(s){}",
                           cells_,
                           openPortals_,
                           closedPortals_,
                           cones_,
                           segments_.size(),
                           dropped_ == 0 ? std::string() : std::format("  {} DROPPED", dropped_));
    }

    void VisibilityGeometryOverlay::Draw(DebugDraw& draw) const
    {
        if (!visible_)
        {
            return;
        }
        // The quads first, so the wireframes and the cones are drawn over the filled openings
        // rather than under them.
        for (const Quad& quad : quads_)
        {
            draw.Quad(quad.corners[0], quad.corners[1], quad.corners[2], quad.corners[3], quad.colour);
        }
        for (const Segment& segment : segments_)
        {
            draw.Line(segment.from, segment.to, segment.colour);
        }
    }

} // namespace cnahouse::debug
