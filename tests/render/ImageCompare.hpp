// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"

namespace cnahouse::testsupport
{

    /// @brief A decoded frame. Row-major, top-left origin, RGBA8 -- XNA's `Color` layout.
    struct Image
    {
        int width = 0;
        int height = 0;
        std::vector<Microsoft::Xna::Framework::Color> pixels;

        [[nodiscard]] bool Empty() const noexcept
        {
            return width <= 0 || height <= 0 || pixels.empty();
        }
    };

    /// @brief A rectangle of the frame that is legitimately not reproducible.
    ///
    /// **Excluded explicitly, never by widening the tolerance.** The corner frame-time readout
    /// genuinely differs between two runs of the same build, and absorbing it by raising the
    /// per-channel tolerance to 255 would absorb every real regression with it. Naming the rectangle
    /// keeps the rest of the frame strict.
    struct Region
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        [[nodiscard]] bool Contains(int px, int py) const noexcept
        {
            return px >= x && py >= y && px < x + width && py < y + height;
        }
    };

    /// @brief What two frames differ by.
    ///
    /// **Four numbers, not one.** A single "percentage different" hides the distinction that
    /// actually matters: a frame where every pixel is off by one is a driver rounding difference,
    /// and a frame where 0.3 % of pixels are off by 200 is a missing object. `maxChannelDelta` and
    /// `differingPixels` separate those; a test that only had the mean would pass the second.
    struct ImageDiff
    {
        /// @brief True when the two frames are not even the same size. Nothing else is meaningful.
        bool sizeMismatch = false;
        std::size_t comparedPixels = 0;
        /// @brief Pixels where any channel differs by more than the tolerance.
        std::size_t differingPixels = 0;
        /// @brief The largest single-channel difference anywhere, 0..255.
        int maxChannelDelta = 0;
        /// @brief Mean absolute channel difference across every channel of every pixel.
        double meanChannelDelta = 0.0;

        [[nodiscard]] double DifferingFraction() const noexcept
        {
            return comparedPixels == 0
                       ? 0.0
                       : static_cast<double>(differingPixels) / static_cast<double>(comparedPixels);
        }

        [[nodiscard]] std::string ToString() const;
    };

    /// @brief Compares @p actual against @p expected.
    ///
    /// @param channelTolerance a per-channel difference at or below this is not counted as a
    ///        differing pixel. **It is not slack to be widened when a test fails**: the reference
    ///        images are produced under the same software rasteriser CI uses
    ///        (`LIBGL_ALWAYS_SOFTWARE=1`, `HOUSE-00138`), so a real difference is a real difference.
    ///        The tolerance exists for Mesa version drift, not for hardware variation.
    [[nodiscard]] inline ImageDiff Compare(const Image& actual,
                                           const Image& expected,
                                           int channelTolerance,
                                           const std::vector<Region>& ignore = {});

    /// @brief Builds a diagnostic frame: changed pixels are magenta, stable pixels are dimmed.
    ///
    /// This is an artefact, not another comparison rule. It uses the exact same channel tolerance
    /// and ignored regions as `Compare`, so the image points at precisely the pixels counted in the
    /// failure message. Ignored pixels are transparent.
    [[nodiscard]] inline Image DifferenceImage(const Image& actual,
                                               const Image& expected,
                                               int channelTolerance,
                                               const std::vector<Region>& ignore = {});

    namespace detail
    {
        inline int Delta(std::uint8_t a, std::uint8_t b) noexcept
        {
            return std::abs(static_cast<int>(a) - static_cast<int>(b));
        }
    } // namespace detail

    inline std::string ImageDiff::ToString() const
    {
        if (sizeMismatch)
        {
            return "the two frames are different sizes";
        }
        return std::format("{} of {} pixels differ ({:.4f}%), max channel delta {}, mean {:.3f}",
                           differingPixels,
                           comparedPixels,
                           DifferingFraction() * 100.0,
                           maxChannelDelta,
                           meanChannelDelta);
    }

    inline ImageDiff Compare(const Image& actual,
                             const Image& expected,
                             int channelTolerance,
                             const std::vector<Region>& ignore)
    {
        ImageDiff diff;
        if (actual.width != expected.width || actual.height != expected.height ||
            actual.pixels.size() != expected.pixels.size())
        {
            diff.sizeMismatch = true;
            return diff;
        }

        double total = 0.0;
        for (std::size_t i = 0; i < actual.pixels.size(); ++i)
        {
            const int px = static_cast<int>(i % static_cast<std::size_t>(actual.width));
            const int py = static_cast<int>(i / static_cast<std::size_t>(actual.width));
            bool skipped = false;
            for (const Region& region : ignore)
            {
                if (region.Contains(px, py))
                {
                    skipped = true;
                    break;
                }
            }
            if (skipped)
            {
                // Not counted in `comparedPixels` either, so `DifferingFraction` stays a fraction of
                // what was actually looked at.
                continue;
            }
            ++diff.comparedPixels;

            const auto& a = actual.pixels[i];
            const auto& e = expected.pixels[i];
            // ALPHA is compared too. A frame that lost its alpha channel looks identical in a
            // viewer that ignores it and is wrong everywhere it is composited.
            const int deltas[4] = {detail::Delta(a.getRProperty(), e.getRProperty()),
                                   detail::Delta(a.getGProperty(), e.getGProperty()),
                                   detail::Delta(a.getBProperty(), e.getBProperty()),
                                   detail::Delta(a.getAProperty(), e.getAProperty())};
            int worst = 0;
            for (const int d : deltas)
            {
                worst = std::max(worst, d);
                total += static_cast<double>(d);
            }
            diff.maxChannelDelta = std::max(diff.maxChannelDelta, worst);
            if (worst > channelTolerance)
            {
                ++diff.differingPixels;
            }
        }
        // Guarded: an empty pair would otherwise produce 0.0/0.0, and a NaN propagates through
        // every assertion that touches it while reading as "not greater than the threshold".
        diff.meanChannelDelta =
            diff.comparedPixels == 0 ? 0.0 : total / (static_cast<double>(diff.comparedPixels) * 4.0);
        return diff;
    }

    inline Image DifferenceImage(const Image& actual,
                                 const Image& expected,
                                 int channelTolerance,
                                 const std::vector<Region>& ignore)
    {
        Image image;
        if (actual.width != expected.width || actual.height != expected.height ||
            actual.pixels.size() != expected.pixels.size())
        {
            return image;
        }

        image.width = actual.width;
        image.height = actual.height;
        image.pixels.reserve(expected.pixels.size());
        for (std::size_t i = 0; i < expected.pixels.size(); ++i)
        {
            const int px = static_cast<int>(i % static_cast<std::size_t>(image.width));
            const int py = static_cast<int>(i / static_cast<std::size_t>(image.width));
            const bool skipped = std::ranges::any_of(
                ignore, [px, py](const Region& region) { return region.Contains(px, py); });
            if (skipped)
            {
                image.pixels.emplace_back(0, 0, 0, 0);
                continue;
            }

            const auto& a = actual.pixels[i];
            const auto& e = expected.pixels[i];
            const int worst = std::max({detail::Delta(a.getRProperty(), e.getRProperty()),
                                        detail::Delta(a.getGProperty(), e.getGProperty()),
                                        detail::Delta(a.getBProperty(), e.getBProperty()),
                                        detail::Delta(a.getAProperty(), e.getAProperty())});
            if (worst > channelTolerance)
            {
                image.pixels.emplace_back(255, 0, 255, 255);
            }
            else
            {
                image.pixels.emplace_back(static_cast<int>(e.getRProperty()) / 4,
                                          static_cast<int>(e.getGProperty()) / 4,
                                          static_cast<int>(e.getBProperty()) / 4,
                                          255);
            }
        }
        return image;
    }

} // namespace cnahouse::testsupport
