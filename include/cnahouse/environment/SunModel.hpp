// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/environment/SimClock.hpp"

namespace cnahouse::environment
{

    /// @brief The altitude of the sun's centre at the moment it is said to rise or set.
    ///
    /// Not zero, and the difference is about four minutes of clock time at this latitude. The
    /// conventional figure is the sun's semidiameter (~16') plus mean atmospheric refraction at the
    /// horizon (~34'), so the geometric centre is 50' = 0.8333° BELOW the horizon when the upper
    /// limb appears to touch it. Every published rise/set table uses it, the USNO's included, which
    /// is what makes `tests/unit/reference/suntimes.usno.txt` comparable to this model at all.
    inline constexpr double kRefractedHorizonDeg = -0.8333;

    /// @brief §32.1's twilight thresholds, as altitudes of the sun's centre.
    inline constexpr double kCivilTwilightDeg = -6.0;
    inline constexpr double kNauticalTwilightDeg = -12.0;
    inline constexpr double kAstronomicalTwilightDeg = -18.0;

    /// @brief Where the sun is. §32.1's four outputs, plus the two intermediates worth having.
    ///
    /// Angles in degrees throughout, because every number §32 states is in degrees and converting
    /// at the boundary twice is how a sign error gets in.
    struct SunPosition
    {
        /// @brief Declination δ: how far north of the celestial equator the sun is, ±23.44°.
        double declinationDeg = 0.0;
        /// @brief Hour angle H, normalised to `[-180, 180)`. Negative before local apparent noon,
        ///        zero at it, positive after — so it reads as a clock and not as a compass.
        double hourAngleDeg = 0.0;
        /// @brief Altitude above the true horizon. **Geometric: no refraction is applied**, which
        ///        is why `kRefractedHorizonDeg` and not zero is the rise/set threshold.
        double altitudeDeg = 0.0;
        /// @brief Azimuth measured from NORTH, clockwise through east, `[0, 360)`.
        double azimuthDeg = 0.0;
        /// @brief Right ascension α, `[0, 360)`.
        double rightAscensionDeg = 0.0;
        /// @brief Apparent ecliptic longitude λ, `[0, 360)`.
        double eclipticLongitudeDeg = 0.0;
    };

    /// @brief Where the observer is. Defaults to §33's location.
    struct SunObserver
    {
        double latitudeDeg = kDefaultLatitudeDeg;
        /// @brief East positive, so §33's −75.30 is west.
        double longitudeDeg = kDefaultLongitudeDeg;
        /// @brief The offset the observer's STANDARD clock runs at, with no daylight saving in it.
        int utcOffsetMinutes = kDefaultUtcOffsetMinutes;
    };

    /// @brief §32.1's *"Reykjavik"* preset, which exists *"purely to make the seasonal and diurnal
    ///        extremes easy to test"*.
    ///
    /// 64.13° N, 21.90° W, UTC+0 all year -- Iceland has not kept daylight saving since 1968. At
    /// this latitude the sun's LOWER transit on the June solstice is `lat + 23.44 - 90` = −2.4°, so
    /// it dips below the horizon and never as far as civil twilight: the night is a long dusk. That
    /// single fact exercises a whole branch of `SunDayFor` that §33's location cannot reach.
    [[nodiscard]] inline SunObserver ReykjavikObserver() noexcept
    {
        return SunObserver{64.13, -21.90, 0};
    }

    /// @brief §32.1's *"Equator"* preset: 0° N, 0° E, UTC+0.
    ///
    /// The other extreme, and the one where nothing varies -- twelve-hour days all year and a sun
    /// that passes overhead twice. A seasonal term with the wrong sign is invisible at 40° N for
    /// half the year and is never invisible here.
    [[nodiscard]] inline SunObserver EquatorObserver() noexcept
    {
        return SunObserver{0.0, 0.0, 0};
    }

    /// @brief Days since the J2000.0 epoch, 2000-01-01 12:00 UT. Fractional, negative before it.
    ///
    /// §32.1's `n`, and the only time quantity the position model takes: everything else about a
    /// clock — the compression, daylight saving, which calendar day it is — has been resolved
    /// before this point. @p utc is a civil reading in UNIVERSAL time.
    [[nodiscard]] double DaysSinceJ2000(const CivilTime& utc) noexcept;

    /// @brief The same, for §35.1's epoch seconds at an observer.
    ///
    /// §35.1's epoch counts local STANDARD seconds, so universal time is that plus the observer's
    /// offset removed. Daylight saving is deliberately not consulted: it moves a clock's face and
    /// not the earth.
    [[nodiscard]] double DaysSinceJ2000ForEpochSeconds(double epochSeconds, int utcOffsetMinutes) noexcept;

    /// @brief Local mean sidereal time in degrees, east-positive and wrapped to `[0, 360)`.
    ///
    /// This is the earth-rotation term shared by sun, moon and `StarField`. Keeping one function
    /// prevents a sky whose catalogue slowly drifts away from the two bodies it is drawn beside.
    [[nodiscard]] double LocalSiderealTimeDeg(double daysSinceJ2000, double longitudeDeg) noexcept;

    /// @brief §32.1's solar position model, evaluated at @p daysSinceJ2000 for @p observer.
    ///
    /// ~40 flops, and cheap enough to call once a frame as §32.1 says. The formulation is the
    /// USNO's own low-precision solar coordinates — mean longitude, mean anomaly, the two largest
    /// terms of the equation of centre, then the rotation into equatorial and horizontal
    /// coordinates — and it is stated to hold to about 0.01° in declination across 1950–2050.
    ///
    /// **It is an approximation and the test that guards it says so.** `SunModelTests` compares it
    /// against 390 rise and set times published by the USNO, computed there from a full ephemeris;
    /// the two are different computations and agreeing to within minutes is the claim, not
    /// identity.
    [[nodiscard]] SunPosition SunPositionAt(double daysSinceJ2000, const SunObserver& observer) noexcept;

    /// @brief The sun as @p clock sees it, at the clock's own location.
    [[nodiscard]] SunPosition SunPositionFor(const SimClock& clock) noexcept;

    /// @brief When the sun crosses a threshold altitude on one local standard day.
    ///
    /// Minutes after local standard midnight, fractional. `occurs` is false on a day where the sun
    /// never reaches the threshold at all — which at §33's 40.05° N never happens for rise and set
    /// but does for astronomical twilight in June, and would happen for everything inside a polar
    /// circle. A caller that ignores it gets a number that means nothing.
    struct SunEvent
    {
        bool occurs = false;
        double minutesOfDay = 0.0;
    };

    /// @brief The rise, the upper transit and the set of one local standard day.
    struct SunDay
    {
        SunEvent rise;
        SunEvent set;
        /// @brief Upper transit — local apparent noon. Always occurs; the sun crosses the meridian
        ///        every day whether or not it is above the horizon when it does.
        SunEvent transit;
        /// @brief The altitude at transit, which is the day's maximum.
        double transitAltitudeDeg = 0.0;
    };

    /// @brief The day's solar events for the civil date in @p localStandardDate at @p observer.
    ///
    /// **Solved from `SunPositionAt` and from nothing else**: the crossings are bracketed on a
    /// coarse scan of the day and then bisected on the model's own altitude. There is no second
    /// formula for sunrise here, deliberately — a closed-form hour-angle solution would be a
    /// different approximation, and then a disagreement with the USNO could not be attributed. The
    /// only quantity introduced is the threshold, `kRefractedHorizonDeg`, which is a convention
    /// rather than a computation.
    ///
    /// Costs about 1 500 evaluations of a 40-flop function. That is a per-DAY query and not a
    /// per-frame one; §32.2's lighting reads `SunPositionFor` instead.
    [[nodiscard]] SunDay SunDayFor(const CivilTime& localStandardDate,
                                   const SunObserver& observer,
                                   double thresholdDeg = kRefractedHorizonDeg) noexcept;

    /// @brief How long the sun is above the threshold on @p day, in minutes.
    ///
    /// The two degenerate cases are answered rather than left to the caller: a day with no crossing
    /// is **1 440 minutes** when the transit is above the threshold (midnight sun) and **0** when it
    /// is below (polar night). At §33's 40.05° N neither happens, and a `daylight` term that
    /// silently returned `set − rise` = 0 for the midnight sun would be wrong in the one place
    /// nobody would think to look.
    [[nodiscard]] double DaylightMinutes(const SunDay& day) noexcept;

    /// @brief Daylight minutes at a CONTINUOUS calendar position: §35.2b's compressed year needs it.
    ///
    /// @p calendarDaysSinceEpoch is `SimClock::CalendarDays()` — fractional days since
    /// 2031-01-01. **The fractional part is what this function exists for.** Under the default
    /// compression the calendar crosses a day every real minute of play, so a day length quantised
    /// to whole days would step by up to three minutes sixty times an hour, and §35's whole
    /// argument for a continuous season phase applies here word for word.
    ///
    /// Interpolated linearly between the two bracketing days' `SunDayFor` answers, and
    /// deliberately **not** re-derived from a closed-form hour-angle expression. A closed form
    /// would be a SECOND approximation living beside `SunPositionAt`, and `HOUSE-01561` kept the
    /// model to one on purpose; the day length is very nearly linear across one day, so
    /// interpolating what the model already says costs nothing in accuracy and adds no formula.
    [[nodiscard]] double DaylightMinutesAt(double calendarDaysSinceEpoch,
                                           const SunObserver& observer,
                                           double thresholdDeg = kRefractedHorizonDeg) noexcept;

    /// @brief The daylight length where @p clock currently is, at the clock's own location.
    [[nodiscard]] double DaylightMinutesFor(const SimClock& clock) noexcept;

} // namespace cnahouse::environment
