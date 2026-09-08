// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/physics/Ground.hpp"
#include "cnahouse/physics/Sweep.hpp"

namespace cnahouse::physics
{
    class BroadPhase;
}

namespace cnahouse::debug
{
    class DebugDraw;

    /// @brief One wire segment: what the whole overlay is made of.
    struct Segment
    {
        Microsoft::Xna::Framework::Vector3 from;
        Microsoft::Xna::Framework::Vector3 to;
        Microsoft::Xna::Framework::Color colour;
    };

    /// @brief The body the overlay annotates, and the step it is about to take.
    struct PhysicsOverlayBody
    {
        physics::Capsule capsule;
        /// @brief Metres per second. The forward sweep is this times @ref dt.
        Microsoft::Xna::Framework::Vector3 velocity;
        /// @brief §49.3's fixed step. The sweep drawn is the one the NEXT step would run.
        float dt = 1.0F / 120.0F;
        /// @brief The cell the body is in. The probe, the sweep and the overlap all use it;
        ///        `nullptr` draws the shapes and the capsule and nothing else.
        const physics::CollisionCell* cell = nullptr;
    };

    /// @brief §71's `F9` physics overlay: the collision shapes in the visible cells, the player
    ///        capsule, the ground probe, and the sweep the next step would run (`HOUSE-00562`).
    ///
    /// **Why a list of segments and not a series of draw calls.** The overlay is a presenter, the
    /// same as §71's `F1`: it decides WHAT is drawn and `DebugDraw` decides how. That is what lets
    /// a unit test assert that a shape outside the visible cells is not drawn, that a blocked
    /// sweep is red and a clear one is not, and that the capsule is where the body is -- none of
    /// which is answerable by looking at a screen, and all of which is the sort of thing that
    /// quietly stops being true.
    ///
    /// **Everything is a line.** A wire box would be `DebugDraw::Box`, but a `CollisionObb` may be
    /// yawed (§49.2) and `BoundingBox` cannot hold a rotation; and a solid box reads as a wall at a
    /// glance and hides what is behind it, which is the one thing a debug overlay must not do.
    ///
    /// **The sweep it shows is one it runs itself**, from the body's current velocity, rather than
    /// a recording plumbed out of `PlayerStep`. A hook on §49.3's hot path that exists only when
    /// `CNAHOUSE_DEBUG_TOOLS` is on makes the debug build a different program from the one the
    /// perf tests measure -- `HOUSE-00147` settled that argument for the counters and it settles
    /// this one. One extra sweep a frame is 6 µs of the frame it is annotating.
    class PhysicsOverlay
    {
    public:
        /// @brief The most segments one build may produce.
        ///
        /// `DebugDraw` holds 65 536 line VERTICES -- 32 768 segments -- and drops the rest
        /// SILENTLY. A debugging tool that quietly stops showing some of the world is worse than
        /// one that says it ran out, so the overlay stops at a third of the buffer, counts what it
        /// skipped and puts the number on the screen. The remaining two thirds are for `F4`'s cell
        /// wireframes and portal quads, which are drawn from the same buffer in the same frame.
        static constexpr std::size_t kMaxSegments = 10000;

        /// @brief Segments in one wire circle. 16 is round enough at 2 m and cheap enough that a
        ///        room full of props is still inside @ref kMaxSegments.
        static constexpr int kCircleSegments = 16;

        /// @brief How long a normal is drawn, in metres. Long enough to see, short enough not to
        ///        cross the room it is in.
        static constexpr float kNormalLength = 0.30F;

        // §71 gives no colour key, so this is the one, and it is asserted rather than described:
        // the kinds a body treats differently must LOOK different or the overlay cannot be used to
        // tell them apart.
        static Microsoft::Xna::Framework::Color ColourOf(physics::CollisionKind kind);

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

        /// @brief Rebuilds the frame's geometry and its text.
        ///
        /// @p cells are the cells whose shapes are drawn -- §71 says *"in the visible cells"* --
        /// and a shape listed by two of them is drawn once: a wall between two rooms belongs to
        /// both, and drawing it twice doubles the cost of every corridor.
        void Build(const physics::CollisionWorld& world,
                   std::span<const physics::CollisionCell* const> cells,
                   physics::BroadPhase& broad,
                   const PhysicsOverlayBody& body);

        [[nodiscard]] std::span<const Segment> Segments() const noexcept
        {
            return segments_;
        }

        /// @brief Segments the last build wanted and could not have. Zero unless the world is
        ///        denser than @ref kMaxSegments, and reported on screen when it is not.
        [[nodiscard]] std::size_t Dropped() const noexcept
        {
            return dropped_;
        }

        /// @brief The text half of the overlay, top to bottom. Empty before the first build.
        [[nodiscard]] std::vector<std::string> Lines() const;

        /// @brief What the last build found, for a test and for @ref Lines.
        [[nodiscard]] const physics::GroundProbeResult& Ground() const noexcept
        {
            return ground_;
        }

        [[nodiscard]] const physics::CellSweepHit& Sweep() const noexcept
        {
            return sweep_;
        }

        [[nodiscard]] const physics::CellOverlap& Overlap() const noexcept
        {
            return overlap_;
        }

        /// @brief Queues the last build's segments. Nothing happens when it is not visible.
        void Draw(DebugDraw& draw) const;

    private:
        void Add(const Microsoft::Xna::Framework::Vector3& from,
                 const Microsoft::Xna::Framework::Vector3& to,
                 Microsoft::Xna::Framework::Color colour);
        void AddObb(const physics::CollisionObb& obb, Microsoft::Xna::Framework::Color colour);
        void AddMesh(const physics::CollisionMesh& mesh, Microsoft::Xna::Framework::Color colour);
        void AddCapsule(const physics::Capsule& capsule, Microsoft::Xna::Framework::Color colour);
        void AddCross(const Microsoft::Xna::Framework::Vector3& at,
                      float size,
                      Microsoft::Xna::Framework::Color colour);

        bool visible_ = false;
        std::vector<Segment> segments_;
        std::size_t dropped_ = 0;

        // What the last build was about, kept for `Lines` and for the tests.
        bool built_ = false;
        PhysicsOverlayBody body_;
        /// Copied rather than kept as a `string_view` into the world: the overlay outlives a
        /// frame and the world it was built from can be unloaded between one and the next.
        std::string groundSurface_;
        std::string sweepSurface_;
        std::string bodyCell_;
        std::size_t cellCount_ = 0;
        std::size_t obbCount_ = 0;
        std::size_t meshCount_ = 0;
        std::size_t triangleCount_ = 0;
        physics::GroundProbeResult ground_;
        physics::CellSweepHit sweep_;
        physics::CellOverlap overlap_;
        bool sweptSomething_ = false;
    };

} // namespace cnahouse::debug
