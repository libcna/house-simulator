// SPDX-License-Identifier: MIT
//
// `HOUSE-00157`. A unit test, and that is itself the claim being made: the quality table is a pure
// table and the auto-detect heuristic reads only `app::Platform` — build constants, standard-XNA
// `GraphicsAdapter` values, and facts phase 1 measured once. **If any of this needed a
// `GraphicsDevice`, it would be a capability query, which ADR-0001 forbids.** It compiles and runs
// without one because it is not.
#include <gtest/gtest.h>

#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Platform.hpp"
#include "cnahouse/rendering/Quality.hpp"
#include "cnahouse/rendering/RenderTier.hpp"

namespace
{
    using cnahouse::app::Platform;
    using cnahouse::app::QualityPreset;
    using cnahouse::rendering::AutoDetect;
    using cnahouse::rendering::IsSoftwareRasteriser;
    using cnahouse::rendering::ParticleQuality;
    using cnahouse::rendering::QualitySettings;
    using cnahouse::rendering::Restrict;
    using cnahouse::rendering::SettingsFor;
    using cnahouse::rendering::ShadowQuality;

    /// A well-equipped machine on the validated profile: everything phase 1 measured is available.
    Platform GoodMachine()
    {
        Platform platform;
        platform.rendererName = "OPENGLES3";
        platform.adapterDescription = "Mesa Intel(R) Graphics (ADL GT2)";
        platform.displayWidth = 1920;
        platform.displayHeight = 1080;
        platform.anisotropicFiltering = true;
        platform.floatRenderTargets = true;
        return platform;
    }

    cnahouse::rendering::RenderTier TierS()
    {
        return cnahouse::rendering::RenderTier(cnahouse::app::RenderTier::S);
    }

    /// Tier E, but only where the build has it. Every assertion that depends on the difference says
    /// so, so this file is meaningful in both configurations rather than only in one.
    cnahouse::rendering::RenderTier TierE()
    {
        return cnahouse::rendering::RenderTier(cnahouse::app::RenderTier::E);
    }

    TEST(QualityTests, ThePresetsAreOrderedFromCheapestToMostExpensive)
    {
        // Not a spot check of one row: the whole point of a preset table is that it is monotonic,
        // and a row that is cheaper than the one below it in ONE knob is the kind of mistake that
        // survives review and then confuses every bug report about performance.
        const QualitySettings low = SettingsFor(QualityPreset::Low);
        const QualitySettings medium = SettingsFor(QualityPreset::Medium);
        const QualitySettings high = SettingsFor(QualityPreset::High);
        const QualitySettings ultra = SettingsFor(QualityPreset::Ultra);

        EXPECT_LE(low.shadows, medium.shadows);
        EXPECT_LE(medium.shadows, high.shadows);
        EXPECT_LE(high.shadows, ultra.shadows);

        EXPECT_LT(low.viewDistance, medium.viewDistance);
        EXPECT_LT(medium.viewDistance, high.viewDistance);
        EXPECT_LT(high.viewDistance, ultra.viewDistance);

        // A HIGHER lod bias picks a cheaper mesh sooner, so this one runs the other way.
        EXPECT_GT(low.lodBias, medium.lodBias);
        EXPECT_GT(medium.lodBias, high.lodBias);
        EXPECT_GT(high.lodBias, ultra.lodBias);

        EXPECT_LT(low.anisotropy, medium.anisotropy);
        EXPECT_LT(medium.anisotropy, high.anisotropy);
        EXPECT_LT(high.anisotropy, ultra.anisotropy);

        EXPECT_TRUE(low.halfTextures);
        EXPECT_FALSE(ultra.halfTextures);
    }

    TEST(QualityTests, AnisotropyOfOneMeansTrilinearAndNotFilteringOff)
    {
        // §68's fallback is trilinear -- `TextureFilter::Linear` with mips -- not point sampling.
        // The row records that as 1, which is the value XNA's `MaxAnisotropy` takes for it.
        EXPECT_EQ(SettingsFor(QualityPreset::Low).anisotropy, 1);
        for (const QualityPreset preset :
             {QualityPreset::Low, QualityPreset::Medium, QualityPreset::High, QualityPreset::Ultra})
        {
            EXPECT_GE(SettingsFor(preset).anisotropy, 1) << "0 would mean no filter at all";
            EXPECT_LE(SettingsFor(preset).anisotropy, 16);
        }
    }

    TEST(QualityTests, TierSKeepsBlobShadowsAndLosesOnlyTheTierEPasses)
    {
        // This is what makes "Tier S is complete" (ADR-0003) true rather than merely stated: a
        // Tier-S session still has shadows, and they are the cheaper kind rather than none.
        const QualitySettings restricted =
            Restrict(SettingsFor(QualityPreset::Ultra), GoodMachine(), TierS());

        EXPECT_EQ(restricted.shadows, ShadowQuality::Blob);
        EXPECT_FALSE(restricted.postProcessing);
        EXPECT_EQ(restricted.particles, ParticleQuality::High)
            << "particles are stock geometry and have nothing to do with the tier";
        EXPECT_EQ(restricted.anisotropy, 16) << "and so does sampler state";
    }

    TEST(QualityTests, WithoutFloatRenderTargetsTheShadowMapDropsToBlobs)
    {
        // MEASURED (`HOUSE-00083`): `SurfaceFormat::Single` works end to end on the validated
        // profile, so the shadow map is a real float buffer. A profile where that does not hold has
        // no shadow-map path written for it, and dropping to blobs is more honest than shipping an
        // untested RGBA8 packing.
        Platform platform = GoodMachine();
        platform.floatRenderTargets = false;

        const QualitySettings restricted = Restrict(SettingsFor(QualityPreset::Ultra), platform, TierE());
        if constexpr (cnahouse::rendering::RenderTier::CompiledIn())
        {
            EXPECT_EQ(restricted.shadows, ShadowQuality::Blob);
        }
        else
        {
            EXPECT_EQ(restricted.shadows, ShadowQuality::Blob) << "Tier S reaches the same place";
        }
    }

    TEST(QualityTests, WithoutAnisotropicFilteringEveryPresetSamplesTrilinear)
    {
        // MEASURED (`HOUSE-00109`): anisotropy is available AND effective on the validated profile.
        // Where the profile does not say so, §68's rule is that the row is not offered at all --
        // which here means the value is forced, not that the setting is silently ignored.
        Platform platform = GoodMachine();
        platform.anisotropicFiltering = false;
        for (const QualityPreset preset :
             {QualityPreset::Low, QualityPreset::Medium, QualityPreset::High, QualityPreset::Ultra})
        {
            EXPECT_EQ(Restrict(SettingsFor(preset), platform, TierS()).anisotropy, 1)
                << "preset " << static_cast<int>(preset);
        }
    }

    TEST(QualityTests, RestrictClampsValuesThatCameFromAnEditedSettingsFile)
    {
        // Not hypothetical: `settings.json` is user-editable by design and an older schema version
        // may have written a value this build no longer accepts.
        QualitySettings absurd;
        absurd.anisotropy = 64;
        absurd.viewDistance = 12.0f;
        absurd.lodBias = 9;

        const QualitySettings restricted = Restrict(absurd, GoodMachine(), TierS());
        EXPECT_EQ(restricted.anisotropy, 16);
        EXPECT_FLOAT_EQ(restricted.viewDistance, 1.4f);
        EXPECT_EQ(restricted.lodBias, 2);

        QualitySettings tiny;
        tiny.anisotropy = 0;
        tiny.viewDistance = 0.01f;
        tiny.lodBias = -9;
        const QualitySettings raised = Restrict(tiny, GoodMachine(), TierS());
        EXPECT_EQ(raised.anisotropy, 1);
        EXPECT_FLOAT_EQ(raised.viewDistance, 0.6f);
        EXPECT_EQ(raised.lodBias, -1);
    }

    TEST(QualityTests, ASoftwareRasteriserIsRecognisedByName)
    {
        // The only genuinely reliable signal in the adapter string, and CI runs on one on purpose
        // (`HOUSE-00138` sets `LIBGL_ALWAYS_SOFTWARE=1`).
        EXPECT_TRUE(IsSoftwareRasteriser("llvmpipe (LLVM 15.0.6, 256 bits)"));
        EXPECT_TRUE(IsSoftwareRasteriser("softpipe"));
        EXPECT_TRUE(IsSoftwareRasteriser("Mesa Offscreen"));
        EXPECT_TRUE(IsSoftwareRasteriser("SWRAST")) << "the match must not depend on case";
        EXPECT_FALSE(IsSoftwareRasteriser("Mesa Intel(R) Graphics (ADL GT2)"));
        EXPECT_FALSE(IsSoftwareRasteriser("NVIDIA GeForce RTX 3060"));
        EXPECT_FALSE(IsSoftwareRasteriser("")) << "an unknown adapter is not evidence of anything";
    }

    TEST(QualityTests, AutoDetectPutsASoftwareRasteriserOnTheLowestRow)
    {
        Platform platform = GoodMachine();
        platform.adapterDescription = "llvmpipe (LLVM 15.0.6, 256 bits)";
        EXPECT_EQ(AutoDetect(platform, TierE()), QualityPreset::Low)
            << "no quality setting makes a CPU rasteriser fast";
    }

    TEST(QualityTests, AutoDetectPutsHeadlessOnTheLowestRow)
    {
        Platform platform = GoodMachine();
        platform.rendererName = "HEADLESS";
        EXPECT_EQ(AutoDetect(platform, TierE()), QualityPreset::Low)
            << "an integration test must not run the expensive paths for nothing";
    }

    TEST(QualityTests, AutoDetectStepsDownOnAFourKDisplay)
    {
        // The same GPU is asked to fill four times the pixels of 1080p and this project has no
        // dynamic resolution to absorb that.
        Platform platform = GoodMachine();
        platform.displayWidth = 3840;
        platform.displayHeight = 2160;
        EXPECT_EQ(AutoDetect(platform, TierE()), QualityPreset::Medium);

        platform.displayWidth = 3839;
        if constexpr (cnahouse::rendering::RenderTier::CompiledIn())
        {
            EXPECT_EQ(AutoDetect(platform, TierE()), QualityPreset::High)
                << "the threshold must actually be a threshold";
        }
        else
        {
            // On a Tier-S-only build BOTH sides of the threshold land on `Medium`, for two
            // different reasons -- 4K steps down, and Tier S never starts on the shadow-map row --
            // so there is no distinction here to assert and pretending there is would be a test
            // that passes without checking anything.
            EXPECT_EQ(AutoDetect(platform, TierE()), QualityPreset::Medium);
        }
    }

    TEST(QualityTests, AutoDetectNeverPicksTheTopRow)
    {
        // Standard XNA 4.0 offers no VRAM figure, no GPU class and no feature level, and ADR-0001
        // forbids asking CNA for more. Guessing a machine into `Ultra` from a name string is exactly
        // the confidence those facts do not support, and being wrong costs a stuttering first
        // session.
        Platform platform = GoodMachine();
        platform.adapterDescription = "NVIDIA GeForce RTX 4090";
        platform.displayWidth = 2560;
        EXPECT_NE(AutoDetect(platform, TierE()), QualityPreset::Ultra);
        EXPECT_NE(AutoDetect(platform, TierS()), QualityPreset::Ultra);
    }

    TEST(QualityTests, AutoDetectDoesNotStartATierSSessionOnTheShadowMapRow)
    {
        // Not a performance judgement: `High` would immediately be restricted back to blobs, and a
        // preset name that does not match what is drawn makes every later bug report ambiguous.
        Platform platform = GoodMachine();
        EXPECT_EQ(AutoDetect(platform, TierS()), QualityPreset::Medium);
    }

    TEST(QualityTests, AZeroDisplayWidthIsNotTreatedAsEvidence)
    {
        // A zero width means the adapter was never queried -- an early call, or a headless run --
        // and reading it as "a tiny display, so crank everything up" would be reading noise.
        Platform platform = GoodMachine();
        platform.displayWidth = 0;
        platform.displayHeight = 0;
        const QualityPreset detected = AutoDetect(platform, TierE());
        EXPECT_NE(detected, QualityPreset::Ultra);
        EXPECT_NE(detected, QualityPreset::Low) << "an unknown display is not a bad one";
    }
} // namespace
