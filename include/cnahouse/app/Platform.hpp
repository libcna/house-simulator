// SPDX-License-Identifier: MIT
#pragma once

#include <compare>
#include <string>
#include <string_view>
#include <vector>

namespace cnahouse::app
{

    enum class BuildTarget
    {
        Desktop,
        Web,
        Android,
    };

    struct DisplaySize
    {
        int width = 0;
        int height = 0;

        auto operator<=>(const DisplaySize&) const = default;
    };

    /// @brief What this build and this machine can do. Populated once, at startup, and never re-queried.
    ///
    /// **This struct is the project's answer to "does the renderer support X".** ADR-0001 forbids
    /// `GraphicsDevice::SupportsCapability` and every other CNA capability query, and `cna-house.md`
    /// §68 replaces them with a *project-owned effective feature set*: facts that are either build
    /// constants, standard XNA queries, or values phase 1 measured once and wrote into the platform
    /// profile.
    ///
    /// Every field below says which of those three it is. A field that could only be filled by a
    /// forbidden query does not exist.
    struct Platform
    {
        // --- build facts, baked in by CMake ----------------------------------------------------------
        /// @brief The renderer this binary was built for. Fixed at configure time (§7.3).
        std::string rendererName;
        BuildTarget target = BuildTarget::Desktop;
        /// @brief Input profile used by the compact touch HUD; not a graphics capability query.
        bool hasTouch = false;
        bool hasKeyboard = true;
        /// @brief Whether Tier E was compiled in. `HOUSE-00122` is the only place this is decided.
        bool tierECompiledIn = false;
        bool debugToolsCompiledIn = false;
        std::string version;

        // --- standard XNA queries --------------------------------------------------------------------
        /// @brief `GraphicsAdapter::CurrentDisplayMode`. Plain XNA 4.0, not a CNA extension.
        int displayWidth = 0;
        int displayHeight = 0;
        /// @brief Unique standard-XNA display sizes, filled from `GraphicsAdapter` at startup.
        std::vector<DisplaySize> displaySizes;
        /// @brief The adapter's description, for the bug-report header.
        std::string adapterDescription;

        // --- measured once, in phase 1, and encoded here ----------------------------------------------
        /// @brief `HOUSE-00109`: anisotropic filtering is available AND effective on this profile.
        bool anisotropicFiltering = false;
        /// @brief `HOUSE-00083`: a `SurfaceFormat::Single` render target works end to end.
        ///
        /// So Tier E's shadow map is a real float buffer and needs no RGBA8 packing. Recorded as a
        /// profile fact because it is one -- `HOUSE-00115` lists it among the rows to re-measure if the
        /// renderer changes.
        bool floatRenderTargets = false;
        /// @brief `HOUSE-00090`/`HOUSE-00091`: `OcclusionQuery::PixelCount` is a boolean here, not a
        ///        tally, so glare uses an N×N grid of point queries.
        bool occlusionQueryIsBoolean = true;
        /// @brief `HOUSE-00111`: DXT survives to the GPU, but only through `.xnb`.
        bool blockCompressedTextures = false;
        /// @brief `HOUSE-00106`: measured CPU cost of one `DrawIndexedPrimitives`, in microseconds.
        ///
        /// 8.15 µs on the development machine. It is here rather than in a comment because the draw
        /// budget is computed from it and a different machine will have a different number.
        float drawCallMicroseconds = 8.15f;

        /// @brief Populates everything that can be known without a `GraphicsDevice`.
        [[nodiscard]] static Platform FromBuild();

        /// @brief A one-line summary for the log header and the bug-report footer.
        [[nodiscard]] std::string Summary() const;
    };

} // namespace cnahouse::app
