// SPDX-License-Identifier: MIT
#include "cnahouse/debug/PhysicsOverlay.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <unordered_set>

#include "cnahouse/debug/DebugDraw.hpp"
#include "cnahouse/physics/BroadPhase.hpp"

namespace cnahouse::debug
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        constexpr float kTwoPi = 2.0F * std::numbers::pi_v<float>;

        float Length(const Xna::Vector3& v)
        {
            return std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
        }

        /// The angle a normal makes with straight up, in degrees. What §43.1's slope limit is about.
        float SlopeDegrees(const Xna::Vector3& normal)
        {
            const float clamped = std::clamp(normal.Y, -1.0F, 1.0F);
            return std::acos(clamped) * 180.0F / std::numbers::pi_v<float>;
        }

        std::string Point(const Xna::Vector3& v)
        {
            return std::format("({:.2f}, {:.2f}, {:.2f})", v.X, v.Y, v.Z);
        }

        std::string KindName(physics::CollisionKind kind)
        {
            switch (kind)
            {
                case physics::CollisionKind::Floor:
                    return "floor";
                case physics::CollisionKind::Ceiling:
                    return "ceiling";
                case physics::CollisionKind::Wall:
                    return "wall";
                case physics::CollisionKind::Stair:
                    return "stair";
                case physics::CollisionKind::Prop:
                    return "prop";
                case physics::CollisionKind::Exterior:
                    return "exterior";
            }
            return "?";
        }
    } // namespace

    Xna::Color PhysicsOverlay::ColourOf(physics::CollisionKind kind)
    {
        // The three a body treats differently -- walk on it, walk into it, climb it -- are the
        // three that must be distinguishable at a glance, so they get the unmistakable colours.
        switch (kind)
        {
            case physics::CollisionKind::Floor:
                return Xna::Color::Lime;
            case physics::CollisionKind::Wall:
                return Xna::Color::White;
            case physics::CollisionKind::Stair:
                return Xna::Color::Cyan;
            case physics::CollisionKind::Ceiling:
                return Xna::Color::SkyBlue;
            case physics::CollisionKind::Prop:
                return Xna::Color::Violet;
            case physics::CollisionKind::Exterior:
                return Xna::Color::Gray;
        }
        return Xna::Color::Gray;
    }

    void PhysicsOverlay::Add(const Xna::Vector3& from, const Xna::Vector3& to, Xna::Color colour)
    {
        if (segments_.size() >= kMaxSegments)
        {
            ++dropped_;
            return;
        }
        segments_.push_back(Segment{from, to, colour});
    }

    void PhysicsOverlay::AddObb(const physics::CollisionObb& obb, Xna::Color colour)
    {
        // In the box's own frame first, then yawed into the world: §49.2 allows a yaw about +Y and
        // nothing else, so this is a 2x2 rotation and not a matrix.
        const float c = std::cos(obb.yaw);
        const float s = std::sin(obb.yaw);
        const auto corner = [&](int ix, int iy, int iz)
        {
            const float lx = (ix == 0 ? -obb.halfExtents.X : obb.halfExtents.X);
            const float ly = (iy == 0 ? -obb.halfExtents.Y : obb.halfExtents.Y);
            const float lz = (iz == 0 ? -obb.halfExtents.Z : obb.halfExtents.Z);
            return Xna::Vector3(
                obb.centre.X + lx * c + lz * s, obb.centre.Y + ly, obb.centre.Z - lx * s + lz * c);
        };

        for (int iy = 0; iy < 2; ++iy)
        {
            Add(corner(0, iy, 0), corner(1, iy, 0), colour);
            Add(corner(1, iy, 0), corner(1, iy, 1), colour);
            Add(corner(1, iy, 1), corner(0, iy, 1), colour);
            Add(corner(0, iy, 1), corner(0, iy, 0), colour);
        }
        for (int ix = 0; ix < 2; ++ix)
        {
            for (int iz = 0; iz < 2; ++iz)
            {
                Add(corner(ix, 0, iz), corner(ix, 1, iz), colour);
            }
        }
    }

    void PhysicsOverlay::AddMesh(const physics::CollisionMesh& mesh, Xna::Color colour)
    {
        // Every edge of every triangle, shared edges twice. De-duplicating them would need a map
        // per mesh every frame to save a third of the lines of the two meshes in this house.
        for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
        {
            const Xna::Vector3& a = mesh.vertices[mesh.indices[t]];
            const Xna::Vector3& b = mesh.vertices[mesh.indices[t + 1]];
            const Xna::Vector3& c = mesh.vertices[mesh.indices[t + 2]];
            Add(a, b, colour);
            Add(b, c, colour);
            Add(c, a, colour);
        }
    }

    void PhysicsOverlay::AddCapsule(const physics::Capsule& capsule, Xna::Color colour)
    {
        const float bottom = capsule.centre.Y - capsule.halfHeight;
        const float top = capsule.centre.Y + capsule.halfHeight;
        const auto ring = [&](float y)
        {
            for (int i = 0; i < kCircleSegments; ++i)
            {
                const float a0 = kTwoPi * static_cast<float>(i) / static_cast<float>(kCircleSegments);
                const float a1 = kTwoPi * static_cast<float>(i + 1) / static_cast<float>(kCircleSegments);
                Add(Xna::Vector3(capsule.centre.X + capsule.radius * std::cos(a0),
                                 y,
                                 capsule.centre.Z + capsule.radius * std::sin(a0)),
                    Xna::Vector3(capsule.centre.X + capsule.radius * std::cos(a1),
                                 y,
                                 capsule.centre.Z + capsule.radius * std::sin(a1)),
                    colour);
            }
        };
        ring(bottom);
        ring(top);

        // The caps, as half-circles in the two vertical planes. Without them the shape reads as a
        // cylinder, and the rounded ends are exactly what a sweep's corner behaviour depends on.
        const auto arc = [&](bool alongX, float capY, float sign)
        {
            const int steps = kCircleSegments / 2;
            for (int i = 0; i < steps; ++i)
            {
                const float a0 =
                    std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(steps);
                const float a1 =
                    std::numbers::pi_v<float> * static_cast<float>(i + 1) / static_cast<float>(steps);
                const auto at = [&](float a)
                {
                    const float h = capsule.radius * std::cos(a);
                    const float v = capY + sign * capsule.radius * std::sin(a);
                    return alongX ? Xna::Vector3(capsule.centre.X + h, v, capsule.centre.Z)
                                  : Xna::Vector3(capsule.centre.X, v, capsule.centre.Z + h);
                };
                Add(at(a0), at(a1), colour);
            }
        };
        arc(true, top, 1.0F);
        arc(false, top, 1.0F);
        arc(true, bottom, -1.0F);
        arc(false, bottom, -1.0F);

        // The four sides.
        Add(Xna::Vector3(capsule.centre.X + capsule.radius, bottom, capsule.centre.Z),
            Xna::Vector3(capsule.centre.X + capsule.radius, top, capsule.centre.Z),
            colour);
        Add(Xna::Vector3(capsule.centre.X - capsule.radius, bottom, capsule.centre.Z),
            Xna::Vector3(capsule.centre.X - capsule.radius, top, capsule.centre.Z),
            colour);
        Add(Xna::Vector3(capsule.centre.X, bottom, capsule.centre.Z + capsule.radius),
            Xna::Vector3(capsule.centre.X, top, capsule.centre.Z + capsule.radius),
            colour);
        Add(Xna::Vector3(capsule.centre.X, bottom, capsule.centre.Z - capsule.radius),
            Xna::Vector3(capsule.centre.X, top, capsule.centre.Z - capsule.radius),
            colour);
    }

    void PhysicsOverlay::AddCross(const Xna::Vector3& at, float size, Xna::Color colour)
    {
        Add(Xna::Vector3(at.X - size, at.Y, at.Z), Xna::Vector3(at.X + size, at.Y, at.Z), colour);
        Add(Xna::Vector3(at.X, at.Y, at.Z - size), Xna::Vector3(at.X, at.Y, at.Z + size), colour);
    }

    void PhysicsOverlay::Build(const physics::CollisionWorld& world,
                               std::span<const physics::CollisionCell* const> cells,
                               physics::BroadPhase& broad,
                               const PhysicsOverlayBody& body)
    {
        segments_.clear();
        dropped_ = 0;
        built_ = true;
        body_ = body;
        cellCount_ = 0;
        obbCount_ = 0;
        meshCount_ = 0;
        triangleCount_ = 0;
        ground_ = physics::GroundProbeResult{};
        sweep_ = physics::CellSweepHit{};
        overlap_ = physics::CellOverlap{};
        sweptSomething_ = false;
        groundSurface_.clear();
        sweepSurface_.clear();
        bodyCell_ = body.cell != nullptr ? body.cell->id : std::string();

        // A wall between two rooms is in both cells' shape lists. Drawn once.
        std::unordered_set<std::uint32_t> drawn;
        for (const physics::CollisionCell* cell : cells)
        {
            if (cell == nullptr)
            {
                continue;
            }
            ++cellCount_;
            for (const std::uint32_t shape : cell->shapes)
            {
                if (!drawn.insert(shape).second)
                {
                    continue;
                }
                if (shape < world.obbs.size())
                {
                    const physics::CollisionObb& obb = world.obbs[shape];
                    ++obbCount_;
                    AddObb(obb, ColourOf(obb.kind));
                }
                else
                {
                    const std::size_t index = shape - world.obbs.size();
                    if (index >= world.meshes.size())
                    {
                        continue;
                    }
                    const physics::CollisionMesh& mesh = world.meshes[index];
                    ++meshCount_;
                    triangleCount_ += mesh.TriangleCount();
                    AddMesh(mesh, ColourOf(mesh.kind));
                }
            }
        }

        if (body.cell != nullptr)
        {
            overlap_ = physics::OverlapCell(world, *body.cell, broad, body.capsule);
            ground_ = physics::GroundProbe(world, *body.cell, broad, body.capsule);
            groundSurface_ = std::string(world.SurfaceName(ground_.surface));
        }

        // The capsule is red when it is somewhere it should not be. §49.3's step 5 pushes out of
        // exactly this, and a body that is inside a wall at the END of a frame is the bug the
        // guarantee suite exists for -- so it is the one thing on this overlay that changes colour.
        const bool penetrating = overlap_.overlapped && overlap_.depth > physics::kContactTolerance;
        AddCapsule(body.capsule, penetrating ? Xna::Color::Red : Xna::Color::Yellow);
        if (penetrating)
        {
            Add(body.capsule.centre,
                Xna::Vector3(body.capsule.centre.X + overlap_.normal.X * kNormalLength,
                             body.capsule.centre.Y + overlap_.normal.Y * kNormalLength,
                             body.capsule.centre.Z + overlap_.normal.Z * kNormalLength),
                Xna::Color::Red);
        }

        // The ground probe: §49.3 step 3's short downward sweep, drawn as what it is.
        if (body.cell != nullptr)
        {
            const Xna::Vector3 feet(body.capsule.centre.X, body.capsule.Bottom(), body.capsule.centre.Z);
            const Xna::Color probeColour = ground_.onGround ? Xna::Color::Lime
                                           : ground_.steep  ? Xna::Color::Orange
                                                            : Xna::Color::Gray;
            Add(feet, Xna::Vector3(feet.X, feet.Y - physics::kGroundProbeReach, feet.Z), probeColour);
            if (ground_.onGround || ground_.steep)
            {
                const Xna::Vector3 contact(feet.X, ground_.height, feet.Z);
                AddCross(contact, body.capsule.radius, probeColour);
                Add(contact,
                    Xna::Vector3(contact.X + ground_.normal.X * kNormalLength,
                                 contact.Y + ground_.normal.Y * kNormalLength,
                                 contact.Z + ground_.normal.Z * kNormalLength),
                    probeColour);
            }
        }

        // The sweep the NEXT step would run, from the velocity the body has now.
        const Xna::Vector3 motion(
            body.velocity.X * body.dt, body.velocity.Y * body.dt, body.velocity.Z * body.dt);
        if (body.cell != nullptr && Length(motion) > physics::kContactTolerance)
        {
            sweptSomething_ = true;
            sweep_ = physics::SweepCell(world, *body.cell, broad, body.capsule, motion);
            const Xna::Vector3 stop(body.capsule.centre.X + motion.X * sweep_.time,
                                    body.capsule.centre.Y + motion.Y * sweep_.time,
                                    body.capsule.centre.Z + motion.Z * sweep_.time);
            const Xna::Vector3 wanted(body.capsule.centre.X + motion.X,
                                      body.capsule.centre.Y + motion.Y,
                                      body.capsule.centre.Z + motion.Z);
            const Xna::Color hitColour = sweep_.hit ? Xna::Color::Red : Xna::Color::Lime;
            Add(body.capsule.centre, stop, hitColour);
            if (sweep_.hit)
            {
                // The part of the step that will NOT happen, in grey: the gap between the two is
                // what a slide is about to redistribute.
                Add(stop, wanted, Xna::Color::Gray);
                Add(stop,
                    Xna::Vector3(stop.X + sweep_.normal.X * kNormalLength,
                                 stop.Y + sweep_.normal.Y * kNormalLength,
                                 stop.Z + sweep_.normal.Z * kNormalLength),
                    Xna::Color::Red);
                if (sweep_.shape != physics::CellSweepHit::kNothing)
                {
                    sweepSurface_ = std::string(
                        sweep_.shape < world.obbs.size()
                            ? world.SurfaceName(world.obbs[sweep_.shape].surface)
                            : world.SurfaceName(world.meshes[sweep_.shape - world.obbs.size()].surface));
                }
            }
        }
    }

    std::vector<std::string> PhysicsOverlay::Lines() const
    {
        std::vector<std::string> lines;
        if (!built_)
        {
            return lines;
        }

        lines.push_back(std::format("physics  {} cell(s)  {} obb  {} mesh ({} tri)  {} segment(s){}",
                                    cellCount_,
                                    obbCount_,
                                    meshCount_,
                                    triangleCount_,
                                    segments_.size(),
                                    dropped_ == 0 ? std::string() : std::format("  {} DROPPED", dropped_)));

        const float speed = Length(body_.velocity);
        lines.push_back(std::format("capsule  {}  r {:.2f}  h {:.2f}  |v| {:.2f} m/s  cell {}",
                                    Point(body_.capsule.centre),
                                    body_.capsule.radius,
                                    2.0F * (body_.capsule.halfHeight + body_.capsule.radius),
                                    speed,
                                    bodyCell_.empty() ? "-" : bodyCell_));

        if (body_.cell == nullptr)
        {
            lines.push_back("ground   no cell: shapes only");
        }
        else if (ground_.onGround)
        {
            lines.push_back(std::format("ground   {} '{}'{}  y {:.3f}  gap {:.0f} mm  slope {:.1f}deg",
                                        KindName(ground_.kind),
                                        groundSurface_,
                                        ground_.terrain ? " terrain" : "",
                                        ground_.height,
                                        ground_.distance * 1000.0F,
                                        SlopeDegrees(ground_.normal)));
        }
        else if (ground_.steep)
        {
            lines.push_back(std::format("ground   STEEP {:.1f}deg  y {:.3f}  gap {:.0f} mm",
                                        SlopeDegrees(ground_.normal),
                                        ground_.height,
                                        ground_.distance * 1000.0F));
        }
        else
        {
            lines.push_back("ground   none: airborne");
        }

        if (!sweptSomething_)
        {
            lines.push_back("sweep    at rest");
        }
        else if (sweep_.hit)
        {
            lines.push_back(
                std::format("sweep    BLOCKED t {:.3f}  shape {}{}  n {}  {} tested",
                            sweep_.time,
                            sweep_.shape,
                            sweepSurface_.empty() ? std::string() : std::format(" '{}'", sweepSurface_),
                            Point(sweep_.normal),
                            sweep_.tested));
        }
        else
        {
            lines.push_back(std::format("sweep    clear  {} tested", sweep_.tested));
        }

        if (overlap_.overlapped && overlap_.depth > physics::kContactTolerance)
        {
            lines.push_back(std::format("overlap  INSIDE shape {} by {:.3f} m  out {}",
                                        overlap_.shape,
                                        overlap_.depth,
                                        Point(overlap_.normal)));
        }
        else if (overlap_.overlapped)
        {
            lines.push_back(
                std::format("overlap  touching shape {} ({:.4f} m)", overlap_.shape, overlap_.depth));
        }
        else
        {
            lines.push_back("overlap  clear");
        }
        return lines;
    }

    void PhysicsOverlay::Draw(DebugDraw& draw) const
    {
        if (!visible_)
        {
            return;
        }
        for (const Segment& segment : segments_)
        {
            draw.Line(segment.from, segment.to, segment.colour);
        }
    }

} // namespace cnahouse::debug
