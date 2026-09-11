// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <span>
#include <unordered_map>
#include <vector>

#include "cnahouse/lighting/ShadingGrid.hpp"
#include "cnahouse/util/Ids.hpp"

namespace cnahouse::world
{
    class WorldData;
}

namespace cnahouse::lighting
{

    /// @brief §28.4's transmission of one pane, by what is hanging in front of it.
    ///
    /// The section's own three numbers. Curtains and blinds are §54's interactables and do not
    /// exist yet, so every window is `kGlass` today — the constants are here because the formula
    /// that multiplies them is, and inventing a fourth number later would be worse than declaring
    /// the three now.
    inline constexpr float kTransmissionGlass = 0.86F;
    inline constexpr float kTransmissionCurtain = 0.25F;
    inline constexpr float kTransmissionBlind = 0.05F;

    /// @brief §28.4's `openBoost`: *"1.15 when open"*. An open sash removes the glass's own loss.
    inline constexpr float kOpenBoost = 1.15F;

    /// @brief §28.4's acceptance cone for the DIRECT term: *"within ±75° of the window's outward
    ///        normal"*.
    inline constexpr double kDirectConeDeg = 75.0;

    /// @brief The glazing ratio at which a room counts as fully daylit, at the reference sun.
    ///
    /// §28.4's sum divides by the cell's floor area, so what comes out is a **glazing ratio** — a
    /// real architectural quantity — and not a `[0, 1]` level. Something has to say which ratio is
    /// "fully lit", and that number is measured from this house rather than guessed at.
    ///
    /// Measured over the 36 cells that have windows: ratio **0.026 to 0.402**, median **0.115**.
    /// The habitable rooms cluster tightly just below and above 0.20 — `L1_BED3` and `L2_BED6` at
    /// 0.200, `L0_LIVING` 0.195, `L0_FAMILY` 0.232, `L0_OFFICE` 0.240 — so 0.20 is *"a well-lit
    /// room"* in the only sense this house can define it. `L0_SUNROOM` at **0.402** is half as
    /// bright again, which is correct: it is a sunroom. `L0_GARAGE` at 0.026 gets an eighth of a
    /// bedroom's daylight, which is also correct.
    inline constexpr float kFullDaylightGlazingRatio = 0.20F;

    /// @brief The sun the ratio above is "fully lit" AT.
    ///
    /// **A glazing ratio on its own does not fix a level, and the first draft of this file forgot
    /// that.** `SkyExposureFor` reaches nearly 2 at midsummer noon — the diffuse and direct terms
    /// each approach `sin(altitude)` — so dividing the sum by the glazing ratio alone put every
    /// room with a window at a clamped 1.0 and made the shading, the cloud cover and the open sash
    /// all invisible behind the clamp. The reference has to name a SUN as well as a room.
    ///
    /// The equinox noon at §33's 40.05° N: altitude `90 − 40.05` = **49.95°**, due south, clear
    /// sky, head-on. Chosen because it is the one sun this location has that is neither extreme —
    /// midsummer noon would make a bedroom clamp all summer, midwinter would make the sunroom
    /// clamp all year — and because an equinox noon is what "an ordinary bright day" means at any
    /// latitude, so the constant travels if the location setting ever moves.
    inline constexpr double kReferenceSunAltitudeDeg = 49.95;

    /// @brief `SkyExposureFor` at the reference sun, head-on and clear: `2 · sin(49.95°)`.
    ///
    /// Written out rather than computed at start-up so it is visible beside the ratio it divides.
    /// `DaylightModelTests` asserts the two agree, so it cannot drift from the function.
    inline constexpr double kReferenceSkyExposure = 1.5309711;

    /// @brief §28.4's `skyExposure`, split into the two terms the section describes.
    struct SkyExposure
    {
        /// @brief *"A diffuse sky term proportional to `max(0, sin(sunAltitude))` modulated by
        ///        cloud cover."*
        float diffuse = 0.0F;
        /// @brief *"Plus a direct-sun term that is non-zero only when the sun's azimuth is within
        ///        ±75° of the window's outward normal and the sun is above the horizon, multiplied
        ///        by `(1 − cloudCover)³`."*
        float direct = 0.0F;

        [[nodiscard]] float Total() const noexcept
        {
            return diffuse + direct;
        }
    };

    /// @brief §28.4's `skyExposure` for a window facing @p windowAzimuthDeg.
    ///
    /// @param windowAzimuthDeg the compass bearing of the window's OUTWARD normal, from north
    ///        clockwise — the same convention §32.1's sun azimuth uses.
    /// @param cloudCover `[0, 1]`; out of range is clamped and non-finite is clear sky.
    ///
    /// **The direct term's cone is tapered rather than switched.** §28.4 says the term is *"non-zero
    /// only when the sun's azimuth is within ±75°"*, which a literal implementation satisfies with
    /// a hard edge — and a hard edge is a step in a room's brightness as the sun crosses 75° off
    /// the window, once per window per day, exactly the kind of thing §36.3 spends a paragraph
    /// forbidding for seasons. The taper `(cos Δ − cos 75°) / (1 − cos 75°)` is 1 head-on, reaches
    /// **exactly zero at 75°** and stays there, so it satisfies the sentence as written and has no
    /// step. It introduces no number that is not already in §28.4.
    [[nodiscard]] SkyExposure SkyExposureFor(double windowAzimuthDeg,
                                             double sunAltitudeDeg,
                                             double sunAzimuthDeg,
                                             double cloudCover) noexcept;

    /// @brief One window, as §28.4's sum needs it. Everything here is fixed for the session.
    struct DaylightWindow
    {
        util::Id window;
        /// @brief The interior cell it lights.
        util::Id cell;
        /// @brief The portal rectangle's area, m². The APERTURE, which is what admits light.
        float areaM2 = 0.0F;
        /// @brief The outward normal as a compass bearing, from north clockwise.
        float azimuthDeg = 0.0F;
        float transmission = kTransmissionGlass;
    };

    /// @brief §28.4's daylight model (`HOUSE-01263`).
    ///
    /// Built once over the world's windows and evaluated per frame against the sun. **A window
    /// whose portal joins two interior cells is not in here**: an interior window has no sky, and
    /// the light the far room borrows through it is §28.4's 2-hop flood (`HOUSE-01265`), not a sky
    /// term. `HOUSE-01279` found exactly one, `WIN_L0_KITCHEN_2` between the kitchen and the
    /// sunroom, and counting it would have given the kitchen daylight from a window that faces a
    /// wall.
    class DaylightModel
    {
    public:
        /// @brief Takes the world and the baked grid. Both outlive the model.
        ///
        /// @p shading may be `ShadingGrid::Unshaded()`; every window is then unoccluded, which is
        /// what a checkout with no Blender gets and is wrong in a direction a person can see.
        DaylightModel(const world::WorldData& world, const ShadingGrid& shading);

        /// @brief Deleted, because the model keeps a POINTER to the grid.
        ///
        /// `DaylightModel model(world, LoadShading());` binds a temporary to the parameter, stores
        /// its address, and leaves a dangling pointer the moment the full expression ends. It was
        /// written that way in this task's own first draft of `DaylightModelTests` and the symptom
        /// was not a crash: every window read a shading factor of 1.0, so the baked grid appeared
        /// to make no difference and the test that checks it appeared to find a content bug.
        /// Deleting the overload turns that into a compile error.
        DaylightModel(const world::WorldData&, ShadingGrid&&) = delete;

        /// @brief §28.4's per-cell daylight, `[0, 1]`, written into @p out by cell index.
        ///
        /// @p out must be the world's cell count; the model writes every entry, so a cell with no
        /// windows is set to 0 rather than left at whatever was there.
        void
        Evaluate(double sunAltitudeDeg, double sunAzimuthDeg, double cloudCover, std::span<float> out) const;

        /// @brief The daylight of one cell, for a test or a debug overlay.
        [[nodiscard]] float DaylightFor(util::Id cell,
                                        double sunAltitudeDeg,
                                        double sunAzimuthDeg,
                                        double cloudCover) const noexcept;

        /// @brief §28.4's `openBoost`: how far open a window's sash is, `[0, 1]`.
        ///
        /// Returns false for a window the model does not carry. Clamped; a non-finite value is
        /// refused, because a settings or save file is user-editable text.
        bool SetOpenFraction(util::Id window, float fraction) noexcept;

        [[nodiscard]] float OpenFraction(util::Id window) const noexcept;

        [[nodiscard]] std::span<const DaylightWindow> Windows() const noexcept
        {
            return windows_;
        }

        /// @brief The cell's floor area in m², summed over §15's footprint boxes.
        [[nodiscard]] float FloorAreaFor(util::Id cell) const noexcept;

        /// @brief The cell's glazing ratio: Σ window area ÷ floor area. The quantity §28.4's sum
        ///        is built out of, before the sun and the weather are applied.
        [[nodiscard]] float GlazingRatioFor(util::Id cell) const noexcept;

    private:
        struct CellEntry
        {
            util::Id cell;
            float floorAreaM2 = 0.0F;
            std::size_t firstWindow = 0;
            std::size_t windowCount = 0;
        };

        [[nodiscard]] float Accumulate(const CellEntry& entry,
                                       double sunAltitudeDeg,
                                       double sunAzimuthDeg,
                                       double cloudCover) const noexcept;

        const ShadingGrid* shading_ = nullptr;
        /// @brief Sorted by cell, so one cell's windows are contiguous.
        std::vector<DaylightWindow> windows_;
        std::vector<float> openFraction_;
        std::vector<CellEntry> cells_;
        std::unordered_map<std::uint32_t, std::size_t> cellIndex_;
        std::unordered_map<std::uint32_t, std::size_t> windowIndex_;
    };

} // namespace cnahouse::lighting
