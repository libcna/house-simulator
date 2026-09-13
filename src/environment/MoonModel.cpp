// SPDX-License-Identifier: MIT
#include "cnahouse/environment/MoonModel.hpp"

#include <cmath>

namespace cnahouse::environment
{
    namespace
    {
        constexpr double kDegToRad = 0.017453292519943295769236907684886;
        constexpr double kRadToDeg = 57.295779513082320876798154814105;
        constexpr double kMinutesPerDay = 1440.0;
        constexpr double kScanStepMinutes = 4.0;
        constexpr int kBisectionSteps = 40;

        [[nodiscard]] double Wrap360(double degrees) noexcept
        {
            const double wrapped = std::fmod(degrees, 360.0);
            return wrapped < 0.0 ? wrapped + 360.0 : wrapped;
        }

        [[nodiscard]] double Wrap180(double degrees) noexcept
        {
            return Wrap360(degrees + 180.0) - 180.0;
        }

        [[nodiscard]] double SinDeg(double degrees) noexcept
        {
            return std::sin(degrees * kDegToRad);
        }

        [[nodiscard]] double WholeDaysFromJ2000Date(const CivilTime& time) noexcept
        {
            return static_cast<double>(DaysFromCivil(time.year, time.month, time.day) -
                                       DaysFromCivil(2000, 1, 1));
        }

        [[nodiscard]] double
        AltitudeAt(double dayStartJ2000, double minutesOfDay, const SunObserver& observer) noexcept
        {
            return MoonPositionAt(dayStartJ2000 + minutesOfDay / kMinutesPerDay, observer).altitudeDeg;
        }

        [[nodiscard]] double Bisect(double dayStartJ2000,
                                    double lo,
                                    double hi,
                                    double thresholdDeg,
                                    const SunObserver& observer) noexcept
        {
            const double loSign = AltitudeAt(dayStartJ2000, lo, observer) - thresholdDeg;
            for (int step = 0; step < kBisectionSteps; ++step)
            {
                const double mid = 0.5 * (lo + hi);
                const double midSign = AltitudeAt(dayStartJ2000, mid, observer) - thresholdDeg;
                if ((midSign < 0.0) == (loSign < 0.0))
                {
                    lo = mid;
                }
                else
                {
                    hi = mid;
                }
            }
            return 0.5 * (lo + hi);
        }
    } // namespace

    MoonPosition MoonPositionAt(double daysSinceJ2000, const SunObserver& observer) noexcept
    {
        MoonPosition position;
        if (!std::isfinite(daysSinceJ2000) || !std::isfinite(observer.latitudeDeg) ||
            !std::isfinite(observer.longitudeDeg))
        {
            return position;
        }

        // The ELP2000-82B fundamental arguments, in Julian centuries from J2000.
        // The small quadratic terms matter over the project's supported calendar range but do not
        // change the deliberately short periodic series below.
        const double t = daysSinceJ2000 / 36525.0;
        const double t2 = t * t;
        const double t3 = t2 * t;
        const double t4 = t3 * t;
        const double meanLongitudeDeg =
            Wrap360(218.3164477 + 481267.88123421 * t - 0.0015786 * t2 + t3 / 538841.0 - t4 / 65194000.0);
        const double elongationDeg =
            Wrap360(297.8501921 + 445267.1114034 * t - 0.0018819 * t2 + t3 / 545868.0 - t4 / 113065000.0);
        const double solarAnomalyDeg =
            Wrap360(357.5291092 + 35999.0502909 * t - 0.0001536 * t2 + t3 / 24490000.0);
        const double lunarAnomalyDeg =
            Wrap360(134.9633964 + 477198.8675055 * t + 0.0087414 * t2 + t3 / 69699.0 - t4 / 14712000.0);
        const double latitudeArgumentDeg =
            Wrap360(93.2720950 + 483202.0175233 * t - 0.0036539 * t2 - t3 / 3526000.0 + t4 / 863310000.0);
        const double eccentricity = 1.0 - 0.002516 * t - 0.0000074 * t2;

        // Six largest ELP longitude terms, in degrees. The solar-anomaly term carries the earth's
        // orbital eccentricity factor E; every other retained term has M coefficient zero.
        const double longitudeCorrectionDeg =
            6.288774 * SinDeg(lunarAnomalyDeg) + 1.274027 * SinDeg(2.0 * elongationDeg - lunarAnomalyDeg) +
            0.658314 * SinDeg(2.0 * elongationDeg) + 0.213618 * SinDeg(2.0 * lunarAnomalyDeg) -
            0.185116 * eccentricity * SinDeg(solarAnomalyDeg) - 0.114332 * SinDeg(2.0 * latitudeArgumentDeg);

        // Six largest latitude terms from the same truncation.
        const double latitudeDeg =
            5.128122 * SinDeg(latitudeArgumentDeg) +
            0.280602 * SinDeg(lunarAnomalyDeg + latitudeArgumentDeg) +
            0.277693 * SinDeg(lunarAnomalyDeg - latitudeArgumentDeg) +
            0.173237 * SinDeg(2.0 * elongationDeg - latitudeArgumentDeg) +
            0.055413 * SinDeg(2.0 * elongationDeg - lunarAnomalyDeg + latitudeArgumentDeg) +
            0.046271 * SinDeg(2.0 * elongationDeg - lunarAnomalyDeg - latitudeArgumentDeg);
        const double longitudeDeg = Wrap360(meanLongitudeDeg + longitudeCorrectionDeg);

        const double longitudeRad = longitudeDeg * kDegToRad;
        const double latitudeRad = latitudeDeg * kDegToRad;
        const double obliquityRad =
            (23.4392911111 - 0.0130041667 * t - 0.0000001639 * t2 + 0.0000005036 * t3) * kDegToRad;
        const double sinLongitude = std::sin(longitudeRad);
        const double cosLongitude = std::cos(longitudeRad);
        const double sinLatitude = std::sin(latitudeRad);
        const double cosLatitude = std::cos(latitudeRad);
        const double sinObliquity = std::sin(obliquityRad);
        const double cosObliquity = std::cos(obliquityRad);

        const double equatorialX = cosLatitude * cosLongitude;
        const double equatorialY = cosLatitude * sinLongitude * cosObliquity - sinLatitude * sinObliquity;
        const double equatorialZ = cosLatitude * sinLongitude * sinObliquity + sinLatitude * cosObliquity;
        const double declinationRad = std::asin(equatorialZ);
        const double rightAscensionDeg = Wrap360(std::atan2(equatorialY, equatorialX) * kRadToDeg);

        const double gmstHours = 18.697374558 + 24.06570982441908 * daysSinceJ2000;
        const double hourAngleDeg =
            Wrap180((gmstHours + observer.longitudeDeg / 15.0) * 15.0 - rightAscensionDeg);
        const double observerLatitudeRad = observer.latitudeDeg * kDegToRad;
        const double hourAngleRad = hourAngleDeg * kDegToRad;
        const double sinObserverLatitude = std::sin(observerLatitudeRad);
        const double cosObserverLatitude = std::cos(observerLatitudeRad);
        const double sinDeclination = std::sin(declinationRad);
        const double cosDeclination = std::cos(declinationRad);
        const double sinHourAngle = std::sin(hourAngleRad);
        const double cosHourAngle = std::cos(hourAngleRad);
        const double sinAltitude =
            sinObserverLatitude * sinDeclination + cosObserverLatitude * cosDeclination * cosHourAngle;
        const double clampedSinAltitude = sinAltitude > 1.0 ? 1.0 : (sinAltitude < -1.0 ? -1.0 : sinAltitude);

        position.declinationDeg = declinationRad * kRadToDeg;
        position.hourAngleDeg = hourAngleDeg;
        position.altitudeDeg = std::asin(clampedSinAltitude) * kRadToDeg;
        position.azimuthDeg = Wrap360(std::atan2(-cosDeclination * sinHourAngle,
                                                 cosObserverLatitude * sinDeclination -
                                                     sinObserverLatitude * cosDeclination * cosHourAngle) *
                                      kRadToDeg);
        position.rightAscensionDeg = rightAscensionDeg;
        position.eclipticLongitudeDeg = longitudeDeg;
        position.eclipticLatitudeDeg = latitudeDeg;
        return position;
    }

    MoonPosition MoonPositionFor(const SimClock& clock) noexcept
    {
        const SunObserver observer{clock.latitudeDeg, clock.longitudeDeg, clock.utcOffsetMinutes};
        return MoonPositionAt(
            DaysSinceJ2000ForEpochSeconds(clock.CivilEpochSeconds(), clock.utcOffsetMinutes), observer);
    }

    MoonDay
    MoonDayFor(const CivilTime& localStandardDate, const SunObserver& observer, double thresholdDeg) noexcept
    {
        MoonDay day;
        if (!std::isfinite(thresholdDeg))
        {
            return day;
        }
        const double dayStartJ2000 = WholeDaysFromJ2000Date(localStandardDate) -
                                     static_cast<double>(observer.utcOffsetMinutes) / kMinutesPerDay - 0.5;
        double previousMinutes = 0.0;
        double previousAltitude = AltitudeAt(dayStartJ2000, previousMinutes, observer);
        for (double minutes = kScanStepMinutes; minutes <= kMinutesPerDay; minutes += kScanStepMinutes)
        {
            const double altitude = AltitudeAt(dayStartJ2000, minutes, observer);
            const bool wasBelow = previousAltitude < thresholdDeg;
            const bool isBelow = altitude < thresholdDeg;
            if (wasBelow != isBelow)
            {
                const double crossing =
                    Bisect(dayStartJ2000, previousMinutes, minutes, thresholdDeg, observer);
                if (wasBelow && !day.rise.occurs)
                {
                    day.rise = MoonEvent{true, crossing};
                }
                else if (!wasBelow && !day.set.occurs)
                {
                    day.set = MoonEvent{true, crossing};
                }
            }
            previousMinutes = minutes;
            previousAltitude = altitude;
        }
        return day;
    }

} // namespace cnahouse::environment
