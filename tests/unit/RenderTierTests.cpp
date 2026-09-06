// SPDX-License-Identifier: MIT
//
// `HOUSE-00160` and `HOUSE-00163`. Both types under test are deliberately free of `GraphicsDevice`,
// which is why they can be unit tests at all: ADR-0001 forbids asking the device what it supports,
// so the tier is a build fact plus one load attempt, and the cull policy is arithmetic on a matrix
// determinant. Neither question has a device in it.
#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/rendering/RenderStates.hpp"
#include "cnahouse/rendering/RenderTier.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using cnahouse::app::RenderTier;
    using cnahouse::rendering::CullPolicy;
    using cnahouse::rendering::PolicyForDeterminant;
    using cnahouse::rendering::StateFor;

    TEST(RenderTierTests, TierSIsRequestableInEveryBuild)
    {
        // The one invariant ADR-0003 rests on: Tier S is complete by design and always reachable.
        const cnahouse::rendering::RenderTier tier(RenderTier::S);
        EXPECT_EQ(tier.Active(), RenderTier::S);
        EXPECT_FALSE(tier.IsTierE());
        EXPECT_TRUE(tier.FallbackReason().empty()) << "asking for S is not a fallback";
    }

    TEST(RenderTierTests, AskingForTierEGetsItOnlyIfItWasCompiledIn)
    {
        // Asserting against `CompiledIn()` rather than against a literal is what makes this test
        // meaningful in BOTH configurations instead of only in the one the author happened to run.
        const cnahouse::rendering::RenderTier tier(RenderTier::E);
        EXPECT_EQ(tier.IsTierE(), cnahouse::rendering::RenderTier::CompiledIn());
    }

    TEST(RenderTierTests, TheFallbackNarrowsAndTheSecondCallIsNotAChange)
    {
        cnahouse::rendering::RenderTier tier(RenderTier::E);

        const bool changed = tier.FallBackToS("the effect set did not load");
        EXPECT_EQ(changed, cnahouse::rendering::RenderTier::CompiledIn())
            << "it changed the tier if and only if there was a Tier E to change from";
        EXPECT_EQ(tier.Active(), RenderTier::S);

        // The caller logs on `true`, so a repeated failure must NOT keep returning it -- that is
        // the difference between one warning and one per frame.
        EXPECT_FALSE(tier.FallBackToS("and it still did not"));

        // The reason is recorded EXACTLY when a narrowing happened, which is the type's invariant:
        // a non-empty reason means the tier was moved at run time. On a Tier-S-only build nothing
        // was moved, and reporting a fallback that did not occur would be a false diagnostic --
        // this assertion was written the other way round first and the `headless` preset, where
        // `CNAHOUSE_TIER_E` is off, is what caught it.
        if constexpr (cnahouse::rendering::RenderTier::CompiledIn())
        {
            EXPECT_EQ(tier.FallbackReason(), "the effect set did not load")
                << "the FIRST reason is the useful one; later ones are consequences of it";
        }
        else
        {
            EXPECT_TRUE(tier.FallbackReason().empty())
                << "there was no Tier E to fall back FROM, so nothing was narrowed";
        }
    }

    TEST(RenderTierTests, ThereIsNoRouteBackUpToTierE)
    {
        // The asymmetry is the design: a binary without compiled effects in its content tree cannot
        // acquire them at run time. This test exists so that a future `PromoteToE` has to delete an
        // assertion that says why it is wrong, rather than quietly compiling.
        cnahouse::rendering::RenderTier tier(RenderTier::E);
        tier.FallBackToS("no effects");
        EXPECT_EQ(tier.Active(), RenderTier::S);
        EXPECT_FALSE(tier.TierEselectable()) << "a settings toggle that cannot work is worse than no toggle";
    }

    TEST(RenderTierTests, TierEIsSelectableBeforeAFailureExactlyWhenItWasCompiledIn)
    {
        const cnahouse::rendering::RenderTier fresh(RenderTier::S);
        EXPECT_EQ(fresh.TierEselectable(), cnahouse::rendering::RenderTier::CompiledIn())
            << "running on S by choice must not hide the option to switch";
    }

    TEST(RenderStatesTests, ImportedGeometryIsCulledClockwiseBecauseHouse00071MeasuredIt)
    {
        // MEASURED (`HOUSE-00071`): open single-sided quad, `CullClockwise` 8 960 px,
        // `CullCounterClockwise` 0 px. glTF's counter-clockwise front face is exactly what XNA's
        // default removes, so the default is wrong for every asset this project imports.
        EXPECT_EQ(&StateFor(CullPolicy::ImportedFront), &Gfx::RasterizerState::CullClockwise);
        EXPECT_EQ(&StateFor(CullPolicy::Mirrored), &Gfx::RasterizerState::CullCounterClockwise);
        EXPECT_EQ(&StateFor(CullPolicy::TwoSided), &Gfx::RasterizerState::CullNone);
    }

    TEST(RenderStatesTests, ProceduralGeometryUsesTheSameStateUnderItsOwnName)
    {
        // Same state, separate name, on purpose: `HOUSE-00080` lost a whole probe run to a
        // generated quad wound clockwise in NDC. The name is where a generator author is told.
        EXPECT_EQ(&StateFor(CullPolicy::ProceduralFront), &StateFor(CullPolicy::ImportedFront));
    }

    TEST(RenderStatesTests, TheSharedStatesAreOneObjectEachAndNotCopies)
    {
        // `HOUSE-00106` measured a draw call at 8.15 us of CPU. An allocation per draw would be
        // visible in that, and XNA state objects are immutable after first use anyway.
        EXPECT_EQ(&StateFor(CullPolicy::ImportedFront), &StateFor(CullPolicy::ImportedFront));
        EXPECT_NE(&StateFor(CullPolicy::ImportedFront), &StateFor(CullPolicy::Mirrored));
    }

    TEST(RenderStatesTests, AMirroringWorldMatrixReversesTheWinding)
    {
        // A negative determinant means an odd number of axes were flipped, so the triangle order
        // seen by the rasteriser is reversed and the cull state has to reverse with it.
        EXPECT_EQ(PolicyForDeterminant(1.0f, false), CullPolicy::ImportedFront);
        EXPECT_EQ(PolicyForDeterminant(-1.0f, false), CullPolicy::Mirrored);
        EXPECT_EQ(PolicyForDeterminant(-0.001f, false), CullPolicy::Mirrored)
            << "the sign is the question, not the magnitude";
    }

    TEST(RenderStatesTests, TwoSidedWinsOverMirroring)
    {
        // A foliage card is meant to be seen from behind whether or not its placement mirrors;
        // applying the mirror rule on top of two-sided would cull it from one side again.
        EXPECT_EQ(PolicyForDeterminant(-1.0f, true), CullPolicy::TwoSided);
        EXPECT_EQ(PolicyForDeterminant(1.0f, true), CullPolicy::TwoSided);
    }

    TEST(RenderStatesTests, ADegenerateDeterminantIsTreatedAsUnmirrored)
    {
        // A zero-determinant world matrix is a bug elsewhere (a zero scale), and this records which
        // way it resolves so that it is at least the same way every frame.
        EXPECT_EQ(PolicyForDeterminant(0.0f, false), CullPolicy::ImportedFront);
    }
} // namespace
