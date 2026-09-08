// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "cnahouse/physics/Sweep.hpp"
#include "cnahouse/util/Ids.hpp"

namespace cnahouse::physics
{

    /// @brief What a dynamic obstacle is, for the systems that treat them differently (§49.4).
    enum class DynamicKind : std::uint8_t
    {
        /// @brief A door leaf. *"A door swinging into the player pushes them"*, so its motion is
        ///        authoritative and the player is depenetrated out of it.
        Door,
        /// @brief One of the garage door's five segments.
        GarageSegment,
        /// @brief A pet. *"The player can push them gently aside"* -- so unlike a door, contact
        ///        with one is a message to the animal and not a wall.
        Pet,
        /// @brief One of §49.4's twelve nudgeable props.
        Prop,
    };

    /// @brief A moving box in a cell, rebuilt every frame from whatever is driving it.
    struct DynamicObstacle
    {
        /// @brief World space, refreshed per frame. §49.4: *"an OBB following the leaf's animated
        ///        transform"*.
        CollisionObb shape;
        /// @brief What is driving it -- a door id, a pet id -- so a hit can be reported back to
        ///        the thing that caused it.
        util::Id source;
        DynamicKind kind = DynamicKind::Door;
    };

    /// @brief §49.4's per-cell dynamic list, and its per-frame refresh (`HOUSE-00554`).
    ///
    /// **Rebuilt, not updated in place.** A door that swings from one cell into another, a pet
    /// that walks through a doorway and a prop that is kicked across a room all change WHICH cell
    /// they belong to, and an incremental list has to be told about each of those. Clearing and
    /// refilling costs one pass over a few dozen boxes a frame and cannot go stale -- and the
    /// storage is kept between frames, so the rebuild allocates nothing after the first.
    ///
    /// It is deliberately separate from `CollisionWorld`, which is the STATIC world read from
    /// `collision.bin` and shared, immutable, for the life of the level. Mixing the two would put
    /// a per-frame write into the structure every sweep reads.
    class DynamicObstacles
    {
    public:
        /// @brief Empties every cell's list, keeping the memory. Call once per frame, before the
        ///        systems that own doors and pets add theirs back.
        void BeginFrame() noexcept;

        /// @brief Puts @p obstacle in @p cellId's list for this frame.
        void Add(std::string_view cellId, const DynamicObstacle& obstacle);

        /// @brief What is in @p cellId this frame. Empty for a cell nothing has been added to.
        [[nodiscard]] std::span<const DynamicObstacle> For(std::string_view cellId) const;

        /// @brief Obstacles across every cell. For the overlay, and for counting.
        [[nodiscard]] std::size_t Count() const noexcept
        {
            return count_;
        }

    private:
        // Keyed by cell id. The lists are kept and cleared rather than freed, which is what makes
        // the rebuild allocation-free after the first frame.
        std::unordered_map<std::string, std::vector<DynamicObstacle>> byCell_;
        std::size_t count_ = 0;
    };

    /// @brief What a sweep against the dynamic list found.
    struct DynamicSweepHit : SweepHit
    {
        /// @brief Index into the cell's list, or `kNothing`.
        std::uint32_t obstacle = kNothing;
        static constexpr std::uint32_t kNothing = 0xFFFFFFFFu;
        util::Id source;
        DynamicKind kind = DynamicKind::Door;
    };

    /// @brief Sweeps @p capsule against everything dynamic in @p cellId.
    ///
    /// A separate call from `SweepCell` rather than a flag on it: the static world has a broad
    /// phase and a dynamic list has a few dozen boxes, so one wants a grid and the other wants a
    /// loop, and a caller that only needs the walls should not pay for either.
    [[nodiscard]] DynamicSweepHit SweepDynamic(const DynamicObstacles& obstacles,
                                               std::string_view cellId,
                                               const Capsule& capsule,
                                               const Microsoft::Xna::Framework::Vector3& motion);

    /// @brief The DEEPEST overlap of @p capsule with anything dynamic in @p cellId.
    ///
    /// §49.4: *"A door swinging into the player pushes them (the door's motion is authoritative;
    /// the player is depenetrated)."* This is what that depenetration asks.
    [[nodiscard]] DynamicSweepHit
    OverlapDynamic(const DynamicObstacles& obstacles, std::string_view cellId, const Capsule& capsule);

} // namespace cnahouse::physics
