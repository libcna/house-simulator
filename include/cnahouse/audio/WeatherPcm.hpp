// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "cnahouse/util/Result.hpp"

namespace cnahouse::audio
{
    /// Only the stored PCM weather loops: XNA exposes neither samples nor a public low-pass.
    struct WeatherPcm
    {
        std::vector<std::uint8_t> samples;
        int sampleRate = 0;
        int channels = 0;
        int loopStart = 0;
        int loopLength = 0;

        [[nodiscard]] static util::Result<WeatherPcm> Decode(std::span<const std::uint8_t> cnb);
        [[nodiscard]] static util::Result<WeatherPcm> ReadFromTitle(std::string_view path);
        /// In-place 900 Hz Butterworth low-pass, once at load, with independent stereo state.
        /// Optional loop-level matching separates muffling from the director's shelter attenuation.
        /// The returned gain is bounded to 8 and by 90% full-scale headroom; silence stays silent.
        float LowPass(bool preserveLoopLevel = false);
    };
} // namespace cnahouse::audio
