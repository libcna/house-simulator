// SPDX-License-Identifier: MIT
#include "cnahouse/app/Platform.hpp"

#include <format>

namespace cnahouse::app
{

    Platform Platform::FromBuild()
    {
        Platform platform;
        platform.rendererName = CNAHOUSE_RENDERER_NAME;
        platform.tierECompiledIn = CNAHOUSE_TIER_E != 0;
        platform.debugToolsCompiledIn = CNAHOUSE_DEBUG_TOOLS != 0;
        platform.version = CNAHOUSE_VERSION;

        // The measured phase-1 verdicts for the `linux` / `OPENGLES3` profile. They are constants here
        // because that is what a profile IS: a measurement made once during qualification and encoded,
        // never a runtime query (`cna-house.md` §27.2, §68). A different renderer gets a different
        // profile, and `HOUSE-00115` lists exactly which of these have to be re-measured for it.
        platform.anisotropicFiltering = true;    // HOUSE-00109: 1.47x more far-field contrast
        platform.floatRenderTargets = true;      // HOUSE-00083: 2048^2 Single, bit-exact readback
        platform.occlusionQueryIsBoolean = true; // HOUSE-00090: 1 for a 16 384-pixel quad
        platform.blockCompressedTextures = true; // HOUSE-00111: Dxt1/Dxt5, through .xnb only
        platform.drawCallMicroseconds = 8.15f;   // HOUSE-00106
        return platform;
    }

    std::string Platform::Summary() const
    {
        return std::format("cna-house {} · {} · Tier {} · debug {} · {}x{} · {}",
                           version,
                           rendererName,
                           tierECompiledIn ? "S+E" : "S",
                           debugToolsCompiledIn ? "on" : "off",
                           displayWidth,
                           displayHeight,
                           adapterDescription.empty() ? "adapter unknown" : adapterDescription);
    }

} // namespace cnahouse::app
