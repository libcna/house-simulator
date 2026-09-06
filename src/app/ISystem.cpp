// SPDX-License-Identifier: MIT
#include "cnahouse/app/ISystem.hpp"

#include <array>

namespace cnahouse::app
{
    namespace
    {
        constexpr std::array<std::string_view, static_cast<std::size_t>(UpdateStage::Count)> kStageNames{
            "input",
            "clock",
            "weather",
            "interaction",
            "apertures",
            "lighting",
            "physics",
            "animation",
            "pets",
            "audio",
            "visibility",
            "residency"};
    } // namespace

    std::string_view UpdateStageName(UpdateStage stage) noexcept
    {
        const auto index = static_cast<std::size_t>(stage);
        return index < kStageNames.size() ? kStageNames[index] : std::string_view{"?"};
    }

} // namespace cnahouse::app
