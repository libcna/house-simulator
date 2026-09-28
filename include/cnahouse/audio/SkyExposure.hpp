// SPDX-License-Identifier: MIT
#pragma once

#include <string_view>
#include <unordered_map>

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Result.hpp"

namespace System::IO
{
    class Stream;
}

namespace cnahouse::audio
{
    /// The existing CSKY v2 bake, not daylight or a runtime acoustic simulation.
    class SkyExposure
    {
    public:
        [[nodiscard]] static util::Result<SkyExposure>
        Read(System::IO::Stream& stream, std::string_view name, std::string_view worldHash);
        [[nodiscard]] static util::Result<SkyExposure> ReadFromTitle(std::string_view path,
                                                                     std::string_view worldHash);
        [[nodiscard]] float At(util::Id cell) const noexcept;

        [[nodiscard]] bool Contains(util::Id cell) const noexcept
        {
            return cells_.contains(cell);
        }

        [[nodiscard]] std::size_t CellCount() const noexcept
        {
            return cells_.size();
        }

    private:
        std::unordered_map<util::Id, float> cells_;
    };
} // namespace cnahouse::audio
