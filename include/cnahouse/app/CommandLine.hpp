// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "cnahouse/util/Result.hpp"

namespace cnahouse::app
{

    /// @brief Which quality preset a session runs at.
    enum class QualityPreset
    {
        Low,
        Medium,
        High,
    };

    /// @brief Which render tier a session uses, subject to what the build actually contains.
    enum class RenderTier
    {
        /// @brief Stock XNA effects only. Always available; complete by design (ADR-0003).
        S,
        /// @brief Compiled `.fx`. Only reachable when `CNAHOUSE_TIER_E` was on at configure time.
        E,
    };

    /// @brief Everything the command line can say.
    ///
    /// **There is no `--renderer` option, and its absence is a design decision, not an omission.**
    /// `CNA_GRAPHICS_RENDERER` is fixed at CMake configure time and standard XNA 4.0 offers no way to
    /// change it afterwards (`cna-house.md` §7.3, §8.1), so a separate binary is produced per renderer.
    /// An option that appeared to change it would be a lie. `--renderer-info` prints what the
    /// application already knows about itself and queries nothing.
    struct Options
    {
        QualityPreset quality = QualityPreset::High;
        /// @brief The tier the user asked for. What they actually get is `ResolveTier`.
        RenderTier tier = RenderTier::E;
        bool headless = false;
        bool noAudio = false;
        bool rendererInfo = false;
        bool help = false;
        std::optional<std::string> scene;
        std::optional<std::uint64_t> seed;
        /// @brief Time of day to start at, in hours since midnight.
        std::optional<float> timeOfDay;
        std::optional<std::string> weather;
        /// @brief Take a screenshot to this path and exit.
        std::optional<std::string> screenshot;
        /// @brief `--log=world,content`; empty means every category.
        std::optional<std::string> logCategories;

        /// @brief Where `ContentManager` looks. Not a command-line option, deliberately.
        ///
        /// The shipping convention is "beside the executable", which is what the default means and
        /// what `RUNTIME_OUTPUT_DIRECTORY` arranges. It is a FIELD rather than an option because the
        /// only caller that needs to change it is a test: the test targets run from the repository
        /// root so their fixture paths are stable, and the content they need is in the build tree.
        /// Exposing that as a user-facing option would invite it to be used as one.
        std::string contentRoot = "content";
    };

    /// @brief Parses `argv`. An unknown option is an error, never a silent no-op.
    ///
    /// A mistyped `--quailty=low` that ran anyway at the default quality would produce a bug report
    /// about a setting that was never applied, so the parser refuses and says what it did not
    /// understand (`docs/conventions.md` §5.4).
    [[nodiscard]] util::Result<Options> ParseCommandLine(int argc, const char* const* argv);

    /// @brief The usage text, so `--help` and a parse failure print the same thing.
    [[nodiscard]] std::string UsageText();

    /// @brief The tier this binary can actually provide, given what it was built with.
    ///
    /// Asymmetric on purpose: a user may force Tier E **off**, never **on**. A binary built without
    /// Tier E has no compiled effects in its content, so honouring `--tier=e` would produce a
    /// content-load failure at the first draw — and deciding at configure time exists precisely so
    /// that cannot happen.
    [[nodiscard]] RenderTier ResolveTier(RenderTier requested) noexcept;

    [[nodiscard]] std::string_view QualityPresetName(QualityPreset preset) noexcept;
    [[nodiscard]] std::string_view RenderTierName(RenderTier tier) noexcept;

    /// @brief The `--renderer-info` report: configured renderer, build facts, resolved tier.
    ///
    /// Calls no CNA-specific API. Everything in it is a compile-time constant this binary was built
    /// with, which is exactly `HOUSE-00132`'s acceptance criterion.
    [[nodiscard]] std::string RendererInfo(const Options& options);

} // namespace cnahouse::app
