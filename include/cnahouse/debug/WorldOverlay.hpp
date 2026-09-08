// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/physics/CollisionData.hpp"
#include "cnahouse/world/SpatialIndex.hpp"

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

    /// @brief What §69's `F2` is looking at, for one frame.
    ///
    /// **Names, not ids.** The overlay is read by a person, and `util::Id` is a 32-bit FNV hash --
    /// the reverse map that turns it back into `L0_FOYER` lives in `IdRegistry` and belongs to the
    /// caller, which has the world data open anyway. Passing the string also keeps this struct free
    /// of every header the world model needs.
    struct WorldSnapshot
    {
        /// @brief §16's cell, and which of §16.4's four steps found it (`HOUSE-00559`).
        std::string cell;
        world::SpatialIndex::Step cellFoundBy = world::SpatialIndex::Step::Incremental;
        /// @brief §12's level: `B1`, `L0`, `L1`, `L2`, `L3`.
        std::string level;

        /// @brief The FEET, which is what §16.4 looks a cell up by and what a teleport takes.
        Microsoft::Xna::Framework::Vector3 position;
        /// @brief Radians. §14's yaw (0 = north, positive = east) and §44's pitch.
        float yaw = 0.0F;
        float pitch = 0.0F;

        /// @brief Metres per second, horizontal: what §43.2's table produced this step.
        float speed = 0.0F;
        bool onGround = true;
        bool crouched = false;
        /// @brief §43.2's mode, which is a setting rather than a key being held (D-09).
        bool fastWalk = false;

        /// @brief §62.4's surface name under the body -- `hardwood`, `carpet`, `grass`.
        std::string surface;
        physics::CollisionKind ground = physics::CollisionKind::Floor;
        /// @brief Metres from the feet to the surface. §49.3's probe reaches 0.45 m.
        float groundGap = 0.0F;

        /// @brief §54's held item. Empty until phase 14 has something to hold.
        std::optional<std::string> heldItem;
        /// @brief §50's target interactable and its state, e.g. `D_L0_HALL` / `closed, unlocked`.
        std::optional<std::string> target;
        std::string targetState;
    };

    /// @brief §69's `F2` world overlay: cell, position, yaw/pitch, surface, held item, target
    ///        (`HOUSE-00631`).
    ///
    /// A **presenter**, like §71's `F1` and `F9`: it owns no measurement and takes no queries of
    /// its own, so what it says can be asserted in a unit test rather than looked at. Everything
    /// on it is something another system already knows and the player cannot see.
    ///
    /// **Degrees and compass points, not radians.** §14 makes yaw 0 north and positive east, and
    /// an overlay that prints 2.3562 makes its reader do trigonometry before they can tell which
    /// way they are facing. The whole value of this overlay is answering "where am I and which way
    /// am I pointing" in the time it takes to glance at it.
    class WorldOverlay
    {
    public:
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
        [[nodiscard]] std::vector<std::string> Lines(const WorldSnapshot& snapshot) const;

        /// @brief §14's compass point for a yaw in radians: `N`, `NE`, ... `NW`.
        [[nodiscard]] static std::string Compass(float yaw);

        /// @brief A yaw in radians as a bearing in degrees, 0 (north) to 360.
        [[nodiscard]] static float BearingDegrees(float yaw);

        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                  const ui::TextRenderer& text,
                  const WorldSnapshot& snapshot) const;

    private:
        bool visible_ = false;
    };

} // namespace cnahouse::debug
