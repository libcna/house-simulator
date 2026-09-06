// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>

#include "cnahouse/app/CommandLine.hpp"

namespace cnahouse::rendering
{

    /// @brief The active render tier: a build fact, narrowed once at load, then read by everything.
    ///
    /// **Nothing in this type touches `GraphicsDevice`.** That is the whole point of it. ADR-0001
    /// forbids `SupportsCapability` and every other CNA capability query, and ADR-0003 replaces the
    /// question "what can this device do" with two questions that have honest answers:
    ///
    ///  1. **Was Tier E compiled into this binary?** Decided at configure time by
    ///     `cmake/TierSelection.cmake` and baked in as `CNAHOUSE_TIER_E` (`HOUSE-00122`).
    ///  2. **Did the Tier-E effect set actually load?** Decided once, in `LoadContent`, by *trying*
    ///     (`HOUSE-00161`). A capability query would answer neither: a device that supports compiled
    ///     effects can still be handed a build whose content tree has none.
    ///
    /// Published once and read thereafter, so no part of the frame can disagree with another about
    /// which tier it is drawing.
    class RenderTier
    {
    public:
        using Tier = app::RenderTier;

        /// @brief Whether Tier E exists in this binary at all. A compile-time constant.
        [[nodiscard]] static constexpr bool CompiledIn() noexcept
        {
            return CNAHOUSE_TIER_E != 0;
        }

        /// @brief What the user asked for, before the content has been tried.
        explicit RenderTier(Tier requested) noexcept
            : active_(app::ResolveTier(requested))
        {
        }

        [[nodiscard]] Tier Active() const noexcept
        {
            return active_;
        }

        [[nodiscard]] bool IsTierE() const noexcept
        {
            return active_ == Tier::E;
        }

        /// @brief Narrows to Tier S because the effect set did not load. Never widens.
        ///
        /// Asymmetric by construction: there is no `PromoteToE`. A binary without compiled effects in
        /// its content cannot acquire them at run time, and a method that implied otherwise would
        /// eventually be called.
        ///
        /// @return true if this call changed the tier, so the caller logs once rather than every frame.
        bool FallBackToS(std::string_view reason);

        /// @brief Why the tier was narrowed, or empty if it never was.
        [[nodiscard]] const std::string& FallbackReason() const noexcept
        {
            return reason_;
        }

        /// @brief Whether the user may still choose Tier E in the settings UI.
        ///
        /// False once the effect set has failed: offering a toggle that cannot work is worse than
        /// offering none, because the user will try it and conclude the game is broken.
        [[nodiscard]] bool TierEselectable() const noexcept
        {
            return CompiledIn() && reason_.empty();
        }

    private:
        Tier active_;
        std::string reason_;
    };

} // namespace cnahouse::rendering
