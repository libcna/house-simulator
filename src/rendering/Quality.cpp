// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/Quality.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "cnahouse/app/Platform.hpp"
#include "cnahouse/rendering/RenderTier.hpp"

namespace cnahouse::rendering
{

    std::string_view ShadowQualityName(ShadowQuality quality) noexcept
    {
        switch (quality)
        {
            case ShadowQuality::Off:
                return "off";
            case ShadowQuality::Blob:
                return "blob";
            case ShadowQuality::Map1024:
                return "map-1024";
            case ShadowQuality::Map2048:
                return "map-2048";
        }
        return "?";
    }

    std::string_view ParticleQualityName(ParticleQuality quality) noexcept
    {
        switch (quality)
        {
            case ParticleQuality::Low:
                return "low";
            case ParticleQuality::Medium:
                return "medium";
            case ParticleQuality::High:
                return "high";
        }
        return "?";
    }

    QualitySettings SettingsFor(app::QualityPreset preset) noexcept
    {
        QualitySettings settings;
        switch (preset)
        {
            case app::QualityPreset::Low:
                // No shadows at all, not even blobs: the machines that need this row are the ones where
                // an extra quad per dynamic object is a real cost. Anisotropy 1 means trilinear, which
                // is §68's named fallback rather than "off".
                settings.shadows = ShadowQuality::Off;
                settings.particles = ParticleQuality::Low;
                settings.viewDistance = 0.6f;
                settings.lodBias = 2;
                settings.anisotropy = 1;
                settings.halfTextures = true;
                settings.postProcessing = false;
                break;
            case app::QualityPreset::Medium:
                settings.shadows = ShadowQuality::Blob;
                settings.particles = ParticleQuality::Medium;
                settings.viewDistance = 0.85f;
                settings.lodBias = 1;
                settings.anisotropy = 4;
                settings.halfTextures = false;
                settings.postProcessing = false;
                break;
            case app::QualityPreset::High:
                settings.shadows = ShadowQuality::Map1024;
                settings.particles = ParticleQuality::Medium;
                settings.viewDistance = 1.0f;
                settings.lodBias = 0;
                settings.anisotropy = 8;
                settings.halfTextures = false;
                settings.postProcessing = true;
                break;
            case app::QualityPreset::Ultra:
                settings.shadows = ShadowQuality::Map2048;
                settings.particles = ParticleQuality::High;
                settings.viewDistance = 1.4f;
                settings.lodBias = -1;
                settings.anisotropy = 16;
                settings.halfTextures = false;
                settings.postProcessing = true;
                break;
        }
        return settings;
    }

    QualitySettings
    Restrict(QualitySettings settings, const app::Platform& platform, const RenderTier& tier) noexcept
    {
        if (!tier.IsTierE())
        {
            // ADR-0003: shadow MAPS and the composite are the two Tier-E passes. Blob shadows are
            // stock `BasicEffect` geometry and stay -- which is what makes "Tier S is complete"
            // true rather than merely stated. A Tier-S session still has shadows; they are cheaper.
            if (settings.shadows == ShadowQuality::Map1024 || settings.shadows == ShadowQuality::Map2048)
            {
                settings.shadows = ShadowQuality::Blob;
            }
            settings.postProcessing = false;
        }
        else if (!platform.floatRenderTargets && settings.shadows != ShadowQuality::Off)
        {
            // MEASURED (`HOUSE-00083`): a `SurfaceFormat::Single` render target works end to end on
            // the validated profile, so the shadow map is a real float buffer and needs no RGBA8
            // packing. A profile where it does not hold has no shadow-map path written for it, so
            // the honest thing is to drop to blobs rather than to ship an untested packing.
            settings.shadows = std::min(settings.shadows, ShadowQuality::Blob);
        }

        if (!platform.anisotropicFiltering)
        {
            // MEASURED (`HOUSE-00109`): available AND effective on the validated profile. Elsewhere
            // §68 says trilinear, which is `anisotropy == 1` with mips -- not "filtering off".
            settings.anisotropy = 1;
        }

        // Clamped rather than trusted, because these also arrive from a settings file that a user
        // may have edited and that an older schema version may have written.
        settings.anisotropy = std::clamp(settings.anisotropy, 1, 16);
        settings.viewDistance = std::clamp(settings.viewDistance, 0.6f, 1.4f);
        settings.lodBias = std::clamp(settings.lodBias, -1, 2);
        return settings;
    }

    bool IsSoftwareRasteriser(std::string_view description) noexcept
    {
        std::string lowered;
        lowered.reserve(description.size());
        for (const char c : description)
        {
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        for (const std::string_view needle :
             {"llvmpipe", "softpipe", "swrast", "offscreen", "software rasterizer", "microsoft basic render"})
        {
            if (lowered.find(needle) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    app::QualityPreset AutoDetect(const app::Platform& platform, const RenderTier& tier) noexcept
    {
        // 1. A software rasteriser. The only signal in the adapter string that is genuinely
        //    reliable, and the only one this heuristic acts on with confidence: no quality setting
        //    makes a CPU rasteriser fast. CI runs here on purpose (`HOUSE-00138`).
        if (IsSoftwareRasteriser(platform.adapterDescription))
        {
            return app::QualityPreset::Low;
        }

        // 2. A platform with its own §71.3 row starts on it (`HOUSE-02898`): the browser on Web,
        //    a phone or tablet on Android. Their content ships only that preset's LOD level, and
        //    their budgets are written against that row, not against the desktop's.
        if (platform.target == app::BuildTarget::Web)
        {
            return app::QualityPreset::Medium;
        }
        if (platform.target == app::BuildTarget::Android)
        {
            return app::QualityPreset::Low;
        }

        // 3. The HEADLESS renderer rasterises nothing, so the preset is a formality -- but it must
        //    be the cheap one, because an integration test that ran the expensive paths would be
        //    measuring work no one asked for.
        if (platform.rendererName == "HEADLESS")
        {
            return app::QualityPreset::Low;
        }

        // 4. A display 3 840 pixels wide or more asks the same GPU to fill four times the pixels of
        //    1080p, and this project has no dynamic resolution to absorb that. Stepping down one row
        //    is the cheapest correct guess. A zero width means the adapter was not queried -- a
        //    headless or very early call -- and is not evidence of anything.
        if (platform.displayWidth >= 3840)
        {
            return app::QualityPreset::Medium;
        }

        // 5. Everything else. NOT `Ultra`: guessing a machine into the top row from a name string is
        //    exactly the confidence the available facts do not support, and being wrong there costs
        //    a first session that stutters. `Ultra` is reachable only by asking for it.
        return tier.IsTierE() ? app::QualityPreset::High : app::QualityPreset::Medium;
    }

} // namespace cnahouse::rendering
