// SPDX-License-Identifier: MIT
#pragma once

namespace cnahouse::environment
{

    /// @brief §36.2's seasonal base temperature curve, in °C (`HOUSE-01535`).
    ///
    /// `base(doy, h) = annualMean + annualAmp·cos(2π(doy−201)/365) + diurnalAmp·cos(2π(h−15)/24)`
    ///
    /// **This is the base, not the weather.** §36.2 applies each archetype's `Δtemp` on top of it,
    /// and §36.1's `temperatureC` runs −18…38 °C where this alone spans −7…29. What the base is
    /// for is that everything seasonal falls out of it without a special case: `precipType` is
    /// DERIVED from the temperature -- above 2.5 °C rain, below 0 snow, between them sleet -- so
    /// *"it snows in January and rains in July without any special-casing, and a spring storm can
    /// produce sleet"*. §36.3's *"`W_SNOW` has probability zero in summer"* is the same sentence:
    /// snow in July is impossible by construction rather than by a rule that forbids it.
    /// @{

    /// @brief The annual mean for §33's location, °C.
    ///
    /// The three constants are stated by §36.2 *"for the default location"* -- suburban
    /// Philadelphia -- and are not derived from `SimClock::latitudeDeg`. Deriving them would be
    /// inventing a climate model the design does not ask for and cannot check; moving the house
    /// means measuring three new numbers, and this is where they would go.
    inline constexpr double kAnnualMeanC = 11.0;

    /// @brief Half the annual swing, °C. The year runs 11 ± 12.
    inline constexpr double kAnnualAmplitudeC = 12.0;

    /// @brief Half the daily swing, °C. Smaller than the annual one, which is what makes a January
    ///        afternoon colder than a July night.
    inline constexpr double kDiurnalAmplitudeC = 6.0;

    /// @brief The day of the year the curve peaks: 20 July, three weeks after the solstice.
    ///
    /// Seasonal lag, and it is in §36.2's formula rather than being an approximation of it: the
    /// ground and the air keep warming after the sun has begun to go back, which is why the hottest
    /// week of the year is not the longest day.
    inline constexpr double kWarmestDayOfYear = 201.0;

    /// @brief The hour the curve peaks: 15:00, not noon, for the same reason at a day's scale.
    inline constexpr double kWarmestHourOfDay = 15.0;

    /// @brief The length of the year the annual term uses. §36.2's own 365, so that the curve is
    ///        the formula the design states rather than a leap-aware improvement on it.
    inline constexpr double kAnnualPeriodDays = 365.0;

    /// @}

    /// @brief §36.2's curve at @p dayOfYear (1-based, may be fractional) and @p hourOfDay (0-24).
    ///
    /// Both arguments are periodic, so a caller need not wrap them: day 366 is day 1 and hour 25
    /// is hour 1. Non-finite arguments give the annual mean, because a temperature of NaN would
    /// reach `precipType`, `snowDepth` and the furnace's thermostat within a frame.
    [[nodiscard]] double BaseTemperatureC(double dayOfYear, double hourOfDay) noexcept;

    /// @brief The diurnal part of §36.2's curve, in °C relative to the seasonal base.
    [[nodiscard]] double DiurnalTemperatureDeltaC(double hourOfDay) noexcept;

    /// @brief The one outdoor temperature that weather gating and player-facing UI consume.
    ///
    /// @p weatherDeltaC is the weather system's live, continuously interpolated `Δtemp`; this
    /// function adds it once to the analytic seasonal and diurnal curve. Until the weather system
    /// exists, pass zero. A non-finite delta is treated as zero so NaN cannot enter snow,
    /// precipitation, HVAC or audio decisions.
    [[nodiscard]] double
    OutdoorTemperatureC(double dayOfYear, double hourOfDay, double weatherDeltaC) noexcept;

} // namespace cnahouse::environment
