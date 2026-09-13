// SPDX-License-Identifier: MIT
#pragma once

#include <string_view>

#include "cnahouse/environment/SunModel.hpp"

namespace cnahouse::environment
{

    /// @brief Geocentric altitude of the moon's centre at conventional moonrise/moonset.
    ///
    /// The moon's mean horizontal parallax (~57') puts its geocentric centre above the true
    /// horizon when the upper limb appears through mean atmospheric refraction. The conventional
    /// low-precision value is +0.125 degrees; unlike the sun's threshold, its sign is positive.
    inline constexpr double kMoonRefractedHorizonDeg = 0.125;

    /// @brief §33.5's mean interval between like lunar phases.
    inline constexpr double kSynodicMonthDays = 29.530588;

    /// @brief §33.1's geocentric lunar position, expressed in the same conventions as SunPosition.
    struct MoonPosition
    {
        double declinationDeg = 0.0;
        double hourAngleDeg = 0.0;
        /// @brief Geometric geocentric altitude; no refraction or topocentric parallax is applied.
        double altitudeDeg = 0.0;
        /// @brief Azimuth from north, clockwise through east, `[0, 360)`.
        double azimuthDeg = 0.0;
        double rightAscensionDeg = 0.0;
        double eclipticLongitudeDeg = 0.0;
        double eclipticLatitudeDeg = 0.0;
    };

    /// @brief §33.1's truncated ELP lunar position at days since J2000.0.
    ///
    /// The model evaluates the four fundamental lunar arguments and the six largest periodic
    /// longitude and latitude terms. It is deliberately a compact visual ephemeris, not a second
    /// full astronomical almanac; MoonModelTests compares its rise times against the USNO's.
    [[nodiscard]] MoonPosition MoonPositionAt(double daysSinceJ2000, const SunObserver& observer) noexcept;

    /// @brief The moon at the compressed civil instant and location held by @p clock.
    [[nodiscard]] MoonPosition MoonPositionFor(const SimClock& clock) noexcept;

    /// @brief §33.2's continuous lunar phase and the quantities from which it is built.
    struct MoonPhase
    {
        /// @brief Angle between geocentric sun and moon directions, `[0, 180]` degrees.
        double elongationDeg = 0.0;
        /// @brief Fraction of the apparent lunar disc illuminated, `[0, 1]`.
        double illuminatedFraction = 0.0;
        bool waxing = true;
        /// @brief Continuous lunation position, `[0, 1)`: zero new, 0.5 full.
        double phase = 0.0;
    };

    /// @brief Compute §33.2's phase from already-evaluated geocentric positions.
    [[nodiscard]] MoonPhase MoonPhaseFromPositions(const MoonPosition& moon, const SunPosition& sun) noexcept;

    /// @brief Compute the moon and sun at one J2000 instant, then derive their phase relation.
    [[nodiscard]] MoonPhase MoonPhaseAt(double daysSinceJ2000, const SunObserver& observer) noexcept;

    /// @brief §33.2's phase at the compressed civil instant held by @p clock.
    [[nodiscard]] MoonPhase MoonPhaseFor(const SimClock& clock) noexcept;

    /// @brief The same phase, reusing positions already evaluated for this frame.
    ///
    /// At the default 1x rate this is exactly `MoonPhaseFromPositions`. Above 1x, only the phase
    /// advances faster; @p moon remains the physical position used to draw and light the scene.
    [[nodiscard]] MoonPhase
    MoonPhaseFor(const SimClock& clock, const MoonPosition& moon, const SunPosition& sun) noexcept;

    /// @brief The eight presentation names from §33.2 for a continuous circular @p phase.
    ///
    /// This is the one mapping shared by the overlay and almanac. Finite values are wrapped rather
    /// than clamped because phase is circular; a non-finite value fails closed to "New moon".
    [[nodiscard]] std::string_view MoonPhaseName(double phase) noexcept;

    struct MoonEvent
    {
        bool occurs = false;
        double minutesOfDay = 0.0;
    };

    /// @brief The first rise and set of one local-standard civil date, solved from MoonPositionAt.
    struct MoonDay
    {
        MoonEvent rise;
        MoonEvent set;
    };

    /// @brief Find lunar horizon crossings on one local-standard civil date.
    [[nodiscard]] MoonDay MoonDayFor(const CivilTime& localStandardDate,
                                     const SunObserver& observer,
                                     double thresholdDeg = kMoonRefractedHorizonDeg) noexcept;

} // namespace cnahouse::environment
