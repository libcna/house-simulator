// SPDX-License-Identifier: MIT
#include "cnahouse/audio/WeatherPcm.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include "Microsoft/Xna/Framework/TitleContainer.hpp"
#include "System/IO/BinaryReader.hpp"
#include "System/IO/Stream.hpp"

namespace cnahouse::audio
{
    namespace
    {
        constexpr std::size_t kMaxBytes = 8U * 1024U * 1024U;

        std::uint32_t Crc(std::span<const std::uint8_t> bytes)
        {
            static constexpr auto table = []
            {
                std::array<std::uint32_t, 256> values{};
                for (std::uint32_t index = 0; index < values.size(); ++index)
                {
                    auto value = index;
                    for (int bit = 0; bit < 8; ++bit)
                    {
                        value = (value >> 1U) ^ ((value & 1U) != 0U ? 0x82F63B78U : 0U);
                    }
                    values[index] = value;
                }
                return values;
            }();
            std::uint32_t value = 0xFFFFFFFFU;
            for (const auto byte : bytes)
            {
                value = table[(value ^ byte) & 255U] ^ (value >> 8U);
            }
            return value ^ 0xFFFFFFFFU;
        }

        std::uint64_t Integer(std::span<const std::uint8_t> bytes, std::size_t at, std::size_t size)
        {
            if (at > bytes.size() || size > bytes.size() - at)
            {
                throw std::runtime_error("truncated weather PCM");
            }
            std::uint64_t value = 0;
            for (std::size_t byte = 0; byte < size; ++byte)
            {
                value |= static_cast<std::uint64_t>(bytes[at + byte]) << (byte * 8U);
            }
            return value;
        }
    } // namespace

    util::Result<WeatherPcm> WeatherPcm::Decode(std::span<const std::uint8_t> cnb)
    {
        try
        {
            if (cnb.size() < 64U || cnb.size() > kMaxBytes || Integer(cnb, 0, 4) != 0x1A424E43U ||
                Integer(cnb, 4, 2) != 1U || Integer(cnb, 6, 2) != 0U || Integer(cnb, 8, 4) != 0U ||
                Integer(cnb, 12, 4) != 8U || Integer(cnb, 16, 4) < 1U || Integer(cnb, 16, 4) > 2U ||
                Integer(cnb, 44, 4) != Crc(cnb.first(44)))
            {
                throw std::runtime_error("unsupported or corrupt weather PCM header");
            }
            for (std::size_t index = 48; index < 64; ++index)
            {
                if (cnb[index] != 0U)
                {
                    throw std::runtime_error("weather PCM reserved header bytes");
                }
            }
            const auto count = Integer(cnb, 20, 4);
            const auto toc = Integer(cnb, 32, 8);
            if (count != 3U || toc < 64U || toc > cnb.size() || count * 48U > cnb.size() - toc ||
                Integer(cnb, 24, 8) != cnb.size())
            {
                throw std::runtime_error("invalid weather PCM table bounds");
            }
            const auto table =
                cnb.subspan(static_cast<std::size_t>(toc), static_cast<std::size_t>(count * 48U));
            if (Integer(cnb, 40, 4) != Crc(table))
            {
                throw std::runtime_error("weather PCM table checksum");
            }
            std::span<const std::uint8_t> header;
            std::span<const std::uint8_t> data;
            std::array<std::pair<std::uint64_t, std::uint64_t>, 3> ranges{};
            bool metadata = false;
            for (std::size_t index = 0; index < count; ++index)
            {
                const auto row = table.subspan(index * 48U, 48U);
                const auto kind = Integer(row, 0, 4);
                const auto offset = Integer(row, 8, 8);
                const auto length = Integer(row, 16, 8);
                const auto alignment = Integer(row, 40, 4);
                if (Integer(row, 4, 4) > 1U || Integer(row, 36, 4) != 0U || Integer(row, 44, 4) != 0U ||
                    alignment == 0U || alignment > 4096U || (alignment & (alignment - 1U)) != 0U ||
                    offset % alignment != 0U || offset < toc + count * 48U || offset > cnb.size() ||
                    length > cnb.size() - offset || length != Integer(row, 24, 8))
                {
                    throw std::runtime_error("unsupported weather PCM chunk or invalid bounds");
                }
                for (std::size_t prior = 0; prior < index; ++prior)
                {
                    if (offset < ranges[prior].second && ranges[prior].first < offset + length)
                    {
                        throw std::runtime_error("overlapping weather PCM chunks");
                    }
                }
                ranges[index] = {offset, offset + length};
                const auto payload =
                    cnb.subspan(static_cast<std::size_t>(offset), static_cast<std::size_t>(length));
                if (Integer(row, 32, 4) != Crc(payload))
                {
                    throw std::runtime_error("weather PCM chunk checksum");
                }
                if (kind == 0x48445541U && header.empty())
                {
                    header = payload;
                }
                else if (kind == 0x44445541U && data.empty())
                {
                    data = payload;
                }
                else if (kind == 0x54454D43U && !metadata)
                {
                    metadata = true;
                }
                else
                {
                    throw std::runtime_error("unexpected or duplicate weather PCM chunk");
                }
            }
            if (header.size() != 28U || data.empty() || !metadata)
            {
                throw std::runtime_error("missing weather PCM chunks");
            }
            const auto format = Integer(header, 0, 4);
            const auto rate = Integer(header, 4, 4);
            const auto channels = Integer(header, 8, 4);
            const auto frames = Integer(header, 12, 4);
            const auto start = Integer(header, 16, 4);
            const auto loop = Integer(header, 20, 4);
            if ((format != 1U && format != 2U) || (format == 2U && Integer(cnb, 16, 4) != 2U) ||
                rate < 8000U || rate > 192000U || (channels != 1U && channels != 2U) || frames == 0U ||
                frames > kMaxBytes / (channels * 2U) || start + loop > frames ||
                Integer(header, 24, 4) != 0U || frames * channels * (format == 1U ? 2U : 1U) != data.size())
            {
                throw std::runtime_error("invalid or unsupported weather PCM sample layout");
            }
            WeatherPcm pcm{{},
                           static_cast<int>(rate),
                           static_cast<int>(channels),
                           static_cast<int>(start),
                           static_cast<int>(loop)};
            pcm.samples.reserve(static_cast<std::size_t>(frames * channels * 2U));
            if (format == 1U)
            {
                pcm.samples.assign(data.begin(), data.end());
            }
            else
            {
                for (const auto byte : data)
                {
                    const auto sample = static_cast<std::uint16_t>((static_cast<int>(byte) - 128) * 256);
                    pcm.samples.push_back(static_cast<std::uint8_t>(sample & 255U));
                    pcm.samples.push_back(static_cast<std::uint8_t>(sample >> 8U));
                }
            }
            return pcm;
        }
        catch (const std::exception& error)
        {
            return util::Err(util::ErrorCode::InvalidData, error.what(), "weather PCM");
        }
    }

    util::Result<WeatherPcm> WeatherPcm::ReadFromTitle(std::string_view path)
    {
        try
        {
            auto stream = Microsoft::Xna::Framework::TitleContainer::OpenStream(std::string(path));
            if (stream == nullptr)
            {
                throw std::runtime_error("missing weather PCM");
            }
            System::IO::BinaryReader reader(stream.get(), true);
            const auto bytes = reader.ReadBytes(static_cast<int>(kMaxBytes + 1U));
            return Decode(bytes);
        }
        catch (const std::exception& error)
        {
            return util::Err(util::ErrorCode::IoFailure, error.what(), std::string(path));
        }
    }

    float WeatherPcm::LowPass(bool preserveLoopLevel)
    {
        if ((channels != 1 && channels != 2) || sampleRate < 8000 || samples.empty() ||
            samples.size() % (static_cast<std::size_t>(channels) * 2U) != 0U)
        {
            throw std::runtime_error("invalid weather low-pass PCM");
        }
        const auto original = samples;
        const double k = std::tan(std::numbers::pi * 900.0 / static_cast<double>(sampleRate));
        const double norm = 1.0 / (1.0 + std::sqrt(2.0) * k + k * k);
        const double b0 = k * k * norm;
        const double a1 = 2.0 * (k * k - 1.0) * norm;
        const double a2 = (1.0 - std::sqrt(2.0) * k + k * k) * norm;
        std::array<double, 2> z1{}, z2{};
        const std::size_t frames = original.size() / (static_cast<std::size_t>(channels) * 2U);
        const std::size_t begin = loopLength > 0 ? static_cast<std::size_t>(loopStart) : 0U;
        const std::size_t end = loopLength > 0 ? begin + static_cast<std::size_t>(loopLength) : frames;
        if (begin >= end || end > frames)
        {
            throw std::runtime_error("invalid weather low-pass loop");
        }
        // Warm the same loop before the retained pass: no zero-state seam at the loop boundary.
        std::array<double, 2> loopZ1{}, loopZ2{};
        for (int pass = 0; pass < 2; ++pass)
        {
            if (pass == 1)
            {
                loopZ1 = z1;
                loopZ2 = z2;
                z1 = {};
                z2 = {};
            }
            for (std::size_t frame = pass == 0 ? begin : 0U; frame < (pass == 0 ? end : frames); ++frame)
            {
                if (pass == 1 && frame == begin)
                {
                    z1 = loopZ1;
                    z2 = loopZ2;
                }
                for (std::size_t channel = 0; channel < static_cast<std::size_t>(channels); ++channel)
                {
                    const auto at = (frame * static_cast<std::size_t>(channels) + channel) * 2U;
                    const auto bits = static_cast<unsigned>(original[at]) |
                                      (static_cast<unsigned>(original[at + 1U]) << 8U);
                    const double input = static_cast<double>(bits >= 32768U ? static_cast<int>(bits) - 65536
                                                                            : static_cast<int>(bits));
                    const double output = b0 * input + z1[channel];
                    z1[channel] = 2.0 * b0 * input - a1 * output + z2[channel];
                    z2[channel] = b0 * input - a2 * output;
                    if (pass == 1)
                    {
                        const auto quantized = static_cast<std::uint16_t>(
                            static_cast<int>(std::clamp(std::round(output), -32768.0, 32767.0)));
                        samples[at] = static_cast<std::uint8_t>(quantized & 255U);
                        samples[at + 1U] = static_cast<std::uint8_t>(quantized >> 8U);
                    }
                }
            }
        }
        double beforeEnergy = 0.0;
        double afterEnergy = 0.0;
        double peak = 0.0;
        const auto signedSample = [](const std::vector<std::uint8_t>& bytes, std::size_t at)
        {
            const unsigned bits =
                static_cast<unsigned>(bytes[at]) | (static_cast<unsigned>(bytes[at + 1U]) << 8U);
            return static_cast<double>(bits >= 32768U ? static_cast<int>(bits) - 65536
                                                      : static_cast<int>(bits));
        };
        if (!preserveLoopLevel)
        {
            return 1.0F;
        }
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            for (std::size_t channel = 0; channel < static_cast<std::size_t>(channels); ++channel)
            {
                const auto at = (frame * static_cast<std::size_t>(channels) + channel) * 2U;
                const double filtered = signedSample(samples, at);
                peak = std::max(peak, std::abs(filtered));
                if (frame >= begin && frame < end)
                {
                    const double input = signedSample(original, at);
                    beforeEnergy += input * input;
                    afterEnergy += filtered * filtered;
                }
            }
        }
        if (peak == 0.0 || afterEnergy == 0.0)
        {
            return 1.0F;
        }
        const double gain = std::min({8.0, std::sqrt(beforeEnergy / afterEnergy), 0.9 * 32767.0 / peak});
        for (std::size_t at = 0; at < samples.size(); at += 2U)
        {
            const auto bits =
                static_cast<std::uint16_t>(static_cast<int>(std::round(signedSample(samples, at) * gain)));
            samples[at] = static_cast<std::uint8_t>(bits & 255U);
            samples[at + 1U] = static_cast<std::uint8_t>(bits >> 8U);
        }
        return static_cast<float>(gain);
    }
} // namespace cnahouse::audio
