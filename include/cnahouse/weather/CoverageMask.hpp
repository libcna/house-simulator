// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/util/Result.hpp"

namespace System::IO
{
    class Stream;
}

namespace cnahouse::weather
{
    /// @brief Runtime view of `coverage.bin`, §37.2's 0.5 m rain-shelter height field.
    class CoverageMask
    {
    public:
        static constexpr std::uint32_t kMagic = 0x564F4343u; // 'C','C','O','V' little-endian
        static constexpr std::uint32_t kVersion = 1u;
        static constexpr float kCellSizeMetres = 0.5F;
        static constexpr std::uint32_t kMaxNameBytes = 1024u;
        static constexpr std::uint32_t kMaxSamples = 4194304u;

        [[nodiscard]] static util::Result<CoverageMask> Read(System::IO::Stream& stream,
                                                             std::string_view name);

        /// @brief Opens @p contentPath through XNA's platform-neutral `TitleContainer`.
        [[nodiscard]] static util::Result<CoverageMask> ReadFromTitle(std::string_view contentPath);

        /// @brief Highest finite soffit at the point, or +infinity for open sky/outside the field.
        [[nodiscard]] float HeightAt(float x, float z) const noexcept;

        /// @brief Whether §37.2 permits this particle to be drawn.
        ///
        /// Open sky is encoded as +infinity and is always exposed. At a finite covered sample,
        /// only positions strictly above the covering surface are exposed.
        [[nodiscard]] bool IsExposed(const Microsoft::Xna::Framework::Vector3& position) const noexcept;

        /// @brief Move a particle at or below finite cover to the precipitation volume's top.
        /// @return true exactly when the particle was sheltered and moved.
        bool TeleportSheltered(Microsoft::Xna::Framework::Vector3& position, float volumeTop) const noexcept;

        [[nodiscard]] std::string_view WorldHash() const noexcept
        {
            return worldHash_;
        }

        [[nodiscard]] std::uint32_t Width() const noexcept
        {
            return width_;
        }

        [[nodiscard]] std::uint32_t Height() const noexcept
        {
            return height_;
        }

        [[nodiscard]] std::size_t CoveredSampleCount() const noexcept;

    private:
        std::string worldHash_;
        float originX_ = 0.0F;
        float originZ_ = 0.0F;
        float cellSize_ = kCellSizeMetres;
        float ground_ = 0.0F;
        std::uint32_t width_ = 0u;
        std::uint32_t height_ = 0u;
        std::vector<float> heights_;
    };
} // namespace cnahouse::weather
