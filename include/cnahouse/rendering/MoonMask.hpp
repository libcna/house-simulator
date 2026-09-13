// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Microsoft/Xna/Framework/Color.hpp"

namespace cnahouse::rendering
{

    inline constexpr int kMoonMaskSize = 128;
    inline constexpr std::size_t kMoonMaskTexelCount =
        static_cast<std::size_t>(kMoonMaskSize) * static_cast<std::size_t>(kMoonMaskSize);
    inline constexpr double kMoonMaskPhaseStep = 1.0 / static_cast<double>(kMoonMaskSize);

    using MoonMaskPixels = std::array<Microsoft::Xna::Framework::Color, kMoonMaskTexelCount>;

    /// @brief Build §33.3's 128² RGBA illumination mask at texel centres.
    ///
    /// @p phase is circular (zero new moon, 0.5 full moon) and finite values are wrapped into
    /// `[0,1)`. @p rotationRadians rotates the canonical waxing-right/waning-left mask
    /// counter-clockwise in mask space; this is where the moon pass supplies the bright limb's
    /// position angle. Invalid inputs fail closed to zero.
    [[nodiscard]] MoonMaskPixels GenerateMoonMask(double phase, double rotationRadians) noexcept;

    /// @brief Retains the CPU mask and enforces §33.3's phase-change regeneration threshold.
    ///
    /// Rotation is sampled when a phase update is due. It deliberately cannot cause an upload by
    /// itself: the architectural budget says regeneration occurs only after phase has moved by
    /// more than one mask texel, measured circularly from the last generated phase rather than
    /// from the previous frame.
    class MoonMask final
    {
    public:
        [[nodiscard]] bool Update(double phase, double rotationRadians) noexcept;

        [[nodiscard]] bool HasPixels() const noexcept
        {
            return hasPixels_;
        }

        [[nodiscard]] std::span<const Microsoft::Xna::Framework::Color> Pixels() const noexcept
        {
            return pixels_;
        }

        [[nodiscard]] double GeneratedPhase() const noexcept
        {
            return generatedPhase_;
        }

        [[nodiscard]] std::uint64_t GenerationCount() const noexcept
        {
            return generationCount_;
        }

    private:
        MoonMaskPixels pixels_{};
        double generatedPhase_ = 0.0;
        bool hasPixels_ = false;
        std::uint64_t generationCount_ = 0;
    };

} // namespace cnahouse::rendering
