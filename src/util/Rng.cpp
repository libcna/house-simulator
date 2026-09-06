// SPDX-License-Identifier: MIT
#include "cnahouse/util/Rng.hpp"

#include <bit>
#include <cstdio>
#include <utility>

namespace cnahouse::util
{
    namespace
    {

        /// SplitMix64 -- the seeding sequence xoshiro's author specifies. Its job is to turn one 64-bit
        /// seed into four well-distributed words, so a seed of 1 is as good as any other.
        std::uint64_t SplitMix64(std::uint64_t& state) noexcept
        {
            state += 0x9E3779B97F4A7C15ull;
            std::uint64_t z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        }

        /// A 64x64 -> 128 bit multiply, as a (high, low) pair.
        ///
        /// Written out rather than using `unsigned __int128`, which is a compiler extension that
        /// `-Wpedantic` rejects and that the Web target does not have. Three 32-bit partial products and
        /// one carry; the compiler folds it back into a single `mulx` on x86-64 anyway.
        std::pair<std::uint64_t, std::uint64_t> MultiplyWide(std::uint64_t a, std::uint64_t b) noexcept
        {
            const std::uint64_t aLow = a & 0xFFFFFFFFull;
            const std::uint64_t aHigh = a >> 32;
            const std::uint64_t bLow = b & 0xFFFFFFFFull;
            const std::uint64_t bHigh = b >> 32;

            const std::uint64_t lowLow = aLow * bLow;
            const std::uint64_t lowHigh = aLow * bHigh;
            const std::uint64_t highLow = aHigh * bLow;
            const std::uint64_t highHigh = aHigh * bHigh;

            const std::uint64_t middle = (lowLow >> 32) + (lowHigh & 0xFFFFFFFFull) + highLow;
            const std::uint64_t high = highHigh + (lowHigh >> 32) + (middle >> 32);
            const std::uint64_t low = (middle << 32) | (lowLow & 0xFFFFFFFFull);
            return {high, low};
        }

    } // namespace

    Rng::Rng(std::uint64_t seed) noexcept
    {
        std::uint64_t sm = seed;
        for (auto& word : state_)
        {
            word = SplitMix64(sm);
        }
    }

    std::uint64_t Rng::NextUInt64() noexcept
    {
        const std::uint64_t result = std::rotl(state_[0] + state_[3], 23) + state_[0];
        const std::uint64_t t = state_[1] << 17;
        state_[2] ^= state_[0];
        state_[3] ^= state_[1];
        state_[1] ^= state_[2];
        state_[0] ^= state_[3];
        state_[2] ^= t;
        state_[3] = std::rotl(state_[3], 45);
        return result;
    }

    float Rng::NextFloat() noexcept
    {
        // The top 24 bits, scaled by 2^-24: exactly the mantissa a float can hold, so every
        // representable value in [0,1) is reachable and none is favoured by a rounding step.
        return static_cast<float>(NextUInt64() >> 40) * 0x1.0p-24f;
    }

    float Rng::NextFloat(float min, float max) noexcept
    {
        return min + (max - min) * NextFloat();
    }

    std::int32_t Rng::NextInt(std::int32_t min, std::int32_t max) noexcept
    {
        if (max <= min)
        {
            return min;
        }
        // Computed in unsigned arithmetic throughout: `max - min` on two `int32_t` overflows for a
        // range spanning the whole type, which is exactly the case a bounds helper must survive.
        const std::uint64_t range =
            static_cast<std::uint64_t>(static_cast<std::int64_t>(max) - static_cast<std::int64_t>(min)) +
            1ull;

        // Lemire's method with rejection. A plain `% range` biases the low values whenever `range` does
        // not divide 2^64 -- invisibly, and exactly where a designer would later wonder why the first
        // row of a table comes up slightly too often.
        std::uint64_t x = NextUInt64();
        auto [high, low] = MultiplyWide(x, range);
        if (low < range)
        {
            const std::uint64_t threshold = (~range + 1ull) % range;
            while (low < threshold)
            {
                x = NextUInt64();
                const auto next = MultiplyWide(x, range);
                high = next.first;
                low = next.second;
            }
        }
        return static_cast<std::int32_t>(static_cast<std::int64_t>(min) + static_cast<std::int64_t>(high));
    }

    std::string Rng::ToHex() const
    {
        std::string out;
        out.resize(64);
        for (std::size_t i = 0; i < state_.size(); ++i)
        {
            std::snprintf(out.data() + i * 16, 17, "%016llx", static_cast<unsigned long long>(state_[i]));
        }
        return out;
    }

    bool Rng::FromHex(std::string_view hex) noexcept
    {
        if (hex.size() != 64)
        {
            return false;
        }
        State parsed{};
        for (std::size_t i = 0; i < parsed.size(); ++i)
        {
            std::uint64_t word = 0;
            for (std::size_t c = 0; c < 16; ++c)
            {
                const char ch = hex[i * 16 + c];
                std::uint64_t digit = 0;
                if (ch >= '0' && ch <= '9')
                {
                    digit = static_cast<std::uint64_t>(ch - '0');
                }
                else if (ch >= 'a' && ch <= 'f')
                {
                    digit = static_cast<std::uint64_t>(ch - 'a' + 10);
                }
                else if (ch >= 'A' && ch <= 'F')
                {
                    digit = static_cast<std::uint64_t>(ch - 'A' + 10);
                }
                else
                {
                    // Leave the state untouched: a half-restored generator is worse than a rejected
                    // one, because it looks like it worked.
                    return false;
                }
                word = (word << 4) | digit;
            }
            parsed[i] = word;
        }
        state_ = parsed;
        return true;
    }

} // namespace cnahouse::util
