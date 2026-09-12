// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>

#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace cnahouse::lighting
{

    /// @brief The validated colour-temperature range of `layout.lights.json` (§70.5).
    inline constexpr float kPlanckianLowestKelvin = 1000.0F;
    inline constexpr float kPlanckianHighestKelvin = 12000.0F;

    /// @brief One entry per 100 K, including both endpoints.
    inline constexpr float kPlanckianLutStepKelvin = 100.0F;
    inline constexpr std::size_t kPlanckianLutSize = 111;

    /// @brief Converts a black-body colour temperature to normalised display RGB.
    ///
    /// The 111-entry table is built once from the standard piecewise logarithmic/power
    /// approximation and every call after that is one lookup plus a lerp. Values between entries
    /// are continuous; finite values outside the authored range clamp to its ends. A non-finite
    /// value returns the neutral 6500 K entry so a corrupt runtime value cannot become a NaN in an
    /// XNA effect.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 PlanckianRgb(float kelvin) noexcept;

    /// @brief The table itself, for diagnostics and tests that inspect the whole colour curve.
    [[nodiscard]] const std::array<Microsoft::Xna::Framework::Vector3, kPlanckianLutSize>&
    PlanckianRgbTable() noexcept;

} // namespace cnahouse::lighting
