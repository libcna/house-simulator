// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/WindowGlow.hpp"

#include <algorithm>
#include <bit>

namespace cnahouse::rendering::WindowGlow
{
    namespace
    {
        /// One more FNV-1a round over a 32-bit value, so a house's id and a window's index mix.
        ///
        /// `util::Id::Hash` is the project's hash and takes a string; this is the same function's
        /// arithmetic applied to four bytes, which keeps one hash in the codebase rather than
        /// introducing a second with different collision behaviour.
        [[nodiscard]] constexpr std::uint32_t Mix(std::uint32_t hash, std::uint32_t value) noexcept
        {
            for (int shift = 0; shift < 32; shift += 8)
            {
                hash ^= (value >> shift) & 0xFFu;
                hash *= 16777619u;
            }
            return hash;
        }

        /// The lit-window decision for a house, before it is trimmed to its window count.
        ///
        /// A quarter of the bits are cleared on purpose. A raw hash lights half the windows, and
        /// half of four is two on every house on the street; taking a second, differently-mixed
        /// hash into the AND makes the COUNT vary -- some houses mostly dark, one or two mostly
        /// lit -- which is what a street at nine in the evening looks like.
        [[nodiscard]] std::uint32_t RawMask(util::Id instance) noexcept
        {
            const std::uint32_t base = Mix(instance.Value(), 0x6C69u);   // 'li'
            const std::uint32_t sparse = Mix(instance.Value(), 0x6768u); // 'gh'
            return base & (sparse | (sparse >> 3) | (sparse << 5));
        }
    } // namespace

    std::uint32_t LitMask(util::Id instance, std::uint32_t windowCount) noexcept
    {
        const std::uint32_t count = std::min(windowCount, kMaxWindows);
        if (count == 0u)
        {
            return 0u;
        }
        // `1u << 32` is undefined, which is why the full-width case is spelled out rather than
        // left to the shift.
        const std::uint32_t keep =
            count == kMaxWindows ? 0xFFFFFFFFu : static_cast<std::uint32_t>((1u << count) - 1u);
        return RawMask(instance) & keep;
    }

    bool IsLit(util::Id instance, std::uint32_t windowCount, std::uint32_t index) noexcept
    {
        if (index >= std::min(windowCount, kMaxWindows))
        {
            return false;
        }
        return (LitMask(instance, windowCount) & (1u << index)) != 0u;
    }

    std::uint32_t LitCount(util::Id instance, std::uint32_t windowCount) noexcept
    {
        return static_cast<std::uint32_t>(std::popcount(LitMask(instance, windowCount)));
    }

    float Level(util::Id instance, std::uint32_t windowCount, std::uint32_t index, float night) noexcept
    {
        if (!IsLit(instance, windowCount, index))
        {
            return 0.0F;
        }
        // Clamped rather than trusted: `night` comes from §35's curve, and a card drawn at 1.4 is
        // a window brighter than the sun it is meant to have replaced.
        const float level = std::clamp(night, 0.0F, 1.0F);
        // The low byte of a third mix, so brightness is independent of whether the window is lit
        // at all: reusing `RawMask`'s bits would make every dim window a lit one's neighbour.
        const std::uint32_t shade = Mix(instance.Value(), 0x100u + index) & 0xFFu;
        const float brightness = kDimmest + (1.0F - kDimmest) * (static_cast<float>(shade) / 255.0F);
        return level * brightness;
    }

} // namespace cnahouse::rendering::WindowGlow
