// SPDX-License-Identifier: MIT
#include "cnahouse/lighting/PlanckianLut.hpp"

#include <algorithm>
#include <cmath>

namespace cnahouse::lighting
{
    namespace
    {
        using Microsoft::Xna::Framework::Vector3;

        [[nodiscard]] float UnitChannel(double byteValue) noexcept
        {
            return static_cast<float>(std::clamp(byteValue, 0.0, 255.0) / 255.0);
        }

        /// @brief Tanner Helland's compact black-body-to-display-RGB approximation.
        ///
        /// It is evaluated only while constructing the LUT, never in the per-frame path. The
        /// result is display RGB with its strongest channel normalised to one; luminous flux is a
        /// separate quantity in §28 and must not be folded into the hue.
        [[nodiscard]] Vector3 Approximate(float kelvin) noexcept
        {
            const double temperature = static_cast<double>(kelvin) / 100.0;
            double red = 255.0;
            double green = 0.0;
            double blue = 255.0;
            if (temperature <= 66.0)
            {
                green = 99.4708025861 * std::log(temperature) - 161.1195681661;
                blue = temperature <= 19.0 ? 0.0
                                           : 138.5177312231 * std::log(temperature - 10.0) - 305.0447927307;
            }
            else
            {
                red = 329.698727446 * std::pow(temperature - 60.0, -0.1332047592);
                green = 288.1221695283 * std::pow(temperature - 60.0, -0.0755148492);
            }
            return Vector3(UnitChannel(red), UnitChannel(green), UnitChannel(blue));
        }

        [[nodiscard]] const std::array<Vector3, kPlanckianLutSize>& Table() noexcept
        {
            static const std::array<Vector3, kPlanckianLutSize> table = []
            {
                std::array<Vector3, kPlanckianLutSize> built{};
                for (std::size_t index = 0; index < built.size(); ++index)
                {
                    built[index] = Approximate(kPlanckianLowestKelvin +
                                               static_cast<float>(index) * kPlanckianLutStepKelvin);
                }
                return built;
            }();
            return table;
        }

        [[nodiscard]] float Lerp(float from, float to, float amount) noexcept
        {
            return from + (to - from) * amount;
        }

    } // namespace

    Vector3 PlanckianRgb(float kelvin) noexcept
    {
        const auto& table = Table();
        if (!std::isfinite(kelvin))
        {
            constexpr std::size_t neutralIndex = 55;
            static_assert(kPlanckianLowestKelvin +
                              static_cast<float>(neutralIndex) * kPlanckianLutStepKelvin ==
                          6500.0F);
            return table[neutralIndex];
        }
        if (kelvin <= kPlanckianLowestKelvin)
        {
            return table.front();
        }
        if (kelvin >= kPlanckianHighestKelvin)
        {
            return table.back();
        }

        const float position = (kelvin - kPlanckianLowestKelvin) / kPlanckianLutStepKelvin;
        const auto index = static_cast<std::size_t>(position);
        const std::size_t next = index + 1 < table.size() ? index + 1 : index;
        const float amount = position - static_cast<float>(index);
        return Vector3(Lerp(table[index].X, table[next].X, amount),
                       Lerp(table[index].Y, table[next].Y, amount),
                       Lerp(table[index].Z, table[next].Z, amount));
    }

    const std::array<Vector3, kPlanckianLutSize>& PlanckianRgbTable() noexcept
    {
        return Table();
    }

} // namespace cnahouse::lighting
