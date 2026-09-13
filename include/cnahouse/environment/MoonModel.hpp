// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/environment/SunModel.hpp"

namespace cnahouse::environment
{

    /// @brief Geocentric altitude of the moon's centre at conventional moonrise/moonset.
    ///
    /// The moon's mean horizontal parallax (~57') puts its geocentric centre above the true
    /// horizon when the upper limb appears through mean atmospheric refraction. The conventional
    /// low-precision value is +0.125 degrees; unlike the sun's threshold, its sign is positive.
    inline constexpr double kMoonRefractedHorizonDeg = 0.125;

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
