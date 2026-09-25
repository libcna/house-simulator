// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "Microsoft/Xna/Framework/Vector2.hpp"

namespace cnahouse::player
{

    enum class PointerKind : std::uint8_t
    {
        None,
        Mouse,
        Touch,
    };

    /// @brief One frame of player intent, in game terms rather than device terms.
    ///
    /// Nothing here mentions a key, a button or a pixel. `move` is a direction the player wants to go,
    /// not W-A-S-D; `look` is an angular delta in radians, not mouse counts. That translation is the
    /// whole job of an `IInputSource`, and it is why gamepad support, remapping and the phase-50 touch
    /// UI are all changes to *one* class rather than to every system that reads input.
    struct InputState
    {
        /// @brief Desired movement in the camera's own plane, each component in [-1, 1].
        Microsoft::Xna::Framework::Vector2 move{};
        /// @brief Desired look change this frame, in RADIANS. X is yaw, Y is pitch.
        ///
        /// Radians, not mouse counts, because sensitivity and inversion are settings and must be
        /// applied once, here, rather than by every consumer.
        Microsoft::Xna::Framework::Vector2 look{};

        /// @brief Whether the walk-mode key is DOWN. Kept beside the edge because a source may
        ///        one day be a gamepad trigger, and because a replay records both.
        bool run = false;
        /// @brief The walk-mode key went down this frame. An edge, not a level.
        ///
        /// §43.2: *"Shift TOGGLES between normal and fast walk. It is not hold-to-sprint."* A
        /// consumer given only the level would have to remember last frame's, and this file
        /// already says why edges are the source's job and not the consumer's.
        bool runPressed = false;
        bool crouch = false;
        bool jump = false;

        /// @brief The interact button went down this frame. An edge, not a level.
        ///
        /// Edges are computed by the source, not by consumers: two systems each tracking "was it down
        /// last frame" is two chances to disagree about what frame it is.
        bool interactPressed = false;
        bool cancelPressed = false;
        bool menuPressed = false;

        /// @brief Device-independent one-frame UI navigation edges.
        ///
        /// The settings screen consumes these rather than polling keys, keeping the project's one
        /// input boundary intact and giving keyboard, pointer and touch the same route.
        bool uiUpPressed = false;
        bool uiDownPressed = false;
        bool uiLeftPressed = false;
        bool uiRightPressed = false;
        bool uiAcceptPressed = false;

        /// @brief A primary pointer press in normalised back-buffer coordinates.
        ///
        /// Mouse and touch share hit testing after the source records which device produced it.
        /// `pointerPressed` is an edge; dragging or holding cannot change a setting every frame.
        PointerKind pointerKind = PointerKind::None;
        float pointerX = 0.0F;
        float pointerY = 0.0F;
        bool pointerPressed = false;

        /// @brief ANY key or mouse button went down this frame. The user-gesture signal.
        ///
        /// Separate from every other edge on purpose (`HOUSE-00155`): a browser will not open an
        /// audio device until the user has interacted with the page, and "interacted" means any
        /// input at all -- not `E`, not `Escape`, not a bound action. A gate wired to a specific key
        /// would be a gate the player can fail to find.
        bool anyPressed = false;

        /// @brief §68's `Alt`, HELD: give the cursor back without opening a menu.
        ///
        /// A level and not an edge, because it is a "while you hold this" control and the policy
        /// that reads it (`HOUSE-00624`) has to know the state on every frame, not the moment it
        /// changed.
        bool freeCursorHeld = false;

        /// @brief Request a switch between windowed and fullscreen display modes.
        bool toggleFullscreenPressed = false;

        /// @brief Debug toggles, compiled out of a build without debug tools.
        bool toggleOverlayPressed = false;
        /// @brief §69's `F2`: the world overlay -- cell, position, yaw/pitch, surface, target.
        bool toggleWorldOverlayPressed = false;
        /// @brief §25.8's `F3`: the visibility overlay -- the visible set and why it is that set.
        bool toggleVisibilityOverlayPressed = false;
        /// @brief §25.8's `F4`: the visibility GEOMETRY -- cell boxes, portal quads, the cones.
        bool toggleVisibilityGeometryPressed = false;
        /// @brief §25.8's `F5`: freeze the visibility walk and detach the camera to inspect it.
        bool toggleFreezeVisibilityPressed = false;
        /// @brief §71's `F8`: the environment overlay -- §35's clock, the season, the temperature.
        bool toggleEnvironmentOverlayPressed = false;
        /// @brief §69's `F9`: the physics overlay -- collision shapes, the capsule, the probe.
        bool togglePhysicsOverlayPressed = false;
        bool screenshotPressed = false;
    };

    /// @brief Where `InputState` comes from. The only thing in the project that reads XNA input.
    ///
    /// `HOUSE-00140`'s rule: **no system may read `Keyboard` or `Mouse` directly.** Not a style
    /// preference -- a system that polls the keyboard cannot be driven by a replay, cannot be tested
    /// without a window, and cannot be remapped. The lint of `HOUSE-00021` enforces the boundary.
    class IInputSource
    {
    public:
        virtual ~IInputSource() = default;

        /// @brief Samples the devices and produces this frame's intent.
        /// @param deltaSeconds the clamped frame delta, for any rate-based conversion.
        virtual void Update(float deltaSeconds) = 0;

        [[nodiscard]] virtual const InputState& Current() const noexcept = 0;

        /// @brief Whether look input should be consumed at all this frame.
        ///
        /// False when the window is not focused or the pointer has not been captured.
        /// **`HOUSE-00100` measured why this is separate from "is the window active":**
        /// `Game::IsActive` was true on all 9 999 frames of that probe while `Mouse::GetState`'s
        /// event-driven snapshot never advanced past `(0,0)`. A camera gating only on `IsActive` would
        /// have consumed a garbage delta every frame.
        [[nodiscard]] virtual bool LookAvailable() const noexcept = 0;
    };

} // namespace cnahouse::player
