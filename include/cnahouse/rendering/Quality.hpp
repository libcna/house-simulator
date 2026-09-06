// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string_view>

#include "cnahouse/app/CommandLine.hpp"

namespace cnahouse::app
{
    struct Platform;
}

namespace cnahouse::rendering
{

    class RenderTier;

    /// @brief How shadows are drawn, from cheapest to most expensive.
    enum class ShadowQuality : std::uint8_t
    {
        /// @brief None at all. Everything is lit as if unoccluded.
        Off,
        /// @brief A projected blob under each dynamic thing. Costs one quad and needs no Tier E.
        Blob,
        Map1024,
        Map2048,
    };

    enum class ParticleQuality : std::uint8_t
    {
        Low,
        Medium,
        High,
    };

    [[nodiscard]] std::string_view ShadowQualityName(ShadowQuality quality) noexcept;
    [[nodiscard]] std::string_view ParticleQualityName(ParticleQuality quality) noexcept;

    /// @brief The knobs a quality preset sets. `cna-house.md` §68's Graphics tab, resolved.
    struct QualitySettings
    {
        ShadowQuality shadows = ShadowQuality::Blob;
        ParticleQuality particles = ParticleQuality::Medium;

        /// @brief Multiplies the far plane and the residency radius. §68's 0.6× – 1.4×.
        float viewDistance = 1.0f;
        /// @brief §68's −1 / 0 / +1 / +2. Positive picks a cheaper mesh sooner.
        int lodBias = 0;
        /// @brief 1, 4, 8 or 16. **1 means trilinear**, which is the fallback §68 names.
        int anisotropy = 8;
        /// @brief Half-resolution textures, for the machines that need it.
        bool halfTextures = false;
        /// @brief Tier E's tonemap-and-glare composite.
        bool postProcessing = true;
    };

    /// @brief The row @p preset names. A pure table; nothing about the machine enters here.
    [[nodiscard]] QualitySettings SettingsFor(app::QualityPreset preset) noexcept;

    /// @brief Removes from @p settings everything this build and profile cannot actually do.
    ///
    /// **This is `cna-house.md` §68's "project-owned effective feature set" applied.** No CNA
    /// capability query is involved and none is possible: the inputs are `app::Platform` — build
    /// constants, standard-XNA `GraphicsAdapter` values, and facts phase 1 measured once — plus the
    /// resolved render tier. §68's rule is that nothing meaningless is ever offered, and this is
    /// where "offered" becomes "true".
    [[nodiscard]] QualitySettings
    Restrict(QualitySettings settings, const app::Platform& platform, const RenderTier& tier) noexcept;

    /// @brief The preset to start a first-ever session at.
    ///
    /// **Deliberately coarse, and it is worth saying why.** Standard XNA 4.0 offers no VRAM figure,
    /// no GPU class and no feature level — `GraphicsAdapter` gives a description string and the
    /// display modes, and ADR-0001 forbids asking CNA for anything more. So this heuristic exists to
    /// avoid a bad FIRST frame, not to be right; the user overrides it and the choice is persisted.
    /// It never returns the top row, because guessing a machine into `Ultra` from a name string is
    /// exactly the kind of confidence the available facts do not support.
    [[nodiscard]] app::QualityPreset AutoDetect(const app::Platform& platform,
                                                const RenderTier& tier) noexcept;

    /// @brief Whether @p description names a software rasteriser.
    ///
    /// The one genuinely reliable signal in the adapter string: `llvmpipe`, `softpipe`, `swrast` and
    /// Mesa's offscreen device are CPU rasterisers, and no amount of quality setting makes one fast.
    /// Exposed because CI runs under exactly this (`HOUSE-00138` uses `LIBGL_ALWAYS_SOFTWARE=1`) and
    /// a render test that silently ran at a different preset than it thought would be worthless.
    [[nodiscard]] bool IsSoftwareRasteriser(std::string_view description) noexcept;

} // namespace cnahouse::rendering
