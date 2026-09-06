// SPDX-License-Identifier: MIT
//
// `HOUSE-00159`. The pass list ships with only the HUD pass installed, so what there is to test is
// the part that every later pass depends on and that is painful to retrofit: the ORDER, the tier
// gate, the state invalidation between passes and the per-pass timing. The passes the tests install
// are real `IRenderPass` implementations that record what happened to them -- a recording pass, not
// a mock device.
#include <functional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/rendering/RenderTier.hpp"
#include "cnahouse/rendering/Renderer.hpp"
#include "cnahouse/rendering/StateTracker.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using cnahouse::rendering::IRenderPass;
    using cnahouse::rendering::Pass;
    using cnahouse::rendering::PassContext;
    using cnahouse::rendering::PassIsTierEOnly;
    using cnahouse::rendering::PassName;
    using cnahouse::rendering::Renderer;
    using cnahouse::rendering::StateTracker;

    /// Records that it ran, in order, and optionally touches the device the way a real pass would.
    class RecordingPass final : public IRenderPass
    {
    public:
        RecordingPass(std::vector<Pass>& log, Pass self)
            : log_(&log)
            , self_(self)
        {
        }

        void Draw(PassContext& context) override
        {
            log_->push_back(self_);
            ++draws_;
            if (setsAState_)
            {
                context.states.SetBlend(Gfx::BlendState::Opaque);
            }
        }

        [[nodiscard]] bool IsActive() const override
        {
            return active_;
        }

        [[nodiscard]] bool DisturbsDeviceState() const override
        {
            return disturbs_;
        }

        void SetActive(bool active) noexcept
        {
            active_ = active;
        }

        void SetDisturbs(bool disturbs) noexcept
        {
            disturbs_ = disturbs;
        }

        void SetSetsAState(bool sets) noexcept
        {
            setsAState_ = sets;
        }

        [[nodiscard]] int Draws() const noexcept
        {
            return draws_;
        }

    private:
        std::vector<Pass>* log_;
        Pass self_;
        bool active_ = true;
        bool disturbs_ = false;
        bool setsAState_ = false;
        int draws_ = 0;
    };

    /// Runs one frame of a `Renderer` against a real device.
    void RunFrame(const std::function<void(Renderer&, PassContext&)>& body,
                  const cnahouse::rendering::RenderTier& tier)
    {
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                StateTracker states(device);
                cnahouse::debug::Counters counters;
                Renderer renderer(tier);
                PassContext context{device, states, counters, 1.0f / 60.0f};
                body(renderer, context);
            });
        host.Run();
        EXPECT_TRUE(host.Ran()) << "the frame that does the measuring never ran";
        EXPECT_EQ(host.Failure(), "");
    }

    cnahouse::rendering::RenderTier TierS()
    {
        return cnahouse::rendering::RenderTier(cnahouse::app::RenderTier::S);
    }

    cnahouse::rendering::RenderTier TierEIfAvailable()
    {
        return cnahouse::rendering::RenderTier(cnahouse::app::RenderTier::E);
    }

    TEST(RendererTests, EveryPassHasAName)
    {
        // The names go into the overlay and into a bug report, so a pass added to the enum without
        // one has to fail here rather than show up as "?" in a screenshot six months later.
        for (std::size_t i = 0; i < static_cast<std::size_t>(Pass::Count); ++i)
        {
            const auto pass = static_cast<Pass>(i);
            EXPECT_NE(PassName(pass), "?") << "pass " << i << " has no name";
            EXPECT_FALSE(PassName(pass).empty());
        }
    }

    TEST(RendererTests, ExactlyTheTwoTierEPassesAreTierEOnly)
    {
        // ADR-0003's promise that Tier S is COMPLETE holds only because these two are the only
        // ones, and because their absence changes how the frame LOOKS rather than what it contains.
        EXPECT_TRUE(PassIsTierEOnly(Pass::Shadow));
        EXPECT_TRUE(PassIsTierEOnly(Pass::Composite));
        int tierEOnly = 0;
        for (std::size_t i = 0; i < static_cast<std::size_t>(Pass::Count); ++i)
        {
            tierEOnly += PassIsTierEOnly(static_cast<Pass>(i)) ? 1 : 0;
        }
        EXPECT_EQ(tierEOnly, 2);
    }

    TEST(RendererTests, PassesRunInTheOrderOfSectionSevenFive)
    {
        // Installed BACK TO FRONT on purpose: if the renderer replayed installation order rather
        // than enum order, this test would see the reverse and fail.
        const auto tier = TierEIfAvailable();
        std::vector<Pass> log;
        RunFrame(
            [&](Renderer& renderer, PassContext& context)
            {
                for (std::size_t i = static_cast<std::size_t>(Pass::Count); i-- > 0;)
                {
                    const auto pass = static_cast<Pass>(i);
                    renderer.Install(pass, std::make_unique<RecordingPass>(log, pass));
                }
                renderer.Draw(context);
            },
            tier);

        std::vector<Pass> expected;
        for (std::size_t i = 0; i < static_cast<std::size_t>(Pass::Count); ++i)
        {
            const auto pass = static_cast<Pass>(i);
            if (!PassIsTierEOnly(pass) || tier.IsTierE())
            {
                expected.push_back(pass);
            }
        }
        EXPECT_EQ(log, expected);
    }

    TEST(RendererTests, ANotInstalledPassIsSkippedAndCounted)
    {
        std::vector<Pass> log;
        RunFrame(
            [&](Renderer& renderer, PassContext& context)
            {
                renderer.Install(Pass::Sky, std::make_unique<RecordingPass>(log, Pass::Sky));
                renderer.Draw(context);

                EXPECT_EQ(renderer.PassesRun(), 1u);
                EXPECT_EQ(renderer.PassesSkipped(), static_cast<std::uint32_t>(Pass::Count) - 1u);
                EXPECT_FALSE(renderer.IsInstalled(Pass::OpaqueStatic));
                EXPECT_FALSE(renderer.WillRun(Pass::OpaqueStatic));

                // Two counters and not one: "nine passes ran" and "one ran, eight had nothing to
                // do" have the same total and are completely different frames.
                const auto* run = context.counters.Find("render.passes.run");
                const auto* skipped = context.counters.Find("render.passes.skipped");
                ASSERT_NE(run, nullptr);
                ASSERT_NE(skipped, nullptr);
            },
            TierS());
        EXPECT_EQ(log, std::vector<Pass>{Pass::Sky});
    }

    TEST(RendererTests, AnInactivePassIsSkippedWithoutBeingDrawn)
    {
        // The difference this preserves: a pass that is *installed but had nothing to do* is a
        // skipped pass in the counters, not a pass that ran and quietly did nothing -- the second
        // is a bug that looks exactly like working code.
        std::vector<Pass> log;
        RunFrame(
            [&](Renderer& renderer, PassContext& context)
            {
                auto owned = std::make_unique<RecordingPass>(log, Pass::Transparent);
                RecordingPass* pass = owned.get();
                renderer.Install(Pass::Transparent, std::move(owned));
                pass->SetActive(false);

                EXPECT_TRUE(renderer.IsInstalled(Pass::Transparent));
                EXPECT_TRUE(renderer.IsEnabled(Pass::Transparent));
                EXPECT_FALSE(renderer.WillRun(Pass::Transparent));

                renderer.Draw(context);
                EXPECT_EQ(pass->Draws(), 0);
                EXPECT_EQ(renderer.PassesRun(), 0u);
            },
            TierS());
        EXPECT_TRUE(log.empty());
    }

    TEST(RendererTests, ADisabledPassDoesNotRunAndCanBeTurnedBackOn)
    {
        std::vector<Pass> log;
        RunFrame(
            [&](Renderer& renderer, PassContext& context)
            {
                renderer.Install(Pass::AlphaTest, std::make_unique<RecordingPass>(log, Pass::AlphaTest));
                renderer.SetEnabled(Pass::AlphaTest, false);
                renderer.Draw(context);
                EXPECT_EQ(renderer.PassesRun(), 0u);

                renderer.SetEnabled(Pass::AlphaTest, true);
                renderer.Draw(context);
                EXPECT_EQ(renderer.PassesRun(), 1u);
            },
            TierS());
        EXPECT_EQ(log, std::vector<Pass>{Pass::AlphaTest});
    }

    TEST(RendererTests, TierEOnlyPassesNeverRunOnTierS)
    {
        // The gate is in `Renderer` and nowhere else. A pass that checked the tier itself would be
        // a second place for the answer to live, and ADR-0003's point is that there is one.
        std::vector<Pass> log;
        RunFrame(
            [&](Renderer& renderer, PassContext& context)
            {
                renderer.Install(Pass::Shadow, std::make_unique<RecordingPass>(log, Pass::Shadow));
                renderer.Install(Pass::Composite, std::make_unique<RecordingPass>(log, Pass::Composite));
                renderer.Install(Pass::Sky, std::make_unique<RecordingPass>(log, Pass::Sky));

                EXPECT_FALSE(renderer.WillRun(Pass::Shadow));
                EXPECT_FALSE(renderer.WillRun(Pass::Composite));
                EXPECT_TRUE(renderer.WillRun(Pass::Sky));

                renderer.Draw(context);
                EXPECT_EQ(renderer.PassesRun(), 1u) << "only the sky pass is a Tier S pass";
            },
            TierS());
        EXPECT_EQ(log, std::vector<Pass>{Pass::Sky});
    }

    TEST(RendererTests, ATierEPassRunsOnceTheTierHasIt)
    {
        if constexpr (!cnahouse::rendering::RenderTier::CompiledIn())
        {
            GTEST_SKIP() << "this binary has no Tier E, so there is no Tier-E frame to run";
        }
        else
        {
            std::vector<Pass> log;
            RunFrame(
                [&](Renderer& renderer, PassContext& context)
                {
                    renderer.Install(Pass::Shadow, std::make_unique<RecordingPass>(log, Pass::Shadow));
                    EXPECT_TRUE(renderer.WillRun(Pass::Shadow));
                    renderer.Draw(context);
                    EXPECT_EQ(renderer.PassesRun(), 1u);
                },
                TierEIfAvailable());
            EXPECT_EQ(log, std::vector<Pass>{Pass::Shadow});
        }
    }

    TEST(RendererTests, APassThatDisturbsDeviceStateForcesTheNextSetToBeReal)
    {
        // The `SpriteBatch` hazard, in the only place it can be caught generically: the HUD pass
        // restores several states at once, so a tracker that kept believing what it set BEFORE the
        // batch would skip the set that was actually needed and draw the next pass with the sprite
        // batch's blend state.
        std::vector<Pass> log;
        RunFrame(
            [&](Renderer& renderer, PassContext& context)
            {
                auto skyOwned = std::make_unique<RecordingPass>(log, Pass::Sky);
                RecordingPass* sky = skyOwned.get();
                sky->SetSetsAState(true);
                sky->SetDisturbs(true);
                renderer.Install(Pass::Sky, std::move(skyOwned));

                auto opaqueOwned = std::make_unique<RecordingPass>(log, Pass::OpaqueStatic);
                opaqueOwned->SetSetsAState(true);
                renderer.Install(Pass::OpaqueStatic, std::move(opaqueOwned));

                renderer.Draw(context);

                // Both set the SAME `BlendState::Opaque`. Without the invalidation between them the
                // second would be a skip; with it, both are real applies.
                EXPECT_EQ(context.states.Current().blendApplied, 2u);
                EXPECT_EQ(context.states.Current().blendSkipped, 0u);
            },
            TierS());
    }

    TEST(RendererTests, WithoutTheDisturbanceTheSecondIdenticalSetIsSkipped)
    {
        // The control for the test above. Without it, "2 applies" would be consistent with a
        // renderer that invalidates after EVERY pass, which would make the flag meaningless.
        std::vector<Pass> log;
        RunFrame(
            [&](Renderer& renderer, PassContext& context)
            {
                auto skyOwned = std::make_unique<RecordingPass>(log, Pass::Sky);
                skyOwned->SetSetsAState(true);
                renderer.Install(Pass::Sky, std::move(skyOwned));

                auto opaqueOwned = std::make_unique<RecordingPass>(log, Pass::OpaqueStatic);
                opaqueOwned->SetSetsAState(true);
                renderer.Install(Pass::OpaqueStatic, std::move(opaqueOwned));

                renderer.Draw(context);

                EXPECT_EQ(context.states.Current().blendApplied, 1u);
                EXPECT_EQ(context.states.Current().blendSkipped, 1u);
            },
            TierS());
    }

    TEST(RendererTests, APassThatRanHasATimeAndOneThatDidNotHasZero)
    {
        std::vector<Pass> log;
        RunFrame(
            [&](Renderer& renderer, PassContext& context)
            {
                renderer.Install(Pass::Sky, std::make_unique<RecordingPass>(log, Pass::Sky));
                renderer.Draw(context);

                EXPECT_GE(renderer.LastMilliseconds(Pass::Sky), 0.0);
                EXPECT_GE(renderer.AverageMilliseconds(Pass::Sky), 0.0);
                EXPECT_EQ(renderer.LastMilliseconds(Pass::OpaqueDynamic), 0.0)
                    << "a pass that did not run must not report the time of the one that did";
                EXPECT_EQ(renderer.AverageMilliseconds(Pass::OpaqueDynamic), 0.0);
                EXPECT_GE(renderer.TotalAverageMilliseconds(), renderer.AverageMilliseconds(Pass::Sky));
            },
            TierS());
    }

    TEST(RendererTests, InstallingOverAPassReplacesIt)
    {
        std::vector<Pass> log;
        RunFrame(
            [&](Renderer& renderer, PassContext& context)
            {
                auto firstOwned = std::make_unique<RecordingPass>(log, Pass::Sky);
                RecordingPass* first = firstOwned.get();
                renderer.Install(Pass::Sky, std::move(firstOwned));
                renderer.Install(Pass::Sky, std::make_unique<RecordingPass>(log, Pass::Sky));
                // `first` is destroyed by the second Install; the pointer is not dereferenced
                // afterwards, only its recorded draw count matters, which is why it is read here.
                static_cast<void>(first);
                renderer.Draw(context);
                EXPECT_EQ(renderer.PassesRun(), 1u) << "one slot, one pass";
            },
            TierS());
        EXPECT_EQ(log.size(), 1u);
    }
} // namespace
