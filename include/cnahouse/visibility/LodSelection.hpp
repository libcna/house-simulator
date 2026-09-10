// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace cnahouse::visibility
{

    /// @brief §26.1's five levels, coarsening downwards.
    ///
    /// The order is the whole design: a level is an index, "coarser" is `+1`, and the thresholds
    /// below are read by the same index. `Culled` is a level and not a separate answer, because
    /// §26.1's own table ends with it -- an object under 8 px is not drawn, and saying so with the
    /// same type is what lets one comparison decide between all five.
    enum class LodLevel : std::uint8_t
    {
        Lod0,
        Lod1,
        Lod2,
        Impostor,
        Culled,
        Count,
    };

    /// @brief §26.1's four thresholds in projected pixels, in `LodLevel` order.
    ///
    /// `Culled` has none: it is what is left below the last one.
    inline constexpr std::array<float, static_cast<std::size_t>(LodLevel::Culled)> kLodThresholds{
        220.0F, 90.0F, 30.0F, 8.0F};

    /// @brief §26.1: *"switch up at the threshold, switch down at 0.85 × the threshold"*.
    ///
    /// The deadband is what stops an object standing at a boundary from flipping every frame --
    /// at LOD1's 90 px it holds LOD1 down to 76.5 px and takes LOD1 again only at 90. Without it a
    /// player walking slowly toward a neighbour would see the house swap meshes several times a
    /// second, which is far more visible than either mesh.
    inline constexpr float kLodHysteresis = 0.85F;

    [[nodiscard]] std::string_view LodLevelName(LodLevel level) noexcept;

    /// @brief The pixel height @p level needs, or 0 for `Culled`, which needs none.
    [[nodiscard]] float LodThreshold(LodLevel level) noexcept;

    /// @brief §26.1's metric: the projected screen height of a bounding sphere, in pixels.
    ///
    /// `h = 2 · r · viewportHeight / (2 · d · tan(fovY / 2))`, with **`d` clamped to `> r`** --
    /// §26.1 says so, and it is the clamp that keeps the answer finite for a camera inside the
    /// sphere, where the projection has no height to give.
    ///
    /// @param radius the bounding sphere's radius in metres. Zero or negative gives 0 px: a thing
    ///        with no size projects to nothing, which is `Culled`, and is a truer answer than a
    ///        division that happens not to trap.
    /// @param distance metres from the eye to the sphere's centre.
    /// @param viewportHeight the back buffer's height in pixels -- §26.1's thresholds are pixels,
    ///        so a 720p window and a 4K one legitimately choose differently for the same object.
    /// @param fovYRadians the camera's EFFECTIVE vertical field of view
    ///        (`FirstPersonCamera::EffectiveFieldOfViewDegrees`, in radians), not the setting: a
    ///        window narrower than 4:3 opens the lens, and an object's projected height goes with
    ///        it.
    [[nodiscard]] float
    ProjectedHeight(float radius, float distance, float viewportHeight, float fovYRadians) noexcept;

    /// @brief The level @p height alone puts an object at, with no hysteresis.
    ///
    /// The answer for an object being seen for the first time, and the one `SelectLod` measures
    /// its deadband against.
    [[nodiscard]] LodLevel LevelFor(float height) noexcept;

    /// @brief §26.1's selection with hysteresis: the level to draw at, given the last one.
    ///
    /// Switching to a FINER level needs the finer level's own threshold; falling back to a coarser
    /// one needs the height to be under `kLodHysteresis` × the threshold of the level it is
    /// currently at. An object that crosses two bands at once -- a camera cut, a teleport -- takes
    /// both steps in one call, because the deadband exists to absorb dithering and not to slow a
    /// real change down.
    ///
    /// @param previous what this function returned for this object last frame. `Culled` is the
    ///        right seed for an object that has never been drawn: the first call then simply
    ///        returns `LevelFor(height)`.
    [[nodiscard]] LodLevel SelectLod(float height, LodLevel previous) noexcept;

    /// @brief @p level made @p steps coarser, saturating at `Culled`. Negative makes it finer.
    ///
    /// §26.1's global `lodBias` and §15.4's *"+1 through frosted glass"* are both this, and both
    /// are applied to the SELECTED level rather than inside the selection: hysteresis needs to
    /// remember the level the metric chose, and a bias that changed -- a quality setting moved, a
    /// door closed -- would otherwise read as an object that had moved. Where the bias comes from
    /// is `HOUSE-02393`'s; `visibility::CellDetail::lodBias` is the interior half of it already.
    [[nodiscard]] LodLevel Coarsen(LodLevel level, int steps) noexcept;

} // namespace cnahouse::visibility
