// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace cnahouse::player
{

    /// @brief §10.3's playable volume: the last of §10's five containment layers.
    ///
    /// *"A final invisible boundary at the playable-volume box, 6 m beyond every believable
    /// barrier, as a safety net. Crossing it is impossible in normal play; if it is ever touched,
    /// a debug counter increments so tests can detect a gap in the real barriers."*
    ///
    /// **The counter is the point, not the wall.** The first four layers -- the property fence,
    /// the neighbours' hedges, the road's termination and the planted terrain -- are what actually
    /// keep the player in, and they are the ones that can have a gap in them. This box is where a
    /// gap becomes visible: `HOUSE-00618` walks a bot for twenty minutes and asserts the count is
    /// still zero.
    struct PlayableVolume
    {
        float minX = -40.0F;
        float maxX = 40.0F;
        float minY = -3.5F;
        float maxY = 20.0F;
        float minZ = -52.0F;
        float maxZ = 12.0F;

        [[nodiscard]] bool Contains(const Microsoft::Xna::Framework::Vector3& point) const noexcept
        {
            return point.X >= minX && point.X <= maxX && point.Y >= minY && point.Y <= maxY &&
                   point.Z >= minZ && point.Z <= maxZ;
        }
    };

    /// @brief Keeps the player inside §10.3's box and counts every time it had to.
    class BoundaryGuard
    {
    public:
        explicit BoundaryGuard(PlayableVolume volume = {}) noexcept
            : volume_(volume)
        {
        }

        /// @brief Clamps @p position back into the volume. Returns true if it was outside.
        ///
        /// It CLAMPS as well as counting, because §10 calls this a safety net and a net that only
        /// takes attendance is not one: a player who has found a gap keeps going, and the further
        /// they get the less recoverable the state is.
        bool Contain(Microsoft::Xna::Framework::Vector3& position) noexcept;

        /// @brief How many times the player has CROSSED the boundary, not how many ticks they
        ///        have spent past it.
        ///
        /// An edge and not a level, for the same reason `CellEntered` is: a body held against the
        /// boundary for a second would otherwise count 120 escapes, and `HOUSE-00618`'s assertion
        /// is that the number is zero -- which only means something if one escape is one.
        [[nodiscard]] std::uint64_t Escapes() const noexcept
        {
            return escapes_;
        }

        /// @brief Whether the player was outside at the last `Contain`.
        [[nodiscard]] bool Outside() const noexcept
        {
            return outside_;
        }

        [[nodiscard]] const PlayableVolume& Volume() const noexcept
        {
            return volume_;
        }

        void Reset() noexcept
        {
            escapes_ = 0;
            outside_ = false;
        }

    private:
        PlayableVolume volume_;
        std::uint64_t escapes_ = 0;
        bool outside_ = false;
    };

} // namespace cnahouse::player
