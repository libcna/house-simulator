// SPDX-License-Identifier: MIT
#include "cnahouse/lighting/DaylightModel.hpp"

#include <algorithm>
#include <cmath>

#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::lighting
{
    namespace
    {
        constexpr double kDegToRad = 0.017453292519943295769236907684886;
        constexpr double kRadToDeg = 57.295779513082320876798154814105;

        [[nodiscard]] double ClampUnit(double value) noexcept
        {
            if (!std::isfinite(value))
            {
                return 0.0;
            }
            return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
        }

        /// @brief The compass bearing of an outward normal. §10.1: north is `−Z`, east is `+X`.
        ///
        /// Only six values are reachable -- §15's portals are axis-aligned planes -- but it is
        /// computed rather than tabulated, because a table is a second place the convention can be
        /// written down wrong.
        [[nodiscard]] float BearingOf(float x, float z) noexcept
        {
            const double degrees = std::atan2(static_cast<double>(x), -static_cast<double>(z)) * kRadToDeg;
            return static_cast<float>(degrees < 0.0 ? degrees + 360.0 : degrees);
        }

        /// @brief Which of a portal's two cells is the interior one, or an invalid id for a portal
        ///        that joins two interior cells or two exterior ones.
        [[nodiscard]] util::Id InteriorCellOf(const world::WorldData& world, const world::Portal& portal)
        {
            const world::Cell* a = world.FindCell(portal.cellA);
            const world::Cell* b = world.FindCell(portal.cellB);
            if (a == nullptr || b == nullptr)
            {
                return util::Id();
            }
            // **Open to the SKY, which is not the same as `CellKind::Exterior`.** `EXT_SHED` is
            // an exterior cell because it is outside the house, and it is a shed: walls, a roof,
            // one window, `visibilityHint: opaque`. Treating `Exterior` as "outdoors" excluded its
            // window and left the shed unable to see daylight through the one opening it has.
            // Conversely `visibilityHint: open` alone is not it either — `L0_STAIR_MAIN` and
            // `L1_LANDING` are open to the storey above and are indoors. It is both together, and
            // that picks out exactly the 17 cells a person would point at and call outside.
            const auto outdoors = [](const world::Cell& cell)
            {
                return cell.kind == world::CellKind::Exterior &&
                       cell.visibilityHint == world::VisibilityHint::Open;
            };
            const bool aOutside = outdoors(*a);
            const bool bOutside = outdoors(*b);
            if (aOutside == bOutside)
            {
                // Neither: an interior window, which has no sky (see the class comment). Both:
                // not a window into anything.
                return util::Id();
            }
            return aOutside ? b->id : a->id;
        }

        [[nodiscard]] float FloorAreaOf(const world::Cell& cell) noexcept
        {
            float area = 0.0F;
            for (const world::Footprint& box : cell.boxes)
            {
                area += box.Area();
            }
            return area;
        }

    } // namespace

    SkyExposure SkyExposureFor(double windowAzimuthDeg,
                               double sunAltitudeDeg,
                               double sunAzimuthDeg,
                               double cloudCover) noexcept
    {
        SkyExposure exposure;
        if (!std::isfinite(windowAzimuthDeg) || !std::isfinite(sunAltitudeDeg) ||
            !std::isfinite(sunAzimuthDeg))
        {
            return exposure;
        }
        const double cover = ClampUnit(cloudCover);
        // §28.4: *"proportional to max(0, sin(sunAltitude))"* -- which is the cosine of the zenith
        // angle, and therefore how irradiance on a horizontal surface actually scales. A sun below
        // the horizon contributes nothing through the window; what the sky still gives the room at
        // dusk is §32.2's LUT, not this.
        const double sinAltitude = std::sin(sunAltitudeDeg * kDegToRad);
        if (sinAltitude <= 0.0)
        {
            return exposure;
        }
        // §32.2's own sky-diffuse cloud factor, `1 − 0.35·cloudCover`, rather than a second number
        // for the same physical thing: overcast flattens the light instead of removing it.
        exposure.diffuse = static_cast<float>(sinAltitude * (1.0 - 0.35 * cover));

        double difference = std::fmod(sunAzimuthDeg - windowAzimuthDeg, 360.0);
        if (difference < -180.0)
        {
            difference += 360.0;
        }
        else if (difference > 180.0)
        {
            difference -= 360.0;
        }
        const double cosDifference = std::cos(difference * kDegToRad);
        const double cosCone = std::cos(kDirectConeDeg * kDegToRad);
        if (cosDifference <= cosCone)
        {
            return exposure;
        }
        // Reaches exactly zero at the cone's edge, so §28.4's *"non-zero only when within ±75°"*
        // holds and nothing steps. See the header for why a hard edge was not taken.
        const double taper = (cosDifference - cosCone) / (1.0 - cosCone);
        const double clearSky = (1.0 - cover) * (1.0 - cover) * (1.0 - cover);
        exposure.direct = static_cast<float>(sinAltitude * taper * clearSky);
        return exposure;
    }

    DaylightModel::DaylightModel(const world::WorldData& world, const ShadingGrid& shading)
        : shading_(&shading)
    {
        const std::span<const world::Cell> cells = world.Cells();
        cells_.reserve(cells.size());
        cellIndex_.reserve(cells.size());
        for (const world::Cell& cell : cells)
        {
            cellIndex_.emplace(cell.id.Value(), cells_.size());
            CellEntry entry;
            entry.cell = cell.id;
            entry.floorAreaM2 = FloorAreaOf(cell);
            cells_.push_back(entry);
        }

        // Gather first, then sort by cell, so one cell's windows are contiguous and the per-frame
        // walk is a span rather than a lookup per window.
        std::vector<DaylightWindow> gathered;
        for (const world::Opening& opening : world.Openings())
        {
            if (opening.kind != world::OpeningKind::Window)
            {
                continue;
            }
            const world::Portal* portal = world.FindPortal(opening.portal);
            if (portal == nullptr)
            {
                continue;
            }
            const util::Id interior = InteriorCellOf(world, *portal);
            if (!interior.IsValid())
            {
                continue;
            }
            DaylightWindow window;
            window.window = opening.id;
            window.cell = interior;
            window.areaM2 = portal->Width() * portal->Height();
            // Outward is away from the interior cell, decided from the plane and the cell's own
            // boxes -- never from which of `cellA`/`cellB` the portal happens to name first. A
            // window authored the other way round would otherwise face into the room it lights.
            const world::Cell* room = world.FindCell(interior);
            float centre = 0.0F;
            if (room != nullptr && !room->boxes.empty())
            {
                for (const world::Footprint& box : room->boxes)
                {
                    centre += portal->axis == world::PlaneAxis::X ? (box.minX + box.maxX) * 0.5F
                                                                  : (box.minZ + box.maxZ) * 0.5F;
                }
                centre /= static_cast<float>(room->boxes.size());
            }
            const float outward = portal->planeValue > centre ? 1.0F : -1.0F;
            window.azimuthDeg =
                portal->axis == world::PlaneAxis::X ? BearingOf(outward, 0.0F) : BearingOf(0.0F, outward);
            window.transmission = kTransmissionGlass;
            gathered.push_back(window);
        }
        std::stable_sort(gathered.begin(),
                         gathered.end(),
                         [](const DaylightWindow& left, const DaylightWindow& right)
                         { return left.cell.Value() < right.cell.Value(); });
        windows_ = std::move(gathered);
        openFraction_.assign(windows_.size(), 0.0F);
        windowIndex_.reserve(windows_.size());
        for (std::size_t index = 0; index < windows_.size(); ++index)
        {
            windowIndex_.emplace(windows_[index].window.Value(), index);
            const auto found = cellIndex_.find(windows_[index].cell.Value());
            if (found == cellIndex_.end())
            {
                continue;
            }
            CellEntry& entry = cells_[found->second];
            if (entry.windowCount == 0)
            {
                entry.firstWindow = index;
            }
            ++entry.windowCount;
        }

        // §65.6's authored start, where it names a window: `initialstate.json` opens
        // `WIN_L1_MASTER_N2` half way, and a model that ignored it would light that room as if the
        // sash were shut on the first frame and correctly the moment anything touched it.
        //
        // **The join is the PORTAL and not the id, and that is not an accident of this file.**
        // `interactables.json` names a window `WIN_L1_MASTER_N2` -- cell, compass point, ordinal --
        // and `layout.openings.json` names the same window `WIN_L1_MASTER_BED_2` -- cell, ordinal.
        // All 54 window interactables differ from their opening's id that way, and both rows carry
        // the portal, which is the thing they are actually both about. Matching on the id finds
        // nothing and fails silently, which is what the first draft of this did.
        std::unordered_map<std::uint32_t, std::size_t> byPortal;
        byPortal.reserve(windows_.size());
        for (std::size_t index = 0; index < windows_.size(); ++index)
        {
            const world::Opening* opening = world.FindOpening(windows_[index].window);
            if (opening != nullptr && opening->portal.IsValid())
            {
                byPortal.emplace(opening->portal.Value(), index);
            }
        }
        for (const world::InteractableStart& start : world.GetInitialState().interactables)
        {
            const world::StateTable::Field* field = start.overrides.Find("openFraction");
            if (field == nullptr)
            {
                continue;
            }
            const auto* value = std::get_if<double>(&field->value);
            if (value == nullptr)
            {
                continue;
            }
            const world::Interactable* row = world.FindInteractable(start.id);
            if (row == nullptr || !row->portal.IsValid())
            {
                continue;
            }
            const auto found = byPortal.find(row->portal.Value());
            if (found != byPortal.end())
            {
                openFraction_[found->second] = std::clamp(static_cast<float>(*value), 0.0F, 1.0F);
            }
        }
    }

    bool DaylightModel::SetOpenFraction(util::Id window, float fraction) noexcept
    {
        if (!std::isfinite(fraction))
        {
            return false;
        }
        const auto found = windowIndex_.find(window.Value());
        if (found == windowIndex_.end())
        {
            return false;
        }
        openFraction_[found->second] = std::clamp(fraction, 0.0F, 1.0F);
        return true;
    }

    float DaylightModel::OpenFraction(util::Id window) const noexcept
    {
        const auto found = windowIndex_.find(window.Value());
        return found == windowIndex_.end() ? 0.0F : openFraction_[found->second];
    }

    float DaylightModel::FloorAreaFor(util::Id cell) const noexcept
    {
        const auto found = cellIndex_.find(cell.Value());
        return found == cellIndex_.end() ? 0.0F : cells_[found->second].floorAreaM2;
    }

    float DaylightModel::GlazingRatioFor(util::Id cell) const noexcept
    {
        const auto found = cellIndex_.find(cell.Value());
        if (found == cellIndex_.end())
        {
            return 0.0F;
        }
        const CellEntry& entry = cells_[found->second];
        if (entry.floorAreaM2 <= 0.0F)
        {
            return 0.0F;
        }
        float area = 0.0F;
        for (std::size_t offset = 0; offset < entry.windowCount; ++offset)
        {
            area += windows_[entry.firstWindow + offset].areaM2;
        }
        return area / entry.floorAreaM2;
    }

    float DaylightModel::Accumulate(const CellEntry& entry,
                                    double sunAltitudeDeg,
                                    double sunAzimuthDeg,
                                    double cloudCover) const noexcept
    {
        if (entry.windowCount == 0 || entry.floorAreaM2 <= 0.0F)
        {
            return 0.0F;
        }
        double sum = 0.0;
        for (std::size_t offset = 0; offset < entry.windowCount; ++offset)
        {
            const std::size_t index = entry.firstWindow + offset;
            const DaylightWindow& window = windows_[index];
            const SkyExposure exposure = SkyExposureFor(
                static_cast<double>(window.azimuthDeg), sunAltitudeDeg, sunAzimuthDeg, cloudCover);
            // **§28.4's `shadingFactor` multiplies the DIRECT term only, and this is a correction
            // to the section.** The baked grid is a sun-direction occlusion mask: it stores 0 for
            // every direction behind the window's own wall, so a north window's factor is 0
            // whenever the sun is in the south -- all day at 40° N. Multiplying the whole of
            // `skyExposure` by it, as §28.4 writes it, made `L0_FAMILY` read exactly 0.000 at a
            // 45° sun due south. The diffuse term is a sky-DOME quantity and takes
            // `SkyViewFactor` instead: the same baked occlusion integrated over the hemisphere the
            // window faces rather than sampled along one ray.
            const double sky =
                static_cast<double>(exposure.diffuse) *
                    static_cast<double>(shading_->SkyViewFactor(window.window)) +
                static_cast<double>(exposure.direct) *
                    static_cast<double>(shading_->Factor(window.window, sunAltitudeDeg, sunAzimuthDeg));
            if (sky <= 0.0)
            {
                continue;
            }
            // §28.4 gives `openBoost` as a value and not a curve -- *"1.15 when open"* -- so a
            // half-open sash gets half of the boost, which is the only reading that is continuous
            // as a window swings.
            const double boost =
                1.0 + (static_cast<double>(kOpenBoost) - 1.0) * static_cast<double>(openFraction_[index]);
            sum +=
                static_cast<double>(window.areaM2) * static_cast<double>(window.transmission) * boost * sky;
        }
        const double ratio = sum / static_cast<double>(entry.floorAreaM2);
        // The reference room at the reference sun: §28.4's sum for a 0.20-glazed room with clear
        // glass facing an equinox noon head-on under a clear sky. See the two constants -- a
        // glazing ratio alone does not fix a level, and the first draft of this file forgot that
        // and clamped every room with a window to 1.0.
        const double reference = static_cast<double>(kFullDaylightGlazingRatio) *
                                 static_cast<double>(kTransmissionGlass) * kReferenceSkyExposure;
        return static_cast<float>(std::clamp(ratio / reference, 0.0, 1.0));
    }

    float DaylightModel::DaylightFor(util::Id cell,
                                     double sunAltitudeDeg,
                                     double sunAzimuthDeg,
                                     double cloudCover) const noexcept
    {
        const auto found = cellIndex_.find(cell.Value());
        if (found == cellIndex_.end())
        {
            return 0.0F;
        }
        return Accumulate(cells_[found->second], sunAltitudeDeg, sunAzimuthDeg, cloudCover);
    }

    void DaylightModel::Evaluate(double sunAltitudeDeg,
                                 double sunAzimuthDeg,
                                 double cloudCover,
                                 std::span<float> out) const
    {
        const std::size_t count = std::min(out.size(), cells_.size());
        for (std::size_t index = 0; index < count; ++index)
        {
            out[index] = Accumulate(cells_[index], sunAltitudeDeg, sunAzimuthDeg, cloudCover);
        }
    }

} // namespace cnahouse::lighting
