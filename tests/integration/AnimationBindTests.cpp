// SPDX-License-Identifier: MIT
//
// `HOUSE-00167`'s binding half. This is the check that `HOUSE-00074` and R-16 exist for: vertex
// blend indices are skin-local, the sidecar's name list is the only thing that maps them to bones,
// and a mismatch must be a **named** fatal error rather than a silent deformation. It needs a real
// `Model`, so it is an integration test.
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelBone.hpp"
#include "Microsoft/Xna/Framework/Graphics/ModelBoneCollection.hpp"

#include "cnahouse/animation/Animation.hpp"
#include "cnahouse/animation/AnimationCache.hpp"
#include "cnahouse/util/Result.hpp"

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <functional>

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using cnahouse::anim::AnimationCache;
    using cnahouse::anim::ClipLibrary;
    using cnahouse::anim::Skeleton;
    using cnahouse::util::ErrorCode;

    /// A skeleton whose joints are named @p names, with identity poses. What the poses ARE does not
    /// matter here -- binding is entirely a question of names.
    Skeleton SkeletonNamed(const std::vector<std::string>& names)
    {
        Skeleton skeleton;
        for (std::size_t i = 0; i < names.size(); ++i)
        {
            skeleton.boneNames.push_back(names[i]);
            skeleton.parent.push_back(i == 0 ? -1 : static_cast<std::int32_t>(i) - 1);
            skeleton.bindPose.push_back(Microsoft::Xna::Framework::Matrix::getIdentityProperty());
            skeleton.inverseBindPose.push_back(Microsoft::Xna::Framework::Matrix::getIdentityProperty());
        }
        skeleton.modelBoneIndex.assign(names.size(), -1);
        return skeleton;
    }

    /// A host that loads the fallback box and hands it to the test.
    class BoxHost final : public Microsoft::Xna::Framework::Game
    {
    public:
        using Body = std::function<void(const Gfx::Model&)>;

        explicit BoxHost(Body body)
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

        [[nodiscard]] const std::string& Failure() const noexcept
        {
            return failure_;
        }

    protected:
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override
        {
            Game::Draw(gameTime);
            if (ran_)
            {
                return;
            }
            ran_ = true;
            try
            {
                // MEASURED (`HOUSE-00064`): `Load<Model>` returns BY VALUE.
                Gfx::Model model = getContentProperty().Load<Gfx::Model>("Models/Fallback/box");
                body_(model);
            }
            catch (const std::exception& e)
            {
                failure_ = e.what();
            }
            Exit();
        }

    private:
        Microsoft::Xna::Framework::GraphicsDeviceManager gdm_;
        Body body_;
        bool ran_ = false;
        std::string failure_;
    };

    void RunWithBox(const BoxHost::Body& body)
    {
        BoxHost host(body);
        host.Run();
        EXPECT_TRUE(host.Ran()) << "the frame that loads the model never ran";
        EXPECT_EQ(host.Failure(), "") << "the fallback box must be loadable in every build";
    }

    TEST(AnimationBindTests, EveryJointResolvesWhenTheNamesMatchTheModel)
    {
        RunWithBox(
            [](const Gfx::Model& model)
            {
                const auto& bones = model.getBonesProperty();
                ASSERT_GT(bones.getCountProperty(), 0);

                // Built from the model's OWN bone names, so this asserts the resolution mechanism rather
                // than a name someone typed. Index loops, never a range-`for`: `begin()`/`end()` on
                // these collections are CNAEXT (`docs/conventions.md` §5a).
                std::vector<std::string> names;
                for (int i = 0; i < bones.getCountProperty(); ++i)
                {
                    names.push_back(bones[i]->getNameProperty());
                }

                ClipLibrary library;
                library.Assign(SkeletonNamed(names), {});
                const auto bound = library.BindTo(model);
                ASSERT_TRUE(bound.HasValue()) << bound.Error().ToString();

                EXPECT_TRUE(library.GetSkeleton().IsBound());
                for (std::size_t i = 0; i < names.size(); ++i)
                {
                    EXPECT_EQ(library.GetSkeleton().modelBoneIndex[i], bones[names[i]]->getIndexProperty())
                        << "joint " << i << " ('" << names[i] << "') resolved to the wrong bone";
                }
            });
    }

    TEST(AnimationBindTests, AnUnknownJointNamesItselfInTheError)
    {
        // The failure R-16 is about. A message saying only "the skeleton does not match" would leave
        // someone comparing two lists of sixty-two names by eye.
        RunWithBox(
            [](const Gfx::Model& model)
            {
                ClipLibrary library;
                library.Assign(SkeletonNamed({"definitely_not_a_bone"}), {});

                const auto bound = library.BindTo(model);
                ASSERT_FALSE(bound.HasValue()) << "an unknown joint must not bind";
                EXPECT_EQ(bound.Error().Code(), ErrorCode::SchemaMismatch);
                EXPECT_NE(bound.Error().Message().find("definitely_not_a_bone"), std::string::npos)
                    << bound.Error().ToString();
                EXPECT_NE(bound.Error().Message().find("skin joint 0"), std::string::npos)
                    << "the BLEND INDEX matters as much as the name: " << bound.Error().ToString();
                EXPECT_FALSE(library.GetSkeleton().IsBound());
            });
    }

    TEST(AnimationBindTests, AFailedBindLeavesNoStaleIndicesBehind)
    {
        // A second `BindTo` against a different model must not leave half the previous model's
        // indices in place -- that deforms silently, which is the one outcome this whole mechanism
        // exists to prevent.
        RunWithBox(
            [](const Gfx::Model& model)
            {
                const auto& bones = model.getBonesProperty();
                ASSERT_GT(bones.getCountProperty(), 0);
                const std::string real = bones[0]->getNameProperty();

                ClipLibrary library;
                library.Assign(SkeletonNamed({real}), {});
                ASSERT_TRUE(library.BindTo(model).HasValue());
                ASSERT_TRUE(library.GetSkeleton().IsBound());

                // Now rebind with a bad name in the list. Everything must go back to -1.
                library.Assign(SkeletonNamed({real, "nope"}), {});
                EXPECT_FALSE(library.BindTo(model).HasValue());
                EXPECT_FALSE(library.GetSkeleton().IsBound());
                for (const std::int32_t index : library.GetSkeleton().modelBoneIndex)
                {
                    EXPECT_EQ(index, -1) << "a failed bind must leave nothing usable behind";
                }
            });
    }

    TEST(AnimationCacheTests, AMissingFileIsAnErrorAndThereIsNoFallbackSkeleton)
    {
        // Deliberately unlike `content::AssetCache`. A missing texture becomes grey and the room
        // still reads; a character whose skeleton did not load cannot be drawn at all, and a
        // substitute would be a silently wrong deformation.
        AnimationCache cache(std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/Anim");

        const auto first = cache.Get("no_such_actor");
        ASSERT_FALSE(first.HasValue());
        EXPECT_EQ(cache.Count(), 0u);
        EXPECT_EQ(cache.FailedCount(), 1u);

        // Asked again: still an error, and the SAME error, remembered rather than re-read.
        const auto second = cache.Get("no_such_actor");
        ASSERT_FALSE(second.HasValue());
        EXPECT_EQ(second.Error().Code(), first.Error().Code());
        EXPECT_EQ(cache.FailedCount(), 1u) << "one failed name, however many times it is asked for";
    }

    TEST(AnimationCacheTests, ClearForgetsTheFailuresSoAFixedAssetCanBeRetried)
    {
        // `Clear` is what a content reload calls, and the point of a reload is to try again.
        AnimationCache cache(std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/Anim");
        EXPECT_FALSE(cache.Get("no_such_actor").HasValue());
        EXPECT_EQ(cache.FailedCount(), 1u);

        cache.Clear();
        EXPECT_EQ(cache.FailedCount(), 0u);
        EXPECT_EQ(cache.Count(), 0u);
    }

    TEST(AnimationCacheTests, ThePathIsBuiltFromTheRootAndTheExtension)
    {
        const AnimationCache cache("content/Anim");
        EXPECT_EQ(cache.PathFor("body_f"), "content/Anim/body_f.chanim");
        EXPECT_FALSE(cache.IsLoaded("body_f"));
    }
} // namespace
