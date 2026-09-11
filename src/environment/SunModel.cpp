// SPDX-License-Identifier: MIT
#include "cnahouse/environment/SunModel.hpp"

#include <cmath>

namespace cnahouse::environment
{
    namespace
    {
        constexpr double kDegToRad = 0.017453292519943295769236907684886;
        constexpr double kRadToDeg = 57.295779513082320876798154814105;

        /// @brief @p degrees wrapped into `[0, 360)`. `fmod` alone leaves a negative negative.
        [[nodiscard]] double Wrap360(double degrees) noexcept
        {
            const double wrapped = std::fmod(degrees, 360.0);
            return wrapped < 0.0 ? wrapped + 360.0 : wrapped;
        }

        /// @brief @p degrees wrapped into `[-180, 180)`, which is how an hour angle reads.
        [[nodiscard]] double Wrap180(double degrees) noexcept
        {
            const double wrapped = Wrap360(degrees + 180.0);
            return wrapped - 180.0;
        }

        /// @brief Days from 2000-01-01 to the civil date in @p time, ignoring its time of day.
        [[nodiscard]] double WholeDaysFromJ2000Date(const CivilTime& time) noexcept
        {
            return static_cast<double>(DaysFromCivil(time.year, time.month, time.day) -
                                       DaysFromCivil(2000, 1, 1));
        }

        constexpr double kMinutesPerDay = 1440.0;

        /// @brief The coarse scan's step, in minutes.
        ///
        /// The sun's altitude changes fastest at the equinoxes at this latitude, and even there by
        /// under 0.2° a minute — so a four-minute step moves it by well under a degree and cannot
        /// step over a threshold crossing and back. Small enough to be safe, coarse enough that a
        /// day is 360 evaluations rather than 1 440.
        constexpr double kScanStepMinutes = 4.0;

        /// @brief Bisection steps. A day halved 44 times is under a microsecond; 40 is ample and
        ///        the loop is bounded rather than trusting a tolerance to be reachable in double.
        constexpr int kBisectionSteps = 40;

        /// @brief The altitude at @p minutesOfDay on the day starting at @p dayStartJ2000.
        [[nodiscard]] double
        AltitudeAt(double dayStartJ2000, double minutesOfDay, const SunObserver& observer) noexcept
        {
            return SunPositionAt(dayStartJ2000 + minutesOfDay / kMinutesPerDay, observer).altitudeDeg;
        }

        /// @brief The instant in `[lo, hi]` where altitude − @p thresholdDeg changes sign.
        ///
        /// The caller has already established that it does; this only narrows it.
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
                const double midValue = AltitudeAt(dayStartJ2000, mid, observer) - thresholdDeg;
                if ((midValue < 0.0) == (loSign < 0.0))
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

    double DaysSinceJ2000(const CivilTime& utc) noexcept
    {
        const double dayFraction =
            (static_cast<double>(utc.hour) * 3600.0 + static_cast<double>(utc.minute) * 60.0 +
             static_cast<double>(utc.second)) /
            kSecondsPerDay;
        // J2000.0 is NOON, so a date's midnight is half a day before the epoch's own time of day.
        return WholeDaysFromJ2000Date(utc) + dayFraction - 0.5;
    }

    double DaysSinceJ2000ForEpochSeconds(double epochSeconds, int utcOffsetMinutes) noexcept
    {
        if (!std::isfinite(epochSeconds))
        {
            return 0.0;
        }
        // §35.1's epoch is 2031-01-01T00:00:00 in local STANDARD time. Universal time at that
        // instant is later by the offset's negation: at UTC−5 the epoch is 05:00 UT.
        const double universalSeconds = epochSeconds - static_cast<double>(utcOffsetMinutes) * 60.0;
        const double epochDayFromJ2000 =
            static_cast<double>(DaysFromCivil(2031, 1, 1) - DaysFromCivil(2000, 1, 1));
        return epochDayFromJ2000 + universalSeconds / kSecondsPerDay - 0.5;
    }

    SunPosition SunPositionAt(double daysSinceJ2000, const SunObserver& observer) noexcept
    {
        SunPosition position;
        if (!std::isfinite(daysSinceJ2000) || !std::isfinite(observer.latitudeDeg) ||
            !std::isfinite(observer.longitudeDeg))
        {
            return position;
        }
        const double n = daysSinceJ2000;

        // §32.1, verbatim. L is the mean longitude, g the mean anomaly, and the two sine terms are
        // the equation of centre truncated where the next term is under an arcminute.
        const double meanLongitudeDeg = Wrap360(280.460 + 0.9856474 * n);
        const double meanAnomalyRad = Wrap360(357.528 + 0.9856003 * n) * kDegToRad;
        const double lambdaDeg = Wrap360(meanLongitudeDeg + 1.915 * std::sin(meanAnomalyRad) +
                                         0.020 * std::sin(2.0 * meanAnomalyRad));
        const double obliquityRad = (23.439 - 0.0000004 * n) * kDegToRad;

        const double lambdaRad = lambdaDeg * kDegToRad;
        const double sinLambda = std::sin(lambdaRad);
        const double cosLambda = std::cos(lambdaRad);
        const double sinObliquity = std::sin(obliquityRad);
        const double cosObliquity = std::cos(obliquityRad);

        const double declinationRad = std::asin(sinObliquity * sinLambda);
        const double rightAscensionDeg = Wrap360(std::atan2(cosObliquity * sinLambda, cosLambda) * kRadToDeg);

        // GMST in HOURS, so the hour angle is assembled in hours and converted once. The longitude
        // enters as itself because east is positive here and §32.1's `longitude/15` is east-positive
        // too — at §33's −75.30 the local meridian is five hours behind Greenwich.
        const double gmstHours = 18.697374558 + 24.06570982441908 * n;
        const double hourAngleDeg =
            Wrap180((gmstHours + observer.longitudeDeg / 15.0) * 15.0 - rightAscensionDeg);

        const double latitudeRad = observer.latitudeDeg * kDegToRad;
        const double hourAngleRad = hourAngleDeg * kDegToRad;
        const double sinLatitude = std::sin(latitudeRad);
        const double cosLatitude = std::cos(latitudeRad);
        const double sinDeclination = std::sin(declinationRad);
        const double cosDeclination = std::cos(declinationRad);
        const double sinHourAngle = std::sin(hourAngleRad);
        const double cosHourAngle = std::cos(hourAngleRad);

        const double sinAltitude = sinLatitude * sinDeclination + cosLatitude * cosDeclination * cosHourAngle;
        // The argument can leave [-1, 1] by an ulp at the poles of the expression; asin of that is
        // a NaN that would propagate into the lighting.
        const double clampedSinAltitude = sinAltitude > 1.0 ? 1.0 : (sinAltitude < -1.0 ? -1.0 : sinAltitude);

        // §32.1's azimuth, written with cos δ multiplied through rather than `tan δ`: the two are
        // the same angle and this one has no pole at δ = ±90°.
        const double azimuthDeg =
            Wrap360(std::atan2(-cosDeclination * sinHourAngle,
                               cosLatitude * sinDeclination - sinLatitude * cosDeclination * cosHourAngle) *
                    kRadToDeg);

        position.declinationDeg = declinationRad * kRadToDeg;
        position.hourAngleDeg = hourAngleDeg;
        position.altitudeDeg = std::asin(clampedSinAltitude) * kRadToDeg;
        position.azimuthDeg = azimuthDeg;
        position.rightAscensionDeg = rightAscensionDeg;
        position.eclipticLongitudeDeg = lambdaDeg;
        return position;
    }

    SunPosition SunPositionFor(const SimClock& clock) noexcept
    {
        const SunObserver observer{clock.latitudeDeg, clock.longitudeDeg, clock.utcOffsetMinutes};
        // The CIVIL instant, so §35.2b's compressed date is where the declination is read from —
        // the season the player is in, not the number of times the sun has come up.
        return SunPositionAt(DaysSinceJ2000ForEpochSeconds(clock.CivilEpochSeconds(), clock.utcOffsetMinutes),
                             observer);
    }

    SunDay
    SunDayFor(const CivilTime& localStandardDate, const SunObserver& observer, double thresholdDeg) noexcept
    {
        SunDay day;
        if (!std::isfinite(thresholdDeg))
        {
            return day;
        }
        // Local standard midnight of that date, as days since J2000.
        // Midnight local standard is `-utcOffset` minutes into that UT date: at UTC-5, 05:00 UT.
        const double dayStartJ2000 = WholeDaysFromJ2000Date(localStandardDate) -
                                     static_cast<double>(observer.utcOffsetMinutes) / kMinutesPerDay - 0.5;

        double previousMinutes = 0.0;
        double previousAltitude = AltitudeAt(dayStartJ2000, 0.0, observer);
        double bestAltitude = previousAltitude;
        double bestMinutes = 0.0;

        for (double minutes = kScanStepMinutes; minutes <= kMinutesPerDay; minutes += kScanStepMinutes)
        {
            const double altitude = AltitudeAt(dayStartJ2000, minutes, observer);
            if (altitude > bestAltitude)
            {
                bestAltitude = altitude;
                bestMinutes = minutes;
            }
            const bool wasBelow = previousAltitude < thresholdDeg;
            const bool isBelow = altitude < thresholdDeg;
            if (wasBelow != isBelow)
            {
                const double crossing =
                    Bisect(dayStartJ2000, previousMinutes, minutes, thresholdDeg, observer);
                // Rising when the sun was below and now is not. A day has at most one of each at
                // this latitude, and taking the FIRST of each is what a published table means by
                // "sunrise" on a day that happens to have two.
                if (wasBelow && !day.rise.occurs)
                {
                    day.rise = SunEvent{true, crossing};
                }
                else if (!wasBelow && !day.set.occurs)
                {
                    day.set = SunEvent{true, crossing};
                }
            }
            previousMinutes = minutes;
            previousAltitude = altitude;
        }

        // The transit, refined off the scan's maximum. The altitude is smooth and unimodal about
        // it, so a ternary search on the bracket either side converges without derivatives.
        double lo = bestMinutes - kScanStepMinutes;
        double hi = bestMinutes + kScanStepMinutes;
        for (int step = 0; step < kBisectionSteps; ++step)
        {
            const double third = (hi - lo) / 3.0;
            const double a = lo + third;
            const double b = hi - third;
            if (AltitudeAt(dayStartJ2000, a, observer) < AltitudeAt(dayStartJ2000, b, observer))
            {
                lo = a;
            }
            else
            {
                hi = b;
            }
        }
        const double transitMinutes = 0.5 * (lo + hi);
        day.transit = SunEvent{true, transitMinutes};
        day.transitAltitudeDeg = AltitudeAt(dayStartJ2000, transitMinutes, observer);
        return day;
    }

} // namespace cnahouse::environment
