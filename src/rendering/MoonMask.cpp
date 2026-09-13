// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/MoonMask.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace cnahouse::rendering
{
    namespace Xna = Microsoft::Xna::Framework;

    namespace
    {
        [[nodiscard]] double WrapPhase(double phase) noexcept
        {
            if (!std::isfinite(phase))
            {
                return 0.0;
            }
            return phase - std::floor(phase);
        }

        [[nodiscard]] double CircularPhaseDistance(double a, double b) noexcept
        {
            const double direct = std::abs(a - b);
            return std::min(direct, 1.0 - direct);
        }

        [[nodiscard]] Xna::Color MaskColour(bool lit) noexcept
        {
            // The unlit disc is opaque so it continues to mask the square albedo texture. Its
            // blue-grey channels are 3.1%, 3.5%, 3.9% of full scale: §33.3's 4% earthshine,
            // rounded down by the deliberately cool tint. Outside the lunar disc is transparent.
            return lit ? Xna::Color(255, 255, 255, 255) : Xna::Color(8, 9, 10, 255);
        }
    } // namespace

    MoonMaskPixels GenerateMoonMask(double phase, double rotationRadians) noexcept
    {
        MoonMaskPixels result{};
        const double wrappedPhase = WrapPhase(phase);
        const double rotation = std::isfinite(rotationRadians) ? rotationRadians : 0.0;
        const double cosine = std::cos(rotation);
        const double sine = std::sin(rotation);
        const double terminator = std::cos(2.0 * std::numbers::pi * wrappedPhase);
        const bool waxing = wrappedPhase < 0.5;

        for (int y = 0; y < kMoonMaskSize; ++y)
        {
            for (int x = 0; x < kMoonMaskSize; ++x)
            {
                const double u = (static_cast<double>(x) + 0.5) * (2.0 / kMoonMaskSize) - 1.0;
                const double v = 1.0 - (static_cast<double>(y) + 0.5) * (2.0 / kMoonMaskSize);

                // Inverse-rotate the target texel into the canonical mask. This rotates the
                // resulting mask counter-clockwise by `rotation` without a second resampling pass.
                const double canonicalU = cosine * u + sine * v;
                const double canonicalV = -sine * u + cosine * v;
                const double radiusSquared = canonicalU * canonicalU + canonicalV * canonicalV;
                const std::size_t index = static_cast<std::size_t>(y * kMoonMaskSize + x);
                if (radiusSquared > 1.0)
                {
                    result[index] = Xna::Color(0, 0, 0, 0);
                    continue;
                }

                const double halfChord = std::sqrt(std::max(0.0, 1.0 - canonicalV * canonicalV));
                const double boundary = (waxing ? terminator : -terminator) * halfChord;
                const bool lit = waxing ? canonicalU >= boundary : canonicalU <= boundary;
                result[index] = MaskColour(lit);
            }
        }
        return result;
    }

    bool MoonMask::Update(double phase, double rotationRadians) noexcept
    {
        const double wrappedPhase = WrapPhase(phase);
        if (hasPixels_ && CircularPhaseDistance(wrappedPhase, generatedPhase_) <= kMoonMaskPhaseStep)
        {
            return false;
        }
        pixels_ = GenerateMoonMask(wrappedPhase, rotationRadians);
        generatedPhase_ = wrappedPhase;
        hasPixels_ = true;
        ++generationCount_;
        return true;
    }

} // namespace cnahouse::rendering
