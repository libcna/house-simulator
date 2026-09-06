// SPDX-License-Identifier: MIT
//
// `HOUSE-00143`'s acceptance: a missing asset yields the typed fallback, logs once, and does not
// throw.
//
// An INTEGRATION test rather than a unit test, deliberately: `ContentManager::Load<Model>` needs a
// real `GraphicsDevice`, and a stub `ContentManager` would test the stub. The whole behaviour under
// test is what happens at the XNA content boundary, so the boundary has to be the real one.
#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"

#include "cnahouse/content/Caches.hpp"
#include "cnahouse/util/Log.hpp"

namespace
{
    using cnahouse::content::Caches;
    using cnahouse::content::LoadOutcome;

    /// The smallest `Game` that gives a live `ContentManager` and `GraphicsDevice`. `Run()` is the only
    /// lifetime CNA offers on desktop (`HOUSE-00062`), so the assertions happen inside a frame.
    class CacheHarness : public Microsoft::Xna::Framework::Game
    {
    public:
        explicit CacheHarness(std::function<void(Caches&)> body)
            : gdm_(this)
            , body_(std::move(body))
        {
            gdm_.setPreferredBackBufferWidthProperty(320);
            gdm_.setPreferredBackBufferHeightProperty(240);
            gdm_.setSynchronizeWithVerticalRetraceProperty(false);
            setIsFixedTimeStepProperty(false);
            getContentProperty().setRootDirectoryProperty(CNAHOUSE_TEST_CONTENT_ROOT);
        }

        [[nodiscard]] bool Ran() const noexcept
        {
            return ran_;
        }

    protected:
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override
        {
            Game::Draw(gameTime);
            if (!ran_)
            {
                ran_ = true;
                Caches caches(getContentProperty());
                body_(caches);
            }
            Exit();
        }

    private:
        Microsoft::Xna::Framework::GraphicsDeviceManager gdm_;
        std::function<void(Caches&)> body_;
        bool ran_ = false;
    };

    void WithCaches(const std::function<void(Caches&)>& body)
    {
        cnahouse::util::Log::ResetForTesting();
        CacheHarness harness(body);
        harness.Run();
        ASSERT_TRUE(harness.Ran()) << "the harness never reached a frame";
    }

    TEST(CachesTests, AnAssetThatExistsLoadsAndIsThenCached)
    {
        WithCaches(
            [](Caches& caches)
            {
                LoadOutcome outcome = LoadOutcome::Cached;
                auto* first = caches.models.Get("Models/Fallback/box", outcome);
                EXPECT_NE(first, nullptr);
                EXPECT_EQ(outcome, LoadOutcome::Loaded);

                auto* second = caches.models.Get("Models/Fallback/box", outcome);
                EXPECT_EQ(second, first) << "the same instance, not a second load";
                EXPECT_EQ(outcome, LoadOutcome::Cached);
                EXPECT_EQ(caches.models.LoadedCount(), 1u);
            });
    }

    TEST(CachesTests, AMissingAssetYieldsTheFallbackAndDoesNotThrow)
    {
        // The acceptance criterion, directly. A house with one missing prop should still be walkable:
        // an exception here would turn a content mistake -- the most common kind this project will
        // have -- into an unplayable build.
        WithCaches(
            [](Caches& caches)
            {
                LoadOutcome outcome = LoadOutcome::Cached;
                Microsoft::Xna::Framework::Graphics::Model* model = nullptr;
                EXPECT_NO_THROW(model = caches.models.Get("Models/NoSuchThing", outcome));
                EXPECT_NE(model, nullptr) << "the fallback, not null";
                EXPECT_EQ(outcome, LoadOutcome::FellBack);
                EXPECT_EQ(model, caches.models.Fallback());
            });
    }

    TEST(CachesTests, AMissingAssetIsLoggedOnceNoMatterHowOftenItIsAsked)
    {
        // The "logs once" half. A missing texture is requested every time the room is drawn, so at
        // 60 Hz one bad asset would bury the whole log within a second. `util::Log`'s per-frame limiter
        // is not enough here: the same asset fails every frame forever.
        WithCaches(
            [](Caches& caches)
            {
                cnahouse::util::Log::ClearRing();
                for (int i = 0; i < 500; ++i)
                {
                    (void)caches.textures.Get("Textures/NoSuchThing");
                }
                int complaints = 0;
                for (const auto& record : cnahouse::util::Log::Ring())
                {
                    if (record.message.find("Textures/NoSuchThing") != std::string::npos)
                    {
                        ++complaints;
                        EXPECT_EQ(record.repeats, 1u) << "not even a repeat count -- it is asked once";
                    }
                }
                EXPECT_EQ(complaints, 1) << "500 requests, one message";
                EXPECT_EQ(caches.textures.FailedCount(), 1u);
            });
    }

    TEST(CachesTests, TheFailureRecordIsClearedByClear)
    {
        // After a content rebuild an asset that was missing may now be present, and a cache that
        // remembered forever would keep showing the fallback.
        WithCaches(
            [](Caches& caches)
            {
                (void)caches.textures.Get("Textures/NoSuchThing");
                ASSERT_TRUE(caches.textures.HasFailed("Textures/NoSuchThing"));
                caches.textures.Clear();
                EXPECT_FALSE(caches.textures.HasFailed("Textures/NoSuchThing"));
            });
    }

    TEST(CachesTests, EvictionDropsOneAssetWithoutDisturbingTheRest)
    {
        WithCaches(
            [](Caches& caches)
            {
                (void)caches.textures.Get("Textures/Fallback/grey");
                (void)caches.textures.Get("Textures/Fallback/missing");
                ASSERT_EQ(caches.textures.LoadedCount(), 2u);

                caches.textures.Evict("Textures/Fallback/grey");
                EXPECT_FALSE(caches.textures.IsLoaded("Textures/Fallback/grey"));
                EXPECT_TRUE(caches.textures.IsLoaded("Textures/Fallback/missing"))
                    << "the residency system evicts one pack at a time, not everything";
            });
    }

    TEST(CachesTests, EveryFallbackAssetActuallyLoads)
    {
        // The one asset guaranteed to reach a player's screen if anything goes wrong. If a fallback is
        // itself missing, every other test in this file is measuring nothing.
        WithCaches(
            [](Caches& caches)
            {
                EXPECT_NE(caches.models.Fallback(), nullptr) << "Models/Fallback/box";
                EXPECT_NE(caches.textures.Fallback(), nullptr) << "Textures/Fallback/grey";
                EXPECT_NE(caches.sounds.Fallback(), nullptr) << "Audio/Fallback/silent";
                EXPECT_NE(caches.MissingTexture(), nullptr) << "Textures/Fallback/missing";
            });
    }

    TEST(CachesTests, TheMissingTextureIsSeparateFromTheNeutralFallback)
    {
        // A missing ALBEDO should keep the lighting readable, so its fallback is neutral grey. A
        // missing REQUIRED texture should be unmissable, so it is a magenta checker. Sharing one would
        // make the first case hideous or the second invisible.
        WithCaches([](Caches& caches) { EXPECT_NE(caches.MissingTexture(), caches.textures.Fallback()); });
    }

} // namespace
